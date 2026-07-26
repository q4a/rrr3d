#pragma once

#include <cstdint>
#include <string_view>

namespace rrr3d::input
{

enum class Action : std::uint8_t
{
    Accelerate,
    Brake,
    TurnLeft,
    TurnRight,
    UseWeapon,
    UseMine,
    UseHyper,
    ChangeWeapon,
    SelectWeapon1,
    SelectWeapon2,
    SelectWeapon3,
    SelectWeapon4,
    ToggleCamera,
    ResetVehicle,
    Pause,
    MenuUp,
    MenuDown,
    MenuConfirm,
    MenuBack
};

enum class Source : std::uint8_t
{
    Keyboard,
    Mouse,
    GamepadButton,
    GamepadAxis,
    System
};

struct ActionEvent
{
    Action action = Action::MenuBack;
    float value = 0.0F;
    bool active = false;
    bool repeated = false;
    Source source = Source::System;
    std::uint32_t device_id = 0;
};

constexpr std::string_view actionName(Action action) noexcept
{
    switch (action)
    {
    case Action::Accelerate:
        return "Accelerate";
    case Action::Brake:
        return "Brake";
    case Action::TurnLeft:
        return "TurnLeft";
    case Action::TurnRight:
        return "TurnRight";
    case Action::UseWeapon:
        return "UseWeapon";
    case Action::UseMine:
        return "UseMine";
    case Action::UseHyper:
        return "UseHyper";
    case Action::ChangeWeapon:
        return "ChangeWeapon";
    case Action::SelectWeapon1:
        return "SelectWeapon1";
    case Action::SelectWeapon2:
        return "SelectWeapon2";
    case Action::SelectWeapon3:
        return "SelectWeapon3";
    case Action::SelectWeapon4:
        return "SelectWeapon4";
    case Action::ToggleCamera:
        return "ToggleCamera";
    case Action::ResetVehicle:
        return "ResetVehicle";
    case Action::Pause:
        return "Pause";
    case Action::MenuUp:
        return "MenuUp";
    case Action::MenuDown:
        return "MenuDown";
    case Action::MenuConfirm:
        return "MenuConfirm";
    case Action::MenuBack:
        return "MenuBack";
    }
    return "Unknown";
}

constexpr std::string_view sourceName(Source source) noexcept
{
    switch (source)
    {
    case Source::Keyboard:
        return "keyboard";
    case Source::Mouse:
        return "mouse";
    case Source::GamepadButton:
        return "gamepad-button";
    case Source::GamepadAxis:
        return "gamepad-axis";
    case Source::System:
        return "system";
    }
    return "unknown";
}

} // namespace rrr3d::input
