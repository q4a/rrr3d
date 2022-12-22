#include "stdafx.h"
#include "game//GameCar.h"

#include "game//Logic.h"
#include "game//Race.h"

namespace r3d
{
	namespace game
	{
		const float GameCar::cMaxSteerAngle = D3DX_PI / 6;


		CarMotorDesc::CarMotorDesc(): maxRPM(7000), idlingRPM(1000), maxTorque(2000.0f), SEM(0.7f), gearDiff(3.42f),
		                              autoGear(true), brakeTorque(7500), restTorque(400.0f)
		{
			//задняя
			gears.push_back(1.5f);
			//
			gears.push_back(2.66f);
			gears.push_back(1.78f);
			gears.push_back(1.30f);
			gears.push_back(1.00f);
			gears.push_back(0.74f);
		}

		float CarMotorDesc::CalcRPM(float wheelAxleSpeed, unsigned curGear) const
		{
			if (curGear == cNeutralGear)
				return static_cast<float>(idlingRPM);
			const float rpm = abs(wheelAxleSpeed) * gears[curGear] * gearDiff * 60.0f / (2.0f * D3DX_PI);

			return std::min(rpm, static_cast<float>(maxRPM));
		}

		float CarMotorDesc::CalcTorque(float rpm, unsigned curGear) const
		{
			constexpr float cGameK = 1.15f;

			return maxTorque * gears[curGear] * gearDiff * SEM * cGameK;
		}


		CarWheel::CarWheel(CarWheels* owner): _owner(owner), _wheelShape(nullptr), _summAngle(0), _trailEff(nullptr),
		                                      _actTrail(nullptr), _steerAngle(0), _lead(false), _steer(false),
		                                      _offset(NullVector), _pxPrevPos(NullVector), _pxPrevRot(NullQuaternion),
		                                      invertWheel(false)
		{
			_myContactModify = new MyContactModify(this);
			_trails = new _Trails(this);
		}

		CarWheel::~CarWheel()
		{
			SetSteer(false);
			SetLead(false);

			SafeRelease(_actTrail);
			delete _trails;
			SetTrailEff(nullptr);

			DestroyWheelShape();
			delete _myContactModify;
		}

		CarWheel::MyContactModify::MyContactModify(CarWheel* wheel): _wheel(wheel)
		{
		}

		bool CarWheel::MyContactModify::onWheelContact(NxWheelShape* wheelShape, NxVec3& contactPoint,
		                                               NxVec3& contactNormal, NxReal& contactPosition,
		                                               NxReal& normalForce, NxShape* otherShape,
		                                               NxMaterialIndex& otherShapeMaterialIndex,
		                                               NxU32 otherShapeFeatureIndex)
		{
			const float normReaction = abs(
				wheelShape->getActor().getMass() * contactNormal.dot(px::Scene::cDefGravity) / _wheel->_owner->Size());

			if (_wheel->_owner->GetOwner()->IsClutchLocked())
			{
				normalForce = 0.0f;
			}
			else
			{
				const float tireSpring = _wheel->_owner->GetOwner()->GetTireSpring();
				const float nReac = normalForce / normReaction;
				_wheel->_nReac = nReac;

				//Ограничение допустимой перегрузки, чтобы машина при падении не ускорялась в связи с чрезмерно больгим сцеплением. Допустимая перегрузка в 1.5g.
				if (tireSpring > 0.0f && nReac > tireSpring)
					normalForce = 0.0f;
				else
					normalForce = std::min(normalForce, normReaction * 1.5f); //1.5f g
			}

			return true;
		}

		void CarWheel::Save(SWriter* writer)
		{
			_MyBase::Save(writer);

			writer->WriteValue("shape", _wheelShape->GetName());
			MapObjRec::Lib::SaveRecordRef(writer, "trailEff", _trailEff);

			writer->WriteValue("lead", _lead);
			writer->WriteValue("steer", _steer);
			SWriteValue(writer, "offset", _offset);
			writer->WriteValue("invertWheel", invertWheel);
		}

		void CarWheel::Load(SReader* reader)
		{
			//Если загрузка не чистого актера, то сначала нужно уничтожить занятый шейп
			if (_wheelShape)
				DestroyWheelShape();

			_MyBase::Load(reader);

			std::string shapeName;
			if (reader->ReadValue("shape", shapeName))
			{
				_wheelShape = lsl::StaticCast<px::WheelShape*>(GetPxActor().GetShapes().Find(shapeName));
				_wheelShape->AddRef();
				_wheelShape->SetContactModify(_myContactModify);
			}

			if (!_wheelShape)
				throw Error("CarWheel::Load");

			SetTrailEff(MapObjRec::Lib::LoadRecordRef(reader, "trailEff"));

			bool lead;
			reader->ReadValue("lead", lead);
			SetLead(lead);

			bool steer;
			reader->ReadValue("steer", steer);
			SetSteer(steer);

			SReadValue(reader, "offset", _offset);

			reader->ReadValue("invertWheel", invertWheel);
		}

		void CarWheel::CreateWheelShape()
		{
			LSL_ASSERT(!_wheelShape);

			_wheelShape = &GetPxActor().GetShapes().Add<px::WheelShape>();
			_wheelShape->AddRef();
			_wheelShape->SetContactModify(_myContactModify);
			_wheelShape->SetGroup(px::Scene::cdgWheel);
		}

		void CarWheel::CreateWheelShape(const NxWheelShapeDesc& desc)
		{
			LSL_ASSERT(!_wheelShape);

			_wheelShape = &GetPxActor().GetShapes().Add<px::WheelShape>();
			_wheelShape->AddRef();
			_wheelShape->AssignFromDesc(desc);
			_wheelShape->SetContactModify(_myContactModify);
			_wheelShape->SetGroup(px::Scene::cdgWheel);
		}

		void CarWheel::DestroyWheelShape()
		{
			LSL_ASSERT(_wheelShape);

			_wheelShape->Release();
			GetPxActor().GetShapes().Delete(_wheelShape);
			_wheelShape = nullptr;
		}

		void CarWheel::PxSyncWheel(float alpha)
		{
			const NxWheelShape* wheel = _wheelShape->GetNxShape();
			LSL_ASSERT(wheel);

			NxReal st = wheel->getSuspensionTravel();
			const NxReal r = wheel->getRadius();
			const NxMat34 localPose = wheel->getLocalPose();
			//cast along -Y	
			const NxVec3 dir = localPose.M.getColumn(1);
			const NxVec3 t = localPose.t;

			NxWheelContactData wcd;
			//cast from shape origin
			const NxShape* s = wheel->getContact(wcd);
			if (s && wcd.contactForce > -1000)
				st = wcd.contactPosition - r;

			D3DXQUATERNION quat1;
			D3DXQUATERNION quat2 = NullQuaternion;
			D3DXQuaternionRotationAxis(&quat1, &ZVector, _steerAngle);
			D3DXQuaternionRotationAxis(&quat2, &YVector, _summAngle);

			D3DXQUATERNION resRot = quat2 * quat1;
			if (invertWheel)
			{
				D3DXQUATERNION invRot;
				D3DXQuaternionRotationAxis(&invRot, &ZVector, D3DX_PI);
				resRot = invRot * resRot;
			}

			if (alpha < 1.0f)
				D3DXQuaternionSlerp(&resRot, &_pxPrevRot, &resRot, alpha);
			else
				_pxPrevRot = resRot;
			GetGrActor().SetRot(resRot);

			D3DXVECTOR3 resPos((t - dir * st).get());
			if (alpha < 1.0f)
				D3DXVec3Lerp(&resPos, &_pxPrevPos, &resPos, alpha);
			else
				_pxPrevPos = resPos;
			GetGrActor().SetPos(resPos + _offset);
		}

		void CarWheel::OnProgress(float deltaTime)
		{
			const float slipLong = 0.4f;
			const float slipLat = 0.6f;

			LSL_ASSERT(GetShape());

			_MyBase::OnProgress(deltaTime);

			const NxWheelShape* wheel = _wheelShape->GetNxShape();
			if (wheel)
				_summAngle += wheel->getAxleSpeed() * deltaTime;

			_trails->OnProgress(deltaTime);

			if (_trailEff)
			{
				NxWheelContactData contactDesc;
				const NxShape* contact = GetShape()->GetNxShape()->getContact(contactDesc);
				bool slip = false;

				if (contact)
				{
					slip = abs(contactDesc.lateralSlip) > slipLat || abs(contactDesc.longitudalSlip) > slipLong;
				}

				if (slip)
				{
					if (!_actTrail)
					{
						_actTrail = &_trails->Add(_trailEff);
						_actTrail->AddRef();
					}

					_actTrail->GetGameObj().SetWorldPos(D3DXVECTOR3(contactDesc.contactPoint.get()) + ZVector * 0.001f);
					//Во время установки следа время жизни не меняется
					_actTrail->GetGameObj().SetTimeLife(0);
				}
				else
					SafeRelease(_actTrail);
			}
		}

		px::WheelShape* CarWheel::GetShape() const
		{
			return _wheelShape;
		}

		MapObjRec* CarWheel::GetTrailEff() const
		{
			return _trailEff;
		}

		void CarWheel::SetTrailEff(MapObjRec* value)
		{
			if (ReplaceRef(_trailEff, value))
				_trailEff = value;
		}

		float CarWheel::GetLongSlip() const
		{
			LSL_ASSERT(GetShape());

			NxWheelContactData contactDesc;
			const NxShape* contact = GetShape()->GetNxShape()->getContact(contactDesc);

			return contact ? contactDesc.longitudalSlip : 0;
		}

		float CarWheel::GetLatSlip() const
		{
			NxWheelContactData contactDesc;
			const NxShape* contact = GetShape()->GetNxShape()->getContact(contactDesc);

			return contact ? contactDesc.lateralSlip : 0;
		}

		bool CarWheel::GetLead() const
		{
			return _lead;
		}

		float CarWheel::GetSteerAngle() const
		{
			return _steerAngle;
		}

		void CarWheel::SetSteerAngle(float value)
		{
			_steerAngle = value;
		}

		void CarWheel::SetLead(bool value)
		{
			if (_lead != value)
			{
				if (_lead)
					_owner->RemoveLeadWheel(this);

				_lead = value;

				if (_lead)
					_owner->InsertLeadWheel(this);
			}
		}


		bool CarWheel::GetSteer() const
		{
			return _steer;
		}

		void CarWheel::SetSteer(bool value)
		{
			if (_steer != value)
			{
				if (_steer)
					_owner->RemoveSteerWheel(this);

				_steer = value;

				if (_steer)
					_owner->InsertSteerWheel(this);
			}
		}

		void CarWheel::RemoveWheel()
		{
			if (this->GetSteer())
				_owner->RemoveSteerWheel(this);

			if (this->GetLead())
				_owner->RemoveLeadWheel(this);
		}

		const D3DXVECTOR3& CarWheel::GetOffset() const
		{
			return _offset;
		}

		void CarWheel::SetOffset(const D3DXVECTOR3& value)
		{
			_offset = value;
		}


		CarWheels::CarWheels(GameCar* owner): _owner(owner), _steerContactModify(nullptr)
		{
		}

		CarWheels::~CarWheels()
		{
			SetSteerContactModify(nullptr);
		}

		void CarWheels::LoadPosTo(const std::string& fileName, std::vector<D3DXVECTOR3>& pos)
		{
			std::istream* file = FileSystem::GetInstance()->NewInStream(fileName, FileSystem::omText, 0);
			try
			{
				while (*file && !file->eof())
				{
					D3DXVECTOR3 res;
					*file >> res.x >> res.y >> res.z;
					pos.push_back(res);
				}
			}
			LSL_FINALLY(lsl::FileSystem::GetInstance()->FreeStream(file);)
		}

		void CarWheels::InsertItem(const Value& value)
		{
			_MyBase::InsertItem(value);

			value->SetOwner(_owner);
			value->CreateWheelShape();
			_owner->InsertChild(value);
			value->GetGrActor().SetSceneList(_owner->GetGrActor().GetSceneList());
		}

		void CarWheels::InsertLeadWheel(CarWheel* value)
		{
			_leadGroup.push_back(value);
		}

		void CarWheels::RemoveLeadWheel(CarWheel* value)
		{
			_leadGroup.Remove(value);
		}

		void CarWheels::InsertSteerWheel(CarWheel* value)
		{
			_steerGroup.push_back(value);
		}

		void CarWheels::RemoveSteerWheel(CarWheel* value)
		{
			_steerGroup.Remove(value);
		}

		CarWheel& CarWheels::Add()
		{
			return _MyBase::Add();
		}

		CarWheel& CarWheels::Add(const NxWheelShapeDesc& desc)
		{
			CarWheel* wheel = CreateItem();
			wheel->CreateWheelShape(desc);

			return AddItem(wheel);
		}

		GameCar* CarWheels::GetOwner() const
		{
			return _owner;
		}

		const CarWheels::WheelGroup& CarWheels::GetLeadGroup() const
		{
			return _leadGroup;
		}

		const CarWheels::WheelGroup& CarWheels::GetSteerGroup() const
		{
			return _steerGroup;
		}

		CarWheels::ContactModify* CarWheels::GetSteerContactModify() const
		{
			return _steerContactModify;
		}

		void CarWheels::SetSteerContactModify(ContactModify* value)
		{
			if (Object::ReplaceRef(_steerContactModify, value))
			{
				_steerContactModify = value;
				for (const auto& iter : _steerGroup)
				{
					iter->GetShape()->SetContactModify(value);
					return;
				}
			}
		}


		GameCar::GameCar(): _clutchStrength(0), _clutchTime(0.0f), _mineTime(0), _curGear(-1), _moveCar(mcNone),
		                    _steerWheel(swNone), _kSteerControl(1.0f), _steerSpeed(D3DX_PI / 2.0f), _steerRot(D3DX_PI),
		                    _flyYTorque(D3DX_PI / 1.6f), _clampXTorque(0), _clampYTorque(0), _motorTorqueK(1),
		                    _wheelSteerK(1), _angDamping(IdentityVector), _gravEngine(false), _clutchImmunity(false),
		                    _maxSpeed(0), _tireSpring(0), _disableColor(false), _spinstatus(false), _resplocked(false),
		                    _wasted(false), _burn(false), _unlimitedTurn(false), _doublej(true), _turnForce(1.0f), _burnDamage(0), _backSpeedK(1.0f), _turnfreeze(false),
		                    _turnInverse(false), _inverseTime(0), _LMS(0), _isDamageStop(false), _maxTimeSR(3.0f),
		                    _deminingTime(1.0f), _slowride(false), _demining(false), _onJump(false), _goAiJump(false),
		                    _inFly(false), _lockAJmp(false), _inRage(false), _invisible(false), _isStabilityMine(false),
		                    _isStabilityShot(false), _ghostEff(false), _steerAngle(0), _anyWheelContact(false),
		                    _wheelsContact(false), _bodyContact(false)
		{
			_wheels = new Wheels(this);

			GetPxActor().SetContactReportFlags(NX_NOTIFY_ALL | NX_NOTIFY_CONTACT_MODIFICATION);
			GetPxActor().SetFlag(NX_AF_CONTACT_MODIFICATION, true);

			RegFixedStepEvent();
		}

		GameCar::~GameCar()
		{
			UnregFixedStepEvent();

			Destroy();

			_wheels->Clear();

			delete _wheels;
		}

		void GameCar::MotorProgress(float deltaTime, float& curMotorTorque, float& curBreakTorque, float& curRPM)
		{
			//в режиме вращения используются те же параметры что и при mcAccel.
			if (InSpinStatus())
			{
				SetLastMoveState(1);
				if (GetSpeed() < 0)
				{
					SetCurGear(cNeutralGear);
					curRPM = GetWheelRPM();
					curBreakTorque = _motor.brakeTorque;
					curMotorTorque = 0;
				}
				else
				{
					if (_curGear == cNeutralGear || _curGear == cBackGear)
						SetCurGear(1);

					curRPM = GetWheelRPM();
					curBreakTorque = _motor.restTorque;
					curMotorTorque = _motor.CalcTorque(curRPM, _curGear) * _motorTorqueK;
				}
			}
			else
			{
				switch (_moveCar)
				{
				case mcNone:
					curRPM = GetWheelRPM();
					curBreakTorque = _motor.restTorque;
					curMotorTorque = 0;
					break;

				case mcBrake:
					SetLastMoveState(0);
					SetCurGear(cNeutralGear);
					curRPM = GetWheelRPM();
					curBreakTorque = _motor.brakeTorque;
					curMotorTorque = 0;
					break;

				case mcBack:
					SetLastMoveState(2);
					if (GetSpeed() > 0)
					{
						curRPM = GetWheelRPM();
						curBreakTorque = _motor.brakeTorque;
						curMotorTorque = 0;
					}
					else
					{
						SetCurGear(cBackGear);
						curRPM = GetWheelRPM();
						curBreakTorque = _motor.restTorque;
						if (curRPM < _motor.maxRPM * _backSpeedK)
							curMotorTorque = -_motor.CalcTorque(curRPM, _curGear);
					}
					break;

				case mcAccel:
					SetLastMoveState(1);
					if (GetSpeed() < 0)
					{
						SetCurGear(cNeutralGear);
						curRPM = GetWheelRPM();
						curBreakTorque = _motor.brakeTorque;
						curMotorTorque = 0;
					}
					else
					{
						if (_curGear == cNeutralGear || _curGear == cBackGear)
							SetCurGear(1);

						curRPM = GetWheelRPM();
						curBreakTorque = _motor.restTorque;
						curMotorTorque = _motor.CalcTorque(curRPM, _curGear) * _motorTorqueK;
					}
					break;
				}
			}
		}

		inline float NxQuatAngle(const NxQuat& quat1, const NxQuat& quat2)
		{
			return acos(abs(quat1.dot(quat2) / sqrt(quat1.magnitudeSquared() * quat2.magnitudeSquared()))) * 2;
		}

		inline void NxQuatRotation(NxQuat& quat, const NxQuat& quat1, const NxQuat& quat2)
		{
			quat = quat1;
			quat.invert();
			quat = quat * quat2;
		}

		void GameCar::WheelsProgress(float deltaTime, float motorTorque, float breakTorque)
		{
			NxActor* nxActor = GetPxActor().GetNxActor();
			float speed = GetSpeed(nxActor, nxActor->getGlobalOrientationQuat().rot(NxVec3(1.0f, 0.0f, 0.0f)).get());
			float absSpeed = GetPxActor().GetNxActor()->getLinearVelocity().magnitude();

			if (_maxSpeed > 0 && absSpeed > _maxSpeed)
			{
				motorTorque = breakTorque;
			}

			if (_steerWheel == smManual)
			{
				//nothing		
			}
			else
			{
				if (_steerWheel == swOnLeft)
				{
					if (_wasted == false && _turnfreeze == false)
					{
						_steerAngle = _steerSpeed > 0
							              ? std::min(std::max(_steerAngle, 0.0f) + _steerSpeed * deltaTime,
							                         cMaxSteerAngle * _turnForce)
							              : cMaxSteerAngle;
					}
				}
				else if (_steerWheel == swOnRight)
				{
					if (_wasted == false && _turnfreeze == false)
					{
						_steerAngle = _steerSpeed > 0
							              ? std::max(std::min(_steerAngle, 0.0f) - _steerSpeed * deltaTime,
							                         -cMaxSteerAngle * _turnForce)
							              : -cMaxSteerAngle;
					}
				}
				else
					_steerAngle = 0;
			}

			bool anyLeadWheelContact = false;

			//для сайдвиндера возможны повороты в воздухе, если полностью прокачано шасси:
			if (GetMapObj()->GetPlayer()->GetSlotInst(Player::stHyper) && GetMapObj()->GetPlayer()->
				GetSlotInst(Player::stHyper)->GetItem().GetId() == 43 && GetMapObj()->GetPlayer()->
				GetSlotInst(Player::stTrans)->GetItem().GetName() == "scTrans4")
				anyLeadWheelContact = true;

			CarWheel* backWheel = nullptr;

			for (auto& _wheel : *_wheels)
			{
				CarWheel* wheel = _wheel;
				NxWheelShape* pxWheel = wheel->GetShape()->GetNxShape();

				pxWheel->setBrakeTorque(breakTorque);

				if (_wheel->GetLead())
				{
					pxWheel->setMotorTorque(motorTorque);
				}
				if (_wheel->GetSteer())
				{
					_wheel->SetSteerAngle(_steerAngle);
				}
				if (_wheel->GetPos().x < 0)
					backWheel = _wheel;

				NxWheelContactData data;
				if (pxWheel->getContact(data))
				{
					_anyWheelContact = true;

					if (_wheel->GetLead())
						anyLeadWheelContact = true;
				}
				else
					_wheelsContact = false;
			}

			StabilizeForce(deltaTime);

			LSL_ASSERT(backWheel);

			if (((_gravEngine && _anyWheelContact) || (!_gravEngine && anyLeadWheelContact)) && _steerAngle != 0 && !
				IsClutchLocked()) //_steerWheel != smManual
			{
				float alpha = ClampValue(speed / 10.0f, -1.0f, 1.0f);
				//alpha = 1.0f;

				NxMat34 worldMat = nxActor->getGlobalPose();
				NxQuat rotQuat;
				if (_unlimitedTurn == false)
				{
					if (_turnInverse == false)
						rotQuat.fromAngleAxisFast(alpha * _steerAngle / cMaxSteerAngle * _steerRot * deltaTime,
						                          NxVec3(0, 0, 1));
					else
						rotQuat.fromAngleAxisFast(alpha * -(_steerAngle / cMaxSteerAngle) * _steerRot * deltaTime,
						                          NxVec3(0, 0, 1));
				}
				else if (_unlimitedTurn == true)
				{
					if (_turnInverse == false)
						rotQuat.fromAngleAxisFast(_steerAngle / cMaxSteerAngle * _steerRot * deltaTime,
						                          NxVec3(0, 0, 1));
					else
						rotQuat.fromAngleAxisFast(-(_steerAngle / cMaxSteerAngle) * _steerRot * deltaTime,
						                          NxVec3(0, 0, 1));
				}

				NxMat34 rotMat;
				rotMat.M.fromQuat(rotQuat);
				NxMat34 matOffs1;
				matOffs1.t = NxVec3(backWheel->GetPos().x, 0, 0);
				NxMat34 matOffs2;
				matOffs2.t = -matOffs1.t;
				worldMat = worldMat * matOffs1 * rotMat * matOffs2;

				nxActor->setGlobalPose(worldMat);
			}
		}

		void GameCar::TransmissionProgress(float deltaTime, float curRPM)
		{
			NxWheelContactData contact;
			if (GetWheelContactData(contact) == nullptr)
				return;

			if (_curGear != cNeutralGear && _curGear != cBackGear)
			{
				if (curRPM < _motor.maxRPM / 1.8f)
				{
					if (_motor.autoGear && _curGear > 1)
						GearDown();
				}
				if (curRPM >= _motor.maxRPM)
				{
					if (_motor.autoGear)
						GearUp();
				}
			}
		}

		void GameCar::JumpProgress(float deltaTime)
		{
			NxActor* nxActor = GetPxActor().GetNxActor();
			//коэффициент гравитации, наверное следует также указать и в других функциях, но это потом.
			const float gravityK = this->GetMapObj()->GetPlayer()->GetRace()->GetTournament().GetCurTrack().GetGravityK();

			if (!_anyWheelContact)
			{
				if (_onJump == false)
				{
					//Если нет контакта колес с поверхностью + это не прыжок, то прижимная сила зависит от скорости:
					//Также от скорости зависит наклон машины.
					if (nxActor->getLinearVelocity().magnitude() < 16.0f)
					{
						nxActor->addForce((2.8f * px::Scene::cDefGravity) * gravityK, NX_ACCELERATION);
					}
					else if (nxActor->getLinearVelocity().magnitude() > 16.0f && nxActor->getLinearVelocity().
						magnitude() < 34.0f)
					{
						nxActor->addForce((1.5f * px::Scene::cDefGravity) * gravityK, NX_ACCELERATION);
						nxActor->addLocalTorque(NxVec3(0, 7, 0), NX_ACCELERATION);
					}
					else if (nxActor->getLinearVelocity().magnitude() > 34.0f && nxActor->getLinearVelocity().
						magnitude() < 44.0f)
					{
						nxActor->addForce((1.2f * px::Scene::cDefGravity) * gravityK, NX_ACCELERATION);
						nxActor->addLocalTorque(NxVec3(0, _flyYTorque, 0), NX_ACCELERATION);
					}
					else if (nxActor->getLinearVelocity().magnitude() > 44.0f)
					{
						nxActor->addForce((1.0f * px::Scene::cDefGravity) * gravityK, NX_ACCELERATION);
					}
				}
				else
				{
					//Для прыжка используем прижимную силу по-умолчанию:
					nxActor->addForce((1.0f * px::Scene::cDefGravity) * gravityK, NX_ACCELERATION);
					SetWastedControl(false);
					SetTurnFreeze(false);
				}
			}
		}

		void GameCar::SpinOutProgress(float deltaTime)
		{
			const float curspeed = this->GetPxActor().GetNxActor()->getLinearVelocity().magnitude();
			float smoothK = 1.0f;

			if (curspeed > 42.0f)
				smoothK = 0.9f;
			else if (curspeed >= 38.0f && curspeed <= 42.0f)
				smoothK = 0.8f;
			else if (curspeed >= 32.0f && curspeed < 38.0f)
				smoothK = 0.7f;
			else //curspeed < 32.0f
			{
				SetSpinStatus(false);
				if (curspeed <= 8.0f)
				{
					this->SetRespBlock(false);
					this->SetWastedControl(false);
				}
			}

			if (InSpinStatus() && GetMapObj() != nullptr)
			{
				constexpr float baseK = 0.2f;
				if (curspeed < 38.0f)
					_unlimitedTurn = false;

				//сила заноса = текущая скорость * коэфициент.
				LockClutch((baseK * curspeed) * (this->GetMapObj()->GetPlayer()->GetDriftStrength() * smoothK));
			}

			if (GetWastedControl())
				LockClutch(0.0f);
		}

		void GameCar::StabilizeForce(float deltaTime)
		{
			NxVec3 angMomentum = GetPxActor().GetNxActor()->getAngularMomentum();
			const NxMat33 mat = GetPxActor().GetNxActor()->getGlobalOrientation();
			NxMat33 invMat;

			if ((_angDamping.x != -1 || _angDamping.y != -1 || _angDamping.z != -1 || _clampYTorque > 0 || _clampXTorque
				> 0 || IsClutchLocked()) && mat.getInverse(invMat))
			{
				angMomentum = invMat * angMomentum;
				if (!_anyWheelContact && _clampYTorque > 0)
				{
					const float angY = std::min(abs(angMomentum.y),
					                            GetPxActor().GetNxActor()->getMassSpaceInertiaTensor().y * _clampYTorque *
					                            2.0f);
					angMomentum.y = angMomentum.y >= 0 ? angY : -angY;
				}
				if (!_anyWheelContact && _clampXTorque > 0)
				{
					const float angX = std::min(abs(angMomentum.x),
					                            GetPxActor().GetNxActor()->getMassSpaceInertiaTensor().x * _clampXTorque *
					                            2.0f);
					angMomentum.x = angMomentum.x >= 0 ? angX : -angX;
				}

				float angMomZ = angMomentum.z * _angDamping.z;
				if (IsClutchLocked())
				{
					if (_clutchStrength != 0)
						angMomZ = _clutchStrength * GetNxActor()->getMass();
					else
						angMomZ = angMomentum.z;
					_clutchStrength = 0;
				}

				angMomentum = mat * NxVec3(angMomentum.x * _angDamping.x, angMomentum.y * _angDamping.y, angMomZ);

				if (_clampYTorque > 0 || _clampXTorque > 0)
				{
					NxQuat rot = GetPxActor().GetNxActor()->getGlobalOrientationQuat();

					EulerAngles angles = Eul_FromQuat(*reinterpret_cast<Quat*>(&rot), EulOrdXYZs);
					if (_clampXTorque > 0)
						angles.x = ClampValue(angles.x, -_clampXTorque, _clampXTorque);
					if (_clampYTorque > 0)
						angles.y = ClampValue(angles.y, -_clampYTorque, _clampYTorque);
					rot.setXYZW(reinterpret_cast<float*>(&Eul_ToQuat(angles)));

					GetPxActor().GetNxActor()->setGlobalOrientationQuat(rot);
				}
			}

			GetPxActor().GetNxActor()->setAngularMomentum(angMomentum);
		}

		float GameCar::GetWheelRPM() const
		{
			LSL_ASSERT(!_wheels->GetLeadGroup().empty());

			const NxWheelShape* wheel = _wheels->GetLeadGroup().front()->GetShape()->GetNxShape();

			return _motor.CalcRPM(wheel->getAxleSpeed(), _curGear);
		}

		NxShape* GameCar::GetWheelContactData(NxWheelContactData& contact) const
		{
			LSL_ASSERT(!_wheels->GetLeadGroup().empty());

			const NxWheelShape* wheel = _wheels->GetLeadGroup().front()->GetShape()->GetNxShape();

			return wheel->getContact(contact);
		}

		void GameCar::ApplyWheelSteerK() const
		{
			for (const auto iter : GetWheels())
			{
				if (iter->GetShape() == nullptr || iter->GetShape()->GetNxShape() == nullptr)
					continue;

				NxTireFunctionDesc desc = iter->GetShape()->GetLateralTireForceFunction();
				desc.asymptoteValue *= _wheelSteerK;
				desc.extremumValue *= _wheelSteerK;
				iter->GetShape()->GetNxShape()->setLateralTireForceFunction(desc);
			}
		}

		void GameCar::SaveSource(SWriter* writer)
		{
			_MyBase::SaveSource(writer);

			{
				SWriter* child = writer->NewDummyNode("motor");
				child->WriteValue("autoGear", _motor.autoGear);
				child->WriteValue("brakeTorque", _motor.brakeTorque);
				child->WriteValue("gearDiff", _motor.gearDiff);
				child->WriteValue("maxRPM", _motor.maxRPM);
				child->WriteValue("maxTorque", _motor.maxTorque);
				child->WriteValue("SEM", _motor.SEM);

				child->WriteValue("kSteerControl", _kSteerControl);
				child->WriteValue("steerSpeed", _steerSpeed);
				child->WriteValue("steerRot", _steerRot);
				child->WriteValue("flyYTorque", _flyYTorque);
				child->WriteValue("clampXTorque", _clampXTorque);
				child->WriteValue("clampYTorque", _clampYTorque);
				child->WriteValue("gravEngine", _gravEngine);
				child->WriteValue("clutchImmunity", _clutchImmunity);
				child->WriteValue("maxSpeed", _maxSpeed);
				child->WriteValue("tireSpring", _tireSpring);
				child->WriteValue("disableColor", _disableColor);
				child->WriteValue("spinstatus", _spinstatus);
				child->WriteValue("resplocked", _resplocked);
				child->WriteValue("wasted", _wasted);   
				child->WriteValue("burn", _burn);
				child->WriteValue("unlimitedTurn", _unlimitedTurn);
				child->WriteValue("doublej", _doublej);
				child->WriteValue("turnForce", _turnForce);
				child->WriteValue("burnDamage", _burnDamage);
				child->WriteValue("backSpeedK", _backSpeedK);
				child->WriteValue("turnfreeze", _turnfreeze);
				child->WriteValue("turnInverse", _turnInverse);
				child->WriteValue("inverseTime", _inverseTime);
				child->WriteValue("LMS", _LMS);
				child->WriteValue("isDamageStop", _isDamageStop);
				child->WriteValue("maxTimeSR", _maxTimeSR);
				child->WriteValue("slowride", _slowride);
				child->WriteValue("demining", _demining);
				child->WriteValue("onJump", _onJump);
				child->WriteValue("goAiJump", _goAiJump);
				child->WriteValue("inFly", _inFly);
				child->WriteValue("lockAJmp", _lockAJmp);
				child->WriteValue("inRage", _inRage);
				child->WriteValue("invisible", _invisible);
				child->WriteValue("isStabilityMine", _isStabilityMine);
				child->WriteValue("isStabilityShot", _isStabilityShot);
				child->WriteValue("ghostEff", _ghostEff);
				child->WriteValue("deminingTime", _deminingTime);

				SWriteValue(writer, "angDamping", _angDamping);
			}

			writer->WriteValue("wheels", _wheels);
		}

		void GameCar::LoadSource(SReader* reader)
		{
			_MyBase::LoadSource(reader);

			if (SReader* child = reader->ReadValue("motor"))
			{
				child->ReadValue("autoGear", _motor.autoGear);
				child->ReadValue("brakeTorque", _motor.brakeTorque);
				child->ReadValue("gearDiff", _motor.gearDiff);
				child->ReadValue("maxRPM", _motor.maxRPM);
				child->ReadValue("maxTorque", _motor.maxTorque);
				child->ReadValue("SEM", _motor.SEM);

				child->ReadValue("kSteerControl", _kSteerControl);
				child->ReadValue("steerSpeed", _steerSpeed);
				child->ReadValue("steerRot", _steerRot);
				child->ReadValue("flyYTorque", _flyYTorque);
				child->ReadValue("clampXTorque", _clampXTorque);
				child->ReadValue("clampYTorque", _clampYTorque);
				child->ReadValue("gravEngine", _gravEngine);
				child->ReadValue("clutchImmunity", _clutchImmunity);
				child->ReadValue("maxSpeed", _maxSpeed);
				child->ReadValue("tireSpring", _tireSpring);
				child->ReadValue("disableColor", _disableColor);
				child->ReadValue("spinstatus", _spinstatus);
				child->ReadValue("resplocked", _resplocked);
				child->ReadValue("wasted", _wasted);
				child->ReadValue("burn", _burn);  
				child->ReadValue("unlimitedTurn", _unlimitedTurn);
				child->ReadValue("doublej", _doublej);
				child->ReadValue("turnForce", _turnForce);
				child->ReadValue("burnDamage", _burnDamage);
				child->ReadValue("backSpeedK", _backSpeedK);
				child->ReadValue("turnfreeze", _turnfreeze);
				child->ReadValue("turnInverse", _turnInverse);
				child->ReadValue("inverseTime", _inverseTime);
				child->ReadValue("LMS", _LMS);
				child->ReadValue("isDamageStop", _isDamageStop);
				child->ReadValue("maxTimeSR", _maxTimeSR);
				child->ReadValue("slowride", _slowride);
				child->ReadValue("demining", _demining);
				child->ReadValue("onJump", _onJump);
				child->ReadValue("goAiJump", _goAiJump);
				child->ReadValue("inFly", _inFly);
				child->ReadValue("lockAJmp", _lockAJmp);
				child->ReadValue("inRage", _inRage);
				child->ReadValue("invisible", _invisible);
				child->ReadValue("isStabilityMine", _isStabilityMine);
				child->ReadValue("isStabilityShot", _isStabilityShot);
				child->ReadValue("ghostEff", _ghostEff);
				child->ReadValue("deminingTime", _deminingTime);

				SReadValue(reader, "angDamping", _angDamping);
			}

			reader->ReadValue("wheels", _wheels);

			ApplyWheelSteerK();
		}

		//преобразуем в локльную систему координат тела актера
		void NxQuatFromWorldToLocal(const NxMat33& worldMat, const NxMat33& worldMatRot, NxMat33& outLocalMatRot)
		{
			NxMat33 invWorldMat;
			worldMat.getInverse(invWorldMat);
			outLocalMatRot.multiply(invWorldMat, worldMatRot);
		}

		void NxQuatFromLocalToWorld(const NxMat33& worldMat, const NxMat33& localMatRot, NxMat33& outWorldMatRot)
		{
			outWorldMatRot.multiply(worldMat, localMatRot);
		}

		bool GameCar::OnContactModify(const px::Scene::OnContactModifyEvent& contact)
		{
			_MyBase::OnContactModify(contact);

			GameObject* target = GetGameObjFromActor(contact.actor);

			if (!(target && target->GetMapObj() && target->GetMapObj()->GetRecord() && target->GetMapObj()->GetRecord()
				->GetCategory() == MapObjLib::ctTrack))
				return true;

			bool shapeDyn0 = contact.shape0->getActor().isDynamic();
			bool shapeDyn1 = contact.shape1->getActor().isDynamic();

			const NxTriangleMeshShape* triShape = nullptr;
			unsigned triInd = 0;
			if (contact.shape0->isTriangleMesh())
			{
				triShape = contact.shape0->isTriangleMesh();
				triInd = contact.featureIndex0;
			}
			else if (contact.shape1->isTriangleMesh())
			{
				triShape = contact.shape1->isTriangleMesh();
				triInd = contact.featureIndex1;
			}
			else
				return true;

			{
				//triangle for shape1
				NxTriangle tri;
				triShape->getTriangle(tri, nullptr, nullptr, triInd, true, true);
				NxVec3 triNorm;
				tri.normal(triNorm);
				//если цель является основным взаимодействующим лицом, то необходимо инверитровать нормаль
				if (contact.actorIndex == 0)
					triNorm = -triNorm;

				auto wFricMat = NxMat33(contact.data->localorientation0);
				if (shapeDyn0)
					NxQuatFromLocalToWorld(contact.shape0->getActor().getCMassGlobalPose().M, wFricMat, wFricMat);

				//вычислянм новый базис относительно дополнительной оси трения, берем localorientation1 в мировой системе координат
				NxVec3 secFric = wFricMat.getColumn(2);
				//вычисляем основную ось трения
				NxVec3 firstFric = secFric.cross(triNorm);
				//если secFric совпадает с нормалью, то вычислянм новый базис относительно основной оси трения
				//для наклонные повврехности до 45 градусов считаются не препятсвующими движению, т.е. по ним можно скользить (например по верхужкам прыжков)
				if (firstFric.magnitude() < 0.5f)
				{
					firstFric = wFricMat.getColumn(1);
					secFric = triNorm.cross(firstFric);
				}
				////если firstFric совпадает с нормалью, то оставляем старый базис
				if (secFric.magnitude() > 0.1f)
				{
					firstFric.normalize();
					secFric.normalize();
					//корректируем нормаль для ортоганальной системы
					triNorm = firstFric.cross(secFric);
					triNorm.normalize();
					//
					NxMat33 fricMat;
					fricMat.setColumn(0, triNorm);
					fricMat.setColumn(1, firstFric);
					fricMat.setColumn(2, secFric);
					NxQuat fricRot;
					fricMat.toQuat(fricRot);

					NxQuat rot0;
					//преобразуем в локльную систему координат тела актера
					if (shapeDyn0)
					{
						NxMat33 rot0Mat;
						NxQuatFromWorldToLocal(contact.shape0->getActor().getCMassGlobalPose().M, fricMat, rot0Mat);
						rot0Mat.toQuat(rot0);
					}
					else
						fricMat.toQuat(rot0);

					NxQuat rot1;
					//преобразуем в локльную систему координат тела актера
					if (shapeDyn1)
					{
						NxMat33 rot1Mat;
						NxQuatFromWorldToLocal(contact.shape1->getActor().getCMassGlobalPose().M, fricMat, rot1Mat);
						rot1Mat.toQuat(rot1);
					}
					else
						fricMat.toQuat(rot1);

					contact.data->localorientation0 = rot0;
					contact.data->localorientation1 = rot1;

					NxVec3 velocity = GetPxActor().GetNxActor()->getLinearVelocity();
					if (velocity.magnitude() > 0.1f)
					{
						velocity.normalize();
						float k = abs(velocity.dot(-triNorm));
						float friction = contact.data->dynamicFriction0;
						friction = friction + (-0.3f - friction) * ClampValue(k, 0.0f, 1.0f);

						//contact.data->dynamicFriction0 = friction;
						//contact.data->staticFriction0 = friction - 0.05f;
					}

					contact.data->dynamicFriction0 = 0;
					contact.data->staticFriction0 = 0;

					//changes
					(*contact.changeFlags) |= px::Scene::ContactModifyTraits::NX_CCC_LOCALORIENTATION0 |
						px::Scene::ContactModifyTraits::NX_CCC_LOCALORIENTATION1 |
						px::Scene::ContactModifyTraits::NX_CCC_STATICFRICTION0 |
						px::Scene::ContactModifyTraits::NX_CCC_DYNAMICFRICTION0;
				}

				//Contact pushContact;
				//pushContact.data = *contact.data;
				//pushContact.tri = tri;
				//pushContact.shape0 = contact.shape0;
				//pushContact.shape1 = contact.shape1;
				//contactList.push_back(pushContact);
			}

			return true;
		}

		void GameCar::OnContact(const px::Scene::OnContactEvent& contact)
		{
			const Player* human = GetMapObj()->GetPlayer();
			NxActor* nxActor = GetPxActor().GetNxActor();
			_bodyContact = true;

			const D3DXVECTOR2 touchBorderDamage = GetLogic() ? GetLogic()->GetTouchBorderDamage() : NullVec2;
			const D3DXVECTOR2 touchBorderDamageForce = GetLogic() ? GetLogic()->GetTouchBorderDamageForce() : NullVec2;
			const D3DXVECTOR2 touchCarDamage = GetLogic() ? GetLogic()->GetTouchCarDamage() : NullVec2;
			const D3DXVECTOR2 touchCarDamageForce = GetLogic() ? GetLogic()->GetTouchCarDamageForce() : NullVec2;

			GameObject* target = GetGameObjFromActor(contact.actor);
			const int targetPlayerId = target && target->GetMapObj() && target->GetMapObj()->GetPlayer()
				                           ? target->GetMapObj()->GetPlayer()->GetId()
				                           : cUndefPlayerId;
			const int senderPlayerId = GetMapObj() && GetMapObj()->GetPlayer()
				                           ? GetMapObj()->GetPlayer()->GetId()
				                           : cUndefPlayerId;

			const NxVec3 force = contact.pair->sumNormalForce;
			const float forceLength = contact.pair->sumNormalForce.magnitude();

			if (target && target->GetMapObj() && target->GetMapObj()->GetRecord() && target->GetMapObj()->GetRecord()->
				GetCategory() == MapObjLib::ctTrack)
			{
				float touchForceA = 0.0f;
				if (touchBorderDamageForce.y > touchBorderDamageForce.x)
					touchForceA = ClampValue(
						(forceLength - touchBorderDamageForce.x) / (touchBorderDamageForce.y - touchBorderDamageForce.
							x), 0.0f, 1.0f);
				else if (forceLength > touchBorderDamageForce.x)
					touchForceA = 0.5f;
				const float touchDamage = touchBorderDamage.x + (touchBorderDamage.y - touchBorderDamage.x) * touchForceA;

				//if (!target->GetMapObj()->GetPlayer()->GetHardBorders() && touchForceA == 0.0f)
				//	return;

				if (forceLength > 0.01f)
				{
					NxVec3 norm = force;
					norm.normalize();
					if (contact.actorIndex == 0)
						norm = -norm;

					NxVec3 vel = GetPxActor().GetNxActor()->getLinearVelocity();
					NxContactStreamIterator contIter(contact.stream);

					const bool borderContact = abs(norm.z) < 0.5f && ContainsContactGroup(
						contIter, contact.actorIndex, px::Scene::cdgShotTransparency);
					if (borderContact)
						_clutchTime = 0.0f;

					if (borderContact && vel.magnitude() > 8.0f) //20
					{
						if (target)
						{
							SetWastedControl(false);
							SetSpinStatus(false);
							SetRespBlock(false);
							InFly(false);
							if (human->GetBorderCluth()) //дополнительное сцепление с бордюром:
								GetPxActor().GetNxActor()->addForce(6.0f * px::Scene::cDefGravity, NX_ACCELERATION);
						}
						if (human->GetHardBorders())
						{
							//NxVec3 tang = contact.pair->sumFrictionForce;
							//tang.normalize();
							//tang = tang.cross(norm);
							//tang.normalize();

							NxVec3 tang = vel;
							tang.normalize();
							const float tangDot = abs(tang.dot(norm));

							auto dir = NxVec3(1.0f, 0.0f, 0.0f);
							GetPxActor().GetNxActor()->getGlobalOrientationQuat().rotate(dir);
							const float dirDot = dir.dot(norm);
							const float dirDot2 = dir.dot(tang);

							if (tangDot > 0.1f && (dirDot < 0.707f || dirDot2 < -0.707f))
							{
								if (tangDot < 0.995f)
								{
									//binormal
									tang = norm.cross(tang);

									if (tang.z > 0)
									{
										tang = contact.pair->sumFrictionForce;
										tang.z = abs(tang.z);
									}
									else
									{
										tang = contact.pair->sumFrictionForce;
										tang.z = -abs(tang.z);
									}

									tang.normalize();

									//tangent
									tang = tang.cross(norm);
								}
								else
									tang = NxVec3(0, 0, 0);

								float velN = norm.dot(vel);
								//if (dirDot < 0.707f && dirDot2)
								velN = ClampValue(abs(velN), 4.0f, 14.0f); //2, 12

								const float velT = tang.dot(vel) * 0.6f;
								vel = norm * velN + tang * velT;
								vel.z = 0.0f;
								nxActor->setLinearVelocity(vel);
							}
						}

						if (touchForceA > 0.0f && touchDamage > 0.0f)
							GetLogic()->Damage(this, senderPlayerId, this, touchDamage, dtTouch);
					}
				}
			}
			else if (target && target->GetMapObj() && target->GetMapObj()->GetRecord() && target->GetMapObj()->
				GetRecord()->GetCategory() == MapObjLib::ctCar)
			{
				const NxActor* nxTarget = contact.actor->GetNxActor();

				if (nxTarget && nxTarget->isDynamic())
				{
					const float myEnergy = nxActor->computeKineticEnergy();
					const float targetEnergy = nxTarget->computeKineticEnergy();

					float touchForceA = 0.0f;
					if (touchCarDamageForce.y > touchCarDamageForce.x)
						touchForceA = ClampValue(
							(forceLength - touchCarDamageForce.x) / (touchCarDamageForce.y - touchCarDamageForce.x),
							0.0f, 1.0f);
					else if (forceLength > touchCarDamageForce.x)
						touchForceA = 0.5f;
					const float touchDamage = touchCarDamage.x + (touchCarDamage.y - touchCarDamage.x) * touchForceA;

					if (touchForceA > 0.0f && touchDamage > 0.0f)
					{
						if (myEnergy > targetEnergy)
							GetLogic()->Damage(this, senderPlayerId, target,
							                   touchDamage * GetMapObj()->GetPlayer()->GetBlowDamage(), dtTouch);
						else
							GetLogic()->Damage(target, targetPlayerId, this,
							                   touchDamage * target->GetMapObj()->GetPlayer()->GetBlowDamage(),
							                   dtTouch);
					}
				}
			}
			else
			{
				target->Damage(senderPlayerId, 0.0f, dtTouch);
			}
		}

		void GameCar::OnPxSync(float alpha)
		{
			_MyBase::OnPxSync(alpha);

			for (const auto& _wheel : *_wheels)
				_wheel->PxSyncWheel(alpha);
		}

		void GameCar::OnProgress(float deltaTime)
		{
			_MyBase::OnProgress(deltaTime);

			for (auto& _wheel : *_wheels)
			{
				CarWheel* wheel = _wheel;

				wheel->OnProgress(deltaTime);
			}
		}

		void GameCar::OnFixedStep(float deltaTime)
		{
			_MyBase::OnFixedStep(deltaTime);

			NxActor* nxActor = GetPxActor().GetNxActor();

			LSL_ASSERT(nxActor);

			float curMotorTorque = 0;
			float curBreakTorque = 0;
			float curRPM = 0;
			_anyWheelContact = false;
			_wheelsContact = true;
			_bodyContact = false;

			if (_clutchTime > 0 && (_clutchTime -= deltaTime) < 0.0f)
				_clutchTime = 0.0f;

			if (_mineTime > 0 && (_mineTime -= deltaTime) < 0.0f)
				_mineTime = 0.0f;

			MotorProgress(deltaTime, curMotorTorque, curBreakTorque, curRPM);
			TransmissionProgress(deltaTime, curRPM);
			WheelsProgress(deltaTime, curMotorTorque, curBreakTorque);
			JumpProgress(deltaTime);
			SpinOutProgress(deltaTime);

			GetBehaviors().OnMotor(deltaTime, curRPM, static_cast<float>(_motor.idlingRPM),
			                       static_cast<float>(_motor.maxRPM));
		}

		GameCar* GameCar::IsCar()
		{
			return this;
		}

		void GameCar::LockClutch(float strength)
		{
			if (_clutchImmunity || _clutchTime > 0)
				return;

			_clutchTime = 0.38f;
			_clutchStrength = strength;
		}

		bool GameCar::IsClutchLocked() const
		{
			return _clutchTime > 0;
		}

		void GameCar::LockMine(float time)
		{
			_mineTime = time;
		}

		bool GameCar::IsMineLocked() const
		{
			return _mineTime > 0.0f;
		}

		int GameCar::GearUp()
		{
			if (_curGear < static_cast<int>(_motor.gears.size() - 1))
				return ++_curGear;
			return _curGear;
		}

		int GameCar::GearDown()
		{
			if (_curGear > -1)
				return --_curGear;
			return _curGear;
		}

		const CarMotorDesc& GameCar::GetMotorDesc() const
		{
			return _motor;
		}

		void GameCar::SetMotorDesc(const CarMotorDesc& value)
		{
			_motor = value;

			_curGear = std::min<int>(_curGear, value.gears.size());
		}

		GameCar::MoveCarState GameCar::GetMoveCar() const
		{
			return _moveCar;
		}

		void GameCar::SetMoveCar(MoveCarState value)
		{
			if (_moveCar != value)
			{
				_moveCar = value;

				if (_moveCar != mcNone && GetPxActor().GetNxActor())
					GetPxActor().GetNxActor()->wakeUp();
			}
		}

		GameCar::SteerWheelState GameCar::GetSteerWheel() const
		{
			return _steerWheel;
		}

		void GameCar::SetSteerWheel(SteerWheelState value)
		{
			_steerWheel = value;
		}

		float GameCar::GetSteerWheelAngle() const
		{
			return _steerAngle;
		}

		void GameCar::SetSteerWheelAngle(float value)
		{
			_steerAngle = ClampValue(value, -cMaxSteerAngle, cMaxSteerAngle);
		}

		int GameCar::GetCurGear() const
		{
			return _curGear;
		}

		void GameCar::SetCurGear(int value)
		{
			LSL_ASSERT(_moveCar != mcAccel || value != cBackGear);

			_curGear = ClampValue(value, cNeutralGear, static_cast<int>(_motor.gears.size()) - 1);
		}

		float GameCar::GetSpeed()
		{
			return GetSpeed(GetPxActor().GetNxActor(), GetGrActor().GetWorldDir());
		}

		float GameCar::GetLeadWheelSpeed() const
		{
			if (!_wheels->GetLeadGroup().empty())
			{
				const NxWheelShape* wheel = _wheels->GetLeadGroup().front()->GetShape()->GetNxShape();

				const float speed = wheel->getAxleSpeed() * wheel->getRadius();
				//погрешность 0.1 м/с
				return abs(speed) > 0.1f ? speed : 0.0f;
			}
			return 0;
		}

		float GameCar::GetDrivenWheelSpeed() const
		{
			const CarWheel* wheel = nullptr;

			for (const auto& _wheel : *_wheels)
				if (!_wheel->GetLead())
				{
					wheel = _wheel;
					break;
				}

			if (wheel)
			{
				const float speed = wheel->GetShape()->GetNxShape()->getAxleSpeed() * wheel->GetShape()->GetRadius();
				//погрешность 0.1 м/с
				return abs(speed) > 0.1f ? speed : 0.0f;
			}
			return 0;
		}

		float GameCar::GetRPM() const
		{
			return GetWheelRPM();
		}

		float GameCar::GetKSteerControl() const
		{
			return _kSteerControl;
		}

		void GameCar::SetKSteerControl(float value)
		{
			_kSteerControl = value;
		}

		float GameCar::GetSteerSpeed() const
		{
			return _steerSpeed;
		}

		void GameCar::SetSteerSpeed(float value)
		{
			_steerSpeed = value;
		}

		float GameCar::GetSteerRot() const
		{
			return _steerRot;
		}

		void GameCar::SetSteerRot(float value)
		{
			_steerRot = value;
		}

		D3DXVECTOR3 GameCar::GetAngDamping() const
		{
			return _angDamping;
		}

		void GameCar::SetAngDamping(D3DXVECTOR3 value)
		{
			_angDamping = value;
		}

		float GameCar::GetFlyYTorque() const
		{
			return _flyYTorque;
		}

		void GameCar::SetFlyYTourque(float value)
		{
			_flyYTorque = value;
		}

		float GameCar::GetClampXTorque() const
		{
			return _clampXTorque;
		}

		void GameCar::SetClampXTourque(float value)
		{
			_clampXTorque = value;
		}

		float GameCar::GetClampYTorque() const
		{
			return _clampYTorque;
		}

		void GameCar::SetClampYTourque(float value)
		{
			_clampYTorque = value;
		}

		float GameCar::GetMotorTorqueK() const
		{
			return _motorTorqueK;
		}

		void GameCar::SetMotorTorqueK(float value)
		{
			_motorTorqueK = value;
		}

		float GameCar::GetWheelSteerK() const
		{
			return _wheelSteerK;
		}

		void GameCar::SetWheelSteerK(float value)
		{
			if (_wheelSteerK == value)
				return;

			_wheelSteerK = value;
			ApplyWheelSteerK();
		}

		bool GameCar::IsGravEngine() const
		{
			return _gravEngine;
		}

		void GameCar::SetGravEngine(bool value)
		{
			_gravEngine = value;
		}

		bool GameCar::IsClutchImmunity() const
		{
			return _clutchImmunity;
		}

		void GameCar::SetClutchImmunity(bool value)
		{
			_clutchImmunity = value;
		}

		float GameCar::GetMaxSpeed() const
		{
			return _maxSpeed;
		}

		void GameCar::SetMaxSpeed(float value)
		{
			_maxSpeed = value;
		}

		float GameCar::GetTireSpring() const
		{
			return _tireSpring;
		}

		void GameCar::SetTireSpring(float value)
		{
			_tireSpring = value;
		}

		bool GameCar::GetDisableColor() const
		{
			return _disableColor;
		}

		void GameCar::SetDisableColor(bool value)
		{
			_disableColor = value;
		}

		bool GameCar::InSpinStatus() const
		{
			return _spinstatus;
		}

		void GameCar::SetSpinStatus(bool value)
		{
			_spinstatus = value;
		}

		bool GameCar::GetRespBlock() const
		{
			return _resplocked;
		}

		void GameCar::SetRespBlock(bool value)
		{
			_resplocked = value;
		}

		bool GameCar::GetWastedControl() const
		{
			return _wasted;
		}

		void GameCar::SetWastedControl(bool value)
		{
			_wasted = value;
		}

		bool GameCar::IsBurn() const
		{
			return _burn;
		}

		void GameCar::SetBurn(bool value)
		{
			_burn = value;
		}

		bool GameCar::IsUlimitedTurn() const
		{
			return _unlimitedTurn;
		}

		void GameCar::SetUnlimitedTurn(bool value)
		{
			_unlimitedTurn = value;
		}

		bool GameCar::DoubleJumpIsActive() const
		{
			return _doublej;
		}

		void GameCar::DoubleJumpSetActive(bool value)
		{
			_doublej = value;
		}

		float GameCar::GetTurnForce() const
		{
			return _turnForce;
		}

		void GameCar::SetTurnForce(float value)
		{
			_turnForce = value;
		}

		float GameCar::BackSpeedK() const
		{
			return _backSpeedK;
		}

		void GameCar::BackSpeedK(float value)
		{
			_backSpeedK = value;
		}

		float GameCar::GetBurnDamage() const
		{
			return _burnDamage;
		}

		void GameCar::SetBurnDamage(float value)
		{
			_burnDamage = value;
		}

		bool GameCar::GetTurnFreeze() const
		{
			return _turnfreeze;
		}

		void GameCar::SetTurnFreeze(bool value)
		{
			_turnfreeze = value;
		}

		void GameCar::SetTurnInverse(bool value)
		{
			_turnInverse = value;
		}

		bool GameCar::GetTurnInverse() const
		{
			return _turnInverse;
		}

		void GameCar::SetInverseTime(float value)
		{
			_inverseTime = value;
		}

		float GameCar::GetInverseTime() const
		{
			return _inverseTime;
		}

		int GameCar::GetLastMoveState() const
		{
			return _LMS;
		}

		void GameCar::SetLastMoveState(int value)
		{
			_LMS = value;
		}

		bool GameCar::GetDamageStop() const
		{
			return _isDamageStop;
		}

		void GameCar::SetDamageStop(bool value)
		{
			_isDamageStop = value;
		}

		float GameCar::GetMaxTimeSR() const
		{
			return _maxTimeSR;
		}

		void GameCar::SetMaxTimeSR(float value)
		{
			_maxTimeSR = value;
		}

		float GameCar::GetDeminingTime() const
		{
			return _deminingTime;
		}

		void GameCar::SetDeminingTime(float value)
		{
			_deminingTime = value;
		}

		bool GameCar::GetSlowRide() const
		{
			return _slowride;
		}

		void GameCar::SetSlowRide(bool value)
		{
			_slowride = value;
		}

		bool GameCar::GetDemining() const
		{
			return _demining;
		}

		void GameCar::SetDemining(bool value)
		{
			_demining = value;
		}

		bool GameCar::OnJump() const
		{
			return _onJump;
		}

		void GameCar::OnJump(bool value)
		{
			_onJump = value;
		}

		bool GameCar::GoAiJump() const
		{
			return _goAiJump;
		}

		void GameCar::GoAiJump(bool value)
		{
			_goAiJump = value;
		}

		bool GameCar::InFly() const
		{
			return _inFly;
		}

		void GameCar::InFly(bool value)
		{
			_inFly = value;
		}

		bool GameCar::LockAIJump() const
		{
			return _lockAJmp;
		}

		void GameCar::LockAIJump(bool value)
		{
			_lockAJmp = value;
		}

		bool GameCar::InRage() const
		{
			return _inRage;
		}

		void GameCar::InRage(bool value)
		{
			_inRage = value;
		}

		bool GameCar::Invisible() const
		{
			return _invisible;
		}

		void GameCar::Invisible(bool value)
		{
			_invisible = value;
		}

		bool GameCar::IsStabilityMine() const
		{
			return _isStabilityMine;
		}

		void GameCar::StabilityMine(bool value)
		{
			_isStabilityMine = value;
		}

		bool GameCar::IsStabilityShot() const
		{
			return _isStabilityShot;
		}

		void GameCar::StabilityShot(bool value)
		{
			_isStabilityShot = value;
		}


		bool GameCar::GetGhostEff() const
		{
			return _ghostEff;
		}

		void GameCar::SetGhostEff(bool value)
		{
			_ghostEff = value;
		}

		bool GameCar::IsAnyWheelContact() const
		{
			return _anyWheelContact;
		}

		bool GameCar::IsWheelsContact() const
		{
			return _wheelsContact;
		}

		bool GameCar::IsBodyContact() const
		{
			return _bodyContact;
		}

		GameCar::Wheels& GameCar::GetWheels() const
		{
			return *_wheels;
		}

		float GameCar::GetSpeed(NxActor* nxActor, const D3DXVECTOR3& dir)
		{
			if (nxActor)
			{
				float speed = D3DXVec3Dot(&dir, &D3DXVECTOR3(nxActor->getLinearVelocity().get()));
				//погрешность 1 м/с
				if (abs(speed) < 1.0f)
					speed = 0.0f;

				return speed;
			}

			return 0.0f;
		}


		DestrObj::DestrObj(): _checkDestruction(false)
		{
			_destrList = new DestrList(this);

			GetPxActor().SetFlag(NX_AF_DISABLE_RESPONSE, true);
		}

		DestrObj::~DestrObj()
		{
			delete _destrList;
		}

		void DestrObj::OnDeath(GameObject* sender, DamageType damageType, GameObject* target)
		{
			_MyBase::OnDeath(sender, damageType, target);

			_checkDestruction = true;
		}

		void DestrObj::SaveSource(SWriter* writer)
		{
			_MyBase::SaveSource(writer);

			writer->WriteValue("destrList", _destrList);
		}

		void DestrObj::LoadSource(SReader* reader)
		{
			_MyBase::LoadSource(reader);

			reader->ReadValue("destrList", _destrList);
		}

		void DestrObj::OnProgress(float deltaTime)
		{
			_MyBase::OnProgress(deltaTime);

			_destrList->OnProgress(deltaTime);

			if (_checkDestruction)
			{
				_checkDestruction = false;

				for (auto& iter : *_destrList)
				{
					iter->GetGameObj().SetParent(nullptr);
					iter->GetGameObj().SetOwner(nullptr);
					iter->SetName("");
					GetLogic()->GetMap()->InsertMapObj(iter);

					GameObject& gameObj = iter->GetGameObj();
					gameObj.SetWorldPos(GetWorldPos());
					gameObj.SetWorldRot(GetWorldRot());
				}

				_destrList->LockDestr();
				_destrList->LockNotify();
				try
				{
					_destrList->Clear();
				}
				LSL_FINALLY(_destrList->UnlockDestr(); _destrList->UnlockNotify();)
			}
		}

		DestrObj::DestrList& DestrObj::GetDestrList() const
		{
			return *_destrList;
		}
	}
}
