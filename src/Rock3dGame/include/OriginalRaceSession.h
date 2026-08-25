#pragma once

#include "OriginalAICar.h"
#include "OriginalAchievmentModel.h"
#include "OriginalGameObject.h"
#include "OriginalHumanPlayer.h"
#include "OriginalLogic.h"
#include "OriginalPlayer.h"
#include "OriginalProfile.h"
#include "OriginalRace.h"
#include "OriginalRaceLifecycle.h"
#include "OriginalTrace.h"
#include "OriginalWeapon.h"

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
    HumanShot,
    EffectSound,
    ContactImpact,
    WeaponShotEffect,
    Damage,
    MapObjectDamage,
    Kill,
    Bonus,
    SpeedArrow,
    DecorationDestroyed,
    MinePlaced,
    MineContact,
    HyperActivated,
    Achievement,
    ProjectileImpact,
    VehicleDestroyed,
    VehicleEnergyDamage,
    LowLife,
    RaceFinish,
    LastLap,
    Overboard,
    DeathMine,
    MoveInverse,
    LostControl,
    LeadFinish,
    SecondFinish,
    ThirdFinish,
    LastFinish,
    LeadChanged,
    ThirdChanged,
    ThirdFar,
    LastFar,
    Domination,
    Death,
    FinishFirst,
    FinishSecond,
    FinishThird,
    FinishLast,
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
    // Digital gaMine is an edge; analog bindings (and Maslo) are evaluated
    // continuously with the source alpha-dependent readiness delay.
    bool useMine = false;
    float mineHeld = 0.0F;
    bool mineAnalogBinding = false;
    bool useHyper = false;
    bool changeWeapon = false;
    int weaponChange = 1;
    int weaponSlot = -1;
    int fireWeaponSlot = -1;
    bool reset = false;
    bool chatMode = false;
};

// Preserve the public adapter name while the active runtime object is now the
// source-derived Player class instead of session-owned anonymous state.
using RacerRuntime = source::Player;
static_assert(
    PlayerProfile::weaponSlotCount == source::Player::weaponSlotCount,
    "portable profile and source Player slot layouts must match");

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

struct AngularMomentumRequest
{
    std::size_t racer = 0;
    Vec3 momentum;
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
    DamageType damageType = DamageType::Simple;
    // Exact EventEffect/LifeEffect sound selected when its source object is
    // created. Empty for non-audio race events.
    std::string soundPath{};
    // ShotEffect owns one persistent Source3d per serialized sound on each
    // equipped slot. A valid index identifies that source owner so repeated
    // Play calls can be ignored while it is already active, and a shot made
    // outside the audible radius can start when its owner approaches later.
    std::size_t soundSource = RacerRuntime::invalidWeapon;
    // PairPxContactEffect owns one Source3d for an actor pair and destroys
    // it as soon as the pair has had no live point for 0.1 seconds. These
    // fields preserve that identity independently from ShotEffect slots.
    std::uint32_t soundContactActor =
        std::numeric_limits<std::uint32_t>::max();
    r3d::physics::CollisionSurface soundContactSurface =
        r3d::physics::CollisionSurface::TrackPlane;
    // LifeEffect's Source3d belongs to the spawned effect object. It may
    // start late while that object exists, but must stop when the object's
    // source-derived visible lifetime ends. target-child effects follow the
    // contacted racer for that lifetime.
    float soundLifetimeSeconds = -1.0F;
    std::size_t soundFollowRacer = RacerRuntime::invalidWeapon;
    float authoritativeLife = 0.0F;
    bool authoritativeDeath = false;
    // NetRace::OnDamage1/2 applies authoritative packets through the same
    // GameObject listener graph, but must not send the resulting local event
    // back over the wire a second time.
    bool networkReplicated = false;
    bool networkMapObject = false;
    std::uint8_t networkSlotMask = 0U;
    std::uint32_t networkProjectileId = 0U;
    std::vector<Vec3> networkCoordinates{};
};

struct NetworkDamageResult
{
    float life = 0.0F;
    bool death = false;
};

struct ReplicatedShot
{
    std::size_t racer = RacerRuntime::invalidWeapon;
    std::size_t target = RacerRuntime::invalidWeapon;
    std::uint8_t slotMask = 0U;
    std::uint32_t projectileId = 0U;
    std::vector<Vec3> coordinates{};
};

struct ReplicatedBonus
{
    std::size_t racer = RacerRuntime::invalidWeapon;
    std::size_t bonus = RacerRuntime::invalidWeapon;
    BonusKind kind = BonusKind::Unknown;
    float value = 0.0F;
};

struct ReplicatedMineContact
{
    std::size_t racer = RacerRuntime::invalidWeapon;
    std::size_t projectileOwner = RacerRuntime::invalidWeapon;
    std::uint32_t projectileId = 0U;
    Vec3 point;
    bool mapProjectile = false;
};

struct ReplicatedRaceResult
{
    std::size_t racer = 0U;
    std::int32_t rewardMoney = 0;
    std::int32_t pickedMoney = 0;
    std::uint32_t place = 0U;
    std::int32_t rewardPoints = 0;
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
    // DeathEffect::targetChild stores the effect in the contacted car's
    // include list.  transform is target-local while this index is valid.
    std::size_t parentRacer = RacerRuntime::invalidWeapon;
    std::size_t mountSlot = RacerRuntime::invalidWeapon;
    // PairPxContactEffect state. The source groups contacts by actor pair,
    // keeps no more than two points and stops emitting 0.1 seconds after a
    // point disappears while already emitted particles finish their lives.
    r3d::physics::CollisionSurface contactSurface =
        r3d::physics::CollisionSurface::TrackPlane;
    std::uint32_t contactActor =
        std::numeric_limits<std::uint32_t>::max();
    std::uint8_t contactIndex = 0;
    float ageSeconds = 0.0F;
    float emissionEndSeconds = -1.0F;
    // FxSystemWaitingEnd owns the two-stage source lifetime. The renderer
    // supplies the equivalent particle-end boundary through totalSeconds.
    source::GameObject effectOwner;
    source::FxSystemWaitingEnd waitingEnd;
    bool waitForParticleEnd = false;
    Vec3 detachedSourceVelocity;
    // Sounds serialized on an effect object belong to its LifeEffect, not
    // to the event which spawned it. They start from the first progress
    // callback and share the spawned object's lifetime/attachment.
    source::LifeEffect lifeEffect;
    std::vector<std::string> lifeSoundPaths;
    std::size_t lifeSoundRacer = RacerRuntime::invalidWeapon;
    std::size_t lifeSoundFollowRacer = RacerRuntime::invalidWeapon;
};

struct MineRuntime
{
    std::size_t owner = 0;
    // Proj::_playerId is cleared when its weapon object is destroyed, while
    // the owning Player still retains the bonus-projectile id for network
    // lookup. Keep those two source identities separate.
    std::size_t damageOwner = RacerRuntime::invalidWeapon;
    std::size_t weapon = 0;
    std::size_t projectile = 0;
    std::uint8_t visualVariant = 0;
    Vec3 position;
    Quat rotation;
    Vec3 velocity;
    ProjectileCollisionBox collision;
    float seconds = 0.0F;
    // Proj::_time1 is distinct from GameObject::_timeLife. MineUpdate turns
    // it from the arming elapsed time into the -1 armed sentinel.
    float armingTime = 0.0F;
    float armingAlpha = 0.0F;
    float damage = 0.0F;
    float impulseSpeed = 0.0F;
    float maximumLife = -1.0F;
    std::uint32_t type = 11U;
    std::uint32_t networkProjectileId = 0U;
    std::size_t networkPendingContact = RacerRuntime::invalidWeapon;
    // Weapon-created mines retain their source car pointer during the
    // 0.25-second MineUpdate arming window.  AutoProj fragments do not.
    bool linkedToOwner = true;
    // DeathEffect::effectPxIgnoreSenderCar is permanent for the spawned
    // effect actor, unlike the ordinary mine arming delay.
    bool ignoreOwnerCollision = false;
    bool active = true;
};

struct ProjectileRuntime
{
    std::size_t owner = 0;
    std::size_t damageOwner = RacerRuntime::invalidWeapon;
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
    float maximumLifeSeconds = 0.0F;
    float ageSeconds = 0.0F;
    float beamWidthScale = 1.0F;
    // LaserUpdate writes sampler[0].scale.x = beamLength / 10 for the
    // distorted laser only; geometry scale and UV scale are independent.
    float beamTextureScale = 1.0F;
    float reflectionCooldown = 0.0F;
    // Proj::RocketUpdate stores its current clearance above TrackPlane in
    // _vec1.z and only lowers it when the terrain rises into the projectile.
    float trackClearance = 0.0F;
    std::size_t target = RacerRuntime::invalidWeapon;
    std::uint32_t hitCount = 0;
    bool attached = false;
    bool directWeapon = false;
    bool ballistic = false;
    // Fire and Drobilka follow the weapon without SetParent.  Proj::OnDestroy
    // clears their weapon pointer but does not kill the projectile actor, so
    // it continues with its last PhysX velocity instead of following a new
    // car after respawn.
    bool detachedFromWeapon = false;
    // PhysX can report the shooter's car after a free projectile has left
    // its launch overlap (for example a reflected ptThunder).  The portable
    // vehicle box is coarser than the source shapes, so arm that contact only
    // after the projectile has separated once instead of ignoring the owner
    // for its entire lifetime.
    bool ownerCollisionArmed = false;
    // The source behavior belongs to this concrete projectile actor.  It
    // permits one distinguished death effect and carries target/pair flags.
    source::DeathEffect deathEffect;
    bool active = true;
};

class OriginalRaceSession
{
public:
    explicit OriginalRaceSession(const Race& race,
                                 bool legacyWindowsDebug = false);

    void reset();
    void applyPlayerProfile(const PlayerProfile& profile);
    void writePlayerProfile(PlayerProfile& profile) const;
    void applyAchievementProfile(const ProfileState& profile);
    void writeAchievementProfile(ProfileState& profile) const;
    void setCampaign(bool campaign) noexcept;
    void setEnableMineBug(bool enabled) noexcept;
    void setSpringBorders(bool enabled) noexcept;
    void setPaused(bool paused) noexcept;
    // AIDebug::Control toggles AICar::_enbAI for the inspected human car on
    // F7.  This is intentionally runtime-only and never changes race setup.
    void setDebugHumanAiControl(bool enabled) noexcept;
    // GameMode::GoRace is server-authoritative in a network match. Stage 0
    // holds the red semaphore while peers load, stages 1..3 advance the
    // countdown, and stage 4 releases vehicle control.
    void synchronizeNetworkCountdown(std::int32_t stage) noexcept;
    void setNetworkFinishControlled(bool controlled) noexcept;
    void startNetworkFinishTimer() noexcept;
    void synchronizeNetworkFinishResults(
        const std::vector<ReplicatedRaceResult>& results) noexcept;
    // Logic::Damage is host-authoritative in the source network game. A
    // client emits the reflected damage event without changing life; the
    // host applies it and clients later consume the returned life/death.
    void setNetworkGameplayRole(bool enabled, bool host,
                                std::vector<bool> ownedRacers);
    bool disconnectNetworkRacer(std::size_t racer) noexcept;
    // NetPlayer::OnSetGamerId/OnSetColor mutate the active Player, not the
    // immutable descriptor used to construct its backend actors.
    bool synchronizePlayerPresentation(
        std::size_t racer, int gamerId,
        const std::array<float, 4>& color) noexcept;
    NetworkDamageResult applyNetworkPlayerDamage(
        std::size_t target, std::size_t attacker, Vec3 position,
        float value, DamageType damageType,
        const r3d::physics::VehicleState& vehicle,
        bool synchronizeState = false, float targetLife = 0.0F,
        bool death = false);
    NetworkDamageResult applyNetworkMapObjectDamage(
        std::uint32_t targetObjectId, std::size_t attacker,
        float value, DamageType damageType,
        bool synchronizeState = false, float targetLife = 0.0F,
        bool death = false);
    void queueNetworkShot(ReplicatedShot shot);
    void queueNetworkBonus(ReplicatedBonus bonus);
    void queueNetworkMineContact(ReplicatedMineContact contact);
    void update(float seconds,
                const std::vector<r3d::physics::VehicleState>& vehicles,
                const RaceControl& humanControl);
    void completeRaceForExit(
        const std::vector<r3d::physics::VehicleState>& vehicles);

    RacePhase phase() const noexcept;
    float countdownSeconds() const noexcept;
    std::int32_t countdownStage() const noexcept;
    float elapsedSeconds() const noexcept;
    bool finishPresentationReady() const noexcept;
    // GameMode::Pause couples World::Pause with Logic::Mute(scEffects).
    // The audio backend consumes this state at its platform boundary.
    bool effectsMuted() const noexcept;
    const std::vector<r3d::physics::VehicleInput>& vehicleInputs() const
        noexcept;
    const std::vector<RacerRuntime>& racers() const noexcept;
    std::size_t humanRacer() const noexcept;
    const std::vector<source::RaceResult>& results() const noexcept;
    const source::RaceResult* resultForRacer(
        std::size_t racer) const noexcept;
    const source::PlayerItemRack* playerItems(
        std::size_t racer) const noexcept;
    Vec3 mapPosition(std::size_t racer) const noexcept;
    const std::vector<bool>& decorationActive() const noexcept;
    const std::vector<float>& decorationLife() const noexcept;
    const std::vector<bool>& bonusActive() const noexcept;
    const std::vector<float>& bonusScales() const noexcept;
    bool racerHasAiController(std::size_t racer) const noexcept;
    std::size_t racerForMapObjectId(
        std::uint32_t mapObjectId) const noexcept;
    std::size_t decorationForMapObjectId(
        std::uint32_t mapObjectId) const noexcept;
    std::size_t bonusForMapObjectId(
        std::uint32_t mapObjectId) const noexcept;
    const std::vector<RaceEvent>& events() const noexcept;
    const std::vector<RaceEffect>& effects() const noexcept;
    const std::vector<MineRuntime>& mines() const noexcept;
    const std::vector<ProjectileRuntime>& projectiles() const noexcept;
    std::vector<RespawnRequest> takeRespawns();
    std::vector<VelocityRequest> takeVelocityRequests();
    std::vector<AngularVelocityRequest> takeAngularVelocityRequests();
    std::vector<AngularMomentumRequest> takeAngularMomentumRequests();

private:
    bool legacyWindowsDebug_ = false;
    using TraceNodeRef = source::Trace::NodeRef;

    const std::vector<std::uint32_t>& tracePathAt(
        std::size_t path) const;
    const TracePoint& tracePoint(std::size_t pathNode) const;
    const TracePoint& tracePoint(std::size_t path,
                                 std::size_t pathNode) const;
    void buildSourceTrace();
    TraceNodeRef racerTraceNode(std::size_t racer) const noexcept;
    float tracePathLength(std::size_t path) const;
    float lapPosition(
        std::size_t racer,
        const r3d::physics::VehicleState& vehicle) const;
    float lastCorrectLapPosition(std::size_t racer) const;
    void updateProgress(std::size_t racer,
                        const r3d::physics::VehicleState& vehicle,
                        float seconds);
    std::vector<source::Player::ProgressResult> progressPlayers(
        float seconds,
        const std::vector<r3d::physics::VehicleState>& vehicles);
    r3d::physics::VehicleInput aiInput(
        std::size_t racer,
        const r3d::physics::VehicleState& vehicle,
        float seconds);
    void updateAiTracks(
        const std::vector<r3d::physics::VehicleState>& vehicles);
    void updatePlaces(
        const std::vector<r3d::physics::VehicleState>& vehicles);
    void updateGameplay(
        float seconds,
        const std::vector<r3d::physics::VehicleState>& vehicles,
        const RaceControl& humanControl);
    void queueRespawn(
        std::size_t racer,
        const std::vector<r3d::physics::VehicleState>& vehicles);
    void destroyRacer(
        std::size_t racer, Vec3 position,
        const r3d::physics::VehicleState& vehicle,
        bool gameObjectAlreadyDestroyed = false);
    void releaseRacerProjectileReferences(
        std::size_t racer) noexcept;
    void pushDamageEvent(
        std::size_t target, std::size_t attacker, const Vec3& position,
        float damage, DamageType damageType, bool networkReplicated);
    void appendPlayerGameEvents(
        std::size_t racer, const Vec3& position,
        bool networkReplicated);
    bool applyRacerDamageInternal(
        std::size_t target, std::size_t attacker, const Vec3& position,
        float sourceDamage, DamageType damageType,
        const r3d::physics::VehicleState& vehicle,
        bool incomingAlreadySupported, bool synchronizeState,
        float targetLife, bool death, bool networkReplicated);
    std::size_t findWeapon(std::string_view record,
                           WeaponSlot slot) const noexcept;
    const Vehicle& vehicleForRacer(std::size_t racer) const noexcept;
    bool damageDecorationWithBox(
        Transform transform, ProjectileCollisionBox collision,
        float damage, std::size_t attacker,
        Vec3* contactPoint = nullptr);
    bool damageDecoration(std::size_t instance, float damage,
                          std::size_t attacker);
    NetworkDamageResult applyDecorationDamageInternal(
        std::size_t instance, float damage, std::size_t attacker,
        DamageType damageType, bool synchronizeState,
        float targetLife, bool death, bool networkReplicated);
    void updateAchievements(float seconds);
    void completeRemainingRacers(
        const std::vector<r3d::physics::VehicleState>& vehicles);
    void completeRacer(const source::RaceResult& result,
                       float finishTime) noexcept;
    void applyCampaignRewards() noexcept;

    const Race& race_;
    source::Trace sourceTrace_{4U};
    source::TouchDeath groundTouchDeath_;
    source::PairPxContactEffect pairContactEffect_;
    RacePhase phase_ = RacePhase::Countdown;
    RacePhase phaseBeforePause_ = RacePhase::Countdown;
    float elapsedSeconds_ = 0.0F;
    bool networkFinishControlled_ = false;
    bool networkGameplayEnabled_ = false;
    bool networkGameplayHost_ = false;
    std::vector<bool> networkOwnedRacers_;
    std::size_t humanRacer_ = RacerRuntime::invalidWeapon;
    std::vector<RacerRuntime> racers_;
    std::vector<r3d::physics::VehicleInput> vehicleInputs_;
    std::vector<bool> decorationActive_;
    std::vector<float> decorationLife_;
    // Active source owner. The parallel arrays are renderer/physics views,
    // no longer the authority for damage or death.
    std::vector<source::DestrObj> decorationObjects_;
    std::vector<bool> bonusActive_;
    std::vector<source::GameObject> bonusObjects_;
    std::vector<source::AutoProj> bonusProjectiles_;
    std::vector<float> bonusScales_;
    std::vector<std::size_t> bonusNetworkPendingContact_;
    source::HumanPlayer humanPlayer_;
    std::vector<source::AIPlayer> aiPlayers_;
    source::AISystem aiSystem_{4U};
    // Per-frame non-owning adapter reused by source::AISystem without
    // reconstructing trace geometry in the session.
    std::vector<source::AISystem::Entry> aiSystemEntriesScratch_;
    // Per-frame adapter snapshot reused without allocation. Retained target
    // ownership lives in source::AICar::AttackState, not in this buffer.
    std::vector<source::AICar::AttackTarget> aiAttackTargetsScratch_;
    std::vector<Vec3> previousPositions_;
    std::vector<RaceEvent> events_;
    std::vector<RaceEffect> effects_;
    std::vector<MineRuntime> mines_;
    std::vector<ProjectileRuntime> projectiles_;
    std::vector<RespawnRequest> respawns_;
    std::vector<VelocityRequest> velocityRequests_;
    std::vector<AngularVelocityRequest> angularVelocityRequests_;
    std::vector<AngularMomentumRequest> angularMomentumRequests_;
    std::vector<ReplicatedShot> pendingNetworkShots_;
    std::vector<ReplicatedBonus> pendingNetworkBonuses_;
    std::vector<ReplicatedMineContact> pendingNetworkMineContacts_;
    PlayerProfile initialPlayerProfile_;
    std::uint32_t initialAchievementPoints_ = 0;
    std::map<std::string, std::uint32_t>
        initialAchievementIterations_;
    source::AchievmentModel achievementModel_;
    source::RaceRunState raceRunState_;
    source::GameModeRaceState gameModeRaceState_;
    source::RaceLifecycle raceLifecycle_;
    source::RacePlaceModel racePlaceModel_;
    bool campaign_ = true;
    bool campaignRewardsApplied_ = false;
    bool enableMineBug_ = true;
    bool springBorders_ = true;
    bool debugHumanAiControl_ = false;
};

bool runOriginalRaceSessionSmokeTest(const Race& race, std::string& error);

} // namespace r3d::game::originalrace
