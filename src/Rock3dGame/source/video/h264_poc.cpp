/*
 * See header/video/h264_poc.h.
 *
 * Everything here follows ISO/IEC 14496-10: 7.3.2.1.1 for the sequence
 * parameter set, 7.3.3 for the slice header, and 8.2.1.1 for the picture order
 * count itself. Only as much of each is read as the next field's position
 * depends on -- the fields are variable-length, so they cannot be skipped, only
 * parsed through.
 */

#include "video/h264_poc.h"

#include <cstring>
#include <vector>

namespace r3d
{

namespace video
{

namespace
{

/* A bit reader over RBSP, which is the NAL payload with its emulation
   prevention bytes already removed. */
class BitReader
{
public:
	BitReader(const unsigned char* data, size_t size): _data(data), _size(size) {}

	bool Bit(unsigned& out)
	{
		if (_pos >= _size * 8)
			return false;

		out = (_data[_pos >> 3] >> (7 - (_pos & 7))) & 1;
		++_pos;
		return true;
	}

	bool Bits(unsigned count, unsigned& out)
	{
		out = 0;
		for (unsigned i = 0; i < count; ++i)
		{
			unsigned bit = 0;
			if (!Bit(bit))
				return false;
			out = (out << 1) | bit;
		}
		return true;
	}

	/* Unsigned exp-Golomb, 9.1. */
	bool UE(unsigned& out)
	{
		unsigned zeros = 0;
		for (;;)
		{
			unsigned bit = 0;
			if (!Bit(bit))
				return false;
			if (bit)
				break;
			if (++zeros > 31)
				return false;
		}

		unsigned rest = 0;
		if (zeros && !Bits(zeros, rest))
			return false;

		out = (1u << zeros) - 1 + rest;
		return true;
	}

	/* Signed exp-Golomb, 9.1.1. */
	bool SE(int& out)
	{
		unsigned value = 0;
		if (!UE(value))
			return false;

		out = (value & 1) ? int((value + 1) / 2) : -int(value / 2);
		return true;
	}

private:
	const unsigned char* _data;
	size_t _size;
	size_t _pos = 0;
};

/*
 * NAL payload to RBSP: 00 00 03 in the stream means a literal 00 00 followed by
 * an escape byte that is not part of the data. Missing this misreads every
 * field after the first occurrence.
 */
void Unescape(const unsigned char* nal, size_t size, std::vector<unsigned char>& rbsp)
{
	rbsp.clear();
	rbsp.reserve(size);

	for (size_t i = 0; i < size; ++i)
	{
		if (i + 2 < size && nal[i] == 0 && nal[i + 1] == 0 && nal[i + 2] == 3)
		{
			rbsp.push_back(0);
			rbsp.push_back(0);
			i += 2;
			continue;
		}
		rbsp.push_back(nal[i]);
	}
}

/* 7.3.2.1.1.1 -- read past a scaling list without keeping it. */
bool SkipScalingList(BitReader& bits, unsigned size)
{
	int lastScale = 8;
	int nextScale = 8;

	for (unsigned i = 0; i < size; ++i)
	{
		if (nextScale != 0)
		{
			int delta = 0;
			if (!bits.SE(delta))
				return false;
			nextScale = (lastScale + delta + 256) % 256;
		}
		lastScale = nextScale == 0 ? lastScale : nextScale;
	}

	return true;
}

}

bool ParseSpsForPoc(const unsigned char* nal, size_t size, H264PocState& state)
{
	if (!nal || size < 4)
		return false;

	std::vector<unsigned char> rbsp;
	Unescape(nal + 1, size - 1, rbsp);      /* skip the NAL header byte */
	if (rbsp.size() < 3)
		return false;

	BitReader bits(&rbsp[0], rbsp.size());

	unsigned profileIdc = 0;
	unsigned constraints = 0;
	unsigned levelIdc = 0;
	unsigned spsId = 0;

	if (!bits.Bits(8, profileIdc) || !bits.Bits(8, constraints) ||
	    !bits.Bits(8, levelIdc) || !bits.UE(spsId))
		return false;

	state.separateColourPlane = false;

	/* The high profiles carry chroma and scaling information the baseline ones
	   do not; everything after it shifts if this is skipped. */
	if (profileIdc == 100 || profileIdc == 110 || profileIdc == 122 ||
	    profileIdc == 244 || profileIdc == 44  || profileIdc == 83  ||
	    profileIdc == 86  || profileIdc == 118 || profileIdc == 128 ||
	    profileIdc == 138 || profileIdc == 139 || profileIdc == 134 ||
	    profileIdc == 135)
	{
		unsigned chromaFormat = 0;
		if (!bits.UE(chromaFormat))
			return false;

		if (chromaFormat == 3)
		{
			unsigned separate = 0;
			if (!bits.Bit(separate))
				return false;
			state.separateColourPlane = separate != 0;
		}

		unsigned bitDepthLuma = 0;
		unsigned bitDepthChroma = 0;
		unsigned transformBypass = 0;
		unsigned scalingMatrix = 0;

		if (!bits.UE(bitDepthLuma) || !bits.UE(bitDepthChroma) ||
		    !bits.Bit(transformBypass) || !bits.Bit(scalingMatrix))
			return false;

		if (scalingMatrix)
		{
			const unsigned lists = (chromaFormat != 3) ? 8u : 12u;
			for (unsigned i = 0; i < lists; ++i)
			{
				unsigned present = 0;
				if (!bits.Bit(present))
					return false;
				if (present && !SkipScalingList(bits, i < 6 ? 16u : 64u))
					return false;
			}
		}
	}

	unsigned log2MaxFrameNumMinus4 = 0;
	if (!bits.UE(log2MaxFrameNumMinus4))
		return false;
	state.log2MaxFrameNum = log2MaxFrameNumMinus4 + 4;

	unsigned picOrderCntType = 0;
	if (!bits.UE(picOrderCntType))
		return false;
	state.picOrderCntType = picOrderCntType;

	if (picOrderCntType == 0)
	{
		unsigned log2MaxPocLsbMinus4 = 0;
		if (!bits.UE(log2MaxPocLsbMinus4))
			return false;
		state.log2MaxPicOrderCntLsb = log2MaxPocLsbMinus4 + 4;
	}

	/*
	 * frame_mbs_only_flag decides whether the slice header carries
	 * field_pic_flag, which sits between frame_num and pic_order_cnt_lsb. It is
	 * several fields further on, and each one in between is variable length, so
	 * they have to be parsed through.
	 */
	unsigned maxNumRefFrames = 0;
	unsigned gapsAllowed = 0;
	unsigned picWidthMbs = 0;
	unsigned picHeightMapUnits = 0;
	unsigned frameMbsOnly = 0;

	if (!bits.UE(maxNumRefFrames) || !bits.Bit(gapsAllowed) ||
	    !bits.UE(picWidthMbs) || !bits.UE(picHeightMapUnits) ||
	    !bits.Bit(frameMbsOnly))
		return false;

	state.frameMbsOnly = frameMbsOnly != 0;

	state.prevPicOrderCntMsb = 0;
	state.prevPicOrderCntLsb = 0;
	state.valid = true;
	return true;
}

bool ComputePoc(const unsigned char* avcc, size_t size, H264PocState& state, int& poc)
{
	if (!state.valid || !avcc)
		return false;

	/* Type 2 is defined to have display order equal to decode order, and type 1
	   is not implemented -- see the header. */
	if (state.picOrderCntType != 0)
		return false;

	/* Walk the access unit's length-prefixed NAL units for the first slice. */
	size_t pos = 0;
	while (pos + 4 <= size)
	{
		const size_t nalSize = (size_t(avcc[pos]) << 24) | (size_t(avcc[pos + 1]) << 16)
		                     | (size_t(avcc[pos + 2]) << 8) | size_t(avcc[pos + 3]);
		pos += 4;

		if (nalSize == 0 || pos + nalSize > size)
			break;

		const unsigned char* nal = avcc + pos;
		const unsigned type = nal[0] & 0x1F;
		pos += nalSize;

		/* 1 is a non-IDR slice, 5 an IDR slice. Everything else -- parameter
		   sets, SEI, delimiters -- carries no picture order count. */
		if (type != 1 && type != 5)
			continue;

		std::vector<unsigned char> rbsp;
		Unescape(nal + 1, nalSize - 1, rbsp);
		if (rbsp.empty())
			return false;

		BitReader bits(&rbsp[0], rbsp.size());

		unsigned firstMb = 0;
		unsigned sliceType = 0;
		unsigned ppsId = 0;
		if (!bits.UE(firstMb) || !bits.UE(sliceType) || !bits.UE(ppsId))
			return false;

		if (state.separateColourPlane)
		{
			unsigned colourPlane = 0;
			if (!bits.Bits(2, colourPlane))
				return false;
		}

		unsigned frameNum = 0;
		if (!bits.Bits(state.log2MaxFrameNum, frameNum))
			return false;

		bool fieldPic = false;
		if (!state.frameMbsOnly)
		{
			unsigned fieldPicFlag = 0;
			if (!bits.Bit(fieldPicFlag))
				return false;
			fieldPic = fieldPicFlag != 0;

			if (fieldPic)
			{
				unsigned bottom = 0;
				if (!bits.Bit(bottom))
					return false;
			}
		}

		const bool idr = type == 5;
		if (idr)
		{
			unsigned idrPicId = 0;
			if (!bits.UE(idrPicId))
				return false;
		}

		unsigned pocLsb = 0;
		if (!bits.Bits(state.log2MaxPicOrderCntLsb, pocLsb))
			return false;

		/* 8.2.1.1. An IDR restarts the count, which is what keeps this from
		   drifting across the whole film. */
		const int maxPocLsb = 1 << state.log2MaxPicOrderCntLsb;

		int prevMsb = 0;
		int prevLsb = 0;
		if (!idr)
		{
			prevMsb = state.prevPicOrderCntMsb;
			prevLsb = state.prevPicOrderCntLsb;
		}

		int msb = 0;
		if (int(pocLsb) < prevLsb && (prevLsb - int(pocLsb)) >= maxPocLsb / 2)
			msb = prevMsb + maxPocLsb;
		else if (int(pocLsb) > prevLsb && (int(pocLsb) - prevLsb) > maxPocLsb / 2)
			msb = prevMsb - maxPocLsb;
		else
			msb = prevMsb;

		poc = msb + int(pocLsb);

		state.prevPicOrderCntMsb = msb;
		state.prevPicOrderCntLsb = int(pocLsb);
		return true;
	}

	return false;
}

}

}
