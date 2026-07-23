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
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <atomic>
#include <cmath>
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
        system_.SetGravity(toJolt({0.0F, 0.0F, description_.gravity}));
        createTrack();
        vehicles_.reserve(description_.spawns.size());
        for (const auto& spawn : description_.spawns)
            createVehicle(spawn);
        system_.OptimizeBroadPhase();
        reset();
    }

    ~JoltVehicleWorld() override
    {
        for (auto& vehicle : vehicles_)
        {
            if (vehicle.constraint != nullptr)
            {
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
                bodies.RemoveBody(vehicle.body);
                bodies.DestroyBody(vehicle.body);
            }
        }
        if (!trackBody_.IsInvalid())
        {
            bodies.RemoveBody(trackBody_);
            bodies.DestroyBody(trackBody_);
        }
    }

    void reset() noexcept override
    {
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
        ++vehicle.resetCount;
        updateState(vehicle);
    }

    void step(float seconds, const VehicleInput& input) noexcept override
    {
        std::vector<VehicleInput> inputs(vehicles_.size());
        if (!inputs.empty())
            inputs.front() = input;
        step(seconds, inputs);
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
            vehicle.controller->SetDriverInput(
                input.throttle, input.steering, input.brake, 0.0F);
            if (input.throttle != 0.0F || input.brake != 0.0F ||
                input.steering != 0.0F)
                system_.GetBodyInterface().ActivateBody(vehicle.body);
        }

        float remaining = std::clamp(seconds, 0.0F, 0.25F);
        constexpr float fixedStep = 1.0F / 120.0F;
        while (remaining > 0.0F)
        {
            const float delta = std::min(remaining, fixedStep);
            system_.Update(delta, 1, &tempAllocator_, &jobs_);
            remaining -= delta;
        }
        for (auto& vehicle : vehicles_)
            updateState(vehicle);
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

private:
    struct VehicleRuntime
    {
        VehicleSpawn spawn;
        JPH::BodyID body;
        JPH::Ref<JPH::VehicleConstraint> constraint;
        JPH::WheeledVehicleController* controller = nullptr;
        VehicleState state;
        std::uint32_t resetCount = 0;
    };

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
        JPH::TriangleList triangles;
        std::size_t indexCount = 0;
        for (const auto& mesh : description_.collisionMeshes)
            indexCount += mesh.indices.size();
        triangles.reserve(indexCount / 3U);
        for (const auto& mesh : description_.collisionMeshes)
        {
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
                triangles.emplace_back(toJolt(a), toJolt(c), toJolt(b));
            }
        }
        JPH::MeshShapeSettings shapeSettings(triangles);
        const auto shapeResult = shapeSettings.Create();
        if (shapeResult.HasError())
            throw std::runtime_error(
                ("Jolt track mesh: " + shapeResult.GetError()).c_str());
        JPH::BodyCreationSettings settings(
            shapeResult.Get(), JPH::RVec3::sZero(), JPH::Quat::sIdentity(),
            JPH::EMotionType::Static, Layers::nonMoving);
        settings.mFriction = 0.5F;
        settings.mRestitution = 0.5F;
        trackBody_ = system_.GetBodyInterface().CreateAndAddBody(
            settings, JPH::EActivation::DontActivate);
        if (trackBody_.IsInvalid())
            throw std::runtime_error("Jolt could not create track body");
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
            settings.mWheels.push_back(wheel);
        }

        auto* controllerSettings = new JPH::WheeledVehicleControllerSettings;
        controllerSettings->mEngine.mMaxTorque = source.maximumTorque;
        controllerSettings->mEngine.mMaxRPM = source.maximumRpm;
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
    }

    void updateState(VehicleRuntime& vehicle) noexcept
    {
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
        state.wheels.clear();
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
            if (vehicle.constraint->GetWheel(index)->HasContact())
                ++contacts;
        }
        state.contactCount = contacts;
    }

    WorldDescription description_;
    BroadPhaseLayerInterface broadPhaseInterface_;
    ObjectVsBroadPhaseFilter objectVsBroadPhase_;
    ObjectLayerPairFilter objectLayerPairs_;
    JPH::PhysicsSystem system_;
    JPH::TempAllocatorImpl tempAllocator_;
    JPH::JobSystemThreadPool jobs_;
    JPH::BodyID trackBody_;
    std::vector<VehicleRuntime> vehicles_;
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
    if (settled.contactCount == 0)
    {
        error = "original vehicle wheels did not contact map1 collision mesh; "
                "position=" + std::to_string(settled.body.position.x) + "," +
                std::to_string(settled.body.position.y) + "," +
                std::to_string(settled.body.position.z);
        return false;
    }
    input.throttle = 1.0F;
    for (int step = 0; step < 480; ++step)
        world->step(1.0F / 120.0F, input);
    const auto accelerated = world->vehicle();
    if (accelerated.speed < 1.0F || accelerated.engineRpm <= 0.0F)
    {
        error = "original marauder engine did not accelerate on map1";
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
    error.clear();
    return true;
}

} // namespace r3d::physics
