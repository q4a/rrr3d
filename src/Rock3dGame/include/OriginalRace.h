#pragma once

#include "physics/OriginalVehiclePhysics.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
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

// Proj::CreatePxBox stores ComputeAABB(false) as a PhysX box whose pose is
// local to the projectile actor.  Keep that source representation separate
// from visual bounds so gameplay contacts do not need guessed radii.
struct ProjectileCollisionBox
{
    Vec3 center;
    Vec3 halfExtents;
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
    // Material stores source ValueRange values.  D3D9 evaluates both ranges
    // with the active graph/Fx frame immediately before every draw.
    std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, 4> colorMaximum{1.0F, 1.0F, 1.0F, 1.0F};
    float alphaMinimum = 1.0F;
    float alphaMaximum = 1.0F;
    float emissive = 0.0F;
    float specular = 0.0F;
    float shininess = 128.0F;
    bool ignoreFog = false;
    std::uint16_t atlasColumns = 1;
    std::uint16_t atlasRows = 1;
    float animationRate = 24.0F;
    // Optional second sampler from LibMaterial (the source bump/normal map).
    std::string normalTexturePath{};
    // Bonus\\maslo uses its second 2D sampler with
    // D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR rather than as a normal map.
    std::string reflectionTexturePath{};
    bool reflectionTextureCoordinates = false;
    // Material::moZWrite is independent from D3D blending (for example the
    // opaque j_swell sprite disables depth writes).
    bool writeDepth = true;
    // Animated Image2D sampler offset range.  The original renderer feeds
    // the owning visual node's normalized animation frame into this range.
    Vec3 textureOffsetMinimum;
    Vec3 textureOffsetMaximum;

    MaterialDefinition() = default;
    MaterialDefinition(std::string sourceRecord,
                       std::string sourceTexture,
                       MaterialBlend sourceBlend,
                       float sourceAlphaReference)
        : record(sourceRecord),
          texturePath(sourceTexture),
          blend(sourceBlend),
          alphaReference(sourceAlphaReference)
    {
    }
};

struct VisualNode
{
    enum class CullMode
    {
        Inherit,
        Clockwise,
        CounterClockwise,
        None,
    };

    enum class AnimationMode
    {
        None = 0,
        Once = 1,
        Repeat = 2,
        Tile = 3,
        TwoSide = 4,
        Manual = 5,
        Inheritance = 6,
    };

    std::string meshPath;
    Transform transform;
    std::vector<MaterialDefinition> materials;
    int subMesh = -1;
    // Both ntPlane and ntSprite use generated quad geometry, but only
    // Sprite::DoRender calls Engine::RenderSpritePT and faces the camera.
    bool plane = false;
    bool billboard = false;
    bool fixedDirection = false;
    bool invertCullFace = false;
    CullMode cullMode = CullMode::Inherit;
    AnimationMode animationMode = AnimationMode::None;
    float animationDuration = 1.0F;
    float animationFrame = 0.0F;
    // BaseSceneNode::OnProgress applies these serialized velocities whenever
    // animMode is not amNone. They animate the node transform itself rather
    // than the material frame.
    Vec3 speedPosition;
    Vec3 speedScale;
    Quat speedRotation;
};

enum class ParticleRenderMode
{
    PointSprite,
    Sprite,
    DirectionalSprite,
    Plane,
    Trail,
    Node,
};

enum class ParticleMaximumAction
{
    WaitForFree,
    ReplaceLatest,
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
    Quat startRotationMinimum;
    Quat startRotationMaximum;
    float rangeLifeMinimum = 0.0F;
    float rangeLifeMaximum = 0.0F;
    Vec3 rangePositionMinimum;
    Vec3 rangePositionMaximum;
    Vec3 rangeScaleMinimum;
    Vec3 rangeScaleMaximum;
    Quat rangeRotationMinimum;
    Quat rangeRotationMaximum;
    Vec3 velocityMinimum;
    Vec3 velocityMaximum;
    Quat rotationVelocityMinimum;
    Quat rotationVelocityMaximum;
    Vec3 scaleVelocityMinimum;
    Vec3 scaleVelocityMaximum;
    Vec3 accelerationMinimum;
    Vec3 accelerationMaximum;
    Vec3 gravity;
    bool worldCoordinates = true;
    bool autoRotate = false;
    bool distanceTriggered = false;
    bool fixedDirection = false;
    ParticleRenderMode renderMode = ParticleRenderMode::Sprite;
    // FxManager transforms the normalized particle-group lifetime through
    // the particle-system node animation before applying LibMaterial.
    VisualNode::AnimationMode animationMode =
        VisualNode::AnimationMode::None;
    float animationDuration = 1.0F;
    float animationFrame = 0.0F;
    // FxParticleSystem is a BaseSceneNode too. Its node transform advances
    // before the emitter evaluates its individual particles.
    Vec3 nodeSpeedPosition;
    Vec3 nodeSpeedScale;
    Quat nodeSpeedRotation;
    ParticleMaximumAction maximumAction =
        ParticleMaximumAction::WaitForFree;
    // FxTrailManager is configured once in DataBase.cpp and shared by the
    // trail/heatTrail records.
    float trailWidth = 0.3F;
    Vec3 trailFixedUp{0.0F, 0.0F, 1.0F};
    bool trailFixedUpEnabled = true;
};

enum class LightingMode
{
    None,
    Standard,
    Pixel,
    Reflection,
    Bump,
    Refraction,
    PlanarReflection,
};

// Runtime render queues used by GraphManager::RenderScenes.  The legacy
// serializer has a known enum/string-table mismatch; ObjectDefinition keeps
// the runtime meaning after that mapping has been applied.
enum class GraphOrder
{
    Default,
    Opacity,
    Effect,
    Last,
};

struct DestructionPieceDefinition
{
    std::vector<VisualNode> visualNodes;
    std::vector<CollisionShape> collisionShapes;
    Transform transform;
    Vec3 shapePosition;
    Quat shapeRotation;
    Vec3 halfExtents;
    float mass = 0.0F;
    bool dynamic = false;
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
    std::vector<DestructionPieceDefinition> destructionPieces;
    std::vector<std::string> soundPaths;
    Vec3 bodyShapePosition;
    Quat bodyShapeRotation;
    Vec3 bodyHalfExtents;
    float bodyMass = 0.0F;
    bool dynamicBody = false;
    float maximumLife = -1.0F;
    float maximumTimeLife = -1.0F;
    bool destructible = false;
    bool planarReflection = false;
    bool castsShadow = false;
    // IActor::gpCullOpacity: fade this actor when it obscures the player in
    // the original isometric camera.
    bool cullOpacity = false;
    LightingMode lighting = LightingMode::Standard;
    GraphOrder graphOrder = GraphOrder::Default;
};

struct ObjectInstance
{
    std::uint32_t definition = 0;
    Transform transform;
};

struct DecorationFragmentState
{
    std::size_t instance = 0;
    std::size_t piece = 0;
    Transform transform;
};

struct VehicleDeathFragmentState
{
    std::size_t racer = 0;
    std::size_t effect = 0;
    Transform transform;
};

struct DeathEffectDefinition
{
    ObjectDefinition visual;
    Vec3 position;
    Vec3 impulse;
    bool ignoreRotation = false;
};

struct ShotEffectDefinition
{
    ObjectDefinition visual;
    std::vector<std::string> soundPaths;
    Vec3 position;
    Vec3 impulse;
    float duration = 0.0F;
    bool ignoreRotation = false;
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
    ObjectDefinition lowLifeEffect;
    Vec3 lowLifeEffectPosition{0.0F, 0.0F, 0.5F};
    float lowLifeLevel = 0.35F;
    // DataBase::LoadCar creates damageEnergy<car> and attaches a
    // DamageEffect filtered to dtEnergy for every vehicle.
    ObjectDefinition energyDamageEffect;
    ObjectDefinition shieldEffect;
    Vec3 shieldEffectScale{1.3F, 1.7F, 1.7F};
    std::vector<DeathEffectDefinition> deathEffects;
    r3d::physics::VehicleDescription physics;
    float maximumLife = 100.0F;
    LightingMode lighting = LightingMode::Standard;
};

enum class WeaponSlot
{
    Hyper,
    Mine,
    Primary,
    Support,
};

struct NestedProjectileDefinition
{
    bool valid = false;
    std::uint32_t type = 0U;
    ProjectileCollisionBox collision;
    DeathEffectDefinition deathEffect;
    float speed = 0.0F;
    float minimumLife = 0.0F;
    float maximumLife = 0.0F;
    float damage = 0.0F;
};

struct ProjectileDefinition
{
    static constexpr std::size_t invalidProjectile =
        std::numeric_limits<std::size_t>::max();

    std::uint32_t type = 0;
    ObjectDefinition visual;
    ObjectDefinition secondaryVisual;
    ObjectDefinition tertiaryVisual;
    ProjectileCollisionBox secondaryCollision;
    ProjectileCollisionBox tertiaryCollision;
    // GameBase::DeathEffect attached to the projectile model.  This is
    // distinct from model2/model3, which the original weapon code uses for
    // type-specific live/impact visuals.
    DeathEffectDefinition deathEffect;
    Vec3 position;
    Vec3 size;
    // Proj::Desc::sizeAddPx offsets the start of LaserUpdate's PhysX ray.
    // It is deliberately not folded into the projectile collision box.
    Vec3 sizeAddPx;
    Vec3 offset;
    Quat rotation;
    ProjectileCollisionBox collision;
    float speed = 0.0F;
    float relativeSpeedMinimum = 13.0F;
    bool relativeSpeed = false;
    float angularSpeed = 0.0F;
    float maximumDistance = 0.0F;
    float minimumLife = 0.0F;
    float maximumLife = 0.0F;
    float mass = 100.0F;
    float damage = 0.0F;
    bool modelSize = true;
    // MineRipUpdate instantiates model2/model3 as autonomous gotProj
    // records.  Their gameplay values belong to those nested records rather
    // than to the parent mine projectile.
    NestedProjectileDefinition secondaryProjectile;
    NestedProjectileDefinition tertiaryProjectile;
    // Some death effects are themselves gotProj records (the mortar impact
    // creates the source ptCrater contact field).  They live in the weapon's
    // projectile table for renderer asset ownership, but are not fired from
    // the weapon mount.
    std::size_t deathProjectile = invalidProjectile;
    bool spawnOnParentDeath = false;
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
    ShotEffectDefinition shotEffect;
};

enum class BonusKind
{
    Money,
    Medpack,
    Ammunition,
    Shield,
    Speed,
    SlowHazard,
    OilHazard,
    MineHazard,
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
    DeathEffectDefinition deathEffect;
    Transform transform;
    BonusKind kind = BonusKind::Unknown;
    Vec3 size;
    Vec3 offset;
    ProjectileCollisionBox collision;
    float value = 0.0F;
    float speed = 0.0F;
    std::uint32_t projectileType = 0U;
    bool modelSize = true;
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

struct EnvironmentLamp
{
    Vec3 position;
    Quat rotation;
    std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
    float range = 0.0F;
    bool enabled = false;
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
    // Environment::ewGarage disables the sky, fog and sun while wtGarage
    // enables two source spot lamps.  Race worlds keep the legacy defaults.
    bool skyEnabled = true;
    bool fogEnabled = true;
    bool directionalLightEnabled = true;
    bool dynamicReflectionsEnabled = true;
    std::array<EnvironmentLamp, 3> lamps;
};

struct PresentationCamera
{
    Vec3 position;
    Quat rotation;
    float verticalFovDegrees = 90.0F;
    float nearDistance = 1.0F;
    float farDistance = 100.0F;
    bool valid = false;
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
    std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
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
    // Original ctTrack/ctDecoration PhysX triangle meshes retained for
    // gameplay raycasts as well as the Jolt backend.
    std::vector<r3d::physics::TriangleMesh> collisionMeshes;
    std::vector<ObjectDefinition> decorationDefinitions;
    std::vector<ObjectInstance> decorationInstances;
    // Same index as collisionMeshes; invalid means track geometry, otherwise
    // the source ctDecoration instance that owns the PhysX triangle mesh.
    std::vector<std::size_t> collisionMeshDecorationInstances;
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
    std::array<float, 2> touchBorderDamage{};
    std::array<float, 2> touchBorderDamageForce{};
    std::array<float, 2> touchCarDamage{};
    std::array<float, 2> touchCarDamageForce{};
    EnvironmentDescription environment;
    ObjectDefinition rainEffect;
    // Source ctEffects/trail record referenced by the wheel behavior type 9.
    ObjectDefinition wheelTrailEffect;
    Vehicle vehicle;
    PresentationCamera presentationCamera;
};

struct TournamentAdvance
{
    std::size_t trackIndex = 0;
    bool passComplete = false;
    bool passChampion = false;
    bool planetChampion = false;
};

inline constexpr std::uint32_t originalTournamentPlanetCount = 5U;

enum class FinishTransition
{
    RaceMenu,
    PassFailed,
    PassCompleted,
    PlanetCompleted,
    Final,
};

Race loadFirstOriginalRace(const resource::ResourceFileSystem& resources);
Race loadOriginalRace(const resource::ResourceFileSystem& resources,
                      std::size_t trackIndex,
                      std::string_view playerCar = {});
// Builds the exact RaceMenu2::CarFrame scene from db.xml and the already
// parsed garage vehicle/weapon definitions.  It deliberately contains no
// invented track geometry or menu backdrop.
Race loadOriginalGarageScene(
    const resource::ResourceFileSystem& resources,
    const Race& sourceRace);
// Builds RaceMenu2::SpaceshipFrame exactly from the original Misc\space2
// and Misc\angar records, Environment::wtAngar/ewAngar and the serialized
// source camera/lamp transforms.
Race loadOriginalAngarScene(
    const resource::ResourceFileSystem& resources);
std::size_t resolveOriginalTournamentTrack(
    const Race& race, const PlayerProfile& profile) noexcept;
void writeOriginalTournamentSelection(
    const Race& race, std::size_t trackIndex,
    PlayerProfile& profile) noexcept;
TournamentAdvance completeOriginalTournamentTrack(
    const Race& race, std::size_t trackIndex,
    ProfileState& profile) noexcept;
FinishTransition originalFinishTransition(
    const TournamentAdvance& advance, std::uint32_t currentPlanet,
    bool campaign) noexcept;
void applyOriginalPlayerProfile(
    Race& race, const resource::ResourceFileSystem& resources,
    const PlayerProfile& profile);
r3d::physics::WorldDescription makePhysicsDescription(
    const Race& race, const resource::ResourceFileSystem& resources);
bool runOriginalRaceResourceSmokeTest(
    const Race& race, const resource::ResourceFileSystem& resources,
    std::string& error);
bool runOriginalTournamentProgressSmokeTest(std::string& error);

} // namespace r3d::game::originalrace
