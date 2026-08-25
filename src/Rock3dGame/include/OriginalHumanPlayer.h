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

    // Exact three-stage gate used by HumanPlayer::Control. Event actions are
    // rejected by block/car/chat state; continuous driving is evaluated
    // before the chat check; Hyper/analog Mine are evaluated after it. The
    // Windows AIDebug owner suppresses only continuous progress.
    struct ControlGate
    {
        bool inputActions = false;
        bool driving = false;
        bool progressWeapons = false;
    };

    HumanPlayer() = default;
    explicit HumanPlayer(int currentWeapon) noexcept;

    std::size_t GetWeaponByIndex(
        int number,
        std::span<WeaponItem* const> primaryWeapons) const noexcept;
    int GetWeaponCount(
        std::span<WeaponItem* const> primaryWeapons) const noexcept;
    int GetCurWeapon() const noexcept;
    void SetCurWeapon(int index) noexcept;
    void ChangeWeapon(
        int direction,
        std::span<WeaponItem* const> primaryWeapons) noexcept;
    Selection SelectWeapon(
        std::span<WeaponItem* const> primaryWeapons) noexcept;

    static DrivingCommand OnInputProgress(
        bool accelerateDown, bool backDown,
        float leftDown, float rightDown) noexcept;
    static ControlGate EvaluateControl(
        bool playerBlocked, bool carPresent,
        bool chatMode, bool debugAiControl) noexcept;
    static bool ResetCar(
        bool carPresent, bool anyWheelContact,
        bool bodyContact) noexcept;

private:
    int currentWeapon_ = 0;
};

} // namespace r3d::game::originalrace::source
