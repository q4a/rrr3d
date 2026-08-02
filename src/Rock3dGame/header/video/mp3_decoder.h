#pragma once

/*
 * The cutscene soundtrack to PCM, via AudioToolbox.
 *
 * Behind a plain C++ interface for the same reason H264Decoder is, and here it
 * is not merely tidiness: XPlatform's <windows.h> typedefs BOOL as INT while
 * Objective-C's objc.h typedefs it as bool, so a translation unit that includes
 * both does not compile. Keeping every Apple header on this side of the line
 * lets video_avf.cpp include the engine and the Win32 shim as usual.
 *
 * The whole track is decoded in one call. These films run about thirty seconds
 * of stereo 48kHz, which is around 12 MB as 16-bit PCM -- less than one of the
 * textures the game loads without comment, and it removes the streaming path
 * and everything that can go wrong in it.
 */

#include <cstddef>
#include <vector>

namespace r3d
{

namespace video
{

/*
 * `data` is the MP3 bitstream, which for an AVI means its audio packets
 * concatenated in order.
 *
 * Returns false if AudioToolbox will not open or decode it, in which case the
 * cutscene plays silently rather than not at all.
 */
bool DecodeMp3ToPcm(const unsigned char* data, size_t size,
	std::vector<short>& pcm, unsigned& channels, unsigned& sampleRate);

}

}
