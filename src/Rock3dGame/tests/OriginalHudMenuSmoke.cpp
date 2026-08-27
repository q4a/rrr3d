#include "OriginalHudMenu.h"

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
    miniMap.Clear();
    if (miniMap.IsValid() || !miniMap.GetGeometry().vertices.empty() ||
        !miniMap.GetGeometry().indices.empty())
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

    std::cout << "Original HudMenu smoke passed\n";
    return 0;
}
