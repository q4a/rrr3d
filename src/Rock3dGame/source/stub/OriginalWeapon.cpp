#include "OriginalWeapon.h"

namespace r3d::game::originalrace::source
{

namespace
{

constexpr std::uint32_t masloProjectileType = 10U;

} // namespace

Weapon::Weapon(const Desc& desc) : desc_(desc) {}

void Weapon::Reset() noexcept
{
    shotTime_ = 0.0F;
}

void Weapon::OnProgress(float deltaTime) noexcept
{
    shotTime_ += deltaTime;
}

float Weapon::GetShotTime() const noexcept
{
    return shotTime_;
}

bool Weapon::IsReadyShot(float delay) const noexcept
{
    // Weapon.cpp uses a strict comparison, which matters on the first frame
    // and for analog mine bindings with a zero threshold.
    return shotTime_ > delay;
}

bool Weapon::IsReadyShot() const noexcept
{
    return IsReadyShot(desc_.shotDelay);
}

bool Weapon::IsMaslo() const noexcept
{
    return !desc_.projectileTypes.empty() &&
           desc_.projectileTypes.front() == masloProjectileType;
}

void Weapon::OnShot(bool projectileCreated) noexcept
{
    // Weapon::CreateShot resets _shotTime only after PrepareProj succeeds.
    if (projectileCreated)
        shotTime_ = 0.0F;
}

const Weapon::Desc& Weapon::GetDesc() const noexcept
{
    return desc_;
}

void Weapon::SetDesc(const Desc& value)
{
    desc_ = value;
}

void Weapon::SetDesc(
    float shotDelay,
    std::span<const std::uint32_t> projectileTypes)
{
    desc_.shotDelay = shotDelay;
    desc_.projectileTypes.assign(
        projectileTypes.begin(), projectileTypes.end());
}

void WeaponRack::Reset() noexcept
{
    for (auto& weapon : primary)
        weapon.Reset();
    hyper.Reset();
    mine.Reset();
}

void WeaponRack::OnProgress(float deltaTime) noexcept
{
    for (auto& weapon : primary)
        weapon.OnProgress(deltaTime);
    hyper.OnProgress(deltaTime);
    mine.OnProgress(deltaTime);
}

} // namespace r3d::game::originalrace::source
