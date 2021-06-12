#include "stdafx.h"

#include "graph\\driver\D3D9RenderDriver.h"
#include "d3d9.h"
#include "r3dExceptions.h"

namespace r3d
{

namespace graph
{

namespace d3d9
{

D3DPOOL MemoryPoolToD3D9[MEMORY_POOL_END] = {D3DPOOL_DEFAULT, D3DPOOL_MANAGED, D3DPOOL_SYSTEMMEM, D3DPOOL_SCRATCH};

D3DRENDERSTATETYPE RenderStateToD3D9[RENDER_STATE_END] = {D3DRS_ZENABLE, D3DRS_FILLMODE, D3DRS_SHADEMODE,D3DRS_ZWRITEENABLE, D3DRS_ALPHATESTENABLE, D3DRS_LASTPIXEL, D3DRS_SRCBLEND, D3DRS_DESTBLEND, D3DRS_CULLMODE, D3DRS_ZFUNC, D3DRS_ALPHAREF, D3DRS_ALPHAFUNC, D3DRS_DITHERENABLE, D3DRS_ALPHABLENDENABLE, D3DRS_FOGENABLE, D3DRS_SPECULARENABLE, D3DRS_FOGCOLOR, D3DRS_FOGTABLEMODE, D3DRS_FOGSTART, D3DRS_FOGEND, D3DRS_FOGDENSITY, D3DRS_RANGEFOGENABLE, D3DRS_STENCILENABLE, D3DRS_STENCILFAIL, D3DRS_STENCILZFAIL, D3DRS_STENCILPASS, D3DRS_STENCILFUNC,D3DRS_STENCILREF, D3DRS_STENCILMASK, D3DRS_STENCILWRITEMASK, D3DRS_TEXTUREFACTOR, D3DRS_WRAP0, D3DRS_WRAP1, D3DRS_WRAP2, D3DRS_WRAP3, D3DRS_WRAP4, D3DRS_WRAP5, D3DRS_WRAP6, D3DRS_WRAP7, D3DRS_CLIPPING, D3DRS_LIGHTING, D3DRS_AMBIENT, D3DRS_FOGVERTEXMODE, D3DRS_COLORVERTEX, D3DRS_LOCALVIEWER, D3DRS_NORMALIZENORMALS, D3DRS_DIFFUSEMATERIALSOURCE, D3DRS_SPECULARMATERIALSOURCE, D3DRS_AMBIENTMATERIALSOURCE, D3DRS_EMISSIVEMATERIALSOURCE, D3DRS_VERTEXBLEND, D3DRS_CLIPPLANEENABLE, D3DRS_POINTSIZE, D3DRS_POINTSIZE_MIN, D3DRS_POINTSPRITEENABLE, D3DRS_POINTSCALEENABLE, D3DRS_POINTSCALE_A, D3DRS_POINTSCALE_B, D3DRS_POINTSCALE_C, D3DRS_MULTISAMPLEANTIALIAS, D3DRS_MULTISAMPLEMASK, D3DRS_PATCHEDGESTYLE, D3DRS_DEBUGMONITORTOKEN, D3DRS_POINTSIZE_MAX, D3DRS_INDEXEDVERTEXBLENDENABLE, D3DRS_COLORWRITEENABLE, D3DRS_TWEENFACTOR, D3DRS_BLENDOP, D3DRS_POSITIONDEGREE, D3DRS_NORMALDEGREE, D3DRS_SCISSORTESTENABLE, D3DRS_SLOPESCALEDEPTHBIAS, D3DRS_ANTIALIASEDLINEENABLE, D3DRS_MINTESSELLATIONLEVEL, D3DRS_MAXTESSELLATIONLEVEL, D3DRS_ADAPTIVETESS_X, D3DRS_ADAPTIVETESS_Y, D3DRS_ADAPTIVETESS_Z, D3DRS_ADAPTIVETESS_W, D3DRS_ENABLEADAPTIVETESSELLATION, D3DRS_TWOSIDEDSTENCILMODE, D3DRS_CCW_STENCILFAIL, D3DRS_CCW_STENCILZFAIL, D3DRS_CCW_STENCILPASS, D3DRS_CCW_STENCILFUNC, D3DRS_COLORWRITEENABLE1, D3DRS_COLORWRITEENABLE2, D3DRS_COLORWRITEENABLE3, D3DRS_BLENDFACTOR, D3DRS_SRGBWRITEENABLE, D3DRS_DEPTHBIAS, D3DRS_WRAP8, D3DRS_WRAP9, D3DRS_WRAP10, D3DRS_WRAP11, D3DRS_WRAP12, D3DRS_WRAP13, D3DRS_WRAP14, D3DRS_WRAP15, D3DRS_SEPARATEALPHABLENDENABLE, D3DRS_SRCBLENDALPHA, D3DRS_DESTBLENDALPHA, D3DRS_BLENDOPALPHA};

D3DSAMPLERSTATETYPE SamplerStateToD3D9[SAMPLER_STATE_END] = {D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_ADDRESSW, D3DSAMP_BORDERCOLOR, D3DSAMP_MAGFILTER, D3DSAMP_MINFILTER, D3DSAMP_MIPFILTER, D3DSAMP_MIPMAPLODBIAS, D3DSAMP_MAXMIPLEVEL, D3DSAMP_MAXANISOTROPY, D3DSAMP_SRGBTEXTURE, D3DSAMP_ELEMENTINDEX, D3DSAMP_DMAPOFFSET};

D3DTEXTURESTAGESTATETYPE TextureStageStateToD3D9[TEXTURE_STAGE_STATE_END] = {D3DTSS_COLOROP, D3DTSS_COLORARG1, D3DTSS_COLORARG2, D3DTSS_ALPHAOP, D3DTSS_ALPHAARG1, D3DTSS_ALPHAARG2, D3DTSS_BUMPENVMAT00, D3DTSS_BUMPENVMAT01, D3DTSS_BUMPENVMAT10, D3DTSS_BUMPENVMAT11, D3DTSS_TEXCOORDINDEX, D3DTSS_BUMPENVLSCALE, D3DTSS_BUMPENVLOFFSET, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTSS_COLORARG0, D3DTSS_ALPHAARG0, D3DTSS_RESULTARG, D3DTSS_CONSTANT};

D3DTRANSFORMSTATETYPE TransformStateTypeToD3D9[TRANSFORM_STATE_TYPE_END] = {D3DTS_WORLD, D3DTS_VIEW, D3DTS_PROJECTION, D3DTS_TEXTURE0, D3DTS_TEXTURE1, D3DTS_TEXTURE2, D3DTS_TEXTURE3, D3DTS_TEXTURE4, D3DTS_TEXTURE5, D3DTS_TEXTURE6, D3DTS_TEXTURE7};




D3D9RenderDriver::D3D9RenderDriver(D3DPRESENT_PARAMETERS& d3dpp): _d3d9(0), _d3dDevice9(0), _d3d9Ex(NULL), _d3dDevice9Ex(NULL), _frameLatencyOk(false)
{
	Init(d3dpp, false);
}

D3D9RenderDriver::~D3D9RenderDriver()
{
	_d3dDevice9->Release();
	_d3d9->Release();
}

void D3D9RenderDriver::Init(D3DPRESENT_PARAMETERS& d3dpp, bool d3dEx)
{
	HRESULT hr = d3dEx ? Direct3DCreate9Ex(D3D_SDK_VERSION, &_d3d9Ex) : D3D_OK;

	if (hr == D3D_OK && _d3d9Ex)
	{
		_d3d9 = _d3d9Ex;
	}
	else
	{
		if (d3dEx)
		{
			LSL_LOG("d3d9Ex init failed");

			d3dEx = false;
			_d3d9Ex = NULL;
		}
		
		_d3d9 = Direct3DCreate9(D3D_SDK_VERSION);

		if (_d3d9 == 0)
		{
			LSL_LOG("_d3d9 init failed");

			throw EInitD3D9Failed();
		}
	}	

	hr = _d3d9->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &_d3dCaps9);

	if (hr != D3D_OK)
	{
		LSL_LOG(lsl::StrFmt("_d3d9->GetDeviceCaps hr=%d", hr));
		throw EGetD3DCaps9Failed(hr);
	}

	DWORD typeVP;
	if (_d3dCaps9.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
		typeVP = D3DCREATE_HARDWARE_VERTEXPROCESSING;
	else
		typeVP = D3DCREATE_SOFTWARE_VERTEXPROCESSING;

	_screenMode = GetDisplayMode();

	DisplayMode minMode;
	minMode.width = 0;
	minMode.height = 0;

	DisplayMode maxMode;
	maxMode.width = MAXUINT;
	maxMode.height = MAXUINT;

	for (unsigned i = 0; i < _d3d9->GetAdapterModeCount(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8); ++i)
	{
		D3DDISPLAYMODE d3dMode;		
		_d3d9->EnumAdapterModes(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8, i, &d3dMode);

		DisplayMode mode;
		mode.format = d3dMode.Format;
		mode.height = d3dMode.Height;
		mode.width = d3dMode.Width;
		mode.refreshRate = d3dMode.RefreshRate;

		if (mode.width < cMinResWidth || mode.height < cMinResHeight)
		{
			if (mode.width <= cMaxResWidth && mode.width >= minMode.width && mode.height <= cMaxResHeight && mode.height >= minMode.height)
				minMode = mode;
		}
		else if (mode.width > cMaxResWidth || mode.height > cMaxResHeight)
		{
			if (mode.width >= cMinResWidth && mode.width <= maxMode.width && mode.height >= cMinResHeight && mode.height <= maxMode.height)
				maxMode = mode;
		}
		else
		{
			_displayModes.push_back(mode);
		}
	}

	if (_displayModes.size() == 0)
	{
		if (minMode.width != 0 && minMode.height != 0)
			_displayModes.push_back(minMode);
		if (maxMode.width != MAXUINT && maxMode.height != MAXUINT)
			_displayModes.push_back(maxMode);
	}

	unsigned refreshRate = FindPrefRate();

	if (refreshRate != 0)
	{
		for (DisplayModes::const_iterator iter = _displayModes.begin(); iter != _displayModes.end();)
		{
			if (iter->refreshRate != refreshRate)
				iter = _displayModes.erase(iter);
			else
				++iter;
		}
	}

	DisplayMode dispMode;

	if (d3dpp.BackBufferWidth == 0 && d3dpp.BackBufferHeight == 0 && FindNearMode(lsl::Point(cOptimalResWidth, cOptimalResHeight), dispMode, _screenMode.height != 0 ? _screenMode.width/(float)_screenMode.height : 0.0f))
	{
		d3dpp.BackBufferWidth = dispMode.width;
		d3dpp.BackBufferHeight = dispMode.height;
	}
	else if (!d3dpp.Windowed && FindNearMode(lsl::Point(d3dpp.BackBufferWidth, d3dpp.BackBufferHeight), dispMode))
	{
		d3dpp.BackBufferWidth = dispMode.width;
		d3dpp.BackBufferHeight = dispMode.height;
	}
	else if (!d3dpp.Windowed)
	{
		d3dpp.BackBufferWidth = _screenMode.width;
		d3dpp.BackBufferHeight = _screenMode.height;
	}

	if (!d3dpp.Windowed)
		d3dpp.FullScreen_RefreshRateInHz = refreshRate;
	else
		d3dpp.FullScreen_RefreshRateInHz = 0;

	hr = d3dEx ? _d3d9Ex->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dpp.hDeviceWindow, typeVP, &d3dpp, NULL, &_d3dDevice9Ex) : D3D_OK;

	if (hr == D3D_OK && _d3dDevice9Ex)
	{
		_d3dDevice9 = _d3dDevice9Ex;
		_frameLatencyOk = _d3dDevice9Ex->SetMaximumFrameLatency(2) == D3D_OK;
	}
	else
	{
		if (d3dEx)
		{
			LSL_LOG("d3dDevice9Ex init failed");

			_d3dDevice9Ex = NULL;
			lsl::SafeRelease(_d3d9Ex);
			_d3d9 = NULL;
			Init(d3dpp, false);
			return;
		}

		hr = _d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dpp.hDeviceWindow, typeVP, &d3dpp, &_d3dDevice9);

		if (hr != D3D_OK || _d3dDevice9 == NULL)
		{
			LSL_LOG(lsl::StrFmt("_d3d9->CreateDevice hr=%d", hr));

			throw ECreateD3DDevice9Failed(hr);
		}
	}
}

unsigned D3D9RenderDriver::FindPrefRate()
{
	const unsigned prefHZ = 60;
	unsigned minHz = 0;
	
	for (DisplayModes::const_iterator iter = _displayModes.begin(); iter != _displayModes.end(); ++iter)
		if (iter->refreshRate != 0 && ((iter->refreshRate >= prefHZ && minHz > iter->refreshRate) || (iter->refreshRate <= prefHZ && minHz < iter->refreshRate) || minHz == 0))
			minHz = iter->refreshRate;

	LSL_LOG(lsl::StrFmt("pref rate %d", minHz));

	return minHz;
}

HRESULT D3D9RenderDriver::Present(const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion)
{
	return _d3dDevice9->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

HRESULT D3D9RenderDriver::TestCooperativeLevel()
{
	return _d3dDevice9->TestCooperativeLevel();
}

HRESULT D3D9RenderDriver::Reset(D3DPRESENT_PARAMETERS& d3dpp)
{
	if (!d3dpp.Windowed)
		d3dpp.FullScreen_RefreshRateInHz = FindPrefRate();
	else
		d3dpp.FullScreen_RefreshRateInHz = 0;

	return _d3dDevice9->Reset(&d3dpp);
}

HRESULT D3D9RenderDriver::CheckDeviceFormat(UINT adapter, D3DDEVTYPE deviceType, D3DFORMAT adapterFormat, DWORD usage, D3DRESOURCETYPE rType, D3DFORMAT checkFormat)
{
	return _d3d9->CheckDeviceFormat(adapter, deviceType, adapterFormat, usage, rType, checkFormat);
}

HRESULT D3D9RenderDriver::CheckDeviceMultiSampleType(UINT adapter, D3DDEVTYPE deviceType, D3DFORMAT surfaceFormat, BOOL windowed, D3DMULTISAMPLE_TYPE multiSampleType, DWORD *pQualityLevels)
{
	return _d3d9->CheckDeviceMultiSampleType(adapter, deviceType, surfaceFormat, windowed, multiSampleType, pQualityLevels);
}

HRESULT D3D9RenderDriver::SetRenderState(RenderState state, DWORD value)
{
	return _d3dDevice9->SetRenderState(RenderStateToD3D9[state], value);
}

HRESULT D3D9RenderDriver::SetTexture(DWORD sampler, IDirect3DBaseTexture9* value)
{
	return _d3dDevice9->SetTexture(sampler, value);
}

HRESULT D3D9RenderDriver::SetSamplerState(DWORD sampler, SamplerState state, DWORD value)
{
	return _d3dDevice9->SetSamplerState(sampler, SamplerStateToD3D9[state], value);
}

HRESULT D3D9RenderDriver::SetTextureStageState(DWORD sampler, TextureStageState state, DWORD value)
{
	return _d3dDevice9->SetTextureStageState(sampler, TextureStageStateToD3D9[state], value);
}

HRESULT D3D9RenderDriver::SetTransform(TransformStateType state, const D3DMATRIX* pMatrix)
{
	return _d3dDevice9->SetTransform(TransformStateTypeToD3D9[state], pMatrix);
}

HRESULT D3D9RenderDriver::SetMaterial(const D3DMATERIAL9* value)
{
	return _d3dDevice9->SetMaterial(value);
}

IDirect3DDevice9* D3D9RenderDriver::GetDevice()
{
	return _d3dDevice9;
}

const D3DCAPS9& D3D9RenderDriver::GetCaps() const
{
	return _d3dCaps9;
}

bool D3D9RenderDriver::IsFrameLatencyOk() const
{
	return _frameLatencyOk;
}

DisplayMode D3D9RenderDriver::GetScreenMode() const
{
	return _screenMode;
}

DisplayMode D3D9RenderDriver::GetDisplayMode() const
{
	D3DDISPLAYMODE d3dDispMode;
	_d3d9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3dDispMode);

	DisplayMode mode;
	mode.format = d3dDispMode.Format;
	mode.width = d3dDispMode.Width;
	mode.height = d3dDispMode.Height;
	mode.refreshRate = d3dDispMode.RefreshRate;

	if (mode.refreshRate == 0)
		mode.refreshRate = 60;

	//if (mode.refreshRate == 0)
	//{
		//HMONITOR monitor = _d3d9->GetAdapterMonitor(D3DADAPTER_DEFAULT);
		//lsl::appLog.Append("ERROR: Display refresh rate == 0");
		//MONITORINFO info;
		//GetMonitorInfo(monitor, &info);
		//MONITORINFO
		//info.
	//}
	//else
	//	mode.refreshRate = d3dDispMode.RefreshRate;

	return mode;
}

const DisplayModes& D3D9RenderDriver::GetDisplayModes() const
{
	return _displayModes;
}

UINT D3D9RenderDriver::GetAdapterCount() const
{
	return _d3d9->GetAdapterCount();
}

HRESULT D3D9RenderDriver::GetAdapterIdentifier(UINT adapter, DWORD flags, D3DADAPTER_IDENTIFIER9* pIdentifier)
{
	return _d3d9->GetAdapterIdentifier(adapter, flags, pIdentifier);
}

}

}

}