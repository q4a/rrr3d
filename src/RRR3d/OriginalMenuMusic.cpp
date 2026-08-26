#include "OriginalMenuMusic.h"

#include "OriginalAudioSpec.h"
#include "resource/ResourceFileSystem.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <future>
#include <iterator>
#include <optional>
#include <system_error>
#include <utility>
#include <vector>

namespace rrr3d::audio
{
namespace
{

std::string dataPath(const std::string &legacyPath)
{
	return "Data\\" + legacyPath;
}

enum class LoadState
{
	Unloaded,
	Loading,
	Loaded,
	Failed
};

struct LoadedTrack
{
	LoadState state = LoadState::Unloaded;
	r3d::audio::SoundHandle sound = r3d::audio::invalidSound;
	r3d::audio::SoundInfo info;
};

struct DecodeResult
{
	std::size_t index = 0;
	r3d::audio::SoundHandle sound = r3d::audio::invalidSound;
	r3d::audio::SoundInfo info;
	std::string error;
};

} // namespace

struct OriginalMenuMusic::Impl
{
	Impl(r3d::audio::AudioBackend &backend, const r3d::resource::ResourceFileSystem &fileSystem,
	     std::filesystem::path persistedState, std::uint64_t seed, bool shouldPersist)
		: Impl(backend, fileSystem, std::move(persistedState), seed,
		       shouldPersist,
		       r3d::game::originalaudio::
		           loadOriginalMusicCatalog(fileSystem).menu,
		       {})
	{
	}

	Impl(r3d::audio::AudioBackend &backend,
	     const r3d::resource::ResourceFileSystem &fileSystem,
	     std::filesystem::path persistedState, std::uint64_t seed,
	     bool shouldPersist,
	     std::vector<r3d::game::MusicCatTrack> tracks,
	     std::vector<std::size_t> initialPlaylist)
		: audio(backend), resources(fileSystem), statePath(std::move(persistedState)),
		  music(std::move(tracks), seed), loaded(music.tracks().size()), persistState(shouldPersist)
	{
		music.setPlaylist(std::move(initialPlaylist));
	}

	bool initialize(std::string &error, bool startPlayback)
	{
		if (initialized)
		{
			error.clear();
			return true;
		}

		if (persistState)
		{
			std::ifstream input(statePath, std::ios::binary);
			if (input)
			{
				const std::string state((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
				std::string restoreError;
				if (!music.restore(state, restoreError))
					stateWarning = std::move(restoreError);
			}
		}
		playbackRequested = startPlayback;
		if (playbackRequested && !music.currentTrack() && !music.play())
		{
			error = "Original menu MusicCat contains no tracks";
			return false;
		}

		initialized = true;
		lastSave = std::chrono::steady_clock::now();
		return scheduleDecode(error);
	}

	bool update(std::string &error)
	{
		if (!initialized)
		{
			error = "Original menu music is not initialized";
			return false;
		}
		if (!reapDecode(error))
			return false;

		if (voice != r3d::audio::invalidVoice && audio.isVoiceActive(voice))
		{
			music.setPlaybackPosition(audio.voicePositionFrames(voice), currentTotalFrames());
		}
		else if (voice != r3d::audio::invalidVoice)
		{
			voice = r3d::audio::invalidVoice;
			trackStarted = false;
			if (playbackRequested && !music.paused())
			{
				if (!music.next())
				{
					error = "Original menu MusicCat could not select its automatic next track";
					return false;
				}
				++transitions;
				if (!writeState(error))
					return false;
			}
		}

		trimDecodedCache();
		if (!startCurrent(error) || !scheduleDecode(error))
			return false;

		const auto now = std::chrono::steady_clock::now();
		if (persistState && now - lastSave >= std::chrono::seconds(5))
		{
			capturePosition();
			if (!writeState(error))
				return false;
			lastSave = now;
		}
		error.clear();
		return true;
	}

	void shutdown() noexcept
	{
		if (!initialized)
			return;
		capturePosition();
		std::string ignored;
		writeState(ignored);
		if (voice != r3d::audio::invalidVoice)
			audio.stop(voice);
		voice = r3d::audio::invalidVoice;
		trackStarted = false;

		if (decode)
		{
			try
			{
				const DecodeResult result = decode->get();
				if (result.sound != r3d::audio::invalidSound)
				{
					loaded[result.index].sound = result.sound;
					loaded[result.index].info = result.info;
					loaded[result.index].state = LoadState::Loaded;
				}
			}
			catch (...)
			{
			}
			decode.reset();
		}
		for (auto &entry : loaded)
		{
			if (entry.sound != r3d::audio::invalidSound)
				audio.unloadSound(entry.sound);
			entry = {};
		}
		initialized = false;
		playbackRequested = false;
	}

	bool play(std::string &error)
	{
		if (!initialized)
		{
			error = "Original menu music is not initialized";
			return false;
		}
		if (voice != r3d::audio::invalidVoice)
			audio.stop(voice);
		voice = r3d::audio::invalidVoice;
		trackStarted = false;
		music.setPaused(false);
		music.setPlaybackPosition(0, 0);
		if (!music.play())
		{
			error = "Original menu MusicCat contains no playable tracks";
			return false;
		}
		playbackRequested = true;
		trimDecodedCache();
		if (!writeState(error))
			return false;
		return startCurrent(error) && scheduleDecode(error);
	}

	void stop() noexcept
	{
		if (voice != r3d::audio::invalidVoice)
			audio.stop(voice);
		voice = r3d::audio::invalidVoice;
		trackStarted = false;
		playbackRequested = false;
		music.setPaused(false);
		music.setPlaybackPosition(0, currentTotalFrames());
		trimDecodedCache();
	}

	bool pause(bool shouldPause, std::string &error)
	{
		if (!initialized)
		{
			error = "Original menu music is not initialized";
			return false;
		}
		capturePosition();
		music.setPaused(shouldPause);
		if (shouldPause)
		{
			// MusicCat::Pause(true) stores Source::GetPos and calls StopMusic;
			// it does not retain a paused XAudio Proxy. Recreate the backend
			// voice from the saved PCM frame on Pause(false).
			if (voice != r3d::audio::invalidVoice)
				audio.stop(voice);
			voice = r3d::audio::invalidVoice;
			trackStarted = false;
		}
		else if (playbackRequested &&
		    (!startCurrent(error) || !scheduleDecode(error)))
			return false;
		return writeState(error);
	}

	bool next(std::string &error)
	{
		if (!initialized)
		{
			error = "Original menu music is not initialized";
			return false;
		}
		if (voice != r3d::audio::invalidVoice)
			audio.stop(voice);
		voice = r3d::audio::invalidVoice;
		trackStarted = false;
		if (!music.next())
		{
			error = "Original menu MusicCat could not select its next track";
			return false;
		}
		++transitions;
		playbackRequested = true;
		trimDecodedCache();
		if (!writeState(error))
			return false;
		return startCurrent(error) && scheduleDecode(error);
	}

	bool seekCurrent(std::uint64_t mixerFrame, std::string &error)
	{
		const auto current = music.currentTrack();
		if (!current || loaded[*current].state != LoadState::Loaded)
		{
			error = "The current original menu track is not decoded yet";
			return false;
		}
		const auto total = loaded[*current].info.mixerFrames;
		if (total == 0 || mixerFrame >= total)
		{
			error = "Original menu music seek position is outside the track";
			return false;
		}
		if (voice != r3d::audio::invalidVoice)
			audio.stop(voice);
		voice = r3d::audio::invalidVoice;
		trackStarted = false;
		music.setPlaybackPosition(mixerFrame, total);
		return startCurrent(error);
	}

	bool saveState(std::string &error)
	{
		capturePosition();
		return writeState(error);
	}

	bool verifySavedState(std::string &error)
	{
		if (!persistState)
		{
			error = "MusicCat persistence is disabled";
			return false;
		}
		if (!saveState(error))
			return false;
		std::ifstream input(statePath, std::ios::binary);
		if (!input)
		{
			error = "Unable to reopen the saved MusicCat state";
			return false;
		}
		const std::string state((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
		r3d::game::MusicCat restored(music.tracks(), 1);
		if (!restored.restore(state, error))
			return false;
		if (restored.serialize() != music.serialize())
		{
			error = "Saved MusicCat state does not match the active playlist";
			return false;
		}
		error.clear();
		return true;
	}

	bool reapDecode(std::string &error)
	{
		if (!decode || decode->wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			return true;
		DecodeResult result;
		try
		{
			result = decode->get();
		}
		catch (const std::exception &exception)
		{
			error = "Background menu-music decode failed: ";
			error += exception.what();
			decode.reset();
			return false;
		}
		decode.reset();
		if (result.sound == r3d::audio::invalidSound)
		{
			loaded[result.index].state = LoadState::Failed;
			error = "Background menu-music decode failed: " + result.error;
			return false;
		}
		loaded[result.index].sound = result.sound;
		loaded[result.index].info = result.info;
		loaded[result.index].state = LoadState::Loaded;
		return true;
	}

	bool scheduleDecode(std::string &error)
	{
		if (decode)
			return true;
		std::optional<std::size_t> candidate;
		if (const auto current = music.currentTrack(); current && loaded[*current].state == LoadState::Unloaded)
			candidate = current;
		// The SDL backend stores decoded stereo-float PCM.  Normal gameplay
		// therefore keeps only the current track and, once playback has
		// started, the next playlist entry warm.  The old eager scan retained
		// all 14 menu/race tracks (hundreds of MiB) and caused memory pressure
		// and severe slowdown after the background worker caught up.  The
		// persistence smoke fixture deliberately keeps the eager policy so it
		// can continue auditing every source container.
		if (!candidate && trackStarted)
		{
			for (auto track = music.playlist().rbegin(); track != music.playlist().rend(); ++track)
			{
				if (*track < loaded.size() &&
				    loaded[*track].state == LoadState::Unloaded)
				{
					candidate = *track;
					break;
				}
			}
		}
		// GameMode::MusicCat does not pop the game playlist before
		// DoStartRace.  Warm exactly that upcoming entry without changing the
		// source queue or retaining the full game soundtrack in memory.
		if (!candidate && !playbackRequested && !music.currentTrack() &&
		    !music.playlist().empty())
		{
			for (auto upcoming = music.playlist().rbegin();
			     upcoming != music.playlist().rend(); ++upcoming)
			{
				if (*upcoming < loaded.size() &&
				    loaded[*upcoming].state == LoadState::Unloaded)
				{
					candidate = *upcoming;
					break;
				}
			}
		}
		if (!candidate && persistState)
		{
			for (std::size_t index = 0; index < loaded.size(); ++index)
			{
				if (loaded[index].state == LoadState::Unloaded)
				{
					candidate = index;
					break;
				}
			}
		}
		if (!candidate)
		{
			error.clear();
			return true;
		}

		std::filesystem::path path;
		try
		{
			path = resources.resolve(dataPath(music.tracks()[*candidate].path));
		}
		catch (const std::exception &exception)
		{
			error = exception.what();
			return false;
		}
		loaded[*candidate].state = LoadState::Loading;
		auto *backend = &audio;
		const std::size_t index = *candidate;
		decode.emplace(std::async(std::launch::async, [backend, path = std::move(path), index]() {
			DecodeResult result;
			result.index = index;
			result.sound = backend->loadOgg(path, result.info, result.error);
			return result;
		}));
		error.clear();
		return true;
	}

	void trimDecodedCache() noexcept
	{
		if (persistState)
			return;
		const auto current = music.currentTrack();
		std::optional<std::size_t> next;
		if (trackStarted && !music.playlist().empty())
			next = music.playlist().back();
		for (std::size_t index = 0; index < loaded.size(); ++index)
		{
			if ((current && index == *current) ||
			    (next && index == *next) ||
			    loaded[index].state == LoadState::Loading)
			{
				continue;
			}
			if (loaded[index].sound != r3d::audio::invalidSound)
				audio.unloadSound(loaded[index].sound);
			loaded[index] = {};
		}
	}

	bool startCurrent(std::string &error)
	{
		if (!playbackRequested || trackStarted || music.paused())
			return true;
		const auto current = music.currentTrack();
		if (!current || loaded[*current].state != LoadState::Loaded)
			return true;
		const auto &entry = loaded[*current];
		if (music.positionFrames() >= entry.info.mixerFrames)
		{
			if (!music.next())
			{
				error = "Original menu MusicCat could not advance past a completed saved track";
				return false;
			}
			++transitions;
			trimDecodedCache();
			return true;
		}

		r3d::audio::PlayOptions options;
		options.bus = r3d::audio::Bus::Music;
		options.paused = music.paused();
		options.startFrame = music.positionFrames();
		voice = audio.play(entry.sound, options, error);
		if (voice == r3d::audio::invalidVoice)
			return false;
		trackStarted = true;
		music.setPlaybackPosition(options.startFrame, entry.info.mixerFrames);
		return true;
	}

	void capturePosition() noexcept
	{
		if (voice != r3d::audio::invalidVoice && audio.isVoiceActive(voice))
			music.setPlaybackPosition(audio.voicePositionFrames(voice), currentTotalFrames());
	}

	std::uint64_t currentTotalFrames() const noexcept
	{
		const auto current = music.currentTrack();
		if (!current || loaded[*current].state != LoadState::Loaded)
			return music.totalFrames();
		return loaded[*current].info.mixerFrames;
	}

	bool writeState(std::string &error)
	{
		if (!persistState)
		{
			error.clear();
			return true;
		}
		std::error_code directoryError;
		std::filesystem::create_directories(statePath.parent_path(), directoryError);
		if (directoryError)
		{
			error = "Unable to create the MusicCat state directory: " + directoryError.message();
			return false;
		}
		auto temporary = statePath;
		temporary += ".tmp";
		{
			std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
			if (!output || !(output << music.serialize()) || !output.flush())
			{
				error = "Unable to write the temporary MusicCat state";
				return false;
			}
		}
		std::error_code renameError;
		std::filesystem::rename(temporary, statePath, renameError);
		if (renameError)
		{
			error = "Unable to publish the MusicCat state: " + renameError.message();
			return false;
		}
		lastSave = std::chrono::steady_clock::now();
		error.clear();
		return true;
	}

	r3d::audio::AudioBackend &audio;
	const r3d::resource::ResourceFileSystem &resources;
	std::filesystem::path statePath;
	r3d::game::MusicCat music;
	std::vector<LoadedTrack> loaded;
	std::optional<std::future<DecodeResult>> decode;
	r3d::audio::VoiceHandle voice = r3d::audio::invalidVoice;
	std::chrono::steady_clock::time_point lastSave{};
	std::string stateWarning;
	std::uint64_t transitions = 0;
	bool persistState = true;
	bool initialized = false;
	bool trackStarted = false;
	bool playbackRequested = false;
};

OriginalMenuMusic::OriginalMenuMusic(r3d::audio::AudioBackend &audio,
                                     const r3d::resource::ResourceFileSystem &resources,
                                     std::filesystem::path statePath, std::uint64_t randomSeed, bool persistState)
	: impl_(std::make_unique<Impl>(audio, resources, std::move(statePath), randomSeed, persistState))
{
}

OriginalMenuMusic::OriginalMenuMusic(
	r3d::audio::AudioBackend &audio,
	const r3d::resource::ResourceFileSystem &resources,
	std::filesystem::path statePath, std::uint64_t randomSeed,
	bool persistState, std::vector<r3d::game::MusicCatTrack> tracks,
	std::vector<std::size_t> initialPlaylist)
	: impl_(std::make_unique<Impl>(
		  audio, resources, std::move(statePath), randomSeed,
		  persistState, std::move(tracks), std::move(initialPlaylist)))
{
}

OriginalMenuMusic::~OriginalMenuMusic()
{
	shutdown();
}

bool OriginalMenuMusic::initialize(std::string &error, bool startPlayback)
{
	return impl_->initialize(error, startPlayback);
}

bool OriginalMenuMusic::update(std::string &error)
{
	return impl_->update(error);
}

void OriginalMenuMusic::shutdown() noexcept
{
	impl_->shutdown();
}

bool OriginalMenuMusic::play(std::string &error)
{
	return impl_->play(error);
}

void OriginalMenuMusic::stop() noexcept
{
	impl_->stop();
}

bool OriginalMenuMusic::pause(bool paused, std::string &error)
{
	return impl_->pause(paused, error);
}

bool OriginalMenuMusic::next(std::string &error)
{
	return impl_->next(error);
}

bool OriginalMenuMusic::seekCurrent(std::uint64_t mixerFrame, std::string &error)
{
	return impl_->seekCurrent(mixerFrame, error);
}

bool OriginalMenuMusic::saveState(std::string &error)
{
	return impl_->saveState(error);
}

bool OriginalMenuMusic::verifySavedState(std::string &error)
{
	return impl_->verifySavedState(error);
}

std::optional<std::size_t> OriginalMenuMusic::currentTrack() const noexcept
{
	return impl_->music.currentTrack();
}

std::uint64_t OriginalMenuMusic::currentPositionFrames() const noexcept
{
	if (impl_->voice != r3d::audio::invalidVoice && impl_->audio.isVoiceActive(impl_->voice))
		return impl_->audio.voicePositionFrames(impl_->voice);
	return impl_->music.positionFrames();
}

bool OriginalMenuMusic::paused() const noexcept
{
	return impl_->music.paused();
}

bool OriginalMenuMusic::currentVoiceActive() const noexcept
{
	return impl_->voice != r3d::audio::invalidVoice && impl_->audio.isVoiceActive(impl_->voice);
}

bool OriginalMenuMusic::allTracksLoaded() const noexcept
{
	return std::all_of(impl_->loaded.begin(), impl_->loaded.end(),
	                   [](const LoadedTrack &entry) { return entry.state == LoadState::Loaded; });
}

bool OriginalMenuMusic::backgroundDecodeActive() const noexcept
{
	return impl_->decode.has_value();
}

std::size_t OriginalMenuMusic::loadedTrackCount() const noexcept
{
	return static_cast<std::size_t>(
		std::count_if(impl_->loaded.begin(), impl_->loaded.end(),
	                  [](const LoadedTrack &entry) { return entry.state == LoadState::Loaded; }));
}

std::uint64_t OriginalMenuMusic::transitionCount() const noexcept
{
	return impl_->transitions;
}

const std::vector<std::size_t> &OriginalMenuMusic::playlist() const noexcept
{
	return impl_->music.playlist();
}

const r3d::game::MusicCatTrack *OriginalMenuMusic::track(std::size_t index) const noexcept
{
	return index < impl_->music.tracks().size() ? &impl_->music.tracks()[index] : nullptr;
}

const r3d::audio::SoundInfo *OriginalMenuMusic::trackInfo(std::size_t index) const noexcept
{
	return index < impl_->loaded.size() && impl_->loaded[index].state == LoadState::Loaded ? &impl_->loaded[index].info
	                                                                                       : nullptr;
}

} // namespace rrr3d::audio
