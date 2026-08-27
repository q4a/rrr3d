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

} // namespace r3d::game::originalracemenu
