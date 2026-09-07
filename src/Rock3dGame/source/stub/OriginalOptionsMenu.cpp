#include "OriginalOptionsMenu.h"

#include <algorithm>
#include <cmath>

namespace r3d::game::originaloptions
{

OptionsMenuState::OptionsMenuState(Catalog catalog)
    : catalog_(std::move(catalog))
{
    if (catalog_.displayModes.empty())
        catalog_.displayModes.emplace_back(1280U, 720U);
}

void OptionsMenuState::begin(const originalrace::UserConfig& config,
                             std::string difficulty)
{
    draft_ = config;
    difficulty_ = std::move(difficulty);
    tab_ = Tab::Game;
    hoveredState_.reset();
    scroll_.fill(0U);
    controlsUseGamepad_ = false;
}

void OptionsMenuState::cancel(const originalrace::UserConfig& config,
                              std::string difficulty)
{
    hoveredState_.reset();
    draft_ = config;
    difficulty_ = std::move(difficulty);
    scroll_.fill(0U);
    controlsUseGamepad_ = false;
}

void OptionsMenuState::commit(originalrace::UserConfig& config,
                              std::string& difficulty) const
{
    config = draft_;
    difficulty = difficulty_;
}

originalrace::UserConfig& OptionsMenuState::draft() noexcept
{
    return draft_;
}

const originalrace::UserConfig& OptionsMenuState::draft() const noexcept
{
    return draft_;
}

std::string& OptionsMenuState::difficulty() noexcept
{
    return difficulty_;
}

const std::string& OptionsMenuState::difficulty() const noexcept
{
    return difficulty_;
}

const Catalog& OptionsMenuState::catalog() const noexcept
{
    return catalog_;
}

Tab OptionsMenuState::tab() const noexcept
{
    return tab_;
}

void OptionsMenuState::setTab(Tab tab) noexcept
{
    if (tab_ == tab)
        return;
    tab_ = tab;
    if (tab_ == Tab::Game || tab_ == Tab::Controls)
        tabScroll(tab_) = 0U;
}

void OptionsMenuState::setHoveredState(std::optional<std::size_t> state) noexcept
{
    hoveredState_ = state && *state < 4U ? state : std::nullopt;
}

bool OptionsMenuState::stateHovered(std::size_t state) const noexcept
{
    return hoveredState_ == state;
}

bool OptionsMenuState::stateHighlighted(std::size_t state) const noexcept
{
    // OptionsMenu::OnMouseEnter/Leave preserves the active tab while another
    // tab's button or its label dummy is hovered.
    return state == static_cast<std::size_t>(tab_) || stateHovered(state);
}

float OptionsMenuState::stateButtonCenterX(float viewportWidth, float normalWidth) noexcept
{
    // waLeft applies to the normal widget size, not the larger glow texture.
    return viewportWidth * 0.5F - 558.0F + normalWidth * 0.5F;
}

bool OptionsMenuState::owns(originalmenu::MenuScreen screen) noexcept
{
    using originalmenu::MenuScreen;
    return screen == MenuScreen::GameOptions ||
           screen == MenuScreen::GraphicsOptions ||
           screen == MenuScreen::SoundOptions ||
           screen == MenuScreen::ControlsOptions;
}

Tab OptionsMenuState::tabForScreen(originalmenu::MenuScreen screen) noexcept
{
    using originalmenu::MenuScreen;
    switch (screen)
    {
    case MenuScreen::GraphicsOptions:
        return Tab::Media;
    case MenuScreen::SoundOptions:
        return Tab::Network;
    case MenuScreen::ControlsOptions:
        return Tab::Controls;
    default:
        return Tab::Game;
    }
}

originalmenu::MenuScreen OptionsMenuState::screenForTab(Tab tab) noexcept
{
    using originalmenu::MenuScreen;
    switch (tab)
    {
    case Tab::Media:
        return MenuScreen::GraphicsOptions;
    case Tab::Network:
        return MenuScreen::SoundOptions;
    case Tab::Controls:
        return MenuScreen::ControlsOptions;
    case Tab::Game:
        return MenuScreen::GameOptions;
    }
    return MenuScreen::GameOptions;
}

std::vector<bool> OptionsMenuState::enabledItems(
    originalmenu::MenuScreen screen, Availability availability) const
{
    const auto tab = tabForScreen(screen);
    std::vector<bool> result(rowCount(tab) + 2U, true);
    if (tab != Tab::Game)
        return result;

    result[3U] = availability.difficulty;
    if (availability.networkClient)
    {
        for (std::size_t row = 3U; row <= 10U; ++row)
            result[row] = false;
    }
    return result;
}

std::uint32_t OptionsMenuState::cycle(std::uint32_t value,
                                      std::uint32_t count,
                                      int direction) noexcept
{
    if (count == 0U)
        return 0U;
    const auto normalized =
        (static_cast<int>(value % count) + (direction < 0 ? -1 : 1) +
         static_cast<int>(count)) % static_cast<int>(count);
    return static_cast<std::uint32_t>(normalized);
}

std::size_t OptionsMenuState::listIndex(
    const std::vector<std::string>& values,
    std::string_view selected) const noexcept
{
    const auto found = std::find(values.begin(), values.end(), selected);
    return found == values.end()
               ? 0U
               : static_cast<std::size_t>(found - values.begin());
}

std::size_t OptionsMenuState::displayModeIndex() const noexcept
{
    const auto current = std::find(
        catalog_.displayModes.begin(), catalog_.displayModes.end(),
        std::pair{draft_.resolutionWidth, draft_.resolutionHeight});
    if (current != catalog_.displayModes.end())
        return static_cast<std::size_t>(current - catalog_.displayModes.begin());

    const auto wantedArea = static_cast<std::uint64_t>(
        draft_.resolutionWidth) * draft_.resolutionHeight;
    std::size_t best = 0U;
    std::uint64_t bestDistance = ~std::uint64_t{0};
    for (std::size_t index = 0U; index < catalog_.displayModes.size(); ++index)
    {
        const auto area = static_cast<std::uint64_t>(
            catalog_.displayModes[index].first) *
            catalog_.displayModes[index].second;
        const auto distance = area > wantedArea ? area - wantedArea
                                                : wantedArea - area;
        if (distance < bestDistance)
        {
            best = index;
            bestDistance = distance;
        }
    }
    return best;
}

bool OptionsMenuState::adjust(originalmenu::MenuScreen screen,
                              std::size_t row, int direction)
{
    const auto tab = tabForScreen(screen);
    direction = direction < 0 ? -1 : 1;
    switch (tab)
    {
    case Tab::Game:
        switch (row)
        {
        case 0U:
            draft_.preferredCamera =
                draft_.preferredCamera == originalrace::PreferredCamera::ThirdPerson
                    ? originalrace::PreferredCamera::Isometric
                    : originalrace::PreferredCamera::ThirdPerson;
            return true;
        case 1U: {
            const auto current = static_cast<std::uint32_t>(std::clamp(
                std::lround((draft_.cameraDistance - 1.0F) / 0.25F),
                0L, 4L));
            draft_.cameraDistance =
                1.0F + static_cast<float>(cycle(current, 5U, direction)) *
                           0.25F;
            return true;
        }
        case 2U:
            draft_.enableHud = !draft_.enableHud;
            return true;
        case 3U: {
            static constexpr std::array<std::string_view, 3> values{
                "gdEasy", "gdNormal", "gdHard"};
            const auto found = std::find(values.begin(), values.end(),
                                         difficulty_);
            const auto current = found == values.end()
                                     ? 1U
                                     : static_cast<std::uint32_t>(
                                           found - values.begin());
            difficulty_ = values[cycle(current, 3U, direction)];
            return true;
        }
        case 4U:
            draft_.springBorders = !draft_.springBorders;
            return true;
        case 5U:
            draft_.upgradeMaxLevel =
                cycle(draft_.upgradeMaxLevel, 3U, direction);
            return true;
        case 6U:
            draft_.weaponMaxLevel =
                cycle(std::clamp(draft_.weaponMaxLevel, 1U, 4U) - 1U,
                      4U, direction) + 1U;
            return true;
        case 7U:
            draft_.maxPlayers =
                cycle(std::clamp(draft_.maxPlayers, 2U, 8U) - 2U,
                      7U, direction) + 2U;
            return true;
        case 8U:
            draft_.maxComputers =
                cycle(std::min(draft_.maxComputers, 7U), 8U, direction);
            return true;
        case 9U:
            draft_.lapsCount =
                cycle(std::clamp(draft_.lapsCount, 1U, 8U) - 1U,
                      8U, direction) + 1U;
            return true;
        case 10U:
            draft_.enableMineBug = !draft_.enableMineBug;
            return true;
        case 11U:
            draft_.disableVideo = !draft_.disableVideo;
            return true;
        default:
            return false;
        }
    case Tab::Media:
        switch (row)
        {
        case 0U: {
            const auto next = cycle(
                static_cast<std::uint32_t>(displayModeIndex()),
                static_cast<std::uint32_t>(catalog_.displayModes.size()),
                direction);
            draft_.resolutionWidth = catalog_.displayModes[next].first;
            draft_.resolutionHeight = catalog_.displayModes[next].second;
            return true;
        }
        case 1U:
            draft_.quality.filtering =
                cycle(draft_.quality.filtering, 4U, direction);
            return true;
        case 2U:
            draft_.quality.msaa = cycle(draft_.quality.msaa, 3U, direction);
            return true;
        case 3U:
            draft_.quality.shadow =
                cycle(draft_.quality.shadow, 3U, direction);
            return true;
        case 4U:
            draft_.quality.environment =
                cycle(draft_.quality.environment, 3U, direction);
            return true;
        case 5U:
            draft_.quality.light = cycle(draft_.quality.light, 3U, direction);
            return true;
        case 6U:
            draft_.quality.postEffect =
                cycle(draft_.quality.postEffect, 3U, direction);
            return true;
        case 7U:
            draft_.fullScreen = !draft_.fullScreen;
            return true;
        default:
            return false;
        }
    case Tab::Network:
        switch (row)
        {
        case 0U:
            if (catalog_.languages.empty())
                return false;
            draft_.language = catalog_.languages[cycle(
                static_cast<std::uint32_t>(
                    listIndex(catalog_.languages, draft_.language)),
                static_cast<std::uint32_t>(catalog_.languages.size()),
                direction)];
            return true;
        case 1U:
            if (catalog_.commentators.empty())
                return false;
            draft_.commentatorStyle = catalog_.commentators[cycle(
                static_cast<std::uint32_t>(listIndex(
                    catalog_.commentators, draft_.commentatorStyle)),
                static_cast<std::uint32_t>(catalog_.commentators.size()),
                direction)];
            return true;
        case 2U:
            draft_.musicVolume = std::clamp(
                draft_.musicVolume + static_cast<float>(direction) * 0.1F,
                0.0F, 2.0F);
            return true;
        case 3U:
            draft_.effectsVolume = std::clamp(
                draft_.effectsVolume + static_cast<float>(direction) * 0.1F,
                0.0F, 2.0F);
            return true;
        case 4U:
            draft_.voiceVolume = std::clamp(
                draft_.voiceVolume + static_cast<float>(direction) * 0.1F,
                0.0F, 2.0F);
            return true;
        default:
            return false;
        }
    case Tab::Controls:
        return false;
    }
    return false;
}

bool OptionsMenuState::setControlBinding(std::string_view action,
                                         bool gamepad,
                                         std::string binding)
{
    if (action.empty())
        return false;
    auto& bindings = gamepad ? draft_.gamepadControls
                             : draft_.keyboardControls;
    bindings[std::string(action)] = std::move(binding);
    return true;
}

bool OptionsMenuState::controlsUseGamepad() const noexcept
{
    return controlsUseGamepad_;
}

void OptionsMenuState::setControlsUseGamepad(bool gamepad) noexcept
{
    controlsUseGamepad_ = gamepad;
}

std::size_t OptionsMenuState::rowCount(Tab tab) const noexcept
{
    switch (tab)
    {
    case Tab::Game:
        return gameRows;
    case Tab::Media:
        return mediaRows;
    case Tab::Network:
        return networkRows;
    case Tab::Controls:
        return controlRows;
    }
    return 0U;
}

std::size_t OptionsMenuState::visibleRowCount(Tab tab) const noexcept
{
    if (tab == Tab::Game)
        return gameVisibleRows;
    if (tab == Tab::Controls)
        return controlVisibleRows;
    return rowCount(tab);
}

std::size_t& OptionsMenuState::tabScroll(Tab tab) noexcept
{
    return scroll_[static_cast<std::size_t>(tab)];
}

std::size_t OptionsMenuState::tabScroll(Tab tab) const noexcept
{
    return scroll_[static_cast<std::size_t>(tab)];
}

std::size_t OptionsMenuState::scroll(Tab tab) const noexcept
{
    return tabScroll(tab);
}

std::size_t OptionsMenuState::visibleEnd(Tab tab) const noexcept
{
    return std::min(scroll(tab) + visibleRowCount(tab), rowCount(tab));
}

bool OptionsMenuState::canScrollUp(Tab tab) const noexcept
{
    return scroll(tab) > 0U;
}

bool OptionsMenuState::canScrollDown(Tab tab) const noexcept
{
    return scroll(tab) + visibleRowCount(tab) < rowCount(tab);
}

bool OptionsMenuState::scrollGrid(Tab tab, int direction) noexcept
{
    auto& value = tabScroll(tab);
    if (direction < 0)
    {
        if (value == 0U)
            return false;
        --value;
        return true;
    }
    if (!canScrollDown(tab))
        return false;
    ++value;
    return true;
}

void OptionsMenuState::ensureVisible(Tab tab, std::size_t row) noexcept
{
    if (row >= rowCount(tab))
        return;
    auto& value = tabScroll(tab);
    if (row < value)
        value = row;
    else if (row >= value + visibleRowCount(tab))
        value = row - visibleRowCount(tab) + 1U;
}

float OptionsMenuState::stateButtonY(float viewportHeight,
                                     std::size_t state) const noexcept
{
    return viewportHeight * 0.5F - 125.0F +
           static_cast<float>(state) * 100.0F;
}

float OptionsMenuState::firstRowY(Tab tab, float viewportHeight) const noexcept
{
    // GUI::Grid::Reposition adds cellSize/2 for waLeftTop grids. Both
    // GameFrame (50px cells) and ControlsFrame (43+7px cells) use it;
    // MediaFrame/NetworkTab position their labels directly instead.
    return viewportHeight * 0.5F -
           (tab == Tab::Controls ? 105.0F
                                 : tab == Tab::Game ? 153.0F : 174.0F);
}

float OptionsMenuState::rowY(Tab tab, float viewportHeight,
                             std::size_t row) const noexcept
{
    return firstRowY(tab, viewportHeight) +
           static_cast<float>(row - scroll(tab)) * 50.0F;
}

float OptionsMenuState::upArrowY(Tab tab, float viewportHeight) const noexcept
{
    return viewportHeight * 0.5F -
           (tab == Tab::Controls ? 150.0F : 200.0F);
}

float OptionsMenuState::downArrowY(float viewportHeight) const noexcept
{
    return viewportHeight * 0.5F + 195.0F;
}

StartOptionsMenuState::StartOptionsMenuState(Catalog catalog)
    : catalog_(std::move(catalog))
{
    if (catalog_.displayModes.empty())
        catalog_.displayModes.emplace_back(1280U, 720U);
    if (catalog_.languages.empty())
        catalog_.languages.emplace_back("english");
    if (catalog_.commentators.empty())
        catalog_.commentators.emplace_back("english");
}

std::size_t StartOptionsMenuState::cycle(std::size_t value,
                                         std::size_t count,
                                         int direction) noexcept
{
    if (count == 0U)
        return 0U;
    if (direction < 0)
        return value == 0U ? count - 1U : value - 1U;
    return (value + 1U) % count;
}

std::size_t StartOptionsMenuState::listIndex(
    const std::vector<std::string>& values,
    std::string_view selected) noexcept
{
    const auto found = std::find(values.begin(), values.end(), selected);
    return found == values.end()
               ? 0U
               : static_cast<std::size_t>(found - values.begin());
}

std::size_t StartOptionsMenuState::displayModeIndex(
    const originalrace::UserConfig& config) const noexcept
{
    const auto exact = std::find(
        catalog_.displayModes.begin(), catalog_.displayModes.end(),
        std::pair{config.resolutionWidth, config.resolutionHeight});
    if (exact != catalog_.displayModes.end())
        return static_cast<std::size_t>(exact - catalog_.displayModes.begin());

    const auto wantedArea = static_cast<std::uint64_t>(
        config.resolutionWidth) * config.resolutionHeight;
    std::size_t best = 0U;
    std::uint64_t bestDistance = ~std::uint64_t{0};
    for (std::size_t index = 0U; index < catalog_.displayModes.size(); ++index)
    {
        const auto area = static_cast<std::uint64_t>(
            catalog_.displayModes[index].first) *
            catalog_.displayModes[index].second;
        const auto distance = area > wantedArea ? area - wantedArea
                                                : wantedArea - area;
        if (distance < bestDistance)
        {
            best = index;
            bestDistance = distance;
        }
    }
    return best;
}

void StartOptionsMenuState::begin(const originalrace::UserConfig& config)
{
    // StartOptionsMenu::LoadCfg uses cPrefCameraEnd rather than the saved
    // camera. Selecting either arrow resolves that sentinel to Isometric and
    // is the only operation which enables Apply.
    cameraIndex_ = cameraSentinel;
    resolutionIndex_ = displayModeIndex(config);
    languageIndex_ = listIndex(catalog_.languages, config.language);
    commentatorIndex_ =
        listIndex(catalog_.commentators, config.commentatorStyle);
    focus_ = 0U;
    applyEnabled_ = false;
}

bool StartOptionsMenuState::adjust(std::size_t row, int direction)
{
    switch (row)
    {
    case 0U:
        if (cameraIndex_ == cameraSentinel)
        {
            cameraIndex_ = 1U;
            applyEnabled_ = true;
        }
        else
        {
            // The source's three-value stepper rejects cPrefCameraEnd in
            // OnSelect, so either arrow toggles between the two real cameras.
            cameraIndex_ = cameraIndex_ == 0U ? 1U : 0U;
        }
        return true;
    case 1U:
        resolutionIndex_ = cycle(
            resolutionIndex_, catalog_.displayModes.size(), direction);
        return true;
    case 2U:
        languageIndex_ = cycle(
            languageIndex_, catalog_.languages.size(), direction);
        return true;
    case 3U:
        commentatorIndex_ = cycle(
            commentatorIndex_, catalog_.commentators.size(), direction);
        return true;
    default:
        return false;
    }
}

bool StartOptionsMenuState::adjustFocused(int direction)
{
    return focus_ < optionRows && adjust(focus_, direction);
}

void StartOptionsMenuState::moveFocus(int direction) noexcept
{
    focus_ = cycle(focus_, focusCount, direction);
}

void StartOptionsMenuState::setFocus(std::size_t focus) noexcept
{
    if (focus < focusCount)
        focus_ = focus;
}

StartOptionsApplyResult StartOptionsMenuState::apply(
    originalrace::UserConfig& config) const
{
    if (!applyEnabled_ || cameraIndex_ >= cameraSentinel)
        return {};

    const bool languageChanged =
        config.language != catalog_.languages[languageIndex_];
    config.preferredCamera =
        cameraIndex_ == 0U
            ? originalrace::PreferredCamera::ThirdPerson
            : originalrace::PreferredCamera::Isometric;
    config.resolutionWidth = resolution().first;
    config.resolutionHeight = resolution().second;
    config.language = catalog_.languages[languageIndex_];
    config.commentatorStyle = catalog_.commentators[commentatorIndex_];
    return {true, languageChanged};
}

std::size_t StartOptionsMenuState::cameraIndex() const noexcept
{
    return cameraIndex_;
}

std::size_t StartOptionsMenuState::resolutionIndex() const noexcept
{
    return resolutionIndex_;
}

std::size_t StartOptionsMenuState::languageIndex() const noexcept
{
    return languageIndex_;
}

std::size_t StartOptionsMenuState::commentatorIndex() const noexcept
{
    return commentatorIndex_;
}

std::size_t StartOptionsMenuState::focus() const noexcept
{
    return focus_;
}

bool StartOptionsMenuState::applyEnabled() const noexcept
{
    return applyEnabled_;
}

const std::pair<std::uint32_t, std::uint32_t>&
StartOptionsMenuState::resolution() const
{
    return catalog_.displayModes[resolutionIndex_];
}

std::string_view StartOptionsMenuState::language() const noexcept
{
    return catalog_.languages[languageIndex_];
}

std::string_view StartOptionsMenuState::commentator() const noexcept
{
    return catalog_.commentators[commentatorIndex_];
}

} // namespace r3d::game::originaloptions
