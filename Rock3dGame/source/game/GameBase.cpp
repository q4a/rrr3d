#include "stdafx.h"
#include "game//GameBase.h"

#include "game//GameObject.h"
#include "game//GameCar.h"
#include "game//Weapon.h"
#include "game//Logic.h"

namespace r3d
{
	namespace game
	{
		const string cDifficultyStr[cDifficultyEnd] = {"gdEasy", "gdNormal", "gdHard", "gdMaster"};
		const char* cScActorTypeStr[cScActorTypeEnd] = {"satBaseObj", "satMeshObj"};

		const std::string GameObjListener::cBonusTypeStr[cBonusTypeEnd] = {
			"btMoney", "btCharge", "btMedpack", "btImmortal"
		};

		Behaviors::ClassList Behaviors::classList;


		Behavior::Behavior(Behaviors* owner): _owner(owner), _removed(false)
		{
		}

		void Behavior::Save(SWriter* writer)
		{
			if (_owner->storeProxy)
				SaveProxy(writer);
			if (_owner->storeSource)
				SaveSource(writer);
		}

		void Behavior::Load(SReader* reader)
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

		Behaviors* Behavior::GetOwner() const
		{
			return _owner;
		}

		GameObject* Behavior::GetGameObj() const
		{
			return _owner->GetGameObj();
		}

		Logic* Behavior::GetLogic() const
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
			const GameObject* parent = owner ? owner->GetOwner() : nullptr;
			if (owner && parent)
			{
				owner->LockDestr();

				const D3DXVECTOR3 pos = gameObj->GetWorldPos();
				const D3DXQUATERNION rot = gameObj->GetWorldRot();

				owner->Delete(mapObj);
				gameObj->SetParent(nullptr);
				gameObj->SetOwner(nullptr);
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
			for (auto& iter : node.GetNodes())
				if (iter.GetType() == graph::SceneNode::ntParticleSystem)
					iter.GetItem<graph::FxParticleSystem>()->SetModeFading(true);
		}

		void FxSystemWaitingEnd::OnProgress(float deltaTime)
		{
			if (IsResurrect())
			{
				unsigned resCnt = 0;

				graph::SceneNode& node = GetGameObj()->GetGrActor();
				for (auto& iter : node.GetNodes())
					if (iter.GetType() == graph::SceneNode::ntParticleSystem)
					{
						const graph::FxParticleSystem* fxSystem = iter.GetItem<graph::FxParticleSystem>();
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
			if (GetGameObj()->GetPxActor().GetNxActor() == nullptr)
				return;

			graph::SceneNode& node = GetGameObj()->GetGrActor();
			for (auto& iter : node.GetNodes())
				if (iter.GetType() == graph::SceneNode::ntParticleSystem)
				{
					auto* fxSystem = iter.GetItem<graph::FxParticleSystem>();

					D3DXVECTOR3 speed(GetGameObj()->GetPxActor().GetNxActor()->getLinearVelocity().get());
					if (GetGameObj()->GetParent())
						GetGameObj()->GetParent()->GetGrActor().WorldToLocalNorm(speed, speed);

					fxSystem->SetSrcSpeed(speed);
				}
		}


		EventEffect::EventEffect(Behaviors* owner): _MyBase(owner), _effect(nullptr), _pos(NullVector),
		                                            _impulse(NullVector), _ignoreRot(false), _makeEffect(nullptr)
		{
			_gameObjEvent = new GameObjEvent(this);
		}

		EventEffect::~EventEffect()
		{
			ClearEffObjList();

			ClearSounds();
			SetEffect(nullptr);

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
				_makeEffect = nullptr;

			mapObj->Release();
			_effObjList.Remove(mapObj);
		}

		void EventEffect::ClearEffObjList()
		{
			for (const auto& iter : _effObjList)
			{
				iter->Release();
				DestroyEffObj(iter);
			}

			_effObjList.clear();
		}

		void EventEffect::DestroyEffObj(MapObj* mapObj, bool destrWorld) const
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
			for (auto& _sound : _sounds)
			{
				if (_sound.source)
					continue;

				_sound.source = GetLogic()->CreateSndSource(Logic::scEffects);
				_sound.source->SetSound(_sound.sound);
			}
		}

		void EventEffect::FreeSource()
		{
			for (auto& _sound : _sounds)
			{
				if (_sound.source == nullptr)
					continue;

				GetLogic()->ReleaseSndSource(_sound.source);
				_sound.source = nullptr;
			}
		}

		void EventEffect::InitSource3d()
		{
			for (auto& _sound : _sounds)
			{
				if (_sound.source3d)
					continue;

				_sound.source3d = GetLogic()->CreateSndSource3d(Logic::scEffects);
				_sound.source3d->SetSound(_sound.sound);
			}
		}

		void EventEffect::FreeSource3d()
		{
			for (auto& _sound : _sounds)
			{
				if (_sound.source3d == nullptr)
					continue;

				GetLogic()->ReleaseSndSource(_sound.source3d);
				_sound.source3d = nullptr;
			}
		}

		MapObj* EventEffect::CreateEffect(const EffectDesc& desc)
		{
			LSL_ASSERT(_effect);

			MapObj* mapObj = nullptr;

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
			for (const auto& iter : _effObjList)
			{
				iter->Release();
				DestroyEffObj(iter, true);
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

				_makeEffect = nullptr;
			}
		}

		MapObj* EventEffect::GetMakeEffect() const
		{
			return _makeEffect;
		}

		bool EventEffect::IsEffectMaked() const
		{
			return _makeEffect ? true : false;
		}

		void EventEffect::SaveSource(SWriter* writer)
		{
			MapObjRec::Lib::SaveRecordRef(writer, "effect", _effect);

			int i = 0;
			SWriter* sounds = writer->NewDummyNode("sounds");
			for (auto iter = _sounds.begin(); iter != _sounds.end(); ++iter, ++i)
			{
				sounds->WriteRef(StrFmt("sound%d", i).c_str(), iter->sound);
			}

			writer->WriteValue("pos", _pos, 3);
			writer->WriteValue("impulse", _impulse, 3);
			writer->WriteValue("ignoreRot", _ignoreRot);
		}

		void EventEffect::LoadSource(SReader* reader)
		{
			ClearSounds();

			SReader* sounds = reader->ReadValue("sounds");
			if (sounds)
			{
				SReader* child = sounds->FirstChildValue();
				while (child)
				{
					child->AddFixUp(true, this, nullptr);
					child = child->NextValue();
				}
			}

			SReader* child = reader->ReadRef("sound", true, this, nullptr);

			SetEffect(MapObjRec::Lib::LoadRecordRef(reader, "effect"));

			reader->ReadValue("pos", _pos, 3);
			reader->ReadValue("impulse", _impulse, 3);
			reader->ReadValue("ignoreRot", _ignoreRot);
		}

		void EventEffect::OnFixUp(const FixUpNames& fixUpNames)
		{
			_MyBase::OnFixUp(fixUpNames);

			for (const auto& fixUpName : fixUpNames)
				if (fixUpName.sender->GetOwnerValue()->GetMyName() == "sounds")
				{
					AddSound(fixUpName.GetCollItem<snd::Sound*>());
					break;
				}
		}

		snd::Source* EventEffect::GiveSource()
		{
			InitSource();

			return !_sounds.empty() ? _sounds[RandomRange(0, _sounds.size() - 1)].source : nullptr;
		}

		snd::Source3d* EventEffect::GiveSource3d()
		{
			InitSource3d();

			return !_sounds.empty() ? _sounds[RandomRange(0, _sounds.size() - 1)].source3d : nullptr;
		}

		void EventEffect::OnProgress(float deltaTime)
		{
			for (const auto& _sound : _sounds)
				if (_sound.source3d)
				{
					_sound.source3d->SetPos3d(GetOwner()->GetGameObj()->GetWorldPos());
				}
		}

		MapObjRec* EventEffect::GetEffect() const
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

			for (const auto _sound : _sounds)
			{
				_sound.sound->Release();
			}

			_sounds.clear();
		}

		const EventEffect::SoundList& EventEffect::GetSounds()
		{
			return _sounds;
		}

		snd::Sound* EventEffect::GetSound() const
		{
			return !_sounds.empty() ? _sounds[0].sound : nullptr;
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

		void LowLifePoints::SaveSource(SWriter* writer)
		{
			_MyBase::SaveSource(writer);

			writer->WriteValue("lifeLevel", _lifeLevel);
		}

		void LowLifePoints::LoadSource(SReader* reader)
		{
			_MyBase::LoadSource(reader);

			reader->ReadValue("lifeLevel", _lifeLevel);
		}

		void LowLifePoints::OnProgress(float deltaTime)
		{
			_MyBase::OnProgress(deltaTime);

			const float maxLife = GetGameObj()->GetMaxLife();
			const float life = GetGameObj()->GetLife();

			if (GetGameObj()->GetLiveState() != GameObject::lsDeath && maxLife > 0.0f && life > 0 && life / maxLife <
				_lifeLevel)
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


		BurnEffect::BurnEffect(Behaviors* owner): _MyBase(owner)
		{
		}

		void BurnEffect::OnProgress(float deltaTime)
		{
			_MyBase::OnProgress(deltaTime);

			if (GetGameObj()->GetLiveState() != GameObject::lsDeath && GetGameObj()->IsCar()->IsBurn())
			{
				MakeEffect();
			}
			else
				FreeEffect();
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

		void DamageEffect::SaveSource(SWriter* writer)
		{
			_MyBase::SaveSource(writer);

			writer->WriteValue("damageType", _damageType);
		}

		void DamageEffect::LoadSource(SReader* reader)
		{
			_MyBase::LoadSource(reader);

			int damageType;
			reader->ReadValue("damageType", damageType);
			_damageType = static_cast<DamageType>(damageType);
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

				NxActor* eff = GetMakeEffect() ? GetMakeEffect()->GetGameObj().GetPxActor().GetNxActor() : nullptr;
				NxActor* car = sender && sender->GetParent() && sender->GetParent()->IsProj() && sender->GetParent()->
				               IsProj()->GetWeapon()
					               ? sender->GetParent()->IsProj()->GetWeapon()->GetPxActor().GetNxActor()
					               : nullptr;

				if (_effectPxIgnoreSenderCar && eff && car)
					eff->getScene().setActorPairFlags(*eff, *car, NX_IGNORE_PAIR);
			}
		}

		void DeathEffect::SaveSource(SWriter* writer)
		{
			_MyBase::SaveSource(writer);

			writer->WriteValue("effectPxIgnoreSenderCar", _effectPxIgnoreSenderCar);
			writer->WriteValue("targetChild", _targetChild);
		}

		void DeathEffect::LoadSource(SReader* reader)
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

		/////////////////////
		///////////////////
		////////////////
		/////////////////

		////////////////////
		//////////////////
		///////////////////
		//////////////////
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
			constexpr float slipLong = 0.4f;
			constexpr float slipLat = 0.7f; //const float slipLat = 0.6f;

			const float volumeK = 4.0f;

			auto& wheel = lsl::StaticCast<CarWheel&>(*GetGameObj());
			snd::Source3d* source = GiveSource3d();

			NxWheelContactData contactDesc;
			const NxShape* contact = wheel.GetShape()->GetNxShape()->getContact(contactDesc);
			float slip = 0.0f;
			if (contact)
				slip = std::max(abs(contactDesc.lateralSlip) - slipLat, 0.0f) + std::max(
					abs(contactDesc.longitudalSlip) - slipLong, 0.0f);

			if (contact && wheel.GetParent()->IsCar()->GetWastedControl())
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
					source->SetVolume(ClampValue(slip * volumeK, 0.0f, 1.0f));
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


		PxDriftEffect::PxDriftEffect(Behaviors* owner): _MyBase(owner)
		{
		}

		PxDriftEffect::~PxDriftEffect()
		{
			DeleteAllEffects();
		}

		void PxDriftEffect::OnProgress(float deltaTime)
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
			const float slipLat = 0.7f; //const float slipLat = 0.6f;

			const float volumeK = 4.0f;

			auto& wheel = lsl::StaticCast<CarWheel&>(*GetGameObj());
			snd::Source3d* source = GiveSource3d();

			NxWheelContactData contactDesc;
			const NxShape* contact = wheel.GetShape()->GetNxShape()->getContact(contactDesc);
			float slip = 0.0f;
			if (contact)
				slip = std::max(abs(contactDesc.lateralSlip) - slipLat, 0.0f) + std::max(
					abs(contactDesc.longitudalSlip) - slipLong, 0.0f);

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
					source->SetVolume(ClampValue(slip * volumeK, 0.0f, 1.0f));
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


		ImmortalEffect::ImmortalEffect(Behaviors* owner): _MyBase(owner), _fadeInTime(-1.0f), _fadeOutTime(-1.0f),
		                                                  _dmgTime(-1.0f), _scale(IdentityVector),
		                                                  _scaleK(IdentityVector)
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
					const AABB aabb1 = GetGameObj()->GetGrActor().GetLocalAABB(false);
					const AABB aabb2 = GetMakeEffect()->GetGameObj().GetGrActor().GetLocalAABB(true);
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

		void ImmortalEffect::SaveSource(SWriter* writer)
		{
			_MyBase::SaveSource(writer);

			SWriteValue(writer, "scaleK", _scaleK);
		}

		void ImmortalEffect::LoadSource(SReader* reader)
		{
			_MyBase::LoadSource(reader);

			SReadValue(reader, "scaleK", _scaleK);
		}


		void ImmortalEffect::OnProgress(float deltaTime)
		{
			MapObj* mapObj = GetMakeEffect();

			if (mapObj && _dmgTime >= 0)
			{
				const float alpha = ClampValue(_dmgTime / 0.25f, 0.0f, 1.0f);

				const graph::Actor::Nodes& nodes = GetMakeEffect()->GetGameObj().GetGrActor().GetNodes();
				for (const auto node : nodes)
				{
					graph::MaterialNode* material = node->GetMaterial();
					if (material == nullptr)
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
				const float alpha = ClampValue(_fadeInTime / 0.5f, 0.0f, 1.0f);
				mapObj->GetGameObj().SetScale(_scale * alpha);
				if (alpha >= 1.0f)
					_fadeInTime = -1.0f;
			}

			if (mapObj && _fadeOutTime >= 0)
			{
				_fadeOutTime += deltaTime;
				const float alpha = ClampValue(_fadeOutTime / 0.5f, 0.0f, 1.0f);
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

		///////////////////////////
		ShellEffect::ShellEffect(Behaviors* owner): _MyBase(owner), _fadeInTime(-1.0f), _fadeOutTime(-1.0f),
		                                            _dmgTime(-1.0f), _scale(IdentityVector), _scaleK(IdentityVector)
		{
		}

		void ShellEffect::OnShellStatus(bool status)
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
					const AABB aabb1 = GetGameObj()->GetGrActor().GetLocalAABB(false);
					const AABB aabb2 = GetMakeEffect()->GetGameObj().GetGrActor().GetLocalAABB(true);
					_scale = aabb1.GetSizes() / aabb2.GetSizes() * _scaleK;

					GetMakeEffect()->GetGameObj().SetScale(0.0f);
				}
			}
			else
			{
				_fadeOutTime = 0.0f;
			}
		}

		void ShellEffect::OnDamage(GameObject* sender, float value, DamageType damageType)
		{
			if (GetMakeEffect())
			{
				_dmgTime = 0;
			}
		}

		void ShellEffect::SaveSource(SWriter* writer)
		{
			_MyBase::SaveSource(writer);

			SWriteValue(writer, "scaleK", _scaleK);
		}

		void ShellEffect::LoadSource(SReader* reader)
		{
			_MyBase::LoadSource(reader);

			SReadValue(reader, "scaleK", _scaleK);
		}


		void ShellEffect::OnProgress(float deltaTime)
		{
			MapObj* mapObj = GetMakeEffect();

			if (mapObj && _dmgTime >= 0)
			{
				const float alpha = ClampValue(_dmgTime / 0.25f, 0.0f, 1.0f);

				const graph::Actor::Nodes& nodes = GetMakeEffect()->GetGameObj().GetGrActor().GetNodes();
				for (const auto node : nodes)
				{
					graph::MaterialNode* material = node->GetMaterial();
					if (material == nullptr)
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
				const float alpha = ClampValue(_fadeInTime / 0.5f, 0.0f, 1.0f);
				mapObj->GetGameObj().SetScale(_scale * alpha);
				if (alpha >= 1.0f)
					_fadeInTime = -1.0f;
			}

			if (mapObj && _fadeOutTime >= 0)
			{
				_fadeOutTime += deltaTime;
				const float alpha = ClampValue(_fadeOutTime / 0.5f, 0.0f, 1.0f);
				mapObj->GetGameObj().SetScale(_scale * (1.0f - alpha));
				if (alpha >= 1.0f)
				{
					_fadeOutTime = -1.0f;
					FreeEffect(true);
				}
			}
		}

		const D3DXVECTOR3& ShellEffect::GetScaleK() const
		{
			return _scaleK;
		}

		void ShellEffect::SetScaleK(const D3DXVECTOR3& value)
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

			GameCar* car = GetGameObj()->IsCar();
			NxActor* target = GetGameObj()->GetPxActor().GetNxActor();
			if (target)
			{
				NxVec3 linSpeed = target->getLinearVelocity();
				const float maxSpeed = linSpeed.magnitude();
				linSpeed.normalize();

				if (car->IsShell() == false && car->IsImmortal() == false && maxSpeed > 20)
				{
					target->setLinearVelocity(linSpeed * 20);
					car->SetTurnFreeze(true);
				}
			}
		}


		SuperSlowEffect::SuperSlowEffect(Behaviors* owner): _MyBase(owner)
		{
		}

		void SuperSlowEffect::OnDestroyEffect(MapObj* sender)
		{
			GetGameObj()->GetPxActor().GetNxActor()->setLinearDamping(0.0f);
			Remove();
		}


		void SuperSlowEffect::OnProgress(float deltaTime)
		{
			_MyBase::OnProgress(deltaTime);

			MakeEffect();

			GameCar* car = GetGameObj()->IsCar();
			NxActor* target = GetGameObj()->GetPxActor().GetNxActor();
			if (target)
			{
				NxVec3 linSpeed = target->getLinearVelocity();
				const float maxSpeed = linSpeed.magnitude();
				linSpeed.normalize();

				if (car->IsAnyWheelContact() == true && car->IsShell() == false && car->IsImmortal() == false &&
					maxSpeed > 1.0f && maxSpeed > 25)
				{
					target->setLinearVelocity(linSpeed * 25);
					car->GetMapObj()->GetPlayer()->SetMineFreeze(true);
				}
			}
		}


		SoundMotor::SoundMotor(Behaviors* owner): _MyBase(owner), _sndIdle(nullptr), _sndRPM(nullptr), _init(false),
		                                          _srcIdle(nullptr), _srcRPM(nullptr), _rpmVolumeRange(0.0f, 1.0f),
		                                          _rpmFreqRange(0.0f, 1.0f)
		{
		}

		SoundMotor::~SoundMotor()
		{
			Free();

			SetSndIdle(nullptr);
			SetSndRPM(nullptr);
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
			Init();

			if (_init)
			{
				constexpr float motorLag = 10000.0f;
				const float distRPM = rpm - _curRPM;
				const float rpmDT = motorLag * deltaTime * (distRPM > 0 ? 1.0f : -1.0f);
				_curRPM = _curRPM + ClampValue(rpmDT, -abs(distRPM), abs(distRPM));

				const float idleAlpha = ClampValue(0.5f * (_curRPM - minRPM) / minRPM, 0.0f, 1.0f);
				const float alpha = ClampValue((_curRPM - minRPM) / (maxRPM - minRPM), 0.0f, 1.0f);

				_srcRPM->Play();
				_srcRPM->SetVolume((_rpmVolumeRange.x + alpha * (_rpmVolumeRange.y - _rpmVolumeRange.x)) * idleAlpha);
				_srcRPM->SetFrequencyRatio((_rpmFreqRange.x + alpha * (_rpmFreqRange.y - _rpmFreqRange.x)));

				_srcIdle->Play();
				_srcIdle->SetVolume(1.0f - idleAlpha);
			}
		}

		void SoundMotor::SaveSource(SWriter* writer)
		{
			_MyBase::SaveSource(writer);

			writer->WriteRef("sndIdle", _sndIdle);
			writer->WriteRef("sndRPM", _sndRPM);

			SWriteValue(writer, "rpmVolumeRange", _rpmVolumeRange);
			SWriteValue(writer, "rpmFreqRange", _rpmFreqRange);
		}

		void SoundMotor::LoadSource(SReader* reader)
		{
			_MyBase::LoadSource(reader);

			reader->ReadRef("sndIdle", true, this, nullptr);
			reader->ReadRef("sndRPM", true, this, nullptr);

			SReadValue(reader, "rpmVolumeRange", _rpmVolumeRange);
			SReadValue(reader, "rpmFreqRange", _rpmFreqRange);
		}

		void SoundMotor::OnFixUp(const FixUpNames& fixUpNames)
		{
			_MyBase::OnFixUp(fixUpNames);

			for (const auto& fixUpName : fixUpNames)
			{
				if (fixUpName.name == "sndIdle")
				{
					SetSndIdle(fixUpName.GetCollItem<snd::Sound*>());
					break;
				}
				if (fixUpName.name == "sndRPM")
				{
					SetSndRPM(fixUpName.GetCollItem<snd::Sound*>());
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

		snd::Sound* SoundMotor::GetSndIdle() const
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

		snd::Sound* SoundMotor::GetSndRPM() const
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
		= default;

		void GusenizaAnim::OnProgress(float deltaTime)
		{
			constexpr float gusLength = 2.5f * 2.0f;

			LSL_ASSERT(GetGameObj()->GetParent());

			const auto car = lsl::StaticCast<GameCar*>(GetGameObj()->GetParent());
			graph::Actor& actor = GetGameObj()->GetGrActor();
			graph::MaterialNode* mat = !actor.GetNodes().Empty() ? actor.GetNodes().front().GetMaterial() : nullptr;

			LSL_ASSERT(mat);

			const float linSpeed = car->GetLeadWheelSpeed();
			_xAnimOff -= linSpeed * deltaTime / gusLength;
			//выделяем дробную часть
			_xAnimOff = _xAnimOff - floor(_xAnimOff);
			const D3DXVECTOR3 offset(1.0f - _xAnimOff, 0, 0.0f);

			mat->SetOffset(offset);
		}


		PodushkaAnim::PodushkaAnim(Behaviors* owner): _MyBase(owner), _targetTag(0), _target(nullptr)
		{
		}

		void PodushkaAnim::SaveSource(SWriter* writer)
		{
			_MyBase::SaveSource(writer);

			writer->WriteValue("targetTag", _targetTag);
		}

		void PodushkaAnim::LoadSource(SReader* reader)
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
			_target = nullptr;
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
				classList.Add<BurnEffect>(btBurnEffect);
				classList.Add<DamageEffect>(btDamageEffect);
				classList.Add<DeathEffect>(btDeathEffect);
				classList.Add<LifeEffect>(btLifeEffect);
				classList.Add<SlowEffect>(btSlowEffect);
				classList.Add<SuperSlowEffect>(btSuperSlowEffect);
				classList.Add<PxWheelSlipEffect>(btPxWheelSlipEffect);
				classList.Add<PxDriftEffect>(btPxDriftEffect);
				classList.Add<ShotEffect>(btShotEffect);
				classList.Add<ImmortalEffect>(btImmortalEffect);
				classList.Add<ShellEffect>(btShellEffect);
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
			for (auto& iter : *this)
				if (iter.GetType() == type)
					return iter.GetItem();
			return nullptr;
		}

		void Behaviors::OnProgress(float deltaTime)
		{
			for (Position pos = First(); const Value* iter = Current(pos); Next(pos))
			{
				if ((*iter)->IsRemoved())
					Delete(pos);
				else
					(*iter)->OnProgress(deltaTime);
			}
		}

		void Behaviors::OnShot(const D3DXVECTOR3& pos)
		{
			for (const auto& iter : *this)
				iter->OnShot(pos);
		}

		void Behaviors::OnMotor(float deltaTime, float rpm, float minRPM, float maxRPM)
		{
			for (const auto& iter : *this)
				iter->OnMotor(deltaTime, rpm, minRPM, maxRPM);
		}

		void Behaviors::OnImmortalStatus(bool status)
		{
			for (const auto& iter : *this)
				iter->OnImmortalStatus(status);
		}

		void Behaviors::OnShellStatus(bool status)
		{
			for (const auto& iter : *this)
				iter->OnShellStatus(status);
		}

		GameObject* Behaviors::GetGameObj() const
		{
			return _gameObj;
		}
	}
}
