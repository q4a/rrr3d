/*
 * See header/video/h264_decoder.h.
 *
 * The shape of a VideoToolbox decode is: describe the format once from the SPS
 * and PPS, open a session against that description asking for the output pixel
 * format you want, then hand it sample buffers. Frames arrive on a callback,
 * not as a return value, which is why there is a one-frame slot here rather
 * than a return.
 */

#include "video/h264_decoder.h"

#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <VideoToolbox/VideoToolbox.h>

#include <cstring>

namespace r3d
{

namespace video
{

namespace
{

struct DecodeSlot
{
	std::vector<unsigned char>* frame;
	unsigned width;
	unsigned height;
	bool filled;
};

void CopyPixelBuffer(CVPixelBufferRef buffer, DecodeSlot* slot)
{
	CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);

	const unsigned width = unsigned(CVPixelBufferGetWidth(buffer));
	const unsigned height = unsigned(CVPixelBufferGetHeight(buffer));
	const size_t pitch = CVPixelBufferGetBytesPerRow(buffer);
	const unsigned char* base =
		static_cast<const unsigned char*>(CVPixelBufferGetBaseAddress(buffer));

	if (base && width && height)
	{
		slot->frame->resize(size_t(width) * height * 4);
		/* Row by row: the buffer's pitch is padded for alignment and is
		   generally wider than width*4. */
		for (unsigned y = 0; y < height; ++y)
			std::memcpy(&(*slot->frame)[size_t(y) * width * 4],
				base + size_t(y) * pitch, size_t(width) * 4);

		slot->width = width;
		slot->height = height;
		slot->filled = true;
	}

	CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
}

void OnDecodedFrame(void* decompressionOutputRefCon, void* sourceFrameRefCon,
	OSStatus status, VTDecodeInfoFlags flags, CVImageBufferRef imageBuffer,
	CMTime presentationTimeStamp, CMTime presentationDuration)
{
	(void)decompressionOutputRefCon;
	(void)flags;
	(void)presentationTimeStamp;
	(void)presentationDuration;

	/*
	 * sourceFrameRefCon, not decompressionOutputRefCon. The first is what
	 * VTDecompressionSessionDecodeFrame was passed for this specific frame; the
	 * second is fixed at session creation, and there is nothing useful to put
	 * there because the destination changes per call.
	 */
	DecodeSlot* slot = static_cast<DecodeSlot*>(sourceFrameRefCon);
	if (!slot || status != noErr || !imageBuffer)
		return;

	CopyPixelBuffer(imageBuffer, slot);
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
	const unsigned char* pps, size_t ppsSize)
{
	Stop();
	_error.clear();

	if (!sps || !spsSize || !pps || !ppsSize)
	{
		_error = "missing SPS or PPS";
		return false;
	}

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
	callback.decompressionOutputRefCon = NULL;   /* set per Decode call */

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
	_havePending = false;
	_pending.clear();
}

bool H264Decoder::Decode(const unsigned char* avcc, size_t size,
	std::vector<unsigned char>& frame)
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

	CMSampleBufferRef sampleBuffer = NULL;
	const size_t sampleSize = size;
	status = CMSampleBufferCreateReady(kCFAllocatorDefault, blockBuffer,
		static_cast<CMVideoFormatDescriptionRef>(_format), 1, 0, NULL, 1, &sampleSize,
		&sampleBuffer);

	CFRelease(blockBuffer);

	if (status != noErr || !sampleBuffer)
	{
		_error = "CMSampleBufferCreateReady failed";
		return false;
	}

	DecodeSlot slot;
	slot.frame = &_pending;
	slot.width = 0;
	slot.height = 0;
	slot.filled = false;

	/*
	 * Synchronous. The alternative is a queue and a condition variable for a
	 * decoder that is already keeping up with 30fps of 1280x720, and the
	 * cutscene path has no frame budget worth defending.
	 */
	VTDecodeInfoFlags infoFlags = 0;
	status = VTDecompressionSessionDecodeFrame(
		static_cast<VTDecompressionSessionRef>(_session), sampleBuffer,
		kVTDecodeFrame_EnableTemporalProcessing, &slot, &infoFlags);

	CFRelease(sampleBuffer);

	if (status != noErr)
	{
		_error = "VTDecompressionSessionDecodeFrame failed";
		return false;
	}

	if (!slot.filled)
		return false;

	_width = slot.width;
	_height = slot.height;
	frame.swap(_pending);
	return true;
}

}

}
