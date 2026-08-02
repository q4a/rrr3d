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
#include "d3dx_texture_internal.h"

#include <cstdio>
#include <cstring>
#include <vector>

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

/* The BC1/BC3 encoder behind D3DXFilterTexture, at the bottom of this file. */
#define STB_DXT_IMPLEMENTATION
#include "../vendor/stb_dxt.h"

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

	{
		unsigned translucent = 0;
		for (int i = 0; i < width * height; ++i)
			if (pixels[i * 4 + 3] != 255)
				++translucent;
		RRR3D_TRACE_FIRST(12, "PNG %dx%d channels=%d translucent=%u/%d",
			width, height, channels, translucent, width * height);
	}

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

/* ------------------------------------------------------- D3DXFilterTexture --
 *
 * Mip generation, for the one shape the engine actually asks for.
 *
 * VideoResource.cpp:732 sets
 *
 *     manualFilter = GetLevelCnt() != 1 && (GetUsage() & D3DUSAGE_AUTOGENMIPMAP)
 *                    && _data->IsCompressed()
 *
 * and Tex2DResource::DoInit strips D3DUSAGE_AUTOGENMIPMAP from exactly the
 * block-compressed formats, because D3D9 drivers cannot autogenerate mips for
 * them. So this is never called on a render target and never on an uncompressed
 * surface: it is always a D3DPOOL_SYSTEMMEM, usage-0, DXT1/3/5 texture whose
 * level 0 has just been filled from file data and whose levels 1..N are
 * untouched. UpdateTexture then copies the whole chain to the real texture.
 *
 * That "untouched" matters more than the old note in REMAINING_WORK.md
 * suggested. An all-zero DXT1 block decodes to solid black and an all-zero
 * DXT3/DXT5 block to black and fully transparent, so the symptom of not
 * implementing this is surfaces going black with distance -- on 201 of the
 * game's textures -- rather than aliasing.
 *
 * DXT cannot be filtered in the compressed domain, so each level is produced by
 * decoding its parent to RGBA, box filtering, and re-encoding. The decode is
 * ours (stb has none); the encode is stb_dxt.
 *
 * Uncompressed formats are handled too, because the API permits them and a
 * caller that hits one should get mips rather than silence -- it is the same
 * box filter without the codec on either side.
 */

namespace
{

void DecodeColorBlock(const unsigned char* block, unsigned char rgba[16][4], bool dxt1)
{
	const unsigned c0 = unsigned(block[0]) | (unsigned(block[1]) << 8);
	const unsigned c1 = unsigned(block[2]) | (unsigned(block[3]) << 8);

	unsigned char c[4][4];
	for (int i = 0; i < 2; ++i)
	{
		const unsigned v = i ? c1 : c0;
		/* 5:6:5, expanded by replicating the high bits into the low ones --
		   which is what the hardware does, and what makes 31 -> 255 rather
		   than 248. */
		c[i][0] = static_cast<unsigned char>(((v >> 11) & 0x1F) * 255 / 31);
		c[i][1] = static_cast<unsigned char>(((v >> 5) & 0x3F) * 255 / 63);
		c[i][2] = static_cast<unsigned char>((v & 0x1F) * 255 / 31);
		c[i][3] = 255;
	}

	/* c0 <= c1 selects the punch-through mode, and only DXT1 has it: with an
	   explicit alpha block the comparison is meaningless and the four-colour
	   interpolation is always used. */
	const bool punchThrough = dxt1 && c0 <= c1;

	for (int i = 0; i < 3; ++i)
	{
		if (punchThrough)
		{
			c[2][i] = static_cast<unsigned char>((int(c[0][i]) + int(c[1][i])) / 2);
			c[3][i] = 0;
		}
		else
		{
			c[2][i] = static_cast<unsigned char>((2 * int(c[0][i]) + int(c[1][i])) / 3);
			c[3][i] = static_cast<unsigned char>((int(c[0][i]) + 2 * int(c[1][i])) / 3);
		}
	}
	c[2][3] = 255;
	c[3][3] = punchThrough ? 0 : 255;

	for (int p = 0; p < 16; ++p)
	{
		const unsigned index = (unsigned(block[4 + (p >> 2)]) >> ((p & 3) * 2)) & 3;
		for (int i = 0; i < 4; ++i)
			rgba[p][i] = c[index][i];
	}
}

void DecodeDxt3Alpha(const unsigned char* block, unsigned char rgba[16][4])
{
	for (int p = 0; p < 16; ++p)
	{
		const unsigned nibble = (unsigned(block[p >> 1]) >> ((p & 1) * 4)) & 0xF;
		rgba[p][3] = static_cast<unsigned char>(nibble * 255 / 15);
	}
}

void DecodeDxt5Alpha(const unsigned char* block, unsigned char rgba[16][4])
{
	unsigned char a[8];
	a[0] = block[0];
	a[1] = block[1];

	if (a[0] > a[1])
	{
		for (int i = 0; i < 6; ++i)
			a[2 + i] = static_cast<unsigned char>(((6 - i) * int(a[0]) + (1 + i) * int(a[1])) / 7);
	}
	else
	{
		for (int i = 0; i < 4; ++i)
			a[2 + i] = static_cast<unsigned char>(((4 - i) * int(a[0]) + (1 + i) * int(a[1])) / 5);
		a[6] = 0;
		a[7] = 255;
	}

	/* Sixteen 3-bit indices packed into six bytes, low bit first. */
	unsigned long long bits = 0;
	for (int i = 0; i < 6; ++i)
		bits |= static_cast<unsigned long long>(block[2 + i]) << (8 * i);

	for (int p = 0; p < 16; ++p)
		rgba[p][3] = a[(bits >> (3 * p)) & 7];
}

/* One BCn level to RGBA8. Dimensions are the level's, which need not be
   multiples of four -- the trailing blocks are partial and their surplus texels
   are simply not written. */
}  /* anonymous */

namespace rrr3d { namespace d3dxtex {

void DecodeSurface(D3DFORMAT format, unsigned width, unsigned height,
	const unsigned char* src, unsigned pitch, unsigned char* dst)
{
	const bool dxt1 = format == D3DFMT_DXT1;
	const unsigned blockBytes = dxt1 ? 8u : 16u;
	const unsigned blocksX = (width + 3) / 4;
	const unsigned blocksY = (height + 3) / 4;

	for (unsigned by = 0; by < blocksY; ++by)
	{
		const unsigned char* row = src + by * pitch;

		for (unsigned bx = 0; bx < blocksX; ++bx)
		{
			const unsigned char* block = row + bx * blockBytes;
			unsigned char rgba[16][4];

			if (dxt1)
			{
				DecodeColorBlock(block, rgba, true);
			}
			else
			{
				DecodeColorBlock(block + 8, rgba, false);
				if (format == D3DFMT_DXT2 || format == D3DFMT_DXT3)
					DecodeDxt3Alpha(block, rgba);
				else
					DecodeDxt5Alpha(block, rgba);
			}

			for (unsigned py = 0; py < 4; ++py)
			{
				const unsigned y = by * 4 + py;
				if (y >= height)
					break;

				for (unsigned px = 0; px < 4; ++px)
				{
					const unsigned x = bx * 4 + px;
					if (x >= width)
						break;

					unsigned char* out = dst + (y * width + x) * 4;
					for (int i = 0; i < 4; ++i)
						out[i] = rgba[py * 4 + px][i];
				}
			}
		}
	}
}

void EncodeSurface(D3DFORMAT format, unsigned width, unsigned height,
	const unsigned char* src, unsigned char* dst, unsigned pitch)
{
	const bool dxt1 = format == D3DFMT_DXT1;
	const unsigned blockBytes = dxt1 ? 8u : 16u;
	const unsigned blocksX = (width + 3) / 4;
	const unsigned blocksY = (height + 3) / 4;

	for (unsigned by = 0; by < blocksY; ++by)
	{
		unsigned char* row = dst + by * pitch;

		for (unsigned bx = 0; bx < blocksX; ++bx)
		{
			/* A partial block is filled by clamping to the edge rather than
			   left undefined: stb reads all sixteen texels regardless, and
			   uninitialised ones would encode noise into the endpoints. */
			unsigned char block[64];
			for (unsigned py = 0; py < 4; ++py)
			{
				const unsigned y = by * 4 + py < height ? by * 4 + py : height - 1;
				for (unsigned px = 0; px < 4; ++px)
				{
					const unsigned x = bx * 4 + px < width ? bx * 4 + px : width - 1;
					const unsigned char* in = src + (y * width + x) * 4;
					for (int i = 0; i < 4; ++i)
						block[(py * 4 + px) * 4 + i] = in[i];
				}
			}

			unsigned char* out = row + bx * blockBytes;

			if (format == D3DFMT_DXT2 || format == D3DFMT_DXT3)
			{
				/* Explicit 4-bit alpha, which stb does not write: DXT3's alpha
				   block is a straight quantise, so it is done here and stb is
				   asked for the colour half only. */
				for (int p = 0; p < 16; p += 2)
				{
					const unsigned lo = block[p * 4 + 3] * 15 / 255;
					const unsigned hi = block[(p + 1) * 4 + 3] * 15 / 255;
					out[p / 2] = static_cast<unsigned char>(lo | (hi << 4));
				}
				stb_compress_dxt_block(out + 8, block, 0, STB_DXT_HIGHQUAL);
			}
			else
			{
				stb_compress_dxt_block(out, block, dxt1 ? 0 : 1, STB_DXT_HIGHQUAL);
			}
		}
	}
}

/* A 2x2 box, which is what a mip level is. Odd dimensions clamp rather than
   wrap, so the last column or row is averaged with itself. */
void BoxFilter(const unsigned char* src, unsigned srcW, unsigned srcH,
	unsigned char* dst, unsigned dstW, unsigned dstH)
{
	for (unsigned y = 0; y < dstH; ++y)
	{
		const unsigned y0 = y * 2 < srcH ? y * 2 : srcH - 1;
		const unsigned y1 = y * 2 + 1 < srcH ? y * 2 + 1 : y0;

		for (unsigned x = 0; x < dstW; ++x)
		{
			const unsigned x0 = x * 2 < srcW ? x * 2 : srcW - 1;
			const unsigned x1 = x * 2 + 1 < srcW ? x * 2 + 1 : x0;

			const unsigned char* a = src + (y0 * srcW + x0) * 4;
			const unsigned char* b = src + (y0 * srcW + x1) * 4;
			const unsigned char* c = src + (y1 * srcW + x0) * 4;
			const unsigned char* d = src + (y1 * srcW + x1) * 4;

			unsigned char* out = dst + (y * dstW + x) * 4;
			for (int i = 0; i < 4; ++i)
				out[i] = static_cast<unsigned char>(
					(unsigned(a[i]) + unsigned(b[i]) + unsigned(c[i]) + unsigned(d[i]) + 2) / 4);
		}
	}
}

}}  /* rrr3d::d3dxtex */

namespace
{

using rrr3d::d3dxtex::DecodeSurface;
using rrr3d::d3dxtex::EncodeSurface;
using rrr3d::d3dxtex::BoxFilter;

/* IsBlockCompressed is already defined above, for the DDS reader. */

/* Bytes per texel for the uncompressed formats this can meet. Zero means "not
   something to filter", and the caller declines rather than guessing. */
unsigned UncompressedTexelSize(D3DFORMAT format)
{
	switch (format)
	{
	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8:
		return 4;
	default:
		return 0;
	}
}

/* One surface of a texture, filtered from its parent. */
HRESULT FilterLevel(IDirect3DTexture9* texture, UINT level, D3DFORMAT format,
	unsigned parentW, unsigned parentH, unsigned width, unsigned height)
{
	const unsigned texel = UncompressedTexelSize(format);
	const bool compressed = IsBlockCompressed(format);
	if (!compressed && !texel)
		return D3DERR_INVALIDCALL;

	D3DLOCKED_RECT parent;
	HRESULT hr = texture->LockRect(level - 1, &parent, NULL, D3DLOCK_READONLY);
	if (FAILED(hr))
		return hr;

	std::vector<unsigned char> parentRgba(size_t(parentW) * parentH * 4);
	if (compressed)
	{
		DecodeSurface(format, parentW, parentH,
			static_cast<const unsigned char*>(parent.pBits), unsigned(parent.Pitch),
			&parentRgba[0]);
	}
	else
	{
		for (unsigned y = 0; y < parentH; ++y)
			std::memcpy(&parentRgba[size_t(y) * parentW * 4],
				static_cast<const unsigned char*>(parent.pBits) + size_t(y) * parent.Pitch,
				size_t(parentW) * 4);
	}
	texture->UnlockRect(level - 1);

	std::vector<unsigned char> rgba(size_t(width) * height * 4);
	BoxFilter(&parentRgba[0], parentW, parentH, &rgba[0], width, height);

	D3DLOCKED_RECT dst;
	hr = texture->LockRect(level, &dst, NULL, 0);
	if (FAILED(hr))
		return hr;

	if (compressed)
	{
		EncodeSurface(format, width, height, &rgba[0],
			static_cast<unsigned char*>(dst.pBits), unsigned(dst.Pitch));
	}
	else
	{
		for (unsigned y = 0; y < height; ++y)
			std::memcpy(static_cast<unsigned char*>(dst.pBits) + size_t(y) * dst.Pitch,
				&rgba[size_t(y) * width * 4], size_t(width) * 4);
	}
	texture->UnlockRect(level);

	return D3D_OK;
}

}

HRESULT WINAPI D3DXFilterTexture(IDirect3DBaseTexture9* baseTexture,
	const PALETTEENTRY* palette, UINT srcLevel, DWORD filter)
{
	if (!baseTexture)
		return D3DERR_INVALIDCALL;

	/*
	 * D3DXFilterCubeTexture and D3DXFilterVolumeTexture are #defined to this
	 * same symbol (d3dx9tex.h), so the type has to be asked rather than assumed.
	 * Only 2D arrives from this game -- the cube call site at
	 * VideoResource.cpp:998 is unreachable, because ResourceManager.cpp:757
	 * pins cube textures to one level and the guard requires more than one --
	 * so anything else is declined rather than half-implemented.
	 */
	if (baseTexture->GetType() != D3DRTYPE_TEXTURE)
		return D3DERR_INVALIDCALL;

	IDirect3DTexture9* texture = static_cast<IDirect3DTexture9*>(baseTexture);

	const UINT levels = texture->GetLevelCount();
	if (levels <= 1)
		return D3D_OK;

	const UINT first = (srcLevel == D3DX_DEFAULT) ? 0 : srcLevel;
	if (first + 1 >= levels)
		return D3D_OK;

	D3DSURFACE_DESC desc;
	HRESULT hr = texture->GetLevelDesc(first, &desc);
	if (FAILED(hr))
		return hr;

	unsigned parentW = desc.Width;
	unsigned parentH = desc.Height;

	for (UINT level = first + 1; level < levels; ++level)
	{
		D3DSURFACE_DESC levelDesc;
		hr = texture->GetLevelDesc(level, &levelDesc);
		if (FAILED(hr))
			return hr;

		hr = FilterLevel(texture, level, desc.Format, parentW, parentH,
			levelDesc.Width, levelDesc.Height);
		if (FAILED(hr))
			return hr;

		parentW = levelDesc.Width;
		parentH = levelDesc.Height;
	}

	return D3D_OK;
}
