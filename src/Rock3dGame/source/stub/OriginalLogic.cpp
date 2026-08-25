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

Logic::TakeBonusResult Logic::TakeBonus(
    Player* player, GameObject* bonus, PlayerBonusType type,
    float value,
    const std::vector<std::uint32_t>& maximumCharges,
    float randomUnit) noexcept
{
    if (player == nullptr || bonus == nullptr || player->destroyed ||
        bonus->destroyed)
        return {};
    // Logic::TakeBonus kills the picked MapObj before Player applies it.
    bonus->Death();
    return {player->TakeBonus(
                type, value, maximumCharges, randomUnit),
            true};
}

} // namespace r3d::game::originalrace::source
