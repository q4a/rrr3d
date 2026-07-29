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
 * Higher is closer to a rigid contact, and it trades against stability: the
 * tire's reaction on the wheel is stiffness * load * radius, and a reaction
 * large enough to overshoot in one sub-step reverses the wheel, flips the sign
 * of the slip, and starts a limit cycle.
 *
 * The value was 2.0, chosen from a harness sweep that is now void. That sweep
 * ran while PxVehicle believed no car was ever accelerating -- see
 * WheelShape::SetDragTorque -- which sent every wheel down the coasting branch
 * of computeTireSlips and produced slips nothing like the ones a driven wheel
 * sees. The old table is not reproduced here because none of its rows describe
 * the model that now runs.
 *
 * Re-swept against the harness with the game's own configuration and torque,
 * jointly with the wheel inertia it interacts with (see mMOI below):
 *
 *          MOI x10       MOI x20       MOI x30       MOI x50
 *   K=2    1 failure     1 failure     1 failure     1 failure
 *   K=4    1 failure     1 failure     none          1 failure
 *   K=8    3 failures    3 failures    none          none
 *
 * K=4 at MOI x30 is the corner of that: it clears every check at the lowest
 * stiffness that does, which leaves the most room for a tire to break away and
 * come back. The slide is a large part of how this game drives, and a tire
 * stiff enough to never lose grip cannot produce one.
 *
 * RRR3D_TIRE_STIFFNESS overrides it, because this is the number to reach for
 * when the cars feel wrong to drive rather than merely wrong on paper.
 */
const float cContactStiffness = 4.0f;

PxQueryHitType::Enum SuspensionQueryPreFilter(
	PxFilterData filterData0, PxFilterData filterData1,
	const void* constantBlock, PxU32 constantBlockSize,
	PxHitFlags& queryFlags)
{
	return (filterData1.word1 == cUndrivableSurface)
		? PxQueryHitType::eNONE
		: PxQueryHitType::eBLOCK;
}

//The tire model has three numbers that decide whether it settles or rings, and
//they interact. Each is overridable so a sweep needs no rebuild; the defaults
//are the tuning.
float EnvFloat(const char* name, float fallback)
{
	if (const char* value = std::getenv(name))
		return static_cast<float>(std::atof(value));
	return fallback;
}

int EnvInt(const char* name, int fallback)
{
	if (const char* value = std::getenv(name))
		return std::atoi(value);
	return fallback;
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

/*
 * Every shape on a car, in actor space, once.
 *
 * The question this answers is geometric and had been argued about from
 * derived numbers for far too long: where is the chassis, where are the wheels,
 * and does anything reach lower than the tyres? A car held up by its own body
 * collision looks identical from every other reading -- the suspension reports
 * contact, the spring force is right, the wheels simply never touch.
 *
 * Bounds come from PxGeometryQuery rather than per-type accessors so a convex
 * chassis measures the same way a box wheel does.
 */
void DumpActorGeometry(PxRigidDynamic& body, const std::vector<WheelShape*>& wheels)
{
	if (!::rrr3d::TraceEnabled())
		return;

	static int dumped = 0;
	if (dumped++ > 0)
		return;

	const PxVec3 cm = body.getCMassLocalPose().p;
	RRR3D_TRACE("GEOM --- car: mass=%.1f centreOfMass=%.3f,%.3f,%.3f nbShapes=%u",
		body.getMass(), cm.x, cm.y, cm.z, body.getNbShapes());

	std::vector<PxShape*> shapes(body.getNbShapes());
	if (shapes.empty())
		return;
	body.getShapes(&shapes[0], static_cast<PxU32>(shapes.size()));

	static const char* cTypeName[] =
		{ "sphere", "plane", "capsule", "box", "convex", "trimesh", "heightfield" };

	for (size_t i = 0; i < shapes.size(); ++i)
	{
		const PxTransform pose = shapes[i]->getLocalPose();
		const PxBounds3 bounds =
			PxGeometryQuery::getWorldBounds(shapes[i]->getGeometry().any(), pose);

		bool isWheel = false;
		for (size_t w = 0; w < wheels.size(); ++w)
			if (wheels[w]->GetNxShape() == shapes[i])
				isWheel = true;

		const int type = static_cast<int>(shapes[i]->getGeometryType());

		RRR3D_TRACE("GEOM shape %u %-6s %s pos=%.3f,%.3f,%.3f "
			"bounds z %.3f..%.3f  x %.3f..%.3f  y %.3f..%.3f",
			(unsigned)i,
			(type >= 0 && type < 7) ? cTypeName[type] : "?",
			isWheel ? "WHEEL " : "chassis",
			pose.p.x, pose.p.y, pose.p.z,
			bounds.minimum.z, bounds.maximum.z,
			bounds.minimum.x, bounds.maximum.x,
			bounds.minimum.y, bounds.maximum.y);
	}

	for (size_t w = 0; w < wheels.size(); ++w)
		RRR3D_TRACE("GEOM wheel %u radius=%.3f travel=%.3f  lowest reach z=%.3f",
			(unsigned)w, wheels[w]->GetRadius(), wheels[w]->GetSuspensionTravel(),
			(wheels[w]->GetNxShape() ? wheels[w]->GetNxShape()->getLocalPose().p.z : 0.0f)
				- wheels[w]->GetRadius() - wheels[w]->GetSuspensionTravel());
}

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
	//RRR3D_VEHICLE_VELCHANGE selects eVELOCITY_CHANGE instead. Both have been
	//tried against the game and the cars behave identically under each, so the
	//update mode is not what is stopping them; eACCELERATION stays the default
	//because it leaves the scene's own gravity integration alone.
	PxVehicleSetUpdateMode(std::getenv("RRR3D_VEHICLE_VELCHANGE")
		? PxVehicleUpdateMode::eVELOCITY_CHANGE
		: PxVehicleUpdateMode::eACCELERATION);

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

	/*
	 * DIAGNOSTIC: does PxVehicleUpdates move the body at all?
	 *
	 * The tire shader computes tens of kilonewtons and the car does not
	 * accelerate. Everything between those two facts has been inspected except
	 * the step that is supposed to connect them, so this reads the rigid body's
	 * velocity either side of the call. If it is unchanged, the forces are being
	 * computed and thrown away, and nothing upstream of that matters.
	 */
	PxVec3 before(0.0f);
	PxRigidDynamic* firstBody = 0;
	if (::rrr3d::TraceEnabled() && !_vehicles.empty())
	{
		firstBody = _vehicles[0]->GetActor() ? _vehicles[0]->GetActor()->GetNxDynamic() : 0;
		if (firstBody)
			before = firstBody->getLinearVelocity();
	}

	PxVehicleUpdates(deltaTime, gravity, *_frictionPairs,
		static_cast<PxU32>(vehicles.size()), &vehicles[0], &queryResults[0]);

	if (firstBody)
	{
		static unsigned long sample = 0;
		if ((++sample % 120) == 0)
		{
			const PxVec3 after = firstBody->getLinearVelocity();
			const PxVec3 delta = after - before;
			RRR3D_TRACE("VAPPLY before=%.3f,%.3f,%.3f after=%.3f,%.3f,%.3f "
				"delta=%.4f,%.4f,%.4f damping=%.2f",
				before.x, before.y, before.z, after.x, after.y, after.z,
				delta.x, delta.y, delta.z, firstBody->getLinearDamping());
		}
	}

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

	DumpActorGeometry(*body, _wheels);

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
		/*
		 * A disc about its axle, times whatever the drivetrain adds.
		 *
		 * The bare disc is 0.5*m*r^2, about 1.6 kg m^2, and against the torques
		 * this game applies -- CalcTorque's first gear is 12450 Nm at the wheel
		 * -- that is not an inertia, it is a rounding error. The tire reaction
		 * needed to hold such a wheel is stiffness * load * radius, and over a
		 * sub-step it overshoots by orders of magnitude: the wheel reverses, the
		 * slip flips sign, the force reverses, and the wheel ends up alternating
		 * between spinning and stopped while the car creeps.
		 *
		 * 2.8 never had this problem because it never had this loop. Its wheel
		 * contact was a static friction *constraint*, so the solver held the
		 * wheel whatever torque arrived and the wheel's inertia never entered
		 * into it. PxVehicle integrates the wheel explicitly, so the inertia is
		 * suddenly load-bearing and the value inherited from a bare disc is
		 * wrong for the job.
		 *
		 * A driven wheel is rigidly coupled through the gearbox to the engine,
		 * and a drivetrain's inertia reflects to the wheel by the square of the
		 * gear ratio -- which is exactly why driveline models use an effective
		 * inertia far above the bare wheel's. This is that, expressed as one
		 * multiplier because the gear ratio is not visible from here.
		 *
		 * x30 puts it at about 48 kg m^2, which is the right order for an
		 * engine of a few tenths reflected through a first gear of around ten,
		 * and it is where the harness sweep in cContactStiffness stops failing.
		 */
		wheelData.mMOI = 0.5f * mass * radius * radius *
			EnvFloat("RRR3D_WHEEL_MOI", 30.0f);
		//The game sets the actual torque every step; these only cap it.
		wheelData.mMaxBrakeTorque = 1.0e7f;
		wheelData.mMaxSteer = PxPi;
		//RRR3D_WHEEL_DAMPING is a sweep knob: wheel damping is one of the three
		//numbers that decide whether a stiff tire settles or oscillates.
		wheelData.mDampingRate = EnvFloat("RRR3D_WHEEL_DAMPING", 0.25f);
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

		/*
		 * The damper needs a floor, and the reason is a difference between the
		 * two engines rather than a difference between the cars.
		 *
		 * NxSpringDesc says it plainly: "The spring is implicitly integrated, so
		 * even high spring and damper coefficients should be robust." An
		 * implicit integrator is unconditionally stable, so 2.8's suspensions
		 * never had to be well damped to behave -- and the shipped ones are not.
		 * Taking the damping ratio c / (2*sqrt(k*m)) across the cars:
		 *
		 *   spring 625000  damper 25000  ->  0.71, well damped
		 *   spring 140000  damper  1000  ->  0.06, essentially undamped
		 *
		 * PxVehicle integrates explicitly. The second car rings, its tire load
		 * swings between a quarter and nearly three times its rest load, and
		 * traction arrives and leaves several times a second. That reads as a
		 * grip problem and is not one.
		 *
		 * The spring rate is left exactly as tuned, because that is what sets
		 * ride height and how a car feels over bumps. Only the damper is raised,
		 * and only when it falls below what the explicit integrator needs.
		 */
		const float criticalDamping =
			2.0f * std::sqrt(std::max(spring.spring, 1.0f) * std::max(sprungMasses[i], 1.0f));
		const float minDampingRatio = 0.35f;

		PxVehicleSuspensionData suspension;
		suspension.mSpringStrength = spring.spring;
		suspension.mSpringDamperRate =
			std::max(spring.damper, minDampingRatio * criticalDamping);
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

	/*
	 * A floor under the longitudinal slip denominator, which governs coasting
	 * wheels and only coasting wheels.
	 *
	 * PxVehicle computes longitudinal slip two ways, and computeTireSlips picks
	 * between them per wheel. A wheel with drive or brake torque applied gets
	 *
	 *     (w*r - vz) / (max(|vz|, |w*r|) + 0.1 * toleranceLength)
	 *
	 * which ignores this value entirely. A wheel with neither gets
	 *
	 *     (w*r - vz) / max(minLongSlipDenominator, |vz|, |w*r|)
	 *
	 * and the header points at the trap it exists for: as the denominator
	 * approaches zero the slip approaches infinity, so a wheel freewheeling at
	 * walking pace produces a force that overshoots zero and oscillates in sign.
	 *
	 * This was written believing it governed launching from rest, because at
	 * the time it did: computeIsAccelApplied only runs when PxVehicle thinks
	 * the driver is accelerating, and it never thought so -- see
	 * WheelShape::SetDragTorque. With that fixed, driven wheels take the first
	 * branch and this number stops reaching them.
	 *
	 * The floor is chosen against the top speed a car actually reaches; the
	 * shipped maxSpeed is 48. Sub-steps are separate -- the header says raising
	 * them at low forward speed is what makes a stiff tire stable, and stiffness
	 * is what this model needs, force being stiffness * slip * load. A soft tire
	 * has to reach absurd slip before it pulls at all.
	 */
	simData.setMinLongSlipDenominator(EnvFloat("RRR3D_SLIP_DENOM", 4.0f));

	/*
	 * Sub-steps, which are what a stiff tire needs to stay stable.
	 *
	 * The signature is (thresholdSpeed, stepsBelow, stepsAbove), and the third
	 * argument is the one that matters here: above the threshold the whole
	 * vehicle takes a single step per frame, 1/60 s, and the tire reaction that
	 * accelerates a wheel is integrated once over the whole of it. With this
	 * model's stiffness that is enough to overshoot, and an overshoot flips the
	 * sign of the slip, and the wheel ends up alternating between spinning and
	 * stopped without the car ever moving.
	 */
	simData.setSubStepCount(EnvFloat("RRR3D_SUBSTEP_SPEED", 5.0f),
		static_cast<PxU32>(EnvInt("RRR3D_SUBSTEPS_LO", 3)),
		static_cast<PxU32>(EnvInt("RRR3D_SUBSTEPS_HI", 1)));

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

	//RRR3D_VEHICLE_NOBRAKE=1 withholds every brake torque, which is the one-run
	//test of the paragraph below: if the cars drive with it set, the brake is
	//what stops them and nothing else needs investigating.
	static const bool gNoBrake = std::getenv("RRR3D_VEHICLE_NOBRAKE") != 0;

	float maxAccel = 0.0f;
	float maxBrake = 0.0f;

	for (size_t i = 0; i < _wheels.size(); ++i)
	{
		WheelShape* wheel = _wheels[i];

		_tireData[i].longitudal = wheel->GetLongitudalTireForceFunction();
		_tireData[i].lateral = wheel->GetLateralTireForceFunction();
		_nxVehicle->mWheelsDynData.setTireForceShaderData(static_cast<PxU32>(i), &_tireData[i]);

		/*
		 * 2.8 summed drive and brake on one axle; PxVehicle keeps them apart,
		 * and the two halves of that difference cost a fix each.
		 *
		 * NxWheelShapeDesc calls motorTorque the "sum engine torque on the
		 * wheel axle" and brakeTorque "the amount of torque applied for
		 * braking" -- two torques on one axle, and the wheel accelerates
		 * whenever the first exceeds the second. PxVehicle treats brake as a
		 * locking mechanism instead: any non-zero brake engages a sticky-wheel
		 * constraint that pins the rotation at zero. So the game's 400 Nm rest
		 * torque, harmless as a summand, became a permanent handbrake and the
		 * wheels never turned.
		 *
		 * Netting the two off per wheel fixed that and was still not enough,
		 * because brake is not only a lock -- it is also a declaration. See
		 * WheelShape::SetDragTorque: one wheel carrying any brake at all makes
		 * PxVehicle treat the whole car as coasting and arm the sticky-tire
		 * constraints, and netting per wheel could never clear the undriven
		 * pair, which is given brake and no drive.
		 *
		 * So the sum is done here rather than deferred to a mechanism that will
		 * not perform it. Drag opposes the wheel's own rotation -- or, at a
		 * standstill, the torque about to start it -- and PxVehicle sees a
		 * single signed axle torque, exactly as 2.8 did. setBrakeTorque now
		 * carries only what the driver asked for, which is the one case where
		 * a lock and a coasting car are both what is wanted.
		 */
		const float drive = wheel->GetMotorTorque();
		const float brake = gNoBrake ? 0.0f : std::fabs(wheel->GetBrakeTorque());
		const float drag = std::fabs(wheel->GetDragTorque());

		/*
		 * Drag opposes rotation, and must never become rotation.
		 *
		 * The obvious form -- subtract drag in the direction the wheel turns --
		 * has no answer for a wheel that is not turning, and the first cut
		 * signed it by the drive torque instead. That drives a stationary
		 * undriven wheel backwards at 400 Nm, which sends it negative, which
		 * flips the sign, which sends it positive: a wheel chattering about zero
		 * under its own idle drag. It showed up as all four wheels of a
		 * stationary car reading tens of rad/s in opposite directions.
		 *
		 * Below the threshold PxVehicle itself calls stationary -- 0.2 m/s at
		 * the contact patch, so 0.2/radius in rad/s -- drag can only cancel the
		 * drive, never exceed it. A parked wheel with no drive gets nothing,
		 * which is what a parked wheel should get.
		 */
		const float omega = _nxVehicle->mWheelsDynData.getWheelRotationSpeed(
			static_cast<PxU32>(i));
		const float stationary = 0.2f / std::max(wheel->GetRadius(), 0.01f);

		float axleTorque = drive;
		if (omega > stationary)
			axleTorque -= drag;
		else if (omega < -stationary)
			axleTorque += drag;
		else if (drive > 0.0f)
			axleTorque -= std::min(drag, drive);
		else if (drive < 0.0f)
			axleTorque += std::min(drag, -drive);

		_nxVehicle->setDriveTorque(static_cast<PxU32>(i), axleTorque);
		_nxVehicle->setBrakeTorque(static_cast<PxU32>(i), brake);
		_nxVehicle->setSteerAngle(static_cast<PxU32>(i), wheel->GetSteerAngle());

		maxAccel = std::max(maxAccel, std::fabs(axleTorque));
		maxBrake = std::max(maxBrake, brake);
	}

	//The predicate PxVehicleUpdates itself computes, printed rather than left to
	//be derived, because everything downstream of it is invisible from here.
	//
	//PxVehicleUpdate.cpp, updateNoDrive:
	//
	//    const bool isIntentionToAccelerate = (maxAccel>0.0f && 0.0f==maxBrake);
	//
	//maxBrake is taken over *every* wheel of the vehicle, so one wheel carrying
	//a brake torque declares the whole car to be coasting. When it does, each
	//wheel that is turning slowly accumulates a low-forward-speed timer, and
	//after a second PxVehicle activates a sticky-tire *constraint* that holds
	//the contact point at rest. A constraint is not a force: it is solved, and
	//it beats whatever the tire shader computed. That is why 54 kN through a
	//tire could move nothing, and why no force-side measurement could see it.
	const bool intentionToAccelerate = (maxAccel > 0.0f && 0.0f == maxBrake);

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
			//muLong/muLat are the grip ceilings the tuning supplies. db.xml's
			//defaults are 0.02; a set-up car should read about 7. Which of the
			//two is present says whether Player::ApplyMobility ever ran.
			RRR3D_TRACE_FIRST(60,
				"VINPUT drive=%.0f,%.0f,%.0f,%.0f brake=%.0f omega=%.2f,%.2f,%.2f,%.2f "
				"muLong=%.3f muLat=%.3f maxAccel=%.0f maxBrake=%.0f accelIntent=%d",
				_wheels[0]->GetMotorTorque(), _wheels[1]->GetMotorTorque(),
				_wheels[2]->GetMotorTorque(), _wheels[3]->GetMotorTorque(),
				std::fabs(_wheels[0]->GetBrakeTorque()),
				_nxVehicle->mWheelsDynData.getWheelRotationSpeed(0),
				_nxVehicle->mWheelsDynData.getWheelRotationSpeed(1),
				_nxVehicle->mWheelsDynData.getWheelRotationSpeed(2),
				_nxVehicle->mWheelsDynData.getWheelRotationSpeed(3),
				_wheels[0]->GetLongitudalTireForceFunction().extremumValue,
				_wheels[0]->GetLateralTireForceFunction().extremumValue,
				//accelIntent=0 while the throttle is down is the defect: it arms
				//the sticky-tire constraints on every slowly-turning wheel.
				maxAccel, maxBrake, (int)intentionToAccelerate);
	}
}

void Vehicle::SyncOutputs()
{
	if (!_nxVehicle)
		return;

	PxRigidDynamic* const body = _actor ? _actor->GetNxDynamic() : 0;

	for (size_t i = 0; i < _wheels.size(); ++i)
	{
		WheelShape* wheel = _wheels[i];
		const PxShape* nxShape = wheel->GetNxShape();
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
		/*
		 * Measured, not derived from PxVehicle's jounce convention.
		 *
		 * NxWheelContactData defines contactPosition as "the distance on the
		 * spring travel distance where the wheel would end up if it was resting
		 * on the contact point", and CarWheel::PxSyncWheel uses it as
		 * `st = contactPosition - radius`, then draws the wheel st below the
		 * shape's origin. So it is the distance from the suspension attachment
		 * down to the ground, with the wheel centre one radius above that.
		 *
		 * Both of those points are known in world space, so projecting one onto
		 * the suspension direction gives the answer outright. Deriving it from
		 * suspJounce instead means guessing which end of the travel jounce is
		 * measured from, and guessing wrong misplaces every rendered wheel by
		 * the length of the travel -- which is how a car whose suspension is
		 * demonstrably carrying it (jounce 0, spring force at rest load, flat
		 * contact normal) still appears to float above its own shadow.
		 */
		const PxTransform shapePose = body
			? body->getGlobalPose() * (nxShape ? nxShape->getLocalPose() : PxTransform(PxIdentity))
			: PxTransform(PxIdentity);

		const PxVec3 down = result.suspLineDir.isFinite() && !result.suspLineDir.isZero()
			? result.suspLineDir.getNormalized()
			: PxVec3(0.0f, 0.0f, -1.0f);

		contact.contactPosition = result.isInAir
			? wheel->GetRadius() + wheel->GetSuspensionTravel()
			: (result.tireContactPoint - shapePose.p).dot(down);

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
		//Sampled, not first-N. The first calls of a run are the frame a car
		//spawns on, where the velocity is legitimately zero and the suspension
		//has not settled -- which reads exactly like a car that never moves.
		static unsigned long rawSample = 0;
		if (i == 0 && ::rrr3d::TraceEnabled() && (++rawSample % 180) == 0)
		{
			PxRigidDynamic* body = _actor ? _actor->GetNxDynamic() : 0;
			PxScene* scene = body ? body->getScene() : 0;
			if (scene)
			{
				/*
				 * From the wheel's real world centre.
				 *
				 * WheelShape::GetPos() is child-actor-relative, so transforming
				 * it by the root body's pose lands somewhere that is not the
				 * wheel -- which is what made earlier readings of this probe
				 * meaningless. The PxShape's local pose is in the root actor's
				 * frame, which is the one the body pose composes with.
				 *
				 * With the geometry dumped, the expected answer is known: the
				 * wheel sits 0.340 above the road when it is touching. Anything
				 * larger is the gap.
				 */
				const PxTransform pose = body->getGlobalPose() *
					(nxShape ? nxShape->getLocalPose() : PxTransform(PxIdentity));
				const PxVec3 origin = pose.p;

				/*
				 * From the wheel centre, with the car's own shapes filtered
				 * out.
				 *
				 * The first cut started the ray a metre below the wheel, to get
				 * out of the chassis convex. That works right up until the car
				 * is resting on the road, at which point the ray starts *under*
				 * the road and reports whatever lies ten metres further down --
				 * a car sitting correctly on the track reads as one floating
				 * above a chasm. Filtering by the vehicle's own marker is the
				 * honest way to skip the chassis.
				 */
				PxQueryFilterData filter;
				filter.flags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC |
					PxQueryFlag::ePREFILTER;

				struct SkipVehicle: PxQueryFilterCallback
				{
					PxQueryHitType::Enum preFilter(const PxFilterData&, const PxShape* shape,
						const PxRigidActor*, PxHitFlags&) override
					{
						return shape->getQueryFilterData().word1 == cUndrivableSurface
							? PxQueryHitType::eNONE : PxQueryHitType::eBLOCK;
					}
					PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&) override
					{
						return PxQueryHitType::eBLOCK;
					}
				} skipVehicle;

				PxRaycastBuffer hit;
				const bool found = scene->raycast(origin, PxVec3(0.0f, 0.0f, -1.0f),
					200.0f, hit, PxHitFlag::eDEFAULT, filter, &skipVehicle);

				//Sleeping is the first thing to rule out: a sleeping
				//PxRigidDynamic does not fall, and PxVehicleUpdates skips it,
				//which would hold a car in the air indefinitely while every
				//other reading looks healthy.
				const PxVec3 velocity = body->getLinearVelocity();

				RRR3D_TRACE_FIRST(12,
					//Per car, with its own radius, because the models differ:
					//radius and suspension travel both vary between them, and
					//"some cars float higher than others" is a statement about
					//a gap that scales with something.
					"RAWCAST car=%p radius=%.3f groundBelowWheel=%.3f gap=%+.3f "
					"vel=%.2f,%.2f,%.2f",
					(void*)body, wheel->GetRadius(),
					(found && hit.hasBlock) ? hit.block.distance : -1.0f,
					(found && hit.hasBlock) ? hit.block.distance - wheel->GetRadius() : 0.0f,
					velocity.x, velocity.y, velocity.z);
			}
		}

		//Whether the suspension raycast found ground at all. A wheel that never
		//does produces no tire force, so nothing downstream of it can be
		//diagnosed until this reads what it should.
		static unsigned long qSample = 0;
		if (i == 0 && (++qSample % 180) == 0)
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
