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
		glm::vec3 _dragRayPos;
		glm::vec3 _dragRayVec;

		void Reset(game::WayPoint* wayPoint);

		void NewLink(const glm::vec3& scrRayPos, const glm::vec3& scrRayVec);
		void FreeLink();
		bool CreateWay(game::WayNode* curNode, game::WayPoint* point, game::WayNode* node);
	public:
		NodeControl(Trace* trace, game::WayPoint* wayPoint, const ControlEventRef& mEvent);
		virtual ~NodeControl();

		void Select(bool active);
		bool RayCastInters(const glm::vec3& rayPos, const glm::vec3& rayVec) const;
		bool Compare(const IMapObjRef& node) const;

		virtual void OnStartDrag(const glm::vec3& scrRayPos, const glm::vec3& scrRayVec);
		virtual void OnEndDrag(const glm::vec3& scrRayPos, const glm::vec3& scrRayVec);
		virtual void OnDrag(const glm::vec3& pos, const glm::vec3& scrRayPos, const glm::vec3& scrRayVec);
		//
		virtual void OnShiftAction(const glm::vec3& scrRayPos, const glm::vec3& scrRayVec);

		glm::vec3 GetPos() const;
		void SetPos(const glm::vec3& value);
		//
		glm::quat GetRot() const;
		void SetRot(const glm::quat& value);
		//
		glm::vec3 GetScale() const;
		void SetScale(const glm::vec3& value);
		//
		glm::vec3 GetDir() const;
		glm::vec3 GetRight() const;
		glm::vec3 GetUp() const;

		D3DXMATRIX GetMat() const;
		AABB GetAABB() const;
	};
private:
	Edit* _edit;
	game::TraceGfx* _traceGfx;

	IWayPathRef _selPath;
	IWayNodeRef _selNode;
	bool _enableVisualize;

	game::WayPoint* PickInstPoint(const glm::vec3& rayPos, const glm::vec3& rayVec);
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