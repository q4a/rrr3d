#include "xplatform.h"

#include "mmsystem.h"
#include "wingdi.h"
#include "winuser.h"
#include "xinput.h"

#include <SDL3/SDL.h>

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include <cstdlib>

#include <dlfcn.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include <sys/mman.h>
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

BOOL SetCurrentDirectoryA(LPCSTR path)
{
	return path && chdir(path) == 0 ? TRUE : FALSE;
}

BOOL SetCurrentDirectoryW(LPCWSTR path)
{
	if (!path)
		return FALSE;

	char narrow[4096];
	if (WideCharToMultiByte(CP_UTF8, 0, path, -1, narrow,
	                        static_cast<int>(sizeof(narrow)), nullptr, nullptr) == 0)
		return FALSE;

	return SetCurrentDirectoryA(narrow);
}

/* ----------------------------------------------------------------- windows */

/* extern "C++" because this file's definitions sit inside one big extern "C"
   block, and an internal helper returning a std::mutex& is not a C function. */
extern "C++" {
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
} /* extern "C++" */

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

/* ------------------------------------------------------------------- input */

namespace
{
	/*
	 * The one keyboard table, in both directions.
	 *
	 * Only the keys the game names are here: the engine compares against VK_
	 * constants -- Player's controls, the menu's escape handling, AIPlayer's
	 * debug keys -- so VK_ is the target alphabet and anything unlisted is
	 * dropped, which is what the Win32 shell effectively did with keys no
	 * handler matched.
	 */
	struct KeyMapping
	{
		SDL_Scancode scancode;
		int virtualKey;
	};

	const KeyMapping cKeyMap[] =
	{
		{ SDL_SCANCODE_BACKSPACE,  VK_BACK },
		{ SDL_SCANCODE_RETURN,     VK_RETURN },
		{ SDL_SCANCODE_KP_ENTER,   VK_RETURN },
		{ SDL_SCANCODE_LCTRL,      VK_CONTROL },
		{ SDL_SCANCODE_RCTRL,      VK_CONTROL },
		{ SDL_SCANCODE_ESCAPE,     VK_ESCAPE },
		{ SDL_SCANCODE_SPACE,      VK_SPACE },
		{ SDL_SCANCODE_PAGEUP,     VK_PRIOR },
		{ SDL_SCANCODE_PAGEDOWN,   VK_NEXT },
		{ SDL_SCANCODE_LEFT,       VK_LEFT },
		{ SDL_SCANCODE_UP,         VK_UP },
		{ SDL_SCANCODE_RIGHT,      VK_RIGHT },
		{ SDL_SCANCODE_DOWN,       VK_DOWN },
		{ SDL_SCANCODE_DELETE,     VK_DELETE },
		{ SDL_SCANCODE_KP_0,       VK_NUMPAD0 },
		{ SDL_SCANCODE_KP_1,       VK_NUMPAD1 },
		{ SDL_SCANCODE_KP_2,       VK_NUMPAD2 },
		{ SDL_SCANCODE_KP_3,       VK_NUMPAD3 },
		{ SDL_SCANCODE_KP_4,       VK_NUMPAD4 },
		{ SDL_SCANCODE_KP_5,       VK_NUMPAD5 },
		{ SDL_SCANCODE_KP_6,       VK_NUMPAD6 },
		{ SDL_SCANCODE_KP_7,       VK_NUMPAD7 },
		{ SDL_SCANCODE_KP_8,       VK_NUMPAD8 },
		{ SDL_SCANCODE_KP_9,       VK_NUMPAD9 },
		{ SDL_SCANCODE_KP_PLUS,    VK_ADD },
		{ SDL_SCANCODE_KP_MINUS,   VK_SUBTRACT },
		{ SDL_SCANCODE_F1,         VK_F1 },
		{ SDL_SCANCODE_F2,         VK_F2 },
		{ SDL_SCANCODE_F3,         VK_F3 },
		{ SDL_SCANCODE_F4,         VK_F4 },
		{ SDL_SCANCODE_F5,         VK_F5 },
		{ SDL_SCANCODE_F6,         VK_F6 },
		{ SDL_SCANCODE_F7,         VK_F7 },
		{ SDL_SCANCODE_PERIOD,     VK_OEM_PERIOD },
	};

	const size_t cKeyMapCount = sizeof(cKeyMap) / sizeof(cKeyMap[0]);
}

/*
 * Letters and digits are handled by range rather than by table: their virtual
 * keys are their own ASCII codes, which is what makes 'W' work without an
 * entry each.
 */
int VirtualKeyFromScancode(int scancode)
{
	for (size_t i = 0; i < cKeyMapCount; ++i)
		if (cKeyMap[i].scancode == scancode)
			return cKeyMap[i].virtualKey;

	if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z)
		return 'A' + (scancode - SDL_SCANCODE_A);

	if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9)
		return '1' + (scancode - SDL_SCANCODE_1);

	if (scancode == SDL_SCANCODE_0)
		return '0';

	return 0;
}

int ScancodeFromVirtualKey(int virtualKey)
{
	/* First match wins, so VK_RETURN maps back to the main Return key rather
	   than the keypad one -- polled state should follow the key a player is
	   most likely holding. */
	for (size_t i = 0; i < cKeyMapCount; ++i)
		if (cKeyMap[i].virtualKey == virtualKey)
			return cKeyMap[i].scancode;

	if (virtualKey >= 'A' && virtualKey <= 'Z')
		return SDL_SCANCODE_A + (virtualKey - 'A');

	if (virtualKey >= '1' && virtualKey <= '9')
		return SDL_SCANCODE_1 + (virtualKey - '1');

	if (virtualKey == '0')
		return SDL_SCANCODE_0;

	return SDL_SCANCODE_UNKNOWN;
}

/*
 * The polled half of input. The engine reads keyboard and mouse state directly
 * as well as receiving events, and SDL keeps both available.
 *
 * SDL_GetKeyboardState is by SCANCODE, for the same reason the shell maps by
 * scancode: the physical key is what the game's controls mean.
 */
SHORT WINAPI GetAsyncKeyState(int virtualKey)
{
	/* Mouse buttons live in the same numbering as virtual keys on Windows, and
	   the engine relies on it. */
	if (virtualKey == VK_LBUTTON || virtualKey == VK_RBUTTON || virtualKey == VK_MBUTTON)
	{
		const SDL_MouseButtonFlags buttons = SDL_GetMouseState(NULL, NULL);
		const SDL_MouseButtonFlags wanted =
			virtualKey == VK_LBUTTON ? SDL_BUTTON_LMASK :
			virtualKey == VK_RBUTTON ? SDL_BUTTON_RMASK : SDL_BUTTON_MMASK;

		return (buttons & wanted) ? static_cast<SHORT>(0x8000) : 0;
	}

	const int scancode = ScancodeFromVirtualKey(virtualKey);
	if (scancode == SDL_SCANCODE_UNKNOWN)
		return 0;

	int count = 0;
	const bool* keys = SDL_GetKeyboardState(&count);
	if (!keys || scancode >= count)
		return 0;

	/* The high bit is "down". The low bit -- "pressed since the last call" --
	   has no SDL equivalent and the engine only ever tests the high one. */
	return keys[scancode] ? static_cast<SHORT>(0x8000) : 0;
}

BOOL WINAPI GetCursorPos(LPPOINT point)
{
	if (!point)
		return FALSE;

	float x = 0.0f;
	float y = 0.0f;
	SDL_GetGlobalMouseState(&x, &y);

	point->x = static_cast<LONG>(x);
	point->y = static_cast<LONG>(y);

	return TRUE;
}

/* Screen to client, answered from the window's position rather than from the
   handle -- which is a CAMetalLayer and cannot be asked. */
BOOL WINAPI ScreenToClient(HWND, LPPOINT point)
{
	if (!point)
		return FALSE;

	float globalX = 0.0f;
	float globalY = 0.0f;
	SDL_GetGlobalMouseState(&globalX, &globalY);

	float windowX = 0.0f;
	float windowY = 0.0f;
	SDL_GetMouseState(&windowX, &windowY);

	/* The difference between the two is the window's origin, which avoids
	   needing the SDL_Window here at all. */
	point->x -= static_cast<LONG>(globalX - windowX);
	point->y -= static_cast<LONG>(globalY - windowY);

	return TRUE;
}

/* Locale-aware on Windows. The game uses the pair to decide whether a key press
   is printable for text entry, and every language it ships -- English, Russian,
   Portuguese -- is covered by treating the input as UTF-8 bytes: a byte with
   the high bit set is part of a multi-byte character and is printable. */
BOOL WINAPI IsCharAlphaA(CHAR ch)
{
	const unsigned char c = static_cast<unsigned char>(ch);

	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c >= 0x80;
}

BOOL WINAPI IsCharAlphaNumericA(CHAR ch)
{
	const unsigned char c = static_cast<unsigned char>(ch);

	return IsCharAlphaA(ch) || (c >= '0' && c <= '9');
}

/* -------------------------------------------------------- dynamic loading */

/*
 * See the header: every caller here is probing for a Windows DLL that does not
 * exist, so failing is the answer rather than a shortfall.
 *
 * dlopen is still used rather than returning NULL unconditionally, because a
 * .dylib name would work and there is no reason to refuse one.
 */
HMODULE LoadLibraryA(LPCSTR name)
{
	return name ? static_cast<HMODULE>(dlopen(name, RTLD_LAZY | RTLD_LOCAL)) : NULL;
}

HMODULE LoadLibraryW(LPCWSTR name)
{
	if (!name)
		return NULL;

	char narrow[4096];
	if (WideCharToMultiByte(CP_UTF8, 0, name, -1, narrow,
	                        static_cast<int>(sizeof(narrow)), nullptr, nullptr) == 0)
		return NULL;

	return LoadLibraryA(narrow);
}

/* GetModuleHandle asks for something already loaded. NULL means "this
   executable", which dlopen(NULL) gives; anything else is a Windows DLL and is
   not here. */
HMODULE GetModuleHandleA(LPCSTR name)
{
	if (!name)
		return static_cast<HMODULE>(dlopen(NULL, RTLD_LAZY | RTLD_LOCAL));

	return static_cast<HMODULE>(dlopen(name, RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD));
}

HMODULE GetModuleHandleW(LPCWSTR name)
{
	if (!name)
		return GetModuleHandleA(NULL);

	char narrow[4096];
	if (WideCharToMultiByte(CP_UTF8, 0, name, -1, narrow,
	                        static_cast<int>(sizeof(narrow)), nullptr, nullptr) == 0)
		return NULL;

	return GetModuleHandleA(narrow);
}

BOOL FreeLibrary(HMODULE module)
{
	return module && dlclose(module) == 0 ? TRUE : FALSE;
}

void* GetProcAddress(HMODULE module, LPCSTR name)
{
	return module && name ? dlsym(module, name) : NULL;
}

/* --------------------------------------------------------- virtual memory */

extern "C++" {
namespace
{
	std::mutex& VirtualAllocMutex()
	{
		static std::mutex lock;
		return lock;
	}

	/*
	 * Address to length, so VirtualFree can call munmap -- which needs a
	 * length that VirtualFree(p, 0, MEM_RELEASE) does not pass.
	 *
	 * A side map rather than a header before the pointer, because a header
	 * would shift the returned address off the page boundary that
	 * newBufferWithBytesNoCopy requires. That alignment is the entire reason
	 * this is not malloc.
	 */
	std::map<void*, size_t>& VirtualAllocSizes()
	{
		static std::map<void*, size_t> sizes;
		return sizes;
	}
}
}

void* VirtualAlloc(void* address, size_t size, DWORD allocationType, DWORD protect)
{
	/* The backend only ever reserves and commits in one call. */
	if (address || size == 0 || !(allocationType & MEM_COMMIT))
		return NULL;

	int prot = PROT_NONE;
	if (protect == PAGE_READONLY)
		prot = PROT_READ;
	else if (protect == PAGE_READWRITE)
		prot = PROT_READ | PROT_WRITE;

	void* mem = mmap(NULL, size, prot, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (mem == MAP_FAILED)
		return NULL;

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

		std::map<void*, size_t>::iterator iter = VirtualAllocSizes().find(address);
		if (iter == VirtualAllocSizes().end())
			return FALSE;

		const size_t length = iter->second;
		VirtualAllocSizes().erase(iter);

		return munmap(address, length) == 0 ? TRUE : FALSE;
	}

	/* MEM_DECOMMIT: hand the pages back but keep the reservation. */
	return madvise(address, size, MADV_FREE) == 0 ? TRUE : FALSE;
}

/* ------------------------------------------------------------------ XInput */

extern "C++" {
namespace
{
	/*
	 * SDL's gamepad API is already an Xbox-shaped abstraction -- that is what
	 * SDL_GAMEPAD_BUTTON_SOUTH and the axis set are -- so this is a relabelling
	 * rather than a translation.
	 *
	 * The scales have to match exactly, because the game compares raw values
	 * against XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE fourteen times: SDL reports
	 * sticks as -32768..32767 and triggers as 0..32767, XInput reports sticks
	 * the same and triggers as 0..255. So triggers are rescaled, sticks are not.
	 */
	SDL_Gamepad* OpenedGamepad(DWORD userIndex)
	{
		int count = 0;
		SDL_JoystickID* ids = SDL_GetGamepads(&count);
		if (!ids)
			return NULL;

		if (userIndex == XUSER_INDEX_ANY)
			userIndex = 0;

		SDL_Gamepad* gamepad = NULL;
		if (static_cast<int>(userIndex) < count)
			gamepad = SDL_OpenGamepad(ids[userIndex]);

		SDL_free(ids);

		return gamepad;
	}

	WORD ReadButtons(SDL_Gamepad* pad)
	{
		WORD buttons = 0;

		if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP))        buttons |= XINPUT_GAMEPAD_DPAD_UP;
		if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN))      buttons |= XINPUT_GAMEPAD_DPAD_DOWN;
		if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT))      buttons |= XINPUT_GAMEPAD_DPAD_LEFT;
		if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT))     buttons |= XINPUT_GAMEPAD_DPAD_RIGHT;
		if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_START))          buttons |= XINPUT_GAMEPAD_START;
		if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_BACK))           buttons |= XINPUT_GAMEPAD_BACK;
		if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_LEFT_STICK))     buttons |= XINPUT_GAMEPAD_LEFT_THUMB;
		if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_STICK))    buttons |= XINPUT_GAMEPAD_RIGHT_THUMB;
		if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER))  buttons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
		if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) buttons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
		if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH))          buttons |= XINPUT_GAMEPAD_A;
		if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST))           buttons |= XINPUT_GAMEPAD_B;
		if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_WEST))           buttons |= XINPUT_GAMEPAD_X;
		if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_NORTH))          buttons |= XINPUT_GAMEPAD_Y;

		return buttons;
	}
}
}

DWORD WINAPI XInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState)
{
	if (!pState)
		return ERROR_DEVICE_NOT_CONNECTED;

	std::memset(pState, 0, sizeof(*pState));

	SDL_Gamepad* pad = OpenedGamepad(dwUserIndex);
	if (!pad)
		return ERROR_DEVICE_NOT_CONNECTED;

	pState->Gamepad.wButtons = ReadButtons(pad);

	pState->Gamepad.sThumbLX = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
	pState->Gamepad.sThumbRX = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX);

	/* Y is inverted between the two: SDL reports down as positive, XInput
	   reports up as positive. Getting this wrong inverts steering. */
	pState->Gamepad.sThumbLY = -SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
	pState->Gamepad.sThumbRY = -SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY);

	/* 0..32767 down to 0..255, because the game compares triggers against
	   XINPUT_GAMEPAD_TRIGGER_THRESHOLD, which is 30 on the XInput scale. */
	pState->Gamepad.bLeftTrigger = static_cast<BYTE>(
		SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) * 255 / 32767);
	pState->Gamepad.bRightTrigger = static_cast<BYTE>(
		SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) * 255 / 32767);

	/* Windows increments this whenever the state changes and the game uses it
	   to skip redundant work. SDL has no counter, so the state always reads as
	   fresh -- which costs work rather than correctness. */
	pState->dwPacketNumber = SDL_GetTicks();

	return ERROR_SUCCESS;
}

/* Buffered gamepad keystrokes. SDL delivers gamepad input as events to the
   shell rather than as a queue to poll here, and ControlManager discards
   everything but the return value. */
DWORD WINAPI XInputGetKeystroke(DWORD, DWORD, PXINPUT_KEYSTROKE pKeystroke)
{
	if (pKeystroke)
		std::memset(pKeystroke, 0, sizeof(*pKeystroke));

	return ERROR_DEVICE_NOT_CONNECTED;
}

/* ----------------------------------------------------------------- windows */

/*
 * The window functions the engine calls on what it thinks is an HWND.
 *
 * None of them can act, because the handle is a CAMetalLayer: fullscreen goes
 * through SDL_SetWindowFullscreen in the shell, and the size the engine asks
 * for comes back from the registry above. They succeed so the engine's own
 * logic runs unchanged rather than being #ifdef'd out.
 */
LONG WINAPI SetWindowLongA(HWND, int, LONG value)
{
	return value;
}

LONG WINAPI GetWindowLongA(HWND, int)
{
	return 0;
}

BOOL WINAPI GetWindowInfo(HWND window, PWINDOWINFO info)
{
	if (!info)
		return FALSE;

	std::memset(info, 0, sizeof(*info));
	info->cbSize = sizeof(*info);

	GetClientRect(window, &info->rcClient);
	info->rcWindow = info->rcClient;

	return TRUE;
}

/* Grows a client rect by the window chrome. There is no chrome to account for
   here -- SDL sizes its windows by their content -- so the rect is unchanged,
   which is also what fullscreen did on Windows. */
BOOL WINAPI AdjustWindowRect(LPRECT, DWORD, BOOL)
{
	return TRUE;
}

BOOL WINAPI SetWindowPos(HWND, HWND, int, int, int, int, UINT)
{
	return TRUE;
}

/* Repainting is driven by the render loop, not by an invalidation queue. */
BOOL WINAPI InvalidateRect(HWND, const RECT*, BOOL)
{
	return TRUE;
}

BOOL WINAPI UpdateWindow(HWND)
{
	return TRUE;
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

/* See wingdi.h: the only caller reaches these through a gdi32.dll that does not
   load, so neither runs. NULL is a failed CreateCompatibleDC on Windows too. */
HDC CreateCompatibleDC(HDC)
{
	return NULL;
}

BOOL DeleteDC(HDC)
{
	return FALSE;
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

DWORD GetModuleFileNameW(void*, LPWSTR filename, DWORD size)
{
	if (!filename || size == 0) return 0;

	char narrow[4096];
#ifdef __APPLE__
	uint32_t needed = sizeof(narrow);
	if (_NSGetExecutablePath(narrow, &needed) != 0)
#else
	ssize_t len = readlink("/proc/self/exe", narrow, sizeof(narrow) - 1);
	if (len == -1)
#endif
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

/*
 * Sanitizer defaults, compiled in rather than left to the environment.
 *
 * Address Sanitizer calls this if it is present, before main, and it is how a
 * program states its own defaults. ASAN_OPTIONS in the environment still wins,
 * so this sets a floor rather than a policy.
 *
 * Both settings exist because the default is neither. By default ASan prints
 * its report and calls _exit(1): no abort, no core, and -- the reason this is
 * here -- a game that dies with a bare exit status looks exactly like a game
 * that was killed by the harness timing it, so a real memory error can be
 * counted as a survivor. halt_on_error stops it continuing past the first
 * report, and abort_on_error turns the exit into a SIGABRT that a debugger
 * catches and a shell reports distinctly.
 *
 * The __attribute__((used)) matters: this is in a shared library that nothing
 * calls this symbol from, and without it the linker is free to drop it.
 */
#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define RRR3D_ASAN 1
#  endif
#endif

#ifdef RRR3D_ASAN
extern "C" __attribute__((used)) const char* __asan_default_options()
{
	return "abort_on_error=1:halt_on_error=1";
}
#endif
