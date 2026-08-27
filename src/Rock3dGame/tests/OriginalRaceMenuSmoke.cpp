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

    GarageFrameState garage;
    std::array<bool, GarageFrameState::colorCount> colors{};
    colors.fill(true);
    colors[1] = false;
    garage.show(
        {{0U, false, true, true},
         {1U, true, false, true},
         {2U, false, false, true},
         {3U, false, true, false}},
        2U, false, colors);
    if (garage.cars().size() != 4U ||
        garage.cars()[0].catalogIndex != 0U ||
        garage.cars()[1].catalogIndex != 1U ||
        garage.cars()[2].catalogIndex != 2U ||
        !garage.cars()[2].locked || garage.selection() != 2U)
        return fail("GarageFrame available/secret/locked order differs");

    GarageFrameState campaignGarage;
    campaignGarage.show(
        {{0U, false, true, true},
         {1U, true, false, true},
         {2U, false, false, true}},
        0U, true, colors);
    if (campaignGarage.cars().size() != 2U ||
        campaignGarage.cars()[1].catalogIndex != 2U)
        return fail("GarageFrame campaign secret filtering differs");

    if (!garage.setFocus(3U))
        return fail("GarageFrame right arrow availability differs");
    const auto moved = garage.handle(event(Action::MenuConfirm));
    if (!moved ||
        moved->type != GarageCommandType::SelectionChanged ||
        moved->catalogIndex != 3U || !garage.canPrevious() ||
        garage.canNext())
        return fail("GarageFrame arrow selection/boundaries differ");
    const auto garageShoulder =
        garage.handle(event(Action::PreviousWeapon));
    if (!garageShoulder || garageShoulder->catalogIndex != 2U)
        return fail("GarageFrame shoulder binding differs");

    garage.setFocus(4U);
    garage.handle(event(Action::MenuDown));
    if (garage.focus() != 6U)
        return fail("GarageFrame unavailable color skip differs");
    const auto repaint = garage.handle(event(Action::MenuConfirm));
    if (!repaint || repaint->type != GarageCommandType::SelectColor ||
        repaint->colorIndex != 2U)
        return fail("GarageFrame color command differs");

    GarageFrameState rangeGarage;
    std::vector<GarageCarCandidate> rangeCars;
    for (std::size_t index = 0U; index < 8U; ++index)
        rangeCars.push_back({index, false, true, true});
    rangeGarage.show(std::move(rangeCars), 4U, false, colors);
    auto range = rangeGarage.visibleRange(510.0F, 100.0F);
    if (range.first != 2U || range.count != 5U ||
        !close(range.firstCenterX, 55.0F))
        return fail("GarageFrame initial car-grid centering differs");
    rangeGarage.select(7U);
    range = rangeGarage.visibleRange(510.0F, 100.0F);
    if (range.first != 3U || range.count != 5U)
        return fail("GarageFrame forward car-grid window differs");
    const auto garageLayout = rangeGarage.layout(
        1920.0F, 1080.0F, 90.0F, 250.0F, 120.0F, 130.0F);
    if (!close(garageLayout.topPanelX, 960.0F) ||
        !close(garageLayout.rightPanelX, 1921.0F) ||
        !close(garageLayout.sidePanelY, 460.0F) ||
        !close(garageLayout.leftArrowX, 165.0F) ||
        !close(garageLayout.rightArrowX, 1745.0F) ||
        !close(garageLayout.arrowY, 540.0F))
        return fail("GarageFrame source layout differs");

    std::cout << "original RaceMenu2 frames smoke passed\n";
    return 0;
}
