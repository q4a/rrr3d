#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace r3d::physics
{

struct Vec3
{
	float x = 0.0F;
	float y = 0.0F;
	float z = 0.0F;
};

struct TrackGeometry
{
	float leftCenterX = -72.0F;
	float rightCenterX = 72.0F;
	float centerZ = 0.0F;
	float radius = 19.0F;
	float roadHalfWidth = 7.0F;
	float rampStartX = 8.0F;
	float rampPeakX = 18.0F;
	float rampHeight = 1.4F;
	float startX = -52.0F;
};

struct TrackSample
{
	Vec3 position;
	Vec3 tangent;
	float distance = 0.0F;
};

struct VehicleInput
{
	float throttle = 0.0F;
	float brake = 0.0F;
	float steering = 0.0F;
};

struct VehicleState
{
	Vec3 position;
	float heading = 0.0F;
	float speed = 0.0F;
	float verticalVelocity = 0.0F;
	float lapProgress = 0.0F;
	float elapsedSeconds = 0.0F;
	std::uint32_t collisionCount = 0;
	std::uint32_t resetCount = 0;
	std::uint8_t checkpointsPassed = 0;
	bool grounded = true;
	bool finished = false;
};

// Portable boundary between gameplay and the selected physics implementation.
// No PhysX types or platform handles are allowed through this interface.
class PhysicsWorld
{
  public:
	virtual ~PhysicsWorld() = default;

	virtual void reset() noexcept = 0;
	virtual void reset(const Vec3 &position, float heading) noexcept = 0;
	virtual void step(float seconds, const VehicleInput &input) noexcept = 0;

	virtual const VehicleState &vehicle() const noexcept = 0;
	virtual const TrackGeometry &track() const noexcept = 0;
	virtual float trackLength() const noexcept = 0;
	virtual TrackSample sampleTrack(float distance) const noexcept = 0;
	virtual float nearestTrackDistance(const Vec3 &position) const noexcept = 0;
};

std::unique_ptr<PhysicsWorld> createMinimalVehiclePhysics(const TrackGeometry &track);

// Deterministic backend-level coverage for acceleration, braking, steering,
// wall collision, ramp take-off/landing, reset, and fixed-step replay.
bool runMinimalVehiclePhysicsSmokeTest(const TrackGeometry &track, std::string &error);

} // namespace r3d::physics
