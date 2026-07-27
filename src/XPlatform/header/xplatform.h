/*
 * Windows types and calls this project depends on, implemented for
 * non-Windows platforms. Included in place of <windows.h>.
 *
 * These are real implementations, not stubs -- the timing, synchronisation
 * and encoding conversions all have direct C++17 or POSIX equivalents, and
 * silently no-oping them would produce a build that runs but misbehaves.
 * Anything that genuinely has no equivalent is marked and left to the caller.
 */

#ifndef XPLATFORM_H
#define XPLATFORM_H

#ifdef _WIN32
#error "xplatform.h is the non-Windows substitute for <windows.h>"
#endif

#include <cfloat>
#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

/* ---- scalar types ---- */

typedef int             BOOL;
typedef unsigned char   BYTE;
typedef unsigned short  WORD;
typedef uint32_t        DWORD;
typedef int32_t         LONG;
typedef unsigned int    UINT;
typedef int             INT;
typedef char            TCHAR;
typedef wchar_t         WCHAR;
typedef void*           HANDLE;
typedef void*           HWND;
typedef void*           LPVOID;
typedef const char*     LPCSTR;
typedef char*           LPSTR;
typedef const wchar_t*  LPCWSTR;
typedef wchar_t*        LPWSTR;
typedef int32_t         HRESULT;
typedef intptr_t        LONG_PTR;
typedef uintptr_t       DWORD_PTR;
typedef uintptr_t       UINT_PTR;
typedef int64_t         __int64;

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define WINAPI
#define APIENTRY
#define CALLBACK
#define __stdcall
#define __cdecl

#define MAXUINT   UINT_MAX
#define INFINITE  0xFFFFFFFFu
#define MAX_PATH  1024

/* Code pages. Everything in this project is treated as UTF-8 on non-Windows. */
#define CP_ACP        0
#define CP_THREAD_ACP 3
#define CP_UTF8       65001

/* Conversion flags. Our converters go through UTF-8, where these are no-ops. */
#define MB_PRECOMPOSED       0x00000001
#define WC_NO_BEST_FIT_CHARS 0x00000400

#define PATH_SEP '/'

typedef union _LARGE_INTEGER {
    struct { DWORD LowPart; LONG HighPart; };
    int64_t QuadPart;
} LARGE_INTEGER;

/* ---- timing (QueryPerformanceCounter / GetTickCount) ---- */

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

#define WAIT_OBJECT_0 0x00000000u
#define WAIT_TIMEOUT  0x00000102u

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

/* MSVC debug-CRT breakpoint */
#define _CrtDbgBreak() __builtin_trap()

int MessageBox(HWND owner, const char* text, const char* caption, UINT type);
void OutputDebugStringA(const char* str);
#define OutputDebugString OutputDebugStringA

#ifndef ZeroMemory
#define ZeroMemory(dst, len) memset((dst), 0, (len))
#endif

/*
 * CRT spellings MSVC provides and the C standard does not.
 */
#define _vsnprintf  vsnprintf
#define _vsnwprintf vswprintf

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

#endif /* XPLATFORM_H */
