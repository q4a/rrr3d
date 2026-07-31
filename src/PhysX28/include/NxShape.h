#ifndef NX_SHAPE_H
#define NX_SHAPE_H

/*
 * PhysX 2.8's NxShape and its seven concrete forms.
 *
 * Transcribed from extern/physx/include/Physics/NxShape.h and the seven
 * Nx*Shape.h files beside it.
 *
 * These are abstract interfaces, as 2.8 has them, and the shim implements them
 * privately. That is what keeps the backend out of the game's translation
 * units: Rock3dGame's stdafx.h pulls px/Physx.h -- and so NxPhysics.h -- into
 * all 52 of its TUs, and not one of them should ever see a Bullet header.
 *
 * The is* helpers are 2.8's and they are not decoration: the game writes
 * `shape->isTriangleMesh()` and `static_cast<NxWheelShape*>(shape)` and expects
 * both to mean what they meant. They are implemented here in terms of a virtual
 * is(), exactly as 2.8 does, so a shim shape only has to answer getType().
 *
 * userData is a public member rather than an accessor pair, again as 2.8 has
 * it. Scene::GetActorFromNxShape reads it directly.
 */

#include "Nxp.h"
#include "NxSimpleTypes.h"
#include "NxVec3.h"
#include "NxMat33.h"
#include "NxMat34.h"
#include "NxShapeDesc.h"
#include "NxRay.h"          /* NxTriangle, for NxTriangleMeshShape::getTriangle */
#include "NxWheelContactData.h"
#include "NxSpringDesc.h"
#include "NxTireFunctionDesc.h"

class NxActor;
class NxTriangleMesh;
class NxConvexMesh;
class NxPlaneShape;
class NxSphereShape;
class NxBoxShape;
class NxCapsuleShape;
class NxConvexShape;
class NxTriangleMeshShape;
class NxWheelShape;

class NxShape
	{
	public:
	virtual NxActor&         getActor() const = 0;
	virtual NxShapeType      getType() const = 0;

	virtual void             setGroup(NxCollisionGroup group) = 0;
	virtual NxCollisionGroup getGroup() const = 0;
	virtual void             setGroupsMask(const NxGroupsMask& mask) = 0;
	virtual const NxGroupsMask getGroupsMask() const = 0;

	virtual void             setLocalPose(const NxMat34& mat) = 0;
	virtual void             setLocalPosition(const NxVec3& vec) = 0;
	virtual void             setLocalOrientation(const NxMat33& mat) = 0;
	virtual NxMat34          getLocalPose() const = 0;

	virtual void             setMaterial(NxMaterialIndex index) = 0;
	virtual NxMaterialIndex  getMaterial() const = 0;

	virtual void             setSkinWidth(NxReal skinWidth) = 0;
	virtual NxReal           getSkinWidth() const = 0;

	virtual void             setFlag(NxShapeFlag flag, bool value) = 0;
	virtual bool             getFlag(NxShapeFlag flag) const = 0;

	/** The 1:1 link to the game's own object. Read directly, not via an
	    accessor -- Scene::GetActorFromNxShape depends on it. */
	void* userData;

	/* 2.8's downcasts, in terms of one virtual. */
	NX_INLINE NxPlaneShape*        isPlane()        { return (NxPlaneShape*)is(NX_SHAPE_PLANE); }
	NX_INLINE const NxPlaneShape*  isPlane() const  { return (const NxPlaneShape*)is(NX_SHAPE_PLANE); }
	NX_INLINE NxSphereShape*       isSphere()       { return (NxSphereShape*)is(NX_SHAPE_SPHERE); }
	NX_INLINE const NxSphereShape* isSphere() const { return (const NxSphereShape*)is(NX_SHAPE_SPHERE); }
	NX_INLINE NxBoxShape*          isBox()          { return (NxBoxShape*)is(NX_SHAPE_BOX); }
	NX_INLINE const NxBoxShape*    isBox() const    { return (const NxBoxShape*)is(NX_SHAPE_BOX); }
	NX_INLINE NxCapsuleShape*      isCapsule()      { return (NxCapsuleShape*)is(NX_SHAPE_CAPSULE); }
	NX_INLINE const NxCapsuleShape* isCapsule() const { return (const NxCapsuleShape*)is(NX_SHAPE_CAPSULE); }
	NX_INLINE NxConvexShape*       isConvexMesh()   { return (NxConvexShape*)is(NX_SHAPE_CONVEX); }
	NX_INLINE const NxConvexShape* isConvexMesh() const { return (const NxConvexShape*)is(NX_SHAPE_CONVEX); }
	NX_INLINE NxTriangleMeshShape* isTriangleMesh() { return (NxTriangleMeshShape*)is(NX_SHAPE_MESH); }
	NX_INLINE const NxTriangleMeshShape* isTriangleMesh() const { return (const NxTriangleMeshShape*)is(NX_SHAPE_MESH); }
	NX_INLINE NxWheelShape*        isWheel()        { return (NxWheelShape*)is(NX_SHAPE_WHEEL); }
	NX_INLINE const NxWheelShape*  isWheel() const  { return (const NxWheelShape*)is(NX_SHAPE_WHEEL); }

	protected:
	NX_INLINE NxShape(): userData(NULL) {}
	virtual ~NxShape() {}

	virtual void* is(NxShapeType type) = 0;
	virtual const void* is(NxShapeType type) const = 0;
	};

class NxPlaneShape : public NxShape
	{
	public:
	virtual void   setPlane(const NxVec3& normal, NxReal d) = 0;
	virtual NxVec3 getPlaneNormal() const = 0;
	virtual NxReal getPlaneD() const = 0;
	virtual void   saveToDesc(NxPlaneShapeDesc& desc) const = 0;
	protected:
	virtual ~NxPlaneShape() {}
	};

class NxSphereShape : public NxShape
	{
	public:
	virtual void   setRadius(NxReal radius) = 0;
	virtual NxReal getRadius() const = 0;
	virtual void   saveToDesc(NxSphereShapeDesc& desc) const = 0;
	protected:
	virtual ~NxSphereShape() {}
	};

class NxBoxShape : public NxShape
	{
	public:
	/** Half-extents, matching the descriptor. */
	virtual void   setDimensions(const NxVec3& dimensions) = 0;
	virtual NxVec3 getDimensions() const = 0;
	virtual void   saveToDesc(NxBoxShapeDesc& desc) const = 0;
	protected:
	virtual ~NxBoxShape() {}
	};

class NxCapsuleShape : public NxShape
	{
	public:
	virtual void   setRadius(NxReal radius) = 0;
	virtual NxReal getRadius() const = 0;
	/** The FULL cylindrical length, along Y. */
	virtual void   setHeight(NxReal height) = 0;
	virtual NxReal getHeight() const = 0;
	virtual void   saveToDesc(NxCapsuleShapeDesc& desc) const = 0;
	protected:
	virtual ~NxCapsuleShape() {}
	};

class NxConvexShape : public NxShape
	{
	public:
	virtual NxConvexMesh& getConvexMesh() = 0;
	virtual const NxConvexMesh& getConvexMesh() const = 0;
	virtual void saveToDesc(NxConvexShapeDesc& desc) const = 0;
	protected:
	virtual ~NxConvexShape() {}
	};

class NxTriangleMeshShape : public NxShape
	{
	public:
	virtual NxTriangleMesh& getTriangleMesh() = 0;
	virtual const NxTriangleMesh& getTriangleMesh() const = 0;

	/*
	 * Fetches a triangle by index. The two trailing bools ask for it in world
	 * space and with a vertex-index remap; GameCar::OnContactModify calls it
	 * as getTriangle(tri, 0, 0, featureIndex, true, true) to rebuild the
	 * friction frame from the touched triangle.
	 */
	virtual void getTriangle(NxTriangle& triangle, NxTriangle* edgeTri, NxU32* edgeFlags,
	                         NxU32 triangleIndex, bool worldSpaceTranslation = true,
	                         bool worldSpaceRotation = true) const = 0;

	virtual void saveToDesc(NxTriangleMeshShapeDesc& desc) const = 0;
	protected:
	virtual ~NxTriangleMeshShape() {}
	};

class NxWheelShape : public NxShape
	{
	public:
	virtual void   setRadius(NxReal radius) = 0;
	virtual NxReal getRadius() const = 0;

	virtual void   setSuspensionTravel(NxReal travel) = 0;
	virtual NxReal getSuspensionTravel() const = 0;

	virtual void setSuspension(const NxSpringDesc& spring) = 0;
	virtual const NxSpringDesc& getSuspension() const = 0;

	virtual void setLongitudalTireForceFunction(const NxTireFunctionDesc& fn) = 0;
	virtual const NxTireFunctionDesc& getLongitudalTireForceFunction() const = 0;
	virtual void setLateralTireForceFunction(const NxTireFunctionDesc& fn) = 0;
	virtual const NxTireFunctionDesc& getLateralTireForceFunction() const = 0;

	virtual void   setInverseWheelMass(NxReal mass) = 0;
	virtual NxReal getInverseWheelMass() const = 0;

	virtual void  setWheelFlags(NxU32 flags) = 0;
	virtual NxU32 getWheelFlags() const = 0;

	/* 2.8 sums motor and brake torque on one axle; they are not a lock. */
	virtual void   setMotorTorque(NxReal torque) = 0;
	virtual NxReal getMotorTorque() const = 0;
	virtual void   setBrakeTorque(NxReal torque) = 0;
	virtual NxReal getBrakeTorque() const = 0;

	virtual void   setSteerAngle(NxReal angle) = 0;
	virtual NxReal getSteerAngle() const = 0;

	virtual void   setAxleSpeed(NxReal speed) = 0;
	virtual NxReal getAxleSpeed() const = 0;

	virtual void setUserWheelContactModify(NxUserWheelContactModify* callback) = 0;
	virtual NxUserWheelContactModify* getUserWheelContactModify() = 0;

	/** False when the wheel is not touching anything this step. */
	virtual bool getContact(NxWheelContactData& contact) const = 0;

	virtual void saveToDesc(NxWheelShapeDesc& desc) const = 0;
	protected:
	virtual ~NxWheelShape() {}
	};

#endif /* NX_SHAPE_H */
