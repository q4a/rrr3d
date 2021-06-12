#ifndef LSL_CLASS_LIST
#define LSL_CLASS_LIST

#include "lslCommon.h"
#include "lslObject.h"

#ifdef DEBUG_MEMORY
	//disable mem leak log
	#pragma push_macro("new")
	#undef new
#endif

namespace lsl
{

//Элементом списка классов может быть только класс имеющий конструктор по умолчанию, если требуется другое поведение то небходимо переопределеить ClassInst::CreateInst и в списке классов ClassList::Add
template<class _Key, class _BaseClass, class _Arg> class BaseClassInst
{
private:
	_Key _key;
public:
	BaseClassInst(const _Key& key): _key(key) {}
	virtual ~BaseClassInst() {}

	virtual _BaseClass* CreateInst(const _Arg& arg) = 0;
	virtual const type_info& GetTypeInfo() const = 0;

	const _Key& GetKey() const
	{
		return _key;
	}
};

template<class _Key, class _BaseClass> class BaseClassInst<_Key, _BaseClass, void>
{
private:
	_Key _key;
public:
	BaseClassInst(const _Key& key): _key(key) {}

	virtual _BaseClass* CreateInst() = 0;
	virtual const type_info& GetTypeInfo() const = 0;

	const _Key& GetKey() const
	{
		return _key;
	}
};

template<class _Key, class _Class, class _BaseClass, class _Arg> class ClassInst: public BaseClassInst<_Key, _BaseClass, _Arg>
{
public:
	ClassInst(const _Key& key): BaseClassInst<_Key, _BaseClass, _Arg>(key) {}

	virtual _BaseClass* CreateInst(const _Arg& arg)
	{
		return new _Class(arg);
	}
	virtual const type_info& GetTypeInfo() const
	{
		return typeid(_Class);
	}
};

template<class _Key, class _Class, class _BaseClass> class ClassInst<_Key, _Class, _BaseClass, void>: public BaseClassInst<_Key, _BaseClass, void>
{
public:
	ClassInst(const _Key& key): BaseClassInst<_Key, _BaseClass, void>(key) {}

	virtual _BaseClass* CreateInst()
	{
		return new _Class();
	}
	virtual const type_info& GetTypeInfo() const
	{
		return typeid(_Class);
	}
};

template<class _Key, class _BaseClass, class _Arg> class BaseClassList: public Object
{
private:
	typedef std::map<_Key, BaseClassInst<_Key, _BaseClass, _Arg>*> _ClassList;
public:
	typedef BaseClassInst<_Key, _BaseClass, _Arg> MyClassInst;
	
	typedef _Key Key;
	typedef _BaseClass BaseClass;
	typedef _Arg Arg;
private:
	_ClassList _classList;
public:
	~BaseClassList();

	template<class _Class> void Add(const _Key& key);
	//добавление производного класса _Type с идентификатором _Class::Type
	template<class _Class> void Add();
	//
	void Del(const _Key& key);
	void Clear();

	MyClassInst* Find(const _Key& key) const;
	template<class _Class> MyClassInst* FindByClass() const;

	MyClassInst& Get(const _Key& key) const;
	template<class _Class> MyClassInst& GetByClass() const;

	//Только для полиморфных
	//Преобразование с выбросом исключения
	template<class _Class1, class _Class2> _Class1& As(_Class2& inst) const;
	//Допустимо ли преобразование из _Class2 в _Class1
	template<class _Class1, class _Class2> _Class1* Is(_Class2* inst) const;
	//Допустимо ли преобразование из _Class в _Base
	//template<class _Base> bool Is(const _Key& key) const;
};

template<class _Key, class _BaseClass, class _Arg> class ArgClassList: public BaseClassList<_Key, _BaseClass, _Arg>
{
public:
	_BaseClass* CreateInst(const _Key& key, const _Arg& arg)
	{
		return Get(key).CreateInst(arg);
	}
};

template<class _Key, class _BaseClass> class ArgClassList<_Key, _BaseClass, void>: public BaseClassList<_Key, _BaseClass, void>
{
public:
	_BaseClass* CreateInst(const _Key& key)
	{
		return Get(key).CreateInst();
	}
};

template<class _Key, class _BaseClass, class _Arg = void> class ClassList: public ArgClassList<_Key, _BaseClass, _Arg>
{
};




template<class _Key, class _BaseClass, class _Arg> BaseClassList<_Key, _BaseClass, _Arg>::~BaseClassList()
{
	Clear();
}

template<class _Key, class _BaseClass, class _Arg> template<class _Class> void BaseClassList<_Key, _BaseClass, _Arg>::Add(const _Key& key)
{
	if (Find(key))
		throw lsl::Error("class not found");

	_classList[key] = new ClassInst<_Key, _Class, _BaseClass, _Arg>(key);
}

template<class _Key, class _BaseClass, class _Arg> template<class _Class> void BaseClassList<_Key, _BaseClass, _Arg>::Add()
{
	Add<_Class>(_Class::Type);
}

template<class _Key, class _BaseClass, class _Arg> void BaseClassList<_Key, _BaseClass, _Arg>::Del(const _Key& key)
{
	if (!Find(key))
		throw lsl::Error("class del not found");

	delete (*iter);
	_classList.erase(iter);
}

template<class _Key, class _BaseClass, class _Arg> void BaseClassList<_Key, _BaseClass, _Arg>::Clear()
{
	for (_ClassList::iterator iter = _classList.begin(); iter != _classList.end(); ++iter)
		delete iter->second;
	_classList.clear();
}

template<class _Key, class _BaseClass, class _Arg> typename BaseClassList<_Key, _BaseClass, _Arg>::MyClassInst* BaseClassList<_Key, _BaseClass, _Arg>::Find(const _Key& key) const
{
	_ClassList::const_iterator iter = _classList.find(key);
	return iter == _classList.end() ? 0 : iter->second;
}

template<class _Key, class _BaseClass, class _Arg> template<class _Class> typename BaseClassList<_Key, _BaseClass, _Arg>::MyClassInst* BaseClassList<_Key, _BaseClass, _Arg>::FindByClass() const
{
	for (_ClassList::const_iterator iter = _classList.begin(); iter != _classList.end(); ++iter)
		if (iter->second->GetTypeInfo() == typeid(_Class))
			return iter->second;

	return 0;
}

template<class _Key, class _BaseClass, class _Arg> typename BaseClassList<_Key, _BaseClass, _Arg>::MyClassInst& BaseClassList<_Key, _BaseClass, _Arg>::Get(const _Key& key) const
{
	MyClassInst* item = Find(key);
	if (!item)
		throw lsl::Error("BaseClassList::Get(const _Key& key)");

	return *item;
}

template<class _Key, class _BaseClass, class _Arg> template<class _Class> typename BaseClassList<_Key, _BaseClass, _Arg>::MyClassInst& BaseClassList<_Key, _BaseClass, _Arg>::GetByClass() const
{
	MyClassInst* item = FindByClass<_Class>();
	if (!item)
		throw lsl::Error("BaseClassList::GetByClass()");

	return *item;
}

template<class _Key, class _BaseClass, class _Arg> template<class _Class1, class _Class2> _Class1& BaseClassList<_Key, _BaseClass, _Arg>::As(_Class2& inst) const
{
	return lsl::StaticCast<_Class1>(inst);
}

template<class _Key, class _BaseClass, class _Arg> template<class _Class1, class _Class2> _Class1* BaseClassList<_Key, _BaseClass, _Arg>::Is(_Class2* inst) const
{
	return dynamic_cast<_Class1>(inst);
}

}

#ifdef DEBUG_MEMORY
	#pragma pop_macro("new")
#endif

#endif