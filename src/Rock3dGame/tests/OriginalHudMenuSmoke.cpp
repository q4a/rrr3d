#include "OriginalHudMenu.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace source = r3d::game::originalrace::source;

namespace
{

bool near(float first, float second, float tolerance = 0.001F)
{
    return std::abs(first - second) <= tolerance;
}

bool point(source::HudPoint value, float x, float y)
{
    return near(value.x, x) && near(value.y, y);
}

} // namespace

int main()
{
    source::HudMenu hud;
    if (hud.GetState() != source::HudMenuState::Main ||
        !hud.IsMiniMapVisible() || !hud.IsPlayerStateVisible())
        return 1;

    if (!point(source::HudMenu::GetMiniMapRect().size, 320.0F, 320.0F) ||
        !point(source::HudMenu::GetWeaponPos(), 155.0F, 50.0F) ||
        !point(source::HudMenu::GetWeaponBoxPos(), 5.0F, -15.0F) ||
        !point(source::HudMenu::GetWeaponLabelPos(), -10.0F, 26.0F) ||
        !point(source::HudMenu::GetWeaponPosMine(), 30.0F, 140.0F) ||
        !point(source::HudMenu::GetWeaponPosMineLabel(), 105.0F, 159.0F) ||
        !point(source::HudMenu::GetWeaponPosHyper(), 30.0F, 32.0F) ||
        !point(source::HudMenu::GetWeaponPosHyperLabel(), 105.0F, 15.0F) ||
        !point(source::HudMenu::GetPlacePos(), 105.0F, 88.0F) ||
        !point(source::HudMenu::GetLapPos(), 0.0F, 200.0F) ||
        !point(source::HudMenu::GetLifeBarPos(), 165.0F, 0.0F) ||
        !point(source::HudMenu::GetPickItemsPos(), 0.0F, 255.0F) ||
        !point(source::HudMenu::GetAchievmentItemsPos(1920.0F),
               1010.0F, 15.0F) ||
        !point(source::HudMenu::GetCarLifeBarPos(), 4.0F, -10.0F))
        return 2;

    if (hud.OnHandleInput(false, false, false) !=
            source::HudMenuCommand::None ||
        hud.OnHandleInput(true, true, false) !=
            source::HudMenuCommand::None ||
        hud.OnHandleInput(true, false, false) !=
            source::HudMenuCommand::ShowExitConfirmation ||
        hud.OnHandleInput(true, false, true) !=
            source::HudMenuCommand::HideExitConfirmation)
        return 3;

    for (int image = 0; image < 4; ++image)
    {
        hud.OnCountdownEvent(image);
        hud.OnProgress(2.0F);
        const auto& visual = hud.GetCountdownVisual();
        if (visual.image != image || !near(visual.alpha, 1.0F) ||
            !near(visual.growthSeconds, 0.0F))
            return 4;
    }

    hud.OnCountdownEvent(4);
    hud.OnProgress(0.75F);
    if (hud.GetCountdownVisual().image != 4 ||
        !near(hud.GetCountdownVisual().alpha, 0.5F) ||
        !near(hud.GetCountdownVisual().growthSeconds, 0.75F))
        return 5;
    hud.OnProgress(0.74F);
    if (hud.GetCountdownVisual().image != 4 ||
        hud.GetCountdownVisual().alpha <= 0.0F)
        return 6;
    hud.OnProgress(0.02F);
    if (hud.GetCountdownVisual().image != -1 ||
        !near(hud.GetCountdownVisual().alpha, 0.0F))
        return 7;

    hud.OnCountdownEvent(4);
    hud.Reset();
    if (hud.GetCountdownVisual().image != -1 ||
        !hud.IsMiniMapVisible() || !hud.IsPlayerStateVisible())
        return 8;

    source::MiniMapFrame miniMap;
    r3d::game::originalrace::Race race;
    race.tracePoints = {
        {1U, {0.0F, 0.0F, 0.0F}, 10.0F},
        {2U, {100.0F, 0.0F, 0.0F}, 10.0F},
        {3U, {100.0F, 100.0F, 0.0F}, 12.0F},
        {4U, {0.0F, 100.0F, 0.0F}, 12.0F}};
    race.tracePath = {1U, 2U, 3U, 4U};
    race.tracePaths = {race.tracePath};
    if (!miniMap.Build(race, 1920.0F) || !miniMap.IsValid())
        return 9;
    const auto& geometry = miniMap.GetGeometry();
    if (geometry.vertices.size() < 8U ||
        geometry.vertices.size() % 2U != 0U ||
        geometry.indices.empty() || geometry.indices.size() % 6U != 0U ||
        !near(geometry.startHeight, geometry.startWidth * 2.0F) ||
        !near(geometry.startAngle, 0.0F))
        return 10;
    const auto first = miniMap.MapPosition({0.0F, 0.0F, 0.0F});
    const auto second = miniMap.MapPosition({100.0F, 0.0F, 0.0F});
    const auto fourth = miniMap.MapPosition({0.0F, 100.0F, 0.0F});
    if (!point(first, geometry.start.x, geometry.start.y) ||
        second.x <= first.x || fourth.y >= first.y)
        return 11;
    miniMap.UpdateLap(3U, 4U);
    if (miniMap.GetShownLap() != 4U || miniMap.GetTotalLaps() != 4U)
        return 40;
    miniMap.UpdateLap(8U, 4U);
    if (miniMap.GetShownLap() != 4U)
        return 41;
    std::vector<source::HudMiniMapPlayerInput> miniMapPlayers{
        {0U, {0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F, 1.0F}},
        {1U, {100.0F, 100.0F, 0.0F},
         {0.0F, 1.0F, 0.0F, 1.0F}}};
    miniMap.UpdatePlayers(miniMapPlayers);
    if (miniMap.GetPlayers().size() != 2U ||
        miniMap.GetPlayers()[0].racer != 0U ||
        miniMap.GetPlayers()[1].racer != 1U ||
        !point(miniMap.GetPlayers()[0].position, first.x, first.y) ||
        !point(miniMap.GetPlayers()[1].position, second.x, fourth.y))
        return 47;
    miniMapPlayers.erase(miniMapPlayers.begin());
    miniMapPlayers[0].mapPosition = {0.0F, 0.0F, 0.0F};
    miniMapPlayers[0].color = {0.0F, 0.0F, 1.0F, 1.0F};
    miniMap.UpdatePlayers(miniMapPlayers);
    const auto* remainingPlayer = miniMap.FindPlayer(1U);
    if (miniMap.FindPlayer(0U) != nullptr || remainingPlayer == nullptr ||
        miniMap.GetPlayers().size() != 1U ||
        !point(remainingPlayer->position, first.x, first.y) ||
        !near(remainingPlayer->color[2], 1.0F))
        return 48;
    miniMap.Clear();
    if (miniMap.IsValid() || !miniMap.GetGeometry().vertices.empty() ||
        !miniMap.GetGeometry().indices.empty() ||
        miniMap.GetShownLap() != 0U || miniMap.GetTotalLaps() != 0U ||
        !miniMap.GetPlayers().empty())
        return 12;

    source::PlayerStateFrame playerState;
    const auto pick = playerState.NewPickItem(100.0F, 0.0F);
    playerState.OnProgress(0.1F, 0.0F);
    const auto* pickState = playerState.FindPickItem(pick);
    if (pickState == nullptr || !near(pickState->position.x, 59.0F) ||
        !near(pickState->position.y, 255.0F) ||
        !near(pickState->alpha, 0.0F))
        return 13;
    playerState.OnProgress(0.15F, 0.15F);
    pickState = playerState.FindPickItem(pick);
    if (pickState == nullptr || !near(pickState->position.x, 72.5F) ||
        !near(pickState->alpha, 0.5F))
        return 14;
    playerState.OnProgress(0.1F, 4.8F);
    pickState = playerState.FindPickItem(pick);
    if (pickState == nullptr || !near(pickState->alpha, 2.0F / 3.0F) ||
        pickState->position.y <= 255.0F)
        return 15;
    playerState.OnProgress(0.2F, 5.0F);
    if (playerState.FindPickItem(pick) != nullptr ||
        !playerState.GetPickItems().empty())
        return 16;

    const auto firstAchievment = playerState.NewAchievment(
        100.0F, 60.0F, 40.0F, 1920.0F, 1080.0F, 0.0F, 0U);
    auto* achievmentState =
        playerState.FindAchievmentItem(firstAchievment);
    if (achievmentState == nullptr ||
        !point(achievmentState->position, -200.0F, 270.0F))
        return 17;
    playerState.OnProgress(0.15F, 0.15F);
    achievmentState =
        playerState.FindAchievmentItem(firstAchievment);
    if (achievmentState == nullptr ||
        !point(achievmentState->position, 405.0F, 152.5F) ||
        !near(achievmentState->pointsAlpha, 0.0F))
        return 18;
    playerState.OnProgress(0.15F, 0.3F);
    achievmentState =
        playerState.FindAchievmentItem(firstAchievment);
    if (achievmentState == nullptr ||
        !point(achievmentState->position, 1010.0F, 35.0F) ||
        !near(achievmentState->scale, 2.0F))
        return 19;
    playerState.OnProgress(0.1F, 0.4F);
    if (!near(playerState.FindAchievmentItem(firstAchievment)->scale,
              1.0F))
        return 20;
    playerState.OnProgress(0.475F, 0.875F);
    if (!near(
            playerState.FindAchievmentItem(firstAchievment)->pointsAlpha,
            0.5F))
        return 21;
    const auto secondAchievment = playerState.NewAchievment(
        100.0F, 60.0F, 40.0F, 1920.0F, 1080.0F, 1.0F, 7U);
    playerState.OnProgress(0.075F, 1.075F);
    if (playerState.GetAchievmentItems().size() != 2U ||
        playerState.GetAchievmentItems().front().id != secondAchievment ||
        playerState.FindAchievmentItem(secondAchievment)->indexTime < 0.0F ||
        playerState.FindAchievmentItem(firstAchievment)->indexTime < 0.0F)
        return 22;
    playerState.OnProgress(0.075F, 1.15F);
    if (playerState.FindAchievmentItem(secondAchievment)->indexTime >= 0.0F ||
        playerState.FindAchievmentItem(firstAchievment)->indexTime >= 0.0F)
        return 23;
    playerState.OnProgress(0.1F, 5.0F);
    if (playerState.FindAchievmentItem(firstAchievment) != nullptr ||
        playerState.FindAchievmentItem(secondAchievment) == nullptr)
        return 24;
    playerState.Reset();
    if (!playerState.GetPickItems().empty() ||
        !playerState.GetAchievmentItems().empty())
        return 25;

    playerState.ShowCarLife(0U, 2U, 1.0F);
    if (!playerState.HasCarLife(2U) ||
        playerState.GetCarLifeItems()[0].barAlpha != 0.0F ||
        playerState.GetCarLifeItems()[0].backgroundAlpha != 0.0F)
        return 26;
    source::HudCarLifeInput carLifeInput;
    carLifeInput.projected = {100.0F, 100.0F};
    carLifeInput.life = 0.6F;
    carLifeInput.viewportWidth = 200.0F;
    carLifeInput.viewportHeight = 120.0F;
    carLifeInput.backWidth = 40.0F;
    carLifeInput.backHeight = 20.0F;
    carLifeInput.targetAlive = true;
    playerState.ProgressCarLife(0U, carLifeInput, 0.15F);
    const auto& carLifeHalf = playerState.GetCarLifeItems()[0];
    if (!carLifeHalf.visible ||
        !point(carLifeHalf.position, 120.0F, 90.0F) ||
        !near(carLifeHalf.life, 0.6F) ||
        !near(carLifeHalf.backgroundAlpha, 0.5F) ||
        !near(carLifeHalf.barAlpha, 0.5F))
        return 27;
    playerState.ProgressCarLife(0U, carLifeInput, 0.15F);
    if (!near(
            playerState.GetCarLifeItems()[0].backgroundAlpha, 1.0F) ||
        !near(playerState.GetCarLifeItems()[0].barAlpha, 1.0F))
        return 28;
    playerState.ShowCarLife(0U, 2U, 1.0F);
    if (!near(
            playerState.GetCarLifeItems()[0].backgroundAlpha, 1.0F) ||
        !near(playerState.GetCarLifeItems()[0].barAlpha, 0.0F))
        return 29;
    carLifeInput.atEdge = true;
    playerState.ProgressCarLife(0U, carLifeInput, 0.15F);
    if (!near(
            playerState.GetCarLifeItems()[0].backgroundAlpha, 0.5F) ||
        !near(playerState.GetCarLifeItems()[0].barAlpha, 0.0F))
        return 30;
    carLifeInput.targetAlive = false;
    playerState.ProgressCarLife(0U, carLifeInput, 0.01F);
    if (playerState.GetCarLifeItems()[0].visible ||
        playerState.HasCarLife(2U))
        return 31;

    auto opponentInput = [](std::size_t racer, int place,
                            float x, float y) {
        source::HudOpponentInput input;
        input.racer = racer;
        input.place = place;
        input.projected = {x, y};
        input.viewportWidth = 200.0F;
        input.viewportHeight = 100.0F;
        input.pointWidth = 20.0F;
        input.pointHeight = 10.0F;
        input.carLifeBackWidth = 40.0F;
        input.carLifeBackHeight = 15.0F;
        input.labelWidth = 20.0F;
        input.labelHeight = 10.0F;
        input.labelAabbMinY = -5.0F;
        input.targetAlive = true;
        return input;
    };
    std::vector<source::HudOpponentInput> opponentInputs{
        opponentInput(1U, 1, 20.0F, 60.0F),
        opponentInput(2U, 3, 100.0F, 60.0F),
        opponentInput(3U, 2, 180.0F, 60.0F)};
    playerState.ProgressOpponents(opponentInputs, 0.1F);
    const auto& sortedOpponents = playerState.GetOpponents();
    if (sortedOpponents.size() != 3U ||
        sortedOpponents[0].racer != 2U ||
        sortedOpponents[1].racer != 3U ||
        sortedOpponents[2].racer != 1U)
        return 32;
    const auto* firstOpponent = playerState.FindOpponent(1U);
    if (firstOpponent == nullptr ||
        !point(firstOpponent->pointPosition, 30.0F, 55.0F) ||
        !point(firstOpponent->labelPosition, 30.0F, 50.0F) ||
        !near(firstOpponent->radius, 20.0F) ||
        !near(firstOpponent->alpha, 1.0F))
        return 33;

    playerState.ShowCarLife(0U, 1U, 1.0F);
    playerState.ProgressOpponents(opponentInputs, 0.1F);
    if (playerState.GetOpponents().front().racer != 1U ||
        !near(playerState.GetOpponents().front().radius, 40.0F) ||
        !near(playerState.GetOpponents().front().alpha, 0.6F))
        return 34;

    opponentInputs[1].atEdge = true;
    playerState.ProgressOpponents(opponentInputs, 0.1F);
    if (!near(playerState.FindOpponent(2U)->alpha, 0.6F))
        return 35;
    opponentInputs[1].atEdge = false;
    opponentInputs[2].projected = opponentInputs[1].projected;
    playerState.ProgressOpponents(opponentInputs, 0.0F);
    if (!near(playerState.FindOpponent(2U)->alpha, 1.0F) ||
        !near(playerState.FindOpponent(3U)->alpha, 0.0F))
        return 36;

    opponentInputs[1].targetAlive = false;
    playerState.ProgressOpponents(opponentInputs, 0.1F);
    if (playerState.FindOpponent(2U)->visible ||
        !near(playerState.FindOpponent(2U)->alpha, 1.0F))
        return 37;
    opponentInputs.erase(opponentInputs.begin() + 1);
    playerState.ProgressOpponents(opponentInputs, 0.1F);
    if (playerState.FindOpponent(2U) != nullptr ||
        playerState.GetOpponents().size() != 2U)
        return 38;
    playerState.Reset();
    if (!playerState.GetOpponents().empty())
        return 39;

    source::HudRaceStateInput raceStateInput;
    raceStateInput.place = 3U;
    raceStateInput.life = 25.0F;
    raceStateInput.maximumLife = 100.0F;
    raceStateInput.carAlive = true;
    raceStateInput.selectedPrimarySlot = 2U;
    raceStateInput.primaryBoxWidth = 100.0F;
    raceStateInput.primaryBoxHeight = 76.0F;
    raceStateInput.weapons[0] = {7U, 2U, 3U, true};
    raceStateInput.weapons[2] = {4U, 5U, 10U, true};
    raceStateInput.weapons[4] = {6U, 8U, 12U, true};
    playerState.UpdateRaceState(raceStateInput);
    const auto& raceState = playerState.GetRaceState();
    if (raceState.place != 3U || !near(raceState.life, 0.25F) ||
        !raceState.weapons[0].visible ||
        !point(raceState.weapons[0].viewPosition, 30.0F, 32.0F) ||
        !point(raceState.weapons[0].labelPosition, 105.0F, 15.0F))
        return 42;
    if (!raceState.weapons[2].visible ||
        raceState.weapons[2].selected ||
        !point(raceState.weapons[2].boxPosition, 205.0F, 88.0F) ||
        !point(raceState.weapons[2].viewPosition, 210.0F, 73.0F) ||
        !point(raceState.weapons[2].labelPosition, 195.0F, 114.0F))
        return 43;
    if (!raceState.weapons[4].visible ||
        !raceState.weapons[4].selected ||
        !point(raceState.weapons[4].boxPosition, 280.0F, 88.0F) ||
        raceState.weapons[4].visual != 6U ||
        raceState.weapons[4].currentCharge != 8U ||
        raceState.weapons[4].totalCharge != 12U)
        return 44;

    raceStateInput.weapons[0] = {};
    raceStateInput.weapons[1] = {9U, 1U, 4U, true};
    raceStateInput.carAlive = false;
    raceStateInput.life = 0.0F;
    playerState.UpdateRaceState(raceStateInput);
    if (playerState.GetRaceState().weapons[0].visible ||
        !playerState.GetRaceState().weapons[1].visible ||
        !point(playerState.GetRaceState().weapons[1].viewPosition,
               30.0F, 32.0F) ||
        !point(playerState.GetRaceState().weapons[1].labelPosition,
               105.0F, 15.0F) ||
        !near(playerState.GetRaceState().life, 0.25F))
        return 45;
    playerState.Reset();
    if (playerState.GetRaceState().place != 1U ||
        !near(playerState.GetRaceState().life, 1.0F) ||
        std::any_of(
            playerState.GetRaceState().weapons.begin(),
            playerState.GetRaceState().weapons.end(),
            [](const source::HudWeaponSlot& weapon) {
                return weapon.visible;
            }))
        return 46;

    if (source::PlayerStateFrame::ResolvePickVisual(
            r3d::game::originalrace::BonusKind::Medpack,
            source::HudPickSlot::None) !=
            source::HudPickVisual::Armor ||
        source::PlayerStateFrame::ResolvePickVisual(
            r3d::game::originalrace::BonusKind::Ammunition,
            source::HudPickSlot::Mine) !=
            source::HudPickVisual::Mine ||
        source::PlayerStateFrame::ResolvePickVisual(
            r3d::game::originalrace::BonusKind::Speed,
            source::HudPickSlot::Primary) !=
            source::HudPickVisual::None)
        return 49;

    source::PlayerStateFrame eventState;
    source::HudPlayerEventInput hudEvent;
    hudEvent.kind = source::HudPlayerEventKind::Pick;
    hudEvent.pickVisual = source::HudPickVisual::Money;
    hudEvent.human = 0U;
    hudEvent.player = 1U;
    hudEvent.itemWidth = 100.0F;
    if (eventState.ProcessEvent(hudEvent).kind !=
        source::HudPlayerEventKind::None)
        return 50;
    hudEvent.player = 0U;
    const auto pickEvent = eventState.ProcessEvent(hudEvent);
    if (pickEvent.kind != source::HudPlayerEventKind::Pick ||
        pickEvent.pickVisual != source::HudPickVisual::Money ||
        eventState.FindPickItem(pickEvent.item) == nullptr)
        return 51;

    hudEvent = {};
    hudEvent.kind = source::HudPlayerEventKind::Achievement;
    hudEvent.slotWidth = 100.0F;
    hudEvent.slotHeight = 60.0F;
    hudEvent.imageHeight = 40.0F;
    hudEvent.viewportWidth = 1920.0F;
    hudEvent.viewportHeight = 1080.0F;
    const auto achievementEvent = eventState.ProcessEvent(hudEvent);
    if (achievementEvent.kind !=
            source::HudPlayerEventKind::Achievement ||
        eventState.FindAchievmentItem(achievementEvent.item) == nullptr)
        return 52;

    hudEvent = {};
    hudEvent.kind = source::HudPlayerEventKind::Damage;
    hudEvent.human = 0U;
    hudEvent.player = 1U;
    hudEvent.target = 0U;
    hudEvent.value = 5.0F;
    hudEvent.targetAvailable = true;
    eventState.ProcessEvent(hudEvent);
    if (eventState.GetCarLifeItems()[1].racer != 0U ||
        !near(eventState.GetCarLifeItems()[1].timeMax, 1.5F))
        return 53;
    hudEvent.player = 0U;
    hudEvent.target = 2U;
    eventState.ProcessEvent(hudEvent);
    if (eventState.GetCarLifeItems()[0].racer != 2U ||
        !near(eventState.GetCarLifeItems()[0].timeMax, 4.0F))
        return 54;

    hudEvent = {};
    hudEvent.kind = source::HudPlayerEventKind::Kill;
    hudEvent.human = 0U;
    hudEvent.player = 0U;
    hudEvent.target = 2U;
    hudEvent.targetAvailable = true;
    hudEvent.killCredit = true;
    hudEvent.itemWidth = 120.0F;
    const auto killEvent = eventState.ProcessEvent(hudEvent);
    if (killEvent.kind != source::HudPlayerEventKind::Kill ||
        killEvent.pickVisual != source::HudPickVisual::Kill ||
        killEvent.target != 2U ||
        eventState.FindPickItem(killEvent.item) == nullptr)
        return 55;
    hudEvent.kind = source::HudPlayerEventKind::Countdown;
    hudEvent.countdownImage = 4;
    const auto countdownEvent = eventState.ProcessEvent(hudEvent);
    if (countdownEvent.kind != source::HudPlayerEventKind::Countdown ||
        countdownEvent.countdownImage != 4)
        return 56;

    std::cout << "Original HudMenu smoke passed\n";
    return 0;
}
