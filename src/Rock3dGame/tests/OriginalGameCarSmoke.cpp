#include "OriginalGameCar.h"
#include "OriginalLogic.h"
#include "OriginalRace.h"

#include <cmath>
#include <iostream>

namespace source = r3d::game::originalrace::source;

int main()
{
    source::GameCar car;
    source::GameObject ordinaryObject;
    const source::GameObject& genericCar = car;
    if (car.IsCar() != &car || genericCar.IsCar() != &car ||
        ordinaryObject.IsCar() != nullptr)
        return 69;
    source::WorldEventPump bridgeWorld;
    source::Logic bridgeLogic;
    bridgeLogic.AttachWorld(&bridgeWorld);
    source::GameCar bridgeCar;
    bridgeCar.SetLogic(&bridgeLogic);
    if (bridgeCar.GetFixedStepEventCount() != 1U ||
        !bridgeWorld.HasFixedStepEvent(&bridgeCar))
        return 71;
    bridgeCar.ConfigureMotor(
        {7500.0F, 3.42F, 7000.0F, 1000.0F,
         2000.0F, 0.805F, 400.0F, 10.0F, true});
    const auto bridgeDrive = bridgeCar.DispatchFixedStepDrive(
        bridgeWorld, 1.0F / 120.0F,
        {1.0F, 0.0F, 0.0F, 0.0F, false},
        {0.0F, 0.0F, 0.0F, 0.0F,
         true, true, true, false});
    if (bridgeDrive.motorTorque <= 0.0F || bridgeDrive.gear != 1)
        return 72;
    const source::GameObjectFrameSync::Pose bridgePose{
        {2.0F, 3.0F, 0.0F}, {0.0F, 0.0F, 0.0F, 1.0F}};
    bridgeCar.SynchronizePhysicsState(
        bridgePose, {1.0F, 0.0F, 0.0F}, true);
    if (!bridgeCar.IsBodyProgressEvent() ||
        bridgeCar.GetLateProgressEventCount() != 1U ||
        bridgeCar.GetFrameEventCount() != 1U ||
        !bridgeWorld.HasLateProgressEvent(&bridgeCar) ||
        !bridgeWorld.HasFrameEvent(&bridgeCar))
        return 73;
    const std::vector<source::GameCar::PxSyncPose> noBridgeWheels;
    const auto bridgeFrame = bridgeCar.DispatchPxSync(
        bridgeWorld, bridgePose, noBridgeWheels, 1.0F / 60.0F);
    if (std::abs(bridgeFrame.body.position.x - 2.0F) > 0.0001F ||
        std::abs(bridgeFrame.body.position.y - 3.0F) > 0.0001F)
        return 74;
    if (!bridgeCar.SynchronizeNetworkPose(
             bridgePose.position, bridgePose.position,
             bridgePose.rotation, {8.0F, 3.0F, 0.0F},
             bridgePose.rotation).snapPosition ||
        !bridgeCar.IsSyncFrameEvent() ||
        bridgeCar.GetFrameEventCount() != 2U)
        return 75;
    bridgeCar.SynchronizePhysicsState(
        bridgePose, {}, false);
    if (bridgeCar.IsBodyProgressEvent() ||
        bridgeCar.GetLateProgressEventCount() != 0U ||
        bridgeCar.GetFrameEventCount() != 1U ||
        bridgeWorld.HasLateProgressEvent(&bridgeCar) ||
        !bridgeWorld.HasFrameEvent(&bridgeCar))
        return 76;
    bridgeCar.SetLogic(nullptr);
    if (bridgeWorld.FixedStepEventCount() != 0U ||
        bridgeWorld.LateProgressEventCount() != 0U ||
        bridgeWorld.FrameEventCount() != 0U)
        return 77;
    source::GameCar lethalCar;
    lethalCar.ResetGameObject(10.0F);
    const auto carKill = lethalCar.Damage(
        3U, 10.0F,
        r3d::game::originalrace::DamageType::Simple);
    source::GameCar mineKilledCar;
    mineKilledCar.ResetGameObject(10.0F);
    const auto mineKill = mineKilledCar.Damage(
        3U, 10.0F,
        r3d::game::originalrace::DamageType::Mine);
    if (!carKill.death || !carKill.killCredit ||
        !mineKill.death || mineKill.killCredit)
        return 70;
    if (car.IsClutchLocked() || car.IsSpringLocked() ||
        car.IsMineLocked() || car.HasSoundMotor() ||
        car.GetBehaviors().GetCount() != 0U ||
        car.GetListenerCount() != 0U)
        return 1;

    if (!car.LockClutch(1.5F) ||
        car.LockClutch(-2.0F) ||
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
    car.SetClutchImmunity(true);
    if (car.LockClutch(1.0F))
        return 4;
    car.SetClutchImmunity(false);

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
    if (car.GetMotorDesc().maximumTorque != 2000.0F ||
        car.GetMaxSpeed() != 10.0F)
        return 57;
    car.SetCurGear(99);
    if (car.GetCurGear() != 5 || car.GearUp() != 5 ||
        car.GearDown() != 4)
        return 58;
    car.SetCurGear(-99);
    car.BindSoundMotor(volumeRange, frequencyRange);
    const auto firstGear = car.OnFixedStepDrive(
        1.0F / 120.0F, {1.0F, 0.0F, 0.0F, 0.0F},
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
    if (car.GearUp() != 2 || car.GearDown() != 1)
        return 59;
    const auto shifted = car.OnFixedStepDrive(
        1.0F / 120.0F, {1.0F, 0.0F, 0.0F, 0.0F},
        {5.0F, 5.0F, 5.0F, 10000.0F, true, true});
    if (shifted.rpm != 7000.0F || shifted.gear != 2 ||
        shifted.motorTorque != expectedFirstGearTorque ||
        car.GetSoundMotorMix().currentRpm <= 0.0F)
        return 39;
    car.SetMotorTorqueK(3.0F);
    car.SetWheelSteerK(2.5F);
    const auto boosted = car.OnFixedStepDrive(
        1.0F / 120.0F, {1.0F, 0.0F, 0.0F, 0.0F},
        {5.0F, 5.0F, 5.0F, 100.0F, true, true});
    const float expectedSecondGearTorque =
        2000.0F * 1.78F * 3.42F * 0.805F;
    if (boosted.gear != 2 ||
        std::abs(boosted.motorTorque -
                 expectedSecondGearTorque * 3.0F) > 0.01F ||
        boosted.lateralGripScale != 2.5F)
        return 53;
    car.SetMotorTorqueK(1.0F);
    car.SetWheelSteerK(1.0F);
    const auto reverseBrake = car.OnFixedStepDrive(
        1.0F / 120.0F, {0.0F, 1.0F, 0.0F, 0.0F},
        {2.0F, 2.0F, 2.0F, 10.0F, true, true});
    if (reverseBrake.motorTorque != 0.0F ||
        reverseBrake.brakeTorque != 7500.0F ||
        // TransmissionProgress still runs while mcBack is braking forward
        // motion, so the low driven-wheel RPM drops gear 2 to gear 1.
        reverseBrake.gear != 1)
        return 40;
    const auto reverseDrive = car.OnFixedStepDrive(
        1.0F / 120.0F, {0.0F, 1.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, 0.0F, 0.0F, true, true});
    if (reverseDrive.gear != 0 ||
        reverseDrive.motorTorque >= 0.0F)
        return 41;
    const auto braking = car.OnFixedStepDrive(
        1.0F / 120.0F, {0.0F, 0.0F, 1.0F, 0.0F},
        {-2.0F, 2.0F, 2.0F, 10.0F, true, true});
    if (braking.gear != -1 || braking.brakeTorque != 7500.0F)
        return 42;
    car.SetMotorTorqueK(3.0F);
    car.SetWheelSteerK(2.5F);
    const auto speedLimited = car.OnFixedStepDrive(
        1.0F / 120.0F, {1.0F, 0.0F, 0.0F, 0.0F},
        {12.0F, 12.0F, 12.0F, 100.0F, true, true});
    if (speedLimited.gear != 2 ||
        speedLimited.motorTorque != speedLimited.brakeTorque ||
        speedLimited.motorTorque != 400.0F ||
        speedLimited.lateralGripScale != 2.5F ||
        car.GetMotorTorqueK() != 3.0F ||
        car.GetWheelSteerK() != 2.5F)
        return 43;
    car.Reset();
    if (car.GetCurGear() != -1 ||
        car.GetMoveCar() != source::GameCar::MoveCarState::None ||
        car.GetMotorTorqueK() != 1.0F ||
        car.GetWheelSteerK() != 1.0F)
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
         3.14159265358979323846F, false,
         0.12F, false, 2.5F, true},
        {{-1.0F, true, false, false, 0.5F,
          {0.25F, 0.5F, 0.75F}},
         {1.0F, false, true, true, 0.4F,
          {-0.25F, -0.5F, -0.75F}}});
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
        secondWheel->IsDriven() || !secondWheel->IsSteering() ||
        !firstWheel->GetLead() || firstWheel->GetSteer() ||
        secondWheel->GetLead() || !secondWheel->GetSteer() ||
        firstWheel->GetInvertWheel() ||
        !secondWheel->GetInvertWheel() ||
        firstWheel->GetOffset() !=
            std::array<float, 3U>{0.25F, 0.5F, 0.75F} ||
        secondWheel->GetOffset() !=
            std::array<float, 3U>{-0.25F, -0.5F, -0.75F} ||
        car.GetLeadWheelCount() != 1U ||
        car.GetLeadWheel(0U) != firstWheel ||
        car.GetLeadWheel(1U) != nullptr ||
        car.GetSteerWheelCount() != 1U ||
        car.GetSteerGroupWheel(0U) != secondWheel ||
        car.GetSteerGroupWheel(1U) != nullptr ||
        firstWheel->GetRadius() != 0.5F ||
        secondWheel->GetRadius() != 0.4F ||
        car.GetKSteerControl() != 0.12F ||
        car.GetSteerSpeed() != steerSpeed ||
        car.GetSteerRot() != 3.14159265358979323846F ||
        car.GetAngDamping() !=
            std::array<float, 3U>{0.5F, 0.6F, 0.7F} ||
        car.GetFlyYTorque() != 2.0F ||
        car.GetClampXTorque() != 0.2F ||
        car.GetClampYTorque() != 0.3F ||
        car.IsGravEngine() || car.IsClutchImmunity() ||
        car.GetTireSpring() != 2.5F || !car.GetDisableColor())
        return 25;
    car.SetSteerWheelAngle(100.0F);
    if (car.GetSteerWheelAngle() != maximumSteerAngle)
        return 60;
    car.SetSteerWheelAngle(0.0F);
    car.SetKSteerControl(0.2F);
    car.SetSteerSpeed(steerSpeed);
    car.SetSteerRot(3.14159265358979323846F);
    car.SetAngDamping({0.5F, 0.6F, 0.7F});
    car.SetFlyYTourque(2.0F);
    car.SetClampXTourque(0.2F);
    car.SetClampYTourque(0.3F);
    car.SetGravEngine(false);
    car.SetTireSpring(2.5F);
    car.SetDisableColor(true);
    if (car.GetKSteerControl() != 0.2F)
        return 61;
    car.SetKSteerControl(0.12F);
    car.OnFixedStepDrive(
        1.0F / 120.0F, {1.0F, 0.0F, 0.0F, 0.0F},
        {2.0F, 2.0F, 2.0F, 6.0F, true, true});
    car.GetWheel(0U)->SetAxleSpeed(6.0F);
    car.GetWheel(1U)->SetAxleSpeed(-4.0F);
    constexpr float expectedSourceRpm =
        6.0F * 2.66F * 3.42F * 60.0F /
        6.28318530717958647692F;
    car.SynchronizeSpeed(-12.0F);
    if (car.GetSpeed() != -12.0F || car.GetLeadWheelSpeed() != 3.0F ||
        std::abs(car.GetDrivenWheelSpeed() + 1.6F) > 0.0001F ||
        std::abs(car.GetRPM() - expectedSourceRpm) > 0.01F)
        return 55;
    car.SynchronizeSpeed(0.99F);
    car.GetWheel(0U)->SetAxleSpeed(0.2F);
    car.GetWheel(1U)->SetAxleSpeed(0.2F);
    if (car.GetSpeed() != 0.0F || car.GetLeadWheelSpeed() != 0.0F ||
        car.GetDrivenWheelSpeed() != 0.0F)
        return 56;
    const auto steered = car.OnFixedStepDrive(
        1.0F / 120.0F, {0.0F, 0.0F, 0.0F, 1.0F},
        {5.0F, 5.0F, 5.0F, 0.0F, true, true});
    const float expectedSteering = steerSpeed / 120.0F;
    if (std::abs(steered.steeringAngle - expectedSteering) > 0.0001F ||
        steered.steeringYaw <= 0.0F || steered.rearWheelX != -1.0F ||
        car.GetSteerWheel() !=
            source::GameCar::SteerWheelState::OnLeft ||
        car.GetSteerWheelAngle() != steered.steeringAngle ||
        std::abs(car.GetWheel(1U)->GetSteerAngle() -
                 expectedSteering) > 0.0001F ||
        steered.angularDamping != std::array<float, 3U>{0.5F, 0.6F, 0.7F} ||
        steered.applyExtraGravity)
        return 45;
    const auto aiFullLock = car.OnFixedStepDrive(
        1.0F / 120.0F, {0.0F, 0.0F, 0.0F, -1.0F, true},
        {5.0F, 5.0F, 5.0F, 0.0F, true, true});
    if (aiFullLock.steeringAngle != -maximumSteerAngle ||
        car.GetSteerWheel() !=
            source::GameCar::SteerWheelState::Manual ||
        car.GetSteerWheelAngle() != -maximumSteerAngle)
        return 54;
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
    if (!car.LockClutch(0.0F))
        return 48;
    const auto clutchSteering = car.OnFixedStepDrive(
        1.0F / 120.0F, {0.0F, 0.0F, 0.0F, 1.0F},
        {5.0F, 5.0F, 5.0F, 0.0F, true, true});
    if (clutchSteering.steeringYaw != 0.0F ||
        clutchSteering.angularDamping[2U] != 1.0F)
        return 49;
    car.Reset();
    if (!car.SetWheelContact(
            0U, true, -0.65F, 0.9F, 1.25F, 0.0125F) ||
        !car.SetWheelContact(1U, true, 5.0F, 5.0F) ||
        car.SetWheelContact(2U, true, 1.0F, 1.0F))
        return 26;
    car.UpdateContactState(true);
    if (!car.IsAnyWheelContact() || !car.IsWheelsContact() ||
        !car.IsBodyContact() ||
        std::abs(car.GetWheel(0U)->GetNormalReaction() - 1.25F) >
            0.0001F ||
        std::abs(car.GetWheel(0U)->GetNormalImpulse() - 0.0125F) >
            0.0001F ||
        std::abs(car.GetWheel(0U)->GetLongSlip() + 0.65F) >
            0.0001F ||
        std::abs(car.GetWheel(0U)->GetLatSlip() - 0.9F) >
            0.0001F)
        return 50;
    car.SetWheelContact(1U, false, 0.0F, 0.0F);
    car.UpdateContactState(false);
    if (!car.IsAnyWheelContact() || car.IsWheelsContact() ||
        car.IsBodyContact())
        return 51;

    source::GameCar contactCar;
    const source::GameCar::ContactRules contactRules{
        false, {10.0F, 20.0F}, {30.0F, 40.0F},
        {4.0F, 8.0F}, {30.0F, 40.0F}};
    contactCar.LockClutch(1.0F);
    source::GameCar::ContactInput lowBorder;
    lowBorder.target = source::GameCar::ContactTarget::Track;
    lowBorder.normalForce = {20.0F, 0.0F, 0.0F};
    lowBorder.linearVelocity = {20.0F, 0.0F, 0.0F};
    lowBorder.sourcePlayerId = 7U;
    lowBorder.shotTransparency = true;
    const auto lowBorderResult =
        contactCar.OnContact(lowBorder, contactRules);
    if (!lowBorderResult.bodyContact || !contactCar.IsBodyContact() ||
        lowBorderResult.cancelClutch ||
        !contactCar.IsClutchLocked() ||
        lowBorderResult.damageTarget !=
            source::GameCar::ContactDamageTarget::None)
        return 71;

    source::GameCar::ContactInput damagingBorder = lowBorder;
    damagingBorder.normalForce = {35.0F, 0.0F, 0.0F};
    const auto damagingBorderResult =
        contactCar.OnContact(damagingBorder, contactRules);
    if (!damagingBorderResult.cancelClutch ||
        contactCar.IsClutchLocked() ||
        damagingBorderResult.setLinearVelocity ||
        damagingBorderResult.damageTarget !=
            source::GameCar::ContactDamageTarget::Source ||
        damagingBorderResult.attackerPlayerId != 7U ||
        std::abs(damagingBorderResult.damage - 15.0F) > 0.0001F)
        return 72;

    source::GameCar::ContactRules springRules = contactRules;
    springRules.springBorders = true;
    source::GameCar::ContactInput springBorder = lowBorder;
    springBorder.linearVelocity = {-20.0F, 10.0F, 0.0F};
    springBorder.frictionForce = {0.0F, 1.0F, 1.0F};
    const auto springBorderResult =
        contactCar.OnContact(springBorder, springRules);
    if (!springBorderResult.cancelClutch ||
        !springBorderResult.setLinearVelocity ||
        springBorderResult.damageTarget !=
            source::GameCar::ContactDamageTarget::None ||
        std::abs(springBorderResult.linearVelocity.x - 14.0F) >
            0.0001F ||
        std::abs(springBorderResult.linearVelocity.y - 2.5F) >
            0.0001F ||
        springBorderResult.linearVelocity.z != 0.0F)
        return 73;

    source::GameCar::ContactInput carContact;
    carContact.target = source::GameCar::ContactTarget::Car;
    carContact.normalForce = {35.0F, 0.0F, 0.0F};
    carContact.sourceKineticEnergy = 100.0F;
    carContact.targetKineticEnergy = 20.0F;
    carContact.sourcePlayerId = 7U;
    carContact.targetPlayerId = 9U;
    carContact.targetDynamic = true;
    const auto targetDamage = contactCar.OnContact(carContact, contactRules);
    carContact.sourceKineticEnergy = 10.0F;
    const auto sourceDamage = contactCar.OnContact(carContact, contactRules);
    if (targetDamage.damageTarget !=
            source::GameCar::ContactDamageTarget::Target ||
        targetDamage.attackerPlayerId != 7U ||
        std::abs(targetDamage.damage - 6.0F) > 0.0001F ||
        sourceDamage.damageTarget !=
            source::GameCar::ContactDamageTarget::Source ||
        sourceDamage.attackerPlayerId != 9U ||
        std::abs(sourceDamage.damage - 6.0F) > 0.0001F)
        return 74;

    source::GameCar::ContactInput otherContact;
    otherContact.target = source::GameCar::ContactTarget::Other;
    otherContact.sourcePlayerId = 12U;
    const auto otherResult =
        contactCar.OnContact(otherContact, contactRules);
    if (!otherResult.touchTarget ||
        otherResult.attackerPlayerId != 12U ||
        otherResult.damage != 0.0F)
        return 75;
    const auto wheelProgress = car.OnProgress(1.0F / 60.0F);
    const auto ownedSlip = car.GetWheelSlipResult(0U);
    if (wheelProgress.wheelsProgressed != 2U ||
        wheelProgress.wheelBehaviorsProgressed != 1U ||
        !ownedSlip.active || !ownedSlip.makeEffect ||
        !ownedSlip.playSound || ownedSlip.volume != 1.0F ||
        car.GetWheelSlipResult(1U).active)
        return 27;
    r3d::game::originalrace::WheelSlipEffectDefinition trailDefinition;
    trailDefinition.visual.record = "trail";
    trailDefinition.soundPaths.push_back("Sounds/SkidAsphalt.ogg");
    trailDefinition.position.z = 0.01F;
    r3d::game::originalrace::WheelSlipEffectDefinition smokeDefinition;
    smokeDefinition.visual.record = "smoke7";
    const std::vector<std::vector<
        r3d::game::originalrace::WheelSlipEffectDefinition>>
        exactDefinitions{{trailDefinition, smokeDefinition}};
    source::GameCar exactSlipCar;
    exactSlipCar.BindWheels(
        exactDefinitions, {true}, &trailDefinition.visual,
        &smokeDefinition.visual);
    exactSlipCar.SetWheelContact(0U, true, 1.0F, 1.0F);
    const auto exactProgress = exactSlipCar.OnProgress(1.0F / 60.0F);
    const auto& exactResults = exactSlipCar.GetWheelSlipResults(0U);
    if (exactProgress.wheelBehaviorsProgressed != 2U ||
        exactSlipCar.GetWheel(0U) == nullptr ||
        exactSlipCar.GetWheel(0U)->GetListenerCount() != 2U ||
        exactSlipCar.GetWheel(0U)->GetSlipEffectCount() != 2U ||
        exactResults.size() != 2U ||
        exactResults[0U].definition !=
            &trailDefinition.visual ||
        exactResults[1U].definition !=
            &smokeDefinition.visual ||
        exactResults[0U].soundPath == nullptr ||
        exactResults[1U].soundPath != nullptr ||
        exactResults[0U].position[2U] != 0.01F ||
        !exactResults[0U].makeEffect ||
        !exactResults[1U].makeEffect)
        return 81;
    exactSlipCar.SetWheelContact(0U, false, 0.0F, 0.0F);
    exactSlipCar.OnProgress(1.0F / 60.0F);
    const auto& exactReleased = exactSlipCar.GetWheelSlipResults(0U);
    if (!exactReleased[0U].freeEffect ||
        !exactReleased[1U].freeEffect ||
        !exactReleased[0U].stopSound ||
        exactReleased[1U].stopSound)
        return 82;
    source::GameCar copiedWheelCar = car;
    if (copiedWheelCar.GetWheelCount() != 2U ||
        copiedWheelCar.GetWheel(0U) == nullptr ||
        copiedWheelCar.GetWheel(0U)->GetParent() != &copiedWheelCar ||
        !copiedWheelCar.GetWheel(0U)->HasSlipEffect() ||
        copiedWheelCar.GetWheel(0U)->GetListenerCount() != 1U ||
        !copiedWheelCar.GetWheelSlipResult(0U).active ||
        copiedWheelCar.GetKSteerControl() != 0.12F ||
        copiedWheelCar.GetTireSpring() != 2.5F ||
        !copiedWheelCar.GetDisableColor() ||
        copiedWheelCar.GetWheel(0U)->GetOffset() !=
            std::array<float, 3U>{0.25F, 0.5F, 0.75F} ||
        std::abs(
            copiedWheelCar.GetWheel(0U)->GetNormalReaction() - 1.25F) >
            0.0001F)
        return 28;
    car.GetWheel(0U)->SetOffset({});
    car.GetWheel(1U)->SetOffset({});
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
    auto* animatedWheel = car.GetWheel(0U);
    animatedWheel->SetOffset({0.25F, 0.0F, 0.0F});
    const auto& offsetWheelPose = animatedWheel->PxSyncWheel(
        {}, {{}, {0.0F, 0.0F, halfQuarterTurn, halfQuarterTurn}},
        {});
    if (std::abs(offsetWheelPose.position.x) > 0.0001F ||
        std::abs(offsetWheelPose.position.y - 0.25F) > 0.0001F ||
        std::abs(offsetWheelPose.position.z) > 0.0001F)
        return 67;
    animatedWheel->SetOffset({});
    animatedWheel->SetAxleSpeed(2.0F);
    animatedWheel->OnProgress(0.5F);
    const auto& spunWheelPose = animatedWheel->PxSyncWheel(
        {}, {}, {{1.0F, 0.0F, 0.0F},
                 {0.0F, 0.0F, halfQuarterTurn,
                  halfQuarterTurn}});
    const float expectedHalfSpin = 0.5F;
    if (std::abs(animatedWheel->GetAxleSpeed() - 2.0F) > 0.0001F ||
        std::abs(animatedWheel->GetSummAngle() - 1.0F) > 0.0001F ||
        std::abs(spunWheelPose.rotation.x) > 0.0001F ||
        std::abs(spunWheelPose.rotation.y -
                 std::sin(expectedHalfSpin)) > 0.0001F ||
        std::abs(spunWheelPose.rotation.z) > 0.0001F ||
        std::abs(spunWheelPose.rotation.w -
                 std::cos(expectedHalfSpin)) > 0.0001F)
        return 51;
    animatedWheel->ConfigureDynamics(-1.0F, true, false, true, 0.5F);
    const auto& invertedWheelPose = animatedWheel->PxSyncWheel(
        {}, {}, {{1.0F, 0.0F, 0.0F}, {}});
    if (std::abs(invertedWheelPose.rotation.z - 1.0F) > 0.0001F ||
        std::abs(invertedWheelPose.rotation.w) > 0.0001F)
        return 52;
    animatedWheel->ConfigureDynamics(-1.0F, true, false, false, 0.5F);
    animatedWheel->SetLead(false);
    animatedWheel->SetSteer(true);
    if (car.GetLeadWheelCount() != 0U ||
        car.GetSteerWheelCount() != 2U ||
        car.GetSteerGroupWheel(0U) != animatedWheel ||
        car.GetSteerGroupWheel(1U) != car.GetWheel(1U))
        return 68;
    animatedWheel->SetLead(true);
    animatedWheel->SetSteer(false);
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

    car.BindWheels({false}, {false});
    car.ConfigureDynamics({}, {{0.0F, true, false, false, 0.5F}});
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
    car.GetWheel(0U)->SetAxleSpeed(10.0F);
    const auto animationProgress = car.OnProgress(0.5F);
    if (animationProgress.animationChildrenProgressed != 2U ||
        animationProgress.animationBehaviorsProgressed != 3U ||
        std::abs(car.GetTrackTextureOffset() - 0.5F) > 0.0001F ||
        std::abs(car.GetCushionAngle(0U) -
                 0.25F * 3.14159265358979323846F) > 0.0001F ||
        car.GetCushionAngle(0U) != car.GetCushionAngle(1U))
        return 31;
    car.GetWheel(0U)->SetAxleSpeed(0.2F);
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
    car.ReleaseWheels();
    if (car.GetAnimationChildCount() != 0U ||
        !car.GetChildren().empty())
        return 34;

    source::PxWheelSlipEffect slip;
    r3d::game::originalrace::ObjectDefinition standaloneSlip;
    slip.Configure(
        &standaloneSlip, {"Sounds/SkidAsphalt.ogg"});
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

    std::cout << "original GameCar lock, contact, SoundMotor and "
                 "wheel-slip animation rules passed\n";
    return 0;
}
