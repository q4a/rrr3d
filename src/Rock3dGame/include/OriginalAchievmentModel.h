#pragma once

#include "OriginalGameObject.h"
#include "OriginalProfile.h"
#include "OriginalRace.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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

// Names and ordering match Achievment::State in the Windows source.  The
// serialized asLocked/asUnlocked/asOpened tokens remain untouched in the
// profile adapter.
enum class AchievmentState : std::uint8_t
{
    Locked,
    Unlocked,
    Opened,
};

struct AchievmentItemView
{
    std::uint32_t classId = 0U;
    AchievmentState state = AchievmentState::Locked;
    std::uint32_t price = 0U;
    int gamerId = 0;
};

// Portable owner for the active AchievmentModel.cpp condition graph.  XML
// parsing stays in OriginalRace; this class owns condition race state,
// iteration persistence, campaign scoring and source event processing.
class AchievmentModel
{
public:
    using Iterations = std::map<std::string, std::uint32_t>;
    using Items = std::map<std::string, AchievementItemProfile>;

    void Configure(
        const std::vector<AchievementDefinition>* definitions,
        std::uint32_t initialPoints,
        const Iterations& initialIterations,
        float difficultyMultiplier) noexcept;
    void ResetRaceState() noexcept;
    void SetCampaign(bool value) noexcept;

    // Achievment/AchievmentMapObj/AchievmentGamer source layer.  XML remains
    // an adapter concern; this owner performs all state transitions and
    // availability checks used by Garage, GamersFrame and AchievmentFrame.
    void ConfigureItems(const Items& items);
    void WriteItems(Items& items) const;
    std::optional<AchievmentItemView> GetItem(
        std::string_view name) const noexcept;
    bool Unlock(std::string_view name);
    bool Open(std::string_view name);
    bool Buy(std::string_view name);
    bool ConsumePoints(std::uint32_t value) noexcept;
    bool CheckAchievment(std::string_view name) const noexcept;
    bool CheckMapObj(std::string_view record) const noexcept;
    bool CheckGamerId(int gamerId) const noexcept;

    // Profile-only callers such as Garage construction execute the same
    // source rules without inventing a second parser/state machine.
    static bool CheckAchievment(
        const Items& items, std::string_view name) noexcept;
    static bool CheckMapObj(
        const Items& items, std::string_view record) noexcept;
    static bool CheckGamerId(
        const Items& items, int gamerId) noexcept;

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

    static AchievmentState ReadState(
        const AchievementItemProfile& item) noexcept;
    static void WriteState(
        AchievementItemProfile& item, AchievmentState state);
    static std::uint32_t ReadUnsigned(
        const AchievementItemProfile& item,
        std::string_view field) noexcept;

    const std::vector<AchievementDefinition>* definitions_ = nullptr;
    Items items_;
    std::vector<ConditionState> conditions_;
    std::uint32_t points_ = 0U;
    float difficultyMultiplier_ = 1.0F;
    std::uint32_t globalKills_ = 0U;
    bool campaign_ = true;
    bool raceStarted_ = true;
    bool raceFinished_ = false;
};

} // namespace r3d::game::originalrace::source
