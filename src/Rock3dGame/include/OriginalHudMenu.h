#pragma once

#include <cstdint>

namespace r3d::game::originalrace::source
{

struct HudPoint
{
    float x = 0.0F;
    float y = 0.0F;
};

struct HudRect
{
    HudPoint size;
};

enum class HudMenuState : std::uint8_t
{
    Main,
};

// Backend command produced by HudMenu::OnHandleInput. The SDL host performs
// cursor/dialog/pause operations; the decision remains source-owned.
enum class HudMenuCommand : std::uint8_t
{
    None,
    ShowExitConfirmation,
    HideExitConfirmation,
};

struct HudCountdownVisual
{
    int image = -1;
    float alpha = 1.0F;
    float growthSeconds = 0.0F;
};

// Backend-neutral owner transcribed from HudMenu/PlayerStateFrame. Widget
// creation is intentionally left at the bgfx boundary, while state,
// visibility, layout and countdown animation live in this source class.
class HudMenu
{
public:
    void Reset() noexcept;

    HudMenuState GetState() const noexcept;
    void SetState(HudMenuState value) noexcept;
    bool IsMiniMapVisible() const noexcept;
    bool IsPlayerStateVisible() const noexcept;

    HudMenuCommand OnHandleInput(
        bool escapeDown, bool repeated, bool paused) const noexcept;

    void OnCountdownEvent(int image) noexcept;
    void OnProgress(float deltaTime) noexcept;
    const HudCountdownVisual& GetCountdownVisual() const noexcept;

    static HudRect GetMiniMapRect() noexcept;
    static HudPoint GetWeaponPos() noexcept;
    static HudPoint GetWeaponBoxPos() noexcept;
    static HudPoint GetWeaponLabelPos() noexcept;
    static HudPoint GetWeaponPosMine() noexcept;
    static HudPoint GetWeaponPosMineLabel() noexcept;
    static HudPoint GetWeaponPosHyper() noexcept;
    static HudPoint GetWeaponPosHyperLabel() noexcept;
    static HudPoint GetPlacePos() noexcept;
    static HudPoint GetLapPos() noexcept;
    static HudPoint GetLifeBarPos() noexcept;
    static HudPoint GetPickItemsPos() noexcept;
    static HudPoint GetAchievmentItemsPos(float viewportWidth) noexcept;
    static HudPoint GetCarLifeBarPos() noexcept;

private:
    void applyState() noexcept;

    HudMenuState state_ = HudMenuState::Main;
    HudCountdownVisual countdown_;
    bool miniMapVisible_ = true;
    bool playerStateVisible_ = true;
};

} // namespace r3d::game::originalrace::source
