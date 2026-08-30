#pragma once

#include "OriginalSpatialAudio.h"
#include "OriginalResourceManager.h"

#include <array>
#include <cstddef>
#include <string>

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
        rrr3d::race::OriginalResourceManager& resources);
    ~OriginalMenuSounds();

    OriginalMenuSounds(const OriginalMenuSounds&) = delete;
    OriginalMenuSounds& operator=(const OriginalMenuSounds&) = delete;

    bool initialize(std::string& error);
    void shutdown() noexcept;
    bool play(OriginalMenuSound sound, std::string& error);

    [[nodiscard]] std::size_t loadedSoundCount() const noexcept;

private:
    rrr3d::race::OriginalResourceManager& resources_;
    std::array<r3d::audio::SoundHandle,
               static_cast<std::size_t>(OriginalMenuSound::Count)>
        sounds_{};
    std::array<float,
               static_cast<std::size_t>(OriginalMenuSound::Count)>
        resourceVolumes_{};
    OriginalSource source_;
    bool initialized_ = false;
};

} // namespace rrr3d::audio
