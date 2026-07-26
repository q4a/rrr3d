#include "MusicCat.h"

#include <algorithm>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string_view>
#include <utility>

namespace r3d::game
{
namespace
{

constexpr std::string_view stateMagic = "RRR3D_MUSICCAT_STATE";
constexpr unsigned int stateVersion = 1;
constexpr std::uint64_t fallbackRandomState = 0x9e3779b97f4a7c15ULL;

void hashByte(std::uint64_t &hash, unsigned char value) noexcept
{
	hash ^= value;
	hash *= 1099511628211ULL;
}

} // namespace

MusicCat::MusicCat(std::vector<MusicCatTrack> tracks, std::uint64_t randomSeed)
	: tracks_(std::move(tracks)), randomState_(randomSeed == 0 ? fallbackRandomState : randomSeed)
{
}

std::optional<std::size_t> MusicCat::play()
{
	if (playlist_.empty())
		generatePlaylist(std::nullopt);

	while (!playlist_.empty())
	{
		const std::size_t track = playlist_.back();
		playlist_.pop_back();
		if (play(track, false))
			return track;
	}
	return std::nullopt;
}

bool MusicCat::play(std::size_t trackIndex, bool excludeFromPlaylist)
{
	if (trackIndex >= tracks_.size())
		return false;
	if (excludeFromPlaylist)
	{
		playlist_.erase(std::remove(playlist_.begin(), playlist_.end(), trackIndex), playlist_.end());
	}

	currentTrack_ = trackIndex;
	positionFrames_ = 0;
	totalFrames_ = 0;
	if (playlist_.empty())
		generatePlaylist(trackIndex);
	return true;
}

std::optional<std::size_t> MusicCat::next()
{
	return play();
}

bool MusicCat::setPlaylist(std::vector<std::size_t> playlist) noexcept
{
	std::set<std::size_t> unique;
	for (const auto track : playlist)
	{
		if (track >= tracks_.size() || !unique.insert(track).second)
			return false;
	}
	playlist_ = std::move(playlist);
	return true;
}

void MusicCat::setPlaybackPosition(std::uint64_t positionFrames, std::uint64_t totalFrames) noexcept
{
	totalFrames_ = totalFrames;
	positionFrames_ = std::min(positionFrames, totalFrames);
}

void MusicCat::setPaused(bool paused) noexcept
{
	paused_ = paused;
}

const std::vector<MusicCatTrack> &MusicCat::tracks() const noexcept
{
	return tracks_;
}

const std::vector<std::size_t> &MusicCat::playlist() const noexcept
{
	return playlist_;
}

std::optional<std::size_t> MusicCat::currentTrack() const noexcept
{
	return currentTrack_;
}

std::uint64_t MusicCat::positionFrames() const noexcept
{
	return positionFrames_;
}

std::uint64_t MusicCat::totalFrames() const noexcept
{
	return totalFrames_;
}

bool MusicCat::paused() const noexcept
{
	return paused_;
}

std::string MusicCat::serialize() const
{
	std::ostringstream output;
	output << stateMagic << ' ' << stateVersion << '\n';
	output << "catalog " << catalogFingerprint() << '\n';
	output << "tracks " << tracks_.size() << '\n';
	output << "current ";
	if (currentTrack_)
		output << *currentTrack_;
	else
		output << -1;
	output << '\n';
	output << "position " << positionFrames_ << '\n';
	output << "total " << totalFrames_ << '\n';
	output << "paused " << (paused_ ? 1 : 0) << '\n';
	output << "random " << randomState_ << '\n';
	output << "playlist " << playlist_.size();
	for (const std::size_t track : playlist_)
		output << ' ' << track;
	output << '\n';
	return output.str();
}

bool MusicCat::restore(const std::string &state, std::string &error)
{
	std::istringstream input(state);
	std::string magic;
	unsigned int version = 0;
	if (!(input >> magic >> version) || magic != stateMagic || version != stateVersion)
	{
		error = "Unsupported MusicCat state header";
		return false;
	}

	auto readLabel = [&](std::string_view expected) {
		std::string label;
		return static_cast<bool>(input >> label) && label == expected;
	};

	std::uint64_t fingerprint = 0;
	std::size_t trackCount = 0;
	long long current = -1;
	std::uint64_t position = 0;
	std::uint64_t total = 0;
	int paused = 0;
	std::uint64_t random = 0;
	std::size_t playlistCount = 0;
	if (!readLabel("catalog") || !(input >> fingerprint) || !readLabel("tracks") || !(input >> trackCount) ||
	    !readLabel("current") || !(input >> current) || !readLabel("position") || !(input >> position) ||
	    !readLabel("total") || !(input >> total) || !readLabel("paused") || !(input >> paused) ||
	    !readLabel("random") || !(input >> random) || !readLabel("playlist") || !(input >> playlistCount))
	{
		error = "MusicCat state is truncated or malformed";
		return false;
	}
	if (fingerprint != catalogFingerprint() || trackCount != tracks_.size())
	{
		error = "MusicCat state belongs to a different track catalog";
		return false;
	}
	if (current < -1 || (current >= 0 && static_cast<std::size_t>(current) >= tracks_.size()) || paused < 0 ||
	    paused > 1 || random == 0 || position > total || playlistCount > tracks_.size())
	{
		error = "MusicCat state contains an out-of-range value";
		return false;
	}

	std::vector<std::size_t> playlist;
	playlist.reserve(playlistCount);
	std::set<std::size_t> unique;
	for (std::size_t index = 0; index < playlistCount; ++index)
	{
		std::size_t track = 0;
		if (!(input >> track) || track >= tracks_.size() || !unique.insert(track).second)
		{
			error = "MusicCat playlist contains an invalid or duplicate track";
			return false;
		}
		playlist.push_back(track);
	}
	std::string trailing;
	if (input >> trailing)
	{
		error = "MusicCat state contains unexpected trailing data";
		return false;
	}

	playlist_ = std::move(playlist);
	currentTrack_ = current < 0 ? std::nullopt : std::optional<std::size_t>(static_cast<std::size_t>(current));
	positionFrames_ = position;
	totalFrames_ = total;
	paused_ = paused != 0;
	randomState_ = random;
	error.clear();
	return true;
}

void MusicCat::generatePlaylist(std::optional<std::size_t> ignore)
{
	std::map<int, std::vector<std::size_t>> grouped;
	std::vector<std::size_t> defaultGroup;
	for (std::size_t index = 0; index < tracks_.size(); ++index)
	{
		if (tracks_[index].group == 0)
			defaultGroup.push_back(index);
		else
			grouped[tracks_[index].group].push_back(index);
	}

	const std::size_t empty = std::numeric_limits<std::size_t>::max();
	playlist_.assign(tracks_.size(), empty);
	std::vector<std::size_t> slots(tracks_.size());
	std::iota(slots.begin(), slots.end(), 0);

	// This is the original non-zero group spacing algorithm from
	// GameMode::MusicCat::GenRandom, with std::shuffle semantics replaced by
	// the class's serializable RNG.
	for (auto &entry : grouped)
	{
		auto &group = entry.second;
		shuffle(group);
		std::vector<std::size_t> occupied;
		occupied.reserve(group.size());
		for (std::size_t index = 0; index < group.size(); ++index)
		{
			const std::size_t slotsCount =
				std::max(slots.size() - static_cast<std::size_t>(slots.size() / static_cast<double>(group.size())),
			             group.size());
			const std::size_t slotsOffset = slots.size() - slotsCount;
			const std::size_t count = std::max(group.size() - 1, std::size_t{1});
			const std::size_t relative = (count * slotsOffset + 2 * (slotsCount - 1) * index) / (2 * count);
			const std::size_t slot = slots[relative];
			playlist_[slot] = group[index];
			occupied.push_back(slot);
		}
		for (const std::size_t slot : occupied)
			slots.erase(std::remove(slots.begin(), slots.end(), slot), slots.end());
	}

	shuffle(defaultGroup);
	for (std::size_t index = 0; index < defaultGroup.size(); ++index)
		playlist_[slots[index]] = defaultGroup[index];

	if (!playlist_.empty() && ignore && playlist_.back() == *ignore)
		std::iter_swap(playlist_.begin(), playlist_.end() - 1);
}

void MusicCat::shuffle(std::vector<std::size_t> &values) noexcept
{
	for (std::size_t count = values.size(); count > 1; --count)
		std::swap(values[count - 1], values[randomIndex(count)]);
}

std::size_t MusicCat::randomIndex(std::size_t upperBound) noexcept
{
	// xorshift64* keeps the complete RNG state serializable as one integer.
	randomState_ ^= randomState_ >> 12U;
	randomState_ ^= randomState_ << 25U;
	randomState_ ^= randomState_ >> 27U;
	const std::uint64_t value = randomState_ * 2685821657736338717ULL;
	return static_cast<std::size_t>(value % upperBound);
}

std::uint64_t MusicCat::catalogFingerprint() const noexcept
{
	std::uint64_t hash = 1469598103934665603ULL;
	for (const auto &track : tracks_)
	{
		for (const unsigned char value : track.path)
			hashByte(hash, value);
		hashByte(hash, 0);
		for (unsigned int shift = 0; shift < 32; shift += 8)
			hashByte(hash, static_cast<unsigned char>(static_cast<unsigned int>(track.group) >> shift));
	}
	return hash;
}

bool runMusicCatSmokeTest(std::string &error)
{
	const std::vector<MusicCatTrack> tracks = {
		{"Music\\Track1.ogg", "Cold Hard Bitch", "Jet", 0},
		{"Music\\Track14.ogg", "Angel's wings (acoustic)", "Social Distortion", 0},
		{"Music\\Track15.ogg", "On our Way", "Stereoside", 0},
	};
	MusicCat music(tracks, 0x4d75736963436174ULL);
	std::set<std::size_t> firstCycle;
	std::optional<std::size_t> previous;
	for (std::size_t index = 0; index < tracks.size(); ++index)
	{
		const auto selected = music.next();
		if (!selected || (previous && *selected == *previous))
		{
			error = "MusicCat shuffle repeated a track inside its first cycle";
			return false;
		}
		firstCycle.insert(*selected);
		previous = selected;
	}
	if (firstCycle.size() != tracks.size())
	{
		error = "MusicCat shuffle did not select every original menu track";
		return false;
	}
	const auto nextCycle = music.next();
	if (!nextCycle || *nextCycle == *previous)
	{
		error = "MusicCat repeated the final track across a shuffle-cycle boundary";
		return false;
	}

	music.setPlaybackPosition(12345, 67890);
	music.setPaused(true);
	const std::string saved = music.serialize();
	MusicCat restored(tracks, 1);
	if (!restored.restore(saved, error))
		return false;
	if (restored.currentTrack() != music.currentTrack() || restored.playlist() != music.playlist() ||
	    restored.positionFrames() != 12345 || restored.totalFrames() != 67890 || !restored.paused() ||
	    restored.serialize() != saved)
	{
		error = "MusicCat state did not survive a serialization round trip";
		return false;
	}

	std::string invalid = saved;
	const auto playlist = invalid.find("playlist ");
	if (playlist == std::string::npos)
	{
		error = "MusicCat smoke could not locate its saved playlist";
		return false;
	}
	invalid.replace(playlist, invalid.size() - playlist, "playlist 2 0 0\n");
	MusicCat rejected(tracks, 1);
	std::string rejectedError;
	if (rejected.restore(invalid, rejectedError))
	{
		error = "MusicCat accepted a corrupt persisted playlist";
		return false;
	}
	error.clear();
	return true;
}

} // namespace r3d::game
