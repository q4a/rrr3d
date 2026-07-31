#ifndef XPLATFORM_WINUSER_H
#define XPLATFORM_WINUSER_H

/*
 * The slice of <winuser.h> the engine reaches for: the DrawText format flags
 * and SetRect.
 *
 * These arrive here rather than with the D3DX headers because they are Win32,
 * not DirectX -- ID3DXFont::DrawText is declared taking a Win32 RECT and Win32
 * DT_ flags, which is why an engine that never calls a GDI function still needs
 * them. What renders the text is Phase 8's business; what the flags *are* is a
 * fact about Win32 and settles here.
 *
 * Values transcribed from mingw-w64 v12.0.0 mingw-w64-headers/include/winuser.h
 * lines 3442-3455 -- the same upstream and tag the DirectX headers are vendored
 * from, so the two agree by construction.
 *
 * Only the ten the engine uses are defined. A DT_ flag that is not here is one
 * nothing calls, and it should stay a compile error rather than become a
 * silently-wrong constant.
 */

#include "windows/windows_base.h"

#define DT_TOP          0x00000000
#define DT_LEFT         0x00000000
#define DT_CENTER       0x00000001
#define DT_RIGHT        0x00000002
#define DT_VCENTER      0x00000004
#define DT_BOTTOM       0x00000008
#define DT_WORDBREAK    0x00000010
#define DT_EXPANDTABS   0x00000040
#define DT_NOCLIP       0x00000100
#define DT_CALCRECT     0x00000400

/* The base of the range Windows reserves for an application's own messages.
   IWorld.h derives WM_GRAPH_EVENT from it, which DirectShow posts cutscene
   notifications through -- so the value only has to be a number the rest of
   the port agrees on, and it may as well be the one it has always been. */
#define WM_APP  0x8000

/*
 * Virtual key codes, the 35 the game names. Same source as the DT_ flags above.
 *
 * These are *virtual* keys -- a layout-dependent identity, which is why phase
 * 5 maps SDL scancodes rather than keycodes for movement and then translates
 * to these. VK_LBUTTON, VK_RBUTTON and VK_MBUTTON are in the same numbering
 * because Windows treats mouse buttons as virtual keys; the engine's
 * GetAsyncKeyState calls rely on that.
 *
 * XInput's own VK_PAD_ codes are not here. They live in xinput.h, which is
 * where Windows puts them, in a 0x58xx range reserved so they cannot collide.
 */
#define VK_LBUTTON      0x01
#define VK_RBUTTON      0x02
#define VK_MBUTTON      0x04
#define VK_BACK         0x08
#define VK_RETURN       0x0D
#define VK_CONTROL      0x11
#define VK_ESCAPE       0x1B
#define VK_SPACE        0x20
#define VK_PRIOR        0x21
#define VK_NEXT         0x22
#define VK_LEFT         0x25
#define VK_UP           0x26
#define VK_RIGHT        0x27
#define VK_DOWN         0x28
#define VK_DELETE       0x2E
#define VK_NUMPAD0      0x60
#define VK_NUMPAD1      0x61
#define VK_NUMPAD2      0x62
#define VK_NUMPAD3      0x63
#define VK_NUMPAD4      0x64
#define VK_NUMPAD5      0x65
#define VK_NUMPAD6      0x66
#define VK_NUMPAD7      0x67
#define VK_NUMPAD8      0x68
#define VK_NUMPAD9      0x69
#define VK_ADD          0x6B
#define VK_SUBTRACT     0x6D
#define VK_F1           0x70
#define VK_F2           0x71
#define VK_F3           0x72
#define VK_F4           0x73
#define VK_F5           0x74
#define VK_F6           0x75
#define VK_F7           0x76
#define VK_OEM_PERIOD   0xBE

#ifdef __cplusplus
extern "C" {
#endif

/* High bit set means down, low bit means pressed since the last call. Phase 5
   answers it from SDL's keyboard state; the low bit has no SDL equivalent and
   the engine only ever tests the high bit. */
SHORT WINAPI GetAsyncKeyState(int virtualKey);

/* Cursor position in screen coordinates, and the conversion into a window's
   client coordinates. ControlManager.cpp:603 pairs them, and the "window" it
   passes is the CAMetalLayer the shell hands over as an HWND -- so like
   GetClientRect, ScreenToClient answers from what the shell published rather
   than from the handle. Phase 5. */
BOOL WINAPI GetCursorPos(LPPOINT point);
BOOL WINAPI ScreenToClient(HWND window, LPPOINT point);

/* Locale-aware on Windows; ControlManager.cpp:489 uses the pair to decide
   whether a key press is a printable character for text entry. */
BOOL WINAPI IsCharAlphaA(CHAR ch);
BOOL WINAPI IsCharAlphaNumericA(CHAR ch);

/* Assigns the four members and returns TRUE. It does not normalise the
   rectangle, which matters: every call site in the game passes 0 for right and
   bottom (Engine.cpp:121, AIPlayer.cpp:700) and relies on DT_NOCLIP to draw
   outside the resulting empty rect. Normalising would silently move the text. */
inline WINBOOL WINAPI SetRect(LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom)
{
	if (!lprc)
		return FALSE;

	lprc->left = xLeft;
	lprc->top = yTop;
	lprc->right = xRight;
	lprc->bottom = yBottom;
	return TRUE;
}

#ifdef __cplusplus
}
#endif

/*
 * Windows defines this, and the engine depends on it in a way that is easy to
 * miss: it has five DrawText methods of its own (gui::Context, gui::Text,
 * graph::TextFont), and on Windows every one of them is really named DrawTextA,
 * because this macro rewrites declaration and call site alike. It compiles there
 * precisely because the rewrite is uniform. Leaving the macro out would not be
 * "cleaner" -- it would rename ID3DXFont::DrawText's call sites and nothing
 * else, which is the one inconsistent outcome.
 */
#define DrawText            DrawTextA
#define IsCharAlpha         IsCharAlphaA
#define IsCharAlphaNumeric  IsCharAlphaNumericA

#endif
