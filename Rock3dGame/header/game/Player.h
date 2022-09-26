#pragma once

#include "RockCar.h"
#include "ControlManager.h"
#include "Trace.h"
#include "ResourceManager.h"

namespace r3d
{
	namespace game
	{
		class Race;
		class Map;
		class Slot;
		class WeaponItem;
		class MobilityItem;
		class ArmoredItem;

		class SlotItem : public Object, public Serializable, protected IProgressEvent
		{
			friend class Player;
		private:
			Slot* _slot;

			std::string _name;
			std::string _info;
			int _cost;
			float _inf;
			int _damage;
			int _id;
			int _linkId;
			bool _modify;
			bool _hide;
			int _pindex;
			bool _limit;
			bool _autoshot;

			graph::IndexedVBMesh* _mesh;
			graph::Tex2DResource* _texture;

			D3DXVECTOR3 _pos;
			D3DXQUATERNION _rot;
		protected:
			void RegProgressEvent();
			void UnregProgressEvent();

			void OnProgress(float deltaTime) override
			{
			}

			void Save(SWriter* writer) override;
			void Load(SReader* reader) override;
			void OnFixUp(const FixUpNames& fixUpNames) override;

			virtual void TransformChanged();

			virtual const std::string& DoGetName() const;
			virtual const std::string& DoGetInfo() const;
			virtual int DoGetCost() const;
			virtual float DoGetInf() const;
			virtual int DoGetDamage() const;
			virtual int DoGetId() const;
			virtual int DoGetLinkId() const;
			virtual bool DoGetModify() const;
			virtual bool DoGetHide() const;
			virtual int DoGetPIndex() const;
			virtual bool DoGetLimit() const;
			virtual bool DoGetAutoShot() const;
			virtual graph::IndexedVBMesh* DoGetMesh() const;
			virtual graph::Tex2DResource* DoGetTexture() const;

			virtual void OnCreateCar(MapObj* car)
			{
			}

			virtual void OnDestroyCar(MapObj* car)
			{
			}

		public:
			SlotItem(Slot* slot);
			~SlotItem() override;

			virtual WeaponItem* IsWeaponItem();
			virtual MobilityItem* IsMobilityItem();
			virtual ArmoredItem* IsArmoredItem();

			Slot* GetSlot() const;
			Player* GetPlayer() const;

			const std::string& GetName() const;
			void SetName(const std::string& value);

			const std::string& GetInfo() const;
			void SetInfo(const std::string& value);

			int GetCost() const;
			void SetCost(int value);

			float GetInf() const;
			void SetInf(float value);

			int GetDamage() const;
			void SetDamage(int value);

			int GetId() const;
			void SetId(int value);

			int GetLinkId() const;
			void SetLinkId(int value);

			bool GetModify() const;
			void SetModify(bool value);

			bool IsHide() const;
			void SetHide(bool value);

			int GetPIndex() const;
			void SetPIndex(int value);

			bool GetLimit() const;
			void SetLimit(bool value);

			bool GetAutoShot() const;
			void SetAutoShot(bool value);

			graph::IndexedVBMesh* GetMesh() const;
			void SetMesh(graph::IndexedVBMesh* value);

			graph::Tex2DResource* GetTexture() const;
			void SetTexture(graph::Tex2DResource* value);

			const D3DXVECTOR3& GetPos() const;
			void SetPos(const D3DXVECTOR3& value);

			const D3DXQUATERNION& GetRot() const;
			void SetRot(const D3DXQUATERNION& value);
		};

		class MobilityItem : public SlotItem
		{
			using _MyBase = SlotItem;
		public:
			struct Tire
			{
				Tire(): extremumSlip(0), extremumValue(0), asymptoteSlip(0), asymptoteValue(0)
				{
				}

				Tire(float mExtremumSlip, float mExtremumValue, float mAsymptoteSlip, float mAsymptoteValue):
					extremumSlip(mExtremumSlip), extremumValue(mExtremumValue), asymptoteSlip(mAsymptoteSlip),
					asymptoteValue(mAsymptoteValue)
				{
				}

				void WriteTo(SWriter* writer)
				{
					writer->WriteValue("extremumSlip", extremumSlip);
					writer->WriteValue("extremumValue", extremumValue);
					writer->WriteValue("asymptoteSlip", asymptoteSlip);
					writer->WriteValue("asymptoteValue", asymptoteValue);
				}

				void ReadFrom(SReader* reader)
				{
					reader->ReadValue("extremumSlip", extremumSlip);
					reader->ReadValue("extremumValue", extremumValue);
					reader->ReadValue("asymptoteSlip", asymptoteSlip);
					reader->ReadValue("asymptoteValue", asymptoteValue);
				}

				float extremumSlip;
				float extremumValue;
				float asymptoteSlip;
				float asymptoteValue;
			};

			class CarFunc
			{
			public:
				CarFunc();

				void WriteTo(SWriter* writer);
				void ReadFrom(SReader* reader);

				Tire longTire;
				Tire latTire;
				float maxTorque;
				float maxSpeed;
				float tireSpring;
			};

			using CarFuncMap = std::map<MapObjRec*, CarFunc>;

		protected:
			void Save(SWriter* writer) override;
			void Load(SReader* reader) override;
		public:
			MobilityItem(Slot* slot);

			MobilityItem* IsMobilityItem() override;

			void ApplyChanges();

			CarFuncMap carFuncMap;
		};

		////
		class ArmoredItem : public SlotItem
		{
			using _MyBase = SlotItem;
		public:
			class CarFunc
			{
			public:
				CarFunc();

				void WriteTo(SWriter* writer);
				void ReadFrom(SReader* reader);

				float life;
			};

			using CarFuncMap = std::map<MapObjRec*, CarFunc>;

		protected:
			void Save(SWriter* writer) override;
			void Load(SReader* reader) override;
		public:
			ArmoredItem(Slot* slot);

			ArmoredItem* IsArmoredItem() override;

			void ApplyChanges();

			virtual float CalcLife(const CarFunc& func);

			CarFuncMap carFuncMap;
		};

		///////////////////
		class WheelItem : public MobilityItem
		{
			using _MyBase = MobilityItem;
		public:
			WheelItem(Slot* slot);
		};

		class TrubaItem : public MobilityItem
		{
			using _MyBase = MobilityItem;
		public:
			TrubaItem(Slot* slot);
		};

		class MotorItem : public MobilityItem
		{
			using _MyBase = MobilityItem;
		public:
			MotorItem(Slot* slot);
		};

		class ArmorItem : public ArmoredItem
		{
			using _MyBase = ArmoredItem;
		private:
			Race* GetRace() const;
		protected:
			const std::string& DoGetName() const override;
			const std::string& DoGetInfo() const override;
			int DoGetCost() const override;
			graph::IndexedVBMesh* DoGetMesh() const override;
			graph::Tex2DResource* DoGetTexture() const override;
		public:
			ArmorItem(Slot* slot);

			float CalcLife(const CarFunc& func) override;
		};

		class WeaponItem : public SlotItem
		{
			using _MyBase = SlotItem;
		private:
			MapObjRec* _mapObj;
			MapObj* _inst;
			Weapon::Desc _wpnDesc;

			unsigned _maxCharge;
			unsigned _weaponBonusC;
			unsigned _springBonusC;
			unsigned _mineBonusC;
			unsigned _cntCharge;
			float _inflation;
			unsigned _curCharge;
			unsigned _chargeStep;
			float _damage;
			int _chargeCost;

			void TransformChanged() override;
			void ApplyWpnDesc();
		protected:
			void OnCreateCar(MapObj* car) override;
			void OnDestroyCar(MapObj* car) override;

			void Save(SWriter* writer) override;
			void Load(SReader* reader) override;
			void OnFixUp(const FixUpNames& fixUpNames) override;
		public:
			WeaponItem(Slot* slot);
			~WeaponItem() override;

			WeaponItem* IsWeaponItem() override;

			bool Shot(const Proj::ShotContext& ctx, int newCharge = -1, Weapon::ProjList* projList = nullptr);
			void Reload();
			bool IsReadyShot(float delay);
			bool IsReadyShot();

			MapObjRec* GetMapObj();
			void SetMapObj(MapObjRec* value);

			//0 - бесконечно
			//>0 - область значений
			//максимально возможный заряд
			unsigned GetMaxCharge();
			void SetMaxCharge(unsigned value);
			//дополнительные заряды  
			unsigned GetWpnBonusCharge();
			void SetWpnBonusCharge(unsigned value);
			//дополнительные прыжки
			unsigned BonusSpringCharge();
			void BonusSpringCharge(unsigned value);
			//дополнительные мины
			unsigned GetMineBonusCharge();
			void SetMineBonusCharge(unsigned value);
			//количество установленного заряда
			unsigned GetCntCharge();
			void SetCntCharge(unsigned value);
			//текущее количество заряда
			unsigned GetCurCharge();
			void SetCurCharge(unsigned value);
			void MinusCurCharge(unsigned value);
			//
			unsigned GetChargeStep();
			void SetChargeStep(unsigned value);
			//
			float GetDamage(bool statDmg = false) const;
			void SetDamage(float value);
			//
			int GetChargeCost() const;
			void SetChargeCost(int value);

			const Weapon::Desc& GetWpnDesc() const;
			void SetWpnDesc(const Weapon::Desc& value);

			Weapon* GetWeapon();
			Weapon::Desc GetDesc();
		};

		class HyperItem : public WeaponItem
		{
			using _MyBase = WeaponItem;
		public:
			HyperItem(Slot* slot);
		};

		class MineItem : public WeaponItem
		{
			using _MyBase = WeaponItem;
		public:
			MineItem(Slot* slot);
		};

		class DroidItem : public WeaponItem
		{
		private:
			float _repairValue;
			float _repairPeriod;

			float _time;
		protected:
			void OnCreateCar(MapObj* car) override;
			void OnDestroyCar(MapObj* car) override;
			void OnProgress(float deltaTime) override;

			void Save(SWriter* writer) override;
			void Load(SReader* reader) override;
		public:
			DroidItem(Slot* slot);
			~DroidItem() override;

			float GetRepairValue() const;
			void SetRepairValue(float value);

			float GetRepairPeriod() const;
			void SetRepairPeriod(float value);
		};

		class ReflectorItem : public WeaponItem
		{
		private:
			float _reflectValue;
		protected:
			void Save(SWriter* writer) override;
			void Load(SReader* reader) override;
		public:
			ReflectorItem(Slot* slot);
			~ReflectorItem() override;

			float GetReflectValue() const;
			void SetReflectValue(float value);

			float Reflect(float damage);
		};

		class TransItem : public SlotItem
		{
			using _MyBase = SlotItem;
		public:
			TransItem(Slot* slot);
		};

		class Player;

		class Slot : public Object, public Serializable
		{
		public:
			enum Type
			{
				stBase = 0,
				stWheel,
				stTruba,
				stArmor,
				stMotor,
				stHyper,
				stMine,
				stWeapon,
				stDroid,
				stReflector,
				stTrans,
				cTypeEnd
			};

			using ClassList = ClassList<Type, SlotItem, Slot*>;

			static ClassList classList;

			static void InitClassList();
		private:
			Player* _player;
			Type _type;
			SlotItem* _item;

			Record* _record;

			void FreeItem();
		protected:
			void Save(SWriter* writer) override;
			void Load(SReader* reader) override;
		public:
			Slot(Player* player);
			~Slot() override;

			SlotItem& CreateItem(Type type);
			SlotItem& GetItem();

			template <class _Item>
			_Item& CreateItem()
			{
				return lsl::StaticCast<_Item&>(CreateItem(classList.GetByClass<_Item>().GetKey()));
			}

			template <class _Item>
			_Item& GetItem()
			{
				return lsl::StaticCast<_Item&>(GetItem());
			}

			Player* GetPlayer();
			Type GetType() const;

			Record* GetRecord();
			void SetRecord(Record* value);
		};

		class Player : public GameObjListener
		{
			friend class MobilityItem;
			friend class ArmoredItem;
		private:
			using AmmoList = List<unsigned>;

			struct BonusProj
			{
				Proj* proj;
				unsigned id;
			};

			using BonusProjs = List<BonusProj>;
		public:
			const float cTimeRestoreCar;

			enum HeadLightMode { hlmNone, hlmOne, hlmTwo };

			enum HeadLight { hlFirst = 0, hlSecond, cHeadLightEnd };

			//Описывает состояние показателей машины
			struct CarState
			{
			private:
				CarState& operator=(const CarState& ref);
				CarState(const CarState& ref);
			public:
				CarState();
				~CarState();

				void Update(float deltaTime);

				void SetCurTile(WayNode* value);
				void SetCurNode(WayNode* value);
				void SetLastNode(WayNode* value);

				//
				WayNode* GetCurTile(bool lastCorrect = false) const;
				int GetPathIndex(bool lastCorrect = false) const;
				bool IsMainPath(bool lastCorrect = false) const;
				float GetPathLength(bool lastCorrect = false) const;
				//текущее пройденное расстояние
				float GetDist(bool lastCorrect = false) const;
				//текущее место на трасе, в относительных единицах в терминах круга
				float GetLap(bool lastCorrect = false) const;

				//место на карте
				D3DXVECTOR3 GetMapPos() const;

				Player* owner;
				MapObjRec* record;
				graph::LibMaterial* colorMat;
				D3DXCOLOR color;

				MapObj* mapObj;
				RockCar* gameObj;
				graph::Actor* grActor;
				NxActor* nxActor;

				//Координаты актера
				D3DXVECTOR3 pos3;
				D3DXVECTOR3 dir3;
				D3DXQUATERNION rot3;
				D3DXMATRIX worldMat;

				D3DXVECTOR2 pos;
				D3DXVECTOR2 dir;
				float speed;
				//Диаметр ограничивающей сферы
				float size;
				float radius;
				float kSteerControl;
				//Линия проведенная через pos в направлении dir актера
				D3DXVECTOR3 dirLine;
				//Линия проведенная через pos и перпендикулярно dir актера
				D3DXVECTOR3 normLine;
				//
				D3DXVECTOR3 trackDirLine;
				D3DXVECTOR3 trackNormLine;
				//
				WayNode* curTile;
				WayNode* curNode;
				WayNode* lastNode;
				float lastNodeCoordX;
				//
				unsigned track;

				//число пройденных кругов
				unsigned numLaps;

				//двигается инвертировано
				bool moveInverse;
				float moveInverseStart;
				//отслеживание резкий колебаний скорости
				float maxSpeed;
				float maxSpeedTime;
				//отслеживание резких колебаний угла направляющего вектора
				D3DXVECTOR3 lastDir;
				float summAngle;
				float summAngleTime;

				bool cheatSlower;
				bool cheatFaster;
			};

			void QuickFinish(Player* player);

			enum SlotType
			{
				stWheel = 0,
				stTruba,
				stArmor,
				stMotor,
				stHyper,
				stMine,
				stWeapon1,
				stWeapon2,
				stWeapon3,
				stWeapon4,
				stTrans,
				cSlotTypeEnd
			};

			static const std::string cSlotTypeStr[cSlotTypeEnd];

			struct MyEventData : public EventData
			{
				Slot::Type slotType;
				BonusType bonusType;
				MapObjRec* record;
				unsigned targetPlayerId;
				DamageType damageType;

				MyEventData(int playerId): EventData(playerId), slotType(Slot::cTypeEnd), bonusType(cBonusTypeEnd),
				                           record(nullptr), targetPlayerId(cUndefPlayerId), damageType(
					                           dtSimple)
				{
				}

				MyEventData(Slot::Type slot = Slot::cTypeEnd, BonusType bonus = cBonusTypeEnd,
				            MapObjRec* mRecord = nullptr, int mTargetPlayerId = cUndefPlayerId,
				            DamageType mDamageType = dtSimple): slotType(slot), bonusType(bonus), record(mRecord),
				                                                targetPlayerId(mTargetPlayerId), damageType(mDamageType)
				{
				}
			};

			static const float cHumanEasingMinDist[cDifficultyEnd];
			static const float cHumanEasingMaxDist[cDifficultyEnd];
			static const float cHumanEasingMinSpeed[cDifficultyEnd];
			static const float cHumanEasingMaxSpeed[cDifficultyEnd];
			static const float cCompCheatMinTorqueK[cDifficultyEnd];
			static const float cCompCheatMaxTorqueK[cDifficultyEnd];

			static const float cHumanArmorK[cDifficultyEnd];

			static const unsigned cBonusProjUndef = 0;

			static const unsigned cCheatDisable = 0;
			static const unsigned cCheatEnableSlower = 1 << 0;
			static const unsigned cCheatEnableFaster = 1 << 1;

			static const unsigned cColorsCount = 7;
			static const D3DXCOLOR cLeftColors[cColorsCount];
			static const D3DXCOLOR cLeftColors2[cColorsCount];

			static const D3DXCOLOR cRightColors[cColorsCount];
			static const D3DXCOLOR cRightColors2[cColorsCount];
		private:
			Race* _race;
			CarState _car;
			float _carMaxSpeedBase;
			float _carTireSpringBase;
			float _timeRestoreCar;
			float _timeRespAfter;
			float _freezeShotTime;
			float _freezeMineTime;
			float _freezeTurnTime;
			float _maxBurnTime;
			float _invTurnTime;
			float _jumpTime;
			float _rageTime;
			float _lowSpeedTime;
			float _timeAfterFinish;
			float _DemTime;
			BonusProjs _bonusProjs;
			unsigned _nextBonusProjId;

			HeadLightMode _headLight;
			GraphManager::LightSrc* _lights[cHeadLightEnd];
			graph::Actor* _nightFlare;
			bool _reflScene;

			Slot* _slot[cSlotTypeEnd];
			int _money;
			int _sponsorMoney;
			int _points;
			int _killstotal;
			int _deadstotal;
			int _racesTotal;
			int _winsTotal;
			int _passTrial;
			bool _OnSuicide;
			bool _skipResults;
			bool _overBoardD;
			float _raceTime;
			float _raceSeconds;
			float _raceMSeconds;
			int _raceMinutes;
			bool _spectator;
			bool _gamer;
			bool _inRace;
			bool _hyperDelay;
			float _liveTime;
			int _pickMoney;
			int _id;
			int _gamerId;
			bool _subj;
			bool _inRamp;
			int _camstatus;
			bool _emptyWpn;
			unsigned _netSlot;
			string _netName;
			int _place;
			bool _finished;
			bool _shotFreeze;
			bool _mineFreeze;
			unsigned _cheatEnable;
			float _block;
			float _recoveryTime;

			void InsertBonusProj(Proj* proj, int projId);
			void RemoveBonusProj(BonusProjs::const_iterator iter);
			void RemoveBonusProj(Proj* proj);
			void ClearBonusProjs();

			void InitLight(HeadLight headLight, const D3DXVECTOR3& pos, const D3DXQUATERNION& rot);
			void FreeLight(HeadLight headLight);
			void SetLightParent(GraphManager::LightSrc* light, MapObj* mapObj);
			void CreateNightLights(MapObj* mapObj);
			void SetLightsParent(MapObj* mapObj);
			void ApplyReflScene();

			void ReleaseCar();
			void ApplyMobility();
			void ApplyArmored();

			void CreateColorMat(const graph::LibMaterial& colorMat);
			void FreeColorMat();
			void ApplyColorMat();
			void ApplyColor();

			void CheatUpdate(float deltaTime);
			void SetCheatK(const CarState& car, float torqueK, float steerK);

			GraphManager* GetGraph();
			WayNode* GetLastNode();
		protected:
			void SendEvent(unsigned id, EventData* data = nullptr);

			void OnDestroy(GameObject* sender) override;
			void OnLowLife(GameObject* sender, Behavior* behavior) override;
			void OnDeath(GameObject* sender, DamageType damageType, GameObject* target) override;
			void OnLapPass();
		public:
			float _backMoveTime;

			Player(Race* race);
			virtual ~Player();

			void ResetMaxSpeed();
			void OnProgress(float deltaTime);

			//>0 искать соперников спереди машины
			//<0 искать соперников сзади машины
			//=0 искать соперников с обеих сторон
			Player* FindClosestEnemy(float viewAngle, bool zTest);
			float ComputeCarBBSize();

			void CreateCar(bool newRace);
			void FreeCar(bool freeState);
			void ResetCar();
			//
			float GetTimeRespAfter();
			void SetTimeRespAfter(float value);

			float GetFreezeShotTime();

			float GetFreezeMineTime();
			void SetFreezeMineTime(float value);

			float GetFreezeTurnTime();
			void SetFreezeTurnTime(float value);

			float GetMaxBurnTime();
			void SetMaxBurnTime(float value);

			float GetInverseTurnTime();
			void SetInverseTurnTime(float value);

			float GetJumpTime();
			void SetJumpTime(float value);

			float GetRageTime();
			void SetRageTime(float value);

			bool Shot(const Proj::ShotContext& ctx, SlotType type, unsigned projId, int newCharge = -1,
			          Weapon::ProjList* projList = nullptr);
			void ReloadWeapons();
			unsigned GetBonusProjId(Proj* proj);
			Proj* GetBonusProj(unsigned id);
			unsigned GetNextBonusProjId() const;

			Race* GetRace();
			Map* GetMap();

			const CarState& GetCar() const;
			void SetCar(MapObjRec* record);

			//
			HeadLightMode GetHeadLight() const;
			void SetHeadlight(HeadLightMode value);
			//Рендериться в отражения сцены, для человеческого игрока следует отключать
			bool GetReflScene() const;
			void SetReflScene(bool value);

			Record* GetSlot(SlotType type);
			void SetSlot(SlotType type, Record* record, const D3DXVECTOR3& pos = NullVector,
			             const D3DXQUATERNION& rot = NullQuaternion);
			Slot* GetSlotInst(SlotType type);
			Slot* GetSlotInst(Slot::Type type);

			void TakeBonus(GameObject* bonus, BonusType type, float value, bool destroy);

			int GetMoney() const;
			void SetMoney(int value);
			void AddMoney(int value);
			//спонсорские деньги используем только лишь для отображения их в диалоговом окне:
			int GetSponsorMoney() const;
			void SetSponsorMoney(int value);

			int GetPoints() const;
			void SetPoints(int value);
			void AddPoints(int value);

			int GetKillsTotal() const;
			void SetKillsTotal(int value);
			void AddKillsTotal(int value);

			int GetDeadsTotal() const;
			void SetDeadsTotal(int value);
			void AddDeadsTotal(int value);

			int GetRacesTotal() const;
			void SetRacesTotal(int value);
			void AddRacesTotal(int value);

			int GetWinsTotal() const;
			void SetWinsTotal(int value);
			void AddWinsTotal(int value);

			int GetPassTrial() const;
			void SetPassTrial(int value);
			void AddPassTrial(int value);

			bool GetSuicide() const;
			void SetSuicide(bool value);

			bool GetAutoSkip() const;
			void SetAutoSkip(bool value);

			bool GetOverBoardD() const;
			void SetOverBoardD(bool value);

			float GetRaceTime() const;
			void SetRaceTime(float value);

			float GetRaceSeconds() const;
			void SetRaceSeconds(float value);

			float GetRaceMSeconds() const;
			void SetRaceMSeconds(float value);

			int GetRaceMinutes() const;
			void SetRaceMinutes(int value);

			bool IsSpectator() const;
			void IsSpectator(bool value);

			bool IsGamer() const;
			void IsGamer(bool value);

			bool InRace() const;
			void InRace(bool value);

			bool GetHyperDelay() const;
			void SetHyperDelay(bool value);

			float GetCarLiveTime() const;
			void SetCarLiveTime(float value);

			int GetPickMoney() const;
			void ResetPickMoney();
			void AddPickMoney(int value);

			const D3DXCOLOR& GetColor() const;
			void SetColor(const D3DXCOLOR& value);

			int GetId() const;
			void SetId(int value);

			int GetGamerId() const;
			void SetGamerId(int value);

			bool isSubject() const;
			void isSubject(bool value);

			bool inRamp() const;
			void inRamp(bool value);

			int GetCameraStatus() const;
			void SetCameraStatus(int value);

			bool IsEmptyWpn() const;
			void IsEmptyWpn(bool value);

			//Внимание!!! является уникальным только для людей, для компов всегда Race::cDefNetId
			unsigned GetNetSlot() const;
			void SetNetSlot(unsigned value);

			const string& GetNetName() const;
			void SetNetName(const string& value);

			int GetPlace() const;
			void SetPlace(int value);

			bool GetFinished() const;
			void SetFinished(bool value);

			bool GetShotFreeze() const;
			void SetShotFreeze(bool value);

			bool GetMineFreeze() const;
			void SetMineFreeze(bool value);

			unsigned GetCheat() const;
			void SetCheat(unsigned value);

			void ResetBlock(bool block);
			bool IsBlock() const;

			float GetBlockTime() const;
			void SetBlockTime(float value);

			graph::Tex2DResource* GetPhoto();
			const std::string& GetName();
			const std::string& GetRealName();

			unsigned Player::GetBonusSpeed() const;
			unsigned Player::GetBonusRPM() const;
			unsigned Player::GetBonusTorque() const;
			float Player::GetSEM() const;
			unsigned int Player::GetIdlingRPM() const;
			float Player::GetBrakeTorque() const;
			float Player::GetRestTorque() const;
			float Player::GetGearDiff() const;
			unsigned Player::GetArmorBonus() const;
			bool Player::GetWeaponBonus() const;
			bool Player::GetMineBonus() const;
			bool Player::GetSpringBonus() const;
			bool Player::GetNitroBonus() const;
			bool Player::GetDoubleJump() const;
			float Player::GetJumpPower() const;
			float Player::GetCoolDown() const;
			bool Player::GetFixRecovery() const;
			float Player::GetDriftStrength() const;
			float Player::GetBlowDamage() const;
			bool Player::GetHardBorders() const;
			bool Player::GetBorderCluth() const;
			bool Player::GetMasloDrop() const;
			bool Player::GetStabilityMine() const;
			bool Player::GetStabilityShot() const;

			bool IsHuman();
			bool IsComputer();
			bool IsOpponent();
		};
	}
}
