#pragma once

#include "OriginalGameObject.h"
#include "OriginalPlayer.h"
#include "OriginalWeapon.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <vector>

namespace r3d::game::originalrace::source
{

// Portable owner for Logic.cpp::PairPxContactEffect.  Jolt supplies actor
// identities, friction magnitude and manifold points; this class preserves
// the original two-effects-per-pair cursor and strict 0.1-second release.
class PairPxContactEffect
{
public:
    using ActorId = std::uint64_t;
    static constexpr float minimumFrictionForce = 10000.0F;
    static constexpr float contactReleaseSeconds = 0.1F;
    static constexpr std::size_t maximumPoints = 2U;
    static constexpr std::size_t invalidSound =
        std::numeric_limits<std::size_t>::max();

    struct Key
    {
        ActorId actor1 = 0U;
        ActorId actor2 = 0U;

        bool operator<(const Key& other) const noexcept;
        bool operator==(const Key& other) const noexcept = default;
    };

    struct Point
    {
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
    };

    struct PointUpdate
    {
        Key key;
        Point point;
        std::uint8_t slot = 0U;
        bool createdEffect = false;
    };

    struct ContactResult
    {
        bool accepted = false;
        bool pairCreated = false;
        bool playSound = false;
        std::size_t sound = invalidSound;
        std::vector<PointUpdate> points;
    };

    struct Release
    {
        Key key;
        std::uint8_t slot = 0U;
        bool death = true;
    };

    void Reset(std::size_t soundCount = 0U) noexcept;
    ContactResult OnContact(
        Key key, float frictionForce, bool firstShapeIsWheel,
        bool secondShapeIsWheel, std::span<const Point> points,
        float randomUnit);
    std::vector<Release> OnProgress(float deltaTime);

    std::size_t GetPairCount() const noexcept;
    std::size_t GetContactCount(Key key) const noexcept;

private:
    struct Contact
    {
        Point point;
        float time = 0.0F;
        bool effect = false;
    };

    struct ContactNode
    {
        std::vector<Contact> contacts;
        std::size_t last = 0U;
        std::size_t sound = invalidSound;
    };

    std::map<Key, ContactNode> contacts_;
    std::size_t soundCount_ = 0U;
};

// Backend-neutral gameplay portion of Logic. Network transport remains at
// the session boundary; selection, reflector policy and GameObject dispatch
// follow Logic.cpp.
class Logic
{
public:
    using ContactRange = std::array<float, 2U>;

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
        std::span<WeaponItem* const> primaryWeapons,
        bool human) noexcept;

    static float ResolveDamage(
        const Player* targetPlayer, float value,
        DamageType damageType) noexcept;
    static GameObject::DamageResult Damage(
        GameObject& target, std::size_t senderPlayerId,
        float supportedValue, DamageType damageType) noexcept;
    static GameObject::DamageResult Damage(
        GameObject& target, std::size_t senderPlayerId,
        float supportedValue, float authoritativeLife,
        bool authoritativeDeath, DamageType damageType) noexcept;

    struct TakeBonusResult
    {
        PlayerBonusResult player;
        bool taken = false;
    };

    static TakeBonusResult TakeBonus(
        Player* player, GameObject* bonus, PlayerBonusType type,
        float value,
        const std::vector<std::uint32_t>& maximumCharges,
        float randomUnit) noexcept;

    struct GameObjectProgress
    {
        std::size_t progressed = 0U;
        std::size_t removed = 0U;
    };

    Logic() = default;
    ~Logic();
    Logic(const Logic&) = delete;
    Logic& operator=(const Logic&) = delete;

    // Source Logic::RegGameObj takes ownership of prepared transient
    // GameObjects (Weapon.cpp uses it for every successful fast projectile).
    void RegGameObj(GameObject* value);
    void CleanGameObjs() noexcept;
    GameObjectProgress ProgressGameObjs(float deltaTime) noexcept;
    std::size_t GetGameObjCount() const noexcept;

    // Logic.cpp owns the global contact behavior and the four serialized
    // GameCar contact ranges. Physics/audio remain backend adapters.
    void ResetContactBehavior(std::size_t soundCount = 0U) noexcept;
    PairPxContactEffect& GetPairPxContactEffect() noexcept;
    const PairPxContactEffect& GetPairPxContactEffect() const noexcept;

    const ContactRange& GetTouchBorderDamage() const noexcept;
    void SetTouchBorderDamage(ContactRange value) noexcept;
    const ContactRange& GetTouchBorderDamageForce() const noexcept;
    void SetTouchBorderDamageForce(ContactRange value) noexcept;
    const ContactRange& GetTouchCarDamage() const noexcept;
    void SetTouchCarDamage(ContactRange value) noexcept;
    const ContactRange& GetTouchCarDamageForce() const noexcept;
    void SetTouchCarDamageForce(ContactRange value) noexcept;

private:
    std::vector<std::unique_ptr<GameObject>> gameObjects_;
    PairPxContactEffect pairPxContactEffect_;
    ContactRange touchBorderDamage_{};
    ContactRange touchBorderDamageForce_{};
    ContactRange touchCarDamage_{};
    ContactRange touchCarDamageForce_{};
};

} // namespace r3d::game::originalrace::source
