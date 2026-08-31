#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
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

enum class CollisionSurface : std::uint8_t
{
    TrackPlane,
    TrackBorder,
    Decoration,
    Vehicle,
    // Map::Map installs an infinite cdgPlaneDeath plane at game Z=0.
    DeathPlane,
};

struct TriangleMesh
{
    std::vector<Vec3> vertices;
    std::vector<std::uint32_t> indices;
    Transform transform;
    CollisionSurface surface = CollisionSurface::TrackPlane;
    // Source ctDecoration instance that owns this PhysX mesh. Keeping the
    // owner is required to remove only that collision body when a gotDestrObj
    // is destroyed; track geometry has no owner.
    std::size_t decorationInstance =
        std::numeric_limits<std::size_t>::max();
};

struct DecorationDescription
{
    struct ChildShape
    {
        Vec3 position;
        Quat rotation;
        Vec3 halfExtents;
    };

    Transform transform;
    Vec3 shapePosition;
    Quat shapeRotation;
    Vec3 halfExtents;
    float mass = 0.0F;
    bool hasBodyShape = false;
    bool dynamic = false;
    bool collisionResponse = true;
    // Actor::InitRootNxActor folds every attached px::Actor child shape into
    // the parent's single actor. DestrObj disables response on that actor,
    // but the shapes must remain present to produce the lethal touch event.
    std::vector<ChildShape> childShapes;
};

struct DecorationState
{
    Transform body;
    bool active = false;
};

struct WheelDescription
{
    struct TireFunction
    {
        float extremumSlip = 0.0F;
        float extremumValue = 0.0F;
        float asymptoteSlip = 0.0F;
        float asymptoteValue = 0.0F;
        float stiffnessFactor = 0.0F;
    };

    Vec3 position;
    float radius = 0.0F;
    float width = 0.0F;
    float suspensionTravel = 0.0F;
    float spring = 0.0F;
    float damper = 0.0F;
    float suspensionTarget = 0.0F;
    float inverseMass = 0.0F;
    std::uint32_t flags = 0U;
    bool driven = false;
    bool steering = false;
    TireFunction longitudinalTire;
    TireFunction lateralTire;
};

struct VehicleDescription
{
    float mass = 0.0F;
    Vec3 halfExtents;
    Vec3 shapePosition;
    Vec3 centerOfMass;
    Vec3 angularDamping{1.0F, 1.0F, 1.0F};
    float bodyFriction = 0.08F;
    float brakeTorque = 0.0F;
    float differentialRatio = 0.0F;
    float maximumRpm = 0.0F;
    float idlingRpm = 1000.0F;
    float maximumTorque = 0.0F;
    // CarMotorDesc::SEM multiplied by its fixed cGameK (1.15).
    float torqueEfficiency = 0.805F;
    float restBrakeTorque = 400.0F;
    float maximumSpeed = 0.0F;
    float tireSpring = 0.0F;
    float airbornePitchAcceleration = 1.9634954084936207F;
    float clampRollAngle = 0.0F;
    float clampPitchAngle = 0.0F;
    float steerAngle = 0.0F;
    float steeringControl = 0.12F;
    float steerSpeed = 1.5707963267948966F;
    float steerRotation = 3.1415926535897932F;
    bool automaticGears = true;
    bool gravitySteering = false;
    bool clutchImmunity = false;
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
    std::vector<DecorationDescription> decorations;
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
    // The Windows mcBack command first brakes forward motion and then
    // engages the dedicated reverse gear. It is distinct from mcBrake,
    // which AI uses as a brake-only command.
    float reverse = 0.0F;
    float brake = 0.0F;
    float steering = 0.0F;
    // GameCar::smManual is independent of the requested angle. AI and an
    // analogue gamepad axis stay manual even when they reach full lock.
    bool manualSteering = false;
    // Player::SetCheatK changes these independently of the driver controls.
    // AI rubber-banding uses both; ordinary players retain the neutral 1.
    float motorTorqueScale = 1.0F;
    float lateralGripScale = 1.0F;
    // GameCar::LockSpring suppresses the automatic airborne pitch torque.
    bool springLocked = false;
};

// Backend-neutral arguments for the original GameCar::OnFixedStep drive
// state machine. A game-layer controller owns gears/RPM/torque; Jolt only
// supplies live wheel/body telemetry and applies the returned command.
struct VehicleFixedStepState
{
    float signedSpeed = 0.0F;
    float absoluteSpeed = 0.0F;
    float horizontalSpeed = 0.0F;
    float drivenWheelAngularSpeed = 0.0F;
    bool anyWheelContact = false;
    bool drivenWheelContact = false;
    // GameCar::WheelsProgress keeps a second flag which is true only when
    // every serialized wheel reports contact. Body contacts are delivered
    // separately by GameCar::OnContact in the same fixed-step lifetime.
    bool allWheelContact = false;
    bool bodyContact = false;
};

struct VehicleDriveCommand
{
    float motorTorque = 0.0F;
    float brakeTorque = 0.0F;
    float engineRpm = 0.0F;
    int gear = -1;
    float steeringAngle = 0.0F;
    float steeringYaw = 0.0F;
    float rearWheelX = 0.0F;
    // Source GameCar::_wheelSteerK after Player::SetCheatK. Jolt applies
    // this to the serialized lateral tire curve but does not own the value.
    float lateralGripScale = 1.0F;
    std::array<float, 3U> angularDamping{1.0F, 1.0F, 1.0F};
    float clampRollAngle = 0.0F;
    float clampPitchAngle = 0.0F;
    bool applyExtraGravity = false;
    float airbornePitchAcceleration = 0.0F;
};

using VehicleFixedStepController = std::function<VehicleDriveCommand(
    std::size_t, float, const VehicleInput&,
    const VehicleFixedStepState&)>;

struct BodyContact
{
    CollisionSurface surface = CollisionSurface::TrackPlane;
    std::size_t otherVehicle = std::numeric_limits<std::size_t>::max();
    Vec3 normal;
    float normalSpeed = 0.0F;
    float force = 0.0F;
    // Source MapObj owning a ctDecoration PhysX actor. DestrObj disables
    // solver response but still receives GameCar::OnContact, so the backend
    // must preserve actor identity even for a sensor manifold.
    std::size_t otherDecoration = std::numeric_limits<std::size_t>::max();
    // PairPxContactEffect in the Windows engine is keyed by the two PhysX
    // actors and tests sumFrictionForce rather than the normal impulse used
    // by touch damage. Preserve both values and the real manifold points at
    // the backend boundary instead of reconstructing them from a car pose.
    std::uint32_t otherActor =
        std::numeric_limits<std::uint32_t>::max();
    float frictionForce = 0.0F;
    // GameCar::OnContact uses sumFrictionForce as a vector when spring
    // borders redirect a fast car.  Keeping only its magnitude is enough
    // for PairPxContactEffect, but loses the source rebound direction.
    Vec3 frictionForceVector{};
    Vec3 point{};
    std::vector<Vec3> points{};
    bool hasPoint = false;
};

struct WheelContactState
{
    Vec3 position;
    Vec3 normal{0.0F, 0.0F, 1.0F};
    float longitudinalSlip = 0.0F;
    float lateralSlip = 0.0F;
    // NxUserWheelContactModify receives the preceding wheel normal force in
    // units of one static wheel load, then caps the force available to the
    // tire constraints at 1.5g (or zero for clutch/tireSpring release). It
    // does not remove vertical suspension support. Keep the tire budget and
    // release state visible at the adapter boundary for parity checks.
    float normalReaction = 0.0F;
    float normalImpulse = 0.0F;
    bool normalForceReleased = false;
    bool hasContact = false;
};

struct VehicleState
{
    Transform body;
    // NxActor wake/sleep events controlled GameObject body progress and
    // graph pose synchronization. Preserve the Jolt activity state instead
    // of inferring it from velocity in the game adapter.
    bool bodyAwake = true;
    std::vector<Transform> wheels;
    std::vector<float> wheelAngularSpeeds;
    std::vector<WheelContactState> wheelContacts;
    Vec3 linearVelocity;
    // PhysX NetPlayer::ResponseStream publishes momentum rather than angular
    // velocity. Preserve the backend's world-space rigid-body value so the
    // source seven-field BitStream can be reproduced without approximation.
    Vec3 angularMomentum;
    // NxActor::computeKineticEnergy returns the sum of translational and
    // rotational rigid-body energy. Keep the backend-computed value because
    // contact attribution cannot reconstruct the exact inertia tensor from a
    // rendered car pose.
    float kineticEnergy = std::numeric_limits<float>::quiet_NaN();
    float speed = 0.0F;
    // GameCar::GetDrivenWheelSpeed is the axle speed of the first wheel
    // outside GetLeadGroup multiplied by its radius. CameraManager uses its
    // signed value to suppress backwards body jitter while that wheel is
    // stopped or rotating in reverse.
    float drivenWheelSpeed = 0.0F;
    float engineRpm = 0.0F;
    // Source CarMotorDesc convention: -1 neutral, 0 reverse, 1..5 forward.
    int gear = -1;
    std::uint32_t contactCount = 0;
    std::uint32_t resetCount = 0;
    std::vector<BodyContact> bodyContacts;
};

struct VehicleResetCommand
{
    std::size_t vehicle = std::numeric_limits<std::size_t>::max();
    Vec3 position;
    Vec3 direction{1.0F, 0.0F, 0.0F};
};

struct VehicleLinearVelocityCommand
{
    std::size_t vehicle = std::numeric_limits<std::size_t>::max();
    Vec3 delta;
};

inline constexpr std::uint64_t invalidProjectileBodyId =
    std::numeric_limits<std::uint64_t>::max();

struct ProjectileBodyDescription
{
    std::uint64_t id = invalidProjectileBodyId;
    Transform transform;
    Vec3 shapePosition;
    Quat shapeRotation;
    Vec3 halfExtents{0.05F, 0.05F, 0.05F};
    Vec3 linearVelocity;
    float mass = 1.0F;
    float gravityFactor = 0.0F;
    bool dynamic = true;
    // Fire/Drobilka own PhysX actors which are repositioned from the mounted
    // weapon every source tick.  A Jolt kinematic sensor preserves that
    // moving contact boundary without integrating it as a free projectile.
    bool kinematic = false;
    // RocketPrepare disables PhysX response but keeps contact reports. Jolt
    // sensors provide the same actor boundary without pushing the cars.
    bool sensor = true;
    // MinePrepare creates its actor in cdgShotTrack. Other projectile and
    // bonus sensors must not block the mine-placement group query.
    bool shotTrack = false;
};

enum class ProjectileBodyCommandKind : std::uint8_t
{
    Create,
    Synchronize,
    Destroy,
};

struct ProjectileBodyCommand
{
    ProjectileBodyCommandKind kind = ProjectileBodyCommandKind::Create;
    ProjectileBodyDescription body;
};

struct ProjectileBodyState
{
    std::uint64_t id = invalidProjectileBodyId;
    Transform body;
    Vec3 linearVelocity;
    // PhysX delivered Proj::OnContact from the projectile actor's real
    // manifold. Preserve the contacted actor identity and point so gameplay
    // does not have to reconstruct contacts from rendered snapshot boxes.
    std::vector<BodyContact> contacts;
    bool active = false;
};

struct WorldRayCastQuery
{
    Vec3 origin;
    Vec3 direction{1.0F, 0.0F, 0.0F};
    float maximumDistance = 0.0F;
    std::size_t ignoredVehicle = std::numeric_limits<std::size_t>::max();
    // MinePrepare and Proj::RocketUpdate use the original track-plane group,
    // while Laser/FrostRay query the complete projectile collision group.
    bool trackPlaneOnly = false;
    // MinePrepare adds cdgShotTrack to cdgTrackPlane so an existing mine is
    // the closest hit and rejects stacking another mine on top of it.
    bool includeProjectileBodies = false;
    // Player::ResetCar additionally includes cdgPlaneDeath. Ordinary
    // projectile queries deliberately exclude it.
    bool includeDeathPlane = false;
};

struct WorldRayCastHit
{
    Vec3 position;
    Vec3 normal;
    CollisionSurface surface = CollisionSurface::TrackPlane;
    float distance = std::numeric_limits<float>::max();
    std::size_t vehicle = std::numeric_limits<std::size_t>::max();
    std::size_t decoration = std::numeric_limits<std::size_t>::max();
    std::size_t projectileBody =
        std::numeric_limits<std::size_t>::max();
    std::uint32_t actor = std::numeric_limits<std::uint32_t>::max();
    bool hit = false;
};

using WorldRayCast =
    std::function<WorldRayCastHit(const WorldRayCastQuery&)>;

// Race::OnFixedStep is a world event: Windows calls it exactly once before
// each PhysX Compute, then dispatches the registered GameCar fixed events.
// Keep that boundary separate from VehicleFixedStepController, which is
// intentionally invoked once per car.  The controller may update the input
// roster and return Player::ResetCar operations that must happen before the
// same solver step rather than one rendered frame later.
using WorldFixedStepController = std::function<void(
    float, const std::vector<VehicleState>&,
    std::vector<VehicleInput>&, std::vector<VehicleResetCommand>&,
    std::vector<VehicleLinearVelocityCommand>&,
    const std::vector<ProjectileBodyState>&,
    std::vector<ProjectileBodyCommand>&)>;

struct DebrisDescription
{
    Transform transform;
    std::vector<TriangleMesh> collisionMeshes;
    Vec3 shapePosition;
    Quat shapeRotation;
    Vec3 halfExtents{0.1F, 0.1F, 0.1F};
    Vec3 localImpulse;
    float mass = 1.0F;
    float lifetime = -1.0F;
    bool dynamic = true;
};

struct DebrisState
{
    Transform body;
    bool active = false;
};

class OriginalVehicleWorld
{
public:
    virtual ~OriginalVehicleWorld() = default;
    virtual void reset() noexcept = 0;
    virtual void resetVehicle(std::size_t index, Vec3 position,
                              Vec3 direction) noexcept = 0;
    virtual void setVehicleEnabled(std::size_t index,
                                   bool enabled) noexcept = 0;
    virtual void addLinearVelocity(std::size_t index,
                                   Vec3 delta) noexcept = 0;
    virtual void addAngularVelocity(std::size_t index,
                                    Vec3 delta) noexcept = 0;
    // GameCar::StabilizeForce installs a complete PhysX angular momentum
    // after applying clutch/oil behavior; it is not an angular-velocity
    // impulse. Preserve that operation at the physics boundary.
    virtual void setAngularMomentum(std::size_t index,
                                    Vec3 momentum) noexcept = 0;
    // Active NetPlayer::ResponseStream receive path: snap position beyond
    // four source units, bias momentum for smaller divergence, snap a
    // sufficiently divergent graph rotation, then install both momenta.
    // The game layer passes the graph comparison because an earlier visual
    // correction can leave it intentionally different from the Jolt body.
    virtual void synchronizeNetworkVehicle(
        std::size_t index, Vec3 position, Quat rotation,
        Vec3 linearMomentum, Vec3 angularMomentum,
        bool graphRotationRequiresSnap = false) noexcept = 0;
    virtual void setWheelTractionEnabled(std::size_t index,
                                         bool enabled) noexcept = 0;
    virtual void clampLinearSpeed(std::size_t index,
                                  float maximumSpeed) noexcept = 0;
    virtual void setVehicleFixedStepController(
        VehicleFixedStepController controller) = 0;
    virtual void setWorldFixedStepController(
        WorldFixedStepController controller) = 0;
    virtual void applyProjectileBodyCommands(
        const std::vector<ProjectileBodyCommand>& commands) noexcept = 0;
    virtual void step(float seconds, const VehicleInput& input) noexcept = 0;
    virtual void step(float seconds,
                      const std::vector<VehicleInput>& inputs) noexcept = 0;
    virtual const VehicleState& vehicle() const noexcept = 0;
    virtual const VehicleState& vehicle(std::size_t index) const noexcept = 0;
    virtual std::size_t vehicleCount() const noexcept = 0;
    virtual void setDecorationEnabled(std::size_t index,
                                      bool enabled) noexcept = 0;
    virtual const DecorationState& decoration(
        std::size_t index) const noexcept = 0;
    virtual std::size_t decorationCount() const noexcept = 0;
    virtual std::size_t addDebris(
        const DebrisDescription& description) noexcept = 0;
    virtual const DebrisState& debris(std::size_t index) const noexcept = 0;
    virtual std::size_t debrisCount() const noexcept = 0;
    virtual const ProjectileBodyState& projectileBody(
        std::size_t index) const noexcept = 0;
    virtual std::size_t projectileBodyCount() const noexcept = 0;
    virtual WorldRayCastHit raycast(
        const WorldRayCastQuery& query) const noexcept = 0;
};

std::unique_ptr<OriginalVehicleWorld> createOriginalVehicleWorld(
    const WorldDescription& description, std::string& error);
bool runOriginalVehiclePhysicsSmokeTest(const WorldDescription& description,
                                        std::string& error);

} // namespace r3d::physics
