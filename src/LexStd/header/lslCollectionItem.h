#ifndef LSL_COLLECTION_ITEM
#define LSL_COLLECTION_ITEM

#include "lslCommon.h"
#include "lslComponent.h"
#include "lslUpdateAble.h"

namespace lsl
{

//A friend declaration does not introduce the name into the enclosing scope, so
//the members below cannot name the type without this.
class CollectionTraits;

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

	//Установление полей CollectionItem без уведомлений
	void SetItemTraits(CollectionItem* item, CollectionTraits* value);
	void SetItemName(CollectionItem* item, const std::string& name);
public:
	//Поиск по имени, это нужно для системы сериализации
	virtual CollectionItem* FindItem(const std::string& name) = 0;
	//Проверка допустимости имени
	virtual bool ValidateName(const std::string& name) = 0;
};

}

#endif