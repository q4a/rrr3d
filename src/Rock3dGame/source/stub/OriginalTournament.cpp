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

void Planet::Unlock()
{
    if (state_ == psUnavailable || state_ == psCompleted)
        SetState(psClosed);
}

bool Planet::Open()
{
    if (state_ == psUnavailable)
        return false;
    if (state_ == psOpen || state_ == psCompleted)
        return true;
    SetState(psOpen);
    return true;
}

bool Planet::Complete()
{
    if (state_ == psCompleted)
        return true;
    if (state_ != psOpen)
        return false;
    SetState(psCompleted);
    return true;
}

void Planet::StartPass(int pass, PlayerState& player,
                       bool campaign) const
{
    if (pass <= 0 || !player.computer)
        return;

    int playerId = player.id;
    if (playerId > 5)
    {
        playerId = campaign ? (playerId - 1) % 4 + 2
                            : (playerId - 1) % 5 + 1;
    }
    const auto* data = GetPlayer(playerId);
    if (data == nullptr)
        return;

    if (pass > data->maxPass)
        pass = data->maxPass;
    player.car.clear();
    player.slots.clear();
    for (const auto& car : data->cars)
    {
        if (car.pass == pass)
        {
            player.car = car.record;
            break;
        }
    }
    if (!player.car.empty())
    {
        for (const auto& slot : data->slots)
        {
            if (slot.pass == pass)
                player.slots.push_back(slot);
        }
    }
    player.maximizeForSkirmish = !campaign && !player.car.empty();
}

void Planet::StartPass(int pass, std::vector<PlayerState>& players,
                       bool campaign) const
{
    for (auto& player : players)
        StartPass(pass, player, campaign);
}

void Planet::CompletePass(int pass)
{
    for (const auto& slot : slots_)
    {
        if (slot.pass == pass)
            completedSlots_.push_back(slot.record);
    }
    for (const auto& car : cars_)
    {
        if (car.pass == pass)
            completedCars_.push_back(car.record);
    }
}

void Planet::NextPass()
{
    SetPass(pass_ + 1);
    if (pass_ >= 3)
        Complete();
}

int Planet::GetPass() const noexcept
{
    return pass_;
}

void Planet::SetPass(int value)
{
    if (pass_ == value)
        return;
    if (pass_ >= 0)
        CompletePass(pass_);
    pass_ = value;
}

void Planet::Reset() noexcept
{
    state_ = psUnavailable;
    pass_ = 0;
    completedSlots_.clear();
    completedCars_.clear();
}

std::uint32_t Planet::GetIndex() const noexcept
{
    return index_;
}

int Planet::GetId() const noexcept
{
    const auto* boss = players_.empty() ? nullptr : &players_.front();
    return boss == nullptr ? -1 : boss->id;
}

const std::string& Planet::GetName() const noexcept
{
    return name_;
}

void Planet::SetName(std::string value)
{
    name_ = std::move(value);
}

Planet::State Planet::GetState() const noexcept
{
    return state_;
}

void Planet::SetState(State value)
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
    completedSlots_.clear();
    completedCars_.clear();
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

Planet::Wheater Planet::GenerateWheater(
    bool allowNight, bool mostProbable, float randomUnit) const noexcept
{
    // Environment::ewNight is the second serialized enum value in both the
    // Windows source and the portable Weather enum.
    constexpr int night = 1;
    Wheater maximum{};
    float maximumChance = 0.0F;
    float chanceSum = 0.0F;
    for (const auto& wheater : wheaters_)
    {
        if (wheater.type == night && !allowNight)
            continue;
        if (maximumChance < wheater.chance)
        {
            maximumChance = wheater.chance;
            maximum = wheater;
        }
        chanceSum += wheater.chance;
    }
    if (mostProbable)
        return maximum;

    const float wanted = chanceSum *
        std::clamp(randomUnit, 0.0F, 1.0F);
    chanceSum = 0.0F;
    for (const auto& wheater : wheaters_)
    {
        if (wheater.type == night && !allowNight)
            continue;
        if (wanted >= chanceSum &&
            wanted <= chanceSum + wheater.chance)
            return wheater;
        chanceSum += wheater.chance;
    }
    return !wheaters_.empty() ? wheaters_.front() : maximum;
}

void Planet::SetWheaters(Wheaters value)
{
    wheaters_ = std::move(value);
}

const Planet::Wheaters& Planet::GetWheaters() const noexcept
{
    return wheaters_;
}

void Planet::InsertSlot(SlotData slot)
{
    if (!slot.record.empty())
        slots_.push_back(std::move(slot));
}

void Planet::ClearSlots() noexcept
{
    slots_.clear();
}

void Planet::SetSlots(Slots value)
{
    slots_ = std::move(value);
}

const Planet::Slots& Planet::GetSlots() const noexcept
{
    return slots_;
}

void Planet::InsertCar(CarData car)
{
    if (!car.record.empty())
        cars_.push_back(std::move(car));
}

void Planet::ClearCars() noexcept
{
    cars_.clear();
}

void Planet::SetCars(Cars value)
{
    cars_ = std::move(value);
}

const Planet::Cars& Planet::GetCars() const noexcept
{
    return cars_;
}

void Planet::InsertPlayer(PlayerData player)
{
    players_.push_back(std::move(player));
}

void Planet::ClearPlayers() noexcept
{
    players_.clear();
}

const Planet::PlayerData* Planet::GetPlayer(int id) const noexcept
{
    const auto found = std::find_if(
        players_.begin(), players_.end(),
        [id](const PlayerData& player) { return player.id == id; });
    return found == players_.end() ? nullptr : &*found;
}

const Planet::PlayerData* Planet::GetPlayer(
    const std::string& name) const noexcept
{
    const auto found = std::find_if(
        players_.begin(), players_.end(),
        [&name](const PlayerData& player) { return player.name == name; });
    return found == players_.end() ? nullptr : &*found;
}

Planet::PlayerData Planet::GetBoss() const
{
    return players_.empty() ? PlayerData{} : players_.front();
}

const Planet::Players& Planet::GetPlayers() const noexcept
{
    return players_;
}

std::vector<std::string> Planet::TakeCompletedSlots() noexcept
{
    return std::move(completedSlots_);
}

std::vector<std::string> Planet::TakeCompletedCars() noexcept
{
    return std::move(completedCars_);
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

Planet* Tournament::NextPlanet(std::size_t index) noexcept
{
    return index + 1U < planets_.size() ? &planets_[index + 1U]
                                        : nullptr;
}

Planet* Tournament::PrevPlanet(std::size_t index) noexcept
{
    return index > 0U && index <= planets_.size()
               ? &planets_[index - 1U]
               : nullptr;
}

Planet& Tournament::AddGamer()
{
    gamers_.emplace_back(static_cast<std::uint32_t>(gamers_.size()));
    return gamers_.back();
}

void Tournament::ClearGamers() noexcept
{
    gamers_.clear();
}

Planet* Tournament::GetGamer(int gamerId) noexcept
{
    const auto found = std::find_if(
        gamers_.begin(), gamers_.end(),
        [gamerId](const Planet& gamer) {
            return gamer.GetId() == gamerId;
        });
    return found == gamers_.end() ? nullptr : &*found;
}

const Planet::PlayerData* Tournament::GetPlayerData(int id) const noexcept
{
    for (const auto& gamer : gamers_)
    {
        if (const auto* player = gamer.GetPlayer(id))
            return player;
    }
    const auto* planet = GetCurPlanet();
    return planet == nullptr ? nullptr : planet->GetPlayer(id);
}

const Planet::PlayerData* Tournament::GetPlayerData(
    const std::string& name) const noexcept
{
    for (const auto& gamer : gamers_)
    {
        if (const auto* player = gamer.GetPlayer(name))
            return player;
    }
    const auto* planet = GetCurPlanet();
    return planet == nullptr ? nullptr : planet->GetPlayer(name);
}

bool Tournament::SetCurPlanet(std::size_t index)
{
    if (index >= planets_.size())
        return false;
    if (hasCurPlanet_ && curPlanet_ == index)
        return true;
    curPlanet_ = index;
    hasCurPlanet_ = true;
    trackList_.clear();
    curTrack_ = NextTrack(nullptr);
    return curTrack_ != nullptr;
}

bool Tournament::ChangePlanet(std::size_t index)
{
    auto* planet = GetPlanet(index);
    if (planet == nullptr)
        return false;
    if (hasCurPlanet_ && curPlanet_ == index)
        return true;
    if (!planet->Open())
        planet->SetPass(1);
    return SetCurPlanet(index);
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
                        std::size_t localTrack)
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

std::size_t Tournament::GetCurPlanetIndex() const noexcept
{
    return hasCurPlanet_ ? curPlanet_ : 0U;
}

Planet* Tournament::GetNextPlanet() noexcept
{
    return hasCurPlanet_ ? NextPlanet(curPlanet_) : nullptr;
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
    const Planet::Track* track)
{
    const auto* planet = GetCurPlanet();
    if (planet == nullptr)
        return nullptr;
    if (campaign_)
    {
        const auto& tracks = planet->GetTracks();
        if (track == nullptr)
        {
            wheaterNightPass_ = false;
            return tracks.empty() ? nullptr : &tracks.front();
        }
        return planet->NextTrack(track);
    }
    if (trackList_.empty())
    {
        wheaterNightPass_ = false;
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

Planet::Wheater Tournament::SelectWheater(
    bool allowNight, bool mostProbable, float randomUnit) noexcept
{
    const auto* planet = GetCurPlanet();
    if (planet == nullptr)
        return {};
    const auto result = planet->GenerateWheater(
        allowNight && !wheaterNightPass_, mostProbable, randomUnit);
    wheater_ = result.type;
    wheaterNightPass_ = wheaterNightPass_ || wheater_ == 1;
    return result;
}

int Tournament::GetWheater() const noexcept { return wheater_; }

bool Tournament::GetWheaterNightPass() const noexcept
{
    return wheaterNightPass_;
}

void Tournament::ResetWheaterNightPass() noexcept
{
    wheaterNightPass_ = false;
}

Tournament::Advance Tournament::CompleteTrack(
    int points, std::uint32_t humanOrOpponentCount)
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
            result.unlockedSlots = planet->TakeCompletedSlots();
            result.unlockedCars = planet->TakeCompletedCars();
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
    for (auto& gamer : gamers_)
        gamer.Reset();
    curTrack_ = nullptr;
    trackList_.clear();
    hasCurPlanet_ = false;
    wheater_ = 0;
    wheaterNightPass_ = false;
}

} // namespace r3d::game::originalrace::source
