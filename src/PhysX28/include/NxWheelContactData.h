#ifndef NX_WHEEL_CONTACT_DATA_H
#define NX_WHEEL_CONTACT_DATA_H

/*
 * PhysX 2.8's NxWheelContactData and NxUserWheelContactModify.
 *
 * Transcribed from extern/physx/include/Physics/NxWheelShape.h.
 *
 * contactPosition is the field most likely to be got wrong, so its meaning is
 * recorded here rather than inferred later. 2.8 documents it as "the distance
 * on the spring travel distance where the wheel would end up if it was resting
 * on the contact point" -- a distance ALONG THE SUSPENSION TRAVEL, not a world
 * position and not a penetration depth.
 *
 * CarWheel::PxSyncWheel uses it as `st = contactPosition - radius` and draws
 * the wheel st below the shape's origin, so an implementation that derives it
 * from a compression figure has to know which end of the travel it is measured
 * from. Both points are available in world space at the time it is computed;
 * projecting is exact and guessing is not.
 *
 * NxUserWheelContactModify has no equivalent in any modern engine -- there is
 * no per-wheel, per-substep hook that can rewrite the normal force. The game
 * uses it for a tire overload limiter and a clutch lock. Recorded as a known
 * gap for whoever implements NxWheelShape.
 */

#include "Nxp.h"
#include "NxSimpleTypes.h"
#include "NxVec3.h"

class NxShape;
class NxWheelShape;

class NxWheelContactData
	{
	public:
	/** Contact point, in world space. */
	NxVec3 contactPoint;
	NxVec3 contactNormal;
	/** The wheel's forward direction at the contact. */
	NxVec3 longitudalDirection;
	NxVec3 lateralDirection;
	NxReal contactForce;
	NxReal longitudalSlip;
	NxReal lateralSlip;
	NxReal longitudalImpulse;
	NxReal lateralImpulse;
	NxMaterialIndex otherShapeMaterialIndex;
	NxShape* otherShape;
	/** A distance along the suspension travel -- see the note above. */
	NxReal contactPosition;

	NX_INLINE NxWheelContactData()
		: contactForce(0), longitudalSlip(0), lateralSlip(0),
		  longitudalImpulse(0), lateralImpulse(0),
		  otherShapeMaterialIndex(0), otherShape(NULL), contactPosition(0)
		{
		contactPoint.zero();
		contactNormal.zero();
		longitudalDirection.zero();
		lateralDirection.zero();
		}
	};

class NxUserWheelContactModify
	{
	public:
	virtual bool onWheelContact(
		NxWheelShape* wheelShape,
		NxVec3& contactPoint,
		NxVec3& contactNormal,
		NxReal& contactPosition,
		NxReal& normalForce,
		NxShape* otherShape,
		NxMaterialIndex& otherShapeMaterialIndex,
		NxU32 otherShapeFeatureIndex) = 0;

	protected:
	/* Protected, as 2.8 has it -- an implementation cannot be deleted through
	   a base pointer. WheelShape::ContactModify derives from this. */
	virtual ~NxUserWheelContactModify() {}
	};

#endif /* NX_WHEEL_CONTACT_DATA_H */
