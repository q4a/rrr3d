/*
 * Non-Windows implementations of the Win32 calls declared in xplatform.h.
 *
 * See the header for the rationale: these are real implementations backed by
 * C++17 facilities, so behaviour matches the Windows build rather than
 * silently degrading.
 */

#include "xplatform.h"

#include <cctype>
#include <cerrno>
#include <map>
#include <sys/mman.h>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <thread>

#include <codecvt>
#include <locale>

#include <mach-o/dyld.h>

namespace {

using SteadyClock = std::chrono::steady_clock;

SteadyClock::time_point ProcessStart()
{
	static const SteadyClock::time_point start = SteadyClock::now();
	return start;
}

struct Event
{
	std::mutex mutex;
	std::condition_variable cond;
	bool signalled = false;
	bool manualReset = false;
};

/*
 * std::wstring on macOS is UTF-32. The project's wide strings only ever hold
 * filesystem paths, so convert through UTF-8 rather than assuming UTF-16.
 */
std::string ToUtf8(const wchar_t* src, int srcLen)
{
	std::wstring in = (srcLen < 0) ? std::wstring(src) : std::wstring(src, src + srcLen);
	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> conv;
	return conv.to_bytes(in);
}

std::wstring FromUtf8(const char* src, int srcLen)
{
	std::string in = (srcLen < 0) ? std::string(src) : std::string(src, src + srcLen);
	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> conv;
	return conv.from_bytes(in);
}

} // namespace

/* ---- timing ---- */

BOOL QueryPerformanceCounter(LARGE_INTEGER* count)
{
	if (!count)
		return FALSE;
	const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
		SteadyClock::now() - ProcessStart()).count();
	count->QuadPart = static_cast<int64_t>(ns);
	return TRUE;
}

BOOL QueryPerformanceFrequency(LARGE_INTEGER* freq)
{
	if (!freq)
		return FALSE;
	/* QueryPerformanceCounter above reports nanoseconds. */
	freq->QuadPart = 1000000000LL;
	return TRUE;
}

DWORD GetTickCount(void)
{
	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		SteadyClock::now() - ProcessStart()).count();
	return static_cast<DWORD>(ms);
}

void Sleep(DWORD milliseconds)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

/* ---- critical sections ---- */

/*
 * Win32 critical sections are recursive; std::mutex is not, so this must be
 * a recursive_mutex or the engine's nested locks would deadlock.
 */
void InitializeCriticalSection(RTL_CRITICAL_SECTION* cs)
{
	cs->opaque = new std::recursive_mutex();
}

void DeleteCriticalSection(RTL_CRITICAL_SECTION* cs)
{
	delete static_cast<std::recursive_mutex*>(cs->opaque);
	cs->opaque = nullptr;
}

void EnterCriticalSection(RTL_CRITICAL_SECTION* cs)
{
	static_cast<std::recursive_mutex*>(cs->opaque)->lock();
}

void LeaveCriticalSection(RTL_CRITICAL_SECTION* cs)
{
	static_cast<std::recursive_mutex*>(cs->opaque)->unlock();
}

/* ---- events ---- */

HANDLE CreateEvent(void*, BOOL manualReset, BOOL initialState, const char*)
{
	Event* event = new Event();
	event->manualReset = (manualReset != FALSE);
	event->signalled = (initialState != FALSE);
	return event;
}

BOOL SetEvent(HANDLE handle)
{
	Event* event = static_cast<Event*>(handle);
	{
		std::lock_guard<std::mutex> lock(event->mutex);
		event->signalled = true;
	}
	if (event->manualReset)
		event->cond.notify_all();
	else
		event->cond.notify_one();
	return TRUE;
}

BOOL ResetEvent(HANDLE handle)
{
	Event* event = static_cast<Event*>(handle);
	std::lock_guard<std::mutex> lock(event->mutex);
	event->signalled = false;
	return TRUE;
}

DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds)
{
	Event* event = static_cast<Event*>(handle);
	std::unique_lock<std::mutex> lock(event->mutex);

	if (milliseconds == INFINITE)
	{
		event->cond.wait(lock, [event] { return event->signalled; });
	}
	else if (!event->cond.wait_for(lock, std::chrono::milliseconds(milliseconds),
	                               [event] { return event->signalled; }))
	{
		return WAIT_TIMEOUT;
	}

	if (!event->manualReset)
		event->signalled = false;
	return WAIT_OBJECT_0;
}

BOOL CloseHandle(HANDLE handle)
{
	delete static_cast<Event*>(handle);
	return TRUE;
}

/* ---- thread pool ---- */

/*
 * The only caller is LexStd's Win32ThreadPool, which queues a small number of
 * long-running decode jobs (WT_EXECUTELONGFUNCTION). A detached thread per
 * item matches that usage; this is not a general-purpose pool.
 */
BOOL QueueUserWorkItem(LPTHREAD_START_ROUTINE func, void* context, unsigned long)
{
	try
	{
		std::thread(func, context).detach();
	}
	catch (const std::system_error&)
	{
		return FALSE;
	}
	return TRUE;
}

/* ---- encoding conversion ---- */

int MultiByteToWideChar(UINT, DWORD, const char* src, int srcLen,
                        wchar_t* dst, int dstLen)
{
	const std::wstring out = FromUtf8(src, srcLen);
	const int needed = static_cast<int>(out.size()) + (srcLen < 0 ? 1 : 0);
	if (dstLen == 0 || dst == nullptr)
		return needed;
	if (needed > dstLen)
		return 0;
	std::memcpy(dst, out.c_str(), needed * sizeof(wchar_t));
	return needed;
}

int WideCharToMultiByte(UINT, DWORD, const wchar_t* src, int srcLen,
                        char* dst, int dstLen, const char*, BOOL* usedDefault)
{
	if (usedDefault)
		*usedDefault = FALSE;
	const std::string out = ToUtf8(src, srcLen);
	const int needed = static_cast<int>(out.size()) + (srcLen < 0 ? 1 : 0);
	if (dstLen == 0 || dst == nullptr)
		return needed;
	if (needed > dstLen)
		return 0;
	std::memcpy(dst, out.c_str(), needed);
	return needed;
}

/* ---- filesystem ---- */

DWORD GetFileAttributesW(const wchar_t* path)
{
	std::error_code ec;
	const std::filesystem::path p(ToUtf8(path, -1));
	const auto status = std::filesystem::status(p, ec);
	if (ec || !std::filesystem::exists(status))
		return INVALID_FILE_ATTRIBUTES;
	return std::filesystem::is_directory(status) ? FILE_ATTRIBUTE_DIRECTORY : 0;
}

DWORD GetModuleFileNameW(void*, wchar_t* buf, DWORD size)
{
	char path[4096];
	uint32_t len = sizeof(path);
	if (_NSGetExecutablePath(path, &len) != 0)
		return 0;

	const std::wstring wide = FromUtf8(path, -1);
	const DWORD copied = static_cast<DWORD>(
		wide.size() < size ? wide.size() : (size ? size - 1 : 0));
	std::memcpy(buf, wide.c_str(), copied * sizeof(wchar_t));
	if (size)
		buf[copied] = L'\0';
	return copied;
}

/* ---- misc ---- */

/*
 * There is no dialog here, so the message goes to stderr. Returning IDIGNORE
 * makes the assert handler in lslException.cpp take its "Ignore" branch and
 * carry on, which is the only sensible choice without a user to ask -- the
 * alternatives are aborting the process or breaking into a debugger.
 */
int MessageBox(HWND, const char* text, const char* caption, UINT)
{
	std::fprintf(stderr, "[%s] %s\n", caption ? caption : "", text ? text : "");
	return IDIGNORE;
}

void OutputDebugStringA(const char* str)
{
	std::fputs(str ? str : "", stderr);
}

/*
 * VirtualAlloc/VirtualFree over mmap.
 *
 * mmap gives page-aligned zero-filled pages, which is what VirtualAlloc
 * promises and what Metal's newBufferWithBytesNoCopy requires. The size map
 * exists because VirtualFree(p, 0, MEM_RELEASE) means "release the whole
 * original reservation" and does not pass a length, where munmap must have one.
 * Storing the size in a header before the pointer would break the page
 * alignment the caller depends on.
 *
 * Allocations here are pooled buffer arenas, so the map is touched rarely and a
 * plain mutex is not worth improving on.
 */
namespace {

std::mutex& VirtualAllocMutex()
{
	static std::mutex mutex;
	return mutex;
}

std::map<void*, size_t>& VirtualAllocSizes()
{
	static std::map<void*, size_t> sizes;
	return sizes;
}

}

void* VirtualAlloc(void* address, size_t size, DWORD allocationType, DWORD protect)
{
	/* The backend only ever reserves and commits in one call. */
	if (address || size == 0 || !(allocationType & MEM_COMMIT))
		return nullptr;

	int prot = PROT_NONE;
	if (protect == PAGE_READONLY)
		prot = PROT_READ;
	else if (protect == PAGE_READWRITE)
		prot = PROT_READ | PROT_WRITE;

	void* mem = mmap(nullptr, size, prot, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (mem == MAP_FAILED)
		return nullptr;

	std::lock_guard<std::mutex> lock(VirtualAllocMutex());
	VirtualAllocSizes()[mem] = size;
	return mem;
}

BOOL VirtualFree(void* address, size_t size, DWORD freeType)
{
	if (!address)
		return FALSE;

	if (freeType & MEM_RELEASE)
	{
		std::lock_guard<std::mutex> lock(VirtualAllocMutex());
		auto iter = VirtualAllocSizes().find(address);
		if (iter == VirtualAllocSizes().end())
			return FALSE;

		const size_t length = iter->second;
		VirtualAllocSizes().erase(iter);
		return munmap(address, length) == 0 ? TRUE : FALSE;
	}

	/* MEM_DECOMMIT: hand the pages back but keep the reservation. */
	return madvise(address, size, MADV_FREE) == 0 ? TRUE : FALSE;
}

HMODULE LoadLibraryA(const char*)
{
	/* No PE loader, and nothing here is packaged as a DLL. */
	return nullptr;
}

FARPROC GetProcAddress(HMODULE, const char*)
{
	return nullptr;
}

BOOL FreeLibrary(HMODULE)
{
	return TRUE;
}

HDC CreateCompatibleDC(HDC)
{
	return nullptr;
}

BOOL DeleteDC(HDC)
{
	return TRUE;
}

SHORT GetAsyncKeyState(int)
{
	return 0;
}

BOOL SetWindowPos(HWND, HWND, int, int, int, int, UINT)
{
	return TRUE;
}

LONG GetWindowLong(HWND, int)
{
	return 0;
}

LONG SetWindowLong(HWND, int, LONG)
{
	return 0;
}

BOOL GetWindowInfo(HWND, PWINDOWINFO info)
{
	if (!info)
		return FALSE;

	std::memset(info, 0, sizeof(*info));
	info->cbSize = sizeof(*info);
	return TRUE;
}

BOOL InvalidateRect(HWND, const RECT*, BOOL)
{
	return TRUE;
}

BOOL UpdateWindow(HWND)
{
	return TRUE;
}

BOOL AdjustWindowRect(RECT*, DWORD, BOOL)
{
	return TRUE;
}

HANDLE GetCurrentThread(void)
{
	return NULL;
}

ULONG_PTR SetThreadAffinityMask(HANDLE, ULONG_PTR)
{
	/* Non-zero is success; the previous mask is what Windows returns. */
	return 1;
}

BOOL GetCursorPos(POINT* point)
{
	if (!point)
		return FALSE;
	point->x = 0;
	point->y = 0;
	return TRUE;
}

BOOL ScreenToClient(HWND, POINT*)
{
	/* No window, so screen and client coordinates coincide. */
	return TRUE;
}

BOOL IsCharAlpha(char ch)
{
	return std::isalpha(static_cast<unsigned char>(ch)) ? TRUE : FALSE;
}

BOOL IsCharAlphaNumeric(char ch)
{
	return std::isalnum(static_cast<unsigned char>(ch)) ? TRUE : FALSE;
}

int _setmbcp(int)
{
	return 0;
}

BOOL SetThreadLocale(DWORD)
{
	return TRUE;
}

BOOL EnumDisplayDevices(const char*, DWORD, PDISPLAY_DEVICE displayDevice, DWORD)
{
	/* No displays enumerated; the caller keeps its configured mode. */
	if (displayDevice)
	{
		const DWORD cb = displayDevice->cb;
		std::memset(displayDevice, 0, sizeof(*displayDevice));
		displayDevice->cb = cb;
	}
	return FALSE;
}

LANGID GetUserDefaultUILanguage(void)
{
	return MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
}

HRESULT CoInitializeEx(void*, DWORD)
{
	return S_OK;
}

void CoUninitialize(void)
{
}

int _wfopen_s(FILE** file, const wchar_t* path, const wchar_t* mode)
{
	if (!file)
		return EINVAL;

	*file = std::fopen(ToUtf8(path, -1).c_str(), ToUtf8(mode, -1).c_str());
	return *file ? 0 : errno;
}

UINT timeBeginPeriod(UINT)
{
	return TIMERR_NOERROR;
}

UINT timeEndPeriod(UINT)
{
	return TIMERR_NOERROR;
}

namespace
{

/*
 * The client size of the one window this game opens.
 *
 * There is no window system behind HWND here -- the handle is whatever the
 * shell put in IView::Desc::handle -- so the size cannot be queried from it and
 * has to be published by whoever created the window. RegisterClientSize is that
 * publication, called from src/RRR3d/sdl_shell.cpp.
 *
 * Returning zero here, which is what this did before, is not harmless: View::
 * ScreenToView divides the mouse position by the client size, so a zero size
 * turns every click coordinate into infinity and the game stops responding to
 * the mouse entirely, with nothing logged.
 */
int clientWidth = 0;
int clientHeight = 0;

}

void RegisterClientSize(HWND, int width, int height)
{
	clientWidth = width;
	clientHeight = height;
}

BOOL GetClientRect(HWND, RECT* rect)
{
	if (!rect)
		return FALSE;

	rect->left = 0;
	rect->top = 0;
	rect->right = clientWidth;
	rect->bottom = clientHeight;
	return TRUE;
}
