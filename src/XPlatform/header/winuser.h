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

#ifdef __cplusplus
extern "C" {
#endif

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
#define DrawText  DrawTextA

#endif
