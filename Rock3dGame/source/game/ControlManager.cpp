#include "stdafx.h"
#include "game//ControlManager.h"
#include "game//World.h"

#include <xinput.h>

namespace r3d
{
	namespace game
	{
		struct XInputState : ControllerState
		{
			unsigned index;
			XINPUT_STATE state;
		};

		const unsigned cGamepadTriggerMax = 255;
		const unsigned cGamepadLeftThumbMax = 32767;
		const unsigned cGamepadRightThumbMax = 32767;

		const VirtualKeyInfo cVirtualKeyInfo[cControllerTypeEnd][cVirtualKeyEnd + 1] = {
			{
				{"Left Arrow", 0, 0}, {"Right Arrow", 0, 0}, {"Up Arrow", 0, 0}, {"Down Arrow", 0, 0}, {"None", 0, 0},
				{"None", 0, 0}, {"Backspace", 0, 0}, {"Space", 0, 0}, {"F6", 0, 0}, {"F7", 0, 0}, {"F4", 0, 0},
				{"F5", 0, 0}, {"F1", 0, 0}, {"F2", 0, 0}, {"F3", 0, 0}, {"L.Thumb Move Y", 0, 0},
				{"R.Thumb Move X", 0, 0}, {"R.Thumb Move Y", 0, 0}, {"L.Thumb Left", 0, 0}, {"L.Thumb Right", 0, 0},
				{"L.Thumb Up", 0, 0}, {"L.Thumb Down", 0, 0}, {"R.Thumb Left", 0, 0}, {"R.Thumb Right", 0, 0},
				{"R.Thumb Up", 0, 0}, {"R.Thumb Down", 0, 0}, {"Escape", 0, 0}, {"Enter", 0, 0}, {"F8", 0, 0},
				{"F9", 0, 0}, {"F11", 0, 0}, {"F12", 0, 0}, {"Insert", 0, 0}, {"Delete", 0, 0}, {"Home", 0, 0},
				{"End", 0, 0}, {"TAB", 0, 0}, {"CAPS LOCK", 0, 0}, {"SHIFT", 0, 0}, {"CTRL", 0, 0}, {"NUMPAD 1", 0, 0},
				{"NUMPAD 0", 0, 0}, {"NUMPAD 2", 0, 0}, {"NUMPAD 3", 0, 0}, {"NUMPAD 4", 0, 0}, {"NUMPAD 5", 0, 0},
				{"NUMPAD 6", 0, 0}, {"NUMPAD 7", 0, 0}, {"NUMPAD 8", 0, 0}, {"NUMPAD 9", 0, 0}, {"+", 0, 0},
				{"-", 0, 0}, {"*", 0, 0}, {"/", 0, 0}, {"~", 0, 0}, {"-", 0, 0}, {"=", 0, 0}, {"[", 0, 0}, {"]", 0, 0},
				{";", 0, 0}, {"'", 0, 0}, {"|", 0, 0}, {",", 0, 0}, {".", 0, 0}, {"/", 0, 0}, {"Page Down", 0, 0},
				{"Page Up", 0, 0}, {"NumLock", 0, 0}, {".", 0, 0}, {"None", 0, 0}
			},
			{
				{"DPad Left", 0, 0}, {"DPad Right", 0, 0}, {"DPad Up", 0, 0}, {"DPad Down", 0, 0}, {"A", 0, 0},
				{"B", 0, 0}, {"X", 0, 0}, {"Y", 0, 0},
				{"Left Trigger", cGamepadTriggerMax, XINPUT_GAMEPAD_TRIGGER_THRESHOLD},
				{"Right Trigger", cGamepadTriggerMax, XINPUT_GAMEPAD_TRIGGER_THRESHOLD}, {"Left Shoulder", 0, 0},
				{"Right Shoulder", 0, 0}, {"L.Thumb Press", 0, 0}, {"R.Thumb Press", 0, 0},
				{"L.Thumb Move X", cGamepadLeftThumbMax, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE},
				{"L.Thumb Move Y", cGamepadLeftThumbMax, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE},
				{"R.Thumb Move X", cGamepadRightThumbMax, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE},
				{"R.Thumb Move Y", cGamepadRightThumbMax, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE},
				{"L.Thumb Left", cGamepadLeftThumbMax, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE},
				{"L.Thumb Right", cGamepadLeftThumbMax, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE},
				{"L.Thumb Up", cGamepadLeftThumbMax, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE},
				{"L.Thumb Down", cGamepadLeftThumbMax, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE},
				{"R.Thumb Left", cGamepadLeftThumbMax, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE},
				{"R.Thumb Right", cGamepadLeftThumbMax, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE},
				{"R.Thumb Up", cGamepadLeftThumbMax, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE},
				{"R.Thumb Down", cGamepadLeftThumbMax, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE}, {"Back", 0, 0},
				{"Start", 0, 0}, {"New Button", 0, 0}, {"New Button", 0, 0}, {"New Button", 0, 0}, {"New Button", 0, 0},
				{"New Button", 0, 0}, {"New Button", 0, 0}, {"New Button", 0, 0}, {"New Button", 0, 0},
				{"New Button", 0, 0}, {"New Button", 0, 0}, {"New Button", 0, 0}, {"New Button", 0, 0},
				{"New Button", 0, 0}, {"New Button", 0, 0}, {"New Button", 0, 0}, {"New Button", 0, 0},
				{"New Button", 0, 0}, {"New Button", 0, 0}, {"New Button", 0, 0}, {"New Button", 0, 0},
				{"New Button", 0, 0}, {"New Button", 0, 0}, {"New Button", 0, 0}, {"New Button", 0, 0},
				{"New Button", 0, 0}, {"New Button", 0, 0}, {"New Button", 0, 0}, {"New Button", 0, 0},
				{"New Button", 0, 0}, {"New Button", 0, 0}, {"New Button", 0, 0}, {"New Button", 0, 0},
				{"New Button", 0, 0}, {"New Button", 0, 0}, {"New Button", 0, 0}, {"New Button", 0, 0},
				{"New Button", 0, 0}, {"New Button", 0, 0}, {"New Button", 0, 0}, {"New Button", 0, 0},
				{"New Button", 0, 0}, {"None", 0, 0}
			}
		};

		const lsl::string cGameActionStr[cGameActionEnd] =
		{  	"gaAccelSec",
			"gaWheelLeftSec",
			"gaWheelRightSec",
			"gaBackSec",
			"gaBreakSec",
			"gaHyperSec",
			"gaMineSec",
			"gaShot1Sec",
			"gaShot2Sec",
			"gaShot3Sec",
			"gaShot4Sec",
			"gaRapidModeSec",
			"gaEasyModeSec",
			"gaResetCarSec",
			"gaViewSwitchSec",
			"gaAccel",
			"gaBack",
			"gaWheelLeft",
			"gaWheelRight",
			"gaBreak",
			"gaHyper",
			"gaMine",
			"gaShot1",
			"gaShot2",
			"gaShot3",
			"gaShot4",
			"gaRapidMode",
			"gaEasyMode",
			"gaResetCar",
			"gaViewSwitch",

			"gaEscape", 
			"gaChat", 
			"gaAction", 
			"gaScreenMode" };

		const string cControllerTypeStr[cControllerTypeEnd] = {"ctKeyboard", "ctGamepad"};


		ControlManager::ControlManager(World* world): _world(world)
		{
			_controllerStates[ctKeyboard] = new ControllerState();
			_controllerStates[ctGamepad] = new XInputState();

			ZeroMemory(_gameKeys, sizeof(_gameKeys));

			_gameKeys[ctKeyboard][gaAccelSec] = CharToVirtualKey('W');
			_gameKeys[ctKeyboard][gaBackSec] = CharToVirtualKey('S');
			_gameKeys[ctKeyboard][gaWheelLeftSec] = CharToVirtualKey('A');
			_gameKeys[ctKeyboard][gaWheelRightSec] = CharToVirtualKey('D');

			_gameKeys[ctKeyboard][gaRapidModeSec] = cVirtualKeyEnd;
			_gameKeys[ctKeyboard][gaEasyModeSec] = vkShift;
			_gameKeys[ctKeyboard][gaBreakSec] = vkCtrl;
			_gameKeys[ctKeyboard][gaHyperSec] = CharToVirtualKey('J');
			_gameKeys[ctKeyboard][gaMineSec] = CharToVirtualKey('K');

			_gameKeys[ctKeyboard][gaShot1Sec] = CharToVirtualKey('H');
			_gameKeys[ctKeyboard][gaShot2Sec] = CharToVirtualKey('Y');
			_gameKeys[ctKeyboard][gaShot3Sec] = CharToVirtualKey('U');
			_gameKeys[ctKeyboard][gaShot4Sec] = CharToVirtualKey('I');
			_gameKeys[ctKeyboard][gaViewSwitchSec] = CharToVirtualKey('C');

			_gameKeys[ctKeyboard][gaResetCarSec] = CharToVirtualKey('R');


			//second
			_gameKeys[ctKeyboard][gaAccel] = vkUp;
			_gameKeys[ctKeyboard][gaBack] = vkDown;
			_gameKeys[ctKeyboard][gaWheelLeft] = vkLeft;
			_gameKeys[ctKeyboard][gaWheelRight] = vkRight;

			_gameKeys[ctKeyboard][gaRapidMode] = cVirtualKeyEnd;
			_gameKeys[ctKeyboard][gaEasyMode] = cVirtualKeyEnd;
			_gameKeys[ctKeyboard][gaBreak] = cVirtualKeyEnd;

			_gameKeys[ctKeyboard][gaHyper] = cVirtualKeyEnd;
			_gameKeys[ctKeyboard][gaMine] = cVirtualKeyEnd;

			_gameKeys[ctKeyboard][gaShot1] = cVirtualKeyEnd;
			_gameKeys[ctKeyboard][gaShot2] = cVirtualKeyEnd;
			_gameKeys[ctKeyboard][gaShot3] = cVirtualKeyEnd;
			_gameKeys[ctKeyboard][gaShot4] = cVirtualKeyEnd;

			_gameKeys[ctKeyboard][gaResetCar] = cVirtualKeyEnd;
			_gameKeys[ctKeyboard][gaViewSwitch] = cVirtualKeyEnd;

			_gameKeys[ctKeyboard][gaEscape] = vkBack;
			_gameKeys[ctKeyboard][gaChat] = vkStart;
			_gameKeys[ctKeyboard][gaAction] = vkStart;
			_gameKeys[ctKeyboard][gaScreenMode] = vkF12;

			/// ///////////////////////////////////////////////////////////////////////

			_gameKeys[ctGamepad][gaAccel] = vkButtonA;
			_gameKeys[ctGamepad][gaBack] = vkDown;
			_gameKeys[ctGamepad][gaWheelLeft] = vkLeft;
			_gameKeys[ctGamepad][gaWheelRight] = vkRight;
			_gameKeys[ctGamepad][gaRapidMode] = vkShoulderRight;
			_gameKeys[ctGamepad][gaEasyMode] = cVirtualKeyEnd;

			_gameKeys[ctGamepad][gaAccelSec] = cVirtualKeyEnd;
			_gameKeys[ctGamepad][gaBackSec] = cVirtualKeyEnd;
			_gameKeys[ctGamepad][gaWheelLeftSec] = cVirtualKeyEnd;
			_gameKeys[ctGamepad][gaWheelRightSec] = cVirtualKeyEnd;
			_gameKeys[ctGamepad][gaRapidModeSec] = cVirtualKeyEnd;
			_gameKeys[ctGamepad][gaEasyModeSec] = cVirtualKeyEnd;
			_gameKeys[ctGamepad][gaBreakSec] = cVirtualKeyEnd;

			_gameKeys[ctGamepad][gaResetCar] = cVirtualKeyEnd;
			_gameKeys[ctGamepad][gaResetCarSec] = cVirtualKeyEnd;
			_gameKeys[ctGamepad][gaBreak] = cVirtualKeyEnd;
			_gameKeys[ctGamepad][gaHyper] = vkButtonY;
			_gameKeys[ctGamepad][gaHyperSec] = vkButtonY;
			_gameKeys[ctGamepad][gaMine] = vkButtonB;
			_gameKeys[ctGamepad][gaMineSec] = vkButtonB;
			_gameKeys[ctGamepad][gaShot1] = cVirtualKeyEnd;
			_gameKeys[ctGamepad][gaShot2] = cVirtualKeyEnd;
			_gameKeys[ctGamepad][gaShot3] = cVirtualKeyEnd;
			_gameKeys[ctGamepad][gaShot4] = cVirtualKeyEnd;

			_gameKeys[ctGamepad][gaShot1Sec] = cVirtualKeyEnd;
			_gameKeys[ctGamepad][gaShot2Sec] = cVirtualKeyEnd;
			_gameKeys[ctGamepad][gaShot3Sec] = cVirtualKeyEnd;
			_gameKeys[ctGamepad][gaShot4Sec] = cVirtualKeyEnd;
			_gameKeys[ctGamepad][gaViewSwitch] = vkBack;
			_gameKeys[ctGamepad][gaViewSwitchSec] = cVirtualKeyEnd;
			_gameKeys[ctGamepad][gaEscape] = cVirtualKeyEnd;
			_gameKeys[ctGamepad][gaChat] = cVirtualKeyEnd;
			_gameKeys[ctGamepad][gaAction] = vkStart;
			_gameKeys[ctGamepad][gaScreenMode] = cVirtualKeyEnd;
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
			case vkF8:
				return GetAsyncKey(VK_F8) == akDown;
			case vkF9:
				return GetAsyncKey(VK_F9) == akDown;
			case vkF11:
				return GetAsyncKey(VK_F11) == akDown;
			case vkF12:
				return GetAsyncKey(VK_F12) == akDown;
			case vkInsert:
				return GetAsyncKey(VK_INSERT) == akDown;
			case vkDelete:
				return GetAsyncKey(VK_DELETE) == akDown;
			case vkHome:
				return GetAsyncKey(VK_HOME) == akDown;
			case vkEnd:
				return GetAsyncKey(VK_END) == akDown;
			case vkTab:
				return GetAsyncKey(VK_TAB) == akDown;
			case vkCaps:
				return GetAsyncKey(VK_CAPITAL) == akDown;
			case vkShift:
				return GetAsyncKey(VK_SHIFT) == akDown;
			case vkCtrl:
				return GetAsyncKey(VK_CONTROL) == akDown;
			case vkNum1:
				return GetAsyncKey(VK_NUMPAD1) == akDown;
			case vkNum0:
				return GetAsyncKey(VK_NUMPAD0) == akDown;
			case vkNum2:
				return GetAsyncKey(VK_NUMPAD2) == akDown;
			case vkNum3:
				return GetAsyncKey(VK_NUMPAD3) == akDown;
			case vkNum4:
				return GetAsyncKey(VK_NUMPAD4) == akDown;
			case vkNum5:
				return GetAsyncKey(VK_NUMPAD5) == akDown;
			case vkNum6:
				return GetAsyncKey(VK_NUMPAD6) == akDown;
			case vkNum7:
				return GetAsyncKey(VK_NUMPAD7) == akDown;
			case vkNum8:
				return GetAsyncKey(VK_NUMPAD8) == akDown;
			case vkNum9:
				return GetAsyncKey(VK_NUMPAD9) == akDown;
			case vkPlus:
				return GetAsyncKey(VK_ADD) == akDown;
			case vkMinus:
				return GetAsyncKey(VK_SUBTRACT) == akDown;
			case vkMultiply:
				return GetAsyncKey(VK_MULTIPLY) == akDown;
			case vkDivide:
				return GetAsyncKey(VK_DIVIDE) == akDown;
			case VK_OEM_3:
				return GetAsyncKey(VK_LMENU) == akDown;
			case vkPgDwn:
				return GetAsyncKey(VK_MENU) == akDown;
			case vkPgUp:
				return GetAsyncKey(VK_MENU) == akDown;
			case vkNumLock:
				return GetAsyncKey(VK_NUMLOCK) == akDown;
			case vkDot:
				return GetAsyncKey(VK_DECIMAL) == akDown;
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

			auto xInput = static_cast<XInputState*>(_controllerStates[ctGamepad]);
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
			case vkF8:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkF9:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkF11:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkF12:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkInsert:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkDelete:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkHome:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkEnd:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkTab:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkCaps:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkShift:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkCtrl:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkNum1:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkNum0:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkNum2:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkNum3:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkNum4:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkNum5:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkNum6:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkNum7:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkNum8:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkNum9:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkPlus:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkMinus:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkMultiply:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkDivide:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkSymbol1:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkSymbol2:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkSymbol3:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkSymbol4:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkSymbol5:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkSymbol6:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkSymbol7:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkSymbol8:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkSymbol9:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkSymbol10:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkSymbol11:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkPgDwn:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkPgUp:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkNumLock:
				if (xInput->state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
					return 1;
				break;
			case vkDot:
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
			auto state = static_cast<XInputState*>(_controllerStates[ctGamepad]);

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

				msg.state = cKeyStateEnd;
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
			for (auto iter = _eventList.begin(); iter != _eventList.end(); ++iter)
			{
				if ((*iter)->OnHandleInput(msg))
					return true;
			}

			return false;
		}

		bool ControlManager::OnMouseClickEvent(const MouseClick& mClick)
		{
			for (auto iter = _eventList.begin(); iter != _eventList.end(); ++iter)
				if ((*iter)->OnMouseClickEvent(mClick))
					return true;

			return false;
		}

		bool ControlManager::OnMouseMoveEvent(const MouseMove& mMove)
		{
			for (auto iter = _eventList.begin(); iter != _eventList.end(); ++iter)
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
			case VK_F8:
				msg.key = vkF8;
				break;
			case VK_F9:
				msg.key = vkF9;
				break;
			case VK_F11:
				msg.key = vkF11;
				break;
			case VK_F12:
				msg.key = vkF12;
				break;
			case VK_INSERT:
				msg.key = vkInsert;
				break;
			case VK_DELETE:
				msg.key = vkDelete;
				break;
			case VK_HOME:
				msg.key = vkHome;
				break;
			case VK_END:
				msg.key = vkEnd;
				break;
			case VK_TAB:
				msg.key = vkTab;
				break;
			case VK_CAPITAL:
				msg.key = vkCaps;
				break;
			case VK_SHIFT:
				msg.key = vkShift;
				break;
			case VK_CONTROL:
				msg.key = vkCtrl;
				break;
			case VK_NUMPAD1:
				msg.key = vkNum1;
				break;
			case VK_NUMPAD0:
				msg.key = vkNum0;
				break;
			case VK_NUMPAD2:
				msg.key = vkNum2;
				break;
			case VK_NUMPAD3:
				msg.key = vkNum3;
				break;
			case VK_NUMPAD4:
				msg.key = vkNum4;
				break;
			case VK_NUMPAD5:
				msg.key = vkNum5;
				break;
			case VK_NUMPAD6:
				msg.key = vkNum6;
				break;
			case VK_NUMPAD7:
				msg.key = vkNum7;
				break;
			case VK_NUMPAD8:
				msg.key = vkNum8;
				break;
			case VK_NUMPAD9:
				msg.key = vkNum9;
				break;
			case VK_ADD:
				msg.key = vkPlus;
				break;
			case VK_SUBTRACT:
				msg.key = vkMinus;
				break;
			case VK_MULTIPLY:
				msg.key = vkMultiply;
				break;
			case VK_DIVIDE:
				msg.key = vkDivide;
				break;
			case VK_OEM_3:
				msg.key = vkSymbol1;
				break;
			case VK_OEM_MINUS:
				msg.key = vkSymbol2;
				break;
			case VK_OEM_PLUS:
				msg.key = vkSymbol3;
				break;
			case VK_OEM_4:
				msg.key = vkSymbol4;
				break;
			case VK_OEM_6:
				msg.key = vkSymbol5;
				break;
			case VK_OEM_1:
				msg.key = vkSymbol6;
				break;
			case VK_OEM_7:
				msg.key = vkSymbol7;
				break;
			case VK_OEM_5:
				msg.key = vkSymbol8;
				break;
			case VK_OEM_COMMA:
				msg.key = vkSymbol9;
				break;
			case VK_OEM_PERIOD:
				msg.key = vkSymbol10;
				break;
			case VK_OEM_2:
				msg.key = vkSymbol11;
				break;
			case VK_PRIOR:
				msg.key = vkPgDwn;
				break;
			case VK_NEXT:
				msg.key = vkPgUp;
				break;
			case VK_NUMLOCK:
				msg.key = vkNumLock;
				break;
			case VK_DECIMAL:
				msg.key = vkDot;
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

		bool ControlManager::OnKeyChar(unsigned key, KeyState state, bool repeat)
		{
			InputMessage msg;
			msg.key = cVirtualKeyEnd;
			msg.unicode = 0;
			msg.state = state;
			msg.controller = ctKeyboard;
			msg.repeat = repeat;
			msg.key = vkChar;
			msg.action = cGameActionEnd;
			msg.unicode = ConvertStrAToW(stringA(1, key), CP_ACP)[0];

			HandleInput(msg);

			return true;
		}

		void ControlManager::OnProgress(float deltaTime)
		{
			UpdateControllerState();

			for (auto iter = _eventList.begin(); iter != _eventList.end(); ++iter)
				(*iter)->OnInputProgress(deltaTime);
		}

		void ControlManager::OnFrame(float deltaTime)
		{
			for (auto iter = _eventList.begin(); iter != _eventList.end(); ++iter)
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
			if (res & ~0x1)
				return akDown;
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

		Point ControlManager::GetMousePos()
		{
			POINT pnt;
			if (GetCursorPos(&pnt) && ScreenToClient(_world->GetView()->GetDesc().handle, &pnt))
			{
				return _world->GetView()->ScreenToView(Point(pnt.x, pnt.y));
			}

			return _world->GetView()->ScreenToView(Point(0, 0));
		}

		D3DXVECTOR2 ControlManager::GetMouseVec()
		{
			Point mPnt = GetMousePos();
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
					gameActions.push_back(static_cast<GameAction>(i));
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

		VirtualKey ControlManager::GetVirtualKeyFromName(ControllerType controller, const string& name)
		{
			if (cVirtualKeyInfo[controller][cVirtualKeyEnd].name == name)
				return cVirtualKeyEnd;

			for (int i = 0; i < cVirtualKeyEnd; ++i)
				if (cVirtualKeyInfo[controller][i].name == name)
					return static_cast<VirtualKey>(i);

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
			return static_cast<char>(key - vkChar);
		}

		VirtualKey ControlManager::CharToVirtualKey(char unicode)
		{
			return static_cast<VirtualKey>(static_cast<int>(vkChar) + unicode);
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
				return static_cast<float>(res);
			if (res > static_cast<int>(info.alphaThreshold))
				return (res - static_cast<int>(info.alphaThreshold)) / d;
			if (res < -static_cast<int>(info.alphaThreshold))
				return (res + static_cast<int>(info.alphaThreshold)) / d;

			return 0.0f;
		}

		float ControlManager::GetGameActionState(GameAction action, bool withAlpha)
		{
			for (int i = 0; i < cControllerTypeEnd; ++i)
			{
				auto controller = static_cast<ControllerType>(i);
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
