#include "stdafx.h"
#include "game//MapObj.h"
#include "game//Player.h"

#include "game//GameCar.h"
#include "game//RockCar.h"
#include "game//Weapon.h"

#include "lslSerialFileXML.h"
#include <sstream>

namespace r3d
{
	namespace game
	{
		MapObj::ClassList MapObj::classList;


		MapObjRec::MapObjRec(const Desc& desc): _MyBase(desc)
		{
		}

		void MapObjRec::Save(MapObj* mapObj)
		{
			GetLib()->SaveToRec(mapObj, this);
		}

		void MapObjRec::Load(MapObj* mapObj)
		{
			GetLib()->LoadFromRec(mapObj, this);
		}

		void MapObjRec::AddProxy(MapObj* mapObj)
		{
			GetLib()->AddProxyTo(mapObj, this);
		}

		MapObjLib* MapObjRec::GetLib()
		{
			return dynamic_cast<MapObjLib*>(_MyBase::GetLib());
		}

		IMapObjLib::Category MapObjRec::GetCategory()
		{
			return GetLib()->GetCategory();
		}


		MapObjLib::MapObjLib(Category category, SerialNode* rootSrc): _MyBase(IMapObjLib_cCategoryStr[category],
		                                                                      rootSrc), _category(category)
		{
		}

		void MapObjLib::SaveRecordRef(SWriter* writer, const std::string& name, MapObjRec* record)
		{
			_MyBase::SaveRecordRef(writer, name, record);
		}

		MapObjRec* MapObjLib::LoadRecordRefFrom(SReader* reader)
		{
			return lsl::StaticCast<MapObjRec*>(_MyBase::LoadRecordRefFrom(reader));
		}

		MapObjRec* MapObjLib::LoadRecordRef(SReader* reader, const std::string& name)
		{
			return lsl::StaticCast<MapObjRec*>(_MyBase::LoadRecordRef(reader, name));
		}

		Record* MapObjLib::CreateRecord(const Record::Desc& desc)
		{
			return new MapObjRec(desc);
		}

		void MapObjLib::SaveToRec(MapObj* mapObj, MapObjRec* record)
		{
			record->GetSrc()->BeginSave();
			try
			{
				mapObj->Save(record->GetSrc());
			}
			LSL_FINALLY(record->GetSrc()->EndSave();)
		}

		void MapObjLib::LoadFromRec(MapObj* mapObj, MapObjRec* record)
		{
			record->GetSrc()->BeginLoad();
			try
			{
				mapObj->Load(record->GetSrc());
			}
			LSL_FINALLY(record->GetSrc()->EndLoad();)
		}

		void MapObjLib::AddProxyTo(MapObj* mapObj, MapObjRec* record)
		{
			mapObj->SetRecord(record);
		}

		MapObjRec* MapObjLib::FindRecord(const std::string& path)
		{
			return StaticCast<MapObjRec*>(_MyBase::FindRecord(path));
		}

		MapObjRec* MapObjLib::GetOrCreateRecord(const std::string& name)
		{
			return dynamic_cast<MapObjRec*>(_MyBase::GetOrCreateRecord(name));
		}

		MapObjLib::Category MapObjLib::GetCategory() const
		{
			return _category;
		}


		MapObj::MapObj(MapObjects* owner): _owner(owner), _player(nullptr), _id(0), _gameObj(nullptr),
		                                   _classList(nullptr), _createClassList(false), _type(static_cast<Type>(0)),
		                                   _record(nullptr), _loadFromRecord(false)
		{
			InitClassList();

			SetClassList(&classList);
			CreateGameObj();
		}

		MapObj::~MapObj()
		{
			SetPlayer(nullptr);
			SetRecord(nullptr);
			SetClassList(nullptr);

			delete _gameObj;
		}

		void MapObj::InitClassList()
		{
			static bool initClassList = false;
			if (!initClassList)
			{
				initClassList = true;

				classList.Add<GameObject>(gotGameObj);

				classList.Add<GameCar>(gotGameCar);
				classList.Add<RockCar>(gotRockCar);
				classList.Add<AutoProj>(gotProj);
				classList.Add<Weapon>(gotWeapon);
				classList.Add<DestrObj>(gotDestrObj);
			}
		}

		void MapObj::CreateGameObj()
		{
			if (_classList)
			{
				GameObj* gameObj = _classList->CreateInst(_type);

				const std::string name;
				GameObj* parent = nullptr;
				Component* compParent = nullptr;
				if (_gameObj)
				{
					gameObj->Assign(_gameObj);
					delete _gameObj;
				}

				_gameObj = gameObj;
				_gameObj->_mapObj = this;
				_gameObj->SetName(name);
				_gameObj->SetParent(parent);
				_gameObj->SetOwner(compParent);
			}
			else
				SafeDelete(_gameObj);
		}

		void MapObj::SaveSource(SWriter* writer) const
		{
			writer->WriteAttr("type", cGameObjTypeStr[_type]);

			_gameObj->storeSource = true;
			_gameObj->storeProxy = true;
			writer->SaveSerializable(_gameObj);
		}

		void MapObj::LoadSource(SReader* reader)
		{
			GameObjType type = gotGameObj;
			std::string typeStr;
			reader->ReadAttr("type")->CastTo<std::string>(&typeStr);
			for (int i = 0; i < cGameObjTypeEnd; ++i)
				if (strcmp(cGameObjTypeStr[i], typeStr.c_str()) == 0)
				{
					type = static_cast<GameObjType>(i);
					break;
				}
			SetGameObj(type);

			_gameObj->storeSource = true;
			_gameObj->storeProxy = true;
			reader->LoadSerializable(_gameObj);
		}

		void MapObj::SaveProxy(SWriter* writer) const
		{
			_gameObj->storeSource = false;
			_gameObj->storeProxy = true;
			_gameObj->SaveProxy(writer);
		}

		void MapObj::LoadProxy(SReader* reader) const
		{
			_gameObj->storeSource = false;
			_gameObj->storeProxy = true;
			_gameObj->LoadProxy(reader);
		}

		void MapObj::Save(SWriter* writer) const
		{
			if (_record)
			{
				MapObjRec::Lib::SaveRecordRef(writer, "record", _record);
				SaveProxy(writer);
			}
			else
				SaveSource(writer);
		}

		void MapObj::Load(SReader* reader)
		{
			if (!_loadFromRecord)
				SetRecord(nullptr);

			if (MapObjRec* record = MapObjRec::Lib::LoadRecordRef(reader, "record"))
			{
				SetRecord(record);
				LoadProxy(reader);
			}
			else
				LoadSource(reader);
		}

		void MapObj::SaveTo(SWriter* writer) const
		{
			Save(writer);
		}

		void MapObj::LoadFrom(SReader* reader)
		{
			Load(reader);
		}

		MapObj::ClassList* MapObj::GetClassList() const
		{
			return _classList;
		}

		MapObj::ClassList* MapObj::GetOrCreateClassList()
		{
			if (!_classList)
			{
				_classList = new ClassList();
				_classList->AddRef();
				_createClassList = true;
				CreateGameObj();
			}
			return _classList;
		}

		void MapObj::SetClassList(ClassList* value)
		{
			if (ReplaceRef(_classList, value))
			{
				if (_createClassList)
				{
					delete _classList;
					_createClassList = false;
				}
				_classList = value;
				CreateGameObj();
			}
		}

		MapObjects* MapObj::GetOwner() const
		{
			return _owner;
		}

		MapObj::Type MapObj::GetType() const
		{
			return _type;
		}

		void MapObj::SetType(Type value)
		{
			if (_type != value)
			{
				_type = value;
				CreateGameObj();
			}
		}

		MapObj::GameObj& MapObj::GetGameObj()
		{
			return *_gameObj;
		}

		const MapObj::GameObj& MapObj::GetGameObj() const
		{
			return *_gameObj;
		}

		MapObj::GameObj& MapObj::SetGameObj(Type type)
		{
			SetType(type);

			return *_gameObj;
		}

		const std::string& MapObj::GetName() const
		{
			return GetGameObj().GetName();
		}

		void MapObj::SetName(const std::string& value)
		{
			GetGameObj().SetName(value);
		}

		GameObject* MapObj::GetParent()
		{
			return GetGameObj().GetParent();
		}

		void MapObj::SetParent(GameObj* value)
		{
			GetGameObj().SetParent(value);
		}

		MapObjRec* MapObj::GetRecord() const
		{
			return _record;
		}

		void MapObj::SetRecord(MapObjRec* value)
		{
			if (ReplaceRef(_record, value))
			{
				_record = value;

				if (_record)
					try
					{
						_loadFromRecord = true;
						_record->Load(this);
					}
					LSL_FINALLY(_loadFromRecord = false;)
			}
		}

		Player* MapObj::GetPlayer() const
		{
			return _player;
		}

		void MapObj::SetPlayer(Player* player)
		{
			if (ReplaceRef(_player, player))
				_player = player;
		}

		unsigned MapObj::GetId() const
		{
			return _id;
		}

		void MapObj::SetId(unsigned value)
		{
			_id = value;
		}


		MapObjects::MapObjects(Component* root): _root(root), _owner(nullptr), _lockCont(false)
		{
			LSL_ASSERT(_root);
		}

		MapObjects::MapObjects(GameObject* owner): _root(owner), _owner(owner), _lockCont(false)
		{
			LSL_ASSERT(_root);
		}

		void MapObjects::LockCont()
		{
			LSL_ASSERT(!_lockCont);

			_lockCont = true;
		}

		void MapObjects::UnlockCont()
		{
			LSL_ASSERT(_lockCont);

			_lockCont = false;
		}

		bool MapObjects::IsContLocked() const
		{
			return _lockCont;
		}

		void MapObjects::SpecialListChanged(const Value& value, bool remove)
		{
			MapObjRec* record = value->GetRecord();
			const RecordNode* recordParent = record ? record->GetParent() : nullptr;

			if (recordParent != nullptr && record->GetCategory() == MapObjLib::ctDecoration && (recordParent->GetName()
				== "Misc" || recordParent->GetName() == "Crush"))
			{
				if (remove)
					_specialList.Remove(value);
				else
					_specialList.push_back(value);
			}
		}

		void MapObjects::InsertItem(const Value& value)
		{
			_MyBase::InsertItem(value);

			value->GetGameObj().storeName = false;
			value->GetGameObj().SetOwner(_root);
			value->SetParent(_owner);

			SpecialListChanged(value, false);
		}

		void MapObjects::RemoveItem(const Value& value)
		{
			LSL_ASSERT(!IsContLocked());

			SpecialListChanged(value, true);

			_MyBase::RemoveItem(value);
		}

		void MapObjects::SaveItem(SWriter* writer, iterator pItem, const std::string& aName)
		{
			SWriter* child = writer->NewDummyNode((*pItem)->GetName().c_str());
			//ѕереходим напр€мую к методам сохранени€ поскольку mapObj €вл€етс€ промежуточным звеном и только делегирует вызовы
			(*pItem)->Save(child);
		}

		void MapObjects::LoadItem(SReader* reader)
		{
			//ѕереходим напр€мую к методам загрузки поскольку mapObj €вл€етс€ промежуточным звеном и только делегирует вызовы
			MapObj* mapObj = CreateItem();
			mapObj->Load(reader);
			mapObj->SetName(reader->GetMyName());

			_MyBase::Add(mapObj);
		}

		void MapObjects::Death(int damageType, GameObject* target)
		{
			for (Position pos = First(); const Value* iter = Current(pos); Next(pos))
				(*iter)->GetGameObj().Death(static_cast<GameObject::DamageType>(damageType), target);
		}

		void MapObjects::OnProgress(float deltaTime)
		{
			for (auto iter = begin(); iter != end();)
			{
				//¬начале прогресс дл€ всех объектов, в том числе и умерших, поскольку они могут выполн€ть какие нибудь действи€. Ќапример помещать умерших потомков в список смертей
				LockCont();
				try
				{
					(*iter)->GetGameObj().OnProgress(deltaTime);
				}
				LSL_FINALLY(UnlockCont();)

				if ((*iter)->GetGameObj().GetLiveState() == GameObject::lsDeath)
				{
					iter = Delete(iter);
				}
				else
				{
					++iter;
				}
			}
		}

		void MapObjects::OnProgressSpecial(float deltaTime)
		{
			for (auto iter = _specialList.begin(); iter != _specialList.end();)
			{
				MapObj* mapObj = *iter;

				//¬начале прогресс дл€ всех объектов, в том числе и умерших, поскольку они могут выполн€ть какие нибудь действи€. Ќапример помещать умерших потомков в список смертей
				LockCont();
				try
				{
					mapObj->GetGameObj().OnProgress(deltaTime);
				}
				LSL_FINALLY(UnlockCont();)

				if (mapObj->GetGameObj().GetLiveState() == GameObject::lsDeath)
				{
					iter = _specialList.erase(iter);
					Delete(mapObj);
				}
				else
				{
					++iter;
				}
			}
		}

		MapObj& MapObjects::Add(GameObjType type, const std::string& baseName)
		{
			MapObj* mapObj = CreateItem();
			mapObj->SetType(type);
			mapObj->SetName(_root->MakeUniqueName(baseName));

			return _MyBase::Add(mapObj);
		}

		MapObj& MapObjects::Add(MapObjRec* record)
		{
			MapObj* mapObj = CreateItem();
			record->AddProxy(mapObj);
			mapObj->SetName(_root->MakeUniqueName(record->GetName()));

			return _MyBase::Add(mapObj);
		}

		Component* MapObjects::GetRoot() const
		{
			return _root;
		}

		GameObject* MapObjects::GetOwner() const
		{
			return _owner;
		}

		void MapObjects::SetStoreOpt(bool storeSource, bool storeProxy)
		{
			for (const auto& iter : *this)
			{
				iter->GetGameObj().storeSource = storeSource;
				iter->GetGameObj().storeProxy = storeProxy;
			}
		}
	}
}
