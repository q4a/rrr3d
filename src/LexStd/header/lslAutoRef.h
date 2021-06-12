#ifndef LSL_AUTOREF
#define LSL_AUTOREF

#include "lslCommon.h"
#include "lslObject.h"

namespace lsl
{

//Debug отслеживание потерянных ссылок(утечки памяти). Поддержка преобразования ссылок
class BaseAutoRef
{
private:
	typedef BaseAutoRef _MyClass;
	typedef std::list<_MyClass*> _RefList;
private:
	const _MyClass* _prevRef;
	mutable _RefList _nextRefList;
protected:
	BaseAutoRef(): _prevRef(0) {}

	void InitRef(const _MyClass* value, Object* owner)
	{
		if (owner && value)
		{
			_prevRef = value;
			value->_nextRefList.push_back(this);
		}
	}
	bool FreeRef(Object* owner)
	{
		if (owner)
		{
			//Нужно удалять ссылку
			if (!_prevRef && _nextRefList.empty())
				return true;

			for (_RefList::iterator iter = _nextRefList.begin(); iter != _nextRefList.end(); ++iter)
			{
				(*iter)->_prevRef = 0;
				if (_prevRef)	
				{
					(*iter)->_prevRef = _prevRef;	
				}
				else if (iter != _nextRefList.begin())	
				{
					(*iter)->_prevRef = *_nextRefList.begin();
				}

				if ((*iter)->_prevRef)									
					(*iter)->_prevRef->_nextRefList.push_back(*iter);
			}
			_nextRefList.clear();

			if (_prevRef)
				_prevRef->_nextRefList.remove(this);		
		}

		return false;
	}
};

//Авто ссылка, включает автоматические AddRef/Release при создании/уничтожении. Также может осуществляться авто-удаление объекта при обнулении его счетчика
//В качестве owner-a может задаваться сам арuумент ссылка (Ref), тогда будет происходить самоудаление объекта
template<class _Ref> class AutoRef: public BaseAutoRef
{
	template<class _Ref> friend class AutoRef;
private:
	typedef AutoRef<_Ref> _MyClass;	
	typedef BaseAutoRef _MyBase;
	typedef std::list<_MyClass*> _RefList;
private:
	_Ref* _ref;
	Object* _owner;

	void InitRef(const BaseAutoRef* value)
	{
		if (_ref)
			_ref->AddRef();

		_MyBase::InitRef(value, _owner);
	}
	void FreeRef()
	{
		bool isFree = _MyBase::FreeRef(_owner);
		
		//Проверка на гарантированное удаление ссылки
		LSL_ASSERT(!(isFree && _ref && _ref->GetRefCnt() > 1));

		if (_ref && _ref->Release() == 0 && _owner)
			_owner->Destroy(_ref);
		_ref = 0;
	}
public:
	AutoRef(): _ref(0), _owner(0) {}

	AutoRef(_Ref* ref, lsl::Object* owner = 0): _ref(ref), _owner(owner)
	{
		if (_owner)
			_owner->AddRef();

		InitRef(0);
	}

	AutoRef(const _MyClass& value): _ref(value._ref), _owner(value._owner)
	{
		if (_owner)
			_owner->AddRef();

		InitRef(&value);
	}

	~AutoRef()
	{
		Release();
	}

	//Преобразование ссылки
	template<class _AutoRef> void Assign(const _AutoRef& value)
	{
		//Защита от присваивания самому себе
		if (this == static_cast<const BaseAutoRef*>(&value))
			return;

		Release();

		_ref = value._ref;
		_owner = value._owner;
		if (_owner)
			_owner->AddRef();

		InitRef(&value);
	}
	void Release()
	{
		//Сначала освобождаем владельца(который может быть одновременно и ссылкой)
		if (_owner)
			_owner->Release();
		//Затем освобождаем ссылку
		FreeRef();
		//И только в конце обнуляем владельца
		_owner = 0;
	}
	void Reset(_Ref* ref, lsl::Object* owner = 0)
	{
		Assign(_MyClass(ref, owner));
	}

	_Ref& Ref() const
	{
		return _ref;
	}
	_Ref* Pnt() const
	{
		return _ref;
	}
	_Ref* operator->() const
	{
		return _ref;
	}

	_MyClass& operator=(const _MyClass& value)
	{
		Assign(value);

		return *this;
	}

	operator bool() const
	{
		return _ref ? true : false;
	}

	bool operator==(const _MyClass& value) const
	{
		return _ref == value._ref;
	}
	bool operator==(const _Ref* value) const
	{
		return _ref == value;
	}

	bool operator!=(const _MyClass& value) const
	{
		return _ref != value._ref;
	}
	bool operator!=(const _Ref* value) const
	{
		return _ref != value;
	}	
};

}

#endif