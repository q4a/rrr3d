#include "SdlInputManager.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>

namespace rrr3d::input
{
namespace
{

void appendDigital(std::vector<ActionEvent> &events, Action action, bool active, bool repeated, Source source,
                   SDL_JoystickID device_id = 0)
{
	events.push_back({action, active ? 1.0F : 0.0F, active, repeated, source, device_id});
}

constexpr std::array<Action, 32> allActions = {
	Action::Accelerate, Action::Brake, Action::TurnLeft, Action::TurnRight,
	Action::UseWeapon, Action::UseAllWeapons, Action::UseMine,
	Action::UseHyper, Action::ChangeWeapon, Action::PreviousWeapon,
	Action::NextWeapon,
	Action::SelectWeapon1, Action::SelectWeapon2, Action::SelectWeapon3, Action::SelectWeapon4,
	Action::ToggleCamera, Action::ResetVehicle, Action::Pause, Action::MenuUp,
	Action::MenuDown, Action::MenuConfirm, Action::MenuBack,
	Action::Debug1, Action::Debug2, Action::Debug3, Action::Debug4,
	Action::Debug5, Action::Debug6, Action::Debug7, Action::DebugOverlay,
	Action::DebugPagePrevious, Action::DebugPageNext};
} // namespace

std::optional<std::string> originalKeyboardBindingName(
    SDL_Scancode scancode)
{
	switch (scancode)
	{
	case SDL_SCANCODE_LEFT:
		return "Left Arrow";
	case SDL_SCANCODE_RIGHT:
		return "Right Arrow";
	case SDL_SCANCODE_UP:
		return "Up Arrow";
	case SDL_SCANCODE_DOWN:
		return "Down Arrow";
	case SDL_SCANCODE_BACKSPACE:
		return "X";
	case SDL_SCANCODE_SPACE:
		return "Space";
	case SDL_SCANCODE_ESCAPE:
		return "Escape";
	case SDL_SCANCODE_RETURN:
	case SDL_SCANCODE_KP_ENTER:
		return "Enter";
	case SDL_SCANCODE_F1:
		return "F1";
	case SDL_SCANCODE_F2:
		return "F2";
	case SDL_SCANCODE_F3:
		return "F3";
	case SDL_SCANCODE_F4:
		return "F4";
	case SDL_SCANCODE_F5:
		return "F5";
	case SDL_SCANCODE_F6:
		return "F6";
	case SDL_SCANCODE_F7:
		return "F7";
	default:
		break;
	}
	const std::string name = SDL_GetScancodeName(scancode);
	if (name.size() == 1U &&
	    ((name.front() >= 'A' && name.front() <= 'Z') ||
	     (name.front() >= '0' && name.front() <= '9')))
	{
		return name;
	}
	return std::nullopt;
}

std::optional<std::string> originalGamepadButtonBindingName(
    SDL_GamepadButton button)
{
	switch (button)
	{
	case SDL_GAMEPAD_BUTTON_SOUTH:
		return "A";
	case SDL_GAMEPAD_BUTTON_EAST:
		return "B";
	case SDL_GAMEPAD_BUTTON_WEST:
		return "X";
	case SDL_GAMEPAD_BUTTON_NORTH:
		return "Y";
	case SDL_GAMEPAD_BUTTON_DPAD_UP:
		return "DPad Up";
	case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
		return "DPad Down";
	case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
		return "DPad Left";
	case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
		return "DPad Right";
	case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
		return "Left Shoulder";
	case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
		return "Right Shoulder";
	case SDL_GAMEPAD_BUTTON_LEFT_STICK:
		return "L.Thumb Press";
	case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
		return "R.Thumb Press";
	case SDL_GAMEPAD_BUTTON_BACK:
		return "Back";
	case SDL_GAMEPAD_BUTTON_START:
		return "Start";
	default:
		return std::nullopt;
	}
}

std::optional<std::string> originalGamepadAxisBindingName(
    SDL_GamepadAxis axis, Sint16 value)
{
	constexpr Sint16 triggerThreshold =
	    static_cast<Sint16>((30 * 32767) / 255);
	if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER &&
	    value > triggerThreshold)
		return "Left Trigger";
	if (axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER &&
	    value > triggerThreshold)
		return "Right Trigger";
	const int threshold =
	    axis == SDL_GAMEPAD_AXIS_RIGHTX ||
	            axis == SDL_GAMEPAD_AXIS_RIGHTY
	        ? 8689
	        : 7849;
	if (std::abs(static_cast<int>(value)) <= threshold)
		return std::nullopt;
	switch (axis)
	{
	case SDL_GAMEPAD_AXIS_LEFTX:
		return value < 0 ? "L.Thumb Left" : "L.Thumb Right";
	case SDL_GAMEPAD_AXIS_LEFTY:
		return value < 0 ? "L.Thumb Up" : "L.Thumb Down";
	case SDL_GAMEPAD_AXIS_RIGHTX:
		return value < 0 ? "R.Thumb Left" : "R.Thumb Right";
	case SDL_GAMEPAD_AXIS_RIGHTY:
		return value < 0 ? "R.Thumb Up" : "R.Thumb Down";
	default:
		return std::nullopt;
	}
}

SdlInputManager::~SdlInputManager()
{
	shutdown();
}

bool SdlInputManager::initialize(std::string &error)
{
	if (initialized_)
		return true;

	int count = 0;
	SDL_JoystickID *device_ids = SDL_GetGamepads(&count);
	if (device_ids == nullptr)
	{
		error = "Unable to enumerate SDL gamepads: ";
		error += SDL_GetError();
		return false;
	}

	initialized_ = true;
	applyKeyboardBindings(
	    r3d::game::originalcontrol::makeDefaultBindings(
	        r3d::game::originalcontrol::ControllerType::Keyboard));
	applyGamepadBindings(
	    r3d::game::originalcontrol::makeDefaultBindings(
	        r3d::game::originalcontrol::ControllerType::Gamepad));
	for (int index = 0; index < count; ++index)
		openGamepad(device_ids[index]);
	SDL_free(device_ids);
	error.clear();
	return true;
}

void SdlInputManager::shutdown() noexcept
{
	for (auto &[device_id, state] : gamepads_)
	{
		static_cast<void>(device_id);
		SDL_CloseGamepad(state.handle);
	}
	gamepads_.clear();
	control_.ResetInput();
	initialized_ = false;
}

void SdlInputManager::applyKeyboardBindings(
    const std::map<std::string, std::string> &bindings)
{
	control_.ApplyBindings(
	    r3d::game::originalcontrol::ControllerType::Keyboard, bindings);
}

void SdlInputManager::applyGamepadBindings(
    const std::map<std::string, std::string> &bindings)
{
	control_.ApplyBindings(
	    r3d::game::originalcontrol::ControllerType::Gamepad, bindings);
}

bool SdlInputManager::openGamepad(SDL_JoystickID device_id) noexcept
{
	if (gamepads_.find(device_id) != gamepads_.end())
		return true;

	SDL_Gamepad *gamepad = SDL_OpenGamepad(device_id);
	if (gamepad == nullptr)
	{
		SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "Unable to open gamepad %d: %s", static_cast<int>(device_id),
		            SDL_GetError());
		return false;
	}

	const char *name = SDL_GetGamepadName(gamepad);
	gamepads_.emplace(device_id, GamepadState{gamepad});
	SDL_Log("Gamepad connected: id=%d name='%s'", static_cast<int>(device_id), name == nullptr ? "Unknown" : name);
	return true;
}

void SdlInputManager::closeGamepad(SDL_JoystickID device_id) noexcept
{
	const auto entry = gamepads_.find(device_id);
	if (entry == gamepads_.end())
		return;

	SDL_CloseGamepad(entry->second.handle);
	gamepads_.erase(entry);
	SDL_Log("Gamepad disconnected: id=%d", static_cast<int>(device_id));
}

void SdlInputManager::appendGamepadReleases(std::vector<ActionEvent> &events, SDL_JoystickID device_id) const
{
	for (const Action action : allActions)
		appendDigital(events, action, false, false, Source::System, device_id);
}

std::vector<ActionEvent> SdlInputManager::processEvent(const SDL_Event &event)
{
	std::vector<ActionEvent> events;
	const auto appendSourceEvents = [&](std::vector<ActionEvent> sourceEvents) {
		events.insert(events.end(), sourceEvents.begin(), sourceEvents.end());
	};
	switch (event.type)
	{
	case SDL_EVENT_GAMEPAD_ADDED:
		openGamepad(event.gdevice.which);
		break;

	case SDL_EVENT_GAMEPAD_REMOVED:
		appendGamepadReleases(events, event.gdevice.which);
		closeGamepad(event.gdevice.which);
		break;

	case SDL_EVENT_WINDOW_FOCUS_LOST:
		for (const Action action : allActions)
			appendDigital(events, action, false, false, Source::System);
		break;

	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP: {
		const bool down = event.key.down;
		const bool repeat = event.key.repeat;
		// Menu.cpp navigates by raw arrow/start virtual keys, while gameplay
		// receives only actions from ControlManager::_gameKeys.
		switch (event.key.scancode)
		{
		case SDL_SCANCODE_UP:
			appendDigital(events, Action::MenuUp, down, repeat,
			              Source::Keyboard);
			break;
		case SDL_SCANCODE_DOWN:
			appendDigital(events, Action::MenuDown, down, repeat,
			              Source::Keyboard);
			break;
		case SDL_SCANCODE_RETURN:
		case SDL_SCANCODE_KP_ENTER:
			appendDigital(events, Action::MenuConfirm, down, repeat,
			              Source::Keyboard);
			break;
		case SDL_SCANCODE_F10:
			appendDigital(events, Action::DebugOverlay, down, repeat,
			              Source::Keyboard);
			break;
		case SDL_SCANCODE_PAGEUP:
			appendDigital(events, Action::DebugPagePrevious, down, repeat,
			              Source::Keyboard);
			break;
		case SDL_SCANCODE_PAGEDOWN:
			appendDigital(events, Action::DebugPageNext, down, repeat,
			              Source::Keyboard);
			break;
		default:
			break;
		}
		// A/B are unreachable keyboard VirtualKey table entries in the source;
		// X is VK_BACK, not the X character. Preserve that collision exactly.
		const bool sourceCharacterCollision =
		    event.key.scancode == SDL_SCANCODE_A ||
		    event.key.scancode == SDL_SCANCODE_B ||
		    event.key.scancode == SDL_SCANCODE_X;
		if (!sourceCharacterCollision)
		{
			if (const auto key = originalKeyboardBindingName(
			        event.key.scancode))
			{
				appendSourceEvents(control_.OnVirtualKey(
				    r3d::game::originalcontrol::ControllerType::Keyboard,
				    *key, down ? 1 : 0, repeat, Source::Keyboard));
			}
		}
		break;
	}

	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
		if (event.button.button == SDL_BUTTON_LEFT)
		{
			appendDigital(events, Action::MenuConfirm, event.button.down, false, Source::Mouse);
		}
		break;

	case SDL_EVENT_MOUSE_WHEEL:
		// Win32 World does not convert WM_MOUSEWHEEL into a GameAction.
		break;

	case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
	case SDL_EVENT_GAMEPAD_BUTTON_UP:
		if (!hasGamepad(event.gbutton.which))
			break;
		switch (static_cast<SDL_GamepadButton>(event.gbutton.button))
		{
		case SDL_GAMEPAD_BUTTON_DPAD_UP:
			appendDigital(events, Action::MenuUp, event.gbutton.down, false, Source::GamepadButton,
			              event.gbutton.which);
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
			appendDigital(events, Action::MenuDown, event.gbutton.down, false, Source::GamepadButton,
			              event.gbutton.which);
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
		case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
			break;
		default:
			break;
		}
		if (const auto key = originalGamepadButtonBindingName(
		        static_cast<SDL_GamepadButton>(event.gbutton.button)))
		{
			appendSourceEvents(control_.OnVirtualKey(
			    r3d::game::originalcontrol::ControllerType::Gamepad,
			    *key, event.gbutton.down ? 1 : 0, false,
			    Source::GamepadButton, event.gbutton.which));
		}
		break;

	case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
		const auto entry = gamepads_.find(event.gaxis.which);
		if (entry == gamepads_.end())
			break;

		const auto axis = static_cast<SDL_GamepadAxis>(event.gaxis.axis);
		if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ||
		    axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
		{
			const int sourceValue = std::clamp(
			    static_cast<int>(std::lround(
			        static_cast<float>(std::max<Sint16>(
			            event.gaxis.value, 0)) * 255.0F / 32767.0F)),
			    0, 255);
			appendSourceEvents(control_.OnVirtualKey(
			    r3d::game::originalcontrol::ControllerType::Gamepad,
			    axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER
			        ? "Left Trigger"
			        : "Right Trigger",
			    sourceValue, false, Source::GamepadAxis,
			    event.gaxis.which));
			break;
		}

		const char* fullKey = nullptr;
		const char* negativeKey = nullptr;
		const char* positiveKey = nullptr;
		int activationThreshold = 7849;
		switch (axis)
		{
		case SDL_GAMEPAD_AXIS_LEFTX:
			fullKey = "L.Thumb Move X";
			negativeKey = "L.Thumb Left";
			positiveKey = "L.Thumb Right";
			break;
		case SDL_GAMEPAD_AXIS_LEFTY:
			fullKey = "L.Thumb Move Y";
			negativeKey = "L.Thumb Up";
			positiveKey = "L.Thumb Down";
			break;
		case SDL_GAMEPAD_AXIS_RIGHTX:
			fullKey = "R.Thumb Move X";
			negativeKey = "R.Thumb Left";
			positiveKey = "R.Thumb Right";
			activationThreshold = 8689;
			break;
		case SDL_GAMEPAD_AXIS_RIGHTY:
			fullKey = "R.Thumb Move Y";
			negativeKey = "R.Thumb Up";
			positiveKey = "R.Thumb Down";
			activationThreshold = 8689;
			break;
		default:
			break;
		}
		if (fullKey == nullptr)
			break;

		const int raw = static_cast<int>(event.gaxis.value);
		appendSourceEvents(control_.OnVirtualKey(
		    r3d::game::originalcontrol::ControllerType::Gamepad,
		    fullKey, raw, false, Source::GamepadAxis,
		    event.gaxis.which));
		appendSourceEvents(control_.OnVirtualKey(
		    r3d::game::originalcontrol::ControllerType::Gamepad,
		    negativeKey,
		    raw < -activationThreshold ? raw : 0,
		    false, Source::GamepadAxis, event.gaxis.which));
		appendSourceEvents(control_.OnVirtualKey(
		    r3d::game::originalcontrol::ControllerType::Gamepad,
		    positiveKey,
		    raw > activationThreshold ? raw : 0,
		    false, Source::GamepadAxis, event.gaxis.which));
		break;
	}

	default:
		break;
	}
	std::vector<ActionEvent> unique;
	unique.reserve(events.size());
	for (const auto &eventValue : events)
	{
		const auto found = std::find_if(
		    unique.begin(), unique.end(),
		    [&](const ActionEvent &value) {
			    return value.action == eventValue.action &&
			           value.source == eventValue.source &&
			           value.device_id == eventValue.device_id;
		    });
		if (found == unique.end())
			unique.push_back(eventValue);
		else
			*found = eventValue;
	}
	if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
	{
		control_.ResetInput();
	}
	else
	{
		if (event.type == SDL_EVENT_GAMEPAD_REMOVED)
			control_.ClearHeldDevice(event.gdevice.which);
		for (const auto &eventValue : unique)
			control_.UpdateActionState(eventValue);
	}
	return unique;
}

float SdlInputManager::heldValue(Action action) const noexcept
{
	return control_.HeldValue(action);
}

float SdlInputManager::heldValue(Action action, Source source) const noexcept
{
	return control_.HeldValue(action, source);
}

std::size_t SdlInputManager::connectedGamepadCount() const noexcept
{
	return gamepads_.size();
}

bool SdlInputManager::hasGamepad(SDL_JoystickID device_id) const noexcept
{
	return gamepads_.find(device_id) != gamepads_.end();
}

bool SdlInputManager::rumble(SDL_JoystickID device_id, float low_frequency, float high_frequency,
                             std::uint32_t duration_ms) noexcept
{
	const auto entry = gamepads_.find(device_id);
	if (entry == gamepads_.end())
		return false;

	const auto toMagnitude = [](float value) {
		const float clamped = std::clamp(value, 0.0F, 1.0F);
		return static_cast<Uint16>(std::lround(clamped * std::numeric_limits<Uint16>::max()));
	};
	return SDL_RumbleGamepad(entry->second.handle, toMagnitude(low_frequency), toMagnitude(high_frequency),
	                         duration_ms);
}

} // namespace rrr3d::input
