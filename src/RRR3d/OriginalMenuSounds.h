#pragma once

#include "audio/AudioBackend.h"

#include <array>
#include <cstddef>
#include <string>

namespace r3d::resource
{
class ResourceFileSystem;
}

namespace rrr3d::audio
{

enum class OriginalMenuSound : std::size_t
{
    ButtonClick,
    Rollover,
    PickupDown,
    PickupUp,
    Repaint,
    ShowPlanet,
    ChangeOption,
    Acceptance,
    Warning,
    Count,
};

// Menu owns one Effects source in the Windows game. Every UI sound stops the
// previous sound on that source before starting the requested SoundSheme cue.
class OriginalMenuSounds
{
public:
    OriginalMenuSounds(
        r3d::audio::AudioBackend& audio,
        const r3d::resource::ResourceFileSystem& resources);
    ~OriginalMenuSounds();

    OriginalMenuSounds(const OriginalMenuSounds&) = delete;
    OriginalMenuSounds& operator=(const OriginalMenuSounds&) = delete;

    bool initialize(std::string& error);
    void shutdown() noexcept;
    bool play(OriginalMenuSound sound, std::string& error);

    [[nodiscard]] std::size_t loadedSoundCount() const noexcept;

private:
    r3d::audio::AudioBackend& audio_;
    const r3d::resource::ResourceFileSystem& resources_;
    std::array<r3d::audio::SoundHandle,
               static_cast<std::size_t>(OriginalMenuSound::Count)>
        sounds_{};
    r3d::audio::VoiceHandle voice_ = r3d::audio::invalidVoice;
    bool initialized_ = false;
};

} // namespace rrr3d::audio
