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
    charges[1] = 0U;
    selection = human.SelectWeapon(itemPointers);
    if (!selection.found || selection.slot != 2U ||
        human.GetCurWeapon() != 2)
        return 2;
    charges[2] = 0U;
    selection = human.SelectWeapon(itemPointers);
    if (selection.found || selection.slot != 0U ||
        human.GetCurWeapon() != 0)
        return 3;

    charges = {1U, 1U, 1U};
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

    auto driving = source::HumanPlayer::OnInputProgress(
        true, true, 0.75F, 0.5F);
    if (driving.throttle != 1.0F || driving.reverse != 0.0F ||
        std::abs(driving.steering - 0.75F) > 0.0001F)
        return 7;
    driving = source::HumanPlayer::OnInputProgress(
        false, true, 0.0F, 0.6F);
    if (driving.throttle != 0.0F || driving.reverse != 1.0F ||
        std::abs(driving.steering + 0.6F) > 0.0001F)
        return 8;

    auto gate = source::HumanPlayer::EvaluateControl(
        false, true, false, false);
    if (!gate.inputActions || !gate.driving ||
        !gate.progressWeapons)
        return 9;
    gate = source::HumanPlayer::EvaluateControl(
        false, true, true, false);
    if (gate.inputActions || !gate.driving || gate.progressWeapons)
        return 10;
    gate = source::HumanPlayer::EvaluateControl(
        false, true, false, true);
    if (!gate.inputActions || gate.driving || gate.progressWeapons)
        return 11;
    gate = source::HumanPlayer::EvaluateControl(
        true, true, false, false);
    if (gate.inputActions || gate.driving || gate.progressWeapons)
        return 12;
    gate = source::HumanPlayer::EvaluateControl(
        false, false, false, false);
    if (gate.inputActions || gate.driving || gate.progressWeapons)
        return 13;
    if (!source::HumanPlayer::ResetCar(true, true, false) ||
        !source::HumanPlayer::ResetCar(true, false, true) ||
        source::HumanPlayer::ResetCar(true, false, false) ||
        source::HumanPlayer::ResetCar(false, true, true))
        return 14;

    std::cout << "original HumanPlayer source rules passed\n";
    return 0;
}
