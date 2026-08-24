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

    // Exact legacy wrap: abs(0 - 1) % 2 + 1 selects pass two.
    if (planet.GetTracks(0).front().catalogIndex != 2U ||
        planet.GetRequestPoints(1, 1U) != 200 ||
        planet.GetRequestPoints(1, 2U) != 300 ||
        planet.GetRequestPoints(8, 4U) != 500 ||
        planet.GetPrice(2).money != 500 ||
        planet.GetPrice(0).money != 0)
    {
        std::cerr << "Planet source rules differ from Windows\n";
        return 1;
    }

    source::Tournament tournament;
    auto& campaignPlanet = tournament.AddPlanet();
    campaignPlanet.AddTrack(1, 0U, 4U);
    campaignPlanet.AddTrack(1, 1U, 4U);
    campaignPlanet.AddTrack(2, 2U, 5U);
    campaignPlanet.SetRequestPoints({{1, 200}, {2, 350}});
    campaignPlanet.Restore(source::Planet::psOpen, 1);

    if (!tournament.Select(0U, 1, 0U))
        return 2;
    const auto next = tournament.CompleteTrack(0, 1U);
    if (next.trackIndex != 1U || next.passComplete)
        return 3;
    const auto failed = tournament.CompleteTrack(199, 1U);
    if (failed.trackIndex != 0U || !failed.passComplete ||
        failed.passChampion || failed.planetPass != 1)
        return 4;
    tournament.Select(0U, 1, 1U);
    const auto pass = tournament.CompleteTrack(200, 1U);
    if (pass.trackIndex != 2U || !pass.passComplete ||
        !pass.passChampion || pass.planetChampion ||
        pass.planetPass != 2)
        return 5;
    const auto champion = tournament.CompleteTrack(350, 1U);
    if (champion.trackIndex != 0U || !champion.passComplete ||
        !champion.passChampion || !champion.planetChampion ||
        champion.planetPass != 3)
        return 6;

    std::cout << "original Planet/Tournament source rules passed\n";
    return 0;
}
