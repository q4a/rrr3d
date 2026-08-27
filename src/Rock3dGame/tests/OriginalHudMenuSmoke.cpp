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

    std::cout << "Original HudMenu smoke passed\n";
    return 0;
}
