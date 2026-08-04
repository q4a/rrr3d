#include "OriginalRaceCommentator.h"

#include "resource/ResourceFileSystem.h"

#include <algorithm>
#include <array>
#include <exception>
#include <utility>

namespace rrr3d::audio
{
namespace
{

using Cue = OriginalRaceCommentator::Cue;

struct CueFiles
{
    Cue cue;
    std::vector<std::string_view> files;
    bool playerPrefix = false;
    bool humanOnly = false;
};

const std::array<CueFiles, 17> cueFiles{{
    {Cue::Start, {"start1.ogg", "start2.ogg", "start3.ogg",
                  "start4.ogg"}},
    {Cue::LastLap, {"lastLap1.ogg", "lastLap2.ogg", "lastLap3.ogg",
                    "lastLap4.ogg", "lastLap5.ogg", "lastLap6.ogg",
                    "lastLap7.ogg"}},
    {Cue::Overboard, {"lowLuck1.ogg", "overboard1.ogg"},
     false, true},
    {Cue::Overboard, {"playerLostControl1.ogg",
                      "playerLostControl2.ogg",
                      "playerLostControl3.ogg"},
     true, true},
    {Cue::DeathMine, {"lowLuck1.ogg"}, false, true},
    {Cue::WrongWay, {"playerMoveInverse1.ogg"}, true},
    {Cue::LostControl, {"playerLostControl1.ogg",
                        "playerLostControl2.ogg",
                        "playerLostControl3.ogg"}, true},
    {Cue::LeaderFinish, {"leaderFinish1.ogg", "leaderFinish2.ogg",
                         "leaderFinish3.ogg", "leaderFinish4.ogg",
                         "leaderFinish5.ogg", "leaderFinish6.ogg"},
     true},
    {Cue::LeaderChanged, {"leaderChanged1.ogg", "leaderChanged2.ogg",
                          "leaderChanged3.ogg", "leaderChanged4.ogg",
                          "leaderChanged5.ogg", "leaderChanged6.ogg"},
     true},
    {Cue::LastFar, {"lastFar1.ogg", "lastFar2.ogg", "lastFar3.ogg",
                    "lastFar4.ogg", "lastFar5.ogg", "lastFar6.ogg",
                    "lastFar7.ogg", "lastFar8.ogg", "lastFar9.ogg",
                    "lastFar10.ogg"}, true},
    {Cue::LowLife, {"lowLife1.ogg", "lowLife2.ogg", "lowLife3.ogg",
                    "lowLife4.ogg", "lowLife5.ogg", "lowLife6.ogg",
                    "lowLife7.ogg"}, true},
    {Cue::Kill, {"playerKill1.ogg", "playerKill2.ogg",
                 "playerKill3.ogg"}, false, true},
    {Cue::Death, {"death1.ogg"}, true},
    {Cue::FinishFirst, {"finishFirst1.ogg"}, true},
    {Cue::FinishSecond, {"finishSecond1.ogg"}, true},
    // GameMode::Start registers finishLast1 for cPlayerFinishThird.  This
    // surprising mapping is intentional and is not corrected in the port.
    {Cue::FinishThird, {"finishLast1.ogg"}, true},
    {Cue::RaceFinish, {"finish1.ogg"}},
}};

const std::array<std::pair<std::string_view, std::string_view>, 4>
    playerFiles{{
        {"svRip", "rip.ogg"},
        {"svSnake", "snake.ogg"},
        {"svTyler", "tailer.ogg"},
        {"svTarkvin", "tarkvin.ogg"},
    }};

} // namespace

OriginalRaceCommentator::OriginalRaceCommentator(
    r3d::audio::AudioBackend& audio,
    const r3d::resource::ResourceFileSystem& resources)
    : audio_(audio), resources_(resources)
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
    const std::string selected =
        style == "russian" ? "russian" : "english";
    try
    {
        for (const auto& definition : cueFiles)
        {
            auto& loaded = sounds_[definition.cue];
            for (const auto file : definition.files)
            {
                const std::string path =
                    "Data/Voice/" + selected + "/" +
                    std::string(file);
                if (!resources_.exists(path))
                    continue;
                r3d::audio::SoundInfo info;
                auto sound = audio_.loadOgg(
                    resources_.resolve(path), info, error);
                if (sound == r3d::audio::invalidSound)
                    continue;
                loaded.push_back(
                    {sound, definition.playerPrefix,
                     definition.humanOnly});
            }
        }
        for (const auto& [player, file] : playerFiles)
        {
            const std::string path =
                "Data/Voice/" + selected + "/" +
                std::string(file);
            if (!resources_.exists(path))
                continue;
            r3d::audio::SoundInfo info;
            auto sound = audio_.loadOgg(
                resources_.resolve(path), info, error);
            if (sound == r3d::audio::invalidSound)
                continue;
            playerSounds_[std::string(player)] = sound;
        }
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        shutdown();
        return false;
    }
    // ResourceManager::LoadCommentator also tolerates absent per-style
    // variants.  The shipped English set has no finish1.ogg, while the
    // finish-place lines are present and remain playable.
    if (sounds_[Cue::Start].empty())
    {
        error = "Original commentator voice set is incomplete";
        shutdown();
        return false;
    }
    initialized_ = true;
    error.clear();
    return true;
}

void OriginalRaceCommentator::shutdown() noexcept
{
    if (voice_ != r3d::audio::invalidVoice)
        audio_.stop(voice_);
    voice_ = r3d::audio::invalidVoice;
    queue_.clear();
    for (auto& [cue, sounds] : sounds_)
    {
        static_cast<void>(cue);
        for (const auto& voice : sounds)
            audio_.unloadSound(voice.sound);
    }
    for (const auto& [player, sound] : playerSounds_)
    {
        static_cast<void>(player);
        audio_.unloadSound(sound);
    }
    sounds_.clear();
    playerSounds_.clear();
    nextSound_.clear();
    nextCueSeconds_.clear();
    lastCuePlayer_.clear();
    initialized_ = false;
    paused_ = false;
}

void OriginalRaceCommentator::reset()
{
    if (!initialized_)
        return;
    if (voice_ != r3d::audio::invalidVoice)
        audio_.stop(voice_);
    voice_ = r3d::audio::invalidVoice;
    queue_.clear();
    nextCueSeconds_.clear();
    lastCuePlayer_.clear();
    enqueue(Cue::Start, nullptr,
            r3d::game::originalrace::RacerRuntime::invalidWeapon,
            false, true);
}

void OriginalRaceCommentator::enqueue(
    Cue cue,
    const r3d::game::originalrace::Race* race,
    std::size_t racer,
    bool replace, bool skipWhenBusy,
    float now, float delay, bool repeatPlayer)
{
    const auto found = sounds_.find(cue);
    if (found == sounds_.end() || found->second.empty())
        return;
    const bool busy =
        voice_ != r3d::audio::invalidVoice &&
        audio_.isVoiceActive(voice_);
    if (skipWhenBusy && (busy || !queue_.empty()))
        return;
    const auto ready = nextCueSeconds_.find(cue);
    if (ready != nextCueSeconds_.end() && now < ready->second)
        return;
    const auto previousPlayer = lastCuePlayer_.find(cue);
    if (!repeatPlayer &&
        racer != r3d::game::originalrace::RacerRuntime::invalidWeapon &&
        previousPlayer != lastCuePlayer_.end() &&
        previousPlayer->second == racer)
        return;
    if (replace)
    {
        if (voice_ != r3d::audio::invalidVoice)
            audio_.stop(voice_);
        voice_ = r3d::audio::invalidVoice;
        queue_.clear();
    }
    auto& index = nextSound_[cue];
    const CueVoice* selected = nullptr;
    for (std::size_t attempt = 0U;
         attempt < found->second.size(); ++attempt)
    {
        const auto& candidate = found->second[
            (index + attempt) % found->second.size()];
        if (!candidate.humanOnly || racer == 0U)
        {
            selected = &candidate;
            index += attempt + 1U;
            break;
        }
    }
    if (selected == nullptr)
        return;
    nextCueSeconds_[cue] = now + delay;
    if (racer != r3d::game::originalrace::RacerRuntime::invalidWeapon)
        lastCuePlayer_[cue] = racer;
    if (selected->playerPrefix)
    {
        if (race == nullptr || racer >= race->racers.size())
            return;
        const auto player = playerSounds_.find(race->racers[racer].name);
        if (player == playerSounds_.end())
            return;
        queue_.push_back(player->second);
    }
    queue_.push_back(selected->sound);
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

void OriginalRaceCommentator::update(
    const r3d::game::originalrace::Race& race,
    const r3d::game::originalrace::OriginalRaceSession& session,
    std::string& error)
{
    using namespace r3d::game::originalrace;
    if (!initialized_ || session.racers().empty())
        return;
    const float now = session.elapsedSeconds();
    for (const auto& event : session.events())
    {
        switch (event.kind)
        {
        case RaceEventKind::LastLap:
            enqueue(Cue::LastLap, &race, event.racer,
                    false, false, now);
            break;
        case RaceEventKind::Overboard:
            enqueue(Cue::Overboard, &race, event.racer,
                    false, true, now);
            break;
        case RaceEventKind::DeathMine:
            enqueue(Cue::DeathMine, &race, event.racer,
                    false, true, now);
            break;
        case RaceEventKind::MoveInverse:
            enqueue(Cue::WrongWay, &race, event.racer,
                    false, false, now, 0.0F, false);
            break;
        case RaceEventKind::LostControl:
            enqueue(Cue::LostControl, &race, event.racer,
                    false, true, now);
            break;
        case RaceEventKind::LeadFinish:
            enqueue(Cue::LeaderFinish, &race, event.racer,
                    false, false, now);
            break;
        case RaceEventKind::LeadChanged:
            enqueue(Cue::LeaderChanged, &race, event.racer,
                    false, false, now);
            break;
        case RaceEventKind::LastFar:
            enqueue(Cue::LastFar, &race, event.racer,
                    false, true, now, 0.0F, false);
            break;
        case RaceEventKind::LowLife:
            enqueue(Cue::LowLife, &race, event.racer,
                    false, true, now, 40.0F);
            break;
        case RaceEventKind::Death:
            enqueue(Cue::Death, &race, event.racer,
                    false, true, now, 40.0F);
            break;
        case RaceEventKind::Kill:
            if (event.killCredit && event.racer == 0U)
            {
                enqueue(Cue::Kill, &race, event.racer,
                        false, true, now);
            }
            break;
        case RaceEventKind::RaceFinish:
            enqueue(Cue::RaceFinish, &race, event.racer,
                    false, false, now);
            break;
        default:
            break;
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
    Cue cue = Cue::FinishLast;
    if (place == 1U)
        cue = Cue::FinishFirst;
    else if (place == 2U)
        cue = Cue::FinishSecond;
    else if (place == 3U)
        cue = Cue::FinishThird;
    // cPlayerFinishLast is emitted by FinishMenu in the Windows game but
    // has no registered commentator comment in GameMode::Start.
    if (cue != Cue::FinishLast)
        enqueue(cue, &race, racer, false, true);
    playNext(error);
}

void OriginalRaceCommentator::pause(bool paused) noexcept
{
    paused_ = paused;
    if (voice_ != r3d::audio::invalidVoice)
        audio_.setVoicePaused(voice_, paused);
}

} // namespace rrr3d::audio
