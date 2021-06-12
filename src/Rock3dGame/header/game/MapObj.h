#ifndef R3D_GAME_GAMEDB
#define R3D_GAME_GAMEDB

#include "RecordLib.h"

namespace r3d
{

namespace game
{

class GameObject;
class MapObj;
class Player;

enum GameObjType 
{
	gotGameObj = 0,

	gotGameCar,
	gotRockCar,
	gotProj,
	gotWeapon,
	gotDestrObj,

	cGameObjTypeEnd
};

static const char* cGameObjTypeStr[cGameObjTypeEnd] =
{
	"gotGameObj",
	"gotGameCar",
	"gotRockCar",
	"gotProj",
	"gotWeapon",
	"gotDestrObj"
};

class IMapObj
{
};

class IMapObjLib
{
public:
	enum Category {ctEffects = 0, ctDecoration, ctTrack, ctWeapon, ctCar, ctWaypoint, ctBonus, cCategoryEnd};
};

static const char* IMapObjLib_cCategoryStr[IMapObjLib::cCategoryEnd] = {"ctEffects", "ctDecoration", "ctTrack", "ctWeapon", "ctCar", "ctWaypoint", "ctBonus"};

class MapObjRec: public Record
{
	friend class MapObjLib;
private:
	typedef Record _MyBase;
public:
	typedef MapObjLib Lib;
protected:
	MapObjRec(const Desc& desc);
public:
	void Save(MapObj* mapObj);
	void Load(MapObj* mapObj);
	void AddProxy(MapObj* mapObj);

	MapObjLib* GetLib();
	IMapObjLib::Category GetCategory();
};

class MapObjLib: public RecordLib, public IMapObjLib
{
private:
	typedef RecordLib _MyBase;
public:
	static void SaveRecordRef(lsl::SWriter* writer, const std::string& name, MapObjRec* record);
	static MapObjRec* LoadRecordRefFrom(lsl::SReader* reader);
	static MapObjRec* LoadRecordRef(lsl::SReader* reader, const std::string& name);
private:
	Category _category;
protected:
	virtual Record* CreateRecord(const Record::Desc& desc);	
public:
	MapObjLib(Category category, lsl::SerialNode* rootSrc);

	void SaveToRec(MapObj* mapObj, MapObjRec* record);
	void LoadFromRec(MapObj* mapObj, MapObjRec* record);
	void AddProxyTo(MapObj* mapObj, MapObjRec* record);

	MapObjRec* FindRecord(const std::string& path);
	MapObjRec* GetOrCreateRecord(const std::string& name);

	Category GetCategory() const;
};

typedef RecordList<MapObjRec> MapObjRecList;

class MapObj: public lsl::Object
{
	friend class MapObjects;
	friend class MapObjLib;
public:
	typedef GameObject GameObj;
	typedef GameObjType Type;
	typedef lsl::ClassList<Type, GameObj> ClassList;

	static ClassList classList;

	static void InitClassList();
private:
	MapObjects* _owner;
	Player* _player;
	unsigned _id;

	GameObj* _gameObj;
	ClassList* _classList;
	bool _createClassList;
	Type _type;

	MapObjRec* _record;
	//Указывает что загрузка ведется из деск установленного в SetDesc, т.е. загружаться должна уже source часть
	bool _loadFromRecord;	

	void CreateGameObj();
protected:
	void SaveSource(lsl::SWriter* writer);
	void LoadSource(lsl::SReader* reader);
	void SaveProxy(lsl::SWriter* writer);
	void LoadProxy(lsl::SReader* reader);

	void Save(lsl::SWriter* writer);
	void Load(lsl::SReader* reader);
public:
	MapObj(MapObjects* owner = 0);
	virtual ~MapObj();

	void SaveTo(lsl::SWriter* writer);
	void LoadFrom(lsl::SReader* reader);

	ClassList* GetClassList();
	ClassList* GetOrCreateClassList();
	void SetClassList(ClassList* value);

	MapObjects* GetOwner();

	Type GetType() const;
	void SetType(Type value);

	GameObj& GetGameObj();
	const GameObj& GetGameObj() const;
	GameObj& SetGameObj(Type type);

	template<class _Class> _Class& GetGameObj();
	template<class _Class> _Class& SetGameObj();

	const std::string& GetName() const;
	void SetName(const std::string& value);

	GameObj* GetParent();
	void SetParent(GameObj* value);

	MapObjRec* GetRecord() const;
	void SetRecord(MapObjRec* value);

	Player* GetPlayer();
	void SetPlayer(Player* player);

	unsigned GetId() const;
	void SetId(unsigned value);
};

class MapObjects: public lsl::Collection<MapObj, void, MapObjects*, MapObjects*>
{
private:
	typedef lsl::BaseCollection<MapObj, void> _MyBase;
	typedef lsl::List<MapObj*> MapObjList;
private:
	lsl::Component* _root;
	GameObject* _owner;
	MapObjList _specialList;

	//заблокирвоать от операций изменяющих итераторы контейнера
	bool _lockCont;

	void LockCont();
	void UnlockCont();
	bool IsContLocked() const;

	void SpecialListChanged(const Value& value, bool remove);	
protected:
	virtual void InsertItem(const Value& value);
	virtual void RemoveItem(const Value& value);

	virtual void SaveItem(lsl::SWriter* writer, iterator pItem, const std::string& aName);
	virtual void LoadItem(lsl::SReader* reader);
public:
	MapObjects(lsl::Component* root);
	MapObjects(GameObject* owner);

	void Death(int damageType, GameObject* target);
	
	void OnProgress(float deltaTime);
	void OnProgressSpecial(float deltaTime);

	MapObj& Add(GameObjType type, const std::string& baseName = "obj");
	MapObj& Add(MapObjRec* record);
	template<class _Class> MapObj& Add(const std::string& baseName = "obj")
	{
		MapObj::InitClassList();

		MapObj::ClassList::MyClassInst* classInst = MapObj::classList.FindByClass<_Class>();
		if (!classInst)
			throw lsl::Error("MapObject& Add(const std::string& name)");
		
		return Add(classInst->GetKey(), baseName);
	}

	lsl::Component* GetRoot();
	GameObject* GetOwner();
	void SetStoreOpt(bool storeSource, bool storeProxy);

	using _MyBase::LockDestr;
	using _MyBase::UnlockDestr;
};




template<class _Class> _Class& MapObj::GetGameObj()
{
	LSL_ASSERT(_classList);

	if (!_classList->FindByClass<_Class>())
		throw lsl::Error("MapObj::GetGameObj()");

	return lsl::StaticCast<_Class&>(GetGameObj());
}

template<class _Class> _Class& MapObj::SetGameObj()
{
	LSL_ASSERT(_classList);
	
	ClassList::MyClassInst* classInst = _classList->FindByClass<_Class>();
	if (!classInst)
		throw lsl::Error("MapObject::SetGameObj()");
		
	return lsl::StaticCast<_Class&>(SetGameObj(classInst->GetKey()));
}

}

}

#endif