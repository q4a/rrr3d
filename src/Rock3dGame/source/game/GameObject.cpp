#include "stdafx.h"
#include "game/Logic.h"

#include "game/GameObject.h"
#include "game/World.h"
#include "GraphManager.h"

namespace r3d
{

namespace game
{

GameObject::GameObject(): _mapObj(0), _logic(0), _destroy(false), _parent(0), _liveState(lsLive), _maxLife(-1), _maxTimeLife(-1), _life(_maxLife), _timeLife(0), _immortalFlag(false), _immortalTime(0), storeSource(true), storeProxy(true), _touchPlayerTime(0), _touchPlayerId(cUndefPlayerId), _posSync(NullVector), _posSyncDir(NullVector), _posSyncLength(0), _rotSync(NullQuaternion), _rotSyncAxis(NullVector), _rotSyncAngle(0.0f), _posSync2(NullVector), _posSyncDir2(NullVector), _posSyncDist2(0), _posSyncLength2(0), _pxPosLerp(NullVector), _pxRotLerp(NullQuaternion), _pxVelocityLerp(NullVector), _rotSync2(NullQuaternion), _rotSyncAxis2(NullVector), _rotSyncAngle2(0), _rotSyncLength2(0), _frameEventCount(0), _progressEventCount(0), _lateProgressEventCount(0), _fixedStepEventCount(0), _syncFrameEvent(false), _bodyProgressEvent(false), _pxPrevPos(NullVector), _pxPrevRot(NullQuaternion), _pxPrevVelocity(NullVector)
{
	_grActor = new graph::Actor();
	_grActor->SetOwner(this);
	_grActor->storeCoords = false;

	_pxActor = new px::Actor(this);
	_pxActor->storeCoords = false;

	_includeList = new IncludeList(this);

	_behaviors = new Behaviors(this);
}

GameObject::~GameObject()
{
	SetSyncFrameEvent(false);
	SetBodyProgressEvent(false);
	Destroy();

	delete _behaviors;

	_includeList->Clear();

	LSL_ASSERT(_children.empty());

	ClearListenerList();

	SetLogic(0);
	SetParent(0);

	delete _includeList;

	delete _pxActor;
	delete _grActor;
}

void GameObject::SetSyncFrameEvent(bool value)
{
	if (_syncFrameEvent != value)
	{
		_syncFrameEvent = value;

		if (value)
			RegFrameEvent();
		else
			UnregFrameEvent();
	}
}

void GameObject::SetBodyProgressEvent(bool value)
{
	if (_bodyProgressEvent != value)
	{
		_bodyProgressEvent = value;

		if (value)
		{
			RegLateProgressEvent();
			RegFrameEvent();
		}
		else
		{
			UnregLateProgressEvent();
			UnregFrameEvent();
		}
	}
}

void GameObject::Destroy()
{
	if (!_destroy)
	{
		_destroy = true;

		for (ListenerList::Position pos = _listenerList.First(); GameObjListener** iter = _listenerList.Current(pos); _listenerList.Next(pos))
			(*iter)->OnDestroy(this);
	}
}

void GameObject::RegFrameEvent()
{
	if (++_frameEventCount == 1 && _logic)
		_logic->RegFrameEvent(this);
}

void GameObject::UnregFrameEvent()
{
	LSL_ASSERT(_frameEventCount > 0);

	if (--_frameEventCount == 0 && _logic)
		_logic->UnregFrameEvent(this);
}

void GameObject::RegProgressEvent()
{
	if (++_progressEventCount == 1 && _logic)
	{
		//_logic->RegProgressEvent(this);
	}
}

void GameObject::UnregProgressEvent()
{
	LSL_ASSERT(_progressEventCount > 0);

	if (--_progressEventCount == 0 && _logic)
	{
		//_logic->UnregProgressEvent(this);
	}
}

void GameObject::RegLateProgressEvent()
{
	if (++_lateProgressEventCount == 1 && _logic)
		_logic->RegLateProgressEvent(this);
}

void GameObject::UnregLateProgressEvent()
{
	LSL_ASSERT(_lateProgressEventCount > 0);

	if (--_lateProgressEventCount == 0 && _logic)
		_logic->UnregLateProgressEvent(this);
}

void GameObject::RegFixedStepEvent()
{
	if (++_fixedStepEventCount == 1 && _logic)
		_logic->RegFixedStepEvent(this);
}

void GameObject::UnregFixedStepEvent()
{
	LSL_ASSERT(_fixedStepEventCount > 0);

	if (--_fixedStepEventCount == 0 && _logic)
		_logic->UnregFixedStepEvent(this);
}

glm::vec3 GameObject::GetContactPoint(const px::Scene::OnContactEvent& contact)
{
	NxContactStreamIterator contIter(contact.stream);

	while (contIter.goNextPair())
		while (contIter.goNextPatch())
			while (contIter.goNextPoint())
			{
				return glm::make_vec3(contIter.getPoint().get());
			}

			return NullVector;
}

bool GameObject::ContainsContactGroup(NxContactStreamIterator& contIter, unsigned actorIndex, px::Scene::CollDisGroup group)
{
	while (contIter.goNextPair())
		while (contIter.goNextPatch())
			while (contIter.goNextPoint())
			{
				if (contIter.getShape(actorIndex)->getGroup() == group)
					return true;
			}

	return false;
}

void GameObject::OnContact(const px::Scene::OnContactEvent& contact)
{
	for (ListenerList::Position pos = _listenerList.First(); GameObjListener** iter = _listenerList.Current(pos); _listenerList.Next(pos))
		(*iter)->OnContact(contact);
}

void GameObject::OnWake()
{
	SetBodyProgressEvent(true);
}

void GameObject::OnSleep()
{
	SetBodyProgressEvent(false);
}

void GameObject::RayCastClosestActor(const glm::vec3& rayStart, const glm::vec3& rayDir, NxShapesType shapesType, RayCastHit& hit, unsigned groups, unsigned mask, float maxDist)
{
	NxRaycastHit nxHit;
	hit.gameActor = 0;

	NxGroupsMask nxMask;
	nxMask.bits0 = mask;
	nxMask.bits1 = 0;
	nxMask.bits2 = 0;
	nxMask.bits3 = 0;

	if (NxShape* shape = GetPxActor().GetScene()->GetNxScene()->raycastClosestShape(NxRay(glm::value_ptr(rayStart), glm::value_ptr(rayDir)), shapesType, nxHit, groups, maxDist, 0xFFFFFFFF, mask > 0 ? &nxMask : 0))
		hit.gameActor = GetGameObjFromShape(shape);

	hit.distance = nxHit.distance;
}

void GameObject::DoDeath(DamageType damageType, GameObject* target)
{
	_liveState = lsDeath;
	_includeList->Death(damageType, target);
}

void GameObject::SendDeath(DamageType damageType, GameObject* target)
{
	OnDeath(this, damageType, target);

	for (ListenerList::Position pos = _listenerList.First(); GameObjListener** iter = _listenerList.Current(pos); _listenerList.Next(pos))
		(*iter)->OnDeath(this, damageType, target);
}

void GameObject::OnPxSync(float alpha)
{
	NxActor* nxActor = _pxActor->GetNxActor();
	if (nxActor == NULL)
		return;

	glm::vec3 pxVelocityLerp = glm::make_vec3(nxActor->getLinearVelocity().get());

	if (alpha < 1.0f)
	{
		_pxPosLerp = Vec3Lerp(_pxPrevPos, _pxActor->GetPos(), alpha);
		_pxRotLerp = glm::slerp(_pxPrevRot, _pxActor->GetRot(), alpha);
		_pxVelocityLerp = Vec3Lerp(_pxPrevVelocity, pxVelocityLerp, alpha);
	}
	else
	{
		_pxPrevPos = _pxPosLerp = _pxActor->GetPos();
		_pxPrevRot = _pxRotLerp = _pxActor->GetRot();
		_pxPrevVelocity = _pxVelocityLerp = pxVelocityLerp;
	}

	_grActor->SetPos(_pxPosLerp);
	_grActor->SetRot(_pxRotLerp);
}

void GameObject::OnProgress(float deltaTime)
{
	_timeLife += deltaTime;

	if (_immortalTime > 0)
	{
		_immortalTime -= deltaTime;
		if (_immortalTime <= 0)
		{
			_immortalTime = 0;
			OnImmortalStatus(false);
			_behaviors->OnImmortalStatus(false);
		}
	}

	_includeList->OnProgress(deltaTime);

	if (_touchPlayerTime > 0.0f && (_touchPlayerTime -= deltaTime) <= 0.0f)
	{
		_touchPlayerId = -1;
		_touchPlayerTime = 0.0f;
	}

	if (_maxTimeLife > 0 && _timeLife > _maxTimeLife)
		Death();

	_behaviors->OnProgress(deltaTime);
}

void GameObject::OnLateProgress(float deltaTime, bool pxStep)
{
	if (_bodyProgressEvent && pxStep)
		OnPxSync(1.0f);
}

void GameObject::OnFrame(float deltaTime, float pxAlpha)
{
	if (_bodyProgressEvent && pxAlpha != -1.0f)
		OnPxSync(pxAlpha);
	//OnPxSync(1.0f);

	if (_syncFrameEvent)
	{
		if (_posSyncLength > 0 && _posSyncLength < 5.0f)
		{
			_posSyncLength = std::max(_posSyncLength - 5.0f * deltaTime, 0.0f);
			_grActor->SetPos(_grActor->GetPos() - _posSyncDir * _posSyncLength);
		}
		else
			_posSyncLength = 0.0f;

		if (_rotSyncAngle != 0)
		{
			if (_rotSyncAngle > 0)
			{
				_rotSyncAngle = std::max(_rotSyncAngle - 1.3f * glm::pi<float>() * deltaTime, 0.0f);
			}
			else
			{
				_rotSyncAngle = std::min(_rotSyncAngle + 1.3f * glm::pi<float>() * deltaTime, 0.0f);
			}
			glm::quat rot = glm::angleAxis(-_rotSyncAngle, _rotSyncAxis);
			_grActor->SetRot(rot * _grActor->GetRot());
		}

		if (_posSyncDist2 > 0 && _posSyncDist2 < 5.0f)
		{
			_posSyncDist2 = std::max(_posSyncDist2 - 5.0f * deltaTime, 0.0f);
			_grActor->SetPos(_grActor->GetPos() + _posSync2 + _posSyncDir2 * _posSyncDist2);
		}
		else if (_posSyncLength2 > 0)
		{
			_posSyncDist2 = 0.0f;
			_grActor->SetPos(_grActor->GetPos() + _posSync2);
		}

		if (_rotSyncAngle2 != 0)
		{
			if (_rotSyncAngle2 > 0)
			{
				_rotSyncAngle2 = std::max(_rotSyncAngle2 - 1.3f * glm::pi<float>() * deltaTime, 0.0f);
			}
			else
			{
				_rotSyncAngle2 = std::min(_rotSyncAngle2 + 1.3f * glm::pi<float>() * deltaTime, 0.0f);
			}

			glm::quat rot = glm::angleAxis(-_rotSyncAngle2, _rotSyncAxis2);
			_grActor->SetRot(rot * _rotSync2 * _grActor->GetRot());
		}
		else if (_rotSyncLength2 != 0)
		{
			_rotSyncAngle2 = 0.0f;
			_grActor->SetRot(_rotSync2 * _grActor->GetRot());
		}
	}
}

void GameObject::OnFixedStep(float deltaTime)
{
}

void GameObject::SaveSource(lsl::SWriter* writer)
{
	writer->WriteValue("grActor", _grActor);
	writer->WriteValue("pxActor", _pxActor);

	writer->WriteValue("maxLife", _maxLife);

	//������� ��������, ����. ������. ���� ��� ������������� � ����� �����������
	//if (GetOwner() != GetParent())
	//	writer->WriteRef("parent", _parent);
}

void GameObject::LoadSource(lsl::SReader* reader)
{
	reader->ReadValue("grActor", _grActor);
	reader->ReadValue("pxActor", _pxActor);

	reader->ReadValue("maxLife", _maxLife);

	//reader->ReadRef("parent", false, this, 0);
}

void GameObject::SaveProxy(lsl::SWriter* writer)
{
	writer->WriteValue("pos", glm::value_ptr(GetPos()), 3);
	writer->WriteValue("scale", glm::value_ptr(GetScale()), 3);
	writer->WriteValue("rot", glm::value_ptr(GetRot()), 4);

	writer->WriteValue("life", _life);
	writer->WriteValue("maxTimeLife", _maxTimeLife);
	writer->WriteValue("timeLife", _timeLife);

	_behaviors->storeSource = true;
	_behaviors->storeProxy = true;
	writer->WriteValue("behaviors", _behaviors);
}

void GameObject::LoadProxy(lsl::SReader* reader)
{
	glm::vec3 pos;
	glm::vec3 scale;
	glm::quat rot;

	reader->ReadValue("pos", glm::value_ptr(pos), 3);
	reader->ReadValue("scale", glm::value_ptr(scale), 3);
	reader->ReadValue("rot", glm::value_ptr(rot), 4);

	reader->ReadValue("life", _life);
	reader->ReadValue("maxTimeLife", _maxTimeLife);
	reader->ReadValue("timeLife", _timeLife);

	SetPos(pos);
	SetScale(scale);
	SetRot(rot);

	_behaviors->storeSource = true;
	_behaviors->storeProxy = true;
	reader->ReadValue("behaviors", _behaviors);
}

void GameObject::Save(lsl::SWriter* writer)
{
	if (storeSource)
		SaveSource(writer);
	if (storeProxy)
		SaveProxy(writer);

	_includeList->SetStoreOpt(storeSource, storeProxy);
	writer->WriteValue("includeList", _includeList);
}

void GameObject::Load(lsl::SReader* reader)
{
	if (storeSource)
		LoadSource(reader);
	if (storeProxy)
		LoadProxy(reader);

	_includeList->SetStoreOpt(storeSource, storeProxy);
	reader->ReadValue("includeList", _includeList);
}

void GameObject::OnFixUp(const FixUpNames& fixUpNames)
{
	_MyBase::OnFixUp(fixUpNames);

	/*for (FixUpNames::const_iterator iter = fixUpNames.begin(); iter != fixUpNames.end(); ++iter)
		if (iter->name == "parent")
			SetParent(iter->GetComponent<GameObject*>());*/
}

NxActor* GameObject::GetNxActor()
{
	NxActor* nxActor = _pxActor->GetNxActor();
	if (!nxActor)
		throw lsl::Error("GameObject::GetNxActor(), not initializated");

	return nxActor;
}

void GameObject::Assign(GameObject* value)
{
	LSL_ASSERT(value);

	storeName = value->storeName;
	SetLogic(value->GetLogic());
}

void GameObject::InsertChild(GameObject* value)
{
	LSL_ASSERT(!value->_parent);

	value->_parent = this;
	_children.push_back(value);
	value->_grActor->SetParent(_grActor);
	value->_pxActor->SetParent(_pxActor);

	value->SetLogic(_logic);
}

void GameObject::RemoveChild(GameObject* value)
{
	LSL_ASSERT(value->_parent == this);

	value->_parent = 0;
	_children.remove(value);
	value->_grActor->SetParent(0);
	value->_pxActor->SetParent(0);
}

void GameObject::ClearChildren()
{
	while (!_children.empty())
		RemoveChild(*_children.begin());
}

void GameObject::InsertListener(GameObjListener* value)
{
	LSL_ASSERT(value != this && _listenerList.IsFind(value) == false);

	value->AddRef();
	_listenerList.Insert(value);
}

void GameObject::RemoveListener(GameObjListener* value)
{
	value->Release();

	_listenerList.Remove(value);
}

void GameObject::ClearListenerList()
{
	for (ListenerList::const_iterator iter = _listenerList.begin(); iter != _listenerList.end(); ++iter)
		(*iter)->Release();

	_listenerList.Clear();
}

void GameObject::Death(DamageType damageType, GameObject* target)
{
	if (_liveState != lsDeath)
	{
		DoDeath(damageType, target);

		SendDeath(damageType, target);
	}
}

void GameObject::Resc()
{
	if (_liveState != lsLive)
	{
		_liveState = lsLive;

		SetTimeLife(0);
		SetLife(GetMaxLife());
	}
}

void GameObject::Damage(int senderPlayerId, float value, float newLife, bool death, DamageType damageType)
{
	MyEventData data;
	data.target = this;
	data.targetPlayerId = GetMapObj() && GetMapObj()->GetPlayer() ? GetMapObj()->GetPlayer()->GetId() : cUndefPlayerId;
	data.damage = value;
	data.damageType = damageType;

	_life = newLife;

	if (_liveState != lsDeath)
	{
		for (ListenerList::Position pos = _listenerList.First(); GameObjListener** iter = _listenerList.Current(pos); _listenerList.Next(pos))
			(*iter)->OnDamage(this, value, damageType);

		if (senderPlayerId != cUndefPlayerId)
		{
			if (damageType == dtTouch)
			{
				_touchPlayerId = senderPlayerId;
				_touchPlayerTime = 3.0f;
			}
		}

		//not immortal
		if (_maxLife >= 0.0f)
			SendEvent(cPlayerDamage, senderPlayerId, &data);

		if (death)
		{
			if (damageType != dtTouch)
			{
				_touchPlayerId = cUndefPlayerId;
				_touchPlayerTime = 0.0f;
			}

			DoDeath(damageType);

			if (IsCar() && senderPlayerId != cUndefPlayerId)
			{
				if (damageType != dtMine)
					SendEvent(cPlayerKill, senderPlayerId, &data);
			}

			SendDeath(damageType);
		}
	}
}

void GameObject::Damage(int senderPlayerId, float value, DamageType damageType)
{
	float newLife = !IsImmortal() ? _life - value : _life;
	bool death = !IsImmortal() && newLife <= 0.0f;

	Damage(senderPlayerId, value, newLife, death, damageType);
}

void GameObject::Healt(float life)
{
	_life = std::min(_life + life, _maxLife);
}

void GameObject::LowLife(Behavior* behavior)
{
	OnLowLife(this, behavior);

	for (ListenerList::Position pos = _listenerList.First(); GameObjListener** iter = _listenerList.Current(pos); _listenerList.Next(pos))
		(*iter)->OnLowLife(this, behavior);
}

bool GameObject::GetImmortalFlag() const
{
	return _immortalFlag;
}

void GameObject::SetImmortalFlag(bool value)
{
	_immortalFlag = value;
}

void GameObject::Immortal(float time)
{
	bool onStatus = _immortalTime == 0;
	_immortalTime = time;

	if (onStatus)
	{
		OnImmortalStatus(true);
		_behaviors->OnImmortalStatus(true);
	}
}

bool GameObject::IsImmortal() const
{
	return _maxLife < 0 || _immortalTime > 0 || _immortalFlag;
}

MapObj* GameObject::GetMapObj()
{
	return _mapObj;
}

Logic* GameObject::GetLogic()
{
	return _logic;
}

void GameObject::SetLogic(Logic* value)
{
	if (ReplaceRef(_logic, value))
	{
		if (_logic)
		{
			if (_frameEventCount > 0)
				_logic->UnregFrameEvent(this);
			//if (_progressEventCount > 0)
			//	_logic->UnregProgressEvent(this);
			if (_lateProgressEventCount > 0)
				_logic->UnregLateProgressEvent(this);
			if (_fixedStepEventCount > 0)
				_logic->UnregFixedStepEvent(this);

			LogicReleased();
		}

		_logic = value;

		for (Children::iterator iter = _children.begin(); iter != _children.end(); ++iter)
			(*iter)->SetLogic(value);

		if (_logic)
		{
			if (_frameEventCount > 0)
				_logic->RegFrameEvent(this);
			//if (_progressEventCount > 0)
			//	_logic->RegProgressEvent(this);
			if (_lateProgressEventCount > 0)
				_logic->RegLateProgressEvent(this);
			if (_fixedStepEventCount > 0)
				_logic->RegFixedStepEvent(this);

			LogicInited();
		}
	}
}

void GameObject::SendEvent(unsigned id, int playerId, MyEventData* data)
{
	if (_logic == NULL || GetMapObj() == NULL)
		return;

	GameMode* game = _logic->GetMap()->GetWorld()->GetGame();

	if (data)
		data->playerId = playerId;

	game->SendEvent(id, data ? data : &MyEventData(playerId));
}

void GameObject::SendEvent(unsigned id, MyEventData* data)
{
	SendEvent(id, GetMapObj()->GetPlayer() ? GetMapObj()->GetPlayer()->GetId() : cUndefPlayerId, data);
}

GameObject* GameObject::GetParent()
{
	return _parent;
}

void GameObject::SetParent(GameObject* value)
{
	if (_parent != value)
	{
		if (_parent)
			_parent->RemoveChild(this);
		if (value)
			value->InsertChild(this);
	}
}

const GameObject::Children& GameObject::GetChildren() const
{
	return _children;
}

GameObject::IncludeList& GameObject::GetIncludeList()
{
	return *_includeList;
}

graph::Actor& GameObject::GetGrActor()
{
	return *_grActor;
}

px::Actor& GameObject::GetPxActor()
{
	return *_pxActor;
}

Proj* GameObject::IsProj()
{
	return 0;
}

GameCar* GameObject::IsCar()
{
	return 0;
}

const glm::vec3& GameObject::GetPos() const
{
	return _grActor->GetPos();
}

void GameObject::SetPos(const glm::vec3& value)
{
	_grActor->SetPos(value);
	_pxActor->SetPos(value);
	_pxPrevPos = value;
}

const glm::vec3& GameObject::GetScale() const
{
	return _grActor->GetScale();
}

void GameObject::SetScale(const glm::vec3& value)
{
	float len = glm::length(value);

	_grActor->SetScale(value);
	_pxActor->SetScale(value);
}

void GameObject::SetScale(float value)
{
	SetScale(glm::vec3(value, value, value));
}

const glm::quat& GameObject::GetRot() const
{
	return _grActor->GetRot();
}

void GameObject::SetRot(const glm::quat& value)
{
	_grActor->SetRot(value);
	_pxActor->SetRot(value);
	_pxPrevRot = value;
}

glm::vec3 GameObject::GetWorldPos() const
{
	return _grActor->GetWorldPos();
}

void GameObject::SetWorldPos(const glm::vec3& value)
{
	_grActor->SetWorldPos(value);
	_pxActor->SetPos(_grActor->GetPos());
	_pxPrevPos = _grActor->GetPos();
}

glm::quat GameObject::GetWorldRot() const
{
	return _grActor->GetWorldRot();
}

void GameObject::SetWorldRot(const glm::quat& value)
{
	_grActor->SetWorldRot(value);
	_pxActor->SetRot(_grActor->GetRot());
	_pxPrevRot = _grActor->GetRot();
}

void GameObject::SetWorldDir(const glm::vec3& value)
{
	glm::vec3 vec3 = value;
	if (_grActor->GetParent())
		_grActor->GetParent()->WorldToLocalNorm(vec3, vec3);

	_grActor->SetDir(vec3);
	_pxActor->SetRot(_grActor->GetRot());
	_pxPrevRot = _grActor->GetRot();
}

void GameObject::SetWorldUp(const glm::vec3& value)
{
	glm::vec3 vec3 = value;
	if (_grActor->GetParent())
		_grActor->GetParent()->WorldToLocalNorm(vec3, vec3);

	_grActor->SetUp(vec3);
	_pxActor->SetRot(_grActor->GetRot());
	_pxPrevRot = _grActor->GetRot();
}

const glm::vec3& GameObject::GetPosSync() const
{
	return _posSync;
}

void GameObject::SetPosSync(const glm::vec3& value)
{
	_posSync = value;
	_posSyncDir = glm::normalize(value);
	_posSyncLength = glm::length(value);

	SetSyncFrameEvent(true);
}

const glm::quat& GameObject::GetRotSync() const
{
	return _rotSync;
}

void GameObject::SetRotSync(const glm::quat& value)
{
	_rotSync = value;
	_rotSyncAxis = glm::axis(_rotSync);
	_rotSyncAngle = glm::angle(_rotSync);
	float angle = std::abs(_rotSyncAngle);

	if (angle > glm::pi<float>())
	{
		_rotSyncAngle = (2 * glm::pi<float>() - angle) * (_rotSyncAngle > 0 ? -1.0f : 1.0f);
	}

	SetSyncFrameEvent(true);
}

const glm::vec3& GameObject::GetPosSync2() const
{
	return _posSync2;
}

void GameObject::SetPosSync2(const glm::vec3& curSync, const glm::vec3& newSync)
{
	_posSyncDir2 = curSync - newSync;
	_posSyncDist2 = glm::length(_posSyncDir2);
	_posSyncDir2 = _posSyncDist2 != 0.0f ? _posSyncDir2 / _posSyncDist2 : NullVector;

	_posSync2 = newSync;
	_posSyncLength2 = glm::length(_posSync2);

	SetSyncFrameEvent(true);
}

const glm::quat& GameObject::GetRotSync2() const
{
	return _rotSync2;
}

void GameObject::SetRotSync2(const glm::quat& curSync, const glm::quat& newSync)
{
	glm::quat dRot = QuatRotation(newSync, curSync);
	_rotSyncAxis2 = glm::axis(dRot);
	_rotSyncAngle2 = glm::angle(dRot);

	float angle = std::abs(_rotSyncAngle2);
	if (angle > glm::pi<float>())
	{
		_rotSyncAngle2 = (2 * glm::pi<float>() - angle) * (_rotSyncAngle2 > 0 ? -1.0f : 1.0f);
	}

	_rotSync2 = newSync;
	glm::vec3 axis = glm::axis(_rotSync2);
	_rotSyncLength2 = glm::angle(_rotSync2);

	SetSyncFrameEvent(true);
}

const glm::vec3& GameObject::GetPxPosLerp() const
{
	return !_bodyProgressEvent && _parent ? _parent->GetPxPosLerp() : _pxPosLerp;
}

const glm::quat& GameObject::GetPxRotLerp() const
{
	return !_bodyProgressEvent && _parent ? _parent->GetPxRotLerp() : _pxRotLerp;
}

const glm::vec3& GameObject::GetPxVelocityLerp() const
{
	return !_bodyProgressEvent && _parent ? _parent->GetPxVelocityLerp() : _pxVelocityLerp;
}

const glm::vec3& GameObject::GetPxPrevPos() const
{
	return _pxPrevPos;
}

const glm::quat& GameObject::GetPxPrevRot() const
{
	return _pxPrevRot;
}

const glm::vec3& GameObject::GetPxPrevVelocity() const
{
	return _pxPrevVelocity;
}

GameObject::LiveState GameObject::GetLiveState() const
{
	return _liveState;
}

float GameObject::GetMaxLife() const
{
	return _maxLife;
}

void GameObject::SetMaxLife(float value)
{
	_life = _maxLife = value;
}

float GameObject::GetTimeLife() const
{
	return _timeLife;
}

void GameObject::SetTimeLife(float value)
{
	_timeLife = value;
}

float GameObject::GetMaxTimeLife() const
{
	return _maxTimeLife;
}

void GameObject::SetMaxTimeLife(float value)
{
	_maxTimeLife = value;
	_timeLife = 0;
}

float GameObject::GetLife() const
{
	return _life;
}

void GameObject::SetLife(float value)
{
	_life = value;
}

int GameObject::GetTouchPlayerId() const
{
	return _touchPlayerId;
}

Behaviors& GameObject::GetBehaviors()
{
	return *_behaviors;
}

GameObject* GameObject::GetGameObjFromActor(px::Actor* actor)
{
	return lsl::StaticCast<GameObject*>(actor->GetOwner());
}

GameObject* GameObject::GetGameObjFromActor(NxActor* actor)
{
	px::Actor* pxActor = px::Scene::GetActorFromNx(actor);

	return pxActor ? GetGameObjFromActor(pxActor) : 0;
}

GameObject* GameObject::GetGameObjFromShape(NxShape* shape)
{
	return GetGameObjFromActor(&shape->getActor());
}

}

}