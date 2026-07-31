#ifndef NX_TIRE_FUNCTION_DESC_H
#define NX_TIRE_FUNCTION_DESC_H

/*
 * PhysX 2.8's NxTireFunctionDesc -- the tire curve.
 *
 * Transcribed from extern/physx/include/Physics/NxWheelShapeDesc.h, which is
 * also where the SDK documents what the curve *means*. That documentation is
 * the specification NxWheelShape gets implemented against later, so it is
 * recorded here rather than rediscovered:
 *
 *   Under NX_WF_CLAMPED_FRICTION -- which every car in this game sets --
 *   "the output from the tire force function is interpreted as friction
 *    coefficients. The maximum friction impulse available to maintain the
 *    constraint is computed by scaling the output with the normal impulse."
 *
 * So the curve is a *ceiling on what a contact may spend*, not the force
 * applied. Reading it as a force is the difference between a car that drives
 * and one that stands on its back wheels.
 *
 * Separately, below a speed threshold -- "the contact point with the ground is
 * within skinWidth of its previous position" -- contacts become static friction
 * contacts with mu = extremumValue. Two regimes, both specified.
 *
 * The curve itself is a two-piece cubic Hermite spline with zero tangent at
 * both knots. Grip *rises* with slip up to extremumSlip; it is not flat below
 * it. hermiteEval is implemented here from the documented construction rather
 * than copied, since this is a GPL-3 tree and the SDK's inline body is not ours
 * to redistribute -- but the shape and the constants are the API's, and the
 * test pins the result against hand-computed values.
 */

#include "NxSimpleTypes.h"

#include <cmath>

class NxTireFunctionDesc
	{
	public:
	/** Slip at which the curve peaks. */
	NxReal extremumSlip;
	/** The peak value. Under clamped friction this is mu at the peak. */
	NxReal extremumValue;
	/** Slip beyond which the curve is flat. */
	NxReal asymptoteSlip;
	/** The value it settles to. */
	NxReal asymptoteValue;
	/** Ignored under NX_WF_CLAMPED_FRICTION; the SDK says so explicitly. */
	NxReal stiffnessFactor;

	NX_INLINE NxTireFunctionDesc() { setToDefault(); }

	NX_INLINE void setToDefault()
		{
		extremumSlip = 1.0f;
		extremumValue = 0.02f;
		asymptoteSlip = 2.0f;
		asymptoteValue = 0.01f;
		stiffnessFactor = 1000000.0f;   /* "quite stiff by default" */
		}

	/*
	 * The SDK returns a numbered reason rather than a bool. Kept, because
	 * NxWheelShapeDesc::isValid folds the result in and the numbering is part
	 * of how a failure is reported.
	 */
	NX_INLINE NxU32 isValid() const
		{
		if (!(0.0f < extremumSlip))          return 1;
		if (!(extremumSlip < asymptoteSlip)) return 2;
		if (!(0.0f < extremumValue))         return 3;
		if (!(0.0f < asymptoteValue))        return 4;
		if (!(0.0f <= stiffnessFactor))      return 5;
		return 0;
		}

	/*
	 * Two cubic Hermite pieces, zero tangent at each knot, flat beyond
	 * asymptoteSlip, and signed by the slip so the curve is odd.
	 *
	 *   0 .. extremumSlip      rises from 0 to extremumValue
	 *   .. asymptoteSlip       falls from extremumValue to asymptoteValue
	 *   beyond                 asymptoteValue
	 */
	NX_INLINE NxReal hermiteEval(NxReal t) const
		{
		const NxReal v = NxReal(std::fabs(double(t)));
		const NxReal sign = (t >= NxReal(0)) ? NxReal(1) : NxReal(-1);

		NxReal f;
		if (v < extremumSlip)
			{
			const NxReal a = v / extremumSlip;
			const NxReal a2 = a * a;
			const NxReal a3 = a * a2;
			f = extremumValue * (-a3 + a2 + a);
			}
		else if (v < asymptoteSlip)
			{
			const NxReal a = (v - extremumSlip) / (asymptoteSlip - extremumSlip);
			const NxReal a2 = a * a;
			const NxReal a3 = a * a2;
			const NxReal diff = asymptoteValue - extremumValue;
			f = NxReal(-2) * diff * a3 + NxReal(3) * diff * a2 + extremumValue;
			}
		else
			{
			f = asymptoteValue;
			}

		return sign * f;
		}
	};

#endif /* NX_TIRE_FUNCTION_DESC_H */
