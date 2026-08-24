#include "OriginalPlayer.h"

#include <algorithm>
#include <cmath>

namespace r3d::game::originalrace::source
{

const std::array<float, 3> Player::humanEasingMinimumDistance{
    20.0F, 20.0F, 20.0F};
const std::array<float, 3> Player::humanEasingMaximumDistance{
    200.0F, 200.0F, 200.0F};
const std::array<float, 3> Player::humanEasingMinimumSpeed{
    95.0F * 1000.0F / 3600.0F,
    110.0F * 1000.0F / 3600.0F,
    125.0F * 1000.0F / 3600.0F};
const std::array<float, 3> Player::humanEasingMaximumSpeed{
    55.0F * 1000.0F / 3600.0F,
    65.0F * 1000.0F / 3600.0F,
    75.0F * 1000.0F / 3600.0F};
const std::array<float, 3> Player::computerCheatMinimumTorque{
    1.05F, 1.20F, 1.30F};
const std::array<float, 3> Player::computerCheatMaximumTorque{
    1.30F, 1.65F, 1.85F};
const std::array<float, 3> Player::humanArmorScale{
    2.0F, 1.75F, 1.5F};

void Player::Reset(float newMaximumLife,
                   std::uint32_t initialPlace) noexcept
{
    *this = Player{};
    maximumLife = std::max(newMaximumLife, 1.0F);
    life = maximumLife;
    place = initialPlace;
}

void Player::ReloadWeapons(
    std::size_t weaponDefinitionCount) noexcept
{
    for (std::size_t slot = 0U; slot < weaponCharges.size(); ++slot)
    {
        if (weaponSlots[slot] != invalidWeapon &&
            weaponSlots[slot] < weaponDefinitionCount)
            weaponCharges[slot] = weaponCapacity[slot];
    }
    if (hyperWeapon != invalidWeapon &&
        hyperWeapon < weaponDefinitionCount)
        hyperCharge = hyperCapacity;
    if (mineWeapon != invalidWeapon &&
        mineWeapon < weaponDefinitionCount)
        mines = mineCapacity;
    SyncSelectedWeapon(weaponDefinitionCount);
}

void Player::SyncSelectedWeapon(
    std::size_t weaponDefinitionCount) noexcept
{
    for (std::size_t offset = 0U; offset < weaponSlots.size(); ++offset)
    {
        const auto slot =
            (selectedWeaponSlot + offset) % weaponSlots.size();
        const auto weapon = weaponSlots[slot];
        if (weapon == invalidWeapon || weapon >= weaponDefinitionCount)
            continue;
        selectedWeaponSlot = slot;
        selectedWeapon = weapon;
        ammunition = weaponCharges[slot];
        return;
    }
    selectedWeapon = invalidWeapon;
    ammunition = 0U;
}

PlayerBonusResult Player::TakeMoney(float value) noexcept
{
    const auto amount = static_cast<std::uint32_t>(
        std::max(value, 0.0F));
    pickedMoney += amount;
    return {PlayerBonusSlot::None, invalidWeapon, amount};
}

PlayerBonusResult Player::TakeMedpack(float value) noexcept
{
    const float previous = life;
    life = std::min(life + value, maximumLife);
    return {PlayerBonusSlot::None, invalidWeapon,
            static_cast<std::uint32_t>(
                std::max(life - previous, 0.0F))};
}

PlayerBonusResult Player::TakeImmortal(float value) noexcept
{
    if (shieldSeconds <= 0.0F)
    {
        shieldEffectSeconds = 0.0F;
        shieldFadeInSeconds = 0.0F;
        // ImmortalEffect::OnImmortalStatus(true) does not clear a running
        // fade-out, but it does clear the damage flash timer.
        shieldDamageSeconds = -1.0F;
    }
    shieldSeconds = std::max(value, 0.0F);
    return {PlayerBonusSlot::None, invalidWeapon,
            static_cast<std::uint32_t>(shieldSeconds)};
}

PlayerBonusResult Player::TakeAmmunition(
    float value,
    const std::vector<std::uint32_t>& maximumCharges,
    float randomUnit) noexcept
{
    struct RechargeTarget
    {
        std::uint32_t* current = nullptr;
        std::uint32_t capacity = 0U;
        std::size_t weapon = invalidWeapon;
        PlayerBonusSlot slot = PlayerBonusSlot::None;
    };
    std::vector<RechargeTarget> targets;
    targets.reserve(weaponSlots.size() + 2U);
    if (hyperWeapon != invalidWeapon &&
        hyperWeapon < maximumCharges.size() &&
        hyperCharge < hyperCapacity)
    {
        targets.push_back(
            {&hyperCharge, hyperCapacity, hyperWeapon,
             PlayerBonusSlot::Hyper});
    }
    if (mineWeapon != invalidWeapon &&
        mineWeapon < maximumCharges.size() &&
        mines < mineCapacity)
    {
        targets.push_back(
            {&mines, mineCapacity, mineWeapon,
             PlayerBonusSlot::Mine});
    }
    for (std::size_t slot = 0U; slot < weaponSlots.size(); ++slot)
    {
        const auto weapon = weaponSlots[slot];
        if (weapon == invalidWeapon || weapon >= maximumCharges.size() ||
            weaponCharges[slot] >= weaponCapacity[slot])
            continue;
        targets.push_back(
            {&weaponCharges[slot], weaponCapacity[slot], weapon,
             PlayerBonusSlot::Primary});
    }
    PlayerBonusResult result;
    if (!targets.empty())
    {
        auto& target = targets[RoundedRandomIndex(
            targets.size(), randomUnit)];
        const auto amount = BonusCharge(
            maximumCharges[target.weapon], value);
        *target.current = std::min(
            *target.current + amount, target.capacity);
        result = {target.slot, target.weapon, amount};
    }
    SyncSelectedWeapon(maximumCharges.size());
    return result;
}

void Player::SetFinished(bool value, float time) noexcept
{
    finished = value;
    if (value)
        finishTime = time;
    else
        finishTime = -1.0F;
}

void Player::Complete(std::uint32_t resultPlace,
                      std::uint32_t resultMoney,
                      std::uint32_t resultPoints,
                      float time) noexcept
{
    SetFinished(true, time);
    place = resultPlace;
    rewardMoney = resultMoney;
    rewardPoints = resultPoints;
}

void Player::AddMoney(std::int32_t value) noexcept
{
    const auto result = static_cast<std::int64_t>(money) + value;
    money = static_cast<std::uint32_t>(std::max<std::int64_t>(result, 0));
}

void Player::AddPoints(std::int32_t value) noexcept
{
    const auto result = static_cast<std::int64_t>(points) + value;
    points = static_cast<std::uint32_t>(
        std::max<std::int64_t>(result, 0));
}

void Player::ApplyRaceReward() noexcept
{
    AddMoney(static_cast<std::int32_t>(rewardMoney + pickedMoney));
    AddPoints(static_cast<std::int32_t>(rewardPoints));
}

void Player::Destroy() noexcept
{
    life = 0.0F;
    destroyed = true;
    lowLife = false;
    lowLifeEffectSeconds = 0.0F;
    shieldSeconds = 0.0F;
    shieldEffectSeconds = 0.0F;
    shieldFadeInSeconds = -1.0F;
    shieldFadeOutSeconds = -1.0F;
    shieldDamageSeconds = -1.0F;
    touchAttacker = invalidWeapon;
    touchAttributionSeconds = 0.0F;
    restoreSeconds = restoreCarSeconds;
}

PlayerRestoreStep Player::ProgressRestore(float seconds) noexcept
{
    if (!destroyed)
        return PlayerRestoreStep::None;
    if (restoreSeconds < 0.0F)
    {
        restoreSeconds = 0.0F;
        destroyed = false;
        return PlayerRestoreStep::ActivateCar;
    }
    restoreSeconds = std::max(0.0F, restoreSeconds - seconds);
    if (restoreSeconds > 0.0F)
        return PlayerRestoreStep::None;
    life = maximumLife;
    restoreSeconds = -1.0F;
    return PlayerRestoreStep::QueueRespawn;
}

void Player::Disconnect() noexcept
{
    disconnected = true;
    destroyed = true;
    SetFinished(false);
    life = 0.0F;
    lowLife = false;
    restoreSeconds = 0.0F;
    shieldSeconds = 0.0F;
    shieldEffectSeconds = 0.0F;
    shieldFadeInSeconds = -1.0F;
    shieldFadeOutSeconds = -1.0F;
    shieldDamageSeconds = -1.0F;
    touchAttacker = invalidWeapon;
    touchAttributionSeconds = 0.0F;
}

float Player::FinishBrake(float elapsedSeconds) const noexcept
{
    return finished && elapsedSeconds - finishTime >= finishBlockSeconds
               ? 1.0F
               : 0.0F;
}

std::size_t Player::RoundedRandomIndex(
    std::size_t count, float randomUnit) noexcept
{
    if (count <= 1U)
        return 0U;
    const float value = static_cast<float>(count - 1U) *
                        std::clamp(randomUnit, 0.0F, 1.0F);
    const float floorValue = std::floor(value);
    const float rounded = value - 0.5F < floorValue
                              ? floorValue
                              : floorValue + 1.0F;
    return std::min(static_cast<std::size_t>(rounded), count - 1U);
}

std::uint32_t Player::BonusCharge(
    std::uint32_t maximumCharge, float value) noexcept
{
    return static_cast<std::uint32_t>(std::max(
        static_cast<float>(maximumCharge) * value, 1.0F));
}

} // namespace r3d::game::originalrace::source
