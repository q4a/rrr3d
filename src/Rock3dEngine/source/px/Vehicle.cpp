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
 * filter data and nothing else -- so the vehicle's own shapes carry a mark that
 * the pre-filter drops.
 *
 * The mark lives in query filter data word1, and which word is not a detail.
 * The engine already owns three of the four: word0 is the collision group,
 * words 2 and 3 are the group mask, packed by Scene::SetShapeGroupsMask as
 * `word3 = bits2 | (bits3 << 16)` and written to the query filter data as well
 * as the simulation one. A mask with bits3 = 0xffff produces word3 = 0xffff0000
 * exactly -- so a marker in word3 is a value the world hands out to itself, and
 * the track ends up flagged undrivable. Every suspension raycast then misses
 * and the cars hover. word1 is the only word the engine leaves alone in query
 * filter data.
 */
const PxU32 cUndrivableSurface = 0x8000ffffu;

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
	return (filterData1.word1 == cUndrivableSurface)
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

	/*
	 * eACCELERATION, not eVELOCITY_CHANGE.
	 *
	 * eVELOCITY_CHANGE has PxVehicleUpdates write the rigid body's velocity
	 * outright. A car whose wheels are all in the air produces no forces, so
	 * what gets written is the velocity it already had -- and the fall gravity
	 * accumulated during the step is overwritten every step. The car hangs in
	 * the air indefinitely.
	 *
	 * That is exactly what the game did: cars two metres above the track,
	 * frozen, never falling. It read like a broken suspension raycast for a
	 * long time, and the raycast was innocent -- a ray of travel + radius is
	 * about half a metre and the ground was 2.045 m down, so reporting inAir
	 * was the correct answer to the wrong question.
	 *
	 * eACCELERATION contributes accelerations instead and leaves the scene's
	 * own integration, gravity included, alone.
	 */
	PxVehicleSetUpdateMode(PxVehicleUpdateMode::eACCELERATION);

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
	{
		//Worth saying out loud: without a batch query there are no suspension
		//raycasts, so nothing below this line runs and a car has no
		//suspension at all -- which looks exactly like a car whose raycasts
		//all miss.
		RRR3D_TRACE_FIRST(4, "VEHICLE no batch query for %u wheels", totalWheels);
		return;
	}

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

	/*
	 * Wheel offsets come from the PxShape's local pose, not from
	 * WheelShape::GetPos().
	 *
	 * GetPos() is the shape's position within *its own actor*, and a car's
	 * wheels each live on their own child actor -- so those coordinates are
	 * relative to a frame that is not the car's. Using them put all four wheels
	 * near the body origin, inside the chassis convex and two metres above the
	 * track, where a suspension ray of travel + radius could never reach the
	 * ground. Every wheel then correctly reported being in the air, and the
	 * cars hung there.
	 *
	 * Shape::ApplyToShape already resolves the child transform when it sets the
	 * PxShape's local pose, and that pose is relative to the root PxRigidActor,
	 * which is the frame PxVehicle wants.
	 */
	std::vector<PxVec3> offsets(wheelCount);
	for (PxU32 i = 0; i < wheelCount; ++i)
	{
		const PxShape* nxShape = _wheels[i]->GetNxShape();
		const PxVec3 local = nxShape
			? nxShape->getLocalPose().p
			: ToPxVec(_wheels[i]->GetPos());

		offsets[i] = local - centreOfMass;
	}

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

		simData.setSceneQueryFilterData(i, PxFilterData(0, 0, 0, 0));

		_tireData[i].longitudal = wheel->GetLongitudalTireForceFunction();
		_tireData[i].lateral = wheel->GetLateralTireForceFunction();

		//The numbers PxVehicle is actually given, once per wheel at build time.
		//A wheel that will not turn under 12 kNm of drive torque has something
		//wrong in here -- a zero mass or MOI, a radius the geometry does not
		//agree with, a suspension that cannot carry the sprung mass.
		RRR3D_TRACE_FIRST(24,
			"VWHEEL %u radius=%.3f mass=%.1f MOI=%.3f sprung=%.1f "
			"spring=%.0f damper=%.0f compression=%.3f droop=%.3f "
			"offset=%.2f,%.2f,%.2f shape=%d",
			(unsigned)i, wheelData.mRadius, wheelData.mMass, wheelData.mMOI,
			suspension.mSprungMass, suspension.mSpringStrength,
			suspension.mSpringDamperRate, suspension.mMaxCompression,
			suspension.mMaxDroop,
			offsets[i].x, offsets[i].y, offsets[i].z, (int)shapeIndex);
	}

	MarkShapesUndrivable();

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

void Vehicle::MarkShapesUndrivable()
{
	PxRigidDynamic* body = _actor ? _actor->GetNxDynamic() : 0;
	if (!body)
		return;

	/*
	 * Only word1, and re-applied every step.
	 *
	 * The other three words belong to the engine's collision groups, and it
	 * rewrites all four together whenever a shape's group changes -- which
	 * happens after a vehicle is built, not only before. Overwriting them here
	 * would break the group filtering the game's own raycasts depend on;
	 * writing only word1 leaves that intact and survives being rewritten,
	 * because this runs again next step.
	 */
	std::vector<PxShape*> shapes(body->getNbShapes());
	if (shapes.empty())
		return;

	body->getShapes(&shapes[0], static_cast<PxU32>(shapes.size()));

	for (size_t i = 0; i < shapes.size(); ++i)
	{
		PxFilterData filter = shapes[i]->getQueryFilterData();
		if (filter.word1 == cUndrivableSurface)
			continue;

		filter.word1 = cUndrivableSurface;
		shapes[i]->setQueryFilterData(filter);
	}
}

void Vehicle::SyncInputs()
{
	if (!_nxVehicle)
		return;

	//The engine may have rewritten filter data since the last step.
	MarkShapesUndrivable();


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

		/*
		 * 2.8 summed these on the axle; PxVehicle does not.
		 *
		 * NxWheelShapeDesc calls motorTorque the "sum engine torque on the
		 * wheel axle" and brakeTorque "the amount of torque applied for
		 * braking" -- two torques on one axle, and the wheel accelerates
		 * whenever the first exceeds the second. PxVehicle instead treats brake
		 * as a locking mechanism: any non-zero brake engages a sticky-wheel
		 * constraint that pins the rotation at zero.
		 *
		 * That matters because GameCar applies _motor.restTorque, 400 Nm, on
		 * every frame it is not braking. Harmless as a summand; as a lock it is
		 * a permanent handbrake, and the wheels never turned -- 12450 Nm of
		 * drive against a wheel of MOI 0.583 held at exactly zero.
		 *
		 * Netting them off restores the 2.8 arithmetic: brake only reaches
		 * PxVehicle to the extent it exceeds the drive torque opposing it.
		 */
		const float drive = wheel->GetMotorTorque();
		const float brake = std::fabs(wheel->GetBrakeTorque());
		const float netBrake = std::max(0.0f, brake - std::fabs(drive));

		_nxVehicle->setDriveTorque(static_cast<PxU32>(i), drive);
		_nxVehicle->setBrakeTorque(static_cast<PxU32>(i), netBrake);
		_nxVehicle->setSteerAngle(static_cast<PxU32>(i), wheel->GetSteerAngle());
	}

	/*
	 * All four wheels of one vehicle on one line, sampled per vehicle.
	 *
	 * Sampling inside the wheel loop on a shared counter does not work: four
	 * wheels across six cars is twenty-four calls a step, so a fixed modulo
	 * lands on the same wheel index forever. The first cut did exactly that and
	 * reported wheel 3 every time -- a wheel the game never drives -- which read
	 * as "no drive torque anywhere" when the only wheel sampled was the one
	 * legitimately getting none.
	 */
	if (::rrr3d::TraceEnabled())
	{
		static unsigned long sample = 0;
		if ((++sample % 30) == 0 && _wheels.size() >= 4)
			RRR3D_TRACE_FIRST(60,
				"VINPUT drive=%.0f,%.0f,%.0f,%.0f brake=%.0f omega=%.2f,%.2f,%.2f,%.2f",
				_wheels[0]->GetMotorTorque(), _wheels[1]->GetMotorTorque(),
				_wheels[2]->GetMotorTorque(), _wheels[3]->GetMotorTorque(),
				std::fabs(_wheels[0]->GetBrakeTorque()),
				_nxVehicle->mWheelsDynData.getWheelRotationSpeed(0),
				_nxVehicle->mWheelsDynData.getWheelRotationSpeed(1),
				_nxVehicle->mWheelsDynData.getWheelRotationSpeed(2),
				_nxVehicle->mWheelsDynData.getWheelRotationSpeed(3));
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

		/*
		 * DIAGNOSTIC: the same cast, done by hand.
		 *
		 * The suspension raycasts miss everything in the game and hit fine in
		 * the harness. A raw PxScene::raycast from the wheel's own world
		 * position splits that: if this hits and the batch query does not, the
		 * fault is in how the batch query is set up; if neither hits, there is
		 * genuinely nothing under the car and the wheel origins are wrong.
		 *
		 * Straight onto PxScene rather than through Scene::RaycastClosestShape,
		 * because routing it through the engine wrapper stopped SyncInputs
		 * running at all.
		 */
		if (i == 0 && ::rrr3d::TraceEnabled())
		{
			PxRigidDynamic* body = _actor ? _actor->GetNxDynamic() : 0;
			PxScene* scene = body ? body->getScene() : 0;
			if (scene)
			{
				const PxTransform pose = body->getGlobalPose();
				const PxVec3 origin = pose.transform(ToPxVec(wheel->GetPos()));

				//Started a metre below the wheel, because a cast from the wheel
				//itself begins inside the car's own chassis convex and returns
				//distance 0 without ever reaching the ground.
				const PxVec3 below = origin + PxVec3(0.0f, 0.0f, -1.0f);

				PxRaycastBuffer hit;
				const bool found = scene->raycast(below, PxVec3(0.0f, 0.0f, -1.0f),
					200.0f, hit);

				RRR3D_TRACE_FIRST(12,
					"RAWCAST wheelZ=%.2f groundBelowWheel=%.3f shape=%p",
					origin.z,
					(found && hit.hasBlock) ? hit.block.distance + 1.0f : -1.0f,
					(found && hit.hasBlock) ? (void*)hit.block.shape : 0);
			}
		}

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
