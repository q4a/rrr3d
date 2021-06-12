#include "stdafx.h"
#include "game\\Weapon.h"

#include "game\\Logic.h"
#include "game\\Race.h"

namespace r3d
{

namespace game
{
 
Proj::Proj(): _pxBox(0), _ignoreContactProj(false), _model(0), _model2(0), _weapon(0), _playerId(cUndefPlayerId), _sprite(0), _tick1(0), _time1(0), _state1(false), _vec1(NullVector)
{	
}

Proj::~Proj()
{
	Destroy();

	FreeSprite();
	FreeModel2(false);
	FreeModel(false);
	SetWeapon(0);

	SetShot(ShotDesc());
}

void Proj::LocateProj(GameObject* weapon, bool pos, bool rot, const D3DXVECTOR3* speed)
{
	if (pos)
	{
		D3DXVECTOR3 pos = _desc.pos;
		if (weapon)
			weapon->GetGrActor().LocalToWorldCoord(pos, pos);
		this->SetPos(pos);
	}

	if (rot)
	{
		D3DXQUATERNION rot = _desc.rot;

		if (speed && D3DXVec3Length(speed) > 1.0f)
		{
			D3DXVECTOR3 dir;
			D3DXVec3Normalize(&dir, speed);
			rot = rot * weapon->GetRot() * QuatShortestArc(XVector, dir);
		}
		else if (weapon)
			rot = rot * weapon->GetWorldRot();

		this->SetRot(rot);
	}
}

void Proj::InitModel()
{
	if (_model)
		return;

	if (_desc.GetModel())
	{
		_model = &GetIncludeList().Add(_desc.GetModel());
		_model->AddRef();
		_model->GetGameObj().InsertListener(this);
	}
}

void Proj::FreeModel(bool remove)
{
	if (_model)
	{
		_model->GetGameObj().RemoveListener(this);
		if (remove)
			GetIncludeList().Delete(_model);

		lsl::SafeRelease(_model);		
	}	
}

void Proj::InitModel2()
{
	if (_model2)
		return;

	if (_desc.GetModel2())
	{
		_model2 = &GetIncludeList().Add(_desc.GetModel2());
		_model2->AddRef();
		_model2->GetGameObj().InsertListener(this);
	}
}

void Proj::FreeModel2(bool remove)
{
	if (_model2)
	{
		_model2->GetGameObj().RemoveListener(this);
		if (remove)
			GetIncludeList().Delete(_model2);

		lsl::SafeRelease(_model2);
	}
}

px::Body* Proj::CreateBody(const NxBodyDesc& desc)
{
	NxBodyDesc body = desc;
	body.mass = _desc.mass;

	this->GetPxActor().SetBody(&body);

	return this->GetPxActor().GetBody();
}

graph::Sprite* Proj::CreateSprite()
{
	if (!_sprite)
	{
		_sprite = &GetGrActor().GetNodes().Add<graph::Sprite>();
		_sprite->AddRef();
	}

	return _sprite;
}

void Proj::FreeSprite()
{
	if (_sprite)
	{
		_sprite->Release();
		GetGrActor().GetNodes().Delete(_sprite);
	}
}

void Proj::InsertProjToGraph(GraphManager* graph)
{
	graph::Actor::GraphDesc desc;
	desc.lighting = graph::Actor::glStd;
	desc.order = graph::Actor::goEffect;
	desc.props.set(graph::Actor::gpColor);
	desc.props.set(graph::Actor::gpDynamic);
	//desc.props.set(graph::Actor::gpMorph);	

	GetGrActor().SetGraph(graph, desc);
}

AABB Proj::ComputeAABB(bool onlyModel)
{
	AABB aabb(NullVector);
	//необходимо быть осторожней со спецэфф.
	if (_desc.modelSize && _model)
	{
		if (!onlyModel)
		{
			aabb = AABB(_desc.size);
			aabb.Offset(_desc.offset);
		}

		aabb.Add(_model->GetGameObj().GetGrActor().GetLocalAABB(false));
	} 
	else if (onlyModel)
		aabb = AABB(IdentityVector * 0.1f);
	else
	{
		aabb = AABB(_desc.size);
		aabb.Offset(_desc.offset);
	}

	return aabb;
}

void Proj::CreatePxBox(NxCollisionGroup group)
{
	AABB aabb = ComputeAABB(false);

	NxBoxShapeDesc boxDesc;
	boxDesc.dimensions = NxVec3(aabb.GetSizes()/2.0f);
	boxDesc.localPose.t.set(aabb.GetCenter());
	boxDesc.group = group;
	this->_pxBox = &this->GetPxActor().GetShapes().Add<px::BoxShape>();
	this->_pxBox->AssignFromDesc(boxDesc);
}

void Proj::AddContactForce(GameObject* target, const D3DXVECTOR3& point, const D3DXVECTOR3& force, NxForceMode mode)
{
	target->GetPxActor().GetNxActor()->addForceAtPos(NxVec3(force), NxVec3(point), mode);
}

void Proj::AddContactForce(GameObject* target, const px::Scene::OnContactEvent& contact, const D3DXVECTOR3& force, NxForceMode mode)
{
	D3DXVECTOR3 point = GetContactPoint(contact);
	AddContactForce(target, point, force, mode);
}

void Proj::SetWeapon(GameObject* weapon)
{
	if (ReplaceRef(_weapon, weapon))
	{
		if (_weapon)
		{
			_weapon->RemoveListener(this);
			if (_weapon == GetParent())
				SetParent(0);
			_playerId = cUndefPlayerId;
		}

		_weapon = weapon;

		if (_weapon)
		{
			weapon->InsertListener(this);
			
			GameObject* car = _weapon->GetParent() ? _weapon->GetParent() : NULL;
			_playerId = car && car->GetMapObj() && car->GetMapObj()->GetPlayer() ? car->GetMapObj()->GetPlayer()->GetId() : cUndefPlayerId;
		}
	}
}

void Proj::LinkToWeapon()
{
	if (_weapon && _weapon != this)
	{
		SetParent(_weapon);
		SetPos(_desc.pos);
		SetRot(_desc.rot);
	}
}

void Proj::SetIgnoreContactProj(bool value)
{
	_ignoreContactProj = value;
}

void Proj::SetShot(const ShotDesc& value)
{
	if (_shot.GetTargetMapObj())
	{
		_shot.GetTargetMapObj()->GetGameObj().RemoveListener(this);		
	}

	_shot = value;

	if (_shot.GetTargetMapObj())
	{
		_shot.GetTargetMapObj()->GetGameObj().InsertListener(this);		
	}
}

void Proj::DamageTarget(GameObject* target, float damage, DamageType damageType)
{
	LSL_ASSERT(target);

	GameObject* car = _weapon && _weapon->GetParent() ? _weapon->GetParent() : NULL;

	int playerId = _playerId;

	//self damage if plr undef
	//if (playerId == cUndefPlayerId && target && target->GetMapObj() && target->GetMapObj()->GetPlayer())
	//	playerId = target->GetMapObj()->GetPlayer()->GetId();

	GetLogic()->Damage(car, playerId, target, damage, damageType);
}

MapObj* Proj::FindNextTaget(float viewAngle)
{
	if (GetLogic() == NULL || _shot.GetTargetMapObj() == NULL)
		return NULL;

	Player* player = GetLogic()->GetRace()->GetPlayerByMapObj(_shot.GetTargetMapObj());
	if (player == NULL)
		return NULL;

	Player* nextPlayer = player->FindClosestEnemy(viewAngle, false);
	if (nextPlayer == NULL)
		return NULL;

	if (_weapon && nextPlayer->GetCar().gameObj == _weapon->GetParent())
	{
		nextPlayer = nextPlayer->FindClosestEnemy(viewAngle, false);
		if (nextPlayer == 0 || nextPlayer == player)
			return NULL;
	}

	return nextPlayer->GetCar().mapObj;
}

void Proj::EnableFilter(GameObject* target, unsigned mask)
{
	if (target->GetPxActor().GetNxActor() == 0)
		return;

	NxGroupsMask nxMask;
	nxMask.bits0 = mask;
	nxMask.bits1 = nxMask.bits2 = nxMask.bits3 = 0;
	for (unsigned i = 0; i < target->GetPxActor().GetNxActor()->getNbShapes(); ++i)
	{
		NxShape* shape = target->GetPxActor().GetNxActor()->getShapes()[i];
		shape->setGroupsMask(nxMask);
	}
	
	target->GetPxActor().GetScene()->GetNxScene()->setFilterOps(NX_FILTEROP_OR, NX_FILTEROP_OR, NX_FILTEROP_AND);
}

void Proj::DisableFilter(GameObject* target)
{
	if (target->GetPxActor().GetNxActor() == 0)
		return;

	NxGroupsMask nxMask;
	nxMask.bits0 = px::Scene::gmDef;
	nxMask.bits1 = nxMask.bits2 = nxMask.bits3 = 0;
	for (unsigned i = 0; i < target->GetPxActor().GetNxActor()->getNbShapes(); ++i)
	{
		NxShape* shape = target->GetPxActor().GetNxActor()->getShapes()[i];
		shape->setGroupsMask(nxMask);
	}

	target->GetPxActor().GetScene()->GetNxScene()->setFilterOps(NX_FILTEROP_AND, NX_FILTEROP_AND, NX_FILTEROP_AND);
}

D3DXVECTOR3 Proj::CalcSpeed(GameObject* weapon)
{
	D3DXVECTOR3 dir = weapon->GetGrActor().GetWorldDir();
	float speed = _desc.speed;

	if (_desc.speedRelative)
	{
		speed += std::max(D3DXVec3Dot(&dir, &weapon->GetPxVelocityLerp()), 0.0f);
	}
	else if (_desc.speedRelativeMin > 0)
	{
		speed = std::max(speed, _desc.speedRelativeMin + std::max(D3DXVec3Dot(&dir, &weapon->GetPxVelocityLerp()), 0.0f));
	}

	float cosa = abs(D3DXVec3Dot(&dir, &D3DXVECTOR3(0, 0, 1)));	
	if (cosa < 0.707f)
	{
		dir.z = 0;
		D3DXVec3Normalize(&dir, &dir);
	}

	return dir * speed;
}

bool Proj::RocketPrepare(GameObject* weapon, bool disableGravity, D3DXVECTOR3* speedVec, NxCollisionGroup pxGroup)
{
	_vec1.z = 0.0f;

	D3DXVECTOR3 speed = CalcSpeed(weapon);
	if (speedVec)
		*speedVec = speed;

	LocateProj(weapon, true, true, &speed);
	InitModel();
	CreatePxBox(pxGroup);

	NxBodyDesc bodyDesc;
	bodyDesc.flags |= disableGravity ? NX_BF_DISABLE_GRAVITY : 0;
	bodyDesc.linearVelocity = NxVec3(speed);

	CreateBody(bodyDesc);

	this->GetPxActor().SetContactReportFlags(NX_NOTIFY_ALL);
	this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);
	SetIgnoreContactProj(true);

	return true;
}

void Proj::RocketContact(const px::Scene::OnContactEvent& contact)
{
	GameObject* target = GetGameObjFromActor(contact.actor);
	if (target == NULL)
		return;

	MapObjRec* record = target->GetMapObj() ? target->GetMapObj()->GetRecord() : NULL;
	RecordNode* recordParent = record ? record->GetParent() : NULL;

	if ((record && record->GetCategory() == MapObjLib::ctCar) || (recordParent && recordParent->GetName() == "Crush"))
	{
		this->Death(dtSimple, target);

		DamageTarget(target, _desc.damage);

		D3DXVECTOR3 dir = D3DXVECTOR3(this->GetPxActor().GetNxActor()->getLinearVelocity().get());
		float dirLength = D3DXVec3Length(&dir);

		if (dirLength > 1.0f)
		{
			dir /= dirLength;

			D3DXVECTOR3 contactDir = GetContactPoint(contact);
			//D3DXVec3Normalize(&contactDir, &contactDir);	
			D3DXVec3Cross(&contactDir, &contactDir, &dir);
			
			//NxVec3 vec3(RandomRange(-1.0f, 1.0f), 0, RandomRange(-1.0f, 1.0f));
			NxVec3 vec3(contactDir);
			if (vec3.magnitude() > 0.01f)
			{
				vec3.normalize();
				target->GetPxActor().GetNxActor()->addLocalTorque(vec3 * _desc.mass * 0.2f, NX_VELOCITY_CHANGE);
			}

			D3DXVec3Normalize(&dir, &(dir - ZVector));

			//dir = dir * dirLength;
			//AddContactForce(target, contact, 150.0f * dir, NX_IMPULSE);
		}
	}
}

void Proj::RocketUpdate(float deltaTime)
{
	const float cTrackHeight = 4.0f;

	D3DXVECTOR3 size = _pxBox->GetDimensions();
	NxVec3 pos = GetPxActor().GetNxActor()->getGlobalPosition(); 
	NxRay nxRay(pos + NxVec3(0, 0, cTrackHeight), NxVec3(0, 0, -1.0f));

	NxRaycastHit hit;			
	NxShape* hitShape = GetLogic()->GetPxScene()->GetNxScene()->raycastClosestShape(nxRay, NX_STATIC_SHAPES, hit, 1 << px::Scene::cdgTrackPlane, NX_MAX_F32, NX_RAYCAST_SHAPE | NX_RAYCAST_IMPACT);

	if (hitShape)
	{
		GameObject* tes = GetGameObjFromShape(hitShape);
		NxCollisionGroup gr = hitShape->getGroup();

		float height = std::max(pos.z - hit.worldImpact.z, size.z);
		if (_vec1.z == 0.0f)
			_vec1.z = height;
		else if (_vec1.z - height > 0.1f)
			_vec1.z = height;

		pos.z = hit.worldImpact.z + _vec1.z;
		GetPxActor().GetNxActor()->setGlobalPosition(pos);
	}
}

bool Proj::HyperPrepare(GameObject* weapon)
{
	LocateProj(weapon, true, true, NULL);
	InitModel();
	LinkToWeapon();

	weapon->GetPxActor().GetNxActor()->addLocalForce(NxVec3(1.0f, 0.0f, 0.0f) * _desc.speed, NX_SMOOTH_VELOCITY_CHANGE);

	return true;
}

bool Proj::MedpackPrepare(GameObject* weapon)
{
	LocateProj(weapon, true, true, NULL);
	InitModel();
	CreatePxBox();

	this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);

	return true;
}

void Proj::MedpackContact(const px::Scene::OnContactEvent& contact)
{
	GameObject* target = GetGameObjFromActor(contact.actor);
	if (target)
	{
		GetLogic()->TakeBonus(target, this, btMedpack, _desc.damage > 0 ? _desc.damage : target->GetMaxLife());
	}
}

bool Proj::ChargePrepare(GameObject* weapon)
{
	return MedpackPrepare(weapon);
}

void Proj::ChargeContact(const px::Scene::OnContactEvent& contact)
{
	GameObject* target = GetGameObjFromActor(contact.actor);
	if (target)
	{
		GetLogic()->TakeBonus(target, this, btCharge, _desc.damage);
	}
}

bool Proj::MoneyPrepare(GameObject* weapon)
{
	return MedpackPrepare(weapon);
}

void Proj::MoneyContact(const px::Scene::OnContactEvent& contact)
{
	GameObject* target = GetGameObjFromActor(contact.actor);
	if (target)
	{
		GetLogic()->TakeBonus(target, this, btMoney, _desc.damage);
	}
}

bool Proj::ImmortalPrepare(GameObject* weapon)
{
	return MedpackPrepare(weapon);
}

void Proj::ImmortalContact(const px::Scene::OnContactEvent& contact)
{
	GameObject* target = GetGameObjFromActor(contact.actor);
	if (target)
	{
		GetLogic()->TakeBonus(target, this, btImmortal, _desc.damage);
	}
}

bool Proj::SpeedArrowPrepare(GameObject* weapon)
{
	LocateProj(weapon, true, true, NULL);
	InitModel();
	CreatePxBox();

	this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);

	return true;
}

void Proj::SpeedArrowContact(const px::Scene::OnContactEvent& contact)
{
	GameObject* target = GetGameObjFromActor(contact.actor);
	if (target)
	{
		target->GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(GetGrActor().GetWorldDir() * _desc.damage));
		target->SendEvent(cPlayerSpeedArrow);
	}
}

bool Proj::LushaPrepare(const ShotContext& ctx)
{
	LocateProj(_weapon, true, true, NULL);
	InitModel();
	CreatePxBox();

	this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);

	return true;
}

void Proj::LushaContact(const px::Scene::OnContactEvent& contact)
{
	GameObject* target = GetGameObjFromActor(contact.actor);
	if (target)
	{
		NxActor* nxTarget = target->GetPxActor().GetNxActor();

		NxVec3 linSpeed = nxTarget->getLinearVelocity();
		float maxSpeed = linSpeed.magnitude();
		linSpeed.normalize();

		if (maxSpeed > 1.0f && maxSpeed > _desc.damage)
		{
			nxTarget->setLinearVelocity(linSpeed * _desc.damage);
		}
	}
}

bool Proj::MinePrepare(const ShotContext& ctx, bool lockMine)
{
	_time1 = -1.0f;
	InitModel();
	CreatePxBox(px::Scene::cdgShotTrack);

	AABB aabb = ComputeAABB(true);
	GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);
	_time1 = 0.0f;	

	if (ctx.projMat)
	{
		SetWorldPos(ctx.projMat->t.get());

		return true;
	}
	else
	{
		D3DXVECTOR3 rayPos = _desc.pos;
		if (_weapon)
			_weapon->GetGrActor().LocalToWorldCoord(rayPos, rayPos);
		NxRay nxRay(NxVec3(rayPos) + NxVec3(0, 0, 2.0f), NxVec3(-ZVector));

		NxRaycastHit hit;
		NxShape* hitShape = GetLogic()->GetPxScene()->GetNxScene()->raycastClosestShape(nxRay, NX_STATIC_SHAPES, hit, (1 << px::Scene::cdgTrackPlane) | (1 << px::Scene::cdgShotTrack), NX_MAX_F32, NX_RAYCAST_SHAPE | NX_RAYCAST_IMPACT | NX_RAYCAST_NORMAL);

		if (hitShape && hitShape->getGroup() != px::Scene::cdgShotTrack) //&& hit.distance < _desc.projMaxDist)
		{
			float offs = std::max(-aabb.min.z, 0.01f);			
			D3DXVECTOR3 normal = hit.worldNormal.get();

			SetWorldPos(D3DXVECTOR3(hit.worldImpact.get()) + ZVector * offs);
			SetWorldUp(normal);

			if (lockMine)
			{
				GameCar* car = _weapon && _weapon->GetParent() ? _weapon->GetParent()->IsCar() : NULL;
				if (car)
					car->LockMine(0.4f);
			}

			return true;
		}
		else
			return false;
	}
}

void Proj::MineContact(const px::Scene::OnContactEvent& contact, bool testLockMine)
{
	GameObject* targetObj = GetGameObjFromActor(contact.actor);
	GameCar* target = targetObj ? targetObj->IsCar() : NULL;
	GameObject* car = _weapon ? _weapon->GetParent() : NULL;

	if (target == NULL || (testLockMine && target->IsMineLocked() && GetLogic() && GetLogic()->GetRace()->GetEnableMineBug()))
		return;

	if (_time1 == -1.0f || car != target)
		GetLogic()->MineContact(this, target, GetContactPoint(contact));
}

float Proj::MineUpdate(float deltaTime, float delay)
{
	if (_time1 >= 0)
	{
		_time1 += deltaTime;
		float alpha = ClampValue(delay > 0 ? _time1/delay : 1.0f, 0.0f, 1.0f);
		if (alpha == 1.0f)
			_time1 = -1.0f;

		return alpha;
	}

	return -1.0f;
}

bool Proj::MasloPrepare(const ShotContext& ctx)
{
	if (MinePrepare(ctx, true))
	{		
		_model->GetGameObj().SetScale(0.0f);
		return true;
	}

	return false;
}

void Proj::MasloContact(const px::Scene::OnContactEvent& contact)
{
	GameObject* target = GetGameObjFromActor(contact.actor);

	if (target)
	{
		GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().GetNxActor() ? target->GetParent()->IsCar() : target->IsCar();

		if (car == NULL || _time1 >= 0 || car->IsMineLocked())
			return;

		D3DXPLANE plane;
		D3DXPlaneFromPointNormal(&plane, &car->GetGrActor().GetWorldPos(), &car->GetGrActor().GetWorldRight());
		float dist = PlaneDistToPoint(plane, GetWorldPos());

		if (car->GetPxActor().GetNxActor()->getLinearVelocity().magnitude() > 3.0f)
			car->LockClutch(abs(dist) > 0.1f && dist > 0 ? -_desc.damage : _desc.damage);
	}
}

void Proj::MasloUpdate(float deltaTime)
{
	float alpha = MineUpdate(deltaTime);

	if (_model && alpha >= 0)
		_model->GetGameObj().SetScale(alpha);
}

bool Proj::MineRipPrepare(const ShotContext& ctx)
{
	return MinePrepare(ctx, true);
}

void Proj::MineRipContact(const px::Scene::OnContactEvent& contact)
{
	MineContact(contact, true);
}

void Proj::MineRipUpdate(float deltaTime)
{
	MineUpdate(deltaTime);

	if (GetTimeLife() > _desc.angleSpeed && GetLiveState() != lsDeath)
	{
		if (_desc.GetModel2())
		{
			MapObj* mapObj = &GetLogic()->GetMap()->AddMapObj(_desc.GetModel2());
			mapObj->GetGameObj().SetPos(GetPos());
			mapObj->GetGameObj().SetRot(GetRot());
			mapObj->GetGameObj().SetScale(GetScale());
		}
		if (_desc.GetModel3())
		{
			Vec3Range vec(D3DXVECTOR3(-3.0f, -3.0f, 3.0f), D3DXVECTOR3(3.0f, 3.0f, 1.0f), Vec3Range::vdVolume);

			for (int i = 0; i < 5; ++i)
			{
				MapObj* mapObj = &GetLogic()->GetMap()->AddMapObj(_desc.GetModel3());
				mapObj->GetGameObj().SetPos(GetPos());
				mapObj->GetGameObj().SetRot(GetRot());
				mapObj->GetGameObj().SetScale(GetScale());

				NxVec3 dir(vec.GetValue());
				dir.normalize();
				mapObj->GetGameObj().GetPxActor().GetNxActor()->addForce(mapObj->GetGameObj().GetPxActor().GetBody()->GetDesc().mass * dir * 10.0f, NX_IMPULSE);

				//Player* player = GetLogic()->GetRace()->GetPlayerByMapObj(_shot.GetTargetMapObj());
				//if (player)
				//{
				//	player->InsertBonusProj();
				//}
			}
		}

		Death();
	}
}

bool Proj::MinePiecePrepare(const ShotContext& ctx)
{
	if (_weapon)
		LocateProj(_weapon, true, true, NULL);
	InitModel();
	CreatePxBox(px::Scene::cdgShotTrack);
	_time1 = -1.0f;

	NxBodyDesc bodyDesc;
	CreateBody(bodyDesc);

	GetPxActor().SetContactReportFlags(NX_NOTIFY_ALL);

	return true;
}

void Proj::MinePieceContact(const px::Scene::OnContactEvent& contact)
{
	MineContact(contact, false);
}

bool Proj::MineProtonPrepare(const ShotContext& ctx)
{
	return MinePrepare(ctx, true);
}

void Proj::MineProtonContact(const px::Scene::OnContactEvent& contact)
{
	MineContact(contact, false);
}

void Proj::MineProtonUpdate(float deltaTime)
{
	MineUpdate(deltaTime);
}

bool Proj::TorpedaPrepare(GameObject* weapon)
{
	_time1 = 0.4f;

	return RocketPrepare(weapon, true, &_vec1);
}

void Proj::TorpedaContact(const px::Scene::OnContactEvent& contact)
{
	RocketContact(contact);
}

void Proj::TorpedaUpdate(float deltaTime)
{
	_time1 = std::max(_time1 - deltaTime, 0.0f);

	MapObj* target = _shot.GetTargetMapObj();
	if (target && _time1 == 0.0f)
	{
		D3DXVECTOR3 targPos = target->GetGameObj().GetWorldPos();
		D3DXVECTOR3 pos = this->GetWorldPos();

		D3DXVECTOR3 dir = targPos - pos;
		float dist = D3DXVec3Length(&dir);				
		if (dist > 1.0f)
			D3DXVec3Normalize(&dir, &dir);
		else
			dir = this->GetGrActor().GetDir();
		
		D3DXQUATERNION rot, rot1;
		QuatShortestArc(XVector, dir, rot1);
		if (_desc.angleSpeed > 0)
			D3DXQuaternionSlerp(&rot, &this->GetRot(), &rot1, deltaTime * _desc.angleSpeed);
		else
			rot = rot1;

		Vec3Rotate(XVector, rot, dir);
		this->SetRot(rot);

		float speed = 0.0f;

		if (_desc.speedRelative)
		{
			speed = D3DXVec3Length(&_vec1);
		}
		else
		{
			speed = std::max(D3DXVec3Dot(&_vec1, &dir), _desc.speed);
		}

		_vec1 = dir * speed;
		
		this->GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(dir * std::max(_desc.speed, speed)));
	}
}

bool Proj::LaserPrepare(GameObject* weapon)
{
	LocateProj(weapon, true, true, NULL);
	InitModel();
	InitModel2();
	LinkToWeapon();

	SetIgnoreContactProj(true);

	return true;
}

GameObject* Proj::LaserUpdate(float deltaTime, bool distort)
{
	if (_weapon == 0)
		return 0;

	D3DXVECTOR3 shotPos = GetWorldPos();
	D3DXVECTOR3 shotDir = GetGrActor().GetWorldDir();
	float scaleLaser = _desc.maxDist;

	EnableFilter(_weapon, px::Scene::gmTemp);

	NxGroupsMask nxMask;
	nxMask.bits0 = px::Scene::gmTemp;
	nxMask.bits1 = 0;
	nxMask.bits2 = 0;
	nxMask.bits3 = 0;

	NxRaycastHit rayhit;	
	NxShape* hitShape = GetLogic()->GetPxScene()->GetNxScene()->raycastClosestShape(NxRay(NxVec3(shotPos + _desc.sizeAddPx), NxVec3(shotDir)), NX_ALL_SHAPES, rayhit, (1 << px::Scene::cdgDefault) | (1 << px::Scene::cdgShotTransparency) | (1 << px::Scene::cdgTrackPlane), _desc.maxDist, NX_RAYCAST_SHAPE | NX_RAYCAST_DISTANCE, &nxMask);
	GameObject* rayHitActor = hitShape ? GetGameObjFromShape(hitShape) : NULL;

	DisableFilter(_weapon);

	if (rayHitActor)
	{
		scaleLaser = std::min(rayhit.distance, _desc.maxDist);
		D3DXVECTOR3 hitPos = shotPos + shotDir * scaleLaser;
		if (_model2)
			_model2->GetGameObj().SetWorldPos(hitPos);

		if (scaleLaser < _desc.maxDist)
			DamageTarget(rayHitActor, deltaTime * _desc.damage, dtEnergy);
	}
	else if (_model2)
		_model2->GetGameObj().SetPos(XVector * scaleLaser);

	if (_model)
	{
		graph::Sprite* sprite = (graph::Sprite*)&_model->GetGameObj().GetGrActor().GetNodes().front();	
		D3DXVECTOR2 size = D3DXVECTOR2(scaleLaser, _desc.size.y);

		if (distort)
		{
			float alpha = lsl::ClampValue(GetTimeLife() / GetMaxTimeLife(), 0.0f, 1.0f);	
			//alpha = lsl::ClampValue((alpha - 0.0f)/0.5f + 0.5f, 0.0f, 1.5f) - lsl::ClampValue((alpha - 0.7f)/0.3f * 1.5f, 0.0f, 1.5f);
			alpha = lsl::ClampValue((alpha - 0.0f)/0.5f * 1.5f + 0.5f, 0.0f, 2.0f) - lsl::ClampValue((alpha - 0.6f)/0.4f * 2.0f, 0.0f, 2.0f);

			size.y = size.y * alpha;
			sprite->material.Get()->samplers[0].SetScale(D3DXVECTOR3(scaleLaser/10.0f, 1.0f, 1.0f));
		}

		sprite->SetPos(XVector * scaleLaser/2.0f);
		sprite->sizes = size;
	}

	return rayHitActor;
}

bool Proj::FirePrepare(GameObject* weapon)
{
	return RocketPrepare(weapon, true);
}

void Proj::FireContact(const px::Scene::OnContactEvent& contact)
{
	GameObject* target = GetGameObjFromActor(contact.actor);
	if (target)
	{
		DamageTarget(target, _desc.damage * contact.deltaTime);
	}
}

void Proj::FireUpdate(float deltaTime)
{
	if (_weapon && _weapon->GetPxActor().GetNxActor() && GetPxActor().GetNxActor())
	{
		GetPxActor().GetNxActor()->setLinearVelocity(_weapon->GetPxActor().GetNxActor()->getLinearVelocity());

		D3DXVECTOR3 pos;
		_weapon->GetGrActor().LocalToWorldCoord(_desc.pos, pos);
		SetPos(pos);

		SetRot(_weapon->GetWorldRot());
	}
}

bool Proj::DrobilkaPrepare(GameObject* weapon)
{
	_time1 = 0.0f;

	LocateProj(weapon, true, true, NULL);
	CreatePxBox();

	NxBodyDesc desc;
	CreateBody(desc);

	this->GetPxActor().SetContactReportFlags(NX_NOTIFY_ALL);
	this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);
	SetIgnoreContactProj(true);

	return true;
}

void Proj::DrobilkaContact(const px::Scene::OnContactEvent& contact)
{
	GameObject* target = GetGameObjFromActor(contact.actor);
	if (target)
	{
		_time1 = 0.5f;
		InitModel();
		if (_model)
		{		
			D3DXVECTOR3 pnt = GetContactPoint(contact);
			_model->GetGameObj().SetWorldPos(pnt);
		}

		DamageTarget(target, _desc.damage * contact.deltaTime);	
	}
}

void Proj::DrobilkaUpdate(float deltaTime)
{
	if (_weapon)
	{
		D3DXVECTOR3 pos;
		_weapon->GetGrActor().LocalToWorldCoord(_desc.pos, pos);
		SetWorldPos(pos);
		SetWorldRot(_weapon->GetWorldRot());

		D3DXQUATERNION rot;
		D3DXQuaternionRotationAxis(&rot, &XVector, _desc.angleSpeed * deltaTime);
		_weapon->SetRot(_weapon->GetRot() * rot);
	}

	if (_model && _model->GetGameObj().GetLiveState() != lsDeath && (_time1 -= deltaTime) <= 0)
	{
		_model->GetGameObj().Death();
		FreeModel(false);
	}
}

bool Proj::SonarPrepare(GameObject* weapon)
{
	return RocketPrepare(weapon);
}

void Proj::SonarContact(const px::Scene::OnContactEvent& contact)
{
	GameObject* target = GetGameObjFromActor(contact.actor);
	if (target)
	{
		DamageTarget(target, _desc.damage * contact.deltaTime, dtEnergy);
		AddContactForce(target, contact, _desc.mass * D3DXVECTOR3(this->GetPxActor().GetNxActor()->getLinearVelocity().get()), NX_IMPULSE);
	}
}

bool Proj::SpringPrepare(GameObject* weapon)
{
	LinkToWeapon();

	GameCar* car = weapon->GetParent()->IsCar();

	if (car && car->IsWheelsContact())
	{
		//static D3DXQUATERNION testRot;
		//_time1 = 0.0f;
		//_state1 = false;
		//_vec1.x = 0.0f;
		//testRot = car->GetRot();
		//SetMaxTimeLife(4.0f);

		car->GetPxActor().GetNxActor()->addLocalForce(NxVec3(0.0f, 0.0f, 1.0f) * _desc.speed, NX_SMOOTH_VELOCITY_CHANGE);
		car->LockSpring();
		return true;
	}

	return false;
}

void Proj::SpringUpdate(float deltaTime)
{
	/*GameCar* car = _weapon->GetParent()->IsCar();

	if (car && !car->IsAnyWheelContact())
	{
		float dAngle = 0.0f;

		if (!_state1)
		{
			_time1 += deltaTime;
			float alpha = lsl::ClampValue(_time1/0.1f, 0.0f, 1.0f);
			dAngle = -D3DX_PI * deltaTime * 1.0f;
			_vec1.x = dAngle;

			if (alpha == 1.0f)
				_state1 = true;
		}
		else
		{
			dAngle = D3DX_PI * deltaTime * 0.75f;
		}

		_vec1.x = _vec1.x + (dAngle - _vec1.x) * lsl::ClampValue(deltaTime * 6.0f, 0.0f, 1.0f);

		D3DXQUATERNION rot;
		D3DXQuaternionRotationAxis(&rot, &YVector, _vec1.x);
		car->SetRot(rot * car->GetRot());
	}
	else if (_state1 || _time1 > 0.0f)
	{
		Death();
		return;
	}*/
	
	/*GameCar* car = _weapon->GetParent()->IsCar();

	if (car && !car->IsAnyWheelContact())
	{
		float angle = 0.0f;

		_time1 += deltaTime;
		float alpha = lsl::ClampValue(_time1/1.0f, 0.0f, 1.0f);
		angle = -D3DX_PI/6 + (D3DX_PI/6 - (-D3DX_PI/6)) * alpha;

		D3DXQUATERNION rot;
		D3DXQuaternionRotationAxis(&rot, &YVector, angle);
		car->SetRot(rot * testRot);

		if (alpha == 1.0f)
			Death();
	}
	else if (_state1 || _time1 > 0.0f)
	{
		Death();
		return;
	}*/
}

bool Proj::FrostRayPrepare(GameObject* weapon)
{
	return LaserPrepare(weapon);
}

void Proj::FrostRayUpdate(float deltaTime)
{
	GameObject* target = LaserUpdate(deltaTime, false);
	SlowEffect* slow = target ? target->GetBehaviors().Find<SlowEffect>() : 0;
	
	if (target && target->IsCar() && slow == 0)
	{
		slow = &target->GetBehaviors().Add<SlowEffect>();
		slow->SetEffect(_desc.GetModel3());
	}
}

bool Proj::MortiraPrepare(GameObject* weapon)
{
	return RocketPrepare(weapon, false);
}

void Proj::MortiraContact(const px::Scene::OnContactEvent& contact)
{
	RocketContact(contact);
}

bool Proj::CraterPrepare(const ShotContext& ctx)
{
	return MinePrepare(ctx, false);
}

void Proj::CraterContact(const px::Scene::OnContactEvent& contact)
{
	GameObject* target = GetGameObjFromActor(contact.actor);
	if (target)
	{
		DamageTarget(target, _desc.damage * contact.deltaTime, dtMine);
	}
}

bool Proj::ImpulsePrepare(GameObject* weapon)
{
	_tick1 = 0;

	return TorpedaPrepare(weapon);
}

void Proj::ImpulseContact(const px::Scene::OnContactEvent& contact)
{
	GameObject* contactActor = GetGameObjFromActor(contact.actor);
	if (contactActor)
	{
		if (_shot.GetTargetMapObj() && contactActor == &_shot.GetTargetMapObj()->GetGameObj())
		{
			DamageTarget(contactActor, _desc.damage / (_tick1 + 1), dtEnergy);
			if (++_tick1 > 2)
			{
				this->Death(dtEnergy, contactActor);
				return;
			}

			MapObj* taget = FindNextTaget(D3DX_PI/2);
			if (taget == 0)
			{
				this->Death(dtEnergy, contactActor);
				return;
			}
			
			ShotDesc shot = _shot;
			shot.SetTargetMapObj(taget);
			SetShot(shot);
		}
		else if (_shot.GetTargetMapObj() == 0)
		{
			DamageTarget(contactActor, _desc.damage, dtEnergy);
			this->Death(dtEnergy, contactActor);
		}
	}
}

void Proj::ImpulseUpdate(float deltaTime)
{
	TorpedaUpdate(deltaTime);
}

bool Proj::ThunderPrepare(GameObject* weapon)
{
	_time1 = 0.0f;

	return RocketPrepare(weapon, true, NULL, px::Scene::cdgShotBorder);
}

void Proj::ThunderContact(const px::Scene::OnContactEvent& contact)
{
	RocketContact(contact);

	if (GetLiveState() == lsDeath)
		return;

	if (_time1 > 0.0f)
		return;
	_time1 = 0.0f;

	NxVec3 velocity = GetPxActor().GetNxActor()->getLinearVelocity();
	NxContactStreamIterator contIter(contact.stream);
	
	if (ContainsContactGroup(contIter, contact.actorIndex, px::Scene::cdgShotTransparency) && velocity.magnitude() > 5.0f)
	{
		_time1 = 0.1f;

		NxVec3 norm = contIter.getPatchNormal();
		if (contact.actorIndex == 0)
			norm = -norm;

		NxVec3 velNorm = velocity;
		velNorm.normalize();
		float angle = velNorm.dot(norm);		
		if (abs(angle) > 0.1f)
		{
			D3DXPLANE plane;
			D3DXPlaneFromPointNormal(&plane, &NullVector, &D3DXVECTOR3(norm.get()));
			D3DXMATRIX mat;
			D3DXMatrixReflect(&mat, &plane);

			D3DXVECTOR3 vel(velocity.get());
			D3DXVec3TransformNormal(&vel, &vel, &mat);
			velocity = NxVec3(vel);
		}
		else
			velocity = -velocity;

		GetPxActor().GetNxActor()->setLinearVelocity(velocity);
	}
}

void Proj::ThunderUpdate(float deltaTime)
{
	RocketUpdate(deltaTime);

	_time1 -= deltaTime;	
}

bool Proj::ResonansePrepare(GameObject* weapon)
{
	return RocketPrepare(weapon, true, NULL);
}

void Proj::ResonanseContact(const px::Scene::OnContactEvent& contact)
{
	RocketContact(contact);
}

void Proj::ResonanseUpdate(float deltaTime)
{
	RocketUpdate(deltaTime);

	D3DXQUATERNION dRot;
	D3DXQuaternionRotationAxis(&dRot, &XVector, _desc.angleSpeed * deltaTime);

	SetRot(dRot * GetRot());
}

void Proj::OnDestroy(GameObject* sender)
{
	_MyBase::OnDestroy(sender);

	if (_model && sender->GetMapObj() == _model)
		lsl::SafeRelease(_model);

	if (_model2 && sender->GetMapObj() == _model2)
		lsl::SafeRelease(_model2);

	if (_weapon && _weapon == sender)
	{
		if (GetParent() == _weapon)
			Death();
		SetWeapon(0);
	}

	if (_shot.GetTargetMapObj() && sender == &_shot.GetTargetMapObj()->GetGameObj())
		_shot.SetTargetMapObj(0);
}

void Proj::OnContact(const px::Scene::OnContactEvent& contact)
{
	_MyBase::OnContact(contact);

	GameObject* target = GetGameObjFromActor(contact.actor);

	if (GetLiveState() != lsDeath && !(target && target->GetLiveState() == lsDeath))
	{
		switch (_desc.type)
		{
		case ptRocket:
			RocketContact(contact);
			break;	

		case ptTorpeda:
			TorpedaContact(contact);
			break;

		case ptMedpack:
			MedpackContact(contact);
			break;
			
		case ptCharge:
			ChargeContact(contact);
			break;
			
		case ptMoney:
			MoneyContact(contact);
			break;

		case ptImmortal:
			ImmortalContact(contact);
			break;

		case ptSpeedArrow:
			SpeedArrowContact(contact);
			break;

		case ptLusha:
			LushaContact(contact);
			break;

		case ptMaslo:
			MasloContact(contact);
			break;

		case ptMine:
			MineContact(contact, true);
			break;

		case ptMineRip:
			MineRipContact(contact);
			break;

		case ptMinePiece:
			MinePieceContact(contact);
			break;

		case ptMineProton:
			MineProtonContact(contact);
			break;

		case ptFire:
			FireContact(contact);
			break;

		case ptDrobilka:
			DrobilkaContact(contact);
			break;

		case ptSonar:
			SonarContact(contact);
			break;

		case ptMortira:
			MortiraContact(contact);
			break;

		case ptCrater:
			CraterContact(contact);
			break;

		case ptImpulse:
			ImpulseContact(contact);
			break;

		case ptThunder:
			ThunderContact(contact);
			break;

		case ptResonanse:
			ResonanseContact(contact);
			break;
		}
	}			
}

void Proj::SaveSource(lsl::SWriter* writer)
{
	_MyBase::SaveSource(writer);

	lsl::SWriter* proj = writer->NewDummyNode("proj");	
	_desc.SaveTo(proj, this);
}

void Proj::LoadSource(lsl::SReader* reader)
{
	_MyBase::LoadSource(reader);

	lsl::SReader* proj = reader->ReadValue("proj");	
	_desc.LoadFrom(proj, this);
}

void Proj::OnFixUp(const FixUpNames& fixUpNames)
{
	_MyBase::OnFixUp(fixUpNames);

	for (FixUpNames::const_iterator iter = fixUpNames.begin(); iter != fixUpNames.end(); ++iter)
	{
		if (iter->sender->GetOwnerValue()->GetMyName() == "proj")
		{
			_desc.OnFixUp(*iter);
			break;
		}
	}
}

Proj* Proj::IsProj()
{
	return this;
}

void Proj::OnProgress(float deltaTime)
{
	_MyBase::OnProgress(deltaTime);

	switch (_desc.type)
	{
	case ptRocket:
		RocketUpdate(deltaTime);
		break;

	case ptTorpeda:
		TorpedaUpdate(deltaTime);
		break;

	case ptLaser:
		LaserUpdate(deltaTime, true);
		break;

	case ptFire:
		FireUpdate(deltaTime);
		break;

	case ptMaslo:
		MasloUpdate(deltaTime);
		break;

	case ptMine:
		MineUpdate(deltaTime);
		break;

	case ptMineRip:
		MineRipUpdate(deltaTime);
		break;	

	case ptMineProton:
		MineProtonUpdate(deltaTime);
		break;

	case ptDrobilka:
		DrobilkaUpdate(deltaTime);
		break;

	case ptSpring:
		SpringUpdate(deltaTime);
		break;

	case ptFrostRay:
		FrostRayUpdate(deltaTime);
		break;

	case ptImpulse:
		ImpulseUpdate(deltaTime);
		break;

	case ptThunder:
		ThunderUpdate(deltaTime);
		break;

	case ptResonanse:
		ResonanseUpdate(deltaTime);
		break;
	}		
}

bool Proj::PrepareProj(GameObject* weapon, const ShotContext& ctx)
{
	SetLogic(ctx.logic);
	SetWeapon(weapon);
	SetShot(ctx.shot);

	float timeLife = 0;
	if (_desc.speed > 0.0f)
		timeLife = _desc.maxDist / _desc.speed;
	SetMaxTimeLife(std::max(timeLife, _desc.minTimeLife.GetValue()));

	bool res = true;	
	switch (_desc.type)
	{
	case ptRocket:
		res = RocketPrepare(weapon);
		break;

	case ptHyper:
		res = HyperPrepare(weapon);
		break;

	case ptTorpeda:
		res = TorpedaPrepare(weapon);
		break;

	case ptLaser:
		res = LaserPrepare(weapon);
		break;

	case ptMedpack:
		res = MedpackPrepare(weapon);
		break;
		
	case ptCharge:
		res = ChargePrepare(weapon);
		break;
		
	case ptMoney:
		res = MoneyPrepare(weapon);
		break;

	case ptImmortal:
		res = ImmortalPrepare(weapon);
		break;

	case ptSpeedArrow:
		res = SpeedArrowPrepare(weapon);
		break;

	case ptLusha:
		res = LushaPrepare(ctx);
		break;

	case ptMaslo:
		res = MasloPrepare(ctx);
		break;

	case ptMine:
		res = MinePrepare(ctx, true);
		break;	

	case ptMineRip:
		res = MineRipPrepare(ctx);
		break;

	case ptMinePiece:
		res = MinePiecePrepare(ctx);
		break;	

	case ptMineProton:
		res = MineProtonPrepare(ctx);
		break;	

	case ptFire:
		res = FirePrepare(weapon);
		break;

	case ptDrobilka:
		res = DrobilkaPrepare(weapon);
		break;

	case ptSonar:
		res = SonarPrepare(weapon);
		break;

	case ptSpring:
		res = SpringPrepare(weapon);
		break;

	case ptFrostRay:
		res = FrostRayPrepare(weapon);
		break;

	case ptMortira:
		res = MortiraPrepare(weapon);
		break;

	case ptCrater:
		res = CraterPrepare(ctx);
		break;

	case ptImpulse:
		res = ImpulsePrepare(weapon);
		break;

	case ptThunder:
		res = ThunderPrepare(weapon);
		break;

	case ptResonanse:
		res = ResonansePrepare(weapon);
		break;
	}

	//nxActor у blaster manticora не был создан!!!
	GetPxActor().SetScene(ctx.logic->GetPxScene());

	LSL_ASSERT(GetPxActor().GetNxActor());

	if (GetPxActor().GetNxActor() == NULL)
	{
		LSL_LOG("Proj::PrepareProj GetPxActor().GetNxActor() == NULL!!!");
		return false;
	}

	//Игнорируем контакты снаряда с родителем (т.е. с самим собой)
	if (_ignoreContactProj && weapon && weapon->GetPxActor().GetNxActor())
	{		
		GetPxActor().GetScene()->GetNxScene()->setActorPairFlags(*weapon->GetPxActor().GetNxActor(), *GetPxActor().GetNxActor(), NX_IGNORE_PAIR);
	}

	return res;
}

void Proj::MineContact(GameObject* target, const D3DXVECTOR3& point)
{
	this->Death();

	if (target == NULL)
	{
		LSL_LOG("Proj::MineContact target == NULL");
		return;
	}

	DamageTarget(target, _desc.damage, dtMine);
	AddContactForce(target, point, D3DXVECTOR3(0.0f, 0.0f, _desc.speed), NX_IMPULSE);	
}

const Proj::Desc& Proj::GetDesc() const
{
	return _desc;
}

void Proj::SetDesc(const Desc& value)
{
	_desc = value;
}

const Proj::ShotDesc& Proj::GetShot() const
{
	return _shot;
}

GameObject* Proj::GetWeapon()
{
	return _weapon;
}




AutoProj::AutoProj(): _prepare(false)
{
}

AutoProj::~AutoProj()
{
	FreeProj();
}

void AutoProj::InitProj()
{
	if (!_prepare && GetLogic())
	{
		_prepare = true;

		D3DXVECTOR3 pos = GetPos();
		D3DXQUATERNION rot = GetRot();

		Proj::ShotContext ctx;
		ctx.logic = GetLogic();
		_MyBase::PrepareProj(NULL, ctx);

		SetPos(pos);
		SetRot(rot);
	}
}

void AutoProj::FreeProj()
{
	if (_prepare)
	{
		_prepare = false;
	}
}

void AutoProj::SaveSource(lsl::SWriter* writer)
{
	_MyBase::SaveSource(writer);
}

void AutoProj::LoadSource(lsl::SReader* reader)
{
	FreeProj();

	_MyBase::LoadSource(reader);

	//начальная инициализация. Чтобы показывалась модель. Лучшего места к сожалению не найдено
	InitProj();
}

void AutoProj::LogicReleased()
{
	FreeProj();
}

void AutoProj::LogicInited()
{
	InitProj();
}




Weapon::Weapon(): _shotTime(0)
{
}

Weapon::~Weapon()
{
}

void Weapon::SaveSource(lsl::SWriter* writer)
{
	_MyBase::SaveSource(writer);

	_desc.SaveTo(writer, this);
}

void Weapon::LoadSource(lsl::SReader* reader)
{
	_MyBase::LoadSource(reader);

	Desc desc;
	desc.LoadFrom(reader, this);	
	SetDesc(desc);
}

void Weapon::OnFixUp(const FixUpNames& fixUpNames)
{
	_MyBase::OnFixUp(fixUpNames);

	_desc.OnFixUp(fixUpNames);
}

void Weapon::OnProgress(float deltaTime)
{
	_MyBase::OnProgress(deltaTime);

	_shotTime += deltaTime;
}

bool Weapon::Shot(const ShotDesc& shotDesc, ProjList* projList)
{
	Proj::ShotContext ctx;	
	ctx.logic = GetLogic();
	ctx.shot = shotDesc;

	return CreateShot(this, _desc, ctx, projList);
}

bool Weapon::Shot(const D3DXVECTOR3& target, ProjList* projList)
{
	ShotDesc desc;
	desc.target = target;

	return Shot(desc, projList);
}

bool Weapon::Shot(MapObj* target, ProjList* projList)
{
	ShotDesc desc;
	desc.SetTargetMapObj(target);

	return Shot(desc, projList);
}

bool Weapon::Shot(ProjList* projList)
{
	return Shot(ShotDesc(), projList);
}

float Weapon::GetShotTime()
{
	return _shotTime;
}

bool Weapon::IsReadyShot(float delay) const
{
	return _shotTime > delay;
}

bool Weapon::IsReadyShot() const
{
	return IsReadyShot(_desc.shotDelay);
}

bool Weapon::IsMaslo() const
{
	return _desc.projList.size() > 0 && _desc.projList.front().type == Proj::ptMaslo;
}

const Weapon::Desc& Weapon::GetDesc() const
{
	return _desc;
}

void Weapon::SetDesc(const Desc& value)
{
	_desc = value;
}

bool Weapon::CreateShot(Weapon* weapon, const Weapon::Desc& desc, const Proj::ShotContext& ctx, ProjList* projList)
{
	bool res = false;

	for (ProjDescList::const_iterator iter = desc.projList.begin(); iter != desc.projList.end(); ++iter)
	{
		if (weapon == NULL && 
			(iter->type == Proj::ptRocket ||
			iter->type == Proj::ptHyper ||
			iter->type == Proj::ptTorpeda ||
			iter->type == Proj::ptLaser ||
			iter->type == Proj::ptMedpack ||
			iter->type == Proj::ptCharge ||
			iter->type == Proj::ptMoney ||
			iter->type == Proj::ptImmortal ||
			iter->type == Proj::ptSpeedArrow ||
			iter->type == Proj::ptFire ||
			iter->type == Proj::ptDrobilka ||
			iter->type == Proj::ptSonar ||
			iter->type == Proj::ptSpring ||
			iter->type == Proj::ptFrostRay ||
			iter->type == Proj::ptMortira ||
			iter->type == Proj::ptCrater ||
			iter->type == Proj::ptImpulse
			))
			continue;

		Proj* proj = new Proj();
		proj->SetDesc(*iter);

		if (proj->PrepareProj(weapon, ctx))
		{
			ctx.logic->RegGameObj(proj);

			if (weapon)
			{
				weapon->_shotTime = 0.0f;
				weapon->GetBehaviors().OnShot(iter->pos);
			}

			if (projList)
				projList->push_back(proj);

			res = true;
		}
		else
		{
			delete proj;
		}
	}

	return res;
}

}

}