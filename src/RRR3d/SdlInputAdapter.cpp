#include "SdlInputAdapter.h"

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>

#include <cmath>

namespace rrr3d::sdl
{
namespace
{

lsl::KeyState key_state(bool down) noexcept
{
    return down ? lsl::ksDown : lsl::ksUp;
}

lsl::Point point(float x, float y) noexcept
{
    return {static_cast<int>(std::lround(x)),
            static_cast<int>(std::lround(y))};
}

bool shift_pressed(SDL_Keymod modifiers) noexcept
{
    return (modifiers & SDL_KMOD_SHIFT) != 0;
}

bool control_pressed(SDL_Keymod modifiers) noexcept
{
    return (modifiers & SDL_KMOD_CTRL) != 0;
}

std::optional<lsl::MouseKey> mouse_key(Uint8 button) noexcept
{
    switch (button)
    {
    case SDL_BUTTON_LEFT:
        return lsl::mkLeft;
    case SDL_BUTTON_RIGHT:
        return lsl::mkRight;
    case SDL_BUTTON_MIDDLE:
        return lsl::mkMiddle;
    default:
        return std::nullopt;
    }
}

} // namespace

KeyInput translate_key_input(const SDL_KeyboardEvent& event) noexcept
{
    return {event.scancode,
            event.key,
            key_state(event.down),
            event.repeat,
            shift_pressed(event.mod),
            control_pressed(event.mod)};
}

std::optional<MouseButtonInput> translate_mouse_button_input(
    const SDL_MouseButtonEvent& event) noexcept
{
    const auto translated_button = mouse_key(event.button);
    if (!translated_button)
        return std::nullopt;

    const SDL_Keymod modifiers = SDL_GetModState();
    return MouseButtonInput{*translated_button,
                            key_state(event.down),
                            point(event.x, event.y),
                            shift_pressed(modifiers),
                            control_pressed(modifiers)};
}

MouseMotionInput translate_mouse_motion_input(
    const SDL_MouseMotionEvent& event) noexcept
{
    const SDL_Keymod modifiers = SDL_GetModState();
    return {point(event.x, event.y),
            point(event.xrel, event.yrel),
            shift_pressed(modifiers),
            control_pressed(modifiers)};
}

} // namespace rrr3d::sdl
