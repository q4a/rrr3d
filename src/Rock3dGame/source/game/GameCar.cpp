#include "stdafx.h"
#include "game\\GameCar.h"

#include "game\\Logic.h"
#include "game\\Race.h"

namespace r3d
{

namespace game
{

const float GameCar::cMaxSteerAngle = D3DX_PI / 6;	




CarMotorDesc::CarMotorDesc(): maxRPM(7000), idlingRPM(1000), maxTorque(2000.0f), SEM(0.7f), gearDiff(3.42f), autoGear(true), brakeTorque(7500), restTorque(400.0f)
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
	else		
	{
		float rpm = abs(wheelAxleSpeed) * gears[curGear] * gearDiff * 60.0f / (2.0f * D3DX_PI);

		return std::min(rpm, static_cast<float>(maxRPM));
	}
}

float CarMotorDesc::CalcTorque(float rpm, unsigned curGear) const
{
	const float cGameK = 1.15f;

	return maxTorque * gears[curGear] * gearDiff * SEM * cGameK;
}




CarWheel::CarWheel(CarWheels* owner): _owner(owner), _wheelShape(0), _summAngle(0), _trailEff(0), _actTrail(0), invertWheel(false), _steerAngle(0), _lead(false), _steer(false), _offset(NullVector), _pxPrevPos(NullVector), _pxPrevRot(NullQuaternion)
{
	_myContactModify = new MyContactModify(this);
	_trails = new _Trails(this);	
}

CarWheel::~CarWheel()
{
	SetSteer(false);
	SetLead(false);

	lsl::SafeRelease(_actTrail);
	delete _trails;
	SetTrailEff(0);

	DestroyWheelShape();
	delete _myContactModify;
}

CarWheel::MyContactModify::MyContactModify(CarWheel* wheel): _wheel(wheel)
{	
}

bool CarWheel::MyContactModify::onWheelContact(NxWheelShape* wheelShape, NxVec3& contactPoint, NxVec3& contactNormal, NxReal& contactPosition, NxReal& normalForce, NxShape* otherShape, NxMaterialIndex& otherShapeMaterialIndex, NxU32 otherShapeFeatureIndex)
{
	float normReaction = abs(wheelShape->getActor().getMass() * contactNormal.dot(px::Scene::cDefGravity) / _wheel->_owner->Size());

	if (_wheel->_owner->GetOwner()->IsClutchLocked())
	{
		normalForce = 0.0f;
	}
	else
	{	
		float tireSpring = _wheel->_owner->GetOwner()->GetTireSpring();
		float nReac = normalForce/normReaction;
		_wheel->_nReac = nReac;
		
		//Ограничение допустимой перегрузки, чтобы машина при падении не ускорялась в связи с чрезмерно больгим сцеплением. Допустимая перегрузка в 1.5g.
		if (tireSpring > 0.0f && nReac > tireSpring)
			normalForce = 0.0f;
		else
			normalForce = std::min(normalForce, normReaction * 1.5f); //1.5f g
	}

	return true;
}

void CarWheel::Save(lsl::SWriter* writer)
{
	_MyBase::Save(writer);

	writer->WriteValue("shape", _wheelShape->GetName());
	MapObjRec::Lib::SaveRecordRef(writer, "trailEff", _trailEff);
	
	writer->WriteValue("lead", _lead);
	writer->WriteValue("steer", _steer);
	lsl::SWriteValue(writer, "offset", _offset);
	writer->WriteValue("invertWheel", invertWheel);	
}

void CarWheel::Load(lsl::SReader* reader)
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
		lsl::Error("CarWheel::Load");

	SetTrailEff(MapObjRec::Lib::LoadRecordRef(reader, "trailEff"));

	bool lead;
	reader->ReadValue("lead", lead);
	SetLead(lead);
	
	bool steer;
	reader->ReadValue("steer", steer);
	SetSteer(steer);

	lsl::SReadValue(reader, "offset", _offset);

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
	_wheelShape = 0;
	
}

void CarWheel::PxSyncWheel(float alpha)
{
	NxWheelShape* wheel = _wheelShape->GetNxShape();
	LSL_ASSERT(wheel);

	NxReal st = wheel->getSuspensionTravel();
	NxReal r = wheel->getRadius();			
	NxMat34 localPose = wheel->getLocalPose();
	//cast along -Y	
	NxVec3 dir = localPose.M.getColumn(1);
	NxVec3 t = localPose.t;

	NxWheelContactData wcd;
	//cast from shape origin
	NxShape* s = wheel->getContact(wcd);
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

	NxWheelShape* wheel = _wheelShape->GetNxShape();
	if (wheel)
		_summAngle += wheel->getAxleSpeed() * deltaTime;

	_trails->OnProgress(deltaTime);

	if (_trailEff)
	{
		NxWheelContactData contactDesc;
		NxShape* contact = GetShape()->GetNxShape()->getContact(contactDesc);
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
			lsl::SafeRelease(_actTrail);
	}
}

px::WheelShape* CarWheel::GetShape()
{
	return _wheelShape;
}

game::MapObjRec* CarWheel::GetTrailEff()
{
	return _trailEff;
}

void CarWheel::SetTrailEff(game::MapObjRec* value)
{
	if (ReplaceRef(_trailEff, value))
		_trailEff = value;
}

float CarWheel::GetLongSlip()
{
	LSL_ASSERT(GetShape());

	NxWheelContactData contactDesc;
	NxShape* contact = GetShape()->GetNxShape()->getContact(contactDesc);
	
	return contact ? contactDesc.longitudalSlip : 0;
}

float CarWheel::GetLatSlip()
{
	NxWheelContactData contactDesc;
	NxShape* contact = GetShape()->GetNxShape()->getContact(contactDesc);
	
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

const D3DXVECTOR3& CarWheel::GetOffset() const
{
	return _offset;
}

void CarWheel::SetOffset(const D3DXVECTOR3& value)
{
	_offset = value;
}




CarWheels::CarWheels(GameCar* owner): _owner(owner), _steerContactModify(0)
{
}

CarWheels::~CarWheels()
{
	SetSteerContactModify(0);
}

void CarWheels::LoadPosTo(const std::string& fileName, std::vector<D3DXVECTOR3>& pos)
{
	std::istream* file = lsl::FileSystem::GetInstance()->NewInStream(fileName, lsl::FileSystem::omText, 0);
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

	return _MyBase::AddItem(wheel);
}

GameCar* CarWheels::GetOwner()
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

CarWheels::ContactModify* CarWheels::GetSteerContactModify()
{
	return _steerContactModify;
}

void CarWheels::SetSteerContactModify(ContactModify* value)
{
	if (lsl::Object::ReplaceRef(_steerContactModify, value))
	{
		_steerContactModify = value;
		for (WheelGroup::iterator iter = _steerGroup.begin(); iter != _steerGroup.end(); ++iter)
		{
			(*iter)->GetShape()->SetContactModify(value);
			return;
		}
	}
}




GameCar::GameCar(): _clutchStrength(0), _clutchTime(0.0f), _springTime(0), _mineTime(0), _curGear(-1), _moveCar(mcNone), _steerWheel(swNone), _kSteerControl(1.0f), _steerSpeed(D3DX_PI/2.0f), _steerRot(D3DX_PI), _angDamping(IdentityVector), _flyYTorque(D3DX_PI/1.6f), _clampXTorque(0), _clampYTorque(0), _motorTorqueK(1), _wheelSteerK(1), _gravEngine(false), _clutchImmunity(false), _maxSpeed(0), _tireSpring(0), _disableColor(false), _steerAngle(0), _anyWheelContact(false), _wheelsContact(false), _bodyContact(false)
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
	switch (_moveCar)
	{
	case mcNone:
		curRPM = GetWheelRPM();
		curBreakTorque = _motor.restTorque;
		curMotorTorque = 0;
		break;			
		
	case mcBrake:
		SetCurGear(cNeutralGear);
		curRPM = GetWheelRPM();
		curBreakTorque = _motor.brakeTorque;
		curMotorTorque = 0;		
		break;

	case mcBack:
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
			if (curRPM < _motor.maxRPM)
				curMotorTorque = -static_cast<float>(_motor.CalcTorque(curRPM, _curGear));
		}
		break;

	case mcAccel:
		if (GetSpeed() < 0)
		{
			SetCurGear(cNeutralGear);
			curRPM = GetWheelRPM();
			curBreakTorque = _motor.brakeTorque;
			curMotorTorque = 0;
		}
		else
		{
			if (_curGear == cNeutralGear ||  _curGear == cBackGear)
				SetCurGear(1);				

			curRPM = GetWheelRPM();
			curBreakTorque = _motor.restTorque;
			curMotorTorque = _motor.CalcTorque(curRPM, _curGear) * _motorTorqueK;
		}
		break;
	}
}

inline float NxQuatAngle(const NxQuat& quat1, const NxQuat& quat2)
{	
	return acos(abs(quat1.dot(quat2)/sqrt(quat1.magnitudeSquared() * quat2.magnitudeSquared()))) * 2;
}

inline const void NxQuatRotation(NxQuat& quat, const NxQuat& quat1, const NxQuat& quat2)
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
			_steerAngle = _steerSpeed > 0 ? std::min(std::max(_steerAngle, 0.0f) + _steerSpeed * deltaTime, cMaxSteerAngle) : cMaxSteerAngle;
		else if (_steerWheel == swOnRight)
			_steerAngle = _steerSpeed > 0 ? std::max(std::min(_steerAngle, 0.0f) - _steerSpeed * deltaTime, -cMaxSteerAngle) : -cMaxSteerAngle;
		else
			_steerAngle = 0;
	}

	bool anyLeadWheelContact = false;
	CarWheel* backWheel = NULL;

	for (Wheels::iterator iter = _wheels->begin(); iter != _wheels->end(); ++iter)
	{
		CarWheel* wheel = *iter;
		NxWheelShape* pxWheel = wheel->GetShape()->GetNxShape();

		pxWheel->setBrakeTorque(breakTorque);

		if ((*iter)->GetLead())
		{
			pxWheel->setMotorTorque(motorTorque);
		}
		if ((*iter)->GetSteer())
		{			
			(*iter)->SetSteerAngle(_steerAngle);
		}
		if ((*iter)->GetPos().x < 0)
			backWheel = *iter;

		NxWheelContactData data;
		if (pxWheel->getContact(data))
		{
			_anyWheelContact = true;

			if ((*iter)->GetLead())
				anyLeadWheelContact = true;
		}
		else
			_wheelsContact = false;
	}

	StabilizeForce(deltaTime);

	//static float dFixAngle = 0;
	//static NxVec3 dFixAxis;
	//static SteerWheelState lastSteer = swNone;

	LSL_ASSERT(backWheel);

	if (((_gravEngine && _anyWheelContact) || (!_gravEngine && anyLeadWheelContact)) && _steerAngle != 0 && !IsClutchLocked()) //_steerWheel != smManual
	{
		float alpha = lsl::ClampValue(speed/10.0f, -1.0f, 1.0f);
		//alpha = 1.0f;

		NxMat34 worldMat = nxActor->getGlobalPose();
		NxQuat rotQuat;
		rotQuat.fromAngleAxisFast(alpha * _steerAngle/cMaxSteerAngle * _steerRot * deltaTime, NxVec3(0, 0, 1));
		NxMat34 rotMat;
		rotMat.M.fromQuat(rotQuat);
		NxMat34 matOffs1;
		matOffs1.t = NxVec3(backWheel->GetPos().x, 0, 0);
		NxMat34 matOffs2;
		matOffs2.t = -matOffs1.t;
		worldMat = worldMat * matOffs1 * rotMat * matOffs2;

		nxActor->setGlobalPose(worldMat);

		//if (_steerWheel != swNone)
		//	lastSteer = _steerWheel;
	}
	/*else if (lastSteer != swNone)
	{
		//const float cFixAngleStep = 15.0f * D3DX_PI / 180.0f;
		const float cFixAngleStep = 22.5f * D3DX_PI / 180.0f;

		NxQuat fixRot;
		fixRot.fromAngleAxisFast(0, NxVec3(0, 0, 1));
		NxQuat worldRot = nxActor->getGlobalOrientationQuat();
		NxQuatRotation(fixRot, fixRot, worldRot);

		D3DXQUATERNION rot;
		fixRot.getXYZW(rot);
		D3DXVECTOR3 xAxis;
		D3DXQuaternionToAxisAngle(&rot, &xAxis, &dFixAngle);
		dFixAxis.set(xAxis);
		dFixAxis.normalize();

		if ((dFixAxis.z > 0 && lastSteer == swOnLeft) || (dFixAxis.z < 0 && lastSteer == swOnRight))
			dFixAngle = Ceil<float>(dFixAngle / cFixAngleStep) * cFixAngleStep - dFixAngle;
		else
			dFixAngle = Floor<float>(dFixAngle / cFixAngleStep) * cFixAngleStep - dFixAngle;

		//dFixAngle = Round<float>(dFixAngle / cFixAngleStep) * cFixAngleStep - dFixAngle;

		lastSteer = swNone;
	}
	else if (dFixAngle != 0)
	{
		float dFixAngleNew = 0;
		if (dFixAngle > 0)
			dFixAngleNew = std::max(dFixAngle - _steerRot * deltaTime, 0.0f);
		else
			dFixAngleNew = std::min(dFixAngle + _steerRot * deltaTime, 0.0f);

		NxQuat dRot;
		dRot.fromAngleAxisFast(dFixAngle - dFixAngleNew, dFixAxis);
		nxActor->setGlobalOrientationQuat(nxActor->getGlobalOrientationQuat() * dRot);

		dFixAngle = dFixAngleNew;
	}*/
}

void GameCar::TransmissionProgress(float deltaTime, float curRPM)
{
	NxWheelContactData contact;
	if (GetWheelContactData(contact) == 0)
		return;

	if (_curGear != cNeutralGear &&  _curGear != cBackGear)
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

	_springTime = std::max(_springTime - deltaTime, 0.0f);

	if (!_anyWheelContact)
	{
		//если нет контакта необходжимо машину замедлять и быстро приземлять, чтобы она не улетала на прыжках в космос
		//nxActor->setLinearDamping(0.4f);
		nxActor->addForce(1.0f * px::Scene::cDefGravity, NX_ACCELERATION);
		
		//наклоняем машину вперед если она движется в OXY
		NxVec3 vel = nxActor->getLinearVelocity();
		vel.z = 0.0f;
		if (vel.magnitude() > 1.0f && _flyYTorque != 0 && _springTime == 0.0f)
			nxActor->addLocalTorque(NxVec3(0, _flyYTorque, 0), NX_ACCELERATION);
	}
}

void GameCar::StabilizeForce(float deltaTime)
{
	NxVec3 angMomentum = GetPxActor().GetNxActor()->getAngularMomentum();	
	NxMat33 mat = GetPxActor().GetNxActor()->getGlobalOrientation();	
	NxMat33 invMat;

	if ((_angDamping.x != -1 || _angDamping.y != -1 || _angDamping.z != -1 || _clampYTorque > 0 || _clampXTorque > 0 || IsClutchLocked()) && mat.getInverse(invMat))
	{
		angMomentum = invMat * angMomentum;		
		if (!_anyWheelContact && _clampYTorque > 0)
		{
			float angY = std::min(abs(angMomentum.y), GetPxActor().GetNxActor()->getMassSpaceInertiaTensor().y * _clampYTorque * 2.0f);
			angMomentum.y = angMomentum.y >= 0 ? angY : -angY;
		}
		if (!_anyWheelContact && _clampXTorque > 0)
		{
			float angX = std::min(abs(angMomentum.x), GetPxActor().GetNxActor()->getMassSpaceInertiaTensor().x * _clampXTorque * 2.0f);
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

			EulerAngles angles = Eul_FromQuat(*(Quat*)&rot, EulOrdXYZs);
			if (_clampXTorque > 0)
				angles.x = lsl::ClampValue(angles.x, -_clampXTorque, _clampXTorque);
			if (_clampYTorque > 0)
				angles.y = lsl::ClampValue(angles.y, -_clampYTorque, _clampYTorque);
			rot.setXYZW((float*)&Eul_ToQuat(angles));

			GetPxActor().GetNxActor()->setGlobalOrientationQuat(rot);
		}
	}

	GetPxActor().GetNxActor()->setAngularMomentum(angMomentum);
}

float GameCar::GetWheelRPM() const
{
	LSL_ASSERT(!_wheels->GetLeadGroup().empty());

	NxWheelShape* wheel = _wheels->GetLeadGroup().front()->GetShape()->GetNxShape();

	return _motor.CalcRPM(wheel->getAxleSpeed(), _curGear);
}

NxShape* GameCar::GetWheelContactData(NxWheelContactData& contact)
{
	LSL_ASSERT(!_wheels->GetLeadGroup().empty());

	NxWheelShape* wheel = _wheels->GetLeadGroup().front()->GetShape()->GetNxShape();

	return wheel->getContact(contact);
}

void GameCar::ApplyWheelSteerK()
{
	for (CarWheels::const_iterator iter = GetWheels().begin(); iter != GetWheels().end(); ++iter)
	{
		if ((*iter)->GetShape() == NULL || (*iter)->GetShape()->GetNxShape() == NULL)
			continue;

		NxTireFunctionDesc desc = (*iter)->GetShape()->GetLateralTireForceFunction();
		desc.asymptoteValue *= _wheelSteerK;
		desc.extremumValue *= _wheelSteerK;
		(*iter)->GetShape()->GetNxShape()->setLateralTireForceFunction(desc);
	}
}

void GameCar::SaveSource(lsl::SWriter* writer)
{
	_MyBase::SaveSource(writer);

	{
		lsl::SWriter* child = writer->NewDummyNode("motor");
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

		lsl::SWriteValue(writer, "angDamping", _angDamping);		
	}

	writer->WriteValue("wheels", _wheels);	
}

void GameCar::LoadSource(lsl::SReader* reader)
{
	_MyBase::LoadSource(reader);

	if (lsl::SReader* child = reader->ReadValue("motor"))
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

		lsl::SReadValue(reader, "angDamping", _angDamping);
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

	GameObject* target =GetGameObjFromActor(contact.actor);

	if (!(target && target->GetMapObj() && target->GetMapObj()->GetRecord() && target->GetMapObj()->GetRecord()->GetCategory() == MapObjLib::ctTrack))
		return true;

	bool shapeDyn0 = contact.shape0->getActor().isDynamic();
	bool shapeDyn1 = contact.shape1->getActor().isDynamic();

	const NxTriangleMeshShape* triShape = 0;
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
		triShape->getTriangle(tri, 0, 0, triInd, true, true);
		NxVec3 triNorm;
		tri.normal(triNorm);
		//если цель является основным взаимодействующим лицом, то необходимо инверитровать нормаль
		if (contact.actorIndex == 0)
			triNorm = -triNorm;
		
		NxMat33 wFricMat = NxMat33(contact.data->localorientation0);
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
				friction = friction + (-0.3f - friction) * lsl::ClampValue(k, 0.0f, 1.0f);

				//contact.data->dynamicFriction0 = friction;
				//contact.data->staticFriction0 = friction - 0.05f;
			}
			
			contact.data->dynamicFriction0 = 0;
			contact.data->staticFriction0 = 0;

			//changes
			(*contact.changeFlags) |= px::Scene::ContactModifyTraits::NX_CCC_LOCALORIENTATION0 | px::Scene::ContactModifyTraits::NX_CCC_LOCALORIENTATION1 | px::Scene::ContactModifyTraits::NX_CCC_STATICFRICTION0 | px::Scene::ContactModifyTraits::NX_CCC_DYNAMICFRICTION0;
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
	NxActor* nxActor = GetPxActor().GetNxActor();
	_bodyContact = true;
	bool springBorders = GetLogic() && GetLogic()->GetRace()->GetSpringBorders();

	D3DXVECTOR2 touchBorderDamage = GetLogic() ? GetLogic()->GetTouchBorderDamage() : NullVec2;
	D3DXVECTOR2 touchBorderDamageForce = GetLogic() ? GetLogic()->GetTouchBorderDamageForce() : NullVec2;
	D3DXVECTOR2 touchCarDamage = GetLogic() ? GetLogic()->GetTouchCarDamage() : NullVec2;
	D3DXVECTOR2 touchCarDamageForce = GetLogic() ? GetLogic()->GetTouchCarDamageForce() : NullVec2;

	GameObject* target = GetGameObjFromActor(contact.actor);
	int targetPlayerId = target && target->GetMapObj() && target->GetMapObj()->GetPlayer() ? target->GetMapObj()->GetPlayer()->GetId() : cUndefPlayerId;
	int senderPlayerId = GetMapObj() && GetMapObj()->GetPlayer() ? GetMapObj()->GetPlayer()->GetId() : cUndefPlayerId;	
	
	NxVec3 force = contact.pair->sumNormalForce;
	float forceLength = contact.pair->sumNormalForce.magnitude();

	if (target && target->GetMapObj() && target->GetMapObj()->GetRecord() && target->GetMapObj()->GetRecord()->GetCategory() == MapObjLib::ctTrack)
	{
		float touchForceA = 0.0f;
		if (touchBorderDamageForce.y > touchBorderDamageForce.x)
			touchForceA = lsl::ClampValue((forceLength - touchBorderDamageForce.x) / (touchBorderDamageForce.y - touchBorderDamageForce.x), 0.0f, 1.0f);
		else if (forceLength > touchBorderDamageForce.x)
			touchForceA = 0.5f;
		float touchDamage = touchBorderDamage.x + (touchBorderDamage.y - touchBorderDamage.x) * touchForceA;

		if (!springBorders && touchForceA == 0.0f)
			return;

		if (forceLength > 0.01f)
		{
			NxVec3 norm = force;
			norm.normalize();
			if (contact.actorIndex == 0)
				norm = -norm;

			NxVec3 vel = GetPxActor().GetNxActor()->getLinearVelocity();			
			NxContactStreamIterator contIter(contact.stream);

			bool borderContact = abs(norm.z) < 0.5f && ContainsContactGroup(contIter, contact.actorIndex, px::Scene::cdgShotTransparency);
			if (borderContact)
				_clutchTime = 0.0f;

			if (borderContact && vel.magnitude() > 16.0f) //20
			{
				if (springBorders)
				{
					//NxVec3 tang = contact.pair->sumFrictionForce;
					//tang.normalize();
					//tang = tang.cross(norm);
					//tang.normalize();

					NxVec3 tang = vel;
					tang.normalize();
					float tangDot = abs(tang.dot(norm));

					NxVec3 dir = NxVec3(1.0f, 0.0f, 0.0f);
					GetPxActor().GetNxActor()->getGlobalOrientationQuat().rotate(dir);
					float dirDot = dir.dot(norm);
					float dirDot2 = dir.dot(tang);

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
							velN = lsl::ClampValue(abs(velN), 4.0f, 14.0f); //2, 12

						float velT = tang.dot(vel) * 0.5f;
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
	else if (target && target->GetMapObj() && target->GetMapObj()->GetRecord() && target->GetMapObj()->GetRecord()->GetCategory() == MapObjLib::ctCar)
	{
		NxActor* nxTarget = contact.actor->GetNxActor();

		if (nxTarget && nxTarget->isDynamic())
		{
			float myEnergy = nxActor->computeKineticEnergy();
			float targetEnergy = nxTarget->computeKineticEnergy();

			float touchForceA = 0.0f;
			if (touchCarDamageForce.y > touchCarDamageForce.x)
				touchForceA = lsl::ClampValue((forceLength - touchCarDamageForce.x) / (touchCarDamageForce.y - touchCarDamageForce.x), 0.0f, 1.0f);
			else if (forceLength > touchCarDamageForce.x)
				touchForceA = 0.5f;
			float touchDamage = touchCarDamage.x + (touchCarDamage.y - touchCarDamage.x) * touchForceA;

			if (touchForceA > 0.0f && touchDamage > 0.0f)
			{
				if (myEnergy > targetEnergy)
					GetLogic()->Damage(this, senderPlayerId, target, touchDamage, dtTouch);
				else
					GetLogic()->Damage(target, targetPlayerId, this, touchDamage, dtTouch);
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

	for (Wheels::iterator iter = _wheels->begin(); iter != _wheels->end(); ++iter)
		(*iter)->PxSyncWheel(alpha);
}

void GameCar::OnProgress(float deltaTime)
{
	_MyBase::OnProgress(deltaTime);

	for (Wheels::iterator iter = _wheels->begin(); iter != _wheels->end(); ++iter)
	{
		CarWheel* wheel = *iter;		

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

	GetBehaviors().OnMotor(deltaTime, curRPM, static_cast<float>(_motor.idlingRPM), static_cast<float>(_motor.maxRPM));	
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

void GameCar::LockSpring()
{
	_springTime = 1.5f;
}

bool GameCar::IsSpringLocked() const
{
	return _springTime > 0;
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
	else
		return _curGear;
}

int GameCar::GearDown()
{
	if (_curGear > -1)
		return --_curGear;
	else
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

float GameCar::GetSteerWheelAngle()
{
	return _steerAngle;
}

void GameCar::SetSteerWheelAngle(float value)
{
	_steerAngle = lsl::ClampValue(value, -cMaxSteerAngle, cMaxSteerAngle);
}

int GameCar::GetCurGear() const
{
	return _curGear;
}

void GameCar::SetCurGear(int value)
{
	LSL_ASSERT(_moveCar != mcAccel || value != cBackGear);

	_curGear = lsl::ClampValue(value, cNeutralGear, (int)_motor.gears.size() - 1);
}

float GameCar::GetSpeed()
{
	return GetSpeed(GetPxActor().GetNxActor(), GetGrActor().GetWorldDir());
}

float GameCar::GetLeadWheelSpeed()
{
	if (!_wheels->GetLeadGroup().empty())
	{
		NxWheelShape* wheel = _wheels->GetLeadGroup().front()->GetShape()->GetNxShape();
		
		float speed = wheel->getAxleSpeed() * wheel->getRadius();
		//погрешность 0.1 м/с
		return abs(speed) > 0.1f ? speed : 0.0f;
	}
	else
		return 0;
}

float GameCar::GetDrivenWheelSpeed()
{
	CarWheel* wheel = 0;

	for (Wheels::iterator iter = _wheels->begin(); iter != _wheels->end(); ++iter)
		if (!(*iter)->GetLead())
		{
			wheel = *iter;
			break;
		}

	if (wheel)
	{
		float speed = wheel->GetShape()->GetNxShape()->getAxleSpeed() * wheel->GetShape()->GetRadius();		
		//погрешность 0.1 м/с
		return abs(speed) > 0.1f ? speed : 0.0f;
	}
	else
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

GameCar::Wheels& GameCar::GetWheels()
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

void DestrObj::SaveSource(lsl::SWriter* writer)
{
	_MyBase::SaveSource(writer);

	writer->WriteValue("destrList", _destrList);
}

void DestrObj::LoadSource(lsl::SReader* reader)
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

		for (DestrList::iterator iter = _destrList->begin(); iter != _destrList->end(); ++iter)
		{
			(*iter)->GetGameObj().SetParent(0);
			(*iter)->GetGameObj().SetOwner(0);
			(*iter)->SetName("");
			GetLogic()->GetMap()->InsertMapObj(*iter);

			game::GameObject& gameObj = (*iter)->GetGameObj();
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

DestrObj::DestrList& DestrObj::GetDestrList()
{
	return *_destrList;
}

}

}