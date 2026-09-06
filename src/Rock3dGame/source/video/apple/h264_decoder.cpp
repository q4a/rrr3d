/*
 * See header/video/h264_decoder.h, especially the note about decode order.
 */

#include "video/h264_decoder.h"
#include "video/h264_poc.h"

#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <VideoToolbox/VideoToolbox.h>

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

void CopyPixelBuffer(CVPixelBufferRef buffer, std::vector<unsigned char>& out,
	unsigned& width, unsigned& height)
{
	CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);

	width = unsigned(CVPixelBufferGetWidth(buffer));
	height = unsigned(CVPixelBufferGetHeight(buffer));
	const size_t pitch = CVPixelBufferGetBytesPerRow(buffer);
	const unsigned char* base =
		static_cast<const unsigned char*>(CVPixelBufferGetBaseAddress(buffer));

	if (base && width && height)
	{
		out.resize(size_t(width) * height * 4);
		/* Row by row: the pitch is padded for alignment and is generally wider
		   than width*4. */
		for (unsigned y = 0; y < height; ++y)
			std::memcpy(&out[size_t(y) * width * 4], base + size_t(y) * pitch,
				size_t(width) * 4);
	}

	CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
}

}

namespace
{

void OnDecodedFrame(void* decompressionOutputRefCon, void* sourceFrameRefCon,
	OSStatus status, VTDecodeInfoFlags flags, CVImageBufferRef imageBuffer,
	CMTime presentationTimeStamp, CMTime presentationDuration)
{
	(void)decompressionOutputRefCon;
	(void)flags;
	(void)presentationDuration;

	DecoderSink* sink = static_cast<DecoderSink*>(sourceFrameRefCon);
	if (!sink || !sink->decoder || status != noErr || !imageBuffer)
		return;

	/*
	 * The presentation time VideoToolbox derived from the bitstream. AVI
	 * supplies none, so this is the only ordering information there is; when it
	 * is missing the decode order is used instead, which is right for a stream
	 * without B-frames and no worse than the old behaviour otherwise.
	 */
	const double pts = CMTIME_IS_VALID(presentationTimeStamp)
		? CMTimeGetSeconds(presentationTimeStamp)
		: -1.0;

	std::vector<unsigned char> pixels;
	unsigned width = 0;
	unsigned height = 0;
	CopyPixelBuffer(imageBuffer, pixels, width, height);

	sink->Deliver(pts, pixels, width, height);
}

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

	_frameDuration = frameDuration > 0.0 ? frameDuration : 1.0 / 30.0;

	/*
	 * The display order comes from here, not from VideoToolbox. Measured: given
	 * a sample with an invalid presentation stamp, VideoToolbox hands the same
	 * invalid stamp straight back -- it does not derive one from the bitstream.
	 * So the picture order counts are read out of the stream instead, and the
	 * SPS is what says how.
	 */
	_poc = H264PocState();
	ParseSpsForPoc(sps, spsSize, _poc);

	const uint8_t* const parameterSets[2] = { sps, pps };
	const size_t parameterSetSizes[2] = { spsSize, ppsSize };

	CMVideoFormatDescriptionRef format = NULL;
	/* The 4 is the NAL length prefix AnnexBToAvcc writes. */
	OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(
		kCFAllocatorDefault, 2, parameterSets, parameterSetSizes, 4, &format);

	if (status != noErr || !format)
	{
		_error = "CMVideoFormatDescriptionCreateFromH264ParameterSets failed";
		return false;
	}

	const CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(format);
	_width = unsigned(dimensions.width);
	_height = unsigned(dimensions.height);

	/*
	 * 32BGRA out, so VideoToolbox does the YUV to RGB conversion. Asking for
	 * the native format and converting here would mean carrying the BT.601 and
	 * BT.709 matrices and their range variants, and getting one wrong is a
	 * subtle colour shift rather than a visible failure.
	 */
	const void* attributeKeys[] =
	{
		kCVPixelBufferPixelFormatTypeKey,
		kCVPixelBufferIOSurfacePropertiesKey,
	};

	const SInt32 pixelFormat = kCVPixelFormatType_32BGRA;
	CFNumberRef pixelFormatValue = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pixelFormat);
	CFDictionaryRef ioSurface = CFDictionaryCreate(kCFAllocatorDefault, NULL, NULL, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	const void* attributeValues[] = { pixelFormatValue, ioSurface };
	CFDictionaryRef attributes = CFDictionaryCreate(kCFAllocatorDefault,
		attributeKeys, attributeValues, 2,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	VTDecompressionOutputCallbackRecord callback;
	callback.decompressionOutputCallback = OnDecodedFrame;
	callback.decompressionOutputRefCon = NULL;   /* the per-frame sink is used */

	VTDecompressionSessionRef session = NULL;
	status = VTDecompressionSessionCreate(kCFAllocatorDefault, format, NULL,
		attributes, &callback, &session);

	CFRelease(attributes);
	CFRelease(ioSurface);
	CFRelease(pixelFormatValue);

	if (status != noErr || !session)
	{
		CFRelease(format);
		_error = "VTDecompressionSessionCreate failed";
		return false;
	}

	/* const_cast: the member is void* so the header stays free of CoreMedia,
	   and CMVideoFormatDescriptionRef is a pointer-to-const. */
	_format = const_cast<void*>(static_cast<const void*>(format));
	_session = session;
	_pushed = 0;
	_queue.clear();
	return true;
}

void H264Decoder::Stop()
{
	if (_session)
	{
		VTDecompressionSessionInvalidate(static_cast<VTDecompressionSessionRef>(_session));
		CFRelease(static_cast<VTDecompressionSessionRef>(_session));
		_session = nullptr;
	}

	if (_format)
	{
		CFRelease(static_cast<CMVideoFormatDescriptionRef>(_format));
		_format = nullptr;
	}

	_width = 0;
	_height = 0;
	_pushed = 0;
	_queue.clear();
}

bool H264Decoder::Push(const unsigned char* avcc, size_t size)
{
	if (!_session || !avcc || !size)
		return false;

	CMBlockBufferRef blockBuffer = NULL;
	OSStatus status = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault,
		const_cast<unsigned char*>(avcc), size, kCFAllocatorNull, NULL, 0, size, 0,
		&blockBuffer);

	if (status != noErr || !blockBuffer)
	{
		_error = "CMBlockBufferCreateWithMemoryBlock failed";
		return false;
	}

	/*
	 * A decode timestamp but no presentation timestamp.
	 *
	 * AVI stores frames in decode order and carries no timestamps, so a decode
	 * time is what it genuinely knows. Leaving the presentation stamp invalid
	 * is what makes VideoToolbox derive one from the stream's own picture order
	 * counts and report it on the callback, which is the ordering Pop() then
	 * uses. Supplying a made-up presentation time here instead would assert
	 * that decode order is display order -- which for these B-frame-coded films
	 * is precisely the thing that was wrong.
	 */
	/*
	 * The picture order count of this access unit, as its presentation time.
	 * Two POC units are one frame -- the specification counts fields -- so the
	 * halving is what turns it into a frame index. Where it cannot be read the
	 * decode index stands in, which is correct for a stream without B-frames.
	 */
	int poc = 0;
	const bool havePoc = ComputePoc(avcc, size, _poc, poc);
	const double pts = havePoc
		? (double(poc) * 0.5 * _frameDuration)
		: (double(_pushed) * _frameDuration);

	CMSampleTimingInfo timing;
	timing.duration = CMTimeMakeWithSeconds(_frameDuration, 90000);
	timing.presentationTimeStamp = CMTimeMakeWithSeconds(pts, 90000);
	timing.decodeTimeStamp = CMTimeMakeWithSeconds(double(_pushed) * _frameDuration, 90000);

	CMSampleBufferRef sampleBuffer = NULL;
	const size_t sampleSize = size;
	status = CMSampleBufferCreateReady(kCFAllocatorDefault, blockBuffer,
		static_cast<CMVideoFormatDescriptionRef>(_format), 1, 1, &timing, 1, &sampleSize,
		&sampleBuffer);

	CFRelease(blockBuffer);

	if (status != noErr || !sampleBuffer)
	{
		_error = "CMSampleBufferCreateReady failed";
		return false;
	}

	DecoderSink sink;
	sink.decoder = this;
	sink.order = _pushed++;

	/*
	 * Temporal processing on, so VideoToolbox may hold a frame back and emit it
	 * once what it depends on has arrived. Output is synchronous, so `sink`
	 * cannot outlive this call.
	 */
	VTDecodeInfoFlags infoFlags = 0;
	status = VTDecompressionSessionDecodeFrame(
		static_cast<VTDecompressionSessionRef>(_session), sampleBuffer,
		kVTDecodeFrame_EnableTemporalProcessing, &sink, &infoFlags);

	CFRelease(sampleBuffer);

	if (status != noErr)
	{
		_error = "VTDecompressionSessionDecodeFrame failed";
		return false;
	}

	return true;
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
