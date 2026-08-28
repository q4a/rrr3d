#pragma once

#include "OriginalWorld.h"

#include "OriginalGameMode.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace r3d::game::originalrace::source
{

class Player;

// Gameplay-owned flags and Player transitions from
// Race::StartRace/GoRace/ExitRace. World, renderer and physics teardown stay
// at their platform boundaries; Player::FreeCar(true) remains source state.
class RaceRunState
{
public:
    void Reset() noexcept;
    bool StartRace(
        std::span<Player> players, Player* human,
        bool night, std::size_t weaponDefinitionCount) noexcept;
    void GoRace(Player* human) noexcept;
    bool ExitRace(std::span<Player> players) noexcept;

    bool IsStartRace() const noexcept;
    bool IsRaceGo() const noexcept;

private:
    bool startRace_ = false;
    bool goRace_ = false;
};

// Backend-neutral transcription of Race::Result.  The source Race owns this
// data after Player::pickedMoney has been reset; HUD, network and campaign
// settlement must therefore read it from here rather than from Player.
struct RaceResult
{
    std::size_t playerId = 0U;
    std::uint32_t place = 0U;
    std::uint32_t money = 0U;
    std::uint32_t points = 0U;
    std::uint32_t pickedMoney = 0U;
    float voiceNameDuration = 0.0F;
};

struct RaceLifecyclePlayer
{
    std::size_t playerId = 0U;
    bool human = false;
    bool opponent = false;
    bool disconnected = false;
    bool finished = false;
    std::uint32_t laps = 0U;
    float lapPosition = 0.0F;
    std::uint32_t pickedMoney = 0U;
};

enum class RaceLifecycleEventKind : std::uint8_t
{
    LeadFinish,
    SecondFinish,
    ThirdFinish,
    LastFinish,
    RaceFinish,
    PassLap,
    LastLap,
};

struct RaceLifecycleEvent
{
    RaceLifecycleEventKind kind = RaceLifecycleEventKind::PassLap;
    std::size_t playerId = 0U;
};

struct RaceLapPassResult
{
    std::optional<RaceResult> completed;
    std::vector<RaceLifecycleEvent> events;
};

// Owns the portable form of Race::_results and the original event ordering in
// Race::OnLapPass/CompleteRace.  Rendering and physics remain adapters.
class RaceLifecycle
{
public:
    void Reset() noexcept;

    RaceLapPassResult OnLapPass(
        const RaceLifecyclePlayer& player,
        std::uint32_t lapCount,
        std::size_t activePlayerCount,
        bool hasHuman,
        std::size_t leaderPlayerId,
        const std::array<std::uint32_t, 3>& rewardMoney,
        const std::array<std::uint32_t, 3>& rewardPoints);

    std::vector<RaceResult> CompleteRemaining(
        std::vector<RaceLifecyclePlayer> players,
        std::uint32_t lapCount,
        const std::array<std::uint32_t, 3>& rewardMoney,
        const std::array<std::uint32_t, 3>& rewardPoints);

    void LoadResults(std::vector<RaceResult> results);
    const RaceResult* GetResult(std::size_t playerId) const noexcept;
    const std::vector<RaceResult>& GetResults() const noexcept;

private:
    std::optional<RaceResult> CompleteRace(
        const RaceLifecyclePlayer& player,
        const std::array<std::uint32_t, 3>& rewardMoney,
        const std::array<std::uint32_t, 3>& rewardPoints);

    std::vector<RaceResult> results_;
};

struct RacePlacePlayer
{
    std::size_t playerId = 0U;
    bool disconnected = false;
    bool finished = false;
    std::uint32_t place = 0U;
    float lap = 0.0F;
    float lastCorrectLap = 0.0F;
    float lastCorrectPathLength = 1.0F;
    bool lastCorrectMainPath = false;
};

enum class RacePlaceEventKind : std::uint8_t
{
    LeadChanged,
    ThirdChanged,
    LastFar,
    Domination,
    ThirdFar,
};

struct RacePlaceEvent
{
    RacePlaceEventKind kind = RacePlaceEventKind::LeadChanged;
    std::size_t playerId = 0U;
    std::size_t otherPlayerId = 0U;
};

struct RacePlaceUpdate
{
    std::vector<std::size_t> order;
    std::vector<RacePlaceEvent> events;
};

// Portable owner of Race::_playerPlaceList and Race::OnLateProgress. It keeps
// the previous ordered list because source lead/third events compare pointers
// from the preceding late-progress pass, not the mutable Player::place field.
class RacePlaceModel final : public LateProgressEvent
{
public:
    void Reset() noexcept;
    RacePlaceUpdate Update(const std::vector<RacePlacePlayer>& players,
                           bool hasResults);
    void PrepareLateProgress(
        std::vector<RacePlacePlayer> players, bool hasResults);
    bool HasPreparedLateProgress() const noexcept;
    RacePlaceUpdate TakeLateProgressUpdate();
    void OnLateProgress(
        float deltaTime, bool physicsStep) override;

private:
    std::vector<std::size_t> order_;
    float lastLeadPlace_ = 0.0F;
    float lastThirdPlace_ = 0.0F;
    std::vector<RacePlacePlayer> preparedPlayers_;
    RacePlaceUpdate preparedUpdate_;
    bool preparedHasResults_ = false;
    bool prepared_ = false;
    bool updated_ = false;
};

} // namespace r3d::game::originalrace::source
