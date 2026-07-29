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
    void pause(bool paused) noexcept;

    enum class Cue
    {
        Start,
        LastLap,
        WrongWay,
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
    void enqueue(Cue cue, bool replace, bool skipWhenBusy);
    void playNext(std::string& error);

    r3d::audio::AudioBackend& audio_;
    const r3d::resource::ResourceFileSystem& resources_;
    std::map<Cue, std::vector<r3d::audio::SoundHandle>> sounds_;
    std::map<Cue, std::size_t> nextSound_;
    std::deque<r3d::audio::SoundHandle> queue_;
    r3d::audio::VoiceHandle voice_ = r3d::audio::invalidVoice;
    bool initialized_ = false;
    bool paused_ = false;
    bool wrongWay_ = false;
};

} // namespace rrr3d::audio
