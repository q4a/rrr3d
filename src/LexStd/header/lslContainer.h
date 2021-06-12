#ifndef LSL_CONTAINER
#define LSL_CONTAINER

#include "lslCommon.h"
#include "lslSafeContainer.h"
#include "lslObject.h"

namespace lsl
{

typedef std::vector<bool> BoolVec;
typedef std::map<unsigned, bool> BoolMap;

template<class _Item> class List: public std::list<_Item>
{
private:
	typedef std::list<_Item> _MyBase;
private:
	using _MyBase::remove;
	using _MyBase::remove_if;
public:
	//Удаляет один элемент с таким значением
	iterator Remove(const _Item& item)
	{
		iterator iter = Find(item);
		if (iter != end())
			return _MyBase::erase(iter);
		return iter;
	}

	template<class _T> void RemoveIf(_T pred)
	{
		_MyBase::remove_if(pred);
	}

	//Удаляет все элементы с таким значением
	void RemoveValue(const _Item& item)
	{
		_MyBase::remove(item);
	}

	iterator Find(const _Item& item)
	{
		return std::find(begin(), end(), item);
	}

	const_iterator Find(const _Item& item) const
	{
		return std::find(begin(), end(), item);
	}

	bool IsFind(const _Item& item) const
	{
		return Find(item) != end();
	}
};

template<class _Item> class Vector: public std::vector<_Item>
{
private:
	typedef std::vector<_Item> _MyBase;
public:
	//Удаляет один элемент с таким значением
	iterator Remove(const _Item& item)
	{
		iterator iter = Find(item);
		if (iter != end())
			return _MyBase::erase(iter);
		return iter;
	}

	iterator Find(const _Item& item)
	{
		return std::find(begin(), end(), item);
	}

	const_iterator Find(const _Item& item) const
	{
		return std::find(begin(), end(), item);
	}

	bool IsFind(const _Item& item) const
	{
		return Find(item) != end();
	}
};

//Модель контейнера способная к уведомлению. При создании и уничтожении экземпляра класса уведомления не происходит. Если значения элементов меняется уведомления также нет. В этом случай можно использовать метод вроде обычного Update
template<class _Item> class BaseContainer
{
protected:
	virtual void InsertItem(_Item& item) {};
	virtual void RemoveItem(_Item& item) {};
};

template<> class BaseContainer<Object*>
{
private:
	typedef Object* _Item;
protected:
	virtual void InsertItem(_Item& item)
	{
		item->AddRef();
	}
	virtual void RemoveItem(_Item& item)
	{
		item->Release();
	}
};

template<class _Item> class Container: public BaseContainer<_Item>
{
private:
	typedef BaseContainer<_Item> _MyBase;
	typedef Container<_Item> _MyClass;
	typedef lsl::List<_Item> _Cont;
	typedef SafeContainer<_MyClass, _Item> _SafeCont;
public:
	enum Duplicates
	{
		dupError,
		dupIgnore,
		dupAccept
	};	

	typedef _Item Item;
	typedef typename _Cont::iterator iterator;
	typedef typename _Cont::const_iterator const_iterator;
	typedef typename _SafeCont::Position Position;

	typedef typename _Cont::reverse_iterator reverse_iterator;
	typedef typename _Cont::const_reverse_iterator const_reverse_iterator;
private:
	Duplicates _duplicates;	

	_Cont _cont;
	_SafeCont* _safeCont;
protected:
	//Манипуляции с элементами контейнера
	bool AddItem(const _Item& item);
	void DeleteItem(iterator iter);
	void DeleteItem(iterator sIter, iterator eIter);
	void ClearItems();
public:
	Container(Duplicates duplicates = dupError);
	Container(const _MyClass& ref);
	~Container();
	
	void Insert(const _Item& item);
	void Remove(iterator iter);
	void Remove(const Position& pos);
	void Remove(const _Item& item);	
	void Remove(iterator sIter, iterator eIter);	
	void Clear();

	bool CheckDuplicate(const _Item& item) const;
	iterator Find(const _Item& item);
	const_iterator Find(const _Item& item) const;
	bool IsFind(const _Item& item) const;

	unsigned Size() const;
	bool Empty() const;

	iterator begin();
	iterator end();

	const_iterator begin() const;	
	const_iterator end() const;

	reverse_iterator rbegin();
	reverse_iterator rend();

	const_reverse_iterator rbegin() const;
	const_reverse_iterator rend() const;

	_Item& front();
	_Item& back();

	const _Item& front() const;	
	const _Item& back() const;

	Position First();
	_Item* Current(const Position& pos);
	void Next(Position& pos);

	_MyClass& operator=(const _MyClass& value);
};




template<class _Item> Container<_Item>::Container(Duplicates duplicates): _safeCont(0), _duplicates(duplicates)
{
}

template<class _Item> Container<_Item>::Container(const _MyClass& ref): _safeCont(0), _duplicates(dupError)
{
	*this = ref;
}

template<class _Item> Container<_Item>::~Container()
{
	Clear();

	lsl::SafeDelete(_safeCont);
}

template<class _Item> bool Container<_Item>::AddItem(const _Item& item)
{
	if (!CheckDuplicate(item))
		return false;

	_cont.push_back(item);

	return true;
}

template<class _Item> void Container<_Item>::DeleteItem(iterator iter)
{
	_cont.erase(iter);
}

template<class _Item> void Container<_Item>::DeleteItem(iterator sIter, iterator eIter)
{
	_cont.erase(sIter, eIter);
}

template<class _Item> void Container<_Item>::ClearItems()
{
	_cont.clear();
}

template<class _Item> void Container<_Item>::Insert(const _Item& item)
{
	bool safe = !(_safeCont && !_safeCont->SafeInsert(item));

	if (safe && AddItem(item))
		InsertItem(_cont.back());
}

template<class _Item> void Container<_Item>::Remove(iterator iter)
{
	bool safe = !(_safeCont && !_safeCont->SafeRemove(iter));

	if (safe)
	{
		RemoveItem(*iter);
		DeleteItem(iter);
	}
}

template<class _Item> void Container<_Item>::Remove(const Position& pos)
{
	LSL_ASSERT(_safeCont);

	_safeCont->Remove(pos);
}

template<class _Item> void Container<_Item>::Remove(const _Item& item)
{
	Remove(Find(item));
}

template<class _Item> void Container<_Item>::Remove(iterator sIter, iterator eIter)
{
	bool safe = !(_safeCont && !_safeCont->SafeRemove(sIter, eIter));

	if (safe)
	{
		for (iterator iter = sIter; iter != eIter; ++iter)
			RemoveItem(*iter);
		DeleteItem(sIter, eIter);
	}
}

template<class _Item> void Container<_Item>::Clear()
{
	bool safe = !(_safeCont && !_safeCont->SafeClear());
	
	if (safe)
		Remove(begin(), end());
}
template<class _Item> bool Container<_Item>::CheckDuplicate(const _Item& item) const
{
	if (_duplicates == dupAccept)
		return true;

	bool isFind = Find(item) != _cont.end();
	switch (_duplicates)
	{
	case dupError:
		LSL_ASSERT(!isFind);		
		return !isFind;

	case dupIgnore:
		return !isFind;

	default:
		LSL_ASSERT(false);

		return false;
	}
}

template<class _Item> typename Container<_Item>::iterator Container<_Item>::Find(const _Item& item)
{
	return std::find(begin(), end(), item);
}

template<class _Item> typename Container<_Item>::const_iterator Container<_Item>::Find(const _Item& item) const
{
	return std::find(begin(), end(), item);
}

template<class _Item> bool Container<_Item>::IsFind(const _Item& item) const
{
	return Find(item) != end();
}

template<class _Item> unsigned Container<_Item>::Size() const
{
	return _cont.size();
}

template<class _Item> bool Container<_Item>::Empty() const
{
	return _cont.empty();
}

template<class _Item> typename Container<_Item>::iterator Container<_Item>::begin()
{
	return _cont.begin();
}

template<class _Item> typename Container<_Item>::iterator Container<_Item>::end()
{
	return _cont.end();
}

template<class _Item> typename Container<_Item>::const_iterator Container<_Item>::begin() const
{
	return _cont.begin();
}

template<class _Item> typename Container<_Item>::const_iterator Container<_Item>::end() const
{
	return _cont.end();
}

template<class _Item> typename Container<_Item>::reverse_iterator Container<_Item>::rbegin()
{
	return _cont.rbegin();
}

template<class _Item> typename Container<_Item>::reverse_iterator Container<_Item>::rend()
{
	return _cont.rend();
}

template<class _Item> typename Container<_Item>::const_reverse_iterator Container<_Item>::rbegin() const
{
	return _cont.rbegin();
}

template<class _Item> typename Container<_Item>::const_reverse_iterator Container<_Item>::rend() const
{
	return _cont.rend();
}

template<class _Item> _Item& Container<_Item>::front()
{
	return *begin();
}

template<class _Item> _Item& Container<_Item>::back()
{
	return *(--end());
}

template<class _Item> const _Item& Container<_Item>::front() const
{
	return *begin();
}

template<class _Item> const _Item& Container<_Item>::back() const
{
	return *(--end());
}

template<class _Item> typename Container<_Item>::Position Container<_Item>::First()
{
	if (!_safeCont)
		_safeCont = new _SafeCont(this);

	return _safeCont->First();
}

template<class _Item> _Item* Container<_Item>::Current(const Position& pos)
{
	LSL_ASSERT(_safeCont);

	return _safeCont->Current(pos);
}

template<class _Item> void Container<_Item>::Next(Position& pos)
{
	LSL_ASSERT(_safeCont);

	_safeCont->Next(pos);
}

template<class _Item> typename Container<_Item>::_MyClass& Container<_Item>::operator=(const _MyClass& value)
{
	Clear();

	_duplicates = value._duplicates;
	_cont = value._cont;
	for (iterator iter = begin(); iter != end(); ++iter)
		InsertItem(*iter);

	return *this;
}

}

#endif