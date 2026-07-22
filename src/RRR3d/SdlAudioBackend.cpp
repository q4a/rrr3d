#include "SdlAudioBackend.h"

#include <SDL3/SDL.h>
#include <vorbis/vorbisfile.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace rrr3d::audio
{
namespace
{

constexpr std::size_t maximumDecodedBytes = 512U * 1024U * 1024U;
constexpr int decodeBlockFrames = 4096;
constexpr int mixBlockFrames = 2048;

class VorbisFile
{
  public:
	~VorbisFile()
	{
		if (open_)
			ov_clear(&file_);
	}

	bool open(const std::filesystem::path &path) noexcept
	{
		const std::string native_path = path.string();
		open_ = ov_fopen(native_path.c_str(), &file_) == 0;
		return open_;
	}

	OggVorbis_File *get() noexcept
	{
		return &file_;
	}

  private:
	OggVorbis_File file_{};
	bool open_ = false;
};

class AudioStream
{
  public:
	explicit AudioStream(SDL_AudioStream *stream) : stream_(stream)
	{
	}
	~AudioStream()
	{
		if (stream_ != nullptr)
			SDL_DestroyAudioStream(stream_);
	}

	SDL_AudioStream *get() const noexcept
	{
		return stream_;
	}

  private:
	SDL_AudioStream *stream_ = nullptr;
};

const char *deviceEventName(r3d::audio::PlaybackDeviceEvent event) noexcept
{
	switch (event)
	{
	case r3d::audio::PlaybackDeviceEvent::Added:
		return "added";
	case r3d::audio::PlaybackDeviceEvent::Removed:
		return "removed";
	case r3d::audio::PlaybackDeviceEvent::FormatChanged:
		return "format-changed";
	}
	return "unknown";
}

} // namespace

SdlAudioBackend::~SdlAudioBackend()
{
	shutdown();
}

bool SdlAudioBackend::initialize(std::string &error)
{
	if (running_.load(std::memory_order_acquire))
	{
		error.clear();
		return true;
	}

	const SDL_AudioSpec mixer_spec{SDL_AUDIO_F32, mixerChannels, mixerSampleRate};
	SDL_AudioStream *stream =
		SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &mixer_spec, &SdlAudioBackend::feedAudio, this);
	if (stream == nullptr)
	{
		error = "Unable to open the SDL default playback device: ";
		error += SDL_GetError();
		return false;
	}

	{
		std::lock_guard<std::mutex> lock(mutex_);
		stream_ = stream;
		paused_ = false;
	}
	running_.store(true, std::memory_order_release);

	if (!SDL_ResumeAudioStreamDevice(stream))
	{
		error = "Unable to start SDL audio playback: ";
		error += SDL_GetError();
		running_.store(false, std::memory_order_release);
		SDL_DestroyAudioStream(stream);
		std::lock_guard<std::mutex> lock(mutex_);
		stream_ = nullptr;
		return false;
	}

	error.clear();
	return true;
}

void SdlAudioBackend::shutdown() noexcept
{
	if (!running_.exchange(false, std::memory_order_acq_rel))
		return;

	SDL_AudioStream *stream = nullptr;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		stream = stream_;
	}
	if (stream != nullptr)
	{
		SDL_PauseAudioStreamDevice(stream);
		SDL_DestroyAudioStream(stream);
	}

	std::lock_guard<std::mutex> lock(mutex_);
	stream_ = nullptr;
	voices_.clear();
	sounds_.clear();
	paused_ = false;
	nextSound_ = 1;
	nextVoice_ = 1;
}

r3d::audio::SoundHandle SdlAudioBackend::loadOgg(const std::filesystem::path &path, r3d::audio::SoundInfo &info,
                                                 std::string &error)
{
	info = {};
	if (!running_.load(std::memory_order_acquire))
	{
		error = "Audio backend is not initialized";
		return r3d::audio::invalidSound;
	}

	VorbisFile vorbis;
	if (!vorbis.open(path))
	{
		error = "Unable to open Ogg/Vorbis resource: ";
		error += path.string();
		return r3d::audio::invalidSound;
	}

	const vorbis_info *source_info = ov_info(vorbis.get(), -1);
	const ogg_int64_t source_frames = ov_pcm_total(vorbis.get(), -1);
	if (source_info == nullptr || ov_streams(vorbis.get()) != 1 || source_info->rate <= 0 ||
	    source_info->channels <= 0 || source_info->channels > 8 || source_frames <= 0)
	{
		error = "Invalid Ogg/Vorbis stream metadata: ";
		error += path.string();
		return r3d::audio::invalidSound;
	}

	const long double converted_frames =
		std::ceil(static_cast<long double>(source_frames) * mixerSampleRate / source_info->rate);
	const long double converted_bytes = converted_frames * mixerChannels * sizeof(float);
	if (converted_bytes <= 0.0L || converted_bytes > static_cast<long double>(maximumDecodedBytes))
	{
		error = "Decoded Ogg/Vorbis resource exceeds the 512 MiB safety limit: ";
		error += path.string();
		return r3d::audio::invalidSound;
	}

	const SDL_AudioSpec source_spec{SDL_AUDIO_F32, source_info->channels, static_cast<int>(source_info->rate)};
	const SDL_AudioSpec mixer_spec{SDL_AUDIO_F32, mixerChannels, mixerSampleRate};
	AudioStream converter(SDL_CreateAudioStream(&source_spec, &mixer_spec));
	if (converter.get() == nullptr)
	{
		error = "Unable to create SDL audio converter: ";
		error += SDL_GetError();
		return r3d::audio::invalidSound;
	}

	std::vector<float> interleaved;
	interleaved.reserve(static_cast<std::size_t>(decodeBlockFrames * source_info->channels));
	int bitstream = 0;
	while (true)
	{
		float **channels = nullptr;
		const long decoded_frames = ov_read_float(vorbis.get(), &channels, decodeBlockFrames, &bitstream);
		if (decoded_frames == 0)
			break;
		if (decoded_frames < 0)
		{
			error = "Corrupt Ogg/Vorbis packet in resource: ";
			error += path.string();
			return r3d::audio::invalidSound;
		}

		const std::size_t sample_count = static_cast<std::size_t>(decoded_frames) * source_info->channels;
		interleaved.resize(sample_count);
		for (long frame = 0; frame < decoded_frames; ++frame)
		{
			for (int channel = 0; channel < source_info->channels; ++channel)
			{
				interleaved[static_cast<std::size_t>(frame) * source_info->channels + channel] =
					channels[channel][frame];
			}
		}

		const std::size_t byte_count = sample_count * sizeof(float);
		if (byte_count > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
		    !SDL_PutAudioStreamData(converter.get(), interleaved.data(), static_cast<int>(byte_count)))
		{
			error = "Unable to convert Ogg/Vorbis PCM data: ";
			error += SDL_GetError();
			return r3d::audio::invalidSound;
		}
	}

	if (!SDL_FlushAudioStream(converter.get()))
	{
		error = "Unable to flush converted Ogg/Vorbis data: ";
		error += SDL_GetError();
		return r3d::audio::invalidSound;
	}

	const int available_bytes = SDL_GetAudioStreamAvailable(converter.get());
	if (available_bytes <= 0 || static_cast<std::size_t>(available_bytes) > maximumDecodedBytes ||
	    available_bytes % static_cast<int>(sizeof(float) * mixerChannels) != 0)
	{
		error = "SDL produced invalid converted PCM data for: ";
		error += path.string();
		return r3d::audio::invalidSound;
	}

	Sound sound;
	sound.samples.resize(static_cast<std::size_t>(available_bytes) / sizeof(float));
	const int received_bytes = SDL_GetAudioStreamData(converter.get(), sound.samples.data(), available_bytes);
	if (received_bytes != available_bytes)
	{
		error = "Unable to retrieve all converted PCM data: ";
		error += SDL_GetError();
		return r3d::audio::invalidSound;
	}

	sound.info.sourceSampleRate = static_cast<int>(source_info->rate);
	sound.info.sourceChannels = source_info->channels;
	sound.info.mixerFrames = sound.samples.size() / mixerChannels;
	sound.info.durationSeconds = static_cast<double>(source_frames) / static_cast<double>(source_info->rate);
	info = sound.info;

	std::lock_guard<std::mutex> lock(mutex_);
	const auto handle = allocateSoundHandle();
	if (handle == r3d::audio::invalidSound)
	{
		error = "No free portable audio sound handles";
		return r3d::audio::invalidSound;
	}
	sounds_.emplace(handle, std::move(sound));
	error.clear();
	return handle;
}

bool SdlAudioBackend::unloadSound(r3d::audio::SoundHandle sound) noexcept
{
	std::lock_guard<std::mutex> lock(mutex_);
	const auto sound_entry = sounds_.find(sound);
	if (sound_entry == sounds_.end())
		return false;

	for (auto voice = voices_.begin(); voice != voices_.end();)
	{
		if (voice->second.sound == sound)
			voice = voices_.erase(voice);
		else
			++voice;
	}
	sounds_.erase(sound_entry);
	return true;
}

r3d::audio::VoiceHandle SdlAudioBackend::play(r3d::audio::SoundHandle sound, const r3d::audio::PlayOptions &options,
                                              std::string &error)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (sounds_.find(sound) == sounds_.end())
	{
		error = "Cannot play an invalid or unloaded sound handle";
		return r3d::audio::invalidVoice;
	}

	const auto handle = allocateVoiceHandle();
	if (handle == r3d::audio::invalidVoice)
	{
		error = "No free portable audio voice handles";
		return r3d::audio::invalidVoice;
	}
	voices_.emplace(handle, Voice{sound, 0, options.bus, clampVolume(options.volume), options.loop, options.paused});
	error.clear();
	return handle;
}

bool SdlAudioBackend::stop(r3d::audio::VoiceHandle voice) noexcept
{
	std::lock_guard<std::mutex> lock(mutex_);
	return voices_.erase(voice) != 0;
}

void SdlAudioBackend::stopAll() noexcept
{
	std::lock_guard<std::mutex> lock(mutex_);
	voices_.clear();
}

bool SdlAudioBackend::setVoicePaused(r3d::audio::VoiceHandle voice, bool paused) noexcept
{
	std::lock_guard<std::mutex> lock(mutex_);
	const auto entry = voices_.find(voice);
	if (entry == voices_.end())
		return false;
	entry->second.paused = paused;
	return true;
}

bool SdlAudioBackend::isVoiceActive(r3d::audio::VoiceHandle voice) const noexcept
{
	std::lock_guard<std::mutex> lock(mutex_);
	return voices_.find(voice) != voices_.end();
}

void SdlAudioBackend::setPaused(bool paused) noexcept
{
	SDL_AudioStream *stream = nullptr;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		paused_ = paused;
		stream = stream_;
	}
	if (stream == nullptr)
		return;

	if (paused)
		SDL_PauseAudioStreamDevice(stream);
	else
		SDL_ResumeAudioStreamDevice(stream);
}

bool SdlAudioBackend::paused() const noexcept
{
	std::lock_guard<std::mutex> lock(mutex_);
	return paused_;
}

void SdlAudioBackend::setMasterVolume(float volume) noexcept
{
	std::lock_guard<std::mutex> lock(mutex_);
	masterVolume_ = clampVolume(volume);
}

float SdlAudioBackend::masterVolume() const noexcept
{
	std::lock_guard<std::mutex> lock(mutex_);
	return masterVolume_;
}

void SdlAudioBackend::setBusVolume(r3d::audio::Bus bus, float volume) noexcept
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (bus == r3d::audio::Bus::Music)
		musicVolume_ = clampVolume(volume);
	else
		effectsVolume_ = clampVolume(volume);
}

float SdlAudioBackend::busVolume(r3d::audio::Bus bus) const noexcept
{
	std::lock_guard<std::mutex> lock(mutex_);
	return bus == r3d::audio::Bus::Music ? musicVolume_ : effectsVolume_;
}

void SdlAudioBackend::notifyPlaybackDeviceEvent(r3d::audio::PlaybackDeviceEvent event, std::uint32_t device_id) noexcept
{
	playbackDeviceEvents_.fetch_add(1, std::memory_order_relaxed);
	SDL_Log("Playback device %s: id=%u; SDL default stream migration remains active", deviceEventName(event),
	        device_id);
}

std::string SdlAudioBackend::driverName() const
{
	const char *name = SDL_GetCurrentAudioDriver();
	return name == nullptr ? "Unavailable" : name;
}

std::string SdlAudioBackend::outputDeviceName() const
{
	const char *name = SDL_GetAudioDeviceName(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK);
	return name == nullptr ? "System default" : name;
}

r3d::audio::Statistics SdlAudioBackend::statistics() const noexcept
{
	std::lock_guard<std::mutex> lock(mutex_);
	return {sounds_.size(), voices_.size(), mixedFrames_.load(std::memory_order_relaxed),
	        playbackDeviceEvents_.load(std::memory_order_relaxed)};
}

void SDLCALL SdlAudioBackend::feedAudio(void *userdata, SDL_AudioStream *stream, int additional_amount,
                                        int total_amount)
{
	static_cast<void>(total_amount);
	static_cast<SdlAudioBackend *>(userdata)->mixAndQueue(stream, additional_amount);
}

void SdlAudioBackend::mixAndQueue(SDL_AudioStream *stream, int additional_amount) noexcept
{
	if (additional_amount <= 0)
		return;

	constexpr int bytes_per_frame = static_cast<int>(sizeof(float) * mixerChannels);
	int frames_remaining = (additional_amount + bytes_per_frame - 1) / bytes_per_frame;
	std::array<float, static_cast<std::size_t>(mixBlockFrames * mixerChannels)> mix{};

	while (frames_remaining > 0)
	{
		const int frame_count = std::min(frames_remaining, mixBlockFrames);
		const std::size_t sample_count = static_cast<std::size_t>(frame_count * mixerChannels);
		std::fill_n(mix.begin(), sample_count, 0.0F);

		if (running_.load(std::memory_order_acquire))
		{
			std::lock_guard<std::mutex> lock(mutex_);
			for (auto voice_entry = voices_.begin(); voice_entry != voices_.end();)
			{
				Voice &voice = voice_entry->second;
				const auto sound_entry = sounds_.find(voice.sound);
				if (sound_entry == sounds_.end())
				{
					voice_entry = voices_.erase(voice_entry);
					continue;
				}
				if (voice.paused)
				{
					++voice_entry;
					continue;
				}

				const Sound &sound = sound_entry->second;
				const float bus_volume = voice.bus == r3d::audio::Bus::Music ? musicVolume_ : effectsVolume_;
				const float gain = masterVolume_ * bus_volume * voice.volume;
				bool finished = false;
				for (int frame = 0; frame < frame_count; ++frame)
				{
					if (voice.sampleCursor + 1 >= sound.samples.size())
					{
						if (voice.loop)
							voice.sampleCursor = 0;
						else
						{
							finished = true;
							break;
						}
					}

					const std::size_t output = static_cast<std::size_t>(frame * mixerChannels);
					mix[output] += sound.samples[voice.sampleCursor] * gain;
					mix[output + 1] += sound.samples[voice.sampleCursor + 1] * gain;
					voice.sampleCursor += mixerChannels;
				}

				if (finished)
					voice_entry = voices_.erase(voice_entry);
				else
					++voice_entry;
			}
		}

		for (std::size_t index = 0; index < sample_count; ++index)
			mix[index] = std::clamp(mix[index], -1.0F, 1.0F);

		const int byte_count = frame_count * bytes_per_frame;
		if (!SDL_PutAudioStreamData(stream, mix.data(), byte_count))
		{
			SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Unable to queue mixed audio: %s", SDL_GetError());
			return;
		}
		mixedFrames_.fetch_add(static_cast<std::uint64_t>(frame_count), std::memory_order_relaxed);
		frames_remaining -= frame_count;
	}
}

float SdlAudioBackend::clampVolume(float value) noexcept
{
	return std::clamp(value, 0.0F, 1.0F);
}

r3d::audio::VoiceHandle SdlAudioBackend::allocateVoiceHandle() noexcept
{
	for (std::uint64_t attempt = 0; attempt < std::numeric_limits<r3d::audio::VoiceHandle>::max(); ++attempt)
	{
		const auto candidate = nextVoice_++;
		if (nextVoice_ == r3d::audio::invalidVoice)
			nextVoice_ = 1;
		if (candidate != r3d::audio::invalidVoice && voices_.find(candidate) == voices_.end())
			return candidate;
	}
	return r3d::audio::invalidVoice;
}

r3d::audio::SoundHandle SdlAudioBackend::allocateSoundHandle() noexcept
{
	for (std::uint64_t attempt = 0; attempt < std::numeric_limits<r3d::audio::SoundHandle>::max(); ++attempt)
	{
		const auto candidate = nextSound_++;
		if (nextSound_ == r3d::audio::invalidSound)
			nextSound_ = 1;
		if (candidate != r3d::audio::invalidSound && sounds_.find(candidate) == sounds_.end())
			return candidate;
	}
	return r3d::audio::invalidSound;
}

} // namespace rrr3d::audio
