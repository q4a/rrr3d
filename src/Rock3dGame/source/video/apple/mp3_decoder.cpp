/*
 * See header/video/mp3_decoder.h -- in particular why this is a separate
 * translation unit rather than part of video_avf.cpp.
 */

#include "video/mp3_decoder.h"

#import <AudioToolbox/AudioToolbox.h>

#include <algorithm>
#include <cstring>

namespace r3d
{

namespace video
{

namespace
{

/*
 * The MP3 track to PCM, via AudioToolbox.
 *
 * AudioFileStream would let this be incremental; AudioFileOpenWithCallbacks on
 * an in-memory buffer is simpler and the whole track fits comfortably, so the
 * decode happens once at Open() and there is no streaming path to get wrong.
 */
OSStatus ReadProc(void* clientData, SInt64 position, UInt32 requestCount,
	void* buffer, UInt32* actualCount)
{
	std::vector<unsigned char>* data = static_cast<std::vector<unsigned char>*>(clientData);
	if (size_t(position) >= data->size())
	{
		*actualCount = 0;
		return noErr;
	}

	const size_t available = data->size() - size_t(position);
	const UInt32 take = UInt32(std::min<size_t>(requestCount, available));
	std::memcpy(buffer, &(*data)[size_t(position)], take);
	*actualCount = take;
	return noErr;
}

SInt64 SizeProc(void* clientData)
{
	return SInt64(static_cast<std::vector<unsigned char>*>(clientData)->size());
}

}

bool DecodeMp3ToPcm(const unsigned char* data, size_t size,
	std::vector<short>& pcm, unsigned& channels, unsigned& sampleRate)
{
	pcm.clear();

	/* AudioFile wants a seekable object; the packets are scattered through the
	   AVI, so they are gathered into one buffer first. */
	std::vector<unsigned char> buffer(data, data + size);

	AudioFileID file = nullptr;
	if (AudioFileOpenWithCallbacks(&buffer, ReadProc, nullptr, SizeProc, nullptr,
			kAudioFileMP3Type, &file) != noErr || !file)
		return false;

	ExtAudioFileRef ext = nullptr;
	if (ExtAudioFileWrapAudioFileID(file, false, &ext) != noErr || !ext)
	{
		AudioFileClose(file);
		return false;
	}

	AudioStreamBasicDescription source;
	UInt32 propSize = sizeof(source);
	ExtAudioFileGetProperty(ext, kExtAudioFileProperty_FileDataFormat, &propSize, &source);

	channels = unsigned(source.mChannelsPerFrame ? source.mChannelsPerFrame : 2);
	sampleRate = unsigned(source.mSampleRate ? source.mSampleRate : 44100);

	/* 16-bit interleaved PCM, which is what the XAudio2 shim's voices take. */
	AudioStreamBasicDescription target;
	std::memset(&target, 0, sizeof(target));
	target.mFormatID = kAudioFormatLinearPCM;
	target.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
	target.mSampleRate = source.mSampleRate ? source.mSampleRate : 44100;
	target.mChannelsPerFrame = channels;
	target.mBitsPerChannel = 16;
	target.mFramesPerPacket = 1;
	target.mBytesPerFrame = 2 * channels;
	target.mBytesPerPacket = target.mBytesPerFrame;

	if (ExtAudioFileSetProperty(ext, kExtAudioFileProperty_ClientDataFormat,
			sizeof(target), &target) != noErr)
	{
		ExtAudioFileDispose(ext);
		AudioFileClose(file);
		return false;
	}

	const UInt32 chunkFrames = 8192;
	std::vector<short> chunk(size_t(chunkFrames) * channels);

	for (;;)
	{
		AudioBufferList list;
		list.mNumberBuffers = 1;
		list.mBuffers[0].mNumberChannels = channels;
		list.mBuffers[0].mDataByteSize = UInt32(chunk.size() * sizeof(short));
		list.mBuffers[0].mData = &chunk[0];

		UInt32 frames = chunkFrames;
		if (ExtAudioFileRead(ext, &frames, &list) != noErr || frames == 0)
			break;

		pcm.insert(pcm.end(), chunk.begin(), chunk.begin() + long(frames) * channels);
	}

	ExtAudioFileDispose(ext);
	AudioFileClose(file);
	return !pcm.empty();
}

}

}

