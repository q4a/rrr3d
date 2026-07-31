#ifndef NX_SCENE_DESC_H
#define NX_SCENE_DESC_H

/*
 * PhysX 2.8's NxSceneDesc.
 *
 * Transcribed from extern/physx/include/Physics/NxSceneDesc.h.
 *
 * maxTimestep defaults to 1/60 and the game never changes it, which settles a
 * question the shim would otherwise have to guess at: World.cpp also steps at
 * 1/60, so 2.8 ran exactly ONE substep per step. That is why
 * NX_SMOOTH_IMPULSE and NX_SMOOTH_VELOCITY_CHANGE -- which spread a change
 * across substeps -- were already identical to their non-smooth counterparts
 * here, and can be mapped straight through. The equivalence is conditional on
 * the step size, so whatever implements it should assert the step rather than
 * assume it.
 *
 * What the game actually sets: gravity, upAxis = 2 (Z-up, not the default 0),
 * timeStepMethod = NX_TIMESTEP_VARIABLE, and the three callbacks.
 */

#include "Nxp.h"
#include "NxSimpleTypes.h"
#include "NxVec3.h"

class NxUserNotify;
class NxUserContactReport;
class NxUserContactModify;
class NxUserTriggerReport;

enum NxSimulationType
	{
	NX_SIMULATION_SW = 0,
	NX_SIMULATION_HW = 1
	};

class NxSceneDesc
	{
	public:
	NxVec3 gravity;
	NxUserNotify*        userNotify;
	NxUserTriggerReport* userTriggerReport;
	NxUserContactReport* userContactReport;
	NxUserContactModify* userContactModify;
	/** 1/60 by default, and the game leaves it there -- see the note above. */
	NxReal               maxTimestep;
	NxU32                maxIter;
	NxTimeStepMethod     timeStepMethod;
	NxSimulationType     simType;
	NxU32                flags;
	/** 0 by default; the game sets 2, for Z-up. */
	NxU32                upAxis;
	NxU32                subdivisionLevel;
	void*                userData;

	NX_INLINE NxSceneDesc() { setToDefault(); }

	NX_INLINE void setToDefault()
		{
		gravity.zero();
		userNotify        = NULL;
		userTriggerReport = NULL;
		userContactReport = NULL;
		userContactModify = NULL;
		maxTimestep       = 1.0f / 60.0f;
		maxIter           = 8;
		timeStepMethod    = NX_TIMESTEP_FIXED;
		simType           = NX_SIMULATION_SW;
		flags             = 0;
		upAxis            = 0;
		subdivisionLevel  = 5;
		userData          = NULL;
		}

	NX_INLINE bool isValid() const
		{
		if (maxTimestep <= 0.0f) return false;
		if (maxIter < 1)         return false;
		if (upAxis > 2)          return false;
		if (!gravity.isFinite()) return false;
		return true;
		}
	};

#endif /* NX_SCENE_DESC_H */
