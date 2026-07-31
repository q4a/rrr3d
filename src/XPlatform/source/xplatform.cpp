#include "xplatform.h"

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>

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
