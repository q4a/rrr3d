/*
 * Minimal compatibility layer for the vendored Wine d3dx9 math code.
 *
 * Wine's d3dx9math.h includes "d3dx9.h" and its math.c includes
 * "d3dx9_private.h"; both are Wine-internal and pull in the rest of the
 * d3dx9 DLL. All the math actually needs is a handful of scalar typedefs,
 * the three D3D base types, and no-op logging macros. That is what this
 * provides, so the math can be built standalone on any platform.
 *
 * On Windows the DirectX/Windows SDK already defines these, so we defer
 * to it rather than risk conflicting definitions.
 */

#ifndef D3DX9_COMPAT_H
#define D3DX9_COMPAT_H

#ifdef _WIN32

#include <windows.h>
#include <d3d9types.h>

#else /* !_WIN32 */

/* One source of truth for the Windows scalar types and Win32 calls. */
#include "xplatform.h"

/* D3D base types, layout-identical to d3d9types.h */
typedef DWORD D3DCOLOR;

typedef struct _D3DVECTOR {
    float x;
    float y;
    float z;
} D3DVECTOR;

typedef struct _D3DCOLORVALUE {
    float r;
    float g;
    float b;
    float a;
} D3DCOLORVALUE;

typedef struct _D3DMATRIX {
    union {
        struct {
            float _11, _12, _13, _14;
            float _21, _22, _23, _24;
            float _31, _32, _33, _34;
            float _41, _42, _43, _44;
        };
        float m[4][4];
    };
} D3DMATRIX;

typedef struct _D3DVIEWPORT9 {
    DWORD X;
    DWORD Y;
    DWORD Width;
    DWORD Height;
    float MinZ;
    float MaxZ;
} D3DVIEWPORT9;

/*
 * Only referenced by the spherical-harmonics cube map declarations, which
 * this project never calls. An opaque type is enough to let the header parse.
 */
typedef struct IDirect3DCubeTexture9 IDirect3DCubeTexture9;

/* HRESULT values and tests used by the math code */
#define S_OK                ((HRESULT)0)
#define S_FALSE             ((HRESULT)1)
#define D3D_OK              S_OK
#define E_OUTOFMEMORY       ((HRESULT)0x8007000EL)
#define D3DERR_INVALIDCALL  ((HRESULT)0x8876086CL)

#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr)    (((HRESULT)(hr)) <  0)

/*
 * windef.h defines these; the vendored math.c expects them. Deliberately
 * restricted to C translation units -- the C++ side of this project builds
 * with NOMINMAX and uses std::min/std::max, which these macros would break.
 */
#ifndef __cplusplus
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
#endif

#endif /* !_WIN32 */

/*
 * Half-float conversion, defined in d3dx9_float16.c. Declared here because
 * Wine keeps these in d3dx_helpers.h, which we do not vendor.
 */
#ifdef __cplusplus
extern "C" {
#endif
unsigned short float_32_to_16(const float in);
float float_16_to_32(const unsigned short in);
#ifdef __cplusplus
}
#endif

/*
 * Wine's math.c logs through the DLL's debug channels. Nothing here needs
 * them, so compile them out. The variadic no-op still parses the argument
 * list, so unused-variable warnings stay suppressed.
 */
#ifndef TRACE
#define TRACE(...)  do { } while (0)
#endif
#ifndef WARN
#define WARN(...)   do { } while (0)
#endif
#ifndef ERR
#define ERR(...)    do { } while (0)
#endif
#ifndef FIXME
#define FIXME(...)  do { } while (0)
#endif
#ifndef WINE_DEFAULT_DEBUG_CHANNEL
#define WINE_DEFAULT_DEBUG_CHANNEL(x)
#endif

#endif /* D3DX9_COMPAT_H */
