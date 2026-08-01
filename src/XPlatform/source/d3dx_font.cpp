/*
 * ID3DXFont: the text D3DX would draw.
 *
 * Not a debug-only concern -- every piece of UI text in this game goes through
 * TextFont, which is a thin wrapper over this. Without it the menus render but
 * are unreadable and unnavigable.
 *
 * The engine uses five methods: DrawTextA, DrawTextW, OnLostDevice,
 * OnResetDevice and Release. The other thirteen return E_NOTIMPL or a zeroed
 * answer rather than pretending.
 *
 * Shape:
 *
 *   - Glyphs are rasterised by the vendored stb_truetype, on demand, into a
 *     single 1024x1024 atlas packed in rows. On demand rather than up front
 *     because the game's six localisations span Latin-1 and Cyrillic, and
 *     baking every range for every font size would cost far more atlas than
 *     any one session touches.
 *
 *   - Text is drawn as one DrawPrimitiveUP per string through the fixed
 *     function pipeline, with the atlas modulating the vertex colour. The
 *     engine's own draw path is DrawPrimitiveUP throughout, so this is the
 *     shape the backend is already exercised on.
 *
 *   - Every device state this touches is saved and restored. D3DX does the
 *     same via a state block; the engine draws text in the middle of its own
 *     passes and would otherwise find its shaders and blend state changed
 *     underneath it.
 *
 * DrawTextA takes UTF-8, not a Windows codepage. That follows the rest of the
 * platform layer -- xplatform.h treats CP_ACP as UTF-8 off Windows -- and it is
 * what the localisation loader now produces, having decoded the UTF-16 source
 * files (see ConvertStrUtf16LEToW).
 */

#include "xplatform.h"
#include "directx/d3dx9.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "../vendor/stb_truetype.h"

namespace
{

const int cAtlasSize = 1024;
const int cAtlasPadding = 1;

struct Glyph
{
	float u0, v0, u1, v1;    /* atlas texture coordinates */
	int width, height;       /* bitmap size, pixels */
	int bearingX, bearingY;  /* offset from the pen position to the bitmap corner */
	int advance;             /* pen movement, pixels */
};

/*
 * Where a face name resolves to on disk.
 *
 * Font discovery is the one genuinely platform-specific part of drawing text,
 * and it is confined to this function so the rest compiles anywhere. macOS
 * ships the Microsoft core fonts, so the names this game asks for -- Arial
 * throughout -- resolve directly. CoreText would do this properly, and would
 * also make this file macOS-only; a lookup table is the smaller price.
 */
std::string ResolveFontFile(const std::string& face, bool bold, bool italic)
{
	static const char* cFontDirs[] =
	{
		"/System/Library/Fonts/Supplemental/",
		"/System/Library/Fonts/",
		"/Library/Fonts/",
	};

	std::string name = face.empty() ? "Arial" : face;

	/* Style suffixes, in the order the supplemental fonts name them. */
	std::vector<std::string> candidates;
	if (bold && italic)
		candidates.push_back(name + " Bold Italic");
	if (bold)
		candidates.push_back(name + " Bold");
	if (italic)
		candidates.push_back(name + " Italic");
	candidates.push_back(name);
	/* Last resort: a face that is always present. */
	candidates.push_back("Arial");
	candidates.push_back("Helvetica");

	for (size_t c = 0; c < candidates.size(); ++c)
	{
		for (size_t d = 0; d < sizeof(cFontDirs) / sizeof(cFontDirs[0]); ++d)
		{
			const std::string path = std::string(cFontDirs[d]) + candidates[c] + ".ttf";
			if (std::FILE* file = std::fopen(path.c_str(), "rb"))
			{
				std::fclose(file);
				return path;
			}
		}
	}

	return std::string();
}

/*
 * One UTF-8 code point. Returns the replacement character on a malformed
 * sequence rather than stopping, so one bad byte in a localisation file costs
 * a glyph and not the rest of the string.
 */
unsigned NextUtf8(const char* text, int length, int& pos)
{
	const unsigned char* bytes = reinterpret_cast<const unsigned char*>(text);
	const unsigned char lead = bytes[pos];

	int extra = 0;
	unsigned code = 0;

	if (lead < 0x80)        { code = lead;        extra = 0; }
	else if ((lead & 0xe0) == 0xc0) { code = lead & 0x1f; extra = 1; }
	else if ((lead & 0xf0) == 0xe0) { code = lead & 0x0f; extra = 2; }
	else if ((lead & 0xf8) == 0xf0) { code = lead & 0x07; extra = 3; }
	else { ++pos; return 0xfffd; }

	if (pos + extra >= length)
	{
		pos = length;
		return 0xfffd;
	}

	for (int i = 1; i <= extra; ++i)
	{
		const unsigned char byte = bytes[pos + i];
		if ((byte & 0xc0) != 0x80)
		{
			++pos;
			return 0xfffd;
		}
		code = (code << 6) | (byte & 0x3f);
	}

	pos += extra + 1;
	return code;
}

struct TextVertex
{
	float x, y, z, rhw;
	D3DCOLOR color;
	float u, v;
};

const DWORD cTextFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

class Font: public ID3DXFont
{
public:
	Font(IDirect3DDevice9* device, const D3DXFONT_DESCA& desc)
		: _refCount(1), _device(device), _desc(desc), _atlas(NULL),
		  _penX(cAtlasPadding), _penY(cAtlasPadding), _rowHeight(0),
		  _ascent(0), _descent(0), _lineGap(0), _scale(0.0f)
	{
		_device->AddRef();
	}

	bool Build();

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override
	{
		if (out)
			*out = this;
		AddRef();
		return S_OK;
	}

	ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	ULONG STDMETHODCALLTYPE Release() override
	{
		const ULONG count = --_refCount;
		if (!count)
			delete this;
		return count;
	}

	HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** device) override
	{
		if (!device)
			return D3DERR_INVALIDCALL;
		*device = _device;
		_device->AddRef();
		return D3D_OK;
	}

	HRESULT STDMETHODCALLTYPE GetDescA(D3DXFONT_DESCA* desc) override
	{
		if (!desc)
			return D3DERR_INVALIDCALL;
		*desc = _desc;
		return D3D_OK;
	}

	HRESULT STDMETHODCALLTYPE GetDescW(D3DXFONT_DESCW*) override { return E_NOTIMPL; }
	WINBOOL STDMETHODCALLTYPE GetTextMetricsA(TEXTMETRICA*) override { return FALSE; }
	WINBOOL STDMETHODCALLTYPE GetTextMetricsW(TEXTMETRICW*) override { return FALSE; }
	HDC STDMETHODCALLTYPE GetDC() override { return NULL; }

	HRESULT STDMETHODCALLTYPE GetGlyphData(UINT, IDirect3DTexture9**, RECT*, POINT*) override
	{
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE PreloadCharacters(UINT, UINT) override { return D3D_OK; }
	HRESULT STDMETHODCALLTYPE PreloadGlyphs(UINT, UINT) override { return D3D_OK; }
	HRESULT STDMETHODCALLTYPE PreloadTextA(const char*, INT) override { return D3D_OK; }
	HRESULT STDMETHODCALLTYPE PreloadTextW(const WCHAR*, INT) override { return D3D_OK; }

	INT STDMETHODCALLTYPE DrawTextA(ID3DXSprite* sprite, const char* string, INT count,
		RECT* rect, DWORD format, D3DCOLOR color) override;
	INT STDMETHODCALLTYPE DrawTextW(ID3DXSprite* sprite, const WCHAR* string, INT count,
		RECT* rect, DWORD format, D3DCOLOR color) override;

	/*
	 * The atlas lives in D3DPOOL_MANAGED, so the runtime restores it across a
	 * device reset and there is nothing to release or rebuild here. D3DX's own
	 * font does the same; these exist because the interface has them.
	 */
	HRESULT STDMETHODCALLTYPE OnLostDevice() override { return D3D_OK; }
	HRESULT STDMETHODCALLTYPE OnResetDevice() override { return D3D_OK; }

private:
	~Font();

	const Glyph* GetGlyph(unsigned code);
	INT Draw(const std::vector<unsigned>& codes, RECT* rect, DWORD format, D3DCOLOR color);

	ULONG _refCount;
	IDirect3DDevice9* _device;
	D3DXFONT_DESCA _desc;

	std::vector<unsigned char> _fontData;
	stbtt_fontinfo _fontInfo;

	IDirect3DTexture9* _atlas;
	std::map<unsigned, Glyph> _glyphs;

	int _penX, _penY, _rowHeight;
	int _ascent, _descent, _lineGap;
	float _scale;
};

Font::~Font()
{
	if (_atlas)
		_atlas->Release();
	_device->Release();
}

bool Font::Build()
{
	const bool bold = _desc.Weight >= FW_BOLD;
	const std::string path = ResolveFontFile(_desc.FaceName, bold, _desc.Italic != 0);

	if (path.empty())
	{
		std::fprintf(stderr, "rrr3d: no font file for '%s'\n", _desc.FaceName);
		return false;
	}

	std::FILE* file = std::fopen(path.c_str(), "rb");
	if (!file)
		return false;

	std::fseek(file, 0, SEEK_END);
	const long size = std::ftell(file);
	std::fseek(file, 0, SEEK_SET);

	_fontData.resize(size > 0 ? size : 0);
	const bool read = size > 0 && std::fread(_fontData.data(), 1, size, file) == static_cast<size_t>(size);
	std::fclose(file);

	if (!read || !stbtt_InitFont(&_fontInfo, _fontData.data(), 0))
	{
		std::fprintf(stderr, "rrr3d: cannot parse font %s\n", path.c_str());
		return false;
	}

	/*
	 * D3DXFONT_DESC::Height is a LOGFONT height: negative means the character
	 * height, positive the cell height including internal leading. The engine
	 * passes both -- Engine.cpp computes a negative one from the point size,
	 * the UI passes positive pixels -- so both are handled rather than assumed.
	 */
	const int height = _desc.Height < 0 ? -_desc.Height : _desc.Height;
	_scale = _desc.Height < 0
		? stbtt_ScaleForPixelHeight(&_fontInfo, static_cast<float>(height))
		: stbtt_ScaleForMappingEmToPixels(&_fontInfo, static_cast<float>(height));

	stbtt_GetFontVMetrics(&_fontInfo, &_ascent, &_descent, &_lineGap);

	HRESULT hr = _device->CreateTexture(cAtlasSize, cAtlasSize, 1, 0,
		D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &_atlas, NULL);
	if (FAILED(hr))
	{
		std::fprintf(stderr, "rrr3d: font atlas CreateTexture failed hr=0x%08x\n", hr);
		return false;
	}

	/* Cleared: an unpacked region must not show as stray ink. */
	D3DLOCKED_RECT locked;
	if (SUCCEEDED(_atlas->LockRect(0, &locked, NULL, 0)))
	{
		for (int y = 0; y < cAtlasSize; ++y)
			std::memset(static_cast<unsigned char*>(locked.pBits) + y * locked.Pitch, 0, cAtlasSize * 4);
		_atlas->UnlockRect(0);
	}

	return true;
}

/*
 * A glyph, rasterised and packed on first use.
 *
 * Packing is a simple row allocator: advance along the current row, drop to a
 * new one when it is full. Good enough for text of one size, which is what a
 * font object is, and it never needs to move a glyph already handed out.
 */
const Glyph* Font::GetGlyph(unsigned code)
{
	std::map<unsigned, Glyph>::iterator known = _glyphs.find(code);
	if (known != _glyphs.end())
		return &known->second;

	const int index = stbtt_FindGlyphIndex(&_fontInfo, static_cast<int>(code));

	int advance = 0;
	int bearing = 0;
	stbtt_GetGlyphHMetrics(&_fontInfo, index, &advance, &bearing);

	int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	stbtt_GetGlyphBitmapBox(&_fontInfo, index, _scale, _scale, &x0, &y0, &x1, &y1);

	Glyph glyph;
	glyph.width = x1 - x0;
	glyph.height = y1 - y0;
	glyph.bearingX = x0;
	glyph.bearingY = y0;
	glyph.advance = static_cast<int>(advance * _scale + 0.5f);
	glyph.u0 = glyph.v0 = glyph.u1 = glyph.v1 = 0.0f;

	if (glyph.width > 0 && glyph.height > 0)
	{
		if (_penX + glyph.width + cAtlasPadding > cAtlasSize)
		{
			_penX = cAtlasPadding;
			_penY += _rowHeight + cAtlasPadding;
			_rowHeight = 0;
		}

		if (_penY + glyph.height + cAtlasPadding > cAtlasSize)
		{
			/* Atlas exhausted. Blank rather than wrong, and said once. */
			static bool reported = false;
			if (!reported)
			{
				reported = true;
				std::fprintf(stderr, "rrr3d: font atlas full, further glyphs blank\n");
			}
			glyph.width = glyph.height = 0;
			_glyphs.insert(std::map<unsigned, Glyph>::value_type(code, glyph));
			return &_glyphs.find(code)->second;
		}

		std::vector<unsigned char> coverage(static_cast<size_t>(glyph.width) * glyph.height, 0);
		stbtt_MakeGlyphBitmap(&_fontInfo, coverage.data(), glyph.width, glyph.height,
			glyph.width, _scale, _scale, index);

		RECT area;
		area.left = _penX;
		area.top = _penY;
		area.right = _penX + glyph.width;
		area.bottom = _penY + glyph.height;

		D3DLOCKED_RECT locked;
		if (SUCCEEDED(_atlas->LockRect(0, &locked, &area, 0)))
		{
			for (int y = 0; y < glyph.height; ++y)
			{
				unsigned char* row = static_cast<unsigned char*>(locked.pBits) + y * locked.Pitch;
				for (int x = 0; x < glyph.width; ++x)
				{
					/*
					 * White with coverage in alpha. The vertex colour supplies
					 * the actual colour through MODULATE, so one atlas serves
					 * every colour the UI asks for.
					 */
					const unsigned char a = coverage[static_cast<size_t>(y) * glyph.width + x];
					row[x * 4 + 0] = 255;
					row[x * 4 + 1] = 255;
					row[x * 4 + 2] = 255;
					row[x * 4 + 3] = a;
				}
			}
			_atlas->UnlockRect(0);
		}

		glyph.u0 = static_cast<float>(_penX) / cAtlasSize;
		glyph.v0 = static_cast<float>(_penY) / cAtlasSize;
		glyph.u1 = static_cast<float>(_penX + glyph.width) / cAtlasSize;
		glyph.v1 = static_cast<float>(_penY + glyph.height) / cAtlasSize;

		_penX += glyph.width + cAtlasPadding;
		if (glyph.height > _rowHeight)
			_rowHeight = glyph.height;
	}

	_glyphs.insert(std::map<unsigned, Glyph>::value_type(code, glyph));
	return &_glyphs.find(code)->second;
}

/*
 * Lay the text out and, unless DT_CALCRECT, draw it.
 *
 * Supports the flags the engine passes: DT_CALCRECT, DT_LEFT / DT_CENTER /
 * DT_RIGHT, DT_TOP / DT_VCENTER / DT_BOTTOM, DT_WORDBREAK, DT_SINGLELINE and
 * DT_NOCLIP. Returns the text height, as DrawText does.
 */
INT Font::Draw(const std::vector<unsigned>& codes, RECT* rect, DWORD format, D3DCOLOR color)
{
	if (!rect)
		return 0;

	const int lineHeight = static_cast<int>((_ascent - _descent + _lineGap) * _scale + 0.5f);
	const int baseline = static_cast<int>(_ascent * _scale + 0.5f);
	const int boxWidth = rect->right - rect->left;

	/* Break into lines, honouring explicit newlines and optional word wrap. */
	std::vector<std::vector<unsigned> > lines;
	std::vector<int> widths;

	std::vector<unsigned> line;
	int lineWidth = 0;
	size_t lastSpace = std::string::npos;
	int widthAtSpace = 0;

	for (size_t i = 0; i < codes.size(); ++i)
	{
		const unsigned code = codes[i];

		if (code == '\n')
		{
			lines.push_back(line);
			widths.push_back(lineWidth);
			line.clear();
			lineWidth = 0;
			lastSpace = std::string::npos;
			continue;
		}

		if (code == '\r')
			continue;

		const Glyph* glyph = GetGlyph(code);
		const int advance = glyph ? glyph->advance : 0;

		if ((format & DT_WORDBREAK) && !(format & DT_SINGLELINE) &&
			boxWidth > 0 && lineWidth + advance > boxWidth && !line.empty())
		{
			if (lastSpace != std::string::npos)
			{
				/* Wrap at the last space, carrying the rest to the next line. */
				std::vector<unsigned> carried(line.begin() + lastSpace + 1, line.end());
				line.resize(lastSpace);

				lines.push_back(line);
				widths.push_back(widthAtSpace);

				line = carried;
				lineWidth = 0;
				for (size_t c = 0; c < line.size(); ++c)
				{
					const Glyph* g = GetGlyph(line[c]);
					lineWidth += g ? g->advance : 0;
				}
			}
			else
			{
				lines.push_back(line);
				widths.push_back(lineWidth);
				line.clear();
				lineWidth = 0;
			}
			lastSpace = std::string::npos;
		}

		if (code == ' ')
		{
			lastSpace = line.size();
			widthAtSpace = lineWidth;
		}

		line.push_back(code);
		lineWidth += advance;
	}

	lines.push_back(line);
	widths.push_back(lineWidth);

	int maxWidth = 0;
	for (size_t i = 0; i < widths.size(); ++i)
		if (widths[i] > maxWidth)
			maxWidth = widths[i];

	const int totalHeight = static_cast<int>(lines.size()) * lineHeight;

	if (format & DT_CALCRECT)
	{
		rect->right = rect->left + maxWidth;
		rect->bottom = rect->top + totalHeight;
		return totalHeight;
	}

	/* Vertical placement of the block. */
	int originY = rect->top;
	if (format & DT_VCENTER)
		originY = rect->top + ((rect->bottom - rect->top) - totalHeight) / 2;
	else if (format & DT_BOTTOM)
		originY = rect->bottom - totalHeight;

	std::vector<TextVertex> vertices;
	vertices.reserve(codes.size() * 6);

	for (size_t l = 0; l < lines.size(); ++l)
	{
		int penX = rect->left;
		if (format & DT_CENTER)
			penX = rect->left + (boxWidth - widths[l]) / 2;
		else if (format & DT_RIGHT)
			penX = rect->right - widths[l];

		const int penY = originY + static_cast<int>(l) * lineHeight + baseline;

		for (size_t c = 0; c < lines[l].size(); ++c)
		{
			const Glyph* glyph = GetGlyph(lines[l][c]);
			if (!glyph)
				continue;

			if (glyph->width > 0 && glyph->height > 0)
			{
				/*
				 * The half-pixel shift is the D3D9 rule for screen-space quads:
				 * pixel centres sit at .5, so without it every glyph samples
				 * between texels and comes out blurred.
				 */
				const float x0 = static_cast<float>(penX + glyph->bearingX) - 0.5f;
				const float y0 = static_cast<float>(penY + glyph->bearingY) - 0.5f;
				const float x1 = x0 + glyph->width;
				const float y1 = y0 + glyph->height;

				TextVertex quad[4];
				quad[0].x = x0; quad[0].y = y0; quad[0].u = glyph->u0; quad[0].v = glyph->v0;
				quad[1].x = x1; quad[1].y = y0; quad[1].u = glyph->u1; quad[1].v = glyph->v0;
				quad[2].x = x0; quad[2].y = y1; quad[2].u = glyph->u0; quad[2].v = glyph->v1;
				quad[3].x = x1; quad[3].y = y1; quad[3].u = glyph->u1; quad[3].v = glyph->v1;

				for (int v = 0; v < 4; ++v)
				{
					quad[v].z = 0.0f;
					quad[v].rhw = 1.0f;
					quad[v].color = color;
				}

				vertices.push_back(quad[0]);
				vertices.push_back(quad[1]);
				vertices.push_back(quad[2]);
				vertices.push_back(quad[2]);
				vertices.push_back(quad[1]);
				vertices.push_back(quad[3]);
			}

			penX += glyph->advance;
		}
	}

	if (vertices.empty())
		return totalHeight;

	/*
	 * Saved and restored around the draw. The engine calls DrawText from inside
	 * its own render passes, so anything left changed here corrupts whatever it
	 * was drawing.
	 */
	IDirect3DVertexShader9* savedVertexShader = NULL;
	IDirect3DPixelShader9* savedPixelShader = NULL;
	IDirect3DBaseTexture9* savedTexture = NULL;
	DWORD savedFvf = 0;

	_device->GetVertexShader(&savedVertexShader);
	_device->GetPixelShader(&savedPixelShader);
	_device->GetTexture(0, &savedTexture);
	_device->GetFVF(&savedFvf);

	static const D3DRENDERSTATETYPE cSavedStates[] =
	{
		D3DRS_ALPHABLENDENABLE, D3DRS_SRCBLEND, D3DRS_DESTBLEND,
		D3DRS_ZENABLE, D3DRS_ZWRITEENABLE, D3DRS_CULLMODE,
		D3DRS_ALPHATESTENABLE, D3DRS_LIGHTING, D3DRS_FOGENABLE,
	};
	const unsigned cSavedStateCnt = sizeof(cSavedStates) / sizeof(cSavedStates[0]);

	DWORD savedValues[cSavedStateCnt];
	for (unsigned i = 0; i < cSavedStateCnt; ++i)
	{
		savedValues[i] = 0;
		_device->GetRenderState(cSavedStates[i], &savedValues[i]);
	}

	_device->SetVertexShader(NULL);
	_device->SetPixelShader(NULL);
	_device->SetFVF(cTextFvf);
	_device->SetTexture(0, _atlas);

	_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	_device->SetRenderState(D3DRS_ZENABLE, FALSE);
	_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	_device->SetRenderState(D3DRS_LIGHTING, FALSE);
	_device->SetRenderState(D3DRS_FOGENABLE, FALSE);

	/* Atlas alpha times vertex colour: one white atlas, any text colour. */
	_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	_device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	_device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

	_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,
		static_cast<UINT>(vertices.size() / 3), vertices.data(), sizeof(TextVertex));

	for (unsigned i = 0; i < cSavedStateCnt; ++i)
		_device->SetRenderState(cSavedStates[i], savedValues[i]);

	_device->SetTexture(0, savedTexture);
	_device->SetFVF(savedFvf);
	_device->SetVertexShader(savedVertexShader);
	_device->SetPixelShader(savedPixelShader);

	if (savedTexture)
		savedTexture->Release();
	if (savedVertexShader)
		savedVertexShader->Release();
	if (savedPixelShader)
		savedPixelShader->Release();

	return totalHeight;
}

INT STDMETHODCALLTYPE Font::DrawTextA(ID3DXSprite*, const char* string, INT count,
	RECT* rect, DWORD format, D3DCOLOR color)
{
	if (!string || !rect)
		return 0;

	const int length = count < 0 ? static_cast<int>(std::strlen(string)) : count;

	std::vector<unsigned> codes;
	codes.reserve(length);
	for (int pos = 0; pos < length; )
		codes.push_back(NextUtf8(string, length, pos));

	return Draw(codes, rect, format, color);
}

INT STDMETHODCALLTYPE Font::DrawTextW(ID3DXSprite*, const WCHAR* string, INT count,
	RECT* rect, DWORD format, D3DCOLOR color)
{
	if (!string || !rect)
		return 0;

	int length = count;
	if (length < 0)
	{
		length = 0;
		while (string[length])
			++length;
	}

	std::vector<unsigned> codes;
	codes.reserve(length);
	for (int i = 0; i < length; ++i)
		codes.push_back(static_cast<unsigned>(string[i]));

	return Draw(codes, rect, format, color);
}

}

HRESULT WINAPI D3DXCreateFontA(IDirect3DDevice9* device, INT height, UINT width, UINT weight,
	UINT mipLevels, BOOL italic, DWORD charSet, DWORD outputPrecision, DWORD quality,
	DWORD pitchAndFamily, const char* faceName, ID3DXFont** font)
{
	if (!device || !font)
		return D3DERR_INVALIDCALL;

	*font = NULL;

	D3DXFONT_DESCA desc;
	std::memset(&desc, 0, sizeof(desc));
	desc.Height = height;
	desc.Width = width;
	desc.Weight = weight;
	desc.MipLevels = mipLevels;
	desc.Italic = italic;
	desc.CharSet = static_cast<BYTE>(charSet);
	desc.OutputPrecision = static_cast<BYTE>(outputPrecision);
	desc.Quality = static_cast<BYTE>(quality);
	desc.PitchAndFamily = static_cast<BYTE>(pitchAndFamily);
	std::snprintf(desc.FaceName, sizeof(desc.FaceName), "%s", faceName ? faceName : "Arial");

	Font* built = new Font(device, desc);
	if (!built->Build())
	{
		built->Release();
		return E_FAIL;
	}

	*font = built;
	return D3D_OK;
}
