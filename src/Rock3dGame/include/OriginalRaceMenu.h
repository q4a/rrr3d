#pragma once

#include "InputActions.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
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
    float itemY(float viewportHeight, float bottomPanelHeight) const noexcept;

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
    float rightPanelX = 0.0F;
    float sidePanelY = 0.0F;
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

} // namespace r3d::game::originalracemenu
