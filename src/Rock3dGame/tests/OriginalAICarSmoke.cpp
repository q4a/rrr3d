#include "OriginalAICar.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

namespace
{

float randomValue = 1.0F;
double uniformRandomValue = 0.0;

float testRandom() { return randomValue; }
double testUniformRandom() { return uniformRandomValue; }

} // namespace

int main()
{
    namespace source = r3d::game::originalrace::source;

    source::Trace trace(4U);
    auto* start = trace.AddPoint(1U);
    start->SetPos({0.0F, 0.0F, 0.0F});
    start->SetSize(40.0F);
    auto* turn = trace.AddPoint(2U);
    turn->SetPos({100.0F, 0.0F, 0.0F});
    turn->SetSize(40.0F);
    auto* finish = trace.AddPoint(3U);
    finish->SetPos({100.0F, 100.0F, 0.0F});
    finish->SetSize(40.0F);
    auto* path = trace.AddPath();
    path->Add(start);
    path->Add(turn);
    path->Add(finish);

    source::Player player;
    player.Reset(100.0F, 1U, &trace);
    player.car.Update(
        trace, {40.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
        20.0F, 1.0F / 60.0F);

    source::AICar ai(4U);
    source::AICar::VehicleState vehicle;
    vehicle.position = {40.0F, 0.0F, 0.0F};
    vehicle.direction = {1.0F, 0.0F, 0.0F};
    vehicle.speed = 20.0F;
    vehicle.size = 4.0F;
    vehicle.steeringControl = 0.1F;
    auto command = ai.Update(
        1.0F / 60.0F, player.car, vehicle);
    if (ai.path.curTile == nullptr || ai.path.nextTile == nullptr ||
        std::abs(ai.path.moveDir.x) < 0.1F ||
        command.move != source::AICar::MoveCarState::Accelerate)
        return 1;

    ai.path.ResetTracks();
    ai.path.LockTrack(1U);
    std::uint32_t unlocked = 0U;
    if (!ai.path.FindFirstUnlockTrack(0U, 3U, unlocked) ||
        unlocked != 2U)
        return 2;
    unlocked = 0U;
    if (ai.path.FindLastUnlockTrack(0U, 3U, unlocked) ||
        unlocked != 0U)
        return 3;
    ai.path.LockTrack(0U);
    if (!ai.path.FindLastSiblingUnlock(0U, unlocked) ||
        unlocked != 2U)
        return 4;

    ai.path.ResetTracks();
    vehicle.position = {60.0F, 0.0F, 0.0F};
    vehicle.speed = 120.0F;
    player.car.Update(
        trace, vehicle.position, vehicle.direction, vehicle.speed,
        1.0F / 60.0F);
    command = ai.Update(
        1.0F / 60.0F, player.car, vehicle);
    if (!ai.path.brake ||
        command.move != source::AICar::MoveCarState::Brake)
        return 5;

    vehicle.position = {1000.0F, 1000.0F, 0.0F};
    vehicle.speed = 10.0F;
    player.car.Update(
        trace, vehicle.position, vehicle.direction, vehicle.speed,
        1.0F / 60.0F);
    ai.Update(1.0F / 60.0F, player.car, vehicle);
    if (player.car.GetLiveTile() != nullptr ||
        ai.path.curTile != player.car.GetLastNode())
        return 6;

    ai.Reset(4U);
    vehicle.position = {40.0F, 0.0F, 0.0F};
    vehicle.speed = 0.0F;
    player.car.Update(
        trace, vehicle.position, vehicle.direction, vehicle.speed,
        1.0F / 60.0F);
    command = ai.Update(0.6F, player.car, vehicle);
    if (command.move != source::AICar::MoveCarState::Accelerate)
        return 7;
    command = ai.Update(0.5F, player.car, vehicle);
    if (!ai.control.blocking || !ai.control.backMoving ||
        command.move != source::AICar::MoveCarState::Reverse)
        return 8;
    command = ai.Update(0.51F, player.car, vehicle);
    if (ai.control.backMoving ||
        command.move != source::AICar::MoveCarState::Accelerate)
        return 9;
    command = ai.Update(1.4F, player.car, vehicle);
    if (!command.resetCar || !ai.TakeResetCar() || ai.TakeResetCar())
        return 10;

    ai.Reset(4U);
    vehicle.position = {40.0F, 0.0F, 0.0F};
    vehicle.direction = {1.0F, 0.0F, 0.0F};
    vehicle.direction3 = vehicle.direction;
    vehicle.speed = 20.0F;
    player.car.Update(
        trace, vehicle.position, vehicle.direction, vehicle.speed,
        1.0F / 60.0F);
    ai.path.Update(1.0F / 60.0F, player.car, vehicle);
    std::array<source::AICar::AttackTarget, 4> targets{};
    targets[0] = {vehicle.position, 4.0F, 2.0F, true};
    targets[1] = {{60.0F, 0.0F, 0.0F}, 4.0F, 2.0F, true};
    targets[2] = {{20.0F, 0.0F, 0.0F}, 4.0F, 2.0F, true};
    targets[3] = {{58.0F, 0.0F, 0.0F}, 4.0F, 2.0F, false};
    std::array<source::AICar::AttackWeapon, 1> weapons{{
        {0U, 0U, 100.0F, 10U, 10U, true}}};
    source::AICar::AttackContext attack;
    attack.owner = 0U;
    attack.targets = targets;
    attack.weapons = weapons;
    attack.randomSource = &testRandom;
    attack.uniformRandomSource = &testUniformRandom;
    randomValue = 1.0F;
    auto attackDecision = ai.attack.Update(
        player.car, vehicle, ai.path, attack);
    if (!attackDecision.hasWeaponShot() ||
        attackDecision.weaponSlot != 0U ||
        attackDecision.weaponTarget != 1U ||
        ai.attack.target != 1U || ai.attack.backTarget != 2U)
        return 11;

    // FindEnemy retains the current target when a newly closest candidate
    // is within the current car's full source size.
    targets[3].active = true;
    attackDecision = ai.attack.Update(
        player.car, vehicle, ai.path, attack);
    if (ai.attack.target != 1U ||
        attackDecision.weaponTarget != 1U)
        return 12;

    // An unready installed ordinary weapon aborts ShotByEnemy before any
    // other slot can fire.
    weapons[0].ready = false;
    attackDecision = ai.attack.Update(
        player.car, vehicle, ai.path, attack);
    if (attackDecision.hasWeaponShot())
        return 13;

    // ptTorpeda is the source exception which can fire at the retained
    // target behind the car.
    weapons[0].ready = true;
    weapons[0].projectileType = 2U;
    targets[1].active = false;
    targets[3].active = false;
    attackDecision = ai.attack.Update(
        player.car, vehicle, ai.path, attack);
    if (!attackDecision.hasWeaponShot() ||
        attackDecision.weaponTarget != 2U)
        return 14;

    attack.weapons = {};
    attack.hyper = {true, 10.0F, 10U, 10U};
    ai.path.brake = false;
    attackDecision = ai.attack.Update(
        player.car, vehicle, ai.path, attack);
    if (!attackDecision.useHyper)
        return 15;
    vehicle.position = {90.0F, 0.0F, 0.0F};
    player.car.Update(
        trace, vehicle.position, vehicle.direction, vehicle.speed,
        1.0F / 60.0F);
    ai.path.Update(1.0F / 60.0F, player.car, vehicle);
    ai.path.brake = false;
    attackDecision = ai.attack.Update(
        player.car, vehicle, ai.path, attack);
    if (attackDecision.useHyper)
        return 16;

    ai.attack.Reset();
    attack.hyper = {};
    attack.mine = {true, false, 3U, 3U};
    randomValue = 1.0F;
    attackDecision = ai.attack.Update(
        player.car, vehicle, ai.path, attack);
    if (!attackDecision.useMine ||
        std::abs(ai.attack.placeMineRandom) > 0.0001F)
        return 17;
    ai.attack.target = 1U;
    ai.attack.backTarget = 2U;
    ai.attack.DisposeTarget(2U);
    if (ai.attack.target != 1U ||
        ai.attack.backTarget != source::AICar::invalidIndex)
        return 18;

    // AISystem::ComputeTracks uses the source WayNode geometry directly.
    // Three longitudinally-overlapping cars form one ordered lane chain;
    // an isolated fourth car is still reset but reserves no lane.
    std::array<source::Player, 4> lanePlayers;
    std::array<source::AICar, 4> laneCars{
        source::AICar(4U), source::AICar(4U),
        source::AICar(4U), source::AICar(4U)};
    const std::array<source::TraceVec3, 4> lanePositions{{
        {40.0F, 15.0F, 0.0F},
        {42.0F, 5.0F, 0.0F},
        {44.0F, -5.0F, 0.0F},
        {70.0F, 0.0F, 0.0F}}};
    std::array<source::AISystem::Entry, 4> laneEntries{};
    for (std::size_t index = 0U; index < lanePlayers.size(); ++index)
    {
        lanePlayers[index].Reset(100.0F, 1U, &trace);
        lanePlayers[index].car.Update(
            trace, lanePositions[index], {1.0F, 0.0F, 0.0F},
            20.0F, 1.0F / 60.0F);
        laneEntries[index] = {
            index, &laneCars[index], &lanePlayers[index].car,
            lanePositions[index], 3.0F, true};
    }
    laneCars[3].path.LockTrack(0U);
    source::AISystem aiSystem(4U);
    aiSystem.ComputeTracks(laneEntries);
    const auto isLocked = [&](std::size_t car, std::size_t track) {
        return laneCars[car].path.lockTracks[track];
    };
    if (isLocked(0U, 0U) || !isLocked(0U, 1U) ||
        !isLocked(0U, 2U) || isLocked(0U, 3U))
        return 19;
    if (!isLocked(1U, 0U) || isLocked(1U, 1U) ||
        !isLocked(1U, 2U) || isLocked(1U, 3U))
        return 20;
    if (!isLocked(2U, 0U) || !isLocked(2U, 1U) ||
        isLocked(2U, 2U) || isLocked(2U, 3U))
        return 21;
    if (std::any_of(
            laneCars[3].path.lockTracks.begin(),
            laneCars[3].path.lockTracks.end(),
            [](bool locked) { return locked; }))
        return 22;

    lanePlayers[0].ConfigureIdentity(
        1, 1, 0U, "Computer", "", {1.0F, 1.0F, 1.0F, 1.0F});
    lanePlayers[1].ConfigureIdentity(
        source::Player::humanId, 0, 0U, "Human", "",
        {1.0F, 1.0F, 1.0F, 1.0F});
    source::AIPlayer computerOwner(&lanePlayers[0], false, 4U);
    source::AIPlayer humanOwner(&lanePlayers[1], true, 4U);
    if (computerOwner.HasCar() ||
        computerOwner.GetCheat() !=
            (source::AIPlayer::cheatEnableFaster |
             source::AIPlayer::cheatEnableSlower) ||
        humanOwner.GetCheat() != source::AIPlayer::cheatDisabled)
        return 23;
    if (lanePlayers[0].GetCheat() != computerOwner.GetCheat() ||
        lanePlayers[1].GetCheat() != source::Player::cheatDisabled)
        return 28;
    computerOwner.CreateCar();
    computerOwner.SetEnabled(false);
    vehicle.position = lanePositions[0];
    vehicle.speed = 20.0F;
    command = computerOwner.OnProgress(
        1.0F / 60.0F, vehicle, &testRandom);
    if (!computerOwner.HasCar() || computerOwner.GetCar() == nullptr ||
        command.move != source::AICar::MoveCarState::None)
        return 24;
    computerOwner.SetEnabled(true);
    command = computerOwner.OnProgress(
        1.0F / 60.0F, vehicle, &testRandom);
    if (command.move != source::AICar::MoveCarState::Accelerate)
        return 25;
    computerOwner.FreeCar();
    if (computerOwner.HasCar() || computerOwner.GetCar() != nullptr ||
        computerOwner.OnProgress(
            1.0F / 60.0F, vehicle, &testRandom).move !=
            source::AICar::MoveCarState::None)
        return 26;
    computerOwner.Reset(nullptr, false, 4U);
    if (lanePlayers[0].GetCheat() != source::Player::cheatDisabled)
        return 29;

    std::cout << "original AIPlayer/AICar/AISystem source rules passed\n";
    return 0;
}
