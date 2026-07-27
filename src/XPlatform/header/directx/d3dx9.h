/*
 * d3dx9.h umbrella header.
 *
 * This replaces MinGW-w64's d3dx9.h rather than vendoring it, for one reason:
 * MinGW's version includes its own d3dx9math.h, and this project uses Wine's
 * instead -- because Wine is the only one of the two that also ships an
 * implementation, which MathLib vendors and builds. Including both would
 * redefine every D3DX math type.
 *
 * The guard name matches MinGW's so that the sub-headers below, which all
 * include "d3dx9.h" back, see it as already included.
 *
 * Only the sub-headers this project actually needs are pulled in. The rest
 * (d3dx9anim.h, d3dx9xof.h) are added when needed.
 */

#ifndef __D3DX9_H__
#define __D3DX9_H__

#include <limits.h>

#include "d3d9.h"

/* GDI and OLE declarations the d3dx9 headers expect from <windows.h> */
#include "gdi_compat.h"

/* Wine's, with the out-of-line functions implemented in MathLib */
#include "d3d/d3dx9math.h"

#include "d3dx9core.h"
#include "d3dx9mesh.h"

/* D3DXIMAGE_INFO, D3DXCreateTextureFromFileEx, D3DX_DEFAULT, and
 * D3DXCreateSphere. Declarations only, as with the effects framework. */
#include "d3dx9shape.h"

/*
 * Declarations only -- there is no open implementation of the D3DX effects
 * framework. Shader.cpp needs ID3DXEffect, ID3DXInclude, D3DXMACRO and
 * D3DXHANDLE to compile; something has to provide them before it links.
 */
/*
 * Constants the DirectX SDK defines but MinGW-w64's headers omit.
 */
#ifndef D3DX_DEFAULT
#define D3DX_DEFAULT ((UINT) -1)
#endif
#ifndef D3DX_DEFAULT_NONPOW2
#define D3DX_DEFAULT_NONPOW2 ((UINT) -2)
#endif
#ifndef D3DX_FROM_FILE
#define D3DX_FROM_FILE ((UINT) -3)
#endif
#ifndef D3DXERR_INVALIDDATA
#define D3DXERR_INVALIDDATA MAKE_D3DHRESULT(2900)
#endif

#include "d3dx9shader.h"
#include "d3dx9effect.h"

/* After d3dx9shader.h: d3dx9tex.h references ID3DXTextureShader. */
#include "d3dx9tex.h"

#endif /* __D3DX9_H__ */
