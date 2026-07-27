#include "stdafx.h"

#include "px/Physx.h"

#include "lslSerialValue.h"

namespace r3d
{

namespace px
{

//The PhysX 2.8 Visual Remote Debugger connection lived here. PhysX 4.1
//replaces it with PVD (PxPvd), created alongside the foundation and connected
//over PxPvdTransport rather than by host and port constants. Not reinstated:
//nothing in the port needs it yet, and it would be dead configuration.

//ToPx/FromPx live in px/Physx.h -- the game layer converts at the same
//boundaries this file does.

namespace
{

//The rotation taking mass-space vectors to world space.
PxQuat MassFrameRotation(const PxRigidDynamic& body)
{
	return (body.getGlobalPose() * body.getCMassLocalPose()).q;
}

}

D3DXVECTOR3 GetLinearMomentum(const PxRigidDynamic& body)
{
	return FromPx(body.getLinearVelocity() * body.getMass());
}

void SetLinearMomentum(PxRigidDynamic& body, const D3DXVECTOR3& value)
{
	const float mass = body.getMass();

	//A zero-mass dynamic actor is kinematic in all but name; leave it alone
	//rather than dividing by zero.
	if (mass > 0.0f)
		body.setLinearVelocity(ToPx(value) / mass);
}

D3DXVECTOR3 GetAngularMomentum(const PxRigidDynamic& body)
{
	const PxQuat rot = MassFrameRotation(body);
	const PxVec3 inertia = body.getMassSpaceInertiaTensor();

	//L = R * I * R^T * w, evaluated by taking w into mass space, scaling by the
	//diagonal tensor, and rotating back.
	const PxVec3 local = rot.rotateInv(body.getAngularVelocity());
	return FromPx(rot.rotate(PxVec3(local.x * inertia.x, local.y * inertia.y, local.z * inertia.z)));
}

float ComputeKineticEnergy(const PxRigidDynamic& body)
{
	const PxVec3 linVel = body.getLinearVelocity();
	const float translational = 0.5f * body.getMass() * linVel.magnitudeSquared();

	const PxVec3 inertia = body.getMassSpaceInertiaTensor();
	const PxVec3 angVel = MassFrameRotation(body).rotateInv(body.getAngularVelocity());
	const float rotational = 0.5f * (angVel.x * angVel.x * inertia.x +
		angVel.y * angVel.y * inertia.y +
		angVel.z * angVel.z * inertia.z);

	return translational + rotational;
}

void AddLocalForce(PxRigidDynamic& body, const D3DXVECTOR3& force, PxForceMode::Enum mode)
{
	body.addForce(body.getGlobalPose().q.rotate(ToPx(force)), mode);
}

void AddLocalTorque(PxRigidDynamic& body, const D3DXVECTOR3& torque, PxForceMode::Enum mode)
{
	body.addTorque(body.getGlobalPose().q.rotate(ToPx(torque)), mode);
}

void SetAngularMomentum(PxRigidDynamic& body, const D3DXVECTOR3& value)
{
	const PxQuat rot = MassFrameRotation(body);
	const PxVec3 inertia = body.getMassSpaceInertiaTensor();

	//w = R * I^-1 * R^T * L. A zero principal moment means no rotation is
	//possible about that axis, which is what PhysX means by a locked axis.
	const PxVec3 local = rot.rotateInv(ToPx(value));
	const PxVec3 scaled(
		inertia.x > 0.0f ? local.x / inertia.x : 0.0f,
		inertia.y > 0.0f ? local.y / inertia.y : 0.0f,
		inertia.z > 0.0f ? local.z / inertia.z : 0.0f);

	body.setAngularVelocity(rot.rotate(scaled));
}

//const float Scene::maxTimeStep = 1.0f/75.0f;
//const unsigned Scene::maxSimIter = 8;
const PxVec3 Scene::cDefGravity(0.0f, 0.0f, -20.0f);
const int Scene::cDefMatInd = 0;

PxPhysics* Manager::_nxSDK = 0;
PxCooking* Manager::_nxCooking = 0;
PxFoundation* Manager::_nxFoundation = 0;
PxMaterial* Manager::_defMaterial = 0;
std::vector<PxMaterial*> Manager::_materials;
unsigned Manager::_sdkRefCnt = 0;

Shapes::ClassList Shapes::classList;


//PhysX 3+ decides collision filtering in a shader supplied by the application.
//The extensions library ships one that, in its own words, emulates 2.8.x
//filtering: it reads the collision group from filter data word0 -- which is
//what Shape::ApplyToShape writes -- and consults the same 0..31 pairwise table
//that PxSetGroupCollisionFlag maintains.
//
//The only thing it does not do is ask for the reports this engine needs, so
//this wrapper adds them. 2.8 enabled contact notification and contact
//modification per actor; PhysX 3+ requires the pair flags be requested here,
//in the shader, because filtering happens before any actor is consulted.
static PxFilterFlags SceneFilterShader(
	PxFilterObjectAttributes attributes0, PxFilterData filterData0,
	PxFilterObjectAttributes attributes1, PxFilterData filterData1,
	PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize)
{
	PxFilterFlags flags = PxDefaultSimulationFilterShader(
		attributes0, filterData0, attributes1, filterData1,
		pairFlags, constantBlock, constantBlockSize);

	//BEHAVIOUR GAP: this asks for reports on every surviving pair, where 2.8
	//raised them only for actors carrying the matching contact-report flags.
	//Actor::_desc.contactReportFlags is still stored and serialised but no
	//longer reaches the simulation, so the callbacks currently fire more often
	//than they used to and the engine filters afterwards.
	if (!(flags & (PxFilterFlag::eKILL | PxFilterFlag::eSUPPRESS)))
	{
		pairFlags |= PxPairFlag::eNOTIFY_TOUCH_FOUND
			| PxPairFlag::eNOTIFY_TOUCH_PERSISTS
			| PxPairFlag::eNOTIFY_CONTACT_POINTS
			| PxPairFlag::eMODIFY_CONTACTS;

		//Only pairs where both actors carry an exception need the callback,
		//which is the only place actor identity is visible.
		if ((filterData0.word1 & Scene::cPairExceptionBit) &&
			(filterData1.word1 & Scene::cPairExceptionBit))
		{
			flags |= PxFilterFlag::eCALLBACK;
		}
	}

	return flags;
}

Scene::Scene(Manager* manager): _manager(manager), _lastDeltaTime(0)
{
	_contactModify = new ContactModify(this);
	_simulationEvents = new SimulationEvents(this);
	_pairFilter = new PairFilter(this);

	//2.8's upAxis and timeStepMethod have no PhysX 3+ equivalents and need
	//none: the up axis was only ever advisory, gravity already encodes it, and
	//simulate() takes the step directly, which is what NX_TIMESTEP_VARIABLE
	//selected.
	PxSceneDesc sceneDesc(_manager->GetSDK().getTolerancesScale());
	sceneDesc.gravity = cDefGravity;
	sceneDesc.filterShader = SceneFilterShader;
	sceneDesc.simulationEventCallback = _simulationEvents;
	sceneDesc.contactModifyCallback = _contactModify;
	sceneDesc.filterCallback = _pairFilter;

	//PhysX 2.8 owned its worker threads; PhysX 3+ makes that the caller's job.
	_cpuDispatcher = PxDefaultCpuDispatcherCreate(0);
	sceneDesc.cpuDispatcher = _cpuDispatcher;

	_nxScene = _manager->GetSDK().createScene(sceneDesc);

	//BEHAVIOUR GAP: PxSetGroupCollisionFlag is global where
	//NxScene::setGroupCollisionFlag was per-scene. The game runs one scene, so
	//this is currently equivalent; a second scene would silently share the
	//table.
	PxSetGroupCollisionFlag(cdgShot, cdgShot, false);
	//
	PxSetGroupCollisionFlag(cdgShotBorder, cdgShotBorder, false);
	PxSetGroupCollisionFlag(cdgShotBorder, cdgShot, false);
	//
	PxSetGroupCollisionFlag(cdgShotTrack, cdgShot, false);
	PxSetGroupCollisionFlag(cdgShotTrack, cdgShotBorder, false);
	PxSetGroupCollisionFlag(cdgShotTrack, cdgShotTrack, false);
	//
	PxSetGroupCollisionFlag(cdgShotTransparency, cdgShot, false);
	//
	PxSetGroupCollisionFlag(cdgWheel, cdgShot, false);
	PxSetGroupCollisionFlag(cdgWheel, cdgShotBorder, false);
	PxSetGroupCollisionFlag(cdgWheel, cdgShotTrack, false);
	PxSetGroupCollisionFlag(cdgWheel, cdgShotTransparency, false);
	//
	PxSetGroupCollisionFlag(cdgTrackPlane, cdgShot, false);
	PxSetGroupCollisionFlag(cdgTrackPlane, cdgShotBorder, false);
}

Scene::~Scene()
{
	LSL_ASSERT(_userList.empty());

	_nxScene->release();
	_cpuDispatcher->release();

	delete _pairFilter;
	delete _simulationEvents;
	delete _contactModify;
}

Scene::ContactModify::ContactModify(Scene* scene): _scene(scene)
{
}

void Scene::ContactModify::onContactModify(PxContactModifyPair* const pairs, PxU32 count)
{
	for (PxU32 i = 0; i < count; ++i)
	{
		PxContactModifyPair& pair = pairs[i];

		OnContactModifyEvent contactEvent;

		contactEvent.shape0 = pair.shape[0];
		contactEvent.shape1 = pair.shape[1];
		//No PhysX 3+ equivalent; see OnContactModifyEvent.
		contactEvent.featureIndex0 = 0;
		contactEvent.featureIndex1 = 0;

		contactEvent.contacts = &pair.contacts;

		Actor* actor0 = GetActorFromNx(pair.actor[0]);
		Actor* actor1 = GetActorFromNx(pair.actor[1]);

		if (!actor0 || !actor1)
			continue;

		bool accept = true;

		//Отправляем событие первому актеру
		contactEvent.actor = actor1;
		contactEvent.actorIndex = 1;
		if (actor0->GetOwner() && !actor0->GetOwner()->OnContactModify(contactEvent))
			accept = false;

		//Отправляем событие второму актеру
		if (accept)
		{
			contactEvent.actor = actor0;
			contactEvent.actorIndex = 0;
			if (actor1->GetOwner() && !actor1->GetOwner()->OnContactModify(contactEvent))
				accept = false;
		}

		//2.8 rejected the whole constraint by returning false. PhysX 3+ has no
		//such return, so the equivalent is ignoring every contact in the set.
		if (!accept)
		{
			for (PxU32 c = 0; c < pair.contacts.size(); ++c)
				pair.contacts.ignore(c);
		}
	}
}

Scene::SimulationEvents::SimulationEvents(Scene* scene): _scene(scene)
{
}

void Scene::SimulationEvents::onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 count)
{
	//An actor removed from the scene during simulation must not be dereferenced.
	bool removedActor0 = pairHeader.flags.isSet(PxContactPairHeaderFlag::eREMOVED_ACTOR_0);
	bool removedActor1 = pairHeader.flags.isSet(PxContactPairHeaderFlag::eREMOVED_ACTOR_1);

	for (PxU32 i = 0; i < count; ++i)
	{
		const PxContactPair& pair = pairs[i];

		OnContactEvent contact1;
		OnContactEvent contact2;

		contact1.pair = &pair;
		contact1.events = pair.events;
		contact1.deltaTime = _scene->_lastDeltaTime;

		//2.8 handed over summed normal and friction forces. PhysX 3+ reports a
		//per-point impulse that already combines both, so the sum is converted
		//back to a force by dividing by the step, and friction is left at zero
		//rather than reported wrongly. See OnContactEvent.
		PxContactPairPoint points[cMaxContactPoints];
		PxU32 numPoints = pair.extractContacts(points, cMaxContactPoints);

		PxVec3 sumImpulse(0.0f);
		for (PxU32 p = 0; p < numPoints; ++p)
			sumImpulse += points[p].impulse;

		contact1.sumNormalForce = _scene->_lastDeltaTime > 0.0f
			? FromPx(sumImpulse / _scene->_lastDeltaTime)
			: NullVector;
		contact1.sumFrictionForce = NullVector;

		contact2 = contact1;

		contact1.actor = !removedActor0 ? GetActorFromNx(pairHeader.actors[0]) : 0;
		contact1.actorIndex = 0;
		contact2.actor = !removedActor1 ? GetActorFromNx(pairHeader.actors[1]) : 0;
		contact2.actorIndex = 1;

		if (contact1.actor && contact2.actor)
		{
			if (contact1.actor->GetOwner())
				contact1.actor->GetOwner()->OnContact(contact2);

			if (contact2.actor->GetOwner())
				contact2.actor->GetOwner()->OnContact(contact1);

			for (UserList::iterator iter = _scene->_userList.begin(); iter != _scene->_userList.end(); ++iter)
				(*iter)->OnContact(contact1, contact2);
		}
	}
}

void Scene::SimulationEvents::onWake(PxActor** actors, PxU32 count)
{
	for (unsigned i = 0; i < count; ++i)
	{
		Actor* actor = GetActorFromNx(actors[i]->is<PxRigidActor>());

		if (actor && actor->GetOwner())
			actor->GetOwner()->OnWake();
	}
}

void Scene::SimulationEvents::onSleep(PxActor** actors, PxU32 count)
{
	for (unsigned i = 0; i < count; ++i)
	{
		Actor* actor = GetActorFromNx(actors[i]->is<PxRigidActor>());

		if (actor && actor->GetOwner())
			actor->GetOwner()->OnSleep();
	}
}

PxShape* Scene::RaycastClosestShape(const D3DXVECTOR3& origin, const D3DXVECTOR3& dir,
	float maxDist, unsigned groups, PxQueryFlags queryFlags, PxRaycastHit& outHit,
	const PxGroupsMask* groupsMask)
{
	PxQueryFilterData filterData;
	filterData.data.word0 = groups;
	filterData.flags = queryFlags;

	if (groupsMask)
	{
		filterData.data.word2 = PxU32(groupsMask->bits0 | (PxU32(groupsMask->bits1) << 16));
		filterData.data.word3 = PxU32(groupsMask->bits2 | (PxU32(groupsMask->bits3) << 16));
	}

	PxRaycastBuffer buffer;
	//PhysX requires a normalized direction; 2.8's NxRay did not.
	const PxVec3 unitDir = ToPx(dir).getNormalized();

	if (!_nxScene->raycast(ToPx(origin), unitDir, maxDist, buffer, PxHitFlag::eDEFAULT, filterData) ||
		!buffer.hasBlock)
	{
		return 0;
	}

	outHit = buffer.block;
	return outHit.shape;
}

unsigned Scene::GetShapeGroup(const PxShape& shape)
{
	return shape.getSimulationFilterData().word0;
}

Scene::PairFilter::PairFilter(Scene* scene): _scene(scene)
{
}

PxFilterFlags Scene::PairFilter::pairFound(PxU32,
	PxFilterObjectAttributes, PxFilterData, const PxActor* a0, const PxShape*,
	PxFilterObjectAttributes, PxFilterData, const PxActor* a1, const PxShape*,
	PxPairFlags& pairFlags)
{
	if (_scene->IsActorPairIgnored(a0, a1))
		return PxFilterFlag::eSUPPRESS;

	pairFlags |= PxPairFlag::eCONTACT_DEFAULT;
	return PxFilterFlags();
}

void Scene::SetActorPairIgnored(PxRigidActor& actor0, PxRigidActor& actor1, bool ignored)
{
	const PxActor* a = &actor0;
	const PxActor* b = &actor1;
	if (b < a)
		std::swap(a, b);

	if (ignored)
	{
		_ignoredPairs.insert(std::make_pair(a, b));
		//Both sides must be marked or the shader will not defer to the callback.
		MarkPairException(actor0);
		MarkPairException(actor1);
	}
	else
	{
		//The marker bit is deliberately left set. Clearing it would need a scan
		//of every remaining exception, and a spurious callback only costs a
		//lookup that returns false.
		_ignoredPairs.erase(std::make_pair(a, b));
	}
}

bool Scene::IsActorPairIgnored(const PxActor* actor0, const PxActor* actor1) const
{
	if (!actor0 || !actor1)
		return false;

	const PxActor* a = actor0;
	const PxActor* b = actor1;
	if (b < a)
		std::swap(a, b);

	return _ignoredPairs.find(std::make_pair(a, b)) != _ignoredPairs.end();
}

void Scene::MarkPairException(PxRigidActor& actor)
{
	const PxU32 numShapes = actor.getNbShapes();
	if (numShapes == 0)
		return;

	std::vector<PxShape*> shapes(numShapes);
	actor.getShapes(&shapes[0], numShapes);

	for (PxU32 i = 0; i < numShapes; ++i)
	{
		PxFilterData filter = shapes[i]->getSimulationFilterData();
		filter.word1 |= cPairExceptionBit;
		shapes[i]->setSimulationFilterData(filter);
	}
}

//The word layout is PxDefaultSimulationFilterShader's own -- see
//ExtDefaultSimulationFilterShader.cpp, which packs the mask the same way.
void Scene::SetShapeGroupsMask(PxShape& shape, const PxGroupsMask& mask)
{
	PxFilterData filter = shape.getSimulationFilterData();
	filter.word2 = PxU32(mask.bits0 | (PxU32(mask.bits1) << 16));
	filter.word3 = PxU32(mask.bits2 | (PxU32(mask.bits3) << 16));

	shape.setSimulationFilterData(filter);
	shape.setQueryFilterData(filter);
}

PxGroupsMask Scene::GetShapeGroupsMask(const PxShape& shape)
{
	const PxFilterData filter = shape.getSimulationFilterData();

	PxGroupsMask mask;
	mask.bits0 = PxU16(filter.word2 & 0xffff);
	mask.bits1 = PxU16(filter.word2 >> 16);
	mask.bits2 = PxU16(filter.word3 & 0xffff);
	mask.bits3 = PxU16(filter.word3 >> 16);
	return mask;
}

Actor* Scene::GetActorFromNx(const PxRigidActor* actor)
{
	return actor && actor->userData ? reinterpret_cast<Actor*>(actor->userData) : 0;
}

Actor* Scene::GetActorFromNxShape(const PxShape* shape)
{
	//PhysX 3+ shapes know their actor only while attached, and an exclusive
	//shape has exactly one.
	return shape ? GetActorFromNx(shape->getActor()) : 0;
}

void Scene::CreateGroundPlane()
{
	/*NxPlaneShapeDesc planeShape;
	planeShape.normal.set(ZVector);
	NxActorDesc planeActor;
	planeActor.shapes.push_back(&planeShape);
	_nxScene->createActor(planeActor);*/
}

PxRigidActor* Scene::CreateNxActor(const PxTransform& pose, bool dynamic, Actor* actor)
{
	if (!pose.isSane())
		throw lsl::Error("PxRigidActor* Scene::CreateNxActor(const PxTransform& pose, bool dynamic, Actor* actor)");

	PxRigidActor* res = dynamic
		? static_cast<PxRigidActor*>(_manager->GetSDK().createRigidDynamic(pose))
		: static_cast<PxRigidActor*>(_manager->GetSDK().createRigidStatic(pose));

	if (!res)
		return 0;

	res->userData = actor;
	//Shapes are attached by the caller before the actor sees a simulation step.
	_nxScene->addActor(*res);
	return res;
}

void Scene::ReleaseNxActor(PxRigidActor* nxActor, Actor* actor)
{
	_nxScene->removeActor(*nxActor);
	nxActor->release();
}

void Scene::Compute(float deltaTime)
{
	_lastDeltaTime = deltaTime;

	//flushStream is gone in PhysX 3+, and fetchResults no longer needs to be
	//told which simulation stage to wait for.
	_nxScene->simulate(deltaTime);
	_nxScene->fetchResults(true);
}

void Scene::InsertUser(SceneUser* value)
{
	LSL_ASSERT(value && value->_scene == 0);

	value->_scene = this;
	_userList.push_back(value);
	value->AddRef();
}

void Scene::RemoveUser(SceneUser* value)
{
	LSL_ASSERT(value && value->_scene == this);

	value->_scene = 0;
	_userList.remove(value);
	value->Release();
}

PxScene* Scene::GetNxScene()
{
	return _nxScene;
}


Manager::Manager()
{
	InitSDK();
	px::Shapes::RegisterClasses();	
}

Manager::~Manager()
{
	ClearSceneList();

	ReleaseSDK();
}

void Manager::InitSDK()
{
	LSL_LOG("px InitSDK");

	if (_sdkRefCnt++ == 0)
	{
		LSL_LOG("px create sdk");

		static PxDefaultAllocator allocator;
		static PxDefaultErrorCallback errorCallback;

		Manager::_nxFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, allocator, errorCallback);
		if (!Manager::_nxFoundation)
			throw lsl::Error("Unable to create the PhysX foundation");

		Manager::_nxSDK = PxCreatePhysics(PX_PHYSICS_VERSION, *Manager::_nxFoundation, PxTolerancesScale());
		if (!Manager::_nxSDK)
			throw lsl::Error("Unable to initialize the PhysX SDK");

		//PhysX 2.8 had two global tuning parameters set here:
		//  NX_ADAPTIVE_FORCE 0.0 -- adaptive force was removed outright in
		//    PhysX 4, so there is nothing to set. It was disabled anyway.
		//  NX_SKIN_WIDTH 0.025 -- allowed interpenetration, now per-shape via
		//    PxShape::setContactOffset rather than a global. Applied where
		//    shapes are created so the behaviour is preserved.

		LSL_LOG("px create cooking");

		Manager::_nxCooking = PxCreateCooking(PX_PHYSICS_VERSION, *Manager::_nxFoundation,
			PxCookingParams(PxTolerancesScale()));
		if (!Manager::_nxCooking)
			throw lsl::Error("The cooking library has not been initialized");

		//PxDefaultSimulationFilterShader and the collision group table it reads
		//live in the extensions library, which has to be initialised explicitly.
		if (!PxInitExtensions(*Manager::_nxSDK, 0))
			throw lsl::Error("Unable to initialize the PhysX extensions");

		//The material NxScene gave every scene at index 0, with the friction and
		//restitution the 2.8 code assigned to it.
		Manager::_defMaterial = Manager::_nxSDK->createMaterial(0.5f, 0.5f, 0.5f);
		if (!Manager::_defMaterial)
			throw lsl::Error("Unable to create the default PhysX material");

		//Index 0, matching NxScene's built-in material.
		Manager::_materials.clear();
		Manager::_materials.push_back(Manager::_defMaterial);
	}
}

void Manager::ReleaseSDK()
{
	LSL_ASSERT(_sdkRefCnt > 0);

	if (--_sdkRefCnt == 0)
	{
		//Index 0 is the default material, released just below.
		for (size_t i = 1; i < Manager::_materials.size(); ++i)
			Manager::_materials[i]->release();
		Manager::_materials.clear();

		Manager::_defMaterial->release();
		Manager::_defMaterial = 0;

		PxCloseExtensions();

		Manager::_nxCooking->release();
		Manager::_nxCooking = 0;

		Manager::_nxSDK->release();
		Manager::_nxSDK = 0;

		Manager::_nxFoundation->release();
		Manager::_nxFoundation = 0;
	}
}

void Manager::Compute(float deltaTime)
{
	for (SceneList::iterator iter = _sceneList.begin(); iter != _sceneList.end(); ++iter)
		(*iter)->Compute(deltaTime);	
}

Scene* Manager::AddScene()
{
	Scene* res = new Scene(this);
	_sceneList.push_back(res);
	res->SetName(MakeUniqueName("scene"));
	res->SetOwner(this);
	return res;
}

void Manager::DelScene(Scene* value)
{
	_sceneList.remove(value);
	delete value;
}

void Manager::ClearSceneList()
{
	for (SceneList::iterator iter = _sceneList.begin(); iter != _sceneList.end(); ++iter)
		delete *iter;
	_sceneList.clear();
}

const Manager::SceneList& Manager::GetSceneList()
{
	return _sceneList;
}

PxPhysics& Manager::GetSDK()
{
	return *_nxSDK;
}

PxCooking& Manager::GetCooking()
{
	return *_nxCooking;
}

PxMaterial& Manager::GetDefaultMaterial()
{
	LSL_ASSERT(_defMaterial);

	return *_defMaterial;
}

PxU16 Manager::RegisterMaterial(PxMaterial* material)
{
	LSL_ASSERT(material);

	for (size_t i = 0; i < _materials.size(); ++i)
		if (_materials[i] == material)
			return static_cast<PxU16>(i);

	_materials.push_back(material);
	return static_cast<PxU16>(_materials.size() - 1);
}

PxMaterial* Manager::GetMaterialByIndex(PxU16 index)
{
	return index < _materials.size() ? _materials[index] : _defMaterial;
}


TriangleMesh::TriangleMesh(): _meshData(0)
{
}

TriangleMesh::~TriangleMesh()
{
	SetMeshData(0);
}

void TriangleMesh::LoadMesh(const D3DXVECTOR3& scale, int id, PxTriangleMeshDesc& desc)
{
	LSL_ASSERT(_meshData);

	if (!_meshData->IsInit())
		_meshData->Load();

	if (!_meshData->vb.GetFormat(res::VertexData::vtPos3))
		throw lsl::Error("PxTriangleMesh* TriangleMesh::GetOrCreateMesh(const D3DXVECTOR3& scale)");

	bool scaling = (scale != IdentityVector) == TRUE;
	//scaling = false;

	int sVertex = id < 0 ? 0 : _meshData->faceGroups[id].sVertex;
	int vertCnt = id < 0 ? _meshData->vb.GetVertexCount() : _meshData->faceGroups[id].vertexCnt;
	int sFace = id < 0 ? 0 : _meshData->faceGroups[id].sFace;
	int faceCnt = id < 0 ? _meshData->fb.GetFaceCount() : _meshData->faceGroups[id].faceCnt;

	D3DXVECTOR3* vertices = new D3DXVECTOR3[vertCnt];	
	//Если в формате вершины только позиция, то копируется буффер целиком
	if (_meshData->vb.GetVertexSize() == sizeof(D3DXVECTOR3))
	{
		_meshData->vb.CopyDataTo(vertices, sVertex, vertCnt);
		if (scaling)
			for (unsigned i = 0; i < _meshData->vb.GetVertexCount(); ++i)
				vertices[i] *= scale;
	}
	//Иначе копируется только часть вершины соответствующая позиции
	else		
		for (int i = 0; i < vertCnt; ++i)
		{
			vertices[i] = *_meshData->vb[sVertex + i].Pos3();
			if (scaling)
				vertices[i] = vertices[i] * scale;
		}

	desc.points.count     = vertCnt;
	desc.points.stride    = sizeof(D3DXVECTOR3);
	desc.points.data      = vertices;
	desc.triangles.count  = faceCnt;
	desc.triangles.stride = _meshData->fb.GetFaceSize();
	desc.triangles.data   = _meshData->fb.GetData() + _meshData->fb.GetFaceSize() * sFace;

	//Формат индексов выводится из размера грани, а не предполагается 32-битным
	//как в версии для PhysX 2.8
	desc.flags = PxMeshFlags();
	if (_meshData->fb.GetFaceSize() == 3 * sizeof(PxU16))
		desc.flags |= PxMeshFlag::e16_BIT_INDICES;
}

void TriangleMesh::FreeMesh(PxTriangleMeshDesc& desc)
{
	delete[] static_cast<const D3DXVECTOR3*>(desc.points.data);
}

TriangleMesh::MeshList::iterator TriangleMesh::GetOrCreateMesh(const D3DXVECTOR3& scale, int id)
{
	MeshVal val;
	val.scale = scale;
	val.id = id;
	MeshList::iterator findIter = _meshList.Find(val);
	if (findIter == _meshList.end())
	{
		_meshList.push_back(val);
		findIter = --_meshList.end();		
	}
	++findIter->sumRef;

	return findIter;
}

void TriangleMesh::ReleaseMesh(MeshList::iterator iter)
{
	if (--(iter->sumRef) == 0)
	{
		LSL_ASSERT(!iter->tri && !iter->convex);

		_meshList.erase(iter);
	}
}

PxTriangleMesh* TriangleMesh::GetOrCreateTri(const D3DXVECTOR3& scale, int id)
{
	MeshList::iterator mesh = GetOrCreateMesh(scale, id);
	++(mesh->triRef);

	if (mesh->tri)	
		return mesh->tri;

	PxTriangleMeshDesc desc;
	LoadMesh(scale, id, desc);

	PxDefaultMemoryOutputStream buf;
	if (!px::GetCooking().cookTriangleMesh(desc, buf))
		throw lsl::Error("Error cooking TriangleMesh");
	PxDefaultMemoryInputData readBuffer(buf.getData(), buf.getSize());

	mesh->tri = px::GetSDK().createTriangleMesh(readBuffer);

	FreeMesh(desc);

	return mesh->tri;
}

void TriangleMesh::ReleaseTri(PxTriangleMesh* mesh)
{
	for (MeshList::iterator iter = _meshList.begin(); iter != _meshList.end(); ++iter)
	{
		if (iter->tri == mesh)
		{
			if (--(iter->triRef) == 0)
			{
				iter->tri->release();
				iter->tri = 0;
			}

			ReleaseMesh(iter);
			return;
		}
	}

	LSL_ASSERT(false);
}

PxConvexMesh* TriangleMesh::GetOrCreateConvex(const D3DXVECTOR3& scale, int id)
{
	MeshList::iterator mesh = GetOrCreateMesh(scale, id);
	++(mesh->convexRef);

	if (mesh->convex)	
		return mesh->convex;

	PxTriangleMeshDesc desc;
	LoadMesh(scale, id, desc);

	PxConvexMeshDesc convexDesc;
	convexDesc.points   = desc.points;
	convexDesc.indices  = desc.triangles;
	convexDesc.flags    = PxConvexFlag::eCOMPUTE_CONVEX;


	PxDefaultMemoryOutputStream buf;
	if (!px::GetCooking().cookConvexMesh(convexDesc, buf))
		throw lsl::Error("Error cooking ConvexMesh");
	PxDefaultMemoryInputData readBuffer(buf.getData(), buf.getSize());

	mesh->convex = px::GetSDK().createConvexMesh(readBuffer);

	FreeMesh(desc);

	return mesh->convex;
}

void TriangleMesh::ReleaseConvex(PxConvexMesh* mesh)
{
	for (MeshList::iterator iter = _meshList.begin(); iter != _meshList.end(); ++iter)
	{
		if (iter->convex == mesh)
		{
			if (--(iter->convexRef) == 0)
			{
				iter->convex->release();
				iter->convex = 0;
			}

			ReleaseMesh(iter);
			return;
		}
	}

	LSL_ASSERT(false);
}

res::MeshData* TriangleMesh::GetMeshData()
{
	return _meshData;
}

void TriangleMesh::SetMeshData(res::MeshData* value)
{
	if (_meshData != value)
	{
		if (_meshData)
		{
			LSL_ASSERT(IsEmpty());

			_meshData->Release();
		}

		_meshData = value;

		if (_meshData)
			_meshData->AddRef();
	}
}

bool TriangleMesh::IsEmpty() const
{
	return _meshList.empty();
}


Shape::Shape(Shapes* owner): _owner(owner), _type(stUnknown), _nxShape(0), _pos(NullVector), _rot(NullQuaternion), _scale(IdentityVector), _materialIndex(0), _density(1.0f), _skinWidth(-1), _group(0)
{
	SetType(Type);
}

void Shape::SetNxShape(PxShape* value)
{
	_nxShape = value;
	_delayInitialization = false;
}

void Shape::SetType(ShapeType value)
{
	_type = value;
}

void Shape::ReloadNxShape(bool allowInitialization)
{
	GetActor()->ReloadNxShape(this, allowInitialization);
}

D3DXVECTOR3 Shape::TransformLocalPos(const D3DXVECTOR3& inValue)
{
	D3DXVECTOR3 tmp;
	GetActor()->LocalToWorldPos(_pos, tmp, true);
	return tmp;
}

//PhysX 3+ has a single local pose where 2.8 had separate position and
//orientation setters, so each of these reads the pose back and replaces its
//own half rather than overwriting both.
void Shape::SyncPos()
{
	LSL_ASSERT(_nxShape);

	PxTransform pose = _nxShape->getLocalPose();
	pose.p = ToPx(TransformLocalPos(_pos));
	_nxShape->setLocalPose(pose);
}

void Shape::SyncRot()
{
	LSL_ASSERT(_nxShape);

	PxTransform pose = _nxShape->getLocalPose();
	pose.q = ToPx(_rot);
	_nxShape->setLocalPose(pose);
}

void Shape::SyncGeometry()
{
	if (_nxShape)
		_nxShape->setGeometry(CreateGeometry().any());
}

void Shape::SyncScale()
{
}

void Shape::Save(lsl::SWriter* writer)
{
	writer->WriteValue("pos", _pos, 3);
	writer->WriteValue("rot", _rot, 4);
	writer->WriteValue("scale", _scale, 3);

	writer->WriteValue("materialIndex", _materialIndex);
	writer->WriteValue("density", _density);
	writer->WriteValue("skinWidth", _skinWidth);
	writer->WriteValue("group", _group);
}

void Shape::Load(lsl::SReader* reader)
{
	reader->ReadValue("pos", _pos, 3);
	reader->ReadValue("rot", _rot, 4);
	reader->ReadValue("scale", _scale, 3);

	reader->ReadValue("materialIndex", _materialIndex);
	reader->ReadValue("density", _density);
	reader->ReadValue("skinWidth", _skinWidth);
	reader->ReadValue("group", _group);
}


void Shape::ApplyToShape(PxShape& shape)
{
	//Child actors share their parent's PxRigidActor, so a shape's local pose is
	//expressed in the root actor's space rather than its own owner's.
	shape.setLocalPose(PxTransform(ToPx(TransformLocalPos(_pos)), ToPx(_rot)));

	//PhysX 2.8 had a global NX_SKIN_WIDTH plus a per-shape skinWidth. PhysX 3+
	//has only the per-shape contact offset, so the global default is folded in
	//here -- see Manager::InitSDK.
	if (_skinWidth > 0.0f)
		shape.setContactOffset(_skinWidth);

	//Collision group; PhysX 3+ uses filter data rather than a group index.
	PxFilterData filter;
	filter.word0 = _group;
	shape.setSimulationFilterData(filter);
	shape.setQueryFilterData(filter);
}

ShapeType Shape::GetType() const
{
	return _type;
}

Shapes* Shape::GetOwner()
{
	return _owner;
}

Actor* Shape::GetActor()
{
	return _owner->GetActor();
}

PxShape* Shape::GetNxShape()
{
	return _nxShape;
}

const D3DXVECTOR3& Shape::GetPos() const
{
	return _pos;
}

void Shape::SetPos(const D3DXVECTOR3& value)
{
	_pos = value;
	if (_nxShape)
		SyncPos();
}

const D3DXQUATERNION& Shape::GetRot() const
{
	return _rot;
}

void Shape::SetRot(const D3DXQUATERNION& value)
{
	_rot = value;
	if (_nxShape)
		SyncRot();
}

const D3DXVECTOR3& Shape::GetScale() const
{
	return _scale;
}

void Shape::SetScale(D3DXVECTOR3& value)
{
	_scale = value;
	if (_nxShape)
		SyncScale();
}

PxU16 Shape::GetMaterialIndex()
{
	return _materialIndex;
}

void Shape::SetMaterialIndex(PxU16 value)
{
	if (_materialIndex != value)
	{
		_materialIndex = value;

		//Manager keeps the index-to-PxMaterial table PhysX 3+ dropped.
		if (_nxShape)
		{
			if (PxMaterial* material = Manager::GetMaterialByIndex(value))
			{
				PxMaterial* materials[1] = {material};
				_nxShape->setMaterials(materials, 1);
			}
		}
	}
}

float Shape::GetDensity() const
{
	return _density;
}

void Shape::SetDensity(float value)
{
	if (_density != value)
	{
		_density = value;
		ReloadNxShape();
	}
}

float Shape::GetSkinWidth() const
{
	return _skinWidth;
}

void Shape::SetSkinWidth(float value)
{
	if (_skinWidth != value)
	{
		_skinWidth = value;
		//2.8's per-shape skinWidth is PhysX 3+'s contact offset. See
		//Shape::ApplyToShape, which folds in the old global NX_SKIN_WIDTH.
		if (_nxShape && value > 0.0f)
			_nxShape->setContactOffset(value);
	}
}

unsigned Shape::GetGroup() const
{
	return _group;
}

void Shape::SetGroup(unsigned value)
{
	if (_group != value)
	{
		_group = value;
		//2.8 collision groups become filter data words in PhysX 3+; the pairwise
		//enable/disable table Scene sets up is expressed in the filter shader.
		if (_nxShape)
		{
			PxFilterData filter;
			filter.word0 = _group;
			_nxShape->setSimulationFilterData(filter);
			_nxShape->setQueryFilterData(filter);
		}
	}
}


PlaneShape::PlaneShape(Shapes* owner): _MyBase(owner), _normal(ZVector), _dist(0.0f)
{
	SetType(Type);
}

//PxPlaneGeometry carries no normal and no distance: a PhysX 3+ plane is always
//the YZ plane through the origin with +X as its normal, and the plane equation
//is expressed entirely by the shape's local pose.
//
//Two differences from 2.8 follow. The equations have opposite sign
//conventions -- 2.8 is n.X = d, PhysX 4.1 is n.v + d = 0 -- so the distance is
//negated. And 2.8 documented plane shapes as living in world space, ignoring
//both actor and shape pose; in PhysX 4.1 the plane is posed like any other
//shape, so a plane on a moving actor now moves with it. Every plane in this
//game is on a static actor, so that difference is currently inert.
void PlaneShape::SyncPlanePose()
{
	if (PxShape* shape = GetNxShape())
		shape->setLocalPose(PxTransformFromPlaneEquation(PxPlane(ToPx(_normal), -_dist)));
}

PxGeometryHolder PlaneShape::CreateGeometry()
{
	return PxGeometryHolder(PxPlaneGeometry());
}

void PlaneShape::ApplyToShape(PxShape& shape)
{
	_MyBase::ApplyToShape(shape);

	//Overrides the base pose deliberately: for a plane, _pos and _rot carry no
	//meaning and the equation is the whole of the transform.
	shape.setLocalPose(PxTransformFromPlaneEquation(PxPlane(ToPx(_normal), -_dist)));
}

void PlaneShape::Save(lsl::SWriter* writer)
{
	_MyBase::Save(writer);

	writer->WriteValue("normal", _normal, 3);
	writer->WriteValue("dist", _dist);
}

void PlaneShape::Load(lsl::SReader* reader)
{
	_MyBase::Load(reader);

	reader->ReadValue("normal", _normal, 3);
	reader->ReadValue("dist", _dist);
}


const D3DXVECTOR3& PlaneShape::GetNormal() const
{
	return _normal;
}

void PlaneShape::SetNormal(const D3DXVECTOR3& value)
{
	_normal = value;

	SyncPlanePose();
}

float PlaneShape::GetDist() const
{
	return _dist;
}

void PlaneShape::SetDist(float value)
{
	_dist = value;

	SyncPlanePose();
}


BoxShape::BoxShape(Shapes* owner): _MyBase(owner), _dimensions(NullVector)
{
	SetType(Type);
}

PxGeometryHolder BoxShape::CreateGeometry()
{
	//Both NxBoxShapeDesc::dimensions and PxBoxGeometry take half-extents.
	return PxGeometryHolder(PxBoxGeometry(_dimensions.x, _dimensions.y, _dimensions.z));
}

void BoxShape::Save(lsl::SWriter* writer)
{
	_MyBase::Save(writer);

	writer->WriteValue("dimensions", _dimensions, 3);
}

void BoxShape::Load(lsl::SReader* reader)
{
	_MyBase::Load(reader);

	reader->ReadValue("dimensions", _dimensions, 3);
}


const D3DXVECTOR3& BoxShape::GetDimensions() const
{
	return _dimensions;
}

void BoxShape::SetDimensions(const D3DXVECTOR3& value)
{
	if (_dimensions != value)
	{
		_dimensions = value;

		SyncGeometry();
	}
}


SphereShape::SphereShape(Shapes* owner): _MyBase(owner), _radius(1.0f)
{
	SetType(Type);
}

PxGeometryHolder SphereShape::CreateGeometry()
{
	return PxGeometryHolder(PxSphereGeometry(_radius));
}

void SphereShape::Save(lsl::SWriter* writer)
{
	_MyBase::Save(writer);

	writer->WriteValue("radius", _radius);
}

void SphereShape::Load(lsl::SReader* reader)
{
	_MyBase::Load(reader);

	reader->ReadValue("radius", _radius);	
}


float SphereShape::GetRadius() const
{
	return _radius;
}

void SphereShape::SetRadius(float value)
{
	_radius = value;

	SyncGeometry();
}


CapsuleShape::CapsuleShape(Shapes* owner): _MyBase(owner), _radius(1.0f), _height(1.0f), _capsuleFlags(0)
{
	SetType(Type);
}

PxGeometryHolder CapsuleShape::CreateGeometry()
{
	//Two convention changes from PhysX 2.8, both of which silently alter
	//collision if missed:
	//  - NxCapsuleShapeDesc::height is the full length of the cylindrical
	//    section; PxCapsuleGeometry takes half of it.
	//  - PhysX 2.8 capsules run along Y, PhysX 3+ capsules run along X. The
	//    rotation that corrects for this is applied in ApplyToShape via the
	//    local pose, see CapsuleShape::ApplyToShape.
	return PxGeometryHolder(PxCapsuleGeometry(_radius, _height * 0.5f));
}

void CapsuleShape::ApplyToShape(PxShape& shape)
{
	_MyBase::ApplyToShape(shape);

	//PhysX 2.8 capsules were aligned along Y, PhysX 3+ along X. Compose a
	//quarter turn about Z onto the local pose so the capsule keeps the
	//orientation the content was authored against.
	PxTransform pose = shape.getLocalPose();
	pose.q = pose.q * PxQuat(PxHalfPi, PxVec3(0.0f, 0.0f, 1.0f));
	shape.setLocalPose(pose);
}

void CapsuleShape::Save(lsl::SWriter* writer)
{
	_MyBase::Save(writer);

	writer->WriteValue("radius", _radius);
	writer->WriteValue("height", _height);
	writer->WriteValue("capsuleFlags", _capsuleFlags);
}

void CapsuleShape::Load(lsl::SReader* reader)
{
	_MyBase::Load(reader);

	reader->ReadValue("radius", _radius);
	reader->ReadValue("height", _height);
	reader->ReadValue("capsuleFlags", _capsuleFlags);
}


float CapsuleShape::GetRadius() const
{
	return _radius;
}

void CapsuleShape::SetRadius(float value)
{
	_radius = value;

	SyncGeometry();
}

float CapsuleShape::GetHeight() const
{
	return _height;
}

void CapsuleShape::SetHeight(float value)
{
	_height = value;

	SyncGeometry();
}

unsigned CapsuleShape::GetCapsuleFlags() const
{
	return _capsuleFlags;
}

void CapsuleShape::SetCapsuleFlags(unsigned value)
{
	_capsuleFlags = value;

	if (GetNxShape())
		ReloadNxShape();
}


TriangleMeshShape::TriangleMeshShape(Shapes* owner): _MyBase(owner), _mesh(0), _meshId(-1), _nxMesh(0)
{
	SetType(Type);
}

TriangleMeshShape::~TriangleMeshShape()
{
	SetMesh(0);
}

void TriangleMeshShape::FreeNxMesh()
{
	if (_nxMesh)
	{
		LSL_ASSERT(_mesh);

		_mesh->ReleaseTri(_nxMesh);
		_nxMesh = 0;
	}
}

PxGeometryHolder TriangleMeshShape::CreateGeometry()
{
	if (!_nxMesh)
		_nxMesh = _mesh ? _mesh->GetOrCreateTri(GetScale() * GetActor()->GetWorldScale(), _meshId) : 0;

	LSL_ASSERT(_nxMesh);

	return PxGeometryHolder(PxTriangleMeshGeometry(_nxMesh));
}

void TriangleMeshShape::SyncScale()
{
	FreeNxMesh();
	ReloadNxShape();
}

void TriangleMeshShape::Save(lsl::SWriter* writer)
{
	_MyBase::Save(writer);

	writer->WriteRef("mesh", _mesh);
	writer->WriteValue("meshId", _meshId);
}

void TriangleMeshShape::Load(lsl::SReader* reader)
{
	_MyBase::Load(reader);

	//reader->ReadRef("mesh", true, this, 0);
	//К сожалению инстанцирование актера происходит до фикса, поэтому пока без него
	FixUpName fixUp;
	reader->ReadRef("mesh", true, 0, &fixUp);
	reader->ReadValue("meshId", _meshId);

	SetMesh(fixUp.GetCollItem<TriangleMesh*>(), _meshId);
}

void TriangleMeshShape::OnFixUp(const FixUpNames& fixUpNames)
{
	_MyBase::OnFixUp(fixUpNames);

	for (FixUpNames::const_iterator iter = fixUpNames.begin(); iter != fixUpNames.end(); ++iter)
		if (iter->name == "mesh")
			SetMesh(iter->GetCollItem<TriangleMesh*>(), _meshId);
}


TriangleMesh* TriangleMeshShape::GetMesh()
{
	return _mesh;
}

void TriangleMeshShape::SetMesh(TriangleMesh* value, int meshId)
{
	if (ReplaceRef(_mesh, value) || _meshId != meshId)
	{
		FreeNxMesh();
		_mesh = value;
		_meshId = meshId;
		ReloadNxShape();
	}
}

int TriangleMeshShape::GetMeshId()
{
	return _meshId;
}


ConvexShape::ConvexShape(Shapes* owner): _MyBase(owner), _mesh(0), _meshId(-1), _nxMesh(0)
{
	SetType(Type);
}

ConvexShape::~ConvexShape()
{
	SetMesh(0);
}

void ConvexShape::FreeNxMesh()
{
	if (_nxMesh)
	{
		LSL_ASSERT(_mesh);

		_mesh->ReleaseConvex(_nxMesh);
		_nxMesh = 0;
	}
}

PxGeometryHolder ConvexShape::CreateGeometry()
{
	if (!_nxMesh)
		_nxMesh = _mesh ? _mesh->GetOrCreateConvex(GetScale() * GetActor()->GetWorldScale(), _meshId) : 0;

	LSL_ASSERT(_nxMesh);

	return PxGeometryHolder(PxConvexMeshGeometry(_nxMesh));
}

void ConvexShape::Save(lsl::SWriter* writer)
{
	_MyBase::Save(writer);

	writer->WriteRef("mesh", _mesh);
	writer->WriteValue("meshId", _meshId);
}

void ConvexShape::Load(lsl::SReader* reader)
{
	_MyBase::Load(reader);

	//reader->ReadRef("mesh", true, this, 0);
	//К сожалению инстанцирование актера происходит до фикса, поэтому пока без него
	FixUpName fixUp;
	reader->ReadRef("mesh", true, 0, &fixUp);
	reader->ReadValue("meshId", _meshId);

	SetMesh(fixUp.GetCollItem<TriangleMesh*>(), _meshId);
}

void ConvexShape::OnFixUp(const FixUpNames& fixUpNames)
{
	_MyBase::OnFixUp(fixUpNames);

	for (FixUpNames::const_iterator iter = fixUpNames.begin(); iter != fixUpNames.end(); ++iter)
		if (iter->name == "mesh")
			SetMesh(iter->GetCollItem<TriangleMesh*>(), _meshId);
}


TriangleMesh* ConvexShape::GetMesh()
{
	return _mesh;
}

void ConvexShape::SetMesh(TriangleMesh* value, int meshId)
{
	if (ReplaceRef(_mesh, value) || _meshId != meshId)
	{
		FreeNxMesh();
		_mesh = value;
		_meshId = meshId;
		ReloadNxShape();
	}
}

int ConvexShape::GetMeshId()
{
	return _meshId;
}


SpringDesc::SpringDesc(): spring(0.0f), damper(0.0f), targetValue(0.0f)
{
}

TireFunctionDesc::TireFunctionDesc():
	extremumSlip(1.0f),
	extremumValue(0.02f),
	asymptoteSlip(2.0f),
	asymptoteValue(0.01f),
	stiffnessFactor(1000000.0f)
{
}

WheelDesc::WheelDesc():
	radius(1.0f),
	suspensionTravel(1.0f),
	inverseWheelMass(1.0f),
	wheelFlags(0),
	motorTorque(0.0f),
	steerAngle(0.0f)
{
}

WheelContactData::WheelContactData():
	contactPoint(NullVector),
	contactNormal(NullVector),
	longitudalDirection(NullVector),
	lateralDirection(NullVector),
	contactForce(0.0f),
	longitudalSlip(0.0f),
	lateralSlip(0.0f),
	longitudalImpulse(0.0f),
	lateralImpulse(0.0f),
	otherShapeMaterialIndex(0),
	contactPosition(0.0f)
{
}

WheelShape::WheelShape(Shapes* owner): _MyBase(owner), _axleSpeed(0.0f), _brakeTorque(0.0f), _contactModify(0)
{
	SetType(Type);

	AssignFromDesc(WheelDesc(), false);
}

WheelShape::~WheelShape()
{
	SetContactModify(0);
}

PxGeometryHolder WheelShape::CreateGeometry()
{
	//A 2.8 wheel was not a solid. It cast a ray of suspensionTravel and applied
	//suspension and tire forces to the body itself, so it never collided in the
	//ordinary sense. PhysX 4.1 has no such shape, and PxVehicle models the wheel
	//as data on a PxVehicleWheels rather than as a shape at all.
	//
	//A sphere of the wheel radius is a placeholder so the shape object exists
	//and can still be found by name; ApplyToShape clears eSIMULATION_SHAPE so it
	//cannot collide in the meantime. Until the PxVehicle work lands, wheels have
	//no suspension and generate no tire force.
	return PxGeometryHolder(PxSphereGeometry(_radius > 0.0f ? _radius : 1.0f));
}

void WheelShape::ApplyToShape(PxShape& shape)
{
	_MyBase::ApplyToShape(shape);

	shape.setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
}

void WheelShape::AssignFromDesc(const WheelDesc& desc, bool reloadShape)
{
	_radius = desc.radius;
	_suspensionTravel = desc.suspensionTravel;
	_suspension = desc.suspension;
	_longitudalTireForceFunction = desc.longitudalTireForceFunction;
	_lateralTireForceFunction = desc.lateralTireForceFunction;
	_inverseWheelMass = desc.inverseWheelMass;
	_wheelFlags = desc.wheelFlags;
	_motorTorque = desc.motorTorque;
	_steerAngle = desc.steerAngle;

	//The base half of this used to be Shape::AssignFromDesc, which went with the
	//descriptor API. Shape properties are set through the accessors now, so the
	//radius is the only field here that changes the geometry.
	if (reloadShape)
		SyncGeometry();
}

void WheelShape::AssignToDesc(WheelDesc& desc)
{
	desc.radius = _radius;
	desc.suspensionTravel = _suspensionTravel;
	desc.suspension = _suspension;
	desc.longitudalTireForceFunction = _longitudalTireForceFunction;
	desc.lateralTireForceFunction = _lateralTireForceFunction;
	desc.inverseWheelMass = _inverseWheelMass;
	desc.wheelFlags = _wheelFlags;
	desc.motorTorque = _motorTorque;
	desc.steerAngle = _steerAngle;
}

void WheelShape::SaveTireForceFunction(lsl::SWriter* writer, const TireFunctionDesc& func)
{
	writer->WriteValue("asymptoteSlip", func.asymptoteSlip);
	writer->WriteValue("asymptoteValue", func.asymptoteValue);
	writer->WriteValue("extremumSlip", func.extremumSlip);
	writer->WriteValue("extremumValue", func.extremumValue);
	writer->WriteValue("stiffnessFactor", func.stiffnessFactor);
}

void WheelShape::LoadTireForceFunction(lsl::SReader* reader, TireFunctionDesc& func)
{
	reader->ReadValue("asymptoteSlip", func.asymptoteSlip);
	reader->ReadValue("asymptoteValue", func.asymptoteValue);
	reader->ReadValue("extremumSlip", func.extremumSlip);
	reader->ReadValue("extremumValue", func.extremumValue);
	reader->ReadValue("stiffnessFactor", func.stiffnessFactor);
}

void WheelShape::Save(lsl::SWriter* writer)
{
	_MyBase::Save(writer);

	writer->WriteValue("radius", _radius);
	writer->WriteValue("suspensionTravel", _suspensionTravel);	
	{
		lsl::SWriter* child = writer->NewDummyNode("suspension");
		child->WriteValue("damper", _suspension.damper);
		child->WriteValue("spring", _suspension.spring);
		child->WriteValue("targetValue", _suspension.targetValue);
	}
	{
		lsl::SWriter* child = writer->NewDummyNode("longitudalTireForceFunction");
		SaveTireForceFunction(child, _longitudalTireForceFunction);
	}
	{
		lsl::SWriter* child = writer->NewDummyNode("lateralTireForceFunction");
		SaveTireForceFunction(child, _lateralTireForceFunction);
	}
	writer->WriteValue("inverseWheelMass", _inverseWheelMass);
	writer->WriteValue("wheelFlags", _wheelFlags);
	writer->WriteValue("motorTorque", _motorTorque);
	writer->WriteValue("steerAngle", _steerAngle);
}

void WheelShape::Load(lsl::SReader* reader)
{
	_MyBase::Load(reader);

	reader->ReadValue("radius", _radius);
	reader->ReadValue("suspensionTravel", _suspensionTravel);
	if (lsl::SReader* child = reader->ReadValue("suspension"))
	{		
		child->ReadValue("damper", _suspension.damper);
		child->ReadValue("spring", _suspension.spring);
		child->ReadValue("targetValue", _suspension.targetValue);
	}
	if (lsl::SReader* child = reader->ReadValue("longitudalTireForceFunction"))	
		LoadTireForceFunction(child, _longitudalTireForceFunction);
	if (lsl::SReader* child = reader->ReadValue("lateralTireForceFunction"))	
		LoadTireForceFunction(child, _lateralTireForceFunction);
	reader->ReadValue("inverseWheelMass", _inverseWheelMass);
	reader->ReadValue("wheelFlags", _wheelFlags);
	reader->ReadValue("motorTorque", _motorTorque);
	reader->ReadValue("steerAngle", _steerAngle);
}


//Every setter below caches only. In PhysX 2.8 each pushed straight onto a live
//NxWheelShape; PhysX 4.1 has no such object, so the values sit here until the
//PxVehicle work reads them when building the vehicle. Radius is the exception,
//because it is the one field the placeholder geometry depends on.

float WheelShape::GetRadius() const
{
	return _radius;
}

void WheelShape::SetRadius(float value)
{
	if (_radius != value)
	{
		_radius = value;
		SyncGeometry();
	}
}

float WheelShape::GetSuspensionTravel() const
{
	return _suspensionTravel;
}

void WheelShape::SetSuspensionTravel(float value)
{
	_suspensionTravel = value;
}

const SpringDesc& WheelShape::GetSuspension() const
{
	return _suspension;
}

void WheelShape::SetSuspension(const SpringDesc& value)
{
	_suspension = value;
}

const TireFunctionDesc& WheelShape::GetLongitudalTireForceFunction() const
{
	return _longitudalTireForceFunction;
}

void WheelShape::SetLongitudalTireForceFunction(const TireFunctionDesc& value)
{
	_longitudalTireForceFunction = value;
}

const TireFunctionDesc& WheelShape::GetLateralTireForceFunction() const
{
	return _lateralTireForceFunction;
}

void WheelShape::SetLateralTireForceFunction(const TireFunctionDesc& value)
{
	_lateralTireForceFunction = value;
}

float WheelShape::GetInverseWheelMass() const
{
	return _inverseWheelMass;
}

void WheelShape::SetInverseWheelMass(float value)
{
	_inverseWheelMass = value;
}

UINT WheelShape::GetWheelFlags() const
{
	return _wheelFlags;
}

void WheelShape::SetWheelFlags(UINT value)
{
	_wheelFlags = value;
}

float WheelShape::GetMotorTorque() const
{
	return _motorTorque;
}

void WheelShape::SetMotorTorque(float value)
{
	_motorTorque = value;
}

float WheelShape::GetSteerAngle() const
{
	return _steerAngle;
}

void WheelShape::SetSteerAngle(float value)
{
	_steerAngle = value;
}

PxShape* WheelShape::GetContact(WheelContactData& data) const
{
	//No suspension raycast runs, so there is never a contact to report. Callers
	//test the return value, so reporting none is the safe answer -- reporting a
	//fabricated contact would drive tire trails and slip effects off invented
	//numbers.
	data = WheelContactData();
	return 0;
}

float WheelShape::GetAxleSpeed() const
{
	return _axleSpeed;
}

void WheelShape::SetAxleSpeed(float value)
{
	//NX_WF_AXLE_SPEED_OVERRIDE let the game drive this directly; otherwise the
	//solver computed it. Nothing computes it now, so the setter is the only
	//source.
	_axleSpeed = value;
}

float WheelShape::GetBrakeTorque() const
{
	return _brakeTorque;
}

void WheelShape::SetBrakeTorque(float value)
{
	_brakeTorque = value;
}

WheelShape::ContactModify* WheelShape::GetContactModify()
{
	return _contactModify;
}

void WheelShape::SetContactModify(ContactModify* value)
{
	if (ReplaceRef(_contactModify, value))
		_contactModify = value;
}


Body::Body(Actor* actor): _actor(actor)
{
}

void Body::Save(lsl::SWriter* writer)
{
	writer->WriteValue("mass", _desc.mass);	

	//Rows 0-2 are the basis, row 3 the translation -- the same 12 floats the
	//NxMat34 form wrote, so existing saves stay readable.
	D3DXVECTOR3 massLocalPose[4];
	for (int i = 0; i < 4; ++i)
		massLocalPose[i] = D3DXVECTOR3(_desc.massLocalPose.m[i][0], _desc.massLocalPose.m[i][1], _desc.massLocalPose.m[i][2]);
	writer->WriteValue("massLocalPose", massLocalPose[0], 12);

	writer->WriteValue("flags", _desc.flags);

	writer->WriteValue("sleepEnergyThreshold", _desc.sleepEnergyThreshold);

	lsl::SWriteValue(writer, "linearVelocity", _desc.linearVelocity);
}

void Body::Load(lsl::SReader* reader)
{
	reader->ReadValue("mass", _desc.mass);
	
	D3DXVECTOR3 massLocalPose[4];
	if (reader->ReadValue("massLocalPose", massLocalPose[0], 12))
	{
		D3DXMatrixIdentity(&_desc.massLocalPose);
		for (int i = 0; i < 4; ++i)
		{
			_desc.massLocalPose.m[i][0] = massLocalPose[i].x;
			_desc.massLocalPose.m[i][1] = massLocalPose[i].y;
			_desc.massLocalPose.m[i][2] = massLocalPose[i].z;
		}
	}

	reader->ReadValue("flags", _desc.flags);

	reader->ReadValue("sleepEnergyThreshold", _desc.sleepEnergyThreshold);

	lsl::SReadValue(reader, "linearVelocity", _desc.linearVelocity);
}

BodyDesc::BodyDesc(): mass(0.0f), flags(0), sleepEnergyThreshold(-1.0f), linearVelocity(NullVector)
{
	D3DXMatrixIdentity(&massLocalPose);
}




const BodyDesc& Body::GetDesc()
{
	return _desc;
}

void Body::SetDesc(const BodyDesc& value)
{
	_desc = value;

	//Only a dynamic actor has a velocity; a static one silently had none in 2.8
	//too, since NxActorDesc::body was null for it.
	if (_actor)
	{
		if (PxRigidDynamic* dynamic = _actor->GetNxDynamic())
			dynamic->setLinearVelocity(ToPx(value.linearVelocity));
	}
}


Shapes::Shapes(Actor* owner): _owner(owner)
{
	SetClassList(&classList);
}

void Shapes::RegisterClasses()
{
	classList.Add<PlaneShape>();
	classList.Add<BoxShape>();
	classList.Add<CapsuleShape>();
	classList.Add<SphereShape>();
	classList.Add<TriangleMeshShape>();
	classList.Add<ConvexShape>();	
	classList.Add<WheelShape>();
}

void Shapes::InsertItem(const Value& value)
{
	_MyBase::InsertItem(value);
	
	//По идее все условия соотв. тому что фигура не будет создана к этому моменту, но однако при нескольких sender-ах может произойти преждевременный вызов ReloadNxActor() !!!!. На самом деле если объеденить все эвенты в один то здесь проверка не нужна, но пока...
	if (_owner->_nxActor && !value->GetNxShape())
		_owner->CreateNxShape(value);
}

void Shapes::RemoveItem(const Value& value)
{
	_MyBase::RemoveItem(value);

	if (_owner->_nxActor && value->GetNxShape())
		_owner->DestroyNxShape(value);
}

Actor* Shapes::GetActor()
{
	return _owner;
}


Actor::Desc::Desc(): flags(0), contactReportFlags(0)
{
}

Actor::Actor(ActorUser* owner): _owner(owner), _nxActor(0), _scene(0), _parent(0), _body(0), _pos(NullVector), _rot(NullQuaternion), _scale(IdentityVector), storeCoords(true)
{
	_shapes = new Shapes(this);
}

Actor::~Actor()
{
	LSL_ASSERT(_children.size() == 0);

	if (_parent)
		_parent->RemoveChild(this);	
	SetScene(0);

	SetBody(0);
	delete _shapes;	
}

void Actor::CreateNxShape(Shape* shape)
{
	LSL_ASSERT(_nxActor && !shape->_nxShape);

	//not all conditions is completed to create nxShape (neccesary params will be set next, PxTriangleMesh for example)
	PxGeometryHolder geometry = shape->CreateGeometry();
	if (geometry.getType() == PxGeometryType::eINVALID)
	{
		shape->_delayInitialization = true;
		return;
	}

	//createExclusiveShape both creates the shape and attaches it, replacing
	//NxActor::createShape. The material is the one PhysX 2.8 kept at scene
	//index 0; per-shape material selection is still open -- see
	//Shape::SetMaterialIndex.
	PxMaterial* material = Manager::GetMaterialByIndex(shape->GetMaterialIndex());
	PxShape* nxShape = PxRigidActorExt::createExclusiveShape(*_nxActor, geometry.any(),
		material ? *material : Manager::GetDefaultMaterial());
	if (!nxShape)
	{
		shape->_delayInitialization = true;
		return;
	}

	shape->SetNxShape(nxShape);
	//ApplyToShape carries the local pose through LocalToWorldPos, which is what
	//the descriptor path did by hand before creating the shape.
	shape->ApplyToShape(*nxShape);
}

void Actor::DestroyNxShape(Shape* shape)
{
	LSL_ASSERT(_nxActor && shape->_nxShape);

	PxShape* tmp = shape->_nxShape;
	shape->SetNxShape(0);
	_nxActor->detachShape(*tmp);
}

void Actor::ReloadNxShape(Shape* shape, bool allowInitialization)
{
	if (_nxActor && shape->_nxShape)
	{
		//У фигуры должен быть по крайней мере 1 shape
		PxShape* oldNxShape = shape->_nxShape;
		shape->SetNxShape(0);

		CreateNxShape(shape);

		_nxActor->detachShape(*oldNxShape);
	}
	else if ((allowInitialization || shape->_delayInitialization) && _nxActor && shape->_nxShape == NULL)
	{
		CreateNxShape(shape);
	}
}

unsigned Actor::CountShapesIncludeChildren() const
{
	unsigned res = _shapes->Size();

	for (Children::const_iterator iter = _children.begin(); iter != _children.end(); ++iter)
		res += (*iter)->CountShapesIncludeChildren();

	return res;
}

void Actor::CreateNxShapesIncludeChildren()
{
	for (Shapes::iterator iter = _shapes->begin(); iter != _shapes->end(); ++iter)
		CreateNxShape(*iter);

	for (Children::iterator iter = _children.begin(); iter != _children.end(); ++iter)
		(*iter)->CreateNxShapesIncludeChildren();
}

void Actor::SetNxActorIncludeChildren(PxRigidActor* value)
{
	_nxActor = value;
	if (!_nxActor)
		for (Shapes::iterator iter = _shapes->begin(); iter != _shapes->end(); ++iter)
			(*iter)->SetNxShape(0);

	for (Children::iterator iter = _children.begin(); iter != _children.end(); ++iter)
		(*iter)->_nxActor = value;
}

void Actor::InitRootNxActor()
{
	if (!_nxActor && _scene)
	{
		//Пустые физические актеры не инстанцируем
		if (CountShapesIncludeChildren() == 0)
			return;

		PxTransform pose(ToPx(_pos), ToPx(_rot));
		if (!pose.isSane())
		{
			LSL_LOG("Actor::InitRootNxActor pose is not sane");
			return;
		}

		//PhysX 3+ fixes static versus dynamic at creation. _body is the same
		//discriminator NxActorDesc::body was.
		_nxActor = _scene->CreateNxActor(pose, _body != 0, this);

		if (!_nxActor)
		{
			LSL_LOG("Actor::InitRootNxActor !_nxActor");
			throw lsl::Error("Actor::InitNxActor failed");
		}

		//Shapes are attached to the actor rather than described before it, so
		//this happens after creation and there is no list to unpack afterwards.
		SetNxActorIncludeChildren(_nxActor);
		CreateNxShapesIncludeChildren();

		if (_body)
			ApplyBodyDesc();

		if (_owner && _body)
			_owner->OnSetBody(true);
	}
}

void Actor::ApplyBodyDesc()
{
	PxRigidDynamic* dynamic = GetNxDynamic();
	if (!dynamic)
		return;

	const BodyDesc& desc = _body->GetDesc();

	dynamic->setMass(desc.mass);
	dynamic->setLinearVelocity(ToPx(desc.linearVelocity));
	dynamic->setSleepThreshold(desc.sleepEnergyThreshold);

	//NX_BF_DISABLE_GRAVITY.
	dynamic->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, GetFlag(bfDisableGravity));

	//Если установлен такой флаг то центр масс не вычисляется при создании, а значит должен браться из значения указанного в body
	if (GetFlag(bfLockCenterOfMass))
	{
		//massLocalPose keeps D3DX's row-vector layout: rows 0-2 the basis,
		//row 3 the translation. See BodyDesc.
		const D3DXMATRIX& m = desc.massLocalPose;
		PxMat33 basis(
			PxVec3(m._11, m._12, m._13),
			PxVec3(m._21, m._22, m._23),
			PxVec3(m._31, m._32, m._33));

		dynamic->setCMassLocalPose(PxTransform(PxVec3(m._41, m._42, m._43), PxQuat(basis)));
	}
	else
	{
		//BEHAVIOUR GAP: 2.8 computed mass and inertia from the shapes and their
		//per-shape density when no explicit centre of mass was given.
		//Shape::_density is still stored and serialised but no longer reaches
		//the simulation, so this uses the body mass with a computed inertia
		//instead. Closing it means PxRigidBodyExt::updateMassAndInertia with the
		//per-shape densities.
		PxRigidBodyExt::setMassAndUpdateInertia(*dynamic, desc.mass);
	}

	//BEHAVIOUR GAP: bfDisableResponse (NX_AF_DISABLE_RESPONSE) is expressed by
	//clearing PxShapeFlag::eSIMULATION_SHAPE on every shape, and
	//bfContactModification (NX_AF_CONTACT_MODIFICATION) by the eMODIFY_CONTACTS
	//pair flag. Neither is applied yet -- the filter shader currently requests
	//contact modification for every pair.
}

void Actor::FreeRootNxActor()
{
	if (_nxActor)
	{
		LSL_ASSERT(_scene);

		_scene->ReleaseNxActor(_nxActor, this);
		_nxActor = 0;
		SetNxActorIncludeChildren(0);

		if (_owner && _body)
			_owner->OnSetBody(false);
	}
}

void Actor::InitChildNxActor()
{
	LSL_ASSERT(_parent);

	//поле _nxActor также служит для индикации состояния инциализированности
	if (!_nxActor && _parent->_nxActor)
	{
		_nxActor = _parent->_nxActor;
		for (Shapes::iterator iter = _shapes->begin(); iter != _shapes->end(); ++iter)
			CreateNxShape(*iter);
	}
}

void Actor::FreeChildNxActor()
{
	if (_nxActor)	
	{
		for (Shapes::iterator iter = _shapes->begin(); iter != _shapes->end(); ++iter)
			DestroyNxShape(*iter);
		_nxActor = 0;
	}
}

void Actor::InitNxActor()
{
	if (_parent)	
		InitChildNxActor();
	else	
		InitRootNxActor();
}

void Actor::FreeNxActor()
{
	if (_parent)	
		FreeChildNxActor();
	else	
		FreeRootNxActor();
}

void Actor::ReloadNxActor()
{
	FreeNxActor();
	InitNxActor();
}

void Actor::Save(lsl::SWriter* writer)
{
	//2.8 read the live actor's state back into the descriptor before saving.
	//PhysX 3+ has no descriptor to read back into, and both fields _desc still
	//carries are engine-side settings the simulation never changes, so the
	//cached values are already current.

	writer->WriteValue("flags", _desc.flags);
	writer->WriteValue("contactReportFlags", _desc.contactReportFlags);

	if (_body)
		writer->WriteValue("body", _body);

	if (storeCoords)
	{
		writer->WriteValue("pos", GetPos(), 3);
		writer->WriteValue("rot", GetRot(), 4);
		writer->WriteValue("scale", GetScale(), 3);
	}

	writer->WriteValue("shapes", _shapes);
	writer->WriteRef("scene", _scene);	
}

void Actor::Load(lsl::SReader* reader)
{
	SetScene(0);

	reader->ReadValue("flags", _desc.flags);
	reader->ReadValue("contactReportFlags", _desc.contactReportFlags);

	if (lsl::SReader* child = reader->ReadValue("body"))
	{
		if (!_body)
			_body = new Body(this);
		child->LoadSerializable(_body);
	}

	if (storeCoords)
	{
		reader->ReadValue("pos", _pos, 3);
		reader->ReadValue("rot", _rot, 4);
		reader->ReadValue("scale", _scale, 3);
	}

	reader->ReadValue("shapes", _shapes);
	//Читаем ссылку на сцену в саму последнию очередь, потому что после её фикса актер перегружается
	reader->ReadRef("scene", false, this, 0);
}

void Actor::OnFixUp(const FixUpNames& fixUpNames)
{
	for (FixUpNames::const_iterator iter = fixUpNames.begin(); iter != fixUpNames.end(); ++iter)
		if (iter->name == "scene")
			SetScene(iter->GetComponent<Scene*>());
}

void Actor::InsertChild(Actor* child)
{
	LSL_ASSERT(!child->_parent);
	
	child->FreeNxActor();
	//
	child->_parent = this;
	_children.push_back(child);
	child->SetScene(_scene);	
	//
	child->InitNxActor();
}

void Actor::RemoveChild(Actor* child)
{
	LSL_ASSERT(child->_parent == this);

	child->FreeNxActor();
	child->_parent = 0;
	_children.remove(child);
	child->InitNxActor();
}

void Actor::LocalToWorldPos(const D3DXVECTOR3& inValue, D3DXVECTOR3& outValue, bool nxActorSpace)
{
	outValue = inValue;
	Actor* curNode = this;
	while (curNode && !(nxActorSpace && !curNode->GetParent()))
	{
		outValue = inValue + curNode->_pos;
		curNode = _parent->_parent;
	}
}

void Actor::WorldToLocalPos(const D3DXVECTOR3& inValue, D3DXVECTOR3& outValue, bool nxActorSpace)
{
	outValue = inValue;
}

BoxShape& Actor::AddBBShape(const AABB& aabb)
{
	D3DXVECTOR3 sizes = aabb.GetSizes();
	sizes /= 2.0f;

	px::BoxShape& bbShape = GetShapes().Add<px::BoxShape>();
	bbShape.SetDimensions(sizes);
	bbShape.SetPos(aabb.GetCenter());

	return bbShape;
}

ActorUser* Actor::GetOwner()
{
	return _owner;
}

PxRigidActor* Actor::GetNxActor()
{
	return _nxActor;
}

PxRigidDynamic* Actor::GetNxDynamic()
{
	return _nxActor ? _nxActor->is<PxRigidDynamic>() : 0;
}

Scene* Actor::GetScene()
{
	return _scene;
}

void Actor::SetScene(Scene* value)
{
	if (ReplaceRef(_scene, value))
	{
		FreeNxActor();

		//Если у родителя другой мэнеджер то происходит отсоеденение текущего узла
		//if (_parent && _parent->_scene != value)
		//	SetParent(0);
		_scene = value;
		InitNxActor();

		for (Children::iterator iter = _children.begin(); iter != _children.end(); ++iter)
			(*iter)->SetScene(value);
		if (_parent)		
			_parent->SetScene(value);		
	}
}

Actor* Actor::GetParent()
{
	return _parent;
}

void Actor::SetParent(Actor* value)
{
	if (_parent != value)
	{
		if (_parent)
			_parent->RemoveChild(this);
		if (value)
			value->InsertChild(this);
		else
			InitNxActor();
	}
}

Body* Actor::GetBody()
{
	return _body;
}

void Actor::SetBody(const BodyDesc* value)
{
	if (value)
	{
		if (!_body)
			_body = new Body(this);
		_body->SetDesc(*value);
	}
	else if (_body)
	{
		lsl::SafeDelete(_body);
		ReloadNxActor();
	}
}

Shapes& Actor::GetShapes()
{
	return *_shapes;
}

unsigned Actor::GetFlags() const
{
	return _desc.flags;
}

bool Actor::GetFlag(unsigned value) const
{
	return (_desc.flags & value) > 0 ? true : false;
}

void Actor::SetFlags(unsigned value)
{
	_desc.flags = value;

	ReloadNxActor();
}

void Actor::SetFlag(unsigned value, bool set)
{
	SetFlags(set ? _desc.flags | value : _desc.flags ^ value);
}

unsigned Actor::GetContactReportFlags() const
{
	return _desc.contactReportFlags;
}

bool Actor::GetContactReportFlag(unsigned value) const
{
	return (_desc.contactReportFlags & value) > 0 ? true : false;
}

void Actor::SetContactReportFlags(unsigned value)
{
	_desc.contactReportFlags = value;

	ReloadNxActor();
}

void Actor::SetContactReportFlag(unsigned value, bool set)
{
	SetContactReportFlags(set ? _desc.contactReportFlags | value : _desc.contactReportFlags ^ value);
}

const D3DXVECTOR3& Actor::GetPos() const
{
	if (!_parent && _nxActor)
		_pos = FromPx(_nxActor->getGlobalPose().p);

	return _pos;
}

void Actor::SetPos(const D3DXVECTOR3& value)
{
	_pos = value;
	if (_nxActor)
	{
		if (!_parent)
		{
			//PhysX 3+ has one global pose rather than separate position and
			//orientation setters.
			PxTransform pose = _nxActor->getGlobalPose();
			pose.p = ToPx(value);
			_nxActor->setGlobalPose(pose);
		}
		else
			for (Shapes::iterator iter = _shapes->begin(); iter != _shapes->end(); ++iter)
				(*iter)->SyncPos();
	}
}

const D3DXQUATERNION& Actor::GetRot() const
{
	if (!_parent && _nxActor)
		_rot = FromPx(_nxActor->getGlobalPose().q);

	return _rot;
}

void Actor::SetRot(const D3DXQUATERNION& value)
{
	_rot = value;
	if (_nxActor)
	{
		if (!_parent)
		{
			PxTransform pose = _nxActor->getGlobalPose();
			pose.q = ToPx(value);
			_nxActor->setGlobalPose(pose);
		}
		else
			for (Shapes::iterator iter = _shapes->begin(); iter != _shapes->end(); ++iter)
				(*iter)->SyncRot();
	}
}

const D3DXVECTOR3& Actor::GetScale() const
{
	return _scale;
}

void Actor::SetScale(const D3DXVECTOR3& value)
{
	_scale = value;
	if (_nxActor)
	{
		for (Shapes::iterator iter = _shapes->begin(); iter != _shapes->end(); ++iter)
			(*iter)->SyncScale();
	}
}

D3DXVECTOR3 Actor::GetWorldScale() const
{
	const Actor* actor = this;
	D3DXVECTOR3 scale = IdentityVector;

	while (actor)
	{
		scale *= actor->_scale;
		actor = actor->_parent;
	}

	return scale;
}

}

}