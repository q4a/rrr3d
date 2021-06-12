#include "stdafx.h"

#include "lslUpdateAble.h"
#include "lslException.h"
#include <cstdlib>
#include <cstring>

namespace lsl
{

namespace
{
	unsigned UpdateAbleUpdateIdCnt = 0;
}

unsigned UpdateAble::UpdateData::GetUniqueId()
{
	if (UpdateAbleUpdateIdCnt >= std::numeric_limits<unsigned>::max())
		throw Error("UpdateAble::UpdateData::GetUniqueId");
	return ++UpdateAbleUpdateIdCnt;
}

UpdateAble::UpdateData::~UpdateData()
{
}




UpdateAble::UpdateAble(): _updateCount(0)
{
}

void UpdateAble::NotifyChanged(UpdateAble* sender, const UpdateData* data)
{
	if (IsUpdating())
	{
		_UpdateMap::iterator iter = _updateMap.find(sender);
		if (iter == _updateMap.end())
			iter = _updateMap.insert(_UpdateMap::value_type(sender, 0)).first;
		if (iter->second && data)
			iter->second->Unite(*data);
		else
			iter->second = data ? data->Clone() : 0;
	}
	else
		DoNotifyChanged(sender, data);
}

void UpdateAble::BeginUpdate()
{
	++_updateCount;
}

void UpdateAble::EndUpdate()
{
	LSL_ASSERT(_updateCount != 0);

	if (--_updateCount == 0)
	{
		for (_UpdateMap::iterator iter = _updateMap.begin(); iter != _updateMap.end(); ++iter)
		{
			try
			{
				DoNotifyChanged(iter->first, iter->second);
				if (iter->second)
					delete iter->second;
			}
			catch (...)
			{
				//аварийное завершение
				for (_UpdateMap::iterator iter2 = iter; iter2 != _updateMap.end(); ++iter2)
					if (iter2->second)
						delete iter2->second;
				_updateMap.clear();
				throw;
			}
		}
		_updateMap.clear();
	}
}

bool UpdateAble::IsUpdating() const
{
	return _updateCount > 0;
}




UpdateAbleOwned::UpdateAbleOwned(UpdateAble* owner): _owner(owner)
{
}

UpdateAble* UpdateAbleOwned::GetOwner()
{
	return _owner;
}

void UpdateAbleOwned::SetOwner(UpdateAble* value)
{
	_owner = value;
}

void UpdateAbleOwned::DoNotifyChanged(UpdateAble* sender, const UpdateData* data)
{
	if (_owner)
		_owner->NotifyChanged(sender, data);
}

}