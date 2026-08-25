#include "OriginalRaceLifecycle.h"

#include "OriginalPlayer.h"

#include <algorithm>

namespace r3d::game::originalrace::source
{

void RaceRunState::Reset() noexcept
{
    startRace_ = false;
    goRace_ = false;
}

bool RaceRunState::StartRace(
    std::span<Player> players, Player* human) noexcept
{
    if (startRace_)
        return false;
    startRace_ = true;
    goRace_ = false;
    for (auto& player : players)
    {
        player.SetFinished(false);
        player.ResetBlock(false);
    }
    if (human != nullptr)
        human->ResetBlock(true);
    return true;
}

void RaceRunState::GoRace(Player* human) noexcept
{
    goRace_ = true;
    if (human != nullptr)
        human->ResetBlock(false);
}

bool RaceRunState::ExitRace() noexcept
{
    goRace_ = false;
    if (!startRace_)
        return false;
    startRace_ = false;
    return true;
}

bool RaceRunState::IsStartRace() const noexcept
{
    return startRace_;
}

bool RaceRunState::IsRaceGo() const noexcept
{
    return goRace_;
}

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

void RacePlaceModel::Reset() noexcept
{
    order_.clear();
    lastLeadPlace_ = 0.0F;
    lastThirdPlace_ = 0.0F;
}

RacePlaceUpdate RacePlaceModel::Update(
    const std::vector<RacePlacePlayer>& players, bool hasResults)
{
    RacePlaceUpdate output;
    std::vector<std::size_t> activeIds;
    activeIds.reserve(players.size());
    for (const auto& player : players)
    {
        if (!player.disconnected)
            activeIds.push_back(player.playerId);
    }

    // Race::DelPlayer clears _playerPlaceList. The portable roster retains a
    // disconnected tombstone for stable renderer/network indices, so detect
    // the equivalent membership change here.
    if (!order_.empty())
    {
        auto previousIds = order_;
        std::sort(previousIds.begin(), previousIds.end());
        auto currentIds = activeIds;
        std::sort(currentIds.begin(), currentIds.end());
        if (previousIds != currentIds)
            order_.clear();
    }

    const auto findPlayer = [&](std::size_t id) -> const RacePlacePlayer* {
        const auto found = std::find_if(
            players.begin(), players.end(),
            [id](const RacePlacePlayer& player) {
                return player.playerId == id && !player.disconnected;
            });
        return found == players.end() ? nullptr : &*found;
    };
    const RacePlacePlayer* lastLeader =
        order_.empty() ? nullptr : findPlayer(order_.front());
    const RacePlacePlayer* lastThird =
        order_.size() < 3U ? nullptr : findPlayer(order_[2U]);

    output.order = std::move(activeIds);
    std::sort(
        output.order.begin(), output.order.end(),
        [&](std::size_t firstId, std::size_t secondId) {
            const auto* first = findPlayer(firstId);
            const auto* second = findPlayer(secondId);
            if (first == nullptr || second == nullptr)
                return first != nullptr;
            if (first->finished && second->finished)
                return first->place < second->place;
            if (first->finished != second->finished)
                return first->finished;
            return first->lap > second->lap;
        });

    const auto at = [&](std::size_t index) -> const RacePlacePlayer* {
        return index < output.order.size()
                   ? findPlayer(output.order[index])
                   : nullptr;
    };
    const auto* leader = at(0U);
    const auto* second = at(1U);
    const auto* third = at(2U);
    const auto* nextLast =
        output.order.size() >= 2U
            ? at(output.order.size() - 2U)
            : nullptr;
    const auto* last =
        output.order.size() >= 2U
            ? at(output.order.size() - 1U)
            : nullptr;

    if (leader != nullptr && lastLeader != nullptr &&
        leader->playerId != lastLeader->playerId &&
        leader->lastCorrectMainPath &&
        lastLeader->lastCorrectMainPath)
    {
        const float newLeadPlace = leader->lastCorrectLap;
        if (leader->lastCorrectPathLength *
                    (newLeadPlace - lastLeadPlace_) >
                300.0F &&
            !hasResults)
        {
            output.events.push_back(
                {RacePlaceEventKind::LeadChanged, leader->playerId,
                 lastLeader->playerId});
        }
        lastLeadPlace_ = newLeadPlace;
    }
    if (third != nullptr && lastThird != nullptr &&
        third->playerId != lastThird->playerId &&
        third->lastCorrectMainPath &&
        lastThird->lastCorrectMainPath)
    {
        const float newThirdPlace = third->lastCorrectLap;
        if (third->lastCorrectPathLength *
                    (newThirdPlace - lastThirdPlace_) >
                300.0F &&
            !hasResults)
        {
            output.events.push_back(
                {RacePlaceEventKind::ThirdChanged, third->playerId,
                 lastThird->playerId});
        }
        lastThirdPlace_ = newThirdPlace;
    }
    if (last != nullptr && nextLast != nullptr &&
        last->lastCorrectMainPath && nextLast->lastCorrectMainPath &&
        last->lastCorrectPathLength *
                (nextLast->lastCorrectLap - last->lastCorrectLap) >
            70.0F)
    {
        output.events.push_back(
            {RacePlaceEventKind::LastFar, last->playerId,
             nextLast->playerId});
    }
    if (leader != nullptr && second != nullptr &&
        leader->lastCorrectMainPath && second->lastCorrectMainPath &&
        leader->lastCorrectPathLength *
                (leader->lastCorrectLap - second->lastCorrectLap) >
            70.0F &&
        !hasResults)
    {
        output.events.push_back(
            {RacePlaceEventKind::Domination, leader->playerId,
             second->playerId});
    }
    if (second != nullptr && third != nullptr &&
        second->lastCorrectMainPath && third->lastCorrectMainPath &&
        third->lastCorrectPathLength *
                (second->lastCorrectLap - third->lastCorrectLap) >
            70.0F &&
        !hasResults)
    {
        output.events.push_back(
            {RacePlaceEventKind::ThirdFar, third->playerId,
             second->playerId});
    }

    order_ = output.order;
    return output;
}

} // namespace r3d::game::originalrace::source
