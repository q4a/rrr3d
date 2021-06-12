#include "stdafx.h"
#include "game\\World.h"

#include "game\\Logic.h"

namespace r3d
{

namespace game
{

LogicBehaviors::ClassList LogicBehaviors::classList;




LogicBehavior::LogicBehavior(LogicBehaviors* owner): _owner(owner)
{
}

void LogicBehavior::RegProgressEvent()
{
	_owner->GetLogic()->RegProgressEvent(this);
}

void LogicBehavior::UnregProgressEvent()
{
	_owner->GetLogic()->UnregProgressEvent(this);
}

LogicBehaviors* LogicBehavior::GetOwner()
{
	return _owner;
}

Logic* LogicBehavior::GetLogic()
{
	return _owner->GetLogic();
}

Map* LogicBehavior::GetMap()
{
	return GetLogic()->GetMap();
}




LogicEventEffect::LogicEventEffect(LogicBehaviors* owner): _MyBase(owner), _effect(0), _pos(NullVector)
{
	_gameObjEvent = new GameObjEvent(this);	
}

LogicEventEffect::~LogicEventEffect()
{
	ClearEffObjList();
	SetEffect(0);

	delete _gameObjEvent;
}

LogicEventEffect::GameObjEvent::GameObjEvent(LogicEventEffect* effect): _effect(effect)
{
}

void LogicEventEffect::GameObjEvent::OnDestroy(GameObject* sender)
{
	_effect->OnDestroyEffect(sender->GetMapObj());

	_effect->RemoveEffObj(sender->GetMapObj());
}

void LogicEventEffect::InsertEffObj(MapObj* mapObj)
{
	mapObj->AddRef();
	_effObjList.push_back(mapObj);
}

void LogicEventEffect::RemoveEffObj(MapObj* mapObj)
{
	mapObj->Release();
	_effObjList.Remove(mapObj);
}

void LogicEventEffect::ClearEffObjList()
{
	for (EffObjList::iterator iter = _effObjList.begin(); iter != _effObjList.end(); ++iter)
	{
		(*iter)->Release();
		DeleteEffect(*iter);
	}

	_effObjList.clear();
}

void LogicEventEffect::DestroyEffObj(MapObj* mapObj)
{
	mapObj->GetGameObj().RemoveListener(_gameObjEvent);
	GetMap()->DelMapObj(mapObj);
}

MapObj* LogicEventEffect::CreateEffect(const EffectDesc& desc)
{
	LSL_ASSERT(_effect);

	MapObj* mapObj = &GetMap()->AddMapObj(_effect);
	InsertEffObj(mapObj);
	mapObj->GetGameObj().InsertListener(_gameObjEvent);

	mapObj->GetGameObj().SetWorldPos(_pos + desc.pos);

	return mapObj;
}

MapObj* LogicEventEffect::CreateEffect()
{
	return CreateEffect(EffectDesc());
}

void LogicEventEffect::DeleteEffect(MapObj* mapObj)
{
	RemoveEffObj(mapObj);
	DestroyEffObj(mapObj);
}

void LogicEventEffect::Save(lsl::SWriter* writer)
{
	MapObjRec::Lib::SaveRecordRef(writer, "effect", _effect);

	writer->WriteValue("pos", _pos, 3);
}

void LogicEventEffect::Load(lsl::SReader* reader)
{
	SetEffect(MapObjRec::Lib::LoadRecordRef(reader, "effect"));

	reader->ReadValue("pos", _pos, 3);
}

MapObjRec* LogicEventEffect::GetEffect()
{
	return _effect;
}

void LogicEventEffect::SetEffect(MapObjRec* value)
{
	if (ReplaceRef(_effect, value))
		_effect = value;
}

const D3DXVECTOR3& LogicEventEffect::GetPos() const
{
	return _pos;	
}

void LogicEventEffect::SetPos(const D3DXVECTOR3& value)
{
	_pos = value;
}




PairPxContactEffect::PairPxContactEffect(LogicBehaviors* owner): _MyBase(owner)
{
	RegProgressEvent();
}

PairPxContactEffect::~PairPxContactEffect()
{
	UnregProgressEvent();

	ReleaseContacts(false);

	ClearSounds();
}

PairPxContactEffect::ContactMap::iterator PairPxContactEffect::GetOrCreateContact(const Key& key)
{
	std::pair<PairPxContactEffect::ContactMap::iterator, bool> res = _contactMap.insert(ContactMap::value_type(key, 0));
	if (res.second)
	{
		LSL_ASSERT(res.first->second == 0);

		ContactNode* node = new ContactNode();
		res.first->second = node;

		if (!node->source && !_sounds.empty())
		{
			unsigned index = static_cast<unsigned>(_sounds.size() * Random());
			index = lsl::ClampValue<unsigned>(index, 0, _sounds.size() - 1);

			node->source = GetLogic()->CreateSndSource3d(Logic::scEffects);
			node->source->SetSound(_sounds[index]);			
		}
	}

	return res.first;
}

void PairPxContactEffect::InsertContact(ContactMap::iterator iter, NxShape* shape1, NxShape* shape2, const D3DXVECTOR3& point)
{
	ContactNode* node = iter->second;
	
	if (node->last == node->list.end())
	{
		if (node->list.size() >= 2)
			return;

		node->last = node->list.insert(node->list.end(), Contact());		
	}
	//effect может быть удален из вне, например в RemoveContactByEffect
	if (!node->last->effect)
	{
		node->last->effect = CreateEffect();
		node->last->effect->AddRef();
	}

	LSL_ASSERT(node->last->effect);

	node->last->effect->GetGameObj().SetWorldPos(point);
	node->last->shape1 = shape1;
	node->last->shape2 = shape2;
	node->last->point = point;
	node->last->time = 0.0f;
	++(node->last);
}

PairPxContactEffect::ContactMap::iterator PairPxContactEffect::ReleaseContact(ContactMap::iterator iter, ContactList::iterator cIter1, ContactList::iterator cIter2, bool death, float deltaTime, float cRelTime)
{
	for (ContactList::iterator cIter = cIter1; cIter != cIter2; ++cIter)
	{
		cIter->time += deltaTime;
		//если время ожидания закончилось - освобождаем
		if (cIter->time > cRelTime)
		{
			//effect может быть удален из вне, например в RemoveContactByEffect
			if (cIter->effect)
			{
				cIter->effect->Release();
				if (death)
					cIter->effect->GetGameObj().Death();
			}
		}
		//иначе исключаем из очереди удаления
		else
		{
			cIter1 = cIter;
			++cIter1;
		}
	}

	iter->second->list.erase(cIter1, cIter2);
	if (iter->second->list.empty())
	{
		ContactNode* node = iter->second;
		if (node->source)
		{
			GetLogic()->ReleaseSndSource(node->source);
			node->source = 0;
		}
		delete node;

		return _contactMap.erase(iter);
	}
	else
		return ++iter;
}

void PairPxContactEffect::ReleaseContacts(bool death)
{
	for (ContactMap::iterator iter = _contactMap.begin(); iter != _contactMap.end();)
		iter = ReleaseContact(iter, iter->second->list.begin(), iter->second->list.end(), death);
	
	LSL_ASSERT(_contactMap.empty());
}

void PairPxContactEffect::RemoveContactByEffect(MapObj* effect)
{
	for (ContactMap::iterator iter = _contactMap.begin(); iter != _contactMap.end(); ++iter)
		for (ContactList::iterator cIter = iter->second->list.begin(); cIter != iter->second->list.end(); ++cIter)	
			if (cIter->effect == effect)
			{
				lsl::SafeRelease(cIter->effect);
				return;
			}
}

void PairPxContactEffect::OnDestroyEffect(MapObj* sender)
{
	RemoveContactByEffect(sender);
}

void PairPxContactEffect::OnContact(const px::Scene::OnContactEvent& contact1, const px::Scene::OnContactEvent& contact2)
{
#if !_DEBUG
	NxContactStreamIterator streamIter(contact1.stream);
	ContactNode* contNode = 0;
	ContactMap::iterator mapIter;

	while (streamIter.goNextPair())
	{
		NxShape* shapes[2];
		shapes[0] = !streamIter.isDeletedShape(0) ? streamIter.getShape(0) : 0;
		shapes[1] = !streamIter.isDeletedShape(1) ? streamIter.getShape(1) : 0;

		bool checkShapes = D3DXVec3Length(&contact1.sumFrictionForce) > 10000.0f;
		for (int i = 0; i < 2; ++i)
		{
			checkShapes &= shapes[i] && shapes[i]->getType() != NX_SHAPE_WHEEL;
			if (!checkShapes)
				break;
		}

		if (checkShapes)
		{
			if (!contNode)
			{
				mapIter = GetOrCreateContact(Key(contact1.actor, contact2.actor));
				contNode = mapIter->second;
			}

			LSL_ASSERT(contNode);

			bool activateSnd = false;

			while (streamIter.goNextPatch())
				while (streamIter.goNextPoint())
				{
					InsertContact(mapIter, shapes[0], shapes[1], streamIter.getPoint().get());

					if (contNode->source && !activateSnd)
					{
						activateSnd = true;
						contNode->source->SetPos3d(streamIter.getPoint().get());
						contNode->source->Play();
					}
				}
		}
		else
		{
			//Нельзя пропускать!
			while (streamIter.goNextPatch())
				while (streamIter.goNextPoint()) {}
		}
	}
#endif
}

void PairPxContactEffect::OnProgress(float deltaTime)
{
	for (ContactMap::iterator iter = _contactMap.begin(); iter != _contactMap.end();)
	{
		ContactNode* node = iter->second;
		ContactList::iterator lastIter = node->last;
		node->last = node->list.begin();
		iter = ReleaseContact(iter, lastIter, node->list.end(), true, deltaTime, 0.1f);
	}
}

void PairPxContactEffect::InsertSound(snd::Sound* value)
{
	_sounds.push_back(value);
	value->AddRef();
}

PairPxContactEffect::Sounds::iterator PairPxContactEffect::RemoveSound(Sounds::const_iterator iter)
{
	(*iter)->Release();

	return _sounds.erase(iter);
}

void PairPxContactEffect::RemoveSound(snd::Sound* sound)
{
	RemoveSound(_sounds.Find(sound));
}

void PairPxContactEffect::ClearSounds()
{
	for (Sounds::iterator iter = _sounds.begin(); iter != _sounds.end(); ++iter)	
		(*iter)->Release();

	_sounds.clear();
}

const PairPxContactEffect::Sounds& PairPxContactEffect::GetSounds() const
{
	return _sounds;
}




LogicBehaviors::LogicBehaviors(Logic* logic): _logic(logic)
{
	InitClassList();

	SetClassList(&classList);
}

LogicBehaviors::~LogicBehaviors()
{
	Clear();
}
	
void LogicBehaviors::InitClassList()
{
	static bool initClassList = false;

	if (!initClassList)
	{
		initClassList = true;

		classList.Add<PairPxContactEffect>(lbtPairPxContactEffect);
	}
}

void LogicBehaviors::OnContact(const px::Scene::OnContactEvent& contact1, const px::Scene::OnContactEvent& contact2)
{
	for (iterator iter = begin(); iter != end(); ++iter)
		(*iter)->OnContact(contact1, contact2);
}

Logic* LogicBehaviors::GetLogic()
{
	return _logic;
}




Logic::Logic(World* world): _world(world), _initSndCat(false), _touchBorderDamage(0, 0), _touchBorderDamageForce(0, 0), _touchCarDamage(0, 0), _touchCarDamageForce(0, 0)
{
	ZeroMemory(_volume, sizeof(_volume));
	ZeroMemory(_mute, sizeof(_mute));

	_behaviors = new LogicBehaviors(this);

	_pxSceneUser = new PxSceneUser(this);
	_world->GetPxScene()->InsertUser(_pxSceneUser);
}

Logic::~Logic()
{
	CleanGameObjs();

	_world->GetPxScene()->RemoveUser(_pxSceneUser);
	delete _pxSceneUser;

	delete _behaviors;

	FreeSndCat();
}

Logic::PxSceneUser::PxSceneUser(Logic* logic): _logic(logic)
{
}

void Logic::PxSceneUser::OnContact(const px::Scene::OnContactEvent& contact1, const px::Scene::OnContactEvent& contact2)
{
	_logic->_behaviors->OnContact(contact1, contact2);
}

void Logic::InitSndCat()
{
	if (!_initSndCat)
	{
		_initSndCat = true;

		for (int i = 0; i < cSndCategoryEnd; ++i)
		{
			_sndCat[i] = GetAudio()->CreateSubmixVoice();
			_volume[i] = _sndCat[i]->GetVolume();
		}
	}
}

void Logic::FreeSndCat()
{
	if (_initSndCat)
	{
		_initSndCat = false;

		for (int i = 0; i < cSndCategoryEnd; ++i)
			GetAudio()->ReleaseVoice(_sndCat[i]);
	}
}

void Logic::Save(lsl::SWriter* writer)
{
	writer->WriteValue("behaviors", _behaviors);
}

void Logic::Load(lsl::SReader* reader)
{
	reader->ReadValue("behaviors", _behaviors);
}

void Logic::RegFixedStepEvent(IFixedStepEvent* user)
{
	_world->RegFixedStepEvent(user);
}

void Logic::UnregFixedStepEvent(IFixedStepEvent* user)
{
	_world->UnregFixedStepEvent(user);
}

void Logic::RegProgressEvent(IProgressEvent* user)
{
	_world->RegProgressEvent(user);
}

void Logic::UnregProgressEvent(IProgressEvent* user)
{
	_world->UnregProgressEvent(user);
}

void Logic::RegLateProgressEvent(ILateProgressEvent* user)
{
	_world->RegLateProgressEvent(user);
}

void Logic::UnregLateProgressEvent(ILateProgressEvent* user)
{
	_world->UnregLateProgressEvent(user);
}

void Logic::RegFrameEvent(IFrameEvent* user)
{
	_world->RegFrameEvent(user);
}

void Logic::UnregFrameEvent(IFrameEvent* user)
{
	_world->UnregFrameEvent(user);
}

void Logic::RegGameObj(GameObject* gameObj)
{
	gameObj->AddRef();
	_gameObjList.push_back(gameObj);
}

void Logic::CleanGameObjs()
{
	for (GameObjList::const_iterator iter = _gameObjList.begin(); iter != _gameObjList.end(); ++iter)
	{
		(*iter)->Release();
		delete *iter;
	}

	_gameObjList.clear();
}

void Logic::Shot(Player* player, MapObj* target, Player::SlotType type)
{
	if (IsNetGame())
	{
		NetPlayer* netPlayer = GetNet()->GetPlayer(player);
		if (netPlayer)
			netPlayer->Shot(target, type);
	}
	else
	{
		Proj::ShotContext ctx;
		ctx.logic = this;
		ctx.shot.SetTargetMapObj(target);

		WeaponItem* wpn = player->GetSlotInst(type) ? player->GetSlotInst(type)->GetItem().IsWeaponItem() : NULL;
		if (wpn && wpn->IsReadyShot())			
			player->Shot(ctx, type, player->GetNextBonusProjId());
	}

	if (player && player->IsHuman() && type != Player::stHyper)
		_world->GetGame()->SendEvent(cHumanShot);
}

void Logic::Shot(Player* player, MapObj* target)
{
	if (IsNetGame())
	{
		NetPlayer* netPlayer = GetNet()->GetPlayer(player);
		if (netPlayer)
			netPlayer->Shot(target);
	}
	else 
	{
		Proj::ShotContext ctx;
		ctx.logic = this;
		ctx.shot.SetTargetMapObj(target);

		for (int i = Player::stWeapon1; i <= Player::stWeapon4; ++i)
		{
			Player::SlotType type = (Player::SlotType)i;
			WeaponItem* wpn = player->GetSlotInst(type) ? player->GetSlotInst(type)->GetItem().IsWeaponItem() : NULL;

			if (wpn && wpn->IsReadyShot())
				player->Shot(ctx, type, player->GetNextBonusProjId());
		}
	}

	if (player && player->IsHuman())
		_world->GetGame()->SendEvent(cHumanShot);
}

void Logic::Damage(GameObject* sender, int senderPlayerId, GameObject* target, float value, GameObject::DamageType damageType)
{
	LSL_ASSERT(target);

	if (target->GetMapObj() == NULL)
	{
		LSL_LOG("Logic::Damage target->GetMapObj() = NULL");
		return;
	}

	Player* targetPlayer = target->GetMapObj() ? target->GetMapObj()->GetPlayer() : NULL;
	if (targetPlayer)
	{
		Slot* slot = targetPlayer->GetSlotInst(Slot::stReflector);

		if (slot && damageType != GameObject::dtTouch)
		{
			ReflectorItem& reflector = slot->GetItem<ReflectorItem>();
			value = reflector.Reflect(value);
		}
	}

	if (IsNetGame())
	{
		GetNet()->race()->Damage(senderPlayerId, target->GetMapObj(), value, damageType);
	}
	else
	{
		target->Damage(senderPlayerId, value, damageType);
	}
}

bool Logic::TakeBonus(GameObject* sender, GameObject* bonus, GameObject::BonusType type, float value)
{
	LSL_ASSERT(sender && sender->GetMapObj());
	if (sender == NULL)
		return false;

	Player* player = GetRace()->GetPlayerByMapObj(sender->GetMapObj());
	LSL_ASSERT(player && bonus && bonus->GetMapObj());
	if (player == NULL)
		return false;

	if (IsNetGame())
	{
		NetPlayer* senderPlayer = GetNet()->GetPlayer(sender->GetMapObj());

		LSL_ASSERT(senderPlayer);

		senderPlayer->TakeBonus(bonus->GetMapObj(), type, value);
	}
	else
	{
		player->TakeBonus(bonus, type, value);
	}

	return true;
}

bool Logic::MineContact(Proj* sender, GameObject* target, const D3DXVECTOR3& point)
{
	LSL_ASSERT(sender && target && target->GetMapObj());
	if (target == NULL)
		return false;

	if (IsNetGame())
	{
		NetPlayer* targetPlayer = GetNet()->GetPlayer(target->GetMapObj());

		LSL_ASSERT(targetPlayer);

		targetPlayer->MineContact(sender, point);
	}
	else
	{
		sender->MineContact(target, point);
	}

	return true;
}

snd::Source* Logic::CreateSndSource(SndCategory category)
{
	InitSndCat();

	snd::Source* sound = GetAudio()->CreateSource();
	sound->InsertReceiver(_sndCat[category]);

	return sound;
}

snd::Source3d* Logic::CreateSndSource3d(SndCategory category)
{
	InitSndCat();

	snd::Source3d* sound = GetAudio()->CreateSource3d();
	sound->InsertReceiver(_sndCat[category]);

	return sound;
}

void Logic::ReleaseSndSource(snd::Source* source)
{
	GetAudio()->ReleaseVoice(source);
}

float Logic::GetVolume(SndCategory category)
{
	InitSndCat();

	return _sndCat[category]->GetVolume();
}

void Logic::SetVolume(SndCategory category, float value)
{
	InitSndCat();

	_volume[category] = value;

	if (!_mute[category])
		_sndCat[category]->SetVolume(value);
}

void Logic::AutodetectVolume()
{
	SetVolume(scMusic, 1.2f);
	SetVolume(scEffects, 0.8f);
	SetVolume(scVoice, 1.2f);
}

void Logic::Mute(SndCategory category, bool value)
{
	_mute[category] = value;

	_sndCat[category]->SetVolume(value ? 0.0f : _volume[category]);

}

const D3DXVECTOR2& Logic::GetTouchBorderDamage() const
{
	return _touchBorderDamage;
}

void Logic::SetTouchBorderDamage(const D3DXVECTOR2& value)
{
	_touchBorderDamage = value;
}

const D3DXVECTOR2& Logic::GetTouchBorderDamageForce() const
{
	return _touchBorderDamageForce;
}

void Logic::SetTouchBorderDamageForce(const D3DXVECTOR2& value)
{
	_touchBorderDamageForce = value;
}

const D3DXVECTOR2& Logic::GetTouchCarDamage() const
{
	return _touchCarDamage;
}

void Logic::SetTouchCarDamage(const D3DXVECTOR2& value)
{
	_touchCarDamage = value;
}

const D3DXVECTOR2& Logic::GetTouchCarDamageForce() const
{
	return _touchCarDamageForce;
}

void Logic::SetTouchCarDamageForce(const D3DXVECTOR2& value)
{
	_touchCarDamageForce = value;
}

void Logic::OnProgress(float deltaTime)
{
	MapObjLib::Category catList[3] = {MapObjLib::ctEffects, MapObjLib::ctCar, MapObjLib::ctBonus};

	GetMap()->GetMapObjList(MapObjLib::ctDecoration).OnProgressSpecial(deltaTime);

	for (int i = 0; i < ARRAY_LENGTH(catList); ++i)
		GetMap()->GetMapObjList(catList[i]).OnProgress(deltaTime);

	//for (int i = 0; i < MapObjLib::cCategoryEnd; ++i)
	//	GetMap()->GetMapObjList((MapObjLib::Category)i).OnProgress(deltaTime);

	for (GameObjList::iterator iter = _gameObjList.begin(); iter != _gameObjList.end();)
	{
		GameObject* inst = *iter;

		inst->OnProgress(deltaTime);

		if (inst->GetLiveState() == GameObject::lsDeath)
		{
			iter = _gameObjList.erase(iter);
			inst->Release();
			delete inst;
		}
		else
			++iter;
	}
}
bool Logic::IsNetGame()
{
	return GetNet()->isStarted();
}

Map* Logic::GetMap()
{
	return _world->GetMap();
}

Race* Logic::GetRace()
{
	return _world->GetGame()->GetRace();
}

NetGame* Logic::GetNet()
{
	return _world->GetGame()->netGame();
}

px::Scene* Logic::GetPxScene()
{
	return _world->GetPxScene();
}

snd::Engine* Logic::GetAudio()
{
	return _world->GetAudio();
}

LogicBehaviors& Logic::GetBehaviors()
{
	return *_behaviors;
}

}

}