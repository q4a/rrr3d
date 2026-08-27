#include "OriginalHudMenu.h"

#include <algorithm>

namespace r3d::game::originalrace::source
{

void HudMenu::Reset() noexcept
{
    state_ = HudMenuState::Main;
    countdown_ = {};
    applyState();
}

HudMenuState HudMenu::GetState() const noexcept
{
    return state_;
}

void HudMenu::SetState(HudMenuState value) noexcept
{
    if (state_ == value)
        return;
    state_ = value;
    applyState();
}

bool HudMenu::IsMiniMapVisible() const noexcept
{
    return miniMapVisible_;
}

bool HudMenu::IsPlayerStateVisible() const noexcept
{
    return playerStateVisible_;
}

HudMenuCommand HudMenu::OnHandleInput(
    bool escapeDown, bool repeated, bool paused) const noexcept
{
    if (!escapeDown || repeated)
        return HudMenuCommand::None;
    return paused ? HudMenuCommand::HideExitConfirmation
                  : HudMenuCommand::ShowExitConfirmation;
}

void HudMenu::OnCountdownEvent(int image) noexcept
{
    if (image < 0 || image > 4)
        return;
    countdown_.image = image;
    countdown_.alpha = 1.0F;
    countdown_.growthSeconds = 0.0F;
}

void HudMenu::OnProgress(float deltaTime) noexcept
{
    if (countdown_.image != 4)
        return;
    deltaTime = std::max(deltaTime, 0.0F);
    countdown_.alpha -= deltaTime / 1.5F;
    if (countdown_.alpha > 0.0F)
    {
        countdown_.growthSeconds += deltaTime;
        return;
    }
    countdown_.image = -1;
    countdown_.alpha = 0.0F;
}

const HudCountdownVisual& HudMenu::GetCountdownVisual() const noexcept
{
    return countdown_;
}

HudRect HudMenu::GetMiniMapRect() noexcept
{
    return {{320.0F, 320.0F}};
}

HudPoint HudMenu::GetWeaponPos() noexcept
{
    return {155.0F, 50.0F};
}

HudPoint HudMenu::GetWeaponBoxPos() noexcept
{
    return {5.0F, -15.0F};
}

HudPoint HudMenu::GetWeaponLabelPos() noexcept
{
    return {-10.0F, 26.0F};
}

HudPoint HudMenu::GetWeaponPosMine() noexcept
{
    return {30.0F, 140.0F};
}

HudPoint HudMenu::GetWeaponPosMineLabel() noexcept
{
    return {105.0F, 159.0F};
}

HudPoint HudMenu::GetWeaponPosHyper() noexcept
{
    return {30.0F, 32.0F};
}

HudPoint HudMenu::GetWeaponPosHyperLabel() noexcept
{
    return {105.0F, 15.0F};
}

HudPoint HudMenu::GetPlacePos() noexcept
{
    return {105.0F, 88.0F};
}

HudPoint HudMenu::GetLapPos() noexcept
{
    return {0.0F, 200.0F};
}

HudPoint HudMenu::GetLifeBarPos() noexcept
{
    return {165.0F, 0.0F};
}

HudPoint HudMenu::GetPickItemsPos() noexcept
{
    return {0.0F, 255.0F};
}

HudPoint HudMenu::GetAchievmentItemsPos(float viewportWidth) noexcept
{
    return {(100.0F + viewportWidth) * 0.5F, 15.0F};
}

HudPoint HudMenu::GetCarLifeBarPos() noexcept
{
    return {4.0F, -10.0F};
}

void HudMenu::applyState() noexcept
{
    const bool main = state_ == HudMenuState::Main;
    miniMapVisible_ = main;
    playerStateVisible_ = main;
}

} // namespace r3d::game::originalrace::source
