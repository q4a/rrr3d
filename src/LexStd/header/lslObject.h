#ifndef LSL_OBJECT
#define LSL_OBJECT

#include "lslCommon.h"
#include "lslException.h"

namespace lsl
{

class ObjReference
{
public:
	template<class _T> static bool ReplaceRef(_T* curValue, _T* newValue)
	{
		if (curValue != newValue)
		{
			if (curValue)
				curValue->Release();
			if (newValue)
				newValue->AddRef();
			return true;
		}
		return false;
	}
private:
	volatile mutable unsigned _refCnt;
public:
	ObjReference(): _refCnt(0)
	{
	}

	ObjReference(const ObjReference& ref): _refCnt(0)
	{
	}

	~ObjReference()
	{
		LSL_ASSERT(_refCnt == 0);
	}

	void AddRef() const
	{
		++_refCnt;
	}

	unsigned Release() const
	{
		LSL_ASSERT(_refCnt > 0);
		
		return --_refCnt;
	}
	
	unsigned GetRefCnt() const
	{
		return _refCnt;
	}
};

class Object: public virtual ObjReference
{
public:
	virtual ~Object()
	{
	}

	//Разрушить object в контексте данного(this) экземпляра. Полезно когда выполнение происходит на границах модулей (т.е. обладает своей областью памяти или определенным диапазоном в разделяемой памяти). Полезно например при выполненнии на границах dll и exe (dll и dll).
	virtual void Destroy(Object* object) const
	{
		delete object;
	}

	Object& operator=(const Object& ref)
	{
		return *this;
	}
};

}

#endif