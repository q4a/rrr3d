#pragma once

#include "lslUtility.h"

#include <SDL3/SDL_events.h>

#include <optional>

namespace rrr3d::sdl
{

struct KeyInput
{
    SDL_Scancode scancode;
    SDL_Keycode keycode;
    lsl::KeyState state;
    bool repeat;
    bool shift;
    bool control;
};

struct MouseButtonInput
{
    lsl::MouseKey button;
    lsl::KeyState state;
    lsl::Point position;
    bool shift;
    bool control;
};

struct MouseMotionInput
{
    lsl::Point position;
    lsl::Point relative;
    bool shift;
    bool control;
};

KeyInput translate_key_input(const SDL_KeyboardEvent& event) noexcept;
std::optional<MouseButtonInput> translate_mouse_button_input(
    const SDL_MouseButtonEvent& event) noexcept;
MouseMotionInput translate_mouse_motion_input(
    const SDL_MouseMotionEvent& event) noexcept;

} // namespace rrr3d::sdl
