/*
 * NxScene over btDiscreteDynamicsWorld.
 */

#include "Px28Impl.h"

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
	  _contactReport(NULL)
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
		_world->stepSimulation(_pendingStep, 0, _pendingStep);

		collectContacts(_pendingStep);

		_pendingStep = 0.0f;
		}

	return true;
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

void Scene::setTiming(NxReal, NxU32, NxTimeStepMethod)
	{
	Unimplemented("NxScene::setTiming");
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

void Scene::setFilterOps(NxFilterOp, NxFilterOp, NxFilterOp)  { Unimplemented("NxScene::setFilterOps"); }
void Scene::setFilterBool(bool)                               { Unimplemented("NxScene::setFilterBool"); }
void Scene::setShapePairFlags(NxShape&, NxShape&, NxU32)      { Unimplemented("NxScene::setShapePairFlags"); }

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

void Scene::setUserContactModify(NxUserContactModify*)  { Unimplemented("NxScene::setUserContactModify"); }
void Scene::setUserNotify(NxUserNotify*)                { Unimplemented("NxScene::setUserNotify"); }

} /* namespace px28 */
