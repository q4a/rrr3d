#include "stdafx.h"

#include "px\\Physx.h"
#include "px\\Stream.h"

#include "lslSerialValue.h"

namespace r3d
{

namespace px
{

#ifdef _DEBUG
	#define SAMPLES_USE_VRD
	//Change this setting to the IP number or DNS name of the computer that is running the VRD
	const char* const cSamplesVRDHost = "localhost";
	//Change this setting to the port on which the VRD is listening, or keep the default: NX_DBG_DEFAULT_PORT
	const DWORD cNxDbgDefaultPort = NX_DBG_DEFAULT_PORT;
	//Change this setting to decide what type of information is sent to the VRD. Default: NX_DBG_EVENTMASK_EVERYTHING
	const DWORD cSamplesVrdEventMask = NX_DBG_EVENTMASK_EVERYTHING;
#endif

//const float Scene::maxTimeStep = 1.0f/75.0f;
//const unsigned Scene::maxSimIter = 8;
const NxVec3 Scene::cDefGravity(0.0f, 0.0f, -20.0f);
const int Scene::cDefMatInd = 0;

NxPhysicsSDK* Manager::_nxSDK = 0;
NxCookingInterface* Manager::_nxCooking = 0;
unsigned Manager::_sdkRefCnt = 0;

Shapes::ClassList Shapes::classList;




Scene::Scene(Manager* manager): _manager(manager), _lastDeltaTime(0)
{
	_contactModify = new ContactModify(this);
	_contactReport = new ContactReport(this);
	_userNotify = new UserNotify(this);

	NxSceneDesc sceneDesc;
	sceneDesc.gravity				= cDefGravity;
	sceneDesc.userContactReport		= 0;
	sceneDesc.upAxis                = 2; //ZVector
	sceneDesc.timeStepMethod        = NX_TIMESTEP_VARIABLE;
	//sceneDesc.maxTimestep           = maxTimeStep;
	//sceneDesc.maxIter               = maxSimIter;
	sceneDesc.userContactModify     = _contactModify;
	sceneDesc.userContactReport     = _contactReport;
	sceneDesc.userNotify            = _userNotify;

	_nxScene = _manager->GetSDK().createScene(sceneDesc);

	NxMaterial* defMat = _nxScene->getMaterialFromIndex(0);
	defMat->setStaticFriction(0.5f);
	defMat->setDynamicFriction(0.5f);
	defMat->setRestitution(0.5f);

	_nxScene->setGroupCollisionFlag(cdgShot, cdgShot, false);
	//
	_nxScene->setGroupCollisionFlag(cdgShotBorder, cdgShotBorder, false);
	_nxScene->setGroupCollisionFlag(cdgShotBorder, cdgShot, false);
	//
	_nxScene->setGroupCollisionFlag(cdgShotTrack, cdgShot, false);
	_nxScene->setGroupCollisionFlag(cdgShotTrack, cdgShotBorder, false);
	_nxScene->setGroupCollisionFlag(cdgShotTrack, cdgShotTrack, false);		
	//
	_nxScene->setGroupCollisionFlag(cdgShotTransparency, cdgShot, false);
	//
	_nxScene->setGroupCollisionFlag(cdgWheel, cdgShot, false);
	_nxScene->setGroupCollisionFlag(cdgWheel, cdgShotBorder, false);
	_nxScene->setGroupCollisionFlag(cdgWheel, cdgShotTrack, false);
	_nxScene->setGroupCollisionFlag(cdgWheel, cdgShotTransparency, false);
	//
	_nxScene->setGroupCollisionFlag(cdgTrackPlane, cdgShot, false);
	_nxScene->setGroupCollisionFlag(cdgTrackPlane, cdgShotBorder, false);
}

Scene::~Scene()
{
	LSL_ASSERT(_userList.empty());

	_manager->GetSDK().releaseScene(*_nxScene);

	delete _userNotify;
	delete _contactReport;
	delete _contactModify;
}

Scene::ContactModify::ContactModify(Scene* scene): _scene(scene)
{
}

bool Scene::ContactModify::onContactConstraint(NxU32& changeFlags, const NxShape* shape0, const NxShape* shape1, const NxU32 featureIndex0, const NxU32 featureIndex1, NxContactCallbackData& data)
{
	OnContactModifyEvent contactEvent;	
	
	contactEvent.shape0 = shape0;
	contactEvent.shape1 = shape1;
	contactEvent.featureIndex0 = featureIndex0;
	contactEvent.featureIndex1 = featureIndex1;

	contactEvent.changeFlags = &changeFlags;
	contactEvent.data = &data;

	Actor* actor0 = _scene->GetActorFromNx(&shape0->getActor());
	Actor* actor1 = _scene->GetActorFromNx(&shape1->getActor());

	if (actor0 && actor1)
	{
		//Отправляем событие первому актеру
		contactEvent.actor = actor1;
		contactEvent.actorIndex = 1;
		if (actor0->GetOwner() && !actor0->GetOwner()->OnContactModify(contactEvent))
			return false;

		//Отправляем событие второму актеру
		contactEvent.actor = actor0;
		contactEvent.actorIndex = 0;
		if (actor1->GetOwner() && !actor1->GetOwner()->OnContactModify(contactEvent))
			return false;
	}

	return true;
}

Scene::ContactReport::ContactReport(Scene* scene): _scene(scene)
{
}

void Scene::ContactReport::onContactNotify(NxContactPair& pair, NxU32 events)
{
	OnContactEvent contact1;
	OnContactEvent contact2;

	contact1.pair = &pair;
	contact1.events = events;
	contact1.deltaTime = _scene->_lastDeltaTime;
	contact1.stream = pair.stream;
	contact1.sumNormalForce = D3DXVECTOR3(pair.sumNormalForce.get());
	contact1.sumFrictionForce = D3DXVECTOR3(pair.sumFrictionForce.get());
	contact2 = contact1;

	contact1.actor = !pair.isDeletedActor[0] ? _scene->GetActorFromNx(pair.actors[0]) : 0;
	contact1.actorIndex = 0;
	contact2.actor = !pair.isDeletedActor[1] ? _scene->GetActorFromNx(pair.actors[1]) : 0;
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

Scene::UserNotify::UserNotify(Scene* scene): _scene(scene)
{
}

void Scene::UserNotify::onWake(NxActor** actors, NxU32 count)
{
	for (unsigned i = 0; i < count; ++i)
	{
		Actor* actor = _scene->GetActorFromNx(actors[i]);

		if (actor && actor->GetOwner())
			actor->GetOwner()->OnWake();
	}
}

void Scene::UserNotify::onSleep(NxActor** actors, NxU32 count)
{
	for (unsigned i = 0; i < count; ++i)
	{
		Actor* actor = _scene->GetActorFromNx(actors[i]);

		if (actor && actor->GetOwner())
			actor->GetOwner()->OnSleep();
	}
}

Actor* Scene::GetActorFromNx(NxActor* actor)
{
	return actor->userData ? reinterpret_cast<Actor*>(actor->userData) : 0;
}

Actor* Scene::GetActorFromNxShape(NxShape* shape)
{
	return GetActorFromNx(&shape->getActor());
}

void Scene::CreateGroundPlane()
{
	/*NxPlaneShapeDesc planeShape;
	planeShape.normal.set(ZVector);
	NxActorDesc planeActor;
	planeActor.shapes.push_back(&planeShape);
	_nxScene->createActor(planeActor);*/
}

NxActor* Scene::CreateNxActor(const NxActorDesc& desc, Actor* actor)
{
	if (!desc.isValid())
		throw lsl::Error("NxActor* Scene::CreateNxActor(const NxActorDesc& desc, Actor* actor)");

	NxActor* res = _nxScene->createActor(desc);
	res->userData = actor;
	return res;
}

void Scene::ReleaseNxActor(NxActor* nxActor, Actor* actor)
{
	_nxScene->releaseActor(*nxActor);
}

void Scene::Compute(float deltaTime)
{
	_lastDeltaTime = deltaTime;

	_nxScene->simulate(deltaTime);
	_nxScene->flushStream();
	_nxScene->fetchResults(NX_RIGID_BODY_FINISHED, true);
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

NxScene* Scene::GetNxScene()
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

		NxPhysicsSDKDesc desc;
		NxSDKCreateError errorCode = NXCE_NO_ERROR;
		Manager::_nxSDK = NxCreatePhysicsSDK(NX_PHYSICS_SDK_VERSION, NULL, 0, desc, &errorCode);
		NxPhysicsSDK* nxSDK = Manager::_nxSDK;

		if(!nxSDK)
		{
			std::stringstream sstream;
			sstream << "\nSDK create error ("<<errorCode<<" - "<<errorCode<<").\nUnable to initialize the Physx SDK, exiting the sample.\n\n";
			throw lsl::Error(sstream.str());
		}

#ifdef SAMPLES_USE_VRD
		// The settings for the VRD host and port are found in SampleCommonCode/SamplesVRDSettings.h
		if (nxSDK->getFoundationSDK().getRemoteDebugger() && !nxSDK->getFoundationSDK().getRemoteDebugger()->isConnected())
			nxSDK->getFoundationSDK().getRemoteDebugger()->connect(cSamplesVRDHost, cNxDbgDefaultPort, cSamplesVrdEventMask);
#endif
		//Чтобы тачка не вела себя странно
		nxSDK->setParameter(NX_ADAPTIVE_FORCE, 0.0f);
		//Допустимое взаимопроникновение тел
		nxSDK->setParameter(NX_SKIN_WIDTH, 0.025f);

		LSL_LOG("px create cooking");

		Manager::_nxCooking = NxGetCookingLib(NX_PHYSICS_SDK_VERSION);
		if (!Manager::_nxCooking)
			throw lsl::Error("The cooking library has not been initialized");
		Manager::_nxCooking->NxInitCooking();
	}

	
}

void Manager::ReleaseSDK()
{
	LSL_ASSERT(_sdkRefCnt > 0);

	if (--_sdkRefCnt == 0)
	{
		Manager::_nxSDK->release();
		Manager::_nxSDK = 0;

		Manager::_nxCooking->NxCloseCooking();
		Manager::_nxCooking = 0;
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

NxPhysicsSDK& Manager::GetSDK()
{
	return *_nxSDK;
}

NxCookingInterface& Manager::GetCooking()
{
	return *_nxCooking;
}




TriangleMesh::TriangleMesh(): _meshData(0)
{
}

TriangleMesh::~TriangleMesh()
{
	SetMeshData(0);
}

void TriangleMesh::LoadMesh(const D3DXVECTOR3& scale, int id, NxTriangleMeshDesc& desc)
{
	LSL_ASSERT(_meshData);

	if (!_meshData->IsInit())
		_meshData->Load();

	if (!_meshData->vb.GetFormat(res::VertexData::vtPos3))
		throw lsl::Error("NxTriangleMesh* TriangleMesh::GetOrCreateMesh(const D3DXVECTOR3& scale)");

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

	desc.numVertices          = vertCnt;
	desc.pointStrideBytes     = sizeof(D3DXVECTOR3);
	desc.points               = vertices;
	desc.numTriangles         = faceCnt;
	desc.triangleStrideBytes  = _meshData->fb.GetFaceSize();		
	desc.triangles            = _meshData->fb.GetData() + _meshData->fb.GetFaceSize() * sFace;
	desc.flags                = 0;
}

void TriangleMesh::FreeMesh(NxTriangleMeshDesc& desc)
{
	delete desc.points;
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

NxTriangleMesh* TriangleMesh::GetOrCreateTri(const D3DXVECTOR3& scale, int id)
{
	MeshList::iterator mesh = GetOrCreateMesh(scale, id);
	++(mesh->triRef);

	if (mesh->tri)	
		return mesh->tri;

	NxTriangleMeshDesc desc;
	LoadMesh(scale, id, desc);

	MemoryWriteBuffer buf;
	if (!px::GetCooking().NxCookTriangleMesh(desc, buf))
		throw lsl::Error("Error cooking TriangleMesh");
	MemoryReadBuffer readBuffer(buf.data);

	mesh->tri = px::GetSDK().createTriangleMesh(readBuffer);

	FreeMesh(desc);

	return mesh->tri;
}

void TriangleMesh::ReleaseTri(NxTriangleMesh* mesh)
{
	for (MeshList::iterator iter = _meshList.begin(); iter != _meshList.end(); ++iter)
	{
		if (iter->tri == mesh)
		{
			if (--(iter->triRef) == 0)
			{
				px::GetSDK().releaseTriangleMesh(*iter->tri);
				iter->tri = 0;
			}

			ReleaseMesh(iter);
			return;
		}
	}

	LSL_ASSERT(false);
}

NxConvexMesh* TriangleMesh::GetOrCreateConvex(const D3DXVECTOR3& scale, int id)
{
	MeshList::iterator mesh = GetOrCreateMesh(scale, id);
	++(mesh->convexRef);

	if (mesh->convex)	
		return mesh->convex;

	NxTriangleMeshDesc desc;
	LoadMesh(scale, id, desc);

	NxConvexMeshDesc convexDesc;
	convexDesc.numVertices          = desc.numVertices;
	convexDesc.pointStrideBytes     = desc.pointStrideBytes;
	convexDesc.points               = desc.points;
	convexDesc.numTriangles         = desc.numTriangles;
	convexDesc.triangleStrideBytes  = desc.triangleStrideBytes;
	convexDesc.triangles            = desc.triangles;
	convexDesc.flags                |= NX_CF_COMPUTE_CONVEX;


	MemoryWriteBuffer buf;
	if (!px::GetCooking().NxCookConvexMesh(convexDesc, buf))
		throw lsl::Error("Error cooking TriangleMesh");
	MemoryReadBuffer readBuffer(buf.data);

	mesh->convex = px::GetSDK().createConvexMesh(readBuffer);

	FreeMesh(desc);

	return mesh->convex;
}

void TriangleMesh::ReleaseConvex(NxConvexMesh* mesh)
{
	for (MeshList::iterator iter = _meshList.begin(); iter != _meshList.end(); ++iter)
	{
		if (iter->convex == mesh)
		{
			if (--(iter->convexRef) == 0)
			{
				px::GetSDK().releaseConvexMesh(*iter->convex);
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

void Shape::SetNxShape(NxShape* value)
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

void Shape::SyncPos()
{
	LSL_ASSERT(_nxShape);

	_nxShape->setLocalPosition(NxVec3(TransformLocalPos(_pos)));
}

void Shape::SyncRot()
{
	LSL_ASSERT(_nxShape);

	NxQuat quat;
	quat.setXYZW(_rot);
	_nxShape->setLocalOrientation(NxMat33(quat));
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

void Shape::AssignFromDesc(const NxShapeDesc& desc, bool reloadShape)
{
	desc.localPose.t.get(_pos);
	//
	NxQuat quat;
	desc.localPose.M.toQuat(quat);
	quat.getXYZW(_rot);

	_materialIndex = desc.materialIndex;
	_density = desc.density;
	_skinWidth = desc.skinWidth;
	_group = desc.group;
	if (reloadShape)
		ReloadNxShape();
}

void Shape::AssignToDesc(NxShapeDesc& desc)
{
	desc.localPose.t.set(_pos);
	//
	NxQuat quat;
	quat.setXYZW(_rot);
	desc.localPose.M.fromQuat(quat);

	desc.materialIndex = _materialIndex;
	desc.density = _density;
	desc.skinWidth = _skinWidth;
	desc.group = _group;
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

NxShape* Shape::GetNxShape()
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

NxU16 Shape::GetMaterialIndex()
{
	return _materialIndex;
}

void Shape::SetMaterialIndex(NxU16 value)
{
	if (_materialIndex != value)
	{
		_materialIndex = value;
		if (_nxShape)
			_nxShape->setMaterial(_materialIndex);
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
		if (_nxShape)
			_nxShape->setSkinWidth(value);
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
		if (_nxShape)
			_nxShape->setGroup(_group);
	}
}




PlaneShape::PlaneShape(Shapes* owner): _MyBase(owner), _normal(ZVector), _dist(0.0f)
{
	SetType(Type);
}

NxShapeDesc* PlaneShape::CreateDesc()
{
	NxPlaneShapeDesc* desc = new NxPlaneShapeDesc();
	AssignToDesc(*desc);
	return desc;
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

void PlaneShape::AssignFromDesc(const NxPlaneShapeDesc& desc, bool reloadShape)
{
	_normal = desc.normal.get();
	_dist = desc.d;

	_MyBase::AssignFromDesc(desc, reloadShape);
}

void PlaneShape::AssignToDesc(NxPlaneShapeDesc& desc)
{
	desc.normal.set(_normal);
	desc.d = _dist;

	_MyBase::AssignToDesc(desc);
}

NxPlaneShape* PlaneShape::GetNxShape()
{
	return static_cast<NxPlaneShape*>(_MyBase::GetNxShape());
}

const D3DXVECTOR3& PlaneShape::GetNormal() const
{
	return _normal;
}

void PlaneShape::SetNormal(const D3DXVECTOR3& value)
{
	_normal = value;
	
	if (GetNxShape())	
		GetNxShape()->setPlane(NxVec3(value), _dist);
}

float PlaneShape::GetDist() const
{
	return _dist;
}

void PlaneShape::SetDist(float value)
{
	_dist = value;
	
	if (GetNxShape())	
		GetNxShape()->setPlane(NxVec3(value), _dist);
}




BoxShape::BoxShape(Shapes* owner): _MyBase(owner), _dimensions(NullVector)
{
	SetType(Type);
}

NxShapeDesc* BoxShape::CreateDesc()
{
	NxBoxShapeDesc* desc = new NxBoxShapeDesc();
	AssignToDesc(*desc);

	return desc;
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

void BoxShape::AssignFromDesc(const NxBoxShapeDesc& desc, bool reloadShape)
{
	_dimensions = desc.dimensions.get();

	_MyBase::AssignFromDesc(desc, reloadShape);
}

void BoxShape::AssignToDesc(NxBoxShapeDesc& desc)
{
	desc.dimensions.set(_dimensions);

	_MyBase::AssignToDesc(desc);
}

NxBoxShape* BoxShape::GetNxShape()
{
	return static_cast<NxBoxShape*>(_MyBase::GetNxShape());
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
		
		if (GetNxShape())
		{
			NxVec3 vec3;
			vec3.set(_dimensions);
			GetNxShape()->setDimensions(vec3);
		}
	}
}




SphereShape::SphereShape(Shapes* owner): _MyBase(owner), _radius(1.0f)
{
	SetType(Type);
}

NxShapeDesc* SphereShape::CreateDesc()
{
	NxSphereShapeDesc* desc = new NxSphereShapeDesc();
	AssignToDesc(*desc);
	
	return desc;	
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

void SphereShape::AssignFromDesc(const NxSphereShapeDesc& desc, bool reloadShape)
{
	_radius = desc.radius;
	
	_MyBase::AssignFromDesc(desc, reloadShape);
}

void SphereShape::AssignToDesc(NxSphereShapeDesc& desc)
{
	desc.radius = _radius;
	
	_MyBase::AssignToDesc(desc);
}

NxSphereShape* SphereShape::GetNxShape()
{
	return static_cast<NxSphereShape*>(_MyBase::GetNxShape());
}

float SphereShape::GetRadius() const
{
	return _radius;
}

void SphereShape::SetRadius(float value)
{
	_radius = value;

	if (GetNxShape())
		GetNxShape()->setRadius(value);		
}




CapsuleShape::CapsuleShape(Shapes* owner): _MyBase(owner), _radius(1.0f), _height(1.0f), _capsuleFlags(0)
{
	SetType(Type);
}

NxShapeDesc* CapsuleShape::CreateDesc()
{
	NxCapsuleShapeDesc* desc = new NxCapsuleShapeDesc();
	AssignToDesc(*desc);
	
	return desc;	
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

void CapsuleShape::AssignFromDesc(const NxCapsuleShapeDesc& desc, bool reloadShape)
{
	_radius = desc.radius;
	_height = desc.height;
	_capsuleFlags = desc.flags;

	_MyBase::AssignFromDesc(desc, reloadShape);
}

void CapsuleShape::AssignToDesc(NxCapsuleShapeDesc& desc)
{
	desc.radius = _radius;
	desc.height = _height;
	desc.flags = _capsuleFlags;

	_MyBase::AssignToDesc(desc);
}

NxCapsuleShape* CapsuleShape::GetNxShape()
{
	return static_cast<NxCapsuleShape*>(_MyBase::GetNxShape());
}

float CapsuleShape::GetRadius() const
{
	return _radius;
}

void CapsuleShape::SetRadius(float value)
{
	_radius = value;

	if (GetNxShape())
		GetNxShape()->setRadius(value);		
}

float CapsuleShape::GetHeight() const
{
	return _height;
}

void CapsuleShape::SetHeight(float value)
{
	_height = value;

	if (GetNxShape())
		GetNxShape()->setHeight(value);
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

NxShapeDesc* TriangleMeshShape::CreateDesc()
{
	NxTriangleMeshShapeDesc* desc = new NxTriangleMeshShapeDesc();
	AssignToDesc(*desc);

	return desc;
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

void TriangleMeshShape::AssignFromDesc(const NxTriangleMeshShapeDesc& desc, bool reloadShape)
{
	_MyBase::AssignFromDesc(desc, reloadShape);
}

void TriangleMeshShape::AssignToDesc(NxTriangleMeshShapeDesc& desc)
{
	_MyBase::AssignToDesc(desc);

	if (!_nxMesh)
		_nxMesh = _mesh ? _mesh->GetOrCreateTri(GetScale() * GetActor()->GetWorldScale(), _meshId) : 0;
	desc.meshData = _nxMesh;
}

NxTriangleMeshShape* TriangleMeshShape::GetNxShape()
{
	return static_cast<NxTriangleMeshShape*>(_MyBase::GetNxShape());
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

NxShapeDesc* ConvexShape::CreateDesc()
{
	NxConvexShapeDesc* desc = new NxConvexShapeDesc();
	AssignToDesc(*desc);

	return desc;
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

void ConvexShape::AssignFromDesc(const NxConvexShapeDesc& desc, bool reloadShape)
{
	_MyBase::AssignFromDesc(desc, reloadShape);
}

void ConvexShape::AssignToDesc(NxConvexShapeDesc& desc)
{
	_MyBase::AssignToDesc(desc);

	if (!_nxMesh)
		_nxMesh = _mesh ? _mesh->GetOrCreateConvex(IdentityVector, _meshId) : 0;
	desc.meshData = _nxMesh;
}

NxConvexShape* ConvexShape::GetNxShape()
{
	return static_cast<NxConvexShape*>(_MyBase::GetNxShape());
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




WheelShape::WheelShape(Shapes* owner): _MyBase(owner), _contactModify(0)
{
	SetType(Type);

	AssignFromDesc(NxWheelShapeDesc(), false);
}

WheelShape::~WheelShape()
{
	SetContactModify(0);
}

NxShapeDesc* WheelShape::CreateDesc()
{
	NxWheelShapeDesc* desc = new NxWheelShapeDesc();
	AssignToDesc(*desc);
	return desc;
}

void WheelShape::SaveTireForceFunction(lsl::SWriter* writer, const NxTireFunctionDesc& func)
{
	writer->WriteValue("asymptoteSlip", func.asymptoteSlip);
	writer->WriteValue("asymptoteValue", func.asymptoteValue);
	writer->WriteValue("extremumSlip", func.extremumSlip);
	writer->WriteValue("extremumValue", func.extremumValue);
	writer->WriteValue("stiffnessFactor", func.stiffnessFactor);
}

void WheelShape::LoadTireForceFunction(lsl::SReader* reader, NxTireFunctionDesc& func)
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

void WheelShape::AssignFromDesc(const NxWheelShapeDesc& desc, bool reloadShape)
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

	_MyBase::AssignFromDesc(desc, reloadShape);
}

void WheelShape::AssignToDesc(NxWheelShapeDesc& desc)
{
	_MyBase::AssignToDesc(desc);

	desc.radius = _radius;
	desc.suspensionTravel = _suspensionTravel;
	desc.suspension = _suspension;
	desc.longitudalTireForceFunction = _longitudalTireForceFunction;
	desc.lateralTireForceFunction = _lateralTireForceFunction;
	desc.inverseWheelMass = _inverseWheelMass;
	desc.wheelFlags = _wheelFlags;
	desc.motorTorque = _motorTorque;
	desc.steerAngle = _steerAngle;
	desc.wheelContactModify = _contactModify;
}

NxWheelShape* WheelShape::GetNxShape()
{
	return static_cast<NxWheelShape*>(_MyBase::GetNxShape());
}

float WheelShape::GetRadius() const
{
	return _radius;
}

void WheelShape::SetRadius(float value)
{
	if (_radius != value)
	{
		_radius = value;
		if (GetNxShape())		
			GetNxShape()->setRadius(value);
	}
}

float WheelShape::GetSuspensionTravel() const
{
	return _suspensionTravel;
}

void WheelShape::SetSuspensionTravel(float value)
{
	if (_suspensionTravel != value)
	{
		_suspensionTravel = value;
		if (GetNxShape())		
			GetNxShape()->setSuspensionTravel(value);
	}
}

const NxSpringDesc& WheelShape::GetSuspension() const
{
	return _suspension;
}

void WheelShape::SetSuspension(const NxSpringDesc& value)
{
	_suspension = value;
	if (GetNxShape())		
		GetNxShape()->setSuspension(value);	
}

const NxTireFunctionDesc& WheelShape::GetLongitudalTireForceFunction() const
{
	return _longitudalTireForceFunction;
}

void WheelShape::SetLongitudalTireForceFunction(const NxTireFunctionDesc& value)
{
	_longitudalTireForceFunction = value;
	if (GetNxShape())		
		GetNxShape()->setLongitudalTireForceFunction(value);	
}

const NxTireFunctionDesc& WheelShape::GetLateralTireForceFunction() const
{
	return _lateralTireForceFunction;
}

void WheelShape::SetLateralTireForceFunction(const NxTireFunctionDesc& value)
{
	_lateralTireForceFunction = value;
	if (GetNxShape())		
		GetNxShape()->setLateralTireForceFunction(value);	
}

float WheelShape::GetInverseWheelMass() const
{
	return _inverseWheelMass;
}

void WheelShape::SetInverseWheelMass(float value)
{
	if (_inverseWheelMass != value)
	{
		_inverseWheelMass = value;
		if (GetNxShape())		
			GetNxShape()->setInverseWheelMass(value);
	}
}

UINT WheelShape::GetWheelFlags() const
{
	return _wheelFlags;
}

void WheelShape::SetWheelFlags(UINT value)
{
	if (_wheelFlags != value)
	{
		_wheelFlags = value;
		if (GetNxShape())		
			GetNxShape()->setWheelFlags(value);
	}
}

float WheelShape::GetMotorTorque() const
{
	return _motorTorque;
}

void WheelShape::SetMotorTorque(float value)
{
	if (_motorTorque != value)
	{
		_motorTorque = value;
		if (GetNxShape())		
			GetNxShape()->setMotorTorque(value);
	}
}

float WheelShape::GetSteerAngle() const
{
	return _steerAngle;
}

void WheelShape::SetSteerAngle(float value)
{
	if (_steerAngle != value)
	{
		_steerAngle = value;
		if (GetNxShape())		
			GetNxShape()->setSteerAngle(value);
	}
}

WheelShape::ContactModify* WheelShape::GetContactModify()
{
	return _contactModify;
}

void WheelShape::SetContactModify(ContactModify* value)
{
	if (ReplaceRef(_contactModify, value))
	{
		_contactModify = value;
		if (GetNxShape())
			GetNxShape()->setUserWheelContactModify(_contactModify);
	}
}




Body::Body(Actor* actor): _actor(actor)
{
}

void Body::Save(lsl::SWriter* writer)
{
	writer->WriteValue("mass", _desc.mass);	

	D3DXVECTOR3 massLocalPose[4];
	for (int i = 0; i < 3; ++i)
		massLocalPose[i] = _desc.massLocalPose.M.getRow(i).get();
	massLocalPose[3] = _desc.massLocalPose.t.get();
	writer->WriteValue("massLocalPose", massLocalPose[0], 12);

	writer->WriteValue("flags", _desc.flags);

	writer->WriteValue("sleepEnergyThreshold", _desc.sleepEnergyThreshold);

	lsl::SWriteValue(writer, "linearVelocity", D3DXVECTOR3(_desc.linearVelocity.get()));
}

void Body::Load(lsl::SReader* reader)
{
	reader->ReadValue("mass", _desc.mass);
	
	D3DXVECTOR3 massLocalPose[4];
	if (reader->ReadValue("massLocalPose", massLocalPose[0], 12))
	{
		for (int i = 0; i < 3; ++i)
			_desc.massLocalPose.M.setRow(i, NxVec3(massLocalPose[i]));
		_desc.massLocalPose.t.set(massLocalPose[3]);
	}

	reader->ReadValue("flags", _desc.flags);

	reader->ReadValue("sleepEnergyThreshold", _desc.sleepEnergyThreshold);

	D3DXVECTOR3 linearVelocity;
	lsl::SReadValue(reader, "linearVelocity", linearVelocity);
	_desc.linearVelocity = NxVec3(linearVelocity);
}

const NxBodyDesc& Body::GetDesc()
{
	return _desc;
}

void Body::SetDesc(const NxBodyDesc& value)
{
	_desc = value;

	if (_actor && _actor->GetNxActor())
	{
		_actor->GetNxActor()->setLinearVelocity(value.linearVelocity);
		//
		//...
		//
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

	NxShapeDesc* shapeDesc = shape->CreateDesc();
	D3DXVECTOR3 pos;
	LocalToWorldPos(D3DXVECTOR3(shapeDesc->localPose.t.get()), pos, true);
	shapeDesc->localPose.t.set(pos);

	//not all conditions is completed to create nxShape (neccesary params will be set next, NxTriangleMesh for example)		
	if (shapeDesc->isValid())
		shape->SetNxShape(_nxActor->createShape(*shapeDesc));
	else
		shape->_delayInitialization = true;

	delete shapeDesc;
}

void Actor::DestroyNxShape(Shape* shape)
{
	LSL_ASSERT(_nxActor && shape->_nxShape);

	NxShape* tmp = shape->_nxShape;
	shape->SetNxShape(0);
	_nxActor->releaseShape(*tmp);
}

void Actor::ReloadNxShape(Shape* shape, bool allowInitialization)
{
	if (_nxActor && shape->_nxShape)
	{
		//У фигуры должен быть по крайней мере 1 shape
		NxShape* oldNxShape = shape->_nxShape;
		shape->SetNxShape(0);

		CreateNxShape(shape);

		_nxActor->releaseShape(*oldNxShape);
	}
	else if ((allowInitialization || shape->_delayInitialization) && _nxActor && shape->_nxShape == NULL)
	{
		CreateNxShape(shape);
	}
}

void Actor::FillShapeDescList(_NxShapeDescList& shapeList)
{
	for (Shapes::iterator iter = _shapes->begin(); iter != _shapes->end(); ++iter)
	{
		NxShapeDesc* shapeDesc = (*iter)->CreateDesc();
		shapeList.push_back(shapeDesc);
	}
}

void Actor::FillShapeDescListIncludeChildren(_NxShapeDescList& shapeList)
{
	FillShapeDescList(shapeList);

	for (Children::iterator iter = _children.begin(); iter != _children.end(); ++iter)
		(*iter)->FillShapeDescListIncludeChildren(shapeList);
}

void Actor::UnpackActorShapeList(NxShape*const* begin, NxShape*const* end)
{
	Shapes::iterator pShape = _shapes->begin();
	for (NxShape*const* iter = begin; iter != end; ++iter, ++pShape)	
		(*pShape)->SetNxShape(*iter);
}

unsigned Actor::UnpackActorShapeListIncludeChildren(NxShape*const* shape, unsigned numShapes, unsigned curShape)
{
	unsigned nextInd = curShape;
	if (!GetShapes().Empty())
	{
		LSL_ASSERT(curShape < numShapes);
		
		unsigned endInd = curShape + GetShapes().Size();
		UnpackActorShapeList(&shape[curShape], &shape[endInd]);
		
		nextInd = endInd;
	}
	for (Children::iterator iter = _children.begin(); iter != _children.end(); ++iter)
		nextInd = (*iter)->UnpackActorShapeListIncludeChildren(shape, numShapes, nextInd);
	return nextInd;
}

void Actor::SetNxActorIncludeChildren(NxActor* value)
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
		NxActorDesc actorDesc = _desc;

		FillShapeDescListIncludeChildren(actorDesc.shapes);
		//Пустые физические актеры не инстанцируем
		if (actorDesc.shapes.empty())
			return;

		actorDesc.globalPose.t.set(_pos);
		NxQuat rot;
		rot.setXYZW(_rot);
		actorDesc.globalPose.M.fromQuat(rot);
		actorDesc.body = _body ? &_body->GetDesc() : 0;

		LSL_ASSERT(actorDesc.isValid());
		if (!actorDesc.isValid())
		{
			LSL_LOG("Actor::InitRootNxActor !actorDesc.isValid()");
			return;
		}

		_nxActor = _scene->CreateNxActor(actorDesc, this);

		for (_NxShapeDescList::iterator iter = actorDesc.shapes.begin(); iter != actorDesc.shapes.end(); ++iter)
			delete (*iter);

		if (!_nxActor)
		{
			LSL_LOG("Actor::InitRootNxActor !_nxActor");
			throw lsl::Error("Actor::InitNxActor failed");
		}

		SetNxActorIncludeChildren(_nxActor);
		UnpackActorShapeListIncludeChildren(_nxActor->getShapes(), _nxActor->getNbShapes(), 0);

		//Если установлен такой флаг то центр масс не вычисляется при создании, а значит должен браться из значения указанного в body
		if (_body && GetFlag(NX_AF_LOCK_COM))
		{
			_nxActor->setCMassOffsetLocalPose(_body->GetDesc().massLocalPose);
		}

		if (_owner && _body)
			_owner->OnSetBody(true);
	}
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
	if (_nxActor)
		_nxActor->saveToDesc(_desc);

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

BoxShape& Actor::AddBBShape(const AABB& aabb, const NxBoxShapeDesc& desc)
{
	NxBoxShapeDesc descShape = desc;
	D3DXVECTOR3 sizes = aabb.GetSizes();
	sizes /= 2.0f;
	D3DXVECTOR3 pos = aabb.GetCenter();
	descShape.dimensions.set(sizes);
	descShape.localPose.t.set(pos);
	px::BoxShape& bbShape = GetShapes().Add<px::BoxShape>();
	bbShape.AssignFromDesc(descShape);

	return bbShape;
}

ActorUser* Actor::GetOwner()
{
	return _owner;
}

NxActor* Actor::GetNxActor()
{
	return _nxActor;
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

void Actor::SetBody(const NxBodyDesc* value)
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
		_nxActor->getGlobalPosition().get(_pos);

	return _pos;
}

void Actor::SetPos(const D3DXVECTOR3& value)
{
	_pos = value;
	if (_nxActor)
	{
		if (!_parent)
			_nxActor->setGlobalPosition(NxVec3(value));
		else
			for (Shapes::iterator iter = _shapes->begin(); iter != _shapes->end(); ++iter)
				(*iter)->SyncPos();
	}
}

const D3DXQUATERNION& Actor::GetRot() const
{
	if (!_parent && _nxActor)
		_nxActor->getGlobalOrientationQuat().getXYZW(_rot);

	return _rot;
}

void Actor::SetRot(const D3DXQUATERNION& value)
{
	_rot = value;
	if (_nxActor)
	{
		if (!_parent)
		{
			NxQuat quat;
			quat.setXYZW(value);
			_nxActor->setGlobalOrientationQuat(quat);
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