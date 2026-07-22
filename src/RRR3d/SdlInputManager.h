#pragma once

#include "InputActions.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
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

	std::vector<ActionEvent> processEvent(const SDL_Event &event);

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

	bool openGamepad(SDL_JoystickID device_id) noexcept;
	void closeGamepad(SDL_JoystickID device_id) noexcept;
	void appendGamepadReleases(std::vector<ActionEvent> &events, SDL_JoystickID device_id) const;

	bool initialized_ = false;
	std::map<SDL_JoystickID, GamepadState> gamepads_;
};

} // namespace rrr3d::input
