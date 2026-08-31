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

    MainFrameState mainFrame;
    mainFrame.show();
    if (mainFrame.enabledItems().size() != 5U ||
        mainFrame.activate(0U) != MainFrameCommand::SinglePlayer ||
        mainFrame.activate(1U) != MainFrameCommand::Network ||
        mainFrame.activate(2U) != MainFrameCommand::Options ||
        mainFrame.activate(3U) != MainFrameCommand::Authors ||
        mainFrame.activate(4U) != MainFrameCommand::Exit ||
        mainFrame.activate(5U))
        return fail("MainFrame concrete command mapping differs");

    GameModeFrameState gameModeFrame;
    gameModeFrame.show(false);
    if (gameModeFrame.activate(1U) ||
        gameModeFrame.activate(0U) !=
            GameModeFrameCommand::Championship ||
        gameModeFrame.activate(2U) != GameModeFrameCommand::Back)
        return fail("GameModeFrame concrete disabled/command policy differs");
    gameModeFrame.show(true);
    if (gameModeFrame.activate(1U) != GameModeFrameCommand::Skirmish)
        return fail("GameModeFrame did not enable completed tutorial path");

    TournamentFrameState tournamentFrame;
    tournamentFrame.show(false, false);
    if (tournamentFrame.activate(0U) || tournamentFrame.activate(2U) ||
        tournamentFrame.activate(1U) !=
            TournamentFrameCommand::NewGame ||
        tournamentFrame.activate(3U) != TournamentFrameCommand::Back)
        return fail("TournamentFrame concrete profile gates differ");
    tournamentFrame.show(true, true);
    if (tournamentFrame.activate(0U) !=
            TournamentFrameCommand::Continue ||
        tournamentFrame.activate(2U) != TournamentFrameCommand::Load)
        return fail("TournamentFrame concrete commands differ");

    DifficultyFrameState difficultyFrame;
    difficultyFrame.show();
    const auto hardDifficulty = difficultyFrame.activate(2U);
    if (!hardDifficulty ||
        hardDifficulty->type != DifficultyFrameCommandType::StartMatch ||
        hardDifficulty->difficulty != DifficultySelection::Hard ||
        difficultyFrame.difficulty() != DifficultySelection::Hard ||
        difficultyFrame.difficultyName() != "gdHard" ||
        difficultyFrame.activate(3U)->type !=
            DifficultyFrameCommandType::Back ||
        difficultyFrame.difficulty() != DifficultySelection::Hard ||
        difficultyFrame.activate(4U))
        return fail("DifficultyFrame selection/deferred state differs");
    difficultyFrame.deferStartUntilVideo();
    const auto deferredDifficulty = difficultyFrame.onVideoStopped();
    if (!deferredDifficulty || difficultyFrame.waitingForVideo() ||
        deferredDifficulty->type !=
            DifficultyFrameCommandType::StartMatch ||
        deferredDifficulty->difficulty != DifficultySelection::Hard ||
        difficultyFrame.onVideoStopped())
        return fail("DifficultyFrame cVideoStopped ownership differs");
    difficultyFrame.deferStartUntilVideo();
    difficultyFrame.cancelDeferredStart();
    if (difficultyFrame.waitingForVideo() ||
        difficultyFrame.onVideoStopped())
        return fail("DifficultyFrame failed movie cancellation differs");

    NetworkFrameState network;
    network.show({"10.0.0.1", "10.0.0.2", "10.0.0.3",
                  "10.0.0.4", "10.0.0.5", "10.0.0.6",
                  "10.0.0.7"});
    if (network.addressLines().size() != 7U ||
        network.addressLines().front() != "My IP:" ||
        network.addressLines().back() != "10.0.0.6" ||
        network.activate(0U) != NetworkFrameCommand::Create ||
        network.activate(1U) != NetworkFrameCommand::Connect ||
        network.activate(2U) != NetworkFrameCommand::Back ||
        network.activate(3U))
        return fail("NetworkFrame source command/address policy differs");

    ServerTypeFrameState serverType;
    serverType.show();
    const auto localServer = serverType.activate(0U);
    if (serverType.itemCount() != 2U || !localServer ||
        localServer->command != ServerTypeCommand::StartGameMode ||
        localServer->type != MainMenuServerType::Local ||
        serverType.type() != MainMenuServerType::Local ||
        serverType.activate(1U)->command != ServerTypeCommand::Back)
        return fail("ServerTypeFrame local/back policy differs");
    serverType.reset();
    serverType.show(true, true);
    if (serverType.itemCount() != 4U ||
        serverType.activate(1U)->type != MainMenuServerType::Steam ||
        serverType.activate(2U)->type != MainMenuServerType::Lobby)
        return fail("ServerTypeFrame optional source choices differ");

    ClientTypeFrameState clientType;
    clientType.show();
    if (clientType.itemCount() != 3U ||
        clientType.activate(0U) != ClientTypeCommand::BrowseLan ||
        clientType.activate(1U) != ClientTypeCommand::EnterIpAddress ||
        clientType.activate(2U) != ClientTypeCommand::Back)
        return fail("ClientTypeFrame local/IP/back policy differs");

    NetBrowserFrameState browser;
    browser.show();
    if (!browser.waiting() ||
        browser.hint() != NetworkHint::Refreshing)
        return fail("NetBrowserFrame refresh state differs");
    browser.update(
        {"host0", "host1", "host2", "host3", "host4"},
        false, NetworkHint::None);
    if (browser.endpoints().size() != 5U ||
        !browser.canScrollDown() || browser.canScrollUp() ||
        !browser.scroll(1) || browser.scrollOffset() != 1U ||
        !browser.canScrollUp() || browser.canScrollDown() ||
        browser.activate(4U)->type != NetBrowserCommandType::Connect ||
        browser.activate(5U)->type != NetBrowserCommandType::Back)
        return fail("NetBrowserFrame grid/selection state differs");
    browser.update({}, false, NetworkHint::HostListEmpty);
    if (browser.scrollOffset() != 0U ||
        browser.hint() != NetworkHint::HostListEmpty ||
        browser.activate(0U)->type != NetBrowserCommandType::Back)
        return fail("NetBrowserFrame empty completion differs");

    NetIpAddressFrameState ipAddress;
    ipAddress.show();
    if (ipAddress.displayText() != "_" ||
        ipAddress.hint() != NetworkHint::EnterIpAddress ||
        !ipAddress.append("127..0a.0.1") ||
        ipAddress.displayText() != "127.0.0.1" ||
        ipAddress.address() != "127.0.0.1")
        return fail("NetIPAddressFrame PushLine/input differs");
    ipAddress.startWaiting(true, NetworkHint::Connecting);
    if (ipAddress.activate(0U) ||
        ipAddress.activate(1U) != NetIpCommand::Back ||
        ipAddress.hint() != NetworkHint::Connecting)
        return fail("NetIPAddressFrame waiting gate differs");
    ipAddress.startWaiting(false, NetworkHint::EnterIpAddress);
    while (ipAddress.backspace())
    {
    }
    if (ipAddress.displayText() != "_" || !ipAddress.address().empty())
        return fail("NetIPAddressFrame backspace sentinel differs");

    NetworkCallbackState networkCallbacks;
    networkCallbacks.beginConnecting();
    if (!networkCallbacks.connecting())
        return fail("MainMenu connecting owner was not armed");
    const auto connected = networkCallbacks.onConnectedPlayer(true);
    if (networkCallbacks.connecting() || !connected.hideMessage ||
        !connected.matchConnected || connected.pause ||
        connected.message != NetworkCallbackMessage::None)
        return fail("MainMenu OnConnectedPlayer policy differs");
    networkCallbacks.beginConnecting();
    const auto failed = networkCallbacks.onConnectionFailed();
    if (networkCallbacks.connecting() || !failed.hideMessage ||
        failed.message !=
            NetworkCallbackMessage::HostConnectionFailed ||
        failed.dialogAction != NetworkFailureDialogAction::None)
        return fail("MainMenu OnConnectionFailed policy differs");
    const auto ignoredPeer =
        networkCallbacks.onConnectedPlayer(false);
    if (ignoredPeer.hideMessage || ignoredPeer.matchConnected)
        return fail("MainMenu accepted a non-owner connected player");
    const auto inactiveDisconnect =
        networkCallbacks.onDisconnectedPlayer(true, true, true);
    if (!inactiveDisconnect.hideMessage || inactiveDisconnect.pause ||
        inactiveDisconnect.message != NetworkCallbackMessage::None)
        return fail("MainMenu OnDisconnectedPlayer policy differs");
    const auto activeDisconnect = networkCallbacks.onSessionFailure(
        r3d::game::originalnetwork::SessionFailure::HostDisconnected,
        true);
    if (!activeDisconnect.hideMessage || !activeDisconnect.showCursor ||
        !activeDisconnect.pause ||
        activeDisconnect.message !=
            NetworkCallbackMessage::Disconnected ||
        activeDisconnect.dialogAction !=
            NetworkFailureDialogAction::ExitMatch)
        return fail("Menu active owner disconnect policy differs");
    const auto critical = networkCallbacks.onSessionFailure(
        r3d::game::originalnetwork::SessionFailure::Critical, false);
    if (!critical.hideMessage || !critical.showCursor ||
        !critical.pause ||
        critical.message != NetworkCallbackMessage::CriticalError ||
        critical.dialogAction != NetworkFailureDialogAction::ExitMatch)
        return fail("Menu OnFailed critical policy differs");
    networkCallbacks.reset();
    if (networkCallbacks.connecting())
        return fail("MainMenu callback reset retained connecting state");

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
