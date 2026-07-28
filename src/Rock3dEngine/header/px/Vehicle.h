#ifndef PHYSX_VEHICLE_LIBRARY
#define PHYSX_VEHICLE_LIBRARY

/*
 * The vehicle model, over PxVehicleNoDrive.
 *
 * PhysX 3+ deleted NxWheelShape, which in 2.8 was a shape that cast a ray,
 * applied suspension and tire forces to the body, and exposed the results as
 * live readings. There is no equivalent: the vehicle SDK models a wheel as data
 * on a PxVehicleWheels rather than as a shape at all.
 *
 * PxVehicleNoDrive is the closest fit to what this game actually wants. It
 * takes drive torque, brake torque and steer angle per wheel -- exactly what
 * GameCar already computes -- and has no engine, gearbox, clutch or
 * differential of its own, which matters because the game models all four
 * itself in Motor and would otherwise be fighting the SDK for control.
 *
 * The tuning is preserved rather than remapped. PxVehicleTireData describes a
 * tire by lateral stiffness and a friction-vs-slip graph; 2.8's
 * NxTireFunctionDesc is an extremum/asymptote slip curve, and there is no
 * parameter mapping between them. Every car's tuning in Data/Car/*Wheel.txt and
 * db.xml is written in the old terms, so rather than translate it, the old
 * curve maths goes in verbatim through setTireForceShaderFunction and
 * PxVehicleTireData is left unused. Handling is preserved by construction
 * rather than by ear.
 */

#include "px/Physx.h"

#include "vehicle/PxVehicleSDK.h"
#include "vehicle/PxVehicleNoDrive.h"
#include "vehicle/PxVehicleUpdate.h"
#include "vehicle/PxVehicleUtilSetup.h"

#include <vector>

namespace r3d
{

namespace px
{

class Actor;
class WheelShape;

/*
 * The 2.8 tire curve, per wheel, in the form the shader callback receives it.
 *
 * Held by value and by the vehicle rather than read back through WheelShape,
 * because the shader is called from inside PxVehicleUpdates with nothing but
 * this pointer.
 */
struct TireShaderData
{
	TireFunctionDesc longitudal;
	TireFunctionDesc lateral;
};

/*
 * One PxVehicleNoDrive, built from an Actor's WheelShapes.
 *
 * Created and destroyed by the Scene, which is the only thing that knows when
 * an actor has finished being assembled.
 */
class Vehicle
{
private:
	Actor* _actor;
	PxVehicleNoDrive* _nxVehicle;

	std::vector<WheelShape*> _wheels;
	std::vector<TireShaderData> _tireData;

	//Scratch for the per-wheel query results PxVehicleUpdates writes back.
	std::vector<PxWheelQueryResult> _wheelQueryResults;

	bool BuildWheelsSimData(PxVehicleWheelsSimData& simData, PxRigidDynamic& body);
public:
	Vehicle(Actor* actor);
	~Vehicle();

	//False when the actor turned out not to be a vehicle, or setup failed. A
	//half-built vehicle is never registered.
	bool IsValid() const;

	Actor* GetActor();
	PxVehicleWheels* GetNxVehicle();
	unsigned GetWheelCount() const;

	//The game writes motor, brake and steer onto WheelShape; this pushes them
	//into the vehicle immediately before an update.
	void SyncInputs();
	//And this reads suspension and tire results back onto WheelShape, so
	//GetContact and GetAxleSpeed answer with the current step.
	void SyncOutputs();

	PxVehicleWheelQueryResult GetQueryResultBuffer();
};

/*
 * The scene-wide half: the batch query the suspension raycasts go through, the
 * surface/tire friction table, and the vehicle list.
 *
 * Separate from Scene so that Physx.h does not have to include the vehicle SDK
 * headers, which would put them in front of every translation unit that touches
 * physics at all.
 */
class VehicleScene
{
private:
	PxScene* _nxScene;

	PxBatchQuery* _batchQuery;
	PxVehicleDrivableSurfaceToTireFrictionPairs* _frictionPairs;

	//Sized to the largest wheel count the query has been asked for, and grown
	//rather than reallocated per frame.
	std::vector<PxRaycastQueryResult> _raycastResults;
	std::vector<PxRaycastHit> _raycastHits;
	unsigned _queryCapacity;

	std::vector<Vehicle*> _vehicles;

	void EnsureQueryCapacity(unsigned wheels);
public:
	VehicleScene(PxScene* nxScene);
	~VehicleScene();

	//Called once per SDK lifetime, alongside PxInitExtensions.
	static void InitSDK(PxPhysics& sdk);
	static void ReleaseSDK();

	Vehicle* Add(Actor* actor);
	void Remove(Actor* actor);
	Vehicle* Find(Actor* actor);

	//Suspension raycasts, then the vehicle update, then the results back onto
	//the WheelShapes. Runs immediately before PxScene::simulate.
	void Update(float deltaTime, const PxVec3& gravity);

	bool IsEmpty() const;
};

}

}

#endif
