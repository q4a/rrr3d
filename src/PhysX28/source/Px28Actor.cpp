/*
 * NxActor over btRigidBody.
 */

#include "Px28Impl.h"

namespace px28
{

/*
 * Static or dynamic is fixed at creation and never changes, discriminated by
 * NxActorDesc::body being NULL -- that pointer is the whole of 2.8's rule, and
 * px::Actor keys off its own _body for the same purpose.
 */
Actor::Actor(Scene& scene, const NxActorDesc& desc)
	: _scene(&scene), _body(NULL), _compound(NULL),
	  _dynamic(desc.body != NULL), _mass(0.0f),
	  _actorFlags(desc.flags), _contactReportFlags(desc.contactReportFlags)
	{
	_compound = new btCompoundShape();
	_comOffset.setIdentity();

	btVector3 inertia(0, 0, 0);
	if (_dynamic)
		{
		_mass = desc.body->mass;
		_comOffset = ToBullet(desc.body->massLocalPose);

		/*
		 * 2.8 lets massSpaceInertia be zero, meaning "compute it from the
		 * shapes". The compound is empty at this point -- shapes arrive through
		 * createShape afterwards -- so the tensor is left at zero here and
		 * settled when the shape set is known.
		 */
		const NxVec3& tensor = desc.body->massSpaceInertia;
		if (tensor.x != 0.0f || tensor.y != 0.0f || tensor.z != 0.0f)
			inertia = ToBullet(desc.body->massSpaceInertia);
		}

	/* The body starts at the centre of mass, not at the actor origin. */
	btDefaultMotionState* motion =
		new btDefaultMotionState(ToBullet(desc.globalPose) * _comOffset);

	btRigidBody::btRigidBodyConstructionInfo info(_dynamic ? _mass : 0.0f,
	                                              motion, _compound, inertia);
	if (_dynamic)
		{
		info.m_linearDamping = desc.body->linearDamping;
		info.m_angularDamping = desc.body->angularDamping;
		}

	_body = new btRigidBody(info);
	_body->setUserPointer(this);

	if (_dynamic)
		{
		_body->setLinearVelocity(ToBullet(desc.body->linearVelocity));
		_body->setAngularVelocity(ToBullet(desc.body->angularVelocity));

		/*
		 * Bullet deactivates a body that has been slow for a while and stops
		 * integrating it. 2.8 sleeps too, but on its own thresholds and its own
		 * flags (NX_BF_ENERGY_SLEEP_TEST, sleepLinearVelocity), so Bullet's
		 * defaults would put bodies to sleep at moments 2.8 would not.
		 * Disabled until sleeping is implemented against 2.8's rules; a body
		 * that never sleeps is slower, not wrong.
		 */
		_body->setActivationState(DISABLE_DEACTIVATION);
		}

	scene.world().addRigidBody(_body);

	/*
	 * The descriptor's shapes, in descriptor order.
	 *
	 * 2.8 creates an actor and its shapes in one call: NxActorDesc::shapes is a
	 * list of shape descriptors and createActor builds one shape from each.
	 * This is not an alternative to createShape -- the engine uses both. It
	 * fills actorDesc.shapes for the actor's initial shape set and calls
	 * createShape for anything added afterwards.
	 *
	 * Order is a contract, not an implementation detail.
	 * Actor::UnpackActorShapeListIncludeChildren (Physx.cpp:1776) walks
	 * getShapes() positionally against its own list to hand each px::Shape the
	 * NxShape it owns, so shape i here must be the shape built from descriptor
	 * i. Building them in any other order would wire every shape to the wrong
	 * engine object without failing anything.
	 *
	 * After addRigidBody, because createShape calls rebuildCompoundShape, which
	 * ends in updateSingleAabb -- and that needs the body to be in the world.
	 */
	for (NxU32 i = 0; i < desc.shapes.size(); ++i)
		createShape(*desc.shapes[i]);
	}

Actor::~Actor()
	{
	_scene->world().removeRigidBody(_body);

	delete _body->getMotionState();
	delete _body;

	for (size_t i = 0; i < _shapeStates.size(); ++i)
		delete _shapeStates[i];

	delete _compound;
	}

NxScene& Actor::getScene() const
	{
	return *_scene;
	}

/* -------------------------------------------------------------------- pose */

/*
 * Every pose accessor goes through the centre-of-mass offset, because Bullet's
 * body transform is the COM and 2.8's globalPose is the actor origin.
 */
void Actor::setActorTransform(const btTransform& actorWorld)
	{
	const btTransform bodyWorld = actorWorld * _comOffset;

	_body->setWorldTransform(bodyWorld);
	if (_body->getMotionState())
		_body->getMotionState()->setWorldTransform(bodyWorld);

	_body->setInterpolationWorldTransform(bodyWorld);
	_scene->world().updateSingleAabb(_body);
	}

btTransform Actor::actorTransform() const
	{
	return _body->getWorldTransform() * _comOffset.inverse();
	}

void Actor::setGlobalPose(const NxMat34& mat)
	{
	setActorTransform(ToBullet(mat));
	}

void Actor::setGlobalPosition(const NxVec3& vec)
	{
	btTransform actorWorld = actorTransform();
	actorWorld.setOrigin(ToBullet(vec));
	setActorTransform(actorWorld);
	}

void Actor::setGlobalOrientationQuat(const NxQuat& q)
	{
	btTransform actorWorld = actorTransform();
	actorWorld.setRotation(ToBullet(q));
	setActorTransform(actorWorld);
	}

void Actor::setGlobalOrientation(const NxMat33& mat)
	{
	NxMat34 pose;
	pose.M = mat;
	pose.t = ToNx(actorTransform().getOrigin());

	setActorTransform(ToBullet(pose));
	}

NxMat34 Actor::getGlobalPose() const
	{
	return ToNx(actorTransform());
	}

NxVec3 Actor::getGlobalPosition() const
	{
	return ToNx(actorTransform().getOrigin());
	}

NxQuat Actor::getGlobalOrientationQuat() const
	{
	return ToNx(actorTransform().getRotation());
	}

NxMat33 Actor::getGlobalOrientation() const
	{
	return ToNx(actorTransform()).M;
	}

/* ------------------------------------------------------- centre of mass --- */

void Actor::setCMassOffsetLocalPose(const NxMat34& mat)
	{
	/* The actor must not move: its world pose is held while the body's shifts
	   to the new centre of mass. */
	const btTransform actorWorld = actorTransform();

	_comOffset = ToBullet(mat);

	setActorTransform(actorWorld);
	rebuildCompoundShape();
	}

void Actor::setCMassOffsetLocalPosition(const NxVec3& vec)
	{
	NxMat34 pose = ToNx(_comOffset);
	pose.t = vec;

	setCMassOffsetLocalPose(pose);
	}

NxVec3 Actor::getCMassLocalPosition() const
	{
	return ToNx(_comOffset.getOrigin());
	}

/* The body transform is the centre of mass, so this is it directly. */
NxMat34 Actor::getCMassGlobalPose() const
	{
	return ToNx(_body->getWorldTransform());
	}

NxVec3 Actor::getMassSpaceInertiaTensor() const
	{
	return ToNx(_body->getLocalInertia());
	}

/* Half m v^2 for the linear part, half w.I.w for the angular one. GameCar
   compares two actors' energies to decide which wins a collision. */
NxReal Actor::computeKineticEnergy() const
	{
	const btVector3 v = _body->getLinearVelocity();
	const btVector3 w = _body->getAngularVelocity();

	const btMatrix3x3& basis = _body->getWorldTransform().getBasis();
	const btMatrix3x3 inertiaWorld =
		basis.scaled(_body->getLocalInertia()) * basis.transpose();

	return 0.5f * (_mass * v.dot(v) + w.dot(inertiaWorld * w));
	}

/* ------------------------------------------------------------------ shapes */

NxShape* Actor::createShape(const NxShapeDesc& desc)
	{
	/*
	 * isValid() is load-bearing here, not a sanity check.
	 * Actor::CreateNxShape uses !isValid() as its "mesh not loaded yet, defer"
	 * signal, so returning a shape for an invalid descriptor makes the engine
	 * create every mesh shape twice.
	 */
	if (!desc.isValid())
		return NULL;

	switch (desc.type)
		{
		case NX_SHAPE_BOX:
			{
			BoxShape* box = new BoxShape(*this, static_cast<const NxBoxShapeDesc&>(desc));
			_shapes.push_back(box);
			_shapeStates.push_back(box);
			break;
			}
		case NX_SHAPE_SPHERE:
			{
			SphereShape* sphere = new SphereShape(*this, static_cast<const NxSphereShapeDesc&>(desc));
			_shapes.push_back(sphere);
			_shapeStates.push_back(sphere);
			break;
			}
		case NX_SHAPE_CAPSULE:
			{
			CapsuleShape* capsule = new CapsuleShape(*this, static_cast<const NxCapsuleShapeDesc&>(desc));
			_shapes.push_back(capsule);
			_shapeStates.push_back(capsule);
			break;
			}
		case NX_SHAPE_MESH:
			{
			TriangleMeshShape* mesh = new TriangleMeshShape(
				*this, static_cast<const NxTriangleMeshShapeDesc&>(desc));
			_shapes.push_back(mesh);
			_shapeStates.push_back(mesh);
			break;
			}
		case NX_SHAPE_PLANE:
			{
			PlaneShape* plane = new PlaneShape(
				*this, static_cast<const NxPlaneShapeDesc&>(desc));
			_shapes.push_back(plane);
			_shapeStates.push_back(plane);
			break;
			}
		case NX_SHAPE_WHEEL:
			{
			WheelShape* wheel = new WheelShape(
				*this, static_cast<const NxWheelShapeDesc&>(desc));
			_shapes.push_back(wheel);
			_shapeStates.push_back(wheel);
			break;
			}
		default:
			/* NX_SHAPE_CONVEX has no descriptor in the game at all -- convex
			   meshes are cooked but never made into shapes. */
			Unimplemented("NxActor::createShape for this shape type");
		}

	rebuildCompoundShape();

	return _shapes.back();
	}

void Actor::releaseShape(NxShape& shape)
	{
	for (size_t i = 0; i < _shapes.size(); ++i)
		if (_shapes[i] == &shape)
			{
			delete _shapeStates[i];
			_shapes.erase(_shapes.begin() + i);
			_shapeStates.erase(_shapeStates.begin() + i);
			rebuildCompoundShape();
			return;
			}
	}

NxU32 Actor::getNbShapes() const
	{
	return static_cast<NxU32>(_shapes.size());
	}

/* Descriptor order, which the engine depends on:
   Actor::UnpackActorShapeListIncludeChildren walks this positionally. */
NxShape*const* Actor::getShapes() const
	{
	return _shapes.empty() ? NULL : &_shapes[0];
	}

/*
 * Bullet wants one collision shape per body, so the shape set is a compound and
 * it is rebuilt whenever that set changes.
 *
 * What is deliberately NOT done here is recompute the mass. 2.8 did not
 * recompute when shapes were added to or removed from a live actor, and
 * Actor::CreateNxShape and DestroyNxShape do exactly that. Recomputing would be
 * "more correct" and would silently change every car's handling.
 */
void Actor::rebuildCompoundShape()
	{
	while (_compound->getNumChildShapes() > 0)
		_compound->removeChildShapeByIndex(0);

	/* Children sit at comOffset^-1 * shapeLocalPose, because Bullet places them
	   relative to the body -- which is the centre of mass -- while 2.8's
	   localPose is relative to the actor origin. */
	const btTransform toBodySpace = _comOffset.inverse();

	for (size_t i = 0; i < _shapeStates.size(); ++i)
		_compound->addChildShape(toBodySpace * _shapeStates[i]->localPose(),
		                         _shapeStates[i]->bulletShape());

	/*
	 * The inertia tensor follows the shapes -- unlike the mass. 2.8 computed
	 * the tensor from the shape set at creation and the descriptor may leave it
	 * zero meaning "work it out", which cannot happen in the constructor
	 * because the compound is empty until the first shape arrives.
	 */
	if (_dynamic && _mass > 0.0f && _compound->getNumChildShapes() > 0)
		{
		btVector3 inertia(0, 0, 0);
		_compound->calculateLocalInertia(_mass, inertia);
		_body->setMassProps(_mass, inertia);
		_body->updateInertiaTensor();
		}

	_body->setCollisionShape(_compound);

	/*
	 * Skin width is per shape in 2.8 but per object in Bullet, so the largest
	 * of an actor's shapes wins. Every actor in this game has one skin width
	 * across its shapes -- px::Shape carries it and Actor::CreateNxShape
	 * applies the same descriptor value -- so the max is exact here and merely
	 * conservative if that ever stops being true.
	 */
	NxReal skin = 0.0f;
	for (size_t i = 0; i < _shapeStates.size(); ++i)
		{
		const NxReal shapeSkin = _shapeStates[i]->resolvedSkinWidth();
		if (shapeSkin > skin)
			skin = shapeSkin;
		}

	_body->setContactProcessingThreshold(skin);

	/*
	 * The broadphase AABB has to be recomputed, and this is not housekeeping.
	 *
	 * Bullet takes the AABB when the body is added to the world, and at that
	 * moment the compound is empty -- 2.8 creates an actor and then adds shapes
	 * to it, which is exactly what Actor::CreateNxShape does. Without this, a
	 * body carries a degenerate AABB until the first step that happens to
	 * refresh it, and a raycast before then finds nothing at all.
	 *
	 * That is not hypothetical: it is how the harness's raycast scenario
	 * failed, and the game raycasts against freshly built track geometry in
	 * Player::ResetCar.
	 */
	_scene->world().updateSingleAabb(_body);
	}

/* -------------------------------------------------------------------- body */

bool Actor::isDynamic() const
	{
	return _dynamic;
	}

void Actor::setMass(NxReal mass)
	{
	_mass = mass;
	_body->setMassProps(mass, _body->getLocalInertia());
	}

NxReal Actor::getMass() const
	{
	return _mass;
	}

void Actor::setLinearVelocity(const NxVec3& v)
	{
	_body->setLinearVelocity(ToBullet(v));
	}

void Actor::setAngularVelocity(const NxVec3& v)
	{
	_body->setAngularVelocity(ToBullet(v));
	}

NxVec3 Actor::getLinearVelocity() const
	{
	return ToNx(_body->getLinearVelocity());
	}

NxVec3 Actor::getAngularVelocity() const
	{
	return ToNx(_body->getAngularVelocity());
	}

/* ---------------------------------------------------------------- momentum */

NxVec3 Actor::getLinearMomentum() const
	{
	return ToNx(_body->getLinearVelocity() * _mass);
	}

void Actor::setLinearMomentum(const NxVec3& p)
	{
	if (_mass > 0.0f)
		_body->setLinearVelocity(ToBullet(p) / _mass);
	}

/*
 * L = R.I.R' . w -- the world-space inertia tensor times angular velocity, not
 * mass times angular velocity.
 *
 * The get-then-set identity is a hard requirement rather than a nicety.
 * GameCar::StabilizeForce runs every step and always ends with
 * setAngularMomentum, including on the path where its guarding `if` did not
 * fire and the value is an unmodified getAngularMomentum(). Any drift there is
 * permanent and compounds over a race.
 *
 * So the two directions are deliberately each other's exact inverse: this one
 * builds R.diag(I).R' and the setter uses Bullet's own inverse of that same
 * matrix, rather than inverting anything here.
 */
NxVec3 Actor::getAngularMomentum() const
	{
	const btMatrix3x3& basis = _body->getWorldTransform().getBasis();
	const btMatrix3x3 inertiaWorld =
		basis.scaled(_body->getLocalInertia()) * basis.transpose();

	return ToNx(inertiaWorld * _body->getAngularVelocity());
	}

void Actor::setAngularMomentum(const NxVec3& l)
	{
	_body->setAngularVelocity(_body->getInvInertiaTensorWorld() * ToBullet(l));
	}

/* ------------------------------------------------------------------ forces */

void Actor::addForce(const NxVec3& force, NxForceMode mode, bool wakeup)
	{
	if (wakeup)
		_body->activate();

	switch (mode)
		{
		case NX_FORCE:
		case NX_SMOOTH_IMPULSE:
			/* NX_SMOOTH_IMPULSE spread a change across 2.8's substeps, and 2.8
			   ran exactly one -- maxTimestep defaults to 1/60 and World.cpp
			   steps at 1/60 -- so it was already identical to NX_IMPULSE. See
			   Scene::fetchResults. */
			if (mode == NX_FORCE)
				{
				_body->applyCentralForce(ToBullet(force));
				break;
				}
			/* fall through: smooth impulse is an impulse */
		case NX_IMPULSE:
			_body->applyCentralImpulse(ToBullet(force));
			break;
		case NX_VELOCITY_CHANGE:
		case NX_SMOOTH_VELOCITY_CHANGE:
			_body->setLinearVelocity(_body->getLinearVelocity() + ToBullet(force));
			break;
		case NX_ACCELERATION:
			_body->applyCentralForce(ToBullet(force) * _mass);
			break;
		}
	}

void Actor::addForceAtPos(const NxVec3& force, const NxVec3& pos, NxForceMode mode,
                          bool wakeup)
	{
	if (wakeup)
		_body->activate();

	/* Bullet's applyForce takes the offset from the centre of mass; 2.8 takes a
	   world point. */
	const btVector3 relative = ToBullet(pos) - _body->getCenterOfMassPosition();

	switch (mode)
		{
		case NX_FORCE:
		case NX_ACCELERATION:
			_body->applyForce(ToBullet(force) * (mode == NX_ACCELERATION ? _mass : 1.0f),
			                  relative);
			break;
		default:
			_body->applyImpulse(ToBullet(force), relative);
			break;
		}
	}

/*
 * Body frame, at the centre of mass, producing no incidental torque.
 *
 * That last clause is the reason this is not addForceAtPos with the actor
 * origin: every car sets a centre-of-mass offset through bfLockCenterOfMass, so
 * applying at the origin would generate a torque 2.8 never produced.
 */
void Actor::addLocalForce(const NxVec3& force, NxForceMode mode, bool wakeup)
	{
	const btVector3 world =
		_body->getWorldTransform().getBasis() * ToBullet(force);

	addForce(ToNx(world), mode, wakeup);
	}

void Actor::addTorque(const NxVec3& torque, NxForceMode mode, bool wakeup)
	{
	if (wakeup)
		_body->activate();

	switch (mode)
		{
		case NX_FORCE:
			_body->applyTorque(ToBullet(torque));
			break;
		case NX_IMPULSE:
		case NX_SMOOTH_IMPULSE:
			_body->applyTorqueImpulse(ToBullet(torque));
			break;
		case NX_VELOCITY_CHANGE:
		case NX_SMOOTH_VELOCITY_CHANGE:
			_body->setAngularVelocity(_body->getAngularVelocity() + ToBullet(torque));
			break;
		case NX_ACCELERATION:
			_body->applyTorque(_body->getInvInertiaTensorWorld().inverse() *
			                   ToBullet(torque));
			break;
		}
	}

void Actor::addLocalTorque(const NxVec3& torque, NxForceMode mode, bool wakeup)
	{
	const btVector3 world =
		_body->getWorldTransform().getBasis() * ToBullet(torque);

	addTorque(ToNx(world), mode, wakeup);
	}

/* ------------------------------------------------------------------- flags */

/*
 * NX_AF_DISABLE_RESPONSE is the one flag with a Bullet counterpart, and every
 * weapon in the game depends on it: Weapon.cpp sets it on six projectile types
 * so a shot passes *through* what it hits while still reporting the contact
 * that scores the damage. CF_NO_CONTACT_RESPONSE is exactly that -- Bullet
 * still detects and reports the contact, it just applies no impulse.
 */
void Actor::applyResponseFlag()
	{
	const int flags = _body->getCollisionFlags();

	if (_actorFlags & NX_AF_DISABLE_RESPONSE)
		_body->setCollisionFlags(flags | btCollisionObject::CF_NO_CONTACT_RESPONSE);
	else
		_body->setCollisionFlags(flags & ~btCollisionObject::CF_NO_CONTACT_RESPONSE);
	}

void Actor::raiseActorFlag(NxActorFlag flag)
	{
	_actorFlags |= flag;
	applyResponseFlag();
	}

void Actor::clearActorFlag(NxActorFlag flag)
	{
	_actorFlags &= ~static_cast<NxU32>(flag);
	applyResponseFlag();
	}

bool Actor::readActorFlag(NxActorFlag flag) const
	{
	return (_actorFlags & flag) != 0;
	}

NxU32 Actor::getContactReportFlags() const
	{
	return _contactReportFlags;
	}

void Actor::setContactReportFlags(NxU32 flags)
	{
	_contactReportFlags = flags;
	}

/* ------------------------------------------------- not implemented yet ---- */

void Actor::setLinearDamping(NxReal damping)
	{
	_body->setDamping(damping, _body->getAngularDamping());
	}

/*
 * Sleeping. Bullet's own deactivation is disabled at construction -- it sleeps
 * on its own thresholds and 2.8 sleeps on NX_BF_ENERGY_SLEEP_TEST and
 * sleepLinearVelocity -- but the explicit calls still have to work, because
 * GameCar.cpp:1268 wakes an actor after teleporting it and a body that stayed
 * asleep would sit there.
 */
/* forceActivationState, not setActivationState: the latter refuses to change a
   body that is in DISABLE_DEACTIVATION, which every dynamic actor here is. */
void Actor::wakeUp(NxReal)
	{
	_body->forceActivationState(DISABLE_DEACTIVATION);
	_body->activate(true);
	}

void Actor::putToSleep()
	{
	_body->forceActivationState(ISLAND_SLEEPING);
	}

bool Actor::isSleeping() const
	{
	return !_body->isActive();
	}

/* Enough of the descriptor to reconstruct the actor, which is what
   px::Actor::SaveToDesc uses it for. */
void Actor::saveToDesc(NxActorDescBase& desc)
	{
	desc.globalPose = getGlobalPose();
	desc.flags = _actorFlags;
	desc.contactReportFlags = _contactReportFlags;
	desc.userData = userData;
	}

} /* namespace px28 */
