#pragma once

#include <deque>

#include <xaudio2.h>
#include <X3daudio.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include <vorbis/vorbisenc.h>

namespace r3d
{
	namespace snd
	{
		using seek_pos = ogg_int64_t;

		class Engine;
		class SoundLib;

		class Sound : public Resource
		{
			friend SoundLib;
		public:
			static const unsigned cBufferSize;

			class Buffer : public Object
			{
				friend Sound;

				using _MyBase = Object;
			private:
				Sound* _sound;
				//������
				char* _data;
				//������ ������, ������ ��� ����� cBufferSize
				unsigned _dataSize;
				//���������, �������� ������� � ������� ������, � PCM samples
				seek_pos _pos;
				seek_pos _endPos;

				Buffer(Sound* sound);
				~Buffer() override;

				bool Load(seek_pos pos);
				void Free();
				bool IsEndBuffer() const;
			public:
				virtual void AddRef() const;
				virtual unsigned Release() const;

				bool ContainPos(seek_pos pos) const;
				bool IsInit() const;

				char* GetData() const;
				int GetDataSize() const;

				seek_pos GetPos() const;
				seek_pos GetEndPos() const;
			};

			using Buffers = std::map<seek_pos, Buffer*>;
			using BufferList = List<Buffer*>;
		private:
			SoundLib* _lib;
			std::string _fileName;
			unsigned _cacheSize;
			bool _isLoad;
			float _volume;

			FILE* _file;
			//mutable - ������ ��������� ������� ������� non-const
			mutable OggVorbis_File _oggFile;
			vorbis_info _vorbisInfo;
			//���������������� ������ ��������
			Buffers _buffers;
			//������ ������������ ��������
			BufferList _unusedBuffers;
			unsigned _unusedBufSize;
			//��� ������������� �������
			LockedObj* _lockObj;

			void InsertUnusedBuf(Buffer* buffer);
			void RemoveUnusedBuf(BufferList::const_iterator iter);
			void RemoveUnusedBuf(Buffer* buffer);
			void ClearUnusedBuffers();
			void OptimizeUnusedBufs();
		protected:
			//������������� ������
			void DoInit() override;
			void DoFree() override;
			void DoUpdate() override;

			seek_pos GetPrevBuffer(Buffer* pos) const;
			seek_pos GetNextBuffer(Buffer* pos) const;
		public:
			Sound(SoundLib* lib);
			~Sound() override;

			//������� ������ � pos
			Buffer* CreateBuffer(seek_pos pos);
			//pos != 0 ������� ������ ����� pos
			//pos == 0 - �������� � ������
			Buffer* CreateBufferAfter(Buffer* pos);
			//pos != 0 ������� ������ ����� pos
			//pos == 0 - �������� � ������
			Buffer* CreateBufferBefore(Buffer* pos);
			//������� ������
			void DeleteBuffer(Buffer* buffer);
			//������� ��� �������
			void DeleteAllBuffers();
			//���������� ������
			void ReleaseBuffer(Buffer* buffer);
			//������ ������ �������� ����� � �����
			bool IsEndBuffer(Buffer* buffer) const;

			//��������� �� ��������� ��������
			//���������� 0, ���� ������ ��� �� ������
			Buffer* FirstBuffer();
			Buffer* NextBuffer(Buffer* pos);
			Buffer* PrevBuffer(Buffer* pos);
			Buffer* FindBufferByPos(seek_pos pos, Buffers::iterator* nearRes = nullptr);

			//���������������� ���� ��������� � ������ ���� �����, � ���� ������ ����������� �� ���������. ����� Free ������������� ��������� ���� � ��������� �� ��������
			void Load();
			//���������, �� ������� � ������������������ ���������. �������� �����������
			void Unload();
			//����� ��������
			bool IsLoad() const;

			void Lock();
			void Unlock();

			SoundLib* GetLib();
			Engine* GetEngine();

			const std::string& GetFileName() const;
			void SetFileName(const std::string& value);

			//������ ����, ���������� ����������� ���������� ������ � ������ ������������� ��������. ��� ���������� ����� ������� ������ ������� �������������
			unsigned GetCacheSize() const;
			void SetCacheSize(unsigned value);

			const vorbis_info& GetVorbisInfo() const;
			const seek_pos GetPCMTotal() const;

			float GetVolume() const;
			void SetVolume(float value);
		};

		class SoundLib : public ResourceCollection<Sound, void, SoundLib*, SoundLib*>
		{
			friend Engine;

			using _MyBase = ResourceCollection<Sound, void, SoundLib*, SoundLib*>;
		private:
			Engine* _engine;

			SoundLib(Engine* engine);
			~SoundLib() override;
		protected:
			void RemoveItem(const Value& value) override;
		public:
			Engine* GetEngine();
		};

		using VoiceList = List<class Voice*>;

		class Voice : public Object
		{
		private:
			Engine* _engine;
			VoiceList _receivers;

			void DoClearReceivers();
		protected:
			float* _outMatrix;
			int _srcChannels;
			int _destChannels;

			void SendReceivers(IXAudio2Voice* voice);
			void ApplyOutputMatrix(IXAudio2Voice* voice);

			virtual void ChangedReceivers() = 0;
			virtual void ChangedOutMatrix() = 0;
			virtual IXAudio2Voice* GetXVoice() = 0;

			void SetOutputMatrix(int srcChannels, int destChannels, const float* matrix);
			void SetDefOutputMatrix(int srcChannels, int destChannels, float volume);
		public:
			Voice(Engine* engine);
			~Voice() override;

			void InsertReceiver(Voice* voice);
			void InsertReceiver(VoiceList::const_iterator sIter, VoiceList::const_iterator eIter);
			void InsertReceiver(const VoiceList& receivers);
			VoiceList::iterator RemoveReceiver(VoiceList::const_iterator iter);
			void RemoveReceiver(Voice* voice);
			void ClearReceivers();

			Engine* GetEngine();
			const VoiceList& GetReceivers() const;

			float GetVolume();
			void SetVolume(float value);
		};

		class SubmixVoice : public Voice
		{
			using _MyBase = Voice;
		private:
			IXAudio2SubmixVoice* _xVoice;
		protected:
			void ChangedReceivers() override;
			void ChangedOutMatrix() override;

			IXAudio2Voice* GetXVoice() override;
		public:
			SubmixVoice(Engine* engine);
			~SubmixVoice() override;
		};

		class MasteringVoice : public Voice
		{
			using _MyBase = Voice;
		private:
			IXAudio2MasteringVoice* _xVoice;
		protected:
			void ChangedReceivers() override;
			void ChangedOutMatrix() override;

			IXAudio2Voice* GetXVoice() override;
		public:
			MasteringVoice(Engine* engine);
			~MasteringVoice() override;
		};

		enum PlayMode { pmOnce = 0, pmCycle, pmInfite, cPlayModeEnd };

		class Proxy : public Voice
		{
			friend Engine;

			using _MyBase = Voice;
		private:
			using Buffer = Sound::Buffer;
			using Buffers = List<Buffer*>;

			class VoiceCallback : public IXAudio2VoiceCallback
			{
			private:
				Proxy* _proxy;
			public:
				VoiceCallback(Proxy* proxy);

				void STDMETHODCALLTYPE OnStreamEnd() override;
				void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override;
				void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32 SamplesRequired) override;
				void STDMETHODCALLTYPE OnBufferEnd(void* pBufferContext) override;
				void STDMETHODCALLTYPE OnBufferStart(void* pBufferContext) override;
				void STDMETHODCALLTYPE OnLoopEnd(void* pBufferContext) override;
				void STDMETHODCALLTYPE OnVoiceError(void* pBufferContext, HRESULT Error) override;
			};

			class Streaming : public ThreadPool::UserWork
			{
				using _MyBase = UserWork;
			private:
				Proxy* _proxy;

				volatile bool _play;
				volatile seek_pos _pos;
				volatile bool _posChanged;
				volatile unsigned _bufSize;
				volatile unsigned _cacheSize;

				//�������
				//���������� � �������
				Buffers _buffers;
				//��������������
				Buffers _unusedBuffers;
				//����������� ��� �����. ������� � _unusedBuffers
				LockedObj* _unusedBufsLock;
				//������ ���������� � �������
				unsigned _dataSizeBuffers;
				//���. ������� ������� �������
				seek_pos _firstBufPos;

				//������� ����������� ����������� ������: ����������, ��������
				ThreadEvent* _process;
				//��� �������� ��������, ��������� ���� ����������� �������� ������ ��� ���������� ������ �����������
				bool _cached;

				//�������� �� ������� ��������
				//��������� ������
				bool RemoveBuffer(Buffers::iterator iter, bool checkVoiceState = true);
				bool RemoveBuffer(Buffer* buffer);
				//�������� ������������ ������������� xVoice
				//�������� ������ � �����
				void PushBackBuffer(Buffer* buffer, bool endOfStream, seek_pos offs);
				//������� ��� �������
				void DeleteBuffers();

				//���������� true ���� ��� ����������� ����� ������
				bool ApplyPosChanged();
				bool RemoveUnusedBufs();
				bool DoStream();
				bool DoPlay(bool work);

				//��������� ���� ��������, ���������� ���������
				void Changed();

				void Execute(Object* arg) override;
				void OnTerminate() override;
			public:
				Streaming(Proxy* proxy);
				~Streaming() override;

				void Terminate();
				void WaitTerminate();
				void ReleaseBuffer(Buffer* buffer);

				void Play();
				void Stop();
				bool IsPlaying() const;

				seek_pos GetPos() const;
				void SetPos(seek_pos value);

				//��������. ����� ������ ����� ����������� ����� ����� bufSize � cacheSize. ���� ����� ����� ���� �� �������� � ���� ����. �������� �� ��������� ��������� ����������� �������������� �������� ��� ������������ ���������������. ����� ���� ���������� � ���������� ������ ��������. ��� ������ �������� ������� � ������ ��� ��������� ����������� Sound::Load. ������� ���������� �����. ������� ������������ ������������, ����� ����������� � ������ ���� ��������������� ��������� ������ ������������.	
				//������ �������. ���������� ����������� ����������� ����� ������� ���������������
				//0 - ����������� �� ������������
				unsigned GetBufSize() const;
				void SetBufSize(unsigned value);
				//������ ����. ���������� ������������ ������ ������������ ������ ��� ����������� ������������
				//0 - ����������� �� ������������
				unsigned GetCacheSize() const;
				void SetCacheSize(unsigned value);
			};

		public:
			class Report : public Object
			{
			public:
				virtual void OnStreamEnd(Proxy* sender, PlayMode mode)
				{
				}

				virtual void OnTerminate(Proxy* sender)
				{
				}
			};

			using ReportList = Container<Report*>;
		private:
			bool _init;
			Sound* _sound;

			seek_pos _pos;
			PlayMode _playMode;
			float _volume;
			float _frequencyRatio;

			ReportList _reportList;
			volatile bool _onStreamEnd;
			volatile bool _onTerminate;

			VoiceCallback* _voiceCallback;
			IXAudio2SourceVoice* _xVoice;

			Streaming* _streaming;
			bool _run;

			void OnStreamEnd();
			void OnTerminate();
			void SendReports();
		protected:
			Proxy(Engine* engine);
			~Proxy() override;

			void ChangedReceivers() override;
			void ChangedOutMatrix() override;
			IXAudio2Voice* GetXVoice() override;
		public:
			//����������� �������� �����
			void Init();
			//���������� ������� �������
			void Free();
			//����� ���������������
			bool IsInit() const;

			//��������� �����
			void Run();
			//���������� ��������� �����, ����� ������� Report::OnTerminate ����� Free �� ����� �����������
			void Terminate();
			//����� �������
			bool IsRunning() const;
			//����� ��� �����������
			bool IsTerminating() const;
			//����������� �� ����������
			void WaitTerminate();

			void RegReport(Report* report);
			void UnregReport(Report* report);

			void Play();
			void Stop();
			bool IsPlaying() const;

			//����
			Sound* GetSound();
			void SetSound(Sound* value);
			bool IsSoundCompilant(Sound* value) const;

			seek_pos GetPos() const;
			void SetPos(seek_pos value);

			PlayMode GetPlayMode() const;
			void SetPlayMode(PlayMode value);

			float GetVolume() const;
			void SetVolume(float value);

			float GetFrequencyRatio() const;
			void SetFrequencyRatio(float value);

			using _MyBase::SetOutputMatrix;
			using _MyBase::SetDefOutputMatrix;
		};

		class Source : public Voice
		{
			friend Engine;
			using _MyBase = Voice;
		public:
			using Report = Proxy::Report;
			using ReportList = List<Report*>;
		private:
			Sound* _sound;

			seek_pos _pos;
			PlayMode _playMode;
			float _volume;
			float _frequencyRatio;
			ReportList _reportList;

			Proxy* _proxy;

			void ChangedReceivers() override;
			void ChangedOutMatrix() override;
			IXAudio2Voice* GetXVoice() override;

			void ClearReportList();
		protected:
			Source(Engine* engine);
			~Source() override;

			virtual void DoInit();
			virtual void DoFree();

			void Init();
			void Free(bool unload = false);

			Proxy* GetProxy();
		public:
			virtual void Play();
			virtual void Stop();
			virtual bool IsPlaying() const;

			void RegReport(Report* report);
			void UnregReport(Report* report);

			Sound* GetSound();
			void SetSound(Sound* value, bool unload = false);

			seek_pos GetPos() const;
			void SetPos(seek_pos value);

			PlayMode GetPlayMode() const;
			void SetPlayMode(PlayMode value);

			float GetVolume() const;
			void SetVolume(float value);

			float GetFrequencyRatio() const;
			void SetFrequencyRatio(float value);
		};

		class Source3d : public Source
		{
			friend Engine;
			using _MyBase = Source;
		public:
			class MyReport : public Proxy::Report
			{
			private:
				Source3d* _source;
			public:
				MyReport(Source3d* source);

				void OnStreamEnd(Proxy* sender, PlayMode mode) override;
			};

		private:
			MyReport* _myReport;
			D3DXVECTOR3 _pos3d;
			float _distScaler;

			X3DAUDIO_EMITTER _xEmitter;
			X3DAUDIO_DSP_SETTINGS _dspSettings;
			bool _changed3d;

			bool _play;
			double _playTime;

			void ApplyX3dEffect();
			void CleanUpX3d();
		protected:
			Source3d(Engine* engine);
			~Source3d() override;

			void DoInit() override;
			void DoFree() override;

			void ApplyOutputMatrix();
			void ApplyChanges3d();
			void Changed3d();
		public:
			void Play() override;
			void Stop() override;
			bool IsPlaying() const override;

			const D3DXVECTOR3& GetPos3d();
			void SetPos3d(const D3DXVECTOR3& value);

			float GetDistScaler() const;
			void SetDistScaler(float value);
		};

		struct Listener
		{
			Listener(): pos(NullVector), rot(NullQuaternion)
			{
			}

			D3DXVECTOR3 pos;
			D3DXQUATERNION rot;
		};

		class Engine : public Component
		{
			using SoundLibList = List<SoundLib*>;
			using VoiceList = List<Voice*>;
			using ProxyList = List<Proxy*>;
			using Source3dList = List<Source3d*>;

			friend Source;
			friend Source3d;
			friend Voice;
			friend MasteringVoice;
			friend SubmixVoice;
			friend Proxy;
		public:
			//������ 3�
			//m3dSurround - �������, �� ���� x3dAudio
			//m3dFlat - ������� ��������� �� ����������
			enum Mode3d { m3dFlat, m3dSurround };

		private:
			MasteringVoice* _mainVoice;
			Listener* _listener;
			float _distScaler;
			Mode3d _mode3d;
			bool _changed3d;
			bool _isComputing;

			SoundLibList _soundLibList;
			VoiceList _voiceList;

			IXAudio2* _xAudio;
			XAUDIO2_DEVICE_DETAILS _xDevCaps;

			//
			ProxyList _proxyList;
			//��� ����������
			ProxyList _srcPool;
			unsigned _poolMaxSize;
			//������ �������������� ����������, ���������������, ��� ������������� �� ����� ������ ��������� ���������� � ��� 
			ProxyList _srcCache;
			//
			ProxyList _deleteProxyList;

			//
			bool _initX3dAudio;
			X3DAUDIO_HANDLE _x3dAudio;
			Source3dList _proxy3dList;

			//�������� ����������
			Proxy* CreateProxy();
			void DoDeleteProxy(Proxy* proxy);
			ProxyList::const_iterator DeleteProxy(ProxyList::const_iterator iter);
			void DeleteProxy(Proxy* proxy);
			void DeleteProxys();

			//����������� � �����
			bool InsertPool(Proxy* proxy);
			void RemovePool(ProxyList::iterator iter);
			void RemovePool(Proxy* proxy);
			void ClearPool();
			void OptimizePool();

			//����������� � �����
			void InsertCache(Proxy* proxy);
			ProxyList::iterator RemoveCache(ProxyList::iterator iter);
			ProxyList::iterator RemoveCache(Proxy* proxy);
			void ClearCache();
			Proxy* FlushCache(Sound* sound);

			//���������, ������������ ����������
			Proxy* AllocProxy(Sound* sound);
			void ReleaseProxy(Proxy* proxy, bool unload);
			void ComputeProxy(Proxy* proxy);

			void InitX3dAudio();
			void Changed3d();
			void RegisterSource3d(Source3d* proxy);
			void UnregisterSource3d(Source3d* proxy);

			IXAudio2* GetXAudio();
			X3DAUDIO_HANDLE* GetX3dAudio();
		public:
			Engine();
			~Engine() override;

			void Init();
			void Free();

			void Compute(float deltaTime);

			SoundLib* CreateSoundLib();
			void ReleaseSoundLib(SoundLib* lib);

			SubmixVoice* CreateSubmixVoice();
			MasteringVoice* CreateMasteringVoice();
			Source* CreateSource();
			Source3d* CreateSource3d();

			void ReleaseVoice(Voice* voice);
			void ReleaseSoundLinks(Sound* sound);

			const Listener* GetListener() const;
			void SetListener(const Listener* value);

			const XAUDIO2_DEVICE_DETAILS& GetDevCaps() const;

			MasteringVoice* GetMainVoice();

			float GetDistScaler() const;
			void SetDistScaler(float value);

			Mode3d GetMode3d() const;
			void SetMode3d(Mode3d value);
		};
	}
}
