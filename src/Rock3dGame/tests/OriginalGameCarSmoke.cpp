#include "OriginalGameCar.h"

#include <cmath>
#include <iostream>

namespace source = r3d::game::originalrace::source;

int main()
{
    source::GameCar car;
    if (car.IsClutchLocked() || car.IsSpringLocked() ||
        car.IsMineLocked())
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
