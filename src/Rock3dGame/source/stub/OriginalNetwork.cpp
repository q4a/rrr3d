#include "OriginalNetwork.h"

#include <net/NetLib.h>

#include <algorithm>
#include <limits>
#include <system_error>

namespace r3d::game::originalnetwork
{

namespace
{

std::string errorMessage(std::uint32_t value)
{
    if (value == 0U)
        return {};
    return std::system_category().message(static_cast<int>(value));
}

} // namespace

class OriginalNetworkSession::Impl final : public net::INetServiceUser
{
public:
    net::INetService& service = net::GetNetService();
    OriginalNetworkModels models{service};
    SessionSnapshot value;
    bool init = false;
    std::uint64_t modelRevision = 0U;

    void changed() noexcept
    {
        ++value.revision;
    }

    void clearError()
    {
        value.failure = SessionFailure::None;
        value.lastError = 0U;
        value.lastErrorMessage.clear();
    }

    void fail(std::uint32_t error, SessionFailure failure)
    {
        value.state = SessionState::Failed;
        value.failure = failure;
        value.lastError = error;
        value.lastErrorMessage = errorMessage(error);
        if (value.lastErrorMessage.empty())
            value.lastErrorMessage = "network operation failed";
        changed();
    }

    bool OnConnected(net::INetConnection*) override
    {
        if (service.isServer() && !models.acceptsConnections())
            return false;
        if (service.isClient())
            value.state = SessionState::Connected;
        value.peerCount = service.connectionCount();
        clearError();
        changed();
        return true;
    }

    void OnDisconnected(net::INetConnection*) override
    {
        value.peerCount = service.connectionCount();
        if (service.isClient())
        {
            value.state = SessionState::Failed;
            value.failure = SessionFailure::HostDisconnected;
            value.lastError = 0U;
            value.lastErrorMessage = "host disconnected";
        }
        changed();
    }

    void OnConnectionFailed(net::INetConnection*, unsigned error) override
    {
        fail(error, SessionFailure::ConnectionFailed);
    }

    void OnPingComplete() override
    {
        value.state = SessionState::Idle;
        changed();
    }

    void OnFailed(unsigned error) override
    {
        fail(error, SessionFailure::Critical);
    }

    void refreshAdapters()
    {
        lsl::StringVec source;
        value.adapterAddresses.clear();
        if (service.GetAdapterAddresses(source))
        {
            value.adapterAddresses.assign(source.begin(), source.end());
            std::sort(
                value.adapterAddresses.begin(),
                value.adapterAddresses.end());
            value.adapterAddresses.erase(
                std::unique(
                    value.adapterAddresses.begin(),
                    value.adapterAddresses.end()),
                value.adapterAddresses.end());
        }
    }

    void refreshEndpoints()
    {
        std::vector<Endpoint> endpoints;
        endpoints.reserve(service.endpointList().size());
        for (const auto& endpoint : service.endpointList())
        {
            if (endpoint.address.empty())
                continue;
            endpoints.push_back(
                {endpoint.address,
                 static_cast<std::uint16_t>(
                     std::min(
                         endpoint.port,
                         static_cast<unsigned>(
                             std::numeric_limits<std::uint16_t>::max())))});
        }
        if (endpoints != value.discoveredHosts)
        {
            value.discoveredHosts = std::move(endpoints);
            changed();
        }
    }

    void refreshModels()
    {
        auto snapshot = models.snapshot();
        if (snapshot.revision == modelRevision)
            return;
        modelRevision = snapshot.revision;
        value.models = std::move(snapshot);
        changed();
    }
};

OriginalNetworkSession::OriginalNetworkSession()
    : impl_(std::make_unique<Impl>())
{
}

OriginalNetworkSession::~OriginalNetworkSession()
{
    finalize();
}

bool OriginalNetworkSession::initialize(std::string& error)
{
    error.clear();
    if (impl_->init)
        return true;

    impl_->service.user(impl_.get());
    impl_->service.syncRate(70U);
    impl_->service.Initializate();
    if (!impl_->service.IsInit())
    {
        impl_->service.user(nullptr);
        error = "NetLib initialization failed";
        return false;
    }

    impl_->init = true;
    impl_->value.state = SessionState::Idle;
    impl_->clearError();
    impl_->refreshAdapters();
    impl_->refreshModels();
    impl_->changed();
    return true;
}

void OriginalNetworkSession::finalize() noexcept
{
    if (!impl_ || !impl_->init)
        return;
    impl_->service.user(nullptr);
    impl_->service.Finalizate();
    impl_->init = false;
    impl_->value = {};
}

void OriginalNetworkSession::process(std::uint32_t milliseconds)
{
    if (!impl_->init)
        return;
    impl_->service.Process(milliseconds);
    impl_->models.process(milliseconds);
    impl_->value.peerCount = impl_->service.connectionCount();
    impl_->refreshEndpoints();
    impl_->refreshModels();
}

void OriginalNetworkSession::setGamerCatalog(
    std::vector<std::int32_t> gamerIds)
{
    impl_->models.setGamerCatalog(std::move(gamerIds));
}

bool OriginalNetworkSession::beginLanSearch(
    std::string& error, bool legacyWindowsDebug)
{
    error.clear();
    if (!impl_->init)
    {
        error = "NetLib is not initialized";
        return false;
    }

    impl_->service.Close();
    impl_->refreshModels();
    impl_->clearError();
    impl_->value.discoveredHosts.clear();
    impl_->value.state = SessionState::Searching;
    // NetGame.cpp uses 500/250 under Windows _DEBUG and 3000/500 in the
    // shipped build. This must depend on the explicit compatibility mode,
    // not on whether the native macOS target itself was built with symbols.
    impl_->service.Ping(defaultPort,
                        legacyWindowsDebug ? 500U : 3000U,
                        legacyWindowsDebug ? 250U : 500U);
    impl_->changed();
    return true;
}

void OriginalNetworkSession::cancelLanSearch() noexcept
{
    if (!impl_->init)
        return;
    impl_->service.CancelPing();
    if (impl_->value.state == SessionState::Searching)
    {
        impl_->value.state = SessionState::Idle;
        impl_->changed();
    }
}

bool OriginalNetworkSession::createHost(std::string& error)
{
    error.clear();
    if (!impl_->init)
    {
        error = "NetLib is not initialized";
        return false;
    }

    impl_->service.CancelPing();
    impl_->service.Close();
    impl_->clearError();
    impl_->service.StartServer(defaultPort, nullptr);
    if (!impl_->service.isServer())
    {
        error = "NetLib did not create the LAN server";
        impl_->fail(0U, SessionFailure::Critical);
        return false;
    }
    if (!impl_->models.createHostRace(error))
    {
        impl_->service.Close();
        impl_->fail(0U, SessionFailure::Critical);
        return false;
    }
    impl_->value.state = SessionState::Hosting;
    impl_->value.peerCount = 0U;
    impl_->refreshModels();
    impl_->changed();
    return true;
}

bool OriginalNetworkSession::startMatch(
    const NetworkMatchState& match,
    const NetworkPlayerState& localPlayer,
    std::string& error)
{
    if (!impl_->init)
    {
        error = "NetLib is not initialized";
        return false;
    }
    if (!impl_->models.startMatch(match, localPlayer, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::exitMatch(std::string& error)
{
    if (!impl_->init)
    {
        error = "NetLib is not initialized";
        return false;
    }
    if (!impl_->models.exitMatch(error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::startRace(std::string& error)
{
    if (!impl_->models.startRace(error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::exitRace(
    std::int32_t track, std::int32_t weather,
    const std::vector<NetworkRaceResult>& results, std::string& error)
{
    if (!impl_->models.exitRace(track, weather, results, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::setRaceGoStage(
    std::int32_t stage, std::string& error)
{
    if (!impl_->models.setRaceGoStage(stage, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::setPlanet(
    std::int32_t planet, std::int32_t track,
    std::int32_t weather, std::string& error)
{
    if (!impl_->models.setPlanet(planet, track, weather, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::setTrack(
    std::int32_t track, std::string& error)
{
    if (!impl_->models.setTrack(track, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::setUpgradeMaxLevel(
    std::int32_t level, std::string& error)
{
    if (!impl_->models.setUpgradeMaxLevel(level, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::setWeaponMaxLevel(
    std::int32_t level, std::string& error)
{
    if (!impl_->models.setWeaponMaxLevel(level, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::setCurrentDifficulty(
    std::int32_t difficulty, std::string& error)
{
    if (!impl_->models.setCurrentDifficulty(difficulty, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::setLapsCount(
    std::uint32_t laps, std::string& error)
{
    if (!impl_->models.setLapsCount(laps, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::setMaxPlayers(
    std::uint32_t players, std::string& error)
{
    if (!impl_->models.setMaxPlayers(players, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::setMaxComputers(
    std::uint32_t computers, std::string& error)
{
    if (!impl_->models.setMaxComputers(computers, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::setSpringBorders(
    bool enabled, std::string& error)
{
    if (!impl_->models.setSpringBorders(enabled, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::setEnableMineBug(
    bool enabled, std::string& error)
{
    if (!impl_->models.setEnableMineBug(enabled, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::disconnectPlayer(
    std::uint32_t ownerId, std::string& error)
{
    error.clear();
    if (!impl_->init)
    {
        error = "NetLib is not initialized";
        return false;
    }
    if (!impl_->service.isServer())
    {
        error = "only the NetRace host can disconnect players";
        return false;
    }
    auto* connection = impl_->service.GetConnectionById(ownerId);
    if (connection == nullptr)
    {
        error = "source NetPlayer connection is not active";
        return false;
    }

    // NetGame::DisconnectPlayer resolves the peer through NetPlayer::ownerId
    // and asks the original NetService to close that exact connection.
    impl_->service.Disconnect(connection);
    impl_->value.peerCount = impl_->service.connectionCount();
    impl_->refreshModels();
    impl_->changed();
    return true;
}

bool OriginalNetworkSession::sendPlayerDamage(
    std::uint32_t senderModelId, std::uint32_t targetModelId,
    float value, std::int32_t damageType, float targetLife,
    bool death, std::string& error)
{
    if (!impl_->models.sendPlayerDamage(
            senderModelId, targetModelId, value, damageType,
            targetLife, death, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::sendMapObjectDamage(
    std::uint32_t senderModelId, std::uint32_t targetObjectId,
    float value, std::int32_t damageType, float targetLife,
    bool death, std::string& error)
{
    if (!impl_->models.sendMapObjectDamage(
            senderModelId, targetObjectId, value, damageType,
            targetLife, death, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::pushLine(
    std::string_view text, std::string& error)
{
    if (!impl_->models.pushLine(text, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::setLocalPlayerState(
    const NetworkPlayerState& state, std::string& error)
{
    if (!impl_->models.setLocalPlayerState(state, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::setLocalPlayerGamerId(
    std::int32_t gamerId, std::string& error)
{
    if (!impl_->models.setLocalPlayerGamerId(gamerId, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::setLocalPlayerReady(
    bool ready, std::string& error)
{
    if (!impl_->models.setLocalPlayerReady(ready, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::setLocalPlayerGoWait(
    bool waiting, std::string& error)
{
    if (!impl_->models.setLocalPlayerGoWait(waiting, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::setLocalPlayerFinished(
    bool finished, std::string& error)
{
    if (!impl_->models.setLocalPlayerFinished(finished, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::setOwnedPlayerFinished(
    std::uint32_t modelId, bool finished, std::string& error)
{
    if (!impl_->models.setOwnedPlayerFinished(
            modelId, finished, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::sendLocalShot(
    std::uint32_t targetObjectId, std::uint8_t slotMask,
    std::uint32_t projectileId,
    const std::vector<std::array<float, 3>>& coordinates,
    std::string& error)
{
    if (!impl_->models.sendLocalShot(
            targetObjectId, slotMask, projectileId, coordinates,
            error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::sendOwnedPlayerShot(
    std::uint32_t modelId, std::uint32_t targetObjectId,
    std::uint8_t slotMask, std::uint32_t projectileId,
    const std::vector<std::array<float, 3>>& coordinates,
    std::string& error)
{
    if (!impl_->models.sendOwnedPlayerShot(
            modelId, targetObjectId, slotMask, projectileId,
            coordinates, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::sendLocalBonus(
    std::uint32_t bonusObjectId, std::int32_t bonusType, float value,
    std::string& error)
{
    if (!impl_->models.sendLocalBonus(
            bonusObjectId, bonusType, value, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::sendOwnedPlayerBonus(
    std::uint32_t modelId, std::uint32_t bonusObjectId,
    std::int32_t bonusType, float value, std::string& error)
{
    if (!impl_->models.sendOwnedPlayerBonus(
            modelId, bonusObjectId, bonusType, value, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::sendLocalMineContactPlayer(
    std::uint32_t projectileOwnerModelId,
    std::uint32_t projectileId,
    const std::array<float, 3>& point, std::string& error)
{
    if (!impl_->models.sendLocalMineContactPlayer(
            projectileOwnerModelId, projectileId, point, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::sendOwnedPlayerMineContactPlayer(
    std::uint32_t modelId, std::uint32_t projectileOwnerModelId,
    std::uint32_t projectileId,
    const std::array<float, 3>& point, std::string& error)
{
    if (!impl_->models.sendOwnedPlayerMineContactPlayer(
            modelId, projectileOwnerModelId, projectileId, point,
            error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::sendLocalMineContactMap(
    std::uint32_t projectileObjectId,
    const std::array<float, 3>& point, std::string& error)
{
    if (!impl_->models.sendLocalMineContactMap(
            projectileObjectId, point, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::sendOwnedPlayerMineContactMap(
    std::uint32_t modelId, std::uint32_t projectileObjectId,
    const std::array<float, 3>& point, std::string& error)
{
    if (!impl_->models.sendOwnedPlayerMineContactMap(
            modelId, projectileObjectId, point, error))
        return false;
    impl_->refreshModels();
    return true;
}

bool OriginalNetworkSession::hostGoWaitComplete() const noexcept
{
    return impl_->models.hostGoWaitComplete();
}

bool OriginalNetworkSession::hostRaceFinishComplete() const noexcept
{
    return impl_->models.hostRaceFinishComplete();
}

bool OriginalNetworkSession::connect(
    const Endpoint& endpoint, std::string& error)
{
    error.clear();
    if (!impl_->init)
    {
        error = "NetLib is not initialized";
        return false;
    }
    if (endpoint.address.empty())
    {
        error = "host address is empty";
        return false;
    }

    impl_->service.CancelPing();
    impl_->service.Close();
    impl_->refreshModels();
    impl_->clearError();
    const unsigned port = endpoint.port == 0U
                              ? defaultPort
                              : endpoint.port;
    if (!impl_->service.Connect(
            net::Endpoint(endpoint.address, port), nullptr))
    {
        error = "NetLib rejected the host endpoint";
        impl_->fail(0U, SessionFailure::ConnectionFailed);
        return false;
    }
    impl_->value.state = SessionState::Connecting;
    impl_->value.peerCount = 0U;
    impl_->changed();
    return true;
}

void OriginalNetworkSession::close() noexcept
{
    if (!impl_->init)
        return;
    impl_->service.CancelPing();
    impl_->service.Close();
    impl_->refreshModels();
    impl_->value.state = SessionState::Idle;
    impl_->value.peerCount = 0U;
    impl_->clearError();
    impl_->changed();
}

bool OriginalNetworkSession::initialized() const noexcept
{
    return impl_->init;
}

SessionSnapshot OriginalNetworkSession::snapshot() const
{
    return impl_->value;
}

const char* stateName(SessionState state) noexcept
{
    switch (state)
    {
    case SessionState::Stopped:
        return "stopped";
    case SessionState::Idle:
        return "idle";
    case SessionState::Searching:
        return "searching";
    case SessionState::Hosting:
        return "hosting";
    case SessionState::Connecting:
        return "connecting";
    case SessionState::Connected:
        return "connected";
    case SessionState::Failed:
        return "failed";
    }
    return "unknown";
}

} // namespace r3d::game::originalnetwork
