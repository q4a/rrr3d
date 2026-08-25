#include "OriginalWeapon.h"

#include <algorithm>

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

WeaponItem::WeaponItem(
    Weapon* weapon, std::uint32_t maximumCharge,
    std::uint32_t countCharge, std::uint32_t* currentCharge,
    std::uint32_t chargeStep, float damage, int chargeCost) noexcept
{
    Bind(weapon, maximumCharge, countCharge, currentCharge,
         chargeStep, damage, chargeCost);
}

void WeaponItem::Bind(
    Weapon* weapon, std::uint32_t maximumCharge,
    std::uint32_t countCharge, std::uint32_t* currentCharge,
    std::uint32_t chargeStep, float damage, int chargeCost) noexcept
{
    weapon_ = weapon;
    maximumCharge_ = maximumCharge;
    countCharge_ = countCharge;
    currentCharge_ = currentCharge;
    chargeStep_ = chargeStep;
    damage_ = damage;
    chargeCost_ = chargeCost;
}

bool WeaponItem::Shot(bool projectileCreated, int newCharge) noexcept
{
    bool result = false;
    if (currentCharge_ != nullptr &&
        (*currentCharge_ > 0U || maximumCharge_ == 0U))
    {
        result = weapon_ != nullptr && projectileCreated;
        if (newCharge == -1)
        {
            newCharge = result
                            ? static_cast<int>(*currentCharge_) - 1
                            : static_cast<int>(*currentCharge_);
        }
    }

    if (currentCharge_ != nullptr)
    {
        // Player.cpp applies this even when the charge gate or projectile
        // preparation failed.  That detail is required by NetPlayer::DoShot.
        *currentCharge_ = static_cast<std::uint32_t>(
            std::max(newCharge, 0));
    }
    if (weapon_ != nullptr)
        weapon_->OnShot(result);
    return result;
}

void WeaponItem::Reload() noexcept
{
    if (currentCharge_ != nullptr)
        *currentCharge_ = countCharge_;
}

bool WeaponItem::IsReadyShot(float delay) const noexcept
{
    return weapon_ != nullptr && weapon_->IsReadyShot(delay);
}

bool WeaponItem::IsReadyShot() const noexcept
{
    return weapon_ != nullptr && weapon_->IsReadyShot();
}

bool WeaponItem::IsInstalled() const noexcept
{
    return weapon_ != nullptr && currentCharge_ != nullptr;
}

bool WeaponItem::HasShotCharge() const noexcept
{
    return currentCharge_ != nullptr &&
           (*currentCharge_ > 0U || maximumCharge_ == 0U);
}

std::uint32_t WeaponItem::GetMaxCharge() const noexcept
{
    return maximumCharge_;
}

std::uint32_t WeaponItem::GetCntCharge() const noexcept
{
    return countCharge_;
}

std::uint32_t WeaponItem::GetCurCharge() const noexcept
{
    return currentCharge_ != nullptr ? *currentCharge_ : 0U;
}

std::uint32_t WeaponItem::GetChargeStep() const noexcept
{
    return chargeStep_;
}

float WeaponItem::GetDamage() const noexcept
{
    return damage_;
}

int WeaponItem::GetChargeCost() const noexcept
{
    return chargeCost_;
}

Weapon* WeaponItem::GetWeapon() const noexcept
{
    return weapon_;
}

Weapon::Desc WeaponItem::GetDesc() const
{
    return weapon_ != nullptr ? weapon_->GetDesc() : Weapon::Desc{};
}

bool Logic::ShotPlan::Get(SlotType type) const noexcept
{
    return slots[static_cast<std::size_t>(type)];
}

Logic::ShotPlan Logic::Shot(
    const WeaponItem* weapon, SlotType type, bool human) noexcept
{
    ShotPlan result;
    result.humanShotEvent = human && type != SlotType::Hyper;
    if (weapon != nullptr && weapon->IsReadyShot())
    {
        result.slots[static_cast<std::size_t>(type)] = true;
        result.shotCount = 1U;
    }
    return result;
}

Logic::ShotPlan Logic::ShotAll(
    std::span<const WeaponItem> primaryWeapons,
    bool human) noexcept
{
    ShotPlan result;
    result.humanShotEvent = human;
    const std::size_t count = std::min(
        primaryWeapons.size(), WeaponRack::primarySlotCount);
    for (std::size_t slot = 0U; slot < count; ++slot)
    {
        if (!primaryWeapons[slot].IsReadyShot())
            continue;
        result.slots[slot +
                     static_cast<std::size_t>(SlotType::Weapon1)] = true;
        ++result.shotCount;
    }
    return result;
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
