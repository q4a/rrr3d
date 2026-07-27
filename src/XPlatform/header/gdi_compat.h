/*
 * The handful of GDI and OLE declarations the vendored d3dx9 headers expect
 * from <windows.h>. dxvk's native Windows headers deliberately stop at the
 * scalar types and COM, since a D3D translation layer never needs GDI.
 *
 * ID3DXFont is the reason this exists: it is not optional here. TextFont is a
 * first-class engine resource and every piece of UI text in the game -- labels,
 * buttons, steppers, drop boxes, the menu system -- renders through it. The
 * declarations below let that code compile; a real text backend still has to
 * be written.
 */

#ifndef GDI_COMPAT_H
#define GDI_COMPAT_H

#ifdef _WIN32
#error "gdi_compat.h is for non-Windows builds only"
#endif

#include "windows/windows_base.h"

typedef double DOUBLE;
typedef GUID*  LPGUID;

#define STDAPI extern "C" HRESULT

/* wingdi.h */
#define LF_FACESIZE 32

typedef struct tagTEXTMETRICA {
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
    BYTE  tmFirstChar;
    BYTE  tmLastChar;
    BYTE  tmDefaultChar;
    BYTE  tmBreakChar;
    BYTE  tmItalic;
    BYTE  tmUnderlined;
    BYTE  tmStruckOut;
    BYTE  tmPitchAndFamily;
    BYTE  tmCharSet;
} TEXTMETRICA, *LPTEXTMETRICA;

typedef struct tagTEXTMETRICW {
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
} TEXTMETRICW, *LPTEXTMETRICW;

typedef struct _POINTFLOAT {
    FLOAT x;
    FLOAT y;
} POINTFLOAT;

typedef struct _GLYPHMETRICSFLOAT {
    FLOAT      gmfBlackBoxX;
    FLOAT      gmfBlackBoxY;
    POINTFLOAT gmfptGlyphOrigin;
    FLOAT      gmfCellIncX;
    FLOAT      gmfCellIncY;
} GLYPHMETRICSFLOAT, *LPGLYPHMETRICSFLOAT;

/* LOGFONT values used by TextFont::Desc and D3DXCreateFont */
#define FW_NORMAL          400
#define FW_BOLD            700

#define DEFAULT_CHARSET      1
#define ANSI_CHARSET         0
#define RUSSIAN_CHARSET    204

#define OUT_DEFAULT_PRECIS   0
#define DEFAULT_QUALITY      0
#define DEFAULT_PITCH        0
#define FF_DONTCARE       0x00

/* DrawText format flags used by TextFont::DrawText */
#define DT_LEFT        0x00000000
#define DT_CENTER      0x00000001
#define DT_RIGHT       0x00000002
#define DT_TOP         0x00000000
#define DT_VCENTER     0x00000004
#define DT_BOTTOM      0x00000008
#define DT_WORDBREAK   0x00000010
#define DT_SINGLELINE  0x00000020
#define DT_EXPANDTABS  0x00000040
#define DT_NOCLIP      0x00000100
#define DT_CALCRECT    0x00000400


/*
 * Screen DPI query, used only to size the debug overlay font. HDC and RECT
 * come from the dxvk Windows headers; only these helpers are missing.
 */
#define LOGPIXELSY 90

inline HDC GetDC(HWND) { return NULL; }
inline int ReleaseDC(HWND, HDC) { return 1; }

//96 dpi is the standard logical DPI; the overlay only needs a sane size.
inline int GetDeviceCaps(HDC, int) { return 96; }

inline int MulDiv(int number, int numerator, int denominator)
{
    if (!denominator)
        return -1;
    return static_cast<int>((static_cast<long long>(number) * numerator) / denominator);
}

/* TEXT() maps to the narrow form -- this project builds MBCS, not UNICODE. */
#ifndef TEXT
#define TEXT(s) s
#endif

inline BOOL SetRect(RECT* r, int left, int top, int right, int bottom)
{
    if (!r)
        return FALSE;
    r->left = left; r->top = top; r->right = right; r->bottom = bottom;
    return TRUE;
}

/*
 * objidl.h. Only ever used as a pointer in the d3dx9 declarations we compile,
 * so an incomplete type is enough.
 */
struct IStream;

#endif /* GDI_COMPAT_H */
