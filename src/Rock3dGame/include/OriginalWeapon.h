#pragma once

#include "OriginalGameObject.h"
#include "OriginalRace.h"
#include "OriginalSlot.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace r3d::game::originalrace::source
{

class Player;

// Gameplay-owned contact rules from Proj. PhysX actor lookup and the final
// Jolt velocity/momentum writes remain backend adapters.
class Proj : public GameObject, public GameObjectListener
{
public:
    // Weapon.h::Proj::Type serialized order. The original cProjTypeEnd alias
    // is intentionally not reproduced because it aliases ptRocket (zero).
    enum class ProjectileType : std::uint32_t
    {
        Rocket = 0U,
        Hyper,
        Torpeda,
        Laser,
        Medpack,
        Charge,
        Money,
        Immortal,
        SpeedArrow,
        Lusha,
        Maslo,
        Mine,
        MineRip,
        MinePiece,
        Fire,
        Drobilka,
        Sonar,
        Spring,
        FrostRay,
        Mortira,
        Crater,
        Impulse,
        Thunder,
        Resonanse,
        MineProton,
    };

    struct Vec3
    {
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;

        bool operator==(const Vec3&) const noexcept = default;
    };

    struct Quat
    {
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
        float w = 1.0F;

        bool operator==(const Quat&) const noexcept = default;
    };

    // Weapon.h::Proj::ShotDesc is copied into every projectile before its
    // type-specific PrepareProj path runs.  The concrete target listener is
    // maintained by Proj; this value also preserves the independent world
    // target used by Weapon::Shot(const glm::vec3&).
    struct ShotDesc
    {
        GameObject* targetMapObject = nullptr;
        Vec3 target{};
    };

    // PhysX supplied the final actor transform from PrepareProj.  The Jolt
    // adapter supplies those backend values explicitly while source Logic,
    // target attribution and ownership remain in the original transaction.
    struct ShotContext
    {
        Logic* logic = nullptr;
        ShotDesc shot;
        std::size_t playerId = GameObject::undefinedPlayerId;
        // PrepareProj can reject mine placement, Spring without complete
        // wheel contact, or a backend actor which failed to materialize.
        bool preparationAccepted = true;
        float maximumLife = -1.0F;
        Vec3 position{};
        Quat rotation{};
        // RocketPrepare/TorpedaPrepare retain the prepared actor velocity in
        // _vec1. The backend supplies the result of the source CalcSpeed
        // formula because the final actor remains owned by Jolt.
        Vec3 launchVelocity{};
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

    // Proj owns the PhysX scene-query shape in the Windows source.  The
    // portable Race session only adapts this request to Jolt (or to the
    // headless collision fallback); it must not reconstruct offsets, masks
    // or maximum distances independently.
    enum class SceneRayGroup : std::uint8_t
    {
        None,
        Projectile,
        TrackPlane,
        TrackPlaneAndShotTrack,
    };

    struct SceneRayQuery
    {
        Vec3 origin;
        Vec3 direction{1.0F, 0.0F, 0.0F};
        float maximumDistance = 0.0F;
        SceneRayGroup group = SceneRayGroup::None;
        bool rejectShotTrack = false;
        bool valid = false;
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

    struct MineRipUpdateResult
    {
        MineUpdateResult arming;
        bool split = false;
    };

    struct MineRipChildSpawn
    {
        ProjectileDefinition definition;
        Vec3 linearVelocity;
        float maximumLife = -1.0F;
    };

    struct MineRipSplitPlan
    {
        std::vector<MineRipChildSpawn> children;
        bool destroyParent = false;
    };

    using RandomUnitSource = std::function<float()>;

    struct DeathProjectileSpawnPlan
    {
        ProjectileDefinition definition;
        Vec3 positionOffset;
        std::size_t projectile =
            ProjectileDefinition::invalidProjectile;
        float maximumLife = -1.0F;
        bool ignoreSenderCar = false;
        bool spawn = false;
    };

    // GameObject::DoDeath propagates Death to both initialized include
    // models. Only children with FxSystemWaitingEnd resurrect into the
    // world; model3 is never an initialized Proj child (Frost installs it
    // on the contacted car instead).
    struct SourceModelRelease
    {
        ObjectDefinition definition;
        Vec3 position;
        Quat rotation;
        Vec3 scale{1.0F, 1.0F, 1.0F};
        bool secondary = false;
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

    // Portable graph state owned by the concrete Proj. In the Windows
    // renderer LaserUpdate writes these values directly to _model's Sprite
    // position/sizes and sampler[0] scale.
    struct LaserVisualState
    {
        float distance = 0.0F;
        float beamWidthScale = 1.0F;
        float textureScale = 1.0F;
        bool valid = false;
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

    // Backend-neutral result of the source Proj::OnContact type switch.
    // PhysX/Jolt contact extraction remains outside; this identifies the
    // exact source handler and the DamageTarget attribution it performs.
    enum class ContactHandler : std::uint8_t
    {
        None,
        Rocket,
        Torpeda,
        Medpack,
        Charge,
        Money,
        Immortal,
        SpeedArrow,
        Lusha,
        Maslo,
        Mine,
        MineRip,
        MinePiece,
        Fire,
        Drobilka,
        Sonar,
        Mortira,
        Crater,
        Impulse,
        Thunder,
        Resonanse,
        MineProton,
    };

    struct ContactRoute
    {
        ContactHandler handler = ContactHandler::None;
        DamageType damageType = DamageType::Simple;
        bool rocketResponse = false;
        bool testMineLock = false;
        bool appliesDamage = false;
    };

    // Source Proj::OnProgress switch. The selected method remains concrete;
    // renderer queries and final Jolt actor writes are adapter boundaries.
    enum class ProgressHandler : std::uint8_t
    {
        None,
        Rocket,
        Torpeda,
        Laser,
        Fire,
        Maslo,
        Mine,
        MineRip,
        MineProton,
        Drobilka,
        Spring,
        FrostRay,
        Impulse,
        Thunder,
        Resonanse,
    };

    struct ProgressRoute
    {
        ProgressHandler handler = ProgressHandler::None;
        bool attached = false;
        bool ray = false;
        bool homing = false;
        bool rocketHeight = false;
        bool mineArming = false;
    };

    struct AttachedProgressResult
    {
        Vec3 position{};
        Quat rotation{};
        Vec3 direction{};
        Vec3 linearVelocity{};
        bool setLinearVelocity = false;
        bool valid = false;
    };

    struct FreeProgressResult
    {
        Vec3 position{};
        Quat rotation{};
        Vec3 linearVelocity{};
        bool setLinearVelocity = false;
        bool valid = false;
    };

    struct PlacedMineProgressResult
    {
        MineUpdateResult arming{};
        bool split = false;
        bool valid = false;
    };

    enum class PrepareHandler : std::uint8_t
    {
        None,
        Rocket,
        Hyper,
        Torpeda,
        Laser,
        Medpack,
        Charge,
        Money,
        Immortal,
        SpeedArrow,
        Lusha,
        Maslo,
        Mine,
        MineRip,
        MinePiece,
        Fire,
        Drobilka,
        Sonar,
        Spring,
        FrostRay,
        Mortira,
        Crater,
        Impulse,
        Thunder,
        Resonanse,
        MineProton,
    };

    struct PreparationRoute
    {
        PrepareHandler handler = PrepareHandler::None;
        bool valid = false;
        bool initializeModel = false;
        bool initializeSecondaryModel = false;
        bool linkedToWeapon = false;
        bool attached = false;
        bool ray = false;
        bool rocketPrepare = false;
        bool homing = false;
        bool ballistic = false;
        bool minePlacement = false;
        bool lockMineOnPlacement = false;
        bool mineTestsLock = false;
        bool ignoreWeaponContact = false;
        bool requiresWeapon = false;
    };

    // Proj::DamageTarget owns damage attribution in the Windows source.
    // Applying the command remains a Logic/session boundary so network
    // authority can preserve the original request/response ordering.
    struct DamageCommand
    {
        Logic* logic = nullptr;
        GameObject* senderCar = nullptr;
        GameObject* target = nullptr;
        std::size_t playerId = GameObject::undefinedPlayerId;
        float damage = 0.0F;
        DamageType damageType = DamageType::Simple;
        bool valid = false;
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

    // Complete source transaction for moving projectiles which collide with
    // a concrete car/destructible actor. The backend supplies contact geometry
    // and consumes force/effect commands; Proj owns the OnContact dispatch,
    // damage attribution and scratch-state mutation.
    struct DynamicContactResult
    {
        ContactRoute route;
        DamageCommand damage;
        TorqueResult torque;
        ContinuousContactResult continuous;
        ImpulseContactResult impulse;
        bool handled = false;
        bool destroyBeforeDamage = false;
        bool destroyAfterDamage = false;
    };

    // Backend-neutral result of Proj::MineContact(GameObject*, point).
    // The Windows transaction kills the mine before validating/damaging the
    // target, then applies the vertical contact force after damage.
    struct MineContactResult
    {
        DamageCommand damage;
        Vec3 impulse;
        bool handled = false;
        bool destroyBeforeDamage = false;
        bool applyImpulseAfterDamage = false;
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
    static SceneRayQuery MinePlacementSceneRay(
        Vec3 projectileWorldPosition) noexcept;
    static bool AcceptSceneRayHit(
        const SceneRayQuery& query, bool hit,
        bool shotTrackHit) noexcept;
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
    static GameCar* ResolveContactCar(GameObject* target) noexcept;
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
    static SpringPrepareResult SpringPrepare(
        GameObject* weapon, float speed) noexcept;
    static DamageType DamageTypeFor(
        std::uint32_t type) noexcept;
    static ContactRoute ContactRouteFor(
        std::uint32_t type, bool projectileDestroyed,
        bool targetDestroyed) noexcept;
    static ProgressRoute ProgressRouteFor(
        std::uint32_t type) noexcept;
    static PreparationRoute PreparationRouteFor(
        std::uint32_t type) noexcept;
    static TypeRules GetTypeRules(std::uint32_t type) noexcept;
    static BonusContactResult BonusContact(
        std::uint32_t type, bool hasTarget, float damage,
        float targetMaximumLife) noexcept;
    static ProjectileCollisionBox ComputeAABB(
        const ProjectileDefinition& description,
        bool onlyModel) noexcept;

    // Concrete stateful dispatch corresponding to Proj::OnProgress and
    // Proj::OnContact. Jolt supplies query/contact values and consumes the
    // returned actor writes; the source scratch members stay owned here.
    RocketUpdateResult ProgressRocket(
        float projectileZ, float trackZ, float boxHalfExtentZ,
        bool trackHit) noexcept;
    Quat ProgressResonanse(
        Quat rotation, float deltaTime) noexcept;
    TorpedaUpdateResult ProgressTorpeda(
        float deltaTime, Vec3 position, Quat rotation,
        bool hasTarget, Vec3 targetPosition) noexcept;
    MineUpdateResult ProgressMine(
        float deltaTime, float delay = 0.25F) noexcept;
    // Original Proj::OnProgress first advances GameObject::_timeLife. The
    // session calls this backend half only after Logic::OnProgress, so it
    // must read that concrete clock rather than increment it again.
    MineRipUpdateResult ProgressMineRip(
        float deltaTime, float delay = 0.25F) noexcept;
    MineRipSplitPlan BuildMineRipSplitPlan(
        const RandomUnitSource& randomUnit) const;
    DeathProjectileSpawnPlan BuildDeathProjectileSpawnPlan(
        std::span<const ProjectileDefinition> weaponProjectiles,
        const DeathEffect::SpawnResult& deathEffect,
        const RandomUnitSource& randomUnit) const;
    std::vector<SourceModelRelease>
        BuildSourceModelReleasePlan() const;
    LaunchResult PrepareLaunch(
        Vec3 worldDirection, Vec3 weaponVelocity) noexcept;
    float PrepareMaximumLife(
        float sampledMinimumLife) noexcept;
    float ProgressThunder(float deltaTime) noexcept;
    ThunderContactResult ContactThunder(
        Vec3 linearVelocity, Vec3 contactNormal,
        bool shotTransparencyContact) noexcept;
    TorqueResult ContactRocket(
        GameObject* target, Vec3 contactPoint,
        Vec3 linearVelocity) const noexcept;
    ImpulseContactResult ContactImpulse(
        bool hasContactActor, bool hasTarget,
        bool contactIsTarget) noexcept;
    DynamicContactResult ContactDynamic(
        GameObject* target, Vec3 contactPoint,
        Vec3 linearVelocity, float deltaTime) noexcept;
    MineContactResult ResolveMineContact(
        GameObject* target) noexcept;
    Player* FindNextTarget(
        Player* currentTarget, Player* weaponOwner,
        std::span<Player* const> players, float viewAngle) noexcept;
    void RetargetImpulse(GameObject* target) noexcept;
    LaserUpdateResult ProgressLaser(
        float maximumDistance, bool hit, float hitDistance,
        float deltaTime, bool distort,
        Vec3 worldDirection) noexcept;
    LaserUpdateResult ProgressFrostRay(
        float maximumDistance, bool hit, float hitDistance,
        float deltaTime,
        Vec3 worldDirection) noexcept;
    bool AttachFrostSlow(
        GameObject* target, Player* targetPlayer,
        std::size_t weapon, std::size_t projectile,
        const ObjectDefinition* effectDefinition = nullptr) noexcept;
    ContinuousContactResult ContactFire(
        GameObject* target, float deltaTime) const noexcept;
    ContinuousContactResult ContactDrobilka(
        GameObject* target, float deltaTime,
        Vec3 contactPoint) noexcept;
    ContinuousContactResult ContactSonar(
        GameObject* target, Vec3 linearVelocity,
        float deltaTime) const noexcept;
    SpringPrepareResult PrepareSpring() noexcept;
    bool ContactMine(
        GameObject* target, bool mineBugEnabled) const noexcept;
    ContactResult ContactSpeedArrow(
        GameObject* target) const noexcept;
    ContactResult ContactLusha(
        GameObject* target, Vec3 linearVelocity) const noexcept;
    ContactResult ContactMaslo(
        GameObject* target, Vec3 carPosition,
        Vec3 carWorldRight, Vec3 oilPosition,
        Vec3 linearVelocity) noexcept;
    BonusContactResult ContactBonus(
        GameObject* target, Player* targetPlayer) const noexcept;
    DamageCommand DamageTarget(
        GameObject* target, float damage,
        DamageType damageType = DamageType::Simple) noexcept;
    // Exact first half of Proj::OnContact: GameObject listeners run before
    // the projectile/target live-state guard and the type switch.
    ContactRoute BeginContact(GameObject* target) noexcept;
    ContactRoute RouteContact(bool targetDestroyed) const noexcept;
    ProgressRoute RouteProgress() const noexcept;
    PreparationRoute RoutePreparation() const noexcept;
    SceneRayQuery ProgressSceneRay(
        Vec3 worldPosition, Vec3 worldDirection) const noexcept;
    AttachedProgressResult ProgressAttached(
        Vec3 weaponPosition, Quat weaponRotation,
        Vec3 weaponScale, Vec3 weaponLinearVelocity,
        float deltaTime) noexcept;
    FreeProgressResult ProgressFree(
        Vec3 position, Quat rotation, Vec3 linearVelocity,
        float deltaTime, bool trackHit, float trackZ,
        float boxHalfExtentZ, bool shotTransparencyContact,
        Vec3 contactNormal) noexcept;
    PlacedMineProgressResult ProgressPlacedMine(
        Vec3 position, Quat rotation,
        float deltaTime, float armingDelay = 0.25F) noexcept;
    void ProgressDrobilka(float deltaTime) noexcept;

    Proj();
    ~Proj() override;
    Proj* IsProj() noexcept override;
    const Proj* IsProj() const noexcept override;
    void PrepareSource(
        const ProjectileDefinition& description,
        GameObject* weapon,
        const ShotContext& context) noexcept;
    void SetSourceWeapon(
        GameObject* value, bool linkToWeapon = false) noexcept;
    void SetSourceTarget(GameObject* value) noexcept;
    void SetShot(const ShotDesc& value) noexcept;
    ShotDesc GetShot() const noexcept;
    void SyncSourceTransform(
        const Vec3& position, const Quat& rotation) noexcept;
    void SyncSourceWeaponTransform(
        const Vec3& position, const Quat& rotation) noexcept;
    // Backend adapters expose the five scratch members used by the original
    // type-specific Proj methods. Their meaning depends on description.type:
    // _time1 is mine arming/torpedo homing/thunder reflection time, _vec1 is
    // torpedo velocity or rocket track clearance, and _tick1 counts Impulse
    // chain hits.
    void ResetSourceRuntimeState() noexcept;
    float GetSourceTimer() const noexcept;
    void SetSourceTimer(float value) noexcept;
    const Vec3& GetSourceVector() const noexcept;
    void SetSourceVector(Vec3 value) noexcept;
    std::uint32_t GetSourceTick() const noexcept;
    void SetSourceTick(std::uint32_t value) noexcept;
    bool GetSourceState() const noexcept;
    void SetSourceState(bool value) noexcept;
    bool GetIgnoreContactProj() const noexcept;
    void SetIgnoreContactProj(bool value) noexcept;
    void SetExternalLifetimeManaged(bool value) noexcept;
    bool IsExternalLifetimeManaged() const noexcept;
    bool InitSourceModel(bool secondary = false);
    bool FreeSourceModel(
        bool secondary = false, bool remove = false) noexcept;
    MapObj* GetSourceModel() noexcept;
    const MapObj* GetSourceModel() const noexcept;
    MapObj* GetSourceModel2() noexcept;
    const MapObj* GetSourceModel2() const noexcept;
    const LaserVisualState& GetLaserVisualState() const noexcept;
    const ProjectileDefinition& GetDesc() const noexcept;
    ProjectileCollisionBox ComputeAABB(bool onlyModel) const noexcept;
    GameObject* GetSourceWeapon() const noexcept;
    GameObject* GetSourceTarget() const noexcept;
    std::size_t GetSourcePlayerId() const noexcept;
    bool IsPrepared() const noexcept;
    void ConfigureDeathEffect(
        bool effectPhysicsIgnoreSenderCar,
        bool targetChild) noexcept;
    DeathEffect::SpawnResult DestroyWithEffect(
        GameObject* target, bool logicAvailable,
        bool senderIsWeaponProjectile,
        DamageType damageType = DamageType::Simple) noexcept;
    DeathEffectBehavior* GetDeathEffectBehavior() noexcept;
    const DeathEffectBehavior* GetDeathEffectBehavior() const noexcept;

protected:
    void OnDestroy(GameObject& sender) noexcept override;

private:
    void LinkToSourceWeapon(
        const Vec3& worldPosition,
        const Quat& worldRotation) noexcept;
    void ApplySourcePreparationState(
        const ShotContext& context) noexcept;

    ProjectileDefinition description_;
    GameObject* weapon_ = nullptr;
    GameObject* target_ = nullptr;
    Vec3 shotTarget_{};
    std::size_t playerId_ = GameObject::undefinedPlayerId;
    bool prepared_ = false;
    std::uint32_t sourceTick_ = 0U;
    float sourceTimer_ = 0.0F;
    bool sourceState_ = false;
    Vec3 sourceVector_{};
    bool ignoreContactProj_ = false;
    bool externalLifetimeManaged_ = false;
    LaserVisualState laserVisualState_;
    MapObj* sourceModel_ = nullptr;
    MapObj* sourceModel2_ = nullptr;
    DeathEffectBehavior* deathEffect_ = nullptr;
};

// MapObj.cpp registers gotProj as AutoProj.  Unlike a weapon shot, this
// source object prepares itself when Logic is attached, keeps the serialized
// map transform, and releases only its prepared state when Logic goes away.
// MineUpdate owns the short arming interval used by the oil model/contact.
class AutoProj : public Proj
{
public:
    static constexpr std::uint32_t masloType = 10U;

    ~AutoProj() override;
    void Reset(std::uint32_t type) noexcept;
    void Reset(const ProjectileDefinition& description) noexcept;
    void LogicInited() noexcept override;
    void LogicReleased() noexcept override;
    void OnProgress(float deltaTime) noexcept;

    bool IsPrepared() const noexcept;
    bool IsArming() const noexcept;
    float GetModelScale() const noexcept;
    std::uint32_t GetType() const noexcept;

private:
    static bool UsesMineUpdate(std::uint32_t type) noexcept;

    ProjectileDefinition autoDescription_;
    // Negative means that the source Proj did not touch actor scale.
    float modelScale_ = -1.0F;
    bool prepared_ = false;
};

// GameBase.cpp::ShotEffect is itself the concrete btShotEffect entry: one
// object is simultaneously the GameObject listener, EventEffect owner and
// OnShot callback. The backend may create a child visual and a Source3d, but
// their identity remains attached to this source behavior per weapon actor.
class ShotEffect final : public Behavior, public EventEffect
{
public:
    struct SpawnResult
    {
        bool createEffect = false;
        bool playSound = false;
        bool child = true;
        EventEffect* owner = nullptr;
        EffectId effectId = invalidEffect;
        std::array<float, 3U> position{};
        std::array<float, 3U> impulse{};
        bool ignoreRotation = false;
    };

    explicit ShotEffect(Behaviors* owner) noexcept;

    void OnProgress(float deltaTime) noexcept override;
    void Configure(ShotEffectDefinition definition);
    void Reset() noexcept;
    void CopyStateFrom(const ShotEffect& value);
    std::optional<SpawnResult> ConsumeSpawnResult();
    std::size_t GetPendingSpawnCount() const noexcept;
    const std::array<float, 3U>& GetLastShotPosition() const noexcept;
    std::uint64_t GetShotCount() const noexcept;
    const ShotEffectDefinition& GetDefinition() const noexcept;
    std::string_view SelectSound(float randomUnit) const noexcept;

protected:
    void OnShot(
        const std::array<float, 3U>& position) noexcept override;

private:
    SpawnResult BuildSpawn(
        const std::array<float, 3U>& position) noexcept;
    ShotEffectDefinition definition_;
    std::uint64_t shotCount_ = 0U;
    std::array<float, 3U> lastShotPosition_{};
    std::vector<SpawnResult> pendingSpawns_;
};

// Backend-neutral transcription of the original Weapon timer and Desc
// ownership. Projectile preparation remains at the Jolt/bgfx session
// boundary; readiness and successful-shot lifetime belong here.
class Weapon : public GameObject
{
public:
    using ShotDesc = Proj::ShotDesc;
    using ShotContext = Proj::ShotContext;
    using ProjList = std::vector<Proj*>;

    struct Desc
    {
        float shotDelay = 0.0F;
        std::vector<ProjectileDefinition> projectiles;

        const ProjectileDefinition& Front() const noexcept;
    };

    using DescHandle = std::shared_ptr<const Desc>;

    Weapon();
    explicit Weapon(const Desc& desc);
    Weapon(const Weapon& other);
    Weapon& operator=(const Weapon& other);
    Weapon(Weapon&& other);
    Weapon& operator=(Weapon&& other);
    ~Weapon() override = default;

    void Reset() noexcept;
    void OnProgress(float deltaTime) noexcept;
    float GetShotTime() const noexcept;
    bool IsReadyShot(float delay) const noexcept;
    bool IsReadyShot() const noexcept;
    bool IsMaslo() const noexcept;
    void OnShot(bool projectileCreated = true) noexcept;
    bool Shot(
        const ShotDesc& shot,
        ProjList* projectiles = nullptr);
    bool Shot(
        Proj::Vec3 target,
        ProjList* projectiles = nullptr);
    bool Shot(
        GameObject* target,
        ProjList* projectiles = nullptr);
    bool Shot(ProjList* projectiles = nullptr);
    // Source Weapon::Shot builds one context for every descriptor from the
    // live weapon actor before CreateShot iterates the batch. Jolt supplies
    // only the mounted actor velocity and sampled FloatRange values.
    std::vector<ShotContext> BuildShotContexts(
        Logic* logic, std::size_t playerId,
        const ShotDesc& shot, Proj::Vec3 weaponVelocity,
        std::span<const float> sampledMinimumLifetimes = {}) const;
    // Weapon::CreateShot dispatches Behaviors::OnShot separately for every
    // projectile accepted by PrepareProj.  Keep this separate from the
    // WeaponItem transaction because one trigger may create several actors.
    void OnProjectilePrepared(
        const std::array<float, 3U>& position = {}) noexcept;

    const Desc& GetDesc() const noexcept;
    DescHandle GetDescHandle() const noexcept;
    void SetDesc(const Desc& value);
    void SetDescHandle(DescHandle value) noexcept;
    void SetDesc(float shotDelay,
                 std::span<const std::uint32_t> projectileTypes);
    void SetDesc(float shotDelay,
                 std::span<const ProjectileDefinition> projectiles);
    const ShotEffect& GetShotEffect() const noexcept;
    void ConfigureShotEffect(ShotEffectDefinition definition);
    const ShotEffectDefinition&
    GetShotEffectDefinition() const noexcept;
    std::optional<ShotEffect::SpawnResult>
    ConsumeShotEffectSpawn();
    std::string_view SelectShotEffectSound(
        float randomUnit) const noexcept;
    const std::array<float, 3U>& GetLastShotPosition() const noexcept;
    // DrobilkaUpdate rotates the source Weapon actor itself. The quaternion
    // is the backend-visible delta relative to its serialized mount pose;
    // renderer and Jolt consume it without keeping a second Player mirror.
    void RotateDrobilka(float angle) noexcept;
    Proj::Quat GetDrobilkaRotation() const noexcept;

    // Backend-neutral Weapon::CreateShot commit point.  Preparation values
    // which used to come from PhysX are carried by ShotContext; successful
    // objects are immediately transferred to Logic exactly as in Windows.
    static Proj* CreateShot(
        Weapon* weapon, const ProjectileDefinition& description,
        const ShotContext& context);
    static bool CreateShot(
        Weapon* weapon, const Desc& description,
        std::span<const ShotContext> contexts,
        ProjList* projectiles = nullptr);

private:
    void BindSourceBehaviors();
    std::vector<ShotContext> MakeShotContexts(
        const ShotDesc& shot);
    static bool CanCreateWithoutWeapon(
        std::uint32_t projectileType) noexcept;

    DescHandle desc_;
    float shotTime_ = 0.0F;
    Proj::Quat drobilkaRotation_{0.0F, 0.0F, 0.0F, 1.0F};
    ShotEffect* shotEffect_ = nullptr;
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
    void AttachWeapon(Weapon* weapon) noexcept;

    WeaponItem* IsWeaponItem() noexcept override;
    const WeaponItem* IsWeaponItem() const noexcept override;
    void OnCreateCar() noexcept override;
    void OnDestroyCar() noexcept override;

    // Source WeaponItem::Shot owns the complete Weapon::CreateShot batch and
    // commits one charge after at least one Proj was prepared. Per-projectile
    // Jolt transform/query values are supplied as the portable context span.
    bool Shot(
        std::span<const Weapon::ShotContext> contexts,
        int newCharge = -1,
        Weapon::ProjList* projectiles = nullptr);
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
    void SetShotEffectDefinition(ShotEffectDefinition value);
    const ShotEffectDefinition&
    GetShotEffectDefinition() const noexcept;
    Weapon* GetWeapon() const noexcept;
    Weapon::Desc GetDesc() const;
    const std::string& GetMapObjRecord() const noexcept;
    void SetMapObjRecord(std::string value);

private:
    Weapon* weapon_ = nullptr;
    bool carAttached_ = false;
    std::uint32_t maximumCharge_ = 0U;
    std::uint32_t countCharge_ = 0U;
    std::uint32_t currentCharge_ = 0U;
    std::uint32_t chargeStep_ = 1U;
    float damage_ = 0.0F;
    int chargeCost_ = 0;
    std::string mapObjRecord_;
    Weapon::DescHandle weaponDesc_ =
        std::make_shared<Weapon::Desc>();
    ShotEffectDefinition shotEffectDefinition_;
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

} // namespace r3d::game::originalrace::source
