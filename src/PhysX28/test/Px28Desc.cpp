// Descriptor defaults and the tire curve.
//
// The defaults are not incidental: WheelShape's constructor does
// AssignFromDesc(NxWheelShapeDesc(), false), so NxWheelShapeDesc's
// default-constructed values *are* the game's default wheel tuning. A wrong
// default here is a silently different car.

#include "NxSpringDesc.h"
#include "NxTireFunctionDesc.h"

#include <cstdio>
#include <cmath>

static int gFailures = 0;

static void Check(const char *what, bool ok)
{
	printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
	if (!ok) ++gFailures;
}

static bool Near(NxReal a, NxReal b) { return std::fabs(double(a - b)) < 1e-5; }

int RunDescTests()
{
	printf("Descriptor defaults and the tire curve\n\n");

	{
		NxSpringDesc s;
		Check("NxSpringDesc defaults to all zeros",
		      s.spring == 0 && s.damper == 0 && s.targetValue == 0);
		Check("a negative damper is invalid", !NxSpringDesc(1, -1).isValid());
	}

	{
		NxTireFunctionDesc t;
		Check("extremumSlip defaults to 1.0", Near(t.extremumSlip, 1.0f));
		Check("extremumValue defaults to 0.02", Near(t.extremumValue, 0.02f));
		Check("asymptoteSlip defaults to 2.0", Near(t.asymptoteSlip, 2.0f));
		Check("asymptoteValue defaults to 0.01", Near(t.asymptoteValue, 0.01f));
		Check("stiffnessFactor defaults to 1e6", Near(t.stiffnessFactor, 1000000.0f));
		Check("the defaults are valid", t.isValid() == 0);
	}

	// isValid returns a numbered reason, not a bool, and the numbers are the
	// SDK's -- NxWheelShapeDesc::isValid folds them in.
	{
		NxTireFunctionDesc t;
		t.extremumSlip = 0;
		Check("isValid() reports reason 1 for extremumSlip <= 0", t.isValid() == 1);
		t.setToDefault();
		t.asymptoteSlip = t.extremumSlip;
		Check("isValid() reports reason 2 when the slips do not increase", t.isValid() == 2);
		t.setToDefault();
		t.extremumValue = 0;
		Check("isValid() reports reason 3 for extremumValue <= 0", t.isValid() == 3);
	}

	// The curve. Grip RISES with slip up to extremumSlip -- it is not flat
	// below it, which is the reading that produces a tire with no breakaway.
	{
		NxTireFunctionDesc t;
		t.extremumSlip = 1.0f;   t.extremumValue = 6.5f;
		t.asymptoteSlip = 2.0f;  t.asymptoteValue = 3.0f;

		Check("the curve is zero at zero slip", Near(t.hermiteEval(0), 0));
		Check("it peaks at extremumSlip", Near(t.hermiteEval(1.0f), 6.5f));
		Check("it reaches the asymptote at asymptoteSlip", Near(t.hermiteEval(2.0f), 3.0f));
		Check("it is flat beyond asymptoteSlip", Near(t.hermiteEval(50.0f), 3.0f));

		// Rising, not flat, on the way up.
		const NxReal quarter = t.hermiteEval(0.25f);
		const NxReal half = t.hermiteEval(0.5f);
		Check("grip rises with slip below the peak",
		      quarter > 0 && half > quarter && half < 6.5f);

		// Falling between the knots.
		const NxReal mid = t.hermiteEval(1.5f);
		Check("grip falls between extremum and asymptote", mid < 6.5f && mid > 3.0f);

		// Odd: the curve is signed by the slip, so a reversed slip reverses
		// the force rather than repeating it.
		Check("the curve is odd in slip", Near(t.hermiteEval(-0.5f), -half));

		// Hand-computed against the documented construction: at a = 0.5 on the
		// first piece, F = extremumValue * (-a^3 + a^2 + a) = 6.5 * 0.625.
		Check("first piece matches -a^3 + a^2 + a", Near(half, 6.5f * 0.625f));
	}

	// The shipped default of 0.02 versus a set-up car's 6.5 is a factor of 325,
	// which is why a car whose upgrades never applied reads as having no grip.
	{
		NxTireFunctionDesc shipped;
		Check("the default curve peaks at 0.02, not at a driveable value",
		      Near(shipped.hermiteEval(1.0f), 0.02f));
	}

	printf("\n%s (%d failure%s)\n", gFailures ? "FAILED" : "all defaults hold",
	       gFailures, gFailures == 1 ? "" : "s");
	return gFailures;
}
