#include "RenderDriver.h"

namespace r3d
{

namespace graph
{

namespace d3d9
{

class D3D9RenderDriver: public RenderDriver
{
private:
	IDirect3D9* _d3d9;	
	IDirect3DDevice9* _d3dDevice9;
	IDirect3D9Ex* _d3d9Ex;
	IDirect3DDevice9Ex* _d3dDevice9Ex;

	D3DCAPS9 _d3dCaps9;
	bool _frameLatencyOk;
	DisplayMode _screenMode;
	DisplayModes _displayModes;

	void Init(D3DPRESENT_PARAMETERS& d3dpp, bool d3dEx);
	unsigned FindPrefRate();
public:
	D3D9RenderDriver(D3DPRESENT_PARAMETERS& d3dpp);
	virtual ~D3D9RenderDriver();

	virtual HRESULT Present(const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion);
	virtual HRESULT TestCooperativeLevel();
	virtual HRESULT Reset(D3DPRESENT_PARAMETERS& d3dpp);
	virtual HRESULT CheckDeviceFormat(UINT adapter, D3DDEVTYPE deviceType, D3DFORMAT adapterFormat, DWORD usage, D3DRESOURCETYPE rType, D3DFORMAT checkFormat);
	virtual HRESULT CheckDeviceMultiSampleType(UINT adapter, D3DDEVTYPE deviceType, D3DFORMAT surfaceFormat, BOOL windowed, D3DMULTISAMPLE_TYPE multiSampleType, DWORD* pQualityLevels);

	virtual HRESULT SetRenderState(RenderState state, DWORD value);
	virtual HRESULT SetTexture(DWORD sampler, IDirect3DBaseTexture9* value);
	virtual HRESULT SetSamplerState(DWORD sampler, SamplerState state, DWORD value);
	virtual HRESULT SetTextureStageState(DWORD sampler, TextureStageState state, DWORD value);
	virtual HRESULT SetTransform(TransformStateType state, const D3DMATRIX* pMatrix);
	virtual HRESULT SetMaterial(const D3DMATERIAL9* value);

	virtual IDirect3DDevice9* GetDevice();
	virtual const D3DCAPS9& GetCaps() const;
	virtual bool IsFrameLatencyOk() const;

	virtual DisplayMode GetScreenMode() const;
	virtual DisplayMode GetDisplayMode() const;
	virtual const DisplayModes& GetDisplayModes() const;

	virtual UINT GetAdapterCount() const;
	virtual HRESULT GetAdapterIdentifier(UINT adapter, DWORD flags, D3DADAPTER_IDENTIFIER9* pIdentifier);

};

}

}

}