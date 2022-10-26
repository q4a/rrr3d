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
		glm::vec3 _pos;
	public:
		PointLink(WayPoint* point): _point(point) {LSL_ASSERT(_point); _point->AddRef();}
		~PointLink() {_point->Release();};

		WayPoint* GetPoint() {return _point;}
		const glm::vec3& GetPos() const {return _pos;}
		void SetPos(const glm::vec3& value) {_pos = value;}
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

	void DrawNodes(graph::Engine& engine, glm::vec3* vBuf, unsigned triCnt, const D3DXCOLOR& color);
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