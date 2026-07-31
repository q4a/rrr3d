#include "xplatform.h"

#include "mmsystem.h"
#include "wingdi.h"

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include <cstdlib>

#include <mach-o/dyld.h>
#include <sys/stat.h>

namespace
{

std::chrono::steady_clock::time_point ProcessStart()
{
	static const std::chrono::steady_clock::time_point start =
		std::chrono::steady_clock::now();
	return start;
}

/*
 * Win32 events. A manual-reset event stays signalled until Reset; an
 * auto-reset event releases exactly one waiter and clears itself.
 */
struct Event
{
	std::mutex mutex;
	std::condition_variable condition;
	bool signalled = false;
	bool manualReset = false;
};

/* -------------------------------------------------------------------------
 * UTF-8 <-> UTF-32 (wchar_t is 32 bits here, which is the whole point).
 *
 * CP_ACP is treated as UTF-8. On Windows it is whatever the system codepage
 * is, but the sources are UTF-8 and the MSVC build sets /utf-8, so UTF-8 is
 * the encoding narrow strings actually carry in this program.
 * ---------------------------------------------------------------------- */

size_t DecodeUtf8(const char* src, size_t len, size_t& pos, uint32_t& out)
{
	const unsigned char lead = static_cast<unsigned char>(src[pos]);
	size_t extra;
	uint32_t code;

	if (lead < 0x80)       { extra = 0; code = lead; }
	else if (lead < 0xE0)  { extra = 1; code = lead & 0x1F; }
	else if (lead < 0xF0)  { extra = 2; code = lead & 0x0F; }
	else                   { extra = 3; code = lead & 0x07; }

	if (pos + extra >= len + 1 && extra > 0 && pos + extra > len - 1)
	{
		/* Truncated sequence: consume one byte and substitute. */
		++pos;
		out = 0xFFFD;
		return 1;
	}

	for (size_t i = 1; i <= extra; ++i)
		code = (code << 6) | (static_cast<unsigned char>(src[pos + i]) & 0x3F);

	pos += extra + 1;
	out = code;
	return extra + 1;
}

size_t EncodeUtf8(uint32_t code, char* dst)
{
	if (code < 0x80)
	{
		if (dst) dst[0] = static_cast<char>(code);
		return 1;
	}
	if (code < 0x800)
	{
		if (dst)
		{
			dst[0] = static_cast<char>(0xC0 | (code >> 6));
			dst[1] = static_cast<char>(0x80 | (code & 0x3F));
		}
		return 2;
	}
	if (code < 0x10000)
	{
		if (dst)
		{
			dst[0] = static_cast<char>(0xE0 | (code >> 12));
			dst[1] = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
			dst[2] = static_cast<char>(0x80 | (code & 0x3F));
		}
		return 3;
	}
	if (dst)
	{
		dst[0] = static_cast<char>(0xF0 | (code >> 18));
		dst[1] = static_cast<char>(0x80 | ((code >> 12) & 0x3F));
		dst[2] = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
		dst[3] = static_cast<char>(0x80 | (code & 0x3F));
	}
	return 4;
}

thread_local DWORD gLastError = 0;

} /* namespace */

extern "C" {

/* -------------------------------------------------------- critical sections */

void InitializeCriticalSection(LPCRITICAL_SECTION section)
{
	section->opaque = new std::recursive_mutex();
}

void DeleteCriticalSection(LPCRITICAL_SECTION section)
{
	delete static_cast<std::recursive_mutex*>(section->opaque);
	section->opaque = nullptr;
}

void EnterCriticalSection(LPCRITICAL_SECTION section)
{
	static_cast<std::recursive_mutex*>(section->opaque)->lock();
}

void LeaveCriticalSection(LPCRITICAL_SECTION section)
{
	static_cast<std::recursive_mutex*>(section->opaque)->unlock();
}

BOOL TryEnterCriticalSection(LPCRITICAL_SECTION section)
{
	return static_cast<std::recursive_mutex*>(section->opaque)->try_lock() ? TRUE : FALSE;
}

/* ------------------------------------------------------------------ events */

HANDLE CreateEventA(void*, BOOL manualReset, BOOL initialState, LPCSTR)
{
	/* Named events are not supported: nothing in this program opens one by
	   name, and cross-process signalling has no meaning here. */
	Event* event = new Event();
	event->manualReset = manualReset != FALSE;
	event->signalled = initialState != FALSE;
	return event;
}

BOOL SetEvent(HANDLE handle)
{
	Event* event = static_cast<Event*>(handle);
	if (!event) return FALSE;
	{
		std::lock_guard<std::mutex> lock(event->mutex);
		event->signalled = true;
	}
	if (event->manualReset)
		event->condition.notify_all();
	else
		event->condition.notify_one();
	return TRUE;
}

BOOL ResetEvent(HANDLE handle)
{
	Event* event = static_cast<Event*>(handle);
	if (!event) return FALSE;
	std::lock_guard<std::mutex> lock(event->mutex);
	event->signalled = false;
	return TRUE;
}

DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds)
{
	Event* event = static_cast<Event*>(handle);
	if (!event) return WAIT_FAILED;

	std::unique_lock<std::mutex> lock(event->mutex);

	if (milliseconds == INFINITE)
	{
		event->condition.wait(lock, [event] { return event->signalled; });
	}
	else if (!event->condition.wait_for(lock, std::chrono::milliseconds(milliseconds),
	                                    [event] { return event->signalled; }))
	{
		return WAIT_TIMEOUT;
	}

	if (!event->manualReset)
		event->signalled = false;

	/* WAIT_OBJECT_0 is zero. Callers here test the result in ways that depend
	   on that, so it is returned rather than something more readable. */
	return WAIT_OBJECT_0;
}

BOOL CloseHandle(HANDLE handle)
{
	delete static_cast<Event*>(handle);
	return TRUE;
}

/* ----------------------------------------------------------------- threads */

HANDLE CreateThread(void*, size_t, LPTHREAD_START_ROUTINE start,
                    LPVOID parameter, DWORD, DWORD* threadId)
{
	if (threadId)
		*threadId = 0;
	std::thread(start, parameter).detach();
	/* Win32 hands back a joinable handle; nothing here joins one, so a
	   non-null sentinel is enough and is honest about what it is. */
	return reinterpret_cast<HANDLE>(1);
}

BOOL QueueUserWorkItem(LPTHREAD_START_ROUTINE function, LPVOID context, DWORD)
{
	if (!function) return FALSE;
	std::thread(function, context).detach();
	return TRUE;
}

/* ------------------------------------------------------------------- files */

DWORD GetFileAttributesA(LPCSTR filename)
{
	if (!filename) return INVALID_FILE_ATTRIBUTES;

	struct stat info;
	if (stat(filename, &info) != 0)
		return INVALID_FILE_ATTRIBUTES;

	DWORD attributes = 0;
	if (S_ISDIR(info.st_mode))
		attributes |= FILE_ATTRIBUTE_DIRECTORY;
	if ((info.st_mode & S_IWUSR) == 0)
		attributes |= FILE_ATTRIBUTE_READONLY;
	if (attributes == 0)
		attributes = FILE_ATTRIBUTE_NORMAL;
	return attributes;
}

DWORD GetFileAttributesW(LPCWSTR filename)
{
	if (!filename) return INVALID_FILE_ATTRIBUTES;

	char narrow[4096];
	if (WideCharToMultiByte(CP_UTF8, 0, filename, -1, narrow,
	                        static_cast<int>(sizeof(narrow)), nullptr, nullptr) == 0)
		return INVALID_FILE_ATTRIBUTES;

	return GetFileAttributesA(narrow);
}

/* ----------------------------------------------------------------- windows */

namespace
{
	std::mutex& ClientSizeLock()
	{
		static std::mutex lock;
		return lock;
	}

	/* Deliberately a flat list rather than a map: the game creates one window,
	   the map editor two, and a linear scan of three entries under a lock costs
	   nothing next to being able to read this in a debugger. */
	struct ClientSize
	{
		HWND window;
		long width;
		long height;
	};

	std::vector<ClientSize>& ClientSizes()
	{
		static std::vector<ClientSize> sizes;
		return sizes;
	}
} /* namespace */

void RegisterClientSize(HWND window, long width, long height)
{
	std::lock_guard<std::mutex> guard(ClientSizeLock());

	std::vector<ClientSize>& sizes = ClientSizes();
	for (size_t i = 0; i < sizes.size(); ++i)
		if (sizes[i].window == window)
		{
			sizes[i].width = width;
			sizes[i].height = height;
			return;
		}

	const ClientSize added = { window, width, height };
	sizes.push_back(added);
}

BOOL GetClientRect(HWND window, LPRECT rect)
{
	if (!rect)
		return FALSE;

	/* Win32 zeroes the rect and returns FALSE for a bad handle. Callers here
	   divide by the width to form an aspect ratio, so leaving it untouched
	   would hand them whatever was on the stack. */
	rect->left = 0;
	rect->top = 0;
	rect->right = 0;
	rect->bottom = 0;

	std::lock_guard<std::mutex> guard(ClientSizeLock());

	const std::vector<ClientSize>& sizes = ClientSizes();
	for (size_t i = 0; i < sizes.size(); ++i)
		if (sizes[i].window == window)
		{
			rect->right = sizes[i].width;
			rect->bottom = sizes[i].height;
			return TRUE;
		}

	return FALSE;
}

/* ----------------------------------------------------------------- locale */

/*
 * GameMode::AutodetectLanguage takes PRIMARYLANGID of this and looks it up
 * against the three languages the game ships -- English (9), Russian (25) and
 * Portuguese (22), from GameMode.cpp:1105-1124. Anything it does not recognise
 * leaves the language at its default, so returning a real answer for those
 * three and English otherwise is the whole job.
 *
 * The source is the POSIX locale environment rather than CFLocale, which keeps
 * this file free of Objective-C and Foundation. LANG is what the terminal and
 * the launcher both set; LC_ALL wins over it when present, as POSIX says.
 */
LANGID GetUserDefaultUILanguage(void)
{
	const char* locale = getenv("LC_ALL");
	if (!locale || !*locale)
		locale = getenv("LANG");
	if (!locale || !*locale)
		return 9; /* LANG_ENGLISH */

	/* Compare only the two-letter language, not the territory: pt_BR and pt_PT
	   are one language as far as the game is concerned. */
	if (strncmp(locale, "ru", 2) == 0)
		return 25; /* LANG_RUSSIAN */
	if (strncmp(locale, "pt", 2) == 0)
		return 22; /* LANG_PORTUGUESE */

	return 9;
}

/* Windows sets the CRT's per-thread locale for the multibyte conversions the
   ANSI functions do. Everything in this tree is UTF-8 since the transcode, and
   GameMode calls setlocale() on the same line -- which is the call that
   actually changes anything. These two succeed and do nothing. */
BOOL SetThreadLocale(DWORD)
{
	return TRUE;
}

int _setmbcp(int)
{
	return 0;
}

/* -------------------------------------------------------------------- COM */

HRESULT CoInitializeEx(void*, DWORD)
{
	return S_OK;
}

void CoUninitialize(void)
{
}

/* ---------------------------------------------------------------- threads */

/* See the header: the TSC-coherence hazard this guards against on Windows does
   not exist here, so accepting the mask and reporting the previous one -- which
   is what Windows returns -- is accurate rather than a stub. */

HANDLE GetCurrentThread(void)
{
	/* Windows returns a pseudo-handle meaning "the calling thread". The only
	   consumer passes it straight to SetThreadAffinityMask, which ignores it. */
	return reinterpret_cast<HANDLE>(-2);
}

ULONG_PTR SetThreadAffinityMask(HANDLE, ULONG_PTR)
{
	/* Non-zero is success; the value is the previous mask, and every core was
	   permitted before and still is. */
	return ~static_cast<ULONG_PTR>(0);
}

/* ------------------------------------------------------------- multimedia */

/* See mmsystem.h: there is no global timer resolution to raise here, and the
   fine-grained behaviour these ask for is what nanosleep and steady_clock
   already provide. Accepting the period and succeeding is accurate. */

MMRESULT timeBeginPeriod(UINT)
{
	return TIMERR_NOERROR;
}

MMRESULT timeEndPeriod(UINT)
{
	return TIMERR_NOERROR;
}

/* ------------------------------------------------------------------- misc */

int MulDiv(int number, int numerator, int denominator)
{
	/* Win32 rounds the quotient to nearest, away from zero on a tie, and
	   returns -1 rather than trapping. Plain integer division truncates, which
	   would quietly cost a pixel of font height at some DPI values. */
	if (denominator == 0)
		return -1;

	const long long product = static_cast<long long>(number) * numerator;
	const bool negative = (product < 0) != (denominator < 0);

	/* Rounded on magnitudes, so the tie breaks away from zero in both signs.
	   A tie only arises when the denominator is even, and then d / 2 is exact. */
	const unsigned long long p = product < 0
	                                 ? 0ULL - static_cast<unsigned long long>(product)
	                                 : static_cast<unsigned long long>(product);
	const unsigned long long d = denominator < 0
	                                 ? 0ULL - static_cast<unsigned long long>(denominator)
	                                 : static_cast<unsigned long long>(denominator);
	const unsigned long long rounded = (p + d / 2) / d;

	if (rounded > (negative ? 2147483648ULL : 2147483647ULL))
		return -1;

	return negative ? -static_cast<int>(rounded - 1) - 1 : static_cast<int>(rounded);
}

/* --------------------------------------------------------- device contexts */

/* There is no GDI here, and the one caller only wants a handle to hand straight
   back to GetDeviceCaps. A distinguishable non-null value is the whole contract:
   NULL would read as failure. */
static int TheScreenDC = 0;

HDC GetDC(HWND)
{
	return &TheScreenDC;
}

int ReleaseDC(HWND, HDC)
{
	return 1;
}

int GetDeviceCaps(HDC, int index)
{
	/* 96, because that is what Windows answers on a default display, and the
	   number is not a display property here -- it is a font size. Engine.cpp
	   turns it into -MulDiv(9, dpi, 72), so 96 reproduces the same 12-pixel
	   font the game has always drawn its FPS counter in. A Retina backing
	   scale is the Metal layer's business, not this one's. */
	if (index == LOGPIXELSY)
		return 96;

	return 0;
}

/* ------------------------------------------------------------------ timing */

DWORD GetTickCount(void)
{
	const auto elapsed = std::chrono::steady_clock::now() - ProcessStart();
	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
	/* Truncates to 32 bits, as Win32 does. */
	return static_cast<DWORD>(ms);
}

BOOL QueryPerformanceCounter(LARGE_INTEGER* count)
{
	if (!count) return FALSE;
	const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
	count->QuadPart = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
	return TRUE;
}

BOOL QueryPerformanceFrequency(LARGE_INTEGER* frequency)
{
	if (!frequency) return FALSE;
	frequency->QuadPart = 1000000000; /* the counter above is in nanoseconds */
	return TRUE;
}

void Sleep(DWORD milliseconds)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

/* ----------------------------------------------------------------- strings */

int MultiByteToWideChar(UINT, DWORD, LPCSTR src, int srcLen,
                        LPWSTR dst, int dstLen)
{
	if (!src) return 0;
	const size_t len = (srcLen < 0) ? strlen(src) : static_cast<size_t>(srcLen);

	int produced = 0;
	size_t pos = 0;
	while (pos < len)
	{
		uint32_t code = 0;
		DecodeUtf8(src, len, pos, code);
		if (dst)
		{
			if (produced >= dstLen) return 0;
			dst[produced] = static_cast<wchar_t>(code);
		}
		++produced;
	}

	if (srcLen < 0)
	{
		/* A negative length means the terminator is included in the count. */
		if (dst)
		{
			if (produced >= dstLen) return 0;
			dst[produced] = L'\0';
		}
		++produced;
	}

	return produced;
}

int WideCharToMultiByte(UINT, DWORD, LPCWSTR src, int srcLen,
                        LPSTR dst, int dstLen, LPCSTR, BOOL*)
{
	if (!src) return 0;
	size_t len;
	if (srcLen < 0)
	{
		len = 0;
		while (src[len]) ++len;
	}
	else
	{
		len = static_cast<size_t>(srcLen);
	}

	int produced = 0;
	for (size_t i = 0; i < len; ++i)
	{
		char scratch[4];
		const size_t n = EncodeUtf8(static_cast<uint32_t>(src[i]), dst ? scratch : nullptr);
		if (dst)
		{
			if (produced + static_cast<int>(n) > dstLen) return 0;
			memcpy(dst + produced, scratch, n);
		}
		produced += static_cast<int>(n);
	}

	if (srcLen < 0)
	{
		if (dst)
		{
			if (produced >= dstLen) return 0;
			dst[produced] = '\0';
		}
		++produced;
	}

	return produced;
}

/* ------------------------------------------------------------------ module */

DWORD GetModuleFileNameA(void*, LPSTR filename, DWORD size)
{
	if (!filename || size == 0) return 0;
	uint32_t needed = size;
	if (_NSGetExecutablePath(filename, &needed) != 0)
	{
		filename[0] = '\0';
		return 0;
	}
	return static_cast<DWORD>(strlen(filename));
}

DWORD GetModuleFileNameW(void*, LPWSTR filename, DWORD size)
{
	if (!filename || size == 0) return 0;

	char narrow[4096];
	uint32_t needed = sizeof(narrow);
	if (_NSGetExecutablePath(narrow, &needed) != 0)
	{
		filename[0] = L'\0';
		return 0;
	}

	const int produced = MultiByteToWideChar(CP_UTF8, 0, narrow, -1,
	                                         filename, static_cast<int>(size));
	return produced > 0 ? static_cast<DWORD>(produced - 1) : 0;
}

/* -------------------------------------------------------------------- misc */

int MessageBoxA(void*, LPCSTR text, LPCSTR caption, UINT type)
{
	fprintf(stderr, "[%s] %s\n", caption ? caption : "", text ? text : "");
	fflush(stderr);

	/*
	 * There is no dialog to answer with. IDIGNORE is the choice that lets the
	 * program keep running, which is what an unattended build wants; the text
	 * has already been printed, so nothing is lost by not stopping.
	 */
	if (type & MB_ABORTRETRYIGNORE)
		return IDIGNORE;
	return IDOK;
}

void OutputDebugStringA(LPCSTR text)
{
	if (!text) return;
	fputs(text, stderr);
	fflush(stderr);
}

DWORD GetLastError(void)         { return gLastError; }
void  SetLastError(DWORD error)  { gLastError = error; }

} /* extern "C" */
