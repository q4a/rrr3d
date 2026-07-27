/*
 * The two Direct3D 11 enums DXVK's shared-resource and annotation headers name.
 *
 * DXVK describes shared resources in D3D11's vocabulary because that is where
 * the interop actually happens; the D3D9 front-end only carries the values
 * through. Nothing here creates or consumes a D3D11 resource, so the enums are
 * all that is needed and their numeric values match the real header.
 */

#ifndef __XPLATFORM_D3D11_H__
#define __XPLATFORM_D3D11_H__

#ifdef _WIN32
#error "this d3d11.h is the non-Windows stand-in; Windows has the real one"
#endif

typedef enum D3D11_USAGE
{
    D3D11_USAGE_DEFAULT   = 0,
    D3D11_USAGE_IMMUTABLE = 1,
    D3D11_USAGE_DYNAMIC   = 2,
    D3D11_USAGE_STAGING   = 3
} D3D11_USAGE;

typedef enum D3D11_TEXTURE_LAYOUT
{
    D3D11_TEXTURE_LAYOUT_UNDEFINED         = 0,
    D3D11_TEXTURE_LAYOUT_ROW_MAJOR         = 1,
    D3D11_TEXTURE_LAYOUT_64K_STANDARD_SWIZZLE = 2
} D3D11_TEXTURE_LAYOUT;

/*
 * DXGI_FORMAT appears in DXVK's shared-resource description. Only the type is
 * needed -- the values are carried, never interpreted -- so the enum is left
 * open rather than transcribing 130 entries nothing reads.
 */
typedef enum DXGI_FORMAT
{
    DXGI_FORMAT_UNKNOWN              = 0,
    /* Named where DXVK maps a D3D9 format onto a shared-resource description. */
    DXGI_FORMAT_R10G10B10A2_UNORM    = 24,
    DXGI_FORMAT_R8G8B8A8_UNORM       = 28,
    DXGI_FORMAT_R16G16B16A16_FLOAT   = 10,
    DXGI_FORMAT_B8G8R8A8_UNORM       = 87,
    DXGI_FORMAT_B8G8R8X8_UNORM       = 88
} DXGI_FORMAT;

typedef struct DXGI_SAMPLE_DESC
{
    UINT Count;
    UINT Quality;
} DXGI_SAMPLE_DESC;

/* Bind and misc flags DXVK sets when describing a shared resource. */
#define D3D11_BIND_SHADER_RESOURCE      0x8
#define D3D11_BIND_RENDER_TARGET        0x20
#define D3D11_BIND_DEPTH_STENCIL        0x40
#define D3D11_BIND_UNORDERED_ACCESS     0x80

#define D3D11_RESOURCE_MISC_SHARED               0x2
#define D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX    0x100
#define D3D11_RESOURCE_MISC_SHARED_NTHANDLE      0x800

#ifdef __cplusplus

#include "windows/windows_base.h"

/*
 * The debug annotation interface, D3D11_1's. DXVK's own IDXVKUserDefinedAnnotation
 * derives from it so tools like Xcode's GPU capture can see event scopes.
 * Method order is the vtable contract and must not move.
 */
struct ID3DUserDefinedAnnotation: public IUnknown
{
    virtual INT STDMETHODCALLTYPE BeginEvent(LPCWSTR Name) = 0;
    virtual INT STDMETHODCALLTYPE EndEvent() = 0;
    virtual void STDMETHODCALLTYPE SetMarker(LPCWSTR Name) = 0;
    virtual BOOL STDMETHODCALLTYPE GetStatus() = 0;
};

#endif /* __cplusplus */

#endif /* __XPLATFORM_D3D11_H__ */
