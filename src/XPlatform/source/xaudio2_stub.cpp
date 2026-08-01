/*
 * Silent XAudio2 and X3DAudio.
 *
 * Every call succeeds and produces no sound. This is a link seam, not an
 * implementation: it exists so the game runs while the audio port is
 * outstanding, because a rendering phase cannot be verified against a game
 * that will not start.
 *
 * Phase 11 replaces it. That means implementing these same interfaces over
 * FAudio's C API -- see the header of xaudio2.h, which records that FAudio
 * ships no xaudio2.h of its own and that a C++ shim is owed. Nothing in
 * src/Rock3dGame/source/snd/Audio.cpp changes when it does.
 *
 * WHERE SILENCE IS NOT ENOUGH. Three answers here are load-bearing, because
 * the game reads them back and acts on them rather than ignoring them:
 *
 *   - GetState must report an empty queue. The streaming code tops the queue
 *     up until it is full, so any other answer makes it stop feeding.
 *   - GetVolume must return what SetVolume was given. Audio.cpp fades by
 *     reading the current volume and stepping from it; a constant would freeze
 *     the fade rather than silence it.
 *   - X3DAudioCalculate must fill pMatrixCoefficients. The caller allocates
 *     SrcChannelCount * DstChannelCount floats and hands them straight to
 *     SetOutputMatrix, so leaving them uninitialised is a real bug that would
 *     surface the moment the matrix reaches a working backend.
 *
 * Anything that only sets state nobody reads back is a plain no-op.
 */

#include "xaudio2.h"
#include "x3daudio.h"

#include <cstring>
#include <new>

namespace
{

/*
 * The IXAudio2Voice half, shared by all three voice kinds.
 *
 * A voice is not an IUnknown -- it is not reference counted, and DestroyVoice
 * is how it ends -- so this deletes itself there and nowhere else.
 */
template <class Interface>
class StubVoice: public Interface
{
public:
	void STDMETHODCALLTYPE GetVoiceDetails(XAUDIO2_VOICE_DETAILS* details) override
	{
		if (details)
			std::memset(details, 0, sizeof(*details));
	}

	HRESULT STDMETHODCALLTYPE SetOutputVoices(const XAUDIO2_VOICE_SENDS*) override { return S_OK; }
	HRESULT STDMETHODCALLTYPE SetEffectChain(const XAUDIO2_EFFECT_CHAIN*) override { return S_OK; }
	HRESULT STDMETHODCALLTYPE EnableEffect(UINT32, UINT32) override { return S_OK; }
	HRESULT STDMETHODCALLTYPE DisableEffect(UINT32, UINT32) override { return S_OK; }

	void STDMETHODCALLTYPE GetEffectState(UINT32, BOOL* enabled) override
	{
		if (enabled)
			*enabled = FALSE;
	}

	HRESULT STDMETHODCALLTYPE SetEffectParameters(UINT32, const void*, UINT32, UINT32) override { return S_OK; }

	HRESULT STDMETHODCALLTYPE GetEffectParameters(UINT32, void* parameters, UINT32 bytes) override
	{
		if (parameters && bytes)
			std::memset(parameters, 0, bytes);
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE SetFilterParameters(const XAUDIO2_FILTER_PARAMETERS*, UINT32) override { return S_OK; }

	void STDMETHODCALLTYPE GetFilterParameters(XAUDIO2_FILTER_PARAMETERS* parameters) override
	{
		if (parameters)
			std::memset(parameters, 0, sizeof(*parameters));
	}

	HRESULT STDMETHODCALLTYPE SetOutputFilterParameters(IXAudio2Voice*,
		const XAUDIO2_FILTER_PARAMETERS*, UINT32) override { return S_OK; }

	void STDMETHODCALLTYPE GetOutputFilterParameters(IXAudio2Voice*,
		XAUDIO2_FILTER_PARAMETERS* parameters) override
	{
		if (parameters)
			std::memset(parameters, 0, sizeof(*parameters));
	}

	/* Remembered, not discarded -- see the note at the top of the file. */
	HRESULT STDMETHODCALLTYPE SetVolume(float volume, UINT32) override
	{
		_volume = volume;
		return S_OK;
	}

	void STDMETHODCALLTYPE GetVolume(float* volume) override
	{
		if (volume)
			*volume = _volume;
	}

	HRESULT STDMETHODCALLTYPE SetChannelVolumes(UINT32, const float*, UINT32) override { return S_OK; }

	void STDMETHODCALLTYPE GetChannelVolumes(UINT32 channels, float* volumes) override
	{
		for (UINT32 i = 0; volumes && i < channels; ++i)
			volumes[i] = 1.0f;
	}

	HRESULT STDMETHODCALLTYPE SetOutputMatrix(IXAudio2Voice*, UINT32, UINT32,
		const float*, UINT32) override { return S_OK; }

	void STDMETHODCALLTYPE GetOutputMatrix(IXAudio2Voice*, UINT32 srcChannels,
		UINT32 dstChannels, float* matrix) override
	{
		for (UINT32 i = 0; matrix && i < srcChannels * dstChannels; ++i)
			matrix[i] = 0.0f;
	}

	void STDMETHODCALLTYPE DestroyVoice() override { delete this; }

private:
	float _volume = 1.0f;
};

class StubSourceVoice: public StubVoice<IXAudio2SourceVoice>
{
public:
	HRESULT STDMETHODCALLTYPE Start(UINT32, UINT32) override { return S_OK; }
	HRESULT STDMETHODCALLTYPE Stop(UINT32, UINT32) override { return S_OK; }
	HRESULT STDMETHODCALLTYPE SubmitSourceBuffer(const XAUDIO2_BUFFER*, const XAUDIO2_BUFFER_WMA*) override { return S_OK; }
	HRESULT STDMETHODCALLTYPE FlushSourceBuffers() override { return S_OK; }
	HRESULT STDMETHODCALLTYPE Discontinuity() override { return S_OK; }
	HRESULT STDMETHODCALLTYPE ExitLoop(UINT32) override { return S_OK; }

	/*
	 * All zero, which means BuffersQueued == 0 and SamplesPlayed == 0.
	 *
	 * The queue depth is what the streaming code steers on: it submits until
	 * the queue is full and stops when it is. Reporting it permanently empty
	 * keeps it feeding, which is the harmless direction; reporting it full
	 * would stall the stream and look like a decoder bug.
	 */
	void STDMETHODCALLTYPE GetState(XAUDIO2_VOICE_STATE* state) override
	{
		if (state)
			std::memset(state, 0, sizeof(*state));
	}

	HRESULT STDMETHODCALLTYPE SetFrequencyRatio(float ratio, UINT32) override
	{
		_ratio = ratio;
		return S_OK;
	}

	void STDMETHODCALLTYPE GetFrequencyRatio(float* ratio) override
	{
		if (ratio)
			*ratio = _ratio;
	}

	HRESULT STDMETHODCALLTYPE SetSourceSampleRate(UINT32) override { return S_OK; }

private:
	float _ratio = 1.0f;
};

class StubXAudio2: public IXAudio2
{
public:
	/* ---- IUnknown ---- */

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** object) override
	{
		if (object)
			*object = NULL;
		return E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE AddRef() override { return ++_refs; }

	ULONG STDMETHODCALLTYPE Release() override
	{
		const ULONG refs = --_refs;
		if (!refs)
			delete this;
		return refs;
	}

	/* ---- IXAudio2 ---- */

	HRESULT STDMETHODCALLTYPE GetDeviceCount(UINT32* count) override
	{
		if (count)
			*count = 1;
		return S_OK;
	}

	/*
	 * One device, stereo. Audio.cpp reads OutputFormat.dwChannelMask and hands
	 * it to X3DAudioInitialize, so this has to be a mask that names as many
	 * channels as nChannels claims, not zero.
	 */
	HRESULT STDMETHODCALLTYPE GetDeviceDetails(UINT32, XAUDIO2_DEVICE_DETAILS* details) override
	{
		if (!details)
			return E_POINTER;

		std::memset(details, 0, sizeof(*details));
		details->OutputFormat.Format.nChannels = 2;
		details->OutputFormat.Format.nSamplesPerSec = 44100;
		details->OutputFormat.Format.wBitsPerSample = 16;
		details->OutputFormat.Format.nBlockAlign = 4;
		details->OutputFormat.Format.nAvgBytesPerSec = 44100 * 4;
		/* SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT. Written as the literal
		   because nothing in the game names those constants, and the header
		   here carries only what the game names. */
		details->OutputFormat.dwChannelMask = 0x3;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE Initialize(UINT32, XAUDIO2_PROCESSOR) override { return S_OK; }
	HRESULT STDMETHODCALLTYPE RegisterForCallbacks(IXAudio2EngineCallback*) override { return S_OK; }
	void STDMETHODCALLTYPE UnregisterForCallbacks(IXAudio2EngineCallback*) override {}

	HRESULT STDMETHODCALLTYPE CreateSourceVoice(IXAudio2SourceVoice** voice,
		const WAVEFORMATEX*, UINT32, float, IXAudio2VoiceCallback*,
		const XAUDIO2_VOICE_SENDS*, const XAUDIO2_EFFECT_CHAIN*) override
	{
		if (!voice)
			return E_POINTER;
		*voice = new (std::nothrow) StubSourceVoice();
		return *voice ? S_OK : E_OUTOFMEMORY;
	}

	HRESULT STDMETHODCALLTYPE CreateSubmixVoice(IXAudio2SubmixVoice** voice,
		UINT32, UINT32, UINT32, UINT32,
		const XAUDIO2_VOICE_SENDS*, const XAUDIO2_EFFECT_CHAIN*) override
	{
		if (!voice)
			return E_POINTER;
		*voice = new (std::nothrow) StubVoice<IXAudio2SubmixVoice>();
		return *voice ? S_OK : E_OUTOFMEMORY;
	}

	HRESULT STDMETHODCALLTYPE CreateMasteringVoice(IXAudio2MasteringVoice** voice,
		UINT32, UINT32, UINT32, UINT32, const XAUDIO2_EFFECT_CHAIN*) override
	{
		if (!voice)
			return E_POINTER;
		*voice = new (std::nothrow) StubVoice<IXAudio2MasteringVoice>();
		return *voice ? S_OK : E_OUTOFMEMORY;
	}

	HRESULT STDMETHODCALLTYPE StartEngine() override { return S_OK; }
	void STDMETHODCALLTYPE StopEngine() override {}
	HRESULT STDMETHODCALLTYPE CommitChanges(UINT32) override { return S_OK; }

	void STDMETHODCALLTYPE GetPerformanceData(XAUDIO2_PERFORMANCE_DATA* data) override
	{
		if (data)
			std::memset(data, 0, sizeof(*data));
	}

	void STDMETHODCALLTYPE SetDebugConfiguration(void*, void*) override {}

private:
	ULONG _refs = 1;
};

}

HRESULT XAudio2Create(IXAudio2** xaudio2, UINT32, XAUDIO2_PROCESSOR)
{
	if (!xaudio2)
		return E_POINTER;
	*xaudio2 = new (std::nothrow) StubXAudio2();
	return *xaudio2 ? S_OK : E_OUTOFMEMORY;
}

/* ------------------------------------------------------------ X3DAudio --- */

void X3DAudioInitialize(UINT32, FLOAT32, X3DAUDIO_HANDLE instance)
{
	if (instance)
		std::memset(instance, 0, X3DAUDIO_HANDLE_BYTESIZE);
}

/*
 * A unity mix and a stationary emitter.
 *
 * The matrix is the part that matters: the caller allocates
 * SrcChannelCount * DstChannelCount floats immediately before this call and
 * passes them straight to SetOutputMatrix afterwards, so anything left
 * uninitialised here is garbage that reaches the mixer as soon as one exists.
 * Equal weight per destination channel is the neutral answer -- no panning,
 * no attenuation, and the same total energy however the channels are counted.
 */
void X3DAudioCalculate(const X3DAUDIO_HANDLE, const X3DAUDIO_LISTENER*,
	const X3DAUDIO_EMITTER*, UINT32, X3DAUDIO_DSP_SETTINGS* settings)
{
	if (!settings)
		return;

	if (settings->pMatrixCoefficients)
	{
		const UINT32 count = settings->SrcChannelCount * settings->DstChannelCount;
		const float level = settings->DstChannelCount
			? 1.0f / float(settings->DstChannelCount) : 0.0f;
		for (UINT32 i = 0; i < count; ++i)
			settings->pMatrixCoefficients[i] = level;
	}

	for (UINT32 i = 0; settings->pDelayTimes && i < settings->DstChannelCount; ++i)
		settings->pDelayTimes[i] = 0.0f;

	settings->LPFDirectCoefficient      = 1.0f;
	settings->LPFReverbCoefficient      = 1.0f;
	settings->ReverbLevel               = 0.0f;
	settings->DopplerFactor             = 1.0f;
	settings->EmitterToListenerAngle    = 0.0f;
	settings->EmitterToListenerDistance = 0.0f;
	settings->EmitterVelocityComponent  = 0.0f;
	settings->ListenerVelocityComponent = 0.0f;
}
