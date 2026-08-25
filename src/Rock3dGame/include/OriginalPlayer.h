#pragma once

#include "OriginalGameObject.h"
#include "OriginalGameCar.h"
#include "OriginalTrace.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
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
        Trace::NodeRef GetCurTileRef(bool lastCorrect = false) const noexcept;
        Trace::NodeRef GetLiveTileRef() const noexcept;
        Trace::NodeRef GetCurNodeRef() const noexcept;
        Trace::NodeRef GetLastNodeRef() const noexcept;
        std::int32_t GetPathIndex(bool lastCorrect = false) const noexcept;
        bool IsMainPath(bool lastCorrect = false) const noexcept;
        float GetPathLength(bool lastCorrect = false) const noexcept;
        float GetDist(bool lastCorrect = false) const noexcept;
        float GetLap(bool lastCorrect = false) const noexcept;
        TraceVec3 GetMapPos() const noexcept;
        float GetLastNodeCoordX() const noexcept;
        std::uint32_t GetTrack() const noexcept;

        std::uint32_t numLaps = 0U;
        bool moveInverse = false;

    private:
        void SetCurTile(WayNode* value) noexcept;
        void SetCurNode(WayNode* value) noexcept;
        void SetLastNode(WayNode* value) noexcept;

        Trace* trace_ = nullptr;
        TraceVec3 position_{};
        TraceVec3 direction_{1.0F, 0.0F, 0.0F};
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
    BehaviorProgressResult ProgressBehaviors(
        float deltaTime, float lowLifeLevel,
        float linearSpeed) noexcept;
    bool ConsumeEnergyDamageEffectCreated() noexcept;

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
    void OnDamageEvent(float value, DamageType damageType) noexcept override;
    void OnImmortalStatusEvent(bool status) noexcept override;

private:
    bool energyDamageEffectCreated_ = false;
};

} // namespace r3d::game::originalrace::source
