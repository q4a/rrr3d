#ifndef GAME_CAR
#define GAME_CAR

#include "IGameCar.h"
#include "GameObject.h"

namespace r3d
{
	namespace game
	{
		class CarWheel : public GameObject
		{
			friend class CarWheels;
		private:
			using _MyBase = GameObject;

			class _Trails : public MapObjects
			{
			private:
				using _MyBase = MapObjects;
			public:
				_Trails(Component* parent): _MyBase(parent)
				{
				}
			};

			class MyContactModify : public px::WheelShape::ContactModify
			{
			private:
				CarWheel* _wheel;
			public:
				MyContactModify(CarWheel* wheel);

				bool onWheelContact(NxWheelShape* wheelShape, NxVec3& contactPoint, NxVec3& contactNormal,
				                    NxReal& contactPosition, NxReal& normalForce, NxShape* otherShape,
				                    NxMaterialIndex& otherShapeMaterialIndex, NxU32 otherShapeFeatureIndex) override;
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
			void Save(SWriter* writer) override;
			void Load(SReader* reader) override;

			void CreateWheelShape();
			void CreateWheelShape(const NxWheelShapeDesc& desc);
			void DestroyWheelShape();
		public:
			CarWheel(CarWheels* owner);
			~CarWheel() override;

			void PxSyncWheel(float alpha);

			void OnProgress(float deltaTime) override;

			px::WheelShape* GetShape() const;

			MapObjRec* GetTrailEff() const;
			void SetTrailEff(MapObjRec* value);

			float GetLongSlip() const;
			float GetLatSlip() const;

			float GetSteerAngle() const;
			void SetSteerAngle(float value);

			bool GetLead() const;
			void SetLead(bool value);

			bool GetSteer() const;
			void SetSteer(bool value);
			void RemoveWheel();

			const D3DXVECTOR3& GetOffset() const;
			void SetOffset(const D3DXVECTOR3& value);

			bool invertWheel;
			float _nReac{};
		};

		class GameCar;

		class CarWheels : public Collection<CarWheel, void, CarWheels*, CarWheels*>
		{
			friend CarWheel;
		private:
			using _MyBase = Collection<CarWheel, void, CarWheels*, CarWheels*>;
		public:
			using WheelGroup = List<CarWheel*>;
			using ContactModify = px::WheelShape::ContactModify;

			static void LoadPosTo(const std::string& fileName, std::vector<D3DXVECTOR3>& pos);
		private:
			GameCar* _owner;
			ContactModify* _steerContactModify;

			WheelGroup _leadGroup;
			WheelGroup _steerGroup;
		protected:
			void InsertItem(const Value& value) override;

			void InsertLeadWheel(CarWheel* value);
			void RemoveLeadWheel(CarWheel* value);
			void InsertSteerWheel(CarWheel* value);
			void RemoveSteerWheel(CarWheel* value);
		public:
			CarWheels(GameCar* owner);
			~CarWheels() override;

			CarWheel& Add();
			CarWheel& Add(const NxWheelShapeDesc& desc);

			GameCar* GetOwner() const;

			const WheelGroup& GetLeadGroup() const;
			const WheelGroup& GetSteerGroup() const;

			ContactModify* GetSteerContactModify() const;
			void SetSteerContactModify(ContactModify* value);
		};

		class GameCar : public GameObject
		{
		private:
			using _MyBase = GameObject;
		public:
			static const int cNeutralGear = CarMotorDesc::cNeutralGear;
			static const int cBackGear = CarMotorDesc::cBackGear;
			static const float cMaxSteerAngle;

			enum MoveCarState { mcNone, mcBrake, mcBack, mcAccel };

			enum SteerWheelState { swNone, swOnLeft, swOnRight, smManual };

			using Wheels = CarWheels;
		private:
			Wheels* _wheels;

			MapObjRec* _hyperDrive{};
			MapObj* _hyper{};

			float _clutchStrength;
			float _clutchTime;
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
			bool _spinstatus;
			bool _resplocked;
			bool _wasted;
			bool _burn;
			float _burnDamage;
			float _backSpeedK;
			bool _turnfreeze;
			bool _turnInverse;
			float _inverseTime;
			int _LMS;
			bool _isDamageStop;
			float _maxTimeSR;
			float _deminingTime;
			bool _slowride;
			bool _demining;
			bool _onJump;
			bool _goAiJump;
			bool _inFly;
			bool _lockAJmp;
			bool _inRage;
			bool _invisible;
			bool _isStabilityMine;
			bool _isStabilityShot;
			//блокировка респавна на время
			bool _ghostEff;

			float _steerAngle;
			bool _anyWheelContact;
			bool _wheelsContact;
			bool _bodyContact;

			void MotorProgress(float deltaTime, float& curMotorTorque, float& curBreakTorque, float& curRPM);
			void WheelsProgress(float deltaTime, float motorTorque, float breakTorque);
			void TransmissionProgress(float deltaTime, float curRPM);
			void JumpProgress(float deltaTime);
			void SpinOutProgress(float deltaTime);
			void StabilizeForce(float deltaTime);

			float GetWheelRPM() const;
			NxShape* GetWheelContactData(NxWheelContactData& contact) const;

			void ApplyWheelSteerK() const;
		protected:
			void SaveSource(SWriter* writer) override;
			void LoadSource(SReader* reader) override;
			bool OnContactModify(const px::Scene::OnContactModifyEvent& contact) override;
			void OnContact(const px::Scene::OnContactEvent& contact) override;
		public:
			GameCar();
			~GameCar() override;

			void OnPxSync(float alpha) override;
			void OnProgress(float deltaTime) override;
			void OnFixedStep(float deltaTime) override;

			GameCar* IsCar() override;

			void LockClutch(float strength);
			bool IsClutchLocked() const;

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

			float GetSteerWheelAngle() const;
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
			float GetLeadWheelSpeed() const;
			//Линейная скорость движения ведомых колес
			float GetDrivenWheelSpeed() const;
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

			bool InSpinStatus() const;
			void SetSpinStatus(bool value);

			bool GetRespBlock() const;
			void SetRespBlock(bool value);

			bool GetWastedControl() const;
			void SetWastedControl(bool value);

			bool IsBurn() const;
			void SetBurn(bool value);

			float BackSpeedK() const;
			void BackSpeedK(float value);

			float GetBurnDamage() const;
			void SetBurnDamage(float value);

			bool GetTurnFreeze() const;
			void SetTurnFreeze(bool value);

			bool GetTurnInverse() const;
			void SetTurnInverse(bool value);

			float GetInverseTime() const;
			void SetInverseTime(float value);

			int GetLastMoveState() const;
			void SetLastMoveState(int value);

			bool GetDamageStop() const;
			void SetDamageStop(bool value);

			float GetMaxTimeSR() const;
			void SetMaxTimeSR(float value);

			float GetDeminingTime() const;
			void SetDeminingTime(float value);

			bool GetSlowRide() const;
			void SetSlowRide(bool value);

			bool GetDemining() const;
			void SetDemining(bool value);

			bool OnJump() const;
			void OnJump(bool value);

			bool GoAiJump() const;
			void GoAiJump(bool value);

			bool InFly() const;
			void InFly(bool value);

			bool LockAIJump() const;
			void LockAIJump(bool value);

			bool InRage() const;
			void InRage(bool value);

			bool Invisible() const;
			void Invisible(bool value);

			bool IsStabilityMine() const;
			void StabilityMine(bool value);

			bool IsStabilityShot() const;
			void StabilityShot(bool value);

			bool GetGhostEff() const;
			void SetGhostEff(bool value);

			bool IsAnyWheelContact() const;
			bool IsWheelsContact() const;
			bool IsBodyContact() const;

			Wheels& GetWheels() const;

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

		class DestrObj : public GameObject
		{
		private:
			using _MyBase = GameObject;
		public:
			class DestrList : public MapObjects
			{
				friend DestrObj;
			public:
				DestrList(GameObject* parent): MapObjects(parent)
				{
				}
			};

		private:
			DestrList* _destrList;
			bool _checkDestruction;
		protected:
			void OnDeath(GameObject* sender, DamageType damageType, GameObject* target) override;

			void SaveSource(SWriter* writer) override;
			void LoadSource(SReader* reader) override;
		public:
			DestrObj();
			~DestrObj() override;

			void OnProgress(float deltaTime) override;

			DestrList& GetDestrList() const;
		};
	}
}

#endif
