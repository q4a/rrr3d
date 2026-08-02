/*
 * NxWheelShape, implemented from the 2.8 specification.
 *
 * A 2.8 wheel is not a collision shape. It occupies no volume in the broad
 * phase; it is a raycast down the suspension axis plus a force model, run once
 * per step before the solver. That is why _bulletShape here is a btEmptyShape:
 * the compound has to contain something for the child indices to line up, and
 * it must contribute nothing.
 *
 * The geometry, from NxWheelShapeDesc.h:
 *
 *   -Y   suspension travel, and the direction the raycast goes
 *   +X   the axle; the wheel rolls about it
 *   +Y   what steerAngle rotates about
 *   +Z   forward
 *
 * WHAT IS SPECIFIED AND WHAT IS NOT. The curve, the two friction regimes, the
 * meaning of contactPosition and the summing of motor and brake torque are all
 * documented by the SDK, and are reproduced here. Two things are not, and are
 * marked where they occur: how inverseWheelMass becomes a rotational inertia,
 * and the exact implicit integration of the suspension. Both are chosen to
 * satisfy what the SDK *does* state -- that the wheel behaves as a disc and
 * that the spring is implicitly integrated and stable at any coefficient --
 * and neither reads a game constant to do it.
 *
 * NOTHING HERE IS TUNED. No value in db.xml, no wheel file under Data/Car and
 * no car parameter is read, scaled, clamped or invented. Where a formula
 * had to be chosen the choice is documented and derived from the API's own
 * description, never from how the game drives.
 */

#include "Px28Impl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace px28
{

namespace
{

/*
 * The rotational inertia of the wheel, from inverseWheelMass.
 *
 * NOT SPECIFIED by the SDK beyond "the wheel is treated as a disc". A disc
 * about its axle has I = m*r^2/2, so 1/I = 2*inverseWheelMass/r^2. That is
 * what this is, and it is the only reading consistent with the SDK's own
 * sentence -- a hoop or a point mass would be a different constant with no
 * justification behind it.
 *
 * The consequence is confined to how fast axleSpeed responds to torque. It
 * cannot change where a car sits, how much grip it has, or where it ends up:
 * those come from the suspension and the tire curves.
 */
NxReal inverseWheelInertia(NxReal inverseWheelMass, NxReal radius)
	{
	return 2.0f * inverseWheelMass / (radius * radius);
	}

/*
 * Slip, as the tire functions expect it.
 *
 * Longitudinal slip is the mismatch between the tread's speed and the ground's,
 * normalised by the larger of the two so it stays bounded at rest and at speed
 * alike. Lateral slip is the tangent of the slip angle -- sideways over
 * forwards -- which is what makes the lateral curve's slip axis the same
 * quantity as the longitudinal one.
 *
 * The epsilon keeps a stationary wheel from dividing by zero; it does not bias
 * the result, because at that point both numerator and denominator are within
 * it and the static-friction regime below has taken over anyway.
 */
const NxReal cSlipEpsilon = 1e-4f;

NxReal longitudinalSlip(NxReal treadSpeed, NxReal groundSpeed)
	{
	const NxReal denom = std::max(std::fabs(groundSpeed),
	                     std::max(std::fabs(treadSpeed), cSlipEpsilon));
	return (treadSpeed - groundSpeed) / denom;
	}

NxReal lateralSlip(NxReal lateralSpeed, NxReal forwardSpeed)
	{
	const NxReal denom = std::max(std::fabs(forwardSpeed), cSlipEpsilon);
	return lateralSpeed / denom;
	}

bool wheelTraceEnabled()
	{
	static const bool on = [] {
		const char* v = std::getenv("RRR3D_WHEEL_TRACE");
		return v && v[0] != '0';
	}();
	return on;
	}

}

/* ------------------------------------------------------------ construction */

WheelShape::WheelShape(Actor& actor, const NxWheelShapeDesc& desc)
	: ShapeImpl<NxWheelShape>(actor, desc, NX_SHAPE_WHEEL),
	  _radius(desc.radius), _suspensionTravel(desc.suspensionTravel),
	  _suspension(desc.suspension),
	  _longitudalTireForceFunction(desc.longitudalTireForceFunction),
	  _lateralTireForceFunction(desc.lateralTireForceFunction),
	  _inverseWheelMass(desc.inverseWheelMass), _wheelFlags(desc.wheelFlags),
	  _motorTorque(desc.motorTorque), _brakeTorque(desc.brakeTorque),
	  _steerAngle(desc.steerAngle), _axleSpeed(0.0f),
	  _contactModify(desc.wheelContactModify),
	  _contactShape(NULL), _hadContact(false)
	{
	/* No collision volume -- see the file header. */
	_bulletShape = new btEmptyShape();
	_ownsBulletShape = true;

	_lastContactPoint.setZero();
	_actor->scene().registerWheel(this);
	}

WheelShape::~WheelShape()
	{
	_actor->scene().unregisterWheel(this);
	}

/* ------------------------------------------------------------- accessors -- */

void WheelShape::setRadius(NxReal radius) { _radius = radius; }
NxReal WheelShape::getRadius() const { return _radius; }

void WheelShape::setSuspensionTravel(NxReal travel) { _suspensionTravel = travel; }
NxReal WheelShape::getSuspensionTravel() const { return _suspensionTravel; }

void WheelShape::setSuspension(const NxSpringDesc& spring) { _suspension = spring; }
const NxSpringDesc& WheelShape::getSuspension() const { return _suspension; }

void WheelShape::setLongitudalTireForceFunction(const NxTireFunctionDesc& fn)
	{
	_longitudalTireForceFunction = fn;
	}

const NxTireFunctionDesc& WheelShape::getLongitudalTireForceFunction() const
	{
	return _longitudalTireForceFunction;
	}

void WheelShape::setLateralTireForceFunction(const NxTireFunctionDesc& fn)
	{
	_lateralTireForceFunction = fn;
	}

const NxTireFunctionDesc& WheelShape::getLateralTireForceFunction() const
	{
	return _lateralTireForceFunction;
	}

void WheelShape::setInverseWheelMass(NxReal mass) { _inverseWheelMass = mass; }
NxReal WheelShape::getInverseWheelMass() const { return _inverseWheelMass; }

void WheelShape::setWheelFlags(NxU32 flags) { _wheelFlags = flags; }
NxU32 WheelShape::getWheelFlags() const { return _wheelFlags; }

/* Motor and brake are summed on one axle, as 2.8 sums them -- there is no lock
   and no separate channel. Stored separately only because the game sets them
   separately. */
void WheelShape::setMotorTorque(NxReal torque) { _motorTorque = torque; }
NxReal WheelShape::getMotorTorque() const { return _motorTorque; }
void WheelShape::setBrakeTorque(NxReal torque) { _brakeTorque = torque; }
NxReal WheelShape::getBrakeTorque() const { return _brakeTorque; }

void WheelShape::setSteerAngle(NxReal angle) { _steerAngle = angle; }
NxReal WheelShape::getSteerAngle() const { return _steerAngle; }

void WheelShape::setAxleSpeed(NxReal speed) { _axleSpeed = speed; }
NxReal WheelShape::getAxleSpeed() const { return _axleSpeed; }

void WheelShape::setUserWheelContactModify(NxUserWheelContactModify* callback)
	{
	_contactModify = callback;
	}

NxUserWheelContactModify* WheelShape::getUserWheelContactModify()
	{
	return _contactModify;
	}

/*
 * The shape the wheel is resting on, or NULL when it is airborne.
 *
 * A shape and not a bool: GameCar.cpp:191 assigns the result to an NxShape*
 * and :787 returns it as one. The contact data is filled in only when there is
 * a contact, which is also 2.8's rule.
 */
NxShape* WheelShape::getContact(NxWheelContactData& contact) const
	{
	if (!_hadContact)
		return NULL;

	contact = _contact;
	return _contactShape;
	}

void WheelShape::saveToDesc(NxWheelShapeDesc& desc) const
	{
	desc.localPose = getLocalPose();
	desc.group = _group;
	desc.materialIndex = _material;
	desc.skinWidth = _skinWidth;
	desc.shapeFlags = _flags;

	desc.radius = _radius;
	desc.suspensionTravel = _suspensionTravel;
	desc.suspension = _suspension;
	desc.longitudalTireForceFunction = _longitudalTireForceFunction;
	desc.lateralTireForceFunction = _lateralTireForceFunction;
	desc.inverseWheelMass = _inverseWheelMass;
	desc.wheelFlags = _wheelFlags;
	desc.motorTorque = _motorTorque;
	desc.brakeTorque = _brakeTorque;
	desc.steerAngle = _steerAngle;
	desc.wheelContactModify = _contactModify;
	}

/* ------------------------------------------------------------------ frames */

/*
 * The wheel's frame in world space, with steer applied.
 *
 * steerAngle turns the wheel about its own +Y, so it rotates forward and
 * lateral while leaving the suspension axis alone -- which is why the axis is
 * taken from the unsteered pose and the other two from the steered one.
 */
btTransform WheelShape::worldPose() const
	{
	return _actor->actorTransform() * _localPose;
	}

void WheelShape::frame(btVector3& origin, btVector3& suspensionDir,
                       btVector3& forward, btVector3& lateral) const
	{
	const btTransform pose = worldPose();
	const btMatrix3x3& basis = pose.getBasis();

	origin = pose.getOrigin();

	/* Down the suspension: the shape's own -Y. */
	suspensionDir = -basis.getColumn(1);

	const btQuaternion steer(basis.getColumn(1), _steerAngle);
	const btMatrix3x3 steered = btMatrix3x3(steer) * basis;

	forward = steered.getColumn(2);
	lateral = steered.getColumn(0);
	}

/* ------------------------------------------------------------ the step ---- */

/*
 * One wheel, one step. Called by Scene::fetchResults before the solver runs,
 * because a wheel's forces are part of the step rather than something applied
 * between steps.
 */
void WheelShape::step(NxReal dt)
	{
	if (dt <= 0.0f)
		return;

	btVector3 origin, down, forward, lateral;
	frame(origin, down, forward, lateral);

	btRigidBody* body = _actor->body();

	/* The raycast runs the full travel plus the radius: at full extension the
	   contact is exactly suspensionTravel + radius below the origin. */
	const NxReal reach = _suspensionTravel + _radius;
	const btVector3 to = origin + down * reach;

	NxRaycastHit hit;
	NxShape* other = _actor->scene().raycastForWheel(origin, to, _actor, hit);

	/*
	 * RRR3D_WHEEL_TRACE=1 reports the first few suspension rays and what they
	 * found. A wheel that never finds the ground is invisible from outside --
	 * the car simply rests on its hull and does not drive -- and the numbers
	 * that settle it are the ray's endpoints, which nothing else can show.
	 */
	if (wheelTraceEnabled())
		{
		/* Sampled, not the first N: a wheel's interesting state is the one it
		   settles into, and the first frames are all spawn. */
		static long calls = 0;
		if ((calls++ % 997) == 0)
			{
			std::fprintf(stderr,
				"wheel: origin (%.3f %.3f %.3f) down (%.3f %.3f %.3f) "
				"reach %.3f -> %s\n",
				double(origin.x()), double(origin.y()), double(origin.z()),
				double(down.x()), double(down.y()), double(down.z()),
				double(reach),
				other ? "HIT" : "nothing");
			}
		}

	if (!other)
		{
		airborne(dt);
		return;
		}

	/*
	 * contactPosition is a distance ALONG THE SUSPENSION TRAVEL, measured from
	 * the shape's origin -- "the distance on the spring travel distance where
	 * the wheel would end up if it was resting on the contact point".
	 *
	 * CarWheel::PxSyncWheel reads it as `st = contactPosition - radius` and
	 * draws the wheel st below the origin, which fixes the convention: this is
	 * the origin-to-contact distance, and the wheel centre sits one radius
	 * short of it. Projected rather than taken from the ray parameter, so it
	 * stays exact if the ray and the suspension axis ever stop coinciding.
	 */
	const btVector3 contactPoint = ToBullet(hit.worldImpact);
	btVector3 contactNormal = ToBullet(hit.worldNormal);
	NxReal contactPosition = (contactPoint - origin).dot(down);

	/* The suspension compression: zero at full extension, growing as the
	   contact comes up towards the origin. */
	NxReal compression = _suspensionTravel - (contactPosition - _radius);
	compression = std::max(0.0f, std::min(compression, _suspensionTravel));

	/*
	 * The suspension force, implicitly integrated.
	 *
	 * The SDK states the spring is implicit "so even high spring and damper
	 * coefficients should be robust", and this game needs that: across the
	 * shipped cars the damping ratio runs from 0.71 down to about 0.06, so an
	 * explicit integrator would let the undamped ones ring, swinging tire load
	 * and making traction arrive and leave several times a second. That reads
	 * as a grip problem and is not one.
	 *
	 * The exact form is NOT SPECIFIED. This is the standard implicit
	 * spring-damper for a mass under gravity,
	 *
	 *     f = (k*x + c*v) / (1 + dt*c*w + dt*dt*k*w)
	 *
	 * with w the effective inverse mass along the axis. It reduces to the
	 * explicit force as dt -> 0, is unconditionally stable for any k and c,
	 * and introduces no parameter of its own.
	 */
	const btVector3 up = -down;
	const btVector3 contactVelocity = body->getVelocityInLocalPoint(contactPoint - body->getCenterOfMassPosition());
	const NxReal compressionRate = -contactVelocity.dot(up);

	const NxReal w = effectiveInverseMass(body, contactPoint, up);
	const NxReal k = _suspension.spring;
	const NxReal c = _suspension.damper;

	NxReal normalForce =
		(k * (compression - _suspension.targetValue) + c * compressionRate)
		/ (1.0f + dt * c * w + dt * dt * k * w);

	/* A suspension pulls nothing down; it only pushes the body up. */
	normalForce = std::max(0.0f, normalForce);

	/*
	 * The game's own hook, and 2.8 gives it the final say: it may move the
	 * contact, change its normal, rewrite the normal force, or reject the
	 * contact outright. GameCar uses it for a tire overload limiter and a
	 * clutch lock, so skipping it would change how the car drives.
	 */
	NxMaterialIndex material = other->getMaterial();

	if (_contactModify)
		{
		NxVec3 point = ToNx(contactPoint);
		NxVec3 normal = ToNx(contactNormal);

		if (!_contactModify->onWheelContact(this, point, normal, contactPosition,
				normalForce, other, material, hit.faceID))
			{
			airborne(dt);
			return;
			}

		contactNormal = ToBullet(normal);
		}

	body->applyForce(up * normalForce, contactPoint - body->getCenterOfMassPosition());

	/* --------------------------------------------------------- tire forces */

	const NxReal forwardSpeed = contactVelocity.dot(forward);
	const NxReal lateralSpeed = contactVelocity.dot(lateral);
	const NxReal treadSpeed = _axleSpeed * _radius;

	const NxReal lngSlip = longitudinalSlip(treadSpeed, forwardSpeed);
	const NxReal latSlip = lateralSlip(lateralSpeed, forwardSpeed);

	/*
	 * Under NX_WF_CLAMPED_FRICTION -- which DataBase.cpp:419 sets on every car
	 * in this game -- the curve's output is a FRICTION COEFFICIENT and the
	 * maximum friction impulse is that coefficient times the normal impulse.
	 * It is a ceiling on what the contact may spend, not the force applied.
	 * Reading it as a force is the difference between a car that drives and one
	 * that stands on its back wheels.
	 */
	const bool clamped = (_wheelFlags & NX_WF_CLAMPED_FRICTION) != 0;

	/*
	 * Below a speed threshold -- "the contact point with the ground is within
	 * skinWidth of its previous position" -- the contact becomes a static
	 * friction contact with mu = extremumValue. Both regimes are specified;
	 * this is the second one.
	 */
	const bool stationary = _hadContact &&
		(contactPoint - _lastContactPoint).length() < resolvedSkinWidth();

	NxReal lngMu = stationary
		? _longitudalTireForceFunction.extremumValue
		: std::fabs(_longitudalTireForceFunction.hermiteEval(lngSlip));
	NxReal latMu = stationary
		? _lateralTireForceFunction.extremumValue
		: std::fabs(_lateralTireForceFunction.hermiteEval(latSlip));

	const NxReal normalImpulse = normalForce * dt;

	/*
	 * The impulse the tire would need to erase the slip completely, and the
	 * ceiling the friction coefficient puts on it. The smaller one is spent --
	 * which is what makes a tire grip below the limit and slide above it,
	 * rather than always applying the curve's value.
	 */
	const NxReal lngW = effectiveInverseMass(body, contactPoint, forward);
	const NxReal latW = effectiveInverseMass(body, contactPoint, lateral);

	NxReal lngImpulse = (lngW > 0.0f) ? (treadSpeed - forwardSpeed) / lngW : 0.0f;
	NxReal latImpulse = (latW > 0.0f) ? -lateralSpeed / latW : 0.0f;

	if (clamped)
		{
		const NxReal lngMax = lngMu * normalImpulse;
		const NxReal latMax = latMu * normalImpulse;

		lngImpulse = std::max(-lngMax, std::min(lngImpulse, lngMax));
		latImpulse = std::max(-latMax, std::min(latImpulse, latMax));
		}
	else
		{
		/*
		 * The default model, in which the curve's output is a force directly.
		 * Implemented rather than asserted on because wheelFlags is serialised
		 * -- a saved game can carry a value this build never wrote.
		 */
		lngImpulse = _longitudalTireForceFunction.hermiteEval(lngSlip) * dt;
		latImpulse = -_lateralTireForceFunction.hermiteEval(latSlip) * dt;
		}

	const btVector3 lever = contactPoint - body->getCenterOfMassPosition();
	body->applyImpulse(forward * lngImpulse, lever);
	body->applyImpulse(lateral * latImpulse, lever);

	/*
	 * The wheel's own spin. The road's reaction to the longitudinal impulse
	 * acts on the tread at one radius, and motor and brake torque are summed --
	 * 2.8 sums them, so a wheel under both gets the difference and not a lock.
	 */
	const NxReal invInertia = inverseWheelInertia(_inverseWheelMass, _radius);
	const NxReal driveTorque = _motorTorque - brakeTorqueSign() * _brakeTorque;

	_axleSpeed += driveTorque * invInertia * dt;
	_axleSpeed -= lngImpulse * _radius * invInertia;

	/* ------------------------------------------------------- publish state */

	_contact.contactPoint = ToNx(contactPoint);
	_contact.contactNormal = ToNx(contactNormal);
	_contact.longitudalDirection = ToNx(forward);
	_contact.lateralDirection = ToNx(lateral);
	_contact.contactForce = normalForce;
	_contact.longitudalSlip = lngSlip;
	_contact.lateralSlip = latSlip;
	_contact.longitudalImpulse = lngImpulse;
	_contact.lateralImpulse = latImpulse;
	_contact.otherShapeMaterialIndex = material;
	_contact.otherShape = other;
	_contact.contactPosition = contactPosition;

	_contactShape = other;
	_lastContactPoint = contactPoint;
	_hadContact = true;
	}

/*
 * Airborne: no suspension force, no tire force, and the wheel still spins.
 *
 * A driven wheel off the ground accelerates freely, which is what makes the
 * engine over-rev and what the game's traction logic reads back through
 * getAxleSpeed. Zeroing it instead would be a quieter answer and a wrong one.
 */
void WheelShape::airborne(NxReal dt)
	{
	const NxReal invInertia = inverseWheelInertia(_inverseWheelMass, _radius);
	const NxReal driveTorque = _motorTorque - brakeTorqueSign() * _brakeTorque;

	_axleSpeed += driveTorque * invInertia * dt;

	_contactShape = NULL;
	_hadContact = false;
	}

/*
 * Brake torque opposes rotation, so it takes the sign of the axle speed --
 * and must not reverse it. A brake that pushes a stopped wheel backwards is
 * the classic way for a parked car to creep.
 */
NxReal WheelShape::brakeTorqueSign() const
	{
	if (_axleSpeed > 0.0f)
		return 1.0f;
	if (_axleSpeed < 0.0f)
		return -1.0f;
	return 0.0f;
	}

/*
 * The body's inverse mass as felt at a point along a direction: the scalar that
 * turns an impulse into the velocity change it produces there. Linear inverse
 * mass plus the angular term from the lever arm.
 */
NxReal WheelShape::effectiveInverseMass(const btRigidBody* body,
                                        const btVector3& point,
                                        const btVector3& dir) const
	{
	const btVector3 lever = point - body->getCenterOfMassPosition();
	const btVector3 cross = lever.cross(dir);
	const btVector3 angular =
		body->getInvInertiaTensorWorld() * cross;

	return body->getInvMass() + angular.cross(lever).dot(dir);
	}

}
