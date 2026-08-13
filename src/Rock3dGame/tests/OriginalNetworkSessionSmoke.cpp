#include "OriginalNetwork.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

int main()
{
    using namespace r3d::game::originalnetwork;

    OriginalNetworkSession session;
    std::string error;
    if (!session.initialize(error))
    {
        std::cerr << error << '\n';
        return 1;
    }
    auto state = session.snapshot();
    if (state.state != SessionState::Idle || state.revision == 0U)
        return 2;

    if (!session.beginLanSearch(error) ||
        session.snapshot().state != SessionState::Searching)
        return 3;
    session.cancelLanSearch();
    if (session.snapshot().state != SessionState::Idle)
        return 4;

    if (!session.createHost(error))
    {
        std::cerr << error << '\n';
        return 5;
    }
    for (std::uint32_t time = 1U; time <= 20U; ++time)
    {
        session.process(time);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    state = session.snapshot();
    if (state.state != SessionState::Hosting || state.lastError != 0U)
    {
        std::cerr << "host state=" << stateName(state.state)
                  << " error=" << state.lastErrorMessage << '\n';
        return 6;
    }

    NetworkMatchState match;
    match.maxPlayers = 4U;
    match.maxComputers = 2U;
    NetworkPlayerState localPlayer;
    localPlayer.gamerId = 7;
    localPlayer.car = "marauder";
    if (!session.startMatch(match, localPlayer, error) ||
        !session.snapshot().models.matchActive ||
        session.snapshot().models.players.size() != 1U)
    {
        std::cerr << "session NetRace::StartMatch failed: " << error << '\n';
        return 13;
    }
    if (!session.exitMatch(error))
    {
        std::cerr << "session NetRace::ExitMatch failed: " << error << '\n';
        return 14;
    }
    state = session.snapshot();
    if (state.models.matchActive || state.models.raceActive ||
        !state.models.players.empty())
    {
        std::cerr << "session NetRace::DoExitMatch left player models\n";
        return 14;
    }

    error.clear();
    if (session.disconnectPlayer(serverOwnerId + 1U, error) ||
        error.empty())
    {
        std::cerr << "missing NetGame::DisconnectPlayer peer gate\n";
        return 7;
    }

    session.close();
    if (session.snapshot().failure != SessionFailure::None)
        return 8;

    // MainMenu::OnConnectionFailed is asynchronous in the original code.
    // Connecting to the source port immediately after closing our listener
    // gives the portable callback bridge the same deterministic refusal.
    if (!session.connect({"127.0.0.1", defaultPort}, error))
    {
        std::cerr << "refused connection did not start: " << error << '\n';
        return 9;
    }
    for (std::uint32_t attempt = 0U; attempt < 200U; ++attempt)
    {
        session.process(attempt + 100U);
        state = session.snapshot();
        if (state.state == SessionState::Failed)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (state.state != SessionState::Failed ||
        state.failure != SessionFailure::ConnectionFailed ||
        state.lastErrorMessage.empty())
    {
        std::cerr << "connection failure state=" << stateName(state.state)
                  << " message=" << state.lastErrorMessage << '\n';
        return 10;
    }

    session.close();
    if (session.snapshot().failure != SessionFailure::None)
        return 11;
    session.finalize();
    if (session.initialized() ||
        session.snapshot().state != SessionState::Stopped)
        return 12;

    std::cout << "Original NetGame lifecycle smoke passed on port "
              << defaultPort << '\n';
    return 0;
}
