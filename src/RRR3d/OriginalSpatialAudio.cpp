#include "OriginalSpatialAudio.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <string>

namespace rrr3d::audio
{
namespace
{

std::string canonicalSoundPath(std::string_view path)
{
    std::string result;
    result.reserve(path.size());
    for (const char character : path)
    {
        const char separator = character == '\\' ? '/' : character;
        result.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(separator))));
    }
    return result;
}

bool nearlyEqual(float left, float right) noexcept
{
    return std::abs(left - right) < 0.0001F;
}

} // namespace

OriginalSource3dMix originalSource3dFlatMix(
    float distance, bool proxyPlaying,
    float sourceDistanceOffset) noexcept
{
    if (!std::isfinite(distance))
        distance = std::numeric_limits<float>::max();
    distance = std::max(distance, 0.0F);
    const float scaler = std::max(
        originalSource3dDistance + sourceDistanceOffset, 0.001F);
    const float stopDistance = scaler + scaler * 0.5F;

    OriginalSource3dMix result;
    result.gain = std::clamp(1.0F - distance / scaler, 0.0F, 1.0F);
    result.proxyPlaying = proxyPlaying;
    if (distance > stopDistance)
    {
        result.stopped = proxyPlaying;
        result.proxyPlaying = false;
    }
    else if (distance < scaler && !proxyPlaying)
    {
        result.started = true;
        result.proxyPlaying = true;
    }
    return result;
}

float originalSoundVolume(std::string_view path) noexcept
{
    static constexpr std::array<std::string_view, 7> doubledSounds{
        "data/sounds/carcrash05.ogg",
        "data/sounds/missile_launch.ogg",
        "data/sounds/shredder.ogg",
        "data/sounds/klicka5.ogg",
        "data/sounds/exhaust_b_heavy.ogg",
        "data/sounds/firegun.ogg",
        "data/sounds/shieldon.ogg",
    };
    const std::string canonical = canonicalSoundPath(path);
    return std::find(
               doubledSounds.begin(), doubledSounds.end(), canonical) !=
                   doubledSounds.end()
               ? 2.0F
               : 1.0F;
}

bool runOriginalSpatialAudioSmokeTest() noexcept
{
    const auto initial = originalSource3dFlatMix(0.0F, false);
    const auto audible = originalSource3dFlatMix(15.0F, true);
    const auto silentLag = originalSource3dFlatMix(30.0F, true);
    const auto stopped = originalSource3dFlatMix(45.001F, true);
    const auto staysStopped = originalSource3dFlatMix(35.0F, false);
    const auto resumed = originalSource3dFlatMix(29.999F, false);
    return initial.started && initial.proxyPlaying &&
           nearlyEqual(initial.gain, 1.0F) &&
           audible.proxyPlaying && nearlyEqual(audible.gain, 0.5F) &&
           silentLag.proxyPlaying && nearlyEqual(silentLag.gain, 0.0F) &&
           stopped.stopped && !stopped.proxyPlaying &&
           !staysStopped.proxyPlaying && !staysStopped.started &&
           resumed.started && resumed.proxyPlaying &&
           nearlyEqual(originalSoundVolume(
                           "Data\\Sounds\\missile_launch.ogg"),
                       2.0F) &&
           nearlyEqual(originalSoundVolume(
                           "Data/Sounds/cluster_rocket.ogg"),
                       1.0F);
}

} // namespace rrr3d::audio
