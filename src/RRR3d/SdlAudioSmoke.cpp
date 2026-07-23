#include "SdlAudioSmoke.h"

#include "OriginalAudioSpec.h"
#include "SdlAudioBackend.h"
#include "resource/ResourceFileSystem.h"

#include <SDL3/SDL_timer.h>

#include <cmath>
#include <filesystem>
#include <fstream>
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

std::string dataResourcePath(std::string_view legacyPath)
{
	std::string path = "Data\\";
	path.append(legacyPath);
	return path;
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

bool auditOggDirectory(const std::filesystem::path &directory,
                       std::size_t expectedCount, std::string &error)
{
	std::size_t count = 0;
	std::error_code iterator_error;
	for (std::filesystem::recursive_directory_iterator iterator(directory, iterator_error), end;
	     !iterator_error && iterator != end; iterator.increment(iterator_error))
	{
		if (!iterator->is_regular_file() || iterator->path().extension() != ".ogg")
			continue;
		const auto stem = iterator->path().stem().string();
		if (stem.size() >= 2 &&
		    stem.compare(stem.size() - 2, 2, " 2") == 0)
			continue;
		++count;
		std::ifstream stream(iterator->path(), std::ios::binary);
		char capture_pattern[4]{};
		if (!stream.read(capture_pattern, sizeof(capture_pattern)) ||
		    std::string_view(capture_pattern, sizeof(capture_pattern)) != "OggS")
		{
			error = "Invalid Ogg capture pattern: ";
			error += iterator->path().string();
			return false;
		}
	}
	if (iterator_error)
	{
		error = "Unable to audit original Ogg directory: ";
		error += iterator_error.message();
		return false;
	}
	if (count != expectedCount)
	{
		error = "Original Ogg count mismatch in ";
		error += directory.string();
		error += ": expected ";
		error += std::to_string(expectedCount);
		error += ", found ";
		error += std::to_string(count);
		return false;
	}
	return true;
}

} // namespace

bool runSdlAudioSmokeTest(SdlAudioBackend &audio, const r3d::resource::ResourceFileSystem &resources,
                          std::string &error)
{
	if (!auditOggDirectory(resources.root() / "Data/Music", 16, error) ||
	    !auditOggDirectory(resources.root() / "Data/Sounds", 46, error) ||
	    !auditOggDirectory(resources.root() / "Data/Voice", 120, error))
	{
		return false;
	}

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
		audio.setBusVolume(r3d::audio::Bus::Voice, 1.0F);
	};
	auto fail = [&](std::string message) {
		error = std::move(message);
		cleanup();
		return false;
	};
	auto load = [&](std::string_view legacy_path, r3d::audio::SoundInfo &info) {
		try
		{
			return audio.loadOgg(resources.resolve(dataResourcePath(legacy_path)), info, error);
		}
		catch (const std::exception &exception)
		{
			error = exception.what();
			return r3d::audio::invalidSound;
		}
	};

	r3d::audio::SoundInfo music_info;
	music = load(r3d::game::originalaudio::menuTracks[0].path, music_info);
	if (music == r3d::audio::invalidSound)
		return fail("Music decode failed: " + error);
	if (music_info.sourceSampleRate != 44100 || music_info.sourceChannels != 2 || music_info.durationSeconds < 60.0 ||
	    music_info.mixerFrames == 0 || music_info.peakAmplitude <= 0.0F || music_info.rmsAmplitude <= 0.0F)
	{
		return fail("Decoded Track1.ogg metadata is invalid");
	}

	r3d::audio::SoundInfo click_info;
	click = load(r3d::game::originalaudio::mainButtonClick, click_info);
	if (click == r3d::audio::invalidSound)
		return fail("UI effect decode failed: " + error);
	if (click_info.durationSeconds <= 0.05 || click_info.durationSeconds >= 1.0 || click_info.mixerFrames == 0 ||
	    click_info.peakAmplitude <= 0.0F || click_info.rmsAmplitude <= 0.0F)
		return fail("Decoded click.ogg metadata is invalid");

	r3d::audio::SoundInfo gameplay_info;
	gameplay_effect = load(r3d::game::originalaudio::fireGun, gameplay_info);
	if (gameplay_effect == r3d::audio::invalidSound)
		return fail("Gameplay effect decode failed: " + error);
	if (gameplay_info.durationSeconds < 1.0 || gameplay_info.mixerFrames == 0 ||
	    gameplay_info.peakAmplitude <= 0.0F || gameplay_info.rmsAmplitude <= 0.0F)
		return fail("Decoded fireGun.ogg metadata is invalid");

	audio.setMasterVolume(3.0F);
	audio.setBusVolume(r3d::audio::Bus::Music, r3d::game::originalaudio::defaultMusicVolume);
	audio.setBusVolume(r3d::audio::Bus::Effects, r3d::game::originalaudio::defaultEffectsVolume);
	audio.setBusVolume(r3d::audio::Bus::Voice, -1.0F);
	if (!nearlyEqual(audio.busVolume(r3d::audio::Bus::Voice), 0.0F))
		return fail("Negative voice volume did not clamp to zero");
	audio.setBusVolume(r3d::audio::Bus::Voice,
	                   r3d::game::originalaudio::defaultVoiceVolume);
	if (!nearlyEqual(audio.masterVolume(), r3d::audio::maximumVolume) ||
	    !nearlyEqual(audio.busVolume(r3d::audio::Bus::Music),
	                 r3d::game::originalaudio::defaultMusicVolume) ||
	    !nearlyEqual(audio.busVolume(r3d::audio::Bus::Effects),
	                 r3d::game::originalaudio::defaultEffectsVolume) ||
	    !nearlyEqual(audio.busVolume(r3d::audio::Bus::Voice),
	                 r3d::game::originalaudio::defaultVoiceVolume))
	{
		return fail("Master/music/effects/voice volume controls did not retain clamped values");
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

	const auto click_loop = audio.play(
		click, {r3d::audio::Bus::Effects, 1.0F, true, false}, error);
	if (click_loop == r3d::audio::invalidVoice)
		return fail("Unable to create a looping UI effect voice: " + error);
	const auto loop_boundary_ms = static_cast<std::uint64_t>(
		std::ceil(click_info.durationSeconds * 1000.0)) + 150;
	SDL_Delay(static_cast<std::uint32_t>(loop_boundary_ms));
	if (!audio.isVoiceActive(click_loop) || !audio.stop(click_loop))
		return fail("Looping voice did not survive and stop after a sample boundary");

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
