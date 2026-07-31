#ifndef NX_ACTOR_DESC_H
#define NX_ACTOR_DESC_H

/*
 * PhysX 2.8's NxBodyDesc, NxActorDescBase and NxActorDesc.
 *
 * Transcribed from extern/physx/include/Physics/NxBodyDesc.h and NxActorDesc.h.
 *
 * NxBodyDesc's default flags explain a number that shows up in the shipped
 * data: setToDefault assigns NX_BF_VISUALIZATION and then ORs in
 * NX_BF_ENERGY_SLEEP_TEST, which is 256 | 2048 = 2304 -- exactly the
 * <flags>2304</flags> in db.xml. That value is not a deliberate combination
 * someone chose; it is simply the default, serialised. Which means getting
 * either enumerator wrong would make every saved body load with different sleep
 * behaviour, silently.
 *
 * The discriminator that matters structurally: NxActorDescBase::body is NULL
 * for a static actor and points at a NxBodyDesc for a dynamic one. 2.8 decides
 * static-versus-dynamic entirely on that pointer, and so does the engine --
 * px::Actor keys off _body for the same purpose.
 */

#include "Nxp.h"
#include "NxSimpleTypes.h"
#include "NxVec3.h"
#include "NxMat34.h"
#include "NxArray.h"
#include "NxShapeDesc.h"

class NxBodyDesc
	{
	public:
	NxMat34 massLocalPose;
	NxVec3  massSpaceInertia;
	NxVec3  linearVelocity;
	NxVec3  angularVelocity;
	NxReal  wakeUpCounter;
	NxReal  mass;
	NxReal  linearDamping;
	NxReal  angularDamping;
	NxReal  maxAngularVelocity;
	NxReal  CCDMotionThreshold;
	NxU32   flags;
	NxReal  sleepLinearVelocity;
	NxReal  sleepAngularVelocity;
	NxU32   solverIterationCount;
	NxReal  sleepEnergyThreshold;
	NxReal  sleepDamping;
	NxReal  contactReportThreshold;

	NX_INLINE NxBodyDesc() { setToDefault(); }

	NX_INLINE void setToDefault()
		{
		massLocalPose.id();
		massSpaceInertia.zero();
		linearVelocity.zero();
		angularVelocity.zero();
		wakeUpCounter          = 20.0f * 0.02f;
		mass                   = 0.0f;
		linearDamping          = 0.0f;
		angularDamping         = 0.05f;
		maxAngularVelocity     = -1.0f;
		/* 256 | 2048 = 2304, which is what db.xml stores. */
		flags                  = NX_BF_VISUALIZATION;
		sleepLinearVelocity    = -1.0f;
		sleepAngularVelocity   = -1.0f;
		CCDMotionThreshold     = 0.0f;
		solverIterationCount   = 4;
		flags                 |= NX_BF_ENERGY_SLEEP_TEST;
		sleepEnergyThreshold   = -1.0f;
		sleepDamping           = 0.0f;
		contactReportThreshold = NX_MAX_REAL;
		}

	NX_INLINE bool isValid() const
		{
		if (mass < 0.0f)                 return false;
		if (solverIterationCount < 1)    return false;
		if (solverIterationCount > 255)  return false;
		if (linearDamping < 0.0f)        return false;
		if (angularDamping < 0.0f)       return false;
		if (!massLocalPose.isFinite())   return false;
		return true;
		}
	};

class NxActorDescBase
	{
	public:
	/** NULL means a STATIC actor. This pointer is the whole discriminator. */
	NxBodyDesc*  body;
	NxReal       density;
	NxMat34      globalPose;
	NxU32        flags;
	void*        userData;
	const char*  name;
	NxU16        group;
	NxU16        dominanceGroup;
	NxU32        contactReportFlags;
	NxU16        forceFieldMaterial;
	void*        compartment;

	NX_INLINE virtual ~NxActorDescBase() {}

	NX_INLINE void setToDefault()
		{
		body               = NULL;
		density            = 0.0f;
		globalPose.id();
		flags              = 0;
		userData           = NULL;
		name               = NULL;
		group              = 0;
		dominanceGroup     = 0;
		contactReportFlags = 0;
		forceFieldMaterial = 0;
		compartment        = NULL;
		}

	NX_INLINE virtual bool isValid() const
		{
		if (!globalPose.isFinite())        return false;
		if (body && !body->isValid())      return false;
		if (density < 0.0f)                return false;
		return true;
		}

	protected:
	NX_INLINE NxActorDescBase() { setToDefault(); }
	};

class NxActorDesc : public NxActorDescBase
	{
	public:
	NxArray<NxShapeDesc*, NxAllocatorDefault> shapes;

	NX_INLINE NxActorDesc() { setToDefault(); }

	NX_INLINE void setToDefault()
		{
		NxActorDescBase::setToDefault();
		shapes.clear();
		}

	NX_INLINE virtual bool isValid() const
		{
		if (!NxActorDescBase::isValid()) return false;
		for (NxU32 i = 0; i < shapes.size(); ++i)
			if (!shapes[i] || !shapes[i]->isValid()) return false;
		return true;
		}
	};

#endif /* NX_ACTOR_DESC_H */
