#include "OriginalRaceMenu.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{

using namespace r3d::game::originalracemenu;
using rrr3d::input::Action;
using rrr3d::input::ActionEvent;
using rrr3d::input::Source;

int fail(const std::string& message)
{
    std::cerr << "original RaceMenu2 frames smoke failed: " << message
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
    RaceMenuState menu;
    if (menu.state() != State::Main || !menu.visibility().car ||
        !menu.visibility().main || menu.visibility().spaceship)
        return fail("RaceMenu initial ApplyState differs");
    if (!menu.setState(State::Garage) ||
        menu.lastState() != State::Main || !menu.visibility().car ||
        !menu.visibility().garage || menu.visibility().main)
        return fail("RaceMenu Garage visibility/last state differs");
    if (!menu.setState(State::Angar) || !menu.visibility().spaceship ||
        menu.visibility().car || !menu.visibility().angar ||
        menu.setState(State::Angar))
        return fail("RaceMenu Angar visibility/idempotence differs");

    RaceMainFrameState main;
    main.show(true);
    if (!main.enabled(0U) || main.enabled(1U) || main.enabled(6U) ||
        main.moveSelection(0U, 1) != 0U ||
        main.command(1U).has_value())
        return fail("RaceMainFrame client-ready gate differs");
    main.show(false);
    if (main.moveSelection(0U, -1) != 6U ||
        main.command(3U) != RaceMainCommand::Angar ||
        !close(main.itemX(1920.0F, 0U), 480.0F) ||
        !close(main.itemX(1920.0F, 6U), 1440.0F) ||
        !close(main.itemY(1080.0F, 254.0F), 881.0F))
        return fail("RaceMainFrame navigation/layout/command differs");

    GamersFrameState gamers;
    gamers.show({{10U, true}, {11U, false}, {12U, true}}, 11U);
    if (gamers.selection() != 0U || gamers.previous() ||
        gamers.next() != 2U || gamers.focus() != GamerFocus::Next)
        return fail("GamersFrame initial available selection differs");
    if (gamers.handle(event(Action::MenuUp)).has_value() ||
        gamers.focus() != GamerFocus::Right)
        return fail("GamersFrame Next-to-arrow navigation differs");
    const auto selected = gamers.handle(event(Action::MenuConfirm));
    if (!selected ||
        selected->type != GamerCommandType::SelectionChanged ||
        selected->index != 2U || selected->gamerId != 12U)
        return fail("GamersFrame right-arrow selection differs");
    gamers.handle(event(Action::MenuDown));
    const auto confirmed = gamers.handle(event(Action::MenuConfirm));
    if (!confirmed || confirmed->type != GamerCommandType::Confirm ||
        confirmed->gamerId != 12U)
        return fail("GamersFrame next/confirm command differs");
    const auto shoulder = gamers.handle(event(Action::PreviousWeapon));
    if (!shoulder || shoulder->index != 0U ||
        shoulder->gamerId != 10U)
        return fail("GamersFrame shoulder virtual-key binding differs");

    gamers.updateEntries({{10U, false}, {11U, true}, {12U, true}});
    if (gamers.selection() != 1U ||
        gamers.selectedGamerId() != 11U)
        return fail("GamersFrame availability refresh clamp differs");

    const auto layout = gamers.layout(
        1920.0F, 1080.0F, 254.0F, 40.0F, 100.0F);
    if (!close(layout.planetRadius, 413.0F) ||
        !close(layout.planetX, 935.0F) ||
        !close(layout.viewportSize, 930.0F) ||
        !close(layout.leftX, 465.0F) ||
        !close(layout.rightX, 1411.0F) ||
        !close(layout.nextX, 1410.0F) ||
        !close(layout.nextY, 980.0F))
        return fail("GamersFrame source layout differs");

    std::cout << "original RaceMenu2 frames smoke passed\n";
    return 0;
}
