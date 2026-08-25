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

    source::HumanPlayer human;
    auto selection = human.SelectWeapon(items);
    if (!selection.found || selection.slot != 1U ||
        human.GetCurWeapon() != 1)
        return 1;
    charges[1] = 0U;
    selection = human.SelectWeapon(items);
    if (!selection.found || selection.slot != 2U ||
        human.GetCurWeapon() != 2)
        return 2;
    charges[2] = 0U;
    selection = human.SelectWeapon(items);
    if (selection.found || selection.slot != 0U ||
        human.GetCurWeapon() != 0)
        return 3;

    charges = {1U, 1U, 1U};
    human.ChangeWeapon(1, items);
    human.ChangeWeapon(1, items);
    human.ChangeWeapon(1, items);
    if (human.GetCurWeapon() != 2)
        return 4;
    human.ChangeWeapon(-1, items);
    if (human.GetCurWeapon() != 1 ||
        human.GetWeaponCount(items) != 3 ||
        human.GetWeaponByIndex(0, items) != 0U ||
        human.GetWeaponByIndex(2, items) != 2U ||
        human.GetWeaponByIndex(3, items) != items.size())
        return 5;
    std::array<source::WeaponItem, 3U> sparse{
        items[0], source::WeaponItem{}, items[2]};
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

    std::cout << "original HumanPlayer source rules passed\n";
    return 0;
}
