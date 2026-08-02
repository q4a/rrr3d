#pragma once

/*
 * H.264 to BGRA, over VideoToolbox.
 *
 * Deliberately asks VideoToolbox for kCVPixelFormatType_32BGRA rather than the
 * native yuv420p: it does the colour conversion on the way out, which is both
 * faster than doing it here and removes an entire class of thing to get wrong
 * (the two BT.601/BT.709 matrices, studio versus full range). BGRA is also
 * exactly what a D3DFMT_A8R8G8B8 texture wants, so the frame goes to the GPU
 * without a second pass.
 *
 * Feeding order is the caller's problem and is simply "in file order": these
 * files are Main profile and the game's cutscenes do not reorder frames enough
 * to need a reorder buffer, so decoded frames are emitted as they come out.
 *
 * Objective-C++ behind a plain C++ interface, so the demuxer and the engine can
 * both include this without dragging CoreMedia into their translation units.
 */

#include <cstddef>
#include <string>
#include <vector>

namespace r3d
{

namespace video
{

class H264Decoder
{
public:
	H264Decoder();
	~H264Decoder();

	H264Decoder(const H264Decoder&) = delete;
	H264Decoder& operator=(const H264Decoder&) = delete;

	/*
	 * SPS and PPS as raw NAL units, without start codes -- FindParameterSets in
	 * avi_reader.h returns them in that form.
	 */
	bool Start(const unsigned char* sps, size_t spsSize,
	           const unsigned char* pps, size_t ppsSize);
	void Stop();

	bool IsRunning() const { return _session != nullptr; }
	const char* Error() const { return _error.empty() ? nullptr : _error.c_str(); }

	unsigned Width() const { return _width; }
	unsigned Height() const { return _height; }

	/*
	 * One AVCC access unit in, at most one frame out.
	 *
	 * Returns true when `frame` has been filled with Width()*Height() BGRA
	 * pixels. False means the decoder consumed the data without producing a
	 * frame, which is ordinary at the start of a stream, or that it failed --
	 * Error() distinguishes them.
	 */
	bool Decode(const unsigned char* avcc, size_t size, std::vector<unsigned char>& frame);

private:
	void* _session = nullptr;         /* VTDecompressionSessionRef */
	void* _format = nullptr;          /* CMVideoFormatDescriptionRef */
	unsigned _width = 0;
	unsigned _height = 0;
	std::string _error;

	/* Filled by the decompression callback, drained by Decode. */
	std::vector<unsigned char> _pending;
	bool _havePending = false;
};

}

}
