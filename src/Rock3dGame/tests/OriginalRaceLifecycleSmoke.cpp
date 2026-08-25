#include "OriginalRaceLifecycle.h"

#include <array>
#include <vector>

namespace source = r3d::game::originalrace::source;

int main()
{
    const std::array<std::uint32_t, 3> money{100U, 60U, 30U};
    const std::array<std::uint32_t, 3> points{10U, 6U, 3U};
    source::RaceLifecycle race;

    source::RaceLifecyclePlayer human;
    human.playerId = 0U;
    human.human = true;
    human.laps = 2U;
    human.pickedMoney = 7U;
    auto lap = race.OnLapPass(human, 3U, 6U, true, 2U, money, points);
    if (lap.completed || lap.events.size() != 2U ||
        lap.events[0].kind != source::RaceLifecycleEventKind::PassLap ||
        lap.events[1].kind != source::RaceLifecycleEventKind::LastLap ||
        lap.events[1].playerId != 2U)
        return 1;

    source::RaceLifecyclePlayer computer;
    computer.playerId = 1U;
    computer.laps = 3U;
    computer.pickedMoney = 4U;
    lap = race.OnLapPass(computer, 3U, 6U, true, 0U, money, points);
    if (!lap.completed || lap.completed->place != 1U ||
        lap.completed->money != 100U ||
        lap.completed->pickedMoney != 4U || lap.events.size() != 1U ||
        lap.events[0].kind != source::RaceLifecycleEventKind::LeadFinish)
        return 2;

    human.laps = 3U;
    lap = race.OnLapPass(human, 3U, 6U, true, 0U, money, points);
    if (!lap.completed || lap.completed->place != 2U ||
        lap.events.size() != 3U ||
        lap.events[0].kind != source::RaceLifecycleEventKind::SecondFinish ||
        lap.events[1].kind != source::RaceLifecycleEventKind::RaceFinish ||
        lap.events[2].kind != source::RaceLifecycleEventKind::PassLap)
        return 3;

    race.Reset();
    std::vector<source::RaceLifecyclePlayer> remaining(3U);
    remaining[0].playerId = 0U;
    remaining[0].human = true;
    remaining[0].lapPosition = 2.8F;
    remaining[1].playerId = 1U;
    remaining[1].lapPosition = 1.1F;
    remaining[2].playerId = 2U;
    remaining[2].lapPosition = 0.7F;
    const auto completed =
        race.CompleteRemaining(remaining, 3U, money, points);
    if (completed.size() != 3U || completed[0].playerId != 1U ||
        completed[1].playerId != 2U || completed[2].playerId != 0U ||
        completed[2].place != 3U)
        return 4;

    race.Reset();
    source::RaceLifecyclePlayer firstAi;
    firstAi.playerId = 4U;
    firstAi.laps = 1U;
    auto first = race.OnLapPass(firstAi, 1U, 2U, false, 4U, money, points);
    source::RaceLifecyclePlayer lastAi = firstAi;
    lastAi.playerId = 5U;
    auto last = race.OnLapPass(lastAi, 1U, 2U, false, 5U, money, points);
    if (first.events.size() != 1U || last.events.size() != 2U ||
        last.events[0].kind != source::RaceLifecycleEventKind::SecondFinish ||
        last.events[1].kind != source::RaceLifecycleEventKind::RaceFinish)
        return 5;

    return 0;
}
