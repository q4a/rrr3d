#include "OriginalOptionsMenu.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{

using namespace r3d::game::originaloptions;
using r3d::game::originalmenu::MenuScreen;
using r3d::game::originalrace::PreferredCamera;
using r3d::game::originalrace::UserConfig;

int fail(const std::string& message)
{
    std::cerr << "original OptionsMenu smoke failed: " << message << '\n';
    return 1;
}

bool close(float left, float right)
{
    return std::abs(left - right) < 0.001F;
}

Catalog catalog()
{
    return {{{1280U, 720U}, {1280U, 800U}, {3024U, 1964U}},
            {"english", "russian"},
            {"classic", "metal"}};
}

} // namespace

int main()
{
    UserConfig config;
    config.cameraDistance = 2.0F;
    config.lapsCount = 8U;
    config.resolutionWidth = 1280U;
    config.resolutionHeight = 800U;
    config.language = "russian";
    config.commentatorStyle = "classic";
    std::string difficulty = "gdNormal";

    OptionsMenuState options(catalog());
    options.begin(config, difficulty);
    if (!options.adjust(MenuScreen::GameOptions, 1U, 1) ||
        !close(options.draft().cameraDistance, 1.0F) ||
        !options.adjust(MenuScreen::GameOptions, 1U, -1) ||
        !close(options.draft().cameraDistance, 2.0F))
        return fail("camera-distance stepper does not wrap like source");
    if (!options.adjust(MenuScreen::GameOptions, 9U, 1) ||
        options.draft().lapsCount != 1U ||
        !options.adjust(MenuScreen::GameOptions, 9U, -1) ||
        options.draft().lapsCount != 8U)
        return fail("lap-count stepper does not wrap like source");

    const auto clientItems = options.enabledItems(
        MenuScreen::GameOptions, {false, true});
    if (clientItems.size() != OptionsMenuState::gameRows + 2U ||
        clientItems[3U] || clientItems[4U] || clientItems[10U] ||
        !clientItems[2U] || !clientItems[11U])
        return fail("GameFrame source availability gates differ");

    options.setTab(Tab::Media);
    if (OptionsMenuState::screenForTab(options.tab()) !=
            MenuScreen::GraphicsOptions ||
        !options.adjust(MenuScreen::GraphicsOptions, 0U, 1) ||
        options.draft().resolutionWidth != 3024U ||
        options.draft().resolutionHeight != 1964U ||
        !options.adjust(MenuScreen::GraphicsOptions, 7U, 1) ||
        options.draft().fullScreen)
        return fail("MediaFrame source stepper ownership differs");

    options.setTab(Tab::Network);
    options.draft().musicVolume = 2.0F;
    if (!options.adjust(MenuScreen::SoundOptions, 0U, 1) ||
        options.draft().language != "english" ||
        !options.adjust(MenuScreen::SoundOptions, 2U, 1) ||
        !close(options.draft().musicVolume, 2.0F))
        return fail("NetworkTab language/volume behavior differs");

    options.setTab(Tab::Controls);
    options.setControlsUseGamepad(true);
    if (!options.setControlBinding("gaForward", true, "GamepadA") ||
        options.draft().gamepadControls["gaForward"] != "GamepadA" ||
        !options.controlsUseGamepad())
        return fail("ControlFrame binding ownership differs");

    options.setTab(Tab::Game);
    options.ensureVisible(Tab::Game, 11U);
    if (options.scroll(Tab::Game) != 5U ||
        options.visibleEnd(Tab::Game) != 12U ||
        !close(options.firstRowY(Tab::Game, 1080.0F), 387.0F) ||
        !close(options.upArrowY(Tab::Game, 1080.0F), 340.0F) ||
        !close(options.downArrowY(1080.0F), 735.0F))
        return fail("GameFrame grid scrolling/layout differs");
    options.setTab(Tab::Controls);
    options.ensureVisible(Tab::Controls, 17U);
    if (options.scroll(Tab::Controls) != 12U ||
        options.visibleEnd(Tab::Controls) != 18U)
        return fail("ControlFrame grid scrolling differs");

    UserConfig committed;
    std::string committedDifficulty;
    options.commit(committed, committedDifficulty);
    if (committed.gamepadControls["gaForward"] != "GamepadA" ||
        committedDifficulty != "gdNormal")
        return fail("OptionsMenu ApplyChanges draft commit differs");
    options.cancel(config, difficulty);
    if (options.draft().gamepadControls.contains("gaForward"))
        return fail("OptionsMenu CancelChanges did not restore config");

    StartOptionsMenuState startup(catalog());
    startup.begin(config);
    UserConfig startupResult = config;
    if (startup.cameraIndex() != StartOptionsMenuState::cameraSentinel ||
        startup.applyEnabled() || startup.apply(startupResult).applied ||
        startup.resolution() != std::pair{1280U, 800U} ||
        startup.language() != "russian")
        return fail("StartOptionsMenu LoadCfg sentinel differs");
    startup.adjust(1U, 1);
    startup.adjust(2U, 1);
    if (startup.apply(startupResult).applied)
        return fail("non-camera StartOptions stepper enabled Apply");
    startup.setFocus(0U);
    if (!startup.adjustFocused(-1) || !startup.applyEnabled() ||
        startup.cameraIndex() != 1U)
        return fail("StartOptions camera sentinel gate differs");
    startup.adjust(0U, -1);
    if (startup.cameraIndex() != 0U)
        return fail("StartOptions real camera toggle differs");
    const auto startupApply = startup.apply(startupResult);
    if (!startupApply.applied || !startupApply.languageChanged ||
        startupResult.preferredCamera != PreferredCamera::ThirdPerson ||
        startupResult.resolutionWidth != 3024U ||
        startupResult.resolutionHeight != 1964U ||
        startupResult.language != "english")
        return fail("StartOptionsMenu ApplyChanges differs");
    startup.setFocus(StartOptionsMenuState::applyRow);
    startup.moveFocus(1);
    if (startup.focus() != 0U)
        return fail("StartOptions source navigation ring differs");

    std::cout << "original OptionsMenu smoke passed\n";
    return 0;
}
