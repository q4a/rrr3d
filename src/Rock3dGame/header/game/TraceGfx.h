#pragma once

#include "Trace.h"

namespace r3d
{

namespace game
{

class TraceGfx: public graph::BaseSceneNode
{
public:
	class PointLink: public Object
	{
	private:
		WayPoint* _point;
		D3DXVECTOR3 _pos;
	public:
		PointLink(WayPoint* point): _point(point) {LSL_ASSERT(_point); _point->AddRef();}
		~PointLink() {_point->Release();};

		WayPoint* GetPoint() {return _point;}
		const D3DXVECTOR3& GetPos() const {return _pos;}
		void SetPos(const D3DXVECTOR3& value) {_pos = value;}
	};
private:
	Trace* _trace;
	graph::Box* _wayPnt;
	graph::Sprite* _sprite;
	graph::LibMaterial* _libMat;

	WayPoint* _selPoint;
	WayPath* _selPath;
	WayNode* _selNode;
	PointLink* _pointLink;

	void DrawNodes(graph::Engine& engine, D3DXVECTOR3* vBuf, unsigned triCnt, const D3DXCOLOR& color);
protected:
	virtual void DoRender(graph::Engine& engine);
public:
	TraceGfx(Trace* trace);
	virtual ~TraceGfx();

	WayPoint* GetSelPoint();
	void SetSelPoint(WayPoint* value);

	WayPath* GetSelPath();
	void SetSelPath(WayPath* value);
	WayNode* GetSelNode();
	void SetSelNode(WayNode* value);

	PointLink* GetPointLink();
	void SetPointLink(PointLink* value);
};

}

}