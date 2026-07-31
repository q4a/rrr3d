// Descriptor defaults and the tire curve.
//
// The defaults are not incidental: WheelShape's constructor does
// AssignFromDesc(NxWheelShapeDesc(), false), so NxWheelShapeDesc's
// default-constructed values *are* the game's default wheel tuning. A wrong
// default here is a silently different car.

#include "NxSpringDesc.h"
#include "NxTireFunctionDesc.h"
#include "NxShapeDesc.h"
#include "NxActorDesc.h"
#include "NxMaterialDesc.h"
#include "NxSceneDesc.h"
#include "NxTriangleMeshDesc.h"
#include "NxUserContactReport.h"
#include "NxWheelContactData.h"

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

	// isValid() is not a formality. Actor::CreateNxShape uses !isValid() as its
	// "the mesh has not loaded yet, defer this shape" signal, so a mesh
	// descriptor with a null meshData must be INVALID -- returning true there
	// creates every mesh shape in the game twice.
	{
		NxTriangleMeshShapeDesc mesh;
		Check("a mesh descriptor with no meshData is INVALID", !mesh.isValid());
		Check("...and reports reason 1", mesh.checkValid() == 1);

		NxConvexShapeDesc convex;
		Check("a convex descriptor with no meshData is INVALID", !convex.isValid());
	}

	// Shape descriptor defaults.
	{
		NxBoxShapeDesc box;
		Check("box dimensions are HALF-extents, defaulting to 0.5",
		      Near(box.dimensions.x, 0.5f));
		Check("a default box is valid", box.isValid());
		Check("skinWidth defaults to -1, meaning 'use the global'", Near(box.skinWidth, -1.0f));
		Check("density defaults to 1 and mass to -1", Near(box.density, 1.0f) && Near(box.mass, -1.0f));
		Check("group defaults to 0", box.group == 0);
		box.group = 32;
		Check("a group of 32 or more is invalid -- only 32 groups exist", !box.isValid());
	}

	{
		NxCapsuleShapeDesc cap;
		Check("a default capsule is INVALID (radius and height are 0)", !cap.isValid());
		cap.radius = 0.5f; cap.height = 2.0f;
		Check("...and valid once given a radius and height", cap.isValid());
	}

	// The wheel descriptor's defaults ARE the game's default wheel tuning,
	// because WheelShape's constructor takes a default-constructed one.
	{
		NxWheelShapeDesc wheel;
		Check("wheel radius defaults to 1", Near(wheel.radius, 1.0f));
		Check("suspensionTravel defaults to 1", Near(wheel.suspensionTravel, 1.0f));
		Check("inverseWheelMass defaults to 1", Near(wheel.inverseWheelMass, 1.0f));
		Check("wheelFlags default to 0, NOT to clamped friction", wheel.wheelFlags == 0);
		Check("torques and steer default to 0",
		      wheel.motorTorque == 0 && wheel.brakeTorque == 0 && wheel.steerAngle == 0);

		// The fromCtor path leaves the nested descriptors to their own
		// constructors, so the tire curve still has the SDK's defaults.
		Check("the nested tire curve kept its own defaults",
		      Near(wheel.longitudalTireForceFunction.extremumValue, 0.02f));
		Check("the nested spring kept its own defaults",
		      wheel.suspension.spring == 0 && wheel.suspension.damper == 0);
	}

	// NxBodyDesc's default flags are literally the number db.xml stores. That
	// value was never a choice anyone made -- it is the default, serialised.
	{
		NxBodyDesc body;
		Check("default body flags are 2304, which is what db.xml stores",
		      body.flags == 2304);
		Check("...being NX_BF_VISUALIZATION | NX_BF_ENERGY_SLEEP_TEST",
		      body.flags == (NX_BF_VISUALIZATION | NX_BF_ENERGY_SLEEP_TEST));
		Check("angularDamping defaults to 0.05", Near(body.angularDamping, 0.05f));
		Check("solverIterationCount defaults to 4", body.solverIterationCount == 4);
		Check("a default body is valid", body.isValid());
	}

	// body == NULL is the entire static-vs-dynamic discriminator, in 2.8 and
	// in the engine's px::Actor alike.
	{
		NxActorDesc actor;
		Check("an actor descriptor defaults to STATIC (body is null)", actor.body == NULL);
		Check("...and has no shapes", actor.shapes.empty());
		Check("a default actor descriptor is valid", actor.isValid());

		NxBoxShapeDesc box;
		actor.shapes.push_back(&box);
		Check("a valid shape keeps the actor valid", actor.isValid());

		NxTriangleMeshShapeDesc unloaded;   // meshData is null
		actor.shapes.push_back(&unloaded);
		Check("an actor holding an unloaded mesh shape is INVALID", !actor.isValid());
	}

	// The material fields that decided the backend.
	{
		NxMaterialDesc m;
		Check("dirOfAnisotropy defaults to +X",
		      Near(m.dirOfAnisotropy.x, 1.0f) && Near(m.dirOfAnisotropy.y, 0.0f));
		Check("friction combine mode defaults to AVERAGE",
		      m.frictionCombineMode == NX_CM_AVERAGE);
		Check("anisotropy is off by default", (m.flags & NX_MF_ANISOTROPIC) == 0);
		Check("a default material is valid", m.isValid());
		m.restitution = 2.0f;
		Check("a restitution above 1 is invalid", !m.isValid());
	}

	// maxTimestep settles a question the shim would otherwise have to guess at.
	{
		NxSceneDesc scene;
		Check("maxTimestep defaults to 1/60", Near(scene.maxTimestep, 1.0f / 60.0f));
		Check("...which is also the game's step, so 2.8 ran ONE substep",
		      Near(scene.maxTimestep, 1.0f / 60.0f));
		Check("upAxis defaults to 0; the game sets 2 for Z-up", scene.upAxis == 0);
		Check("timeStepMethod defaults to FIXED; the game sets VARIABLE",
		      scene.timeStepMethod == NX_TIMESTEP_FIXED);
		Check("a default scene descriptor is valid", scene.isValid());
	}

	{
		NxTriangleMeshDesc mesh;
		Check("an empty mesh descriptor is invalid", !mesh.isValid());
		Check("...reporting reason 1, too few vertices", mesh.checkValid() == 1);
		Check("convexEdgeThreshold defaults to 0.001", Near(mesh.convexEdgeThreshold, 0.001f));

		// A non-indexed mesh has to define a whole number of triangles.
		static const float pts[12] = {0,0,0, 1,0,0, 0,1,0, 1,1,0};
		mesh.numVertices = 4;
		mesh.points = pts;
		mesh.triangles = NULL;
		Check("a non-indexed mesh with a vertex count not divisible by 3 is invalid",
		      mesh.checkValid() == 2);
		mesh.numVertices = 3;
		Check("...and valid when it is", mesh.isValid());
	}

	{
		NxConvexMeshDesc convex;
		static const float pts[12] = {0,0,0, 1,0,0, 0,1,0, 0,0,1};
		convex.numVertices = 4;
		convex.points = pts;
		Check("a convex descriptor without a hull or COMPUTE_CONVEX is invalid",
		      !convex.isValid());
		convex.flags = NX_CF_COMPUTE_CONVEX;
		Check("...and valid once asked to compute the hull", convex.isValid());
	}

	// The friction-basis change flags, nested in NxUserContactModify exactly as
	// 2.8 nests them. These are why the backend is Bullet: PhysX 3+ and Jolt
	// have no per-contact friction orientation and btManifoldPoint does.
	{
		Check("NX_CCC_LOCALORIENTATION0 is 1<<6",
		      NxUserContactModify::NX_CCC_LOCALORIENTATION0 == (1 << 6));
		Check("NX_CCC_LOCALORIENTATION1 is 1<<7",
		      NxUserContactModify::NX_CCC_LOCALORIENTATION1 == (1 << 7));
		Check("NX_CCC_STATICFRICTION0 is 1<<8",
		      NxUserContactModify::NX_CCC_STATICFRICTION0 == (1 << 8));
		Check("NX_CCC_DYNAMICFRICTION0 is 1<<10",
		      NxUserContactModify::NX_CCC_DYNAMICFRICTION0 == (1 << 10));

		// Physx.h reaches them through this typedef, so it has to work.
		typedef NxUserContactModify ContactModifyTraits;
		typedef ContactModifyTraits::NxContactCallbackData CallbackData;
		CallbackData d;
		d.staticFriction0 = 0.0f;
		Check("the nested NxContactCallbackData is reachable via the traits typedef",
		      d.staticFriction0 == 0.0f);
	}

	// The contact pair is a value the game copies and stores.
	{
		NxContactPair pair;
		Check("a contact pair defaults to no actors and no stream",
		      pair.actors[0] == NULL && pair.stream == NULL);
		Check("...and to neither actor deleted",
		      !pair.isDeletedActor[0] && !pair.isDeletedActor[1]);
	}

	printf("\n%s (%d failure%s)\n", gFailures ? "FAILED" : "all defaults hold",
	       gFailures, gFailures == 1 ? "" : "s");
	return gFailures;
}
