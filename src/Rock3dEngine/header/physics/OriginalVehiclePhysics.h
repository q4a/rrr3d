#pragma once

#include <cstddef>
#include <cstdint>
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
    Transform transform;
    Vec3 shapePosition;
    Quat shapeRotation;
    Vec3 halfExtents;
    float mass = 0.0F;
    bool hasBodyShape = false;
    bool dynamic = false;
    bool collisionResponse = true;
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
    };

    Vec3 position;
    float radius = 0.0F;
    float width = 0.0F;
    float suspensionTravel = 0.0F;
    float spring = 0.0F;
    float damper = 0.0F;
    float inverseMass = 0.0F;
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
    // Player::SetCheatK changes these independently of the driver controls.
    // AI rubber-banding uses both; ordinary players retain the neutral 1.
    float motorTorqueScale = 1.0F;
    float lateralGripScale = 1.0F;
    // GameCar::LockSpring suppresses the automatic airborne pitch torque.
    bool springLocked = false;
};

struct BodyContact
{
    CollisionSurface surface = CollisionSurface::TrackPlane;
    std::size_t otherVehicle = std::numeric_limits<std::size_t>::max();
    Vec3 normal;
    float normalSpeed = 0.0F;
    float force = 0.0F;
    // PairPxContactEffect in the Windows engine is keyed by the two PhysX
    // actors and tests sumFrictionForce rather than the normal impulse used
    // by touch damage. Preserve both values and the real manifold points at
    // the backend boundary instead of reconstructing them from a car pose.
    std::uint32_t otherActor =
        std::numeric_limits<std::uint32_t>::max();
    float frictionForce = 0.0F;
    Vec3 point{};
    std::vector<Vec3> points{};
    bool hasPoint = false;
};

struct WheelContactState
{
    Vec3 position;
    float longitudinalSlip = 0.0F;
    float lateralSlip = 0.0F;
    bool hasContact = false;
};

struct VehicleState
{
    Transform body;
    std::vector<Transform> wheels;
    std::vector<float> wheelAngularSpeeds;
    std::vector<WheelContactState> wheelContacts;
    Vec3 linearVelocity;
    float speed = 0.0F;
    float engineRpm = 0.0F;
    // Source CarMotorDesc convention: -1 neutral, 0 reverse, 1..5 forward.
    int gear = -1;
    std::uint32_t contactCount = 0;
    std::uint32_t resetCount = 0;
    std::vector<BodyContact> bodyContacts;
};

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
    virtual void setWheelTractionEnabled(std::size_t index,
                                         bool enabled) noexcept = 0;
    virtual void clampLinearSpeed(std::size_t index,
                                  float maximumSpeed) noexcept = 0;
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
};

std::unique_ptr<OriginalVehicleWorld> createOriginalVehicleWorld(
    const WorldDescription& description, std::string& error);
bool runOriginalVehiclePhysicsSmokeTest(const WorldDescription& description,
                                        std::string& error);

} // namespace r3d::physics
