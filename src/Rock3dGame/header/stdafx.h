#pragma once

#include "MathCommon.h"
#include "lslCommon.h"

#ifdef _WIN32
#include <windows.h>
#include <MMSystem.h>
#include "targetver.h"
#endif
#include "lslObject.h"
#include "r3dMath.h"
#include "lslUtility.h"
#include "lslException.h"
#include "lslSDK.h"
#include "lslComponent.h"
#include "lslSerialization.h"
#include "EulerAngles.h"

#if defined(_WIN32) || defined(RRR3D_RENDERER_ENABLED)
#include "GraphManager.h"
#endif
#if defined(_WIN32) || defined(RRR3D_PHYSICS_ENABLED)
#include "px/Physx.h"
#endif
#if defined(_WIN32) || defined(RRR3D_NETWORK_ENABLED)
#include "net/NetLib.h"
#endif
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
