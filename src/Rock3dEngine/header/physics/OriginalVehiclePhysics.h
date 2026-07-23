#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace r3d::physics
{

struct Vec3
{
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct Quat
{
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float w = 1.0F;
};

struct Transform
{
    Vec3 position;
    Vec3 scale{1.0F, 1.0F, 1.0F};
    Quat rotation;
};

struct TriangleMesh
{
    std::vector<Vec3> vertices;
    std::vector<std::uint32_t> indices;
    Transform transform;
};

struct WheelDescription
{
    Vec3 position;
    float radius = 0.0F;
    float width = 0.0F;
    float suspensionTravel = 0.0F;
    float spring = 0.0F;
    float damper = 0.0F;
    float inverseMass = 0.0F;
    bool driven = false;
    bool steering = false;
};

struct VehicleDescription
{
    float mass = 0.0F;
    Vec3 halfExtents;
    Vec3 shapePosition;
    Vec3 centerOfMass;
    float brakeTorque = 0.0F;
    float differentialRatio = 0.0F;
    float maximumRpm = 0.0F;
    float maximumTorque = 0.0F;
    float steerAngle = 0.0F;
    std::vector<WheelDescription> wheels;
};

struct VehicleSpawn
{
    VehicleDescription vehicle;
    Vec3 position;
    Vec3 direction{1.0F, 0.0F, 0.0F};
};

struct WorldDescription
{
    std::vector<TriangleMesh> collisionMeshes;
    // The legacy fields keep the Windows/M9 single-car data contract intact.
    // Multi-car races populate spawns; the first spawn is always the human.
    VehicleDescription vehicle;
    Vec3 startPosition;
    Vec3 startDirection{1.0F, 0.0F, 0.0F};
    std::vector<VehicleSpawn> spawns;
    float gravity = -20.0F;
};

struct VehicleInput
{
    float throttle = 0.0F;
    float brake = 0.0F;
    float steering = 0.0F;
};

struct VehicleState
{
    Transform body;
    std::vector<Transform> wheels;
    Vec3 linearVelocity;
    float speed = 0.0F;
    float engineRpm = 0.0F;
    std::uint32_t contactCount = 0;
    std::uint32_t resetCount = 0;
};

class OriginalVehicleWorld
{
public:
    virtual ~OriginalVehicleWorld() = default;
    virtual void reset() noexcept = 0;
    virtual void resetVehicle(std::size_t index, Vec3 position,
                              Vec3 direction) noexcept = 0;
    virtual void step(float seconds, const VehicleInput& input) noexcept = 0;
    virtual void step(float seconds,
                      const std::vector<VehicleInput>& inputs) noexcept = 0;
    virtual const VehicleState& vehicle() const noexcept = 0;
    virtual const VehicleState& vehicle(std::size_t index) const noexcept = 0;
    virtual std::size_t vehicleCount() const noexcept = 0;
};

std::unique_ptr<OriginalVehicleWorld> createOriginalVehicleWorld(
    const WorldDescription& description, std::string& error);
bool runOriginalVehiclePhysicsSmokeTest(const WorldDescription& description,
                                        std::string& error);

} // namespace r3d::physics
