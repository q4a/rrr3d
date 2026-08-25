#include "OriginalRaceLifecycle.h"

#include <algorithm>

namespace r3d::game::originalrace::source
{

void RaceLifecycle::Reset() noexcept
{
    results_.clear();
}

std::optional<RaceResult> RaceLifecycle::CompleteRace(
    const RaceLifecyclePlayer& player,
    const std::array<std::uint32_t, 3>& rewardMoney,
    const std::array<std::uint32_t, 3>& rewardPoints)
{
    if (GetResult(player.playerId) != nullptr)
        return std::nullopt;

    RaceResult result;
    result.playerId = player.playerId;
    result.place = results_.empty() ? 1U : results_.back().place + 1U;
    result.voiceNameDuration = 1.5F;
    if (result.place >= 1U && result.place <= rewardMoney.size())
    {
        const std::size_t reward = result.place - 1U;
        result.money = rewardMoney[reward];
        result.points = rewardPoints[reward];
    }
    result.pickedMoney = player.pickedMoney;
    results_.push_back(result);
    return result;
}

RaceLapPassResult RaceLifecycle::OnLapPass(
    const RaceLifecyclePlayer& player,
    std::uint32_t lapCount,
    std::size_t activePlayerCount,
    bool hasHuman,
    std::size_t leaderPlayerId,
    const std::array<std::uint32_t, 3>& rewardMoney,
    const std::array<std::uint32_t, 3>& rewardPoints)
{
    RaceLapPassResult output;
    if (player.laps >= lapCount)
    {
        output.completed = CompleteRace(
            player, rewardMoney, rewardPoints);

        // This deliberately remains the source if/else-if chain. Places
        // between third and last do not have a commentator event.
        if (results_.size() == 1U)
            output.events.push_back(
                {RaceLifecycleEventKind::LeadFinish, player.playerId});
        else if (results_.size() == 2U)
            output.events.push_back(
                {RaceLifecycleEventKind::SecondFinish, player.playerId});
        else if (results_.size() == 3U)
            output.events.push_back(
                {RaceLifecycleEventKind::ThirdFinish, player.playerId});
        else if (results_.size() == activePlayerCount)
            output.events.push_back(
                {RaceLifecycleEventKind::LastFinish, player.playerId});

        const bool raceComplete =
            player.human ||
            (results_.size() >= activePlayerCount && !hasHuman);
        if (raceComplete)
            output.events.push_back(
                {RaceLifecycleEventKind::RaceFinish, player.playerId});
    }

    if (player.human)
        output.events.push_back(
            {RaceLifecycleEventKind::PassLap, player.playerId});

    if (player.human && player.laps == lapCount - 1U)
        output.events.push_back(
            {RaceLifecycleEventKind::LastLap, leaderPlayerId});

    return output;
}

std::vector<RaceResult> RaceLifecycle::CompleteRemaining(
    std::vector<RaceLifecyclePlayer> players,
    std::uint32_t lapCount,
    const std::array<std::uint32_t, 3>& rewardMoney,
    const std::array<std::uint32_t, 3>& rewardPoints)
{
    players.erase(
        std::remove_if(
            players.begin(), players.end(),
            [&](const RaceLifecyclePlayer& player) {
                return player.disconnected ||
                       GetResult(player.playerId) != nullptr;
            }),
        players.end());
    std::stable_sort(
        players.begin(), players.end(),
        [lapCount](const RaceLifecyclePlayer& first,
                   const RaceLifecyclePlayer& second) {
            const auto placePosition = [lapCount](
                                           const RaceLifecyclePlayer& value) {
                return value.human || value.opponent
                           ? -static_cast<float>(lapCount) - 1.0F +
                                 value.lapPosition
                           : value.lapPosition;
            };
            return placePosition(first) > placePosition(second);
        });

    std::vector<RaceResult> completed;
    completed.reserve(players.size());
    for (const auto& player : players)
    {
        if (auto result = CompleteRace(
                player, rewardMoney, rewardPoints))
            completed.push_back(*result);
    }
    return completed;
}

void RaceLifecycle::LoadResults(std::vector<RaceResult> results)
{
    std::stable_sort(
        results.begin(), results.end(),
        [](const RaceResult& first, const RaceResult& second) {
            return first.place < second.place;
        });
    results_ = std::move(results);
}

const RaceResult* RaceLifecycle::GetResult(
    std::size_t playerId) const noexcept
{
    const auto found = std::find_if(
        results_.begin(), results_.end(),
        [playerId](const RaceResult& result) {
            return result.playerId == playerId;
        });
    return found == results_.end() ? nullptr : &*found;
}

const std::vector<RaceResult>& RaceLifecycle::GetResults() const noexcept
{
    return results_;
}

} // namespace r3d::game::originalrace::source
