//ќписывает состо€ние режима гонка. ¬заимодействует с игроком Player, с миром World.
//ƒанный модуль используетс€ Menu, AI

#pragma once

#include "DataBase.h"
#include "Logic.h"
#include "Player.h"
#include "HumanPlayer.h"
#include "AIPlayer.h"
#include "Environment.h"
#include "AchievmentModel.h"

namespace r3d
{

namespace game
{

class GameMode;
class Race;
class Tournament;

class Workshop
{
public:
	typedef lsl::List<Slot*> Slots;
	typedef lsl::List<Slot*> Items;
private:
	Race* _race;
	lsl::RootNode* _rootNode;
	RecordLib* _lib;
	Slots _slots;
	Items _items;

	void SaveSlot(Slot* slot, const std::string& name);
	
	//база данных
	Slot* AddSlot(Record* record);
	void DelSlot(Slots::const_iterator iter);
	void DelSlot(Slot* slot);
	void DeleteSlots();

	void LoadWheels();
	void LoadTruba();
	void LoadEngines();	
	void LoadArmors();
	void LoadWeapons();

	void LoadLib();	
	
	const RecordLib& GetLib();	
public:
	Workshop(Race* race);
	~Workshop();

	//найти слот в базе данных
	Record& GetRecord(const std::string& name);
	Slot* FindSlot(Record* record);

	//магазин, доступные слоты
	void InsertItem(Slot* slot);
	void RemoveItem(Items::iterator iter);
	bool RemoveItem(Slot* slot);
	void ClearItems();

	//купить слот
	bool BuyItem(Player* player, Slot* slot);
	Slot* BuyItem(Player* player, Record* slot);
	//продать слот
	void SellItem(Player* player, Slot* slot, bool sellDiscount = true, int chargeCount = -1);
	void SellItem(Player* player, Record* slot, bool sellDiscount = true, int chargeCount = -1);
	int GetCostItem(Slot* slot, bool sellDiscount = true, int chargeCount = -1);
	//купить зар€ды дл€
	bool BuyChargeFor(Player* player, WeaponItem* slot);
	Slot* BuyUpgrade(Player* player, Record* slot);

	int GetCost(Slot* slot) const;
	int GetSellCost(Slot* slot) const;
	int GetChargeCost(WeaponItem* slot) const;
	int GetChargeSellCost(WeaponItem* slot) const;
	void Reset();

	//слоты в базе данных
	const Slots& GetSlots();
	//доступные слоты
	const Items& GetItems();
	//найти слот в базе по имени
	Slot& GetSlot(const std::string& name);

	Race* GetRace();
};

class Garage: lsl::Component
{
public:
	static const int cUpgCntLevel = 3;
	static const int cUpgMaxLevel = cUpgCntLevel - 1;
	static const int cWeaponMaxLevel = 4;

	struct PlaceItem
	{
		PlaceItem(): slot(0), rot(NullQuaternion) {}
		PlaceItem(Slot* mSlot, const D3DXQUATERNION& mRot = NullQuaternion, const D3DXVECTOR3& mOffset = NullVector): slot(mSlot), rot(mRot), offset(mOffset) {}

		Slot* slot;
		D3DXQUATERNION rot;
		D3DXVECTOR3 offset;
	};
	typedef lsl::List<PlaceItem> PlaceItems;

	struct PlaceSlot
	{
		PlaceSlot(): active(false), show(false), pos(NullVector), defItem(0), lock(false) {}

		const PlaceItem* FindItem(Record* slot) const;
		const PlaceItem* FindItem(Slot* slot) const;
		
		//активен
		bool active;
		//отобразить на модели
		bool show;
		//ккординаты
		D3DXVECTOR3 pos;
		//итемы
		PlaceItems items;
		//итем по умолчанию, ставитс€ на машину если нет других вариантов
		Slot* defItem;
		//заблокирован, манипул€ции с ним запрещены
		bool lock;
	};

	struct BodyMesh
	{
		BodyMesh(): mesh(0), meshId(-1), texture(0), decal(false) {}

		graph::IndexedVBMesh* mesh;
		int meshId;
		graph::Tex2DResource* texture;
		bool decal;
	};
	typedef lsl::List<BodyMesh> BodyMeshes;

	class Car: public Object
	{
		friend Garage;
	public:
		struct NightLight
		{
			bool head;
			D3DXVECTOR3 pos;
			D3DXVECTOR2 size;			
		};
		typedef lsl::Vector<NightLight> NightLights;
	private:
		MapObjRec* _record;
		BodyMeshes _bodyMeshes;
		int _cost;
		std::string _name;
		std::string _desc;
		int _initialUpgradeSet;
		NightLights _nightLights;

		graph::IndexedVBMesh* _wheel;
		std::string _wheels;

		PlaceSlot _slot[Player::cSlotTypeEnd];

		Car();
		virtual ~Car();		
	public:
		void Assign(Car* ref);
		void ClearSlots();

		void SaveTo(lsl::SWriter* writer, Garage* owner);
		void LoadFrom(lsl::SReader* reader, Garage* owner);		

		MapObjRec* GetRecord();
		void SetRecord(MapObjRec* value);

		void AddBody(const BodyMesh& body);
		void AddBody(graph::IndexedVBMesh* mesh, graph::Tex2DResource* texture, bool decal, int meshId = -1);
		void ClearBodies();
		const BodyMeshes& GetBodies() const;

		int GetCost() const;
		void SetCost(int value);

		const std::string& GetName() const;
		void SetName(const std::string& value);

		const std::string& GetDesc() const;
		void SetDesc(const std::string& value);

		//0 - default
		//1,2,3, ... - levels
		int GetInitialUpgradeSet() const;
		void SetInitialUpgradeSet(int value);

		const NightLights& GetNightLights() const;
		void SetNightLights(const NightLights& value);

		graph::IndexedVBMesh* GetWheel();
		void SetWheel(graph::IndexedVBMesh* value);

		const std::string& GetWheels() const;
		void SetWheels(const std::string& value);

		const PlaceSlot& GetSlot(Player::SlotType type);
		void SetSlot(Player::SlotType type, const PlaceSlot& value);
	};

	typedef lsl::List<Car*> Cars;
	typedef lsl::List<Car*> Items;	
private:
	Race* _race;
	Cars _cars;
	Items _items;
	int _upgradeMaxLevel;
	int _weaponMaxLevel;

	void FillStandartSlots(Car* car, const std::string& wheel, bool truba, const D3DXVECTOR3* hyperDrive, bool spring, bool maslo, bool mineSpike, bool mineRip, bool mineProton, bool lockedMax);

	//записи о всех машинвх
	Car* AddCar();
	void DeleteCar(Cars::iterator iter);
	void DeleteCar(Car* car);
	void DeleteCars();

	void LoadCars();
	void LoadLib();
	
	Workshop& GetShop();	
protected:
	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);	
public:
	Garage(Race* race, const std::string& name);
	~Garage();

	//поиск машин
	Cars::iterator FindCar(Car* car);	
	Car* FindCar(MapObjRec* record);
	Car* FindCar(const std::string& name);

	//магазин, имеющиес€ машины
	void InsertItem(Car* item);
	void RemoveItem(Items::const_iterator iter);
	void RemoveItem(Car* item);
	void ClearItems();
	Items::iterator FindItem(Car* item);

	//установить слот на машину
	//возвращает слот игрока
	Slot* InstalSlot(Player* player, Player::SlotType type, Car* car, Slot* slot, int chargeCount = -1);
	bool IsSlotSupported(Car* car, Player::SlotType type, Slot* slot);
	bool TestCompSlot(Car* car, Car* newCar, Player::SlotType type, Slot* slot);
	void GetSupportedSlots(Player* player, Car* car, std::pair<Slot*, int> (&slots)[Player::cSlotTypeEnd], bool includeDef);
	//level - 0,1,2
	Record* GetUpgradeCar(Car* car, Player::SlotType type, int level);
	Slot* GetUpgradeCar(Car* car, Player* player, Player::SlotType type, int level);
	int GetUpgradeCarLevel(Car* car, Player::SlotType type, Record* record);
	void UpgradeCar(Player* player, int level, bool instalMaxCharge);
	void MaxUpgradeCar(Player* player);

	float GetMobilitySkill(const MobilityItem::CarFunc& func);
	float GetMobilitySkill(Car* car, Player* player, Player::SlotType type, int level);	
	float GetArmorSkill(Garage::Car* car, Player* player, float& armorVal, float& maxArmorVal);
	float GetDamageSkill(Garage::Car* car, Player* player, float& damageVal, float& maxDamageVal);
	float GetSpeedSkill(Garage::Car* car, Player* player);

	//купить машину
	//продать старую машину
	int GetCarCost(Car* car);
	int GetCarSellCost(Player* player);
	bool BuyCar(Player* player, Garage::Car* car);
	void Reset();

	//список всех машин
	const Cars& GetCars() const;
	//список доступных машин
	const Items& GetItems()	 const;

	int GetUpgradeMaxLevel() const;
	void SetUpgradeMaxLevel(int value);

	int GetWeaponMaxLevel() const;
	void SetWeaponMaxLevel(int value);
};

class Planet: public Object
{	
public:
	struct Price
	{
		Price(): money(0), points(0) {}
		Price(int mMoney, int mPoints): money(mMoney), points(mPoints) {}

		int money;
		int points;
	};
	typedef lsl::Vector<Price> Prices;
	
	class Track: public Object
	{
		friend Planet; 
	private:
		int _index;
		Planet* _planet;

		Track(Planet* planet): _index(-1), _planet(planet), world(Environment::wtWorld1), wheater(Environment::ewFair), numLaps(1) {}
		~Track() {}
	public:
		std::string level;
		Environment::WorldType world;
		Environment::Wheater wheater;
		//число кругов
		unsigned numLaps;		

		int GetIndex() const;
		Planet* GetPlanet();
		
		unsigned GetLapsCount();
	};

	typedef lsl::Vector<Track*> Tracks;
	typedef std::map<int, Tracks> TrackMap;

	struct SlotData
	{
		SlotData(): record(0), charge(0), type(Player::cSlotTypeEnd), pass(0) {}
		SlotData(Record* mRecord, int mCharge, Player::SlotType mType, int mPass): record(mRecord), charge(mCharge), type(mType), pass(mPass) {}

		Record* record;
		int charge;
		Player::SlotType type;
		int pass;		
	};
	typedef lsl::List<SlotData> Slots;

	struct CarData
	{
		CarData(): record(0), pass(0) {}
		CarData(MapObjRec* mRecord, int mPass): record(mRecord), pass(mPass) {}

		MapObjRec* record;
		int pass;
	};
	typedef lsl::List<CarData> Cars;

	struct PlayerData
	{
		PlayerData(): id(-1), name(scNull), photo(0), maxPass(0) {}

		int id;
		std::string name;
		std::string bonus;
		graph::Tex2DResource* photo;		
		int maxPass;

		Cars cars;
		Slots slots;		
	};
	typedef lsl::List<PlayerData> Players;

	//—осто€ние: открыта, закрыта, недоступна
	//заплатив очки можно перейтий из psClosed в psOpen
	enum State {psOpen, psClosed, psUnavailable, psCompleted};

	enum WorldType {wtWorld1 = 0, wtWorld2, wtWorld3, wtWorld4, wtWorld5, wtWorld6, cWorldTypeEnd};	

	struct Wheater
	{
		Wheater(): type(Environment::ewClody), chance(0) {}
		Wheater(Environment::Wheater mType, float mChance): type(mType), chance(mChance) {}

		Environment::Wheater type;
		float chance;
	};
	typedef lsl::List<Wheater> Wheaters;

	typedef std::map<int, int> RequestPoints;

	static const std::string cWorldTypeStr[cWorldTypeEnd];
private:
	Race* _race;
	int _index;

	TrackMap _trackMap;
	Tracks _trackList;
	std::string _name;
	std::string _info;
	RequestPoints _requestPoints;	
	State _state;
	WorldType _worldType;
	Wheaters _wheaters;
	Prices _prices;
	Slots _slots;
	Cars _cars;
	Players _players;
	int _pass;

	graph::IndexedVBMesh* _mesh;
	graph::Tex2DResource* _texture;

	//проход начат
	//pass = 0, планета была разблокирована
	//pass 1...n начать этап
	void StartPass(int pass, Player* player);
	void StartPass(int pass);
	//проход завершен
	//pass = 0, планета была открыта
	//pass 1...n пройден этап	
	void CompletePass(int pass);

	void SaveSlots(lsl::SWriter* writer, const std::string& name, Slots& mSlots, Tournament* owner);
	void LoadSlots(lsl::SReader* reader, const std::string& name, Slots& mSlots, Tournament* owner);
	void SaveCars(lsl::SWriter* writer, const std::string& name, Cars& mCars, Tournament* owner);
	void LoadCars(lsl::SReader* reader, const std::string& name, Cars& mCars, Tournament* owner);
public:
	Planet(Race* race, int index);
	virtual ~Planet();
	
	Track* AddTrack(int pass);
	void ClearTracks();
	Track* NextTrack(Track* track);
	TrackMap::const_iterator GetTracks(int pass) const;	
	const Tracks& GetTracks() const;
	const TrackMap& GetTrackMap() const;
	const Tracks& GetTrackList() const;

	void Unlock();
	bool Open();
	bool Complete();

	//начать проход
	void StartPass(Player* player);
	void StartPass();
	//следующий
	void NextPass();	
	//текущий
	int GetPass() const;
	void SetPass(int value);
	//
	void Reset();

	void SaveTo(lsl::SWriter* writer, Tournament* owner);
	void LoadFrom(lsl::SReader* reader, Tournament* owner);

	int GetIndex() const;
	int GetId() const;

	const std::string& GetName() const;
	void SetName(const std::string& value);

	const std::string& GetInfo() const;
	void SetInfo(const std::string& value);

	const RequestPoints& GetRequestPoints() const;
	void SetRequestPoints(const RequestPoints& value);

	int GetRequestPoints(int pass) const;
	bool HasRequestPoints(int pass, int points) const;	

	State GetState() const;
	void SetState(State value);
	
	WorldType GetWorldType() const;
	void SetWorldType(WorldType value);

	Wheater GenerateWheater(bool allowNight, bool mostProbable) const;
	void SetWheaters(const Wheaters& wheaters);

	Price GetPrice(int place) const;
	void SetPrices(const Prices& prices);

	graph::IndexedVBMesh* GetMesh();
	void SetMesh(graph::IndexedVBMesh* value);

	graph::Tex2DResource* GetTexture();
	void SetTexture(graph::Tex2DResource* value);

	void InsertSlot(const SlotData& slot);
	void InsertSlot(Record* slot, int pass);
	void InsertSlot(const lsl::string& name, int pass);
	void ClearSlots();
	void SetSlots(const Slots& value);

	void InsertCar(const CarData& car);
	void InsertCar(MapObjRec* car, int pass);
	void InsertCar(const lsl::string& name, int pass);
	void SetCars(const Cars& value);
	void ClearCars();	
	Cars::const_iterator FindCar(Garage::Car* car);
	const Cars& GetCars() const;

	void InsertPlayer(const PlayerData& data);
	void ClearPlayers();
	const PlayerData* GetPlayer(int id) const;
	const PlayerData* GetPlayer(const std::string& name) const;
	PlayerData GetBoss() const;
};

class Tournament: Component
{
public:
	typedef lsl::Vector<Planet*> Planets;	
private:
	Race* _race;
	Planets _planets;
	Planets _gamers;
	
	Planet* _curPlanet;
	Planet::Track* _curTrack;
	Planet::Tracks _trackList;
	Environment::Wheater _wheater;
	bool _wheaterNightPass;
	int _lapsCount;

	void LoadPlanets();
	void LoadGamers();
	void LoadLib();
protected:
	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);	
public:
	Tournament(Race* race, const std::string& name);
	~Tournament();

	Planet* AddPlanet();
	void ClearPlanets();
	Planets::iterator FindPlanet(Planet* planet);
	Planet* FindPlanet(const std::string& name);
	Planet* NextPlanet(Planet* planet);
	Planet* PrevPlanet(Planet* planet);
	Planet* GetPlanet(int index);
	
	Planet* AddGamer();
	void DelGamer(Planets::const_iterator iter);
	void ClearGamers();
	Planets::iterator FindGamer(Planet* planet);
	Planet* GetGamer(int gamerId);

	void CompleteTrack(int points, bool& passComplete, bool& passChampion, bool& planetChampion);
	bool ChangePlanet(Planet* planet);

	Planet& GetCurPlanet();
	int GetCurPlanetIndex();
	void SetCurPlanet(Planet* value);
	Planet* GetNextPlanet();

	Planet::Track& GetCurTrack();
	int GetCurTrackIndex();
	void SetCurTrack(Planet::Track* value);
	const Planet::Tracks& GetTrackList() const;	
	//track == NULL if need first track
	//return NULL if pass completed (tracks queue ended)
	Planet::Track* NextTrack(Planet::Track* track);

	Environment::Wheater GetWheater() const;
	void SetWheater(Environment::Wheater value);

	unsigned GetLapsCount() const;
	void SetLapsCount(unsigned value);

	const Planet::PlayerData* GetPlayerData(int id) const;
	const Planet::PlayerData* GetPlayerData(const std::string& name) const;
	Player* GetBossPlayer();

	void Reset();

	const Planets& GetPlanets() const;
	const Planets& GetGamers() const;
};

class Race: public Component, IFixedStepEvent, ILateProgressEvent
{
public:
	typedef lsl::Vector<Player*> PlayerList;
	typedef lsl::Vector<HumanPlayer*> Humans;
	typedef lsl::Vector<AIPlayer*> AIPlayers;
	
	enum Mode {rmChampionship = 0, rmSkirmish, cModeEnd};

	struct Result
	{
		Result(): playerId(cUndefPlayerId), place(0), money(0), points(0), pickMoney(0), voiceNameDur(0) {}

		int playerId;
		unsigned place;
		int money;
		int points;
		int pickMoney;
		float voiceNameDur;		

		bool operator==(const Result& result) const
		{
			return playerId == result.playerId;
		}
		bool operator!=(const Result& result) const
		{
			return playerId != result.playerId;
		}
	};
	typedef lsl::Vector<Result> Results;

	class Profile: public Object
	{
	protected:
		Race* _race;		
		std::string _name;		
		bool _netGame;
		Difficulty _difficulty;
	protected:
		virtual void EnterGame() = 0;
		virtual void SaveGame(lsl::SWriter* writer) = 0;
		virtual void LoadGame(lsl::SReader* reader) = 0;
	public:
		Profile(Race* race, const std::string& name);

		void Enter();
		void Reset();

		void SaveGame(std::ostream& stream);
		void LoadGame(std::istream& stream);

		void SaveGameFile();
		void LoadGameFile();

		const std::string& GetName() const;

		bool netGame() const;
		void netGame(bool value);

		Difficulty difficulty() const;
		void difficulty(Difficulty value);
	};
	typedef lsl::Vector<Profile*> Profiles;	

	class SnProfile: public Profile
	{
		typedef Profile _MyBase;	
	protected:
		void SaveWorkshop(lsl::SWriter* writer);
		void LoadWorkshop(lsl::SReader* reader);
		void SaveGarage(lsl::SWriter* writer);
		void LoadGarage(lsl::SReader* reader);
		void SaveTournament(lsl::SWriter* writer);
		void LoadTournament(lsl::SReader* reader);
		//
		void SavePlayer(Player* player, lsl::SWriter* writer);
		void LoadPlayer(Player* player, lsl::SReader* reader);
		void SaveHumans(lsl::SWriter* writer);
		void LoadHumans(lsl::SReader* reader);
		void SaveAIPlayers(lsl::SWriter* writer);
		void LoadAIPlayers(lsl::SReader* reader);

		virtual void EnterGame();
		virtual void SaveGame(lsl::SWriter* writer);
		virtual void LoadGame(lsl::SReader* reader);		
	public:
		SnProfile(Race* race, const std::string& name);
	};
	
	class SkProfile: public Profile
	{
		typedef Profile _MyBase;
	private:

	protected:
		virtual void EnterGame();
		virtual void SaveGame(lsl::SWriter* writer);
		virtual void LoadGame(lsl::SReader* reader);
	public:
		SkProfile(Race* race, const std::string& name);
	};

	typedef lsl::List<int> Planets;

	static const float cSellDiscount;	

	static const int cTournamentPlanetCount = 5;

	//self
	static const int cHuman = 0;
	//
	static const int cCampaignMaxPlayers = 6;
	//
	static const int cCampaignMaxHumans = 3;
	//
	static const int cMaxPlayers = 8;
	
	//computers
	static const int cComputerBit = 0;
	//
	static const int cComputerMask = 0x000000FF;
	//boss
	static const int cComputer1 = 0x00000001;
	//rip
	static const int cComputer2 = 0x00000002;
	//butcher
	static const int cComputer3 = 0x00000003;
	//comp4
	static const int cComputer4 = 0x00000004;
	//comp5
	static const int cComputer5 = 0x00000005;
	//
	static const int cComputerCount = 5;

	//human opponents
	static const int cOpponentBit = 8;
	//
	static const int cOpponentMask = 0x0000FF00;
	//
	static const int cOpponent1 = 0x00000100;
	//
	static const int cOpponent2 = 0x00000200;
	//
	static const int cOpponent3 = 0x00000300;	

	//net
	static const int cDefaultNetSlot = 0;
private:
	GameMode* _game;	

	Profiles _profiles;
	Profile* _profile;
	Profile* _lastProfile;
	Profile* _lastNetProfile;
	Profile* _skProfile;
	Profile* _snClientProfile;
	Mode _mode;

	bool _startRace;
	bool _goRace;	
	Results _results;
	bool _planetChampion;
	bool _passChampion;
	float _lastLeadPlace;
	float _lastThirdPlace;
	bool _carChanged;
	Difficulty _minDifficulty;
	int _tutorialStage;
	bool _springBorders;
	bool _enableMineBug;
	
	Workshop* _workshop;
	Garage* _garage;
	Tournament* _tournament;
	AchievmentModel* _achievment;
	Planets _planetsCompleted;

	//—писок игроков
	PlayerList _playerList;
	PlayerList _playerPlaceList;
	//люди
	HumanPlayer* _human;
	//компьтерные игроки
	AIPlayers _aiPlayers;
	//»». –аботает только в режиме гонки
	//≈сли научить режиму менеджера, то небходимо также просчитывать »» перед стартом гонки, пошагово
	AISystem* _aiSystem;
	
	void DisposePlayer(Player* player);	

	//игрок завершил трасу, вызваетс€ в пор€дке убывани€ по месту
	void CompleteRace(Player* player);
	void CompleteRace(const Results* results);
	void CompletePlanet(int index);

	void SaveGame(lsl::SWriter* writer);
	void LoadGame(lsl::SReader* reader);
	void LoadLib();
protected:
	virtual void OnFixedStep(float deltaTime);
	virtual void OnLateProgress(float deltaTime, bool pxStep);
public:
	Race(GameMode* game, const std::string& name);
	~Race();

	Player* AddPlayer(int plrId);
	Player* AddPlayer(int plrId, int gamerId, int netSlot, const D3DXCOLOR& color);
	void DelPlayer(PlayerList::const_iterator iter);
	void DelPlayer(Player* plr);
	void ClearPlayerList();

	AIPlayer* AddAIPlayer(Player* player);
	void DelAIPlayer(AIPlayers::const_iterator iter);
	void DelAIPlayer(AIPlayer* plr);
	void ClearAIPlayers();
	AIPlayer* FindAIPlayer(Player* player);

	HumanPlayer* CreateHuman(Player* player);
	void FreeHuman();

	void CreatePlayers(unsigned numAI);

	Player* GetPlayerByMapObj(MapObj* mapObj);
	Player* GetPlayerById(int id) const;
	Player* GetPlayerByNetSlot(unsigned netSlot) const;
	const PlayerList& GetPlayerList() const;
	HumanPlayer* GetHuman();
	const AIPlayers& GetAIPlayers() const;

	Profile* AddProfile(const std::string& name);
	void DelProfile(Profiles::const_iterator iter, bool saveLib = false);
	void DelProfile(int index, bool saveLib = false);
	void DelProfile(Profile* profile, bool saveLib = false);
	void ClearProfiles(bool saveLib = false);
	//
	Profiles::const_iterator FindProfile(Profile* profile) const;
	Profiles::const_iterator FindProfile(const std::string& name) const;
	std::string MakeProfileName(const std::string& base = "profile") const;
	const Profiles& GetProfiles() const;

	void EnterProfile(Profile* profile, Mode mode);
	void ExitProfile();
	void NewProfile(Mode mode, bool netGame, bool netClient);
	bool IsMatchStarted() const;
	
	void SaveProfile();
	void LoadProfile();
	void SaveLib();

	Profile* GetProfile();

	Profile* GetLastProfile();
	void SetLastProfile(Profile* value);

	Profile* GetLastNetProfile();
	void SetLastNetProfile(Profile* value);

	Mode GetMode() const;	
	bool IsCampaign() const;
	bool IsSkirmish() const;

	bool HasMoney(Player* player, int cost);
	bool BuyItem(Player* player, int cost);
	void SellItem(Player* player, int cost, bool sellDiscount = true);
	int GetCost(int realCost);
	int GetSellCost(int realCost, bool sellDiscount = true);	

	void HumanNextPlayer();
	void ResetCarPos();

	void StartRace();
	void ExitRace(const Results* results = NULL);
	bool IsStartRace() const;

	void GoRace();	
	bool IsRaceGo() const;

	bool GetCarChanged() const;
	void SetCarChanged(bool value);

	Difficulty GetMinDifficulty() const;
	void SetMinDifficulty(Difficulty value);

	int GetTutorialStage() const;
	void CompleteTutorialStage();
	bool IsTutorialCompletedFirstStage();
	bool IsTutorialCompleted();

	bool GetSpringBorders() const;
	void SetSpringBorders(bool value);

	bool GetEnableMineBug() const;
	void SetEnableMineBug(bool value);

	void SendEvent(unsigned id, EventData* data = NULL);	
	
	void OnLapPass(Player* player);	

	GameMode* GetGame();
	World* GetWorld();
	DataBase* GetDB();
	Map* GetMap();
	
	Workshop& GetWorkshop();
	Garage& GetGarage();
	Tournament& GetTournament();
	AchievmentModel& GetAchievment();
	const Planets& GetPlanetsCompleted() const;

	const Result* GetResult(int playerId) const;
	const Results& GetResults() const;
	bool GetPlanetChampion() const;
	bool GetPassChampion() const;
	void ResetChampion();
	int GetTotalPoints() const;
	
	MapObjRec* GetMapObjRec(MapObjLib::Category category, const std::string& name);
	MapObjRec* GetCar(const std::string& name);
	Record* GetSlot(const std::string& name);
	graph::Tex2DResource* GetTexture(const std::string& name);
	graph::IndexedVBMesh* GetMesh(const std::string& name);
	graph::LibMaterial* GetLibMat(const std::string& name);
	const std::string& GetString(StringValue value);

	static bool IsHumanId(int id);
	static bool IsComputerId(int id);
	static bool IsOpponentId(int id);
};

}

}