#pragma once

#include "MusicCat.h"
#include "audio/AudioBackend.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace r3d::resource
{
class ResourceFileSystem;
}

namespace rrr3d::audio
{

// Connects the portable MusicCat policy to AudioBackend. Ogg decoding happens
// on one background worker while the render/input loop continues.
class OriginalMenuMusic
{
  public:
	OriginalMenuMusic(r3d::audio::AudioBackend &audio, const r3d::resource::ResourceFileSystem &resources,
	                  std::filesystem::path statePath, std::uint64_t randomSeed, bool persistState);
	OriginalMenuMusic(r3d::audio::AudioBackend &audio,
	                  const r3d::resource::ResourceFileSystem &resources,
	                  std::filesystem::path statePath,
	                  std::uint64_t randomSeed, bool persistState,
	                  std::vector<r3d::game::MusicCatTrack> tracks,
	                  std::vector<std::size_t> initialPlaylist);
	~OriginalMenuMusic();

	OriginalMenuMusic(const OriginalMenuMusic &) = delete;
	OriginalMenuMusic &operator=(const OriginalMenuMusic &) = delete;

	// GameMode starts menu music during initialization, but does not consume a
	// game-music playlist entry until DoStartRace.  The second form preserves
	// that source lifetime while still allowing the first track to decode in
	// the background.
	bool initialize(std::string &error, bool startPlayback = true);
	bool update(std::string &error);
	void shutdown() noexcept;

	bool play(std::string &error);
	void stop() noexcept;
	bool pause(bool paused, std::string &error);
	bool next(std::string &error);
	bool seekCurrent(std::uint64_t mixerFrame, std::string &error);
	bool saveState(std::string &error);
	bool verifySavedState(std::string &error);

	std::optional<std::size_t> currentTrack() const noexcept;
	std::uint64_t currentPositionFrames() const noexcept;
	bool paused() const noexcept;
	bool currentVoiceActive() const noexcept;
	bool allTracksLoaded() const noexcept;
	bool backgroundDecodeActive() const noexcept;
	std::size_t loadedTrackCount() const noexcept;
	std::uint64_t transitionCount() const noexcept;
	const std::vector<std::size_t> &playlist() const noexcept;
	const r3d::game::MusicCatTrack *track(std::size_t index) const noexcept;
	const r3d::audio::SoundInfo *trackInfo(std::size_t index) const noexcept;

  private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace rrr3d::audio
