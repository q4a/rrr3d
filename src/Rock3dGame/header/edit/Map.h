#pragma once

#include "IMap.h"
#include "Utils.h"

namespace r3d
{

namespace edit
{

class Edit;

class MapObj: public IMapObj, public ExternImpl<game::MapObj>
{
protected:
	virtual VirtImpl* GetImpl() {return this;}
public:
	MapObj(Inst* inst);

	const std::string& GetName() const;
	IMapObjRecRef MapObj::GetRecord();

	D3DXVECTOR3 GetPos() const;
	void SetPos(const D3DXVECTOR3& value);

	D3DXVECTOR3 GetScale() const;
	void SetScale(const D3DXVECTOR3& value);

	D3DXQUATERNION GetRot() const;
	void SetRot(const D3DXQUATERNION& value);

	game::Map::MapObjList::const_iterator mapIter;
};

class Map: public IMap
{
	friend Edit;
private:
	static const D3DXCOLOR bbColor;
	static const D3DXCOLOR selColor;

	typedef lsl::List<game::MapObj*> MapObjList;

	class NodeControl: public ScNodeCont
	{
		typedef ScNodeCont _MyBase;
	private:
		Map* _map;
		game::MapObj* _mapObj;
		ControlEventRef _event;
	public:
		NodeControl(game::MapObj* mapObj, Map* map, const ControlEventRef& mEvent);
		virtual ~NodeControl();

		void Select(bool active);
		void OnShiftAction(const D3DXVECTOR3& scrRayPos, const D3DXVECTOR3& scrRayVec);
	};
public:
	typedef game::Map Inst;	
private:
	Edit* _edit;
	ITrace* _trace;
	bool _showBB;

	game::MapObj* PickInstMapObj(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec);
	
	void ApplyShowBB();
	void OnUpdateLevel();

	Inst* GetInst();
public:
	Map(Edit* edit);
	virtual ~Map();

	IMapObjRef AddMapObj(const IMapObjRecRef& record);
	void DelMapObj(IMapObjRef& ref);
	void ClearMap();
	IMapObjRef PickMapObj(const lsl::Point& coord);
	IScNodeContRef GetMapObjControl(const IMapObjRef& ref, const ControlEventRef& mEvent);

	std::string GetCatName(unsigned i) const;
	unsigned GetCatCount();
	//
	IMapObjRef GetFirst(unsigned cat);
	void GetNext(unsigned cat, IMapObjRef& ref);	

	bool GetShowBBox() const;
	void SetShowBBox(bool value);

	ITrace* GetTrace();
};

}

}