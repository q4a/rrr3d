#include "physics/OriginalVehiclePhysics.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/EstimateCollisionResponse.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace r3d::physics
{
namespace
{

namespace Layers
{
constexpr JPH::ObjectLayer nonMoving = 0;
constexpr JPH::ObjectLayer moving = 1;
} // namespace Layers

namespace BroadPhaseLayers
{
constexpr JPH::BroadPhaseLayer nonMoving(0);
constexpr JPH::BroadPhaseLayer moving(1);
constexpr JPH::uint count = 2;
} // namespace BroadPhaseLayers

class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
{
public:
    bool ShouldCollide(JPH::ObjectLayer first,
                       JPH::ObjectLayer second) const override
    {
        return first != Layers::nonMoving || second == Layers::moving;
    }
};

class BroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface
{
public:
    JPH::uint GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayers::count;
    }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(
        JPH::ObjectLayer layer) const override
    {
        return layer == Layers::nonMoving ? BroadPhaseLayers::nonMoving
                                         : BroadPhaseLayers::moving;
    }
};

class ObjectVsBroadPhaseFilter final
    : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
    bool ShouldCollide(JPH::ObjectLayer layer,
                       JPH::BroadPhaseLayer broadPhase) const override
    {
        return layer == Layers::moving ||
               broadPhase == BroadPhaseLayers::moving;
    }
};

class JoltRuntime
{
public:
    JoltRuntime()
    {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory;
        JPH::RegisterTypes();
    }

    ~JoltRuntime()
    {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
};

JoltRuntime& runtime()
{
    static JoltRuntime value;
    return value;
}

JPH::Vec3 toJolt(Vec3 value)
{
    return {value.x, value.z, value.y};
}

JPH::Vec3 toJoltAngular(Vec3 value)
{
    // Z-up -> Y-up swaps two axes and therefore changes handedness.
    // Angular velocity is an axial vector and needs det(P) * P.
    return {-value.x, -value.z, -value.y};
}

JPH::Quat toJolt(Quat value)
{
    return {-value.x, -value.z, -value.y, value.w};
}

Vec3 fromJolt(JPH::Vec3Arg value)
{
    return {value.GetX(), value.GetZ(), value.GetY()};
}

Quat fromJolt(JPH::QuatArg value)
{
    // Changing from game Z-up to Jolt Y-up is a reflection. Quaternion
    // imaginary components are axial, hence det(P) * P.
    return {-value.GetX(), -value.GetZ(), -value.GetY(), value.GetW()};
}

Vec3 quaternionToEulerXYZ(Quat value)
{
    const float sinRoll =
        2.0F * (value.w * value.x + value.y * value.z);
    const float cosRoll =
        1.0F - 2.0F * (value.x * value.x + value.y * value.y);
    const float sinPitch = std::clamp(
        2.0F * (value.w * value.y - value.z * value.x),
        -1.0F, 1.0F);
    const float sinYaw =
        2.0F * (value.w * value.z + value.x * value.y);
    const float cosYaw =
        1.0F - 2.0F * (value.y * value.y + value.z * value.z);
    return {std::atan2(sinRoll, cosRoll), std::asin(sinPitch),
            std::atan2(sinYaw, cosYaw)};
}

Quat quaternionFromEulerXYZ(Vec3 value)
{
    const float cr = std::cos(0.5F * value.x);
    const float sr = std::sin(0.5F * value.x);
    const float cp = std::cos(0.5F * value.y);
    const float sp = std::sin(0.5F * value.y);
    const float cy = std::cos(0.5F * value.z);
    const float sy = std::sin(0.5F * value.z);
    return {sr * cp * cy - cr * sp * sy,
            cr * sp * cy + sr * cp * sy,
            cr * cp * sy - sr * sp * cy,
            cr * cp * cy + sr * sp * sy};
}

Vec3 transformPoint(const Transform& transform, Vec3 value)
{
    value.x *= transform.scale.x;
    value.y *= transform.scale.y;
    value.z *= transform.scale.z;
    const auto& q = transform.rotation;
    const Vec3 t{2.0F * (q.y * value.z - q.z * value.y),
                 2.0F * (q.z * value.x - q.x * value.z),
                 2.0F * (q.x * value.y - q.y * value.x)};
    const Vec3 rotated{
        value.x + q.w * t.x + (q.y * t.z - q.z * t.y),
        value.y + q.w * t.y + (q.z * t.x - q.x * t.z),
        value.z + q.w * t.z + (q.x * t.y - q.y * t.x)};
    return {rotated.x + transform.position.x,
            rotated.y + transform.position.y,
            rotated.z + transform.position.z};
}

constexpr JPH::uint64 bodyKindMask = 0xf000000000000000ULL;
constexpr JPH::uint64 vehicleBodyKind = 0x1000000000000000ULL;
constexpr JPH::uint64 surfaceBodyKind = 0x2000000000000000ULL;

JPH::uint64 vehicleUserData(std::size_t index)
{
    return vehicleBodyKind | static_cast<JPH::uint64>(index);
}

JPH::uint64 surfaceUserData(CollisionSurface surface)
{
    return surfaceBodyKind | static_cast<JPH::uint64>(surface);
}

bool vehicleIndex(JPH::uint64 userData, std::size_t& index)
{
    if ((userData & bodyKindMask) != vehicleBodyKind)
        return false;
    index = static_cast<std::size_t>(userData & ~bodyKindMask);
    return true;
}

CollisionSurface collisionSurface(JPH::uint64 userData)
{
    if ((userData & bodyKindMask) != surfaceBodyKind)
        return CollisionSurface::TrackPlane;
    const auto value = static_cast<std::uint8_t>(userData & ~bodyKindMask);
    return value <= static_cast<std::uint8_t>(CollisionSurface::Decoration)
               ? static_cast<CollisionSurface>(value)
               : CollisionSurface::TrackPlane;
}

class OriginalContactListener final : public JPH::ContactListener
{
public:
    void resize(std::size_t count)
    {
        std::scoped_lock lock(mutex_);
        pending_.assign(count, {});
    }

    void beginStep()
    {
        std::scoped_lock lock(mutex_);
        for (auto& contacts : pending_)
            contacts.clear();
    }

    std::vector<BodyContact> take(std::size_t index)
    {
        std::scoped_lock lock(mutex_);
        if (index >= pending_.size())
            return {};
        return std::move(pending_[index]);
    }

    void OnContactAdded(const JPH::Body& first, const JPH::Body& second,
                        const JPH::ContactManifold& manifold,
                        JPH::ContactSettings& settings) override
    {
        configureMaterial(first, second, settings);
        record(first, second, manifold, settings);
    }

    void OnContactPersisted(const JPH::Body& first,
                            const JPH::Body& second,
                            const JPH::ContactManifold& manifold,
                            JPH::ContactSettings& settings) override
    {
        configureMaterial(first, second, settings);
        record(first, second, manifold, settings);
    }

private:
    static void configureMaterial(
        const JPH::Body& first, const JPH::Body& second,
        JPH::ContactSettings& settings) noexcept
    {
        std::size_t firstVehicle = 0;
        std::size_t secondVehicle = 0;
        const bool firstIsVehicle =
            vehicleIndex(first.GetUserData(), firstVehicle);
        const bool secondIsVehicle =
            vehicleIndex(second.GetUserData(), secondVehicle);
        if (!firstIsVehicle && !secondIsVehicle)
            return;
        const JPH::Body& other = firstIsVehicle ? second : first;
        if (collisionSurface(other.GetUserData()) ==
            CollisionSurface::TrackBorder)
        {
            // The Windows border material uses NX_CM_MAX and dynamic
            // friction 4.0.
            settings.mCombinedFriction = 4.0F;
        }
        else
        {
            // Car materials use NX_CM_MIN (0.08 or the 0.02 wake model).
            settings.mCombinedFriction =
                std::min(first.GetFriction(), second.GetFriction());
        }
    }

    void recordOne(std::size_t vehicle, const JPH::Body& body,
                   const JPH::Body& other, JPH::Vec3Arg outwardNormal,
                   float estimatedForce)
    {
        const JPH::Vec3 bodyVelocity = body.GetLinearVelocity();
        std::size_t otherVehicle = std::numeric_limits<std::size_t>::max();
        const bool otherIsVehicle =
            vehicleIndex(other.GetUserData(), otherVehicle);
        const JPH::Vec3 otherVelocity =
            otherIsVehicle ? other.GetLinearVelocity()
                           : JPH::Vec3::sZero();
        const float normalSpeed = std::max(
            0.0F, -(bodyVelocity - otherVelocity).Dot(outwardNormal));
        const float bodyInverseMass =
            body.GetMotionProperties()->GetInverseMass();
        const float otherInverseMass =
            otherIsVehicle
                ? other.GetMotionProperties()->GetInverseMass()
                : 0.0F;
        const float inverseMass = bodyInverseMass + otherInverseMass;
        constexpr float originalContactStep = 1.0F / 120.0F;
        const float effectiveMassForce =
            inverseMass > 0.0F
                ? normalSpeed / (inverseMass * originalContactStep)
                : 0.0F;
        const float force =
            std::max(effectiveMassForce, estimatedForce);

        BodyContact contact;
        contact.surface =
            otherIsVehicle ? CollisionSurface::Vehicle
                           : collisionSurface(other.GetUserData());
        contact.otherVehicle = otherVehicle;
        contact.normal = fromJolt(outwardNormal);
        contact.normalSpeed = normalSpeed;
        contact.force = force;

        std::scoped_lock lock(mutex_);
        if (vehicle >= pending_.size())
            return;
        auto& contacts = pending_[vehicle];
        const auto found = std::find_if(
            contacts.begin(), contacts.end(),
            [&](const BodyContact& value) {
                return value.surface == contact.surface &&
                       value.otherVehicle == contact.otherVehicle;
            });
        if (found == contacts.end())
            contacts.push_back(contact);
        else if (contact.force > found->force)
            *found = contact;
    }

    void record(const JPH::Body& first, const JPH::Body& second,
                const JPH::ContactManifold& manifold,
                const JPH::ContactSettings& settings)
    {
        JPH::CollisionEstimationResult estimation;
        JPH::EstimateCollisionResponse(
            first, second, manifold, estimation,
            settings.mCombinedFriction,
            settings.mCombinedRestitution, 1.0F, 4U);
        float estimatedForce = 0.0F;
        constexpr float originalContactStep = 1.0F / 120.0F;
        for (const auto& impulse : estimation.mImpulses)
            estimatedForce +=
                std::abs(impulse.mContactImpulse) / originalContactStep;
        std::size_t firstVehicle = 0;
        std::size_t secondVehicle = 0;
        if (vehicleIndex(first.GetUserData(), firstVehicle))
        {
            recordOne(firstVehicle, first, second,
                      -manifold.mWorldSpaceNormal, estimatedForce);
        }
        if (vehicleIndex(second.GetUserData(), secondVehicle))
        {
            recordOne(secondVehicle, second, first,
                      manifold.mWorldSpaceNormal, estimatedForce);
        }
    }

    std::mutex mutex_;
    std::vector<std::vector<BodyContact>> pending_;
};

// The original PhysX triangle meshes are used both for solid collision and
// suspension raycasts. PhysX accepted the suspension-facing side independently
// of stored triangle winding; this adapter preserves that behaviour while the
// Jolt mesh itself keeps a single solid front face.
class OriginalWheelCollisionTester final
    : public JPH::VehicleCollisionTester
{
public:
    explicit OriginalWheelCollisionTester(JPH::ObjectLayer layer)
        : VehicleCollisionTester(layer)
    {
    }

    bool Collide(JPH::PhysicsSystem& system,
                 const JPH::VehicleConstraint& vehicle,
                 JPH::uint wheelIndex, JPH::RVec3Arg origin,
                 JPH::Vec3Arg direction, const JPH::BodyID& vehicleBody,
                 JPH::Body*& outBody, JPH::SubShapeID& outSubShape,
                 JPH::RVec3& outPosition, JPH::Vec3& outNormal,
                 float& outLength) const override
    {
        const JPH::DefaultBroadPhaseLayerFilter defaultBroadPhase =
            system.GetDefaultBroadPhaseLayerFilter(mObjectLayer);
        const JPH::BroadPhaseLayerFilter& broadPhase =
            mBroadPhaseLayerFilter != nullptr ? *mBroadPhaseLayerFilter
                                              : defaultBroadPhase;
        const JPH::DefaultObjectLayerFilter defaultObject =
            system.GetDefaultLayerFilter(mObjectLayer);
        const JPH::ObjectLayerFilter& object =
            mObjectLayerFilter != nullptr ? *mObjectLayerFilter
                                          : defaultObject;
        const JPH::IgnoreSingleBodyFilter defaultBody(vehicleBody);
        const JPH::BodyFilter& bodyFilter =
            mBodyFilter != nullptr ? *mBodyFilter : defaultBody;
        const auto* wheel = vehicle.GetWheel(wheelIndex)->GetSettings();
        const float rayLength = wheel->mSuspensionMaxLength + wheel->mRadius;
        const JPH::RRayCast ray{origin, rayLength * direction};

        class Collector final : public JPH::CastRayCollector
        {
        public:
            Collector(JPH::PhysicsSystem& physics, const JPH::RRayCast& value)
                : system(physics), ray(value)
            {
            }

            void AddHit(const JPH::RayCastResult& hit) override
            {
                if (hit.mFraction >= GetEarlyOutFraction())
                    return;
                JPH::BodyLockRead lock(system.GetBodyLockInterfaceNoLock(),
                                       hit.mBodyID);
                if (!lock.Succeeded() || lock.GetBody().IsSensor())
                    return;
                std::size_t vehicle = 0;
                const JPH::uint64 userData = lock.GetBody().GetUserData();
                if (vehicleIndex(userData, vehicle) ||
                    ((userData & bodyKindMask) == surfaceBodyKind &&
                     collisionSurface(userData) ==
                         CollisionSurface::TrackBorder))
                    return;
                const auto position = ray.GetPointOnRay(hit.mFraction);
                auto normal = lock.GetBody().GetWorldSpaceSurfaceNormal(
                    hit.mSubShapeID2, position);
                if (normal.Dot(JPH::Vec3::sAxisY()) < 0.0F)
                    normal = -normal;
                if (normal.Dot(JPH::Vec3::sAxisY()) <=
                    std::cos(80.0F * 3.14159265358979323846F / 180.0F))
                    return;
                UpdateEarlyOutFraction(hit.mFraction);
                body = &lock.GetBody();
                subShape = hit.mSubShapeID2;
                contactPosition = position;
                contactNormal = normal;
            }

            JPH::PhysicsSystem& system;
            JPH::RRayCast ray;
            const JPH::Body* body = nullptr;
            JPH::SubShapeID subShape;
            JPH::RVec3 contactPosition;
            JPH::Vec3 contactNormal;
        } collector(system, ray);

        JPH::RayCastSettings settings;
        settings.mBackFaceModeTriangles =
            JPH::EBackFaceMode::CollideWithBackFaces;
        system.GetNarrowPhaseQueryNoLock().CastRay(
            ray, settings, collector, broadPhase, object, bodyFilter);
        if (collector.body == nullptr)
            return false;
        outBody = const_cast<JPH::Body*>(collector.body);
        outSubShape = collector.subShape;
        outPosition = collector.contactPosition;
        outNormal = collector.contactNormal;
        outLength = std::max(
            0.0F, rayLength * collector.GetEarlyOutFraction() - wheel->mRadius);
        return true;
    }

    void PredictContactProperties(
        JPH::PhysicsSystem&, const JPH::VehicleConstraint& vehicle,
        JPH::uint wheelIndex, JPH::RVec3Arg origin, JPH::Vec3Arg direction,
        const JPH::BodyID&, JPH::Body*&, JPH::SubShapeID&,
        JPH::RVec3& position, JPH::Vec3& normal,
        float& suspensionLength) const override
    {
        const auto* wheel = vehicle.GetWheel(wheelIndex)->GetSettings();
        const float denominator = direction.Dot(normal);
        if (denominator < -1.0e-6F)
        {
            position = origin + JPH::Vec3(position - origin).Dot(normal) /
                                    denominator * direction;
            suspensionLength = JPH::Clamp(
                JPH::Vec3(position - origin).Dot(direction) - wheel->mRadius,
                0.0F, wheel->mSuspensionMaxLength);
        }
        else
        {
            suspensionLength = wheel->mSuspensionMaxLength;
        }
    }
};

class JoltVehicleWorld final : public OriginalVehicleWorld
{
public:
    explicit JoltVehicleWorld(const WorldDescription& description)
        : description_(description), tempAllocator_(32U * 1024U * 1024U),
          jobs_(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
                std::max(1, static_cast<int>(std::thread::hardware_concurrency()) -
                                1))
    {
        if (description_.spawns.empty())
        {
            description_.spawns.push_back(
                {description_.vehicle, description_.startPosition,
                 description_.startDirection});
        }
        validate();
        system_.Init(4096, 0, 16384, 4096, broadPhaseInterface_,
                     objectVsBroadPhase_, objectLayerPairs_);
        contactListener_.resize(description_.spawns.size());
        system_.SetContactListener(&contactListener_);
        system_.SetGravity(
            toJolt(Vec3{0.0F, 0.0F, description_.gravity}));
        createTrack();
        vehicles_.reserve(description_.spawns.size());
        for (const auto& spawn : description_.spawns)
            createVehicle(spawn);
        system_.OptimizeBroadPhase();
        reset();
    }

    ~JoltVehicleWorld() override
    {
        system_.SetContactListener(nullptr);
        clearDebris();
        for (auto& vehicle : vehicles_)
        {
            if (vehicle.constraint != nullptr)
            {
                if (vehicle.stepListenerRegistered)
                    system_.RemoveStepListener(vehicle.constraint);
                system_.RemoveConstraint(vehicle.constraint);
                vehicle.constraint = nullptr;
                vehicle.controller = nullptr;
            }
        }
        auto& bodies = system_.GetBodyInterface();
        for (auto& vehicle : vehicles_)
        {
            if (!vehicle.body.IsInvalid())
            {
                if (vehicle.enabled)
                    bodies.RemoveBody(vehicle.body);
                bodies.DestroyBody(vehicle.body);
            }
        }
        for (const auto trackBody : trackBodies_)
        {
            if (trackBody.IsInvalid())
                continue;
            bodies.RemoveBody(trackBody);
            bodies.DestroyBody(trackBody);
        }
    }

    void reset() noexcept override
    {
        clearDebris();
        for (std::size_t index = 0; index < vehicles_.size(); ++index)
        {
            const auto& spawn = vehicles_[index].spawn;
            resetVehicle(index, spawn.position, spawn.direction);
        }
    }

    void resetVehicle(std::size_t index, Vec3 position,
                      Vec3 direction) noexcept override
    {
        if (index >= vehicles_.size())
            return;
        setVehicleEnabled(index, true);
        auto& vehicle = vehicles_[index];
        const float length =
            std::sqrt(direction.x * direction.x + direction.y * direction.y);
        if (length <= 0.0001F)
            direction = {1.0F, 0.0F, 0.0F};
        else
        {
            direction.x /= length;
            direction.y /= length;
        }
        const float angle = std::atan2(direction.y, direction.x);
        auto& bodies = system_.GetBodyInterface();
        bodies.SetPositionAndRotation(
            vehicle.body, toJolt(position),
            JPH::Quat::sRotation(JPH::Vec3::sAxisY(), -angle),
            JPH::EActivation::Activate);
        bodies.SetLinearAndAngularVelocity(
            vehicle.body, JPH::Vec3::sZero(), JPH::Vec3::sZero());
        vehicle.controller->SetDriverInput(0.0F, 0.0F, 0.0F, 0.0F);
        for (auto* wheel : vehicle.constraint->GetWheels())
        {
            wheel->SetAngularVelocity(0.0F);
            wheel->SetRotationAngle(0.0F);
            wheel->SetSteerAngle(0.0F);
        }
        vehicle.steeringAngle = 0.0F;
        vehicle.wheelTractionEnabled = true;
        vehicle.currentGear = -1;
        vehicle.motorTorque = 0.0F;
        vehicle.engineRpm = vehicle.spawn.vehicle.idlingRpm;
        ++vehicle.resetCount;
        updateState(vehicle);
    }

    void setVehicleEnabled(std::size_t index,
                           bool enabled) noexcept override
    {
        if (index >= vehicles_.size() ||
            vehicles_[index].enabled == enabled)
            return;
        auto& vehicle = vehicles_[index];
        auto& bodies = system_.GetBodyInterface();
        if (enabled)
        {
            bodies.AddBody(vehicle.body, JPH::EActivation::Activate);
            vehicle.constraint->SetEnabled(true);
            system_.AddStepListener(vehicle.constraint);
            vehicle.stepListenerRegistered = true;
            vehicle.enabled = true;
            updateState(vehicle);
            return;
        }
        if (vehicle.stepListenerRegistered)
        {
            system_.RemoveStepListener(vehicle.constraint);
            vehicle.stepListenerRegistered = false;
        }
        vehicle.constraint->SetEnabled(false);
        bodies.RemoveBody(vehicle.body);
        vehicle.enabled = false;
        updateState(vehicle);
    }

    void step(float seconds, const VehicleInput& input) noexcept override
    {
        std::vector<VehicleInput> inputs(vehicles_.size());
        if (!inputs.empty())
            inputs.front() = input;
        step(seconds, inputs);
    }

    void addLinearVelocity(std::size_t index,
                           Vec3 delta) noexcept override
    {
        if (index >= vehicles_.size() || !vehicles_[index].enabled)
            return;
        auto& bodies = system_.GetBodyInterface();
        bodies.AddLinearVelocity(
            vehicles_[index].body, toJolt(delta));
        bodies.ActivateBody(vehicles_[index].body);
    }

    void addAngularVelocity(std::size_t index,
                            Vec3 delta) noexcept override
    {
        if (index >= vehicles_.size() || !vehicles_[index].enabled)
            return;
        auto& bodies = system_.GetBodyInterface();
        bodies.AddLinearAndAngularVelocity(
            vehicles_[index].body, JPH::Vec3::sZero(),
            toJoltAngular(delta));
        bodies.ActivateBody(vehicles_[index].body);
    }

    void setWheelTractionEnabled(std::size_t index,
                                 bool enabled) noexcept override
    {
        if (index >= vehicles_.size())
            return;
        vehicles_[index].wheelTractionEnabled =
            enabled || vehicles_[index].spawn.vehicle.clutchImmunity;
    }

    void clampLinearSpeed(std::size_t index,
                          float maximumSpeed) noexcept override
    {
        if (index >= vehicles_.size() || !vehicles_[index].enabled ||
            maximumSpeed <= 0.0F)
            return;
        auto& bodies = system_.GetBodyInterface();
        const auto velocity =
            bodies.GetLinearVelocity(vehicles_[index].body);
        const float speed = velocity.Length();
        if (speed > maximumSpeed)
        {
            bodies.SetLinearVelocity(
                vehicles_[index].body,
                velocity * (maximumSpeed / speed));
        }
    }

    void step(float seconds,
              const std::vector<VehicleInput>& rawInputs) noexcept override
    {
        const float simulationSeconds =
            std::clamp(seconds, 0.0F, 0.25F);
        float remaining = simulationSeconds;
        constexpr float fixedStep = 1.0F / 120.0F;
        contactListener_.beginStep();
        while (remaining > 0.0F)
        {
            const float delta = std::min(remaining, fixedStep);
            for (std::size_t index = 0; index < vehicles_.size(); ++index)
            {
                VehicleInput input;
                if (index < rawInputs.size())
                    input = rawInputs[index];
                prepareVehicleStep(vehicles_[index], input, delta);
            }
            system_.Update(delta, 1, &tempAllocator_, &jobs_);
            remaining -= delta;
        }
        for (auto& vehicle : vehicles_)
            updateState(vehicle);
        for (auto& debris : debris_)
        {
            if (!debris.state.active)
                continue;
            if (debris.lifetime > 0.0F)
            {
                debris.lifetime -= simulationSeconds;
                if (debris.lifetime <= 0.0F)
                {
                    destroyDebris(debris);
                    continue;
                }
            }
            updateState(debris);
        }
    }

    const VehicleState& vehicle() const noexcept override
    {
        return vehicles_.front().state;
    }

    const VehicleState& vehicle(std::size_t index) const noexcept override
    {
        return vehicles_[std::min(index, vehicles_.size() - 1U)].state;
    }

    std::size_t vehicleCount() const noexcept override
    {
        return vehicles_.size();
    }

    std::size_t addDebris(
        const DebrisDescription& description) noexcept override
    {
        const JPH::Vec3 halfExtents{
            std::max(description.halfExtents.x, 0.05F),
            std::max(description.halfExtents.z, 0.05F),
            std::max(description.halfExtents.y, 0.05F)};
        const auto box = new JPH::BoxShape(halfExtents);
        const auto shifted = JPH::RotatedTranslatedShapeSettings(
                                 toJolt(description.shapePosition),
                                 toJolt(description.shapeRotation), box)
                                 .Create();
        if (shifted.HasError())
            return std::numeric_limits<std::size_t>::max();
        JPH::BodyCreationSettings settings(
            shifted.Get(), toJolt(description.transform.position),
            toJolt(description.transform.rotation),
            JPH::EMotionType::Dynamic, Layers::moving);
        settings.mOverrideMassProperties =
            JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass =
            std::max(description.mass, 1.0F);
        settings.mFriction = 0.5F;
        settings.mRestitution = 0.5F;
        settings.mEnhancedInternalEdgeRemoval = true;
        settings.mUserData = surfaceUserData(
            CollisionSurface::Decoration);
        DebrisRuntime runtime;
        runtime.body = system_.GetBodyInterface().CreateAndAddBody(
            settings, JPH::EActivation::Activate);
        if (runtime.body.IsInvalid())
            return std::numeric_limits<std::size_t>::max();
        runtime.state.body = description.transform;
        runtime.state.active = true;
        runtime.lifetime = description.lifetime;
        if (description.localImpulse.x != 0.0F ||
            description.localImpulse.y != 0.0F ||
            description.localImpulse.z != 0.0F)
        {
            const JPH::Vec3 worldImpulse =
                toJolt(description.transform.rotation) *
                toJolt(description.localImpulse);
            system_.GetBodyInterface().AddImpulse(
                runtime.body, worldImpulse);
        }
        debris_.push_back(std::move(runtime));
        return debris_.size() - 1U;
    }

    const DebrisState& debris(std::size_t index) const noexcept override
    {
        return index < debris_.size() ? debris_[index].state
                                      : emptyDebris_;
    }

    std::size_t debrisCount() const noexcept override
    {
        return debris_.size();
    }

private:
    struct VehicleRuntime
    {
        VehicleSpawn spawn;
        JPH::BodyID body;
        JPH::Ref<JPH::VehicleConstraint> constraint;
        JPH::WheeledVehicleController* controller = nullptr;
        VehicleState state;
        float steeringAngle = 0.0F;
        float motorTorque = 0.0F;
        float engineRpm = 1000.0F;
        int currentGear = -1;
        std::uint32_t resetCount = 0;
        bool wheelTractionEnabled = true;
        bool enabled = true;
        bool stepListenerRegistered = true;
    };

    struct DebrisRuntime
    {
        JPH::BodyID body;
        DebrisState state;
        float lifetime = -1.0F;
    };

    static float sourceGearRatio(int gear) noexcept
    {
        constexpr std::array<float, 6> ratios{
            1.5F, 2.66F, 1.78F, 1.30F, 1.00F, 0.74F};
        return ratios[static_cast<std::size_t>(
            std::clamp(gear, 0, static_cast<int>(ratios.size() - 1U)))];
    }

    static float sourceRpm(const VehicleDescription& source, int gear,
                           float wheelAngularSpeed) noexcept
    {
        if (gear < 0)
            return source.idlingRpm;
        constexpr float radiansPerRevolution =
            6.28318530717958647692F;
        const float rpm =
            std::abs(wheelAngularSpeed) * sourceGearRatio(gear) *
            source.differentialRatio * 60.0F / radiansPerRevolution;
        return std::min(rpm, source.maximumRpm);
    }

    static float sourceTorque(const VehicleDescription& source,
                              int gear) noexcept
    {
        return source.maximumTorque * sourceGearRatio(gear) *
               source.differentialRatio * source.torqueEfficiency;
    }

    void stabilizeVehicle(VehicleRuntime& vehicle, bool anyContact) noexcept
    {
        const auto& source = vehicle.spawn.vehicle;
        auto& bodies = system_.GetBodyInterface();
        const JPH::Quat rotation = bodies.GetRotation(vehicle.body);
        JPH::Vec3 localAngularVelocity =
            rotation.Conjugated() *
            bodies.GetAngularVelocity(vehicle.body);
        localAngularVelocity.SetX(
            localAngularVelocity.GetX() * source.angularDamping.x);
        localAngularVelocity.SetZ(
            localAngularVelocity.GetZ() * source.angularDamping.y);
        localAngularVelocity.SetY(
            localAngularVelocity.GetY() *
            (vehicle.wheelTractionEnabled
                 ? source.angularDamping.z
                 : 1.0F));
        if (!anyContact)
        {
            if (source.clampRollAngle > 0.0F)
                localAngularVelocity.SetX(std::clamp(
                    localAngularVelocity.GetX(),
                    -2.0F * source.clampRollAngle,
                    2.0F * source.clampRollAngle));
            if (source.clampPitchAngle > 0.0F)
                localAngularVelocity.SetZ(std::clamp(
                    localAngularVelocity.GetZ(),
                    -2.0F * source.clampPitchAngle,
                    2.0F * source.clampPitchAngle));
        }
        bodies.SetAngularVelocity(
            vehicle.body, rotation * localAngularVelocity);

        if (source.clampRollAngle > 0.0F ||
            source.clampPitchAngle > 0.0F)
        {
            Vec3 euler = quaternionToEulerXYZ(fromJolt(rotation));
            if (source.clampRollAngle > 0.0F)
                euler.x = std::clamp(
                    euler.x, -source.clampRollAngle,
                    source.clampRollAngle);
            if (source.clampPitchAngle > 0.0F)
                euler.y = std::clamp(
                    euler.y, -source.clampPitchAngle,
                    source.clampPitchAngle);
            bodies.SetPositionAndRotation(
                vehicle.body, bodies.GetPosition(vehicle.body),
                toJolt(quaternionFromEulerXYZ(euler)),
                JPH::EActivation::Activate);
        }
    }

    void applySourceSteering(VehicleRuntime& vehicle, float input,
                             float delta, bool anyContact,
                             bool drivenContact) noexcept
    {
        const auto& source = vehicle.spawn.vehicle;
        const float target = input * source.steerAngle;
        if (std::abs(input) >= 0.999F && source.steerSpeed > 0.0F)
        {
            if (target > 0.0F)
                vehicle.steeringAngle = std::min(
                    std::max(vehicle.steeringAngle, 0.0F) +
                        source.steerSpeed * delta,
                    source.steerAngle);
            else
                vehicle.steeringAngle = std::max(
                    std::min(vehicle.steeringAngle, 0.0F) -
                        source.steerSpeed * delta,
                    -source.steerAngle);
        }
        else
        {
            // Legacy smManual (analogue pad and AI) writes the angle
            // directly; keyboard left/right uses the ramp above.
            vehicle.steeringAngle = target;
        }

        const bool steeringContact =
            source.gravitySteering ? anyContact : drivenContact;
        if (!steeringContact || !vehicle.wheelTractionEnabled ||
            std::abs(vehicle.steeringAngle) <= 0.0001F ||
            source.steerAngle <= 0.0001F)
            return;
        auto& bodies = system_.GetBodyInterface();
        const auto rotation = bodies.GetRotation(vehicle.body);
        const auto velocity = bodies.GetLinearVelocity(vehicle.body);
        const float forwardSpeed =
            velocity.Dot(rotation * JPH::Vec3::sAxisX());
        const float alpha =
            std::clamp(forwardSpeed / 10.0F, -1.0F, 1.0F);
        const float sourceYaw =
            alpha * (vehicle.steeringAngle / source.steerAngle) *
            source.steerRotation * delta;
        const auto yaw = JPH::Quat::sRotation(
            JPH::Vec3::sAxisY(), -sourceYaw);
        const auto correctedRotation = rotation * yaw;
        float rearWheelX = 0.0F;
        for (const auto& wheel : source.wheels)
            rearWheelX = std::min(rearWheelX, wheel.position.x);
        const JPH::Vec3 rearPivot{rearWheelX, 0.0F, 0.0F};
        const auto position = bodies.GetPosition(vehicle.body);
        const auto pivot = position + rotation * rearPivot;
        bodies.SetPositionAndRotation(
            vehicle.body, pivot - correctedRotation * rearPivot,
            correctedRotation, JPH::EActivation::Activate);
    }

    void prepareVehicleStep(VehicleRuntime& vehicle, VehicleInput input,
                            float delta) noexcept
    {
        if (!vehicle.enabled)
            return;
        input.throttle = std::clamp(input.throttle, 0.0F, 1.0F);
        input.reverse = std::clamp(input.reverse, 0.0F, 1.0F);
        input.brake = std::clamp(input.brake, 0.0F, 1.0F);
        input.steering = std::clamp(input.steering, -1.0F, 1.0F);

        const auto& source = vehicle.spawn.vehicle;
        bool anyContact = false;
        bool drivenContact = false;
        float drivenWheelSpeed = 0.0F;
        bool foundDrivenWheel = false;
        for (JPH::uint index = 0;
             index < vehicle.constraint->GetWheels().size(); ++index)
        {
            const auto* wheel = vehicle.constraint->GetWheel(index);
            anyContact = anyContact || wheel->HasContact();
            if (!source.wheels[index].driven)
                continue;
            drivenContact = drivenContact || wheel->HasContact();
            if (!foundDrivenWheel)
            {
                drivenWheelSpeed = wheel->GetAngularVelocity();
                foundDrivenWheel = true;
            }
        }

        auto& bodies = system_.GetBodyInterface();
        const JPH::Quat rotation = bodies.GetRotation(vehicle.body);
        const JPH::Vec3 velocity =
            bodies.GetLinearVelocity(vehicle.body);
        const float signedSpeed =
            velocity.Dot(rotation * JPH::Vec3::sAxisX());
        // PhysX settles a braked wheel to exact zero. Jolt keeps tiny solver
        // residuals, so use a narrow dead zone at the mcAccel/mcBack
        // direction transition to preserve the source state change.
        constexpr float directionDeadZone = 0.1F;

        float brakeTorque = source.restBrakeTorque;
        float motorTorque = 0.0F;
        float rpm = sourceRpm(
            source, vehicle.currentGear, drivenWheelSpeed);
        if (input.brake > 0.0001F)
        {
            vehicle.currentGear = -1;
            rpm = source.idlingRpm;
            brakeTorque = source.brakeTorque * input.brake;
        }
        else if (input.reverse > 0.0001F)
        {
            if (signedSpeed > directionDeadZone)
            {
                rpm = sourceRpm(
                    source, vehicle.currentGear, drivenWheelSpeed);
                brakeTorque = source.brakeTorque;
            }
            else
            {
                vehicle.currentGear = 0;
                rpm = sourceRpm(source, 0, drivenWheelSpeed);
                if (rpm < source.maximumRpm)
                    motorTorque =
                        -sourceTorque(source, 0) * input.reverse;
            }
        }
        else if (input.throttle > 0.0001F)
        {
            if (signedSpeed < -directionDeadZone)
            {
                vehicle.currentGear = -1;
                rpm = source.idlingRpm;
                brakeTorque = source.brakeTorque;
            }
            else
            {
                if (vehicle.currentGear <= 0)
                    vehicle.currentGear = 1;
                rpm = sourceRpm(
                    source, vehicle.currentGear, drivenWheelSpeed);
                motorTorque =
                    sourceTorque(source, vehicle.currentGear) *
                    input.throttle;
            }
        }

        // GameCar::TransmissionProgress uses the first driven wheel and
        // changes the gear only after the current frame's RPM/torque have
        // already been calculated.
        if (drivenContact && source.automaticGears &&
            vehicle.currentGear > 0)
        {
            if (rpm < source.maximumRpm / 1.8F &&
                vehicle.currentGear > 1)
                --vehicle.currentGear;
            if (rpm >= source.maximumRpm &&
                vehicle.currentGear < 5)
                ++vehicle.currentGear;
        }

        if (source.maximumSpeed > 0.0F &&
            velocity.Length() > source.maximumSpeed)
            motorTorque = brakeTorque;
        vehicle.motorTorque = motorTorque;
        vehicle.engineRpm = rpm;

        stabilizeVehicle(vehicle, anyContact);
        applySourceSteering(
            vehicle, input.steering, delta, anyContact, drivenContact);

        // PhysX accepts restTorque on a rolling powered wheel. Jolt treats
        // the same value as a wheel lock at low speed, which previously held
        // the free rear axle stationary. Keep full braking commands exact,
        // but omit only this incompatible rest brake while power is applied.
        if (std::abs(motorTorque) > 0.0001F &&
            brakeTorque <= source.restBrakeTorque + 0.0001F)
            brakeTorque = 0.0F;
        const float brakeInput =
            source.brakeTorque > 0.0F
                ? std::clamp(
                      brakeTorque / source.brakeTorque, 0.0F, 1.0F)
                : 0.0F;
        const float steeringInput =
            source.steerAngle > 0.0001F
                ? std::clamp(
                      vehicle.steeringAngle / source.steerAngle,
                      -1.0F, 1.0F)
                : 0.0F;
        // Motor/differential propagation is deliberately bypassed. The
        // Windows code writes the complete CarMotorDesc torque to every
        // driven NxWheelShape rather than splitting it across an axle.
        vehicle.controller->SetDriverInput(
            0.0F, steeringInput, brakeInput, 0.0F);
        for (JPH::uint index = 0;
             index < vehicle.constraint->GetWheels().size(); ++index)
        {
            if (!source.wheels[index].driven)
                continue;
            static_cast<JPH::WheelWV*>(
                vehicle.constraint->GetWheel(index))
                ->ApplyTorque(motorTorque, delta);
        }

        if (!anyContact)
        {
            bodies.AddForce(
                vehicle.body,
                toJolt(Vec3{0.0F, 0.0F,
                            source.mass * description_.gravity}));
            const JPH::Vec3 horizontal{
                velocity.GetX(), 0.0F, velocity.GetZ()};
            if (horizontal.Length() > 1.0F &&
                !input.springLocked &&
                source.airbornePitchAcceleration != 0.0F)
            {
                const JPH::Quat currentRotation =
                    bodies.GetRotation(vehicle.body);
                bodies.AddLinearAndAngularVelocity(
                    vehicle.body, JPH::Vec3::sZero(),
                    currentRotation *
                        JPH::Vec3{
                            0.0F, 0.0F,
                            -source.airbornePitchAcceleration * delta});
            }
        }

        if (input.throttle != 0.0F || input.reverse != 0.0F ||
            input.brake != 0.0F || input.steering != 0.0F)
            bodies.ActivateBody(vehicle.body);
    }

    void destroyDebris(DebrisRuntime& debris) noexcept
    {
        if (debris.body.IsInvalid())
            return;
        auto& bodies = system_.GetBodyInterface();
        if (debris.state.active)
            bodies.RemoveBody(debris.body);
        bodies.DestroyBody(debris.body);
        debris.body = JPH::BodyID();
        debris.state.active = false;
    }

    void clearDebris() noexcept
    {
        for (auto& debris : debris_)
            destroyDebris(debris);
        debris_.clear();
    }

    void validate()
    {
        if (description_.collisionMeshes.empty() ||
            description_.spawns.empty())
            throw std::runtime_error("incomplete original race physics data");
        for (const auto& spawn : description_.spawns)
        {
            if (spawn.vehicle.mass <= 0.0F ||
                spawn.vehicle.wheels.size() != 4 ||
                !std::any_of(
                    spawn.vehicle.wheels.begin(),
                    spawn.vehicle.wheels.end(),
                    [](const WheelDescription& wheel) {
                        return wheel.driven;
                    }))
                throw std::runtime_error(
                    "incomplete original vehicle physics data");
        }
    }

    void createTrack()
    {
        constexpr std::size_t surfaceCount = 3U;
        std::array<JPH::TriangleList, surfaceCount> triangles;
        auto surfaceIndex = [](CollisionSurface surface) {
            switch (surface)
            {
            case CollisionSurface::TrackBorder:
                return 1U;
            case CollisionSurface::Decoration:
                return 2U;
            default:
                return 0U;
            }
        };
        for (const auto& mesh : description_.collisionMeshes)
            triangles[surfaceIndex(mesh.surface)].reserve(
                triangles[surfaceIndex(mesh.surface)].size() +
                mesh.indices.size() / 3U);
        for (const auto& mesh : description_.collisionMeshes)
        {
            auto& surfaceTriangles = triangles[surfaceIndex(mesh.surface)];
            for (std::size_t index = 0; index + 2 < mesh.indices.size();
                 index += 3)
            {
                const Vec3 a = transformPoint(
                    mesh.transform, mesh.vertices.at(mesh.indices[index]));
                const Vec3 b = transformPoint(
                    mesh.transform, mesh.vertices.at(mesh.indices[index + 1]));
                const Vec3 c = transformPoint(
                    mesh.transform, mesh.vertices.at(mesh.indices[index + 2]));
                // The Z-up -> Y-up axis exchange reverses handedness, so
                // reverse winding to preserve the original solid front face.
                surfaceTriangles.emplace_back(
                    toJolt(a), toJolt(c), toJolt(b));
            }
        }
        constexpr std::array<CollisionSurface, surfaceCount> surfaces{
            CollisionSurface::TrackPlane,
            CollisionSurface::TrackBorder,
            CollisionSurface::Decoration};
        constexpr std::array<float, surfaceCount> frictions{
            0.1F, 4.0F, 0.5F};
        constexpr std::array<float, surfaceCount> restitutions{
            0.0F, 0.0F, 0.5F};
        for (std::size_t index = 0; index < triangles.size(); ++index)
        {
            if (triangles[index].empty())
                continue;
            JPH::MeshShapeSettings shapeSettings(triangles[index]);
            const auto shapeResult = shapeSettings.Create();
            if (shapeResult.HasError())
                throw std::runtime_error(
                    ("Jolt track mesh: " + shapeResult.GetError()).c_str());
            JPH::BodyCreationSettings settings(
                shapeResult.Get(), JPH::RVec3::sZero(),
                JPH::Quat::sIdentity(), JPH::EMotionType::Static,
                Layers::nonMoving);
            settings.mFriction = frictions[index];
            settings.mRestitution = restitutions[index];
            settings.mUserData = surfaceUserData(surfaces[index]);
            const auto body =
                system_.GetBodyInterface().CreateAndAddBody(
                    settings, JPH::EActivation::DontActivate);
            if (body.IsInvalid())
                throw std::runtime_error(
                    "Jolt could not create track body");
            trackBodies_.push_back(body);
        }
    }

    void createVehicle(const VehicleSpawn& spawn)
    {
        const auto& source = spawn.vehicle;
        const JPH::Vec3 halfExtents = toJolt(source.halfExtents);
        const auto box = new JPH::BoxShape(halfExtents);
        const auto translated = JPH::RotatedTranslatedShapeSettings(
                                    toJolt(source.shapePosition),
                                    JPH::Quat::sIdentity(), box)
                                    .Create();
        if (translated.HasError())
            throw std::runtime_error(
                ("Jolt car shape transform: " + translated.GetError())
                    .c_str());
        const Vec3 centerOfMassOffset{
            source.centerOfMass.x - source.shapePosition.x,
            source.centerOfMass.y - source.shapePosition.y,
            source.centerOfMass.z - source.shapePosition.z};
        const auto shifted = JPH::OffsetCenterOfMassShapeSettings(
                                 toJolt(centerOfMassOffset), translated.Get())
                                 .Create();
        if (shifted.HasError())
            throw std::runtime_error(
                ("Jolt car shape: " + shifted.GetError()).c_str());
        JPH::BodyCreationSettings bodySettings(
            shifted.Get(), toJolt(spawn.position),
            JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic,
            Layers::moving);
        bodySettings.mOverrideMassProperties =
            JPH::EOverrideMassProperties::CalculateInertia;
        bodySettings.mMassPropertiesOverride.mMass = source.mass;
        bodySettings.mFriction = source.bodyFriction;
        bodySettings.mRestitution = 0.0F;
        bodySettings.mEnhancedInternalEdgeRemoval = true;
        bodySettings.mUserData = vehicleUserData(vehicles_.size());
        VehicleRuntime runtime;
        runtime.spawn = spawn;
        runtime.body = system_.GetBodyInterface().CreateAndAddBody(
            bodySettings, JPH::EActivation::Activate);
        if (runtime.body.IsInvalid())
            throw std::runtime_error("Jolt could not create vehicle body");
        JPH::BodyLockWrite lock(system_.GetBodyLockInterface(), runtime.body);
        if (!lock.Succeeded())
            throw std::runtime_error("Jolt could not lock vehicle body");

        JPH::VehicleConstraintSettings settings;
        settings.mUp = JPH::Vec3::sAxisY();
        settings.mForward = JPH::Vec3::sAxisX();
        for (const auto& sourceWheel : source.wheels)
        {
            auto* wheel = new JPH::WheelSettingsWV;
            wheel->mPosition = toJolt(sourceWheel.position);
            wheel->mSuspensionDirection = -JPH::Vec3::sAxisY();
            wheel->mSteeringAxis = JPH::Vec3::sAxisY();
            wheel->mWheelUp = JPH::Vec3::sAxisY();
            wheel->mWheelForward = JPH::Vec3::sAxisX();
            wheel->mSuspensionMinLength = 0.0F;
            wheel->mSuspensionMaxLength = sourceWheel.suspensionTravel;
            wheel->mSuspensionSpring = JPH::SpringSettings(
                JPH::ESpringMode::StiffnessAndDamping,
                sourceWheel.spring, sourceWheel.damper);
            wheel->mRadius = sourceWheel.radius;
            wheel->mWidth = sourceWheel.width;
            const float wheelMass = sourceWheel.inverseMass > 0.0F
                                        ? 1.0F / sourceWheel.inverseMass
                                        : 10.0F;
            wheel->mInertia =
                0.5F * wheelMass * sourceWheel.radius * sourceWheel.radius;
            // NxWheelShape has no equivalent of Jolt's built-in wheel
            // angular drag. Source rolling resistance comes from restTorque.
            wheel->mAngularDamping = 0.0F;
            wheel->mMaxSteerAngle =
                sourceWheel.steering ? source.steerAngle : 0.0F;
            wheel->mMaxBrakeTorque = source.brakeTorque;
            wheel->mMaxHandBrakeTorque = 0.0F;
            auto applyTire = [](JPH::LinearCurve& output,
                                const WheelDescription::TireFunction&
                                    input,
                                bool lateral) {
                if (input.extremumSlip <= 0.0F ||
                    input.extremumValue <= 0.0F ||
                    input.asymptoteSlip <= 0.0F ||
                    input.asymptoteValue <= 0.0F)
                    return;
                const float slipScale =
                    lateral ? 180.0F / 3.14159265358979323846F
                            : 1.0F;
                output.Clear();
                output.Reserve(3);
                output.AddPoint(0.0F, 0.0F);
                output.AddPoint(input.extremumSlip * slipScale,
                                input.extremumValue);
                output.AddPoint(
                    std::max(input.asymptoteSlip,
                             input.extremumSlip) *
                        slipScale,
                    input.asymptoteValue);
            };
            applyTire(wheel->mLongitudinalFriction,
                      sourceWheel.longitudinalTire, false);
            applyTire(wheel->mLateralFriction,
                      sourceWheel.lateralTire, true);
            settings.mWheels.push_back(wheel);
        }

        auto* controllerSettings = new JPH::WheeledVehicleControllerSettings;
        controllerSettings->mEngine.mMaxTorque = source.maximumTorque;
        controllerSettings->mEngine.mMaxRPM =
            std::max(source.maximumRpm, 100.0F);
        controllerSettings->mEngine.mMinRPM = std::clamp(
            source.idlingRpm, 1.0F,
            controllerSettings->mEngine.mMaxRPM);
        controllerSettings->mEngine.mNormalizedTorque.Clear();
        controllerSettings->mEngine.mNormalizedTorque.Reserve(2);
        controllerSettings->mEngine.mNormalizedTorque.AddPoint(
            0.0F, source.torqueEfficiency);
        controllerSettings->mEngine.mNormalizedTorque.AddPoint(
            1.0F, source.torqueEfficiency);
        controllerSettings->mTransmission.mGearRatios =
            {2.66F, 1.78F, 1.30F, 1.00F, 0.74F};
        controllerSettings->mTransmission.mReverseGearRatios = {-1.5F};
        controllerSettings->mTransmission.mMode =
            JPH::ETransmissionMode::Manual;
        // CarMotorDesc shifts at maxRPM and maxRPM / 1.8 with no artificial
        // clutch delay. Keep Jolt's thresholds just inside its assertions.
        controllerSettings->mTransmission.mShiftUpRPM =
            controllerSettings->mEngine.mMaxRPM * 0.999F;
        controllerSettings->mTransmission.mShiftDownRPM =
            controllerSettings->mEngine.mMaxRPM / 1.8F;
        controllerSettings->mTransmission.mSwitchTime = 0.0F;
        controllerSettings->mTransmission.mClutchReleaseTime = 0.0F;
        controllerSettings->mTransmission.mSwitchLatency = 0.0F;
        // Jolt requires differential torque weights to sum to one even in
        // neutral. Keep topology metadata, but leave the manual transmission
        // permanently disengaged; propulsion is applied directly to WheelWV.
        std::vector<JPH::uint> driven;
        for (JPH::uint index = 0; index < source.wheels.size(); ++index)
            if (source.wheels[index].driven)
                driven.push_back(index);
        const std::size_t differentialCount =
            (driven.size() + 1U) / 2U;
        for (std::size_t index = 0; index < driven.size(); index += 2U)
        {
            JPH::VehicleDifferentialSettings differential;
            differential.mLeftWheel =
                static_cast<int>(driven[index]);
            differential.mRightWheel =
                index + 1U < driven.size()
                    ? static_cast<int>(driven[index + 1U])
                    : -1;
            differential.mDifferentialRatio = source.differentialRatio;
            differential.mEngineTorqueRatio =
                1.0F / static_cast<float>(differentialCount);
            controllerSettings->mDifferentials.push_back(differential);
        }
        settings.mController = controllerSettings;
        runtime.constraint =
            new JPH::VehicleConstraint(lock.GetBody(), settings);
        runtime.constraint->SetVehicleCollisionTester(
            new OriginalWheelCollisionTester(Layers::moving));
        system_.AddConstraint(runtime.constraint);
        system_.AddStepListener(runtime.constraint);
        runtime.controller = static_cast<JPH::WheeledVehicleController*>(
            runtime.constraint->GetController());
        runtime.controller->GetTransmission().Set(0, 0.0F);
        vehicles_.push_back(std::move(runtime));
        const std::size_t vehicleIndexValue = vehicles_.size() - 1U;
        vehicles_.back().constraint->SetCombineFriction(
            [this, vehicleIndexValue](JPH::uint, float& longitudinal,
                                      float& lateral, const JPH::Body&,
                                      const JPH::SubShapeID&) {
                // GameCar::IsClutchLocked makes the PhysX wheel contact
                // modifier set every wheel's normal force to zero.  Jolt
                // exposes the equivalent at the tire/contact friction
                // boundary, so oil disables both force components while the
                // source clutch timer is active.
                if (!vehicles_[vehicleIndexValue].wheelTractionEnabled)
                {
                    longitudinal = 0.0F;
                    lateral = 0.0F;
                    return;
                }
                // NxWheelShape evaluates its serialized tire functions
                // directly; the contacted material index is passed to the
                // callback but the original implementation does not scale
                // either tire coefficient by ground friction.
            });
        vehicles_.back().controller->SetTireMaxImpulseCallback(
            [this, vehicleIndexValue](
                JPH::uint wheelIndex, float& longitudinal,
                float& lateral, float suspensionImpulse,
                float longitudinalFriction, float lateralFriction,
                float, float, float deltaTime) {
                const auto& vehicle = vehicles_[vehicleIndexValue];
                const auto& source = vehicle.spawn.vehicle;
                const auto* wheel =
                    vehicle.constraint->GetWheel(wheelIndex);
                float allowedSuspensionImpulse = suspensionImpulse;
                if (wheel->HasContact() && !source.wheels.empty())
                {
                    const float normalAcceleration = std::abs(
                        system_.GetGravity().Dot(
                            wheel->GetContactNormal()));
                    const float normalReactionImpulse =
                        source.mass * normalAcceleration * deltaTime /
                        static_cast<float>(source.wheels.size());
                    if (normalReactionImpulse > 1.0e-6F)
                    {
                        const float reactionRatio =
                            suspensionImpulse / normalReactionImpulse;
                        if (source.tireSpring > 0.0F &&
                            reactionRatio > source.tireSpring)
                            allowedSuspensionImpulse = 0.0F;
                        else
                            allowedSuspensionImpulse = std::min(
                                suspensionImpulse,
                                normalReactionImpulse * 1.5F);
                    }
                }
                longitudinal =
                    longitudinalFriction * allowedSuspensionImpulse;
                lateral = lateralFriction * allowedSuspensionImpulse;
            });
    }

    void updateState(VehicleRuntime& vehicle) noexcept
    {
        if (!vehicle.enabled)
        {
            vehicle.state.linearVelocity = {};
            vehicle.state.speed = 0.0F;
            vehicle.state.engineRpm = 0.0F;
            vehicle.state.gear = -1;
            vehicle.state.contactCount = 0U;
            vehicle.state.bodyContacts.clear();
            vehicle.state.wheels.clear();
            vehicle.state.wheelAngularSpeeds.clear();
            vehicle.state.wheelContacts.clear();
            return;
        }
        JPH::BodyLockRead lock(system_.GetBodyLockInterface(), vehicle.body);
        if (!lock.Succeeded())
            return;
        const JPH::Body& body = lock.GetBody();
        auto& state = vehicle.state;
        state.body.position = fromJolt(body.GetPosition());
        state.body.rotation = fromJolt(body.GetRotation());
        state.body.scale = {1.0F, 1.0F, 1.0F};
        state.linearVelocity = fromJolt(body.GetLinearVelocity());
        state.speed = body.GetLinearVelocity().Length();
        state.engineRpm = vehicle.engineRpm;
        state.gear = vehicle.currentGear;
        state.resetCount = vehicle.resetCount;
        std::size_t vehicleIndexValue =
            static_cast<std::size_t>(&vehicle - vehicles_.data());
        state.bodyContacts = contactListener_.take(vehicleIndexValue);
        state.wheels.clear();
        state.wheelAngularSpeeds.clear();
        state.wheelContacts.clear();
        state.wheels.reserve(vehicle.constraint->GetWheels().size());
        state.wheelAngularSpeeds.reserve(
            vehicle.constraint->GetWheels().size());
        state.wheelContacts.reserve(
            vehicle.constraint->GetWheels().size());
        JPH::uint contacts = 0;
        for (JPH::uint index = 0;
             index < vehicle.constraint->GetWheels().size();
             ++index)
        {
            const auto matrix = vehicle.constraint->GetWheelWorldTransform(
                index, JPH::Vec3::sAxisZ(), JPH::Vec3::sAxisY());
            Transform wheel;
            wheel.position = fromJolt(matrix.GetTranslation());
            wheel.rotation = fromJolt(matrix.GetQuaternion());
            wheel.scale = {1.0F, 1.0F, 1.0F};
            state.wheels.push_back(wheel);
            const auto* joltWheel =
                vehicle.constraint->GetWheel(index);
            state.wheelAngularSpeeds.push_back(
                joltWheel->GetAngularVelocity());
            WheelContactState contact;
            contact.hasContact = joltWheel->HasContact();
            if (contact.hasContact)
            {
                contact.position =
                    fromJolt(joltWheel->GetContactPosition());
                const auto* wheeled =
                    static_cast<const JPH::WheelWV*>(joltWheel);
                contact.longitudinalSlip = wheeled->mLongitudinalSlip;
                contact.lateralSlip = wheeled->mLateralSlip;
                ++contacts;
            }
            state.wheelContacts.push_back(contact);
        }
        state.contactCount = contacts;
    }

    void updateState(DebrisRuntime& debris) noexcept
    {
        if (!debris.state.active || debris.body.IsInvalid())
            return;
        JPH::BodyLockRead lock(system_.GetBodyLockInterface(), debris.body);
        if (!lock.Succeeded())
            return;
        const JPH::Body& body = lock.GetBody();
        debris.state.body.position = fromJolt(body.GetPosition());
        debris.state.body.rotation = fromJolt(body.GetRotation());
    }

    WorldDescription description_;
    BroadPhaseLayerInterface broadPhaseInterface_;
    ObjectVsBroadPhaseFilter objectVsBroadPhase_;
    ObjectLayerPairFilter objectLayerPairs_;
    JPH::PhysicsSystem system_;
    OriginalContactListener contactListener_;
    JPH::TempAllocatorImpl tempAllocator_;
    JPH::JobSystemThreadPool jobs_;
    std::vector<JPH::BodyID> trackBodies_;
    std::vector<VehicleRuntime> vehicles_;
    std::vector<DebrisRuntime> debris_;
    DebrisState emptyDebris_;
};

} // namespace

std::unique_ptr<OriginalVehicleWorld> createOriginalVehicleWorld(
    const WorldDescription& description, std::string& error)
{
    try
    {
        // Jolt's allocator callback must be registered before member
        // construction creates TempAllocatorImpl.
        static_cast<void>(runtime());
        auto result = std::make_unique<JoltVehicleWorld>(description);
        error.clear();
        return result;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return nullptr;
    }
}

bool runOriginalVehiclePhysicsSmokeTest(const WorldDescription& description,
                                        std::string& error)
{
    const auto& selectedVehicle =
        description.spawns.empty()
            ? description.vehicle
            : description.spawns.front().vehicle;
    WorldDescription drivetrainDescription;
    drivetrainDescription.vehicle = selectedVehicle;
    drivetrainDescription.startPosition = {0.0F, 0.0F, 2.0F};
    drivetrainDescription.startDirection = {1.0F, 0.0F, 0.0F};
    drivetrainDescription.gravity = description.gravity;
    drivetrainDescription.spawns.push_back(
        {selectedVehicle, drivetrainDescription.startPosition,
         drivetrainDescription.startDirection});
    TriangleMesh drivetrainFloor;
    drivetrainFloor.surface = CollisionSurface::TrackPlane;
    drivetrainFloor.vertices = {{-400.0F, -400.0F, 0.0F},
                                {400.0F, -400.0F, 0.0F},
                                {400.0F, 400.0F, 0.0F},
                                {-400.0F, 400.0F, 0.0F}};
    drivetrainFloor.indices = {0U, 1U, 2U, 0U, 2U, 3U};
    drivetrainDescription.collisionMeshes.push_back(
        std::move(drivetrainFloor));

    auto world =
        createOriginalVehicleWorld(drivetrainDescription, error);
    if (!world)
        return false;
    VehicleInput input;
    for (int step = 0; step < 240; ++step)
        world->step(1.0F / 120.0F, input);
    const auto settled = world->vehicle();
    const auto& sourceVehicle = selectedVehicle;
    if (std::abs(settled.engineRpm - sourceVehicle.idlingRpm) > 1.0F)
    {
        error = "Jolt engine idle RPM did not preserve CarMotorDesc: " +
                std::to_string(settled.engineRpm) + " vs " +
                std::to_string(sourceVehicle.idlingRpm);
        return false;
    }
    const auto settledWheelContacts = static_cast<std::uint32_t>(
        std::count_if(
            settled.wheelContacts.begin(), settled.wheelContacts.end(),
            [](const WheelContactState& contact) {
                return contact.hasContact;
            }));
    if (settled.contactCount == 0 ||
        settled.wheelContacts.size() != settled.wheels.size() ||
        settledWheelContacts != settled.contactCount)
    {
        error = "original per-wheel contact state did not match map1 "
                "collision; "
                "position=" + std::to_string(settled.body.position.x) + "," +
                std::to_string(settled.body.position.y) + "," +
                std::to_string(settled.body.position.z);
        return false;
    }
    world->setWheelTractionEnabled(0U, false);
    input.throttle = 1.0F;
    bool sawWheelSlip = false;
    for (int step = 0; step < 480; ++step)
    {
        world->step(1.0F / 120.0F, input);
        sawWheelSlip = sawWheelSlip || std::any_of(
            world->vehicle().wheelContacts.begin(),
            world->vehicle().wheelContacts.end(),
            [](const WheelContactState& contact) {
                return contact.hasContact &&
                       (std::abs(contact.longitudinalSlip) > 0.4F ||
                        std::abs(contact.lateralSlip) > 0.6F);
            });
    }
    if (!sawWheelSlip)
    {
        error = "Jolt did not expose the source per-wheel slip trigger";
        return false;
    }
    const float clutchLockedSpeed = world->vehicle().speed;
    WorldDescription immuneDescription = drivetrainDescription;
    immuneDescription.vehicle.clutchImmunity = true;
    immuneDescription.spawns.front().vehicle.clutchImmunity = true;
    auto immuneWorld =
        createOriginalVehicleWorld(immuneDescription, error);
    if (!immuneWorld)
        return false;
    input = {};
    for (int step = 0; step < 240; ++step)
        immuneWorld->step(1.0F / 120.0F, input);
    immuneWorld->setWheelTractionEnabled(0U, false);
    input.throttle = 1.0F;
    for (int step = 0; step < 120; ++step)
        immuneWorld->step(1.0F / 120.0F, input);
    if (immuneWorld->vehicle().speed < 0.5F)
    {
        error = "source clutchImmunity did not preserve tire traction";
        return false;
    }
    world->reset();
    input = {};
    for (int step = 0; step < 240; ++step)
        world->step(1.0F / 120.0F, input);
    input.throttle = 1.0F;
    for (int step = 0; step < 480; ++step)
        world->step(1.0F / 120.0F, input);
    const auto accelerated = world->vehicle();
    if (accelerated.speed < 1.0F || accelerated.engineRpm <= 0.0F)
    {
        error = "original marauder engine did not accelerate on map1: "
                "speed=" + std::to_string(accelerated.speed) +
                ", rpm=" + std::to_string(accelerated.engineRpm) +
                ", gear=" + std::to_string(accelerated.gear);
        for (const float wheelSpeed : accelerated.wheelAngularSpeeds)
            error += ", wheel=" + std::to_string(wheelSpeed);
        return false;
    }
    if (accelerated.speed <= clutchLockedSpeed + 0.5F)
    {
        error = "source clutch lock did not suppress wheel traction";
        return false;
    }
    if (accelerated.wheelAngularSpeeds.size() !=
            sourceVehicle.wheels.size() ||
        accelerated.wheelContacts.size() !=
            sourceVehicle.wheels.size())
    {
        error = "Jolt did not expose every original wheel's angular speed";
        return false;
    }
    bool drivenWheelRolling = false;
    std::size_t freeWheelContacts = 0U;
    std::size_t freeWheelsRolling = 0U;
    for (std::size_t index = 0; index < sourceVehicle.wheels.size();
         ++index)
    {
        if (!accelerated.wheelContacts[index].hasContact)
            continue;
        const bool rolling =
            std::abs(accelerated.wheelAngularSpeeds[index]) > 0.5F;
        if (sourceVehicle.wheels[index].driven)
            drivenWheelRolling = drivenWheelRolling || rolling;
        else
        {
            ++freeWheelContacts;
            if (rolling)
                ++freeWheelsRolling;
        }
    }
    if (!drivenWheelRolling || freeWheelContacts == 0U ||
        freeWheelsRolling != freeWheelContacts)
    {
        error = "non-driven rear wheels remained locked under throttle";
        return false;
    }
    input = {};
    input.brake = 1.0F;
    for (int step = 0; step < 240; ++step)
        world->step(1.0F / 120.0F, input);
    if (world->vehicle().speed >= accelerated.speed)
    {
        error = "original marauder brakeTorque did not reduce speed";
        return false;
    }
    const Quat before = world->vehicle().body.rotation;
    input = {};
    input.throttle = 0.7F;
    input.steering = 0.7F;
    for (int step = 0; step < 60; ++step)
        world->step(1.0F / 120.0F, input);
    const Quat after = world->vehicle().body.rotation;
    const float rotationDelta = std::abs(before.x - after.x) +
                                std::abs(before.y - after.y) +
                                std::abs(before.z - after.z) +
                                std::abs(before.w - after.w);
    const float beforeYaw = quaternionToEulerXYZ(before).z;
    const float afterYaw = quaternionToEulerXYZ(after).z;
    const float yawDelta = std::atan2(
        std::sin(afterYaw - beforeYaw),
        std::cos(afterYaw - beforeYaw));
    // GameCar::swOnLeft increases the source steering angle and rotates the
    // +X car-forward vector toward +Y. Preserve that sign through the
    // Z-up-to-Y-up Jolt conversion so player, AI, and wheel visuals agree.
    if (rotationDelta < 0.01F || yawDelta <= 0.01F)
    {
        error = "positive original steering did not turn the vehicle left";
        return false;
    }
    world->reset();
    if (world->vehicle().resetCount < 2)
    {
        error = "vehicle reset did not restore the original trace start";
        return false;
    }
    if (world->vehicle().gear != -1 ||
        std::any_of(
            world->vehicle().wheelAngularSpeeds.begin(),
            world->vehicle().wheelAngularSpeeds.end(),
            [](float speed) { return std::abs(speed) > 0.001F; }))
    {
        error = "vehicle reset did not clear source gear/wheel state";
        return false;
    }
    input = {};
    for (int step = 0; step < 240; ++step)
        world->step(1.0F / 120.0F, input);
    const float reverseStart = world->vehicle().body.position.x;
    input.reverse = 1.0F;
    for (int step = 0; step < 360; ++step)
        world->step(1.0F / 120.0F, input);
    const float reverseEnd = world->vehicle().body.position.x;
    if (world->vehicle().gear != 0 ||
        reverseEnd >= reverseStart - 0.25F)
    {
        error = "source mcBack/reverse gear did not move the vehicle "
                "backwards";
        return false;
    }
    input = {};
    input.throttle = 1.0F;
    for (int step = 0; step < 480; ++step)
        world->step(1.0F / 120.0F, input);
    if (world->vehicle().gear <= 0 ||
        world->vehicle().body.position.x <= reverseEnd + 0.25F)
    {
        error = "mcAccel did not brake reverse motion and re-engage first "
                "gear";
        return false;
    }

    WorldDescription airborneDescription = drivetrainDescription;
    airborneDescription.startPosition = {0.0F, 0.0F, 20.0F};
    airborneDescription.spawns.front().position =
        airborneDescription.startPosition;
    for (auto& vertex :
         airborneDescription.collisionMeshes.front().vertices)
        vertex.z = -100.0F;
    auto airborneWorld =
        createOriginalVehicleWorld(airborneDescription, error);
    if (!airborneWorld)
        return false;
    airborneWorld->addLinearVelocity(0U, {5.0F, 0.0F, 0.0F});
    const Quat airborneBefore = airborneWorld->vehicle().body.rotation;
    input = {};
    for (int step = 0; step < 30; ++step)
        airborneWorld->step(1.0F / 120.0F, input);
    const auto airborne = airborneWorld->vehicle();
    const float airborneRotation =
        std::abs(airborneBefore.x - airborne.body.rotation.x) +
        std::abs(airborneBefore.y - airborne.body.rotation.y) +
        std::abs(airborneBefore.z - airborne.body.rotation.z) +
        std::abs(airborneBefore.w - airborne.body.rotation.w);
    if (airborne.linearVelocity.z > -8.0F ||
        (selectedVehicle.airbornePitchAcceleration != 0.0F &&
         airborneRotation < 0.005F))
    {
        error = "source JumpProgress extra gravity/pitch acceleration was "
                "not preserved";
        return false;
    }

    auto springLockedWorld =
        createOriginalVehicleWorld(airborneDescription, error);
    if (!springLockedWorld)
        return false;
    springLockedWorld->addLinearVelocity(
        0U, {5.0F, 0.0F, 0.0F});
    const Quat springLockedBefore =
        springLockedWorld->vehicle().body.rotation;
    input = {};
    input.springLocked = true;
    for (int step = 0; step < 30; ++step)
        springLockedWorld->step(1.0F / 120.0F, input);
    const Quat springLockedAfter =
        springLockedWorld->vehicle().body.rotation;
    const float springLockedRotation =
        std::abs(
            springLockedBefore.x - springLockedAfter.x) +
        std::abs(
            springLockedBefore.y - springLockedAfter.y) +
        std::abs(
            springLockedBefore.z - springLockedAfter.z) +
        std::abs(
            springLockedBefore.w - springLockedAfter.w);
    if (selectedVehicle.airbornePitchAcceleration != 0.0F &&
        springLockedRotation >= 0.005F)
    {
        error = "source GameCar::LockSpring did not suppress airborne "
                "pitch acceleration";
        return false;
    }

    if (description.spawns.size() > 1U)
    {
        WorldDescription matrixDescription = drivetrainDescription;
        matrixDescription.spawns.clear();
        for (std::size_t index = 0; index < description.spawns.size();
             ++index)
        {
            auto spawn = description.spawns[index];
            spawn.position = {0.0F, static_cast<float>(index) * 15.0F,
                              2.0F};
            spawn.direction = {1.0F, 0.0F, 0.0F};
            matrixDescription.spawns.push_back(std::move(spawn));
        }
        matrixDescription.vehicle =
            matrixDescription.spawns.front().vehicle;
        auto matrixWorld =
            createOriginalVehicleWorld(matrixDescription, error);
        if (!matrixWorld)
            return false;
        std::vector<VehicleInput> matrixInputs(
            matrixWorld->vehicleCount());
        for (int step = 0; step < 240; ++step)
            matrixWorld->step(1.0F / 120.0F, matrixInputs);
        for (auto& matrixInput : matrixInputs)
            matrixInput.throttle = 1.0F;
        for (int step = 0; step < 120; ++step)
            matrixWorld->step(1.0F / 120.0F, matrixInputs);
        for (std::size_t index = 0;
             index < matrixWorld->vehicleCount(); ++index)
        {
            const auto& state = matrixWorld->vehicle(index);
            if (state.gear <= 0 || state.contactCount == 0U ||
                state.speed < 0.1F)
            {
                error = "original multi-car drivetrain matrix failed at "
                        "vehicle " + std::to_string(index);
                return false;
            }
        }
    }

    WorldDescription contactDescription;
    contactDescription.vehicle =
        description.spawns.empty() ? description.vehicle
                                   : description.spawns.front().vehicle;
    contactDescription.startPosition = {0.0F, 0.0F, 2.0F};
    contactDescription.startDirection = {1.0F, 0.0F, 0.0F};
    contactDescription.gravity = description.gravity;
    contactDescription.spawns.push_back(
        {contactDescription.vehicle, contactDescription.startPosition,
         contactDescription.startDirection});
    TriangleMesh floor;
    floor.surface = CollisionSurface::TrackPlane;
    floor.vertices = {{-30.0F, -30.0F, 0.0F},
                      {30.0F, -30.0F, 0.0F},
                      {30.0F, 30.0F, 0.0F},
                      {-30.0F, 30.0F, 0.0F}};
    floor.indices = {0U, 1U, 2U, 0U, 2U, 3U};
    contactDescription.collisionMeshes.push_back(std::move(floor));
    TriangleMesh border;
    border.surface = CollisionSurface::TrackBorder;
    border.vertices = {{4.0F, -8.0F, 0.0F},
                       {4.0F, 8.0F, 5.0F},
                       {4.0F, 8.0F, 0.0F},
                       {4.0F, -8.0F, 5.0F}};
    border.indices = {0U, 1U, 2U, 0U, 3U, 1U};
    contactDescription.collisionMeshes.push_back(std::move(border));
    auto contactWorld =
        createOriginalVehicleWorld(contactDescription, error);
    if (!contactWorld)
        return false;
    input = {};
    input.throttle = 1.0F;
    bool sawBorderContact = false;
    for (int step = 0; step < 1200 && !sawBorderContact; ++step)
    {
        contactWorld->step(1.0F / 120.0F, input);
        sawBorderContact = std::any_of(
            contactWorld->vehicle().bodyContacts.begin(),
            contactWorld->vehicle().bodyContacts.end(),
            [](const BodyContact& contact) {
                return contact.surface ==
                           CollisionSurface::TrackBorder &&
                       contact.normalSpeed > 0.0F &&
                       contact.force > 0.0F;
            });
    }
    if (!sawBorderContact)
    {
        error =
            "Jolt body contact listener did not preserve track-border "
            "surface metadata";
        return false;
    }
    DebrisDescription debrisDescription;
    debrisDescription.transform.position = {0.0F, -10.0F, 4.0F};
    debrisDescription.halfExtents = {0.5F, 0.5F, 0.5F};
    debrisDescription.mass = 200.0F;
    const std::size_t debrisIndex =
        contactWorld->addDebris(debrisDescription);
    if (debrisIndex == std::numeric_limits<std::size_t>::max() ||
        contactWorld->debrisCount() != 1U ||
        !contactWorld->debris(debrisIndex).active)
    {
        error = "source gotDestrObj dynamic body was not created";
        return false;
    }
    const float debrisStartHeight =
        contactWorld->debris(debrisIndex).body.position.z;
    input = {};
    for (int step = 0; step < 120; ++step)
        contactWorld->step(1.0F / 120.0F, input);
    if (contactWorld->debris(debrisIndex).body.position.z >=
        debrisStartHeight - 0.1F)
    {
        error = "source gotDestrObj dynamic body did not enter Jolt physics";
        return false;
    }
    DebrisDescription wreckDescription;
    wreckDescription.transform.position = {0.0F, -15.0F, 4.0F};
    wreckDescription.halfExtents = {1.46261F, 0.745738F, 0.534895F};
    wreckDescription.localImpulse = {12000.0F, 0.0F, 0.0F};
    wreckDescription.mass = 1200.0F;
    wreckDescription.lifetime = 0.25F;
    const std::size_t wreckIndex = contactWorld->addDebris(wreckDescription);
    if (wreckIndex == std::numeric_limits<std::size_t>::max() ||
        !contactWorld->debris(wreckIndex).active)
    {
        error = "source vehicle wreck dynamic body was not created";
        return false;
    }
    const float wreckStartX =
        contactWorld->debris(wreckIndex).body.position.x;
    contactWorld->step(0.1F, input);
    if (!contactWorld->debris(wreckIndex).active ||
        contactWorld->debris(wreckIndex).body.position.x <=
            wreckStartX + 0.01F)
    {
        error = "source vehicle wreck impulse was not applied";
        return false;
    }
    contactWorld->step(0.1F, input);
    contactWorld->step(0.1F, input);
    if (contactWorld->debris(wreckIndex).active)
    {
        error = "source vehicle wreck maxTimeLife was not applied";
        return false;
    }
    contactWorld->setVehicleEnabled(0U, false);
    contactWorld->step(0.1F, input);
    if (contactWorld->vehicle().speed != 0.0F ||
        contactWorld->vehicle().contactCount != 0U)
    {
        error = "destroyed source vehicle remained in Jolt physics";
        return false;
    }
    const auto resetBeforeRestore = contactWorld->vehicle().resetCount;
    contactWorld->resetVehicle(
        0U, contactDescription.spawns.front().position,
        contactDescription.spawns.front().direction);
    if (contactWorld->vehicle().resetCount != resetBeforeRestore + 1U)
    {
        error = "source vehicle restore did not re-enable Jolt body";
        return false;
    }
    contactWorld->reset();
    if (contactWorld->debrisCount() != 0U)
    {
        error = "source gotDestrObj dynamic bodies survived race reset";
        return false;
    }
    error.clear();
    return true;
}

} // namespace r3d::physics
