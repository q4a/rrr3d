#include "OriginalRockCar.h"

namespace r3d::game::originalrace::source
{

GameCar::ProgressResult RockCar::OnProgress(float deltaTime) noexcept
{
    auto result = GameCar::OnProgress(deltaTime);
    weapons_.OnProgress(deltaTime);
    return result;
}

WeaponRack& RockCar::GetWeapons() noexcept
{
    return weapons_;
}

const WeaponRack& RockCar::GetWeapons() const noexcept
{
    return weapons_;
}

} // namespace r3d::game::originalrace::source
