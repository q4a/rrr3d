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

#define XUSER_MAX_COUNT                 4
#define XUSER_INDEX_ANY                 0x000000FF

/* Virtual keys reported by XInputGetKeystroke. */
#define VK_PAD_A                        0x5800
#define VK_PAD_B                        0x5801
#define VK_PAD_X                        0x5802
#define VK_PAD_Y                        0x5803
#define VK_PAD_RSHOULDER                0x5804
#define VK_PAD_LSHOULDER                0x5805
#define VK_PAD_LTRIGGER                 0x5806
#define VK_PAD_RTRIGGER                 0x5807
#define VK_PAD_DPAD_UP                  0x5810
#define VK_PAD_DPAD_DOWN                0x5811
#define VK_PAD_DPAD_LEFT                0x5812
#define VK_PAD_DPAD_RIGHT               0x5813
#define VK_PAD_START                    0x5814
#define VK_PAD_BACK                     0x5815
#define VK_PAD_LTHUMB_PRESS             0x5816
#define VK_PAD_RTHUMB_PRESS             0x5817
#define VK_PAD_LTHUMB_UP                0x5820
#define VK_PAD_LTHUMB_DOWN              0x5821
#define VK_PAD_LTHUMB_RIGHT             0x5822
#define VK_PAD_LTHUMB_LEFT              0x5823
#define VK_PAD_RTHUMB_UP                0x5830
#define VK_PAD_RTHUMB_DOWN              0x5831
#define VK_PAD_RTHUMB_RIGHT             0x5832
#define VK_PAD_RTHUMB_LEFT              0x5833

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
