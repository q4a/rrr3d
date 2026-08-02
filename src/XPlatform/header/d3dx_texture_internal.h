#ifndef RRR3D_D3DX_TEXTURE_INTERNAL_H
#define RRR3D_D3DX_TEXTURE_INTERNAL_H

/*
 * The pixel half of D3DXFilterTexture, exposed so it can be tested.
 *
 * The function itself takes an IDirect3DTexture9 and so needs a device, a
 * window and a GPU -- none of which src/Tests has, deliberately: it runs on a
 * machine with no display. But almost nothing that can go wrong here is in the
 * D3D plumbing. It is in the BCn codec and the box filter, and those are pure
 * functions over buffers.
 *
 * So they are declared here rather than left in the anonymous namespace, and
 * the parts that genuinely need a device -- LockRect, level walking, type
 * dispatch -- stay unexposed and untested by that suite.
 *
 * Not a public header. XPlatform's consumers see xplatform.h and d3dx9.h; this
 * is for src/Tests and for d3dx_texture.cpp itself.
 */

#include "xplatform.h"

namespace rrr3d
{
namespace d3dxtex
{

/*
 * One BCn surface to RGBA8, tightly packed at `width * height * 4`.
 *
 * `pitch` is the source's row stride in bytes, one row being one row of
 * blocks. Dimensions need not be multiples of four: trailing blocks are
 * partial and their surplus texels are not written.
 */
void DecodeSurface(D3DFORMAT format, unsigned width, unsigned height,
	const unsigned char* src, unsigned pitch, unsigned char* dst);

/* The reverse. Partial blocks clamp to the edge rather than reading
   uninitialised texels, which would encode noise into the endpoints. */
void EncodeSurface(D3DFORMAT format, unsigned width, unsigned height,
	const unsigned char* src, unsigned char* dst, unsigned pitch);

/* A 2x2 box, which is what a mip level is. Odd dimensions clamp. */
void BoxFilter(const unsigned char* src, unsigned srcW, unsigned srcH,
	unsigned char* dst, unsigned dstW, unsigned dstH);

}
}

#endif
