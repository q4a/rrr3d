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
#include "NxCooking.h"
#include "Px28Contact.h"

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
class TriangleMesh;

/* ------------------------------------------------------------------ shapes */

/*
 * The state every shape carries, independent of which Nx*Shape interface the
 * concrete class has to derive from.
 *
 * The split exists because 2.8's downcasts are real inheritance -- isBox()
 * returns an NxBoxShape*, and the game does static_cast<NxWheelShape*>(shape)
 * -- so BoxShape must derive from NxBoxShape, not from a shared Shape class.
 * Without the split, each of the seven shape types would repeat the same
 * fifteen accessors.
 */
class ShapeState
	{
	public:
	ShapeState(Actor& actor, const NxShapeDesc& desc, NxShapeType type);

	/* Virtual, and that is why shapes are owned through ShapeState* rather than
	   NxShape*: 2.8 declares ~NxShape() protected, so the owner cannot delete
	   through the interface it hands the game. */
	virtual ~ShapeState();

	btCollisionShape* bulletShape() const { return _bulletShape; }
	const btTransform& localPose() const { return _localPose; }

	/* The shape's own, or the scene's global when it is left at -1. */
	NxReal resolvedSkinWidth() const;

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

	/* False for a mesh shape: the btBvhTriangleMeshShape belongs to the cooked
	   TriangleMesh and is shared between every instance of it. */
	bool _ownsBulletShape;
	};

/*
 * The common NxShape surface, mixed into whichever Nx*Shape interface a
 * concrete shape implements.
 */
template<class NxInterface>
class ShapeImpl: public NxInterface, public ShapeState
	{
	public:
	ShapeImpl(Actor& actor, const NxShapeDesc& desc, NxShapeType type)
		: ShapeState(actor, desc, type) {}

	virtual NxActor& getActor() const;

	virtual NxShapeType getType() const { return _type; }

	virtual void setGroup(NxCollisionGroup group) { _group = group; }
	virtual NxCollisionGroup getGroup() const     { return _group; }

	/* Written directly by Weapon.cpp:293,310 and GameObject.cpp:202, and
	   round-tripped verbatim -- getGroup() is serialised. */
	virtual void setGroupsMask(const NxGroupsMask& mask) { _groupsMask = mask; }
	virtual const NxGroupsMask getGroupsMask() const     { return _groupsMask; }

	virtual void setFlag(NxShapeFlag flag, bool value)
		{
		if (value)
			_flags |= flag;
		else
			_flags &= ~static_cast<NxU32>(flag);
		}
	virtual bool getFlag(NxShapeFlag flag) const { return (_flags & flag) != 0; }

	virtual void setLocalPose(const NxMat34& mat);
	virtual void setLocalPosition(const NxVec3& vec);
	virtual void setLocalOrientation(const NxMat33& mat);
	virtual NxMat34 getLocalPose() const { return ToNx(_localPose); }

	virtual void setMaterial(NxMaterialIndex index) { _material = index; }
	virtual NxMaterialIndex getMaterial() const     { return _material; }

	virtual void   setSkinWidth(NxReal width) { _skinWidth = width; }
	virtual NxReal getSkinWidth() const       { return _skinWidth; }

	protected:
	/* One virtual behind all fourteen of 2.8's isX() helpers. */
	virtual void* is(NxShapeType type)
		{
		return type == _type ? static_cast<NxInterface*>(this) : NULL;
		}
	virtual const void* is(NxShapeType type) const
		{
		return type == _type ? static_cast<const NxInterface*>(this) : NULL;
		}
	};

class BoxShape: public ShapeImpl<NxBoxShape>
	{
	public:
	BoxShape(Actor& actor, const NxBoxShapeDesc& desc);

	virtual void   setDimensions(const NxVec3& dimensions);
	virtual NxVec3 getDimensions() const;
	virtual void   saveToDesc(NxBoxShapeDesc& desc) const;
	};

class SphereShape: public ShapeImpl<NxSphereShape>
	{
	public:
	SphereShape(Actor& actor, const NxSphereShapeDesc& desc);

	virtual void   setRadius(NxReal radius);
	virtual NxReal getRadius() const;
	virtual void   saveToDesc(NxSphereShapeDesc& desc) const;
	};

/*
 * A mesh instance. The btBvhTriangleMeshShape is owned by the TriangleMesh and
 * shared between every shape that references it -- TriangleMesh::GetOrCreateTri
 * reference-counts one per (mesh, scale) pair, so several actors legitimately
 * point at the same cooked mesh.
 *
 * Which is why ~ShapeState must not delete this one, and why _ownsBulletShape
 * exists.
 */
class TriangleMeshShape: public ShapeImpl<NxTriangleMeshShape>
	{
	public:
	TriangleMeshShape(Actor& actor, const NxTriangleMeshShapeDesc& desc);
	virtual ~TriangleMeshShape();

	virtual NxTriangleMesh& getTriangleMesh();
	virtual const NxTriangleMesh& getTriangleMesh() const;
	virtual void getTriangle(NxTriangle& triangle, NxTriangle* edgeTri, NxU32* edgeFlags,
	                         NxU32 triangleIndex, bool worldSpaceTranslation,
	                         bool worldSpaceRotation) const;
	virtual void saveToDesc(NxTriangleMeshShapeDesc& desc) const;

	private:
	TriangleMesh* _mesh;
	};

/*
 * A plane. Only Scene::CreateGroundPlane makes one and its body is entirely
 * commented out, so nothing in the game currently creates a plane shape -- but
 * db.xml can deserialise one, so it exists.
 *
 * 2.8's plane is n.X = d in WORLD space, ignoring the shape's pose. Note also
 * the pre-existing bug at Physx.cpp:878, where SetDist passes NxVec3(value) as
 * the normal; that is the game's and is inherited faithfully rather than fixed.
 */
class PlaneShape: public ShapeImpl<NxPlaneShape>
	{
	public:
	PlaneShape(Actor& actor, const NxPlaneShapeDesc& desc);

	virtual void   setPlane(const NxVec3& normal, NxReal d);
	virtual NxVec3 getPlaneNormal() const;
	virtual NxReal getPlaneD() const;
	virtual void   saveToDesc(NxPlaneShapeDesc& desc) const;

	private:
	NxVec3 _normal;
	NxReal _d;
	};

class CapsuleShape: public ShapeImpl<NxCapsuleShape>
	{
	public:
	CapsuleShape(Actor& actor, const NxCapsuleShapeDesc& desc);

	virtual void   setRadius(NxReal radius);
	virtual NxReal getRadius() const;
	virtual void   setHeight(NxReal height);
	virtual NxReal getHeight() const;
	virtual void   saveToDesc(NxCapsuleShapeDesc& desc) const;

	private:
	NxReal _radius;
	NxReal _height;
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
	/* Pushes NX_AF_DISABLE_RESPONSE down to Bullet's CF_NO_CONTACT_RESPONSE. */
	void applyResponseFlag();

	/* The actor origin, which is the body transform with the COM offset undone. */
	btTransform actorTransform() const;
	void setActorTransform(const btTransform& actorWorld);

	Scene* _scene;
	btRigidBody* _body;
	btCompoundShape* _compound;

	/* Descriptor order, not Bullet's. Actor::UnpackActorShapeListIncludeChildren
	   walks getShapes() positionally, so creation order is part of the
	   contract.
	 *
	   Two vectors over one because getShapes() has to hand back a contiguous
	   NxShape*const*, while ownership and the compound rebuild need the
	   ShapeState side. Same length, same order, always. */
	std::vector<NxShape*> _shapes;
	std::vector<ShapeState*> _shapeStates;

	bool _dynamic;

	/* Computed once, at creation, from the descriptor -- exactly as 2.8 did.
	   Recomputing when shapes change would be "more correct" and would silently
	   change every car's handling. */
	NxReal _mass;

	NxU32 _actorFlags;
	NxU32 _contactReportFlags;

	/*
	 * 2.8 keeps the actor's pose and its centre of mass separate: globalPose is
	 * the actor origin and massLocalPose offsets the COM from it. Bullet does
	 * not -- a btRigidBody's world transform *is* its centre of mass.
	 *
	 * So the offset is held here and applied at every boundary:
	 *
	 *   bodyWorld  = actorWorld * comOffset
	 *   actorWorld = bodyWorld  * comOffset^-1
	 *   compound child = comOffset^-1 * shapeLocalPose
	 *
	 * This is not bookkeeping. Every car sets a COM offset through
	 * bfLockCenterOfMass, and Physx.cpp:1845 applies massLocalPose on the actor
	 * creation path -- so treating the body origin as the actor origin would
	 * put every shape in the wrong place and make addLocalForce generate torque
	 * 2.8 never produced.
	 */
	btTransform _comOffset;
	};

/* ------------------------------------------------------------------ meshes */

class TriangleMesh: public NxTriangleMesh
	{
	public:
	TriangleMesh(const std::vector<float>& vertices, const std::vector<int>& indices);
	virtual ~TriangleMesh();

	virtual NxU32 getCount(NxU32 subMeshIndex, NxU32 flags) const;

	btBvhTriangleMeshShape* shape() const { return _shape; }

	/*
	 * Deferred destruction, which is 2.8's contract and not an improvement on
	 * it. releaseTriangleMesh does not destroy a mesh that shapes still
	 * reference -- 2.8 reference counts it, which is why NxTriangleMesh has a
	 * getReferenceCount() at all -- and destruction happens when the last
	 * shape referencing it goes away.
	 *
	 * The game depends on this and the sequence is not obscure.
	 * px::Actor::ReloadNxShape (Physx.cpp:1735) reads
	 *
	 *     shape->SetNxShape(0); CreateNxShape(shape); releaseShape(*oldNxShape);
	 *
	 * -- the new shape is built BEFORE the old one is released -- and
	 * TriangleMeshShape::SyncScale releases the mesh before either. So between
	 * those two lines the actor holds a shape whose mesh the game has already
	 * released, and building the new shape walks every shape on the actor to
	 * rebuild the compound. Destroying the mesh eagerly makes that a
	 * use-after-free of the btBvhTriangleMeshShape, which crashes inside
	 * Bullet with a stack that names neither the mesh nor the release.
	 */
	void addShapeRef() { ++_shapeRefs; }
	static void releaseShapeRef(TriangleMesh* mesh);
	static void releaseFromSdk(TriangleMesh* mesh);

	/* In the mesh's own space; the shape applies its pose. False if the index
	   is out of range, which is how a stale feature index fails visibly. */
	bool getTriangleVertices(NxU32 triangleIndex, NxVec3 vertices[3]) const;

	private:
	/* Owned by value: btTriangleIndexVertexArray keeps pointers into these, and
	   the descriptor they came from is freed by FreeMesh the moment the cook
	   returns. */
	std::vector<float> _vertices;
	std::vector<int> _indices;

	btTriangleIndexVertexArray* _array;
	btBvhTriangleMeshShape* _shape;

	/* Shapes currently referencing this mesh, and whether the game has asked
	   for it to go. It is destroyed when both say it can be. */
	unsigned _shapeRefs;
	bool _sdkReleased;
	};

/* --------------------------------------------------------------- materials */

class Material: public NxMaterial
	{
	public:
	Material(const NxMaterialDesc& desc, NxMaterialIndex index);

	virtual void   setDynamicFriction(NxReal value) { _desc.dynamicFriction = value; }
	virtual NxReal getDynamicFriction() const       { return _desc.dynamicFriction; }
	virtual void   setStaticFriction(NxReal value)  { _desc.staticFriction = value; }
	virtual NxReal getStaticFriction() const        { return _desc.staticFriction; }
	virtual void   setRestitution(NxReal value)     { _desc.restitution = value; }
	virtual NxReal getRestitution() const           { return _desc.restitution; }
	virtual void   setFlags(NxU32 flags)            { _desc.flags = flags; }
	virtual NxU32  getFlags() const                 { return _desc.flags; }

	virtual void   setDirOfAnisotropy(const NxVec3& dir) { _desc.dirOfAnisotropy = dir; }
	virtual NxVec3 getDirOfAnisotropy() const            { return _desc.dirOfAnisotropy; }

	virtual void loadFromDesc(const NxMaterialDesc& desc) { _desc = desc; }
	virtual void saveToDesc(NxMaterialDesc& desc) const   { desc = _desc; }

	/* Serialised into db.xml and saved games, so it round-trips verbatim. */
	virtual NxMaterialIndex getMaterialIndex() const { return _index; }

	const NxMaterialDesc& desc() const { return _desc; }

	private:
	NxMaterialDesc _desc;
	NxMaterialIndex _index;
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

	virtual NxMaterial* createMaterial(const NxMaterialDesc& desc);
	virtual void        releaseMaterial(NxMaterial& material);
	virtual NxMaterial* getMaterialFromIndex(NxMaterialIndex index);
	virtual NxU32       getNbMaterials() const;

	virtual void  setGroupCollisionFlag(NxCollisionGroup g1, NxCollisionGroup g2, bool enable);
	virtual bool  getGroupCollisionFlag(NxCollisionGroup g1, NxCollisionGroup g2) const;
	virtual void  setActorPairFlags(NxActor& a, NxActor& b, NxU32 flags);
	virtual NxU32 getActorPairFlags(NxActor& a, NxActor& b) const;

	/* Consulted from Bullet's broadphase filter callback. */
	bool shouldCollide(const Actor& a, const Actor& b) const;

	/* Not implemented yet -- each aborts naming itself. */
	virtual void  setFilterOps(NxFilterOp op0, NxFilterOp op1, NxFilterOp op2);
	virtual void  setFilterBool(bool flag);
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

	/* Indexed BY material index, so slot 0 is the scene default and a released
	   material leaves a hole rather than shifting everything after it down --
	   which would silently repoint every shape that referenced a later one. */
	std::vector<Material*> _materials;

	/* 2.8's skin width is a *global* SDK parameter that shapes inherit by
	   leaving their own at -1. Captured per scene at creation so a shape can
	   resolve its own without reaching back to the SDK. */
	NxReal _skinWidth;

	NxReal _pendingStep;

	/*
	 * 2.8 supports exactly 32 collision groups -- NxShapeDesc::checkValid
	 * rejects a group of 32 or more -- and the matrix is per scene, not global,
	 * which is worth knowing because the obvious modern equivalents are global.
	 *
	 * Every pair starts enabled, and Scene::Scene disables fifteen of them.
	 */
	bool _groupCollision[32][32];

	/* setActorPairFlags(NX_IGNORE_PAIR) is per-pair and cannot live in filter
	   data, so it belongs here and is consulted from the pair filter. Keyed on
	   the ordered pair with the lower pointer first, so lookup does not depend
	   on which way round the caller passed them. */
	std::map<std::pair<const NxActor*, const NxActor*>, NxU32> _actorPairFlags;

	btOverlapFilterCallback* _filter;

	/* Walks Bullet's manifolds after a step and delivers onContactNotify. */
	void collectContacts(NxReal elapsedTime);

	/* And before the solver runs, offers each contact to onContactConstraint. */
	void modifyContacts();

	NxUserContactReport* _contactReport;
	NxUserContactModify* _contactModify;
	NxUserNotify* _userNotify;

	/* How NxGroupsMask combines; Weapon.cpp changes these around a raycast. */
	NxFilterOp _filterOp0;
	NxFilterOp _filterOp1;
	NxFilterOp _filterOp2;
	bool _filterBool;

	std::map<std::pair<const NxShape*, const NxShape*>, NxU32> _shapePairFlags;

	/* Recorded by setTiming; the step is what fetchResults' single substep
	   depends on being 1/60. */
	NxReal _maxTimestep;
	NxU32 _maxIter;

	/* One record per reported pair, rebuilt each step. Held by the scene rather
	   than by the callback so the storage the game's NxConstContactStream points
	   at outlives the call it was handed to -- 2.8 required it be read
	   synchronously, and this keeps that true without depending on it. */
	std::vector<ContactStreamRecord*> _contactStreams;
	};

} /* namespace px28 */

#endif /* PX28_IMPL_H */
