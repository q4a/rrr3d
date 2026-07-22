#include "stdafx.h"

#include "lslSDK.h"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace lsl
{

class PlatformThreadPool: public ThreadPool
{
private:
	unsigned _minThreads;
	unsigned _maxThreads;
public:
	PlatformThreadPool();

	void QueueWork(UserWork* value, Object* arg, Flags flags = Flags(0));

	unsigned GetMinThreads();
	void SetMinThreads(unsigned value);

	unsigned GetMaxThreads();
	void SetMaxThreads(unsigned value);
};

#ifndef _WIN32
struct PortableEventState
{
	PortableEventState(bool manual, bool initial): manualReset(manual), signaled(initial) {}

	std::mutex mutex;
	std::condition_variable condition;
	bool manualReset;
	bool signaled;
};
#endif

class PlatformThreadEvent: public ThreadEvent
{
	friend class PlatformSDK;
private:
#ifdef _WIN32
	HANDLE _event;
#else
	PortableEventState _event;
#endif
protected:
	PlatformThreadEvent(bool manualReset, bool open, const std::string& name);
	virtual ~PlatformThreadEvent();
public:
	bool WaitOne(unsigned mlsTimeOut = INFINITE);
	void Set();
	void Reset();
};

class PlatformSDK: public SDK
{
private:
	PlatformThreadPool* _threadPool;
public:
	PlatformSDK();
	virtual ~PlatformSDK();

	ThreadPool* GetThreadPool();
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
	ThreadPool::UserWork* work;
	Object* arg;
};

SDK* instance = 0;

void ExecuteThreadWork(ThreadParameter* param)
{
	LSL_ASSERT(param);

	ThreadPool::UserWork* work = param->work;
	Object* arg = param->arg;
	delete param;

	work->BeginExecution();
	try
	{
		work->Execute(arg);
		work->Release();
	}
	LSL_FINALLY(work->EndExecution();)
}

#ifdef _WIN32
DWORD __stdcall ThreadPoolStart(void* threadParameter)
{
	ExecuteThreadWork(reinterpret_cast<ThreadParameter*>(threadParameter));
	return 0;
}
#endif

class FreeStaticData
{
public:
	~FreeStaticData()
	{
		lsl::SafeDelete(instance);
	}
};

FreeStaticData freeStaticData;

} // namespace

Profiler* Profiler::_i;

PlatformThreadPool::PlatformThreadPool():
	_minThreads(1),
	_maxThreads(std::max(1u, std::thread::hardware_concurrency()))
{
}

void PlatformThreadPool::QueueWork(UserWork* value, Object* arg, Flags flags)
{
	value->AddRef();
	ThreadParameter* param = new ThreadParameter;
	param->work = value;
	param->arg = arg;

#ifdef _WIN32
	DWORD win32Flags = 0;
	if (flags.test(tfLongFunc) || flags.test(tfBackground))
		win32Flags |= WT_EXECUTELONGFUNCTION;
	if (!QueueUserWorkItem(&ThreadPoolStart, param, win32Flags))
	{
		value->Release();
		delete param;
		throw lsl::Error("QueueUserWorkItem failed");
	}
#else
	(void)flags;
	try
	{
		std::thread([param]() { ExecuteThreadWork(param); }).detach();
	}
	catch (...)
	{
		value->Release();
		delete param;
		throw;
	}
#endif
}

unsigned PlatformThreadPool::GetMinThreads()
{
	return _minThreads;
}

void PlatformThreadPool::SetMinThreads(unsigned value)
{
	_minThreads = std::max(1u, value);
	_maxThreads = std::max(_maxThreads, _minThreads);
}

unsigned PlatformThreadPool::GetMaxThreads()
{
	return _maxThreads;
}

void PlatformThreadPool::SetMaxThreads(unsigned value)
{
	_maxThreads = std::max(_minThreads, value);
}

void* SDK::GetDataFrom(LockedObj* obj)
{
	return obj->_data;
}

void SDK::SetDataTo(LockedObj* obj, void* data)
{
	obj->_data = data;
}

PlatformThreadEvent::PlatformThreadEvent(bool manualReset, bool open, const std::string& name)
#ifndef _WIN32
	: _event(manualReset, open)
#endif
{
#ifdef _WIN32
	_event = CreateEvent(0, manualReset, open, name.empty() ? 0 : name.c_str());
	if (!_event)
		throw lsl::Error("CreateEvent failed");
#else
	(void)name;
#endif
}

PlatformThreadEvent::~PlatformThreadEvent()
{
#ifdef _WIN32
	CloseHandle(_event);
#endif
}

bool PlatformThreadEvent::WaitOne(unsigned mlsTimeOut)
{
#ifdef _WIN32
	return WaitForSingleObject(_event, mlsTimeOut) == WAIT_OBJECT_0;
#else
	std::unique_lock<std::mutex> lock(_event.mutex);
	bool signaled = false;
	if (mlsTimeOut == INFINITE)
	{
		_event.condition.wait(lock, [this]() { return _event.signaled; });
		signaled = true;
	}
	else
	{
		signaled = _event.condition.wait_for(
			lock, std::chrono::milliseconds(mlsTimeOut),
			[this]() { return _event.signaled; });
	}

	if (signaled && !_event.manualReset)
		_event.signaled = false;
	return signaled;
#endif
}

void PlatformThreadEvent::Set()
{
#ifdef _WIN32
	SetEvent(_event);
#else
	{
		std::lock_guard<std::mutex> lock(_event.mutex);
		_event.signaled = true;
	}
	if (_event.manualReset)
		_event.condition.notify_all();
	else
		_event.condition.notify_one();
#endif
}

void PlatformThreadEvent::Reset()
{
#ifdef _WIN32
	ResetEvent(_event);
#else
	std::lock_guard<std::mutex> lock(_event.mutex);
	_event.signaled = false;
#endif
}

PlatformSDK::PlatformSDK(): _threadPool(0)
{
}

PlatformSDK::~PlatformSDK()
{
	lsl::SafeDelete(_threadPool);
}

ThreadPool* PlatformSDK::GetThreadPool()
{
	if (!_threadPool)
		_threadPool = new PlatformThreadPool();
	return _threadPool;
}

LockedObj* PlatformSDK::CreateLockedObj()
{
	LockedObj* obj = new LockedObj();
	obj->AddRef();

#ifdef _WIN32
	RTL_CRITICAL_SECTION* section = new RTL_CRITICAL_SECTION;
	InitializeCriticalSection(section);
	SetDataTo(obj, section);
#else
	SetDataTo(obj, new std::recursive_mutex());
#endif

	return obj;
}

void PlatformSDK::DestroyLockedObj(LockedObj* value)
{
#ifdef _WIN32
	RTL_CRITICAL_SECTION* section = reinterpret_cast<RTL_CRITICAL_SECTION*>(GetDataFrom(value));
	DeleteCriticalSection(section);
	delete section;
#else
	delete reinterpret_cast<std::recursive_mutex*>(GetDataFrom(value));
#endif

	value->Release();
	delete value;
}

void PlatformSDK::Lock(LockedObj* obj)
{
#ifdef _WIN32
	EnterCriticalSection(reinterpret_cast<RTL_CRITICAL_SECTION*>(GetDataFrom(obj)));
#else
	reinterpret_cast<std::recursive_mutex*>(GetDataFrom(obj))->lock();
#endif
}

void PlatformSDK::Unlock(LockedObj* obj)
{
#ifdef _WIN32
	LeaveCriticalSection(reinterpret_cast<RTL_CRITICAL_SECTION*>(GetDataFrom(obj)));
#else
	reinterpret_cast<std::recursive_mutex*>(GetDataFrom(obj))->unlock();
#endif
}

ThreadEvent* PlatformSDK::CreateThreadEvent(bool manualReset, bool open, const std::string& name)
{
	return new PlatformThreadEvent(manualReset, open, name);
}

void PlatformSDK::DestroyThreadEvent(ThreadEvent* value)
{
	delete static_cast<PlatformThreadEvent*>(value);
}

float PlatformSDK::GetTime()
{
	return static_cast<float>(GetTimeDbl());
}

double PlatformSDK::GetTimeDbl()
{
	return rrr3d::platform::steady_seconds();
}

Profiler::Profiler(): _cpuFreq(1000000000ull)
{
}

void Profiler::ResetSample(SampleData& data)
{
	data.frames = 0;
	data.time = 0;
	data.dt = 0.0f;
	data.summDt = 0;
	data.maxDt = 0.0f;
	data.minDt = std::numeric_limits<float>::max();
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

	iter->second.time = rrr3d::platform::steady_nanoseconds();
	_stack.push(name);
}

void Profiler::End()
{
	if (_stack.empty())
		return;

	lsl::string name = _stack.top();
	_stack.pop();

	Samples::iterator iter = _samples.find(name);
	const uint64_t startTime = iter->second.time;
	iter->second.time = rrr3d::platform::steady_nanoseconds();
	const float dt = 1000.0f * (iter->second.time - startTime) /
		static_cast<float>(_cpuFreq);

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
	if (iter != _samples.end())
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
		instance = new PlatformSDK();
	return instance;
}

void ReleaseSDK()
{
	lsl::SafeDelete(instance);
}

} // namespace lsl
