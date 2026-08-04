#pragma once

#include "OriginalRaceSession.h"
#include "audio/AudioBackend.h"

#include <cstddef>
#include <deque>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace r3d::resource
{
class ResourceFileSystem;
}

namespace rrr3d::audio
{

class OriginalRaceCommentator
{
public:
    OriginalRaceCommentator(
        r3d::audio::AudioBackend& audio,
        const r3d::resource::ResourceFileSystem& resources);
    ~OriginalRaceCommentator();

    OriginalRaceCommentator(const OriginalRaceCommentator&) = delete;
    OriginalRaceCommentator& operator=(
        const OriginalRaceCommentator&) = delete;

    bool initialize(std::string_view style, std::string& error);
    void shutdown() noexcept;
    void reset();
    void update(
        const r3d::game::originalrace::Race& race,
        const r3d::game::originalrace::OriginalRaceSession& session,
        std::string& error);
    void finishPlace(
        const r3d::game::originalrace::Race& race,
        std::size_t racer, std::uint32_t place,
        std::string& error);
    void pause(bool paused) noexcept;

    enum class Cue
    {
        Start,
        LastLap,
        Overboard,
        DeathMine,
        WrongWay,
        LostControl,
        LeaderFinish,
        LeaderChanged,
        LastFar,
        LowLife,
        Kill,
        Death,
        FinishFirst,
        FinishSecond,
        FinishThird,
        FinishLast,
        RaceFinish,
    };

private:
    struct CueVoice
    {
        r3d::audio::SoundHandle sound = r3d::audio::invalidSound;
        bool playerPrefix = false;
        bool humanOnly = false;
    };

    void enqueue(
        Cue cue,
        const r3d::game::originalrace::Race* race,
        std::size_t racer,
        bool replace, bool skipWhenBusy,
        float now = 0.0F,
        float delay = 0.0F,
        bool repeatPlayer = true);
    void playNext(std::string& error);

    r3d::audio::AudioBackend& audio_;
    const r3d::resource::ResourceFileSystem& resources_;
    std::map<Cue, std::vector<CueVoice>> sounds_;
    std::map<std::string, r3d::audio::SoundHandle> playerSounds_;
    std::map<Cue, std::size_t> nextSound_;
    std::map<Cue, float> nextCueSeconds_;
    std::map<Cue, std::size_t> lastCuePlayer_;
    std::deque<r3d::audio::SoundHandle> queue_;
    r3d::audio::VoiceHandle voice_ = r3d::audio::invalidVoice;
    bool initialized_ = false;
    bool paused_ = false;
};

} // namespace rrr3d::audio
