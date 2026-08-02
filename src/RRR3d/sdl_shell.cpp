/*
 * The SDL3 shell: the non-Windows entry point, window and main loop.
 *
 * SDL events are NOT translated into Win32 MSG structs. That would look like a
 * faithful port and would be the wrong shape: the engine's OnKeyEvent,
 * OnMouseMoveEvent and MainProgress are plain calls, and MainWndProc only ever
 * existed because Win32 delivers input as messages. There is no message queue
 * here to reproduce.
 *
 * The window handle the engine receives is not a window. It is the
 * CAMetalLayer from SDL_Metal_CreateView, because that is what the D3D9-on-Metal
 * stack needs to draw into -- and it is why XPlatform's GetClientRect and
 * ScreenToClient answer from a registered size rather than from the handle.
 *
 * The shell does not own the layer. Creating a second CAMetalLayer in the same
 * view means the one drawn is not the one composited, which is a defect that
 * looks like the game rendering nothing at all.
 */

#include "stdafx.h"

#include "Rock3dGame.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>

namespace
{

const lsl::Point cResolution(1280, 720);
const bool cFullScreen = false;

SDL_Window* mainWindow = NULL;
SDL_MetalView metalView = NULL;
void* metalLayer = NULL;
r3d::IWorld* rock3dWorld = NULL;

lsl::Point MouseCoord(float x, float y)
{
	return lsl::Point(static_cast<int>(x), static_cast<int>(y));
}

bool ShiftHeld(SDL_Keymod mod)   { return (mod & SDL_KMOD_SHIFT) != 0; }
bool ControlHeld(SDL_Keymod mod) { return (mod & SDL_KMOD_CTRL) != 0; }

/* Publishes the drawable size to XPlatform, which is what GetClientRect and
   ScreenToClient answer from -- nothing here can ask the handle, because the
   handle is a CAMetalLayer. */
void PublishClientSize()
{
	int width = 0;
	int height = 0;
	SDL_GetWindowSizeInPixels(mainWindow, &width, &height);

	RegisterClientSize(static_cast<HWND>(metalLayer), width, height);
}

/*
 * One SDL event, delivered straight to the engine.
 *
 * Returns false when the application should quit, which is the only thing the
 * Win32 loop used WM_QUIT for.
 */
bool HandleEvent(const SDL_Event& event)
{
	if (!rock3dWorld)
		return true;

	r3d::IView* view = rock3dWorld->GetView();

	switch (event.type)
	{
	case SDL_EVENT_QUIT:
		return false;

	case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
	case SDL_EVENT_WINDOW_RESIZED:
		PublishClientSize();
		rock3dWorld->OnDisplayChange();
		return true;

	case SDL_EVENT_KEY_DOWN:
	{
		const unsigned key = VirtualKeyFromScancode(event.key.scancode);
		if (key)
			view->OnKeyEvent(key, lsl::ksDown, event.key.repeat);
		return true;
	}

	case SDL_EVENT_KEY_UP:
	{
		const unsigned key = VirtualKeyFromScancode(event.key.scancode);
		if (key)
			view->OnKeyEvent(key, lsl::ksUp, false);
		return true;
	}

	case SDL_EVENT_TEXT_INPUT:
		/*
		 * WM_CHAR's replacement, and it is a separate event for the same reason
		 * Windows separated it: this is composed text, after the keyboard
		 * layout and any dead keys have had their say, which is what a text
		 * field wants and what a movement key must not use.
		 */
		for (const char* c = event.text.text; *c; ++c)
			view->OnKeyChar(static_cast<unsigned char>(*c), lsl::ksDown, false);
		return true;

	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
	{
		lsl::MouseKey key;
		if (event.button.button == SDL_BUTTON_LEFT)
			key = lsl::mkLeft;
		else if (event.button.button == SDL_BUTTON_RIGHT)
			key = lsl::mkRight;
		else if (event.button.button == SDL_BUTTON_MIDDLE)
			key = lsl::mkMiddle;
		else
			return true;

		const SDL_Keymod mod = SDL_GetModState();
		view->OnMouseClickEvent(key,
		                        event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
		                            ? lsl::ksDown : lsl::ksUp,
		                        MouseCoord(event.button.x, event.button.y),
		                        ShiftHeld(mod), ControlHeld(mod));
		return true;
	}

	case SDL_EVENT_MOUSE_MOTION:
	{
		const SDL_Keymod mod = SDL_GetModState();
		view->OnMouseMoveEvent(MouseCoord(event.motion.x, event.motion.y),
		                       ShiftHeld(mod), ControlHeld(mod));
		return true;
	}

	default:
		return true;
	}
}

/*
 * Shaped like the Win32 loop, including the input-reset behaviour, because
 * that is engine state rather than a Win32 artefact: ResetInput exists so a
 * mode change does not inherit keys held down across it, and the three-frame
 * delay is the engine's own.
 */
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

		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_EVENT_QUIT)
			{
				/* Which way the loop ended, because all three look identical
				   from outside and the difference is the whole diagnosis. */
				LSL_LOG("MainLoop: SDL_EVENT_QUIT");
				return EXIT_SUCCESS;
			}

			/* Input is dropped while a reset is pending, exactly as the Win32
			   loop skipped every message but WM_SETCURSOR and WM_DESTROY. */
			if (inputWasReset)
				continue;

			if (!HandleEvent(event))
			{
				LSL_LOG("MainLoop: HandleEvent asked to stop");
				return EXIT_SUCCESS;
			}

			inputWasReset = inputWasReset || rock3dWorld->InputWasReset();
		}

		if (rock3dWorld->IsTerminate())
		{
			LSL_LOG(lsl::StrFmt("MainLoop: world terminated, result %d",
				rock3dWorld->GetTerminateResult()));
			return rock3dWorld->GetTerminateResult();
		}

		/* Rendered here rather than inside the event pump, so a slow frame
		   does not stall input. */
		rock3dWorld->MainProgress();
	}
}

void ErrMessage(const std::string& message)
{
	std::fprintf(stderr, "RRR3d error: %s\n", message.c_str());
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", message.c_str(),
	                         mainWindow);
}

} /* namespace */

int main(int, char**)
{
	int exitResult = EXIT_SUCCESS;

	SetCurrentDirectoryW(lsl::GetAppPath().c_str());
	lsl::appLog.Clear();
	lsl::appLog.Append("Init...");

	/* SDL_INIT_GAMEPAD here rather than later, because phase 11's XInput
	   replacement reads from the same subsystem and the shell is what owns it. */
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
	{
		std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return EXIT_FAILURE;
	}

	try
	{
		const SDL_WindowFlags flags =
			SDL_WINDOW_METAL |
			(cFullScreen ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_RESIZABLE);

		mainWindow = SDL_CreateWindow("Rock3D", cResolution.x, cResolution.y, flags);
		if (!mainWindow)
			throw lsl::Error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());

		/*
		 * One view, one layer, handed straight through. The engine treats
		 * desc.handle as an HWND and never dereferences it -- everything that
		 * would have asked Win32 about it now asks XPlatform instead.
		 */
		metalView = SDL_Metal_CreateView(mainWindow);
		if (!metalView)
			throw lsl::Error(std::string("SDL_Metal_CreateView failed: ") + SDL_GetError());

		metalLayer = SDL_Metal_GetLayer(metalView);
		if (!metalLayer)
			throw lsl::Error("SDL_Metal_GetLayer returned nothing");

		PublishClientSize();

		r3d::IView::Desc desc;
		desc.fullscreen = cFullScreen;
		desc.handle = static_cast<HWND>(metalLayer);
		desc.resolution = cFullScreen ? lsl::Point(0, 0) : cResolution;

		rock3dWorld = r3d::CreateWorld(desc, true);
		rock3dWorld->RunGame();

		lsl::appLog.Append("Run...");

		exitResult = MainLoop();

		lsl::appLog.Append("Terminate...");

		r3d::ReleaseWorld(rock3dWorld);
		rock3dWorld = NULL;
	}
	catch (const lsl::Error& err)
	{
		ErrMessage(err.what());
		exitResult = EXIT_FAILURE;
	}
	catch (const std::exception& err)
	{
		lsl::appLog << "stdError: " << err.what() << '\n';
		ErrMessage(err.what());
		exitResult = EXIT_FAILURE;
	}

	if (metalView)
		SDL_Metal_DestroyView(metalView);
	if (mainWindow)
		SDL_DestroyWindow(mainWindow);

	SDL_Quit();

	lsl::appLog.Append("Exit");
	lsl::FileSystem::Release();

	return exitResult;
}
