/*
 * Windows API substitutes for non-Windows platforms. Included in place of
 * <windows.h>.
 *
 * The Windows *types* (scalars, COM, LARGE_INTEGER, ...) come from the
 * vendored dxvk native headers in windows/, which are the same set used by
 * native non-Wine D3D9 builds and which d3d9.h needs anyway. This header adds
 * the Win32 *calls* this project uses, which those headers do not provide.
 *
 * The implementations in xplatform.cpp are real, not stubs -- the timing,
 * synchronisation and encoding conversions all have direct C++17 or POSIX
 * equivalents, and silently no-oping them would produce a build that runs but
 * misbehaves.
 */

#ifndef XPLATFORM_H
#define XPLATFORM_H

#ifdef _WIN32
#error "xplatform.h is the non-Windows substitute for <windows.h>"
#endif

/* Windows types, COM, LARGE_INTEGER. Also included from C. */
#include "windows/windows_base.h"

#ifdef __cplusplus
	#include <cfloat>
	#include <climits>
	#include <cstdarg>
	#include <cstddef>
	#include <cstdint>
	#include <cstdio>
	#include <ctime>
	#include <cstring>
#else
	#include <float.h>
	#include <limits.h>
	#include <stdarg.h>
	#include <stddef.h>
	#include <stdint.h>
	#include <stdio.h>
	#include <string.h>
#endif

/* ---- types the dxvk headers do not cover ---- */

typedef char     TCHAR;
typedef int64_t  __int64;

#ifndef MAXUINT
#define MAXUINT UINT_MAX
#endif
#ifndef INFINITE
#define INFINITE 0xFFFFFFFFu
#endif

#define PATH_SEP '/'

/* Code pages. Everything in this project is treated as UTF-8 on non-Windows. */
#define CP_ACP        0
#define CP_THREAD_ACP 3
#define CP_UTF8       65001

/* Conversion flags. Our converters go through UTF-8, where these are no-ops. */
#define MB_PRECOMPOSED       0x00000001
#define WC_NO_BEST_FIT_CHARS 0x00000400

#ifdef __cplusplus
extern "C" {
#endif

/* ---- timing ---- */

BOOL  QueryPerformanceCounter(LARGE_INTEGER* count);
BOOL  QueryPerformanceFrequency(LARGE_INTEGER* freq);
DWORD GetTickCount(void);
void  Sleep(DWORD milliseconds);

/* ---- critical sections (std::recursive_mutex underneath) ---- */

typedef struct _RTL_CRITICAL_SECTION {
    void* opaque;
} RTL_CRITICAL_SECTION, CRITICAL_SECTION;

void InitializeCriticalSection(RTL_CRITICAL_SECTION* cs);
void DeleteCriticalSection(RTL_CRITICAL_SECTION* cs);
void EnterCriticalSection(RTL_CRITICAL_SECTION* cs);
void LeaveCriticalSection(RTL_CRITICAL_SECTION* cs);

/* ---- events (condition variable underneath) ---- */

HANDLE CreateEvent(void* attrs, BOOL manualReset, BOOL initialState, const char* name);
BOOL   SetEvent(HANDLE event);
BOOL   ResetEvent(HANDLE event);
DWORD  WaitForSingleObject(HANDLE event, DWORD milliseconds);
BOOL   CloseHandle(HANDLE handle);

/* ---- thread pool ---- */

typedef DWORD (*LPTHREAD_START_ROUTINE)(void*);
#define WT_EXECUTELONGFUNCTION 0x00000010

BOOL QueueUserWorkItem(LPTHREAD_START_ROUTINE func, void* context, unsigned long flags);

/* ---- encoding conversion ---- */

int MultiByteToWideChar(UINT codePage, DWORD flags, const char* src, int srcLen,
                        wchar_t* dst, int dstLen);
int WideCharToMultiByte(UINT codePage, DWORD flags, const wchar_t* src, int srcLen,
                        char* dst, int dstLen, const char* defaultChar, BOOL* usedDefault);

/* ---- filesystem ---- */

#define INVALID_FILE_ATTRIBUTES    ((DWORD)-1)
#define FILE_ATTRIBUTE_DIRECTORY   0x00000010

DWORD GetFileAttributesW(const wchar_t* path);
DWORD GetModuleFileNameW(void* module, wchar_t* buf, DWORD size);

/* ---- misc ---- */

#define MB_OK                0x00000000
#define MB_ABORTRETRYIGNORE  0x00000002
#define MB_ICONEXCLAMATION   0x00000030
#define MB_TASKMODAL         0x00002000

#define IDOK      1
#define IDABORT   3
#define IDRETRY   4
#define IDIGNORE  5

int  MessageBox(HWND owner, const char* text, const char* caption, UINT type);
void OutputDebugStringA(const char* str);

/*
 * The engine asks the window for its client area to size render targets. There
 * is no window system behind HWND yet -- RRR3d.cpp's Win32 shell has not been
 * replaced -- so this reports an empty rect and callers fall back to the size
 * they were configured with. Revisit when the SDL window lands.
 */
BOOL GetClientRect(HWND wnd, RECT* rect);

/*
 * Polled keyboard state. Used by debug overlays and the camera fly-through, all
 * of which the SDL input port will re-source from SDL_GetKeyboardState.
 * Reports every key up until then.
 */
SHORT GetAsyncKeyState(int virtualKey);

typedef struct tagWINDOWINFO
{
    DWORD cbSize;
    RECT  rcWindow;
    RECT  rcClient;
    DWORD dwStyle;
    DWORD dwExStyle;
    DWORD dwWindowStatus;
    UINT  cxWindowBorders;
    UINT  cyWindowBorders;
    WORD  atomWindowType;
    WORD  wCreatorVersion;
} WINDOWINFO, *PWINDOWINFO;

/*
 * Window management, all no-ops for the same reason GetClientRect is: there is
 * no window system behind HWND until the SDL shell lands.
 */
BOOL SetWindowPos(HWND wnd, HWND insertAfter, int x, int y, int cx, int cy, UINT flags);
LONG GetWindowLong(HWND wnd, int index);
LONG SetWindowLong(HWND wnd, int index, LONG newLong);
BOOL GetWindowInfo(HWND wnd, PWINDOWINFO info);
BOOL InvalidateRect(HWND wnd, const RECT* rect, BOOL erase);
BOOL UpdateWindow(HWND wnd);

/*
 * Expands a client rect to the window rect a given style would need. With no
 * window decorations to account for, the rect passes through unchanged.
 */
BOOL AdjustWindowRect(RECT* rect, DWORD style, BOOL menu);

/*
 * Thread affinity. The one caller pins the main thread to CPU 0 so
 * QueryPerformanceCounter does not jump between cores on older AMD parts --
 * a Windows-specific hazard with no macOS analogue, where mach_absolute_time
 * is coherent across cores. Reports success and does nothing.
 */
HANDLE GetCurrentThread(void);
ULONG_PTR SetThreadAffinityMask(HANDLE thread, ULONG_PTR affinityMask);
BOOL GetCursorPos(POINT* point);
BOOL ScreenToClient(HWND wnd, POINT* point);

/*
 * winuser.h character classification, used when validating typed player names.
 * Byte-oriented, as the Windows MBCS versions are.
 */
BOOL IsCharAlpha(char ch);
BOOL IsCharAlphaNumeric(char ch);

/*
 * Locale selection. Text encoding is UTF-8 throughout here and there is no
 * thread locale to set, so both succeed and change nothing.
 */
int  _setmbcp(int codepage);
BOOL SetThreadLocale(DWORD locale);

/* combaseapi.h. COM apartment setup has no meaning here. */
#define COINIT_MULTITHREADED     0x0
#define COINIT_DISABLE_OLE1DDE   0x4

HRESULT CoInitializeEx(void* reserved, DWORD coInit);
void CoUninitialize(void);

/* MSVC's 32-bit time_t variant. */
#ifdef __cplusplus
inline long _time32(long* dest)
{
    const long now = static_cast<long>(std::time(nullptr));
    if (dest)
        *dest = now;
    return now;
}
#endif

/*
 * mmsystem.h. These raise and restore the Windows timer interrupt resolution,
 * which the frame limiter does so Sleep() is accurate to a millisecond. macOS
 * timers are already fine-grained and there is nothing global to change, so
 * these succeed and do nothing.
 */
#define TIMERR_NOERROR 0

/*
 * winuser.h. The base of the range reserved for application-defined window
 * messages; the engine derives WM_GRAPH_EVENT from it. Nothing dispatches
 * these yet -- there is no message loop off Windows -- but the constant has to
 * carry the same value so the derived ids stay stable.
 */
#define WM_APP 0x8000

/*
 * winuser.h virtual key codes. The input port replaces GetAsyncKeyState and the
 * WM_KEYDOWN path with SDL, but ControlManager stores and serialises these
 * numeric values -- they appear in saved key bindings -- so they must keep the
 * same values whatever reads them.
 */
#define VK_LBUTTON      0x01
#define VK_RBUTTON      0x02
#define VK_MBUTTON      0x04
#define VK_BACK         0x08
#define VK_TAB          0x09
#define VK_RETURN       0x0D
#define VK_SHIFT        0x10
#define VK_CONTROL      0x11
#define VK_MENU         0x12
#define VK_ESCAPE       0x1B
#define VK_SPACE        0x20
#define VK_PRIOR        0x21
#define VK_NEXT         0x22
#define VK_END          0x23
#define VK_HOME         0x24
#define VK_LEFT         0x25
#define VK_UP           0x26
#define VK_RIGHT        0x27
#define VK_DOWN         0x28
#define VK_DELETE       0x2E
#define VK_NUMPAD0      0x60
#define VK_NUMPAD1      0x61
#define VK_NUMPAD2      0x62
#define VK_NUMPAD3      0x63
#define VK_NUMPAD4      0x64
#define VK_NUMPAD5      0x65
#define VK_NUMPAD6      0x66
#define VK_NUMPAD7      0x67
#define VK_NUMPAD8      0x68
#define VK_NUMPAD9      0x69
#define VK_F1           0x70
#define VK_F2           0x71
#define VK_F3           0x72
#define VK_F4           0x73
#define VK_F5           0x74
#define VK_F6           0x75
#define VK_F7           0x76
#define VK_F8           0x77
#define VK_F9           0x78
#define VK_F10          0x79
#define VK_F11          0x7A
#define VK_F12          0x7B
#define VK_ADD          0x6B
#define VK_SUBTRACT     0x6D
#define VK_OEM_PERIOD   0xBE

/*
 * Window styles and SetWindowPos flags. Nothing creates a window off Windows
 * yet -- RRR3d.cpp's Win32 shell is still to be replaced with SDL -- so these
 * exist to keep the mode-switching code compiling until it is rewritten.
 */
#define WS_OVERLAPPEDWINDOW 0x00CF0000
#define WS_POPUP            0x80000000
#define WS_VISIBLE          0x10000000
#define WS_EX_TOPMOST       0x00000008

#define GWL_STYLE          (-16)
#define GWL_EXSTYLE        (-20)

#define SWP_NOMOVE         0x0002
#define SWP_NOSIZE         0x0001
#define SWP_NOZORDER       0x0004
#define SWP_FRAMECHANGED   0x0020
#define SWP_SHOWWINDOW     0x0040

/* wingdi.h charsets used when picking a UI font. */
#define EASTEUROPE_CHARSET 238
#define BALTIC_CHARSET     186

/*
 * winerror.h. Only the two the game tests for.
 */
#define ERROR_SUCCESS 0L

/*
 * wingdi.h display enumeration. GameMode walks the attached displays to build
 * the resolution list; with no display server behind HWND it finds none and the
 * configured mode is used as-is.
 */
#define DISPLAY_DEVICE_ATTACHED_TO_DESKTOP 0x00000001

typedef struct _DISPLAY_DEVICEA
{
    DWORD cb;
    CHAR  DeviceName[32];
    CHAR  DeviceString[128];
    DWORD StateFlags;
    CHAR  DeviceID[128];
    CHAR  DeviceKey[128];
} DISPLAY_DEVICEA, DISPLAY_DEVICE, *PDISPLAY_DEVICE;

BOOL EnumDisplayDevices(const char* device, DWORD deviceNum, PDISPLAY_DEVICE displayDevice, DWORD flags);

/*
 * winnls.h. The UI language selects which Data/<language>.txt the game loads.
 * Reporting neutral English leaves it on the default; wiring this to
 * CFLocaleCopyPreferredLanguages is a small follow-up once the port runs.
 */
typedef WORD LANGID;

#define LANG_NEUTRAL       0x00
#define SUBLANG_NEUTRAL    0x00
#define SORT_DEFAULT       0x0

#define MAKELANGID(p, s)   ((((WORD)(s)) << 10) | (WORD)(p))
#define PRIMARYLANGID(lgid) ((WORD)(lgid) & 0x3ff)
#define MAKELCID(lgid, srt) ((DWORD)((((DWORD)((WORD)(srt))) << 16) | ((DWORD)((WORD)(lgid)))))

LANGID GetUserDefaultUILanguage(void);

/* mbctype.h -- the code page selector _setmbcp takes. */
#define _MB_CP_LOCALE (-4)

/*
 * DirectShow filter graph event codes. The video player is stubbed, but World
 * and GameMode switch on these in their own graph-event handlers, so the values
 * have to exist and match.
 */
#define EC_COMPLETE   0x01
#define EC_USERABORT  0x02
#define EC_ERRORABORT 0x03

/* Calling conventions. windows_base.h defines WINAPI; these are its siblings. */
#ifndef CALLBACK
#define CALLBACK
#endif
#ifndef APIENTRY
#define APIENTRY
#endif

#ifdef __cplusplus
/*
 * MSVC's secure CRT. There is no wide-path fopen off Windows -- paths there are
 * bytes, and this project treats them as UTF-8 -- so both arguments are
 * narrowed on the way through. Returns 0 on success, like the original.
 */
int _wfopen_s(FILE** file, const wchar_t* path, const wchar_t* mode);
#endif /* __cplusplus */

UINT timeBeginPeriod(UINT period);
UINT timeEndPeriod(UINT period);

#ifdef __cplusplus
}
#endif

#define OutputDebugString OutputDebugStringA

/* MSVC debug-CRT breakpoint */
#define _CrtDbgBreak() __builtin_trap()

/*
 * CRT spellings MSVC provides and the C standard does not.
 */
#define _vsnprintf  vsnprintf
#define _vsnwprintf vswprintf

#ifdef __cplusplus
template <size_t size>
inline int sprintf_s(char (&buffer)[size], const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}

inline int sprintf_s(char* buffer, size_t size, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}
#endif /* __cplusplus */

#endif /* XPLATFORM_H */
