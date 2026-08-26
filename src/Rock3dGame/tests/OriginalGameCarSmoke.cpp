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
    if (!car.OnProgress(0.38F).clutchReleased ||
        car.IsClutchLocked())
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
    if (!car.IsSpringLocked() || car.IsMineLocked())
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

    car.BindWheels({true, false}, {true, false});
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
        secondWheel->GetBehaviors().GetCount() != 0U)
        return 25;
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
    car.ReleaseWheels();
    if (car.GetWheelCount() != 0U ||
        !car.GetChildren().empty())
        return 29;

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
