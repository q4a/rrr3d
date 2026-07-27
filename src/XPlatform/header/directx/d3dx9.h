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
 * (d3dx9tex.h, d3dx9anim.h, d3dx9shape.h) are added when needed.
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

/*
 * Declarations only -- there is no open implementation of the D3DX effects
 * framework. Shader.cpp needs ID3DXEffect, ID3DXInclude, D3DXMACRO and
 * D3DXHANDLE to compile; something has to provide them before it links.
 */
#include "d3dx9shader.h"
#include "d3dx9effect.h"

#endif /* __D3DX9_H__ */
