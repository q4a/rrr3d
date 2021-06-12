#ifndef LSL_COMMON
#define LSL_COMMON

#ifdef _DEBUG
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
#include "d3d\d3dx9math.h"

#ifdef DEBUG_MEMORY
	#define new new( _NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#include <string>
#include <algorithm>
#include <sstream>
#include <cstdlib>
#include <Unknwn.h>
#include <string>
#include <bitset>
#include <vector>
#include <limits>
#include <exception>
#include <list>
#include <cstdio>

#endif