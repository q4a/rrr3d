#ifndef LSL_TYP_COLLECTION
#define LSL_TYP_COLLECTION

#include "lslCommon.h"
#include "lslSerialization.h"
#include "lslCollectionItem.h"
#include "lslException.h"
#include "lslClassList.h"
#include "lslUtility.h"

namespace lsl
{

template<class _Item, class _IdType> class CollectionValue
{
private:
	_Item* _item;
	_IdType _type;
public:
	CollectionValue(): _item(0), _type(_IdType()) {}
	CollectionValue(_Item* item, _IdType type): _item(item), _type(type) {}

	operator _Item*() const
	{
		return _item;
	}

	_Item& operator*() const
	{
		return *_item;
	}

	_Item* operator->() const
	{
		return _item;
	}

	_Item* GetItem() const
	{
		return _item;
	}
	template<class _Type> _Type* GetItem() const
	{
		return lsl::StaticCast<_Type*>(_item);
	}
	template<class _Type> _Type* IsItem() const
	{
		return dynamic_cast<_Type*>(_item);
	}

	_IdType GetType() const
	{
		return _type;
	}
};

template<class _Item> class CollectionValue<_Item, void>
{
private:
	_Item* _item;	
public:
	CollectionValue(): _item(0) {}
	CollectionValue(_Item* item): _item(item) {}

	operator _Item*() const
	{
		return _item;
	}

	_Item& operator*() const
	{
		return *_item;
	}

	_Item* operator->() const
	{
		return _item;
	}

	_Item* GetItem() const
	{
		return _item;
	}
};

template<class _Item, class _IdType> class BaseCollection: public virtual lsl::Serializable
{	 
public:
	typedef _Item Item;
	typedef CollectionValue<_Item, _IdType> Value;
private:
	typedef BaseCollection<_Item, _IdType> _MyClass;
	typedef std::list<Value> _Cont;
	typedef SafeContainer<_MyClass, Value> _SafeCont;

	friend _SafeCont;
public:
	enum ContainerChange {ccAdd, ccDelete};

	typedef typename _Cont::iterator iterator;
	typedef typename _Cont::const_iterator const_iterator;
	typedef typename _SafeCont::Position Position;
private:
	_Cont _cont;
	_SafeCont* _safeCont;

	unsigned _lockDestr;
	unsigned _lockNotify;

	//ќперации дл€ SafeContainer
	void Insert(const Value& item);
	void Remove(iterator iter);
	void Remove(iterator sIter, iterator eIter);
protected:
	//‘абричные методы
	//virtual _Item* CreateItem(...);
	virtual void DestroyItem(_Item* value) = 0;

	//”ведомлени€ о вставке/удалении
	virtual void InsertItem(const Value& value);
	virtual void RemoveItem(const Value& value);

	void LockDestr();
	void UnlockDestr();
	bool IsLockDestr() const;
	//
	void LockNotify();
	void UnlockNotify();
	bool IsLockNotify() const;

	//ƒобавление нового item, по концепции сама коллекци€ создает item поэтому здесь он объ€влен как protected, но есть возможность переопределени€
	_Item& AddItem(const Value& value);
	_Item& Add(const Value& value);

	//так как не все item-ы унаследованы от lsl::Seriazable то примен€етс€ перегрузка чтобы их разделить
	void WriteItem(SWriter* writer, void* value);
	void WriteItem(SWriter* writer, lsl::Serializable* value);
	void ReadItem(SReader* reader, void* value);
	void ReadItem(SReader* reader, lsl::Serializable* value);
	//—охранение итем-а в кусок writer с рекомендуемым именем aName
	virtual void SaveItem(SWriter* writer, iterator pItem, const std::string& aName);
	//«агрузка итем-а из reader, где reader есть сам item
	virtual void LoadItem(SReader* reader) = 0;
	//
	virtual void Save(SWriter* writer);
	virtual void Load(SReader* reader);
public:
	BaseCollection();
	virtual ~BaseCollection();

	iterator Delete(iterator iter);
	void Delete(const Position& pos);
	iterator Delete(_Item* item);
	iterator Delete(iterator sIter, iterator eIter);	
	void Clear();

	//ѕоиск
	iterator Find(_Item* item);
	unsigned Size() const;
	bool Empty() const;

	iterator begin();
	iterator end();

	const_iterator begin() const;
	const_iterator end() const;

	_Item& front();
	_Item& back();

	Position First();
	Value* Current(const Position& pos);
	void Next(Position& pos);

	_IdType GetItemType(_Item* item);
};

// ласс фабрика
template<class _Item, class _IdType, class _Arg> class BaseCollectionCL: public BaseCollection<_Item, _IdType>
{
private:
	typedef BaseCollection<_Item, _IdType> _MyBase;
public:
	typedef ClassList<_IdType, _Item, _Arg> ClassList;	
private:
	ClassList* _classList;
	bool _createClassList;
	StringList _classNames;
protected:
	void SaveType(SWriter* writer, Value value);
	_IdType LoadType(SReader* reader);

	virtual void SaveItem(SWriter* writer, iterator pItem, const std::string& aName);
public:
	BaseCollectionCL();
	virtual ~BaseCollectionCL();

	ClassList* GetClassList();
	ClassList* GetOrCreateClassList();
	void SetClassList(ClassList* value);

	const StringList& GetClassNames() const;
	void SetClassNames(const StringList& value);
	void SetClassNames(const char* value[], unsigned cnt);
};

// оллекци€ с аргументом, при чем случай с аргументом указател€ на сам класс
template<class _Item, class _IdType, class _Arg, class _ArgThis> class Collection: public BaseCollectionCL<_Item, _IdType, _Arg>
{
private:
	typedef BaseCollectionCL<_Item, _IdType, _Arg> _MyBase;
protected:
	virtual _Item* CreateItem(const _IdType& key);
	virtual void DestroyItem(_Item* value);

	virtual void LoadItem(SReader* reader);
public:
	virtual ~Collection();

	//ƒобавление с ид key
	_Item& Add(_IdType key);
	//добавление производного item-a с классом _Type
	template<class _Type> _Type& Add();
};

//—читаетс€ что аргументом Item-a об€зательно выступает сам скласс или один из его наследников
//ѕараметр _Traits нужен дл€ определени€ общих свойств у шаблонных коллекций (это общее свойства обычно передаетс€ в качестве аргумента Item-у)
//“ак как Traits включаетс€ путем множественного наследовани€, то нет аозможности использовать специализацию Collection<Item, _IdType, Arg, ArgThis>, поскольку в шаблонный параметр _ArgThis никаким образом не вставить унаследованный шаблон ComCollection.
template<class _Item, class _IdType, class _Arg, class _ArgThis> class ComCollection: public Collection<_Item, _IdType, _Arg, _ArgThis>, public CollectionTraits
{
private:
	typedef Collection<_Item, _IdType, _Arg, _ArgThis> _MyBase;
	typedef ComCollection<_Item, _IdType, _Arg, _ArgThis> _MyClass;	
public:
	static const char* const cDefItemName;
private:
	virtual CollectionItem* FindItem(const std::string& name);
protected:
	virtual void InsertItem(const Value& value);
	virtual void OnItemChangeName(CollectionItem* item, const std::string& newName);

	virtual void Save(SWriter* writer);
	virtual void Load(SReader* reader);
public:
	virtual _Item* Find(const std::string& name);
	virtual bool ValidateName(const std::string& name);

	std::string MakeUniqueName(const std::string& base);
};

template<class _Item, class _IdType, class _Arg, class _ArgThis> const char* const ComCollection<_Item, _IdType, _Arg, _ArgThis>::cDefItemName = "item";




template<class _Item, class _IdType> BaseCollection<_Item, _IdType>::BaseCollection(): _safeCont(0), _lockDestr(0), _lockNotify(0)
{
}

template<class _Item, class _IdType> BaseCollection<_Item, _IdType>::~BaseCollection()
{
	Clear();

	lsl::SafeDelete(_safeCont);
}

template<class _Item, class _IdType> void BaseCollection<_Item, _IdType>::Insert(const Value& item)
{
	Add(item);
}

template<class _Item, class _IdType> void BaseCollection<_Item, _IdType>::Remove(iterator iter)
{
	_cont.erase(iter);	
}

template<class _Item, class _IdType> void BaseCollection<_Item, _IdType>::Remove(iterator sIter, iterator eIter)
{
	_cont.erase(sIter, eIter);
}

template<class _Item, class _IdType> void BaseCollection<_Item, _IdType>::InsertItem(const Value& value)
{
	//Nothing
}

template<class _Item, class _IdType> void BaseCollection<_Item, _IdType>::RemoveItem(const Value& value)
{
	//Nothing
}

template<class _Item, class _IdType> void BaseCollection<_Item, _IdType>::LockDestr()
{
	++_lockDestr;
}

template<class _Item, class _IdType> void BaseCollection<_Item, _IdType>::UnlockDestr()
{
	LSL_ASSERT(_lockDestr > 0);

	--_lockDestr;
}

template<class _Item, class _IdType> bool BaseCollection<_Item, _IdType>::IsLockDestr() const
{
	return _lockDestr > 0;
}

template<class _Item, class _IdType> void BaseCollection<_Item, _IdType>::LockNotify()
{
	++_lockNotify;
}

template<class _Item, class _IdType> void BaseCollection<_Item, _IdType>::UnlockNotify()
{
	LSL_ASSERT(_lockNotify > 0);

	--_lockNotify;
}

template<class _Item, class _IdType> bool BaseCollection<_Item, _IdType>::IsLockNotify() const
{
	return _lockNotify > 0;
}

template<class _Item, class _IdType> _Item& BaseCollection<_Item, _IdType>::AddItem(const Value& value)
{
	bool safe = !(_safeCont && !_safeCont->SafeInsert(value));

	if (safe)
	{
		_cont.push_back(value);
		
		if (!IsLockNotify())
			InsertItem(value);
	}

	return *value;
}

template<class _Item, class _IdType> _Item& BaseCollection<_Item, _IdType>::Add(const Value& value)
{
	return AddItem(value);	
}

template<class _Item, class _IdType> void BaseCollection<_Item, _IdType>::WriteItem(SWriter* writer, void* value)
{
}

template<class _Item, class _IdType> void BaseCollection<_Item, _IdType>::WriteItem(SWriter* writer, lsl::Serializable* value)
{
	writer->SaveSerializable(value);
}

template<class _Item, class _IdType> void BaseCollection<_Item, _IdType>::ReadItem(SReader* reader, void* value)
{
}

template<class _Item, class _IdType> void BaseCollection<_Item, _IdType>::ReadItem(SReader* reader, lsl::Serializable* value)
{
	reader->LoadSerializable(value);
}

template<class _Item, class _IdType> void BaseCollection<_Item, _IdType>::SaveItem(SWriter* writer, iterator pItem, const std::string& aName)
{
	lsl::SWriter* child = writer->NewDummyNode(aName.c_str());
	WriteItem(child, *pItem);
}

template<class _Item, class _IdType> void BaseCollection<_Item, _IdType>::Save(SWriter* writer)
{
	lsl::SWriter* child = writer->NewDummyNode("items");
	int i = 0;
	for (iterator iter = begin(); iter != end(); ++iter, ++i)
	{
		char buf[128];
		sprintf_s(buf, "item%d", i);
		SaveItem(child, iter, buf);
	}
}

template<class _Item, class _IdType> void BaseCollection<_Item, _IdType>::Load(SReader* reader)
{
	Clear();

	if (SReader* child = reader->ReadValue("items"))
	{
		SReader* value = child->FirstChildValue();
		while (value)
		{
			LoadItem(value);
			value = value->NextValue();
		}
	}
}

template<class _Item, class _IdType> typename BaseCollection<_Item, _IdType>::iterator BaseCollection<_Item, _IdType>::Delete(iterator iter)
{
	if (!IsLockNotify())
		RemoveItem(*iter);		
	if (!IsLockDestr())
		DestroyItem(*iter);

	bool safe = !(_safeCont && !_safeCont->SafeRemove(iter));
	if (safe)
		return _cont.erase(iter);

	iterator res = iter;
	return ++res;
}

template<class _Item, class _IdType> void BaseCollection<_Item, _IdType>::Delete(const Position& pos)
{
	LSL_ASSERT(_safeCont);

	if (!IsLockNotify())
		RemoveItem(pos->GetItem());		
	if (!IsLockDestr())
		DestroyItem(pos->GetItem());

	_safeCont->Remove(pos);
}

template<class _Item, class _IdType> typename BaseCollection<_Item, _IdType>::iterator BaseCollection<_Item, _IdType>::Delete(_Item* item)
{
	return Delete(Find(item));
}

template<class _Item, class _IdType> typename BaseCollection<_Item, _IdType>::iterator BaseCollection<_Item, _IdType>::Delete(iterator sIter, iterator eIter)
{
	for (iterator iter = sIter; iter != eIter; ++iter)
	{
		if (!IsLockNotify())
			RemoveItem(*iter);			
		if (!IsLockDestr())
			DestroyItem(*iter);
	}

	bool safe = !(_safeCont && !_safeCont->SafeRemove(sIter, eIter));
	if (safe)	
		return _cont.erase(sIter, eIter);

	return eIter;
}

template<class _Item, class _IdType> void BaseCollection<_Item, _IdType>::Clear()
{
	Delete(begin(), end());
}

template<class _Item, class _IdType> typename BaseCollection<_Item, _IdType>::iterator BaseCollection<_Item, _IdType>::Find(_Item* item)
{
	return std::find(begin(), end(), item);
}

template<class _Item, class _IdType> unsigned BaseCollection<_Item, _IdType>::Size() const
{
	return _cont.size();
}

template<class _Item, class _IdType> bool BaseCollection<_Item, _IdType>::Empty() const
{
	return _cont.empty();
}

template<class _Item, class _IdType> typename BaseCollection<_Item, _IdType>::iterator BaseCollection<_Item, _IdType>::begin()
{
	return _cont.begin();
}

template<class _Item, class _IdType> typename BaseCollection<_Item, _IdType>::iterator BaseCollection<_Item, _IdType>::end()
{
	return _cont.end();
}

template<class _Item, class _IdType> typename BaseCollection<_Item, _IdType>::const_iterator BaseCollection<_Item, _IdType>::begin() const
{
	return _cont.begin();
}

template<class _Item, class _IdType> typename BaseCollection<_Item, _IdType>::const_iterator BaseCollection<_Item, _IdType>::end() const
{
	return _cont.end();
}

template<class _Item, class _IdType> _Item& BaseCollection<_Item, _IdType>::front()
{
	return *_cont.front();
}

template<class _Item, class _IdType> _Item& BaseCollection<_Item, _IdType>::back()
{
	return *_cont.back();
}

template<class _Item, class _IdType> typename BaseCollection<_Item, _IdType>::Position BaseCollection<_Item, _IdType>::First()
{
	if (!_safeCont)
		_safeCont = new _SafeCont(this);

	return _safeCont->First();
}

template<class _Item, class _IdType> typename BaseCollection<_Item, _IdType>::Value* BaseCollection<_Item, _IdType>::Current(const Position& pos)
{
	LSL_ASSERT(_safeCont);

	return _safeCont->Current(pos);
}

template<class _Item, class _IdType> void BaseCollection<_Item, _IdType>::Next(Position& pos)
{
	LSL_ASSERT(_safeCont);

	return _safeCont->Next(pos);
}

template<class _Item, class _IdType> _IdType BaseCollection<_Item, _IdType>::GetItemType(_Item* item)
{
	return FindItem(item).GetType();
}




template<class _Item, class _IdType, class _Arg> BaseCollectionCL<_Item, _IdType, _Arg>::BaseCollectionCL(): _classList(0), _createClassList(false)
{
}

template<class _Item, class _IdType, class _Arg> BaseCollectionCL<_Item, _IdType, _Arg>::~BaseCollectionCL()
{
	SetClassList(0);
}

template<class _Item, class _IdType, class _Arg> void BaseCollectionCL<_Item, _IdType, _Arg>::SaveType(SWriter* writer, Value value)
{
	_IdType type = value.GetType();
	if (_classNames.size() > static_cast<unsigned>(type))
		writer->WriteAttr("type", _classNames[type]);
	else
		writer->WriteAttr("type", type);
}

template<class _Item, class _IdType, class _Arg> _IdType BaseCollectionCL<_Item, _IdType, _Arg>::LoadType(SReader* reader)
{
	_IdType res = _IdType(0);
	if (const SReader::ValueDesc* desc = reader->ReadAttr("type"))
		if (desc->type == SReader::vtInt)		
			desc->CastToFromInt(&res);		
		else
		{
			std::string str;
			desc->CastTo(&str);
			int val = ConvStrToEnum(str, _classNames);
			if (val > -1)
				res = _IdType(val);
		}

	return res;
}

template<class _Item, class _IdType, class _Arg> void BaseCollectionCL<_Item, _IdType, _Arg>::SaveItem(SWriter* writer, iterator pItem, const std::string& aName)
{
	lsl::SWriter* child = writer->NewDummyNode(aName.c_str());
	SaveType(child, *pItem);
	WriteItem(child, *pItem);
}

template<class _Item, class _IdType, class _Arg> typename BaseCollectionCL<_Item, _IdType, _Arg>::ClassList* BaseCollectionCL<_Item, _IdType, _Arg>::GetClassList()
{
	return _classList;
}

template<class _Item, class _IdType, class _Arg> typename BaseCollectionCL<_Item, _IdType, _Arg>::ClassList* BaseCollectionCL<_Item, _IdType, _Arg>::GetOrCreateClassList()
{
	if (!_classList)
	{
		_classList = new ClassList();
		_classList->AddRef();
		_createClassList = true;
	}
	return _classList;
}

template<class _Item, class _IdType, class _Arg> void BaseCollectionCL<_Item, _IdType, _Arg>::SetClassList(ClassList* value)
{
	if (Object::ReplaceRef(_classList, value))
	{
		if (_createClassList)
		{
			delete _classList;
			_createClassList = false;
		}
		_classList = value;
	}
}

template<class _Item, class _IdType, class _Arg> const StringList& BaseCollectionCL<_Item, _IdType, _Arg>::GetClassNames() const
{
	return _classNames;
}

template<class _Item, class _IdType, class _Arg> void BaseCollectionCL<_Item, _IdType, _Arg>::SetClassNames(const StringList& value)
{
	_classNames = value;
}

template<class _Item, class _IdType, class _Arg> void BaseCollectionCL<_Item, _IdType, _Arg>::SetClassNames(const char* value[], unsigned cnt)
{
	_classNames.clear();
	for (unsigned i = 0; i < cnt; ++i)
		_classNames.push_back(value[i]);
}




template<class _Item, class _IdType, class _Arg, class _ArgThis> Collection<_Item, _IdType, _Arg, _ArgThis>::~Collection()
{
	Clear();
}

template<class _Item, class _IdType, class _Arg, class _ArgThis> _Item* Collection<_Item, _IdType, _Arg, _ArgThis>::CreateItem(const _IdType& key)
{
	return GetClassList()->CreateInst(key, static_cast<_ArgThis>(this));
}

template<class _Item, class _IdType, class _Arg, class _ArgThis> void Collection<_Item, _IdType, _Arg, _ArgThis>::DestroyItem(_Item* value)
{
	delete value;
}

template<class _Item, class _IdType, class _Arg, class _ArgThis> void Collection<_Item, _IdType, _Arg, _ArgThis>::LoadItem(SReader* reader)
{
	ReadItem(reader, &Add(LoadType(reader)));
}

template<class _Item, class _IdType, class _Arg, class _ArgThis> _Item& Collection<_Item, _IdType, _Arg, _ArgThis>::Add(_IdType key)
{
	return _MyBase::Add(Value(CreateItem(key), key));
}

template<class _Item, class _IdType, class _Arg, class _ArgThis> template<class _Type> _Type& Collection<_Item, _IdType, _Arg, _ArgThis>::Add()
{
	ClassList::MyClassInst* classInst = GetClassList()->FindByClass<_Type>();
	if (!classInst)
		throw lsl::Error("_Type& Collection::Add()");

	return lsl::StaticCast<_Type&>(Add(classInst->GetKey()));
}




template<class _Item, class _IdType, class _Arg, class _ArgThis> CollectionItem* ComCollection<_Item, _IdType, _Arg, _ArgThis>::FindItem(const std::string& name)
{
	return Find(name);
}

template<class _Item, class _IdType, class _Arg, class _ArgThis> void ComCollection<_Item, _IdType, _Arg, _ArgThis>::InsertItem(const Value& value)
{
	SetItemTraits(value, this);
	SetItemName(value, MakeUniqueName(cDefItemName));
}

template<class _Item, class _IdType, class _Arg, class _ArgThis> void ComCollection<_Item, _IdType, _Arg, _ArgThis>::OnItemChangeName(CollectionItem* item, const std::string& newName)
{
	//Nothing
}

template<class _Item, class _IdType, class _Arg, class _ArgThis> void ComCollection<_Item, _IdType, _Arg, _ArgThis>::Save(SWriter* writer)
{
	CollectionTraits::Save(writer);
	_MyBase::Save(writer);	
}

template<class _Item, class _IdType, class _Arg, class _ArgThis> void ComCollection<_Item, _IdType, _Arg, _ArgThis>::Load(SReader* reader)
{
	CollectionTraits::Load(reader);
	_MyBase::Load(reader);	
}

template<class _Item, class _IdType, class _Arg, class _ArgThis> _Item* ComCollection<_Item, _IdType, _Arg, _ArgThis>::Find(const std::string& name)
{
	for (iterator iter = begin(); iter != end(); ++iter)
		if ((*iter)->GetName() == name)
			return *iter;

	return 0;
}

template<class _Item, class _IdType, class _Arg, class _ArgThis> bool ComCollection<_Item, _IdType, _Arg, _ArgThis>::ValidateName(const std::string& name)
{
	return !Find(name);
}

template<class _Item, class _IdType, class _Arg, class _ArgThis> std::string ComCollection<_Item, _IdType, _Arg, _ArgThis>::MakeUniqueName(const std::string& base)
{
	std::ostringstream oss(base, std::ios::ate);
	std::streamoff nPos = oss.tellp();
	int cnt = 0;
	do
	{
		oss.seekp(nPos);
		oss<<cnt;
		++cnt;		
	}
	while (Find(oss.str()));
	return oss.str();
}

}

#endif