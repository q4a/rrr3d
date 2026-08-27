#include "OriginalMainMenu.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{

using namespace r3d::game::mainmenu2;
using r3d::game::originalmenu::MenuScreen;
using rrr3d::input::Action;
using rrr3d::input::ActionEvent;
using rrr3d::input::Source;

int fail(const std::string& message)
{
    std::cerr << "original MainMenu2 frames smoke failed: " << message
              << '\n';
    return 1;
}

ActionEvent event(Action action, bool repeated = false)
{
    return {action, 1.0F, true, repeated, Source::Keyboard, 0U};
}

bool close(float left, float right)
{
    return std::abs(left - right) < 0.001F;
}

} // namespace

int main()
{
    FrameController frame;
    frame.show(MenuScreen::Main, 5U);
    if (!frame.enabled(4U) || frame.detachedBackItem() ||
        !close(frame.itemX(1920.0F), 965.0F) ||
        !close(frame.itemY(1080.0F, 4U), 652.0F))
        return fail("MainFrame item policy/layout differs");

    frame.show(MenuScreen::GameMode, 3U, {false, false, false});
    if (frame.enabled(1U) || frame.firstEnabled() != 0U ||
        frame.moveSelection(0U, 1) != 2U ||
        frame.moveSelection(2U, -1) != 0U ||
        !frame.detachedBackItem() ||
        !close(frame.itemY(1080.0F, 2U), 690.0F))
        return fail("GameModeFrame tutorial/back policy differs");

    frame.show(MenuScreen::Tournament, 4U, {true, true, false});
    if (frame.enabled(0U) || !frame.enabled(1U) || !frame.enabled(2U))
        return fail("TournamentFrame accepted any profile as last profile");
    frame.show(MenuScreen::Tournament, 4U, {true, false, true});
    if (!frame.enabled(0U) || frame.enabled(2U))
        return fail("TournamentFrame continue/load ownership differs");

    ProfileFrameState profiles;
    profiles.show(6U);
    if (profiles.focus() != ProfileFocus::Back ||
        profiles.visibleBegin() != 0U || profiles.visibleEnd() != 4U ||
        profiles.canScrollUp() || !profiles.canScrollDown() ||
        !close(profiles.rowY(1080.0F, 3U), 609.0F) ||
        !close(profiles.upArrowY(1080.0F), 432.0F) ||
        !close(profiles.downArrowY(1080.0F), 660.0F) ||
        !close(profiles.backY(1080.0F), 690.0F))
        return fail("ProfileFrame initial grid/layout differs");

    profiles.handle(event(Action::MenuDown));
    profiles.handle(event(Action::TurnRight));
    if (profiles.focus() != ProfileFocus::Close ||
        profiles.focusIndex() != 0U)
        return fail("ProfileFrame horizontal item/close navigation differs");
    profiles.handle(event(Action::MenuDown));
    profiles.handle(event(Action::MenuDown));
    profiles.handle(event(Action::MenuDown));
    profiles.handle(event(Action::MenuDown));
    if (profiles.focus() != ProfileFocus::Down)
        return fail("ProfileFrame last-row/down-arrow navigation differs");
    const auto scrolled = profiles.handle(event(Action::MenuConfirm));
    if (!scrolled || scrolled->type != ProfileCommandType::Scrolled ||
        profiles.scroll() != 1U)
        return fail("ProfileFrame scroll command differs");

    profiles.handle(event(Action::MenuUp));
    if (profiles.focus() != ProfileFocus::Item ||
        profiles.focusIndex() != 4U)
        return fail("ProfileFrame arrow-to-last-visible navigation differs");
    const auto selected = profiles.handle(event(Action::MenuConfirm));
    if (!selected || selected->type != ProfileCommandType::Select ||
        selected->index != 4U)
        return fail("ProfileFrame select command differs");
    profiles.handle(event(Action::TurnLeft));
    const auto removed = profiles.handle(event(Action::MenuConfirm));
    if (!removed || removed->type != ProfileCommandType::Delete ||
        removed->index != 4U)
        return fail("ProfileFrame close/delete command differs");

    profiles.setProfileCount(2U);
    if (profiles.scroll() != 0U || profiles.focus() != ProfileFocus::Back)
        return fail("ProfileFrame deletion clamp/focus differs");
    const auto back = profiles.handle(event(Action::MenuBack));
    if (!back || back->type != ProfileCommandType::Back)
        return fail("ProfileFrame back command differs");

    std::cout << "original MainMenu2 frames smoke passed\n";
    return 0;
}
