#pragma once

#include "OriginalWorld.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace r3d::game::originalrace
{

struct ObjectDefinition;

// GameObjListener::DamageType from the Windows source.
enum class DamageType : std::uint8_t
{
    Simple,
    Energy,
    Mine,
    Touch,
    DeathPlane,
};

namespace source
{

class GameObject;
class GameObjectFrameSync;
class Proj;
class GameCar;
class Behavior;
class Behaviors;
class Logic;
class Map;
class MapObj;
class MapObjects;
class Player;

// Backend-neutral GameObjListener. Reference counting belongs to the legacy
// lsl owner; portable listeners are non-owning and retain the source callback
// order and one-registration rule.
class GameObjectListener
{
public:
    virtual ~GameObjectListener() = default;
    virtual void OnDestroy(GameObject&) noexcept {}
    virtual void OnDeath(
        GameObject&, DamageType, GameObject*) noexcept {}
    virtual void OnDamage(
        GameObject&, float, DamageType) noexcept {}
    virtual void OnLowLife(GameObject&) noexcept {}
    virtual void OnLowLife(
        GameObject& sender, Behavior*) noexcept
    {
        OnLowLife(sender);
    }
    // The physics adapter resolves the other actor before dispatching the
    // source GameObject contact callback to its registered listeners.
    virtual void OnContact(
        GameObject&, GameObject*) noexcept {}
};

// GameBase.h::BehaviorType. Keep the serialized numeric order even when a
// backend-specific behavior is introduced in a later source block.
enum class BehaviorType : std::uint8_t
{
    TouchDeath = 0,
    ResurrectObj,
    FxSystemWaitingEnd,
    FxSystemSrcSpeed,
    LowLifePoints,
    DamageEffect,
    DeathEffect,
    LifeEffect,
    SlowEffect,
    PxWheelSlipEffect,
    ShotEffect,
    ImmortalEffect,
    SoundMotor,
    GusenizaAnim,
    PodushkaAnim,
};

const char* BehaviorTypeName(BehaviorType value) noexcept;

// Portable equivalent of Behavior::PxNotify. The physics backend owns the
// actual collision flags; behaviors keep the source-level subscription that
// tells the owner which callbacks must be dispatched.
enum class BehaviorPhysicsNotify : std::uint8_t
{
    Contact = 0,
    ContactModify,
    Count,
};

// Backend-neutral transcription of GameBase::Behavior. Concrete behavior
// state machines keep their renderer/physics adapters, while this base owns
// the original GameObject listener registration and deferred removal flag.
class Behavior : public GameObjectListener
{
public:
    explicit Behavior(Behaviors* owner) noexcept;
    ~Behavior() override = default;

    virtual void OnProgress(float deltaTime) noexcept = 0;

    void Remove() noexcept;
    bool IsRemoved() const noexcept;
    Behaviors* GetOwner() noexcept;
    const Behaviors* GetOwner() const noexcept;
    GameObject* GetGameObj() noexcept;
    const GameObject* GetGameObj() const noexcept;
    Logic* GetLogic() noexcept;
    const Logic* GetLogic() const noexcept;

protected:
    bool GetPhysicsNotify(BehaviorPhysicsNotify notify) const noexcept;
    void SetPhysicsNotify(
        BehaviorPhysicsNotify notify, bool value) noexcept;
    virtual void OnShot(
        const std::array<float, 3U>&) noexcept {}
    virtual void OnMotor(float, float, float, float) noexcept {}
    virtual void OnImmortalStatus(bool) noexcept {}

private:
    friend class Behaviors;
    Behaviors* owner_ = nullptr;
    bool removed_ = false;
    std::array<bool, static_cast<std::size_t>(
        BehaviorPhysicsNotify::Count)> physicsNotifies_{};
};

class Behaviors
{
public:
    struct ProgressResult
    {
        std::size_t progressed = 0U;
        std::size_t removed = 0U;
    };

    explicit Behaviors(GameObject* gameObject) noexcept;
    ~Behaviors();
    Behaviors(const Behaviors&) = delete;
    Behaviors& operator=(const Behaviors&) = delete;

    Behavior& Add(BehaviorType type, std::unique_ptr<Behavior> value);
    template<class _Behavior, class... _Args>
    _Behavior& Add(BehaviorType type, _Args&&... args)
    {
        static_assert(std::is_base_of_v<Behavior, _Behavior>);
        auto value = std::make_unique<_Behavior>(
            this, std::forward<_Args>(args)...);
        auto* result = value.get();
        Add(type, std::move(value));
        return *result;
    }

    Behavior* Find(BehaviorType type) noexcept;
    const Behavior* Find(BehaviorType type) const noexcept;
    bool Delete(Behavior* value) noexcept;
    void Clear() noexcept;
    std::size_t GetCount() const noexcept;
    bool RequiresPhysicsNotify(
        BehaviorPhysicsNotify notify) const noexcept;

    ProgressResult OnProgress(float deltaTime) noexcept;
    void OnShot(const std::array<float, 3U>& position) noexcept;
    void OnMotor(float deltaTime, float rpm,
                 float minimumRpm, float maximumRpm) noexcept;
    void OnImmortalStatus(bool status) noexcept;

    GameObject* GetGameObj() noexcept;
    const GameObject* GetGameObj() const noexcept;

private:
    struct Entry
    {
        BehaviorType type = BehaviorType::TouchDeath;
        std::unique_ptr<Behavior> value;
    };
    bool RemoveAt(std::size_t index) noexcept;

    GameObject* gameObject_ = nullptr;
    std::vector<Entry> entries_;
};

// Gameplay-owned part of GameObject. Graph/PhysX actors and listener
// dispatch remain backend boundaries, while lifetime, immortality, damage,
// healing, death and touch attribution execute through the original owner.
class GameObject : public FixedStepEvent,
                   public LateProgressEvent,
                   public FrameEvent
{
public:
    using Vector3 = std::array<float, 3U>;
    using Quaternion = std::array<float, 4U>;
    using Children = std::vector<GameObject*>;
    using IncludeList = MapObjects;

    static constexpr std::size_t undefinedPlayerId =
        std::numeric_limits<std::size_t>::max();

    enum class LiveState : std::uint8_t
    {
        Live,
        Death,
    };

    struct ProgressResult
    {
        bool immortalityEnded = false;
        bool touchAttributionEnded = false;
        bool lifetimeDeath = false;
        std::size_t includedProgressed = 0U;
        std::size_t includedRemoved = 0U;
        std::size_t behaviorsProgressed = 0U;
        std::size_t behaviorsRemoved = 0U;
    };

    struct DamageResult
    {
        float previousLife = -1.0F;
        float newLife = -1.0F;
        float damage = 0.0F;
        DamageType damageType = DamageType::Simple;
        bool wasLive = false;
        bool death = false;
        bool killCredit = false;
    };

    GameObject();
    GameObject(const GameObject& other);
    GameObject& operator=(const GameObject& other) noexcept;
    GameObject(GameObject&& other);
    GameObject& operator=(GameObject&& other) noexcept;
    virtual ~GameObject();

    // Legacy GameObject::Assign is deliberately narrower than C++ value
    // copying: MapObj concrete-type replacement keeps only Logic/store flags.
    void AssignSource(GameObject& value) noexcept;

    const std::string& GetName() const noexcept;
    void SetName(std::string value);
    const Vector3& GetPos() const noexcept;
    void SetPos(Vector3 value) noexcept;
    const Vector3& GetScale() const noexcept;
    void SetScale(Vector3 value) noexcept;
    const Quaternion& GetRot() const noexcept;
    void SetRot(Quaternion value) noexcept;
    Vector3 GetWorldPos() const noexcept;
    void SetWorldPos(Vector3 value) noexcept;
    Vector3 GetWorldScale() const noexcept;
    Quaternion GetWorldRot() const noexcept;
    void SetWorldRot(Quaternion value) noexcept;

    // State serialized by GameObject::SaveProxy/LoadProxy. Record source
    // state (graph/physics definition and maxLife) is copied separately.
    void CopyProxyStateFrom(const GameObject& value) noexcept;

    void ResetGameObject(float maximumLifeValue) noexcept;
    ProgressResult OnProgress(
        float deltaTime, bool allowLifetimeDeath = true) noexcept;

    DamageResult Damage(std::size_t senderPlayerId, float value,
                        DamageType damageType = DamageType::Simple) noexcept;
    DamageResult Damage(std::size_t senderPlayerId, float value,
                        float newLife, bool death,
                        DamageType damageType) noexcept;
    virtual bool Death(
        DamageType damageType = DamageType::Simple,
        GameObject* target = nullptr) noexcept;
    bool Resc() noexcept;
    void Healt(float value) noexcept;
    void LowLife(Behavior* behavior = nullptr) noexcept;
    void OnContact(GameObject* target) noexcept;

    bool InsertListener(GameObjectListener* value) noexcept;
    bool RemoveListener(GameObjectListener* value) noexcept;
    void ClearListenerList() noexcept;
    std::size_t GetListenerCount() const noexcept;
    bool DestroyObject() noexcept;
    bool IsObjectDestroyed() const noexcept;
    MapObj* GetMapObj() noexcept;
    const MapObj* GetMapObj() const noexcept;
    Logic* GetLogic() noexcept;
    const Logic* GetLogic() const noexcept;
    void SetLogic(Logic* value) noexcept;
    unsigned GetFrameEventCount() const noexcept;
    unsigned GetProgressEventCount() const noexcept;
    unsigned GetLateProgressEventCount() const noexcept;
    unsigned GetFixedStepEventCount() const noexcept;
    bool IsSyncFrameEvent() const noexcept;
    bool IsBodyProgressEvent() const noexcept;

    // GameObject.cpp exposes concrete source identity without RTTI.  The
    // physics adapter can therefore resolve a generic contact actor exactly
    // as the Windows code did before dispatching Proj-specific behavior.
    virtual Proj* IsProj() noexcept;
    virtual const Proj* IsProj() const noexcept;
    virtual GameCar* IsCar() noexcept;
    virtual const GameCar* IsCar() const noexcept;

    void InsertChild(GameObject* value);
    void RemoveChild(GameObject* value) noexcept;
    void ClearChildren() noexcept;
    GameObject* GetParent() noexcept;
    const GameObject* GetParent() const noexcept;
    void SetParent(GameObject* value);
    const Children& GetChildren() const noexcept;
    IncludeList& GetIncludeList() noexcept;
    const IncludeList& GetIncludeList() const noexcept;
    Behaviors& GetBehaviors() noexcept;
    const Behaviors& GetBehaviors() const noexcept;
    GameObjectFrameSync& GetFrameSync() noexcept;
    const GameObjectFrameSync& GetFrameSync() const noexcept;

    void SetImmortalFlag(bool value) noexcept;
    bool GetImmortalFlag() const noexcept;
    void Immortal(float time) noexcept;
    bool IsImmortal() const noexcept;
    bool IsTimedImmortal() const noexcept;

    LiveState GetLiveState() const noexcept;
    float GetMaxLife() const noexcept;
    void SetMaxLife(float value) noexcept;
    float GetLife() const noexcept;
    void SetLife(float value) noexcept;
    float GetTimeLife() const noexcept;
    void SetTimeLife(float value) noexcept;
    float GetMaxTimeLife() const noexcept;
    void SetMaxTimeLife(float value) noexcept;
    std::size_t GetTouchPlayerId() const noexcept;

    // Public while renderer/HUD/network adapters still consume the staged
    // source object graph directly.
    float life = -1.0F;
    float maximumLife = -1.0F;
    float timeLife = 0.0F;
    float maximumTimeLife = -1.0F;
    float shieldSeconds = 0.0F;
    std::size_t touchAttacker = undefinedPlayerId;
    float touchAttributionSeconds = 0.0F;
    bool immortalFlag = false;
    bool destroyed = false;

protected:
    void RegFrameEvent();
    void UnregFrameEvent() noexcept;
    void RegProgressEvent() noexcept;
    void UnregProgressEvent() noexcept;
    void RegLateProgressEvent();
    void UnregLateProgressEvent() noexcept;
    void RegFixedStepEvent();
    void UnregFixedStepEvent() noexcept;
    void SetSyncFrameEvent(bool value);
    void SetBodyProgressEvent(bool value);
    void OnFixedStep(float deltaTime) noexcept override;
    void OnLateProgress(
        float deltaTime, bool physicsStep) noexcept override;
    void OnFrame(
        float deltaTime, float physicsAlpha) noexcept override;

    // GameObject.cpp releases the old concrete object before propagating a
    // new Logic through children, then initializes the concrete object.
    virtual void LogicReleased() noexcept {}
    virtual void LogicInited() noexcept {}
    virtual void OnDestroyEvent() noexcept {}
    virtual void OnDeathEvent(
        DamageType, GameObject*) noexcept {}
    virtual void OnDamageEvent(float, DamageType) noexcept {}
    // GameObject.cpp sends the public cPlayerDamage event after every
    // listener has observed the assigned life, then sends cPlayerKill after
    // DoDeath and before the death-listener graph.  These backend-neutral
    // hooks retain those two distinct dispatch points.
    virtual void OnDamageDispatchEvent(
        std::size_t, float, DamageType) noexcept {}
    virtual void OnKillDispatchEvent(
        std::size_t, float, DamageType) noexcept {}
    virtual void OnLowLifeEvent() noexcept {}
    virtual void OnImmortalStatusEvent(bool) noexcept {}

private:
    friend class MapObj;
    void SetMapObj(MapObj* value) noexcept;
    void SendDeath(DamageType damageType, GameObject* target) noexcept;
    std::vector<GameObjectListener*> listeners_;
    MapObj* mapObj_ = nullptr;
    Logic* logic_ = nullptr;
    GameObject* parent_ = nullptr;
    Children children_;
    IncludeList* includeList_ = nullptr;
    Behaviors* behaviors_ = nullptr;
    // SetPosSync/SetRotSync are state of the source GameObject, not of the
    // renderer or network adapter. Indirection only permits the definition
    // to remain below GameObject in this header.
    std::unique_ptr<GameObjectFrameSync> frameSync_;
    std::string name_;
    Vector3 position_{};
    Vector3 scale_{1.0F, 1.0F, 1.0F};
    Quaternion rotation_{0.0F, 0.0F, 0.0F, 1.0F};
    unsigned frameEventCount_ = 0U;
    unsigned progressEventCount_ = 0U;
    unsigned lateProgressEventCount_ = 0U;
    unsigned fixedStepEventCount_ = 0U;
    bool syncFrameEvent_ = false;
    bool bodyProgressEvent_ = false;
    bool objectDestroyed_ = false;
};

// GameObject.cpp::SetPosSync/SetRotSync and the second correction channel
// are graph-side state: PhysX may snap to an authoritative network pose while
// the rendered actor consumes the old-to-new error at the source rates.  The
// Jolt body remains authoritative; this owner returns the exact graph pose
// which the original OnFrame would have submitted.
class GameObjectFrameSync
{
public:
    struct Vector
    {
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
    };

    struct Quaternion
    {
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
        float w = 1.0F;
    };

    struct Pose
    {
        Vector position;
        Quaternion rotation;
    };

    struct NetworkCorrection
    {
        bool snapPosition = false;
        bool snapRotation = false;
    };

    void Reset() noexcept;
    void SetPosSync(Vector value) noexcept;
    void SetRotSync(Quaternion value) noexcept;
    void SetPosSync2(Vector current, Vector next) noexcept;
    void SetRotSync2(Quaternion current, Quaternion next) noexcept;
    // GameObject::OnWake/OnSleep controls whether OnPxSync participates in
    // late/frame progress. Jolt supplies the real body active state and the
    // completed physics state; no gameplay inference is made from speed.
    void OnPhysicsState(
        Pose pose, Vector linearVelocity, bool awake) noexcept;

    // Active NetPlayer::ResponseStream thresholds. A far physics position or
    // divergent graph rotation starts the matching GameObject correction.
    NetworkCorrection OnNetworkPose(
        Vector physicsPosition, Vector graphPosition,
        Quaternion graphRotation, Vector targetPosition,
        Quaternion targetRotation) noexcept;
    Pose OnFrame(Pose physicsPose, float deltaTime,
                 float physicsAlpha = 1.0F) noexcept;

    const Vector& GetPosSync() const noexcept;
    const Quaternion& GetRotSync() const noexcept;
    const Vector& GetPosSync2() const noexcept;
    const Quaternion& GetRotSync2() const noexcept;
    const Vector& GetRenderVelocity() const noexcept;
    bool IsBodyProgressEvent() const noexcept;
    bool HasPhysicsState() const noexcept;
    bool HasActiveCorrection() const noexcept;

private:
    Vector posSync_;
    Vector posSyncDirection_;
    float posSyncLength_ = 0.0F;
    Quaternion rotSync_;
    Vector rotSyncAxis_{1.0F, 0.0F, 0.0F};
    float rotSyncAngle_ = 0.0F;

    Vector posSync2_;
    Vector posSyncDirection2_;
    float posSyncDistance2_ = 0.0F;
    float posSyncLength2_ = 0.0F;
    Quaternion rotSync2_;
    Vector rotSyncAxis2_{1.0F, 0.0F, 0.0F};
    float rotSyncAngle2_ = 0.0F;
    float rotSyncLength2_ = 0.0F;

    Pose previousPhysicsPose_;
    Pose currentPhysicsPose_;
    Pose renderPhysicsPose_;
    Vector previousPhysicsVelocity_;
    Vector currentPhysicsVelocity_;
    Vector renderPhysicsVelocity_;
    bool bodyProgressEvent_ = false;
    bool physicsStateInitialized_ = false;
};

// Gameplay-owned part of GameCar.h::DestrObj. The original queues its
// serialized destruction list from OnDeath and releases it once from the
// following progress callback.
class DestrObj : public GameObject
{
public:
    DestrObj();
    ~DestrObj() override;

    DamageResult Damage(
        std::size_t senderPlayerId, float value,
        DamageType damageType = DamageType::Simple) noexcept;
    DamageResult Damage(std::size_t senderPlayerId, float value,
                        float newLife, bool death,
                        DamageType damageType) noexcept;
    bool Death(
        DamageType damageType = DamageType::Simple,
        GameObject* target = nullptr) noexcept override;
    bool OnProgress(float deltaTime) noexcept;
    bool HasPendingDestruction() const noexcept;
    MapObjects& GetDestrList() noexcept;
    const MapObjects& GetDestrList() const noexcept;
    std::size_t ReleaseDestruction(Map& map);

private:
    MapObjects* destructionList_ = nullptr;
    bool checkDestruction_ = false;
};

class TouchDeath final : public Behavior
{
public:
    explicit TouchDeath(Behaviors* owner) noexcept;

    void OnProgress(float deltaTime) noexcept override;
    void OnContact(
        GameObject& sender, GameObject* target) noexcept override;
};

// Concrete serialized type-1 behavior. It intercepts the first Death,
// revives the object and detaches an included MapObj into Logic::Map while
// preserving its world transform. A second Death is final.
class ResurrectObj : public Behavior
{
public:
    explicit ResurrectObj(Behaviors* owner) noexcept;

    void OnProgress(float deltaTime) noexcept override;
    void OnDeath(GameObject& sender, DamageType damageType,
                 GameObject* target) noexcept override;
    bool IsResurrect() const noexcept;

protected:
    bool Resurrect(GameObject& owner) noexcept;

private:
    bool resurrect_ = false;
};

// Concrete Behavior counterpart of the source FxSystemWaitingEnd class.
// The graphics backend supplies only the current live-particle count; death
// interception, resurrection and the final Death notification remain in the
// GameObject listener/progress graph.
class FxSystemWaitingEnd final : public ResurrectObj
{
public:
    explicit FxSystemWaitingEnd(Behaviors* owner) noexcept;

    void OnProgress(float deltaTime) noexcept override;
    void OnDeath(GameObject& sender, DamageType damageType,
                 GameObject* target) noexcept override;

    void SetLiveParticleCount(std::size_t value) noexcept;
    bool IsResurrect() const noexcept;
    bool IsFading() const noexcept;
    bool ConsumeBeginFading() noexcept;
    bool ConsumeFinalDeath() noexcept;

private:
    std::size_t liveParticles_ = 0U;
    bool fading_ = false;
    bool beginFading_ = false;
    bool finalDeath_ = false;
};

// GameBase.cpp::FxSystemSrcSpeed copies the owning PhysX actor's linear
// velocity into every direct particle system.  When the effect GameObject is
// included below another object, the source first converts that velocity
// through the parent's complete inverse world matrix.  The renderer later
// performs FxFlowEmitter's matching local-to-world conversion.
class FxSystemSrcSpeed final : public Behavior
{
public:
    struct Vector
    {
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
    };

    struct Quaternion
    {
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
        float w = 1.0F;
    };

    struct ParentTransform
    {
        Vector scale{1.0F, 1.0F, 1.0F};
        Quaternion rotation;
    };

    FxSystemSrcSpeed() noexcept;
    explicit FxSystemSrcSpeed(Behaviors* owner) noexcept;

    void Reset() noexcept;
    bool OnProgress(
        bool physicsActorAvailable, Vector actorLinearVelocity,
        const ParentTransform* parent = nullptr) noexcept;
    void OnProgress(float deltaTime) noexcept override;
    void SetPhysicsInput(
        bool actorAvailable, Vector velocity,
        const ParentTransform* parent = nullptr) noexcept;
    const Vector& GetSourceSpeed() const noexcept;
    const Vector& GetWorldSourceSpeed() const noexcept;
    bool HasPhysicsActor() const noexcept;

private:
    Vector sourceSpeed_;
    Vector actorVelocity_;
    Vector worldSourceSpeed_;
    ParentTransform parent_;
    bool actorAvailable_ = false;
    bool hasParent_ = false;
};

// EventEffect owns one distinguished _makeEffect actor in addition to any
// transient actors created by ShotEffect. These operations preserve the
// original MakeEffect/FreeEffect/OnDestroy identity rules independently of
// the renderer object which realizes the actor.
class EventEffect : public Behavior
{
public:
    using EffectId = std::uint64_t;
    static constexpr EffectId invalidEffect = 0U;

private:
    struct EffectState
    {
        std::vector<EffectId> effectIds;
        EffectId makeEffectId = invalidEffect;
        EffectId nextEffectId = 1U;
    };

public:
    // Backend effects can outlive the source GameObject behavior which
    // created them. Windows removed GameObjEvent from the child during owner
    // destruction; the portable renderer uses this weak identity to preserve
    // that rule without retaining or dereferencing the destroyed behavior.
    class EffectReference
    {
    public:
        EffectReference() = default;

        bool HasOwner() const noexcept;
        bool HasEffect() const noexcept;
        bool IsEffectMaked() const noexcept;
        bool IsOwnedBy(const EventEffect& owner) const noexcept;
        bool NotifyDestroyed() noexcept;
        EffectId GetEffectId() const noexcept;
        void Reset() noexcept;

    private:
        friend class EventEffect;
        EffectReference(
            std::weak_ptr<EffectState> state, EffectId effect) noexcept;

        std::weak_ptr<EffectState> state_;
        EffectId effect_ = invalidEffect;
    };

    EventEffect() noexcept;
    explicit EventEffect(Behaviors* owner) noexcept;
    EventEffect(const EventEffect& other);
    EventEffect& operator=(const EventEffect& other);
    EventEffect(EventEffect&&) noexcept = default;
    EventEffect& operator=(EventEffect&&) noexcept = default;

    struct SpawnResult
    {
        bool createEffect = false;
        bool child = true;
        EventEffect* owner = nullptr;
        EffectId effectId = invalidEffect;
        const ObjectDefinition* definition = nullptr;
        std::array<float, 3U> position{};
        std::array<float, 3U> impulse{};
        bool ignoreRotation = false;
    };

    // The source base updates Source3d position. SDL performs that backend
    // operation from the owning GameObject; the ordered behavior callback
    // remains here to preserve the original class/listener hierarchy.
    void OnProgress(float deltaTime) noexcept override;

    void Configure(
        const ObjectDefinition* definition,
        std::array<float, 3U> position = {},
        std::array<float, 3U> impulse = {},
        bool ignoreRotation = false) noexcept;
    void ConfigureSounds(std::vector<std::string> soundPaths);
    void Reset() noexcept;
    bool MakeEffect() noexcept;
    bool FreeEffect() noexcept;
    bool OnDestroyEffect() noexcept;
    bool OnDestroyEffect(EffectId effect) noexcept;
    bool IsEffectMaked() const noexcept;
    EffectId GetMakeEffectId() const noexcept;
    std::size_t GetEffectCount() const noexcept;
    bool HasEffect(EffectId effect) const noexcept;
    EffectReference ObserveEffect(EffectId effect) noexcept;
    SpawnResult GetSpawnResult(bool created) noexcept;
    const ObjectDefinition* GetEffectDefinition() const noexcept;
    const std::array<float, 3U>& GetPosition() const noexcept;
    const std::array<float, 3U>& GetImpulse() const noexcept;
    bool GetIgnoreRotation() const noexcept;
    const std::vector<std::string>& GetSoundPaths() const noexcept;
    const std::string* SelectSoundPath(float randomUnit) const noexcept;

protected:
    EffectId CreateEffect() noexcept;

private:
    const ObjectDefinition* definition_ = nullptr;
    std::array<float, 3U> position_{};
    std::array<float, 3U> impulse_{};
    bool ignoreRotation_ = false;
    std::shared_ptr<EffectState> effectState_ =
        std::make_shared<EffectState>();
    std::vector<std::string> soundPaths_;
};

// Backend-neutral transcription of GameBase::DeathEffect.  The caller owns
// the transform/actor boundary; this class owns the original one-live-effect
// rule and decides the two PhysX-era relationship flags at the death event.
class DeathEffect final : public EventEffect
{
public:
    struct SpawnResult
    {
        bool createEffect = false;
        bool targetChild = false;
        bool ignoreSenderCar = false;
        EventEffect* owner = nullptr;
        EffectId effectId = invalidEffect;
        const ObjectDefinition* definition = nullptr;
        std::array<float, 3U> position{};
        std::array<float, 3U> impulse{};
        bool ignoreRotation = false;
    };

    DeathEffect() noexcept;
    DeathEffect(bool effectPhysicsIgnoreSenderCar,
                bool targetChild) noexcept;
    DeathEffect(Behaviors* owner,
                bool effectPhysicsIgnoreSenderCar = false,
                bool targetChild = false) noexcept;

    void OnProgress(float deltaTime) noexcept override;
    void Reset(bool effectPhysicsIgnoreSenderCar = false,
               bool targetChild = false) noexcept;
    void ConfigureSource(
        const ObjectDefinition* definition,
        std::array<float, 3U> position,
        std::array<float, 3U> impulse,
        bool ignoreRotation,
        std::vector<std::string> soundPaths);
    SpawnResult OnDeath(bool logicAvailable, bool hasTarget,
                        bool senderIsWeaponProjectile) noexcept;

    bool GetEffectPxIgnoreSenderCar() const noexcept;
    void SetEffectPxIgnoreSenderCar(bool value) noexcept;
    bool GetTargetChild() const noexcept;
    void SetTargetChild(bool value) noexcept;
    void SetSpawnContext(bool logicAvailable,
                         bool senderIsWeaponProjectile) noexcept;
    SpawnResult ConsumeSpawnResult() noexcept;
    bool HasLiveEffects() const noexcept;

protected:
    void OnDeath(GameObject& sender, DamageType damageType,
                 GameObject* target) noexcept override;

private:
    bool effectPhysicsIgnoreSenderCar_ = false;
    bool targetChild_ = false;
    SpawnResult pending_;
    bool logicAvailable_ = false;
    bool senderIsWeaponProjectile_ = false;
};

class LifeEffect final : public EventEffect
{
public:
    LifeEffect() noexcept;
    explicit LifeEffect(Behaviors* owner) noexcept;

    void Reset() noexcept;
    // LifeEffect retries until GiveSource3d can supply a source and then
    // plays that source exactly once for the lifetime of the object.
    bool OnProgress(bool sourceAvailable) noexcept;
    void OnProgress(float deltaTime) noexcept override;
    void ConfigureSounds(std::vector<std::string> soundPaths);
    void SetSourceAvailable(bool value) noexcept;
    void SetSoundSelectionUnit(float value) noexcept;
    bool HasPlayed() const noexcept;
    const std::string* ConsumePlayRequest() noexcept;

private:
    bool play_ = false;
    bool sourceAvailable_ = false;
    float soundSelectionUnit_ = 0.0F;
    const std::string* playRequest_ = nullptr;
};

// Direct transcriptions of the four remaining EventEffect-derived Windows
// behaviors. Effect actors/sounds stay behind portable backend adapters, but
// the registered behavior and the effect state now have one source identity.
class LowLifePoints final : public EventEffect
{
public:
    struct ProgressResult
    {
        bool activated = false;
        bool released = false;
        EventEffect::SpawnResult spawn;
    };

    explicit LowLifePoints(float lifeLevel = 0.35F) noexcept;
    explicit LowLifePoints(
        Behaviors* owner, float lifeLevel = 0.35F) noexcept;
    void OnProgress(float deltaTime) noexcept override;
    void Configure(
        const ObjectDefinition* definition,
        std::array<float, 3U> position,
        float lifeLevel = 0.35F) noexcept;
    void Reset(float lifeLevel = 0.35F) noexcept;
    ProgressResult OnProgress(
        GameObject& gameObject, float deltaTime,
        Behavior* behavior = nullptr) noexcept;
    ProgressResult ConsumeProgressResult() noexcept;

    float GetLifeLevel() const noexcept;
    void SetLifeLevel(float value) noexcept;
    bool IsEffectMaked() const noexcept;
    float GetEffectSeconds() const noexcept;
    const ObjectDefinition* GetEffectDefinition() const noexcept;
    const std::array<float, 3U>& GetEffectPosition() const noexcept;

private:
    float lifeLevel_ = 0.35F;
    float effectSeconds_ = 0.0F;
    ProgressResult pendingProgress_;
};

class DamageEffect final : public EventEffect
{
public:
    explicit DamageEffect(
        DamageType damageType = DamageType::Simple,
        float maximumTimeLife = 0.5F) noexcept;
    DamageEffect(
        Behaviors* owner,
        DamageType damageType = DamageType::Simple,
        float maximumTimeLife = 0.5F) noexcept;
    void Configure(
        const ObjectDefinition* definition,
        DamageType damageType,
        float maximumTimeLife) noexcept;
    void ConfigureSounds(std::vector<std::string> soundPaths);
    void Reset() noexcept;
    bool OnDamage(DamageType damageType) noexcept;
    void OnProgress(float deltaTime) noexcept override;

    DamageType GetDamageType() const noexcept;
    void SetDamageType(DamageType value) noexcept;
    bool IsEffectMaked() const noexcept;
    float GetEffectSeconds() const noexcept;
    EventEffect::SpawnResult GetSpawnResult(bool created) noexcept;
    const ObjectDefinition* GetEffectDefinition() const noexcept;
    const std::vector<std::string>& GetSoundPaths() const noexcept;
    bool HasPlayRequest() const noexcept;
    const std::string* ConsumePlayRequest(float randomUnit) noexcept;
    std::optional<EventEffect::SpawnResult>
        ConsumeSpawnResult() noexcept;

protected:
    void OnDamage(
        GameObject& sender, float value,
        DamageType damageType) noexcept override;

private:
    DamageType damageType_ = DamageType::Simple;
    float maximumTimeLife_ = 0.5F;
    float effectSeconds_ = 0.0F;
    bool playRequest_ = false;
    std::optional<EventEffect::SpawnResult> pendingSpawn_;
};

class ImmortalEffect final : public EventEffect
{
public:
    static constexpr float fadeSeconds = 0.5F;
    static constexpr float damageSeconds = 0.25F;

    ImmortalEffect() noexcept;
    explicit ImmortalEffect(Behaviors* owner) noexcept;

    void Configure(
        const ObjectDefinition* definition,
        std::array<float, 3U> scaleK) noexcept;
    void ConfigureSounds(std::vector<std::string> soundPaths);
    void Reset() noexcept;
    void OnImmortalStatus(bool status) noexcept override;
    void OnDamage() noexcept;
    void OnProgress(float deltaTime) noexcept override;

    bool IsEffectMaked() const noexcept;
    float GetEffectSeconds() const noexcept;
    float GetFadeInTime() const noexcept;
    float GetFadeOutTime() const noexcept;
    float GetDamageTime() const noexcept;
    float GetScale() const noexcept;
    float GetDamageAlpha() const noexcept;
    const ObjectDefinition* GetEffectDefinition() const noexcept;
    const std::array<float, 3U>& GetScaleK() const noexcept;
    const std::vector<std::string>& GetSoundPaths() const noexcept;
    bool HasPlayRequest() const noexcept;
    const std::string* ConsumePlayRequest(float randomUnit) noexcept;

protected:
    void OnDamage(
        GameObject& sender, float value,
        DamageType damageType) noexcept override;

private:
    float fadeInTime_ = -1.0F;
    float fadeOutTime_ = -1.0F;
    float damageTime_ = -1.0F;
    float effectSeconds_ = 0.0F;
    std::array<float, 3U> scaleK_{1.0F, 1.0F, 1.0F};
    bool playRequest_ = false;
};

class SlowEffect final : public EventEffect
{
public:
    static constexpr float maximumSpeed = 20.0F;

    struct ProgressResult
    {
        bool limitSpeed = false;
        bool released = false;
    };

    SlowEffect() noexcept;
    explicit SlowEffect(Behaviors* owner) noexcept;
    void OnProgress(float deltaTime) noexcept override;
    void Reset() noexcept;
    bool Attach(const ObjectDefinition* effectDefinition,
                float maximumTimeLife, std::size_t weapon,
                std::size_t projectile) noexcept;
    bool Attach(float maximumTimeLife, std::size_t weapon,
                std::size_t projectile) noexcept;
    ProgressResult OnProgress(
        float deltaTime, float linearSpeed) noexcept;
    void SetLinearSpeed(float value) noexcept;
    ProgressResult ConsumeProgressResult() noexcept;

    bool IsEffectMaked() const noexcept;
    float GetRemainingSeconds() const noexcept;
    std::size_t GetWeapon() const noexcept;
    std::size_t GetProjectile() const noexcept;
    const ObjectDefinition* GetEffectDefinition() const noexcept;
    EventEffect::SpawnResult GetSpawnResult(bool created) noexcept;

private:
    float maximumTimeLife_ = -1.0F;
    float timeLife_ = 0.0F;
    std::size_t weapon_ = GameObject::undefinedPlayerId;
    std::size_t projectile_ = GameObject::undefinedPlayerId;
    float linearSpeed_ = 0.0F;
    ProgressResult pendingProgress_;
};

} // namespace source
} // namespace r3d::game::originalrace
