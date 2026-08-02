#pragma once

/*
 * Picture order counts, read out of the H.264 bitstream.
 *
 * This exists because AVI carries no presentation timestamps and VideoToolbox
 * does not invent any: given a sample with an invalid presentation stamp it
 * hands the same invalid stamp back, and frames come out in the order they went
 * in. That order is decode order, and for these films decode order is not
 * display order -- they are coded with three consecutive B-frames, so the P
 * that the B-frames reference is stored *before* them and shown *after*. Played
 * in decode order the picture visibly jumps forward and back.
 *
 * The display order is in the stream itself, as each slice's picture order
 * count. Reading it needs a little of the slice header, which needs a little of
 * the sequence parameter set, and that is all this does.
 *
 * Scope: pic_order_cnt_type 0, which is what these files use and what almost
 * all H.264 uses. Type 2 is defined to have display order equal to decode
 * order, so it needs nothing. Type 1 is rare and is declined rather than
 * guessed at -- the caller then falls back to decode order, which is what it
 * did before this existed.
 */

#include <cstddef>
#include <cstdint>

namespace r3d
{

namespace video
{

struct H264PocState
{
	/* From the SPS. */
	unsigned log2MaxPicOrderCntLsb = 4;
	unsigned log2MaxFrameNum = 4;
	unsigned picOrderCntType = 0;
	bool frameMbsOnly = true;
	bool separateColourPlane = false;
	bool valid = false;

	/* Carried between pictures, per the specification's algorithm. */
	int prevPicOrderCntMsb = 0;
	int prevPicOrderCntLsb = 0;
};

/* Reads what is needed from an SPS NAL unit (without its start code, and
   including the NAL header byte). Returns false if it cannot be parsed. */
bool ParseSpsForPoc(const unsigned char* nal, size_t size, H264PocState& state);

/*
 * The picture order count of an access unit, given the AVCC buffer for it.
 *
 * Updates `state`, which must be the one filled by ParseSpsForPoc and must be
 * carried across the whole stream in decode order. Returns false when the
 * ordering cannot be determined, in which case the caller should fall back to
 * decode order.
 */
bool ComputePoc(const unsigned char* avcc, size_t size, H264PocState& state, int& poc);

}

}
