/*
 * NxScene over btDiscreteDynamicsWorld.
 */

#include "Px28Impl.h"

#include <cstdio>
#include <cstdlib>

namespace px28
{

namespace
{
	/* Bullet asks this for every broadphase pair; it forwards the question to
	   the scene, which answers it in 2.8's terms. */
	class PairFilter: public btOverlapFilterCallback
		{
		public:
		explicit PairFilter(const Scene& scene): _scene(&scene) {}

		virtual bool needBroadphaseCollision(btBroadphaseProxy* a, btBroadphaseProxy* b) const
			{
			const btCollisionObject* objectA =
				static_cast<const btCollisionObject*>(a->m_clientObject);
			const btCollisionObject* objectB =
				static_cast<const btCollisionObject*>(b->m_clientObject);

			const Actor* actorA = static_cast<const Actor*>(objectA->getUserPointer());
			const Actor* actorB = static_cast<const Actor*>(objectB->getUserPointer());
			if (!actorA || !actorB)
				return true;

			return _scene->shouldCollide(*actorA, *actorB);
			}

		private:
		const Scene* _scene;
		};
}

Scene::Scene(const NxSceneDesc& desc)
	: _config(NULL), _dispatcher(NULL), _broadphase(NULL), _solver(NULL),
	  _world(NULL), _skinWidth(0.025f), _pendingStep(0.0f), _filter(NULL),
	  _contactReport(NULL), _contactModify(NULL), _userNotify(NULL),
	  _filterOp0(NX_FILTEROP_AND), _filterOp1(NX_FILTEROP_AND),
	  _filterOp2(NX_FILTEROP_AND), _filterBool(false),
	  _maxTimestep(1.0f / 60.0f), _maxIter(8)
	{
	_config = new btDefaultCollisionConfiguration();
	_dispatcher = new btCollisionDispatcher(_config);
	_broadphase = new btDbvtBroadphase();
	_solver = new btSequentialImpulseConstraintSolver();
	_world = new btDiscreteDynamicsWorld(_dispatcher, _broadphase, _solver, _config);

	_world->setGravity(ToBullet(desc.gravity));

	/* Slot 0 is the scene's default material, reserved before any call to
	   createMaterial so the game's first material lands on index 1. */
	_materials.push_back(new Material(NxMaterialDesc(), 0));

	/* Every group pair starts enabled; Scene::Scene turns fifteen of them off. */
	for (int i = 0; i < 32; ++i)
		for (int j = 0; j < 32; ++j)
			_groupCollision[i][j] = true;

	_filter = new PairFilter(*this);
	_broadphase->getOverlappingPairCache()->setOverlapFilterCallback(_filter);
	}

Scene::~Scene()
	{
	/* Actors first: each removes its rigid body from the world on the way out,
	   so the world has to outlive them. */
	while (!_actors.empty())
		{
		NxActor* actor = _actors.back();
		_actors.pop_back();
		delete static_cast<Actor*>(actor);
		}

	for (size_t i = 0; i < _materials.size(); ++i)
		delete _materials[i];

	for (size_t i = 0; i < _contactStreams.size(); ++i)
		delete _contactStreams[i];

	delete _world;
	delete _solver;
	delete _filter;
	delete _broadphase;
	delete _dispatcher;
	delete _config;
	}

/* ------------------------------------------------------------------ actors */

NxActor* Scene::createActor(const NxActorDesc& desc)
	{
	if (!desc.isValid())
		return NULL;

	Actor* actor = new Actor(*this, desc);
	_actors.push_back(actor);

	return actor;
	}

void Scene::releaseActor(NxActor& actor)
	{
	for (std::vector<NxActor*>::iterator iter = _actors.begin();
	     iter != _actors.end(); ++iter)
		if (*iter == &actor)
			{
			_actors.erase(iter);
			delete static_cast<Actor*>(&actor);
			return;
			}
	}

NxU32 Scene::getNbActors() const
	{
	return static_cast<NxU32>(_actors.size());
	}

NxActor** Scene::getActors()
	{
	return _actors.empty() ? NULL : &_actors[0];
	}

/* --------------------------------------------------------------- materials */

/*
 * The hidden contract, and the highest-consequence one in the whole shim.
 *
 * Indices are handed out sequentially from 1, with 0 reserved for the scene's
 * default material. Nothing in 2.8's API says so, and nothing checks it -- but
 * the result is serialised. DataBase.cpp:4364-4397 creates five materials in
 * this order: car1, car2, wheel, track, border. So the track is index 4 and the
 * border is 5, which is exactly what bin/Debug/db.xml stores: 66 shapes at
 * materialIndex 4 and 59 at 5.
 *
 * Index 3 never appears in the shipped data, and that is the independent
 * confirmation the ordering is right rather than merely plausible: the third
 * material is _nxWheelMaterial, which DataBase creates with
 * NX_MF_DISABLE_FRICTION and then never assigns to any shape.
 *
 * Get the allocation order wrong and every track surface silently swaps its
 * friction for another's, with no error anywhere.
 */
NxMaterial* Scene::createMaterial(const NxMaterialDesc& desc)
	{
	const NxMaterialIndex index = static_cast<NxMaterialIndex>(_materials.size());

	Material* material = new Material(desc, index);
	_materials.push_back(material);

	return material;
	}

void Scene::releaseMaterial(NxMaterial& material)
	{
	for (size_t i = 0; i < _materials.size(); ++i)
		if (_materials[i] == &material)
			{
			/* The slot is emptied, not erased: indices are shape-visible and
			   serialised, so compacting would repoint every shape that
			   referenced a later material. */
			delete _materials[i];
			_materials[i] = NULL;
			return;
			}
	}

NxMaterial* Scene::getMaterialFromIndex(NxMaterialIndex index)
	{
	return index < _materials.size() ? _materials[index] : NULL;
	}

NxU32 Scene::getNbMaterials() const
	{
	NxU32 count = 0;
	for (size_t i = 0; i < _materials.size(); ++i)
		if (_materials[i])
			++count;

	return count;
	}

/* --------------------------------------------------------------- filtering */

/*
 * Symmetric, because 2.8's is: Scene::Scene sets (cdgWheel, cdgShot) and never
 * (cdgShot, cdgWheel), and expects both directions off. Storing one triangle
 * would work equally well; storing both makes the lookup a single read and
 * makes getGroupCollisionFlag(b, a) answer without thinking about it.
 */
void Scene::setGroupCollisionFlag(NxCollisionGroup g1, NxCollisionGroup g2, bool enable)
	{
	if (g1 >= 32 || g2 >= 32)
		return;

	_groupCollision[g1][g2] = enable;
	_groupCollision[g2][g1] = enable;
	}

bool Scene::getGroupCollisionFlag(NxCollisionGroup g1, NxCollisionGroup g2) const
	{
	if (g1 >= 32 || g2 >= 32)
		return false;

	return _groupCollision[g1][g2];
	}

namespace
{
	std::pair<const NxActor*, const NxActor*> PairKey(const NxActor& a, const NxActor& b)
		{
		/* Ordered, so (a, b) and (b, a) are the same entry. */
		return &a < &b ? std::make_pair(&a, &b) : std::make_pair(&b, &a);
		}

	std::pair<const NxShape*, const NxShape*> ShapePairKey(const NxShape& a,
	                                                       const NxShape& b)
		{
		return &a < &b ? std::make_pair(&a, &b) : std::make_pair(&b, &a);
		}

	/* Bullet reports -1 for a child index when the body is not a compound. The
	   shim always builds a compound, so a valid index is the normal case and
	   this is the guard for the degenerate one. */
	NxShape* ShapeAt(const Actor& actor, int childIndex)
		{
		if (childIndex < 0 || static_cast<NxU32>(childIndex) >= actor.getNbShapes())
			return actor.getNbShapes() > 0 ? actor.getShapes()[0] : NULL;

		return actor.getShapes()[childIndex];
		}

	/*
	 * Bullet's closest-hit callback, taught 2.8's three filters: the
	 * static/dynamic split, the 32-bit group mask, and NxGroupsMask.
	 *
	 * needsCollision runs before the narrow phase and rejects by actor;
	 * addSingleResult refines to the specific shape, because groups live on
	 * shapes and a compound may hold several.
	 */
	class RayFilter: public btCollisionWorld::ClosestRayResultCallback
		{
		public:
		RayFilter(const btVector3& from, const btVector3& to, const Scene& scene,
		          NxShapesType shapeType, NxU32 groups, const NxGroupsMask* mask)
			: btCollisionWorld::ClosestRayResultCallback(from, to),
			  shape(NULL), _shapeType(shapeType), _groups(groups), _mask(mask)
			{
			(void)scene;
			}

		virtual bool needsCollision(btBroadphaseProxy* proxy) const
			{
			const btCollisionObject* object =
				static_cast<const btCollisionObject*>(proxy->m_clientObject);
			const Actor* actor = static_cast<const Actor*>(object->getUserPointer());
			if (!actor)
				return false;

			const bool dynamic = actor->isDynamic();
			if (dynamic && !(_shapeType & NX_DYNAMIC_SHAPES))
				return false;
			if (!dynamic && !(_shapeType & NX_STATIC_SHAPES))
				return false;

			return true;
			}

		virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult& result,
		                                 bool normalInWorldSpace)
			{
			const Actor* actor =
				static_cast<const Actor*>(result.m_collisionObject->getUserPointer());
			if (!actor || actor->getNbShapes() == 0)
				return 1.0f;

			/* m_localShapeInfo carries the compound child index, which is what
			   turns a hit on a body into a hit on a 2.8 shape. */
			int child = 0;
			if (result.m_localShapeInfo)
				child = result.m_localShapeInfo->m_triangleIndex;
			if (child < 0 || static_cast<NxU32>(child) >= actor->getNbShapes())
				child = 0;

			NxShape* candidate = actor->getShapes()[child];

			/* The 32-bit group mask: bit N set means group N is wanted. */
			if (!(_groups & (1u << candidate->getGroup())))
				return 1.0f;

			/* NxGroupsMask, which the game only ever uses through bits0 --
			   Weapon.cpp:293,310 and GameObject.cpp:202. A zero mask means
			   "no constraint" rather than "match nothing". */
			if (_mask)
				{
				const NxGroupsMask shapeMask = candidate->getGroupsMask();
				const bool any = _mask->bits0 || _mask->bits1 ||
				                 _mask->bits2 || _mask->bits3;
				if (any && !((shapeMask.bits0 & _mask->bits0) ||
				             (shapeMask.bits1 & _mask->bits1) ||
				             (shapeMask.bits2 & _mask->bits2) ||
				             (shapeMask.bits3 & _mask->bits3)))
					return 1.0f;
				}

			shape = candidate;

			return btCollisionWorld::ClosestRayResultCallback::addSingleResult(
				result, normalInWorldSpace);
			}

		NxShape* shape;

		private:
		NxShapesType _shapeType;
		NxU32 _groups;
		const NxGroupsMask* _mask;
		};
}

void Scene::setActorPairFlags(NxActor& a, NxActor& b, NxU32 flags)
	{
	if (flags == 0)
		_actorPairFlags.erase(PairKey(a, b));
	else
		_actorPairFlags[PairKey(a, b)] = flags;
	}

NxU32 Scene::getActorPairFlags(NxActor& a, NxActor& b) const
	{
	std::map<std::pair<const NxActor*, const NxActor*>, NxU32>::const_iterator iter =
		_actorPairFlags.find(PairKey(a, b));

	return iter == _actorPairFlags.end() ? 0 : iter->second;
	}

/*
 * The question Bullet's broadphase filter asks, answered in 2.8's terms.
 *
 * NX_IGNORE_PAIR is checked first because it is the more specific rule:
 * GameBase.cpp:722 and Weapon.cpp:1584 use it to stop a weapon colliding with
 * the car that fired it, regardless of what the group matrix says.
 *
 * Group is read from the actor's first shape. 2.8 puts the group on the shape,
 * not the actor, and Bullet's broadphase proxy is per body -- so an actor whose
 * shapes span several groups cannot be filtered exactly here. That does not
 * arise in this game (px::Actor assigns one group to every shape it creates),
 * but it is an assumption rather than a guarantee, so it is written down.
 */
bool Scene::shouldCollide(const Actor& a, const Actor& b) const
	{
	std::map<std::pair<const NxActor*, const NxActor*>, NxU32>::const_iterator iter =
		_actorPairFlags.find(PairKey(a, b));
	if (iter != _actorPairFlags.end() && (iter->second & NX_IGNORE_PAIR))
		return false;

	const NxShape*const* shapesA = a.getShapes();
	const NxShape*const* shapesB = b.getShapes();
	if (a.getNbShapes() == 0 || b.getNbShapes() == 0)
		return true;

	return getGroupCollisionFlag(shapesA[0]->getGroup(), shapesB[0]->getGroup());
	}


/* ---------------------------------------------------------------- stepping */

/*
 * 2.8 splits a step into three calls and the engine uses all three:
 * Scene::Compute does simulate(dt), flushStream(), then fetchResults(...,true).
 * Only fetchResults blocks, so simulate records the interval and fetchResults
 * is where the world actually advances. Nothing here is asynchronous, so the
 * split costs nothing and the game cannot tell.
 */
void Scene::simulate(NxReal elapsedTime)
	{
	_pendingStep = elapsedTime;
	}

void Scene::flushStream()
	{
	}

bool Scene::fetchResults(NxSimulationStatus, bool)
	{
	if (_pendingStep > 0.0f)
		{
		static unsigned physicsTraceStep = 0;
		const bool physicsTrace = std::getenv("RRR3D_PHYSICS_TRACE") != NULL;
		Actor* traceActor = NULL;
		if (physicsTrace && !_wheels.empty() && physicsTraceStep < 120)
			traceActor = static_cast<Actor*>(&_wheels.front()->getActor());

		if (traceActor)
			{
			const NxVec3 pos = traceActor->getGlobalPosition();
			const NxVec3 vel = traceActor->getLinearVelocity();
			const btVector3 gravity = _world->getGravity();
			std::fprintf(stderr,
				"physics step %u before actor %p dt %.6f mass %.3f sleeping %d "
				"posZ %.6f velZ %.6f gravityZ %.3f ccdRadius %.3f ccdThreshold %.3f\n",
				physicsTraceStep, static_cast<void*>(traceActor), double(_pendingStep),
				double(traceActor->getMass()), traceActor->isSleeping() ? 1 : 0,
				double(pos.z), double(vel.z), double(gravity.z()),
				double(traceActor->body()->getCcdSweptSphereRadius()),
				double(traceActor->body()->getCcdMotionThreshold()));
			}

		/*
		 * One substep, and the maxSubSteps argument is what enforces it.
		 *
		 * This is not a simplification: NxSceneDesc::maxTimestep defaults to
		 * 1/60 and World.cpp fixes the step at 1/60, so 2.8 ran exactly one
		 * substep too. That equivalence is why NX_SMOOTH_IMPULSE and
		 * NX_SMOOTH_VELOCITY_CHANGE were already identical to the non-smooth
		 * modes, and it is conditional on the step size -- which is why the
		 * step is asserted rather than assumed once setTiming is implemented.
		 */
		/*
		 * Collision detection, then modification, then the solver -- which is
		 * 2.8's order and the only one that works: onContactConstraint exists
		 * to rewrite a constraint *before* it is solved, so it has to run
		 * after the manifolds exist and before stepSimulation solves them.
		 *
		 * performDiscreteCollisionDetection builds the manifolds; the
		 * stepSimulation that follows reuses them rather than rebuilding, so
		 * the edits survive into the solve.
		 */
		if (_contactModify)
			{
			_world->performDiscreteCollisionDetection();
			if (traceActor)
				{
				for (int m = 0; m < _dispatcher->getNumManifolds(); ++m)
					{
					btPersistentManifold* manifold =
						_dispatcher->getManifoldByIndexInternal(m);
					Actor* actor0 = static_cast<Actor*>(
						manifold->getBody0()->getUserPointer());
					Actor* actor1 = static_cast<Actor*>(
						manifold->getBody1()->getUserPointer());
					if (actor0 != traceActor && actor1 != traceActor)
						continue;

					for (int p = 0; p < manifold->getNumContacts(); ++p)
						{
						const btManifoldPoint& point = manifold->getContactPoint(p);
						const btVector3& normal = point.m_normalWorldOnB;
						const btVector3 worldA = point.getPositionWorldOnA();
						const btVector3 worldB = point.getPositionWorldOnB();
						std::fprintf(stderr,
							"physics contact step %u targetSide %d otherMass %.3f "
							"distance %.6f normal %.3f %.3f %.3f "
							"pointA %.3f %.3f %.3f pointB %.3f %.3f %.3f "
							"child %d %d\n",
							physicsTraceStep, actor0 == traceActor ? 0 : 1,
							double(actor0 == traceActor ? actor1->getMass() : actor0->getMass()),
							double(point.getDistance()), double(normal.x()),
							double(normal.y()), double(normal.z()),
							double(worldA.x()), double(worldA.y()), double(worldA.z()),
							double(worldB.x()), double(worldB.y()), double(worldB.z()),
							point.m_index0, point.m_index1);
						}
					}
				}
			modifyContacts();
			}

		/*
		 * Wheels, before the solver.
		 *
		 * A 2.8 wheel's suspension and tire forces are part of the step, not
		 * something applied between steps: they are computed from the state at
		 * the start of the step and solved along with everything else. Running
		 * them afterwards would delay every wheel by a frame, which at 60Hz is
		 * a car that responds late to its own suspension.
		 */
		for (size_t i = 0; i < _wheels.size(); ++i)
			_wheels[i]->step(_pendingStep);

		/*
		 * A car deliberately dropped onto a thin triangle track can move farther
		 * than Bullet's contact envelope in one 60 Hz interval. Bullet does not
		 * run its swept CCD path for compound shapes (cars are compounds because
		 * their wheel placeholders share the actor), so the chassis can cross the
		 * one-sided road before the penetration solver recovers.
		 *
		 * Substep only that impact interval: a wheel has already found ground and
		 * the owning body can move farther than the scene skin width. Controls and
		 * wheel forces are still evaluated once above; ordinary free fall and
		 * settled driving retain the game's single 60 Hz rigid-body step.
		 */
		bool substepImpact = false;
		for (size_t i = 0; i < _wheels.size() && !substepImpact; ++i)
			{
			NxWheelContactData contact;
			if (_wheels[i]->getContact(contact) &&
				_wheels[i]->getActor().getLinearVelocity().magnitude() * _pendingStep > _skinWidth)
				substepImpact = true;
			}

		if (substepImpact)
			_world->stepSimulation(_pendingStep, 4, _pendingStep / 4.0f);
		else
			_world->stepSimulation(_pendingStep, 0, _pendingStep);

		if (traceActor)
			{
			for (int m = 0; m < _dispatcher->getNumManifolds(); ++m)
				{
				btPersistentManifold* manifold =
					_dispatcher->getManifoldByIndexInternal(m);
				Actor* actor0 = static_cast<Actor*>(
					manifold->getBody0()->getUserPointer());
				Actor* actor1 = static_cast<Actor*>(
					manifold->getBody1()->getUserPointer());
				if (actor0 != traceActor && actor1 != traceActor)
					continue;

				for (int p = 0; p < manifold->getNumContacts(); ++p)
					{
					const btManifoldPoint& point = manifold->getContactPoint(p);
					const btVector3& normal = point.m_normalWorldOnB;
					std::fprintf(stderr,
						"physics solved-contact step %u targetSide %d otherMass %.3f "
						"distance %.6f impulse %.3f normal %.3f %.3f %.3f child %d %d\n",
						physicsTraceStep, actor0 == traceActor ? 0 : 1,
						double(actor0 == traceActor ? actor1->getMass() : actor0->getMass()),
						double(point.getDistance()), double(point.getAppliedImpulse()),
						double(normal.x()), double(normal.y()), double(normal.z()),
						point.m_index0, point.m_index1);
					}
				}

			const NxVec3 pos = traceActor->getGlobalPosition();
			const NxVec3 vel = traceActor->getLinearVelocity();
			std::fprintf(stderr,
				"physics step %u after  actor %p posZ %.6f velZ %.6f sleeping %d\n",
				physicsTraceStep, static_cast<void*>(traceActor), double(pos.z),
				double(vel.z), traceActor->isSleeping() ? 1 : 0);
			++physicsTraceStep;
			}

		collectContacts(_pendingStep);

		_pendingStep = 0.0f;
		}

	return true;
	}

/* ----------------------------------------------------- contact modification */

/*
 * 2.8 calls onContactConstraint once per contact, before the solver runs, and
 * lets the callback rewrite the constraint -- friction magnitudes and, crucially
 * for this game, the friction *frame*.
 *
 * NX_CCC_LOCALORIENTATION0/1 is the reason the backend is Bullet.
 * GameCar::OnContactModify rebuilds the friction basis from the touched track
 * triangle, and PhysX 3+ and Jolt have no per-contact friction orientation at
 * all. btManifoldPoint::m_lateralFrictionDir1/2 is the direct equivalent, and
 * setting them with CF_HAS_FRICTION_ANCHOR off is what makes the solver use
 * them rather than deriving its own from the relative velocity.
 *
 * Worth knowing while reading GameCar's handler: it sets dynamicFriction0 and
 * staticFriction0 to zero unconditionally, with the computed velocity-dependent
 * value commented out. So the basis construction currently decides only which
 * tangent is made frictionless, and the magnitude is always zero.
 */
void Scene::setUserContactModify(NxUserContactModify* callback)
	{
	_contactModify = callback;
	}

void Scene::modifyContacts()
	{
	if (!_contactModify)
		return;

	const int manifolds = _dispatcher->getNumManifolds();

	for (int i = 0; i < manifolds; ++i)
		{
		btPersistentManifold* manifold = _dispatcher->getManifoldByIndexInternal(i);

		Actor* actor0 = static_cast<Actor*>(manifold->getBody0()->getUserPointer());
		Actor* actor1 = static_cast<Actor*>(manifold->getBody1()->getUserPointer());
		if (!actor0 || !actor1)
			continue;

		for (int p = 0; p < manifold->getNumContacts(); ++p)
			{
			btManifoldPoint& point = manifold->getContactPoint(p);

			NxShape* shape0 = ShapeAt(*actor0, point.m_index0);
			NxShape* shape1 = ShapeAt(*actor1, point.m_index1);
			if (!shape0 || !shape1)
				continue;

			/*
			 * The defaults 2.8 hands the callback: the material's own friction,
			 * so a callback that changes nothing leaves the contact alone.
			 */
			NxUserContactModify::NxContactCallbackData data;
			data.minImpulse = 0.0f;
			data.maxImpulse = NX_MAX_REAL;
			data.error.zero();
			data.target.zero();
			data.localpos0 = ToNx(point.m_localPointA);
			data.localpos1 = ToNx(point.m_localPointB);
			data.localorientation0.id();
			data.localorientation1.id();
			data.staticFriction0 = data.staticFriction1 = point.m_combinedFriction;
			data.dynamicFriction0 = data.dynamicFriction1 = point.m_combinedFriction;
			data.restitution = point.m_combinedRestitution;

			NxU32 changeFlags = NxUserContactModify::NX_CCC_NONE;

			if (!_contactModify->onContactConstraint(
					changeFlags, shape0, shape1,
					static_cast<NxU32>(point.m_index0),
					static_cast<NxU32>(point.m_index1), data))
				{
				/* 2.8 lets the callback reject the contact outright. */
				point.m_appliedImpulse = 0.0f;
				point.m_combinedFriction = 0.0f;
				point.m_combinedRestitution = 0.0f;
				continue;
				}

			if (changeFlags & (NxUserContactModify::NX_CCC_STATICFRICTION0 |
			                   NxUserContactModify::NX_CCC_DYNAMICFRICTION0))
				point.m_combinedFriction = data.dynamicFriction0;

			if (changeFlags & NxUserContactModify::NX_CCC_RESTITUTION)
				point.m_combinedRestitution = data.restitution;

			/*
			 * The friction frame. 2.8 hands it over as a quaternion whose X and
			 * Y axes are the two tangent directions; Bullet wants those two
			 * directions directly.
			 */
			if (changeFlags & (NxUserContactModify::NX_CCC_LOCALORIENTATION0 |
			                   NxUserContactModify::NX_CCC_LOCALORIENTATION1))
				{
				const btQuaternion frame = ToBullet(
					(changeFlags & NxUserContactModify::NX_CCC_LOCALORIENTATION0)
						? data.localorientation0 : data.localorientation1);

				const btMatrix3x3 basis(frame);

				point.m_lateralFrictionDir1 = basis.getColumn(0);
				point.m_lateralFrictionDir2 = basis.getColumn(1);
				/* Bullet 3.25 replaced m_lateralFrictionInitialized with a bit
				   in m_contactPointFlags; setting it is what stops the solver
				   deriving its own basis from the relative velocity and
				   discarding the one the game just computed. */
				point.m_contactPointFlags |=
					BT_CONTACT_FLAG_LATERAL_FRICTION_INITIALIZED;
				}
			}
		}
	}

/* --------------------------------------------------------------- contacts */

void Scene::setUserContactReport(NxUserContactReport* callback)
	{
	_contactReport = callback;
	}

/*
 * Walk Bullet's manifolds and deliver one onContactNotify per pair.
 *
 * Bullet's btPersistentManifold is already one per shape pair with its points
 * sharing a normal, so a manifold maps to a 2.8 pair holding a single patch.
 * The patch level still exists because the game walks all three -- GameObject,
 * Logic, Weapon and GameCar all nest goNextPair/goNextPatch/goNextPoint.
 *
 * Impulse to force: Bullet accumulates m_appliedImpulse over the step, and 2.8
 * reports a force. Dividing by the step converts one to the other, which is
 * what makes sumNormalForce comparable to m*dv/dt.
 */
void Scene::collectContacts(NxReal elapsedTime)
	{
	if (!_contactReport || elapsedTime <= 0.0f)
		return;

	/* Last step's records die here rather than at the end of the callback, so
	   nothing the game was handed is freed while it might still be looking. */
	for (size_t i = 0; i < _contactStreams.size(); ++i)
		delete _contactStreams[i];
	_contactStreams.clear();

	const int manifolds = _dispatcher->getNumManifolds();

	for (int i = 0; i < manifolds; ++i)
		{
		const btPersistentManifold* manifold =
			_dispatcher->getManifoldByIndexInternal(i);

		if (manifold->getNumContacts() == 0)
			continue;

		Actor* actor0 = static_cast<Actor*>(manifold->getBody0()->getUserPointer());
		Actor* actor1 = static_cast<Actor*>(manifold->getBody1()->getUserPointer());
		if (!actor0 || !actor1)
			continue;

		ContactStreamRecord* record = new ContactStreamRecord();
		_contactStreams.push_back(record);

		ContactPairRecord pairRecord;
		pairRecord.shapes[0] = NULL;
		pairRecord.shapes[1] = NULL;
		pairRecord.shapeFlags = 0;

		ContactPatchRecord patch;
		patch.normal.zero();

		NxVec3 sumNormalForce;
		sumNormalForce.zero();

		for (int p = 0; p < manifold->getNumContacts(); ++p)
			{
			const btManifoldPoint& point = manifold->getContactPoint(p);

			/*
			 * m_index0 and m_index1 are the child indices within a compound,
			 * which is how a contact resolves back to a 2.8 shape rather than
			 * merely to an actor. The game needs it:
			 * GameObject::ContainsContactGroup reads
			 * getShape(actorIndex)->getGroup().
			 */
			if (!pairRecord.shapes[0])
				pairRecord.shapes[0] = ShapeAt(*actor0, point.m_index0);
			if (!pairRecord.shapes[1])
				pairRecord.shapes[1] = ShapeAt(*actor1, point.m_index1);

			ContactPointRecord pointRecord;
			pointRecord.point = ToNx(point.getPositionWorldOnB());
			pointRecord.separation = point.getDistance();
			pointRecord.normalForce = point.getAppliedImpulse() / elapsedTime;
			pointRecord.featureIndex0 = static_cast<NxU32>(point.m_index0);
			pointRecord.featureIndex1 = static_cast<NxU32>(point.m_index1);

			patch.normal = ToNx(point.m_normalWorldOnB);
			sumNormalForce += patch.normal * pointRecord.normalForce;

			patch.points.push_back(pointRecord);
			}

		pairRecord.patches.push_back(patch);
		record->pairs.push_back(pairRecord);

		NxContactPair pair;
		pair.actors[0] = actor0;
		pair.actors[1] = actor1;
		pair.stream = ToStream(record);
		pair.sumNormalForce = sumNormalForce;
		pair.isDeletedActor[0] = false;
		pair.isDeletedActor[1] = false;

		_contactReport->onContactNotify(pair, NX_NOTIFY_ON_TOUCH);
		}
	}

/*
 * Recorded, and the maxTimestep is the thing that matters.
 *
 * fetchResults runs exactly one substep, and that is only equivalent to 2.8
 * because maxTimestep is 1/60 and World.cpp steps at 1/60. If the game ever
 * asked for a different maximum, the substep count would diverge and
 * NX_SMOOTH_IMPULSE would stop being identical to NX_IMPULSE -- so the
 * assumption is asserted rather than left as a comment.
 */
void Scene::setTiming(NxReal maxTimestep, NxU32 maxIter, NxTimeStepMethod)
	{
	_maxTimestep = maxTimestep;
	_maxIter = maxIter;

	if (maxTimestep < 1.0f / 60.0f - 1e-6f)
		Unimplemented("NxScene::setTiming with a maxTimestep below 1/60 "
		              "-- fetchResults runs one substep, which is only "
		              "equivalent to 2.8 while the step is 1/60");
	}

/* ----------------------------------------------------------------- gravity */

void Scene::setGravity(const NxVec3& gravity)
	{
	_world->setGravity(ToBullet(gravity));
	}

void Scene::getGravity(NxVec3& gravity) const
	{
	gravity = ToNx(_world->getGravity());
	}

/* ------------------------------------------------- not implemented yet ---- */

/*
 * How NxGroupsMask combines. Weapon.cpp:302 sets (OR, OR, AND) before a
 * homing-missile raycast and :319 sets (AND, AND, AND) after -- so the ops are
 * scene state the raycast reads, not a one-off.
 */
void Scene::setFilterOps(NxFilterOp op0, NxFilterOp op1, NxFilterOp op2)
	{
	_filterOp0 = op0;
	_filterOp1 = op1;
	_filterOp2 = op2;
	}

void Scene::setFilterBool(bool flag)
	{
	_filterBool = flag;
	}

void Scene::setShapePairFlags(NxShape& a, NxShape& b, NxU32 flags)
	{
	if (flags == 0)
		_shapePairFlags.erase(ShapePairKey(a, b));
	else
		_shapePairFlags[ShapePairKey(a, b)] = flags;
	}

/*
 * `groups` is a 32-bit MASK of collision groups, not a group index -- which is
 * the other half of why NxShapeDesc::checkValid rejects a group of 32 or more.
 * Player.cpp:1727 passes `1 << cdgTrackPlane`, and :1764 ORs three of them
 * together.
 *
 * The ray is a point and a direction, and 2.8 does not require the direction to
 * be normalised -- Player.cpp passes NxVec3(-ZVector), which happens to be, but
 * GameObject::RayCast forwards whatever the caller had. maxDist is measured
 * along the normalised direction either way, so it is normalised here.
 */
NxShape* Scene::raycastClosestShape(const NxRay& worldRay, NxShapesType shapeType,
                                    NxRaycastHit& hit, NxU32 groups, NxReal maxDist,
                                    NxU32, const NxGroupsMask* groupsMask,
                                    NxShape**) const
	{
	hit.shape = NULL;
	hit.distance = 0.0f;

	NxVec3 direction = worldRay.dir;
	const NxReal length = direction.magnitude();
	if (length <= 0.0f)
		return NULL;
	direction *= 1.0f / length;

	/* NX_MAX_F32 means "as far as it goes"; Bullet needs a real endpoint. */
	const NxReal distance = maxDist >= NX_MAX_F32 ? 1.0e6f : maxDist;

	const btVector3 from = ToBullet(worldRay.orig);
	const btVector3 to = ToBullet(worldRay.orig + direction * distance);

	RayFilter callback(from, to, *this, shapeType, groups, groupsMask);
	_world->rayTest(from, to, callback);

	if (!callback.hasHit())
		return NULL;

	hit.shape = callback.shape;
	hit.worldImpact = ToNx(callback.m_hitPointWorld);
	hit.worldNormal = ToNx(callback.m_hitNormalWorld);
	hit.distance = callback.m_closestHitFraction * distance;
	hit.material = NULL;

	return hit.shape;
	}

/* ----------------------------------------------------------------- wheels */

void Scene::registerWheel(WheelShape* wheel)
	{
	_wheels.push_back(wheel);
	}

void Scene::unregisterWheel(WheelShape* wheel)
	{
	for (size_t i = 0; i < _wheels.size(); ++i)
		if (_wheels[i] == wheel)
			{
			_wheels.erase(_wheels.begin() + i);
			return;
			}
	}

/*
 * The suspension raycast.
 *
 * Its own function rather than raycastClosestShape, because the exclusion rule
 * is different and load-bearing: a wheel must not find the car it is bolted to.
 * A car's own hull sits directly in the path of every one of its suspension
 * rays, so without this every wheel rests on its own body at zero travel and
 * the car never touches the ground.
 *
 * The whole actor is excluded, not just the wheel's own shape -- 2.8's wheels
 * ignore the actor they belong to, and a car's hull is one actor with several
 * shapes.
 */
NxShape* Scene::raycastForWheel(const btVector3& from, const btVector3& to,
                                const Actor* exclude, NxRaycastHit& hit) const
	{
	class WheelRay: public btCollisionWorld::ClosestRayResultCallback
		{
		public:
		WheelRay(const btVector3& from, const btVector3& to, const Actor* exclude)
			: btCollisionWorld::ClosestRayResultCallback(from, to),
			  shape(NULL), _exclude(exclude)
			{
			}

		virtual bool needsCollision(btBroadphaseProxy* proxy) const
			{
			const btCollisionObject* object =
				static_cast<const btCollisionObject*>(proxy->m_clientObject);
			return static_cast<const Actor*>(object->getUserPointer()) != _exclude;
			}

		virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult& result,
		                                 bool normalInWorldSpace)
			{
			/* m_localShapeInfo carries the compound child index, which is what
			   turns a hit on a body into a hit on a 2.8 shape -- the same
			   resolution RayFilter does. */
			const Actor* actor =
				static_cast<const Actor*>(result.m_collisionObject->getUserPointer());
			if (actor && actor->getNbShapes() > 0)
				{
				int child = result.m_localShapeInfo
					? result.m_localShapeInfo->m_triangleIndex : 0;
				if (child < 0 || static_cast<NxU32>(child) >= actor->getNbShapes())
					child = 0;

				shape = actor->getShapes()[child];
				}

			return ClosestRayResultCallback::addSingleResult(result, normalInWorldSpace);
			}

		NxShape* shape;

		private:
		const Actor* _exclude;
		};

	WheelRay callback(from, to, exclude);
	_world->rayTest(from, to, callback);

	if (!callback.hasHit())
		return NULL;

	hit.shape = callback.shape;
	hit.worldImpact = ToNx(callback.m_hitPointWorld);
	hit.worldNormal = ToNx(callback.m_hitNormalWorld);
	hit.distance = callback.m_closestHitFraction * (to - from).length();
	hit.material = NULL;
	hit.faceID = 0;

	return hit.shape;
	}

/* Joint breakage and sleep transitions. The game passes a handler through
   NxSceneDesc::userNotify but nothing in it is reachable: there are no joints,
   and Bullet's own deactivation is disabled. Stored so a later sleep
   implementation has somewhere to deliver. */
void Scene::setUserNotify(NxUserNotify* callback)
	{
	_userNotify = callback;
	}

} /* namespace px28 */
