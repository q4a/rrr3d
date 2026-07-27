/*
 * The D3DX seam: what is still missing.
 *
 * Direct3D itself now comes from src/D3D9Metal -- DXVK's D3D9 front-end over
 * d9mt's Metal backend -- so Direct3DCreate9 and Direct3DCreate9Ex are no
 * longer here.
 *
 * What remains is D3DX, which is not part of Direct3D and which no backend
 * choice provides. It is ours regardless:
 *
 *   ID3DXEffect over 23 .fx files, texture loading, and ID3DXFont -- which is
 *   not a debug-only concern here, every piece of UI text in the game renders
 *   through it.
 *
 * DDS loading has moved to d3dx_texture.cpp, which implements it. This file is
 * now only the part that does not exist yet. Each entry reports failure rather
 * than returning a half-built object, so the engine stops at a named point
 * instead of crashing somewhere less informative.
 *
 * D3DXGetImageInfoFromFileW and the file-path texture loaders are listed here
 * as unimplemented, but they are also unwanted: failing the GetImageInfo probe
 * is what keeps VideoResource.cpp on its decode-in-memory branch. See the
 * header of d3dx_texture.cpp.
 */

#include "xplatform.h"
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

/* The two in-memory loaders are implemented in d3dx_texture.cpp. */

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
