#pragma once

#include "OriginalAICar.h"
#include "OriginalAchievmentModel.h"
#include "OriginalDataBase.h"
#include "OriginalGameObject.h"
#include "OriginalHumanPlayer.h"
#include "OriginalLogic.h"
#include "OriginalMap.h"
#include "OriginalPlayer.h"
#include "OriginalProfile.h"
#include "OriginalRace.h"
#include "OriginalRaceLifecycle.h"
#include "OriginalTrace.h"
#include "OriginalWeapon.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <map>
#include <optional>
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
    VehicleLowLife,
    VehicleSlowEffect,
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

// Persistent EventEffect Source3d identity. These sources belong to the
// behavior attached to a car, not to the transient visual actor spawned by
// that behavior.
enum class EventEffectSoundOwner : std::uint8_t
{
    None,
    DamageEffect,
    ImmortalEffect,
};

struct RaceControl
{
    r3d::physics::VehicleInput driving;
    // Source ControlManager messages in their original delivery order.
    // Legacy one-shot fields below remain for backend-neutral regressions;
    // the active SDL path uses this message list.
    std::vector<originalcontrol::InputMessage> inputMessages;
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
    EventEffectSoundOwner soundEventOwner =
        EventEffectSoundOwner::None;
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
    // Local source transaction requested a NetPlayer RPC but deliberately
    // did not yet mutate gameplay state. UI/achievement consumers ignore
    // this transport event and observe the replicated application instead.
    bool networkRequest = false;
    // The Windows packet's uint object field carries either a spawned
    // projectile ID or a live MapObj ID. This flag preserves that distinction.
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
    RaceEffect() = default;
    RaceEffect(const RaceEffect&) = delete;
    RaceEffect& operator=(const RaceEffect&) = delete;
    RaceEffect(RaceEffect&&) noexcept = default;
    RaceEffect& operator=(RaceEffect&&) noexcept = default;

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
    // EventEffect applies this local impulse to a spawned physics actor.
    // Current shipped ShotEffect records use zero, but source ownership is
    // retained for the Jolt adapter instead of dropping the serialized field.
    Vec3 sourceImpulse;
    // Exact MapObj record selected by the owning EventEffect behavior. This
    // is a non-owning source record reference; Race owns the catalog for the
    // complete session just as MapObjRec::Object did on Windows.
    const ObjectDefinition* sourceDefinition = nullptr;
    // Resurrected Proj include models outlive their concrete Proj owner.
    // Windows MapObjRec is reference-counted; retain the equivalent parsed
    // record when the portable source owner is about to be released.
    std::shared_ptr<const ObjectDefinition> sourceDefinitionOwner;
    // EventEffect::GameObjEvent::OnDestroy callback target. Transient Logic
    // owners are retained while this identity is live; actors rebuilt at a
    // car respawn detach the callback before replacing their behavior graph.
    source::EventEffect* sourceEventOwner = nullptr;
    source::EventEffect::EffectId sourceEventId =
        source::EventEffect::invalidEffect;
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
    // Stable LogicEventEffect identity. Multiple generations may share the
    // same actor pair/slot while an older particle object is still fading.
    source::LogicEventEffect::EffectId logicEffectId =
        source::LogicEventEffect::invalidEffect;
    float ageSeconds = 0.0F;
    float emissionEndSeconds = -1.0F;
    // A source effect is a real GameObject with concrete serialized
    // behaviors. Keeping it behind stable storage preserves listener and
    // Behavior owner identity while RaceEffect entries move in the vector.
    std::unique_ptr<source::GameObject> effectOwner =
        std::make_unique<source::GameObject>();
    source::FxSystemWaitingEndBehavior* waitingEnd = nullptr;
    source::FxSystemSrcSpeedBehavior* sourceSpeed = nullptr;
    bool waitForParticleEnd = false;
    Vec3 sourceVelocity;
    // Sounds serialized on an effect object belong to its LifeEffect, not
    // to the event which spawned it. They start from the first progress
    // callback and share the spawned object's lifetime/attachment.
    source::LifeEffectBehavior* lifeEffect = nullptr;
    std::size_t lifeSoundRacer = RacerRuntime::invalidWeapon;
    std::size_t lifeSoundFollowRacer = RacerRuntime::invalidWeapon;
};

struct MineRuntime
{
    std::size_t owner = 0;
    std::size_t weapon = 0;
    std::size_t projectile = 0;
    // Mine, MineRip core and every detached fragment are distinct source
    // Proj GameObjects.  In particular they must not share DeathEffect's
    // one-shot listener state when the parent runtime is copied to create
    // the autonomous model2/model3 objects.
    source::Proj* sourceObject = nullptr;
    std::uint8_t visualVariant = 0;
    Vec3 position;
    Quat rotation;
    Vec3 velocity;
    float armingAlpha = 0.0F;
    std::uint32_t networkProjectileId = 0U;
    std::size_t networkPendingContact = RacerRuntime::invalidWeapon;
    // Weapon-created mines retain their source car pointer during the
    // 0.25-second MineUpdate arming window.  AutoProj fragments do not.
    bool linkedToOwner = true;
    // DeathEffect::effectPxIgnoreSenderCar is permanent for the spawned
    // effect actor, unlike the ordinary mine arming delay.
    bool ignoreOwnerCollision = false;
    std::uint64_t physicsBodyId =
        r3d::physics::invalidProjectileBodyId;
    Vec3 physicsPreviousPosition;
    std::vector<r3d::physics::BodyContact> physicsContacts;
    bool physicsBacked = false;
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
    float impactDistance = 0.0F;
    float distance = 0.0F;
    float beamWidthScale = 1.0F;
    // LaserUpdate writes sampler[0].scale.x = beamLength / 10 for the
    // distorted laser only; geometry scale and UV scale are independent.
    float beamTextureScale = 1.0F;
    std::size_t target = RacerRuntime::invalidWeapon;
    bool attached = false;
    bool directWeapon = false;
    bool ballistic = false;
    // Headless/session fallback invokes Race::OnFixedStep inside the frame
    // update after World::Progress has already run. A projectile created by
    // that callback must not receive its first Proj progress pass until the
    // next frame, matching World fixed-step -> PhysX Compute ordering.
    bool deferProgressOnce = false;
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
    // Source Logic owns every successfully prepared Proj. Runtime keeps the
    // same non-owning pointer that the original optional ProjList exposed.
    source::Proj* sourceObject = nullptr;
    std::uint64_t physicsBodyId =
        r3d::physics::invalidProjectileBodyId;
    Vec3 physicsPreviousPosition;
    std::vector<r3d::physics::BodyContact> physicsContacts;
    bool physicsBacked = false;
    bool active = true;
};

class OriginalRaceSession
{
public:
    explicit OriginalRaceSession(const Race& race,
                                 bool legacyWindowsDebug = false,
                                 source::WorldEventPump* world = nullptr);
    ~OriginalRaceSession();

    source::WorldEventPump& sourceWorld() noexcept;
    const source::WorldEventPump& sourceWorld() const noexcept;

    void reset();
    void applyPlayerProfile(const PlayerProfile& profile);
    void writePlayerProfile(PlayerProfile& profile) const;
    void applyAchievementProfile(const ProfileState& profile);
    void writeAchievementProfile(ProfileState& profile) const;
    std::optional<source::AchievmentItemView> achievementItem(
        std::string_view name) const noexcept;
    std::uint32_t achievementPoints() const noexcept;
    bool purchaseAchievement(std::string_view name);
    bool checkAchievement(std::string_view name) const noexcept;
    bool checkAchievementMapObject(
        std::string_view record) const noexcept;
    bool checkAchievementGamer(int gamerId) const noexcept;
    void setCampaign(bool campaign) noexcept;
    void setEnableMineBug(bool enabled) noexcept;
    void setSpringBorders(bool enabled) noexcept;
    void setPaused(bool paused) noexcept;
    void setSoundVolume(
        source::Logic::SoundCategory category, float value) noexcept;
    float soundVolume(
        source::Logic::SoundCategory category) const noexcept;
    float storedSoundVolume(
        source::Logic::SoundCategory category) const noexcept;
    void muteSound(
        source::Logic::SoundCategory category, bool value) noexcept;
    bool soundMuted(
        source::Logic::SoundCategory category) const noexcept;
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
    // Native physics invokes the source Race::OnFixedStep owner once before
    // every Jolt solver substep.  This is deliberately separate from the
    // per-vehicle GameCar drive callback below.
    void setExternalRaceFixedStep(bool enabled) noexcept;
    void setExternalProjectilePhysics(bool enabled);
    void setWorldRaycast(r3d::physics::WorldRayCast raycast);
    void raceFixedStep(
        float deltaTime,
        const std::vector<r3d::physics::VehicleState>& vehicles,
        std::vector<r3d::physics::VehicleInput>& inputs,
        std::vector<r3d::physics::VehicleResetCommand>& resets);
    // Native Jolt calls this after its completed solver step. Headless/source
    // tests retain the synchronous fallback in update().
    void setExternalRaceLateProgress(bool enabled) noexcept;
    void lateProgress(
        float seconds,
        const std::vector<r3d::physics::VehicleState>& vehicles);
    void completeRaceForExit(
        const std::vector<r3d::physics::VehicleState>& vehicles);

    RacePhase phase() const noexcept;
    float countdownSeconds() const noexcept;
    std::int32_t countdownStage() const noexcept;
    float elapsedSeconds() const noexcept;
    bool finishPresentationReady() const noexcept;
    // GameMode::Pause couples World::Pause with Logic::Mute(scEffects).
    // The audio backend consumes Logic's resulting category volume at its
    // platform boundary.
    bool effectsMuted() const noexcept;
    const std::vector<r3d::physics::VehicleInput>& vehicleInputs() const
        noexcept;
    const std::vector<RacerRuntime>& racers() const noexcept;
    // The native runtime keeps the Windows World::cMaxSimStep clock at 1/60
    // while Jolt integrates two backend substeps at 1/120. Session-only
    // source regressions have no physics world and use one equivalent
    // fixed-step advance from progressPlayers instead.
    void setExternalVehicleFixedStep(bool enabled) noexcept;
    r3d::physics::VehicleDriveCommand racerFixedStepDrive(
        std::size_t racer, float deltaTime,
        const r3d::physics::VehicleInput& input,
        const r3d::physics::VehicleFixedStepState& state) noexcept;
    source::GameObjectFrameSync::NetworkCorrection
    synchronizeRacerNetworkPose(
        std::size_t racer,
        source::GameObjectFrameSync::Vector physicsPosition,
        source::GameObjectFrameSync::Vector graphPosition,
        source::GameObjectFrameSync::Quaternion graphRotation,
        source::GameObjectFrameSync::Vector targetPosition,
        source::GameObjectFrameSync::Quaternion targetRotation) noexcept;
    void synchronizeRacerPhysicsState(
        std::size_t racer,
        source::GameObjectFrameSync::Pose pose,
        source::GameObjectFrameSync::Vector linearVelocity,
        bool awake) noexcept;
    r3d::physics::VehicleState racerFrameState(
        std::size_t racer,
        const r3d::physics::VehicleState& physicsState,
        float deltaTime, float physicsAlpha = 1.0F) noexcept;
    source::SoundMotorMix racerMotorMix(
        std::size_t racer) const noexcept;
    std::size_t humanRacer() const noexcept;
    // Race::GetTotalPoints and Planet::GetRequestPoints iterate the active
    // PlayerList and include both the local Human and network Opponents.
    // Disconnected portable storage is excluded because NetPlayer's source
    // destructor has already removed that Player from the list.
    std::uint32_t humanOrOpponentCount() const noexcept;
    std::uint32_t totalHumanOrOpponentPoints() const noexcept;
    // Race::CompleteRace clears every remaining Player's accumulated pass
    // points whenever Tournament::CompleteTrack reaches the end of a pass,
    // whether that pass succeeds or fails.
    void resetTournamentPassPoints() noexcept;
    const std::vector<source::RaceResult>& results() const noexcept;
    const source::RaceResult* resultForRacer(
        std::size_t racer) const noexcept;
    Vec3 mapPosition(std::size_t racer) const noexcept;
    const std::vector<bool>& decorationActive() const noexcept;
    const std::vector<float>& decorationLife() const noexcept;
    const std::vector<bool>& bonusActive() const noexcept;
    const std::vector<float>& bonusScales() const noexcept;
    const source::Map& sourceMap() const noexcept;
    bool racerHasAiController(std::size_t racer) const noexcept;
    std::uint32_t racerMapObjectId(std::size_t racer) const noexcept;
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
    std::vector<r3d::physics::ProjectileBodyCommand>
        takeProjectileBodyCommands();
    void synchronizeProjectilePhysics(
        const std::vector<r3d::physics::ProjectileBodyState>& states);

private:
    bool legacyWindowsDebug_ = false;
    using TraceNodeRef = source::Trace::NodeRef;
    struct PendingAiAttack
    {
        std::size_t racer = RacerRuntime::invalidWeapon;
        source::AICar::AttackDecision decision;
        std::size_t weapon = RacerRuntime::invalidWeapon;
        std::size_t weaponSlot = RacerRuntime::invalidWeapon;
        std::uint32_t projectileId = 0U;
        Transform weaponTransform;
        source::Weapon::ProjList sourceProjectiles;
        std::size_t sourceRuntimeCount = 0U;
        bool sourcePrepared = false;
        std::size_t hyperWeapon = RacerRuntime::invalidWeapon;
        std::uint32_t hyperProjectileId = 0U;
        Transform hyperWeaponTransform;
        Transform hyperProjectileTransform;
        Vec3 hyperPosition;
        Vec3 hyperVelocityDelta;
        float hyperDuration = 0.0F;
        source::Weapon::ProjList hyperSourceProjectiles;
        bool hyperSpringLocked = false;
        bool hyperVelocityQueued = false;
        bool hyperRuntimeMaterialized = false;
        bool hyperSourcePrepared = false;
        std::size_t mineWeapon = RacerRuntime::invalidWeapon;
        std::uint32_t mineProjectileId = 0U;
        Transform mineWeaponTransform;
        Transform mineTransform;
        source::Weapon::ProjList mineSourceProjectiles;
        bool mineRuntimeMaterialized = false;
        bool mineSourcePrepared = false;
    };
    source::MapObjects& decorationObjects() noexcept;
    const source::MapObjects& decorationObjects() const noexcept;
    source::MapObjects& bonusObjects() noexcept;
    const source::MapObjects& bonusObjects() const noexcept;
    bool decorationIsActive(std::size_t index) const noexcept;
    float decorationLifeValue(std::size_t index) const noexcept;
    void refreshDecorationView() const;
    bool bonusIsActive(std::size_t index) const noexcept;
    float bonusScaleValue(std::size_t index) const noexcept;
    void refreshBonusView() const;
    bool projectileIsActive(
        const ProjectileRuntime& projectile) const noexcept;
    bool mineIsActive(const MineRuntime& mine) const noexcept;
    void refreshProjectileView() const noexcept;
    void refreshMineView() const noexcept;

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
        const source::AICar::Command& command) const;
    void updateAiTracks(
        const std::vector<r3d::physics::VehicleState>& vehicles);
    void progressAi(
        float seconds,
        const std::vector<r3d::physics::VehicleState>& vehicles);
    Transform sourceWeaponWorldTransform(
        const std::vector<r3d::physics::VehicleState>& vehicles,
        std::size_t owner, std::size_t weapon,
        std::optional<std::size_t> primaryMount) const;
    ProjectileRuntime buildWeaponProjectileRuntime(
        const std::vector<r3d::physics::VehicleState>& vehicles,
        std::size_t owner, std::size_t weapon,
        std::size_t primaryMount, std::size_t preparedOrdinal,
        source::Proj& projectile,
        std::size_t homingTarget);
    void queueProjectileBodyCreate(ProjectileRuntime& projectile);
    void queueProjectileBodySynchronize(
        const ProjectileRuntime& projectile);
    void queueProjectileBodyDestroy(
        const ProjectileRuntime& projectile);
    void queueMineBodyCreate(MineRuntime& mine);
    void queueMineBodySynchronize(const MineRuntime& mine);
    void queueMineBodyDestroy(const MineRuntime& mine);
    void queueBonusBodyCreate(std::size_t bonus);
    void queueBonusBodyDestroy(std::size_t bonus);
    r3d::physics::WorldRayCastHit queryWorldRay(
        const std::vector<r3d::physics::VehicleState>& vehicles,
        Vec3 origin, Vec3 direction, float maximumDistance,
        std::size_t ignoredVehicle, bool trackPlaneOnly) const;
    source::ResetCarRayKind queryResetWorld(
        const std::vector<r3d::physics::VehicleState>& vehicles,
        std::size_t ownVehicle, Vec3 origin) const;
    bool prepareAiWeaponAttack(
        PendingAiAttack& attack,
        const std::vector<r3d::physics::VehicleState>& vehicles);
    bool prepareAiHyperAttack(
        PendingAiAttack& attack,
        const std::vector<r3d::physics::VehicleState>& vehicles);
    bool prepareAiMineAttack(
        PendingAiAttack& attack,
        const std::vector<r3d::physics::VehicleState>& vehicles);
    void discardPendingAiAttack(PendingAiAttack& attack) noexcept;
    void progressRaceFixedStep(
        float seconds,
        const std::vector<r3d::physics::VehicleState>& vehicles,
        std::vector<r3d::physics::VehicleInput>& inputs,
        std::vector<r3d::physics::VehicleResetCommand>* resets,
        bool deferEvents);
    void updatePlaces(
        float seconds,
        const std::vector<r3d::physics::VehicleState>& vehicles);
    void synchronizeRacerGameCars(
        const std::vector<r3d::physics::VehicleState>& vehicles);
    void ingestPairContacts(
        const std::vector<r3d::physics::VehicleState>& vehicles);
    void releasePairContacts();
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
    bool findDecorationWithBox(
        Transform transform, ProjectileCollisionBox collision,
        std::size_t& hit, Vec3* contactPoint = nullptr) const;
    bool damageDecorationWithBox(
        Transform transform, ProjectileCollisionBox collision,
        float damage, std::size_t attacker,
        source::Proj* projectile,
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
    void notifyEffectDestroyed(RaceEffect& effect) noexcept;
    void clearEffects() noexcept;

    const Race& race_;
    // Declared before Logic so Logic can unregister its ProgressEvent while
    // the World owner is still alive during destruction.
    source::WorldEventPump ownedGameplayWorld_;
    source::WorldEventPump* gameplayWorld_ = nullptr;
    source::Logic logic_;
    // Source DataBase owns the seven RecordLib trees; Map owns live global
    // objects, Trace and the permanent death plane. They outlive Player so
    // Player::~Player can execute the original FreeCar/DelMapObj path.
    source::DataBase dataBase_;
    source::Map map_;
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
    std::vector<RaceEvent> deferredFixedStepEvents_;
    // Renderer compatibility views rebuilt from live MapObj/GameObject on
    // demand. They never participate in gameplay decisions.
    mutable std::vector<bool> decorationActive_;
    mutable std::vector<float> decorationLife_;
    // Renderer compatibility views rebuilt from live MapObj/AutoProj on
    // demand. Source GameObject state owns pickup/hazard lifetime; Jolt only
    // follows it with a sensor body.
    mutable std::vector<bool> bonusActive_;
    mutable std::vector<float> bonusScales_;
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
    // One source AICar::UpdateAI result per racer.  The session executes the
    // returned Jolt and weapon commands but never reorders Path/Attack/Control.
    std::vector<source::AICar::ProgressResult> aiProgressScratch_;
    std::vector<bool> aiProgressValidScratch_;
    // A render frame can contain more than one source 60 Hz fixed step.
    // Preserve every resulting AttackState command in source call order;
    // aiProgressScratch_ is only the latest drive-command cache.
    std::vector<PendingAiAttack> pendingAiAttacks_;
    std::vector<Vec3> previousPositions_;
    std::vector<RaceEvent> events_;
    std::vector<RaceEffect> effects_;
    // Backend pose/contact views. The source Proj registered in Logic owns
    // lifetime; active is refreshed for renderer/test compatibility.
    mutable std::vector<MineRuntime> mines_;
    mutable std::vector<ProjectileRuntime> projectiles_;
    std::vector<RespawnRequest> respawns_;
    std::vector<VelocityRequest> velocityRequests_;
    std::vector<AngularVelocityRequest> angularVelocityRequests_;
    std::vector<AngularMomentumRequest> angularMomentumRequests_;
    std::vector<r3d::physics::ProjectileBodyCommand>
        projectileBodyCommands_;
    std::uint64_t nextProjectileBodyId_ = 1U;
    std::vector<std::uint64_t> bonusPhysicsBodyIds_;
    std::vector<std::vector<r3d::physics::BodyContact>>
        bonusPhysicsContacts_;
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
    bool externalRaceFixedStep_ = false;
    bool externalProjectilePhysics_ = false;
    r3d::physics::WorldRayCast worldRaycast_;
    bool externalVehicleFixedStep_ = false;
    bool externalRaceLateProgress_ = false;
    bool raceLateProgressPending_ = false;
};

bool runOriginalRaceSessionSmokeTest(const Race& race, std::string& error);

} // namespace r3d::game::originalrace
