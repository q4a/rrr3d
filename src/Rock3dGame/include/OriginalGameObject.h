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
    bool OnProgress(float deltaTime) noexcept;
    bool HasPendingDestruction() const noexcept;

private:
    bool checkDestruction_ = false;
};

} // namespace source
} // namespace r3d::game::originalrace
