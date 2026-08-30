#include "OriginalSpatialAudio.h"

#include "OriginalAudioSpec.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>

namespace rrr3d::audio
{
using r3d::audio::AudioBackend;
using r3d::audio::Bus;
using r3d::audio::invalidSound;
using r3d::audio::invalidVoice;
using r3d::audio::PlaybackDeviceEvent;
using r3d::audio::PlayOptions;
using r3d::audio::SoundHandle;
using r3d::audio::SoundInfo;
using r3d::audio::Statistics;
using r3d::audio::VoiceHandle;

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

OriginalSource::~OriginalSource()
{
    Detach();
}

OriginalSource::OriginalSource(OriginalSource&& other) noexcept
{
    MoveFrom(std::move(other));
}

OriginalSource& OriginalSource::operator=(
    OriginalSource&& other) noexcept
{
    if (this != &other)
    {
        Detach();
        MoveFrom(std::move(other));
    }
    return *this;
}

void OriginalSource::MoveFrom(OriginalSource&& other) noexcept
{
    backend_ = std::exchange(other.backend_, nullptr);
    sound_ = std::exchange(other.sound_, invalidSound);
    voice_ = std::exchange(other.voice_, invalidVoice);
    bus_ = other.bus_;
    resourceVolume_ = other.resourceVolume_;
    volume_ = other.volume_;
    frequencyRatio_ = other.frequencyRatio_;
    playbackPositionFrames_ = other.playbackPositionFrames_;
    loop_ = other.loop_;
    paused_ = other.paused_;
}

void OriginalSource::Attach(AudioBackend& backend) noexcept
{
    if (backend_ == &backend)
        return;
    Detach();
    backend_ = &backend;
}

void OriginalSource::Detach() noexcept
{
    Stop();
    backend_ = nullptr;
}

void OriginalSource::SetBus(Bus value) noexcept
{
    if (bus_ == value)
        return;
    Stop();
    bus_ = value;
}

Bus OriginalSource::GetBus() const noexcept { return bus_; }

void OriginalSource::SetSound(
    SoundHandle sound, float resourceVolume) noexcept
{
    if (sound_ != sound)
    {
        Stop();
        sound_ = sound;
    }
    resourceVolume_ = resourceVolume;
    ApplyVoice();
}

SoundHandle OriginalSource::GetSound() const noexcept { return sound_; }
VoiceHandle OriginalSource::GetVoice() const noexcept { return voice_; }

void OriginalSource::SetLoop(bool value) noexcept
{
    if (loop_ == value)
        return;
    Stop();
    loop_ = value;
}

bool OriginalSource::GetLoop() const noexcept { return loop_; }

void OriginalSource::SetVolume(float value) noexcept
{
    volume_ = value;
    ApplyVoice();
}

float OriginalSource::GetVolume() const noexcept { return volume_; }

void OriginalSource::SetFrequencyRatio(float value) noexcept
{
    frequencyRatio_ = value;
    ApplyVoice();
}

float OriginalSource::GetFrequencyRatio() const noexcept
{
    return frequencyRatio_;
}

void OriginalSource::SetPlaybackPositionFrames(
    std::uint64_t value) noexcept
{
    playbackPositionFrames_ = value;
    if (backend_ != nullptr && voice_ != invalidVoice)
    {
        backend_->stop(voice_);
        voice_ = invalidVoice;
    }
}

std::uint64_t OriginalSource::GetPlaybackPositionFrames() const noexcept
{
    if (backend_ != nullptr && voice_ != invalidVoice &&
        backend_->isVoiceActive(voice_))
    {
        return backend_->voicePositionFrames(voice_);
    }
    return playbackPositionFrames_;
}

bool OriginalSource::Play(std::string& error, bool paused)
{
    if (backend_ == nullptr || sound_ == invalidSound)
    {
        error = "Original Source has no backend sound";
        return false;
    }
    paused_ = paused;
    if (voice_ != invalidVoice && backend_->isVoiceActive(voice_))
    {
        ApplyVoice();
        error.clear();
        return true;
    }
    voice_ = invalidVoice;
    PlayOptions options;
    options.bus = bus_;
    options.volume = resourceVolume_ * volume_;
    options.loop = loop_;
    options.paused = paused_;
    options.startFrame = playbackPositionFrames_;
    voice_ = backend_->play(sound_, options, error);
    if (voice_ == invalidVoice)
        return false;
    ApplyVoice();
    error.clear();
    return true;
}

void OriginalSource::Stop() noexcept
{
    if (backend_ != nullptr && voice_ != invalidVoice)
    {
        if (backend_->isVoiceActive(voice_))
        {
            playbackPositionFrames_ =
                backend_->voicePositionFrames(voice_);
        }
        backend_->stop(voice_);
    }
    voice_ = invalidVoice;
}

bool OriginalSource::IsPlaying() const noexcept
{
    return backend_ != nullptr && voice_ != invalidVoice &&
           backend_->isVoiceActive(voice_);
}

void OriginalSource::SetPaused(bool value) noexcept
{
    paused_ = value;
    if (backend_ != nullptr && voice_ != invalidVoice)
        backend_->setVoicePaused(voice_, value);
}

bool OriginalSource::GetPaused() const noexcept { return paused_; }

void OriginalSource::ApplyVoice() noexcept
{
    if (backend_ == nullptr || voice_ == invalidVoice ||
        !backend_->isVoiceActive(voice_))
    {
        return;
    }
    backend_->setVoiceParameters(
        voice_, resourceVolume_ * volume_, frequencyRatio_, 0.0F);
    backend_->setVoicePaused(voice_, paused_);
}

OriginalSource3d::~OriginalSource3d()
{
    Detach();
}

OriginalSource3d::OriginalSource3d(
    OriginalSource3d&& other) noexcept
{
    MoveFrom(std::move(other));
}

OriginalSource3d& OriginalSource3d::operator=(
    OriginalSource3d&& other) noexcept
{
    if (this != &other)
    {
        Detach();
        MoveFrom(std::move(other));
    }
    return *this;
}

void OriginalSource3d::MoveFrom(OriginalSource3d&& other) noexcept
{
    backend_ = std::exchange(other.backend_, nullptr);
    sound_ = std::exchange(other.sound_, invalidSound);
    voice_ = std::exchange(other.voice_, invalidVoice);
    bus_ = other.bus_;
    position_ = other.position_;
    resourceVolume_ = other.resourceVolume_;
    volume_ = other.volume_;
    frequencyRatio_ = other.frequencyRatio_;
    playbackPositionFrames_ = other.playbackPositionFrames_;
    distanceScaler_ = other.distanceScaler_;
    loop_ = other.loop_;
    play_ = std::exchange(other.play_, false);
    proxyPlaying_ = std::exchange(other.proxyPlaying_, false);
}

void OriginalSource3d::Attach(AudioBackend& backend) noexcept
{
    if (backend_ == &backend)
        return;
    Detach();
    backend_ = &backend;
}

void OriginalSource3d::Detach() noexcept
{
    Stop();
    backend_ = nullptr;
}

void OriginalSource3d::SetSound(
    SoundHandle sound, float resourceVolume) noexcept
{
    if (sound_ != sound)
    {
        Stop();
        sound_ = sound;
    }
    resourceVolume_ = resourceVolume;
}

SoundHandle OriginalSource3d::GetSound() const noexcept
{
    return sound_;
}

VoiceHandle OriginalSource3d::GetVoice() const noexcept
{
    return voice_;
}

void OriginalSource3d::SetLoop(bool value) noexcept
{
    if (loop_ == value)
        return;
    const bool restart = play_;
    Stop();
    loop_ = value;
    play_ = restart && sound_ != invalidSound && backend_ != nullptr;
}

bool OriginalSource3d::GetLoop() const noexcept { return loop_; }
void OriginalSource3d::SetVolume(float value) noexcept { volume_ = value; }
float OriginalSource3d::GetVolume() const noexcept { return volume_; }
void OriginalSource3d::SetFrequencyRatio(float value) noexcept
{
    frequencyRatio_ = value;
}
float OriginalSource3d::GetFrequencyRatio() const noexcept
{
    return frequencyRatio_;
}
void OriginalSource3d::SetPlaybackPositionFrames(
    std::uint64_t value) noexcept
{
    playbackPositionFrames_ = value;
    if (backend_ != nullptr && voice_ != invalidVoice)
    {
        backend_->stop(voice_);
        voice_ = invalidVoice;
    }
}
std::uint64_t OriginalSource3d::GetPlaybackPositionFrames() const noexcept
{
    if (backend_ != nullptr && voice_ != invalidVoice)
        return backend_->voicePositionFrames(voice_);
    return playbackPositionFrames_;
}
void OriginalSource3d::SetPos3d(OriginalAudioPosition value) noexcept
{
    position_ = value;
}
OriginalAudioPosition OriginalSource3d::GetPos3d() const noexcept
{
    return position_;
}
void OriginalSource3d::SetDistanceScaler(float value) noexcept
{
    distanceScaler_ = value;
}
float OriginalSource3d::GetDistanceScaler() const noexcept
{
    return distanceScaler_;
}

bool OriginalSource3d::Play() noexcept
{
    if (backend_ == nullptr || sound_ == invalidSound)
        return false;
    // Source3d::Play is intentionally idempotent while _play is true.
    play_ = true;
    return true;
}

void OriginalSource3d::Stop() noexcept
{
    play_ = false;
    proxyPlaying_ = false;
    if (backend_ != nullptr && voice_ != invalidVoice)
    {
        playbackPositionFrames_ =
            backend_->voicePositionFrames(voice_);
        backend_->stop(voice_);
    }
    voice_ = invalidVoice;
}

bool OriginalSource3d::IsPlaying() const noexcept { return play_; }
bool OriginalSource3d::IsProxyPlaying() const noexcept
{
    return proxyPlaying_;
}

bool OriginalSource3d::Update(
    OriginalAudioPosition listener, bool paused,
    std::string& error)
{
    if (backend_ == nullptr || sound_ == invalidSound)
    {
        error = "Original Source3d has no backend sound";
        return false;
    }
    if (voice_ != invalidVoice && !backend_->isVoiceActive(voice_))
    {
        voice_ = invalidVoice;
        proxyPlaying_ = false;
        // Source3d::MyReport clears _play only for pmOnce.
        if (!loop_)
            play_ = false;
    }
    if (!play_)
    {
        error.clear();
        return true;
    }

    const float dx = position_.x - listener.x;
    const float dy = position_.y - listener.y;
    const float dz = position_.z - listener.z;
    const auto spatial = originalSource3dFlatMix(
        std::sqrt(dx * dx + dy * dy + dz * dz),
        proxyPlaying_, distanceScaler_);
    proxyPlaying_ = spatial.proxyPlaying;
    if (spatial.stopped && voice_ != invalidVoice)
    {
        playbackPositionFrames_ =
            backend_->voicePositionFrames(voice_);
        backend_->stop(voice_);
        voice_ = invalidVoice;
    }
    const float gain =
        spatial.gain * volume_ * resourceVolume_;

    if (voice_ == invalidVoice && proxyPlaying_)
    {
        PlayOptions options;
        options.bus = bus_;
        options.volume = gain;
        options.loop = loop_;
        options.paused = paused;
        options.startFrame = playbackPositionFrames_;
        voice_ = backend_->play(sound_, options, error);
        if (voice_ == invalidVoice)
        {
            proxyPlaying_ = false;
            return false;
        }
    }
    if (voice_ != invalidVoice)
    {
        backend_->setVoiceParameters(
            voice_, gain, frequencyRatio_, 0.0F);
        backend_->setVoicePaused(
            voice_, paused || !proxyPlaying_);
    }
    error.clear();
    return true;
}

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
    const float sourceMissileMix =
        r3d::game::originalaudio::masteringVoiceVolume *
        r3d::game::originalaudio::defaultEffectsVolume *
        originalSoundVolume("Data/Sounds/missile_launch.ogg") *
        initial.gain;
    class FakeAudio final : public AudioBackend
    {
    public:
        bool initialize(std::string&) override { return true; }
        void shutdown() noexcept override {}
        SoundHandle loadOgg(
            const std::filesystem::path&, SoundInfo&,
            std::string&) override { return 1U; }
        bool unloadSound(SoundHandle) noexcept override { return true; }
        VoiceHandle play(
            SoundHandle, const PlayOptions& options,
            std::string&) override
        {
            const auto voice = next_++;
            active_[voice] = true;
            positions_[voice] = options.startFrame;
            bus = options.bus;
            volume = options.volume;
            voicePaused = options.paused;
            startFrame = options.startFrame;
            return voice;
        }
        bool stop(VoiceHandle voice) noexcept override
        {
            active_[voice] = false;
            ++stops;
            return true;
        }
        void stopAll() noexcept override { active_.clear(); }
        bool setVoicePaused(
            VoiceHandle, bool value) noexcept override
        {
            voicePaused = value;
            return true;
        }
        bool setVoiceParameters(
            VoiceHandle, float value, float pitchValue,
            float) noexcept override
        {
            volume = value;
            pitch = pitchValue;
            return true;
        }
        bool isVoiceActive(VoiceHandle voice) const noexcept override
        {
            const auto found = active_.find(voice);
            return found != active_.end() && found->second;
        }
        std::uint64_t voicePositionFrames(
            VoiceHandle voice) const noexcept override
        {
            const auto found = positions_.find(voice);
            return found != positions_.end() ? found->second : 0U;
        }
        void setPaused(bool) noexcept override {}
        bool paused() const noexcept override { return false; }
        void setMasterVolume(float) noexcept override {}
        float masterVolume() const noexcept override { return 1.0F; }
        void setBusVolume(Bus, float) noexcept override {}
        float busVolume(Bus) const noexcept override { return 1.0F; }
        void notifyPlaybackDeviceEvent(
            PlaybackDeviceEvent, std::uint32_t) noexcept override {}
        std::string driverName() const override { return "fake"; }
        std::string outputDeviceName() const override { return "fake"; }
        Statistics statistics() const noexcept override { return {}; }

        void finish(VoiceHandle voice) { active_[voice] = false; }
        void setPosition(VoiceHandle voice, std::uint64_t value)
        {
            positions_[voice] = value;
        }

        float volume = 0.0F;
        float pitch = 0.0F;
        bool voicePaused = false;
        std::size_t stops = 0U;
        std::uint64_t startFrame = 0U;
        Bus bus = Bus::Effects;

    private:
        VoiceHandle next_ = 1U;
        std::unordered_map<VoiceHandle, bool> active_;
        std::unordered_map<VoiceHandle, std::uint64_t> positions_;
    };

    FakeAudio ordinaryFake;
    OriginalSource ordinary;
    ordinary.Attach(ordinaryFake);
    ordinary.SetBus(Bus::Voice);
    ordinary.SetSound(9U, 2.0F);
    ordinary.SetVolume(0.25F);
    ordinary.SetFrequencyRatio(1.5F);
    ordinary.SetPlaybackPositionFrames(33U);
    std::string ordinaryError;
    const bool ordinaryStarted = ordinary.Play(ordinaryError);
    const auto ordinaryVoice = ordinary.GetVoice();
    const bool ordinaryMix =
        nearlyEqual(ordinaryFake.volume, 0.5F) &&
        nearlyEqual(ordinaryFake.pitch, 1.5F) &&
        ordinaryFake.bus == Bus::Voice && ordinaryFake.startFrame == 33U;
    ordinaryFake.setPosition(ordinaryVoice, 77U);
    ordinary.Stop();
    const bool ordinaryResumed =
        ordinary.GetPlaybackPositionFrames() == 77U &&
        ordinary.Play(ordinaryError) && ordinaryFake.startFrame == 77U;
    ordinary.SetPlaybackPositionFrames(0U);
    const bool ordinaryRewound =
        ordinary.Play(ordinaryError, true) &&
        ordinaryFake.startFrame == 0U && ordinaryFake.voicePaused;
    ordinary.Stop();

    FakeAudio fake;
    OriginalSource3d source;
    source.Attach(fake);
    source.SetSound(7U, 2.0F);
    source.SetLoop(true);
    source.SetVolume(0.5F);
    source.SetFrequencyRatio(1.25F);
    source.SetPos3d({});
    std::string error;
    const bool sourceStarted =
        source.Play() && source.Update({}, false, error);
    const auto sourceVoice = source.GetVoice();
    const float sourceStartVolume = fake.volume;
    const float sourceStartPitch = fake.pitch;
    fake.setPosition(sourceVoice, 123U);
    source.SetPos3d({45.001F, 0.0F, 0.0F});
    const bool sourceStoppedAtLag =
        source.Update({}, false, error) &&
        source.IsPlaying() && !source.IsProxyPlaying() &&
        source.GetVoice() == invalidVoice && fake.stops == 1U;
    source.SetPos3d({35.0F, 0.0F, 0.0F});
    const bool sourceStayedStopped =
        source.Update({}, false, error) &&
        !source.IsProxyPlaying() &&
        source.GetVoice() == invalidVoice && fake.stops == 1U;
    source.SetPos3d({29.0F, 0.0F, 0.0F});
    const bool sourceResumed =
        source.Update({}, false, error) &&
        source.IsProxyPlaying() && !fake.voicePaused &&
        source.GetVoice() != invalidVoice && fake.startFrame == 123U;
    const auto resumedVoice = source.GetVoice();
    OriginalSource3d moved(std::move(source));
    const bool moveKeptProxy =
        moved.GetVoice() == resumedVoice &&
        source.GetVoice() == invalidVoice;
    fake.setPosition(resumedVoice, 222U);
    moved.Stop();
    const bool explicitStopKeptPosition =
        moved.GetPlaybackPositionFrames() == 222U &&
        moved.Play() && moved.Update({}, false, error) &&
        fake.startFrame == 222U;
    moved.SetPlaybackPositionFrames(0U);
    const bool explicitSeekRestarted =
        moved.Update({}, false, error) && fake.startFrame == 0U;
    moved.Stop();

    OriginalSource3d once;
    once.Attach(fake);
    once.SetSound(8U);
    once.SetLoop(false);
    once.Play();
    const bool onceStarted = once.Update({}, false, error);
    const auto onceVoice = once.GetVoice();
    fake.finish(onceVoice);
    const bool onceReportedEnd =
        once.Update({}, false, error) && !once.IsPlaying() &&
        once.GetVoice() == invalidVoice;

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
                       1.0F) &&
           // XAudio2 output is MasteringVoice * Logic category * Source *
           // Sound resource * the flat output matrix.  This catches the
           // former port regression where the master was ten times louder.
           nearlyEqual(sourceMissileMix, 0.16F) &&
           nearlyEqual(
               r3d::game::originalaudio::masteringVoiceVolume *
                   r3d::game::originalaudio::defaultMusicVolume,
               0.12F) &&
           nearlyEqual(
               r3d::game::originalaudio::masteringVoiceVolume *
                   r3d::game::originalaudio::defaultVoiceVolume,
               0.12F) &&
           ordinaryStarted && ordinaryVoice != invalidVoice &&
           ordinaryMix && ordinaryResumed && ordinaryRewound &&
           ordinaryFake.stops == 3U &&
           sourceStarted && sourceVoice != invalidVoice &&
           nearlyEqual(sourceStartVolume, 1.0F) &&
           nearlyEqual(sourceStartPitch, 1.25F) &&
           sourceStoppedAtLag && sourceStayedStopped &&
           sourceResumed && moveKeptProxy &&
           explicitStopKeptPosition && explicitSeekRestarted &&
           fake.stops == 4U &&
           onceStarted && onceVoice != invalidVoice && onceReportedEnd;
}

} // namespace rrr3d::audio
