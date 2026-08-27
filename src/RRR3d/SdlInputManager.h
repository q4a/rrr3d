#pragma once

#include "InputActions.h"
#include "OriginalControlManager.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace rrr3d::input
{

// SDL platform boundary for the source ControlManager VirtualKey table.
// Capture names are the exact strings persisted by Windows user.xml.
std::optional<std::string> originalKeyboardBindingName(
    SDL_Scancode scancode);
std::optional<std::string> originalGamepadButtonBindingName(
    SDL_GamepadButton button);
std::optional<std::string> originalGamepadAxisBindingName(
    SDL_GamepadAxis axis, Sint16 value);

class SdlInputManager
{
  public:
	// XInput constants used by the Windows ControlManager.  Right-thumb
	// bindings deliberately use the right threshold to become active but the
	// left threshold for value normalization, matching its VirtualKey table.
	static constexpr float sourceLeftStickDeadZone = 7849.0F / 32767.0F;
	static constexpr float sourceRightStickDeadZone = 8689.0F / 32767.0F;
	static constexpr float triggerDeadZone = 30.0F / 255.0F;

	SdlInputManager() = default;
	~SdlInputManager();

	SdlInputManager(const SdlInputManager &) = delete;
	SdlInputManager &operator=(const SdlInputManager &) = delete;

	bool initialize(std::string &error);
	void shutdown() noexcept;
	void resetInput() noexcept;
	void applyKeyboardBindings(
	    const std::map<std::string, std::string> &bindings);
	void applyGamepadBindings(
	    const std::map<std::string, std::string> &bindings);

	std::vector<ActionEvent> processEvent(const SDL_Event &event);
	float heldValue(Action action) const noexcept;
	float heldValue(Action action, Source source) const noexcept;

	std::size_t connectedGamepadCount() const noexcept;
	bool hasGamepad(SDL_JoystickID device_id) const noexcept;
	bool rumble(SDL_JoystickID device_id, float low_frequency, float high_frequency,
	            std::uint32_t duration_ms) noexcept;

  private:
	struct GamepadState
	{
		SDL_Gamepad *handle = nullptr;
	};

    bool openGamepad(SDL_JoystickID device_id) noexcept;
    void closeGamepad(SDL_JoystickID device_id) noexcept;
    void appendGamepadReleases(std::vector<ActionEvent> &events, SDL_JoystickID device_id) const;

    bool initialized_ = false;
    std::map<SDL_JoystickID, GamepadState> gamepads_;
    r3d::game::originalcontrol::ControlManager control_;
};

} // namespace rrr3d::input
