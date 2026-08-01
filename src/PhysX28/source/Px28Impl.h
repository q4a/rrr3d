#ifndef PX28_IMPL_H
#define PX28_IMPL_H

/*
 * The concrete classes behind the 2.8 interfaces, and the only place Bullet
 * appears.
 *
 * This file is under source/ and never installed. include/ stays free of every
 * backend header, which is what keeps Bullet's types and macros out of the 52
 * Rock3dGame translation units that pull in NxPhysics.h through stdafx.h.
 *
 * Ownership: the shim holds exactly one Bullet object per 2.8 object, and the
 * wrapper dies only from the 2.8-named release call. So a wrapper pointer stays
 * valid for exactly as long as 2.8 promised it would, and the game's
 * static_cast<NxWheelShape*>(shape) and shape->isTriangleMesh() keep meaning
 * what they meant.
 */

#include "NxPhysics.h"

#include <btBulletDynamicsCommon.h>

#include <map>
#include <vector>

namespace px28
{

/* NxVec3 and btVector3 are both (x, y, z) with no handedness of their own, so
   these are componentwise and no convention can move across them. The game's
   Z-up world stays Z-up; the shim never reinterprets an axis. */
inline btVector3 ToBullet(const NxVec3& v) { return btVector3(v.x, v.y, v.z); }
inline NxVec3    ToNx(const btVector3& v)  { return NxVec3(v.x(), v.y(), v.z()); }

inline btQuaternion ToBullet(const NxQuat& q)
	{
	return btQuaternion(q.x, q.y, q.z, q.w);
	}

inline NxQuat ToNx(const btQuaternion& q)
	{
	NxQuat out;
	out.setXYZW(q.x(), q.y(), q.z(), q.w());
	return out;
	}

btTransform ToBullet(const NxMat34& m);
NxMat34     ToNx(const btTransform& t);

/*
 * Called when the game reaches a path the shim does not implement yet.
 *
 * Deliberately fatal and deliberately named. A stub that returns zero would let
 * the game run and behave subtly wrongly, which is the failure mode this whole
 * approach exists to avoid -- there would be nothing to see and nothing in a
 * log. Aborting says which call, so the next thing to implement names itself.
 */
[[noreturn]] void Unimplemented(const char* what);

class Scene;
class Actor;

/* ------------------------------------------------------------------ shapes */

class Shape: public NxShape
	{
	public:
	Shape(Actor& actor, const NxShapeDesc& desc);
	virtual ~Shape();

	btCollisionShape* bulletShape() const { return _bulletShape; }
	const btTransform& localPose() const { return _localPose; }

	/* --- NxShape ---------------------------------------------------------- */
	virtual NxActor& getActor() const;
	virtual void     setGroup(NxCollisionGroup group);
	virtual NxCollisionGroup getGroup() const;
	virtual void     setGroupsMask(const NxGroupsMask& mask);
	virtual const NxGroupsMask getGroupsMask() const;
	virtual void     setFlag(NxShapeFlag flag, bool value);
	virtual bool     getFlag(NxShapeFlag flag) const;
	virtual void     setLocalPose(const NxMat34& mat);
	virtual NxMat34  getLocalPose() const;
	virtual NxMat34  getGlobalPose() const;
	virtual void     setMaterial(NxMaterialIndex index);
	virtual NxMaterialIndex getMaterial() const;
	virtual void     setSkinWidth(NxReal width);
	virtual NxReal   getSkinWidth() const;
	virtual NxShapeType getType() const;

	protected:
	Actor* _actor;
	btCollisionShape* _bulletShape;
	btTransform _localPose;
	NxCollisionGroup _group;
	NxGroupsMask _groupsMask;
	NxMaterialIndex _material;
	NxU32 _flags;
	NxReal _skinWidth;
	NxShapeType _type;
	};

class BoxShape: public Shape
	{
	public:
	BoxShape(Actor& actor, const NxBoxShapeDesc& desc);

	virtual void   setDimensions(const NxVec3& dim);
	virtual NxVec3 getDimensions() const;
	};

/* ------------------------------------------------------------------ actors */

class Actor: public NxActor
	{
	public:
	Actor(Scene& scene, const NxActorDesc& desc);
	virtual ~Actor();

	btRigidBody* body() const { return _body; }
	Scene& scene() const { return *_scene; }

	/* Rebuilt whenever the shape set changes. 2.8 does *not* recompute mass
	   when shapes are added or removed from a live actor, and neither does
	   this -- see the note on _mass below. */
	void rebuildCompoundShape();

	/* --- NxActor ---------------------------------------------------------- */
	virtual NxScene& getScene() const;
	virtual void     setGlobalPose(const NxMat34& mat);
	virtual void     setGlobalPosition(const NxVec3& vec);
	virtual void     setGlobalOrientationQuat(const NxQuat& q);
	virtual NxMat34  getGlobalPose() const;
	virtual NxVec3   getGlobalPosition() const;
	virtual NxQuat   getGlobalOrientationQuat() const;

	virtual NxShape*       createShape(const NxShapeDesc& desc);
	virtual void           releaseShape(NxShape& shape);
	virtual NxU32          getNbShapes() const;
	virtual NxShape*const* getShapes() const;

	virtual bool isDynamic() const;

	virtual void   setMass(NxReal mass);
	virtual NxReal getMass() const;

	virtual void   setLinearVelocity(const NxVec3& v);
	virtual void   setAngularVelocity(const NxVec3& v);
	virtual NxVec3 getLinearVelocity() const;
	virtual NxVec3 getAngularVelocity() const;

	virtual NxVec3 getLinearMomentum() const;
	virtual void   setLinearMomentum(const NxVec3& p);
	virtual NxVec3 getAngularMomentum() const;
	virtual void   setAngularMomentum(const NxVec3& l);

	virtual void addForce(const NxVec3& force, NxForceMode mode, bool wakeup);
	virtual void addForceAtPos(const NxVec3& force, const NxVec3& pos,
	                           NxForceMode mode, bool wakeup);
	virtual void addLocalForce(const NxVec3& force, NxForceMode mode, bool wakeup);
	virtual void addTorque(const NxVec3& torque, NxForceMode mode, bool wakeup);
	virtual void addLocalTorque(const NxVec3& torque, NxForceMode mode, bool wakeup);

	virtual void  raiseActorFlag(NxActorFlag flag);
	virtual void  clearActorFlag(NxActorFlag flag);
	virtual bool  readActorFlag(NxActorFlag flag) const;
	virtual NxU32 getContactReportFlags() const;
	virtual void  setContactReportFlags(NxU32 flags);

	/* Not implemented yet -- each aborts naming itself. */
	virtual void    saveToDesc(NxActorDescBase& desc);
	virtual void    setGlobalOrientation(const NxMat33& mat);
	virtual NxMat33 getGlobalOrientation() const;
	virtual void    setCMassOffsetLocalPose(const NxMat34& mat);
	virtual void    setCMassOffsetLocalPosition(const NxVec3& vec);
	virtual NxVec3  getCMassLocalPosition() const;
	virtual NxMat34 getCMassGlobalPose() const;
	virtual NxVec3  getMassSpaceInertiaTensor() const;
	virtual NxReal  computeKineticEnergy() const;
	virtual void    setLinearDamping(NxReal damping);
	virtual void    wakeUp(NxReal wakeCounterValue);
	virtual void    putToSleep();
	virtual bool    isSleeping() const;

	private:
	Scene* _scene;
	btRigidBody* _body;
	btCompoundShape* _compound;

	/* Descriptor order, not Bullet's. Actor::UnpackActorShapeListIncludeChildren
	   walks getShapes() positionally, so creation order is part of the
	   contract. */
	std::vector<NxShape*> _shapes;

	bool _dynamic;

	/* Computed once, at creation, from the descriptor -- exactly as 2.8 did.
	   Recomputing when shapes change would be "more correct" and would silently
	   change every car's handling. */
	NxReal _mass;

	NxU32 _actorFlags;
	NxU32 _contactReportFlags;
	};

/* ------------------------------------------------------------------- scene */

class Scene: public NxScene
	{
	public:
	explicit Scene(const NxSceneDesc& desc);
	virtual ~Scene();

	btDiscreteDynamicsWorld& world() { return *_world; }
	NxReal skinWidth() const { return _skinWidth; }

	/* --- NxScene ---------------------------------------------------------- */
	virtual NxActor* createActor(const NxActorDesc& desc);
	virtual void     releaseActor(NxActor& actor);
	virtual NxU32    getNbActors() const;
	virtual NxActor**getActors();

	virtual void simulate(NxReal elapsedTime);
	virtual void flushStream();
	virtual bool fetchResults(NxSimulationStatus status, bool block);
	virtual void setTiming(NxReal maxTimestep, NxU32 maxIter, NxTimeStepMethod method);

	virtual void setGravity(const NxVec3& gravity);
	virtual void getGravity(NxVec3& gravity) const;

	/* Not implemented yet -- each aborts naming itself. */
	virtual NxMaterial* createMaterial(const NxMaterialDesc& desc);
	virtual void        releaseMaterial(NxMaterial& material);
	virtual NxMaterial* getMaterialFromIndex(NxMaterialIndex index);
	virtual NxU32       getNbMaterials() const;

	virtual void  setGroupCollisionFlag(NxCollisionGroup g1, NxCollisionGroup g2, bool enable);
	virtual bool  getGroupCollisionFlag(NxCollisionGroup g1, NxCollisionGroup g2) const;
	virtual void  setFilterOps(NxFilterOp op0, NxFilterOp op1, NxFilterOp op2);
	virtual void  setFilterBool(bool flag);
	virtual void  setActorPairFlags(NxActor& a, NxActor& b, NxU32 flags);
	virtual NxU32 getActorPairFlags(NxActor& a, NxActor& b) const;
	virtual void  setShapePairFlags(NxShape& a, NxShape& b, NxU32 flags);

	virtual NxShape* raycastClosestShape(const NxRay& worldRay, NxShapesType shapeType,
	                                     NxRaycastHit& hit, NxU32 groups, NxReal maxDist,
	                                     NxU32 hintFlags, const NxGroupsMask* groupsMask,
	                                     NxShape** cache) const;

	virtual void setUserContactReport(NxUserContactReport* callback);
	virtual void setUserContactModify(NxUserContactModify* callback);
	virtual void setUserNotify(NxUserNotify* callback);

	private:
	btDefaultCollisionConfiguration* _config;
	btCollisionDispatcher* _dispatcher;
	btBroadphaseInterface* _broadphase;
	btSequentialImpulseConstraintSolver* _solver;
	btDiscreteDynamicsWorld* _world;

	std::vector<NxActor*> _actors;

	/* 2.8's skin width is a *global* SDK parameter that shapes inherit by
	   leaving their own at -1. Captured per scene at creation so a shape can
	   resolve its own without reaching back to the SDK. */
	NxReal _skinWidth;

	NxReal _pendingStep;
	};

} /* namespace px28 */

#endif /* PX28_IMPL_H */
