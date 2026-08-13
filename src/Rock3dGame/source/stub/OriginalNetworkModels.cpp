#include "OriginalNetworkModels.h"

#include <net/NetLib.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <istream>
#include <iterator>
#include <map>
#include <ostream>
#include <stdexcept>
#include <utility>

namespace r3d::game::originalnetwork
{

namespace
{

constexpr std::size_t maximumCommandBytes = 4095U;
constexpr std::size_t fixedMatchBytes = 38U;
constexpr std::size_t maximumStringBytes = 1024U * 1024U;
constexpr std::size_t maximumRememberedEvents = 128U;

static_assert(std::endian::native == std::endian::little,
              "The original Motor Rock network protocol is little-endian");

class PortableNetRace;
class PortableNetPlayer;

constexpr std::array<std::array<float, 4>, 14> sourcePlayerColors{{
    {1.0F, 1.0F, 1.0F, 1.0F},
    {0.0F, 0.0F, 1.0F, 1.0F},
    {1.0F, 0.0F, 0.0F, 1.0F},
    {0.0F, 1.0F, 0.0F, 1.0F},
    {1.0F, 1.0F, 0.0F, 1.0F},
    {1.0F, 144.0F / 255.0F, 0.0F, 1.0F},
    {51.0F / 255.0F, 51.0F / 255.0F, 51.0F / 255.0F, 1.0F},
    {6.0F / 255.0F, 175.0F / 255.0F, 250.0F / 255.0F, 1.0F},
    {183.0F / 255.0F, 11.0F / 255.0F, 174.0F / 255.0F, 1.0F},
    {177.0F / 255.0F, 201.0F / 255.0F, 3.0F / 255.0F, 1.0F},
    {169.0F / 255.0F, 57.0F / 255.0F, 0.0F, 1.0F},
    {48.0F / 255.0F, 55.0F / 255.0F, 61.0F / 255.0F, 1.0F},
    {0.0F, 159.0F / 255.0F, 21.0F / 255.0F, 1.0F},
    {112.0F / 255.0F, 118.0F / 255.0F, 156.0F / 255.0F, 1.0F},
}};

std::map<net::INetService*, OriginalNetworkModels::Impl*>& contexts()
{
    static std::map<net::INetService*, OriginalNetworkModels::Impl*> values;
    return values;
}

OriginalNetworkModels::Impl* findContext(net::INetService* service) noexcept
{
    const auto found = contexts().find(service);
    return found == contexts().end() ? nullptr : found->second;
}

OriginalNetworkModels::Impl& contextFor(net::INetService* service)
{
    auto* context = findContext(service);
    if (context == nullptr)
        throw std::runtime_error("original network model context is absent");
    return *context;
}

bool readBytes(std::istream& stream, void* value, std::size_t size)
{
    stream.read(static_cast<char*>(value),
                static_cast<std::streamsize>(size));
    return stream.good() ||
           stream.gcount() == static_cast<std::streamsize>(size);
}

template <class T>
bool readScalar(std::istream& stream, T& value)
{
    return readBytes(stream, &value, sizeof(value));
}

template <class T>
void writeScalar(std::ostream& stream, const T& value)
{
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

bool readString(std::istream& stream, std::string& value)
{
    std::uint32_t size = 0U;
    if (!readScalar(stream, size) || size > maximumStringBytes)
        return false;
    value.resize(size);
    return size == 0U || readBytes(stream, value.data(), size);
}

void writeString(std::ostream& stream, const std::string& value)
{
    const auto size = static_cast<std::uint32_t>(value.size());
    writeScalar(stream, size);
    stream.write(value.data(), static_cast<std::streamsize>(value.size()));
}

std::string remainingString(std::istream& stream)
{
    return {std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
}

bool readMatch(std::istream& stream, NetworkMatchState& value)
{
    return readScalar(stream, value.mode) &&
           readScalar(stream, value.upgradeMaxLevel) &&
           readScalar(stream, value.weaponMaxLevel) &&
           readScalar(stream, value.lapsCount) &&
           readScalar(stream, value.maxPlayers) &&
           readScalar(stream, value.maxComputers) &&
           readScalar(stream, value.springBorders) &&
           readScalar(stream, value.enableMineBug) &&
           readScalar(stream, value.planet) &&
           readScalar(stream, value.track) &&
           readScalar(stream, value.weather) &&
           ((value.profileXml = remainingString(stream)), true);
}

void writeMatch(std::ostream& stream, const NetworkMatchState& value)
{
    writeScalar(stream, value.mode);
    writeScalar(stream, value.upgradeMaxLevel);
    writeScalar(stream, value.weaponMaxLevel);
    writeScalar(stream, value.lapsCount);
    writeScalar(stream, value.maxPlayers);
    writeScalar(stream, value.maxComputers);
    writeScalar(stream, value.springBorders);
    writeScalar(stream, value.enableMineBug);
    writeScalar(stream, value.planet);
    writeScalar(stream, value.track);
    writeScalar(stream, value.weather);
    stream.write(value.profileXml.data(),
                 static_cast<std::streamsize>(value.profileXml.size()));
}

bool readSlots(std::istream& stream, NetworkPlayerState& value)
{
    for (auto& slot : value.slots)
    {
        if (!readString(stream, slot.record) ||
            !readScalar(stream, slot.chargeCount) ||
            !readScalar(stream, slot.armor4))
            return false;
    }
    return true;
}

void writeSlots(std::ostream& stream, const NetworkPlayerState& value)
{
    for (const auto& slot : value.slots)
    {
        writeString(stream, slot.record);
        writeScalar(stream, slot.chargeCount);
        writeScalar(stream, slot.armor4);
    }
}

bool readPlayerState(std::istream& stream, NetworkPlayerState& value)
{
    return readBytes(stream, value.color.data(), sizeof(float) * 4U) &&
           readString(stream, value.car) &&
           readScalar(stream, value.raceReady) &&
           readScalar(stream, value.raceGoWait) &&
           readScalar(stream, value.raceFinish) &&
           readScalar(stream, value.gamerId) && readSlots(stream, value);
}

void writePlayerState(std::ostream& stream,
                      const NetworkPlayerState& value)
{
    stream.write(reinterpret_cast<const char*>(value.color.data()),
                 sizeof(float) * 4U);
    writeString(stream, value.car);
    writeScalar(stream, value.raceReady);
    writeScalar(stream, value.raceGoWait);
    writeScalar(stream, value.raceFinish);
    writeScalar(stream, value.gamerId);
    writeSlots(stream, value);
}

bool readPlayerDescriptor(std::istream& stream, std::uint8_t& playerId,
                          std::uint32_t& netSlot)
{
    std::array<std::uint8_t, 8> bytes{};
    if (!readBytes(stream, bytes.data(), bytes.size()))
        return false;
    playerId = bytes[0];
    std::memcpy(&netSlot, bytes.data() + 4U, sizeof(netSlot));
    return true;
}

void writePlayerDescriptor(std::ostream& stream, std::uint8_t playerId,
                           std::uint32_t netSlot)
{
    // MSVC and Apple Clang both place the unsigned member at offset four.
    // Emit the padding explicitly so it cannot contain indeterminate bytes.
    std::array<std::uint8_t, 8> bytes{};
    bytes[0] = playerId;
    std::memcpy(bytes.data() + 4U, &netSlot, sizeof(netSlot));
    stream.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

std::string utf16ToUtf8(const std::vector<std::uint16_t>& input)
{
    std::string result;
    for (std::size_t index = 0; index < input.size(); ++index)
    {
        std::uint32_t code = input[index];
        if (code >= 0xd800U && code <= 0xdbffU &&
            index + 1U < input.size())
        {
            const auto low = input[index + 1U];
            if (low >= 0xdc00U && low <= 0xdfffU)
            {
                code = 0x10000U + ((code - 0xd800U) << 10U) +
                       (low - 0xdc00U);
                ++index;
            }
        }
        if (code <= 0x7fU)
            result.push_back(static_cast<char>(code));
        else if (code <= 0x7ffU)
        {
            result.push_back(static_cast<char>(0xc0U | (code >> 6U)));
            result.push_back(static_cast<char>(0x80U | (code & 0x3fU)));
        }
        else if (code <= 0xffffU)
        {
            result.push_back(static_cast<char>(0xe0U | (code >> 12U)));
            result.push_back(
                static_cast<char>(0x80U | ((code >> 6U) & 0x3fU)));
            result.push_back(static_cast<char>(0x80U | (code & 0x3fU)));
        }
        else
        {
            result.push_back(static_cast<char>(0xf0U | (code >> 18U)));
            result.push_back(
                static_cast<char>(0x80U | ((code >> 12U) & 0x3fU)));
            result.push_back(
                static_cast<char>(0x80U | ((code >> 6U) & 0x3fU)));
            result.push_back(static_cast<char>(0x80U | (code & 0x3fU)));
        }
    }
    return result;
}

bool readWindowsWideString(std::istream& stream, std::string& value)
{
    std::uint32_t count = 0U;
    if (!readScalar(stream, count) || count > maximumStringBytes / 2U)
        return false;
    std::vector<std::uint16_t> units(count);
    if (count > 0U &&
        !readBytes(stream, units.data(), units.size() * sizeof(units[0])))
        return false;
    value = utf16ToUtf8(units);
    return true;
}

std::vector<std::uint16_t> utf8ToUtf16(std::string_view input)
{
    std::vector<std::uint16_t> result;
    for (std::size_t index = 0U; index < input.size();)
    {
        const auto first = static_cast<std::uint8_t>(input[index++]);
        std::uint32_t code = first;
        unsigned continuation = 0U;
        if ((first & 0xe0U) == 0xc0U)
        {
            code = first & 0x1fU;
            continuation = 1U;
        }
        else if ((first & 0xf0U) == 0xe0U)
        {
            code = first & 0x0fU;
            continuation = 2U;
        }
        else if ((first & 0xf8U) == 0xf0U)
        {
            code = first & 0x07U;
            continuation = 3U;
        }
        for (unsigned part = 0U;
             part < continuation && index < input.size(); ++part)
        {
            const auto next =
                static_cast<std::uint8_t>(input[index++]);
            code = (code << 6U) | (next & 0x3fU);
        }
        if (code <= 0xffffU)
        {
            if (code >= 0xd800U && code <= 0xdfffU)
                code = 0xfffdU;
            result.push_back(static_cast<std::uint16_t>(code));
        }
        else if (code <= 0x10ffffU)
        {
            code -= 0x10000U;
            result.push_back(static_cast<std::uint16_t>(
                0xd800U | (code >> 10U)));
            result.push_back(static_cast<std::uint16_t>(
                0xdc00U | (code & 0x3ffU)));
        }
        else
        {
            result.push_back(0xfffdU);
        }
    }
    return result;
}

unsigned setBitCount(std::uint8_t value)
{
    unsigned result = 0U;
    for (unsigned bit = 0U; bit < 6U; ++bit)
        result += (value >> bit) & 1U;
    return result;
}

} // namespace

class OriginalNetworkModels::Impl
{
public:
    explicit Impl(net::INetService& source);

    ~Impl();

    net::INetService& service;
    NetworkModelSnapshot value;
    PortableNetRace* race = nullptr;
    std::map<std::uint32_t, PortableNetPlayer*> players;
    std::vector<std::int32_t> gamerCatalog;
    std::uint64_t nextEventSequence = 1U;

    void touch() noexcept { ++value.revision; }

    void event(NetworkEvent event)
    {
        event.sequence = nextEventSequence++;
        if (value.events.size() == maximumRememberedEvents)
            value.events.erase(value.events.begin());
        value.events.push_back(std::move(event));
        touch();
    }

    void registerRace(PortableNetRace* model)
    {
        race = model;
        value.raceModelPresent = true;
        touch();
    }

    void unregisterRace(PortableNetRace* model)
    {
        if (race != model)
            return;
        race = nullptr;
        value.raceModelPresent = false;
        value.matchActive = false;
        value.raceActive = false;
        touch();
    }

    void registerPlayer(PortableNetPlayer* model, NetworkPlayerState state);
    void unregisterPlayer(PortableNetPlayer* model);
    void updatePlayer(const NetworkPlayerState& state);
    PortableNetPlayer* localPlayer() const;
    std::int32_t generateGamerId(std::uint32_t modelId) const;
    std::array<float, 4> generateColor(std::uint32_t modelId) const;
    bool makePlayer(std::uint8_t playerId, std::uint32_t netSlot,
                    std::string& error);
    bool makeHuman(std::string& error);
    bool makeComputers(std::string& error);
    void beginRace();
    void clearPlayersLocally(net::INetPlayer& player);

    void applyMatch(NetworkMatchState match, std::uint32_t sender)
    {
        const bool newMatch = !value.matchActive;
        value.match = std::move(match);
        value.matchActive = true;
        value.raceActive = false;
        value.paused = false;
        value.raceGoStage = -1;
        if (newMatch)
            value.currentDifficultySet = false;
        value.results.clear();
        event({NetworkEventKind::MatchStarted, sender});
    }
};

namespace
{

class PortableNetRace final : public net::NetModelRPC<PortableNetRace>
{
public:
    explicit PortableNetRace(const Desc& desc)
        : NetModelRPC(desc), context_(contextFor(desc.player->net())),
          service_(desc.player->net())
    {
        RegRPC(&PortableNetRace::OnStartMatch);
        RegRPC(&PortableNetRace::OnExitMatch);
        RegRPC(&PortableNetRace::OnSetPlanet);
        RegRPC(&PortableNetRace::OnSetTrack);
        RegRPC(&PortableNetRace::OnStartRace);
        RegRPC(&PortableNetRace::OnExitRace);
        RegRPC(&PortableNetRace::OnRaceGo);
        RegRPC(&PortableNetRace::OnPause);
        RegRPC(&PortableNetRace::OnDamagePlayer);
        RegRPC(&PortableNetRace::OnDamageMapObject);
        RegRPC(&PortableNetRace::OnSetUpgradeMaxLevel);
        RegRPC(&PortableNetRace::OnSetWeaponMaxLevel);
        RegRPC(&PortableNetRace::OnSetCurrentDifficulty);
        RegRPC(&PortableNetRace::OnSetLapsCount);
        RegRPC(&PortableNetRace::OnSetMaxPlayers);
        RegRPC(&PortableNetRace::OnSetMaxComputers);
        RegRPC(&PortableNetRace::OnSetSpringBorders);
        RegRPC(&PortableNetRace::OnSetEnableMineBug);
        RegRPC(&PortableNetRace::OnPushLine);
        context_.registerRace(this);
    }

    ~PortableNetRace() override
    {
        if (auto* context = findContext(service_))
            context->unregisterRace(this);
    }

    bool sendStartMatch(const NetworkMatchState& match,
                        const NetworkPlayerState& local,
                        std::string& error)
    {
        if (fixedMatchBytes + match.profileXml.size() > maximumCommandBytes)
        {
            error = "source NetRace match payload exceeds 4095 bytes";
            return false;
        }
        context_.applyMatch(match, player()->id());
        std::ostream& stream =
            NewRPC(net::cNetTargetOthers, &PortableNetRace::OnStartMatch);
        writeMatch(stream, match);
        CloseRPC();
        if (!context_.makeHuman(error))
            return false;
        if (auto* model = context_.localPlayer())
            applyLocalState(*model, local);
        return true;
    }

    bool makeHuman(std::string& error)
    {
        return context_.makeHuman(error);
    }

    void sendExitMatch()
    {
        // NetRace::DoExitMatch copies NetGame::_players and deletes every
        // NetPlayer locally before GameMode::ExitMatch. Deleting from the
        // live map directly would invalidate the model destructors' walk.
        context_.clearPlayersLocally(*player());
        context_.value.matchActive = false;
        context_.value.raceActive = false;
        context_.value.paused = false;
        context_.value.raceGoStage = -1;
        context_.value.results.clear();
        context_.event({NetworkEventKind::MatchExited, player()->id()});
        MakeRPC(net::cNetTargetOthers, &PortableNetRace::OnExitMatch);
    }

    bool sendStartRace(std::string& error)
    {
        if (!context_.makeComputers(error))
            return false;
        context_.beginRace();
        context_.value.raceActive = true;
        context_.event({NetworkEventKind::RaceStarted, player()->id()});
        MakeRPC(net::cNetTargetOthers, &PortableNetRace::OnStartRace);
        return true;
    }

    void sendExitRace(
        std::int32_t track, std::int32_t weather,
        const std::vector<NetworkRaceResult>& results)
    {
        context_.value.match.track = track;
        context_.value.match.weather = weather;
        context_.value.results = results;
        context_.value.raceActive = false;
        context_.value.raceGoStage = -1;
        context_.event({NetworkEventKind::RaceExited, player()->id()});
        std::ostream& stream =
            NewRPC(net::cNetTargetOthers, &PortableNetRace::OnExitRace);
        writeScalar(stream, track);
        writeScalar(stream, weather);
        writeScalar(stream, static_cast<std::uint32_t>(results.size()));
        for (const auto& result : results)
        {
            writeScalar(stream, result.playerModelId);
            writeScalar(stream, result.playerPoints);
            writeScalar(stream, result.playerMoney);
            writeScalar(stream, result.money);
            writeScalar(stream, result.pickedMoney);
            writeScalar(stream, result.place);
            writeScalar(stream, result.points);
            writeScalar(stream, result.voiceNameDuration);
        }
        CloseRPC();
    }

    void sendRaceGo(std::int32_t stage)
    {
        context_.value.raceGoStage = stage;
        NetworkEvent event{NetworkEventKind::RaceGo, player()->id()};
        event.intValue = stage;
        context_.event(std::move(event));
        std::ostream& stream =
            NewRPC(net::cNetTargetOthers, &PortableNetRace::OnRaceGo);
        writeScalar(stream, stage);
        CloseRPC();
    }

    void sendPlanet(std::int32_t planet, std::int32_t track,
                    std::int32_t weather)
    {
        context_.value.match.planet = planet;
        context_.value.match.track = track;
        context_.value.match.weather = weather;
        context_.touch();
        std::ostream& stream =
            NewRPC(net::cNetTargetOthers, &PortableNetRace::OnSetPlanet);
        writeScalar(stream, planet);
        writeScalar(stream, track);
        writeScalar(stream, weather);
        CloseRPC();
    }

    void sendTrack(std::int32_t track)
    {
        context_.value.match.track = track;
        context_.touch();
        std::ostream& stream =
            NewRPC(net::cNetTargetOthers, &PortableNetRace::OnSetTrack);
        writeScalar(stream, track);
        CloseRPC();
    }

    void sendUpgradeMaxLevel(std::int32_t level)
    {
        if (context_.value.match.upgradeMaxLevel == level)
            return;
        context_.value.match.upgradeMaxLevel = level;
        context_.touch();
        std::ostream& stream = NewRPC(
            net::cNetTargetOthers,
            &PortableNetRace::OnSetUpgradeMaxLevel);
        writeScalar(stream, level);
        CloseRPC();
    }

    void sendWeaponMaxLevel(std::int32_t level)
    {
        if (context_.value.match.weaponMaxLevel == level)
            return;
        context_.value.match.weaponMaxLevel = level;
        context_.touch();
        std::ostream& stream = NewRPC(
            net::cNetTargetOthers,
            &PortableNetRace::OnSetWeaponMaxLevel);
        writeScalar(stream, level);
        CloseRPC();
    }

    void sendCurrentDifficulty(std::int32_t difficulty)
    {
        if (context_.value.currentDifficulty == difficulty)
        {
            if (context_.value.currentDifficultySet)
                return;
        }
        context_.value.currentDifficulty = difficulty;
        context_.value.currentDifficultySet = true;
        context_.touch();
        std::ostream& stream = NewRPC(
            net::cNetTargetOthers,
            &PortableNetRace::OnSetCurrentDifficulty);
        writeScalar(stream, difficulty);
        CloseRPC();
    }

    void sendLapsCount(std::uint32_t laps)
    {
        if (context_.value.match.lapsCount == laps)
            return;
        context_.value.match.lapsCount = laps;
        context_.touch();
        std::ostream& stream = NewRPC(
            net::cNetTargetOthers,
            &PortableNetRace::OnSetLapsCount);
        writeScalar(stream, laps);
        CloseRPC();
    }

    void sendMaxPlayers(std::uint32_t players)
    {
        if (context_.value.match.maxPlayers == players)
            return;
        context_.value.match.maxPlayers = players;
        context_.touch();
        std::ostream& stream = NewRPC(
            net::cNetTargetOthers,
            &PortableNetRace::OnSetMaxPlayers);
        writeScalar(stream, players);
        CloseRPC();
    }

    void sendMaxComputers(std::uint32_t computers)
    {
        if (context_.value.match.maxComputers == computers)
            return;
        context_.value.match.maxComputers = computers;
        context_.touch();
        std::ostream& stream = NewRPC(
            net::cNetTargetOthers,
            &PortableNetRace::OnSetMaxComputers);
        writeScalar(stream, computers);
        CloseRPC();
    }

    void sendSpringBorders(bool enabled)
    {
        if (context_.value.match.springBorders == enabled)
            return;
        context_.value.match.springBorders = enabled;
        context_.touch();
        std::ostream& stream = NewRPC(
            net::cNetTargetOthers,
            &PortableNetRace::OnSetSpringBorders);
        writeScalar(stream, enabled);
        CloseRPC();
    }

    void sendEnableMineBug(bool enabled)
    {
        if (context_.value.match.enableMineBug == enabled)
            return;
        context_.value.match.enableMineBug = enabled;
        context_.touch();
        std::ostream& stream = NewRPC(
            net::cNetTargetOthers,
            &PortableNetRace::OnSetEnableMineBug);
        writeScalar(stream, enabled);
        CloseRPC();
    }

    void sendDamage(
        NetworkEventKind kind, std::uint32_t senderModelId,
        std::uint32_t targetId, float value, std::int32_t damageType,
        float targetLife, bool death)
    {
        const unsigned target = context_.service.isClient()
                                    ? net::cNetTargetAll
                                    : net::cNetTargetOthers;
        std::ostream& stream =
            kind == NetworkEventKind::PlayerDamage
                ? NewRPC(target, &PortableNetRace::OnDamagePlayer)
                : NewRPC(target, &PortableNetRace::OnDamageMapObject);
        writeScalar(stream, senderModelId);
        writeScalar(stream, targetId);
        writeScalar(stream, value);
        writeScalar(stream, damageType);
        writeScalar(stream, targetLife);
        writeScalar(
            stream, static_cast<std::uint8_t>(death ? 1U : 0U));
        CloseRPC();
    }

    bool sendLine(std::string_view text)
    {
        const auto units = utf8ToUtf16(text);
        if (sizeof(std::uint32_t) +
                units.size() * sizeof(std::uint16_t) >
            maximumCommandBytes)
            return false;
        std::ostream& stream =
            NewRPC(net::cNetTargetOthers, &PortableNetRace::OnPushLine);
        writeScalar(stream, static_cast<std::uint32_t>(units.size()));
        if (!units.empty())
        {
            stream.write(
                reinterpret_cast<const char*>(units.data()),
                static_cast<std::streamsize>(
                    units.size() * sizeof(units.front())));
        }
        CloseRPC();
        return true;
    }

protected:
    void OnDescWrite(const net::NetMessage&, std::ostream&) override {}

    void OnStateRead(const net::NetMessage& msg,
                     const net::NetCmdHeader&, std::istream& stream) override
    {
        NetworkMatchState match;
        if (!readMatch(stream, match))
            return;
        context_.applyMatch(std::move(match), msg.sender);
        std::string ignored;
        context_.makeHuman(ignored);
    }

    void OnStateWrite(const net::NetMessage&, std::ostream& stream) override
    {
        if (context_.value.matchActive)
            writeMatch(stream, context_.value.match);
    }

private:
    OriginalNetworkModels::Impl& context_;
    net::INetService* service_ = nullptr;

    static void applyLocalState(PortableNetPlayer& model,
                                const NetworkPlayerState& state);

    void OnStartMatch(const net::NetMessage& msg,
                      const net::NetCmdHeader&, std::istream& stream)
    {
        NetworkMatchState match;
        if (!readMatch(stream, match))
            return;
        context_.applyMatch(std::move(match), msg.sender);
        std::string ignored;
        context_.makeHuman(ignored);
    }

    void OnExitMatch(const net::NetMessage& msg,
                     const net::NetCmdHeader&, std::istream&)
    {
        // The Windows host consumes a client's ExitMatch instead of
        // forwarding it, then closes the match for all local players.
        if (context_.service.isServer())
            msg.Discard();
        context_.clearPlayersLocally(*player());
        context_.value.matchActive = false;
        context_.value.raceActive = false;
        context_.value.paused = false;
        context_.value.raceGoStage = -1;
        context_.value.results.clear();
        context_.event({NetworkEventKind::MatchExited, msg.sender});
    }

    void OnSetPlanet(const net::NetMessage&, const net::NetCmdHeader&,
                     std::istream& stream)
    {
        if (readScalar(stream, context_.value.match.planet) &&
            readScalar(stream, context_.value.match.track) &&
            readScalar(stream, context_.value.match.weather))
            context_.touch();
    }

    void OnSetTrack(const net::NetMessage&, const net::NetCmdHeader&,
                    std::istream& stream)
    {
        if (readScalar(stream, context_.value.match.track))
            context_.touch();
    }

    void OnStartRace(const net::NetMessage& msg,
                     const net::NetCmdHeader&, std::istream&)
    {
        context_.beginRace();
        context_.value.raceActive = true;
        context_.event({NetworkEventKind::RaceStarted, msg.sender});
    }

    void OnExitRace(const net::NetMessage& msg,
                    const net::NetCmdHeader&, std::istream& stream)
    {
        std::int32_t track = 0;
        std::int32_t weather = 0;
        std::uint32_t count = 0U;
        if (!readScalar(stream, track) || !readScalar(stream, weather) ||
            !readScalar(stream, count) || count > 64U)
            return;
        std::vector<NetworkRaceResult> results(count);
        for (auto& result : results)
        {
            if (!readScalar(stream, result.playerModelId) ||
                !readScalar(stream, result.playerPoints) ||
                !readScalar(stream, result.playerMoney) ||
                !readScalar(stream, result.money) ||
                !readScalar(stream, result.pickedMoney) ||
                !readScalar(stream, result.place) ||
                !readScalar(stream, result.points) ||
                !readScalar(stream, result.voiceNameDuration))
                return;
        }
        context_.value.match.track = track;
        context_.value.match.weather = weather;
        context_.value.results = std::move(results);
        context_.value.raceActive = false;
        context_.value.raceGoStage = -1;
        context_.event({NetworkEventKind::RaceExited, msg.sender});
    }

    void OnRaceGo(const net::NetMessage& msg, const net::NetCmdHeader&,
                  std::istream& stream)
    {
        std::int32_t stage = -1;
        if (!readScalar(stream, stage))
            return;
        context_.value.raceGoStage = stage;
        NetworkEvent event{NetworkEventKind::RaceGo, msg.sender};
        event.intValue = stage;
        context_.event(std::move(event));
    }

    void OnPause(const net::NetMessage& msg, const net::NetCmdHeader&,
                 std::istream& stream)
    {
        bool paused = false;
        if (!readScalar(stream, paused))
            return;
        context_.value.paused = paused;
        NetworkEvent event{NetworkEventKind::Pause, msg.sender};
        event.flag = paused;
        context_.event(std::move(event));
    }

    void readDamage(const net::NetMessage& msg, std::istream& stream,
                    NetworkEventKind kind)
    {
        NetworkEvent event{kind, msg.sender};
        std::uint32_t sender = 0U;
        std::int32_t damageType = 0;
        std::uint8_t death = 0U;
        if (!readScalar(stream, sender) ||
            !readScalar(stream, event.target) ||
            !readScalar(stream, event.value) ||
            !readScalar(stream, damageType) ||
            !readScalar(stream, event.targetLife) ||
            !readScalar(stream, death))
            return;
        event.playerModelId = sender;
        event.intValue = damageType;
        event.flag = death != 0U;
        // NetRace::OnDamage1/2 makes the server authoritative: the incoming
        // client command is not forwarded unchanged. The host applies it,
        // computes targetLife/death, then emits a new cNetTargetOthers RPC.
        if (net()->isServer())
            msg.Discard();
        context_.event(std::move(event));
    }

    void OnDamagePlayer(const net::NetMessage& msg,
                        const net::NetCmdHeader&, std::istream& stream)
    {
        readDamage(msg, stream, NetworkEventKind::PlayerDamage);
    }

    void OnDamageMapObject(const net::NetMessage& msg,
                           const net::NetCmdHeader&, std::istream& stream)
    {
        readDamage(msg, stream, NetworkEventKind::MapObjectDamage);
    }

    void OnSetUpgradeMaxLevel(const net::NetMessage&,
                              const net::NetCmdHeader&, std::istream& stream)
    {
        if (readScalar(stream, context_.value.match.upgradeMaxLevel))
            context_.touch();
    }

    void OnSetWeaponMaxLevel(const net::NetMessage&,
                             const net::NetCmdHeader&, std::istream& stream)
    {
        if (readScalar(stream, context_.value.match.weaponMaxLevel))
            context_.touch();
    }

    void OnSetCurrentDifficulty(const net::NetMessage&,
                                const net::NetCmdHeader&,
                                std::istream& stream)
    {
        if (readScalar(stream, context_.value.currentDifficulty))
        {
            context_.value.currentDifficultySet = true;
            context_.touch();
        }
    }

    void OnSetLapsCount(const net::NetMessage&,
                        const net::NetCmdHeader&, std::istream& stream)
    {
        if (readScalar(stream, context_.value.match.lapsCount))
            context_.touch();
    }

    void OnSetMaxPlayers(const net::NetMessage&,
                         const net::NetCmdHeader&, std::istream& stream)
    {
        if (readScalar(stream, context_.value.match.maxPlayers))
            context_.touch();
    }

    void OnSetMaxComputers(const net::NetMessage&,
                           const net::NetCmdHeader&, std::istream& stream)
    {
        if (readScalar(stream, context_.value.match.maxComputers))
            context_.touch();
    }

    void OnSetSpringBorders(const net::NetMessage&,
                            const net::NetCmdHeader&, std::istream& stream)
    {
        if (readScalar(stream, context_.value.match.springBorders))
            context_.touch();
    }

    void OnSetEnableMineBug(const net::NetMessage&,
                            const net::NetCmdHeader&, std::istream& stream)
    {
        if (readScalar(stream, context_.value.match.enableMineBug))
            context_.touch();
    }

    void OnPushLine(const net::NetMessage& msg, const net::NetCmdHeader&,
                    std::istream& stream)
    {
        NetworkEvent event{NetworkEventKind::ChatLine, msg.sender};
        if (readWindowsWideString(stream, event.text))
            context_.event(std::move(event));
    }
};

class PortableNetPlayer final : public net::NetModelRPC<PortableNetPlayer>
{
public:
    explicit PortableNetPlayer(const Desc& desc)
        : NetModelRPC(desc), context_(contextFor(desc.player->net())),
          service_(desc.player->net())
    {
        RegRPC(&PortableNetPlayer::OnSetGamerId);
        RegRPC(&PortableNetPlayer::OnSetColor);
        RegRPC(&PortableNetPlayer::OnSetCar);
        RegRPC(&PortableNetPlayer::OnCarSlotsChanged);
        RegRPC(&PortableNetPlayer::OnRaceReady);
        RegRPC(&PortableNetPlayer::OnRaceGoWait);
        RegRPC(&PortableNetPlayer::OnRaceFinish);
        RegRPC(&PortableNetPlayer::OnShot);
        RegRPC(&PortableNetPlayer::OnTakeBonus);
        RegRPC(&PortableNetPlayer::OnMineContactPlayerProjectile);
        RegRPC(&PortableNetPlayer::OnMineContactMapProjectile);

        if (!readPlayerDescriptor(desc.stream, state_.playerId,
                                  state_.netSlot))
            throw std::runtime_error("invalid original NetPlayer descriptor");
        state_.modelId = id();
        state_.ownerId = ownerId();
        state_.owner = owner();
        if (state_.playerId == 0U)
        {
            state_.gamerId = context_.generateGamerId(id());
            state_.color = context_.generateColor(id());
        }
        context_.registerPlayer(this, state_);
        syncState(ssDelta);
    }

    ~PortableNetPlayer() override
    {
        if (auto* context = findContext(service_))
            context->unregisterPlayer(this);
    }

    const NetworkPlayerState& state() const noexcept { return state_; }

    void process(std::uint32_t milliseconds)
    {
        if (!vehicleControlFresh_ ||
            milliseconds - lastVehicleUpdateMilliseconds_ < 1000U)
        {
            return;
        }

        vehicleControlFresh_ = false;
        if (state_.vehicle.moveState == 0U &&
            state_.vehicle.steerState == 0U)
        {
            return;
        }

        // Original NetPlayer::Process clears the requested movement states
        // after _dAlpha's one-second grace period. The received physical
        // wheel angle remains a snapshot value; swNone stops driving it.
        state_.vehicle.moveState = 0U;
        state_.vehicle.steerState = 0U;
        context_.updatePlayer(state_);
    }

    void setGamerId(std::int32_t value)
    {
        // NetPlayer::SetGamerId always emits cNetTargetAll, even if the
        // requested ID equals the current generated value. GamersFrame waits
        // for that authoritative event before entering Garage.
        sendGamerId(value, false, net::cNetTargetAll);
    }

    void applyLocalState(const NetworkPlayerState& value)
    {
        state_.vehicle = value.vehicle;
        if (state_.gamerId != value.gamerId)
            sendGamerId(value.gamerId, false, net::cNetTargetAll);
        if (state_.color != value.color)
        {
            state_.color = value.color;
            sendColor(value.color, false, net::cNetTargetOthers);
        }
        if (state_.car != value.car)
        {
            state_.car = value.car;
            std::ostream& stream =
                NewRPC(net::cNetTargetOthers, &PortableNetPlayer::OnSetCar);
            writeString(stream, state_.car);
            CloseRPC();
        }
        if (state_.slots != value.slots || state_.money != value.money)
        {
            state_.slots = value.slots;
            state_.money = value.money;
            std::ostream& stream = NewRPC(
                net::cNetTargetOthers,
                &PortableNetPlayer::OnCarSlotsChanged);
            writeScalar(stream, state_.money);
            writeSlots(stream, state_);
            CloseRPC();
        }
        setReady(value.raceReady);
        setGoWait(value.raceGoWait);
        setFinished(value.raceFinish);
        context_.updatePlayer(state_);
    }

    void setReady(bool ready)
    {
        if (state_.raceReady == ready)
            return;
        state_.raceReady = ready;
        context_.updatePlayer(state_);
        std::ostream& stream =
            NewRPC(net::cNetTargetOthers, &PortableNetPlayer::OnRaceReady);
        writeScalar(stream, ready);
        CloseRPC();
    }

    void setGoWait(bool waiting)
    {
        if (state_.raceGoWait == waiting)
            return;
        state_.raceGoWait = waiting;
        context_.updatePlayer(state_);
        std::ostream& stream =
            NewRPC(net::cNetTargetOthers, &PortableNetPlayer::OnRaceGoWait);
        writeScalar(stream, waiting);
        CloseRPC();
    }

    void setFinished(bool finished)
    {
        if (state_.raceFinish == finished)
            return;
        state_.raceFinish = finished;
        context_.updatePlayer(state_);
        std::ostream& stream =
            NewRPC(net::cNetTargetOthers, &PortableNetPlayer::OnRaceFinish);
        writeScalar(stream, finished);
        CloseRPC();
    }

    void resetRaceFlags()
    {
        state_.raceReady = false;
        state_.raceGoWait = false;
        state_.raceFinish = false;
        context_.updatePlayer(state_);
    }

    bool sendShot(
        std::uint32_t targetObjectId, std::uint8_t slotMask,
        std::uint32_t projectileId,
        const std::vector<std::array<float, 3>>& coordinates)
    {
        if ((slotMask & 0xc0U) != 0U ||
            coordinates.size() != setBitCount(slotMask))
            return false;
        std::ostream& stream =
            NewRPC(net::cNetTargetOthers, &PortableNetPlayer::OnShot);
        writeScalar(stream, targetObjectId);
        writeScalar(stream, slotMask);
        writeScalar(stream, projectileId);
        for (const auto& coordinate : coordinates)
        {
            stream.write(
                reinterpret_cast<const char*>(coordinate.data()),
                sizeof(float) * coordinate.size());
        }
        CloseRPC();
        return true;
    }

    void sendBonus(std::uint32_t bonusObjectId,
                   std::int32_t bonusType, float value)
    {
        std::ostream& stream = NewRPC(
            net::cNetTargetAll,
            &PortableNetPlayer::OnTakeBonus);
        writeScalar(stream, bonusObjectId);
        writeScalar(stream, bonusType);
        writeScalar(stream, value);
        CloseRPC();
    }

    void sendMineContactPlayer(
        std::uint32_t projectileOwnerModelId,
        std::uint32_t projectileId,
        const std::array<float, 3>& point)
    {
        std::ostream& stream = NewRPC(
            net::cNetTargetAll,
            &PortableNetPlayer::OnMineContactPlayerProjectile);
        writeScalar(stream, projectileOwnerModelId);
        writeScalar(stream, projectileId);
        stream.write(
            reinterpret_cast<const char*>(point.data()),
            sizeof(float) * point.size());
        CloseRPC();
    }

    void sendMineContactMap(
        std::uint32_t projectileObjectId,
        const std::array<float, 3>& point)
    {
        std::ostream& stream = NewRPC(
            net::cNetTargetAll,
            &PortableNetPlayer::OnMineContactMapProjectile);
        writeScalar(stream, projectileObjectId);
        stream.write(
            reinterpret_cast<const char*>(point.data()),
            sizeof(float) * point.size());
        CloseRPC();
    }

protected:
    void OnDescWrite(const net::NetMessage&, std::ostream& stream) override
    {
        writePlayerDescriptor(stream, state_.playerId, state_.netSlot);
    }

    void OnStateRead(const net::NetMessage&,
                     const net::NetCmdHeader&, std::istream& stream) override
    {
        if (readPlayerState(stream, state_))
            context_.updatePlayer(state_);
    }

    void OnStateWrite(const net::NetMessage&, std::ostream& stream) override
    {
        writePlayerState(stream, state_);
    }

    void OnSerialize(const net::NetMessage&, net::BitStream& stream) override
    {
        glm::vec3 position(state_.vehicle.position[0],
                           state_.vehicle.position[1],
                           state_.vehicle.position[2]);
        glm::quat rotation(state_.vehicle.rotation[3],
                           state_.vehicle.rotation[0],
                           state_.vehicle.rotation[1],
                           state_.vehicle.rotation[2]);
        glm::vec3 linear(state_.vehicle.linearMomentum[0],
                         state_.vehicle.linearMomentum[1],
                         state_.vehicle.linearMomentum[2]);
        glm::vec3 angular(state_.vehicle.angularMomentum[0],
                          state_.vehicle.angularMomentum[1],
                          state_.vehicle.angularMomentum[2]);
        auto move = state_.vehicle.moveState;
        auto steer = state_.vehicle.steerState;
        auto angle = state_.vehicle.steerWheelsAngle;
        stream.Serialize(position);
        stream.Serialize(rotation);
        stream.Serialize(linear);
        stream.Serialize(angular);
        stream.Serialize(move);
        stream.Serialize(steer);
        stream.Serialize(angle);
        if (!stream.isReading())
            return;
        // NetPlayer::ResponseStream deliberately ignores the owner's echoed
        // stream on a client and all vehicle streams after the racer finishes.
        if ((owner() && context_.service.isClient()) || state_.raceFinish)
            return;
        state_.vehicle.position = {position.x, position.y, position.z};
        state_.vehicle.rotation =
            {rotation.x, rotation.y, rotation.z, rotation.w};
        state_.vehicle.linearMomentum = {linear.x, linear.y, linear.z};
        state_.vehicle.angularMomentum = {angular.x, angular.y, angular.z};
        state_.vehicle.moveState = move;
        state_.vehicle.steerState = steer;
        state_.vehicle.steerWheelsAngle = angle;
        lastVehicleUpdateMilliseconds_ = context_.service.time();
        vehicleControlFresh_ = true;
        context_.updatePlayer(state_);
    }

private:
    OriginalNetworkModels::Impl& context_;
    net::INetService* service_ = nullptr;
    NetworkPlayerState state_;
    std::uint32_t lastVehicleUpdateMilliseconds_ = 0U;
    bool vehicleControlFresh_ = false;

    bool gamerIdAvailable(std::int32_t value) const
    {
        for (const auto& player : context_.value.players)
            if (player.modelId != id() && player.gamerId == value)
                return false;
        return true;
    }

    bool colorAvailable(const std::array<float, 4>& value) const
    {
        for (const auto& player : context_.value.players)
        {
            if (player.modelId == id())
                continue;
            bool same = true;
            for (std::size_t index = 0U; index < value.size(); ++index)
                same = same &&
                       std::abs(player.color[index] - value[index]) < 0.001F;
            if (same)
                return false;
        }
        return true;
    }

    void sendGamerId(std::int32_t value, bool failed, unsigned target)
    {
        std::ostream& stream =
            NewRPC(target, &PortableNetPlayer::OnSetGamerId);
        writeScalar(stream, value);
        writeScalar(stream, failed);
        CloseRPC();
    }

    void sendColor(const std::array<float, 4>& value, bool failed,
                   unsigned target)
    {
        std::ostream& stream =
            NewRPC(target, &PortableNetPlayer::OnSetColor);
        stream.write(reinterpret_cast<const char*>(value.data()),
                     sizeof(float) * value.size());
        writeScalar(stream, failed);
        CloseRPC();
    }

    void playerEvent(NetworkEventKind kind, const net::NetMessage& msg,
                     bool failed = false)
    {
        NetworkEvent event{kind, msg.sender};
        event.playerModelId = id();
        event.flag = failed;
        context_.event(std::move(event));
    }

    void OnSetGamerId(const net::NetMessage& msg,
                      const net::NetCmdHeader&, std::istream& stream)
    {
        std::int32_t value = -1;
        bool failed = false;
        if (!readScalar(stream, value) || !readScalar(stream, failed))
            return;
        if (net()->isServer() && !gamerIdAvailable(value))
        {
            msg.Discard();
            value = state_.gamerId;
            failed = true;
            // NetPlayer::OnSetGamerId applies a failed local-host request
            // and emits its event in place.  Only a remote owner receives a
            // directed authoritative rollback RPC.
            if (player()->id() != msg.sender)
            {
                sendGamerId(value, failed, msg.sender);
                return;
            }
        }
        state_.gamerId = value;
        context_.updatePlayer(state_);
        playerEvent(NetworkEventKind::PlayerGamerId, msg, failed);
    }

    void OnSetColor(const net::NetMessage& msg,
                    const net::NetCmdHeader&, std::istream& stream)
    {
        std::array<float, 4> value{};
        bool failed = false;
        if (!readBytes(stream, value.data(), sizeof(float) * value.size()) ||
            !readScalar(stream, failed))
            return;
        if (net()->isServer() && !colorAvailable(value))
        {
            msg.Discard();
            sendColor(state_.color, true, msg.sender);
            return;
        }
        state_.color = value;
        context_.updatePlayer(state_);
        playerEvent(NetworkEventKind::PlayerColor, msg, failed);
    }

    void OnSetCar(const net::NetMessage& msg, const net::NetCmdHeader&,
                  std::istream& stream)
    {
        if (!readString(stream, state_.car))
            return;
        context_.updatePlayer(state_);
        playerEvent(NetworkEventKind::PlayerCar, msg);
    }

    void OnCarSlotsChanged(const net::NetMessage& msg,
                           const net::NetCmdHeader&, std::istream& stream)
    {
        if (!readScalar(stream, state_.money) || !readSlots(stream, state_))
            return;
        context_.updatePlayer(state_);
        playerEvent(NetworkEventKind::PlayerSlots, msg);
    }

    void OnRaceReady(const net::NetMessage& msg, const net::NetCmdHeader&,
                     std::istream& stream)
    {
        if (!readScalar(stream, state_.raceReady))
            return;
        context_.updatePlayer(state_);
        playerEvent(NetworkEventKind::PlayerReady, msg);
    }

    void OnRaceGoWait(const net::NetMessage& msg, const net::NetCmdHeader&,
                      std::istream& stream)
    {
        if (!readScalar(stream, state_.raceGoWait))
            return;
        context_.updatePlayer(state_);
        playerEvent(NetworkEventKind::PlayerGoWait, msg);
    }

    void OnRaceFinish(const net::NetMessage& msg, const net::NetCmdHeader&,
                      std::istream& stream)
    {
        if (!readScalar(stream, state_.raceFinish))
            return;
        context_.updatePlayer(state_);
        playerEvent(NetworkEventKind::PlayerFinish, msg);
    }

    void OnShot(const net::NetMessage& msg, const net::NetCmdHeader&,
                std::istream& stream)
    {
        NetworkEvent event{NetworkEventKind::Shot, msg.sender};
        std::uint8_t slots = 0U;
        std::uint32_t projectile = 0U;
        if (!readScalar(stream, event.target) || !readScalar(stream, slots) ||
            !readScalar(stream, projectile))
            return;
        event.playerModelId = id();
        event.intValue = static_cast<std::int32_t>(projectile);
        event.slotMask = slots;
        const auto count = setBitCount(slots);
        event.coordinates.resize(count);
        for (auto& coordinate : event.coordinates)
            if (!readBytes(stream, coordinate.data(), sizeof(float) * 3U))
                return;
        context_.event(std::move(event));
    }

    void OnTakeBonus(const net::NetMessage& msg, const net::NetCmdHeader&,
                     std::istream& stream)
    {
        NetworkEvent event{NetworkEventKind::Bonus, msg.sender};
        if (!readScalar(stream, event.target) ||
            !readScalar(stream, event.intValue) ||
            !readScalar(stream, event.value))
            return;
        event.playerModelId = id();
        context_.event(std::move(event));
    }

    void OnMineContactPlayerProjectile(const net::NetMessage& msg,
                                       const net::NetCmdHeader&,
                                       std::istream& stream)
    {
        NetworkEvent event{NetworkEventKind::MineContact, msg.sender};
        std::uint32_t projectile = 0U;
        std::array<float, 3> point{};
        if (!readScalar(stream, event.target) ||
            !readScalar(stream, projectile) ||
            !readBytes(stream, point.data(), sizeof(float) * 3U))
            return;
        event.playerModelId = id();
        event.intValue = static_cast<std::int32_t>(projectile);
        event.flag = true;
        event.coordinates.push_back(point);
        context_.event(std::move(event));
    }

    void OnMineContactMapProjectile(const net::NetMessage& msg,
                                    const net::NetCmdHeader&,
                                    std::istream& stream)
    {
        NetworkEvent event{NetworkEventKind::MineContact, msg.sender};
        std::array<float, 3> point{};
        if (!readScalar(stream, event.target) ||
            !readBytes(stream, point.data(), sizeof(float) * 3U))
            return;
        event.playerModelId = id();
        event.coordinates.push_back(point);
        context_.event(std::move(event));
    }
};

void PortableNetRace::applyLocalState(PortableNetPlayer& model,
                                      const NetworkPlayerState& state)
{
    model.applyLocalState(state);
}

} // namespace

void OriginalNetworkModels::Impl::clearPlayersLocally(
    net::INetPlayer& player)
{
    std::vector<PortableNetPlayer*> copy;
    copy.reserve(players.size());
    for (const auto& entry : players)
        if (entry.second != nullptr)
            copy.push_back(entry.second);
    for (auto* model : copy)
        player.DeleteModel(model, true);
}

void OriginalNetworkModels::Impl::beginRace()
{
    value.paused = false;
    value.raceGoStage = -1;
    value.results.clear();
    for (const auto& entry : players)
    {
        auto* model = entry.second;
        if (model != nullptr)
            model->resetRaceFlags();
    }
    touch();
}

OriginalNetworkModels::Impl::Impl(net::INetService& source)
    : service(source)
{
    contexts()[&service] = this;
    auto& classes = service.modelClasses();
    if (classes.Find(raceModelClassId) == nullptr)
        classes.Add<PortableNetRace>(raceModelClassId);
    if (classes.Find(playerModelClassId) == nullptr)
        classes.Add<PortableNetPlayer>(playerModelClassId);
}

OriginalNetworkModels::Impl::~Impl()
{
    contexts().erase(&service);
}

void OriginalNetworkModels::Impl::registerPlayer(
    PortableNetPlayer* model, NetworkPlayerState state)
{
    players[state.modelId] = model;
    value.players.push_back(std::move(state));
    touch();
}

void OriginalNetworkModels::Impl::unregisterPlayer(PortableNetPlayer* model)
{
    for (auto iterator = players.begin(); iterator != players.end(); ++iterator)
    {
        if (iterator->second != model)
            continue;
        const auto id = iterator->first;
        players.erase(iterator);
        std::erase_if(value.players, [id](const auto& state) {
            return state.modelId == id;
        });
        touch();
        return;
    }
}

void OriginalNetworkModels::Impl::updatePlayer(
    const NetworkPlayerState& state)
{
    const auto found = std::find_if(
        value.players.begin(), value.players.end(), [&](const auto& item) {
            return item.modelId == state.modelId;
        });
    if (found == value.players.end())
        value.players.push_back(state);
    else
        *found = state;
    touch();
}

PortableNetPlayer* OriginalNetworkModels::Impl::localPlayer() const
{
    for (const auto& [id, model] : players)
        if (model != nullptr && model->owner() && model->state().playerId == 0U)
            return model;
    return nullptr;
}

std::int32_t OriginalNetworkModels::Impl::generateGamerId(
    std::uint32_t modelId) const
{
    for (const auto gamerId : gamerCatalog)
    {
        const bool available = std::none_of(
            value.players.begin(), value.players.end(),
            [&](const auto& player) {
                return player.playerId == 0U &&
                       player.modelId != modelId &&
                       player.gamerId == gamerId;
            });
        if (available)
            return gamerId;
    }
    return -1;
}

std::array<float, 4> OriginalNetworkModels::Impl::generateColor(
    std::uint32_t modelId) const
{
    for (const auto& color : sourcePlayerColors)
    {
        const bool available = std::none_of(
            value.players.begin(), value.players.end(),
            [&](const auto& player) {
                if (player.playerId != 0U ||
                    player.modelId == modelId)
                    return false;
                for (std::size_t index = 0U; index < color.size(); ++index)
                    if (std::abs(player.color[index] - color[index]) >=
                        0.001F)
                        return false;
                return true;
            });
        if (available)
            return color;
    }
    return {1.0F, 1.0F, 1.0F, 1.0F};
}

bool OriginalNetworkModels::Impl::makePlayer(
    std::uint8_t playerId, std::uint32_t netSlot, std::string& error)
{
    if (service.player() == nullptr)
    {
        error = "NetRace has no local NetPlayer allocator";
        return false;
    }
    for (const auto& [id, model] : players)
    {
        if (model != nullptr && model->owner() &&
            model->state().playerId == playerId &&
            model->state().netSlot == netSlot)
        {
            return true;
        }
    }
    std::ostream& stream = service.player()->NewModel<PortableNetPlayer>();
    writePlayerDescriptor(stream, playerId, netSlot);
    service.player()->CloseCmd();
    for (const auto& [id, model] : players)
    {
        if (model != nullptr && model->owner() &&
            model->state().playerId == playerId &&
            model->state().netSlot == netSlot)
        {
            return true;
        }
    }
    error = "NetRace did not allocate source NetPlayer class ID 2";
    return false;
}

bool OriginalNetworkModels::Impl::makeHuman(std::string& error)
{
    if (localPlayer() != nullptr)
        return true;
    if (service.player() == nullptr)
    {
        error = "NetRace has no local human allocator";
        return false;
    }
    return makePlayer(0U, service.player()->netIndex() + 1U, error);
}

bool OriginalNetworkModels::Impl::makeComputers(std::string& error)
{
    if (!service.isServer())
    {
        error = "only the NetRace host can allocate computer players";
        return false;
    }
    const auto humanCount = static_cast<std::uint32_t>(std::count_if(
        value.players.begin(), value.players.end(), [](const auto& player) {
            return player.playerId == 0U;
        }));
    std::uint32_t maxComputers = value.match.maxComputers;
    std::uint32_t maxPlayers = value.match.maxPlayers;
    if (value.match.mode == 0)
    {
        // NetRace::StartRace clamps championship fields to
        // cComputerCount-1, cCampaignMaxPlayers-1 and
        // cCampaignMaxHumans respectively.
        maxComputers = std::clamp<std::uint32_t>(maxComputers, 4U, 5U);
        maxPlayers = std::clamp<std::uint32_t>(
            maxPlayers, humanCount + maxComputers, maxComputers + 3U);
    }
    const auto computerCount = std::min(
        maxComputers,
        maxPlayers > humanCount ? maxPlayers - humanCount : 0U);

    // NetRace::StartRace reconciles both sides of the AI count. Its second
    // loop repeatedly deletes _aiPlayers.back() when the host lowered
    // MaxComputers/MaxPlayers between races. Keeping those class-ID-2 models
    // alive would leave extra cars in physics, rendering and the mini-map.
    // NetServer processes this reliable cDelModelRPC locally as well as
    // forwarding it, matching DeleteModel(model, false) in the source.
    std::vector<PortableNetPlayer*> computers;
    computers.reserve(players.size());
    for (const auto& [id, model] : players)
    {
        static_cast<void>(id);
        if (model != nullptr && model->state().playerId != 0U)
            computers.push_back(model);
    }
    while (computers.size() > computerCount)
    {
        auto* model = computers.back();
        computers.pop_back();
        service.player()->DeleteModel(model, false);
    }

    for (std::uint32_t index = 0U; index < computerCount; ++index)
    {
        if (!makePlayer(
                static_cast<std::uint8_t>(index + 1U), 0U, error))
            return false;
    }
    return true;
}

OriginalNetworkModels::OriginalNetworkModels(net::INetService& service)
    : impl_(std::make_unique<Impl>(service))
{
}

OriginalNetworkModels::~OriginalNetworkModels() = default;

void OriginalNetworkModels::setGamerCatalog(
    std::vector<std::int32_t> gamerIds)
{
    impl_->gamerCatalog = std::move(gamerIds);
}

void OriginalNetworkModels::process(std::uint32_t milliseconds)
{
    for (const auto& [id, player] : impl_->players)
    {
        static_cast<void>(id);
        if (player != nullptr)
            player->process(milliseconds);
    }
}

bool OriginalNetworkModels::createHostRace(std::string& error)
{
    error.clear();
    if (!impl_->service.isServer() || impl_->service.player() == nullptr)
    {
        error = "NetRace can only be allocated by an active server";
        return false;
    }
    if (impl_->race != nullptr)
        return true;
    impl_->service.player()->MakeModel<PortableNetRace>();
    if (impl_->race == nullptr)
    {
        error = "NetLib did not allocate source NetRace class ID 1";
        return false;
    }
    return true;
}

bool OriginalNetworkModels::startMatch(
    const NetworkMatchState& match, const NetworkPlayerState& localPlayer,
    std::string& error)
{
    error.clear();
    if (impl_->race == nullptr || !impl_->service.isServer())
    {
        error = "source NetRace host model is not active";
        return false;
    }
    return impl_->race->sendStartMatch(match, localPlayer, error);
}

bool OriginalNetworkModels::exitMatch(std::string& error)
{
    error.clear();
    if (impl_->race == nullptr)
    {
        error = "source NetRace model is not active";
        return false;
    }
    impl_->race->sendExitMatch();
    return true;
}

bool OriginalNetworkModels::startRace(std::string& error)
{
    error.clear();
    if (impl_->race == nullptr || !impl_->value.matchActive)
    {
        error = "source NetRace match is not active";
        return false;
    }
    return impl_->race->sendStartRace(error);
}

bool OriginalNetworkModels::exitRace(
    std::int32_t track, std::int32_t weather,
    const std::vector<NetworkRaceResult>& results, std::string& error)
{
    error.clear();
    if (impl_->race == nullptr || !impl_->service.isServer() ||
        !impl_->value.raceActive)
    {
        error = "only the active NetRace host can exit a race";
        return false;
    }
    if (results.size() > 64U)
    {
        error = "source NetRace result payload exceeds 64 players";
        return false;
    }
    impl_->race->sendExitRace(track, weather, results);
    return true;
}

bool OriginalNetworkModels::setRaceGoStage(std::int32_t stage,
                                            std::string& error)
{
    error.clear();
    if (impl_->race == nullptr || !impl_->value.raceActive)
    {
        error = "source NetRace race is not active";
        return false;
    }
    impl_->race->sendRaceGo(stage);
    return true;
}

bool OriginalNetworkModels::setPlanet(std::int32_t planet,
                                       std::int32_t track,
                                       std::int32_t weather,
                                       std::string& error)
{
    error.clear();
    if (impl_->race == nullptr)
    {
        error = "source NetRace model is not active";
        return false;
    }
    impl_->race->sendPlanet(planet, track, weather);
    return true;
}

bool OriginalNetworkModels::setTrack(std::int32_t track, std::string& error)
{
    error.clear();
    if (impl_->race == nullptr)
    {
        error = "source NetRace model is not active";
        return false;
    }
    impl_->race->sendTrack(track);
    return true;
}

namespace
{

bool requireHostMatch(OriginalNetworkModels::Impl& impl,
                      std::string& error)
{
    error.clear();
    if (impl.race == nullptr || !impl.value.matchActive)
    {
        error = "source NetRace match is not active";
        return false;
    }
    if (!impl.service.isServer())
    {
        error = "only the NetRace host can change match options";
        return false;
    }
    return true;
}

} // namespace

bool OriginalNetworkModels::setUpgradeMaxLevel(
    std::int32_t level, std::string& error)
{
    if (!requireHostMatch(*impl_, error))
        return false;
    impl_->race->sendUpgradeMaxLevel(level);
    return true;
}

bool OriginalNetworkModels::setWeaponMaxLevel(
    std::int32_t level, std::string& error)
{
    if (!requireHostMatch(*impl_, error))
        return false;
    impl_->race->sendWeaponMaxLevel(level);
    return true;
}

bool OriginalNetworkModels::setCurrentDifficulty(
    std::int32_t difficulty, std::string& error)
{
    if (!requireHostMatch(*impl_, error))
        return false;
    impl_->race->sendCurrentDifficulty(difficulty);
    return true;
}

bool OriginalNetworkModels::setLapsCount(
    std::uint32_t laps, std::string& error)
{
    if (!requireHostMatch(*impl_, error))
        return false;
    impl_->race->sendLapsCount(laps);
    return true;
}

bool OriginalNetworkModels::setMaxPlayers(
    std::uint32_t players, std::string& error)
{
    if (!requireHostMatch(*impl_, error))
        return false;
    impl_->race->sendMaxPlayers(players);
    return true;
}

bool OriginalNetworkModels::setMaxComputers(
    std::uint32_t computers, std::string& error)
{
    if (!requireHostMatch(*impl_, error))
        return false;
    impl_->race->sendMaxComputers(computers);
    return true;
}

bool OriginalNetworkModels::setSpringBorders(
    bool enabled, std::string& error)
{
    if (!requireHostMatch(*impl_, error))
        return false;
    impl_->race->sendSpringBorders(enabled);
    return true;
}

bool OriginalNetworkModels::setEnableMineBug(
    bool enabled, std::string& error)
{
    if (!requireHostMatch(*impl_, error))
        return false;
    impl_->race->sendEnableMineBug(enabled);
    return true;
}

bool OriginalNetworkModels::sendPlayerDamage(
    std::uint32_t senderModelId, std::uint32_t targetModelId,
    float value, std::int32_t damageType, float targetLife,
    bool death, std::string& error)
{
    error.clear();
    if (impl_->race == nullptr || !impl_->value.raceActive)
    {
        error = "source NetRace race is not active";
        return false;
    }
    impl_->race->sendDamage(
        NetworkEventKind::PlayerDamage, senderModelId,
        targetModelId, value, damageType, targetLife, death);
    return true;
}

bool OriginalNetworkModels::sendMapObjectDamage(
    std::uint32_t senderModelId, std::uint32_t targetObjectId,
    float value, std::int32_t damageType, float targetLife,
    bool death, std::string& error)
{
    error.clear();
    if (impl_->race == nullptr || !impl_->value.raceActive)
    {
        error = "source NetRace race is not active";
        return false;
    }
    impl_->race->sendDamage(
        NetworkEventKind::MapObjectDamage, senderModelId,
        targetObjectId, value, damageType, targetLife, death);
    return true;
}

bool OriginalNetworkModels::pushLine(
    std::string_view text, std::string& error)
{
    error.clear();
    if (impl_->race == nullptr)
    {
        error = "source NetRace model is not active";
        return false;
    }
    if (!impl_->race->sendLine(text))
    {
        error = "source NetRace UTF-16 line exceeds the command limit";
        return false;
    }
    return true;
}

bool OriginalNetworkModels::setLocalPlayerState(
    const NetworkPlayerState& state, std::string& error)
{
    error.clear();
    auto* model = impl_->localPlayer();
    if (model == nullptr)
    {
        error = "source local NetPlayer model is not active";
        return false;
    }
    model->applyLocalState(state);
    return true;
}

bool OriginalNetworkModels::setLocalPlayerGamerId(
    std::int32_t gamerId, std::string& error)
{
    error.clear();
    auto* model = impl_->localPlayer();
    if (model == nullptr)
    {
        error = "source local NetPlayer model is not active";
        return false;
    }
    model->setGamerId(gamerId);
    return true;
}

bool OriginalNetworkModels::setLocalPlayerReady(bool ready,
                                                 std::string& error)
{
    error.clear();
    auto* model = impl_->localPlayer();
    if (model == nullptr)
    {
        error = "source local NetPlayer model is not active";
        return false;
    }
    model->setReady(ready);
    return true;
}

bool OriginalNetworkModels::setLocalPlayerGoWait(bool waiting,
                                                  std::string& error)
{
    error.clear();
    auto* model = impl_->localPlayer();
    if (model == nullptr)
    {
        error = "source local NetPlayer model is not active";
        return false;
    }
    model->setGoWait(waiting);
    return true;
}

bool OriginalNetworkModels::setLocalPlayerFinished(bool finished,
                                                    std::string& error)
{
    error.clear();
    auto* model = impl_->localPlayer();
    if (model == nullptr)
    {
        error = "source local NetPlayer model is not active";
        return false;
    }
    model->setFinished(finished);
    return true;
}

bool OriginalNetworkModels::setOwnedPlayerFinished(
    std::uint32_t modelId, bool finished, std::string& error)
{
    error.clear();
    const auto found = impl_->players.find(modelId);
    if (found == impl_->players.end() || found->second == nullptr ||
        !found->second->owner())
    {
        error = "source owned NetPlayer model is not active";
        return false;
    }
    found->second->setFinished(finished);
    return true;
}

bool OriginalNetworkModels::sendLocalShot(
    std::uint32_t targetObjectId, std::uint8_t slotMask,
    std::uint32_t projectileId,
    const std::vector<std::array<float, 3>>& coordinates,
    std::string& error)
{
    error.clear();
    auto* model = impl_->localPlayer();
    if (model == nullptr)
    {
        error = "source local NetPlayer model is not active";
        return false;
    }
    if (!model->sendShot(
            targetObjectId, slotMask, projectileId, coordinates))
    {
        error = "source NetPlayer shot slot/coordinate payload is invalid";
        return false;
    }
    return true;
}

bool OriginalNetworkModels::sendOwnedPlayerShot(
    std::uint32_t modelId, std::uint32_t targetObjectId,
    std::uint8_t slotMask, std::uint32_t projectileId,
    const std::vector<std::array<float, 3>>& coordinates,
    std::string& error)
{
    error.clear();
    const auto found = impl_->players.find(modelId);
    if (found == impl_->players.end() || found->second == nullptr ||
        !found->second->owner())
    {
        error = "source owned NetPlayer model is not active";
        return false;
    }
    if (!found->second->sendShot(
            targetObjectId, slotMask, projectileId, coordinates))
    {
        error = "source NetPlayer shot slot/coordinate payload is invalid";
        return false;
    }
    return true;
}

bool OriginalNetworkModels::sendLocalBonus(
    std::uint32_t bonusObjectId, std::int32_t bonusType, float value,
    std::string& error)
{
    error.clear();
    auto* model = impl_->localPlayer();
    if (model == nullptr)
    {
        error = "source local NetPlayer model is not active";
        return false;
    }
    model->sendBonus(bonusObjectId, bonusType, value);
    return true;
}

bool OriginalNetworkModels::sendOwnedPlayerBonus(
    std::uint32_t modelId, std::uint32_t bonusObjectId,
    std::int32_t bonusType, float value, std::string& error)
{
    error.clear();
    const auto found = impl_->players.find(modelId);
    if (found == impl_->players.end() || found->second == nullptr ||
        !found->second->owner())
    {
        error = "source owned NetPlayer model is not active";
        return false;
    }
    found->second->sendBonus(bonusObjectId, bonusType, value);
    return true;
}

bool OriginalNetworkModels::sendLocalMineContactPlayer(
    std::uint32_t projectileOwnerModelId,
    std::uint32_t projectileId,
    const std::array<float, 3>& point, std::string& error)
{
    error.clear();
    auto* model = impl_->localPlayer();
    if (model == nullptr)
    {
        error = "source local NetPlayer model is not active";
        return false;
    }
    model->sendMineContactPlayer(
        projectileOwnerModelId, projectileId, point);
    return true;
}

bool OriginalNetworkModels::sendOwnedPlayerMineContactPlayer(
    std::uint32_t modelId, std::uint32_t projectileOwnerModelId,
    std::uint32_t projectileId,
    const std::array<float, 3>& point, std::string& error)
{
    error.clear();
    const auto found = impl_->players.find(modelId);
    if (found == impl_->players.end() || found->second == nullptr ||
        !found->second->owner())
    {
        error = "source owned NetPlayer model is not active";
        return false;
    }
    found->second->sendMineContactPlayer(
        projectileOwnerModelId, projectileId, point);
    return true;
}

bool OriginalNetworkModels::sendLocalMineContactMap(
    std::uint32_t projectileObjectId,
    const std::array<float, 3>& point, std::string& error)
{
    error.clear();
    auto* model = impl_->localPlayer();
    if (model == nullptr)
    {
        error = "source local NetPlayer model is not active";
        return false;
    }
    model->sendMineContactMap(projectileObjectId, point);
    return true;
}

bool OriginalNetworkModels::sendOwnedPlayerMineContactMap(
    std::uint32_t modelId, std::uint32_t projectileObjectId,
    const std::array<float, 3>& point, std::string& error)
{
    error.clear();
    const auto found = impl_->players.find(modelId);
    if (found == impl_->players.end() || found->second == nullptr ||
        !found->second->owner())
    {
        error = "source owned NetPlayer model is not active";
        return false;
    }
    found->second->sendMineContactMap(projectileObjectId, point);
    return true;
}

bool OriginalNetworkModels::acceptsConnections() const noexcept
{
    const auto sourceLimit = impl_->value.match.mode == 0
                                 ? 3U
                                 : 6U;
    const auto maximum = std::min(
        impl_->value.match.maxPlayers, sourceLimit);
    const auto humans = static_cast<std::uint32_t>(std::count_if(
        impl_->value.players.begin(), impl_->value.players.end(),
        [](const auto& player) { return player.playerId == 0U; }));
    return !impl_->value.raceActive &&
           humans < maximum;
}

NetworkModelSnapshot OriginalNetworkModels::snapshot() const
{
    return impl_->value;
}

} // namespace r3d::game::originalnetwork
