#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

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

// Gameplay-owned part of GameObject. Graph/PhysX actors and listener
// dispatch remain backend boundaries, while lifetime, immortality, damage,
// healing, death and touch attribution execute through the original owner.
class GameObject
{
public:
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

    GameObject() = default;

    void ResetGameObject(float maximumLifeValue) noexcept;
    ProgressResult OnProgress(float deltaTime) noexcept;

    DamageResult Damage(std::size_t senderPlayerId, float value,
                        DamageType damageType = DamageType::Simple) noexcept;
    DamageResult Damage(std::size_t senderPlayerId, float value,
                        float newLife, bool death,
                        DamageType damageType) noexcept;
    bool Death(DamageType damageType = DamageType::Simple) noexcept;
    bool Resc() noexcept;
    void Healt(float value) noexcept;

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
};

// Gameplay-owned part of GameCar.h::DestrObj. The original queues its
// serialized destruction list from OnDeath and releases it once from the
// following progress callback.
class DestrObj : public GameObject
{
public:
    DamageResult Damage(
        std::size_t senderPlayerId, float value,
        DamageType damageType = DamageType::Simple) noexcept;
    DamageResult Damage(std::size_t senderPlayerId, float value,
                        float newLife, bool death,
                        DamageType damageType) noexcept;
    bool Death(
        DamageType damageType = DamageType::Simple) noexcept;
    bool OnProgress(float deltaTime) noexcept;
    bool HasPendingDestruction() const noexcept;

private:
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
        const GameObject& gameObject, float deltaTime) noexcept;

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
