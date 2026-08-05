#include "OriginalMenuSounds.h"

#include "resource/ResourceFileSystem.h"

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
    const r3d::resource::ResourceFileSystem& resources)
    : audio_(audio), resources_(resources)
{
    sounds_.fill(r3d::audio::invalidSound);
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
            if (!resources_.exists(path))
            {
                error = "Missing original Menu SoundSheme file: " + path;
                shutdown();
                return false;
            }
            r3d::audio::SoundInfo info;
            const auto loaded = audio_.loadOgg(
                resources_.resolve(path), info, error);
            if (loaded == r3d::audio::invalidSound)
            {
                shutdown();
                return false;
            }
            sounds_[static_cast<std::size_t>(source.sound)] = loaded;
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
    if (voice_ != r3d::audio::invalidVoice)
        audio_.stop(voice_);
    voice_ = r3d::audio::invalidVoice;
    for (auto& sound : sounds_)
    {
        if (sound != r3d::audio::invalidSound)
            audio_.unloadSound(sound);
        sound = r3d::audio::invalidSound;
    }
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
    if (voice_ != r3d::audio::invalidVoice)
        audio_.stop(voice_);
    voice_ = r3d::audio::invalidVoice;
    r3d::audio::PlayOptions options;
    options.bus = r3d::audio::Bus::Effects;
    voice_ = audio_.play(sounds_[index], options, error);
    return voice_ != r3d::audio::invalidVoice;
}

std::size_t OriginalMenuSounds::loadedSoundCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        sounds_.begin(), sounds_.end(), [](auto sound) {
            return sound != r3d::audio::invalidSound;
        }));
}

} // namespace rrr3d::audio
