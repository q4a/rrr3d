#ifndef NX_PHYSICS_H
#define NX_PHYSICS_H

/*
 * The umbrella header, as PhysX 2.8 has it.
 *
 * This is the file the game includes -- px/Physx.h does `#include
 * "NxPhysics.h"` and nothing else -- so it is the shim's entire public surface
 * as far as the engine is concerned.
 *
 * Which makes one rule matter more than any other here: **no backend header may
 * ever be reachable from this file.** Rock3dGame's stdafx.h pulls px/Physx.h
 * into all 52 of its translation units, so anything visible from here is
 * visible to the whole game. Every live type below is an abstract interface,
 * implemented privately in src/PhysX28/source, precisely so that Bullet stays
 * on the far side of that boundary.
 *
 * The practical test: this header includes only other shim headers and the C++
 * standard library. If a Bullet include ever appears beneath it, the game's
 * build time doubles and its type names start colliding.
 */

#include "NxSimpleTypes.h"
#include "Nxp.h"

/* Value types. */
#include "NxVec3.h"
#include "NxQuat.h"
#include "NxMat33.h"
#include "NxMat34.h"
#include "NxRay.h"
#include "NxArray.h"
#include "NxStream.h"

/* Descriptors. */
#include "NxSpringDesc.h"
#include "NxTireFunctionDesc.h"
#include "NxShapeDesc.h"
#include "NxActorDesc.h"
#include "NxMaterialDesc.h"
#include "NxSceneDesc.h"
#include "NxTriangleMeshDesc.h"

/* Callbacks. */
#include "NxUserContactReport.h"
#include "NxWheelContactData.h"

/* Live objects. */
#include "NxShape.h"
#include "NxActor.h"
#include "NxScene.h"
#include "NxPhysicsSDK.h"

#endif /* NX_PHYSICS_H */
