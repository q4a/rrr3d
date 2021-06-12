#pragma once

#include "ISceneControl.h"

namespace r3d
{

namespace edit
{

class IWayPoint: public ExternInterf
{
public:
	virtual bool IsUsedByPath() const = 0;
	virtual unsigned GetId() const = 0;

	virtual float GetSize() const = 0;
	virtual void SetSize(float value) = 0;

	/*virtual const D3DXVECTOR3& GetPos() const;
	virtual void SetPos(const D3DXVECTOR3& value);

	virtual const D3DXVECTOR3& GetOff() const;
	virtual void SetOff(const D3DXVECTOR3& value);*/
};
typedef AutoRef<IWayPoint> IWayPointRef;

typedef AutoRef<class IWayPath> IWayPathRef;

class IWayNode: public ExternInterf
{
public:
	virtual IWayPathRef GetPath() = 0;
};
typedef AutoRef<IWayNode> IWayNodeRef;

class IWayPath: public ExternInterf
{
public:
	virtual void Delete(IWayNodeRef& value) = 0;
	virtual void Clear() = 0;

	virtual IWayNodeRef First() = 0;
	virtual void Next(IWayNodeRef& ref) = 0;
};

class ITrace: public ExternInterf
{
public:
	class ControlEvent: public Object
	{
	public:
		virtual void OnAddAndSelPoint(const IWayPointRef& ref) = 0;
		virtual void OnPathesUpdate() = 0;
	};
	typedef lsl::AutoRef<ControlEvent> ControlEventRef;
public:
	virtual IWayPointRef PickPoint(const lsl::Point& scrCoord) = 0;

	virtual IWayPointRef AddPoint() = 0;
	virtual void DelPoint(IWayPointRef& value) = 0;
	virtual void ClearPoints() = 0;
	virtual IScNodeContRef GetPointControl(const IWayPointRef& ref, const ControlEventRef& mEvent) = 0;

	virtual IWayPathRef AddPath() = 0;
	virtual void DelPath(IWayPathRef& value) = 0;
	virtual void ClearPathes() = 0;
	virtual IWayPathRef GetSelPath() = 0;
	virtual void SelectPath(const IWayPathRef& value) = 0;
	//
	virtual IWayNodeRef GetSelNode() = 0;
	virtual void SelectNode(const IWayNodeRef& value) = 0;

	virtual void Clear() = 0;

	virtual IWayPointRef FirstPoint() = 0;
	virtual void NextPoint(IWayPointRef& ref) = 0;

	virtual IWayPathRef FirstPath() = 0;
	virtual void NextPath(IWayPathRef& ref) = 0;

	virtual bool GetEnableVisualize() const = 0;
	virtual void EnableVisualize(bool value) = 0;
};

}

}