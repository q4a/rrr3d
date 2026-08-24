#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
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

    using Prices = std::vector<Price>;
    using Tracks = std::vector<Track>;
    using TrackMap = std::map<int, Tracks>;
    using RequestPoints = std::map<int, int>;

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

    void Unlock() noexcept;
    bool Open() noexcept;
    bool Complete() noexcept;
    void NextPass() noexcept;
    int GetPass() const noexcept;
    void SetPass(int value) noexcept;
    void Reset() noexcept;

    std::uint32_t GetIndex() const noexcept;
    State GetState() const noexcept;
    void SetState(State value) noexcept;
    void Restore(State state, int pass) noexcept;

    const RequestPoints& GetRequestPoints() const noexcept;
    void SetRequestPoints(RequestPoints value);
    int GetRequestPoints(int pass,
                         std::uint32_t humanOrOpponentCount) const noexcept;
    bool HasRequestPoints(int pass, int points,
                          std::uint32_t humanOrOpponentCount) const noexcept;

    Price GetPrice(int place) const noexcept;
    void SetPrices(Prices value);

private:
    std::uint32_t index_ = 0U;
    TrackMap trackMap_;
    Tracks emptyTracks_;
    RequestPoints requestPoints_;
    State state_ = psUnavailable;
    Prices prices_;
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
    };

    explicit Tournament(bool campaign = true) noexcept;

    Planet& AddPlanet();
    void ClearPlanets() noexcept;
    Planet* GetPlanet(std::size_t index) noexcept;
    const Planet* GetPlanet(std::size_t index) const noexcept;

    bool SetCurPlanet(std::size_t index) noexcept;
    bool SetCurTrack(std::size_t catalogIndex) noexcept;
    bool Select(std::size_t planetIndex, int pass,
                std::size_t localTrack) noexcept;
    const Planet* GetCurPlanet() const noexcept;
    const Planet::Track* GetCurTrack() const noexcept;
    std::size_t GetCurTrackIndex() const noexcept;
    const Planet::Track* NextTrack(const Planet::Track* track) noexcept;

    Advance CompleteTrack(int points,
                          std::uint32_t humanOrOpponentCount) noexcept;
    void Reset() noexcept;

private:
    std::vector<Planet> planets_;
    std::size_t curPlanet_ = 0U;
    const Planet::Track* curTrack_ = nullptr;
    std::vector<const Planet::Track*> trackList_;
    bool hasCurPlanet_ = false;
    bool campaign_ = true;
};

} // namespace r3d::game::originalrace::source
