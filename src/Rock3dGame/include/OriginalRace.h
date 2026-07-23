#pragma once

#include "physics/OriginalVehiclePhysics.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
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

enum class MaterialBlend
{
    Opaque,
    AlphaTest,
    Transparency,
    Additive,
};

struct MaterialDefinition
{
    std::string record;
    std::string texturePath;
    MaterialBlend blend = MaterialBlend::Opaque;
    float alphaReference = 0.0F;
};

struct VisualNode
{
    std::string meshPath;
    Transform transform;
    std::vector<MaterialDefinition> materials;
    int subMesh = -1;
};

struct ObjectDefinition
{
    std::string record;
    std::vector<VisualNode> visualNodes;
    std::string visualMeshPath;
    std::string texturePath;
    Transform visualTransform;
    std::vector<CollisionShape> collisionShapes;
    float maximumLife = -1.0F;
    bool destructible = false;
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
    std::vector<VisualNode> bodyVisuals;
    std::vector<VisualNode> wheelVisuals;
    std::string bodyMeshPath;
    std::string wheelMeshPath;
    std::string texturePath;
    std::string idleSoundPath;
    std::string rpmSoundPath;
    Transform bodyVisualTransform;
    std::vector<Transform> wheelVisualTransforms;
    std::vector<Vec3> wheelVisualOffsets;
    r3d::physics::VehicleDescription physics;
    float maximumLife = 100.0F;
};

enum class BonusKind
{
    Money,
    Medpack,
    Ammunition,
    Mine,
    Shield,
    Speed,
    Unknown,
};

struct BonusInstance
{
    std::string record;
    ObjectDefinition visual;
    Transform transform;
    BonusKind kind = BonusKind::Unknown;
    float value = 0.0F;
};

enum class Weather
{
    Fair,
    Night,
    Cloudy,
    Rainy,
    Sahara,
    Hell,
    Snow,
};

struct EnvironmentDescription
{
    Weather weather = Weather::Fair;
    Vec3 sunPosition;
    Quat sunRotation;
    std::string skyTexturePath;
    std::array<float, 4> fogColor{148.0F / 255.0F,
                                  193.0F / 255.0F,
                                  235.0F / 255.0F, 1.0F};
    std::array<float, 4> ambientColor{0.18F, 0.18F, 0.18F, 1.0F};
    float fogIntensity = 0.5F;
    bool rain = false;
};

struct Racer
{
    std::string name;
    std::size_t vehicle = 0;
    bool human = false;
};

struct TrackCatalogEntry
{
    std::string levelPath;
    std::uint32_t lapCount = 0;
    std::string worldType;
    std::uint32_t planetIndex = 0;
    std::uint32_t racePass = 1;
};

struct Race
{
    std::string levelPath;
    std::uint32_t lapCount = 0;
    std::vector<ObjectDefinition> trackDefinitions;
    std::vector<ObjectInstance> trackInstances;
    std::vector<ObjectDefinition> decorationDefinitions;
    std::vector<ObjectInstance> decorationInstances;
    std::vector<BonusInstance> bonuses;
    std::vector<TracePoint> tracePoints;
    std::vector<std::uint32_t> tracePath;
    std::vector<TrackCatalogEntry> trackCatalog;
    std::vector<Vehicle> vehicles;
    std::vector<Racer> racers;
    EnvironmentDescription environment;
    Vehicle vehicle;
};

Race loadFirstOriginalRace(const resource::ResourceFileSystem& resources);
Race loadOriginalRace(const resource::ResourceFileSystem& resources,
                      std::size_t trackIndex,
                      std::string_view playerCar = {});
r3d::physics::WorldDescription makePhysicsDescription(
    const Race& race, const resource::ResourceFileSystem& resources);
bool runOriginalRaceResourceSmokeTest(
    const Race& race, const resource::ResourceFileSystem& resources,
    std::string& error);

} // namespace r3d::game::originalrace
