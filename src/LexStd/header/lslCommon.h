#ifndef LSL_COMMON
#define LSL_COMMON

// The debug heap below is the MSVC CRT's (_malloc_dbg / _free_dbg / crtdbg.h),
// and the redefinition of new only matches MSVC's debug operator new. There is
// no equivalent on clang, so leave DEBUG_MEMORY undefined there and let the
// blocks guarded by it compile out.
#if defined(_DEBUG) && defined(_MSC_VER)
	#define DEBUG_MEMORY
#endif

#ifdef DEBUG_MEMORY
	#include <crtdbg.h>
	#define _CRTDBG_MAP_ALLOC

	#define malloc(size) _malloc_dbg(size, _NORMAL_BLOCK, __FILE__, __LINE__)
	#define free(size) _free_dbg(size, _NORMAL_BLOCK)
#endif

#include <map>
#include <iostream>

#ifdef DEBUG_MEMORY
	#define new new( _NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#include <string>
#include <algorithm>
#include <sstream>
#include <cstdlib>
#include <string>
#include <bitset>
#include <vector>
#include <limits>
#include <exception>
#include <list>
#include <cstdio>
#include <iterator>
#include <memory>
#include <utility>

#endif