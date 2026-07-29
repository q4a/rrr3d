#pragma once

#include "InputActions.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace rrr3d::input
{

class SdlInputManager
{
  public:
	static constexpr float stickDeadZone = 0.25F;
	static constexpr float menuStickPressThreshold = 0.55F;
	static constexpr float triggerDeadZone = 0.12F;

	SdlInputManager() = default;
	~SdlInputManager();

	SdlInputManager(const SdlInputManager &) = delete;
	SdlInputManager &operator=(const SdlInputManager &) = delete;

	bool initialize(std::string &error);
	void shutdown() noexcept;
	void applyKeyboardBindings(
	    const std::map<std::string, std::string> &bindings);
	void applyGamepadBindings(
	    const std::map<std::string, std::string> &bindings);

	std::vector<ActionEvent> processEvent(const SDL_Event &event);
	float heldValue(Action action) const noexcept;

	std::size_t connectedGamepadCount() const noexcept;
	bool hasGamepad(SDL_JoystickID device_id) const noexcept;
	bool rumble(SDL_JoystickID device_id, float low_frequency, float high_frequency,
	            std::uint32_t duration_ms) noexcept;

  private:
	struct GamepadState
	{
		SDL_Gamepad *handle = nullptr;
		int menu_vertical_direction = 0;
	};

	struct GamepadAxisBinding
	{
		Action action = Action::Accelerate;
		int direction = 0;
		bool trigger = false;
	};

	bool openGamepad(SDL_JoystickID device_id) noexcept;
	void closeGamepad(SDL_JoystickID device_id) noexcept;
	void appendGamepadReleases(std::vector<ActionEvent> &events, SDL_JoystickID device_id) const;
	void clearHeldSource(Source source) noexcept;
	void clearHeldDevice(SDL_JoystickID device_id) noexcept;

	bool initialized_ = false;
	bool keyboard_bindings_configured_ = false;
	std::map<SDL_Scancode, std::vector<Action>> keyboard_actions_;
	std::map<SDL_GamepadButton, std::vector<Action>>
	    gamepad_button_actions_;
	std::map<SDL_GamepadAxis, std::vector<GamepadAxisBinding>>
	    gamepad_axis_actions_;
	std::map<SDL_JoystickID, GamepadState> gamepads_;
	std::map<std::tuple<Action, Source, SDL_JoystickID>, float>
	    held_action_values_;
};

} // namespace rrr3d::input
