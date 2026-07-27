#include "SdlInputManager.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <string_view>

namespace rrr3d::input
{
namespace
{

void appendDigital(std::vector<ActionEvent> &events, Action action, bool active, bool repeated, Source source,
                   SDL_JoystickID device_id = 0)
{
	events.push_back({action, active ? 1.0F : 0.0F, active, repeated, source, device_id});
}

void appendAnalog(std::vector<ActionEvent> &events, Action action, float value, Source source, SDL_JoystickID device_id)
{
	events.push_back({action, value, value > 0.0F, false, source, device_id});
}

float normalizeSignedAxis(Sint16 value) noexcept
{
	if (value < 0)
		return static_cast<float>(value) / 32768.0F;
	return static_cast<float>(value) / 32767.0F;
}

float applySignedDeadZone(float value, float dead_zone) noexcept
{
	const float magnitude = std::abs(value);
	if (magnitude <= dead_zone)
		return 0.0F;
	const float scaled = (magnitude - dead_zone) / (1.0F - dead_zone);
	return std::copysign(std::min(scaled, 1.0F), value);
}

float applyTriggerDeadZone(Sint16 value) noexcept
{
	const float normalized = std::clamp(static_cast<float>(value) / 32767.0F, 0.0F, 1.0F);
	if (normalized <= SdlInputManager::triggerDeadZone)
		return 0.0F;
	return (normalized - SdlInputManager::triggerDeadZone) / (1.0F - SdlInputManager::triggerDeadZone);
}

void appendDirectionalAxis(std::vector<ActionEvent> &events, float signed_value, SDL_JoystickID device_id)
{
	appendAnalog(events, Action::TurnLeft, std::max(-signed_value, 0.0F), Source::GamepadAxis, device_id);
	appendAnalog(events, Action::TurnRight, std::max(signed_value, 0.0F), Source::GamepadAxis, device_id);
}

constexpr std::array<Action, 19> allActions = {
	Action::Accelerate, Action::Brake, Action::TurnLeft, Action::TurnRight,
	Action::UseWeapon, Action::UseMine, Action::UseHyper, Action::ChangeWeapon,
	Action::SelectWeapon1, Action::SelectWeapon2, Action::SelectWeapon3, Action::SelectWeapon4,
	Action::ToggleCamera, Action::ResetVehicle, Action::Pause, Action::MenuUp,
	Action::MenuDown, Action::MenuConfirm, Action::MenuBack};

SDL_Scancode legacyScancode(const std::string &name) noexcept
{
	if (name == "None")
		return SDL_SCANCODE_UNKNOWN;
	if (name == "Up Arrow")
		return SDL_SCANCODE_UP;
	if (name == "Down Arrow")
		return SDL_SCANCODE_DOWN;
	if (name == "Left Arrow")
		return SDL_SCANCODE_LEFT;
	if (name == "Right Arrow")
		return SDL_SCANCODE_RIGHT;
	if (name == "Enter")
		return SDL_SCANCODE_RETURN;
	if (name == "Back")
		return SDL_SCANCODE_BACKSPACE;
	const auto scancode = SDL_GetScancodeFromName(name.c_str());
	return scancode;
}

std::optional<Action> gameAction(std::string_view name) noexcept
{
	if (name == "gaAccel")
		return Action::Accelerate;
	if (name == "gaBreak")
		return Action::Brake;
	if (name == "gaWheelLeft")
		return Action::TurnLeft;
	if (name == "gaWheelRight")
		return Action::TurnRight;
	if (name == "gaShot" || name == "gaShotAll")
		return Action::UseWeapon;
	if (name == "gaMine")
		return Action::UseMine;
	if (name == "gaHyper")
		return Action::UseHyper;
	if (name == "gaWeaponDown" || name == "gaWeaponUp")
		return Action::ChangeWeapon;
	if (name == "gaShot1")
		return Action::SelectWeapon1;
	if (name == "gaShot2")
		return Action::SelectWeapon2;
	if (name == "gaShot3")
		return Action::SelectWeapon3;
	if (name == "gaShot4")
		return Action::SelectWeapon4;
	if (name == "gaViewSwitch")
		return Action::ToggleCamera;
	if (name == "gaResetCar")
		return Action::ResetVehicle;
	if (name == "gaEscape")
		return Action::Pause;
	return std::nullopt;
}

} // namespace

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
	keyboard_actions_.clear();
	keyboard_bindings_configured_ = false;
	initialized_ = false;
}

void SdlInputManager::applyKeyboardBindings(
    const std::map<std::string, std::string> &bindings)
{
	keyboard_actions_.clear();
	for (const auto &[name, key] : bindings)
	{
		const auto action = gameAction(name);
		const auto scancode = legacyScancode(key);
		if (!action || scancode == SDL_SCANCODE_UNKNOWN)
			continue;
		auto &actions = keyboard_actions_[scancode];
		if (std::find(actions.begin(), actions.end(), *action) ==
		    actions.end())
			actions.push_back(*action);
	}
	keyboard_bindings_configured_ = true;
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
	gamepads_.emplace(device_id, GamepadState{gamepad, 0});
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
		for (auto &[device_id, state] : gamepads_)
		{
			static_cast<void>(device_id);
			state.menu_vertical_direction = 0;
		}
		break;

	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP: {
		const bool down = event.key.down;
		const bool repeat = event.key.repeat;
		if (keyboard_bindings_configured_)
		{
			// Menu navigation remains available independently of the race
			// bindings, matching ControlManager's GUI navigation layer.
			switch (event.key.scancode)
			{
			case SDL_SCANCODE_UP:
			case SDL_SCANCODE_W:
				appendDigital(events, Action::MenuUp, down, repeat,
				              Source::Keyboard);
				break;
			case SDL_SCANCODE_DOWN:
			case SDL_SCANCODE_S:
				appendDigital(events, Action::MenuDown, down, repeat,
				              Source::Keyboard);
				break;
			case SDL_SCANCODE_RETURN:
			case SDL_SCANCODE_KP_ENTER:
			case SDL_SCANCODE_SPACE:
				appendDigital(events, Action::MenuConfirm, down, repeat,
				              Source::Keyboard);
				break;
			case SDL_SCANCODE_ESCAPE:
			case SDL_SCANCODE_BACKSPACE:
				appendDigital(events, Action::MenuBack, down, repeat,
				              Source::Keyboard);
				break;
			default:
				break;
			}
			const auto found =
			    keyboard_actions_.find(event.key.scancode);
			if (found != keyboard_actions_.end())
			{
				for (const auto action : found->second)
					appendDigital(events, action, down, repeat,
					              Source::Keyboard);
			}
			break;
		}
		switch (event.key.scancode)
		{
		case SDL_SCANCODE_UP:
			appendDigital(events, Action::MenuUp, down, repeat, Source::Keyboard);
			appendDigital(events, Action::Accelerate, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_DOWN:
			appendDigital(events, Action::MenuDown, down, repeat, Source::Keyboard);
			appendDigital(events, Action::Brake, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_LEFT:
			appendDigital(events, Action::TurnLeft, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_RIGHT:
			appendDigital(events, Action::TurnRight, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_W:
			appendDigital(events, Action::MenuUp, down, repeat, Source::Keyboard);
			appendDigital(events, Action::UseWeapon, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_S:
			appendDigital(events, Action::MenuDown, down, repeat, Source::Keyboard);
			appendDigital(events, Action::Brake, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_A:
			appendDigital(events, Action::TurnLeft, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_D:
			appendDigital(events, Action::TurnRight, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_RETURN:
		case SDL_SCANCODE_KP_ENTER:
			appendDigital(events, Action::MenuConfirm, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_SPACE:
			appendDigital(events, Action::MenuConfirm, down, repeat, Source::Keyboard);
			appendDigital(events, Action::UseWeapon, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_TAB:
			appendDigital(events, Action::ChangeWeapon, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_C:
			appendDigital(events, Action::ToggleCamera, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_M:
		case SDL_SCANCODE_E:
			appendDigital(events, Action::UseMine, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_Q:
			appendDigital(events, Action::UseHyper, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_1:
			appendDigital(events, Action::SelectWeapon1, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_2:
			appendDigital(events, Action::SelectWeapon2, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_3:
			appendDigital(events, Action::SelectWeapon3, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_4:
			appendDigital(events, Action::SelectWeapon4, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_R:
			appendDigital(events, Action::ResetVehicle, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_ESCAPE:
			appendDigital(events, Action::MenuBack, down, repeat, Source::Keyboard);
			appendDigital(events, Action::Pause, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_BACKSPACE:
			appendDigital(events, Action::MenuBack, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_P:
			appendDigital(events, Action::Pause, down, repeat, Source::Keyboard);
			break;
		default:
			break;
		}
		break;
	}

	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
		if (event.button.button == SDL_BUTTON_LEFT)
		{
			appendDigital(events, Action::MenuConfirm, event.button.down, false, Source::Mouse);
			appendDigital(events, Action::UseWeapon, event.button.down, false, Source::Mouse);
		}
		else if (event.button.button == SDL_BUTTON_RIGHT)
		{
			appendDigital(events, Action::MenuBack, event.button.down, false, Source::Mouse);
		}
		break;

	case SDL_EVENT_MOUSE_WHEEL: {
		float vertical = event.wheel.y;
		if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
			vertical = -vertical;
		if (vertical > 0.0F)
		{
			appendDigital(events, Action::MenuUp, true, false, Source::Mouse);
			appendDigital(events, Action::ChangeWeapon, true, false, Source::Mouse);
		}
		else if (vertical < 0.0F)
		{
			appendDigital(events, Action::MenuDown, true, false, Source::Mouse);
			appendDigital(events, Action::ChangeWeapon, true, false, Source::Mouse);
		}
		break;
	}

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
			appendDigital(events, Action::TurnLeft, event.gbutton.down, false, Source::GamepadButton,
			              event.gbutton.which);
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
			appendDigital(events, Action::TurnRight, event.gbutton.down, false, Source::GamepadButton,
			              event.gbutton.which);
			break;
		case SDL_GAMEPAD_BUTTON_SOUTH:
			appendDigital(events, Action::MenuConfirm, event.gbutton.down, false, Source::GamepadButton,
			              event.gbutton.which);
			appendDigital(events, Action::Accelerate, event.gbutton.down, false, Source::GamepadButton,
			              event.gbutton.which);
			break;
		case SDL_GAMEPAD_BUTTON_EAST:
			appendDigital(events, Action::MenuBack, event.gbutton.down, false, Source::GamepadButton,
			              event.gbutton.which);
			appendDigital(events, Action::Brake, event.gbutton.down, false, Source::GamepadButton,
			              event.gbutton.which);
			break;
		case SDL_GAMEPAD_BUTTON_BACK:
			appendDigital(events, Action::ResetVehicle, event.gbutton.down, false, Source::GamepadButton,
			              event.gbutton.which);
			break;
		case SDL_GAMEPAD_BUTTON_START:
			appendDigital(events, Action::MenuConfirm, event.gbutton.down, false, Source::GamepadButton,
			              event.gbutton.which);
			appendDigital(events, Action::Pause, event.gbutton.down, false, Source::GamepadButton, event.gbutton.which);
			break;
		case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
		case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
			appendDigital(events, Action::ChangeWeapon, event.gbutton.down, false, Source::GamepadButton,
			              event.gbutton.which);
			break;
		case SDL_GAMEPAD_BUTTON_WEST:
			appendDigital(events, Action::UseWeapon, event.gbutton.down, false, Source::GamepadButton,
			              event.gbutton.which);
			break;
		case SDL_GAMEPAD_BUTTON_NORTH:
			appendDigital(events, Action::UseWeapon, event.gbutton.down, false, Source::GamepadButton,
			              event.gbutton.which);
			break;
		case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
			appendDigital(events, Action::ToggleCamera, event.gbutton.down, false, Source::GamepadButton,
			              event.gbutton.which);
			break;
		default:
			break;
		}
		break;

	case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
		const auto entry = gamepads_.find(event.gaxis.which);
		if (entry == gamepads_.end())
			break;

		const auto axis = static_cast<SDL_GamepadAxis>(event.gaxis.axis);
		if (axis == SDL_GAMEPAD_AXIS_LEFTY)
		{
			const float normalized = normalizeSignedAxis(event.gaxis.value);
			int next_direction = entry->second.menu_vertical_direction;
			if (next_direction == 0)
			{
				if (normalized <= -menuStickPressThreshold)
					next_direction = -1;
				else if (normalized >= menuStickPressThreshold)
					next_direction = 1;
			}
			else if (next_direction < 0)
			{
				if (normalized >= menuStickPressThreshold)
					next_direction = 1;
				else if (normalized > -stickDeadZone)
					next_direction = 0;
			}
			else
			{
				if (normalized <= -menuStickPressThreshold)
					next_direction = -1;
				else if (normalized < stickDeadZone)
					next_direction = 0;
			}

			const int previous = entry->second.menu_vertical_direction;
			if (next_direction != previous)
			{
				if (previous < 0)
					appendDigital(events, Action::MenuUp, false, false, Source::GamepadAxis, event.gaxis.which);
				else if (previous > 0)
					appendDigital(events, Action::MenuDown, false, false, Source::GamepadAxis, event.gaxis.which);

				if (next_direction < 0)
					appendDigital(events, Action::MenuUp, true, false, Source::GamepadAxis, event.gaxis.which);
				else if (next_direction > 0)
					appendDigital(events, Action::MenuDown, true, false, Source::GamepadAxis, event.gaxis.which);
				entry->second.menu_vertical_direction = next_direction;
			}
		}
		else if (axis == SDL_GAMEPAD_AXIS_LEFTX)
		{
			appendDirectionalAxis(events, applySignedDeadZone(normalizeSignedAxis(event.gaxis.value), stickDeadZone),
			                      event.gaxis.which);
		}
		else if (axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
		{
			appendAnalog(events, Action::UseMine, applyTriggerDeadZone(event.gaxis.value), Source::GamepadAxis,
			             event.gaxis.which);
		}
		else if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER)
		{
			appendAnalog(events, Action::UseHyper, applyTriggerDeadZone(event.gaxis.value), Source::GamepadAxis,
			             event.gaxis.which);
		}
		break;
	}

	default:
		break;
	}
	return events;
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
