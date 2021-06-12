#ifndef RENDER_DRIVER
#define RENDER_DRIVER

#include "lslObject.h"
#include "DriverTypes.h"

namespace r3d
{

namespace graph
{

class RenderDriver: public lsl::Object
{
public:
	static const unsigned cMinResWidth;
	static const unsigned cMinResHeight;
	//
	static const unsigned cMaxResWidth;
	static const unsigned cMaxResHeight;
	//
	static const unsigned cOptimalResWidth;
	static const unsigned cOptimalResHeight;
public:
	virtual HRESULT Present(const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) = 0;
	virtual HRESULT TestCooperativeLevel() = 0;
	virtual HRESULT Reset(D3DPRESENT_PARAMETERS& d3dpp) = 0;
	virtual HRESULT CheckDeviceFormat(UINT adapter, D3DDEVTYPE deviceType, D3DFORMAT adapterFormat, DWORD usage, D3DRESOURCETYPE rType, D3DFORMAT checkFormat) = 0;
	virtual HRESULT CheckDeviceMultiSampleType(UINT adapter, D3DDEVTYPE deviceType, D3DFORMAT surfaceFormat, BOOL windowed, D3DMULTISAMPLE_TYPE multiSampleType, DWORD* pQualityLevels) = 0;

	virtual HRESULT SetRenderState(RenderState state, DWORD value) = 0;
	virtual HRESULT SetTexture(DWORD sampler, IDirect3DBaseTexture9* value) = 0;
	virtual HRESULT SetSamplerState(DWORD sampler, SamplerState state, DWORD value) = 0;
	virtual HRESULT SetTextureStageState(DWORD sampler, TextureStageState state, DWORD value) = 0;
	virtual HRESULT SetTransform(TransformStateType state, const D3DMATRIX* pMatrix) = 0;
	virtual HRESULT SetMaterial(const D3DMATERIAL9* value) = 0;

	virtual IDirect3DDevice9* GetDevice() = 0;
	virtual const D3DCAPS9& GetCaps() const = 0;
	virtual bool IsFrameLatencyOk() const = 0;

	virtual DisplayMode GetScreenMode() const = 0;
	virtual DisplayMode GetDisplayMode() const = 0;
	virtual const DisplayModes& GetDisplayModes() const = 0;

	virtual UINT GetAdapterCount() const = 0;
	virtual HRESULT GetAdapterIdentifier(UINT adapter, DWORD flags, D3DADAPTER_IDENTIFIER9* pIdentifier) = 0;

	//Временно, необходимо убрать
	float CalcAspect();
	bool FindNearMode(const lsl::Point& resolution, DisplayMode& mode, float aspect = 0) const;
};

}

}

#endif