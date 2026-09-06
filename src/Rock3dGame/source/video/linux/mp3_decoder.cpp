/*
 * See header/video/mp3_decoder.h -- in particular why this is a separate
 * translation unit rather than part of video_avf.cpp.
 */

#include "video/mp3_decoder.h"

namespace r3d
{

namespace video
{

bool DecodeMp3ToPcm(const unsigned char* data, size_t size,
	std::vector<short>& pcm, unsigned& channels, unsigned& sampleRate)
{
	return false;
}

}

}

