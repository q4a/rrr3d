#pragma once

#include "OriginalGameObject.h"
#include "OriginalPlayer.h"
#include "OriginalWeapon.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace r3d::game::originalrace::source
{

// Backend-neutral gameplay portion of Logic. Network transport remains at
// the session boundary; selection, reflector policy and GameObject dispatch
// follow Logic.cpp.
class Logic
{
public:
    enum class SlotType : std::uint8_t
    {
        Hyper = 0U,
        Mine = 1U,
        Weapon1 = 2U,
        Weapon2 = 3U,
        Weapon3 = 4U,
        Weapon4 = 5U,
    };

    struct ShotPlan
    {
        std::array<bool, 6U> slots{};
        std::size_t shotCount = 0U;
        bool humanShotEvent = false;

        bool Get(SlotType type) const noexcept;
    };

    static ShotPlan Shot(const WeaponItem* weapon, SlotType type,
                         bool human) noexcept;
    static ShotPlan ShotAll(
        std::span<const WeaponItem> primaryWeapons,
        bool human) noexcept;

    static float ResolveDamage(
        const PlayerItemRack* targetItems, float value,
        DamageType damageType) noexcept;
    static GameObject::DamageResult Damage(
        GameObject& target, std::size_t senderPlayerId,
        float supportedValue, DamageType damageType) noexcept;
    static GameObject::DamageResult Damage(
        GameObject& target, std::size_t senderPlayerId,
        float supportedValue, float authoritativeLife,
        bool authoritativeDeath, DamageType damageType) noexcept;

    struct TakeBonusResult
    {
        PlayerBonusResult player;
        bool taken = false;
    };

    static TakeBonusResult TakeBonus(
        Player* player, GameObject* bonus, PlayerBonusType type,
        float value,
        const std::vector<std::uint32_t>& maximumCharges,
        float randomUnit) noexcept;
};

} // namespace r3d::game::originalrace::source
