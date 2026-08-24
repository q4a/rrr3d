#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace r3d::game::originalrace::source
{

// Portable transcription of the gameplay-only part of the Windows
// Planet/Tournament classes.  Rendering, Object reference counting and
// PhysX callbacks deliberately stay outside this module; the progression
// rules and their original method boundaries remain here.
class Planet
{
public:
    struct Price
    {
        int money = 0;
        int points = 0;
    };

    struct Track
    {
        std::size_t catalogIndex = 0U;
        std::size_t index = 0U;
        std::uint32_t planetIndex = 0U;
        int pass = 1;
        std::uint32_t numLaps = 1U;
    };

    struct SlotData
    {
        std::string record;
        std::uint32_t charge = 0U;
        std::string type;
        int pass = 0;
    };

    struct CarData
    {
        std::string record;
        int pass = 0;
    };

    struct PlayerData
    {
        int id = -1;
        std::string name;
        std::string bonus;
        std::string photoPath;
        int maxPass = 0;
        std::vector<CarData> cars;
        std::vector<SlotData> slots;
    };

    struct PlayerState
    {
        int id = -1;
        bool computer = false;
        std::string car;
        std::vector<SlotData> slots;
        bool maximizeForSkirmish = false;
    };

    using Prices = std::vector<Price>;
    using Tracks = std::vector<Track>;
    using TrackMap = std::map<int, Tracks>;
    using RequestPoints = std::map<int, int>;
    using Slots = std::vector<SlotData>;
    using Cars = std::vector<CarData>;
    using Players = std::vector<PlayerData>;

    // Values intentionally match Planet::State in the Windows save format.
    enum State
    {
        psOpen = 0,
        psClosed = 1,
        psUnavailable = 2,
        psCompleted = 3,
    };

    explicit Planet(std::uint32_t index = 0U) noexcept;

    Track& AddTrack(int pass, std::size_t catalogIndex,
                    std::uint32_t numLaps);
    void ClearTracks() noexcept;
    const Track* NextTrack(const Track* track) const noexcept;
    const Tracks& GetTracks(int pass) const noexcept;
    const Tracks& GetTracks() const noexcept;
    const TrackMap& GetTrackMap() const noexcept;

    void Unlock();
    bool Open();
    bool Complete();
    void StartPass(int pass, PlayerState& player, bool campaign) const;
    void StartPass(int pass, std::vector<PlayerState>& players,
                   bool campaign) const;
    void CompletePass(int pass);
    void NextPass();
    int GetPass() const noexcept;
    void SetPass(int value);
    void Reset() noexcept;

    std::uint32_t GetIndex() const noexcept;
    int GetId() const noexcept;
    const std::string& GetName() const noexcept;
    void SetName(std::string value);
    State GetState() const noexcept;
    void SetState(State value);
    void Restore(State state, int pass) noexcept;

    const RequestPoints& GetRequestPoints() const noexcept;
    void SetRequestPoints(RequestPoints value);
    int GetRequestPoints(int pass,
                         std::uint32_t humanOrOpponentCount) const noexcept;
    bool HasRequestPoints(int pass, int points,
                          std::uint32_t humanOrOpponentCount) const noexcept;

    Price GetPrice(int place) const noexcept;
    void SetPrices(Prices value);

    void InsertSlot(SlotData slot);
    void ClearSlots() noexcept;
    void SetSlots(Slots value);
    const Slots& GetSlots() const noexcept;
    void InsertCar(CarData car);
    void ClearCars() noexcept;
    void SetCars(Cars value);
    const Cars& GetCars() const noexcept;
    void InsertPlayer(PlayerData player);
    void ClearPlayers() noexcept;
    const PlayerData* GetPlayer(int id) const noexcept;
    const PlayerData* GetPlayer(const std::string& name) const noexcept;
    PlayerData GetBoss() const;
    const Players& GetPlayers() const noexcept;

    std::vector<std::string> TakeCompletedSlots() noexcept;
    std::vector<std::string> TakeCompletedCars() noexcept;

private:
    std::uint32_t index_ = 0U;
    std::string name_;
    TrackMap trackMap_;
    Tracks emptyTracks_;
    RequestPoints requestPoints_;
    State state_ = psUnavailable;
    Prices prices_;
    Slots slots_;
    Cars cars_;
    Players players_;
    std::vector<std::string> completedSlots_;
    std::vector<std::string> completedCars_;
    int pass_ = -1;
};

class Tournament
{
public:
    struct Advance
    {
        std::size_t trackIndex = 0U;
        int planetPass = 1;
        bool passComplete = false;
        bool passChampion = false;
        bool planetChampion = false;
        std::vector<std::string> unlockedSlots;
        std::vector<std::string> unlockedCars;
    };

    explicit Tournament(bool campaign = true) noexcept;

    Planet& AddPlanet();
    void ClearPlanets() noexcept;
    Planet* GetPlanet(std::size_t index) noexcept;
    const Planet* GetPlanet(std::size_t index) const noexcept;
    Planet* NextPlanet(std::size_t index) noexcept;
    Planet* PrevPlanet(std::size_t index) noexcept;

    Planet& AddGamer();
    void ClearGamers() noexcept;
    Planet* GetGamer(int gamerId) noexcept;
    const Planet::PlayerData* GetPlayerData(int id) const noexcept;
    const Planet::PlayerData* GetPlayerData(
        const std::string& name) const noexcept;

    bool SetCurPlanet(std::size_t index);
    bool ChangePlanet(std::size_t index);
    bool SetCurTrack(std::size_t catalogIndex) noexcept;
    bool Select(std::size_t planetIndex, int pass,
                std::size_t localTrack);
    const Planet* GetCurPlanet() const noexcept;
    std::size_t GetCurPlanetIndex() const noexcept;
    Planet* GetNextPlanet() noexcept;
    const Planet::Track* GetCurTrack() const noexcept;
    std::size_t GetCurTrackIndex() const noexcept;
    const Planet::Track* NextTrack(const Planet::Track* track);

    Advance CompleteTrack(int points,
                          std::uint32_t humanOrOpponentCount);
    void Reset() noexcept;

private:
    std::vector<Planet> planets_;
    std::vector<Planet> gamers_;
    std::size_t curPlanet_ = 0U;
    const Planet::Track* curTrack_ = nullptr;
    std::vector<const Planet::Track*> trackList_;
    bool hasCurPlanet_ = false;
    bool campaign_ = true;
};

} // namespace r3d::game::originalrace::source
