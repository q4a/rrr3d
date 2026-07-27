/*
 * The graphics seam.
 *
 * These twelve entry points are the entire boundary between the ported engine
 * and a working renderer. Everything else in the tree compiles and links; this
 * file is what stands between the build and a running game.
 *
 * They are split in two, and the two halves have different futures:
 *
 *   Direct3DCreate9, Direct3DCreate9Ex
 *       Direct3D itself. The plan is DXVK's D3D9 front-end with a Metal
 *       backend behind DxvkContext -- 62 methods, derived in
 *       docs/macos-graphics-backend.md. This is the large piece.
 *
 *   The ten D3DX entry points
 *       Not part of Direct3D, so no backend choice provides them; they are
 *       ours regardless. Effects (ID3DXEffect over 23 .fx files), texture
 *       loading, ID3DXFont -- which is not debug-only here, every piece of UI
 *       text renders through it -- and one D3DXCreateSphere.
 *
 * Until then each reports failure rather than returning a half-built object.
 * The engine checks these results, so the game starts, logs, and stops at
 * device creation instead of crashing somewhere less informative.
 */

#include "xplatform.h"
#include "directx/d3d9.h"
#include "directx/d3dx9.h"

#include <cstdio>

namespace
{

/* Logged once so a failed launch says which piece is missing, not just that it failed. */
void ReportMissing(const char* what)
{
	static bool reported = false;
	if (!reported)
	{
		reported = true;
		std::fprintf(stderr,
			"rrr3d: no graphics backend on this platform yet -- %s.\n"
			"       See docs/macos-graphics-backend.md.\n", what);
	}
}

}

IDirect3D9* WINAPI Direct3DCreate9(UINT)
{
	ReportMissing("Direct3DCreate9");
	return NULL;
}

HRESULT WINAPI Direct3DCreate9Ex(UINT, IDirect3D9Ex** d3d9ex)
{
	ReportMissing("Direct3DCreate9Ex");
	if (d3d9ex)
		*d3d9ex = NULL;
	return E_NOTIMPL;
}

/* ---- D3DX: effects ---- */

HRESULT WINAPI D3DXCreateEffectPool(ID3DXEffectPool** pool)
{
	ReportMissing("D3DXCreateEffectPool");
	if (pool)
		*pool = NULL;
	return E_NOTIMPL;
}

HRESULT WINAPI D3DXCreateEffect(struct IDirect3DDevice9*, const void*, UINT,
	const D3DXMACRO*, struct ID3DXInclude*, DWORD,
	struct ID3DXEffectPool*, struct ID3DXEffect** effect, struct ID3DXBuffer** compilation_errors)
{
	ReportMissing("D3DXCreateEffect");
	if (effect)
		*effect = NULL;
	if (compilation_errors)
		*compilation_errors = NULL;
	return E_NOTIMPL;
}

/* ---- D3DX: texture loading ---- */

HRESULT WINAPI D3DXCreateTextureFromFileExA(struct IDirect3DDevice9*, const char*,
	UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, DWORD, DWORD, D3DCOLOR,
	D3DXIMAGE_INFO*, PALETTEENTRY*, struct IDirect3DTexture9** texture)
{
	ReportMissing("D3DXCreateTextureFromFileExA");
	if (texture)
		*texture = NULL;
	return E_NOTIMPL;
}

HRESULT WINAPI D3DXCreateCubeTextureFromFileExA(struct IDirect3DDevice9*, const char*,
	UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, DWORD, DWORD, D3DCOLOR,
	D3DXIMAGE_INFO*, PALETTEENTRY*, struct IDirect3DCubeTexture9** cube)
{
	ReportMissing("D3DXCreateCubeTextureFromFileExA");
	if (cube)
		*cube = NULL;
	return E_NOTIMPL;
}

HRESULT WINAPI D3DXCreateTextureFromFileInMemoryEx(struct IDirect3DDevice9*, const void*,
	UINT, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, DWORD, DWORD, D3DCOLOR,
	D3DXIMAGE_INFO*, PALETTEENTRY*, struct IDirect3DTexture9** texture)
{
	ReportMissing("D3DXCreateTextureFromFileInMemoryEx");
	if (texture)
		*texture = NULL;
	return E_NOTIMPL;
}

HRESULT WINAPI D3DXCreateCubeTextureFromFileInMemoryEx(struct IDirect3DDevice9*, const void*,
	UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, DWORD, DWORD, D3DCOLOR,
	D3DXIMAGE_INFO*, PALETTEENTRY*, struct IDirect3DCubeTexture9** cube)
{
	ReportMissing("D3DXCreateCubeTextureFromFileInMemoryEx");
	if (cube)
		*cube = NULL;
	return E_NOTIMPL;
}

HRESULT WINAPI D3DXFilterTexture(struct IDirect3DBaseTexture9*, const PALETTEENTRY*, UINT, DWORD)
{
	ReportMissing("D3DXFilterTexture");
	return E_NOTIMPL;
}

HRESULT WINAPI D3DXGetImageInfoFromFileW(const WCHAR*, D3DXIMAGE_INFO* info)
{
	ReportMissing("D3DXGetImageInfoFromFileW");
	if (info)
		std::memset(info, 0, sizeof(*info));
	return E_NOTIMPL;
}

/* ---- D3DX: text and geometry ---- */

HRESULT WINAPI D3DXCreateFontA(struct IDirect3DDevice9*, INT, UINT, UINT, UINT, BOOL,
	DWORD, DWORD, DWORD, DWORD, const char*, struct ID3DXFont** font)
{
	ReportMissing("D3DXCreateFontA");
	if (font)
		*font = NULL;
	return E_NOTIMPL;
}

HRESULT WINAPI D3DXCreateSphere(struct IDirect3DDevice9*, float, UINT, UINT,
	struct ID3DXMesh** mesh, struct ID3DXBuffer** adjacency)
{
	ReportMissing("D3DXCreateSphere");
	if (mesh)
		*mesh = NULL;
	if (adjacency)
		*adjacency = NULL;
	return E_NOTIMPL;
}
