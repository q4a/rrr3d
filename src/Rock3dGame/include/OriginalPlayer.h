#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace r3d::game::originalrace::source
{

// Slot order deliberately follows Player::SlotType from the Windows source:
// stHyper, stMine, stWeapon1..stWeapon4.  Player::TakeBonus depends on this
// order before applying its rounded random selection.
enum class PlayerBonusSlot : std::uint8_t
{
    None,
    Primary,
    Hyper,
    Mine,
};

struct PlayerBonusResult
{
    PlayerBonusSlot slot = PlayerBonusSlot::None;
    std::size_t weapon = std::numeric_limits<std::size_t>::max();
    std::uint32_t amount = 0U;
};

enum class PlayerRestoreStep : std::uint8_t
{
    None,
    QueueRespawn,
    ActivateCar,
};

// Portable transcription of the gameplay-owned portion of Player.  Renderer
// actor ownership and the PhysX RockCar pointer remain backend boundaries,
// while race state, inventory, bonuses, finish blocking and restore lifecycle
// retain the source class's rules.  Fields remain visible during the staged
// object-graph migration because HUD/network adapters still consume them.
class Player
{
public:
    static constexpr std::size_t invalidWeapon =
        std::numeric_limits<std::size_t>::max();
    static constexpr std::size_t weaponSlotCount = 4U;
    static constexpr float restoreCarSeconds = 2.0F;
    static constexpr float finishBlockSeconds = 0.3F;

    static const std::array<float, 3> humanEasingMinimumDistance;
    static const std::array<float, 3> humanEasingMaximumDistance;
    static const std::array<float, 3> humanEasingMinimumSpeed;
    static const std::array<float, 3> humanEasingMaximumSpeed;
    static const std::array<float, 3> computerCheatMinimumTorque;
    static const std::array<float, 3> computerCheatMaximumTorque;
    static const std::array<float, 3> humanArmorScale;

    void Reset(float newMaximumLife, std::uint32_t initialPlace) noexcept;
    void ReloadWeapons(std::size_t weaponDefinitionCount) noexcept;
    void SyncSelectedWeapon(std::size_t weaponDefinitionCount) noexcept;

    PlayerBonusResult TakeMoney(float value) noexcept;
    PlayerBonusResult TakeMedpack(float value) noexcept;
    PlayerBonusResult TakeImmortal(float value) noexcept;
    PlayerBonusResult TakeAmmunition(
        float value,
        const std::vector<std::uint32_t>& maximumCharges,
        float randomUnit) noexcept;

    void SetFinished(bool value, float time = -1.0F) noexcept;
    void Complete(std::uint32_t resultPlace,
                  std::uint32_t resultMoney,
                  std::uint32_t resultPoints,
                  float time) noexcept;
    void AddMoney(std::int32_t value) noexcept;
    void AddPoints(std::int32_t value) noexcept;
    void ApplyRaceReward() noexcept;

    void Destroy() noexcept;
    PlayerRestoreStep ProgressRestore(float seconds) noexcept;
    void Disconnect() noexcept;
    float FinishBrake(float elapsedSeconds) const noexcept;

    static std::size_t RoundedRandomIndex(
        std::size_t count, float randomUnit) noexcept;
    static std::uint32_t BonusCharge(
        std::uint32_t maximumCharge, float value) noexcept;

    std::uint32_t completedLaps = 0;
    std::size_t nextPathNode = 1;
    std::uint32_t place = 1;
    float life = 100.0F;
    float maximumLife = 100.0F;
    std::uint32_t ammunition = 10;
    std::uint32_t mines = 0;
    std::uint32_t mineCapacity = 0;
    std::array<std::size_t, weaponSlotCount> weaponSlots{
        invalidWeapon, invalidWeapon, invalidWeapon, invalidWeapon};
    std::array<std::uint32_t, weaponSlotCount> weaponCharges{};
    std::array<std::uint32_t, weaponSlotCount> weaponCapacity{};
    // Proj::DrobilkaUpdate rotates the mounted weapon actor itself. Keep the
    // actor rotation independent from projectile age for renderer/contact use.
    std::array<float, weaponSlotCount> weaponSpinRadians{};
    std::size_t selectedWeaponSlot = 0;
    std::size_t selectedWeapon = invalidWeapon;
    std::size_t hyperWeapon = invalidWeapon;
    std::uint32_t hyperCharge = 0;
    std::uint32_t hyperCapacity = 0;
    std::size_t mineWeapon = invalidWeapon;
    std::uint32_t money = 0;
    std::uint32_t points = 0;
    std::uint32_t pickedMoney = 0;
    std::uint32_t rewardMoney = 0;
    std::uint32_t rewardPoints = 0;
    float shieldSeconds = 0.0F;
    float speedBoostSeconds = 0.0F;
    float slowSeconds = 0.0F;
    std::size_t slowWeapon = invalidWeapon;
    std::size_t slowProjectile = invalidWeapon;
    float clutchSeconds = 0.0F;
    float mineLockSeconds = 0.0F;
    float springLockSeconds = 0.0F;
    std::size_t touchAttacker = invalidWeapon;
    float touchAttributionSeconds = 0.0F;
    float restoreSeconds = 0.0F;
    float lowLifeEffectSeconds = 0.0F;
    float shieldEffectSeconds = 0.0F;
    float shieldFadeInSeconds = -1.0F;
    float shieldFadeOutSeconds = -1.0F;
    float shieldDamageSeconds = -1.0F;
    float finishTime = -1.0F;
    bool wrongWay = false;
    bool finished = false;
    bool destroyed = false;
    bool disconnected = false;
    bool lowLife = false;
};

} // namespace r3d::game::originalrace::source
