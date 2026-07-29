#pragma once

#include "OriginalProfile.h"
#include "OriginalRace.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace r3d::game::originalrace
{

enum class RacePhase
{
    Countdown,
    Racing,
    Finished,
    Paused,
};

enum class RaceEventKind
{
    CountdownChanged,
    Checkpoint,
    Lap,
    Finish,
    Respawn,
    WeaponFired,
    WeaponShotEffect,
    Damage,
    Kill,
    Bonus,
    DecorationDestroyed,
    MinePlaced,
    HyperActivated,
    Achievement,
    ProjectileImpact,
    VehicleDestroyed,
    LowLife,
};

enum class PickSlot : std::uint8_t
{
    None,
    Primary,
    Hyper,
    Mine,
};

struct RaceControl
{
    r3d::physics::VehicleInput driving;
    bool useWeapon = false;
    bool useAllWeapons = false;
    bool useMine = false;
    bool useHyper = false;
    bool changeWeapon = false;
    int weaponChange = 1;
    int weaponSlot = -1;
    int fireWeaponSlot = -1;
    bool reset = false;
};

struct RacerRuntime
{
    static constexpr std::size_t invalidWeapon =
        std::numeric_limits<std::size_t>::max();

    std::uint32_t completedLaps = 0;
    std::size_t nextPathNode = 1;
    std::uint32_t place = 1;
    float life = 100.0F;
    float maximumLife = 100.0F;
    std::uint32_t ammunition = 10;
    std::uint32_t mines = 0;
    std::uint32_t mineCapacity = 0;
    std::array<std::size_t, PlayerProfile::weaponSlotCount> weaponSlots{
        invalidWeapon, invalidWeapon, invalidWeapon, invalidWeapon};
    std::array<std::uint32_t, PlayerProfile::weaponSlotCount> weaponCharges{};
    std::array<std::uint32_t, PlayerProfile::weaponSlotCount>
        weaponCapacity{};
    // Proj::DrobilkaUpdate rotates the mounted weapon actor itself.  Keep
    // that per-slot actor state separate from projectile age so both the
    // renderer and the contact transform observe the same source rotation.
    std::array<float, PlayerProfile::weaponSlotCount> weaponSpinRadians{};
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
    bool lowLife = false;
};

struct RespawnRequest
{
    std::size_t racer = 0;
    Vec3 position;
    Vec3 direction{1.0F, 0.0F, 0.0F};
};

struct VelocityRequest
{
    std::size_t racer = 0;
    Vec3 delta;
};

struct AngularVelocityRequest
{
    std::size_t racer = 0;
    Vec3 delta;
};

struct RaceEvent
{
    RaceEventKind kind = RaceEventKind::Checkpoint;
    std::size_t racer = 0;
    std::size_t target = 0;
    Vec3 position;
    float value = 0.0F;
    PickSlot pickSlot = PickSlot::None;
    std::size_t weapon = RacerRuntime::invalidWeapon;
    bool touchDamage = false;
    // GameObject::Damage emits car death for dtMine but deliberately does
    // not emit cPlayerKill.  Keep destruction consumers active while
    // excluding that event from kill HUD/commentary/achievements.
    bool killCredit = true;
};

struct RaceEffect
{
    RaceEventKind kind = RaceEventKind::WeaponFired;
    Vec3 origin;
    Vec3 target;
    float seconds = 0.0F;
    float totalSeconds = 0.0F;
    std::size_t weapon = 0;
    std::size_t projectile = 0;
    std::uint8_t visualVariant = 0;
    std::size_t bonus = RacerRuntime::invalidWeapon;
    bool ignoreRotation = false;
    std::size_t racer = RacerRuntime::invalidWeapon;
    std::size_t vehicleEffect = RacerRuntime::invalidWeapon;
    Transform transform;
    std::size_t mountSlot = RacerRuntime::invalidWeapon;
};

struct MineRuntime
{
    std::size_t owner = 0;
    std::size_t weapon = 0;
    std::size_t projectile = 0;
    std::uint8_t visualVariant = 0;
    Vec3 position;
    Quat rotation;
    Vec3 velocity;
    ProjectileCollisionBox collision;
    float seconds = 0.0F;
    float damage = 0.0F;
    float impulseSpeed = 0.0F;
    float maximumLife = -1.0F;
    std::uint32_t type = 11U;
    // Weapon-created mines retain their source car pointer during the
    // 0.25-second MineUpdate arming window.  AutoProj fragments do not.
    bool linkedToOwner = true;
    bool active = true;
};

struct ProjectileRuntime
{
    std::size_t owner = 0;
    std::size_t weapon = 0;
    std::size_t projectile = 0;
    std::size_t mountSlot = 0;
    Vec3 position;
    Vec3 direction{1.0F, 0.0F, 0.0F};
    Vec3 velocity;
    Quat rotation;
    float speed = 0.0F;
    float maximumDistance = 0.0F;
    float impactDistance = 0.0F;
    float damage = 0.0F;
    float distance = 0.0F;
    float angularSpeed = 0.0F;
    float homingDelay = 0.0F;
    float lifeSeconds = 0.0F;
    float ageSeconds = 0.0F;
    float reflectionCooldown = 0.0F;
    // Proj::RocketUpdate stores its current clearance above TrackPlane in
    // _vec1.z and only lowers it when the terrain rises into the projectile.
    float trackClearance = 0.0F;
    std::size_t target = RacerRuntime::invalidWeapon;
    std::uint32_t hitCount = 0;
    bool attached = false;
    bool directWeapon = false;
    bool ballistic = false;
    bool active = true;
};

class OriginalRaceSession
{
public:
    explicit OriginalRaceSession(const Race& race);

    void reset();
    void applyPlayerProfile(const PlayerProfile& profile);
    void writePlayerProfile(PlayerProfile& profile) const;
    void applyAchievementProfile(const ProfileState& profile);
    void writeAchievementProfile(ProfileState& profile) const;
    void setEnableMineBug(bool enabled) noexcept;
    void setSpringBorders(bool enabled) noexcept;
    void setPaused(bool paused) noexcept;
    void update(float seconds,
                const std::vector<r3d::physics::VehicleState>& vehicles,
                const RaceControl& humanControl);

    RacePhase phase() const noexcept;
    float countdownSeconds() const noexcept;
    float elapsedSeconds() const noexcept;
    const std::vector<r3d::physics::VehicleInput>& vehicleInputs() const
        noexcept;
    const std::vector<RacerRuntime>& racers() const noexcept;
    const std::vector<bool>& decorationActive() const noexcept;
    const std::vector<bool>& bonusActive() const noexcept;
    const std::vector<RaceEvent>& events() const noexcept;
    const std::vector<RaceEffect>& effects() const noexcept;
    const std::vector<MineRuntime>& mines() const noexcept;
    const std::vector<ProjectileRuntime>& projectiles() const noexcept;
    std::vector<RespawnRequest> takeRespawns();
    std::vector<VelocityRequest> takeVelocityRequests();
    std::vector<AngularVelocityRequest> takeAngularVelocityRequests();

private:
    const TracePoint& tracePoint(std::size_t pathNode) const;
    void updateProgress(std::size_t racer,
                        const r3d::physics::VehicleState& vehicle);
    r3d::physics::VehicleInput aiInput(
        std::size_t racer,
        const r3d::physics::VehicleState& vehicle,
        float seconds);
    void updatePlaces(
        const std::vector<r3d::physics::VehicleState>& vehicles);
    void updateGameplay(
        float seconds,
        const std::vector<r3d::physics::VehicleState>& vehicles,
        const RaceControl& humanControl);
    void queueRespawn(std::size_t racer,
                      const r3d::physics::VehicleState& vehicle);
    void destroyRacer(
        std::size_t racer, std::size_t attacker, Vec3 position,
        const r3d::physics::VehicleState& vehicle,
        bool touchDamage = false, bool killCredit = true);
    std::size_t findWeapon(std::string_view record,
                           WeaponSlot slot) const noexcept;
    void syncSelectedWeapon(RacerRuntime& racer) const noexcept;
    float damageAfterSupport(std::size_t racer, float damage,
                             bool touchDamage) const noexcept;
    bool damageDecorationAlongRay(
        Vec3 origin, Vec3 direction, float maximumDistance,
        float damage, std::size_t attacker);
    bool damageDecorationWithBox(
        Transform transform, ProjectileCollisionBox collision,
        float damage, std::size_t attacker,
        Vec3* contactPoint = nullptr);
    bool damageDecoration(std::size_t instance, float damage,
                          std::size_t attacker);
    void updateAchievements(float seconds);
    void completeAchievement(std::size_t achievement);

    const Race& race_;
    RacePhase phase_ = RacePhase::Countdown;
    RacePhase phaseBeforePause_ = RacePhase::Countdown;
    float countdownSeconds_ = 3.0F;
    float elapsedSeconds_ = 0.0F;
    int countdownDisplay_ = 3;
    std::vector<RacerRuntime> racers_;
    std::vector<r3d::physics::VehicleInput> vehicleInputs_;
    std::vector<bool> decorationActive_;
    std::vector<float> decorationLife_;
    std::vector<bool> bonusActive_;
    std::vector<std::array<float, PlayerProfile::weaponSlotCount>>
        weaponCooldown_;
    std::vector<float> mineCooldown_;
    std::vector<float> hyperCooldown_;
    std::vector<float> repairSeconds_;
    std::vector<float> stuckSeconds_;
    std::vector<float> touchCooldown_;
    std::vector<bool> aiBrake_;
    std::vector<Vec3> previousPositions_;
    std::vector<RaceEvent> events_;
    std::vector<RaceEffect> effects_;
    std::vector<MineRuntime> mines_;
    std::vector<ProjectileRuntime> projectiles_;
    std::vector<RespawnRequest> respawns_;
    std::vector<VelocityRequest> velocityRequests_;
    std::vector<AngularVelocityRequest> angularVelocityRequests_;
    PlayerProfile initialPlayerProfile_;
    std::uint32_t initialAchievementPoints_ = 0;
    std::map<std::string, std::uint32_t>
        initialAchievementIterations_;
    std::uint32_t achievementPoints_ = 0;
    float achievementMultiplier_ = 1.2F;
    std::vector<std::uint32_t> achievementIterations_;
    std::vector<std::uint32_t> achievementConditionCounters_;
    std::vector<float> achievementConditionTimers_;
    std::uint32_t achievementGlobalKills_ = 0;
    std::uint32_t achievementPreviousLapPlace_ = 0;
    bool enableMineBug_ = true;
    bool springBorders_ = true;
};

bool runOriginalRaceSessionSmokeTest(const Race& race, std::string& error);

} // namespace r3d::game::originalrace
