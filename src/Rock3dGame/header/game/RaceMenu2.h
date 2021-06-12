#pragma once

#include "MenuSystem.h"

namespace r3d
{

namespace game
{

namespace n
{

class RaceMenu;

class CarFrame: public MenuFrame
{
public:
	enum CamStyle {csCar, csSlots, cCamStyleEnd};
private:
	RaceMenu* _raceMenu;
	Garage::Car* _car;
	CamStyle _camStyle;
	D3DXVECTOR3 _camLastPos;
	D3DXQUATERNION _camLastRot;

	MapObj* _garageMapObj;
	MapObj* _carMapObj;
	MapObj* _secretMapObj;
protected:
	virtual void OnShow(bool value);
public:
	CarFrame(Menu* menu, RaceMenu* raceMenu, gui::Widget* parent);
	virtual ~CarFrame();

	void OnProgress(float deltaTime);

	Garage::Car* GetCar();
	void SetCar(Garage::Car* value, const D3DXCOLOR& color, bool secret = false);
	void SetCar(MapObjRec* value, const D3DXCOLOR& color, bool secret = false);

	D3DXCOLOR GetCarColor();
	void SetCarColor(const D3DXCOLOR& value);

	void SetSlots(Player* player, bool includeDefSlots);

	void SetCamStyle(CamStyle value, bool instant);
};

class SpaceshipFrame: public MenuFrame
{
private:
	RaceMenu* _raceMenu;

	MapObj* _spaceMapObj;
	MapObj* _angarMapObj;
	float _redLampTime;
protected:
	virtual void OnShow(bool value);
public:
	SpaceshipFrame(Menu* menu, RaceMenu* raceMenu, gui::Widget* parent);
	virtual ~SpaceshipFrame();

	void OnProgress(float deltaTime);
};

class GarageFrame: public MenuFrame
{
private:
	enum MenuItem {miExit, miBuy, cMenuItemEnd};
	enum Label {mlHeader, mlCarName, mlCarInfo, mlMoney, mlPrice, cLabelEnd};

	struct CarData
	{
		CarData(): car(NULL), locked(false) {}

		Garage::Car* car;
		bool locked;
	};

	typedef std::list<CarData> Cars;
private:
	RaceMenu* _raceMenu;	
	int _lastCarIndex;
	Cars _cars;
	CarData _carData;

	gui::Button* _menuItems[cMenuItemEnd];
	gui::Label* _labels[cLabelEnd];

	gui::PlaneFon* _bottomPanel;
	gui::PlaneFon* _topPanel;
	gui::PlaneFon* _leftPanel;
	gui::PlaneFon* _rightPanel;
	gui::PlaneFon* _headerBg;
	gui::PlaneFon* _moneyBg;
	gui::PlaneFon* _stateBg;
	
	gui::ProgressBar* _speedBar;
	gui::ProgressBar* _armorBar;
	gui::ProgressBar* _damageBar;
	gui::Label* _armorBarValue;
	gui::Label* _damageBarValue;
	gui::Label* _speedBarValue;

	gui::Button* _leftArrow;
	gui::Button* _rightArrow;
	gui::Grid* _leftColorGrid;
	gui::Grid* _rightColorGrid;
	gui::Grid* _carGrid;

	gui::PlaneFon* AddCar(gui::Grid* grid, Garage::Car* car, bool locked);
	void UpdateCarList(gui::Grid* grid);
	Cars::const_iterator FindCar(Garage::Car* car);
	void AdjustCarList();
	void SelectCar(const CarData& carData);
	void PrevCar();
	void NextCar();

	gui::Widget* AddColor(gui::Grid* grid, const D3DXCOLOR& color);
	void UpdateColorList(gui::Grid* grid, const D3DXCOLOR colors[], unsigned count);
	void RefreshColorList(gui::Grid* grid, const D3DXCOLOR colors[], unsigned count);
	void RefreshColorList();
	void SelectColor(const D3DXCOLOR& value);

	void ShowMessage(StringValue message, gui::Widget* sender, const D3DXVECTOR2& slotSize);
	void ShowAccept(const std::string& message, gui::Widget* sender, const D3DXVECTOR2& slotSize);
protected:
	virtual void OnShow(bool value);
	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);
	virtual void OnInvalidate();
	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);	

	Garage::Car* car();
	D3DXCOLOR color();
public:
	GarageFrame(Menu* menu, RaceMenu* raceMenu, gui::Widget* parent);
	virtual ~GarageFrame();

	void OnProcessNetEvent(unsigned id, NetEventData* data);
};

class WorkshopFrame: public MenuFrame
{
private:
	enum MenuItem {miExit, cMenuItemEnd};
	enum Label {mlHeader, mlMoney, mlHint, cLabelEnd};

	struct SlotBox
	{
		int upLevel;
		bool selected;

		gui::PlaneFon* plane;
		gui::ViewPort3d* viewport;
		gui::Mesh3d* mesh3d;
		gui::Button* level;
		gui::PlaneFon* chargeBox;
		gui::ProgressBar* chargeBar;
		gui::Button* chargeButton;
		gui::PlaneFon* slotIcon;		
	};

	struct DragItem
	{
		gui::ViewPort3d* viewport;
		gui::Mesh3d* mesh3d;
		Slot* slot;
		Player::SlotType slotType;
		int chargeCount;
	};

	static const int cUndefInfo = -1;
	static const int cGoodInfo = 0;
	static const int cSlotInfo = 1;	
private:
	RaceMenu* _raceMenu;
	int _goodScroll;
	DragItem _dragItem;
	SlotBox _slots[Player::cSlotTypeEnd];
	gui::Widget* _overGood;
	gui::Widget* _overGood2;
	int _infoId;
	int _numSelectedSlots;	

	gui::Button* _menuItems[cMenuItemEnd];
	gui::Label* _labels[cLabelEnd];

	gui::PlaneFon* _bottomPanel;
	gui::PlaneFon* _topPanel;
	gui::PlaneFon* _leftPanel;	
	gui::PlaneFon* _moneyBg;
	gui::PlaneFon* _stateBg;	
	gui::Grid* _goodGrid;
	gui::Button* _downArrow;
	gui::Button* _upArrow;	

	gui::ProgressBar* _speedBar;
	gui::ProgressBar* _armorBar;
	gui::ProgressBar* _damageBar;
	gui::ProgressBar* _speedBarBonus;
	gui::ProgressBar* _armorBarBonus;
	gui::ProgressBar* _damageBarBonus;
	gui::Label* _armorBarValue;
	gui::Label* _damageBarValue;
	gui::Label* _speedBarValue;

	gui::ViewPort3d* AddGood(Slot* slot, int index, int count);
	void UpdateGoods();
	void AdjustGood();
	void ScrollGood(int step);	

	void StartDrag(Slot* slot, Player::SlotType slotType, int chargeCount);
	bool StopDrag(bool dropOut, bool intoGood);
	void ResetDrag();
	bool IsDragItem();
	void UpdateDragPos(const D3DXVECTOR2& pos);

	void SetSlotActive(Player::SlotType type, bool active, bool enabled);
	void UpdateSlot(Player::SlotType type, Slot* slot);
	void UpdateSlots();
	bool UpgradeSlot(gui::Widget* sender, Player::SlotType type, int level);
	void SelectSlots(Slot* slot, bool select);
	void UpdateSelection();
	void SetOverGood(gui::Widget* value);
	void SetOverGood2(gui::Widget* value);

	void BuySlot(gui::Widget* sender, Slot* slot);
	void SellSlot(Slot* slot, bool sellDiscount, int chargeCount);
	void InstalSlot(Player::SlotType type, Slot* slot, int chargeCount);
	void UpdateMoney();

	void ShowInfo(Slot* slot, int cost, gui::Widget* sender, const D3DXVECTOR2& slotSize, int infoId);
	void ShowInfo(Slot* slot, gui::Widget* sender, const D3DXVECTOR2& slotSize, int infoId);
	void HideInfo(int infoId);
	bool UpdateSlotInfo(gui::Widget* sender, const SlotBox& slotBox, Slot* slot, Player::SlotType type);	

	void UpdateStats();
	void UpdateBonusStats(Player::SlotType type, Slot* slot);

	void ShowMessage(StringValue message, gui::Widget* sender, const D3DXVECTOR2& slotSize);
	void ShowAccept(const std::string& message, gui::Widget* sender, const D3DXVECTOR2& slotSize, Slot* slot);
protected:
	virtual void OnShow(bool value);
	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);
	virtual void OnInvalidate();

	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
	virtual bool OnMouseDown(gui::Widget* sender, const gui::MouseClick& mClick);
	virtual bool OnMouseMove(gui::Widget* sender, const gui::MouseMove& mMove);
	virtual bool OnMouseEnter(gui::Widget* sender, const gui::MouseMove& mMove);
	virtual void OnMouseLeave(gui::Widget* sender, bool wasReset);

	Garage::Car* car();
public:
	WorkshopFrame(Menu* menu, RaceMenu* raceMenu, gui::Widget* parent);
	virtual ~WorkshopFrame();

	void OnProcessEvent(unsigned id, EventData* data);
};

class GamersFrame: public MenuFrame, IGameUser
{
private:	
	enum Label {mlInfo, mlName, mlBonus, cLabelEnd};
private:
	RaceMenu* _raceMenu;
	int _planetIndex;
	
	gui::Label* _labels[cLabelEnd];

	gui::PlaneFon* _bottomPanel;
	gui::PlaneFon* _space;
	gui::PlaneFon* _photo;
	gui::PlaneFon* _photoLight;
	gui::Button* _leftArrow;
	gui::Button* _rightArrow;
	gui::Button* _nextArrow;

	gui::ViewPort3d* _viewport;
	gui::Mesh3d* _mesh3d;

	void SelectPlanet(int value);
	void PrevPlanet();
	void NextPlanet();
	int GetNextPlanetIndex(int sIndex);
	int GetPrevPlanetIndex(int sIndex);
protected:	
	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);
	virtual void OnShow(bool value);
	virtual void OnInvalidate();
	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
public:
	GamersFrame(Menu* menu, RaceMenu* raceMenu, gui::Widget* parent);
	virtual ~GamersFrame();

	void OnProcessEvent(unsigned id, EventData* data);
	void OnProcessNetEvent(unsigned id, NetEventData* data);
};

class AngarFrame: public MenuFrame, IGameUser
{
private:
	enum MenuItem {miExit, cMenuItemEnd};
	enum Label {mlPlanetName, mlBossName, mlBossInfo, cLabelEnd};

	struct PlanetBox
	{
		gui::ViewPort3d* viewport;		
		gui::Button* slot;
		gui::Dummy* slotClip;
		gui::PlaneFon* doorDown;
		gui::PlaneFon* doorUp;		
	};
	typedef std::vector<PlanetBox> Planets;
private:
	RaceMenu* _raceMenu;	
	int _planetIndex;
	int _planetPrevIndex;
	float _doorTime;

	gui::Button* _menuItems[cMenuItemEnd];
	gui::Label* _labels[cLabelEnd];

	gui::Dummy* _bottomPanelBg;
	gui::PlaneFon* _bottomPanel;	
	gui::Grid* _planetGridBg;
	gui::Grid* _planetGrid;
	gui::PlaneFon* _planetInfo;
	gui::Button* _planetInfoClose;
	gui::PlaneFon* _planetBoss;	
	gui::ViewPort3d* _planetBossCar;
	Planets _planets;	

	PlanetBox& AddPlanet(Planet* car, int index);
	void UpdatePlanets();
	void SelectPlanet(int index);
	void SetDoorPos(PlanetBox& planet, float alpha);

	void ShowMessage(StringValue message, gui::Widget* sender, const D3DXVECTOR2& slotSize);
	void ShowAccept(const std::string& message, gui::Widget* sender, const D3DXVECTOR2& slotSize, lsl::Object* data);
protected:
	virtual void OnShow(bool value);
	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);
	virtual void OnInvalidate();
	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
	virtual void OnFocusChanged(gui::Widget* sender);
	virtual void OnProcessEvent(unsigned id, EventData* data);
public:
	AngarFrame(Menu* menu, RaceMenu* raceMenu, gui::Widget* parent);
	virtual ~AngarFrame();

	void OnProgress(float deltaTime);
};

class AchievmentFrame: public MenuFrame
{
private:
	enum MenuItem {miExit, cMenuItemEnd};
	enum Label {mlPoints, mlRewards, cLabelEnd};
	enum Box {mbViper, mbBuggi, mbAirblade, mbReflector, mbDroid, mbTankchetti, mbPhaser, mbMustang, mbMusicTrack, cMenuBoxEnd};

	struct AchievmentBox
	{
		gui::PlaneFon* image;
		gui::Label* price;
		gui::Button* button;

		Achievment* model;
	};

	typedef std::vector<AchievmentBox> Achievments;
private:
	RaceMenu* _raceMenu;	
	
	gui::Button* _menuItems[cMenuItemEnd];
	gui::Label* _labels[cLabelEnd];

	gui::PlaneFon* _bg;
	gui::PlaneFon* _bottomPanel;
	gui::PlaneFon* _panel;
	Achievments _achievments;

	const AchievmentBox* AddAchievment(unsigned index, const std::string& lockImg, const std::string& img, const D3DXVECTOR2& pos, Achievment* model);
	const AchievmentBox* GetAchievment(Achievment* model);
	void UpdateAchievments();
	void UpdateSelection(gui::Widget* sender, bool select);

	void ShowMessage(StringValue message, gui::Widget* sender);
	void ShowAccept(const std::string& message, gui::Widget* sender, Achievment* achievment);
protected:
	virtual void OnShow(bool value);
	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);
	virtual void OnInvalidate();

	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);
	virtual bool OnMouseEnter(gui::Widget* sender, const gui::MouseMove& mMove);
	virtual void OnMouseLeave(gui::Widget* sender, bool wasReset);
public:
	AchievmentFrame(Menu* menu, RaceMenu* raceMenu, gui::Widget* parent);
	virtual ~AchievmentFrame();
};

class RaceMainFrame: public MenuFrame
{
private:
	enum MenuItem {miStartRace = 0, miWorkshop, miGarage, miSpace, miAchivments, miOptions, miExit, cMenuItemEnd};
	enum Label {mlPlayer = 0, mlPassing, mlTournamet, mlWeapons, mlBoss, mlMoney, mlPassInfo, mlTournamentInfo, cLabelEnd};	

	struct PlayerBox
	{
		gui::Dummy* bgRoot;
		gui::PlaneFon* bg;
		gui::PlaneFon* photo;
		gui::Label* name;
		gui::Label* readyLabel;
		gui::Button* kick;		
		gui::PlaneFon* readyState;
		gui::ViewPort3d* viewportCar;

		NetPlayer* netPlayer;
	};
	typedef lsl::Vector<PlayerBox> Players;
private:
	RaceMenu* _raceMenu;

	gui::Button* _menuItems[cMenuItemEnd];
	gui::Label* _labels[cLabelEnd];

	gui::PlaneFon* _bottomPanel;
	gui::PlaneFon* _topPanel;
	gui::PlaneFon* _moneyBg;
	gui::PlaneFon* _stateBg;
	gui::PlaneFon* _wheater;
	gui::PlaneFon* _frameImages[3];
	gui::PlaneFon* _photoImages[2];
	gui::ProgressBar* _chargeBars[6];
	gui::ViewPort3d* _slots[6];
	gui::ProgressBar* _speedBar;
	gui::ProgressBar* _armorBar;
	gui::ProgressBar* _damageBar;
	gui::Label* _armorBarValue;
	gui::Label* _damageBarValue;
	gui::Label* _speedBarValue;
	gui::Label* _netInfo;

	gui::ViewPort3d* _viewportCar;
	gui::Dummy* _playerGrid;
	Players _players;

	void RaceRady(bool ready);	

	PlayerBox* AddPlayer(NetPlayer* netPlayer, unsigned index);
	void AdjustPlayer(PlayerBox* box, bool invert);
	void UpdatePlayers();
protected:	
	virtual void OnShow(bool value);
	virtual void OnAdjustLayout(const D3DXVECTOR2& vpSize);
	virtual void OnInvalidate();

	virtual bool OnClick(gui::Widget* sender, const gui::MouseClick& mClick);	
public:
	RaceMainFrame(Menu* menu, RaceMenu* raceMenu, gui::Widget* parent);
	virtual ~RaceMainFrame();

	void OnReceiveCmd(const net::NetMessage& msg, const net::NetCmdHeader& header, const void* data, unsigned size);
	void OnConnectedPlayer(NetPlayer* sender);
	void OnDisconnectedPlayer(NetPlayer* sender);
};

class RaceMenu: INetGameUser, gui::Widget::Event, IGameUser
{
public:
	enum State {msMain = 0, msGarage, msWorkshop, msGamers, msAngar, msAchievments, cStateEnd};	
private:
	Menu* _menu;
	Player* _player;
	State _state;
	State _lastState;

	gui::Dummy* _root;

	CarFrame* _carFrame;
	SpaceshipFrame* _spaceshipFrame;
	RaceMainFrame* _mainFrame;
	GarageFrame* _garageFrame;
	WorkshopFrame* _workshopFrame;
	GamersFrame* _gamers;
	AngarFrame* _angarFrame;
	AchievmentFrame* _achievmentFrame;
	
	void ApplyState(State state);
protected:
	virtual void OnProcessEvent(unsigned id, EventData* data);
	virtual void OnReceiveCmd(const net::NetMessage& msg, const net::NetCmdHeader& header, const void* data, unsigned size);
	virtual void OnConnectedPlayer(NetPlayer* sender);
	virtual void OnDisconnectedPlayer(NetPlayer* sender);
	virtual void OnProcessNetEvent(unsigned id, NetEventData* data);
public:
	RaceMenu(Menu* menu, gui::Widget* parent, Player* player);
	virtual ~RaceMenu();

	gui::Button* CreateMenuButton(const std::string& icon, gui::Widget* parent, gui::Widget::Event* guiEvent);
	gui::Button* CreateMenuButton2(StringValue name, gui::Widget* parent, const D3DXCOLOR& textColor, gui::Widget::Event* guiEvent);
	gui::Button* CreateArrow(gui::Widget* parent, gui::Widget::Event* guiEvent);
	gui::Button* CreateArrow2(gui::Widget* parent, gui::Widget::Event* guiEvent);
	gui::Button* CreatePlusButton(gui::Widget* parent, gui::Widget::Event* guiEvent);
	void CreateCar(gui::ViewPort3d* viewport, Garage::Car* car, const D3DXCOLOR& color, Slot* slots[Player::cSlotTypeEnd]);
	void CreateCar(gui::ViewPort3d* viewport, Player* player);

	void AdjustLayout(const D3DXVECTOR2& vpSize);
	void Show(bool value);
	void UpdateStats(Garage::Car* car, Player* player, gui::ProgressBar* armorBar, gui::ProgressBar* speedBar, gui::ProgressBar* damageBar, gui::Label* armorBarValue, gui::Label* damageBarValue, gui::Label* speedBarValue);

	void OnProgress(float deltaTime);

	Player* GetPlayer();
	gui::Widget* GetRoot();
	
	State GetState() const;
	void SetState(State value);

	State GetLastState() const;

	CarFrame* carFrame();
	SpaceshipFrame* spaceshipFrame();
};

}

}

}