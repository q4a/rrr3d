#include "OriginalHumanPlayer.h"

#include <algorithm>

namespace r3d::game::originalrace::source
{

HumanPlayer::HumanPlayer(int currentWeapon) noexcept
    : currentWeapon_(currentWeapon)
{
}

std::size_t HumanPlayer::GetWeaponByIndex(
    int number,
    std::span<const WeaponItem> primaryWeapons) const noexcept
{
    for (std::size_t slot = 0U;
         slot < primaryWeapons.size(); ++slot)
    {
        if (primaryWeapons[slot].IsInstalled() && --number < 0)
            return slot;
    }
    return primaryWeapons.size();
}

int HumanPlayer::GetWeaponCount(
    std::span<const WeaponItem> primaryWeapons) const noexcept
{
    int count = 0;
    for (const auto& weapon : primaryWeapons)
    {
        if (!weapon.IsInstalled())
            break;
        ++count;
    }
    return count;
}

int HumanPlayer::GetCurWeapon() const noexcept
{
    return currentWeapon_;
}

void HumanPlayer::SetCurWeapon(int index) noexcept
{
    currentWeapon_ = index;
}

void HumanPlayer::ChangeWeapon(
    int direction,
    std::span<const WeaponItem> primaryWeapons) noexcept
{
    if (direction < 0)
    {
        currentWeapon_ = std::max(currentWeapon_ - 1, 0);
        return;
    }
    const int count = GetWeaponCount(primaryWeapons);
    currentWeapon_ = count > 0
                         ? std::min(currentWeapon_ + 1, count - 1)
                         : 0;
}

HumanPlayer::Selection HumanPlayer::SelectWeapon(
    std::span<const WeaponItem> primaryWeapons) noexcept
{
    if (primaryWeapons.empty())
        return {};
    for (std::size_t offset = 0U;
         offset < primaryWeapons.size(); ++offset)
    {
        const std::size_t slot =
            (static_cast<std::size_t>(std::max(currentWeapon_, 0)) +
             offset) % primaryWeapons.size();
        const auto& weapon = primaryWeapons[slot];
        if (weapon.IsInstalled() && weapon.GetCurCharge() > 0U)
        {
            currentWeapon_ = static_cast<int>(slot);
            return {slot, true};
        }
    }
    currentWeapon_ = 0;
    return {0U, false};
}

HumanPlayer::DrivingCommand HumanPlayer::OnInputProgress(
    bool accelerateDown, bool backDown,
    float leftDown, float rightDown) noexcept
{
    DrivingCommand result;
    if (accelerateDown)
        result.throttle = 1.0F;
    else if (backDown)
        result.reverse = 1.0F;

    // HumanPlayer::Control evaluates left before right. Two simultaneous
    // digital directions therefore steer left instead of cancelling out.
    if (leftDown != 0.0F)
        result.steering = leftDown;
    else if (rightDown != 0.0F)
        result.steering = -rightDown;
    return result;
}

HumanPlayer::ControlGate HumanPlayer::EvaluateControl(
    bool playerBlocked, bool carPresent,
    bool chatMode, bool debugAiControl) noexcept
{
    ControlGate result;
    if (playerBlocked || !carPresent)
        return result;

    // Control::OnHandleInput checks chat immediately after block/mapObj.
    result.inputActions = !chatMode;

    // Control::OnInputProgress checks the debug AICar before writing the
    // move/steering state, then checks chat only before Hyper/Mine polling.
    result.driving = !debugAiControl;
    result.progressWeapons = result.driving && !chatMode;
    return result;
}

bool HumanPlayer::ResetCar(
    bool carPresent, bool anyWheelContact,
    bool bodyContact) noexcept
{
    return carPresent && (anyWheelContact || bodyContact);
}

} // namespace r3d::game::originalrace::source
