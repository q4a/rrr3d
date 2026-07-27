#pragma once

// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the ROCK3DGAME_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// ROCK3DGAME_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
// __declspec is MSVC-only. Elsewhere a shared library exports through symbol
// visibility, and there is no import side to declare at all.
#ifdef _WIN32
	#ifdef ROCK3DGAME_EXPORTS
	#define ROCK3DGAME_API __declspec(dllexport)
	#else
	#define ROCK3DGAME_API __declspec(dllimport)
	#endif
#else
	#define ROCK3DGAME_API __attribute__((visibility("default")))
#endif

#include "IWorld.h"

#ifndef ROCK3DGAME_EXPORTS
	namespace r3d {using namespace edit;}
#endif

namespace r3d
{

using namespace game;

ROCK3DGAME_API IWorld* CreateWorld(const IView::Desc& viewDesc, bool steamInit);
ROCK3DGAME_API void ReleaseWorld(IWorld* world);

}