#pragma once

namespace r3d
{

namespace game
{

struct MouseClick
{
	MouseClick(): key(mkLeft), state(ksUp), shift1(false), coord(0, 0) {}

	MouseKey key;
	KeyState state;
	bool shift1;
	//оконные координаты
	lsl::Point coord;
	//проекционные координаты в видовом пространстве, [-1.0..1.0]
	D3DXVECTOR2 projCoord;

	//экранный луч в мировом пространстве
	D3DXVECTOR3 scrRayPos;
	D3DXVECTOR3 scrRayVec;
};

struct MouseMove
{
	MouseMove(): shift1(false), coord(0, 0) {}

	bool shift1;
	//оконные координаты
	Point coord;
	//проекционные координаты в видовом пространстве, [-1.0..1.0]
	D3DXVECTOR2 projCoord;
	//Разность между текущим и предыдущим значением координаты
	Point dtCoord;
	//Разность между текущим значением координаты и координатой при клике
	Point offCoord;

	//Состояние последнего клика мыши
	MouseClick click;

	//экранный луч в мировом пространстве
	D3DXVECTOR3 scrRayPos;
	D3DXVECTOR3 scrRayVec;
};

enum AsyncKey
{
	akNone = 0,
	akDown,
	akLastDown,
};

enum ControllerType {ctKeyboard = 0, ctGamepad, cControllerTypeEnd};

extern const lsl::string cControllerTypeStr[cControllerTypeEnd];

enum VirtualKey {vkLeft = 0, vkRight, vkUp, vkDown, vkButtonA, vkButtonB, vkButtonX, vkButtonY, vkTriggerLeft, vkTriggerRight, vkShoulderLeft, vkShoulderRight, vkThumbLeftPress, vkThumbRightPress, vkThumbLeftMoveX, vkThumbLeftMoveY, vkThumbRightMoveX, vkThumbRightMoveY, vkThumbLeftMoveLeft, vkThumbLeftMoveRight, vkThumbLeftModeUp, vkThumbLeftModeDown, vkThumbRightMoveLeft, vkThumbRightMoveRight, vkThumbRightModeUp, vkThumbRightModeDown, vkBack, vkStart, cVirtualKeyEnd, vkChar = 128, cVirtualKeyForce = INT_MAX};

struct VirtualKeyInfo
{
	std::string name;
	unsigned alphaMax;
	unsigned alphaThreshold;
};

extern const VirtualKeyInfo cVirtualKeyInfo[cControllerTypeEnd][cVirtualKeyEnd + 1];

struct ControllerState
{
	ControllerState(): plugged(false) {}
	virtual ~ControllerState() {}

	bool plugged;
};

enum GameAction {
	gaAccel = 0,
	gaBreak,
	gaWheelLeft,
	gaWheelRight,
	
	gaShot,
	gaShot1,
	gaShot2,
	gaShot3,
	gaShot4,
	gaShotAll,
	gaHyper,
	gaMine,
	gaWeaponDown,
	gaWeaponUp,

	gaViewSwitch,
	gaAction,
	gaEscape,
	gaResetCar,

	gaDebug1,
	gaDebug2,
	gaDebug3,
	gaDebug4,
	gaDebug5,
	gaDebug6,
	gaDebug7,

	cGameActionEnd,
	cGameActionUserEnd = gaDebug1
};

extern const lsl::string cGameActionStr[cGameActionEnd];

struct InputMessage
{
	GameAction action;
	VirtualKey key;
	KeyState state;
	bool repeat;
	ControllerType controller;
	WCHAR unicode;
};

class ControlEvent: public Object
{	
public:
	virtual bool OnMouseClickEvent(const MouseClick& mClick) {return false;}
	virtual bool OnMouseMoveEvent(const MouseMove& mMove) {return false;}	
	virtual bool OnHandleInput(const InputMessage& msg) {return false;}

	virtual void OnInputProgress(float deltaTime) {}
	virtual void OnInputFrame(float deltaTime) {}
};

class ControlManager
{
	friend class World;
private:
	typedef List<ControlEvent*> EventList;
	typedef lsl::Vector<GameAction> GameActions;
private:
	World* _world;
	EventList _eventList;	

	ControllerState* _controllerStates[cControllerTypeEnd];
	VirtualKey _gameKeys[cControllerTypeEnd][cGameActionEnd];

	int GetKeyboardKeyState(VirtualKey key);
	int GetGamepadKeyState(VirtualKey key);

	void UpdateControllerState();
	bool HandleInput(const InputMessage& msg);

	bool OnMouseClickEvent(const MouseClick& mClick);
	bool OnMouseMoveEvent(const MouseMove& mMove);
	bool OnKeyEvent(unsigned key, KeyState state, bool repeat);
	bool OnKeyChar(unsigned key, lsl::KeyState state, bool repeat);
public:
	ControlManager(World* world);
	virtual ~ControlManager();

	void OnProgress(float deltaTime);
	void OnFrame(float deltaTime);
	void ResetInput(bool reset);

	void InsertEvent(ControlEvent* value);
	void RemoveEvent(ControlEvent* value);

	AsyncKey GetAsyncKey(unsigned key);	
	AsyncKey IsMouseDown(MouseKey key);	

	lsl::Point GetMousePos();
	D3DXVECTOR2 GetMouseVec();

	const ControllerState& GetControllerState(ControllerType controller);
	void GetGameAction(ControllerType controller, VirtualKey key, GameActions& gameActions);

	VirtualKeyInfo GetVirtualKeyInfo(ControllerType controller, VirtualKey key);
	VirtualKey GetVirtualKeyFromName(ControllerType controller, const lsl::string& name);
	VirtualKeyInfo GetGameActionInfo(ControllerType controller, GameAction action);

	char VirtualKeyToChar(VirtualKey key);
	VirtualKey CharToVirtualKey(char unicode);

	int GetVirtualKeyState(ControllerType controller, VirtualKey key);
	int GetGameActionState(ControllerType controller, GameAction action);
	float GetGameActionState(ControllerType controller, GameAction action, bool withAlpha);
	float GetGameActionState(GameAction action, bool withAlpha);

	VirtualKey GetGameKey(ControllerType controller, GameAction action) const;
	void SetGameKey(ControllerType controller, GameAction action, VirtualKey key);
};

}

}