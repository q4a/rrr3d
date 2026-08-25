#include "OriginalRaceCommentator.h"

#include "resource/ResourceFileSystem.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <exception>
#include <utility>

namespace rrr3d::audio
{
namespace
{

std::string voiceFile(std::string_view serialized)
{
    if (serialized.empty())
        return {};
    std::string result(serialized);
    const auto separator = result.find_last_of("\\/");
    if (separator != std::string::npos)
        result.erase(0U, separator + 1U);
    return result;
}

float randomUnit()
{
    // lsl::Random() in the Windows source uses this exact expression.
    return std::rand() / static_cast<float>(RAND_MAX);
}

} // namespace

OriginalRaceCommentator::OriginalRaceCommentator(
    r3d::audio::AudioBackend& audio,
    const r3d::resource::ResourceFileSystem& resources,
    const r3d::game::originalgamedata::Catalog& gameData)
    : audio_(audio), resources_(resources), gameData_(gameData)
{
}

OriginalRaceCommentator::~OriginalRaceCommentator()
{
    shutdown();
}

bool OriginalRaceCommentator::initialize(
    std::string_view style, std::string& error)
{
    shutdown();
    const auto styleEntry = std::find(
        gameData_.commentatorStyles.begin(),
        gameData_.commentatorStyles.end(), style);
    if (styleEntry == gameData_.commentatorStyles.end())
    {
        error = "game.xml does not declare commentator style: " +
                std::string(style);
        return false;
    }
    const std::string selected(style);
    try
    {
        globalDelaySeconds_ = gameData_.commentator.delay;
        for (const auto& [name, serialized] :
             gameData_.commentator.comments)
        {
            Comment comment;
            comment.chance = serialized.chance;
            comment.delay = serialized.delay;
            comment.repeatPlayer = serialized.repeatPlayer;
            if (serialized.busy ==
                r3d::game::originalgamedata::
                    CommentatorBusyAction::Queue)
                comment.busy = BusyAction::Queue;
            else if (serialized.busy ==
                     r3d::game::originalgamedata::
                         CommentatorBusyAction::Replace)
                comment.busy = BusyAction::Replace;

            for (const auto& voice : serialized.voices)
            {
                CommentVoice loadedVoice;
                loadedVoice.weight = voice.weight;
                loadedVoice.startPlayer = voice.startPlayer;
                loadedVoice.endPlayer = voice.endPlayer;
                loadedVoice.humanOnly = voice.humanOnly;
                const std::string file = voiceFile(voice.sound);
                const std::string path =
                    "Data/Voice/" + selected + "/" + file;
                if (!file.empty() && resources_.exists(path))
                {
                    const auto cached = loadedSounds_.find(path);
                    if (cached != loadedSounds_.end())
                    {
                        loadedVoice.sound = cached->second;
                    }
                    else
                    {
                        r3d::audio::SoundInfo info;
                        const auto sound = audio_.loadOgg(
                            resources_.resolve(path), info, error);
                        if (sound != r3d::audio::invalidSound)
                        {
                            loadedSounds_.emplace(path, sound);
                            loadedVoice.sound = sound;
                        }
                    }
                }
                // ResourceManager::LoadCommentator preserves unavailable
                // per-language alternatives and filters them in Generate.
                comment.voices.push_back(loadedVoice);
            }
            comments_.emplace(name, std::move(comment));
        }
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        shutdown();
        return false;
    }

    constexpr std::array<std::string_view, 5> required{{
        "raceStartTime2", "playerMoveInverse", "playerFinishLast",
        "playerDomination", "raceFinish"}};
    const bool definitionsComplete = std::all_of(
        required.begin(), required.end(), [&](std::string_view name) {
            return comments_.find(std::string(name)) != comments_.end();
        });
    if (!definitionsComplete || loadedSounds_.empty())
    {
        error = "Original serialized commentator table is incomplete";
        shutdown();
        return false;
    }

    initialized_ = true;
    reset();
    error.clear();
    return true;
}

void OriginalRaceCommentator::shutdown() noexcept
{
    stop();
    for (const auto& [path, sound] : loadedSounds_)
    {
        static_cast<void>(path);
        audio_.unloadSound(sound);
    }
    comments_.clear();
    loadedSounds_.clear();
    globalDelaySeconds_ = 0.0F;
    timeSeconds_ = 0.0F;
    silenceSeconds_ = 0.0F;
    initialized_ = false;
    paused_ = false;
}

void OriginalRaceCommentator::stop() noexcept
{
    if (voice_ != r3d::audio::invalidVoice)
        audio_.stop(voice_);
    voice_ = r3d::audio::invalidVoice;
    queue_.clear();
}

void OriginalRaceCommentator::reset()
{
    if (!initialized_)
        return;
    stop();
    timeSeconds_ = 0.0F;
    silenceSeconds_ = 0.0F;
    for (auto& [name, comment] : comments_)
    {
        static_cast<void>(name);
        comment.nextSeconds = 0.0F;
        comment.lastPlayer =
            r3d::game::originalrace::source::Player::undefinedId;
    }
}

const OriginalRaceCommentator::CommentVoice*
OriginalRaceCommentator::generate(Comment& comment, int playerId)
{
    if (comment.chance < randomUnit() * 100.0F)
        return nullptr;
    if (!comment.repeatPlayer &&
        playerId !=
            r3d::game::originalrace::source::Player::undefinedId &&
        comment.lastPlayer == playerId)
        return nullptr;
    if (timeSeconds_ < comment.nextSeconds)
        return nullptr;

    comment.nextSeconds = timeSeconds_ + comment.delay;
    comment.lastPlayer = playerId;
    float totalWeight = 0.0F;
    for (const auto& voice : comment.voices)
    {
        if (voice.sound != r3d::audio::invalidSound &&
            (!voice.humanOnly ||
             playerId ==
                 r3d::game::originalrace::source::Player::humanId))
            totalWeight += voice.weight;
    }
    const float selectedWeight = totalWeight * randomUnit();
    float accumulatedWeight = 0.0F;
    for (const auto& voice : comment.voices)
    {
        if (voice.sound == r3d::audio::invalidSound ||
            (voice.humanOnly &&
             playerId !=
                 r3d::game::originalrace::source::Player::humanId))
            continue;
        if (selectedWeight >= accumulatedWeight &&
            selectedWeight <= accumulatedWeight + voice.weight)
            return &voice;
        accumulatedWeight += voice.weight;
    }
    return nullptr;
}

bool OriginalRaceCommentator::isSpeaking() const noexcept
{
    return (voice_ != r3d::audio::invalidVoice &&
            audio_.isVoiceActive(voice_)) ||
           !queue_.empty();
}

void OriginalRaceCommentator::enqueue(
    std::string_view commentName,
    const r3d::game::originalrace::Race* race,
    std::size_t racer, int playerId,
    std::string& error)
{
    const auto found = comments_.find(std::string(commentName));
    if (found == comments_.end())
        return;
    auto& comment = found->second;
    if (comment.busy == BusyAction::Skip &&
        (globalDelaySeconds_ > silenceSeconds_ || isSpeaking()))
        return;

    const CommentVoice* selected = generate(comment, playerId);
    if (selected == nullptr)
        return;
    std::deque<r3d::audio::SoundHandle> utterance;
    if (selected->startPlayer)
    {
        if (race == nullptr || racer >= race->racers.size())
            return;
        const auto player = comments_.find(race->racers[racer].name);
        if (player == comments_.end())
            return;
        const auto* prefix = generate(player->second, playerId);
        if (prefix == nullptr)
            return;
        utterance.push_back(prefix->sound);
    }
    utterance.push_back(selected->sound);
    if (selected->endPlayer)
    {
        if (race == nullptr || racer >= race->racers.size())
            return;
        const auto player = comments_.find(race->racers[racer].name);
        if (player == comments_.end())
            return;
        const auto* suffix = generate(player->second, playerId);
        if (suffix == nullptr)
            return;
        utterance.push_back(suffix->sound);
    }

    const bool wasSpeaking = isSpeaking();
    const bool replace = comment.busy == BusyAction::Replace ||
                         comment.busy == BusyAction::Skip;
    if (replace)
    {
        if (voice_ != r3d::audio::invalidVoice)
            audio_.stop(voice_);
        voice_ = r3d::audio::invalidVoice;
        queue_.clear();
    }
    queue_.insert(queue_.end(), utterance.begin(), utterance.end());
    if (replace || !wasSpeaking)
    {
        playNext(error);
        silenceSeconds_ = 0.0F;
    }
}

void OriginalRaceCommentator::playNext(std::string& error)
{
    if (paused_ || queue_.empty())
        return;
    if (voice_ != r3d::audio::invalidVoice &&
        audio_.isVoiceActive(voice_))
        return;
    voice_ = r3d::audio::invalidVoice;
    const auto sound = queue_.front();
    queue_.pop_front();
    r3d::audio::PlayOptions options;
    options.bus = r3d::audio::Bus::Voice;
    voice_ = audio_.play(sound, options, error);
}

void OriginalRaceCommentator::progress(
    float seconds, std::string& error)
{
    if (!initialized_ || paused_)
        return;
    const float elapsed = std::max(seconds, 0.0F);
    timeSeconds_ += elapsed;
    silenceSeconds_ += elapsed;
    // The Windows Source3d report calls Commentator::OnStreamEnd, which
    // immediately starts the next queued sound.  AudioBackend is polled, so
    // do the equivalent once per application frame.
    playNext(error);
}

void OriginalRaceCommentator::update(
    const r3d::game::originalrace::Race& race,
    const r3d::game::originalrace::OriginalRaceSession& session,
    float,
    std::string& error)
{
    using namespace r3d::game::originalrace;
    if (!initialized_)
        return;
    for (const auto& event : session.events())
    {
        std::string_view name;
        bool hasPlayer = true;
        switch (event.kind)
        {
        case RaceEventKind::CountdownChanged:
            hasPlayer = false;
            if (event.value == 2.0F)
                name = "raceStartTime2";
            break;
        case RaceEventKind::LastLap:
            name = "raceLastLap";
            break;
        case RaceEventKind::Overboard:
            name = "playerOverboard";
            break;
        case RaceEventKind::DeathMine:
            name = "playerDeathMine";
            break;
        case RaceEventKind::MoveInverse:
            name = "playerMoveInverse";
            break;
        case RaceEventKind::LostControl:
            name = "playerLostControl";
            break;
        case RaceEventKind::LeadFinish:
            name = "playerLeadFinish";
            break;
        case RaceEventKind::SecondFinish:
            name = "playerSecondFinish";
            break;
        case RaceEventKind::ThirdFinish:
            name = "playerThirdFinish";
            break;
        case RaceEventKind::LastFinish:
            name = "playerLastFinish";
            break;
        case RaceEventKind::LeadChanged:
            name = "playerLeadChanged";
            break;
        case RaceEventKind::ThirdChanged:
            name = "playerThirdChanged";
            break;
        case RaceEventKind::ThirdFar:
            name = "playerThirdFar";
            break;
        case RaceEventKind::LastFar:
            name = "playerLastFar";
            break;
        case RaceEventKind::Domination:
            name = "playerDomination";
            break;
        case RaceEventKind::LowLife:
            name = "playerLowLife";
            break;
        case RaceEventKind::Death:
            name = "playerDeath";
            break;
        case RaceEventKind::Kill:
            if (event.killCredit)
                name = "playerKill";
            break;
        case RaceEventKind::SpeedArrow:
            name = "playerSpeedArrow";
            break;
        case RaceEventKind::RaceFinish:
            hasPlayer = false;
            name = "raceFinish";
            break;
        default:
            break;
        }
        if (!name.empty())
        {
            const int playerId =
                hasPlayer && event.racer < race.racers.size()
                    ? race.racers[event.racer].playerId
                    : source::Player::undefinedId;
            enqueue(name, &race, event.racer, playerId, error);
        }
    }
    playNext(error);
}

void OriginalRaceCommentator::finishPlace(
    const r3d::game::originalrace::Race& race,
    std::size_t racer, std::uint32_t place,
    std::string& error)
{
    if (!initialized_)
        return;
    std::string_view name = "playerFinishLast";
    if (place == 1U)
        name = "playerFinishFirst";
    else if (place == 2U)
        name = "playerFinishSecond";
    else if (place == 3U)
        name = "playerFinishThird";
    const int playerId =
        racer < race.racers.size()
            ? race.racers[racer].playerId
            : r3d::game::originalrace::source::Player::undefinedId;
    enqueue(name, &race, racer, playerId, error);
    playNext(error);
}

void OriginalRaceCommentator::pause(bool paused) noexcept
{
    paused_ = paused;
    if (voice_ != r3d::audio::invalidVoice)
        audio_.setVoicePaused(voice_, paused);
}

std::size_t OriginalRaceCommentator::commentCount() const noexcept
{
    return comments_.size();
}

std::size_t OriginalRaceCommentator::loadedVoiceCount() const noexcept
{
    return loadedSounds_.size();
}

} // namespace rrr3d::audio
