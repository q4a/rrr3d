#include "OriginalPlayer.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace source = r3d::game::originalrace::source;

int main()
{
    source::Player player;
    player.Reset(80.0F, 3U);
    if (player.life != 80.0F || player.maximumLife != 80.0F ||
        player.place != 3U || player.finished || player.destroyed)
        return 1;

    player.weaponSlots = {2U, source::Player::invalidWeapon, 4U, 5U};
    player.weaponCapacity = {6U, 0U, 3U, 2U};
    player.weaponCharges = {1U, 0U, 0U, 1U};
    player.selectedWeaponSlot = 1U;
    player.hyperWeapon = 0U;
    player.hyperCapacity = 2U;
    player.hyperCharge = 0U;
    player.mineWeapon = 1U;
    player.mineCapacity = 4U;
    player.mines = 1U;
    player.SyncSelectedWeapon(6U);
    if (player.selectedWeaponSlot != 2U || player.selectedWeapon != 4U ||
        player.ammunition != 0U)
        return 2;

    const std::vector<std::uint32_t> maximumCharges{
        10U, 8U, 6U, 4U, 3U, 2U};
    const auto hyper = player.TakeAmmunition(
        0.5F, maximumCharges, 0.0F);
    if (hyper.slot != source::PlayerBonusSlot::Hyper ||
        hyper.weapon != 0U || player.hyperCharge != 2U)
        return 3;
    const auto mine = player.TakeAmmunition(
        0.5F, maximumCharges, 0.0F);
    if (mine.slot != source::PlayerBonusSlot::Mine ||
        mine.weapon != 1U || player.mines != 4U)
        return 4;

    player.ReloadWeapons(maximumCharges.size());
    if (player.weaponCharges[0] != 6U ||
        player.weaponCharges[2] != 3U ||
        player.weaponCharges[3] != 2U ||
        player.hyperCharge != 2U || player.mines != 4U)
        return 5;

    player.life = 50.0F;
    player.TakeMedpack(7.5F);
    if (std::abs(player.life - 57.5F) > 0.001F)
        return 6;
    player.TakeMedpack(100.0F);
    if (player.life != player.maximumLife)
        return 7;
    player.TakeMoney(19.9F);
    player.TakeImmortal(4.5F);
    if (player.pickedMoney != 19U ||
        std::abs(player.shieldSeconds - 4.5F) > 0.001F ||
        player.immortalEffect.GetFadeInTime() != 0.0F)
        return 8;

    player.Complete(1U, 100U, 20U, 10.0F);
    if (!player.finished || player.place != 1U ||
        player.FinishBrake(10.29F) != 0.0F ||
        player.FinishBrake(10.3F) != 1.0F)
        return 9;
    player.ApplyRaceReward();
    if (player.money != 119U || player.points != 20U)
        return 10;

    player.Destroy();
    if (!player.destroyed || player.life != 0.0F ||
        player.ProgressRestore(1.0F) != source::PlayerRestoreStep::None ||
        player.ProgressRestore(1.0F) !=
            source::PlayerRestoreStep::QueueRespawn ||
        player.life != player.maximumLife || !player.destroyed ||
        player.ProgressRestore(0.01F) !=
            source::PlayerRestoreStep::ActivateCar ||
        player.destroyed)
        return 11;

    if (source::Player::RoundedRandomIndex(4U, 0.16F) != 0U ||
        source::Player::RoundedRandomIndex(4U, 0.5F) != 2U ||
        source::Player::BonusCharge(3U, 0.5F) != 1U ||
        source::Player::BonusCharge(10U, 0.0F) != 1U)
        return 12;

    source::Trace trace(4U);
    auto* first = trace.AddPoint(1U);
    first->SetPos({0.0F, 0.0F, 0.0F});
    first->SetSize(30.0F);
    auto* second = trace.AddPoint(2U);
    second->SetPos({100.0F, 0.0F, 0.0F});
    second->SetSize(30.0F);
    auto* third = trace.AddPoint(3U);
    third->SetPos({200.0F, 0.0F, 0.0F});
    third->SetSize(30.0F);
    auto* branchMiddle = trace.AddPoint(4U);
    branchMiddle->SetPos({100.0F, 100.0F, 0.0F});
    branchMiddle->SetSize(30.0F);
    auto* mainPath = trace.AddPath();
    mainPath->Add(first);
    mainPath->Add(second);
    mainPath->Add(third);
    auto* branchPath = trace.AddPath();
    branchPath->Add(second);
    branchPath->Add(branchMiddle);
    branchPath->Add(third);

    source::Player tracedPlayer;
    tracedPlayer.Reset(100.0F, 1U, &trace);
    const auto firstUpdate = tracedPlayer.car.Update(
        trace, {40.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
        10.0F, 1.0F / 60.0F);
    const auto firstMap = tracedPlayer.car.GetMapPos();
    if (!firstUpdate.lastNodeChanged ||
        firstUpdate.previousLast.valid() ||
        firstUpdate.lastNode.path != 0U ||
        tracedPlayer.car.GetPathIndex() != 0 ||
        std::abs(firstMap.x - 40.0F) > 0.001F)
        return 13;

    tracedPlayer.car.Update(
        trace, {75.0F, 0.0F, 0.0F}, {-1.0F, 0.0F, 0.0F},
        10.0F, 1.0F / 60.0F);
    const auto inverse = tracedPlayer.car.Update(
        trace, {50.0F, 0.0F, 0.0F}, {-1.0F, 0.0F, 0.0F},
        10.0F, 1.0F / 60.0F);
    if (!inverse.moveInverseStarted || !tracedPlayer.car.moveInverse)
        return 14;
    tracedPlayer.car.Update(
        trace, {55.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
        100.0F, 1.0F / 60.0F);
    const auto lostControl = tracedPlayer.car.Update(
        trace, {60.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
        10.0F, 1.0F / 60.0F);
    if (tracedPlayer.car.moveInverse || !lostControl.lostControl)
        return 15;

    const auto branch = tracedPlayer.car.Update(
        trace, {100.0F, 50.0F, 0.0F}, {0.0F, 1.0F, 0.0F},
        10.0F, 1.0F / 60.0F);
    if (!branch.lastNodeChanged || branch.lastNode.path != 1U ||
        tracedPlayer.car.GetPathIndex() != 1)
        return 16;
    const auto lastMap = tracedPlayer.car.GetMapPos();
    tracedPlayer.car.Update(
        trace, {1000.0F, 1000.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
        10.0F, 1.0F / 60.0F);
    const auto retainedMap = tracedPlayer.car.GetMapPos();
    if (tracedPlayer.car.GetLiveTileRef().valid() ||
        !tracedPlayer.car.GetLastNodeRef().valid() ||
        std::abs(lastMap.x - retainedMap.x) > 0.001F ||
        std::abs(lastMap.y - retainedMap.y) > 0.001F)
        return 17;

    player.Disconnect();
    if (!player.disconnected || !player.destroyed || player.finished ||
        player.life != 0.0F)
        return 18;

    std::cout << "original Player source rules passed\n";
    return 0;
}
