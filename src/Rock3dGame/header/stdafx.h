#pragma once

#include "lslCommon.h"

#include <windows.h>
#include <MMSystem.h>

#include "targetver.h"
#include "lslObject.h"
#include "r3dMath.h"
#include "lslUtility.h"
#include "lslException.h"
#include "lslSDK.h"
#include "EulerAngles.h"

#include "GraphManager.h"
#include "px\Physx.h"
#include "net\NetLib.h"
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