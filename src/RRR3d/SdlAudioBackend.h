#pragma once

#include "audio/AudioBackend.h"

#include <SDL3/SDL_audio.h>

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace rrr3d::audio
{

class SdlAudioBackend final : public r3d::audio::AudioBackend
{
  public:
	static constexpr int mixerSampleRate = 48000;
	static constexpr int mixerChannels = 2;

	SdlAudioBackend() = default;
	~SdlAudioBackend() override;

	SdlAudioBackend(const SdlAudioBackend &) = delete;
	SdlAudioBackend &operator=(const SdlAudioBackend &) = delete;

	bool initialize(std::string &error) override;
	void shutdown() noexcept override;

	r3d::audio::SoundHandle loadOgg(const std::filesystem::path &path, r3d::audio::SoundInfo &info,
	                                std::string &error) override;
	bool unloadSound(r3d::audio::SoundHandle sound) noexcept override;

	r3d::audio::VoiceHandle play(r3d::audio::SoundHandle sound, const r3d::audio::PlayOptions &options,
	                             std::string &error) override;
	bool stop(r3d::audio::VoiceHandle voice) noexcept override;
	void stopAll() noexcept override;
	bool setVoicePaused(r3d::audio::VoiceHandle voice, bool paused) noexcept override;
	bool isVoiceActive(r3d::audio::VoiceHandle voice) const noexcept override;

	void setPaused(bool paused) noexcept override;
	bool paused() const noexcept override;
	void setMasterVolume(float volume) noexcept override;
	float masterVolume() const noexcept override;
	void setBusVolume(r3d::audio::Bus bus, float volume) noexcept override;
	float busVolume(r3d::audio::Bus bus) const noexcept override;

	void notifyPlaybackDeviceEvent(r3d::audio::PlaybackDeviceEvent event, std::uint32_t deviceId) noexcept override;
	std::string driverName() const override;
	std::string outputDeviceName() const override;
	r3d::audio::Statistics statistics() const noexcept override;

  private:
	struct Sound
	{
		std::vector<float> samples;
		r3d::audio::SoundInfo info;
	};

	struct Voice
	{
		r3d::audio::SoundHandle sound = r3d::audio::invalidSound;
		std::size_t sampleCursor = 0;
		r3d::audio::Bus bus = r3d::audio::Bus::Effects;
		float volume = 1.0F;
		bool loop = false;
		bool paused = false;
	};

	static void SDLCALL feedAudio(void *userdata, SDL_AudioStream *stream, int additionalAmount, int totalAmount);
	void mixAndQueue(SDL_AudioStream *stream, int additionalAmount) noexcept;

	static float clampVolume(float value) noexcept;
	r3d::audio::VoiceHandle allocateVoiceHandle() noexcept;
	r3d::audio::SoundHandle allocateSoundHandle() noexcept;

	mutable std::mutex mutex_;
	SDL_AudioStream *stream_ = nullptr;
	std::map<r3d::audio::SoundHandle, Sound> sounds_;
	std::map<r3d::audio::VoiceHandle, Voice> voices_;
	r3d::audio::SoundHandle nextSound_ = 1;
	r3d::audio::VoiceHandle nextVoice_ = 1;
	float masterVolume_ = 1.0F;
	float musicVolume_ = 1.0F;
	float effectsVolume_ = 1.0F;
	bool paused_ = false;
	std::atomic_bool running_{false};
	std::atomic_uint64_t mixedFrames_{0};
	std::atomic_uint32_t playbackDeviceEvents_{0};
};

} // namespace rrr3d::audio
