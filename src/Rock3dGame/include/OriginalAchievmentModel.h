#pragma once

#include "OriginalGameObject.h"
#include "OriginalRace.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace r3d::game::originalrace::source
{

enum class AchievmentEventKind : std::uint8_t
{
    Bonus,
    Kill,
    Damage,
    Lap,
    RaceFinish,
    Death,
};

// Backend-neutral EventData used by the original condition classes.  A
// RaceFinish intentionally has hasPlayer=false: Race::OnLapPass sends that
// source event with NULL data, and the original LapPass condition rejects it
// at its common player-data guard.
struct AchievmentEvent
{
    AchievmentEventKind kind = AchievmentEventKind::Damage;
    bool hasPlayer = true;
    std::size_t playerId = GameObject::undefinedPlayerId;
    std::size_t targetPlayerId = GameObject::undefinedPlayerId;
    float value = 0.0F;
    DamageType damageType = DamageType::Simple;
    BonusKind bonusKind = BonusKind::Unknown;
    std::uint32_t bonusTotalCount = 0U;
};

struct AchievmentRaceState
{
    std::uint32_t humanPlace = 1U;
    std::uint32_t humanLaps = 0U;
    std::uint32_t lapCount = 1U;
    std::uint32_t playerCount = 1U;
};

// Portable owner for the active AchievmentModel.cpp condition graph.  XML
// parsing stays in OriginalRace; this class owns condition race state,
// iteration persistence, campaign scoring and source event processing.
class AchievmentModel
{
public:
    using Iterations = std::map<std::string, std::uint32_t>;

    void Configure(
        const std::vector<AchievementDefinition>* definitions,
        std::uint32_t initialPoints,
        const Iterations& initialIterations,
        float difficultyMultiplier) noexcept;
    void ResetRaceState() noexcept;
    void SetCampaign(bool value) noexcept;

    std::vector<std::size_t> Process(
        float deltaTime, std::span<const AchievmentEvent> events,
        const AchievmentRaceState& raceState) noexcept;

    std::uint32_t GetPoints() const noexcept;
    Iterations GetIterations() const;

private:
    struct ConditionState
    {
        std::uint32_t iteration = 0U;
        std::uint32_t counter = 0U;
        std::uint32_t total = 0U;
        float timer = 0.0F;
    };

    bool CompleteIteration(
        std::size_t condition,
        std::vector<std::size_t>& completed) noexcept;

    const std::vector<AchievementDefinition>* definitions_ = nullptr;
    std::vector<ConditionState> conditions_;
    std::uint32_t points_ = 0U;
    float difficultyMultiplier_ = 1.0F;
    std::uint32_t globalKills_ = 0U;
    bool campaign_ = true;
    bool raceStarted_ = true;
    bool raceFinished_ = false;
};

} // namespace r3d::game::originalrace::source
