#include "stdafx.h"
#include "game\DataBase.h"

#include "lslSerialFileXML.h"
#include "game\World.h"

namespace r3d
{

namespace game
{

DataBase::DataBase(World* world, const std::string& name): _world(world), _initMapObjLib(false), _rootNode(0)
{
	SetName(name);
	SetOwner(world);	
}

DataBase::~DataBase()
{	
}

void DataBase::InitMapObjLib()
{
	if (!_initMapObjLib)
	{
		_initMapObjLib = true;

		for (int i = 0; i < MapObjLib::cCategoryEnd; ++i)
		{
			_mapObjLib[i] = new MapObjLib(MapObjLib::Category(i), _rootNode);
			_mapObjLib[i]->SetName(IMapObjLib_cCategoryStr[i]);
			_mapObjLib[i]->SetOwner(this);
			_mapObjLib[i]->SrcSync();
		}
	}
}

void DataBase::FreeMapObjLib()
{
	if (_initMapObjLib)
	{
		_initMapObjLib = false;

		for (int i = 0; i < MapObjLib::cCategoryEnd; ++i)
			delete _mapObjLib[i];
	}	
}

MapObj* DataBase::NewMapObj()
{
	MapObj* res = new MapObj();

	return res;
}

MapObj* DataBase::NewChildMapObj(MapObj* mapObj, MapObjLib::Category category, const std::string& name)
{
	return &mapObj->GetGameObj().GetIncludeList().Add(GetRecord(category, name));
}

void DataBase::SaveMapObj(MapObj* mapObj, MapObjLib::Category category, const std::string& name)
{
	LSL_ASSERT(GetMapObjLib(category)->FindRecord(name) == 0);

	GetMapObjLib(category)->GetOrCreateRecord(name)->Save(mapObj);
	delete mapObj;
}

graph::IVBMeshNode* DataBase::AddMeshNode(graph::SceneNode* scNode, const std::string& mesh, int meshId)
{
	graph::IVBMeshNode* node = &scNode->GetNodes().Add<graph::IVBMeshNode>();
	ComplexMesh* actorMesh = &_world->GetResManager()->GetMeshLib().Get(mesh);
	node->SetMesh(actorMesh->GetOrCreateIVBMesh());
	node->SetMeshId(meshId);

	return node;
}

graph::IVBMeshNode* DataBase::AddMeshNode(MapObj* mapObj, const std::string& mesh, int meshId)
{
	return AddMeshNode(&mapObj->GetGameObj().GetGrActor(), mesh, meshId);
}

graph::Sprite* DataBase::AddSprite(game::MapObj* mapObj, bool fixDir, const D3DXVECTOR2& sizes)
{
	graph::Sprite* node = &mapObj->GetGameObj().GetGrActor().GetNodes().Add<graph::Sprite>();
	node->fixDirection = fixDir;
	node->sizes = sizes;

	return node;
}

graph::Sprite* DataBase::AddFxSprite(game::MapObj* mapObj, const std::string& libMat, const Vec3Range& speedPos, const Vec3Range& speedScale, const QuatRange& speedRot, bool autoRot, graph::SceneNode::AnimMode animMode, float animDuration, float frame, bool dir, const D3DXVECTOR2& sizes)
{
	graph::Sprite* node = &mapObj->GetGameObj().GetGrActor().GetNodes().Add<graph::Sprite>();
	node->fixDirection = dir;
	node->sizes = sizes;
	node->animMode(animMode);
	node->animDuration = animDuration;
	node->frame = frame;

	node->SetPos(speedPos.GetMin());
	node->speedPos = speedPos.GetMax();
	node->SetRot(speedRot.GetMin());
	node->speedRot = speedRot.GetMax();
	node->SetScale(speedScale.GetMin());
	node->speedScale = speedScale.GetMax();

	if (!libMat.empty())
		AddLibMat(&node->material, libMat);

	return node;
}

graph::PlaneNode* DataBase::AddPlaneNode(game::MapObj* mapObj, const D3DXVECTOR2& sizes)
{
	graph::PlaneNode* node = &mapObj->GetGameObj().GetGrActor().GetNodes().Add<graph::PlaneNode>();
	node->SetSize(sizes);

	return node;
}

graph::PlaneNode* DataBase::AddFxPlane(game::MapObj* mapObj, const std::string& libMat, const Vec3Range& speedPos, const Vec3Range& speedScale, const QuatRange& speedRot, graph::SceneNode::AnimMode animMode, float animDuration, float frame, const D3DXVECTOR2& sizes)
{
	graph::PlaneNode* node = &mapObj->GetGameObj().GetGrActor().GetNodes().Add<graph::PlaneNode>();
	node->SetSize(sizes);
	node->animMode(animMode);
	node->animDuration = animDuration;
	node->frame = frame;

	node->SetPos(speedPos.GetMin());
	node->speedPos = speedPos.GetMax();
	node->SetRot(speedRot.GetMin());
	node->speedRot = speedRot.GetMax();
	node->SetScale(speedScale.GetMin());
	node->speedScale = speedScale.GetMax();

	if (!libMat.empty())
		AddLibMat(&node->material, libMat);

	return node;
}

graph::LibMaterial* DataBase::AddLibMat(graph::MaterialNode* node, const std::string& libMat)
{
	graph::LibMaterial* res = &_world->GetResManager()->GetMatLib().Get(libMat);
	node->Set(res);

	return res;
}

graph::LibMaterial* DataBase::AddLibMat(graph::IVBMeshNode* node, const std::string& libMat)
{
	graph::LibMaterial* res = &_world->GetResManager()->GetMatLib().Get(libMat);
	node->material.Insert(res);

	return res;
}

void DataBase::AddToGraph(graph::Actor& grActor, GraphType type, bool dynamic, bool morph, bool disableShadows, bool cullOpacity)
{
	using namespace graph;

	graph::Actor::GraphDesc desc;

	switch (type)
	{
	case gtDefFixPipe:
		desc.lighting = IActor::glStd;
		desc.order = IActor::goDefault;
		desc.props.set(IActor::gpColor);
		desc.props.set(IActor::gpReflWater);
		desc.props.set(IActor::gpReflScene);

		if (!disableShadows)
			desc.props.set(IActor::gpShadowCast);
		break;

	case gtDef:
		desc.lighting = IActor::glPix;
		desc.order = IActor::goDefault;
		desc.props.set(IActor::gpColor);
		desc.props.set(IActor::gpReflWater);
		desc.props.set(IActor::gpReflScene);

		if (!disableShadows)
		{
			desc.props.set(IActor::gpShadowCast);
			desc.props.set(IActor::gpShadowApp);
		}
		break;

	case gtRefl:
		desc.lighting = IActor::glRefl;
		desc.order = IActor::goDefault;
		desc.props.set(IActor::gpColor);
		desc.props.set(IActor::gpReflWater);
		desc.props.set(IActor::gpReflScene);

		if (!disableShadows)
		{
			desc.props.set(IActor::gpShadowCast);
			desc.props.set(IActor::gpShadowApp);
		}
		break;

	case gtBumb:
		desc.lighting = IActor::glBump;
		desc.order = IActor::goDefault;
		desc.props.set(IActor::gpColor);
		desc.props.set(IActor::gpReflWater);
		desc.props.set(IActor::gpReflScene);

		if (!disableShadows)
		{
			desc.props.set(IActor::gpShadowCast);
			desc.props.set(IActor::gpShadowApp);
		}
		break;

	case gtPlanarRefl:
		desc.lighting = IActor::glPlanarRefl;
		desc.order = IActor::goDefault;
		desc.props.set(IActor::gpColor);
		desc.props.set(IActor::gpReflWater);
		desc.props.set(IActor::gpReflScene);

		if (!disableShadows)
		{
			desc.props.set(IActor::gpShadowCast);
			desc.props.set(IActor::gpShadowApp);
		}
		break;

	case gtEffect:
		LSL_ASSERT(dynamic);

		desc.lighting = IActor::glStd;
		desc.order = IActor::goEffect;
		desc.props.set(IActor::gpColor);
		break;

	case gtRefrEffect:	
		LSL_ASSERT(dynamic);

		desc.lighting = IActor::glRefr;
		desc.order = IActor::goDefault;
		desc.props.set(IActor::gpColor);
	}

	if (dynamic)
	{
		desc.props.set(IActor::gpDynamic);
	}

	if (morph)
	{
		desc.props.set(IActor::gpMorph);
	}

	if (cullOpacity)
	{
		desc.props.set(IActor::gpCullOpacity);
	}

	grActor.SetGraph(_world->GetGraph(), desc);
}

void DataBase::AddToGraph(MapObj* mapObj, GraphType type, bool dynamic, bool morph, bool disableShadows, bool cullOpacity)
{
	AddToGraph(mapObj->GetGameObj().GetGrActor(), type, dynamic, morph, disableShadows, cullOpacity);
}

px::BoxShape* DataBase::AddPxBox(MapObj* mapObj, const AABB& aabb)
{
	px::BoxShape* res = &mapObj->GetGameObj().GetPxActor().AddBBShape(aabb);
	mapObj->GetGameObj().GetPxActor().SetScene(_world->GetPxScene());

	return res;
}

px::BoxShape* DataBase::AddPxBox(MapObj* mapObj)
{
	AABB aabb = mapObj->GetGameObj().GetGrActor().GetLocalAABB(false);
	
	return AddPxBox(mapObj, aabb);
}

px::CapsuleShape* DataBase::AddPxCapsule(MapObj* mapObj, float radius, float height, const D3DXVECTOR3& dir, const D3DXVECTOR3& up)
{
	D3DXVECTOR3 right;
	D3DXVec3Cross(&right, &dir, &up);

	NxCapsuleShapeDesc desc;
	desc.radius = radius;
	desc.height = height;

	desc.localPose.M.setColumn(0, NxVec3(right));
	desc.localPose.M.setColumn(1, NxVec3(dir));
	desc.localPose.M.setColumn(2, NxVec3(up));

	px::CapsuleShape& shape = mapObj->GetGameObj().GetPxActor().GetShapes().Add<px::CapsuleShape>();
	shape.AssignFromDesc(desc);

	mapObj->GetGameObj().GetPxActor().SetScene(_world->GetPxScene());

	return &shape;
}

px::CapsuleShape* DataBase::AddPxCapsule(MapObj* mapObj)
{
	AABB aabb = mapObj->GetGameObj().GetGrActor().GetLocalAABB(false);

	return AddPxCapsule(mapObj, aabb.GetSizes().y/2.0f, aabb.GetSizes().x, D3DXVECTOR3(1, 0, 0), D3DXVECTOR3(0, 0, 1));
}

px::SphereShape* DataBase::AddPxSpere(MapObj* mapObj, float radius)
{
	px::SphereShape& shape = mapObj->GetGameObj().GetPxActor().GetShapes().Add<px::SphereShape>();
	shape.SetRadius(radius);

	mapObj->GetGameObj().GetPxActor().SetScene(_world->GetPxScene());

	return &shape;
}

px::TriangleMeshShape* DataBase::AddPxMesh(MapObj* mapObj, const std::string& meshName, int meshId )
{
	ComplexMesh& mesh = _world->GetResManager()->GetMeshLib().Get(meshName);
	px::TriangleMeshShape& triShape = mapObj->GetGameObj().GetPxActor().GetShapes().Add<px::TriangleMeshShape>();
	triShape.SetMesh(mesh.GetOrCreatePxMesh(), meshId);

	mapObj->GetGameObj().GetPxActor().SetScene(_world->GetPxScene());

	return &triShape;
}

px::ConvexShape* DataBase::AddPxConvex(MapObj* mapObj, const std::string& meshName, int meshId)
{
	ComplexMesh& mesh = _world->GetResManager()->GetMeshLib().Get(meshName);
	px::ConvexShape& triShape = mapObj->GetGameObj().GetPxActor().GetShapes().Add<px::ConvexShape>();
	triShape.SetMesh(mesh.GetOrCreatePxMesh(), meshId);

	mapObj->GetGameObj().GetPxActor().SetScene(_world->GetPxScene());

	return &triShape;
}

px::Body* DataBase::AddPxBody(MapObj* mapObj, const NxBodyDesc& desc)
{
	mapObj->GetGameObj().GetPxActor().SetBody(&desc);

	return mapObj->GetGameObj().GetPxActor().GetBody();
}

px::Body* DataBase::AddPxBody(MapObj* mapObj, float mass, const D3DXVECTOR3* massPos)
{
	NxBodyDesc body;
	body.mass = mass;
	body.sleepEnergyThreshold = 0.05f;

	if (massPos)
	{
		body.massLocalPose.t.set(NxVec3(*massPos));
		mapObj->GetGameObj().GetPxActor().SetFlag(NX_AF_LOCK_COM);
	}

	return AddPxBody(mapObj, body);
}

CarWheel* DataBase::AddWheel(unsigned index, GameCar& car, const std::string& meshName, const std::string& matName, const D3DXVECTOR3& pos, bool steer, bool lead, CarWheel* master, const CarDesc& carDesc)
{
	game::CarWheel* wheel = &car.GetWheels().Add();
	wheel->SetLead(lead);
	wheel->SetSteer(steer);
	wheel->SetPos(pos);
	//wheel->invertWheel = pos.y < 0;	

	float meshRadius = 0.0f;
	if (!meshName.empty())
	{
		graph::IVBMeshNode* meshNode = AddMeshNode(&wheel->GetGrActor(), meshName);
		graph::LibMaterial* meshMat = AddLibMat(meshNode, !carDesc.wheelMat.empty() ? carDesc.wheelMat : matName);
		if (carDesc.wheelOffsetModel.size() > 0)
			wheel->SetOffset(index < carDesc.wheelOffsetModel.size() ? carDesc.wheelOffsetModel[index] : carDesc.wheelOffsetModel[carDesc.wheelOffsetModel.size() - 1]);		

		if (pos.y < 0)
		{
			meshNode->SetScale(D3DXVECTOR3(carDesc.wheelScaleModel.x, -carDesc.wheelScaleModel.y, carDesc.wheelScaleModel.z));
			meshNode->material.SetCullMode(D3DCULL_CCW);
		}
		else
		{
			meshNode->SetScale(carDesc.wheelScaleModel);
		}

		meshRadius = (meshNode->GetLocalAABB(false).max.x - meshNode->GetLocalAABB(false).min.x) / 2.0f;

		AddToGraph(wheel->GetGrActor(), carDesc.bump ? gtBumb : gtDef, true);
	}

	if (!carDesc.wheelMesh2.empty())
	{
		graph::IVBMeshNode* meshNode = AddMeshNode(&wheel->GetGrActor(), carDesc.wheelMesh2);
		AddLibMat(meshNode, "Car\\blend");
	}

	NxWheelShapeDesc descShapeWheel;
	//начальная инициализация, например группы
	wheel->GetShape()->AssignToDesc(descShapeWheel);

	descShapeWheel.inverseWheelMass = carDesc.inverseWheelMass;
	descShapeWheel.radius = carDesc.wheelRadius != 0.0f ? carDesc.wheelRadius : meshRadius;
	descShapeWheel.suspensionTravel = carDesc.suspensionTravel;
	descShapeWheel.suspension.spring = carDesc.suspensionSpring;
	descShapeWheel.suspension.damper = carDesc.suspensionDamper;

	descShapeWheel.wheelFlags = NX_WF_CLAMPED_FRICTION;

	NxQuat q;
	q.fromAngleAxis(90, NxVec3(1,0,0));
	NxQuat q2;
	q2.fromAngleAxis(90.0f, NxVec3(0,0,1));
	q = q2 * q;
	descShapeWheel.localPose.M.fromQuat(q);

	wheel->GetShape()->AssignFromDesc(descShapeWheel);
	
	//if (master)
	//{
	//	wheel->GetPxActor().SetMasterSer(&master->GetPxActor());
	//	wheel->GetGrActor().SetMasterSer(&master->GetGrActor());
	//}

	return wheel;
}

graph::FxParticleSystem* DataBase::AddFxSystem(graph::SceneNode* node, graph::FxManager* manager, graph::FxParticleSystem::ChildStyle childStyle)
{
	graph::FxParticleSystem* res = &node->GetNodes().Add<graph::FxParticleSystem>();
	res->SetFxManager(manager);
	res->SetChildStyle(childStyle);

	return res;
}

graph::FxParticleSystem* DataBase::AddFxSystem(MapObj* mapObj, graph::FxManager* manager, graph::FxParticleSystem::ChildStyle childStyle)
{
	return AddFxSystem(&mapObj->GetGameObj().GetGrActor(), manager, childStyle);
}

graph::FxFlowEmitter* DataBase::AddFxFlowEmitter(graph::FxParticleSystem* fxSystem, const graph::FxEmitter::ParticleDesc& partDesc, const graph::FxFlowEmitter::FlowDesc& flowDesc, bool worldCoordSys)
{
	graph::FxFlowEmitter* node = &fxSystem->GetEmitters().Add<graph::FxFlowEmitter>();
	node->SetParticleDesc(partDesc);
	node->SetFlowDesc(flowDesc);
	node->SetWorldCoordSys(worldCoordSys);

	return node;
}

void DataBase::LoadDecor(const std::string& name, const std::string& mesh, const std::string& libMat, GraphType graphType, bool px, bool cullOpacity, bool disableShadows)
{
	MapObj* mapObj = NewMapObj();

	graph::IVBMeshNode* meshNode = AddMeshNode(mapObj, mesh);
	AddLibMat(meshNode, libMat);
	AddToGraph(mapObj, graphType, false, false, disableShadows, cullOpacity);

	if (px)
		px::TriangleMeshShape* meshShape = AddPxMesh(mapObj, mesh);

	SaveMapObj(mapObj, MapObjLib::ctDecoration, name);
}

void DataBase::LoadTrack(const std::string& name, const std::string& mesh, const std::string& libMat, bool pxDefGroup, bool pxShotGroup, bool bump, bool planarRefl, const std::string& pxMesh, bool cullOpacity, const D3DXVECTOR4& vec1, const D3DXVECTOR4& vec2, const D3DXVECTOR4& vec3)
{
	//pxDefGroup = false;
	//pxShotGroup = false;
	D3DXVECTOR4 myVec3 = vec3;
	myVec3.x = -1;

	MapObj* mapObj = NewMapObj();
	graph::IVBMeshNode* meshNode = AddMeshNode(mapObj, mesh);
	AddLibMat(meshNode, libMat);

#ifdef _DEBUG
	if (pxMesh == "" && (pxDefGroup || pxShotGroup))
	{
		if (!meshNode->GetMesh()->GetData()->IsInit())
			meshNode->GetMesh()->GetData()->Load();
		LSL_ASSERT(meshNode->GetMesh()->GetSubsetCount() > (pxShotGroup ? (unsigned)1 : 0));
		//meshNode->GetMesh()->GetData()->Free();
	}
#endif

	std::string pxMeshRes = pxMesh != "" ? pxMesh : mesh;
	if (pxDefGroup)
	{
		px::TriangleMeshShape* meshShape = AddPxMesh(mapObj, pxMeshRes, 0);
		meshShape->SetMaterialIndex(_trackMaterial->getMaterialIndex());
		meshShape->SetGroup(px::Scene::cdgTrackPlane);
		myVec3.x = 0;
	}
	else if (!pxShotGroup)
	{
		px::TriangleMeshShape* meshShape = AddPxMesh(mapObj, pxMeshRes, -1);
		meshShape->SetMaterialIndex(_trackMaterial->getMaterialIndex());
		meshShape->SetGroup(px::Scene::cdgTrackPlane);
	}

	if (pxShotGroup)
	{
		px::TriangleMeshShape* meshShape = AddPxMesh(mapObj, pxMeshRes, 1);
		meshShape->SetMaterialIndex(_borderMaterial->getMaterialIndex());
		meshShape->SetGroup(px::Scene::cdgShotTransparency);
	}

	mapObj->GetGameObj().GetGrActor().vec1(vec1);
	mapObj->GetGameObj().GetGrActor().vec2(vec2);
	mapObj->GetGameObj().GetGrActor().vec3(myVec3);

	if (planarRefl)
		AddToGraph(mapObj, gtPlanarRefl, false, false, false, cullOpacity);
	else if (bump)
		AddToGraph(mapObj, gtBumb, false, false, false, cullOpacity);
	else
		AddToGraph(mapObj, gtDef, false, false, false, cullOpacity);

	SaveMapObj(mapObj, MapObjLib::ctTrack, name);
}

void DataBase::LoadCar(const std::string& name, const std::string& mesh, const std::string& wheelMesh, const std::string& libMat, const std::string& wheelCoords, const CarDesc& carDesc)
{
	MapObj* mapObj = NewMapObj();
	RockCar* gameObj = &mapObj->SetGameObj<game::RockCar>();

	graph::IVBMeshNode* meshNode = AddMeshNode(mapObj, mesh, (carDesc.guseniza || carDesc.podushka) ? 0 : -1);
	AddLibMat(meshNode, libMat);
	meshNode->SetPos(carDesc.bodyOffsetModel);
	meshNode->SetScale(carDesc.bodyScaleModel);
	AddToGraph(mapObj, gtRefl, true);

	if (!carDesc.bodyMesh2.empty())
	{
		graph::IVBMeshNode* meshNode = AddMeshNode(mapObj, carDesc.bodyMesh2, (carDesc.guseniza || carDesc.podushka) ? 0 : -1);
		AddLibMat(meshNode, "Car\\blend");
	}

	AddPxBody(mapObj, carDesc.mass, &carDesc.centerMassPos);
	
	AABB aabb = carDesc.bodyAABB;

	if (D3DXVec3Length(&aabb.GetSizes()) < 0.001f)
		aabb = mapObj->GetGameObj().GetGrActor().GetLocalAABB(false);

	aabb.Scale(carDesc.bodyScale);
	aabb.Offset(carDesc.bodyOffset);
	px::BoxShape* carShape = AddPxBox(mapObj, aabb);
	
	carShape->SetMaterialIndex(carDesc.wakeFrictionModel ? _nxCarMaterial2->getMaterialIndex() : _nxCarMaterial1->getMaterialIndex());

	std::vector<D3DXVECTOR3> posWheels;
	CarWheels::LoadPosTo("Data\\" + wheelCoords, posWheels);

	for (unsigned i = 0; i < posWheels.size(); ++i)	
	{
		bool backWheel = i >= 2;
		bool lead = (carDesc.frontWheelDrive && i <= 1) || (carDesc.backWheelDrive && backWheel);
		game::CarWheel* wheel = AddWheel(i, *gameObj, (backWheel && !carDesc.wheelMeshBack.empty()) ? carDesc.wheelMeshBack : wheelMesh, libMat, posWheels[i], i < 2, lead, (backWheel && !carDesc.wheelMeshBack.empty()) ? NULL : (i > 0 ? &gameObj->GetWheels().front() : 0), carDesc);
		if (!carDesc.halfWheelEff || i < 2)
		{
			if (carDesc.wheelEff)
			{
				PxWheelSlipEffect& slipEffect = wheel->GetBehaviors().Add<PxWheelSlipEffect>();
				slipEffect.SetEffect(GetRecord(MapObjLib::ctEffects, "trail"));
				slipEffect.SetPos(ZVector * 0.01f);
				
				//для одного колеса
				if (i == 0)
					slipEffect.SetSound(&GetSound("Sounds\\SkidAsphalt.ogg"));
			}

			{
				PxWheelSlipEffect& smokeEffect = wheel->GetBehaviors().Add<PxWheelSlipEffect>();
				smokeEffect.SetEffect(GetRecord(MapObjLib::ctEffects, "smoke7"));
			}
		}
	}
	gameObj->SetKSteerControl(0.12f);
	gameObj->SetSteerSpeed(carDesc.steerSpeed);
	gameObj->SetSteerRot(carDesc.steerRot);
	gameObj->SetFlyYTourque(carDesc.flyYTorque);	
	gameObj->SetClampXTourque(carDesc.clampXTorque);	
	gameObj->SetClampYTourque(carDesc.clampYTorque);	
	gameObj->SetAngDamping(carDesc.angDamping);
	gameObj->SetGravEngine(carDesc.gravEngine);
	gameObj->SetClutchImmunity(carDesc.clutchImmunity);
	gameObj->SetTireSpring(carDesc.tireSpring);
	gameObj->SetMaxSpeed(carDesc.maxSpeed);

	game::CarMotorDesc motorDesc = gameObj->GetMotorDesc();
	motorDesc.maxRPM = carDesc.maxRPM;
	gameObj->SetMotorDesc(motorDesc);

	{
		LowLifePoints& lowLife = gameObj->GetBehaviors().Add<LowLifePoints>();
		lowLife.SetEffect(GetRecord(MapObjLib::ctEffects, "smoke6"));
		lowLife.SetPos(ZVector * 0.5f);
	}
	{
		DeathEffect& deathEffect1 = gameObj->GetBehaviors().Add<DeathEffect>();
		deathEffect1.SetEffect(GetRecord(MapObjLib::ctEffects, "death2"));		
		deathEffect1.SetIgnoreRot(true);
	}
	if (!carDesc.bodyDestr.empty())
	{
		MapObjRec* record = GetRecord(MapObjLib::ctEffects, carDesc.bodyDestr, false);
		if (record == NULL)
		{
			game::MapObj* mapObj = NewMapObj();
			AddToGraph(mapObj, gtDefFixPipe, true);
			mapObj->GetGameObj().SetPos(D3DXVECTOR3(0, 0, 1.5f));
			mapObj->GetGameObj().SetMaxTimeLife(10);

			graph::IVBMeshNode* node = AddMeshNode(mapObj, carDesc.bodyDestr + ".r3d");
			AddLibMat(node, carDesc.bodyDestr);

			AddPxBox(mapObj);
			AddPxBody(mapObj, 1200.0f, 0);

			NewChildMapObj(mapObj, MapObjLib::ctEffects, "fire1")->GetGameObj().SetPos(D3DXVECTOR3(1.0f, 0.0f, 0.3f));
			NewChildMapObj(mapObj, MapObjLib::ctEffects, "fire1")->GetGameObj().SetPos(D3DXVECTOR3(-1.0f, -0.5f, 0.5f));

			SaveMapObj(mapObj, MapObjLib::ctEffects, carDesc.bodyDestr);
			record = GetRecord(MapObjLib::ctEffects, carDesc.bodyDestr);
		}

		DeathEffect& deathEffect2 = gameObj->GetBehaviors().Add<DeathEffect>();
		deathEffect2.SetEffect(record);
		deathEffect2.SetImpulse(XVector * 20000.0f);
	}
	{
		ImmortalEffect& effect = gameObj->GetBehaviors().Add<ImmortalEffect>();
		effect.SetEffect(GetRecord(MapObjLib::ctEffects, "shield1"));
		effect.SetScaleK(D3DXVECTOR3(1.3f, 1.7f, 1.7f));
	}

	{
		MapObjRec* record = _mapObjLib[MapObjLib::ctEffects]->FindRecord("damageEnergy" + name);
		if (!record)
		{
			game::MapObj* mapObj = NewMapObj();
			mapObj->GetGameObj().SetMaxTimeLife(0.5f);
			mapObj->GetGameObj().SetPos(carDesc.bodyOffsetModel);
			mapObj->GetGameObj().SetScale(carDesc.bodyScaleModel);

			graph::IVBMeshNode* node = AddMeshNode(mapObj, mesh);
			node->SetScale(1.1f);
			AddLibMat(node, "Effect\\shield1");
			node->animMode(graph::SceneNode::amTwoSide);
			node->animDuration = 0.5f;
			AddToGraph(mapObj, gtEffect, true);

			SaveMapObj(mapObj, MapObjLib::ctEffects, "damageEnergy" + name);
			record = GetRecord(MapObjLib::ctEffects, "damageEnergy" + name);
		}

		DamageEffect& damageEff = gameObj->GetBehaviors().Add<DamageEffect>();
		damageEff.SetDamageType(GameObject::dtEnergy);
		damageEff.SetEffect(record);
		//damageEff.AddSound(&GetSound("Sounds\\bullets_hp_01.ogg"));
		//damageEff.AddSound(&GetSound("Sounds\\bullets_hp_02.ogg"));
		//damageEff.AddSound(&GetSound("Sounds\\bullets_hp_03.ogg"));
	}

	{
		SoundMotor& effect = gameObj->GetBehaviors().Add<SoundMotor>();

		effect.SetSndIdle(&GetSound(!carDesc.soundMotorIdle.empty() ? carDesc.soundMotorIdle : "Sounds\\engine_player_heavy_tom.ogg"));
		effect.SetSndRPM(&GetSound(!carDesc.soundMotorHigh.empty() ? carDesc.soundMotorHigh : "Sounds\\Motor_high02.ogg"));

		effect.SetRPMVolumeRange(carDesc.motorVolumeRange);
		effect.SetRPMFreqRange(carDesc.motorFreqRange);
	}

	if (carDesc.guseniza)
	{
		MapObj* childObj = &mapObj->GetGameObj().GetIncludeList().Add(gotGameObj);
		childObj->GetGameObj().SetPos(carDesc.bodyOffsetModel);
		childObj->GetGameObj().SetScale(carDesc.bodyScaleModel);

		graph::IVBMeshNode* meshNode = AddMeshNode(childObj, mesh, 1);
		graph::LibMaterial* libMat = AddLibMat(meshNode, "Car\\gusenizaChain");
		AddToGraph(childObj, gtDefFixPipe, true);

		childObj->GetGameObj().GetBehaviors().Add<GusenizaAnim>();
	}

	if (carDesc.podushka)
	{
		MapObj* childObj = &mapObj->GetGameObj().GetIncludeList().Add(gotGameObj);
		childObj->GetGameObj().SetPos(carDesc.bodyOffsetModel);
		childObj->GetGameObj().SetScale(carDesc.bodyScaleModel);

		for (int i = 0; i < 2; ++i)
		{
			graph::IVBMeshNode* meshNode = AddMeshNode(childObj, mesh, i + 1);			
			meshNode->tag(i + 1);
			graph::LibMaterial* libMat = AddLibMat(meshNode, "Car\\podushka");
			childObj->GetGameObj().GetBehaviors().Add<PodushkaAnim>().targetTag(i + 1);
		}

		AddToGraph(childObj, gtRefl, true);
	}

	//x = 1.0f - bump
	if (carDesc.bump)
		mapObj->GetGameObj().GetGrActor().vec3(D3DXVECTOR4(1.0f, 0.0f, 0.0f, 0.0f));

	SaveMapObj(mapObj, MapObjLib::ctCar, name);
}

void DataBase::LoadCrushObj(const std::string& name, const std::string& mesh, const std::string& libMat, float mass, int subMeshCount, int staticSubMeshes[], int staticCount, bool cullOpacity)
{
	//reklama
	{
		game::MapObj* mapObj = NewMapObj();
		game::DestrObj& gameObj = mapObj->SetGameObj<game::DestrObj>();
		//необходимо для рендера BB
		AddToGraph(mapObj, gtDefFixPipe, true);
		mapObj->GetGameObj().GetPxActor().SetScene(_world->GetPxScene());
		mapObj->GetGameObj().SetMaxLife(0);

		for (int i = 0; i < subMeshCount; ++i)
		{
			game::MapObj& part = gameObj.GetDestrList().Add<game::GameObject>();
			AddToGraph(&part, gtDefFixPipe, true, false, false, cullOpacity);

			graph::IVBMeshNode* node = AddMeshNode(&part, mesh, i);
			AddLibMat(node, libMat);

			bool isStatic = false;
			if (staticSubMeshes)
				for (int j = 0; j < staticCount; ++j)
					if (staticSubMeshes[j] == i)
					{
						isStatic = true;
						break;
					}

			if (isStatic)
				AddPxMesh(&part, mesh, i);
			else
			{
				px::BoxShape* shape = AddPxBox(&part);
				shape->SetSkinWidth(0.1f);

				AddPxBody(&part, mass, 0);
			}			
		}
		SaveMapObj(mapObj, MapObjLib::ctDecoration, name);
	}
}

void DataBase::LoadFxFlow(const std::string& name, const std::string& libMat, graph::FxManager* fxManager, const graph::FxEmitter::ParticleDesc& partDesc, const graph::FxFlowEmitter::FlowDesc& flowDesc, bool worldCoordSys, float timeLife, GraphType graphType)
{
	game::MapObj* mapObj = NewMapObj();
	AddToGraph(mapObj, graphType, true);
	mapObj->GetGameObj().SetMaxTimeLife(timeLife);

	graph::FxParticleSystem* fxSystem = AddFxSystem(mapObj, fxManager);
	if (!libMat.empty())
		AddLibMat(&fxSystem->material, libMat);

	graph::FxFlowEmitter* emitter = AddFxFlowEmitter(fxSystem, partDesc, flowDesc, worldCoordSys);
	SaveMapObj(mapObj, MapObjLib::ctEffects, name);
}

void DataBase::LoadFxSprite(const std::string& name, const std::string& libMat, const Vec3Range& speedPos, const Vec3Range& speedScale, const QuatRange& speedRot, bool autoRot, graph::SceneNode::AnimMode animMode, float animDuration, float frame, float timeLife, bool dir, const D3DXVECTOR2& sizes, GraphType graphType, bool morph)
{
	game::MapObj* mapObj = NewMapObj();
	AddToGraph(mapObj, graphType, true, morph);
	mapObj->GetGameObj().SetMaxTimeLife(timeLife);

	AddFxSprite(mapObj, libMat, speedPos, speedScale, speedRot, autoRot, animMode, animDuration, frame, dir, sizes);

	SaveMapObj(mapObj, MapObjLib::ctEffects, name);
}

void DataBase::LoadFxPlane(const std::string& name, const std::string& libMat, const Vec3Range& speedPos, const Vec3Range& speedScale, const QuatRange& speedRot, graph::SceneNode::AnimMode animMode, float animDuration, float frame, float timeLife, const D3DXVECTOR2& sizes, GraphType graphType)
{
	game::MapObj* mapObj = NewMapObj();
	AddToGraph(mapObj, graphType, true);
	mapObj->GetGameObj().SetMaxTimeLife(timeLife);

	AddFxPlane(mapObj, libMat, speedPos, speedScale, speedRot, animMode, animDuration, frame, sizes);

	SaveMapObj(mapObj, MapObjLib::ctEffects, name);
}

void DataBase::LoadSndSources()
{
	//klicka5
	{
		game::MapObj* mapObj = NewMapObj();
		mapObj->GetGameObj().SetMaxTimeLife(2.0f);
		
		{
			LifeEffect& effect = mapObj->GetGameObj().GetBehaviors().Add<LifeEffect>();
			effect.SetSound(&GetSound("Sounds\\klicka5.ogg"));
		}

		SaveMapObj(mapObj, MapObjLib::ctEffects, "Snd\\klicka5");
	}	

	//shieldOn
	{
		game::MapObj* mapObj = NewMapObj();
		mapObj->GetGameObj().SetMaxTimeLife(2.0f);

		{
			LifeEffect& effect = mapObj->GetGameObj().GetBehaviors().Add<LifeEffect>();
			effect.SetSound(&GetSound("Sounds\\shieldOn.ogg"));
		}

		SaveMapObj(mapObj, MapObjLib::ctEffects, "Snd\\shieldOn");
	}	
}

void DataBase::LoadEffects()
{
	//wheels1
	{
		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 3;
		desc.maxNumAction = graph::FxEmitter::mnaWaitingFree;
		desc.startTime = 0;
		desc.density = 3.0f;
		desc.life = 0;
		desc.startPos = Vec3Range(D3DXVECTOR3(-0.1f, -0.1f, -0.1f), D3DXVECTOR3(0.1f, 0.1f, 0.1f), Vec3Range::vdVolume, Point3U(100, 100, 100));
		desc.startScale = Vec3Range(IdentityVector * 0.9f, IdentityVector * 1.1f);

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedPos = Vec3Range(D3DXVECTOR3(-1.5f, -1.5f, 1.0f), D3DXVECTOR3(1.5f, 1.5f, 5.0f), Vec3Range::vdVolume) * 3.0f;
		D3DXQUATERNION spRot1, spRot2;
		D3DXQuaternionRotationAxis(&spRot1, &(-IdentityVector), D3DX_PI);
		D3DXQuaternionRotationAxis(&spRot2, &IdentityVector, 2.0f * D3DX_PI);
		descFlow.speedRot = QuatRange(spRot1, spRot2, QuatRange::vdVolume, Point2U(100, 100));
		descFlow.gravitation = D3DXVECTOR3(0, 0, -9.80f);

		LoadFxFlow("wheels1", "", _fxWheelManager, desc, descFlow, true);
	}

	//truba1
	{
		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 1;
		desc.maxNumAction = graph::FxEmitter::mnaWaitingFree;
		desc.startTime = 0;
		desc.density = 1.0f;
		desc.life = 0;
		desc.startPos = Vec3Range(D3DXVECTOR3(-0.1f, -0.1f, -0.1f), D3DXVECTOR3(0.1f, 0.1f, 0.1f), Vec3Range::vdVolume, Point3U(100, 100, 100));
		desc.startScale = Vec3Range(IdentityVector * 0.9f, IdentityVector * 1.1f);		

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedPos = Vec3Range(D3DXVECTOR3(-1.5f, -1.5f, 4.0f), D3DXVECTOR3(1.5f, 1.5f, 5.0f), Vec3Range::vdVolume) * 3.0f;
		D3DXQUATERNION spRot1, spRot2;
		D3DXQuaternionRotationAxis(&spRot1, &(-IdentityVector), D3DX_PI);
		D3DXQuaternionRotationAxis(&spRot2, &IdentityVector, 2.0f * D3DX_PI);
		descFlow.speedRot = QuatRange(spRot1, spRot2, QuatRange::vdVolume, Point2U(100, 100));
		descFlow.gravitation = D3DXVECTOR3(0, 0, -9.80f);

		LoadFxFlow("truba1", "", _fxTrubaManager, desc, descFlow, true);
	}

	//spark1
	{
		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 30;
		desc.maxNumAction = graph::FxEmitter::mnaWaitingFree;
		desc.startTime = 0;
		desc.density = 30;
		desc.life = 1.0f;
		desc.startPos = Vec3Range(D3DXVECTOR3(-0.1f, -0.1f, -0.1f), D3DXVECTOR3(0.1f, 0.1f, 0.1f), Vec3Range::vdVolume);
		desc.startScale = Vec3Range(D3DXVECTOR3(1.0f, 0.70f, 0.70f), D3DXVECTOR3(1.5f, 1.0f, 1.0f));
				
		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedPos = Vec3Range(D3DXVECTOR3(-12.0f, -12.0f, 0.0f), D3DXVECTOR3(12.0f, 12.0f, 12.0f), Vec3Range::vdVolume);
		descFlow.autoRot = true;
		descFlow.gravitation = D3DXVECTOR3(0, 0, -5.0f);
		
		LoadFxFlow("spark1", "Effect\\spark1", _fxDirSpriteManager, desc, descFlow, true, 1.0f);
	}

	//spark2
	{
		game::MapObj* mapObj = NewMapObj();
		AddToGraph(mapObj, gtEffect, true);

		graph::FxParticleSystem* fxSystem = AddFxSystem(mapObj, _fxDirSpriteManager);
		AddLibMat(&fxSystem->material, "Effect\\spark1");

		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 0;
		desc.maxNumAction = graph::FxEmitter::mnaWaitingFree;
		desc.startTime = 0.1f;
		desc.density = FloatRange(7.0f, 10.0f);
		desc.life = FloatRange(0.3f, 0.7f);
		desc.startPos = Vec3Range(D3DXVECTOR3(-0.1f, -0.1f, -0.1f), D3DXVECTOR3(0.1f, 0.1f, 0.1f), Vec3Range::vdVolume);
		desc.startScale = Vec3Range(D3DXVECTOR3(0.2f, 0.40f, 0.40f), D3DXVECTOR3(0.9f, 0.5f, 0.5f));
				
		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedPos = Vec3Range(D3DXVECTOR3(-6.0f, -6.0f, -6.0f), D3DXVECTOR3(6.0f, 6.0f, 6.0f), Vec3Range::vdVolume);
		descFlow.autoRot = true;
		descFlow.gravitation = D3DXVECTOR3(0, 0, -2.0f);

		mapObj->GetGameObj().GetBehaviors().Add<FxSystemWaitingEnd>();
		graph::FxFlowEmitter* emitter = AddFxFlowEmitter(fxSystem, desc, descFlow, true);
		SaveMapObj(mapObj, MapObjLib::ctEffects, "spark2");
	}

	//spark3
	{
		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 30;
		desc.maxNumAction = graph::FxEmitter::mnaWaitingFree;
		desc.startTime = 0;
		desc.density = 30;
		desc.life = 0.7f;
		desc.startPos = Vec3Range(D3DXVECTOR3(-0.1f, -0.1f, -0.1f), D3DXVECTOR3(0.1f, 0.1f, 0.1f), Vec3Range::vdVolume);
		desc.startScale = Vec3Range(D3DXVECTOR3(0.5f, 0.4f, 0.4f), D3DXVECTOR3(0.6f, 0.5f, 0.5f));

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedPos = Vec3Range(D3DXVECTOR3(-12.0f, -12.0f, 0.0f), D3DXVECTOR3(12.0f, 12.0f, 12.0f), Vec3Range::vdVolume);
		descFlow.autoRot = true;
		descFlow.gravitation = D3DXVECTOR3(0, 0, -5.0f);

		LoadFxFlow("spark3", "Effect\\spark1", _fxDirSpriteManager, desc, descFlow, true, 0.7f);
	}

	//streak1
	{
		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 15;
		desc.maxNumAction = graph::FxEmitter::mnaWaitingFree;
		desc.startTime = 0;
		desc.density = 15;
		desc.life = 0.7f;
		desc.startPos = Vec3Range(D3DXVECTOR3(-0.1f, -0.1f, -0.1f), D3DXVECTOR3(0.1f, 0.1f, 0.1f), Vec3Range::vdVolume);
		desc.startScale = Vec3Range(D3DXVECTOR3(1.3f, 0.4f, 0.4f), D3DXVECTOR3(1.6f, 0.4f, 0.4f));

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedPos = Vec3Range(D3DXVECTOR3(-10.0f, -10.0f, -10.0f), D3DXVECTOR3(10.0f, 10.0f, 10.0f), Vec3Range::vdVolume);
		descFlow.speedScale = Vec3Range(D3DXVECTOR3(7.5f, 0.0f, 0.0f), D3DXVECTOR3(10.5f, 0.0f, 0.0f));
		descFlow.autoRot = true;

		LoadFxFlow("streak1", "Effect\\streak1", _fxDirSpriteManager, desc, descFlow, true, 0.7f);
	}

	//laserSparkBase
	{
		game::MapObj* mapObj = NewMapObj();
		AddToGraph(mapObj, gtEffect, true);

		graph::FxParticleSystem* fxSystem = AddFxSystem(mapObj, _fxDirSpriteManager);
		AddLibMat(&fxSystem->material, "Effect\\flareLaser2");

		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 0;
		desc.maxNumAction = graph::FxEmitter::mnaWaitingFree;
		desc.startTime = 0.1f;
		desc.density = FloatRange(7.0f, 10.0f);
		desc.life = FloatRange(0.3f, 0.7f);
		desc.startPos = Vec3Range(D3DXVECTOR3(-0.1f, -0.1f, -0.1f), D3DXVECTOR3(0.1f, 0.1f, 0.1f), Vec3Range::vdVolume);
		desc.startScale = Vec3Range(D3DXVECTOR3(0.4f, 0.6f, 0.6f), D3DXVECTOR3(1.1f, 0.7f, 0.7f));

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedPos = Vec3Range(D3DXVECTOR3(-6.0f, -6.0f, -6.0f), D3DXVECTOR3(6.0f, 6.0f, 6.0f), Vec3Range::vdVolume);
		descFlow.autoRot = true;
		descFlow.gravitation = D3DXVECTOR3(0, 0, -2.0f);

		mapObj->GetGameObj().GetBehaviors().Add<FxSystemWaitingEnd>();
		graph::FxFlowEmitter* emitter = AddFxFlowEmitter(fxSystem, desc, descFlow, true);
		SaveMapObj(mapObj, MapObjLib::ctEffects, "laserSparkBase");
	}

	//blink
	{
		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 15;
		desc.maxNumAction = graph::FxEmitter::mnaWaitingFree;
		desc.startTime = 0;
		desc.density = 15;
		desc.life = 0.5f;
		desc.startPos = Vec3Range(D3DXVECTOR3(-0.1f, -0.1f, -0.1f), D3DXVECTOR3(0.1f, 0.1f, 0.1f), Vec3Range::vdVolume);
		desc.startScale = Vec3Range(D3DXVECTOR3(0.5f, 0.5f, 0.5f), D3DXVECTOR3(0.7f, 0.7f, 0.7f));

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedPos = Vec3Range(D3DXVECTOR3(-10.0f, -10.0f, -10.0f), D3DXVECTOR3(10.0f, 10.0f, 10.0f), Vec3Range::vdVolume);
		//descFlow.speedScale = Vec3Range(D3DXVECTOR3(13.5f, 0.0f, 0.0f), D3DXVECTOR3(16.5f, 0.0f, 0.0f));
		descFlow.autoRot = true;		

		LoadFxFlow("blink", "Effect\\blink", _fxSpriteManager, desc, descFlow, true, 0.5f);
	}

	//lightning1
	{
		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 1;
		desc.maxNumAction = graph::FxEmitter::mnaWaitingFree;
		desc.startTime = 0;
		desc.density = 1;
		desc.life = 0.25f;
		desc.startPos = Vec3Range(D3DXVECTOR3(-0.1f, -0.1f, -0.1f), D3DXVECTOR3(0.1f, 0.1f, 0.1f), Vec3Range::vdVolume);
		desc.startScale = Vec3Range(D3DXVECTOR3(0.8f, 0.8f, 0.8f), D3DXVECTOR3(1.1f, 1.1f, 1.1f));

		D3DXQUATERNION rot1;
		D3DXQuaternionRotationAxis(&rot1, &D3DXVECTOR3(0.0f, 0.0f, 1.0f), 0.0f);
		D3DXQUATERNION rot2;
		D3DXQuaternionRotationAxis(&rot2, &D3DXVECTOR3(0.0f, 0.0f, 1.0f), 2*D3DX_PI);		
		desc.startRot = QuatRange(rot1, rot2, QuatRange::vdVolume);

		graph::FxFlowEmitter::FlowDesc descFlow;

		LoadFxFlow("lightning1", "Effect\\lightning1", _fxSpriteManager, desc, descFlow, false);
	}

	//smoke1
	{
		graph::FxEmitter::ParticleDesc desc;
		desc.life = FloatRange(2.0f, 3.0f);
		desc.startType = graph::FxEmitter::sotDist;
		desc.startTime = FloatRange(1.0f, 1.1f);
		desc.density = FloatRange(1.0f, 3.0f);
		desc.startPos = Vec3Range(D3DXVECTOR3(-0.1f, -0.1f, -0.1f), D3DXVECTOR3(0.1f, 0.1f, 0.1f), Vec3Range::vdVolume);
		desc.startScale = D3DXVECTOR3(IdentityVector) / 2;
		
		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedScale = D3DXVECTOR3(1.0f, 1.0f, 1.0f);
		descFlow.speedPos = Vec3Range(D3DXVECTOR3(-0.5f, -0.5f, 2.0f), D3DXVECTOR3(0.5f, 0.5f, 2.0f), Vec3Range::vdVolume, Point3U(100, 100, 100));
		
		LoadFxFlow("smoke1", "Effect\\smoke1", _fxSpriteManager, desc, descFlow, true);
	}

	//smoke2
	{
		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 0;
		desc.life = FloatRange(1.0f, 2.0f);
		desc.startTime = FloatRange(1.0f, 1.1f);
		desc.startType = graph::FxEmitter::sotDist;
		desc.density = 1.0f;
		desc.startPos = Vec3Range(IdentityVector * (-0.1f), IdentityVector * 0.1f, Vec3Range::vdVolume);
		desc.startScale = Vec3Range(IdentityVector * 0.7f, IdentityVector * 1.3f, Vec3Range::vdVolume);
		desc.startRot = QuatRange(D3DXQUATERNION(-1, -1, -1, -1), D3DXQUATERNION(1, 1, 1, 1), QuatRange::vdVolume);

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedScale = IdentityVector * 2.0f;
		descFlow.speedPos = -XVector;
		
		LoadFxFlow("smoke2", "Effect\\smoke2", _fxSpriteManager, desc, descFlow, true);
	}

	//smoke3
	{
		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 10;
		desc.life = 1.0f;
		desc.startTime = 0;
		desc.startType = graph::FxEmitter::sotTime;
		desc.density = 10.0f;
		desc.startPos = Vec3Range(IdentityVector * (-0.6f), IdentityVector * 0.6f, Vec3Range::vdVolume);
		desc.startScale = Vec3Range(IdentityVector * 0.7f, IdentityVector * 1.1f, Vec3Range::vdVolume);
		desc.startRot = QuatRange(D3DXQUATERNION(-1, -1, -1, -1), D3DXQUATERNION(1, 1, 1, 1), QuatRange::vdVolume);

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedScale = IdentityVector * 3.0f;
		descFlow.speedPos = Vec3Range(D3DXVECTOR3(15.0f, -2.0f, -2.0f), D3DXVECTOR3(19.0f, 2.0f, 2.0f), Vec3Range::vdVolume);
		descFlow.acceleration = D3DXVECTOR3(-20.0f, 0.0f, 0.0f);
		descFlow.gravitation = D3DXVECTOR3(0, 0, 1.0f);

		LoadFxFlow("smoke3", "Effect\\smoke1", _fxSpriteManager, desc, descFlow, true, 1.0f);
	}

	//smoke4
	{
		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 0;
		desc.life = FloatRange(1.0f, 2.0f);
		desc.startTime = FloatRange(1.0f, 1.1f);
		desc.startType = graph::FxEmitter::sotDist;
		desc.density = 1.0f;
		desc.startPos = Vec3Range(IdentityVector * (-0.1f), IdentityVector * 0.1f, Vec3Range::vdVolume);
		desc.startScale = Vec3Range(IdentityVector * 0.5f, IdentityVector * 0.7f, Vec3Range::vdVolume);
		desc.startRot = QuatRange(D3DXQUATERNION(-1, -1, -1, -1), D3DXQUATERNION(1, 1, 1, 1), QuatRange::vdVolume);

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedScale = IdentityVector * 1.0f;
		descFlow.speedPos = -XVector;

		LoadFxFlow("smoke4", "Effect\\smoke3", _fxSpriteManager, desc, descFlow, true);
	}

	//smoke5
	{
		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 0;
		desc.life = FloatRange(0.5f, 0.5f);
		desc.startTime = FloatRange(0.5f, 0.5f);
		desc.startType = graph::FxEmitter::sotDist;
		desc.density = 1.0f;
		desc.startPos = NullVector;
		desc.startScale = IdentityVector * 0.7f;
		desc.startRot = QuatRange(D3DXQUATERNION(-1, -1, -1, -1), D3DXQUATERNION(1, 1, 1, 1), QuatRange::vdVolume);

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedScale = IdentityVector * 4.0f;
		descFlow.speedPos = NullVector;

		LoadFxFlow("smoke5", "Effect\\thunder1", _fxSpriteManager, desc, descFlow, true);
	}

	//smoke6
	{
		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 0;
		desc.startType = graph::FxEmitter::sotDist;
		desc.life = FloatRange(0.2f, 0.3f);
		desc.startTime = FloatRange(1.0f);
		desc.density = FloatRange(1.0f, 1.0f);
		desc.startPos = Vec3Range(D3DXVECTOR3(-0.2f, -0.2f, -0.2f), D3DXVECTOR3(0.2f, 0.2f, 0.2f), Vec3Range::vdVolume);
		desc.startRot = QuatRange(D3DXQUATERNION(-1, -1, -1, -1), D3DXQUATERNION(1, 1, 1, 1), QuatRange::vdVolume);
		desc.startScale = D3DXVECTOR3(IdentityVector) * 1.5f;

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedScale = IdentityVector * (-2.0f);
		descFlow.speedPos = Vec3Range(D3DXVECTOR3(-1.0f, -0.5f, 5.0f), D3DXVECTOR3(-1.3f, 0.5f, 6.0f), Vec3Range::vdVolume);

		LoadFxFlow("smoke6", "Effect\\smoke6", _fxSpriteManager, desc, descFlow, true);
	}

	//smoke7
	{
		game::MapObj* mapObj = NewMapObj();
		AddToGraph(mapObj, gtEffect, true);

		graph::FxParticleSystem* fxSystem = AddFxSystem(mapObj, _fxSpriteManager);
		AddLibMat(&fxSystem->material, "Effect\\smoke7");

		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 100;
		desc.startType = graph::FxEmitter::sotDist;
		desc.life = FloatRange(0.5f, 0.6f);
		desc.startTime = FloatRange(1.0f);
		desc.density = 1.0f;
		desc.startPos = Vec3Range(D3DXVECTOR3(-0.2f, -0.2f, -0.2f), D3DXVECTOR3(0.2f, 0.2f, 0.2f), Vec3Range::vdVolume);
		desc.startRot = QuatRange(D3DXQUATERNION(-1, -1, -1, -1), D3DXQUATERNION(1, 1, 1, 1), QuatRange::vdVolume);
		desc.startScale = D3DXVECTOR3(IdentityVector) * 1.5f;

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedScale = IdentityVector * (3.0f);
		descFlow.speedPos = Vec3Range(D3DXVECTOR3(0.0f, 0.0f, 0.8f), D3DXVECTOR3(0.0f, 0.0f, 1.0f));

		mapObj->GetGameObj().GetBehaviors().Add<FxSystemWaitingEnd>();
		graph::FxFlowEmitter* emitter = AddFxFlowEmitter(fxSystem, desc, descFlow, true);
		SaveMapObj(mapObj, MapObjLib::ctEffects, "smoke7");
	}

	//smoke8
	{
		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 0;
		desc.life = FloatRange(0.4f, 0.4f);
		desc.startTime = FloatRange(0.75f, 0.75f);
		desc.startType = graph::FxEmitter::sotDist;
		desc.density = 1.0f;
		desc.startPos = NullVector;
		desc.startScale = IdentityVector * 0.7f;
		desc.startRot = QuatRange(D3DXQUATERNION(-1, -1, -1, -1), D3DXQUATERNION(1, 1, 1, 1), QuatRange::vdVolume);

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedScale = IdentityVector * 4.0f;
		descFlow.speedPos = NullVector;

		LoadFxFlow("smoke8", "Effect\\sonar", _fxSpriteManager, desc, descFlow, true);
	}

	//accelEff
	{
		graph::FxEmitter::ParticleDesc desc;
		desc.life = 0.5f;
		desc.startTime = 0.1f;
		desc.startDuration = 1.0f;
		desc.startPos = NullVector;
		desc.startScale = IdentityVector * 1.0f;
		desc.density = 2.0f;		

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedScale = IdentityVector * (-0.25f);
		descFlow.speedPos = XVector * 3.0f;		

		LoadFxFlow("accelEff", "Effect\\flare3", _fxSpriteManager, desc, descFlow, false, 1.5f);
	}

	//explosion2
	LoadFxSprite("explosion2", "Effect\\explosion2", NullVector, Vec3Range(NullVector, D3DXVECTOR3(25.0f, 25.0f, 25.0f)), NullQuaternion, false, graph::SceneNode::amOnce, 1.0f, 0.0f, 1.0f);
	//explosion3
	LoadFxSprite("explosion3", "Effect\\explosion3", NullVector, D3DXVECTOR3(6.0f, 6.0f, 6.0f), NullQuaternion, false, graph::SceneNode::amOnce, 0.8f, 0.0f, 0.8f);
	//explosion4
	LoadFxSprite("explosion4", "Effect\\explosion4", NullVector, D3DXVECTOR3(6.0f, 6.0f, 6.0f), NullQuaternion, false, graph::SceneNode::amOnce, 0.7f, 0.0f, 0.7f);
	//boom1
	LoadFxSprite("boom1", "Effect\\boom1", NullVector, D3DXVECTOR3(1.5f, 1.5f, 1.5f), NullQuaternion, false, graph::SceneNode::amOnce, 0.7f, 0.0f, 0.7f);
	//boom2
	LoadFxSprite("boom2", "Effect\\boom2", NullVector, D3DXVECTOR3(1.2f, 1.2f, 1.2f), NullQuaternion, false, graph::SceneNode::amOnce, 0.7f, 0.0f, 0.7f);
	//boomSpark1
	LoadFxSprite("boomSpark1", "Effect\\boomSpark1", NullVector, Vec3Range(D3DXVECTOR3(1.0f, 1.0f, 1.0f), D3DXVECTOR3(10.0f, 10.0f, 10.0f)), NullQuaternion, false, graph::SceneNode::amOnce, 0.5f, 0.0f, 0.5f);
	//boomSpark2
	LoadFxSprite("boomSpark2", "Effect\\boomSpark2", NullVector, Vec3Range(D3DXVECTOR3(1.0f, 1.0f, 1.0f), D3DXVECTOR3(10.0f, 10.0f, 10.0f)), NullQuaternion, false, graph::SceneNode::amOnce, 0.5f, 0.0f, 0.5f);
	//refr1
	LoadFxSprite("refr1", "Effect\\j_swell", NullVector, Vec3Range(NullVector, IdentityVector * 100.0f), NullQuaternion, false, graph::SceneNode::amOnce, 0.5f, 0.0f, 0.5f, false, IdentityVec2, gtRefrEffect);
	//flare1
	LoadFxSprite("flare1", "Effect\\flare1", NullVector, Vec3Range(IdentityVector, NullVector), NullQuaternion, false, graph::SceneNode::amNone, 0);
	//flare2
	LoadFxSprite("flare2", "Effect\\flare2", NullVector, IdentityVector, NullQuaternion, false, graph::SceneNode::amTwoSide, 1.0f, 0, 0, false, IdentityVec2 * 0.5f);
	//flare3
	LoadFxSprite("flare3", "Effect\\flare3", NullVector, Vec3Range(IdentityVector, NullVector), NullQuaternion, false, graph::SceneNode::amNone, 0);
	//flare4
	LoadFxSprite("flare4", "Effect\\flare4", NullVector, IdentityVector, NullQuaternion, false, graph::SceneNode::amNone, 0);
	//flare5	
	LoadFxSprite("flare5", "Effect\\flare5", NullVector, IdentityVector, NullQuaternion, false, graph::SceneNode::amNone, 0, 0, 0, false, D3DXVECTOR2(0.8f, 0.8f));
	//flare6
	LoadFxSprite("flare6", "Effect\\flare6", NullVector, IdentityVector, NullQuaternion, false, graph::SceneNode::amNone, 0, 0, 0.15f, false, D3DXVECTOR2(2.0f, 2.0f));
	//firePatron
	LoadFxSprite("firePatron", "Effect\\firePatron", NullVector, IdentityVector, NullQuaternion, false, graph::SceneNode::amNone, 0, 0, 0, false, D3DXVECTOR2(2.0f, 2.0f));
	//lens1
	LoadFxSprite("lens1", "Effect\\lens1", NullVector, IdentityVector, NullQuaternion, false, graph::SceneNode::amNone, 0, 0, 0, false, D3DXVECTOR2(3.0f, 0.75f));	
	//death1
	LoadFxSprite("death1", "Effect\\gunEff2", NullVector, Vec3Range(NullVector, IdentityVector * 10.0f), NullQuaternion, false, graph::SceneNode::amOnce, 0.25f, 0.0f, 0.25f);
	//engine1
	LoadFxSprite("engine1", "Effect\\engine1", NullVector, Vec3Range(IdentityVector * 2.0f, NullVector), NullQuaternion, false, graph::SceneNode::amRepeat, 0.5f, 0.0f, 0.0f, true, D3DXVECTOR2(2.0f, 0.75f));
	//explosionRay
	LoadFxSprite("explosionRay", "Effect\\ExplosionRay", NullVector, Vec3Range(IdentityVector, IdentityVector * 6.0f), NullQuaternion, false, graph::SceneNode::amOnce, 1.0f, 0.0f, 1.0f, false, D3DXVECTOR2(2.0f, 2.0f));
	//laserBlue
	LoadFxSprite("laserBlue", "Effect\\laser3-blue", NullVector, Vec3Range(IdentityVector, NullVector), NullQuaternion, true, graph::SceneNode::amNone, 0.0f, 0.0f, 0.0f, true, D3DXVECTOR2(5.0f, 2.0f));	
	//ExplosionRing
	LoadFxSprite("explosionRing", "Effect\\ExplosionRing", NullVector, Vec3Range(IdentityVector, IdentityVector * 16.0f), NullQuaternion, false, graph::SceneNode::amOnce, 0.5f, 0.0f, 0.0f, false, D3DXVECTOR2(1.0f, 1.0f));
	//protonRay
	LoadFxSprite("protonRay", "Effect\\protonRay", NullVector, Vec3Range(IdentityVector, IdentityVector * 25.0f), NullQuaternion, false, graph::SceneNode::amOnce, 0.7f, 0.0f, 0.0f, false, D3DXVECTOR2(1.0f, 1.0f));
	//protonRing
	LoadFxPlane("protonRing", "Effect\\protonRing", NullVector, Vec3Range(IdentityVector, IdentityVector * 25.0f), NullQuaternion, graph::SceneNode::amOnce, 0.5f, 0.0f, 0.0f, D3DXVECTOR2(1.0f, 1.0f));
	//phaseRing
	LoadFxSprite("phaseRing", "Effect\\phaseRing", NullVector, Vec3Range(IdentityVector, NullVector), NullQuaternion, true, graph::SceneNode::amNone, 0, 0.0f, 0.0f, true, D3DXVECTOR2(3.0f, 1.5f));
	//shotEff1
	LoadFxSprite("shotEff1", "Effect\\rad_add", NullVector, Vec3Range(IdentityVector, NullVector), NullQuaternion, false, graph::SceneNode::amNone, 0, 0, 0.15f, false, D3DXVECTOR2(2.0f, 2.0f));
	//shotEff2
	LoadFxSprite("shotEff2", "Effect\\flash1", NullVector, Vec3Range(IdentityVector, NullVector), NullQuaternion, false, graph::SceneNode::amNone, 0, 0, 0.15f, false, D3DXVECTOR2(2.0f, 2.0f));
	//shotEff3
	LoadFxSprite("shotEff3", "Effect\\flash2", NullVector, Vec3Range(IdentityVector, NullVector), NullQuaternion, false, graph::SceneNode::amNone, 0, 0, 0.15f, true, D3DXVECTOR2(2.0f, 1.0f));
	//laserRay
	LoadFxSprite("laserRay", "Effect\\laserRay", NullVector, Vec3Range(IdentityVector, NullVector), NullQuaternion, false, graph::SceneNode::amOnce, 1.0f, 0, 0.0f, true, D3DXVECTOR2(1.0f, 0.3f), gtEffect, true);
	//frostRay
	LoadFxSprite("frostRay", "Effect\\frostRay", NullVector, Vec3Range(IdentityVector, NullVector), NullQuaternion, false, graph::SceneNode::amTwoSide, 1.0f, 0, 0.0f, true, D3DXVECTOR2(1.0f, 0.5f), gtEffect, true);
	//frostShot
	{
		D3DXQUATERNION lineRot1;
		D3DXQuaternionRotationAxis(&lineRot1, &ZVector, D3DX_PI);
		LoadFxSprite("frostShot", "Effect\\frostLine", NullVector, Vec3Range(IdentityVector, NullVector), QuatRange(NullQuaternion, lineRot1), false, graph::SceneNode::amTwoSide, 1.0f, 0, 1.0f, false, D3DXVECTOR2(4.0f, 4.0f));
	}

	//fire1
	{
		game::MapObj* mapObj = NewMapObj();
		AddToGraph(mapObj, gtEffect, true);
		
		graph::FxParticleSystem* fxSystem = AddFxSystem(mapObj, _fxSpriteManager);
		AddLibMat(&fxSystem->material, "Effect\\fire1");

		graph::FxEmitter::ParticleDesc desc;
		desc.life = FloatRange(0.5f, 0.6f);
		desc.startTime = FloatRange(0.1f, 0.2f);
		desc.density = FloatRange(2.0f, 3.0f);
		desc.startPos = Vec3Range(D3DXVECTOR3(-0.1f, -0.1f, -0.5f), D3DXVECTOR3(0.1f, 0.1f, 0.1f), Vec3Range::vdVolume);
		D3DXQUATERNION spRot1, spRot2;
		D3DXQuaternionRotationAxis(&spRot1, &(-ZVector), D3DX_PI);
		D3DXQuaternionRotationAxis(&spRot2, &ZVector, 2.0f * D3DX_PI);
		desc.startRot = QuatRange(spRot1, spRot2, QuatRange::vdVolume);

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedPos = Vec3Range(D3DXVECTOR3(-1, -1, 7), D3DXVECTOR3(1, 1, 7), Vec3Range::vdVolume) * 0.4f;

		graph::FxFlowEmitter* emitter = AddFxFlowEmitter(fxSystem, desc, descFlow, true);		
		NewChildMapObj(mapObj, MapObjLib::ctEffects, "smoke1")->GetGameObj().SetPos(D3DXVECTOR3(0.0f, 0.0f, 0.5f));

		SaveMapObj(mapObj, MapObjLib::ctEffects, "fire1");
	}

	//fire2
	{
		game::MapObj* mapObj = NewMapObj();
		AddToGraph(mapObj, gtEffect, true);

		graph::FxParticleSystem* fxSystem = AddFxSystem(mapObj, _fxSpriteManager);
		AddLibMat(&fxSystem->material, "Effect\\fire1");

		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 100;
		desc.maxNumAction = graph::FxEmitter::mnaWaitingFree;
		desc.life = FloatRange(1.5f, 2.25f);
		desc.startTime = 0.4f;
		desc.startDuration = 0.0f;
		desc.startType = graph::FxEmitter::sotDist;
		desc.density = 1.0f;
		desc.startPos = Vec3Range(D3DXVECTOR3(-0.0f, -0.2f, -0.2f), D3DXVECTOR3(0.0f, 0.2f, 0.2f), Vec3Range::vdVolume);
		desc.startScale = D3DXVECTOR3(0.7f, 0.7f, 0.7f);
		desc.startRot = QuatRange(D3DXQUATERNION(-1, -1, -1, -1), D3DXQUATERNION(1, 1, 1, 1), QuatRange::vdVolume);

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedPos = Vec3Range(D3DXVECTOR3(11.0f, -0.5f, 0.0f), D3DXVECTOR3(13.0f, 0.5f, 0.1f), Vec3Range::vdVolume);
		descFlow.speedRot = NullQuaternion;
		descFlow.speedScale = Vec3Range(D3DXVECTOR3(2.0f, 2.0f, 2.0f), D3DXVECTOR3(2.4f, 2.4f, 2.4f));
		descFlow.acceleration = D3DXVECTOR3(-7.0f, 0.0f, 0.0f);
		descFlow.gravitation = D3DXVECTOR3(0.0f, 0.0f, 1.0f);
		descFlow.autoRot = false;

		graph::FxFlowEmitter* emitter = AddFxFlowEmitter(fxSystem, desc, descFlow, true);

		mapObj->GetGameObj().GetBehaviors().Add<FxSystemWaitingEnd>();	
		mapObj->GetGameObj().GetBehaviors().Add<FxSystemSrcSpeed>();

		SaveMapObj(mapObj, MapObjLib::ctEffects, "fire2");
	}
	
	//Death2
	{
		game::MapObj* mapObj = NewMapObj();
		mapObj->GetGameObj().SetPos(D3DXVECTOR3(0, 0, 1.5f));
		mapObj->GetGameObj().SetMaxTimeLife(10);
		AddToGraph(mapObj, gtEffect, true);

		NewChildMapObj(mapObj, MapObjLib::ctEffects, "explosion2");
		NewChildMapObj(mapObj, MapObjLib::ctEffects, "wheels1");
		NewChildMapObj(mapObj, MapObjLib::ctEffects, "truba1");
		NewChildMapObj(mapObj, MapObjLib::ctEffects, "refr1");
		NewChildMapObj(mapObj, MapObjLib::ctEffects, "spark1");

		//debris
		{
			graph::FxParticleSystem* fxSystem = AddFxSystem(mapObj, _fxPiecesManager, graph::FxParticleSystem::csUnique);
			
			graph::FxEmitter::ParticleDesc desc;
			desc.maxNum = 3;
			desc.maxNumAction = graph::FxEmitter::mnaWaitingFree;
			desc.startTime = 0;
			desc.density = 3;
			desc.life = 0;
			desc.startPos = Vec3Range(D3DXVECTOR3(-0.1f, -0.1f, -0.1f), D3DXVECTOR3(0.1f, 0.1f, 0.1f), Vec3Range::vdVolume, Point3U(100, 100, 100));
			desc.startScale = Vec3Range(IdentityVector * 0.9f, IdentityVector * 1.1f);			
				
			graph::FxFlowEmitter::FlowDesc descFlow;
			descFlow.speedPos = Vec3Range(D3DXVECTOR3(-1.5f, -1.5f, 2.0f), D3DXVECTOR3(1.5f, 1.5f, 5.0f), Vec3Range::vdVolume) * 3.0f;
			//
			D3DXQUATERNION spRot1, spRot2;
			D3DXQuaternionRotationAxis(&spRot1, &(-IdentityVector), D3DX_PI);
			D3DXQuaternionRotationAxis(&spRot2, &IdentityVector, 2.0f * D3DX_PI);
			descFlow.speedRot = QuatRange(spRot1, spRot2, QuatRange::vdVolume, Point2U(100, 100));
			//
			descFlow.gravitation = D3DXVECTOR3(0, 0, -9.80f);

			AddFxFlowEmitter(fxSystem, desc, descFlow, true);
			
			//smoke debr
			{
				graph::FxParticleSystem* smokeSys = AddFxSystem(&fxSystem->GetChild(), _fxPSpriteManager);
				AddLibMat(&smokeSys->material, "Effect\\smoke1");
				graph::FxFlowEmitter* smoke = &smokeSys->GetEmitters().Add<graph::FxFlowEmitter>();

				graph::FxEmitter::ParticleDesc desc;
				desc.life = FloatRange(2.0f, 3.0f);
				desc.startType = graph::FxEmitter::sotDist;
				desc.startTime = FloatRange(1.0f, 1.1f);
				desc.density = FloatRange(1.0f, 3.0f);
				desc.startPos = Vec3Range(D3DXVECTOR3(-0.1f, -0.1f, -0.1f), D3DXVECTOR3(0.1f, 0.1f, 0.1f), Vec3Range::vdVolume);
				desc.startScale = D3DXVECTOR3(IdentityVector);

				graph::FxFlowEmitter::FlowDesc descFlow = smoke->GetFlowDesc();
				descFlow.speedScale = IdentityVector * 3.0f;
				descFlow.speedPos = Vec3Range(D3DXVECTOR3(-0.5f, -0.5f, 2.0f), D3DXVECTOR3(0.5f, 0.5f, 2.0f), Vec3Range::vdVolume, Point3U(100, 100, 100));
				
				AddFxFlowEmitter(smokeSys, desc, descFlow, true);
			}
		}

		{
			LifeEffect& effect = mapObj->GetGameObj().GetBehaviors().Add<LifeEffect>();
			effect.SetSound(&GetSound("Sounds\\carcrash05.ogg"));
		}

		SaveMapObj(mapObj, MapObjLib::ctEffects, "death2");
	}	

	//Death3
	{
		game::MapObj* mapObj = NewMapObj();
		mapObj->GetGameObj().SetPos(D3DXVECTOR3(0, 0, 0.3f));
		mapObj->GetGameObj().SetMaxTimeLife(0.7f);
		AddToGraph(mapObj, gtEffect, true);

		{
			MapObj* child = NewChildMapObj(mapObj, MapObjLib::ctEffects, "explosion3");
			child->GetGameObj().SetPos(D3DXVECTOR3(0.0f, 0.0f, 2.0f));
		}
		NewChildMapObj(mapObj, MapObjLib::ctEffects, "explosionRing");
		NewChildMapObj(mapObj, MapObjLib::ctEffects, "spark3");

		{
			LifeEffect& effect = mapObj->GetGameObj().GetBehaviors().Add<LifeEffect>();
			effect.SetSound(&GetSound("Sounds\\carcrash05.ogg"));
		}

		SaveMapObj(mapObj, MapObjLib::ctEffects, "death3");
	}

	//Death4
	{
		game::MapObj* mapObj = NewMapObj();
		mapObj->GetGameObj().SetMaxTimeLife(0.7f);
		AddToGraph(mapObj, gtEffect, true);

		MapObj* exp = NewChildMapObj(mapObj, MapObjLib::ctEffects, "explosion4");
		exp->GetGameObj().SetPos(ZVector * 1.0f);

		MapObj* ray = NewChildMapObj(mapObj, MapObjLib::ctEffects, "protonRay");
		ray->GetGameObj().SetPos(ZVector * 1.0f);

		MapObj* ring = NewChildMapObj(mapObj, MapObjLib::ctEffects, "protonRing");
		ring->GetGameObj().SetPos(ZVector * 0.0f);

		{
			LifeEffect& effect = mapObj->GetGameObj().GetBehaviors().Add<LifeEffect>();
			effect.SetSound(&GetSound("Sounds\\carcrash05.ogg"));
		}

		SaveMapObj(mapObj, MapObjLib::ctEffects, "death4");
	}

	//Death5
	{
		game::MapObj* mapObj = NewMapObj();		
		mapObj->GetGameObj().SetMaxTimeLife(0.7f);
		AddToGraph(mapObj, gtEffect, true);

		NewChildMapObj(mapObj, MapObjLib::ctEffects, "boom1");
		NewChildMapObj(mapObj, MapObjLib::ctEffects, "boomSpark1");

		SaveMapObj(mapObj, MapObjLib::ctEffects, "death5");
	}

	//Death6
	{
		game::MapObj* mapObj = NewMapObj();		
		mapObj->GetGameObj().SetMaxTimeLife(0.7f);
		AddToGraph(mapObj, gtEffect, true);

		NewChildMapObj(mapObj, MapObjLib::ctEffects, "boom2");
		NewChildMapObj(mapObj, MapObjLib::ctEffects, "boomSpark2");

		SaveMapObj(mapObj, MapObjLib::ctEffects, "death6");
	}

	//trail
	{
		game::MapObj* mapObj = NewMapObj();
		AddToGraph(mapObj, gtEffect, true);

		graph::FxParticleSystem* fxSystem = AddFxSystem(mapObj, _fxTrailManager);
		AddLibMat(&fxSystem->material, "Effect\\asphaltMarks");

		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 100;
		desc.life = 10.0f;
		desc.startTime = 1.0f;
		desc.density = 1.0f;
		desc.startPos = NullVector;
		desc.startType = graph::FxEmitter::sotDist;			

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedPos = NullVector;

		mapObj->GetGameObj().GetBehaviors().Add<FxSystemWaitingEnd>();
		graph::FxFlowEmitter* emitter = AddFxFlowEmitter(fxSystem, desc, descFlow, true);
		SaveMapObj(mapObj, MapObjLib::ctEffects, "trail");
	}

	//heatTrail
	{
		game::MapObj* mapObj = NewMapObj();
		AddToGraph(mapObj, gtEffect, true);

		graph::FxParticleSystem* fxSystem = AddFxSystem(mapObj, _fxTrailManager);
		AddLibMat(&fxSystem->material, "Effect\\heatTrail");

		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 100;
		desc.maxNumAction = graph::FxEmitter::mnaWaitingFree;
		desc.startTime = 1.0f;
		desc.density = 1.0f;
		desc.life = 1.0f;
		desc.startPos = NullVector;
		desc.startType = graph::FxEmitter::sotDist;

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedPos = NullVector;

		mapObj->GetGameObj().GetBehaviors().Add<FxSystemWaitingEnd>();
		graph::FxFlowEmitter* emitter = AddFxFlowEmitter(fxSystem, desc, descFlow, true);

		SaveMapObj(mapObj, MapObjLib::ctEffects, "heatTrail");
	}

	//resonanseBolt
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();		

		graph::IVBMeshNode* mesh = AddMeshNode(mapObj, "Effect\\sphere.r3d");
		AddLibMat(mesh, "Effect\\gravBall");
		mesh->SetScale(0.25f);
		AddToGraph(mapObj, gtDefFixPipe, true);

		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "heatTrail");	
		}

		SaveMapObj(mapObj, MapObjLib::ctEffects, "resonanseBolt");
	}

	//sonar
	{
		game::MapObj* mapObj = NewMapObj();
		AddToGraph(mapObj, gtEffect, true);

		graph::FxParticleSystem* fxSystem = AddFxSystem(mapObj, _fxPSpriteManager);
		AddLibMat(&fxSystem->material, "Effect\\sonar");

		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 6;
		desc.maxNumAction = graph::FxEmitter::mnaWaitingFree;
		desc.life = 1.05f;
		desc.startTime = 0.2f;
		desc.startDuration = 1.0f;
		desc.startType = graph::FxEmitter::sotTime;
		desc.density = 1.0f;
		desc.startPos = NullVector;
		desc.startScale = IdentityVector * 0.6f;
		desc.startRot = NullQuaternion;		
		desc.rangeLife = FloatRange(0.0f, -0.6f);
		desc.rangePos = Vec3Range(D3DXVECTOR3(1.0f, 0, 0), D3DXVECTOR3(-1.0f, 0, 0));

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedPos = XVector * 2.0f;
		descFlow.speedRot = NullQuaternion;
		descFlow.speedScale = IdentityVector * 6.0f;
		descFlow.autoRot = true;

		graph::FxFlowEmitter* emitter = AddFxFlowEmitter(fxSystem, desc, descFlow, false);
		SaveMapObj(mapObj, MapObjLib::ctEffects, "sonar");
	}

	//powerShot1
	{
		MapObj* mapObj = NewMapObj();
		{
			MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "shotEff3");
		}
		{
			MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "smoke3");
		}
		SaveMapObj(mapObj, MapObjLib::ctEffects, "powerShot1");
	}

	//ringWay
	{
		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 0;
		desc.maxNumAction = graph::FxEmitter::mnaWaitingFree;
		desc.startType = graph::FxEmitter::sotTime;
		desc.startTime = 0.5f;
		desc.density = 1;
		desc.life = 1.5f;
		desc.startPos = NullVector;
		desc.startScale = NullVector;

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedScale = IdentityVector * 7.5f;

		LoadFxFlow("ringWay", "Effect\\ring1", _fxPlaneManager, desc, descFlow, false);
	}

	//flareLaser1
	{
		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 3;		
		desc.maxNumAction = graph::FxEmitter::mnaWaitingFree;
		desc.startType = graph::FxEmitter::sotTime;
		desc.startTime = 2.0f;
		desc.density = 3;
		desc.life = 1.0f;
		desc.startPos = NullVector;
		desc.startScale = IdentityVector * 2.0f;
		desc.rangeRot = QuatRange(D3DXQUATERNION(-1, -1, -1, -1), D3DXQUATERNION(1, 1, 1, 1), QuatRange::vdVolume);

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedRot = QuatRange(D3DXQUATERNION(-1, -1, -1, -1), D3DXQUATERNION(1, 1, 1, 1), QuatRange::vdVolume);

		LoadFxFlow("flareLaser1", "Effect\\flareLaser1", _fxSpriteManager, desc, descFlow, false, 1.0f);
	}

	//flareLaser3
	{
		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 0;
		desc.maxNumAction = graph::FxEmitter::mnaWaitingFree;
		desc.startType = graph::FxEmitter::sotTime;
		desc.startTime = 0.15f;
		desc.density = 1;
		desc.life = FloatRange(0.15f, 0.5f);
		desc.startPos = NullVector;
		desc.startScale = Vec3Range(IdentityVector * 1.25f, IdentityVector * 2.0f);
		desc.rangeRot = QuatRange(D3DXQUATERNION(-1, -1, -1, -1), D3DXQUATERNION(1, 1, 1, 1), QuatRange::vdVolume);

		graph::FxFlowEmitter::FlowDesc descFlow;		

		LoadFxFlow("flareLaser3", "Effect\\flareLaser3", _fxSpriteManager, desc, descFlow, false);
	}

	//laserShot
	{
		MapObj* mapObj = NewMapObj();
		{
			MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "flareLaser1");
			eff->GetGameObj().SetPos(D3DXVECTOR3(0.4f, 0.0f, 0.0f));
		}
		SaveMapObj(mapObj, MapObjLib::ctEffects, "laserShot");
	}

	//laserSpark
	{
		MapObj* mapObj = NewMapObj();
		{
			NewChildMapObj(mapObj, MapObjLib::ctEffects, "laserSparkBase");
		}
		{
			NewChildMapObj(mapObj, MapObjLib::ctEffects, "flareLaser3");
		}
		SaveMapObj(mapObj, MapObjLib::ctEffects, "laserSpark");
	}

	//rain
	{
		game::MapObj* mapObj = NewMapObj();
		AddToGraph(mapObj, gtEffect, true);

		graph::FxParticleSystem* fxSystem = AddFxSystem(mapObj, _fxDirSpriteManager);
		AddLibMat(&fxSystem->material, "Effect\\drop");

		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 0;
		desc.maxNumAction = graph::FxEmitter::mnaWaitingFree;
		desc.life = FloatRange(1.0f, 2.0f);		
		desc.startTime = FloatRange(0.05f, 0.1f);
		desc.startDuration = 0.0f;
		desc.startType = graph::FxEmitter::sotTime;
		desc.density = FloatRange(20.0f, 30.0f);
		desc.startPos = Vec3Range(D3DXVECTOR3(-6.0f, -8.0f, 2.0f), D3DXVECTOR3(6.0f, 4.0f, 3.0f), Vec3Range::vdVolume);
		desc.startScale = D3DXVECTOR3(1.0f, 0.1f, 1.0f);
		desc.startRot = NullQuaternion;		

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedPos = Vec3Range(D3DXVECTOR3(0.0f, 4.0f, -12.0f), D3DXVECTOR3(0.0f, 2.0f, -12.0f), Vec3Range::vdVolume);
		descFlow.speedRot = NullQuaternion;
		descFlow.speedScale = NullVector;
		descFlow.autoRot = true;

		graph::FxFlowEmitter* emitter = AddFxFlowEmitter(fxSystem, desc, descFlow, false);
		SaveMapObj(mapObj, MapObjLib::ctEffects, "rain");
	}

	//rainIso
	{
		game::MapObj* mapObj = NewMapObj();
		AddToGraph(mapObj, gtEffect, true);

		graph::FxParticleSystem* fxSystem = AddFxSystem(mapObj, _fxDirSpriteManager);
		AddLibMat(&fxSystem->material, "Effect\\drop");

		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 0;
		desc.maxNumAction = graph::FxEmitter::mnaWaitingFree;
		desc.life = FloatRange(1.0f, 2.0f);		
		desc.startTime = FloatRange(0.05f, 0.1f);
		desc.startDuration = 0.0f;
		desc.startType = graph::FxEmitter::sotTime;
		desc.density = FloatRange(20.0f, 25.0f);
		desc.startPos = Vec3Range(D3DXVECTOR3(40.0f, 1.0f, 17.0f), D3DXVECTOR3(-2.4f, 22.05f, 13.0f), Vec3Range::vdVolume);
		desc.startScale = Vec3Range(D3DXVECTOR3(3.0f, 0.3f, 1.0f), D3DXVECTOR3(3.5f, 0.25f, 1.0f), Vec3Range::vdVolume);
		desc.startRot = NullQuaternion;		

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedPos = Vec3Range(D3DXVECTOR3(0.0f, 15.0f, -45.0f), D3DXVECTOR3(0.0f, 7.5f, -45.0f), Vec3Range::vdVolume);
		descFlow.speedRot = NullQuaternion;
		descFlow.speedScale = NullVector;
		descFlow.autoRot = true;

		graph::FxFlowEmitter* emitter = AddFxFlowEmitter(fxSystem, desc, descFlow, false);
		SaveMapObj(mapObj, MapObjLib::ctEffects, "rainIso");
	}

	//frostSmoke
	{
		game::MapObj* mapObj = NewMapObj();
		AddToGraph(mapObj, gtEffect, true);

		graph::FxParticleSystem* fxSystem = AddFxSystem(mapObj, _fxSpriteManager);
		AddLibMat(&fxSystem->material, "Effect\\frostSmoke");
		fxSystem->animMode(graph::SceneNode::amTwoSide);

		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 0;
		desc.startType = graph::FxEmitter::sotDist;
		desc.life = FloatRange(1.5f, 1.6f);
		desc.startTime = FloatRange(0.7f);
		desc.density = FloatRange(1.0f, 1.0f);
		desc.startPos = Vec3Range(D3DXVECTOR3(-0.6f, -0.6f, -0.6f), D3DXVECTOR3(0.6f, 0.6f, 0.6f), Vec3Range::vdVolume);
		D3DXQUATERNION rot;
		D3DXQuaternionRotationAxis(&rot, &ZVector, D3DX_PI);
		desc.startRot = QuatRange(NullQuaternion, rot, QuatRange::vdLinear);
		desc.startScale = D3DXVECTOR3(IdentityVector) * 4.0f;
		
		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedScale = -IdentityVector * 1.0f;
		descFlow.speedPos = Vec3Range(D3DXVECTOR3(-0.5f, -0.5f, 3.0f), D3DXVECTOR3(0.5f, 0.5f, 3.5f), Vec3Range::vdVolume);
		graph::FxFlowEmitter* emitter = AddFxFlowEmitter(fxSystem, desc, descFlow, true);

		mapObj->GetGameObj().GetBehaviors().Add<FxSystemWaitingEnd>();

		SaveMapObj(mapObj, MapObjLib::ctEffects, "frostSmoke");
	}

	//frostHit
	{
		game::MapObj* mapObj = NewMapObj();
		AddToGraph(mapObj, gtEffect, true);

		graph::FxParticleSystem* fxSystem = AddFxSystem(mapObj, _fxSpriteManager);
		AddLibMat(&fxSystem->material, "Effect\\frostHit");
		fxSystem->animMode(graph::SceneNode::amOnce);

		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 0;
		desc.startType = graph::FxEmitter::sotDist;
		desc.life = FloatRange(0.7f, 0.9f);
		desc.startTime = 0.5f;
		desc.density = FloatRange(1.0f, 1.0f);
		desc.startPos = Vec3Range(D3DXVECTOR3(-0.3f, -0.3f, -0.3f), D3DXVECTOR3(0.3f, 0.3f, 0.3f), Vec3Range::vdVolume);
		D3DXQUATERNION rot;
		D3DXQuaternionRotationAxis(&rot, &ZVector, D3DX_PI);
		desc.startRot = QuatRange(NullQuaternion, rot, QuatRange::vdLinear);
		desc.startScale = D3DXVECTOR3(IdentityVector) * 1.5f;

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedScale = IdentityVector * 2.0f;
		descFlow.speedPos = Vec3Range(D3DXVECTOR3(-0.5f, -0.5f, 1.0f), D3DXVECTOR3(0.5f, 0.5f, 1.2f), Vec3Range::vdVolume);
		graph::FxFlowEmitter* emitter = AddFxFlowEmitter(fxSystem, desc, descFlow, true);

		mapObj->GetGameObj().GetBehaviors().Add<FxSystemWaitingEnd>();

		SaveMapObj(mapObj, MapObjLib::ctEffects, "frostHit");
	}

	//frost
	{
		game::MapObj* mapObj = NewMapObj();
		AddToGraph(mapObj, gtEffect, true);
		mapObj->GetGameObj().SetMaxTimeLife(1.0f);

		NewChildMapObj(mapObj, MapObjLib::ctEffects, "frostSmoke");

		graph::IVBMeshNode* mesh = AddMeshNode(mapObj, "Effect\\frost.r3d");
		mesh->animMode(graph::SceneNode::amOnce);
		mesh->animDuration = 1.0f;
		AddLibMat(mesh, "Effect\\frost");

		SaveMapObj(mapObj, MapObjLib::ctEffects, "frost");
	}

	//fireTrail
	{
		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 0;
		desc.life = FloatRange(0.5f, 0.6f);
		desc.startTime = FloatRange(1.0f, 1.1f);
		desc.startType = graph::FxEmitter::sotDist;
		desc.density = 1.0f;
		desc.startPos = Vec3Range(IdentityVector * (-0.1f), IdentityVector * 0.1f, Vec3Range::vdVolume);
		desc.startScale = Vec3Range(IdentityVector * 0.5f, IdentityVector * 0.6f, Vec3Range::vdVolume);
		desc.startRot = QuatRange(D3DXQUATERNION(-1, -1, -1, -1), D3DXQUATERNION(1, 1, 1, 1), QuatRange::vdVolume);

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedScale = IdentityVector * 0.5f;
		descFlow.speedPos = -XVector;

		LoadFxFlow("fireTrail", "Effect\\fireTrail", _fxSpriteManager, desc, descFlow, true);
	}

	//hyperRing
	{
		game::MapObj* mapObj = NewMapObj();
		AddToGraph(mapObj, gtEffect, true);

		graph::FxParticleSystem* fxSystem = AddFxSystem(mapObj, _fxSpriteManager);
		AddLibMat(&fxSystem->material, "Effect\\ring2");
		fxSystem->animMode(graph::SceneNode::amTwoSide);

		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 0;
		desc.startType = graph::FxEmitter::sotDist;
		desc.life = 0.7f;
		desc.startTime = 1.6f;
		desc.density = FloatRange(1.0f, 1.0f);
		desc.startPos = NullVector;
		desc.startRot = NullQuaternion;
		desc.startScale = D3DXVECTOR3(0.5f, 1.0f, 1.0f);

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedScale = IdentityVector * 2.0f;
		descFlow.speedPos = NullVector;
		graph::FxFlowEmitter* emitter = AddFxFlowEmitter(fxSystem, desc, descFlow, true);

		mapObj->GetGameObj().GetBehaviors().Add<FxSystemWaitingEnd>();

		SaveMapObj(mapObj, MapObjLib::ctEffects, "hyperRing");
	}


	//hyperBlaster
	{
		game::MapObj* mapObj = NewMapObj();
		AddToGraph(mapObj, gtEffect, true);

		AddFxSprite(mapObj, "Effect\\blaster2", NullVector, Vec3Range(IdentityVector, NullVector), NullQuaternion, true, graph::SceneNode::amNone, 0, 0, true, D3DXVECTOR2(6.0f, 0.5f));

		MapObj* ring = NewChildMapObj(mapObj, MapObjLib::ctEffects, "hyperRing");
		ring->GetGameObj().SetPos(XVector * 3.0f);
		
		SaveMapObj(mapObj, MapObjLib::ctEffects, "hyperBlaster");
	}

	//crater
	{
		game::MapObj* mapObj = NewMapObj();
		AddToGraph(mapObj, gtEffect, true);

		AddFxPlane(mapObj, "Effect\\crater", NullVector, Vec3Range(IdentityVector * 8.0f, NullVector), NullQuaternion, graph::SceneNode::amOnce, 5.0f, 0, D3DXVECTOR2(1.0f, 1.0f));

		graph::FxParticleSystem* fxSystem = AddFxSystem(mapObj, _fxSpriteManager);
		AddLibMat(&fxSystem->material, "Effect\\fire2");
		fxSystem->animMode(graph::SceneNode::amOnce);

		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 50;
		desc.startType = graph::FxEmitter::sotTime;
		desc.life = FloatRange(0.5f, 1.0f);
		desc.startTime = 0;
		desc.density = 5.0f;
		desc.startPos = Vec3Range(D3DXVECTOR3(-2.0f, -2.0f, 0.0f), D3DXVECTOR3(2.0f, 2.0f, 0.0f), Vec3Range::vdVolume);
		D3DXQUATERNION rot1, rot2;
		D3DXQuaternionRotationAxis(&rot1, &ZVector, -D3DX_PI/3);
		D3DXQuaternionRotationAxis(&rot2, &ZVector, D3DX_PI/3);
		desc.startRot = QuatRange(rot1, rot2, QuatRange::vdLinear);
		desc.startScale = D3DXVECTOR3(1.0f, 1.0f, 1.0f);

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedScale = NullVector;
		descFlow.speedPos = NullVector;
		graph::FxFlowEmitter* emitter = AddFxFlowEmitter(fxSystem, desc, descFlow, false);

		SaveMapObj(mapObj, MapObjLib::ctEffects, "crater");
	}

	//mortiraBallDeath
	{
		game::MapObj* mapObj = NewMapObj();
		game::AutoProj& gameObj = mapObj->SetGameObj<game::AutoProj>();		

		game::AutoProj::Desc projDesc;
		
		projDesc.type = Proj::ptCrater;
		projDesc.SetModel(GetRecord(MapObjLib::ctEffects, "crater"));
		projDesc.pos = D3DXVECTOR3(0, 0.0f, 0.1f);
		projDesc.size = D3DXVECTOR3(6.0f, 6.0f, 0.1f);
		projDesc.modelSize = false;
		projDesc.speed = 0.0f;
		projDesc.maxDist = 20.0f;
		projDesc.mass = 100.0f;
		projDesc.damage = 10.0f;
		projDesc.minTimeLife = 3.0f;
				
		gameObj.SetDesc(projDesc);
		gameObj.GetPxActor().SetScene(_world->GetPxScene());

		MapObj* death = NewChildMapObj(mapObj, MapObjLib::ctEffects, "death3");
		death->GetGameObj().SetPos(ZVector * 1.0f);
		
		SaveMapObj(mapObj, MapObjLib::ctEffects, "mortiraBallDeath");
	}

	//mortiraBall
	{
		game::MapObj* mapObj = NewMapObj();
		AddToGraph(mapObj, gtEffect, true);

		graph::IVBMeshNode* mesh = AddMeshNode(mapObj, "Weapon\\mortiraBall.r3d");
		AddLibMat(mesh, "Weapon\\mortiraBall");

		{
			DeathEffect& deathEffect = mapObj->GetGameObj().GetBehaviors().Add<DeathEffect>();
			deathEffect.SetEffect(GetRecord(MapObjLib::ctEffects, "mortiraBallDeath"));
			deathEffect.SetIgnoreRot(true);
			deathEffect.SetEffectPxIgnoreSenderCar(true);
		}

		SaveMapObj(mapObj, MapObjLib::ctEffects, "mortiraBall");
	}

	//shield1
	{
		game::MapObj* mapObj = NewMapObj();
		AddToGraph(mapObj, gtEffect, true);

		graph::IVBMeshNode* mesh = AddMeshNode(mapObj, "Effect\\sphere.r3d");
		AddLibMat(mesh, "Effect\\shield2");
		mesh->animMode(graph::SceneNode::amRepeat);
		mesh->animDuration = 5.5f;

		mesh = AddMeshNode(mapObj, "Effect\\sphere.r3d");
		AddLibMat(mesh, "Effect\\shield2Hor");
		mesh->SetScale(0.975f);
		mesh->animMode(graph::SceneNode::amRepeat);
		mesh->animDuration = 6.0f;

		mesh = AddMeshNode(mapObj, "Effect\\sphere.r3d");
		AddLibMat(mesh, "Effect\\shield2Vert");
		mesh->SetScale(0.95f);
		mesh->animMode(graph::SceneNode::amRepeat);
		mesh->animDuration = 6.5f;

		SaveMapObj(mapObj, MapObjLib::ctEffects, "shield1");
	}
}

void DataBase::LoadWorld1()
{
	LoadDecor("World1\\berge", "World1\\berge.r3d", "World1\\berge", gtDefFixPipe, false, true, false);
	LoadDecor("World1\\columns", "World1\\columns.r3d", "World1\\Track\\track1");
	LoadDecor("World1\\eldertree2", "World1\\eldertree2.r3d", "World1\\eldertree");
	LoadDecor("World1\\eldertree", "World1\\eldertree.r3d", "World1\\eldertree");
	LoadDecor("World1\\lowpalma1", "World1\\lowpalma1.r3d", "World1\\lowpalma");
	LoadDecor("World1\\lowpalma2", "World1\\lowpalma2.r3d", "World1\\lowpalma");
	LoadDecor("World1\\lowpalma3", "World1\\lowpalma3.r3d", "World1\\lowpalma");
	LoadDecor("World1\\palma2", "World1\\palma2.r3d", "World1\\palma");
	LoadDecor("World1\\palma3", "World1\\palma3.r3d", "World1\\palma");
	LoadDecor("World1\\palma4", "World1\\palma4.r3d", "World1\\palma");
	LoadDecor("World1\\palma", "World1\\palma.r3d", "World1\\palma");
	LoadDecor("World1\\paporotnik", "World1\\paporotnik.r3d", "World1\\paporotnik");
	LoadDecor("World1\\stone1", "World1\\stone1.r3d", "World1\\stone");
	LoadDecor("World1\\stone2", "World1\\stone2.r3d", "World1\\stone");
	LoadDecor("World1\\stone3", "World1\\stone3.r3d", "World1\\stone");
	LoadDecor("World1\\wand", "World1\\wand.r3d", "World1\\wand");
	LoadDecor("World1\\naves", "World1\\naves.r3d", "World1\\naves", gtDefFixPipe, false, true, false);
	LoadDecor("World1\\zdanie1", "World1\\zdaine1.r3d", "World1\\zdanie1");
	LoadDecor("World1\\zdanie2", "World1\\zdaine2.r3d", "World1\\zdanie2");

	D3DXPLANE plane;
	float cosAng = cos(15.0f * D3DX_PI/180.0f);
	D3DXPlaneFromPointNormal(&plane, &D3DXVECTOR3(0, 0, 6.2f), &D3DXVECTOR3(-sqrt(1.0f - cosAng * cosAng), 0.0f, cosAng));
	D3DXVECTOR4 vec1 = plane;

	D3DXVECTOR4 vec3(0.0f, -0.15f, 0.075f, 0.0f);

	LoadTrack("World1\\jump", "World1\\Track\\jump.r3d", "World1\\Track\\track1", true, true);
	LoadTrack("World1\\most", "World1\\Track\\most.r3d", "World1\\Track\\most", true, true, false, false, "World1\\Track\\pxMost.r3d", true);
	LoadTrack("World1\\track1", "World1\\Track\\track1.r3d", "World1\\Track\\track1", true, true, false, false, "World1\\Track\\pxTrack1.r3d");
	LoadTrack("World1\\track2", "World1\\Track\\track2.r3d", "World1\\Track\\track1", true, true, false, false, "World1\\Track\\pxTrack2.r3d");
	LoadTrack("World1\\track3", "World1\\Track\\track3.r3d", "World1\\Track\\track1", true, true, false, false, "World1\\Track\\pxTrack3.r3d", false, vec1, NullVec4, vec3);
	LoadTrack("World1\\track4", "World1\\Track\\track4.r3d", "World1\\Track\\track1", true, true, false, false, "World1\\Track\\pxTrack4.r3d");
	LoadTrack("World1\\track5", "World1\\Track\\track5.r3d", "World1\\Track\\track1", true, true, false, false, "World1\\Track\\pxTrack5.r3d");
	LoadTrack("World1\\tramp1", "World1\\Track\\tramp1.r3d", "World1\\Track\\track1", true, true, false, false, "World1\\Track\\pxTramp1.r3d");
	LoadTrack("World1\\tramp2", "World1\\Track\\tramp2.r3d", "World1\\Track\\track1", true, true, false, false, "World1\\Track\\pxTramp2.r3d", false, vec1, NullVec4, vec3);
	LoadTrack("World1\\podjem2", "World1\\Track\\podjem2.r3d", "World1\\Track\\track1", true, true, false, false, "World1\\Track\\pxPodjem2.r3d");
	LoadTrack("World1\\jumper1", "World1\\Track\\jumper1.r3d", "World1\\Track\\track1", true, true, false, false, "World1\\Track\\pxTramp1.r3d");
	LoadTrack("World1\\jumper2", "World1\\Track\\jumper2.r3d", "World1\\Track\\track1", true, true, false, false, "World1\\Track\\pxTramp2.r3d", false, vec1, NullVec4, vec3);
}

void DataBase::LoadWorld2()
{
	LoadDecor("World2\\atom", "World2\\atom.r3d", "World2\\atom");
	LoadDecor("World2\\bochki", "World2\\bochki.r3d", "World2\\bochki");
	LoadDecor("World2\\deadtree1", "World2\\deadtree1.r3d", "World2\\deadtree1");
	LoadDecor("World2\\deadtree2", "World2\\deadtree2.r3d", "World2\\deadtree2");
	LoadDecor("World2\\deadtree3", "World2\\deadtree3.r3d", "World2\\deadtree3");
	LoadDecor("World2\\deadtree3_b", "World2\\deadtree3_b.r3d", "World2\\deadtree3");
	LoadDecor("World2\\elka", "World2\\elka.r3d", "World2\\elka");
	LoadDecor("World2\\factory", "World2\\factory.r3d", "World2\\factory");
	LoadDecor("World2\\haus1", "World2\\haus1.r3d", "World2\\haus1");
	LoadDecor("World2\\haus1_2", "World2\\haus1_2.r3d", "World2\\haus1");
	LoadDecor("World2\\haus3", "World2\\haus3.r3d", "World2\\Haus3");
	LoadDecor("World2\\machineFactory", "World2\\machineFactory.r3d", "World2\\machineFactory");	
	LoadDecor("World2\\metal1_1", "World2\\metal1_1.r3d", "World2\\metal1");
	LoadDecor("World2\\metal1_2", "World2\\metal1_2.r3d", "World2\\metal1");
	LoadDecor("World2\\naves1_1", "World2\\naves1_1.r3d", "World2\\naves1", gtDefFixPipe, false, true, false);
	LoadDecor("World2\\naves1_2", "World2\\naves1_2.r3d", "World2\\naves1", gtDefFixPipe, false, true, false);
	LoadDecor("World2\\poplar1_1", "World2\\poplar1_1.r3d", "World2\\poplar1");
	LoadDecor("World2\\poplar1_2", "World2\\poplar1_2.r3d", "World2\\poplar1");
	LoadDecor("World2\\pregrada", "World2\\pregrada.r3d", "World2\\pregrada", gtDefFixPipe, false, false, false);
	LoadDecor("World2\\projector", "World2\\projector.r3d", "World2\\projektor", gtDefFixPipe, false, false, false);
	LoadDecor("World2\\pumpjack", "World2\\pumpjack.r3d", "World2\\pumpjack");	
	LoadDecor("World2\\skelet1_1", "World2\\skelet1_1.r3d", "World2\\skelet1");
	LoadDecor("World2\\skelet1_2", "World2\\skelet1_2.r3d", "World2\\skelet1");
	LoadDecor("World2\\skelet1_3", "World2\\skelet1_3.r3d", "World2\\skelet1");	
	LoadDecor("World2\\strelka1_1", "World2\\strelka1_1.r3d", "World2\\strelka1", gtDefFixPipe, false, false, false);
	LoadDecor("World2\\strelka1_2", "World2\\strelka1_2.r3d", "World2\\strelka1", gtDefFixPipe, false, false, false);
	LoadDecor("World2\\tramplin1", "World2\\tramplin1.r3d", "World2\\tramplin1", gtDef, true, false, false);
	LoadDecor("World2\\truba1", "World2\\truba1.r3d", "World2\\truba1");
	LoadDecor("World2\\truba2", "World2\\truba2.r3d", "World2\\truba2");
	LoadDecor("World2\\truba3", "World2\\truba3.r3d", "World2\\truba3");
	LoadDecor("World2\\truba4", "World2\\truba4.r3d", "World2\\truba4");
	LoadDecor("World2\\isle1", "World2\\isle1.r3d", "World2\\isle1");
	LoadDecor("World2\\isle2", "World2\\isle2.r3d", "World2\\isle1");
	LoadDecor("World2\\isle3", "World2\\isle3.r3d", "World2\\isle1");
	LoadDecor("World2\\isle4", "World2\\isle4.r3d", "World2\\isle1");

	{
		MapObj* mapObj = NewMapObj();

		for (int i = 0; i < 4; ++i)
		{
			graph::IVBMeshNode* meshNode = AddMeshNode(mapObj, "World2\\semaphore.r3d", i);
			AddLibMat(meshNode, "World2\\semaphore");
		}

		AddToGraph(mapObj, gtDef, false, false, false, true);

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "World2\\semaphore");
	}

	D3DXPLANE plane;
	float cosAng = cos(15.0f * D3DX_PI/180.0f);
	D3DXPlaneFromPointNormal(&plane, &D3DXVECTOR3(0, 0, 6.2f), &D3DXVECTOR3(-sqrt(1.0f - cosAng * cosAng), 0.0f, cosAng));
	D3DXVECTOR4 vec1 = plane;

	D3DXVECTOR4 vec3(0.0f, -0.15f, 0.10f, 0.0f);
	
	LoadTrack("World2\\jump", "World2\\Track\\jump.r3d", "World2\\Track\\track1", true, true, true);
	LoadTrack("World2\\most", "World2\\Track\\most.r3d", "World2\\Track\\most", true, true, true, false, "", true);
	LoadTrack("World2\\track1", "World2\\Track\\track1.r3d", "World2\\Track\\track1", true, true, true, false, "World2\\Track\\pxTrack1.r3d");
	LoadTrack("World2\\track2", "World2\\Track\\track2.r3d", "World2\\Track\\track1", true, true, true, false, "World2\\Track\\pxTrack2.r3d");
	LoadTrack("World2\\track3", "World2\\Track\\track3.r3d", "World2\\Track\\track1", true, true, true, false, "World2\\Track\\pxTrack3.r3d", false, vec1, NullVec4, vec3);
	LoadTrack("World2\\track4", "World2\\Track\\track4.r3d", "World2\\Track\\track1", true, true, true, false, "World2\\Track\\pxTrack4.r3d");	
	LoadTrack("World2\\track6", "World2\\Track\\track6.r3d", "World2\\Track\\track1", true, true, true, false, "World2\\Track\\pxTrack6.r3d");	
	LoadTrack("World2\\tramp1", "World2\\Track\\tramp1.r3d", "World2\\Track\\track1", true, true, true, false, "World2\\Track\\pxTramp1.r3d");
	LoadTrack("World2\\tramp2", "World2\\Track\\tramp2.r3d", "World2\\Track\\track1", true, true, true, false, "World2\\Track\\pxTramp2.r3d", false, vec1, NullVec4, vec3);	
	LoadTrack("World2\\jumper1", "World2\\Track\\jumper1.r3d", "World2\\Track\\track1", true, true, true, false, "World2\\Track\\pxTramp1.r3d");	
	LoadTrack("World2\\jumper2", "World2\\Track\\jumper2.r3d", "World2\\Track\\track1", true, true, true, false, "World2\\Track\\pxTramp2.r3d", false, vec1, NullVec4, vec3);	

	//{
	//	MapObj* mapObj = NewMapObj();
	//	graph::IVBMeshNode* meshNode = AddMeshNode(mapObj, "World2\\Track\\track2.r3d");
	//#ifdef _DEBUG
	//	if (!meshNode->GetMesh()->GetData()->IsInit())
	//		meshNode->GetMesh()->GetData()->Load();
	//	LSL_ASSERT(meshNode->GetMesh()->GetSubsetCount() == 2);
	//#endif
	//	bool borderPxTransp = meshNode->GetMesh()->GetSubsetCount() == 2;
	//	AddLibMat(meshNode, "World2\\Track\\track2road");
	//	AddLibMat(meshNode, "World2\\Track\\track2");
	//	AddPxMesh(mapObj, "World2\\Track\\pxTrack2.r3d", borderPxTransp ? 0 : -1)->SetGroup(borderPxTransp ? px::Scene::cdgTrackPlane : 0);
	//	if (borderPxTransp)
	//		AddPxMesh(mapObj, "World2\\Track\\pxTrack2.r3d", 1)->SetGroup(px::Scene::cdgShotTransparency);
	//	AddToGraph(mapObj, gtBumb, false);
	//	SaveMapObj(mapObj, MapObjLib::ctTrack, "World2\\track2");
	//}	
}

void DataBase::LoadWorld3()
{
	LoadDecor("World3\\fabrika", "World3\\fabrika.r3d", "World3\\fabrika");
	LoadDecor("World3\\naves", "World3\\naves.r3d", "World3\\naves", gtDefFixPipe, false, true, false);
	LoadDecor("World3\\tower", "World3\\tower.r3d", "World3\\tower");
	LoadDecor("World3\\grass1", "World3\\grass1.r3d", "World3\\grass");
	LoadDecor("World3\\grass2", "World3\\grass2.r3d", "World3\\grass");
	LoadDecor("World3\\tower2", "World3\\tower2.r3d", "World3\\tower2");
	LoadDecor("World3\\ventil1", "World3\\ventil1.r3d", "World3\\ventil1");
	LoadDecor("World3\\ventil2", "World3\\ventil2.r3d", "World3\\ventil2");
	LoadDecor("World3\\windmil", "World3\\windmil.r3d", "World3\\windmil");
	LoadDecor("World3\\helpborder", "World3\\helpborder.r3d", "World3\\Track\\track");
	LoadDecor("World3\\stone1", "World3\\stone1.r3d", "World3\\stone");
	LoadDecor("World3\\stone2", "World3\\stone2.r3d", "World3\\stone");
	LoadDecor("World3\\stone3", "World3\\stone3.r3d", "World3\\stone");

	D3DXPLANE plane;
	float cosAng = cos(15.0f * D3DX_PI/180.0f);
	D3DXPlaneFromPointNormal(&plane, &D3DXVECTOR3(0, 0, 6.2f), &D3DXVECTOR3(-sqrt(1.0f - cosAng * cosAng), 0.0f, cosAng));
	D3DXVECTOR4 vec1 = plane;

	D3DXPlaneFromPointNormal(&plane, &D3DXVECTOR3(0, 0, 6.2f), &ZVector);
	D3DXVECTOR4 vec1Up = plane;

	D3DXVECTOR4 vec3(0.0f, -0.15f, 0.125f, 0.0f);

	LoadTrack("World3\\most", "World3\\Track\\most.r3d", "World3\\Track\\most", true, true, false, false, "", true);
	LoadTrack("World3\\track1", "World3\\Track\\track1.r3d", "World3\\Track\\track", true, true, false, false, "World3\\Track\\pxTrack1.r3d", false, vec1Up, NullVec4, vec3);
	LoadTrack("World3\\track2", "World3\\Track\\track2.r3d", "World3\\Track\\track", true, true, false, false, "World3\\Track\\pxTrack2.r3d");
	LoadTrack("World3\\track3", "World3\\Track\\track3.r3d", "World3\\Track\\track", true, true, false, false, "World3\\Track\\pxTrack3.r3d");
	LoadTrack("World3\\track4", "World3\\Track\\track4.r3d", "World3\\Track\\track", true, true, false, false, "World3\\Track\\pxTrack4.r3d");
	LoadTrack("World3\\podjem1", "World3\\Track\\podjem1.r3d", "World3\\Track\\track", true, true, false, false, "World3\\Track\\pxPodjem1.r3d");
	LoadTrack("World3\\podjem2", "World3\\Track\\podjem2.r3d", "World3\\Track\\track", true, true, false, false, "World3\\Track\\pxPodjem2.r3d", false, vec1, NullVec4, vec3);
}

void DataBase::LoadWorld4()
{
	LoadDecor("World4\\architect1", "World4\\architect1.r3d", "World4\\architect1");
	LoadDecor("World4\\architect2", "World4\\architect2.r3d", "World4\\architect2");
	LoadDecor("World4\\architect3_1", "World4\\architect3_1.r3d", "World4\\architect3");
	LoadDecor("World4\\architect3_2", "World4\\architect3_2.r3d", "World4\\architect3");
	LoadDecor("World4\\architect4", "World4\\architect4.r3d", "World4\\architect4");
	LoadDecor("World4\\build", "World4\\build.r3d", "World4\\build");
	LoadDecor("World4\\gora1", "World4\\gora1.r3d", "World4\\gora1");
	LoadDecor("World4\\gora2", "World4\\gora2.r3d", "World4\\gora2");
	LoadDecor("World4\\kolba", "World4\\kolba.r3d", "World4\\kolba");
	LoadDecor("World4\\lavaplace1", "World4\\lavaplace1.r3d", "World4\\lavaplace");		
	LoadDecor("World4\\lavaplace2", "World4\\lavaplace2.r3d", "World4\\lavaplace");
	LoadDecor("World4\\lavaplace3", "World4\\lavaplace3.r3d", "World4\\lavaplace");
	LoadDecor("World4\\lavaplace4", "World4\\lavaplace4.r3d", "World4\\lavaplace");
	LoadDecor("World4\\lavaplace5", "World4\\lavaplace5.r3d", "World4\\lavaplace");
	LoadDecor("World4\\pushka", "World4\\pushka.r3d", "World4\\pushka");
	LoadDecor("World4\\naves", "World4\\naves.r3d", "World4\\naves", gtDefFixPipe, false, true, false);
	LoadDecor("World4\\volcano", "World4\\volcano.r3d", "World4\\volcano");
	LoadDecor("World4\\crystals", "World4\\crystals.r3d", "World4\\crystals");

	D3DXPLANE plane;
	float cosAng = cos(15.0f * D3DX_PI/180.0f);
	D3DXPlaneFromPointNormal(&plane, &D3DXVECTOR3(0, 0, 6.2f), &D3DXVECTOR3(-sqrt(1.0f - cosAng * cosAng), 0.0f, cosAng));
	D3DXVECTOR4 vec1 = plane;

	D3DXVECTOR4 vec3(0.0f, -0.25f, 0.25f, 0.0f);	

	LoadTrack("World4\\most", "World4\\Track\\most.r3d", "World4\\Track\\track1", true, true, false, false, "World4\\Track\\pxMost.r3d", true);
	LoadTrack("World4\\most2", "World4\\Track\\most2.r3d", "World4\\Track\\most2", true, true, false, false, "World4\\Track\\pxMost2.r3d", true);
	LoadTrack("World4\\track1", "World4\\Track\\track1.r3d", "World4\\Track\\track1", true, true, false, false, "World4\\Track\\pxTrack1.r3d");
	LoadTrack("World4\\track2", "World4\\Track\\track2.r3d", "World4\\Track\\track2", true, true, false, false, "World4\\Track\\pxTrack2.r3d");
	LoadTrack("World4\\track3", "World4\\Track\\track3.r3d", "World4\\Track\\track1", true, true, false, false, "World4\\Track\\pxTrack3.r3d");
	LoadTrack("World4\\track4", "World4\\Track\\track4.r3d", "World4\\Track\\track1", true, true, false, false, "World4\\Track\\pxTrack4.r3d");
	LoadTrack("World4\\track5", "World4\\Track\\track5.r3d", "World4\\Track\\track1", true, true, false, false, "World4\\Track\\pxTrack5.r3d", false, vec1, NullVec4, vec3);
	LoadTrack("World4\\podjem1", "World4\\Track\\podjem1.r3d", "World4\\Track\\track1", true, true, false, false, "World4\\Track\\pxPodjem1.r3d", false, vec1, NullVec4, vec3);
	LoadTrack("World4\\podjem2", "World4\\Track\\podjem2.r3d", "World4\\Track\\track1", true, true, false, false, "World4\\Track\\pxPodjem2.r3d");
	LoadTrack("World4\\jumper1", "World4\\Track\\jumper1.r3d", "World4\\Track\\track1", true, true, false, false, "World4\\Track\\pxPodjem1.r3d", false, vec1, NullVec4, vec3);
	LoadTrack("World4\\jumper2", "World4\\Track\\jumper2.r3d", "World4\\Track\\track1", true, true, false, false, "World4\\Track\\pxPodjem2.r3d");
}

void DataBase::LoadWorld5()
{
	LoadDecor("World5\\cannon", "World5\\cannon.r3d", "World5\\cannon");
	LoadDecor("World5\\cannon2", "World5\\cannon2.r3d", "World5\\cannon2");
	LoadDecor("World5\\cannon3", "World5\\cannon3.r3d", "World5\\cannon3");
	LoadDecor("World5\\mountain", "World5\\mountain.r3d", "World5\\mountain");
	LoadDecor("World5\\naves", "World5\\naves.r3d", "World5\\naves", gtDefFixPipe, false, true, false);
	LoadDecor("World5\\snowPlate", "World5\\snowPlate.r3d", "World5\\snowPlate");
	LoadDecor("World5\\snowstone", "World5\\snowstone.r3d", "World5\\snowstone");
	LoadDecor("World5\\snowstone2", "World5\\snowstone2.r3d", "World5\\snowstone2");
	LoadDecor("World5\\snowTree", "World5\\snowTree.r3d", "World5\\treeSnow");
	LoadDecor("World5\\transportship", "World5\\transportship.r3d", "World5\\transportship");
	LoadDecor("World5\\piece", "World5\\piece.r3d", "World5\\piece");

	D3DXPLANE plane;

	D3DXPlaneFromPointNormal(&plane, &D3DXVECTOR3(0, 0, 6.2f), &ZVector);
	D3DXVECTOR4 plane1 = plane;

	float cosAng = cos(20.0f * D3DX_PI/180.0f);
	D3DXPlaneFromPointNormal(&plane, &D3DXVECTOR3(0, 0, 6.2f), &D3DXVECTOR3(-sqrt(1.0f - cosAng * cosAng), 0.0f, cosAng));
	D3DXVECTOR4 plane2 = plane;

	D3DXVECTOR4 vec3(0.0f, -0.15f, 0.125f, 0.0f);

	LoadTrack("World5\\most", "World5\\Track\\most.r3d", "World5\\Track\\most", true, true, false, false, "", true);	
	LoadTrack("World5\\trackVentil", "World5\\Track\\trackVentil.r3d", "World5\\Track\\track1");
	LoadTrack("World5\\track1", "World5\\Track\\track1.r3d", "World5\\Track\\track1", true, true, false, true, "World5\\Track\\PXtrack1.r3d", false, plane1, NullVec4, vec3);
	LoadTrack("World5\\track2", "World5\\Track\\track2.r3d", "World5\\Track\\track1", true, true, false, true, "World5\\Track\\PXtrack2.r3d", false, plane1);
	LoadTrack("World5\\track3", "World5\\Track\\track3.r3d", "World5\\Track\\track1", true, true, false, true, "World5\\Track\\PXtrack3.r3d", false, plane1);
	LoadTrack("World5\\track4", "World5\\Track\\track4.r3d", "World5\\Track\\track1", true, true, false, true, "World5\\Track\\PXtrack4.r3d", false, plane1);
	LoadTrack("World5\\podjem1", "World5\\Track\\podjem1.r3d", "World5\\Track\\track1", true, true, false, true, "World5\\Track\\PXpodjem1.r3d", false, plane1);
	LoadTrack("World5\\podjem2", "World5\\Track\\podjem2.r3d", "World5\\Track\\track1", true, true, false, true, "World5\\Track\\PXpodjem2.r3d", false, plane2, NullVec4, vec3);
	LoadTrack("World5\\podjem3", "World5\\Track\\podjem3.r3d", "World5\\Track\\track1", true, true, false, true, "World5\\Track\\PXpodjem3.r3d", false, plane2, NullVec4, vec3);
	LoadTrack("World5\\tramp1", "World5\\Track\\tramp1.r3d", "World5\\Track\\track1", true, true, false, true, "World5\\Track\\PXpodjem1.r3d", false, plane1);
	LoadTrack("World5\\tramp2", "World5\\Track\\tramp2.r3d", "World5\\Track\\track1", true, true, false, true, "World5\\Track\\PXpodjem2.r3d", false, plane2, NullVec4, vec3);
}

void DataBase::LoadWorld6()
{
	LoadDecor("World6\\haus1", "World6\\haus1.r3d", "World6\\haus1");
	LoadDecor("World6\\haus2", "World6\\haus2.r3d", "World6\\haus2");
	LoadDecor("World6\\haus3", "World6\\haus3.r3d", "World6\\haus3");
	LoadDecor("World6\\haus4", "World6\\haus4.r3d", "World6\\haus4");
	LoadDecor("World6\\naves", "World6\\naves.r3d", "World6\\naves", gtDefFixPipe, false, true, false);	
	LoadDecor("World6\\nuke", "World6\\nuke.r3d", "World6\\nuke");	
	LoadDecor("World6\\stone1", "World6\\stone1.r3d", "World6\\stone");
	LoadDecor("World6\\stone2", "World6\\stone2.r3d", "World6\\stone");
	LoadDecor("World6\\stone3", "World6\\stone3.r3d", "World6\\stone");

	D3DXPLANE plane;
	float cosAng = cos(15.0f * D3DX_PI/180.0f);
	D3DXPlaneFromPointNormal(&plane, &D3DXVECTOR3(0, 0, 6.2f), &D3DXVECTOR3(-sqrt(1.0f - cosAng * cosAng), 0.0f, cosAng));
	D3DXVECTOR4 vec1 = plane;

	D3DXPlaneFromPointNormal(&plane, &D3DXVECTOR3(0, 0, 6.2f), &ZVector);
	D3DXVECTOR4 vec1Up = plane;

	D3DXVECTOR4 vec3(0.0f, -0.15f, 0.125f, 0.0f);

	LoadTrack("World6\\most", "World6\\Track\\most.r3d", "World6\\Track\\track1", true, true, false, false, "World6\\Track\\pxMost.r3d");
	LoadTrack("World6\\track1", "World6\\Track\\track1.r3d", "World6\\Track\\track1", true, true, false, false, "");
	LoadTrack("World6\\track2", "World6\\Track\\track2.r3d", "World6\\Track\\track1", true, true, false, false, "");
	LoadTrack("World6\\track3", "World6\\Track\\track3.r3d", "World6\\Track\\track1", true, true, false, false, "");
	LoadTrack("World6\\track4", "World6\\Track\\track4.r3d", "World6\\Track\\track1", true, true, false, false, "");
	LoadTrack("World6\\track5", "World6\\Track\\track5.r3d", "World6\\Track\\track1", true, true, false, false, "");			
	LoadTrack("World6\\entertunnel", "World6\\Track\\entertunnel.r3d", "World6\\Track\\track1", true, true, false, false, "World6\\Track\\pxEnterTunnel.r3d");
	LoadTrack("World6\\tunnel", "World6\\Track\\tunnel.r3d", "World6\\Track\\tonnel", true, true, false, false, "World6\\Track\\pxTunel.r3d");
	LoadTrack("World6\\podjem1", "World6\\Track\\podjem1.r3d", "World6\\Track\\track1", false, false, false, false, "");

	LoadTrack("World6\\tramp1", "World6\\Track\\tramp1.r3d", "World6\\Track\\tramplin3", false, false, false, false, "");
	LoadTrack("World6\\tramp2", "World6\\Track\\tramp2.r3d", "World6\\Track\\tramplin3", false, false, false, false, "");	
	LoadTrack("World6\\jump", "World6\\Track\\jump.r3d", "World6\\Track\\track1", false, false, false, false, "");
	LoadTrack("World6\\block1", "World6\\Track\\block1.r3d", "World6\\Track\\track1", false, false, false, false, "");
	LoadTrack("World6\\block2", "World6\\Track\\block2.r3d", "World6\\Track\\track1", false, false, false, false, "");	
}

void DataBase::LoadMisc()
{
	//bulletProj
	{
		game::MapObj* mapObj = NewMapObj();
		AddToGraph(mapObj, gtEffect, true);

		graph::Sprite* node = AddSprite(mapObj, true, D3DXVECTOR2(2.0f, 0.5f));
		AddLibMat(&node->material, "Effect\\bullet");

		{
			DeathEffect& deathEff = mapObj->GetGameObj().GetBehaviors().Add<DeathEffect>();			
			deathEff.SetEffect(GetRecord(MapObjLib::ctEffects, "death5"));			
			deathEff.SetTargetChild(true);
		}

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\bulletProj");
	}

	//rifleProj
	{
		game::MapObj* mapObj = NewMapObj();
		AddToGraph(mapObj, gtEffect, true);

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\rifleProj.r3d");
		AddLibMat(node, "Weapon\\rifleProj");

		{
			MapObj* child = &mapObj->GetGameObj().GetIncludeList().Add(gotGameObj);
			AddToGraph(child, gtEffect, true);
			child->GetGameObj().SetPos(D3DXVECTOR3(-0.4f, 0.0f, 0.0f));

			
			{
				graph::FxParticleSystem* fxSystem = AddFxSystem(child, _fxSpriteManager);
				AddLibMat(&fxSystem->material, "Effect\\dust_smoke_06");			

				graph::FxEmitter::ParticleDesc desc;
				desc.maxNum = 100;
				desc.startType = graph::FxEmitter::sotDist;
				desc.life = 0.5f;
				desc.startTime = 0.25f;
				desc.density = 1.0f;
				desc.startScale = IdentityVector * 0.7f;		

				graph::FxFlowEmitter::FlowDesc descFlow;
				descFlow.speedPos = -XVector * 5.0f;
				descFlow.speedScale = -IdentityVector * 0.5f;

				graph::FxFlowEmitter* emitter = AddFxFlowEmitter(fxSystem, desc, descFlow, false);
			}

			child->GetGameObj().GetBehaviors().Add<FxSystemWaitingEnd>();
		}

		{
			DeathEffect& deathEff = mapObj->GetGameObj().GetBehaviors().Add<DeathEffect>();			
			deathEff.SetEffect(GetRecord(MapObjLib::ctEffects, "death6"));			
			deathEff.SetTargetChild(true);
		}

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\rifleProj");
	}

	//blaster
	{
		game::MapObj* mapObj = NewMapObj();
		AddToGraph(mapObj, gtEffect, true);	

		graph::FxParticleSystem* fxSystem = AddFxSystem(mapObj, _fxSpriteManager);
		AddLibMat(&fxSystem->material, "Effect\\blaster");

		graph::FxEmitter::ParticleDesc desc;
		desc.maxNum = 100;
		desc.startType = graph::FxEmitter::sotDist;
		desc.life = 0.4f;
		desc.startTime = 0.5f;
		desc.density = 1.0f;
		desc.startPos = Vec3Range(IdentityVector * (-0.1f), IdentityVector * 0.1f, Vec3Range::vdVolume);
		desc.startScale = Vec3Range(IdentityVector * 0.9f, IdentityVector * 1.1f);

		graph::FxFlowEmitter::FlowDesc descFlow;
		descFlow.speedScale = -IdentityVector * 2.0f;

		mapObj->GetGameObj().GetBehaviors().Add<FxSystemWaitingEnd>();
		graph::FxFlowEmitter* emitter = AddFxFlowEmitter(fxSystem, desc, descFlow, true);

		{
			DeathEffect& deathEff = mapObj->GetGameObj().GetBehaviors().Add<DeathEffect>();			
			deathEff.SetEffect(GetRecord(MapObjLib::ctEffects, "death6"));			
			deathEff.SetTargetChild(true);
		}

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\blaster");
	}

	//rocket
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();
		
		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\rocket.r3d");
		node->SetScale(0.8f);
		AddLibMat(node, "Weapon\\rocket");
		AddToGraph(mapObj, gtDefFixPipe, true);
		
		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "smoke2");
			eff->GetGameObj().SetPos(D3DXVECTOR3(-0.5f, 0.0f, 0.0f));
			eff->GetGameObj().GetBehaviors().Add<FxSystemWaitingEnd>();
			//eff->GetGameObj().rescOnTime = 1.5f;
			//eff->GetGameObj().sendToDeathList = true;
		}
		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "flare1");
			eff->GetGameObj().SetPos(D3DXVECTOR3(-1.1f, 0.0f, 0.0f));
			eff->GetGameObj().SetScale(2.0f);
		}
		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "engine1");
			eff->GetGameObj().SetPos(D3DXVECTOR3(-2.3f, 0.0f, 0.05f));	
		}
		{
			DeathEffect& deathEff = mapObj->GetGameObj().GetBehaviors().Add<DeathEffect>();			
			deathEff.SetEffect(GetRecord(MapObjLib::ctEffects, "death5"));
			deathEff.SetTargetChild(true);
		}

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\rocket");
	}

	//rocketAir
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\rocketAir.r3d");		
		AddLibMat(node, "Weapon\\rocketAir");
		AddToGraph(mapObj, gtDefFixPipe, true);

		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "smoke4");
			eff->GetGameObj().SetPos(D3DXVECTOR3(-0.5f, 0.0f, 0.0f));
			eff->GetGameObj().GetBehaviors().Add<FxSystemWaitingEnd>();
		}
		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "flare1");
			eff->GetGameObj().SetPos(D3DXVECTOR3(-1.1f, 0.0f, 0.0f));
			eff->GetGameObj().SetScale(2.0f);
		}
		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "engine1");
			eff->GetGameObj().SetPos(D3DXVECTOR3(-2.3f, 0.0f, 0.05f));	
		}
		{
			DeathEffect& deathEff = mapObj->GetGameObj().GetBehaviors().Add<DeathEffect>();			
			deathEff.SetEffect(GetRecord(MapObjLib::ctEffects, "death5"));			
			deathEff.SetTargetChild(true);
		}

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\rocketAir");
	}

	//thunder
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		graph::IVBMeshNode* mesh = AddMeshNode(mapObj, "Effect\\sphere.r3d");
		AddLibMat(mesh, "Effect\\gravBall");
		mesh->SetScale(0.25f);
		AddToGraph(mapObj, gtDefFixPipe, true);

		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "smoke5");
			eff->GetGameObj().SetPos(D3DXVECTOR3(0.0f, 0.0f, 0.0f));
			eff->GetGameObj().GetBehaviors().Add<FxSystemWaitingEnd>();
		}
		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "flare1");			
			eff->GetGameObj().SetScale(1.5f);
		}		
		{
			DeathEffect& deathEff = mapObj->GetGameObj().GetBehaviors().Add<DeathEffect>();			
			deathEff.SetEffect(GetRecord(MapObjLib::ctEffects, "death6"));
			deathEff.SetTargetChild(true);
		}

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\thunder");
	}

	//phaserBolt
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();		

		graph::IVBMeshNode* mesh = AddMeshNode(mapObj, "Effect\\sphere.r3d");
		AddLibMat(mesh, "Effect\\phaserBolt");
		mesh->animMode(graph::SceneNode::amRepeat);
		mesh->SetScale(0.45f);
		mesh->animDuration = 5.5f;
		AddToGraph(mapObj, gtDefFixPipe, true);

		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "smoke8");
			eff->GetGameObj().SetPos(D3DXVECTOR3(0.0f, 0.0f, 0.0f));
			eff->GetGameObj().GetBehaviors().Add<FxSystemWaitingEnd>();
		}
		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "flare1");			
			eff->GetGameObj().SetScale(1.5f);
		}		
		{
			DeathEffect& deathEff = mapObj->GetGameObj().GetBehaviors().Add<DeathEffect>();			
			deathEff.SetEffect(GetRecord(MapObjLib::ctEffects, "death6"));
			deathEff.SetTargetChild(true);
		}

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\phaserBolt");
	}

	//torpeda
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\torpeda.r3d");
		AddLibMat(node, "Weapon\\torpeda");
		AddToGraph(mapObj, gtDefFixPipe, true);

		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "flare3");
			eff->GetGameObj().SetPos(D3DXVECTOR3(-1.0f, 0.0f, 0.0f));
			eff->GetGameObj().SetScale(2.0f);
		}
		{
			DeathEffect& deathEff = mapObj->GetGameObj().GetBehaviors().Add<DeathEffect>();			
			deathEff.SetEffect(GetRecord(MapObjLib::ctEffects, "death5"));			
			deathEff.SetTargetChild(true);
		}
		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "heatTrail");	
		}

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\torpeda");
	}

	//resonanse
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		AddFxSprite(mapObj, "Effect\\flare3", NullVector, Vec3Range(IdentityVector, NullVector), NullQuaternion, false, graph::SceneNode::amNone, 0, 0.0f, false, IdentityVec2);
		AddToGraph(mapObj, gtEffect, true);

		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "resonanseBolt");
			eff->GetGameObj().SetPos(D3DXVECTOR3(0.0f, 0.5f, 0.0f));
		}
		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "resonanseBolt");
			eff->GetGameObj().SetPos(D3DXVECTOR3(0.0f, -0.5f, 0.0f));
		}
		{
			DeathEffect& deathEff = mapObj->GetGameObj().GetBehaviors().Add<DeathEffect>();			
			deathEff.SetEffect(GetRecord(MapObjLib::ctEffects, "death5"));			
			deathEff.SetTargetChild(true);
		}

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\resonanse");
	}

	//mine1
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\mine1.r3d");
		AddLibMat(node, "Weapon\\mine1");
		AddToGraph(mapObj, gtDefFixPipe, true);

		DeathEffect& deathEffect = gameObj.GetBehaviors().Add<DeathEffect>();
		deathEffect.SetEffect(GetRecord(MapObjLib::ctEffects, "death1"));

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\mine1");
	}

	//mine2
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\mine2.r3d");
		AddLibMat(node, "Weapon\\mine2");
		AddToGraph(mapObj, gtDefFixPipe, true);

		DeathEffect& deathEffect = gameObj.GetBehaviors().Add<DeathEffect>();
		deathEffect.SetEffect(GetRecord(MapObjLib::ctEffects, "death3"));
		deathEffect.SetIgnoreRot(true);
		deathEffect.SetPos(ZVector * 0.5f);

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\mine2");
	}
	//mine2Kern
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\mine2.r3d", 0);
		AddLibMat(node, "Weapon\\mine2");
		AddToGraph(mapObj, gtDefFixPipe, true);

		DeathEffect& deathEffect = gameObj.GetBehaviors().Add<DeathEffect>();
		deathEffect.SetEffect(GetRecord(MapObjLib::ctEffects, "death3"));
		deathEffect.SetIgnoreRot(true);
		deathEffect.SetPos(ZVector * 0.5f);

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\mine2Kern");
	}
	//mine2Piece
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\mine2Piece.r3d", 0);
		AddLibMat(node, "Weapon\\mine2");
		AddToGraph(mapObj, gtDefFixPipe, true);

		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "flare2");
			eff->GetGameObj().SetPos(D3DXVECTOR3(0.0f, 0.0f, 0.25f));
		}

		DeathEffect& deathEffect = gameObj.GetBehaviors().Add<DeathEffect>();
		deathEffect.SetEffect(GetRecord(MapObjLib::ctEffects, "death3"));
		deathEffect.SetIgnoreRot(true);
		deathEffect.SetPos(ZVector * 0.5f);

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\mine2Piece");
	}

	//mine3
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\mine3.r3d");
		AddLibMat(node, "Weapon\\mine3");
		AddToGraph(mapObj, gtDefFixPipe, true);

		{
			MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "ringWay");
			eff->GetGameObj().SetPos(ZVector * 0.1f);
		}	

		DeathEffect& deathEffect = gameObj.GetBehaviors().Add<DeathEffect>();
		deathEffect.SetEffect(GetRecord(MapObjLib::ctEffects, "death4"));
		deathEffect.SetIgnoreRot(true);
		
		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\mine3");
	}

	//medpack
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Bonus\\medpack.r3d");
		AddLibMat(node, "Bonus\\medpack");
		AddToGraph(mapObj, gtDefFixPipe, true);

		{
			DeathEffect& deathEffect = mapObj->GetGameObj().GetBehaviors().Add<DeathEffect>();
			deathEffect.SetEffect(GetRecord(MapObjLib::ctEffects, "Snd\\klicka5"));
		}

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\medpack");
	}
	//ammo
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Bonus\\ammo.r3d");
		AddLibMat(node, "Bonus\\ammo");
		AddToGraph(mapObj, gtDefFixPipe, true);

		{
			DeathEffect& deathEffect = mapObj->GetGameObj().GetBehaviors().Add<DeathEffect>();
			deathEffect.SetEffect(GetRecord(MapObjLib::ctEffects, "Snd\\klicka5"));
		}

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\ammo");
	}
	//money
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Bonus\\money.r3d");
		AddLibMat(node, "Bonus\\money");
		AddToGraph(mapObj, gtDefFixPipe, true);

		{
			DeathEffect& deathEffect = mapObj->GetGameObj().GetBehaviors().Add<DeathEffect>();
			deathEffect.SetEffect(GetRecord(MapObjLib::ctEffects, "Snd\\klicka5"));
		}

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\money");
	}
	//shield
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Bonus\\shield.r3d");
		AddLibMat(node, "Bonus\\shield");
		AddToGraph(mapObj, gtDefFixPipe, true);

		{
			DeathEffect& deathEffect = mapObj->GetGameObj().GetBehaviors().Add<DeathEffect>();
			deathEffect.SetEffect(GetRecord(MapObjLib::ctEffects, "Snd\\shieldOn"));
		}

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\shield");
	}
	//speedArrow
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		graph::PlaneNode* node = AddPlaneNode(mapObj, IdentityVec2);
		node->SetScale(D3DXVECTOR3(5.0f, 3.0f, 1.0f));
		AddLibMat(&node->material, "Bonus\\speedArrow");
		AddToGraph(mapObj, gtEffect, true);

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\speedArrow");
	}
	//strelkaAnim
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		graph::PlaneNode* node = AddPlaneNode(mapObj, IdentityVec2);
		//D3DXQUATERNION rot;
		//D3DXQuaternionRotationAxis(&rot, &ZVector, -D3DX_PI/2);
		//node->SetRot(rot);
		node->SetScale(D3DXVECTOR3(4.0f, 3.0f, 1.0f));
		node->animMode(graph::SceneNode::amRepeat);
		node->animDuration = 1.0f;
		AddLibMat(&node->material, "Bonus\\strelkaAnim");
		AddToGraph(mapObj, gtEffect, true);

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\strelkaAnim");
	}
	//lusha
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		graph::PlaneNode* node = AddPlaneNode(mapObj, IdentityVec2);
		node->SetScale(D3DXVECTOR3(4.5f, 4.0f, 1.0f));
		AddLibMat(&node->material, "Bonus\\lusha");
		AddToGraph(mapObj, gtEffect, true);

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\lusha");
	}
	//snowLusha
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		graph::PlaneNode* node = AddPlaneNode(mapObj, IdentityVec2);
		node->SetScale(D3DXVECTOR3(4.0f, 4.0f, 1.0f));
		AddLibMat(&node->material, "Bonus\\snowLusha");
		AddToGraph(mapObj, gtEffect, true);

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\snowLusha");
	}
	//hellLusha
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		graph::PlaneNode* node = AddPlaneNode(mapObj, IdentityVec2);
		node->SetScale(D3DXVECTOR3(4.0f, 4.0f, 1.0f));
		AddLibMat(&node->material, "Bonus\\hellLusha");
		AddToGraph(mapObj, gtEffect, true);

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\hellLusha");
	}
	//maslo
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		graph::PlaneNode* node = AddPlaneNode(mapObj, IdentityVec2);
		node->SetScale(D3DXVECTOR3(2.5f, 2.5f, 1.0f));
		AddLibMat(&node->material, "Bonus\\maslo");
		AddToGraph(mapObj, gtEffect, true);

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\maslo");
	}

	//laserPulse
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "laserBlue");
		}
		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "flare5");
			eff->GetGameObj().SetPos(D3DXVECTOR3(-2.5f, 0.0f, 0.0f));
		}
		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "flare5");
			eff->GetGameObj().SetPos(D3DXVECTOR3(2.5f, 0.0f, 0.0f));
		}
		{
			DeathEffect& deathEffect = gameObj.GetBehaviors().Add<DeathEffect>();
			deathEffect.SetEffect(GetRecord(MapObjLib::ctEffects, "streak1"));
			deathEffect.SetTargetChild(true);
		}

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\laserPulse");
	}
	//spherePulse
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "flare4");
		}
		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "lens1");
		}				
		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "lightning1");
		}					
		{
			DeathEffect& deathEffect = gameObj.GetBehaviors().Add<DeathEffect>();
			deathEffect.SetEffect(GetRecord(MapObjLib::ctEffects, "blink"));	
			deathEffect.SetSound(&GetSound("Sounds\\spherePulseDeath.ogg"));
			deathEffect.SetTargetChild(true);
		}
		

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\spherePulse");
	}

	//shotBall
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\shotBall.r3d");
		node->SetScale(2.0f);
		AddLibMat(node, "Weapon\\shotBall");
		AddToGraph(mapObj, gtDefFixPipe, true);

		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "fireTrail");
			eff->GetGameObj().GetBehaviors().Add<FxSystemWaitingEnd>();			
		}
		{
			game::MapObj* eff = NewChildMapObj(mapObj, MapObjLib::ctEffects, "firePatron");		
		}		
		{
			DeathEffect& deathEff = mapObj->GetGameObj().GetBehaviors().Add<DeathEffect>();			
			deathEff.SetEffect(GetRecord(MapObjLib::ctEffects, "death5"));			
			deathEff.SetTargetChild(true);
		}

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\shotBall");
	}

	{
		MapObj* mapObj = NewMapObj();

		graph::IVBMeshNode* meshNode = AddMeshNode(mapObj, "GUI\\garage.r3d");
		AddLibMat(meshNode, "GUI\\garage1");
		AddLibMat(meshNode, "GUI\\garage2");
		AddToGraph(mapObj, gtDef, false);

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\garage");
	}

	{
		MapObj* mapObj = NewMapObj();

		graph::IVBMeshNode* meshNode = AddMeshNode(mapObj, "GUI\\angar.r3d");
		AddLibMat(meshNode, "GUI\\angar1");
		AddLibMat(meshNode, "GUI\\angar2");
		AddToGraph(mapObj, gtDef, false);

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\angar");
	}

	{
		MapObj* mapObj = NewMapObj();

		graph::IVBMeshNode* meshNode = AddMeshNode(mapObj, "GUI\\question.r3d");
		AddLibMat(meshNode, "GUI\\question");
		AddToGraph(mapObj, gtDef, false, false, true);

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\question");
	}

	{
		MapObj* mapObj = NewMapObj();

		graph::PlaneNode* node = AddPlaneNode(mapObj, IdentityVec2);		
		AddLibMat(&node->material, "GUI\\space2");
		AddToGraph(mapObj, gtDefFixPipe, false);

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Misc\\space2");
	}
}

void DataBase::LoadCrush()
{
	//poster
	{
		int staticMeshes[] = {0};
		LoadCrushObj("Crush\\poster", "Crush\\pregrada.r3d", "Crush\\pregrada", 200.0f, 4, staticMeshes, 1);
	}
	//crush1
	{
		int staticMeshes[] = {0, 1};
		LoadCrushObj("Crush\\crush1", "Crush\\crush1.r3d", "Crush\\crush1", 200.0f, 14, staticMeshes, 2);
	}
	//crush2
	{
		int staticMeshes[] = {0, 1};
		LoadCrushObj("Crush\\crush2", "Crush\\crush2.r3d", "Crush\\crush2", 200.0f, 12, staticMeshes, 2);
	}
	//reklama
	{
		int staticMeshes[] = {0};
		LoadCrushObj("Crush\\reklama", "Crush\\reklama.r3d", "Crush\\reklama", 200.0f, 11, staticMeshes, 1, true);
		LoadCrushObj("Crush\\reklama2", "Crush\\reklama.r3d", "Crush\\reklama2", 200.0f, 11, staticMeshes, 1, true);
		LoadCrushObj("Crush\\reklama3", "Crush\\reklama.r3d", "Crush\\reklama3", 200.0f, 11, staticMeshes, 1, true);
	}

	LoadCrushObj("Crush\\box", "Crush\\box.r3d", "Crush\\box", 200.0f, 12, NULL, 0);

	//bochka
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();	

		graph::IVBMeshNode* mesh = AddMeshNode(mapObj, "Crush\\bochka.r3d");
		AddLibMat(mesh, "Crush\\bochka");
		AddToGraph(mapObj, gtDefFixPipe, true);

		//px::CapsuleShape* shape = AddPxCapsule(mapObj);
		px::BoxShape* shape = AddPxBox(mapObj);
		shape->SetSkinWidth(0.1f);

		AddPxBody(mapObj, 200.0f, 0);

		mapObj->GetGameObj().GetPxActor().SetScene(_world->GetPxScene());

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Crush\\bochka");		
	}

	//znak
	{
		game::MapObj* mapObj = NewMapObj();
		game::GameObject& gameObj = mapObj->GetGameObj();	

		graph::IVBMeshNode* mesh = AddMeshNode(mapObj, "Crush\\znak.r3d");
		AddLibMat(mesh, "Crush\\znak");
		AddToGraph(mapObj, gtDefFixPipe, true);

		px::BoxShape* shape = AddPxBox(mapObj);
		shape->SetSkinWidth(0.1f);

		AddPxBody(mapObj, 200.0f, 0);

		mapObj->GetGameObj().GetPxActor().SetScene(_world->GetPxScene());

		SaveMapObj(mapObj, MapObjLib::ctDecoration, "Crush\\znak");		
	}
}

void DataBase::LoadBonus()
{
	//mineSpike
	{
		game::MapObj* mapObj = NewMapObj();
		game::AutoProj& gameObj = mapObj->SetGameObj<game::AutoProj>();
		//необходимо для рендера BB
		AddToGraph(mapObj, gtDefFixPipe, true);

		AutoProj::Desc desc;

		desc.type = Proj::ptMine;
		desc.SetModel(GetRecord(MapObjLib::ctDecoration, "Misc\\mine1"));
		desc.pos = NullVector;
		desc.size = D3DXVECTOR3(0.0f, 0.0f, 1.7f);
		desc.modelSize = true;
		desc.damage = 11.0f;
		desc.maxDist = 0.0f;
		desc.speed = 3000.0f;

		gameObj.SetDesc(desc);
		gameObj.GetPxActor().SetScene(_world->GetPxScene());

		SaveMapObj(mapObj, MapObjLib::ctBonus, "mineSpike");
	}
	//mineRipKern
	{
		game::MapObj* mapObj = NewMapObj();
		game::AutoProj& gameObj = mapObj->SetGameObj<game::AutoProj>();
		//необходимо для рендера BB
		AddToGraph(mapObj, gtDefFixPipe, true);

		AutoProj::Desc desc;

		desc.type = Proj::ptMine;
		desc.SetModel(GetRecord(MapObjLib::ctDecoration, "Misc\\mine2Kern"));
		desc.pos = NullVector;
		desc.size = D3DXVECTOR3(0.0f, 0.0f, 1.7f);
		desc.modelSize = true;
		desc.damage = 10.0f;
		desc.maxDist = 0.0f;
		desc.speed = 3000.0f;
		desc.minTimeLife = FloatRange(4.0f, 4.5f);

		gameObj.SetDesc(desc);
		gameObj.GetPxActor().SetScene(_world->GetPxScene());

		SaveMapObj(mapObj, MapObjLib::ctBonus, "mineRipKern");
	}	
	//mineRipPiece
	{
		game::MapObj* mapObj = NewMapObj();
		game::AutoProj& gameObj = mapObj->SetGameObj<game::AutoProj>();
		//необходимо для рендера BB
		AddToGraph(mapObj, gtDefFixPipe, true);

		AutoProj::Desc desc;

		desc.type = Proj::ptMinePiece;
		desc.SetModel(GetRecord(MapObjLib::ctDecoration, "Misc\\mine2Piece"));
		desc.pos = NullVector;
		desc.size = D3DXVECTOR3(1.0f, 1.0f, 1.7f);
		desc.modelSize = true;
		desc.damage = 4.0f;
		desc.maxDist = 0.0f;
		desc.speed = 3000.0f;
		desc.minTimeLife = FloatRange(4.0f, 4.5f);

		gameObj.SetDesc(desc);
		gameObj.GetPxActor().SetScene(_world->GetPxScene());

		SaveMapObj(mapObj, MapObjLib::ctBonus, "mineRipPiece");
	}	
	//mineProton
	{
		game::MapObj* mapObj = NewMapObj();
		game::AutoProj& gameObj = mapObj->SetGameObj<game::AutoProj>();
		//необходимо для рендера BB
		AddToGraph(mapObj, gtDefFixPipe, true);

		AutoProj::Desc desc;

		desc.type = Proj::ptMineProton;
		desc.SetModel(GetRecord(MapObjLib::ctDecoration, "Misc\\mine3"));
		desc.pos = D3DXVECTOR3(0.0f, 0.0f, 0.2f);
		desc.size = D3DXVECTOR3(5.0f, 5.0f, 1.7f);
		desc.modelSize = false;
		desc.damage = 25.0f;
		desc.maxDist = 0.0f;
		desc.speed = 3000.0f;
		//desc.angleSpeed = 1.0f;

		gameObj.SetDesc(desc);
		gameObj.GetPxActor().SetScene(_world->GetPxScene());

		SaveMapObj(mapObj, MapObjLib::ctBonus, "mineProton");
	}

	//medpack
	{
		game::MapObj* mapObj = NewMapObj();
		game::AutoProj& gameObj = mapObj->SetGameObj<game::AutoProj>();
		//необходимо для рендера BB
		AddToGraph(mapObj, gtDefFixPipe, true);

		AutoProj::Desc desc;

		desc.type = Proj::ptMedpack;
		desc.SetModel(GetRecord(MapObjLib::ctDecoration, "Misc\\medpack"));
		desc.pos = NullVector;
		desc.size = D3DXVECTOR3(0.0f, 0.0f, 1.7f);
		desc.modelSize = true;
		desc.damage = 0.0f;
		desc.maxDist = 0.0f;

		gameObj.SetDesc(desc);
		gameObj.GetPxActor().SetScene(_world->GetPxScene());

		SaveMapObj(mapObj, MapObjLib::ctBonus, "medpack");
	}
	//money
	{
		game::MapObj* mapObj = NewMapObj();
		game::AutoProj& gameObj = mapObj->SetGameObj<game::AutoProj>();
		//необходимо для рендера BB
		AddToGraph(mapObj, gtDefFixPipe, true);

		AutoProj::Desc desc;

		desc.type = Proj::ptMoney;
		desc.SetModel(GetRecord(MapObjLib::ctDecoration, "Misc\\money"));
		desc.pos = NullVector;
		desc.size = D3DXVECTOR3(0.0f, 0.0f, 1.7f);
		desc.modelSize = true;
		desc.damage = 2000.0f;
		desc.maxDist = 0.0f;		

		gameObj.SetDesc(desc);
		gameObj.GetPxActor().SetScene(_world->GetPxScene());

		SaveMapObj(mapObj, MapObjLib::ctBonus, "money");
	}
	//ammo
	{
		game::MapObj* mapObj = NewMapObj();
		game::AutoProj& gameObj = mapObj->SetGameObj<game::AutoProj>();
		//необходимо для рендера BB
		AddToGraph(mapObj, gtDefFixPipe, true);

		AutoProj::Desc desc;

		desc.type = Proj::ptCharge;
		desc.SetModel(GetRecord(MapObjLib::ctDecoration, "Misc\\ammo"));
		desc.pos = NullVector;
		desc.size = D3DXVECTOR3(0.0f, 0.0f, 1.7f);
		desc.modelSize = true;
		desc.damage = 1.0f;
		desc.maxDist = 0.0f;		

		gameObj.SetDesc(desc);
		gameObj.GetPxActor().SetScene(_world->GetPxScene());

		SaveMapObj(mapObj, MapObjLib::ctBonus, "ammo");
	}
	//shield
	{
		game::MapObj* mapObj = NewMapObj();
		game::AutoProj& gameObj = mapObj->SetGameObj<game::AutoProj>();
		//необходимо для рендера BB
		AddToGraph(mapObj, gtDefFixPipe, true);

		AutoProj::Desc desc;

		desc.type = Proj::ptImmortal;
		desc.SetModel(GetRecord(MapObjLib::ctDecoration, "Misc\\shield"));
		desc.pos = NullVector;
		desc.size = D3DXVECTOR3(0.0f, 0.0f, 1.7f);
		desc.modelSize = true;
		desc.damage = 10.0f;
		desc.maxDist = 0.0f;

		gameObj.SetDesc(desc);
		gameObj.GetPxActor().SetScene(_world->GetPxScene());

		SaveMapObj(mapObj, MapObjLib::ctBonus, "shield");
	}
	//speedArrow
	{
		game::MapObj* mapObj = NewMapObj();
		game::AutoProj& gameObj = mapObj->SetGameObj<game::AutoProj>();
		//необходимо для рендера BB
		AddToGraph(mapObj, gtDefFixPipe, true);

		AutoProj::Desc desc;

		desc.type = Proj::ptSpeedArrow;
		desc.SetModel(GetRecord(MapObjLib::ctDecoration, "Misc\\strelkaAnim"));
		desc.pos = NullVector;
		desc.modelSize = true;
		desc.size = D3DXVECTOR3(0.0f, 0.0f, 1.7f);
		desc.damage = 200.0f * 1000.0f / 3600;

		gameObj.SetDesc(desc);
		gameObj.GetPxActor().SetScene(_world->GetPxScene());

		SaveMapObj(mapObj, MapObjLib::ctBonus, "speedArrow");
	}
	//lusha
	{
		game::MapObj* mapObj = NewMapObj();
		game::AutoProj& gameObj = mapObj->SetGameObj<game::AutoProj>();
		//необходимо для рендера BB
		AddToGraph(mapObj, gtDefFixPipe, true);

		AutoProj::Desc desc;

		desc.type = Proj::ptLusha;
		desc.SetModel(GetRecord(MapObjLib::ctDecoration, "Misc\\lusha"));
		desc.pos = NullVector;
		desc.modelSize = false;
		desc.size = D3DXVECTOR3(1.25f, 1.25f, 1.7f);
		desc.damage = 20.0f * 1000.0f / 3600;

		gameObj.SetDesc(desc);
		gameObj.GetPxActor().SetScene(_world->GetPxScene());

		SaveMapObj(mapObj, MapObjLib::ctBonus, "lusha");
	}
	//snowLusha
	{
		game::MapObj* mapObj = NewMapObj();
		game::AutoProj& gameObj = mapObj->SetGameObj<game::AutoProj>();
		//необходимо для рендера BB
		AddToGraph(mapObj, gtDefFixPipe, true);

		AutoProj::Desc desc;

		desc.type = Proj::ptLusha;
		desc.SetModel(GetRecord(MapObjLib::ctDecoration, "Misc\\snowLusha"));
		desc.pos = NullVector;
		desc.modelSize = false;
		desc.size = D3DXVECTOR3(1.25f, 1.25f, 1.7f);
		desc.damage = 20.0f * 1000.0f / 3600;

		gameObj.SetDesc(desc);
		gameObj.GetPxActor().SetScene(_world->GetPxScene());

		SaveMapObj(mapObj, MapObjLib::ctBonus, "snowLusha");
	}
	//hellLusha
	{
		game::MapObj* mapObj = NewMapObj();
		game::AutoProj& gameObj = mapObj->SetGameObj<game::AutoProj>();
		//необходимо для рендера BB
		AddToGraph(mapObj, gtDefFixPipe, true);

		AutoProj::Desc desc;

		desc.type = Proj::ptLusha;
		desc.SetModel(GetRecord(MapObjLib::ctDecoration, "Misc\\hellLusha"));
		desc.pos = NullVector;
		desc.modelSize = false;
		desc.size = D3DXVECTOR3(1.25f, 1.25f, 1.7f);
		desc.damage = 20.0f * 1000.0f / 3600;

		gameObj.SetDesc(desc);
		gameObj.GetPxActor().SetScene(_world->GetPxScene());

		SaveMapObj(mapObj, MapObjLib::ctBonus, "hellLusha");
	}
	//maslo
	{
		game::MapObj* mapObj = NewMapObj();
		game::AutoProj& gameObj = mapObj->SetGameObj<game::AutoProj>();
		//необходимо для рендера BB
		AddToGraph(mapObj, gtDefFixPipe, true);

		AutoProj::Desc desc;

		desc.type = Proj::ptMaslo;
		desc.SetModel(GetRecord(MapObjLib::ctDecoration, "Misc\\maslo"));
		desc.pos = NullVector;
		desc.modelSize = false;
		desc.size = D3DXVECTOR3(1.25f, 1.25f, 1.7f);
		desc.damage = 1.5f;

		gameObj.SetDesc(desc);
		gameObj.GetPxActor().SetScene(_world->GetPxScene());

		SaveMapObj(mapObj, MapObjLib::ctBonus, "maslo");
	}
}

void DataBase::LoadWeapons()
{
	//bulletGun
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();
		AddToGraph(mapObj, gtDefFixPipe, true);

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\bulletGun.r3d");
		AddLibMat(node, "Weapon\\bulletGun");

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetEffect(GetRecord(MapObjLib::ctEffects, "shotEff1"));		
			effect.SetSound(&GetSound("Sounds\\phalanx_shot_a.ogg"));
		}

		SaveMapObj(mapObj, MapObjLib::ctWeapon, "bulletGun");
	}

	//rifleWeapon
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();		
		
		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\rifleWeapon.r3d");
		AddLibMat(node, "Weapon\\rifleWeapon");
		AddToGraph(mapObj, gtDefFixPipe, true);

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetEffect(GetRecord(MapObjLib::ctEffects, "shotEff2"));
			effect.SetSound(&GetSound("Sounds\\cluster_rocket.ogg"));
		}
		
		SaveMapObj(mapObj, MapObjLib::ctWeapon, "rifleWeapon");
	}

	//airWeapon
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();		

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\airWeapon.r3d");
		AddLibMat(node, "Weapon\\airWeapon");
		AddToGraph(mapObj, gtDefFixPipe, true);

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetEffect(GetRecord(MapObjLib::ctEffects, "shotEff2"));
			effect.SetSound(&GetSound("Sounds\\airBladeRocket.ogg"));
		}

		SaveMapObj(mapObj, MapObjLib::ctWeapon, "airWeapon");
	}

	//blasterGun
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();		
		
		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\blasterGun.r3d");
		AddLibMat(node, "Weapon\\blasterGun");
		AddToGraph(mapObj, gtDefFixPipe, true);

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetEffect(GetRecord(MapObjLib::ctEffects, "shotEff2"));
			effect.SetSound(&GetSound("Sounds\\icon_rail.ogg"));
		}
		
		SaveMapObj(mapObj, MapObjLib::ctWeapon, "blasterGun");
	}
	
	//rocketLauncher
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();		
		
		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\rocketLauncher.r3d");
		AddLibMat(node, "Weapon\\rocketLauncher");
		AddToGraph(mapObj, gtDefFixPipe, true);

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetSound(&GetSound("Sounds\\missile_launch.ogg"));
		}
		
		SaveMapObj(mapObj, MapObjLib::ctWeapon, "rocketLauncher");
	}

	//torpedaWeapon
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\torpeda.r3d");
		AddLibMat(node, "Weapon\\torpeda");
		AddToGraph(mapObj, gtDefFixPipe, true);

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetSound(&GetSound("Sounds\\missile_launch.ogg"));
		}
		
		SaveMapObj(mapObj, MapObjLib::ctWeapon, "torpedaWeapon");
	}

	//phaseImpulse
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\phaseImpulse.r3d");
		AddLibMat(node, "Weapon\\phaseImpulse");
		AddToGraph(mapObj, gtDefFixPipe, true);

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetSound(&GetSound("Sounds\\fazowij_izluchatel.ogg"));
		}

		SaveMapObj(mapObj, MapObjLib::ctWeapon, "phaseImpulse");
	}

	//tankLaser
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\tankLaser.r3d");
		AddLibMat(node, "Weapon\\tankLaser");
		AddToGraph(mapObj, gtDefFixPipe, true);

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();			
			effect.SetEffect(GetRecord(MapObjLib::ctEffects, "laserShot"));
			effect.SetSound(&GetSound("Sounds\\laserGuseniza.ogg"));
		}
		
		SaveMapObj(mapObj, MapObjLib::ctWeapon, "tankLaser");
	}

	//asyncFrost
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\asyncFrost.r3d");
		AddLibMat(node, "Weapon\\asyncFrost");
		AddToGraph(mapObj, gtDefFixPipe, true);

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetEffect(GetRecord(MapObjLib::ctEffects, "frostShot"));
			effect.SetSound(&GetSound("Sounds\\frost_ray.ogg"));
		}

		SaveMapObj(mapObj, MapObjLib::ctWeapon, "asyncFrost");	
	}

	//asyncFrost2
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\asyncFrost2.r3d");
		AddLibMat(node, "Weapon\\asyncFrost2");
		AddToGraph(mapObj, gtDefFixPipe, true);

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetEffect(GetRecord(MapObjLib::ctEffects, "frostShot"));
			effect.SetSound(&GetSound("Sounds\\frost_ray.ogg"));
		}

		SaveMapObj(mapObj, MapObjLib::ctWeapon, "asyncFrost2");	
	}

	//masloWeapon
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetSound(&GetSound("Sounds\\maslo.ogg"));
		}

		SaveMapObj(mapObj, MapObjLib::ctWeapon, "masloWeapon");
	}

	//mine1Weapon
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetSound(&GetSound("Sounds\\shredder.ogg"));
		}

		SaveMapObj(mapObj, MapObjLib::ctWeapon, "mine1Weapon");
	}

	//mine2Weapon
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetSound(&GetSound("Sounds\\shredder.ogg"));
		}

		SaveMapObj(mapObj, MapObjLib::ctWeapon, "mine2Weapon");
	}

	//mine3Weapon
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetSound(&GetSound("Sounds\\shredder.ogg"));
		}

		SaveMapObj(mapObj, MapObjLib::ctWeapon, "mine3Weapon");
	}

	//hyperdrive
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();

		//graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\hyperdrive.r3d");
		//AddLibMat(node, "Weapon\\hyperdrive");
		//AddToGraph(mapObj, gtDefFixPipe, true);

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetSound(&GetSound("Sounds\\exhaust_b_heavy.ogg"));
		}
		
		SaveMapObj(mapObj, MapObjLib::ctWeapon, "hyperdrive");
	}

	//fireGun
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\fireGun.r3d");
		AddLibMat(node, "Weapon\\fireGun");
		AddToGraph(mapObj, gtDefFixPipe, true);

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetSound(&GetSound("Sounds\\fireGun.ogg"));
		}

		SaveMapObj(mapObj, MapObjLib::ctWeapon, "fireGun");
	}

	//pulsator
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();		

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\pulsator.r3d");
		AddLibMat(node, "Weapon\\pulsator");
		AddToGraph(mapObj, gtDefFixPipe, true);

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetSound(&GetSound("Sounds\\pulsator.ogg"));
		}

		SaveMapObj(mapObj, MapObjLib::ctWeapon, "pulsator");
	}

	//hyperBlaster
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();		

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\hyperBlaster.r3d");
		AddLibMat(node, "Weapon\\hyperBlaster");
		AddToGraph(mapObj, gtDefFixPipe, true);

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetEffect(GetRecord(MapObjLib::ctEffects, "flare6"));
			effect.SetSound(&GetSound("Sounds\\ultrazwukovoi_blaser.ogg"));
		}

		SaveMapObj(mapObj, MapObjLib::ctWeapon, "hyperBlaster");
	}

	//sphereGun
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();		

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\sphereGun.r3d");
		AddLibMat(node, "Car\\podushka");
		AddToGraph(mapObj, gtDefFixPipe, true);

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetSound(&GetSound("Sounds\\gun_podushkat.ogg"));
		}

		SaveMapObj(mapObj, MapObjLib::ctWeapon, "sphereGun");
	}

	//drobilka
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();		

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\drobilka.r3d");
		AddLibMat(node, "Weapon\\drobilka");
		AddToGraph(mapObj, gtDefFixPipe, true);

		SaveMapObj(mapObj, MapObjLib::ctWeapon, "drobilka");
	}

	//sonar
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();		

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\sonar.r3d");
		AddLibMat(node, "Car\\devildriver");
		AddToGraph(mapObj, gtDefFixPipe, true);	

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetSound(&GetSound("Sounds\\sonar.ogg"));
		}

		SaveMapObj(mapObj, MapObjLib::ctWeapon, "sonar");
	}

	//turel
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();		

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\turel.r3d");
		AddLibMat(node, "Weapon\\turel");
		AddToGraph(mapObj, gtDefFixPipe, true);

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetSound(&GetSound("Sounds\\turel.ogg"));
			effect.SetEffect(GetRecord(MapObjLib::ctEffects, "powerShot1"));
			effect.SetPos(D3DXVECTOR3(1.4f, 0.0f, 0.1f));
		}

		SaveMapObj(mapObj, MapObjLib::ctWeapon, "turel");
	}

	//mortira
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();		

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\mortira.r3d");
		AddLibMat(node, "Weapon\\mortira");
		AddToGraph(mapObj, gtDefFixPipe, true);
		
		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetSound(&GetSound("Sounds\\mortira.ogg"));
			effect.SetEffect(GetRecord(MapObjLib::ctEffects, "powerShot1"));
			effect.SetPos(D3DXVECTOR3(1.7f, 0.0f, 0.0f));
		}

		SaveMapObj(mapObj, MapObjLib::ctWeapon, "mortira");
	}

	//spring
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetSound(&GetSound("Sounds\\missile_launch.ogg"));			
		}

		SaveMapObj(mapObj, MapObjLib::ctWeapon, "spring");
	}

	//rezonator
	{
		game::MapObj* mapObj = NewMapObj();
		game::Weapon& gameObj = mapObj->SetGameObj<game::Weapon>();		

		graph::IVBMeshNode* node = AddMeshNode(mapObj, "Weapon\\rezonator.r3d");
		AddLibMat(node, "Weapon\\rezonator");
		AddToGraph(mapObj, gtDefFixPipe, true);

		//shotEff
		{
			ShotEffect& effect = gameObj.GetBehaviors().Add<ShotEffect>();
			effect.SetEffect(GetRecord(MapObjLib::ctEffects, "flare6"));
			effect.SetSound(&GetSound("Sounds\\rezonator.ogg"));
		}

		SaveMapObj(mapObj, MapObjLib::ctWeapon, "rezonator");
	}
}

void DataBase::LoadCars()
{
	{
		CarDesc desc;
		desc.mass = 2000.0f;
		desc.bodyAABB = AABB(D3DXVECTOR3(-1.55339f, -0.793131f, -0.438524f), D3DXVECTOR3(1.79646f, 0.768305f, 0.847951f));
		desc.bodyOffsetModel = D3DXVECTOR3(0, 0, 0);
		desc.bodyScaleModel = D3DXVECTOR3(1.0f, 1.0f, 0.95f);
		desc.bodyScale = D3DXVECTOR3(0.95f, 1.1f, 1.0f);
		desc.bodyOffset = D3DXVECTOR3(-0.05f, 0.0f, -0.17f);
		desc.centerMassPos = D3DXVECTOR3(0.1f, 0, -0.75f);

		desc.wheelRadius = 0.42947042f;
		desc.wheelOffsetModel.push_back(D3DXVECTOR3(0.05f, 0, 0));
		desc.wheelOffsetModel.push_back(D3DXVECTOR3(0.05f, 0, 0));
		desc.wheelOffsetModel.push_back(D3DXVECTOR3(0.0f, 0, 0));
		desc.wheelOffsetModel.push_back(D3DXVECTOR3(0.0f, 0, 0));
		desc.inverseWheelMass = 0.1f;
		desc.maxSpeed = 0.0f;
		desc.tireSpring = 0.0f;
		desc.steerSpeed = 3.5f;
		desc.steerRot = 3.5f;
		desc.flyYTorque = D3DX_PI/6;
		desc.clampXTorque = D3DX_PI/12;
		desc.clampYTorque = D3DX_PI/6;
		desc.wakeFrictionModel = false;

		desc.suspensionTravel = 0.30f;
		desc.suspensionSpring = 140000.0f;
		desc.suspensionDamper = 1000.0f;
		desc.suspensionTarget = 0.0f;

		desc.wheelMat = "Car\\marauderWheel";
		desc.bodyDestr = "Car\\marauderCrush";

		LoadCar("marauder", "Car\\marauder.r3d", "Car\\marauderWheel.r3d", "Car\\marauder", "Car\\marauderWheel.txt", desc);
	}

	{
		CarDesc desc;
		desc.mass = 2000.0f;
		desc.bodyAABB = AABB(D3DXVECTOR3(-2.08688f, -0.887617f, -0.367699f), D3DXVECTOR3(1.59955f, 0.851927f, 1.05358f));
		desc.bodyOffsetModel = D3DXVECTOR3(-0.09f, 0.0f, 0.06f);
		//desc.bodyOffsetModel = D3DXVECTOR3(-0.09f, 0.0f, 0.05f);
		//desc.bodyScaleModel = D3DXVECTOR3(1.1f, 1.1f, 1.1f);
		desc.bodyScale = D3DXVECTOR3(0.9f, 1.0f, 0.95f);
		desc.bodyOffset = D3DXVECTOR3(0.18f, 0.0f, -0.10f);
		desc.centerMassPos = D3DXVECTOR3(0.1f, 0, -0.60f);

		desc.wheelRadius = 0.37786922f;
		desc.inverseWheelMass = 0.1f;
		desc.maxSpeed = 0.0f;
		desc.tireSpring = 0.0f;
		desc.steerSpeed = 3.5f;
		desc.steerRot = 3.5f;
		desc.flyYTorque = D3DX_PI/2;
		desc.clampXTorque = D3DX_PI/12;
		desc.clampYTorque = D3DX_PI/9;
		desc.wakeFrictionModel = false;

		desc.suspensionTravel = 0.20f;
		desc.suspensionSpring = 140000.0f;
		desc.suspensionDamper = 1000.0f;
		desc.suspensionTarget = 0.0f;

		desc.wheelMat = "Car\\dirtdevilWheel";
		desc.bodyDestr = "Car\\dirtdevilCrush";

		LoadCar("dirtdevil", "Car\\dirtdevil.r3d", "Car\\dirtdevilWheel.r3d", "Car\\dirtdevil", "Car\\dirtdevilWheel.txt", desc);
	}

	{
		CarDesc desc;
		desc.mass = 2000.0f;
		desc.bodyAABB = AABB(D3DXVECTOR3(-1.77654f, -1.21723f, -0.425364f), D3DXVECTOR3(1.92265f, 1.21507f, 0.868031f));
		desc.bodyOffsetModel = D3DXVECTOR3(0.0f, 0.0f, 0.13f);
		desc.bodyScale = D3DXVECTOR3(0.95f, 1.0f, 1.0f);
		desc.bodyOffset = D3DXVECTOR3(-0.2f, 0.0f, 0.0f);
		desc.centerMassPos = D3DXVECTOR3(0.0f, 0, -0.55f);

		desc.wheelRadius = 0.34159720f;
		desc.wheelOffsetModel.push_back(D3DXVECTOR3(0.22f, 0, -0.01f));
		desc.wheelOffsetModel.push_back(D3DXVECTOR3(0.22f, 0, -0.01f));
		desc.wheelOffsetModel.push_back(D3DXVECTOR3(0.09f, 0, 0.08f));
		desc.wheelOffsetModel.push_back(D3DXVECTOR3(0.09f, 0, 0.08f));
		desc.inverseWheelMass = 0.1f;
		desc.maxSpeed = 0.0f;
		desc.tireSpring = 0.0f;
		desc.steerSpeed = 3.5f;
		desc.steerRot = 3.5f;
		desc.flyYTorque = D3DX_PI/6;
		desc.clampXTorque = D3DX_PI/12;
		desc.clampYTorque = D3DX_PI/9;
		desc.wakeFrictionModel = false;

		desc.suspensionTravel = 0.25f;
		desc.suspensionSpring = 140000.0f;
		desc.suspensionDamper = 1000.0f;
		desc.suspensionTarget = 0.0f;

		desc.wheelMat = "Car\\manticoraWheel";
		desc.wheelMeshBack = "Car\\manticoraWheelBack.r3d";
		desc.bodyDestr = "Car\\manticoraCrush";

		LoadCar("manticora", "Car\\manticora.r3d", "Car\\manticoraWheel.r3d", "Car\\manticora", "Car\\manticoraWheel.txt", desc);
	}

	{
		CarDesc desc;
		desc.mass = 2000.0f;
		desc.bodyAABB = AABB(D3DXVECTOR3(-1.77654f, -1.21723f, -0.425364f), D3DXVECTOR3(1.92265f, 1.21507f, 0.868031f));
		desc.bodyOffsetModel = D3DXVECTOR3(0.0f, 0.0f, 0.13f);
		desc.bodyScale = D3DXVECTOR3(0.95f, 1.0f, 1.0f);
		desc.bodyOffset = D3DXVECTOR3(-0.2f, 0.0f, 0.0f);
		desc.centerMassPos = D3DXVECTOR3(0.0f, 0, -0.55f);
		
		desc.wheelRadius = 0.34159720f;
		desc.wheelOffsetModel.push_back(D3DXVECTOR3(0.22f, 0, -0.01f));
		desc.wheelOffsetModel.push_back(D3DXVECTOR3(0.22f, 0, -0.01f));
		desc.wheelOffsetModel.push_back(D3DXVECTOR3(0.09f, 0, 0.08f));
		desc.wheelOffsetModel.push_back(D3DXVECTOR3(0.09f, 0, 0.08f));
		desc.inverseWheelMass = 0.1f;
		desc.maxSpeed = 0.0f;
		desc.tireSpring = 0.0f;
		desc.steerSpeed = 3.5f;
		desc.steerRot = 3.5f;
		desc.flyYTorque = D3DX_PI/6;
		desc.clampXTorque = D3DX_PI/12;
		desc.clampYTorque = D3DX_PI/9;
		desc.wakeFrictionModel = false;

		desc.suspensionTravel = 0.25f;
		desc.suspensionSpring = 140000.0f;
		desc.suspensionDamper = 1000.0f;
		desc.suspensionTarget = 0.0f;

		desc.wheelMat = "Car\\manticoraBossWheel";
		desc.wheelMeshBack = "Car\\manticoraWheelBack.r3d";
		desc.bodyDestr = "Car\\manticoraCrush";

		LoadCar("manticoraBoss", "Car\\manticoraBoss.r3d", "Car\\manticoraWheel.r3d", "Car\\manticoraBoss", "Car\\manticoraWheel.txt", desc);
	}

	{
		CarDesc desc;
		desc.mass = 2000.0f;
		desc.bodyScale = D3DXVECTOR3(0.95f, 0.85f, 0.8f);
		desc.bodyOffset = D3DXVECTOR3(-0.056f, 0.0f, -0.2f);
		desc.centerMassPos = D3DXVECTOR3(0, 0, -1.05f);
		//desc.bump = true;

		desc.wheelRadius = 0.0f;
		desc.inverseWheelMass = 0.1f;
		desc.maxSpeed = 0.0f;
		desc.tireSpring = 0.0f;
		desc.steerSpeed = 3.5f;
		desc.steerRot = 3.5f;
		desc.flyYTorque = D3DX_PI/3;
		desc.clampXTorque = D3DX_PI/12;
		desc.clampYTorque = D3DX_PI/6;
		desc.wakeFrictionModel = false;

		desc.suspensionTravel = 0.25f;
		desc.suspensionSpring = 140000.0f;
		desc.suspensionDamper = 1000.0f;
		desc.suspensionTarget = 0.0f;

		desc.wheelMat = "Car\\airbladeWheel";
		desc.bodyDestr = "Car\\airbladeCrush";
		desc.bodyDestr = "Car\\airbladeCrush";

		LoadCar("airblade", "Car\\airblade.r3d", "Car\\airbladeWheel.r3d", "Car\\airblade", "Car\\airbladeWheel.txt", desc);
	}

	{	
		CarDesc desc;
		desc.mass = 2000.0f;
		desc.bodyAABB = AABB(D3DXVECTOR3(-1.72021f, -1.43783f, -0.435831f), D3DXVECTOR3(1.71345f, 1.43783f, 0.441906f));
		desc.bodyScale = D3DXVECTOR3(1.0f, 0.85f, 1.0f);
		desc.centerMassPos = D3DXVECTOR3(0.0f, 0, -0.47f);

		desc.wheelRadius = 0.34f;
		desc.inverseWheelMass = 0.1f;
		desc.maxSpeed = 0.0f;
		desc.tireSpring = 0.0f;
		desc.steerSpeed = 3.5f;
		desc.steerRot = 3.5f;
		desc.flyYTorque = 0;
		desc.clampXTorque = D3DX_PI/12;
		desc.clampYTorque = D3DX_PI/6;
		desc.wakeFrictionModel = true;

		desc.suspensionTravel = 0.1f;
		desc.suspensionSpring = 625000.0f;
		desc.suspensionDamper = 25000.0f;
		desc.suspensionTarget = 0.0f;
		desc.halfWheelEff = true;
		desc.guseniza = true;
		desc.clutchImmunity = true;

		desc.soundMotorIdle = "Sounds\\guseniza_stop.ogg";
		desc.soundMotorHigh = "Sounds\\guseniza_move.ogg";

		desc.bodyDestr = "Car\\gusenizaCrush";

		LoadCar("guseniza", "Car\\guseniza.r3d", "", "Car\\guseniza", "Car\\gusenizaWheel.txt", desc);
	}

	{	
		CarDesc desc;
		desc.mass = 2000.0f;
		desc.bodyAABB = AABB(D3DXVECTOR3(-1.72021f, -1.43783f, -0.435831f), D3DXVECTOR3(1.71345f, 1.43783f, 0.441906f));
		desc.bodyScale = D3DXVECTOR3(1.0f, 0.85f, 1.0f);
		desc.centerMassPos = D3DXVECTOR3(0.0f, 0, -0.47f);

		desc.wheelRadius = 0.34f;
		desc.inverseWheelMass = 0.1f;
		desc.maxSpeed = 0.0f;
		desc.tireSpring = 0.0f;
		desc.steerSpeed = 3.5f;
		desc.steerRot = 3.5f;
		desc.flyYTorque = 0;
		desc.clampXTorque = D3DX_PI/12;
		desc.clampYTorque = D3DX_PI/6;
		desc.wakeFrictionModel = true;

		desc.suspensionTravel = 0.1f;
		desc.suspensionSpring = 625000.0f;
		desc.suspensionDamper = 25000.0f;
		desc.suspensionTarget = 0.0f;
		desc.halfWheelEff = true;
		desc.guseniza = true;
		desc.clutchImmunity = true;

		desc.bodyDestr = "Car\\gusenizaCrush";

		LoadCar("gusenizaBoss", "Car\\gusenizaBoss.r3d", "", "Car\\gusenizaBoss", "Car\\gusenizaWheel.txt", desc);
	}

	{
		CarDesc desc;
		desc.mass = 2000.0f;
		desc.bodyAABB = AABB(D3DXVECTOR3(-1.93629f, -0.882337f, -0.554882f), D3DXVECTOR3(2.13104f, 0.885529f, 0.624387f));
		desc.bodyOffsetModel = D3DXVECTOR3(0.0f, 0.0f, -0.09f);
		desc.bodyScale = D3DXVECTOR3(0.9f, 0.9f, 0.9f);
		desc.bodyOffset = D3DXVECTOR3(-0.1f, 0.0f, 0.0f);
		desc.centerMassPos = D3DXVECTOR3(0.15f, 0, -0.52f);

		desc.wheelRadius = 0.34f;
		desc.inverseWheelMass = 0.1f;
		desc.maxSpeed = 0.0f;
		desc.tireSpring = 0.0f;
		desc.steerSpeed = 3.5f;
		desc.steerRot = 3.5f;
		desc.flyYTorque = 0;
		desc.clampXTorque = D3DX_PI/12;
		desc.clampYTorque = D3DX_PI/6;
		desc.wakeFrictionModel = true;

		desc.suspensionTravel = 0.1f;
		desc.suspensionSpring = 625000.0f;
		desc.suspensionDamper = 25000.0f;
		desc.suspensionTarget = 0.0f;
		desc.halfWheelEff = true;
		desc.clutchImmunity = true;
		desc.podushka = true;

		desc.soundMotorIdle = "Sounds\\podushka_stop.ogg";
		desc.soundMotorHigh = "Sounds\\podushka_move.ogg";

		desc.bodyDestr = "Car\\podushkaCrush";

		LoadCar("podushka", "Car\\podushka.r3d", "", "Car\\podushka", "Car\\podushkaWheel.txt", desc);
	}

	{
		CarDesc desc;
		desc.mass = 2000.0f;
		desc.bodyAABB = AABB(D3DXVECTOR3(-1.93629f, -0.882337f, -0.554882f), D3DXVECTOR3(2.13104f, 0.885529f, 0.624387f));
		desc.bodyOffsetModel = D3DXVECTOR3(0.0f, 0.0f, -0.09f);
		desc.bodyScale = D3DXVECTOR3(0.9f, 0.9f, 0.9f);
		desc.bodyOffset = D3DXVECTOR3(-0.1f, 0.0f, 0.0f);
		desc.centerMassPos = D3DXVECTOR3(0.15f, 0, -0.52f);

		desc.wheelRadius = 0.34f;
		desc.inverseWheelMass = 0.1f;
		desc.maxSpeed = 0.0f;
		desc.tireSpring = 0.0f;
		desc.steerSpeed = 3.5f;
		desc.steerRot = 3.5f;
		desc.flyYTorque = 0;
		desc.clampXTorque = D3DX_PI/12;
		desc.clampYTorque = D3DX_PI/6;
		desc.wakeFrictionModel = true;

		desc.suspensionTravel = 0.1f;
		desc.suspensionSpring = 625000.0f;
		desc.suspensionDamper = 25000.0f;
		desc.suspensionTarget = 0.0f;
		desc.halfWheelEff = true;
		desc.clutchImmunity = true;
		desc.podushka = true;

		desc.soundMotorIdle = "Sounds\\podushka_stop.ogg";
		desc.soundMotorHigh = "Sounds\\podushka_move.ogg";

		desc.bodyDestr = "Car\\podushkaCrush";

		LoadCar("podushkaBoss", "Car\\podushkaBoss.r3d", "", "Car\\podushkaBoss", "Car\\podushkaWheel.txt", desc);
	}

	{
		CarDesc desc;
		desc.mass = 2500.0f;
		desc.bodyScale = D3DXVECTOR3(1.0f, 1.1f, 0.9f);
		desc.bodyOffset = D3DXVECTOR3(-0.2f, 0.0f, -0.1f);
		desc.centerMassPos = D3DXVECTOR3(0.0f, 0, -1.2f);
		desc.bodyOffsetModel = D3DXVECTOR3(0.0f, 0.0f, 0.1f);
		desc.frontWheelDrive = true;
		desc.backWheelDrive = true;

		desc.wheelRadius = 0.62686455f;
		desc.wheelOffsetModel.push_back(D3DXVECTOR3(-0.09f, 0, 0.0f));
		desc.wheelOffsetModel.push_back(D3DXVECTOR3(-0.09f, 0, 0.0f));
		desc.wheelOffsetModel.push_back(D3DXVECTOR3(0.24f, 0, 0.0f));
		desc.wheelOffsetModel.push_back(D3DXVECTOR3(0.24f, 0, 0.0f));
		desc.inverseWheelMass = 0.1f;
		desc.maxSpeed = 48.0f;
		desc.tireSpring = 0.0f;
		desc.steerSpeed = 4.0f;
		desc.flyYTorque = 0;
		desc.clampXTorque = D3DX_PI/12;
		desc.clampYTorque = D3DX_PI/9;
		desc.maxRPM = 3750;
		desc.wakeFrictionModel = false;

		desc.suspensionTravel = 0.65f;
		desc.suspensionSpring = 120000.0f;
		desc.suspensionDamper = 2500.0f;
		desc.suspensionTarget = 0.0f;

		desc.wheelMat = "Car\\monstertruckWheel";
		desc.soundMotorIdle = "Sounds\\monstertruckstop.ogg";

		desc.bodyDestr = "Car\\monstertruckCrush";

		LoadCar("monstertruck", "Car\\monstertruck.r3d", "Car\\monstertruckWheel.r3d", "Car\\monstertruck", "Car\\monstertruckWheel.txt", desc);
	}

	{
		CarDesc desc;
		desc.mass = 2500.0f;
		desc.bodyScale = D3DXVECTOR3(1.0f, 1.1f, 0.9f);
		desc.bodyOffset = D3DXVECTOR3(-0.2f, 0.0f, -0.1f);
		desc.centerMassPos = D3DXVECTOR3(0.0f, 0, -1.2f);
		desc.frontWheelDrive = true;
		desc.backWheelDrive = true;

		desc.wheelRadius = 0.0f;
		desc.inverseWheelMass = 0.1f;
		desc.maxSpeed = 48.0f;
		desc.tireSpring = 0.0f;
		desc.steerSpeed = 4.0f;
		desc.flyYTorque = 0;
		desc.clampXTorque = D3DX_PI/12;
		desc.clampYTorque = D3DX_PI/9;
		desc.maxRPM = 3750;
		desc.wakeFrictionModel = false;

		desc.suspensionTravel = 0.65f;
		desc.suspensionSpring = 120000.0f;
		desc.suspensionDamper = 2500.0f;
		desc.suspensionTarget = 0.0f;

		desc.wheelMat = "Car\\monstertruckBossWheel";
		desc.soundMotorIdle = "Sounds\\monstertruckstop.ogg";

		desc.bodyDestr = "Car\\monstertruckCrush";

		LoadCar("monstertruckBoss", "Car\\monstertruckBoss.r3d", "Car\\monstertruckBossWheel.r3d", "Car\\monstertruckBoss", "Car\\monstertruckWheel.txt", desc);
	}

	{	
		CarDesc desc;
		desc.mass = 2000.0f;
		desc.bodyScale = D3DXVECTOR3(0.9f, 0.85f, 0.9f);
		desc.centerMassPos = D3DXVECTOR3(0.0f, 0, -0.75f);
		desc.frontWheelDrive = true;
		desc.backWheelDrive = true;
		desc.gravEngine = true;

		desc.wheelRadius = 0.342f;
		desc.inverseWheelMass = 0.1f;
		desc.maxSpeed = 48.0f;
		desc.tireSpring = 0.0f;
		desc.steerSpeed = 3.5f;
		desc.steerRot = 4.0f;
		desc.flyYTorque = 0;
		desc.clampXTorque = D3DX_PI/12;
		desc.clampYTorque = D3DX_PI/12;
		desc.wheelEff = false;
		desc.clutchImmunity = true;
		desc.wakeFrictionModel = true;

		desc.suspensionTravel = 0.4f;
		desc.suspensionSpring = 140000.0f;
		desc.suspensionDamper = 1000.0f;
		desc.suspensionTarget = 0.0f;

		desc.soundMotorIdle = "Sounds\\podushka_stop.ogg";
		desc.soundMotorHigh = "Sounds\\podushka_move.ogg";

		desc.bodyDestr = "Car\\devildriverCrush";

		LoadCar("devildriver", "Car\\devildriver.r3d", "", "Car\\devildriver", "Car\\devildriverWheel.txt", desc);
	}

	{	
		CarDesc desc;
		desc.mass = 2000.0f;
		desc.bodyScale = D3DXVECTOR3(0.9f, 0.85f, 0.9f);
		desc.centerMassPos = D3DXVECTOR3(0.0f, 0, -0.75f);
		desc.frontWheelDrive = true;
		desc.backWheelDrive = true;
		desc.gravEngine = true;

		desc.wheelRadius = 0.342f;
		desc.inverseWheelMass = 0.1f;
		desc.maxSpeed = 48.0f;
		desc.tireSpring = 0.0f;
		desc.steerSpeed = 3.5f;
		desc.steerRot = 4.0f;
		desc.flyYTorque = 0;
		desc.clampXTorque = D3DX_PI/12;
		desc.clampYTorque = D3DX_PI/12;
		desc.wheelEff = false;
		desc.clutchImmunity = true;
		desc.wakeFrictionModel = true;

		desc.suspensionTravel = 0.4f;
		desc.suspensionSpring = 140000.0f;
		desc.suspensionDamper = 1000.0f;
		desc.suspensionTarget = 0.0f;

		desc.soundMotorIdle = "Sounds\\podushka_stop.ogg";
		desc.soundMotorHigh = "Sounds\\podushka_move.ogg";

		desc.bodyDestr = "Car\\devildriverCrush";

		LoadCar("devildriverBoss", "Car\\devildriverBoss.r3d", "", "Car\\devildriverBoss", "Car\\devildriverWheel.txt", desc);
	}

	{
		CarDesc desc;
		desc.mass = 1500.0f;
		desc.bodyScale = D3DXVECTOR3(0.95f, 1.0f, 0.95f);
		desc.bodyOffset = D3DXVECTOR3(0.1f, 0.0f, 0.0f);
		desc.centerMassPos = D3DXVECTOR3(0.1f, 0, -0.55f);

		desc.wheelRadius = 0.0f;
		desc.inverseWheelMass = 0.1f;
		desc.maxSpeed = 0.0f;
		desc.tireSpring = 0.0f;
		desc.steerSpeed = 4.0f;
		desc.flyYTorque = 0;
		desc.clampXTorque = D3DX_PI/12;
		desc.clampYTorque = D3DX_PI/6;
		desc.wakeFrictionModel = false;

		desc.suspensionTravel = 0.3f;
		desc.suspensionSpring = 100000.0f;
		desc.suspensionDamper = 800.0f;
		desc.suspensionTarget = 0.0f;

		desc.wheelMat = "Car\\buggiWheel";
		desc.bodyDestr = "Effect\\destrCar";

		LoadCar("buggi", "Car\\buggi.r3d", "Car\\buggiWheel.r3d", "Car\\buggi", "Car\\buggiWheel.txt", desc);
	}

	{
		CarDesc desc;
		desc.mass = 2000.0f;	
		desc.bodyScale = D3DXVECTOR3(0.9f, 0.85f, 0.9f);
		desc.bodyOffset = D3DXVECTOR3(0.15f, 0.0f, 0.08f);
		desc.centerMassPos = D3DXVECTOR3(0.1f, 0, -0.45f);

		desc.wheelRadius = 0.0f;
		desc.inverseWheelMass = 0.1f;
		desc.maxSpeed = 0.0f;
		desc.tireSpring = 0.0f;
		desc.steerSpeed = 4.0f;
		desc.flyYTorque = 0;
		desc.clampXTorque = D3DX_PI/12;
		desc.clampYTorque = D3DX_PI/6;
		desc.wakeFrictionModel = false;

		desc.suspensionTravel = 0.2f;
		desc.suspensionSpring = 140000.0f;
		desc.suspensionDamper = 1000.0f;
		desc.suspensionTarget = 0.0f;

		desc.wheelMat = "Car\\tankchettiWheel";
		desc.bodyDestr = "Effect\\destrCar";

		LoadCar("tankchetti", "Car\\tankchetti.r3d", "Car\\tankchettiWheel.r3d", "Car\\tankchetti", "Car\\tankchettiWheel.txt", desc);
	}

	{
		CarDesc desc;
		desc.mass = 2000.0f;
		desc.bodyScale = D3DXVECTOR3(0.95f, 1.0f, 1.0f);
		desc.bodyOffset = D3DXVECTOR3(0.17f, 0.0f, 0.0f);
		desc.centerMassPos = D3DXVECTOR3(0.2f, 0, -0.70f);
		desc.frontWheelDrive = true;
		desc.backWheelDrive = true;

		desc.wheelRadius = 0.0f;
		desc.inverseWheelMass = 0.1f;
		desc.maxSpeed = 0.0f;
		desc.tireSpring = 0.0f;
		desc.steerSpeed = 4.0f;
		desc.steerRot = 3.5f;
		desc.flyYTorque = D3DX_PI/6;
		desc.clampXTorque = D3DX_PI/12;
		desc.clampYTorque = D3DX_PI/6;
		desc.wakeFrictionModel = false;

		desc.suspensionTravel = 0.20f;
		desc.suspensionSpring = 150000.0f;
		desc.suspensionDamper = 1000.0f;
		desc.suspensionTarget = 0.0f;

		desc.wheelMat = "Car\\mustangWheel";
		desc.bodyDestr = "Effect\\destrCar";

		LoadCar("mustang", "Car\\mustang.r3d", "Car\\mustangWheel.r3d", "Car\\mustang", "Car\\mustangWheel.txt", desc);
	}

	{
		CarDesc desc;
		desc.mass = 2000.0f;
		desc.bodyAABB = AABB(D3DXVECTOR3(-1.93629f, -0.882337f, -0.554882f), D3DXVECTOR3(2.13104f, 0.885529f, 0.624387f));
		desc.bodyOffsetModel = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
		desc.bodyScale = D3DXVECTOR3(0.9f, 0.9f, 0.9f);
		desc.bodyOffset = D3DXVECTOR3(-0.1f, 0.0f, 0.0f);
		desc.centerMassPos = D3DXVECTOR3(0.15f, 0, -0.52f);

		desc.wheelRadius = 0.34f;
		desc.inverseWheelMass = 0.1f;
		desc.maxSpeed = 0.0f;
		desc.tireSpring = 0.0f;
		desc.steerSpeed = 3.5f;
		desc.steerRot = 3.5f;
		desc.flyYTorque = 0;
		desc.clampXTorque = D3DX_PI/12;
		desc.clampYTorque = D3DX_PI/6;
		desc.wakeFrictionModel = true;

		desc.suspensionTravel = 0.1f;
		desc.suspensionSpring = 625000.0f;
		desc.suspensionDamper = 25000.0f;
		desc.suspensionTarget = 0.0f;
		desc.halfWheelEff = false;
		desc.clutchImmunity = true;
		desc.podushka = false;

		desc.soundMotorIdle = "Sounds\\podushka_stop.ogg";
		desc.soundMotorHigh = "Sounds\\podushka_move.ogg";

		desc.bodyDestr = "Effect\\destrCar";

		LoadCar("xCar", "Car\\xCar.r3d", "Car\\xCarWheel.r3d", "Car\\xCar", "Car\\xCarWheel.txt", desc);
	}
}

void DataBase::LoadDB()
{
	ClearDB();

	LoadSndSources();
	LoadEffects();
	LoadWorld1();
	LoadWorld2();
	LoadWorld3();
	LoadWorld4();
	LoadWorld5();
	LoadWorld6();
	LoadMisc();
	LoadCrush();
	LoadBonus();
	LoadWeapons();
	LoadCars();
}

void DataBase::Init()
{
	LSL_LOG("db init");

	if (_rootNode)
		return;

	_rootNode = new lsl::RootNode("root", this);

	LSL_LOG("db managers");

	_fxPSpriteManager = new graph::FxPointSpritesManager();
	_fxPSpriteManager->SetName("fxPSpriteManager");
	_fxPSpriteManager->SetOwner(this);

	_fxSpriteManager = new graph::FxSpritesManager();
	_fxSpriteManager->SetName("fxSpriteManager");
	_fxSpriteManager->SetOwner(this);

	_fxDirSpriteManager = new graph::FxSpritesManager();
	_fxDirSpriteManager->SetName("fxDirSpriteManager");
	_fxDirSpriteManager->SetOwner(this);
	_fxDirSpriteManager->dirSprite = true;

	_fxPlaneManager = new graph::FxPlaneManager();
	_fxPlaneManager->SetName("fxPlaneManager");
	_fxPlaneManager->SetOwner(this);	

	_fxWheelManager = new graph::FxNodeManager();
	_fxWheelManager->SetName("fxWheelManager");
	_fxWheelManager->SetOwner(this);
	{
		graph::IVBMeshNode* node = AddMeshNode(&_fxWheelManager->GetNode(), "Effect\\wheel.r3d");
		AddLibMat(node, "Effect\\wheel");
	}

	_fxTrubaManager = new graph::FxNodeManager();
	_fxTrubaManager->SetName("fxTrubaManager");
	_fxTrubaManager->SetOwner(this);
	{
		graph::IVBMeshNode* node = AddMeshNode(&_fxTrubaManager->GetNode(), "Effect\\truba.r3d");
		AddLibMat(node, "Effect\\truba");
	}

	_fxPiecesManager = new graph::FxNodeManager();
	_fxPiecesManager->SetName("fxPiecesManager");
	_fxPiecesManager->SetOwner(this);
	{
		graph::IVBMeshNode* node = AddMeshNode(&_fxPiecesManager->GetNode(), "Effect\\pieces1.r3d");
		AddLibMat(node, "Effect\\pieces");
	}

	_fxTrailManager = new graph::FxTrailManager();
	_fxTrailManager->SetName("fxTrailManager");
	_fxTrailManager->SetOwner(this);	
	_fxTrailManager->SetTrailWidth(0.3f);
	_fxTrailManager->fixedUp = true;
	_fxTrailManager->fixedUpVec = ZVector;

	{
		NxMaterialDesc carMaterialDesc;
		carMaterialDesc.staticFriction = 0.08f;
		carMaterialDesc.dynamicFriction = 0.08f;
		carMaterialDesc.restitution = 0.0f;
		carMaterialDesc.staticFrictionV = 3.2f;
		carMaterialDesc.dynamicFrictionV = 2.0f;
		carMaterialDesc.dirOfAnisotropy = NxVec3(0, 0, 1.0f);
		//устаналвиаем данные флаги чтобы машина нормально скользила по боковой поверхности, иначе она приподнимается вверх
		carMaterialDesc.flags = NX_MF_ANISOTROPIC | NX_MF_DISABLE_STRONG_FRICTION;
		carMaterialDesc.frictionCombineMode = NX_CM_MIN;
		_nxCarMaterial1 = _world->GetPxScene()->GetNxScene()->createMaterial(carMaterialDesc);
	}
	{
		NxMaterialDesc carMaterialDesc;
		carMaterialDesc.staticFriction = 0.02f;
		carMaterialDesc.dynamicFriction = 0.02f;
		carMaterialDesc.restitution = 0.0f;
		carMaterialDesc.staticFrictionV = 3.2f;
		carMaterialDesc.dynamicFrictionV = 2.0f;
		carMaterialDesc.dirOfAnisotropy = NxVec3(0, 0, 1.0f);
		//устаналвиаем данные флаги чтобы машина нормально скользила по боковой поверхности, иначе она приподнимается вверх
		carMaterialDesc.flags = NX_MF_ANISOTROPIC | NX_MF_DISABLE_STRONG_FRICTION;
		carMaterialDesc.frictionCombineMode = NX_CM_MIN;
		_nxCarMaterial2 = _world->GetPxScene()->GetNxScene()->createMaterial(carMaterialDesc);
	}

	NxMaterialDesc descMaterialWheel;
	descMaterialWheel.restitution = 0.0f;
	descMaterialWheel.flags = NX_MF_DISABLE_FRICTION;
	_nxWheelMaterial = _world->GetPxScene()->GetNxScene()->createMaterial(descMaterialWheel);

	NxMaterialDesc trackMaterialDesc;
	trackMaterialDesc.staticFriction = 0.1f;
	trackMaterialDesc.dynamicFriction = 0.1f;
	trackMaterialDesc.restitution = 0.0f;
	trackMaterialDesc.frictionCombineMode = NX_CM_AVERAGE;
	_trackMaterial = _world->GetPxScene()->GetNxScene()->createMaterial(trackMaterialDesc);

	NxMaterialDesc borderMaterialDesc;
	borderMaterialDesc.staticFriction = 0.1f;
	borderMaterialDesc.dynamicFriction = 4.0f;
	borderMaterialDesc.restitution = 0.0f;	
	borderMaterialDesc.frictionCombineMode = NX_CM_MAX;
	_borderMaterial = _world->GetPxScene()->GetNxScene()->createMaterial(borderMaterialDesc);

	try
	{
		LSL_LOG("db load data");

		Load("db.xml");
	}
	catch (const EUnableToOpen&)
	{
		LSL_LOG("db reset data");

		ResetDB("db.xml");
	}

	LSL_LOG("db init lib");

	InitMapObjLib();

	PairPxContactEffect& pxContactEff = _world->GetLogic()->GetBehaviors().Add<PairPxContactEffect>();
	pxContactEff.SetEffect(GetRecord(MapObjLib::ctEffects, "spark2"));
	pxContactEff.InsertSound(&GetSound("Sounds\\light_impact01.ogg"));
	pxContactEff.InsertSound(&GetSound("Sounds\\light_impact02.ogg"));
	pxContactEff.InsertSound(&GetSound("Sounds\\light_impact03.ogg"));
	pxContactEff.InsertSound(&GetSound("Sounds\\light_impact04.ogg"));
	pxContactEff.InsertSound(&GetSound("Sounds\\light_impact05.ogg"));
}

void DataBase::Release()
{
	if (!_rootNode)
		return;

	FreeMapObjLib();

	delete _fxTrailManager;
	delete _fxPiecesManager;
	delete _fxTrubaManager;
	delete _fxWheelManager;
	delete _fxDirSpriteManager;
	delete _fxPlaneManager;
	delete _fxSpriteManager;
	delete _fxPSpriteManager;

	delete _rootNode;
}

void DataBase::ResetDB(const std::string& fileName)
{
	InitMapObjLib();
	LoadDB();
	Save(fileName);
}

void DataBase::ClearDB()
{
	for (int i = 0; i < MapObjLib::cCategoryEnd; ++i)	
		_mapObjLib[i]->Clear();
}

void DataBase::Save(const std::string& fileName)
{
	SerialFileXML xml;
	xml.SaveNodeToFile(*_rootNode, fileName);
}

void DataBase::Load(const std::string& fileName)
{
	FreeMapObjLib();
	SerialFileXML xml;
	xml.LoadNodeFromFile(*_rootNode, fileName);
	InitMapObjLib();
}

MapObjLib* DataBase::GetMapObjLib(MapObjLib::Category category)
{
	return _mapObjLib[category];
}

MapObjRec* DataBase::GetRecord(MapObjLib::Category category, const std::string& name, bool assertFind)
{
	MapObjRec* record = _mapObjLib[category]->FindRecord(name);

	if (assertFind && record == 0)
		throw lsl::Error("MapObj record " + name + " does not exist");

	return record;
}

snd::Sound& DataBase::GetSound(const std::string& name)
{
	snd::Sound* sound = _world->GetResManager()->GetSoundLib().Find(name);
	if (sound == 0)
		throw lsl::Error("Sound " + name + " does not exist");

	return *sound;
}

}

}