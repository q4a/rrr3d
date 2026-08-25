#pragma once

#include "OriginalSlot.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace r3d::game::originalrace::source
{

// Gameplay-owned contact rules from Proj. PhysX actor lookup and the final
// Jolt velocity/momentum writes remain backend adapters.
class Proj
{
public:
    struct Vec3
    {
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
    };

    struct Quat
    {
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
        float w = 1.0F;
    };

    struct ContactResult
    {
        Vec3 linearVelocity;
        float clutchStrength = 0.0F;
        bool setLinearVelocity = false;
        bool lockClutch = false;
        bool sendSpeedArrowEvent = false;
    };

    struct RocketUpdateResult
    {
        float positionZ = 0.0F;
        float clearance = 0.0F;
    };

    struct TorpedaUpdateResult
    {
        Vec3 direction;
        Vec3 linearVelocity;
        Quat rotation;
        float homingDelay = 0.0F;
        bool setLinearVelocity = false;
    };

    struct ThunderContactResult
    {
        Vec3 linearVelocity;
        float reflectionCooldown = 0.0F;
        bool setLinearVelocity = false;
    };

    struct TorqueResult
    {
        Vec3 localVelocityChange;
        bool apply = false;
    };

    struct LaunchResult
    {
        Vec3 direction;
        Vec3 linearVelocity;
        float speed = 0.0F;
    };

    struct MineUpdateResult
    {
        float timer = -1.0F;
        float visualScale = -1.0F;
        bool armed = true;
    };

    struct ImpulseContactResult
    {
        float damage = 0.0F;
        std::uint32_t hitCount = 0U;
        bool applyDamage = false;
        bool findNextTarget = false;
        bool destroy = false;
    };

    struct LaserUpdateResult
    {
        float distance = 0.0F;
        float damage = 0.0F;
        float beamWidthScale = 1.0F;
        float textureScale = 0.0F;
        bool applyDamage = false;
    };

    struct ContinuousContactResult
    {
        Vec3 impulse;
        float damage = 0.0F;
        bool applyImpulse = false;
    };

    struct SpringPrepareResult
    {
        Vec3 localVelocityChange;
        bool prepared = false;
        bool lockSpring = false;
    };

    struct TypeRules
    {
        bool rocketPrepare = false;
        bool attached = false;
        bool linkedToWeapon = false;
        bool ray = false;
        bool homing = false;
        bool ballistic = false;
        bool mineTestsLock = false;
    };

    struct DestroyResult
    {
        bool destroy = false;
        bool clearWeapon = false;
        bool clearTarget = false;
    };

    enum class BonusContactType : std::uint8_t
    {
        None,
        Medpack,
        Charge,
        Money,
        Immortal,
    };

    struct BonusContactResult
    {
        BonusContactType type = BonusContactType::None;
        float value = 0.0F;
        bool take = false;
    };

    static ContactResult SpeedArrowContact(
        Vec3 worldDirection, float damage) noexcept;
    static ContactResult LushaContact(
        Vec3 linearVelocity, float damage) noexcept;
    static ContactResult MasloContact(
        Vec3 carPosition, Vec3 carWorldRight, Vec3 oilPosition,
        Vec3 linearVelocity, float damage, bool arming,
        bool mineLocked, bool clutchLocked,
        bool clutchImmune) noexcept;

    // These are direct backend-neutral transcriptions of the source Proj
    // update/contact methods. Raycasts and final actor writes stay in the
    // Jolt adapter, while the original state transitions live here.
    static RocketUpdateResult RocketUpdate(
        float projectileZ, float trackZ, float boxHalfExtentZ,
        float clearance, bool trackHit) noexcept;
    static TorpedaUpdateResult TorpedaUpdate(
        float deltaTime, Vec3 position, Quat rotation,
        Vec3 storedVelocity, float homingDelay, bool hasTarget,
        Vec3 targetPosition, float sourceSpeed, bool speedRelative,
        float angleSpeed) noexcept;
    static float ThunderUpdate(
        float reflectionCooldown, float deltaTime) noexcept;
    static ThunderContactResult ThunderContact(
        Vec3 linearVelocity, Vec3 contactNormal,
        float reflectionCooldown,
        bool shotTransparencyContact) noexcept;
    static Quat ResonanseUpdate(
        Quat rotation, float angleSpeed, float deltaTime) noexcept;
    static TorqueResult RocketContactTorque(
        Vec3 contactPoint, Vec3 linearVelocity, float mass) noexcept;
    static LaunchResult CalcSpeed(
        Vec3 worldDirection, Vec3 weaponVelocity, float sourceSpeed,
        float speedRelativeMinimum, bool speedRelative) noexcept;
    static MineUpdateResult MineUpdate(
        float timer, float deltaTime, float delay = 0.25F) noexcept;
    static bool MineContactAllowed(
        bool hasTarget, bool testMineLock, bool mineBugEnabled,
        bool targetMineLocked, float armingTimer,
        bool targetIsOwner) noexcept;
    static bool MineRipUpdate(
        float timeLife, float splitTime, bool death) noexcept;
    static ImpulseContactResult ImpulseContact(
        bool hasContactActor, bool hasTarget, bool contactIsTarget,
        std::uint32_t hitCount, float damage) noexcept;
    static float PrepareMaximumLife(
        float speed, float maximumDistance,
        float sampledMinimumLife) noexcept;
    static LaserUpdateResult LaserUpdate(
        float maximumDistance, bool hit, float hitDistance,
        float deltaTime, float damage, bool distort,
        float timeLife, float maximumTimeLife) noexcept;
    static ContinuousContactResult FireContact(
        bool hasTarget, float damage, float deltaTime) noexcept;
    static ContinuousContactResult DrobilkaContact(
        bool hasTarget, float damage, float deltaTime) noexcept;
    static ContinuousContactResult SonarContact(
        bool hasTarget, Vec3 linearVelocity, float mass,
        float damage, float deltaTime) noexcept;
    static SpringPrepareResult SpringPrepare(
        bool hasCar, bool wheelsContact, float speed) noexcept;
    static TypeRules GetTypeRules(std::uint32_t type) noexcept;
    static DestroyResult OnDestroy(
        bool senderIsWeapon, bool parentIsWeapon,
        bool senderIsTarget) noexcept;
    static BonusContactResult BonusContact(
        std::uint32_t type, bool hasTarget, float damage,
        float targetMaximumLife) noexcept;
};

// MapObj.cpp registers gotProj as AutoProj.  Unlike a weapon shot, this
// source object prepares itself when Logic is attached, keeps the serialized
// map transform, and releases only its prepared state when Logic goes away.
// MineUpdate owns the short arming interval used by the oil model/contact.
class AutoProj
{
public:
    static constexpr std::uint32_t masloType = 10U;

    void Reset(std::uint32_t type) noexcept;
    void LogicInited(bool hasLogic = true) noexcept;
    void LogicReleased() noexcept;
    void OnProgress(float deltaTime) noexcept;

    bool IsPrepared() const noexcept;
    bool IsArming() const noexcept;
    float GetModelScale() const noexcept;
    std::uint32_t GetType() const noexcept;

private:
    static bool UsesMineUpdate(std::uint32_t type) noexcept;

    std::uint32_t type_ = 0U;
    float armingTimer_ = -1.0F;
    // Negative means that the source Proj did not touch actor scale.
    float modelScale_ = -1.0F;
    bool prepared_ = false;
};

// GameBase.cpp::ShotEffect receives one OnShot callback only after
// Weapon::PrepareProj succeeds. The backend may create a child visual and a
// Source3d, but this owner preserves the callback lifetime per weapon actor.
class ShotEffect
{
public:
    void Reset() noexcept;
    void OnShot() noexcept;
    std::uint64_t GetShotCount() const noexcept;

private:
    std::uint64_t shotCount_ = 0U;
};

// Backend-neutral transcription of the original Weapon timer and Desc
// ownership. Projectile preparation remains at the Jolt/bgfx session
// boundary; readiness and successful-shot lifetime belong here.
class Weapon
{
public:
    struct ProjectileDesc
    {
        std::uint32_t type = 0U;
        float speed = 0.0F;
        float maximumDistance = 0.0F;
        float damage = 0.0F;
    };

    struct Desc
    {
        float shotDelay = 0.0F;
        std::vector<ProjectileDesc> projectiles;

        const ProjectileDesc& Front() const noexcept;
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
    // Weapon::CreateShot dispatches Behaviors::OnShot separately for every
    // projectile accepted by PrepareProj.  Keep this separate from the
    // WeaponItem transaction because one trigger may create several actors.
    void OnProjectilePrepared() noexcept;

    const Desc& GetDesc() const noexcept;
    void SetDesc(const Desc& value);
    void SetDesc(float shotDelay,
                 std::span<const std::uint32_t> projectileTypes);
    void SetDesc(float shotDelay,
                 std::span<const ProjectileDesc> projectiles);
    const ShotEffect& GetShotEffect() const noexcept;

private:
    Desc desc_;
    float shotTime_ = 0.0F;
    ShotEffect shotEffect_;
};

// Backend-neutral transcription of Player::WeaponItem. The Windows object
// owns its charge fields directly. The pointer accepted by Bind is only a
// load-time source for the staged RacerRuntime setup; live mutations stay in
// this object.
class WeaponItem : public SlotItem
{
public:
    explicit WeaponItem(
        SlotType type = SlotType::Weapon) noexcept;
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

    WeaponItem* IsWeaponItem() noexcept override;
    const WeaponItem* IsWeaponItem() const noexcept override;
    void OnCreateCar() noexcept override;
    void OnDestroyCar() noexcept override;

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
    void SetMaxCharge(std::uint32_t value) noexcept;
    std::uint32_t GetCntCharge() const noexcept;
    void SetCntCharge(std::uint32_t value) noexcept;
    std::uint32_t GetCurCharge() const noexcept;
    void SetCurCharge(std::uint32_t value) noexcept;
    std::uint32_t GetChargeStep() const noexcept;
    void SetChargeStep(std::uint32_t value) noexcept;
    float GetDamage(bool statisticsDamage = false) const noexcept;
    void SetDamage(float value) noexcept;
    int GetChargeCost() const noexcept;
    void SetChargeCost(int value) noexcept;
    const Weapon::Desc& GetWpnDesc() const noexcept;
    void SetWpnDesc(const Weapon::Desc& value);
    Weapon* GetWeapon() const noexcept;
    Weapon::Desc GetDesc() const;

private:
    Weapon* weapon_ = nullptr;
    bool carAttached_ = false;
    std::uint32_t maximumCharge_ = 0U;
    std::uint32_t countCharge_ = 0U;
    std::uint32_t currentCharge_ = 0U;
    std::uint32_t chargeStep_ = 1U;
    float damage_ = 0.0F;
    int chargeCost_ = 0;
    Weapon::Desc weaponDesc_;
};

class HyperItem final : public WeaponItem
{
public:
    HyperItem() noexcept : WeaponItem(SlotType::Hyper) {}
};

class MineItem final : public WeaponItem
{
public:
    MineItem() noexcept : WeaponItem(SlotType::Mine) {}
};

// Exact gameplay-owned portion of Player::DroidItem. The serialized
// repairValue is retained for source/profile parity, although the Windows
// OnProgress implementation heals by the literal 5.0f value.
class DroidItem : public WeaponItem
{
public:
    DroidItem() noexcept;
    DroidItem(Weapon* weapon, std::uint32_t maximumCharge,
              std::uint32_t countCharge,
              std::uint32_t* currentCharge,
              float repairValue = 5.0F,
              float repairPeriod = 1.0F) noexcept;

    void Bind(Weapon* weapon, std::uint32_t maximumCharge,
              std::uint32_t countCharge,
              std::uint32_t* currentCharge,
              float repairValue, float repairPeriod) noexcept;
    void OnCreateCar() noexcept override;
    void OnDestroyCar() noexcept override;
    float OnProgress(float deltaTime, float& life,
                     float maximumLife, bool death) noexcept;

    float GetRepairValue() const noexcept;
    void SetRepairValue(float value) noexcept;
    float GetRepairPeriod() const noexcept;
    void SetRepairPeriod(float value) noexcept;
    float GetRepairTime() const noexcept;
    bool IsProgressRegistered() const noexcept;

private:
    float repairValue_ = 5.0F;
    float repairPeriod_ = 1.0F;
    float time_ = 0.0F;
    bool progressRegistered_ = false;
};

// Exact gameplay-owned portion of Player::ReflectorItem. Logic::Damage
// decides whether touch damage bypasses it; this object owns only the source
// coefficient and clamped reflection formula.
class ReflectorItem : public WeaponItem
{
public:
    ReflectorItem() noexcept;
    ReflectorItem(Weapon* weapon, std::uint32_t maximumCharge,
                  std::uint32_t countCharge,
                  std::uint32_t* currentCharge,
                  float reflectValue = 0.25F) noexcept;

    void Bind(Weapon* weapon, std::uint32_t maximumCharge,
              std::uint32_t countCharge,
              std::uint32_t* currentCharge,
              float reflectValue) noexcept;
    float GetReflectValue() const noexcept;
    void SetReflectValue(float value) noexcept;
    float Reflect(float damage) const noexcept;

private:
    float reflectValue_ = 0.25F;
};

// One source Weapon map object exists for every installed Player slot,
// including Hyper and Mine. The rack is embedded in Player and replaces the
// old parallel session timers without changing the backend projectile step.
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
