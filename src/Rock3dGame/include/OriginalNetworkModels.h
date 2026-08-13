#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace net
{
class INetService;
}

namespace r3d::game::originalnetwork
{

inline constexpr std::uint32_t raceModelClassId = 1U;
inline constexpr std::uint32_t playerModelClassId = 2U;
inline constexpr std::size_t playerSlotCount = 10U;

// The indexes are part of the Windows wire protocol: NetModelRPC assigns an
// RPC number from registration order and adds NetModel::cSysRPCEnd (1).
enum class RaceRpc : std::uint8_t
{
    StartMatch = 1,
    ExitMatch,
    SetPlanet,
    SetTrack,
    StartRace,
    ExitRace,
    RaceGo,
    Pause,
    DamagePlayer,
    DamageMapObject,
    SetUpgradeMaxLevel,
    SetWeaponMaxLevel,
    SetCurrentDifficulty,
    SetLapsCount,
    SetMaxPlayers,
    SetMaxComputers,
    SetSpringBorders,
    SetEnableMineBug,
    PushLine,
};

enum class PlayerRpc : std::uint8_t
{
    SetGamerId = 1,
    SetColor,
    SetCar,
    CarSlotsChanged,
    RaceReady,
    RaceGoWait,
    RaceFinish,
    Shot,
    TakeBonus,
    MineContactPlayerProjectile,
    MineContactMapProjectile,
};

struct NetworkMatchState
{
    std::int32_t mode = 1;
    std::int32_t upgradeMaxLevel = 0;
    std::int32_t weaponMaxLevel = 0;
    std::uint32_t lapsCount = 3U;
    std::uint32_t maxPlayers = 6U;
    std::uint32_t maxComputers = 5U;
    bool springBorders = false;
    bool enableMineBug = false;
    std::int32_t planet = 0;
    std::int32_t track = 0;
    std::int32_t weather = 0;

    // Race::Profile::SaveGame writes an XML document directly after the fixed
    // match fields, without a length prefix. Keeping it opaque preserves the
    // original command layout and permits a source profile serializer to be
    // connected without changing the protocol.
    std::string profileXml;
};

struct NetworkEquipmentSlot
{
    std::string record;
    std::uint32_t chargeCount = 0U;
    bool armor4 = false;

    bool operator==(const NetworkEquipmentSlot&) const = default;
};

struct NetworkVehicleState
{
    // Quaternion uses the GLM memory/wire order x, y, z, w.
    std::array<float, 3> position{};
    std::array<float, 4> rotation{0.0F, 0.0F, 0.0F, 1.0F};
    // The source names these linVel/angVel but serializes PhysX momentum.
    std::array<float, 3> linearMomentum{};
    std::array<float, 3> angularMomentum{};
    std::uint8_t moveState = 0U;
    std::uint8_t steerState = 0U;
    float steerWheelsAngle = 0.0F;

    bool operator==(const NetworkVehicleState&) const = default;
};

struct NetworkPlayerState
{
    std::uint32_t modelId = 0U;
    std::uint32_t ownerId = 0U;
    std::uint8_t playerId = 0U;
    std::uint32_t netSlot = 0U;
    bool owner = false;
    std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
    std::string car;
    bool raceReady = false;
    bool raceGoWait = false;
    bool raceFinish = false;
    std::int32_t gamerId = -1;
    std::int32_t money = 0;
    std::array<NetworkEquipmentSlot, playerSlotCount> slots;
    NetworkVehicleState vehicle;

    bool operator==(const NetworkPlayerState&) const = default;
};

struct NetworkRaceResult
{
    std::uint32_t playerModelId = 0U;
    std::int32_t playerPoints = 0;
    std::int32_t playerMoney = 0;
    std::int32_t money = 0;
    std::int32_t pickedMoney = 0;
    std::uint32_t place = 0U;
    std::int32_t points = 0;
    float voiceNameDuration = 0.0F;
};

enum class NetworkEventKind
{
    MatchStarted,
    MatchExited,
    RaceStarted,
    RaceExited,
    RaceGo,
    Pause,
    PlayerDamage,
    MapObjectDamage,
    ChatLine,
    PlayerIdentity,
    PlayerCar,
    PlayerSlots,
    PlayerReady,
    PlayerGoWait,
    PlayerFinish,
    Shot,
    Bonus,
    MineContact,
};

struct NetworkEvent
{
    NetworkEvent() = default;
    NetworkEvent(NetworkEventKind eventKind, std::uint32_t eventSender)
        : kind(eventKind), sender(eventSender)
    {
    }

    NetworkEventKind kind = NetworkEventKind::MatchStarted;
    std::uint32_t sender = 0U;
    std::uint32_t playerModelId = 0U;
    std::uint32_t target = 0U;
    std::int32_t intValue = 0;
    float value = 0.0F;
    float targetLife = 0.0F;
    bool flag = false;
    std::string text;
    std::vector<std::array<float, 3>> coordinates;
};

struct NetworkModelSnapshot
{
    bool raceModelPresent = false;
    bool matchActive = false;
    bool raceActive = false;
    bool paused = false;
    std::int32_t raceGoStage = -1;
    std::int32_t currentDifficulty = 0;
    NetworkMatchState match;
    std::vector<NetworkPlayerState> players;
    std::vector<NetworkRaceResult> results;
    std::vector<NetworkEvent> events;
    std::uint64_t revision = 0U;
};

// Runtime binding for the original NetRace/NetPlayer model classes. It uses
// NetLib's class factory, model allocation, reliable RPC and delta BitStream;
// it is not a parallel or replacement protocol.
class OriginalNetworkModels final
{
public:
    class Impl;

    explicit OriginalNetworkModels(net::INetService& service);
    ~OriginalNetworkModels();

    OriginalNetworkModels(const OriginalNetworkModels&) = delete;
    OriginalNetworkModels& operator=(const OriginalNetworkModels&) = delete;

    bool createHostRace(std::string& error);
    bool startMatch(const NetworkMatchState& match,
                    const NetworkPlayerState& localPlayer,
                    std::string& error);
    bool exitMatch(std::string& error);
    bool startRace(std::string& error);
    bool setRaceGoStage(std::int32_t stage, std::string& error);
    bool setPlanet(std::int32_t planet, std::int32_t track,
                   std::int32_t weather, std::string& error);
    bool setTrack(std::int32_t track, std::string& error);
    bool setPaused(bool paused, std::string& error);
    bool setLocalPlayerState(const NetworkPlayerState& state,
                             std::string& error);
    bool setLocalPlayerReady(bool ready, std::string& error);
    bool setLocalPlayerGoWait(bool waiting, std::string& error);
    bool setLocalPlayerFinished(bool finished, std::string& error);

    [[nodiscard]] bool acceptsConnections() const noexcept;
    [[nodiscard]] NetworkModelSnapshot snapshot() const;

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace r3d::game::originalnetwork
