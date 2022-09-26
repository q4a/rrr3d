#ifndef LSL_COLLECTION
#define LSL_COLLECTION

#include "lslCommon.h"
#include "lslBaseCollection.h"

namespace lsl
{

//Специализация для случая присутсвтия аргумента, и аргумент не есть указатель на сам класс
template<class _Item, class _IdType, class _Arg> class Collection<_Item, _IdType, _Arg, void>: public BaseCollectionCL<_Item, _IdType, _Arg>
{
private:
	typedef BaseCollectionCL<_Item, _IdType, _Arg> _MyBase;
protected:
	virtual _Item* CreateItem(const _IdType& key, const _Arg& arg);
	virtual void DestroyItem(_Item* value);

	void LoadItemFrom(SReader* reader, const _Arg& arg);
public:
	virtual ~Collection();

	//Добавление с ид key
	_Item& Add(_IdType key, const _Arg& arg);
	//добавление производного item-a с классом _Type
	template<class _Type> _Type& Add(const _Arg& arg);
};

//Специализация для случая отсутствия аргумента
template<class _Item, class _IdType> class Collection<_Item, _IdType, void, void>: public BaseCollectionCL<_Item, _IdType, void>
{
private:
	typedef BaseCollectionCL<_Item, _IdType, void> _MyBase;
protected:
	virtual _Item* CreateItem(const _IdType& key);
	virtual void DestroyItem(_Item* value);

	virtual void LoadItem(SReader* reader);	
public:
	virtual ~Collection();

	//Добавление с ид key
	_Item& Add(_IdType key);
	//добавление производного item-a с классом _Type
	template<class _Type> _Type& Add();
};

//Коллекция с аргументом, при чем случай с аргументом указателя на сам класс
template<class _Item, class _Arg, class _ArgThis> class Collection<_Item, void, _Arg, _ArgThis>: public BaseCollection<_Item, void>
{
private:
	typedef BaseCollection<_Item, void> _MyBase;
protected:
	virtual _Item* CreateItem();
	virtual void DestroyItem(_Item* value);

	virtual void LoadItem(SReader* reader);	
public:
	virtual ~Collection();

	_Item& Add();
};

//Специализация для случая присутсвтия аргумента, и аргумент не есть указатель на сам класс
template<class _Item, class _Arg> class Collection<_Item, void, _Arg, void>: public BaseCollection<_Item, void>
{
private:
	typedef BaseCollection<_Item, void> _MyBase;
protected:
	virtual _Item* CreateItem(const _Arg& arg);
	virtual void DestroyItem(_Item* value);

	void LoadItemFrom(SReader* reader, const _Arg& arg);
public:
	virtual ~Collection();

	_Item& Add(const _Arg& arg);
};

//Специализация для случая отсутствия аргумента
template<class _Item> class Collection<_Item, void, void, void>: public BaseCollection<_Item, void>
{
private:
	typedef BaseCollection<_Item, void> _MyBase;
protected:
	virtual _Item* CreateItem();
	virtual void DestroyItem(_Item* value);

	virtual void LoadItem(SReader* reader);	
public:
	virtual ~Collection();

	_Item& Add();
};




template<class _Item, class _IdType, class _Arg> Collection<_Item, _IdType, _Arg, void>::~Collection()
{
	Clear();
}

template<class _Item, class _IdType, class _Arg> _Item* Collection<_Item, _IdType, _Arg, void>::CreateItem(const _IdType& key, const _Arg& arg)
{
	return GetClassList()->CreateInst(key, arg);
}

template<class _Item, class _IdType, class _Arg> void Collection<_Item, _IdType, _Arg, void>::DestroyItem(_Item* value)
{
	delete value;
}

template<class _Item, class _IdType, class _Arg> void Collection<_Item, _IdType, _Arg, void>::LoadItemFrom(SReader* reader, const _Arg& arg)
{
	ReadItem(reader, &Add(LoadType(reader), arg));
}

template<class _Item, class _IdType, class _Arg> _Item& Collection<_Item, _IdType, _Arg, void>::Add(_IdType key, const _Arg& arg)
{
	return _MyBase::Add(Value(CreateItem(key, arg), key));
}

template<class _Item, class _IdType, class _Arg> template<class _Type> _Type& Collection<_Item, _IdType, _Arg, void>::Add(const _Arg& arg)
{
	ClassList::MyClassInst* classInst = GetClassList()->FindByClass<_Type>();
	if (!classInst)
		throw lsl::Error("_Type& Collection::Add()");

	return lsl::StaticCast<_Type&>(Add(classInst->GetKey(), arg));
}




template<class _Item, class _IdType> Collection<_Item, _IdType, void, void>::~Collection()
{
	Clear();
}

template<class _Item, class _IdType> _Item* Collection<_Item, _IdType, void, void>::CreateItem(const _IdType& key)
{
	return GetClassList()->CreateInst(key);
}

template<class _Item, class _IdType> void Collection<_Item, _IdType, void, void>::DestroyItem(_Item* value)
{
	delete value;
}

template<class _Item, class _IdType> void Collection<_Item, _IdType, void, void>::LoadItem(SReader* reader)
{
	ReadItem(reader, &Add(LoadType(reader)));
}

template<class _Item, class _IdType> _Item& Collection<_Item, _IdType, void, void>::Add(_IdType key)
{
	return _MyBase::Add(Value(CreateItem(key), key));
}

template<class _Item, class _IdType> template<class _Type> _Type& Collection<_Item, _IdType, void, void>::Add()
{
	ClassList::MyClassInst* classInst = GetClassList()->FindByClass<_Type>();
	if (!classInst)
		throw lsl::Error("_Type& Collection::Add()");

	return lsl::StaticCast<_Type&>(Add(classInst->GetKey()));
}





template<class _Item, class _Arg, class _ArgThis> Collection<_Item, void, _Arg, _ArgThis>::~Collection()
{
	Clear();
}

template<class _Item, class _Arg, class _ArgThis> _Item* Collection<_Item, void, _Arg, _ArgThis>::CreateItem()
{
	return new _Item(static_cast<_ArgThis>(this)); 
}

template<class _Item, class _Arg, class _ArgThis> void Collection<_Item, void, _Arg, _ArgThis>::DestroyItem(_Item* value)
{
	delete value;
}

template<class _Item, class _Arg, class _ArgThis> void Collection<_Item, void, _Arg, _ArgThis>::LoadItem(SReader* reader)
{
	ReadItem(reader, &Add());
}

template<class _Item, class _Arg, class _ArgThis> _Item& Collection<_Item, void, _Arg, _ArgThis>::Add()
{
	return _MyBase::Add(CreateItem());
}




template<class _Item, class _Arg> Collection<_Item, void, _Arg, void>::~Collection()
{
	Clear();
}

template<class _Item, class _Arg> _Item* Collection<_Item, void, _Arg, void>::CreateItem(const _Arg& arg)
{
	return new _Item(arg);
}

template<class _Item, class _Arg> void Collection<_Item, void, _Arg, void>::DestroyItem(_Item* value)
{
	delete value;
}

template<class _Item, class _Arg> void Collection<_Item, void, _Arg, void>::LoadItemFrom(SReader* reader, const _Arg& arg)
{
	ReadItem(reader, &Add(arg));
}

template<class _Item, class _Arg> _Item& Collection<_Item, void, _Arg, void>::Add(const _Arg& arg)
{
	return _MyBase::Add(CreateItem(arg));
}




template<class _Item> Collection<_Item, void, void, void>::~Collection()
{
	Clear();
}

template<class _Item> _Item* Collection<_Item, void, void, void>::CreateItem()
{
	return new _Item();
}

template<class _Item> void Collection<_Item, void, void, void>::DestroyItem(_Item* value)
{
	delete value;
}

template<class _Item> void Collection<_Item, void, void, void>::LoadItem(SReader* reader)
{
	ReadItem(reader, &Add());
}

template<class _Item> _Item& Collection<_Item, void, void, void>::Add()
{
	return _MyBase::Add(CreateItem());
}

}

#endif