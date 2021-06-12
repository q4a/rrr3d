#include "stdafx.h"
#include "game\\GameBase.h"

#include "game\\GameObject.h"
#include "game\\GameCar.h"
#include "game\\Weapon.h"
#include "game\\Logic.h"

namespace r3d
{

namespace game
{

const lsl::string cDifficultyStr[cDifficultyEnd] = {"gdEasy", "gdNormal", "gdHard"};
const char* cScActorTypeStr[cScActorTypeEnd] = {"satBaseObj", "satMeshObj"};

const std::string GameObjListener::cBonusTypeStr[cBonusTypeEnd] = {"btMoney", "btCharge", "btMedpack", "btImmortal"};

Behaviors::ClassList Behaviors::classList;




Behavior::Behavior(Behaviors* owner): _owner(owner), _removed(false)
{	
}

void Behavior::Save(lsl::SWriter* writer)
{
	if (_owner->storeProxy)
		SaveProxy(writer);
	if (_owner->storeSource)
		SaveSource(writer);
}

void Behavior::Load(lsl::SReader* reader)
{
	if (_owner->storeProxy)
		LoadProxy(reader);
	if (_owner->storeSource)
		LoadSource(reader);
}

bool Behavior::GetPxNotify(PxNotify notify) const
{
	return _pxNotifies[notify];
}

void Behavior::SetPxNotify(PxNotify notify, bool value)
{
	if (_pxNotifies[notify] != value)
	{
		switch (notify)
		{
		case pxContact:
			GetGameObj()->GetPxActor().SetContactReportFlag(NX_NOTIFY_ALL, value);
			break;			
			
		case pxContactModify:
			GetGameObj()->GetPxActor().SetContactReportFlag(NX_NOTIFY_CONTACT_MODIFICATION, value);
			break;
		}
	}
}

void Behavior::Remove()
{
	_removed = true;
}

bool Behavior::IsRemoved() const
{
	return _removed;
}

Behaviors* Behavior::GetOwner()
{
	return _owner;
}

GameObject* Behavior::GetGameObj()
{
	return _owner->GetGameObj();
}

Logic* Behavior::GetLogic()
{
	return GetGameObj()->GetLogic();
}




TouchDeath::TouchDeath(Behaviors* owner): _MyBase(owner)
{
	SetPxNotify(pxContact, true);
}

void TouchDeath::OnContact(const px::Scene::OnContactEvent& contact)
{
	GameObject* target = GameObject::GetGameObjFromActor(contact.actor);

	if (target)
		target->Death(dtDeathPlane);
}




ResurrectObj::ResurrectObj(Behaviors* owner): _MyBase(owner), _resurrect(false)
{
}

void ResurrectObj::Resurrect()
{
	GameObject* gameObj = GetGameObj();
	MapObj* mapObj = gameObj->GetMapObj();

	LSL_ASSERT(mapObj);

	gameObj->Resc();

	MapObjects* owner = mapObj->GetOwner();
	GameObject* parent = owner ? owner->GetOwner() : 0;
	if (owner && parent)
	{
		owner->LockDestr();
		
		D3DXVECTOR3 pos = gameObj->GetWorldPos();
		D3DXQUATERNION rot = gameObj->GetWorldRot();

		owner->Delete(mapObj);
		gameObj->SetParent(0);
		gameObj->SetOwner(0);
		gameObj->SetName("");

		gameObj->SetWorldPos(pos);
		gameObj->SetWorldRot(rot);
		gameObj->GetLogic()->GetMap()->InsertMapObj(mapObj);

		owner->UnlockDestr();
	}
}

void ResurrectObj::OnDeath(GameObject* sender, DamageType damageType, GameObject* target)
{
	if (!_resurrect)
	{
		_resurrect = true;
		Resurrect();
	}
}

bool ResurrectObj::IsResurrect() const
{
	return _resurrect;
}




FxSystemWaitingEnd::FxSystemWaitingEnd(Behaviors* owner): _MyBase(owner)
{
}

void FxSystemWaitingEnd::Resurrect()
{
	_MyBase::Resurrect();

	graph::SceneNode& node = GetGameObj()->GetGrActor();
	for (graph::SceneNode::Nodes::iterator iter = node.GetNodes().begin(); iter != node.GetNodes().end(); ++iter)
		if (iter->GetType() == graph::SceneNode::ntParticleSystem)
			iter->GetItem<graph::FxParticleSystem>()->SetModeFading(true);
}

void FxSystemWaitingEnd::OnProgress(float deltaTime)
{
	if (IsResurrect())
	{
		unsigned resCnt = 0;

		graph::SceneNode& node = GetGameObj()->GetGrActor();
		for (graph::SceneNode::Nodes::iterator iter = node.GetNodes().begin(); iter != node.GetNodes().end(); ++iter)
			if (iter->GetType() == graph::SceneNode::ntParticleSystem)
			{
				graph::FxParticleSystem* fxSystem = iter->GetItem<graph::FxParticleSystem>();
				resCnt += fxSystem->GetCntParticle();
			}

		if (resCnt == 0)
			GetGameObj()->Death();
	}	
}




FxSystemSrcSpeed::FxSystemSrcSpeed(Behaviors* owner): _MyBase(owner)
{
}

void FxSystemSrcSpeed::OnProgress(float deltaTime)
{
	if (GetGameObj()->GetPxActor().GetNxActor() == 0)
		return;

	graph::SceneNode& node = GetGameObj()->GetGrActor();
	for (graph::SceneNode::Nodes::iterator iter = node.GetNodes().begin(); iter != node.GetNodes().end(); ++iter)
		if (iter->GetType() == graph::SceneNode::ntParticleSystem)
		{
			graph::FxParticleSystem* fxSystem = iter->GetItem<graph::FxParticleSystem>();

			D3DXVECTOR3 speed(GetGameObj()->GetPxActor().GetNxActor()->getLinearVelocity().get());
			if (GetGameObj()->GetParent())
				GetGameObj()->GetParent()->GetGrActor().WorldToLocalNorm(speed, speed);

			fxSystem->SetSrcSpeed(speed);
		}
}




EventEffect::EventEffect(Behaviors* owner): _MyBase(owner), _effect(0), _pos(NullVector), _impulse(NullVector), _ignoreRot(false), _makeEffect(0)
{
	_gameObjEvent = new GameObjEvent(this);
}

EventEffect::~EventEffect()
{
	ClearEffObjList();

	ClearSounds();
	SetEffect(0);

	delete _gameObjEvent;
}

EventEffect::GameObjEvent::GameObjEvent(EventEffect* effect): _effect(effect)
{
}

void EventEffect::GameObjEvent::OnDestroy(GameObject* sender)
{
	_effect->OnDestroyEffect(sender->GetMapObj());

	_effect->RemoveEffObj(sender->GetMapObj());
}

void EventEffect::InsertEffObj(MapObj* mapObj)
{
	mapObj->AddRef();
	_effObjList.push_back(mapObj);
}

void EventEffect::RemoveEffObj(MapObj* mapObj)
{
	if (_makeEffect == mapObj)
		_makeEffect = 0;

	mapObj->Release();
	_effObjList.Remove(mapObj);
}

void EventEffect::ClearEffObjList()
{
	for (EffObjList::iterator iter = _effObjList.begin(); iter != _effObjList.end(); ++iter)
	{
		(*iter)->Release();
		DestroyEffObj(*iter);
	}

	_effObjList.clear();
}

void EventEffect::DestroyEffObj(MapObj* mapObj, bool destrWorld)
{
	//
	mapObj->GetGameObj().RemoveListener(_gameObjEvent);

	//удаляем только дочерний объект
	if (mapObj->GetOwner()->GetOwner() == GetGameObj())
	{
		GetGameObj()->GetIncludeList().Delete(mapObj);		
	}
	else if (destrWorld)
	{
		GetLogic()->GetMap()->DelMapObj(mapObj);
	}
}

void EventEffect::InitSource()
{
	for (SoundList::iterator iter = _sounds.begin(); iter != _sounds.end(); ++iter)
	{
		if (iter->source)
			continue;

		iter->source = GetLogic()->CreateSndSource(Logic::scEffects);
		iter->source->SetSound(iter->sound);
	}
}

void EventEffect::FreeSource()
{
	for (SoundList::iterator iter = _sounds.begin(); iter != _sounds.end(); ++iter)
	{
		if (iter->source == NULL)
			continue;

		GetLogic()->ReleaseSndSource(iter->source);
		iter->source = NULL;
	}
}

void EventEffect::InitSource3d()
{
	for (SoundList::iterator iter = _sounds.begin(); iter != _sounds.end(); ++iter)
	{
		if (iter->source3d)
			continue;

		iter->source3d = GetLogic()->CreateSndSource3d(Logic::scEffects);
		iter->source3d->SetSound(iter->sound);
	}
}

void EventEffect::FreeSource3d()
{
	for (SoundList::iterator iter = _sounds.begin(); iter != _sounds.end(); ++iter)
	{
		if (iter->source3d == NULL)
			continue;

		GetLogic()->ReleaseSndSource(iter->source3d);
		iter->source3d = NULL;
	}
}

MapObj* EventEffect::CreateEffect(const EffectDesc& desc)
{
	LSL_ASSERT(_effect);

	MapObj* mapObj = 0;	
	
	//дочерний объект
	if (desc.parent)
	{
		mapObj = &desc.parent->GetIncludeList().Add(_effect);
	}
	else if (desc.child)
	{
		mapObj = &GetGameObj()->GetIncludeList().Add(_effect);
	}
	//глобальный
	else
	{
		mapObj = &GetLogic()->GetMap()->AddMapObj(_effect);
	}

	InsertEffObj(mapObj);
	mapObj->GetGameObj().InsertListener(_gameObjEvent);
	mapObj->GetGameObj().SetPos(_pos + desc.pos);
	if (!_ignoreRot)
		mapObj->GetGameObj().SetRot(desc.rot);

	if (D3DXVec3Length(&_impulse) > 0.001f && mapObj->GetGameObj().GetPxActor().GetNxActor())
		mapObj->GetGameObj().GetPxActor().GetNxActor()->addLocalForce(NxVec3(_impulse), NX_IMPULSE);

	return mapObj;
}

MapObj* EventEffect::CreateEffect()
{
	return CreateEffect(EffectDesc());
}

void EventEffect::DeleteEffect(MapObj* mapObj)
{
	RemoveEffObj(mapObj);
	DestroyEffObj(mapObj);
}

void EventEffect::DeleteAllEffects()
{
	for (EffObjList::iterator iter = _effObjList.begin(); iter != _effObjList.end(); ++iter)
	{
		(*iter)->Release();
		DestroyEffObj(*iter, true);
	}

	_effObjList.clear();
}

void EventEffect::MakeEffect(const EffectDesc& desc)
{
	if (!_makeEffect)
		_makeEffect = CreateEffect(desc);
}

void EventEffect::MakeEffect()
{
	if (!_makeEffect)
		_makeEffect = CreateEffect();
}

void EventEffect::FreeEffect(bool death)
{
	if (_makeEffect)
	{
		if (death)
			_makeEffect->GetGameObj().Death();
		else
			DeleteEffect(_makeEffect);

		_makeEffect = 0;
	}
}

MapObj* EventEffect::GetMakeEffect()
{
	return _makeEffect;
}

bool EventEffect::IsEffectMaked() const
{
	return _makeEffect ? true : false;
}

void EventEffect::SaveSource(lsl::SWriter* writer)
{
	MapObjRec::Lib::SaveRecordRef(writer, "effect", _effect);

	int i = 0;
	lsl::SWriter* sounds = writer->NewDummyNode("sounds");
	for (SoundList::iterator iter = _sounds.begin(); iter != _sounds.end(); ++iter, ++i)
	{
		sounds->WriteRef(lsl::StrFmt("sound%d", i).c_str(), iter->sound);
	}

	writer->WriteValue("pos", _pos, 3);
	writer->WriteValue("impulse", _impulse, 3);
	writer->WriteValue("ignoreRot", _ignoreRot);	
}

void EventEffect::LoadSource(lsl::SReader* reader)
{
	ClearSounds();

	lsl::SReader* sounds = reader->ReadValue("sounds");
	if (sounds)
	{
		lsl::SReader* child = sounds->FirstChildValue();
		while (child)
		{
			child->AddFixUp(true, this, NULL);
			child = child->NextValue();
		}		
	}

	lsl::SReader* child = reader->ReadRef("sound", true, this, 0);

	SetEffect(MapObjRec::Lib::LoadRecordRef(reader, "effect"));	
	
	reader->ReadValue("pos", _pos, 3);
	reader->ReadValue("impulse", _impulse, 3);
	reader->ReadValue("ignoreRot", _ignoreRot);	
}

void EventEffect::OnFixUp(const FixUpNames& fixUpNames)
{
	_MyBase::OnFixUp(fixUpNames);

	for (FixUpNames::const_iterator iter = fixUpNames.begin(); iter != fixUpNames.end(); ++iter)
		if (iter->sender->GetOwnerValue()->GetMyName() == "sounds")
		{
			AddSound(iter->GetCollItem<snd::Sound*>());
			break;
		}
}

snd::Source* EventEffect::GiveSource()
{
	InitSource();

	return _sounds.size() > 0 ? _sounds[RandomRange(0, _sounds.size() - 1)].source : NULL;
}

snd::Source3d* EventEffect::GiveSource3d()
{
	InitSource3d();

	return _sounds.size() > 0 ? _sounds[RandomRange(0, _sounds.size() - 1)].source3d : NULL;
}

void EventEffect::OnProgress(float deltaTime)
{
	for (SoundList::iterator iter = _sounds.begin(); iter != _sounds.end(); ++iter)
		if (iter->source3d)
		{
			iter->source3d->SetPos3d(GetOwner()->GetGameObj()->GetWorldPos());
		}
}

MapObjRec* EventEffect::GetEffect()
{
	return _effect;
}

void EventEffect::SetEffect(MapObjRec* value)
{
	if (ReplaceRef(_effect, value))
		_effect = value;
}

void EventEffect::AddSound(snd::Sound* sound)
{
	sound->AddRef();

	EffectSound eff;
	eff.sound = sound;

	_sounds.push_back(eff);
}

void EventEffect::ClearSounds()
{
	FreeSource();
	FreeSource3d();

	for (SoundList::const_iterator iter = _sounds.begin(); iter != _sounds.end(); ++iter)
	{
		iter->sound->Release();
	}

	_sounds.clear();
}

const EventEffect::SoundList& EventEffect::GetSounds()
{
	return _sounds;
}

snd::Sound* EventEffect::GetSound()
{
	return _sounds.size() > 0 ? _sounds[0].sound : NULL;
}

void EventEffect::SetSound(snd::Sound* value)
{
	ClearSounds();

	AddSound(value);
}

const D3DXVECTOR3& EventEffect::GetPos() const
{
	return _pos;
}

void EventEffect::SetPos(const D3DXVECTOR3& value)
{
	_pos = value;
}

const D3DXVECTOR3& EventEffect::GetImpulse() const
{
	return _impulse;
}

void EventEffect::SetImpulse(const D3DXVECTOR3& value)
{
	_impulse = value;
}

bool EventEffect::GetIgnoreRot() const
{
	return _ignoreRot;
}

void EventEffect::SetIgnoreRot(bool value)
{
	_ignoreRot = value;
}




LowLifePoints::LowLifePoints(Behaviors* owner): _MyBase(owner), _lifeLevel(0.35f)
{
}

void LowLifePoints::SaveSource(lsl::SWriter* writer)
{
	_MyBase::SaveSource(writer);

	writer->WriteValue("lifeLevel", _lifeLevel);
}

void LowLifePoints::LoadSource(lsl::SReader* reader)
{
	_MyBase::LoadSource(reader);

	reader->ReadValue("lifeLevel", _lifeLevel);
}

void LowLifePoints::OnProgress(float deltaTime)
{
	_MyBase::OnProgress(deltaTime);

	float maxLife = GetGameObj()->GetMaxLife();
	float life = GetGameObj()->GetLife();	

	if (GetGameObj()->GetLiveState() != GameObject::lsDeath && maxLife > 0.0f && life > 0 && life/maxLife < _lifeLevel)
	{
		if (!IsEffectMaked())
			GetGameObj()->LowLife(this);
		MakeEffect();
	}
	else
		FreeEffect();
}

float LowLifePoints::GetLifeLevel() const
{
	return _lifeLevel;
}

void LowLifePoints::SetLifeLevel(float value)
{
	_lifeLevel = value;
}




DamageEffect::DamageEffect(Behaviors* owner): _MyBase(owner), _damageType(dtSimple)
{
}

void DamageEffect::OnDamage(GameObject* sender, float value, DamageType damageType)
{
	if (_damageType == damageType)
	{
		MakeEffect();

		snd::Source3d* source = GiveSource3d();
		if (source)
		{
			source->Stop();
			source->SetPlayMode(snd::pmOnce);
			source->SetPos(0);
			source->Play();
		}
	}
}

void DamageEffect::SaveSource(lsl::SWriter* writer)
{
	_MyBase::SaveSource(writer);

	writer->WriteValue("damageType", _damageType);
}

void DamageEffect::LoadSource(lsl::SReader* reader)
{
	_MyBase::LoadSource(reader);

	int damageType;
	reader->ReadValue("damageType", damageType);
	_damageType = DamageType(damageType);
}

void DamageEffect::OnProgress(float deltaTime)
{
	_MyBase::OnProgress(deltaTime);
}

DamageEffect::DamageType DamageEffect::GetDamageType() const
{
	return _damageType;
}

void DamageEffect::SetDamageType(DamageType value)
{
	_damageType = value;
}




DeathEffect::DeathEffect(Behaviors* owner): _MyBase(owner), _effectPxIgnoreSenderCar(false), _targetChild(false)
{
}

void DeathEffect::OnDeath(GameObject* sender, DamageType damageType, GameObject* target)
{
	if (GetGameObj()->GetLogic())
	{
		EffectDesc desc;
		desc.child = false;

		if (_targetChild && target)
		{
			desc.parent = target;
			target->GetGrActor().WorldToLocalCoord(GetGameObj()->GetWorldPos(), desc.pos);
			desc.rot = NullQuaternion;
		}
		else
		{
			desc.pos = GetGameObj()->GetWorldPos();
			desc.rot = GetGameObj()->GetWorldRot();
		}

		MakeEffect(desc);

		NxActor* eff = GetMakeEffect() ? GetMakeEffect()->GetGameObj().GetPxActor().GetNxActor() : 0;
		NxActor* car = sender && sender->GetParent() && sender->GetParent()->IsProj() && sender->GetParent()->IsProj()->GetWeapon() ? sender->GetParent()->IsProj()->GetWeapon()->GetPxActor().GetNxActor() : 0;

		if (_effectPxIgnoreSenderCar && eff && car)
			eff->getScene().setActorPairFlags(*eff, *car, NX_IGNORE_PAIR);
	}
}

void DeathEffect::SaveSource(lsl::SWriter* writer)
{
	_MyBase::SaveSource(writer);

	writer->WriteValue("effectPxIgnoreSenderCar", _effectPxIgnoreSenderCar);
	writer->WriteValue("targetChild", _targetChild);
}

void DeathEffect::LoadSource(lsl::SReader* reader)
{
	_MyBase::LoadSource(reader);

	reader->ReadValue("effectPxIgnoreSenderCar", _effectPxIgnoreSenderCar);
	reader->ReadValue("targetChild", _targetChild);
}

void DeathEffect::OnProgress(float deltaTime)
{
	_MyBase::OnProgress(deltaTime);
}

bool DeathEffect::GetEffectPxIgnoreSenderCar() const
{
	return _effectPxIgnoreSenderCar;
}

void DeathEffect::SetEffectPxIgnoreSenderCar(bool value)
{
	_effectPxIgnoreSenderCar = value;
}

bool DeathEffect::GetTargetChild() const
{
	return _targetChild;
}

void DeathEffect::SetTargetChild(bool value)
{
	_targetChild = value;
}




LifeEffect::LifeEffect(Behaviors* owner): _MyBase(owner), _play(false)
{
}

void LifeEffect::OnProgress(float deltaTime)
{
	_MyBase::OnProgress(deltaTime);

	if (!_play)
	{
		snd::Source3d* source = GiveSource3d();
		if (source)
		{
			_play = true;	

			source->SetPlayMode(snd::pmOnce);
			source->SetPos(0);
			source->Play();
		}
	}
}




PxWheelSlipEffect::PxWheelSlipEffect(Behaviors* owner): _MyBase(owner)
{	
}

PxWheelSlipEffect::~PxWheelSlipEffect()
{
	DeleteAllEffects();
}

void PxWheelSlipEffect::OnProgress(float deltaTime)
{
	_MyBase::OnProgress(deltaTime);

#if !_DEBUG
	//1
	//const float slipLong = 0.6f;
	//const float slipLat = 0.7f;
	//2
	//const float slipLong = 0.4f;
	//const float slipLat = 0.5f;
	//3
	const float slipLong = 0.4f;
	const float slipLat = 0.7f;//const float slipLat = 0.6f;

	const float volumeK = 4.0f;

	CarWheel& wheel = lsl::StaticCast<CarWheel&>(*GetGameObj());
	snd::Source3d* source = GiveSource3d();
	
	NxWheelContactData contactDesc;
	NxShape* contact = wheel.GetShape()->GetNxShape()->getContact(contactDesc);
	float slip = 0.0f;		
	if (contact)
		slip = std::max(abs(contactDesc.lateralSlip) - slipLat, 0.0f) + std::max(abs(contactDesc.longitudalSlip) - slipLong, 0.0f);

	if (slip > 0)
	{
		EffectDesc desc;
		desc.pos = D3DXVECTOR3(contactDesc.contactPoint.get());
		desc.child = false;
		if (GetMakeEffect())
			GetMakeEffect()->GetGameObj().SetPos(GetPos() + desc.pos);
		else		
			MakeEffect(desc);

		if (source)
		{
			source->SetPlayMode(snd::pmInfite);
			if (!source->IsPlaying())
				source->SetPos(0);
			//slip
			source->SetVolume(lsl::ClampValue(slip * volumeK, 0.0f, 1.0f));
			source->Play();
		}
	}
	else
	{
		FreeEffect(true);

		if (source)
			source->Stop();
	}
#endif
}




ShotEffect::ShotEffect(Behaviors* owner): _MyBase(owner)
{
}

void ShotEffect::OnShot(const D3DXVECTOR3& pos)
{
	if (GetEffect())
	{
		EffectDesc desc;
		desc.child = true;
		desc.pos = pos;
		CreateEffect(desc);
	}

	snd::Source3d* source = GiveSource3d();
	if (source)
	{
		source->SetPlayMode(snd::pmOnce);
		source->SetPos(0);
		source->Play();
	}
}




ImmortalEffect::ImmortalEffect(Behaviors* owner): _MyBase(owner), _fadeInTime(-1.0f), _fadeOutTime(-1.0f), _dmgTime(-1.0f), _scale(IdentityVector), _scaleK(IdentityVector)
{
}

void ImmortalEffect::OnImmortalStatus(bool status)
{
	if (status)
	{
		if (GetEffect())
		{
			EffectDesc desc;
			desc.child = true;		
			MakeEffect(desc);
		}

		snd::Source3d* source = GiveSource3d();
		if (source)
		{
			source->SetPlayMode(snd::pmOnce);
			source->SetPos(0);
			source->Play();
		}

		_fadeInTime = 0.0f;
		if (GetMakeEffect())
		{
			AABB aabb1 = GetGameObj()->GetGrActor().GetLocalAABB(false);
			AABB aabb2 = GetMakeEffect()->GetGameObj().GetGrActor().GetLocalAABB(true);
			_scale = aabb1.GetSizes() / aabb2.GetSizes() * _scaleK;

			GetMakeEffect()->GetGameObj().SetScale(0.0f);
		}
	}
	else
	{
		_fadeOutTime = 0.0f;		
	}
}

void ImmortalEffect::OnDamage(GameObject* sender, float value, DamageType damageType)
{
	if (GetMakeEffect())
	{
		_dmgTime = 0;
	}
}

void ImmortalEffect::SaveSource(lsl::SWriter* writer)
{
	_MyBase::SaveSource(writer);

	lsl::SWriteValue(writer, "scaleK", _scaleK);
}

void ImmortalEffect::LoadSource(lsl::SReader* reader)
{
	_MyBase::LoadSource(reader);

	lsl::SReadValue(reader, "scaleK", _scaleK);
}


void ImmortalEffect::OnProgress(float deltaTime)
{
	MapObj* mapObj = GetMakeEffect();

	if (mapObj && _dmgTime >= 0)
	{	
		float alpha = ClampValue(_dmgTime / 0.25f, 0.0f, 1.0f);

		const graph::Actor::Nodes& nodes = GetMakeEffect()->GetGameObj().GetGrActor().GetNodes();
		for (graph::Actor::Nodes::const_iterator iter = nodes.begin(); iter != nodes.end(); ++iter)
		{
			graph::MaterialNode* material = (*iter)->GetMaterial();
			if (material == NULL)
				continue;

			material->SetColor(D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f + 2.5f * (1.0f - alpha)));
		}

		if (alpha >= 1.0f)
			_dmgTime = -1.0f;
		else
			_dmgTime += deltaTime;
	}

	if (mapObj && _fadeInTime >= 0)
	{	
		_fadeInTime += deltaTime;
		float alpha = ClampValue(_fadeInTime / 0.5f, 0.0f, 1.0f);
		mapObj->GetGameObj().SetScale(_scale * alpha);
		if (alpha >= 1.0f)
			_fadeInTime = -1.0f;
	}

	if (mapObj && _fadeOutTime >= 0)
	{	
		_fadeOutTime += deltaTime;
		float alpha = ClampValue(_fadeOutTime / 0.5f, 0.0f, 1.0f);
		mapObj->GetGameObj().SetScale(_scale * (1.0f - alpha));
		if (alpha >= 1.0f)
		{
			_fadeOutTime = -1.0f;
			FreeEffect(true);
		}
	}
}

const D3DXVECTOR3& ImmortalEffect::GetScaleK() const
{
	return _scaleK;
}

void ImmortalEffect::SetScaleK(const D3DXVECTOR3& value)
{
	_scaleK = value;
}




SlowEffect::SlowEffect(Behaviors* owner): _MyBase(owner)
{
}

void SlowEffect::OnDestroyEffect(MapObj* sender)
{
	GetGameObj()->GetPxActor().GetNxActor()->setLinearDamping(0.0f);
	Remove();	
}

void SlowEffect::OnProgress(float deltaTime)
{
	_MyBase::OnProgress(deltaTime);

	MakeEffect();

	NxActor* target = GetGameObj()->GetPxActor().GetNxActor();
	if (target)
	{
		NxVec3 linSpeed = target->getLinearVelocity();
		float maxSpeed = linSpeed.magnitude();
		linSpeed.normalize();

		if (maxSpeed > 1.0f && maxSpeed > 20)
			target->setLinearVelocity(linSpeed * 20);
	}
}




SoundMotor::SoundMotor(Behaviors* owner): _MyBase(owner), _sndIdle(0), _sndRPM(0), _init(false), _srcIdle(0), _srcRPM(0), _rpmVolumeRange(0.0f, 1.0f), _rpmFreqRange(0.0f, 1.0f)
{
}

SoundMotor::~SoundMotor()
{
	Free();

	SetSndIdle(0);
	SetSndRPM(0);
}

void SoundMotor::Init()
{
	if (!_init && _sndIdle && _sndRPM)
	{
		_init = true;

		_curRPM = 0.0f;

		_srcIdle = GetLogic()->CreateSndSource3d(Logic::scEffects);
		_srcIdle->SetSound(_sndIdle);
		_srcIdle->SetPlayMode(snd::pmInfite);

		_srcRPM = GetLogic()->CreateSndSource3d(Logic::scEffects);
		_srcRPM->SetSound(_sndRPM);
		_srcRPM->SetPlayMode(snd::pmInfite);
	}
}
	
void SoundMotor::Free()
{
	if (_init)
	{
		_init = false;

		GetLogic()->ReleaseSndSource(_srcRPM);
		GetLogic()->ReleaseSndSource(_srcIdle);
	}
}

void SoundMotor::OnMotor(float deltaTime, float rpm, float minRPM, float maxRPM)
{
	const float motorLag = 10000.0f;

	Init();

	if (_init)
	{
		float distRPM = rpm - _curRPM;
		float rpmDT = motorLag * deltaTime * (distRPM > 0 ? 1.0f : -1.0f);
		_curRPM = _curRPM + lsl::ClampValue(rpmDT, -abs(distRPM), abs(distRPM));

		float idleAlpha = lsl::ClampValue(0.5f * (_curRPM - minRPM)/minRPM, 0.0f, 1.0f);
		float alpha = ClampValue((_curRPM - minRPM)/(maxRPM - minRPM), 0.0f, 1.0f);

		_srcRPM->Play();		
		_srcRPM->SetVolume((_rpmVolumeRange.x + alpha * (_rpmVolumeRange.y - _rpmVolumeRange.x)) * idleAlpha);
		_srcRPM->SetFrequencyRatio((_rpmFreqRange.x + alpha * (_rpmFreqRange.y - _rpmFreqRange.x)));

		_srcIdle->Play();
		_srcIdle->SetVolume(1.0f - idleAlpha);
	}
}

void SoundMotor::SaveSource(lsl::SWriter* writer)
{
	_MyBase::SaveSource(writer);

	writer->WriteRef("sndIdle", _sndIdle);
	writer->WriteRef("sndRPM", _sndRPM);

	lsl::SWriteValue(writer, "rpmVolumeRange", _rpmVolumeRange);
	lsl::SWriteValue(writer, "rpmFreqRange", _rpmFreqRange);
}

void SoundMotor::LoadSource(lsl::SReader* reader)
{
	_MyBase::LoadSource(reader);

	reader->ReadRef("sndIdle", true, this, 0);
	reader->ReadRef("sndRPM", true, this, 0);

	lsl::SReadValue(reader, "rpmVolumeRange", _rpmVolumeRange);
	lsl::SReadValue(reader, "rpmFreqRange", _rpmFreqRange);
}

void SoundMotor::OnFixUp(const FixUpNames& fixUpNames)
{
	_MyBase::OnFixUp(fixUpNames);

	for (FixUpNames::const_iterator iter = fixUpNames.begin(); iter != fixUpNames.end(); ++iter)
	{
		if (iter->name == "sndIdle")
		{
			SetSndIdle(iter->GetCollItem<snd::Sound*>());
			break;
		}
		if (iter->name == "sndRPM")
		{
			SetSndRPM(iter->GetCollItem<snd::Sound*>());
			break;
		}
	}
}

void SoundMotor::OnProgress(float deltaTime)
{
	if (_init)
	{
		_srcIdle->SetPos3d(GetOwner()->GetGameObj()->GetWorldPos());
		_srcRPM->SetPos3d(GetOwner()->GetGameObj()->GetWorldPos());
	}
}

snd::Sound* SoundMotor::GetSndIdle()
{
	return _sndIdle;
}

void SoundMotor::SetSndIdle(snd::Sound* value)
{
	if (ReplaceRef(_sndIdle, value))
	{
		Free();
		_sndIdle = value;
	}
}

snd::Sound* SoundMotor::GetSndRPM()
{
	return _sndIdle;
}

void SoundMotor::SetSndRPM(snd::Sound* value)
{
	if (ReplaceRef(_sndRPM, value))
	{
		Free();
		_sndRPM = value;
	}
}

const D3DXVECTOR2& SoundMotor::GetRPMVolumeRange() const
{
	return _rpmVolumeRange;
}

void SoundMotor::SetRPMVolumeRange(const D3DXVECTOR2& value)
{
	_rpmVolumeRange = value;
}

const D3DXVECTOR2& SoundMotor::GetRPMFreqRange() const
{
	return _rpmFreqRange;
}

void SoundMotor::SetRPMFreqRange(const D3DXVECTOR2& value)
{
	_rpmFreqRange = value;
}




GusenizaAnim::GusenizaAnim(Behaviors* owner): _MyBase(owner), _xAnimOff(0)
{
}

GusenizaAnim::~GusenizaAnim()
{
}

void GusenizaAnim::OnProgress(float deltaTime)
{
	const float gusLength = 2.5f * 2.0f;
	const float gusTexWidth = 800.0f;

	LSL_ASSERT(GetGameObj()->GetParent());

	GameCar* car = lsl::StaticCast<game::GameCar*>(GetGameObj()->GetParent());
	graph::Actor& actor = GetGameObj()->GetGrActor();
	graph::MaterialNode* mat = !actor.GetNodes().Empty() ? actor.GetNodes().front().GetMaterial() : NULL;

	LSL_ASSERT(mat);

	float linSpeed = car->GetLeadWheelSpeed();
	_xAnimOff -= linSpeed * deltaTime / gusLength;
	//выделяем дробную часть
	_xAnimOff = _xAnimOff - floor(_xAnimOff);
	D3DXVECTOR3 offset(1.0f - _xAnimOff, 0, 0.0f);

	mat->SetOffset(offset);
}




PodushkaAnim::PodushkaAnim(Behaviors* owner): _MyBase(owner), _targetTag(0), _target(NULL)
{
}

void PodushkaAnim::SaveSource(lsl::SWriter* writer)
{
	_MyBase::SaveSource(writer);

	writer->WriteValue("targetTag", _targetTag);
}

void PodushkaAnim::LoadSource(lsl::SReader* reader)
{
	_MyBase::LoadSource(reader);

	reader->ReadValue("targetTag", _targetTag);
}

void PodushkaAnim::OnProgress(float deltaTime)
{
	if (_target == NULL)
		_target = lsl::StaticCast<graph::IVBMeshNode*>(GetGameObj()->GetGrActor().GetNodeByTag(_targetTag));

	LSL_ASSERT(_target);

	GameCar* car = lsl::StaticCast<game::GameCar*>(GetGameObj()->GetParent());
	
	float linSpeed = car->GetLeadWheelSpeed();
	if (abs(linSpeed) > 1.0f)
	{
		D3DXMATRIX localMat = _target->GetMat();
		D3DXQUATERNION rotQuat;
		D3DXQuaternionRotationAxis(&rotQuat, &XVector, D3DX_PI * deltaTime * linSpeed * 0.1f);
		D3DXMATRIX rotMat;
		D3DXMatrixRotationQuaternion(&rotMat, &rotQuat);

		const res::FaceGroup& fg = _target->GetMesh()->GetData()->faceGroups[_target->GetMeshId()];
		D3DXVECTOR3 offset = (fg.minPos + fg.maxPos) / 2;

		D3DXMATRIX matOffs1;
		D3DXMatrixTranslation(&matOffs1, -offset.x, -offset.y, -offset.z);
		D3DXMATRIX matOffs2;
		D3DXMatrixTranslation(&matOffs2, offset.x, offset.y, offset.z);
		localMat = localMat * matOffs1 * rotMat * matOffs2;

		_target->SetLocalMat(localMat);
	}
}

int PodushkaAnim::targetTag() const
{
	return _targetTag;
}

void PodushkaAnim::targetTag(int value)
{
	_targetTag = value;
	_target = NULL;
}




Behaviors::Behaviors(GameObject* gameObj): _gameObj(gameObj), storeProxy(true), storeSource(true)
{
	InitClassList();

	SetClassList(&classList);
}

Behaviors::~Behaviors()
{
	//Освобождаем занятые ресурсы
	Clear();
}

void Behaviors::InitClassList()
{
	static bool initClassList = false;

	if (!initClassList)
	{
		initClassList = true;

		classList.Add<TouchDeath>(btTouchDeath);
		classList.Add<ResurrectObj>(btResurrectObj);
		classList.Add<FxSystemWaitingEnd>(btFxSystemWaitingEnd);		
		classList.Add<FxSystemSrcSpeed>(btFxSystemSrcSpeed);		
		classList.Add<LowLifePoints>(btLowLifePoints);
		classList.Add<DamageEffect>(btDamageEffect);
		classList.Add<DeathEffect>(btDeathEffect);
		classList.Add<LifeEffect>(btLifeEffect);		
		classList.Add<SlowEffect>(btSlowEffect);
		classList.Add<PxWheelSlipEffect>(btPxWheelSlipEffect);
		classList.Add<ShotEffect>(btShotEffect);	
		classList.Add<ImmortalEffect>(btImmortalEffect);			
		classList.Add<SoundMotor>(btSoundMotor);
		classList.Add<GusenizaAnim>(btGusenizaAnim);
		classList.Add<PodushkaAnim>(btPodushkaAnim);
	}
}

void Behaviors::InsertItem(const Value& value)
{
	_MyBase::InsertItem(value);

	_gameObj->InsertListener(value);
}

void Behaviors::RemoveItem(const Value& value)
{
	_MyBase::RemoveItem(value);

	_gameObj->RemoveListener(value);
}

Behavior* Behaviors::Find(BehaviorType type)
{
	for (iterator iter = begin(); iter != end(); ++iter)
		if (iter->GetType() == type)
			return iter->GetItem();
	return 0;
}

void Behaviors::OnProgress(float deltaTime)
{
	for (Position pos = First(); Value* iter = Current(pos); Next(pos))
	{
		if ((*iter)->IsRemoved())
			Delete(pos);
		else
			(*iter)->OnProgress(deltaTime);
	}
}

void Behaviors::OnShot(const D3DXVECTOR3& pos)
{
	for (iterator iter = begin(); iter != end(); ++iter)
		(*iter)->OnShot(pos);
}

void Behaviors::OnMotor(float deltaTime, float rpm, float minRPM, float maxRPM)
{
	for (iterator iter = begin(); iter != end(); ++iter)
		(*iter)->OnMotor(deltaTime, rpm, minRPM, maxRPM);
}

void Behaviors::OnImmortalStatus(bool status)
{
	for (iterator iter = begin(); iter != end(); ++iter)
		(*iter)->OnImmortalStatus(status);
}

GameObject* Behaviors::GetGameObj()
{
	return _gameObj;
}

}

}