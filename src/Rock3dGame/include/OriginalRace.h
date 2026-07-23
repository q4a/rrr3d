#pragma once

#include "physics/OriginalVehiclePhysics.h"

#include <cstdint>
#include <string>
#include <vector>

namespace r3d::resource
{
class ResourceFileSystem;
}

namespace r3d::game::originalrace
{

using Vec3 = r3d::physics::Vec3;
using Quat = r3d::physics::Quat;
using Transform = r3d::physics::Transform;

struct CollisionShape
{
    std::string meshPath;
    std::uint32_t materialGroup = 0;
};

struct ObjectDefinition
{
    std::string record;
    std::string visualMeshPath;
    std::string texturePath;
    Transform visualTransform;
    std::vector<CollisionShape> collisionShapes;
};

struct ObjectInstance
{
    std::uint32_t definition = 0;
    Transform transform;
};

struct TracePoint
{
    std::uint32_t id = 0;
    Vec3 position;
    float width = 0.0F;
};

struct Vehicle
{
    std::string record;
    std::string bodyMeshPath;
    std::string wheelMeshPath;
    std::string texturePath;
    std::string idleSoundPath;
    std::string rpmSoundPath;
    Transform bodyVisualTransform;
    std::vector<Transform> wheelVisualTransforms;
    std::vector<Vec3> wheelVisualOffsets;
    r3d::physics::VehicleDescription physics;
};

struct Race
{
    std::string levelPath;
    std::uint32_t lapCount = 0;
    std::vector<ObjectDefinition> trackDefinitions;
    std::vector<ObjectInstance> trackInstances;
    std::vector<TracePoint> tracePoints;
    std::vector<std::uint32_t> tracePath;
    Vehicle vehicle;
};

Race loadFirstOriginalRace(const resource::ResourceFileSystem& resources);
r3d::physics::WorldDescription makePhysicsDescription(
    const Race& race, const resource::ResourceFileSystem& resources);
bool runOriginalRaceResourceSmokeTest(
    const Race& race, const resource::ResourceFileSystem& resources,
    std::string& error);

} // namespace r3d::game::originalrace
