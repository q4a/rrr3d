#pragma once

/*
 * A RIFF/AVI demuxer, for the game's cutscenes.
 *
 * The 14 files in Data/Video are H.264 Main/L3.1 yuv420p plus MP3, in an AVI
 * container, and the video is Annex B with SPS and PPS in band -- the first
 * packet of final_eng.avi begins 00 00 00 01 67. That is worth stating because
 * it decides the two things this header exists to serve: AVFoundation will not
 * open an AVI at all, and VideoToolbox will not take Annex B, so something has
 * to read the container and hand over length-prefixed NAL units.
 *
 * Scope is exactly that. It reads the streams the game ships and no more: one
 * video stream, one audio stream, no interleave beyond what these files use, no
 * OpenDML extensions. A file it cannot read is reported rather than guessed at.
 *
 * Nothing here touches Direct3D, VideoToolbox or the engine, so src/VideoProbe
 * can exercise it on its own.
 */

#include <cstdint>
#include <string>
#include <vector>

namespace r3d
{

namespace video
{

struct AviPacket
{
	/* Offset and length within the file buffer, rather than a copy: a 50 MB
	   cutscene is not worth duplicating a packet at a time. */
	size_t offset = 0;
	size_t size = 0;
	bool keyFrame = false;
};

struct AviStream
{
	uint32_t fourCC = 0;             /* 'H264', 'mp3 ', ... from strh/strf */
	uint32_t formatTag = 0;          /* WAVE_FORMAT_* for audio, else 0 */
	unsigned width = 0;
	unsigned height = 0;
	unsigned channels = 0;
	unsigned sampleRate = 0;
	unsigned bitsPerSample = 0;
	unsigned blockAlign = 0;
	/* The stream's own rate/scale, which is what makes frames into seconds. */
	uint32_t rate = 0;
	uint32_t scale = 1;
	/* strf, verbatim. For audio this is the WAVEFORMATEX the decoder needs. */
	std::vector<unsigned char> format;
	std::vector<AviPacket> packets;
};

class AviReader
{
public:
	bool Open(const std::string& fileName);
	void Close();

	bool IsOpen() const { return !_data.empty(); }
	const std::string& Error() const { return _error; }

	/* -1 when the file has no stream of that kind. */
	int VideoStream() const { return _videoStream; }
	int AudioStream() const { return _audioStream; }

	const AviStream& Stream(int index) const { return _streams[size_t(index)]; }
	unsigned StreamCount() const { return unsigned(_streams.size()); }

	/* Seconds per frame for the video stream, from its own rate and scale. */
	double FrameDuration() const;

	const unsigned char* Data() const { return _data.empty() ? nullptr : &_data[0]; }
	size_t Size() const { return _data.size(); }

	const unsigned char* PacketData(const AviPacket& packet) const
	{
		return _data.empty() ? nullptr : &_data[packet.offset];
	}

private:
	bool Parse();
	bool ParseHeaderList(size_t pos, size_t end);
	bool ParseStreamList(size_t pos, size_t end);
	bool ParseMovi(size_t pos, size_t end);

	uint32_t U32(size_t pos) const;
	uint16_t U16(size_t pos) const;

	std::vector<unsigned char> _data;
	std::vector<AviStream> _streams;
	std::string _error;
	int _videoStream = -1;
	int _audioStream = -1;
};

/*
 * Annex B to AVCC, which is the conversion VideoToolbox requires: it takes
 * length-prefixed NAL units and rejects start codes.
 *
 * Returns false if no NAL unit was found at all, which is how a stream that is
 * already AVCC -- or is not H.264 -- is declined rather than mangled.
 */
bool AnnexBToAvcc(const unsigned char* data, size_t size,
	std::vector<unsigned char>& out);

/*
 * The SPS and PPS carried in an Annex B buffer. Both are needed to build a
 * CMVideoFormatDescription, and these files carry them in band on the first
 * packet rather than in the container's extradata.
 */
bool FindParameterSets(const unsigned char* data, size_t size,
	std::vector<unsigned char>& sps, std::vector<unsigned char>& pps);

}

}
