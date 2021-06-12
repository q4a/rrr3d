#include "stdafx.h"
#include "game\World.h"

#include "edit\Trace.h"
#include "edit\Edit.h"


namespace r3d
{

namespace edit
{

WayPoint::WayPoint(Inst* inst): ExternImpl<game::WayPoint>(inst)
{
}

bool WayPoint::IsUsedByPath() const
{
	return !GetInst()->GetNodes().empty();
}

unsigned WayPoint::GetId() const
{
	return GetInst()->GetId();
}

float WayPoint::GetSize() const
{
	return GetInst()->GetSize();
}

void WayPoint::SetSize(float value)
{
	GetInst()->SetSize(value);
}




WayNode::WayNode(Inst* inst): ExternImpl<game::WayNode>(inst)
{
}

IWayPathRef WayNode::GetPath()
{
	WayPath* path = GetInst()->GetPath() ? new WayPath(GetInst()->GetPath()) : 0;
	return IWayPathRef(path, path);
}




WayPath::WayPath(Inst* inst): ExternImpl<game::WayPath>(inst)
{
}

void WayPath::Delete(IWayNodeRef& value)
{
	game::WayNode* node = value->GetImpl<WayNode>()->GetInst();
	value.Release();
	GetInst()->Delete(node);
}

void WayPath::Clear()
{
	GetInst()->Clear();
}

IWayNodeRef WayPath::First()
{
	WayNode* node = GetInst()->GetFirst() ? new WayNode(GetInst()->GetFirst()) : 0;
	return IWayNodeRef(node, node);
}

void WayPath::Next(IWayNodeRef& ref)
{
	game::WayNode* next = ref ? ref->GetImpl<WayNode>()->GetInst()->GetNext() : 0;
	WayNode* node = next ? new WayNode(next) : 0;
	ref.Reset(node, node);
}




Trace::Trace(Inst* inst, Edit* edit): ExternImpl<game::Trace>(inst), _edit(edit), _enableVisualize(false)
{
	game::Trace& trace = *GetInst();
	_traceGfx = new game::TraceGfx(&trace);
}

Trace::~Trace()
{
	SelectNode(0);
	SelectPath(0);
	GetInst()->ClearPathes();

	EnableVisualize(false);
	delete _traceGfx;
}

Trace::NodeControl::NodeControl(Trace* trace, game::WayPoint* wayPoint, const ControlEventRef& mEvent): _trace(trace), _wayPoint(wayPoint), _link(0), _dragRayPos(NullVector), _dragRayVec(NullVector), _event(mEvent)
{
	LSL_ASSERT(_wayPoint);

	_wayPoint->AddRef();
}
	
Trace::NodeControl::~NodeControl()
{
	FreeLink();

	_wayPoint->Release();
}

void Trace::NodeControl::Reset(game::WayPoint* wayPoint)
{
	if (ReplaceRef(_wayPoint, wayPoint))
		_wayPoint = wayPoint;

	LSL_ASSERT(_wayPoint);
}

void Trace::NodeControl::NewLink(const D3DXVECTOR3& scrRayPos, const D3DXVECTOR3& scrRayVec)
{
	FreeLink();

	_link = new game::TraceGfx::PointLink(_wayPoint);
	_trace->_traceGfx->SetPointLink(_link);
}

void Trace::NodeControl::FreeLink()
{
	if (_link)
	{
		if (_trace->_traceGfx->GetPointLink() == _link)
			_trace->_traceGfx->SetPointLink(0);
		lsl::SafeDelete(_link);
	}
	
}

bool Trace::NodeControl::CreateWay(game::WayNode* curNode, game::WayPoint* point, game::WayNode* node)
{
	//Первый случай, существует текущий узел
	if (curNode && point && curNode->GetPoint() != point)
	{
		game::WayPath* path = curNode->GetPath();
		if (!curNode->GetNext())
		{
			path->Add(point, 0);
			return true;
		}
		if (!curNode->GetPrev())
		{
			path->Add(point, curNode);
			return true;
		}
	}
	//
	//Второй случай, существует выделенный узел
	if (node && node->GetPoint() != _wayPoint)
	{
		game::WayPath* path = node->GetPath();
		if (!node->GetNext())
		{
			path->Add(_wayPoint, 0);
			return true;
		}
		if (!node->GetPrev())
		{
			path->Add(_wayPoint, node);
			return true;
		}
	}
	//
	//Третий случай. Отсутствуют узлы, создается новый путь
	if (point && point != _wayPoint)
	{
		game::WayPath* path = _trace->GetInst()->AddPath();
		path->Add(_wayPoint);
		path->Add(point);

		return true;
	}

	return false;
}

void Trace::NodeControl::Select(bool active)
{
	_trace->_traceGfx->SetSelPoint(active ? _wayPoint : 0);
}

bool Trace::NodeControl::RayCastInters(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec) const
{
	return RayCastIntersectSphere(rayPos, rayVec, _wayPoint->GetPos(), _wayPoint->GetSize()/2.0f);
}

bool Trace::NodeControl::Compare(const IMapObjRef& node) const
{
	return false;
}

void Trace::NodeControl::OnStartDrag(const D3DXVECTOR3& scrRayPos, const D3DXVECTOR3& scrRayVec)
{
	NewLink(scrRayPos, scrRayVec);

	_dragRayPos = scrRayPos;
	_dragRayVec = scrRayVec;
}

void Trace::NodeControl::OnEndDrag(const D3DXVECTOR3& scrRayPos, const D3DXVECTOR3& scrRayVec)
{
	FreeLink();

	game::WayNode* curNode = _trace->GetInst()->RayCast(_dragRayPos, _dragRayVec);
	game::WayPoint* point = _trace->PickInstPoint(scrRayPos, scrRayVec);
	game::WayNode* node = _trace->GetInst()->RayCast(scrRayPos, scrRayVec);

	if (CreateWay(curNode, point, node))
	{
		if (_event)
			_event->OnPathesUpdate();
		return;
	}
}

void Trace::NodeControl::OnDrag(const D3DXVECTOR3& pos, const D3DXVECTOR3& scrRayPos, const D3DXVECTOR3& scrRayVec)
{
	if (_link)
		_link->SetPos(pos);
}

void Trace::NodeControl::OnShiftAction(const D3DXVECTOR3& scrRayPos, const D3DXVECTOR3& scrRayVec)
{
	game::WayPoint* newPoint = _trace->GetInst()->AddPoint();
	newPoint->SetPos(_wayPoint->GetPos());
	newPoint->SetSize(_wayPoint->GetSize());

	game::WayNode* curNode = _trace->GetInst()->RayCast(scrRayPos, scrRayVec);
	//Если узел по клику не найден, то берем путь из списка вейпойнта
	if (!curNode)
		curNode = (!_wayPoint->GetNodes().empty()) ? *_wayPoint->GetNodes().begin() : 0;
	if (curNode)
	{
		game::WayPath* path = curNode->GetPath();
		CreateWay(curNode, newPoint, 0);
	}

	Select(false);
	Reset(newPoint);
	Select(true);

	if (_event)
	{
		WayPoint* point = new WayPoint(newPoint);
		ControlEventRef mEvent = _event;
		mEvent->OnAddAndSelPoint(IWayPointRef(point, point));
		mEvent->OnPathesUpdate();
	}
}

D3DXVECTOR3 Trace::NodeControl::GetPos() const
{
	return _wayPoint->GetPos();
}

void Trace::NodeControl::SetPos(const D3DXVECTOR3& value)
{
	_wayPoint->SetPos(value);
}

D3DXQUATERNION Trace::NodeControl::GetRot() const
{
	return NullQuaternion;
}

void Trace::NodeControl::SetRot(const D3DXQUATERNION& value)
{
	//Nothing
}

D3DXVECTOR3 Trace::NodeControl::GetScale() const
{
	float radius = _wayPoint->GetSize() / sqrt(3.0f);

	return D3DXVECTOR3(radius, radius, radius);
}

void Trace::NodeControl::SetScale(const D3DXVECTOR3& value)
{
	_wayPoint->SetSize(D3DXVec3Length(&value));
}

D3DXVECTOR3 Trace::NodeControl::GetDir() const
{
	return XVector;
}

D3DXVECTOR3 Trace::NodeControl::GetRight() const
{
	return YVector;
}

D3DXVECTOR3 Trace::NodeControl::GetUp() const
{
	return ZVector;
}

D3DXMATRIX Trace::NodeControl::GetMat() const
{
	return BuildWorldMatrix(GetPos(), GetScale(), GetRot());
}

AABB Trace::NodeControl::GetAABB() const
{
	return AABB(1.0f);
}

game::WayPoint* Trace::PickInstPoint(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec)
{
	float minDist = 0;
	game::WayPoint* point = 0;	
	
	for (Inst::Points::const_iterator iter = GetInst()->GetPoints().begin(); iter != GetInst()->GetPoints().end(); ++iter)
	{
		float t;
		if (RayCastIntersectSphere(rayPos, rayVec, (*iter)->GetPos(), (*iter)->GetSize() / 2.0f, &t))
		{
			if (minDist > t || point == 0)
			{
				point = *iter;
				minDist = t;
			}
		}
	}

	return point;
}

IWayPointRef Trace::PickPoint(const lsl::Point& scrCoord)
{
	D3DXVECTOR3 scrRayPos;
	D3DXVECTOR3 scrRayVec;
	_edit->GetWorld()->GetCamera()->ScreenToRay(scrCoord, scrRayPos, scrRayVec);

	game::WayPoint* point = PickInstPoint(scrRayPos, scrRayVec);
	return IWayPointRef(point ? new WayPoint(point) : 0, this);
}

IWayPointRef Trace::AddPoint()
{
	return IWayPointRef(new WayPoint(GetInst()->AddPoint()), this);
}

void Trace::DelPoint(IWayPointRef& value)
{
	game::WayPoint* wp = value->GetImpl<WayPoint>()->GetInst();
	value.Release();
	GetInst()->DelPoint(wp);
}

void Trace::ClearPoints()
{
	GetInst()->ClearPoints();
}

IScNodeContRef Trace::GetPointControl(const IWayPointRef& ref, const ControlEventRef& mEvent)
{
	LSL_ASSERT(ref);

	return IScNodeContRef(new NodeControl(this, ref->GetImpl<WayPoint>()->GetInst(), mEvent), this);
}

IWayPathRef Trace::AddPath()
{
	return IWayPathRef(new WayPath(GetInst()->AddPath()), this);
}

void Trace::DelPath(IWayPathRef& value)
{
	game::WayPath* path = value->GetImpl<WayPath>()->GetInst();
	value.Release();
	GetInst()->DelPath(path);
}

void Trace::ClearPathes()
{
	GetInst()->ClearPathes();
}

IWayPathRef Trace::GetSelPath()
{
	return _selPath;
}

void Trace::SelectPath(const IWayPathRef& value)
{
	_selPath = value;

	if (_selPath)
		_traceGfx->SetSelPath(_selPath->GetImpl<WayPath>()->GetInst());
	else
		_traceGfx->SetSelPath(0);

}

IWayNodeRef Trace::GetSelNode()
{
	return _selNode;
}

void Trace::SelectNode(const IWayNodeRef& value)
{
	_selNode = value;
	if (_selNode)
		_traceGfx->SetSelNode(_selNode->GetImpl<WayNode>()->GetInst());
	else
		_traceGfx->SetSelNode(0);
}

void Trace::Clear()
{
	GetInst()->Clear();
}

IWayPointRef Trace::FirstPoint()
{
	Inst::Points::const_iterator iter = GetInst()->GetPoints().begin();
	WayPoint* point = 0;
	if (iter != GetInst()->GetPoints().end())
	{
		point = new WayPoint(static_cast<game::WayPoint*>(*iter));
		point->traceIter = iter;
	}

	return IWayPointRef(point, point);
}

void Trace::NextPoint(IWayPointRef& ref)
{
	WayPoint* point = ref->GetImpl<WayPoint>();
	
	if (++(point->traceIter) != GetInst()->GetPoints().end())
	{
		WayPoint* newPoint = new WayPoint(static_cast<game::WayPoint*>(*point->traceIter));
		newPoint->traceIter = point->traceIter;
		ref.Reset(newPoint, newPoint);
	}
	else
		ref.Release();
}

IWayPathRef Trace::FirstPath()
{
	Inst::Pathes::const_iterator iter = GetInst()->GetPathes().begin();
	WayPath* path = 0;
	if (iter != GetInst()->GetPathes().end())
	{
		path = new WayPath(static_cast<game::WayPath*>(*iter));
		path->traceIter = iter;
	}

	return IWayPathRef(path, path);
}

void Trace::NextPath(IWayPathRef& ref)
{
	WayPath* path = ref->GetImpl<WayPath>();
	
	if (++(path->traceIter) != GetInst()->GetPathes().end())
	{
		WayPath* newPath = new WayPath(static_cast<game::WayPath*>(*path->traceIter));
		newPath->traceIter = path->traceIter;
		ref.Reset(newPath, newPath);
	}
	else
		ref.Release();
}

bool Trace::GetEnableVisualize() const
{
	return _enableVisualize;
}

void Trace::EnableVisualize(bool value)
{
	if (_enableVisualize != value)
	{
		if (_enableVisualize)
			_edit->GetWorld()->GetGraph()->RemoveScNode(_traceGfx);	
		_enableVisualize = value;
		if (_enableVisualize)
			_edit->GetWorld()->GetGraph()->InsertScNode(_traceGfx);
	}
}

}

}