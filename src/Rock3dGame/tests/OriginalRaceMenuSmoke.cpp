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

    std::array<WorkshopSlotState, WorkshopFrameState::slotCount>
        workshopSlots{};
    for (auto& slot : workshopSlots)
    {
        slot.active = true;
        slot.installed = true;
        slot.levelControlVisible = true;
        slot.controlEnabled = true;
    }
    for (std::size_t slot = 6U; slot < workshopSlots.size(); ++slot)
    {
        workshopSlots[slot].levelControlVisible = false;
        workshopSlots[slot].chargeControlVisible = true;
    }
    std::vector<WorkshopGoodCandidate> workshopCandidates;
    for (std::size_t index = 0U; index < 16U; ++index)
    {
        workshopCandidates.push_back(
            {index, static_cast<std::uint32_t>(160U - index),
             index == 1U, index != 2U});
    }
    WorkshopFrameState workshop;
    workshop.show(std::move(workshopCandidates), workshopSlots);
    if (workshop.goods().size() != 14U ||
        workshop.goods().front().catalogIndex != 15U ||
        workshop.maximumScroll() != 1U || !workshop.scrollGoods(1) ||
        workshop.visibleGood(0U)->catalogIndex != 12U ||
        workshop.scrollGoods(1))
        return fail("WorkshopFrame goods/filter/sort/scroll differs");

    if (!workshop.setPointerFocus(1U))
        return fail("WorkshopFrame pointer good focus differs");
    const auto goodCommand =
        workshop.handle(event(Action::MenuConfirm, false));
    if (!goodCommand ||
        goodCommand->type != WorkshopCommandType::ActivateGood ||
        goodCommand->index != 12U)
        return fail("WorkshopFrame visible-good command differs");
    workshop.handle(event(Action::MenuUp));
    if (workshop.focus() != WorkshopFrameState::firstSlotFocus + 2U)
        return fail("WorkshopFrame goods-to-armor navigation differs");
    workshop.handle(event(Action::TurnRight));
    if (workshop.focus() != WorkshopFrameState::firstSlotFocus + 3U)
        return fail("WorkshopFrame armor-to-motor graph differs");

    workshopSlots[5U].controlEnabled = false;
    workshop.updateSlots(workshopSlots);
    workshop.handle(event(Action::MenuUp));
    if (workshop.focus() != WorkshopFrameState::firstSlotFocus + 4U)
        return fail("WorkshopFrame disabled mine traversal differs");
    if (!workshop.setPointerFocus(
            WorkshopFrameState::firstSlotFocus + 5U))
        return fail("WorkshopFrame active slot pointer focus differs");
    const auto disabledControl =
        workshop.handle(event(Action::MenuConfirm));
    if (disabledControl)
        return fail("WorkshopFrame disabled slot control activated");
    const auto slotPlane = workshop.handle(
        event(Action::MenuConfirm), true);
    if (!slotPlane ||
        slotPlane->type != WorkshopCommandType::ActivateSlot ||
        slotPlane->index != 5U || !slotPlane->pointerSlotPlane)
        return fail("WorkshopFrame slot-plane command differs");

    workshop.startDrag({"weapon", 7U, true},
                       r3d::game::originalrace::GarageSlotType::Weapon1);
    if (!workshop.drag().active() || !workshop.drag().origin)
        return fail("WorkshopFrame source drag ownership differs");
    workshop.beginConfirmation(WorkshopConfirmationType::Sell, 8U);
    workshop.setConfirmationYesFocused(false);
    if (workshop.confirmation().type !=
            WorkshopConfirmationType::Sell ||
        workshop.confirmation().pendingCatalogIndex != 8U ||
        workshop.confirmation().yesFocused)
        return fail("WorkshopFrame confirmation ownership differs");
    workshop.clearDrag();
    workshop.cancelConfirmation();
    if (workshop.drag().active() ||
        workshop.confirmation().type != WorkshopConfirmationType::None)
        return fail("WorkshopFrame drag/confirmation reset differs");

    const auto workshopLayout = workshop.layout(
        1920.0F, 1080.0F, 100.0F, 250.0F, 300.0F, 500.0F,
        100.0F, 100.0F);
    if (!close(workshopLayout.goods[0][0], 91.0F) ||
        !close(workshopLayout.goods[1][0], 191.0F) ||
        !close(workshopLayout.goods[3][1], 349.0F) ||
        !close(workshopLayout.slots[0][0], 714.6F) ||
        !close(workshopLayout.slots[4][0], 1704.6F) ||
        !close(workshopLayout.arrowX, 180.0F) ||
        !close(workshopLayout.upArrowY, 265.0F) ||
        !close(workshopLayout.downArrowY, 658.0F) ||
        !close(workshopLayout.backY, 870.0F))
        return fail("WorkshopFrame source layout differs");

    SpaceshipFrameState spaceship;
    auto lamp = spaceship.progress(1.6F);
    if (!lamp.enabled || !close(lamp.intensity, 0.0F) ||
        !close(spaceship.sceneSeconds(), 1.6F))
        return fail("SpaceshipFrame first red-lamp phase differs");
    lamp = spaceship.progress(0.0F);
    if (!lamp.enabled || lamp.intensity <= 0.4F ||
        lamp.intensity >= 0.5F)
        return fail("SpaceshipFrame red-lamp ramp differs");

    AngarFrameState angar;
    angar.show(
        {{AngarPlanetState::Open, true, false},
         {AngarPlanetState::Unavailable, false, true},
         {AngarPlanetState::Closed, false, false}},
        true, true, false, 0U);
    if (angar.selection() != 1 || angar.focus() != 1U ||
        !close(angar.doorAlpha(1U), 1.0F))
        return fail("AngarFrame champion OnShow selection differs");
    angar.handle(event(Action::MenuUp));
    if (angar.focus() != 3U || angar.selection() != -1)
        return fail("AngarFrame planet-to-Back navigation differs");
    angar.handle(event(Action::TurnLeft));
    if (angar.focus() != 3U)
        return fail("AngarFrame Back horizontal null edge differs");
    angar.handle(event(Action::MenuDown));
    if (angar.focus() != 0U || angar.selection() != 0)
        return fail("AngarFrame Back-to-first-planet navigation differs");
    const auto stay = angar.handle(event(Action::MenuConfirm));
    if (!stay || stay->type != AngarCommandType::RequestTravel ||
        stay->planet != 0U || !stay->fromPlanetSlot ||
        !angar.travelDialog().visible)
        return fail("AngarFrame stay-planet request differs");
    angar.setTravelYesFocused(false);
    if (angar.handle(event(Action::MenuConfirm)).has_value() ||
        angar.travelDialog().visible)
        return fail("AngarFrame travel No result differs");
    angar.handle(event(Action::TurnRight));
    const auto fly = angar.handle(event(Action::MenuConfirm));
    if (!fly || fly->planet != 1U ||
        fly->type != AngarCommandType::RequestTravel)
        return fail("AngarFrame next-planet request differs");
    const auto change = angar.handle(event(Action::MenuConfirm));
    if (!change || change->type != AngarCommandType::ChangePlanet ||
        change->planet != 1U || angar.travelDialog().visible)
        return fail("AngarFrame accepted travel result differs");

    angar.selectPlanet(0);
    if (!close(angar.doorAlpha(0U), 0.0F) ||
        !close(angar.doorAlpha(1U), 1.0F))
        return fail("AngarFrame door animation start differs");
    angar.progress(0.125F);
    if (!close(angar.doorAlpha(0U), 0.5F) ||
        !close(angar.doorAlpha(1U), 0.5F))
        return fail("AngarFrame door animation midpoint differs");

    const auto angarLayout = angar.layout(
        1920.0F, 1080.0F, 900.0F, 250.0F, 400.0F, 300.0F,
        100.0F);
    if (!close(angarLayout.bottomPanelX, 960.0F) ||
        !close(angarLayout.bottomPanelY, 935.0F) ||
        !close(angarLayout.firstPlanetX, 635.0F) ||
        !close(angarLayout.planetX(1U), 859.0F) ||
        !close(angarLayout.planetY, 900.0F) ||
        !close(angarLayout.slotY, 1002.0F) ||
        !close(angarLayout.backX, 50.0F) ||
        !close(angarLayout.infoX, 635.0F) ||
        !close(angarLayout.closeY, 540.0F))
        return fail("AngarFrame source layout differs");

    AngarFrameState skirmishAngar;
    skirmishAngar.show(
        {{AngarPlanetState::Open, true, false},
         {AngarPlanetState::Open, false, false}},
        false, false, false, 0U);
    skirmishAngar.setPointerFocus(1U);
    const auto directChange =
        skirmishAngar.handle(event(Action::MenuConfirm));
    if (!directChange ||
        directChange->type != AngarCommandType::ChangePlanet ||
        directChange->planet != 1U)
        return fail("AngarFrame skirmish direct travel differs");

    const auto& achievementDefinitions =
        AchievementFrameState::definitions();
    if (achievementDefinitions[0U].name != "viper" ||
        achievementDefinitions[8U].name != "armor4" ||
        achievementDefinitions[8U].openedImage.find("musicTrack.png") ==
            std::string_view::npos)
        return fail("AchievmentFrame source definition order differs");
    std::array<AchievementEntry,
               AchievementFrameState::achievementCount>
        achievementEntries{};
    for (auto& entry : achievementEntries)
        entry.state = AchievementState::Locked;
    achievementEntries[6U] = {AchievementState::Unlocked, 600U};
    achievementEntries[8U] = {AchievementState::Unlocked, 800U};
    AchievementFrameState achievements;
    achievements.show(achievementEntries);
    if (achievements.focus() != AchievementFrameState::noFocus)
        return fail("AchievmentFrame OnShow focus reset differs");
    achievements.handle(event(Action::TurnLeft));
    if (achievements.focus() != AchievementFrameState::backFocus)
        return fail("AchievmentFrame first direction focus differs");
    achievements.handle(event(Action::MenuUp));
    if (achievements.focus() != 6U)
        return fail("AchievmentFrame Back-to-phaser graph differs");
    achievements.handle(event(Action::TurnLeft));
    if (achievements.focus() != 8U)
        return fail("AchievmentFrame recursive disabled traversal differs");
    const auto rewardRequest =
        achievements.handle(event(Action::MenuConfirm));
    if (!rewardRequest ||
        rewardRequest->type !=
            AchievementCommandType::RequestPurchase ||
        rewardRequest->achievement != 8U ||
        !achievements.confirmation().visible ||
        !achievements.confirmation().yesFocused)
        return fail("AchievmentFrame purchase request differs");
    achievements.setPurchaseYesFocused(false);
    if (achievements.handle(event(Action::MenuConfirm)).has_value() ||
        achievements.confirmation().visible)
        return fail("AchievmentFrame purchase No result differs");
    achievements.handle(event(Action::MenuConfirm));
    const auto rewardPurchase =
        achievements.handle(event(Action::MenuConfirm));
    if (!rewardPurchase ||
        rewardPurchase->type != AchievementCommandType::Purchase ||
        rewardPurchase->achievement != 8U)
        return fail("AchievmentFrame purchase Yes result differs");
    achievementEntries[8U].state = AchievementState::Opened;
    achievements.update(achievementEntries);
    if (achievements.focus() != AchievementFrameState::noFocus)
        return fail("AchievmentFrame opened button disable differs");
    achievements.setPointerFocus(0U);
    if (achievements.handle(event(Action::MenuConfirm)).has_value())
        return fail("AchievmentFrame locked pointer click differs");

    const auto achievementLayout = achievements.layout(
        1920.0F, 1080.0F, 100.0F, 50.0F);
    if (!close(achievementLayout.scale, 1.5F) ||
        !close(achievementLayout.cardX(0U), 1215.0F) ||
        !close(achievementLayout.cardY(0U), 157.5F) ||
        !close(achievementLayout.rewardsY, 832.5F) ||
        !close(achievementLayout.pointsY, 1040.0F) ||
        !close(achievementLayout.backX, 50.0F) ||
        !close(achievementLayout.backY, 1042.0F))
        return fail("AchievmentFrame source layout differs");

    FinishMenuFrameState finish;
    finish.show(
        {{0U, 10U, 0.4F}, {1U, 11U, 0.8F},
         {2U, 12U, 1.2F}, {9U, 42U, 7.0F}});
    if (!finish.shown() || finish.playerCount() != 3U ||
        finish.animationComplete() ||
        !finish.handle(event(Action::MenuConfirm)) ||
        finish.handle(event(Action::MenuConfirm, true)))
        return fail("FinishMenu OnShow/input ownership differs");
    if (!finish.progress(0.16F, 1920.0F).empty())
        return fail("FinishMenu initial reveal delay differs");
    auto finishEvents = finish.progress(0.0F, 1920.0F);
    if (finishEvents.size() != 1U ||
        finishEvents[0U].type != FinishEventType::First ||
        finishEvents[0U].racer != 0U ||
        !finish.row(0U).visible || finish.row(0U).offsetX >= 0.0F)
        return fail("FinishMenu first result reveal differs");
    finish.progress(0.4F, 1920.0F);
    finishEvents = finish.progress(0.0F, 1920.0F);
    if (finishEvents.size() != 1U ||
        finishEvents[0U].type != FinishEventType::Second ||
        finish.row(1U).offsetX <= 0.0F)
        return fail("FinishMenu variable second voice duration differs");
    finish.progress(0.8F, 1920.0F);
    finishEvents = finish.progress(0.0F, 1920.0F);
    if (finishEvents.size() != 1U ||
        finishEvents[0U].type != FinishEventType::Third)
        return fail("FinishMenu variable third voice duration differs");
    finish.progress(1.2F, 1920.0F);
    finishEvents = finish.progress(0.0F, 1920.0F);
    if (finishEvents.size() != 1U ||
        finishEvents[0U].type != FinishEventType::Last ||
        finishEvents[0U].racer != 9U ||
        finishEvents[0U].playerId != 42U ||
        !finish.lastEventDispatched() || !finish.animationComplete())
        return fail("FinishMenu exact last Race::Result event differs");
    const auto finishLayout = finish.layout(
        1920.0F, 1080.0F, 300.0F, 200.0F);
    if (!close(finishLayout.top, 240.0F) ||
        !close(finishLayout.leftLabelX, 630.0F) ||
        !close(finishLayout.rightLabelX, 1290.0F) ||
        !close(finishLayout.lineWidth, 1320.0F) ||
        !close(finishLayout.rowCenterY(2U), 740.0F))
        return fail("FinishMenu source layout differs");
    finish.hide();
    if (finish.shown() || finish.playerCount() != 0U)
        return fail("FinishMenu OnHide lifetime differs");

    std::cout << "original RaceMenu2 frames smoke passed\n";
    return 0;
}
