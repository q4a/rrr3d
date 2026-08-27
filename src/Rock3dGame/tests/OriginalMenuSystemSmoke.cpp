#include "OriginalMenuSystem.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{

using namespace r3d::game::originalmenu;

int fail(const std::string& message)
{
    std::cerr << "original MenuSystem smoke failed: " << message << '\n';
    return 1;
}

bool close(float left, float right)
{
    return std::abs(left - right) < 0.001F;
}

} // namespace

int main()
{
    MenuSystem menu;
    if (!menu.Visibility().main || !menu.Visibility().guiMode ||
        !menu.Visibility().cursor || menu.HasModal())
        return fail("initial msMain2 visibility differs from Menu::ApplyState");

    menu.ShowModal(FrameId::Accept, true);
    menu.Show(FrameId::Music, true);
    menu.SetState(MenuState::Hud);
    if (!menu.Visibility().hud || menu.Visibility().guiMode ||
        menu.Visibility().cursor || menu.Visible(FrameId::Accept) ||
        menu.Visible(FrameId::Music) || !menu.ConsumeInputReset())
        return fail("SetState did not reproduce transient close/reset policy");

    menu.SetLoadingVisible(true);
    if (!menu.Visibility().info ||
        menu.TopModal() != FrameId::Loading ||
        menu.Frame(FrameId::Loading).topmost != MenuSystem::topmostLoading)
        return fail("loading frame does not override the current state");
    menu.SetLoadingVisible(false);

    menu.ShowModal(FrameId::Message, true);
    menu.ShowModal(FrameId::Accept, true);
    if (menu.TopModal() != FrameId::Accept)
        return fail("equal-level modal ordering differs from widget tree");
    menu.ShowModal(FrameId::Accept, false);
    if (menu.TopModal() != FrameId::Message)
        return fail("closing top modal did not restore previous modal");

    const auto clamped = menu.SetPos(
        FrameId::Message, {-100.0F, 900.0F}, Anchor::Center,
        {300.0F, 100.0F}, {1280.0F, 720.0F});
    if (!close(clamped.x, 165.0F) || !close(clamped.y, 655.0F))
        return fail("MenuFrame 15-pixel viewport clamp differs from source");

    menu.Show(FrameId::Weapon, false);
    const auto hiddenLayout = menu.Frame(FrameId::Weapon).layoutRevision;
    menu.AdjustLayout(FrameId::Weapon, {1920.0F, 1080.0F});
    if (menu.Frame(FrameId::Weapon).layoutRevision != hiddenLayout)
        return fail("hidden MenuFrame was laid out");
    menu.Show(FrameId::Weapon, true);
    menu.Invalidate(FrameId::Weapon);
    if (menu.Frame(FrameId::Weapon).invalidateRevision < 2U ||
        menu.Frame(FrameId::Weapon).layoutRevision < 2U)
        return fail("Show/Invalidate did not call source layout sequence");

    auto& screens = menu.Screens();
    screens = {MenuScreen::Main, MenuScreen::Options};
    menu.Selection() = 3U;
    if (screens.size() != 2U || screens.back() != MenuScreen::Options ||
        menu.Selection() != 3U || !menu.ConsumeInputReset())
        return fail("source screen stack/selection ownership failed");
    screens = std::vector<MenuScreen>{MenuScreen::Main, MenuScreen::RaceMenu};
    screens.pop_back();
    if (screens.back() != MenuScreen::Main ||
        menu.InputResetRevision() < 3U)
        return fail("screen transition reset revisions were not retained");

    std::cout << "original MenuSystem smoke passed\n";
    return 0;
}
