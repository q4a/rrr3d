#include "OriginalTournament.h"

#include <iostream>

namespace source = r3d::game::originalrace::source;

int main()
{
    source::Planet planet(0U);
    planet.AddTrack(1, 0U, 4U);
    planet.AddTrack(1, 1U, 4U);
    planet.AddTrack(2, 2U, 5U);
    planet.SetRequestPoints({{1, 200}, {2, 350}});
    planet.SetPrices({{1000, 100}, {500, 50}});
    planet.SetWheaters({{0, 0.2F}, {1, 0.7F}, {2, 0.1F}});
    planet.InsertCar({"car-pass-zero", 0});
    planet.InsertSlot({"slot-pass-one", 4U, "stWeapon1", 1});

    source::Planet::PlayerData computer;
    computer.id = 2;
    computer.name = "scRip";
    computer.maxPass = 2;
    computer.cars = {{"car-one", 1}, {"car-two", 2}};
    computer.slots = {
        {"engine-one", 0U, "stMotor", 1},
        {"weapon-two", 5U, "stWeapon1", 2},
    };
    planet.InsertPlayer(computer);

    // Exact legacy wrap: abs(0 - 1) % 2 + 1 selects pass two.
    if (planet.GetTracks(0).front().catalogIndex != 2U ||
        planet.GetRequestPoints(1, 1U) != 200 ||
        planet.GetRequestPoints(1, 2U) != 300 ||
        planet.GetRequestPoints(8, 4U) != 500 ||
        planet.GetPrice(2).money != 500 ||
        planet.GetPrice(0).money != 0 ||
        planet.GenerateWheater(true, true, 0.0F).type != 1 ||
        planet.GenerateWheater(false, true, 0.0F).type != 0 ||
        planet.GenerateWheater(true, false, 0.95F).type != 2 ||
        planet.GetPlayer(2) == nullptr ||
        planet.GetPlayer("scRip") == nullptr ||
        planet.GetBoss().id != 2)
    {
        std::cerr << "Planet source rules differ from Windows\n";
        return 1;
    }

    source::Planet::PlayerState computerState;
    computerState.id = 9;
    computerState.computer = true;
    planet.StartPass(2, computerState, true);
    if (computerState.car != "car-two" ||
        computerState.slots.size() != 1U ||
        computerState.slots.front().record != "weapon-two")
        return 2;

    planet.Restore(source::Planet::psClosed, 0);
    if (!planet.Open() || planet.GetPass() != 1 ||
        planet.TakeCompletedCars() !=
            std::vector<std::string>{"car-pass-zero"})
        return 3;

    source::Tournament tournament;
    auto& campaignPlanet = tournament.AddPlanet();
    campaignPlanet.AddTrack(1, 0U, 4U);
    campaignPlanet.AddTrack(1, 1U, 4U);
    campaignPlanet.AddTrack(2, 2U, 5U);
    campaignPlanet.SetRequestPoints({{1, 200}, {2, 350}});
    campaignPlanet.SetWheaters({{0, 0.2F}, {1, 0.7F}, {2, 0.1F}});
    campaignPlanet.InsertSlot(
        {"slot-pass-one", 0U, {}, 1});
    campaignPlanet.Restore(source::Planet::psOpen, 1);

    if (!tournament.Select(0U, 1, 0U))
        return 4;
    if (tournament.SelectWheater(true, true, 0.0F).type != 1 ||
        !tournament.GetWheaterNightPass() ||
        tournament.SelectWheater(true, true, 0.0F).type != 0)
        return 12;
    const auto next = tournament.CompleteTrack(0, 1U);
    if (next.trackIndex != 1U || next.passComplete)
        return 5;
    const auto failed = tournament.CompleteTrack(199, 1U);
    if (failed.trackIndex != 0U || !failed.passComplete ||
        failed.passChampion || failed.planetPass != 1)
        return 6;
    tournament.Select(0U, 1, 1U);
    const auto pass = tournament.CompleteTrack(200, 1U);
    if (pass.trackIndex != 2U || !pass.passComplete ||
        !pass.passChampion || pass.planetChampion ||
        pass.planetPass != 2)
        return 7;
    if (pass.unlockedSlots !=
        std::vector<std::string>{"slot-pass-one"})
        return 8;
    const auto champion = tournament.CompleteTrack(350, 1U);
    if (champion.trackIndex != 0U || !champion.passComplete ||
        !champion.passChampion || !champion.planetChampion ||
        champion.planetPass != 3)
        return 9;

    auto& second = tournament.AddPlanet();
    second.AddTrack(1, 3U, 4U);
    second.Restore(source::Planet::psUnavailable, 0);
    if (tournament.PrevPlanet(1U) == nullptr ||
        tournament.NextPlanet(0U) == nullptr)
        return 10;
    second.Unlock();
    if (!tournament.ChangePlanet(1U) ||
        tournament.GetCurPlanetIndex() != 1U ||
        tournament.GetCurTrack()->catalogIndex != 3U)
        return 11;

    std::cout << "original Planet/Tournament source rules passed\n";
    return 0;
}
