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
#include <string>
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

// Backend-neutral ownership half of Logic.cpp::LogicEventEffect. The D3D9
// MapObj visual is replaced by a stable handle consumed by the bgfx adapter,
// while create/death/final-destroy identity remains in the source owner.
class LogicEventEffect : public LogicBehavior
{
public:
    using EffectId = std::uint64_t;
    static constexpr EffectId invalidEffect = 0U;

    struct Vector3
    {
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
    };

    ~LogicEventEffect() override;

    const std::string& GetEffectRecord() const noexcept;
    void SetEffectRecord(std::string value);
    const Vector3& GetPos() const noexcept;
    void SetPos(Vector3 value) noexcept;
    bool HasEffect(EffectId effect) const noexcept;
    bool IsEffectDying(EffectId effect) const noexcept;
    Vector3 GetEffectPosition(EffectId effect) const noexcept;
    std::size_t GetEffectCount() const noexcept;
    virtual void NotifyEffectDestroyed(EffectId effect) noexcept;

protected:
    LogicEventEffect(
        LogicBehaviors* owner, LogicBehaviorType type) noexcept;
    EffectId CreateEffect(Vector3 position);
    void SetEffectPosition(EffectId effect, Vector3 position) noexcept;
    void BeginDeleteEffect(EffectId effect) noexcept;
    void ClearEffects() noexcept;

private:
    struct EffectInstance
    {
        EffectId id = invalidEffect;
        Vector3 position;
        bool dying = false;
    };

    std::string effectRecord_;
    Vector3 position_;
    std::vector<EffectInstance> effects_;
    EffectId nextEffectId_ = 1U;
};

// Portable owner for Logic.cpp::PairPxContactEffect.  Jolt supplies actor
// identities, friction magnitude and manifold points; this class preserves
// the original two-effects-per-pair cursor and strict 0.1-second release.
class PairPxContactEffect final : public LogicEventEffect
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
        EffectId effect = invalidEffect;
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
        EffectId effect = invalidEffect;
        std::uint8_t slot = 0U;
        bool death = true;
    };

    explicit PairPxContactEffect(LogicBehaviors* owner);
    ~PairPxContactEffect() override;
    void NotifyEffectDestroyed(EffectId effect) noexcept override;

    // DataBase::Init owns the serialized effect/sound catalog. Reconfiguring
    // it also releases every live contact, just as replacing the Windows
    // database tears down the old LogicBehavior instance.
    void Configure(
        std::string effectRecord,
        std::vector<std::string> soundPaths);
    void ClearContacts() noexcept;
    void Reset() noexcept;
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
    const std::vector<std::string>& GetSounds() const noexcept;
    std::string_view GetSound(std::size_t index) const noexcept;

private:
    struct Contact
    {
        Point point;
        float time = 0.0F;
        EffectId effect = invalidEffect;
    };

    struct ContactNode
    {
        std::vector<Contact> contacts;
        std::size_t last = 0U;
        std::size_t sound = invalidSound;
    };

    std::map<Key, ContactNode> contacts_;
    std::vector<Release> pendingReleases_;
    std::vector<std::string> soundPaths_;
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
    LogicBehavior& Add(LogicBehaviorType type);
    PairPxContactEffect& AddPairPxContactEffect();
    PairPxContactEffect* FindPairPxContactEffect() noexcept;
    const PairPxContactEffect* FindPairPxContactEffect() const noexcept;

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

    // Logic::SndCategory owns the persistent gain and mute state. The native
    // audio backend is only the replacement for the three XAudio2 submix
    // voices and consumes GetVolume() at its platform boundary.
    enum class SoundCategory : std::uint8_t
    {
        Music = 0U,
        Effects,
        Voice,
        Count,
    };

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

    // DataBase.cpp installs the global contact behavior. Logic owns its
    // lifetime and the four serialized GameCar contact ranges.
    void ClearContactBehavior() noexcept;
    PairPxContactEffect::ContactResult OnContact(
        PairPxContactEffect::Key key, float frictionForce,
        bool firstShapeIsWheel, bool secondShapeIsWheel,
        std::span<const PairPxContactEffect::Point> points,
        float randomUnit);
    LogicBehaviors& GetBehaviors() noexcept;
    const LogicBehaviors& GetBehaviors() const noexcept;
    PairPxContactEffect& GetPairPxContactEffect();
    const PairPxContactEffect& GetPairPxContactEffect() const;

    const ContactRange& GetTouchBorderDamage() const noexcept;
    void SetTouchBorderDamage(ContactRange value) noexcept;
    const ContactRange& GetTouchBorderDamageForce() const noexcept;
    void SetTouchBorderDamageForce(ContactRange value) noexcept;
    const ContactRange& GetTouchCarDamage() const noexcept;
    void SetTouchCarDamage(ContactRange value) noexcept;
    const ContactRange& GetTouchCarDamageForce() const noexcept;
    void SetTouchCarDamageForce(ContactRange value) noexcept;

    float GetVolume(SoundCategory category) const noexcept;
    float GetStoredVolume(SoundCategory category) const noexcept;
    void SetVolume(SoundCategory category, float value) noexcept;
    void AutodetectVolume() noexcept;
    void Mute(SoundCategory category, bool value) noexcept;
    bool IsMuted(SoundCategory category) const noexcept;

private:
    static constexpr std::size_t SoundCategoryIndex(
        SoundCategory category) noexcept
    {
        return static_cast<std::size_t>(category);
    }

    WorldEventPump* world_ = nullptr;
    Map* map_ = nullptr;
    std::vector<std::unique_ptr<GameObject>> gameObjects_;
    std::unique_ptr<LogicBehaviors> behaviors_;
    ProgressResult lastProgressResult_;
    ContactRange touchBorderDamage_{};
    ContactRange touchBorderDamageForce_{};
    ContactRange touchCarDamage_{};
    ContactRange touchCarDamageForce_{};
    std::array<float, static_cast<std::size_t>(SoundCategory::Count)>
        soundVolumes_{1.0F, 1.0F, 1.0F};
    std::array<bool, static_cast<std::size_t>(SoundCategory::Count)>
        soundMutes_{};
};

} // namespace r3d::game::originalrace::source
