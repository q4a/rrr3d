#ifndef NX_SPRING_DESC_H
#define NX_SPRING_DESC_H

/*
 * PhysX 2.8's NxSpringDesc.
 *
 * Transcribed from extern/physx/include/Physics/NxSpringDesc.h.
 *
 * Three numbers, and one sentence in the SDK's own documentation that the shim
 * has to take seriously:
 *
 *   "The spring is implicitly integrated, so even high spring and damper
 *    coefficients should be robust."
 *
 * That is why the shipped cars get away with the tuning they have. An implicit
 * integrator is unconditionally stable, so 2.8's suspensions never needed to be
 * well damped -- and across this game's cars the damping ratio runs from 0.71
 * down to about 0.06. Anything reproducing this with an explicit integrator
 * will see the undamped cars ring, their tire load swing, and traction arrive
 * and leave several times a second. That reads as a grip problem and is not
 * one.
 *
 * Recorded here rather than acted on: the fix is in how NxWheelShape integrates,
 * not in changing these numbers. They are the car's tuning and they are not
 * ours to adjust.
 */

#include "NxSimpleTypes.h"

class NxSpringDesc
	{
	public:
	/** Spring coefficient. Range: (-inf, inf) */
	NxReal spring;
	/** Damper coefficient. Range: [0, inf) */
	NxReal damper;
	/** The value at which the spring force is zero.
	    Every car in this game ships with this at 0, meaning rest at full
	    extension -- so a naive reading gives a wheel with no droop at all. */
	NxReal targetValue;

	NX_INLINE NxSpringDesc() { setToDefault(); }
	NX_INLINE NxSpringDesc(NxReal s, NxReal d = 0, NxReal target = 0)
		: spring(s), damper(d), targetValue(target) {}

	NX_INLINE void setToDefault()
		{
		spring = 0;
		damper = 0;
		targetValue = 0;
		}

	NX_INLINE bool isValid() const { return damper >= 0; }
	};

#endif /* NX_SPRING_DESC_H */
