/*
 * video::Player for macOS: the AVI demuxer and the VideoToolbox decoder from
 * next door, presented through the engine's own Direct3D device.
 *
 * Replaces video_stub.cpp, which reported STATE_NO_GRAPH so every cutscene was
 * treated as finished the moment it started.
 *
 * WHY THIS PRESENTS ITSELF. World::MainProgress returns early while _videoMode
 * is set (World.cpp:509). That is not an oversight: with DirectShow the VMR
 * owned the client area and painted it, so the engine deliberately stopped
 * rendering for the duration. Nothing therefore calls Present, and
 * Player::OnPaint is only reached from World::OnPaint, which came from WM_PAINT
 * on Windows and from nowhere in the SDL shell. So World calls Progress() from
 * that early-return point and this does the whole frame: decode, upload, draw,
 * present.
 *
 * WHY A D3D9 QUAD AND NOT AN AVPlayerLayer. The CAMetalLayer and its drawables
 * belong to d9mt; a sibling layer would contend with it for the drawable and
 * for frame pacing, and sdl_shell.cpp already warns about a second layer. Going
 * through the device the engine owns has none of that, and the decoder is
 * already handing us BGRA, which is what D3DFMT_A8R8G8B8 wants.
 *
 * AUDIO is the MP3 track, decoded by AudioToolbox and played through the same
 * XAudio2 shim the game uses -- so it inherits the FAudio path that phase 11
 * fixed rather than opening a second output device. Video is presented against
 * the audio clock, because a dropped frame is invisible and a stutter in the
 * soundtrack is not.
 */

#include "stdafx.h"

#include "video/VideoPlayer.h"
#include "video/avi_reader.h"
#include "video/h264_decoder.h"
#include "video/mp3_decoder.h"

#include <xaudio2.h>

#include "GraphManager.h"
#include "graph/Engine.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <cstring>
#include <vector>

namespace r3d
{

namespace video
{

namespace
{

/* One screen-space quad. D3DFVF_XYZRHW is already transformed, so no matrices
   and no fixed-function lighting are involved. */
struct QuadVertex
{
	float x, y, z, rhw;
	float u, v;
};

const DWORD cQuadFvf = D3DFVF_XYZRHW | D3DFVF_TEX1;

}

struct PlayerImpl
{
	AviReader reader;
	H264Decoder decoder;

	IDirect3DTexture9* texture = nullptr;
	unsigned textureWidth = 0;
	unsigned textureHeight = 0;

	std::vector<unsigned char> sps;
	std::vector<unsigned char> pps;
	std::vector<unsigned char> frame;
	std::vector<unsigned char> avcc;

	size_t nextPacket = 0;
	double frameDuration = 1.0 / 30.0;
	double elapsed = 0.0;
	int shownFrame = -1;
	/* The presentation time of the frame currently on screen. Frames are
	   released when the clock reaches this, rather than by counting, because
	   display order is not decode order -- see h264_decoder.h. */
	double framePts = 0.0;
	bool frameIsNew = false;

	PlaybackState state = STATE_NO_GRAPH;
	graph::Engine* engine = nullptr;

	/* Audio: the whole MP3 track decoded up front. These are at most 30
	   seconds of stereo 48kHz, which is under 12 MB as PCM -- cheaper than
	   streaming it, and it cannot underrun. */
	IXAudio2SourceVoice* voice = nullptr;
	std::vector<short> pcm;
	IXAudio2* xaudio = nullptr;
	unsigned audioRate = 0;
	std::chrono::steady_clock::time_point startTime;
	unsigned presentCount = 0;
	unsigned tickCount = 0;
	int lastTraceFrame = 0;
	double lastTraceTime = 0.0;

	void ReleaseTexture()
	{
		if (texture)
		{
			texture->Release();
			texture = nullptr;
		}
		textureWidth = 0;
		textureHeight = 0;
	}
};

Player::Player(IVideoGraphUser* user): _dShowPlayer(0), _size(0, 0), _user(user), _fullScreen(false)
{
	_impl = new PlayerImpl();
}

Player::~Player()
{
	Unload();
	delete _impl;
	_impl = nullptr;
}

void Player::Initialize(HWND handle)
{
	LSL_LOG("video: VideoToolbox player");
}

void Player::Finalize()
{
	Unload();
}

void Player::Open(const lsl::string& fileName)
{
	Unload();

	if (!_impl)
		return;

	/* GetAppFilePath turns the shipped backslash paths into something a POSIX
	   filesystem answers to, and roots them at the application directory. */
	const lsl::string path = lsl::ConvertStrWToA(lsl::GetAppFilePath(fileName), CP_UTF8);

	if (!_impl->reader.Open(path))
	{
		LSL_LOG(lsl::StrFmt("video: cannot open %s: %s",
			path.c_str(), _impl->reader.Error().c_str()));
		return;
	}

	const int videoIndex = _impl->reader.VideoStream();
	if (videoIndex < 0)
	{
		LSL_LOG("video: no video stream");
		_impl->reader.Close();
		return;
	}

	const AviStream& video = _impl->reader.Stream(videoIndex);
	if (video.packets.empty())
	{
		LSL_LOG("video: no video packets");
		_impl->reader.Close();
		return;
	}

	const AviPacket& first = video.packets[0];
	if (!FindParameterSets(_impl->reader.PacketData(first), first.size,
			_impl->sps, _impl->pps))
	{
		LSL_LOG("video: no SPS/PPS");
		_impl->reader.Close();
		return;
	}

	const double duration0 = _impl->reader.FrameDuration();
	if (!_impl->decoder.Start(&_impl->sps[0], _impl->sps.size(),
			&_impl->pps[0], _impl->pps.size(),
			duration0 > 0.0 ? duration0 : 1.0 / 30.0))
	{
		LSL_LOG(lsl::StrFmt("video: decoder: %s",
			_impl->decoder.Error() ? _impl->decoder.Error() : "would not start"));
		_impl->reader.Close();
		return;
	}

	const double duration = _impl->reader.FrameDuration();
	_impl->frameDuration = duration > 0.0 ? duration : 1.0 / 30.0;
	_impl->nextPacket = 0;
	_impl->elapsed = 0.0;
	_impl->shownFrame = -1;
	_impl->state = STATE_STOPPED;

	/* Audio, if there is any. A film without a soundtrack still plays. */
	const int audioIndex = _impl->reader.AudioStream();
	if (audioIndex >= 0)
	{
		const AviStream& audio = _impl->reader.Stream(audioIndex);

		std::vector<unsigned char> track;
		for (size_t i = 0; i < audio.packets.size(); ++i)
		{
			const unsigned char* bytes = _impl->reader.PacketData(audio.packets[i]);
			track.insert(track.end(), bytes, bytes + audio.packets[i].size);
		}

		unsigned channels = 0;
		unsigned sampleRate = 0;
		if (!track.empty() && DecodeMp3ToPcm(&track[0], track.size(), _impl->pcm,
				channels, sampleRate))
		{
			WAVEFORMATEX format;
			std::memset(&format, 0, sizeof(format));
			format.cbSize = sizeof(format);
			format.wFormatTag = 1;
			format.nChannels = WORD(channels);
			format.wBitsPerSample = 16;
			format.nSamplesPerSec = sampleRate;
			format.nBlockAlign = WORD(2 * channels);
			format.nAvgBytesPerSec = sampleRate * format.nBlockAlign;

			if (_impl->xaudio)
			{
				if (SUCCEEDED(_impl->xaudio->CreateSourceVoice(&_impl->voice, &format)))
				{
					/* The clock Progress runs the picture against. Without this
					   it stays zero, the audio clock is silently skipped, and
					   presentation falls back to accumulated frame deltas --
					   which is what made the first version stutter. */
					_impl->audioRate = sampleRate;

					XAUDIO2_BUFFER buffer;
					std::memset(&buffer, 0, sizeof(buffer));
					buffer.AudioBytes = UINT32(_impl->pcm.size() * sizeof(short));
					buffer.pAudioData = reinterpret_cast<const BYTE*>(&_impl->pcm[0]);
					buffer.Flags = XAUDIO2_END_OF_STREAM;
					_impl->voice->SubmitSourceBuffer(&buffer);
				}
			}
		}
	}

	LSL_LOG(lsl::StrFmt("video: %s, %ux%u, %zu frames",
		path.c_str(), video.width, video.height, video.packets.size()));
}

void Player::Unload()
{
	if (!_impl)
		return;

	if (_impl->voice)
	{
		_impl->voice->Stop();
		_impl->voice->DestroyVoice();
		_impl->voice = nullptr;
	}

	_impl->decoder.Stop();
	_impl->reader.Close();
	_impl->ReleaseTexture();
	_impl->pcm.clear();
	_impl->frame.clear();
	_impl->state = STATE_NO_GRAPH;
}

void Player::Play()
{
	if (_impl && _impl->state != STATE_NO_GRAPH)
		_impl->state = STATE_RUNNING;
}

void Player::Pause()
{
	if (_impl && _impl->state == STATE_RUNNING)
		_impl->state = STATE_PAUSED;
}

void Player::Stop()
{
	if (_impl && _impl->state != STATE_NO_GRAPH)
		_impl->state = STATE_STOPPED;
}

PlaybackState Player::state() const
{
	return _impl ? _impl->state : STATE_NO_GRAPH;
}

bool Player::GetFullScreen() const
{
	return _fullScreen;
}

void Player::SetFullScreen(bool value)
{
	_fullScreen = value;
}

void Player::UpdateVideoWindow(const lsl::Point& size)
{
	_size = size;
}

bool Player::OnPaint(HWND handle)
{
	/* Progress does the drawing; nothing repaints out of band. */
	return false;
}

void Player::DisplayModeChanged()
{
}

void Player::OnWMGraphEvent()
{
}

void CALLBACK Player::OnGraphEvent(HWND hwnd, long evCode, LONG_PTR param1, LONG_PTR param2)
{
}

void Player::SetOutputs(IXAudio2* xaudio, graph::Engine* engine)
{
	if (!_impl)
		return;

	_impl->xaudio = xaudio;
	_impl->engine = engine;
}

/*
 * One frame: advance the clock, decode up to it, draw, present.
 *
 * The clock is the soundtrack's when there is one. A frame presented a little
 * early or late is invisible; a gap in the audio is not, and resampling the
 * soundtrack to the video's frame rate to avoid one would be the wrong way
 * round.
 */
bool Player::Progress(float deltaTime)
{
	if (!_impl || _impl->state != STATE_RUNNING || !_impl->engine)
		return false;

	const int videoIndex = _impl->reader.VideoStream();
	if (videoIndex < 0)
		return false;

	const AviStream& video = _impl->reader.Stream(videoIndex);

	if (_impl->voice && _impl->shownFrame < 0)
		_impl->voice->Start();

	/* The audio clock, when there is audio: SamplesPlayed is authoritative and
	   does not drift against the output device the way an accumulated
	   deltaTime does. */
	if (_impl->voice && _impl->audioRate)
	{
		XAUDIO2_VOICE_STATE state;
		_impl->voice->GetState(&state);
		_impl->elapsed = double(state.SamplesPlayed) / double(_impl->audioRate);
	}
	else
	{
		/*
		 * A wall clock, not an accumulation of deltaTime. World::MainProgress
		 * returns early during video mode and so skips its own frame limiter,
		 * which makes the deltas it reports both very small and very uneven;
		 * summing them drifts and delivers frames in bursts.
		 */
		if (_impl->shownFrame < 0)
			_impl->startTime = std::chrono::steady_clock::now();

		_impl->elapsed = std::chrono::duration<double>(
			std::chrono::steady_clock::now() - _impl->startTime).count();
	}

	const int wanted = int(_impl->elapsed / _impl->frameDuration);
	(void)wanted;

	/* Decode forward to the wanted frame, dropping rather than lagging. Every
	   packet must still be fed -- H.264 frames reference each other, so one
	   cannot be skipped, only its output discarded. */
	/*
	 * Decode forward to the wanted frame, dropping rather than lagging. Every
	 * packet must still be fed -- H.264 frames reference each other, so one
	 * cannot be skipped, only its output discarded.
	 *
	 * Pop returns frames in display order, which is not the order they were
	 * pushed in: these films are I,B,B,B,P and a B-frame is stored before the
	 * frame it is shown after. See h264_decoder.h.
	 */
	while (_impl->framePts <= _impl->elapsed)
	{
		std::vector<unsigned char> got;
		double pts = -1.0;
		const bool atEnd = _impl->nextPacket >= video.packets.size();

		if (_impl->decoder.Pop(got, pts, atEnd))
		{
			_impl->frame.swap(got);
			_impl->framePts = pts;
			++_impl->shownFrame;
			_impl->frameIsNew = true;
			continue;
		}

		if (atEnd)
			break;

		const AviPacket& packet = video.packets[_impl->nextPacket++];
		if (AnnexBToAvcc(_impl->reader.PacketData(packet), packet.size, _impl->avcc))
			_impl->decoder.Push(&_impl->avcc[0], _impl->avcc.size());
	}

	if (_impl->nextPacket >= video.packets.size() && _impl->decoder.Pending() == 0
		&& _impl->framePts <= _impl->elapsed)
	{
		/* Finished. GameMode's EC_COMPLETE handler is what this stands in for. */
		_impl->state = STATE_STOPPED;
		return false;
	}

	/*
	 * RRR3D_VIDEO_TRACE=1 -- pacing, once a second.
	 *
	 * "Choppy" can mean several different faults and they are not
	 * distinguishable by eye: presented counts far below 30 mean the decode is
	 * not keeping up; presented far above mean the same frame is being pushed
	 * repeatedly; a wanted count that jumps means the clock is uneven.
	 */
	static const bool videoTrace = [] {
		const char* v = std::getenv("RRR3D_VIDEO_TRACE");
		return v && v[0] != '0';
	}();

	if (videoTrace)
	{
		++_impl->tickCount;
		if (_impl->elapsed - _impl->lastTraceTime >= 1.0)
		{
			std::fprintf(stderr,
				"video %5.1fs  presented %3u  decoded %3d  ticks %6u  packets %zu\n",
				_impl->elapsed, _impl->presentCount,
				_impl->shownFrame - _impl->lastTraceFrame,
				_impl->tickCount, _impl->nextPacket);
			std::fflush(stderr);
			_impl->presentCount = 0;
			_impl->tickCount = 0;
			_impl->lastTraceFrame = _impl->shownFrame;
			_impl->lastTraceTime = _impl->elapsed;
		}
	}

	if (_impl->frame.empty())
		return true;

	/*
	 * Only when the picture has actually changed.
	 *
	 * World::MainProgress returns early during video mode and so skips its own
	 * frame limiter; this loop therefore runs as fast as the machine allows,
	 * which measured at around 540 iterations a second. Presenting each one
	 * pushes the same image to the swapchain nine times over, which is wasted
	 * work at best and erratic delivery at worst. A film has 30 new frames a
	 * second and there is nothing else on screen to redraw for.
	 */
	if (!_impl->frameIsNew)
	{
		/*
		 * And do not spin while waiting for the next one. Without this the loop
		 * ran about 86,000 times a second doing nothing, holding a core at full
		 * tilt for the length of the cutscene. A millisecond is far finer than
		 * the 33 between frames, so it costs no accuracy.
		 */
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		return true;
	}

	_impl->frameIsNew = false;
	++_impl->presentCount;

	IDirect3DDevice9* device = _impl->engine->GetDriver().GetDevice();
	if (!device)
		return true;

	const unsigned width = _impl->decoder.Width();
	const unsigned height = _impl->decoder.Height();

	if (!_impl->texture || _impl->textureWidth != width || _impl->textureHeight != height)
	{
		_impl->ReleaseTexture();
		if (FAILED(device->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC,
				D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &_impl->texture, NULL)))
			return true;

		_impl->textureWidth = width;
		_impl->textureHeight = height;
	}

	D3DLOCKED_RECT locked;
	if (SUCCEEDED(_impl->texture->LockRect(0, &locked, NULL, D3DLOCK_DISCARD)))
	{
		for (unsigned y = 0; y < height; ++y)
			std::memcpy(static_cast<unsigned char*>(locked.pBits) + size_t(y) * locked.Pitch,
				&_impl->frame[size_t(y) * width * 4], size_t(width) * 4);
		_impl->texture->UnlockRect(0);
	}

	/*
	 * Letterboxed rather than stretched: the films are 16:9 and the window need
	 * not be, and a stretched cutscene is a more obvious fault than a black bar.
	 */
	const float targetW = float(_size.x ? _size.x : int(width));
	const float targetH = float(_size.y ? _size.y : int(height));
	const float scale = std::min(targetW / float(width), targetH / float(height));
	const float drawW = float(width) * scale;
	const float drawH = float(height) * scale;
	const float x0 = (targetW - drawW) * 0.5f;
	const float y0 = (targetH - drawH) * 0.5f;

	/* Half-texel offset: D3D9 samples texel centres, and without it a
	   one-to-one blit is half a pixel soft. */
	const QuadVertex quad[4] =
	{
		{ x0 - 0.5f,         y0 - 0.5f,         0.0f, 1.0f, 0.0f, 0.0f },
		{ x0 + drawW - 0.5f, y0 - 0.5f,         0.0f, 1.0f, 1.0f, 0.0f },
		{ x0 - 0.5f,         y0 + drawH - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
		{ x0 + drawW - 0.5f, y0 + drawH - 0.5f, 0.0f, 1.0f, 1.0f, 1.0f },
	};

	device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

	if (SUCCEEDED(device->BeginScene()))
	{
		device->SetRenderState(D3DRS_ZENABLE, FALSE);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		device->SetRenderState(D3DRS_LIGHTING, FALSE);

		device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
		device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

		device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

		device->SetVertexShader(NULL);
		device->SetPixelShader(NULL);
		device->SetFVF(cQuadFvf);
		device->SetTexture(0, _impl->texture);

		device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(QuadVertex));

		device->SetTexture(0, NULL);
		device->EndScene();
	}

	device->Present(NULL, NULL, NULL, NULL);
	return true;
}

}

}
