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

#include "../include/NxCooking.h"

#include <cmath>
#include <cstdio>
#include <cstring>

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

/*
 * A box dropped onto a static box comes to rest on top of it.
 *
 * This is the first scenario that needs shapes, contacts and the solver, and
 * the resting height is the thing worth measuring: 2.8's skin width is
 * *permitted interpenetration*, so shapes rest overlapping rather than exactly
 * touching. Bullet resolves to its own allowed penetration instead, which is
 * why the tolerance below is loose and why the check is asserted as a bound
 * rather than an equality -- see the note in NxShapeDesc.h. Tightening it is
 * what implementing skin width properly will look like.
 */
void TestBoxRestsOnGround()
	{
	std::printf("resting on a static box\n");

	NxSceneDesc sceneDesc;
	sceneDesc.gravity.set(NxVec3(0.0f, 0.0f, -9.81f));

	px28::Scene scene(sceneDesc);

	/* Ground: a static actor with a wide, flat box, top surface at z = 0. */
	NxActorDesc groundDesc;
	groundDesc.globalPose.t.set(NxVec3(0.0f, 0.0f, -1.0f));

	NxActor* ground = scene.createActor(groundDesc);
	Check(ground != NULL, "ground actor created");
	if (!ground)
		return;

	NxBoxShapeDesc groundShape;
	groundShape.dimensions.set(NxVec3(50.0f, 50.0f, 1.0f));
	Check(ground->createShape(groundShape) != NULL, "ground shape created");
	Check(ground->getNbShapes() == 1, "ground has one shape");

	/* Faller: a unit cube (half-extent 0.5) starting well above. */
	NxBodyDesc bodyDesc;
	bodyDesc.mass = 5.0f;

	NxActorDesc boxDesc;
	boxDesc.body = &bodyDesc;
	boxDesc.globalPose.t.set(NxVec3(0.0f, 0.0f, 6.0f));

	NxActor* box = scene.createActor(boxDesc);
	Check(box != NULL, "falling actor created");
	if (!box)
		return;

	NxBoxShapeDesc boxShape;
	boxShape.dimensions.set(NxVec3(0.5f, 0.5f, 0.5f));
	NxShape* shape = box->createShape(boxShape);
	Check(shape != NULL, "falling shape created");

	/* The downcasts the game relies on. */
	Check(shape->isBox() != NULL, "isBox() downcasts a box shape");
	Check(shape->isSphere() == NULL, "isSphere() returns NULL for a box");
	Check(shape->isWheel() == NULL, "isWheel() returns NULL for a box");
	Check(shape->getType() == NX_SHAPE_BOX, "getType() reports NX_SHAPE_BOX");
	Check(&shape->getActor() == box, "getActor() returns the owning actor");
	CheckVecNear(shape->isBox()->getDimensions(), NxVec3(0.5f, 0.5f, 0.5f), 1e-5f,
	             "box half-extents survive the round trip");

	for (int i = 0; i < 240; ++i)
		{
		scene.simulate(1.0f / 60.0f);
		scene.flushStream();
		scene.fetchResults(NX_RIGID_BODY_FINISHED, true);
		}

	const NxVec3 pos = box->getGlobalPosition();

	/* It fell -- the precondition that stops everything below passing by
	   accident on a scene that never simulated. */
	Check(pos.z < 6.0f, "the box actually fell");

	/* And it stopped, on top of the ground rather than through it. Centre at
	   half-extent above z = 0, give or take the resting penetration. */
	CheckNear(pos.z, 0.5f, 0.05f, "the box rests on the ground");

	const NxVec3 velocity = box->getLinearVelocity();
	Check(std::fabs(velocity.z) < 0.1f, "the box has come to rest");

	scene.releaseActor(*box);
	scene.releaseActor(*ground);
	}

/* getShapes() must come back in creation order: the engine's
   Actor::UnpackActorShapeListIncludeChildren walks it positionally. */
void TestShapeOrder()
	{
	std::printf("shape order\n");

	NxSceneDesc sceneDesc;
	px28::Scene scene(sceneDesc);

	NxActorDesc actorDesc;
	NxActor* actor = scene.createActor(actorDesc);
	if (!actor)
		{
		Check(false, "actor created");
		return;
		}

	NxBoxShapeDesc box;
	box.dimensions.set(NxVec3(1.0f, 1.0f, 1.0f));
	NxSphereShapeDesc sphere;
	sphere.radius = 2.0f;
	NxCapsuleShapeDesc capsule;
	capsule.radius = 0.5f;
	capsule.height = 3.0f;

	NxShape* first = actor->createShape(box);
	NxShape* second = actor->createShape(sphere);
	NxShape* third = actor->createShape(capsule);

	Check(actor->getNbShapes() == 3, "three shapes");

	NxShape*const* shapes = actor->getShapes();
	Check(shapes[0] == first, "getShapes()[0] is the first created");
	Check(shapes[1] == second, "getShapes()[1] is the second created");
	Check(shapes[2] == third, "getShapes()[2] is the third created");

	Check(shapes[1]->isSphere() != NULL, "the sphere downcasts");
	CheckNear(shapes[1]->isSphere()->getRadius(), 2.0f, 1e-5f, "sphere radius");

	/* 2.8's capsule: Y axis, and `height` is the full cylinder length between
	   cap centres -- so total length is height + 2*radius, not height. */
	Check(shapes[2]->isCapsule() != NULL, "the capsule downcasts");
	CheckNear(shapes[2]->isCapsule()->getRadius(), 0.5f, 1e-5f, "capsule radius");
	CheckNear(shapes[2]->isCapsule()->getHeight(), 3.0f, 1e-5f, "capsule height");

	/* Releasing the middle one keeps the rest in order. */
	actor->releaseShape(*second);
	Check(actor->getNbShapes() == 2, "two shapes after a release");
	shapes = actor->getShapes();
	Check(shapes[0] == first, "order survives a release: [0]");
	Check(shapes[1] == third, "order survives a release: [1]");

	scene.releaseActor(*actor);
	}

/*
 * Material indices, reproducing DataBase.cpp:4364-4397 exactly.
 *
 * This is the highest-consequence check in the harness and it needs no
 * simulation at all. The indices are serialised into db.xml, so getting the
 * allocation order wrong swaps every track surface's friction for another's
 * with no error anywhere -- the game would simply handle differently.
 */
void TestMaterialIndices()
	{
	std::printf("material indices\n");

	NxSceneDesc sceneDesc;
	px28::Scene scene(sceneDesc);

	/* Before anything is created, slot 0 exists and is the scene default. */
	Check(scene.getMaterialFromIndex(0) != NULL, "index 0 is the scene default");
	Check(scene.getNbMaterials() == 1, "a fresh scene has one material");

	/* The five DataBase creates, in its order. */
	NxMaterialDesc carMaterial1;
	carMaterial1.staticFriction = 0.08f;
	carMaterial1.dynamicFriction = 0.08f;
	carMaterial1.staticFrictionV = 3.2f;
	carMaterial1.dynamicFrictionV = 2.0f;
	carMaterial1.dirOfAnisotropy = NxVec3(0, 0, 1.0f);
	carMaterial1.flags = NX_MF_ANISOTROPIC | NX_MF_DISABLE_STRONG_FRICTION;

	NxMaterialDesc carMaterial2 = carMaterial1;
	carMaterial2.staticFriction = 0.02f;
	carMaterial2.dynamicFriction = 0.02f;

	NxMaterialDesc wheelMaterial;
	wheelMaterial.flags = NX_MF_DISABLE_FRICTION;

	NxMaterialDesc trackMaterial;
	NxMaterialDesc borderMaterial;

	NxMaterial* car1   = scene.createMaterial(carMaterial1);
	NxMaterial* car2   = scene.createMaterial(carMaterial2);
	NxMaterial* wheel  = scene.createMaterial(wheelMaterial);
	NxMaterial* track  = scene.createMaterial(trackMaterial);
	NxMaterial* border = scene.createMaterial(borderMaterial);

	Check(car1->getMaterialIndex() == 1, "car material 1 is index 1");
	Check(car2->getMaterialIndex() == 2, "car material 2 is index 2");
	Check(wheel->getMaterialIndex() == 3, "wheel material is index 3");

	/* The two that db.xml stores: 66 shapes reference 4 and 59 reference 5. */
	Check(track->getMaterialIndex() == 4, "track material is index 4, as db.xml stores");
	Check(border->getMaterialIndex() == 5, "border material is index 5, as db.xml stores");

	Check(scene.getNbMaterials() == 6, "six materials including the default");

	/* Lookup by index round-trips, which is how a shape resolves its own. */
	Check(scene.getMaterialFromIndex(4) == track, "index 4 resolves to the track material");
	Check(scene.getMaterialFromIndex(5) == border, "index 5 resolves to the border material");
	Check(scene.getMaterialFromIndex(99) == NULL, "an unused index resolves to NULL");

	/* The descriptor survives, including the anisotropy the car materials set. */
	NxMaterialDesc readBack;
	car1->saveToDesc(readBack);
	CheckNear(readBack.staticFriction, 0.08f, 1e-6f, "static friction round trips");
	CheckNear(readBack.dynamicFrictionV, 2.0f, 1e-6f, "dynamicFrictionV round trips");
	CheckVecNear(readBack.dirOfAnisotropy, NxVec3(0, 0, 1.0f), 1e-6f,
	             "dirOfAnisotropy round trips");
	Check((readBack.flags & NX_MF_ANISOTROPIC) != 0, "NX_MF_ANISOTROPIC round trips");

	/*
	 * Releasing must not compact. Indices are shape-visible and serialised, so
	 * shifting later materials down would silently repoint every shape that
	 * referenced one -- exactly the failure this ordering exists to prevent.
	 */
	scene.releaseMaterial(*wheel);
	Check(scene.getMaterialFromIndex(3) == NULL, "a released material leaves a hole");
	Check(scene.getMaterialFromIndex(4) == track, "the track material keeps index 4");
	Check(scene.getMaterialFromIndex(5) == border, "the border material keeps index 5");
	Check(scene.getNbMaterials() == 5, "the released material is no longer counted");
	}

/*
 * The exact 8x8 group matrix from Scene::Scene (Physx.cpp:62-79), pair by pair.
 *
 * The group names are the engine's own CollDisGroup enum, repeated here rather
 * than included: the harness deliberately does not depend on Rock3dEngine, and
 * a check that read the same header it is verifying would agree with itself
 * whatever the values were.
 */
void TestGroupCollisionMatrix()
	{
	std::printf("collision group matrix\n");

	enum
		{
		cdgDefault = 0, cdgShot = 1, cdgShotBorder = 2, cdgShotTransparency = 3,
		cdgWheel = 4, cdgShotTrack = 5, cdgTrackPlane = 6, cdgPlaneDeath = 7
		};

	NxSceneDesc sceneDesc;
	px28::Scene scene(sceneDesc);

	/* Everything collides until told otherwise. */
	Check(scene.getGroupCollisionFlag(cdgShot, cdgShot), "pairs start enabled");
	Check(scene.getGroupCollisionFlag(cdgDefault, cdgPlaneDeath), "pairs start enabled");

	scene.setGroupCollisionFlag(cdgShot, cdgShot, false);
	scene.setGroupCollisionFlag(cdgShotBorder, cdgShotBorder, false);
	scene.setGroupCollisionFlag(cdgShotBorder, cdgShot, false);
	scene.setGroupCollisionFlag(cdgShotTrack, cdgShot, false);
	scene.setGroupCollisionFlag(cdgShotTrack, cdgShotBorder, false);
	scene.setGroupCollisionFlag(cdgShotTrack, cdgShotTrack, false);
	scene.setGroupCollisionFlag(cdgShotTransparency, cdgShot, false);
	scene.setGroupCollisionFlag(cdgWheel, cdgShot, false);
	scene.setGroupCollisionFlag(cdgWheel, cdgShotBorder, false);
	scene.setGroupCollisionFlag(cdgWheel, cdgShotTrack, false);
	scene.setGroupCollisionFlag(cdgWheel, cdgShotTransparency, false);
	scene.setGroupCollisionFlag(cdgTrackPlane, cdgShot, false);
	scene.setGroupCollisionFlag(cdgTrackPlane, cdgShotBorder, false);

	/* The thirteen pairs the game disables, each read back both ways round --
	   Scene::Scene only ever sets one direction and expects both off. */
	const int disabled[][2] =
		{
		{cdgShot, cdgShot},
		{cdgShotBorder, cdgShotBorder},
		{cdgShotBorder, cdgShot},
		{cdgShotTrack, cdgShot},
		{cdgShotTrack, cdgShotBorder},
		{cdgShotTrack, cdgShotTrack},
		{cdgShotTransparency, cdgShot},
		{cdgWheel, cdgShot},
		{cdgWheel, cdgShotBorder},
		{cdgWheel, cdgShotTrack},
		{cdgWheel, cdgShotTransparency},
		{cdgTrackPlane, cdgShot},
		{cdgTrackPlane, cdgShotBorder},
		};

	const int disabledCount = sizeof(disabled) / sizeof(disabled[0]);

	for (int i = 0; i < disabledCount; ++i)
		{
		char label[96];
		std::snprintf(label, sizeof(label), "group pair (%d,%d) is disabled",
		              disabled[i][0], disabled[i][1]);
		Check(!scene.getGroupCollisionFlag(disabled[i][0], disabled[i][1]), label);

		std::snprintf(label, sizeof(label), "group pair (%d,%d) is disabled symmetrically",
		              disabled[i][1], disabled[i][0]);
		Check(!scene.getGroupCollisionFlag(disabled[i][1], disabled[i][0]), label);
		}

	/*
	 * And every other pair of the eight groups in use is still enabled. This is
	 * the half that catches an over-broad disable -- checking only the thirteen
	 * would pass just as well if setGroupCollisionFlag turned everything off.
	 */
	int stillEnabled = 0;
	for (int a = 0; a <= cdgPlaneDeath; ++a)
		for (int b = a; b <= cdgPlaneDeath; ++b)
			{
			bool expected = true;
			for (int i = 0; i < disabledCount; ++i)
				if ((disabled[i][0] == a && disabled[i][1] == b) ||
				    (disabled[i][0] == b && disabled[i][1] == a))
					expected = false;

			if (expected)
				{
				char label[96];
				std::snprintf(label, sizeof(label), "group pair (%d,%d) is still enabled", a, b);
				Check(scene.getGroupCollisionFlag(a, b), label);
				++stillEnabled;
				}
			}

	Check(stillEnabled == 23, "23 of the 36 pairs among the eight groups remain enabled");
	}

/*
 * A projectile with NX_AF_DISABLE_RESPONSE passes through what it hits.
 *
 * Every weapon in the game depends on this: Weapon.cpp sets the flag on six
 * projectile types so a shot scores its damage from the contact without being
 * deflected by it. The contact still has to be *reported* -- that half waits on
 * contact reports, and is noted here rather than silently untested.
 */
void TestDisableResponsePassesThrough()
	{
	std::printf("NX_AF_DISABLE_RESPONSE\n");

	NxSceneDesc sceneDesc;
	sceneDesc.gravity.set(NxVec3(0.0f, 0.0f, -9.81f));

	px28::Scene scene(sceneDesc);

	NxActorDesc wallDesc;
	wallDesc.globalPose.t.set(NxVec3(0.0f, 0.0f, 0.0f));
	NxActor* wall = scene.createActor(wallDesc);

	NxBoxShapeDesc wallShape;
	wallShape.dimensions.set(NxVec3(50.0f, 50.0f, 1.0f));
	wall->createShape(wallShape);

	/* Two identical projectiles, one of which ignores contact response. */
	NxBodyDesc bodyDesc;
	bodyDesc.mass = 1.0f;

	NxActorDesc shotDesc;
	shotDesc.body = &bodyDesc;
	shotDesc.globalPose.t.set(NxVec3(0.0f, 0.0f, 5.0f));

	NxActor* solid = scene.createActor(shotDesc);
	NxActor* ghost = scene.createActor(shotDesc);

	NxBoxShapeDesc shotShape;
	shotShape.dimensions.set(NxVec3(0.2f, 0.2f, 0.2f));
	solid->createShape(shotShape);
	ghost->createShape(shotShape);

	ghost->raiseActorFlag(NX_AF_DISABLE_RESPONSE);
	Check(ghost->readActorFlag(NX_AF_DISABLE_RESPONSE), "the flag reads back");
	Check(!solid->readActorFlag(NX_AF_DISABLE_RESPONSE), "the other actor is unaffected");

	for (int i = 0; i < 180; ++i)
		{
		scene.simulate(1.0f / 60.0f);
		scene.flushStream();
		scene.fetchResults(NX_RIGID_BODY_FINISHED, true);
		}

	/* The ordinary one stops on top of the wall. */
	Check(solid->getGlobalPosition().z > 0.5f,
	      "the ordinary projectile is stopped by the wall");

	/* The flagged one is somewhere below it, still falling. */
	Check(ghost->getGlobalPosition().z < -1.0f,
	      "the NX_AF_DISABLE_RESPONSE projectile passes through");

	/* Clearing the flag puts it back, so the mapping is not one-way. */
	ghost->clearActorFlag(NX_AF_DISABLE_RESPONSE);
	Check(!ghost->readActorFlag(NX_AF_DISABLE_RESPONSE), "the flag clears");
	}

/* setActorPairFlags(NX_IGNORE_PAIR) stops one specific pair colliding,
   whatever the group matrix says -- GameBase.cpp:722 and Weapon.cpp:1584 use it
   so a weapon does not hit the car that fired it. */
void TestActorPairFlags()
	{
	std::printf("actor pair flags\n");

	NxSceneDesc sceneDesc;
	sceneDesc.gravity.set(NxVec3(0.0f, 0.0f, -9.81f));

	px28::Scene scene(sceneDesc);

	NxActorDesc groundDesc;
	NxActor* ground = scene.createActor(groundDesc);
	NxBoxShapeDesc groundShape;
	groundShape.dimensions.set(NxVec3(50.0f, 50.0f, 1.0f));
	ground->createShape(groundShape);

	NxBodyDesc bodyDesc;
	bodyDesc.mass = 1.0f;
	NxActorDesc boxDesc;
	boxDesc.body = &bodyDesc;
	boxDesc.globalPose.t.set(NxVec3(0.0f, 0.0f, 5.0f));

	NxActor* box = scene.createActor(boxDesc);
	NxBoxShapeDesc boxShape;
	boxShape.dimensions.set(NxVec3(0.2f, 0.2f, 0.2f));
	box->createShape(boxShape);

	Check(scene.getActorPairFlags(*box, *ground) == 0, "no pair flags by default");

	scene.setActorPairFlags(*box, *ground, NX_IGNORE_PAIR);
	Check(scene.getActorPairFlags(*box, *ground) == NX_IGNORE_PAIR, "the flag reads back");

	/* And the other way round, because the game sets it in one order and the
	   filter is asked in whichever order Bullet happens to use. */
	Check(scene.getActorPairFlags(*ground, *box) == NX_IGNORE_PAIR,
	      "pair flags are order-independent");

	for (int i = 0; i < 180; ++i)
		{
		scene.simulate(1.0f / 60.0f);
		scene.flushStream();
		scene.fetchResults(NX_RIGID_BODY_FINISHED, true);
		}

	Check(box->getGlobalPosition().z < -1.0f, "an ignored pair does not collide");
	}

/*
 * Contact reports and the stream iterator.
 *
 * A box is dropped on a static box and the contacts it generates while resting
 * are inspected. Resting is the right moment to measure: the impulse over a
 * step is then exactly what holds the box up, so sumNormalForce has a closed
 * form -- m*g -- and can be checked against arithmetic rather than against
 * whatever the solver happened to produce.
 */
class RecordingReport: public NxUserContactReport
	{
	public:
	RecordingReport()
		: pairs(0), points(0), lastForce(0.0f, 0.0f, 0.0f),
		  normalPointsUp(false), shapesResolved(false), patchesSeen(0) {}

	virtual void onContactNotify(NxContactPair& pair, NxU32)
		{
		++pairs;
		lastForce = pair.sumNormalForce;

		Check(!pair.isDeletedActor[0] && !pair.isDeletedActor[1],
		      "neither actor in a live contact is reported deleted");

		NxContactStreamIterator iter(pair.stream);

		while (iter.goNextPair())
			{
			/* Both sides resolve to a real shape -- this is what
			   GameObject::ContainsContactGroup depends on. */
			if (iter.getShape(0) && iter.getShape(1))
				shapesResolved = true;

			while (iter.goNextPatch())
				{
				++patchesSeen;

				/* The contact is on top of the ground, so the normal is along
				   z. Which sign depends on which body Bullet made body0, so
				   the magnitude is what is asserted. */
				if (std::fabs(iter.getPatchNormal().z) > 0.9f)
					normalPointsUp = true;

				while (iter.goNextPoint())
					{
					++points;
					iter.getPoint();
					iter.getSeparation();
					iter.getPointNormalForce();
					}
				}
			}
		}

	int pairs;
	int points;
	NxVec3 lastForce;
	bool normalPointsUp;
	bool shapesResolved;
	int patchesSeen;
	};

void TestContactReports()
	{
	std::printf("contact reports\n");

	NxSceneDesc sceneDesc;
	sceneDesc.gravity.set(NxVec3(0.0f, 0.0f, -9.81f));

	px28::Scene scene(sceneDesc);

	RecordingReport report;
	scene.setUserContactReport(&report);

	NxActorDesc groundDesc;
	groundDesc.globalPose.t.set(NxVec3(0.0f, 0.0f, -1.0f));
	NxActor* ground = scene.createActor(groundDesc);
	NxBoxShapeDesc groundShape;
	groundShape.dimensions.set(NxVec3(50.0f, 50.0f, 1.0f));
	ground->createShape(groundShape);

	const float mass = 5.0f;

	NxBodyDesc bodyDesc;
	bodyDesc.mass = mass;
	NxActorDesc boxDesc;
	boxDesc.body = &bodyDesc;
	boxDesc.globalPose.t.set(NxVec3(0.0f, 0.0f, 3.0f));

	NxActor* box = scene.createActor(boxDesc);
	NxBoxShapeDesc boxShape;
	boxShape.dimensions.set(NxVec3(0.5f, 0.5f, 0.5f));
	box->createShape(boxShape);

	/* Let it land and settle. */
	for (int i = 0; i < 300; ++i)
		{
		scene.simulate(1.0f / 60.0f);
		scene.flushStream();
		scene.fetchResults(NX_RIGID_BODY_FINISHED, true);
		}

	Check(report.pairs > 0, "a contact pair was reported");
	Check(report.points > 0, "the stream yielded contact points");
	Check(report.patchesSeen > 0, "the stream yielded a patch");
	Check(report.shapesResolved, "getShape() resolves both sides to a shape");
	Check(report.normalPointsUp, "the patch normal is along the contact axis");

	/*
	 * A resting box is held up by exactly its weight, so the normal force over
	 * the last step should be m*g. This is the check that would catch the
	 * impulse-versus-force confusion: Bullet accumulates an impulse over the
	 * step and 2.8 reports a force, and forgetting to divide by dt would make
	 * this come out 60 times too small.
	 */
	const float weight = mass * 9.81f;
	CheckNear(std::fabs(report.lastForce.z), weight, weight * 0.2f,
	          "sumNormalForce on a resting box is its weight");

	scene.setUserContactReport(NULL);
	}

/* An iterator over an empty stream yields nothing and does not walk off the
   end -- the game constructs one per contact without checking first. */
void TestEmptyContactStream()
	{
	std::printf("empty contact stream\n");

	NxContactStreamIterator iter(NULL);

	Check(!iter.goNextPair(), "an empty stream has no pairs");
	Check(!iter.goNextPatch(), "an empty stream has no patches");
	Check(!iter.goNextPoint(), "an empty stream has no points");
	Check(iter.getNumPairs() == 0, "an empty stream reports zero pairs");
	Check(iter.getShape(0) == NULL, "an empty stream has no shape 0");
	Check(iter.isDeletedShape(0), "a missing shape reads as deleted");
	}

/* Raycasts, with 2.8's three filters: static/dynamic, the 32-bit group mask,
   and distance. Player.cpp:1727 uses all three at once. */
void TestRaycasts()
	{
	std::printf("raycasts\n");

	enum { cdgDefault = 0, cdgShot = 1, cdgTrackPlane = 6 };

	NxSceneDesc sceneDesc;
	px28::Scene scene(sceneDesc);

	/* A static slab, top at z = 0, in the track-plane group. */
	NxActorDesc groundDesc;
	groundDesc.globalPose.t.set(NxVec3(0.0f, 0.0f, -1.0f));
	NxActor* ground = scene.createActor(groundDesc);

	NxBoxShapeDesc groundShape;
	groundShape.dimensions.set(NxVec3(50.0f, 50.0f, 1.0f));
	groundShape.group = cdgTrackPlane;
	NxShape* groundShapePtr = ground->createShape(groundShape);

	NxRaycastHit hit;

	/* Straight down from 10 units up: hits at z = 0, distance 10. */
	NxRay down(NxVec3(0.0f, 0.0f, 10.0f), NxVec3(0.0f, 0.0f, -1.0f));
	NxShape* found = scene.raycastClosestShape(down, NX_STATIC_SHAPES, hit,
	                                           1 << cdgTrackPlane, NX_MAX_F32,
	                                           NX_RAYCAST_SHAPE, NULL, NULL);

	Check(found == groundShapePtr, "the ray hits the ground shape");
	CheckNear(hit.distance, 10.0f, 1e-2f, "the hit distance is the drop");
	CheckNear(hit.worldImpact.z, 0.0f, 1e-2f, "the impact is on the top face");
	CheckNear(hit.worldNormal.z, 1.0f, 1e-2f, "the surface normal points up");

	/* A group mask that excludes the ground's group finds nothing -- this is
	   the filter Player::ResetCar leans on to ignore everything but the track. */
	NxShape* filtered = scene.raycastClosestShape(down, NX_STATIC_SHAPES, hit,
	                                              1 << cdgShot, NX_MAX_F32,
	                                              NX_RAYCAST_SHAPE, NULL, NULL);
	Check(filtered == NULL, "a group mask that excludes the shape finds nothing");

	/* Asking only for dynamic shapes skips a static one. */
	NxShape* dynamicOnly = scene.raycastClosestShape(down, NX_DYNAMIC_SHAPES, hit,
	                                                 0xffffffff, NX_MAX_F32,
	                                                 NX_RAYCAST_SHAPE, NULL, NULL);
	Check(dynamicOnly == NULL, "NX_DYNAMIC_SHAPES skips a static actor");

	/* maxDist that stops short finds nothing. */
	NxShape* tooShort = scene.raycastClosestShape(down, NX_ALL_SHAPES, hit,
	                                              0xffffffff, 5.0f,
	                                              NX_RAYCAST_SHAPE, NULL, NULL);
	Check(tooShort == NULL, "a ray that stops short of the shape misses");

	/* Pointing away misses. */
	NxRay up(NxVec3(0.0f, 0.0f, 10.0f), NxVec3(0.0f, 0.0f, 1.0f));
	NxShape* wrongWay = scene.raycastClosestShape(up, NX_ALL_SHAPES, hit,
	                                              0xffffffff, NX_MAX_F32,
	                                              NX_RAYCAST_SHAPE, NULL, NULL);
	Check(wrongWay == NULL, "a ray pointing away misses");

	scene.releaseActor(*ground);
	}

/*
 * Cooking a triangle mesh and instancing it, which is the track's path:
 * Physx.cpp:470 cooks into a MemoryWriteBuffer and reads it straight back.
 *
 * The stream here is the harness's own NxStream, which is what makes this a
 * real round trip rather than a shortcut -- the shim never sees the buffer.
 */
class MemoryStream: public NxStream
	{
	public:
	MemoryStream(): _readPos(0) {}

	virtual NxU8 readByte() const   { NxU8 v = 0; readBuffer(&v, sizeof(v)); return v; }
	virtual NxU16 readWord() const  { NxU16 v = 0; readBuffer(&v, sizeof(v)); return v; }
	virtual NxU32 readDword() const { NxU32 v = 0; readBuffer(&v, sizeof(v)); return v; }
	virtual float readFloat() const { float v = 0; readBuffer(&v, sizeof(v)); return v; }
	virtual double readDouble() const { double v = 0; readBuffer(&v, sizeof(v)); return v; }

	virtual void readBuffer(void* buffer, NxU32 size) const
		{
		if (_readPos + size > _data.size())
			return;
		std::memcpy(buffer, &_data[_readPos], size);
		_readPos += size;
		}

	virtual NxStream& storeByte(NxU8 b)     { return storeBuffer(&b, sizeof(b)); }
	virtual NxStream& storeWord(NxU16 w)    { return storeBuffer(&w, sizeof(w)); }
	virtual NxStream& storeDword(NxU32 d)   { return storeBuffer(&d, sizeof(d)); }
	virtual NxStream& storeFloat(NxReal f)  { return storeBuffer(&f, sizeof(f)); }
	virtual NxStream& storeDouble(NxF64 f)  { return storeBuffer(&f, sizeof(f)); }

	virtual NxStream& storeBuffer(const void* buffer, NxU32 size)
		{
		const NxU8* bytes = static_cast<const NxU8*>(buffer);
		_data.insert(_data.end(), bytes, bytes + size);
		return *this;
		}

	private:
	std::vector<NxU8> _data;
	mutable size_t _readPos;
	};

void TestTriangleMeshCooking()
	{
	std::printf("triangle mesh cooking\n");

	NxPhysicsSDK* sdk = NxCreatePhysicsSDK(NX_PHYSICS_SDK_VERSION);
	Check(sdk != NULL, "the SDK is created at the right version");
	if (!sdk)
		return;

	/* A wrong version is refused rather than half-served. */
	NxSDKCreateError error = NXCE_NO_ERROR;
	Check(NxCreatePhysicsSDK(0x01020304, NULL, NULL, NxPhysicsSDKDesc(), &error) == NULL,
	      "a mismatched SDK version is refused");
	Check(error == NXCE_WRONG_VERSION, "and reports NXCE_WRONG_VERSION");

	NxCookingInterface* cooking = NxGetCookingLib(NX_PHYSICS_SDK_VERSION);
	Check(cooking != NULL, "the cooking library is available");
	Check(cooking->NxInitCooking(), "cooking initialises");

	/* Two triangles making a 20x20 square at z = 0. */
	const float vertices[] =
		{
		-10.0f, -10.0f, 0.0f,
		 10.0f, -10.0f, 0.0f,
		 10.0f,  10.0f, 0.0f,
		-10.0f,  10.0f, 0.0f,
		};
	const NxU32 indices[] = { 0, 1, 2, 0, 2, 3 };

	NxTriangleMeshDesc meshDesc;
	meshDesc.numVertices = 4;
	meshDesc.numTriangles = 2;
	meshDesc.pointStrideBytes = 3 * sizeof(float);
	meshDesc.triangleStrideBytes = 3 * sizeof(NxU32);
	meshDesc.points = vertices;
	meshDesc.triangles = indices;

	MemoryStream stream;
	Check(cooking->NxCookTriangleMesh(meshDesc, stream), "the mesh cooks");

	NxTriangleMesh* mesh = sdk->createTriangleMesh(stream);
	Check(mesh != NULL, "the cooked mesh reads back");
	if (!mesh)
		return;

	Check(mesh->getCount(0, 0) == 2, "the mesh has two triangles");

	/* An empty descriptor is rejected -- isValid() is the engine's "not loaded
	   yet" signal and createShape depends on it. */
	NxTriangleMeshShapeDesc emptyShape;
	Check(!emptyShape.isValid(), "a mesh shape with no meshData is invalid");

	/* Instance it, and check the geometry is really there by raycasting it. */
	NxSceneDesc sceneDesc;
	NxScene* scene = sdk->createScene(sceneDesc);

	NxActorDesc actorDesc;
	NxActor* actor = scene->createActor(actorDesc);

	NxTriangleMeshShapeDesc shapeDesc;
	shapeDesc.meshData = mesh;
	Check(shapeDesc.isValid(), "a mesh shape with meshData is valid");

	NxShape* shape = actor->createShape(shapeDesc);
	Check(shape != NULL, "the mesh shape is created");
	Check(shape && shape->isTriangleMesh() != NULL, "isTriangleMesh() downcasts");

	NxRaycastHit hit;
	NxRay down(NxVec3(1.0f, 1.0f, 5.0f), NxVec3(0.0f, 0.0f, -1.0f));
	NxShape* found = scene->raycastClosestShape(down, NX_ALL_SHAPES, hit,
	                                            0xffffffff, NX_MAX_F32,
	                                            NX_RAYCAST_SHAPE, NULL, NULL);

	Check(found == shape, "a ray hits the instanced mesh");
	CheckNear(hit.distance, 5.0f, 1e-2f, "at the distance the geometry implies");

	/* The same cooked mesh instanced twice -- TriangleMesh::GetOrCreateTri
	   reference-counts one per (mesh, scale), so several actors share it. */
	NxActor* second = scene->createActor(actorDesc);
	Check(second->createShape(shapeDesc) != NULL, "the same mesh instances twice");

	sdk->releaseScene(*scene);
	sdk->releaseTriangleMesh(*mesh);
	NxReleasePhysicsSDK(sdk);
	}

/*
 * Contact modification, which is the reason the backend is Bullet.
 *
 * GameCar::OnContactModify rebuilds the friction basis from the touched track
 * triangle via NX_CCC_LOCALORIENTATION0/1. PhysX 3+ and Jolt have no
 * per-contact friction orientation at all; btManifoldPoint's lateral friction
 * directions are the direct equivalent.
 */
class RecordingModify: public NxUserContactModify
	{
	public:
	RecordingModify(): calls(0), sawBothShapes(false), lastFriction(-1.0f) {}

	virtual bool onContactConstraint(NxU32& changeFlags,
	                                 const NxShape* shape0, const NxShape* shape1,
	                                 const NxU32, const NxU32,
	                                 NxContactCallbackData& data)
		{
		++calls;

		if (shape0 && shape1)
			sawBothShapes = true;

		lastFriction = data.dynamicFriction0;

		/* What GameCar does: zero the friction and hand back a basis. */
		data.dynamicFriction0 = 0.0f;
		data.staticFriction0 = 0.0f;
		data.localorientation0.id();

		changeFlags |= NX_CCC_LOCALORIENTATION0 | NX_CCC_LOCALORIENTATION1 |
		               NX_CCC_STATICFRICTION0 | NX_CCC_DYNAMICFRICTION0;

		return true;
		}

	int calls;
	bool sawBothShapes;
	NxReal lastFriction;
	};

void TestContactModification()
	{
	std::printf("contact modification\n");

	NxSceneDesc sceneDesc;
	sceneDesc.gravity.set(NxVec3(0.0f, 0.0f, -9.81f));

	px28::Scene scene(sceneDesc);

	RecordingModify modify;
	scene.setUserContactModify(&modify);

	NxActorDesc groundDesc;
	groundDesc.globalPose.t.set(NxVec3(0.0f, 0.0f, -1.0f));
	NxActor* ground = scene.createActor(groundDesc);
	NxBoxShapeDesc groundShape;
	groundShape.dimensions.set(NxVec3(50.0f, 50.0f, 1.0f));
	ground->createShape(groundShape);

	NxBodyDesc bodyDesc;
	bodyDesc.mass = 5.0f;
	NxActorDesc boxDesc;
	boxDesc.body = &bodyDesc;
	boxDesc.globalPose.t.set(NxVec3(0.0f, 0.0f, 2.0f));

	NxActor* box = scene.createActor(boxDesc);
	NxBoxShapeDesc boxShape;
	boxShape.dimensions.set(NxVec3(0.5f, 0.5f, 0.5f));
	box->createShape(boxShape);

	for (int i = 0; i < 200; ++i)
		{
		scene.simulate(1.0f / 60.0f);
		scene.flushStream();
		scene.fetchResults(NX_RIGID_BODY_FINISHED, true);
		}

	Check(modify.calls > 0, "onContactConstraint was called");
	Check(modify.sawBothShapes, "both shapes are passed to the callback");

	/* The callback is handed the material's friction, not zero -- a callback
	   that changes nothing must leave the contact as it was. */
	Check(modify.lastFriction >= 0.0f, "the callback receives a friction to modify");

	scene.setUserContactModify(NULL);
	}

/* getTriangle, which OnContactModify needs to rebuild the friction frame from
   the touched track surface. */
void TestMeshGetTriangle()
	{
	std::printf("mesh getTriangle\n");

	NxPhysicsSDK* sdk = NxCreatePhysicsSDK(NX_PHYSICS_SDK_VERSION);
	NxCookingInterface* cooking = NxGetCookingLib(NX_PHYSICS_SDK_VERSION);
	if (!sdk || !cooking)
		{
		Check(false, "SDK and cooking are available");
		return;
		}

	const float vertices[] =
		{
		0.0f, 0.0f, 0.0f,
		2.0f, 0.0f, 0.0f,
		0.0f, 3.0f, 0.0f,
		};
	const NxU32 indices[] = { 0, 1, 2 };

	NxTriangleMeshDesc meshDesc;
	meshDesc.numVertices = 3;
	meshDesc.numTriangles = 1;
	meshDesc.pointStrideBytes = 3 * sizeof(float);
	meshDesc.triangleStrideBytes = 3 * sizeof(NxU32);
	meshDesc.points = vertices;
	meshDesc.triangles = indices;

	MemoryStream stream;
	cooking->NxCookTriangleMesh(meshDesc, stream);
	NxTriangleMesh* mesh = sdk->createTriangleMesh(stream);

	NxSceneDesc sceneDesc;
	NxScene* scene = sdk->createScene(sceneDesc);

	/* Offset the actor, so world space is distinguishable from mesh space. */
	NxActorDesc actorDesc;
	actorDesc.globalPose.t.set(NxVec3(10.0f, 20.0f, 30.0f));
	NxActor* actor = scene->createActor(actorDesc);

	NxTriangleMeshShapeDesc shapeDesc;
	shapeDesc.meshData = mesh;
	NxShape* shape = actor->createShape(shapeDesc);

	const NxTriangleMeshShape* meshShape = shape->isTriangleMesh();
	Check(meshShape != NULL, "the shape downcasts to a mesh shape");
	if (!meshShape)
		return;

	NxTriangle tri;
	meshShape->getTriangle(tri, 0, 0, 0, true, true);

	/* World space: the mesh vertices plus the actor's position. */
	CheckVecNear(tri.verts[0], NxVec3(10.0f, 20.0f, 30.0f), 1e-4f,
	             "triangle vertex 0 in world space");
	CheckVecNear(tri.verts[1], NxVec3(12.0f, 20.0f, 30.0f), 1e-4f,
	             "triangle vertex 1 in world space");
	CheckVecNear(tri.verts[2], NxVec3(10.0f, 23.0f, 30.0f), 1e-4f,
	             "triangle vertex 2 in world space");

	sdk->releaseScene(*scene);
	NxReleasePhysicsSDK(sdk);
	}

/*
 * Skin width resolution: -1 means "use the SDK's global", anything else is the
 * shape's own. 228 shapes in db.xml take the global and 69 override it, so both
 * paths are live in shipped data.
 */
void TestSkinWidthResolution()
	{
	std::printf("skin width\n");

	NxPhysicsSDK* sdk = NxCreatePhysicsSDK(NX_PHYSICS_SDK_VERSION);
	if (!sdk)
		return;

	Check(sdk->setParameter(NX_SKIN_WIDTH, 0.025f), "the global skin width is settable");
	CheckNear(sdk->getParameter(NX_SKIN_WIDTH), 0.025f, 1e-6f,
	          "and reads back -- Manager::InitSDK sets exactly this");

	NxSceneDesc sceneDesc;
	NxScene* scene = sdk->createScene(sceneDesc);

	NxActorDesc actorDesc;
	NxActor* actor = scene->createActor(actorDesc);

	/* The default: -1, deferring to the global. */
	NxBoxShapeDesc inherits;
	inherits.dimensions.set(NxVec3(1.0f, 1.0f, 1.0f));
	CheckNear(inherits.skinWidth, -1.0f, 1e-6f, "a shape descriptor defaults to -1");
	NxShape* inheriting = actor->createShape(inherits);
	CheckNear(inheriting->getSkinWidth(), -1.0f, 1e-6f,
	          "and the shape reports -1, not the resolved value");

	/* The override db.xml uses for 69 shapes. */
	NxBoxShapeDesc overrides;
	overrides.dimensions.set(NxVec3(1.0f, 1.0f, 1.0f));
	overrides.skinWidth = 0.1f;
	NxShape* overriding = actor->createShape(overrides);
	CheckNear(overriding->getSkinWidth(), 0.1f, 1e-6f, "an override round trips");

	overriding->setSkinWidth(0.2f);
	CheckNear(overriding->getSkinWidth(), 0.2f, 1e-6f, "and is settable");

	sdk->releaseScene(*scene);
	NxReleasePhysicsSDK(sdk);
	}

/*
 * The centre-of-mass offset, which is where 2.8 and Bullet genuinely disagree:
 * 2.8 keeps globalPose (the actor origin) and massLocalPose (the COM) separate,
 * while a btRigidBody's world transform *is* its centre of mass.
 *
 * Every car sets an offset through bfLockCenterOfMass and Physx.cpp:1845
 * applies it on the actor creation path, so getting this wrong moves every
 * shape and makes addLocalForce generate torque 2.8 never produced.
 */
void TestCentreOfMassOffset()
	{
	std::printf("centre of mass\n");

	NxSceneDesc sceneDesc;
	px28::Scene scene(sceneDesc);

	NxBodyDesc bodyDesc;
	bodyDesc.mass = 10.0f;
	bodyDesc.massLocalPose.t.set(NxVec3(0.0f, 0.0f, -0.4f));

	NxActorDesc actorDesc;
	actorDesc.body = &bodyDesc;
	actorDesc.globalPose.t.set(NxVec3(1.0f, 2.0f, 3.0f));

	NxActor* actor = scene.createActor(actorDesc);
	if (!actor)
		{
		Check(false, "actor created");
		return;
		}

	NxBoxShapeDesc boxShape;
	boxShape.dimensions.set(NxVec3(0.5f, 0.5f, 0.5f));
	actor->createShape(boxShape);

	/* getGlobalPose is the ACTOR origin, not the centre of mass. */
	CheckVecNear(actor->getGlobalPosition(), NxVec3(1.0f, 2.0f, 3.0f), 1e-5f,
	             "globalPose is the actor origin, not the centre of mass");

	CheckVecNear(actor->getCMassLocalPosition(), NxVec3(0.0f, 0.0f, -0.4f), 1e-5f,
	             "the descriptor's massLocalPose is the COM offset");

	/* And the COM in world space is the actor origin plus the offset. */
	CheckVecNear(actor->getCMassGlobalPose().t, NxVec3(1.0f, 2.0f, 2.6f), 1e-5f,
	             "getCMassGlobalPose is the offset COM in world space");

	/* Moving the actor moves both, keeping the offset. */
	actor->setGlobalPosition(NxVec3(5.0f, 5.0f, 5.0f));
	CheckVecNear(actor->getGlobalPosition(), NxVec3(5.0f, 5.0f, 5.0f), 1e-5f,
	             "setGlobalPosition round trips through the offset");
	CheckVecNear(actor->getCMassGlobalPose().t, NxVec3(5.0f, 5.0f, 4.6f), 1e-5f,
	             "and the COM follows it");

	/* Changing the offset must NOT move the actor. */
	actor->setCMassOffsetLocalPosition(NxVec3(0.0f, 0.2f, 0.0f));
	CheckVecNear(actor->getGlobalPosition(), NxVec3(5.0f, 5.0f, 5.0f), 1e-5f,
	             "changing the COM offset leaves the actor where it was");
	CheckVecNear(actor->getCMassLocalPosition(), NxVec3(0.0f, 0.2f, 0.0f), 1e-5f,
	             "and the new offset reads back");

	/*
	 * addLocalForce applies at the centre of mass and must produce NO torque,
	 * offset or not. That is the whole reason the offset is tracked: applying
	 * at the actor origin would spin a car that 2.8 pushed straight.
	 */
	actor->setAngularVelocity(NxVec3(0.0f, 0.0f, 0.0f));
	actor->addLocalForce(NxVec3(100.0f, 0.0f, 0.0f), NX_FORCE, true);

	for (int i = 0; i < 10; ++i)
		{
		scene.simulate(1.0f / 60.0f);
		scene.flushStream();
		scene.fetchResults(NX_RIGID_BODY_FINISHED, true);
		}

	const NxVec3 spin = actor->getAngularVelocity();
	Check(std::fabs(spin.x) < 1e-3f && std::fabs(spin.y) < 1e-3f &&
	      std::fabs(spin.z) < 1e-3f,
	      "addLocalForce at an offset COM produces no torque");

	Check(actor->getLinearVelocity().x > 0.0f, "and it does accelerate the actor");

	scene.releaseActor(*actor);
	}

/* The rest of the actor surface the game calls: inertia, kinetic energy,
   damping, sleeping and saveToDesc. */
void TestActorRemainder()
	{
	std::printf("actor remainder\n");

	NxSceneDesc sceneDesc;
	px28::Scene scene(sceneDesc);

	NxBodyDesc bodyDesc;
	bodyDesc.mass = 4.0f;

	NxActorDesc actorDesc;
	actorDesc.body = &bodyDesc;
	actorDesc.globalPose.t.set(NxVec3(0.0f, 0.0f, 10.0f));
	actorDesc.flags = NX_AF_LOCK_COM;

	NxActor* actor = scene.createActor(actorDesc);
	NxBoxShapeDesc boxShape;
	boxShape.dimensions.set(NxVec3(1.0f, 1.0f, 1.0f));
	actor->createShape(boxShape);

	/* A uniform box's inertia is m/3 * (b^2 + c^2) for half-extents; with
	   half-extents 1 that is 4/3 * 2 = 2.667 on every axis. GameCar.cpp:733
	   multiplies this by a torque clamp every step. */
	const NxVec3 inertia = actor->getMassSpaceInertiaTensor();
	CheckNear(inertia.x, 2.6667f, 1e-2f, "box inertia tensor x");
	CheckNear(inertia.y, 2.6667f, 1e-2f, "box inertia tensor y");
	CheckNear(inertia.z, 2.6667f, 1e-2f, "box inertia tensor z");

	/* Kinetic energy: half m v^2 with no spin. */
	actor->setLinearVelocity(NxVec3(3.0f, 0.0f, 0.0f));
	actor->setAngularVelocity(NxVec3(0.0f, 0.0f, 0.0f));
	CheckNear(actor->computeKineticEnergy(), 0.5f * 4.0f * 9.0f, 1e-3f,
	          "kinetic energy is half m v squared");

	/* Plus half w.I.w when it spins. */
	actor->setLinearVelocity(NxVec3(0.0f, 0.0f, 0.0f));
	actor->setAngularVelocity(NxVec3(2.0f, 0.0f, 0.0f));
	CheckNear(actor->computeKineticEnergy(), 0.5f * 2.6667f * 4.0f, 1e-2f,
	          "and half omega I omega when spinning");

	actor->setLinearDamping(0.0f);

	/* Sleeping: GameCar wakes an actor after teleporting it. */
	actor->putToSleep();
	Check(actor->isSleeping(), "putToSleep sleeps the actor");
	actor->wakeUp();
	Check(!actor->isSleeping(), "wakeUp wakes it again");

	/* saveToDesc round-trips the pose and flags px::Actor reads back. */
	NxActorDesc saved;
	actor->saveToDesc(saved);
	CheckVecNear(saved.globalPose.t, NxVec3(0.0f, 0.0f, 10.0f), 1e-4f,
	             "saveToDesc carries the pose");
	Check((saved.flags & NX_AF_LOCK_COM) != 0, "and the actor flags");

	/* getGlobalOrientation, the NxMat33 form GameCar.cpp:725 uses. */
	NxQuat quarterTurn;
	quarterTurn.fromAngleAxis(90.0f, NxVec3(0, 0, 1));
	actor->setGlobalOrientationQuat(quarterTurn);

	NxVec3 rotatedX;
	actor->getGlobalOrientation().multiply(NxVec3(1, 0, 0), rotatedX);
	CheckVecNear(rotatedX, NxVec3(0, 1, 0), 1e-4f,
	             "getGlobalOrientation agrees with the quaternion form");

	scene.releaseActor(*actor);
	}

/* Convex cooking, the SDK's foundation, and the scene state the game sets. */
void TestSdkRemainder()
	{
	std::printf("SDK remainder\n");

	NxPhysicsSDK* sdk = NxCreatePhysicsSDK(NX_PHYSICS_SDK_VERSION);
	NxCookingInterface* cooking = NxGetCookingLib(NX_PHYSICS_SDK_VERSION);
	if (!sdk || !cooking)
		return;

	/* Physx.cpp:295 asks the foundation for a debugger and guards on
	   isConnected() before using it. */
	Check(sdk->getFoundationSDK().getRemoteDebugger() != NULL,
	      "the foundation offers a remote debugger");
	Check(!sdk->getFoundationSDK().getRemoteDebugger()->isConnected(),
	      "which is never connected");

	/* Convex cooking: TriangleMesh::GetOrCreateConvex cooks these, though
	   nothing ever builds a shape from one. */
	const float vertices[] =
		{
		0.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f, 1.0f,
		};

	NxConvexMeshDesc convexDesc;
	convexDesc.numVertices = 4;
	convexDesc.pointStrideBytes = 3 * sizeof(float);
	convexDesc.points = vertices;

	MemoryStream stream;
	Check(cooking->NxCookConvexMesh(convexDesc, stream), "a convex mesh cooks");

	NxConvexMesh* convex = sdk->createConvexMesh(stream);
	Check(convex != NULL, "and reads back");
	if (convex)
		{
		Check(convex->getCount(0) == 4, "with its four points");
		sdk->releaseConvexMesh(*convex);
		}

	NxScene* scene = sdk->createScene(NxSceneDesc());

	/* Weapon.cpp changes the filter ops around a raycast. */
	scene->setFilterOps(NX_FILTEROP_OR, NX_FILTEROP_OR, NX_FILTEROP_AND);
	scene->setFilterBool(true);

	/* World.cpp fixes the step at 1/60, which is what makes one substep
	   equivalent to 2.8. setTiming records it. */
	scene->setTiming(1.0f / 60.0f, 8, NX_TIMESTEP_FIXED);

	scene->setUserNotify(NULL);

	sdk->releaseScene(*scene);
	NxReleasePhysicsSDK(sdk);
	}

} /* namespace */

int main()
	{
	std::printf("PhysX28Harness\n================================\n");

	TestTransformConversion();
	TestFreeFall();
	TestStaticActorDoesNotFall();
	TestBoxRestsOnGround();
	TestShapeOrder();
	TestMaterialIndices();
	TestGroupCollisionMatrix();
	TestDisableResponsePassesThrough();
	TestActorPairFlags();
	TestContactReports();
	TestEmptyContactStream();
	TestRaycasts();
	TestTriangleMeshCooking();
	TestContactModification();
	TestMeshGetTriangle();
	TestSkinWidthResolution();
	TestCentreOfMassOffset();
	TestActorRemainder();
	TestSdkRemainder();

	std::printf("================================\n");
	if (gFailures == 0)
		std::printf("OK: %d checks, 0 failures\n", gChecks);
	else
		std::printf("FAILED: %d of %d checks\n", gFailures, gChecks);

	return gFailures == 0 ? 0 : 1;
	}
