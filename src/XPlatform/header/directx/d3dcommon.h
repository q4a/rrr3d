/*
 * The handful of d3dcommon.h declarations DXVK's D3D9 front-end uses.
 *
 * Of the real header it wants exactly three things: D3D_OK, which d3d9.h
 * already defines here; D3D_MAX_SIMULTANEOUS_RENDERTARGETS; and ID3DBlob,
 * which it holds while disassembling a shader.
 *
 * ID3DBlob gets a real definition rather than a forward declaration because
 * the front-end calls GetBufferPointer and GetBufferSize on it. Note DXVK's own
 * comment at the call site: what actually arrives there is a D3DXBUFFER, which
 * shares ID3DBlob's leading vtable layout -- so the layout below has to stay
 * exactly IUnknown plus those two, in that order.
 */

#ifndef __XPLATFORM_D3DCOMMON_H__
#define __XPLATFORM_D3DCOMMON_H__

#ifdef _WIN32
#error "this d3dcommon.h is the non-Windows stand-in; Windows has the real one"
#endif

#include "windows/windows_base.h"

#ifndef D3D_OK
#define D3D_OK S_OK
#endif

#define D3D_MAX_SIMULTANEOUS_RENDERTARGETS 8

#ifdef __cplusplus

struct ID3DBlob: public IUnknown
{
    virtual void* STDMETHODCALLTYPE GetBufferPointer() = 0;
    virtual SIZE_T STDMETHODCALLTYPE GetBufferSize() = 0;
};

typedef ID3DBlob* LPD3DBLOB;

#endif /* __cplusplus */

#endif /* __XPLATFORM_D3DCOMMON_H__ */
