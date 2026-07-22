#include "SdlInputSmoke.h"

#include "SdlInputManager.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace rrr3d::input
{
namespace
{

struct VirtualRumbleState
{
	bool called = false;
	Uint16 low = 0;
	Uint16 high = 0;
};

bool SDLCALL virtualRumble(void *userdata, Uint16 low, Uint16 high)
{
	auto &state = *static_cast<VirtualRumbleState *>(userdata);
	state.called = true;
	state.low = low;
	state.high = high;
	return true;
}

std::vector<ActionEvent> drainInputEvents(SdlInputManager &input)
{
	std::vector<ActionEvent> actions;
	SDL_PumpEvents();
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		auto translated = input.processEvent(event);
		actions.insert(actions.end(), translated.begin(), translated.end());
	}
	return actions;
}

bool contains(const std::vector<ActionEvent> &events, Action action, Source source, bool active,
              SDL_JoystickID device_id = 0)
{
	return std::any_of(events.begin(), events.end(), [&](const ActionEvent &event) {
		return event.action == action && event.source == source && event.active == active &&
		       (device_id == 0 || event.device_id == device_id);
	});
}

void updateVirtualInput(SdlInputManager &input, std::vector<ActionEvent> &actions)
{
	actions.clear();
	for (int attempt = 0; attempt < 3; ++attempt)
	{
		SDL_UpdateJoysticks();
		auto batch = drainInputEvents(input);
		actions.insert(actions.end(), batch.begin(), batch.end());
		if (attempt != 2)
			SDL_Delay(1);
	}
}

std::vector<ActionEvent> processAxis(SdlInputManager &input, SDL_JoystickID device_id, SDL_GamepadAxis axis,
                                     Sint16 value)
{
	SDL_Event event{};
	event.gaxis.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
	event.gaxis.which = device_id;
	event.gaxis.axis = static_cast<Uint8>(axis);
	event.gaxis.value = value;
	return input.processEvent(event);
}

std::vector<ActionEvent> processButton(SdlInputManager &input, SDL_JoystickID device_id, SDL_GamepadButton button,
                                       bool down)
{
	SDL_Event event{};
	event.gbutton.type = down ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
	event.gbutton.which = device_id;
	event.gbutton.button = static_cast<Uint8>(button);
	event.gbutton.down = down;
	return input.processEvent(event);
}

template <typename Predicate>
bool waitForAxis(SDL_Gamepad *gamepad, SDL_GamepadAxis axis, Sint16 &observed, Predicate matches)
{
	for (int attempt = 0; attempt < 50; ++attempt)
	{
		SDL_UpdateJoysticks();
		observed = SDL_GetGamepadAxis(gamepad, axis);
		if (matches(observed))
			return true;
		SDL_Delay(1);
	}
	return false;
}

bool waitForButton(SDL_Gamepad *gamepad, SDL_GamepadButton button, bool expected)
{
	for (int attempt = 0; attempt < 50; ++attempt)
	{
		SDL_UpdateJoysticks();
		if (SDL_GetGamepadButton(gamepad, button) == expected)
			return true;
		SDL_Delay(1);
	}
	return false;
}

} // namespace

bool runSdlInputSmokeTest(SdlInputManager &input, std::string &error)
{
	SDL_Event event{};
	event.key.type = SDL_EVENT_KEY_DOWN;
	event.key.down = true;
	event.key.scancode = SDL_SCANCODE_DOWN;
	auto actions = input.processEvent(event);
	if (!contains(actions, Action::MenuDown, Source::Keyboard, true))
	{
		error = "keyboard action mapping failed";
		return false;
	}

	event = {};
	event.wheel.type = SDL_EVENT_MOUSE_WHEEL;
	event.wheel.y = 1.0F;
	event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
	actions = input.processEvent(event);
	if (!contains(actions, Action::MenuUp, Source::Mouse, true))
	{
		error = "mouse wheel action mapping failed";
		return false;
	}

	event = {};
	event.button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
	event.button.button = SDL_BUTTON_LEFT;
	event.button.down = true;
	actions = input.processEvent(event);
	if (!contains(actions, Action::MenuConfirm, Source::Mouse, true))
	{
		error = "mouse button action mapping failed";
		return false;
	}

	event = {};
	event.window.type = SDL_EVENT_WINDOW_FOCUS_LOST;
	actions = input.processEvent(event);
	if (!contains(actions, Action::MenuConfirm, Source::System, false))
	{
		error = "focus-loss action reset failed";
		return false;
	}

	const std::size_t initial_gamepads = input.connectedGamepadCount();
	const char *previous_background_hint = SDL_GetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS);
	const bool had_background_hint = previous_background_hint != nullptr;
	const std::string saved_background_hint = had_background_hint ? previous_background_hint : "";
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
	VirtualRumbleState rumble_state;
	SDL_VirtualJoystickDesc description;
	SDL_INIT_INTERFACE(&description);
	description.type = SDL_JOYSTICK_TYPE_GAMEPAD;
	description.naxes = SDL_GAMEPAD_AXIS_COUNT;
	description.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
	description.name = "RRR3D Milestone 7 Virtual Gamepad";
	description.userdata = &rumble_state;
	description.Rumble = &virtualRumble;

	const SDL_JoystickID virtual_id = SDL_AttachVirtualJoystick(&description);
	if (virtual_id == 0)
	{
		if (had_background_hint)
			SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, saved_background_hint.c_str());
		else
			SDL_ResetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS);
		error = "unable to attach virtual SDL gamepad: ";
		error += SDL_GetError();
		return false;
	}

	SDL_Joystick *joystick = nullptr;
	const auto cleanup = [&]() {
		if (joystick != nullptr)
		{
			SDL_CloseJoystick(joystick);
			joystick = nullptr;
		}
		if (SDL_IsJoystickVirtual(virtual_id))
			SDL_DetachVirtualJoystick(virtual_id);
		SDL_UpdateJoysticks();
		drainInputEvents(input);
		if (had_background_hint)
			SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, saved_background_hint.c_str());
		else
			SDL_ResetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS);
	};
	const auto fail = [&](std::string message) {
		error = std::move(message);
		cleanup();
		return false;
	};

	updateVirtualInput(input, actions);
	if (!input.hasGamepad(virtual_id) || input.connectedGamepadCount() != initial_gamepads + 1)
	{
		return fail("virtual gamepad hot-plug was not detected");
	}

	joystick = SDL_OpenJoystick(virtual_id);
	if (joystick == nullptr)
	{
		return fail(std::string("unable to open virtual joystick: ") + SDL_GetError());
	}
	SDL_Gamepad *gamepad = SDL_GetGamepadFromID(virtual_id);
	if (gamepad == nullptr)
		return fail("unable to get the opened virtual gamepad");

	if (!SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTY, 7000))
	{
		return fail(std::string("unable to set virtual stick: ") + SDL_GetError());
	}
	Sint16 inside_dead_zone = 0;
	if (!waitForAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY, inside_dead_zone,
	                 [](Sint16 value) { return value >= 6000 && value <= 8000; }))
	{
		return fail("virtual stick did not reach the requested dead-zone value");
	}
	actions = processAxis(input, virtual_id, SDL_GAMEPAD_AXIS_LEFTY, inside_dead_zone);
	if (contains(actions, Action::MenuDown, Source::GamepadAxis, true, virtual_id))
	{
		return fail("left-stick dead zone admitted a menu action");
	}
	// SDL 3.4 can defer the next virtual-axis state until the event generated
	// for the previous state has been consumed.
	drainInputEvents(input);

	if (!SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTY, 24576))
	{
		return fail("unable to move virtual stick outside the dead zone");
	}
	Sint16 menu_down_value = 0;
	if (!waitForAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY, menu_down_value, [](Sint16 value) { return value >= 20000; }))
	{
		return fail("virtual stick did not reach the requested menu value; observed " +
		            std::to_string(menu_down_value));
	}
	actions = processAxis(input, virtual_id, SDL_GAMEPAD_AXIS_LEFTY, menu_down_value);
	if (!contains(actions, Action::MenuDown, Source::GamepadAxis, true, virtual_id))
	{
		return fail("left-stick menu action mapping failed");
	}

	SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTY, 0);
	Sint16 released_value = menu_down_value;
	if (!waitForAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY, released_value,
	                 [](Sint16 value) { return value > -100 && value < 100; }))
	{
		return fail("virtual stick did not return to center");
	}
	actions = processAxis(input, virtual_id, SDL_GAMEPAD_AXIS_LEFTY, released_value);
	if (!contains(actions, Action::MenuDown, Source::GamepadAxis, false, virtual_id))
	{
		return fail("left-stick hysteresis release failed");
	}

	SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTX, -24000);
	Sint16 steering_value = 0;
	if (!waitForAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX, steering_value, [](Sint16 value) { return value <= -20000; }))
	{
		return fail("virtual steering axis did not reach the requested value");
	}
	actions = processAxis(input, virtual_id, SDL_GAMEPAD_AXIS_LEFTX, steering_value);
	if (!contains(actions, Action::TurnLeft, Source::GamepadAxis, true, virtual_id))
	{
		return fail("analog steering action mapping failed");
	}

	SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 22000);
	Sint16 trigger_value = 0;
	if (!waitForAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, trigger_value,
	                 [](Sint16 value) { return value >= 18000; }))
	{
		return fail("virtual trigger did not reach the requested value");
	}
	actions = processAxis(input, virtual_id, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, trigger_value);
	if (!contains(actions, Action::Accelerate, Source::GamepadAxis, true, virtual_id))
	{
		return fail("analog trigger action mapping failed");
	}

	if (!SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_DPAD_UP, true))
	{
		return fail("unable to press virtual D-pad button");
	}
	if (!waitForButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP, true))
		return fail("virtual D-pad state was not visible through SDL Gamepad");
	actions = processButton(input, virtual_id, SDL_GAMEPAD_BUTTON_DPAD_UP, true);
	if (!contains(actions, Action::MenuUp, Source::GamepadButton, true, virtual_id))
	{
		return fail("gamepad D-pad action mapping failed");
	}
	SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_DPAD_UP, false);
	waitForButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP, false);
	processButton(input, virtual_id, SDL_GAMEPAD_BUTTON_DPAD_UP, false);

	SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true);
	if (!waitForButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH, true))
		return fail("virtual confirm state was not visible through SDL Gamepad");
	actions = processButton(input, virtual_id, SDL_GAMEPAD_BUTTON_SOUTH, true);
	if (!contains(actions, Action::MenuConfirm, Source::GamepadButton, true, virtual_id))
	{
		return fail("gamepad confirm action mapping failed");
	}
	SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, false);
	waitForButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH, false);
	processButton(input, virtual_id, SDL_GAMEPAD_BUTTON_SOUTH, false);

	SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_EAST, true);
	if (!waitForButton(gamepad, SDL_GAMEPAD_BUTTON_EAST, true))
		return fail("virtual back state was not visible through SDL Gamepad");
	actions = processButton(input, virtual_id, SDL_GAMEPAD_BUTTON_EAST, true);
	if (!contains(actions, Action::MenuBack, Source::GamepadButton, true, virtual_id))
	{
		return fail("gamepad back action mapping failed");
	}
	SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_EAST, false);
	waitForButton(gamepad, SDL_GAMEPAD_BUTTON_EAST, false);
	processButton(input, virtual_id, SDL_GAMEPAD_BUTTON_EAST, false);

	if (!input.rumble(virtual_id, 0.25F, 0.75F, 20) || !rumble_state.called || rumble_state.low == 0 ||
	    rumble_state.high <= rumble_state.low)
	{
		return fail("gamepad rumble dispatch failed");
	}

	SDL_CloseJoystick(joystick);
	joystick = nullptr;
	if (!SDL_DetachVirtualJoystick(virtual_id))
	{
		return fail(std::string("unable to detach virtual gamepad: ") + SDL_GetError());
	}
	updateVirtualInput(input, actions);
	if (input.hasGamepad(virtual_id) || input.connectedGamepadCount() != initial_gamepads)
	{
		return fail("virtual gamepad hot-unplug was not detected");
	}

	error.clear();
	return true;
}

} // namespace rrr3d::input
