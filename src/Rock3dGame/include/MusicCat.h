#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace r3d::game
{

struct MusicCatTrack
{
	std::string path;
	std::string name;
	std::string band;
	int group = 0;
};

// Platform-independent port of GameMode::MusicCat. Audio decoding and native
// playback deliberately live outside this class; MusicCat owns only the
// original playlist policy plus resumable user state.
class MusicCat
{
  public:
	explicit MusicCat(std::vector<MusicCatTrack> tracks, std::uint64_t randomSeed);

	std::optional<std::size_t> play();
	bool play(std::size_t trackIndex, bool excludeFromPlaylist = true);
	std::optional<std::size_t> next();
	bool setPlaylist(std::vector<std::size_t> playlist) noexcept;

	void setPlaybackPosition(std::uint64_t positionFrames, std::uint64_t totalFrames) noexcept;
	void setPaused(bool paused) noexcept;

	const std::vector<MusicCatTrack> &tracks() const noexcept;
	const std::vector<std::size_t> &playlist() const noexcept;
	std::optional<std::size_t> currentTrack() const noexcept;
	std::uint64_t positionFrames() const noexcept;
	std::uint64_t totalFrames() const noexcept;
	bool paused() const noexcept;

	std::string serialize() const;
	bool restore(const std::string &state, std::string &error);

  private:
	void generatePlaylist(std::optional<std::size_t> ignore);
	void shuffle(std::vector<std::size_t> &values) noexcept;
	std::size_t randomIndex(std::size_t upperBound) noexcept;
	std::uint64_t catalogFingerprint() const noexcept;

	std::vector<MusicCatTrack> tracks_;
	std::vector<std::size_t> playlist_;
	std::optional<std::size_t> currentTrack_;
	std::uint64_t positionFrames_ = 0;
	std::uint64_t totalFrames_ = 0;
	std::uint64_t randomState_ = 1;
	bool paused_ = false;
};

bool runMusicCatSmokeTest(std::string &error);

} // namespace r3d::game
