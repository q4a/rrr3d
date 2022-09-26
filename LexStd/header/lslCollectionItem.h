#ifndef LSL_COLLECTION_ITEM
#define LSL_COLLECTION_ITEM

#include "lslCommon.h"
#include "lslComponent.h"
#include "lslUpdateAble.h"

namespace lsl
{

class CollectionItem: public virtual Object
{
public:
	friend class CollectionTraits;
private:
	CollectionTraits* _collection;
	std::string _name;
public:
	CollectionItem();
	CollectionItem(const CollectionItem& ref);

	CollectionTraits* GetCollection();
	const CollectionTraits* GetCollection() const;

	const std::string& GetName() const;
	void SetName(const std::string& value);

	CollectionItem& operator=(const CollectionItem& ref);
};

class CollectionTraits: public lsl::Component
{
	friend CollectionItem;
protected:
	virtual void OnItemChangeName(CollectionItem* item, const std::string& newName) = 0;

	//”становление полей CollectionItem без уведомлений
	void SetItemTraits(CollectionItem* item, CollectionTraits* value);
	void SetItemName(CollectionItem* item, const std::string& name);
public:
	//ѕоиск по имени, это нужно дл€ системы сериализации
	virtual CollectionItem* FindItem(const std::string& name) = 0;
	//ѕроверка допустимости имени
	virtual bool ValidateName(const std::string& name) = 0;
};

}

#endif