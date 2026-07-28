/*
 * The SDL window and event loop: what _tWinMain and MainWndProc do on Windows.
 *
 * Included from RRR3d.cpp in place of the Win32 shell, not compiled on its own,
 * so it shares that file's anonymous namespace and its OnKeyEvent /
 * OnMouseClickEvent forwarders. Keeping it beside them rather than duplicating
 * them is the point: the shell differs, what it forwards to does not.
 *
 * Two things this deliberately does not do.
 *
 * It does not translate SDL events into Win32 messages. The engine's entry
 * points -- OnKeyEvent, OnMouseMoveEvent, MainProgress -- are already plain
 * calls; MainWndProc only exists because Win32 insists on delivering input as
 * messages. Reconstructing MSG structures to feed a loop we also wrote would be
 * ceremony around a function call.
 *
 * It does not own the Metal layer. desc.handle carries the NSWindow* through
 * the engine to CreateMetalViewFromHWND, which decides what to do with it --
 * see src/MetalBridge/source/metalbridge_present.mm. That keeps SDL out of the
 * graphics backend and the backend out of here.
 */

#include <SDL3/SDL.h>

namespace
{

SDL_Window* sdlWindow;

/*
 * SDL scancodes to the Win32 virtual-key codes the engine speaks.
 *
 * Scancodes, not keycodes: these are positions on the keyboard, so WASD stays
 * where the player's fingers are on an AZERTY or Dvorak layout. The engine
 * treats these as physical bindings -- ControlManager maps them to accelerate,
 * brake, fire -- so position is the right thing to preserve. Text entry goes
 * through SDL_EVENT_TEXT_INPUT instead, which is layout-aware, and that is the
 * split OnKeyEvent and OnKeyChar already assume.
 *
 * Only the keys the game binds are here. Anything else returns 0 and is
 * dropped, rather than being forwarded as a code that means something else.
 */
unsigned VirtualKeyFromScancode(SDL_Scancode code)
{
	switch (code)
	{
	/* Letters. VK_A..VK_Z are the ASCII values, which is a Win32 guarantee. */
	case SDL_SCANCODE_A: return 'A';
	case SDL_SCANCODE_B: return 'B';
	case SDL_SCANCODE_C: return 'C';
	case SDL_SCANCODE_D: return 'D';
	case SDL_SCANCODE_E: return 'E';
	case SDL_SCANCODE_F: return 'F';
	case SDL_SCANCODE_G: return 'G';
	case SDL_SCANCODE_H: return 'H';
	case SDL_SCANCODE_I: return 'I';
	case SDL_SCANCODE_J: return 'J';
	case SDL_SCANCODE_K: return 'K';
	case SDL_SCANCODE_L: return 'L';
	case SDL_SCANCODE_M: return 'M';
	case SDL_SCANCODE_N: return 'N';
	case SDL_SCANCODE_O: return 'O';
	case SDL_SCANCODE_P: return 'P';
	case SDL_SCANCODE_Q: return 'Q';
	case SDL_SCANCODE_R: return 'R';
	case SDL_SCANCODE_S: return 'S';
	case SDL_SCANCODE_T: return 'T';
	case SDL_SCANCODE_U: return 'U';
	case SDL_SCANCODE_V: return 'V';
	case SDL_SCANCODE_W: return 'W';
	case SDL_SCANCODE_X: return 'X';
	case SDL_SCANCODE_Y: return 'Y';
	case SDL_SCANCODE_Z: return 'Z';

	case SDL_SCANCODE_0: return '0';
	case SDL_SCANCODE_1: return '1';
	case SDL_SCANCODE_2: return '2';
	case SDL_SCANCODE_3: return '3';
	case SDL_SCANCODE_4: return '4';
	case SDL_SCANCODE_5: return '5';
	case SDL_SCANCODE_6: return '6';
	case SDL_SCANCODE_7: return '7';
	case SDL_SCANCODE_8: return '8';
	case SDL_SCANCODE_9: return '9';

	case SDL_SCANCODE_RETURN:    return VK_RETURN;
	case SDL_SCANCODE_ESCAPE:    return VK_ESCAPE;
	case SDL_SCANCODE_BACKSPACE: return VK_BACK;
	case SDL_SCANCODE_TAB:       return VK_TAB;
	case SDL_SCANCODE_SPACE:     return VK_SPACE;
	case SDL_SCANCODE_DELETE:    return VK_DELETE;
	case SDL_SCANCODE_INSERT:    return VK_INSERT;
	case SDL_SCANCODE_HOME:      return VK_HOME;
	case SDL_SCANCODE_END:       return VK_END;
	case SDL_SCANCODE_PAGEUP:    return VK_PRIOR;
	case SDL_SCANCODE_PAGEDOWN:  return VK_NEXT;

	case SDL_SCANCODE_LEFT:  return VK_LEFT;
	case SDL_SCANCODE_RIGHT: return VK_RIGHT;
	case SDL_SCANCODE_UP:    return VK_UP;
	case SDL_SCANCODE_DOWN:  return VK_DOWN;

	case SDL_SCANCODE_LSHIFT:
	case SDL_SCANCODE_RSHIFT: return VK_SHIFT;
	case SDL_SCANCODE_LCTRL:
	case SDL_SCANCODE_RCTRL:  return VK_CONTROL;
	case SDL_SCANCODE_LALT:
	case SDL_SCANCODE_RALT:   return VK_MENU;

	case SDL_SCANCODE_F1:  return VK_F1;
	case SDL_SCANCODE_F2:  return VK_F2;
	case SDL_SCANCODE_F3:  return VK_F3;
	case SDL_SCANCODE_F4:  return VK_F4;
	case SDL_SCANCODE_F5:  return VK_F5;
	case SDL_SCANCODE_F6:  return VK_F6;
	case SDL_SCANCODE_F7:  return VK_F7;
	case SDL_SCANCODE_F8:  return VK_F8;
	case SDL_SCANCODE_F9:  return VK_F9;
	case SDL_SCANCODE_F10: return VK_F10;
	case SDL_SCANCODE_F11: return VK_F11;
	case SDL_SCANCODE_F12: return VK_F12;

	case SDL_SCANCODE_KP_0: return VK_NUMPAD0;
	case SDL_SCANCODE_KP_1: return VK_NUMPAD1;
	case SDL_SCANCODE_KP_2: return VK_NUMPAD2;
	case SDL_SCANCODE_KP_3: return VK_NUMPAD3;
	case SDL_SCANCODE_KP_4: return VK_NUMPAD4;
	case SDL_SCANCODE_KP_5: return VK_NUMPAD5;
	case SDL_SCANCODE_KP_6: return VK_NUMPAD6;
	case SDL_SCANCODE_KP_7: return VK_NUMPAD7;
	case SDL_SCANCODE_KP_8: return VK_NUMPAD8;
	case SDL_SCANCODE_KP_9: return VK_NUMPAD9;
	case SDL_SCANCODE_KP_PLUS:   return VK_ADD;
	case SDL_SCANCODE_KP_MINUS:  return VK_SUBTRACT;
	case SDL_SCANCODE_PERIOD:    return VK_OEM_PERIOD;

	default: return 0;
	}
}

lsl::MouseKey MouseKeyFromSDL(Uint8 button, bool& known)
{
	known = true;
	switch (button)
	{
	case SDL_BUTTON_LEFT:   return lsl::mkLeft;
	case SDL_BUTTON_RIGHT:  return lsl::mkRight;
	case SDL_BUTTON_MIDDLE: return lsl::mkMiddle;
	default:
		known = false;
		return lsl::mkLeft;
	}
}

bool ShiftHeld()
{
	return (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
}

bool CtrlHeld()
{
	return (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
}

SDL_MetalView sdlMetalView;

/*
 * The CAMetalLayer the graphics backend should render into.
 *
 * SDL makes the view and the layer; we hand the layer over rather than making
 * our own. The alternative -- attaching a second layer to the window's content
 * view -- puts two CAMetalLayers in the same view, and the one being drawn is
 * not the one composited, which shows up as a running process with a window
 * that never appears.
 *
 * Returns null if this is not a Metal build, and the backend then makes a
 * detached layer: the headless path it had before this shell existed.
 */
HWND MetalLayerHandle(SDL_Window* window)
{
	sdlMetalView = SDL_Metal_CreateView(window);
	if (!sdlMetalView)
	{
		std::fprintf(stderr, "rrr3d: SDL_Metal_CreateView failed: %s\n", SDL_GetError());
		return 0;
	}

	/*
	 * The engine scales mouse input by the client size, which it asks for
	 * through GetClientRect -- and nothing off Windows can answer that from a
	 * handle. Publishing it here is what makes clicks land where they are seen.
	 */
	int width = 0;
	int height = 0;
	SDL_GetWindowSize(window, &width, &height);

	/*
	 * The game draws its own cursor -- see "menu create cursor" in the log --
	 * so the system one is hidden, as MainWndProc does with SetCursor(0). With
	 * both visible the drawn cursor trails the real one by a frame, which reads
	 * as input lag rather than as two cursors.
	 */
	SDL_HideCursor();

	HWND handle = reinterpret_cast<HWND>(SDL_Metal_GetLayer(sdlMetalView));
	RegisterClientSize(handle, width, height);

	return handle;
}

/*
 * Drains the event queue, forwarding to the same handlers MainWndProc uses.
 * Returns false when the game should stop.
 *
 * The input-reset dance mirrors MainLoop's: the engine asks for input to be
 * dropped for a few frames after a mode change, so events are pumped -- SDL
 * requires that or the window stops responding -- and then discarded.
 */
bool PumpEvents(r3d::IWorld* world, bool inputWasReset)
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		if (event.type == SDL_EVENT_QUIT ||
			event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
			return false;

		if (inputWasReset)
			continue;

		switch (event.type)
		{
		case SDL_EVENT_KEY_DOWN:
		{
			const unsigned key = VirtualKeyFromScancode(event.key.scancode);
			if (key)
				OnKeyEvent(key, lsl::ksDown, event.key.repeat != 0);
			break;
		}

		case SDL_EVENT_KEY_UP:
		{
			const unsigned key = VirtualKeyFromScancode(event.key.scancode);
			if (key)
				OnKeyEvent(key, lsl::ksUp, false);
			break;
		}

		case SDL_EVENT_TEXT_INPUT:
		{
			/*
			 * WM_CHAR's counterpart: characters as typed, after the layout and
			 * any dead keys. Sent as UTF-8, and the engine's text fields are
			 * byte-oriented, so each byte goes through -- which is right for
			 * ASCII and is what the Win32 path delivered too.
			 */
			for (const char* c = event.text.text; c && *c; ++c)
				OnKeyChar(static_cast<unsigned char>(*c), lsl::ksDown, false);
			break;
		}

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
		{
			bool known = false;
			const lsl::MouseKey key = MouseKeyFromSDL(event.button.button, known);
			if (!known)
				break;

			const lsl::KeyState state =
				event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? lsl::ksDown : lsl::ksUp;

			OnMouseClickEvent(key, state,
				lsl::Point(static_cast<int>(event.button.x), static_cast<int>(event.button.y)),
				ShiftHeld(), CtrlHeld());
			break;
		}

		case SDL_EVENT_MOUSE_MOTION:
			OnMouseMoveEvent(
				lsl::Point(static_cast<int>(event.motion.x), static_cast<int>(event.motion.y)),
				ShiftHeld(), CtrlHeld());
			break;

		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		{
			int width = 0;
			int height = 0;
			SDL_GetWindowSize(sdlWindow, &width, &height);
			RegisterClientSize(0, width, height);

			if (world)
				world->OnDisplayChange();
			break;
		}
		}
	}

	return true;
}

/* MainLoop's counterpart. Same shape: pump, check for termination, render. */
int MainLoop(r3d::IWorld* world)
{
	int resetInputFrames = 0;

	while (true)
	{
		bool inputWasReset = world->InputWasReset();

		if (inputWasReset && (++resetInputFrames) >= 3)
		{
			resetInputFrames = 0;
			world->ResetInput(false);
		}

		if (!PumpEvents(world, inputWasReset))
			return EXIT_SUCCESS;

		if (world->IsTerminate())
			return world->GetTerminateResult();

		/* Rendered after the pump so input is never blocked behind a frame. */
		world->MainProgress();
	}
}

}
