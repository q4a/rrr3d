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
        float seconds,
        std::string& error);
    void finishPlace(
        const r3d::game::originalrace::Race& race,
        std::size_t racer, std::uint32_t place,
        std::string& error);
    void pause(bool paused) noexcept;
    [[nodiscard]] std::size_t commentCount() const noexcept;
    [[nodiscard]] std::size_t loadedVoiceCount() const noexcept;

private:
    enum class BusyAction
    {
        Skip,
        Queue,
        Replace,
    };

    struct CommentVoice
    {
        r3d::audio::SoundHandle sound = r3d::audio::invalidSound;
        float weight = 0.0F;
        bool startPlayer = false;
        bool endPlayer = false;
        bool humanOnly = false;
    };

    struct Comment
    {
        std::vector<CommentVoice> voices;
        float chance = 0.0F;
        float delay = 0.0F;
        float nextSeconds = 0.0F;
        std::size_t lastPlayer = static_cast<std::size_t>(-1);
        BusyAction busy = BusyAction::Skip;
        bool repeatPlayer = true;
    };

    void enqueue(
        std::string_view commentName,
        const r3d::game::originalrace::Race* race,
        std::size_t racer,
        std::string& error);
    const CommentVoice* generate(
        Comment& comment, std::size_t racer);
    [[nodiscard]] bool isSpeaking() const noexcept;
    void playNext(std::string& error);

    r3d::audio::AudioBackend& audio_;
    const r3d::resource::ResourceFileSystem& resources_;
    std::map<std::string, Comment> comments_;
    std::map<std::string, r3d::audio::SoundHandle> loadedSounds_;
    std::deque<r3d::audio::SoundHandle> queue_;
    r3d::audio::VoiceHandle voice_ = r3d::audio::invalidVoice;
    float globalDelaySeconds_ = 0.0F;
    float timeSeconds_ = 0.0F;
    float silenceSeconds_ = 0.0F;
    bool initialized_ = false;
    bool paused_ = false;
};

} // namespace rrr3d::audio
