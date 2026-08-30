#pragma once

#include "audio/AudioBackend.h"

#include <cstdint>
#include <string>
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

struct OriginalAudioPosition
{
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

// Native-backend counterpart of snd::Source3d.  Game code owns this source;
// SDL owns only the current voice proxy.  Play intent, proxy hysteresis,
// source/resource gain, pitch, loop mode and stop/restart lifetime remain
// independent of the platform backend just as they were above XAudio2.
class OriginalSource3d
{
public:
    OriginalSource3d() noexcept = default;
    ~OriginalSource3d();
    OriginalSource3d(const OriginalSource3d&) = delete;
    OriginalSource3d& operator=(const OriginalSource3d&) = delete;
    OriginalSource3d(OriginalSource3d&& other) noexcept;
    OriginalSource3d& operator=(OriginalSource3d&& other) noexcept;

    void Attach(r3d::audio::AudioBackend& backend) noexcept;
    void Detach() noexcept;
    void SetSound(
        r3d::audio::SoundHandle sound,
        float resourceVolume = 1.0F) noexcept;
    r3d::audio::SoundHandle GetSound() const noexcept;
    r3d::audio::VoiceHandle GetVoice() const noexcept;
    void SetLoop(bool value) noexcept;
    bool GetLoop() const noexcept;
    void SetVolume(float value) noexcept;
    float GetVolume() const noexcept;
    void SetFrequencyRatio(float value) noexcept;
    float GetFrequencyRatio() const noexcept;
    void SetPlaybackPositionFrames(std::uint64_t value) noexcept;
    std::uint64_t GetPlaybackPositionFrames() const noexcept;
    void SetPos3d(OriginalAudioPosition value) noexcept;
    OriginalAudioPosition GetPos3d() const noexcept;
    void SetDistanceScaler(float value) noexcept;
    float GetDistanceScaler() const noexcept;

    bool Play() noexcept;
    void Stop() noexcept;
    bool IsPlaying() const noexcept;
    bool IsProxyPlaying() const noexcept;
    bool Update(
        OriginalAudioPosition listener, bool paused,
        std::string& error);

private:
    void MoveFrom(OriginalSource3d&& other) noexcept;

    r3d::audio::AudioBackend* backend_ = nullptr;
    r3d::audio::SoundHandle sound_ = r3d::audio::invalidSound;
    r3d::audio::VoiceHandle voice_ = r3d::audio::invalidVoice;
    r3d::audio::Bus bus_ = r3d::audio::Bus::Effects;
    OriginalAudioPosition position_;
    float resourceVolume_ = 1.0F;
    float volume_ = 1.0F;
    float frequencyRatio_ = 1.0F;
    std::uint64_t playbackPositionFrames_ = 0U;
    float distanceScaler_ = 0.0F;
    bool loop_ = false;
    bool play_ = false;
    bool proxyPlaying_ = false;
};

OriginalSource3dMix originalSource3dFlatMix(
    float distance, bool proxyPlaying,
    float sourceDistanceOffset = 0.0F) noexcept;

// ResourceManager::LoadSound stores a per-resource volume in Sound and Proxy
// multiplies it with the Source volume. Paths not listed explicitly use 1.
float originalSoundVolume(std::string_view path) noexcept;

bool runOriginalSpatialAudioSmokeTest() noexcept;

} // namespace rrr3d::audio
