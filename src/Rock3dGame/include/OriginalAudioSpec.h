#pragma once

#include <array>

namespace r3d::game::originalaudio
{

// Paths and defaults extracted from the legacy game. Paths are relative to
// the original Data directory, exactly as ResourceManager receives them.
struct TrackSpec
{
	const char *path;
	const char *name;
	const char *band;
	int group;
};

inline constexpr std::array<TrackSpec, 3> menuTracks{{
	{"Music\\Track1.ogg", "Cold Hard Bitch", "Jet", 0},
	{"Music\\Track14.ogg", "Angel's wings (acoustic)", "Social Distortion", 0},
	{"Music\\Track15.ogg", "On our Way", "Stereoside", 0},
}};

inline constexpr char mainButtonClick[] = "Sounds\\UI\\click.ogg";
inline constexpr char rollover[] = "Sounds\\UI\\navedenie.ogg";
inline constexpr char fireGun[] = "Sounds\\fireGun.ogg";

// Logic::AutodetectVolume defaults. The legacy sound API permits gain above
// unity, and the options UI exposes the 0..2 range.
inline constexpr float defaultMusicVolume = 1.2F;
inline constexpr float defaultEffectsVolume = 0.8F;
inline constexpr float defaultVoiceVolume = 1.2F;

} // namespace r3d::game::originalaudio
