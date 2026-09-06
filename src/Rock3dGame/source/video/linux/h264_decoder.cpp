/*
 * See header/video/h264_decoder.h, especially the note about decode order.
 */

#include "video/h264_decoder.h"
#include "video/h264_poc.h"

#include <algorithm>
#include <cstring>

namespace r3d
{

namespace video
{

/*
 * What the decompression callback writes through. A struct rather than the
 * decoder itself so the callback stays a plain C function and the header keeps
 * no CoreMedia in it.
 */
struct DecoderSink
{
	H264Decoder* decoder;
	unsigned long long order;

	/* A member, because the header grants friendship to this struct and not to
	   a free function -- and a free one could not be declared there anyway
	   without naming CVImageBufferRef. */
	void Deliver(double pts, std::vector<unsigned char>& pixels,
		unsigned width, unsigned height) const
	{
		if (pixels.empty())
			return;

		H264Decoder::Frame frame;
		frame.pts = pts;
		frame.order = order;
		frame.pixels.swap(pixels);

		decoder->_width = width;
		decoder->_height = height;
		decoder->_queue.push_back(std::move(frame));
	}
};

namespace
{

const size_t cReorderDepth = 4;

}

H264Decoder::H264Decoder()
{
}

H264Decoder::~H264Decoder()
{
	Stop();
}

bool H264Decoder::Start(const unsigned char* sps, size_t spsSize,
	const unsigned char* pps, size_t ppsSize, double frameDuration)
{
	Stop();
	_error.clear();

	if (!sps || !spsSize || !pps || !ppsSize)
	{
		_error = "missing SPS or PPS";
		return false;
	}

	return false;
}

void H264Decoder::Stop()
{
}

bool H264Decoder::Push(const unsigned char* avcc, size_t size)
{
	if (!_session || !avcc || !size)
		return false;

	return false;
}

bool H264Decoder::Pop(std::vector<unsigned char>& frame, double& pts, bool flush)
{
	if (_queue.empty())
		return false;

	/*
	 * Held back until enough are in hand that the earliest cannot be overtaken.
	 * Four is comfortably above the three consecutive B-frames these streams
	 * use; being generous costs four frames of latency at the start of a
	 * cutscene, which nobody can see.
	 */
	if (!flush && _queue.size() < cReorderDepth)
		return false;

	size_t best = 0;
	for (size_t i = 1; i < _queue.size(); ++i)
	{
		const Frame& a = _queue[i];
		const Frame& b = _queue[best];

		/* Presentation time when both have one; decode order otherwise, since a
		   stream VideoToolbox gave no timestamps for is one where decode order
		   is the best answer available. */
		const bool earlier = (a.pts >= 0.0 && b.pts >= 0.0)
			? (a.pts < b.pts)
			: (a.order < b.order);

		if (earlier)
			best = i;
	}

	frame.swap(_queue[best].pixels);
	pts = _queue[best].pts;
	_queue.erase(_queue.begin() + long(best));
	return true;
}

}

}
