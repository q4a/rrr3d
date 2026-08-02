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
 * DECODE ORDER IS NOT DISPLAY ORDER, and this interface exists in this shape
 * because of it. These films are coded I,B,B,B,P,B,B,B,P... -- ffprobe on
 * final_eng.avi says so -- and a B-frame is stored before the frame it is
 * displayed after. Emitting frames as VideoToolbox produced them played the
 * picture visibly back and forth.
 *
 * AVI carries no presentation timestamps at all, so the ordering cannot come
 * from the container. It comes from the bitstream: each sample is given a
 * decode timestamp and no presentation timestamp, VideoToolbox derives the
 * presentation time from the stream's own picture order counts, and Pop()
 * releases frames in that order once enough are in hand to be sure the next one
 * has arrived.
 *
 * Objective-C++ behind a plain C++ interface, so the demuxer and the engine can
 * both include this without dragging CoreMedia into their translation units.
 */

#include "video/h264_poc.h"

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
	 * avi_reader.h returns them in that form. `frameDuration` is seconds per
	 * frame, used to stamp decode times.
	 */
	bool Start(const unsigned char* sps, size_t spsSize,
	           const unsigned char* pps, size_t ppsSize, double frameDuration);
	void Stop();

	bool IsRunning() const { return _session != nullptr; }
	const char* Error() const { return _error.empty() ? nullptr : _error.c_str(); }

	unsigned Width() const { return _width; }
	unsigned Height() const { return _height; }

	/* One AVCC access unit in, in the order the container stores them. */
	bool Push(const unsigned char* avcc, size_t size);

	/*
	 * The next frame in *display* order, or false if not enough have been
	 * decoded yet to know which that is.
	 *
	 * `pts` is its presentation time in seconds from the start of the stream.
	 * Set `flush` when the container has no more packets, which releases what
	 * is still held back.
	 */
	bool Pop(std::vector<unsigned char>& frame, double& pts, bool flush = false);

	/* How many frames are waiting. */
	size_t Pending() const { return _queue.size(); }

private:
	struct Frame
	{
		double pts;
		unsigned long long order;      /* tie-break, and the fallback ordering */
		std::vector<unsigned char> pixels;
	};

	void* _session = nullptr;         /* VTDecompressionSessionRef */
	void* _format = nullptr;          /* CMVideoFormatDescriptionRef */
	unsigned _width = 0;
	unsigned _height = 0;
	double _frameDuration = 1.0 / 30.0;
	unsigned long long _pushed = 0;
	H264PocState _poc;
	std::string _error;

	std::vector<Frame> _queue;

	friend struct DecoderSink;
};

}

}
