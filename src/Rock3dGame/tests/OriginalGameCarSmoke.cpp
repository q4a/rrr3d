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

    std::cout << "original GameCar lock state rules passed\n";
    return 0;
}
