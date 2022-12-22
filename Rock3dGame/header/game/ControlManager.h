#pragma once

namespace r3d
{
	namespace game
	{
		struct MouseClick
		{
			MouseClick(): key(mkLeft), state(ksUp), shift1(false), coord(0, 0)
			{
			}

			MouseKey key;
			KeyState state;
			bool shift1;
			//оконные координаты
			Point coord;
			//проекционные координаты в видовом пространстве, [-1.0..1.0]
			D3DXVECTOR2 projCoord;

			//экранный луч в мировом пространстве
			D3DXVECTOR3 scrRayPos;
			D3DXVECTOR3 scrRayVec;
		};

		struct MouseMove
		{
			MouseMove(): shift1(false), coord(0, 0)
			{
			}

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

		enum ControllerType { ctKeyboard = 0, ctGamepad, cControllerTypeEnd };

		extern const string cControllerTypeStr[cControllerTypeEnd];

		enum VirtualKey
		{
			vkLeft = 0,
			vkRight,
			vkUp,
			vkDown,
			vkButtonA,
			vkButtonB,
			vkButtonX,
			vkButtonY,
			vkTriggerLeft,
			vkTriggerRight,
			vkShoulderLeft,
			vkShoulderRight,
			vkThumbLeftPress,
			vkThumbRightPress,
			vkThumbLeftMoveX,
			vkThumbLeftMoveY,
			vkThumbRightMoveX,
			vkThumbRightMoveY,
			vkThumbLeftMoveLeft,
			vkThumbLeftMoveRight,
			vkThumbLeftModeUp,
			vkThumbLeftModeDown,
			vkThumbRightMoveLeft,
			vkThumbRightMoveRight,
			vkThumbRightModeUp,
			vkThumbRightModeDown,
			vkBack,
			vkStart,
			vkF8,
			vkF9,
			vkF11,
			vkF12,
			vkInsert,
			vkDelete,
			vkHome,
			vkEnd,
			vkTab,
			vkCaps,
			vkShift,
			vkCtrl,
			vkNum1,
			vkNum0,
			vkNum2,
			vkNum3,
			vkNum4,
			vkNum5,
			vkNum6,
			vkNum7,
			vkNum8,
			vkNum9,
			vkPlus,
			vkMinus,
			vkMultiply,
			vkDivide,
			vkSymbol1,
			vkSymbol2,
			vkSymbol3,
			vkSymbol4,
			vkSymbol5,
			vkSymbol6,
			vkSymbol7,
			vkSymbol8,
			vkSymbol9,
			vkSymbol10,
			vkSymbol11,
			vkPgDwn,
			vkPgUp,
			vkNumLock,
			vkDot,
			cVirtualKeyEnd,
			vkChar = 128,
			cVirtualKeyForce = INT_MAX
		};

		struct VirtualKeyInfo
		{
			std::string name;
			unsigned alphaMax;
			unsigned alphaThreshold;
		};

		extern const VirtualKeyInfo cVirtualKeyInfo[cControllerTypeEnd][cVirtualKeyEnd + 1];

		struct ControllerState
		{
			ControllerState(): plugged(false)
			{
			}

			virtual ~ControllerState()
			{
			}

			bool plugged;
		};

		enum GameAction
		{
			gaAccelSec = 0,
			gaWheelLeftSec,
			gaWheelRightSec,
			gaBackSec,
			gaBreakSec,
			gaHyperSec,
			gaMineSec,
			gaShot1Sec,
			gaShot2Sec,
			gaShot3Sec,
			gaShot4Sec,
			gaRapidModeSec,
			gaEasyModeSec,
			gaResetCarSec,
			gaViewSwitchSec,
			gaAccel,
			gaBack,
			gaWheelLeft,
			gaWheelRight,
			gaBreak,	
			gaHyper,
			gaMine,
			gaShot1,
			gaShot2,
			gaShot3,
			gaShot4,
			gaRapidMode,
			gaEasyMode,
			gaResetCar,		
			gaViewSwitch,
			gaEscape,
			gaChat,
			gaAction,
			gaScreenMode,

			cGameActionEnd,
			cGameActionUserEnd = gaAction
		};

		extern const string cGameActionStr[cGameActionEnd];

		struct InputMessage
		{
			GameAction action;
			VirtualKey key;
			KeyState state;
			bool repeat;
			ControllerType controller;
			WCHAR unicode;
		};

		class ControlEvent : public Object
		{
		public:
			virtual bool OnMouseClickEvent(const MouseClick& mClick) { return false; }
			virtual bool OnMouseMoveEvent(const MouseMove& mMove) { return false; }
			virtual bool OnHandleInput(const InputMessage& msg) { return false; }

			virtual void OnInputProgress(float deltaTime)
			{
			}

			virtual void OnInputFrame(float deltaTime)
			{
			}
		};

		class ControlManager
		{
			friend class World;
		private:
			using EventList = List<ControlEvent*>;
			using GameActions = Vector<GameAction>;
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
			bool OnKeyChar(unsigned key, KeyState state, bool repeat);
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

			Point GetMousePos();
			D3DXVECTOR2 GetMouseVec();

			const ControllerState& GetControllerState(ControllerType controller);
			void GetGameAction(ControllerType controller, VirtualKey key, GameActions& gameActions);

			VirtualKeyInfo GetVirtualKeyInfo(ControllerType controller, VirtualKey key);
			VirtualKey GetVirtualKeyFromName(ControllerType controller, const string& name);
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
