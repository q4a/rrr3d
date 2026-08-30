#include "OriginalMenuSounds.h"

#include <algorithm>
#include <array>
#include <exception>
#include <string_view>

namespace rrr3d::audio
{
namespace
{

using Sound = OriginalMenuSound;

struct SourceSound
{
    Sound sound;
    std::string_view path;
};

// Menu::Menu, Menu::ShowAccept, Menu::ShowMessage and
// WorkshopFrame::StartDrag/ResetDrag.
constexpr std::array<SourceSound, 9> sourceSounds{{
    {Sound::ButtonClick, "Sounds/UI/click.ogg"},
    {Sound::Rollover, "Sounds/UI/navedenie.ogg"},
    {Sound::PickupDown, "Sounds/UI/pickup_down.ogg"},
    {Sound::PickupUp, "Sounds/UI/pickup_up.ogg"},
    {Sound::Repaint, "Sounds/UI/repaint.ogg"},
    {Sound::ShowPlanet, "Sounds/UI/showPlanet.ogg"},
    {Sound::ChangeOption, "Sounds/UI/changeOption.ogg"},
    {Sound::Acceptance, "Sounds/UI/acception.ogg"},
    {Sound::Warning, "Sounds/UI/warning.ogg"},
}};

} // namespace

OriginalMenuSounds::OriginalMenuSounds(
    r3d::audio::AudioBackend& audio,
    rrr3d::race::OriginalResourceManager& resources)
    : resources_(resources)
{
    sounds_.fill(r3d::audio::invalidSound);
    resourceVolumes_.fill(1.0F);
    source_.Attach(audio);
    source_.SetBus(r3d::audio::Bus::Effects);
}

OriginalMenuSounds::~OriginalMenuSounds()
{
    shutdown();
}

bool OriginalMenuSounds::initialize(std::string& error)
{
    shutdown();
    try
    {
        for (const auto& source : sourceSounds)
        {
            const std::string path = "Data/" + std::string(source.path);
            const auto& resource = resources_.GetSound(path);
            const auto index = static_cast<std::size_t>(source.sound);
            sounds_[index] = resource.sound;
            resourceVolumes_[index] = resource.volume;
        }
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        shutdown();
        return false;
    }
    initialized_ = true;
    error.clear();
    return true;
}

void OriginalMenuSounds::shutdown() noexcept
{
    source_.Stop();
    sounds_.fill(r3d::audio::invalidSound);
    resourceVolumes_.fill(1.0F);
    initialized_ = false;
}

bool OriginalMenuSounds::play(
    OriginalMenuSound sound, std::string& error)
{
    const auto index = static_cast<std::size_t>(sound);
    if (!initialized_ || index >= sounds_.size() ||
        sounds_[index] == r3d::audio::invalidSound)
    {
        error = "Original Menu SoundSheme is not initialized";
        return false;
    }
    // Menu::PlaySound calls StopSound before assigning and rewinding the one
    // shared source, so fast focus/click sequences never overlap.
    source_.Stop();
    source_.SetSound(sounds_[index], resourceVolumes_[index]);
    source_.SetLoop(false);
    source_.SetPlaybackPositionFrames(0U);
    return source_.Play(error);
}

std::size_t OriginalMenuSounds::loadedSoundCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        sounds_.begin(), sounds_.end(), [](auto sound) {
            return sound != r3d::audio::invalidSound;
        }));
}

} // namespace rrr3d::audio
