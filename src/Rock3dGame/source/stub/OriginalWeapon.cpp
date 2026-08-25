#include "OriginalWeapon.h"

#include <algorithm>
#include <cmath>

namespace r3d::game::originalrace::source
{

namespace
{

constexpr std::uint32_t masloProjectileType = 10U;

float length(Proj::Vec3 value) noexcept
{
    return std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z);
}

Proj::Vec3 normalized(Proj::Vec3 value) noexcept
{
    const float magnitude = length(value);
    if (magnitude <= 0.000001F)
        return {};
    return {value.x / magnitude, value.y / magnitude,
            value.z / magnitude};
}

} // namespace

Proj::ContactResult Proj::SpeedArrowContact(
    Vec3 worldDirection, float damage) noexcept
{
    const auto direction = normalized(worldDirection);
    return {{direction.x * damage, direction.y * damage,
             direction.z * damage},
            0.0F, true, false, true};
}

Proj::ContactResult Proj::LushaContact(
    Vec3 linearVelocity, float damage) noexcept
{
    ContactResult result;
    const float speed = length(linearVelocity);
    if (speed > 1.0F && speed > damage)
    {
        const auto direction = normalized(linearVelocity);
        result.linearVelocity = {
            direction.x * damage, direction.y * damage,
            direction.z * damage};
        result.setLinearVelocity = true;
    }
    return result;
}

Proj::ContactResult Proj::MasloContact(
    Vec3 carPosition, Vec3 carWorldRight, Vec3 oilPosition,
    Vec3 linearVelocity, float damage, bool arming,
    bool mineLocked, bool clutchLocked,
    bool clutchImmune) noexcept
{
    ContactResult result;
    if (arming || mineLocked || clutchLocked || clutchImmune ||
        length(linearVelocity) <= 3.0F)
    {
        return result;
    }
    const Vec3 offset{
        oilPosition.x - carPosition.x,
        oilPosition.y - carPosition.y,
        oilPosition.z - carPosition.z};
    const float distance =
        carWorldRight.x * offset.x +
        carWorldRight.y * offset.y +
        carWorldRight.z * offset.z;
    result.clutchStrength =
        std::abs(distance) > 0.1F && distance > 0.0F
            ? -damage
            : damage;
    result.lockClutch = true;
    return result;
}

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

DroidItem::DroidItem(
    Weapon* weapon, std::uint32_t maximumCharge,
    std::uint32_t countCharge, std::uint32_t* currentCharge,
    float repairValue, float repairPeriod) noexcept
{
    Bind(weapon, maximumCharge, countCharge, currentCharge,
         repairValue, repairPeriod);
}

void DroidItem::Bind(
    Weapon* weapon, std::uint32_t maximumCharge,
    std::uint32_t countCharge, std::uint32_t* currentCharge,
    float repairValue, float repairPeriod) noexcept
{
    WeaponItem::Bind(
        weapon, maximumCharge, countCharge, currentCharge);
    repairValue_ = repairValue;
    repairPeriod_ = repairPeriod;
    time_ = 0.0F;
    progressRegistered_ = false;
}

void DroidItem::OnCreateCar() noexcept
{
    time_ = 0.0F;
    progressRegistered_ = true;
}

void DroidItem::OnDestroyCar() noexcept
{
    // The source unregisters the progress event here. _time is reset by the
    // next OnCreateCar, not by OnDestroyCar itself.
    progressRegistered_ = false;
}

float DroidItem::OnProgress(
    float deltaTime, float& life, float maximumLife, bool death) noexcept
{
    if (!progressRegistered_)
        return 0.0F;
    if (life >= maximumLife || death)
    {
        time_ = 0.0F;
        return 0.0F;
    }
    if ((time_ += deltaTime) > repairPeriod_)
    {
        time_ -= repairPeriod_;
        const float previousLife = life;
        // Player.cpp intentionally uses a literal rather than _repairValue.
        life = std::min(maximumLife, life + 5.0F);
        return life - previousLife;
    }
    return 0.0F;
}

float DroidItem::GetRepairValue() const noexcept
{
    return repairValue_;
}

void DroidItem::SetRepairValue(float value) noexcept
{
    repairValue_ = value;
}

float DroidItem::GetRepairPeriod() const noexcept
{
    return repairPeriod_;
}

void DroidItem::SetRepairPeriod(float value) noexcept
{
    repairPeriod_ = value;
}

float DroidItem::GetRepairTime() const noexcept
{
    return time_;
}

bool DroidItem::IsProgressRegistered() const noexcept
{
    return progressRegistered_;
}

ReflectorItem::ReflectorItem(
    Weapon* weapon, std::uint32_t maximumCharge,
    std::uint32_t countCharge, std::uint32_t* currentCharge,
    float reflectValue) noexcept
{
    Bind(weapon, maximumCharge, countCharge, currentCharge,
         reflectValue);
}

void ReflectorItem::Bind(
    Weapon* weapon, std::uint32_t maximumCharge,
    std::uint32_t countCharge, std::uint32_t* currentCharge,
    float reflectValue) noexcept
{
    WeaponItem::Bind(
        weapon, maximumCharge, countCharge, currentCharge);
    reflectValue_ = reflectValue;
}

float ReflectorItem::GetReflectValue() const noexcept
{
    return reflectValue_;
}

void ReflectorItem::SetReflectValue(float value) noexcept
{
    reflectValue_ = value;
}

float ReflectorItem::Reflect(float damage) const noexcept
{
    return damage * std::clamp(1.0F - reflectValue_, 0.0F, 1.0F);
}

void PlayerItemRack::Reset() noexcept
{
    types_.fill(Type::None);
    droids_.fill(DroidItem{});
    reflectors_.fill(ReflectorItem{});
}

void PlayerItemRack::BindDroid(
    std::size_t slot, Weapon* weapon,
    std::uint32_t maximumCharge, std::uint32_t countCharge,
    std::uint32_t* currentCharge, float repairValue,
    float repairPeriod) noexcept
{
    if (slot >= slotCount)
        return;
    types_[slot] = Type::Droid;
    droids_[slot].Bind(
        weapon, maximumCharge, countCharge, currentCharge,
        repairValue, repairPeriod);
    reflectors_[slot] = ReflectorItem{};
}

void PlayerItemRack::BindReflector(
    std::size_t slot, Weapon* weapon,
    std::uint32_t maximumCharge, std::uint32_t countCharge,
    std::uint32_t* currentCharge, float reflectValue) noexcept
{
    if (slot >= slotCount)
        return;
    types_[slot] = Type::Reflector;
    reflectors_[slot].Bind(
        weapon, maximumCharge, countCharge, currentCharge,
        reflectValue);
    droids_[slot] = DroidItem{};
}

void PlayerItemRack::OnCreateCar() noexcept
{
    for (std::size_t slot = 0U; slot < slotCount; ++slot)
    {
        if (types_[slot] == Type::Droid)
            droids_[slot].OnCreateCar();
    }
}

void PlayerItemRack::OnDestroyCar() noexcept
{
    for (std::size_t slot = 0U; slot < slotCount; ++slot)
    {
        if (types_[slot] == Type::Droid)
            droids_[slot].OnDestroyCar();
    }
}

float PlayerItemRack::OnProgress(
    float deltaTime, float& life, float maximumLife, bool death) noexcept
{
    float healed = 0.0F;
    for (std::size_t slot = 0U; slot < slotCount; ++slot)
    {
        if (types_[slot] == Type::Droid)
        {
            healed += droids_[slot].OnProgress(
                deltaTime, life, maximumLife, death);
        }
    }
    return healed;
}

float PlayerItemRack::Reflect(float damage) const noexcept
{
    for (std::size_t slot = 0U; slot < slotCount; ++slot)
    {
        if (types_[slot] == Type::Reflector)
            return reflectors_[slot].Reflect(damage);
    }
    return damage;
}

PlayerItemRack::Type PlayerItemRack::GetType(
    std::size_t slot) const noexcept
{
    return slot < slotCount ? types_[slot] : Type::None;
}

DroidItem* PlayerItemRack::GetDroid(std::size_t slot) noexcept
{
    return slot < slotCount && types_[slot] == Type::Droid
               ? &droids_[slot]
               : nullptr;
}

const DroidItem* PlayerItemRack::GetDroid(
    std::size_t slot) const noexcept
{
    return slot < slotCount && types_[slot] == Type::Droid
               ? &droids_[slot]
               : nullptr;
}

ReflectorItem* PlayerItemRack::GetReflector(
    std::size_t slot) noexcept
{
    return slot < slotCount && types_[slot] == Type::Reflector
               ? &reflectors_[slot]
               : nullptr;
}

const ReflectorItem* PlayerItemRack::GetReflector(
    std::size_t slot) const noexcept
{
    return slot < slotCount && types_[slot] == Type::Reflector
               ? &reflectors_[slot]
               : nullptr;
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
