#include "SdlAudioSmoke.h"

#include "SdlAudioBackend.h"
#include "resource/ResourceFileSystem.h"

#include <SDL3/SDL_timer.h>

#include <cmath>
#include <functional>
#include <string_view>
#include <utility>

namespace rrr3d::audio
{
namespace
{

bool nearlyEqual(float left, float right) noexcept
{
	return std::abs(left - right) < 0.0001F;
}

bool waitUntil(const std::function<bool()> &predicate, std::uint64_t timeout_ms)
{
	const std::uint64_t deadline = SDL_GetTicks() + timeout_ms;
	while (SDL_GetTicks() < deadline)
	{
		if (predicate())
			return true;
		SDL_Delay(5);
	}
	return predicate();
}

} // namespace

bool runSdlAudioSmokeTest(SdlAudioBackend &audio, const r3d::resource::ResourceFileSystem &resources,
                          std::string &error)
{
	const auto baseline = audio.statistics();
	r3d::audio::SoundHandle music = r3d::audio::invalidSound;
	r3d::audio::SoundHandle click = r3d::audio::invalidSound;
	r3d::audio::SoundHandle gameplay_effect = r3d::audio::invalidSound;
	auto cleanup = [&]() {
		audio.setPaused(false);
		audio.stopAll();
		if (gameplay_effect != r3d::audio::invalidSound)
			audio.unloadSound(gameplay_effect);
		if (click != r3d::audio::invalidSound)
			audio.unloadSound(click);
		if (music != r3d::audio::invalidSound)
			audio.unloadSound(music);
		audio.setMasterVolume(1.0F);
		audio.setBusVolume(r3d::audio::Bus::Music, 1.0F);
		audio.setBusVolume(r3d::audio::Bus::Effects, 1.0F);
	};
	auto fail = [&](std::string message) {
		error = std::move(message);
		cleanup();
		return false;
	};
	auto load = [&](std::string_view virtual_path, r3d::audio::SoundInfo &info) {
		try
		{
			return audio.loadOgg(resources.resolve(virtual_path), info, error);
		}
		catch (const std::exception &exception)
		{
			error = exception.what();
			return r3d::audio::invalidSound;
		}
	};

	r3d::audio::SoundInfo music_info;
	music = load("Data/Music/Track1.ogg", music_info);
	if (music == r3d::audio::invalidSound)
		return fail("Music decode failed: " + error);
	if (music_info.sourceSampleRate != 44100 || music_info.sourceChannels != 2 || music_info.durationSeconds < 60.0 ||
	    music_info.mixerFrames == 0)
	{
		return fail("Decoded Track1.ogg metadata is invalid");
	}

	r3d::audio::SoundInfo click_info;
	click = load("Data/Sounds/UI/click.ogg", click_info);
	if (click == r3d::audio::invalidSound)
		return fail("UI effect decode failed: " + error);
	if (click_info.durationSeconds <= 0.05 || click_info.durationSeconds >= 1.0 || click_info.mixerFrames == 0)
		return fail("Decoded click.ogg metadata is invalid");

	r3d::audio::SoundInfo gameplay_info;
	gameplay_effect = load("Data/Sounds/fireGun.ogg", gameplay_info);
	if (gameplay_effect == r3d::audio::invalidSound)
		return fail("Gameplay effect decode failed: " + error);
	if (gameplay_info.durationSeconds < 1.0 || gameplay_info.mixerFrames == 0)
		return fail("Decoded fireGun.ogg metadata is invalid");

	audio.setMasterVolume(2.0F);
	audio.setBusVolume(r3d::audio::Bus::Music, 0.25F);
	audio.setBusVolume(r3d::audio::Bus::Effects, 0.75F);
	if (!nearlyEqual(audio.masterVolume(), 1.0F) || !nearlyEqual(audio.busVolume(r3d::audio::Bus::Music), 0.25F) ||
	    !nearlyEqual(audio.busVolume(r3d::audio::Bus::Effects), 0.75F))
	{
		return fail("Master/music/effects volume controls did not retain clamped values");
	}

	r3d::audio::PlayOptions music_options;
	music_options.bus = r3d::audio::Bus::Music;
	music_options.volume = 0.5F;
	music_options.loop = true;
	music_options.paused = true;
	const auto music_voice = audio.play(music, music_options, error);
	if (music_voice == r3d::audio::invalidVoice || !audio.isVoiceActive(music_voice))
		return fail("Unable to create a paused looping music voice: " + error);

	const auto click_voice = audio.play(click, {}, error);
	if (click_voice == r3d::audio::invalidVoice)
		return fail("Unable to create a one-shot UI effect voice: " + error);

	audio.setPaused(true);
	if (!audio.paused())
		return fail("Global audio pause state was not applied");
	audio.setPaused(false);
	if (audio.paused() || !audio.setVoicePaused(music_voice, false))
		return fail("Global/voice audio resume state was not applied");

	const auto frames_before = audio.statistics().mixedFrames;
	if (!waitUntil([&]() { return audio.statistics().mixedFrames > frames_before; }, 1500))
		return fail("SDL playback callback did not consume mixed frames");
	if (!waitUntil([&]() { return !audio.isVoiceActive(click_voice); }, 1500))
		return fail("One-shot UI effect did not finish and release its voice");
	if (!audio.isVoiceActive(music_voice))
		return fail("Looping music voice stopped unexpectedly");

	const auto gameplay_voice = audio.play(gameplay_effect, {r3d::audio::Bus::Effects, 0.6F, false, false}, error);
	if (gameplay_voice == r3d::audio::invalidVoice || !audio.unloadSound(gameplay_effect) ||
	    audio.isVoiceActive(gameplay_voice))
	{
		return fail("Unloading a gameplay effect did not release its active voice");
	}
	gameplay_effect = r3d::audio::invalidSound;

	const auto device_events_before = audio.statistics().playbackDeviceEvents;
	audio.notifyPlaybackDeviceEvent(r3d::audio::PlaybackDeviceEvent::Added, 1001);
	audio.notifyPlaybackDeviceEvent(r3d::audio::PlaybackDeviceEvent::FormatChanged, 1001);
	audio.notifyPlaybackDeviceEvent(r3d::audio::PlaybackDeviceEvent::Removed, 1001);
	if (audio.statistics().playbackDeviceEvents != device_events_before + 3)
		return fail("Playback-device change notifications were not retained");

	if (!audio.stop(music_voice) || audio.isVoiceActive(music_voice))
		return fail("Looping music voice did not stop");

	cleanup();
	const auto final_stats = audio.statistics();
	if (final_stats.loadedSounds != baseline.loadedSounds || final_stats.activeVoices != baseline.activeVoices)
	{
		error = "Audio smoke test leaked sounds or voices";
		return false;
	}

	error.clear();
	return true;
}

} // namespace rrr3d::audio
