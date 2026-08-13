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

// FinalMenu::OnShow stops MusicCat and starts this source track once.
inline constexpr std::array<TrackSpec, 1> finalTracks{{
	{"Music\\TrackFinal.ogg", "", "", 0},
}};

// Ordering, metadata and grouping are the serialized gameMusic/tracks catalog
// from game.xml. UserConfig::gameMusicPlaylist stores indices into this list.
inline constexpr std::array<TrackSpec, 11> gameTracks{{
	{"Music\\Track2.ogg", "Highway Star", "Frantick", 1},
	{"Music\\Track4.ogg", "Born to be Wild", "Frantick", 0},
	{"Music\\Track5.ogg", "Ace of Spider", "Frantick", 0},
	{"Music\\Track6.ogg", "Radar Love", "Frantick", 2},
	{"Music\\Track7.ogg", "Riot in Everyone", "Speed Stroke", 2},
	{"Music\\Track8.ogg", "Paranoid", "Frantick", 3},
	{"Music\\Track9.ogg", "Paranoid", "RuslanoS ft Frantick", 3},
	{"Music\\Track11.ogg", "Age of Rock'n'Roll", "Speed Stroke", 1},
	{"Music\\Track12.ogg", "Radar Love", "Frantick", 1},
	{"Music\\Track13.ogg", "Paranoid", "S.S.H.", 3},
	{"Music\\Track3.ogg", "Highway Star", "Frantick", 1},
}};

inline constexpr char mainButtonClick[] = "Sounds\\UI\\click.ogg";
inline constexpr char rollover[] = "Sounds\\UI\\navedenie.ogg";
inline constexpr char fireGun[] = "Sounds\\fireGun.ogg";

// snd::Engine::Init applies this gain to the XAudio2 mastering voice before
// any Logic category, Source or per-resource volume.  Keeping it separate
// from the user-facing 0..2 controls is essential: the shipped defaults can
// exceed unity precisely because the final mastering stage is one tenth.
inline constexpr float masteringVoiceVolume = 0.1F;

// Logic::AutodetectVolume defaults. The legacy sound API permits gain above
// unity, and the options UI exposes the 0..2 range.
inline constexpr float defaultMusicVolume = 1.2F;
inline constexpr float defaultEffectsVolume = 0.8F;
inline constexpr float defaultVoiceVolume = 1.2F;

} // namespace r3d::game::originalaudio
