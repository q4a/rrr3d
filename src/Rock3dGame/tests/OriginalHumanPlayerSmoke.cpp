#include "OriginalHumanPlayer.h"

#include <array>
#include <cmath>
#include <iostream>

int main()
{
    namespace source = r3d::game::originalrace::source;

    std::array<source::Weapon, 3U> weapons{};
    std::array<std::uint32_t, 3U> charges{0U, 1U, 1U};
    std::array<source::WeaponItem, 3U> items{
        source::WeaponItem(&weapons[0], 7U, 1U, &charges[0]),
        source::WeaponItem(&weapons[1], 7U, 1U, &charges[1]),
        source::WeaponItem(&weapons[2], 7U, 1U, &charges[2])};
    std::array<source::WeaponItem*, 3U> itemPointers{
        &items[0], &items[1], &items[2]};
    for (auto& item : items)
        item.OnCreateCar();

    source::HumanPlayer human;
    auto selection = human.SelectWeapon(itemPointers);
    if (!selection.found || selection.slot != 1U ||
        human.GetCurWeapon() != 1)
        return 1;
    items[1].SetCurCharge(0U);
    selection = human.SelectWeapon(itemPointers);
    if (!selection.found || selection.slot != 2U ||
        human.GetCurWeapon() != 2)
        return 2;
    items[2].SetCurCharge(0U);
    selection = human.SelectWeapon(itemPointers);
    if (selection.found || selection.slot != 0U ||
        human.GetCurWeapon() != 0)
        return 3;

    for (auto& item : items)
        item.SetCurCharge(1U);
    human.ChangeWeapon(1, itemPointers);
    human.ChangeWeapon(1, itemPointers);
    human.ChangeWeapon(1, itemPointers);
    if (human.GetCurWeapon() != 2)
        return 4;
    human.ChangeWeapon(-1, itemPointers);
    if (human.GetCurWeapon() != 1 ||
        human.GetWeaponCount(itemPointers) != 3 ||
        human.GetWeaponByIndex(0, itemPointers) != 0U ||
        human.GetWeaponByIndex(2, itemPointers) != 2U ||
        human.GetWeaponByIndex(3, itemPointers) != itemPointers.size())
        return 5;
    std::array<source::WeaponItem*, 3U> sparse{
        &items[0], nullptr, &items[2]};
    if (human.GetWeaponCount(sparse) != 1 ||
        human.GetWeaponByIndex(0, sparse) != 0U ||
        human.GetWeaponByIndex(1, sparse) != 2U)
        return 6;

    std::array<source::WeaponItem*, 3U> emptyItems{};
    human.SetCurWeapon(0);
    human.ChangeWeapon(1, emptyItems);
    if (human.GetCurWeapon() != -1)
        return 7;

    using Action = rrr3d::input::Action;
    using Controller = r3d::game::originalcontrol::ControllerType;
    using InputMessage = r3d::game::originalcontrol::InputMessage;
    using Source = rrr3d::input::Source;
    const std::array inputMessages{
        InputMessage{Action::UseAllWeapons, "Space", true, false,
                     Controller::Keyboard, U'\0', 1.0F,
                     Source::Keyboard, 0U},
        InputMessage{Action::ResetVehicle, "Backspace", true, false,
                     Controller::Keyboard, U'\0', 1.0F,
                     Source::Keyboard, 0U},
        InputMessage{Action::UseMine, "RightTrigger", true, false,
                     Controller::Gamepad, U'\0', 1.0F,
                     Source::GamepadAxis, 1U},
        InputMessage{Action::UseMine, "M", true, false,
                     Controller::Keyboard, U'\0', 1.0F,
                     Source::Keyboard, 0U},
        InputMessage{Action::UseWeapon, "L", true, true,
                     Controller::Keyboard, U'\0', 1.0F,
                     Source::Keyboard, 0U},
        InputMessage{Action::PreviousWeapon, "Q", true, false,
                     Controller::Keyboard, U'\0', 1.0F,
                     Source::Keyboard, 0U},
        InputMessage{Action::SelectWeapon3, "3", true, false,
                     Controller::Keyboard, U'\0', 1.0F,
                     Source::Keyboard, 0U},
    };
    const auto commands = source::HumanPlayer::OnHandleInput(
        inputMessages, false, true, false);
    using Kind = source::HumanPlayer::InputCommandKind;
    if (commands.size() != 5U ||
        commands[0].kind != Kind::ShotAll ||
        commands[1].kind != Kind::ResetCar ||
        commands[2].kind != Kind::ShotMine ||
        commands[3].kind != Kind::ChangeWeapon ||
        commands[3].value != -1 ||
        commands[4].kind != Kind::ShotWeaponSlot ||
        commands[4].value != 2)
        return 8;
    if (!source::HumanPlayer::OnHandleInput(
             inputMessages, true, true, false).empty() ||
        !source::HumanPlayer::OnHandleInput(
             inputMessages, false, false, false).empty() ||
        !source::HumanPlayer::OnHandleInput(
             inputMessages, false, true, true).empty())
        return 9;

    auto driving = source::HumanPlayer::OnInputProgress(
        true, true, 0.75F, 0.5F, true, false);
    if (driving.throttle != 1.0F || driving.reverse != 0.0F ||
        std::abs(driving.steering - 0.75F) > 0.0001F ||
        !driving.manualSteering)
        return 10;
    driving = source::HumanPlayer::OnInputProgress(
        false, true, 0.0F, 0.6F);
    if (driving.throttle != 0.0F || driving.reverse != 1.0F ||
        std::abs(driving.steering + 0.6F) > 0.0001F ||
        driving.manualSteering)
        return 11;

    auto gate = source::HumanPlayer::EvaluateControl(
        false, true, false, false);
    if (!gate.inputActions || !gate.driving ||
        !gate.progressWeapons)
        return 12;
    gate = source::HumanPlayer::EvaluateControl(
        false, true, true, false);
    if (gate.inputActions || !gate.driving || gate.progressWeapons)
        return 13;
    gate = source::HumanPlayer::EvaluateControl(
        false, true, false, true);
    if (!gate.inputActions || gate.driving || gate.progressWeapons)
        return 14;
    gate = source::HumanPlayer::EvaluateControl(
        true, true, false, false);
    if (gate.inputActions || gate.driving || gate.progressWeapons)
        return 15;
    gate = source::HumanPlayer::EvaluateControl(
        false, false, false, false);
    if (gate.inputActions || gate.driving || gate.progressWeapons)
        return 16;
    if (!source::HumanPlayer::ResetCar(true, true, false) ||
        !source::HumanPlayer::ResetCar(true, false, true) ||
        source::HumanPlayer::ResetCar(true, false, false) ||
        source::HumanPlayer::ResetCar(false, true, true))
        return 17;

    std::cout << "original HumanPlayer source rules passed\n";
    return 0;
}
