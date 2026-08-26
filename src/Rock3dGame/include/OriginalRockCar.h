#pragma once

#include "OriginalGameCar.h"
#include "OriginalWeapon.h"

namespace r3d::game::originalrace::source
{

// Backend-neutral transcription of the original RockCar owner. Windows
// keeps the six installed Weapon map objects below RockCar and progresses
// that collection immediately after GameCar. Map/serialization remain at
// the portable adapter boundary; gameplay lifetime and update order live
// here.
class RockCar : public GameCar
{
public:
    RockCar() = default;
    RockCar(const RockCar&) = default;
    RockCar& operator=(const RockCar&) = default;
    RockCar(RockCar&&) noexcept = default;
    RockCar& operator=(RockCar&&) noexcept = default;
    ~RockCar() override = default;

    ProgressResult OnProgress(float deltaTime) noexcept;
    WeaponRack& GetWeapons() noexcept;
    const WeaponRack& GetWeapons() const noexcept;

private:
    WeaponRack weapons_;
};

} // namespace r3d::game::originalrace::source
