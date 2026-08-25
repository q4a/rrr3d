#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace r3d::game::originalrace::source
{

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

} // namespace r3d::game::originalrace::source
