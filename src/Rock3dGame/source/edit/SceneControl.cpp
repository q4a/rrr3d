#include "stdafx.h"
#include "game\World.h"

#include "edit\SceneControl.h"
#include "edit\Edit.h"

namespace r3d
{

namespace edit
{

const SceneControl::DirMove DMCoordMoveToSC[graph::MovCoordSys::cDirMoveEnd] = {SceneControl::dmNone, SceneControl::dmX, SceneControl::dmY, SceneControl::dmZ, SceneControl::dmXY, SceneControl::dmXZ, SceneControl::dmYZ};




SceneControl::SceneControl(Edit* edit): _edit(edit), _selMode(smNone), _selNode(0), _linkBB(false), _actor(0), _movCoordSys(0), _scaleCoordSys(0)
{
	_control = new Control(this);
	_edit->GetWorld()->GetControl()->InsertEvent(_control);
}

SceneControl::~SceneControl()
{
	SetSelMode(smNone);
	SelectNode(0);

	_edit->GetWorld()->GetControl()->RemoveEvent(_control);
	delete _control;
}

SceneControl::Control::Control(SceneControl* owner): _owner(owner)
{
	ResetState();
}

void SceneControl::Control::ResetState()
{
	_shiftAction = false;
	_clDrag = false;
	_startDrag = false;
	_clDragOff = NullVector;
	_clDirMove = graph::MovCoordSys::dmNone;
	_clDirOff = NullVector;
	_clScDirMove = graph::ScaleCoordSys::dmNone;
	_clStScale = NullVector;
	_clRotating = false;
	_clStartRot = NullQuaternion;
}

bool SceneControl::Control::OnMouseClickEvent(const game::MouseClick& mClick)
{
	if (!_owner->_selNode)
		return false;
	INode* selNode = _owner->_selNode.Pnt();
	//Сброс сотояния перед кликом
	if (mClick.state == lsl::ksDown)
		ResetState();

	//Нажата левая кнопка мыши
	if (mClick.key == lsl::mkLeft && mClick.state == lsl::ksDown)
	{
		switch (_owner->_selMode)
		{
		case smNone:
			_clDrag = true;
			_clDragOff = _owner->ComputePos(selNode, mClick.scrRayPos, mClick.scrRayVec, dmView, NullVector);
			_clDragOff = _clDragOff - selNode->GetPos();
			return false;

		case smMove:
			_clDirMove = _owner->_movCoordSys->OnMouseClick(mClick.scrRayPos, mClick.scrRayVec, mClick.state);
			_clDirOff = _owner->ComputePos(selNode, mClick.scrRayPos, mClick.scrRayVec, DMCoordMoveToSC[_clDirMove], NullVector);
			_clDirOff = _clDirOff - selNode->GetPos();
			return _clDirMove != graph::MovCoordSys::dmNone;
			
		case smRotate:
			_clRotating = selNode->RayCastInters(mClick.scrRayPos, mClick.scrRayVec);
			_clStartRot = selNode->GetRot();
			return _clRotating;

		case smScale:
			_clScDirMove = _owner->_scaleCoordSys->OnMouseClick(mClick.scrRayPos, mClick.scrRayVec, mClick.state, _owner->_edit->GetWorld()->GetCamera()->GetPos());
			_clStScale = selNode->GetScale();
			return _clScDirMove != graph::ScaleCoordSys::dmNone;
		}
	}

	//Отпущена левая кнопка мыши.
	if (mClick.key == lsl::mkLeft && mClick.state == lsl::ksUp)
	{
		switch (_owner->_selMode)
		{
		case smNone:
			if (_startDrag)
			{
				selNode->OnEndDrag(mClick.scrRayPos, mClick.scrRayVec);
				return true;
			}
		}
	}

	return false;
}

bool SceneControl::Control::OnMouseMoveEvent(const game::MouseMove& mMove)
{
	if (!_owner->_selNode)
		return false;

	INodeRef selNode = _owner->_selNode;

	D3DXVECTOR2 offCoord(static_cast<float>(mMove.offCoord.x), static_cast<float>(mMove.offCoord.y));
	
	//Нажата левая кнопка мыши
	if (mMove.click.key == lsl::mkLeft && mMove.click.state == lsl::ksDown)
	{
		switch (_owner->_selMode)
		{
		case smNone:
			if (_clDrag)
			{
				if (!_startDrag && D3DXVec3Length(&_clDragOff) > 0.1f)
				{
					_startDrag = true;
					selNode->OnStartDrag(mMove.scrRayPos, mMove.scrRayVec);
				}
				if (_startDrag)
				{
					D3DXVECTOR3 pos = _owner->ComputePos(selNode.Pnt(), mMove.scrRayPos, mMove.scrRayVec, dmView, _clDragOff);
					selNode->OnDrag(pos, mMove.scrRayPos, mMove.scrRayVec);
				}
			}			
			break;

		case smMove:
		{
			if (_clDirMove != graph::MovCoordSys::dmNone)
			{
				bool shitMovUpdate = false;

				if (!_shiftAction && mMove.shift1)
				{
					graph::MovCoordSys::DirMove dirMove = _clDirMove;
					D3DXVECTOR3 clDirOff = _clDirOff;

					selNode->OnShiftAction(mMove.scrRayPos, mMove.scrRayVec);

					_shiftAction = true;
					_clDirMove = dirMove;
					_clDirOff = clDirOff;
					shitMovUpdate = true;
				}

				D3DXVECTOR3 pos = _owner->ComputePos(selNode.Pnt(), mMove.scrRayPos, mMove.scrRayVec, DMCoordMoveToSC[_clDirMove], _clDirOff);
				selNode->SetPos(pos);
				_owner->_movCoordSys->SetWorldPos(pos);

				return true;
			}
			return false;			
		}
		
		case smRotate:
			if (_clRotating)
			{
				if (!_shiftAction && mMove.shift1)
				{
					bool clRotating = _clRotating;
					D3DXQUATERNION clStartRot = _clStartRot;

					selNode->OnShiftAction(mMove.scrRayPos, mMove.scrRayVec);

					_shiftAction = true;
					_clRotating = clRotating;
					_clStartRot = clStartRot;	
				}

				//
				float angleZ = offCoord.x / 400.0f * 2 * D3DX_PI;
				angleZ = ceil(angleZ / (D3DX_PI/12.0f)) * (D3DX_PI/12.0f);
				//
				float angleY = -offCoord.y  / 400.0f * 2 * D3DX_PI;
				angleY = ceil(angleY / (D3DX_PI/12.0f)) * (D3DX_PI/12.0f);

				D3DXQUATERNION rotZ;
				D3DXQuaternionRotationAxis(&rotZ, &ZVector, angleZ);
				D3DXQUATERNION rotY;
				D3DXQuaternionRotationAxis(&rotY, &_owner->_edit->GetWorld()->GetCamera()->GetRight(), angleY);
				D3DXQUATERNION rot = abs(angleZ) > abs(angleY) ? rotZ : rotY;
					
				selNode->SetRot(_clStartRot * rot);

				return true;
			}
			break;

		case smScale:
			if (_clScDirMove != graph::ScaleCoordSys::dmNone)
				if (!_shiftAction && mMove.shift1)
				{
					graph::ScaleCoordSys::DirMove clScDirMove = _clScDirMove;
					D3DXVECTOR3 clStScale = _clStScale;

					selNode->OnShiftAction(mMove.scrRayPos, mMove.scrRayVec);

					_shiftAction = true;
					_clScDirMove = clScDirMove;
					_clStScale = clStScale;	
				}

			D3DXVECTOR2 fS(offCoord.x, -offCoord.y);
			fS /= 100.0f;
			switch (_clScDirMove)
			{					
			case graph::ScaleCoordSys::dmXYZ:
				selNode->SetScale(_clStScale + D3DXVECTOR3(fS.y, fS.y, fS.y));
				break;
			case graph::ScaleCoordSys::dmX:
				selNode->SetScale(_clStScale + D3DXVECTOR3(fS.x, 0, 0));
				break;
			case graph::ScaleCoordSys::dmY:
				selNode->SetScale(_clStScale + D3DXVECTOR3(0, fS.x, 0));
				break;
			case graph::ScaleCoordSys::dmZ:
				selNode->SetScale(_clStScale + D3DXVECTOR3(0, 0, fS.y));
				break;
			}
			return true;
		}
	}

	//Движение мышью
	switch (_owner->_selMode)
	{
	case smLink:
	{
		D3DXVECTOR3 pos = _owner->ComputePos(selNode.Pnt(), mMove.scrRayPos, mMove.scrRayVec, dmXY, NullVector);
		selNode->SetPos(pos);
		break;
	}

	case smMove:
		_owner->_movCoordSys->OnMouseMove(mMove.scrRayPos, mMove.scrRayVec);
		break;

	case smScale:
		_owner->_scaleCoordSys->OnMouseMove(mMove.scrRayPos, mMove.scrRayVec, _owner->_edit->GetWorld()->GetCamera()->GetPos());
		break;

	}

	return false;
}

bool SceneControl::ComputeAxeLink(const AABB& aabb, const D3DXMATRIX& aabbToWorld, const D3DXMATRIX& worldToAABB, const D3DXVECTOR3& normOff, INode* ignore, float& outDistOff, const float distLink)
{
	bool res = false;
	outDistOff = distLink;
	game::Map* map = _edit->GetWorld()->GetMap();

	for (game::Map::Objects::const_iterator iter = map->GetObjects().begin(); iter != map->GetObjects().end(); ++iter)	
	{
		game::MapObj* mapObj = iter->second;
		graph::BaseSceneNode* testSc = &(mapObj->GetGameObj().GetGrActor());
		if (!ignore->Compare(IMapObjRef(new MapObj(mapObj), this)))
		{			
			AABB test = testSc->GetLocalAABB(true);
			
			float dist;
			//Вычисляем двухсторонее пересечение test-а боксом aabb. Выбираем  наименьшее значение длины проникновения, начиная с distLink.
			if (test.AABBLineCastIntersect(aabb, normOff, aabbToWorld * testSc->GetInvWorldMat(), testSc->GetWorldMat() * worldToAABB, dist) && abs(dist) < abs(outDistOff))
			{
				res = true;
				outDistOff = dist;
			}		
		}
	}

	return res;
}

void SceneControl::ComputeLink(INode* node, const D3DXVECTOR3& pos, D3DXVECTOR3& resPos)
{
	D3DXVECTOR3 oldPos = node->GetPos();
	D3DXVECTOR3 offset = pos - oldPos;


	D3DXVECTOR3 newOff = offset;
	bool repeat = false;
	unsigned repCnt = 0;
	//Перемещение раскладывается на орты в системе координат mapObj, и по каждой орте ищется возможный  link. Если их несколько то процесс продолжается пока не будет наден близкий к нулю link или пока не истечет заданное число итераций (сделано на случай неопреденных ситуаций чтобы избежать зацикливания)
	//Порядкозависима, также зависит от начального перемещения (провялется например что нужно надаливать на поврезность для выравнивания по краям)
	do
	{
		float xDist;
		float yDist;
		float zDist;
		repeat = false;

		D3DXMATRIX worldMat;
		D3DXMatrixTranslation(&worldMat, newOff.x, newOff.y, newOff.z);
		worldMat = node->GetMat() * worldMat;
		D3DXMATRIX invWorldMat;
		D3DXMatrixInverse(&invWorldMat, 0, &worldMat);
		AABB aabb = node->GetAABB();

		if (!repeat && ComputeAxeLink(aabb, worldMat, invWorldMat, ZVector, node, zDist))
		{
			newOff += zDist * node->GetUp();
			repeat |= abs(zDist) > 0.001f;
		}
		if (!repeat && ComputeAxeLink(aabb, worldMat, invWorldMat, YVector, node, yDist))
		{
			newOff += yDist * node->GetRight();
			repeat |= abs(yDist) > 0.001f;
		}
		if (!repeat && ComputeAxeLink(aabb, worldMat, invWorldMat, XVector, node, xDist))
		{
			newOff += xDist * node->GetDir();
			repeat |= abs(xDist) > 0.001f;
		}

		++repCnt;
	}
	while (repeat && repCnt < 20);

	resPos = oldPos + newOff;
}

void SceneControl::CreateGraphActor()
{
	LSL_ASSERT(_actor == 0);

	graph::Actor::GraphDesc desc;
	desc.order = graph::Actor::goLast;
	_actor = new graph::Actor();
	_actor->SetGraph(_edit->GetWorld()->GetGraph(), desc);
}

void SceneControl::ReleaseGraphActor()
{
	LSL_ASSERT(_actor);

	lsl::SafeDelete(_actor);
}

void SceneControl::CreateMovCoordSys()
{
	LSL_ASSERT(_selNode);

	if (!_movCoordSys)
	{
		CreateGraphActor();

		_movCoordSys = &_actor->GetNodes().Add<graph::MovCoordSys>();
		_movCoordSys->AddRef();
		//_actor->SetParent(&GetInstSelMapObj()->GetGameObj().GetGrActor());
		_movCoordSys->SetWorldPos(_selNode->GetPos());
	}
}

void SceneControl::FreeMovCoordSys()
{
	if (_movCoordSys)
	{
		lsl::SafeRelease(_movCoordSys);
		ReleaseGraphActor();
	}
}

void SceneControl::CreateScaleCoordSys()
{
	LSL_ASSERT(_selNode);

	if (!_scaleCoordSys)
	{
		CreateGraphActor();

		_scaleCoordSys = &_actor->GetNodes().Add<graph::ScaleCoordSys>();
		_scaleCoordSys->AddRef();
		//_actor->SetParent(&GetInstSelMapObj()->GetGameObj().GetGrActor());
		_scaleCoordSys->SetWorldPos(_selNode->GetPos());
	}
}

void SceneControl::FreeScaleCoordSys()
{
	if (_scaleCoordSys)
	{
		lsl::SafeRelease(_scaleCoordSys);
		ReleaseGraphActor();
	}
}

void SceneControl::ApplySelMode()
{
	FreeMovCoordSys();
	FreeScaleCoordSys();

	if (!_selNode)
		return;
	
	switch (_selMode)
	{
	case smMove:
		CreateMovCoordSys();
		break;

	case smScale:
		CreateScaleCoordSys();
		break;
	}
}

D3DXVECTOR3 SceneControl::ComputePoint(const D3DXVECTOR3& curPos, const D3DXVECTOR3& rayStart, const D3DXVECTOR3& rayVec, DirMove dirMove, const D3DXVECTOR3& centerOff)
{
	D3DXVECTOR3 planeNorm;

	switch (dirMove)
	{
	case dmNone:
		return curPos;

	case dmX:
	case dmY:
	case dmXY:
		planeNorm = ZVector;
		break;

	case dmZ:
	case dmXZ:
		planeNorm = YVector;
		break;

	case dmYZ:
		planeNorm = XVector;
		break;

	case dmView:
		planeNorm = -_edit->GetWorld()->GetCamera()->GetDir();
		break;
	}
	const bool movAxe[3] =
	{
		(dirMove == dmX || dirMove == dmXY || dirMove == dmXZ || dirMove == dmView),
		(dirMove == dmY || dirMove == dmXY || dirMove == dmYZ || dirMove == dmView),
		(dirMove == dmZ || dirMove == dmXZ || dirMove == dmYZ || dirMove == dmView)
	};

	D3DXVECTOR3 pos = curPos;
	D3DXPLANE plane;
	D3DXPlaneFromPointNormal(&plane, &pos, &planeNorm);
	D3DXVECTOR3 newPos;
	if (abs(D3DXVec3Dot(&planeNorm, &rayVec)) < 0.05f || !RayCastIntersectPlane(rayStart, rayVec, plane, newPos))
	{
		newPos = NullVector;

		D3DXPlaneFromPointNormal(&plane, &pos, &XVector);		
		if (dirMove == dmZ && abs(D3DXVec3Dot(&XVector, &rayVec)) < 0.05f || !RayCastIntersectPlane(rayStart, rayVec, plane, newPos))
			newPos = NullVector;
	}

	for (int i = 0; i < 3; ++i)
		if (!movAxe[i])
			newPos[i] = pos[i];

	newPos -= centerOff;

	return newPos;
}

D3DXVECTOR3 SceneControl::ComputePos(INode* node, const D3DXVECTOR3& rayStart, const D3DXVECTOR3& rayVec, DirMove dirMove, const D3DXVECTOR3& centerOff)
{
	D3DXVECTOR3 newPos = ComputePoint(node->GetPos(), rayStart, rayVec, dirMove, centerOff);
	
	if (_linkBB)
		ComputeLink(node, newPos, newPos);

	return newPos;
}

SceneControl::INodeRef SceneControl::GetSelNode()
{
	return _selNode;
}

void SceneControl::SelectNode(const INodeRef& value)
{
	if (_selNode != value)
	{
		//Сбрасываем состояние в случае имзенения выделения
		_control->ResetState();

		if (_selNode)
		{
			_selNode->Select(false);
		}
		_selNode = value;		
		if (_selNode)
		{
			_selNode->Select(true);			
		}

		ApplySelMode();
	}
}

SceneControl::SelMode SceneControl::GetSelMode() const
{
	return _selMode;
}

void SceneControl::SetSelMode(SelMode value)
{
	if (_selMode != value)
	{
		//Сбрасываем состояние в случае имзенения выделения
		_control->ResetState();

		_selMode = value;
		ApplySelMode();		
	}
}

bool SceneControl::GetLinkBB() const
{
	return _linkBB;
}

void SceneControl::SetLinkBB(bool value)
{
	_linkBB = value;
}

}

}