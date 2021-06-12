#pragma once

#include "ITrace.h"

#include "Utils.h"
#include "game\TraceGfx.h"

namespace r3d
{

namespace edit
{

class Edit;

class WayPoint: public IWayPoint, public ExternImpl<game::WayPoint>
{
protected:
	virtual VirtImpl* GetImpl() {return this;}
public:
	WayPoint(Inst* inst);

	bool IsUsedByPath() const;

	unsigned GetId() const;

	float GetSize() const;
	void SetSize(float value);

	game::Trace::Points::const_iterator traceIter;
};

class WayNode: public IWayNode, public ExternImpl<game::WayNode>
{
protected:
	virtual VirtImpl* GetImpl() {return this;}
public:
	WayNode(Inst* inst);

	IWayPathRef GetPath();
};

class WayPath: public IWayPath, public ExternImpl<game::WayPath>
{
protected:
	virtual VirtImpl* GetImpl() {return this;}
public:
	WayPath(Inst* inst);

	void Delete(IWayNodeRef& value);
	void Clear();

	IWayNodeRef First();
	void Next(IWayNodeRef& ref);

	game::Trace::Pathes::const_iterator traceIter;
};

class Trace: public ITrace, public ExternImpl<game::Trace>
{
private:
	class NodeControl: public IScNodeCont
	{
	private:
		Trace* _trace;
		game::WayPoint* _wayPoint;
		ControlEventRef _event;

		game::TraceGfx::PointLink* _link;
		D3DXVECTOR3 _dragRayPos;
		D3DXVECTOR3 _dragRayVec;
		
		void Reset(game::WayPoint* wayPoint);

		void NewLink(const D3DXVECTOR3& scrRayPos, const D3DXVECTOR3& scrRayVec);
		void FreeLink();
		bool CreateWay(game::WayNode* curNode, game::WayPoint* point, game::WayNode* node);
	public:
		NodeControl(Trace* trace, game::WayPoint* wayPoint, const ControlEventRef& mEvent);
		virtual ~NodeControl();

		void Select(bool active);
		bool RayCastInters(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec) const;
		bool Compare(const IMapObjRef& node) const;

		virtual void OnStartDrag(const D3DXVECTOR3& scrRayPos, const D3DXVECTOR3& scrRayVec);
		virtual void OnEndDrag(const D3DXVECTOR3& scrRayPos, const D3DXVECTOR3& scrRayVec);
		virtual void OnDrag(const D3DXVECTOR3& pos, const D3DXVECTOR3& scrRayPos, const D3DXVECTOR3& scrRayVec);
		//
		virtual void OnShiftAction(const D3DXVECTOR3& scrRayPos, const D3DXVECTOR3& scrRayVec);
		
		D3DXVECTOR3 GetPos() const;
		void SetPos(const D3DXVECTOR3& value);
		//
		D3DXQUATERNION GetRot() const;
		void SetRot(const D3DXQUATERNION& value);	
		//
		D3DXVECTOR3 GetScale() const;
		void SetScale(const D3DXVECTOR3& value);
		//
		D3DXVECTOR3 GetDir() const;
		D3DXVECTOR3 GetRight() const;
		D3DXVECTOR3 GetUp() const;

		D3DXMATRIX GetMat() const;
		AABB GetAABB() const;
	};	
private:
	Edit* _edit;
	game::TraceGfx* _traceGfx;
	
	IWayPathRef _selPath;
	IWayNodeRef _selNode;
	bool _enableVisualize;

	game::WayPoint* PickInstPoint(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec);
protected:
	virtual VirtImpl* GetImpl() {return this;}
public:
	Trace(Inst* inst, Edit* edit);
	virtual ~Trace();

	IWayPointRef PickPoint(const lsl::Point& scrCoord);

	IWayPointRef AddPoint();
	void DelPoint(IWayPointRef& value);
	void ClearPoints();
	IScNodeContRef GetPointControl(const IWayPointRef& ref, const ControlEventRef& mEvent);

	IWayPathRef AddPath();
	void DelPath(IWayPathRef& value);
	void ClearPathes();
	IWayPathRef GetSelPath();
	void SelectPath(const IWayPathRef& value);
	//
	IWayNodeRef GetSelNode();
	void SelectNode(const IWayNodeRef& value);

	void Clear();

	IWayPointRef FirstPoint();
	void NextPoint(IWayPointRef& ref);

	IWayPathRef FirstPath();
	void NextPath(IWayPathRef& ref);

	bool GetEnableVisualize() const;
	void EnableVisualize(bool value);
};

}

}