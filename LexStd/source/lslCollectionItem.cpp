#include "stdafx.h"

#include "lslCollectionItem.h"

namespace lsl
{

CollectionItem::CollectionItem(): _collection(0)
{
}

CollectionItem::CollectionItem(const CollectionItem& ref): _collection(0)
{
	*this = ref;
}

CollectionTraits* CollectionItem::GetCollection()
{
	return _collection;
}

const CollectionTraits* CollectionItem::GetCollection() const
{
	return _collection;
}

 const std::string& CollectionItem::GetName() const
{
	return _name;
}

void CollectionItem::SetName(const std::string& value)
{
	if (_name != value)
	{
		if (_collection)
		{
			if (!_collection->ValidateName(value))
				throw lsl::Error("Duplicate CollectionItem Name");

			_collection->OnItemChangeName(this, value);
		}
		_name = value;
	}
}

CollectionItem& CollectionItem::operator=(const CollectionItem& ref)
{
	SetName(ref.GetName());

	return *this;
}




void CollectionTraits::SetItemTraits(CollectionItem* item, CollectionTraits* value)
{
	item->_collection = value;
}

void CollectionTraits::SetItemName(CollectionItem* item, const std::string& name)
{
	item->_name = name;
}

}