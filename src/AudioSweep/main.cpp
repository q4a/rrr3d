/*
 * The engine rev sound, without the game.
 *
 * A race dies within a second of starting with audio enabled. Address
 * Sanitizer names an out-of-bounds write inside FAudio's resampler and could
 * say nothing more, because the Homebrew dylib it was reported against is
 * uninstrumented. Finding out which of the game's many audio behaviours
 * provokes it took eight runs per condition, and five plausible fixes that were
 * all wrong.
 *
 * This reproduces it in one process in under a second, with nothing of the game
 * involved: one source voice with the shape snd::Proxy builds
 * (Audio.cpp:1182-1192) -- PCM, 16-bit, the sound's own sample rate, block
 * align 2*channels, flags 0, XAUDIO2_DEFAULT_FREQ_RATIO for the maximum -- fed
 * a looping buffer, with SetFrequencyRatio swept the way SoundMotor::OnMotor
 * sweeps it.
 *
 * That sweep is the whole point. Every car in db.xml has
 * <rpmFreqRange>0 1</rpmFreqRange>, and GameBase.cpp:1099 computes the ratio as
 * x + alpha*(y - x), so the ratio *is* alpha: it runs from 0.0 at minRPM to 1.0
 * at maxRPM. _srcRPM is a sample recorded at redline and pitched down for lower
 * revs, crossfaded against _srcIdle. Real XAudio2 never refused those ratios --
 * it clamps to XAUDIO2_MIN_FREQ_RATIO, 1/1024 -- so the sub-unity range is not
 * an edge case, it is the entire rev sweep.
 *
 * It is also the version guard for FAudio.
 *
 * extern/faudio is pinned to 26.06 because 26.07 and 26.08 corrupt memory on
 * exactly this path -- see tools/setup-faudio-macos.sh for the two defects and
 * how they were measured. That pin is the kind of decision that rots quietly:
 * someone upgrades to pick up an unrelated fix, the game still starts, and the
 * heap corruption comes back as an intermittent death several seconds into a
 * race. Running this under the asan preset after any version change is what
 * turns that into one line of output. Holding the ratio at zero
 * (`AudioSweep 0 0 2`) is the shortest reproduction of the worse of the two.
 *
 * Run it under the asan preset, where FAudio is instrumented too:
 *
 *     cmake --preset=macos-arm64-asan && cmake --build build/macos-arm64-asan
 *     build/macos-arm64-asan/bin/Debug/AudioSweep
 *
 * Exit status is the result: 0 if the sweep completed with every call
 * succeeding, non-zero otherwise. A sanitizer report aborts before that and is
 * the more informative failure.
 *
 * Arguments:
 *     AudioSweep [min-ratio] [max-ratio] [seconds]
 *
 * The default sweep is the game's, 0.0 to 1.0. Passing a narrower range
 * bisects: if 0.5..1.0 survives and 0.0..1.0 does not, the fault has a
 * threshold and the threshold is worth knowing.
 */

#include "xplatform.h"
#include "xaudio2.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace
{

const UINT32 cSampleRate = 44100;
const UINT32 cChannels   = 1;

/*
 * A second of a sawtooth at 220Hz.
 *
 * Deliberately not silence: a resampler reading past its buffer may land on
 * zeroes and produce nothing audible or detectable, and this program's whole
 * job is to make the fault reachable. Content also means the run can be
 * listened to, which is how the *other* half of the bug -- that the rev sound
 * stopped sweeping -- gets checked once the crash is fixed.
 */
std::vector<short> MakeBuffer()
{
	std::vector<short> samples(cSampleRate * cChannels);

	for (size_t i = 0; i < samples.size(); ++i)
	{
		const double phase = double(i % (cSampleRate / 220)) / (cSampleRate / 220);
		samples[i] = static_cast<short>((phase * 2.0 - 1.0) * 8000.0);
	}

	return samples;
}

/*
 * The game's voices all carry a callback -- snd::Proxy passes _voiceCallback to
 * CreateSourceVoice -- and RRR3D_AUDIO_NO_CALLBACKS was one of the conditions
 * measured against the crash. Carrying one here keeps the voice the same shape
 * as the game's rather than a simpler thing that might miss the fault.
 */
class Callback: public IXAudio2VoiceCallback
{
public:
	Callback(): bufferEnds(0) {}

	void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
	void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
	void STDMETHODCALLTYPE OnStreamEnd() override {}
	void STDMETHODCALLTYPE OnBufferStart(void*) override {}
	void STDMETHODCALLTYPE OnBufferEnd(void*) override { ++bufferEnds; }
	void STDMETHODCALLTYPE OnLoopEnd(void*) override {}
	void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT error) override
	{
		std::fprintf(stderr, "AudioSweep: OnVoiceError 0x%08lx\n",
			static_cast<unsigned long>(error));
		failed = true;
	}

	unsigned bufferEnds;
	bool failed = false;
};

}

int main(int argc, char** argv)
{
	const float minRatio = argc > 1 ? float(std::atof(argv[1])) : 0.0f;
	const float maxRatio = argc > 2 ? float(std::atof(argv[2])) : 1.0f;
	const float seconds  = argc > 3 ? float(std::atof(argv[3])) : 3.0f;

	std::printf("AudioSweep: ratio %.4f -> %.4f over %.1fs\n",
		minRatio, maxRatio, seconds);

	IXAudio2* xaudio = NULL;
	if (FAILED(XAudio2Create(&xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR)) || !xaudio)
	{
		std::fprintf(stderr, "AudioSweep: XAudio2Create failed\n");
		return 1;
	}

	IXAudio2MasteringVoice* master = NULL;
	if (FAILED(xaudio->CreateMasteringVoice(&master)) || !master)
	{
		std::fprintf(stderr, "AudioSweep: CreateMasteringVoice failed\n");
		return 1;
	}

	/* snd::Proxy::Init, Audio.cpp:1182-1192, field for field. */
	WAVEFORMATEX wfm;
	std::memset(&wfm, 0, sizeof(wfm));
	wfm.cbSize          = sizeof(wfm);
	wfm.nChannels       = cChannels;
	wfm.wBitsPerSample  = 16;
	wfm.nSamplesPerSec  = cSampleRate;
	wfm.nAvgBytesPerSec = cSampleRate * cChannels * 2;
	wfm.nBlockAlign     = 2 * cChannels;
	wfm.wFormatTag      = 1;

	Callback callback;
	IXAudio2SourceVoice* voice = NULL;
	if (FAILED(xaudio->CreateSourceVoice(&voice, &wfm, 0,
			XAUDIO2_DEFAULT_FREQ_RATIO, &callback, 0, 0)) || !voice)
	{
		std::fprintf(stderr, "AudioSweep: CreateSourceVoice failed\n");
		return 1;
	}

	const std::vector<short> samples = MakeBuffer();

	/*
	 * Topped up rather than looped.
	 *
	 * xaudio2.h declares only the surface Audio.cpp uses, so there is no
	 * XAUDIO2_LOOP_INFINITE in it -- and that absence is accurate rather than a
	 * gap, because the game does not loop buffers either. snd::Streaming keeps a
	 * queue fed from the Vorbis decoder, one SubmitSourceBuffer at a time
	 * (Audio.cpp:879), which is also the path the crash was reported on. Doing
	 * the same here keeps this a reproduction rather than an approximation.
	 */
	XAUDIO2_BUFFER buffer;
	std::memset(&buffer, 0, sizeof(buffer));
	buffer.AudioBytes = static_cast<UINT32>(samples.size() * sizeof(short));
	buffer.pAudioData = reinterpret_cast<const BYTE*>(samples.data());

	const UINT32 cQueueDepth = 3;
	for (UINT32 i = 0; i < cQueueDepth; ++i)
	{
		if (FAILED(voice->SubmitSourceBuffer(&buffer)))
		{
			std::fprintf(stderr, "AudioSweep: SubmitSourceBuffer failed\n");
			return 1;
		}
	}

	if (FAILED(voice->Start()))
	{
		std::fprintf(stderr, "AudioSweep: Start failed\n");
		return 1;
	}

	/*
	 * 60Hz, because that is the rate OnMotor is driven at and the rate at which
	 * the game re-sets the ratio. A voice whose ratio changes every frame is
	 * the condition; one set once and left alone is not.
	 */
	const int steps = int(seconds * 60.0f);
	int rejected = 0;

	for (int i = 0; i < steps; ++i)
	{
		const float alpha = steps > 1 ? float(i) / float(steps - 1) : 1.0f;
		const float ratio = minRatio + alpha * (maxRatio - minRatio);

		const HRESULT hr = voice->SetFrequencyRatio(ratio);
		if (FAILED(hr))
			++rejected;

		/* Keep the queue fed, the way Streaming::Update does. A ratio well
		   below 1.0 consumes source data slowly, so this rarely fires at the
		   bottom of the sweep and every frame at the top -- which is itself
		   part of the condition being reproduced. */
		XAUDIO2_VOICE_STATE state;
		voice->GetState(&state);
		for (UINT32 q = state.BuffersQueued; q < cQueueDepth; ++q)
			voice->SubmitSourceBuffer(&buffer);

		std::this_thread::sleep_for(std::chrono::milliseconds(16));

		if (callback.failed)
		{
			std::fprintf(stderr,
				"AudioSweep: FAIL voice error at step %d, ratio %.4f\n", i, ratio);
			return 1;
		}
	}

	voice->Stop();
	voice->DestroyVoice();
	master->DestroyVoice();
	xaudio->Release();

	std::printf("AudioSweep: %d steps, %u buffer ends, %d ratios rejected\n",
		steps, callback.bufferEnds, rejected);

	/*
	 * A rejected ratio is a failure, not a curiosity. The workaround this test
	 * exists to remove made SetFrequencyRatio refuse everything below 1.0, so a
	 * non-zero count here means it -- or something like it -- is still in place
	 * and the rev sweep is still gone.
	 */
	if (rejected != 0)
	{
		std::fprintf(stderr,
			"AudioSweep: FAIL %d of %d ratios were rejected; the sweep is not intact\n",
			rejected, steps);
		return 1;
	}

	std::printf("AudioSweep: ok\n");
	return 0;
}
