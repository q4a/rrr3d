#pragma once

#include "OriginalGameObject.h"
#include "OriginalPlayer.h"
#include "OriginalWeapon.h"
#include "OriginalWorld.h"

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

class Map;
class Logic;
class LogicBehaviors;

enum class LogicBehaviorType : std::uint8_t
{
    PairPxContactEffect = 0U,
    Count,
};

// Logic.cpp keeps global, non-GameObject behaviors in a separate owner.
// The owner supplies Map/Logic access and the same World progress
// registration used by the original IProgressEvent base.
class LogicBehavior : public ProgressEvent
{
public:
    virtual ~LogicBehavior() = default;

    LogicBehaviorType GetType() const noexcept;
    LogicBehaviors* GetOwner() noexcept;
    const LogicBehaviors* GetOwner() const noexcept;
    Logic* GetLogic() noexcept;
    const Logic* GetLogic() const noexcept;
    Map* GetMap() noexcept;
    const Map* GetMap() const noexcept;

protected:
    LogicBehavior(
        LogicBehaviors* owner, LogicBehaviorType type) noexcept;
    void RegProgressEvent();
    void UnregProgressEvent() noexcept;

private:
    friend class LogicBehaviors;
    LogicBehaviors* owner_ = nullptr;
    LogicBehaviorType type_ = LogicBehaviorType::PairPxContactEffect;
};

// Portable owner for Logic.cpp::PairPxContactEffect.  Jolt supplies actor
// identities, friction magnitude and manifold points; this class preserves
// the original two-effects-per-pair cursor and strict 0.1-second release.
class PairPxContactEffect final : public LogicBehavior
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

    explicit PairPxContactEffect(LogicBehaviors* owner);
    ~PairPxContactEffect() override;

    void Reset(std::size_t soundCount = 0U) noexcept;
    ContactResult OnContact(
        Key key, float frictionForce, bool firstShapeIsWheel,
        bool secondShapeIsWheel, std::span<const Point> points,
        float randomUnit);
    // Logic.cpp registers this behavior in World's progress list. Native
    // effect destruction is consumed separately after the source callback.
    void OnProgress(float deltaTime) override;
    std::vector<Release> TakeReleases();

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
    std::vector<Release> pendingReleases_;
    std::size_t soundCount_ = 0U;
};

// Concrete counterpart of Logic.cpp::LogicBehaviors. The Windows catalog
// contains exactly one shipped type, PairPxContactEffect; Jolt contact
// conversion enters here rather than bypassing the owner.
class LogicBehaviors final
{
public:
    explicit LogicBehaviors(Logic* logic);
    ~LogicBehaviors();

    Logic* GetLogic() noexcept;
    const Logic* GetLogic() const noexcept;
    std::size_t GetCount() const noexcept;
    LogicBehavior* Find(LogicBehaviorType type) noexcept;
    const LogicBehavior* Find(LogicBehaviorType type) const noexcept;

    PairPxContactEffect::ContactResult OnContact(
        PairPxContactEffect::Key key, float frictionForce,
        bool firstShapeIsWheel, bool secondShapeIsWheel,
        std::span<const PairPxContactEffect::Point> points,
        float randomUnit);
    void AttachWorld();
    void DetachWorld() noexcept;

private:
    Logic* logic_ = nullptr;
    std::unique_ptr<PairPxContactEffect> pairPxContactEffect_;
};

// Backend-neutral gameplay portion of Logic. Network transport remains at
// the session boundary; selection, reflector policy and GameObject dispatch
// follow Logic.cpp.
class Logic final : public WorldHost
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
        bool playerApplied = false;
        bool requestSenderPlayer = false;
    };

    static TakeBonusResult TakeBonus(
        Player* player, GameObject* bonus, PlayerBonusType type,
        float value,
        const std::vector<std::uint32_t>& maximumCharges,
        float randomUnit, bool networkGame = false,
        bool senderNetworkPlayerAvailable = false) noexcept;

    // Logic.cpp::MineContact selects either the contacted NetPlayer RPC or
    // the immediate Proj::MineContact overload. Network ownership is checked
    // by NetPlayer itself; the backend only reports whether that model exists.
    struct MineContactResult
    {
        bool accepted = false;
        bool requestTargetPlayer = false;
        bool applyProjectile = false;
    };

    static MineContactResult MineContact(
        Proj* sender, GameObject* target, bool networkGame,
        bool targetNetworkPlayerAvailable) noexcept;

    struct GameObjectProgress
    {
        std::size_t progressed = 0U;
        std::size_t removed = 0U;
    };

    struct ProgressResult
    {
        GameObjectProgress decoration;
        GameObjectProgress effects;
        GameObjectProgress cars;
        GameObjectProgress bonuses;
        GameObjectProgress transient;
    };

    Logic();
    ~Logic();
    Logic(const Logic&) = delete;
    Logic& operator=(const Logic&) = delete;

    // Source Logic::RegGameObj takes ownership of prepared transient
    // GameObjects (Weapon.cpp uses it for every successful fast projectile).
    void RegGameObj(GameObject* value);
    bool HasGameObj(const GameObject* value) const noexcept;
    void CleanGameObjs() noexcept;
    GameObjectProgress ProgressGameObjs(float deltaTime) noexcept;
    ProgressResult OnProgress(float deltaTime) noexcept;
    std::size_t GetGameObjCount() const noexcept;
    void SetMap(Map* value) noexcept;
    Map* GetMap() noexcept;
    const Map* GetMap() const noexcept;

    // Exact LogicBehavior::RegProgressEvent ownership. World invokes Logic
    // first through WorldHost, then PairPxContactEffect through its ordered
    // progress list.
    void AttachWorld(WorldEventPump* world) noexcept;
    void DetachWorld() noexcept;
    WorldEventPump* GetWorld() noexcept;
    const WorldEventPump* GetWorld() const noexcept;
    void RegFixedStepEvent(FixedStepEvent* event);
    void UnregFixedStepEvent(FixedStepEvent* event) noexcept;
    void RegProgressEvent(ProgressEvent* event);
    void UnregProgressEvent(ProgressEvent* event) noexcept;
    void RegLateProgressEvent(LateProgressEvent* event);
    void UnregLateProgressEvent(LateProgressEvent* event) noexcept;
    void RegFrameEvent(FrameEvent* event);
    void UnregFrameEvent(FrameEvent* event) noexcept;
    void OnLogicProgress(float deltaTime) override;
    const ProgressResult& GetLastProgressResult() const noexcept;

    // Logic.cpp owns the global contact behavior and the four serialized
    // GameCar contact ranges. Physics/audio remain backend adapters.
    void ResetContactBehavior(std::size_t soundCount = 0U) noexcept;
    PairPxContactEffect::ContactResult OnContact(
        PairPxContactEffect::Key key, float frictionForce,
        bool firstShapeIsWheel, bool secondShapeIsWheel,
        std::span<const PairPxContactEffect::Point> points,
        float randomUnit);
    LogicBehaviors& GetBehaviors() noexcept;
    const LogicBehaviors& GetBehaviors() const noexcept;
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
    WorldEventPump* world_ = nullptr;
    Map* map_ = nullptr;
    std::vector<std::unique_ptr<GameObject>> gameObjects_;
    std::unique_ptr<LogicBehaviors> behaviors_;
    ProgressResult lastProgressResult_;
    ContactRange touchBorderDamage_{};
    ContactRange touchBorderDamageForce_{};
    ContactRange touchCarDamage_{};
    ContactRange touchCarDamageForce_{};
};

} // namespace r3d::game::originalrace::source
