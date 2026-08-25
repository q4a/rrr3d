#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace r3d::game::originalrace::source
{

// Backend-neutral transcription of the original Weapon timer and Desc
// ownership. Projectile preparation remains at the Jolt/bgfx session
// boundary; readiness and successful-shot lifetime belong here.
class Weapon
{
public:
    struct Desc
    {
        float shotDelay = 0.0F;
        std::vector<std::uint32_t> projectileTypes;
    };

    Weapon() = default;
    explicit Weapon(const Desc& desc);

    void Reset() noexcept;
    void OnProgress(float deltaTime) noexcept;
    float GetShotTime() const noexcept;
    bool IsReadyShot(float delay) const noexcept;
    bool IsReadyShot() const noexcept;
    bool IsMaslo() const noexcept;
    void OnShot(bool projectileCreated = true) noexcept;

    const Desc& GetDesc() const noexcept;
    void SetDesc(const Desc& value);
    void SetDesc(float shotDelay,
                 std::span<const std::uint32_t> projectileTypes);

private:
    Desc desc_;
    float shotTime_ = 0.0F;
};

// Backend-neutral transcription of Player::WeaponItem.  The Windows object
// owned its charge fields directly; the portable Player already owns the
// profile-backed storage, so this class binds to that storage instead of
// creating a second, divergent copy.
class WeaponItem
{
public:
    WeaponItem() = default;
    WeaponItem(Weapon* weapon, std::uint32_t maximumCharge,
               std::uint32_t countCharge,
               std::uint32_t* currentCharge,
               std::uint32_t chargeStep = 1U,
               float damage = 0.0F,
               int chargeCost = 0) noexcept;

    void Bind(Weapon* weapon, std::uint32_t maximumCharge,
              std::uint32_t countCharge,
              std::uint32_t* currentCharge,
              std::uint32_t chargeStep = 1U,
              float damage = 0.0F,
              int chargeCost = 0) noexcept;

    // projectileCreated is the result of the backend preparation step which
    // Weapon::CreateShot performed in Windows.  newCharge is used by
    // NetPlayer::DoShot to commit the charge carried by a replicated shot.
    bool Shot(bool projectileCreated, int newCharge = -1) noexcept;
    void Reload() noexcept;
    bool IsReadyShot(float delay) const noexcept;
    bool IsReadyShot() const noexcept;
    bool IsInstalled() const noexcept;
    bool HasShotCharge() const noexcept;

    std::uint32_t GetMaxCharge() const noexcept;
    std::uint32_t GetCntCharge() const noexcept;
    std::uint32_t GetCurCharge() const noexcept;
    std::uint32_t GetChargeStep() const noexcept;
    float GetDamage() const noexcept;
    int GetChargeCost() const noexcept;
    Weapon* GetWeapon() const noexcept;
    Weapon::Desc GetDesc() const;

private:
    Weapon* weapon_ = nullptr;
    std::uint32_t maximumCharge_ = 0U;
    std::uint32_t countCharge_ = 0U;
    std::uint32_t* currentCharge_ = nullptr;
    std::uint32_t chargeStep_ = 1U;
    float damage_ = 0.0F;
    int chargeCost_ = 0;
};

// Slot selection from Logic::Shot.  Slot order deliberately matches the
// Windows ShotSlots packet: Hyper, Mine, Weapon1..Weapon4.
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
};

// One source Weapon map object exists for every installed slot, including
// Hyper and Mine. This small owner replaces three unrelated session timers.
struct WeaponRack
{
    static constexpr std::size_t primarySlotCount = 4U;

    void Reset() noexcept;
    void OnProgress(float deltaTime) noexcept;

    std::array<Weapon, primarySlotCount> primary;
    Weapon hyper;
    Weapon mine;
};

} // namespace r3d::game::originalrace::source
