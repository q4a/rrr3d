#ifndef NX_ACTOR_H
#define NX_ACTOR_H

/*
 * PhysX 2.8's NxActor.
 *
 * Transcribed from extern/physx/include/Physics/NxActor.h.
 *
 * Two guarantees the shim inherits, both of which the engine depends on:
 *
 *   getShapes() returns the shapes in the order they were created.
 *   Actor::UnpackActorShapeListIncludeChildren walks the returned array
 *   positionally and pairs each entry with the px::Shape that produced it, so
 *   any reordering silently mis-assigns every shape on the actor.
 *
 *   userData is a public member, and Scene::GetActorFromNx reads it directly.
 *
 * Three semantics recorded here because a plausible substitute gets them wrong:
 *
 *   addLocalForce and addLocalTorque apply at the CENTRE OF MASS, in the
 *   actor's frame, and generate no incidental torque. The obvious modern
 *   equivalent applies at the actor ORIGIN, which does. Every car in this game
 *   sets a centre-of-mass offset, so the difference is not academic.
 *
 *   getAngularMomentum is R*I*R^T*w -- the world inertia tensor times angular
 *   velocity -- not mass times velocity. GameCar::StabilizeForce reads it and
 *   writes it back every step, including on the path where nothing changed, so
 *   a get/set that is not the identity makes every car's spin drift forever.
 *
 *   NX_SLEEP_INTERVAL is wakeUp's default argument and is part of the API.
 */

#include "Nxp.h"
#include "NxSimpleTypes.h"
#include "NxVec3.h"
#include "NxMat33.h"
#include "NxMat34.h"
#include "NxQuat.h"
#include "NxActorDesc.h"

#define NX_SLEEP_INTERVAL (20.0f * 0.02f)

class NxScene;
class NxShape;

class NxActor
	{
	public:
	virtual NxScene& getScene() const = 0;

	virtual void saveToDesc(NxActorDescBase& desc) = 0;

	/* ---- pose ---------------------------------------------------------- */
	virtual void    setGlobalPose(const NxMat34& mat) = 0;
	virtual void    setGlobalPosition(const NxVec3& vec) = 0;
	virtual void    setGlobalOrientation(const NxMat33& mat) = 0;
	virtual void    setGlobalOrientationQuat(const NxQuat& q) = 0;
	virtual NxMat34 getGlobalPose() const = 0;
	virtual NxVec3  getGlobalPosition() const = 0;
	virtual NxMat33 getGlobalOrientation() const = 0;
	virtual NxQuat  getGlobalOrientationQuat() const = 0;

	/* ---- shapes -------------------------------------------------------- */
	virtual NxShape*       createShape(const NxShapeDesc& desc) = 0;
	virtual void           releaseShape(NxShape& shape) = 0;
	virtual NxU32          getNbShapes() const = 0;
	/** In creation order. See the note above. */
	virtual NxShape*const* getShapes() const = 0;

	/* ---- flags --------------------------------------------------------- */
	virtual void  raiseActorFlag(NxActorFlag flag) = 0;
	virtual void  clearActorFlag(NxActorFlag flag) = 0;
	virtual bool  readActorFlag(NxActorFlag flag) const = 0;
	virtual NxU32 getContactReportFlags() const = 0;
	virtual void  setContactReportFlags(NxU32 flags) = 0;

	virtual bool isDynamic() const = 0;

	/* ---- mass ---------------------------------------------------------- */
	virtual void    setCMassOffsetLocalPose(const NxMat34& mat) = 0;
	virtual void    setCMassOffsetLocalPosition(const NxVec3& vec) = 0;
	virtual NxVec3  getCMassLocalPosition() const = 0;
	virtual NxMat34 getCMassGlobalPose() const = 0;
	virtual void    setMass(NxReal mass) = 0;
	virtual NxReal  getMass() const = 0;
	virtual NxVec3  getMassSpaceInertiaTensor() const = 0;
	virtual NxReal  computeKineticEnergy() const = 0;

	/* ---- velocity and momentum ----------------------------------------- */
	virtual void   setLinearDamping(NxReal damping) = 0;
	virtual void   setLinearVelocity(const NxVec3& v) = 0;
	virtual void   setAngularVelocity(const NxVec3& v) = 0;
	virtual NxVec3 getLinearVelocity() const = 0;
	virtual NxVec3 getAngularVelocity() const = 0;
	/** mass * v */
	virtual NxVec3 getLinearMomentum() const = 0;
	virtual void   setLinearMomentum(const NxVec3& p) = 0;
	/** R*I*R^T*w, not mass*w. See the note above. */
	virtual NxVec3 getAngularMomentum() const = 0;
	virtual void   setAngularMomentum(const NxVec3& l) = 0;

	/* ---- forces -------------------------------------------------------- */
	virtual void addForce(const NxVec3& force, NxForceMode mode = NX_FORCE, bool wakeup = true) = 0;
	virtual void addForceAtPos(const NxVec3& force, const NxVec3& pos, NxForceMode mode = NX_FORCE, bool wakeup = true) = 0;
	/** At the centre of mass, in the actor's frame. No incidental torque. */
	virtual void addLocalForce(const NxVec3& force, NxForceMode mode = NX_FORCE, bool wakeup = true) = 0;
	virtual void addTorque(const NxVec3& torque, NxForceMode mode = NX_FORCE, bool wakeup = true) = 0;
	virtual void addLocalTorque(const NxVec3& torque, NxForceMode mode = NX_FORCE, bool wakeup = true) = 0;

	virtual void wakeUp(NxReal wakeCounterValue = NX_SLEEP_INTERVAL) = 0;
	virtual void putToSleep() = 0;
	virtual bool isSleeping() const = 0;

	/** The 1:1 link to the game's object. Read directly by Scene::GetActorFromNx. */
	void* userData;

	protected:
	NX_INLINE NxActor(): userData(NULL) {}
	virtual ~NxActor() {}
	};

#endif /* NX_ACTOR_H */
