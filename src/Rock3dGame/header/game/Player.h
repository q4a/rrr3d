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

class SlotItem: public Object, public Serializable, protected IProgressEvent
{
	friend class Player;
private:
	Slot* _slot;

	std::string _name;
	std::string _info;
	int _cost;

	graph::IndexedVBMesh* _mesh;
	graph::Tex2DResource* _texture;

	D3DXVECTOR3 _pos;
	D3DXQUATERNION _rot;
protected:
	void RegProgressEvent();
	void UnregProgressEvent();

	virtual void OnProgress(float deltaTime) {}
	virtual void Save(SWriter* writer);
	virtual void Load(SReader* reader);
	virtual void OnFixUp(const FixUpNames& fixUpNames);

	virtual void TransformChanged();

	virtual const std::string& DoGetName() const;
	virtual const std::string& DoGetInfo() const;
	virtual int DoGetCost() const;
	virtual graph::IndexedVBMesh* DoGetMesh() const;
	virtual graph::Tex2DResource* DoGetTexture() const;

	virtual void OnCreateCar(MapObj* car) {}
	virtual void OnDestroyCar(MapObj* car) {}
public:
	SlotItem(Slot* slot);
	virtual ~SlotItem();

	virtual WeaponItem* IsWeaponItem();
	virtual MobilityItem* IsMobilityItem();

	Slot* GetSlot() const;
	Player* GetPlayer() const;

	const std::string& GetName() const;
	void SetName(const std::string& value);

	const std::string& GetInfo() const;
	void SetInfo(const std::string& value);

	int GetCost() const;
	void SetCost(int value);

	graph::IndexedVBMesh* GetMesh() const;
	void SetMesh(graph::IndexedVBMesh* value);

	graph::Tex2DResource* GetTexture() const;
	void SetTexture(graph::Tex2DResource* value);

	const D3DXVECTOR3& GetPos() const;
	void SetPos(const D3DXVECTOR3& value);

	const D3DXQUATERNION& GetRot() const;
	void SetRot(const D3DXQUATERNION& value);
};

class MobilityItem: public SlotItem
{
	typedef SlotItem _MyBase;
public:
	struct Tire
	{
		Tire(): extremumSlip(0), extremumValue(0), asymptoteSlip(0), asymptoteValue(0) {}
		Tire(float mExtremumSlip, float mExtremumValue, float mAsymptoteSlip, float mAsymptoteValue): extremumSlip(mExtremumSlip), extremumValue(mExtremumValue), asymptoteSlip(mAsymptoteSlip), asymptoteValue(mAsymptoteValue) {}

		void WriteTo(lsl::SWriter* writer)
		{
			writer->WriteValue("extremumSlip", extremumSlip);
			writer->WriteValue("extremumValue", extremumValue);
			writer->WriteValue("asymptoteSlip", asymptoteSlip);
			writer->WriteValue("asymptoteValue", asymptoteValue);
		}
		void ReadFrom(lsl::SReader* reader)
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

		void WriteTo(lsl::SWriter* writer);
		void ReadFrom(lsl::SReader* reader);

		Tire longTire;
		Tire latTire;
		float maxTorque;
		float life;
		float maxSpeed;
		float tireSpring;
	};
	typedef std::map<MapObjRec*, CarFunc> CarFuncMap;

protected:
	virtual void Save(SWriter* writer);
	virtual void Load(SReader* reader);
public:
	MobilityItem(Slot* slot);

	virtual MobilityItem* IsMobilityItem();
	
	void ApplyChanges();

	virtual float CalcLife(const CarFunc& func);

	CarFuncMap carFuncMap;
};

class WheelItem: public MobilityItem
{
	typedef MobilityItem _MyBase;
public:
	WheelItem(Slot* slot);
};

class TrubaItem: public MobilityItem
{
	typedef MobilityItem _MyBase;
public:
	TrubaItem(Slot* slot);
};

class MotorItem: public MobilityItem
{
	typedef MobilityItem _MyBase;
public:
	MotorItem(Slot* slot);
};

class ArmorItem: public MobilityItem
{
	typedef MobilityItem _MyBase;
private:
	bool _armor4Installed;

	Race* GetRace() const;	
protected:
	virtual const std::string& DoGetName() const;
	virtual const std::string& DoGetInfo() const;
	virtual int DoGetCost() const;
	virtual graph::IndexedVBMesh* DoGetMesh() const;
	virtual graph::Tex2DResource* DoGetTexture() const;
public:
	ArmorItem(Slot* slot);

	virtual float CalcLife(const CarFunc& func);

	bool CheckArmor4(bool ignorePlayers = false) const;
	bool IsArmor4Installed() const;
	void InstalArmor4(bool instal);
};

class WeaponItem: public SlotItem
{
	typedef SlotItem _MyBase;
private:
	MapObjRec* _mapObj;
	MapObj* _inst;
	Weapon::Desc _wpnDesc;

	unsigned _maxCharge;
	unsigned _cntCharge;
	unsigned _curCharge;
	unsigned _chargeStep;
	float _damage;
	int _chargeCost;

	virtual void TransformChanged();	
	void ApplyWpnDesc();
protected:	
	virtual void OnCreateCar(MapObj* car);
	virtual void OnDestroyCar(MapObj* car);

	virtual void Save(SWriter* writer);
	virtual void Load(SReader* reader);
	virtual void OnFixUp(const FixUpNames& fixUpNames);
public:
	WeaponItem(Slot* slot);
	virtual ~WeaponItem();

	virtual WeaponItem* IsWeaponItem();

	bool Shot(const Proj::ShotContext& ctx, int newCharge = -1, Weapon::ProjList* projList = NULL);
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
	//количество установленного заряда
	unsigned GetCntCharge();
	void SetCntCharge(unsigned value);
	//текущее количество заряда
	unsigned GetCurCharge();
	void SetCurCharge(unsigned value);
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

class HyperItem: public WeaponItem
{
	typedef WeaponItem _MyBase;
public:
	HyperItem(Slot* slot);
};

class MineItem: public WeaponItem
{
	typedef WeaponItem _MyBase;
public:
	MineItem(Slot* slot);
};

class DroidItem: public WeaponItem
{
private:
	float _repairValue;
	float _repairPeriod;

	float _time;
protected:
	virtual void OnCreateCar(MapObj* car);
	virtual void OnDestroyCar(MapObj* car);	
	virtual void OnProgress(float deltaTime);

	virtual void Save(SWriter* writer);
	virtual void Load(SReader* reader);
public:
	DroidItem(Slot* slot);
	virtual ~DroidItem();

	float GetRepairValue() const;
	void SetRepairValue(float value);

	float GetRepairPeriod() const;
	void SetRepairPeriod(float value);
};

class ReflectorItem: public WeaponItem
{
private:
	float _reflectValue;
protected:
	virtual void Save(SWriter* writer);
	virtual void Load(SReader* reader);
public:
	ReflectorItem(Slot* slot);
	virtual ~ReflectorItem();

	float GetReflectValue() const;
	void SetReflectValue(float value);

	float Reflect(float damage);
};

class Player;

class Slot: public Object, public lsl::Serializable
{
public:
	enum Type {stBase = 0, stWheel, stTruba, stArmor, stMotor, stHyper, stMine, stWeapon, stDroid, stReflector, cTypeEnd};
	typedef lsl::ClassList<Type, SlotItem, Slot*> ClassList;

	static ClassList classList;

	static void InitClassList();
private:
	Player* _player;
	Type _type;
	SlotItem* _item;

	Record* _record;

	void FreeItem();
protected:
	virtual void Save(SWriter* writer);
	virtual void Load(SReader* reader);
public:
	Slot(Player* player);
	virtual ~Slot();

	SlotItem& CreateItem(Type type);
	SlotItem& GetItem();

	template<class _Item> _Item& CreateItem()
	{
		return lsl::StaticCast<_Item&>(CreateItem(classList.GetByClass<_Item>().GetKey()));
	}
	template<class _Item> _Item& GetItem()
	{
		return lsl::StaticCast<_Item&>(GetItem());
	}

	Player* GetPlayer();
	Type GetType() const;
	
	Record* GetRecord();
	void SetRecord(Record* value);
};

class Player: public GameObjListener
{
	friend class MobilityItem;
private:
	typedef lsl::List<unsigned> AmmoList;

	struct BonusProj
	{
		Proj* proj;
		unsigned id;
	};
	typedef lsl::List<BonusProj> BonusProjs;
public:
	const float cTimeRestoreCar;	

	enum HeadLightMode {hlmNone, hlmOne, hlmTwo};
	enum HeadLight {hlFirst = 0, hlSecond, cHeadLightEnd};

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

	enum SlotType {stWheel = 0, stTruba, stArmor, stMotor, stHyper, stMine, stWeapon1, stWeapon2, stWeapon3, stWeapon4, cSlotTypeEnd};

	static const std::string cSlotTypeStr[cSlotTypeEnd];

	struct MyEventData: public EventData
	{
		Slot::Type slotType;
		BonusType bonusType;
		MapObjRec* record;
		unsigned targetPlayerId;
		GameObject::DamageType damageType;

		MyEventData(int playerId): EventData(playerId), slotType(Slot::cTypeEnd), bonusType(cBonusTypeEnd), record(NULL), targetPlayerId(cUndefPlayerId), damageType(GameObject::dtSimple) {}
		MyEventData(Slot::Type slot = Slot::cTypeEnd, BonusType bonus = cBonusTypeEnd, MapObjRec* mRecord = NULL, int mTargetPlayerId = cUndefPlayerId, GameObject::DamageType mDamageType = GameObject::dtSimple): slotType(slot), bonusType(bonus), record(mRecord), targetPlayerId(mTargetPlayerId), damageType(mDamageType) {}
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
	static const D3DXCOLOR cRightColors[cColorsCount];
private:
	Race* _race;
	CarState _car;
	float _carMaxSpeedBase;
	float _carTireSpringBase;
	float _timeRestoreCar;
	BonusProjs _bonusProjs;
	unsigned _nextBonusProjId;

	HeadLightMode _headLight;
	GraphManager::LightSrc* _lights[cHeadLightEnd];
	graph::Actor* _nightFlare;
	bool _reflScene;

	Slot* _slot[cSlotTypeEnd];
	int _money;
	int _points;
	int _pickMoney;
	int _id;
	int _gamerId;
	unsigned _netSlot;
	lsl::string _netName;
	int _place;
	bool _finished;
	unsigned _cheatEnable;
	float _block;

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

	void CreateColorMat(const graph::LibMaterial& colorMat);
	void FreeColorMat();
	void ApplyColorMat();
	void ApplyColor();

	void CheatUpdate(float deltaTime);
	void SetCheatK(const Player::CarState& car, float torqueK, float steerK);

	GraphManager* GetGraph();
	WayNode* GetLastNode();	
protected:
	void SendEvent(unsigned id, EventData* data = NULL);

	virtual void OnDestroy(GameObject* sender);
	virtual void OnLowLife(GameObject* sender, Behavior* behavior);	
	virtual void OnDeath(GameObject* sender, DamageType damageType, GameObject* target);
	void OnLapPass();
public:
	Player(Race* race);
	virtual ~Player();

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
	bool Shot(const Proj::ShotContext& ctx, SlotType type, unsigned projId, int newCharge = -1, Weapon::ProjList* projList = NULL);
	void ReloadWeapons();
	unsigned GetBonusProjId(game::Proj* proj);
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
	void SetSlot(SlotType type, Record* record, const D3DXVECTOR3& pos = NullVector, const D3DXQUATERNION& rot = NullQuaternion);
	Slot* GetSlotInst(SlotType type);
	Slot* GetSlotInst(Slot::Type type);

	void TakeBonus(GameObject* bonus, BonusType type, float value);

	int GetMoney() const;
	void SetMoney(int value);
	void AddMoney(int value);

	int GetPoints() const;
	void SetPoints(int value);
	void AddPoints(int value);

	int GetPickMoney() const;
	void ResetPickMoney();

	const D3DXCOLOR& GetColor() const;
	void SetColor(const D3DXCOLOR& value);

	int GetId() const;
	void SetId(int value);

	int GetGamerId() const;
	void SetGamerId(int value);

	//Внимание!!! является уникальным только для людей, для компов всегда Race::cDefNetId
	unsigned GetNetSlot() const;
	void SetNetSlot(unsigned value);

	const lsl::string& GetNetName() const;
	void SetNetName(const lsl::string& value);

	int GetPlace() const;
	void SetPlace(int value);

	bool GetFinished() const;
	void SetFinished(bool value);

	unsigned GetCheat() const;
	void SetCheat(unsigned value);

	void ResetBlock(bool block);
	bool IsBlock() const;

	float GetBlockTime() const;
	void SetBlockTime(float value);

	graph::Tex2DResource* GetPhoto();
	const std::string& GetName();

	bool IsHuman();
	bool IsComputer();
	bool IsOpponent();
};

}

}