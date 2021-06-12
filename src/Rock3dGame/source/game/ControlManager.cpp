#include "stdafx.h"
#include "game\\ControlManager.h"
#include "game\\World.h"

#include <xinput.h>

namespace r3d
{

namespace game
{

struct XInputState: ControllerState
{
	unsigned index;
	XINPUT_STATE state;
};

const unsigned cGamepadTriggerMax = 255;
const unsigned cGamepadLeftThumbMax = 32767;
const unsigned cGamepadRightThumbMax = 32767;

const VirtualKeyInfo cVirtualKeyInfo[cControllerTypeEnd][cVirtualKeyEnd + 1] = {
	{{"Left Arrow", 0, 0}, {"Right Arrow", 0, 0}, {"Up Arrow", 0, 0}, {"Down Arrow", 0, 0}, {"A", 0, 0}, {"B", 0, 0}, {"X", 0, 0}, {"Space", 0, 0}, {"F6", 0, 0}, {"F7", 0, 0}, {"F4", 0, 0}, {"F5", 0, 0}, {"F1", 0, 0}, {"F2", 0, 0}, {"F3", 0, 0}, {"L.Thumb Move Y", 0, 0}, {"R.Thumb Move X", 0, 0}, {"R.Thumb Move Y", 0, 0}, {"L.Thumb Left", 0, 0}, {"L.Thumb Right", 0, 0}, {"L.Thumb Up", 0, 0}, {"L.Thumb Down", 0, 0}, {"R.Thumb Left", 0, 0}, {"R.Thumb Right", 0, 0}, {"R.Thumb Up", 0, 0}, {"R.Thumb Down", 0, 0}, {"Escape", 0, 0}, {"Enter", 0, 0}, {"None", 0, 0}},
	{{"DPad Left", 0, 0}, {"DPad Right", 0, 0}, {"DPad Up", 0, 0}, {"DPad Down", 0, 0}, {"A", 0, 0}, {"B", 0, 0}, {"X", 0, 0}, {"Y", 0, 0}, {"Left Trigger", cGamepadTriggerMax, XINPUT_GAMEPAD_TRIGGER_THRESHOLD}, {"Right Trigger", cGamepadTriggerMax, XINPUT_GAMEPAD_TRIGGER_THRESHOLD}, {"Left Shoulder", 0, 0}, {"Right Shoulder", 0, 0}, {"L.Thumb Press", 0, 0}, {"R.Thumb Press", 0, 0}, {"L.Thumb Move X", cGamepadLeftThumbMax, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE}, {"L.Thumb Move Y", cGamepadLeftThumbMax, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE}, {"R.Thumb Move X", cGamepadRightThumbMax, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE}, {"R.Thumb Move Y", cGamepadRightThumbMax, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE}, {"L.Thumb Left", cGamepadLeftThumbMax, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE}, {"L.Thumb Right", cGamepadLeftThumbMax, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE}, {"L.Thumb Up", cGamepadLeftThumbMax, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE}, {"L.Thumb Down", cGamepadLeftThumbMax, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE}, {"R.Thumb Left", cGamepadLeftThumbMax, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE}, {"R.Thumb Right", cGamepadLeftThumbMax, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE}, {"R.Thumb Up", cGamepadLeftThumbMax, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE}, {"R.Thumb Down", cGamepadLeftThumbMax, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE}, {"Back", 0, 0}, {"Start", 0, 0}, {"None", 0, 0}}
};

const lsl::string cGameActionStr[cGameActionEnd] = {"gaAccel", "gaBreak", "gaWheelLeft", "gaWheelRight", "gaShot", "gaShot1", "gaShot2", "gaShot3", "gaShot4", "gaShotAll", "gaHyper", "gaMine", "gaWeaponDown", "gaWeaponUp", "gaViewSwitch", "gaAction", "gaEscape", "gaResetCar", "gaDebug1", "gaDebug2", "gaDebug3", "gaDebug4", "gaDebug5", "gaDebug6", "gaDebug7"};

const lsl::string cControllerTypeStr[cControllerTypeEnd] = {"ctKeyboard", "ctGamepad"};




ControlManager::ControlManager(World* world): _world(world)
{	
	_controllerStates[ctKeyboard] = new ControllerState();
	_controllerStates[ctGamepad] = new XInputState();

	ZeroMemory(_gameKeys, sizeof(_gameKeys));

	_gameKeys[ctKeyboard][gaAccel] = vkUp;
	_gameKeys[ctKeyboard][gaBreak] = vkDown;	
	_gameKeys[ctKeyboard][gaWheelLeft] = vkLeft;
	_gameKeys[ctKeyboard][gaWheelRight] = vkRight;
	_gameKeys[ctKeyboard][gaResetCar] = CharToVirtualKey('R');
	_gameKeys[ctKeyboard][gaShot] = CharToVirtualKey('W');
	_gameKeys[ctKeyboard][gaShot1] = CharToVirtualKey('1');
	_gameKeys[ctKeyboard][gaShot2] = CharToVirtualKey('2');
	_gameKeys[ctKeyboard][gaShot3] = CharToVirtualKey('3');
	_gameKeys[ctKeyboard][gaShot4] = CharToVirtualKey('4');
	_gameKeys[ctKeyboard][gaShotAll] = vkButtonY;
	_gameKeys[ctKeyboard][gaMine] = CharToVirtualKey('E');
	_gameKeys[ctKeyboard][gaHyper] = CharToVirtualKey('Q');
	_gameKeys[ctKeyboard][gaWeaponDown] = cVirtualKeyEnd;
	_gameKeys[ctKeyboard][gaWeaponUp] = cVirtualKeyEnd;	
	_gameKeys[ctKeyboard][gaAction] = vkStart;
	_gameKeys[ctKeyboard][gaEscape] = vkBack;
	_gameKeys[ctKeyboard][gaViewSwitch] = CharToVirtualKey('C');
	_gameKeys[ctKeyboard][gaDebug1] = vkThumbLeftPress;
	_gameKeys[ctKeyboard][gaDebug2] = vkThumbRightPress;
	_gameKeys[ctKeyboard][gaDebug3] = vkThumbLeftMoveX;
	_gameKeys[ctKeyboard][gaDebug4] = vkShoulderLeft;
	_gameKeys[ctKeyboard][gaDebug5] = vkShoulderRight;
	_gameKeys[ctKeyboard][gaDebug6] = vkTriggerLeft;
	_gameKeys[ctKeyboard][gaDebug7] = vkTriggerRight;

	_gameKeys[ctGamepad][gaAccel] = vkButtonA;
	_gameKeys[ctGamepad][gaBreak] = vkButtonB;
	_gameKeys[ctGamepad][gaWheelLeft] = vkLeft;
	_gameKeys[ctGamepad][gaWheelRight] = vkRight;
	_gameKeys[ctGamepad][gaResetCar] = vkBack;
	_gameKeys[ctGamepad][gaShot] = vkButtonX;
	_gameKeys[ctGamepad][gaShot1] = cVirtualKeyEnd;
	_gameKeys[ctGamepad][gaShot2] = cVirtualKeyEnd;
	_gameKeys[ctGamepad][gaShot3] = cVirtualKeyEnd;
	_gameKeys[ctGamepad][gaShot4] = cVirtualKeyEnd;
	_gameKeys[ctGamepad][gaShotAll] = vkButtonY;
	_gameKeys[ctGamepad][gaMine] = vkTriggerRight;
	_gameKeys[ctGamepad][gaHyper] = vkTriggerLeft;
	_gameKeys[ctGamepad][gaWeaponDown] = vkShoulderLeft;
	_gameKeys[ctGamepad][gaWeaponUp] = vkShoulderRight;
	_gameKeys[ctGamepad][gaAction] = vkButtonA;
	_gameKeys[ctGamepad][gaEscape] = vkStart;
	_gameKeys[ctGamepad][gaViewSwitch] = vkThumbRightPress;
	_gameKeys[ctGamepad][gaDebug1] = cVirtualKeyEnd;
	_gameKeys[ctGamepad][gaDebug2] = cVirtualKeyEnd;
	_gameKeys[ctGamepad][gaDebug3] = cVirtualKeyEnd;
	_gameKeys[ctGamepad][gaDebug4] = cVirtualKeyEnd;
	_gameKeys[ctGamepad][gaDebug5] = cVirtualKeyEnd;
	_gameKeys[ctGamepad][gaDebug6] = cVirtualKeyEnd;
	_gameKeys[ctGamepad][gaDebug7] = cVirtualKeyEnd;
}
	
ControlManager::~ControlManager()
{
	LSL_ASSERT(_eventList.empty());

	for (int i = 0; i < cControllerTypeEnd; ++i)
		delete _controllerStates[i];
}

int ControlManager::GetKeyboardKeyState(VirtualKey key)
{
	switch (key)
	{
	case vkLeft:
		return GetAsyncKey(VK_LEFT) == akDown;
	case vkRight:
		return GetAsyncKey(VK_RIGHT) == akDown;
	case vkUp:
		return GetAsyncKey(VK_UP) == akDown;
	case vkDown:
		return GetAsyncKey(VK_DOWN) == akDown;
	case vkButtonX:
		return GetAsyncKey(VK_BACK) == akDown;
	case vkButtonY:
		return GetAsyncKey(VK_SPACE) == akDown;	
	case vkBack:
		return GetAsyncKey(VK_ESCAPE) == akDown;
	case vkStart:
		return GetAsyncKey(VK_RETURN) == akDown;
	case vkShoulderLeft:
		return GetAsyncKey(VK_F4) == akDown;
	case vkShoulderRight:
		return GetAsyncKey(VK_F5) == akDown;
	case vkTriggerLeft:
		return GetAsyncKey(VK_F6) == akDown;
	case vkTriggerRight:
		return GetAsyncKey(VK_F7) == akDown;
	case vkThumbLeftPress:
		return GetAsyncKey(VK_F1) == akDown;
	case vkThumbRightPress:
		return GetAsyncKey(VK_F2) == akDown;
	case vkThumbLeftMoveX:
		return GetAsyncKey(VK_F3) == akDown;
	default:
		if (key > vkChar)
			return GetAsyncKey(VirtualKeyToChar(key)) == akDown;
		break;
	}

	return 0;
}

int ControlManager::GetGamepadKeyState(VirtualKey key)
{
	_controllerStates[ctKeyboard]->plugged = true;

	XInputState* xInput = (XInputState*)_controllerStates[ctGamepad];
	if (!xInput->plugged)
		return 0;

	switch (key)
	{
	case vkLeft:
		if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
			return 1;
		break;
	case vkRight:
		if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
			return 1;
		break;
	case vkUp:
		if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)
			return 1;
		break;
	case vkDown:
		if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
			return 1;
		break;
	case vkButtonA:
		if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_A)
			return 1;
		break;
	case vkButtonB:
		if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_B)
			return 1;
		break;
	case vkButtonX:
		if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_X)
			return 1;
		break;
	case vkButtonY:
		if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_Y)
			return 1;
		break;
	case vkBack:
		if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK)
			return 1;
		break;
	case vkStart:
		if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
			return 1;
		break;
	case vkShoulderLeft:
		if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
			return 1;
		break;
	case vkShoulderRight:
		if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
			return 1;
		break;
	case vkTriggerLeft:
		if (xInput->state.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
			return xInput->state.Gamepad.bLeftTrigger;
		break;
	case vkTriggerRight:
		if (xInput->state.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
			return xInput->state.Gamepad.bRightTrigger;
		break;	
	case vkThumbLeftPress:
		if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB)
			return 1;
		break;
	case vkThumbRightPress:
		if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)
			return 1;
		break;

	case vkThumbLeftMoveX:
		return xInput->state.Gamepad.sThumbLX;
	case vkThumbLeftMoveY:
		return xInput->state.Gamepad.sThumbLY;
	case vkThumbRightMoveX:
		return xInput->state.Gamepad.sThumbRX;
	case vkThumbRightMoveY:
		return xInput->state.Gamepad.sThumbRY;

	case vkThumbLeftMoveLeft:
		if (xInput->state.Gamepad.sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
			return xInput->state.Gamepad.sThumbLX;
		break;
	case vkThumbLeftMoveRight:
		if (xInput->state.Gamepad.sThumbLX > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
			return xInput->state.Gamepad.sThumbLX;
		break;
	case vkThumbLeftModeUp:
		if (xInput->state.Gamepad.sThumbLY < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
			return xInput->state.Gamepad.sThumbLY;
		break;
	case vkThumbLeftModeDown:
		if (xInput->state.Gamepad.sThumbLY > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
			return xInput->state.Gamepad.sThumbLY;
		break;

	case vkThumbRightMoveLeft:
		if (xInput->state.Gamepad.sThumbRX < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
			return xInput->state.Gamepad.sThumbRX;
		break;
	case vkThumbRightMoveRight:
		if (xInput->state.Gamepad.sThumbRX > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
			return xInput->state.Gamepad.sThumbRX;
		break;
	case vkThumbRightModeUp:
		if (xInput->state.Gamepad.sThumbRY < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
			return xInput->state.Gamepad.sThumbRY;
		break;
	case vkThumbRightModeDown:
		if (xInput->state.Gamepad.sThumbRY > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
			return xInput->state.Gamepad.sThumbRY;
		break;
	}

	return 0;
}

void ControlManager::UpdateControllerState()
{
	XInputState* state = (XInputState*)_controllerStates[ctGamepad];

	for (int i = 0; i < 4; ++i)
	{		
		unsigned dwResult = XInputGetState(i, &state->state);
		bool plugged = dwResult == ERROR_SUCCESS;		

		state->plugged = plugged;
		state->index = i;

		if (plugged)
			break;
	}

	while (true)
	{
		XINPUT_KEYSTROKE keystroke;
		unsigned dwResult = XInputGetKeystroke(XUSER_INDEX_ANY, XINPUT_FLAG_GAMEPAD, &keystroke);
		if (dwResult != ERROR_SUCCESS)
			break;

		if (_world->InputWasReset())
			continue;

		InputMessage msg;

		msg.state = cKeyStateEnd;;
		if (keystroke.Flags & XINPUT_KEYSTROKE_KEYDOWN)
			msg.state = ksDown;
		else if (keystroke.Flags & XINPUT_KEYSTROKE_KEYUP)
			msg.state = ksUp;

		msg.repeat = (keystroke.Flags & XINPUT_KEYSTROKE_REPEAT) != 0;
		msg.key = cVirtualKeyEnd;
		msg.unicode = keystroke.Unicode;		
		msg.controller = ctGamepad;

		switch (keystroke.VirtualKey)
		{
		case VK_PAD_X:
			msg.key = vkButtonX;
			break;
		case VK_PAD_Y:
			msg.key = vkButtonY;
			break;
		case VK_PAD_A:
			msg.key = vkButtonA;
			break;
		case VK_PAD_B:
			msg.key = vkButtonB;
			break;
		case VK_PAD_RSHOULDER:
			msg.key = vkShoulderRight;
			break;
		case VK_PAD_LSHOULDER:
			msg.key = vkShoulderLeft;
			break;
		case VK_PAD_LTRIGGER:
			msg.key = vkTriggerLeft;
			break;
		case VK_PAD_RTRIGGER:
			msg.key = vkTriggerRight;
			break;
		case VK_PAD_DPAD_UP:
			msg.key = vkUp;
			break;
		case VK_PAD_DPAD_DOWN:
			msg.key = vkDown;
			break;
		case VK_PAD_DPAD_LEFT:
			msg.key = vkLeft;
			break;
		case VK_PAD_DPAD_RIGHT:
			msg.key = vkRight;
			break;
		case VK_PAD_LTHUMB_PRESS:
			msg.key = vkThumbLeftPress;
			break;
		case VK_PAD_RTHUMB_PRESS:
			msg.key = vkThumbRightPress;
			break;
		case VK_PAD_BACK:
			msg.key = vkBack;
			break;
		case VK_PAD_START:
			msg.key = vkStart;
			break;

		case VK_PAD_LTHUMB_LEFT:
			msg.key = vkThumbLeftMoveLeft;
			break;
		case VK_PAD_LTHUMB_RIGHT:
			msg.key = vkThumbLeftMoveRight;
			break;
		case VK_PAD_LTHUMB_UP:
			msg.key = vkThumbLeftModeUp;
			break;
		case VK_PAD_LTHUMB_DOWN:
			msg.key = vkThumbLeftModeDown;
			break;

		case VK_PAD_RTHUMB_LEFT:
			msg.key = vkThumbRightMoveLeft;
			break;
		case VK_PAD_RTHUMB_RIGHT:
			msg.key = vkThumbRightMoveRight;
			break;
		case VK_PAD_RTHUMB_UP:
			msg.key = vkThumbRightModeUp;
			break;
		case VK_PAD_RTHUMB_DOWN:
			msg.key = vkThumbRightModeDown;
			break;
		}

		if (msg.key == cVirtualKeyEnd)
			continue;

		GameActions gameActions;
		GetGameAction(ctGamepad, msg.key, gameActions);
		if (gameActions.empty())
			gameActions.push_back(cGameActionEnd);

		for (unsigned i = 0; i < gameActions.size(); ++i)
		{
			msg.action = gameActions[i];

			if (HandleInput(msg))
				break;
		}
	}
}

bool ControlManager::HandleInput(const InputMessage& msg)
{
	for (EventList::iterator iter = _eventList.begin(); iter != _eventList.end(); ++iter)
	{		
		if ((*iter)->OnHandleInput(msg))
			return true;
	}

	return false;
}

bool ControlManager::OnMouseClickEvent(const MouseClick& mClick)
{
	for (EventList::iterator iter = _eventList.begin(); iter != _eventList.end(); ++iter)
		if ((*iter)->OnMouseClickEvent(mClick))
			return true;

	return false;
}

bool ControlManager::OnMouseMoveEvent(const MouseMove& mMove)
{
	for (EventList::iterator iter = _eventList.begin(); iter != _eventList.end(); ++iter)
		if ((*iter)->OnMouseMoveEvent(mMove))
			return true;

	return false;
}

bool ControlManager::OnKeyEvent(unsigned key, KeyState state, bool repeat)
{
	InputMessage msg;
	msg.key = cVirtualKeyEnd;
	msg.unicode = 0;
	msg.state = state;
	msg.controller = ctKeyboard;
	msg.repeat = repeat;

	switch (key)
	{
	case VK_LEFT:
		msg.key = vkLeft;
		break;
	case VK_RIGHT:
		msg.key = vkRight;
		break;
	case VK_UP:
		msg.key = vkUp;
		break;
	case VK_DOWN:
		msg.key = vkDown;
		break;
	case VK_BACK:
		msg.key = vkButtonX;
		break;
	case VK_SPACE:
		msg.key = vkButtonY;
		break;
	case VK_ESCAPE:
		msg.key = vkBack;
		break;
	case VK_RETURN:
		msg.key = vkStart;
		break;
	case VK_F1:
		msg.key = vkThumbLeftPress;
		break;
	case VK_F2:
		msg.key = vkThumbRightPress;
		break;
	case VK_F3:
		msg.key = vkThumbLeftMoveX;
		break;
	case VK_F4:
		msg.key = vkShoulderLeft;
		break;
	case VK_F5:
		msg.key = vkShoulderRight;
		break;
	case VK_F6:
		msg.key = vkTriggerLeft;
		break;
	case VK_F7:
		msg.key = vkTriggerRight;
		break;
	default:
		if (IsCharAlpha(key) || IsCharAlphaNumeric(key))
		{
			msg.key = CharToVirtualKey(key);
		}
		break;
	}

	if (msg.key == cVirtualKeyEnd)
		return false;

	GameActions gameActions;
	GetGameAction(ctKeyboard, msg.key, gameActions);
	if (gameActions.empty())
		gameActions.push_back(cGameActionEnd);

	for (unsigned i = 0; i < gameActions.size(); ++i)
	{
		msg.action = gameActions[i];
		if (HandleInput(msg))
			break;
	}

	return true;
}

bool ControlManager::OnKeyChar(unsigned key, lsl::KeyState state, bool repeat)
{
	InputMessage msg;
	msg.key = cVirtualKeyEnd;
	msg.unicode = 0;
	msg.state = state;
	msg.controller = ctKeyboard;
	msg.repeat = repeat;
	msg.key = vkChar;
	msg.action = cGameActionEnd;
	msg.unicode = lsl::ConvertStrAToW(lsl::stringA(1, key), CP_ACP)[0];

	HandleInput(msg);

	return true;
}

void ControlManager::OnProgress(float deltaTime)
{
	UpdateControllerState();

	for (EventList::iterator iter = _eventList.begin(); iter != _eventList.end(); ++iter)	
		(*iter)->OnInputProgress(deltaTime);
}

void ControlManager::OnFrame(float deltaTime)
{
	for (EventList::iterator iter = _eventList.begin(); iter != _eventList.end(); ++iter)	
		(*iter)->OnInputFrame(deltaTime);
}

void ControlManager::ResetInput(bool reset)
{	
}

void ControlManager::InsertEvent(ControlEvent* value)
{
	if (_eventList.IsFind(value))
		return;

	_eventList.push_back(value);
	value->AddRef();
}

void ControlManager::RemoveEvent(ControlEvent* value)
{
	EventList::const_iterator iter = _eventList.Find(value);
	if (iter == _eventList.end())
		return;

	_eventList.erase(iter);
	value->Release();
}

AsyncKey ControlManager::GetAsyncKey(unsigned key)
{
	//GetAsyncKeyState return
	//Последний бит - клавиша нажата в данный момент
	//Первый бит - клавиша была нажата с момента последнего вызова
	unsigned res = GetAsyncKeyState(key);

	if (res & 0x1)
		return akLastDown;
	else if (res & ~0x1)
		return akDown;
	else
		return akNone;
}

AsyncKey ControlManager::IsMouseDown(MouseKey key)
{
	switch (key)
	{
	case mkLeft:
		return GetAsyncKey(VK_LBUTTON);
		
	case mkRight:
		return GetAsyncKey(VK_RBUTTON);
		
	case mkMiddle:
		return GetAsyncKey(VK_MBUTTON);
	}

	return akNone;
}

lsl::Point ControlManager::GetMousePos()
{
	POINT pnt;
	if (GetCursorPos(&pnt) && ScreenToClient(_world->GetView()->GetDesc().handle, &pnt))
	{
		return _world->GetView()->ScreenToView(lsl::Point(pnt.x, pnt.y));
	}

	return _world->GetView()->ScreenToView(lsl::Point(0, 0));
}

D3DXVECTOR2 ControlManager::GetMouseVec()
{
	lsl::Point mPnt = GetMousePos();
	return D3DXVECTOR2(static_cast<float>(mPnt.x), static_cast<float>(mPnt.y));
}

const ControllerState& ControlManager::GetControllerState(ControllerType controller)
{
	return *_controllerStates[controller];
}

void ControlManager::GetGameAction(ControllerType controller, VirtualKey key, GameActions& gameActions)
{
	if (key == cVirtualKeyEnd)
		return;

	for (int i = 0; i < cGameActionEnd; ++i)
		if (_gameKeys[controller][i] == key)
			gameActions.push_back((GameAction)i);
}

VirtualKeyInfo ControlManager::GetVirtualKeyInfo(ControllerType controller, VirtualKey key)
{
	if (key < vkChar)
		return cVirtualKeyInfo[controller][key];

	VirtualKeyInfo info;
	info.name = VirtualKeyToChar(key);
	info.alphaMax = 0;
	info.alphaThreshold = 0;

	return info;
}

VirtualKey ControlManager::GetVirtualKeyFromName(ControllerType controller, const lsl::string& name)
{
	if (cVirtualKeyInfo[controller][cVirtualKeyEnd].name == name)
		return cVirtualKeyEnd;

	for (int i = 0; i < cVirtualKeyEnd; ++i)
		if (cVirtualKeyInfo[controller][i].name == name)
			return (VirtualKey)i;

	if (!name.empty())
		return CharToVirtualKey(name[0]);

	return cVirtualKeyEnd;
}

VirtualKeyInfo ControlManager::GetGameActionInfo(ControllerType controller, GameAction action)
{
	return GetVirtualKeyInfo(controller, GetGameKey(controller, action));
}

char ControlManager::VirtualKeyToChar(VirtualKey key)
{
	return (char)(key - vkChar);
}

VirtualKey ControlManager::CharToVirtualKey(char unicode)
{
	return (VirtualKey)((int)vkChar + unicode);
}

int ControlManager::GetVirtualKeyState(ControllerType controller, VirtualKey key)
{
	switch (controller)
	{	
	case ctGamepad:
		return GetGamepadKeyState(key);

	case ctKeyboard:
	default:
		return GetKeyboardKeyState(key);
	}
}

int ControlManager::GetGameActionState(ControllerType controller, GameAction action)
{
	return GetVirtualKeyState(controller, GetGameKey(controller, action));
}

float ControlManager::GetGameActionState(ControllerType controller, GameAction action, bool withAlpha)
{
	VirtualKeyInfo info = GetGameActionInfo(controller, action);		

	if (withAlpha && info.alphaMax == 0)
		return 0.0f;

	float d = static_cast<float>(info.alphaMax - info.alphaThreshold);
	int res = GetGameActionState(controller, action);

	if (d == 0.0f && res != 0)
		return (float)res;
	else if (res > (int)info.alphaThreshold)
		return (res - (int)info.alphaThreshold) / d;
	else if (res < -((int)info.alphaThreshold))
		return (res + (int)info.alphaThreshold) / d;

	return 0.0f;
}

float ControlManager::GetGameActionState(GameAction action, bool withAlpha)
{
	for (int i = 0; i < cControllerTypeEnd; ++i)
	{
		ControllerType controller = (ControllerType)i;
		float res = GetGameActionState(controller, action, withAlpha);

		if (res != 0.0f)
			return res;
	}

	return 0;
}

VirtualKey ControlManager::GetGameKey(ControllerType controller, GameAction action) const
{
	return _gameKeys[controller][action];
}

void ControlManager::SetGameKey(ControllerType controller, GameAction action, VirtualKey key)
{
	_gameKeys[controller][action] = key;
}

}

}