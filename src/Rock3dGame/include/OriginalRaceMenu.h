#pragma once

#include "InputActions.h"
#include "OriginalGarage.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace r3d::game::originalracemenu
{

enum class State : std::uint8_t
{
    Main,
    Garage,
    Workshop,
    Gamers,
    Angar,
    Achievements,
    Count,
};

struct Visibility
{
    bool car = false;
    bool spaceship = false;
    bool main = false;
    bool garage = false;
    bool workshop = false;
    bool gamers = false;
    bool angar = false;
    bool achievements = false;
};

class RaceMenuState
{
public:
    RaceMenuState() noexcept;

    State state() const noexcept;
    State lastState() const noexcept;
    const Visibility& visibility() const noexcept;
    bool setState(State state) noexcept;

private:
    void applyState() noexcept;

    State state_ = State::Main;
    State lastState_ = State::Count;
    Visibility visibility_{};
};

enum class RaceMainCommand : std::uint8_t
{
    StartRace,
    Workshop,
    Garage,
    Angar,
    Achievements,
    Options,
    Exit,
};

// Exact widget origins produced by RaceMainFrame::OnAdjustLayout.  The
// Windows GUI keeps a widget's origin separate from the alignment offset of
// its texture; children are positioned from these origins, not from the
// centre of the panel image.
struct RaceMainLayout
{
    float topOriginX = 0.0F;
    float topOriginY = 0.0F;
    float bottomOriginX = 0.0F;
    float bottomOriginY = 0.0F;
    float statsOriginX = 0.0F;
    float statsOriginY = 0.0F;
    float moneyOriginX = 0.0F;
    float moneyOriginY = 0.0F;
};

class RaceMainFrameState
{
public:
    static constexpr std::size_t itemCount = 7U;

    RaceMainFrameState();

    void show(bool clientRaceReady) noexcept;
    void invalidate(bool clientRaceReady) noexcept;
    const std::array<bool, itemCount>& enabledItems() const noexcept;
    bool enabled(std::size_t item) const noexcept;
    std::size_t firstEnabled() const noexcept;
    std::size_t moveSelection(std::size_t selection,
                              int direction) const noexcept;
    std::optional<RaceMainCommand> command(
        std::size_t selection) const noexcept;

    float itemX(float viewportWidth, std::size_t item,
                float itemWidth = 110.0F,
                float spacing = 50.0F) const noexcept;
    float itemY(float viewportHeight, float itemHeight = 94.0F) const noexcept;
    RaceMainLayout layout(float viewportWidth, float viewportHeight,
                          float bottomPanelHeight) const noexcept;

private:
    std::array<bool, itemCount> enabled_{};
};

struct GamerEntry
{
    std::uint32_t gamerId = 0U;
    bool available = false;
};

enum class GamerFocus : std::uint8_t
{
    Next,
    Left,
    Right,
};

enum class GamerCommandType : std::uint8_t
{
    SelectionChanged,
    Confirm,
};

struct GamerCommand
{
    GamerCommandType type = GamerCommandType::SelectionChanged;
    std::size_t index = 0U;
    std::uint32_t gamerId = 0U;
};

struct GamerLayout
{
    float planetRadius = 0.0F;
    float planetX = 0.0F;
    float planetY = 0.0F;
    float viewportSize = 0.0F;
    float leftX = 0.0F;
    float rightX = 0.0F;
    float nextX = 0.0F;
    float nextY = 0.0F;
};

class GamersFrameState
{
public:
    void show(std::vector<GamerEntry> entries,
              std::uint32_t currentGamerId);
    void updateEntries(std::vector<GamerEntry> entries);

    std::size_t selection() const noexcept;
    std::optional<std::uint32_t> selectedGamerId() const noexcept;
    GamerFocus focus() const noexcept;
    void setFocus(GamerFocus focus) noexcept;
    bool available(std::size_t index) const noexcept;
    std::optional<std::size_t> previous() const noexcept;
    std::optional<std::size_t> next() const noexcept;
    std::optional<GamerCommand> select(std::size_t index) noexcept;
    std::optional<GamerCommand> handle(
        const rrr3d::input::ActionEvent& event) noexcept;

    GamerLayout layout(float viewportWidth, float viewportHeight,
                       float bottomPanelHeight, float arrowWidth,
                       float nextArrowWidth) const noexcept;

private:
    std::optional<std::size_t> adjacent(std::size_t from,
                                        int direction) const noexcept;

    std::vector<GamerEntry> entries_;
    std::size_t selection_ = 0U;
    GamerFocus focus_ = GamerFocus::Next;
};

struct GarageCarCandidate
{
    std::size_t catalogIndex = 0U;
    bool secret = false;
    bool owned = false;
    bool achievementUnlocked = false;
};

struct GarageCarEntry
{
    std::size_t catalogIndex = 0U;
    bool locked = false;
};

enum class GarageCommandType : std::uint8_t
{
    SelectionChanged,
    Back,
    BuyOrSelect,
    SelectColor,
};

struct GarageCommand
{
    GarageCommandType type = GarageCommandType::SelectionChanged;
    std::size_t catalogIndex = 0U;
    std::size_t colorIndex = 0U;
};

struct GarageVisibleRange
{
    std::size_t first = 0U;
    std::size_t count = 0U;
    float firstCenterX = 0.0F;
};

struct GarageLayout
{
    float topPanelX = 0.0F;
    float topPanelY = 0.0F;
    float bottomPanelX = 0.0F;
    float bottomPanelY = 0.0F;
    float leftPanelX = 0.0F;
    float rightPanelX = 0.0F;
    float sidePanelY = 0.0F;
    float statsOriginX = 0.0F;
    float statsOriginY = 0.0F;
    float moneyOriginX = 0.0F;
    float moneyOriginY = 0.0F;
    float leftArrowX = 0.0F;
    float rightArrowX = 0.0F;
    float arrowY = 0.0F;
    float backX = 0.0F;
    float backY = 0.0F;
    float buyX = 0.0F;
    float buyY = 0.0F;
};

class GarageFrameState
{
public:
    static constexpr std::size_t colorCount = 14U;
    static constexpr std::size_t focusCount = 4U + colorCount;

    void show(std::vector<GarageCarCandidate> candidates,
              std::size_t currentCatalogIndex, bool campaign,
              const std::array<bool, colorCount>& colorsAvailable);
    void hide() noexcept;
    void updateColors(
        const std::array<bool, colorCount>& colorsAvailable) noexcept;

    const std::vector<GarageCarEntry>& cars() const noexcept;
    bool empty() const noexcept;
    std::size_t selection() const noexcept;
    const GarageCarEntry* selectedCar() const noexcept;
    bool canPrevious() const noexcept;
    bool canNext() const noexcept;
    bool colorAvailable(std::size_t colorIndex) const noexcept;

    std::size_t focus() const noexcept;
    bool setFocus(std::size_t focus) noexcept;
    std::optional<GarageCommand> select(std::size_t selection) noexcept;
    std::optional<GarageCommand> handle(
        const rrr3d::input::ActionEvent& event) noexcept;

    GarageVisibleRange visibleRange(float viewportWidth,
                                    float carCellWidth) noexcept;
    GarageLayout layout(float viewportWidth, float viewportHeight,
                         float topPanelHeight, float bottomPanelHeight,
                         float leftPanelWidth,
                         float rightPanelWidth) const noexcept;

private:
    bool focusAvailable(std::size_t focus) const noexcept;
    std::size_t neighbor(std::size_t focus,
                         rrr3d::input::Action action) const noexcept;

    std::vector<GarageCarEntry> cars_;
    std::array<bool, colorCount> colorsAvailable_{};
    std::size_t selection_ = 0U;
    std::size_t focus_ = 0U;
    std::size_t visibleFirst_ = 0U;
    std::size_t visibleCount_ = 0U;
    std::size_t lastVisibleSelection_ =
        static_cast<std::size_t>(-1);
};

struct WorkshopGoodCandidate
{
    std::size_t catalogIndex = 0U;
    std::uint32_t cost = 0U;
    bool mobilityFamily = false;
    bool unlocked = false;
};

struct WorkshopGoodEntry
{
    std::size_t catalogIndex = 0U;
    std::uint32_t cost = 0U;
};

struct WorkshopSlotState
{
    bool active = false;
    bool locked = false;
    bool installed = false;
    bool chargeControlVisible = false;
    bool levelControlVisible = false;
    bool controlEnabled = false;
};

struct WorkshopDragState
{
    originalrace::ProfileSlot item;
    std::optional<originalrace::GarageSlotType> origin;

    bool active() const noexcept
    {
        return !item.record.empty();
    }
};

enum class WorkshopConfirmationType : std::uint8_t
{
    None,
    Buy,
    Sell,
};

struct WorkshopConfirmationState
{
    WorkshopConfirmationType type = WorkshopConfirmationType::None;
    std::size_t pendingCatalogIndex = static_cast<std::size_t>(-1);
    bool yesFocused = true;
};

enum class WorkshopCommandType : std::uint8_t
{
    Back,
    ActivateGood,
    ActivateSlot,
};

struct WorkshopCommand
{
    WorkshopCommandType type = WorkshopCommandType::Back;
    std::size_t index = 0U;
    bool pointerSlotPlane = false;
};

struct WorkshopLayout
{
    std::array<std::array<float, 2>, 12U> goods{};
    std::array<
        std::array<float, 2>,
        static_cast<std::size_t>(originalrace::GarageSlotType::Count)>
        slots{};
    float arrowX = 0.0F;
    float upArrowY = 0.0F;
    float downArrowY = 0.0F;
    float backY = 0.0F;
};

class WorkshopFrameState
{
public:
    static constexpr std::size_t visibleGoodCount = 12U;
    static constexpr std::size_t goodColumns = 3U;
    static constexpr std::size_t firstGoodFocus = 1U;
    static constexpr std::size_t firstSlotFocus =
        firstGoodFocus + visibleGoodCount;
    static constexpr std::size_t slotCount =
        static_cast<std::size_t>(originalrace::GarageSlotType::Count);

    void show(std::vector<WorkshopGoodCandidate> candidates,
              const std::array<WorkshopSlotState, slotCount>& slots);
    void hide() noexcept;
    void updateGoods(std::vector<WorkshopGoodCandidate> candidates);
    void updateSlots(
        const std::array<WorkshopSlotState, slotCount>& slots) noexcept;

    const std::vector<WorkshopGoodEntry>& goods() const noexcept;
    std::size_t scroll() const noexcept;
    std::size_t maximumScroll() const noexcept;
    bool scrollGoods(int step) noexcept;
    const WorkshopGoodEntry* visibleGood(
        std::size_t visibleIndex) const noexcept;

    std::size_t focus() const noexcept;
    bool setPointerFocus(std::size_t focus) noexcept;
    std::optional<WorkshopCommand> handle(
        const rrr3d::input::ActionEvent& event,
        bool pointerSlotPlane = false) noexcept;

    WorkshopDragState& drag() noexcept;
    const WorkshopDragState& drag() const noexcept;
    void startDrag(originalrace::ProfileSlot item,
                   std::optional<originalrace::GarageSlotType> origin);
    void clearDrag() noexcept;

    const WorkshopConfirmationState& confirmation() const noexcept;
    void beginConfirmation(WorkshopConfirmationType type,
                           std::size_t pendingCatalogIndex) noexcept;
    void cancelConfirmation() noexcept;
    void setConfirmationYesFocused(bool value) noexcept;

    WorkshopLayout layout(float viewportWidth, float viewportHeight,
                          float topPanelHeight,
                          float bottomPanelHeight,
                          float leftPanelWidth, float leftPanelHeight,
                          float slotWidth, float slotHeight) const noexcept;

private:
    static std::vector<WorkshopGoodEntry> buildGoods(
        std::vector<WorkshopGoodCandidate> candidates);
    bool pointerFocusAvailable(std::size_t focus) const noexcept;
    bool keyboardFocusAvailable(std::size_t focus) const noexcept;
    std::size_t neighbor(std::size_t focus,
                         rrr3d::input::Action action) const noexcept;

    std::vector<WorkshopGoodEntry> goods_;
    std::array<WorkshopSlotState, slotCount> slots_{};
    std::size_t scroll_ = 0U;
    std::size_t focus_ = 0U;
    WorkshopDragState drag_;
    WorkshopConfirmationState confirmation_;
};

enum class AngarPlanetState : std::uint8_t
{
    Open,
    Closed,
    Unavailable,
    Completed,
};

struct AngarPlanetEntry
{
    AngarPlanetState state = AngarPlanetState::Unavailable;
    bool current = false;
    bool next = false;
};

struct SpaceshipLampState
{
    bool enabled = false;
    float intensity = 0.0F;
};

class SpaceshipFrameState
{
public:
    SpaceshipLampState progress(float deltaTime) noexcept;
    float sceneSeconds() const noexcept;

private:
    float redLampTime_ = 0.0F;
    float sceneSeconds_ = 0.0F;
};

enum class AngarCommandType : std::uint8_t
{
    Back,
    RequestTravel,
    ChangePlanet,
    CannotTravel,
};

struct AngarCommand
{
    AngarCommandType type = AngarCommandType::Back;
    std::size_t planet = 0U;
    bool fromPlanetSlot = false;
};

struct AngarTravelDialogState
{
    bool visible = false;
    std::size_t target = 0U;
    bool yesFocused = true;
    bool fromPlanetSlot = false;
};

struct AngarLayout
{
    float bottomPanelX = 0.0F;
    float bottomPanelY = 0.0F;
    float firstPlanetX = 0.0F;
    float planetY = 0.0F;
    float slotY = 0.0F;
    float backX = 0.0F;
    float backY = 0.0F;
    float infoX = 0.0F;
    float infoY = 0.0F;
    float infoLeft = 0.0F;
    float infoTop = 0.0F;
    float closeX = 0.0F;
    float closeY = 0.0F;

    float planetX(std::size_t index) const noexcept;
};

class AngarFrameState
{
public:
    void show(std::vector<AngarPlanetEntry> planets,
              bool planetChampion, bool campaign,
              bool networkClient, std::size_t currentPlanet);
    void hide() noexcept;

    const std::vector<AngarPlanetEntry>& planets() const noexcept;
    std::size_t planetCount() const noexcept;
    int selection() const noexcept;
    int previousSelection() const noexcept;
    std::size_t focus() const noexcept;
    bool selectPlanet(int index) noexcept;
    bool setPointerFocus(std::size_t focus) noexcept;
    void closeInfo() noexcept;

    void progress(float deltaTime) noexcept;
    float doorAlpha(std::size_t index) const noexcept;

    const AngarTravelDialogState& travelDialog() const noexcept;
    void cancelTravel() noexcept;
    void setTravelYesFocused(bool value) noexcept;
    std::optional<AngarCommand> handle(
        const rrr3d::input::ActionEvent& event) noexcept;

    AngarLayout layout(float viewportWidth, float viewportHeight,
                       float bottomPanelWidth, float bottomPanelHeight,
                       float infoWidth, float infoHeight,
                       float backWidth) const noexcept;

private:
    std::optional<AngarCommand> activateFocus() noexcept;
    std::optional<AngarCommand> requestTravel(
        std::size_t index, bool fromPlanetSlot) noexcept;

    std::vector<AngarPlanetEntry> planets_;
    bool planetChampion_ = false;
    bool campaign_ = true;
    bool networkClient_ = false;
    std::size_t currentPlanet_ = 0U;
    int selection_ = -1;
    int previousSelection_ = -1;
    float doorTime_ = -1.0F;
    std::size_t focus_ = 0U;
    AngarTravelDialogState travelDialog_;
};

enum class AchievementState : std::uint8_t
{
    Missing,
    Locked,
    Unlocked,
    Opened,
};

struct AchievementDefinition
{
    std::string_view name;
    std::string_view lockedImage;
    std::string_view openedImage;
    float x = 0.0F;
    float y = 0.0F;
};

struct AchievementEntry
{
    AchievementState state = AchievementState::Missing;
    std::uint32_t price = 0U;
};

enum class AchievementCommandType : std::uint8_t
{
    Back,
    RequestPurchase,
    Purchase,
};

struct AchievementCommand
{
    AchievementCommandType type = AchievementCommandType::Back;
    std::size_t achievement = 0U;
};

struct AchievementConfirmationState
{
    bool visible = false;
    std::size_t pending = 0U;
    bool yesFocused = true;
};

struct AchievementLayout
{
    float scale = 1.0F;
    float centerX = 0.0F;
    float centerY = 0.0F;
    float bottomPanelY = 0.0F;
    float rewardsY = 0.0F;
    float pointsY = 0.0F;
    float backX = 0.0F;
    float backY = 0.0F;

    float cardX(std::size_t index) const noexcept;
    float cardY(std::size_t index) const noexcept;
};

class AchievementFrameState
{
public:
    static constexpr std::size_t achievementCount = 9U;
    static constexpr std::size_t backFocus = achievementCount;
    static constexpr std::size_t noFocus = backFocus + 1U;

    static const std::array<AchievementDefinition, achievementCount>&
        definitions() noexcept;

    void show(
        const std::array<AchievementEntry, achievementCount>& entries)
        noexcept;
    void hide() noexcept;
    void update(
        const std::array<AchievementEntry, achievementCount>& entries)
        noexcept;

    const std::array<AchievementEntry, achievementCount>& entries()
        const noexcept;
    const AchievementEntry* entry(std::size_t index) const noexcept;
    std::size_t focus() const noexcept;
    bool setPointerFocus(std::size_t focus) noexcept;
    bool focusable(std::size_t focus) const noexcept;

    const AchievementConfirmationState& confirmation() const noexcept;
    void cancelPurchase() noexcept;
    void setPurchaseYesFocused(bool value) noexcept;
    std::optional<AchievementCommand> handle(
        const rrr3d::input::ActionEvent& event) noexcept;

    AchievementLayout layout(float viewportWidth, float viewportHeight,
                             float backWidth,
                             float backHeight) const noexcept;

private:
    static std::size_t direction(
        rrr3d::input::Action action) noexcept;
    std::size_t findFocusable(
        std::size_t element, std::vector<std::size_t> ignored,
        std::size_t navigationDirection) const noexcept;
    void moveFocus(std::size_t navigationDirection) noexcept;

    std::array<AchievementEntry, achievementCount> entries_{};
    std::size_t focus_ = noFocus;
    AchievementConfirmationState confirmation_{};
};

enum class FinishEventType : std::uint8_t
{
    First,
    Second,
    Third,
    Last,
};

struct FinishEntry
{
    std::size_t racer = 0U;
    std::size_t playerId = 0U;
    float voiceNameDuration = 0.0F;
};

struct FinishEvent
{
    FinishEventType type = FinishEventType::First;
    std::size_t racer = 0U;
    std::size_t playerId = 0U;
};

struct FinishRowState
{
    float alpha = 0.0F;
    float offsetX = 0.0F;
    bool visible = false;
};

struct FinishLayout
{
    float top = 0.0F;
    float leftLabelX = 0.0F;
    float rightLabelX = 0.0F;
    float lineWidth = 0.0F;
    float leftWidth = 0.0F;
    float leftHeight = 0.0F;

    float rowTop(std::size_t index) const noexcept;
    float rowCenterY(std::size_t index) const noexcept;
};

// Backend-neutral transcription of FinishMenu.  Race::Results order and
// voiceNameDur remain authoritative; the renderer only consumes row poses and
// dispatches the returned source event ids to the audio backend.
class FinishMenuFrameState
{
public:
    static constexpr std::size_t boxCount = 3U;

    void show(std::vector<FinishEntry> results) noexcept;
    void hide() noexcept;
    bool shown() const noexcept;
    bool handle(const rrr3d::input::ActionEvent& event) const noexcept;

    std::vector<FinishEvent> progress(
        float deltaTime, float viewportWidth) noexcept;
    const std::vector<FinishEntry>& results() const noexcept;
    std::size_t playerCount() const noexcept;
    const FinishRowState& row(std::size_t index) const noexcept;
    bool animationComplete() const noexcept;
    bool lastEventDispatched() const noexcept;

    FinishLayout layout(float viewportWidth, float viewportHeight,
                        float leftWidth,
                        float leftHeight) const noexcept;

private:
    std::vector<FinishEntry> results_;
    std::array<FinishRowState, boxCount> rows_{};
    float time_ = -1.0F;
    bool shown_ = false;
    bool lastEventDispatched_ = false;
};

} // namespace r3d::game::originalracemenu
