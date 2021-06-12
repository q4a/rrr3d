#pragma once

#include "ITrace.h"
#include "IMapObj.h"
#include "ISceneControl.h"

namespace r3d
{

namespace edit
{

class IMap: public Object
{
public:
	class ControlEvent: public Object
	{
	public:
		virtual void OnAddAndSelMapObj(const IMapObjRef& mapObj) = 0;
	};
	typedef lsl::AutoRef<ControlEvent> ControlEventRef;
public:
	virtual IMapObjRef AddMapObj(const IMapObjRecRef& record) = 0;
	virtual void DelMapObj(IMapObjRef& ref) = 0;
	virtual void ClearMap() = 0;
	virtual IMapObjRef PickMapObj(const lsl::Point& coord) = 0;
	virtual IScNodeContRef GetMapObjControl(const IMapObjRef& ref, const ControlEventRef& mEvent) = 0;

	virtual std::string GetCatName(unsigned i) const = 0;
	virtual unsigned GetCatCount() = 0;
	//
	virtual IMapObjRef GetFirst(unsigned cat) = 0;
	virtual void GetNext(unsigned cat, IMapObjRef& ref) = 0;

	virtual bool GetShowBBox() const = 0;
	virtual void SetShowBBox(bool value) = 0;

	virtual ITrace* GetTrace() = 0;
};

}

}