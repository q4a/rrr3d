/*
 * NxScene over btDiscreteDynamicsWorld.
 */

#include "Px28Impl.h"

namespace px28
{

Scene::Scene(const NxSceneDesc& desc)
	: _config(NULL), _dispatcher(NULL), _broadphase(NULL), _solver(NULL),
	  _world(NULL), _skinWidth(0.025f), _pendingStep(0.0f)
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

	delete _world;
	delete _solver;
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
		_pendingStep = 0.0f;
		}

	return true;
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

void Scene::setGroupCollisionFlag(NxCollisionGroup, NxCollisionGroup, bool)      { Unimplemented("NxScene::setGroupCollisionFlag"); }
bool Scene::getGroupCollisionFlag(NxCollisionGroup, NxCollisionGroup) const      { Unimplemented("NxScene::getGroupCollisionFlag"); }
void Scene::setFilterOps(NxFilterOp, NxFilterOp, NxFilterOp)                     { Unimplemented("NxScene::setFilterOps"); }
void Scene::setFilterBool(bool)                                                  { Unimplemented("NxScene::setFilterBool"); }
void Scene::setActorPairFlags(NxActor&, NxActor&, NxU32)                         { Unimplemented("NxScene::setActorPairFlags"); }
NxU32 Scene::getActorPairFlags(NxActor&, NxActor&) const                         { Unimplemented("NxScene::getActorPairFlags"); }
void Scene::setShapePairFlags(NxShape&, NxShape&, NxU32)                         { Unimplemented("NxScene::setShapePairFlags"); }

NxShape* Scene::raycastClosestShape(const NxRay&, NxShapesType, NxRaycastHit&, NxU32,
                                    NxReal, NxU32, const NxGroupsMask*, NxShape**) const
	{
	Unimplemented("NxScene::raycastClosestShape");
	}

void Scene::setUserContactReport(NxUserContactReport*)  { Unimplemented("NxScene::setUserContactReport"); }
void Scene::setUserContactModify(NxUserContactModify*)  { Unimplemented("NxScene::setUserContactModify"); }
void Scene::setUserNotify(NxUserNotify*)                { Unimplemented("NxScene::setUserNotify"); }

} /* namespace px28 */
