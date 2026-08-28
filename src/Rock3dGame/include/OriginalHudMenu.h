#pragma once

#include "OriginalRace.h"

#include <array>
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

struct HudMiniMapPlayerInput
{
    std::size_t racer = static_cast<std::size_t>(-1);
    Vec3 mapPosition;
    std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
};

struct HudMiniMapPlayer
{
    std::size_t racer = static_cast<std::size_t>(-1);
    HudPoint position;
    std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
};

// Source MiniMapFrame::BuildPath/UpdateMap owner. It retains the aligned and
// smoothed road strip, world-to-map transform and original start marker;
// bgfx only converts these vertices to its upload format.
class MiniMapFrame
{
public:
    bool Build(const Race& race, float viewportWidth);
    void UpdateLap(std::uint32_t completedLaps,
                   std::uint32_t totalLaps) noexcept;
    void UpdatePlayers(
        const std::vector<HudMiniMapPlayerInput>& inputs) noexcept;
    void Clear() noexcept;
    HudPoint MapPosition(Vec3 position) const noexcept;
    const HudMiniMapGeometry& GetGeometry() const noexcept;
    std::uint32_t GetShownLap() const noexcept;
    std::uint32_t GetTotalLaps() const noexcept;
    const std::vector<HudMiniMapPlayer>& GetPlayers() const noexcept;
    const HudMiniMapPlayer* FindPlayer(std::size_t racer) const noexcept;
    bool IsValid() const noexcept;

private:
    HudMiniMapGeometry geometry_;
    float minimumX_ = 0.0F;
    float minimumY_ = 0.0F;
    float maximumY_ = 0.0F;
    float scale_ = 1.0F;
    float originX_ = 0.0F;
    float originY_ = 0.0F;
    std::uint32_t shownLap_ = 0U;
    std::uint32_t totalLaps_ = 0U;
    std::vector<HudMiniMapPlayer> players_;
    bool valid_ = false;
};

using HudItemId = std::uint64_t;

enum class HudPickSlot : std::uint8_t
{
    None,
    Primary,
    Hyper,
    Mine,
};

enum class HudPickVisual : std::uint8_t
{
    None,
    Armor,
    Weapon,
    Hyper,
    Mine,
    Money,
    Immortal,
    Kill,
};

enum class HudPlayerEventKind : std::uint8_t
{
    None,
    Pick,
    Achievement,
    Damage,
    Kill,
    Countdown,
};

struct HudPlayerEventInput
{
    HudPlayerEventKind kind = HudPlayerEventKind::None;
    HudPickVisual pickVisual = HudPickVisual::None;
    std::size_t player = static_cast<std::size_t>(-1);
    std::size_t target = static_cast<std::size_t>(-1);
    std::size_t human = static_cast<std::size_t>(-1);
    float value = 0.0F;
    float itemWidth = 0.0F;
    float slotWidth = 0.0F;
    float slotHeight = 0.0F;
    float imageHeight = 0.0F;
    float viewportWidth = 0.0F;
    float viewportHeight = 0.0F;
    float now = 0.0F;
    int countdownImage = -1;
    bool targetAvailable = false;
    bool killCredit = true;
};

struct HudPlayerEventResult
{
    HudPlayerEventKind kind = HudPlayerEventKind::None;
    HudPickVisual pickVisual = HudPickVisual::None;
    HudItemId item = 0U;
    std::size_t target = static_cast<std::size_t>(-1);
    int countdownImage = -1;
};

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

struct HudCarLifeInput
{
    HudPoint projected;
    float life = 1.0F;
    float viewportWidth = 0.0F;
    float viewportHeight = 0.0F;
    float backWidth = 0.0F;
    float backHeight = 0.0F;
    bool targetAlive = false;
    bool atEdge = false;
};

struct HudCarLife
{
    static constexpr std::size_t invalidRacer =
        static_cast<std::size_t>(-1);

    std::size_t racer = invalidRacer;
    HudPoint position;
    float timer = -1.0F;
    float timeMax = 4.0F;
    float life = 1.0F;
    float backgroundAlpha = 0.0F;
    float barAlpha = 1.0F;
    bool visible = false;
};

struct HudOpponentInput
{
    std::size_t racer = HudCarLife::invalidRacer;
    int place = 0;
    HudPoint projected;
    float viewportWidth = 0.0F;
    float viewportHeight = 0.0F;
    float pointWidth = 0.0F;
    float pointHeight = 0.0F;
    float carLifeBackWidth = 0.0F;
    float carLifeBackHeight = 0.0F;
    float labelWidth = 0.0F;
    float labelHeight = 0.0F;
    float labelAabbMinY = 0.0F;
    bool targetAlive = false;
    bool atEdge = false;
};

struct HudOpponent
{
    std::size_t racer = HudCarLife::invalidRacer;
    int place = 0;
    HudPoint pointPosition;
    HudPoint labelPosition;
    HudPoint center;
    float radius = 0.0F;
    float alpha = 1.0F;
    bool visible = false;
};

struct HudWeaponSlotInput
{
    std::size_t visual = HudCarLife::invalidRacer;
    std::uint32_t currentCharge = 0U;
    std::uint32_t totalCharge = 0U;
    bool mounted = false;
};

struct HudRaceStateInput
{
    static constexpr std::size_t weaponTypeCount = 6U;

    std::array<HudWeaponSlotInput, weaponTypeCount> weapons{};
    std::size_t selectedPrimarySlot = 0U;
    std::uint32_t place = 1U;
    float life = 1.0F;
    float maximumLife = 1.0F;
    float primaryBoxWidth = 0.0F;
    float primaryBoxHeight = 0.0F;
    bool carAlive = false;
};

struct HudWeaponSlot
{
    std::size_t type = 0U;
    std::size_t visual = HudCarLife::invalidRacer;
    HudPoint boxPosition;
    HudPoint viewPosition;
    HudPoint labelPosition;
    std::uint32_t currentCharge = 0U;
    std::uint32_t totalCharge = 0U;
    bool visible = false;
    bool primary = false;
    bool selected = false;
};

struct HudPlayerRaceState
{
    std::array<HudWeaponSlot, HudRaceStateInput::weaponTypeCount>
        weapons{};
    std::uint32_t place = 1U;
    float life = 1.0F;
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
    HudPlayerEventResult ProcessEvent(
        const HudPlayerEventInput& input);
    static HudPickVisual ResolvePickVisual(
        BonusKind kind, HudPickSlot slot) noexcept;
    void OnProgress(float deltaTime, float now);
    void ShowCarLife(
        std::size_t slot, std::size_t racer, float timeMax) noexcept;
    void ProgressCarLife(
        std::size_t slot, const HudCarLifeInput& input,
        float deltaTime) noexcept;
    void ProgressOpponents(
        const std::vector<HudOpponentInput>& inputs,
        float deltaTime) noexcept;
    void UpdateRaceState(const HudRaceStateInput& input) noexcept;
    void Reset() noexcept;

    const std::vector<HudPickItem>& GetPickItems() const noexcept;
    const std::vector<HudAchievmentItem>&
    GetAchievmentItems() const noexcept;
    const HudPickItem* FindPickItem(HudItemId id) const noexcept;
    const HudAchievmentItem* FindAchievmentItem(
        HudItemId id) const noexcept;
    const std::array<HudCarLife, 2>& GetCarLifeItems() const noexcept;
    bool HasCarLife(std::size_t racer) const noexcept;
    const std::vector<HudOpponent>& GetOpponents() const noexcept;
    const HudOpponent* FindOpponent(std::size_t racer) const noexcept;
    const HudPlayerRaceState& GetRaceState() const noexcept;

private:
    HudItemId nextId_ = 1U;
    std::vector<HudPickItem> pickItems_;
    std::vector<HudAchievmentItem> achievmentItems_;
    std::array<HudCarLife, 2> carLifeItems_{};
    std::vector<HudOpponent> opponents_;
    HudPlayerRaceState raceState_;
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
