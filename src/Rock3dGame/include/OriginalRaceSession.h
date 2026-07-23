#pragma once

#include "OriginalRace.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace r3d::game::originalrace
{

enum class RacePhase
{
    Countdown,
    Racing,
    Finished,
    Paused,
};

enum class RaceEventKind
{
    CountdownChanged,
    Checkpoint,
    Lap,
    Finish,
    Respawn,
    WeaponFired,
    Damage,
    Bonus,
    DecorationDestroyed,
};

struct RaceControl
{
    r3d::physics::VehicleInput driving;
    bool useWeapon = false;
    bool reset = false;
};

struct RacerRuntime
{
    std::uint32_t completedLaps = 0;
    std::size_t nextPathNode = 1;
    std::uint32_t place = 1;
    float life = 100.0F;
    float maximumLife = 100.0F;
    std::uint32_t ammunition = 10;
    std::uint32_t mines = 0;
    std::uint32_t money = 0;
    float shieldSeconds = 0.0F;
    float speedBoostSeconds = 0.0F;
    float finishTime = -1.0F;
    bool wrongWay = false;
    bool finished = false;
};

struct RespawnRequest
{
    std::size_t racer = 0;
    Vec3 position;
    Vec3 direction{1.0F, 0.0F, 0.0F};
};

struct RaceEvent
{
    RaceEventKind kind = RaceEventKind::Checkpoint;
    std::size_t racer = 0;
    std::size_t target = 0;
    Vec3 position;
    float value = 0.0F;
};

struct RaceEffect
{
    RaceEventKind kind = RaceEventKind::WeaponFired;
    Vec3 origin;
    Vec3 target;
    float seconds = 0.0F;
};

class OriginalRaceSession
{
public:
    explicit OriginalRaceSession(const Race& race);

    void reset();
    void setPaused(bool paused) noexcept;
    void update(float seconds,
                const std::vector<r3d::physics::VehicleState>& vehicles,
                const RaceControl& humanControl);

    RacePhase phase() const noexcept;
    float countdownSeconds() const noexcept;
    float elapsedSeconds() const noexcept;
    const std::vector<r3d::physics::VehicleInput>& vehicleInputs() const
        noexcept;
    const std::vector<RacerRuntime>& racers() const noexcept;
    const std::vector<bool>& decorationActive() const noexcept;
    const std::vector<bool>& bonusActive() const noexcept;
    const std::vector<RaceEvent>& events() const noexcept;
    const std::vector<RaceEffect>& effects() const noexcept;
    std::vector<RespawnRequest> takeRespawns();

private:
    const TracePoint& tracePoint(std::size_t pathNode) const;
    void updateProgress(std::size_t racer,
                        const r3d::physics::VehicleState& vehicle);
    r3d::physics::VehicleInput aiInput(
        std::size_t racer,
        const r3d::physics::VehicleState& vehicle) const;
    void updatePlaces(
        const std::vector<r3d::physics::VehicleState>& vehicles);
    void updateGameplay(
        float seconds,
        const std::vector<r3d::physics::VehicleState>& vehicles,
        const RaceControl& humanControl);
    void queueRespawn(std::size_t racer,
                      const r3d::physics::VehicleState& vehicle);

    const Race& race_;
    RacePhase phase_ = RacePhase::Countdown;
    RacePhase phaseBeforePause_ = RacePhase::Countdown;
    float countdownSeconds_ = 3.0F;
    float elapsedSeconds_ = 0.0F;
    int countdownDisplay_ = 3;
    std::vector<RacerRuntime> racers_;
    std::vector<r3d::physics::VehicleInput> vehicleInputs_;
    std::vector<bool> decorationActive_;
    std::vector<bool> bonusActive_;
    std::vector<float> weaponCooldown_;
    std::vector<float> stuckSeconds_;
    std::vector<Vec3> previousPositions_;
    std::vector<RaceEvent> events_;
    std::vector<RaceEffect> effects_;
    std::vector<RespawnRequest> respawns_;
};

bool runOriginalRaceSessionSmokeTest(const Race& race, std::string& error);

} // namespace r3d::game::originalrace
