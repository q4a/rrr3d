#ifndef GAME_CAR
#define GAME_CAR

#include "IGameCar.h"
#include "GameObject.h"

namespace r3d
{

namespace game
{

class CarWheel: public GameObject
{
	friend class CarWheels;
private:
	typedef GameObject _MyBase;

	class _Trails: public MapObjects
	{
	private:
		typedef MapObjects _MyBase;
	public:
		_Trails(lsl::Component* parent): _MyBase(parent) {}
	};
	class MyContactModify: public px::WheelShape::ContactModify
	{
	private:
		CarWheel* _wheel;
	public:
		MyContactModify(CarWheel* wheel);

		virtual bool onWheelContact(NxWheelShape* wheelShape, NxVec3& contactPoint, NxVec3& contactNormal, NxReal& contactPosition, NxReal& normalForce, NxShape* otherShape, NxMaterialIndex& otherShapeMaterialIndex, NxU32 otherShapeFeatureIndex);
	};
private:
	CarWheels* _owner;
	MyContactModify* _myContactModify;
	px::WheelShape* _wheelShape;
	float _summAngle;
	MapObjRec* _trailEff;	

	_Trails* _trails;
	MapObj* _actTrail;

	float _steerAngle;
	bool _lead;
	bool _steer;
	D3DXVECTOR3 _offset;

	D3DXVECTOR3 _pxPrevPos;
	D3DXQUATERNION _pxPrevRot;
protected:
	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
	
	void CreateWheelShape();
	void CreateWheelShape(const NxWheelShapeDesc& desc);
	void DestroyWheelShape();	
public:
	CarWheel(CarWheels* owner);
	virtual ~CarWheel();

	void PxSyncWheel(float alpha);

	virtual void OnProgress(float deltaTime);

	px::WheelShape* GetShape();

	MapObjRec* GetTrailEff();
	void SetTrailEff(MapObjRec* value);

	float GetLongSlip();
	float GetLatSlip();

	float GetSteerAngle() const;
	void SetSteerAngle(float value);

	bool GetLead() const;
	void SetLead(bool value);

	bool GetSteer() const;
	void SetSteer(bool value);

	const D3DXVECTOR3& GetOffset() const;
	void SetOffset(const D3DXVECTOR3& value);

	bool invertWheel;	
	float _nReac;
};

class GameCar;

class CarWheels: public lsl::Collection<CarWheel, void, CarWheels*, CarWheels*>
{
	friend CarWheel;
private:
	typedef lsl::Collection<CarWheel, void, CarWheels*, CarWheels*> _MyBase;
public:
	typedef lsl::List<CarWheel*> WheelGroup;
	typedef px::WheelShape::ContactModify ContactModify;

	static void LoadPosTo(const std::string& fileName, std::vector<D3DXVECTOR3>& pos);
private:
	GameCar* _owner;
	ContactModify* _steerContactModify;

	WheelGroup _leadGroup;
	WheelGroup _steerGroup;
protected:
	virtual void InsertItem(const Value& value);

	void InsertLeadWheel(CarWheel* value);
	void RemoveLeadWheel(CarWheel* value);
	void InsertSteerWheel(CarWheel* value);
	void RemoveSteerWheel(CarWheel* value);
public:
	CarWheels(GameCar* owner);
	virtual ~CarWheels();

	CarWheel& Add();
	CarWheel& Add(const NxWheelShapeDesc& desc);

	GameCar* GetOwner();

	const WheelGroup& GetLeadGroup() const;
	const WheelGroup& GetSteerGroup() const;

	ContactModify* GetSteerContactModify();
	void SetSteerContactModify(ContactModify* value);
};

class GameCar: public GameObject
{
private:
	typedef GameObject _MyBase;
public:
	static const int cNeutralGear = CarMotorDesc::cNeutralGear;
	static const int cBackGear = CarMotorDesc::cBackGear;
	static const float cMaxSteerAngle;

	enum MoveCarState {mcNone, mcBrake, mcBack, mcAccel};
	enum SteerWheelState {swNone, swOnLeft, swOnRight, smManual};

	typedef CarWheels Wheels;
private:
	Wheels* _wheels;

	MapObjRec* _hyperDrive;
	MapObj* _hyper;

	float _clutchStrength;
	float _clutchTime;
	float _springTime;
	float _mineTime;
	int _curGear;
	CarMotorDesc _motor;
	MoveCarState _moveCar;
	SteerWheelState _steerWheel;	
	float _kSteerControl;
	float _steerSpeed;
	float _steerRot;
	float _flyYTorque;
	float _clampXTorque;
	float _clampYTorque;
	float _motorTorqueK;
	float _wheelSteerK;
	D3DXVECTOR3 _angDamping;
	bool _gravEngine;
	bool _clutchImmunity;	
	float _maxSpeed;
	float _tireSpring;
	bool _disableColor;

	float _steerAngle;
	bool _anyWheelContact;
	bool _wheelsContact;
	bool _bodyContact;

	void MotorProgress(float deltaTime, float& curMotorTorque, float& curBreakTorque, float& curRPM);
	void WheelsProgress(float deltaTime, float motorTorque, float breakTorque);
	void TransmissionProgress(float deltaTime, float curRPM);
	void JumpProgress(float deltaTime);
	void StabilizeForce(float deltaTime);

	float GetWheelRPM() const;
	NxShape* GetWheelContactData(NxWheelContactData& contact);

	void ApplyWheelSteerK();
protected:
	virtual void SaveSource(lsl::SWriter* writer);
	virtual void LoadSource(lsl::SReader* reader);
	virtual bool OnContactModify(const px::Scene::OnContactModifyEvent& contact);
	virtual void OnContact(const px::Scene::OnContactEvent& contact);
public:
	GameCar();
	virtual ~GameCar();

	virtual void OnPxSync(float alpha);	
	virtual void OnProgress(float deltaTime);
	virtual void OnFixedStep(float deltaTime);
	
	virtual GameCar* IsCar();

	void LockClutch(float strength);
	bool IsClutchLocked() const;

	void LockSpring();
	bool IsSpringLocked() const;

	void LockMine(float time);
	bool IsMineLocked() const;

	int GearUp();
	int GearDown();

	const CarMotorDesc& GetMotorDesc() const;
	void SetMotorDesc(const CarMotorDesc& value);

	MoveCarState GetMoveCar() const;
	void SetMoveCar(MoveCarState value);

	SteerWheelState GetSteerWheel() const;
	void SetSteerWheel(SteerWheelState value);

	float GetSteerWheelAngle();
	//Только для steerWheel == smManual
	void SetSteerWheelAngle(float value);

	//-1 - нейтральная
	//0 - задняя
	//>0 передачи
	int GetCurGear() const;
	void SetCurGear(int value);
	//Текущее число оборотов
	float GetRPM() const;
	//Скорость движения, проекция скорости центра масс на направление движения
	float GetSpeed();
	//Линейная скорость движения ведущих колес
	float GetLeadWheelSpeed();
	//Линейная скорость движения ведомых колес
	float GetDrivenWheelSpeed();
	//коэффициент управляемости
	//ИИ используется для вычисления
	//-позицинирования по траектории
	//-дистанции торможения
	float GetKSteerControl() const;
	void SetKSteerControl(float value);

	float GetSteerSpeed() const;
	void SetSteerSpeed(float value);

	float GetSteerRot() const;
	void SetSteerRot(float value);

	D3DXVECTOR3 GetAngDamping() const;
	void SetAngDamping(D3DXVECTOR3 value);

	float GetFlyYTorque() const;
	void SetFlyYTourque(float value);

	float GetClampXTorque() const;
	void SetClampXTourque(float value);

	float GetClampYTorque() const;
	void SetClampYTourque(float value);

	float GetMotorTorqueK() const;
	void SetMotorTorqueK(float value);

	float GetWheelSteerK() const;
	void SetWheelSteerK(float value);

	bool IsGravEngine() const;
	void SetGravEngine(bool value);

	bool IsClutchImmunity() const;
	void SetClutchImmunity(bool value);

	float GetMaxSpeed() const;
	void SetMaxSpeed(float value);

	float GetTireSpring() const;
	void SetTireSpring(float value);

	bool GetDisableColor() const;
	void SetDisableColor(bool value);

	bool IsAnyWheelContact() const;
	bool IsWheelsContact() const;
	bool IsBodyContact() const;

	Wheels& GetWheels();	

	//struct Contact
	//{
	//	PxContactCallbackData data;
	//	NxTriangle tri;
	//	const NxShape* shape0;
	//	const NxShape* shape1;
	//};
	//typedef lsl::List<Contact> ContactList;	
	//ContactList contactList;

	static float GetSpeed(NxActor* nxActor, const D3DXVECTOR3& dir);
};

class DestrObj: public GameObject
{
private:
	typedef GameObject _MyBase;
public:
	class DestrList: public MapObjects
	{
		friend DestrObj;
	public:
		DestrList(GameObject* parent): MapObjects(parent) {}
	};
private:
	DestrList* _destrList;
	bool _checkDestruction;
protected:
	virtual void OnDeath(GameObject* sender, DamageType damageType, GameObject* target);

	virtual void SaveSource(lsl::SWriter* writer);
	virtual void LoadSource(lsl::SReader* reader);
public:
	DestrObj();
	virtual ~DestrObj();

	virtual void OnProgress(float deltaTime);

	DestrList& GetDestrList();
};

}

}

#endif