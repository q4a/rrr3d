#ifndef XPLATFORM_H
#define XPLATFORM_H

/*
 * A Win32 substitute layer.
 *
 * The engine is written against Win32 and there is no value in rewriting it.
 * This provides the pieces it actually calls, with Win32's semantics -- not an
 * improved version of them. Where Win32 returns a surprising value, so does
 * this: WaitForSingleObject returns 0 for "signalled", MessageBox returns an
 * ID*, GetTickCount wraps at 32 bits. Callers were written against those
 * answers and several of them depend on the surprise.
 *
 * On Windows this header is not on the include path at all, so nothing here can
 * shadow the real thing.
 */

#ifdef _WIN32
#error "xplatform.h is the substitute layer; on Windows include <windows.h>"
#endif

/* C headers, not the <c...> spellings: the vendored D3DX math implementation is
   C and reaches this header through d3d9types.h. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>

/* ---------------------------------------------------------------- types -- */

/*
 * The Windows scalar types, the COM declaration macros, HRESULT and its
 * constants all come from DXVK's native base -- vendored by
 * tools/vendor-directx-headers.py. Using DXVK's rather than a hand-written set
 * is deliberate: the D3D9 implementation this port runs on is DXVK, so the game
 * and the backend agree on those definitions by construction.
 */
#include "windows/windows_base.h"

typedef int64_t             __int64;
typedef int32_t             __int32;
typedef int16_t             __int16;
typedef int8_t              __int8;

/* windows_base.h has FLOAT but not DOUBLE, which d3dx9anim.h uses. */
#ifndef XPLATFORM_HAS_DOUBLE
#define XPLATFORM_HAS_DOUBLE
typedef double              DOUBLE;
#endif

/*
 * The last few names the vendored D3DX headers reach for from headers this port
 * does not carry. They appear only in declarations the engine never calls --
 * .X-file loading, mesh streaming -- so IStream is left an incomplete type on
 * purpose: a pointer to it is all those signatures need, and leaving it
 * incomplete means any accidental use is a compile error rather than a link
 * one.
 */
#ifndef LPGUID
typedef GUID*               LPGUID;
#endif

/* Typedef, not a bare struct declaration: the vendored math implementation is C,
   where a struct tag alone does not make `IStream` a type name. */
typedef struct IStream IStream;

#ifndef STDAPI
#define STDAPI          HRESULT WINAPI
#define STDAPI_(type)   type WINAPI
#endif

/*
 * windows_base.h has DECLARE_INTERFACE and DECLARE_INTERFACE_ but not the
 * IID-carrying form the D3DX headers use. The trailing string is only consumed
 * by MSVC's __declspec(uuid), so the interface is declared exactly as the
 * two-argument form does and the IID is discarded -- which is what MinGW's own
 * headers do off MSVC.
 */
#ifndef DECLARE_INTERFACE_IID_
#define DECLARE_INTERFACE_IID_(i, b, d) DECLARE_INTERFACE_(i, b)
#endif

#define __cdecl
#define CALLBACK

/*
 * __declspec(dllexport) / __declspec(dllimport) become nothing. Symbols in a
 * shared library here have default visibility already, so the export
 * annotations Windows requires are simply unnecessary rather than translated.
 * These are the only two forms the tree uses.
 */
#define __declspec(x)

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#define MAXUINT   ((UINT)~((UINT)0))
#define MAXDWORD  ((DWORD)~((DWORD)0))
#define MAXWORD   ((WORD)0xffff)
#define MAXBYTE   ((BYTE)0xff)
#define MAXLONG   ((LONG)0x7fffffff)

/* --------------------------------------------------------- code pages --- */

#define CP_ACP        0
#define CP_UTF8       65001
#define MB_PRECOMPOSED       0x00000001
#define WC_NO_BEST_FIT_CHARS 0x00000400

/* ------------------------------------------------------------ waiting --- */

/* windows_base.h already carries some of these. */
#ifndef INFINITE
#define INFINITE        0xFFFFFFFF
#endif
#ifndef WAIT_OBJECT_0
#define WAIT_OBJECT_0   0x00000000
#endif
#ifndef WAIT_TIMEOUT
#define WAIT_TIMEOUT    0x00000102
#endif
#ifndef WAIT_FAILED
#define WAIT_FAILED     0xFFFFFFFF
#endif

/* --------------------------------------------------------- messagebox --- */

#define MB_OK                0x00000000
#define MB_OKCANCEL          0x00000001
#define MB_ABORTRETRYIGNORE  0x00000002
#define MB_YESNO             0x00000004
#define MB_ICONERROR         0x00000010
#define MB_ICONEXCLAMATION   0x00000030
#define MB_TASKMODAL         0x00002000

#define IDOK      1
#define IDCANCEL  2
#define IDABORT   3
#define IDRETRY   4
#define IDIGNORE  5
#define IDYES     6
#define IDNO      7

/* --------------------------------------------------- critical sections --- */

/*
 * Win32 critical sections are recursive and this engine nests its locks, so a
 * plain std::mutex deadlocks. The opaque storage is sized to hold a
 * std::recursive_mutex* and nothing else; the implementation owns its layout.
 */
typedef struct _RTL_CRITICAL_SECTION
{
	void* opaque;
} RTL_CRITICAL_SECTION, CRITICAL_SECTION;

typedef RTL_CRITICAL_SECTION* LPCRITICAL_SECTION;

#ifdef __cplusplus
extern "C" {
#endif

void  InitializeCriticalSection(LPCRITICAL_SECTION section);
void  DeleteCriticalSection(LPCRITICAL_SECTION section);
void  EnterCriticalSection(LPCRITICAL_SECTION section);
void  LeaveCriticalSection(LPCRITICAL_SECTION section);
BOOL  TryEnterCriticalSection(LPCRITICAL_SECTION section);

/* ------------------------------------------------------------- events --- */

HANDLE CreateEventA(void* attributes, BOOL manualReset, BOOL initialState, LPCSTR name);
BOOL   SetEvent(HANDLE event);
BOOL   ResetEvent(HANDLE event);
DWORD  WaitForSingleObject(HANDLE handle, DWORD milliseconds);
BOOL   CloseHandle(HANDLE handle);

/* ------------------------------------------------------------ threads --- */

typedef DWORD (*LPTHREAD_START_ROUTINE)(LPVOID);

HANDLE CreateThread(void* attributes, size_t stackSize,
                    LPTHREAD_START_ROUTINE start, LPVOID parameter,
                    DWORD creationFlags, DWORD* threadId);

/* --------------------------------------------------------- thread pool --- */

#define WT_EXECUTEDEFAULT        0x00000000
#define WT_EXECUTELONGFUNCTION   0x00000010

/* Win32 runs the callback on a pool thread and returns immediately. There is no
   pool here; each item gets a detached thread, which matches the only thing the
   caller depends on -- that QueueWork does not block. */
BOOL QueueUserWorkItem(LPTHREAD_START_ROUTINE function, LPVOID context, DWORD flags);

/* ------------------------------------------------------------ files --- */

#define INVALID_FILE_ATTRIBUTES   ((DWORD)-1)
#define FILE_ATTRIBUTE_READONLY   0x00000001
#define FILE_ATTRIBUTE_DIRECTORY  0x00000010
#define FILE_ATTRIBUTE_NORMAL     0x00000080

DWORD GetFileAttributesA(LPCSTR filename);
DWORD GetFileAttributesW(LPCWSTR filename);

/* ---------------------------------------------------------- module entry --- */

/* windows_base.h defines WINAPI as nothing; APIENTRY is its other spelling.
 *
 * The DLL_* reasons let Rock3dGame's dllmain.cpp compile unchanged. Its DllMain
 * does nothing but break out of a switch and return TRUE, and nothing calls it
 * here -- a dylib has no such entry point. It is kept rather than excluded so
 * the file stays identical on both platforms. */
#define APIENTRY

#define DLL_PROCESS_DETACH  0
#define DLL_PROCESS_ATTACH  1
#define DLL_THREAD_ATTACH   2
#define DLL_THREAD_DETACH   3

/* ------------------------------------------------------------ windows --- */

/* An HWND here is not a window handle -- the SDL3 shell passes a CAMetalLayer
   through as one -- so nothing can be asked of it. GetClientRect therefore
   answers from a size the shell publishes rather than from the handle.
 *
 * This is why XPlatform is built SHARED. A static archive is linked into both
 * the executable and libRock3dGame.dylib, giving each its own copy of the size
 * below: the shell would register into one and the engine would read the other,
 * silently, forever. */
void RegisterClientSize(HWND window, long width, long height);
BOOL GetClientRect(HWND window, LPRECT rect);

/* ------------------------------------------------------------- timing --- */

DWORD GetTickCount(void);
BOOL  QueryPerformanceCounter(LARGE_INTEGER* count);
BOOL  QueryPerformanceFrequency(LARGE_INTEGER* frequency);
void  Sleep(DWORD milliseconds);

/* ------------------------------------------------------------ strings --- */

int MultiByteToWideChar(UINT codePage, DWORD flags, LPCSTR src, int srcLen,
                        LPWSTR dst, int dstLen);
int WideCharToMultiByte(UINT codePage, DWORD flags, LPCWSTR src, int srcLen,
                        LPSTR dst, int dstLen, LPCSTR defaultChar,
                        BOOL* usedDefaultChar);

/* ------------------------------------------------------------- module --- */

DWORD GetModuleFileNameA(void* module, LPSTR filename, DWORD size);
DWORD GetModuleFileNameW(void* module, LPWSTR filename, DWORD size);

/* --------------------------------------------------------------- misc --- */

int  MulDiv(int number, int numerator, int denominator);

/* COM apartment init. Rock3dGame.cpp calls CoInitializeEx at startup and
   CoUninitialize at shutdown, purely so DirectShow can create its filter graph;
   nothing else in the tree is a COM client. Both succeed and do nothing here --
   there is no apartment model to enter. */
#define COINIT_MULTITHREADED      0x0
#define COINIT_APARTMENTTHREADED  0x2
#define COINIT_DISABLE_OLE1DDE    0x4
#define COINIT_SPEED_OVER_MEMORY  0x8

HRESULT CoInitializeEx(void* reserved, DWORD coInit);
void    CoUninitialize(void);

/* Windows pins the main thread to one core so QueryPerformanceCounter cannot
   jump between cores -- a workaround for old multi-socket and AMD machines
   whose TSCs were not synchronised. Rock3dGame.cpp:18 does it for that reason
   and says so in its comment.
 *
 * macOS has no thread-affinity API of this shape, and does not need one:
   mach_absolute_time is coherent across cores, so the hazard being avoided
   does not exist. Returning the previous mask, as Windows does, is accurate. */
HANDLE    GetCurrentThread(void);
ULONG_PTR SetThreadAffinityMask(HANDLE thread, ULONG_PTR affinityMask);
int  MessageBoxA(void* owner, LPCSTR text, LPCSTR caption, UINT type);
void OutputDebugStringA(LPCSTR text);
/* ERROR_SUCCESS is the DWORD status zero that XInputGetState and friends
   return; it is not an HRESULT and does not go through SUCCEEDED(). */
#define ERROR_SUCCESS  0

DWORD GetLastError(void);
void  SetLastError(DWORD error);

#ifdef __cplusplus
} /* extern "C" */
#endif

#define CreateEvent         CreateEventA
#define GetFileAttributes   GetFileAttributesA
#define GetModuleFileName   GetModuleFileNameA
#define MessageBox          MessageBoxA
#define OutputDebugString   OutputDebugStringA

/* winnt.h's, and it is the identity in an ANSI build. Two call sites, both
   passing a literal font name to D3DXCreateFont. */
#define TEXT(quote) quote

/* ------------------------------------------------------- secure CRT ---- */

/* C++ only. Nothing compiled as C in this tree calls the _s functions, and the
   array-size overloads below need templates. */
#ifdef __cplusplus

/*
 * The _s functions are Microsoft's. The array-size overloads are what the
 * engine actually calls, so they are what is provided; each mirrors the
 * truncating behaviour of the real thing rather than its invalid-parameter
 * handler, which no caller here relies on.
 */
inline int vsprintf_s(char* buffer, size_t size, const char* format, va_list args)
{
	return vsnprintf(buffer, size, format, args);
}

inline int sprintf_s(char* buffer, size_t size, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	const int result = vsnprintf(buffer, size, format, args);
	va_end(args);
	return result;
}

template<size_t size> inline int sprintf_s(char (&buffer)[size], const char* format, ...)
{
	va_list args;
	va_start(args, format);
	const int result = vsnprintf(buffer, size, format, args);
	va_end(args);
	return result;
}

/*
 * The underscore-prefixed CRT names. _vsnprintf differs from vsnprintf in that
 * it does not guarantee a terminator on truncation; callers here always pass a
 * buffer they then terminate themselves, so the standard function is a safe
 * stand-in and a strictly better one.
 */
inline int _vsnprintf(char* buffer, size_t count, const char* format, va_list args)
{
	return vsnprintf(buffer, count, format, args);
}

inline int _vsnwprintf(wchar_t* buffer, size_t count, const wchar_t* format, va_list args)
{
	return vswprintf(buffer, count, format, args);
}

inline int _snprintf(char* buffer, size_t count, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	const int result = vsnprintf(buffer, count, format, args);
	va_end(args);
	return result;
}

/* Microsoft's array-length macro. A template rather than the usual
   sizeof(a)/sizeof(a[0]), so that it refuses a pointer instead of silently
   returning 1 or 2 -- which is the whole reason MSVC provides it. */
template<class T, size_t size> char (&_countof_helper(T (&array)[size]))[size];
#define _countof(array) (sizeof(_countof_helper(array)))

/* Microsoft's explicitly-32-bit time(). The one caller seeds srand with it, so
   the truncation that made _time32 a named function on Windows is harmless --
   and reproducing it keeps the seed identical on both platforms. */
inline long _time32(long* destTime)
{
	const long now = static_cast<long>(time(NULL));
	if (destTime)
		*destTime = now;
	return now;
}

inline int strcpy_s(char* dst, size_t size, const char* src)
{
	if (!dst || !src || size == 0)
		return 22; /* EINVAL */
	size_t i = 0;
	for (; i + 1 < size && src[i]; ++i)
		dst[i] = src[i];
	dst[i] = '\0';
	return src[i] ? 34 : 0; /* ERANGE when truncated */
}

template<size_t size> inline int strcpy_s(char (&dst)[size], const char* src)
{
	return strcpy_s(dst, size, src);
}

#endif /* __cplusplus */

#endif /* XPLATFORM_H */
