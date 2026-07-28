#include "stdafx.h"

#include "px/Vehicle.h"

#include "rrr3d_trace.h"

#include <algorithm>
#include <cmath>

namespace r3d
{

namespace px
{

namespace
{

/*
 * Shapes belonging to a vehicle are not drivable surfaces.
 *
 * A suspension raycast starts inside the car and points down, so without this
 * every wheel would immediately hit its own chassis. PhysX has no way to say
 * "ignore this actor" from inside a batch query pre-filter -- the shader sees
 * filter data and nothing else -- so the vehicle's own shapes are marked in
 * query filter data word3 and the pre-filter drops them.
 */
const PxU32 cUndrivableSurface = 0xffff0000;
const PxU32 cDrivableSurface   = 0x0000ffff;

/*
 * How hard a tire resists sliding, per unit of slip, per unit of load.
 *
 * The one number here with no counterpart in the 2.8 data, and it exists
 * because of a modelling mismatch rather than a missing parameter: 2.8 solved
 * the wheel contact as a static friction constraint, so there was no stiffness
 * to specify -- the force was simply whatever the constraint needed. PxVehicle
 * computes tire force from slip, so a constraint has to be approximated by a
 * response stiff enough that slip stays negligible under ordinary demand.
 *
 * Higher is closer to a rigid contact. It is also less stable at this step
 * rate, and the harness is unambiguous about where the knee is -- sweeping it
 * against the four scenarios:
 *
 *   K = 2    wheels roll, slip 0.03, car tracks straight    1 failure
 *   K = 5    wheels start sliding                           2 failures
 *   K = 15                                                  3 failures
 *   K = 40   wheels spinning at ten times road speed        5 failures
 *
 * RRR3D_TIRE_STIFFNESS overrides it, because this is the number to reach for
 * when the cars feel wrong to drive rather than merely wrong on paper.
 */
const float cContactStiffness = 2.0f;

PxQueryHitType::Enum SuspensionQueryPreFilter(
	PxFilterData filterData0, PxFilterData filterData1,
	const void* constantBlock, PxU32 constantBlockSize,
	PxHitFlags& queryFlags)
{
	return (filterData1.word3 == cUndrivableSurface)
		? PxQueryHitType::eNONE
		: PxQueryHitType::eBLOCK;
}

//A cubic Hermite with a zero tangent at both ends, which is smoothstep.
float SmoothStepBetween(float x0, float y0, float x1, float y1, float x)
{
	if (x1 <= x0)
		return y1;

	const float t = std::min(std::max((x - x0) / (x1 - x0), 0.0f), 1.0f);
	return y0 + (y1 - y0) * t * t * (3.0f - 2.0f * t);
}

/*
 * How much grip a tire has at a given slip: NxTireFunctionDesc read as a
 * friction ceiling rather than as a force.
 *
 * Every car in this game sets wheelFlags = 64, NX_WF_CLAMPED_FRICTION, and the
 * 2.8 header says what that means:
 *
 *   mu = NxTireFunctionDesc::extremumValue   (for the clamped friction model)
 *   (friction force) = mu * (normal force)
 *   "the wheel contacts are modeled as static friction contacts"
 *
 * So the curve never gave the applied force. It gives what the contact may
 * spend, and the solver spent whatever was needed below it. That also explains
 * something in Player::ApplyMobility that reads like a bug and is not: it zeroes
 * stiffnessFactor and never adds anything back, because the clamped model does
 * not read it.
 *
 * The header names only extremumValue, but taking that literally throws away
 * asymptoteSlip and asymptoteValue, and those two are what a tire *losing* grip
 * is described by -- the shipped longitudinal curve falls from 7 to 6.4, the
 * lateral one from 6.5 all the way to 3. A tire that keeps full grip however
 * hard it is sliding cannot break away and cannot come back, and the slide is a
 * large part of how this game drives. So grip holds at the extremum until the
 * tire starts to slide and then falls along the curve the data already
 * describes:
 *
 *   |slip| <= extremumSlip     full grip, extremumValue
 *   up to asymptoteSlip        falling, extremumValue -> asymptoteValue
 *   beyond                     asymptoteValue
 */
float EvalTireGripCeiling(const TireFunctionDesc& function, float slip)
{
	const float s = std::fabs(slip);

	if (s <= function.extremumSlip)
		return function.extremumValue;

	if (s < function.asymptoteSlip)
		return SmoothStepBetween(function.extremumSlip, function.extremumValue,
			function.asymptoteSlip, function.asymptoteValue, s);

	return function.asymptoteValue;
}

/*
 * The tire force shader PxVehicleUpdates calls, per wheel, per substep.
 *
 * Signature is fixed by PxVehicleComputeTireForce. shaderData is whatever was
 * handed to setTireForceShaderData for this wheel -- here, the wheel's own pair
 * of 2.8 curves.
 */
void Compute2_8TireForce(
	const void* shaderData, const PxF32 tireFriction,
	const PxF32 longSlip, const PxF32 latSlip, const PxF32 camber,
	const PxF32 wheelOmega, const PxF32 wheelRadius, const PxF32 recipWheelRadius,
	const PxF32 restTireLoad, const PxF32 normalisedTireLoad, const PxF32 tireLoad,
	const PxF32 gravity, const PxF32 recipGravity,
	PxF32& wheelTorque, PxF32& tireLongForceMag, PxF32& tireLatForceMag,
	PxF32& tireAlignMoment)
{
	const TireShaderData& tire = *static_cast<const TireShaderData*>(shaderData);

	//A wheel off the ground carries no load and therefore no force. PxVehicle
	//still calls the shader for it.
	if (tireLoad <= 0.0f)
	{
		wheelTorque = 0.0f;
		tireLongForceMag = 0.0f;
		tireLatForceMag = 0.0f;
		tireAlignMoment = 0.0f;
		return;
	}

	/*
	 * A static friction contact, with the 2.8 curve supplying only the ceiling.
	 *
	 * Under NX_WF_CLAMPED_FRICTION -- which every car in this game selects --
	 * mu is extremumValue and the contact supplies whatever force is needed to
	 * stop the tire sliding, up to mu * load. It is not a force that grows with
	 * slip. Making it one is what put 214 kN through a single tire and stood the
	 * car on its back wheels.
	 *
	 * So: a very stiff response in slip, saturating at the ceiling. The
	 * stiffness is high enough that under any ordinary demand the slip stays
	 * near zero and the force settles at exactly what is being asked of it,
	 * which is what a static contact does. Only when the demand exceeds mu *
	 * load does the tire break away, which is the one behaviour the ceiling is
	 * there to produce.
	 */
	float stiffness = cContactStiffness;
	if (const char* override = std::getenv("RRR3D_TIRE_STIFFNESS"))
		stiffness = static_cast<float>(std::atof(override));

	const float muLong = EvalTireGripCeiling(tire.longitudal, longSlip) * tireFriction;
	const float muLat  = EvalTireGripCeiling(tire.lateral, latSlip) * tireFriction;

	float longForce = stiffness * longSlip * tireLoad;
	float latForce  = -stiffness * latSlip * tireLoad;

	const float maxLong = muLong * tireLoad;
	const float maxLat  = muLat * tireLoad;

	longForce = std::min(std::max(longForce, -maxLong), maxLong);
	latForce  = std::min(std::max(latForce, -maxLat), maxLat);

	/*
	 * The friction circle. A tire cannot spend more grip than it has, and
	 * without this a wheel that is both spinning and sliding produces the full
	 * ceiling in each direction at once -- which is where the diagonal launches
	 * came from.
	 */
	const float limit = std::max(maxLong, maxLat);
	const float demanded = std::sqrt(longForce * longForce + latForce * latForce);
	if (demanded > limit && demanded > 0.0f)
	{
		const float scale = limit / demanded;
		longForce *= scale;
		latForce *= scale;
	}

	tireLongForceMag = longForce;
	tireLatForceMag = latForce;

	//Reaction on the wheel itself, which is what slows a spinning wheel down.
	//
	//This sign is load-bearing and it has been checked: inverting it turns the
	//drive torque and the tire reaction into positive feedback, and the harness
	//answers with a car doing 50 m/s straight up with 11350 rad/s wheels.
	wheelTorque = -tireLongForceMag * wheelRadius;

	//2.8 had no self-aligning torque; adding one would change handling.
	tireAlignMoment = 0.0f;

	//What the tire model is actually doing, from inside it. Everything else is
	//inferred from a car's trajectory two integrations later.
	//
	//Sampled rather than taken from the front: the first calls of any run are
	//the drop onto the ground, where the load is pinned at its clamp and every
	//slip is zero, which says nothing about driving.
	if (::rrr3d::TraceEnabled())
	{
		static unsigned long call = 0;
		if ((++call % 331) == 0)
			RRR3D_TRACE_FIRST(60,
				"TIRE load=%.1f rest=%.1f norm=%.3f friction=%.2f longSlip=%.4f latSlip=%.4f "
				"longF=%.1f latF=%.1f omega=%.2f torque=%.1f",
				tireLoad, restTireLoad, normalisedTireLoad, tireFriction, longSlip, latSlip,
				tireLongForceMag, tireLatForceMag, wheelOmega, wheelTorque);
	}
}

PxVec3 ToPxVec(const D3DXVECTOR3& value)
{
	return PxVec3(value.x, value.y, value.z);
}

} // namespace

void CollectWheelShapes(Actor* actor, std::vector<WheelShape*>& out)
{
	if (!actor)
		return;

	Shapes& shapes = actor->GetShapes();
	for (Shapes::iterator iter = shapes.begin(); iter != shapes.end(); ++iter)
	{
		Shape* shape = *iter;
		if (shape && shape->GetType() == stWheel)
			out.push_back(static_cast<WheelShape*>(shape));
	}

	const Actor::Children& children = actor->GetChildren();
	for (Actor::Children::const_iterator iter = children.begin(); iter != children.end(); ++iter)
		CollectWheelShapes(*iter, out);
}

/* ---------------------------------------------------------------- scene --- */

bool g_vehicleSdkInitialised = false;

void VehicleScene::InitSDK(PxPhysics& sdk)
{
	if (g_vehicleSdkInitialised)
		return;

	if (!PxInitVehicleSDK(sdk))
	{
		LSL_LOG("PxInitVehicleSDK failed");
		return;
	}

	//This engine is Z-up and drives along +X: the scene gravity is (0,0,-20)
	//and every camera default in ContextInfo uses dir = XVector, up = ZVector.
	PxVehicleSetBasisVectors(PxVec3(0.0f, 0.0f, 1.0f), PxVec3(1.0f, 0.0f, 0.0f));

	//eVELOCITY_CHANGE applies the vehicle's result as an immediate velocity
	//change rather than an acceleration, which is what keeps a car stable at
	//the 1/60 step this game runs at.
	PxVehicleSetUpdateMode(PxVehicleUpdateMode::eVELOCITY_CHANGE);

	g_vehicleSdkInitialised = true;
}

void VehicleScene::ReleaseSDK()
{
	if (!g_vehicleSdkInitialised)
		return;

	PxCloseVehicleSDK();
	g_vehicleSdkInitialised = false;
}

VehicleScene::VehicleScene(PxScene* nxScene)
	: _nxScene(nxScene), _batchQuery(0), _frictionPairs(0), _queryCapacity(0)
{
	/*
	 * One surface type and one tire type, with friction 1.
	 *
	 * PxVehicle multiplies the shader's result by this, and the 2.8 curves
	 * already carry the whole force magnitude, so anything other than 1 here
	 * would silently rescale every car's tuning. Per-material friction, which
	 * 2.8 expressed through the material index the wheel reported, belongs in
	 * the surface table when it is wired up -- see the note in SyncOutputs.
	 */
	_frictionPairs = PxVehicleDrivableSurfaceToTireFrictionPairs::allocate(1, 1);

	const PxMaterial* material = &Manager::GetDefaultMaterial();
	PxVehicleDrivableSurfaceType surfaceType;
	surfaceType.mType = 0;

	_frictionPairs->setup(1, 1, &material, &surfaceType);
	_frictionPairs->setTypePairFriction(0, 0, 1.0f);
}

VehicleScene::~VehicleScene()
{
	for (size_t i = 0; i < _vehicles.size(); ++i)
		delete _vehicles[i];
	_vehicles.clear();

	if (_batchQuery)
	{
		_batchQuery->release();
		_batchQuery = 0;
	}

	if (_frictionPairs)
	{
		_frictionPairs->release();
		_frictionPairs = 0;
	}
}

void VehicleScene::EnsureQueryCapacity(unsigned wheels)
{
	if (wheels <= _queryCapacity && _batchQuery)
		return;

	if (_batchQuery)
	{
		_batchQuery->release();
		_batchQuery = 0;
	}

	_queryCapacity = wheels;
	_raycastResults.resize(_queryCapacity);
	_raycastHits.resize(_queryCapacity);

	PxBatchQueryDesc desc(_queryCapacity, 0, 0);
	desc.queryMemory.userRaycastResultBuffer = &_raycastResults[0];
	desc.queryMemory.userRaycastTouchBuffer = &_raycastHits[0];
	desc.queryMemory.raycastTouchBufferSize = _queryCapacity;
	desc.preFilterShader = SuspensionQueryPreFilter;

	_batchQuery = _nxScene->createBatchQuery(desc);
}

Vehicle* VehicleScene::Add(Actor* actor)
{
	if (Vehicle* existing = Find(actor))
		return existing;

	Vehicle* vehicle = new Vehicle(actor);
	if (!vehicle->IsValid())
	{
		delete vehicle;
		return 0;
	}

	_vehicles.push_back(vehicle);
	return vehicle;
}

void VehicleScene::Remove(Actor* actor)
{
	for (size_t i = 0; i < _vehicles.size(); ++i)
	{
		if (_vehicles[i]->GetActor() == actor)
		{
			delete _vehicles[i];
			_vehicles.erase(_vehicles.begin() + i);
			return;
		}
	}
}

Vehicle* VehicleScene::Find(Actor* actor)
{
	for (size_t i = 0; i < _vehicles.size(); ++i)
		if (_vehicles[i]->GetActor() == actor)
			return _vehicles[i];

	return 0;
}

bool VehicleScene::IsEmpty() const
{
	return _vehicles.empty();
}

void VehicleScene::Update(float deltaTime, const PxVec3& gravity)
{
	if (_vehicles.empty() || deltaTime <= 0.0f)
		return;

	unsigned totalWheels = 0;
	for (size_t i = 0; i < _vehicles.size(); ++i)
		totalWheels += _vehicles[i]->GetWheelCount();

	if (totalWheels == 0)
		return;

	EnsureQueryCapacity(totalWheels);
	if (!_batchQuery)
		return;

	std::vector<PxVehicleWheels*> vehicles(_vehicles.size());
	std::vector<PxVehicleWheelQueryResult> queryResults(_vehicles.size());

	for (size_t i = 0; i < _vehicles.size(); ++i)
	{
		_vehicles[i]->SyncInputs();
		vehicles[i] = _vehicles[i]->GetNxVehicle();
		queryResults[i] = _vehicles[i]->GetQueryResultBuffer();
	}

	PxVehicleSuspensionRaycasts(_batchQuery, static_cast<PxU32>(vehicles.size()),
		&vehicles[0], totalWheels, &_raycastResults[0]);

	PxVehicleUpdates(deltaTime, gravity, *_frictionPairs,
		static_cast<PxU32>(vehicles.size()), &vehicles[0], &queryResults[0]);

	for (size_t i = 0; i < _vehicles.size(); ++i)
		_vehicles[i]->SyncOutputs();
}

/* -------------------------------------------------------------- vehicle --- */

Vehicle::Vehicle(Actor* actor): _actor(actor), _nxVehicle(0)
{
	PxRigidDynamic* body = actor ? actor->GetNxDynamic() : 0;
	if (!body)
		return;

	//Root actor and every child, in the order they were added, which is the
	//order the game's CarWheel objects hold them in.
	CollectWheelShapes(actor, _wheels);

	//PxVehicle wants at least four, and every car in this game has four.
	if (_wheels.size() < 4)
	{
		_wheels.clear();
		return;
	}

	PxVehicleWheelsSimData* simData =
		PxVehicleWheelsSimData::allocate(static_cast<PxU32>(_wheels.size()));

	if (!BuildWheelsSimData(*simData, *body))
	{
		simData->free();
		_wheels.clear();
		return;
	}

	_nxVehicle = PxVehicleNoDrive::allocate(static_cast<PxU32>(_wheels.size()));
	_nxVehicle->setup(&GetSDK(), body, *simData);

	simData->free();

	_wheelQueryResults.resize(_wheels.size());
}

Vehicle::~Vehicle()
{
	if (_nxVehicle)
	{
		_nxVehicle->free();
		_nxVehicle = 0;
	}
}

bool Vehicle::BuildWheelsSimData(PxVehicleWheelsSimData& simData, PxRigidDynamic& body)
{
	const PxU32 wheelCount = static_cast<PxU32>(_wheels.size());

	//Offsets are measured from the centre of mass, which is what both
	//PxVehicleComputeSprungMasses and setWheelCentreOffset expect.
	const PxVec3 centreOfMass = body.getCMassLocalPose().p;

	std::vector<PxVec3> offsets(wheelCount);
	for (PxU32 i = 0; i < wheelCount; ++i)
		offsets[i] = ToPxVec(_wheels[i]->GetPos()) - centreOfMass;

	std::vector<PxF32> sprungMasses(wheelCount);
	PxVehicleComputeSprungMasses(wheelCount, &offsets[0], PxVec3(0.0f, 0.0f, 0.0f),
		body.getMass(), 2 /* the up axis, Z */, &sprungMasses[0]);

	_tireData.resize(wheelCount);

	//Index each of the actor's shapes so a wheel can be mapped to its own.
	std::vector<PxShape*> actorShapes(body.getNbShapes());
	if (!actorShapes.empty())
		body.getShapes(&actorShapes[0], static_cast<PxU32>(actorShapes.size()));

	for (PxU32 i = 0; i < wheelCount; ++i)
	{
		WheelShape* wheel = _wheels[i];

		const float radius = wheel->GetRadius() > 0.0f ? wheel->GetRadius() : 0.4f;
		const float inverseMass = wheel->GetInverseWheelMass();
		const float mass = inverseMass > 0.0f ? 1.0f / inverseMass : 20.0f;

		PxVehicleWheelData wheelData;
		wheelData.mRadius = radius;
		wheelData.mWidth = radius * 0.5f;
		wheelData.mMass = mass;
		//A disc about its axle.
		wheelData.mMOI = 0.5f * mass * radius * radius;
		//The game sets the actual torque every step; these only cap it.
		wheelData.mMaxBrakeTorque = 1.0e7f;
		wheelData.mMaxSteer = PxPi;
		wheelData.mDampingRate = 0.25f;
		simData.setWheelData(i, wheelData);

		/*
		 * Suspension.
		 *
		 * spring and damper carry over unchanged -- both are already in
		 * PxVehicle's units. The travel split is a judgement call: 2.8 gave a
		 * single suspensionTravel and a targetValue in [0,1] saying where along
		 * it the spring rests, and PxVehicle wants compression and droop either
		 * side of rest. targetValue is 0 in every shipped car, meaning rest at
		 * full extension, which would leave zero droop and a wheel that cannot
		 * follow a dip -- so a tenth of the travel is kept as droop.
		 */
		const SpringDesc& spring = wheel->GetSuspension();
		const float travel = wheel->GetSuspensionTravel() > 0.0f
			? wheel->GetSuspensionTravel() : 0.1f;

		float droop = travel * spring.targetValue;
		if (droop < travel * 0.1f)
			droop = travel * 0.1f;

		PxVehicleSuspensionData suspension;
		suspension.mSpringStrength = spring.spring;
		suspension.mSpringDamperRate = spring.damper;
		suspension.mMaxCompression = travel - droop;
		suspension.mMaxDroop = droop;
		suspension.mSprungMass = sprungMasses[i];
		simData.setSuspensionData(i, suspension);

		//PxVehicleTireData is deliberately left at its defaults: the shader
		//below ignores it entirely. Leaving it unset would trip PxVehicle's own
		//validation, so it is set and then not used.
		simData.setTireData(i, PxVehicleTireData());

		//Straight down in the chassis frame, this engine being Z-up.
		simData.setSuspTravelDirection(i, PxVec3(0.0f, 0.0f, -1.0f));

		simData.setWheelCentreOffset(i, offsets[i]);
		//Applying suspension and tire force at the wheel centre gives a car
		//that rolls over on its own weight; both are applied below the centre
		//of mass, which is the standard remedy and what the SDK's own samples
		//do.
		const PxVec3 forceAppPoint(offsets[i].x, offsets[i].y, -0.3f);
		simData.setSuspForceAppPointOffset(i, forceAppPoint);
		simData.setTireForceAppPointOffset(i, forceAppPoint);

		//The wheel's own shape, so PxVehicle can pose it, and so the suspension
		//raycast can be told to ignore it.
		PxI32 shapeIndex = -1;
		for (size_t s = 0; s < actorShapes.size(); ++s)
			if (actorShapes[s] == wheel->GetNxShape())
				shapeIndex = static_cast<PxI32>(s);
		simData.setWheelShapeMapping(i, shapeIndex);

		simData.setSceneQueryFilterData(i, PxFilterData(0, 0, 0, cDrivableSurface));

		_tireData[i].longitudal = wheel->GetLongitudalTireForceFunction();
		_tireData[i].lateral = wheel->GetLateralTireForceFunction();
	}

	//Every shape on this actor is off-limits to the suspension raycasts.
	for (size_t s = 0; s < actorShapes.size(); ++s)
		actorShapes[s]->setQueryFilterData(PxFilterData(0, 0, 0, cUndrivableSurface));

	return true;
}

bool Vehicle::IsValid() const
{
	return _nxVehicle != 0;
}

Actor* Vehicle::GetActor()
{
	return _actor;
}

PxVehicleWheels* Vehicle::GetNxVehicle()
{
	return _nxVehicle;
}

unsigned Vehicle::GetWheelCount() const
{
	return static_cast<unsigned>(_wheels.size());
}

PxVehicleWheelQueryResult Vehicle::GetQueryResultBuffer()
{
	PxVehicleWheelQueryResult result;
	result.wheelQueryResults = _wheelQueryResults.empty() ? 0 : &_wheelQueryResults[0];
	result.nbWheelQueryResults = static_cast<PxU32>(_wheelQueryResults.size());
	return result;
}

void Vehicle::SyncInputs()
{
	if (!_nxVehicle)
		return;

	//The tire shader is set every step rather than once, because the game is
	//free to retune a wheel at any point -- the garage does exactly that -- and
	//the shader data is held by pointer.
	_nxVehicle->mWheelsDynData.setTireForceShaderFunction(Compute2_8TireForce);

	for (size_t i = 0; i < _wheels.size(); ++i)
	{
		WheelShape* wheel = _wheels[i];

		_tireData[i].longitudal = wheel->GetLongitudalTireForceFunction();
		_tireData[i].lateral = wheel->GetLateralTireForceFunction();
		_nxVehicle->mWheelsDynData.setTireForceShaderData(static_cast<PxU32>(i), &_tireData[i]);

		_nxVehicle->setDriveTorque(static_cast<PxU32>(i), wheel->GetMotorTorque());
		_nxVehicle->setBrakeTorque(static_cast<PxU32>(i), std::fabs(wheel->GetBrakeTorque()));
		_nxVehicle->setSteerAngle(static_cast<PxU32>(i), wheel->GetSteerAngle());
	}
}

void Vehicle::SyncOutputs()
{
	if (!_nxVehicle)
		return;

	for (size_t i = 0; i < _wheels.size(); ++i)
	{
		WheelShape* wheel = _wheels[i];
		const PxWheelQueryResult& result = _wheelQueryResults[i];

		wheel->SetAxleSpeed(_nxVehicle->mWheelsDynData.getWheelRotationSpeed(
			static_cast<PxU32>(i)));

		WheelContactData contact;
		contact.contactPoint = D3DXVECTOR3(result.tireContactPoint.x,
			result.tireContactPoint.y, result.tireContactPoint.z);
		contact.contactNormal = D3DXVECTOR3(result.tireContactNormal.x,
			result.tireContactNormal.y, result.tireContactNormal.z);
		contact.longitudalDirection = D3DXVECTOR3(result.tireLongitudinalDir.x,
			result.tireLongitudinalDir.y, result.tireLongitudinalDir.z);
		contact.lateralDirection = D3DXVECTOR3(result.tireLateralDir.x,
			result.tireLateralDir.y, result.tireLateralDir.z);

		contact.contactForce = result.suspSpringForce;
		contact.longitudalSlip = result.longitudinalSlip;
		contact.lateralSlip = result.lateralSlip;
		//2.8 reported the impulse the solver applied. PxVehicle does not expose
		//one, and every caller in the game only tests the slips, so these stay
		//zero rather than carry an invented number.
		contact.longitudalImpulse = 0.0f;
		contact.lateralImpulse = 0.0f;

		contact.otherShapeMaterialIndex = 0;

		/*
		 * contactPosition is what CarWheel::PxSyncWheel places the wheel with:
		 * it subtracts the radius and uses the remainder as suspension
		 * compression. suspJounce is measured the other way -- positive into
		 * the travel from full droop -- so it is converted rather than passed
		 * through.
		 */
		const float travel = wheel->GetSuspensionTravel();
		contact.contactPosition = wheel->GetRadius() + travel - result.suspJounce;

		wheel->SetContactData(contact, result.isInAir ? 0 : result.tireContactShape);

		//Whether the suspension raycast found ground at all. A wheel that never
		//does produces no tire force, so nothing downstream of it can be
		//diagnosed until this reads what it should.
		if (i == 0)
			RRR3D_TRACE_FIRST(20,
				"WHEELQ inAir=%d jounce=%.4f springForce=%.1f contactShape=%p "
				"normal=%.2f,%.2f,%.2f suspDir=%.2f,%.2f,%.2f",
				(int)result.isInAir, result.suspJounce, result.suspSpringForce,
				(void*)result.tireContactShape,
				result.tireContactNormal.x, result.tireContactNormal.y, result.tireContactNormal.z,
				result.suspLineDir.x, result.suspLineDir.y, result.suspLineDir.z);
	}
}

}

}
