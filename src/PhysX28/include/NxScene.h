#ifndef NX_SCENE_H
#define NX_SCENE_H

/*
 * PhysX 2.8's NxScene, NxMaterial and the mesh handles.
 *
 * Transcribed from extern/physx/include/Physics/NxScene.h, NxMaterial.h,
 * NxTriangleMesh.h and NxConvexMesh.h.
 *
 * createMaterial is the method with a hidden contract. It hands out indices
 * sequentially from 1, with 0 reserved for the scene's default material, and
 * db.xml serialises the result -- <materialIndex>4</materialIndex> is the track
 * and 5 is the border, because DataBase.cpp creates them fifth and sixth.
 * Nothing in the API says so, and getting the allocation order wrong silently
 * swaps every surface's friction.
 *
 * simulate/flushStream/fetchResults is 2.8's three-call step, and the engine
 * uses all three: Scene::Compute does simulate(dt), flushStream(), then
 * fetchResults(NX_RIGID_BODY_FINISHED, true).
 *
 * setGroupCollisionFlag is per-scene here. It is worth noting because the
 * obvious modern equivalents are global, and this game builds an 8x8 group
 * matrix in Scene::Scene.
 */

#include "Nxp.h"
#include "NxSimpleTypes.h"
#include "NxVec3.h"
#include "NxRay.h"
#include "NxSceneDesc.h"
#include "NxMaterialDesc.h"
#include "NxActorDesc.h"
#include "NxUserContactReport.h"

class NxActor;
class NxShape;

class NxMaterial
	{
	public:
	virtual void            setDynamicFriction(NxReal value) = 0;
	virtual NxReal          getDynamicFriction() const = 0;
	virtual void            setStaticFriction(NxReal value) = 0;
	virtual NxReal          getStaticFriction() const = 0;
	virtual void            setRestitution(NxReal value) = 0;
	virtual NxReal          getRestitution() const = 0;
	virtual void            setFlags(NxU32 flags) = 0;
	virtual NxU32           getFlags() const = 0;
	virtual void            setDirOfAnisotropy(const NxVec3& dir) = 0;
	virtual NxVec3          getDirOfAnisotropy() const = 0;
	virtual void            loadFromDesc(const NxMaterialDesc& desc) = 0;
	virtual void            saveToDesc(NxMaterialDesc& desc) const = 0;
	/** Serialised into saved games and db.xml. */
	virtual NxMaterialIndex getMaterialIndex() const = 0;
	protected:
	virtual ~NxMaterial() {}
	};

class NxTriangleMesh
	{
	public:
	virtual NxU32 getCount(NxU32 subMeshIndex, NxU32 flags) const = 0;
	protected:
	virtual ~NxTriangleMesh() {}
	};

class NxConvexMesh
	{
	public:
	virtual NxU32 getCount(NxU32 flags) const = 0;
	protected:
	virtual ~NxConvexMesh() {}
	};

class NxScene
	{
	public:
	/* ---- actors -------------------------------------------------------- */
	virtual NxActor* createActor(const NxActorDesc& desc) = 0;
	virtual void     releaseActor(NxActor& actor) = 0;
	virtual NxU32    getNbActors() const = 0;
	virtual NxActor**getActors() = 0;

	/* ---- materials ----------------------------------------------------- */
	/** Indices are handed out sequentially from 1; 0 is the scene default. */
	virtual NxMaterial* createMaterial(const NxMaterialDesc& desc) = 0;
	virtual void        releaseMaterial(NxMaterial& material) = 0;
	virtual NxMaterial* getMaterialFromIndex(NxMaterialIndex index) = 0;
	virtual NxU32       getNbMaterials() const = 0;

	/* ---- stepping ------------------------------------------------------ */
	virtual void simulate(NxReal elapsedTime) = 0;
	virtual void flushStream() = 0;
	virtual bool fetchResults(NxSimulationStatus status, bool block = false) = 0;
	virtual void setTiming(NxReal maxTimestep = 1.0f / 60.0f, NxU32 maxIter = 8,
	                       NxTimeStepMethod method = NX_TIMESTEP_FIXED) = 0;

	virtual void   setGravity(const NxVec3& gravity) = 0;
	virtual void   getGravity(NxVec3& gravity) const = 0;

	/* ---- filtering ----------------------------------------------------- */
	/** Per-scene, not global. */
	virtual void setGroupCollisionFlag(NxCollisionGroup g1, NxCollisionGroup g2, bool enable) = 0;
	virtual bool getGroupCollisionFlag(NxCollisionGroup g1, NxCollisionGroup g2) const = 0;
	virtual void setFilterOps(NxFilterOp op0, NxFilterOp op1, NxFilterOp op2) = 0;
	virtual void setFilterBool(bool flag) = 0;
	virtual void setActorPairFlags(NxActor& a, NxActor& b, NxU32 flags) = 0;
	virtual NxU32 getActorPairFlags(NxActor& a, NxActor& b) const = 0;
	virtual void setShapePairFlags(NxShape& a, NxShape& b, NxU32 flags) = 0;

	/* ---- queries ------------------------------------------------------- */
	/*
	 * `groups` is a 32-bit MASK of collision groups, not a single group index
	 * -- which is the other half of why NxShapeDesc::checkValid rejects a group
	 * of 32 or more. hintFlags says which NxRaycastHit fields the caller wants
	 * filled in.
	 */
	virtual NxShape* raycastClosestShape(const NxRay& worldRay, NxShapesType shapeType,
	                                     NxRaycastHit& hit,
	                                     NxU32 groups = 0xffffffff,
	                                     NxReal maxDist = NX_MAX_F32,
	                                     NxU32 hintFlags = 0xffffffff,
	                                     const NxGroupsMask* groupsMask = NULL,
	                                     NxShape** cache = NULL) const = 0;

	/* ---- callbacks ----------------------------------------------------- */
	virtual void setUserContactReport(NxUserContactReport* callback) = 0;
	virtual void setUserContactModify(NxUserContactModify* callback) = 0;
	virtual void setUserNotify(NxUserNotify* callback) = 0;

	void* userData;

	protected:
	NX_INLINE NxScene(): userData(NULL) {}
	virtual ~NxScene() {}
	};

#endif /* NX_SCENE_H */
