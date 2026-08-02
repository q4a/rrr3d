/*
 * The cutscene pipeline, without the game.
 *
 * Demuxes an .avi, decodes N frames of H.264 through VideoToolbox, and writes
 * one of them as a TGA. Nothing of the engine, no device, no window -- so when
 * a cutscene is black in game this says whether the fault is in the decode or
 * in how it reaches the screen.
 *
 * In the house style of src/D3D9Triangle: it checks the pixels itself and exits
 * non-zero, because "wrote a file and returned 0" is a signal a decoder that
 * produced nothing can also emit. The check is deliberately weak in what it
 * asserts about content -- it cannot know what the film looks like -- and strong
 * about what would indicate failure: a frame that is entirely one colour is
 * either an undecoded buffer or a cleared one, and either way is not video.
 *
 * Usage:
 *     VideoProbe <file.avi> [frames] [out.tga]
 *
 * Defaults to Data/Video/final_eng.avi, 40 frames, video.tga. Forty because the
 * first frames of an H.264 stream can legitimately produce nothing while the
 * decoder fills, so a probe that gave up at one would report a failure that is
 * not there.
 */

#include "video/avi_reader.h"
#include "video/h264_decoder.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{

bool WriteTga(const char* path, const unsigned char* bgra, unsigned width, unsigned height)
{
	std::FILE* file = std::fopen(path, "wb");
	if (!file)
		return false;

	unsigned char header[18];
	std::memset(header, 0, sizeof(header));
	header[2] = 2;                                     /* uncompressed true colour */
	header[12] = static_cast<unsigned char>(width & 0xFF);
	header[13] = static_cast<unsigned char>((width >> 8) & 0xFF);
	header[14] = static_cast<unsigned char>(height & 0xFF);
	header[15] = static_cast<unsigned char>((height >> 8) & 0xFF);
	header[16] = 32;                                   /* bits per pixel */
	header[17] = 0x20;                                 /* top-down */

	std::fwrite(header, 1, sizeof(header), file);
	std::fwrite(bgra, 1, size_t(width) * height * 4, file);
	std::fclose(file);
	return true;
}

/* True when every pixel is the same colour, which no frame of film is. */
bool IsUniform(const std::vector<unsigned char>& frame)
{
	if (frame.size() < 8)
		return true;

	for (size_t i = 4; i < frame.size(); i += 4)
		if (std::memcmp(&frame[0], &frame[i], 3) != 0)
			return false;

	return true;
}

}

int main(int argc, char** argv)
{
	const char* path = argc > 1 ? argv[1] : "Data/Video/final_eng.avi";
	const int wanted = argc > 2 ? std::atoi(argv[2]) : 40;
	const char* out = argc > 3 ? argv[3] : "video.tga";

	r3d::video::AviReader reader;
	if (!reader.Open(path))
	{
		std::fprintf(stderr, "VideoProbe: %s: %s\n", path, reader.Error().c_str());
		return 1;
	}

	const int videoIndex = reader.VideoStream();
	if (videoIndex < 0)
	{
		std::fprintf(stderr, "VideoProbe: %s has no video stream\n", path);
		return 1;
	}

	const r3d::video::AviStream& video = reader.Stream(videoIndex);
	std::printf("VideoProbe: %s\n", path);
	std::printf("  video: %ux%u, %zu packets, %.4f s/frame\n",
		video.width, video.height, video.packets.size(), reader.FrameDuration());

	if (reader.AudioStream() >= 0)
	{
		const r3d::video::AviStream& audio = reader.Stream(reader.AudioStream());
		std::printf("  audio: format 0x%04x, %u ch, %u Hz, %zu packets\n",
			audio.formatTag, audio.channels, audio.sampleRate, audio.packets.size());
	}

	if (video.packets.empty())
	{
		std::fprintf(stderr, "VideoProbe: FAIL no video packets\n");
		return 1;
	}

	/* SPS and PPS come from the first packet: these files carry them in band
	   rather than in the container's extradata. */
	const r3d::video::AviPacket& first = video.packets[0];
	std::vector<unsigned char> sps;
	std::vector<unsigned char> pps;
	if (!r3d::video::FindParameterSets(reader.PacketData(first), first.size, sps, pps))
	{
		std::fprintf(stderr, "VideoProbe: FAIL no SPS/PPS in the first packet\n");
		return 1;
	}

	std::printf("  SPS %zu bytes, PPS %zu bytes\n", sps.size(), pps.size());

	r3d::video::H264Decoder decoder;
	if (!decoder.Start(&sps[0], sps.size(), &pps[0], pps.size()))
	{
		std::fprintf(stderr, "VideoProbe: FAIL %s\n",
			decoder.Error() ? decoder.Error() : "decoder would not start");
		return 1;
	}

	std::printf("  decoder: %ux%u\n", decoder.Width(), decoder.Height());

	std::vector<unsigned char> frame;
	std::vector<unsigned char> avcc;
	int decoded = 0;
	int lastGood = -1;

	const int limit = wanted < int(video.packets.size()) ? wanted : int(video.packets.size());
	for (int i = 0; i < limit; ++i)
	{
		const r3d::video::AviPacket& packet = video.packets[size_t(i)];
		if (!r3d::video::AnnexBToAvcc(reader.PacketData(packet), packet.size, avcc))
			continue;

		std::vector<unsigned char> got;
		if (decoder.Decode(&avcc[0], avcc.size(), got))
		{
			++decoded;
			if (!IsUniform(got))
			{
				frame.swap(got);
				lastGood = i;
			}
		}
	}

	std::printf("  decoded %d of %d packets\n", decoded, limit);

	if (decoded == 0)
	{
		std::fprintf(stderr, "VideoProbe: FAIL nothing decoded%s%s\n",
			decoder.Error() ? ": " : "", decoder.Error() ? decoder.Error() : "");
		return 1;
	}

	if (frame.empty())
	{
		std::fprintf(stderr,
			"VideoProbe: FAIL every decoded frame was a single flat colour\n");
		return 1;
	}

	if (!WriteTga(out, &frame[0], decoder.Width(), decoder.Height()))
	{
		std::fprintf(stderr, "VideoProbe: FAIL cannot write %s\n", out);
		return 1;
	}

	std::printf("VideoProbe: ok -- frame %d written to %s (%ux%u)\n",
		lastGood, out, decoder.Width(), decoder.Height());
	return 0;
}
