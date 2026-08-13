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

    error.clear();
    if (session.disconnectPlayer(serverOwnerId + 1U, error) ||
        error.empty())
    {
        std::cerr << "missing NetGame::DisconnectPlayer peer gate\n";
        return 7;
    }

    session.close();
    session.finalize();
    if (session.initialized() ||
        session.snapshot().state != SessionState::Stopped)
        return 8;

    std::cout << "Original NetGame lifecycle smoke passed on port "
              << defaultPort << '\n';
    return 0;
}
