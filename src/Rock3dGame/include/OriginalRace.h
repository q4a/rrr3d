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

struct PlayerProfile;
struct ProfileState;

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
    std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
    float emissive = 0.0F;
    float specular = 0.0F;
    float shininess = 128.0F;
    bool ignoreFog = false;
    std::uint16_t atlasColumns = 1;
    std::uint16_t atlasRows = 1;
    float animationRate = 24.0F;
};

struct VisualNode
{
    std::string meshPath;
    Transform transform;
    std::vector<MaterialDefinition> materials;
    int subMesh = -1;
    bool plane = false;
    bool fixedDirection = false;
};

struct ParticleEmitterDefinition
{
    Transform transform;
    std::vector<MaterialDefinition> materials;
    std::uint32_t maximumParticles = 0;
    float lifeMinimum = 0.0F;
    float lifeMaximum = 0.0F;
    float startTimeMinimum = 0.0F;
    float startTimeMaximum = 0.0F;
    float startDuration = 0.0F;
    float densityMinimum = 0.0F;
    float densityMaximum = 0.0F;
    Vec3 startPositionMinimum;
    Vec3 startPositionMaximum;
    Vec3 startScaleMinimum{1.0F, 1.0F, 1.0F};
    Vec3 startScaleMaximum{1.0F, 1.0F, 1.0F};
    Vec3 velocityMinimum;
    Vec3 velocityMaximum;
    Vec3 scaleVelocityMinimum;
    Vec3 scaleVelocityMaximum;
    Vec3 accelerationMinimum;
    Vec3 accelerationMaximum;
    Vec3 gravity;
    bool worldCoordinates = true;
    bool autoRotate = false;
    bool distanceTriggered = false;
};

struct ObjectDefinition
{
    std::string record;
    std::vector<VisualNode> visualNodes;
    std::vector<ParticleEmitterDefinition> particleEmitters;
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

struct VehicleWeaponPlacement
{
    std::string record;
    Quat rotation;
    Vec3 offset;
};

struct VehicleWeaponMount
{
    bool active = false;
    bool show = false;
    Vec3 position;
    std::vector<VehicleWeaponPlacement> placements;
};

struct VehicleNightLight
{
    bool head = true;
    Vec3 position;
    std::array<float, 2> size{1.0F, 1.0F};
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
    std::array<VehicleWeaponMount, 4> weaponMounts;
    std::vector<VehicleNightLight> nightLights;
    r3d::physics::VehicleDescription physics;
    float maximumLife = 100.0F;
};

enum class WeaponSlot
{
    Hyper,
    Mine,
    Primary,
    Support,
};

struct ProjectileDefinition
{
    std::uint32_t type = 0;
    ObjectDefinition visual;
    ObjectDefinition secondaryVisual;
    ObjectDefinition tertiaryVisual;
    Vec3 position;
    Vec3 size;
    Vec3 offset;
    Quat rotation;
    float speed = 0.0F;
    float relativeSpeedMinimum = 13.0F;
    bool relativeSpeed = false;
    float angularSpeed = 0.0F;
    float maximumDistance = 0.0F;
    float minimumLife = 0.0F;
    float mass = 100.0F;
    float damage = 0.0F;
};

struct WeaponDefinition
{
    std::string record;
    std::string name;
    WeaponSlot slot = WeaponSlot::Primary;
    VisualNode visual;
    std::vector<ProjectileDefinition> projectiles;
    float damage = 0.0F;
    std::uint32_t maximumCharge = 0;
    std::uint32_t reloadCharge = 0;
    std::uint32_t chargeStep = 1;
    float shotDelay = 0.1F;
    float repairPeriod = 0.0F;
    float repairValue = 0.0F;
    float reflectValue = 0.0F;
    std::uint32_t projectileType = 0;
    float projectileSpeed = 0.0F;
    float maximumDistance = 85.0F;
    std::string effectTexturePath;
    std::string soundPath;
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

struct AchievementDefinition
{
    std::string name;
    std::uint32_t classId = 0;
    std::uint32_t reward = 0;
    std::uint32_t iterationCount = 1;
    std::uint32_t killsNumber = 0;
    float killsTime = 0.0F;
    std::uint32_t place = 1;
    BonusKind bonusKind = BonusKind::Unknown;
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

enum class EnvironmentSurface
{
    None,
    Grass,
    Water,
    GroundFog,
    Magma,
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
    EnvironmentSurface surface = EnvironmentSurface::None;
    bool planarReflection = false;
    float surfaceHeight = 0.0F;
    float surfaceScroll = 0.0F;
    float hdrLuminanceKey = 1.1F;
    float hdrBrightThreshold = 1.5F;
    float hdrGaussianScalar = 30.0F;
    float hdrExposure = 15.0F;
};

struct RacerSlot
{
    std::string record;
    std::string type;
    std::uint32_t charge = 0;
};

struct Racer
{
    std::string name;
    std::string photoPath;
    std::size_t vehicle = 0;
    bool human = false;
    std::vector<RacerSlot> loadout;
    Vehicle configuredVehicle;
    bool hasConfiguredVehicle = false;
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
    std::vector<std::vector<std::uint32_t>> tracePaths;
    std::vector<TrackCatalogEntry> trackCatalog;
    std::vector<Vehicle> vehicles;
    std::vector<WeaponDefinition> weapons;
    std::vector<AchievementDefinition> achievements;
    std::vector<Racer> racers;
    std::array<std::uint32_t, 3> rewardMoney{};
    std::array<std::uint32_t, 3> rewardPoints{};
    std::vector<std::uint32_t> requiredPoints;
    EnvironmentDescription environment;
    Vehicle vehicle;
};

struct TournamentAdvance
{
    std::size_t trackIndex = 0;
    bool passComplete = false;
    bool passChampion = false;
    bool planetChampion = false;
};

Race loadFirstOriginalRace(const resource::ResourceFileSystem& resources);
Race loadOriginalRace(const resource::ResourceFileSystem& resources,
                      std::size_t trackIndex,
                      std::string_view playerCar = {});
std::size_t resolveOriginalTournamentTrack(
    const Race& race, const PlayerProfile& profile) noexcept;
void writeOriginalTournamentSelection(
    const Race& race, std::size_t trackIndex,
    PlayerProfile& profile) noexcept;
TournamentAdvance completeOriginalTournamentTrack(
    const Race& race, std::size_t trackIndex,
    ProfileState& profile) noexcept;
void applyOriginalPlayerProfile(
    Race& race, const resource::ResourceFileSystem& resources,
    const PlayerProfile& profile);
r3d::physics::WorldDescription makePhysicsDescription(
    const Race& race, const resource::ResourceFileSystem& resources);
bool runOriginalRaceResourceSmokeTest(
    const Race& race, const resource::ResourceFileSystem& resources,
    std::string& error);

} // namespace r3d::game::originalrace
