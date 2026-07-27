/*
 * XInput 1.3 declarations -- the subset src/Rock3dGame/game/ControlManager.cpp
 * uses, and nothing else.
 *
 * As with xaudio2.h, this is a compile-and-link seam rather than an
 * implementation: XInputGetState reports no controller connected, so the game
 * falls back to keyboard input instead of failing to build.
 *
 * SDL_GameController is the intended replacement and maps onto this almost
 * one-for-one. The exception is XInputGetKeystroke, which has no SDL analogue
 * at all -- SDL reports button state, not press/release/repeat events -- so
 * that one needs explicit edge detection when the input port is done.
 */

#ifndef XPLATFORM_XINPUT_H
#define XPLATFORM_XINPUT_H

#ifdef _WIN32
#error "xinput.h here is the non-Windows substitute; Windows has its own"
#endif

#include "windows/windows_base.h"

#define XINPUT_GAMEPAD_DPAD_UP          0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN        0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT        0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT       0x0008
#define XINPUT_GAMEPAD_START            0x0010
#define XINPUT_GAMEPAD_BACK             0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB       0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB      0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER    0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER   0x0200
#define XINPUT_GAMEPAD_A                0x1000
#define XINPUT_GAMEPAD_B                0x2000
#define XINPUT_GAMEPAD_X                0x4000
#define XINPUT_GAMEPAD_Y                0x8000

#define XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE   7849
#define XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE  8689
#define XINPUT_GAMEPAD_TRIGGER_THRESHOLD     30

#define XINPUT_FLAG_GAMEPAD             0x00000001

#define XINPUT_KEYSTROKE_KEYDOWN        0x0001
#define XINPUT_KEYSTROKE_KEYUP          0x0002
#define XINPUT_KEYSTROKE_REPEAT         0x0004

#define ERROR_DEVICE_NOT_CONNECTED      1167
#define ERROR_EMPTY                     4306

typedef struct _XINPUT_GAMEPAD
{
    WORD  wButtons;
    BYTE  bLeftTrigger;
    BYTE  bRightTrigger;
    SHORT sThumbLX;
    SHORT sThumbLY;
    SHORT sThumbRX;
    SHORT sThumbRY;
} XINPUT_GAMEPAD, *PXINPUT_GAMEPAD;

typedef struct _XINPUT_STATE
{
    DWORD          dwPacketNumber;
    XINPUT_GAMEPAD Gamepad;
} XINPUT_STATE, *PXINPUT_STATE;

typedef struct _XINPUT_KEYSTROKE
{
    WORD  VirtualKey;
    WCHAR Unicode;
    WORD  Flags;
    BYTE  UserIndex;
    BYTE  HidCode;
} XINPUT_KEYSTROKE, *PXINPUT_KEYSTROKE;

DWORD XInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState);
DWORD XInputGetKeystroke(DWORD dwUserIndex, DWORD dwReserved, PXINPUT_KEYSTROKE pKeystroke);

#endif /* XPLATFORM_XINPUT_H */
