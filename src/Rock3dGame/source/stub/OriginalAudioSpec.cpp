#include "OriginalAudioSpec.h"

#include "resource/ResourceFileSystem.h"

namespace r3d::game::originalaudio
{
MusicCatalog loadOriginalMusicCatalog(
	const resource::ResourceFileSystem &resources)
{
	return originalgamedata::loadOriginalGameDataCatalog(resources).music;
}

} // namespace r3d::game::originalaudio
