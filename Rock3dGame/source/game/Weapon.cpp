#include "stdafx.h"
#include "game//Weapon.h"

#include "game//Logic.h"
#include "game//Race.h"

namespace r3d
{
	namespace game
	{
		extern unsigned int GAME_DIFF = 3;

		Proj::Proj(): _model(nullptr), _model2(nullptr), _model3(nullptr), _weapon(nullptr), _playerId(cUndefPlayerId),
		              _pxBox(nullptr), _ignoreContactProj(false), _sprite(nullptr), _tick1(0), _time1(0),
		              _state1(false), _vec1(NullVector)
		{
		}

		Proj::~Proj()
		{
			Destroy();

			FreeSprite();
			FreeModel2(false);
			FreeModel(false);
			SetWeapon(nullptr);

			SetShot(ShotDesc());
		}

		void Proj::RandomizeLocateProj(GameObject* weapon, bool pos, bool rot, const D3DXVECTOR3* speed)
		{
			if (pos)
			{
				auto pos = D3DXVECTOR3(_desc.pos.x, _desc.pos.y + RandomRange(-_desc.angleSpeed, _desc.angleSpeed),
				                       _desc.pos.z + RandomRange(-_desc.angleSpeed, _desc.angleSpeed));
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

		void Proj::VariabilityLocateProj(GameObject* weapon, bool pos, bool rot, const D3DXVECTOR3* speed)
		{
			//"scAirWeapon"

			if (pos)
			{
				D3DXVECTOR3 pos1 = _desc.pos;
				auto pos2 = D3DXVECTOR3(_desc.pos.x, -_desc.pos.y, _desc.pos.z);

				int stage = 0;
				if (weapon && weapon->GetParent() != nullptr)
					stage = weapon->GetParent()->GetMapObj()->GetPlayer()->GetSlotInst(Player::stWeapon1)->GetItem().
					                IsWeaponItem()->GetCurCharge();

				if (stage == 1 || stage == 3 || stage == 5 || stage == 7 || stage == 9 || stage == 11 || stage == 13 ||
					stage == 15)
				{
					if (weapon)
						weapon->GetGrActor().LocalToWorldCoord(pos1, pos1);
					this->SetPos(pos1);
				}
				else
				{
					if (weapon)
						weapon->GetGrActor().LocalToWorldCoord(pos2, pos2);
					this->SetPos(pos2);
				}
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

				SafeRelease(_model);
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

				SafeRelease(_model2);
			}
		}

		void Proj::InitModel3()
		{
			if (_model3)
				return;

			if (_desc.GetModel3())
			{
				_model3 = &GetIncludeList().Add(_desc.GetModel3());
				_model3->AddRef();
				_model3->GetGameObj().InsertListener(this);
			}
		}

		void Proj::FreeModel3(bool remove)
		{
			if (_model3)
			{
				_model3->GetGameObj().RemoveListener(this);
				if (remove)
					GetIncludeList().Delete(_model3);

				SafeRelease(_model3);
			}
		}

		px::Body* Proj::CreateBody(const NxBodyDesc& desc) const
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

		void Proj::FreeSprite() const
		{
			if (_sprite)
			{
				_sprite->Release();
				GetGrActor().GetNodes().Delete(_sprite);
			}
		}

		void Proj::InsertProjToGraph(GraphManager* graph) const
		{
			graph::Actor::GraphDesc desc;
			desc.lighting = graph::Actor::glStd;
			desc.order = graph::Actor::goEffect;
			desc.props.set(graph::Actor::gpColor);
			desc.props.set(graph::Actor::gpDynamic);
			//desc.props.set(graph::Actor::gpMorph);	

			GetGrActor().SetGraph(graph, desc);
		}

		AABB Proj::ComputeAABB(bool onlyModel) const
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
			const AABB aabb = ComputeAABB(false);

			NxBoxShapeDesc boxDesc;
			boxDesc.dimensions = NxVec3(aabb.GetSizes() / 2.0f);
			boxDesc.localPose.t.set(aabb.GetCenter());
			boxDesc.group = group;
			this->_pxBox = &this->GetPxActor().GetShapes().Add<px::BoxShape>();
			this->_pxBox->AssignFromDesc(boxDesc);
		}

		void Proj::AddContactForce(GameObject* target, const D3DXVECTOR3& point, const D3DXVECTOR3& force,
		                           NxForceMode mode)
		{
			target->GetPxActor().GetNxActor()->addForceAtPos(NxVec3(force), NxVec3(point), mode);
		}

		void Proj::AddContactForce(GameObject* target, const px::Scene::OnContactEvent& contact,
		                           const D3DXVECTOR3& force, NxForceMode mode) const
		{
			const D3DXVECTOR3 point = GetContactPoint(contact);
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
						SetParent(nullptr);
					_playerId = cUndefPlayerId;
				}

				_weapon = weapon;

				if (_weapon)
				{
					weapon->InsertListener(this);

					const GameObject* car = _weapon->GetParent() ? _weapon->GetParent() : nullptr;
					_playerId = car && car->GetMapObj() && car->GetMapObj()->GetPlayer()
						            ? car->GetMapObj()->GetPlayer()->GetId()
						            : cUndefPlayerId;
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

		bool Proj::GetIgnoreContactProj() const
		{
			return _ignoreContactProj;
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

		void Proj::DamageTarget(GameObject* target, float damage, DamageType damageType) const
		{
			LSL_ASSERT(target);

			GameObject* car = _weapon && _weapon->GetParent() ? _weapon->GetParent() : nullptr;

			const int playerId = _playerId;

			//self damage if plr undef
			//if (playerId == cUndefPlayerId && target && target->GetMapObj() && target->GetMapObj()->GetPlayer())
			GetLogic()->Damage(car, playerId, target, damage, damageType);
		}

		MapObj* Proj::FindNextTaget(float viewAngle) const
		{
			if (GetLogic() == nullptr || _shot.GetTargetMapObj() == nullptr)
				return nullptr;

			Player* player = GetLogic()->GetRace()->GetPlayerByMapObj(_shot.GetTargetMapObj());
			if (player == nullptr)
				return nullptr;

			Player* nextPlayer = player->FindClosestEnemy(viewAngle, false);
			if (nextPlayer == nullptr)
				return nullptr;

			if (_weapon && nextPlayer->GetCar().gameObj == _weapon->GetParent())
			{
				nextPlayer = nextPlayer->FindClosestEnemy(viewAngle, false);
				if (nextPlayer == nullptr || nextPlayer == player)
					return nullptr;
			}

			return nextPlayer->GetCar().mapObj;
		}

		void Proj::EnableFilter(GameObject* target, unsigned mask)
		{
			if (target->GetPxActor().GetNxActor() == nullptr)
				return;

			NxGroupsMask nxMask;
			nxMask.bits0 = mask;
			nxMask.bits1 = nxMask.bits2 = nxMask.bits3 = 0;
			for (unsigned i = 0; i < target->GetPxActor().GetNxActor()->getNbShapes(); ++i)
			{
				NxShape* shape = target->GetPxActor().GetNxActor()->getShapes()[i];
				shape->setGroupsMask(nxMask);
			}

			target->GetPxActor().GetScene()->GetNxScene()->
			        setFilterOps(NX_FILTEROP_OR, NX_FILTEROP_OR, NX_FILTEROP_AND);
		}

		void Proj::DisableFilter(GameObject* target)
		{
			if (target->GetPxActor().GetNxActor() == nullptr)
				return;

			NxGroupsMask nxMask;
			nxMask.bits0 = px::Scene::gmDef;
			nxMask.bits1 = nxMask.bits2 = nxMask.bits3 = 0;
			for (unsigned i = 0; i < target->GetPxActor().GetNxActor()->getNbShapes(); ++i)
			{
				NxShape* shape = target->GetPxActor().GetNxActor()->getShapes()[i];
				shape->setGroupsMask(nxMask);
			}

			target->GetPxActor().GetScene()->GetNxScene()->setFilterOps(NX_FILTEROP_AND, NX_FILTEROP_AND,
			                                                            NX_FILTEROP_AND);
		}

		D3DXVECTOR3 Proj::CalcSpeed(GameObject* weapon) const
		{
			D3DXVECTOR3 dir = weapon->GetGrActor().GetWorldDir();
			float speed = _desc.speed;

			if (_desc.speedRelative)
			{
				speed += std::max(D3DXVec3Dot(&dir, &weapon->GetPxVelocityLerp()), 0.0f);
			}
			else if (_desc.speedRelativeMin > 0)
			{
				speed = std::max(
					speed, _desc.speedRelativeMin + std::max(D3DXVec3Dot(&dir, &weapon->GetPxVelocityLerp()), 0.0f));
			}

			const float cosa = abs(D3DXVec3Dot(&dir, &D3DXVECTOR3(0, 0, 1)));
			if (cosa < 0.707f)
			{
				dir.z = 0;
				D3DXVec3Normalize(&dir, &dir);
			}

			return dir * speed;
		}

		D3DXVECTOR3 Proj::CalcFlySpeed(GameObject* weapon) const
		{
			D3DXVECTOR3 dir = weapon->GetGrActor().GetWorldDir();
			//по-умолчанию скорость полёта статическая.
			const float speed = _desc.speed;

			//скорость полёта == скорости машины до использования нитро.
			if (_desc.speedRelative)
			{
				weapon->GetParent()->GetPxActor().GetNxActor()->getLinearVelocity().magnitude();
			}

			const float cosa = abs(D3DXVec3Dot(&dir, &D3DXVECTOR3(0, 0, 1)));
			if (cosa < 0.707f)
			{
				dir.z = 0;
				D3DXVec3Normalize(&dir, &dir);
			}

			return dir * speed;
		}

		D3DXVECTOR3 Proj::BrakeFlySpeed(GameObject* weapon) const
		{
			D3DXVECTOR3 dir = weapon->GetGrActor().GetWorldDir();
			const float speed = _desc.speedRelativeMin;

			const float cosa = abs(D3DXVec3Dot(&dir, &D3DXVECTOR3(0, 0, 1)));
			if (cosa < 0.707f)
			{
				dir.z = 0;
				D3DXVec3Normalize(&dir, &dir);
			}

			return dir * speed;
		}

		D3DXVECTOR3 Proj::GetFixedBackSpeed(GameObject* weapon) const
		{
			D3DXVECTOR3 dir = -(weapon->GetGrActor().GetWorldDir());
			float speed = 18;

			speed = std::max(speed, 18 + std::max(D3DXVec3Dot(&dir, &weapon->GetPxVelocityLerp()), 0.0f));

			const float cosa = abs(D3DXVec3Dot(&dir, &D3DXVECTOR3(0, 0, 1)));
			if (cosa < 0.707f)
			{
				dir.z = 0;
				D3DXVec3Normalize(&dir, &dir);
			}

			return dir * speed;
		}

		D3DXVECTOR3 Proj::GetDoubleSpeed(GameObject* weapon) const
		{
			D3DXVECTOR3 dir = weapon->GetGrActor().GetWorldDir();
			float speed = _desc.speed * 2;

			if (_desc.speedRelative)
			{
				speed += std::max(D3DXVec3Dot(&dir, &weapon->GetPxVelocityLerp()), 0.0f);
			}
			else if (_desc.speedRelativeMin > 0)
			{
				speed = std::max(
					speed, _desc.speedRelativeMin + std::max(D3DXVec3Dot(&dir, &weapon->GetPxVelocityLerp()), 0.0f));
			}

			const float cosa = abs(D3DXVec3Dot(&dir, &D3DXVECTOR3(0, 0, 1)));
			if (cosa < 0.707f)
			{
				dir.z = 0;
				D3DXVec3Normalize(&dir, &dir);
			}

			return dir * speed;
		}

		bool Proj::FixRocketPrepare(GameObject* weapon, bool disableGravity, D3DXVECTOR3* speedVec,
		                            NxCollisionGroup pxGroup)
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

		void Proj::FixRocketContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			if (target == nullptr)
				return;

			MapObjRec* record = target->GetMapObj() ? target->GetMapObj()->GetRecord() : nullptr;
			const RecordNode* recordParent = record ? record->GetParent() : nullptr;

			if ((record && record->GetCategory() == MapObjLib::ctCar) || (recordParent && recordParent->GetName() ==
				"Crush"))
			{
				if (record->GetName() == "dinamo")
				{
					target->SetMaxTimeLife(0.1f);
				}

				this->Death(dtSimple, target);

				DamageTarget(target, _desc.damage, dtAim);

				auto dir = D3DXVECTOR3(this->GetPxActor().GetNxActor()->getLinearVelocity().get());
				const float dirLength = D3DXVec3Length(&dir);

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

		bool Proj::RocketPrepare(GameObject* weapon, bool disableGravity, D3DXVECTOR3* speedVec,
		                         NxCollisionGroup pxGroup)
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
			if (weapon && weapon->GetParent() != nullptr && weapon->GetParent()->IsCar()->InRage())
				this->SetRage(true);

			return true;
		}

		void Proj::RocketContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			const GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			                                                                                    GetNxActor()
				                     ? target->GetParent()->IsCar()
				                     : target->IsCar();
			if (target == nullptr)
				return;

			MapObjRec* record = target->GetMapObj() ? target->GetMapObj()->GetRecord() : nullptr;
			const RecordNode* recordParent = record ? record->GetParent() : nullptr;

			if ((record && record->GetCategory() == MapObjLib::ctCar) || (recordParent && recordParent->GetName() ==
				"Crush"))
			{
				ShotContext ctx;
				if (car)
				{
					if (car->IsShell() == true)
					{
						//позже сменить на рандомный вектор либо уничтожить при отсутствии wpn.
						if (_weapon != nullptr)
						{
							const D3DXVECTOR3 speed = CalcSpeed(this->GetWeapon());
							GetPxActor().GetScene()->GetNxScene()->setActorPairFlags(
								*_weapon->GetPxActor().GetNxActor(), *GetPxActor().GetNxActor(), NX_NOTIFY_ALL);
							this->GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(-speed / 2));
							//проблему с десинхронизацией рикошета можно будет потом решить с помощью закономерной имитации рандома.
							GetPxActor().GetNxActor()->addLocalForce(
								NxVec3(D3DXVECTOR3(0.0f, RandomRange(-10.0f, 10.0f), RandomRange(-1.5f, 1.5f))),
								NX_VELOCITY_CHANGE);
							this->SetReverse(true);
						}
						else
						{
							if (this->GetReverse() == false)
								this->Death();
						}
					}
					else
					{
						if (this->GetReverse() == false)
						{
							if (this->GetRage())
								//1.25 - коэффициент повышеного урона (во время ярости)
								DamageTarget(target, _desc.damage * 1.25f, dtSimple);
							else
								DamageTarget(target, _desc.damage, dtSimple);
							this->Death();
						}
						else
						{
							//после рикошета атакбонус не даётся.
							//после рикошета ярость не действует.
							//убийство от рикошета не делает бота враждебным.
							DamageTarget(target, _desc.damage, dtNone);
							this->Death();
						}
					}
				}
				else
				{
					if (record->GetName() == "dinamo")
					{
						target->SetMaxTimeLife(0.1f);
					}
					this->Death();
				}

				if (target->IsShell() == false && target->IsImmortal() == false)
				{
					auto dir = D3DXVECTOR3(this->GetPxActor().GetNxActor()->getLinearVelocity().get());
					const float dirLength = D3DXVec3Length(&dir);

					if (dirLength > 1.0f)
					{
						dir /= dirLength;

						D3DXVECTOR3 contactDir = GetContactPoint(contact);
						D3DXVec3Cross(&contactDir, &contactDir, &dir);

						NxVec3 vec3(contactDir);
						if (vec3.magnitude() > 0.01f)
						{
							vec3.normalize();
							if (target->IsCar())
							{
								if (target->IsCar() != nullptr)
								{
									//сила подброса зависит от навыка "corner"
									if (target->IsCar() && target->IsCar()->IsStabilityShot())
										target->GetPxActor().GetNxActor()->addLocalTorque(
											vec3 * _desc.mass * 0.1f, NX_VELOCITY_CHANGE);
									else
										target->GetPxActor().GetNxActor()->addLocalTorque(
											vec3 * _desc.mass * 0.2f, NX_VELOCITY_CHANGE);
								}
							}
							else
								target->GetPxActor().GetNxActor()->addLocalTorque(
									vec3 * _desc.mass * 0.2f, NX_VELOCITY_CHANGE);
						}

						D3DXVec3Normalize(&dir, &(dir - ZVector));
					}
				}
			}
		}

		void Proj::RocketUpdate(float deltaTime)
		{
			constexpr float cTrackHeight = 4.0f;

			const D3DXVECTOR3 size = _pxBox->GetDimensions();
			NxVec3 pos = GetPxActor().GetNxActor()->getGlobalPosition();
			const NxRay nxRay(pos + NxVec3(0, 0, cTrackHeight), NxVec3(0, 0, -1.0f));

			NxRaycastHit hit;
			NxShape* hitShape = GetLogic()->GetPxScene()->GetNxScene()->raycastClosestShape(
				nxRay, NX_STATIC_SHAPES, hit, 1 << px::Scene::cdgTrackPlane, NX_MAX_F32,
				NX_RAYCAST_SHAPE | NX_RAYCAST_IMPACT);

			if (hitShape)
			{
				GetGameObjFromShape(hitShape);
				hitShape->getGroup();

				const float height = std::max(pos.z - hit.worldImpact.z, size.z);
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
			Player* plr = weapon->GetParent()->GetMapObj()->GetPlayer();
			GameCar* car = weapon->GetParent()->IsCar();
			if (plr->GetHyperDelay() == true)
				return false;

			LocateProj(weapon, true, true, nullptr);

			InitModel();
			LinkToWeapon();

			//повышенное ускорение с места
			if (plr->GetNitroBonus() && car->GetSpeed() < 5.0f)
				weapon->GetPxActor().GetNxActor()->addLocalForce(NxVec3(2.0f, 0.0f, 0.0f) * _desc.speed,
				                                                 NX_SMOOTH_VELOCITY_CHANGE);
			else
				weapon->GetPxActor().GetNxActor()->addLocalForce(NxVec3(1.0f, 0.0f, 0.0f) * _desc.speed,
				                                                 NX_SMOOTH_VELOCITY_CHANGE);

			this->SetMaxTimeLife(_desc.damage + 0.1f);
			plr->SetHyperDelay(true);
			return true;
		}

		void Proj::HyperUpdate(float deltaTime)
		{
			if (GetLiveState() != lsDeath)
			{
				if (_weapon && _weapon->GetParent() != nullptr)
				{
					//задержка между использованием нитро
					if (GetTimeLife() >= _desc.damage)
					{
						_weapon->GetParent()->GetMapObj()->GetPlayer()->SetHyperDelay(false);
						this->Death();
					}
				}
				else
					this->Death();
			}
		}

		bool Proj::SpringPrepare(GameObject* weapon)
		{
			LinkToWeapon();
			GameCar* car = weapon->GetParent()->IsCar();

			if (car == nullptr || car->OnJump() == true)
				return false;

			//дополнительная задержка чтобы избежать использования прыжка в воздухе.
			const Player* plr = car->GetMapObj()->GetPlayer();
			if (plr->GetCarLiveTime() <= 0.5f)
				return false;

			if (car->IsWheelsContact() == false)
				return false;

			car->OnJump(true);
			const float force = _desc.speed * plr->GetJumpPower();
			car->GetPxActor().GetNxActor()->addLocalForce(NxVec3(0.0f, 0.0f, 1.0f) * force, NX_SMOOTH_VELOCITY_CHANGE);

			DOUBLE_JUMP = false;
			return true;
		}

		bool Proj::Spring2Prepare(GameObject* weapon)
		{
			LinkToWeapon();

			GameCar* car = weapon->GetParent()->IsCar();
			//ротация активироана.
			static D3DXQUATERNION testRot;
			_time1 = 0.0f;
			_state1 = false;
			_vec1.x = 0.0f;
			testRot = car->GetRot();
			SetMaxTimeLife(4.0f);

			const NxVec3 subvelocity = GetPxActor().GetNxActor()->getLinearVelocity() / 2;
			const float velocity = car->GetPxActor().GetNxActor()->getLinearVelocity().magnitude();
			if (car != nullptr)
			{
				const Player* plr = car->GetMapObj()->GetPlayer();
				if (plr->GetCarLiveTime() <= 0.5f)
					return false;

				if (car->OnJump() == true || car->IsWheelsContact() == false)
					return false;

				car->OnJump(true);
				//движение вперед:
				if (car->GetSpeed() > 0 && velocity > 8)
				{
					car->GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(subvelocity));
					car->GetPxActor().GetNxActor()->addLocalForce(NxVec3(1.0f, 0.0f, 0.0f) * _desc.speed,
					                                              NX_SMOOTH_VELOCITY_CHANGE);
					car->GetPxActor().GetNxActor()->addForce(NxVec3(0.0f, 0.0f, 1.1f) * _desc.speed,
					                                         NX_SMOOTH_VELOCITY_CHANGE);
				}
				//движение назад:
				else if (car->GetSpeed() <= 0 && velocity > 8)
				{
					car->GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(subvelocity));
					car->GetPxActor().GetNxActor()->addLocalForce(NxVec3(-1.0f, 0.0f, 0.0f) * _desc.speed,
					                                              NX_SMOOTH_VELOCITY_CHANGE);
					car->GetPxActor().GetNxActor()->addForce(NxVec3(0.0f, 0.0f, 1.0f) * _desc.speed,
					                                         NX_SMOOTH_VELOCITY_CHANGE);
				}
				//на месте
				else
				{
					car->GetPxActor().GetNxActor()->addForce(NxVec3(0.0f, 0.0f, 1.2f) * _desc.speed,
					                                         NX_SMOOTH_VELOCITY_CHANGE);
					car->GetPxActor().GetNxActor()->addLocalForce(NxVec3(0.1f, 0.0f, 0.0f) * _desc.speed,
					                                              NX_SMOOTH_VELOCITY_CHANGE);
				}
				return true;
			}
			return false;

			//spring3 prepare (альтернатива)
			/*
			LinkToWeapon();
		
			GameCar* car = weapon->GetParent()->IsCar();
			const NxVec3 subvelocity = GetPxActor().GetNxActor()->getLinearVelocity() / 2;
			float velocity = car->GetPxActor().GetNxActor()->getLinearVelocity().magnitude();
			if (car && car->IsWheelsContact())
			{
				//прижок без ротации
				if (_desc.speedRelative == false)
				{
					car->GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(subvelocity));
					car->GetPxActor().GetNxActor()->addLocalForce(NxVec3(1.0f, 0.0f, 0.0f) * _desc.speed, NX_SMOOTH_VELOCITY_CHANGE);
					car->GetPxActor().GetNxActor()->addForce(NxVec3(0.0f, 0.0f, 1.0f) * _desc.speed, NX_SMOOTH_VELOCITY_CHANGE);
				}
				return true;
			}
			return false;*/
		}

		void Proj::Spring2Update(float deltaTime)
		{
			if (_weapon != nullptr)
			{
				GameCar* car = _weapon->GetParent()->IsCar();

				if (car && !car->IsAnyWheelContact())
				{
					float dAngle;

					if (!_state1)
					{
						_time1 += deltaTime;
						const float alpha = ClampValue(_time1 / 0.1f, 0.0f, 1.0f);
						dAngle = -D3DX_PI * deltaTime * 1.0f;
						_vec1.x = dAngle;

						if (alpha == 1.0f)
							_state1 = true;
					}
					else
					{
						dAngle = D3DX_PI * deltaTime * 0.75f;
					}

					_vec1.x = _vec1.x + (dAngle - _vec1.x) * ClampValue(deltaTime * 6.0f, 0.0f, 1.0f);

					D3DXQUATERNION rot;
					D3DXQuaternionRotationAxis(&rot, &YVector, _vec1.x);
					car->SetRot(rot * car->GetRot());
				}
				else if (_state1 || _time1 > 0.0f)
				{
					Death();
				}
			}
			else
			{
				Death();
			}
		}

		bool Proj::RipperPrepare(GameObject* weapon)
		{
			if (weapon->GetParent()->IsCar()->IsWheelsContact() == false || weapon->GetParent()->IsCar()->OnJump())
				return false;

			_time1 = 0.0f;

			LocateProj(weapon, true, true, nullptr);
			CreatePxBox();

			const NxBodyDesc desc;
			CreateBody(desc);

			this->GetPxActor().SetContactReportFlags(NX_NOTIFY_ALL);
			this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);
			SetIgnoreContactProj(true);

			return true;
		}

		void Proj::RipperContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);

			if (target)
			{
				const GameCar* car;
				if (this->GetWeapon() != nullptr)
					car = _weapon->GetParent()->IsCar();
				else car = nullptr;

				_time1 = 0.5f;
				InitModel();
				if (_model)
				{
					const D3DXVECTOR3 pnt = GetContactPoint(contact);
					_model->GetGameObj().SetWorldPos(pnt);
				}

				if (this->GetWeapon() != nullptr && car->IsAnyWheelContact() == false)
					DamageTarget(target, _desc.damage * contact.deltaTime, dtCash);
			}
		}

		void Proj::RipperUpdate(float deltaTime)
		{
			if (_weapon)
			{
				D3DXVECTOR3 pos;
				_weapon->GetGrActor().LocalToWorldCoord(_desc.pos, pos);
				SetWorldPos(pos);
				SetWorldRot(_weapon->GetWorldRot());

				D3DXQUATERNION rot;
				D3DXQuaternionRotationAxis(&rot, &ZVector, _desc.angleSpeed * deltaTime);
				_weapon->SetRot(_weapon->GetRot() * rot);
			}

			if (_model && _model->GetGameObj().GetLiveState() != lsDeath && (_time1 -= deltaTime) <= 0)
			{
				_model->GetGameObj().Death();
				FreeModel(false);
			}
		}

		bool Proj::ShellPrepare(GameObject* weapon)
		{
			Player* plr = weapon->GetParent()->GetMapObj()->GetPlayer();
			float timeshell;
			const int curcharge = plr->GetSlotInst(Player::stHyper)->GetItem().IsWeaponItem()->GetCurCharge();

			if (weapon->GetParent()->IsCar()->IsShell() == true)
				return false;

			//Длительность действия щита зависит от количества зарядов. Чем чаще используешь щит, тем короче его действие.
			if (curcharge > 2)
				timeshell = 3.0f;
			else if (curcharge == 2)
				timeshell = 2.0f;
			else if (curcharge == 1)
				timeshell = 1.0f;
			else
				timeshell = 0.0f;
			//3, 2 и 1 секунда. В сумме не более 6 секунд на круг.

			LocateProj(weapon, true, true, nullptr);
			InitModel();
			LinkToWeapon();

			//Чтобы время эффекта соответсвовало времени действия:
			SetMaxTimeLife(timeshell);
			//Устанавливаем силовое поле на заданное время:
			weapon->GetParent()->IsCar()->Shell(timeshell);

			return true;
		}

		void Proj::BlasterContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			const GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			                                                                                    GetNxActor()
				                     ? target->GetParent()->IsCar()
				                     : target->IsCar();

			if (target == nullptr)
				return;

			MapObjRec* record = target->GetMapObj() ? target->GetMapObj()->GetRecord() : nullptr;
			const RecordNode* recordParent = record ? record->GetParent() : nullptr;
			D3DXVECTOR3 speed;

			if (this->GetWeapon() != nullptr && _weapon->GetParent()->GetPxActor().GetNxActor() != nullptr)
				speed = CalcSpeed(this->GetWeapon());
			else
				speed = NullVector;

			if ((record && record->GetCategory() == MapObjLib::ctCar) || (recordParent && recordParent->GetName() ==
				"Crush") || (recordParent && recordParent->GetName() == "CarDestr"))
			{
				ShotContext ctx;
				if (car)
				{
					RocketContact(contact);
					//дополнительный подброс
					if (car != nullptr)
					{
						if (this->GetPxActor().GetNxActor() != nullptr && car->GetPxActor().GetNxActor() != nullptr &&
							car->GetLife() > _desc.damage)
						{
							if (car->IsStabilityShot() == false)
								target->GetPxActor().GetNxActor()->addForce(
									NxVec3(ZVector * 2), NX_SMOOTH_VELOCITY_CHANGE);
						}
					}
				}
				else
				{
					if (record->GetName() != "dinamo")
					{
						if (this->GetPxActor().GetNxActor() != nullptr && target->GetPxActor().GetNxActor() != nullptr)
							target->GetPxActor().GetNxActor()->addForce(NxVec3(speed / 2), NX_SMOOTH_VELOCITY_CHANGE);
						this->Death(dtSimple, target);
					}
					else
					{
						target->SetMaxTimeLife(0.1f);
					}
				}
			}
		}

		bool Proj::AirWeaponPrepare(GameObject* weapon, bool disableGravity, D3DXVECTOR3* speedVec,
		                            NxCollisionGroup pxGroup)
		{
			_vec1.z = 0.0f;

			D3DXVECTOR3 speed = CalcSpeed(weapon);
			if (speedVec)
				*speedVec = speed;

			VariabilityLocateProj(weapon, true, true, &speed);
			InitModel();
			CreatePxBox(pxGroup);

			NxBodyDesc bodyDesc;
			bodyDesc.flags |= disableGravity ? NX_BF_DISABLE_GRAVITY : 0;
			bodyDesc.linearVelocity = NxVec3(speed);

			CreateBody(bodyDesc);

			this->GetPxActor().SetContactReportFlags(NX_NOTIFY_ALL);
			this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);
			SetIgnoreContactProj(true);

			if (weapon && weapon->GetParent() != nullptr && weapon->GetParent()->IsCar()->InRage())
				this->SetRage(true);


			return true;
		}

		void Proj::AirWeaponUpdate(float deltaTime)
		{
			RocketUpdate(deltaTime);
			if (GetLiveState() != lsDeath)
			{
				if (GetTimeLife() > _desc.angleSpeed && _desc.GetModel2())
				{
					InitModel2();
				}
			}
		}

		bool Proj::RezonatorPrepare(GameObject* weapon)
		{
			return RocketPrepare(weapon, true, nullptr);
		}

		void Proj::RezonatorContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			if (target && target->IsCar())
			{
				RocketContact(contact);
			}
		}

		void Proj::RezonatorUpdate(float deltaTime)
		{
			RocketUpdate(deltaTime);

			D3DXQUATERNION dRot;
			D3DXQuaternionRotationAxis(&dRot, &XVector, _desc.angleSpeed * deltaTime);

			SetRot(dRot * GetRot());
		}

		bool Proj::SonarPrepare(GameObject* weapon)
		{
			_time1 = 0.0f;

			return RocketPrepare(weapon, true, nullptr, px::Scene::cdgShotBorder);
		}

		void Proj::SonarContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			if (target == nullptr)
				return;

			GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			               GetNxActor()
				               ? target->GetParent()->IsCar()
				               : target->IsCar();

			if (car)
			{
				if (this->GetRage())
					DamageTarget(car, _desc.damage * 1.2f, dtSimple);
				else
					DamageTarget(car, _desc.damage, dtSimple);
				this->Death();
			}
			else
			{
				AddContactForce(target, contact,
				                _desc.mass * D3DXVECTOR3(this->GetPxActor().GetNxActor()->getLinearVelocity().get()),
				                NX_IMPULSE);
			}

			if (GetLiveState() == lsDeath)
				return;

			if (_time1 > 0.0f)
				return;
			_time1 = 0.0f;

			NxVec3 velocity = GetPxActor().GetNxActor()->getLinearVelocity();
			NxContactStreamIterator contIter(contact.stream);

			if (ContainsContactGroup(contIter, contact.actorIndex, px::Scene::cdgShotTransparency) && velocity.
				magnitude() > 5.0f)
			{
				_time1 = 0.1f;

				NxVec3 norm = contIter.getPatchNormal();
				if (contact.actorIndex == 0)
					norm = -norm;

				NxVec3 velNorm = velocity;
				velNorm.normalize();
				const float angle = velNorm.dot(norm);
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

		void Proj::SonarUpdate(float deltaTime)
		{
			RocketUpdate(deltaTime);

			_time1 -= deltaTime;
		}

		void Proj::MiniGunUpdate(float deltaTime) const
		{
			if (GetLiveState() != lsDeath && _weapon && _weapon->GetParent() != nullptr)
			{
				D3DXQUATERNION rot;
				D3DXQuaternionRotationAxis(&rot, &XVector, 0.3f * deltaTime);
				_weapon->SetRot(_weapon->GetRot() * rot);

				if (GetTimeLife() > 0.1f)
					GetPxActor().GetNxActor()->addLocalForce(
						NxVec3(D3DXVECTOR3(0.0f, RandomRange(-0.1f, 0.1f), RandomRange(0.0f, 0.1f))),
						NX_VELOCITY_CHANGE);
			}
		}

		bool Proj::ShotGunPrepare(GameObject* weapon, D3DXVECTOR3* speedVec, NxCollisionGroup pxGroup)
		{
			_vec1.z = 0.0f;

			D3DXVECTOR3 speed = CalcSpeed(weapon);
			if (speedVec)
				*speedVec = speed;
			//случайная стартовая позиция (YZ):
			RandomizeLocateProj(weapon, true, true, &speed);
			InitModel();
			CreatePxBox(pxGroup);

			NxBodyDesc bodyDesc;
			bodyDesc.flags = NX_BF_DISABLE_GRAVITY;
			bodyDesc.linearVelocity = NxVec3(speed);

			CreateBody(bodyDesc);

			this->GetPxActor().SetContactReportFlags(NX_NOTIFY_ALL);
			this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);
			SetIgnoreContactProj(true);
			if (weapon && weapon->GetParent() != nullptr && weapon->GetParent()->IsCar()->InRage())
				this->SetRage(true);

			return true;
		}

		void Proj::ShotGunUpdate(float deltaTime)
		{
			RocketUpdate(deltaTime);
			// расброс картечи (XYZ):
			if (GetLiveState() != lsDeath)
			{
				if (GetTimeLife() >= 0.5f && GetTimeLife() < 0.6f)
				{
					GetPxActor().GetNxActor()->addLocalForce(
						NxVec3(D3DXVECTOR3(RandomRange(0.0f, _desc.angleSpeed * 3),
						                   RandomRange(-_desc.angleSpeed * 2, _desc.angleSpeed * 2),
						                   RandomRange(-_desc.angleSpeed * 2, _desc.angleSpeed * 2))),
						NX_SMOOTH_VELOCITY_CHANGE);
				}
			}
		}

		bool Proj::TrinityPrepare(GameObject* weapon)
		{
			this->GetGrActor().SetVisible(false);
			return RocketPrepare(weapon);
		}

		void Proj::TrinityUpdate(float deltaTime)
		{
			RocketUpdate(deltaTime);
			if (GetLiveState() != lsDeath)
			{
				if (GetTimeLife() >= 0.3f && GetTimeLife() < 0.4f)
				{
					//растроение ракет:
					GetPxActor().GetNxActor()->addLocalForce(NxVec3(YVector) * _desc.angleSpeed,
					                                         NX_SMOOTH_VELOCITY_CHANGE);
					this->GetGrActor().SetVisible(true);
				}
				else if (GetTimeLife() >= 0.8f && GetTimeLife() < 1.0f)
				{
					if (_weapon && _weapon->GetPxActor().GetNxActor() && GetPxActor().GetNxActor())
					{
						CalcSpeed(_model->GetParent());
						if (this->GetMaxLife() != 33.0f)
						{
							GetPxActor().GetNxActor()->setLinearVelocity(
								NxVec3((this->GetPxActor().GetNxActor()->getLinearVelocity()) * 1.4f));
							this->SetMaxLife(33.0f);
						}
					}
				}
				else if (GetTimeLife() > 1.5f)
				{
					GetPxActor().GetNxActor()->addLocalForce(NxVec3(-YVector) * _desc.angleSpeed / 5,
					                                         NX_SMOOTH_VELOCITY_CHANGE);
				}
			}
		}


		bool Proj::LaserPrepare(GameObject* weapon)
		{
			LocateProj(weapon, true, true, nullptr);
			InitModel();
			InitModel2();
			LinkToWeapon();

			SetIgnoreContactProj(true);

			return true;
		}

		GameObject* Proj::LaserUpdate(float deltaTime, bool distort)
		{
			if (_weapon == nullptr)
				return nullptr;

			//Режим воздушного лазера (speed.Relative):
			if (_desc.speedRelative == true)
			{
				if (this->GetMaxLife() != 33)
				{
					D3DXQUATERNION rot;
					D3DXQuaternionRotationAxis(&rot, &YVector, _desc.angleSpeed);
					this->SetRot(this->GetRot() * rot);
					this->SetMaxLife(33);
				}
			}

			D3DXVECTOR3 shotDir = GetGrActor().GetWorldDir();
			const D3DXVECTOR3 shotPos = GetWorldPos();
			float scaleLaser = _desc.maxDist;

			EnableFilter(_weapon, px::Scene::gmTemp);

			NxGroupsMask nxMask;
			nxMask.bits0 = px::Scene::gmTemp;
			nxMask.bits1 = 0;
			nxMask.bits2 = 0;
			nxMask.bits3 = 0;

			NxRaycastHit rayhit;
			NxShape* hitShape = GetLogic()->GetPxScene()->GetNxScene()->raycastClosestShape(
				NxRay(NxVec3(shotPos + _desc.sizeAddPx), NxVec3(shotDir)), NX_ALL_SHAPES, rayhit,
				(1 << px::Scene::cdgDefault) | (1 << px::Scene::cdgShotTransparency) | (1 << px::Scene::cdgTrackPlane),
				_desc.maxDist, NX_RAYCAST_SHAPE | NX_RAYCAST_DISTANCE, &nxMask);
			GameObject* rayHitActor = hitShape ? GetGameObjFromShape(hitShape) : nullptr;

			DisableFilter(_weapon);

			if (rayHitActor)
			{
				scaleLaser = std::min(rayhit.distance, _desc.maxDist);
				const D3DXVECTOR3 hitPos = shotPos + shotDir * scaleLaser;
				if (_model2)
					_model2->GetGameObj().SetWorldPos(hitPos);

				if (scaleLaser < _desc.maxDist)
				{
					DamageTarget(rayHitActor, deltaTime * _desc.damage, dtLaser);
				}
			}
			else if (_model2)
				_model2->GetGameObj().SetPos(XVector * scaleLaser);

			if (_model)
			{
				const auto sprite = dynamic_cast<graph::Sprite*>(&_model->GetGameObj().GetGrActor().GetNodes().front());
				auto size = D3DXVECTOR2(scaleLaser, _desc.size.y);

				if (distort)
				{
					float alpha = ClampValue(GetTimeLife() / GetMaxTimeLife(), 0.0f, 1.0f);
					//alpha = lsl::ClampValue((alpha - 0.0f)/0.5f + 0.5f, 0.0f, 1.5f) - lsl::ClampValue((alpha - 0.7f)/0.3f * 1.5f, 0.0f, 1.5f);
					alpha = ClampValue((alpha - 0.0f) / 0.5f * 1.5f + 0.5f, 0.0f, 2.0f) - ClampValue(
						(alpha - 0.6f) / 0.4f * 2.0f, 0.0f, 2.0f);

					size.y = size.y * alpha;
					sprite->material.Get()->samplers[0].SetScale(D3DXVECTOR3(scaleLaser / 10.0f, 1.0f, 1.0f));
				}

				sprite->SetPos(XVector * scaleLaser / 2.0f);
				sprite->sizes = size;
			}

			return rayHitActor;
		}

		GameObject* Proj::FrostUpdate(float deltaTime, bool distort) const
		{
			if (_weapon == nullptr)
				return nullptr;

			const D3DXVECTOR3 shotPos = GetWorldPos();
			D3DXVECTOR3 shotDir = GetGrActor().GetWorldDir();
			float scaleLaser = _desc.maxDist;

			EnableFilter(_weapon, px::Scene::gmTemp);

			NxGroupsMask nxMask;
			nxMask.bits0 = px::Scene::gmTemp;
			nxMask.bits1 = 0;
			nxMask.bits2 = 0;
			nxMask.bits3 = 0;

			NxRaycastHit rayhit;
			NxShape* hitShape = GetLogic()->GetPxScene()->GetNxScene()->raycastClosestShape(
				NxRay(NxVec3(shotPos + _desc.sizeAddPx), NxVec3(shotDir)), NX_ALL_SHAPES, rayhit,
				(1 << px::Scene::cdgDefault) | (1 << px::Scene::cdgShotTransparency) | (1 << px::Scene::cdgTrackPlane),
				_desc.maxDist, NX_RAYCAST_SHAPE | NX_RAYCAST_DISTANCE, &nxMask);
			GameObject* rayHitActor = hitShape ? GetGameObjFromShape(hitShape) : nullptr;

			DisableFilter(_weapon);

			if (rayHitActor)
			{
				scaleLaser = std::min(rayhit.distance, _desc.maxDist);
				const D3DXVECTOR3 hitPos = shotPos + shotDir * scaleLaser;
				if (_model2)
					_model2->GetGameObj().SetWorldPos(hitPos);

				if (scaleLaser < _desc.maxDist)
				{
					DamageTarget(rayHitActor, deltaTime * _desc.damage, dtEnergy);
				}
			}
			else if (_model2)
				_model2->GetGameObj().SetPos(XVector * scaleLaser);

			if (_model)
			{
				const auto sprite = dynamic_cast<graph::Sprite*>(&_model->GetGameObj().GetGrActor().GetNodes().front());
				auto size = D3DXVECTOR2(scaleLaser, _desc.size.y);

				if (distort)
				{
					float alpha = ClampValue(GetTimeLife() / GetMaxTimeLife(), 0.0f, 1.0f);
					//alpha = lsl::ClampValue((alpha - 0.0f)/0.5f + 0.5f, 0.0f, 1.5f) - lsl::ClampValue((alpha - 0.7f)/0.3f * 1.5f, 0.0f, 1.5f);
					alpha = ClampValue((alpha - 0.0f) / 0.5f * 1.5f + 0.5f, 0.0f, 2.0f) - ClampValue(
						(alpha - 0.6f) / 0.4f * 2.0f, 0.0f, 2.0f);

					size.y = size.y * alpha;
					sprite->material.Get()->samplers[0].SetScale(D3DXVECTOR3(scaleLaser / 10.0f, 1.0f, 1.0f));
				}

				sprite->SetPos(XVector * scaleLaser / 2.0f);
				sprite->sizes = size;
			}

			return rayHitActor;
		}

		bool Proj::FrostRayPrepare(GameObject* weapon)
		{
			return LaserPrepare(weapon);
		}

		void Proj::FrostRayUpdate(float deltaTime) const
		{
			GameObject* target = FrostUpdate(deltaTime, false);
			SlowEffect* slow = target ? target->GetBehaviors().Find<SlowEffect>() : nullptr;

			if (target && target->IsCar() && slow == nullptr)
			{
				slow = &target->GetBehaviors().Add<SlowEffect>();
				slow->SetEffect(_desc.GetModel3());
			}
		}

		void Proj::NewFrostRayUpdate(float deltaTime) const
		{
			GameObject* target = FrostUpdate(deltaTime, false);
			SuperSlowEffect* superslow = target ? target->GetBehaviors().Find<SuperSlowEffect>() : nullptr;

			if (target && target->IsCar() && superslow == nullptr)
			{
				superslow = &target->GetBehaviors().Add<SuperSlowEffect>();
				superslow->SetEffect(_desc.GetModel3());
			}
		}

		bool Proj::MortiraPrepare(GameObject* weapon, D3DXVECTOR3* speedVec, NxCollisionGroup pxGroup)
		{
			_vec1.z = 0.0f;

			D3DXVECTOR3 speed = CalcSpeed(weapon);
			if (speedVec)
				*speedVec = speed;

			LocateProj(weapon, true, true, &speed);
			InitModel();
			CreatePxBox(pxGroup);

			NxBodyDesc bodyDesc;
			bodyDesc.flags = 0;
			bodyDesc.linearVelocity = NxVec3(speed);

			CreateBody(bodyDesc);

			this->GetPxActor().SetContactReportFlags(NX_NOTIFY_ALL);
			this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);
			SetIgnoreContactProj(true);
			if (weapon && weapon->GetParent() != nullptr && weapon->GetParent()->IsCar()->InRage())
				this->SetRage(true);

			return true;
		}

		void Proj::MortiraContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			if (target == nullptr)
				return;

			MapObjRec* record = target->GetMapObj() ? target->GetMapObj()->GetRecord() : nullptr;
			const RecordNode* recordParent = record ? record->GetParent() : nullptr;
			const GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			                                                                                    GetNxActor()
				                     ? target->GetParent()->IsCar()
				                     : target->IsCar();

			D3DXVECTOR3 speed;

			if (this->GetWeapon() != nullptr && _weapon->GetParent()->GetPxActor().GetNxActor() != nullptr)
				speed = CalcSpeed(this->GetWeapon());
			else
				speed = NullVector;

			if (record)
			{
				if (recordParent && recordParent->GetName() == "CarDestr")
				{
					//отталкивание обломков машин
					if (target->GetPxActor().GetNxActor() != nullptr)
						target->GetPxActor().GetNxActor()->addForce(NxVec3(speed * 30), NX_ACCELERATION);
				}

				if (record->GetCategory() == MapObjLib::ctCar)
				{
					if (car != nullptr)
					{
						//отталкивание вражеской машины
						if (car->GetPxActor().GetNxActor() != nullptr && car->GetLife() > _desc.damage)
						{
							if (car->IsStabilityShot())
								car->GetPxActor().GetNxActor()->addForce(NxVec3(speed * 60), NX_ACCELERATION);
							else
								car->GetPxActor().GetNxActor()->addForce(NxVec3(speed * 30), NX_ACCELERATION);
						}

						if (this->GetRage())
							DamageTarget(target, _desc.damage * 1.2f, dtCash);
						else
							DamageTarget(target, _desc.damage, dtCash);
						this->Death();
					}
				}

				if (recordParent && recordParent->GetName() == "Crush")
				{
					//уничтожение преград
					target->Death();
					this->Death();
				}
			}
		}

		bool Proj::MolotovPrepare(GameObject* weapon, D3DXVECTOR3* speedVec, NxCollisionGroup pxGroup)
		{
			const GameCar* car = weapon->GetParent()->IsCar();
			if (car && car->IsAnyWheelContact())
				return false;

			_vec1.z = 0.0f;

			D3DXVECTOR3 speed = CalcSpeed(weapon);
			if (speedVec)
				*speedVec = speed;

			LocateProj(weapon, true, true, &speed);
			InitModel();
			CreatePxBox(pxGroup);

			NxBodyDesc bodyDesc;
			bodyDesc.flags = 0;
			bodyDesc.linearVelocity = NxVec3(speed);

			CreateBody(bodyDesc);

			this->GetPxActor().SetContactReportFlags(NX_NOTIFY_ALL);
			this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);

			SetIgnoreContactProj(true);

			if (weapon && weapon->GetParent() != nullptr && weapon->GetParent()->IsCar()->InRage())
				this->SetRage(true);

			return true;
		}

		void Proj::MolotovContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			const GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			                                                                                    GetNxActor()
				                     ? target->GetParent()->IsCar()
				                     : target->IsCar();
			ShotContext ctx;
			if (target == nullptr || target->GetPxActor().GetNxActor() == nullptr || target->GetMapObj() == nullptr)
				return;

			MapObjRec* record = target->GetMapObj() ? target->GetMapObj()->GetRecord() : nullptr;
			record ? record->GetParent() : nullptr;

			if ((record && record->GetCategory() == MapObjLib::ctCar) || (record && record->GetCategory() ==
				MapObjLib::ctTrack))
			{
				if (car)
				{
					if (this->GetRage())
						DamageTarget(target, _desc.damage * 1.25f, dtCash);
					else
						DamageTarget(target, _desc.damage, dtCash);
					this->Death();
				}
				else
				{
					//подготовка к спавну кратера:
					CRATER_POSX = this->GetPos().x;
					CRATER_POSY = this->GetPos().y;
					CRATER_POSZ = this->GetPos().z;
					CRATER_SPAWN = true;
					this->Death();
				}
			}
		}

		bool Proj::CraterPrepare(const ShotContext& ctx)
		{
			if (CRATER_SPAWN == true)
			{
				SetIgnoreContactProj(true);
				CRATER_SPAWN = false;
				_weapon->SetWorldPos(D3DXVECTOR3(CRATER_POSX, CRATER_POSY, CRATER_POSZ * 2));
				_time1 = -1.0f;
				InitModel();
				CreatePxBox(px::Scene::cdgShotTrack);

				const AABB aabb = ComputeAABB(true);
				GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);
				_time1 = 0.0f;

				if (ctx.projMat)
				{
					SetWorldPos(ctx.projMat->t.get());

					return true;
				}
				D3DXVECTOR3 rayPos = _desc.pos;
				if (_weapon)
					_weapon->GetGrActor().LocalToWorldCoord(rayPos, rayPos);
				const NxRay nxRay(NxVec3(rayPos) + NxVec3(0, 0, 2.0f), NxVec3(-ZVector));

				NxRaycastHit hit;
				const NxShape* hitShape = GetLogic()->GetPxScene()->GetNxScene()->raycastClosestShape(
					nxRay, NX_STATIC_SHAPES, hit, (1 << px::Scene::cdgTrackPlane) | (1 << px::Scene::cdgShotTrack),
					NX_MAX_F32, NX_RAYCAST_SHAPE | NX_RAYCAST_IMPACT | NX_RAYCAST_NORMAL);

				if (hitShape && hitShape->getGroup() != px::Scene::cdgShotTrack) //&& hit.distance < _desc.projMaxDist)
				{
					const float offs = std::max(-aabb.min.z, 0.01f);
					const D3DXVECTOR3 normal = hit.worldNormal.get();

					SetWorldPos(D3DXVECTOR3(hit.worldImpact.get()) + ZVector * offs);
					SetWorldUp(normal);
					return true;
				}
				return false;
			}
			return false;
		}

		void Proj::CraterContact(const px::Scene::OnContactEvent& contact) const
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			if (target == nullptr)
				return;

			if (GetTimeLife() > 1.0f)
			{
				GetPxActor().GetScene()->GetNxScene()->setActorPairFlags(
					*_weapon->GetPxActor().GetNxActor(), *GetPxActor().GetNxActor(), NX_NOTIFY_ALL);
			}
			/* для кратера почему-то не работает!!!
			MapObjRec* record = target->GetMapObj() ? target->GetMapObj()->GetRecord() : NULL;
			RecordNode* recordParent = record ? record->GetParent() : NULL;
		
			if (record && recordParent && recordParent->GetName() == "Crush")
			{
				if (record->GetName() == "dinamo")
				{
					target->SetMaxTimeLife(0.5f);
				}		
			}*/

			if (target)
			{
				GameCar* car = target->IsCar();
				if (car)
				{
					if (car == nullptr || car->IsShell() || car->IsImmortal())
						return;

					DamageTarget(target, _desc.damage * contact.deltaTime, dtNull);
					car->SetBurn(true);
					car->SetBurnDamage(_desc.damage / 10);
					car->GetMapObj()->GetPlayer()->SetMaxBurnTime(-1.0f);
				}
			}
		}

		bool Proj::ArtilleryPrepare(const ShotContext& ctx)
		{
			D3DXVECTOR3 speed = CalcSpeed(_weapon);
			LocateProj(_weapon, true, true, &speed);

			InitModel();
			CreatePxBox(px::Scene::cdgShotTrack);

			NxBodyDesc bodyDesc;
			bodyDesc.flags = NX_BF_DISABLE_GRAVITY;
			bodyDesc.linearVelocity = NxVec3(speed);
			CreateBody(bodyDesc);

			GetPxActor().SetContactReportFlags(NX_NOTIFY_ALL);
			SetIgnoreContactProj(true);

			return true;
		}

		void Proj::ArtilleryContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			if (target == nullptr || GetTimeLife() < 0.2f)
				return;

			GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			               GetNxActor()
				               ? target->GetParent()->IsCar()
				               : target->IsCar();

			if (car)
			{
				this->Death();
				car->Death();
				if (_weapon && _weapon->GetParent() != nullptr)
					_weapon->GetParent()->IsCar()->GetMapObj()->GetPlayer()->AddKillsTotal(1);
			}
			else
			{
				//рикошет от трассы:
				//GetPxActor().GetNxActor()->addLocalForce(NxVec3(ZVector), NX_SMOOTH_VELOCITY_CHANGE); 
				this->SetMaxTimeLife(0.1f);
				this->SetMaxLife(33);
				//для взрывных бочек:
				const MapObjRec* record = target->GetMapObj() ? target->GetMapObj()->GetRecord() : nullptr;
				const RecordNode* recordParent = record ? record->GetParent() : nullptr;

				if (record && recordParent && recordParent->GetName() == "Crush")
				{
					if (record->GetName() == "dinamo")
					{
						target->SetMaxTimeLife(0.1f);
					}
				}
			}
		}

		void Proj::ArtilleryUpdate(float deltaTime) const
		{
			if (GetLiveState() != lsDeath)
			{
				if (GetTimeLife() < 0.1f)
				{
					if (_desc.damage != 0)
						GetGrActor().SetVisible(false);
					GetPxActor().GetNxActor()->addLocalForce(NxVec3(ZVector) * _desc.angleSpeed,
					                                         NX_SMOOTH_VELOCITY_CHANGE);
					GetPxActor().GetNxActor()->addLocalForce(NxVec3(YVector) * (_desc.damage * 3),
					                                         NX_SMOOTH_VELOCITY_CHANGE);
				}
				else
				{
					GetGrActor().SetVisible(true);
					if (this->GetMaxLife() != 33 && GetTimeLife() > 0.5f)
					{
						GetPxActor().GetNxActor()->addLocalForce(
							NxVec3(D3DXVECTOR3(_desc.damage, 0, -1.2f + _desc.damage)), NX_SMOOTH_VELOCITY_CHANGE);
					}
				}
			}
		}

		bool Proj::GrenadePrepare(const ShotContext& ctx)
		{
			_vec1.z = 0.0f;

			D3DXVECTOR3 speed = CalcSpeed(_weapon);
			LocateProj(_weapon, true, true, &speed);

			InitModel();
			CreatePxBox(px::Scene::cdgDefault);
			_time1 = 0.0f;

			NxBodyDesc bodyDesc;
			bodyDesc.flags = NX_BF_DISABLE_GRAVITY;
			bodyDesc.linearVelocity = NxVec3(speed);
			CreateBody(bodyDesc);

			GetPxActor().SetContactReportFlags(NX_NOTIFY_ALL);
			if (_weapon && _weapon->GetParent() != nullptr && _weapon->GetParent()->IsCar()->InRage())
				this->SetRage(true);

			return true;
		}

		void Proj::GrenadeContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			               GetNxActor()
				               ? target->GetParent()->IsCar()
				               : target->IsCar();

			if (target == nullptr || _time1 < 0.1f)
				return;

			if (car)
			{
				if (this->GetRage())
					DamageTarget(car, _desc.damage * 1.25f, dtCash);
				else
					DamageTarget(car, _desc.damage, dtCash);
				this->Death();
			}
			else
			{
				D3DXVECTOR3 ReverseVector(GetPxActor().GetNxActor()->getLinearVelocity().x + RandomRange(-0.5f, 0.5f),
				                          GetPxActor().GetNxActor()->getLinearVelocity().y + RandomRange(-0.5f, 0.5f),
				                          10.0f + RandomRange(-2.0f, 3.0f));
				GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(ReverseVector));
				this->SetMaxLife(77);
				const MapObjRec* record = target->GetMapObj() ? target->GetMapObj()->GetRecord() : nullptr;
				const RecordNode* recordParent = record ? record->GetParent() : nullptr;

				if (record && recordParent && recordParent->GetName() == "Crush")
				{
					if (record->GetName() == "dinamo")
					{
						target->SetMaxTimeLife(0.1f);
					}
				}
			}
		}

		void Proj::GrenadeUpdate(float deltaTime)
		{
			if (_time1 >= 0)
			{
				_time1 += deltaTime;
			}

			if (GetLiveState() != lsDeath)
			{
				if (_weapon)
				{
					if (GetTimeLife() < 0.1f)
					{
						D3DXVECTOR3 speed = CalcSpeed(_weapon);
						GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(speed));
						GetPxActor().GetNxActor()->addForce(NxVec3(ZVector * 5), NX_VELOCITY_CHANGE);
					}
					else if (GetTimeLife() >= 0.1f)
					{
						GetPxActor().GetNxActor()->addForce(NxVec3(-ZVector / 2), NX_VELOCITY_CHANGE);
					}
				}
			}
		}

		bool Proj::TorpedaPrepare(GameObject* weapon)
		{
			_time1 = 0.4f;
			this->SetMaxLife(static_cast<float>(weapon->GetParent()->IsCar()->GetMapObj()->GetPlayer()->GetPlace()));
			return FixRocketPrepare(weapon, true, &_vec1);
		}

		void Proj::TorpedaUpdate(float deltaTime)
		{
			if (_weapon == nullptr)
				this->Death();

			_time1 = std::max(_time1 - deltaTime, 0.0f);

			MapObj* target = _shot.GetTargetMapObj();
			if (target && _time1 == 0.0f)
			{
				const D3DXVECTOR3 targPos = target->GetGameObj().GetWorldPos();
				const D3DXVECTOR3 pos = this->GetWorldPos();

				D3DXVECTOR3 dir = targPos - pos;
				const float dist = D3DXVec3Length(&dir);
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

				float speed;

				if (_desc.speedRelative)
				{
					speed = D3DXVec3Length(&_vec1);
				}
				else
				{
					speed = std::max(D3DXVec3Dot(&_vec1, &dir), _desc.speed);
				}

				_vec1 = dir * speed;

				if (target->GetGameObj().IsCar()->GetMapObj()->GetPlayer()->GetPlace() < this->GetMaxLife())
					this->GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(dir * std::max(_desc.speed, speed)));
			}
		}

		void Proj::SphereUpdate(float deltaTime)
		{
			if (_weapon == nullptr)
				this->Death();

			_time1 = std::max(_time1 - deltaTime, 0.0f);

			MapObj* target = _shot.GetTargetMapObj();
			if (target && _time1 == 0.0f)
			{
				const D3DXVECTOR3 targPos = target->GetGameObj().GetWorldPos();
				const D3DXVECTOR3 pos = this->GetWorldPos();

				D3DXVECTOR3 dir = targPos - pos;
				const float dist = D3DXVec3Length(&dir);
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

				float speed;

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

		bool Proj::ImpulsePrepare(GameObject* weapon)
		{
			_tick1 = 0;
			_time1 = 0.4f;
			return FixRocketPrepare(weapon, true, &_vec1);
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

					MapObj* taget = FindNextTaget(D3DX_PI / 2);
					if (taget == nullptr)
					{
						this->Death(dtEnergy, contactActor);
						return;
					}

					ShotDesc shot = _shot;
					shot.SetTargetMapObj(taget);
					SetShot(shot);
				}
				else if (_shot.GetTargetMapObj() == nullptr)
				{
					DamageTarget(contactActor, _desc.damage, dtEnergy);
					this->Death(dtEnergy, contactActor);
				}
			}
		}

		void Proj::PlazmaContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			const GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			                                                                                    GetNxActor()
				                     ? target->GetParent()->IsCar()
				                     : target->IsCar();
			if (target == nullptr)
				return;

			MapObjRec* record = target->GetMapObj() ? target->GetMapObj()->GetRecord() : nullptr;
			const RecordNode* recordParent = record ? record->GetParent() : nullptr;

			if ((record && record->GetCategory() == MapObjLib::ctCar) || (recordParent && recordParent->GetName() ==
				"Crush"))
			{
				ShotContext ctx;
				if (car)
				{
					if (car->IsShell() == true)
					{
						if (this->GetWeapon())
						{
							const D3DXVECTOR3 speed = CalcSpeed(this->GetWeapon());
							this->GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(-speed));
						}
					}
					else
					{
						if (this->GetMaxLife() != 88)
						{
							GetPxActor().GetNxActor()->setLinearVelocity(
								GetPxActor().GetNxActor()->getLinearVelocity() * 0.6f);
							this->SetMaxLife(88);
						}
						DamageTarget(target, _desc.damage * contact.deltaTime, dtLaser);
					}
				}
				else
				{
					const MapObjRec* record = target->GetMapObj() ? target->GetMapObj()->GetRecord() : nullptr;
					const RecordNode* recordParent = record ? record->GetParent() : nullptr;

					if (record && recordParent && recordParent->GetName() == "Crush")
					{
						if (record->GetName() == "dinamo")
						{
							target->SetMaxTimeLife(0.1f);
						}
					}
				}
			}
		}

		void Proj::BlazerContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			if (target == nullptr || target->IsCar() == nullptr)
				return;

			const GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			                                                                                    GetNxActor()
				                     ? target->GetParent()->IsCar()
				                     : target->IsCar();

			if (car)
			{
				if (car->GetMapObj() == nullptr)
					return;

				if (car->GetDamageStop() == true)
					this->Death();

				const Player* plr = car->GetMapObj()->GetPlayer();
				//магма должна дамажить конкретного игрока:
				if (this->GetMaxLife() != plr->GetGamerId())
				{
					//только если это первый контакт!
					if (this->GetName() != "IsContacted")
					{
						this->SetName("IsContacted");
						this->SetMaxLife(static_cast<float>(plr->GetGamerId()));
						this->SetMaxTimeLife(_desc.angleSpeed);
					}
				}
				else
				{
					if (this->GetName() == "IsContacted")
					{
						GetGrActor().SetVisible(false);
						this->SetPos(car->GetPos());
						//время действия не должно превышать минимальное время респавна машины;				
						DamageTarget(target, _desc.damage * contact.deltaTime, dtSimple);
					}
				}
			}
			/* менее имбовый, более стабильный и упрощенный вариант магмагана:
			GameObject* target = GetGameObjFromActor(contact.actor);
			if (target == NULL || target->IsCar() == false)
				return;
		
			GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().GetNxActor() ? target->GetParent()->IsCar() : target->IsCar();
		
			if (car)
			{			
				if (car->GetMapObj() == NULL || car->IsShell() || car->IsImmortal())
					return;
		
				car->SetBurn(true);
				car->SetBurnDamage(_desc.damage);
				car->GetMapObj()->GetPlayer()->SetMaxBurnTime(-_desc.angleSpeed);
			}*/
		}

		void Proj::BlazerUpdate(float deltaTime)
		{
			if (GetGrActor().GetVisible() == true)
				RocketUpdate(deltaTime);
		}

		void Proj::PhoenixContact(const px::Scene::OnContactEvent& contact) const
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			if (target)
			{
				DamageTarget(target, _desc.damage * contact.deltaTime, dtCash);
			}
		}

		void Proj::PhoenixUpdate(float deltaTime)
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

		bool Proj::MagneticPrepare(GameObject* weapon, D3DXVECTOR3* speedVec, NxCollisionGroup pxGroup)
		{
			_vec1.z = 0.0f;

			D3DXVECTOR3 speed = CalcSpeed(weapon);

			//притяжение вдвое сильнее, если машина очень медленно едет, либо стоит на месте:
			if (speedVec)
			{
				if (weapon && weapon->GetParent()->IsCar()->GetPxActor().GetNxActor()->getLinearVelocity().magnitude() <
					5.0f)
					*speedVec = speed * 2;
				else
					*speedVec = speed;
			}

			LocateProj(weapon, true, true, &speed);
			InitModel();
			CreatePxBox(pxGroup);

			NxBodyDesc bodyDesc;
			bodyDesc.flags = NX_BF_DISABLE_GRAVITY;
			bodyDesc.linearVelocity = NxVec3(speed);

			CreateBody(bodyDesc);

			this->GetPxActor().SetContactReportFlags(NX_NOTIFY_ALL);
			this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);
			SetIgnoreContactProj(true);

			return true;
		}

		void Proj::MagneticContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);

			if (target == nullptr)
				return;

			MapObjRec* record = target->GetMapObj() ? target->GetMapObj()->GetRecord() : nullptr;
			const RecordNode* recordParent = record ? record->GetParent() : nullptr;

			if (target && record)
			{
				if (record->GetCategory() == MapObjLib::ctCar)
				{
					D3DXVECTOR3 speed;
					if (_weapon)
						speed = CalcSpeed(_weapon);
					else
						speed = NullVector;

					if (_weapon && this->GetMaxLife() == -1)
					{
						target->GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(-(speed / 100) * _desc.damage));
						this->Death();
					}
				}

				if (recordParent && recordParent->GetName() == "CarDestr" || recordParent && recordParent->GetName() ==
					"Crush")
				{
					D3DXVECTOR3 speed;
					if (_weapon)
						speed = CalcSpeed(_weapon);
					else
						speed = NullVector;

					if (_weapon && this->GetMaxLife() == -1)
					{
						target->GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(-speed));
						this->Death();
					}
				}

				if (record->GetCategory() == MapObjLib::ctBonus)
				{
					//черный список объектов которые не должны притягиваться маг. пушкой:
					if (record->GetName() != "speedArrow" && record->GetName() != "lusha" && record->GetName() !=
						"graviton" && record->GetName() != "sand" && record->GetName() != "spinner" && record->GetName()
						!= "maslo" && record->GetName() != "acid")
					{
						target->SetPos(D3DXVECTOR3(this->GetPos().x, this->GetPos().y, this->GetPos().z - 1.0f));
						GetGrActor().SetVisible(false);
						if (this->GetMaxLife() == -1)
						{
							this->SetMaxTimeLife(0.3f);
							this->SetMaxLife(14);
						}
					}
					else
						return;
				}
			}
		}

		void Proj::MagneticUpdate(float deltaTime)
		{
			RocketUpdate(deltaTime);

			if (GetLiveState() != lsDeath && this->GetMaxLife() == 14)
			{
				D3DXVECTOR3 speed;

				if (_weapon)
					speed = CalcSpeed(_weapon);
				else
					speed = NullVector;

				this->SetMaxLife(69);
				GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(-speed));
			}
		}

		//Grenade в даном случае относиться к МВД-11.
		bool Proj::DiscPrepare(GameObject* weapon)
		{
			_time1 = 0.0f;
			_vec1.z = 0.0f;

			D3DXVECTOR3 speed = CalcSpeed(weapon);

			LocateProj(weapon, true, true, &speed);
			InitModel();
			CreatePxBox(px::Scene::cdgShotBorder);

			NxBodyDesc bodyDesc;
			bodyDesc.flags = NX_BF_DISABLE_GRAVITY;
			bodyDesc.linearVelocity = NxVec3(speed);

			CreateBody(bodyDesc);

			this->GetPxActor().SetContactReportFlags(NX_NOTIFY_ALL);
			this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);
			if (weapon && weapon->GetParent() != nullptr && weapon->GetParent()->IsCar()->InRage())
				this->SetRage(true);

			return true;
		}

		void Proj::DiscContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			const GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			                                                                                    GetNxActor()
				                     ? target->GetParent()->IsCar()
				                     : target->IsCar();

			if (target == nullptr || GetLiveState() == lsDeath)
				return;

			if (target)
			{
				if (car)
				{
					if (_time1 <= -0.1f)
					{
						if (this->GetRage())
							DamageTarget(target, _desc.damage * 1.25f, dtSimple);
						else
							DamageTarget(target, _desc.damage, dtSimple);
						this->Death();
					}
				}
				AddContactForce(target, contact,
				                _desc.mass * D3DXVECTOR3(this->GetPxActor().GetNxActor()->getLinearVelocity().get()),
				                NX_IMPULSE);
				const MapObjRec* record = target->GetMapObj() ? target->GetMapObj()->GetRecord() : nullptr;
				const RecordNode* recordParent = record ? record->GetParent() : nullptr;

				if (record && recordParent && recordParent->GetName() == "Crush")
				{
					if (record->GetName() == "dinamo")
					{
						target->SetMaxTimeLife(0.1f);
					}
				}
			}

			const NxVec3 subvelocity = GetPxActor().GetNxActor()->getLinearVelocity() / 2;
			NxContactStreamIterator contIter(contact.stream);

			if (GetMaxLife() != 99 && ContainsContactGroup(contIter, contact.actorIndex, px::Scene::cdgShotTransparency)
				&& subvelocity.magnitude() > 2.0f)
			{
				//рикошет от бордюра:
				/*
				NxVec3 norm = contIter.getPatchNormal();
				if (contact.actorIndex == 0)
					norm = -norm;
		
				NxVec3 velNorm = subvelocity;
				velNorm.normalize();
				float angle = velNorm.dot(norm);		
				if (abs(angle) > 0.1f)
				{
					D3DXPLANE plane;
					D3DXPlaneFromPointNormal(&plane, &NullVector, &D3DXVECTOR3(norm.get()));
					D3DXMATRIX mat;
					D3DXMatrixReflect(&mat, &plane);
		
					D3DXVECTOR3 vel(subvelocity.get());
					D3DXVec3TransformNormal(&vel, &vel, &mat);
					subvelocity = NxVec3(vel);
				}
				else
					subvelocity = -subvelocity;
		
				
				GetPxActor().GetNxActor()->setLinearVelocity(subvelocity); 
				SetMaxTimeLife(0.3f);*/
				//сейчас действует остановка диска при контакте с бордюром (что более практично, нежели рикошет):
				GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(NullVector));
				SetMaxLife(99);
			}
		}

		void Proj::DiscUpdate(float deltaTime)
		{
			RocketUpdate(deltaTime);

			_time1 -= deltaTime;

			if (GetLiveState() != lsDeath)
			{
				if (GetPxActor().GetNxActor()->getLinearVelocity().magnitude() > 1.0f)
				{
					SetWorldRot(this->GetWorldRot());
					D3DXQUATERNION rot;
					D3DXQuaternionRotationAxis(&rot, &ZVector, 8 * deltaTime);
					this->SetRot(this->GetRot() * rot);
				}

				if (_weapon && this->GetMaxLife() != 99)
				{
					if (GetTimeLife() > 0.1f && GetTimeLife() < 1.6f)
					{
						D3DXVECTOR3 speed = CalcSpeed(_weapon);
						GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(speed));
					}
					else if (GetTimeLife() >= 1.6f)
					{
						GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(NullVector));
					}
				}
			}
		}

		bool Proj::ShockerPrepare(GameObject* weapon)
		{
			return FixRocketPrepare(weapon, true);
		}

		void Proj::ShockerContact(const px::Scene::OnContactEvent& contact) const
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			               GetNxActor()
				               ? target->GetParent()->IsCar()
				               : target->IsCar();

			if (target == nullptr)
				return;

			if (car && car->GetMapObj() != nullptr && car->IsShell() == false && car->IsImmortal() == false)
			{
				car->SetInverseTime(_desc.mass);
				if (_desc.speedRelative == true)
					car->GetMapObj()->GetPlayer()->SetShotFreeze(true);
				car->SetTurnInverse(true);
				DamageTarget(car, _desc.damage * contact.deltaTime, dtBolt);
			}
		}

		void Proj::ShockerUpdate(float deltaTime)
		{
			if (GetLiveState() != lsDeath && this->GetWeapon() != nullptr && _weapon->GetParent()->GetPxActor().
				GetNxActor() != nullptr)
			{
				if (_weapon && _weapon->GetPxActor().GetNxActor() && GetPxActor().GetNxActor())
				{
					GetPxActor().GetNxActor()->setLinearVelocity(
						_weapon->GetPxActor().GetNxActor()->getLinearVelocity());
					D3DXVECTOR3 pos;
					_weapon->GetGrActor().LocalToWorldCoord(_desc.pos, pos);
					SetPos(pos);
					SetRot(_weapon->GetWorldRot());
				}
			}
		}

		bool Proj::MinePrepare(const ShotContext& ctx, bool lockMine)
		{
			_time1 = -1.0f;
			InitModel();
			CreatePxBox(px::Scene::cdgShotTrack);

			const AABB aabb = ComputeAABB(true);
			GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);
			SetIgnoreContactProj(true);
			_time1 = 0.0f;

			if (ctx.projMat)
			{
				SetWorldPos(ctx.projMat->t.get());

				return true;
			}
			D3DXVECTOR3 rayPos = _desc.pos;
			if (_weapon)
				_weapon->GetGrActor().LocalToWorldCoord(rayPos, rayPos);
			const NxRay nxRay(NxVec3(rayPos) + NxVec3(0, 0, 2.0f), NxVec3(-ZVector));

			NxRaycastHit hit;
			const NxShape* hitShape = GetLogic()->GetPxScene()->GetNxScene()->raycastClosestShape(
				nxRay, NX_STATIC_SHAPES, hit, (1 << px::Scene::cdgTrackPlane) | (1 << px::Scene::cdgShotTrack),
				NX_MAX_F32, NX_RAYCAST_SHAPE | NX_RAYCAST_IMPACT | NX_RAYCAST_NORMAL);

			if (hitShape && hitShape->getGroup() != px::Scene::cdgShotTrack) //&& hit.distance < _desc.projMaxDist)
			{
				const float offs = std::max(-aabb.min.z, 0.01f);
				const D3DXVECTOR3 normal = hit.worldNormal.get();

				SetWorldPos(D3DXVECTOR3(hit.worldImpact.get()) + ZVector * offs);
				SetWorldUp(normal);

				if (lockMine)
				{
					GameCar* car = _weapon && _weapon->GetParent() ? _weapon->GetParent()->IsCar() : nullptr;
					if (car)
					{
						//время минобага (ванильно 0.4f)
						if (car->GetMapObj()->GetPlayer()->IsComputer())
						{
							//боты не используют минобаг
							car->LockMine(0.0f);
						}
						else
						{
							//для игроков минобаг увеличен до 1.0сек
							car->LockMine(1.0f);
						}
					}
				}
				return true;
			}
			return false;
		}

		void Proj::MineContact(const px::Scene::OnContactEvent& contact, bool testLockMine)
		{
			GameObject* targetObj = GetGameObjFromActor(contact.actor);
			GameCar* target = targetObj ? targetObj->IsCar() : nullptr;
			//GameObject* car = _weapon ? _weapon->GetParent() : NULL;

			if (target == nullptr || (testLockMine && target->IsMineLocked() && GetLogic() && GetLogic()->GetRace()->
				GetEnableMineBug()))
				return;

			if (target->GetDemining() == true)
			{
				this->SetMaxTimeLife(0.3f);
				return;
			}

			GetLogic()->MineContact(this, target, GetContactPoint(contact));
		}

		float Proj::MineUpdate(float deltaTime, float delay)
		{
			if (GetLiveState() != lsDeath && GetTimeLife() > 2.0f)
			{
				if (_weapon != nullptr)
					GetPxActor().GetScene()->GetNxScene()->setActorPairFlags(
						*_weapon->GetPxActor().GetNxActor(), *GetPxActor().GetNxActor(), NX_NOTIFY_ALL);
			}
			if (_time1 >= 0)
			{
				_time1 += deltaTime;
				const float alpha = ClampValue(delay > 0 ? _time1 / delay : 1.0f, 0.0f, 1.0f);
				if (alpha == 1.0f)
					_time1 = -1.0f;

				return alpha;
			}

			return -1.0f;
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
				if (_desc.GetModel3() && this->GetMaxLife() != 99)
				{
					if (this->GetMaxLife() != 99)
					{
						//piece1:
						MapObj* piece1 = &GetLogic()->GetMap()->AddMapObj(_desc.GetModel3());
						piece1->GetGameObj().SetPos(GetPos());
						piece1->GetGameObj().SetRot(GetRot());
						piece1->GetGameObj().SetScale(GetScale());

						NxVec3 dir1(D3DXVECTOR3(0.0f, 0.2f, 0.5f));
						dir1.normalize();
						piece1->GetGameObj().GetPxActor().GetNxActor()->addForce(
							piece1->GetGameObj().GetPxActor().GetBody()->GetDesc().mass * dir1 * 5.0f, NX_IMPULSE);

						//piece2:
						MapObj* piece2 = &GetLogic()->GetMap()->AddMapObj(_desc.GetModel3());
						piece2->GetGameObj().SetPos(GetPos());
						piece2->GetGameObj().SetRot(GetRot());
						piece2->GetGameObj().SetScale(GetScale());

						NxVec3 dir2(D3DXVECTOR3(-0.2f, -0.2f, 0.5f));
						dir2.normalize();
						piece2->GetGameObj().GetPxActor().GetNxActor()->addForce(
							piece2->GetGameObj().GetPxActor().GetBody()->GetDesc().mass * dir2 * 5.0f, NX_IMPULSE);

						//piece3:
						MapObj* piece3 = &GetLogic()->GetMap()->AddMapObj(_desc.GetModel3());
						piece3->GetGameObj().SetPos(GetPos());
						piece3->GetGameObj().SetRot(GetRot());
						piece3->GetGameObj().SetScale(GetScale());

						NxVec3 dir3(D3DXVECTOR3(0.2f, -0.2f, 0.5f));
						dir3.normalize();
						piece3->GetGameObj().GetPxActor().GetNxActor()->addForce(
							piece3->GetGameObj().GetPxActor().GetBody()->GetDesc().mass * dir3 * 5.0f, NX_IMPULSE);

						//piece4:
						MapObj* piece4 = &GetLogic()->GetMap()->AddMapObj(_desc.GetModel3());
						piece4->GetGameObj().SetPos(GetPos());
						piece4->GetGameObj().SetRot(GetRot());
						piece4->GetGameObj().SetScale(GetScale());

						NxVec3 dir4(D3DXVECTOR3(-0.2f, 0.2f, 0.5f));
						dir4.normalize();
						piece4->GetGameObj().GetPxActor().GetNxActor()->addForce(
							piece4->GetGameObj().GetPxActor().GetBody()->GetDesc().mass * dir4 * 5.0f, NX_IMPULSE);

						//piece5:
						MapObj* piece5 = &GetLogic()->GetMap()->AddMapObj(_desc.GetModel3());
						piece5->GetGameObj().SetPos(GetPos());
						piece5->GetGameObj().SetRot(GetRot());
						piece5->GetGameObj().SetScale(GetScale());

						NxVec3 dir5(D3DXVECTOR3(0.0f, 0.0f, 1.0f));
						dir5.normalize();
						piece5->GetGameObj().GetPxActor().GetNxActor()->addForce(
							piece5->GetGameObj().GetPxActor().GetBody()->GetDesc().mass * dir5 * 5.0f, NX_IMPULSE);

						this->SetMaxLife(99);
					}
				}

				Death();
			}
		}

		bool Proj::MinePiecePrepare(const ShotContext& ctx)
		{
			if (_weapon)
				LocateProj(_weapon, true, true, nullptr);
			InitModel();
			CreatePxBox(px::Scene::cdgShotTrack);
			_time1 = -1.0f;

			const NxBodyDesc bodyDesc;
			CreateBody(bodyDesc);

			GetPxActor().SetContactReportFlags(NX_NOTIFY_ALL);

			return true;
		}

		bool Proj::MasloPrepare(const ShotContext& ctx)
		{
			if (_weapon != nullptr)
				if (_weapon->GetParent()->IsCar()->IsAnyWheelContact() == false && _weapon->GetParent()->GetMapObj()->
					GetPlayer()->GetMasloDrop() == false)
					return false;

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
			if (target == nullptr)
				return;

			GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			               GetNxActor()
				               ? target->GetParent()->IsCar()
				               : target->IsCar();
			if (car == nullptr || car->IsMineLocked())
				return;

			Player* plr = car->GetMapObj()->GetPlayer();

			//на новичке боты не стреляют, если наехали на масло:
			if (plr->IsComputer() && GAME_DIFF == 0)
			{
				plr->SetShotFreeze(true);
			}

			if (car)
			{
				const float driftStrength = plr->GetDriftStrength();

				if (car->GetPxActor().GetNxActor()->getLinearVelocity().magnitude() > 1.0f)
				{
					D3DXPLANE plane;
					D3DXPlaneFromPointNormal(&plane, &car->GetGrActor().GetWorldPos(),
					                         &car->GetGrActor().GetWorldRight());
					const float dist = PlaneDistToPoint(plane, GetWorldPos());

					if (plr->GetSlotInst(Player::stWheel))
					{
						//Wheel1: масло сильно заносит машину + замедление на 2 секунды.
						if (plr->GetSlotInst(Player::stWheel)->GetItem().GetName() == "scWheel1")
						{
							car->SetMaxTimeSR(2.0f);
							car->SetSlowRide(true);
							plr->ResetMaxSpeed();
							car->SetMaxSpeed(car->GetMaxSpeed() * 0.6f);

							if (GetLogic() && GetLogic()->GetRace()->GetOilDestroyer())
							{
								//в режиме разрушения масла, заносы сильнее:
								car->LockClutch(-3.0f * driftStrength);
								this->Death();
							}
							else
							{
								car->LockClutch(abs(dist) > 0.1f && dist > 0
									                ? -((_desc.damage * 1.6f) * driftStrength)
									                : ((_desc.damage * 1.6f) * driftStrength));
							}
						}
						//Wheel2:  масло заносит машину слабее, замедление на 1 секунду.
						else if (plr->GetSlotInst(Player::stWheel)->GetItem().GetName() == "scWheel2")
						{
							car->SetMaxTimeSR(1.0f);
							car->SetSlowRide(true);
							plr->ResetMaxSpeed();
							car->SetMaxSpeed(car->GetMaxSpeed() * 0.6f);

							if (GetLogic() && GetLogic()->GetRace()->GetOilDestroyer())
							{
								//в режиме разрушения масла, заносы сильнее:
								car->LockClutch(-2.7f * driftStrength);
								this->Death();
							}
							else
							{
								car->LockClutch(abs(dist) > 0.1f && dist > 0
									                ? -((_desc.damage * 1.2f) * driftStrength)
									                : ((_desc.damage * 1.2f) * driftStrength));
							}
						}
						//Wheel3:  масло заносит машину слабее, замедление отсутсвует.
						else if (plr->GetSlotInst(Player::stWheel)->GetItem().GetName() == "scWheel3")
						{
							if (GetLogic() && GetLogic()->GetRace()->GetOilDestroyer())
							{
								//в режиме разрушения масла, заносы сильнее:
								car->LockClutch(-2.6f * driftStrength);
								this->Death();
							}
							else
							{
								car->LockClutch(abs(dist) > 0.1f && dist > 0
									                ? -_desc.damage * driftStrength
									                : _desc.damage * driftStrength);
							}
						}
						//Wheel4: занос от масла небольшой.
						else if (plr->GetSlotInst(Player::stWheel)->GetItem().GetName() == "scWheel4")
						{
							if (GetLogic() && GetLogic()->GetRace()->GetOilDestroyer())
							{
								//в режиме разрушения масла, заносы сильнее:
								car->LockClutch(-1.5f * driftStrength);
								this->Death();
							}
							else
							{
								car->LockClutch(abs(dist) > 0.1f && dist > 0
									                ? -((_desc.damage * driftStrength) / 2)
									                : ((_desc.damage * driftStrength) / 2));
							}
						}
						//GusWheel: только замедление на 2.5 секунды.
						else
						{
							car->SetMaxTimeSR(2.5f);
							car->SetSlowRide(true);
							plr->ResetMaxSpeed();
							car->SetMaxSpeed(car->GetMaxSpeed() * 0.6f);
						}
					}
					else
					{
						//Для машин без колес замедление на 2 секунды (-40% скорости)
						car->SetMaxTimeSR(2.0f);
						car->SetSlowRide(true);
						plr->ResetMaxSpeed();
						car->SetMaxSpeed(car->GetMaxSpeed() * 0.6f);
					}
				}
			}
		}

		void Proj::MasloUpdate(float deltaTime)
		{
			const float alpha = MineUpdate(deltaTime);

			if (_model && alpha >= 0)
				_model->GetGameObj().SetScale(alpha);
		}

		void Proj::AcidContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);

			if (target)
			{
				GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
				               GetNxActor()
					               ? target->GetParent()->IsCar()
					               : target->IsCar();
				Player* plr = car->GetMapObj()->GetPlayer();
				target->GetPxActor().GetNxActor();

				if (car == nullptr || car->IsMineLocked())
					return;

				DamageTarget(target, _desc.damage * contact.deltaTime, dtNull);

				car->SetMaxTimeSR(_desc.maxDist); //время замедления машины (при наезде на кислоту)
				car->SetSlowRide(true);
				plr->ResetMaxSpeed();
				car->SetMaxSpeed(car->GetMaxSpeed() * _desc.speed); //собственно сила замедления.

				if (car->GetLife() > _desc.damage && this->GetMaxTimeLife() > 2.0f)
					this->SetMaxTimeLife(2.0f);
				else
					this->SetMaxTimeLife(1.0f);
			}
		}

		bool Proj::FirePrepare(GameObject* weapon, bool disableGravity, D3DXVECTOR3* speedVec, NxCollisionGroup pxGroup)
		{
			GameCar* car = weapon->GetParent()->IsCar();
			D3DXVECTOR3 speed = NullVector;
			if (car)
			{
				//если машина движется, а не стоит то задаем огнемету следующие парамерты:
				if (car->GetPxActor().GetNxActor()->getLinearVelocity().magnitude() > 10.0f)
				{
					//время жизни больше:
					this->SetMaxTimeLife(0.7f);
					//скорость движения и вектор снаряда зависит от последней передачи и текущей скорости:
					if (car->GetPxActor().GetNxActor()->getLinearVelocity().magnitude() > 37.0f)
					{
						//задний ход на большой скорости:			
						if (car->GetMoveCar() == car->mcBack || car->GetLastMoveState() == 2)
						{
							speed = GetFixedBackSpeed(weapon);
							this->SetMaxTimeLife(0.2f);
						}
						else
							speed = ZVector;
					}
					else
					{
						//задний ход:	
						if (car->GetMoveCar() == car->mcBack || car->GetLastMoveState() == 2)
						{
							speed = GetFixedBackSpeed(weapon);
							this->SetMaxTimeLife(0.2f);
						}
						else
							speed = CalcSpeed(weapon);
					}
				}
				else
				{
					//если машина неподвижна:
					this->SetMaxTimeLife(0.5f);
					speed = GetDoubleSpeed(weapon); //скорость снаряда огнемета увеличена в два раза.		
				}
			}

			_vec1.z = 0.0f;
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

		void Proj::FireContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			const GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			                                                                                    GetNxActor()
				                     ? target->GetParent()->IsCar()
				                     : target->IsCar();
			if (target == nullptr)
				return;

			MapObjRec* record = target->GetMapObj() ? target->GetMapObj()->GetRecord() : nullptr;
			const RecordNode* recordParent = record ? record->GetParent() : nullptr;

			if ((record && record->GetCategory() == MapObjLib::ctCar) || (recordParent && recordParent->GetName() ==
				"Crush"))
			{
				if (car && car->IsShell() == false && car->IsImmortal() == false)
				{
					DamageTarget(target, _desc.damage, dtMine);
					this->Death();
				}

				auto dir = D3DXVECTOR3(this->GetPxActor().GetNxActor()->getLinearVelocity().get());
				const float dirLength = D3DXVec3Length(&dir);

				if (dirLength > 1.0f)
				{
					dir /= dirLength;

					D3DXVECTOR3 contactDir = GetContactPoint(contact);
					D3DXVec3Cross(&contactDir, &contactDir, &dir);

					NxVec3 vec3(contactDir);
					if (vec3.magnitude() > 0.01f)
					{
						vec3.normalize();
						target->GetPxActor().GetNxActor()->addLocalTorque(vec3 * _desc.mass * 0.2f, NX_VELOCITY_CHANGE);
					}

					D3DXVec3Normalize(&dir, &(dir - ZVector));
				}
			}
		}

		void Proj::FireUpdate(float deltaTime) const
		{
			if (_weapon && _weapon->GetParent() != nullptr)
			{
				GameCar* car = _weapon->GetParent()->IsCar();

				if (car && car->GetPxActor().GetNxActor()->getLinearVelocity().magnitude() > 10.0f)
				{
					if (GetLiveState() != lsDeath && GetTimeLife() >= 0.2f)
					{
						if (car->GetMoveCar() != car->mcBack && car->GetLastMoveState() != 2)
						{
							if (_weapon && _weapon->GetPxActor().GetNxActor() && GetPxActor().GetNxActor())
								GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(ZVector));
						}
					}
				}
			}
		}

		bool Proj::SpikesPrepare(const ShotContext& ctx, bool lockMine)
		{
			return MinePrepare(ctx, true);
		}

		void Proj::SpikesContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			               GetNxActor()
				               ? target->GetParent()->IsCar()
				               : target->IsCar();
			car->GetMapObj()->GetPlayer();

			if (car == nullptr || car->IsMineLocked())
				return;

			if (car->GetPxActor().GetNxActor()->getLinearVelocity().magnitude() > 8.0f)
			{
				car->SetWastedControl(true);
				car->SetRespBlock(true);
				this->Death();
			}
		}

		void Proj::SmokesContact(const px::Scene::OnContactEvent& contact) const
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			const GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			                                                                                    GetNxActor()
				                     ? target->GetParent()->IsCar()
				                     : target->IsCar();
			Player* plr = car->GetMapObj()->GetPlayer();
			//Заморозка стрельбы для ИИ:
			if (plr->IsComputer())
			{
				plr->SetShotFreeze(true);
			}
		}

		void Proj::BurnMineContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);

			if (target == nullptr)
				return;

			if (target && target->IsCar())
			{
				GameCar* car = target->IsCar();
				if (car->IsMineLocked() || car->IsShell() || car->IsImmortal())
					return;

				InitModel2();
				MapObj* mapObj = &GetLogic()->GetMap()->AddMapObj(_desc.GetModel2());
				mapObj->GetGameObj().SetPos(this->GetPos());
				mapObj->GetGameObj().SetMaxTimeLife(1.5f);
				this->SetMaxTimeLife(1.0f);


				car->SetBurn(true);
				car->SetBurnDamage(_desc.damage);
				car->GetMapObj()->GetPlayer()->SetMaxBurnTime(-_desc.angleSpeed);
			}
		}

		bool Proj::BarrelPrepare(const ShotContext& ctx)
		{
			if (_weapon)
				LocateProj(_weapon, true, true, nullptr);
			InitModel();
			CreatePxBox(px::Scene::cdgShotTrack);
			_time1 = -1.0f;

			const NxBodyDesc bodyDesc;
			CreateBody(bodyDesc);

			GetPxActor().SetContactReportFlags(NX_NOTIFY_ALL);

			return true;
		}

		bool Proj::BoomPrepare(const ShotContext& ctx)
		{
			InitModel();
			CreatePxBox(px::Scene::cdgShotTrack);

			ComputeAABB(true);
			GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);

			NxBodyDesc bodyDesc;
			bodyDesc.flags = NX_BF_DISABLE_GRAVITY;
			CreateBody(bodyDesc);

			this->GetPxActor().SetContactReportFlags(NX_NOTIFY_ALL);
			this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);
			SetMaxTimeLife(0.2f);
			return true;
		}

		void Proj::BoomContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			               GetNxActor()
				               ? target->GetParent()->IsCar()
				               : target->IsCar();

			if (target == nullptr)
				return;

			if (car)
			{
				DamageTarget(car, _desc.damage, dtNull);
				this->Death();
			}
		}

		bool Proj::CorePrepare(GameObject* weapon, D3DXVECTOR3* speedVec, NxCollisionGroup pxGroup)
		{
			GameCar* car = _weapon && _weapon->GetParent() ? _weapon->GetParent()->IsCar() : nullptr;
			if (car)
			{
				//время минобага (ванильно 0.4f)
				if (car->GetMapObj()->GetPlayer()->IsComputer())
				{
					//боты не используют минобаг
					car->LockMine(0.0f);
				}
				else
				{
					//для игроков минобаг увеличен до 1.0сек
					car->LockMine(1.0f);
				}
			}
			_vec1.z = 0.0f;
			_time1 = -1.0f;

			//ядро движется в никуда:
			D3DXVECTOR3 speed = NullVector;
			if (speedVec)
				*speedVec = speed;

			LocateProj(weapon, true, true, &speed);
			InitModel();
			CreatePxBox(pxGroup);

			NxBodyDesc bodyDesc;
			bodyDesc.flags = NX_BF_DISABLE_GRAVITY;
			bodyDesc.linearVelocity = NxVec3(speed);

			CreateBody(bodyDesc);
			_time1 = 0.0f;

			this->GetPxActor().SetContactReportFlags(NX_NOTIFY_ALL);
			this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);
			SetIgnoreContactProj(true);

			return true;
		}

		void Proj::CoreContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			               GetNxActor()
				               ? target->GetParent()->IsCar()
				               : target->IsCar();
			car->GetMapObj()->GetPlayer();

			if (target == nullptr || _time1 <= _desc.speedRelativeMin)
				return;

			if (car)
			{
				if (car->GetMapObj() == nullptr || car->IsMineLocked())
					return;

				DamageTarget(car, _desc.damage, dtMine);
				//дополнительный подброс
				if (this->GetPxActor().GetNxActor() != nullptr && car->GetPxActor().GetNxActor() != nullptr)
				{
					if (car->IsAnyWheelContact())
					{
						if (car->GetPxActor().GetNxActor()->getLinearVelocity().magnitude() > 40.0f)
							target->GetPxActor().GetNxActor()->addForce(NxVec3((ZVector * _desc.speed) / 100),
							                                            NX_SMOOTH_VELOCITY_CHANGE);
						else
							target->GetPxActor().GetNxActor()->addForce(NxVec3((ZVector * _desc.speed) / 200),
							                                            NX_SMOOTH_VELOCITY_CHANGE);
					}
					else
					{
						if (car->GetPxActor().GetNxActor()->getLinearVelocity().magnitude() > 40.0f)
							target->GetPxActor().GetNxActor()->addForce(NxVec3(-((ZVector * _desc.speed) / 100)),
							                                            NX_SMOOTH_VELOCITY_CHANGE);
						else
							target->GetPxActor().GetNxActor()->addForce(NxVec3(-((ZVector * _desc.speed) / 200)),
							                                            NX_SMOOTH_VELOCITY_CHANGE);
					}
					this->Death();
				}
			}
		}

		void Proj::CoreUpdate(float deltaTime)
		{
			if (_time1 >= 0)
			{
				_time1 += deltaTime;
			}
			constexpr float cTrackHeight = 4;

			const D3DXVECTOR3 size = _pxBox->GetDimensions();
			NxVec3 pos = GetPxActor().GetNxActor()->getGlobalPosition();
			const NxRay nxRay(pos + NxVec3(0, 0, cTrackHeight), NxVec3(0, 0, -1.0f));

			NxRaycastHit hit;
			NxShape* hitShape = GetLogic()->GetPxScene()->GetNxScene()->raycastClosestShape(
				nxRay, NX_STATIC_SHAPES, hit, 1 << px::Scene::cdgTrackPlane, NX_MAX_F32,
				NX_RAYCAST_SHAPE | NX_RAYCAST_IMPACT);

			if (hitShape)
			{
				GetGameObjFromShape(hitShape);
				hitShape->getGroup();

				const float height = std::max(pos.z - hit.worldImpact.z, size.z);
				if (_vec1.z == 0.0f)
					_vec1.z = height;
				else if (_vec1.z - height > 0.1f)
					_vec1.z = height;

				pos.z = hit.worldImpact.z + _vec1.z;
				GetPxActor().GetNxActor()->setGlobalPosition(pos);
			}

			if (GetLiveState() != lsDeath)
			{
				if (_model)
				{
					SetWorldRot(this->GetWorldRot());
					D3DXQUATERNION rot;
					D3DXQuaternionRotationAxis(&rot, &XVector, _desc.angleSpeed * deltaTime);
					this->SetRot(this->GetRot() * rot);
				}

				if (_weapon == nullptr)
					this->Death();
			}
		}

		bool Proj::ScatterPrepare(GameObject* weapon, D3DXVECTOR3* speedVec, NxCollisionGroup pxGroup)
		{
			_vec1.z = 0.0f;
			_time1 = -1.0f;

			D3DXVECTOR3 speed = NullVector;
			if (speedVec)
				*speedVec = speed;

			LocateProj(weapon, true, true, &speed);
			InitModel();
			CreatePxBox(pxGroup);

			NxBodyDesc bodyDesc;
			bodyDesc.flags = 0;
			bodyDesc.linearVelocity = NxVec3(speed);

			CreateBody(bodyDesc);
			_time1 = 0.0f;

			this->GetPxActor().SetContactReportFlags(NX_NOTIFY_ALL);
			this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);
			SetIgnoreContactProj(true);

			int stage = 0;
			if (_weapon && _weapon->GetParent() != nullptr)
				stage = _weapon->GetParent()->GetMapObj()->GetPlayer()->GetSlotInst(Player::stMine)->GetItem().
				                 IsWeaponItem()->GetCurCharge();

			if (stage == 2 || stage == 4 || stage == 6 || stage == 8 || stage == 10)
				this->SetReverse(true);
			else
				this->SetReverse(false);

			return true;
		}

		void Proj::ScatterContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			const GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			                                                                                    GetNxActor()
				                     ? target->GetParent()->IsCar()
				                     : target->IsCar();
			car->GetMapObj()->GetPlayer();

			if (target == nullptr)
				return;

			if (car)
			{
				if (car->GetMapObj() == nullptr || _time1 <= ((_desc.speedRelativeMin / 100) * 1.2f))
					return;

				DamageTarget(target, _desc.damage, dtMine);
				//дополнительный подброс
				if (this->GetPxActor().GetNxActor() != nullptr)
				{
					if (this->GetMaxLife() == 88)
						car->GetPxActor().GetNxActor()->addForce(NxVec3(ZVector * _desc.speed / 4),
						                                         NX_SMOOTH_VELOCITY_CHANGE);
					else
						car->GetPxActor().GetNxActor()->addForce(
							NxVec3(this->GetPxActor().GetNxActor()->getLinearVelocity()) * _desc.speed,
							NX_SMOOTH_VELOCITY_CHANGE);
					this->Death();
				}
			}
			else
			{
				AddContactForce(target, contact,
				                _desc.mass * D3DXVECTOR3(this->GetPxActor().GetNxActor()->getLinearVelocity().get()),
				                NX_IMPULSE);
				const MapObjRec* record = target->GetMapObj() ? target->GetMapObj()->GetRecord() : nullptr;
				const RecordNode* recordParent = record ? record->GetParent() : nullptr;
				if (record && recordParent && recordParent->GetName() == "Crush")
				{
					if (record->GetName() == "dinamo")
					{
						target->SetMaxTimeLife(0.1f);
					}
				}
			}
		}

		void Proj::ForwardPointUpdate(float deltaTime)
		{
			if (_time1 >= 0)
			{
				_time1 += deltaTime;
			}

			//Мина подстраивается под высоту трассы:
			constexpr float cTrackHeight = 4.0f;

			const D3DXVECTOR3 size = _pxBox->GetDimensions();
			NxVec3 pos = GetPxActor().GetNxActor()->getGlobalPosition();
			const NxRay nxRay(pos + NxVec3(0, 0, cTrackHeight), NxVec3(0, 0, -1.0f));

			NxRaycastHit hit;
			NxShape* hitShape = GetLogic()->GetPxScene()->GetNxScene()->raycastClosestShape(
				nxRay, NX_STATIC_SHAPES, hit, 1 << px::Scene::cdgTrackPlane, NX_MAX_F32,
				NX_RAYCAST_SHAPE | NX_RAYCAST_IMPACT);

			if (hitShape)
			{
				GetGameObjFromShape(hitShape);
				hitShape->getGroup();

				const float height = std::max(pos.z - hit.worldImpact.z, size.z);
				if (_vec1.z == 0.0f)
					_vec1.z = height;
				else if (_vec1.z - height > 0.1f)
					_vec1.z = height;

				pos.z = hit.worldImpact.z + _vec1.z;
				GetPxActor().GetNxActor()->setGlobalPosition(pos);
			}

			if (GetLiveState() != lsDeath)
			{
				if (_model)
				{
					//ротация спутников:
					SetWorldRot(this->GetWorldRot());
					D3DXQUATERNION rot;
					D3DXQuaternionRotationAxis(&rot, &TSNVector, 5 * deltaTime);
					this->SetRot(this->GetRot() * rot);
				}


				if (_weapon && _weapon->GetPxActor().GetNxActor() && GetPxActor().GetNxActor())
				{
					//задержка перед раскрытием
					if (_time1 >= (_desc.speedRelativeMin / 100) && _time1 < (_desc.angleSpeed / 100))
					{
						if (this->GetReverse())
							GetPxActor().GetNxActor()->setLinearVelocity(NxVec3((FPVector2 * 22) / 100));
						else
							GetPxActor().GetNxActor()->setLinearVelocity(NxVec3((FPVector * 25) / 100));
						this->GetGrActor().SetVisible(true);
					}
					//время раскрытия
					else if (_time1 >= (_desc.angleSpeed / 100))
					{
						GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(NullVector));
						this->SetMaxLife(88);
					}
					else if (_time1 < (_desc.speedRelativeMin / 100))
						this->GetGrActor().SetVisible(false);
				}
			}

			if (_weapon == nullptr)
				this->Death();
		}

		void Proj::LeftPointUpdate(float deltaTime)
		{
			if (_time1 >= 0)
			{
				_time1 += deltaTime;
			}

			constexpr float cTrackHeight = 4.0f;

			const D3DXVECTOR3 size = _pxBox->GetDimensions();
			NxVec3 pos = GetPxActor().GetNxActor()->getGlobalPosition();
			const NxRay nxRay(pos + NxVec3(0, 0, cTrackHeight), NxVec3(0, 0, -1.0f));

			NxRaycastHit hit;
			NxShape* hitShape = GetLogic()->GetPxScene()->GetNxScene()->raycastClosestShape(
				nxRay, NX_STATIC_SHAPES, hit, 1 << px::Scene::cdgTrackPlane, NX_MAX_F32,
				NX_RAYCAST_SHAPE | NX_RAYCAST_IMPACT);

			if (hitShape)
			{
				GetGameObjFromShape(hitShape);
				hitShape->getGroup();

				const float height = std::max(pos.z - hit.worldImpact.z, size.z);
				if (_vec1.z == 0.0f)
					_vec1.z = height;
				else if (_vec1.z - height > 0.1f)
					_vec1.z = height;

				pos.z = hit.worldImpact.z + _vec1.z;
				GetPxActor().GetNxActor()->setGlobalPosition(pos);
			}

			if (GetLiveState() != lsDeath)
			{
				if (_model)
				{
					//ротация спутников:
					SetWorldRot(this->GetWorldRot());
					D3DXQUATERNION rot;
					D3DXQuaternionRotationAxis(&rot, &TSNVector, 5 * deltaTime);
					this->SetRot(this->GetRot() * rot);
				}

				if (_weapon && _weapon->GetPxActor().GetNxActor() && GetPxActor().GetNxActor())
				{
					//задержка перед раскрытием
					if (_time1 >= (_desc.speedRelativeMin / 100) && _time1 < (_desc.angleSpeed / 100))
					{
						if (this->GetReverse())
							GetPxActor().GetNxActor()->setLinearVelocity(NxVec3((LPVector2 * 22) / 100));
						else
							GetPxActor().GetNxActor()->setLinearVelocity(NxVec3((LPVector * 25) / 100));
						this->GetGrActor().SetVisible(true);
					}
					//время раскрытия		
					else if (_time1 >= (_desc.angleSpeed / 100))
					{
						GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(NullVector));
						this->SetMaxLife(88);
					}
					else if (_time1 < (_desc.speedRelativeMin / 100))
						this->GetGrActor().SetVisible(false);
				}

				if (_weapon == nullptr)
					this->Death();
			}
		}

		void Proj::RightPointUpdate(float deltaTime)
		{
			if (_time1 >= 0)
			{
				_time1 += deltaTime;
			}

			constexpr float cTrackHeight = 4.0f;

			const D3DXVECTOR3 size = _pxBox->GetDimensions();
			NxVec3 pos = GetPxActor().GetNxActor()->getGlobalPosition();
			const NxRay nxRay(pos + NxVec3(0, 0, cTrackHeight), NxVec3(0, 0, -1.0f));

			NxRaycastHit hit;
			NxShape* hitShape = GetLogic()->GetPxScene()->GetNxScene()->raycastClosestShape(
				nxRay, NX_STATIC_SHAPES, hit, 1 << px::Scene::cdgTrackPlane, NX_MAX_F32,
				NX_RAYCAST_SHAPE | NX_RAYCAST_IMPACT);

			if (hitShape)
			{
				GetGameObjFromShape(hitShape);
				hitShape->getGroup();

				const float height = std::max(pos.z - hit.worldImpact.z, size.z);
				if (_vec1.z == 0.0f)
					_vec1.z = height;
				else if (_vec1.z - height > 0.1f)
					_vec1.z = height;

				pos.z = hit.worldImpact.z + _vec1.z;
				GetPxActor().GetNxActor()->setGlobalPosition(pos);
			}

			if (GetLiveState() != lsDeath)
			{
				if (_model)
				{
					//ротация спутников:
					SetWorldRot(this->GetWorldRot());
					D3DXQUATERNION rot;
					D3DXQuaternionRotationAxis(&rot, &TSNVector, 5 * deltaTime);
					this->SetRot(this->GetRot() * rot);
				}

				if (_weapon && _weapon->GetPxActor().GetNxActor() && GetPxActor().GetNxActor())
				{
					//задержка перед раскрытием
					if (_time1 >= (_desc.speedRelativeMin / 100) && _time1 < (_desc.angleSpeed / 100))
					{
						if (this->GetReverse())
							GetPxActor().GetNxActor()->setLinearVelocity(NxVec3((RPVector2 * 22) / 100));
						else
							GetPxActor().GetNxActor()->setLinearVelocity(NxVec3((RPVector * 25) / 100));
						this->GetGrActor().SetVisible(true);
					}
					//время раскрытия		
					else if (_time1 >= (_desc.angleSpeed / 100))
					{
						GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(NullVector));
						this->SetMaxLife(88);
					}
					else if (_time1 < (_desc.speedRelativeMin / 100))
						this->GetGrActor().SetVisible(false);
				}

				if (_weapon == nullptr)
					this->Death();
			}
		}

		bool Proj::LaserMinePrepare(const ShotContext& ctx)
		{
			if (_weapon && _weapon->GetParent() != nullptr)
			{
				Player* plr = _weapon->GetParent()->IsCar()->GetMapObj()->GetPlayer();
				Race* race = plr->GetRace();

				//модель мины и цвет лазера зависят от цвета машины родителя:
				if (plr->GetColor() == clrWhite)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_White"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5White"));
					InitModel();
					InitModel2();
					InitModel3();
					//_model->GetGameObj().GetMapObj()->
				}
				else if (plr->GetColor() == clrBlue)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Blue"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Blue"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrSea)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Sea"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Sea"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrGay)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Gay"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Gay"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrGuy)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Guy"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Guy"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrTurquoise)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Turquoise"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Turquoise"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrEasyViolet)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_EasyViolet"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5EasyViolet"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrBrightBrown)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_BrightBrown"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5BrightBrown"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrOrange)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Orange"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Orange"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrPeach)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Peach"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Peach"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrBrightYellow)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_BrightYellow"));
					_desc.SetModel3(
						race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5BrightYellow"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrKhaki)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Khaki"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Khaki"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrKargo)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Kargo"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Kargo"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrGray)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Gray"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Gray"));
					//this->GetMapObj()->GetGameObj().get
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrYellow)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Yellow"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Yellow"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrAcid)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Acid"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Acid"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrSalad)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Salad"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Salad"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrBirch)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Birch"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Birch"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrBrightGreen)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_BrightGreen"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5BrightGreen"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrGreen)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Green"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Green"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrNeo)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Neo"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Neo"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrBrown)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Brown"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Brown"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrCherry)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Cherry"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Cherry"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrRed)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Red"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Red"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrPink)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Pink"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Pink"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrBarbie)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Barbie"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Barbie"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrViolet)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Violet"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5Violet"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrDarkGray)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_DarkGray"));
					_desc.SetModel3(race->GetDB()->GetRecord(MapObjLib::ctDecoration, "Misc\\mine5\\mine5DarkGray"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrBoss)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Boss"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrRip)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Rip"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrShred)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Shred"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrKristy)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Kristy"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrBot1)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Bot1"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrBot2)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Bot2"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrBot3)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Bot3"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrBot4)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Bot4"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrBot5)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Bot5"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrBot6)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Bot6"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrBot7)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Bot7"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrBot8)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Bot8"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrBot9)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Bot9"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrBot10)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Bot10"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrBot11)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Bot11"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else if (plr->GetColor() == clrBot12)
				{
					_desc.SetModel(race->GetDB()->GetRecord(MapObjLib::ctEffects, "laserRay_Bot12"));
					InitModel();
					InitModel2();
					InitModel3();
				}
				else
				{
					InitModel();
					InitModel2();
					InitModel3();
				}

				CreatePxBox(px::Scene::cdgShotTrack);
				SetIgnoreContactProj(true);

				const AABB aabb = ComputeAABB(true);
				GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);

				//установка мины на трассу:
				if (ctx.projMat)
				{
					SetWorldPos(ctx.projMat->t.get());
					return true;
				}
				D3DXVECTOR3 rayPos = _desc.pos;
				_weapon->GetGrActor().LocalToWorldCoord(rayPos, rayPos);
				const NxRay nxRay(NxVec3(rayPos) + NxVec3(0, 0, 2.0f), NxVec3(-ZVector));

				NxRaycastHit hit;
				const NxShape* hitShape = GetLogic()->GetPxScene()->GetNxScene()->raycastClosestShape(
					nxRay, NX_STATIC_SHAPES, hit, (1 << px::Scene::cdgTrackPlane) | (1 << px::Scene::cdgShotTrack),
					NX_MAX_F32, NX_RAYCAST_SHAPE | NX_RAYCAST_IMPACT | NX_RAYCAST_NORMAL);

				if (hitShape && hitShape->getGroup() != px::Scene::cdgShotTrack)
				{
					const float offs = std::max(-aabb.min.z, 0.01f);
					const D3DXVECTOR3 normal = hit.worldNormal.get();

					SetWorldPos(D3DXVECTOR3(hit.worldImpact.get()) + ZVector * offs);
					SetWorldUp(normal);
					return true;
				}
				return false;
			}
			return false;
		}

		void Proj::LaserMineContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			if (target == nullptr)
				return;

			if (target)
			{
				//уничтожаем щитом лазерную мину:
				if (target->IsImmortal() || target->IsShell())
					this->Death();
				else
				{
					if (_desc.speedRelative == true)
						DamageTarget(target, contact.deltaTime * (_desc.damage * 10), dtNull);
					else
					{
						if (target->GetPxActor().GetNxActor()->getLinearVelocity().magnitude() > 8.0f)
							DamageTarget(target, _desc.damage / 8, dtLaser);
						else
							DamageTarget(target, contact.deltaTime * _desc.damage, dtNull);
					}
				}
				//лазерная мина активируется после контакта с целью:		
				if (_desc.speedRelative == true)
					this->SetMaxTimeLife(_desc.speedRelativeMin);
				else
				{
					if (GetMaxTimeLife() == 0)
						this->SetMaxTimeLife(_desc.speedRelativeMin);
				}
			}
		}

		GameObject* Proj::LaserMineUpdate(float deltaTime, bool distort)
		{
			if (_weapon && _weapon->GetParent() != nullptr)
			{
				const D3DXVECTOR3 shotPos = GetWorldPos();
				D3DXVECTOR3 shotDir = ZVector;
				float scaleLaser = _desc.maxDist;

				NxRaycastHit rayhit;
				NxShape* hitShape = GetLogic()->GetPxScene()->GetNxScene()->raycastClosestShape(
					NxRay(NxVec3(shotPos + _desc.sizeAddPx), NxVec3(shotDir)), NX_ALL_SHAPES, rayhit,
					(1 << px::Scene::cdgDefault) | (1 << px::Scene::cdgShotTransparency) | (1 <<
						px::Scene::cdgTrackPlane), _desc.maxDist, NX_RAYCAST_SHAPE | NX_RAYCAST_DISTANCE);
				GameObject* rayHitActor = hitShape ? GetGameObjFromShape(hitShape) : nullptr;

				//разворот лазера по вертикали:
				if (this->GetMaxLife() != 33)
				{
					D3DXQUATERNION rot;
					D3DXQuaternionRotationAxis(&rot, &YVector, _desc.angleSpeed);
					_model->GetGameObj().SetRot(this->GetRot() * rot);
					_model2->GetGameObj().SetRot(this->GetRot() * rot);
					this->SetMaxLife(33);
				}

				if (GetTimeLife() > 0.1f)
				{
					if (rayHitActor)
					{
						scaleLaser = std::min(rayhit.distance, _desc.maxDist);
						const D3DXVECTOR3 hitPos = shotPos + shotDir * scaleLaser;
						if (_model2)
							_model2->GetGameObj().SetWorldPos(hitPos);
					}
					else if (_model2)
						_model2->GetGameObj().SetPos(_model3->GetGameObj().GetPos());

					if (_model)
					{
						const auto sprite = dynamic_cast<graph::Sprite*>(&_model->GetGameObj().GetGrActor().GetNodes().
						                                                         front());
						//ширина лазера == desc.size.y 
						auto size = D3DXVECTOR2(scaleLaser, _desc.size.y / 3);

						if (distort)
						{
							float alpha = ClampValue(GetTimeLife() / GetMaxTimeLife(), 0.0f, 1.0f);
							alpha = ClampValue((alpha - 0.0f) / 0.5f * 1.5f + 0.5f, 0.0f, 2.0f) - ClampValue(
								(alpha - 0.6f) / 0.4f * 2.0f, 0.0f, 2.0f);

							size.y = size.y * alpha;
							sprite->material.Get()->samplers[0].SetScale(D3DXVECTOR3(scaleLaser / 10.0f, 1.0f, 1.0f));
						}

						sprite->SetPos(XVector * scaleLaser / 2.0f);
						sprite->sizes = size;
					}
				}
				return rayHitActor;
			}
			//если нет родителя, то уничтожаем proj:
			this->Death();
			return nullptr;
		}

		void Proj::AspirineContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			if (target == nullptr)
				return;

			GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			               GetNxActor()
				               ? target->GetParent()->IsCar()
				               : target->IsCar();

			if (car)
			{
				//максимальный запас жизней:
				const float maxLife = car->GetMaxLife();
				//текущий запас жизней:
				const float carlife = car->GetLife();
				//добавка от аспирина:
				const float value = (car->GetMaxLife() / 100) * _desc.damage;

				//если меньше 70% жизней то добавляем 30% (значение указано в воркшопе)
				if (carlife < maxLife * 0.7f)
				{
					car->SetLife(carlife + value); //value = 30%;
				}
				else
				{
					car->SetLife(maxLife);
				}

				this->Death();
			}
		}

		bool Proj::HyperFlyPrepare(GameObject* weapon)
		{
			if (weapon->GetParent()->IsCar()->IsWheelsContact() == false)
			{
				D3DXVECTOR3 speed = CalcFlySpeed(weapon);

				LocateProj(weapon, true, true, &speed);
				CreatePxBox(px::Scene::cdgShot);

				NxBodyDesc bodyDesc;
				bodyDesc.flags = NX_BF_DISABLE_GRAVITY;
				bodyDesc.linearVelocity = NxVec3(speed);
				CreateBody(bodyDesc);

				this->SetMaxTimeLife(_desc.damage);
				weapon->GetParent()->IsCar()->InFly(true);
				//_weapon->GetParent()->SetRot(this->GetRot()); - дополнительное выравнивание. А нужно ли оно вообще?
			}
			return true;
		}

		void Proj::HyperFlyUpdate(float deltaTime)
		{
			if (GetLiveState() != lsDeath)
			{
				if (_weapon && _weapon->GetParent() != nullptr)
				{
					if (_weapon->GetParent()->IsCar()->InFly())
					{
						D3DXVECTOR3 speed = CalcFlySpeed(_weapon);
						//скорость во время воздушного тормоза:
						if (_weapon->GetParent()->IsCar()->GetMoveCar() == _weapon->GetParent()->IsCar()->mcBrake)
							speed = BrakeFlySpeed(_weapon);

						_weapon->GetParent()->GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(speed));

						_weapon->GetParent()->GetPxActor().GetNxActor()->setAngularMomentum(NxVec3(NullVector));
						//заморозка вращения машины во время полёта (способ 1)
						_weapon->GetParent()->GetPxActor().GetNxActor()->setAngularVelocity(NxVec3(NullVector));
						//заморозка вращения машины во время полёта (способ 2)
						//_weapon->GetParent()->SetRot(this->GetRot()); //идеальное выравнивание машины, но повороты не возможны!!!!
						//_weapon->GetParent()->SetWorldRot(this->GetRot());  //также идеальное выравнивание машины, но повороты не возможны!!!!
					}
				}
				else
				{
					this->Death();
				}
			}
		}

		bool Proj::BonusPrepare(GameObject* weapon)
		{
			LocateProj(weapon, true, true, nullptr);
			InitModel();
			CreatePxBox();

			this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);

			return true;
		}

		void Proj::MedpackContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			               GetNxActor()
				               ? target->GetParent()->IsCar()
				               : target->IsCar();
			if (target == nullptr)
				return;

			if (car)
			{
				car->SetDamageStop(true);
				car->SetDamageStop(false);
				car->SetBurn(false);
				car->SetBurnDamage(0);
				car->GetMapObj()->GetPlayer()->SetMaxBurnTime(0);

				GetLogic()->TakeBonus(target, this, btMedpack, _desc.damage > 0 ? _desc.damage : target->GetMaxLife(),
				                      true);
			}
		}

		void Proj::MoneyContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			if (target)
			{
				GetLogic()->TakeBonus(target, this, btMoney, _desc.damage, true);
			}
		}

		void Proj::ChargeContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			if (target)
			{
				GetLogic()->TakeBonus(target, this, btCharge, _desc.damage, true);
			}
		}

		void Proj::ImmortalContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			if (target)
			{
				if (target->IsCar())
				{
					if (target->IsCar()->InRage())
						this->Death();
					else
					{
						GetLogic()->TakeBonus(target, this, btImmortal, _desc.damage, true);
					}
				}
			}
		}

		bool Proj::SpeedArrowPrepare(GameObject* weapon)
		{
			LocateProj(weapon, true, true, nullptr);
			InitModel();
			CreatePxBox();

			this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);

			return true;
		}

		void Proj::SpeedArrowContact(const px::Scene::OnContactEvent& contact) const
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			if (target && target->IsCar())
			{
				target->GetPxActor().GetNxActor()->setLinearVelocity(NxVec3(GetGrActor().GetWorldDir() * _desc.damage));
				target->SendEvent(cPlayerSpeedArrow);
			}
		}

		bool Proj::LushaPrepare(const ShotContext& ctx)
		{
			LocateProj(_weapon, true, true, nullptr);
			InitModel();
			CreatePxBox();

			this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);

			return true;
		}

		void Proj::LushaContact(const px::Scene::OnContactEvent& contact) const
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			if (target && target->IsCar())
			{
				NxActor* nxTarget = target->GetPxActor().GetNxActor();

				NxVec3 linSpeed = nxTarget->getLinearVelocity();
				const float maxSpeed = linSpeed.magnitude();
				linSpeed.normalize();

				if (maxSpeed > 1.0f && maxSpeed > _desc.damage)
				{
					nxTarget->setLinearVelocity(linSpeed * _desc.damage);
				}
			}
		}

		bool Proj::SpinnerPrepare(const ShotContext& ctx)
		{
			LocateProj(_weapon, true, true, nullptr);
			InitModel();
			CreatePxBox();

			this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);

			return true;
		}

		void Proj::SpinnerContact(const px::Scene::OnContactEvent& contact) const
		{
			GameObject* target = GetGameObjFromActor(contact.actor);

			if (target == nullptr)
				return;

			GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			               GetNxActor()
				               ? target->GetParent()->IsCar()
				               : target->IsCar();
			//на новичке боты не стреляют, если наехали на лужу:
			if (car->GetMapObj()->GetPlayer()->IsComputer() && GAME_DIFF == 0)
				car->GetMapObj()->GetPlayer()->SetShotFreeze(true);

			if (target && target->IsCar())
			{
				const NxActor* nxTarget = target->GetPxActor().GetNxActor();
				//контакт будет засчитан, если машина не слишком медленно едет:
				if (nxTarget->getLinearVelocity().magnitude() > 32.0f)
				{
					UnlimitedTurn = true;
					car->SetSpinStatus(true);
					car->SetRespBlock(true);
					//target->SendEvent(cPlayerSpinOut); //отправляем событие для озвучки коментатором.
				}
				else
				{
					//mcAccel
					if (car->GetSpeed() > 4)
					{
						car->LockClutch(
							RandomRange(-2.8f, 2.8f) + car->GetPxActor().GetNxActor()->getLinearVelocity().magnitude() *
							0.1f);
						car->GetPxActor().GetNxActor()->addLocalForce(NxVec3(car->GetGrActor().GetRight() * 18.0f),
						                                              NX_ACCELERATION);
						car->SetRespBlock(true);
					}
					//mcBack
					if (car->GetMoveCar() == 2)
					{
						car->SetMoveCar(car->mcBack);
						car->LockClutch(RandomRange(-1.6f, 1.6f));
						car->GetPxActor().GetNxActor()->addLocalForce(NxVec3(car->GetGrActor().GetRight() * -12.0f),
						                                              NX_ACCELERATION);
						car->SetRespBlock(true);
					}
				}
			}
		}

		void Proj::SandContact(const px::Scene::OnContactEvent& contact) const
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			               GetNxActor()
				               ? target->GetParent()->IsCar()
				               : target->IsCar();
			Player* plr = car->GetMapObj()->GetPlayer();
			target->GetPxActor().GetNxActor();

			if (target && target->IsCar())
			{
				car->GetSpeed();
				car->SetMaxTimeSR(1.5f);
				car->SetSlowRide(true);
				plr->ResetMaxSpeed();
				car->SetMaxSpeed(car->GetMaxSpeed() * 0.4f);
			}
		}

		void Proj::LavaContact(const px::Scene::OnContactEvent& contact) const
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			               GetNxActor()
				               ? target->GetParent()->IsCar()
				               : target->IsCar();
			Player* plr = car->GetMapObj()->GetPlayer();

			if (car)
			{
				if (car->IsShell() || car->IsImmortal())
					return;

				if (car->GetPxActor().GetNxActor()->getLinearVelocity().magnitude() > 8.0f)
				{
					//урон от лавы
					DamageTarget(target, contact.deltaTime * 100.0f, dtDeathPlane);
					//замедление
					car->SetMaxTimeSR(3.0f);
					car->SetSlowRide(true);
					plr->ResetMaxSpeed();
					car->SetMaxSpeed(car->GetMaxSpeed() * 0.7f);
					//поджог:
					car->SetBurn(true);
					car->SetBurnDamage(50.0f);
					car->GetMapObj()->GetPlayer()->SetMaxBurnTime(-1.5f);
				}
			}
		}

		bool Proj::FireBoostPrepare(const ShotContext& ctx)
		{
			LocateProj(_weapon, true, true, nullptr);
			InitModel();
			InitModel2();
			_model2->GetGameObj().GetGrActor().SetVisible(false);
			CreatePxBox();

			this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);

			return true;
		}

		void Proj::FireBoostContact(const px::Scene::OnContactEvent& contact) const
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			if (target == nullptr)
			{
				_model2->GetGameObj().GetGrActor().SetVisible(false);
				return;
			}

			GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			               GetNxActor()
				               ? target->GetParent()->IsCar()
				               : target->IsCar();
			Player* plr = car->GetMapObj()->GetPlayer();

			if (car)
			{
				//InitModel2();
				_model2->GetGameObj().GetGrActor().SetVisible(true);
				if (car->IsShell() || car->IsImmortal())
					return;

				if (car->GetPxActor().GetNxActor()->getLinearVelocity().magnitude() > 5.0f)
				{
					//урон от огня
					DamageTarget(target, contact.deltaTime * 100.0f, dtDeathPlane);
					//замедление
					car->SetMaxTimeSR(3.0f);
					car->SetSlowRide(true);
					plr->ResetMaxSpeed();
					car->SetMaxSpeed(car->GetMaxSpeed() * 0.7f);
				}
			}
		}

		void Proj::DangerContact(const px::Scene::OnContactEvent& contact) const
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			const GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			                                                                                    GetNxActor()
				                     ? target->GetParent()->IsCar()
				                     : target->IsCar();
			if (target == nullptr)
				return;

			if (car)
			{
				car->GetWheels().front().SetParent(nullptr);
				car->GetWheels().back().SetParent(nullptr);
			}
		}

		bool Proj::BaloonPrepare(const ShotContext& ctx)
		{
			LocateProj(_weapon, true, true, nullptr);
			InitModel();
			CreatePxBox(px::Scene::cdgDefault);
			NxBodyDesc bodyDesc;
			bodyDesc.flags = NX_BF_DISABLE_GRAVITY;
			CreateBody(bodyDesc);

			GetPxActor().SetContactReportFlags(NX_NOTIFY_ALL);
			this->SetMaxTimeLife(20.0f);
			return true;
		}

		bool Proj::RockPrepare(const ShotContext& ctx)
		{
			LocateProj(_weapon, true, true, nullptr);
			InitModel();
			CreatePxBox(px::Scene::cdgDefault);
			GetPxActor().SetContactReportFlags(NX_NOTIFY_ALL);
			return true;
		}

		void Proj::PlatformContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			const GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			                                                                                    GetNxActor()
				                     ? target->GetParent()->IsCar()
				                     : target->IsCar();
			if (target == nullptr)
				return;

			if (car)
			{
				this->SetMaxTimeLife(0.2f);
			}
		}

		void Proj::GravitonContact(const px::Scene::OnContactEvent& contact) const
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			               GetNxActor()
				               ? target->GetParent()->IsCar()
				               : target->IsCar();

			if (target == nullptr)
				return;

			if (car)
			{
				if (car->GetMoveCar() == car->mcBack || car->GetLastMoveState() == 2)
					target->GetPxActor().GetNxActor()->addForce(NxVec3(ZVector * 2), NX_SMOOTH_VELOCITY_CHANGE);
				else
				{
					if (car->GetPxActor().GetNxActor()->getLinearVelocity().magnitude() > 34.0f)
					{
						target->GetPxActor().GetNxActor()->addForce(NxVec3(ZVector * 2), NX_SMOOTH_VELOCITY_CHANGE);
					}
					else
					{
						target->GetPxActor().GetNxActor()->addForce(NxVec3(ZVector * (car->GetSpeed() / 4)),
						                                            NX_SMOOTH_VELOCITY_CHANGE);
					}
				}
			}
		}

		bool Proj::SapperPrepare(GameObject* weapon)
		{
			LinkToWeapon();
			GameCar* car = weapon->GetParent()->IsCar();
			if (car)
			{
				car->SetDemining(true);
				car->SetDeminingTime(_desc.angleSpeed);
				return true;
			}
			return false;
		}

		bool Proj::TriggerPrepare(const ShotContext& ctx)
		{
			LocateProj(_weapon, true, true, nullptr);
			InitModel();
			CreatePxBox();

			this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);
			//триггеры видны только в режиме разработчика и в редакторе трасс.
			//if (GetLogic()->GetRace()->GetDevMode() == false)
			//this->GetGrActor().SetVisible(false);

			if (EDIT_MODE == false)
				this->GetGrActor().SetVisible(false);

			return true;
		}

		void Proj::IsoContact(const px::Scene::OnContactEvent& contact) const
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			const GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			                                                                                    GetNxActor()
				                     ? target->GetParent()->IsCar()
				                     : target->IsCar();

			if (target == nullptr || car == nullptr)
				return;

			if (car && car->GetMapObj()->GetPlayer()->isSubject())
				car->GetMapObj()->GetPlayer()->SetCameraStatus(2);
		}

		void Proj::ThirdContact(const px::Scene::OnContactEvent& contact) const
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			const GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			                                                                                    GetNxActor()
				                     ? target->GetParent()->IsCar()
				                     : target->IsCar();

			if (target == nullptr || car == nullptr)
				return;

			if (car && car->GetMapObj()->GetPlayer()->isSubject())
				car->GetMapObj()->GetPlayer()->SetCameraStatus(1);
		}

		void Proj::FreeContact(const px::Scene::OnContactEvent& contact) const
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			const GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			                                                                                    GetNxActor()
				                     ? target->GetParent()->IsCar()
				                     : target->IsCar();

			if (target == nullptr || car == nullptr)
				return;

			if (car && car->GetMapObj()->GetPlayer()->isSubject())
				car->GetMapObj()->GetPlayer()->SetCameraStatus(0);
		}

		void Proj::AutoCamOffContact(const px::Scene::OnContactEvent& contact) const
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			const GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			                                                                                    GetNxActor()
				                     ? target->GetParent()->IsCar()
				                     : target->IsCar();

			if (target == nullptr || car == nullptr)
				return;

			if (car->GetMapObj()->GetPlayer()->isSubject())
				car->GetMapObj()->GetPlayer()->SetCameraStatus(3);
		}

		void Proj::AutoCamOnContact(const px::Scene::OnContactEvent& contact) const
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			const GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			                                                                                    GetNxActor()
				                     ? target->GetParent()->IsCar()
				                     : target->IsCar();

			if (target == nullptr || car == nullptr)
				return;

			if (car->GetMapObj()->GetPlayer()->isSubject())
				car->GetMapObj()->GetPlayer()->SetCameraStatus(4);
		}

		void Proj::AltFinishContact(const px::Scene::OnContactEvent& contact) const
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			const GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			                                                                                    GetNxActor()
				                     ? target->GetParent()->IsCar()
				                     : target->IsCar();

			if (target == nullptr || car == nullptr)
				return;

			if (car && car->GetMapObj()->GetPlayer()->GetFinished() == false)
			{
				car->GetMapObj()->GetPlayer()->QuickFinish(car->GetMapObj()->GetPlayer());
				car->GetMapObj()->GetPlayer()->SetFinished(true);
			}
		}

		void Proj::HeadLightOffContact(const px::Scene::OnContactEvent& contact) const
		{
			const GameObject* target = GetGameObjFromActor(contact.actor);
			if (target == nullptr)
				return;

			if (target->GetMapObj()->GetPlayer())
				target->GetMapObj()->GetPlayer()->SetHeadlight(Player::hlmNone);
		}

		void Proj::HeadLightOnContact(const px::Scene::OnContactEvent& contact) const
		{
			const GameObject* target = GetGameObjFromActor(contact.actor);

			if (target == nullptr)
				return;

			if (target->GetMapObj()->GetPlayer()->IsComputer())
				target->GetMapObj()->GetPlayer()->SetHeadlight(Player::hlmOne);
			else if (target->GetMapObj()->GetPlayer()->IsHuman())
				target->GetMapObj()->GetPlayer()->SetHeadlight(Player::hlmTwo);
		}

		void Proj::InRampContact(const px::Scene::OnContactEvent& contact) const
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			const GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			                                                                                    GetNxActor()
				                     ? target->GetParent()->IsCar()
				                     : target->IsCar();

			if (target == nullptr || car == nullptr)
				return;

			if (car && car->OnJump())
			{
				car->GetMapObj()->GetPlayer()->inRamp(true);
			}
		}

		void Proj::AiJumpContact(const px::Scene::OnContactEvent& contact) const
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			               GetNxActor()
				               ? target->GetParent()->IsCar()
				               : target->IsCar();

			if (target == nullptr || car == nullptr)
				return;

			//если скорость меньше максимальной.
			if (car->GetSpeed() < car->GetMaxSpeed())
			{
				car->GoAiJump(true);
			}
		}

		void Proj::RageContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			const GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			                                                                                    GetNxActor()
				                     ? target->GetParent()->IsCar()
				                     : target->IsCar();
			if (target == nullptr)
				return;

			if (car)
			{
				if (car->IsImmortal())
					this->Death();
				else
				{
					car->GetMapObj()->GetPlayer()->SetRageTime(0);
					GetLogic()->TakeBonus(target, this, btRage, _desc.damage, true);
				}
			}
		}

		void Proj::LuckyContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			               GetNxActor()
				               ? target->GetParent()->IsCar()
				               : target->IsCar();
			if (target == nullptr)
				return;

			if (car)
			{
				GetLogic()->TakeBonus(target, this, btLucky, _desc.damage, true);
				const float HP = car->GetLife() * 100 / car->GetMaxLife();

				//деньги
				if (car->IsShell() || car->InRage() || car->IsImmortal())
				{
					DamageTarget(car, 0, dtMoney);
					car->GetMapObj()->GetPlayer()->AddMoney(1000);
					this->Death();
				}

				//аспирин, аптека:
				if (HP < 10)
				{
					DamageTarget(car, 0, dtMedpack);
					car->SetLife(car->GetLife() * 2);
					this->Death();
				}
				else
				{
					if (HP < 90)
					{
						DamageTarget(car, 0, dtMedpack);
						car->SetLife(car->GetMaxLife());
						this->Death();
					}
				}

				//щит, мины:		
				if (car->GetMapObj()->GetPlayer()->GetPlace() == 1)
				{
					if (car->GetMapObj()->GetPlayer()->GetSlotInst(Player::stMine)->GetItem().IsWeaponItem()->
					         GetCurCharge() == 0)
					{
						DamageTarget(car, 0, dtReload);
						this->Death();
						car->GetMapObj()->GetPlayer()->GetSlotInst(Player::stMine)->GetItem().IsWeaponItem()->
						     SetCurCharge(
							     car->GetMapObj()->GetPlayer()->GetSlotInst(Player::stMine)->GetItem().IsWeaponItem()->
							          GetCntCharge());
					}
					else
					{
						DamageTarget(car, 0, dtShieldOn);
						car->Immortal(10);
						this->Death();
					}
				}

				//рейдж, патроны:
				if (car->GetMapObj()->GetPlayer()->GetPlace() == TOTALPLAYERS_COUNT - SPECTATORS_COUNT)
				{
					if (car->GetMapObj()->GetPlayer()->GetSlotInst(Player::stWeapon1)->GetItem().IsWeaponItem()->
					         GetCurCharge() == 0)
					{
						DamageTarget(car, 0, dtReload);
						this->Death();
						car->GetMapObj()->GetPlayer()->GetSlotInst(Player::stWeapon1)->GetItem().IsWeaponItem()->
						     SetCurCharge(
							     car->GetMapObj()->GetPlayer()->GetSlotInst(Player::stWeapon1)->GetItem().IsWeaponItem()
							        ->GetCntCharge());
					}
					else
					{
						DamageTarget(car, 0, dtRage);
						car->InRage(true);
						this->Death();
					}
				}
			}
		}

		void Proj::UnLuckyContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			const GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			                                                                                    GetNxActor()
				                     ? target->GetParent()->IsCar()
				                     : target->IsCar();
			if (target == nullptr)
				return;

			GetLogic()->TakeBonus(target, this, btUnLucky, _desc.damage, true);

			if (car)
			{
				//замедление, занос
				if (car->GetMapObj()->GetPlayer()->GetPlace() == 1)
				{
					MasloContact(contact);
					this->Death();
				}
				else
				{
					SpikesContact(contact);
					this->Death();
				}
			}
		}

		bool Proj::RJPrepare(GameObject* weapon)
		{
			//только если прыжки ботов доступны на этой трассе.
			if (weapon && weapon->GetParent()->GetMapObj()->GetPlayer()->GetRace()->GetTournament().GetCurTrack().
			                      AiJumpEnabled() == false)
				return false;

			_vec1.z = 0.0f;
			D3DXVECTOR3 speed = CalcSpeed(weapon);

			LocateProj(weapon, true, true, &speed);
			//InitModel();
			CreatePxBox(px::Scene::cdgShot);

			NxBodyDesc bodyDesc;
			bodyDesc.flags = NX_BF_DISABLE_GRAVITY;
			bodyDesc.linearVelocity = NxVec3(speed);
			CreateBody(bodyDesc);

			this->GetPxActor().SetContactReportFlags(NX_NOTIFY_ALL);
			this->GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);
			SetIgnoreContactProj(true);
			this->GetGrActor().showBB = true;
			this->GetGrActor().colorBB = clrRed;

			return true;
		}

		void Proj::RJContact(const px::Scene::OnContactEvent& contact)
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			if (target == nullptr)
				return;

			if (target)
			{
				GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
				               GetNxActor()
					               ? target->GetParent()->IsCar()
					               : target->IsCar();
				//прыжок боту не нужен, если он использует щит.
				if (car != nullptr && !car->IsImmortal())
				{
					if (!car->LockAIJump() || car->GetSpeed() < 10)
					{
						car->GoAiJump(true);
					}
				}
				this->Death();
			}
		}

		void Proj::AiLockJumpContact(const px::Scene::OnContactEvent& contact) const
		{
			GameObject* target = GetGameObjFromActor(contact.actor);
			GameCar* car = target->GetParent() && target->GetParent()->IsCar() && target->GetParent()->GetPxActor().
			               GetNxActor()
				               ? target->GetParent()->IsCar()
				               : target->IsCar();

			if (target == nullptr || car == nullptr)
				return;

			if (car->GetMapObj()->GetPlayer()->IsComputer())
			{
				car->LockAIJump(true);
				car->OnJump(true);
			}
		}

		////////////////////////////////////////////////////////////////////////
		void Proj::OnDestroy(GameObject* sender)
		{
			_MyBase::OnDestroy(sender);

			if (_model && sender->GetMapObj() == _model)
				SafeRelease(_model);

			if (_model2 && sender->GetMapObj() == _model2)
				SafeRelease(_model2);

			if (_model3 && sender->GetMapObj() == _model3)
				SafeRelease(_model3);

			if (_weapon && _weapon == sender)
			{
				if (GetParent() == _weapon)
					Death();
				SetWeapon(nullptr);
			}

			if (_shot.GetTargetMapObj() && sender == &_shot.GetTargetMapObj()->GetGameObj())
				_shot.SetTargetMapObj(nullptr);
		}

		void Proj::OnContact(const px::Scene::OnContactEvent& contact)
		{
			_MyBase::OnContact(contact);

			const GameObject* target = GetGameObjFromActor(contact.actor);

			if (GetLiveState() != lsDeath && !(target && target->GetLiveState() == lsDeath))
			{
				switch (_desc.type)
				{
				case ptRocket:
					RocketContact(contact);
					break;

				case ptRipper:
					RipperContact(contact);
					break;

				case ptBlaster:
					BlasterContact(contact);
					break;

				case ptAirWeapon:
					RocketContact(contact);
					break;

				case ptRezonator:
					RezonatorContact(contact);
					break;

				case ptSonar:
					SonarContact(contact);
					break;

				case ptMiniGun:
					RocketContact(contact);
					break;

				case ptShotGun:
					RocketContact(contact);
					break;

				case ptTrinity:
					RocketContact(contact);
					break;

				case ptMortira:
					MortiraContact(contact);
					break;

				case ptMolotov:
					MolotovContact(contact);
					break;

				case ptCrater:
					CraterContact(contact);
					break;

				case ptArtillery:
					ArtilleryContact(contact);
					break;

				case ptGrenade:
					GrenadeContact(contact);
					break;

				case ptTorpeda:
					FixRocketContact(contact);
					break;

				case ptImpulse:
					ImpulseContact(contact);
					break;

				case ptSphereGun:
					FixRocketContact(contact);
					break;

				case ptPlazma:
					PlazmaContact(contact);
					break;

				case ptBlazer:
					BlazerContact(contact);
					break;

				case ptPhoenix:
					PhoenixContact(contact);
					break;

				case ptMagnetic:
					MagneticContact(contact);
					break;

				case ptDisc:
					DiscContact(contact);
					break;

				case ptShocker:
					ShockerContact(contact);
					break;

				case ptMiniMine:
					MineContact(contact, false);
					break;

				case ptMine:
					MineContact(contact, true);
					break;

				case ptMineRip:
					MineContact(contact, true);
					break;

				case ptMineProton:
					MineContact(contact, false);
					break;

				case ptMaslo:
					MasloContact(contact);
					break;

				case ptAcid:
					AcidContact(contact);
					break;

				case ptFire:
					FireContact(contact);
					break;

				case ptSpikes:
					SpikesContact(contact);
					break;

				case ptSmokes:
					SmokesContact(contact);
					break;

				case ptBurnMine:
					BurnMineContact(contact);
					break;

				case ptBoom:
					BoomContact(contact);
					break;

				case ptCore:
					CoreContact(contact);
					break;

				case ptForwardPoint:
					ScatterContact(contact);
					break;

				case ptLeftPoint:
					ScatterContact(contact);
					break;

				case ptRightPoint:
					ScatterContact(contact);
					break;

				case ptLaserMine:
					LaserMineContact(contact);
					break;

				case ptAspirine:
					AspirineContact(contact);
					break;

				case ptMedpack:
					MedpackContact(contact);
					break;

				case ptMoney:
					MoneyContact(contact);
					break;

				case ptCharge:
					ChargeContact(contact);
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

				case ptSpinner:
					SpinnerContact(contact);
					break;

				case ptSand:
					SandContact(contact);
					break;

				case ptLava:
					LavaContact(contact);
					break;

				case ptFireBoost:
					FireBoostContact(contact);
					break;

				case ptDanger:
					DangerContact(contact);
					break;

				case ptPlatform:
					PlatformContact(contact);
					break;

				case ptGraviton:
					GravitonContact(contact);
					break;

				case ptIsoCam:
					IsoContact(contact);
					break;

				case ptThirdCam:
					ThirdContact(contact);
					break;

				case ptCamFree:
					FreeContact(contact);
					break;

				case ptAutoCamOff:
					AutoCamOffContact(contact);
					break;

				case ptAutoCamOn:
					AutoCamOnContact(contact);
					break;

				case ptAltFinish:
					AltFinishContact(contact);
					break;

				case ptHeadLightOff:
					HeadLightOffContact(contact);
					break;

				case ptHeadLightOn:
					HeadLightOnContact(contact);
					break;

				case ptInRamp:
					InRampContact(contact);
					break;

				case ptAiJump:
					AiJumpContact(contact);
					break;

				case ptRage:
					RageContact(contact);
					break;

				case ptLucky:
					LuckyContact(contact);
					break;

				case ptUnLucky:
					UnLuckyContact(contact);
					break;

				case ptRocketJump:
					RJContact(contact);
					break;

				case ptAiLockJump:
					AiLockJumpContact(contact);
					break;
				}
			}
		}

		void Proj::SaveSource(SWriter* writer)
		{
			_MyBase::SaveSource(writer);

			SWriter* proj = writer->NewDummyNode("proj");
			_desc.SaveTo(proj, this);
		}

		void Proj::LoadSource(SReader* reader)
		{
			_MyBase::LoadSource(reader);

			SReader* proj = reader->ReadValue("proj");
			_desc.LoadFrom(proj, this);
		}

		void Proj::OnFixUp(const FixUpNames& fixUpNames)
		{
			_MyBase::OnFixUp(fixUpNames);

			for (const auto& fixUpName : fixUpNames)
			{
				if (fixUpName.sender->GetOwnerValue()->GetMyName() == "proj")
				{
					_desc.OnFixUp(fixUpName);
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

			case ptHyper:
				HyperUpdate(deltaTime);
				break;

			case ptSpring2:
				Spring2Update(deltaTime);
				break;

			case ptRipper:
				RipperUpdate(deltaTime);
				break;

			case ptBlaster:
				RocketUpdate(deltaTime);
				break;

			case ptAirWeapon:
				AirWeaponUpdate(deltaTime);
				break;

			case ptRezonator:
				RezonatorUpdate(deltaTime);
				break;

			case ptSonar:
				SonarUpdate(deltaTime);
				break;

			case ptMiniGun:
				MiniGunUpdate(deltaTime);
				break;

			case ptShotGun:
				ShotGunUpdate(deltaTime);
				break;

			case ptTrinity:
				TrinityUpdate(deltaTime);
				break;

			case ptLaser:
				LaserUpdate(deltaTime, true);
				break;

			case ptFrostRay:
				FrostRayUpdate(deltaTime);
				break;

			case ptNewFrostRay:
				NewFrostRayUpdate(deltaTime);
				break;

			case ptCrater:
				MineUpdate(deltaTime);
				break;

			case ptArtillery:
				ArtilleryUpdate(deltaTime);
				break;

			case ptGrenade:
				GrenadeUpdate(deltaTime);
				break;

			case ptTorpeda:
				TorpedaUpdate(deltaTime);
				break;

			case ptImpulse:
				SphereUpdate(deltaTime);
				break;

			case ptSphereGun:
				SphereUpdate(deltaTime);
				break;

			case ptPlazma:
				RocketUpdate(deltaTime);
				break;

			case ptBlazer:
				BlazerUpdate(deltaTime);
				break;

			case ptPhoenix:
				PhoenixUpdate(deltaTime);
				break;

			case ptMagnetic:
				MagneticUpdate(deltaTime);
				break;

			case ptDisc:
				DiscUpdate(deltaTime);
				break;

			case ptShocker:
				ShockerUpdate(deltaTime);
				break;

			case ptMine:
				MineUpdate(deltaTime);
				break;

			case ptMiniMine:
				MineUpdate(deltaTime);
				break;

			case ptMineRip:
				MineRipUpdate(deltaTime);
				break;

			case ptMineProton:
				MineUpdate(deltaTime);
				break;

			case ptMaslo:
				MasloUpdate(deltaTime);
				break;

			case ptAcid:
				MasloUpdate(deltaTime);
				break;

			case ptFire:
				FireUpdate(deltaTime);
				break;

			case ptSpikes:
				MineUpdate(deltaTime);
				break;

			case ptSmokes:
				MineUpdate(deltaTime);
				break;

			case ptBurnMine:
				MineUpdate(deltaTime);
				break;

			case ptCore:
				CoreUpdate(deltaTime);
				break;

			case ptForwardPoint:
				ForwardPointUpdate(deltaTime);
				break;

			case ptLeftPoint:
				LeftPointUpdate(deltaTime);
				break;

			case ptRightPoint:
				RightPointUpdate(deltaTime);
				break;

			case ptLaserMine:
				LaserMineUpdate(deltaTime, true);
				break;

			case ptAspirine:
				MineUpdate(deltaTime);
				break;

			case ptHyperFly:
				HyperFlyUpdate(deltaTime);
				break;

			case ptRocketJump:
				RocketUpdate(deltaTime);
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

			case ptSpring:
				res = SpringPrepare(weapon);
				break;

			case ptSpring2:
				res = Spring2Prepare(weapon);
				break;

			case ptRipper:
				res = RipperPrepare(weapon);
				break;

			case ptShell:
				res = ShellPrepare(weapon);
				break;

			case ptBlaster:
				res = RocketPrepare(weapon);
				break;

			case ptAirWeapon:
				res = AirWeaponPrepare(weapon);
				break;

			case ptRezonator:
				res = RezonatorPrepare(weapon);
				break;

			case ptSonar:
				res = SonarPrepare(weapon);
				break;

			case ptMiniGun:
				ShotGunPrepare(weapon);
				break;

			case ptShotGun:
				res = ShotGunPrepare(weapon);
				break;

			case ptTrinity:
				res = TrinityPrepare(weapon);
				break;

			case ptLaser:
				res = LaserPrepare(weapon);
				break;

			case ptFrostRay:
				res = FrostRayPrepare(weapon);
				break;

			case ptNewFrostRay:
				res = FrostRayPrepare(weapon);
				break;

			case ptMortira:
				res = MortiraPrepare(weapon);
				break;

			case ptMolotov:
				res = MolotovPrepare(weapon);
				break;

			case ptCrater:
				res = CraterPrepare(ctx);
				break;

			case ptArtillery:
				ArtilleryPrepare(ctx);
				break;

			case ptGrenade:
				GrenadePrepare(ctx);
				break;

			case ptTorpeda:
				res = TorpedaPrepare(weapon);
				break;

			case ptImpulse:
				res = ImpulsePrepare(weapon);
				break;

			case ptSphereGun:
				res = ImpulsePrepare(weapon);
				break;

			case ptPlazma:
				res = RocketPrepare(weapon);
				break;

			case ptBlazer:
				res = RocketPrepare(weapon);
				break;

			case ptPhoenix:
				RocketPrepare(weapon, true);
				break;

			case ptMagnetic:
				MagneticPrepare(weapon);
				break;

			case ptDisc:
				DiscPrepare(weapon);
				break;

			case ptShocker:
				res = ShockerPrepare(weapon);
				break;

			case ptMine:
				res = MinePrepare(ctx, true);
				break;

			case ptMiniMine:
				res = MinePrepare(ctx, true);
				break;

			case ptMineRip:
				res = MinePrepare(ctx, true);
				break;

			case ptMinePiece:
				res = MinePiecePrepare(ctx);
				break;

			case ptMineProton:
				res = MinePrepare(ctx, true);
				break;

			case ptMaslo:
				res = MasloPrepare(ctx);
				break;

			case ptAcid:
				res = MasloPrepare(ctx);
				break;

			case ptFire:
				res = FirePrepare(weapon);
				break;

			case ptSpikes:
				res = SpikesPrepare(ctx, true);
				break;

			case ptSmokes:
				res = MinePrepare(ctx, true);
				break;

			case ptBurnMine:
				res = MinePrepare(ctx, true);
				break;

			case ptBarrel:
				res = BarrelPrepare(ctx);
				break;

			case ptBoom:
				res = BoomPrepare(ctx);
				break;

			case ptCore:
				res = CorePrepare(weapon);
				break;

			case ptForwardPoint:
				res = ScatterPrepare(weapon);
				break;

			case ptLeftPoint:
				res = ScatterPrepare(weapon);
				break;

			case ptRightPoint:
				res = ScatterPrepare(weapon);
				break;

			case ptLaserMine:
				res = LaserMinePrepare(ctx);
				break;

			case ptAspirine:
				res = MinePrepare(ctx, true);
				break;

			case ptHyperFly:
				res = HyperFlyPrepare(weapon);
				break;

			case ptMedpack:
				res = BonusPrepare(weapon);
				break;

			case ptMoney:
				res = BonusPrepare(weapon);
				break;

			case ptCharge:
				res = BonusPrepare(weapon);
				break;

			case ptImmortal:
				res = BonusPrepare(weapon);
				break;

			case ptSpeedArrow:
				res = SpeedArrowPrepare(weapon);
				break;

			case ptLusha:
				res = LushaPrepare(ctx);
				break;

			case ptSpinner:
				res = SpinnerPrepare(ctx);
				break;

			case ptSand:
				res = LushaPrepare(ctx);
				break;

			case ptGraviton:
				res = LushaPrepare(ctx);
				break;

			case ptLava:
				res = LushaPrepare(ctx);
				break;

			case ptFireBoost:
				res = FireBoostPrepare(ctx);
				break;

			case ptDanger:
				res = LushaPrepare(ctx);
				break;

			case ptBaloon:
				res = BaloonPrepare(ctx);
				break;

			case ptRock:
				res = RockPrepare(ctx);
				break;


			case ptPlatform:
				res = RockPrepare(ctx);
				break;

			case ptSapper:
				res = SapperPrepare(weapon);
				break;

			case ptIsoCam:
				res = TriggerPrepare(ctx);
				break;

			case ptThirdCam:
				res = TriggerPrepare(ctx);
				break;

			case ptCamFree:
				res = TriggerPrepare(ctx);
				break;

			case ptAutoCamOff:
				res = TriggerPrepare(ctx);
				break;

			case ptAutoCamOn:
				res = TriggerPrepare(ctx);
				break;

			case ptAltFinish:
				res = TriggerPrepare(ctx);
				break;

			case ptHeadLightOff:
				res = TriggerPrepare(ctx);
				break;

			case ptInRamp:
				res = TriggerPrepare(ctx);
				break;

			case ptAiJump:
				res = TriggerPrepare(ctx);
				break;

			case ptHeadLightOn:
				res = TriggerPrepare(ctx);
				break;

			case ptRage:
				res = BonusPrepare(weapon);
				break;

			case ptLucky:
				res = BonusPrepare(weapon);
				break;

			case ptUnLucky:
				res = BonusPrepare(weapon);
				break;

			case ptRocketJump:
				res = RJPrepare(weapon);
				break;

			case ptAiLockJump:
				res = TriggerPrepare(ctx);
				break;
			}

			//nxActor у blaster manticora не был создан!!!
			GetPxActor().SetScene(ctx.logic->GetPxScene());

			LSL_ASSERT(GetPxActor().GetNxActor());

			if (GetPxActor().GetNxActor() == nullptr)
			{
				LSL_LOG("Proj::PrepareProj GetPxActor().GetNxActor() == NULL!!!")
				return false;
			}

			//Игнорируем контакты снаряда с родителем (т.е. с самим собой) 
			if (_ignoreContactProj && weapon && weapon->GetPxActor().GetNxActor())
			{
				GetPxActor().GetScene()->GetNxScene()->setActorPairFlags(
					*weapon->GetPxActor().GetNxActor(), *GetPxActor().GetNxActor(), NX_IGNORE_PAIR);
			}

			return res;
		}

		void Proj::MineContact(GameObject* target, const D3DXVECTOR3& point)
		{
			this->Death();

			if (target == nullptr)
			{
				LSL_LOG("Proj::MineContact target == NULL")
				return;
			}

			if (target->IsCar() && target->IsCar() != nullptr)
			{
				//сила подброса зависит от навыка "corner"
				if (target->IsCar()->IsStabilityMine())
					AddContactForce(target, point, D3DXVECTOR3(0.0f, 0.0f, _desc.speed / 2), NX_IMPULSE);
				else
					AddContactForce(target, point, D3DXVECTOR3(0.0f, 0.0f, _desc.speed), NX_IMPULSE);
			}

			if (GetTimeLife() <= 2.0f)
			{
				DamageTarget(target, _desc.damage, dtMine);
			}
			else
				DamageTarget(target, _desc.damage, dtNone);
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

		GameObject* Proj::GetWeapon() const
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

				const D3DXVECTOR3 pos = GetPos();
				const D3DXQUATERNION rot = GetRot();

				ShotContext ctx;
				ctx.logic = GetLogic();
				PrepareProj(nullptr, ctx);

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

		void AutoProj::SaveSource(SWriter* writer)
		{
			_MyBase::SaveSource(writer);
		}

		void AutoProj::LoadSource(SReader* reader)
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
		= default;

		void Weapon::SaveSource(SWriter* writer)
		{
			_MyBase::SaveSource(writer);

			_desc.SaveTo(writer, this);
		}

		void Weapon::LoadSource(SReader* reader)
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

		float Weapon::GetShotTime() const
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

		bool Weapon::IsAutoMine() const
		{
			return !_desc.projList.empty() && (_desc.projList.front().type == Proj::ptCrater || _desc.projList.front()
				.type == Proj::ptMiniMine || _desc.projList.front().type == Proj::ptMineProton || _desc.projList.front()
				.type == Proj::ptMaslo || _desc.projList.front().type == Proj::ptAcid || _desc.projList.front().type ==
				Proj::ptFire || _desc.projList.front().type == Proj::ptSpikes || _desc.projList.front().type ==
				Proj::ptSmokes || _desc.projList.front().type == Proj::ptBurnMine || _desc.projList.front().type ==
				Proj::ptCore || _desc.projList.front().type == Proj::ptForwardPoint || _desc.projList.front().type ==
				Proj::ptLeftPoint || _desc.projList.front().type == Proj::ptRightPoint || _desc.projList.front().type ==
				Proj::ptAspirine);
		}

		const Weapon::Desc& Weapon::GetDesc() const
		{
			return _desc;
		}

		void Weapon::SetDesc(const Desc& value)
		{
			_desc = value;
		}

		bool Weapon::CreateShot(Weapon* weapon, const Desc& desc, const Proj::ShotContext& ctx, ProjList* projList)
		{
			bool res = false;

			for (const auto& iter : desc.projList)
			{
				if (weapon == nullptr &&
					(iter.type == Proj::ptRocket ||
						iter.type == Proj::ptHyper ||
						iter.type == Proj::ptSpring ||
						iter.type == Proj::ptSpring2 ||
						iter.type == Proj::ptRipper ||
						iter.type == Proj::ptShell ||
						iter.type == Proj::ptBlaster ||
						iter.type == Proj::ptAirWeapon ||
						iter.type == Proj::ptMiniGun ||
						iter.type == Proj::ptShotGun ||
						iter.type == Proj::ptTrinity ||
						iter.type == Proj::ptLaser ||
						iter.type == Proj::ptFrostRay ||
						iter.type == Proj::ptNewFrostRay ||
						iter.type == Proj::ptMortira ||
						iter.type == Proj::ptMolotov ||
						iter.type == Proj::ptCrater ||
						iter.type == Proj::ptArtillery ||
						iter.type == Proj::ptGrenade ||
						iter.type == Proj::ptTorpeda ||
						iter.type == Proj::ptImpulse ||
						iter.type == Proj::ptSphereGun ||
						iter.type == Proj::ptPlazma ||
						iter.type == Proj::ptPhoenix ||
						iter.type == Proj::ptMagnetic ||
						iter.type == Proj::ptDisc ||
						iter.type == Proj::ptShocker ||
						iter.type == Proj::ptFire ||
						iter.type == Proj::ptBoom ||
						iter.type == Proj::ptCore ||
						iter.type == Proj::ptForwardPoint ||
						iter.type == Proj::ptLeftPoint ||
						iter.type == Proj::ptRightPoint ||
						iter.type == Proj::ptMedpack ||
						iter.type == Proj::ptMoney ||
						iter.type == Proj::ptCharge ||
						iter.type == Proj::ptImmortal ||
						iter.type == Proj::ptSpeedArrow ||
						iter.type == Proj::ptRage ||
						iter.type == Proj::ptLucky ||
						iter.type == Proj::ptUnLucky ||
						iter.type == Proj::ptRocketJump ||
						iter.type == Proj::ptSapper
					))
					continue;

				auto proj = new Proj();
				proj->SetDesc(iter);

				if (proj->PrepareProj(weapon, ctx))
				{
					ctx.logic->RegGameObj(proj);

					if (weapon)
					{
						weapon->_shotTime = 0.0f;
						weapon->GetBehaviors().OnShot(iter.pos);
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
