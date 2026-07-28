#include "stdafx.h"

#include "Rock3dGame.h"

namespace
{

#ifdef _DEBUG
	bool fullScreen = false;
#else
	bool fullScreen = false;
#endif

const lsl::Point cResolution(1280, 720);

HWND mainWindow;
r3d::IWorld* rock3dWorld;




void OnKeyEvent(unsigned key, lsl::KeyState state, bool repeat)
{
	if (rock3dWorld)
		rock3dWorld->GetView()->OnKeyEvent(key, state, repeat);
}

void OnKeyChar(unsigned key, lsl::KeyState state, bool repeat)
{
	if (rock3dWorld)
		rock3dWorld->GetView()->OnKeyChar(key, state, repeat);
}

void OnMouseClickEvent(lsl::MouseKey key, lsl::KeyState state, lsl::Point coord, bool shift, bool ctrl)
{
	if (rock3dWorld)
		rock3dWorld->GetView()->OnMouseClickEvent(key, state, coord, shift, ctrl);
}

void OnMouseMoveEvent(lsl::Point coord, bool shift, bool ctrl)
{
	if (rock3dWorld)
		rock3dWorld->GetView()->OnMouseMoveEvent(coord, shift, ctrl);
}

DWORD GetWndStyles(bool fullScreen)
{
	if (fullScreen)
	{
		return WS_POPUP | WS_VISIBLE;
	}
	else
	{
		return WS_OVERLAPPEDWINDOW | WS_VISIBLE;
	}	
}

DWORD GetExtWndStyles(bool fullScreen)
{
	if (fullScreen)
	{
		return WS_EX_TOPMOST;
	}
	else
	{
		return 0;
	}
}

/*
 * Everything below is the Win32 shell: window class, message loop and
 * _tWinMain. Replacing it with an SDL window and event loop is its own piece
 * of work -- see the plan's Stage 4 -- so off Windows it compiles out and the
 * placeholder entry point at the bottom of this file stands in.
 */
#ifdef _WIN32

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY: 
		PostQuitMessage(0);
		return 0;

	case WM_KEYDOWN:
	{
		//Только если клавиша не повторяется (не зажата!)
		bool repeat = (lParam >> 30) & 0x1;

		OnKeyEvent(wParam, lsl::ksDown, repeat);
		return 0;
	}

	case WM_KEYUP:	
		OnKeyEvent(wParam, lsl::ksUp, false);
		return 0;

	case WM_CHAR:
	{
		bool repeat = (lParam >> 30) & 0x1;
		bool released = (lParam >> 31) & 0x1;

		OnKeyChar(wParam, released ? lsl::ksUp : lsl::ksDown, repeat);
		return 0;
	}

	case WM_LBUTTONDOWN:
	{
		POINTS points = MAKEPOINTS(lParam);

		OnMouseClickEvent(lsl::mkLeft, lsl::ksDown, lsl::Point(points.x, points.y), (wParam & MK_SHIFT) ? true : false, (wParam & MK_CONTROL) ? true : false);
		return 0;
	}

	case WM_LBUTTONUP:
	{
		POINTS points = MAKEPOINTS(lParam);

		OnMouseClickEvent(lsl::mkLeft, lsl::ksUp, lsl::Point(points.x, points.y), (wParam & MK_SHIFT) ? true : false, (wParam & MK_CONTROL) ? true : false);
		return 0;
	}

	case WM_RBUTTONDOWN:
	{
		POINTS points = MAKEPOINTS(lParam);

		OnMouseClickEvent(lsl::mkRight, lsl::ksDown, lsl::Point(points.x, points.y), (wParam & MK_SHIFT) ? true : false, (wParam & MK_CONTROL) ? true : false);
		return 0;
	}

	case WM_RBUTTONUP:
	{
		POINTS points = MAKEPOINTS(lParam);

		OnMouseClickEvent(lsl::mkRight, lsl::ksUp, lsl::Point(points.x, points.y), (wParam & MK_SHIFT) ? true : false, (wParam & MK_CONTROL) ? true : false);
		return 0;
	}

	case WM_MOUSEMOVE:
	{
		POINTS points = MAKEPOINTS(lParam);

		OnMouseMoveEvent(lsl::Point(points.x, points.y), (wParam & MK_SHIFT) ? true : false, (wParam & MK_CONTROL) ? true : false);
		return 0;
	}	

	case WM_SETCURSOR:
		// Turn off window cursor
#ifndef _DEBUG	
		SetCursor(0);
#endif
		return 0;

	case WM_PAINT:
		if (rock3dWorld && rock3dWorld->OnPaint(hWnd))
			return 0;
		break;

	case WM_ERASEBKGND:
		if (rock3dWorld && rock3dWorld->IsVideoPlaying())
			return 1;

	case WM_DISPLAYCHANGE:
		if (rock3dWorld)
			rock3dWorld->OnDisplayChange();
		break;

	case r3d::WM_GRAPH_EVENT:
		if (rock3dWorld)
			rock3dWorld->OnWMGraphEvent();
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

int MainLoop()
{
	int resetInputFrames = 0;

	while (true)
	{	
		bool inputWasReset = rock3dWorld->InputWasReset();

		if (inputWasReset && (++resetInputFrames) >= 3)
		{
			resetInputFrames = 0;
			rock3dWorld->ResetInput(false);
		}

		MSG msg;
		//Обработка сообщений
		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE | PM_QS_INPUT | PM_QS_POSTMESSAGE | PM_QS_SENDMESSAGE))
		{
			if (msg.message == WM_QUIT)
				return EXIT_SUCCESS;
			if (inputWasReset && (msg.message != WM_SETCURSOR && msg.message != WM_DESTROY))
				continue;

			TranslateMessage(&msg);
			DispatchMessage(&msg);

			inputWasReset = inputWasReset || rock3dWorld->InputWasReset();
		}

		//првоеряем состояние, в случае успеха выход
		if (rock3dWorld->IsTerminate())
			return rock3dWorld->GetTerminateResult();

		//Рендерим здесь чтобы не блокировать обработку сообщений (например сообщений от клавиатуры и мыши, что используется)
		rock3dWorld->MainProgress();
	}
}

void ErrMessage(const std::string& message)
{
	MessageBox(mainWindow, message.c_str(), "Error", MB_OK | MB_ICONERROR);
}

}




int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
#ifdef DEBUG_MEMORY
	_CrtMemState _ms;
	_CrtMemCheckpoint(&_ms);
#endif

	int exitResult = EXIT_SUCCESS;

	SetCurrentDirectoryW(lsl::GetAppPath().c_str());
	lsl::appLog.Clear();
	lsl::appLog.Append("Init...");

	try
	{
		lsl::Point pnt(0, 0);

		LOGBRUSH wndBrushDesc;
		wndBrushDesc.lbColor = RGB(0, 0, 0);
		wndBrushDesc.lbStyle = BS_SOLID;
		wndBrushDesc.lbHatch = 0;
		HBRUSH wndBrush = CreateBrushIndirect(&wndBrushDesc);

		RECT rect;
		rect.left = rect.top = 0;
		rect.right = cResolution.x;
		rect.bottom = cResolution.y;
		if (!fullScreen)
			AdjustWindowRect(&rect, GetWndStyles(fullScreen), false);

		WNDCLASSEX wc;
		wc.cbSize        = sizeof(WNDCLASSEX);
		wc.style         = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc   = &MainWndProc;
		wc.cbClsExtra    = 0;
		wc.cbWndExtra    = 0;
		wc.hInstance     = hInstance;
		wc.hIcon         = LoadIcon(hInstance, IDI_APPLICATION);
		wc.hCursor       = LoadCursor(0, IDC_ARROW);
		wc.hbrBackground = wndBrush;
		wc.lpszMenuName  = 0;
		wc.lpszClassName = _T("Rock3D");
		wc.hIconSm       = LoadIcon(hInstance, MAKEINTRESOURCE(108));
		RegisterClassEx(&wc);
		mainWindow = CreateWindowEx(GetExtWndStyles(fullScreen), _T("Rock3D"), _T(""), GetWndStyles(fullScreen), pnt.x, pnt.y, rect.right - rect.left, rect.bottom - rect.top, 0, 0, hInstance, 0);		
				
		r3d::IView::Desc desc;
		desc.fullscreen = fullScreen;
		desc.handle = mainWindow;
		desc.resolution = fullScreen ? lsl::Point(0, 0) : cResolution;

		rock3dWorld = r3d::CreateWorld(desc, true);
		rock3dWorld->RunGame();
		
		lsl::appLog.Append("Run...");
		
		exitResult = MainLoop();

		lsl::appLog.Append("Terminate...");

		r3d::ReleaseWorld(rock3dWorld);
	}
	//lsl искл. Автоматически записывается в лог и делает assert
	catch (const lsl::Error& err)
	{
		ErrMessage(err.what());		
	}
//отключаем стд исключения чтобы точнее поймать их место в дебагере
#ifndef _DEBUG
	catch(const std::exception& err)
	{
		//вручную останавливаем, чтобы поймать место
		LSL_ASSERT(false);

		lsl::appLog << "stdError: " << err.what() << '\n';
		ErrMessage(err.what());
	}
	catch (...)
	{
		//вручную останавливаем, чтобы поймать место
		LSL_ASSERT(false);

		lsl::appLog << "undefError" << '\n';
		ErrMessage("undefError");
	}
#endif

	lsl::appLog.Append("Exit");
	lsl::FileSystem::Release();

#ifdef DEBUG_MEMORY	
	_CrtMemDumpAllObjectsSince(&_ms);
	//_CrtDumpMemoryLeaks();
#endif

	return exitResult;
}

#else

/*
 * The SDL window, event loop and key mapping -- MainWndProc and MainLoop's
 * counterparts. Included rather than compiled separately so it shares this
 * file's anonymous namespace and the forwarders above it.
 */
#include "sdl_shell.cpp"

//Closes the anonymous namespace opened at the top of the file; the brace that
//did so on Windows sits inside the guarded shell above.
}

int main(int argc, char* argv[])
{
	int exitResult = EXIT_SUCCESS;

	lsl::appLog.Clear();
	lsl::appLog.Append("Init...");

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
	{
		std::fprintf(stderr, "rrr3d: SDL_Init failed: %s\n", SDL_GetError());
		return EXIT_FAILURE;
	}

	try
	{
		/*
		 * SDL_WINDOW_METAL so SDL gives the window a layer-hosting content
		 * view. Without it the view is not set up for a CAMetalLayer and
		 * attaching one in metalbridge_present.mm fights AppKit for it.
		 */
		sdlWindow = SDL_CreateWindow("Motor Rock", cResolution.x, cResolution.y,
			SDL_WINDOW_METAL | (fullScreen ? SDL_WINDOW_FULLSCREEN : 0));

		if (!sdlWindow)
			throw lsl::Error(lsl::StrFmt("SDL_CreateWindow failed: %s", SDL_GetError()));

		r3d::IView::Desc desc;
		desc.fullscreen = fullScreen;
		//SDL's CAMetalLayer; CreateMetalViewFromHWND configures it in place.
		desc.handle = MetalLayerHandle(sdlWindow);
		desc.resolution = fullScreen ? lsl::Point(0, 0) : cResolution;

		rock3dWorld = r3d::CreateWorld(desc, true);
		rock3dWorld->RunGame();

		lsl::appLog.Append("Run...");

		exitResult = MainLoop(rock3dWorld);

		lsl::appLog.Append("Terminate...");

		r3d::ReleaseWorld(rock3dWorld);
	}
	catch (const lsl::Error& err)
	{
		std::fprintf(stderr, "rrr3d: %s\n", err.what());
		exitResult = EXIT_FAILURE;
	}
	catch (const std::exception& err)
	{
		std::fprintf(stderr, "rrr3d: %s\n", err.what());
		exitResult = EXIT_FAILURE;
	}

	if (sdlMetalView)
		SDL_Metal_DestroyView(sdlMetalView);
	if (sdlWindow)
		SDL_DestroyWindow(sdlWindow);
	SDL_Quit();

	lsl::appLog.Append("Exit");
	lsl::FileSystem::Release();

	return exitResult;
}

#endif /* _WIN32 */
