#include "stdafx.h"
#include "game//Logic.h"

#include "game//GameObject.h"
#include "game//World.h"
#include "GraphManager.h"

namespace r3d
{
	namespace game
	{
		GameObject::GameObject(): _mapObj(nullptr), _logic(nullptr), _parent(nullptr), _liveState(lsLive), _maxLife(-1),
		                          _reverse(false), _rage(false), _maxTimeLife(-1), _life(_maxLife), _timeLife(0),
		                          _immortalFlag(false), _shellFlag(false), _immortalTime(0), _shellTime(0),
		                          _destroy(false), _frameEventCount(0), _progressEventCount(0),
		                          _lateProgressEventCount(0), _fixedStepEventCount(0), _syncFrameEvent(false),
		                          _bodyProgressEvent(false), _touchPlayerId(cUndefPlayerId), _touchPlayerTime(0),
		                          _posSync(NullVector), _posSyncDir(NullVector), _posSyncLength(0),
		                          _rotSync(NullQuaternion), _rotSyncAxis(NullVector), _rotSyncAngle(0.0f),
		                          _posSync2(NullVector), _posSyncDir2(NullVector), _posSyncDist2(0), _posSyncLength2(0),
		                          _rotSync2(NullQuaternion), _rotSyncAxis2(NullVector), _rotSyncAngle2(0),
		                          _rotSyncLength2(0), _pxPosLerp(NullVector), _pxRotLerp(NullQuaternion),
		                          _pxVelocityLerp(NullVector), _pxPrevPos(NullVector), _pxPrevRot(NullQuaternion),
		                          _pxPrevVelocity(NullVector), storeSource(true), storeProxy(true)
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

			SetLogic(nullptr);
			SetParent(nullptr);

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

				for (ListenerList::Position pos = _listenerList.First(); GameObjListener** iter = _listenerList.
					     Current(pos); _listenerList.Next(pos))
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

		D3DXVECTOR3 GameObject::GetContactPoint(const px::Scene::OnContactEvent& contact)
		{
			NxContactStreamIterator contIter(contact.stream);

			while (contIter.goNextPair())
				while (contIter.goNextPatch())
					while (contIter.goNextPoint())
					{
						return contIter.getPoint().get();
					}

			return NullVector;
		}

		bool GameObject::ContainsContactGroup(NxContactStreamIterator& contIter, unsigned actorIndex,
		                                      px::Scene::CollDisGroup group)
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
			for (ListenerList::Position pos = _listenerList.First(); GameObjListener** iter = _listenerList.Current(pos)
			     ; _listenerList.Next(pos))
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

		void GameObject::RayCastClosestActor(const D3DXVECTOR3& rayStart, const D3DXVECTOR3& rayDir,
		                                     NxShapesType shapesType, RayCastHit& hit, unsigned groups, unsigned mask,
		                                     float maxDist) const
		{
			NxRaycastHit nxHit;
			hit.gameActor = nullptr;

			NxGroupsMask nxMask;
			nxMask.bits0 = mask;
			nxMask.bits1 = 0;
			nxMask.bits2 = 0;
			nxMask.bits3 = 0;

			if (NxShape* shape = GetPxActor().GetScene()->GetNxScene()->raycastClosestShape(
				NxRay(NxVec3(rayStart), NxVec3(rayDir)), shapesType, nxHit, groups, maxDist, 0xFFFFFFFF,
				mask > 0 ? &nxMask : nullptr))
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

			for (ListenerList::Position pos = _listenerList.First(); GameObjListener** iter = _listenerList.Current(pos)
			     ; _listenerList.Next(pos))
				(*iter)->OnDeath(this, damageType, target);
		}

		void GameObject::OnPxSync(float alpha)
		{
			const NxActor* nxActor = _pxActor->GetNxActor();
			if (nxActor == nullptr)
				return;

			const D3DXVECTOR3 pxVelocityLerp = nxActor->getLinearVelocity().get();

			if (alpha < 1.0f)
			{
				D3DXVec3Lerp(&_pxPosLerp, &_pxPrevPos, &_pxActor->GetPos(), alpha);
				D3DXQuaternionSlerp(&_pxRotLerp, &_pxPrevRot, &_pxActor->GetRot(), alpha);
				D3DXVec3Lerp(&_pxVelocityLerp, &_pxPrevVelocity, &pxVelocityLerp, alpha);
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

			if (_shellTime > 0)
			{
				_shellTime -= deltaTime;
				if (_shellTime <= 0)
				{
					_shellTime = 0;
					OnShellStatus(false);
					_behaviors->OnShellStatus(false);
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
						_rotSyncAngle = std::max(_rotSyncAngle - 1.3f * D3DX_PI * deltaTime, 0.0f);
					else
						_rotSyncAngle = std::min(_rotSyncAngle + 1.3f * D3DX_PI * deltaTime, 0.0f);
					D3DXQUATERNION rot;
					D3DXQuaternionRotationAxis(&rot, &_rotSyncAxis, -_rotSyncAngle);
					_grActor->SetRot(_grActor->GetRot() * rot);
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
						_rotSyncAngle2 = std::max(_rotSyncAngle2 - 1.3f * D3DX_PI * deltaTime, 0.0f);
					else
						_rotSyncAngle2 = std::min(_rotSyncAngle2 + 1.3f * D3DX_PI * deltaTime, 0.0f);

					D3DXQUATERNION rot;
					D3DXQuaternionRotationAxis(&rot, &_rotSyncAxis2, -_rotSyncAngle2);
					_grActor->SetRot(_grActor->GetRot() * _rotSync2 * rot);
				}
				else if (_rotSyncLength2 != 0)
				{
					_rotSyncAngle2 = 0.0f;
					_grActor->SetRot(_grActor->GetRot() * _rotSync2);
				}
			}
		}

		void GameObject::OnFixedStep(float deltaTime)
		{
		}

		void GameObject::SaveSource(SWriter* writer)
		{
			writer->WriteValue("grActor", _grActor);
			writer->WriteValue("pxActor", _pxActor);

			writer->WriteValue("maxLife", _maxLife);
			writer->WriteValue("reverse", _reverse);
			writer->WriteValue("rage", _rage);

			//������� ��������, ����. ������. ���� ��� ������������� � ����� �����������
			//if (GetOwner() != GetParent())
			//	writer->WriteRef("parent", _parent);
		}

		void GameObject::LoadSource(SReader* reader)
		{
			reader->ReadValue("grActor", _grActor);
			reader->ReadValue("pxActor", _pxActor);

			reader->ReadValue("maxLife", _maxLife);
			reader->ReadValue("reverse", _reverse);
			reader->ReadValue("rage", _rage);

			//reader->ReadRef("parent", false, this, 0);
		}

		void GameObject::SaveProxy(SWriter* writer)
		{
			writer->WriteValue("pos", GetPos(), 3);
			writer->WriteValue("scale", GetScale(), 3);
			writer->WriteValue("rot", GetRot(), 4);

			writer->WriteValue("life", _life);
			writer->WriteValue("maxTimeLife", _maxTimeLife);
			writer->WriteValue("timeLife", _timeLife);

			_behaviors->storeSource = true;
			_behaviors->storeProxy = true;
			writer->WriteValue("behaviors", _behaviors);
		}

		void GameObject::LoadProxy(SReader* reader)
		{
			D3DXVECTOR3 pos;
			D3DXVECTOR3 scale;
			D3DXQUATERNION rot;

			reader->ReadValue("pos", pos, 3);
			reader->ReadValue("scale", scale, 3);
			reader->ReadValue("rot", rot, 4);

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

		void GameObject::Save(SWriter* writer)
		{
			if (storeSource)
				SaveSource(writer);
			if (storeProxy)
				SaveProxy(writer);

			_includeList->SetStoreOpt(storeSource, storeProxy);
			writer->WriteValue("includeList", _includeList);
		}

		void GameObject::Load(SReader* reader)
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

		NxActor* GameObject::GetNxActor() const
		{
			NxActor* nxActor = _pxActor->GetNxActor();
			if (!nxActor)
				throw Error("GameObject::GetNxActor(), not initializated");

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

			value->_parent = nullptr;
			_children.remove(value);
			value->_grActor->SetParent(nullptr);
			value->_pxActor->SetParent(nullptr);
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
			for (const auto iter : _listenerList)
				iter->Release();

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
			data.targetPlayerId = GetMapObj() && GetMapObj()->GetPlayer()
				                      ? GetMapObj()->GetPlayer()->GetId()
				                      : cUndefPlayerId;
			data.damage = value;
			data.damageType = damageType;

			_life = newLife;

			if (_liveState != lsDeath)
			{
				for (ListenerList::Position pos = _listenerList.First(); GameObjListener** iter = _listenerList.
					     Current(pos); _listenerList.Next(pos))
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
						//attackbonus ��������� ����� ����� �����������.
						if (GetMapObj()->GetPlayer() != nullptr)
						{
							Player* plr = GetLogic()->GetRace()->GetPlayerById(senderPlayerId);
							const Player* victim = GetMapObj()->GetPlayer();
							//���� ������ �� ��������
							if (victim->GetId() != plr->GetId())
							{
								//��� ����������� � �������� ������� 100$
								if (damageType == dtAim || damageType == dtEnergy)
								{
									SendEvent(cPlayerKill, senderPlayerId, &data);
									plr->AddPickMoney(100);
									plr->AddKillsTotal(1);
								}

								//��� �������� ������, � ����� ������ ������� 500$
								if (damageType == dtSimple || damageType == dtLaser || damageType == dtTouch)
								{
									SendEvent(cPlayerKill, senderPlayerId, &data);
									plr->AddPickMoney(500);
									plr->AddKillsTotal(1);
								}

								//��� ��� ������� 250$
								if (damageType == dtMine)
								{
									SendEvent(cMineKill, senderPlayerId, &data);
									plr->AddPickMoney(250);
									plr->AddKillsTotal(1);
								}

								//��� ���������� ������ ������� 750$
								if (damageType == dtCash)
								{
									SendEvent(cPlayerKill, senderPlayerId, &data);
									plr->AddPickMoney(750);
									plr->AddKillsTotal(1);
								}

								//��� ���������� ��� �������, �� � ���������� ���� �������.
								if (damageType == dtNull)
								{
									plr->AddKillsTotal(1);
								}
							}
							else
							{
								//SendEvent(cPlayerSuicide, senderPlayerId, &data); //massmod ���� �� �������! ����� ������� ������� � ����� ��� �����������.
							}
						}
					}

					SendDeath(damageType);
				}
			}
		}

		void GameObject::Damage(int senderPlayerId, float value, DamageType damageType)
		{
			const float newLife = !IsImmortal() && !IsShell() ? _life - value : _life;
			const bool death = !IsImmortal() && !IsShell() && newLife <= 0.0f;

			Damage(senderPlayerId, value, newLife, death, damageType);
		}

		void GameObject::Healt(float life)
		{
			_life = std::min(_life + life, _maxLife);
		}

		void GameObject::LowLife(Behavior* behavior)
		{
			OnLowLife(this, behavior);

			for (ListenerList::Position pos = _listenerList.First(); GameObjListener** iter = _listenerList.Current(pos)
			     ; _listenerList.Next(pos))
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

		bool GameObject::GetShellFlag() const
		{
			return _shellFlag;
		}

		void GameObject::SetShellFlag(bool value)
		{
			_shellFlag = value;
		}

		void GameObject::Immortal(float time)
		{
			const bool onStatus = _immortalTime == 0;
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

		void GameObject::Shell(float time)
		{
			const bool onStatus = _shellTime == 0;
			_shellTime = time;

			if (onStatus)
			{
				OnShellStatus(true);
				_behaviors->OnShellStatus(true);
			}
		}

		bool GameObject::IsShell() const
		{
			return _maxLife < 0 || _shellTime > 0 || _shellFlag;
		}

		MapObj* GameObject::GetMapObj() const
		{
			return _mapObj;
		}

		Logic* GameObject::GetLogic() const
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

				for (const auto& iter : _children)
					iter->SetLogic(value);

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

		void GameObject::SendEvent(unsigned id, int playerId, MyEventData* data) const
		{
			if (_logic == nullptr || GetMapObj() == nullptr)
				return;

			GameMode* game = _logic->GetMap()->GetWorld()->GetGame();

			if (data)
				data->playerId = playerId;

			game->SendEvent(id, data ? data : &MyEventData(playerId));
		}

		void GameObject::SendEvent(unsigned id, MyEventData* data) const
		{
			SendEvent(id, GetMapObj()->GetPlayer() ? GetMapObj()->GetPlayer()->GetId() : cUndefPlayerId, data);
		}

		GameObject* GameObject::GetParent() const
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

		GameObject::IncludeList& GameObject::GetIncludeList() const
		{
			return *_includeList;
		}

		graph::Actor& GameObject::GetGrActor() const
		{
			return *_grActor;
		}

		px::Actor& GameObject::GetPxActor() const
		{
			return *_pxActor;
		}

		Proj* GameObject::IsProj()
		{
			return nullptr;
		}

		GameCar* GameObject::IsCar()
		{
			return nullptr;
		}

		const D3DXVECTOR3& GameObject::GetPos() const
		{
			return _grActor->GetPos();
		}

		void GameObject::SetPos(const D3DXVECTOR3& value)
		{
			_grActor->SetPos(value);
			_pxActor->SetPos(value);
			_pxPrevPos = value;
		}

		const D3DXVECTOR3& GameObject::GetScale() const
		{
			return _grActor->GetScale();
		}

		void GameObject::SetScale(const D3DXVECTOR3& value)
		{
			D3DXVec3Length(&value);

			_grActor->SetScale(value);
			_pxActor->SetScale(value);
		}

		void GameObject::SetScale(float value)
		{
			SetScale(D3DXVECTOR3(value, value, value));
		}

		const D3DXQUATERNION& GameObject::GetRot() const
		{
			return _grActor->GetRot();
		}

		void GameObject::SetRot(const D3DXQUATERNION& value)
		{
			_grActor->SetRot(value);
			_pxActor->SetRot(value);
			_pxPrevRot = value;
		}

		D3DXVECTOR3 GameObject::GetWorldPos() const
		{
			return _grActor->GetWorldPos();
		}

		void GameObject::SetWorldPos(const D3DXVECTOR3& value)
		{
			_grActor->SetWorldPos(value);
			_pxActor->SetPos(_grActor->GetPos());
			_pxPrevPos = _grActor->GetPos();
		}

		D3DXQUATERNION GameObject::GetWorldRot() const
		{
			return _grActor->GetWorldRot();
		}

		void GameObject::SetWorldRot(const D3DXQUATERNION& value)
		{
			_grActor->SetWorldRot(value);
			_pxActor->SetRot(_grActor->GetRot());
			_pxPrevRot = _grActor->GetRot();
		}

		void GameObject::SetWorldDir(const D3DXVECTOR3& value)
		{
			D3DXVECTOR3 vec3 = value;
			if (_grActor->GetParent())
				_grActor->GetParent()->WorldToLocalNorm(vec3, vec3);

			_grActor->SetDir(vec3);
			_pxActor->SetRot(_grActor->GetRot());
			_pxPrevRot = _grActor->GetRot();
		}

		void GameObject::SetWorldUp(const D3DXVECTOR3& value)
		{
			D3DXVECTOR3 vec3 = value;
			if (_grActor->GetParent())
				_grActor->GetParent()->WorldToLocalNorm(vec3, vec3);

			_grActor->SetUp(vec3);
			_pxActor->SetRot(_grActor->GetRot());
			_pxPrevRot = _grActor->GetRot();
		}

		const D3DXVECTOR3& GameObject::GetPosSync() const
		{
			return _posSync;
		}

		void GameObject::SetPosSync(const D3DXVECTOR3& value)
		{
			_posSync = value;
			D3DXVec3Normalize(&_posSyncDir, &value);
			_posSyncLength = D3DXVec3Length(&value);

			SetSyncFrameEvent(true);
		}

		const D3DXQUATERNION& GameObject::GetRotSync() const
		{
			return _rotSync;
		}

		void GameObject::SetRotSync(const D3DXQUATERNION& value)
		{
			_rotSync = value;
			D3DXQuaternionToAxisAngle(&_rotSync, &_rotSyncAxis, &_rotSyncAngle);
			const float angle = abs(_rotSyncAngle);

			if (angle > D3DX_PI)
				_rotSyncAngle = (2 * D3DX_PI - angle) * (_rotSyncAngle > 0 ? -1.0f : 1.0f);

			SetSyncFrameEvent(true);
		}

		const D3DXVECTOR3& GameObject::GetPosSync2() const
		{
			return _posSync2;
		}

		void GameObject::SetPosSync2(const D3DXVECTOR3& curSync, const D3DXVECTOR3& newSync)
		{
			_posSyncDir2 = curSync - newSync;
			_posSyncDist2 = D3DXVec3Length(&_posSyncDir2);
			_posSyncDir2 = _posSyncDist2 != 0.0f ? _posSyncDir2 / _posSyncDist2 : NullVector;

			_posSync2 = newSync;
			_posSyncLength2 = D3DXVec3Length(&_posSync2);

			SetSyncFrameEvent(true);
		}

		const D3DXQUATERNION& GameObject::GetRotSync2() const
		{
			return _rotSync2;
		}

		void GameObject::SetRotSync2(const D3DXQUATERNION& curSync, const D3DXQUATERNION& newSync)
		{
			D3DXQUATERNION dRot;
			QuatRotation(dRot, newSync, curSync);
			D3DXQuaternionToAxisAngle(&dRot, &_rotSyncAxis2, &_rotSyncAngle2);

			const float angle = abs(_rotSyncAngle2);
			if (angle > D3DX_PI)
				_rotSyncAngle2 = (2 * D3DX_PI - angle) * (_rotSyncAngle2 > 0 ? -1.0f : 1.0f);

			_rotSync2 = newSync;
			D3DXVECTOR3 axis;
			D3DXQuaternionToAxisAngle(&_rotSync2, &axis, &_rotSyncLength2);

			SetSyncFrameEvent(true);
		}

		const D3DXVECTOR3& GameObject::GetPxPosLerp() const
		{
			return !_bodyProgressEvent && _parent ? _parent->GetPxPosLerp() : _pxPosLerp;
		}

		const D3DXQUATERNION& GameObject::GetPxRotLerp() const
		{
			return !_bodyProgressEvent && _parent ? _parent->GetPxRotLerp() : _pxRotLerp;
		}

		const D3DXVECTOR3& GameObject::GetPxVelocityLerp() const
		{
			return !_bodyProgressEvent && _parent ? _parent->GetPxVelocityLerp() : _pxVelocityLerp;
		}

		const D3DXVECTOR3& GameObject::GetPxPrevPos() const
		{
			return _pxPrevPos;
		}

		const D3DXQUATERNION& GameObject::GetPxPrevRot() const
		{
			return _pxPrevRot;
		}

		const D3DXVECTOR3& GameObject::GetPxPrevVelocity() const
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

		bool GameObject::GetReverse() const
		{
			return _reverse;
		}

		void GameObject::SetReverse(bool value)
		{
			_reverse = value;
		}

		bool GameObject::GetRage() const
		{
			return _rage;
		}

		void GameObject::SetRage(bool value)
		{
			_rage = value;
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

		Behaviors& GameObject::GetBehaviors() const
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

			return pxActor ? GetGameObjFromActor(pxActor) : nullptr;
		}

		GameObject* GameObject::GetGameObjFromShape(NxShape* shape)
		{
			return GetGameObjFromActor(&shape->getActor());
		}
	}
}
