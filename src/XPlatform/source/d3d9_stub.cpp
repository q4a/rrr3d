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
#include <cstring>

namespace
{

/*
 * Reported once, for the two that are genuinely absent.
 *
 * The other three say nothing at all. Failing is what they are FOR -- the
 * engine reads E_NOTIMPL as "take the other branch" -- and
 * D3DXGetImageInfoFromFileW is called once per texture, so reporting it would
 * put several hundred lines of "error" on stderr to describe the design
 * working. See this file's header.
 */
void ReportMissing(const char* what, bool& reported)
{
	if (reported)
		return;
	reported = true;

	std::fprintf(stderr, "rrr3d: %s is not implemented.\n", what);
}

}

/* The effects framework is implemented in d3dx_effect.cpp. */

/* ---- D3DX: texture loading ----
 *
 * These three are the DELIBERATE failures. VideoResource.cpp probes with
 * GetImageInfo and only hands the filename to D3DX if that succeeds; failing
 * it keeps every texture on the decode-in-memory branch, which is one code
 * path instead of two and owes D3DX no resampling or mip generation. Silent,
 * because this is the intended outcome and not a fault. */

HRESULT WINAPI D3DXCreateTextureFromFileExA(struct IDirect3DDevice9*, const char*,
	UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, DWORD, DWORD, D3DCOLOR,
	D3DXIMAGE_INFO*, PALETTEENTRY*, struct IDirect3DTexture9** texture)
{
	if (texture)
		*texture = NULL;
	return E_NOTIMPL;
}

HRESULT WINAPI D3DXCreateCubeTextureFromFileExA(struct IDirect3DDevice9*, const char*,
	UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, DWORD, DWORD, D3DCOLOR,
	D3DXIMAGE_INFO*, PALETTEENTRY*, struct IDirect3DCubeTexture9** cube)
{
	if (cube)
		*cube = NULL;
	return E_NOTIMPL;
}

HRESULT WINAPI D3DXGetImageInfoFromFileW(const WCHAR*, D3DXIMAGE_INFO* info)
{
	if (info)
		std::memset(info, 0, sizeof(*info));
	return E_NOTIMPL;
}

/* The two in-memory loaders are implemented in d3dx_texture.cpp. */

/* ---- and this one is genuinely absent ---- */

/* D3DXFilterTexture is implemented in d3dx_texture.cpp. */

/* D3DXCreateFontA is implemented in d3dx_font.cpp. */

/*
 * VideoResource.cpp:449 builds a sphere mesh for one primitive kind. A null
 * mesh is handled by the caller; the primitive simply does not appear.
 */
HRESULT WINAPI D3DXCreateSphere(struct IDirect3DDevice9*, float, UINT, UINT,
	struct ID3DXMesh** mesh, struct ID3DXBuffer** adjacency)
{
	static bool reported = false;
	ReportMissing("D3DXCreateSphere", reported);
	if (mesh)
		*mesh = NULL;
	if (adjacency)
		*adjacency = NULL;
	return E_NOTIMPL;
}
