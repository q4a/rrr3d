#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace r3d::game::originalrace
{

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
class Behavior;
class Behaviors;
class Logic;
class Map;
class MapObj;
class MapObjects;

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
    virtual void OnShot(
        const std::array<float, 3U>&) noexcept {}
    virtual void OnMotor(float, float, float, float) noexcept {}
    virtual void OnImmortalStatus(bool) noexcept {}

private:
    friend class Behaviors;
    Behaviors* owner_ = nullptr;
    bool removed_ = false;
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
class GameObject
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
    ProgressResult OnProgress(float deltaTime) noexcept;

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
    std::string name_;
    Vector3 position_{};
    Vector3 scale_{1.0F, 1.0F, 1.0F};
    Quaternion rotation_{0.0F, 0.0F, 0.0F, 1.0F};
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

    // Active NetPlayer::ResponseStream thresholds. A far physics position or
    // divergent graph rotation starts the matching GameObject correction.
    NetworkCorrection OnNetworkPose(
        Vector physicsPosition, Vector graphPosition,
        Quaternion graphRotation, Vector targetPosition,
        Quaternion targetRotation) noexcept;
    Pose OnFrame(Pose physicsPose, float deltaTime) noexcept;

    const Vector& GetPosSync() const noexcept;
    const Quaternion& GetRotSync() const noexcept;
    const Vector& GetPosSync2() const noexcept;
    const Quaternion& GetRotSync2() const noexcept;
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

class TouchDeath
{
public:
    bool OnContact(GameObject* target) const noexcept;
};

// GameBase.h::ResurrectObj intercepts the first Death notification, revives
// the effect GameObject and lets its backend adapter detach the actor from an
// owning include list while preserving its world transform.  A second Death
// is final.
class ResurrectObj
{
public:
    void Reset() noexcept;
    bool OnDeath(GameObject& owner) noexcept;
    bool OnDeath(GameObject& owner, Map& map);
    bool IsResurrect() const noexcept;

private:
    bool resurrect_ = false;
};

// FxSystemWaitingEnd enters particle fading on the intercepted death and
// issues the final GameObject::Death only after all already emitted particles
// have expired. Particle counting itself remains a renderer boundary.
class FxSystemWaitingEnd : public ResurrectObj
{
public:
    struct ProgressResult
    {
        bool beginFading = false;
        bool finalDeath = false;
    };

    void Reset() noexcept;
    ProgressResult OnDeath(GameObject& owner) noexcept;
    ProgressResult OnDeath(GameObject& owner, Map& map);
    ProgressResult OnProgress(
        GameObject& owner, std::size_t liveParticles) noexcept;
    bool IsFading() const noexcept;

private:
    bool fading_ = false;
};

// GameBase.cpp::FxSystemSrcSpeed copies the owning PhysX actor's linear
// velocity into every direct particle system.  When the effect GameObject is
// included below another object, the source first converts that velocity
// through the parent's complete inverse world matrix.  The renderer later
// performs FxFlowEmitter's matching local-to-world conversion.
class FxSystemSrcSpeed
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

    void Reset() noexcept;
    bool OnProgress(
        bool physicsActorAvailable, Vector actorLinearVelocity,
        const ParentTransform* parent = nullptr) noexcept;
    const Vector& GetSourceSpeed() const noexcept;

private:
    Vector sourceSpeed_;
};

// EventEffect owns one distinguished _makeEffect actor in addition to any
// transient actors created by ShotEffect. These operations preserve the
// original MakeEffect/FreeEffect/OnDestroy identity rules independently of
// the renderer object which realizes the actor.
class EventEffect
{
public:
    void Reset() noexcept;
    bool MakeEffect() noexcept;
    bool FreeEffect() noexcept;
    bool OnDestroyEffect() noexcept;
    bool IsEffectMaked() const noexcept;

private:
    bool effectMaked_ = false;
};

// Backend-neutral transcription of GameBase::DeathEffect.  The caller owns
// the transform/actor boundary; this class owns the original one-live-effect
// rule and decides the two PhysX-era relationship flags at the death event.
class DeathEffect : public EventEffect
{
public:
    struct SpawnResult
    {
        bool createEffect = false;
        bool targetChild = false;
        bool ignoreSenderCar = false;
    };

    DeathEffect() = default;
    DeathEffect(bool effectPhysicsIgnoreSenderCar,
                bool targetChild) noexcept;

    void Reset(bool effectPhysicsIgnoreSenderCar = false,
               bool targetChild = false) noexcept;
    SpawnResult OnDeath(bool logicAvailable, bool hasTarget,
                        bool senderIsWeaponProjectile) noexcept;

    bool GetEffectPxIgnoreSenderCar() const noexcept;
    void SetEffectPxIgnoreSenderCar(bool value) noexcept;
    bool GetTargetChild() const noexcept;
    void SetTargetChild(bool value) noexcept;

private:
    bool effectPhysicsIgnoreSenderCar_ = false;
    bool targetChild_ = false;
};

class LifeEffect : public EventEffect
{
public:
    void Reset() noexcept;
    // LifeEffect retries until GiveSource3d can supply a source and then
    // plays that source exactly once for the lifetime of the object.
    bool OnProgress(bool sourceAvailable) noexcept;
    bool HasPlayed() const noexcept;

private:
    bool play_ = false;
};

// Backend-neutral state owned by the original GameBase behavior classes.
// Effect actors/sounds remain renderer and audio adapters, but their state
// machines live here instead of being reconstructed in RaceSession.
class LowLifePoints
{
public:
    struct ProgressResult
    {
        bool activated = false;
        bool released = false;
    };

    LowLifePoints(float lifeLevel = 0.35F) noexcept;
    void Reset(float lifeLevel = 0.35F) noexcept;
    ProgressResult OnProgress(
        GameObject& gameObject, float deltaTime,
        Behavior* behavior = nullptr) noexcept;

    float GetLifeLevel() const noexcept;
    void SetLifeLevel(float value) noexcept;
    bool IsEffectMaked() const noexcept;
    float GetEffectSeconds() const noexcept;

private:
    float lifeLevel_ = 0.35F;
    float effectSeconds_ = 0.0F;
    EventEffect eventEffect_;
};

class DamageEffect
{
public:
    explicit DamageEffect(
        DamageType damageType = DamageType::Simple,
        float maximumTimeLife = 0.5F) noexcept;
    void Reset() noexcept;
    bool OnDamage(DamageType damageType) noexcept;
    void OnProgress(float deltaTime) noexcept;

    DamageType GetDamageType() const noexcept;
    void SetDamageType(DamageType value) noexcept;
    bool IsEffectMaked() const noexcept;
    float GetEffectSeconds() const noexcept;

private:
    DamageType damageType_ = DamageType::Simple;
    float maximumTimeLife_ = 0.5F;
    float effectSeconds_ = 0.0F;
    EventEffect eventEffect_;
};

class ImmortalEffect
{
public:
    static constexpr float fadeSeconds = 0.5F;
    static constexpr float damageSeconds = 0.25F;

    void Reset() noexcept;
    void OnImmortalStatus(bool status) noexcept;
    void OnDamage() noexcept;
    void OnProgress(float deltaTime) noexcept;

    bool IsEffectMaked() const noexcept;
    float GetEffectSeconds() const noexcept;
    float GetFadeInTime() const noexcept;
    float GetFadeOutTime() const noexcept;
    float GetDamageTime() const noexcept;
    float GetScale() const noexcept;
    float GetDamageAlpha() const noexcept;

private:
    float fadeInTime_ = -1.0F;
    float fadeOutTime_ = -1.0F;
    float damageTime_ = -1.0F;
    float effectSeconds_ = 0.0F;
    EventEffect eventEffect_;
};

class SlowEffect
{
public:
    static constexpr float maximumSpeed = 20.0F;

    struct ProgressResult
    {
        bool limitSpeed = false;
        bool released = false;
    };

    void Reset() noexcept;
    bool Attach(float maximumTimeLife, std::size_t weapon,
                std::size_t projectile) noexcept;
    ProgressResult OnProgress(
        float deltaTime, float linearSpeed) noexcept;

    bool IsEffectMaked() const noexcept;
    float GetRemainingSeconds() const noexcept;
    std::size_t GetWeapon() const noexcept;
    std::size_t GetProjectile() const noexcept;

private:
    float maximumTimeLife_ = -1.0F;
    float timeLife_ = 0.0F;
    std::size_t weapon_ = GameObject::undefinedPlayerId;
    std::size_t projectile_ = GameObject::undefinedPlayerId;
    EventEffect eventEffect_;
};

} // namespace source
} // namespace r3d::game::originalrace
