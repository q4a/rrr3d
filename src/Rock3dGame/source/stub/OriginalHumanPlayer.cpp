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
    std::span<WeaponItem* const> primaryWeapons) const noexcept
{
    for (std::size_t slot = 0U;
         slot < primaryWeapons.size(); ++slot)
    {
        if (primaryWeapons[slot] != nullptr &&
            primaryWeapons[slot]->IsInstalled() && --number < 0)
            return slot;
    }
    return primaryWeapons.size();
}

int HumanPlayer::GetWeaponCount(
    std::span<WeaponItem* const> primaryWeapons) const noexcept
{
    int count = 0;
    for (const auto& weapon : primaryWeapons)
    {
        if (weapon == nullptr || !weapon->IsInstalled())
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
    std::span<WeaponItem* const> primaryWeapons) noexcept
{
    if (direction < 0)
    {
        currentWeapon_ = std::max(currentWeapon_ - 1, 0);
        return;
    }
    const int count = GetWeaponCount(primaryWeapons);
    // HumanPlayer.cpp uses min(cur + 1, GetWeaponCount() - 1) verbatim.
    // With no installed primary weapon the source therefore stores -1.
    currentWeapon_ = std::min(currentWeapon_ + 1, count - 1);
}

HumanPlayer::Selection HumanPlayer::SelectWeapon(
    std::span<WeaponItem* const> primaryWeapons) noexcept
{
    if (primaryWeapons.empty())
        return {};
    for (std::size_t offset = 0U;
         offset < primaryWeapons.size(); ++offset)
    {
        const std::size_t slot =
            (static_cast<std::size_t>(std::max(currentWeapon_, 0)) +
             offset) % primaryWeapons.size();
        const auto* weapon = primaryWeapons[slot];
        if (weapon != nullptr && weapon->IsInstalled() &&
            weapon->GetCurCharge() > 0U)
        {
            currentWeapon_ = static_cast<int>(slot);
            return {slot, true};
        }
    }
    currentWeapon_ = 0;
    return {0U, false};
}

std::vector<HumanPlayer::InputCommand> HumanPlayer::OnHandleInput(
    std::span<const originalcontrol::InputMessage> messages,
    bool playerBlocked, bool carPresent, bool chatMode)
{
    using Action = rrr3d::input::Action;
    using Source = rrr3d::input::Source;

    std::vector<InputCommand> commands;
    commands.reserve(messages.size());
    for (const auto& message : messages)
    {
        // The four keyboard driving booleans are updated before these gates
        // in the source. Continuous driving is polled separately by the
        // portable ControlManager owner, so only event commands remain here.
        if (playerBlocked || !carPresent || chatMode ||
            !message.action.has_value() || !message.active ||
            message.repeat)
            continue;

        const auto action = *message.action;
        if (action == Action::UseAllWeapons)
        {
            commands.push_back({InputCommandKind::ShotAll, 0});
            continue;
        }
        if (action == Action::ResetVehicle)
        {
            commands.push_back({InputCommandKind::ResetCar, 0});
            continue;
        }
        if (action == Action::UseMine)
        {
            // gaMine with alphaMax != 0 is not an edge command. It is polled
            // later by OnInputProgress with the alpha-dependent delay.
            if (message.source != Source::GamepadAxis)
                commands.push_back({InputCommandKind::ShotMine, 0});
            continue;
        }
        if (action == Action::UseWeapon)
        {
            commands.push_back({InputCommandKind::ShotCurrent, 0});
            continue;
        }
        if (action == Action::PreviousWeapon)
        {
            commands.push_back({InputCommandKind::ChangeWeapon, -1});
            continue;
        }
        if (action == Action::NextWeapon ||
            action == Action::ChangeWeapon)
        {
            commands.push_back({InputCommandKind::ChangeWeapon, 1});
            continue;
        }
        if (action >= Action::SelectWeapon1 &&
            action <= Action::SelectWeapon4)
        {
            commands.push_back(
                {InputCommandKind::ShotWeaponSlot,
                 static_cast<int>(action) -
                     static_cast<int>(Action::SelectWeapon1)});
        }
    }
    return commands;
}

HumanPlayer::DrivingCommand HumanPlayer::OnInputProgress(
    bool accelerateDown, bool backDown,
    float leftDown, float rightDown,
    bool leftAnalog, bool rightAnalog) noexcept
{
    DrivingCommand result;
    if (accelerateDown)
        result.throttle = 1.0F;
    else if (backDown)
        result.reverse = 1.0F;

    // HumanPlayer::Control evaluates left before right. Two simultaneous
    // digital directions therefore steer left instead of cancelling out.
    if (leftDown != 0.0F)
    {
        result.steering = leftDown;
        result.manualSteering = leftAnalog;
    }
    else if (rightDown != 0.0F)
    {
        result.steering = -rightDown;
        result.manualSteering = rightAnalog;
    }
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
