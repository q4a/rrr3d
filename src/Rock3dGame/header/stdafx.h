#pragma once

#include "MathCommon.h"
#include "lslCommon.h"

#include <windows.h>
//Only timeBeginPeriod/timeEndPeriod are used from here, by World.cpp's frame
//limiter. XPlatform substitutes those; see xplatform.h.
#ifdef _WIN32
	#include <MMSystem.h>
#endif

#include "targetver.h"
#include "lslObject.h"
#include "r3dMath.h"
#include "lslUtility.h"
#include "lslException.h"
#include "lslSDK.h"
#include "EulerAngles.h"

#include "GraphManager.h"
#include "px/Physx.h"

#ifndef _WIN32
//PhysX 3+ scopes everything in namespace physx where the 2.8 Nx* types were
//global. px/Physx.h imports it inside r3d::px; the game layer names the same
//types directly, so it is imported here too. Verified that nothing this project
//declares collides with a PhysX 4.1 type name.
using namespace physx;
#endif
#include "net/NetLib.h"
#include "IWorld.h"

//#define STEAM_SERVICE

#ifdef _DEBUG
	#define DEBUG_PX 1
#else
	//#define DEBUG_PX 1
	//#define _RETAIL
#endif

#ifdef STEAM_SERVICE
	#pragma comment(lib, "steam_api.lib")
#endif