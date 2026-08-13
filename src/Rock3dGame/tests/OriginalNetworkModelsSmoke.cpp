#include "stdafx.h"

#include "OriginalNetworkModels.h"

#include <net/NetService.h>

#include <arpa/inet.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace
{

struct User final : net::INetServiceUser
{
    bool OnConnected(net::INetConnection*) override { return true; }
};

unsigned reservePort()
{
    for (unsigned attempt = 0U; attempt < 32U; ++attempt)
    {
        const int tcp = socket(AF_INET, SOCK_STREAM, 0);
        if (tcp < 0)
            return 0U;
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        if (bind(tcp, reinterpret_cast<const sockaddr*>(&address),
                 sizeof(address)) != 0)
        {
            close(tcp);
            continue;
        }
        socklen_t size = sizeof(address);
        if (getsockname(tcp, reinterpret_cast<sockaddr*>(&address), &size) !=
            0)
        {
            close(tcp);
            continue;
        }
        const int udp = socket(AF_INET, SOCK_DGRAM, 0);
        const bool available =
            udp >= 0 && bind(udp, reinterpret_cast<const sockaddr*>(&address),
                             sizeof(address)) == 0;
        const auto port =
            available ? static_cast<unsigned>(ntohs(address.sin_port)) : 0U;
        if (udp >= 0)
            close(udp);
        close(tcp);
        if (port != 0U)
            return port;
    }
    return 0U;
}

bool pump(net::NetService& server, net::NetService& client,
          const std::function<bool()>& complete, unsigned& clock,
          unsigned timeout)
{
    for (unsigned elapsed = 0U; elapsed < timeout; ++elapsed)
    {
        ++clock;
        server.Process(clock);
        client.Process(clock);
        if (complete())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

const r3d::game::originalnetwork::NetworkPlayerState* remotePlayer(
    const r3d::game::originalnetwork::NetworkModelSnapshot& snapshot,
    std::uint32_t ownerId)
{
    for (const auto& player : snapshot.players)
        if (player.ownerId == ownerId)
            return &player;
    return nullptr;
}

} // namespace

int main()
{
    using namespace r3d::game::originalnetwork;

    const unsigned port = reservePort();
    if (port == 0U)
        return 1;

    User serverUser;
    User clientUser;
    net::NetService server;
    net::NetService client;
    server.user(&serverUser);
    client.user(&clientUser);
    server.syncRate(1U);
    client.syncRate(1U);

    {
        OriginalNetworkModels serverModels(server);
        OriginalNetworkModels clientModels(client);
        server.Initializate();
        client.Initializate();
        server.StartServer(port, nullptr);

        std::string error;
        if (!serverModels.createHostRace(error))
        {
            std::cerr << error << '\n';
            return 2;
        }
        if (!client.Connect(net::Endpoint("127.0.0.1", port), nullptr))
            return 3;

        unsigned clock = 0U;
        if (!pump(server, client, [&]() {
                return client.isConnected() &&
                       clientModels.snapshot().raceModelPresent;
            }, clock, 3000U))
            return 4;

        NetworkMatchState match;
        match.mode = 1;
        match.upgradeMaxLevel = 3;
        match.weaponMaxLevel = 2;
        match.lapsCount = 4U;
        match.maxPlayers = 6U;
        match.maxComputers = 3U;
        match.springBorders = true;
        match.enableMineBug = true;
        match.planet = 2;
        match.track = 1;
        match.weather = 3;
        match.profileXml =
            "<?xml version=\"1.0\"?><profile dfficulty=\"normal\"/>";

        NetworkPlayerState hostPlayer;
        hostPlayer.gamerId = 7;
        hostPlayer.color = {0.1F, 0.2F, 0.3F, 1.0F};
        hostPlayer.car = "marauder";
        hostPlayer.money = 900;
        hostPlayer.slots[0].record = "marauderWheel";
        hostPlayer.slots[0].chargeCount = 1U;
        hostPlayer.vehicle.position = {12.0F, 3.0F, -8.0F};
        hostPlayer.vehicle.linearMomentum = {40.0F, 0.0F, 2.0F};
        hostPlayer.vehicle.moveState = 1U;

        if (!serverModels.startMatch(match, hostPlayer, error))
        {
            std::cerr << error << '\n';
            return 5;
        }
        if (!pump(server, client, [&]() {
                const auto serverState = serverModels.snapshot();
                const auto clientState = clientModels.snapshot();
                return serverState.matchActive && clientState.matchActive &&
                       serverState.players.size() == 2U &&
                       clientState.players.size() == 2U;
            }, clock, 4000U))
            return 6;

        const auto clientState = clientModels.snapshot();
        if (clientState.match.profileXml != match.profileXml ||
            clientState.match.lapsCount != 4U ||
            clientState.match.planet != 2 || clientState.match.track != 1)
            return 7;
        const auto* hostOnClient = remotePlayer(clientState, net::cServerPlayer);
        if (hostOnClient == nullptr || hostOnClient->car != "marauder" ||
            hostOnClient->gamerId != 7 ||
            hostOnClient->slots[0].record != "marauderWheel")
            return 8;

        NetworkPlayerState clientPlayer;
        clientPlayer.gamerId = 8;
        clientPlayer.color = {0.8F, 0.2F, 0.1F, 1.0F};
        clientPlayer.car = "buggi";
        clientPlayer.vehicle.position = {-2.0F, 1.0F, 4.0F};
        if (!clientModels.setLocalPlayerState(clientPlayer, error) ||
            !clientModels.setLocalPlayerReady(true, error))
            return 9;
        if (!serverModels.setPlanet(4, 0, 2, error) ||
            !serverModels.setTrack(2, error) ||
            !serverModels.startRace(error) ||
            !serverModels.setRaceGoStage(3, error) ||
            !serverModels.setPaused(true, error))
            return 10;

        if (!pump(server, client, [&]() {
                const auto serverState = serverModels.snapshot();
                const auto clientState2 = clientModels.snapshot();
                const auto* clientOnServer =
                    remotePlayer(serverState, net::cServerPlayer + 1U);
                const auto* host =
                    remotePlayer(clientState2, net::cServerPlayer);
                const auto replicatedComputers = std::count_if(
                    clientState2.players.begin(),
                    clientState2.players.end(),
                    [](const auto& player) {
                        return player.playerId >= 1U &&
                               player.playerId <= 5U;
                    });
                return clientOnServer != nullptr &&
                       clientOnServer->raceReady &&
                       clientOnServer->car == "buggi" &&
                       host != nullptr &&
                       host->vehicle.position[0] == 12.0F &&
                       replicatedComputers == 3 &&
                       clientState2.players.size() == 5U &&
                       clientState2.raceActive && clientState2.paused &&
                       clientState2.raceGoStage == 3 &&
                       clientState2.match.planet == 4 &&
                       clientState2.match.track == 2;
            }, clock, 4000U))
            return 11;

        client.Close();
        server.Close();
        client.Finalizate();
        server.Finalizate();
    }

    std::cout
        << "Original NetRace/NetPlayer class IDs, RPC order, match/state "
           "payloads, host-created AI and vehicle BitStream loopback passed\n";
    return 0;
}
