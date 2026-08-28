#include "OriginalRaceLifecycle.h"
#include "OriginalPlayer.h"
#include "OriginalRace.h"

#include <array>
#include <algorithm>
#include <vector>

namespace source = r3d::game::originalrace::source;

int main()
{
    source::GameModeRaceState gameMode;
    gameMode.Reset(false);
    if (gameMode.CountdownStage() != source::GameModeRaceState::goRaceWait ||
        gameMode.CountdownSeconds() != 4.0F || gameMode.IsRaceGo())
        return 12;
    if (gameMode.OnFrame(0.99F).countdownStage)
        return 13;
    auto gameAdvance = gameMode.OnFrame(0.02F);
    if (!gameAdvance.countdownStage ||
        *gameAdvance.countdownStage != source::GameModeRaceState::goRace1)
        return 14;
    gameMode.Pause(true);
    if (!gameMode.IsPaused() ||
        gameMode.OnFrame(2.0F).countdownStage ||
        gameMode.CountdownStage() != source::GameModeRaceState::goRace1)
        return 15;
    gameMode.Pause(false);
    if (gameMode.IsPaused())
        return 22;
    gameMode.OnFrame(0.99F);
    if (gameMode.CountdownStage() != source::GameModeRaceState::goRace2)
        return 16;
    const auto external = gameMode.SynchronizeCountdown(
        source::GameModeRaceState::goRaceWait);
    if (!external || gameMode.CountdownSeconds() != 3.0F ||
        gameMode.OnFrame(5.0F).countdownStage)
        return 17;
    const auto externalGo = gameMode.SynchronizeCountdown(
        source::GameModeRaceState::goRace);
    if (!externalGo || !externalGo->raceStarted || !gameMode.IsRaceGo())
        return 18;
    gameMode.RunFinishTimer();
    if (gameMode.OnFrame(3.0F).finishTimeEnded ||
        gameMode.IsFinishPresentationReady())
        return 19;
    if (!gameMode.OnFrame(0.01F).finishTimeEnded ||
        !gameMode.IsFinishPresentationReady())
        return 20;
    gameMode.Reset(true);
    if (!gameMode.IsRaceGo() || gameMode.CountdownSeconds() != 0.0F)
        return 21;

    std::array<source::Player, 2U> runPlayers{};
    std::array<r3d::game::originalrace::WeaponDefinition, 1U>
        runWeaponDefinitions{};
    runWeaponDefinitions[0].maximumCharge = 4U;
    runWeaponDefinitions[0].reloadCharge = 4U;
    for (auto& player : runPlayers)
    {
        player.Reset(80.0F, 1U);
        player.CreateCar(true);
        player.SetFinished(true, 1.0F);
        player.weaponSlots[0] = 0U;
        player.weaponCapacity[0] = 4U;
        player.weaponCharges[0] = 1U;
        player.BindWeaponItems(runWeaponDefinitions);
    }
    runPlayers.front().SetId(source::Player::humanId);
    runPlayers.back().SetId(1);
    source::RaceRunState run;
    if (!run.StartRace(runPlayers, &runPlayers.front(), true, 1U) ||
        !run.IsStartRace() || run.IsRaceGo() ||
        !runPlayers.front().IsBlock() ||
        runPlayers.back().IsBlock() ||
        runPlayers.front().GetFinished() ||
        runPlayers.back().GetFinished() ||
        runPlayers.front().GetHeadLight() !=
            source::Player::HeadLightMode::Two ||
        runPlayers.back().GetHeadLight() !=
            source::Player::HeadLightMode::One ||
        runPlayers.front().GetPrimaryWeaponItems()[0]
                ->GetCurCharge() != 4U ||
        runPlayers.back().GetPrimaryWeaponItems()[0]
                ->GetCurCharge() != 4U ||
        run.StartRace(runPlayers, &runPlayers.front(), true, 1U))
        return 9;
    run.GoRace(&runPlayers.front());
    if (!run.IsRaceGo() || runPlayers.front().IsBlock())
        return 10;
    runPlayers.front().car.numLaps = 2U;
    runPlayers.back().car.numLaps = 3U;
    if (!run.ExitRace(runPlayers) || run.IsStartRace() || run.IsRaceGo() ||
        runPlayers.front().car.numLaps != 0U ||
        runPlayers.back().car.numLaps != 0U ||
        runPlayers.front().GetHeadLight() !=
            source::Player::HeadLightMode::None ||
        runPlayers.back().GetHeadLight() !=
            source::Player::HeadLightMode::None ||
        run.ExitRace(runPlayers))
        return 11;

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

    source::RacePlaceModel places;
    std::vector<source::RacePlacePlayer> placePlayers(3U);
    for (std::size_t index = 0U; index < placePlayers.size(); ++index)
    {
        placePlayers[index].playerId = index;
        placePlayers[index].lastCorrectMainPath = true;
        placePlayers[index].lastCorrectPathLength = 100.0F;
    }
    placePlayers[0].finished = true;
    placePlayers[0].place = 2U;
    placePlayers[0].lap = 100.0F;
    placePlayers[1].finished = true;
    placePlayers[1].place = 1U;
    placePlayers[1].lap = 0.0F;
    placePlayers[2].finished = true;
    placePlayers[2].place = 3U;
    placePlayers[2].lap = 200.0F;
    auto placeUpdate = places.Update(placePlayers, true);
    if (placeUpdate.order != std::vector<std::size_t>{1U, 0U, 2U})
        return 6;

    places.Reset();
    for (auto& player : placePlayers)
    {
        player.finished = false;
        player.place = 0U;
    }
    placePlayers[0].lap = placePlayers[0].lastCorrectLap = 3.9F;
    placePlayers[1].lap = placePlayers[1].lastCorrectLap = 2.0F;
    placePlayers[2].lap = placePlayers[2].lastCorrectLap = 1.0F;
    places.Update(placePlayers, false);
    placePlayers[1].lap = placePlayers[1].lastCorrectLap = 4.0F;
    placeUpdate = places.Update(placePlayers, false);
    const auto leadChanged = std::find_if(
        placeUpdate.events.begin(), placeUpdate.events.end(),
        [](const source::RacePlaceEvent& event) {
            return event.kind ==
                       source::RacePlaceEventKind::LeadChanged &&
                   event.playerId == 1U && event.otherPlayerId == 0U;
        });
    if (leadChanged == placeUpdate.events.end())
        return 7;

    // Race::DelPlayer clears _playerPlaceList. A changed active roster must
    // not report a synthetic lead swap against the disconnected tombstone.
    placePlayers[1].disconnected = true;
    placePlayers[2].lap = placePlayers[2].lastCorrectLap = 8.0F;
    placeUpdate = places.Update(placePlayers, false);
    if (std::any_of(
            placeUpdate.events.begin(), placeUpdate.events.end(),
            [](const source::RacePlaceEvent& event) {
                return event.kind ==
                       source::RacePlaceEventKind::LeadChanged;
            }))
        return 8;

    places.Reset();
    source::WorldEventPump lateWorld;
    lateWorld.RegLateProgressEvent(&places);
    for (auto& player : placePlayers)
    {
        player.disconnected = false;
        player.finished = false;
    }
    placePlayers[0].lap = 1.0F;
    placePlayers[1].lap = 3.0F;
    placePlayers[2].lap = 2.0F;
    places.PrepareLateProgress(placePlayers, false);
    if (!places.HasPreparedLateProgress() ||
        !places.TakeLateProgressUpdate().order.empty())
        return 22;
    lateWorld.LateProgress(1.0F / 60.0F, true);
    if (places.HasPreparedLateProgress())
        return 23;
    placeUpdate = places.TakeLateProgressUpdate();
    if (placeUpdate.order != std::vector<std::size_t>{1U, 2U, 0U} ||
        !places.TakeLateProgressUpdate().order.empty())
        return 24;
    lateWorld.UnregLateProgressEvent(&places);

    return 0;
}
