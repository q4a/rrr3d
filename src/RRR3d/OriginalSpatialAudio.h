#pragma once

#include <string_view>

namespace rrr3d::audio
{

// snd::Engine starts in m3dFlat with this distance scaler. Source3d adds
// half of the scaler as a stop lag so silent looping proxies do not churn at
// the audible boundary.
inline constexpr float originalSource3dDistance = 30.0F;

struct OriginalSource3dMix
{
    float gain = 0.0F;
    bool proxyPlaying = false;
    bool stopped = false;
    bool started = false;
};

OriginalSource3dMix originalSource3dFlatMix(
    float distance, bool proxyPlaying,
    float sourceDistanceOffset = 0.0F) noexcept;

// ResourceManager::LoadSound stores a per-resource volume in Sound and Proxy
// multiplies it with the Source volume. Paths not listed explicitly use 1.
float originalSoundVolume(std::string_view path) noexcept;

bool runOriginalSpatialAudioSmokeTest() noexcept;

} // namespace rrr3d::audio
