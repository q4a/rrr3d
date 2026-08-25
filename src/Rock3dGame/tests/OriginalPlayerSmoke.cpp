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
    player.OnLapPass(maximumCharges.size());
    if (player.car.numLaps != 1U || player.weaponCharges[0] != 6U)
        return 27;

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
    const float immortalLife = player.life;
    player.Damage(1U, 5.0F, r3d::game::originalrace::DamageType::Energy);
    if (player.life != immortalLife ||
        player.immortalEffect.GetDamageTime() != 0.0F ||
        !player.ConsumeEnergyDamageEffectCreated())
        return 21;
    const auto immortalDamageEvents = player.TakeGameEvents();
    if (immortalDamageEvents.size() != 1U ||
        immortalDamageEvents[0].kind != source::PlayerGameEventKind::Damage ||
        immortalDamageEvents[0].otherPlayerId != 1U ||
        immortalDamageEvents[0].value != 5.0F)
        return 23;
    player.Damage(1U, 5.0F, r3d::game::originalrace::DamageType::Energy);
    if (player.ConsumeEnergyDamageEffectCreated() ||
        player.TakeGameEvents().size() != 1U)
        return 22;

    player.Complete(1U, 100U, 20U, 10.0F);
    if (!player.finished || player.place != 1U ||
        player.FinishBrake(10.29F) != 0.0F ||
        player.FinishBrake(10.3F) != 1.0F)
        return 9;
    player.SetBlockTime(source::Player::finishBlockSeconds);
    if (!player.IsBlock() ||
        player.ProgressBlock(0.1F) != source::PlayerBlockMove::Coast ||
        std::abs(player.GetBlockTime() - 0.2F) > 0.001F ||
        player.ProgressBlock(0.21F) != source::PlayerBlockMove::Brake ||
        player.GetBlockTime() != 0.0F)
        return 19;
    player.ResetBlock(false);
    if (player.IsBlock() || player.GetBlockTime() != -1.0F ||
        player.ProgressBlock(1.0F) !=
            source::PlayerBlockMove::Unblocked)
        return 20;

    source::Player progressPlayer;
    progressPlayer.Reset(80.0F, 1U);
    progressPlayer.SetBlockTime(0.2F);
    const std::vector<source::Player::CheatPlayerView> progressViews{
        {0U, true, true, 0.0F},
        {1U, false, true, 1.0F}};
    const auto progress = progressPlayer.OnProgress(
        0.1F, true, source::Player::cheatDisabled,
        0U, 1U, progressViews);
    if (progress.cheat.faster || progress.cheat.slower ||
        progress.restore != source::PlayerRestoreStep::None ||
        progress.blockMove != source::PlayerBlockMove::Coast)
        return 31;
    progressPlayer.Destroy();
    progressPlayer.car.cheatFaster = true;
    progressPlayer.car.cheatSlower = true;
    const auto destroyedProgress = progressPlayer.OnProgress(
        0.25F, false, source::Player::cheatEnableFaster,
        0U, 1U, progressViews);
    if (destroyedProgress.restore != source::PlayerRestoreStep::None ||
        progressPlayer.car.cheatFaster ||
        progressPlayer.car.cheatSlower)
        return 32;
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

    source::Player lethalPlayer;
    lethalPlayer.Reset(50.0F, 1U);
    lethalPlayer.Damage(
        2U, 60.0F, r3d::game::originalrace::DamageType::Simple);
    const auto lethalEvents = lethalPlayer.TakeGameEvents();
    if (lethalEvents.size() != 3U ||
        lethalEvents[0].kind != source::PlayerGameEventKind::Damage ||
        lethalEvents[1].kind != source::PlayerGameEventKind::Kill ||
        lethalEvents[2].kind != source::PlayerGameEventKind::Death ||
        lethalEvents[1].otherPlayerId != 2U)
        return 24;

    source::Player minePlayer;
    minePlayer.Reset(50.0F, 1U);
    minePlayer.Damage(
        3U, 60.0F, r3d::game::originalrace::DamageType::Mine);
    const auto mineEvents = minePlayer.TakeGameEvents();
    if (mineEvents.size() != 3U ||
        mineEvents[0].kind != source::PlayerGameEventKind::Damage ||
        mineEvents[1].kind != source::PlayerGameEventKind::DeathMine ||
        mineEvents[2].kind != source::PlayerGameEventKind::Death)
        return 25;

    source::Player overboardPlayer;
    overboardPlayer.Reset(50.0F, 1U);
    overboardPlayer.Damage(
        4U, 0.0F, r3d::game::originalrace::DamageType::Touch);
    overboardPlayer.TakeGameEvents();
    overboardPlayer.Death(
        r3d::game::originalrace::DamageType::DeathPlane);
    const auto overboardEvents = overboardPlayer.TakeGameEvents();
    if (overboardEvents.size() != 2U ||
        overboardEvents[0].kind !=
            source::PlayerGameEventKind::Overboard ||
        overboardEvents[1].kind != source::PlayerGameEventKind::Death ||
        overboardEvents[1].otherPlayerId != 4U)
        return 26;

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

    std::vector<source::Player::CheatPlayerView> cheatPlayers{
        {0U, true, true, tracedPlayer.car.GetLap() + 0.25F},
        {1U, false, true, tracedPlayer.car.GetLap() + 0.49F}};
    const auto faster = tracedPlayer.CheatUpdate(
        source::Player::cheatEnableFaster |
            source::Player::cheatEnableSlower,
        2U, 1U, cheatPlayers);
    if (!faster.faster || faster.slower ||
        faster.torqueScale <= 1.0F ||
        !tracedPlayer.car.cheatFaster)
        return 28;

    tracedPlayer.car.Update(
        trace, {40.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
        40.0F, 1.0F / 60.0F);
    cheatPlayers[0].lap = tracedPlayer.car.GetLap() - 0.25F;
    const auto slower = tracedPlayer.CheatUpdate(
        source::Player::cheatEnableFaster |
            source::Player::cheatEnableSlower,
        2U, 1U, cheatPlayers);
    if (!slower.slower || slower.faster ||
        !tracedPlayer.car.cheatSlower)
        return 29;

    cheatPlayers[0].lap = tracedPlayer.car.GetLap() + 0.05F;
    cheatPlayers[1].lap = tracedPlayer.car.GetLap() + 0.49F;
    const auto computersExcluded = tracedPlayer.CheatUpdate(
        source::Player::cheatEnableFaster,
        2U, 1U, cheatPlayers);
    if (computersExcluded.faster || tracedPlayer.car.cheatFaster)
        return 30;

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

    source::Trace resetTrace(4U);
    auto* resetFirst = resetTrace.AddPoint(11U);
    resetFirst->SetPos({0.0F, 0.0F, 0.0F});
    resetFirst->SetSize(8.0F);
    auto* resetSecond = resetTrace.AddPoint(12U);
    resetSecond->SetPos({100.0F, 0.0F, 0.0F});
    resetSecond->SetSize(24.0F);
    auto* resetThird = resetTrace.AddPoint(13U);
    resetThird->SetPos({200.0F, 0.0F, 0.0F});
    resetThird->SetSize(8.0F);
    auto* resetPath = resetTrace.AddPath();
    resetPath->Add(resetFirst);
    resetPath->Add(resetSecond);
    resetPath->Add(resetThird);

    source::Player resetPlayer;
    resetPlayer.Reset(100.0F, 1U, &resetTrace);
    resetPlayer.car.Update(
        resetTrace, {75.0F, 0.0F, 2.0F}, {1.0F, 0.0F, 0.0F},
        10.0F, 1.0F / 60.0F);
    std::vector<source::TraceVec3> resetRays;
    const auto resetPose = resetPlayer.ResetCar(
        [&](const source::TraceVec3& origin) {
            resetRays.push_back(origin);
            return source::ResetCarRayKind::TrackPlane;
        });
    // ResetCar uses the retained coordinate and the midpoint trace height,
    // not the height at each of the 0/-2/+2 longitudinal samples.
    if (!resetPose.valid || resetPose.node.path != 0U ||
        resetPose.node.node != 0U || resetRays.size() != 3U ||
        std::abs(resetPose.position.x - 75.0F) > 0.001F ||
        std::abs(resetPose.position.z - 4.0F) > 0.001F ||
        std::abs(resetRays[1].x - 73.0F) > 0.001F ||
        std::abs(resetRays[2].x - 77.0F) > 0.001F)
        return 23;

    std::size_t blockedRays = 0U;
    const auto blockedPose = resetPlayer.ResetCar(
        [&](const source::TraceVec3&) {
            ++blockedRays;
            return source::ResetCarRayKind::Blocked;
        });
    // Every attempted location is blocked. Windows retains the original
    // first fallback; it does not return the last failed six-metre step.
    if (!blockedPose.valid || blockedRays != 5U ||
        std::abs(blockedPose.position.x - 75.0F) > 0.001F)
        return 24;

    source::Player deathPlaneResetPlayer;
    deathPlaneResetPlayer.Reset(100.0F, 1U, &resetTrace);
    std::size_t deathPlaneRays = 0U;
    const auto deathPlanePose = deathPlaneResetPlayer.ResetCar(
        [&](const source::TraceVec3&) {
            return deathPlaneRays++ == 0U
                       ? source::ResetCarRayKind::DeathPlane
                       : source::ResetCarRayKind::TrackPlane;
        });
    if (!deathPlanePose.valid || deathPlaneRays != 4U ||
        deathPlanePose.node.node != 0U ||
        std::abs(deathPlanePose.position.x) > 0.001F ||
        std::abs(deathPlanePose.position.z - 4.0F) > 0.001F)
        return 25;

    player.Disconnect();
    if (!player.disconnected || !player.destroyed || player.finished ||
        player.life != 0.0F)
        return 18;

    std::cout << "original Player source rules passed\n";
    return 0;
}
