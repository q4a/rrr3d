#ifndef XPLATFORM_WINDOWS_H
#define XPLATFORM_WINDOWS_H

/*
 * Stands in for <windows.h> so the engine's includes need no #ifdef.
 *
 * This directory is only on the include path off Windows, so on Windows the
 * real header is found instead and this file is never seen.
 */

#include "xplatform.h"

/*
 * IUnknown, and with it the COM vtable machinery the DirectX headers declare
 * their interfaces against. On Windows this arrives through windows.h and
 * objbase.h; DXVK's vendored objbase.h does not include unknwn.h itself, so the
 * bridge is made here rather than by editing a vendored file.
 */
#include "windows/unknwn.h"

/* GDI's font types and constants, which the D3DX font interfaces are declared
   in terms of. */
#include "wingdi.h"

/* The DrawText format flags and SetRect, for the same reason. */
#include "winuser.h"

#endif
