#ifndef LSL_UTILITY
#define LSL_UTILITY

#include "lslCommon.h"
#include "lslObject.h"
#include "lslException.h"
#include "lslMath.h"
#include "lslVariant.h"
#include "lslContainer.h"

namespace lsl
{

typedef std::string string;
typedef std::string stringA;
typedef std::wstring stringW;
typedef char TCHAR;

typedef lsl::Vector<string> StringVec;

const TCHAR cStrLev = '\\';
const TCHAR cStrComma = ',';
const lsl::string cStrRLev = "..\\";

#define ARRAY_LENGTH(arr) _countof(arr)

//При использовании множественного виртуального наследования от абстрактных классов(интерфейсов) и при иерархичном наследовании реализаций этих самых интерфейсов может возникать ошибочный warning 4250 (warning C4250: 'Class2' : inherits 'Class1::Class1::Method1' via dominance). На самом деле какой именно(или иначе, по какому именно пути) метод наследуется не играет совершенно никакой роли, поскольку классы полностью абстрактны(т.е. главным образом не содержат релизаций этого метода) а также неследуются виртуально.
////Выключить ошибочный warning
//#pragma warning(disable : WARNING_MULTIPLE_VIRTUAL_INHERIT_C4250)
////Восстановить умолчание
//#pragma warning(default : WARNING_MULTIPLE_VIRTUAL_INHERIT_C4250)
#define WARNING_MULTIPLE_VIRTUAL_INHERIT_C4250 4250

//Базовый класс с поддержкой идентификации типа во время выполнения, а также сохранения идентификатора типа. Выступает в качестве супер класса (самого базового) для иерархии
//Может быть также использован с целью получения несвязанных друг с другом интерфейсов(по его type), которые реализуются(поддерживаются) каким нибудь из классов иерархии. По аналогии с IUnknown::QueryInterface
template<class _Type> class SuperClass
{
private:
	typedef SuperClass<_Type> _MyClass;
public:
	typedef _Type Type;
private:
	Type _type;
protected:
	virtual bool IsType(Type type, void** out) = 0;

	void SetType(Type value)
	{
		_type = value;
	}
public:
	template<class _Class> bool Is(Type type, _Class** out)
	{
		//Инициализируем указатель на случай если он не был(чтобы не получить av при разыменовывании) а также проверяем на статическую совместимость типов
		_MyClass* myClass = (*out = 0);
		//
		void* tmp;
		bool ok = IsType(type, &tmp);
		*out = reinterpret_cast<_Class*>(tmp);
		return ok;
	}
	template<class _Class> bool Is(_Class** out)
	{
		return Is(_Class::Type, out);
	}

	Type GetType() const
	{
		return _type;
	}
};

//Супер класс для ветки реализаций, включает кастование между реализациями. Интерфейс переходит именно к этому классу как к базовой реализации
class VirtImpl
{
public:
	virtual ~VirtImpl() {}

	template<class _Class> _Class* CastTo()
	{
		return static_cast<_Class*>(this);
	}
	template<class _Class> const _Class* CastTo() const
	{
		return static_cast<const _Class*>(this);
	}

	//Сравнивание реализаций. Может быть виртуально замещено
	virtual bool Equal(const VirtImpl* value) const
	{
		return (this == value);
	}
};

//Супер класс для ветки интерфейсов, включает переход к реализации
class ExternInterf: public virtual Object
{
private:
	const VirtImpl* GetConstImpl() const
	{
		return const_cast<ExternInterf*>(this)->GetImpl();
	}
protected:
	virtual VirtImpl* GetImpl() = 0;
public:
	virtual ~ExternInterf() {}

	template<class _Class> _Class* GetImpl()
	{
		return GetImpl()->CastTo<_Class>();
	}
	template<class _Class> const _Class* GetImpl() const
	{
		return GetImpl()->CastTo<_Class>();
	}

	//Сравнивание интерфейсов. На практике реалзиация этих интерфейсов определяется результат
	bool Equal(const ExternInterf* value) const
	{
		return value ? GetConstImpl()->Equal(value->GetConstImpl()) : false;
	}
};

//Ветка реализаций. Включает ссылку на внешний класс-реализацию (подход внешних интерфейсов)
template<class _Inst> class ExternImpl: public VirtImpl
{
public:
	typedef _Inst Inst;
private:
	_Inst* _inst;
protected:
	void InstAddRef(Object* ref)
	{
		ref->AddRef();
	}
	void InstRelease(Object* ref)
	{
		ref->Release();
	}
	void InstAddRef(void* ref) {}
	void InstRelease(void* ref) {}
public:
	ExternImpl(_Inst* inst): _inst(inst)
	{
		LSL_ASSERT(_inst);

		InstAddRef(_inst);
	}
	~ExternImpl()
	{
		InstRelease(_inst);
	}

	//При сравнении внешних интерфейсов нужно сравнивать их реализации поскольку может существовать несколько внешних интрфейсов для одной реализации.
	virtual bool Equal(const VirtImpl* value) const
	{
		return (_inst == value->CastTo<ExternImpl<_Inst>>()->_inst);
	}

	_Inst* GetInst()
	{
		return _inst;
	}
	const _Inst* GetInst() const
	{
		return _inst;
	}
};

//Внешний итератор специально для IExternImplace, в случае необходимости перебирания элементов (например для списка)
template<class _ExternImplace, class _Iterator> class ExternIterator
{
private:
	_Iterator _iter;
public:
	ExternIterator(const _Iterator& iter);

	_ExternImplace* operator*()
	{
		return new _ExternImplace(*_iter);
	}
	ExternIterator& operator++()
	{
		++_iter;
		return *this;
	}
};

template<class _Key, class _BaseClass> class ClassMapList
{
private:
	typedef std::map<_Key, _BaseClass*> _Map;
public:
	typedef _Key Key;
	typedef _BaseClass BaseClass;
private:
	_Map _map;
public:
	~ClassMapList();

	template<class _Class> void Add(const _Key& key);
	template<class _Class> void Add(const _Key& key, const _Class& ref);
	void Delete(const _Key& key);
	void Clear();

	_BaseClass& GetInstance(const _Key& key) const;	
};

template<class _Class, class _ClassMapList> class RegisterMapClass
{
public:
	RegisterMapClass(_ClassMapList& mapList, typename _ClassMapList::Key key);
	void Test();
};

enum KeyState {ksDown = 0, ksUp, cKeyStateEnd};
enum MouseKey {mkLeft = 0, mkRight, mkMiddle, cMouseKeyEnd};

struct Point
{
	int x;
	int y;

	Point() {}
	Point(int mX, int mY): x(mX), y(mY) {};


	Point& operator+=(const Point& value)
	{
		x += value.x;
		y += value.y;

		return *this;
	}
	Point& operator-=(const Point& value)
	{
		x -= value.x;
		y -= value.y;

		return *this;
	}	

	operator int*()
	{
		return &x;
	}
	operator const int*() const
	{
		return &x;
	}
};

struct Rect
{
	Rect() {}
	Rect(int mLeft, int mTop, int mRight, int mBottom): left(mLeft), top(mTop), right(mRight), bottom(mBottom) {}

	int left;
	int top;
	int right;
	int bottom;
};

template<unsigned _Bits> class Bitset: public std::bitset<_Bits>
{
private:
	typedef std::bitset<_Bits> _MyBase;
public:
	Bitset() {}
	
	explicit Bitset(unsigned long _Val): _MyBase(_Val) {}
	explicit Bitset(const std::string& value): _MyBase(value) {}
};

typedef std::vector<std::string> StringList;




template<class _Pnt> inline void SafeRelease(_Pnt& pnt)
{
	if (pnt)
	{
		pnt->Release();
		pnt = 0;
	}
}

template<class _Pnt> inline void SafeFree(_Pnt& pnt)
{
	if (pnt)
	{
		std::free(pnt);
		pnt = 0;
	}
}

template<class _Pnt> inline void SafeDelete(_Pnt& pnt)
{
	if (pnt)
	{
		delete pnt;
		pnt = 0;
	}
}

template<class _Value> inline _Value ClampValue(const _Value& value, const _Value& low, const _Value& high)
{
	return value > low ? (value < high ? value : high) : low;
}

template<class _T> inline void ExtractFilePathBase(std::basic_string<_T>& filePath, const std::basic_string<_T>& fileName, _T del)
{
	std::basic_string<_T>::size_type posName = fileName.rfind(del) + 1;
	filePath = fileName;
	filePath.erase(posName, fileName.size() - posName);
}

inline void ExtractFilePath(std::string& filePath, const std::string& fileName)
{
	ExtractFilePathBase(filePath, fileName, '\\');
}

inline void ExtractFilePath(std::wstring& filePath, const std::wstring& fileName)
{
	ExtractFilePathBase(filePath, fileName, L'\\');
}

inline void ExtractFileExt(std::string& fileExt, const std::string& fileName)
{
	fileExt = fileName.substr(fileName.rfind('.'), fileName.size());
}

template<class _Iter, class _Item> inline _Item FindItemByName(_Iter begin, _Iter end, const std::string& name, _Item item)
{
	for (_Iter iter = begin; iter != end; ++iter)
		if ((*iter)->GetName() == name && (*iter) != item)
			return (*iter);
	return 0;	
}

inline int ConvStrToEnum(const char* str, const char* strList[], unsigned strListSize)
{
	for (unsigned i = 0; i < strListSize; ++i)
		if (strcmp(strList[i], str) == 0)		
			return i;

	return -1;
}

inline int ConvStrToEnum(const lsl::string& str, const lsl::string strList[], unsigned strListSize)
{
	for (unsigned i = 0; i < strListSize; ++i)
		if (str == strList[i])
			return i;

	return -1;
}

inline int ConvStrToEnum(const std::string& str, const StringList& strList)
{
	for (unsigned i = 0; i < strList.size(); ++i)
		if (str == strList[i])
			return i;

	return -1;
}




template<class _Key, class _BaseClass> ClassMapList<_Key, _BaseClass>::~ClassMapList()
{
	Clear();
}

template<class _Key, class _BaseClass> template<class _Class> void ClassMapList<_Key, _BaseClass>::Add(const _Key& key)
{
	if (_map.find(key) == _map.end())
		_map.insert(_map.end(), std::make_pair(key, new _Class()));
	else
	{
		LSL_ASSERT(false);
	}
}

template<class _Key, class _BaseClass> template<class _Class> void ClassMapList<_Key, _BaseClass>::Add(const _Key& key, const _Class& ref)
{
	if (_map.find(key) == _map.end())
		_map.insert(_map.end(), std::make_pair(key, new _Class(ref)));
	else
	{
		LSL_ASSERT(false);
	}
}

template<class _Key, class _BaseClass> void ClassMapList<_Key, _BaseClass>::Delete(const _Key& key)
{
	_Map::iterator ter = _map.find(key);
	if (ter == _map.end())
	{
		throw lsl::Error("template<class _Key, class _BaseClass> void ClassMapList<_Key, _BaseClass>::Delete(const _Key& key)");
	}
	delete iter->second;
	_map.erase(iter);
}

template<class _Key, class _BaseClass> void ClassMapList<_Key, _BaseClass>::Clear()
{
	for (_Map::iterator iter = _map.begin(); iter != _map.end(); ++iter)
		delete iter->second;	
	_map.clear();
}

template<class _Key, class _BaseClass> _BaseClass& ClassMapList<_Key, _BaseClass>::GetInstance(const _Key& key) const
{
	_Map::const_iterator iter = _map.find(key);
	if ( iter != _map.end())
		return *iter->second;
	else
		throw lsl::Error("template<class _Key, class _BaseClass> _BaseClass& ClassMapList<_Key, _BaseClass>::GetInstance(const _Key& key) const");
}




template<class _Class, class _ClassMapList> RegisterMapClass<_Class, _ClassMapList>::RegisterMapClass(_ClassMapList& mapList, typename _ClassMapList::Key key)
{
	mapList.Add<_Class>(key);
}

template<class _Class, class _ClassMapList> void RegisterMapClass<_Class, _ClassMapList>::Test()
{
}




static Point& operator+(const Point& value1, const Point& value2)
{
	Point res(value1);

	return res += value2;
}

static Point& operator-(const Point& value1, const Point& value2)
{
	Point res(value1);

	return res -= value2;
}




inline double GetTimeDbl()
{
	__int64 gTime, freq;
	QueryPerformanceCounter((LARGE_INTEGER*)&gTime);  // Get current count
	QueryPerformanceFrequency((LARGE_INTEGER*)&freq); // Get processor freq
	
	return gTime/static_cast<double>(freq);
}




#pragma warning(disable:4996)

inline string StrFmt(const TCHAR* value, ...)
{
	va_list arglist;
	va_start(arglist, value);

	const int cBufSize = 256;
	TCHAR cBuf[cBufSize];
	int bufSize = 1;
	TCHAR* buf = 0;

	int res = -1;
	while (res == -1)
	{
		if (buf == 0)
			buf = cBuf;
		else
		{
			if (bufSize > 1)
				delete[] buf;
			buf = new TCHAR[(++bufSize) * cBufSize];
		}
	
		res = _vsnprintf(buf, bufSize * cBufSize, value, arglist);
	}

	string ret(buf, res);
	if (bufSize > 1)
		delete[] buf;

	return ret;
}

inline stringW StrFmtW(const wchar_t* value, ...)
{
	va_list arglist;
	va_start(arglist, value);

	const int cBufSize = 256;
	wchar_t cBuf[cBufSize];
	int bufSize = 1;
	wchar_t* buf = 0;

	int res = -1;
	while (res == -1)
	{
		if (buf == 0)
			buf = cBuf;
		else
		{
			if (bufSize > 1)
				delete[] buf;
			buf = new wchar_t[(++bufSize) * cBufSize];
		}

		res = _vsnwprintf(buf, bufSize * cBufSize, value, arglist);
	}

	stringW ret(buf, res);
	if (bufSize > 1)
		delete[] buf;

	return ret;
}

#pragma warning(default:4996)

inline void StrDelSpacesBi(string& str)
{
	while (str.size() > 0 && isspace(str[str.size() - 1]))
		str.erase(str.size() - 1, 1);

	while (str.size() > 0 && isspace(str[0]))
		str.erase(0, 1);
}

inline void StrExtractValues(const string& str, StringVec& vec, const lsl::string& del)
{
	int pos = 0;
	int ePos = str.find(del);
	if (ePos < 0)
		ePos = str.size();

	while (ePos > pos)
	{
		lsl::string val = str.substr(pos, ePos - pos);
		lsl::StrDelSpacesBi(val);
		if (!val.empty())
			vec.push_back(val);

		pos = ePos + del.size();
		ePos = str.find(del, pos);
		if (ePos < 0)
			ePos = str.size();
	}
}

inline void StrExtractValues(const string& str, StringVec& vec, const TCHAR del = cStrComma)
{	
	StrExtractValues(str, vec, lsl::string(1, del));
}

inline void StrLinkValues(const StringVec& vec, string& str, const TCHAR del = cStrComma)
{
	if (!vec.empty())
	{
		for (StringVec::const_iterator iter = vec.begin(); iter != vec.end(); ++iter)
		{
			str += *iter;
			if (iter != --vec.end())
				str += del;
		}		
	}
}

inline string ExtractFileDir(const string& str, const TCHAR del = cStrLev)
{
	lsl::string res(str);
	lsl::string::size_type pos = res.rfind(del);
	if (pos != string::npos)
		res.erase(pos + 1);

	return res;
}

inline lsl::stringW ConvertStrAToW(const char* str, unsigned length, UINT codePage = CP_ACP)
{
	int num = MultiByteToWideChar(codePage, MB_PRECOMPOSED, str, length, 0, 0);

	lsl::stringW::value_type* resStr = new lsl::stringW::value_type[num];
	MultiByteToWideChar(codePage, MB_PRECOMPOSED, str, length, resStr, num);
	lsl::stringW outStr(resStr, num);
	delete[] resStr;

	return outStr;
}

inline lsl::stringW ConvertStrAToW(const lsl::stringA& str, UINT codePage = CP_ACP)
{
	return ConvertStrAToW(str.c_str(), str.length(), codePage);
}

inline lsl::stringA ConvertStrWToA(const wchar_t* str, unsigned length, UINT codePage = CP_ACP)
{
	int num = WideCharToMultiByte(codePage, WC_NO_BEST_FIT_CHARS, str, length, 0, 0, 0, 0);

	lsl::stringA::value_type* resStr = new lsl::stringA::value_type[num];
	WideCharToMultiByte(codePage, WC_NO_BEST_FIT_CHARS, str, length, resStr, num, 0, 0);
	lsl::stringA outStr(resStr, num);
	delete[] resStr;

	return outStr;
}

inline lsl::stringA ConvertStrWToA(const lsl::stringW& str, UINT codePage = CP_ACP)
{
	return ConvertStrWToA(str.c_str(), str.length(), codePage);
}

}

#endif