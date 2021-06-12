#pragma once

#include "game\MapObj.h"
#include "game\RockCar.h"

namespace r3d
{

namespace game
{

class DataBase: public Component
{
private:
	enum GraphType
	{
		gtDefFixPipe,
		gtDef,		
		gtRefl,
		gtBumb,
		gtPlanarRefl,
		gtEffect,
		gtRefrEffect,
	};

	struct CarDesc
	{
		CarDesc(): mass(0), centerMassPos(NullVector), bodyScale(IdentityVector), bodyOffset(NullVector), bodyAABB(NullAABB), bodyOffsetModel(NullVector), bodyScaleModel(IdentityVector), wheelRadius(0), wheelScaleModel(IdentityVector), inverseWheelMass(0), maxSpeed(0), tireSpring(2.0f), steerSpeed(D3DX_PI/1.6f), steerRot(D3DX_PI), flyYTorque(D3DX_PI/2), clampXTorque(0), clampYTorque(0), angDamping(1, 1, 0), maxRPM(7000), frontWheelDrive(true), backWheelDrive(false), gravEngine(false), clutchImmunity(false), wheelEff(true), halfWheelEff(false), guseniza(false), podushka(false), wakeFrictionModel(false), suspensionTravel(0), suspensionSpring(0), suspensionDamper(0), suspensionTarget(0), soundMotorIdle(""), soundMotorHigh(""), motorVolumeRange(0, 1), motorFreqRange(0, 1), bump(false) {}

		float mass;
		D3DXVECTOR3 centerMassPos;
		D3DXVECTOR3 bodyScale;
		D3DXVECTOR3 bodyOffset;
		AABB bodyAABB;
		D3DXVECTOR3 bodyOffsetModel;
		D3DXVECTOR3 bodyScaleModel;

		float wheelRadius;
		std::vector<D3DXVECTOR3> wheelOffsetModel;
		D3DXVECTOR3 wheelScaleModel;
		float inverseWheelMass;
		float maxSpeed;
		float tireSpring;
		float steerSpeed;
		float steerRot;
		float flyYTorque;
		float clampXTorque;
		float clampYTorque;
		D3DXVECTOR3 angDamping;
		int maxRPM;

		bool frontWheelDrive;
		bool backWheelDrive;
		bool gravEngine;
		bool clutchImmunity;
		bool wheelEff;
		bool halfWheelEff;
		bool guseniza;
		bool podushka;
		bool wakeFrictionModel;

		float suspensionTravel;
		float suspensionSpring;
		float suspensionDamper;
		float suspensionTarget;

		string soundMotorIdle;
		string soundMotorHigh;
		D3DXVECTOR2 motorVolumeRange;
		D3DXVECTOR2 motorFreqRange;

		bool bump;

		lsl::string wheelMat;
		lsl::string wheelMeshBack;
		lsl::string bodyDestr;

		lsl::string bodyMesh2;
		lsl::string wheelMesh2;
	};
private:
	World* _world;
	lsl::RootNode* _rootNode;

	graph::FxPointSpritesManager* _fxPSpriteManager;
	graph::FxSpritesManager* _fxSpriteManager;
	graph::FxSpritesManager* _fxDirSpriteManager;
	graph::FxPlaneManager* _fxPlaneManager;
	graph::FxNodeManager* _fxWheelManager;
	graph::FxNodeManager* _fxTrubaManager;
	graph::FxNodeManager* _fxPiecesManager;
	graph::FxTrailManager* _fxTrailManager;

	bool _initMapObjLib;
	MapObjLib* _mapObjLib[MapObjLib::cCategoryEnd];

	NxMaterial* _nxCarMaterial1;
	NxMaterial* _nxCarMaterial2;
	NxMaterial* _nxWheelMaterial;
	NxMaterial* _trackMaterial;	
	NxMaterial* _borderMaterial;

	void InitMapObjLib();
	void FreeMapObjLib();

	MapObj* NewMapObj();
	MapObj* NewChildMapObj(MapObj* mapObj, MapObjLib::Category category, const std::string& name);
	void SaveMapObj(MapObj* mapObj, MapObjLib::Category category, const std::string& name);

	//
	graph::IVBMeshNode* AddMeshNode(graph::SceneNode* scNode, const std::string& mesh, int meshId = -1);
	graph::IVBMeshNode* AddMeshNode(MapObj* mapObj, const std::string& mesh, int meshId = -1);
	graph::Sprite* AddSprite(game::MapObj* mapObj, bool fixDir, const D3DXVECTOR2& sizes);
	graph::Sprite* AddFxSprite(game::MapObj* mapObj, const std::string& libMat, const Vec3Range& speedPos, const Vec3Range& speedScale, const QuatRange& speedRot, bool autoRot, graph::SceneNode::AnimMode animMode, float animDuration, float frame = 0.0f, bool dir = false, const D3DXVECTOR2& sizes = IdentityVec2);
	graph::PlaneNode* AddPlaneNode(game::MapObj* mapObj, const D3DXVECTOR2& sizes);	
	graph::PlaneNode* AddFxPlane(game::MapObj* mapObj, const std::string& libMat, const Vec3Range& speedPos, const Vec3Range& speedScale, const QuatRange& speedRot, graph::SceneNode::AnimMode animMode, float animDuration, float frame = 0.0f, const D3DXVECTOR2& sizes = IdentityVec2);
	
	graph::LibMaterial* AddLibMat(graph::MaterialNode* node, const std::string& libMat);
	graph::LibMaterial* AddLibMat(graph::IVBMeshNode* node, const std::string& libMat);
	void AddToGraph(graph::Actor& grActor, GraphType type, bool dynamic, bool morph = false, bool disableShadows = false, bool cullOpacity = false);
	void AddToGraph(MapObj* mapObj, GraphType type, bool dynamic, bool morph = false, bool disableShadows = false, bool cullOpacity = false);
	
	//
	px::BoxShape* AddPxBox(MapObj* mapObj, const AABB& aabb);
	px::BoxShape* AddPxBox(MapObj* mapObj);
	px::CapsuleShape* AddPxCapsule(MapObj* mapObj, float radius, float height, const D3DXVECTOR3& dir, const D3DXVECTOR3& up);
	px::CapsuleShape* AddPxCapsule(MapObj* mapObj);
	px::SphereShape* AddPxSpere(MapObj* mapObj, float radius);
	px::TriangleMeshShape* AddPxMesh(MapObj* mapObj, const std::string& meshName, int meshId = -1);
	px::ConvexShape* AddPxConvex(MapObj* mapObj, const std::string& meshName, int meshId = -1);	
	px::Body* AddPxBody(MapObj* mapObj, const NxBodyDesc& desc);
	px::Body* AddPxBody(MapObj* mapObj, float mass, const D3DXVECTOR3* massPos);
	
	//
	CarWheel* AddWheel(unsigned index, GameCar& car, const std::string& meshName, const std::string& matName, const D3DXVECTOR3& pos, bool steer, bool lead, CarWheel* master, const CarDesc& carDesc);
	
	//
	graph::FxParticleSystem* AddFxSystem(graph::SceneNode* node, graph::FxManager* manager, graph::FxParticleSystem::ChildStyle childStyle = graph::FxParticleSystem::csProxy);
	graph::FxParticleSystem* AddFxSystem(MapObj* mapObj, graph::FxManager* manager, graph::FxParticleSystem::ChildStyle childStyle = graph::FxParticleSystem::csProxy);
	graph::FxFlowEmitter* AddFxFlowEmitter(graph::FxParticleSystem* fxSystem, const graph::FxEmitter::ParticleDesc& partDesc, const graph::FxFlowEmitter::FlowDesc& flowDesc, bool worldCoordSys);

	//pxDefGroup - 0 px default group
	//pxShotGroup - 1 px shot group
	void LoadDecor(const std::string& name, const std::string& mesh, const std::string& libMat, GraphType graphType = gtDefFixPipe, bool px = false, bool cullOpacity = false, bool disableShadows = true);
	void LoadTrack(const std::string& name, const std::string& mesh, const std::string& libMat, bool pxDefGroup = false, bool pxShotGroup = false, bool bump = false, bool planarRefl = false, const std::string& pxMesh = "", bool cullOpacity = false, const D3DXVECTOR4& vec1 = D3DXVECTOR4(0, 0, 0, 0), const D3DXVECTOR4& vec2 = D3DXVECTOR4(0, 0, 0, 0), const D3DXVECTOR4& vec3 = D3DXVECTOR4(0, 0, 0, 0));
	void LoadCar(const std::string& name, const std::string& mesh, const std::string& wheelMesh, const std::string& libMat, const std::string& wheelCoords, const CarDesc& carDesc);
	void LoadCrushObj(const std::string& name, const std::string& mesh, const std::string& libMat, float mass, int subMeshCount, int staticSubMeshes[], int staticCount, bool cullOpacity = false);

	//
	void LoadFxFlow(const std::string& name, const std::string& libMat, graph::FxManager* fxManager, const graph::FxEmitter::ParticleDesc& partDesc, const graph::FxFlowEmitter::FlowDesc& flowDesc, bool worldCoordSys, float timeLife = 0.0f, GraphType graphType = gtEffect);
	void LoadFxSprite(const std::string& name, const std::string& libMat, const Vec3Range& speedPos, const Vec3Range& speedScale, const QuatRange& speedRot, bool autoRot, graph::SceneNode::AnimMode animMode, float animDuration, float frame = 0.0f, float timeLife = 0.0f, bool dir = false, const D3DXVECTOR2& sizes = IdentityVec2, GraphType graphType = gtEffect, bool morph = false);
	void LoadFxPlane(const std::string& name, const std::string& libMat, const Vec3Range& speedPos, const Vec3Range& speedScale, const QuatRange& speedRot, graph::SceneNode::AnimMode animMode, float animDuration, float frame = 0.0f, float timeLife = 0.0f, const D3DXVECTOR2& sizes = IdentityVec2, GraphType graphType = gtEffect);

	void LoadSndSources();
	void LoadEffects();
	void LoadWorld1();
	void LoadWorld2();
	void LoadWorld3();
	void LoadWorld4();
	void LoadWorld5();
	void LoadWorld6();
	void LoadMisc();	
	void LoadCrush();
	void LoadBonus();
	void LoadWeapons();
	void LoadCars();
	void LoadDB();
public:
	//база данных инициализируется в конструктуре, поэтому уже должна иметь имя и владельца
	DataBase(World* world, const std::string& name);
	virtual ~DataBase();

	void Init();
	void Release();

	void Save(const std::string& fileName);
	void Load(const std::string& fileName);
	void ResetDB(const std::string& fileName);
	void ClearDB();

	MapObjLib* GetMapObjLib(MapObjLib::Category category);
	MapObjRec* GetRecord(MapObjLib::Category category, const std::string& name, bool assertFind = true);
	snd::Sound& GetSound(const std::string& name);
};

}

}