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

// The Windows UI has three distinct failure callbacks.  Keep that source
// distinction at the portable session boundary instead of forcing the menu
// to infer it from an operating-system error number.
enum class SessionFailure
{
    None,
    HostDisconnected,
    ConnectionFailed,
    Critical,
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
    SessionFailure failure = SessionFailure::None;
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
    void setGamerCatalog(std::vector<std::int32_t> gamerIds);

    bool beginLanSearch(std::string& error,
                        bool legacyWindowsDebug = false);
    void cancelLanSearch() noexcept;
    bool createHost(std::string& error);
    bool connect(const Endpoint& endpoint, std::string& error);
    void close() noexcept;

    bool startMatch(const NetworkMatchState& match,
                    const NetworkPlayerState& localPlayer,
                    std::string& error);
    bool exitMatch(std::string& error);
    bool startRace(std::string& error);
    bool exitRace(std::int32_t track, std::int32_t weather,
                  const std::vector<NetworkRaceResult>& results,
                  std::string& error);
    bool setRaceGoStage(std::int32_t stage, std::string& error);
    bool setPlanet(std::int32_t planet, std::int32_t track,
                   std::int32_t weather, std::string& error);
    bool setTrack(std::int32_t track, std::string& error);
    bool setUpgradeMaxLevel(std::int32_t level, std::string& error);
    bool setWeaponMaxLevel(std::int32_t level, std::string& error);
    bool setCurrentDifficulty(std::int32_t difficulty,
                              std::string& error);
    bool setLapsCount(std::uint32_t laps, std::string& error);
    bool setMaxPlayers(std::uint32_t players, std::string& error);
    bool setMaxComputers(std::uint32_t computers, std::string& error);
    bool setSpringBorders(bool enabled, std::string& error);
    bool setEnableMineBug(bool enabled, std::string& error);
    bool disconnectPlayer(std::uint32_t ownerId, std::string& error);
    bool sendPlayerDamage(std::uint32_t senderModelId,
                          std::uint32_t targetModelId, float value,
                          std::int32_t damageType, float targetLife,
                          bool death, std::string& error);
    bool sendMapObjectDamage(std::uint32_t senderModelId,
                             std::uint32_t targetObjectId, float value,
                             std::int32_t damageType, float targetLife,
                             bool death, std::string& error);
    bool pushLine(std::string_view text, std::string& error);
    bool setLocalPlayerState(const NetworkPlayerState& state,
                             std::string& error);
    bool setLocalPlayerGamerId(std::int32_t gamerId,
                               std::string& error);
    bool setLocalPlayerReady(bool ready, std::string& error);
    bool setLocalPlayerGoWait(bool waiting, std::string& error);
    bool setLocalPlayerFinished(bool finished, std::string& error);
    bool setOwnedPlayerFinished(std::uint32_t modelId, bool finished,
                                std::string& error);
    bool sendLocalShot(
        std::uint32_t targetObjectId, std::uint8_t slotMask,
        std::uint32_t projectileId,
        const std::vector<std::array<float, 3>>& coordinates,
        std::string& error);
    bool sendOwnedPlayerShot(
        std::uint32_t modelId, std::uint32_t targetObjectId,
        std::uint8_t slotMask, std::uint32_t projectileId,
        const std::vector<std::array<float, 3>>& coordinates,
        std::string& error);
    bool sendLocalBonus(std::uint32_t bonusObjectId,
                        std::int32_t bonusType, float value,
                        std::string& error);
    bool sendOwnedPlayerBonus(
        std::uint32_t modelId, std::uint32_t bonusObjectId,
        std::int32_t bonusType, float value, std::string& error);
    bool sendLocalMineContactPlayer(
        std::uint32_t projectileOwnerModelId,
        std::uint32_t projectileId,
        const std::array<float, 3>& point, std::string& error);
    bool sendOwnedPlayerMineContactPlayer(
        std::uint32_t modelId,
        std::uint32_t projectileOwnerModelId,
        std::uint32_t projectileId,
        const std::array<float, 3>& point, std::string& error);
    bool sendLocalMineContactMap(
        std::uint32_t projectileObjectId,
        const std::array<float, 3>& point, std::string& error);
    bool sendOwnedPlayerMineContactMap(
        std::uint32_t modelId, std::uint32_t projectileObjectId,
        const std::array<float, 3>& point, std::string& error);

    [[nodiscard]] bool hostGoWaitComplete() const noexcept;
    [[nodiscard]] bool hostRaceFinishComplete() const noexcept;
    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] SessionSnapshot snapshot() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] const char* stateName(SessionState state) noexcept;

} // namespace r3d::game::originalnetwork
