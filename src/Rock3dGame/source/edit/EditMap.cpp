#include "stdafx.h"
#include "game\World.h"

#include "edit\Map.h"
#include "edit\Edit.h"

#include "edit\Trace.h"


namespace r3d
{

namespace edit
{

const D3DXCOLOR Map::bbColor(clrRed);
const D3DXCOLOR Map::selColor(clrGreen);


			
	
MapObj::MapObj(Inst* inst): ExternImpl<Inst>(inst)
{
}

const std::string& MapObj::GetName() const
{
	return GetInst()->GetName();
}

D3DXVECTOR3 MapObj::GetPos() const
{
	return GetInst()->GetGameObj().GetPos();
}

void MapObj::SetPos(const D3DXVECTOR3& value)
{
	 GetInst()->GetGameObj().SetPos(value);
}

D3DXVECTOR3 MapObj::GetScale() const
{
	return GetInst()->GetGameObj().GetScale();
}

void MapObj::SetScale(const D3DXVECTOR3& value)
{
	GetInst()->GetGameObj().SetScale(value);
}

D3DXQUATERNION MapObj::GetRot() const
{
	return GetInst()->GetGameObj().GetRot();
}

void MapObj::SetRot(const D3DXQUATERNION& value)
{
	GetInst()->GetGameObj().SetRot(value);
}

IMapObjRecRef MapObj::GetRecord()
{
	game::MapObjRec* rec = GetInst()->GetRecord();
	MapObjRec* ext = rec ? new MapObjRec(rec) : 0;

	return IMapObjRecRef(ext, ext);
}




Map::Map(Edit* edit): _edit(edit), _showBB(true)
{
	_trace = new Trace(&GetInst()->GetTrace(), edit);

	ApplyShowBB();
}

Map::~Map()
{
	delete _trace;	
}

Map::NodeControl::NodeControl(game::MapObj* mapObj, Map* map, const ControlEventRef& mEvent): _MyBase(&mapObj->GetGameObj().GetGrActor()), _mapObj(mapObj), _map(map), _event(mEvent)
{	
}

Map::NodeControl::~NodeControl()
{	
}

void Map::NodeControl::Select(bool active)
{
	GetNode()->showBB = active || _map->_showBB;
	GetNode()->colorBB = active ? Map::selColor : Map::bbColor;
}

void Map::NodeControl::OnShiftAction(const D3DXVECTOR3& scrRayPos, const D3DXVECTOR3& scrRayVec)
{
	Select(false);
	_mapObj = &_map->GetInst()->AddMapObj(_mapObj);
	Reset(&_mapObj->GetGameObj().GetGrActor());
	Select(true);

	if (_event)	
	{
		MapObj* mapObj = new MapObj(_mapObj);
		_event->OnAddAndSelMapObj(IMapObjRef(mapObj, mapObj));		
	}
}

game::MapObj* Map::PickInstMapObj(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec)
{
	float minDist = 0;
	game::MapObj* mapObj = 0;	
	
	for (Inst::Objects::const_iterator iter = GetInst()->GetObjects().begin(); iter != GetInst()->GetObjects().end(); ++iter)
	{
		D3DXVECTOR3 nearVec;
		D3DXVECTOR3 farVec;
		game::MapObj* item = iter->second;

		if (item->GetGameObj().GetGrActor().RayCastIntersBB(rayPos, rayVec, nearVec, farVec, true))
		{
			float dist = D3DXVec3Length(&(rayPos - nearVec));
			if (minDist > dist || mapObj == 0)
			{
				mapObj = item;
				minDist = dist;
			}
		}
	}

	return mapObj;
}

void Map::ApplyShowBB()
{
	IScNodeContRef selNode = _edit->GetScControl()->GetSelNode();

	for (Inst::Objects::const_iterator iter = GetInst()->GetObjects().begin(); iter != GetInst()->GetObjects().end(); ++iter)
	{
		game::MapObj* mapObj = iter->second;

		//Объект не принадлежит к выделеному
		if (!selNode || !selNode->Compare(IMapObjRef(new MapObj(mapObj), this)))
		{
			mapObj->GetGameObj().GetGrActor().showBB = _showBB;
			mapObj->GetGameObj().GetGrActor().showBBIncludeChild = true;
			mapObj->GetGameObj().GetGrActor().colorBB = bbColor;
		}
	}
}

void Map::OnUpdateLevel()
{
	ApplyShowBB();
}

Map::Inst* Map::GetInst()
{
	return _edit->GetWorld()->GetMap();
}

IMapObjRef Map::AddMapObj(const IMapObjRecRef& record)
{
	game::MapObj* mapObj = &GetInst()->AddMapObj(record->GetImpl<MapObjRec>()->GetInst());
	mapObj->GetGameObj().GetGrActor().showBB = _showBB;
	mapObj->GetGameObj().GetGrActor().showBBIncludeChild = true;
	mapObj->GetGameObj().GetGrActor().colorBB = bbColor;

	return IMapObjRef(new MapObj(mapObj), this);
}

void Map::DelMapObj(IMapObjRef& ref)
{
	game::MapObj* mapObj = ref->GetImpl<MapObj>()->GetInst();
	mapObj->GetGameObj().GetGrActor().showBB = false;

	ref.Release();
	GetInst()->DelMapObj(mapObj);
}

void Map::ClearMap()
{
	GetInst()->Clear();
}

IMapObjRef Map::PickMapObj(const lsl::Point& coord)
{
	D3DXVECTOR3 scrRayPos;
	D3DXVECTOR3 scrRayVec;
	_edit->GetWorld()->GetCamera()->ScreenToRay(coord, scrRayPos, scrRayVec);
	game::MapObj* mapObj = PickInstMapObj(scrRayPos, scrRayVec);

	return IMapObjRef(mapObj ? new MapObj(mapObj) : 0, this);
}

IScNodeContRef Map::GetMapObjControl(const IMapObjRef& ref, const ControlEventRef& mEvent)
{
	LSL_ASSERT(ref);

	return IScNodeContRef(new NodeControl(ref->GetImpl<MapObj>()->GetInst(), this, mEvent), this);
}

std::string Map::GetCatName(unsigned i) const
{
	return game::IMapObjLib_cCategoryStr[i];
}

unsigned Map::GetCatCount()
{
	return game::MapObjLib::cCategoryEnd;
}

IMapObjRef Map::GetFirst(unsigned i)
{
	Inst::MapObjList& list = GetInst()->GetMapObjList(game::MapObjLib::Category(i));

	Inst::MapObjList::const_iterator iter = list.begin();
	MapObj* mapObj = 0;
	if (iter != list.end())
	{
		mapObj = new MapObj(*iter);
		mapObj->mapIter = iter;
	}

	return IMapObjRef(mapObj, this);
}

void Map::GetNext(unsigned cat, IMapObjRef& ref)
{
	MapObj* mapObj = ref->GetImpl<MapObj>();	
	
	if (++(mapObj->mapIter) != GetInst()->GetMapObjList(game::MapObjLib::Category(cat)).end())
	{
		MapObj* newMapObj = new MapObj(*mapObj->mapIter);
		newMapObj->mapIter = mapObj->mapIter;
		ref.Reset(newMapObj, this);
	}
	else
		ref.Release();
}

bool Map::GetShowBBox() const
{
	return _showBB;
}

void Map::SetShowBBox(bool value)
{
	if (_showBB != value)
	{
		_showBB = value;
		ApplyShowBB();
	}
}

ITrace* Map::GetTrace()
{
	return _trace;
}

}

}