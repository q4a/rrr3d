#ifndef XPLATFORM_WINGDI_H
#define XPLATFORM_WINGDI_H

/*
 * The slice of <wingdi.h> the D3DX font interfaces need.
 *
 * ID3DXFont is a GDI-flavoured API: D3DXCreateFont takes GDI's weight, charset,
 * precision, quality and pitch constants, and ID3DXFont::GetTextMetrics hands
 * back a TEXTMETRIC. None of that implies GDI actually exists here -- these are
 * the shapes and values the interface is declared in terms of, and the font
 * implementation this port supplies later reads them.
 *
 * Values are GDI's, because they are what the engine passes and what a
 * reimplementation has to interpret.
 */

#include "xplatform.h"

#define LF_FACESIZE     32
#define LF_FULLFACESIZE 64

/* Weights. */
#define FW_DONTCARE   0
#define FW_THIN       100
#define FW_NORMAL     400
#define FW_MEDIUM     500
#define FW_SEMIBOLD   600
#define FW_BOLD       700
#define FW_HEAVY      900

/* Charsets. */
#define ANSI_CHARSET        0
#define DEFAULT_CHARSET     1
#define SYMBOL_CHARSET      2
#define RUSSIAN_CHARSET     204
#define EASTEUROPE_CHARSET  238
#define BALTIC_CHARSET      186
#define OEM_CHARSET         255

/* Output precision. */
#define OUT_DEFAULT_PRECIS      0
#define OUT_TT_PRECIS           4
#define OUT_TT_ONLY_PRECIS      7

/* Clipping precision. */
#define CLIP_DEFAULT_PRECIS     0

/* Quality. */
#define DEFAULT_QUALITY         0
#define DRAFT_QUALITY           1
#define PROOF_QUALITY           2
#define ANTIALIASED_QUALITY     4
#define CLEARTYPE_QUALITY       5

/* Pitch, in the low two bits; family in the high nibble. */
#define DEFAULT_PITCH   0
#define FIXED_PITCH     1
#define VARIABLE_PITCH  2

#define FF_DONTCARE     (0 << 4)
#define FF_ROMAN        (1 << 4)
#define FF_SWISS        (2 << 4)
#define FF_MODERN       (3 << 4)

typedef struct tagLOGFONTA
{
	LONG lfHeight;
	LONG lfWidth;
	LONG lfEscapement;
	LONG lfOrientation;
	LONG lfWeight;
	BYTE lfItalic;
	BYTE lfUnderline;
	BYTE lfStrikeOut;
	BYTE lfCharSet;
	BYTE lfOutPrecision;
	BYTE lfClipPrecision;
	BYTE lfQuality;
	BYTE lfPitchAndFamily;
	CHAR lfFaceName[LF_FACESIZE];
} LOGFONTA, *PLOGFONTA, *LPLOGFONTA;

typedef struct tagLOGFONTW
{
	LONG  lfHeight;
	LONG  lfWidth;
	LONG  lfEscapement;
	LONG  lfOrientation;
	LONG  lfWeight;
	BYTE  lfItalic;
	BYTE  lfUnderline;
	BYTE  lfStrikeOut;
	BYTE  lfCharSet;
	BYTE  lfOutPrecision;
	BYTE  lfClipPrecision;
	BYTE  lfQuality;
	BYTE  lfPitchAndFamily;
	WCHAR lfFaceName[LF_FACESIZE];
} LOGFONTW, *PLOGFONTW, *LPLOGFONTW;

typedef struct tagTEXTMETRICA
{
	LONG tmHeight;
	LONG tmAscent;
	LONG tmDescent;
	LONG tmInternalLeading;
	LONG tmExternalLeading;
	LONG tmAveCharWidth;
	LONG tmMaxCharWidth;
	LONG tmWeight;
	LONG tmOverhang;
	LONG tmDigitizedAspectX;
	LONG tmDigitizedAspectY;
	BYTE tmFirstChar;
	BYTE tmLastChar;
	BYTE tmDefaultChar;
	BYTE tmBreakChar;
	BYTE tmItalic;
	BYTE tmUnderlined;
	BYTE tmStruckOut;
	BYTE tmPitchAndFamily;
	BYTE tmCharSet;
} TEXTMETRICA, *PTEXTMETRICA, *LPTEXTMETRICA;

typedef struct tagTEXTMETRICW
{
	LONG  tmHeight;
	LONG  tmAscent;
	LONG  tmDescent;
	LONG  tmInternalLeading;
	LONG  tmExternalLeading;
	LONG  tmAveCharWidth;
	LONG  tmMaxCharWidth;
	LONG  tmWeight;
	LONG  tmOverhang;
	LONG  tmDigitizedAspectX;
	LONG  tmDigitizedAspectY;
	WCHAR tmFirstChar;
	WCHAR tmLastChar;
	WCHAR tmDefaultChar;
	WCHAR tmBreakChar;
	BYTE  tmItalic;
	BYTE  tmUnderlined;
	BYTE  tmStruckOut;
	BYTE  tmPitchAndFamily;
	BYTE  tmCharSet;
} TEXTMETRICW, *PTEXTMETRICW, *LPTEXTMETRICW;

/* Used only by D3DXCreateText, which this engine never calls -- but the
   declaration has to parse. */
typedef struct _POINTFLOAT
{
	float x;
	float y;
} POINTFLOAT;

typedef struct _GLYPHMETRICSFLOAT
{
	float      gmfBlackBoxX;
	float      gmfBlackBoxY;
	POINTFLOAT gmfptGlyphOrigin;
	float      gmfCellIncX;
	float      gmfCellIncY;
} GLYPHMETRICSFLOAT, *PGLYPHMETRICSFLOAT, *LPGLYPHMETRICSFLOAT;

typedef LOGFONTA    LOGFONT;
typedef PLOGFONTA   PLOGFONT;
typedef LPLOGFONTA  LPLOGFONT;
typedef TEXTMETRICA TEXTMETRIC;

/* ------------------------------------------------------- device contexts --- */

/* The engine asks for exactly one device-context capability, and only to size
   a font: GetDeviceCaps(GetDC(NULL), LOGPIXELSY) at Engine.cpp:54. The other
   indices are not defined, so a second caller gets a compile error rather than
   a zero. */
#define LOGPIXELSY  90

#ifdef __cplusplus
extern "C" {
#endif

HDC  GetDC(HWND window);
int  ReleaseDC(HWND window, HDC dc);
int  GetDeviceCaps(HDC dc, int index);

#ifdef __cplusplus
}
#endif

#endif
