/*
 * See header/video/avi_reader.h for what this is and why it exists.
 *
 * RIFF is a tree of chunks: a four-character id, a 32-bit size, then that many
 * bytes, padded to even. A LIST chunk's payload begins with a further id naming
 * what the list holds. AVI puts its stream declarations in 'hdrl' and its data
 * in 'movi', and that is all this needs.
 */

#include "video/avi_reader.h"

#include <cstdio>
#include <cstring>

namespace r3d
{

namespace video
{

namespace
{

inline uint32_t FourCC(const char* s)
{
	return uint32_t(uint8_t(s[0]))
	     | (uint32_t(uint8_t(s[1])) << 8)
	     | (uint32_t(uint8_t(s[2])) << 16)
	     | (uint32_t(uint8_t(s[3])) << 24);
}

const uint32_t cRiff = FourCC("RIFF");
const uint32_t cAvi  = FourCC("AVI ");
const uint32_t cList = FourCC("LIST");
const uint32_t cHdrl = FourCC("hdrl");
const uint32_t cStrl = FourCC("strl");
const uint32_t cStrh = FourCC("strh");
const uint32_t cStrf = FourCC("strf");
const uint32_t cMovi = FourCC("movi");
const uint32_t cVids = FourCC("vids");
const uint32_t cAuds = FourCC("auds");

/* A data chunk is named "##dc"/"##db" for video or "##wb" for audio, where ##
   is the stream index in ASCII. */
bool DataChunk(uint32_t id, unsigned& stream, bool& isAudio)
{
	const unsigned tens = (id & 0xFF) - unsigned('0');
	const unsigned ones = ((id >> 8) & 0xFF) - unsigned('0');
	if (tens > 9 || ones > 9)
		return false;

	const unsigned kind = (id >> 16) & 0xFFFF;
	const unsigned dc = unsigned('d') | (unsigned('c') << 8);
	const unsigned db = unsigned('d') | (unsigned('b') << 8);
	const unsigned wb = unsigned('w') | (unsigned('b') << 8);

	if (kind == dc || kind == db)
		isAudio = false;
	else if (kind == wb)
		isAudio = true;
	else
		return false;

	stream = tens * 10 + ones;
	return true;
}

}

uint32_t AviReader::U32(size_t pos) const
{
	if (pos + 4 > _data.size())
		return 0;
	return uint32_t(_data[pos])
	     | (uint32_t(_data[pos + 1]) << 8)
	     | (uint32_t(_data[pos + 2]) << 16)
	     | (uint32_t(_data[pos + 3]) << 24);
}

uint16_t AviReader::U16(size_t pos) const
{
	if (pos + 2 > _data.size())
		return 0;
	return uint16_t(uint16_t(_data[pos]) | (uint16_t(_data[pos + 1]) << 8));
}

void AviReader::Close()
{
	_data.clear();
	_streams.clear();
	_error.clear();
	_videoStream = -1;
	_audioStream = -1;
}

bool AviReader::Open(const std::string& fileName)
{
	Close();

	std::FILE* file = std::fopen(fileName.c_str(), "rb");
	if (!file)
	{
		_error = "cannot open " + fileName;
		return false;
	}

	std::fseek(file, 0, SEEK_END);
	const long size = std::ftell(file);
	std::fseek(file, 0, SEEK_SET);

	if (size <= 0)
	{
		std::fclose(file);
		_error = "empty file";
		return false;
	}

	/*
	 * Read whole. The largest cutscene is 50 MB, they are played one at a time,
	 * and holding the file means packets can be referenced by offset instead of
	 * copied -- which matters more than the resident size, because the
	 * alternative is a copy per frame at 30fps.
	 */
	_data.resize(size_t(size));
	const size_t got = std::fread(&_data[0], 1, _data.size(), file);
	std::fclose(file);

	if (got != _data.size())
	{
		Close();
		_error = "short read";
		return false;
	}

	if (!Parse())
	{
		_data.clear();
		return false;
	}

	return true;
}

bool AviReader::Parse()
{
	if (_data.size() < 12 || U32(0) != cRiff || U32(8) != cAvi)
	{
		_error = "not a RIFF/AVI file";
		return false;
	}

	size_t pos = 12;
	const size_t end = _data.size();

	while (pos + 8 <= end)
	{
		const uint32_t id = U32(pos);
		const uint32_t size = U32(pos + 4);
		const size_t body = pos + 8;
		/* Chunks are word-aligned; an odd size is followed by a pad byte. */
		const size_t next = body + size + (size & 1);

		if (body + size > end)
			break;

		if (id == cList)
		{
			const uint32_t listType = U32(body);
			if (listType == cHdrl)
			{
				if (!ParseHeaderList(body + 4, body + size))
					return false;
			}
			else if (listType == cMovi)
			{
				if (!ParseMovi(body + 4, body + size))
					return false;
			}
		}

		pos = next;
	}

	if (_streams.empty())
	{
		_error = "no streams";
		return false;
	}

	return true;
}

bool AviReader::ParseHeaderList(size_t pos, size_t end)
{
	while (pos + 8 <= end)
	{
		const uint32_t id = U32(pos);
		const uint32_t size = U32(pos + 4);
		const size_t body = pos + 8;
		const size_t next = body + size + (size & 1);

		if (body + size > end)
			break;

		if (id == cList && U32(body) == cStrl)
		{
			if (!ParseStreamList(body + 4, body + size))
				return false;
		}

		pos = next;
	}

	return true;
}

bool AviReader::ParseStreamList(size_t pos, size_t end)
{
	AviStream stream;
	bool isVideo = false;
	bool isAudio = false;
	bool haveHeader = false;

	while (pos + 8 <= end)
	{
		const uint32_t id = U32(pos);
		const uint32_t size = U32(pos + 4);
		const size_t body = pos + 8;
		const size_t next = body + size + (size & 1);

		if (body + size > end)
			break;

		if (id == cStrh && size >= 32)
		{
			const uint32_t type = U32(body);
			isVideo = type == cVids;
			isAudio = type == cAuds;

			stream.fourCC = U32(body + 4);
			stream.scale = U32(body + 20);
			stream.rate = U32(body + 24);
			if (stream.scale == 0)
				stream.scale = 1;
			haveHeader = true;
		}
		else if (id == cStrf && size > 0)
		{
			stream.format.assign(_data.begin() + long(body),
			                     _data.begin() + long(body + size));

			if (isVideo && size >= 40)
			{
				/* BITMAPINFOHEADER: width and height at 4 and 8, compression
				   at 16. Height can be negative for a bottom-up bitmap, which
				   is meaningless for a coded stream but still appears. */
				stream.width = U32(body + 4);
				const int32_t h = int32_t(U32(body + 8));
				stream.height = unsigned(h < 0 ? -h : h);
				const uint32_t compression = U32(body + 16);
				if (compression)
					stream.fourCC = compression;
			}
			else if (isAudio && size >= 16)
			{
				/* WAVEFORMATEX. */
				stream.formatTag = U16(body);
				stream.channels = U16(body + 2);
				stream.sampleRate = U32(body + 4);
				stream.blockAlign = U16(body + 12);
				stream.bitsPerSample = U16(body + 14);
			}
		}

		pos = next;
	}

	if (!haveHeader)
		return true;

	_streams.push_back(stream);
	const int index = int(_streams.size()) - 1;

	/* First of each kind wins; these files carry exactly one of each. */
	if (isVideo && _videoStream < 0)
		_videoStream = index;
	else if (isAudio && _audioStream < 0)
		_audioStream = index;

	return true;
}

bool AviReader::ParseMovi(size_t pos, size_t end)
{
	while (pos + 8 <= end)
	{
		const uint32_t id = U32(pos);
		const uint32_t size = U32(pos + 4);
		const size_t body = pos + 8;
		const size_t next = body + size + (size & 1);

		if (body + size > end)
			break;

		/* 'rec ' groups one frame's chunks together; its contents are ordinary
		   chunks, so descend rather than skip. */
		if (id == cList)
		{
			if (!ParseMovi(body + 4, body + size))
				return false;
			pos = next;
			continue;
		}

		unsigned streamIndex = 0;
		bool isAudio = false;
		if (DataChunk(id, streamIndex, isAudio) && streamIndex < _streams.size() && size > 0)
		{
			AviPacket packet;
			packet.offset = body;
			packet.size = size;
			_streams[streamIndex].packets.push_back(packet);
		}

		pos = next;
	}

	return true;
}

double AviReader::FrameDuration() const
{
	if (_videoStream < 0)
		return 0.0;

	const AviStream& stream = _streams[size_t(_videoStream)];
	if (stream.rate == 0)
		return 0.0;

	return double(stream.scale) / double(stream.rate);
}

/* ------------------------------------------------------------- Annex B ---- */

namespace
{

/* The next start code at or after `pos`, and how long it is (3 or 4 bytes).
   Returns size when there is none. */
size_t NextStartCode(const unsigned char* data, size_t size, size_t pos, size_t& codeLen)
{
	for (size_t i = pos; i + 3 <= size; ++i)
	{
		if (data[i] == 0 && data[i + 1] == 0)
		{
			if (data[i + 2] == 1)
			{
				codeLen = 3;
				return i;
			}
			if (i + 4 <= size && data[i + 2] == 0 && data[i + 3] == 1)
			{
				codeLen = 4;
				return i;
			}
		}
	}

	codeLen = 0;
	return size;
}

}

bool AnnexBToAvcc(const unsigned char* data, size_t size, std::vector<unsigned char>& out)
{
	out.clear();
	if (!data || size < 4)
		return false;

	size_t codeLen = 0;
	size_t start = NextStartCode(data, size, 0, codeLen);
	if (start == size)
		return false;

	bool any = false;

	while (start < size)
	{
		const size_t nalStart = start + codeLen;
		size_t nextCodeLen = 0;
		const size_t nextStart = NextStartCode(data, size, nalStart, nextCodeLen);
		const size_t nalSize = nextStart - nalStart;

		if (nalSize > 0)
		{
			/* Four-byte big-endian length, which is what
			   CMVideoFormatDescriptionCreateFromH264ParameterSets is told to
			   expect below. */
			const unsigned char header[4] =
			{
				static_cast<unsigned char>((nalSize >> 24) & 0xFF),
				static_cast<unsigned char>((nalSize >> 16) & 0xFF),
				static_cast<unsigned char>((nalSize >> 8) & 0xFF),
				static_cast<unsigned char>(nalSize & 0xFF),
			};
			out.insert(out.end(), header, header + 4);
			out.insert(out.end(), data + nalStart, data + nalStart + nalSize);
			any = true;
		}

		start = nextStart;
		codeLen = nextCodeLen;
		if (codeLen == 0)
			break;
	}

	return any;
}

bool FindParameterSets(const unsigned char* data, size_t size,
	std::vector<unsigned char>& sps, std::vector<unsigned char>& pps)
{
	sps.clear();
	pps.clear();

	if (!data || size < 4)
		return false;

	size_t codeLen = 0;
	size_t start = NextStartCode(data, size, 0, codeLen);

	while (start < size && codeLen != 0)
	{
		const size_t nalStart = start + codeLen;
		size_t nextCodeLen = 0;
		const size_t nextStart = NextStartCode(data, size, nalStart, nextCodeLen);
		const size_t nalSize = nextStart - nalStart;

		if (nalSize > 0)
		{
			/* Low five bits of the first byte are the NAL type: 7 is SPS,
			   8 is PPS. */
			const unsigned type = data[nalStart] & 0x1F;
			if (type == 7 && sps.empty())
				sps.assign(data + nalStart, data + nalStart + nalSize);
			else if (type == 8 && pps.empty())
				pps.assign(data + nalStart, data + nalStart + nalSize);
		}

		start = nextStart;
		codeLen = nextCodeLen;
	}

	return !sps.empty() && !pps.empty();
}

}

}
