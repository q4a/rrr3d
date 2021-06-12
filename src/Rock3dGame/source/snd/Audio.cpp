#include "stdafx.h"

#include "snd\Audio.h"

namespace r3d
{

namespace snd
{

const unsigned Sound::cBufferSize = SAFE_MIN(65536 * 20, XAUDIO2_MAX_BUFFER_BYTES);




Sound::Sound(SoundLib* lib): _lib(lib), _cacheSize(cBufferSize * 10), _isLoad(false), _unusedBufSize(0), _volume(1.0f)
{
}

Sound::~Sound()
{
	Free();
}

Sound::Buffer::Buffer(Sound* sound): _sound(sound), _data(0), _dataSize(0), _pos(0), _endPos(0)
{
}

Sound::Buffer::~Buffer()
{
	Free();
}

bool Sound::Buffer::Load(seek_pos pos)
{
	if (!_data)
	{
		_data = (char*)malloc(cBufferSize);
		_dataSize = 0;

		_pos = pos;
		ov_pcm_seek(&_sound->_oggFile, _pos);

		int ret = 1;
		int sec = 0;	
		while (ret && _dataSize < cBufferSize)
		{
			ret = ov_read(&_sound->_oggFile, _data + _dataSize, cBufferSize - _dataSize, 0, 2, 1, &sec);
			_dataSize += ret;
		}

		_endPos = ov_pcm_tell(&_sound->_oggFile);		

		return _dataSize > 0;
	}

	return true;
}

void Sound::Buffer::Free()
{
	if (_data)
	{
		free(_data);
		_data = 0;		
	}
}

bool Sound::Buffer::IsEndBuffer() const
{
	return _dataSize < cBufferSize;
}

void Sound::Buffer::AddRef() const
{
	_MyBase::AddRef();
	
	if (GetRefCnt() == 1)
		_sound->RemoveUnusedBuf(const_cast<Buffer*>(this));
}

unsigned Sound::Buffer::Release() const
{
	unsigned res = _MyBase::Release();

	if (res == 0)
		_sound->InsertUnusedBuf(const_cast<Buffer*>(this));

	return res;
}

bool Sound::Buffer::ContainPos(seek_pos pos) const
{
	return pos >= _pos && pos < _endPos;
}

bool Sound::Buffer::IsInit() const
{
	return _data ? true : false;
}

char* Sound::Buffer::GetData() const
{
	return _data;
}

int Sound::Buffer::GetDataSize() const
{
	return _dataSize;
}

seek_pos Sound::Buffer::GetPos() const
{
	return _pos;
}

seek_pos Sound::Buffer::GetEndPos() const
{
	return _endPos;
}

void Sound::InsertUnusedBuf(Buffer* buffer)
{
	BufferList::iterator iter = _unusedBuffers.Find(buffer);
	if (iter == _unusedBuffers.end())
	{
		_unusedBuffers.push_back(buffer);
		_unusedBufSize += buffer->GetDataSize();
	}
}

void Sound::RemoveUnusedBuf(BufferList::const_iterator iter)
{
	_unusedBufSize -= (*iter)->GetDataSize();
	_unusedBuffers.erase(iter);
}

void Sound::RemoveUnusedBuf(Buffer* buffer)
{
	BufferList::iterator iter = _unusedBuffers.Find(buffer); 
	if (iter != _unusedBuffers.end())	
		RemoveUnusedBuf(iter);
}

void Sound::ClearUnusedBuffers()
{
	_unusedBuffers.clear();
	_unusedBufSize = 0;
}

void Sound::OptimizeUnusedBufs()
{
	while (!_unusedBuffers.empty() && _cacheSize < _unusedBufSize)
	{
		Buffer* buffer = _unusedBuffers.front();
		RemoveUnusedBuf(_unusedBuffers.begin());

		DeleteBuffer(buffer);
	}
}

void Sound::DoInit()
{
	LSL_ASSERT(_buffers.empty());
	
	if (_wfopen_s(&_file, GetAppFilePath(_fileName).c_str(), L"rb") != 0)
		throw lsl::Error("void Sound::DoInit() " + _fileName);

	if (ov_open_callbacks(_file, &_oggFile, 0, 0, OV_CALLBACKS_DEFAULT) < 0)	
		throw lsl::Error("void Sound::DoInit(). ov_open_callbacks");

	if (ov_seekable(&_oggFile) == 0)
		throw lsl::Error("!ov_seekable");

	_vorbisInfo = *ov_info(&_oggFile, -1);

	_lockObj = GetSDK()->CreateLockedObj();
}

void Sound::DoFree()
{
	_isLoad = false;

	DeleteAllBuffers();

	ov_clear(&_oggFile);
	fclose(_file);

	GetSDK()->DestroyLockedObj(_lockObj);
}

void Sound::DoUpdate()
{
	//nothing
}

seek_pos Sound::GetPrevBuffer(Buffer* pos) const
{
	return pos && !IsEndBuffer(pos) ? pos->GetPos() - 1 : -1;
}

seek_pos Sound::GetNextBuffer(Buffer* pos) const
{
	return pos && !IsEndBuffer(pos) ? pos->GetEndPos() : -1;
}

Sound::Buffer* Sound::CreateBuffer(seek_pos pos)
{
	LSL_ASSERT(IsInit());

	seek_pos pcmTotal = GetPCMTotal();
	if (pos >= pcmTotal)
		return 0;

	Buffers::iterator iter;
	Buffer* buffer = FindBufferByPos(pos, &iter);

	//не найдено, создаем новый
	if (buffer == 0)
	{
		buffer = new Buffer(this);

		if (!buffer->Load(pos))
		{
			delete buffer;
			return NULL;
		}

		iter = _buffers.insert(iter, Buffers::value_type(pos, buffer));

		LSL_ASSERT(iter != _buffers.end());
	}

	LSL_ASSERT(buffer && buffer->IsInit());

	buffer->AddRef();

	return buffer;
}

Sound::Buffer* Sound::CreateBufferAfter(Buffer* pos)
{
	seek_pos nextPos = GetNextBuffer(pos);
	if (nextPos >= 0)
		return CreateBuffer(nextPos);
	return NULL;
}

Sound::Buffer* Sound::CreateBufferBefore(Buffer* pos)
{
	seek_pos prevPos = GetPrevBuffer(pos);
	if (prevPos >= 0)
		return CreateBuffer(prevPos);
	return NULL;
}

void Sound::DeleteBuffer(Buffer* buffer)
{
	LSL_ASSERT(buffer && buffer->GetRefCnt() == 0);

	_buffers.erase(buffer->GetPos());
	RemoveUnusedBuf(buffer);

	delete buffer;
}

void Sound::DeleteAllBuffers()
{
	for (Buffers::iterator iter = _buffers.begin(); iter != _buffers.end(); ++iter)
	{
		LSL_ASSERT(iter->second->GetRefCnt() == 0);

		delete iter->second;
	}
	_buffers.clear();
	ClearUnusedBuffers();
}

void Sound::ReleaseBuffer(Buffer* buffer)
{
	buffer->Release();

	if (!_isLoad)
		OptimizeUnusedBufs();	
}

bool Sound::IsEndBuffer(Buffer* buffer) const
{
	return IsInit() ? buffer->GetEndPos() >= GetPCMTotal() || buffer->IsEndBuffer() : false;
}

Sound::Buffer* Sound::FirstBuffer()
{
	return FindBufferByPos(0);
}

Sound::Buffer* Sound::NextBuffer(Buffer* pos)
{
	seek_pos nextPos = GetNextBuffer(pos);
	if (nextPos >= 0)
		return FindBufferByPos(nextPos);
	return NULL;
}

Sound::Buffer* Sound::PrevBuffer(Buffer* pos)
{
	seek_pos prevPos = GetPrevBuffer(pos);
	if (prevPos >= 0)
		return FindBufferByPos(prevPos);
	return NULL;	
}

Sound::Buffer* Sound::FindBufferByPos(seek_pos pos, Buffers::iterator* nearRes)
{
	Buffer* buffer = 0;

	Buffers::iterator iter = _buffers.upper_bound(pos);
	if (iter != _buffers.end() && iter->second->ContainPos(pos))
	{
		buffer = iter->second;
		
		LSL_ASSERT(buffer);
	}
	else if (iter != _buffers.begin() && (--iter)->second->ContainPos(pos))
	{
		buffer = iter->second;
		
		LSL_ASSERT(buffer);
	}

	if (nearRes)
		*nearRes = iter;

	return buffer;
}

void Sound::Load()
{
	if (!_isLoad)
	{
		Init();

		_isLoad = true;

		Buffer* buffer = CreateBuffer(0);
		while (buffer)
		{
			buffer->Release();
			buffer = CreateBufferAfter(buffer);
		}
	}
}

void Sound::Unload()
{
	if (_isLoad)
	{
		_isLoad = false;
		OptimizeUnusedBufs();
	}
}

bool Sound::IsLoad() const
{
	return _isLoad;
}

void Sound::Lock()
{
	LSL_ASSERT(IsInit());

	GetSDK()->Lock(_lockObj);
}

void Sound::Unlock()
{
	LSL_ASSERT(IsInit());

	GetSDK()->Unlock(_lockObj);
}

SoundLib* Sound::GetLib()
{
	return _lib;
}

Engine* Sound::GetEngine()
{
	return _lib->GetEngine();
}

const std::string& Sound::GetFileName() const
{
	return _fileName;
}

void Sound::SetFileName(const std::string& value)
{
	Free();
	_fileName = value;
}

unsigned Sound::GetCacheSize() const
{
	return _cacheSize;
}

void Sound::SetCacheSize(unsigned value)
{
	_cacheSize = value;
	ClearUnusedBuffers();
}

const vorbis_info& Sound::GetVorbisInfo() const
{
	LSL_ASSERT(IsInit());

	return _vorbisInfo;
}

const seek_pos Sound::GetPCMTotal() const
{
	return IsInit() ? ov_pcm_total(&_oggFile, -1) : 0;
}

float Sound::GetVolume() const
{
	return _volume;
}

void Sound::SetVolume(float value)
{
	_volume = value;
}




SoundLib::SoundLib(Engine* engine): _engine(engine)
{
}

SoundLib::~SoundLib()
{	
	Clear();
}

void SoundLib::RemoveItem(const Value& value)
{
	_MyBase::RemoveItem(value);

	_engine->ReleaseSoundLinks(value);
}

Engine* SoundLib::GetEngine()
{
	return _engine;
}




Voice::Voice(Engine* engine): _engine(engine), _outMatrix(0), _srcChannels(0), _destChannels(0)
{
}
	
Voice::~Voice()
{
	DoClearReceivers();

	if (_outMatrix)
		delete[] _outMatrix;
}

void Voice::DoClearReceivers()
{
	if (!_receivers.empty())
	{
		for (VoiceList::iterator iter = _receivers.begin(); iter != _receivers.end(); ++iter)
			(*iter)->Release();

		_receivers.clear();
	}
}

void Voice::SendReceivers(IXAudio2Voice* voice)
{
	LSL_ASSERT(voice);

	HRESULT hr;	

	if (!_receivers.empty())
	{
		XAUDIO2_VOICE_SENDS sends;

		sends.SendCount = _receivers.size();
		sends.pSends = new XAUDIO2_SEND_DESCRIPTOR[_receivers.size()];		

		int i = 0;
		for (VoiceList::iterator iter = _receivers.begin(); iter != _receivers.end(); ++iter, ++i)
		{
			sends.pSends[i].Flags = 0;
			sends.pSends[i].pOutputVoice = (*iter)->GetXVoice();
		}

		if (GetXVoice() && FAILED(hr = GetXVoice()->SetOutputVoices(&sends)))
			LSL_LOG("GetXVoice()->SetOutputVoices(&sends)");

		delete[] sends.pSends;
	}
	else
	{
		XAUDIO2_SEND_DESCRIPTOR voice;
		Voice* masterVoice = GetEngine()->GetMainVoice();
		voice.pOutputVoice = masterVoice->GetXVoice();
		voice.Flags = 0;

		XAUDIO2_VOICE_SENDS sends;
		sends.SendCount = 1;
		sends.pSends = &voice;

		if (GetXVoice() && FAILED(hr = GetXVoice()->SetOutputVoices(&sends)))
			LSL_LOG("GetXVoice()->SetOutputVoices(&sends)");
	}
}

void Voice::ApplyOutputMatrix(IXAudio2Voice* voice)
{
	LSL_ASSERT(voice);

	HRESULT hr;

	if (_outMatrix == 0)
		return;

	if (_receivers.empty())
	{
		Voice* masterVoice = GetEngine()->GetMainVoice();

		if (masterVoice->GetXVoice() && FAILED(hr = voice->SetOutputMatrix(masterVoice->GetXVoice(), _srcChannels, _destChannels, _outMatrix)))
			LSL_LOG("_xVoice->SetOutputMatrix");
	}
	else
	{
		for (VoiceList::const_iterator iter = _receivers.begin(); iter != _receivers.end(); ++iter)
		{
			if ((*iter)->GetXVoice() && FAILED(hr = voice->SetOutputMatrix((*iter)->GetXVoice(), _srcChannels, _destChannels, _outMatrix)))
				LSL_LOG("_xVoice->SetOutputMatrix");
		}
	}
}

void Voice::InsertReceiver(Voice* voice)
{
	_receivers.push_back(voice);
	voice->AddRef();

	ChangedReceivers();
}

void Voice::InsertReceiver(VoiceList::const_iterator sIter, VoiceList::const_iterator eIter)
{
	for (VoiceList::const_iterator iter = sIter; iter != eIter; ++iter)
	{
		_receivers.push_back(*iter);
		(*iter)->AddRef();
	}

	ChangedReceivers();
}

void Voice::InsertReceiver(const VoiceList& receivers)
{
	InsertReceiver(receivers.begin(), receivers.end());
}

VoiceList::iterator Voice::RemoveReceiver(VoiceList::const_iterator iter)
{
	VoiceList::iterator res = _receivers.erase(iter);
	(*iter)->Release();

	ChangedReceivers();

	return res;
}

void Voice::RemoveReceiver(Voice* voice)
{
	RemoveReceiver(_receivers.Find(voice));
}

void Voice::ClearReceivers()
{
	DoClearReceivers();

	ChangedReceivers();
}

Engine* Voice::GetEngine()
{
	return _engine;
}

void Voice::SetOutputMatrix(int srcChannels, int destChannels, const float* matrix)
{
	if (_outMatrix)
	{
		delete[] _outMatrix;
		_outMatrix = 0;
	}

	if (matrix)
	{
		_outMatrix = new float[srcChannels * destChannels];
		memcpy(_outMatrix, matrix, srcChannels * destChannels * sizeof(float));

		_srcChannels = srcChannels;
		_destChannels = destChannels;
	}

	ChangedOutMatrix();
}

void Voice::SetDefOutputMatrix(int srcChannels, int destChannels, float volume)
{
	float* matrix = new float[srcChannels * destChannels];

	//алгоритм распределяет на каждый выходной канал по одному входному каналу с повторением
	//Например:
	//2.0 to 5.1 with 0.5 volume coef
	//const float matrix[12] = {1.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.0f, 0.0f, 0.5f, 0.5f, 0.0f, 0.0f, 0.5f};

	int numChannel = 0;
	for (int i = 0; i < destChannels; ++i)
	{
		ZeroMemory(&matrix[i * srcChannels], sizeof(float) * srcChannels);
		matrix[i * srcChannels + numChannel] = volume;

		if (++numChannel >= srcChannels)
			numChannel = 0;
	}

	SetOutputMatrix(srcChannels, destChannels, matrix);

	delete[] matrix;
}

const VoiceList& Voice::GetReceivers() const
{
	return _receivers;
}

float Voice::GetVolume()
{
	float val = 0.0f;

	if (GetXVoice())
		GetXVoice()->GetVolume(&val);

	return val;
}

void Voice::SetVolume(float value)
{
	if (GetXVoice())
		GetXVoice()->SetVolume(value);
}




SubmixVoice::SubmixVoice(Engine* engine): _MyBase(engine), _xVoice(0)
{
	HRESULT hr;

	if (engine->GetXAudio() && FAILED(hr = engine->GetXAudio()->CreateSubmixVoice(&_xVoice, engine->GetDevCaps().OutputFormat.Format.nChannels, engine->GetDevCaps().OutputFormat.Format.nSamplesPerSec, 0, 0, 0)))
	{
		LSL_LOG("Error create SubmixVoice");
	}
}
	
SubmixVoice::~SubmixVoice()
{
	if (_xVoice)
		_xVoice->DestroyVoice();
}

void SubmixVoice::ChangedReceivers()
{
	if (_xVoice)
		SendReceivers(_xVoice);
}

void SubmixVoice::ChangedOutMatrix()
{
	if (_xVoice)
		ApplyOutputMatrix(_xVoice);
}

IXAudio2Voice* SubmixVoice::GetXVoice()
{
	return _xVoice;
}




MasteringVoice::MasteringVoice(Engine* engine): _MyBase(engine), _xVoice(0)
{
	if (engine->GetXAudio() && FAILED(engine->GetXAudio()->CreateMasteringVoice(&_xVoice, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, 0, 0)))
	{
		LSL_LOG("Error create MasteringVoice");
	}
}
	
MasteringVoice::~MasteringVoice()
{
	if (_xVoice)
		_xVoice->DestroyVoice();	
}

void MasteringVoice::ChangedReceivers()
{
	if (_xVoice)
		SendReceivers(_xVoice);
}

void MasteringVoice::ChangedOutMatrix()
{
	if (_xVoice)
		ApplyOutputMatrix(_xVoice);
}

IXAudio2Voice* MasteringVoice::GetXVoice()
{
	return _xVoice;
}




Proxy::Proxy(Engine* engine): _MyBase(engine), _init(false), _sound(0), _pos(0), _playMode(pmOnce), _volume(1.0f), _frequencyRatio(1.0f), _onStreamEnd(false), _onTerminate(false), _xVoice(0), _run(false), _streaming(NULL)
{	
}
	
Proxy::~Proxy()
{
	Free();

	for (ReportList::iterator iter = _reportList.begin(); iter != _reportList.end(); ++iter)	
		(*iter)->Release();
	_reportList.Clear();

	SetSound(0);
}

Proxy::VoiceCallback::VoiceCallback(Proxy* proxy): _proxy(proxy)
{
}

void Proxy::VoiceCallback::OnStreamEnd()
{
	switch (_proxy->_playMode)
	{
	case pmOnce:
		_proxy->Stop();
		_proxy->SetPos(0);
		break;
		
	case pmCycle:
		_proxy->SetPos(-1);
		break;

	case pmInfite:
		_proxy->SetPos(-1);
		break;
	}
	
	_proxy->OnStreamEnd();
}

void Proxy::VoiceCallback::OnVoiceProcessingPassEnd()
{
}

void Proxy::VoiceCallback::OnVoiceProcessingPassStart(UINT32 SamplesRequired)
{
}

void Proxy::VoiceCallback::OnBufferEnd(void* pBufferContext)
{
	_proxy->_streaming->ReleaseBuffer(reinterpret_cast<Buffer*>(pBufferContext));
}

void Proxy::VoiceCallback::OnBufferStart(void* pBufferContext)
{
}

void Proxy::VoiceCallback::OnLoopEnd(void* pBufferContext)
{
}

void Proxy::VoiceCallback::OnVoiceError(void* pBufferContext, HRESULT Error)
{
}

Proxy::Streaming::Streaming(Proxy* proxy): _proxy(proxy), _play(false), _pos(0), _posChanged(false), _bufSize(Sound::cBufferSize * 5), _cacheSize(Sound::cBufferSize * 5), _dataSizeBuffers(0), _firstBufPos(0), _cached(false)
{
	_process = GetSDK()->CreateThreadEvent(true, true, "");
	_unusedBufsLock = GetSDK()->CreateLockedObj();
}

Proxy::Streaming::~Streaming()
{
	GetSDK()->DestroyLockedObj(_unusedBufsLock);
	GetSDK()->DestroyThreadEvent(_process);
}

bool Proxy::Streaming::RemoveBuffer(Buffers::iterator iter, bool checkVoiceState)
{
	Buffer* buffer = *iter;

	if (checkVoiceState)
	{
		XAUDIO2_VOICE_STATE state;
		_proxy->_xVoice->GetState(&state);
		if (state.pCurrentBufferContext == buffer)
			return false;
	}

	_buffers.erase(iter);
	_dataSizeBuffers -= buffer->GetDataSize();

	buffer->Release();
	_proxy->_sound->ReleaseBuffer(buffer);

	if (_buffers.empty())
	{
		_dataSizeBuffers = 0;
		_firstBufPos = 0;
	}

	return true;
}

bool Proxy::Streaming::RemoveBuffer(Buffer* buffer)
{
	int cnt = 0;

	for (Buffers::iterator iter = _buffers.begin(); iter != _buffers.end(); ++iter)
		if (*iter == buffer)
		{			
			if (RemoveBuffer(iter, cnt == 0))
				return true;

			++cnt;
		}

	return false;
}

void Proxy::Streaming::PushBackBuffer(Buffer* buffer, bool endOfStream, seek_pos offs)
{
	buffer->AddRef();
	_buffers.push_back(buffer);
	_dataSizeBuffers += buffer->GetDataSize();

	XAUDIO2_BUFFER xBuf;
	ZeroMemory(&xBuf, sizeof(xBuf));
	xBuf.pAudioData = reinterpret_cast<BYTE*>(buffer->GetData());
	xBuf.AudioBytes = buffer->GetDataSize();
	xBuf.PlayBegin = static_cast<UINT32>(offs);
	xBuf.LoopBegin = XAUDIO2_NO_LOOP_REGION;
	xBuf.Flags = endOfStream ? XAUDIO2_END_OF_STREAM : 0;
	xBuf.pContext = buffer;

	HRESULT hr;

	if(FAILED(hr = _proxy->_xVoice->SubmitSourceBuffer(&xBuf)))
	{
		throw lsl::Error("void Proxy::Buffering()");
	}

	if (_buffers.size() == 1)
		_firstBufPos = buffer->GetPos();
}

void Proxy::Streaming::DeleteBuffers()
{
	if (_buffers.empty())
		return;

	_proxy->_xVoice->Stop();
	_proxy->_xVoice->FlushSourceBuffers();

	//Внимание! Если буффер на данный момент проигрывается то он мог бы быть не удален из очереди в результате ожет быть даже вызван несущесвтующий каллбек для прокси, либо в методе DoStream нерпавильно забуферизован первый буффер. Полагаюсь на  то что метод Stop выполняемый синхронно останалвивает воспроизведение полностью
	while (!_buffers.empty())
		RemoveBuffer(_buffers.begin(), false);
}

bool Proxy::Streaming::ApplyPosChanged()
{
	if (_posChanged)
	{
		_posChanged = false;
		
		_proxy->_sound->Lock();
		try
		{
			DeleteBuffers();
		}
		LSL_FINALLY(_proxy->_sound->Unlock();)

		return true;
	}
	else
		return false;
}

bool Proxy::Streaming::RemoveUnusedBufs()
{
	bool res = false;

	GetSDK()->Lock(_unusedBufsLock);
	try
	{
		if (_unusedBuffers.empty())
		{
			res = false;			
		}
		else
		{
			_proxy->_sound->Lock();
			try
			{
				for (Buffers::iterator iter = _unusedBuffers.begin(); iter != _unusedBuffers.end();)
				{
					//удалем по значению, т.к. другой конейнер!!!
					if (RemoveBuffer(*iter))
						iter = _unusedBuffers.erase(iter);
					else
						++iter;
				}
			}
			LSL_FINALLY(_proxy->_sound->Unlock();)

			res = true;
		}
	}
	LSL_FINALLY(GetSDK()->Unlock(_unusedBufsLock);)

	return res;
}

bool Proxy::Streaming::DoStream()
{
	unsigned minBufSize = _bufSize;
	if (_cached)
	{
		//состояние кеширование true пока загруженные данные первышают уровень буферизации
		_cached = _dataSizeBuffers >= minBufSize;
	}
	else
	{
		minBufSize += _cacheSize;
		//переход в состояние кеширования только при дополнительном превышении _proxy->_cacheSize
		_cached = _dataSizeBuffers >= minBufSize;
	}	

	bool work = _buffers.size() < XAUDIO2_MAX_QUEUED_BUFFERS && _dataSizeBuffers < minBufSize;

	if (work)
	{
		Buffer* buffer = 0;
		seek_pos offs = 0;

		_proxy->_sound->Lock();
		try
		{
			if (!_buffers.empty())
			{
				buffer = _proxy->_sound->CreateBufferAfter(_buffers.back());
			}
			else
			{	
				buffer = _proxy->_sound->CreateBuffer(_pos);
				if (buffer)
					offs = _pos - buffer->GetPos();
			}
		}
		LSL_FINALLY(_proxy->_sound->Unlock();)

		if (buffer)
			PushBackBuffer(buffer, _proxy->_sound->IsEndBuffer(buffer), std::max<seek_pos>(offs, 0));

		if (_buffers.size() == 1 && _proxy->_sound->IsEndBuffer(_buffers.back()) && _proxy->_playMode != pmOnce)
		{
			Buffer* firstBuffer = _proxy->_sound->CreateBuffer(0);
			PushBackBuffer(firstBuffer, firstBuffer == _buffers.back(), 0);
		}

		work = buffer != 0;		
	}

	return work;
}

bool Proxy::Streaming::DoPlay(bool work)
{
	//проигровать только если кеширование завершено или работа по загрузке ресурсов окончена
	if (_play && (_dataSizeBuffers >= _bufSize || !work))
	{
		_proxy->_xVoice->Start();
	}
	else if (!_play)
	{
		_proxy->_xVoice->Stop();
	}

	return false;
}

void Proxy::Streaming::Changed()
{
	_process->Set();
}

void Proxy::Streaming::Execute(lsl::Object* arg)
{
	do
	{
		//закрываем событие перед обработкой, посколку имзенения применяются в первый проход
		_process->Reset();

		bool work = false;

		ApplyPosChanged();
		RemoveUnusedBufs();
		work |= DoStream();
		DoPlay(work);

		//если больше нет работы, впадаем вспячку
		if (!IsTerminating() && !work)
			_process->WaitOne();
	}
	while (!IsTerminating());

	DeleteBuffers();
}

void Proxy::Streaming::OnTerminate()
{
	_proxy->OnTerminate();
}

void Proxy::Streaming::Terminate()
{
	Stop();

	_MyBase::Terminate();

	Changed();	
}

void Proxy::Streaming::WaitTerminate()
{
	while (IsTerminating()) {}
}

void Proxy::Streaming::Play()
{
	if (!_play && !IsTerminating() && _pos < _proxy->_sound->GetPCMTotal())
	{
		_play = true;
		Changed();
	}
}

void Proxy::Streaming::Stop()
{
	if (_play)
	{
		_play = false;
		_proxy->_xVoice->Stop();
		//Changed();
	}
}

bool Proxy::Streaming::IsPlaying() const
{
	return _play;
}

void Proxy::Streaming::ReleaseBuffer(Buffer* buffer)
{
	GetSDK()->Lock(_unusedBufsLock);
	try
	{	
		_unusedBuffers.push_back(buffer);
	}
	LSL_FINALLY(GetSDK()->Unlock(_unusedBufsLock);)

	Changed();
}

seek_pos Proxy::Streaming::GetPos() const
{
	XAUDIO2_VOICE_STATE state;
	_proxy->_xVoice->GetState(&state);

	return state.SamplesPlayed + _firstBufPos;
}

void Proxy::Streaming::SetPos(seek_pos pos)
{
	_pos = std::max<seek_pos>(pos, 0);

	if (pos >= 0)
	{
		_posChanged = true;
		Changed();
	}
}

void Proxy::OnStreamEnd()
{
	_onStreamEnd = true;	
}

void Proxy::OnTerminate()
{
	_onTerminate = true;
}

void Proxy::SendReports()
{
	try
	{
		for (ReportList::Position pos = _reportList.First(); Report** iter = _reportList.Current(pos); _reportList.Next(pos))
		{
			Report* report = *iter;
			if (_onStreamEnd)
				report->OnStreamEnd(this, _playMode);
			if (_onTerminate)
				report->OnTerminate(this);
		}
	}
	LSL_FINALLY(_onStreamEnd = false; _onTerminate = false;)	
}

void Proxy::ChangedReceivers()
{
	if (_xVoice)
		SendReceivers(_xVoice);
}

void Proxy::ChangedOutMatrix()
{
	if (_xVoice)
		ApplyOutputMatrix(_xVoice);
}

IXAudio2Voice* Proxy::GetXVoice()
{
	return _xVoice;
}

void Proxy::Init()
{
	HRESULT hr;

	if (!_init)
	{
		LSL_ASSERT(_sound);

		_init = true;

		_sound->Init();

		_voiceCallback = new VoiceCallback(this);

		WAVEFORMATEX wfm;
		ZeroMemory(&wfm, sizeof(wfm));
		wfm.cbSize          = sizeof(wfm);
		wfm.nChannels       = _sound->GetVorbisInfo().channels;
		wfm.wBitsPerSample  = 16;
		wfm.nSamplesPerSec  = _sound->GetVorbisInfo().rate;
		wfm.nAvgBytesPerSec = wfm.nSamplesPerSec * wfm.nChannels * 2;
		wfm.nBlockAlign     = 2 * wfm.nChannels;
		wfm.wFormatTag      = 1;

		if (GetEngine()->GetXAudio() && FAILED(hr = GetEngine()->GetXAudio()->CreateSourceVoice(&_xVoice, &wfm, 0, XAUDIO2_DEFAULT_FREQ_RATIO, _voiceCallback, 0, 0)))
		{
			LSL_LOG("Failed CreateProxyVoice");
		}

		if (_xVoice)
		{
			SendReceivers(_xVoice);
			ApplyOutputMatrix(_xVoice);

			_xVoice->SetVolume(_volume * _sound->GetVolume());
			_xVoice->SetFrequencyRatio(_frequencyRatio);

			_streaming = new Streaming(this);
			_streaming->SetPos(_pos);
		}
	}

	Run();
}

void Proxy::Free()
{
	if (_init)
	{
		Terminate();
		WaitTerminate();

		_init = false;

		if (_streaming)
			_pos = _streaming->GetPos();

		//may makes any callbacks
		if (_xVoice)
		{
			_xVoice->DestroyVoice();
			_xVoice = 0;
		}

		_onStreamEnd = false;
		_onTerminate = false;

		lsl::SafeDelete(_streaming);
		delete _voiceCallback;
	}
}

bool Proxy::IsInit() const
{
	return _init;
}

void Proxy::Run()
{
	if (!_run && IsInit())
	{
		//на всякий случай, если пердыдущий запуск ещё не завершился
		WaitTerminate();

		_run = true;

		if (_streaming)
		{
			ThreadPool::Flags flags;
			flags |= ThreadPool::tfBackground;
			GetSDK()->GetThreadPool()->QueueWork(_streaming, 0, flags);
		}
	}
}

void Proxy::Terminate()
{
	if (_run)
	{
		_run = false;

		if (_streaming)
			_streaming->Terminate();
	}
}

bool Proxy::IsRunning() const
{
	return _run;
}

bool Proxy::IsTerminating() const
{
	return IsInit() && _streaming ? _streaming->IsTerminating() : false;
}

void Proxy::WaitTerminate()
{
	if (IsInit() && _streaming)
		_streaming->WaitTerminate();
}

void Proxy::RegReport(Report* report)
{
	ReportList::iterator iter = _reportList.Find(report);
	if (iter == _reportList.end())
	{
		_reportList.Insert(report);
		report->AddRef();
	}	
}

void Proxy::UnregReport(Report* report)
{
	ReportList::iterator iter = _reportList.Find(report);
	if (iter != _reportList.end())
	{
		_reportList.Remove(iter);
		report->Release();
	}
}

void Proxy::Play()
{
	//if (_sound && _sound->GetFileName() != "Data\\Sounds\\shredder.ogg")
	//	return;

	Init();

	if (_streaming)
		_streaming->Play();
}

void Proxy::Stop()
{
	if (IsInit() && _streaming)
		_streaming->Stop();
}

bool Proxy::IsPlaying() const
{
	return IsInit() && _streaming ? _streaming->IsPlaying() : false;
}

Sound* Proxy::GetSound()
{
	return _sound;
}

void Proxy::SetSound(Sound* value)
{
	if (ReplaceRef(_sound, value))
	{
		if (IsSoundCompilant(value))
		{
			Terminate();
			WaitTerminate();
		}
		else
			Free();

		_sound = value;
	}
}

bool Proxy::IsSoundCompilant(Sound* value) const
{
	if (IsInit())	
		return _sound == value || (_sound->GetVorbisInfo().channels == value->GetVorbisInfo().channels && _sound->GetVorbisInfo().rate == value->GetVorbisInfo().rate);
	else
		return false;
}

seek_pos Proxy::GetPos() const
{
	return IsInit() && _streaming ? _streaming->GetPos() : _pos;
}

void Proxy::SetPos(seek_pos value)
{
	_pos = std::max<seek_pos>(value, 0);

	if (IsInit() && _streaming)
		_streaming->SetPos(value);
}

PlayMode Proxy::GetPlayMode() const
{
	return _playMode;
}

void Proxy::SetPlayMode(PlayMode value)
{
	_playMode = value;
}

float Proxy::GetVolume() const
{
	return _volume;
}

void Proxy::SetVolume(float value)
{
	_volume = value;

	if (_xVoice)
		_xVoice->SetVolume(_volume * _sound->GetVolume());
}

float Proxy::GetFrequencyRatio() const
{
	return _frequencyRatio;
}

void Proxy::SetFrequencyRatio(float value)
{
	_frequencyRatio = value;

	if (_xVoice)
		_xVoice->SetFrequencyRatio(value);
}




Source::Source(Engine* engine): _MyBase(engine), _sound(0), _pos(0), _playMode(pmOnce), _volume(1.0f), _frequencyRatio(1.0f), _proxy(0)
{
}

Source::~Source()
{
	ClearReportList();
	Free();

	SetSound(0);
}

void Source::ChangedReceivers()
{
	if (_proxy)
	{
		_proxy->ClearReceivers();
		_proxy->InsertReceiver(GetReceivers());
	}
}

void Source::ChangedOutMatrix()
{
	if (_proxy)
		_proxy->SetOutputMatrix(_srcChannels, _destChannels, _outMatrix);
}

IXAudio2Voice* Source::GetXVoice()
{
	return 0;
}

void Source::ClearReportList()
{
	for (ReportList::const_iterator iter = _reportList.begin(); iter != _reportList.end(); ++iter)
	{
		if (_proxy)
			_proxy->UnregReport(*iter);
		(*iter)->Release();	
	}
	_reportList.clear();
}

void Source::DoInit()
{
	//nothing
}

void Source::DoFree()
{
	//nothing
}

void Source::Init()
{
	if (!_proxy)
	{
		LSL_ASSERT(_sound);

		_sound->Init();

		_proxy = GetEngine()->AllocProxy(_sound);
		_proxy->AddRef();		

		_proxy->SetPos(_pos);
		_proxy->SetPlayMode(_playMode);
		_proxy->SetVolume(_volume);
		_proxy->SetFrequencyRatio(_frequencyRatio);

		_proxy->InsertReceiver(GetReceivers());
		for (ReportList::const_iterator iter = _reportList.begin(); iter != _reportList.end(); ++iter)
			_proxy->RegReport(*iter);		

		int nSrcChannels = _sound->GetVorbisInfo().channels;
		int nDestChannels = GetEngine()->GetDevCaps().OutputFormat.Format.nChannels;
		if (_outMatrix == 0)
			_proxy->SetDefOutputMatrix(nSrcChannels, nDestChannels, 1.0f);
		else
			_proxy->SetOutputMatrix(_srcChannels, _destChannels, _outMatrix);

		DoInit();
	}
}
	
void Source::Free(bool unload)
{
	if (_proxy)
	{
		Stop();
		DoFree();
		_pos = _proxy->GetPos();

		for (ReportList::const_iterator iter = _reportList.begin(); iter != _reportList.end(); ++iter)
			_proxy->UnregReport(*iter);
		_proxy->ClearReceivers();
		
		_proxy->Release();
		GetEngine()->ReleaseProxy(_proxy, unload);
		_proxy = 0;
	}
}

Proxy* Source::GetProxy()
{
	return _proxy;
}

void Source::Play()
{
	Init();

	_proxy->Play();
}

void Source::Stop()
{
	if (_proxy)
		_proxy->Stop();
}

bool Source::IsPlaying() const
{
	return _proxy ? _proxy->IsPlaying() : false;
}

void Source::RegReport(Report* report)
{
	report->AddRef();
	_reportList.push_back(report);	

	if (_proxy)
		_proxy->RegReport(report);
}

void Source::UnregReport(Report* report)
{
	if (_proxy)
		_proxy->UnregReport(report);

	report->Release();
	_reportList.Remove(report);	
}

Sound* Source::GetSound()
{
	return _sound;
}

void Source::SetSound(Sound* value, bool unload)
{
	if (ReplaceRef(_sound, value))
	{
		Free(unload);

		_sound = value;
	}
}

seek_pos Source::GetPos() const
{
	return _proxy ? _proxy->GetPos() : _pos;
}

void Source::SetPos(seek_pos value)
{
	_pos = value;

	if (_proxy)
		_proxy->SetPos(value);
}

PlayMode Source::GetPlayMode() const
{
	return _playMode;
}

void Source::SetPlayMode(PlayMode value)
{
	_playMode = value;

	if (_proxy)
		_proxy->SetPlayMode(value);
}

float Source::GetVolume() const
{
	return _volume;
}

void Source::SetVolume(float value)
{
	_volume = value;

	if (_proxy)
		_proxy->SetVolume(value);
}

float Source::GetFrequencyRatio() const
{
	return _frequencyRatio;
}

void Source::SetFrequencyRatio(float value)
{
	_frequencyRatio = value;

	if (_proxy)
		_proxy->SetFrequencyRatio(value);
}




Source3d::Source3d(Engine* engine): _MyBase(engine), _pos3d(NullVector), _distScaler(0), _changed3d(false), _play(false), _playTime(0)
{
	_myReport = new MyReport(this);

	ZeroMemory(&_dspSettings, sizeof(_dspSettings));
	ZeroMemory(&_xEmitter, sizeof(_xEmitter));

	engine->RegisterSource3d(this);
}

Source3d::~Source3d()
{
	Free();

	GetEngine()->UnregisterSource3d(this);

	CleanUpX3d();

	delete _myReport;
}

Source3d::MyReport::MyReport(Source3d* source): _source(source)
{
}

void Source3d::MyReport::OnStreamEnd(Proxy* sender, PlayMode mode)
{
	if (mode == pmOnce)
		_source->_play = false;
}

void Source3d::ApplyX3dEffect()
{
	const float distScaller = GetEngine()->GetDistScaler() + _distScaler;
	const float stopLag = distScaller * 0.5f;
	const Listener* listener = GetEngine()->GetListener();

	if (!(GetProxy() && listener && _play))
		return;	

	D3DXVECTOR3 listDist = _pos3d - listener->pos;
	float listDistLen = D3DXVec3Length(&listDist);

	if (listDistLen > distScaller + stopLag)
	{
		this->GetProxy()->Stop();
		return;
	}
	
	CleanUpX3d();
	
	int nSrcChannels = GetSound()->GetVorbisInfo().channels;
	int nDestChannels = GetEngine()->GetDevCaps().OutputFormat.Format.nChannels;

	if (listener)
	{
		switch (GetEngine()->GetMode3d())
		{
		case Engine::m3dSurround:
		{
			if (GetEngine()->GetX3dAudio() == NULL)
				break;

			X3DAUDIO_LISTENER xList;
			ZeroMemory(&xList, sizeof(xList));
			xList.Position = listener->pos;
			D3DXMATRIX rotMat;
			D3DXMatrixRotationQuaternion(&rotMat, &listener->rot);
			xList.OrientFront = D3DXVECTOR3(rotMat.m[0]);
			xList.OrientTop = D3DXVECTOR3(rotMat.m[2]);

			_xEmitter.ChannelCount = nSrcChannels;
			_xEmitter.CurveDistanceScaler = distScaller;
			_xEmitter.Position = _pos3d;
			_xEmitter.OrientFront = XVector;
			_xEmitter.OrientTop = ZVector;
			_xEmitter.pChannelAzimuths = new float[nSrcChannels];
			ZeroMemory(_xEmitter.pChannelAzimuths, nSrcChannels);
			for (int i = 0; i < nSrcChannels; ++i)
				_xEmitter.pChannelAzimuths[i] = 1.0f;

			_dspSettings.SrcChannelCount = nSrcChannels;
			_dspSettings.DstChannelCount = nDestChannels;
			_dspSettings.pMatrixCoefficients = new float[nSrcChannels * nDestChannels];

			X3DAudioCalculate(*GetEngine()->GetX3dAudio(), &xList, &_xEmitter, X3DAUDIO_CALCULATE_MATRIX, &_dspSettings);
			
			SetOutputMatrix(nSrcChannels, nDestChannels, _dspSettings.pMatrixCoefficients);
			
			break;
		}
			
		case Engine::m3dFlat:
		{
			SetDefOutputMatrix(nSrcChannels, nDestChannels, lsl::ClampValue(1.0f - listDistLen / distScaller, 0.0f, 1.0f));

			break;
		}

		}
	}
	else
	{
		SetDefOutputMatrix(nSrcChannels, nDestChannels, 1.0f);
	}

	//воспроизводим только после всех установок чтобы избежать несогласованностей
	if (listDistLen < distScaller)
	{
		this->GetProxy()->Play();
	}
}

void Source3d::CleanUpX3d()
{
	if (_dspSettings.pMatrixCoefficients)
	{
		delete[] _dspSettings.pMatrixCoefficients;
		_dspSettings.pMatrixCoefficients = 0;
	}
	if (_xEmitter.pChannelAzimuths)
	{
		delete[] _xEmitter.pChannelAzimuths;
		_xEmitter.pChannelAzimuths = 0;
	}
}

void Source3d::DoInit()
{
	_MyBase::DoInit();

	GetProxy()->RegReport(_myReport);
}

void Source3d::DoFree()
{
	GetProxy()->UnregReport(_myReport);
}

void Source3d::ApplyChanges3d()
{
	if (_changed3d)
	{
		_changed3d = false;

		ApplyX3dEffect();
	}
}

void Source3d::Changed3d()
{
	_changed3d = true;
}

void Source3d::Play()
{
	Init();

	if (!_play)
	{
		_play = true;
		_playTime = lsl::GetTimeDbl();
		Changed3d();
		ApplyChanges3d();
	}
}

void Source3d::Stop()
{
	_play = false;

	_MyBase::Stop();
}

bool Source3d::IsPlaying() const
{
	return _play;
}

const D3DXVECTOR3& Source3d::GetPos3d()
{
	return _pos3d;
}

void Source3d::SetPos3d(const D3DXVECTOR3& value)
{
	_pos3d = value;
	Changed3d();
}

float Source3d::GetDistScaler() const
{
	return _distScaler;
}

void Source3d::SetDistScaler(float value)
{
	_distScaler = value;
	Changed3d();
}




Engine::Engine(): _mainVoice(0), _xAudio(0), _initX3dAudio(false), _listener(0), _distScaler(30.0f), _mode3d(m3dFlat), _changed3d(false), _poolMaxSize(20), _isComputing(false)
{
	ZeroMemory(&_xDevCaps, sizeof(_xDevCaps));	
}

Engine::~Engine()
{
	LSL_ASSERT(_soundLibList.empty() && _voiceList.size() <= 1);

	ClearCache();
	ClearPool();
	DeleteProxys();
	Free();

	lsl::SafeDelete(_listener);
}

Proxy* Engine::CreateProxy()
{
	Proxy* proxy = new Proxy(this);
	proxy->AddRef();

	_proxyList.push_back(proxy);

	return proxy;
}

void Engine::DoDeleteProxy(Proxy* proxy)
{
	proxy->Release();
	delete proxy;
}

Engine::ProxyList::const_iterator Engine::DeleteProxy(ProxyList::const_iterator iter)
{
	if (_isComputing)
	{
		if (!_deleteProxyList.IsFind(*iter))
			_deleteProxyList.push_back(*iter);

		return ++iter;
	}

	DoDeleteProxy(*iter);

	return _proxyList.erase(iter);
}

void Engine::DeleteProxy(Proxy* proxy)
{
	DeleteProxy(_proxyList.Find(proxy));
}

void Engine::DeleteProxys()
{
	LSL_ASSERT(!_isComputing);

	for (ProxyList::iterator iter = _proxyList.begin(); iter != _proxyList.end(); ++iter)
		DoDeleteProxy(*iter);

	_proxyList.clear();
}

bool Engine::InsertPool(Proxy* proxy)
{
	if (_poolMaxSize > _srcPool.size())
	{
		_srcPool.push_back(proxy);
		proxy->AddRef();

		return true;
	}
	else
		return false;
}

void Engine::RemovePool(ProxyList::iterator iter)
{
	Proxy* proxy = *iter;
	proxy->Release();
	_srcPool.erase(iter);
}

void Engine::RemovePool(Proxy* proxy)
{
	RemovePool(_srcPool.Find(proxy));
}

void Engine::ClearPool()
{
	for (ProxyList::iterator iter = _srcPool.begin(); iter != _srcPool.end(); ++iter)
		(*iter)->Release();
	_srcPool.clear();
}

void Engine::OptimizePool()
{
	if (_isComputing)
		return;

	while (_poolMaxSize < _srcPool.size())
	{
		Proxy* proxy = *_srcPool.begin();
		RemovePool(_srcPool.begin());
		
		DeleteProxy(proxy);
	}
}

void Engine::InsertCache(Proxy* proxy)
{
	_srcCache.push_back(proxy);
	proxy->AddRef();	
}

Engine::ProxyList::iterator Engine::RemoveCache(ProxyList::iterator iter)
{
	(*iter)->Release();	

	return _srcCache.erase(iter);	
}

Engine::ProxyList::iterator Engine::RemoveCache(Proxy* proxy)
{
	return RemoveCache(_srcCache.Find(proxy));	
}

void Engine::ClearCache()
{
	for (ProxyList::iterator iter = _srcCache.begin(); iter != _srcCache.end(); ++iter)
		(*iter)->Release();

	_srcCache.clear();	
}

Proxy* Engine::FlushCache(Sound* sound)
{
	Proxy* proxy = 0;

	for (ProxyList::iterator iter = _srcCache.begin(); iter != _srcCache.end();)
	{
		Proxy* cacheSrc = *iter;

		//прокси которые ещё в процессе завершения остаются в кеше
		if (cacheSrc->IsTerminating())
		{
			++iter;
		}
		//ищем совместимый звук
		else if (!proxy && sound && cacheSrc->IsSoundCompilant(sound))
		{
			proxy = cacheSrc;
			iter = RemoveCache(iter);
		}
		//вставляем в пул
		//нельзя удалять прокси в процессе их расчета (но добавлять в виду специфики списка можно)
		else if (!_isComputing || _poolMaxSize > _srcPool.size())
		{
			iter = RemoveCache(iter);
			if (!InsertPool(cacheSrc))
				DeleteProxy(cacheSrc);
		}
		else
			++iter;
	}

	return proxy;
}

Proxy* Engine::AllocProxy(Sound* sound)
{
	Proxy* proxy = 0;

	if (sound)	
	{
		//поиск в пуле
		for (ProxyList::iterator iter = _srcPool.begin(); iter != _srcPool.end(); ++iter)		
			if ((*iter)->IsSoundCompilant(sound))
			{
				proxy = *iter;
				RemovePool(iter);
				break;
			}

		//поиск в кеше, с предварительной очисткой		
		if (!proxy)
		{
			OptimizePool();
			proxy = FlushCache(sound);
		}
	}

	//не надено, создаем
	if (!proxy)	
		proxy = CreateProxy();
	else
		//выполянем дополнительную итерацию расчета на случай если прокси лежат в пуле очень мало и его прошлый владелец не успел его обсчитать
		//чтобы избежать некорректных данных для нового владельца
		ComputeProxy(proxy);

	proxy->SetSound(sound);

	return proxy;
}

void Engine::ReleaseProxy(Proxy* proxy, bool unload)
{	
	if (proxy->IsInit() && !unload)
	{
		proxy->Terminate();
		InsertCache(proxy);
	}
	else
	{
		proxy->Terminate();
		proxy->WaitTerminate();
		DeleteProxy(proxy);
	}
}

void Engine::ComputeProxy(Proxy* proxy)
{
	proxy->SendReports();
}

void Engine::InitX3dAudio()
{
	if (!_initX3dAudio && _xAudio)
	{
		_initX3dAudio = true;

		X3DAudioInitialize(_xDevCaps.OutputFormat.dwChannelMask, X3DAUDIO_SPEED_OF_SOUND, _x3dAudio);		
	}
}

void Engine::Changed3d()
{
	_changed3d = true;
}

void Engine::RegisterSource3d(Source3d* proxy)
{
	_proxy3dList.push_back(proxy);
}

void Engine::UnregisterSource3d(Source3d* proxy)
{
	_proxy3dList.Remove(proxy);
}

IXAudio2* Engine::GetXAudio()
{
	return _xAudio;
}

X3DAUDIO_HANDLE* Engine::GetX3dAudio()
{
	InitX3dAudio();

	return &_x3dAudio;
}

void Engine::Init()
{
	HRESULT hr = S_OK;

	if (FAILED(hr = XAudio2Create(&_xAudio, 0, XAUDIO2_DEFAULT_PROCESSOR)))	
	{
		LSL_LOG("Error create _xAudio");

		_xAudio = NULL;
	}

	if (_xAudio && FAILED(hr = _xAudio->GetDeviceDetails(0, &_xDevCaps)))
	{
		LSL_LOG("FAILED(hr = xAudio->GetDeviceDetails(0, &_xDevCaps))");

		_xAudio->Release();
		_xAudio = NULL;
	}

	_mainVoice = this->CreateMasteringVoice();
	_mainVoice->SetVolume(0.1f);
}

void Engine::Free()
{
	delete _mainVoice;
	_mainVoice = 0;

	if (_xAudio)
	{
		_xAudio->Release();
		_xAudio = NULL;
	}
}

void Engine::Compute(float deltaTime)
{
	_isComputing = true;
	try
	{
		for (ProxyList::const_iterator iter = _deleteProxyList.begin(); iter != _deleteProxyList.end(); ++iter)
		{
			_proxyList.Remove(*iter);
			DoDeleteProxy(*iter);
		}
		_deleteProxyList.clear();

		for (ProxyList::iterator iter = _proxyList.begin(); iter != _proxyList.end(); ++iter)
			ComputeProxy(*iter);		

		for (Source3dList::iterator iter = _proxy3dList.begin(); iter != _proxy3dList.end(); ++iter)
		{
			if (_changed3d)
			{
				_changed3d = false;
				(*iter)->Changed3d();
			}

			(*iter)->ApplyChanges3d();
		}
	}
	LSL_FINALLY(_isComputing = false;)
}

SoundLib* Engine::CreateSoundLib()
{
	SoundLib* lib = new SoundLib(this);

	_soundLibList.push_back(lib);

	return lib;
}

void Engine::ReleaseSoundLib(SoundLib* lib)
{
	_soundLibList.Remove(lib);
	delete lib;
}

SubmixVoice* Engine::CreateSubmixVoice()
{
	SubmixVoice* voice = new SubmixVoice(this);
	_voiceList.push_back(voice);

	return voice;
}

MasteringVoice* Engine::CreateMasteringVoice()
{
	MasteringVoice* voice = new MasteringVoice(this);
	_voiceList.push_back(voice);

	return voice;
}

Source* Engine::CreateSource()
{
	Source* voice = new Source(this);
	_voiceList.push_back(voice);

	return voice;
}

Source3d* Engine::CreateSource3d()
{
	Source3d* voice = new Source3d(this);
	_voiceList.push_back(voice);

	return voice;
}

void Engine::ReleaseVoice(Voice* voice)
{
	_voiceList.Remove(voice);

	delete voice;
}

void Engine::ReleaseSoundLinks(Sound* sound)
{
	for (ProxyList::const_iterator iter = _proxyList.begin(); iter != _proxyList.end();)
		if ((*iter)->GetSound() == sound)
		{
			Proxy* proxy = *iter;

			ProxyList::iterator iterCache = _srcCache.Find(proxy);
			if (iterCache != _srcCache.end())
				RemoveCache(iterCache);

			ProxyList::iterator iterPool = _srcPool.Find(proxy);
			if (iterPool != _srcPool.end())
				RemovePool(iterPool);

			iter = DeleteProxy(iter);
		}
		else
			++iter;
}

const Listener* Engine::GetListener() const
{
	return _listener;
}

void Engine::SetListener(const Listener* value)
{
	lsl::SafeDelete(_listener);

	if (value)
	{
		_listener = new Listener(*value);
		InitX3dAudio();		
	}

	Changed3d();
}

const XAUDIO2_DEVICE_DETAILS& Engine::GetDevCaps() const
{
	return _xDevCaps;
}

MasteringVoice* Engine::GetMainVoice()
{
	return _mainVoice;
}

float Engine::GetDistScaler() const
{
	return _distScaler;
}

void Engine::SetDistScaler(float value)
{
	_distScaler = value;
	Changed3d();
}

Engine::Mode3d Engine::GetMode3d() const
{
	return _mode3d;
}

void Engine::SetMode3d(Mode3d value)
{
	_mode3d = value;
	Changed3d();
}

}

}