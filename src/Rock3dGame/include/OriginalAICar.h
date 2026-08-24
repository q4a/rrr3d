#pragma once

#include "OriginalPlayer.h"

#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace r3d::game::originalrace::source
{

// Backend-neutral transcription of the original AICar path and control
// states.  The Windows class wrote directly to GameCar/PhysX; this boundary
// returns the same source command so OriginalRaceSession can apply it to Jolt.
class AICar
{
public:
    using RandomSource = float (*)();
    using UniformRandomSource = double (*)();
    static constexpr std::size_t invalidIndex =
        std::numeric_limits<std::size_t>::max();

    struct VehicleState
    {
        TraceVec3 position{};
        TraceVec3 direction{1.0F, 0.0F, 0.0F};
        TraceVec3 direction3{1.0F, 0.0F, 0.0F};
        float speed = 0.0F;
        float size = 0.0F;
        float steeringControl = 0.0F;
        bool mapObject = true;
        bool cheatSlower = false;
    };

    enum class MoveCarState : std::uint8_t
    {
        None,
        Accelerate,
        Brake,
        Reverse
    };

    struct Command
    {
        MoveCarState move = MoveCarState::None;
        float steeringAngle = 0.0F;
        bool resetCar = false;
    };

    struct AttackTarget
    {
        TraceVec3 position{};
        float size = 0.0F;
        float radius = 0.0F;
        bool active = false;
    };

    struct AttackWeapon
    {
        std::size_t slot = invalidIndex;
        std::uint32_t projectileType = 0U;
        float maximumDistance = 0.0F;
        std::uint32_t capacity = 0U;
        std::uint32_t charge = 0U;
        bool ready = false;
    };

    struct HyperState
    {
        bool installed = false;
        float projectileSpeed = 0.0F;
        std::uint32_t capacity = 0U;
        std::uint32_t charge = 0U;
    };

    struct MineState
    {
        bool installed = false;
        bool oil = false;
        std::uint32_t capacity = 0U;
        std::uint32_t charge = 0U;
    };

    struct AttackContext
    {
        std::size_t owner = invalidIndex;
        std::span<const AttackTarget> targets;
        std::span<const AttackWeapon> weapons;
        HyperState hyper;
        MineState mine;
        bool enabled = true;
        RandomSource randomSource = nullptr;
        UniformRandomSource uniformRandomSource = nullptr;
    };

    struct AttackDecision
    {
        std::size_t weaponSlot = invalidIndex;
        std::size_t weaponTarget = invalidIndex;
        bool useHyper = false;
        bool useMine = false;

        bool hasWeaponShot() const noexcept
        {
            return weaponSlot != invalidIndex &&
                   weaponTarget != invalidIndex;
        }
    };

    struct PathState
    {
        explicit PathState(std::uint32_t trackCount = 4U);

        void Reset(std::uint32_t trackCount);
        void ResetTracks();
        void LockTrack(std::uint32_t track);
        bool FindFirstUnlockTrack(std::uint32_t track,
                                  std::uint32_t target,
                                  std::uint32_t& result) const;
        bool FindLastUnlockTrack(std::uint32_t track,
                                 std::uint32_t target,
                                 std::uint32_t& result) const;
        bool FindFirstSiblingUnlock(std::uint32_t track,
                                    std::uint32_t& result) const;
        bool FindLastSiblingUnlock(std::uint32_t track,
                                   std::uint32_t& result) const;
        void ComputeMovDir(const Player::CarState& car,
                           const VehicleState& vehicle);
        void Update(float deltaTime, const Player::CarState& car,
                    const VehicleState& vehicle,
                    RandomSource randomSource = nullptr);

        WayNode* curTile = nullptr;
        WayNode* nextTile = nullptr;
        WayNode* curNode = nullptr;
        std::vector<bool> freeTracks;
        std::vector<bool> lockTracks;
        float dirArea = 0.0F;
        TraceVec2 moveDir{};
        bool brake = false;

    private:
        std::uint32_t trackCount_ = 4U;
    };

    struct ControlState
    {
        void Reset() noexcept;
        bool UpdateResetCar(float deltaTime,
                            const Player::CarState& car,
                            const VehicleState& vehicle) noexcept;
        Command Update(float deltaTime, const VehicleState& vehicle,
                       const PathState& path, bool enabled) noexcept;

        float steerAngle = 0.0F;
        float timeBlocking = 0.0F;
        bool blocking = false;
        bool backMovingMode = false;
        bool backMoving = false;
        float timeBackMoving = 0.0F;
        float timeResetBlockCar = 0.0F;
    };

    struct AttackState
    {
        void Reset() noexcept;
        void DisposeTarget(std::size_t player) noexcept;
        AttackDecision Update(const Player::CarState& car,
                              const VehicleState& vehicle,
                              const PathState& path,
                              const AttackContext& context);

        std::size_t target = invalidIndex;
        std::size_t backTarget = invalidIndex;
        float placeMineRandom = -1.0F;

    private:
        std::size_t FindEnemy(const Player::CarState& car,
                              const VehicleState& vehicle,
                              const AttackContext& context,
                              int direction,
                              std::size_t currentEnemy) const;
        std::size_t ShotByEnemy(const Player::CarState& car,
                                const VehicleState& vehicle,
                                const AttackContext& context,
                                std::size_t enemy) const;
    };

    explicit AICar(std::uint32_t trackCount = 4U);
    void Reset(std::uint32_t trackCount = 4U);
    Command Update(float deltaTime, const Player::CarState& car,
                   const VehicleState& vehicle, bool enabled = true,
                   RandomSource randomSource = nullptr);
    bool TakeResetCar() noexcept;

    PathState path;
    AttackState attack;
    ControlState control;

private:
    bool resetCar_ = false;
};

} // namespace r3d::game::originalrace::source
