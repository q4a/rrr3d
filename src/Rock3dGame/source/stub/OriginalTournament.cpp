#include "OriginalTournament.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <utility>

namespace r3d::game::originalrace::source
{
namespace
{

int normalizedPass(int pass, int maximumPass) noexcept
{
    if (maximumPass <= 0)
        return 1;
    // Windows Planet::GetTracks uses abs(pass - 1) % maxPass + 1.  Promote
    // before subtraction so a malformed INT_MIN save cannot overflow.
    const auto distance = std::llabs(
        static_cast<long long>(pass) - 1LL);
    return static_cast<int>(distance % maximumPass) + 1;
}

int scaledRequestPoints(int points,
                        std::uint32_t playerCount) noexcept
{
    float scale = 1.0F;
    if (playerCount > 3U)
        scale = 1.35F;
    else if (playerCount > 1U)
        scale = 1.3F;
    if (scale == 1.0F)
        return points;
    return static_cast<int>(
               std::ceil(points * scale / 100.0F)) *
           100;
}

} // namespace

Planet::Planet(std::uint32_t index) noexcept : index_(index) {}

Planet::Track& Planet::AddTrack(int pass, std::size_t catalogIndex,
                                std::uint32_t numLaps)
{
    auto& tracks = trackMap_[pass];
    tracks.push_back(
        {catalogIndex, tracks.size(), index_, pass, numLaps});
    return tracks.back();
}

void Planet::ClearTracks() noexcept
{
    trackMap_.clear();
}

const Planet::Track* Planet::NextTrack(
    const Track* track) const noexcept
{
    const auto& tracks = GetTracks();
    const auto found = std::find_if(
        tracks.begin(), tracks.end(),
        [track](const Track& candidate) { return &candidate == track; });
    if (found == tracks.end() || std::next(found) == tracks.end())
        return nullptr;
    return &*std::next(found);
}

const Planet::Tracks& Planet::GetTracks(int pass) const noexcept
{
    if (trackMap_.empty())
        return emptyTracks_;
    const int maximumPass = trackMap_.rbegin()->first;
    const auto found = trackMap_.find(normalizedPass(pass, maximumPass));
    return found != trackMap_.end() ? found->second
                                   : trackMap_.begin()->second;
}

const Planet::Tracks& Planet::GetTracks() const noexcept
{
    return GetTracks(pass_);
}

const Planet::TrackMap& Planet::GetTrackMap() const noexcept
{
    return trackMap_;
}

void Planet::Unlock() noexcept
{
    if (state_ == psUnavailable || state_ == psCompleted)
        SetState(psClosed);
}

bool Planet::Open() noexcept
{
    if (state_ == psUnavailable)
        return false;
    if (state_ == psOpen || state_ == psCompleted)
        return true;
    SetState(psOpen);
    return true;
}

bool Planet::Complete() noexcept
{
    if (state_ == psCompleted)
        return true;
    if (state_ != psOpen)
        return false;
    SetState(psCompleted);
    return true;
}

void Planet::NextPass() noexcept
{
    SetPass(pass_ + 1);
    if (pass_ >= 3)
        Complete();
}

int Planet::GetPass() const noexcept
{
    return pass_;
}

void Planet::SetPass(int value) noexcept
{
    // Original CompletePass/StartPass callbacks unlock inventory and apply
    // AI loadouts.  Those systems have their own portable adapters; this
    // source-rules class owns the identical state transition itself.
    pass_ = value;
}

void Planet::Reset() noexcept
{
    state_ = psUnavailable;
    pass_ = 0;
}

std::uint32_t Planet::GetIndex() const noexcept
{
    return index_;
}

Planet::State Planet::GetState() const noexcept
{
    return state_;
}

void Planet::SetState(State value) noexcept
{
    if (state_ == value)
        return;
    if (value == psClosed)
        SetPass(0);
    else if (value == psOpen)
        SetPass(1);
    state_ = value;
}

void Planet::Restore(State state, int pass) noexcept
{
    // Serialization restores both independent fields without invoking the
    // runtime inventory callbacks, matching SnProfile::LoadTournament.
    state_ = state;
    pass_ = pass;
}

const Planet::RequestPoints& Planet::GetRequestPoints() const noexcept
{
    return requestPoints_;
}

void Planet::SetRequestPoints(RequestPoints value)
{
    requestPoints_ = std::move(value);
}

int Planet::GetRequestPoints(
    int pass, std::uint32_t humanOrOpponentCount) const noexcept
{
    if (requestPoints_.empty())
        return -1;
    const auto found = requestPoints_.find(pass);
    const int points = found != requestPoints_.end()
                           ? found->second
                           : requestPoints_.rbegin()->second;
    return scaledRequestPoints(
        points, std::max(humanOrOpponentCount, 1U));
}

bool Planet::HasRequestPoints(
    int pass, int points,
    std::uint32_t humanOrOpponentCount) const noexcept
{
    const int required =
        GetRequestPoints(pass, humanOrOpponentCount);
    return required != -1 && points >= required;
}

Planet::Price Planet::GetPrice(int place) const noexcept
{
    if (place <= 0 || static_cast<std::size_t>(place) > prices_.size())
        return {};
    return prices_[static_cast<std::size_t>(place - 1)];
}

void Planet::SetPrices(Prices value)
{
    prices_ = std::move(value);
}

Tournament::Tournament(bool campaign) noexcept : campaign_(campaign) {}

Planet& Tournament::AddPlanet()
{
    planets_.emplace_back(static_cast<std::uint32_t>(planets_.size()));
    return planets_.back();
}

void Tournament::ClearPlanets() noexcept
{
    planets_.clear();
    curTrack_ = nullptr;
    trackList_.clear();
    hasCurPlanet_ = false;
}

Planet* Tournament::GetPlanet(std::size_t index) noexcept
{
    return index < planets_.size() ? &planets_[index] : nullptr;
}

const Planet* Tournament::GetPlanet(std::size_t index) const noexcept
{
    return index < planets_.size() ? &planets_[index] : nullptr;
}

bool Tournament::SetCurPlanet(std::size_t index) noexcept
{
    if (index >= planets_.size())
        return false;
    curPlanet_ = index;
    hasCurPlanet_ = true;
    trackList_.clear();
    curTrack_ = NextTrack(nullptr);
    return curTrack_ != nullptr;
}

bool Tournament::SetCurTrack(std::size_t catalogIndex) noexcept
{
    for (std::size_t planetIndex = 0U;
         planetIndex < planets_.size(); ++planetIndex)
    {
        for (const auto& [pass, tracks] :
             planets_[planetIndex].GetTrackMap())
        {
            (void)pass;
            const auto found = std::find_if(
                tracks.begin(), tracks.end(),
                [catalogIndex](const Planet::Track& track) {
                    return track.catalogIndex == catalogIndex;
                });
            if (found == tracks.end())
                continue;
            curPlanet_ = planetIndex;
            hasCurPlanet_ = true;
            curTrack_ = &*found;
            return true;
        }
    }
    return false;
}

bool Tournament::Select(std::size_t planetIndex, int pass,
                        std::size_t localTrack) noexcept
{
    auto* planet = GetPlanet(planetIndex);
    if (planet == nullptr)
        return false;
    planet->SetPass(pass);
    const auto& tracks = planet->GetTracks();
    if (tracks.empty())
        return false;
    curPlanet_ = planetIndex;
    hasCurPlanet_ = true;
    curTrack_ = &tracks[std::min(localTrack, tracks.size() - 1U)];
    return true;
}

const Planet* Tournament::GetCurPlanet() const noexcept
{
    return hasCurPlanet_ ? GetPlanet(curPlanet_) : nullptr;
}

const Planet::Track* Tournament::GetCurTrack() const noexcept
{
    return curTrack_;
}

std::size_t Tournament::GetCurTrackIndex() const noexcept
{
    return curTrack_ != nullptr ? curTrack_->index : 0U;
}

const Planet::Track* Tournament::NextTrack(
    const Planet::Track* track) noexcept
{
    const auto* planet = GetCurPlanet();
    if (planet == nullptr)
        return nullptr;
    if (campaign_)
    {
        const auto& tracks = planet->GetTracks();
        if (track == nullptr)
            return tracks.empty() ? nullptr : &tracks.front();
        return planet->NextTrack(track);
    }
    if (trackList_.empty())
    {
        for (const auto& [pass, tracks] : planet->GetTrackMap())
        {
            (void)pass;
            const auto first = trackList_.size();
            for (const auto& candidate : tracks)
                trackList_.push_back(&candidate);
            // std::random_shuffle in the Windows source used the process
            // rand() stream and shuffled every pass slice independently.
            for (std::size_t count = trackList_.size() - first;
                 count > 1U; --count)
            {
                const auto selected = static_cast<std::size_t>(
                    std::rand()) % count;
                std::swap(trackList_[first + count - 1U],
                          trackList_[first + selected]);
            }
        }
    }
    if (trackList_.empty())
        return nullptr;
    const auto* next = trackList_.front();
    trackList_.erase(trackList_.begin());
    return next;
}

Tournament::Advance Tournament::CompleteTrack(
    int points, std::uint32_t humanOrOpponentCount) noexcept
{
    Advance result;
    auto* planet = hasCurPlanet_ ? GetPlanet(curPlanet_) : nullptr;
    if (planet == nullptr || curTrack_ == nullptr)
        return result;

    const auto* nextTrack = NextTrack(curTrack_);
    if (nextTrack == nullptr)
    {
        result.passComplete = true;
        if (planet->HasRequestPoints(
                planet->GetPass(), points, humanOrOpponentCount))
        {
            planet->NextPass();
            result.passChampion = true;
            result.planetChampion =
                planet->GetState() == Planet::psCompleted;
        }
        nextTrack = NextTrack(nullptr);
    }

    curTrack_ = nextTrack;
    result.planetPass = planet->GetPass();
    if (curTrack_ != nullptr)
        result.trackIndex = curTrack_->catalogIndex;
    return result;
}

void Tournament::Reset() noexcept
{
    for (auto& planet : planets_)
        planet.Reset();
    curTrack_ = nullptr;
    trackList_.clear();
    hasCurPlanet_ = false;
}

} // namespace r3d::game::originalrace::source
