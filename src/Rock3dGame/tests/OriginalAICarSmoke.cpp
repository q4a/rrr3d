#include "OriginalAICar.h"

#include <cmath>
#include <iostream>

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

    std::cout << "original AICar path/control source rules passed\n";
    return 0;
}
