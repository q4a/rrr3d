#include "physics/OriginalVehiclePhysics.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
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

Vec3 fromJoltAngular(JPH::Vec3Arg value)
{
    return {-value.GetX(), -value.GetZ(), -value.GetY()};
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

struct SourceBodyFriction
{
    JPH::Vec3 firstDirection = JPH::Vec3::sZero();
    float first = 0.0F;
    float second = 0.0F;
};

SourceBodyFriction sourceTrackBodyFriction(
    JPH::QuatArg carRotation, JPH::Vec3Arg contactNormal,
    CollisionSurface surface) noexcept
{
    // DataBase.cpp gives the car material an anisotropy direction of local
    // game +Z (Jolt +Y). GameCar::OnContactModify projects it onto the
    // contacted triangle. When that direction is almost the triangle normal,
    // it falls back to local game +Y (Jolt +Z).
    const JPH::Vec3 carUp =
        carRotation * JPH::Vec3::sAxisY();
    JPH::Vec3 first = carUp.Cross(contactNormal);
    JPH::Vec3 second = carUp;
    if (first.Length() < 0.5F)
    {
        first = carRotation * JPH::Vec3::sAxisZ();
        second = contactNormal.Cross(first);
    }
    if (second.Length() <= 0.1F || first.LengthSq() <= 1.0e-8F)
        first = contactNormal.GetNormalizedPerpendicular();
    else
        first = first.Normalized();
    return {
        first, 0.0F,
        surface == CollisionSurface::TrackBorder ? 4.0F : 0.1F};
}

constexpr JPH::uint64 bodyKindMask = 0xf000000000000000ULL;
constexpr JPH::uint64 vehicleBodyKind = 0x1000000000000000ULL;
constexpr JPH::uint64 surfaceBodyKind = 0x2000000000000000ULL;
constexpr JPH::uint64 decorationBodyKind = 0x3000000000000000ULL;
constexpr JPH::uint64 projectileBodyKind = 0x4000000000000000ULL;
constexpr JPH::uint64 projectileShotTrackMask = 0x0800000000000000ULL;

JPH::uint64 vehicleUserData(std::size_t index)
{
    return vehicleBodyKind | static_cast<JPH::uint64>(index);
}

JPH::uint64 surfaceUserData(CollisionSurface surface)
{
    return surfaceBodyKind | static_cast<JPH::uint64>(surface);
}

JPH::uint64 decorationUserData(std::size_t index)
{
    return decorationBodyKind | static_cast<JPH::uint64>(index);
}

JPH::uint64 projectileUserData(std::size_t index, bool shotTrack)
{
    return projectileBodyKind |
           (shotTrack ? projectileShotTrackMask : 0ULL) |
           static_cast<JPH::uint64>(index);
}

bool vehicleIndex(JPH::uint64 userData, std::size_t& index)
{
    if ((userData & bodyKindMask) != vehicleBodyKind)
        return false;
    index = static_cast<std::size_t>(userData & ~bodyKindMask);
    return true;
}

bool projectileIsShotTrack(JPH::uint64 userData)
{
    return (userData & bodyKindMask) == projectileBodyKind &&
           (userData & projectileShotTrackMask) != 0ULL;
}

bool projectileIndex(JPH::uint64 userData, std::size_t& index)
{
    if ((userData & bodyKindMask) != projectileBodyKind)
        return false;
    index = static_cast<std::size_t>(
        userData & ~(bodyKindMask | projectileShotTrackMask));
    return true;
}

bool decorationIndex(JPH::uint64 userData, std::size_t& index)
{
    if ((userData & bodyKindMask) != decorationBodyKind)
        return false;
    index = static_cast<std::size_t>(userData & ~bodyKindMask);
    return true;
}

CollisionSurface collisionSurface(JPH::uint64 userData)
{
    if ((userData & bodyKindMask) == decorationBodyKind)
        return CollisionSurface::Decoration;
    if ((userData & bodyKindMask) != surfaceBodyKind)
        return CollisionSurface::TrackPlane;
    const auto value = static_cast<std::uint8_t>(userData & ~bodyKindMask);
    return value <= static_cast<std::uint8_t>(CollisionSurface::DeathPlane)
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

    void resizeProjectiles(std::size_t count)
    {
        std::scoped_lock lock(mutex_);
        projectilePending_.resize(count);
    }

    void beginStep()
    {
        std::scoped_lock lock(mutex_);
        for (auto& contacts : pending_)
            contacts.clear();
        for (auto& contacts : projectilePending_)
            contacts.clear();
    }

    std::vector<BodyContact> take(std::size_t index)
    {
        std::scoped_lock lock(mutex_);
        if (index >= pending_.size())
            return {};
        return std::move(pending_[index]);
    }

    std::vector<BodyContact> takeProjectile(std::size_t index)
    {
        std::scoped_lock lock(mutex_);
        if (index >= projectilePending_.size())
            return {};
        return std::move(projectilePending_[index]);
    }

    void OnContactAdded(const JPH::Body& first, const JPH::Body& second,
                        const JPH::ContactManifold& manifold,
                        JPH::ContactSettings& settings) override
    {
        configureMaterial(first, second, manifold, settings);
        record(first, second, manifold, settings);
    }

    void OnContactPersisted(const JPH::Body& first,
                            const JPH::Body& second,
                            const JPH::ContactManifold& manifold,
                            JPH::ContactSettings& settings) override
    {
        configureMaterial(first, second, manifold, settings);
        record(first, second, manifold, settings);
    }

private:
    static void configureMaterial(
        const JPH::Body& first, const JPH::Body& second,
        const JPH::ContactManifold& manifold,
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
        const JPH::Body& car = firstIsVehicle ? first : second;
        const JPH::Body& other = firstIsVehicle ? second : first;
        const auto surface = collisionSurface(other.GetUserData());
        const bool sourceTrackActor =
            (other.GetUserData() & bodyKindMask) == surfaceBodyKind;
        if (sourceTrackActor &&
            (surface == CollisionSurface::TrackPlane ||
             surface == CollisionSurface::TrackBorder))
        {
            const auto source = sourceTrackBodyFriction(
                car.GetRotation(), manifold.mWorldSpaceNormal, surface);
            settings.mCombinedFriction = source.first;
            settings.mCombinedFriction2 = source.second;
            settings.mFrictionDirection1 = source.firstDirection;
        }
        else
        {
            // Car materials use NX_CM_MIN (0.08 or the 0.02 wake model).
            settings.mCombinedFriction =
                std::min(first.GetFriction(), second.GetFriction());
            settings.mCombinedFriction2 = -1.0F;
            settings.mFrictionDirection1 = JPH::Vec3::sZero();
        }
    }

    void recordOne(std::size_t vehicle, const JPH::Body& body,
                   const JPH::Body& other, JPH::Vec3Arg outwardNormal,
                   float estimatedForce, float estimatedFrictionForce,
                   JPH::Vec3Arg estimatedFrictionForceVector,
                   const JPH::ContactManifold& manifold)
    {
        const JPH::Vec3 bodyVelocity = body.GetLinearVelocity();
        std::size_t otherVehicle = std::numeric_limits<std::size_t>::max();
        const bool otherIsVehicle =
            vehicleIndex(other.GetUserData(), otherVehicle);
        std::size_t otherDecoration =
            std::numeric_limits<std::size_t>::max();
        decorationIndex(other.GetUserData(), otherDecoration);
        const bool otherIsMoving =
            other.GetMotionType() != JPH::EMotionType::Static;
        const JPH::Vec3 otherVelocity =
            otherIsMoving ? other.GetLinearVelocity()
                          : JPH::Vec3::sZero();
        const float normalSpeed = std::max(
            0.0F, -(bodyVelocity - otherVelocity).Dot(outwardNormal));
        const float bodyInverseMass =
            body.GetMotionProperties()->GetInverseMass();
        const float otherInverseMass =
            otherIsMoving
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
        contact.otherDecoration = otherDecoration;
        contact.normal = fromJolt(outwardNormal);
        contact.normalSpeed = normalSpeed;
        contact.force = force;
        contact.otherActor =
            other.GetID().GetIndexAndSequenceNumber();
        contact.frictionForce = estimatedFrictionForce;
        contact.frictionForceVector =
            fromJolt(estimatedFrictionForceVector);
        const auto pointCount = std::min<std::size_t>(
            {manifold.mRelativeContactPointsOn1.size(),
             manifold.mRelativeContactPointsOn2.size(), 2U});
        contact.points.reserve(pointCount);
        for (std::size_t index = 0; index < pointCount; ++index)
        {
            const JPH::RVec3 firstPoint =
                manifold.GetWorldSpaceContactPointOn1(
                    static_cast<JPH::uint>(index));
            const JPH::RVec3 secondPoint =
                manifold.GetWorldSpaceContactPointOn2(
                    static_cast<JPH::uint>(index));
            contact.points.push_back(fromJolt(JPH::Vec3(
                firstPoint + 0.5F * (secondPoint - firstPoint))));
        }
        if (!contact.points.empty())
        {
            contact.point = contact.points.front();
            contact.hasPoint = true;
        }

        std::scoped_lock lock(mutex_);
        if (vehicle >= pending_.size())
            return;
        auto& contacts = pending_[vehicle];
        const auto found = std::find_if(
            contacts.begin(), contacts.end(),
            [&](const BodyContact& value) {
                return value.surface == contact.surface &&
                       value.otherVehicle == contact.otherVehicle &&
                       value.otherDecoration == contact.otherDecoration &&
                       value.otherActor == contact.otherActor;
            });
        if (found == contacts.end())
            contacts.push_back(contact);
        else
        {
            if (contact.force > found->force)
            {
                found->normal = contact.normal;
                found->normalSpeed = contact.normalSpeed;
                found->force = contact.force;
            }
            if (contact.frictionForce > found->frictionForce)
            {
                found->frictionForce = contact.frictionForce;
                found->frictionForceVector =
                    contact.frictionForceVector;
                found->point = contact.point;
                found->points = std::move(contact.points);
                found->hasPoint = contact.hasPoint;
            }
        }
    }

    void record(const JPH::Body& first, const JPH::Body& second,
                const JPH::ContactManifold& manifold,
                const JPH::ContactSettings& settings)
    {
        JPH::CollisionEstimationResult estimation;
        JPH::EstimateCollisionResponse(
            first, second, manifold, estimation,
            std::max(
                settings.mCombinedFriction,
                settings.mCombinedFriction2),
            settings.mCombinedRestitution, 1.0F, 4U);
        float estimatedForce = 0.0F;
        float frictionImpulse1 = 0.0F;
        float frictionImpulse2 = 0.0F;
        constexpr float originalContactStep = 1.0F / 120.0F;
        for (const auto& impulse : estimation.mImpulses)
        {
            estimatedForce +=
                std::abs(impulse.mContactImpulse) / originalContactStep;
            frictionImpulse1 += impulse.mFrictionImpulse1;
            frictionImpulse2 += impulse.mFrictionImpulse2;
        }
        const float estimatedFrictionForce =
            std::sqrt(
                frictionImpulse1 * frictionImpulse1 +
                frictionImpulse2 * frictionImpulse2) /
            originalContactStep;
        // EstimateCollisionResponse stores the impulse applied against
        // body 1 along its two contact tangents.  Body 2 receives the
        // opposite impulse.  PhysX exposed the corresponding vector as
        // sumFrictionForce and GameCar consumes its Z sign at borders.
        const JPH::Vec3 estimatedFrictionForceVector =
            -(frictionImpulse1 * estimation.mTangent1 +
              frictionImpulse2 * estimation.mTangent2) /
            originalContactStep;
        std::size_t firstVehicle = 0;
        std::size_t secondVehicle = 0;
        const bool firstIsProjectile =
            (first.GetUserData() & bodyKindMask) == projectileBodyKind;
        const bool secondIsProjectile =
            (second.GetUserData() & bodyKindMask) == projectileBodyKind;
        if (vehicleIndex(first.GetUserData(), firstVehicle) &&
            !secondIsProjectile)
        {
            recordOne(firstVehicle, first, second,
                      -manifold.mWorldSpaceNormal, estimatedForce,
                      estimatedFrictionForce,
                      estimatedFrictionForceVector, manifold);
        }
        if (vehicleIndex(second.GetUserData(), secondVehicle) &&
            !firstIsProjectile)
        {
            recordOne(secondVehicle, second, first,
                      manifold.mWorldSpaceNormal, estimatedForce,
                      estimatedFrictionForce,
                      -estimatedFrictionForceVector, manifold);
        }
        std::size_t firstProjectile = 0;
        std::size_t secondProjectile = 0;
        if (projectileIndex(first.GetUserData(), firstProjectile))
        {
            recordProjectile(
                firstProjectile, first, second,
                -manifold.mWorldSpaceNormal, manifold);
        }
        if (projectileIndex(second.GetUserData(), secondProjectile))
        {
            recordProjectile(
                secondProjectile, second, first,
                manifold.mWorldSpaceNormal, manifold);
        }
    }

    void recordProjectile(
        std::size_t projectile, const JPH::Body& body,
        const JPH::Body& other, JPH::Vec3Arg outwardNormal,
        const JPH::ContactManifold& manifold)
    {
        const auto otherKind = other.GetUserData() & bodyKindMask;
        std::size_t otherVehicle =
            std::numeric_limits<std::size_t>::max();
        std::size_t otherDecoration =
            std::numeric_limits<std::size_t>::max();
        if (otherKind == vehicleBodyKind)
            vehicleIndex(other.GetUserData(), otherVehicle);
        else if (otherKind == decorationBodyKind)
            decorationIndex(other.GetUserData(), otherDecoration);
        else if (otherKind != surfaceBodyKind)
            return;

        BodyContact contact;
        contact.surface = otherKind == vehicleBodyKind
            ? CollisionSurface::Vehicle
            : collisionSurface(other.GetUserData());
        contact.otherVehicle = otherVehicle;
        contact.otherDecoration = otherDecoration;
        contact.otherActor =
            other.GetID().GetIndexAndSequenceNumber();
        contact.normal = fromJolt(outwardNormal);
        const JPH::Vec3 otherVelocity =
            other.GetMotionType() == JPH::EMotionType::Static
                ? JPH::Vec3::sZero()
                : other.GetLinearVelocity();
        contact.normalSpeed = std::max(
            0.0F,
            -(body.GetLinearVelocity() - otherVelocity)
                 .Dot(outwardNormal));
        const auto pointCount = std::min<std::size_t>(
            {manifold.mRelativeContactPointsOn1.size(),
             manifold.mRelativeContactPointsOn2.size(), 2U});
        contact.points.reserve(pointCount);
        for (std::size_t index = 0; index < pointCount; ++index)
        {
            const auto firstPoint =
                manifold.GetWorldSpaceContactPointOn1(
                    static_cast<JPH::uint>(index));
            const auto secondPoint =
                manifold.GetWorldSpaceContactPointOn2(
                    static_cast<JPH::uint>(index));
            contact.points.push_back(fromJolt(JPH::Vec3(
                firstPoint + 0.5F * (secondPoint - firstPoint))));
        }
        if (!contact.points.empty())
        {
            contact.point = contact.points.front();
            contact.hasPoint = true;
        }

        std::scoped_lock lock(mutex_);
        if (projectile >= projectilePending_.size())
            return;
        auto& contacts = projectilePending_[projectile];
        const auto found = std::find_if(
            contacts.begin(), contacts.end(),
            [&](const BodyContact& value) {
                return value.surface == contact.surface &&
                       value.otherVehicle == contact.otherVehicle &&
                       value.otherDecoration == contact.otherDecoration &&
                       value.otherActor == contact.otherActor;
            });
        if (found == contacts.end())
            contacts.push_back(std::move(contact));
        else if (contact.normalSpeed > found->normalSpeed)
            *found = std::move(contact);
    }

    std::mutex mutex_;
    std::vector<std::vector<BodyContact>> pending_;
    std::vector<std::vector<BodyContact>> projectilePending_;
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

struct SourceWheelNormalForce
{
    float reaction = 0.0F;
    float impulse = 0.0F;
};

SourceWheelNormalForce sourceWheelNormalForce(
    float suspensionImpulse, float normalReactionImpulse,
    bool contactForceEnabled, float tireSpring) noexcept
{
    SourceWheelNormalForce result;
    suspensionImpulse = std::max(suspensionImpulse, 0.0F);
    normalReactionImpulse = std::max(normalReactionImpulse, 0.0F);
    result.impulse = suspensionImpulse;
    if (normalReactionImpulse > 1.0e-6F)
    {
        result.reaction = suspensionImpulse / normalReactionImpulse;
        if (!contactForceEnabled ||
            (tireSpring > 0.0F && result.reaction > tireSpring))
        {
            result.impulse = 0.0F;
        }
        else
        {
            result.impulse = std::min(
                suspensionImpulse, normalReactionImpulse * 1.5F);
        }
    }
    else if (!contactForceEnabled)
    {
        result.impulse = 0.0F;
    }
    return result;
}

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
        decorations_.resize(description_.decorations.size());
        for (std::size_t index = 0; index < decorations_.size(); ++index)
        {
            decorations_[index].state.body =
                description_.decorations[index].transform;
            decorations_[index].state.active = true;
        }
        createTrack();
        createDecorationBodies();
        vehicles_.reserve(description_.spawns.size());
        for (const auto& spawn : description_.spawns)
            createVehicle(spawn);
        system_.OptimizeBroadPhase();
        reset();
    }

    ~JoltVehicleWorld() override
    {
        system_.SetContactListener(nullptr);
        clearProjectileBodies();
        clearDebris();
        destroyDecorations();
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
        sourceFixedStepAccumulator_ = 0.0F;
        sourceFixedInputs_.clear();
        clearProjectileBodies();
        clearDebris();
        auto& bodies = system_.GetBodyInterface();
        for (std::size_t index = 0; index < decorations_.size(); ++index)
        {
            setDecorationEnabled(index, true);
            auto& decoration = decorations_[index];
            if (index >= description_.decorations.size())
                continue;
            const auto& source = description_.decorations[index];
            decoration.state.body = source.transform;
            if (decoration.shapeBody.IsInvalid())
                continue;
            bodies.SetPositionAndRotation(
                decoration.shapeBody, toJolt(source.transform.position),
                toJolt(source.transform.rotation),
                source.dynamic ? JPH::EActivation::Activate
                               : JPH::EActivation::DontActivate);
            if (source.dynamic)
            {
                bodies.SetLinearAndAngularVelocity(
                    decoration.shapeBody, JPH::Vec3::sZero(),
                    JPH::Vec3::sZero());
            }
        }
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
        vehicle.wheelNormalReactions.assign(
            vehicle.constraint->GetWheels().size(), 0.0F);
        vehicle.wheelNormalImpulses.assign(
            vehicle.constraint->GetWheels().size(), 0.0F);
        vehicle.wheelTireReleaseConsumed.assign(
            vehicle.constraint->GetWheels().size(), false);
        vehicle.wheelTireReleaseActive.assign(
            vehicle.constraint->GetWheels().size(), false);
        vehicle.currentGear = -1;
        vehicle.motorTorque = 0.0F;
        vehicle.engineRpm = vehicle.spawn.vehicle.idlingRpm;
        vehicle.sourceDriveCommand = {};
        vehicle.sourceDriveCommand.brakeTorque =
            vehicle.spawn.vehicle.restBrakeTorque;
        vehicle.sourceDriveCommand.engineRpm =
            vehicle.spawn.vehicle.idlingRpm;
        vehicle.sourceDriveCommand.gear = -1;
        vehicle.sourceDriveCommand.lateralGripScale = 1.0F;
        vehicle.sourceDriveCommand.angularDamping = {
            vehicle.spawn.vehicle.angularDamping.x,
            vehicle.spawn.vehicle.angularDamping.y,
            vehicle.spawn.vehicle.angularDamping.z};
        vehicle.sourceDriveCommand.clampRollAngle =
            vehicle.spawn.vehicle.clampRollAngle;
        vehicle.sourceDriveCommand.clampPitchAngle =
            vehicle.spawn.vehicle.clampPitchAngle;
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

    void setAngularMomentum(std::size_t index,
                            Vec3 momentum) noexcept override
    {
        if (index >= vehicles_.size() || !vehicles_[index].enabled)
            return;
        auto& vehicle = vehicles_[index];
        JPH::BodyLockRead lock(
            system_.GetBodyLockInterface(), vehicle.body);
        if (!lock.Succeeded())
            return;
        const auto angularVelocity =
            lock.GetBody().GetInverseInertia().Multiply3x3(
                toJoltAngular(momentum));
        lock.ReleaseLock();
        auto& bodies = system_.GetBodyInterface();
        bodies.SetAngularVelocity(vehicle.body, angularVelocity);
        bodies.ActivateBody(vehicle.body);
        updateState(vehicle);
    }

    void synchronizeNetworkVehicle(
        std::size_t index, Vec3 position, Quat rotation,
        Vec3 linearMomentum, Vec3 angularMomentum,
        bool graphRotationRequiresSnap) noexcept override
    {
        if (index >= vehicles_.size() || !vehicles_[index].enabled)
            return;
        auto& vehicle = vehicles_[index];
        JPH::BodyLockRead lock(
            system_.GetBodyLockInterface(), vehicle.body);
        if (!lock.Succeeded())
            return;
        const JPH::Body& body = lock.GetBody();
        const Vec3 currentPosition = fromJolt(body.GetPosition());
        const auto currentRotation = body.GetRotation();
        const auto targetRotation = toJolt(rotation);
        const float quaternionDot = std::abs(
            currentRotation.GetX() * targetRotation.GetX() +
            currentRotation.GetY() * targetRotation.GetY() +
            currentRotation.GetZ() * targetRotation.GetZ() +
            currentRotation.GetW() * targetRotation.GetW());
        const float rotationDifference =
            2.0F * std::acos(std::clamp(quaternionDot, 0.0F, 1.0F));
        lock.ReleaseLock();

        auto& bodies = system_.GetBodyInterface();
        const Vec3 positionDifference{
            position.x - currentPosition.x,
            position.y - currentPosition.y,
            position.z - currentPosition.z};
        const float positionDistance = std::sqrt(
            positionDifference.x * positionDifference.x +
            positionDifference.y * positionDifference.y +
            positionDifference.z * positionDifference.z);
        if (positionDistance > 4.0F)
        {
            bodies.SetPosition(
                vehicle.body, toJolt(position),
                JPH::EActivation::Activate);
        }
        else if (positionDistance > 0.1F)
        {
            const float correction =
                2.0F * vehicle.spawn.vehicle.mass;
            linearMomentum.x += positionDifference.x * correction;
            linearMomentum.y += positionDifference.y * correction;
            linearMomentum.z += positionDifference.z * correction;
        }
        if (graphRotationRequiresSnap || rotationDifference >
            3.14159265358979323846F / 24.0F)
        {
            bodies.SetRotation(
                vehicle.body, targetRotation,
                JPH::EActivation::Activate);
        }

        JPH::BodyLockRead momentumLock(
            system_.GetBodyLockInterface(), vehicle.body);
        if (!momentumLock.Succeeded())
            return;
        const auto angularVelocity =
            momentumLock.GetBody().GetInverseInertia().Multiply3x3(
                toJoltAngular(angularMomentum));
        momentumLock.ReleaseLock();
        bodies.SetLinearAndAngularVelocity(
            vehicle.body,
            toJolt(linearMomentum) /
                std::max(vehicle.spawn.vehicle.mass, 0.001F),
            angularVelocity);
        bodies.ActivateBody(vehicle.body);
        updateState(vehicle);
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

    void setVehicleFixedStepController(
        VehicleFixedStepController controller) override
    {
        fixedStepController_ = std::move(controller);
    }

    void setWorldFixedStepController(
        WorldFixedStepController controller) override
    {
        worldFixedStepController_ = std::move(controller);
    }

    void applyProjectileBodyCommands(
        const std::vector<ProjectileBodyCommand>& commands) noexcept override
    {
        for (const auto& command : commands)
            applyProjectileBodyCommand(command);
    }

    void step(float seconds,
              const std::vector<VehicleInput>& rawInputs) noexcept override
    {
        const float simulationSeconds =
            std::clamp(seconds, 0.0F, 0.25F);
        float remaining = simulationSeconds;
        constexpr float joltStep = 1.0F / 120.0F;
        // Windows gameplay has a separate fixed clock. World::cMaxSimStep
        // is 1/60 even though the replacement backend uses two smaller Jolt
        // solver steps for stability. Never advance Race/GameCar timers at
        // the backend's 120 Hz rate.
        constexpr float sourceStep = 1.0F / 60.0F;
        const bool sourceFixedStepEnabled =
            static_cast<bool>(worldFixedStepController_) ||
            static_cast<bool>(fixedStepController_);
        std::size_t sourceStepsDue = 0U;
        if (sourceFixedStepEnabled)
        {
            sourceFixedStepAccumulator_ += simulationSeconds;
            sourceStepsDue = static_cast<std::size_t>(std::floor(
                (sourceFixedStepAccumulator_ + 0.000001F) / sourceStep));
            sourceFixedStepAccumulator_ -=
                static_cast<float>(sourceStepsDue) * sourceStep;
            sourceFixedStepAccumulator_ = std::max(
                sourceFixedStepAccumulator_, 0.0F);
        }
        float simulatedSeconds = 0.0F;
        std::size_t sourceStepsDispatched = 0U;
        contactListener_.beginStep();
        while (remaining > 0.0F)
        {
            const float delta = std::min(remaining, joltStep);
            const bool dispatchSourceFixedStep =
                sourceStepsDispatched < sourceStepsDue &&
                simulatedSeconds + 0.000001F >=
                    static_cast<float>(sourceStepsDispatched) * sourceStep;
            if (dispatchSourceFixedStep)
            {
                sourceFixedInputs_.assign(
                    rawInputs.begin(), rawInputs.end());
                sourceFixedInputs_.resize(vehicles_.size());
                if (worldFixedStepController_)
                {
                    // World::FixedStep runs before PhysX Scene::Compute.
                    // Refresh the completed state from the preceding source
                    // interval before Race updates its full player roster.
                    fixedStepStates_.resize(vehicles_.size());
                    for (std::size_t index = 0;
                         index < vehicles_.size(); ++index)
                    {
                        updateState(vehicles_[index]);
                        const auto& state = vehicles_[index].state;
                        auto& snapshot = fixedStepStates_[index];
                        snapshot.body = state.body;
                        snapshot.bodyAwake = state.bodyAwake;
                        snapshot.linearVelocity = state.linearVelocity;
                        snapshot.angularMomentum = state.angularMomentum;
                        snapshot.kineticEnergy = state.kineticEnergy;
                        snapshot.speed = state.speed;
                        snapshot.drivenWheelSpeed = state.drivenWheelSpeed;
                        snapshot.engineRpm = state.engineRpm;
                        snapshot.gear = state.gear;
                        snapshot.contactCount = state.contactCount;
                        snapshot.resetCount = state.resetCount;
                    }
                    fixedStepResets_.clear();
                    fixedStepLinearVelocities_.clear();
                    for (auto& projectile : projectileBodies_)
                        updateState(projectile);
                    fixedStepProjectileStates_.clear();
                    fixedStepProjectileStates_.reserve(
                        projectileBodies_.size());
                    for (const auto& projectile : projectileBodies_)
                        fixedStepProjectileStates_.push_back(
                            projectile.state);
                    fixedStepProjectileCommands_.clear();
                    worldFixedStepController_(
                        sourceStep, fixedStepStates_, sourceFixedInputs_,
                        fixedStepResets_, fixedStepLinearVelocities_,
                        fixedStepProjectileStates_,
                        fixedStepProjectileCommands_);
                    sourceFixedInputs_.resize(vehicles_.size());
                    for (const auto& reset : fixedStepResets_)
                        resetVehicle(
                            reset.vehicle, reset.position, reset.direction);
                    for (const auto& velocity : fixedStepLinearVelocities_)
                        addLinearVelocity(
                            velocity.vehicle, velocity.delta);
                    applyProjectileBodyCommands(
                        fixedStepProjectileCommands_);
                }
                ++sourceStepsDispatched;
            }
            fixedStepInputs_ = sourceFixedStepEnabled
                ? sourceFixedInputs_ : rawInputs;
            fixedStepInputs_.resize(vehicles_.size());
            for (std::size_t index = 0; index < vehicles_.size(); ++index)
            {
                prepareVehicleStep(
                    index, vehicles_[index], fixedStepInputs_[index], delta,
                    dispatchSourceFixedStep, sourceStep);
            }
            system_.Update(delta, 1, &tempAllocator_, &jobs_);
            remaining -= delta;
            simulatedSeconds += delta;
        }
        for (auto& vehicle : vehicles_)
            updateState(vehicle);
        for (std::size_t index = 0; index < decorations_.size(); ++index)
            updateState(index);
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
        for (auto& projectile : projectileBodies_)
            updateState(projectile);
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

    void setDecorationEnabled(std::size_t index,
                              bool enabled) noexcept override
    {
        if (index >= decorations_.size() ||
            decorations_[index].state.active == enabled)
            return;
        auto& decoration = decorations_[index];
        auto& bodies = system_.GetBodyInterface();
        auto setBodyEnabled = [&](JPH::BodyID body) {
            if (body.IsInvalid())
                return;
            if (enabled)
                bodies.AddBody(body, JPH::EActivation::Activate);
            else
                bodies.RemoveBody(body);
        };
        for (const auto body : decoration.meshBodies)
            setBodyEnabled(body);
        for (const auto body : decoration.childShapeBodies)
            setBodyEnabled(body);
        setBodyEnabled(decoration.shapeBody);
        decoration.state.active = enabled;
        if (enabled)
            updateState(index);
    }

    const DecorationState& decoration(
        std::size_t index) const noexcept override
    {
        return index < decorations_.size() ? decorations_[index].state
                                           : emptyDecoration_;
    }

    std::size_t decorationCount() const noexcept override
    {
        return decorations_.size();
    }

    std::size_t addDebris(
        const DebrisDescription& description) noexcept override
    {
        JPH::RefConst<JPH::Shape> shape;
        if (!description.collisionMeshes.empty())
        {
            JPH::TriangleList triangles;
            for (const auto& mesh : description.collisionMeshes)
            {
                triangles.reserve(triangles.size() +
                                  mesh.indices.size() / 3U);
                for (std::size_t index = 0;
                     index + 2U < mesh.indices.size(); index += 3U)
                {
                    if (mesh.indices[index] >= mesh.vertices.size() ||
                        mesh.indices[index + 1U] >= mesh.vertices.size() ||
                        mesh.indices[index + 2U] >= mesh.vertices.size())
                        return std::numeric_limits<std::size_t>::max();
                    const Vec3 a = transformPoint(
                        mesh.transform, mesh.vertices[mesh.indices[index]]);
                    const Vec3 b = transformPoint(
                        mesh.transform,
                        mesh.vertices[mesh.indices[index + 1U]]);
                    const Vec3 c = transformPoint(
                        mesh.transform,
                        mesh.vertices[mesh.indices[index + 2U]]);
                    triangles.emplace_back(toJolt(a), toJolt(c), toJolt(b));
                }
            }
            if (triangles.empty())
                return std::numeric_limits<std::size_t>::max();
            JPH::MeshShapeSettings meshSettings(triangles);
            const auto meshResult = meshSettings.Create();
            if (meshResult.HasError())
                return std::numeric_limits<std::size_t>::max();
            shape = meshResult.Get();
        }
        else
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
            shape = shifted.Get();
        }
        const auto motion =
            description.dynamic ? JPH::EMotionType::Dynamic
                                : JPH::EMotionType::Static;
        JPH::BodyCreationSettings settings(
            shape, toJolt(description.transform.position),
            toJolt(description.transform.rotation), motion,
            description.dynamic ? Layers::moving : Layers::nonMoving);
        if (description.dynamic)
        {
            settings.mOverrideMassProperties =
                JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass =
                std::max(description.mass, 1.0F);
        }
        settings.mFriction = 0.5F;
        settings.mRestitution = 0.5F;
        settings.mEnhancedInternalEdgeRemoval = true;
        settings.mUserData = surfaceUserData(
            CollisionSurface::Decoration);
        DebrisRuntime runtime;
        runtime.body = system_.GetBodyInterface().CreateAndAddBody(
            settings, description.dynamic
                          ? JPH::EActivation::Activate
                          : JPH::EActivation::DontActivate);
        if (runtime.body.IsInvalid())
            return std::numeric_limits<std::size_t>::max();
        runtime.state.body = description.transform;
        runtime.state.active = true;
        runtime.lifetime = description.lifetime;
        if (description.dynamic &&
            (description.localImpulse.x != 0.0F ||
            description.localImpulse.y != 0.0F ||
             description.localImpulse.z != 0.0F))
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

    const ProjectileBodyState& projectileBody(
        std::size_t index) const noexcept override
    {
        return index < projectileBodies_.size()
                   ? projectileBodies_[index].state
                   : emptyProjectileBody_;
    }

    std::size_t projectileBodyCount() const noexcept override
    {
        return projectileBodies_.size();
    }

    WorldRayCastHit raycast(
        const WorldRayCastQuery& query) const noexcept override
    {
        WorldRayCastHit result;
        result.distance = std::max(query.maximumDistance, 0.0F);
        if (result.distance <= 0.0F)
            return result;
        const Vec3 sourceDirection = query.direction;
        const float sourceLength = std::sqrt(
            sourceDirection.x * sourceDirection.x +
            sourceDirection.y * sourceDirection.y +
            sourceDirection.z * sourceDirection.z);
        if (sourceLength <= 0.000001F)
            return result;
        const Vec3 normalizedDirection{
            sourceDirection.x / sourceLength,
            sourceDirection.y / sourceLength,
            sourceDirection.z / sourceLength};
        const JPH::RRayCast ray{
            toJolt(query.origin),
            result.distance * toJolt(normalizedDirection)};

        class Collector final : public JPH::CastRayCollector
        {
        public:
            Collector(const JPH::PhysicsSystem& physics,
                      const WorldRayCastQuery& sourceQuery,
                      const JPH::RRayCast& sourceRay,
                      float maximumDistance) noexcept
                : system(physics), query(sourceQuery), ray(sourceRay),
                  maxDistance(maximumDistance)
            {
                hit.distance = maximumDistance;
            }

            void AddHit(const JPH::RayCastResult& value) override
            {
                if (value.mFraction >= GetEarlyOutFraction())
                    return;
                JPH::BodyLockRead lock(
                    system.GetBodyLockInterface(), value.mBodyID);
                if (!lock.Succeeded())
                    return;
                const JPH::Body& body = lock.GetBody();
                const auto userData = body.GetUserData();
                const auto kind = userData & bodyKindMask;
                std::size_t vehicle =
                    std::numeric_limits<std::size_t>::max();
                std::size_t decoration =
                    std::numeric_limits<std::size_t>::max();
                std::size_t projectileBody =
                    std::numeric_limits<std::size_t>::max();
                CollisionSurface surface = CollisionSurface::TrackPlane;
                if (kind == vehicleBodyKind)
                {
                    vehicleIndex(userData, vehicle);
                    if (query.trackPlaneOnly ||
                        vehicle == query.ignoredVehicle)
                        return;
                    surface = CollisionSurface::Vehicle;
                }
                else if (kind == decorationBodyKind)
                {
                    if (query.trackPlaneOnly)
                        return;
                    decorationIndex(userData, decoration);
                    surface = CollisionSurface::Decoration;
                }
                else if (kind == surfaceBodyKind)
                {
                    surface = collisionSurface(userData);
                    if (surface == CollisionSurface::DeathPlane &&
                        !query.includeDeathPlane)
                        return;
                    if (query.trackPlaneOnly &&
                        surface != CollisionSurface::TrackPlane)
                        return;
                }
                else if (kind == projectileBodyKind)
                {
                    if (!query.includeProjectileBodies ||
                        !projectileIsShotTrack(userData))
                        return;
                    projectileIndex(userData, projectileBody);
                    // ShotTrack is not a world material.  Its independent
                    // identity is carried by projectileBody while the query
                    // remains in the track-plane family.
                    surface = CollisionSurface::TrackPlane;
                }
                else
                {
                    return;
                }

                const auto position = ray.GetPointOnRay(value.mFraction);
                auto normal = body.GetWorldSpaceSurfaceNormal(
                    value.mSubShapeID2, position);
                const auto rayDirection = JPH::Vec3(ray.mDirection);
                if (normal.Dot(rayDirection) > 0.0F)
                    normal = -normal;
                hit.hit = true;
                hit.distance = maxDistance * value.mFraction;
                hit.position = fromJolt(JPH::Vec3(position));
                hit.normal = fromJolt(normal);
                hit.surface = surface;
                hit.vehicle = vehicle;
                hit.decoration = decoration;
                hit.projectileBody = projectileBody;
                hit.actor = body.GetID().GetIndexAndSequenceNumber();
                UpdateEarlyOutFraction(value.mFraction);
            }

            const JPH::PhysicsSystem& system;
            const WorldRayCastQuery& query;
            const JPH::RRayCast& ray;
            float maxDistance = 0.0F;
            WorldRayCastHit hit;
        } collector(system_, query, ray, result.distance);

        JPH::RayCastSettings settings;
        settings.mBackFaceModeTriangles =
            JPH::EBackFaceMode::CollideWithBackFaces;
        system_.GetNarrowPhaseQuery().CastRay(
            ray, settings, collector);
        return collector.hit;
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
        float lateralGripScale = 1.0F;
        VehicleDriveCommand sourceDriveCommand;
        std::vector<float> wheelNormalReactions;
        std::vector<float> wheelNormalImpulses;
        std::vector<bool> wheelTireReleaseConsumed;
        std::vector<bool> wheelTireReleaseActive;
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

    struct ProjectileBodyRuntime
    {
        JPH::BodyID body;
        ProjectileBodyState state;
    };

    struct DecorationRuntime
    {
        std::vector<JPH::BodyID> meshBodies;
        std::vector<JPH::BodyID> childShapeBodies;
        JPH::BodyID shapeBody;
        DecorationState state;
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

    void stabilizeVehicle(
        VehicleRuntime& vehicle, bool anyContact,
        const std::array<float, 3U>& angularDamping,
        float clampRollAngle, float clampPitchAngle) noexcept
    {
        auto& bodies = system_.GetBodyInterface();
        const JPH::Quat rotation = bodies.GetRotation(vehicle.body);
        JPH::Vec3 localAngularVelocity =
            rotation.Conjugated() *
            bodies.GetAngularVelocity(vehicle.body);
        localAngularVelocity.SetX(
            localAngularVelocity.GetX() * angularDamping[0U]);
        localAngularVelocity.SetZ(
            localAngularVelocity.GetZ() * angularDamping[1U]);
        localAngularVelocity.SetY(
            localAngularVelocity.GetY() * angularDamping[2U]);
        if (!anyContact)
        {
            if (clampRollAngle > 0.0F)
                localAngularVelocity.SetX(std::clamp(
                    localAngularVelocity.GetX(),
                    -2.0F * clampRollAngle,
                    2.0F * clampRollAngle));
            if (clampPitchAngle > 0.0F)
                localAngularVelocity.SetZ(std::clamp(
                    localAngularVelocity.GetZ(),
                    -2.0F * clampPitchAngle,
                    2.0F * clampPitchAngle));
        }
        bodies.SetAngularVelocity(
            vehicle.body, rotation * localAngularVelocity);

        if (clampRollAngle > 0.0F || clampPitchAngle > 0.0F)
        {
            Vec3 euler = quaternionToEulerXYZ(fromJolt(rotation));
            if (clampRollAngle > 0.0F)
                euler.x = std::clamp(
                    euler.x, -clampRollAngle, clampRollAngle);
            if (clampPitchAngle > 0.0F)
                euler.y = std::clamp(
                    euler.y, -clampPitchAngle, clampPitchAngle);
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

    void applyGameCarSteering(
        VehicleRuntime& vehicle,
        const VehicleDriveCommand& command) noexcept
    {
        vehicle.steeringAngle = command.steeringAngle;
        if (std::abs(command.steeringYaw) <= 0.000001F)
            return;
        auto& bodies = system_.GetBodyInterface();
        const auto rotation = bodies.GetRotation(vehicle.body);
        const auto correctedRotation = rotation * JPH::Quat::sRotation(
            JPH::Vec3::sAxisY(), -command.steeringYaw);
        const JPH::Vec3 rearPivot{command.rearWheelX, 0.0F, 0.0F};
        const auto position = bodies.GetPosition(vehicle.body);
        const auto pivot = position + rotation * rearPivot;
        bodies.SetPositionAndRotation(
            vehicle.body, pivot - correctedRotation * rearPivot,
            correctedRotation, JPH::EActivation::Activate);
    }

    void prepareVehicleStep(std::size_t vehicleIndex,
                            VehicleRuntime& vehicle, VehicleInput input,
                            float delta, bool dispatchSourceFixedStep,
                            float sourceDelta) noexcept
    {
        if (!vehicle.enabled)
            return;
        const auto wheelCount = vehicle.constraint->GetWheels().size();
        if (vehicle.wheelTireReleaseConsumed.size() != wheelCount)
            vehicle.wheelTireReleaseConsumed.assign(wheelCount, false);
        vehicle.wheelTireReleaseActive.assign(wheelCount, false);
        for (JPH::uint index = 0U; index < wheelCount; ++index)
        {
            if (!vehicle.constraint->GetWheel(index)->HasContact())
                vehicle.wheelTireReleaseConsumed[index] = false;
        }
        input.throttle = std::clamp(input.throttle, 0.0F, 1.0F);
        input.reverse = std::clamp(input.reverse, 0.0F, 1.0F);
        input.brake = std::clamp(input.brake, 0.0F, 1.0F);
        input.steering = std::clamp(input.steering, -1.0F, 1.0F);
        input.motorTorqueScale =
            std::max(input.motorTorqueScale, 0.0F);
        input.lateralGripScale =
            std::max(input.lateralGripScale, 0.0F);

        const auto& source = vehicle.spawn.vehicle;
        bool anyContact = false;
        bool drivenContact = false;
        bool allContact = !vehicle.constraint->GetWheels().empty();
        float drivenWheelSpeed = 0.0F;
        bool foundDrivenWheel = false;
        for (JPH::uint index = 0;
             index < vehicle.constraint->GetWheels().size(); ++index)
        {
            const auto* wheel = vehicle.constraint->GetWheel(index);
            anyContact = anyContact || wheel->HasContact();
            allContact = allContact && wheel->HasContact();
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
        const JPH::Vec3 horizontalVelocity{
            velocity.GetX(), 0.0F, velocity.GetZ()};
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
        VehicleDriveCommand gameCarCommand;
        const bool gameCarControlled =
            static_cast<bool>(fixedStepController_);
        if (gameCarControlled)
        {
            if (dispatchSourceFixedStep)
            {
                vehicle.sourceDriveCommand = fixedStepController_(
                    vehicleIndex, sourceDelta, input,
                    {signedSpeed, velocity.Length(),
                     horizontalVelocity.Length(), drivenWheelSpeed,
                     anyContact, drivenContact, allContact,
                     !vehicle.state.bodyContacts.empty()});
            }
            gameCarCommand = vehicle.sourceDriveCommand;
            motorTorque = gameCarCommand.motorTorque;
            brakeTorque = gameCarCommand.brakeTorque;
            rpm = gameCarCommand.engineRpm;
            vehicle.currentGear = gameCarCommand.gear;
            vehicle.lateralGripScale =
                std::max(gameCarCommand.lateralGripScale, 0.0F);
        }
        else
        {
            vehicle.lateralGripScale = input.lateralGripScale;
            // Standalone engine smoke keeps a backend-local controller. The
            // active game always installs source GameCar::OnFixedStep.
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
            else
                motorTorque *= input.motorTorqueScale;
        }
        vehicle.motorTorque = motorTorque;
        vehicle.engineRpm = rpm;

        if (gameCarControlled)
        {
            if (dispatchSourceFixedStep)
            {
                stabilizeVehicle(
                    vehicle, anyContact, gameCarCommand.angularDamping,
                    gameCarCommand.clampRollAngle,
                    gameCarCommand.clampPitchAngle);
                applyGameCarSteering(vehicle, gameCarCommand);
            }
        }
        else
        {
            stabilizeVehicle(
                vehicle, anyContact,
                {source.angularDamping.x, source.angularDamping.y,
                 vehicle.wheelTractionEnabled
                     ? source.angularDamping.z : 1.0F},
                source.clampRollAngle, source.clampPitchAngle);
            applySourceSteering(
                vehicle, input.steering, delta,
                anyContact, drivenContact);
        }

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

        const bool applyExtraGravity = gameCarControlled
            ? gameCarCommand.applyExtraGravity : !anyContact;
        if (applyExtraGravity)
        {
            bodies.AddForce(
                vehicle.body,
                toJolt(Vec3{0.0F, 0.0F,
                            source.mass * description_.gravity}));
            const float pitchAcceleration = gameCarControlled
                ? gameCarCommand.airbornePitchAcceleration
                : (horizontalVelocity.Length() > 1.0F &&
                   !input.springLocked
                       ? source.airbornePitchAcceleration : 0.0F);
            if (pitchAcceleration != 0.0F)
            {
                const JPH::Quat currentRotation =
                    bodies.GetRotation(vehicle.body);
                bodies.AddLinearAndAngularVelocity(
                    vehicle.body, JPH::Vec3::sZero(),
                    currentRotation *
                        JPH::Vec3{
                            0.0F, 0.0F,
                            -pitchAcceleration * delta});
            }
        }

        if (input.throttle != 0.0F || input.reverse != 0.0F ||
            input.brake != 0.0F || input.steering != 0.0F)
            bodies.ActivateBody(vehicle.body);
    }

    ProjectileBodyRuntime* findProjectileBody(
        std::uint64_t id) noexcept
    {
        const auto found = std::find_if(
            projectileBodies_.begin(), projectileBodies_.end(),
            [id](const ProjectileBodyRuntime& value) {
                return value.state.id == id;
            });
        return found == projectileBodies_.end() ? nullptr : &*found;
    }

    void destroyProjectileBody(ProjectileBodyRuntime& projectile) noexcept
    {
        if (projectile.body.IsInvalid())
        {
            projectile.state.active = false;
            return;
        }
        auto& bodies = system_.GetBodyInterface();
        if (projectile.state.active)
            bodies.RemoveBody(projectile.body);
        bodies.DestroyBody(projectile.body);
        projectile.body = JPH::BodyID();
        projectile.state.active = false;
    }

    bool createProjectileBody(
        const ProjectileBodyDescription& description) noexcept
    {
        if (description.id == invalidProjectileBodyId)
            return false;
        ProjectileBodyRuntime* runtime = findProjectileBody(description.id);
        std::size_t index = 0U;
        if (runtime != nullptr)
        {
            index = static_cast<std::size_t>(
                runtime - projectileBodies_.data());
            destroyProjectileBody(*runtime);
        }
        else
        {
            index = projectileBodies_.size();
            projectileBodies_.push_back({});
            runtime = &projectileBodies_.back();
            contactListener_.resizeProjectiles(
                projectileBodies_.size());
        }

        const JPH::Vec3 halfExtents{
            std::max(description.halfExtents.x, 0.01F),
            std::max(description.halfExtents.z, 0.01F),
            std::max(description.halfExtents.y, 0.01F)};
        const auto shifted = JPH::RotatedTranslatedShapeSettings(
                                 toJolt(description.shapePosition),
                                 toJolt(description.shapeRotation),
                                 new JPH::BoxShape(halfExtents))
                                 .Create();
        if (shifted.HasError())
            return false;

        const JPH::EMotionType motionType = description.kinematic
            ? JPH::EMotionType::Kinematic
            : description.dynamic ? JPH::EMotionType::Dynamic
                                  : JPH::EMotionType::Static;
        const bool moving = motionType != JPH::EMotionType::Static;
        JPH::BodyCreationSettings settings(
            shifted.Get(), toJolt(description.transform.position),
            toJolt(description.transform.rotation),
            motionType, moving ? Layers::moving : Layers::nonMoving);
        if (motionType == JPH::EMotionType::Dynamic)
        {
            settings.mOverrideMassProperties =
                JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass =
                std::max(description.mass, 0.001F);
        }
        settings.mGravityFactor = description.gravityFactor;
        settings.mIsSensor = description.sensor;
        settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
        settings.mFriction = 0.0F;
        settings.mRestitution = 0.0F;
        settings.mUserData = projectileUserData(
            index, description.shotTrack);
        runtime->body = system_.GetBodyInterface().CreateAndAddBody(
            settings, moving
                          ? JPH::EActivation::Activate
                          : JPH::EActivation::DontActivate);
        if (runtime->body.IsInvalid())
            return false;
        auto& bodies = system_.GetBodyInterface();
        if (moving)
        {
            bodies.SetLinearVelocity(
                runtime->body, toJolt(description.linearVelocity));
        }
        runtime->state.id = description.id;
        runtime->state.body = description.transform;
        runtime->state.linearVelocity = description.linearVelocity;
        runtime->state.active = true;
        return true;
    }

    void synchronizeProjectileBody(
        const ProjectileBodyDescription& description) noexcept
    {
        auto* runtime = findProjectileBody(description.id);
        if (runtime == nullptr || !runtime->state.active ||
            runtime->body.IsInvalid())
            return;
        auto& bodies = system_.GetBodyInterface();
        bodies.SetPositionAndRotation(
            runtime->body, toJolt(description.transform.position),
            toJolt(description.transform.rotation),
            JPH::EActivation::Activate);
        bodies.SetLinearVelocity(
            runtime->body, toJolt(description.linearVelocity));
        bodies.SetGravityFactor(
            runtime->body, description.gravityFactor);
        updateState(*runtime);
    }

    void applyProjectileBodyCommand(
        const ProjectileBodyCommand& command) noexcept
    {
        switch (command.kind)
        {
        case ProjectileBodyCommandKind::Create:
            createProjectileBody(command.body);
            break;
        case ProjectileBodyCommandKind::Synchronize:
            synchronizeProjectileBody(command.body);
            break;
        case ProjectileBodyCommandKind::Destroy:
            if (auto* runtime = findProjectileBody(command.body.id))
                destroyProjectileBody(*runtime);
            break;
        }
    }

    void updateState(ProjectileBodyRuntime& projectile) noexcept
    {
        if (!projectile.state.active || projectile.body.IsInvalid())
            return;
        JPH::BodyLockRead lock(
            system_.GetBodyLockInterface(), projectile.body);
        if (!lock.Succeeded())
            return;
        const JPH::Body& body = lock.GetBody();
        projectile.state.body.position = fromJolt(body.GetPosition());
        projectile.state.body.rotation = fromJolt(body.GetRotation());
        projectile.state.body.scale = {1.0F, 1.0F, 1.0F};
        projectile.state.linearVelocity =
            body.GetMotionType() == JPH::EMotionType::Static
                ? Vec3{}
                : fromJolt(body.GetLinearVelocity());
        const auto index = static_cast<std::size_t>(
            &projectile - projectileBodies_.data());
        projectile.state.contacts =
            contactListener_.takeProjectile(index);
    }

    void clearProjectileBodies() noexcept
    {
        for (auto& projectile : projectileBodies_)
            destroyProjectileBody(projectile);
        projectileBodies_.clear();
        contactListener_.resizeProjectiles(0U);
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

    void destroyDecorations() noexcept
    {
        auto& bodies = system_.GetBodyInterface();
        for (auto& decoration : decorations_)
        {
            auto destroyBody = [&](JPH::BodyID& body) {
                if (body.IsInvalid())
                    return;
                if (decoration.state.active)
                    bodies.RemoveBody(body);
                bodies.DestroyBody(body);
                body = JPH::BodyID();
            };
            for (auto& body : decoration.meshBodies)
                destroyBody(body);
            decoration.meshBodies.clear();
            for (auto& body : decoration.childShapeBodies)
                destroyBody(body);
            decoration.childShapeBodies.clear();
            destroyBody(decoration.shapeBody);
            decoration.state.active = false;
        }
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
        std::vector<JPH::TriangleList> decorationTriangles(
            decorations_.size());
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
        {
            const bool ownedDecoration =
                mesh.surface == CollisionSurface::Decoration &&
                mesh.decorationInstance < decorationTriangles.size();
            auto& surfaceTriangles =
                ownedDecoration
                    ? decorationTriangles[mesh.decorationInstance]
                    : triangles[surfaceIndex(mesh.surface)];
            surfaceTriangles.reserve(surfaceTriangles.size() +
                                     mesh.indices.size() / 3U);
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
        auto createMeshBody = [&](const JPH::TriangleList& source,
                                  float friction,
                                  float restitution, JPH::uint64 userData,
                                  bool sensor) {
            if (source.empty())
                return JPH::BodyID();
            JPH::MeshShapeSettings shapeSettings(source);
            const auto shapeResult = shapeSettings.Create();
            if (shapeResult.HasError())
                throw std::runtime_error(
                    ("Jolt track mesh: " + shapeResult.GetError()).c_str());
            JPH::BodyCreationSettings settings(
                shapeResult.Get(), JPH::RVec3::sZero(),
                JPH::Quat::sIdentity(), JPH::EMotionType::Static,
                Layers::nonMoving);
            settings.mFriction = friction;
            settings.mRestitution = restitution;
            settings.mUserData = userData;
            settings.mIsSensor = sensor;
            const auto body =
                system_.GetBodyInterface().CreateAndAddBody(
                    settings, JPH::EActivation::DontActivate);
            if (body.IsInvalid())
                throw std::runtime_error(
                    "Jolt could not create track body");
            return body;
        };
        for (std::size_t index = 0; index < triangles.size(); ++index)
        {
            if (triangles[index].empty())
                continue;
            trackBodies_.push_back(createMeshBody(
                triangles[index], frictions[index], restitutions[index],
                surfaceUserData(surfaces[index]),
                false));
        }
        for (std::size_t index = 0;
             index < decorationTriangles.size(); ++index)
        {
            if (decorationTriangles[index].empty())
                continue;
            const bool sensor =
                index < description_.decorations.size() &&
                !description_.decorations[index].collisionResponse;
            decorations_[index].meshBodies.push_back(createMeshBody(
                decorationTriangles[index], 0.5F, 0.5F,
                decorationUserData(index), sensor));
        }
        // Map.cpp creates a +Z NxPlaneShape at world Z=0 in the dedicated
        // cdgPlaneDeath group. Jolt is Y-up, so +Y is the same source plane.
        // PlaneShape uses a finite broad-phase extent; keep it far beyond all
        // serialized maps while preserving the infinite narrow-phase plane.
        JPH::BodyCreationSettings deathPlaneSettings(
            new JPH::PlaneShape(
                JPH::Plane(JPH::Vec3::sAxisY(), 0.0F), nullptr,
                100000.0F),
            JPH::RVec3::sZero(), JPH::Quat::sIdentity(),
            JPH::EMotionType::Static, Layers::nonMoving);
        deathPlaneSettings.mIsSensor = true;
        deathPlaneSettings.mUserData = surfaceUserData(
            CollisionSurface::DeathPlane);
        const auto deathPlane =
            system_.GetBodyInterface().CreateAndAddBody(
                deathPlaneSettings, JPH::EActivation::DontActivate);
        if (deathPlane.IsInvalid())
            throw std::runtime_error(
                "Jolt could not create source death plane");
        trackBodies_.push_back(deathPlane);
    }

    void createDecorationBodies()
    {
        for (std::size_t index = 0;
             index < description_.decorations.size(); ++index)
        {
            const auto& source = description_.decorations[index];
            auto& decoration = decorations_[index];
            auto createBoxBody = [&](Vec3 halfExtents, Vec3 shapePosition,
                                     Quat shapeRotation, bool dynamic,
                                     float mass, bool collisionResponse) {
                const auto box = new JPH::BoxShape(toJolt(halfExtents));
                const auto shifted = JPH::RotatedTranslatedShapeSettings(
                                         toJolt(shapePosition),
                                         toJolt(shapeRotation), box)
                                         .Create();
                if (shifted.HasError())
                {
                    throw std::runtime_error(
                        ("Jolt decoration shape transform: " +
                         shifted.GetError())
                            .c_str());
                }
                const auto motion =
                    dynamic ? JPH::EMotionType::Dynamic
                            : JPH::EMotionType::Static;
                JPH::BodyCreationSettings settings(
                    shifted.Get(), toJolt(source.transform.position),
                    toJolt(source.transform.rotation), motion,
                    dynamic ? Layers::moving : Layers::nonMoving);
                if (dynamic)
                {
                    settings.mOverrideMassProperties =
                        JPH::EOverrideMassProperties::CalculateInertia;
                    settings.mMassPropertiesOverride.mMass =
                        std::max(mass, 1.0F);
                }
                settings.mFriction = 0.5F;
                settings.mRestitution = 0.5F;
                settings.mEnhancedInternalEdgeRemoval = true;
                settings.mUserData = decorationUserData(index);
                settings.mIsSensor = !collisionResponse;
                const auto body =
                    system_.GetBodyInterface().CreateAndAddBody(
                        settings, dynamic ? JPH::EActivation::Activate
                                          : JPH::EActivation::DontActivate);
                if (body.IsInvalid())
                {
                    throw std::runtime_error(
                        "Jolt could not create decoration body");
                }
                return body;
            };
            if (source.hasBodyShape)
            {
                decoration.shapeBody = createBoxBody(
                    source.halfExtents, source.shapePosition,
                    source.shapeRotation, source.dynamic, source.mass,
                    source.collisionResponse);
            }
            // Actor::InitRootNxActor attaches these shapes to the parent's
            // static NX_AF_DISABLE_RESPONSE actor. Their saved body records
            // take effect only after OnDeath detaches the pieces, so the
            // intact forms are static sensors here as well.
            decoration.childShapeBodies.reserve(source.childShapes.size());
            for (const auto& child : source.childShapes)
            {
                decoration.childShapeBodies.push_back(createBoxBody(
                    child.halfExtents, child.position, child.rotation,
                    false, 0.0F, source.collisionResponse));
            }
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
        vehicles_.back().constraint->SetSuspensionMaxImpulseCallback(
            [this, vehicleIndexValue](
                JPH::uint wheelIndex, float suspensionImpulse,
                JPH::Vec3Arg contactNormal, float deltaTime) {
                auto& vehicle = vehicles_[vehicleIndexValue];
                const auto& source = vehicle.spawn.vehicle;
                const auto wheelCount =
                    vehicle.constraint->GetWheels().size();
                if (wheelIndex >= wheelCount || wheelCount == 0U)
                    return 0.0F;
                const float normalReactionImpulse =
                    source.mass *
                    std::abs(system_.GetGravity().Dot(contactNormal)) *
                    deltaTime / static_cast<float>(wheelCount);
                auto force = sourceWheelNormalForce(
                    suspensionImpulse, normalReactionImpulse,
                    vehicle.wheelTractionEnabled, 0.0F);
                if (vehicle.wheelTractionEnabled &&
                    source.tireSpring > 0.0F &&
                    wheelIndex <
                        vehicle.wheelTireReleaseConsumed.size())
                {
                    auto consumed =
                        vehicle.wheelTireReleaseConsumed[wheelIndex];
                    auto active =
                        vehicle.wheelTireReleaseActive[wheelIndex];
                    if (active ||
                        (!consumed &&
                         force.reaction > source.tireSpring))
                    {
                        consumed = true;
                        active = true;
                        force.impulse = 0.0F;
                    }
                }
                if (wheelIndex < vehicle.wheelNormalReactions.size())
                {
                    vehicle.wheelNormalReactions[wheelIndex] =
                        force.reaction;
                }
                if (wheelIndex < vehicle.wheelNormalImpulses.size())
                    vehicle.wheelNormalImpulses[wheelIndex] = force.impulse;
                return force.impulse;
            });
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
                lateral *=
                    vehicles_[vehicleIndexValue].lateralGripScale;
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
            vehicle.state.bodyAwake = false;
            vehicle.state.linearVelocity = {};
            vehicle.state.angularMomentum = {};
            vehicle.state.kineticEnergy = 0.0F;
            vehicle.state.speed = 0.0F;
            vehicle.state.drivenWheelSpeed = 0.0F;
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
        state.bodyAwake = body.IsActive();
        state.body.position = fromJolt(body.GetPosition());
        state.body.rotation = fromJolt(body.GetRotation());
        state.body.scale = {1.0F, 1.0F, 1.0F};
        state.linearVelocity = fromJolt(body.GetLinearVelocity());
        const JPH::Vec3 angularVelocity = body.GetAngularVelocity();
        const JPH::Vec3 angularMomentum =
            body.GetInverseInertia()
                .Inversed3x3()
                .Multiply3x3(angularVelocity);
        state.angularMomentum = fromJoltAngular(angularMomentum);
        state.kineticEnergy =
            0.5F * vehicle.spawn.vehicle.mass *
                body.GetLinearVelocity().LengthSq() +
            0.5F * angularVelocity.Dot(angularMomentum);
        // Player::CarState::Update obtains GameCar::GetSpeed(actor, dir),
        // i.e. the signed projection on the car's local +X axis, and applies
        // its one-metre-per-second dead zone.  Feeding total velocity length
        // here made AICar think a vehicle sliding sideways against a border
        // was still moving forward, so its source blocking/reverse/reset
        // state machine never recovered and opponents accumulated on walls.
        const float longitudinalSpeed = body.GetLinearVelocity().Dot(
            body.GetRotation() * JPH::Vec3::sAxisX());
        state.speed = std::abs(longitudinalSpeed) < 1.0F
                          ? 0.0F
                          : longitudinalSpeed;
        state.drivenWheelSpeed = 0.0F;
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
        bool freeWheelSpeedRead = false;
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
            // GameCar::GetDrivenWheelSpeed searches the first !lead wheel.
            // Despite the historical method name, lead is the powered
            // group and the camera deliberately observes a free wheel.
            if (!freeWheelSpeedRead &&
                index < vehicle.spawn.vehicle.wheels.size() &&
                !vehicle.spawn.vehicle.wheels[index].driven)
            {
                freeWheelSpeedRead = true;
                const float speed =
                    joltWheel->GetAngularVelocity() *
                    vehicle.spawn.vehicle.wheels[index].radius;
                state.drivenWheelSpeed =
                    std::abs(speed) > 0.1F ? speed : 0.0F;
            }
            WheelContactState contact;
            contact.hasContact = joltWheel->HasContact();
            if (contact.hasContact)
            {
                contact.position =
                    fromJolt(joltWheel->GetContactPosition());
                contact.normal =
                    fromJolt(joltWheel->GetContactNormal());
                const auto* wheeled =
                    static_cast<const JPH::WheelWV*>(joltWheel);
                contact.longitudinalSlip = wheeled->mLongitudinalSlip;
                contact.lateralSlip = wheeled->mLateralSlip;
                if (index < vehicle.wheelNormalReactions.size())
                {
                    contact.normalReaction =
                        vehicle.wheelNormalReactions[index];
                }
                if (index < vehicle.wheelNormalImpulses.size())
                {
                    contact.normalImpulse =
                        vehicle.wheelNormalImpulses[index];
                }
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

    void updateState(std::size_t index) noexcept
    {
        if (index >= decorations_.size())
            return;
        auto& decoration = decorations_[index];
        if (!decoration.state.active || decoration.shapeBody.IsInvalid())
            return;
        JPH::BodyLockRead lock(system_.GetBodyLockInterface(),
                               decoration.shapeBody);
        if (!lock.Succeeded())
            return;
        const JPH::Body& body = lock.GetBody();
        decoration.state.body.position = fromJolt(body.GetPosition());
        decoration.state.body.rotation = fromJolt(body.GetRotation());
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
    std::vector<DecorationRuntime> decorations_;
    std::vector<DebrisRuntime> debris_;
    std::vector<ProjectileBodyRuntime> projectileBodies_;
    DecorationState emptyDecoration_;
    DebrisState emptyDebris_;
    ProjectileBodyState emptyProjectileBody_;
    VehicleFixedStepController fixedStepController_;
    WorldFixedStepController worldFixedStepController_;
    std::vector<VehicleInput> fixedStepInputs_;
    std::vector<VehicleInput> sourceFixedInputs_;
    std::vector<VehicleState> fixedStepStates_;
    std::vector<VehicleResetCommand> fixedStepResets_;
    std::vector<VehicleLinearVelocityCommand>
        fixedStepLinearVelocities_;
    std::vector<ProjectileBodyState> fixedStepProjectileStates_;
    std::vector<ProjectileBodyCommand> fixedStepProjectileCommands_;
    float sourceFixedStepAccumulator_ = 0.0F;
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
    const auto flatBodyFriction = sourceTrackBodyFriction(
        JPH::Quat::sIdentity(), JPH::Vec3::sAxisY(),
        CollisionSurface::TrackPlane);
    const auto borderBodyFriction = sourceTrackBodyFriction(
        JPH::Quat::sIdentity(), JPH::Vec3::sAxisZ(),
        CollisionSurface::TrackBorder);
    if (std::abs(flatBodyFriction.first) > 0.0001F ||
        std::abs(flatBodyFriction.second - 0.1F) > 0.0001F ||
        std::abs(flatBodyFriction.firstDirection.Dot(
            JPH::Vec3::sAxisZ())) < 0.999F ||
        std::abs(borderBodyFriction.first) > 0.0001F ||
        std::abs(borderBodyFriction.second - 4.0F) > 0.0001F ||
        std::abs(borderBodyFriction.firstDirection.Dot(
            JPH::Vec3::sAxisX())) < 0.999F)
    {
        error = "GameCar::OnContactModify anisotropic friction failed";
        return false;
    }
    const auto cappedNormalForce = sourceWheelNormalForce(
        3.0F, 1.0F, true, 0.0F);
    const auto releasedTireSpring = sourceWheelNormalForce(
        3.0F, 1.0F, true, 2.0F);
    const auto releasedClutch = sourceWheelNormalForce(
        1.0F, 1.0F, false, 0.0F);
    if (std::abs(cappedNormalForce.reaction - 3.0F) > 0.0001F ||
        std::abs(cappedNormalForce.impulse - 1.5F) > 0.0001F ||
        releasedTireSpring.impulse != 0.0F ||
        releasedClutch.impulse != 0.0F)
    {
        error = "CarWheel::MyContactModify normal-force contract failed";
        return false;
    }
    const auto& selectedVehicle =
        description.spawns.empty()
            ? description.vehicle
            : description.spawns.front().vehicle;
    WorldDescription drivetrainDescription;
    drivetrainDescription.vehicle = selectedVehicle;
    // The drivetrain fixture drops the body two metres onto a flat plane.
    // Its purpose is transmission/clutch/reverse coverage; the source
    // tireSpring release branch is covered by sourceWheelNormalForce above.
    // A 2g landing cutoff would intentionally unload all four wheels and
    // turn the remainder into a chassis-collision test.
    drivetrainDescription.vehicle.tireSpring = 0.0F;
    drivetrainDescription.startPosition = {0.0F, 0.0F, 2.0F};
    drivetrainDescription.startDirection = {1.0F, 0.0F, 0.0F};
    drivetrainDescription.gravity = description.gravity;
    drivetrainDescription.spawns.push_back(
        {drivetrainDescription.vehicle, drivetrainDescription.startPosition,
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
    auto fixedStepWorld =
        createOriginalVehicleWorld(drivetrainDescription, error);
    if (!fixedStepWorld)
        return false;
    const auto resetCountBeforeWorldCallback =
        fixedStepWorld->vehicle().resetCount;
    std::size_t worldFixedStepCalls = 0U;
    std::size_t vehicleFixedStepCalls = 0U;
    bool worldFixedStepRosterValid = true;
    fixedStepWorld->setWorldFixedStepController(
        [&](float delta,
            const std::vector<VehicleState>& states,
            std::vector<VehicleInput>& inputs,
            std::vector<VehicleResetCommand>& resets,
            std::vector<VehicleLinearVelocityCommand>& velocities,
            const std::vector<ProjectileBodyState>& projectiles,
            std::vector<ProjectileBodyCommand>& projectileCommands) {
            ++worldFixedStepCalls;
            worldFixedStepRosterValid =
                worldFixedStepRosterValid &&
                std::abs(delta - 1.0F / 60.0F) < 0.000001F &&
                states.size() == 1U && inputs.size() == 1U &&
                velocities.empty() && projectiles.empty() &&
                projectileCommands.empty();
            inputs.front().throttle = 1.0F;
            if (worldFixedStepCalls == 1U)
            {
                resets.push_back(
                    {0U,
                     {drivetrainDescription.startPosition.x,
                      drivetrainDescription.startPosition.y,
                      drivetrainDescription.startPosition.z + 1.0F},
                     drivetrainDescription.startDirection});
                velocities.push_back({0U, {0.0F, 0.0F, 2.0F}});
                ProjectileBodyCommand projectile;
                projectile.kind = ProjectileBodyCommandKind::Create;
                projectile.body.id = 42U;
                projectile.body.transform.position = {0.0F, 0.0F, 5.0F};
                projectile.body.halfExtents = {0.1F, 0.1F, 0.1F};
                projectile.body.linearVelocity = {10.0F, 0.0F, 0.0F};
                projectile.body.shotTrack = true;
                projectileCommands.push_back(projectile);
                ProjectileBodyCommand touchingProjectile;
                touchingProjectile.kind =
                    ProjectileBodyCommandKind::Create;
                touchingProjectile.body.id = 43U;
                touchingProjectile.body.transform.position = {
                    drivetrainDescription.startPosition.x,
                    drivetrainDescription.startPosition.y,
                    drivetrainDescription.startPosition.z + 1.0F};
                touchingProjectile.body.halfExtents = {
                    0.5F, 0.5F, 0.5F};
                touchingProjectile.body.dynamic = false;
                touchingProjectile.body.kinematic = true;
                projectileCommands.push_back(touchingProjectile);
            }
        });
    fixedStepWorld->setVehicleFixedStepController(
        [&](std::size_t vehicle, float delta,
            const VehicleInput&, const VehicleFixedStepState&) {
            ++vehicleFixedStepCalls;
            worldFixedStepRosterValid =
                worldFixedStepRosterValid && vehicle == 0U &&
                std::abs(delta - 1.0F / 60.0F) < 0.000001F;
            VehicleDriveCommand command;
            command.brakeTorque =
                drivetrainDescription.vehicle.restBrakeTorque;
            command.engineRpm =
                drivetrainDescription.vehicle.idlingRpm;
            command.gear = -1;
            command.lateralGripScale = 1.0F;
            command.angularDamping = {
                drivetrainDescription.vehicle.angularDamping.x,
                drivetrainDescription.vehicle.angularDamping.y,
                drivetrainDescription.vehicle.angularDamping.z};
            return command;
        });
    fixedStepWorld->step(1.0F / 120.0F, VehicleInput{});
    if (worldFixedStepCalls != 0U || vehicleFixedStepCalls != 0U ||
        fixedStepWorld->vehicle().resetCount !=
            resetCountBeforeWorldCallback)
    {
        error = "source 60 Hz fixed clock advanced on one 120 Hz frame";
        return false;
    }
    fixedStepWorld->step(1.0F / 120.0F, VehicleInput{});
    if (worldFixedStepCalls != 1U || vehicleFixedStepCalls != 1U ||
        !worldFixedStepRosterValid ||
        fixedStepWorld->vehicle().resetCount !=
            resetCountBeforeWorldCallback + 1U ||
        fixedStepWorld->vehicle().linearVelocity.z <= 1.0F ||
        fixedStepWorld->projectileBodyCount() != 2U ||
        !fixedStepWorld->projectileBody(0U).active ||
        fixedStepWorld->projectileBody(0U).id != 42U ||
        fixedStepWorld->projectileBody(0U).body.position.x <= 0.05F ||
        fixedStepWorld->projectileBody(1U).id != 43U ||
        std::none_of(
            fixedStepWorld->projectileBody(1U).contacts.begin(),
            fixedStepWorld->projectileBody(1U).contacts.end(),
            [](const BodyContact& contact) {
                return contact.surface == CollisionSurface::Vehicle &&
                       contact.otherVehicle == 0U && contact.hasPoint;
            }))
    {
        error = "source Race world fixed-step/Jolt command bridge failed";
        return false;
    }
    WorldRayCastQuery trackRay;
    trackRay.origin = {0.0F, 0.0F, 10.0F};
    trackRay.direction = {0.0F, 0.0F, -1.0F};
    trackRay.maximumDistance = 20.0F;
    trackRay.trackPlaneOnly = true;
    const auto trackRayHit = fixedStepWorld->raycast(trackRay);
    WorldRayCastQuery shotTrackRay = trackRay;
    shotTrackRay.origin.x =
        fixedStepWorld->projectileBody(0U).body.position.x;
    shotTrackRay.includeProjectileBodies = true;
    const auto shotTrackRayHit =
        fixedStepWorld->raycast(shotTrackRay);
    WorldRayCastQuery vehicleRay;
    vehicleRay.origin = {-10.0F, 0.0F, 3.0F};
    vehicleRay.direction = {1.0F, 0.0F, 0.0F};
    vehicleRay.maximumDistance = 20.0F;
    const auto vehicleRayHit = fixedStepWorld->raycast(vehicleRay);
    vehicleRay.ignoredVehicle = 0U;
    const auto ignoredVehicleRayHit =
        fixedStepWorld->raycast(vehicleRay);
    WorldRayCastQuery deathPlaneRay;
    deathPlaneRay.origin = {500.0F, 0.0F, 10.0F};
    deathPlaneRay.direction = {0.0F, 0.0F, -1.0F};
    deathPlaneRay.maximumDistance = 20.0F;
    deathPlaneRay.ignoredVehicle = 0U;
    const auto excludedDeathPlaneRayHit =
        fixedStepWorld->raycast(deathPlaneRay);
    deathPlaneRay.includeDeathPlane = true;
    const auto deathPlaneRayHit =
        fixedStepWorld->raycast(deathPlaneRay);
    if (!trackRayHit.hit ||
        trackRayHit.surface != CollisionSurface::TrackPlane ||
        std::abs(trackRayHit.distance - 10.0F) > 0.05F ||
        !shotTrackRayHit.hit ||
        shotTrackRayHit.projectileBody != 0U ||
        shotTrackRayHit.distance >= trackRayHit.distance ||
        !vehicleRayHit.hit ||
        vehicleRayHit.surface != CollisionSurface::Vehicle ||
        vehicleRayHit.vehicle != 0U ||
        vehicleRayHit.distance >= vehicleRay.maximumDistance ||
        ignoredVehicleRayHit.hit || excludedDeathPlaneRayHit.hit ||
        !deathPlaneRayHit.hit ||
        deathPlaneRayHit.surface != CollisionSurface::DeathPlane ||
        std::abs(deathPlaneRayHit.distance - 10.0F) > 0.05F)
    {
        error = "source projectile-group Jolt raycast bridge failed";
        return false;
    }
    auto deathPlaneWorld =
        createOriginalVehicleWorld(drivetrainDescription, error);
    if (!deathPlaneWorld)
        return false;
    deathPlaneWorld->resetVehicle(
        0U, {500.0F, 0.0F, -2.0F}, {1.0F, 0.0F, 0.0F});
    deathPlaneWorld->step(1.0F / 60.0F, VehicleInput{});
    const bool deathPlaneContact = std::any_of(
        deathPlaneWorld->vehicle().bodyContacts.begin(),
        deathPlaneWorld->vehicle().bodyContacts.end(),
        [](const BodyContact& contact) {
            return contact.surface == CollisionSurface::DeathPlane &&
                   contact.hasPoint;
        });
    if (!deathPlaneContact)
    {
        error = "source cdgPlaneDeath Jolt sensor contact failed";
        return false;
    }
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
    const float fixedStep = 1.0F / 120.0F;
    for (const auto& contact : settled.wheelContacts)
    {
        if (!contact.hasContact)
            continue;
        const float staticImpulse =
            sourceVehicle.mass *
            std::abs(description.gravity * contact.normal.z) *
            fixedStep /
            static_cast<float>(settled.wheelContacts.size());
        if (!std::isfinite(contact.normalReaction) ||
            !std::isfinite(contact.normalImpulse) ||
            contact.normalImpulse > staticImpulse * 1.5F + 0.01F)
        {
            error = "source wheel normal impulse exceeded the 1.5g cap";
            return false;
        }
    }
    auto networkWorld =
        createOriginalVehicleWorld(drivetrainDescription, error);
    if (!networkWorld)
        return false;
    for (int step = 0; step < 240; ++step)
        networkWorld->step(1.0F / 120.0F, VehicleInput{});
    const Vec3 networkPosition =
        networkWorld->vehicle().body.position;
    constexpr float networkYaw = 0.5F;
    const Quat networkRotation{
        0.0F, 0.0F, std::sin(networkYaw * 0.5F),
        std::cos(networkYaw * 0.5F)};
    const Vec3 networkLinearMomentum{
        sourceVehicle.mass * 2.0F, 0.0F, 0.0F};
    const Vec3 networkAngularMomentum{0.0F, 0.0F, 100.0F};
    networkWorld->synchronizeNetworkVehicle(
        0U, networkPosition, networkRotation, networkLinearMomentum,
        networkAngularMomentum);
    const auto synchronized = networkWorld->vehicle();
    const float synchronizedRotationDot = std::abs(
        synchronized.body.rotation.x * networkRotation.x +
        synchronized.body.rotation.y * networkRotation.y +
        synchronized.body.rotation.z * networkRotation.z +
        synchronized.body.rotation.w * networkRotation.w);
    if (std::abs(synchronized.body.position.x - networkPosition.x) >
            0.001F ||
        std::abs(synchronized.body.position.y - networkPosition.y) >
            0.001F ||
        std::abs(synchronized.body.position.z - networkPosition.z) >
            0.001F ||
        synchronizedRotationDot < 0.999F ||
        std::abs(synchronized.linearVelocity.x - 2.0F) > 0.01F ||
        std::abs(synchronized.angularMomentum.z -
                 networkAngularMomentum.z) > 0.1F ||
        !std::isfinite(synchronized.kineticEnergy) ||
        synchronized.kineticEnergy <=
            0.5F * sourceVehicle.mass * 4.0F)
    {
        error = "active NetPlayer::ResponseStream momentum/rotation "
                "synchronization diverged from the source path: pos=" +
                std::to_string(synchronized.body.position.x) + "," +
                std::to_string(synchronized.body.position.y) + "," +
                std::to_string(synchronized.body.position.z) +
                " base=" + std::to_string(networkPosition.x) + "," +
                std::to_string(networkPosition.y) + "," +
                std::to_string(networkPosition.z) +
                " qdot=" + std::to_string(synchronizedRotationDot) +
                " velocity=" +
                std::to_string(synchronized.linearVelocity.x) + "," +
                std::to_string(synchronized.linearVelocity.y) + "," +
                std::to_string(synchronized.linearVelocity.z) +
                " angularMomentum=" +
                std::to_string(synchronized.angularMomentum.x) + "," +
                std::to_string(synchronized.angularMomentum.y) + "," +
                std::to_string(synchronized.angularMomentum.z);
        return false;
    }
    const Vec3 installedAngularMomentum{25.0F, -50.0F, 75.0F};
    networkWorld->setAngularMomentum(0U, installedAngularMomentum);
    const auto installedMomentum = networkWorld->vehicle();
    if (std::abs(installedMomentum.angularMomentum.x -
                 installedAngularMomentum.x) > 0.1F ||
        std::abs(installedMomentum.angularMomentum.y -
                 installedAngularMomentum.y) > 0.1F ||
        std::abs(installedMomentum.angularMomentum.z -
                 installedAngularMomentum.z) > 0.1F)
    {
        error = "GameCar clutch/oil angular momentum replacement failed";
        return false;
    }
    // Player::CarState exposes signed longitudinal GameCar::GetSpeed, not
    // total rigid-body velocity.  A purely lateral slide must therefore hit
    // the source one-metre dead zone so AICar can enter blocking recovery.
    networkWorld->synchronizeNetworkVehicle(
        0U, synchronized.body.position, {},
        {0.0F, sourceVehicle.mass * 8.0F, 0.0F}, {});
    const auto lateralSlide = networkWorld->vehicle();
    if (lateralSlide.speed != 0.0F ||
        std::abs(lateralSlide.linearVelocity.y - 8.0F) > 0.01F)
    {
        error = "Player::CarState signed longitudinal speed contract was "
                "not preserved for lateral motion";
        return false;
    }
    const Vec3 nearNetworkPosition{
        synchronized.body.position.x + 1.0F,
        synchronized.body.position.y,
        synchronized.body.position.z};
    networkWorld->synchronizeNetworkVehicle(
        0U, nearNetworkPosition, networkRotation,
        networkLinearMomentum, networkAngularMomentum);
    const auto nearSynchronized = networkWorld->vehicle();
    if (std::abs(nearSynchronized.body.position.x -
                 synchronized.body.position.x) > 0.001F ||
        std::abs(nearSynchronized.linearVelocity.x - 4.0F) > 0.01F)
    {
        error = "NetPlayer near-position momentum correction did not "
                "match dPos * 2 * mass";
        return false;
    }
    const Vec3 farNetworkPosition{
        synchronized.body.position.x + 5.0F,
        synchronized.body.position.y,
        synchronized.body.position.z};
    networkWorld->synchronizeNetworkVehicle(
        0U, farNetworkPosition, networkRotation, {}, {});
    const auto farSynchronized = networkWorld->vehicle();
    if (std::abs(farSynchronized.body.position.x -
                 farNetworkPosition.x) > 0.001F ||
        std::abs(farSynchronized.linearVelocity.x) > 0.001F)
    {
        error = "NetPlayer far-position snap did not match the source "
                "four-unit threshold";
        return false;
    }
    // NetPlayer compares the received rotation with GameCar's graph actor,
    // not with the PhysX body.  While a previous graph correction is still
    // being consumed, that comparison can cross pi/24 even when the next
    // authoritative body delta is smaller.  The game layer must be able to
    // carry that source decision through the Jolt boundary unchanged.
    constexpr float forcedNetworkYaw = networkYaw + 0.1F;
    const Quat forcedNetworkRotation{
        0.0F, 0.0F, std::sin(forcedNetworkYaw * 0.5F),
        std::cos(forcedNetworkYaw * 0.5F)};
    networkWorld->synchronizeNetworkVehicle(
        0U, farSynchronized.body.position, forcedNetworkRotation, {}, {},
        true);
    const auto forcedRotation = networkWorld->vehicle();
    const float forcedRotationDot = std::abs(
        forcedRotation.body.rotation.x * forcedNetworkRotation.x +
        forcedRotation.body.rotation.y * forcedNetworkRotation.y +
        forcedRotation.body.rotation.z * forcedNetworkRotation.z +
        forcedRotation.body.rotation.w * forcedNetworkRotation.w);
    if (forcedRotationDot < 0.999F)
    {
        error = "NetPlayer graph-rotation snap decision was lost at the "
                "Jolt boundary";
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
    const auto clutchLocked = world->vehicle();
    const float clutchLockedSpeed = clutchLocked.speed;
    const bool clutchContactForcePresent = std::any_of(
        clutchLocked.wheelContacts.begin(),
        clutchLocked.wheelContacts.end(),
        [](const WheelContactState& contact) {
            return contact.hasContact && contact.normalImpulse > 0.0001F;
        });
    if (clutchContactForcePresent)
    {
        error = "source clutch lock did not release wheel normal force";
        return false;
    }
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
        const auto& immune = immuneWorld->vehicle();
        error = "source clutchImmunity did not preserve tire traction: "
                "speed=" + std::to_string(immune.speed) +
                ", pos=" + std::to_string(immune.body.position.x) + "," +
                std::to_string(immune.body.position.y) + "," +
                std::to_string(immune.body.position.z);
        for (const auto& contact : immune.wheelContacts)
        {
            error += ", wheel=" +
                     std::to_string(contact.normalReaction) + "/" +
                     std::to_string(contact.normalImpulse);
        }
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
    float expectedDrivenWheelSpeed = 0.0F;
    for (std::size_t index = 0; index < sourceVehicle.wheels.size();
         ++index)
    {
        if (sourceVehicle.wheels[index].driven)
            continue;
        expectedDrivenWheelSpeed =
            accelerated.wheelAngularSpeeds[index] *
            sourceVehicle.wheels[index].radius;
        if (std::abs(expectedDrivenWheelSpeed) <= 0.1F)
            expectedDrivenWheelSpeed = 0.0F;
        break;
    }
    if (!std::isfinite(accelerated.drivenWheelSpeed) ||
        std::abs(accelerated.drivenWheelSpeed -
                 expectedDrivenWheelSpeed) > 0.001F)
    {
        error = "GameCar::GetDrivenWheelSpeed did not expose the first "
                "non-lead wheel";
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
        world->vehicle().drivenWheelSpeed != 0.0F ||
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
            spawn.vehicle.tireSpring = 0.0F;
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
    contactDescription.vehicle.tireSpring = 0.0F;
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
    DecorationDescription barrel;
    barrel.transform.position = {0.0F, -10.0F, 4.0F};
    barrel.shapePosition = {0.0F, 0.0F, 0.5F};
    barrel.halfExtents = {0.5F, 0.5F, 0.75F};
    barrel.mass = 200.0F;
    barrel.hasBodyShape = true;
    barrel.dynamic = true;
    contactDescription.decorations.push_back(barrel);
    DecorationDescription destructible;
    destructible.transform.position = {2.0F, 0.0F, 0.0F};
    destructible.collisionResponse = false;
    DecorationDescription::ChildShape attachedBoard;
    attachedBoard.position = {0.0F, 0.0F, 1.5F};
    attachedBoard.halfExtents = {0.1F, 2.0F, 1.5F};
    destructible.childShapes.push_back(attachedBoard);
    contactDescription.decorations.push_back(destructible);
    auto contactWorld =
        createOriginalVehicleWorld(contactDescription, error);
    if (!contactWorld)
        return false;
    if (contactWorld->decorationCount() != 2U ||
        !contactWorld->decoration(0U).active ||
        !contactWorld->decoration(1U).active)
    {
        error = "source ctDecoration bodies were not created independently";
        return false;
    }
    const float barrelStartHeight =
        contactWorld->decoration(0U).body.position.z;
    input = {};
    for (int step = 0; step < 120; ++step)
        contactWorld->step(1.0F / 120.0F, input);
    if (contactWorld->decoration(0U).body.position.z >=
        barrelStartHeight - 0.1F)
    {
        error = "source dynamic decoration did not enter Jolt physics";
        return false;
    }
    contactWorld->setDecorationEnabled(0U, false);
    contactWorld->setDecorationEnabled(1U, false);
    if (contactWorld->decoration(0U).active ||
        contactWorld->decoration(1U).active)
    {
        error = "destroyed ctDecoration collision remained enabled";
        return false;
    }
    contactWorld->reset();
    if (!contactWorld->decoration(0U).active ||
        !contactWorld->decoration(1U).active ||
        std::abs(contactWorld->decoration(0U).body.position.z -
                 barrelStartHeight) > 0.001F)
    {
        error = "race reset did not restore source ctDecoration bodies";
        return false;
    }
    input = {};
    input.throttle = 1.0F;
    bool sawBorderContact = false;
    bool sawDestructibleSensor = false;
    for (int step = 0;
         step < 1200 && (!sawBorderContact || !sawDestructibleSensor);
         ++step)
    {
        contactWorld->step(1.0F / 120.0F, input);
        for (const auto& contact : contactWorld->vehicle().bodyContacts)
        {
            sawDestructibleSensor =
                sawDestructibleSensor ||
                (contact.surface == CollisionSurface::Decoration &&
                 contact.otherDecoration == 1U && contact.hasPoint);
            sawBorderContact = sawBorderContact ||
                (contact.surface ==
                           CollisionSurface::TrackBorder &&
                       contact.normalSpeed > 0.0F &&
                       contact.force > 0.0F &&
                       contact.otherActor !=
                           std::numeric_limits<std::uint32_t>::max() &&
                       contact.hasPoint &&
                       !contact.points.empty() &&
                       std::isfinite(contact.frictionForce) &&
                       contact.frictionForce >= 0.0F &&
                       std::isfinite(contact.frictionForceVector.x) &&
                       std::isfinite(contact.frictionForceVector.y) &&
                       std::isfinite(contact.frictionForceVector.z) &&
                       std::abs(
                           std::sqrt(
                               contact.frictionForceVector.x *
                                   contact.frictionForceVector.x +
                               contact.frictionForceVector.y *
                                   contact.frictionForceVector.y +
                               contact.frictionForceVector.z *
                                   contact.frictionForceVector.z) -
                           contact.frictionForce) <=
                           std::max(1.0F,
                                    contact.frictionForce * 0.001F));
        }
    }
    if (!sawBorderContact)
    {
        error =
            "Jolt body contact listener did not preserve track-border "
            "surface metadata";
        return false;
    }
    if (!sawDestructibleSensor)
    {
        error =
            "source NX_AF_DISABLE_RESPONSE destruct-list box did not report "
            "its owning MapObj contact";
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
    DebrisDescription staticPieceDescription;
    staticPieceDescription.transform.position = {15.0F, -15.0F, 0.0F};
    staticPieceDescription.dynamic = false;
    TriangleMesh staticPieceMesh;
    staticPieceMesh.surface = CollisionSurface::Decoration;
    staticPieceMesh.vertices = {{-1.0F, -1.0F, 0.0F},
                                {1.0F, -1.0F, 0.0F},
                                {1.0F, 1.0F, 0.0F},
                                {-1.0F, 1.0F, 0.0F}};
    staticPieceMesh.indices = {0U, 1U, 2U, 0U, 2U, 3U};
    staticPieceDescription.collisionMeshes.push_back(
        std::move(staticPieceMesh));
    const std::size_t staticPieceIndex =
        contactWorld->addDebris(staticPieceDescription);
    if (staticPieceIndex == std::numeric_limits<std::size_t>::max() ||
        !contactWorld->debris(staticPieceIndex).active)
    {
        error = "source static destruction-list body was not created";
        return false;
    }
    const auto staticPieceStart =
        contactWorld->debris(staticPieceIndex).body.position;
    contactWorld->step(0.1F, input);
    const auto staticPieceEnd =
        contactWorld->debris(staticPieceIndex).body.position;
    if (std::abs(staticPieceStart.x - staticPieceEnd.x) > 0.001F ||
        std::abs(staticPieceStart.y - staticPieceEnd.y) > 0.001F ||
        std::abs(staticPieceStart.z - staticPieceEnd.z) > 0.001F)
    {
        error = "source static destruction-list body entered dynamics";
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
