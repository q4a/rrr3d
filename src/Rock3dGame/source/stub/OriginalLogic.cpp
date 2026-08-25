#include "OriginalLogic.h"

#include <algorithm>

namespace r3d::game::originalrace::source
{

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

float Logic::ResolveDamage(
    const PlayerItemRack* targetItems, float value,
    DamageType damageType) noexcept
{
    if (targetItems == nullptr || damageType == DamageType::Touch)
        return value;
    // Player::GetSlotInst(stReflector) returns the first physical slot.
    return targetItems->Reflect(value);
}

GameObject::DamageResult Logic::Damage(
    GameObject& target, std::size_t senderPlayerId,
    float supportedValue, DamageType damageType) noexcept
{
    return target.Damage(senderPlayerId, supportedValue, damageType);
}

GameObject::DamageResult Logic::Damage(
    GameObject& target, std::size_t senderPlayerId,
    float supportedValue, float authoritativeLife,
    bool authoritativeDeath, DamageType damageType) noexcept
{
    return target.Damage(senderPlayerId, supportedValue,
                         authoritativeLife, authoritativeDeath,
                         damageType);
}

} // namespace r3d::game::originalrace::source
