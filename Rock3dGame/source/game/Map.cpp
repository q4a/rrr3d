#include "stdafx.h"
#include "game//World.h"

#include "game//Map.h"

namespace r3d
{
	namespace game
	{
		Map::MapObjList::MapObjList(Map* owner): _MyBase(owner), _owner(owner)
		{
		}

		void Map::MapObjList::InsertItem(const Value& value)
		{
			_MyBase::InsertItem(value);

			//���� GameObject ������ �������� ����� ���������� �� �����!
			value->GetGameObj().AddRef();
			value->GetGameObj().SetLogic(_owner->_world->GetLogic());

			unsigned id = ++(_owner->_lastId);
			value->SetId(id);
			_owner->_objects.insert(Objects::value_type(id, value));
		}

		void Map::MapObjList::RemoveItem(const Value& value)
		{
			_MyBase::RemoveItem(value);

			value->GetGameObj().Release();
			_owner->_objects.erase(value->GetId());
		}


		Map::Map(World* world): _world(world), _lastId(cDefMapObjId)
		{
			for (int i = 0; i < MapObjLib::cCategoryEnd; ++i)
				_mapObjList[i] = new MapObjList(this);

			_ground = new MapObj();
			TouchDeath& touchDeath = _ground->GetGameObj().GetBehaviors().Add<TouchDeath>();

			px::PlaneShape& shape = _ground->GetGameObj().GetPxActor().GetShapes().Add<px::PlaneShape>();
			shape.SetNormal(ZVector);
			shape.SetDist(0.0f);
			shape.SetGroup(px::Scene::cdgPlaneDeath);
			_ground->GetGameObj().GetPxActor().SetScene(_world->GetPxScene());

			_trace = new Trace(4);
		}

		Map::~Map()
		{
			Clear();

			delete _trace;

			delete _ground;
			for (int i = 0; i < MapObjLib::cCategoryEnd; ++i)
				delete _mapObjList[i];
		}

		void Map::Save(SWriter* writer)
		{
			for (int i = 0; i < MapObjLib::cCategoryEnd; ++i)
				writer->WriteValue(IMapObjLib_cCategoryStr[i], _mapObjList[i]);

			writer->WriteValue("trace", _trace);

			SWriteValue(writer, "sunPos", _world->GetEnv()->GetSunPos());
			SWriteValue(writer, "sunRot", _world->GetEnv()->GetSunRot());
		}

		void Map::Load(SReader* reader)
		{
			Clear();

			for (int i = 0; i < MapObjLib::cCategoryEnd; ++i)
				reader->ReadValue(IMapObjLib_cCategoryStr[i], _mapObjList[i]);

			reader->ReadValue("trace", _trace);

			D3DXVECTOR3 sunPos;
			if (SReadValue(reader, "sunPos", sunPos))
				_world->GetEnv()->SetSunPos(sunPos);

			D3DXQUATERNION sunRot;
			if (SReadValue(reader, "sunRot", sunRot))
				_world->GetEnv()->SetSunRot(sunRot);
		}

		void Map::InsertMapObj(MapObj* value)
		{
			MapObjLib::Category category = value->GetRecord()
				                               ? value->GetRecord()->GetCategory()
				                               : MapObjLib::ctDecoration;

			_mapObjList[category]->AddItem(value);
		}

		MapObj& Map::AddMapObj(MapObjRec* record)
		{
			MapObj& mapObj = _mapObjList[record->GetCategory()]->Add(record);
			return mapObj;
		}

		MapObj& Map::AddMapObj(MapObj* ref)
		{
			LSL_ASSERT(ref && ref->GetRecord());

			MapObj& newMapObj = AddMapObj(ref->GetRecord()->GetCategory(), ref->GetType());

			auto serNode = new RootNode("tmp", GetRoot());

			SWriter* writer = serNode->BeginSave();
			ref->SaveTo(writer);
			serNode->EndSave();

			SReader* reader = serNode->BeginLoad();
			newMapObj.LoadFrom(reader);
			newMapObj.SetName(MakeUniqueName(ref->GetRecord()->GetName()));
			serNode->EndLoad();

			delete serNode;

			return newMapObj;
		}

		MapObj& Map::AddMapObj(MapObjLib::Category category, MapObj::Type type)
		{
			MapObj& mapObj = _mapObjList[category]->Add(type);
			return mapObj;
		}

		void Map::DelMapObj(MapObj* value)
		{
			for (int i = 0; i < MapObjLib::cCategoryEnd; ++i)
			{
				auto iter = _mapObjList[i]->Find(value);
				if (iter != _mapObjList[i]->end())
				{
					_mapObjList[i]->Delete(iter);
					return;
				}
			}
		}

		void Map::Clear()
		{
			_lastId = cDefMapObjId;
			for (int i = 0; i < MapObjLib::cCategoryEnd; ++i)
				_mapObjList[i]->Clear();
		}

		int Map::GetMapObjCount(MapObjRec* record)
		{
			MapObjLib::Category category = record->GetCategory();
			MapObjList* mapObjList = _mapObjList[category];
			int count = 0;

			for (MapObjList::const_iterator iter = mapObjList->begin(); iter != mapObjList->end(); ++iter)
			{
				if ((*iter)->GetRecord() == record)
					++count;
			}

			return count;
		}

		const Map::Objects& Map::GetObjects() const
		{
			return _objects;
		}

		Map::MapObjList& Map::GetMapObjList(MapObjLib::Category type)
		{
			return *_mapObjList[type];
		}

		MapObj* Map::GetMapObj(unsigned id, bool includeDead)
		{
			if (id == cDefMapObjId)
				return nullptr;

			Objects::const_iterator iter = _objects.find(id);
			if (iter != _objects.end() && (includeDead || iter->second->GetGameObj().GetLiveState() !=
				GameObject::lsDeath))
				return iter->second;

			return nullptr;
		}

		MapObj* Map::GetSemaphore()
		{
			for (MapObjList::const_iterator iter = _mapObjList[MapObjLib::ctDecoration]->begin(); iter != _mapObjList[
				     MapObjLib::ctDecoration]->end(); ++iter)
				if ((*iter)->GetRecord() && (*iter)->GetRecord()->GetName() == "semaphore")
					return *iter;

			return nullptr;
		}

		World* Map::GetWorld()
		{
			return _world;
		}

		Trace& Map::GetTrace()
		{
			return *_trace;
		}
	}
}
