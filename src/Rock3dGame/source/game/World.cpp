#include "stdafx.h"

#include "game\World.h"
#include "edit\Edit.h"

#include "lslSerialFileXml.h"

//#define DEBUG_FRAME_SYNC 1
//#define DEBUG_NET 1
#define DRAW_DEBUG_INFO DEBUG_FRAME_SYNC | DEBUG_NET

namespace r3d
{

namespace game
{

const float World::cMaxSimStep = 1/60.0f;

#ifdef DRAW_DEBUG_INFO

namespace
{

gui::Label* _dbgInfo = NULL;

void AdjustDbgInfo(const D3DXVECTOR2& size)
{
	if (_dbgInfo == 0)
		return;

	_dbgInfo->SetPos(D3DXVECTOR2(size.x - 20, 250));
	_dbgInfo->SetSize(D3DXVECTOR2(200.0f, size.y));
}

}

#endif




World::World(): _terminateResult(EXIT_SUCCESS), _terminate(false), _pause(false), _firstTick(0), _lastTick(0), _lastTick2(0), _lastSyncTick(0), _timeAccum(0), _lastDrawTime(0), _curTimeIter(1), _control(0), _view(0), _graph(0), _audio(0), _videoPlayer(NULL), _pxManager(0), _pxScene(0), _resManager(0), _camera(0), _db(0), _map(0), _logic(0), _env(0), _edit(0), _game(0), _syncFreq(60), _time(0), _dTimeReal(0), _inputWasReset(false), _videoMode(false), _warmIterations(0), _timeResolution(0)
{
	ZeroMemory(_iterBuf, sizeof(_iterBuf));

	Profiler::Init(&_profiler);
}

World::~World()
{	
	//SetTimeResolution(0);
}

void World::Init(IView::Desc viewDesc)
{
	LSL_LOG("world init");

	QueryPerformanceFrequency((LARGE_INTEGER*)&_cpuFreq);
	SetName("world");

	if (viewDesc.resolution.x == 0 && viewDesc.resolution.y == 0)
	{
		try
		{
			lsl::SerialFileXML file;
			lsl::RootNode node("root", this);			

			file.LoadNodeFromFile(node, "user.xml");
		
			node.BeginLoad();

			lsl::SReadValue(&node, "resolution", viewDesc.resolution);
			lsl::SReadValue(&node, "fullScreen", viewDesc.fullscreen);

			node.EndLoad();

			View::SetWindowSize(viewDesc.handle, viewDesc.resolution, viewDesc.fullscreen);
		}
		catch (const lsl::EUnableToOpen&) 
		{	
		}
	}

	_control = new ControlManager(this);

	LSL_LOG("graph init");

	_graph = new GraphManager(viewDesc.handle, viewDesc.resolution, viewDesc.fullscreen);
	_graph->SetName("graph");
	_graph->SetOwner(this);
	_graph->GetEngine().InsertVideoRes(this);	

	LSL_LOG("view init");

	viewDesc.resolution = lsl::Point(_graph->GetEngine().GetParams().BackBufferWidth, _graph->GetEngine().GetParams().BackBufferHeight);
	_syncFreq = _graph->GetDisplayMode().refreshRate;
	_view = new View(this, viewDesc);

	LSL_LOG("audio init");

	_audio = new snd::Engine();
	_audio->SetName("audio");
	_audio->SetOwner(this);
	_audio->Init();

	LSL_LOG("video init");

	_videoPlayer = new video::Player(this);
	_videoPlayer->Initialize(viewDesc.handle);
	_videoPlayer->UpdateVideoWindow(viewDesc.resolution);

	LSL_LOG("px init");

	_pxManager = new px::Manager();
	_pxManager->SetName("px");
	_pxManager->SetOwner(this);
	_pxScene = _pxManager->AddScene();
	_pxScene->AddRef();

	LSL_LOG("resManager init");

	_resManager = new ResourceManager(this);
	_resManager->SetName("resManager");
	_resManager->SetOwner(this);

	LSL_LOG("camera init");

	_camera = new CameraManager(this);
	_camera->SetAspect(_view->GetAspect());

	LSL_LOG("db init");

	_db = new DataBase(this, "db");

	LSL_LOG("map init");

	_map = new Map(this);
	_map->SetName("map");
	_map->SetOwner(this);

	LSL_LOG("logic init");

	_logic = new Logic(this);	

	LSL_LOG("env init");

	_env = new Environment(this);

	ResetCamera();
}

void World::Free()
{
	ExitWorldEdit();
	ExitGame();
	_map->Clear();
	_graph->GetEngine().RemoveVideoRes(this);

	lsl::SafeDelete(_env);
	lsl::SafeDelete(_map);
	lsl::SafeDelete(_logic);
	if (_db)
	{
		_db->Release();
		lsl::SafeDelete(_db);
	}

	lsl::SafeDelete(_camera);
	lsl::SafeDelete(_resManager);
	if (_pxScene)
	{
		_pxScene->Release();
		_pxManager->DelScene(_pxScene);
	}
	lsl::SafeDelete(_pxManager);
	lsl::SafeDelete(_audio);
	lsl::SafeDelete(_videoPlayer);
	lsl::SafeDelete(_graph);
	lsl::SafeDelete(_view);
	lsl::SafeDelete(_control);

	LSL_ASSERT(_frameEvents.empty());
	LSL_ASSERT(_fixedStepEvents.empty());
	LSL_ASSERT(_lateProgressEvents.empty());
	LSL_ASSERT(_progressEvents.empty());
}

void World::LoadRes()
{
	LSL_LOG("world _resManager->Load");

	_resManager->Load();

	LSL_LOG("world _db->Init");

	_db->Init();

#ifdef DRAW_DEBUG_INFO
	if (_dbgInfo == NULL)
	{
		_dbgInfo = _graph->GetGUI().CreateLabel();
		_dbgInfo->SetFont(&_resManager->GetTextFontLib().Get("VerySmall"));
		_dbgInfo->SetHorAlign(gui::Text::haRight);
		_dbgInfo->SetVertAlign(gui::Text::vaTop);
		_dbgInfo->SetAlign(gui::Widget::waRightTop);
		AdjustDbgInfo(_graph->GetGUI().GetVPSize());
	}
#endif
}

void World::Terminate(int exitResult)
{
	_terminateResult = exitResult;
	_terminate = true;
}

bool World::IsTerminate() const
{
	return _terminate;
}

int World::GetTerminateResult() const
{
	return _terminateResult;
}

void World::UpdateLevel()
{
	if (static_cast<edit::Edit*>(_edit))
		static_cast<edit::Edit*>(_edit)->OnUpdateLevel();
}

void World::SetTimeResolution(unsigned period)
{
	if (_timeResolution != period)
	{
		if (_timeResolution != 0)
			timeEndPeriod(_timeResolution);

		_timeResolution = period;

		if (_timeResolution != 0)
			timeBeginPeriod(_timeResolution);
	}
}

void World::Progress(float deltaTime)
{
	_logic->OnProgress(deltaTime);

	ProgressEvents::const_iterator iter = _progressEvents.begin();

	while (iter != _progressEvents.end())
	{
		IProgressEvent* inst = *iter;

		inst->OnProgress(deltaTime);

		if (inst->removed())
		{
			iter = _progressEvents.erase(iter);
			inst->Release();
		}
		else
			++iter;
	}		
}

void World::FixedStep(float deltaTime)
{
	for (FixedStepEvents::const_iterator iter = _fixedStepEvents.begin(); iter != _fixedStepEvents.end(); ++iter)
		(*iter)->OnFixedStep(deltaTime);
}

void World::LateProgress(float deltaTime, bool pxStep)
{
	for (LateProgressEvents::const_iterator iter = _lateProgressEvents.begin(); iter != _lateProgressEvents.end(); ++iter)
		(*iter)->OnLateProgress(deltaTime, pxStep);
}

void World::FrameStep(float deltaTime, float pxAlpha)
{
	if (!_pause)
	{
		for (FrameEvents::const_iterator iter = _frameEvents.begin(); iter != _frameEvents.end(); ++iter)
			(*iter)->OnFrame(deltaTime, pxAlpha);

		_env->ProcessScene(deltaTime);
	}

	if (_game && _game->IsStartgame())
	{
		_game->netGame()->Process(_time, deltaTime);
#ifdef STEAM_SERVICE
		_game->steamService()->Process(deltaTime);
#endif
	}

	if (!_pause)
		_control->OnFrame(deltaTime);

	if (_game)
		_game->OnFrame(deltaTime, pxAlpha);
}

bool World::OnMouseClickEvent(const MouseClick& mClick)
{
	bool res = false;	

	res = res || _control->OnMouseClickEvent(mClick);

	res = res || _graph->GetGUI().OnMouseClickEvent(mClick.key, mClick.state, mClick.coord, mClick.shift1, false);	

	return res;
}

bool World::OnMouseMoveEvent(const MouseMove& mMove)
{
	bool res = false;

	res = res || _control->OnMouseMoveEvent(mMove);

	res = res || _graph->GetGUI().OnMouseMoveEvent(mMove.coord, mMove.shift1, false);	

	return res;
}

bool World::OnKeyEvent(unsigned key, KeyState state, bool repeat)
{
	return _control->OnKeyEvent(key, state, repeat);
}

void World::OnKeyChar(unsigned key, lsl::KeyState state, bool repeat)
{
	_control->OnKeyChar(key, state, repeat);
}

void World::OnReset(HWND window, lsl::Point resolution, bool fullScreen)
{
	LSL_LOG("world on reset");

	_graph->Reset(window, resolution, fullScreen);
	_videoPlayer->UpdateVideoWindow(resolution);	

	_syncFreq = _graph->GetDisplayMode().refreshRate;
	_camera->SetAspect(_view->GetAspect());	

	if (_game)
		_game->OnResetView();	

#ifdef DRAW_DEBUG_INFO
	AdjustDbgInfo(_graph->GetGUI().GetVPSize());
#endif

	ResetCamera();
}

bool World::OnPaint(HWND handle)
{
	if (_videoPlayer && _videoPlayer->OnPaint(handle))
		return true;

	return false;
}

void World::OnDisplayChange()
{
	if (_videoPlayer)
		_videoPlayer->DisplayModeChanged();
}

void World::OnWMGraphEvent()
{
	if (_videoPlayer)
		_videoPlayer->OnWMGraphEvent();
}

void World::OnGraphEvent(HWND hwnd, long eventCode, LONG_PTR param1, LONG_PTR param2)
{
	switch (eventCode)
	{		
	case EC_COMPLETE:
	case EC_USERABORT:
		_videoPlayer->Stop();
		break;

	case EC_ERRORABORT:
		LSL_LOG("videoPlayer EC_ERRORABORT");		
		_videoPlayer->Stop();
		break;	
	}

	_game->OnGraphEvent(hwnd, eventCode, param1, param2);
}

void World::OnResetDevice()
{
	ResetCamera();
}

void World::MainProgress()
{
	//simulation
	const float maxTimeStep = cMaxSimStep;
	const int maxTimeIter = cMaxSimIter;
	const float maxSimDT = (maxTimeIter + 1) * maxTimeStep;

	//frame sync
	const float syncStep = 1.0f/_syncFreq;
	const int syncFreq = _syncFreq;	
	const int syncNumFrames = 15;
	//запас синхронизации, в с
	const float syncUpThreshold = 0.002f;
	const float syncDownThreshold = 0.003f;
	//
	const float iterUpThreshold = syncUpThreshold / syncStep;
	const float iterDownThreshold = syncDownThreshold / syncStep;

	bool startRace = _game && _game->IsStartgame() && _game->GetRace()->IsStartRace();	
	bool syncModeOn = true;//_env->GetSyncFrameRate() == Environment::sfrFixed;
	bool enableSync = syncModeOn;
	if (_warmIterations > 0)
	{
		enableSync = false;
		--_warmIterations;
	}

	__int64 newTick;
	QueryPerformanceCounter((LARGE_INTEGER*)&newTick);
	if (_firstTick == 0)
	{
		_firstTick = newTick;
		_lastTick2 = _lastTick = newTick;
	}
	_time = static_cast<unsigned>((newTick - _firstTick) * 1000 / _cpuFreq);	
	double deltaTime = (newTick - _lastTick) / static_cast<double>(_cpuFreq);
	_lastTick = newTick;
	float dt = _dTimeReal = static_cast<float>(deltaTime);	

#if !_DEBUG || DEBUG_FRAME_SYNC
	if (enableSync)
	{
		_dtStack.push_back(FrameDT(deltaTime, 0.0f, 0.0f, 1, 1, _curTimeIter));
		if (_dtStack.size() > syncNumFrames)
			_dtStack.pop_front();
		double frameTime = 0;
		for (unsigned i = 0; i < _dtStack.size(); ++i)
			frameTime += _dtStack[i].dt;
		dt = static_cast<float>(frameTime/_dtStack.size());
	}
	else
		_dtStack.clear();
#endif

	dt = std::min(dt, maxSimDT);
	_timeAccum += dt;
	bool nextStep = _timeAccum >= maxTimeStep;

	_control->OnProgress(dt);

	if (!_pause && startRace)
		Progress(dt);

#ifdef DEBUG_FRAME_SYNC	
	__int64 tick;
	QueryPerformanceCounter((LARGE_INTEGER*)&tick);
#endif

	if (!nextStep)
		LateProgress(dt, false);
	else
		while (nextStep)
		{
			_timeAccum -= maxTimeStep;
			nextStep = _timeAccum >= maxTimeStep;

			if (!_pause && startRace)
			{
				//instead of PxSync. Always first! (after _pxScene->Compute(maxTimeStep))
				if (!nextStep)
					LateProgress(dt, true);

				FixedStep(maxTimeStep);

				_pxScene->Compute(maxTimeStep);
			}
		}

#ifdef DEBUG_FRAME_SYNC
	__int64 pxTick;
	QueryPerformanceCounter((LARGE_INTEGER*)&pxTick);
	pxTick = pxTick - tick;
#endif
	
#ifdef DEBUG_FRAME_SYNC
	QueryPerformanceCounter((LARGE_INTEGER*)&tick);
#endif

	float alpha = static_cast<float>(_timeAccum/maxTimeStep);
	FrameStep(dt, startRace ? alpha : -1.0f);

#ifdef DEBUG_FRAME_SYNC
	__int64 frameTick;
	QueryPerformanceCounter((LARGE_INTEGER*)&frameTick);
	frameTick = frameTick - tick;
#endif

	if (_videoMode)
		return;

#ifdef DEBUG_FRAME_SYNC
	QueryPerformanceCounter((LARGE_INTEGER*)&tick);
#endif

	_graph->MainThread();

#ifdef DEBUG_FRAME_SYNC
	__int64 threadTick;
	QueryPerformanceCounter((LARGE_INTEGER*)&threadTick);
	threadTick = threadTick - tick;
#endif

#ifdef DEBUG_FRAME_SYNC
	QueryPerformanceCounter((LARGE_INTEGER*)&tick);
#endif

	_audio->Compute(dt);

#ifdef DEBUG_FRAME_SYNC
	__int64 audioTick;
	QueryPerformanceCounter((LARGE_INTEGER*)&audioTick);
	audioTick = audioTick - tick;
#endif

#ifdef DEBUG_FRAME_SYNC
	QueryPerformanceCounter((LARGE_INTEGER*)&tick);
#endif

	if (!_graph->Render(dt, _pause))
		return;

	QueryPerformanceCounter((LARGE_INTEGER*)&newTick);	
	float renderTime = std::min((newTick - _lastTick) / static_cast<float>(_cpuFreq), maxSimDT);
#ifdef DEBUG_FRAME_SYNC	
	__int64 renderTick = newTick - tick;
#endif

//#if !_DEBUG || DEBUG_FRAME_SYNC
#if !_DEBUG
	BOOL inVBlank = false;

	if (syncModeOn && _curTimeIter > 1)
	{		
		int syncIter = std::max(static_cast<int>(_curTimeIter - syncFreq * (newTick - _lastTick2) / _cpuFreq), 1);

#ifdef DEBUG_FRAME_SYNC
		Profiler::I().Begin(lsl::StrFmt("blankTime f=%d", syncIter));
#endif

		inVBlank = true;

		while (syncIter > 0)
		{
			D3DRASTER_STATUS status;
			HRESULT hr = _graph->GetEngine().GetDriver().GetDevice()->GetRasterStatus(0, &status);

			if (hr != D3D_OK && hr != D3DCAPS_READ_SCANLINE)
			{
				inVBlank = false;
				break;
			}

			if (inVBlank != status.InVBlank)
			{
				inVBlank = status.InVBlank;
				if (inVBlank)
					--syncIter;
			}
		}

#ifdef DEBUG_FRAME_SYNC
		Profiler::I().End();
#endif

		QueryPerformanceCounter((LARGE_INTEGER*)&newTick);
		_lastTick2 = newTick;
	}
#endif

#ifdef DEBUG_FRAME_SYNC
	Profiler::I().Begin("presentTime");
#endif

	_graph->Present();

#ifdef DEBUG_FRAME_SYNC
	Profiler::I().End();	
#endif

	_graph->GPUSync();	

	__int64 syncTick;
	QueryPerformanceCounter((LARGE_INTEGER*)&syncTick);
	float gpuTime = std::min((syncTick - newTick) / static_cast<float>(_cpuFreq), maxSimDT);

//#if !_DEBUG || DEBUG_FRAME_SYNC
#if !_DEBUG
	if (syncModeOn && inVBlank == false)
	{
#ifdef DEBUG_FRAME_SYNC
		Profiler::I().Begin("syncTime");
#endif
		__int64 fixTick = static_cast<__int64>(_curTimeIter * _cpuFreq / syncFreq);

		//int sleepTime = (_lastTick + fixTick - syncTick) * 1000 / _cpuFreq;
		//if (sleepTime > 1)
		//	Sleep(sleepTime);

		while (syncTick - _lastTick < fixTick)
		{			
			QueryPerformanceCounter((LARGE_INTEGER*)&syncTick);
		}

#ifdef DEBUG_FRAME_SYNC
		Profiler::I().End();
#endif
	}
#endif

#if !_DEBUG || DEBUG_FRAME_SYNC
	//_dtStack may be empty by user input (camera switch)
	if (enableSync && !_dtStack.empty())
	{
		_dtStack.back().renderTime = renderTime;
		_dtStack.back().gpuTime = gpuTime;

		float renderTimeMin = renderTime;
		float renderTimeMax = renderTime;
		float renderTimeAvg = renderTime;
		float gpuTimeAvg = gpuTime;
		float gpuTimeMax = gpuTime;
		float downTimeIterAvg = 0.0f;
		float upTimeIterAvg = 0.0f;

		for (unsigned i = 0; i < _dtStack.size() - 1; ++i)
		{
			renderTimeAvg += _dtStack[i].renderTime;
			downTimeIterAvg += _dtStack[i].downTimeIter;
			upTimeIterAvg += _dtStack[i].upTimeIter;
			gpuTimeAvg += _dtStack[i].gpuTime;

			if (_dtStack[i].curTimeITer == _curTimeIter && gpuTimeMax < _dtStack[i].gpuTime)
				gpuTimeMax = _dtStack[i].gpuTime;
			if (renderTimeMin > _dtStack[i].renderTime)
				renderTimeMin = _dtStack[i].renderTime;
			if (renderTimeMax < _dtStack[i].renderTime)
				renderTimeMax = _dtStack[i].renderTime;
		}

		renderTimeAvg = renderTimeAvg / _dtStack.size();
		gpuTimeAvg = gpuTimeAvg / _dtStack.size();

		float drawTimeMin = renderTimeMin + gpuTimeAvg;
		float drawTimeMax = renderTimeMax + gpuTimeMax;

		//увеличение числа итераций при превышении порога рассинхронизации в syncError
		float newTimeIterMin = drawTimeMin / syncStep;
		float newTimeIterMax = drawTimeMax / syncStep;		

		_dtStack.back().upTimeIter = newTimeIterMax;
		upTimeIterAvg = (upTimeIterAvg + newTimeIterMax)/_dtStack.size();

		_dtStack.back().downTimeIter = newTimeIterMin;
		downTimeIterAvg = (downTimeIterAvg + _dtStack.back().downTimeIter)/_dtStack.size();

		//down	
		if (downTimeIterAvg > _curTimeIter + iterDownThreshold)
		{
			if (gpuTimeAvg > 0.004f)
			{
				float newIter = gpuTimeAvg/syncStep;

				for (int i = 0; i < _curTimeIter; ++i)
				{
					float curIter = _iterBuf[i].gpuTime;
					_iterBuf[i].gpuTime = std::min(curIter + std::max(newIter - curIter, 0.0f) * 0.5f, std::max(curIter, newIter));
				}
			}

			for (unsigned i = 0; i < _dtStack.size(); ++i)
				_dtStack[i].gpuTime = 0.0f;

			_curTimeIter = lsl::ClampValue(static_cast<int>(ceil(downTimeIterAvg - iterDownThreshold)), 1, cMaxSimIter);
		}
		//up
		else if (upTimeIterAvg < _curTimeIter - iterUpThreshold)
		{	
			int upTimeIter = lsl::ClampValue(static_cast<int>(ceil(upTimeIterAvg + iterUpThreshold)), 1, cMaxSimIter);
			for (; upTimeIter < _curTimeIter; ++upTimeIter)
			{
				if (upTimeIterAvg < upTimeIter - (iterUpThreshold + std::max(_iterBuf[upTimeIter - 1].gpuTime - gpuTimeAvg/syncStep, 0.0f)))
				{
					_curTimeIter = upTimeIter;
					break;
				}
			}
		}
	}	
#endif	
	
#ifdef DEBUG_FRAME_SYNC
	static int fixDbgTimeIter = 0;

	if (_control->GetAsyncKey(VK_CONTROL))
	{
		if (_control->GetAsyncKey(VK_NUMPAD0))
			fixDbgTimeIter = 0;
		if (_control->GetAsyncKey(VK_NUMPAD1))
			fixDbgTimeIter = 1;
		if (_control->GetAsyncKey(VK_NUMPAD2))
			fixDbgTimeIter = 2;
		if (_control->GetAsyncKey(VK_NUMPAD3))
			fixDbgTimeIter = 3;
		if (_control->GetAsyncKey(VK_NUMPAD4))
			fixDbgTimeIter = 4;
		if (_control->GetAsyncKey(VK_NUMPAD5))
			fixDbgTimeIter = 5;
		if (_control->GetAsyncKey(VK_NUMPAD6))
			fixDbgTimeIter = 6;
		if (_control->GetAsyncKey(VK_DELETE))
			fixDbgTimeIter = -1;	
	}

	if (fixDbgTimeIter > 0)
	{
		_curTimeIter = fixDbgTimeIter;
		enableSync = true;
	}	
	else if (fixDbgTimeIter == -1)
		enableSync = false;
#endif

#ifdef DEBUG_FRAME_SYNC
	static float dbgDumpTime = 0;
	static double minDt = 0;
	static double maxDt = 0;
	static double minAvg = 0;
	static double maxAvg = 0;
	static int minIter = 0;
	static int maxIter = 0;
	static float minAlpha = 0;
	static float maxAlpha = 0;	
	static int minCurIter = 0;
	static int maxCurIter = 0;	
	static float maxGPUTime = 0;
	static float maxRenderTime = 0;
	static float maxAudioTime = 0;
	static float maxThreadTime = 0;	
	static float maxFrameTime = 0;	
	static float maxPxTime = 0;	

	float audioTime = audioTick / static_cast<float>(_cpuFreq);
	float threadTime = threadTick / static_cast<float>(_cpuFreq);
	float frameTime = frameTick / static_cast<float>(_cpuFreq);
	float pxTime = pxTick / static_cast<float>(_cpuFreq);

	if (_dbgInfo && dbgDumpTime > 1.0f)
	{
		std::string text = lsl::StrFmt((lsl::string() +
			"dt=%0.1f min=%0.1f max=%0.1f\n"
			"avg=%0.1f min=%0.1f max=%0.1f\n"			
			"alpha=%0.3f min=%0.3f max=%0.3f\n"			
			"timeIter=%d min=%d max=%d\n"			
			"gpuTime=%0.1f min=%0.1f max=%0.1f\n"
			"renderTime=%0.1f min=%0.1f max=%0.1f\n"
			"audioTime=%0.1f min=%0.1f max=%0.1f\n"
			"threadTime=%0.1f min=%0.1f max=%0.1f\n"
			"frameTime=%0.1f min=%0.1f max=%0.1f\n"
			"pxTime=%0.1f min=%0.1f max=%0.1f\n"			
			"\n"
			).c_str(),
			deltaTime * 1000, minDt * 1000, maxDt * 1000,
			dt * 1000, minAvg * 1000, maxAvg * 1000,			
			alpha, minAlpha, maxAlpha,			
			_curTimeIter, minCurIter, maxCurIter,			
			gpuTime * 1000, gpuTime * 1000, maxGPUTime * 1000,
			renderTime * 1000, renderTime * 1000, maxRenderTime * 1000,	
			audioTime * 1000, audioTime * 1000, maxAudioTime * 1000,
			threadTime * 1000, threadTime * 1000, maxThreadTime * 1000,
			frameTime * 1000, frameTime * 1000, maxFrameTime * 1000,
			pxTime * 1000, pxTime * 1000, maxPxTime * 1000
		);

		for (Profiler::Samples::const_iterator iter = Profiler::I().samples().begin(); iter != Profiler::I().samples().end(); ++iter)
			if (iter->second.updated)
			{
				float avgDt = iter->second.summDt / iter->second.frames;
				text += lsl::StrFmt("%s=%0.1f min=%0.1f max=%0.1f\n", iter->first.c_str(), avgDt, iter->second.minDt, iter->second.maxDt);

				Profiler::I().ResetSample(iter->first);
			}

		lsl::string gpuText = "iterBuf=";
		for (int i = 0; i < cMaxSimIter; ++i)
			gpuText += lsl::StrFmt("{%0.3f}", _iterBuf[i].gpuTime) + "\n";

		_dbgInfo->SetText(text + gpuText);

		minDt = 1.0f;
		maxDt = 0.0f;
		minAvg = 1.0f;
		maxAvg = 0.0f;
		minIter = 999;
		maxIter = 0;
		minAlpha = 1.0f;
		maxAlpha = 0.0f;		
		minCurIter = maxTimeIter;
		maxCurIter = 0;		
		maxGPUTime = 0;
		maxRenderTime = 0;
		maxAudioTime = 0;
		maxThreadTime = 0;
		maxFrameTime = 0;
		maxPxTime = 0;

		dbgDumpTime = 0.0f;
	}
	
	if (minDt > deltaTime)
		minDt = deltaTime;
	if (maxDt < deltaTime)
		maxDt = deltaTime;
	if (minAvg > dt)
		minAvg = dt;
	if (maxAvg < dt)
		maxAvg = dt;
	if (minAlpha > alpha)
		minAlpha = alpha;
	if (maxAlpha < alpha)
		maxAlpha = alpha;	
	if (minCurIter > _curTimeIter)
		minCurIter = _curTimeIter;
	if (maxCurIter < _curTimeIter)
		maxCurIter = _curTimeIter;	
	if (maxGPUTime < gpuTime)
		maxGPUTime = gpuTime;
	if (maxRenderTime < renderTime)
		maxRenderTime = renderTime;
	if (maxAudioTime < audioTime)
		maxAudioTime = audioTime;	
	if (maxThreadTime < threadTime)
		maxThreadTime = threadTime;		
	if (maxFrameTime < frameTime)
		maxFrameTime = frameTime;	
	if (maxPxTime < pxTime)
		maxPxTime = pxTime;		
	
	dbgDumpTime += dt;
	
	/*static std::fstream fileDump;
	static float fileDumpTime = 0.0f;	
	static int fileDumpFrame = 0;	

	if (fileDumpTime > 0)
	{
		if (!fileDump.is_open())
			fileDump.open("C:\\dump.txt", std::ios_base::out);

		fileDump << "frame=" << fileDumpFrame << '\n';
		fileDump << "deltaTime=" << deltaTime << '\n';		
		fileDump << "timeAccum=" << _timeAccum << '\n';				
		fileDump << '\n';
		fileDump << '\n';

		fileDumpTime -= dt;
		++fileDumpFrame;
		if (fileDumpTime <= 0)
			fileDump.close();
	}
	else if (GetAsyncKeyState('V'))
	{
		fileDumpTime = 1.0f;
		fileDumpFrame = 0;
	}*/
#endif

#if DEBUG_NET
	if (_game && _game->netGame())
	{
		static float pingTime = 0;
		static unsigned framePing = 0;
		//
		static unsigned pingBuf[4] = {0, 0, 0, 0};
		static unsigned lastPing[4] = {0, 0, 0, 0};
		//
		static unsigned bytesSendBuf[4] = {0, 0, 0, 0};
		static unsigned bytesSendLast[4] = {0, 0, 0, 0};
		//
		static unsigned bytesReceivedBuf[4] = {0, 0, 0, 0};
		static unsigned bytesReceivedLast[4] = {0, 0, 0, 0};

		std::string text = "";
		
		if (net::GetNetService().isClient())
		{
			std::string status = "disconnected";
			if (net::GetNetService().isConnecting())
				status = "connecting...";
			else if (net::GetNetService().isConnected())
				status = "connected";

			net::INetPlayer* client = net::GetNetService().player();
			net::INetConnection* connection = net::GetNetService().GetConnection(0);
			net::INetChannel* channel = net::GetNetService().channel();
			if (connection)
			{
				framePing += 1;			
				pingBuf[0] += channel->ping();
				if ((pingTime += dt) > 0.5f)
				{
					unsigned bytesSend = connection->bytesSend() + channel->bytesSend();
					unsigned bytesReceived = connection->bytesReceived() + channel->bytesReceived();
					lastPing[0] = pingBuf[0] / framePing;
					bytesSendLast[0] = bytesSend - bytesSendBuf[0];
					bytesSendBuf[0] = bytesSend;
					bytesReceivedLast[0] = bytesReceived - bytesReceivedBuf[0];
					bytesReceivedBuf[0] = bytesReceived;
					pingBuf[0] = 0;				
					pingTime = 0;
					framePing = 0;
				}
			}

			text += lsl::StrFmt((lsl::string() +
				"client status=%s\n"
				"ping=%d\n"
				"bytesSend=%d\n"
				"bytesReceived=%d\n"
				"\n").c_str(), status.c_str(), lastPing[0], bytesSendLast[0], bytesReceivedLast[0]);
		}

		if (net::GetNetService().isServer())
		{
			net::INetPlayer* server = net::GetNetService().player();

			std::string status = "started";

			std::string players = "";
			bool setPing = (pingTime += dt) > 0.5f;
			if (setPing)
				pingTime = 0;
			for (unsigned i = 0; i < net::GetNetService().connectionCount(); ++i)
			{
				net::INetConnection* player = net::GetNetService().GetConnection(i);
				net::INetChannel* channel = net::GetNetService().channel();

				framePing += 1;
				pingBuf[i] += channel->ping();				
				if (setPing)
				{
					unsigned bytesSend = player->bytesSend() + channel->bytesSend();
					unsigned bytesReceived = player->bytesReceived() + channel->bytesReceived();
					lastPing[i] = pingBuf[i] / framePing;
					bytesSendLast[i] = bytesSend - bytesSendBuf[i];
					bytesSendBuf[i] = bytesSend;
					bytesReceivedLast[i] = bytesReceived - bytesReceivedBuf[i];
					bytesReceivedBuf[i] = bytesReceived;
					pingBuf[i] = 0;					
					framePing = 0;
				}

				players += lsl::StrFmt("player[%d](%d) ping=%d\n bytesSend=%d\n bytesReceived=%d\n", i, player->id(), lastPing[i], bytesSendLast[i], bytesReceivedLast[i]);
			}

			text += lsl::StrFmt((lsl::string() +
				"server status=%s\n"
				"numPlayers=%d\n"
				"%s"
				"\n").c_str(), status.c_str(), net::GetNetService().connectionCount(), players.c_str());
		}

		_dbgInfo->SetText(text);
	}
#endif
}

void World::Pause(bool pause)
{
	_pause = pause;
}

bool World::IsPaused() const
{
	return _pause;
}

void World::ResetInput(bool reset)
{
	_inputWasReset = reset;
	_control->ResetInput(reset);	
}

bool World::InputWasReset() const
{
	return _inputWasReset;
}

void World::ResetCamera()
{	
	_curTimeIter = 1;
	_warmIterations = 10;
	_dtStack.clear();
	ZeroMemory(_iterBuf, sizeof(_iterBuf));
	
	//if (_view->GetDesc().fullscreen && (_env->GetSyncFrameRate() == Environment::sfrFixed))
	//	SetTimeResolution(1);
	//else
	//	SetTimeResolution(0);
}

bool World::GetVideoMode() const
{
	return _videoMode;
}

void World::SetVideoMode(bool value)
{
	if (_videoMode != value)
	{
		_videoMode = value;

		if (_view->GetDesc().fullscreen)
			_view->Reset(_view->GetDesc());
		else if (!_videoMode)
		{
			View::Desc desc = _view->GetDesc();
			desc.fullscreen = true;
			_view->Reset(desc);
		}
	}
}

bool World::IsVideoPlaying() const
{
	return _videoMode && _videoPlayer->state() != STATE_NO_GRAPH;
}

void World::RunGame()
{
	if (_game)
		return;

	graph::Engine::d3dxUse(true);
	_env->ApplyQuality();

	_game = new GameMode(this);

#if _DEBUG | DEBUG_PX
	_game->Run(false);
#else
	_game->Run(true);
#endif
}

void World::ExitGame()
{
	if (_game == NULL)
		return;

#ifdef DRAW_DEBUG_INFO
	if (_dbgInfo)
		_graph->GetGUI().ReleaseWidget(_dbgInfo);
#endif

	lsl::SafeDelete(_game);
}

void World::RunWorldEdit()
{
	if (_edit)
		return;

	graph::Engine::d3dxUse(false);
	_env->ApplyQuality();

	LoadRes();
	_edit = new edit::Edit(this);

	_camera->ChangeStyle(CameraManager::csFreeView);	
	_env->SetEditMode(true);
}

void World::ExitWorldEdit()
{
	lsl::SafeDelete(_edit);
	_env->SetEditMode(false);
}

void World::SaveLevel(const std::string& level)
{
	lsl::RootNode rootNode("root", this);

	lsl::SWriter* writer = rootNode.BeginSave();	
	writer->WriteValue("map", _map);
	//writer->WriteValue("logic", _logic);
	rootNode.EndSave();

	lsl::SerialFileXML xmlFile;
	xmlFile.SaveNodeToFile(rootNode, level);
}

void World::LoadLevel(const std::string& level)
{
	LSL_LOG("world LoadLevel");

	lsl::RootNode rootNode("root", this);
	lsl::SerialFileXML xmlFile;
	xmlFile.LoadNodeFromFile(rootNode, level);

	lsl::SReader* reader = rootNode.BeginLoad();
	reader->ReadValue("map", _map);
	//reader->ReadValue("logic", _logic);
	rootNode.EndLoad();

	UpdateLevel();
}

void World::RegFixedStepEvent(IFixedStepEvent* user)
{
	if (_fixedStepEvents.IsFind(user))
		return;

	user->AddRef();
	_fixedStepEvents.push_back(user);
}

void World::UnregFixedStepEvent(IFixedStepEvent* user)
{
	if (!_fixedStepEvents.IsFind(user))
		return;

	_fixedStepEvents.Remove(user);
	user->Release();
}

void World::RegProgressEvent(IProgressEvent* user)
{
	if (_progressEvents.IsFind(user))
		return;

	user->AddRef();
	_progressEvents.push_back(user);
}

void World::UnregProgressEvent(IProgressEvent* user)
{
	if (!_progressEvents.IsFind(user))
		return;

	_progressEvents.Remove(user);
	user->Release();
}

void World::RegLateProgressEvent(ILateProgressEvent* user)
{
	if (_lateProgressEvents.IsFind(user))
		return;

	user->AddRef();
	_lateProgressEvents.push_back(user);
}

void World::UnregLateProgressEvent(ILateProgressEvent* user)
{
	if (!_lateProgressEvents.IsFind(user))
		return;

	_lateProgressEvents.Remove(user);
	user->Release();
}

void World::RegFrameEvent(IFrameEvent* user)
{
	if (_frameEvents.IsFind(user))
		return;

	user->AddRef();
	_frameEvents.push_back(user);
}

void World::UnregFrameEvent(IFrameEvent* user)
{
	if (!_frameEvents.IsFind(user))
		return;

	_frameEvents.Remove(user);
	user->Release();
}

unsigned World::time() const
{
	return _time;
}

float World::dTimeReal() const
{
	return _dTimeReal;
}

ControlManager* World::GetControl()
{
	return _control;
}

IView* World::GetView()
{
	return _view;
}

GraphManager* World::GetGraph()
{
	return _graph;
}

snd::Engine* World::GetAudio()
{
	return _audio;
}

video::Player* World::GetVideo()
{
	return _videoPlayer;
}

px::Manager* World::GetPxManager()
{
	return _pxManager;
}

px::Scene* World::GetPxScene()
{
	return _pxScene;
}

ResourceManager* World::GetResManager()
{
	return _resManager;
}

CameraManager* World::GetCamera()
{
	return _camera;
}

ICameraManager* World::GetICamera()
{
	return _camera;
}

Logic* World::GetLogic()
{
	return _logic;
}

DataBase* World::GetDB()
{
	return _db;
}

Map* World::GetMap()
{
	return _map;
}

Environment* World::GetEnv()
{
	return _env;
}

edit::IEdit* World::GetEdit()
{
	return _edit;
}

GameMode* World::GetGame()
{
	return _game;
}

}

}