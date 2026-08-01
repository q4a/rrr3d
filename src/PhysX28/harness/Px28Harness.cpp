/*
 * PhysX28Harness -- scenarios that need the backend, and so cannot live in
 * PhysX28Tests, which is deliberately dependency-free.
 *
 * Everything here goes through the Nx API or the shim's own conversions. It
 * depends on neither Rock3dEngine nor windows.h, so it runs long before the
 * graphics port does and fails on its own terms.
 *
 * Assertions are against 2.8's specification and against arithmetic, never
 * against a feel target. That is the whole approach: there is nothing to tune,
 * so there is nothing to tune *to*.
 */

#include "../source/Px28Impl.h"

#include <cmath>
#include <cstdio>

namespace
{

int gFailures = 0;
int gChecks = 0;

void Check(bool condition, const char* what)
	{
	++gChecks;
	if (!condition)
		{
		++gFailures;
		std::printf("  FAIL  %s\n", what);
		}
	}

void CheckNear(float actual, float expected, float tolerance, const char* what)
	{
	++gChecks;
	if (!(std::fabs(actual - expected) <= tolerance))
		{
		++gFailures;
		std::printf("  FAIL  %s: expected %g, got %g (tolerance %g)\n",
		            what, expected, actual, tolerance);
		}
	}

void CheckVecNear(const NxVec3& actual, const NxVec3& expected, float tolerance,
                  const char* what)
	{
	++gChecks;
	if (!(std::fabs(actual.x - expected.x) <= tolerance &&
	      std::fabs(actual.y - expected.y) <= tolerance &&
	      std::fabs(actual.z - expected.z) <= tolerance))
		{
		++gFailures;
		std::printf("  FAIL  %s: expected (%g %g %g), got (%g %g %g)\n",
		            what, expected.x, expected.y, expected.z,
		            actual.x, actual.y, actual.z);
		}
	}

/*
 * The conversion between NxMat34 and btTransform.
 *
 * This is the one piece of the shim where a silent sign or transpose error
 * would mirror every rotation in the game, and it would present as "the cars
 * steer the wrong way" rather than as anything resembling a conversion bug. So
 * it is checked against arithmetic rather than against a round trip alone: a
 * round trip passes just as happily when both directions are transposed.
 */
void TestTransformConversion()
	{
	std::printf("transform conversion\n");

	/* Identity. */
	{
	NxMat34 m;
	m.id();
	const NxMat34 back = px28::ToNx(px28::ToBullet(m));
	CheckVecNear(back.t, NxVec3(0, 0, 0), 1e-6f, "identity translation");
	CheckVecNear(back.M.getColumn(0), NxVec3(1, 0, 0), 1e-6f, "identity column 0");
	CheckVecNear(back.M.getColumn(1), NxVec3(0, 1, 0), 1e-6f, "identity column 1");
	CheckVecNear(back.M.getColumn(2), NxVec3(0, 0, 1), 1e-6f, "identity column 2");
	}

	/* Translation is carried across untouched. */
	{
	NxMat34 m;
	m.id();
	m.t.set(NxVec3(3.0f, -4.0f, 5.5f));
	const btTransform bt = px28::ToBullet(m);
	CheckNear(bt.getOrigin().x(), 3.0f, 1e-6f, "translation x");
	CheckNear(bt.getOrigin().y(), -4.0f, 1e-6f, "translation y");
	CheckNear(bt.getOrigin().z(), 5.5f, 1e-6f, "translation z");
	}

	/*
	 * The one that catches a transpose. A quarter turn about Z takes X to Y;
	 * the transpose takes X to -Y, and a round-trip test cannot tell them
	 * apart. fromAngleAxis takes DEGREES -- see NxQuat.h.
	 */
	{
	NxQuat q;
	q.fromAngleAxis(90.0f, NxVec3(0, 0, 1));

	NxMat34 m;
	m.M.fromQuat(q);
	m.t.zero();

	/* What 2.8's own arithmetic says. */
	NxVec3 nxResult;
	m.M.multiply(NxVec3(1, 0, 0), nxResult);
	CheckVecNear(nxResult, NxVec3(0, 1, 0), 1e-5f, "Nx: +90 deg about Z takes X to Y");

	/* What Bullet says after the conversion. It must agree. */
	const btTransform bt = px28::ToBullet(m);
	const btVector3 btResult = bt.getBasis() * btVector3(1, 0, 0);
	CheckVecNear(px28::ToNx(btResult), NxVec3(0, 1, 0), 1e-5f,
	             "Bullet: +90 deg about Z takes X to Y");
	}

	/* A pose with both rotation and translation survives the round trip. */
	{
	NxQuat q;
	q.fromAngleAxis(37.0f, NxVec3(1, 2, 3));

	NxMat34 m;
	m.M.fromQuat(q);
	m.t.set(NxVec3(-1.25f, 8.0f, 0.5f));

	const NxMat34 back = px28::ToNx(px28::ToBullet(m));
	CheckVecNear(back.t, m.t, 1e-5f, "round trip translation");
	for (int col = 0; col < 3; ++col)
		{
		char label[64];
		std::snprintf(label, sizeof(label), "round trip column %d", col);
		CheckVecNear(back.M.getColumn(col), m.M.getColumn(col), 1e-5f, label);
		}
	}
	}

/*
 * Free fall against the closed-form answer, which is the plan's first
 * simulation check. Bullet's semi-implicit Euler integrator does not produce
 * the analytic position exactly -- it accumulates v*dt using the velocity at
 * the *end* of each step, so after n steps it has fallen g*dt^2*n*(n+1)/2
 * rather than g*(n*dt)^2/2. Asserting the analytic value with a loose
 * tolerance would hide a real error; asserting the integrator's own sum is
 * exact and still catches wrong gravity, wrong mass handling or a dropped step.
 */
void TestFreeFall()
	{
	std::printf("free fall\n");

	NxSceneDesc sceneDesc;
	sceneDesc.gravity.set(NxVec3(0.0f, 0.0f, -9.81f));

	px28::Scene scene(sceneDesc);

	NxVec3 gravity;
	scene.getGravity(gravity);
	CheckVecNear(gravity, NxVec3(0.0f, 0.0f, -9.81f), 1e-6f, "gravity round trips");

	NxBodyDesc bodyDesc;
	bodyDesc.mass = 10.0f;

	NxActorDesc actorDesc;
	actorDesc.body = &bodyDesc;
	actorDesc.globalPose.t.set(NxVec3(0.0f, 0.0f, 100.0f));

	NxActor* actor = scene.createActor(actorDesc);
	Check(actor != NULL, "createActor returned an actor");
	if (!actor)
		return;

	Check(actor->isDynamic(), "a body descriptor makes a dynamic actor");
	CheckNear(actor->getMass(), 10.0f, 1e-6f, "mass is the descriptor's");

	const float dt = 1.0f / 60.0f;
	const int steps = 60;

	for (int i = 0; i < steps; ++i)
		{
		scene.simulate(dt);
		scene.flushStream();
		scene.fetchResults(NX_RIGID_BODY_FINISHED, true);
		}

	/* Semi-implicit Euler: sum of g*dt*dt over each step's end velocity. */
	const float expectedDrop = 9.81f * dt * dt * (steps * (steps + 1) / 2.0f);
	const NxVec3 pos = actor->getGlobalPosition();

	CheckNear(pos.z, 100.0f - expectedDrop, 1e-3f, "free fall height after 1 second");
	CheckNear(pos.x, 0.0f, 1e-6f, "free fall does not drift in x");
	CheckNear(pos.y, 0.0f, 1e-6f, "free fall does not drift in y");

	CheckNear(actor->getLinearVelocity().z, -9.81f * steps * dt, 1e-3f,
	          "free fall velocity after 1 second");

	/*
	 * A precondition, not a nicety: a body that never moved would pass "does
	 * not drift in x" and "does not drift in y" by accident, and a scene that
	 * is not simulating at all is the single most likely way for a harness to
	 * confirm everything and catch nothing.
	 */
	Check(pos.z < 99.0f, "the scene is actually simulating");

	scene.releaseActor(*actor);
	}

/* A static actor -- no body descriptor -- must not move under gravity. */
void TestStaticActorDoesNotFall()
	{
	std::printf("static actors\n");

	NxSceneDesc sceneDesc;
	sceneDesc.gravity.set(NxVec3(0.0f, 0.0f, -9.81f));

	px28::Scene scene(sceneDesc);

	NxActorDesc actorDesc;
	actorDesc.globalPose.t.set(NxVec3(1.0f, 2.0f, 3.0f));
	/* body stays NULL, which is 2.8's whole discriminator for static. */

	NxActor* actor = scene.createActor(actorDesc);
	Check(actor != NULL, "createActor returned an actor");
	if (!actor)
		return;

	Check(!actor->isDynamic(), "a NULL body descriptor makes a static actor");

	for (int i = 0; i < 60; ++i)
		{
		scene.simulate(1.0f / 60.0f);
		scene.flushStream();
		scene.fetchResults(NX_RIGID_BODY_FINISHED, true);
		}

	CheckVecNear(actor->getGlobalPosition(), NxVec3(1.0f, 2.0f, 3.0f), 1e-6f,
	             "a static actor does not move");

	scene.releaseActor(*actor);
	}

} /* namespace */

int main()
	{
	std::printf("PhysX28Harness\n================================\n");

	TestTransformConversion();
	TestFreeFall();
	TestStaticActorDoesNotFall();

	std::printf("================================\n");
	if (gFailures == 0)
		std::printf("OK: %d checks, 0 failures\n", gChecks);
	else
		std::printf("FAILED: %d of %d checks\n", gFailures, gChecks);

	return gFailures == 0 ? 0 : 1;
	}
