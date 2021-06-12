#include "stdafx.h"

#include "graph\\Engine.h"

#include "graph\\driver\D3D9RenderDriver.h"
#include "lslUtility.h"

//#define OCCLUSION_QUERIES

namespace r3d
{

namespace graph
{

bool Engine::_d3dxUse = true;




Engine::Engine(HWND window, lsl::Point resolution, bool fullScreen, unsigned multisampling): _reset(true), _lost(false), _restart(true), _beginScene(false), _beginDraw(false), _dt(0), _pause(false), _filtering(0), _multisampling(0), _d3dQueryEvent(NULL)
{
	ZeroMemory(&_d3dpp, sizeof(_d3dpp));	
	_d3dpp.BackBufferFormat           = D3DFMT_A8R8G8B8; 
	_d3dpp.BackBufferCount            = 1;
	_d3dpp.SwapEffect                 = D3DSWAPEFFECT_DISCARD;
	_d3dpp.EnableAutoDepthStencil     = true;
	_d3dpp.AutoDepthStencilFormat     = D3DFMT_D24X8;
	_d3dpp.Flags                      = 0;
	_d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
	//_d3dpp.PresentationInterval       = D3DPRESENT_INTERVAL_IMMEDIATE;
	_d3dpp.PresentationInterval       = D3DPRESENT_INTERVAL_ONE;

	SetParams(window, resolution, fullScreen, multisampling);

	_driver = new d3d9::D3D9RenderDriver(_d3dpp);
	_driver->GetDevice()->GetSwapChain(0, &_swapChain);
	_driver->GetDevice()->GetDepthStencilSurface(&_dsSurf);	

	CreateQueries();

	_context = new ContextInfo(_driver);	
	_context->SetDefaults();

	InitResources();

#ifdef _DEBUG
	HDC hDC;
	//HFONT hFont;
	int nHeight;
	int nPointSize = 9;
	//char strFontName[] = "Arial";
	hDC = GetDC(NULL);
	nHeight = -( MulDiv( nPointSize, GetDeviceCaps(hDC, LOGPIXELSY), 72 ) );
	ReleaseDC( NULL, hDC );
	// Create a font for statistics and help output
	HRESULT hr = D3DXCreateFont(_driver->GetDevice(), nHeight, 0, FW_BOLD, 0,
		FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("Arial"), &g_pd3dxFont);
	if (hr != D3D_OK)
		MessageBox(NULL,"Call to D3DXCreateFont failed!", "ERROR", MB_OK | MB_ICONEXCLAMATION);
#endif
}

Engine::~Engine()
{
	FreeResources();

	LSL_ASSERT(_videoResList.empty());

	lsl::SafeRelease(_swapChain);
	lsl::SafeRelease(_dsSurf);

	ReleaseQueries();

	delete _context;
	delete _driver;

#ifdef _DEBUG
	g_pd3dxFont->Release();
#endif
}

#ifdef _DEBUG

float CalcDeltaTime(__int64& gTime, __int64& gLastTime)
{
	__int64 freq;
	QueryPerformanceCounter((LARGE_INTEGER*)&gTime);  // Get current count
	QueryPerformanceFrequency((LARGE_INTEGER*)&freq); // Get processor freq
	float deltaTime = static_cast<float>((gTime - gLastTime)/static_cast<double>(freq));
	gLastTime = gTime;
	//Для слишком медленного ФПС фикс.
	if (deltaTime > 1.0f)
		deltaTime = 1;
	//Для слишком быстрого ФПС тоже фикс, поскольку нулевое значение стопорит прогресс
	if (deltaTime == 0)
		deltaTime = 1.0f/10000.0f;

	return deltaTime;
}

void Engine::DrawFPS()
{
	static __int64 gTime, gLastTime;
	float deltaTime = CalcDeltaTime(gTime, gLastTime);


	static double nTimeOfLastFPSUpdate = 0.0;
	static int nFrameCount = 0;
	static char fpsString[255] = "Frames Per Second = ";	
	if (nTimeOfLastFPSUpdate > 1.0f) // Update once a second
	{		
		sprintf_s(fpsString, "FPS - %4.2f \n 'C' - переключить камеру \n"
			"FixedFPS - %i \n", nFrameCount/nTimeOfLastFPSUpdate, (static_cast<int>(1.0f/_dt) / 10) * 10);
		nTimeOfLastFPSUpdate = 0;
		nFrameCount = 0;
	}
	nTimeOfLastFPSUpdate += deltaTime;
	++nFrameCount;
	RECT destRect;
	SetRect(&destRect, 505, 5, 0, 0);
	g_pd3dxFont->DrawText(0 , fpsString, -1, &destRect, DT_NOCLIP, D3DXCOLOR(1.0f, 0.0f, 0.0f, 1.0f));	
}

#endif

void Engine::CreateQueries()
{
	if (_driver->IsFrameLatencyOk())
	{
		LSL_LOG("Ignore CreateQueries");
		return;
	}

	LSL_LOG("CreateQueries");

#ifdef OCCLUSION_QUERIES
	ZeroMemory(_d3dQueryBuf, sizeof(_d3dQueryBuf));
	for (int i = 0; i < 2; ++i)
	{
		HRESULT hr = _driver->GetDevice()->CreateQuery(D3DQUERYTYPE_OCCLUSION, &_d3dQueryBuf[i]);
		if (hr != D3D_OK)
		{
			_d3dQueryBuf[i] = NULL;
			break;
		}
	}
#else
	if (_driver->GetDevice()->CreateQuery(D3DQUERYTYPE_EVENT, &_d3dQueryEvent) != D3D_OK)
		_d3dQueryEvent = NULL;
#endif
}

void Engine::ReleaseQueries()
{
#ifdef OCCLUSION_QUERIES
	for (int i = 0; i < 2; ++i)
		if (_d3dQueryBuf)
			lsl::SafeRelease(_d3dQueryBuf[i]);
	ZeroMemory(_d3dQueryBuf, sizeof(_d3dQueryBuf));
#else
	if (_d3dQueryEvent)
		lsl::SafeRelease(_d3dQueryEvent);
#endif
}

void Engine::UpdateScreenQuad()
{
	D3DXVECTOR4 quadVert = D3DXVECTOR4(0.0f, 0.0f, 1.0f, 1.0f);

	float fLeftU = 0.0f;
	float fTopV = 0.0f;
	float fRightU = 1.0f;
	float fBottomV = 1.0f;

	//Закоментированные смещения вносят искажения при совмещении текстур
	float fPosX = quadVert.x * _d3dpp.BackBufferWidth - 0.5f;
	float fPosY = quadVert.y * _d3dpp.BackBufferHeight - 0.5f;
	float fWidth5 = _d3dpp.BackBufferWidth * quadVert.z - 0.5f;
	float fHeight5 = _d3dpp.BackBufferHeight * quadVert.w - 0.5f;

	res::ScreenVertex vertBuf[4] = 
	{
		res::ScreenVertex(D3DXVECTOR4(fPosX, fPosY, 0.5f, 1.0f), D3DXVECTOR2(fLeftU, fTopV)),
		res::ScreenVertex(D3DXVECTOR4(fWidth5, fPosY, 0.5f, 1.0f), D3DXVECTOR2(fRightU, fTopV)),
		res::ScreenVertex(D3DXVECTOR4(fPosX, fHeight5, 0.5f, 1.0f), D3DXVECTOR2(fLeftU, fBottomV)),
		res::ScreenVertex(D3DXVECTOR4(fWidth5, fHeight5, 0.5f, 1.0f), D3DXVECTOR2(fRightU, fBottomV))
	};

	res::VertexData* data = _meshScreenQuad.GetOrCreateData();
	data->SetVertexCount(4);
	data->SetScreenRHW(true);
	data->SetFormat(res::VertexData::vtPos4);
	data->SetFormat(res::VertexData::vtTex0);
	data->CopyDataFrom(vertBuf);

	_meshScreenQuad.primitiveType = D3DPT_TRIANGLESTRIP;
	_meshScreenQuad.Init(*this);
}

void Engine::InitResources()
{
	{
		res::VertexPT vertBuf[4] = 
		{
			res::VertexPT(D3DXVECTOR3(0.5f, -0.5f, 0.0f), D3DXVECTOR2(1, 1)),
			res::VertexPT(D3DXVECTOR3(0.5f, 0.5f, 0.0f), D3DXVECTOR2(1, 0)),
			res::VertexPT(D3DXVECTOR3(-0.5f, -0.5f, 0.0f), D3DXVECTOR2(0, 1)),
			res::VertexPT(D3DXVECTOR3(-0.5f, 0.5f, 0.0f), D3DXVECTOR2(0, 0))
		};

		res::VertexData* data = _meshPlanePT.GetOrCreateData();
		data->SetVertexCount(4);
		data->SetFormat(res::VertexData::vtPos3);
		data->SetFormat(res::VertexData::vtTex0);
		data->CopyDataFrom(vertBuf);

		_meshPlanePT.primitiveType = D3DPT_TRIANGLESTRIP;
		_meshPlanePT.Init(*this);
	}

	UpdateScreenQuad();
}

void Engine::FreeResources()
{
	_meshPlanePT.Free();
	_meshScreenQuad.Free();
}

bool Engine::ResetDevice()
{
	if (!_reset)
	{
		HRESULT hr = _driver->TestCooperativeLevel();

		if (hr == D3D_OK || hr == D3DERR_DEVICENOTRESET)
		{
			if (_driver->Reset(_d3dpp) != D3D_OK)
			{
				LSL_LOG(lsl::StrFmt("driver->Reset failed hr=%d", hr));
				return false;
			}

			_context->SetDefaults();
			_driver->GetDevice()->GetSwapChain(0, &_swapChain);
			_driver->GetDevice()->GetDepthStencilSurface(&_dsSurf);			

			CreateQueries();
			
			for (_VideoResList::iterator iter = _videoResList.begin(); iter != _videoResList.end(); ++iter)
				(*iter)->OnResetDevice();

#ifdef _DEBUG
			g_pd3dxFont->OnResetDevice();
#endif

			UpdateScreenQuad();
						
			_reset = true;
			_restart = true;
		}
	}

	return _reset;
}

void Engine::LostDevice()
{
	if (_reset)
	{
		_reset = false;

#ifdef _DEBUG
		g_pd3dxFont->OnLostDevice();
#endif

		for (_VideoResList::iterator iter = _videoResList.begin(); iter != _videoResList.end(); ++iter)
			(*iter)->OnLostDevice();
		
		lsl::SafeRelease(_swapChain);
		lsl::SafeRelease(_dsSurf);

		ReleaseQueries();
	}	
}

void Engine::SetParams(HWND window, lsl::Point resolution, bool fullScreen, unsigned multisampling)
{
	_multisampling = multisampling;

	_d3dpp.hDeviceWindow     = window;
	_d3dpp.BackBufferWidth   = resolution.x;
	_d3dpp.BackBufferHeight  = resolution.y;
	_d3dpp.Windowed          = !fullScreen;
	
	D3DMULTISAMPLE_TYPE type;
	unsigned quality;
	ToMultisampling(multisampling, type, quality);
	_d3dpp.MultiSampleType = type;
	_d3dpp.MultiSampleQuality = quality;
}

void Engine::InsertVideoRes(IVideoResource* value)
{
	_videoResList.push_back(value);
}

void Engine::RemoveVideoRes(IVideoResource* value)
{
	_videoResList.remove(value);
}

bool Engine::CheckReset()
{
	if (_lost)
	{
		LostDevice();
		_lost = false;
	}
	return ResetDevice();
}

bool Engine::Reset(HWND window, lsl::Point resolution, bool fullScreen, unsigned multisampling, bool resetDevice)
{
	LostDevice();
	SetParams(window, resolution, fullScreen, multisampling);

	if (resetDevice)
		return ResetDevice();
	else
		return false;
}

bool Engine::IsReset() const
{
	//Не забываем что Render может вызываться в отдельном потоке, поэтому переключение с _lost = true на _reset = false могло пока и не произойти
	return (_reset && !_lost);
}

bool Engine::IsRestart() const
{
	return _restart;
}

void Engine::Restart()
{
	_restart = true;
}

void Engine::ProgressTime(float dt, bool pause)
{
	_dt = dt;
	_pause = pause;
}

bool Engine::BeginScene()
{
	LSL_ASSERT(!_beginScene);

	_beginScene = true;

	if (!IsReset())
		return false;

	if (_driver->GetDevice()->BeginScene() != D3D_OK)
	{
		_driver->GetDevice()->EndScene();
		_lost = true;
		return false;
	}

	return true;	
}

void precision_sleep(double timeout)
{
	double startTime = lsl::GetTimeDbl();
	while (lsl::GetTimeDbl() - startTime < timeout)
	{
		//for (int i = 0; i < 500; ++i)
		//	YieldProcessor();
	}
}

bool Engine::EndScene()
{
	LSL_ASSERT(_beginScene);

	_beginScene = false;
	_restart = false;

	if (IsReset())
	{
#ifdef _DEBUG
		DrawFPS();
#endif

		HRESULT hr = _driver->GetDevice()->EndScene();
		if (hr != D3D_OK)
			throw lsl::Error("_driver->GetDevice()->EndScene()");
		return true;
	}

	return false;
}

void Engine::GPUSync()
{
#ifdef OCCLUSION_QUERIES
	if (IsReset() && IsSyncSupported())
	{
		_d3dQueryBuf[0]->Issue(D3DISSUE_END);

		// Force the driver to execute the commands from the command buffer.
		// Empty the command buffer and wait until the GPU is idle.
		unsigned numberOfPixelsDrawn;
		while(S_FALSE == _d3dQueryBuf[1]->GetData( &numberOfPixelsDrawn, 
			sizeof(DWORD), D3DGETDATA_FLUSH ))
		{}
	}	
#else
	if (IsReset() && _d3dQueryEvent)
	{	
		while (_d3dQueryEvent->GetData(NULL, 0, D3DGETDATA_FLUSH) == S_FALSE);

		//переводим событие в состояние ресурса (issued state), как только буффер команд станет пустым событие самой перейдет в сигнальное состояние, это может произойти во время renderTime (время цпу), в результате даже если буффер снова заполнится то мы не будем ждать на цикле выше. Т.е. мы всегда имеем запас в кадр. Однако на экране будет лаг в один кадр.
		_d3dQueryEvent->Issue(D3DISSUE_END);
	}	
#endif
}

bool Engine::IsSyncSupported()
{
#ifdef OCCLUSION_QUERIES
	return _d3dQueryBuf[0] && _d3dQueryBuf[1];
#else
	return _d3dQueryEvent ? true : false;
#endif
}

bool Engine::Present()
{
	if (!_lost && _driver->GetDevice()->Present(0, 0, 0, 0) != D3D_OK)
	{
		_lost = true;
		return false;
	}

	return true;
}

void Engine::BeginDraw()
{
	LSL_ASSERT(!_beginDraw);

	_beginDraw = true;

	if (!_context->GetShaderStack().empty())
		_context->GetShaderStack().front()->BeginDraw(*this);

	_context->BeginDraw();
}

bool Engine::EndDraw(bool nextPass)
{
	LSL_ASSERT(_beginDraw);

	_beginDraw = false;

	_context->EndDraw(nextPass);

	bool res = true;
	if (!_context->GetShaderStack().empty())
		res = _context->GetShaderStack().front()->EndDraw(*this, nextPass);

	return res;
}

void Engine::BeginBackBufOut(DWORD clearFlags, D3DCOLOR color)
{
	_swapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &_backBuff);
	_driver->GetDevice()->SetRenderTarget(0, _backBuff);
	_driver->GetDevice()->SetDepthStencilSurface(_dsSurf);

	if (clearFlags > 0)
		_driver->GetDevice()->Clear(0, 0, clearFlags, color, 1.0f, 0);
}

void Engine::EndBackBufOut()
{
	_backBuff->Release();
}

IDirect3DSurface9* Engine::GetDSSurf()
{
	return _dsSurf;
}

void Engine::BeginMeshPT()
{
	_meshPlanePT.BeginStream();
}

void Engine::EndMeshPT()
{
	_meshPlanePT.EndStream();
}

void Engine::RenderPlanePT()
{
	do
	{
		BeginDraw();
		_meshPlanePT.Draw();
	}
	while (!EndDraw(true));	
}

void Engine::RenderSpritePT(const D3DXVECTOR3& pos, const D3DXVECTOR3& scale, float turnAngle, const D3DXVECTOR3* fixDirection, const D3DXMATRIX& localMat)
{
	const CameraCI& camera = GetContext().GetCamera();

	D3DXMATRIX rotMat;
	//Направленный спрайт
	if (fixDirection)
	{
		D3DXVECTOR3 xVec = *fixDirection;

		//Видовой вектор
		D3DXVECTOR3 viewVec;
		switch (GetContext().GetCamera().GetDesc().style)
		{
		case csPerspective:
			viewVec = pos - camera.GetDesc().pos; 
			D3DXVec3Normalize(&viewVec, &viewVec);
			break;

		case csOrtho:
			viewVec = GetContext().GetCamera().GetDesc().dir;
			break;
		}

		//
		D3DXVECTOR3 yVec;
		D3DXVec3Cross(&yVec, &xVec, &viewVec);
		D3DXVec3Normalize(&yVec, &yVec);
		//
		D3DXVECTOR3 zVec;
		D3DXVec3Cross(&zVec, &xVec, &yVec);

		MatrixRotationFromAxis(xVec, yVec, zVec, rotMat);
	}
	//Обычный спрайт
	else
	{
		rotMat = camera.GetInvView();
		rotMat._41 = rotMat._42 = rotMat._43 = 0.0f;

		//Локальный поворот спрайта (только для не направленных)
		D3DXMATRIX rotZ;
		D3DXMatrixRotationAxis(&rotZ, &camera.GetDesc().dir, turnAngle);
		rotMat *= rotZ;
	}

	//Результирующая матрица
	D3DXMATRIX worldMat = localMat;
	MatrixScale(scale, worldMat);
	worldMat *= rotMat;
	MatrixSetTranslation(pos, worldMat);

	GetContext().SetWorldMat(worldMat);
	
	RenderPlanePT();
}

void Engine::RenderScreenQuad(bool disableZBuf)
{
	if (!disableZBuf)
	{
		GetContext().SetRenderState(rsZEnable, false);
		GetContext().SetRenderState(rsZWriteEnable, false);
	}

	GetContext().SetRenderState(rsCullMode, D3DCULL_CCW);

	do
	{
		BeginDraw();
		_meshScreenQuad.Draw();
	}
	while (!EndDraw(true));
	
	GetContext().RestoreRenderState(rsCullMode);

	if (!disableZBuf)
	{
		GetContext().RestoreRenderState(rsZWriteEnable);
		GetContext().RestoreRenderState(rsZEnable);
	}
}

float Engine::GetDt(bool ignorePause)
{
	return (!_pause || ignorePause) ? _dt : 0.0f;
}

unsigned Engine::GetFiltering() const
{
	return _filtering;
}

void Engine::SetFiltering(unsigned value)
{
	_filtering = value;
}

unsigned Engine::GetMultisampling() const
{
	return _multisampling;
}

bool Engine::d3dxUse()
{
	return _d3dxUse;
}

void Engine::d3dxUse(bool value)
{
	_d3dxUse = value;
}

RenderDriver& Engine::GetDriver() const
{
	return *_driver;
}

const D3DPRESENT_PARAMETERS& Engine::GetParams() const
{
	return _d3dpp;
}

ContextInfo& Engine::GetContext()
{
	return *_context;
}

const ContextInfo& Engine::GetContext() const
{
	return *_context;
}

void Engine::ToMultisampling(unsigned level, D3DMULTISAMPLE_TYPE& type, unsigned& quality)
{
	quality = 0;
	type = D3DMULTISAMPLE_NONE;

	switch (level)
	{
	case 0:
		type = D3DMULTISAMPLE_NONE;
		break;
	case 1:
		type = D3DMULTISAMPLE_NONMASKABLE;
		break;
	case 2:
		type = D3DMULTISAMPLE_2_SAMPLES;
		break;
	case 3:
		type = D3DMULTISAMPLE_3_SAMPLES;
		break;
	case 4:
		type = D3DMULTISAMPLE_4_SAMPLES;
		break;
	case 5:
		type = D3DMULTISAMPLE_5_SAMPLES;
		break;
	case 6:
		type = D3DMULTISAMPLE_6_SAMPLES;
		break;
	case 7:
		type = D3DMULTISAMPLE_7_SAMPLES;
		break;
	case 8:
		type = D3DMULTISAMPLE_8_SAMPLES;
		break;
	case 9:
		type = D3DMULTISAMPLE_9_SAMPLES;
		break;
	case 10:
		type = D3DMULTISAMPLE_10_SAMPLES;
		break;
	case 11:
		type = D3DMULTISAMPLE_11_SAMPLES;
		break;
	case 12:
		type = D3DMULTISAMPLE_12_SAMPLES;
		break;
	case 13:
		type = D3DMULTISAMPLE_13_SAMPLES;
		break;
	case 14:
		type = D3DMULTISAMPLE_14_SAMPLES;
		break;
	case 15:
		type = D3DMULTISAMPLE_15_SAMPLES;
		break;
	case 16:
		type = D3DMULTISAMPLE_16_SAMPLES;
		break;
	}
}




void RenderStateManager::Apply(Engine& engine)
{
	for (iterator iter = begin(); iter != end(); ++iter)
		engine.GetContext().SetRenderState(iter->first, iter->second);
}

void RenderStateManager::UnApply(Engine& engine)
{
	for (iterator iter = begin(); iter != end(); ++iter)
		engine.GetContext().RestoreRenderState(iter->first);
}




void SamplerStateManager::Apply(Engine& engine, DWORD stage)
{
	for (iterator iter = begin(); iter != end(); ++iter)
		engine.GetContext().SetSamplerState(stage, iter->first, iter->second);
}

void SamplerStateManager::UnApply(Engine& engine, DWORD stage)
{
	for (iterator iter = begin(); iter != end(); ++iter)
		engine.GetContext().RestoreSamplerState(stage, iter->first);
}




DWORD TextureStageStateManager::Get(TextureStageState state) const
{
	_States::const_iterator iter = _states.find(state);
	if (iter != _states.end())
		return iter->second;
	else
		return ContextInfo::GetDefTextureStageState(0, state);
}

void TextureStageStateManager::Set(TextureStageState state, DWORD value)
{
	_states[state] = value;
}

void TextureStageStateManager::Restore(TextureStageState state)
{
	_states.erase(state);
}

void TextureStageStateManager::Reset()
{
	_states.clear();
}

TextureStageStateManager::iterator TextureStageStateManager::begin()
{
	return _states.begin();
}

TextureStageStateManager::iterator TextureStageStateManager::end()
{
	return _states.end();
}

void TextureStageStateManager::Apply(Engine& engine, DWORD stage)
{
	for (iterator iter = begin(); iter != end(); ++iter)		
		engine.GetContext().SetTextureStageState(stage, iter->first, iter->second);
}

void TextureStageStateManager::UnApply(Engine& engine, DWORD stage)
{
	for (iterator iter = begin(); iter != end(); ++iter)
		engine.GetContext().RestoreTextureStageState(stage, iter->first);
}

}

}