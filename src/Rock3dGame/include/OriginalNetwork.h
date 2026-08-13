#pragma once

#include "OriginalNetworkModels.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace r3d::game::originalnetwork
{

// NetGame.cpp uses this port for both its TCP listener and UDP LAN browser.
inline constexpr std::uint16_t defaultPort = 58213U;

enum class SessionState
{
    Stopped,
    Idle,
    Searching,
    Hosting,
    Connecting,
    Connected,
    Failed,
};

struct Endpoint
{
    std::string address;
    std::uint16_t port = defaultPort;

    bool operator==(const Endpoint& other) const noexcept
    {
        return address == other.address && port == other.port;
    }
};

struct SessionSnapshot
{
    SessionState state = SessionState::Stopped;
    std::vector<std::string> adapterAddresses;
    std::vector<Endpoint> discoveredHosts;
    std::uint32_t peerCount = 0U;
    std::uint32_t lastError = 0U;
    std::string lastErrorMessage;
    NetworkModelSnapshot models;
    std::uint64_t revision = 0U;
};

// Portable ownership/lifecycle boundary for the original Boost.Asio NetLib
// and its source-compatible NetRace/NetPlayer graph. A TCP handshake is not
// reported as a completed match until those replicated models exist.
class OriginalNetworkSession final
{
public:
    OriginalNetworkSession();
    ~OriginalNetworkSession();

    OriginalNetworkSession(const OriginalNetworkSession&) = delete;
    OriginalNetworkSession& operator=(const OriginalNetworkSession&) = delete;

    bool initialize(std::string& error);
    void finalize() noexcept;
    void process(std::uint32_t milliseconds);

    bool beginLanSearch(std::string& error);
    void cancelLanSearch() noexcept;
    bool createHost(std::string& error);
    bool connect(const Endpoint& endpoint, std::string& error);
    void close() noexcept;

    bool startMatch(const NetworkMatchState& match,
                    const NetworkPlayerState& localPlayer,
                    std::string& error);
    bool startRace(std::string& error);
    bool setRaceGoStage(std::int32_t stage, std::string& error);
    bool setLocalPlayerState(const NetworkPlayerState& state,
                             std::string& error);
    bool setLocalPlayerReady(bool ready, std::string& error);
    bool setLocalPlayerGoWait(bool waiting, std::string& error);
    bool setLocalPlayerFinished(bool finished, std::string& error);

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] SessionSnapshot snapshot() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] const char* stateName(SessionState state) noexcept;

} // namespace r3d::game::originalnetwork
