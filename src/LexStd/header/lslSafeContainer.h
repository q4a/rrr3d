#ifndef LSL_SAFECONTAINER
#define LSL_SAFECONTAINER

#include "lslCommon.h"
#include "lslAutoRef.h"
#include "lslObject.h"

namespace lsl
{

template<class _Cont, class _Item> class SafeContainer: public lsl::Object
{
private:
	typedef typename _Cont::iterator iterator;

	struct _Position: public lsl::Object
	{
		friend SafeContainer;
	private:
		SafeContainer* _owner;
		//Указывает на текущий элемент
		iterator _myIter;
		//некорректный итератор (был удален). К нему нельзя получать доступ
		bool _invalid;
	public:		
		_Position(SafeContainer* owner, const iterator& iter): _owner(owner), _myIter(iter), _invalid(false)
		{
			_owner->InsertPos(this);
		}	
		virtual ~_Position()
		{
			_owner->RemovePos(this);
		}

		_Item& GetItem()
		{
			return *_myIter;
		}
		const _Item& GetItem() const
		{
			return *_myIter;
		}
	};

	typedef std::list<_Position*> _PosList;	
public:
	typedef _Item Item;
	typedef const lsl::AutoRef<_Position> Position;
private:
	_Cont* _cont;
	_PosList _posList;
	bool _safeMode;

	void InsertPos(_Position* value)
	{
		_posList.push_back(value);

		if (!_safeMode)
		{
			_safeMode = true;
		}
	}
	void RemovePos(_Position* value)
	{
		_posList.remove(value);

		bool mode = !_posList.empty();
		if (_safeMode != mode)
		{
			_safeMode = mode;
		}
	}
public:
	SafeContainer(_Cont* cont): _cont(cont), _safeMode(false)
	{
		LSL_ASSERT(cont);
	}
	virtual ~SafeContainer()
	{
		LSL_ASSERT(_posList.empty());
	}

	Position First()
	{
		return Position(new _Position(this, _cont->begin()), this);
	}

	Item* Current(const Position& pos)
	{
		LSL_ASSERT(_safeMode && !pos->_invalid);

		return pos->_myIter != _cont->end() ? &(*pos->_myIter) : 0;
	}

	void Next(Position& pos)
	{
		LSL_ASSERT(_safeMode);

		//некооректный указатель уже указывает на следующий элемент. Нужно только обнулить _invalid чтобы разркшить доступ
		if (pos->_invalid)		
			pos->_invalid = false;		
		else if (pos->_myIter != _cont->end())
			++pos->_myIter;
	}

	bool SafeInsert(const Item& value)
	{
		return true;
	}
	bool SafeRemove(iterator valIter)
	{
		if (_safeMode)
		{
			iterator nextIter = valIter;
			++nextIter;

			for (_PosList::iterator posIter = _posList.begin(); posIter != _posList.end(); ++posIter)
				if ((*posIter)->_myIter == valIter)
				{
					(*posIter)->_myIter = nextIter;
					(*posIter)->_invalid = true;
				}
		}

		return true;
	}

	bool SafeRemove(iterator sIter, iterator eIter)
	{
		if (_safeMode)
			for (iterator iter = sIter; iter != eIter; ++iter)
				SafeRemove(iter);

		return true;
	}

	bool SafeClear()
	{
		return SafeRemove(_cont->begin(), _cont->end());
	}

	void Insert(const Item& value)
	{
		if (SafeInsert(value))
			_cont->Insert(value);
	}

	void Remove(iterator iter)
	{
		if (SafeRemove(iter))
			_cont->Remove(iter);
	}

	void Remove(const Position& pos)
	{
		LSL_ASSERT(!pos->_invalid);

		Remove(pos->_myIter);
	}

	void Remove(const Item& value)
	{
		Remove(std::find(_cont->begin(), _cont->end(), value));
	}

	void Remove(iterator sIter, iterator eIter)
	{
		if (SafeRemove(sIter, eIter))
			_cont->Remove(sIter, eIter);
	}

	void Clear()
	{
		Remove(_cont->begin(), _cont->end());
	}
};

}

#endif