#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace r3d::audio
{

using SoundHandle = std::uint32_t;
using VoiceHandle = std::uint32_t;

constexpr SoundHandle invalidSound = 0;
constexpr VoiceHandle invalidVoice = 0;

enum class Bus
{
	Music,
	Effects,
	Voice
};

// Matches the 0..2 gain range exposed by the original options UI. The final
// mixer still limits output samples to [-1, 1].
constexpr float maximumVolume = 2.0F;

enum class PlaybackDeviceEvent
{
	Added,
	Removed,
	FormatChanged
};

struct SoundInfo
{
	int sourceSampleRate = 0;
	int sourceChannels = 0;
	std::uint64_t mixerFrames = 0;
	double durationSeconds = 0.0;
	float peakAmplitude = 0.0F;
	float rmsAmplitude = 0.0F;
};

struct PlayOptions
{
	Bus bus = Bus::Effects;
	float volume = 1.0F;
	bool loop = false;
	bool paused = false;
	std::uint64_t startFrame = 0;
};

struct Statistics
{
	std::size_t loadedSounds = 0;
	std::size_t activeVoices = 0;
	std::uint64_t mixedFrames = 0;
	std::uint32_t playbackDeviceEvents = 0;
};

// Platform-independent boundary used by menu/gameplay code. Implementations
// own the device, decoder, mixer thread and all native API objects.
class AudioBackend
{
  public:
	virtual ~AudioBackend() = default;

	virtual bool initialize(std::string &error) = 0;
	virtual void shutdown() noexcept = 0;

	virtual SoundHandle loadOgg(const std::filesystem::path &path, SoundInfo &info, std::string &error) = 0;
	virtual bool unloadSound(SoundHandle sound) noexcept = 0;

	virtual VoiceHandle play(SoundHandle sound, const PlayOptions &options, std::string &error) = 0;
	virtual bool stop(VoiceHandle voice) noexcept = 0;
	virtual void stopAll() noexcept = 0;
	virtual bool setVoicePaused(VoiceHandle voice, bool paused) noexcept = 0;
	virtual bool isVoiceActive(VoiceHandle voice) const noexcept = 0;
	virtual std::uint64_t voicePositionFrames(VoiceHandle voice) const noexcept = 0;

	virtual void setPaused(bool paused) noexcept = 0;
	virtual bool paused() const noexcept = 0;
	virtual void setMasterVolume(float volume) noexcept = 0;
	virtual float masterVolume() const noexcept = 0;
	virtual void setBusVolume(Bus bus, float volume) noexcept = 0;
	virtual float busVolume(Bus bus) const noexcept = 0;

	virtual void notifyPlaybackDeviceEvent(PlaybackDeviceEvent event, std::uint32_t deviceId) noexcept = 0;
	virtual std::string driverName() const = 0;
	virtual std::string outputDeviceName() const = 0;
	virtual Statistics statistics() const noexcept = 0;
};

} // namespace r3d::audio
