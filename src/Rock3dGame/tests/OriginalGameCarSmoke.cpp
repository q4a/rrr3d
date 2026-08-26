#include "OriginalGameCar.h"

#include <cmath>
#include <iostream>

namespace source = r3d::game::originalrace::source;

int main()
{
    source::GameCar car;
    if (car.IsClutchLocked() || car.IsSpringLocked() ||
        car.IsMineLocked() || car.HasSoundMotor() ||
        car.GetBehaviors().GetCount() != 0U ||
        car.GetListenerCount() != 0U)
        return 1;

    if (!car.LockClutch(1.5F, false) ||
        car.LockClutch(-2.0F, false) ||
        !car.IsClutchLocked() ||
        car.ConsumeClutchStrength() != 1.5F ||
        car.ConsumeClutchStrength() != 0.0F)
        return 2;
    car.OnProgress(0.38F);
    if (!car.IsClutchLocked())
        return 3;
    const auto clutchRelease = car.OnFixedStepDrive(0.38F, {}, {});
    if (!clutchRelease.clutchReleased || car.IsClutchLocked())
        return 3;
    if (car.LockClutch(1.0F, true))
        return 4;

    car.LockSpring();
    car.LockMine(0.4F);
    if (!car.IsSpringLocked() || !car.IsMineLocked() ||
        std::abs(car.GetSpringTime() - 1.5F) > 0.0001F ||
        std::abs(car.GetMineTime() - 0.4F) > 0.0001F)
        return 5;
    car.OnProgress(0.4F);
    if (!car.IsSpringLocked() || !car.IsMineLocked())
        return 6;
    const auto mineRelease = car.OnFixedStepDrive(0.4F, {}, {});
    if (!mineRelease.mineReleased || car.IsMineLocked() ||
        !car.IsSpringLocked() ||
        std::abs(car.GetSpringTime() - 1.1F) > 0.0001F)
        return 6;
    car.CancelClutch();
    car.Reset();
    if (car.IsClutchLocked() || car.IsSpringLocked() ||
        car.IsMineLocked())
        return 7;

    source::SoundMotor motor;
    const std::array<float, 2> volumeRange{0.2F, 0.8F};
    const std::array<float, 2> frequencyRange{0.75F, 1.5F};
    const auto idle = motor.OnMotor(
        1.0F / 60.0F, 1000.0F, 1000.0F, 6000.0F,
        volumeRange, frequencyRange);
    if (std::abs(idle.currentRpm - 166.66667F) > 0.001F ||
        idle.idleVolume != 1.0F || idle.rpmVolume != 0.0F ||
        std::abs(idle.rpmFrequencyRatio - 0.75F) > 0.0001F)
        return 8;
    const auto high = motor.OnMotor(
        1.0F, 6000.0F, 1000.0F, 6000.0F,
        volumeRange, frequencyRange);
    if (high.currentRpm != 6000.0F || high.idleVolume != 0.0F ||
        std::abs(high.rpmVolume - 0.8F) > 0.0001F ||
        std::abs(high.rpmFrequencyRatio - 1.5F) > 0.0001F)
        return 9;
    motor.Reset();
    if (motor.GetCurrentRpm() != 0.0F)
        return 10;

    car.BindSoundMotor(volumeRange, frequencyRange);
    if (!car.HasSoundMotor() ||
        car.GetBehaviors().GetCount() != 1U ||
        car.GetBehaviors().Find(
            source::BehaviorType::SoundMotor) == nullptr ||
        car.GetListenerCount() != 1U)
        return 20;
    const auto ownerIdle = car.OnMotor(
        1.0F / 60.0F, 1000.0F, 1000.0F, 6000.0F);
    if (std::abs(ownerIdle.currentRpm - 166.66667F) > 0.001F ||
        car.GetSoundMotorMix().currentRpm != ownerIdle.currentRpm)
        return 21;
    const auto ownerProgress = car.OnProgress(0.0F);
    if (ownerProgress.behaviorsProgressed != 1U ||
        ownerProgress.behaviorsRemoved != 0U)
        return 22;
    source::GameCar copiedCar = car;
    if (!copiedCar.HasSoundMotor() ||
        copiedCar.GetBehaviors().GetCount() != 1U ||
        copiedCar.GetListenerCount() != 1U ||
        copiedCar.GetSoundMotorMix().currentRpm !=
            ownerIdle.currentRpm)
        return 23;
    car.ReleaseSoundMotor();
    if (car.HasSoundMotor() ||
        car.GetBehaviors().GetCount() != 0U ||
        car.GetListenerCount() != 0U ||
        car.GetSoundMotorMix().currentRpm != 0.0F)
        return 24;

    car.ConfigureMotor({7500.0F, 3.42F, 7000.0F, 1000.0F,
                        2000.0F, 0.805F, 400.0F, 10.0F, true});
    car.BindSoundMotor(volumeRange, frequencyRange);
    const auto firstGear = car.OnFixedStepDrive(
        1.0F / 120.0F, {1.0F, 0.0F, 0.0F, 0.0F, 1.0F},
        {0.0F, 0.0F, 0.0F, 0.0F, true, true});
    const float expectedFirstGearTorque =
        2000.0F * 2.66F * 3.42F * 0.805F;
    if (firstGear.gear != 1 ||
        car.GetCurGear() != 1 ||
        car.GetMoveCar() != source::GameCar::MoveCarState::Accel ||
        std::abs(firstGear.motorTorque - expectedFirstGearTorque) >
            0.01F ||
        firstGear.brakeTorque != 400.0F)
        return 38;
    const auto shifted = car.OnFixedStepDrive(
        1.0F / 120.0F, {1.0F, 0.0F, 0.0F, 0.0F, 1.0F},
        {5.0F, 5.0F, 5.0F, 10000.0F, true, true});
    if (shifted.rpm != 7000.0F || shifted.gear != 2 ||
        shifted.motorTorque != expectedFirstGearTorque ||
        car.GetSoundMotorMix().currentRpm <= 0.0F)
        return 39;
    const auto reverseBrake = car.OnFixedStepDrive(
        1.0F / 120.0F, {0.0F, 1.0F, 0.0F, 0.0F, 1.0F},
        {2.0F, 2.0F, 2.0F, 10.0F, true, true});
    if (reverseBrake.motorTorque != 0.0F ||
        reverseBrake.brakeTorque != 7500.0F ||
        // TransmissionProgress still runs while mcBack is braking forward
        // motion, so the low driven-wheel RPM drops gear 2 to gear 1.
        reverseBrake.gear != 1)
        return 40;
    const auto reverseDrive = car.OnFixedStepDrive(
        1.0F / 120.0F, {0.0F, 1.0F, 0.0F, 0.0F, 1.0F},
        {0.0F, 0.0F, 0.0F, 0.0F, true, true});
    if (reverseDrive.gear != 0 ||
        reverseDrive.motorTorque >= 0.0F)
        return 41;
    const auto braking = car.OnFixedStepDrive(
        1.0F / 120.0F, {0.0F, 0.0F, 1.0F, 0.0F, 1.0F},
        {-2.0F, 2.0F, 2.0F, 10.0F, true, true});
    if (braking.gear != -1 || braking.brakeTorque != 7500.0F)
        return 42;
    const auto speedLimited = car.OnFixedStepDrive(
        1.0F / 120.0F, {1.0F, 0.0F, 0.0F, 0.0F, 3.0F},
        {12.0F, 12.0F, 12.0F, 100.0F, true, true});
    if (speedLimited.gear != 2 ||
        speedLimited.motorTorque != speedLimited.brakeTorque ||
        speedLimited.motorTorque != 400.0F)
        return 43;
    car.Reset();
    if (car.GetCurGear() != -1 ||
        car.GetMoveCar() != source::GameCar::MoveCarState::None)
        return 44;
    car.ReleaseSoundMotor();

    car.BindWheels({true, false}, {true, false});
    constexpr float maximumSteerAngle =
        3.14159265358979323846F / 6.0F;
    constexpr float steerSpeed =
        3.14159265358979323846F / 2.0F;
    car.ConfigureDynamics(
        {{0.5F, 0.6F, 0.7F}, 2.0F, 0.2F, 0.3F,
         maximumSteerAngle, steerSpeed,
         3.14159265358979323846F, false},
        {{-1.0F, true, false}, {1.0F, false, true}});
    const auto* firstWheel = car.GetWheel(0U);
    const auto* secondWheel = car.GetWheel(1U);
    if (car.GetWheelCount() != 2U || firstWheel == nullptr ||
        secondWheel == nullptr || firstWheel->GetParent() != &car ||
        secondWheel->GetParent() != &car ||
        !firstWheel->HasSlipEffect() ||
        !firstWheel->HasSlipSound() ||
        firstWheel->GetBehaviors().GetCount() != 1U ||
        firstWheel->GetListenerCount() != 1U ||
        firstWheel->GetBehaviors().Find(
            source::BehaviorType::PxWheelSlipEffect) == nullptr ||
        secondWheel->HasSlipEffect() ||
        secondWheel->GetBehaviors().GetCount() != 0U ||
        !firstWheel->IsDriven() || firstWheel->IsSteering() ||
        secondWheel->IsDriven() || !secondWheel->IsSteering())
        return 25;
    const auto steered = car.OnFixedStepDrive(
        1.0F / 120.0F, {0.0F, 0.0F, 0.0F, 1.0F, 1.0F},
        {5.0F, 5.0F, 5.0F, 0.0F, true, true});
    const float expectedSteering = steerSpeed / 120.0F;
    if (std::abs(steered.steeringAngle - expectedSteering) > 0.0001F ||
        steered.steeringYaw <= 0.0F || steered.rearWheelX != -1.0F ||
        std::abs(car.GetWheel(1U)->GetSteerAngle() -
                 expectedSteering) > 0.0001F ||
        steered.angularDamping != std::array<float, 3U>{0.5F, 0.6F, 0.7F} ||
        steered.applyExtraGravity)
        return 45;
    const auto airborne = car.OnFixedStepDrive(
        1.0F / 120.0F, {},
        {5.0F, 5.0F, 2.0F, 0.0F, false, false});
    if (!airborne.applyExtraGravity ||
        airborne.airbornePitchAcceleration != 2.0F ||
        airborne.clampRollAngle != 0.2F ||
        airborne.clampPitchAngle != 0.3F)
        return 46;
    car.LockSpring();
    const auto springLocked = car.OnFixedStepDrive(
        1.0F / 120.0F, {},
        {5.0F, 5.0F, 2.0F, 0.0F, false, false});
    if (!springLocked.applyExtraGravity ||
        springLocked.airbornePitchAcceleration != 0.0F)
        return 47;
    car.Reset();
    if (!car.LockClutch(0.0F, false))
        return 48;
    const auto clutchSteering = car.OnFixedStepDrive(
        1.0F / 120.0F, {0.0F, 0.0F, 0.0F, 1.0F, 1.0F},
        {5.0F, 5.0F, 5.0F, 0.0F, true, true});
    if (clutchSteering.steeringYaw != 0.0F ||
        clutchSteering.angularDamping[2U] != 1.0F)
        return 49;
    car.Reset();
    if (!car.SetWheelContact(0U, true, -0.65F, 0.9F) ||
        !car.SetWheelContact(1U, true, 5.0F, 5.0F) ||
        car.SetWheelContact(2U, true, 1.0F, 1.0F))
        return 26;
    const auto wheelProgress = car.OnProgress(1.0F / 60.0F);
    const auto ownedSlip = car.GetWheelSlipResult(0U);
    if (wheelProgress.wheelsProgressed != 2U ||
        wheelProgress.wheelBehaviorsProgressed != 1U ||
        !ownedSlip.active || !ownedSlip.makeEffect ||
        !ownedSlip.playSound || ownedSlip.volume != 1.0F ||
        car.GetWheelSlipResult(1U).active)
        return 27;
    source::GameCar copiedWheelCar = car;
    if (copiedWheelCar.GetWheelCount() != 2U ||
        copiedWheelCar.GetWheel(0U) == nullptr ||
        copiedWheelCar.GetWheel(0U)->GetParent() != &copiedWheelCar ||
        !copiedWheelCar.GetWheel(0U)->HasSlipEffect() ||
        copiedWheelCar.GetWheel(0U)->GetListenerCount() != 1U ||
        !copiedWheelCar.GetWheelSlipResult(0U).active)
        return 28;
    constexpr float halfQuarterTurn = 0.70710678118654752440F;
    const source::GameObjectFrameSync::Pose physicalBody{
        {}, {0.0F, 0.0F, halfQuarterTurn, halfQuarterTurn}};
    const source::GameObjectFrameSync::Pose graphBody{};
    const source::GameObjectFrameSync::Pose physicalWheel{
        {0.0F, 1.0F, 0.0F},
        {0.0F, 0.0F, halfQuarterTurn, halfQuarterTurn}};
    const auto& wheelPose = car.GetWheel(0U)->PxSyncWheel(
        physicalBody, graphBody, physicalWheel);
    if (std::abs(wheelPose.position.x - 1.0F) > 0.0001F ||
        std::abs(wheelPose.position.y) > 0.0001F ||
        std::abs(wheelPose.rotation.z) > 0.0001F ||
        std::abs(wheelPose.rotation.w - 1.0F) > 0.0001F)
        return 35;
    car.GetFrameSync().Reset();
    car.GetFrameSync().SetPosSync2(
        {2.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F});
    const auto syncedCar = car.OnPxSync(
        {}, {{{1.0F, 0.0F, 0.0F}, {}},
             {{-1.0F, 0.0F, 0.0F}, {}}},
        0.0F);
    if (std::abs(syncedCar.body.position.x - 2.0F) > 0.0001F ||
        syncedCar.wheels.size() != 2U ||
        std::abs(syncedCar.wheels[0].position.x - 3.0F) > 0.0001F ||
        std::abs(syncedCar.wheels[1].position.x - 1.0F) > 0.0001F)
        return 36;
    car.Reset();
    if (car.GetFrameSync().HasActiveCorrection())
        return 37;
    car.ReleaseWheels();
    if (car.GetWheelCount() != 0U ||
        !car.GetChildren().empty())
        return 29;

    car.BindAnimationChildren(true, 2U);
    const auto* trackChild = car.GetAnimationChild(0U);
    const auto* cushionChild = car.GetAnimationChild(1U);
    if (car.GetAnimationChildCount() != 2U ||
        trackChild == nullptr || cushionChild == nullptr ||
        trackChild->GetParent() != &car ||
        cushionChild->GetParent() != &car ||
        !trackChild->HasTrackAnimation() ||
        trackChild->GetBehaviors().GetCount() != 1U ||
        trackChild->GetBehaviors().Find(
            source::BehaviorType::GusenizaAnim) == nullptr ||
        cushionChild->GetCushionAnimationCount() != 2U ||
        cushionChild->GetBehaviors().GetCount() != 2U ||
        cushionChild->GetListenerCount() != 2U ||
        cushionChild->GetBehaviors().Find(
            source::BehaviorType::PodushkaAnim) == nullptr)
        return 30;
    car.SetLeadWheelSpeed(5.0F);
    const auto animationProgress = car.OnProgress(0.5F);
    if (animationProgress.animationChildrenProgressed != 2U ||
        animationProgress.animationBehaviorsProgressed != 3U ||
        std::abs(car.GetTrackTextureOffset() - 0.5F) > 0.0001F ||
        std::abs(car.GetCushionAngle(0U) -
                 0.25F * 3.14159265358979323846F) > 0.0001F ||
        car.GetCushionAngle(0U) != car.GetCushionAngle(1U))
        return 31;
    car.SetLeadWheelSpeed(0.1F);
    if (car.GetLeadWheelSpeed() != 0.0F)
        return 32;
    source::GameCar copiedAnimationCar = car;
    if (copiedAnimationCar.GetAnimationChildCount() != 2U ||
        copiedAnimationCar.GetAnimationChild(0U)->GetParent() !=
            &copiedAnimationCar ||
        copiedAnimationCar.GetAnimationChild(1U)->GetListenerCount() != 2U ||
        copiedAnimationCar.GetTrackTextureOffset() !=
            car.GetTrackTextureOffset() ||
        copiedAnimationCar.GetCushionAngle(1U) !=
            car.GetCushionAngle(1U))
        return 33;
    car.ReleaseAnimationChildren();
    if (car.GetAnimationChildCount() != 0U ||
        !car.GetChildren().empty())
        return 34;

    source::PxWheelSlipEffect slip;
    const auto quiet = slip.OnProgress(true, 0.4F, 0.7F, true);
    if (quiet.active || quiet.makeEffect || !quiet.stopSound)
        return 11;
    const auto skidding = slip.OnProgress(true, -0.65F, 0.9F, true);
    if (!skidding.active || !skidding.makeEffect ||
        !skidding.playSound || skidding.freeEffect ||
        std::abs(skidding.slip - 0.45F) > 0.0001F ||
        skidding.volume != 1.0F || !slip.IsEffectMaked())
        return 12;
    const auto continued = slip.OnProgress(true, 0.5F, 0.7F, true);
    if (!continued.active || continued.makeEffect ||
        std::abs(continued.volume - 0.4F) > 0.0001F)
        return 13;
    const auto released = slip.OnProgress(false, 5.0F, 5.0F, true);
    if (released.active || !released.freeEffect ||
        !released.stopSound || slip.IsEffectMaked())
        return 14;

    source::GusenizaAnim tracks;
    if (tracks.GetTextureOffset() != 1.0F ||
        std::abs(tracks.OnProgress(0.5F, 5.0F) - 0.5F) >
            0.0001F ||
        std::abs(tracks.OnProgress(0.5F, -5.0F) - 1.0F) >
            0.0001F)
        return 15;
    tracks.Reset();
    if (tracks.GetTextureOffset() != 1.0F)
        return 16;

    source::PodushkaAnim cushion;
    if (cushion.OnProgress(1.0F, 1.0F) != 0.0F ||
        std::abs(cushion.OnProgress(1.0F, 2.0F) -
                 0.2F * 3.14159265358979323846F) > 0.0001F)
        return 17;
    const float angle = cushion.GetAngle();
    if (cushion.OnProgress(1.0F, -0.5F) != angle)
        return 18;
    cushion.Reset();
    if (cushion.GetAngle() != 0.0F)
        return 19;

    std::cout << "original GameCar lock, SoundMotor and wheel-slip "
                 "animation rules passed\n";
    return 0;
}
