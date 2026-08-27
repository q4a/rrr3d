#pragma once

#include "OriginalRace.h"

#include <cstddef>
#include <cstdint>
#include <vector>

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

struct HudMiniMapVertex
{
    float x = 0.0F;
    float y = 0.0F;
    float u = 0.0F;
    float v = 0.0F;
};

struct HudMiniMapGeometry
{
    std::vector<HudMiniMapVertex> vertices;
    std::vector<std::uint16_t> indices;
    HudPoint start;
    float startAngle = 0.0F;
    float startWidth = 0.0F;
    float startHeight = 0.0F;
};

// Source MiniMapFrame::BuildPath/UpdateMap owner. It retains the aligned and
// smoothed road strip, world-to-map transform and original start marker;
// bgfx only converts these vertices to its upload format.
class MiniMapFrame
{
public:
    bool Build(const Race& race, float viewportWidth);
    void Clear() noexcept;
    HudPoint MapPosition(Vec3 position) const noexcept;
    const HudMiniMapGeometry& GetGeometry() const noexcept;
    bool IsValid() const noexcept;

private:
    HudMiniMapGeometry geometry_;
    float minimumX_ = 0.0F;
    float minimumY_ = 0.0F;
    float maximumY_ = 0.0F;
    float scale_ = 1.0F;
    float originX_ = 0.0F;
    float originY_ = 0.0F;
    bool valid_ = false;
};

using HudItemId = std::uint64_t;

struct HudPickItem
{
    HudItemId id = 0U;
    float started = 0.0F;
    HudPoint position;
    float targetX = 0.0F;
    float alpha = 0.0F;
};

struct HudAchievmentItem
{
    HudItemId id = 0U;
    float started = 0.0F;
    HudPoint position;
    float targetX = 0.0F;
    float slotHeight = 0.0F;
    float imageHeight = 0.0F;
    float alpha = 1.0F;
    float pointsAlpha = 0.0F;
    float scale = 1.0F;
    float lastIndex = 0.0F;
    float indexTime = -1.0F;
};

// Source PlayerStateFrame notification queues. GPU images/text remain view
// resources keyed by HudItemId; ordering, lifetime and all motion/fade state
// are owned here.
class PlayerStateFrame
{
public:
    static constexpr std::size_t randomAchievmentPosition = 8U;

    HudItemId NewPickItem(float imageWidth, float now);
    HudItemId NewAchievment(
        float slotWidth, float slotHeight, float imageHeight,
        float viewportWidth, float viewportHeight, float now,
        std::size_t startPosition = randomAchievmentPosition);
    void OnProgress(float deltaTime, float now);
    void Reset() noexcept;

    const std::vector<HudPickItem>& GetPickItems() const noexcept;
    const std::vector<HudAchievmentItem>&
    GetAchievmentItems() const noexcept;
    const HudPickItem* FindPickItem(HudItemId id) const noexcept;
    const HudAchievmentItem* FindAchievmentItem(
        HudItemId id) const noexcept;

private:
    HudItemId nextId_ = 1U;
    std::vector<HudPickItem> pickItems_;
    std::vector<HudAchievmentItem> achievmentItems_;
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
