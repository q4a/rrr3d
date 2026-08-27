#pragma once

#include "OriginalMenuSystem.h"
#include "OriginalProfile.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace r3d::game::originaloptions
{

enum class Tab : std::uint8_t
{
    Game,
    Media,
    Network,
    Controls,
};

struct Catalog
{
    std::vector<std::pair<std::uint32_t, std::uint32_t>> displayModes;
    std::vector<std::string> languages;
    std::vector<std::string> commentators;
};

struct Availability
{
    bool difficulty = true;
    bool networkClient = false;
};

class OptionsMenuState
{
public:
    static constexpr std::size_t gameRows = 12U;
    static constexpr std::size_t mediaRows = 8U;
    static constexpr std::size_t networkRows = 5U;
    static constexpr std::size_t controlRows = 18U;
    static constexpr std::size_t gameVisibleRows = 7U;
    static constexpr std::size_t controlVisibleRows = 6U;

    explicit OptionsMenuState(Catalog catalog);

    void begin(const originalrace::UserConfig& config,
               std::string difficulty);
    void cancel(const originalrace::UserConfig& config,
                std::string difficulty);
    void commit(originalrace::UserConfig& config,
                std::string& difficulty) const;

    originalrace::UserConfig& draft() noexcept;
    const originalrace::UserConfig& draft() const noexcept;
    std::string& difficulty() noexcept;
    const std::string& difficulty() const noexcept;
    const Catalog& catalog() const noexcept;

    Tab tab() const noexcept;
    void setTab(Tab tab) noexcept;
    static bool owns(originalmenu::MenuScreen screen) noexcept;
    static Tab tabForScreen(originalmenu::MenuScreen screen) noexcept;
    static originalmenu::MenuScreen screenForTab(Tab tab) noexcept;

    std::vector<bool> enabledItems(originalmenu::MenuScreen screen,
                                   Availability availability) const;
    bool adjust(originalmenu::MenuScreen screen, std::size_t row,
                int direction);
    bool setControlBinding(std::string_view action, bool gamepad,
                           std::string binding);

    bool controlsUseGamepad() const noexcept;
    void setControlsUseGamepad(bool gamepad) noexcept;

    std::size_t rowCount(Tab tab) const noexcept;
    std::size_t visibleRowCount(Tab tab) const noexcept;
    std::size_t scroll(Tab tab) const noexcept;
    std::size_t visibleEnd(Tab tab) const noexcept;
    bool canScrollUp(Tab tab) const noexcept;
    bool canScrollDown(Tab tab) const noexcept;
    bool scrollGrid(Tab tab, int direction) noexcept;
    void ensureVisible(Tab tab, std::size_t row) noexcept;

    float stateButtonY(float viewportHeight, std::size_t state) const noexcept;
    float firstRowY(Tab tab, float viewportHeight) const noexcept;
    float rowY(Tab tab, float viewportHeight, std::size_t row) const noexcept;
    float upArrowY(Tab tab, float viewportHeight) const noexcept;
    float downArrowY(float viewportHeight) const noexcept;

private:
    static std::uint32_t cycle(std::uint32_t value, std::uint32_t count,
                               int direction) noexcept;
    std::size_t listIndex(const std::vector<std::string>& values,
                          std::string_view selected) const noexcept;
    std::size_t displayModeIndex() const noexcept;
    std::size_t& tabScroll(Tab tab) noexcept;
    std::size_t tabScroll(Tab tab) const noexcept;

    Catalog catalog_;
    originalrace::UserConfig draft_{};
    std::string difficulty_ = "gdNormal";
    Tab tab_ = Tab::Game;
    std::array<std::size_t, 4> scroll_{};
    bool controlsUseGamepad_ = false;
};

struct StartOptionsApplyResult
{
    bool applied = false;
    bool languageChanged = false;
};

class StartOptionsMenuState
{
public:
    static constexpr std::size_t optionRows = 4U;
    static constexpr std::size_t applyRow = optionRows;
    static constexpr std::size_t focusCount = optionRows + 1U;
    static constexpr std::size_t cameraSentinel = 2U;

    explicit StartOptionsMenuState(Catalog catalog);

    void begin(const originalrace::UserConfig& config);
    bool adjust(std::size_t row, int direction);
    bool adjustFocused(int direction);
    void moveFocus(int direction) noexcept;
    void setFocus(std::size_t focus) noexcept;
    StartOptionsApplyResult apply(originalrace::UserConfig& config) const;

    std::size_t cameraIndex() const noexcept;
    std::size_t resolutionIndex() const noexcept;
    std::size_t languageIndex() const noexcept;
    std::size_t commentatorIndex() const noexcept;
    std::size_t focus() const noexcept;
    bool applyEnabled() const noexcept;

    const std::pair<std::uint32_t, std::uint32_t>& resolution() const;
    std::string_view language() const noexcept;
    std::string_view commentator() const noexcept;

private:
    static std::size_t cycle(std::size_t value, std::size_t count,
                             int direction) noexcept;
    static std::size_t listIndex(const std::vector<std::string>& values,
                                 std::string_view selected) noexcept;
    std::size_t displayModeIndex(const originalrace::UserConfig& config)
        const noexcept;

    Catalog catalog_;
    std::size_t cameraIndex_ = cameraSentinel;
    std::size_t resolutionIndex_ = 0U;
    std::size_t languageIndex_ = 0U;
    std::size_t commentatorIndex_ = 0U;
    std::size_t focus_ = 0U;
    bool applyEnabled_ = false;
};

} // namespace r3d::game::originaloptions
