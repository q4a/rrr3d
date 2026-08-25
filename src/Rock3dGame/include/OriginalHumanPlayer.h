#pragma once

#include "OriginalWeapon.h"

#include <cstddef>
#include <span>

namespace r3d::game::originalrace::source
{

// Backend-neutral owner for the active HumanPlayer/Control rules. SDL keeps
// raw device state, while this class preserves the Windows action priority,
// direct-slot mapping and _curWeapon lifecycle before the Jolt/Logic boundary.
class HumanPlayer
{
public:
    struct Selection
    {
        std::size_t slot = 0U;
        bool found = false;
    };

    struct DrivingCommand
    {
        float throttle = 0.0F;
        float reverse = 0.0F;
        float steering = 0.0F;
    };

    HumanPlayer() = default;
    explicit HumanPlayer(int currentWeapon) noexcept;

    std::size_t GetWeaponByIndex(
        int number,
        std::span<const WeaponItem> primaryWeapons) const noexcept;
    int GetWeaponCount(
        std::span<const WeaponItem> primaryWeapons) const noexcept;
    int GetCurWeapon() const noexcept;
    void SetCurWeapon(int index) noexcept;
    void ChangeWeapon(
        int direction,
        std::span<const WeaponItem> primaryWeapons) noexcept;
    Selection SelectWeapon(
        std::span<const WeaponItem> primaryWeapons) noexcept;

    static DrivingCommand OnInputProgress(
        bool accelerateDown, bool backDown,
        float leftDown, float rightDown) noexcept;

private:
    int currentWeapon_ = 0;
};

} // namespace r3d::game::originalrace::source
