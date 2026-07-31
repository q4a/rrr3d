#ifndef XPLATFORM_CRTDBG_H
#define XPLATFORM_CRTDBG_H

/*
 * Stands in for Microsoft's <crtdbg.h>.
 *
 * lslCommon.h turns on DEBUG_MEMORY in Debug builds, which redirects malloc and
 * free to the CRT debug heap and redefines `new` to the four-argument debug
 * form. There is no debug heap here, so the allocation tracking is dropped --
 * but the *shape* is kept, because the redefinition of `new` reaches every
 * translation unit in the engine and px/Physx.h brackets its includes with
 * push_macro/pop_macro on exactly these names. Removing DEBUG_MEMORY instead
 * would mean touching engine headers to work around a header that is trivial
 * to provide.
 *
 * Leak tracking on this platform is the sanitizers' job, not the CRT's.
 */

#include <cstdlib>
#include <new>

#define _NORMAL_BLOCK  1
#define _CLIENT_BLOCK  2
#define _CRT_BLOCK     3
#define _FREE_BLOCK    0
#define _IGNORE_BLOCK  4

#define _malloc_dbg(size, blockType, file, line)          malloc(size)
#define _realloc_dbg(ptr, size, blockType, file, line)    realloc(ptr, size)
#define _calloc_dbg(count, size, blockType, file, line)   calloc(count, size)
#define _free_dbg(ptr, blockType)                         free(ptr)

#define _CrtCheckMemory()          1
#define _CrtDumpMemoryLeaks()      0
#define _CrtSetDbgFlag(flag)       0
#define _CRTDBG_ALLOC_MEM_DF       0
#define _CRTDBG_LEAK_CHECK_DF      0

#define _ASSERT(expr)              ((void)0)
#define _ASSERTE(expr)             ((void)0)

#if defined(__aarch64__) || defined(__arm64__)
	#define _CrtDbgBreak() __builtin_debugtrap()
#else
	#define _CrtDbgBreak() __builtin_trap()
#endif

/*
 * `#define new new(_NORMAL_BLOCK, __FILE__, __LINE__)` needs these to exist.
 * The extra arguments are discarded; the matching deletes have to be declared
 * even though only the compiler calls them, on the path where a constructor
 * throws.
 */
inline void* operator new(std::size_t size, int, const char*, int)
{
	return ::operator new(size);
}

inline void* operator new[](std::size_t size, int, const char*, int)
{
	return ::operator new[](size);
}

inline void operator delete(void* ptr, int, const char*, int) noexcept
{
	::operator delete(ptr);
}

inline void operator delete[](void* ptr, int, const char*, int) noexcept
{
	::operator delete[](ptr);
}

#endif
