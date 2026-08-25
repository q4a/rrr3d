#include "SdlInputManager.h"

#include "OriginalControlBindings.h"

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

float applySourceStickDeadZone(float value, SDL_GamepadAxis axis) noexcept
{
	const float activation =
	    axis == SDL_GAMEPAD_AXIS_RIGHTX ||
	            axis == SDL_GAMEPAD_AXIS_RIGHTY
	        ? SdlInputManager::sourceRightStickDeadZone
	        : SdlInputManager::sourceLeftStickDeadZone;
	const float magnitude = std::abs(value);
	if (magnitude <= activation)
		return 0.0F;
	// Original right-thumb VirtualKey entries contain the left-thumb
	// normalization threshold even though GetGamepadKeyState gates them with
	// XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE.
	const float scaled =
	    (magnitude - SdlInputManager::sourceLeftStickDeadZone) /
	    (1.0F - SdlInputManager::sourceLeftStickDeadZone);
	return std::copysign(std::clamp(scaled, 0.0F, 1.0F), value);
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

SDL_Scancode legacyScancode(const std::string &name) noexcept
{
	const std::string canonical =
	    r3d::game::originalcontrol::canonicalVirtualKeyName(
	        r3d::game::originalcontrol::ControllerType::Keyboard, name);
	if (canonical == "None")
		return SDL_SCANCODE_UNKNOWN;
	if (canonical == "Up Arrow")
		return SDL_SCANCODE_UP;
	if (canonical == "Down Arrow")
		return SDL_SCANCODE_DOWN;
	if (canonical == "Left Arrow")
		return SDL_SCANCODE_LEFT;
	if (canonical == "Right Arrow")
		return SDL_SCANCODE_RIGHT;
	if (canonical == "Enter")
		return SDL_SCANCODE_RETURN;
	if (canonical == "Escape")
		return SDL_SCANCODE_ESCAPE;
	if (canonical == "Space")
		return SDL_SCANCODE_SPACE;
	// Windows maps VK_BACK to the keyboard table's vkButtonX entry. The
	// persisted name is therefore "X", not "Backspace". A and B are table
	// entries without keyboard state handlers and remain inactive after a
	// source-compatible reload.
	if (canonical == "X")
		return SDL_SCANCODE_BACKSPACE;
	if (canonical == "A" || canonical == "B")
		return SDL_SCANCODE_UNKNOWN;
	const auto scancode = SDL_GetScancodeFromName(canonical.c_str());
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
	if (name == "gaShot")
		return Action::UseWeapon;
	if (name == "gaShotAll")
		return Action::UseAllWeapons;
	if (name == "gaMine")
		return Action::UseMine;
	if (name == "gaHyper")
		return Action::UseHyper;
	if (name == "gaWeaponDown")
		return Action::PreviousWeapon;
	if (name == "gaWeaponUp")
		return Action::NextWeapon;
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
	if (name == "gaAction")
		return Action::MenuConfirm;
	if (name == "gaEscape")
		return Action::Pause;
	if (name == "gaDebug1")
		return Action::Debug1;
	if (name == "gaDebug2")
		return Action::Debug2;
	if (name == "gaDebug3")
		return Action::Debug3;
	if (name == "gaDebug4")
		return Action::Debug4;
	if (name == "gaDebug5")
		return Action::Debug5;
	if (name == "gaDebug6")
		return Action::Debug6;
	if (name == "gaDebug7")
		return Action::Debug7;
	return std::nullopt;
}

std::optional<SDL_GamepadButton>
gamepadButton(std::string_view name) noexcept
{
	if (name == "A")
		return SDL_GAMEPAD_BUTTON_SOUTH;
	if (name == "B")
		return SDL_GAMEPAD_BUTTON_EAST;
	if (name == "X")
		return SDL_GAMEPAD_BUTTON_WEST;
	if (name == "Y")
		return SDL_GAMEPAD_BUTTON_NORTH;
	if (name == "DPad Up")
		return SDL_GAMEPAD_BUTTON_DPAD_UP;
	if (name == "DPad Down")
		return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
	if (name == "DPad Left")
		return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
	if (name == "DPad Right")
		return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
	if (name == "Left Shoulder")
		return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
	if (name == "Right Shoulder")
		return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
	if (name == "L.Thumb Press")
		return SDL_GAMEPAD_BUTTON_LEFT_STICK;
	if (name == "R.Thumb Press")
		return SDL_GAMEPAD_BUTTON_RIGHT_STICK;
	if (name == "Back")
		return SDL_GAMEPAD_BUTTON_BACK;
	if (name == "Start")
		return SDL_GAMEPAD_BUTTON_START;
	return std::nullopt;
}

struct ParsedAxis
{
	SDL_GamepadAxis axis = SDL_GAMEPAD_AXIS_INVALID;
	int direction = 0;
	bool trigger = false;
};

std::optional<ParsedAxis> gamepadAxis(std::string_view name) noexcept
{
	if (name == "Left Trigger")
		return ParsedAxis{SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 1, true};
	if (name == "Right Trigger")
		return ParsedAxis{SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 1, true};
	if (name == "L.Thumb Move X")
		return ParsedAxis{SDL_GAMEPAD_AXIS_LEFTX, 0, false};
	if (name == "L.Thumb Move Y")
		return ParsedAxis{SDL_GAMEPAD_AXIS_LEFTY, 0, false};
	if (name == "R.Thumb Move X")
		return ParsedAxis{SDL_GAMEPAD_AXIS_RIGHTX, 0, false};
	if (name == "R.Thumb Move Y")
		return ParsedAxis{SDL_GAMEPAD_AXIS_RIGHTY, 0, false};
	if (name == "L.Thumb Left")
		return ParsedAxis{SDL_GAMEPAD_AXIS_LEFTX, -1, false};
	if (name == "L.Thumb Right")
		return ParsedAxis{SDL_GAMEPAD_AXIS_LEFTX, 1, false};
	if (name == "L.Thumb Up")
		return ParsedAxis{SDL_GAMEPAD_AXIS_LEFTY, -1, false};
	if (name == "L.Thumb Down")
		return ParsedAxis{SDL_GAMEPAD_AXIS_LEFTY, 1, false};
	if (name == "R.Thumb Left")
		return ParsedAxis{SDL_GAMEPAD_AXIS_RIGHTX, -1, false};
	if (name == "R.Thumb Right")
		return ParsedAxis{SDL_GAMEPAD_AXIS_RIGHTX, 1, false};
	if (name == "R.Thumb Up")
		return ParsedAxis{SDL_GAMEPAD_AXIS_RIGHTY, -1, false};
	if (name == "R.Thumb Down")
		return ParsedAxis{SDL_GAMEPAD_AXIS_RIGHTY, 1, false};
	return std::nullopt;
}

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
	gamepad_button_actions_.clear();
	gamepad_axis_actions_.clear();
	held_action_values_.clear();
	keyboard_bindings_configured_ = false;
	initialized_ = false;
}

void SdlInputManager::applyKeyboardBindings(
    const std::map<std::string, std::string> &bindings)
{
	clearHeldSource(Source::Keyboard);
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

void SdlInputManager::applyGamepadBindings(
    const std::map<std::string, std::string> &bindings)
{
	clearHeldSource(Source::GamepadButton);
	clearHeldSource(Source::GamepadAxis);
	gamepad_button_actions_.clear();
	gamepad_axis_actions_.clear();
	for (const auto &[name, key] : bindings)
	{
		const auto action = gameAction(name);
		const std::string canonical =
		    r3d::game::originalcontrol::canonicalVirtualKeyName(
		        r3d::game::originalcontrol::ControllerType::Gamepad,
		        key);
		if (!action || canonical == "None")
			continue;
		if (const auto button = gamepadButton(canonical))
		{
			auto &actions = gamepad_button_actions_[*button];
			if (std::find(actions.begin(), actions.end(), *action) ==
			    actions.end())
				actions.push_back(*action);
			continue;
		}
		if (const auto axis = gamepadAxis(canonical))
		{
			auto &actions = gamepad_axis_actions_[axis->axis];
			const auto duplicate = std::find_if(
			    actions.begin(), actions.end(),
			    [&](const GamepadAxisBinding &value) {
				    return value.action == *action &&
				           value.direction == axis->direction &&
				           value.trigger == axis->trigger;
			    });
			if (duplicate == actions.end())
				actions.push_back(
				    {*action, axis->direction, axis->trigger});
		}
	}
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
			case SDL_SCANCODE_BACKSPACE:
				appendDigital(events, Action::MenuBack, down, repeat,
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
			appendDigital(events, Action::Pause, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_BACKSPACE:
			appendDigital(events, Action::MenuBack, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_P:
			appendDigital(events, Action::Pause, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_F1:
			appendDigital(events, Action::Debug1, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_F2:
			appendDigital(events, Action::Debug2, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_F3:
			appendDigital(events, Action::Debug3, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_F4:
			appendDigital(events, Action::Debug4, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_F5:
			appendDigital(events, Action::Debug5, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_F6:
			appendDigital(events, Action::Debug6, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_F7:
			appendDigital(events, Action::Debug7, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_F10:
			appendDigital(events, Action::DebugOverlay, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_PAGEUP:
			appendDigital(events, Action::DebugPagePrevious, down, repeat, Source::Keyboard);
			break;
		case SDL_SCANCODE_PAGEDOWN:
			appendDigital(events, Action::DebugPageNext, down, repeat, Source::Keyboard);
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
			appendDigital(events, Action::PreviousWeapon, true, false,
			              Source::Mouse);
		}
		else if (vertical < 0.0F)
		{
			appendDigital(events, Action::MenuDown, true, false, Source::Mouse);
			appendDigital(events, Action::NextWeapon, true, false,
			              Source::Mouse);
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
		case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
			break;
		case SDL_GAMEPAD_BUTTON_SOUTH:
			appendDigital(events, Action::MenuConfirm, event.gbutton.down, false, Source::GamepadButton,
			              event.gbutton.which);
			break;
		case SDL_GAMEPAD_BUTTON_EAST:
			appendDigital(events, Action::MenuBack, event.gbutton.down, false, Source::GamepadButton,
			              event.gbutton.which);
			break;
		default:
			break;
		}
		if (const auto found = gamepad_button_actions_.find(
		        static_cast<SDL_GamepadButton>(event.gbutton.button));
		    found != gamepad_button_actions_.end())
		{
			for (const auto action : found->second)
				appendDigital(events, action, event.gbutton.down, false,
				              Source::GamepadButton,
				              event.gbutton.which);
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
		if (const auto found = gamepad_axis_actions_.find(axis);
		    found != gamepad_axis_actions_.end())
		{
			const float signedValue = applySourceStickDeadZone(
			    normalizeSignedAxis(event.gaxis.value), axis);
			for (const auto &binding : found->second)
			{
				float value = binding.trigger
				                  ? applyTriggerDeadZone(event.gaxis.value)
				                  : std::abs(signedValue);
				if (!binding.trigger && binding.direction < 0)
					value = std::max(-signedValue, 0.0F);
				else if (!binding.trigger && binding.direction > 0)
					value = std::max(signedValue, 0.0F);
				appendAnalog(events, binding.action, value,
				             Source::GamepadAxis, event.gaxis.which);
			}
		}
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
		held_action_values_.clear();
	}
	else
	{
		if (event.type == SDL_EVENT_GAMEPAD_REMOVED)
			clearHeldDevice(event.gdevice.which);
		for (const auto &eventValue : unique)
		{
			if (eventValue.source == Source::System)
				continue;
			held_action_values_[{eventValue.action, eventValue.source,
			                     eventValue.device_id}] =
			    eventValue.active ? std::clamp(eventValue.value, 0.0F, 1.0F)
			                      : 0.0F;
		}
	}
	return unique;
}

float SdlInputManager::heldValue(Action action) const noexcept
{
	float value = 0.0F;
	for (const auto &[key, held] : held_action_values_)
	{
		if (std::get<0>(key) == action)
			value = std::max(value, held);
	}
	return value;
}

void SdlInputManager::clearHeldSource(Source source) noexcept
{
	for (auto entry = held_action_values_.begin();
	     entry != held_action_values_.end();)
	{
		if (std::get<1>(entry->first) == source)
			entry = held_action_values_.erase(entry);
		else
			++entry;
	}
}

void SdlInputManager::clearHeldDevice(SDL_JoystickID device_id) noexcept
{
	for (auto entry = held_action_values_.begin();
	     entry != held_action_values_.end();)
	{
		if (std::get<2>(entry->first) == device_id)
			entry = held_action_values_.erase(entry);
		else
			++entry;
	}
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
