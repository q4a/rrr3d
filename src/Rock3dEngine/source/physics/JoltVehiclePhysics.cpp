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
                        JPH::ContactSettings&) override
    {
        record(first, second, manifold);
    }

    void OnContactPersisted(const JPH::Body& first,
                            const JPH::Body& second,
                            const JPH::ContactManifold& manifold,
                            JPH::ContactSettings&) override
    {
        record(first, second, manifold);
    }

private:
    void recordOne(std::size_t vehicle, const JPH::Body& body,
                   const JPH::Body& other, JPH::Vec3Arg outwardNormal)
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
        const float force =
            inverseMass > 0.0F
                ? normalSpeed / (inverseMass * originalContactStep)
                : 0.0F;

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
                const JPH::ContactManifold& manifold)
    {
        std::size_t firstVehicle = 0;
        std::size_t secondVehicle = 0;
        if (vehicleIndex(first.GetUserData(), firstVehicle))
            recordOne(firstVehicle, first, second,
                      -manifold.mWorldSpaceNormal);
        if (vehicleIndex(second.GetUserData(), secondVehicle))
            recordOne(secondVehicle, second, first,
                      manifold.mWorldSpaceNormal);
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
        vehicle.controller->SetDriverInput(0.0F, 0.0F, 1.0F, 0.0F);
        vehicle.wheelTractionEnabled = true;
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
            toJolt(delta));
        bodies.ActivateBody(vehicles_[index].body);
    }

    void setWheelTractionEnabled(std::size_t index,
                                 bool enabled) noexcept override
    {
        if (index >= vehicles_.size())
            return;
        vehicles_[index].wheelTractionEnabled = enabled;
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
        for (std::size_t index = 0; index < vehicles_.size(); ++index)
        {
            VehicleInput input;
            if (index < rawInputs.size())
                input = rawInputs[index];
            input.throttle = std::clamp(input.throttle, 0.0F, 1.0F);
            input.brake = std::clamp(input.brake, 0.0F, 1.0F);
            input.steering = std::clamp(input.steering, -1.0F, 1.0F);
            auto& vehicle = vehicles_[index];
            if (!vehicle.enabled)
                continue;
            if (vehicle.spawn.vehicle.maximumSpeed > 0.0F &&
                vehicle.state.speed >
                    vehicle.spawn.vehicle.maximumSpeed)
                input.throttle = 0.0F;
            vehicle.controller->SetDriverInput(
                input.throttle, input.steering, input.brake, 0.0F);
            if (input.throttle != 0.0F || input.brake != 0.0F ||
                input.steering != 0.0F)
                system_.GetBodyInterface().ActivateBody(vehicle.body);
        }

        const float simulationSeconds =
            std::clamp(seconds, 0.0F, 0.25F);
        float remaining = simulationSeconds;
        constexpr float fixedStep = 1.0F / 120.0F;
        contactListener_.beginStep();
        while (remaining > 0.0F)
        {
            const float delta = std::min(remaining, fixedStep);
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
                spawn.vehicle.wheels.size() != 4)
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
            settings.mFriction = 0.5F;
            settings.mRestitution = 0.5F;
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
        bodySettings.mFriction = 0.5F;
        bodySettings.mRestitution = 0.5F;
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
        controllerSettings->mEngine.mMinRPM = std::min(
            controllerSettings->mEngine.mMinRPM,
            controllerSettings->mEngine.mMaxRPM * 0.25F);
        // Jolt's fixed 4000/2000 automatic shift defaults assert when an
        // original opponent/upgrade has maxRPM <= 4000. Scale the shift
        // points to each db.xml motor, as the legacy controller did.
        controllerSettings->mTransmission.mShiftUpRPM =
            controllerSettings->mEngine.mMaxRPM * 0.86F;
        controllerSettings->mTransmission.mShiftDownRPM =
            controllerSettings->mEngine.mMaxRPM * 0.48F;
        std::vector<JPH::uint> driven;
        for (JPH::uint index = 0; index < source.wheels.size(); ++index)
            if (source.wheels[index].driven)
                driven.push_back(index);
        for (std::size_t index = 0; index + 1 < driven.size(); index += 2)
        {
            JPH::VehicleDifferentialSettings differential;
            differential.mLeftWheel = driven[index];
            differential.mRightWheel = driven[index + 1];
            differential.mDifferentialRatio = source.differentialRatio;
            differential.mEngineTorqueRatio =
                2.0F / static_cast<float>(driven.size());
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
        vehicles_.push_back(std::move(runtime));
        const std::size_t vehicleIndexValue = vehicles_.size() - 1U;
        vehicles_.back().constraint->SetCombineFriction(
            [this, vehicleIndexValue](JPH::uint, float& longitudinal,
                                      float& lateral,
                                      const JPH::Body& contactBody,
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
                const float bodyFriction = contactBody.GetFriction();
                longitudinal = std::sqrt(longitudinal * bodyFriction);
                lateral = std::sqrt(lateral * bodyFriction);
            });
    }

    void updateState(VehicleRuntime& vehicle) noexcept
    {
        if (!vehicle.enabled)
        {
            vehicle.state.linearVelocity = {};
            vehicle.state.speed = 0.0F;
            vehicle.state.engineRpm = 0.0F;
            vehicle.state.contactCount = 0U;
            vehicle.state.bodyContacts.clear();
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
        state.engineRpm =
            vehicle.controller->GetEngine().GetCurrentRPM();
        state.resetCount = vehicle.resetCount;
        std::size_t vehicleIndexValue =
            static_cast<std::size_t>(&vehicle - vehicles_.data());
        state.bodyContacts = contactListener_.take(vehicleIndexValue);
        state.wheels.clear();
        state.wheelContacts.clear();
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
    auto world = createOriginalVehicleWorld(description, error);
    if (!world)
        return false;
    VehicleInput input;
    for (int step = 0; step < 240; ++step)
        world->step(1.0F / 120.0F, input);
    const auto settled = world->vehicle();
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
        error = "original marauder engine did not accelerate on map1";
        return false;
    }
    if (accelerated.speed <= clutchLockedSpeed + 0.5F)
    {
        error = "source clutch lock did not suppress wheel traction";
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
    for (int step = 0; step < 360; ++step)
        world->step(1.0F / 120.0F, input);
    const Quat after = world->vehicle().body.rotation;
    const float rotationDelta = std::abs(before.x - after.x) +
                                std::abs(before.y - after.y) +
                                std::abs(before.z - after.z) +
                                std::abs(before.w - after.w);
    if (rotationDelta < 0.01F)
    {
        error = "original front-wheel steering did not rotate the vehicle";
        return false;
    }
    world->reset();
    if (world->vehicle().resetCount < 2)
    {
        error = "vehicle reset did not restore the original trace start";
        return false;
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
