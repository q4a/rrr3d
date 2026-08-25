#pragma once

#include "OriginalGameObject.h"
#include "OriginalGameCar.h"
#include "OriginalTrace.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <vector>

namespace r3d::game::originalrace::source
{

// Slot order deliberately follows Player::SlotType from the Windows source:
// stHyper, stMine, stWeapon1..stWeapon4.  Player::TakeBonus depends on this
// order before applying its rounded random selection.
enum class PlayerBonusSlot : std::uint8_t
{
    None,
    Primary,
    Hyper,
    Mine,
};

enum class PlayerBonusType : std::uint8_t
{
    Money,
    Charge,
    Medpack,
    Immortal,
};

struct PlayerBonusResult
{
    PlayerBonusSlot slot = PlayerBonusSlot::None;
    std::size_t weapon = std::numeric_limits<std::size_t>::max();
    std::uint32_t amount = 0U;
};

enum class PlayerRestoreStep : std::uint8_t
{
    None,
    QueueRespawn,
    ActivateCar,
};

enum class PlayerBlockMove : std::uint8_t
{
    Unblocked,
    Coast,
    Brake,
};

// Public events emitted by the source car GameObject and its Player listener.
// The race adapter consumes this queue without recreating their ordering.
enum class PlayerGameEventKind : std::uint8_t
{
    Damage,
    Kill,
    Overboard,
    DeathMine,
    Death,
};

struct PlayerGameEvent
{
    PlayerGameEventKind kind = PlayerGameEventKind::Damage;
    std::size_t otherPlayerId = GameObject::undefinedPlayerId;
    float value = 0.0F;
    DamageType damageType = DamageType::Simple;
};

// Backend result of the downward PhysX query made by Player::ResetCar.
// The source accepts the track plane and the player's own car, rejects every
// other shape, and treats a missing hit like the global death plane on the
// first sample.
enum class ResetCarRayKind : std::uint8_t
{
    None,
    TrackPlane,
    DeathPlane,
    OwnCar,
    Blocked,
};

struct ResetCarPose
{
    bool valid = false;
    Trace::NodeRef node;
    TraceVec3 position;
    TraceVec3 direction{1.0F, 0.0F, 0.0F};
};

using ResetCarRayCast =
    std::function<ResetCarRayKind(const TraceVec3&)>;

// Portable transcription of the gameplay-owned portion of Player.  Renderer
// actor ownership and the PhysX RockCar pointer remain backend boundaries,
// while race state, inventory, bonuses, finish blocking and restore lifecycle
// retain the source class's rules.  Fields remain visible during the staged
// object-graph migration because HUD/network adapters still consume them.
class Player : public GameObject
{
public:
    static constexpr std::size_t invalidWeapon =
        std::numeric_limits<std::size_t>::max();
    static constexpr std::size_t weaponSlotCount = 4U;
    static constexpr float restoreCarSeconds = 2.0F;
    static constexpr float finishBlockSeconds = 0.3F;

    static const std::array<float, 3> humanEasingMinimumDistance;
    static const std::array<float, 3> humanEasingMaximumDistance;
    static const std::array<float, 3> humanEasingMinimumSpeed;
    static const std::array<float, 3> humanEasingMaximumSpeed;

    struct BehaviorProgressResult
    {
        GameObject::ProgressResult gameObject;
        bool lowLifeActivated = false;
        bool lowLifeReleased = false;
        bool slowSpeedLimited = false;
        bool slowReleased = false;
    };

    static constexpr std::uint32_t cheatDisabled = 0U;
    static constexpr std::uint32_t cheatEnableSlower = 1U << 0U;
    static constexpr std::uint32_t cheatEnableFaster = 1U << 1U;

    struct CheatPlayerView
    {
        std::size_t playerId = 0U;
        bool humanOrOpponent = false;
        bool active = true;
        float lap = 0.0F;
    };

    struct CheatResult
    {
        bool slower = false;
        bool faster = false;
        float speedLimit = 0.0F;
        float torqueScale = 1.0F;
        float steeringScale = 1.0F;
    };

    struct ProgressResult
    {
        CheatResult cheat;
        PlayerRestoreStep restore = PlayerRestoreStep::None;
        PlayerBlockMove blockMove = PlayerBlockMove::Unblocked;
    };
    static const std::array<float, 3> computerCheatMinimumTorque;
    static const std::array<float, 3> computerCheatMaximumTorque;
    static const std::array<float, 3> humanArmorScale;

    class CarState
    {
    public:
        struct UpdateResult
        {
            Trace::NodeRef previousLast;
            Trace::NodeRef currentTile;
            Trace::NodeRef lastNode;
            bool lastNodeChanged = false;
            bool lapPassed = false;
            bool moveInverseStarted = false;
            bool lostControl = false;
        };

        void Reset(Trace* trace = nullptr) noexcept;
        UpdateResult Update(Trace& trace,
                            const TraceVec3& position,
                            const TraceVec3& direction,
                            float vehicleSpeed,
                            float deltaTime);

        WayNode* GetCurTile(bool lastCorrect = false) noexcept;
        const WayNode* GetCurTile(bool lastCorrect = false) const noexcept;
        WayNode* GetLiveTile() noexcept;
        const WayNode* GetLiveTile() const noexcept;
        WayNode* GetCurNode() noexcept;
        const WayNode* GetCurNode() const noexcept;
        WayNode* GetLastNode() noexcept;
        const WayNode* GetLastNode() const noexcept;
        WayNode* EnsureLastNode() noexcept;
        Trace::NodeRef GetCurTileRef(bool lastCorrect = false) const noexcept;
        Trace::NodeRef GetLiveTileRef() const noexcept;
        Trace::NodeRef GetCurNodeRef() const noexcept;
        Trace::NodeRef GetLastNodeRef() const noexcept;
        std::int32_t GetPathIndex(bool lastCorrect = false) const noexcept;
        bool IsMainPath(bool lastCorrect = false) const noexcept;
        float GetPathLength(bool lastCorrect = false) const noexcept;
        float GetDist(bool lastCorrect = false) const noexcept;
        float GetLap(bool lastCorrect = false) const noexcept;
        float GetSpeed() const noexcept;
        TraceVec3 GetPosition() const noexcept;
        TraceVec3 GetDirection3() const noexcept;
        TraceVec3 GetMapPos() const noexcept;
        float GetLastNodeCoordX() const noexcept;
        std::uint32_t GetTrack() const noexcept;

        std::uint32_t numLaps = 0U;
        bool moveInverse = false;
        bool cheatSlower = false;
        bool cheatFaster = false;

    private:
        void SetCurTile(WayNode* value) noexcept;
        void SetCurNode(WayNode* value) noexcept;
        void SetLastNode(WayNode* value) noexcept;

        Trace* trace_ = nullptr;
        TraceVec3 position_{};
        TraceVec3 direction_{1.0F, 0.0F, 0.0F};
        TraceVec3 direction3_{1.0F, 0.0F, 0.0F};
        float speed_ = 0.0F;
        WayNode* curTile_ = nullptr;
        WayNode* curNode_ = nullptr;
        WayNode* lastNode_ = nullptr;
        float lastNodeCoordX_ = 0.5F;
        std::uint32_t track_ = 0U;
        float moveInverseStart_ = -1.0F;
        float maximumSpeed_ = 0.0F;
        float maximumSpeedTime_ = 0.0F;
        TraceVec3 fallbackMapPosition_{};
    };

    void Reset(float newMaximumLife, std::uint32_t initialPlace,
               Trace* trace = nullptr) noexcept;
    void OnLapPass(std::size_t weaponDefinitionCount) noexcept;
    void ReloadWeapons(std::size_t weaponDefinitionCount) noexcept;
    void SyncSelectedWeapon(std::size_t weaponDefinitionCount) noexcept;

    PlayerBonusResult TakeMoney(float value) noexcept;
    PlayerBonusResult TakeMedpack(float value) noexcept;
    PlayerBonusResult TakeImmortal(float value) noexcept;
    PlayerBonusResult TakeAmmunition(
        float value,
        const std::vector<std::uint32_t>& maximumCharges,
        float randomUnit) noexcept;
    PlayerBonusResult TakeBonus(
        PlayerBonusType type, float value,
        const std::vector<std::uint32_t>& maximumCharges,
        float randomUnit) noexcept;
    PlayerBonusResult TakeBonus(
        GameObject& bonus, PlayerBonusType type, float value,
        const std::vector<std::uint32_t>& maximumCharges,
        float randomUnit) noexcept;
    BehaviorProgressResult ProgressBehaviors(
        float deltaTime, float lowLifeLevel,
        float linearSpeed) noexcept;
    CheatResult CheatUpdate(
        std::uint32_t cheatMask, std::size_t playerId,
        std::size_t difficulty,
        const std::vector<CheatPlayerView>& players) noexcept;
    ProgressResult OnProgress(
        float deltaTime, bool carPresent,
        std::uint32_t cheatMask, std::size_t playerId,
        std::size_t difficulty,
        const std::vector<CheatPlayerView>& players) noexcept;
    Player* FindClosestEnemy(
        float viewAngle, bool zTest,
        std::span<Player* const> players) noexcept;
    bool ConsumeEnergyDamageEffectCreated() noexcept;
    std::vector<PlayerGameEvent> TakeGameEvents() noexcept;

    void SetFinished(bool value, float time = -1.0F) noexcept;
    void Complete(std::uint32_t resultPlace,
                  std::uint32_t resultMoney,
                  std::uint32_t resultPoints,
                  float time) noexcept;
    void AddMoney(std::int32_t value) noexcept;
    void AddPoints(std::int32_t value) noexcept;
    void ApplyRaceReward() noexcept;

    void Destroy() noexcept;
    PlayerRestoreStep ProgressRestore(float seconds) noexcept;
    ResetCarPose ResetCar(const ResetCarRayCast& rayCast);
    void Disconnect() noexcept;
    void ResetBlock(bool block) noexcept;
    bool IsBlock() const noexcept;
    float GetBlockTime() const noexcept;
    void SetBlockTime(float value) noexcept;
    PlayerBlockMove ProgressBlock(float seconds) noexcept;
    float FinishBrake(float elapsedSeconds) const noexcept;

    static std::size_t RoundedRandomIndex(
        std::size_t count, float randomUnit) noexcept;
    static std::uint32_t BonusCharge(
        std::uint32_t maximumCharge, float value) noexcept;

    std::size_t nextPathNode = 1;
    std::uint32_t place = 1;
    std::uint32_t ammunition = 10;
    std::uint32_t mines = 0;
    std::uint32_t mineCapacity = 0;
    std::array<std::size_t, weaponSlotCount> weaponSlots{
        invalidWeapon, invalidWeapon, invalidWeapon, invalidWeapon};
    std::array<std::uint32_t, weaponSlotCount> weaponCharges{};
    std::array<std::uint32_t, weaponSlotCount> weaponCapacity{};
    // Proj::DrobilkaUpdate rotates the mounted weapon actor itself. Keep the
    // actor rotation independent from projectile age for renderer/contact use.
    std::array<float, weaponSlotCount> weaponSpinRadians{};
    std::size_t selectedWeaponSlot = 0;
    std::size_t selectedWeapon = invalidWeapon;
    std::size_t hyperWeapon = invalidWeapon;
    std::uint32_t hyperCharge = 0;
    std::uint32_t hyperCapacity = 0;
    std::size_t mineWeapon = invalidWeapon;
    std::uint32_t money = 0;
    std::uint32_t points = 0;
    std::uint32_t pickedMoney = 0;
    std::uint32_t rewardMoney = 0;
    std::uint32_t rewardPoints = 0;
    float speedBoostSeconds = 0.0F;
    float restoreSeconds = 0.0F;
    float finishTime = -1.0F;
    float blockSeconds = -1.0F;
    bool finished = false;
    bool disconnected = false;
    LowLifePoints lowLifePoints;
    DamageEffect energyDamageEffect{DamageType::Energy, 0.5F};
    ImmortalEffect immortalEffect;
    SlowEffect slowEffect;
    GameCar gameCar;
    CarState car;

protected:
    void OnDeathEvent(
        DamageType damageType, GameObject* target) noexcept override;
    void OnDamageEvent(float value, DamageType damageType) noexcept override;
    void OnDamageDispatchEvent(
        std::size_t senderPlayerId, float value,
        DamageType damageType) noexcept override;
    void OnKillDispatchEvent(
        std::size_t senderPlayerId, float value,
        DamageType damageType) noexcept override;
    void OnImmortalStatusEvent(bool status) noexcept override;

private:
    bool energyDamageEffectCreated_ = false;
    std::vector<PlayerGameEvent> gameEvents_;
};

} // namespace r3d::game::originalrace::source
