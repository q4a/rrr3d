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

    FinalMenuFrameState final;
    final.invalidate(
        "Caption One\nFirst line\nSecond line\n\n\n Caption Two ");
    if (final.credits().size() != 2U ||
        final.credits()[0U].caption != "Caption One" ||
        final.credits()[0U].text != "First line\nSecond line" ||
        final.credits()[1U].caption != "Caption Two" ||
        !final.credits()[1U].text.empty())
        return fail("FinalMenu credit section parsing differs");
    final.show();
    if (!final.shown() || !close(final.time(), 0.0F) ||
        !final.handle(event(Action::MenuConfirm)) ||
        final.handle(event(Action::MenuConfirm, true)) ||
        !final.handle(event(Action::MenuBack)))
        return fail("FinalMenu OnShow/navigation differs");
    ActionEvent pointerConfirm = event(Action::MenuConfirm);
    pointerConfirm.source = Source::Mouse;
    if (final.handle(pointerConfirm, false) ||
        !final.handle(pointerConfirm, true))
        return fail("FinalMenu Back pointer gate differs");
    if (final.progress(0.5F) ||
        !close(final.slideAlpha(0U), 0.5F) ||
        !close(final.slideAlpha(1U), 0.0F))
        return fail("FinalMenu first slide fade differs");
    final.show();
    final.progress(FinalMenuFrameState::duration / 9.0F + 0.5F);
    if (!close(final.slideAlpha(0U), 1.0F) ||
        !close(final.slideAlpha(1U), 0.5F))
        return fail("FinalMenu source slide interval differs");
    final.show();
    if (final.progress(53.5F))
        return fail("FinalMenu closed before source duration");
    const auto finalLayout = final.layout(
        1920.0F, 1080.0F, 1000.0F, 300.0F);
    if (!close(final.progressValue(), 0.5F) ||
        !close(finalLayout.backX, 150.0F) ||
        !close(finalLayout.backY, 1020.0F) ||
        !close(finalLayout.slideX, 760.0F) ||
        !close(finalLayout.slideY, 540.0F) ||
        !close(finalLayout.slideMaximumWidth, 1420.0F) ||
        !close(finalLayout.slideMaximumHeight, 780.0F) ||
        !close(finalLayout.creditsX, 1670.0F) ||
        !close(finalLayout.creditsY, 40.0F))
        return fail("FinalMenu source layout/credits motion differs");
    if (!final.progress(53.5F) ||
        !close(final.progressValue(), 1.0F))
        return fail("FinalMenu 107-second auto close differs");
    final.hide();
    if (final.shown() || !close(final.time(), 0.0F))
        return fail("FinalMenu OnShow(false) state differs");

    std::cout << "original MainMenu2 frames smoke passed\n";
    return 0;
}
