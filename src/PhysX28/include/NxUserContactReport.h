#ifndef NX_USER_CONTACT_REPORT_H
#define NX_USER_CONTACT_REPORT_H

/*
 * PhysX 2.8's contact reporting and modification surface.
 *
 * Transcribed from extern/physx/include/Physics/NxUserContactReport.h and
 * NxContactStreamIterator.h.
 *
 * NxContactStreamIterator is a CONCRETE value type the game constructs on the
 * stack -- GameObject.cpp, Logic.cpp, Weapon.cpp and GameCar.cpp all do
 * `NxContactStreamIterator i(pair.stream);`. So it cannot become an abstract
 * interface, and NxConstContactStream stays a plain pointer that can be copied
 * and stored by value: Scene::OnContactEvent holds one as a member.
 *
 * The stream itself is opaque -- 2.8 types it `const NxU32*` and the layout is
 * private to the SDK. That is convenient rather than limiting: the shim can
 * point it at whatever record it likes, provided this iterator is the only
 * thing that reads it. Nothing in the game does arithmetic on it.
 *
 * NX_CCC_LOCALORIENTATION0/1 live here, nested in NxUserContactModify exactly
 * as 2.8 nests them -- which is why they were kept out of Nxp.h. They are the
 * friction-basis change flags, and the reason the backend is Bullet: PhysX 3+
 * and Jolt have no per-contact friction orientation, and
 * btManifoldPoint::m_lateralFrictionDir1/2 does.
 */

#include "Nxp.h"
#include "NxSimpleTypes.h"
#include "NxVec3.h"
#include "NxQuat.h"

class NxShape;
class NxActor;

/* Opaque to everything but NxContactStreamIterator. */
typedef const NxU32* NxConstContactStream;

class NxContactPair
	{
	public:
	NxActor*             actors[2];
	NxConstContactStream stream;
	NxVec3               sumNormalForce;
	NxVec3               sumFrictionForce;
	/** The actor pointers may reference deleted actors; check before using. */
	bool                 isDeletedActor[2];

	NX_INLINE NxContactPair(): stream(NULL)
		{
		actors[0] = actors[1] = NULL;
		sumNormalForce.zero();
		sumFrictionForce.zero();
		isDeletedActor[0] = isDeletedActor[1] = false;
		}
	};

/*
 * A cursor over one contact stream: pairs, then patches within a pair, then
 * points within a patch. The game walks all three levels.
 *
 * Declared here and implemented by the shim, because the traversal depends on
 * the record format the shim chooses.
 */
class NxContactStreamIterator
	{
	public:
	NxContactStreamIterator(NxConstContactStream stream);

	bool goNextPair();
	bool goNextPatch();
	bool goNextPoint();

	NxU32    getNumPairs();
	NxShape* getShape(NxU32 shapeIndex);
	bool     isDeletedShape(NxU32 shapeIndex);
	NxU16    getShapeFlags();
	NxU32    getNumPatches();
	NxU32    getNumPatchesRemaining();
	NxU32    getNumPoints();
	NxU32    getNumPointsRemaining();

	const NxVec3& getPatchNormal();
	const NxVec3& getPoint();
	NxReal        getSeparation();
	NxReal        getPointNormalForce();
	NxU32         getFeatureIndex0();
	NxU32         getFeatureIndex1();

	private:
	/*
	 * Cursor state, and private to the shim in the same way 2.8's was private
	 * to the SDK. Nothing outside reads these -- the game only ever constructs
	 * an iterator and walks it -- so the layout is free to be whatever the
	 * shim's record format needs.
	 *
	 * Each index starts one before the beginning, because 2.8's traversal is
	 * `while (goNextPair())`: the first call moves to the first element rather
	 * than past it.
	 */
	NxConstContactStream _stream;
	NxU32                _pair;
	NxU32                _patch;
	NxU32                _point;
	};

class NxUserContactReport
	{
	public:
	virtual ~NxUserContactReport() {}
	virtual void onContactNotify(NxContactPair& pair, NxU32 events) = 0;
	};

class NxUserContactModify
	{
	public:
	virtual ~NxUserContactModify() {}

	/*
	 * Which fields of NxContactCallbackData the callback changed. OR them
	 * together. Nested here, as 2.8 nests them -- Physx.h reaches them through
	 * `typedef NxUserContactModify ContactModifyTraits;`.
	 */
	enum NxContactConstraintChange
		{
		NX_CCC_NONE               = 0,
		NX_CCC_MINIMPULSE         = (1<<0),
		NX_CCC_MAXIMPULSE         = (1<<1),
		NX_CCC_ERROR              = (1<<2),
		NX_CCC_TARGET             = (1<<3),
		NX_CCC_LOCALPOSITION0     = (1<<4),
		NX_CCC_LOCALPOSITION1     = (1<<5),
		/** The friction frame in shape 0. No equivalent in PhysX 3+. */
		NX_CCC_LOCALORIENTATION0  = (1<<6),
		NX_CCC_LOCALORIENTATION1  = (1<<7),
		/* Note 2.8's own warning: the 0/1 here are friction parameters, and
		   have nothing to do with shape 0 or shape 1. */
		NX_CCC_STATICFRICTION0    = (1<<8),
		NX_CCC_STATICFRICTION1    = (1<<9),
		NX_CCC_DYNAMICFRICTION0   = (1<<10),
		NX_CCC_DYNAMICFRICTION1   = (1<<11),
		NX_CCC_RESTITUTION        = (1<<12)
		};

	struct NxContactCallbackData
		{
		NxReal minImpulse;
		NxReal maxImpulse;
		NxVec3 error;
		NxVec3 target;
		NxVec3 localpos0;
		NxVec3 localpos1;
		NxQuat localorientation0;
		NxQuat localorientation1;
		NxReal staticFriction0;
		NxReal staticFriction1;
		NxReal dynamicFriction0;
		NxReal dynamicFriction1;
		NxReal restitution;
		};

	virtual bool onContactConstraint(
		NxU32& changeFlags,
		const NxShape* shape0,
		const NxShape* shape1,
		const NxU32 featureIndex0,
		const NxU32 featureIndex1,
		NxContactCallbackData& data) = 0;
	};

class NxJoint;

class NxUserNotify
	{
	public:
	virtual ~NxUserNotify() {}
	virtual bool onJointBreak(NxReal breakingImpulse, NxJoint& brokenJoint) = 0;
	virtual void onWake(NxActor** actors, NxU32 count) = 0;
	virtual void onSleep(NxActor** actors, NxU32 count) = 0;
	};

#endif /* NX_USER_CONTACT_REPORT_H */
