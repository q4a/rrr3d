#include "OriginalAchievmentModel.h"

#include <algorithm>
#include <cmath>

namespace r3d::game::originalrace::source
{

void AchievmentModel::Configure(
    const std::vector<AchievementDefinition>* definitions,
    std::uint32_t initialPoints,
    const Iterations& initialIterations,
    float difficultyMultiplier) noexcept
{
    definitions_ = definitions;
    points_ = initialPoints;
    difficultyMultiplier_ = difficultyMultiplier;
    conditions_.assign(
        definitions_ == nullptr ? 0U : definitions_->size(), {});
    if (definitions_ != nullptr)
    {
        for (std::size_t index = 0U;
             index < definitions_->size(); ++index)
        {
            const auto found = initialIterations.find(
                (*definitions_)[index].name);
            if (found != initialIterations.end())
                conditions_[index].iteration = found->second;
        }
    }
    ResetRaceState();
}

void AchievmentModel::ResetRaceState() noexcept
{
    for (auto& condition : conditions_)
    {
        condition.counter = 0U;
        condition.total = 0U;
        condition.timer = 0.0F;
    }
    globalKills_ = 0U;
    raceStarted_ = true;
    raceFinished_ = false;
}

void AchievmentModel::SetCampaign(bool value) noexcept
{
    campaign_ = value;
}

bool AchievmentModel::CompleteIteration(
    std::size_t condition,
    std::vector<std::size_t>& completed) noexcept
{
    if (!raceStarted_ || raceFinished_ || definitions_ == nullptr ||
        condition >= definitions_->size() ||
        condition >= conditions_.size())
        return false;
    const auto& definition = (*definitions_)[condition];
    auto& state = conditions_[condition];
    if (++state.iteration < std::max(definition.iterationCount, 1U))
        return false;
    state.iteration = 0U;
    if (campaign_)
    {
        points_ += static_cast<std::uint32_t>(std::floor(
            static_cast<float>(definition.reward) *
            difficultyMultiplier_));
    }
    completed.push_back(condition);
    return true;
}

std::vector<std::size_t> AchievmentModel::Process(
    float deltaTime, std::span<const AchievmentEvent> events,
    const AchievmentRaceState& raceState) noexcept
{
    std::vector<std::size_t> completed;
    if (definitions_ == nullptr)
        return completed;

    // SpeedKill is the only source condition registered for progress.
    for (std::size_t index = 0U; index < definitions_->size(); ++index)
    {
        if ((*definitions_)[index].classId != 2U)
            continue;
        auto& state = conditions_[index];
        if (state.timer > 0.0F &&
            (state.timer -= deltaTime) <= 0.0F)
        {
            state.timer = 0.0F;
            state.counter = 0U;
        }
    }

    for (const auto& event : events)
    {
        const bool humanEvent =
            event.hasPlayer && event.playerId == 0U;
        const bool humanKill =
            event.kind == AchievmentEventKind::Kill && humanEvent;
        for (std::size_t index = 0U;
             index < definitions_->size(); ++index)
        {
            const auto& definition = (*definitions_)[index];
            auto& state = conditions_[index];
            switch (definition.classId)
            {
            case 1U: // AchievmentConditionBonus
                if (event.kind == AchievmentEventKind::Bonus &&
                    event.bonusKind == definition.bonusKind)
                {
                    if (state.total == 0U)
                        state.total = event.bonusTotalCount;
                    if (humanEvent && state.total > 0U &&
                        ++state.counter >= state.total)
                    {
                        CompleteIteration(index, completed);
                        state.counter = 0U;
                    }
                }
                break;
            case 2U: // AchievmentConditionSpeedKill
                if (humanKill)
                {
                    if (++state.counter >=
                        std::max(definition.killsNumber, 1U))
                    {
                        state.counter = 0U;
                        state.timer = 0.0F;
                        CompleteIteration(index, completed);
                    }
                    else
                    {
                        state.timer = definition.killsTime;
                    }
                }
                break;
            case 3U: // AchievmentConditionRaceKill
                if (humanKill &&
                    ++state.counter >=
                        std::max(definition.killsNumber, 1U))
                {
                    state.counter = 0U;
                    CompleteIteration(index, completed);
                }
                break;
            case 4U: // AchievmentConditionLapPass
                if (!humanEvent)
                    break;
                if (event.kind == AchievmentEventKind::Lap &&
                    raceState.humanPlace == 1U)
                    ++state.counter;
                else if (event.kind == AchievmentEventKind::RaceFinish &&
                         state.counter >= raceState.lapCount)
                    CompleteIteration(index, completed);
                break;
            case 5U: // AchievmentConditionDodge
                if (!humanEvent)
                    break;
                if (event.kind == AchievmentEventKind::Death ||
                    (event.kind == AchievmentEventKind::Damage &&
                     event.value > 0.0F))
                    ++state.counter;
                else if (event.kind == AchievmentEventKind::Lap)
                {
                    if (state.counter == 0U &&
                        raceState.humanLaps == 1U)
                        CompleteIteration(index, completed);
                    state.counter = 0U;
                }
                break;
            case 6U: // AchievmentConditionLapBreak
                if (humanEvent && event.kind == AchievmentEventKind::Lap)
                {
                    const auto oldPlace = static_cast<int>(state.counter);
                    const auto newPlace =
                        static_cast<int>(raceState.humanPlace);
                    if (raceState.humanLaps >= raceState.lapCount &&
                        oldPlace - newPlace >=
                            static_cast<int>(raceState.playerCount) - 1)
                        CompleteIteration(index, completed);
                    state.counter = raceState.humanPlace;
                }
                break;
            case 7U: // AchievmentConditionSurvival
                if (!humanEvent)
                    break;
                if (event.kind == AchievmentEventKind::Death)
                    ++state.counter;
                else if (event.kind == AchievmentEventKind::Lap &&
                         raceState.humanLaps == raceState.lapCount - 1U &&
                         state.counter == 0U)
                    CompleteIteration(index, completed);
                break;
            case 8U: // AchievmentConditionFirstKill
                if (event.kind == AchievmentEventKind::Kill)
                {
                    if (humanEvent && globalKills_ == 0U)
                        CompleteIteration(index, completed);
                }
                break;
            case 9U: // AchievmentConditionTouchKill
                if (event.kind == AchievmentEventKind::Death &&
                    event.playerId != 0U &&
                    event.targetPlayerId == 0U &&
                    (event.damageType == DamageType::Touch ||
                     event.damageType == DamageType::DeathPlane))
                    CompleteIteration(index, completed);
                break;
            default:
                break;
            }
        }
        if (event.kind == AchievmentEventKind::Kill)
            ++globalKills_;
        // Race.cpp sends cRaceFinish with NULL EventData. Conditions see the
        // event first; GameMode then enters its finish state for future work.
        if (event.kind == AchievmentEventKind::RaceFinish)
            raceFinished_ = true;
    }
    return completed;
}

std::uint32_t AchievmentModel::GetPoints() const noexcept
{
    return points_;
}

AchievmentModel::Iterations AchievmentModel::GetIterations() const
{
    Iterations result;
    if (definitions_ == nullptr)
        return result;
    for (std::size_t index = 0U;
         index < definitions_->size() && index < conditions_.size(); ++index)
    {
        result[(*definitions_)[index].name] =
            conditions_[index].iteration;
    }
    return result;
}

} // namespace r3d::game::originalrace::source
