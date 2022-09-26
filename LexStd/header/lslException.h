#ifndef LSL_EXCEPTION
#define LSL_EXCEPTION

#include "lslCommon.h"

namespace lsl
{

class AppLog
{
private:
	bool _destroy;

	std::ostream* CreateStream();
	void ReleaseStream(std::ostream* stream);
public:
	AppLog(const std::string& mFileName);
	~AppLog();

	template<class _Value> void Write(const _Value& value);
	template<class _Value> void Append(const _Value& value);
	void Clear();

	std::string fileName;
};

class Error: public std::exception
{
private:
	typedef std::exception _MyBase;
private:
	void PrintToLog();
public:
	Error(const char* message);
	Error(const std::string& message);
};

class EUnableToOpen: public Error
{
public:
	EUnableToOpen(const std::string& fileName): Error("Unable to open " + fileName) {}
};

void Assert(const char* expression, const char* filePath, int line);
//Применяется только для полиморфных объектов. При преобраование происходит дебагная проверка соответствия типа
//template<class _Target, class _Class> _Target StaticCast(const _Class& object);

#ifdef _DEBUG
	#define LSL_ASSERT(expression) (void)( (!!(expression)) || (lsl::Assert(#expression, __FILE__, __LINE__), 0) )
#else
	#define LSL_ASSERT(expression) ((void)0)
#endif

#define LSL_LOG(message) lsl::appLog.Append(message);

#ifdef _DEBUG_TRACE
	#define LSL_TRACE(message) lsl::appLog.Append(message);
#else
	#define LSL_TRACE(message) ((void)0)
#endif

#define LSL_FINALLY(expression) catch(...){expression throw;}expression

extern AppLog appLog;




template<class _Value> void AppLog::Write(const _Value& value)
{
	if (_destroy)
		return;

	std::ostream* stream = CreateStream();

	*stream << value;	

#ifdef _CONSOLE
	std::cout << value;
#endif

	ReleaseStream(stream);
}

template<class _Value> void AppLog::Append(const _Value& value)
{
	if (_destroy)
		return;

	std::ostream* stream = CreateStream();

	*stream << value;
	*stream << '\n';

#ifdef _CONSOLE
	std::cout << value << '\n';
#endif

	ReleaseStream(stream);
}

template<class _Value> inline AppLog& operator<<(AppLog& log, const _Value& value)
{
	log.Write(value);
	return log;
}




template<typename _To, typename _From> inline _To StaticCast(_From& x)
{
	dynamic_cast<_To>(x);

	return static_cast<_To>(x);
}

template<typename _To, typename _From> inline _To StaticCast(_From* x)
{
	if (!x)
		return 0;

	LSL_ASSERT(dynamic_cast<_To>(x));

	return static_cast<_To>(x);
}

/*template<class _Target, class _Class> _Target StaticCast<_Target&, _Class>(_Class object)
{
	//LSL_ASSERT(dynamic_cast<_Target*>(&object));

	return static_cast<_Target>(object);
}

template<class _Target, class _Class> _Target StaticCast<_Target, const _Class&>(_Class& object)
{
	//LSL_ASSERT(dynamic_cast<_Target>(object));

	return static_cast<_Target>(object);
}*/

}

#endif