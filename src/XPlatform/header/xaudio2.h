/*
 * XAudio2 2.7 declarations -- exactly the subset src/Rock3dGame/snd/Audio.cpp
 * uses, and nothing else.
 *
 * This is a compile-and-link seam, not an implementation: every method here
 * succeeds and does nothing, so the game runs silently rather than not at all.
 *
 * The shape is deliberate. FAudio (zlib, https://fna-xna.github.io/) is an
 * accuracy-focused reimplementation of precisely this API and is the intended
 * replacement; dropping it in means implementing these same interfaces rather
 * than rewriting Audio.cpp. Keeping the Microsoft names and signatures is what
 * makes that a contained change, and keeps the Windows build byte-identical.
 */

#ifndef XPLATFORM_XAUDIO2_H
#define XPLATFORM_XAUDIO2_H

#ifdef _WIN32
#error "xaudio2.h here is the non-Windows substitute; Windows has its own"
#endif

#include "windows/windows_base.h"

/* mmreg.h. Audio.cpp fills one of these out to describe Vorbis PCM output. */
#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM 1
#endif

typedef struct tWAVEFORMATEX
{
    WORD  wFormatTag;
    WORD  nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD  nBlockAlign;
    WORD  wBitsPerSample;
    WORD  cbSize;
} WAVEFORMATEX, *LPWAVEFORMATEX;

typedef struct
{
    WAVEFORMATEX Format;
    union
    {
        WORD wValidBitsPerSample;
        WORD wSamplesPerBlock;
        WORD wReserved;
    } Samples;
    DWORD  dwChannelMask;
    GUID   SubFormat;
} WAVEFORMATEXTENSIBLE;

#define XAUDIO2_MAX_BUFFER_BYTES        0x80000000
#define XAUDIO2_MAX_QUEUED_BUFFERS      64
#define XAUDIO2_DEFAULT_CHANNELS        0
#define XAUDIO2_DEFAULT_SAMPLERATE      0
#define XAUDIO2_DEFAULT_PROCESSOR       0xFFFFFFFF
#define XAUDIO2_DEFAULT_FREQ_RATIO      4.0f
#define XAUDIO2_END_OF_STREAM           0x0040
#define XAUDIO2_NO_LOOP_REGION          0
#define XAUDIO2_COMMIT_NOW              0

typedef enum XAUDIO2_DEVICE_ROLE
{
    NotDefaultDevice = 0
} XAUDIO2_DEVICE_ROLE;

typedef struct XAUDIO2_DEVICE_DETAILS
{
    WCHAR                DeviceID[256];
    WCHAR                DisplayName[256];
    XAUDIO2_DEVICE_ROLE  Role;
    WAVEFORMATEXTENSIBLE OutputFormat;
} XAUDIO2_DEVICE_DETAILS;

typedef struct XAUDIO2_VOICE_DETAILS
{
    UINT32 CreationFlags;
    UINT32 InputChannels;
    UINT32 InputSampleRate;
} XAUDIO2_VOICE_DETAILS;

typedef struct XAUDIO2_BUFFER
{
    UINT32      Flags;
    UINT32      AudioBytes;
    const BYTE* pAudioData;
    UINT32      PlayBegin;
    UINT32      PlayLength;
    UINT32      LoopBegin;
    UINT32      LoopLength;
    UINT32      LoopCount;
    void*       pContext;
} XAUDIO2_BUFFER;

typedef struct XAUDIO2_VOICE_STATE
{
    void*  pCurrentBufferContext;
    UINT32 BuffersQueued;
    UINT64 SamplesPlayed;
} XAUDIO2_VOICE_STATE;

struct IXAudio2Voice;

typedef struct XAUDIO2_SEND_DESCRIPTOR
{
    UINT32          Flags;
    IXAudio2Voice*  pOutputVoice;
} XAUDIO2_SEND_DESCRIPTOR;

typedef struct XAUDIO2_VOICE_SENDS
{
    UINT32                   SendCount;
    XAUDIO2_SEND_DESCRIPTOR* pSends;
} XAUDIO2_VOICE_SENDS;

/*
 * Voice callbacks. Audio.cpp implements this to refill the streaming buffer;
 * with no engine running, nothing ever invokes it.
 */
struct IXAudio2VoiceCallback
{
    virtual void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32 BytesRequired) = 0;
    virtual void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() = 0;
    virtual void STDMETHODCALLTYPE OnStreamEnd() = 0;
    virtual void STDMETHODCALLTYPE OnBufferStart(void* pBufferContext) = 0;
    virtual void STDMETHODCALLTYPE OnBufferEnd(void* pBufferContext) = 0;
    virtual void STDMETHODCALLTYPE OnLoopEnd(void* pBufferContext) = 0;
    virtual void STDMETHODCALLTYPE OnVoiceError(void* pBufferContext, HRESULT Error) = 0;

    virtual ~IXAudio2VoiceCallback() {}
};

struct IXAudio2Voice
{
    virtual void STDMETHODCALLTYPE GetVoiceDetails(XAUDIO2_VOICE_DETAILS* pVoiceDetails) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetOutputVoices(const XAUDIO2_VOICE_SENDS* pSendList) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetVolume(float Volume, UINT32 OperationSet = XAUDIO2_COMMIT_NOW) = 0;
    virtual void STDMETHODCALLTYPE GetVolume(float* pVolume) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetOutputMatrix(IXAudio2Voice* pDestinationVoice,
        UINT32 SourceChannels, UINT32 DestinationChannels, const float* pLevelMatrix,
        UINT32 OperationSet = XAUDIO2_COMMIT_NOW) = 0;
    virtual void STDMETHODCALLTYPE DestroyVoice() = 0;

    virtual ~IXAudio2Voice() {}
};

struct IXAudio2SourceVoice: public IXAudio2Voice
{
    virtual HRESULT STDMETHODCALLTYPE Start(UINT32 Flags = 0, UINT32 OperationSet = XAUDIO2_COMMIT_NOW) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stop(UINT32 Flags = 0, UINT32 OperationSet = XAUDIO2_COMMIT_NOW) = 0;
    virtual HRESULT STDMETHODCALLTYPE SubmitSourceBuffer(const XAUDIO2_BUFFER* pBuffer,
        const void* pBufferWMA = 0) = 0;
    virtual HRESULT STDMETHODCALLTYPE FlushSourceBuffers() = 0;
    virtual HRESULT STDMETHODCALLTYPE Discontinuity() = 0;
    virtual HRESULT STDMETHODCALLTYPE ExitLoop(UINT32 OperationSet = XAUDIO2_COMMIT_NOW) = 0;
    virtual void STDMETHODCALLTYPE GetState(XAUDIO2_VOICE_STATE* pVoiceState) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetFrequencyRatio(float Ratio,
        UINT32 OperationSet = XAUDIO2_COMMIT_NOW) = 0;
    virtual void STDMETHODCALLTYPE GetFrequencyRatio(float* pRatio) = 0;
};

struct IXAudio2SubmixVoice: public IXAudio2Voice
{
};

struct IXAudio2MasteringVoice: public IXAudio2Voice
{
};

struct IXAudio2
{
    virtual ULONG STDMETHODCALLTYPE AddRef() = 0;
    virtual ULONG STDMETHODCALLTYPE Release() = 0;

    virtual HRESULT STDMETHODCALLTYPE GetDeviceCount(UINT32* pCount) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceDetails(UINT32 Index, XAUDIO2_DEVICE_DETAILS* pDeviceDetails) = 0;

    virtual HRESULT STDMETHODCALLTYPE CreateSourceVoice(IXAudio2SourceVoice** ppSourceVoice,
        const WAVEFORMATEX* pSourceFormat, UINT32 Flags = 0,
        float MaxFrequencyRatio = XAUDIO2_DEFAULT_FREQ_RATIO,
        IXAudio2VoiceCallback* pCallback = 0, const XAUDIO2_VOICE_SENDS* pSendList = 0,
        const void* pEffectChain = 0) = 0;

    virtual HRESULT STDMETHODCALLTYPE CreateSubmixVoice(IXAudio2SubmixVoice** ppSubmixVoice,
        UINT32 InputChannels, UINT32 InputSampleRate, UINT32 Flags = 0, UINT32 ProcessingStage = 0,
        const XAUDIO2_VOICE_SENDS* pSendList = 0, const void* pEffectChain = 0) = 0;

    virtual HRESULT STDMETHODCALLTYPE CreateMasteringVoice(IXAudio2MasteringVoice** ppMasteringVoice,
        UINT32 InputChannels = XAUDIO2_DEFAULT_CHANNELS,
        UINT32 InputSampleRate = XAUDIO2_DEFAULT_SAMPLERATE, UINT32 Flags = 0, UINT32 DeviceIndex = 0,
        const void* pEffectChain = 0) = 0;

    virtual HRESULT STDMETHODCALLTYPE StartEngine() = 0;
    virtual void STDMETHODCALLTYPE StopEngine() = 0;

    virtual ~IXAudio2() {}
};

HRESULT XAudio2Create(IXAudio2** ppXAudio2, UINT32 Flags = 0,
    UINT32 XAudio2Processor = XAUDIO2_DEFAULT_PROCESSOR);

#endif /* XPLATFORM_XAUDIO2_H */
