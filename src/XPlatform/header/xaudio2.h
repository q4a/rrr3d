#ifndef XPLATFORM_XAUDIO2_H
#define XPLATFORM_XAUDIO2_H

/*
 * XAudio2, declared to the surface src/Rock3dGame/source/snd/Audio.cpp uses.
 *
 * Declarations only -- no implementation. Phase 11 supplies that over FAudio,
 * and until then the missing symbols at link time say accurately where the port
 * is. Worth stating plainly, because the port plan gets it wrong: FAudio does
 * NOT ship an xaudio2.h. Its public headers are FAudio.h, F3DAudio.h, FACT*.h,
 * FAPO*.h and FAudioFX.h, and it is a C API -- FAudioVoice_SetVolume(voice, v)
 * over an opaque FAudioVoice*. Wine's xaudio2 DLL is what puts the IXAudio2
 * interfaces on top of it. So phase 11 owes a C++ shim presenting these
 * interfaces over FAudio's functions; this header is that shim's declaration
 * half, and its specification.
 *
 * This is XAudio2 2.7 -- the DirectX SDK version -- not 2.8 or later. Audio.cpp
 * calls XAudio2Create with a processor argument (:2096) and IXAudio2::
 * GetDeviceDetails (:2103), both of which Windows 8 removed when XAudio2 moved
 * into the OS. Declaring the 2.8 shape instead would not compile.
 *
 * On COM: IXAudio2 is an IUnknown and is reference counted. The voices are not
 * -- IXAudio2Voice has no AddRef or Release, and DestroyVoice is how a voice
 * ends. Audio.cpp relies on exactly that: Release on _xAudio, DestroyVoice on
 * every voice.
 */

#include "xplatform.h"

#include "mmsystem.h"        /* WAVEFORMATEX */
#include "windows/unknwn.h"  /* IUnknown, and the vtable machinery */

/* ---------------------------------------------------------------- types --- */

typedef UINT32 XAUDIO2_PROCESSOR;

#define XAUDIO2_DEFAULT_PROCESSOR    0x00000001  /* Processor1 */

#define XAUDIO2_DEFAULT_CHANNELS     0
#define XAUDIO2_DEFAULT_SAMPLERATE   0
#define XAUDIO2_DEFAULT_FREQ_RATIO   2.0f

#define XAUDIO2_MAX_BUFFER_BYTES     0x80000000
#define XAUDIO2_MAX_QUEUED_BUFFERS   64

#define XAUDIO2_NO_LOOP_REGION       0
#define XAUDIO2_END_OF_STREAM        0x0040

/* WAVEFORMATEXTENSIBLE, which XAUDIO2_DEVICE_DETAILS embeds. Audio.cpp reaches
   both through it -- .OutputFormat.Format.nChannels for the submix voice
   (:676) and .OutputFormat.dwChannelMask for X3DAudioInitialize (:2061) -- so
   the union is not decoration. Packed with WAVEFORMATEX for the same reason it
   is: this is a wire format. */
#pragma pack(push, 1)
typedef struct
{
	WAVEFORMATEX Format;
	union
	{
		WORD wValidBitsPerSample;
		WORD wSamplesPerBlock;
		WORD wReserved;
	} Samples;
	DWORD dwChannelMask;
	GUID SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;
#pragma pack(pop)

#define XAUDIO2_MAX_DEVICE_NAME_LEN  256

typedef enum XAUDIO2_DEVICE_ROLE
{
	NotDefaultDevice            = 0x0,
	DefaultConsoleDevice        = 0x1,
	DefaultMultimediaDevice     = 0x2,
	DefaultCommunicationsDevice = 0x4,
	DefaultGameDevice           = 0x8,
	GlobalDefaultDevice         = 0xf,
	InvalidDeviceRole           = ~GlobalDefaultDevice
} XAUDIO2_DEVICE_ROLE;

typedef struct XAUDIO2_DEVICE_DETAILS
{
	WCHAR DeviceID[XAUDIO2_MAX_DEVICE_NAME_LEN];
	WCHAR DisplayName[XAUDIO2_MAX_DEVICE_NAME_LEN];
	XAUDIO2_DEVICE_ROLE Role;
	WAVEFORMATEXTENSIBLE OutputFormat;
} XAUDIO2_DEVICE_DETAILS;

typedef struct XAUDIO2_VOICE_DETAILS
{
	UINT32 CreationFlags;
	UINT32 InputChannels;
	UINT32 InputSampleRate;
} XAUDIO2_VOICE_DETAILS;

struct IXAudio2Voice;

typedef struct XAUDIO2_SEND_DESCRIPTOR
{
	UINT32 Flags;
	IXAudio2Voice* pOutputVoice;
} XAUDIO2_SEND_DESCRIPTOR;

typedef struct XAUDIO2_VOICE_SENDS
{
	UINT32 SendCount;
	XAUDIO2_SEND_DESCRIPTOR* pSends;
} XAUDIO2_VOICE_SENDS;

typedef struct XAUDIO2_EFFECT_DESCRIPTOR
{
	IUnknown* pEffect;
	BOOL InitialState;
	UINT32 OutputChannels;
} XAUDIO2_EFFECT_DESCRIPTOR;

typedef struct XAUDIO2_EFFECT_CHAIN
{
	UINT32 EffectCount;
	XAUDIO2_EFFECT_DESCRIPTOR* pEffectDescriptors;
} XAUDIO2_EFFECT_CHAIN;

typedef enum XAUDIO2_FILTER_TYPE
{
	LowPassFilter,
	BandPassFilter,
	HighPassFilter,
	NotchFilter
} XAUDIO2_FILTER_TYPE;

typedef struct XAUDIO2_FILTER_PARAMETERS
{
	XAUDIO2_FILTER_TYPE Type;
	float Frequency;
	float OneOverQ;
} XAUDIO2_FILTER_PARAMETERS;

typedef struct XAUDIO2_BUFFER
{
	UINT32 Flags;
	UINT32 AudioBytes;
	const BYTE* pAudioData;
	UINT32 PlayBegin;
	UINT32 PlayLength;
	UINT32 LoopBegin;
	UINT32 LoopLength;
	UINT32 LoopCount;
	void* pContext;
} XAUDIO2_BUFFER;

typedef struct XAUDIO2_BUFFER_WMA
{
	const UINT32* pDecodedPacketCumulativeBytes;
	UINT32 PacketCount;
} XAUDIO2_BUFFER_WMA;

/* 2.7's shape: no BuffersQueued. Audio.cpp reads pCurrentBufferContext and
   SamplesPlayed (:826 onwards). */
typedef struct XAUDIO2_VOICE_STATE
{
	void* pCurrentBufferContext;
	UINT32 BuffersQueued;
	UINT64 SamplesPlayed;
} XAUDIO2_VOICE_STATE;

typedef struct XAUDIO2_PERFORMANCE_DATA
{
	UINT64 AudioCyclesSinceLastQuery;
	UINT64 TotalCyclesSinceLastQuery;
	UINT32 MinimumCyclesPerQuantum;
	UINT32 MaximumCyclesPerQuantum;
	UINT32 MemoryUsageInBytes;
	UINT32 CurrentLatencyInSamples;
	UINT32 GlitchesSinceEngineStarted;
	UINT32 ActiveSourceVoiceCount;
	UINT32 TotalSourceVoiceCount;
	UINT32 ActiveSubmixVoiceCount;
	UINT32 ActiveResamplerCount;
	UINT32 ActiveMatrixMixCount;
	UINT32 ActiveXmaSourceVoices;
	UINT32 ActiveXmaStreams;
} XAUDIO2_PERFORMANCE_DATA;

/* ------------------------------------------------------------ callbacks --- */

/* Not an IUnknown: XAudio2 never takes a reference on it, the application owns
   it. Audio.cpp's Proxy::VoiceCallback implements all seven. */
struct IXAudio2VoiceCallback
{
	virtual void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32 BytesRequired) = 0;
	virtual void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() = 0;
	virtual void STDMETHODCALLTYPE OnStreamEnd() = 0;
	virtual void STDMETHODCALLTYPE OnBufferStart(void* pBufferContext) = 0;
	virtual void STDMETHODCALLTYPE OnBufferEnd(void* pBufferContext) = 0;
	virtual void STDMETHODCALLTYPE OnLoopEnd(void* pBufferContext) = 0;
	virtual void STDMETHODCALLTYPE OnVoiceError(void* pBufferContext, HRESULT Error) = 0;

protected:
	/* Protected, as XAudio2 declares it -- the application owns the object and
	   XAudio2 must not delete through this pointer. */
	~IXAudio2VoiceCallback() {}
};

struct IXAudio2EngineCallback
{
	virtual void STDMETHODCALLTYPE OnProcessingPassStart() = 0;
	virtual void STDMETHODCALLTYPE OnProcessingPassEnd() = 0;
	virtual void STDMETHODCALLTYPE OnCriticalError(HRESULT Error) = 0;

protected:
	~IXAudio2EngineCallback() {}
};

/* --------------------------------------------------------------- voices --- */

/*
 * Not an IUnknown, and this is load-bearing rather than a detail: a voice is
 * not reference counted and DestroyVoice is how it ends. Audio.cpp calls
 * DestroyVoice on submix and mastering voices (:685, :719) and never Release.
 */
struct IXAudio2Voice
{
	virtual void STDMETHODCALLTYPE GetVoiceDetails(XAUDIO2_VOICE_DETAILS* pVoiceDetails) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetOutputVoices(const XAUDIO2_VOICE_SENDS* pSendList) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetEffectChain(const XAUDIO2_EFFECT_CHAIN* pEffectChain) = 0;
	virtual HRESULT STDMETHODCALLTYPE EnableEffect(UINT32 EffectIndex, UINT32 OperationSet = 0) = 0;
	virtual HRESULT STDMETHODCALLTYPE DisableEffect(UINT32 EffectIndex, UINT32 OperationSet = 0) = 0;
	virtual void STDMETHODCALLTYPE GetEffectState(UINT32 EffectIndex, BOOL* pEnabled) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetEffectParameters(UINT32 EffectIndex, const void* pParameters,
	                                                      UINT32 ParametersByteSize, UINT32 OperationSet = 0) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetEffectParameters(UINT32 EffectIndex, void* pParameters,
	                                                      UINT32 ParametersByteSize) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetFilterParameters(const XAUDIO2_FILTER_PARAMETERS* pParameters,
	                                                      UINT32 OperationSet = 0) = 0;
	virtual void STDMETHODCALLTYPE GetFilterParameters(XAUDIO2_FILTER_PARAMETERS* pParameters) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetOutputFilterParameters(IXAudio2Voice* pDestinationVoice,
	                                                            const XAUDIO2_FILTER_PARAMETERS* pParameters,
	                                                            UINT32 OperationSet = 0) = 0;
	virtual void STDMETHODCALLTYPE GetOutputFilterParameters(IXAudio2Voice* pDestinationVoice,
	                                                         XAUDIO2_FILTER_PARAMETERS* pParameters) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetVolume(float Volume, UINT32 OperationSet = 0) = 0;
	virtual void STDMETHODCALLTYPE GetVolume(float* pVolume) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetChannelVolumes(UINT32 Channels, const float* pVolumes,
	                                                    UINT32 OperationSet = 0) = 0;
	virtual void STDMETHODCALLTYPE GetChannelVolumes(UINT32 Channels, float* pVolumes) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetOutputMatrix(IXAudio2Voice* pDestinationVoice,
	                                                  UINT32 SourceChannels, UINT32 DestinationChannels,
	                                                  const float* pLevelMatrix, UINT32 OperationSet = 0) = 0;
	virtual void STDMETHODCALLTYPE GetOutputMatrix(IXAudio2Voice* pDestinationVoice,
	                                               UINT32 SourceChannels, UINT32 DestinationChannels,
	                                               float* pLevelMatrix) = 0;
	virtual void STDMETHODCALLTYPE DestroyVoice() = 0;

protected:
	~IXAudio2Voice() {}
};

struct IXAudio2SourceVoice: public IXAudio2Voice
{
	virtual HRESULT STDMETHODCALLTYPE Start(UINT32 Flags = 0, UINT32 OperationSet = 0) = 0;
	virtual HRESULT STDMETHODCALLTYPE Stop(UINT32 Flags = 0, UINT32 OperationSet = 0) = 0;
	virtual HRESULT STDMETHODCALLTYPE SubmitSourceBuffer(const XAUDIO2_BUFFER* pBuffer,
	                                                     const XAUDIO2_BUFFER_WMA* pBufferWMA = NULL) = 0;
	virtual HRESULT STDMETHODCALLTYPE FlushSourceBuffers() = 0;
	virtual HRESULT STDMETHODCALLTYPE Discontinuity() = 0;
	virtual HRESULT STDMETHODCALLTYPE ExitLoop(UINT32 OperationSet = 0) = 0;
	virtual void STDMETHODCALLTYPE GetState(XAUDIO2_VOICE_STATE* pVoiceState) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetFrequencyRatio(float Ratio, UINT32 OperationSet = 0) = 0;
	virtual void STDMETHODCALLTYPE GetFrequencyRatio(float* pRatio) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetSourceSampleRate(UINT32 NewSourceSampleRate) = 0;

protected:
	~IXAudio2SourceVoice() {}
};

struct IXAudio2SubmixVoice: public IXAudio2Voice
{
protected:
	~IXAudio2SubmixVoice() {}
};

struct IXAudio2MasteringVoice: public IXAudio2Voice
{
protected:
	~IXAudio2MasteringVoice() {}
};

/* --------------------------------------------------------------- engine --- */

struct IXAudio2: public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE GetDeviceCount(UINT32* pCount) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDeviceDetails(UINT32 Index,
	                                                   XAUDIO2_DEVICE_DETAILS* pDeviceDetails) = 0;
	virtual HRESULT STDMETHODCALLTYPE Initialize(UINT32 Flags = 0,
	                                             XAUDIO2_PROCESSOR XAudio2Processor = XAUDIO2_DEFAULT_PROCESSOR) = 0;
	virtual HRESULT STDMETHODCALLTYPE RegisterForCallbacks(IXAudio2EngineCallback* pCallback) = 0;
	virtual void STDMETHODCALLTYPE UnregisterForCallbacks(IXAudio2EngineCallback* pCallback) = 0;

	virtual HRESULT STDMETHODCALLTYPE CreateSourceVoice(IXAudio2SourceVoice** ppSourceVoice,
	                                                    const WAVEFORMATEX* pSourceFormat,
	                                                    UINT32 Flags = 0,
	                                                    float MaxFrequencyRatio = XAUDIO2_DEFAULT_FREQ_RATIO,
	                                                    IXAudio2VoiceCallback* pCallback = NULL,
	                                                    const XAUDIO2_VOICE_SENDS* pSendList = NULL,
	                                                    const XAUDIO2_EFFECT_CHAIN* pEffectChain = NULL) = 0;

	virtual HRESULT STDMETHODCALLTYPE CreateSubmixVoice(IXAudio2SubmixVoice** ppSubmixVoice,
	                                                    UINT32 InputChannels, UINT32 InputSampleRate,
	                                                    UINT32 Flags = 0, UINT32 ProcessingStage = 0,
	                                                    const XAUDIO2_VOICE_SENDS* pSendList = NULL,
	                                                    const XAUDIO2_EFFECT_CHAIN* pEffectChain = NULL) = 0;

	virtual HRESULT STDMETHODCALLTYPE CreateMasteringVoice(IXAudio2MasteringVoice** ppMasteringVoice,
	                                                       UINT32 InputChannels = XAUDIO2_DEFAULT_CHANNELS,
	                                                       UINT32 InputSampleRate = XAUDIO2_DEFAULT_SAMPLERATE,
	                                                       UINT32 Flags = 0, UINT32 DeviceIndex = 0,
	                                                       const XAUDIO2_EFFECT_CHAIN* pEffectChain = NULL) = 0;

	virtual HRESULT STDMETHODCALLTYPE StartEngine() = 0;
	virtual void STDMETHODCALLTYPE StopEngine() = 0;
	virtual HRESULT STDMETHODCALLTYPE CommitChanges(UINT32 OperationSet) = 0;
	virtual void STDMETHODCALLTYPE GetPerformanceData(XAUDIO2_PERFORMANCE_DATA* pPerfData) = 0;
	virtual void STDMETHODCALLTYPE SetDebugConfiguration(void* pDebugConfiguration,
	                                                     void* pReserved = NULL) = 0;

protected:
	~IXAudio2() {}
};

#ifdef __cplusplus
extern "C" {
#endif

/* 2.7's signature. From 2.8 onward the processor argument is gone and the
   function is in the OS rather than a redistributable. Audio.cpp:2096 passes
   it, which is what pins this header to 2.7. */
HRESULT XAudio2Create(IXAudio2** ppXAudio2, UINT32 Flags = 0,
                      XAUDIO2_PROCESSOR XAudio2Processor = XAUDIO2_DEFAULT_PROCESSOR);

#ifdef __cplusplus
}
#endif

#endif
