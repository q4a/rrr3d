/*
 * D3DX texture loading.
 *
 * D3DX is not part of Direct3D, so no backend choice provides it -- see the
 * note in d3d9_stub.cpp.
 *
 * Scope is the two entry points the engine actually reaches, both in
 * res/D3DXImageFile.cpp:
 *
 *   D3DXCreateTextureFromFileInMemoryEx        all .dds but the skyboxes, and
 *                                              every .png and .jpg
 *   D3DXCreateCubeTextureFromFileInMemoryEx    the 6 skybox cubemaps
 *
 * The game ships 312 .dds (DXT1 200, DXT3 62, DXT5 19, uncompressed 28, of
 * which 6 are cubemaps), 213 .png and 2 .jpg. DDS is parsed here; PNG and JPG
 * go through the vendored stb_image -- see vendor/README.md for why that and
 * not ImageIO.
 *
 * The split is deliberate. Block-compressed data is handed to the GPU still
 * compressed, so decoding it to RGBA the way stb would is exactly the work
 * worth avoiding; and stb does not read DDS anyway.
 *
 * The file-path variants (D3DXCreateTextureFromFileEx, and the cube one) are
 * deliberately left unimplemented, along with D3DXGetImageInfoFromFileW. Those
 * three form D3DX's "fast path" in VideoResource.cpp: it probes with
 * GetImageInfo and, only if that succeeds, hands the filename to D3DX and lets
 * it resize, filter and build the mip chain. Leaving the probe failing routes
 * every texture down the other branch instead -- read the file, decode to
 * system memory, let the engine call CreateTexture itself -- which is one code
 * path rather than two, and avoids owing D3DX's resampling and mip generation.
 * That branch is not a fallback bolted on for us; it is what the engine already
 * uses whenever a texture is generated rather than loaded.
 *
 * So the textures made here are staging buffers. The caller locks one, copies
 * the bits into its own Resource, and releases it; the GPU texture is created
 * separately by the engine. D3DPOOL_SCRATCH, which is what the caller asks for,
 * says exactly that -- system memory, never bound to the device -- so no format
 * needs Metal support to get through this file.
 */

#include "xplatform.h"
#include "directx/d3dx9.h"

#include <cstdio>
#include <cstring>

/*
 * Only the decoders this game's data needs. The rest are switched off so the
 * unused ones are not compiled in at all -- configuration, not modification;
 * the header itself is unchanged. See vendor/README.md.
 */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_GIF
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_HDR
#define STBI_NO_TGA
#include "../vendor/stb_image.h"

namespace
{

/*
 * The DDS header, as it appears on disk after the four-byte magic.
 *
 * No packing pragma: every member is a uint32_t, so the natural layout is
 * already the file layout. The asserts below are what actually holds that,
 * rather than a pragma that would look like it was doing more than it is.
 */
struct DdsPixelFormat
{
	uint32_t size;
	uint32_t flags;
	uint32_t fourCC;
	uint32_t rgbBitCount;
	uint32_t rBitMask;
	uint32_t gBitMask;
	uint32_t bBitMask;
	uint32_t aBitMask;
};

struct DdsHeader
{
	uint32_t size;
	uint32_t flags;
	uint32_t height;
	uint32_t width;
	uint32_t pitchOrLinearSize;
	uint32_t depth;
	uint32_t mipMapCount;
	uint32_t reserved1[11];
	DdsPixelFormat ddspf;
	uint32_t caps;
	uint32_t caps2;
	uint32_t caps3;
	uint32_t caps4;
	uint32_t reserved2;
};

static_assert(sizeof(DdsPixelFormat) == 32, "DDS_PIXELFORMAT is 32 bytes on disk");
static_assert(sizeof(DdsHeader) == 124, "DDS_HEADER is 124 bytes on disk");

const uint32_t cDdsMagic = 0x20534444;      /* "DDS ", little-endian */

const uint32_t cDdpfAlphaPixels = 0x1;
const uint32_t cDdpfFourCC      = 0x4;
const uint32_t cDdpfRgb         = 0x40;

const uint32_t cDdsCaps2Cubemap = 0x200;

/* Cube faces in the order DDS stores them, which is the D3DCUBEMAP_FACES order. */
const unsigned cCubeFaceCnt = 6;

inline uint32_t MakeFourCC(char a, char b, char c, char d)
{
	return static_cast<uint32_t>(static_cast<unsigned char>(a))
	     | (static_cast<uint32_t>(static_cast<unsigned char>(b)) << 8)
	     | (static_cast<uint32_t>(static_cast<unsigned char>(c)) << 16)
	     | (static_cast<uint32_t>(static_cast<unsigned char>(d)) << 24);
}

/* The D3DFORMAT this pixel format describes, or D3DFMT_UNKNOWN if unhandled. */
D3DFORMAT FormatFromDds(const DdsPixelFormat& pf)
{
	if (pf.flags & cDdpfFourCC)
	{
		if (pf.fourCC == MakeFourCC('D', 'X', 'T', '1')) return D3DFMT_DXT1;
		if (pf.fourCC == MakeFourCC('D', 'X', 'T', '2')) return D3DFMT_DXT2;
		if (pf.fourCC == MakeFourCC('D', 'X', 'T', '3')) return D3DFMT_DXT3;
		if (pf.fourCC == MakeFourCC('D', 'X', 'T', '4')) return D3DFMT_DXT4;
		if (pf.fourCC == MakeFourCC('D', 'X', 'T', '5')) return D3DFMT_DXT5;
		return D3DFMT_UNKNOWN;
	}

	if (pf.flags & cDdpfRgb)
	{
		/* Matched on the channel masks, not the bit count: A8R8G8B8 and
		 * X8R8G8B8 are both 32-bit and differ only in the alpha mask. */
		if (pf.rgbBitCount == 32)
		{
			if (pf.rBitMask == 0x00ff0000 && pf.aBitMask == 0xff000000) return D3DFMT_A8R8G8B8;
			if (pf.rBitMask == 0x00ff0000)                              return D3DFMT_X8R8G8B8;
			if (pf.rBitMask == 0x000000ff && pf.aBitMask == 0xff000000) return D3DFMT_A8B8G8R8;
		}
		if (pf.rgbBitCount == 24 && pf.rBitMask == 0x00ff0000)
			return D3DFMT_R8G8B8;
		if (pf.rgbBitCount == 16)
		{
			if (pf.aBitMask == 0x8000)  return D3DFMT_A1R5G5B5;
			if (pf.aBitMask == 0xf000)  return D3DFMT_A4R4G4B4;
			if (pf.gBitMask == 0x07e0)  return D3DFMT_R5G6B5;
			if (pf.gBitMask == 0x03e0)  return D3DFMT_X1R5G5B5;
		}
	}

	if ((pf.flags & cDdpfAlphaPixels) && !(pf.flags & cDdpfRgb) && pf.rgbBitCount == 8)
		return D3DFMT_A8;

	return D3DFMT_UNKNOWN;
}

bool IsBlockCompressed(D3DFORMAT format)
{
	return format == D3DFMT_DXT1 || format == D3DFMT_DXT2 || format == D3DFMT_DXT3
	    || format == D3DFMT_DXT4 || format == D3DFMT_DXT5;
}

unsigned BytesPerPixel(D3DFORMAT format)
{
	switch (format)
	{
	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8:
	case D3DFMT_A8B8G8R8:
		return 4;

	case D3DFMT_R8G8B8:
		return 3;

	case D3DFMT_A1R5G5B5:
	case D3DFMT_X1R5G5B5:
	case D3DFMT_A4R4G4B4:
	case D3DFMT_R5G6B5:
		return 2;

	case D3DFMT_A8:
		return 1;

	default:
		return 0;
	}
}

/* Bytes one row of the source occupies: a row of pixels, or a row of 4x4 blocks. */
unsigned SrcPitch(D3DFORMAT format, unsigned width)
{
	if (IsBlockCompressed(format))
		return (width + 3) / 4 * (format == D3DFMT_DXT1 ? 8 : 16);

	return width * BytesPerPixel(format);
}

/* How many of those rows a level has. */
unsigned SrcRowCnt(D3DFORMAT format, unsigned height)
{
	return IsBlockCompressed(format) ? (height + 3) / 4 : height;
}

unsigned LevelSize(D3DFORMAT format, unsigned width, unsigned height)
{
	return SrcPitch(format, width) * SrcRowCnt(format, height);
}

/*
 * Bytes one whole mip chain occupies, which is what separates one cube face
 * from the next -- DDS stores cubemaps face-major, each face carrying its own
 * complete chain.
 */
unsigned ChainSize(D3DFORMAT format, unsigned width, unsigned height, unsigned levels)
{
	unsigned total = 0;
	for (unsigned i = 0; i < levels; ++i)
	{
		total += LevelSize(format, width, height);
		width = width > 1 ? width / 2 : 1;
		height = height > 1 ? height / 2 : 1;
	}
	return total;
}

/*
 * One level copied row by row, respecting the destination pitch. A DDS file
 * never pads its rows and a locked surface generally does, so copying the level
 * in one memcpy is wrong wherever the two differ.
 */
void CopyLevel(D3DFORMAT format, unsigned width, unsigned height, const char* src, const D3DLOCKED_RECT& dst)
{
	const unsigned pitch = SrcPitch(format, width);
	const unsigned rows = SrcRowCnt(format, height);
	char* out = static_cast<char*>(dst.pBits);

	for (unsigned row = 0; row < rows; ++row)
		std::memcpy(out + row * dst.Pitch, src + row * pitch, pitch);
}

/* Once per unhandled format, so an unloadable texture says why rather than asserting blind. */
void ReportFormat(const DdsPixelFormat& pf)
{
	std::fprintf(stderr,
		"rrr3d: unsupported DDS pixel format -- flags 0x%x, fourCC 0x%x, %u bpp.\n",
		pf.flags, pf.fourCC, pf.rgbBitCount);
}

/* The four-byte magic only, so "not a DDS" is distinguishable from "a DDS this cannot read". */
bool IsDds(const void* srcData, UINT srcDataSize)
{
	if (!srcData || srcDataSize < 4)
		return false;

	uint32_t magic = 0;
	std::memcpy(&magic, srcData, sizeof(magic));
	return magic == cDdsMagic;
}

/*
 * The header, validated. Returns false and leaves nothing set if the data is
 * not a DDS this file can read.
 */
bool ReadHeader(const void* srcData, UINT srcDataSize, DdsHeader& header, D3DFORMAT& format, const char*& bits)
{
	if (!srcData || srcDataSize < 4 + sizeof(DdsHeader))
		return false;

	const char* data = static_cast<const char*>(srcData);

	uint32_t magic = 0;
	std::memcpy(&magic, data, sizeof(magic));
	if (magic != cDdsMagic)
		return false;

	std::memcpy(&header, data + 4, sizeof(header));
	if (header.size != sizeof(DdsHeader) || header.width == 0 || header.height == 0)
	{
		std::fprintf(stderr, "rrr3d: bad DDS header -- size %u, %ux%u\n",
			header.size, header.width, header.height);
		return false;
	}

	format = FormatFromDds(header.ddspf);
	if (format == D3DFMT_UNKNOWN)
	{
		ReportFormat(header.ddspf);
		return false;
	}

	bits = data + 4 + sizeof(header);
	return true;
}

/*
 * How many levels to produce. D3DX_FROM_FILE and D3DX_DEFAULT both mean "what
 * the file has"; anything else is a cap, since levels cannot be invented
 * without generating them. Both callers here ask for 1.
 */
unsigned LevelCnt(UINT requested, const DdsHeader& header)
{
	const unsigned fileLevels = header.mipMapCount ? header.mipMapCount : 1;

	if (requested == 0 || requested == D3DX_FROM_FILE || requested == D3DX_DEFAULT)
		return fileLevels;

	return requested < fileLevels ? requested : fileLevels;
}

/*
 * PNG and JPG, through stb_image.
 *
 * Always decoded to four channels and handed over as D3DFMT_A8R8G8B8 -- the
 * engine reads info.Format to size its own copy, so a texture that varies with
 * whether the source had an alpha channel would make every caller guess. stb
 * gives RGBA; D3D9's A8R8G8B8 is BGRA in memory, so red and blue are swapped
 * on the way in.
 *
 * One mip level only. These are interface and decal art; the callers ask for
 * one level, and generating a chain is D3DX work this deliberately does not do.
 */
HRESULT LoadStbImage(IDirect3DDevice9* device, const void* data, UINT dataSize,
	D3DPOOL pool, D3DXIMAGE_INFO* info, IDirect3DTexture9** texture)
{
	int width = 0;
	int height = 0;
	int channels = 0;

	stbi_uc* pixels = stbi_load_from_memory(static_cast<const stbi_uc*>(data),
		static_cast<int>(dataSize), &width, &height, &channels, 4);

	if (!pixels)
	{
		std::fprintf(stderr, "rrr3d: stb_image failed: %s\n", stbi_failure_reason());
		return D3DXERR_INVALIDDATA;
	}

	HRESULT hr = device->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, pool, texture, NULL);
	if (FAILED(hr))
	{
		stbi_image_free(pixels);
		return hr;
	}

	D3DLOCKED_RECT rect;
	hr = (*texture)->LockRect(0, &rect, NULL, 0);
	if (FAILED(hr))
	{
		stbi_image_free(pixels);
		(*texture)->Release();
		*texture = NULL;
		return hr;
	}

	for (int y = 0; y < height; ++y)
	{
		const stbi_uc* src = pixels + static_cast<size_t>(y) * width * 4;
		unsigned char* dst = static_cast<unsigned char*>(rect.pBits) + static_cast<size_t>(y) * rect.Pitch;

		for (int x = 0; x < width; ++x)
		{
			dst[x * 4 + 0] = src[x * 4 + 2];   /* B */
			dst[x * 4 + 1] = src[x * 4 + 1];   /* G */
			dst[x * 4 + 2] = src[x * 4 + 0];   /* R */
			dst[x * 4 + 3] = src[x * 4 + 3];   /* A */
		}
	}

	(*texture)->UnlockRect(0);
	stbi_image_free(pixels);

	if (info)
	{
		std::memset(info, 0, sizeof(*info));
		info->Width = width;
		info->Height = height;
		info->Depth = 1;
		info->MipLevels = 1;
		info->Format = D3DFMT_A8R8G8B8;
		info->ResourceType = D3DRTYPE_TEXTURE;
		info->ImageFileFormat = D3DXIFF_PNG;
	}

	return D3D_OK;
}

void FillInfo(D3DXIMAGE_INFO* info, const DdsHeader& header, D3DFORMAT format, unsigned levels, D3DRESOURCETYPE type)
{
	if (!info)
		return;

	std::memset(info, 0, sizeof(*info));
	info->Width = header.width;
	info->Height = header.height;
	info->Depth = 1;
	info->MipLevels = levels;
	info->Format = format;
	info->ResourceType = type;
	info->ImageFileFormat = D3DXIFF_DDS;
}

}

HRESULT WINAPI D3DXCreateTextureFromFileInMemoryEx(IDirect3DDevice9* device, const void* srcData,
	UINT srcDataSize, UINT width, UINT height, UINT mipLevels, DWORD usage, D3DFORMAT format,
	D3DPOOL pool, DWORD filter, DWORD mipFilter, D3DCOLOR colorKey,
	D3DXIMAGE_INFO* srcInfo, PALETTEENTRY* palette, IDirect3DTexture9** texture)
{
	/*
	 * width, height, format, filter, mipFilter and colorKey are all ignored:
	 * honouring them would mean resampling and format conversion, and the
	 * callers pass D3DX_FROM_FILE, D3DFMT_UNKNOWN and D3DX_DEFAULT, which say
	 * "whatever the file is". If a caller ever asks for something else it will
	 * silently get the file's dimensions instead, so this is worth revisiting
	 * before any new call site is added.
	 */
	if (!device || !texture)
		return D3DERR_INVALIDCALL;

	*texture = NULL;

	/* The same entry point serves .dds, .png and .jpg; the data says which. */
	if (!IsDds(srcData, srcDataSize))
		return LoadStbImage(device, srcData, srcDataSize, pool, srcInfo, texture);

	DdsHeader header;
	D3DFORMAT ddsFormat = D3DFMT_UNKNOWN;
	const char* bits = NULL;
	if (!ReadHeader(srcData, srcDataSize, header, ddsFormat, bits))
		return D3DXERR_INVALIDDATA;

	const unsigned levels = LevelCnt(mipLevels, header);
	const char* end = static_cast<const char*>(srcData) + srcDataSize;
	const unsigned needed = ChainSize(ddsFormat, header.width, header.height, levels);
	if (bits + needed > end)
	{
		std::fprintf(stderr, "rrr3d: truncated DDS -- %ux%u fmt %d, %u levels need %u bytes, have %ld\n",
			header.width, header.height, static_cast<int>(ddsFormat), levels, needed,
			static_cast<long>(end - bits));
		return D3DXERR_INVALIDDATA;
	}

	HRESULT hr = device->CreateTexture(header.width, header.height, levels, usage, ddsFormat, pool, texture, NULL);
	if (FAILED(hr))
		return hr;

	unsigned levelWidth = header.width;
	unsigned levelHeight = header.height;

	for (unsigned i = 0; i < levels; ++i)
	{
		D3DLOCKED_RECT rect;
		hr = (*texture)->LockRect(i, &rect, NULL, 0);
		if (FAILED(hr))
		{
			(*texture)->Release();
			*texture = NULL;
			return hr;
		}

		CopyLevel(ddsFormat, levelWidth, levelHeight, bits, rect);
		(*texture)->UnlockRect(i);

		bits += LevelSize(ddsFormat, levelWidth, levelHeight);
		levelWidth = levelWidth > 1 ? levelWidth / 2 : 1;
		levelHeight = levelHeight > 1 ? levelHeight / 2 : 1;
	}

	FillInfo(srcInfo, header, ddsFormat, levels, D3DRTYPE_TEXTURE);
	return D3D_OK;
}

HRESULT WINAPI D3DXCreateCubeTextureFromFileInMemoryEx(IDirect3DDevice9* device, const void* srcData,
	UINT srcDataSize, UINT size, UINT mipLevels, DWORD usage, D3DFORMAT format,
	D3DPOOL pool, DWORD filter, DWORD mipFilter, D3DCOLOR colorKey,
	D3DXIMAGE_INFO* srcInfo, PALETTEENTRY* palette, IDirect3DCubeTexture9** texture)
{
	if (!device || !texture)
		return D3DERR_INVALIDCALL;

	*texture = NULL;

	DdsHeader header;
	D3DFORMAT ddsFormat = D3DFMT_UNKNOWN;
	const char* bits = NULL;
	if (!ReadHeader(srcData, srcDataSize, header, ddsFormat, bits))
		return D3DXERR_INVALIDDATA;

	if (!(header.caps2 & cDdsCaps2Cubemap) || header.width != header.height)
		return D3DXERR_INVALIDDATA;

	const unsigned levels = LevelCnt(mipLevels, header);

	/*
	 * Faces are stored one whole mip chain after another, so the stride is the
	 * file's full chain -- not the truncated one being read. Getting this from
	 * `levels` would land inside face 0's mips for every face after the first.
	 */
	const unsigned fileLevels = header.mipMapCount ? header.mipMapCount : 1;
	const unsigned faceStride = ChainSize(ddsFormat, header.width, header.height, fileLevels);

	const char* end = static_cast<const char*>(srcData) + srcDataSize;
	if (bits + faceStride * cCubeFaceCnt > end)
		return D3DXERR_INVALIDDATA;

	HRESULT hr = device->CreateCubeTexture(header.width, levels, usage, ddsFormat, pool, texture, NULL);
	if (FAILED(hr))
		return hr;

	for (unsigned face = 0; face < cCubeFaceCnt; ++face)
	{
		const char* level = bits + face * faceStride;
		unsigned levelSize = header.width;

		for (unsigned i = 0; i < levels; ++i)
		{
			D3DLOCKED_RECT rect;
			hr = (*texture)->LockRect(static_cast<D3DCUBEMAP_FACES>(face), i, &rect, NULL, 0);
			if (FAILED(hr))
			{
				(*texture)->Release();
				*texture = NULL;
				return hr;
			}

			CopyLevel(ddsFormat, levelSize, levelSize, level, rect);
			(*texture)->UnlockRect(static_cast<D3DCUBEMAP_FACES>(face), i);

			level += LevelSize(ddsFormat, levelSize, levelSize);
			levelSize = levelSize > 1 ? levelSize / 2 : 1;
		}
	}

	/* Width is the face edge; the caller multiplies by six for its own atlas. */
	FillInfo(srcInfo, header, ddsFormat, levels, D3DRTYPE_CUBETEXTURE);
	return D3D_OK;
}
