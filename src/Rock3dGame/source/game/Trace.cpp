#include "stdafx.h"

#include "game\Trace.h"

namespace r3d
{

namespace game
{
	
WayPoint::WayPoint(Trace* trace, unsigned id): _trace(trace), _id(id), _pos(NullVector), _size(0), _off(NullVector)
{
	LSL_ASSERT(_trace);
}

WayPoint::~WayPoint()
{
	LSL_ASSERT(_nodes.empty());
}

void WayPoint::InsertNode(WayNode* node)
{
	_nodes.push_back(node);
}

void WayPoint::RemoveNode(WayNode* node)
{
	_nodes.Remove(node);
}

void WayPoint::Changed()
{
	for (Nodes::iterator iter = _nodes.begin(); iter != _nodes.end(); ++iter)
		(*iter)->Changed();
}

bool WayPoint::RayCast(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec, float* dist) const
{
	return RayCastIntersectSphere(rayPos, rayVec, _pos, _size / 2.0f, dist);
}

bool WayPoint::IsContains(const D3DXVECTOR3& point, float* dist) const
{
	float midDist = D3DXVec3Length(&(point - _pos));
	if (dist)
		*dist = midDist;

	return midDist < _size / 2.0f;
}

unsigned WayPoint::GetId() const
{
	return _id;
}

const D3DXVECTOR3& WayPoint::GetPos() const
{
	return _pos;
}

void WayPoint::SetPos(const D3DXVECTOR3& value)
{
	_pos = value;
	Changed();
}

float WayPoint::GetSize() const
{
	return _size;
}

void WayPoint::SetSize(float value)
{
	_size = value;
	Changed();
}

const D3DXVECTOR3& WayPoint::GetOff() const
{
	return _off;
}

void WayPoint::SetOff(const D3DXVECTOR3& value)
{
	_off = value;
	Changed();
}

bool WayPoint::IsFind(WayPath* path) const
{
	for (Nodes::const_iterator iter = _nodes.begin(); iter != _nodes.end(); ++iter)
		if ((*iter)->GetPath() == path)
			return true;

	return false;
}

bool WayPoint::IsFind(WayNode* node, WayNode* ignore) const
{
	for (Nodes::const_iterator iter = _nodes.begin(); iter != _nodes.end(); ++iter)
		if (*iter != ignore && *iter == node)
			return true;

	return false;
}

WayNode* WayPoint::GetRandomNode(WayNode* ignore, bool hasNext)
{
	LSL_ASSERT(!(ignore && ignore->GetPoint() != this));

	std::vector<WayNode*> nodes;

	for (Nodes::iterator iter = _nodes.begin(); iter != _nodes.end(); ++iter)
	{
		if (*iter == ignore || (hasNext && (*iter)->GetNext() == NULL))
			continue;

		nodes.push_back(*iter);
	}

	if (nodes.size() > 0)
		return nodes[RandomRange(0, nodes.size() - 1)];

	return NULL;
}

const WayPoint::Nodes& WayPoint::GetNodes() const
{
	return _nodes;
}




WayNode::WayNode(WayPath* path, WayPoint* point): _path(path), _point(point), _prev(0), _next(0)
{
	LSL_ASSERT(_point);

	_point->AddRef();
	_point->InsertNode(this);

	_tile = new Tile(this);
}

WayNode::~WayNode()
{
	delete _tile;

	_point->RemoveNode(this);
	_point->Release();
}

WayNode::Tile::Tile(WayNode* node): _node(node), cTrackCnt(node->GetPath()->GetTrace()->cTrackCnt), _dir(NullVec2), _norm(NullVec2), _normLine(NullVector), _midNormLine(NullVector), _changed(false)
{
}

void WayNode::Tile::ApplyChanges() const
{
	if (_changed)
	{
		_changed = false;

		D3DXVECTOR2 sPos = GetPos();
		if (_node->GetNext())
		{
			_dir = D3DXVECTOR2(GetNextPos()) - sPos;
			_dirLength = D3DXVec2Length(&_dir);
			D3DXVec2Normalize(&_dir, &_dir);
		}
		else
		{
			_dir = GetPrevDir();
			_dirLength = GetHeight();
		}

		Vec2NormCW(_dir, _norm);
		Line2FromNorm(_norm, sPos, _dirLine);
		Line2FromNorm(_dir, sPos, _normLine);

		//вычисляем и нормализуем поскольку лубая из составляющих _midDir могла оказаться близкой к нулю
		_midDir = (_dir + GetPrevDir()) / 2.0f;
		D3DXVec2Normalize(&_midDir, &_midDir);
		//линии через node
		Line2FromNorm(_midDir, sPos, _midNormLine);
		//Вычисляем _midNorm
		Line2GetDir(_midNormLine, _midNorm);

		//Вычисляем _nodeRadius
		float cosDelta = D3DXVec2Dot(&_dir, &GetPrevDir());
		//sinA/2 = sin(180 - D/2) = cos(D/2) = №(1 + cosD)/2
		_nodeRadius = GetHeight() / sqrt((1.0f + cosDelta) / 2.0f);

		//Вычисляем _edgeNorm
		if (D3DXVec2CCW(&GetPrevDir(), &_dir) > 0)
			Vec2NormCCW(_midDir, _edgeNorm);
		else
			Vec2NormCW(_midDir, _edgeNorm);
		Line2FromNorm(_edgeNorm, sPos + _nodeRadius * _edgeNorm, _edgeLine);		

		//Вычисляем turnAngle
		if (_node->GetPrev())
			_turnAngle = acos(D3DXVec2Dot(&GetPrevDir(), &_dir));
		else
			_turnAngle = 0.0f;

		//Вычисляем дистанцию до конечного узла
		_finishDist = _dirLength;
		if (_node->_next)
			_finishDist += _node->_next->GetTile().GetFinishDist();
		//Вычисляем дистанцию до начального узла
		//_startDist = _node->GetPath()->GetFirst()->GetTile().GetFinishDist() - _finishDist;
	}
}

const D3DXVECTOR3& WayNode::Tile::GetPos() const
{
	return _node->GetPoint()->GetPos();
}

float WayNode::Tile::GetHeight() const
{
	return _node->GetPoint()->GetSize() / 2.0f;
}

const D3DXVECTOR3& WayNode::Tile::GetNextPos() const
{
	return _node->GetNext() ? _node->GetNext()->GetTile().GetPos() : GetPos();
}

float WayNode::Tile::GetNextHeight() const
{
	return _node->GetNext() ? _node->GetNext()->GetTile().GetHeight() : GetHeight();
}

const D3DXVECTOR2& WayNode::Tile::GetPrevDir() const
{
	const Tile* tile = _node->GetPrev() ? &_node->GetPrev()->GetTile() : this;
	
	tile->ApplyChanges();
	return tile->_dir;
}

const D3DXVECTOR2& WayNode::Tile::GetNextMidNorm() const
{
	const Tile* tile = _node->GetNext() ? &_node->GetNext()->GetTile() : this;
	
	tile->ApplyChanges();
	return tile->_midNorm;
}

const D3DXVECTOR3& WayNode::Tile::GetNextNormLine() const
{
	const Tile* tile = _node->GetNext() ? &_node->GetNext()->GetTile() : this;
	
	tile->ApplyChanges();
	return tile->_midNormLine;
}

float WayNode::Tile::GetNextNodeRadius() const
{
	const Tile* tile = _node->GetNext() ? &_node->GetNext()->GetTile() : this;

	tile->ApplyChanges();
	return tile->_nodeRadius;
}

void WayNode::Tile::Changed()
{
	_changed = true;
}

void WayNode::Tile::GetVBuf(D3DXVECTOR3* vBuf, unsigned length, const D3DXVECTOR3* upVec) const
{
	LSL_ASSERT(length >= 4);

	ApplyChanges();

	D3DXVECTOR2 nextMidNorm = GetNextMidNorm();

	vBuf[0] = GetPos() + D3DXVECTOR3(_midNorm.x, _midNorm.y, 0.0f) * _nodeRadius;
	vBuf[1] = GetPos() - D3DXVECTOR3(_midNorm.x, _midNorm.y, 0.0f) * _nodeRadius;
	vBuf[2] = GetNextPos() + D3DXVECTOR3(nextMidNorm.x, nextMidNorm.y, 0.0f) * GetNextNodeRadius();
	vBuf[3] = GetNextPos() - D3DXVECTOR3(nextMidNorm.x, nextMidNorm.y, 0.0f) * GetNextNodeRadius();

	if (!upVec)
		vBuf[0].z = vBuf[1].z = vBuf[2].z = vBuf[3].z = 0;
}

unsigned WayNode::Tile::ComputeTrackInd(const D3DXVECTOR2& point) const
{
	ApplyChanges();

	float tileWidth = GetWidth(point);
	float trackDiv = tileWidth / cTrackCnt;
	float trackPos = Line2DistToPoint(_dirLine, point);
	
	unsigned trackInd = static_cast<unsigned>(abs(floor(trackPos/trackDiv + cTrackCnt/2.0f)));
	trackInd = std::max<int>(trackInd , 0);
	trackInd = std::min<int>(trackInd , cTrackCnt - 1);

	/*float tileWidth = GetNormLength(point) * 2.0f;
	float trackDiv = tileWidth / cTrackCnt;
	float trackPos = Line2DistToPoint(_dirLine, point);

	float off = cTrackCnt % 2 > 0 ? 0.0f : 0.5f;
	int trackInd = static_cast<int>(Round(trackPos / trackDiv + off));
	
	trackInd = std::max<int>(trackInd , -cTrackCnt/2);
	trackInd = std::min<int>(trackInd , cTrackCnt/2);*/

	return trackInd;
}

D3DXVECTOR2 WayNode::Tile::ComputeTrackNormOff(const D3DXVECTOR2& point, unsigned track) const
{
	ApplyChanges();

	D3DXVECTOR2 dir = _dir;

	float tileWidth = GetWidth(point);
	float trackDiv = tileWidth / cTrackCnt;
	float off = Line2DistToPoint(_dirLine, point);
	off = tileWidth/2.0f + off;
	float trackOff = trackDiv * (track + 0.5f)  - off;

	return _norm * trackOff;
}

void PlaneFromDirVec(const D3DXVECTOR3& dir, const D3DXVECTOR3& norm, const D3DXVECTOR3& pos, D3DXPLANE& plane)
{
	D3DXVECTOR3 right, up;
	D3DXVec3Cross(&right, &dir, &norm);
	D3DXVec3Cross(&up, &right, &dir);
	D3DXPlaneFromPointNormal(&plane, &pos, &up);
}

bool WayNode::Tile::RayCast(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec, float* dist) const
{
	D3DXVECTOR3 dir = GetNextPos() - GetPos();
	D3DXVec3Normalize(&dir, &dir);

	D3DXPLANE plane;
	//Условие неколлинеарности векторов
	if (D3DXVec3Dot(&dir, &rayVec) > 0.001f)
		//плоскость перепендикулярно лучу в центре цилиндра тайла
		PlaneFromDirVec(dir, rayVec, GetPos(), plane);
	//Вектора коллиниарны
	else
		//Строим плоскость через центр тайла перпендикулярно направлению
		D3DXPlaneFromPointNormal(&plane, &((GetPos() + GetNextPos()) / 2.0f), &dir);

	float tmp;
	bool res = RayCastIntersectPlane(rayPos, rayVec, plane, tmp) && IsContains(rayPos + rayVec * tmp);
	if (dist)
		*dist = tmp;

	return res;
}

bool WayNode::Tile::IsContains(const D3DXVECTOR3& point, bool lengthClamp, float* dist, float widthErr) const
{
	ApplyChanges();

	D3DXVECTOR3 pos1 = GetPos();
	D3DXVECTOR3 pos2 = GetNextPos();

	//Расстояние в 2д плоскости
	D3DXVECTOR2 point2 = point;
	float dist1 = Line2DistToPoint(_midNormLine, point2);
	float dist2 = Line2DistToPoint(GetNextNormLine(), point2);
	float dirDist = Line2DistToPoint(_dirLine, point2);
	
	//Высота от плоскости трасы до поверхности ограничивающего цилиндра
	float coordX = ComputeCoordX(dist1);
	float coordZ = ComputeZCoord(coordX);
	//
	float height = ComputeHeight(coordX);
	float halfWidth = ComputeWidth(coordX)/2.0f;
	if (dist)
		*dist = height;

	//Первое условие, ограничение по длине
	//Второе условие, ограничение по ширине
	//Третье условие, ограничение по высоте
	return (!lengthClamp || dist1 * dist2 < 0) && abs(dirDist) < (halfWidth + widthErr) && abs(coordZ - point.z) < height;
}

bool WayNode::Tile::IsZLevelContains(const D3DXVECTOR3& point, float* dist) const
{
	ApplyChanges();

	D3DXVECTOR2 pos = D3DXVECTOR2(point);
	float height = GetHeight(pos);
	float coordZ = GetZCoord(pos);

	return (coordZ - point.z) < height;
}

const D3DXVECTOR2& WayNode::Tile::GetDir() const
{
	ApplyChanges();
	return _dir;
}

float WayNode::Tile::GetDirLength() const
{
	ApplyChanges();

	return _dirLength;
}

const D3DXVECTOR2& WayNode::Tile::GetNorm() const
{
	ApplyChanges();
	return _norm;
}

const D3DXVECTOR3& WayNode::Tile::GetDirLine() const
{
	ApplyChanges();
	return _dirLine;
}

const D3DXVECTOR2& WayNode::Tile::GetMidDir() const
{
	ApplyChanges();
	return _midDir;
}

const D3DXVECTOR2& WayNode::Tile::GetMidNorm() const
{
	ApplyChanges();
	return _midNorm;
}

const D3DXVECTOR3& WayNode::Tile::GetMidNormLine() const
{
	ApplyChanges();
	return _midNormLine;
}

const D3DXVECTOR2& WayNode::Tile::GetEdgeNorm() const
{
	ApplyChanges();
	return _edgeNorm;
}

float WayNode::Tile::GetNodeRadius() const
{
	ApplyChanges();
	return _nodeRadius;
}

const D3DXVECTOR3& WayNode::Tile::GetEdgeLine() const
{
	ApplyChanges();
	return _edgeLine;
}

float WayNode::Tile::GetTurnAngle() const
{
	ApplyChanges();
	return _turnAngle;
}

float WayNode::Tile::GetFinishDist() const
{
	ApplyChanges();
	return _finishDist;
}

float WayNode::Tile::GetStartDist() const
{
	//пока не реализована
	LSL_ASSERT(false);

	ApplyChanges();
	return _startDist;
}

float WayNode::Tile::ComputeCoordX(float dist) const
{
	ApplyChanges();

	return _dirLength > 0.0f ? lsl::ClampValue(dist / _dirLength, 0.0f, 1.0f) : 0.0f;
}

float WayNode::Tile::ComputeCoordX(const D3DXVECTOR2& point) const
{
	ApplyChanges();

	float dist = Line2DistToPoint(_normLine, point);	

	return ComputeCoordX(dist);
}

float WayNode::Tile::ComputeLength(float coordX) const
{
	ApplyChanges();

	return _dirLength * lsl::ClampValue(coordX, 0.0f, 1.0f);
}

float WayNode::Tile::ComputeWidth(float coordX) const
{
	return ComputeHeight(coordX) * 2.0f;
}

float WayNode::Tile::ComputeHeight(float coordX) const
{
	return GetHeight() + (GetNextHeight() - GetHeight()) * coordX;
}

float WayNode::Tile::ComputeZCoord(float coordX) const
{
	float posZ1 = GetPos().z;
	float posZ2 = GetNextPos().z;

	return posZ1 + (posZ2 - posZ1) * coordX;
}

float WayNode::Tile::GetLength(const D3DXVECTOR2& point) const
{
	return ComputeLength(ComputeCoordX(point));
}

float WayNode::Tile::GetHeight(const D3DXVECTOR2& point) const
{
	return ComputeHeight(ComputeCoordX(point));
}

float WayNode::Tile::GetWidth(const D3DXVECTOR2& point) const
{
	return ComputeWidth(ComputeCoordX(point));
}

float WayNode::Tile::GetZCoord(const D3DXVECTOR2& point) const
{
	return ComputeZCoord(ComputeCoordX(point));
}

D3DXVECTOR3 WayNode::Tile::GetPoint(float coordX) const
{
	return GetPos() + (GetNextPos() - GetPos()) * coordX;
}

D3DXVECTOR3 WayNode::Tile::GetCenter3() const
{
	ApplyChanges();

	return (GetNextPos() + GetPos()) * 0.5f;
}

void WayNode::SetPrev(WayNode* node)
{
	_prev = node;
	Changed();
}

void WayNode::SetNext(WayNode* node)
{
	_next = node;
	Changed();
}

void WayNode::Changed()
{
	_tile->Changed();

	/*//Также изменяются соседние тайлы
	//есть связные векторные данные
	if (_prev)
		_prev->_tile->Changed();
	if (_next)
		_next->_tile->Changed();*/

	//Для упрощения изменяем все связные узлы патча (все равно статический)
	//Есть связность дистанций до финиша
	//назад
	WayNode* curNode = _prev;
	while (curNode)
	{
		LSL_ASSERT(curNode != this);

		curNode->_tile->Changed();
		curNode = curNode->_prev;
	}
	//вперед
	curNode = _next;
	while (curNode)
	{
		LSL_ASSERT(curNode != this);

		curNode->_tile->Changed();
		curNode = curNode->_next;
	}	
}

bool WayNode::RayCast(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec, float* dist) const
{
	return RayCastIntersectSphere(rayPos, rayVec, _point->GetPos(), _tile->GetNodeRadius(), dist);
}

bool WayNode::IsContains2(const D3DXVECTOR2& point, float* dist) const
{
	float midDist = D3DXVec2Length(&(point - D3DXVECTOR2(_point->GetPos())));
	if (dist)
		*dist = midDist;

	return midDist < _tile->GetNodeRadius();
}

bool WayNode::IsContains(const D3DXVECTOR3& point, float* dist) const
{
	float midDist = D3DXVec3Length(&(point - _point->GetPos()));
	if (dist)
		*dist = midDist;

	return midDist < _tile->GetNodeRadius();

	//return IsContains2(D3DXVECTOR2(point), dist) && abs(point.z - GetPos().z) < _point->GetSize()/2.0f;
}

WayPath* WayNode::GetPath()
{
	return _path;
}

WayPoint* WayNode::GetPoint()
{
	return _point;
}

const WayNode::Tile& WayNode::GetTile() const
{
	return *_tile;
}

WayNode* WayNode::GetPrev()
{
	return _prev;
}

WayNode* WayNode::GetNext()
{
	return _next;
}

const D3DXVECTOR3& WayNode::GetPos() const
{
	return _point->GetPos();
}

D3DXVECTOR2 WayNode::GetPos2() const
{
	return D3DXVECTOR2(GetPos());
}

float WayNode::GetSize() const
{
	return _point->GetSize();
}

float WayNode::GetRadius() const
{
	return _point->GetSize()/2.0f;
}




WayPath::WayPath(Trace* trace): _trace(trace), _first(0), _last(0), _count(0)
{
}

WayPath::~WayPath()
{
	Clear();
}

WayNode* WayPath::Add(WayPoint* point, WayNode* mWhere)
{
	WayNode* node = new WayNode(this, point);
	++_count;

	//Узел корневой
	if (_first == 0)
	{
		_first = node;
		_last = node;
		node->Changed();
		return node;
	}

	//Условие добавления в конец
	if (mWhere == 0)
	{
		node->SetPrev(_last);
		_last->SetNext(node);
		_last = node;
		node->Changed();
		return node;
	}

	LSL_ASSERT(mWhere);

	//Устанавливаем связи для node
	node->SetPrev(mWhere->_prev);
	node->SetNext(mWhere);

	//Корректируем существующие связи
	if (mWhere->_prev)
		mWhere->_prev->SetNext(node);
	else
		_first = node;
	mWhere->SetPrev(node);

	node->Changed();

	return node;
}

void WayPath::Delete(WayNode* value)
{
	//Корректируем существующие связи
	if (value->_prev)
		value->_prev->SetNext(value->_next);
	if (value->_next)
		value->_next->SetPrev(value->_prev);

	//Меняем при необходимости граничные узлы
	if (value == _first)
		_first = value->_next;
	if (value == _last)
		_last = value->_prev;

	LSL_ASSERT(_count > 0);
	--_count;

	delete value;
}

void WayPath::Clear()
{
	WayNode* node = _first;
	while (node)
	{
		WayNode* tmp = node;
		node = node->_next;
		delete tmp;
	}

	_first = 0;
	_last = 0;
	_count = 0;
}

bool WayPath::IsEnclosed() const
{
	return _enclosed;
}

void WayPath::Enclosed(bool value)
{
	if (_enclosed != value)
	{
		_enclosed = value;
		if (_first)
		{
			_first->SetPrev(_enclosed ? _last : 0);
			_last->SetNext(_enclosed ? _first : 0);
		}
	}
}

WayNode* WayPath::RayCast(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec, WayNode* mWhere, float* dist) const
{
	float minDist = 0;
	WayNode* resNode = 0;

	//Сначала поиск по тайлам
	WayNode* node = mWhere ? mWhere : _first;
	while (node)
	{
		float nodeDist;
		if (node->RayCast(rayPos, rayVec, &nodeDist) && (!resNode || minDist > nodeDist))
		{
			minDist = nodeDist;
			resNode = node;
		}

		node = node->GetNext();
	}

	//Затем находится ближайщий узел
	float nodeDist;
	if (resNode && resNode->GetNext() &&  resNode->GetNext()->RayCast(rayPos, rayVec, &nodeDist))
	{
		resNode = resNode->GetNext();
		//Т.к. resNode != 0, то minDist инициализирован
		minDist = std::min(nodeDist, minDist);
	}
	//При отсутсвии результата проверям конечные узлы
	if (!resNode && _first && _first->RayCast(rayPos, rayVec, &nodeDist))
	{
		resNode = _first;
		minDist = nodeDist;
	}
	if (!resNode && _last && _last->RayCast(rayPos, rayVec, &nodeDist))
	{
		resNode = _last;
		minDist = nodeDist;
	}

	if (dist)
		*dist = minDist;
	return resNode;
}

WayNode* WayPath::IsTileContains(const D3DXVECTOR3& point, WayNode* mWhere) const
{
	WayNode* resNode = 0;

	WayNode* node = mWhere? (mWhere->GetPrev() ? mWhere->GetPrev() : mWhere) : _first;
	//
	while (node)
	{
		if (node->GetTile().IsContains(point))
		{
			resNode = node;
			break;
		}
		node = node->GetNext();
	}

	//При отсутсвии результата проверям конечные узлы
	if (!resNode && _first && _first->IsContains(point))
	{
		resNode = _first;		
	}
	if (!resNode && _last && _last->IsContains(point))
	{
		resNode = _last;		
	}

	return resNode;
}

void WayPath::GetTriStripVBuf(res::VertexData& data, const D3DXVECTOR3* upVec)
{
	data.SetFormat(res::VertexData::vtPos3, true);

	if (GetCount() < 2)	
	{
		data.SetVertexCount(0);
		return;
	}

	data.SetVertexCount(GetCount() * 2);
	data.Init();
	res::VertexIter pVert = data.begin();

	WayNode* node = GetFirst();
	while (node && node->GetNext())
	{
		WayNode* nextNode = node->GetNext();
		node->GetTile().GetVBuf(pVert.Pos3(), 4, upVec);
		//Смещаем только на два индекса, поскольку строится стрип, а две соседних вершины тайла попарно совпадают
		pVert += 2;
		node = nextNode;
	}

	data.Update();
}

Trace* WayPath::GetTrace()
{
	return _trace;
}

WayNode* WayPath::GetFirst()
{
	return _first;
}

WayNode* WayPath::GetLast()
{
	return _last;
}

unsigned WayPath::GetCount() const
{
	return _count;
}

float WayPath::GetLength() const
{
	return _first ? _first->GetTile().GetFinishDist() : 0.0f;
}




Trace::Trace(unsigned tracksCnt): cTrackCnt(tracksCnt), _pointId(0)
{
}

Trace::~Trace()
{
	Clear();
}

WayPoint* Trace::AddPoint(unsigned id)
{
	WayPoint* point = new WayPoint(this, id);
	_points.push_back(point);
	
	_pointId = std::max(_pointId, id + 1);

	return point;
}

void Trace::Save(SWriter* writer)
{
	_MyBase::Save(writer);

	SWriter* sPoints = writer->NewDummyNode("points");
	unsigned i = 0;
	for (Points::iterator iter = _points.begin(); iter != _points.end(); ++iter, ++i)
	{
		WayPoint* point = *iter;

		std::stringstream sstream;
		sstream << "point" << i;
		SWriter* sPoint = sPoints->NewDummyNode(sstream.str().c_str());
		sPoint->WriteAttr("id", point->GetId());
		lsl::SWriteValue(sPoint, "pos", point->GetPos());
		lsl::SWriteValue(sPoint, "size", point->GetSize());		
	}

	SWriter* sPathes = writer->NewDummyNode("pathes");
	i = 0;
	for (Pathes::iterator iter = _pathes.begin(); iter != _pathes.end(); ++iter, ++i)
	{
		WayPath* path = *iter;

		std::stringstream sstream;
		sstream << "path" << i;
		SWriter* sPath = sPathes->NewDummyNode(sstream.str().c_str());

		unsigned iNode = 0;
		WayNode* node = path->GetFirst();
		while (node)
		{
			std::stringstream sstream;
			sstream << "node" << iNode;
			SWriter* sNode = sPath->WriteValue(sstream.str().c_str(), node->GetPoint()->GetId());
			
			node = node->GetNext();
			++iNode;
		}
	}
}

void Trace::Load(SReader* reader)
{
	Clear();

	SReader* sPoints = reader->ReadValue("points");
	SReader* sPoint = sPoints->FirstChildValue();
	while (sPoint)
	{
		const lsl::SIOTraits::ValueDesc* val;
		D3DXVECTOR3 vec3;
		float fVal;

		game::WayPoint* point;
		if (val = sPoint->ReadAttr("id"))
		{
			unsigned id = 0;
			val->CastTo<unsigned>(&id);

			point = AddPoint(id);
		}
		else
			point = AddPoint();

		if (SReadValue(sPoint, "pos", vec3))
			point->SetPos(vec3);
		if (sPoint->ReadValue("size", fVal))
			point->SetSize(fVal);

		sPoint = sPoint->NextValue();
	}

	SReader* sPathes = reader->ReadValue("pathes");
	SReader* sPath = sPathes->FirstChildValue();
	while (sPath)
	{
		WayPath* path = AddPath();

		SReader* sNode = sPath->FirstChildValue();
		while (sNode)
		{
			unsigned id;
			sNode->GetVal().CastTo<unsigned>(&id);
			if (WayPoint* point = FindPoint(id))
			{
				WayNode* node = path->Add(point);
			}

			sNode = sNode->NextValue();
		}

		sPath = sPath->NextValue();
	}

	_MyBase::Load(reader);
}

WayPoint* Trace::AddPoint()
{
	return AddPoint(_pointId++);
}

void Trace::DelPoint(WayPoint* value)
{
	_points.Remove(value);
	delete value;
}

void Trace::ClearPoints()
{
	_pointId = 0;
	for (Points::iterator iter = _points.begin(); iter != _points.end(); ++iter)
		delete *iter;

	_points.clear();
}

WayPath* Trace::AddPath()
{
	WayPath* path = new WayPath(this);
	_pathes.push_back(path);
	
	return path;
}

void Trace::DelPath(WayPath* value)
{
	_pathes.Remove(value);
	delete value;
}

void Trace::ClearPathes()
{
	for (Pathes::iterator iter = _pathes.begin(); iter != _pathes.end(); ++iter)
		delete *iter;

	_pathes.clear();
}

void Trace::Clear()
{
	ClearPathes();
	ClearPoints();
}

WayNode* Trace::RayCast(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec, float* dist) const
{
	float minDist = 0;
	WayNode* resNode = 0;

	for (Pathes::const_iterator iter = _pathes.begin(); iter != _pathes.end(); ++iter)
	{
		WayPath* path = *iter;
		float dist;
		WayNode* node = path->RayCast(rayPos, rayVec, 0, &dist);
		if (node && (!resNode || minDist > dist))
		{
			resNode = node;
			minDist = dist;
		}
	}

	if (dist)
		*dist = minDist;
	return resNode;
}

WayNode* Trace::IsTileContains(const D3DXVECTOR3& point, WayNode* mWhere) const
{
	WayPath* wPath = 0;
	if (mWhere)
	{
		wPath = mWhere->GetPath();
		WayNode* res = wPath->IsTileContains(point, mWhere);
		if (res)
			return res;
	}

	for (Pathes::const_iterator iter = _pathes.begin(); iter != _pathes.end(); ++iter)
	{
		WayNode* res = 0;
		if (*iter != wPath && (res = (*iter)->IsTileContains(point, 0)))
			return res;
	}

	return 0;
}

WayPoint* Trace::FindPoint(unsigned id)
{
	for (Points::iterator iter = _points.begin(); iter != _points.end(); ++iter)
		if ((*iter)->GetId() == id)
			return *iter;

	return 0;
}

WayNode* Trace::FindClosestNode(const D3DXVECTOR3& point)
{
	float minDist;
	WayNode* res = 0;
	for (Points::iterator iter = _points.begin(); iter != _points.end(); ++iter)
	{
		if (!(*iter)->GetNodes().empty())
		{
			float dist;
			(*iter)->IsContains(point, &dist);

			if (!res || dist < minDist)
			{
				minDist = dist;
				res = (*iter)->GetNodes().front();
			}
		}
	}

	return res;
}

const Trace::Points& Trace::GetPoints() const
{
	return _points;
}

const Trace::Pathes& Trace::GetPathes() const
{
	return _pathes;
}

}

}