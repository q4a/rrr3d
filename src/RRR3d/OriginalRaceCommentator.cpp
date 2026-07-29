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
};

const std::array<CueFiles, 11> cueFiles{{
    {Cue::Start, {"start1.ogg", "start2.ogg", "start3.ogg",
                  "start4.ogg"}},
    {Cue::LastLap, {"lastLap1.ogg", "lastLap2.ogg", "lastLap3.ogg",
                    "lastLap4.ogg", "lastLap5.ogg", "lastLap6.ogg",
                    "lastLap7.ogg"}},
    {Cue::WrongWay, {"playerMoveInverse1.ogg"}},
    {Cue::LowLife, {"lowLife1.ogg", "lowLife2.ogg", "lowLife3.ogg",
                    "lowLife4.ogg", "lowLife5.ogg", "lowLife6.ogg",
                    "lowLife7.ogg"}},
    {Cue::Kill, {"playerKill1.ogg", "playerKill2.ogg",
                 "playerKill3.ogg"}},
    {Cue::Death, {"death1.ogg", "lowLuck1.ogg"}},
    {Cue::FinishFirst, {"finishFirst1.ogg", "finishFirst2.ogg",
                        "finishFirst3.ogg", "finishFirst4.ogg",
                        "finishFirst5.ogg", "finishFirst6.ogg",
                        "finishFirst7.ogg"}},
    {Cue::FinishSecond, {"finishSecond1.ogg", "finishSecond2.ogg",
                         "finishSecond3.ogg", "finishSecond4.ogg",
                         "secondFinish1.ogg", "secondFinish2.ogg",
                         "secondFinish3.ogg"}},
    {Cue::FinishThird, {"finishThird1.ogg", "finishThird2.ogg",
                        "finishThird3.ogg", "finishThird4.ogg",
                        "finishThird5.ogg", "thirdFinish1.ogg",
                        "thirdFinish2.ogg", "thirdFinish3.ogg",
                        "thirdFinish4.ogg"}},
    {Cue::FinishLast, {"finishLast1.ogg", "finishLast2.ogg",
                       "finishLast3.ogg", "finishLast4.ogg",
                       "finishLast5.ogg", "finishLast6.ogg",
                       "lastFinish1.ogg", "lastFinish2.ogg",
                       "lastFinish3.ogg", "lastFinish4.ogg",
                       "lastFinish5.ogg"}},
    {Cue::RaceFinish, {"finish1.ogg"}},
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
                {
                    shutdown();
                    return false;
                }
                loaded.push_back(sound);
            }
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
        for (const auto sound : sounds)
            audio_.unloadSound(sound);
    }
    sounds_.clear();
    nextSound_.clear();
    initialized_ = false;
    paused_ = false;
    wrongWay_ = false;
}

void OriginalRaceCommentator::reset()
{
    if (!initialized_)
        return;
    if (voice_ != r3d::audio::invalidVoice)
        audio_.stop(voice_);
    voice_ = r3d::audio::invalidVoice;
    queue_.clear();
    wrongWay_ = false;
    enqueue(Cue::Start, true, false);
}

void OriginalRaceCommentator::enqueue(
    Cue cue, bool replace, bool skipWhenBusy)
{
    const auto found = sounds_.find(cue);
    if (found == sounds_.end() || found->second.empty())
        return;
    const bool busy =
        voice_ != r3d::audio::invalidVoice &&
        audio_.isVoiceActive(voice_);
    if (skipWhenBusy && (busy || !queue_.empty()))
        return;
    if (replace)
    {
        if (voice_ != r3d::audio::invalidVoice)
            audio_.stop(voice_);
        voice_ = r3d::audio::invalidVoice;
        queue_.clear();
    }
    auto& index = nextSound_[cue];
    queue_.push_back(
        found->second[index % found->second.size()]);
    ++index;
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
    const auto& human = session.racers().front();
    if (human.wrongWay && !wrongWay_)
        enqueue(Cue::WrongWay, false, true);
    wrongWay_ = human.wrongWay;
    for (const auto& event : session.events())
    {
        if (event.kind == RaceEventKind::LowLife &&
            event.racer == 0U)
        {
            enqueue(Cue::LowLife, false, true);
        }
        else if (event.kind == RaceEventKind::Lap &&
            event.racer == 0U &&
            human.completedLaps + 1U == race.lapCount)
        {
            enqueue(Cue::LastLap, false, false);
        }
        else if (event.kind == RaceEventKind::Kill)
        {
            if (event.racer == 0U)
                enqueue(Cue::Kill, false, true);
            if (event.target == 0U)
                enqueue(Cue::Death, false, true);
        }
        else if (event.kind == RaceEventKind::Finish &&
                 event.racer == 0U)
        {
            if (human.place == 1U)
                enqueue(Cue::FinishFirst, false, false);
            else if (human.place == 2U)
                enqueue(Cue::FinishSecond, false, false);
            else if (human.place == 3U)
                enqueue(Cue::FinishThird, false, false);
            else
                enqueue(Cue::FinishLast, false, false);
            enqueue(Cue::RaceFinish, false, false);
        }
    }
    playNext(error);
}

void OriginalRaceCommentator::pause(bool paused) noexcept
{
    paused_ = paused;
    if (voice_ != r3d::audio::invalidVoice)
        audio_.setVoicePaused(voice_, paused);
}

} // namespace rrr3d::audio
