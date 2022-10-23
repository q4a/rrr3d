#include "stdafx.h"

#include "lslSDK.h"

namespace lsl
{

class Win32ThreadPool: public ThreadPool
{
public:
	//–азмещение работ в пуле. ѕроисходит в пор€дке очереди: первый вошел первым вышел
	void QueueWork(UserWork* value, Object* arg, Flags flags = Flags(0));

	unsigned GetMinThreads();
	void SetMinThreads(unsigned value);

	unsigned GetMaxThreads();
	void SetMaxThreads(unsigned value);
};

class Win32ThreadEvent: public ThreadEvent
{
	friend class Win32SDK;
private:
	HANDLE _event;
protected:
	Win32ThreadEvent(bool manualReset, bool open, const std::string& name);
	virtual ~Win32ThreadEvent();
public:
	bool WaitOne(unsigned mlsTimeOut = INFINITE);
	void Set();
	void Reset();
};

class Win32SDK: public SDK
{
private:
	Win32ThreadPool* _threadPool;
public:
	Win32SDK();
	virtual ~Win32SDK();

	//
	ThreadPool* GetThreadPool();
	//
	LockedObj* CreateLockedObj();
	void DestroyLockedObj(LockedObj* value);
	void Lock(LockedObj* obj);
	void Unlock(LockedObj* obj);

	ThreadEvent* CreateThreadEvent(bool manualReset, bool open, const std::string& name);
	void DestroyThreadEvent(ThreadEvent* value);

	float GetTime();
	double GetTimeDbl();
};

namespace
{

struct ThreadParameter
{
	Win32ThreadPool* pool;
	ThreadPool::UserWork* work;
	Object* arg;
};

SDK* instance = 0;

DWORD __stdcall ThreadPoolStart(void* lpThreadParameter)
{
	LSL_ASSERT(lpThreadParameter);

	ThreadParameter* param = reinterpret_cast<ThreadParameter*>(lpThreadParameter);
	Win32ThreadPool* pool = param->pool;
	ThreadPool::UserWork* work = param->work;
	Object* arg = param->arg;
	delete param;

	work->BeginExecution();
	try
	{
		work->Execute(arg);
		//»сточник опасности, поскольку внтуренние уведомлени€ о завершении работы уже получены и задачу могут попытатьс€ уничтожить
		work->Release();
	}
	LSL_FINALLY(work->EndExecution();)

	return 0;
}

class FreeStaticData
{
public:
	~FreeStaticData()
	{
		lsl::SafeDelete(instance);
	}
};

FreeStaticData freeStaticData;

}

Profiler* Profiler::_i;




void Win32ThreadPool::QueueWork(UserWork* value, Object* arg, Flags flags)
{
	DWORD dwFlags = 0;
	
	//tfBackground эмулируетс€ с помощью tfLongFunc
#ifdef _WIN32 // FIX_LINUX QueueUserWorkItem
	if (flags.test(tfLongFunc) || flags.test(tfBackground))
		dwFlags |= WT_EXECUTELONGFUNCTION;
#endif

	value->AddRef();
	ThreadParameter* param = new ThreadParameter;
	param->pool = this;
	param->work = value;
	param->arg = arg;
#ifdef _WIN32 // FIX_LINUX QueueUserWorkItem
	QueueUserWorkItem(&ThreadPoolStart, param, dwFlags);
#endif
	//::CreateThread(0, 0, &ThreadPoolStart, value, 0, 0);
}

unsigned Win32ThreadPool::GetMinThreads()
{
	return 0;
}

void Win32ThreadPool::SetMinThreads(unsigned value)
{
}

unsigned Win32ThreadPool::GetMaxThreads()
{
	return 0;
}

void Win32ThreadPool::SetMaxThreads(unsigned value)
{
}




void* SDK::GetDataFrom(LockedObj* obj)
{
	return obj->_data;
}

void SDK::SetDataTo(LockedObj* obj, void* data)
{
	obj->_data = data;
}




Win32ThreadEvent::Win32ThreadEvent(bool manualReset, bool open, const std::string& name)
{
#ifdef _WIN32 // FIX_LINUX ThreadEvents
	_event = CreateEvent(0, manualReset, open, name.empty() ? 0 : name.c_str());
#endif
}

Win32ThreadEvent::~Win32ThreadEvent()
{
#ifdef _WIN32 // FIX_LINUX ThreadEvents
	CloseHandle(_event);
#endif
}

bool Win32ThreadEvent::WaitOne(unsigned mlsTimeOut)
{
#ifdef _WIN32 // FIX_LINUX ThreadEvents
	return WaitForSingleObject(_event, mlsTimeOut) > 0;
#else
	return false;
#endif
}

void Win32ThreadEvent::Set()
{
#ifdef _WIN32 // FIX_LINUX ThreadEvents
	SetEvent(_event);
#endif
}

void Win32ThreadEvent::Reset()
{
#ifdef _WIN32 // FIX_LINUX ThreadEvents
	ResetEvent(_event);
#endif
}




Win32SDK::Win32SDK(): _threadPool(0)
{
}

Win32SDK::~Win32SDK()
{
	lsl::SafeDelete(_threadPool);
}

ThreadPool* Win32SDK::GetThreadPool()
{
	if (!_threadPool)
		_threadPool = new Win32ThreadPool();

	return _threadPool;
}

LockedObj* Win32SDK::CreateLockedObj()
{
	LockedObj* obj = new LockedObj();
	obj->AddRef();

#ifdef _WIN32 // FIX_LINUX RTL_CRITICAL_SECTION
	RTL_CRITICAL_SECTION* section = new RTL_CRITICAL_SECTION;
	InitializeCriticalSection(section);

	SetDataTo(obj, section);
#endif

	return obj;
}

void Win32SDK::DestroyLockedObj(LockedObj* value)
{
#ifdef _WIN32 // FIX_LINUX RTL_CRITICAL_SECTION
	RTL_CRITICAL_SECTION* section = reinterpret_cast<RTL_CRITICAL_SECTION*>(GetDataFrom(value));

	DeleteCriticalSection(section);
	delete section;
#endif

	value->Release();
	delete value;
}

void Win32SDK::Lock(LockedObj* obj)
{
#ifdef _WIN32 // FIX_LINUX RTL_CRITICAL_SECTION
	EnterCriticalSection(reinterpret_cast<RTL_CRITICAL_SECTION*>(GetDataFrom(obj)));
#endif
}

void Win32SDK::Unlock(LockedObj* obj)
{
#ifdef _WIN32 // FIX_LINUX RTL_CRITICAL_SECTION
	LeaveCriticalSection(reinterpret_cast<RTL_CRITICAL_SECTION*>(GetDataFrom(obj)));
#endif
}

ThreadEvent* Win32SDK::CreateThreadEvent(bool manualReset, bool open, const std::string& name)
{
	return new Win32ThreadEvent(manualReset, open, name);
}

void Win32SDK::DestroyThreadEvent(ThreadEvent* value)
{
	delete static_cast<Win32ThreadEvent*>(value);
}

float Win32SDK::GetTime()
{
	return static_cast<float>(GetTimeDbl());
}

double Win32SDK::GetTimeDbl()
{
#ifdef _WIN32 // FIX_LINUX QueryPerformanceFrequency
	__int64 gTime, freq;
	QueryPerformanceCounter((LARGE_INTEGER*)&gTime);  // Get current count
	QueryPerformanceFrequency((LARGE_INTEGER*)&freq); // Get processor freq
	
	return gTime/static_cast<double>(freq);
#else
	return 0;
#endif
}




Profiler::Profiler()
{
#ifdef _WIN32 // FIX_LINUX QueryPerformanceFrequency
	QueryPerformanceFrequency((LARGE_INTEGER*)&_cpuFreq);
#endif
}

void Profiler::ResetSample(SampleData& data)
{
	data.frames = 0;
	data.time = 0;
	data.dt = 0.0f;
	data.summDt = 0;
	data.maxDt = 0.0f;
	data.minDt = FLT_MAX;
	data.updated = false;	
}

void Profiler::Begin(const lsl::string& name)
{	
	Samples::iterator iter = _samples.find(name);
	if (iter == _samples.end())
	{
		iter = _samples.insert(iter, Samples::value_type(name, SampleData()));
		ResetSample(iter->second);
	}

#ifdef _WIN32 // FIX_LINUX QueryPerformanceCounter
	QueryPerformanceCounter((LARGE_INTEGER*)&iter->second.time);
#endif

	_stack.push(name);
}

void Profiler::End()
{
	if (_stack.empty())
		return;

	lsl::string name = _stack.top();
	_stack.pop();

	Samples::iterator iter = _samples.find(name);

	uint64_t time = iter->second.time;
#ifdef _WIN32 // FIX_LINUX QueryPerformanceCounter
	QueryPerformanceCounter((LARGE_INTEGER*)&iter->second.time);
#endif
	float dt = 1000 * (iter->second.time - time) / static_cast<float>(_cpuFreq);

	iter->second.updated = true;
	iter->second.dt = dt;
	iter->second.summDt += dt;	
	++iter->second.frames;

	if (iter->second.maxDt < dt)
		iter->second.maxDt = dt;

	if (iter->second.minDt > dt)
		iter->second.minDt = dt;
}

void Profiler::ResetSample(const lsl::string& name)
{
	Samples::iterator iter = _samples.find(name);
	if (iter == _samples.end())
		return;

	ResetSample(iter->second);
}

const Profiler::Samples& Profiler::samples() const
{
	return _samples;
}

void Profiler::Init(Profiler* profiler)
{
	_i = profiler;
}

Profiler& Profiler::I()
{
	LSL_ASSERT(_i);

	return *_i;
}




SDK* GetSDK()
{
	if (instance == 0)
		instance = new Win32SDK();

	return instance;
}

void ReleaseSDK()
{
	lsl::SafeDelete(instance);
}

}