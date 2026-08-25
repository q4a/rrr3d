#include "OriginalAchievmentModel.h"

#include <algorithm>
#include <vector>

namespace original = r3d::game::originalrace;
namespace source = r3d::game::originalrace::source;

namespace
{

original::AchievementDefinition definition(
    std::uint32_t classId, const char* name)
{
    original::AchievementDefinition result;
    result.classId = classId;
    result.name = name;
    result.reward = 10U;
    result.iterationCount = 1U;
    return result;
}

bool completed(
    const std::vector<std::size_t>& values,
    std::size_t index = 0U)
{
    return std::find(values.begin(), values.end(), index) != values.end();
}

} // namespace

int main()
{
    source::AchievmentRaceState raceState;
    raceState.lapCount = 3U;
    raceState.playerCount = 4U;

    {
        auto item = definition(1U, "bonus");
        item.iterationCount = 2U;
        item.bonusKind = original::BonusKind::Money;
        const std::vector<original::AchievementDefinition> definitions{item};
        source::AchievmentModel model;
        model.Configure(&definitions, 100U, {{"bonus", 1U}}, 1.5F);
        const source::AchievmentEvent event{
            source::AchievmentEventKind::Bonus, true, 0U,
            source::GameObject::undefinedPlayerId, 0.0F,
            original::DamageType::Simple,
            original::BonusKind::Money, 1U};
        if (!completed(model.Process(0.0F, {&event, 1U}, raceState)) ||
            model.GetPoints() != 115U ||
            model.GetIterations().at("bonus") != 0U)
            return 1;
    }

    {
        auto item = definition(2U, "speedKill");
        item.killsNumber = 2U;
        item.killsTime = 0.5F;
        const std::vector<original::AchievementDefinition> definitions{item};
        source::AchievmentModel model;
        model.Configure(&definitions, 0U, {}, 1.0F);
        const source::AchievmentEvent kill{
            source::AchievmentEventKind::Kill, true, 0U, 1U};
        if (completed(model.Process(0.0F, {&kill, 1U}, raceState)) ||
            completed(model.Process(0.51F, {}, raceState)) ||
            completed(model.Process(0.0F, {&kill, 1U}, raceState)))
            return 2;
        if (!completed(model.Process(0.0F, {&kill, 1U}, raceState)))
            return 3;
    }

    {
        auto item = definition(3U, "raceKill");
        item.killsNumber = 2U;
        const std::vector<original::AchievementDefinition> definitions{item};
        source::AchievmentModel model;
        model.Configure(&definitions, 0U, {}, 1.0F);
        const std::vector<source::AchievmentEvent> kills{
            {source::AchievmentEventKind::Kill, true, 0U, 1U},
            {source::AchievmentEventKind::Kill, true, 0U, 2U}};
        if (!completed(model.Process(0.0F, kills, raceState)))
            return 4;
    }

    {
        const std::vector<original::AchievementDefinition> definitions{
            definition(4U, "lapPass")};
        source::AchievmentModel model;
        model.Configure(&definitions, 0U, {}, 1.0F);
        const source::AchievmentEvent lap{
            source::AchievmentEventKind::Lap, true, 0U};
        raceState.humanPlace = 1U;
        for (std::uint32_t lapIndex = 1U; lapIndex <= 3U; ++lapIndex)
        {
            raceState.humanLaps = lapIndex;
            if (completed(model.Process(0.0F, {&lap, 1U}, raceState)))
                return 5;
        }
        const source::AchievmentEvent finish{
            source::AchievmentEventKind::RaceFinish, false};
        // Active Windows Race::OnLapPass passes NULL EventData, and the
        // source condition's common guard rejects this completion branch.
        if (completed(model.Process(0.0F, {&finish, 1U}, raceState)))
            return 6;
    }

    {
        const std::vector<original::AchievementDefinition> definitions{
            definition(5U, "dodge"),
            definition(7U, "survival")};
        source::AchievmentModel model;
        model.Configure(&definitions, 0U, {}, 1.0F);
        source::AchievmentEvent lap{
            source::AchievmentEventKind::Lap, true, 0U};
        raceState.humanLaps = 1U;
        auto result = model.Process(0.0F, {&lap, 1U}, raceState);
        if (!completed(result, 0U) || completed(result, 1U))
            return 7;
        raceState.humanLaps = raceState.lapCount - 1U;
        result = model.Process(0.0F, {&lap, 1U}, raceState);
        if (!completed(result, 1U))
            return 8;
    }

    {
        const std::vector<original::AchievementDefinition> definitions{
            definition(6U, "lapBreak")};
        source::AchievmentModel model;
        model.Configure(&definitions, 0U, {}, 1.0F);
        const source::AchievmentEvent lap{
            source::AchievmentEventKind::Lap, true, 0U};
        raceState.humanPlace = 4U;
        raceState.humanLaps = 1U;
        model.Process(0.0F, {&lap, 1U}, raceState);
        raceState.humanPlace = 1U;
        raceState.humanLaps = raceState.lapCount;
        if (!completed(model.Process(0.0F, {&lap, 1U}, raceState)))
            return 9;
    }

    {
        const std::vector<original::AchievementDefinition> definitions{
            definition(8U, "firstKill"),
            definition(9U, "touchKill")};
        source::AchievmentModel model;
        model.Configure(&definitions, 0U, {}, 1.0F);
        const source::AchievmentEvent kill{
            source::AchievmentEventKind::Kill, true, 0U, 1U};
        auto result = model.Process(0.0F, {&kill, 1U}, raceState);
        if (!completed(result, 0U))
            return 10;
        const source::AchievmentEvent death{
            source::AchievmentEventKind::Death, true, 2U, 0U, 0.0F,
            original::DamageType::DeathPlane};
        result = model.Process(0.0F, {&death, 1U}, raceState);
        if (!completed(result, 1U))
            return 11;
    }

    return 0;
}
