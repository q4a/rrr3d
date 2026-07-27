/*
 * Silent XAudio2 and X3DAudio.
 *
 * Every call succeeds and produces no sound. This exists so the game links and
 * runs while the audio port is outstanding -- see xaudio2.h for why the
 * Microsoft API shape is preserved rather than replaced.
 *
 * Replacing this with FAudio means implementing these same interfaces on top of
 * FAudio's C API; nothing in src/Rock3dGame/snd/Audio.cpp needs to change.
 */

#include "xaudio2.h"
#include "X3daudio.h"

#include <cstring>
#include <new>

namespace
{

class StubSourceVoice: public IXAudio2SourceVoice
{
public:
	void STDMETHODCALLTYPE GetVoiceDetails(XAUDIO2_VOICE_DETAILS* details)
	{
		if (details)
			std::memset(details, 0, sizeof(*details));
	}

	HRESULT STDMETHODCALLTYPE SetOutputVoices(const XAUDIO2_VOICE_SENDS*) { return S_OK; }
	HRESULT STDMETHODCALLTYPE SetVolume(float, UINT32) { return S_OK; }

	void STDMETHODCALLTYPE GetVolume(float* volume)
	{
		if (volume)
			*volume = 0.0f;
	}

	HRESULT STDMETHODCALLTYPE SetOutputMatrix(IXAudio2Voice*, UINT32, UINT32, const float*, UINT32) { return S_OK; }
	void STDMETHODCALLTYPE DestroyVoice() { delete this; }

	HRESULT STDMETHODCALLTYPE Start(UINT32, UINT32) { return S_OK; }
	HRESULT STDMETHODCALLTYPE Stop(UINT32, UINT32) { return S_OK; }
	HRESULT STDMETHODCALLTYPE SubmitSourceBuffer(const XAUDIO2_BUFFER*, const void*) { return S_OK; }
	HRESULT STDMETHODCALLTYPE FlushSourceBuffers() { return S_OK; }
	HRESULT STDMETHODCALLTYPE Discontinuity() { return S_OK; }
	HRESULT STDMETHODCALLTYPE ExitLoop(UINT32) { return S_OK; }

	/*
	 * Reporting zero queued buffers matters: the streaming code tops the queue
	 * up until it is full, so anything else would spin.
	 */
	void STDMETHODCALLTYPE GetState(XAUDIO2_VOICE_STATE* state)
	{
		if (state)
			std::memset(state, 0, sizeof(*state));
	}

	HRESULT STDMETHODCALLTYPE SetFrequencyRatio(float, UINT32) { return S_OK; }

	void STDMETHODCALLTYPE GetFrequencyRatio(float* ratio)
	{
		if (ratio)
			*ratio = 1.0f;
	}
};

template <class _Interface> class StubVoice: public _Interface
{
public:
	void STDMETHODCALLTYPE GetVoiceDetails(XAUDIO2_VOICE_DETAILS* details)
	{
		if (details)
			std::memset(details, 0, sizeof(*details));
	}

	HRESULT STDMETHODCALLTYPE SetOutputVoices(const XAUDIO2_VOICE_SENDS*) { return S_OK; }
	HRESULT STDMETHODCALLTYPE SetVolume(float, UINT32) { return S_OK; }

	void STDMETHODCALLTYPE GetVolume(float* volume)
	{
		if (volume)
			*volume = 0.0f;
	}

	HRESULT STDMETHODCALLTYPE SetOutputMatrix(IXAudio2Voice*, UINT32, UINT32, const float*, UINT32) { return S_OK; }
	void STDMETHODCALLTYPE DestroyVoice() { delete this; }
};

class StubXAudio2: public IXAudio2
{
private:
	ULONG _refCnt;
public:
	StubXAudio2(): _refCnt(1) {}

	ULONG STDMETHODCALLTYPE AddRef() { return ++_refCnt; }

	ULONG STDMETHODCALLTYPE Release()
	{
		ULONG res = --_refCnt;
		if (res == 0)
			delete this;
		return res;
	}

	/*
	 * One device with a plausible stereo format. Audio.cpp reads the channel
	 * count and sample rate out of this to size its output matrices, so an
	 * empty answer would be worse than a fabricated one.
	 */
	HRESULT STDMETHODCALLTYPE GetDeviceCount(UINT32* count)
	{
		if (count)
			*count = 1;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetDeviceDetails(UINT32, XAUDIO2_DEVICE_DETAILS* details)
	{
		if (!details)
			return E_POINTER;

		std::memset(details, 0, sizeof(*details));
		details->OutputFormat.Format.wFormatTag = WAVE_FORMAT_PCM;
		details->OutputFormat.Format.nChannels = 2;
		details->OutputFormat.Format.nSamplesPerSec = 44100;
		details->OutputFormat.Format.wBitsPerSample = 16;
		details->OutputFormat.Format.nBlockAlign = 4;
		details->OutputFormat.Format.nAvgBytesPerSec = 44100 * 4;
		details->OutputFormat.dwChannelMask = 0x3; /* front left + front right */
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE CreateSourceVoice(IXAudio2SourceVoice** voice, const WAVEFORMATEX*,
		UINT32, float, IXAudio2VoiceCallback*, const XAUDIO2_VOICE_SENDS*, const void*)
	{
		if (!voice)
			return E_POINTER;
		*voice = new (std::nothrow) StubSourceVoice();
		return *voice ? S_OK : E_OUTOFMEMORY;
	}

	HRESULT STDMETHODCALLTYPE CreateSubmixVoice(IXAudio2SubmixVoice** voice, UINT32, UINT32,
		UINT32, UINT32, const XAUDIO2_VOICE_SENDS*, const void*)
	{
		if (!voice)
			return E_POINTER;
		*voice = new (std::nothrow) StubVoice<IXAudio2SubmixVoice>();
		return *voice ? S_OK : E_OUTOFMEMORY;
	}

	HRESULT STDMETHODCALLTYPE CreateMasteringVoice(IXAudio2MasteringVoice** voice, UINT32, UINT32,
		UINT32, UINT32, const void*)
	{
		if (!voice)
			return E_POINTER;
		*voice = new (std::nothrow) StubVoice<IXAudio2MasteringVoice>();
		return *voice ? S_OK : E_OUTOFMEMORY;
	}

	HRESULT STDMETHODCALLTYPE StartEngine() { return S_OK; }
	void STDMETHODCALLTYPE StopEngine() {}
};

}

HRESULT XAudio2Create(IXAudio2** ppXAudio2, UINT32, UINT32)
{
	if (!ppXAudio2)
		return E_POINTER;

	*ppXAudio2 = new (std::nothrow) StubXAudio2();
	return *ppXAudio2 ? S_OK : E_OUTOFMEMORY;
}

void X3DAudioInitialize(UINT32, FLOAT, X3DAUDIO_HANDLE Instance)
{
	std::memset(Instance, 0, X3DAUDIO_HANDLE_BYTESIZE);
}

void X3DAudioCalculate(const X3DAUDIO_HANDLE, const X3DAUDIO_LISTENER*, const X3DAUDIO_EMITTER*,
	UINT32, X3DAUDIO_DSP_SETTINGS* pDSPSettings)
{
	if (!pDSPSettings || !pDSPSettings->pMatrixCoefficients)
		return;

	//A zero matrix is silence. The caller allocated it and passes it straight to
	//IXAudio2Voice::SetOutputMatrix.
	const UINT32 count = pDSPSettings->SrcChannelCount * pDSPSettings->DstChannelCount;
	for (UINT32 i = 0; i < count; ++i)
		pDSPSettings->pMatrixCoefficients[i] = 0.0f;
}
