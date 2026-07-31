#ifndef XPLATFORM_D3D9TYPES_H
#define XPLATFORM_D3D9TYPES_H

/*
 * The Direct3D 9 base types.
 *
 * Only what the D3DX math layer needs so far -- D3DXVECTOR3 derives from
 * _D3DVECTOR, D3DXMATRIX from _D3DMATRIX, and D3DXCOLOR converts to and from
 * D3DCOLORVALUE, so those three have to exist before d3dx9math.h will parse.
 * It grows as the graphics work brings up the rest of the D3D9 surface.
 *
 * Layouts are fixed by the API and are what every caller and every shader
 * constant upload assumes: D3DMATRIX is row-major, m[row][column].
 */

#include "xplatform.h"

typedef float FLOAT;

typedef struct _D3DVECTOR
{
	float x;
	float y;
	float z;
} D3DVECTOR;

typedef struct _D3DCOLORVALUE
{
	float r;
	float g;
	float b;
	float a;
} D3DCOLORVALUE;

typedef struct _D3DMATRIX
{
	union
	{
		struct
		{
			float _11, _12, _13, _14;
			float _21, _22, _23, _24;
			float _31, _32, _33, _34;
			float _41, _42, _43, _44;
		};
		float m[4][4];
	};
} D3DMATRIX;

/* Declared, not defined: d3dx9math.h has one prototype taking a cube texture
   pointer (D3DXSHProjectCubeMap), which nothing here calls. The real interface
   arrives with the D3D9 headers proper. */
struct IDirect3DCubeTexture9;
typedef struct IDirect3DCubeTexture9 IDirect3DCubeTexture9;

typedef struct _D3DVIEWPORT9
{
	DWORD X;
	DWORD Y;
	DWORD Width;
	DWORD Height;
	float MinZ;
	float MaxZ;
} D3DVIEWPORT9;

#define D3D_OK S_OK
#define D3DERR_INVALIDCALL ((HRESULT)0x8876086c)

/* D3DCOLOR is a packed ARGB word, not the float quad above. */
typedef DWORD D3DCOLOR;

#define D3DCOLOR_ARGB(a, r, g, b) \
	((D3DCOLOR)((((a) & 0xff) << 24) | (((r) & 0xff) << 16) | \
	            (((g) & 0xff) << 8) | ((b) & 0xff)))
#define D3DCOLOR_RGBA(r, g, b, a) D3DCOLOR_ARGB(a, r, g, b)
#define D3DCOLOR_XRGB(r, g, b)    D3DCOLOR_ARGB(0xff, r, g, b)

#endif
