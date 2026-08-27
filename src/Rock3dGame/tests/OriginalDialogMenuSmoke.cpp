#include "OriginalDialogMenu.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{

using namespace r3d::game::originalmenu;

int fail(const std::string& message)
{
    std::cerr << "original DialogMenu2 smoke failed: " << message << '\n';
    return 1;
}

bool close(float left, float right)
{
    return std::abs(left - right) < 0.01F;
}

} // namespace

int main()
{
    MenuSystem menu;
    menu.AdjustLayout({1920.0F, 1080.0F});
    DialogSystem dialogs(menu);

    const auto& accept = dialogs.ShowAccept(
        "message", "yes", "no", {960.0F, 540.0F}, Anchor::Center,
        {400.0F, 180.0F}, {120.0F, 50.0F});
    if (!accept.visible || !accept.yesFocused ||
        !close(accept.layout.infoSize.x, 325.0F) ||
        !close(accept.layout.yesOffset.x, -70.0F) ||
        menu.TopModal() != FrameId::Accept)
        return fail("normal AcceptDialog source layout/modal differs");
    dialogs.SetAcceptFocus(false);
    if (dialogs.ChooseAccept(false) || dialogs.Accept().visible)
        return fail("AcceptDialog result/lifetime differs");

    const auto& maximum = dialogs.ShowAccept(
        "message", "yes", "no", {0.0F, 0.0F}, Anchor::Center,
        {400.0F, 180.0F}, {120.0F, 50.0F}, true, true, true);
    if (!close(maximum.layout.frameSize.x, 680.0F) ||
        !close(maximum.layout.infoSize.x, 552.5F) ||
        !close(maximum.layout.buttonSize.x, 270.0F) ||
        !close(maximum.layout.yesOffset.x, -110.0F) ||
        !maximum.disableFocus || maximum.center.x < 355.0F)
        return fail("max AcceptDialog scaling/clamp differs");
    dialogs.HideAccept();

    const auto& info = dialogs.ShowInfo(
        "warning", "wait", "ok", {960.0F, 540.0F}, Anchor::Center,
        {500.0F, 300.0F}, 0.0F, false);
    if (!info.visible || info.dismissable ||
        !close(info.titleOffset.y, -105.0F) ||
        !close(info.okOffset.y, 105.0F))
        return fail("InfoDialog layout/loading button policy differs");
    dialogs.HideInfo();

    const auto& weapon = dialogs.ShowWeapon(
        "name", "info", "100", "20", {960.0F, 540.0F},
        Anchor::Center, {320.0F, 180.0F}, 0.1F);
    if (weapon.visible)
        return fail("delayed WeaponDialog became visible immediately");
    dialogs.Progress(0.11F, {1920.0F, 1080.0F});
    if (!dialogs.Weapon().visible ||
        !close(dialogs.Weapon().moneyOffset.x, -60.0F))
        return fail("WeaponDialog delay/layout differs");
    dialogs.HideWeapon();

    dialogs.ShowMusicInfo("music", "track", {400.0F, 100.0F});
    dialogs.Progress(1.0F, {1920.0F, 1080.0F});
    dialogs.Progress(0.0F, {1920.0F, 1080.0F});
    if (!dialogs.Music().visible || dialogs.Music().offset <= 0.0F ||
        !close(dialogs.Music().center.y, 1000.0F))
        return fail("MusicDialog source animation/placement differs");
    dialogs.Progress(5.0F, {1920.0F, 1080.0F});
    dialogs.Progress(0.0F, {1920.0F, 1080.0F});
    if (dialogs.Music().visible)
        return fail("MusicDialog did not expire after source lifetime");

    std::cout << "original DialogMenu2 smoke passed\n";
    return 0;
}
