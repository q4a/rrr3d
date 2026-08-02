/*
 * XAudio2 2.7 and X3DAudio, over FAudio.
 *
 * The game is written against XAudio2 2.7 -- it calls XAudio2Create with a
 * processor argument and uses GetDeviceDetails, both of which 2.8 removed --
 * and 2.7 is a COM API of C++ interfaces. FAudio reimplements the same engine
 * with the same semantics but exposes it as a C API: FAudioVoice_SetVolume
 * (voice, v) over an opaque FAudioVoice*, and no interfaces at all. It ships no
 * xaudio2.h; on Wine it is Wine's own xaudio2 DLL that puts IXAudio2 on top.
 *
 * So this file is that layer. Every interface in xaudio2.h forwards to the
 * FAudio function that does the same thing, and nothing in
 * src/Rock3dGame/source/snd/Audio.cpp changes.
 *
 * WHY THE STRUCTS ARE COPIED FIELD BY FIELD rather than cast. The two
 * declarations describe the same layouts -- FAudioBuffer against
 * XAUDIO2_BUFFER, F3DAUDIO_EMITTER against X3DAUDIO_EMITTER -- and casting
 * would work today. It would also break silently if either side ever moved a
 * field, and the failure would be garbage audio rather than a compile error.
 * The copies are named assignments the compiler checks.
 *
 * Two places genuinely differ and are not copies:
 *
 *   XAUDIO2_DEVICE_DETAILS::DeviceID is WCHAR[256], and WCHAR is 32 bits under
 *   clang. FAudio's is int16_t[256], because it is Win32's wchar_t. The two
 *   are transcoded, not memcpy'd -- the same trap as the UTF-16 language files.
 *
 *   IXAudio2VoiceCallback is a C++ vtable; FAudioVoiceCallback is a struct of
 *   function pointers whose first argument is the struct itself. CallbackBridge
 *   below carries one to the other.
 */

#include "xaudio2.h"
#include "x3daudio.h"

#include <FAudio.h>
#include <F3DAudio.h>

#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

namespace
{

/* ------------------------------------------------------------- callbacks -- */

/*
 * FAudio hands each callback the FAudioVoiceCallback* it was given, so the
 * game's C++ callback is recovered by putting the C struct FIRST and casting
 * back to the bridge. Standard-layout and first member, so the cast is exact.
 */
struct CallbackBridge
{
	FAudioVoiceCallback base;
	IXAudio2VoiceCallback* target;

	static CallbackBridge* from(FAudioVoiceCallback* callback)
	{
		return reinterpret_cast<CallbackBridge*>(callback);
	}

	static void OnBufferEnd(FAudioVoiceCallback* c, void* context)
	{
		from(c)->target->OnBufferEnd(context);
	}
	static void OnBufferStart(FAudioVoiceCallback* c, void* context)
	{
		from(c)->target->OnBufferStart(context);
	}
	static void OnLoopEnd(FAudioVoiceCallback* c, void* context)
	{
		from(c)->target->OnLoopEnd(context);
	}
	static void OnStreamEnd(FAudioVoiceCallback* c)
	{
		from(c)->target->OnStreamEnd();
	}
	static void OnVoiceError(FAudioVoiceCallback* c, void* context, uint32_t error)
	{
		from(c)->target->OnVoiceError(context, HRESULT(error));
	}
	static void OnVoiceProcessingPassEnd(FAudioVoiceCallback* c)
	{
		from(c)->target->OnVoiceProcessingPassEnd();
	}
	static void OnVoiceProcessingPassStart(FAudioVoiceCallback* c, uint32_t bytes)
	{
		from(c)->target->OnVoiceProcessingPassStart(bytes);
	}

	explicit CallbackBridge(IXAudio2VoiceCallback* callback): target(callback)
	{
		base.OnBufferEnd = OnBufferEnd;
		base.OnBufferStart = OnBufferStart;
		base.OnLoopEnd = OnLoopEnd;
		base.OnStreamEnd = OnStreamEnd;
		base.OnVoiceError = OnVoiceError;
		base.OnVoiceProcessingPassEnd = OnVoiceProcessingPassEnd;
		base.OnVoiceProcessingPassStart = OnVoiceProcessingPassStart;
	}
};

/* ---------------------------------------------------------- struct copies -- */

void CopyFormat(FAudioWaveFormatEx& out, const WAVEFORMATEX& in)
{
	out.wFormatTag = in.wFormatTag;
	out.nChannels = in.nChannels;
	out.nSamplesPerSec = in.nSamplesPerSec;
	out.nAvgBytesPerSec = in.nAvgBytesPerSec;
	out.nBlockAlign = in.nBlockAlign;
	out.wBitsPerSample = in.wBitsPerSample;
	out.cbSize = in.cbSize;
}

void CopyBuffer(FAudioBuffer& out, const XAUDIO2_BUFFER& in)
{
	out.Flags = in.Flags;
	out.AudioBytes = in.AudioBytes;
	out.pAudioData = in.pAudioData;
	out.PlayBegin = in.PlayBegin;
	out.PlayLength = in.PlayLength;
	out.LoopBegin = in.LoopBegin;
	out.LoopLength = in.LoopLength;
	out.LoopCount = in.LoopCount;
	out.pContext = in.pContext;
}

/* UTF-16 to this platform's 32-bit wchar_t. The same trap as the language
   files: these are Win32 wchar_t on both sides of FAudio's API, not ours. */
void CopyDeviceString(WCHAR* out, const int16_t* in, size_t count)
{
	size_t o = 0;
	for (size_t i = 0; i < count && in[i] && o + 1 < count; ++i)
	{
		const unsigned unit = unsigned(uint16_t(in[i]));

		if (unit >= 0xD800 && unit <= 0xDBFF && i + 1 < count)
		{
			const unsigned low = unsigned(uint16_t(in[i + 1]));
			if (low >= 0xDC00 && low <= 0xDFFF)
			{
				out[o++] = WCHAR(0x10000u + ((unit - 0xD800u) << 10) + (low - 0xDC00u));
				++i;
				continue;
			}
		}
		out[o++] = WCHAR(unit);
	}
	out[o] = 0;
}

/* ---------------------------------------------------------------- voices -- */

class VoiceBase
{
public:
	virtual ~VoiceBase() {}
	FAudioVoice* faudio() const { return _voice; }

protected:
	FAudioVoice* _voice = NULL;
};

/*
 * The FAudioVoice behind an IXAudio2Voice the game hands back to us -- for
 * SetOutputVoices, SetOutputMatrix and the output filter calls, all of which
 * name another voice.
 *
 * dynamic_cast because every IXAudio2Voice in the process is one of ours and
 * the hierarchy is polymorphic; a registry would be the alternative and would
 * need a lock on the audio thread.
 */
FAudioVoice* FaudioOf(IXAudio2Voice* voice)
{
	VoiceBase* base = dynamic_cast<VoiceBase*>(voice);
	return base ? base->faudio() : NULL;
}

template <class Interface>
class Voice: public Interface, public VoiceBase
{
public:
	void STDMETHODCALLTYPE GetVoiceDetails(XAUDIO2_VOICE_DETAILS* details) override
	{
		if (!details)
			return;

		FAudioVoiceDetails from = {};
		FAudioVoice_GetVoiceDetails(_voice, &from);

		/* 2.7's XAUDIO2_VOICE_DETAILS has no ActiveFlags -- it arrived in 2.8
		   -- so FAudio's extra field is dropped rather than shifted in. */
		details->CreationFlags = from.CreationFlags;
		details->InputChannels = from.InputChannels;
		details->InputSampleRate = from.InputSampleRate;
	}

	HRESULT STDMETHODCALLTYPE SetOutputVoices(const XAUDIO2_VOICE_SENDS* sends) override
	{
		if (!sends)
			return FAudioVoice_SetOutputVoices(_voice, NULL) ? E_FAIL : S_OK;

		std::vector<FAudioSendDescriptor> descriptors(sends->SendCount);
		for (UINT32 i = 0; i < sends->SendCount; ++i)
		{
			descriptors[i].Flags = sends->pSends[i].Flags;
			descriptors[i].pOutputVoice = FaudioOf(sends->pSends[i].pOutputVoice);
		}

		FAudioVoiceSends list = {};
		list.SendCount = sends->SendCount;
		list.pSends = descriptors.empty() ? NULL : &descriptors[0];

		return FAudioVoice_SetOutputVoices(_voice, &list) ? E_FAIL : S_OK;
	}

	/*
	 * Effects, filters and per-channel volumes: the game never uses any of
	 * them -- no effect chain is ever built, no filter is ever set -- so these
	 * forward to FAudio where it is free and succeed where it is not. Wiring
	 * an FAPO chain the game does not create would be code with no caller.
	 */
	HRESULT STDMETHODCALLTYPE SetEffectChain(const XAUDIO2_EFFECT_CHAIN*) override { return S_OK; }
	HRESULT STDMETHODCALLTYPE EnableEffect(UINT32 index, UINT32 set) override
	{
		return FAudioVoice_EnableEffect(_voice, index, set) ? E_FAIL : S_OK;
	}
	HRESULT STDMETHODCALLTYPE DisableEffect(UINT32 index, UINT32 set) override
	{
		return FAudioVoice_DisableEffect(_voice, index, set) ? E_FAIL : S_OK;
	}
	void STDMETHODCALLTYPE GetEffectState(UINT32 index, BOOL* enabled) override
	{
		int32_t state = 0;
		FAudioVoice_GetEffectState(_voice, index, &state);
		if (enabled)
			*enabled = state ? TRUE : FALSE;
	}
	HRESULT STDMETHODCALLTYPE SetEffectParameters(UINT32 index, const void* parameters,
		UINT32 bytes, UINT32 set) override
	{
		return FAudioVoice_SetEffectParameters(_voice, index, parameters, bytes, set)
			? E_FAIL : S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetEffectParameters(UINT32 index, void* parameters,
		UINT32 bytes) override
	{
		return FAudioVoice_GetEffectParameters(_voice, index, parameters, bytes)
			? E_FAIL : S_OK;
	}

	HRESULT STDMETHODCALLTYPE SetFilterParameters(const XAUDIO2_FILTER_PARAMETERS* p,
		UINT32 set) override
	{
		if (!p)
			return E_POINTER;

		FAudioFilterParameters to = {};
		to.Type = FAudioFilterType(p->Type);
		to.Frequency = p->Frequency;
		to.OneOverQ = p->OneOverQ;

		return FAudioVoice_SetFilterParameters(_voice, &to, set) ? E_FAIL : S_OK;
	}

	void STDMETHODCALLTYPE GetFilterParameters(XAUDIO2_FILTER_PARAMETERS* p) override
	{
		if (!p)
			return;

		FAudioFilterParameters from = {};
		FAudioVoice_GetFilterParameters(_voice, &from);

		p->Type = XAUDIO2_FILTER_TYPE(from.Type);
		p->Frequency = from.Frequency;
		p->OneOverQ = from.OneOverQ;
	}

	HRESULT STDMETHODCALLTYPE SetOutputFilterParameters(IXAudio2Voice* destination,
		const XAUDIO2_FILTER_PARAMETERS* p, UINT32 set) override
	{
		if (!p)
			return E_POINTER;

		FAudioFilterParameters to = {};
		to.Type = FAudioFilterType(p->Type);
		to.Frequency = p->Frequency;
		to.OneOverQ = p->OneOverQ;

		return FAudioVoice_SetOutputFilterParameters(_voice, FaudioOf(destination), &to, set)
			? E_FAIL : S_OK;
	}

	void STDMETHODCALLTYPE GetOutputFilterParameters(IXAudio2Voice* destination,
		XAUDIO2_FILTER_PARAMETERS* p) override
	{
		if (!p)
			return;

		FAudioFilterParameters from = {};
		FAudioVoice_GetOutputFilterParameters(_voice, FaudioOf(destination), &from);

		p->Type = XAUDIO2_FILTER_TYPE(from.Type);
		p->Frequency = from.Frequency;
		p->OneOverQ = from.OneOverQ;
	}

	HRESULT STDMETHODCALLTYPE SetVolume(float volume, UINT32 set) override
	{
		return FAudioVoice_SetVolume(_voice, volume, set) ? E_FAIL : S_OK;
	}

	void STDMETHODCALLTYPE GetVolume(float* volume) override
	{
		if (volume)
			FAudioVoice_GetVolume(_voice, volume);
	}

	HRESULT STDMETHODCALLTYPE SetChannelVolumes(UINT32 channels, const float* volumes,
		UINT32 set) override
	{
		return FAudioVoice_SetChannelVolumes(_voice, channels, volumes, set) ? E_FAIL : S_OK;
	}

	void STDMETHODCALLTYPE GetChannelVolumes(UINT32 channels, float* volumes) override
	{
		FAudioVoice_GetChannelVolumes(_voice, channels, volumes);
	}

	HRESULT STDMETHODCALLTYPE SetOutputMatrix(IXAudio2Voice* destination,
		UINT32 srcChannels, UINT32 dstChannels, const float* matrix, UINT32 set) override
	{
		return FAudioVoice_SetOutputMatrix(_voice, FaudioOf(destination),
			srcChannels, dstChannels, matrix, set) ? E_FAIL : S_OK;
	}

	void STDMETHODCALLTYPE GetOutputMatrix(IXAudio2Voice* destination,
		UINT32 srcChannels, UINT32 dstChannels, float* matrix) override
	{
		FAudioVoice_GetOutputMatrix(_voice, FaudioOf(destination),
			srcChannels, dstChannels, matrix);
	}

	/*
	 * A voice is not reference counted -- DestroyVoice is how it ends -- so
	 * this deletes itself here and nowhere else.
	 *
	 * The destructor on VoiceBase is virtual, and required: `delete this` with
	 * `this` typed as the base while the object is really a SourceVoice is
	 * undefined, and clang traps on it rather than getting it quietly wrong.
	 */
	void STDMETHODCALLTYPE DestroyVoice() override
	{
		if (_voice)
		{
			/*
			 * SafeEXT, and this is the difference between a working game and
			 * one that crashes minutes into a race.
			 *
			 * A source voice owns the CallbackBridge that FAudio's mixer
			 * thread calls through, and `delete this` frees it. Plain
			 * DestroyVoice does not wait for a callback already in flight, so
			 * a voice destroyed on the game thread while OnBufferEnd is
			 * running on the audio thread pulls the bridge -- and the game's
			 * own callback object behind it -- out from under it. The result
			 * is a wild call from the mixer, landing anywhere.
			 *
			 * Nothing exercised this before: the silent stub never invoked a
			 * callback at all, so the whole hazard arrived with the real
			 * backend rather than being uncovered by it.
			 *
			 * SafeEXT refuses while the voice is in use, so the loop is the
			 * wait. It is bounded in practice by one mixer quantum.
			 */
			while (FAudioVoice_DestroyVoiceSafeEXT(_voice) != 0)
				{ }

			_voice = NULL;
		}
		delete this;
	}
};

class SourceVoice: public Voice<IXAudio2SourceVoice>
{
public:
	SourceVoice(FAudio* engine, const WAVEFORMATEX* format, UINT32 flags,
	            float maxFrequencyRatio, IXAudio2VoiceCallback* callback)
		: _bridge(callback)
	{
		/*
		 * The format is a member, not a local.
		 *
		 * XAudio2 copies the format it is given; whether FAudio does is not
		 * something to depend on, and the cost of being wrong is a dangling
		 * pointer read on the audio thread -- which surfaces as wild reads
		 * deep inside the resampler, nowhere near here. Owning it for the
		 * voice's lifetime makes the question not arise.
		 */
		std::memset(&_format, 0, sizeof(_format));
		if (format)
			CopyFormat(_format, *format);

		/*
		 * RRR3D_AUDIO_NO_CALLBACKS=1 -- a diagnostic, not an option.
		 *
		 * The game's callbacks mutate game state from the mixer thread:
		 * Proxy::VoiceCallback::OnStreamEnd calls Proxy::Stop() and SetPos().
		 * XAudio2 calls them on its own worker thread too, so the game was
		 * written for that -- but the silent stub never called them at all,
		 * so nothing in this port has ever exercised it. Withholding them
		 * isolates that from every other difference between stub and backend:
		 * audio still plays, and only the concurrency goes away.
		 */
		static const bool noCallbacks = [] {
			const char* v = std::getenv("RRR3D_AUDIO_NO_CALLBACKS");
			return v && v[0] != '0';
		}();

		FAudioVoiceCallback* bridge =
			(callback && !noCallbacks) ? &_bridge.base : NULL;

		FAudioSourceVoice* voice = NULL;
		if (FAudio_CreateSourceVoice(engine, &voice, format ? &_format : NULL, flags,
				maxFrequencyRatio, bridge, NULL, NULL) == 0)
			_voice = voice;
	}

	bool ok() const { return _voice != NULL; }

	HRESULT STDMETHODCALLTYPE Start(UINT32 flags, UINT32 set) override
	{
		return FAudioSourceVoice_Start(source(), flags, set) ? E_FAIL : S_OK;
	}

	HRESULT STDMETHODCALLTYPE Stop(UINT32 flags, UINT32 set) override
	{
		return FAudioSourceVoice_Stop(source(), flags, set) ? E_FAIL : S_OK;
	}

	/*
	 * The WMA overload is never used -- the game streams Ogg Vorbis and
	 * decodes it itself, submitting PCM -- and FAudio has no WMA path at all,
	 * so passing one would be a silent no-op. Ignored, and said so.
	 */
	HRESULT STDMETHODCALLTYPE SubmitSourceBuffer(const XAUDIO2_BUFFER* buffer,
		const XAUDIO2_BUFFER_WMA*) override
	{
		if (!buffer)
			return E_POINTER;

		FAudioBuffer to = {};
		CopyBuffer(to, *buffer);

		return FAudioSourceVoice_SubmitSourceBuffer(source(), &to, NULL) ? E_FAIL : S_OK;
	}

	HRESULT STDMETHODCALLTYPE FlushSourceBuffers() override
	{
		return FAudioSourceVoice_FlushSourceBuffers(source()) ? E_FAIL : S_OK;
	}

	HRESULT STDMETHODCALLTYPE Discontinuity() override
	{
		return FAudioSourceVoice_Discontinuity(source()) ? E_FAIL : S_OK;
	}

	HRESULT STDMETHODCALLTYPE ExitLoop(UINT32 set) override
	{
		return FAudioSourceVoice_ExitLoop(source(), set) ? E_FAIL : S_OK;
	}

	/*
	 * BuffersQueued is what the streaming code steers on: it tops the queue up
	 * until it is full and stops when it is. This is the reason a real backend
	 * matters -- a fixed answer either starves the stream or spins it.
	 */
	void STDMETHODCALLTYPE GetState(XAUDIO2_VOICE_STATE* state) override
	{
		if (!state)
			return;

		FAudioVoiceState from = {};
		FAudioSourceVoice_GetState(source(), &from, 0);

		state->pCurrentBufferContext = from.pCurrentBufferContext;
		state->BuffersQueued = from.BuffersQueued;
		state->SamplesPlayed = from.SamplesPlayed;
	}

	HRESULT STDMETHODCALLTYPE SetFrequencyRatio(float ratio, UINT32 set) override
	{
		/*
		 * SoundMotor drives its RPM layer through ratios from zero to one.
		 * FAudio 26.08 cannot safely process a source below unity: zero is
		 * clamped to 1/1024 and overruns the stream-start tap buffer, while any
		 * sustained sub-unity ratio eventually underflows its unsigned decode
		 * count. Keep the voice at its previous safe pitch until FAudio fixes
		 * that resampler accounting. Volume still provides the intended fade.
		 */
		if (ratio < 1.0f)
			return E_INVALIDARG;
		return FAudioSourceVoice_SetFrequencyRatio(source(), ratio, set) ? E_FAIL : S_OK;
	}

	void STDMETHODCALLTYPE GetFrequencyRatio(float* ratio) override
	{
		if (ratio)
			FAudioSourceVoice_GetFrequencyRatio(source(), ratio);
	}

	HRESULT STDMETHODCALLTYPE SetSourceSampleRate(UINT32 rate) override
	{
		return FAudioSourceVoice_SetSourceSampleRate(source(), rate) ? E_FAIL : S_OK;
	}

private:
	FAudioSourceVoice* source() const
	{
		return reinterpret_cast<FAudioSourceVoice*>(_voice);
	}

	/* Owned for the voice's lifetime -- see the constructor. */
	FAudioWaveFormatEx _format;
	CallbackBridge _bridge;
};

/* ---------------------------------------------------------------- engine -- */

class Engine: public IXAudio2
{
public:
	explicit Engine(FAudio* audio): _audio(audio) {}

	/* Not virtual, because IXAudio2's destructor is protected and non-virtual
	   as 2.7 declares it. Release() deletes through Engine*, which is the most
	   derived type, so no virtual dispatch is needed or possible. */
	~Engine()
	{
		if (_audio)
			FAudio_Release(_audio);
	}

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

	HRESULT STDMETHODCALLTYPE GetDeviceCount(UINT32* count) override
	{
		if (!count)
			return E_POINTER;
		return FAudio_GetDeviceCount(_audio, count) ? E_FAIL : S_OK;
	}

	/*
	 * 2.7 only. Audio.cpp reads OutputFormat.dwChannelMask from this and hands
	 * it to X3DAudioInitialize, so the mask has to be the device's real one --
	 * a wrong mask puts the 3D mixer's channels in the wrong places.
	 */
	HRESULT STDMETHODCALLTYPE GetDeviceDetails(UINT32 index,
		XAUDIO2_DEVICE_DETAILS* details) override
	{
		if (!details)
			return E_POINTER;

		FAudioDeviceDetails from = {};
		if (FAudio_GetDeviceDetails(_audio, index, &from) != 0)
			return E_FAIL;

		std::memset(details, 0, sizeof(*details));
		CopyDeviceString(details->DeviceID, from.DeviceID, XAUDIO2_MAX_DEVICE_NAME_LEN);
		CopyDeviceString(details->DisplayName, from.DisplayName, XAUDIO2_MAX_DEVICE_NAME_LEN);
		details->Role = XAUDIO2_DEVICE_ROLE(from.Role);

		details->OutputFormat.Format.wFormatTag = from.OutputFormat.Format.wFormatTag;
		details->OutputFormat.Format.nChannels = from.OutputFormat.Format.nChannels;
		details->OutputFormat.Format.nSamplesPerSec = from.OutputFormat.Format.nSamplesPerSec;
		details->OutputFormat.Format.nAvgBytesPerSec = from.OutputFormat.Format.nAvgBytesPerSec;
		details->OutputFormat.Format.nBlockAlign = from.OutputFormat.Format.nBlockAlign;
		details->OutputFormat.Format.wBitsPerSample = from.OutputFormat.Format.wBitsPerSample;
		details->OutputFormat.Format.cbSize = from.OutputFormat.Format.cbSize;
		details->OutputFormat.Samples.wValidBitsPerSample =
			from.OutputFormat.Samples.wValidBitsPerSample;
		details->OutputFormat.dwChannelMask = from.OutputFormat.dwChannelMask;

		return S_OK;
	}

	/* FAudioCreate has already initialised the engine; 2.7's Initialize is the
	   COM two-step and there is nothing left for it to do. */
	HRESULT STDMETHODCALLTYPE Initialize(UINT32, XAUDIO2_PROCESSOR) override { return S_OK; }

	HRESULT STDMETHODCALLTYPE RegisterForCallbacks(IXAudio2EngineCallback*) override
	{
		/* Never used: Audio.cpp registers no engine callback. */
		return S_OK;
	}

	void STDMETHODCALLTYPE UnregisterForCallbacks(IXAudio2EngineCallback*) override {}

	HRESULT STDMETHODCALLTYPE CreateSourceVoice(IXAudio2SourceVoice** voice,
		const WAVEFORMATEX* format, UINT32 flags, float maxFrequencyRatio,
		IXAudio2VoiceCallback* callback, const XAUDIO2_VOICE_SENDS*,
		const XAUDIO2_EFFECT_CHAIN*) override
	{
		if (!voice)
			return E_POINTER;
		*voice = NULL;

		SourceVoice* created = new (std::nothrow) SourceVoice(
			_audio, format, flags, maxFrequencyRatio, callback);
		if (!created)
			return E_OUTOFMEMORY;

		if (!created->ok())
		{
			delete created;
			return E_FAIL;
		}

		*voice = created;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE CreateSubmixVoice(IXAudio2SubmixVoice** voice,
		UINT32 inputChannels, UINT32 inputSampleRate, UINT32 flags,
		UINT32 processingStage, const XAUDIO2_VOICE_SENDS*,
		const XAUDIO2_EFFECT_CHAIN*) override
	{
		if (!voice)
			return E_POINTER;
		*voice = NULL;

		FAudioSubmixVoice* created = NULL;
		if (FAudio_CreateSubmixVoice(_audio, &created, inputChannels, inputSampleRate,
				flags, processingStage, NULL, NULL) != 0)
			return E_FAIL;

		*voice = Adopt<IXAudio2SubmixVoice>(created);
		return *voice ? S_OK : E_OUTOFMEMORY;
	}

	HRESULT STDMETHODCALLTYPE CreateMasteringVoice(IXAudio2MasteringVoice** voice,
		UINT32 inputChannels, UINT32 inputSampleRate, UINT32 flags,
		UINT32 deviceIndex, const XAUDIO2_EFFECT_CHAIN*) override
	{
		if (!voice)
			return E_POINTER;
		*voice = NULL;

		FAudioMasteringVoice* created = NULL;
		if (FAudio_CreateMasteringVoice(_audio, &created, inputChannels, inputSampleRate,
				flags, deviceIndex, NULL) != 0)
			return E_FAIL;

		*voice = Adopt<IXAudio2MasteringVoice>(created);
		return *voice ? S_OK : E_OUTOFMEMORY;
	}

	HRESULT STDMETHODCALLTYPE StartEngine() override
	{
		return FAudio_StartEngine(_audio) ? E_FAIL : S_OK;
	}

	void STDMETHODCALLTYPE StopEngine() override { FAudio_StopEngine(_audio); }

	/* FAudio commits every pending operation set at once; 2.7 names one, and
	   the game only ever passes XAUDIO2_COMMIT_ALL. */
	HRESULT STDMETHODCALLTYPE CommitChanges(UINT32) override
	{
		return FAudio_CommitChanges(_audio) ? E_FAIL : S_OK;
	}

	void STDMETHODCALLTYPE GetPerformanceData(XAUDIO2_PERFORMANCE_DATA* data) override
	{
		/* Never read by the game; zeroed rather than half-filled. */
		if (data)
			std::memset(data, 0, sizeof(*data));
	}

	void STDMETHODCALLTYPE SetDebugConfiguration(void*, void*) override {}

private:
	/*
	 * A submix or mastering voice FAudio already created: the wrapper only has
	 * to adopt the handle, because neither kind adds anything to
	 * IXAudio2Voice.
	 */
	template <class Interface>
	class Adopted: public Voice<Interface>
	{
	public:
		explicit Adopted(FAudioVoice* voice) { this->_voice = voice; }
	};

	template <class Interface>
	Interface* Adopt(FAudioVoice* voice)
	{
		Adopted<Interface>* wrapper = new (std::nothrow) Adopted<Interface>(voice);
		if (!wrapper)
			FAudioVoice_DestroyVoice(voice);
		return wrapper;
	}

	FAudio* _audio;
	ULONG _refs = 1;
};

}

HRESULT XAudio2Create(IXAudio2** xaudio2, UINT32 flags, XAUDIO2_PROCESSOR processor)
{
	if (!xaudio2)
		return E_POINTER;
	*xaudio2 = NULL;

	/* Disable audio entirely for diagnostics and headless runs. */
	const char* audioOff = std::getenv("RRR3D_AUDIO_OFF");
	if (audioOff && audioOff[0] != '\0' && std::strcmp(audioOff, "0") != 0)
		return E_FAIL;

	FAudio* audio = NULL;
	/* FAudio only accepts its all-processors sentinel, unlike XAudio2 2.7's
	   Processor1 default used by the game-facing header. */
	if (FAudioCreate(&audio, flags, FAUDIO_DEFAULT_PROCESSOR) != 0 || !audio)
		return E_FAIL;

	Engine* engine = new (std::nothrow) Engine(audio);
	if (!engine)
	{
		FAudio_Release(audio);
		return E_OUTOFMEMORY;
	}

	*xaudio2 = engine;
	return S_OK;
}

/* ------------------------------------------------------------- X3DAudio -- */

/*
 * Straight through. X3DAUDIO_HANDLE and F3DAUDIO_HANDLE are both 20 bytes,
 * and X3DAUDIO_LISTENER, X3DAUDIO_EMITTER and X3DAUDIO_DSP_SETTINGS were
 * transcribed from the SDK headers F3DAudio also reimplements -- field for
 * field, same order, same types. The casts below are the one place this file
 * relies on that rather than copying, because these structures carry pointers
 * to caller-owned curve arrays that a copy would have to walk and rebuild
 * every call, on the audio path, for no gain.
 */
void X3DAudioInitialize(UINT32 speakerChannelMask, FLOAT32 speedOfSound,
                        X3DAUDIO_HANDLE instance)
{
	F3DAudioInitialize(speakerChannelMask, speedOfSound,
		reinterpret_cast<uint8_t*>(instance));
}

void X3DAudioCalculate(const X3DAUDIO_HANDLE instance,
                       const X3DAUDIO_LISTENER* listener,
                       const X3DAUDIO_EMITTER* emitter,
                       UINT32 flags,
                       X3DAUDIO_DSP_SETTINGS* settings)
{
	F3DAudioCalculate(reinterpret_cast<const uint8_t*>(instance),
		reinterpret_cast<const F3DAUDIO_LISTENER*>(listener),
		reinterpret_cast<const F3DAUDIO_EMITTER*>(emitter),
		flags,
		reinterpret_cast<F3DAUDIO_DSP_SETTINGS*>(settings));
}
