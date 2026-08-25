#include "OriginalGameCar.h"

#include <algorithm>

namespace r3d::game::originalrace::source
{

void GameCar::Reset() noexcept
{
    clutchStrength_ = 0.0F;
    clutchTime_ = 0.0F;
    springTime_ = 0.0F;
    mineTime_ = 0.0F;
}

GameCar::ProgressResult GameCar::OnProgress(float deltaTime) noexcept
{
    ProgressResult result;
    if (clutchTime_ > 0.0F)
    {
        clutchTime_ -= deltaTime;
        if (clutchTime_ < 0.0F)
            clutchTime_ = 0.0F;
        result.clutchReleased = clutchTime_ == 0.0F;
    }
    if (mineTime_ > 0.0F)
    {
        mineTime_ -= deltaTime;
        if (mineTime_ < 0.0F)
            mineTime_ = 0.0F;
        result.mineReleased = mineTime_ == 0.0F;
    }
    if (springTime_ > 0.0F)
    {
        springTime_ = std::max(springTime_ - deltaTime, 0.0F);
        result.springReleased = springTime_ == 0.0F;
    }
    return result;
}

bool GameCar::LockClutch(float strength, bool clutchImmunity) noexcept
{
    if (clutchImmunity || clutchTime_ > 0.0F)
        return false;
    clutchTime_ = clutchLockSeconds;
    clutchStrength_ = strength;
    return true;
}

void GameCar::CancelClutch() noexcept
{
    clutchTime_ = 0.0F;
}

bool GameCar::IsClutchLocked() const noexcept
{
    return clutchTime_ > 0.0F;
}

float GameCar::GetClutchTime() const noexcept
{
    return clutchTime_;
}

float GameCar::ConsumeClutchStrength() noexcept
{
    const float result = clutchStrength_;
    clutchStrength_ = 0.0F;
    return result;
}

void GameCar::LockSpring() noexcept
{
    springTime_ = springLockSeconds;
}

bool GameCar::IsSpringLocked() const noexcept
{
    return springTime_ > 0.0F;
}

float GameCar::GetSpringTime() const noexcept
{
    return springTime_;
}

void GameCar::LockMine(float time) noexcept
{
    mineTime_ = time;
}

bool GameCar::IsMineLocked() const noexcept
{
    return mineTime_ > 0.0F;
}

float GameCar::GetMineTime() const noexcept
{
    return mineTime_;
}

} // namespace r3d::game::originalrace::source
