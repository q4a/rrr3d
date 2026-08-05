#include "OriginalRaceSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <numeric>
#include <stdexcept>

namespace r3d::game::originalrace
{
namespace
{

Vec3 subtract(Vec3 first, Vec3 second)
{
    return {first.x - second.x, first.y - second.y, first.z - second.z};
}

Vec3 add(Vec3 first, Vec3 second)
{
    return {first.x + second.x, first.y + second.y, first.z + second.z};
}

Vec3 multiply(Vec3 value, float scale)
{
    return {value.x * scale, value.y * scale, value.z * scale};
}

struct EffectTiming
{
    float emissionSeconds = 0.0F;
    float visibleSeconds = 0.0F;
};

EffectTiming sourceEffectTiming(const ObjectDefinition& definition,
                                float fallbackEmissionSeconds)
{
    EffectTiming result;
    result.emissionSeconds =
        definition.maximumTimeLife > 0.0F
            ? definition.maximumTimeLife
            : std::max(fallbackEmissionSeconds, 0.0F);
    result.visibleSeconds = result.emissionSeconds;
    for (const auto& emitter : definition.particleEmitters)
    {
        if (!emitter.waitForParticleEnd)
            continue;
        const float emissionEnd =
            emitter.emissionDuration > 0.0F
                ? emitter.emissionDuration
                : result.emissionSeconds;
        const float lastBirth =
            emitter.startDuration > 0.0F
                ? std::min(emissionEnd, emitter.startDuration)
                : emissionEnd;
        const float particleLife = std::max(
            emitter.lifeMaximum + emitter.rangeLifeMaximum, 0.0F);
        result.visibleSeconds = std::max(
            result.visibleSeconds, lastBirth + particleLife);
    }
    return result;
}

Vec3 cross(Vec3 first, Vec3 second)
{
    return {first.y * second.z - first.z * second.y,
            first.z * second.x - first.x * second.z,
            first.x * second.y - first.y * second.x};
}

float dot2(Vec3 first, Vec3 second)
{
    return first.x * second.x + first.y * second.y;
}

float dot3(Vec3 first, Vec3 second)
{
    return dot2(first, second) + first.z * second.z;
}

float length2(Vec3 value)
{
    return std::sqrt(dot2(value, value));
}

float length3(Vec3 value)
{
    return std::sqrt(dot3(value, value));
}

Vec3 normalized2(Vec3 value)
{
    const float length = length2(value);
    if (length <= 0.0001F)
        return {1.0F, 0.0F, 0.0F};
    return {value.x / length, value.y / length, 0.0F};
}

Vec3 normalized3(Vec3 value)
{
    const float length = length3(value);
    if (length <= 0.0001F)
        return {1.0F, 0.0F, 0.0F};
    return {value.x / length, value.y / length, value.z / length};
}

Vec3 rotate(const Quat& q, Vec3 value)
{
    const Vec3 twiceCross{
        2.0F * (q.y * value.z - q.z * value.y),
        2.0F * (q.z * value.x - q.x * value.z),
        2.0F * (q.x * value.y - q.y * value.x)};
    return {value.x + q.w * twiceCross.x +
                        (q.y * twiceCross.z - q.z * twiceCross.y),
            value.y + q.w * twiceCross.y +
                        (q.z * twiceCross.x - q.x * twiceCross.z),
            value.z + q.w * twiceCross.z +
                        (q.x * twiceCross.y - q.y * twiceCross.x)};
}

Quat multiply(const Quat& first, const Quat& second)
{
    return {first.w * second.x + first.x * second.w +
                first.y * second.z - first.z * second.y,
            first.w * second.y - first.x * second.z +
                first.y * second.w + first.z * second.x,
            first.w * second.z + first.x * second.y -
                first.y * second.x + first.z * second.w,
            first.w * second.w - first.x * second.x -
                first.y * second.y - first.z * second.z};
}

Quat normalizedQuaternion(Quat value)
{
    const float length = std::sqrt(
        value.x * value.x + value.y * value.y +
        value.z * value.z + value.w * value.w);
    if (length <= 0.000001F)
        return {};
    return {value.x / length, value.y / length,
            value.z / length, value.w / length};
}

Quat shortestArcFromX(Vec3 direction)
{
    direction = normalized3(direction);
    const float dot = std::clamp(direction.x, -1.0F, 1.0F);
    if (dot < -0.999999F)
        return {0.0F, 0.0F, 1.0F, 0.0F};
    const float scale = std::sqrt((1.0F + dot) * 2.0F);
    const float inverseScale =
        scale > 0.000001F ? 1.0F / scale : 0.0F;
    return normalizedQuaternion(
        {0.0F, -direction.z * inverseScale,
         direction.y * inverseScale, scale * 0.5F});
}

Quat quaternionSlerp(Quat first, Quat second, float alpha)
{
    first = normalizedQuaternion(first);
    second = normalizedQuaternion(second);
    float dot = first.x * second.x + first.y * second.y +
                first.z * second.z + first.w * second.w;
    if (dot < 0.0F)
    {
        second = {-second.x, -second.y, -second.z, -second.w};
        dot = -dot;
    }
    dot = std::clamp(dot, -1.0F, 1.0F);
    alpha = std::clamp(alpha, 0.0F, 1.0F);
    if (dot > 0.9995F)
    {
        return normalizedQuaternion(
            {first.x + (second.x - first.x) * alpha,
             first.y + (second.y - first.y) * alpha,
             first.z + (second.z - first.z) * alpha,
             first.w + (second.w - first.w) * alpha});
    }
    const float angle = std::acos(dot);
    const float sine = std::sin(angle);
    const float firstWeight =
        std::sin((1.0F - alpha) * angle) / sine;
    const float secondWeight =
        std::sin(alpha * angle) / sine;
    return normalizedQuaternion(
        {first.x * firstWeight + second.x * secondWeight,
         first.y * firstWeight + second.y * secondWeight,
         first.z * firstWeight + second.z * secondWeight,
         first.w * firstWeight + second.w * secondWeight});
}

Transform compose(const Transform& parent, const Transform& local)
{
    Transform result;
    const auto offset = rotate(
        parent.rotation,
        {local.position.x * parent.scale.x,
         local.position.y * parent.scale.y,
         local.position.z * parent.scale.z});
    result.position = add(parent.position, offset);
    result.scale = {parent.scale.x * local.scale.x,
                    parent.scale.y * local.scale.y,
                    parent.scale.z * local.scale.z};
    result.rotation = multiply(parent.rotation, local.rotation);
    return result;
}

Vec3 transformPoint(const Transform& transform, Vec3 point)
{
    return add(
        transform.position,
        rotate(transform.rotation,
               {point.x * transform.scale.x,
                point.y * transform.scale.y,
                point.z * transform.scale.z}));
}

Quat quaternionFromAxes(Vec3 direction, Vec3 right, Vec3 up)
{
    const float m00 = direction.x;
    const float m01 = right.x;
    const float m02 = up.x;
    const float m10 = direction.y;
    const float m11 = right.y;
    const float m12 = up.y;
    const float m20 = direction.z;
    const float m21 = right.z;
    const float m22 = up.z;
    Quat result;
    const float trace = m00 + m11 + m22;
    if (trace > 0.0F)
    {
        const float scale = std::sqrt(trace + 1.0F) * 2.0F;
        result.w = 0.25F * scale;
        result.x = (m21 - m12) / scale;
        result.y = (m02 - m20) / scale;
        result.z = (m10 - m01) / scale;
    }
    else if (m00 > m11 && m00 > m22)
    {
        const float scale =
            std::sqrt(1.0F + m00 - m11 - m22) * 2.0F;
        result.w = (m21 - m12) / scale;
        result.x = 0.25F * scale;
        result.y = (m01 + m10) / scale;
        result.z = (m02 + m20) / scale;
    }
    else if (m11 > m22)
    {
        const float scale =
            std::sqrt(1.0F + m11 - m00 - m22) * 2.0F;
        result.w = (m02 - m20) / scale;
        result.x = (m01 + m10) / scale;
        result.y = 0.25F * scale;
        result.z = (m12 + m21) / scale;
    }
    else
    {
        const float scale =
            std::sqrt(1.0F + m22 - m00 - m11) * 2.0F;
        result.w = (m10 - m01) / scale;
        result.x = (m02 + m20) / scale;
        result.y = (m12 + m21) / scale;
        result.z = 0.25F * scale;
    }
    return result;
}

Quat rotationWithUp(Vec3 normal)
{
    const Vec3 up = normalized3(normal);
    Vec3 right = subtract(
        {0.0F, 1.0F, 0.0F},
        multiply(up, dot3({0.0F, 1.0F, 0.0F}, up)));
    if (length3(right) <= 0.0001F)
    {
        right = subtract(
            {1.0F, 0.0F, 0.0F},
            multiply(up, dot3({1.0F, 0.0F, 0.0F}, up)));
    }
    right = normalized3(right);
    const Vec3 direction = normalized3(cross(right, up));
    right = normalized3(cross(up, direction));
    return quaternionFromAxes(direction, right, up);
}

struct TrackRayHit
{
    Vec3 position;
    Vec3 normal;
    float distance = std::numeric_limits<float>::max();
    bool hit = false;
};

TrackRayHit raycastTrackPlane(const Race& race, Vec3 origin)
{
    const Vec3 rayDirection{0.0F, 0.0F, -1.0F};
    TrackRayHit result;
    for (const auto& mesh : race.collisionMeshes)
    {
        if (mesh.surface !=
            r3d::physics::CollisionSurface::TrackPlane)
            continue;
        for (std::size_t index = 0;
             index + 2U < mesh.indices.size(); index += 3U)
        {
            const auto firstIndex = mesh.indices[index];
            const auto secondIndex = mesh.indices[index + 1U];
            const auto thirdIndex = mesh.indices[index + 2U];
            if (firstIndex >= mesh.vertices.size() ||
                secondIndex >= mesh.vertices.size() ||
                thirdIndex >= mesh.vertices.size())
                continue;
            const Vec3 first =
                transformPoint(mesh.transform, mesh.vertices[firstIndex]);
            const Vec3 second =
                transformPoint(mesh.transform, mesh.vertices[secondIndex]);
            const Vec3 third =
                transformPoint(mesh.transform, mesh.vertices[thirdIndex]);
            const Vec3 firstEdge = subtract(second, first);
            const Vec3 secondEdge = subtract(third, first);
            const Vec3 determinantAxis =
                cross(rayDirection, secondEdge);
            const float determinant =
                dot3(firstEdge, determinantAxis);
            if (std::abs(determinant) <= 0.000001F)
                continue;
            const float inverseDeterminant = 1.0F / determinant;
            const Vec3 fromFirst = subtract(origin, first);
            const float u =
                dot3(fromFirst, determinantAxis) * inverseDeterminant;
            if (u < 0.0F || u > 1.0F)
                continue;
            const Vec3 coordinateAxis = cross(fromFirst, firstEdge);
            const float v =
                dot3(rayDirection, coordinateAxis) *
                inverseDeterminant;
            if (v < 0.0F || u + v > 1.0F)
                continue;
            const float distance =
                dot3(secondEdge, coordinateAxis) *
                inverseDeterminant;
            if (distance < 0.0F || distance >= result.distance)
                continue;
            result.hit = true;
            result.distance = distance;
            result.position =
                add(origin, multiply(rayDirection, distance));
            result.normal =
                normalized3(cross(firstEdge, secondEdge));
        }
    }
    return result;
}

Vec3 forward(const Quat& q)
{
    return {1.0F - 2.0F * (q.y * q.y + q.z * q.z),
            2.0F * (q.x * q.y + q.w * q.z),
            2.0F * (q.x * q.z - q.w * q.y)};
}

float distanceSquared(Vec3 first, Vec3 second)
{
    const auto difference = subtract(first, second);
    return difference.x * difference.x + difference.y * difference.y +
           difference.z * difference.z;
}

struct OrientedBox
{
    Vec3 center;
    std::array<Vec3, 3> axes;
    std::array<float, 3> halfExtents{};
};

OrientedBox orientedBox(Transform transform,
                        ProjectileCollisionBox collision)
{
    const Vec3 localCenter{
        collision.center.x * transform.scale.x,
        collision.center.y * transform.scale.y,
        collision.center.z * transform.scale.z};
    OrientedBox result;
    result.center = add(transform.position,
                        rotate(transform.rotation, localCenter));
    result.axes = {
        normalized3(rotate(transform.rotation, {1.0F, 0.0F, 0.0F})),
        normalized3(rotate(transform.rotation, {0.0F, 1.0F, 0.0F})),
        normalized3(rotate(transform.rotation, {0.0F, 0.0F, 1.0F}))};
    result.halfExtents = {
        std::abs(collision.halfExtents.x * transform.scale.x),
        std::abs(collision.halfExtents.y * transform.scale.y),
        std::abs(collision.halfExtents.z * transform.scale.z)};
    return result;
}

OrientedBox vehicleBox(
    const r3d::physics::VehicleState& state,
    const r3d::physics::VehicleDescription& description)
{
    Transform transform = state.body;
    transform.position = add(
        state.body.position,
        rotate(state.body.rotation, description.shapePosition));
    ProjectileCollisionBox collision;
    collision.halfExtents = description.halfExtents;
    return orientedBox(transform, collision);
}

bool hasBox(const ProjectileCollisionBox& collision)
{
    return collision.halfExtents.x > 0.0F &&
           collision.halfExtents.y > 0.0F &&
           collision.halfExtents.z > 0.0F;
}

OrientedBox decorationBox(const ObjectInstance& instance,
                          const ObjectDefinition& definition)
{
    Transform local;
    local.position = definition.bodyShapePosition;
    local.rotation = definition.bodyShapeRotation;
    ProjectileCollisionBox collision;
    collision.halfExtents = definition.bodyHalfExtents;
    return orientedBox(compose(instance.transform, local), collision);
}

Vec3 closestPoint(const OrientedBox& box, Vec3 point)
{
    Vec3 result = box.center;
    const Vec3 difference = subtract(point, box.center);
    for (std::size_t axis = 0; axis < box.axes.size(); ++axis)
    {
        const float distance = std::clamp(
            dot3(difference, box.axes[axis]),
            -box.halfExtents[axis], box.halfExtents[axis]);
        result = add(result, multiply(box.axes[axis], distance));
    }
    return result;
}

bool boxesOverlap(const OrientedBox& first, const OrientedBox& second)
{
    // Separating Axis Theorem for the same oriented boxes PhysX receives
    // from Proj::CreatePxBox and the vehicle body shape.
    float rotation[3][3]{};
    float absoluteRotation[3][3]{};
    for (std::size_t i = 0; i < 3U; ++i)
    {
        for (std::size_t j = 0; j < 3U; ++j)
        {
            rotation[i][j] = dot3(first.axes[i], second.axes[j]);
            absoluteRotation[i][j] =
                std::abs(rotation[i][j]) + 1.0e-6F;
        }
    }

    const Vec3 centerDifference =
        subtract(second.center, first.center);
    const std::array<float, 3> translation{
        dot3(centerDifference, first.axes[0]),
        dot3(centerDifference, first.axes[1]),
        dot3(centerDifference, first.axes[2])};

    for (std::size_t i = 0; i < 3U; ++i)
    {
        const float secondRadius =
            second.halfExtents[0] * absoluteRotation[i][0] +
            second.halfExtents[1] * absoluteRotation[i][1] +
            second.halfExtents[2] * absoluteRotation[i][2];
        if (std::abs(translation[i]) >
            first.halfExtents[i] + secondRadius)
            return false;
    }
    for (std::size_t j = 0; j < 3U; ++j)
    {
        const float firstRadius =
            first.halfExtents[0] * absoluteRotation[0][j] +
            first.halfExtents[1] * absoluteRotation[1][j] +
            first.halfExtents[2] * absoluteRotation[2][j];
        const float projected =
            std::abs(translation[0] * rotation[0][j] +
                     translation[1] * rotation[1][j] +
                     translation[2] * rotation[2][j]);
        if (projected > firstRadius + second.halfExtents[j])
            return false;
    }

    for (std::size_t i = 0; i < 3U; ++i)
    {
        const std::size_t nextI = (i + 1U) % 3U;
        const std::size_t lastI = (i + 2U) % 3U;
        for (std::size_t j = 0; j < 3U; ++j)
        {
            const std::size_t nextJ = (j + 1U) % 3U;
            const std::size_t lastJ = (j + 2U) % 3U;
            const float firstRadius =
                first.halfExtents[nextI] *
                    absoluteRotation[lastI][j] +
                first.halfExtents[lastI] *
                    absoluteRotation[nextI][j];
            const float secondRadius =
                second.halfExtents[nextJ] *
                    absoluteRotation[i][lastJ] +
                second.halfExtents[lastJ] *
                    absoluteRotation[i][nextJ];
            const float projected = std::abs(
                translation[lastI] * rotation[nextI][j] -
                translation[nextI] * rotation[lastI][j]);
            if (projected > firstRadius + secondRadius)
                return false;
        }
    }
    return true;
}

bool raycastBox(Vec3 origin, Vec3 direction, float maximumDistance,
                const OrientedBox& box, float& distance)
{
    const Vec3 fromCenter = subtract(origin, box.center);
    float nearDistance = 0.0F;
    float farDistance = maximumDistance;
    for (std::size_t axis = 0; axis < 3U; ++axis)
    {
        const float originCoordinate =
            dot3(fromCenter, box.axes[axis]);
        const float directionCoordinate =
            dot3(direction, box.axes[axis]);
        if (std::abs(directionCoordinate) <= 0.000001F)
        {
            if (std::abs(originCoordinate) > box.halfExtents[axis])
                return false;
            continue;
        }
        float first =
            (-box.halfExtents[axis] - originCoordinate) /
            directionCoordinate;
        float second =
            (box.halfExtents[axis] - originCoordinate) /
            directionCoordinate;
        if (first > second)
            std::swap(first, second);
        nearDistance = std::max(nearDistance, first);
        farDistance = std::min(farDistance, second);
        if (nearDistance > farDistance)
            return false;
    }
    if (farDistance < 0.0F || nearDistance > maximumDistance)
        return false;
    distance = std::max(nearDistance, 0.0F);
    return true;
}

bool raycastTriangle(Vec3 origin, Vec3 direction, float maximumDistance,
                     Vec3 first, Vec3 second, Vec3 third,
                     float& distance)
{
    const Vec3 firstEdge = subtract(second, first);
    const Vec3 secondEdge = subtract(third, first);
    const Vec3 determinantAxis = cross(direction, secondEdge);
    const float determinant = dot3(firstEdge, determinantAxis);
    if (std::abs(determinant) <= 0.000001F)
        return false;
    const float inverseDeterminant = 1.0F / determinant;
    const Vec3 fromFirst = subtract(origin, first);
    const float u =
        dot3(fromFirst, determinantAxis) * inverseDeterminant;
    if (u < 0.0F || u > 1.0F)
        return false;
    const Vec3 coordinateAxis = cross(fromFirst, firstEdge);
    const float v =
        dot3(direction, coordinateAxis) * inverseDeterminant;
    if (v < 0.0F || u + v > 1.0F)
        return false;
    const float hitDistance =
        dot3(secondEdge, coordinateAxis) * inverseDeterminant;
    if (hitDistance < 0.0F || hitDistance > maximumDistance)
        return false;
    distance = hitDistance;
    return true;
}

bool triangleOverlapsBox(Vec3 first, Vec3 second, Vec3 third,
                         const OrientedBox& box)
{
    const std::array<Vec3, 3> edges{
        subtract(second, first), subtract(third, second),
        subtract(first, third)};
    std::array<Vec3, 13> axes{
        box.axes[0], box.axes[1], box.axes[2],
        cross(edges[0], edges[1])};
    std::size_t axis = 4U;
    for (const auto& edge : edges)
    {
        for (const auto& boxAxis : box.axes)
            axes[axis++] = cross(edge, boxAxis);
    }
    for (const auto& testAxis : axes)
    {
        if (dot3(testAxis, testAxis) <= 0.0000001F)
            continue;
        const float firstProjection = dot3(first, testAxis);
        const float secondProjection = dot3(second, testAxis);
        const float thirdProjection = dot3(third, testAxis);
        const float minimum = std::min(
            {firstProjection, secondProjection, thirdProjection});
        const float maximum = std::max(
            {firstProjection, secondProjection, thirdProjection});
        const float center = dot3(box.center, testAxis);
        const float radius =
            box.halfExtents[0] *
                std::abs(dot3(box.axes[0], testAxis)) +
            box.halfExtents[1] *
                std::abs(dot3(box.axes[1], testAxis)) +
            box.halfExtents[2] *
                std::abs(dot3(box.axes[2], testAxis));
        if (maximum < center - radius ||
            minimum > center + radius)
            return false;
    }
    return true;
}

bool trackBorderContact(const Race& race, const OrientedBox& box,
                        Vec3& normal)
{
    for (const auto& mesh : race.collisionMeshes)
    {
        if (mesh.surface !=
            r3d::physics::CollisionSurface::TrackBorder)
            continue;
        for (std::size_t index = 0;
             index + 2U < mesh.indices.size(); index += 3U)
        {
            const auto firstIndex = mesh.indices[index];
            const auto secondIndex = mesh.indices[index + 1U];
            const auto thirdIndex = mesh.indices[index + 2U];
            if (firstIndex >= mesh.vertices.size() ||
                secondIndex >= mesh.vertices.size() ||
                thirdIndex >= mesh.vertices.size())
                continue;
            const Vec3 first = transformPoint(
                mesh.transform, mesh.vertices[firstIndex]);
            const Vec3 second = transformPoint(
                mesh.transform, mesh.vertices[secondIndex]);
            const Vec3 third = transformPoint(
                mesh.transform, mesh.vertices[thirdIndex]);
            if (!triangleOverlapsBox(first, second, third, box))
                continue;
            normal = normalized3(cross(
                subtract(second, first), subtract(third, first)));
            return true;
        }
    }
    return false;
}

Vec3 thunderReflection(Vec3 velocity, Vec3 normal)
{
    const float speed = length3(velocity);
    if (speed <= 0.0001F)
        return velocity;
    normal = normalized3(normal);
    const float angle = dot3(multiply(velocity, 1.0F / speed), normal);
    if (std::abs(angle) > 0.1F)
    {
        return subtract(
            velocity, multiply(normal, 2.0F * dot3(velocity, normal)));
    }
    return multiply(velocity, -1.0F);
}

struct WorldRayHit
{
    float distance = std::numeric_limits<float>::max();
    std::size_t vehicle = RacerRuntime::invalidWeapon;
    bool hit = false;
};

WorldRayHit raycastWorld(
    const Race& race,
    const std::vector<r3d::physics::VehicleState>& vehicles,
    const std::vector<RacerRuntime>& racers, std::size_t ignoredVehicle,
    Vec3 origin, Vec3 direction, float maximumDistance)
{
    WorldRayHit result;
    result.distance = maximumDistance;
    direction = normalized3(direction);
    for (const auto& mesh : race.collisionMeshes)
    {
        for (std::size_t index = 0;
             index + 2U < mesh.indices.size(); index += 3U)
        {
            const auto firstIndex = mesh.indices[index];
            const auto secondIndex = mesh.indices[index + 1U];
            const auto thirdIndex = mesh.indices[index + 2U];
            if (firstIndex >= mesh.vertices.size() ||
                secondIndex >= mesh.vertices.size() ||
                thirdIndex >= mesh.vertices.size())
                continue;
            float distance = result.distance;
            if (!raycastTriangle(
                    origin, direction, result.distance,
                    transformPoint(mesh.transform,
                                   mesh.vertices[firstIndex]),
                    transformPoint(mesh.transform,
                                   mesh.vertices[secondIndex]),
                    transformPoint(mesh.transform,
                                   mesh.vertices[thirdIndex]),
                    distance))
                continue;
            result.hit = true;
            result.distance = distance;
            result.vehicle = RacerRuntime::invalidWeapon;
        }
    }
    for (std::size_t vehicle = 0;
         vehicle < vehicles.size() && vehicle < racers.size(); ++vehicle)
    {
        if (vehicle == ignoredVehicle || racers[vehicle].destroyed ||
            racers[vehicle].finished)
            continue;
        const auto& racer = race.racers.at(vehicle);
        const auto& definition =
            racer.hasConfiguredVehicle
                ? racer.configuredVehicle
                : race.vehicles.at(racer.vehicle);
        float distance = result.distance;
        if (!raycastBox(
                origin, direction, result.distance,
                vehicleBox(vehicles[vehicle], definition.physics),
                distance))
            continue;
        result.hit = true;
        result.distance = distance;
        result.vehicle = vehicle;
    }
    return result;
}

enum class ResetRayKind
{
    None,
    TrackPlane,
    DeathPlane,
    OwnVehicle,
    Blocked,
};

struct ResetRayHit
{
    ResetRayKind kind = ResetRayKind::None;
    float distance = std::numeric_limits<float>::max();
};

ResetRayHit raycastResetWorld(
    const Race& race, const std::vector<bool>& decorationActive,
    const std::vector<r3d::physics::VehicleState>& vehicles,
    const std::vector<RacerRuntime>& racers, std::size_t ownVehicle,
    Vec3 origin)
{
    constexpr Vec3 direction{0.0F, 0.0F, -1.0F};
    ResetRayHit result;
    // Map::Map installs an infinite +Z TouchDeath plane at world Z=0.
    if (origin.z >= 0.0F)
    {
        result.kind = ResetRayKind::DeathPlane;
        result.distance = origin.z;
    }
    for (std::size_t meshIndex = 0;
         meshIndex < race.collisionMeshes.size(); ++meshIndex)
    {
        const auto decoration =
            meshIndex < race.collisionMeshDecorationInstances.size()
                ? race.collisionMeshDecorationInstances[meshIndex]
                : RacerRuntime::invalidWeapon;
        if (decoration != RacerRuntime::invalidWeapon &&
            decoration < decorationActive.size() &&
            !decorationActive[decoration])
        {
            continue;
        }
        const auto& mesh = race.collisionMeshes[meshIndex];
        for (std::size_t index = 0;
             index + 2U < mesh.indices.size(); index += 3U)
        {
            const auto firstIndex = mesh.indices[index];
            const auto secondIndex = mesh.indices[index + 1U];
            const auto thirdIndex = mesh.indices[index + 2U];
            if (firstIndex >= mesh.vertices.size() ||
                secondIndex >= mesh.vertices.size() ||
                thirdIndex >= mesh.vertices.size())
            {
                continue;
            }
            float distance = result.distance;
            if (!raycastTriangle(
                    origin, direction, result.distance,
                    transformPoint(
                        mesh.transform, mesh.vertices[firstIndex]),
                    transformPoint(
                        mesh.transform, mesh.vertices[secondIndex]),
                    transformPoint(
                        mesh.transform, mesh.vertices[thirdIndex]),
                    distance))
            {
                continue;
            }
            result.distance = distance;
            result.kind =
                mesh.surface ==
                        r3d::physics::CollisionSurface::TrackPlane
                    ? ResetRayKind::TrackPlane
                    : ResetRayKind::Blocked;
        }
    }
    for (std::size_t vehicle = 0;
         vehicle < vehicles.size() && vehicle < race.racers.size();
         ++vehicle)
    {
        if (vehicle != ownVehicle && vehicle < racers.size() &&
            racers[vehicle].destroyed)
        {
            continue;
        }
        const auto& source = race.racers[vehicle];
        const auto& definition =
            source.hasConfiguredVehicle
                ? source.configuredVehicle
                : race.vehicles.at(source.vehicle);
        float distance = result.distance;
        if (!raycastBox(
                origin, direction, result.distance,
                vehicleBox(vehicles[vehicle], definition.physics),
                distance))
        {
            continue;
        }
        result.distance = distance;
        result.kind =
            vehicle == ownVehicle ? ResetRayKind::OwnVehicle
                                  : ResetRayKind::Blocked;
    }
    return result;
}

float clampSteering(float value)
{
    return std::clamp(value, -1.0F, 1.0F);
}

float sourceRandomUnit()
{
    return static_cast<float>(std::rand()) /
           static_cast<float>(RAND_MAX);
}

double sourceUniformRandomUnit()
{
    // lsl::RandomRange(int, int) uses RAND_MAX + 1, unlike Random().
    // The resulting unit value never reaches one and gives every inclusive
    // integer result the same-width bucket.
    return static_cast<double>(std::rand()) /
           (static_cast<double>(RAND_MAX) + 1.0);
}

std::size_t sourceUniformRandomIndex(
    std::size_t count, double randomUnit)
{
    if (count <= 1U)
        return 0U;
    const double unit = std::clamp(
        randomUnit, 0.0, std::nextafter(1.0, 0.0));
    return std::min(
        static_cast<std::size_t>(
            std::floor(unit * static_cast<double>(count))),
        count - 1U);
}

std::size_t sourceRoundedRandomIndex(
    std::size_t count, float randomUnit)
{
    if (count <= 1U)
        return 0U;
    const float value =
        static_cast<float>(count - 1U) *
        std::clamp(randomUnit, 0.0F, 1.0F);
    const float floorValue = std::floor(value);
    const float rounded =
        value - 0.5F < floorValue
            ? floorValue
            : floorValue + 1.0F;
    return std::min(
        static_cast<std::size_t>(rounded), count - 1U);
}

std::uint32_t sourceBonusCharge(
    std::uint32_t maximumCharge, float value)
{
    return static_cast<std::uint32_t>(
        std::max(
            static_cast<float>(maximumCharge) * value,
            1.0F));
}

DamageType sourceProjectileDamageType(std::uint32_t type)
{
    switch (type)
    {
    case 3U:  // ptLaser
    case 16U: // ptSonar
    case 18U: // ptFrostRay
    case 21U: // ptImpulse
        return DamageType::Energy;
    case 11U: // ptMine
    case 12U: // ptMineRip
    case 13U: // ptMinePiece
    case 20U: // ptCrater
    case 24U: // ptMineProton
        return DamageType::Mine;
    default:
        return DamageType::Simple;
    }
}

float sampleSourceRange(float minimum, float maximum)
{
    return minimum +
           (std::max(maximum, minimum) - minimum) *
               sourceRandomUnit();
}

Vec3 sourceMineRipFragmentVelocity()
{
    // Weapon.cpp uses Vec3Range((-3,-3,3), (3,3,1), vdVolume)
    // with the default 100^3 grid, normalizes it, then applies a
    // mass*dir*10 NX_IMPULSE.  The mass cancels, leaving dir*10 as
    // the fragment's linear-velocity change.
    constexpr std::uint32_t frequency = 100U;
    constexpr std::uint32_t volume =
        frequency * frequency * frequency;
    const float random = sourceRandomUnit();
    const std::uint32_t cellIndex =
        random >= 1.0F
            ? volume - 1U
            : static_cast<std::uint32_t>(
                  static_cast<float>(volume) * random);
    const std::uint32_t cellX = cellIndex % frequency;
    const std::uint32_t cellY =
        (cellIndex / frequency) % frequency;
    const std::uint32_t cellZ =
        (cellIndex / (frequency * frequency)) % frequency;
    const Vec3 direction{
        -3.0F + 6.0F * static_cast<float>(cellX) / 99.0F,
        -3.0F + 6.0F * static_cast<float>(cellY) / 99.0F,
        3.0F - 2.0F * static_cast<float>(cellZ) / 99.0F};
    return multiply(normalized3(direction), 10.0F);
}

std::string_view recordName(std::string_view value)
{
    const auto slash = value.find_last_of("\\/");
    return slash == std::string_view::npos ? value
                                            : value.substr(slash + 1U);
}

std::string workshopReference(std::string_view record)
{
    return "world\\race\\workshopRoot\\workshop\\" +
           std::string(record);
}

} // namespace

OriginalRaceSession::OriginalRaceSession(const Race& race) : race_(race)
{
    if (race_.tracePath.size() < 2 || race_.tracePoints.empty() ||
        race_.racers.empty())
        throw std::invalid_argument("Original race session data is incomplete");
    reset();
}

void OriginalRaceSession::reset()
{
    phase_ = RacePhase::Countdown;
    phaseBeforePause_ = phase_;
    countdownSeconds_ = 3.0F;
    countdownDisplay_ = 3;
    elapsedSeconds_ = 0.0F;
    finishSecondsRemaining_ = -1.0F;
    racers_.assign(race_.racers.size(), {});
    vehicleInputs_.assign(race_.racers.size(), {});
    weaponCooldown_.assign(race_.racers.size(), {});
    mineCooldown_.assign(race_.racers.size(), 0.0F);
    hyperCooldown_.assign(race_.racers.size(), 0.0F);
    repairSeconds_.assign(race_.racers.size(), 0.0F);
    stuckSeconds_.assign(race_.racers.size(), 0.0F);
    aiBlockingSeconds_.assign(race_.racers.size(), 0.0F);
    aiBackMovingSeconds_.assign(race_.racers.size(), 0.0F);
    touchCooldown_.assign(
        race_.racers.size() * race_.racers.size(), 0.0F);
    aiBrake_.assign(race_.racers.size(), false);
    aiBlocking_.assign(race_.racers.size(), false);
    aiBackMovingMode_.assign(race_.racers.size(), false);
    aiBackMoving_.assign(race_.racers.size(), false);
    aiMineRandom_.assign(race_.racers.size(), -1.0F);
    aiTracks_.assign(race_.racers.size(), 0U);
    aiLockedTracks_.assign(race_.racers.size(), {});
    aiFrontTargets_.assign(
        race_.racers.size(), RacerRuntime::invalidWeapon);
    aiBackTargets_.assign(
        race_.racers.size(), RacerRuntime::invalidWeapon);
    currentTraceNodes_.assign(race_.racers.size(), {});
    lastTraceNodes_.assign(race_.racers.size(), {});
    lastPathCoordinates_.assign(race_.racers.size(), 0.0F);
    wrongWayStartDistances_.assign(race_.racers.size(), -1.0F);
    mapPositions_.assign(race_.racers.size(), tracePoint(0U).position);
    previousPositions_.assign(race_.racers.size(), {});
    maximumSpeeds_.assign(race_.racers.size(), 0.0F);
    maximumSpeedSeconds_.assign(race_.racers.size(), 0.0F);
    lastLeadPlace_ = 0.0F;
    lastThirdPlace_ = 0.0F;
    decorationActive_.assign(race_.decorationInstances.size(), true);
    decorationLife_.clear();
    decorationLife_.reserve(race_.decorationInstances.size());
    for (const auto& instance : race_.decorationInstances)
    {
        const auto& definition =
            race_.decorationDefinitions.at(instance.definition);
        decorationLife_.push_back(
            definition.maximumLife > 0.0F
                ? definition.maximumLife
                : (definition.destructible ? 1.0F : -1.0F));
    }
    bonusActive_.assign(race_.bonuses.size(), true);
    events_.clear();
    effects_.clear();
    mines_.clear();
    projectiles_.clear();
    respawns_.clear();
    velocityRequests_.clear();
    angularVelocityRequests_.clear();
    achievementPoints_ = initialAchievementPoints_;
    achievementIterations_.assign(race_.achievements.size(), 0U);
    achievementConditionCounters_.assign(
        race_.achievements.size(), 0U);
    achievementConditionTotals_.assign(
        race_.achievements.size(), 0U);
    achievementConditionTimers_.assign(
        race_.achievements.size(), 0.0F);
    achievementGlobalKills_ = 0U;
    achievementPreviousLapPlace_ = 0U;
    for (std::size_t index = 0;
         index < race_.achievements.size(); ++index)
    {
        const auto found = initialAchievementIterations_.find(
            race_.achievements[index].name);
        if (found != initialAchievementIterations_.end())
            achievementIterations_[index] = found->second;
    }
    for (std::size_t index = 0; index < racers_.size(); ++index)
    {
        const auto& sourceRacer = race_.racers[index];
        const std::size_t vehicleIndex = sourceRacer.vehicle;
        const auto& vehicle =
            sourceRacer.hasConfiguredVehicle
                ? sourceRacer.configuredVehicle
                : race_.vehicles.at(std::min(
                      vehicleIndex, race_.vehicles.size() - 1U));
        racers_[index].maximumLife = std::max(vehicle.maximumLife, 1.0F);
        racers_[index].life = racers_[index].maximumLife;
        racers_[index].place = static_cast<std::uint32_t>(index + 1U);
        for (std::size_t weaponIndex = 0;
             weaponIndex < race_.weapons.size(); ++weaponIndex)
        {
            const auto& weapon = race_.weapons[weaponIndex];
            if ((weapon.slot == WeaponSlot::Primary ||
                 weapon.slot == WeaponSlot::Support) &&
                racers_[index].weaponSlots[0] ==
                    RacerRuntime::invalidWeapon)
            {
                racers_[index].weaponSlots[0] = weaponIndex;
                racers_[index].weaponCapacity[0] =
                    std::max(weapon.reloadCharge, 1U);
                racers_[index].weaponCharges[0] =
                    racers_[index].weaponCapacity[0];
            }
            else if (weapon.slot == WeaponSlot::Hyper &&
                     racers_[index].hyperWeapon ==
                         RacerRuntime::invalidWeapon)
            {
                racers_[index].hyperWeapon = weaponIndex;
                racers_[index].hyperCapacity =
                    std::max(weapon.reloadCharge, 1U);
                racers_[index].hyperCharge =
                    racers_[index].hyperCapacity;
            }
            else if (weapon.slot == WeaponSlot::Mine &&
                     racers_[index].mineWeapon ==
                         RacerRuntime::invalidWeapon)
            {
                racers_[index].mineWeapon = weaponIndex;
                racers_[index].mineCapacity =
                    std::max(weapon.reloadCharge, 1U);
                racers_[index].mines =
                    racers_[index].mineCapacity;
            }
        }
        if (index == 0)
        {
            auto& runtime = racers_[index];
            runtime.money = initialPlayerProfile_.money;
            runtime.points = initialPlayerProfile_.points;
            const bool hasProfileLoadout = std::any_of(
                initialPlayerProfile_.slots.begin(),
                initialPlayerProfile_.slots.end(),
                [](const ProfileSlot& slot) {
                    return !slot.record.empty();
                });
            if (hasProfileLoadout)
            {
                runtime.weaponSlots.fill(
                    RacerRuntime::invalidWeapon);
                runtime.weaponCharges.fill(0U);
                runtime.weaponCapacity.fill(0U);
                runtime.hyperWeapon =
                    RacerRuntime::invalidWeapon;
                runtime.hyperCharge = 0;
                runtime.hyperCapacity = 0;
                runtime.mineWeapon =
                    RacerRuntime::invalidWeapon;
                runtime.mines = 0;
                runtime.mineCapacity = 0;
            }
            for (std::size_t slot = 0;
                 slot < PlayerProfile::weaponSlotCount; ++slot)
            {
                const auto& profileSlot =
                    initialPlayerProfile_.slots[
                        PlayerProfile::firstWeaponSlot + slot];
                const auto weapon =
                    findWeapon(profileSlot.record, WeaponSlot::Primary);
                if (weapon != RacerRuntime::invalidWeapon)
                {
                    runtime.weaponSlots[slot] = weapon;
                    runtime.weaponCapacity[slot] =
                        profileSlot.hasCharge
                            ? profileSlot.charge
                            : std::max(
                                  race_.weapons[weapon].reloadCharge, 1U);
                    runtime.weaponCharges[slot] =
                        runtime.weaponCapacity[slot];
                }
            }
            const auto& hyper =
                initialPlayerProfile_.slots[PlayerProfile::hyperSlot];
            const auto hyperWeapon =
                findWeapon(hyper.record, WeaponSlot::Hyper);
            if (hyperWeapon != RacerRuntime::invalidWeapon)
            {
                runtime.hyperWeapon = hyperWeapon;
                runtime.hyperCapacity =
                    hyper.hasCharge
                        ? hyper.charge
                        : std::max(
                              race_.weapons[hyperWeapon].reloadCharge, 1U);
                runtime.hyperCharge = runtime.hyperCapacity;
            }
            const auto& mine =
                initialPlayerProfile_.slots[PlayerProfile::mineSlot];
            const auto mineWeapon =
                findWeapon(mine.record, WeaponSlot::Mine);
            if (mineWeapon != RacerRuntime::invalidWeapon)
            {
                runtime.mineWeapon = mineWeapon;
                runtime.mineCapacity =
                    mine.hasCharge
                        ? mine.charge
                        : std::max(
                              race_.weapons[mineWeapon].reloadCharge, 1U);
                runtime.mines = runtime.mineCapacity;
            }
        }
        else if (!sourceRacer.loadout.empty())
        {
            auto& runtime = racers_[index];
            runtime.weaponSlots.fill(RacerRuntime::invalidWeapon);
            runtime.weaponCharges.fill(0U);
            runtime.weaponCapacity.fill(0U);
            runtime.hyperWeapon = RacerRuntime::invalidWeapon;
            runtime.hyperCharge = 0;
            runtime.hyperCapacity = 0;
            runtime.mineWeapon = RacerRuntime::invalidWeapon;
            runtime.mines = 0;
            runtime.mineCapacity = 0;
            for (const auto& slot : sourceRacer.loadout)
            {
                if (slot.type.rfind("stWeapon", 0) == 0)
                {
                    const auto number =
                        slot.type.substr(std::string_view("stWeapon").size());
                    if (number.size() != 1U ||
                        number.front() < '1' ||
                        number.front() > '4')
                        continue;
                    const auto target =
                        static_cast<std::size_t>(number.front() - '1');
                    const auto weapon =
                        findWeapon(slot.record, WeaponSlot::Primary);
                    if (weapon != RacerRuntime::invalidWeapon)
                    {
                        runtime.weaponSlots[target] = weapon;
                        runtime.weaponCapacity[target] = slot.charge;
                        runtime.weaponCharges[target] =
                            runtime.weaponCapacity[target];
                    }
                }
                else if (slot.type == "stHyper")
                {
                    runtime.hyperWeapon =
                        findWeapon(slot.record, WeaponSlot::Hyper);
                    runtime.hyperCapacity = slot.charge;
                    runtime.hyperCharge = runtime.hyperCapacity;
                }
                else if (slot.type == "stMine")
                {
                    runtime.mineWeapon =
                        findWeapon(slot.record, WeaponSlot::Mine);
                    runtime.mineCapacity = slot.charge;
                    runtime.mines = runtime.mineCapacity;
                }
            }
        }
        syncSelectedWeapon(racers_[index]);
    }
    events_.push_back(
        {RaceEventKind::CountdownChanged, 0, 0, {}, 3.0F});
}

void OriginalRaceSession::applyPlayerProfile(
    const PlayerProfile& profile)
{
    initialPlayerProfile_ = profile;
    reset();
}

void OriginalRaceSession::writePlayerProfile(
    PlayerProfile& profile) const
{
    if (racers_.empty())
        return;
    const auto& runtime = racers_.front();
    profile.money = runtime.money;
    profile.points = runtime.points;
    auto writeWeapon = [&](std::size_t profileSlot,
                           std::size_t weapon,
                           std::uint32_t charge) {
        auto& slot = profile.slots[profileSlot];
        if (weapon == RacerRuntime::invalidWeapon ||
            weapon >= race_.weapons.size())
        {
            slot = {};
            return;
        }
        slot.record = workshopReference(race_.weapons[weapon].record);
        slot.charge = charge;
        slot.hasCharge = true;
    };
    writeWeapon(PlayerProfile::hyperSlot, runtime.hyperWeapon,
                runtime.hyperCapacity);
    writeWeapon(PlayerProfile::mineSlot, runtime.mineWeapon,
                runtime.mineCapacity);
    for (std::size_t slot = 0;
         slot < PlayerProfile::weaponSlotCount; ++slot)
    {
        writeWeapon(PlayerProfile::firstWeaponSlot + slot,
                    runtime.weaponSlots[slot],
                    runtime.weaponCapacity[slot]);
    }
}

void OriginalRaceSession::applyAchievementProfile(
    const ProfileState& profile)
{
    initialAchievementPoints_ = profile.achievementPoints;
    initialAchievementIterations_ =
        profile.achievementIterations;
    achievementPoints_ = initialAchievementPoints_;
    achievementMultiplier_ =
        profile.player.difficulty == "gdHard"
            ? 1.5F
            : profile.player.difficulty == "gdNormal" ? 1.2F : 1.0F;
    achievementIterations_.assign(race_.achievements.size(), 0U);
    for (std::size_t index = 0;
         index < race_.achievements.size(); ++index)
    {
        const auto found = initialAchievementIterations_.find(
            race_.achievements[index].name);
        if (found != initialAchievementIterations_.end())
            achievementIterations_[index] = found->second;
    }
}

void OriginalRaceSession::setCampaign(bool campaign) noexcept
{
    campaign_ = campaign;
}

void OriginalRaceSession::writeAchievementProfile(
    ProfileState& profile) const
{
    profile.achievementPoints = achievementPoints_;
    profile.achievementIterations.clear();
    for (std::size_t index = 0;
         index < race_.achievements.size() &&
         index < achievementIterations_.size();
         ++index)
    {
        profile.achievementIterations[
            race_.achievements[index].name] =
            achievementIterations_[index];
    }
}

void OriginalRaceSession::setEnableMineBug(bool enabled) noexcept
{
    enableMineBug_ = enabled;
}

void OriginalRaceSession::setSpringBorders(bool enabled) noexcept
{
    springBorders_ = enabled;
}

std::size_t OriginalRaceSession::findWeapon(
    std::string_view record, WeaponSlot slot) const noexcept
{
    if (record.empty())
        return RacerRuntime::invalidWeapon;
    const auto wanted = recordName(record);
    for (std::size_t index = 0; index < race_.weapons.size(); ++index)
    {
        const auto& weapon = race_.weapons[index];
        const bool slotMatches =
            weapon.slot == slot ||
            (slot == WeaponSlot::Primary &&
             weapon.slot == WeaponSlot::Support);
        if (slotMatches &&
            (weapon.record == record ||
             recordName(weapon.record) == wanted))
            return index;
    }
    return RacerRuntime::invalidWeapon;
}

void OriginalRaceSession::syncSelectedWeapon(
    RacerRuntime& racer) const noexcept
{
    for (std::size_t offset = 0;
         offset < racer.weaponSlots.size(); ++offset)
    {
        const auto slot =
            (racer.selectedWeaponSlot + offset) %
            racer.weaponSlots.size();
        const auto weapon = racer.weaponSlots[slot];
        if (weapon == RacerRuntime::invalidWeapon ||
            weapon >= race_.weapons.size())
            continue;
        racer.selectedWeaponSlot = slot;
        racer.selectedWeapon = weapon;
        racer.ammunition = racer.weaponCharges[slot];
        return;
    }
    racer.selectedWeapon = RacerRuntime::invalidWeapon;
    racer.ammunition = 0;
}

float OriginalRaceSession::damageAfterSupport(
    std::size_t racer, float damage, bool touchDamage) const noexcept
{
    if (touchDamage || racer >= racers_.size())
        return damage;
    float result = damage;
    for (const auto weaponIndex : racers_[racer].weaponSlots)
    {
        if (weaponIndex == RacerRuntime::invalidWeapon ||
            weaponIndex >= race_.weapons.size())
            continue;
        const auto& weapon = race_.weapons[weaponIndex];
        if (weapon.slot == WeaponSlot::Support &&
            weapon.reflectValue > 0.0F)
        {
            result *= std::clamp(
                1.0F - weapon.reflectValue, 0.0F, 1.0F);
            // Logic::Damage asks Player::GetSlotInst(stReflector), which
            // returns the first installed reflector rather than stacking
            // every matching support slot.
            break;
        }
    }
    return result;
}

bool OriginalRaceSession::damageDecorationAlongRay(
    Vec3 origin, Vec3 direction, float maximumDistance,
    float damage, std::size_t attacker)
{
    std::size_t hit = decorationActive_.size();
    float hitDistance = maximumDistance;
    for (std::size_t index = 0;
         index < race_.decorationInstances.size() &&
         index < decorationActive_.size(); ++index)
    {
        if (!decorationActive_[index])
            continue;
        const auto& instance = race_.decorationInstances[index];
        const auto& definition =
            race_.decorationDefinitions.at(instance.definition);
        if (!definition.destructible ||
            definition.bodyHalfExtents.x <= 0.0F ||
            definition.bodyHalfExtents.y <= 0.0F ||
            definition.bodyHalfExtents.z <= 0.0F)
            continue;
        float distance = hitDistance;
        if (!raycastBox(
                origin, normalized3(direction), hitDistance,
                decorationBox(instance, definition), distance))
            continue;
        hit = index;
        hitDistance = distance;
    }
    for (std::size_t meshIndex = 0;
         meshIndex < race_.collisionMeshes.size() &&
         meshIndex <
             race_.collisionMeshDecorationInstances.size();
         ++meshIndex)
    {
        const std::size_t instanceIndex =
            race_.collisionMeshDecorationInstances[meshIndex];
        if (instanceIndex >= decorationActive_.size() ||
            instanceIndex >= race_.decorationInstances.size() ||
            !decorationActive_[instanceIndex])
            continue;
        const auto& instance =
            race_.decorationInstances[instanceIndex];
        if (instance.definition >=
                race_.decorationDefinitions.size() ||
            !race_.decorationDefinitions[instance.definition]
                 .destructible)
            continue;
        const auto& mesh = race_.collisionMeshes[meshIndex];
        for (std::size_t index = 0;
             index + 2U < mesh.indices.size(); index += 3U)
        {
            const auto firstIndex = mesh.indices[index];
            const auto secondIndex = mesh.indices[index + 1U];
            const auto thirdIndex = mesh.indices[index + 2U];
            if (firstIndex >= mesh.vertices.size() ||
                secondIndex >= mesh.vertices.size() ||
                thirdIndex >= mesh.vertices.size())
                continue;
            float distance = hitDistance;
            if (!raycastTriangle(
                    origin, normalized3(direction), hitDistance,
                    transformPoint(mesh.transform,
                                   mesh.vertices[firstIndex]),
                    transformPoint(mesh.transform,
                                   mesh.vertices[secondIndex]),
                    transformPoint(mesh.transform,
                                   mesh.vertices[thirdIndex]),
                    distance))
                continue;
            hit = instanceIndex;
            hitDistance = distance;
        }
    }
    if (hit >= decorationActive_.size())
        return false;
    return damageDecoration(hit, damage, attacker);
}

bool OriginalRaceSession::damageDecorationWithBox(
    Transform transform, ProjectileCollisionBox collision,
    float damage, std::size_t attacker, Vec3* contactPoint)
{
    if (!hasBox(collision))
        return false;
    const OrientedBox source = orientedBox(transform, collision);
    for (std::size_t index = 0;
         index < race_.decorationInstances.size() &&
         index < decorationActive_.size(); ++index)
    {
        if (!decorationActive_[index])
            continue;
        const auto& instance = race_.decorationInstances[index];
        const auto& definition =
            race_.decorationDefinitions.at(instance.definition);
        const OrientedBox target = decorationBox(instance, definition);
        if (!definition.destructible ||
            definition.bodyHalfExtents.x <= 0.0F ||
            definition.bodyHalfExtents.y <= 0.0F ||
            definition.bodyHalfExtents.z <= 0.0F ||
            !boxesOverlap(source, target))
            continue;
        if (contactPoint != nullptr)
            *contactPoint = closestPoint(target, source.center);
        return damageDecoration(index, damage, attacker);
    }
    for (std::size_t meshIndex = 0;
         meshIndex < race_.collisionMeshes.size() &&
         meshIndex <
             race_.collisionMeshDecorationInstances.size();
         ++meshIndex)
    {
        const std::size_t instanceIndex =
            race_.collisionMeshDecorationInstances[meshIndex];
        if (instanceIndex >= decorationActive_.size() ||
            instanceIndex >= race_.decorationInstances.size() ||
            !decorationActive_[instanceIndex])
            continue;
        const auto& instance =
            race_.decorationInstances[instanceIndex];
        if (instance.definition >=
                race_.decorationDefinitions.size() ||
            !race_.decorationDefinitions[instance.definition]
                 .destructible)
            continue;
        const auto& mesh = race_.collisionMeshes[meshIndex];
        for (std::size_t index = 0;
             index + 2U < mesh.indices.size(); index += 3U)
        {
            const auto firstIndex = mesh.indices[index];
            const auto secondIndex = mesh.indices[index + 1U];
            const auto thirdIndex = mesh.indices[index + 2U];
            if (firstIndex >= mesh.vertices.size() ||
                secondIndex >= mesh.vertices.size() ||
                thirdIndex >= mesh.vertices.size())
                continue;
            const Vec3 first = transformPoint(
                mesh.transform, mesh.vertices[firstIndex]);
            const Vec3 second = transformPoint(
                mesh.transform, mesh.vertices[secondIndex]);
            const Vec3 third = transformPoint(
                mesh.transform, mesh.vertices[thirdIndex]);
            if (!triangleOverlapsBox(first, second, third, source))
                continue;
            if (contactPoint != nullptr)
            {
                *contactPoint = multiply(
                    add(add(first, second), third), 1.0F / 3.0F);
            }
            return damageDecoration(
                instanceIndex, damage, attacker);
        }
    }
    return false;
}

bool OriginalRaceSession::damageDecoration(
    std::size_t hit, float damage, std::size_t attacker)
{
    if (hit >= decorationActive_.size() ||
        hit >= race_.decorationInstances.size() ||
        !decorationActive_[hit])
        return false;
    decorationLife_[hit] -= std::max(damage, 0.0F);
    if (decorationLife_[hit] > 0.0F)
        return true;
    decorationActive_[hit] = false;
    const Vec3 position =
        race_.decorationInstances[hit].transform.position;
    events_.push_back(
        {RaceEventKind::DecorationDestroyed, attacker, hit,
         position, damage});
    return true;
}

void OriginalRaceSession::setPaused(bool paused) noexcept
{
    if (paused && phase_ != RacePhase::Paused)
    {
        phaseBeforePause_ = phase_;
        phase_ = RacePhase::Paused;
    }
    else if (!paused && phase_ == RacePhase::Paused)
    {
        phase_ = phaseBeforePause_;
    }
}

RacePhase OriginalRaceSession::phase() const noexcept
{
    return phase_;
}

float OriginalRaceSession::countdownSeconds() const noexcept
{
    return countdownSeconds_;
}

float OriginalRaceSession::elapsedSeconds() const noexcept
{
    return elapsedSeconds_;
}

bool OriginalRaceSession::finishPresentationReady() const noexcept
{
    return phase_ == RacePhase::Finished &&
           finishSecondsRemaining_ == 0.0F;
}

const std::vector<r3d::physics::VehicleInput>&
OriginalRaceSession::vehicleInputs() const noexcept
{
    return vehicleInputs_;
}

const std::vector<RacerRuntime>& OriginalRaceSession::racers() const noexcept
{
    return racers_;
}

Vec3 OriginalRaceSession::mapPosition(std::size_t racer) const noexcept
{
    return racer < mapPositions_.size() ? mapPositions_[racer] : Vec3{};
}

const std::vector<bool>& OriginalRaceSession::decorationActive() const noexcept
{
    return decorationActive_;
}

const std::vector<bool>& OriginalRaceSession::bonusActive() const noexcept
{
    return bonusActive_;
}

const std::vector<RaceEvent>& OriginalRaceSession::events() const noexcept
{
    return events_;
}

const std::vector<RaceEffect>& OriginalRaceSession::effects() const noexcept
{
    return effects_;
}

const std::vector<MineRuntime>& OriginalRaceSession::mines() const noexcept
{
    return mines_;
}

const std::vector<ProjectileRuntime>&
OriginalRaceSession::projectiles() const noexcept
{
    return projectiles_;
}

std::vector<RespawnRequest> OriginalRaceSession::takeRespawns()
{
    auto result = std::move(respawns_);
    respawns_.clear();
    return result;
}

std::vector<VelocityRequest>
OriginalRaceSession::takeVelocityRequests()
{
    auto result = std::move(velocityRequests_);
    velocityRequests_.clear();
    return result;
}

std::vector<AngularVelocityRequest>
OriginalRaceSession::takeAngularVelocityRequests()
{
    auto result = std::move(angularVelocityRequests_);
    angularVelocityRequests_.clear();
    return result;
}

const std::vector<std::uint32_t>& OriginalRaceSession::tracePathAt(
    std::size_t path) const
{
    if (!race_.tracePaths.empty())
        return race_.tracePaths.at(path);
    if (path == 0U)
        return race_.tracePath;
    throw std::out_of_range("Original race trace path is unresolved");
}

const TracePoint& OriginalRaceSession::tracePoint(
    std::size_t pathNode) const
{
    return tracePoint(0U, pathNode);
}

const TracePoint& OriginalRaceSession::tracePoint(
    std::size_t path, std::size_t pathNode) const
{
    const auto& nodes = tracePathAt(path);
    const std::uint32_t id = nodes.at(pathNode % nodes.size());
    const auto found = std::find_if(
        race_.tracePoints.begin(), race_.tracePoints.end(),
        [id](const TracePoint& point) { return point.id == id; });
    if (found == race_.tracePoints.end())
        throw std::runtime_error("Original race trace path is unresolved");
    return *found;
}

OriginalRaceSession::TraceTileProjection
OriginalRaceSession::projectTraceTile(
    std::size_t path, std::size_t segment, Vec3 position,
    float widthError) const
{
    TraceTileProjection result;
    result.node = {path, segment};
    const auto& nodes = tracePathAt(path);
    const std::size_t segmentCount = nodes.size() - 1U;
    auto segmentDirection = [&](std::size_t index) {
        return normalized2(subtract(
            tracePoint(path, index + 1U).position,
            tracePoint(path, index).position));
    };
    auto miterDirection = [&](Vec3 first, Vec3 second) {
        const Vec3 sum = add(first, second);
        return length2(sum) > 0.0001F ? normalized2(sum) : second;
    };
    const auto& start = tracePoint(path, segment);
    const auto& end = tracePoint(path, segment + 1U);
    result.direction = segmentDirection(segment);
    const float length = std::max(
        length2(subtract(end.position, start.position)), 0.0001F);
    const Vec3 relative = subtract(position, start.position);
    const float along = dot2(relative, result.direction);
    result.coordinate = std::clamp(along / length, 0.0F, 1.0F);
    const float width =
        start.width + (end.width - start.width) * result.coordinate;
    const Vec3 normal{result.direction.y, -result.direction.x, 0.0F};
    const float pathZ =
        start.position.z +
        (end.position.z - start.position.z) * result.coordinate;
    const Vec3 previousDirection =
        segment > 0U ? segmentDirection(segment - 1U)
                     : result.direction;
    const Vec3 followingDirection =
        segment + 1U < segmentCount
            ? segmentDirection(segment + 1U)
            : result.direction;
    const Vec3 startMiter =
        miterDirection(previousDirection, result.direction);
    const Vec3 endMiter =
        miterDirection(result.direction, followingDirection);
    constexpr float boundaryEpsilon = 0.05F;
    const bool betweenMiterPlanes =
        dot2(relative, startMiter) >= -boundaryEpsilon &&
        dot2(subtract(position, end.position), endMiter) <=
            boundaryEpsilon;
    result.contains =
        betweenMiterPlanes &&
        std::abs(dot2(relative, normal)) < width * 0.5F + widthError &&
        std::abs(pathZ - position.z) < width * 0.5F + widthError;
    for (std::size_t index = 0U; index < segment; ++index)
    {
        result.pathDistance += length2(subtract(
            tracePoint(path, index + 1U).position,
            tracePoint(path, index).position));
    }
    result.pathDistance += along;
    return result;
}

OriginalRaceSession::TraceTileProjection
OriginalRaceSession::findTraceTile(
    Vec3 position, TraceNodeRef preferred) const
{
    TraceTileProjection result;
    auto scanPath = [&](std::size_t path,
                        std::size_t firstSegment) {
        const auto& nodes = tracePathAt(path);
        const std::size_t segmentCount = nodes.size() - 1U;
        firstSegment = std::min(firstSegment, segmentCount - 1U);
        for (std::size_t segment = firstSegment;
             segment < segmentCount; ++segment)
        {
            auto candidate =
                projectTraceTile(path, segment, position);
            if (candidate.contains)
                return candidate;
        }
        // WayPath::IsTileContains separately tests its first WayNode after
        // the forward scan.  This is the source loop-closing transition.
        if (firstSegment > 0U)
        {
            auto first = projectTraceTile(path, 0U, position);
            const float nodeRadius =
                std::max(tracePoint(path, 0U).width * 0.5F, 0.0001F);
            if (first.contains &&
                length3(subtract(position,
                                 tracePoint(path, 0U).position)) <
                    nodeRadius)
                return first;
        }
        return TraceTileProjection{};
    };

    if (preferred.valid() && preferred.path <
                                 (race_.tracePaths.empty()
                                      ? 1U
                                      : race_.tracePaths.size()))
    {
        const auto& path = tracePathAt(preferred.path);
        if (path.size() > 1U)
        {
            const std::size_t first =
                preferred.node > 0U ? preferred.node - 1U : 0U;
            result = scanPath(preferred.path, first);
            if (result.contains)
                return result;
        }
    }

    const std::size_t pathCount =
        race_.tracePaths.empty() ? 1U : race_.tracePaths.size();
    for (std::size_t path = 0U; path < pathCount; ++path)
    {
        if (preferred.valid() && path == preferred.path)
            continue;
        if (tracePathAt(path).size() < 2U)
            continue;
        result = scanPath(path, 0U);
        if (result.contains)
            return result;
    }
    return {};
}

bool OriginalRaceSession::linkedTraceTransition(
    TraceNodeRef previous, TraceNodeRef current) const
{
    if (!previous.valid() || !current.valid())
        return false;
    const auto& previousPath = tracePathAt(previous.path);
    const auto& currentPath = tracePathAt(current.path);
    if (previous.node >= previousPath.size() ||
        current.node >= currentPath.size())
        return false;
    const std::uint32_t currentPoint = currentPath[current.node];
    const bool linkedFromNext =
        previous.node + 1U < previousPath.size() &&
        previousPath[previous.node + 1U] == currentPoint;
    const bool linkedAtCurrent =
        previous != current &&
        previousPath[previous.node] == currentPoint;
    const bool linkedAtPathStart =
        previous != current && !previousPath.empty() &&
        previousPath.front() == currentPoint;
    return linkedFromNext || linkedAtCurrent || linkedAtPathStart;
}

OriginalRaceSession::TraceNodeRef
OriginalRaceSession::racerTraceNode(std::size_t racer) const noexcept
{
    if (racer < currentTraceNodes_.size() &&
        currentTraceNodes_[racer].valid())
        return currentTraceNodes_[racer];
    if (racer < lastTraceNodes_.size())
        return lastTraceNodes_[racer];
    return {};
}

OriginalRaceSession::TraceNodeRef OriginalRaceSession::aiTraceNode(
    std::size_t racer,
    const r3d::physics::VehicleState& vehicle) const
{
    const TraceNodeRef current =
        racer < currentTraceNodes_.size()
            ? currentTraceNodes_[racer]
            : TraceNodeRef{};
    const TraceNodeRef last =
        racer < lastTraceNodes_.size()
            ? lastTraceNodes_[racer]
            : TraceNodeRef{};
    if (!last.valid())
        return current;
    if (!current.valid())
        return last;

    // AICar::PathState::Update does not immediately accept an unrelated
    // curTile while the car is still inside lastNode (with 5 units of
    // WayTile tolerance).  This prevents branch crossings from making the
    // steering target jump across the map for one frame.
    if (current != last && !linkedTraceTransition(last, current))
    {
        const auto& path = tracePathAt(last.path);
        if (last.node + 1U < path.size() &&
            projectTraceTile(
                last.path, last.node, vehicle.body.position, 5.0F)
                .contains)
            return last;
    }
    return current;
}

float OriginalRaceSession::tracePathLength(std::size_t path) const
{
    const auto& nodes = tracePathAt(path);
    float result = 0.0F;
    for (std::size_t node = 1U; node < nodes.size(); ++node)
    {
        result += length2(subtract(
            tracePoint(path, node).position,
            tracePoint(path, node - 1U).position));
    }
    return result;
}

float OriginalRaceSession::traceDistance(
    TraceNodeRef node, float coordinate) const
{
    if (!node.valid())
        return 0.0F;
    const auto& path = tracePathAt(node.path);
    if (path.size() < 2U)
        return 0.0F;
    const std::size_t segment =
        std::min(node.node, path.size() - 2U);
    float result = 0.0F;
    for (std::size_t index = 0U; index < segment; ++index)
    {
        result += length2(subtract(
            tracePoint(node.path, index + 1U).position,
            tracePoint(node.path, index).position));
    }
    result += length2(subtract(
                  tracePoint(node.path, segment + 1U).position,
                  tracePoint(node.path, segment).position)) *
              std::clamp(coordinate, 0.0F, 1.0F);
    return result;
}

float OriginalRaceSession::lapPosition(
    std::size_t racer,
    const r3d::physics::VehicleState& vehicle) const
{
    if (racer >= racers_.size())
        return 0.0F;
    const TraceNodeRef node = racerTraceNode(racer);
    if (!node.valid())
        return static_cast<float>(racers_[racer].completedLaps);
    float coordinate =
        racer < lastPathCoordinates_.size()
            ? lastPathCoordinates_[racer]
            : 0.0F;
    if (racer < currentTraceNodes_.size() &&
        currentTraceNodes_[racer] == node)
    {
        coordinate = projectTraceTile(
                         node.path, node.node,
                         vehicle.body.position)
                         .coordinate;
    }
    const float length = tracePathLength(node.path);
    if (length <= 0.0001F)
        return static_cast<float>(racers_[racer].completedLaps);
    return static_cast<float>(racers_[racer].completedLaps) +
           traceDistance(node, coordinate) / length;
}

float OriginalRaceSession::lastCorrectLapPosition(
    std::size_t racer) const
{
    if (racer >= racers_.size() || racer >= lastTraceNodes_.size())
        return 0.0F;
    const TraceNodeRef node = lastTraceNodes_[racer];
    if (!node.valid())
        return static_cast<float>(racers_[racer].completedLaps);
    const float length = tracePathLength(node.path);
    if (length <= 0.0001F)
        return static_cast<float>(racers_[racer].completedLaps);
    const float coordinate =
        racer < lastPathCoordinates_.size()
            ? lastPathCoordinates_[racer]
            : 0.0F;
    return static_cast<float>(racers_[racer].completedLaps) +
           traceDistance(node, coordinate) / length;
}

void OriginalRaceSession::updateProgress(
    std::size_t racer, const r3d::physics::VehicleState& vehicle,
    float seconds)
{
    auto& runtime = racers_[racer];
    if (runtime.finished || runtime.destroyed)
        return;

    // Player::CarState::Update, including its deliberately large 80 m/s
    // threshold.  Preserve the original units instead of substituting a
    // heuristic based on steering or contact count.
    auto& maximumSpeed = maximumSpeeds_[racer];
    auto& maximumSpeedSeconds = maximumSpeedSeconds_[racer];
    maximumSpeedSeconds += seconds;
    if (vehicle.speed > maximumSpeed || maximumSpeedSeconds > 1.0F)
    {
        maximumSpeed = vehicle.speed;
        maximumSpeedSeconds = 0.0F;
    }
    else if (std::abs(maximumSpeed - vehicle.speed) < 5.0F)
    {
        maximumSpeedSeconds = 0.0F;
    }
    if (maximumSpeed > 0.0F &&
        maximumSpeed - vehicle.speed > 80.0F &&
        maximumSpeedSeconds <= 1.0F)
    {
        maximumSpeed = vehicle.speed;
        maximumSpeedSeconds = 0.0F;
        events_.push_back({RaceEventKind::LostControl, racer, 0U,
                           vehicle.body.position, vehicle.speed});
    }

    // Trace::IsTileContains checks the current WayPath first, then every
    // other path.  Keep curTile separate from lastNode: an unlinked tile is
    // valid for map/wrong-way state but cannot skip race progress.
    const TraceTileProjection tile = findTraceTile(
        vehicle.body.position, currentTraceNodes_[racer]);
    if (!tile.contains)
    {
        currentTraceNodes_[racer] = {};
        return;
    }
    currentTraceNodes_[racer] = tile.node;

    const auto& tilePath = tracePathAt(tile.node.path);
    const auto& tileStart =
        tracePoint(tile.node.path, tile.node.node);
    const auto& tileEnd =
        tracePoint(tile.node.path, tile.node.node + 1U);
    mapPositions_[racer] = add(
        tileStart.position,
        multiply(subtract(tileEnd.position, tileStart.position),
                 tile.coordinate));
    const bool wasWrongWay = runtime.wrongWay;
    const Vec3 carDirection = normalized2(forward(vehicle.body.rotation));
    if (dot2(tile.direction, carDirection) < 0.0F)
    {
        auto& inverseStart = wrongWayStartDistances_[racer];
        if (inverseStart < 0.0F)
            inverseStart = tile.pathDistance;
        if (!runtime.wrongWay &&
            inverseStart - tile.pathDistance > 20.0F)
            runtime.wrongWay = true;
    }
    else
    {
        runtime.wrongWay = false;
        wrongWayStartDistances_[racer] = -1.0F;
    }
    if (runtime.wrongWay && !wasWrongWay)
    {
        events_.push_back({RaceEventKind::MoveInverse, racer, 0U,
                           vehicle.body.position, tile.pathDistance});
    }

    const TraceNodeRef previous = lastTraceNodes_[racer];
    if (tile.node == previous)
    {
        lastPathCoordinates_[racer] = tile.coordinate;
        return;
    }
    if (previous.valid() &&
        !linkedTraceTransition(previous, tile.node))
        return;

    const bool onLapPass =
        previous.valid() && tile.node.path == 0U &&
        tile.node.node == 0U;
    lastTraceNodes_[racer] = tile.node;
    lastPathCoordinates_[racer] = tile.coordinate;
    if (previous.valid())
    {
        const std::size_t checkpoint =
            onLapPass ? tracePathAt(0U).size() - 1U
                      : tile.node.node;
        events_.push_back({RaceEventKind::Checkpoint, racer,
                           checkpoint, tileStart.position, 0.0F});
    }
    if (tile.node.path == 0U && !onLapPass)
    {
        runtime.nextPathNode = std::clamp<std::size_t>(
            tile.node.node + 1U, 1U, tilePath.size() - 1U);
    }
    if (!onLapPass)
        return;

    ++runtime.completedLaps;
    events_.push_back({RaceEventKind::Lap, racer, runtime.completedLaps,
                       vehicle.body.position,
                       static_cast<float>(runtime.completedLaps)});
    runtime.nextPathNode = 1;
    if (racer == 0U && runtime.completedLaps + 1U == race_.lapCount)
    {
        const auto leader = std::min_element(
            racers_.begin(), racers_.end(),
            [](const RacerRuntime& first, const RacerRuntime& second) {
                return first.place < second.place;
            });
        const std::size_t leaderIndex =
            leader == racers_.end()
                ? 0U
                : static_cast<std::size_t>(leader - racers_.begin());
        events_.push_back({RaceEventKind::LastLap, leaderIndex, 0U,
                           vehicle.body.position,
                           static_cast<float>(runtime.completedLaps)});
    }
    if (runtime.completedLaps >= race_.lapCount)
    {
        runtime.finished = true;
        runtime.finishTime = elapsedSeconds_;
        const auto finishedBefore = std::count_if(
            racers_.begin(), racers_.end(),
            [&](const RacerRuntime& candidate) {
                return &candidate != &runtime && candidate.finished;
            });
        runtime.place =
            static_cast<std::uint32_t>(finishedBefore + 1U);
        const std::size_t reward =
            std::min<std::size_t>(
                runtime.place > 0 ? runtime.place - 1U : 0U,
                race_.rewardMoney.size() - 1U);
        runtime.rewardMoney = race_.rewardMoney[reward];
        runtime.rewardPoints = race_.rewardPoints[reward];
        vehicleInputs_[racer] = {};
        events_.push_back({RaceEventKind::Finish, racer, 0,
                           vehicle.body.position, runtime.finishTime});
        const std::size_t finishCount =
            static_cast<std::size_t>(finishedBefore) + 1U;
        RaceEventKind finishKind = RaceEventKind::LeadFinish;
        if (finishCount == 2U)
            finishKind = RaceEventKind::SecondFinish;
        else if (finishCount == 3U)
            finishKind = RaceEventKind::ThirdFinish;
        else if (finishCount == racers_.size())
            finishKind = RaceEventKind::LastFinish;
        events_.push_back({finishKind, racer, 0U,
                           vehicle.body.position, runtime.finishTime});
        if (racer == 0)
        {
            runtime.money += runtime.rewardMoney + runtime.pickedMoney;
            runtime.points += runtime.rewardPoints;
            events_.push_back({RaceEventKind::RaceFinish, racer, 0U,
                               vehicle.body.position,
                               runtime.finishTime});
            phase_ = RacePhase::Finished;
            // GameMode::RunFinishTimer waits three seconds before Menu
            // exits the race and Race::CompleteRace ranks all remaining
            // cars for FinishMenu.
            finishSecondsRemaining_ = 3.0F;
        }
    }
}

void OriginalRaceSession::updateAiTracks(
    const std::vector<r3d::physics::VehicleState>& vehicles)
{
    constexpr std::uint32_t trackCount = 4U;
    for (auto& tracks : aiLockedTracks_)
        tracks.fill(false);
    struct AiTrackState
    {
        std::size_t racer = 0;
        std::size_t path = 0;
        std::size_t collisionNode = 0;
        Vec3 direction{1.0F, 0.0F, 0.0F};
        Vec3 position{};
        float lateral = 0.0F;
        float radius = 0.5F;
        std::uint32_t currentTrack = 0U;
    };
    std::vector<AiTrackState> states;
    states.reserve(vehicles.size());
    for (std::size_t racer = 1U;
         racer < racers_.size() && racer < vehicles.size(); ++racer)
    {
        if (racers_[racer].destroyed || racers_[racer].finished)
            continue;
        // AISystem::ComputeTracks only inserts AI players whose live
        // CarState::curTile exists.  PathState may retain lastNode for
        // steering, but that fallback must not reserve lanes off-trace.
        TraceNodeRef traceNode =
            racer < currentTraceNodes_.size()
                ? currentTraceNodes_[racer]
                : TraceNodeRef{};
        if (!traceNode.valid())
            continue;
        const auto& path = tracePathAt(traceNode.path);
        traceNode.node = std::min(traceNode.node, path.size() - 2U);
        const std::size_t nextNode = traceNode.node + 1U;
        const auto& previous = tracePoint(traceNode.path, traceNode.node);
        const auto& next = tracePoint(traceNode.path, nextNode);
        const Vec3 direction =
            normalized2(subtract(next.position, previous.position));
        const Vec3 normal{direction.y, -direction.x, 0.0F};
        const Vec3 relative =
            subtract(vehicles[racer].body.position, previous.position);
        const float longitudinal = dot2(relative, direction);
        const float segmentLength = std::max(
            length2(subtract(next.position, previous.position)), 0.0001F);
        const float part =
            std::clamp(longitudinal / segmentLength, 0.0F, 1.0F);
        const float width =
            previous.width + (next.width - previous.width) * part;
        const float trackWidth =
            std::max(width / static_cast<float>(trackCount), 0.0001F);
        const float lateral = dot2(relative, normal);
        const auto rawTrack = static_cast<int>(
            std::floor(lateral / trackWidth +
                       static_cast<float>(trackCount) * 0.5F));
        const auto currentTrack = static_cast<std::uint32_t>(
            std::clamp(std::abs(rawTrack), 0,
                       static_cast<int>(trackCount) - 1));
        // Player::CarState::curNode switches to curTile->next while the car
        // is inside the next WayNode sphere.  AISystem uses that node's
        // infinite tile strip when building overlap chains at corners.
        std::size_t collisionNode = traceNode.node;
        if (nextNode < path.size())
        {
            Vec3 incoming = direction;
            Vec3 outgoing = direction;
            if (nextNode + 1U < path.size())
            {
                outgoing = normalized2(subtract(
                    tracePoint(traceNode.path, nextNode + 1U).position,
                    next.position));
            }
            const float turnAngle = std::acos(std::clamp(
                dot2(incoming, outgoing), -1.0F, 1.0F));
            const float nodeRadius =
                next.width * 0.5F /
                std::sqrt(std::max(
                    (1.0F + std::cos(turnAngle)) * 0.5F,
                    0.000001F));
            if (length3(subtract(
                    vehicles[racer].body.position,
                    next.position)) < nodeRadius)
            {
                collisionNode = std::min(
                    nextNode, path.size() - 2U);
            }
        }
        const auto& source = race_.racers[racer];
        const auto& vehicleDefinition =
            source.hasConfiguredVehicle
                ? source.configuredVehicle
                : race_.vehicles.at(source.vehicle);
        const Vec3 half = vehicleDefinition.physics.halfExtents;
        const float radius =
            std::sqrt(half.x * half.x + half.y * half.y +
                      half.z * half.z);
        states.push_back(
            {racer, traceNode.path, collisionNode, direction,
             vehicles[racer].body.position, lateral, radius,
             currentTrack});
        aiTracks_[racer] = currentTrack;
    }

    auto tileStripContains = [&](const AiTrackState& source,
                                 Vec3 position) {
        const auto& start = tracePoint(
            source.path, source.collisionNode);
        const auto& end = tracePoint(
            source.path, source.collisionNode + 1U);
        const Vec3 direction = normalized2(
            subtract(end.position, start.position));
        const Vec3 relative = subtract(position, start.position);
        const float segmentLength = std::max(
            length2(subtract(end.position, start.position)),
            0.0001F);
        const float coordinate = std::clamp(
            dot2(relative, direction) / segmentLength, 0.0F, 1.0F);
        const float width =
            start.width + (end.width - start.width) * coordinate;
        const float pathZ =
            start.position.z +
            (end.position.z - start.position.z) * coordinate;
        const Vec3 normal{direction.y, -direction.x, 0.0F};
        // WayNode::Tile::IsContains(..., false) deliberately omits the two
        // end planes while retaining lateral and Z bounds.
        return std::abs(dot2(relative, normal)) < width * 0.5F &&
               std::abs(pathZ - position.z) < width * 0.5F;
    };

    std::vector<bool> assigned(states.size(), false);
    for (std::size_t first = 0; first < states.size(); ++first)
    {
        if (assigned[first])
            continue;
        std::vector<std::size_t> chain{first};
        assigned[first] = true;
        for (std::size_t link = 0; link < chain.size(); ++link)
        {
            const auto& source = states[chain[link]];
            for (std::size_t candidate = 0;
                 candidate < states.size(); ++candidate)
            {
                if (assigned[candidate])
                    continue;
                const float longitudinalDistance = std::abs(
                    dot2(source.direction,
                         subtract(states[candidate].position,
                                  source.position)));
                if (longitudinalDistance >
                        states[candidate].radius + source.radius ||
                    !tileStripContains(
                        source, states[candidate].position))
                    continue;
                assigned[candidate] = true;
                chain.push_back(candidate);
            }
        }
        if (chain.size() < 2U)
            continue;
        std::stable_sort(
            chain.begin(), chain.end(),
            [&](std::size_t left, std::size_t right) {
                return states[left].lateral <
                       states[right].lateral;
            });
        std::uint32_t selectedTrack = 0U;
        std::size_t previousState = states.size();
        std::uint32_t previousSelectedTrack = 0U;
        const auto occupied = static_cast<std::uint32_t>(
            std::min<std::size_t>(chain.size(), trackCount));
        for (std::size_t index = 0; index < chain.size(); ++index)
        {
            if (index > 0U && index % trackCount == 0U)
                selectedTrack = 0U;
            const auto& state = states[chain[index]];
            const auto withinGroup =
                static_cast<std::uint32_t>(index % trackCount);
            const std::uint32_t maximumTrack =
                trackCount - (occupied - withinGroup);
            const std::uint32_t minimumTrack =
                std::min(selectedTrack, trackCount - 1U);
            selectedTrack = std::clamp(
                state.currentTrack, minimumTrack,
                std::max(minimumTrack, maximumTrack));
            for (const auto otherState : chain)
            {
                if (otherState != chain[index])
                {
                    aiLockedTracks_[states[otherState].racer]
                                   [selectedTrack] = true;
                }
            }
            if (previousState != states.size() &&
                states[previousState].currentTrack ==
                    state.currentTrack &&
                previousSelectedTrack > 0U)
            {
                aiLockedTracks_[state.racer]
                               [previousSelectedTrack - 1U] = true;
            }
            previousState = chain[index];
            previousSelectedTrack = selectedTrack;
            ++selectedTrack;
        }
    }
}

r3d::physics::VehicleInput OriginalRaceSession::aiInput(
    std::size_t racer, const r3d::physics::VehicleState& vehicle,
    float seconds)
{
    if (racers_[racer].finished || racers_[racer].destroyed)
        return {};
    constexpr float steerAngleBias =
        3.14159265358979323846F / 128.0F;
    constexpr float maximumSpeedBlocking = 0.5F;
    constexpr float maximumTimeBlocking = 1.0F;
    constexpr float steerControl = 1.0F;

    const bool sourceTileMissing =
        racer >= currentTraceNodes_.size() ||
        !currentTraceNodes_[racer].valid();
    TraceNodeRef traceNode = aiTraceNode(racer, vehicle);
    if (!traceNode.valid())
    {
        traceNode = {0U, std::clamp<std::size_t>(
                              racers_[racer].nextPathNode > 0U
                                  ? racers_[racer].nextPathNode - 1U
                                  : 0U,
                              0U, race_.tracePath.size() - 2U)};
    }
    const auto& path = tracePathAt(traceNode.path);
    traceNode.node = std::min(traceNode.node, path.size() - 2U);
    const std::size_t nextPathNode = traceNode.node + 1U;
    const auto& target = tracePoint(traceNode.path, nextPathNode);
    const auto& pathStart = tracePoint(traceNode.path, traceNode.node);
    const Vec3 currentDirection =
        normalized2(subtract(target.position, pathStart.position));
    const TracePoint* following = &target;
    Vec3 followingDirection = currentDirection;
    if (nextPathNode + 1U < path.size())
    {
        following = &tracePoint(traceNode.path, nextPathNode + 1U);
        followingDirection = normalized2(
            subtract(following->position, target.position));
    }
    const auto& racerDefinition = race_.racers[racer];
    const auto& vehicleDefinition =
        racerDefinition.hasConfiguredVehicle
            ? racerDefinition.configuredVehicle
            : race_.vehicles.at(racerDefinition.vehicle);
    float directionArea =
        5.0F + std::abs(vehicle.speed) *
                   vehicleDefinition.physics.steeringControl * 10.0F;
    const Vec3 half = vehicleDefinition.physics.halfExtents;
    const float carSize =
        2.0F * std::sqrt(half.x * half.x + half.y * half.y +
                         half.z * half.z);
    constexpr std::uint32_t trackCount = 4U;
    std::uint32_t wantedTrack =
        racer < aiTracks_.size() ? aiTracks_[racer] : 0U;
    Vec3 movementStart = pathStart.position;
    Vec3 movementEnd = target.position;
    float movementStartWidth = pathStart.width;
    float movementEndWidth = target.width;
    Vec3 movementDirection = currentDirection;
    const float turnAngle = std::acos(std::clamp(
        dot2(currentDirection, followingDirection), -1.0F, 1.0F));
    if (turnAngle > 3.14159265358979323846F / 12.0F)
    {
        const Vec3 middleDirection =
            normalized2(add(currentDirection, followingDirection));
        const bool turnLeft =
            currentDirection.x * followingDirection.y -
                currentDirection.y * followingDirection.x >
            0.0F;
        const Vec3 edgeNormal =
            turnLeft
                ? Vec3{-middleDirection.y, middleDirection.x, 0.0F}
                : Vec3{middleDirection.y, -middleDirection.x, 0.0F};
        const float halfAngleCosine = std::sqrt(std::max(
            (1.0F + dot2(currentDirection, followingDirection)) *
                0.5F,
            0.000001F));
        const float nodeRadius =
            target.width * 0.5F / halfAngleCosine;
        const Vec3 edgePoint = add(
            target.position, multiply(edgeNormal, nodeRadius));
        const float edgeDistance = dot2(
            edgeNormal,
            subtract(vehicle.body.position, edgePoint));
        if (edgeDistance < carSize)
        {
            const float trackWidth =
                target.width / static_cast<float>(trackCount);
            const Vec3 targetPoint = subtract(
                add(target.position,
                    multiply(edgeNormal,
                             trackWidth *
                                 static_cast<float>(trackCount) *
                                 0.5F)),
                vehicle.body.position);
            const float projection =
                dot2(currentDirection, targetPoint);
            if (projection < carSize)
            {
                movementStart = target.position;
                movementEnd = following->position;
                movementStartWidth = target.width;
                movementEndWidth = following->width;
                movementDirection = followingDirection;
            }
            else
            {
                directionArea = projection;
                wantedTrack = turnLeft ? 0U : trackCount - 1U;
            }
        }
    }

    const auto& lockedTracks = aiLockedTracks_[racer];
    const std::uint32_t currentTrack =
        racer < aiTracks_.size() ? aiTracks_[racer] : 0U;
    std::uint32_t selectedTrack = currentTrack;
    if (wantedTrack != currentTrack)
    {
        const int increment =
            wantedTrack > currentTrack ? 1 : -1;
        for (std::uint32_t distance = 1U;
             distance <= static_cast<std::uint32_t>(
                 std::abs(static_cast<int>(wantedTrack) -
                          static_cast<int>(currentTrack)));
             ++distance)
        {
            const auto testTrack = static_cast<std::uint32_t>(
                static_cast<int>(currentTrack) +
                static_cast<int>(distance) * increment);
            if (lockedTracks[testTrack])
                break;
            selectedTrack = testTrack;
        }
    }
    if (selectedTrack == currentTrack && lockedTracks[currentTrack])
    {
        if (currentTrack > 0U && !lockedTracks[currentTrack - 1U])
            selectedTrack = currentTrack - 1U;
        else if (currentTrack + 1U < trackCount &&
                 !lockedTracks[currentTrack + 1U])
            selectedTrack = currentTrack + 1U;
    }

    Vec3 laneTarget = add(
        vehicle.body.position,
        multiply(movementDirection, directionArea));
    const Vec3 segment =
        subtract(movementEnd, movementStart);
    const float segmentLength = std::max(length2(segment), 0.0001F);
    const float segmentPart = std::clamp(
        dot2(subtract(laneTarget, movementStart),
             movementDirection) /
            segmentLength,
        0.0F, 1.0F);
    const float pathWidth =
        movementStartWidth +
        (movementEndWidth - movementStartWidth) * segmentPart;
    const float trackWidth =
        pathWidth / static_cast<float>(trackCount);
    const Vec3 pathNormal{
        movementDirection.y, -movementDirection.x, 0.0F};
    const float wantedLateral =
        trackWidth *
            (static_cast<float>(selectedTrack) + 0.5F) -
        pathWidth * 0.5F;
    const float currentLateral = dot2(
        subtract(laneTarget, movementStart), pathNormal);
    laneTarget = add(
        laneTarget,
        multiply(pathNormal, wantedLateral - currentLateral));
    const Vec3 wanted = normalized2(
        subtract(laneTarget, vehicle.body.position));
    const Vec3 carForward = normalized2(forward(vehicle.body.rotation));
    const float cross = carForward.x * wanted.y - carForward.y * wanted.x;
    const float alignment =
        std::clamp(dot2(carForward, wanted), -1.0F, 1.0F);
    float steeringAngle = std::acos(alignment);
    if (steeringAngle > steerAngleBias)
        steeringAngle = cross > 0.0F ? steeringAngle : -steeringAngle;
    else
        steeringAngle = 0.0F;

    // PathState selects curNode while the car is inside the current
    // WayNode sphere; otherwise it brakes for nextTile.  Use that exact
    // source node and its miter-normal distance instead of an approximation
    // along the incoming segment.
    auto nodeTurn = [&](std::size_t node) {
        if (node == 0U || node + 1U >= path.size())
            return 0.0F;
        const Vec3 incoming = normalized2(subtract(
            tracePoint(traceNode.path, node).position,
            tracePoint(traceNode.path, node - 1U).position));
        const Vec3 outgoing = normalized2(subtract(
            tracePoint(traceNode.path, node + 1U).position,
            tracePoint(traceNode.path, node).position));
        return std::acos(std::clamp(
            dot2(incoming, outgoing), -1.0F, 1.0F));
    };
    const float currentNodeTurn = nodeTurn(traceNode.node);
    const float currentNodeRadius =
        pathStart.width * 0.5F /
        std::sqrt(std::max(
            (1.0F + std::cos(currentNodeTurn)) * 0.5F,
            0.000001F));
    const bool insideCurrentNode =
        length2(subtract(vehicle.body.position,
                         pathStart.position)) < currentNodeRadius;
    const std::size_t brakeNode =
        insideCurrentNode ? traceNode.node : nextPathNode;
    const float brakeTurnAngle = nodeTurn(brakeNode);
    if (brakeTurnAngle > 3.14159265358979323846F / 12.0F)
    {
        const auto& brakePoint =
            tracePoint(traceNode.path, brakeNode);
        const Vec3 brakeIncoming = normalized2(subtract(
            brakePoint.position,
            tracePoint(traceNode.path, brakeNode - 1U).position));
        const Vec3 brakeDirection = normalized2(subtract(
            tracePoint(traceNode.path, brakeNode + 1U).position,
            brakePoint.position));
        const Vec3 middleDirection = normalized2(
            add(brakeIncoming, brakeDirection));
        const float normalDistance = dot2(
            middleDirection,
            subtract(vehicle.body.position, brakePoint.position));
        const float brakeDistance =
            -normalDistance + brakePoint.width;
        const float rotation =
            1.0F - std::max(dot2(carForward, brakeDirection), 0.0F);
        const float demand =
            vehicle.speed * vehicle.speed * steerControl *
            steerControl * rotation;
        if (!aiBrake_[racer] && demand > 1.5F * brakeDistance)
            aiBrake_[racer] = true;
        else if (aiBrake_[racer] && demand < brakeDistance)
            aiBrake_[racer] = false;
    }
    else
    {
        aiBrake_[racer] = false;
    }

    const bool belowBlockingSpeed =
        std::abs(vehicle.speed) < maximumSpeedBlocking;
    if (belowBlockingSpeed)
    {
        aiBlockingSeconds_[racer] += seconds;
        if (aiBlockingSeconds_[racer] > maximumTimeBlocking)
        {
            aiBlockingSeconds_[racer] = 0.0F;
            aiBlocking_[racer] = true;
        }
    }
    else
    {
        aiBlockingSeconds_[racer] = 0.0F;
        aiBlocking_[racer] = false;
    }
    // UpdateResetCar has its own timer and also advances while curTile is
    // null, even if the car still has speed.  It must not set ControlState's
    // blocking flag or force reverse while the car is moving.
    if (belowBlockingSpeed || sourceTileMissing)
        stuckSeconds_[racer] += seconds;
    else
        stuckSeconds_[racer] = 0.0F;

    // AICar::ControlState does not wait passively for the three-second
    // reset.  After one blocked second it alternates reverse and forward,
    // reversing the steering angle while backing up.
    if (!aiBackMovingMode_[racer])
    {
        aiBackMovingMode_[racer] = aiBlocking_[racer];
        aiBackMoving_[racer] = aiBlocking_[racer];
    }
    if (aiBackMovingMode_[racer])
    {
        aiBackMovingSeconds_[racer] += seconds;
        if (aiBackMovingSeconds_[racer] > maximumTimeBlocking ||
            (aiBackMoving_[racer] &&
             std::abs(steeringAngle) < steerAngleBias &&
             aiBackMovingSeconds_[racer] >
                 0.5F * maximumTimeBlocking))
        {
            aiBackMoving_[racer] = !aiBackMoving_[racer];
            aiBackMovingSeconds_[racer] = 0.0F;
            aiBackMovingMode_[racer] = aiBlocking_[racer];
        }
        if (aiBackMoving_[racer])
            steeringAngle = -steeringAngle;
    }

    r3d::physics::VehicleInput input;
    input.steering = clampSteering(
        steeringAngle /
        std::max(vehicleDefinition.physics.steerAngle, 0.01F));
    if (aiBrake_[racer])
        input.brake = 1.0F;
    else if (aiBackMoving_[racer])
        input.reverse = 1.0F;
    else
        input.throttle = 1.0F;
    return input;
}

void OriginalRaceSession::updatePlaces(
    const std::vector<r3d::physics::VehicleState>& vehicles)
{
    const auto oldLeaderFound = std::find_if(
        racers_.begin(), racers_.end(),
        [](const RacerRuntime& racer) { return racer.place == 1U; });
    const std::size_t oldLeader =
        oldLeaderFound == racers_.end()
            ? 0U
            : static_cast<std::size_t>(oldLeaderFound - racers_.begin());
    const auto oldThirdFound = std::find_if(
        racers_.begin(), racers_.end(),
        [](const RacerRuntime& racer) { return racer.place == 3U; });
    const std::size_t oldThird =
        oldThirdFound == racers_.end()
            ? RacerRuntime::invalidWeapon
            : static_cast<std::size_t>(oldThirdFound - racers_.begin());
    std::vector<std::size_t> order(racers_.size());
    std::iota(order.begin(), order.end(), 0U);
    auto score = [&](std::size_t racer) {
        const auto& runtime = racers_[racer];
        if (runtime.finished)
            return 100000.0F - runtime.finishTime;
        if (racer >= vehicles.size())
            return static_cast<float>(runtime.completedLaps);
        // Race::OnLateProgress orders Player::CarState::GetLap(), which is
        // normalized against the currently occupied WayPath (including a
        // branch), rather than an invented main-path checkpoint index.
        return lapPosition(racer, vehicles[racer]);
    };
    std::stable_sort(order.begin(), order.end(),
                     [&](std::size_t first, std::size_t second) {
                         return score(first) > score(second);
                     });
    for (std::size_t place = 0; place < order.size(); ++place)
        racers_[order[place]].place =
            static_cast<std::uint32_t>(place + 1U);

    if (order.empty())
        return;
    const bool hasResults = std::any_of(
        racers_.begin(), racers_.end(),
        [](const RacerRuntime& racer) { return racer.finished; });
    auto onMainPath = [&](std::size_t racer) {
        return racer < lastTraceNodes_.size() &&
               lastTraceNodes_[racer].valid() &&
               lastTraceNodes_[racer].path == 0U;
    };
    auto eventPosition = [&](std::size_t racer) {
        return racer < vehicles.size()
                   ? vehicles[racer].body.position
                   : Vec3{};
    };
    const float mainPathLength = std::max(tracePathLength(0U), 1.0F);
    const std::size_t leader = order.front();
    if (leader != oldLeader && onMainPath(leader) && onMainPath(oldLeader))
    {
        const float newLeadPlace = lastCorrectLapPosition(leader);
        if (mainPathLength * (newLeadPlace - lastLeadPlace_) > 300.0F &&
            !hasResults)
        {
            events_.push_back({RaceEventKind::LeadChanged, leader,
                               oldLeader, eventPosition(leader),
                               newLeadPlace});
        }
        lastLeadPlace_ = newLeadPlace;
    }
    if (order.size() >= 3U)
    {
        const std::size_t third = order[2U];
        if (oldThird != RacerRuntime::invalidWeapon && third != oldThird &&
            onMainPath(third) && onMainPath(oldThird))
        {
            const float newThirdPlace = lastCorrectLapPosition(third);
            if (mainPathLength * (newThirdPlace - lastThirdPlace_) >
                    300.0F &&
                !hasResults)
            {
                events_.push_back({RaceEventKind::ThirdChanged, third,
                                   oldThird, eventPosition(third),
                                   newThirdPlace});
            }
            lastThirdPlace_ = newThirdPlace;
        }
    }
    if (order.size() >= 2U)
    {
        const std::size_t last = order.back();
        const std::size_t nextLast = order[order.size() - 2U];
        if (onMainPath(last) && onMainPath(nextLast) &&
            mainPathLength *
                    (lastCorrectLapPosition(nextLast) -
                     lastCorrectLapPosition(last)) >
                70.0F)
        {
            events_.push_back({RaceEventKind::LastFar, last, nextLast,
                               eventPosition(last), 0.0F});
        }
        const std::size_t second = order[1U];
        if (!hasResults && onMainPath(leader) && onMainPath(second) &&
            mainPathLength *
                    (lastCorrectLapPosition(leader) -
                     lastCorrectLapPosition(second)) >
                70.0F)
        {
            events_.push_back({RaceEventKind::Domination, leader, second,
                               eventPosition(leader), 0.0F});
        }
    }
    if (order.size() >= 3U)
    {
        const std::size_t second = order[1U];
        const std::size_t third = order[2U];
        if (!hasResults && onMainPath(second) && onMainPath(third) &&
            mainPathLength *
                    (lastCorrectLapPosition(second) -
                     lastCorrectLapPosition(third)) >
                70.0F)
        {
            events_.push_back({RaceEventKind::ThirdFar, third, second,
                               eventPosition(third), 0.0F});
        }
    }
}

void OriginalRaceSession::queueRespawn(
    std::size_t racer,
    const std::vector<r3d::physics::VehicleState>& vehicles)
{
    if (racer >= racers_.size() || racer >= vehicles.size())
        return;
    const auto& runtime = racers_[racer];
    TraceNodeRef traceNode =
        racer < lastTraceNodes_.size() &&
                lastTraceNodes_[racer].valid()
            ? lastTraceNodes_[racer]
            : racerTraceNode(racer);
    if (!traceNode.valid())
    {
        traceNode = {0U, std::min<std::size_t>(
                              runtime.nextPathNode > 0U
                                  ? runtime.nextPathNode - 1U
                                  : 0U,
                              race_.tracePath.size() - 2U)};
    }
    const auto& path = tracePathAt(traceNode.path);
    std::size_t nodeIndex =
        std::min(traceNode.node, path.size() - 2U);
    auto segmentLength = [&](std::size_t node) {
        return std::max(
            length2(subtract(
                tracePoint(traceNode.path, node + 1U).position,
                tracePoint(traceNode.path, node).position)),
            0.0001F);
    };
    float distance =
        segmentLength(nodeIndex) *
        std::clamp(lastPathCoordinates_[racer], 0.0F, 1.0F);
    constexpr std::array<float, 3> offsets{
        0.0F, -2.0F, 2.0F};
    bool initialDeathPlane = false;
    Vec3 position{};
    Vec3 direction{1.0F, 0.0F, 0.0F};

    for (int attempt = 0; attempt < 5; ++attempt)
    {
        bool found = false;
        int sample = 0;
        while (sample < static_cast<int>(offsets.size()))
        {
            const auto& start = tracePoint(traceNode.path, nodeIndex);
            const auto& end = tracePoint(traceNode.path, nodeIndex + 1U);
            const float length = segmentLength(nodeIndex);
            const float coordinate = std::clamp(
                (distance + offsets[static_cast<std::size_t>(sample)]) /
                    length,
                0.0F, 1.0F);
            const Vec3 tileDirection =
                normalized2(subtract(end.position, start.position));
            const float width =
                start.width + (end.width - start.width) * coordinate;
            Vec3 rayPosition = add(
                start.position,
                multiply(subtract(end.position, start.position),
                         coordinate));
            rayPosition.z += width * 0.25F;

            if (attempt == 0 && sample == 0)
            {
                position = rayPosition;
                direction = tileDirection;
            }
            if (sample == 0)
            {
                position = rayPosition;
                direction = tileDirection;
            }

            const ResetRayHit hit = raycastResetWorld(
                race_, decorationActive_, vehicles, racers_, racer,
                rayPosition);
            if (!initialDeathPlane && attempt == 0 && sample == 0 &&
                (hit.kind == ResetRayKind::None ||
                 hit.kind == ResetRayKind::DeathPlane))
            {
                initialDeathPlane = true;
                distance = 0.0F;
                sample = 0;
                continue;
            }
            if (hit.kind != ResetRayKind::TrackPlane &&
                hit.kind != ResetRayKind::OwnVehicle)
            {
                break;
            }
            if (sample == static_cast<int>(offsets.size()) - 1)
                found = true;
            ++sample;
        }
        if (found)
            break;

        const float previousDistance = distance - 6.0F;
        if (previousDistance < 0.0F)
        {
            if (nodeIndex == 0U)
                break;
            --nodeIndex;
            distance = std::max(
                segmentLength(nodeIndex) + previousDistance, 0.0F);
        }
        else
        {
            distance = previousDistance;
        }
    }

    respawns_.push_back({racer, position, direction});
    previousPositions_[racer] = vehicles[racer].body.position;
    stuckSeconds_[racer] = 0.0F;
    events_.push_back(
        {RaceEventKind::Respawn, racer, nodeIndex, position, 0.0F});
}

void OriginalRaceSession::destroyRacer(
    std::size_t racer, std::size_t attacker, Vec3 position,
    const r3d::physics::VehicleState& vehicle, DamageType damageType,
    bool killCredit)
{
    if (racer >= racers_.size() || racer >= race_.racers.size() ||
        racers_[racer].destroyed)
        return;
    auto& runtime = racers_[racer];
    runtime.life = 0.0F;
    runtime.destroyed = true;
    runtime.lowLife = false;
    runtime.lowLifeEffectSeconds = 0.0F;
    runtime.shieldSeconds = 0.0F;
    runtime.shieldEffectSeconds = 0.0F;
    runtime.shieldFadeInSeconds = -1.0F;
    runtime.shieldFadeOutSeconds = -1.0F;
    runtime.shieldDamageSeconds = -1.0F;
    runtime.touchAttacker = RacerRuntime::invalidWeapon;
    runtime.touchAttributionSeconds = 0.0F;
    // Player::cTimeRestoreCar in the Windows implementation.
    runtime.restoreSeconds = 2.0F;
    if (racer < vehicleInputs_.size())
        vehicleInputs_[racer] = {};
    if (damageType == DamageType::DeathPlane)
    {
        events_.push_back({RaceEventKind::Overboard, racer, attacker,
                           position, 0.0F});
    }
    else if (damageType == DamageType::Mine)
    {
        events_.push_back({RaceEventKind::DeathMine, racer, attacker,
                           position, 0.0F});
    }
    events_.push_back({RaceEventKind::Death, racer, attacker,
                       position, 0.0F});
    RaceEvent deathEvent;
    deathEvent.kind = RaceEventKind::Kill;
    deathEvent.racer = attacker;
    deathEvent.target = racer;
    deathEvent.position = position;
    deathEvent.touchDamage =
        damageType == DamageType::Touch;
    deathEvent.killCredit = killCredit;
    deathEvent.damageType = damageType;
    events_.push_back(std::move(deathEvent));

    const auto& sourceRacer = race_.racers[racer];
    const auto& definition =
        sourceRacer.hasConfiguredVehicle
            ? sourceRacer.configuredVehicle
            : race_.vehicles.at(sourceRacer.vehicle);
    for (std::size_t index = 0;
         index < definition.deathEffects.size(); ++index)
    {
        const auto& source = definition.deathEffects[index];
        if (!source.visual.soundPaths.empty())
        {
            RaceEvent sound;
            sound.kind = RaceEventKind::EffectSound;
            sound.racer = racer;
            sound.position = add(vehicle.body.position, source.position);
            sound.soundPath = source.visual.soundPaths[
                sourceUniformRandomIndex(
                    source.visual.soundPaths.size(),
                    sourceUniformRandomUnit())];
            events_.push_back(std::move(sound));
        }
        RaceEffect effect;
        effect.kind = RaceEventKind::VehicleDestroyed;
        effect.origin = add(vehicle.body.position, source.position);
        effect.target = add(effect.origin, {1.0F, 0.0F, 0.0F});
        const auto timing = sourceEffectTiming(source.visual, 0.7F);
        effect.totalSeconds = timing.visibleSeconds;
        effect.seconds = effect.totalSeconds;
        effect.emissionEndSeconds = timing.emissionSeconds;
        effect.ignoreRotation = source.ignoreRotation;
        effect.racer = racer;
        effect.vehicleEffect = index;
        effect.transform = vehicle.body;
        effect.transform.position = effect.origin;
        if (source.ignoreRotation)
            effect.transform.rotation = {};
        effects_.push_back(std::move(effect));
    }
}

void OriginalRaceSession::updateGameplay(
    float seconds,
    const std::vector<r3d::physics::VehicleState>& vehicles,
    const RaceControl& humanControl)
{
    const auto clutchImmune = [&](std::size_t racer) {
        if (racer >= race_.racers.size())
            return false;
        const auto& sourceRacer = race_.racers[racer];
        const auto& vehicle =
            sourceRacer.hasConfiguredVehicle
                ? sourceRacer.configuredVehicle
                : race_.vehicles.at(std::min(
                      sourceRacer.vehicle, race_.vehicles.size() - 1U));
        return vehicle.physics.clutchImmunity;
    };
    const auto pushEffectSound =
        [&](const std::vector<std::string>& sounds,
            const Vec3& position, std::size_t racer) {
            if (sounds.empty())
                return;
            RaceEvent event;
            event.kind = RaceEventKind::EffectSound;
            event.racer = racer;
            event.position = position;
            event.soundPath = sounds[sourceUniformRandomIndex(
                sounds.size(), sourceUniformRandomUnit())];
            events_.push_back(std::move(event));
        };
    for (std::size_t racer = 0;
         racer < racers_.size() && racer < vehicles.size(); ++racer)
    {
        auto& runtime = racers_[racer];
        if (!runtime.destroyed)
            continue;
        vehicleInputs_[racer] = {};
        if (runtime.restoreSeconds < 0.0F)
        {
            runtime.restoreSeconds = 0.0F;
            runtime.destroyed = false;
            continue;
        }
        runtime.restoreSeconds =
            std::max(0.0F, runtime.restoreSeconds - seconds);
        if (runtime.restoreSeconds <= 0.0F)
        {
            runtime.life = runtime.maximumLife;
            queueRespawn(racer, vehicles);
            // Keep the old car hidden until the reset request has reached
            // Jolt; it becomes live on the next session update.
            runtime.restoreSeconds = -1.0F;
        }
    }
    if (!racers_.empty() && humanControl.weaponSlot >= 0 &&
        humanControl.weaponSlot <
            static_cast<int>(PlayerProfile::weaponSlotCount))
    {
        const auto slot =
            static_cast<std::size_t>(humanControl.weaponSlot);
        if (racers_[0].weaponSlots[slot] !=
            RacerRuntime::invalidWeapon)
        {
            racers_[0].selectedWeaponSlot = slot;
            syncSelectedWeapon(racers_[0]);
        }
    }
    if (humanControl.changeWeapon && !racers_.empty())
    {
        auto& runtime = racers_[0];
        std::vector<std::size_t> usable;
        for (std::size_t slot = 0;
             slot < runtime.weaponSlots.size(); ++slot)
        {
            if (runtime.weaponSlots[slot] ==
                RacerRuntime::invalidWeapon)
                continue;
            usable.push_back(slot);
        }
        if (!usable.empty())
        {
            const auto current = std::find(
                usable.begin(), usable.end(),
                runtime.selectedWeaponSlot);
            const auto currentIndex =
                current == usable.end()
                    ? 0
                    : static_cast<int>(current - usable.begin());
            const int wanted = std::clamp(
                currentIndex +
                    (humanControl.weaponChange < 0 ? -1 : 1),
                0, static_cast<int>(usable.size()) - 1);
            runtime.selectedWeaponSlot =
                usable[static_cast<std::size_t>(wanted)];
            syncSelectedWeapon(runtime);
        }
    }
    auto directWeaponWorldTransform =
        [&](std::size_t owner, std::size_t weaponIndex) {
            return compose(
                vehicles[owner].body,
                race_.weapons[weaponIndex].visual.transform);
        };
    auto weaponWorldTransform =
        [&](std::size_t owner, std::size_t weaponIndex,
            std::size_t mountSlot) {
            Transform result = vehicles[owner].body;
            const auto& racerDefinition = race_.racers[owner];
            const auto& vehicleDefinition =
                racerDefinition.hasConfiguredVehicle
                    ? racerDefinition.configuredVehicle
                    : race_.vehicles.at(racerDefinition.vehicle);
            if (mountSlot < vehicleDefinition.weaponMounts.size())
            {
                const auto& mount =
                    vehicleDefinition.weaponMounts[mountSlot];
                Transform local;
                local.position = mount.position;
                const auto wanted =
                    recordName(race_.weapons[weaponIndex].record);
                const auto placement = std::find_if(
                    mount.placements.begin(),
                    mount.placements.end(),
                    [&](const VehicleWeaponPlacement& item) {
                        return recordName(item.record) == wanted;
                    });
                if (placement != mount.placements.end())
                {
                    local.position = add(
                        local.position, placement->offset);
                    local.rotation = placement->rotation;
                }
                result = compose(result, local);
            }
            Transform weaponLocal =
                race_.weapons[weaponIndex].visual.transform;
            if (owner < racers_.size() &&
                mountSlot <
                    racers_[owner].weaponSpinRadians.size())
            {
                const float halfAngle =
                    racers_[owner].weaponSpinRadians[mountSlot] * 0.5F;
                const Quat sourceSpin{
                    std::sin(halfAngle), 0.0F, 0.0F,
                    std::cos(halfAngle)};
                weaponLocal.rotation =
                    multiply(sourceSpin, weaponLocal.rotation);
            }
            result = compose(result, weaponLocal);
            return result;
        };
    auto projectileWorldTransform =
        [&](std::size_t owner, std::size_t weaponIndex,
            std::size_t mountSlot,
            const ProjectileDefinition& projectile) {
            Transform result = weaponWorldTransform(
                owner, weaponIndex, mountSlot);
            Transform localProjectile;
            localProjectile.position = projectile.position;
            localProjectile.rotation = projectile.rotation;
            return compose(result, localProjectile);
        };
    auto findClosestEnemy =
        [&](std::size_t source, float viewAngle) {
            if (source >= vehicles.size() ||
                source >= racers_.size())
                return RacerRuntime::invalidWeapon;
            const Vec3 sourcePosition =
                vehicles[source].body.position;
            const Vec3 sourceDirection = normalized3(
                forward(vehicles[source].body.rotation));
            std::size_t result = RacerRuntime::invalidWeapon;
            float minimumPlaneDistance = 0.0F;
            for (std::size_t candidate = 0;
                 candidate < vehicles.size() &&
                 candidate < racers_.size(); ++candidate)
            {
                if (candidate == source ||
                    racers_[candidate].finished ||
                    racers_[candidate].destroyed)
                    continue;
                const Vec3 difference = subtract(
                    vehicles[candidate].body.position,
                    sourcePosition);
                const float distance = length3(difference);
                if (distance <= 0.0001F)
                    continue;
                const float angle = dot3(
                    multiply(difference, 1.0F / distance),
                    sourceDirection);
                const float planeDistance =
                    std::abs(dot3(sourceDirection, difference));
                const bool nearest =
                    result == RacerRuntime::invalidWeapon ||
                    planeDistance < minimumPlaneDistance;
                const bool insideView =
                    viewAngle == 0.0F ||
                    angle >= std::cos(viewAngle);
                if (!nearest || !insideView)
                    continue;
                result = candidate;
                minimumPlaneDistance = planeDistance;
            }
            return result;
        };
    for (std::size_t racer = 0; racer < racers_.size(); ++racer)
    {
        auto& runtime = racers_[racer];
        const bool shieldWasActive = runtime.shieldSeconds > 0.0F;
        runtime.shieldSeconds =
            std::max(0.0F, runtime.shieldSeconds - seconds);
        if (runtime.shieldFadeInSeconds >= 0.0F)
        {
            runtime.shieldFadeInSeconds += seconds;
            if (runtime.shieldFadeInSeconds >= 0.5F)
                runtime.shieldFadeInSeconds = -1.0F;
        }
        if (runtime.shieldDamageSeconds >= 0.0F)
        {
            runtime.shieldDamageSeconds += seconds;
            if (runtime.shieldDamageSeconds >= 0.25F)
                runtime.shieldDamageSeconds = -1.0F;
        }
        if (shieldWasActive && runtime.shieldSeconds <= 0.0F)
            runtime.shieldFadeOutSeconds = 0.0F;
        if (runtime.shieldSeconds > 0.0F ||
            runtime.shieldFadeOutSeconds >= 0.0F)
        {
            runtime.shieldEffectSeconds += seconds;
        }
        if (runtime.shieldFadeOutSeconds >= 0.0F)
        {
            runtime.shieldFadeOutSeconds += seconds;
            if (runtime.shieldFadeOutSeconds >= 0.5F)
            {
                runtime.shieldEffectSeconds = 0.0F;
                runtime.shieldFadeOutSeconds = -1.0F;
                runtime.shieldDamageSeconds = -1.0F;
            }
        }
        runtime.speedBoostSeconds =
            std::max(0.0F, runtime.speedBoostSeconds - seconds);
        runtime.slowSeconds =
            std::max(0.0F, runtime.slowSeconds - seconds);
        if (runtime.slowSeconds <= 0.0F)
        {
            runtime.slowWeapon = RacerRuntime::invalidWeapon;
            runtime.slowProjectile = RacerRuntime::invalidWeapon;
        }
        runtime.clutchSeconds =
            std::max(0.0F, runtime.clutchSeconds - seconds);
        runtime.mineLockSeconds =
            std::max(0.0F, runtime.mineLockSeconds - seconds);
        runtime.springLockSeconds =
            std::max(0.0F, runtime.springLockSeconds - seconds);
        if (runtime.touchAttributionSeconds > 0.0F)
        {
            runtime.touchAttributionSeconds =
                std::max(
                    0.0F,
                    runtime.touchAttributionSeconds - seconds);
            if (runtime.touchAttributionSeconds <= 0.0F)
            {
                runtime.touchAttacker =
                    RacerRuntime::invalidWeapon;
            }
        }
        if (racer < vehicleInputs_.size())
        {
            vehicleInputs_[racer].springLocked =
                runtime.springLockSeconds > 0.0F;
        }
        for (auto& cooldown : weaponCooldown_[racer])
            cooldown = std::max(0.0F, cooldown - seconds);
        mineCooldown_[racer] =
            std::max(0.0F, mineCooldown_[racer] - seconds);
        hyperCooldown_[racer] =
            std::max(0.0F, hyperCooldown_[racer] - seconds);
        if (runtime.destroyed)
        {
            repairSeconds_[racer] = 0.0F;
            runtime.lowLife = false;
            runtime.lowLifeEffectSeconds = 0.0F;
            continue;
        }
        const WeaponDefinition* repair = nullptr;
        for (const auto weaponIndex : runtime.weaponSlots)
        {
            if (weaponIndex == RacerRuntime::invalidWeapon ||
                weaponIndex >= race_.weapons.size())
                continue;
            const auto& candidate = race_.weapons[weaponIndex];
            if (candidate.slot == WeaponSlot::Support &&
                candidate.repairPeriod > 0.0F)
            {
                repair = &candidate;
                break;
            }
        }
        if (repair == nullptr ||
            runtime.life >= runtime.maximumLife)
        {
            repairSeconds_[racer] = 0.0F;
        }
        else if ((repairSeconds_[racer] += seconds) >
                 repair->repairPeriod)
        {
            repairSeconds_[racer] -= repair->repairPeriod;
            runtime.life = std::min(
                runtime.maximumLife,
                runtime.life + 5.0F);
        }
        const auto& sourceRacer = race_.racers[racer];
        const auto& vehicleDefinition =
            sourceRacer.hasConfiguredVehicle
                ? sourceRacer.configuredVehicle
                : race_.vehicles.at(sourceRacer.vehicle);
        const bool lowLife =
            runtime.maximumLife > 0.0F && runtime.life > 0.0F &&
            runtime.life / runtime.maximumLife <
                vehicleDefinition.lowLifeLevel;
        if (lowLife)
        {
            runtime.lowLifeEffectSeconds += seconds;
            if (!runtime.lowLife)
            {
                runtime.lowLife = true;
                events_.push_back(
                    {RaceEventKind::LowLife, racer, 0U,
                     racer < vehicles.size()
                         ? vehicles[racer].body.position
                         : Vec3{},
                     runtime.life / runtime.maximumLife});
            }
        }
        else
        {
            runtime.lowLife = false;
            runtime.lowLifeEffectSeconds = 0.0F;
        }
    }

    auto pushDamageEvent =
        [&](std::size_t target, std::size_t attacker,
            const Vec3& position, float damage,
            DamageType damageType) {
            RaceEvent event;
            event.kind = RaceEventKind::Damage;
            event.racer = target;
            event.target = attacker;
            event.position = position;
            event.value = damage;
            event.touchDamage =
                damageType == DamageType::Touch;
            event.damageType = damageType;
            events_.push_back(std::move(event));
            if (target >= racers_.size())
                return;
            auto& runtime = racers_[target];
            // GameObject::Damage dispatches OnDamage even while immortal
            // (the applied life delta is zero), and ImmortalEffect keeps
            // listening until its fade-out object is freed.
            if (runtime.shieldSeconds > 0.0F ||
                runtime.shieldFadeInSeconds >= 0.0F ||
                runtime.shieldFadeOutSeconds >= 0.0F)
            {
                runtime.shieldDamageSeconds = 0.0F;
            }
            if (damageType != DamageType::Energy ||
                target >= race_.racers.size())
                return;
            // DataBase::LoadCar attaches one dtEnergy DamageEffect to each
            // car. EventEffect::MakeEffect does not replace or restart the
            // active 0.5-second damageEnergy<car> child.
            const bool alreadyActive = std::any_of(
                effects_.begin(), effects_.end(),
                [&](const RaceEffect& effect) {
                    return effect.kind ==
                               RaceEventKind::VehicleEnergyDamage &&
                           effect.racer == target;
                });
            if (alreadyActive)
                return;
            const auto& sourceRacer = race_.racers[target];
            const auto& vehicleDefinition =
                sourceRacer.hasConfiguredVehicle
                    ? sourceRacer.configuredVehicle
                    : race_.vehicles.at(sourceRacer.vehicle);
            const auto& visual =
                vehicleDefinition.energyDamageEffect;
            if (visual.visualNodes.empty() &&
                visual.particleEmitters.empty())
                return;
            RaceEffect effect;
            effect.kind = RaceEventKind::VehicleEnergyDamage;
            effect.racer = target;
            effect.origin = position;
            const auto timing = sourceEffectTiming(visual, 0.5F);
            effect.totalSeconds = timing.visibleSeconds;
            effect.seconds = effect.totalSeconds;
            effect.emissionEndSeconds = timing.emissionSeconds;
            effects_.push_back(std::move(effect));
        };
    auto applyRacerDamage =
        [&](std::size_t target, std::size_t attacker,
            const Vec3& position, float sourceDamage,
            DamageType damageType) {
            if (target >= racers_.size() ||
                target >= vehicles.size() ||
                racers_[target].destroyed)
                return false;
            const bool touch =
                damageType == DamageType::Touch;
            // Logic::Damage applies the first reflector before
            // GameObject::Damage tests immortality.  cPlayerDamage keeps
            // this reflected incoming value even when life cannot change.
            const float incoming = damageAfterSupport(
                target, sourceDamage, touch);
            auto& runtime = racers_[target];
            if (touch &&
                attacker != RacerRuntime::invalidWeapon)
            {
                // GameObject::Damage keeps _touchPlayerId for exactly
                // three seconds, including immortal contacts.
                runtime.touchAttacker = attacker;
                runtime.touchAttributionSeconds = 3.0F;
            }
            if (runtime.shieldSeconds <= 0.0F)
            {
                runtime.life = std::max(
                    0.0F, runtime.life - incoming);
            }
            pushDamageEvent(
                target, attacker, position, incoming, damageType);
            if (runtime.life > 0.0F)
                return false;
            destroyRacer(
                target, attacker, position, vehicles[target],
                damageType, damageType != DamageType::Mine);
            return true;
        };

    auto damageFromContact =
        [](const std::array<float, 2>& damage,
           const std::array<float, 2>& forceRange,
           float force, float& forcePart) {
        forcePart = 0.0F;
        if (forceRange[1] > forceRange[0])
        {
            forcePart = std::clamp(
                (force - forceRange[0]) /
                    (forceRange[1] - forceRange[0]),
                0.0F, 1.0F);
        }
        else if (force > forceRange[0])
        {
            forcePart = 0.5F;
        }
        return damage[0] + (damage[1] - damage[0]) * forcePart;
    };
    auto applyTouchDamage =
        [&](std::size_t target, std::size_t attacker,
            float damage, Vec3 position) {
        if (target >= racers_.size() || damage <= 0.0F ||
            racers_[target].destroyed)
            return;
        applyRacerDamage(
            target, attacker, position, damage,
            DamageType::Touch);
    };

    // DataBase::Init installs PairPxContactEffect globally. PhysX reports
    // every non-wheel manifold, the effect keeps at most two points per
    // actor pair, and OnProgress releases a missing point after 0.1 seconds.
    // spark2 then waits for its already emitted particles (maximum life 0.7)
    // before disappearing.
    constexpr float sourceContactForce = 10000.0F;
    constexpr float sourceContactRelease = 0.1F;
    constexpr float sourceContactParticleLife = 0.7F;
    for (std::size_t racer = 0;
         racer < vehicles.size() && racer < racers_.size(); ++racer)
    {
        if (racers_[racer].destroyed)
            continue;
        for (const auto& contact : vehicles[racer].bodyContacts)
        {
            if (contact.frictionForce <= sourceContactForce ||
                (contact.surface ==
                     r3d::physics::CollisionSurface::Vehicle &&
                 contact.otherVehicle < racer))
            {
                continue;
            }
            std::array<Vec3, 2> points{};
            std::size_t pointCount = std::min<std::size_t>(
                contact.points.size(), points.size());
            for (std::size_t index = 0; index < pointCount; ++index)
                points[index] = contact.points[index];
            if (pointCount == 0U)
            {
                points[0] = contact.hasPoint
                                ? contact.point
                                : vehicles[racer].body.position;
                pointCount = 1U;
            }
            const auto activePair = std::find_if(
                effects_.begin(), effects_.end(),
                [&](const RaceEffect& effect) {
                    return effect.kind == RaceEventKind::ContactImpact &&
                           effect.racer == racer &&
                           effect.contactSurface == contact.surface &&
                           effect.contactActor == contact.otherActor &&
                           effect.emissionEndSeconds >= effect.ageSeconds;
                });
            if (activePair == effects_.end())
            {
                pushEffectSound(
                    race_.contactSoundPaths, points[0], racer);
            }
            for (std::size_t index = 0; index < pointCount; ++index)
            {
                auto effect = std::find_if(
                    effects_.begin(), effects_.end(),
                    [&](const RaceEffect& value) {
                        return value.kind ==
                                   RaceEventKind::ContactImpact &&
                               value.racer == racer &&
                               value.contactSurface == contact.surface &&
                               value.contactActor == contact.otherActor &&
                               value.contactIndex == index &&
                               value.emissionEndSeconds >=
                                   value.ageSeconds;
                    });
                if (effect == effects_.end())
                {
                    RaceEffect created;
                    created.kind = RaceEventKind::ContactImpact;
                    created.racer = racer;
                    created.contactSurface = contact.surface;
                    created.contactActor = contact.otherActor;
                    created.contactIndex =
                        static_cast<std::uint8_t>(index);
                    created.totalSeconds =
                        sourceContactRelease +
                        sourceContactParticleLife;
                    created.seconds = created.totalSeconds;
                    created.ageSeconds = 0.0F;
                    created.emissionEndSeconds =
                        sourceContactRelease;
                    effects_.push_back(std::move(created));
                    effect = std::prev(effects_.end());
                }
                effect->origin = points[index];
                effect->target = add(points[index], contact.normal);
                effect->seconds =
                    sourceContactRelease +
                    sourceContactParticleLife;
                effect->emissionEndSeconds =
                    effect->ageSeconds + sourceContactRelease;
            }
        }
    }

    for (std::size_t racer = 0;
         racer < vehicles.size() && racer < racers_.size(); ++racer)
    {
        for (const auto& contact : vehicles[racer].bodyContacts)
        {
            if (contact.surface !=
                    r3d::physics::CollisionSurface::TrackBorder ||
                std::abs(contact.normal.z) >= 0.5F)
                continue;
            racers_[racer].clutchSeconds = 0.0F;
            float forcePart = 0.0F;
            const float damage = damageFromContact(
                race_.touchBorderDamage, race_.touchBorderDamageForce,
                contact.force, forcePart);
            if ((!springBorders_ && forcePart == 0.0F) ||
                vehicles[racer].speed <= 16.0F)
                continue;

            if (springBorders_)
            {
                const Vec3 normal = normalized3(contact.normal);
                const Vec3 velocity = vehicles[racer].linearVelocity;
                Vec3 tangent = subtract(
                    velocity, multiply(normal, dot3(normal, velocity)));
                const float tangentLength = length3(tangent);
                if (tangentLength > 0.0001F)
                    tangent = multiply(tangent, 1.0F / tangentLength);
                else
                    tangent = {};
                const Vec3 travel = normalized3(velocity);
                const float tangentDot =
                    std::abs(dot3(travel, normal));
                const Vec3 direction =
                    normalized3(forward(vehicles[racer].body.rotation));
                const float directionDot = dot3(direction, normal);
                const float directionTravelDot =
                    dot3(direction, travel);
                if (tangentDot > 0.1F &&
                    (directionDot < 0.707F ||
                     directionTravelDot < -0.707F))
                {
                    const float normalVelocity = std::clamp(
                        std::abs(dot3(normal, velocity)), 4.0F, 14.0F);
                    const float tangentVelocity =
                        dot3(tangent, velocity) * 0.5F;
                    Vec3 wanted = add(
                        multiply(normal, normalVelocity),
                        multiply(tangent, tangentVelocity));
                    wanted.z = 0.0F;
                    velocityRequests_.push_back(
                        {racer, subtract(wanted, velocity)});
                }
            }
            if (forcePart > 0.0F && damage > 0.0F)
                applyTouchDamage(
                    racer, racer, damage,
                    vehicles[racer].body.position);
        }
    }
    for (auto& cooldown : touchCooldown_)
        cooldown = std::max(0.0F, cooldown - seconds);

    auto spawnProjectileImpact =
        [&](const ProjectileRuntime& projectile, const Vec3& position,
            std::size_t targetRacer) {
            if (projectile.weapon >= race_.weapons.size() ||
                projectile.projectile >=
                    race_.weapons[projectile.weapon]
                        .projectiles.size())
                return;
            const auto& definition =
                race_.weapons[projectile.weapon]
                    .projectiles[projectile.projectile];
            auto addVisual =
                [&](const ObjectDefinition& visual,
                    std::uint8_t variant, Vec3 offset = {},
                    bool ignoreRotation = false,
                    bool targetChild = false) {
                    Transform effectTransform;
                    Vec3 effectOrigin = add(position, offset);
                    if (targetChild && targetRacer < vehicles.size())
                    {
                        const Transform& parent =
                            vehicles[targetRacer].body;
                        const Quat inverseRotation{
                            -parent.rotation.x, -parent.rotation.y,
                            -parent.rotation.z, parent.rotation.w};
                        const Vec3 unscaled = rotate(
                            inverseRotation,
                            subtract(position, parent.position));
                        const auto removeScale = [](float value,
                                                    float scale) {
                            return std::abs(scale) > 0.000001F
                                       ? value / scale
                                       : value;
                        };
                        effectTransform.position = {
                            removeScale(unscaled.x, parent.scale.x) +
                                offset.x,
                            removeScale(unscaled.y, parent.scale.y) +
                                offset.y,
                            removeScale(unscaled.z, parent.scale.z) +
                                offset.z};
                        effectOrigin =
                            compose(parent, effectTransform).position;
                    }
                    pushEffectSound(
                        visual.soundPaths, effectOrigin,
                        projectile.owner);
                    if (visual.visualNodes.empty() &&
                        visual.particleEmitters.empty())
                        return;
                    const auto timing =
                        sourceEffectTiming(visual, 0.9F);
                    RaceEffect impact;
                    impact.kind = RaceEventKind::ProjectileImpact;
                    impact.origin = effectOrigin;
                    impact.target =
                        add(impact.origin, projectile.direction);
                    impact.seconds = timing.visibleSeconds;
                    impact.totalSeconds = timing.visibleSeconds;
                    impact.emissionEndSeconds =
                        timing.emissionSeconds;
                    impact.weapon = projectile.weapon;
                    impact.projectile = projectile.projectile;
                    impact.visualVariant = variant;
                    impact.ignoreRotation = ignoreRotation;
                    if (targetChild && targetRacer < vehicles.size())
                    {
                        impact.parentRacer = targetRacer;
                        impact.transform = effectTransform;
                    }
                    effects_.push_back(std::move(impact));
                };
            addVisual(definition.secondaryVisual, 1U);
            addVisual(definition.tertiaryVisual, 2U);
            addVisual(definition.deathEffect.visual, 3U,
                      definition.deathEffect.position,
                      definition.deathEffect.ignoreRotation,
                      definition.deathEffect.targetChild);

            if (definition.deathProjectile ==
                    ProjectileDefinition::invalidProjectile ||
                definition.deathProjectile >=
                    race_.weapons[projectile.weapon]
                        .projectiles.size())
                return;
            const auto& spawned =
                race_.weapons[projectile.weapon]
                    .projectiles[definition.deathProjectile];
            if (spawned.type != 20U)
                return;
            MineRuntime crater;
            crater.owner = projectile.owner;
            crater.weapon = projectile.weapon;
            crater.projectile = definition.deathProjectile;
            crater.position = add(position, spawned.position);
            crater.damage = spawned.damage;
            crater.maximumLife = sampleSourceRange(
                spawned.minimumLife, spawned.maximumLife);
            crater.collision = spawned.collision;
            crater.type = spawned.type;
            crater.impulseSpeed = spawned.speed;
            crater.ignoreOwnerCollision =
                definition.deathEffect
                    .effectPhysicsIgnoreSenderCar;
            mines_.push_back(crater);
        };

    for (auto& projectile : projectiles_)
    {
        if (!projectile.active ||
            projectile.weapon >= race_.weapons.size() ||
            projectile.projectile >=
                race_.weapons[projectile.weapon].projectiles.size())
            continue;
        const auto& projectileDefinition =
            race_.weapons[projectile.weapon]
                .projectiles[projectile.projectile];
        projectile.ageSeconds += seconds;
        if (projectile.attached)
        {
            if (projectile.owner >= vehicles.size() ||
                projectile.owner >= racers_.size())
            {
                projectile.active = false;
                continue;
            }
            projectile.lifeSeconds -= seconds;
            Transform shotTransform;
            if (projectile.directWeapon)
            {
                Transform localProjectile;
                localProjectile.position =
                    projectileDefinition.position;
                localProjectile.rotation =
                    projectileDefinition.rotation;
                shotTransform = compose(
                    directWeaponWorldTransform(
                        projectile.owner, projectile.weapon),
                    localProjectile);
            }
            else
            {
                shotTransform = projectileWorldTransform(
                    projectile.owner, projectile.weapon,
                    projectile.mountSlot, projectileDefinition);
            }
            projectile.position = shotTransform.position;
            projectile.rotation = shotTransform.rotation;
            projectile.direction = normalized3(
                rotate(shotTransform.rotation,
                       {1.0F, 0.0F, 0.0F}));
            if (projectileDefinition.type == 14U)
            {
                // Proj::FireUpdate mirrors the current mounted weapon/car
                // actor velocity, allowing world-coordinate emitters to
                // subtract the correct source motion.
                projectile.velocity =
                    vehicles[projectile.owner].linearVelocity;
                projectile.speed = length3(projectile.velocity);
            }
            const float maximumDistance =
                projectile.maximumDistance > 0.0F
                    ? projectile.maximumDistance
                    : 3.0F;
            const bool sourceRay =
                projectileDefinition.type == 3U ||
                projectileDefinition.type == 18U;
            const bool sourceContact =
                projectileDefinition.type == 14U ||
                projectileDefinition.type == 15U;
            const Vec3 rayOrigin =
                add(projectile.position,
                    projectileDefinition.sizeAddPx);
            const WorldRayHit rayHit =
                sourceRay
                    ? raycastWorld(
                          race_, vehicles, racers_,
                          projectile.owner, rayOrigin,
                          projectile.direction, maximumDistance)
                    : WorldRayHit{};
            projectile.impactDistance =
                sourceRay && rayHit.hit
                    ? rayHit.distance
                    : (sourceRay ? maximumDistance : 0.0F);
            const Vec3 end = add(
                projectile.position,
                multiply(projectile.direction,
                         sourceRay ? projectile.impactDistance : 0.0F));
            effects_.push_back(
                {RaceEventKind::WeaponFired, projectile.position, end,
                 std::max(seconds, 0.03F),
                 std::max(seconds, 0.03F), projectile.weapon,
                 projectile.projectile, 0U,
                 RacerRuntime::invalidWeapon, false,
                 RacerRuntime::invalidWeapon,
                 RacerRuntime::invalidWeapon, {}});
            if (sourceRay &&
                rayHit.vehicle < vehicles.size() &&
                rayHit.vehicle < racers_.size())
            {
                const std::size_t target = rayHit.vehicle;
                applyRacerDamage(
                    target, projectile.owner, end,
                    std::max(
                        projectileDefinition.damage * seconds,
                        0.0F),
                    sourceProjectileDamageType(
                        projectileDefinition.type));
                if (projectileDefinition.type == 18U &&
                    racers_[target].slowSeconds <= 0.0F)
                {
                    const float duration =
                        projectileDefinition.tertiaryVisual
                                    .maximumTimeLife >
                                0.0F
                            ? projectileDefinition.tertiaryVisual
                                  .maximumTimeLife
                            : 1.0F;
                    racers_[target].slowSeconds = duration;
                    racers_[target].slowWeapon =
                        projectile.weapon;
                    racers_[target].slowProjectile =
                        projectile.projectile;
                }
            }
            else if (sourceContact)
            {
                const OrientedBox projectileBox = orientedBox(
                    shotTransform, projectileDefinition.collision);
                auto refreshDrobilkaContact =
                    [&](const Vec3& contactPoint) {
                        if (projectileDefinition.type != 15U)
                            return;
                        auto effect = std::find_if(
                            effects_.begin(), effects_.end(),
                            [&](const RaceEffect& value) {
                                return value.kind ==
                                           RaceEventKind::
                                               ProjectileImpact &&
                                       value.visualVariant == 4U &&
                                       value.racer ==
                                           projectile.owner &&
                                       value.weapon ==
                                           projectile.weapon &&
                                       value.projectile ==
                                           projectile.projectile &&
                                       value.mountSlot ==
                                           projectile.mountSlot;
                            });
                        if (effect == effects_.end())
                        {
                            RaceEffect contact;
                            contact.kind =
                                RaceEventKind::ProjectileImpact;
                            contact.weapon = projectile.weapon;
                            contact.projectile =
                                projectile.projectile;
                            contact.visualVariant = 4U;
                            contact.racer = projectile.owner;
                            contact.mountSlot =
                                projectile.mountSlot;
                            effects_.push_back(std::move(contact));
                            effect = std::prev(effects_.end());
                        }
                        effect->origin = contactPoint;
                        effect->target = add(
                            contactPoint, projectile.direction);
                        // Proj::DrobilkaContact resets _time1 to 0.5.
                        effect->seconds = 0.5F;
                        effect->totalSeconds = 0.5F;
                    };
                for (std::size_t target = 0;
                     target < vehicles.size() &&
                     target < racers_.size(); ++target)
                {
                    if (target == projectile.owner ||
                        racers_[target].finished ||
                        racers_[target].destroyed)
                        continue;
                    const auto& racerDefinition =
                        race_.racers[target];
                    const auto& vehicleDefinition =
                        racerDefinition.hasConfiguredVehicle
                            ? racerDefinition.configuredVehicle
                            : race_.vehicles.at(
                                  racerDefinition.vehicle);
                    const OrientedBox targetBox = vehicleBox(
                        vehicles[target],
                        vehicleDefinition.physics);
                    if (!boxesOverlap(projectileBox, targetBox))
                        continue;
                    const Vec3 contactPoint =
                        closestPoint(targetBox, projectileBox.center);
                    refreshDrobilkaContact(contactPoint);
                    applyRacerDamage(
                        target, projectile.owner,
                        contactPoint,
                        std::max(
                            projectileDefinition.damage * seconds,
                            0.0F),
                        DamageType::Simple);
                }
                Vec3 decorationContact;
                if (projectileDefinition.type == 15U &&
                    damageDecorationWithBox(
                        shotTransform,
                        projectileDefinition.collision,
                        std::max(
                            projectileDefinition.damage * seconds,
                            0.0F),
                        projectile.owner, &decorationContact))
                {
                    refreshDrobilkaContact(decorationContact);
                }
            }
            const float decorationDamage = std::max(
                projectileDefinition.damage * seconds, 0.0F);
            if (sourceRay)
            {
                damageDecorationAlongRay(
                    rayOrigin, projectile.direction,
                    projectile.impactDistance, decorationDamage,
                    projectile.owner);
            }
            else if (sourceContact &&
                     projectileDefinition.type != 15U)
            {
                damageDecorationWithBox(
                    shotTransform, projectileDefinition.collision,
                    decorationDamage, projectile.owner);
            }
            if (projectile.lifeSeconds <= 0.0F)
                projectile.active = false;
            if (projectileDefinition.type == 15U &&
                projectile.owner < racers_.size() &&
                projectile.mountSlot <
                    racers_[projectile.owner]
                        .weaponSpinRadians.size())
            {
                auto& angle =
                    racers_[projectile.owner]
                        .weaponSpinRadians[projectile.mountSlot];
                angle = std::fmod(
                    angle + projectileDefinition.angularSpeed * seconds,
                    6.28318530717958647692F);
            }
            continue;
        }
        if ((projectileDefinition.type == 2U ||
             projectileDefinition.type == 21U) &&
            projectile.target < vehicles.size() &&
            projectile.target < racers_.size() &&
            !racers_[projectile.target].finished &&
            !racers_[projectile.target].destroyed)
        {
            projectile.homingDelay =
                std::max(0.0F, projectile.homingDelay - seconds);
            if (projectile.homingDelay <= 0.0F)
            {
                const Vec3 difference = subtract(
                    vehicles[projectile.target].body.position,
                    projectile.position);
                const float targetDistance = length3(difference);
                if (targetDistance > 0.0001F)
                {
                    const Vec3 targetDirection =
                        targetDistance > 1.0F
                            ? multiply(
                                  difference,
                                  1.0F / targetDistance)
                            : normalized3(rotate(
                                  projectile.rotation,
                                  {1.0F, 0.0F, 0.0F}));
                    const Quat targetRotation =
                        shortestArcFromX(targetDirection);
                    if (projectile.angularSpeed > 0.0F)
                    {
                        projectile.rotation = quaternionSlerp(
                            projectile.rotation, targetRotation,
                            seconds * projectile.angularSpeed);
                    }
                    else
                    {
                        projectile.rotation = targetRotation;
                    }
                    projectile.direction = normalized3(rotate(
                        projectile.rotation,
                        {1.0F, 0.0F, 0.0F}));
                    const float steeredSpeed =
                        projectileDefinition.relativeSpeed
                            ? length3(projectile.velocity)
                            : std::max(
                                  dot3(
                                      projectile.velocity,
                                      projectile.direction),
                                  projectileDefinition.speed);
                    projectile.speed = std::max(
                        projectileDefinition.speed, steeredSpeed);
                    projectile.velocity = multiply(
                        projectile.direction, projectile.speed);
                }
            }
        }
        const Vec3 previous = projectile.position;
        const float speed = std::max(projectile.speed, 1.0F);
        projectile.lifeSeconds -= seconds;
        if (projectile.ballistic)
            projectile.velocity.z -= 20.0F * seconds;
        Vec3 movement = projectile.ballistic
                            ? multiply(projectile.velocity, seconds)
                            : multiply(projectile.direction,
                                       speed * seconds);
        const float step = length3(movement);
        projectile.position = add(projectile.position, movement);
        if (length3(movement) > 0.0001F)
            projectile.direction = normalized3(movement);
        projectile.distance += step;
        const bool sourceRocketUpdate =
            projectileDefinition.type == 0U ||
            projectileDefinition.type == 22U ||
            projectileDefinition.type == 23U;
        if (sourceRocketUpdate)
        {
            // Proj::RocketUpdate casts from pos + Z*4 against TrackPlane and
            // preserves the projectile's lowest established clearance.
            const auto trackHit = raycastTrackPlane(
                race_, add(projectile.position, {0.0F, 0.0F, 4.0F}));
            if (trackHit.hit)
            {
                const float height = std::max(
                    projectile.position.z - trackHit.position.z,
                    projectileDefinition.collision.halfExtents.z);
                if (projectile.trackClearance == 0.0F ||
                    projectile.trackClearance - height > 0.1F)
                {
                    projectile.trackClearance = height;
                }
                projectile.position.z =
                    trackHit.position.z + projectile.trackClearance;
            }
        }
        if (projectileDefinition.type == 23U &&
            std::abs(projectileDefinition.angularSpeed) > 0.0001F)
        {
            const float halfAngle =
                projectileDefinition.angularSpeed * seconds * 0.5F;
            const Quat sourceSpin{
                std::sin(halfAngle), 0.0F, 0.0F,
                std::cos(halfAngle)};
            projectile.rotation =
                multiply(projectile.rotation, sourceSpin);
        }
        projectile.reflectionCooldown =
            std::max(0.0F,
                     projectile.reflectionCooldown - seconds);
        if (projectileDefinition.type == 22U &&
            projectile.reflectionCooldown <= 0.0F)
        {
            Transform thunderTransform;
            thunderTransform.position = projectile.position;
            thunderTransform.rotation = projectile.rotation;
            Vec3 normal;
            if (length3(projectile.velocity) > 5.0F &&
                trackBorderContact(
                    race_,
                    orientedBox(
                        thunderTransform,
                        projectileDefinition.collision),
                    normal))
            {
                projectile.velocity =
                    thunderReflection(projectile.velocity, normal);
                projectile.direction =
                    normalized3(projectile.velocity);
                // ThunderContact only changes PhysX linear velocity. The
                // projectile actor/model rotation remains the shot rotation
                // (and Resonanse is the only RocketUpdate variant that spins
                // its actor explicitly).
                projectile.reflectionCooldown = 0.1F;
            }
        }
        effects_.push_back(
            {RaceEventKind::WeaponFired, previous,
             projectile.position, std::max(seconds, 0.03F),
             std::max(seconds, 0.03F), projectile.weapon,
             projectile.projectile, 0U,
             RacerRuntime::invalidWeapon, false,
             RacerRuntime::invalidWeapon,
             RacerRuntime::invalidWeapon, {}});

        for (std::size_t target = 0;
             target < vehicles.size() && target < racers_.size();
             ++target)
        {
            if (target == projectile.owner ||
                racers_[target].finished ||
                racers_[target].destroyed)
                continue;
            if (projectileDefinition.type == 21U &&
                projectile.target < racers_.size() &&
                target != projectile.target)
                continue;
            const auto& racerDefinition = race_.racers[target];
            const auto& vehicleDefinition =
                racerDefinition.hasConfiguredVehicle
                    ? racerDefinition.configuredVehicle
                    : race_.vehicles.at(racerDefinition.vehicle);
            Transform projectileTransform;
            projectileTransform.position = projectile.position;
            projectileTransform.rotation = projectile.rotation;
            const OrientedBox projectileBox = orientedBox(
                projectileTransform,
                projectileDefinition.collision);
            const OrientedBox targetBox = vehicleBox(
                vehicles[target], vehicleDefinition.physics);
            if (!boxesOverlap(projectileBox, targetBox))
                continue;
            const Vec3 contactPoint =
                closestPoint(targetBox, projectileBox.center);
            const bool sonarContact =
                projectileDefinition.type == 16U;
            const bool targetedImpulse =
                projectileDefinition.type == 21U &&
                projectile.target < racers_.size();
            const float sourceDamage =
                targetedImpulse
                    ? projectileDefinition.damage /
                          static_cast<float>(
                              projectile.hitCount + 1U)
                    : projectile.damage;
            const bool targetDestroyed = applyRacerDamage(
                target, projectile.owner, contactPoint,
                std::max(
                    sourceDamage *
                        (sonarContact ? seconds : 1.0F),
                    0.0F),
                sourceProjectileDamageType(
                    projectileDefinition.type));
            if (sonarContact)
            {
                const float targetMass =
                    std::max(vehicleDefinition.physics.mass, 1.0F);
                const Vec3 impulse = multiply(
                    projectile.velocity, projectileDefinition.mass);
                velocityRequests_.push_back(
                    {target, multiply(impulse, 1.0F / targetMass)});
                // AddContactForce(..., NX_IMPULSE) also applies the
                // off-centre angular impulse.  Use the source box inertia
                // with Jolt's world-space angular velocity boundary.
                const Vec3 lever = subtract(
                    contactPoint, vehicles[target].body.position);
                const Vec3 worldTorque = cross(lever, impulse);
                const Quat inverseRotation{
                    -vehicles[target].body.rotation.x,
                    -vehicles[target].body.rotation.y,
                    -vehicles[target].body.rotation.z,
                    vehicles[target].body.rotation.w};
                const Vec3 localTorque =
                    rotate(inverseRotation, worldTorque);
                const Vec3 half =
                    vehicleDefinition.physics.halfExtents;
                const Vec3 inertia{
                    targetMass *
                        (half.y * half.y + half.z * half.z) /
                        3.0F,
                    targetMass *
                        (half.x * half.x + half.z * half.z) /
                        3.0F,
                    targetMass *
                        (half.x * half.x + half.y * half.y) /
                        3.0F};
                const Vec3 localAngularDelta{
                    localTorque.x / std::max(inertia.x, 0.001F),
                    localTorque.y / std::max(inertia.y, 0.001F),
                    localTorque.z / std::max(inertia.z, 0.001F)};
                angularVelocityRequests_.push_back(
                    {target,
                     rotate(
                         vehicles[target].body.rotation,
                         localAngularDelta)});
            }
            if (projectileDefinition.type == 0U ||
                projectileDefinition.type == 2U ||
                projectileDefinition.type == 19U ||
                projectileDefinition.type == 22U ||
                projectileDefinition.type == 23U)
            {
                const Vec3 torqueDirection =
                    cross(contactPoint, projectile.direction);
                if (length3(torqueDirection) > 0.01F)
                {
                    angularVelocityRequests_.push_back(
                        {target,
                         rotate(
                             vehicles[target].body.rotation,
                             multiply(
                                 normalized3(torqueDirection),
                                 projectileDefinition.mass * 0.2F))});
                }
            }
            if (sonarContact)
            {
                continue;
            }
            if (projectileDefinition.type == 21U)
            {
                if (!targetedImpulse ||
                    targetDestroyed ||
                    ++projectile.hitCount > 2U)
                {
                    spawnProjectileImpact(
                        projectile, projectile.position, target);
                    projectile.active = false;
                    break;
                }
                std::size_t nextTarget = findClosestEnemy(
                    target, 1.57079632679489661923F);
                if (nextTarget == projectile.owner)
                {
                    nextTarget = findClosestEnemy(
                        nextTarget, 1.57079632679489661923F);
                    if (nextTarget == target)
                    {
                        nextTarget =
                            RacerRuntime::invalidWeapon;
                    }
                }
                if (nextTarget == RacerRuntime::invalidWeapon)
                {
                    spawnProjectileImpact(
                        projectile, projectile.position, target);
                    projectile.active = false;
                    break;
                }
                projectile.target = nextTarget;
                projectile.homingDelay = 0.0F;
                break;
            }
            spawnProjectileImpact(
                projectile, projectile.position, target);
            projectile.active = false;
            break;
        }
        Transform liveProjectileTransform;
        liveProjectileTransform.position = projectile.position;
        liveProjectileTransform.rotation = projectile.rotation;
        if (projectile.active &&
            projectileDefinition.type == 16U)
        {
            damageDecorationWithBox(
                liveProjectileTransform,
                projectileDefinition.collision,
                projectile.damage * seconds, projectile.owner);
        }
        else if (projectile.active &&
                 !(projectileDefinition.type == 21U &&
                   projectile.target < racers_.size()) &&
                 damageDecorationWithBox(
                     liveProjectileTransform,
                     projectileDefinition.collision,
                     projectile.damage, projectile.owner))
        {
            spawnProjectileImpact(
                projectile, projectile.position,
                RacerRuntime::invalidWeapon);
            projectile.active = false;
        }
        if (projectile.active &&
            projectile.lifeSeconds <= 0.0F)
        {
            spawnProjectileImpact(
                projectile, projectile.position,
                RacerRuntime::invalidWeapon);
            projectile.active = false;
        }
    }
    projectiles_.erase(
        std::remove_if(
            projectiles_.begin(), projectiles_.end(),
            [](const ProjectileRuntime& projectile) {
                return !projectile.active;
            }),
        projectiles_.end());

    if (!vehicles.empty())
    {
        if (humanControl.reset && !racers_[0].destroyed)
            queueRespawn(0, vehicles);
        previousPositions_[0] = vehicles[0].body.position;
    }
    for (std::size_t racer = 0;
         racer < vehicles.size() && racer < racers_.size(); ++racer)
    {
        if (racers_[racer].destroyed)
            continue;
        const auto& sourceRacer = race_.racers[racer];
        const auto& definition =
            sourceRacer.hasConfiguredVehicle
                ? sourceRacer.configuredVehicle
                : race_.vehicles.at(sourceRacer.vehicle);
        const OrientedBox body =
            vehicleBox(vehicles[racer], definition.physics);
        const float verticalRadius =
            std::abs(body.axes[0].z) * body.halfExtents[0] +
            std::abs(body.axes[1].z) * body.halfExtents[1] +
            std::abs(body.axes[2].z) * body.halfExtents[2];
        // Map::Map creates a +Z plane at world Z=0 with TouchDeath.  Any
        // car shape crossing that plane receives Death(dtDeathPlane).
        if (body.center.z - verticalRadius > 0.0F)
            continue;
        const std::size_t attacker =
            racers_[racer].touchAttributionSeconds > 0.0F
                ? racers_[racer].touchAttacker
                : RacerRuntime::invalidWeapon;
        destroyRacer(
            racer, attacker, vehicles[racer].body.position,
            vehicles[racer], DamageType::DeathPlane, false);
    }
    for (std::size_t racer = 1;
         racer < vehicles.size() && racer < racers_.size(); ++racer)
    {
        if (stuckSeconds_[racer] > 3.0F &&
            !racers_[racer].destroyed)
            queueRespawn(racer, vehicles);
    }

    auto pushShotEffect =
        [&](std::size_t weapon,
            const Transform& weaponTransform,
            const ProjectileDefinition& projectile) {
            if (weapon >= race_.weapons.size())
                return;
            const auto& source = race_.weapons[weapon].shotEffect;
            if ((source.visual.visualNodes.empty() &&
                 source.visual.particleEmitters.empty()) ||
                source.duration <= 0.0F)
                return;
            Transform local;
            local.position = add(projectile.position, source.position);
            RaceEffect effect;
            effect.kind = RaceEventKind::WeaponShotEffect;
            effect.transform = compose(
                weaponTransform, local);
            if (source.ignoreRotation)
                effect.transform.rotation = {};
            effect.origin = effect.transform.position;
            effect.target = add(
                effect.origin,
                rotate(effect.transform.rotation,
                       {1.0F, 0.0F, 0.0F}));
            const auto timing = sourceEffectTiming(
                source.visual, source.duration);
            effect.totalSeconds = timing.visibleSeconds;
            effect.seconds = timing.visibleSeconds;
            effect.emissionEndSeconds = timing.emissionSeconds;
            effect.weapon = weapon;
            effect.ignoreRotation = source.ignoreRotation;
            effects_.push_back(std::move(effect));
        };

    auto placeMine = [&](std::size_t owner) {
        if (owner >= vehicles.size() || owner >= racers_.size() ||
            racers_[owner].destroyed ||
            racers_[owner].mines == 0 || mineCooldown_[owner] > 0.0F)
            return;
        const std::size_t weapon = racers_[owner].mineWeapon;
        if (weapon == RacerRuntime::invalidWeapon ||
            weapon >= race_.weapons.size())
            return;
        const auto& projectiles = race_.weapons[weapon].projectiles;
        const auto* projectile =
            projectiles.empty() ? nullptr : &projectiles.front();
        if (projectile == nullptr)
            return;
        const Transform weaponTransform =
            directWeaponWorldTransform(owner, weapon);
        Transform localProjectile;
        localProjectile.position = projectile->position;
        localProjectile.rotation = projectile->rotation;
        const Vec3 rayPosition =
            compose(weaponTransform, localProjectile).position;
        const auto hit = raycastTrackPlane(
            race_, add(rayPosition, {0.0F, 0.0F, 2.0F}));
        if (!hit.hit)
            return;
        const float minimumZ =
            projectile->collision.center.z -
            projectile->collision.halfExtents.z;
        const float offset = std::max(-minimumZ, 0.01F);
        const Vec3 position =
            add(hit.position, {0.0F, 0.0F, offset});
        --racers_[owner].mines;
        mineCooldown_[owner] =
            std::max(race_.weapons[weapon].shotDelay, 0.0F);
        racers_[owner].mineLockSeconds = 0.4F;
        MineRuntime mine;
        mine.owner = owner;
        mine.weapon = weapon;
        mine.projectile = 0U;
        mine.position = position;
        mine.rotation = rotationWithUp(hit.normal);
        mine.damage = projectile->damage;
        mine.impulseSpeed = projectile->speed;
        mine.type = projectile->type;
        mine.collision = projectile->collision;
        if (projectile->minimumLife > 0.0F)
        {
            mine.maximumLife = sampleSourceRange(
                projectile->minimumLife,
                projectile->maximumLife);
        }
        pushShotEffect(weapon, weaponTransform, *projectile);
        mines_.push_back(mine);
        events_.push_back({RaceEventKind::MinePlaced, owner, weapon,
                           weaponTransform.position, 0.0F});
    };
    if (humanControl.useMine)
        placeMine(0);
    auto activateHyper = [&](std::size_t owner) {
        if (owner >= racers_.size() ||
            racers_[owner].destroyed ||
            racers_[owner].hyperCharge == 0 ||
            racers_[owner].hyperWeapon ==
                RacerRuntime::invalidWeapon ||
            racers_[owner].hyperWeapon >= race_.weapons.size() ||
            hyperCooldown_[owner] > 0.0F)
            return;
        const auto& weapon =
            race_.weapons[racers_[owner].hyperWeapon];
        if (weapon.projectiles.empty())
            return;
        const auto& projectile = weapon.projectiles.front();
        if (owner >= vehicles.size())
            return;
        const auto& racerDefinition = race_.racers[owner];
        const auto& vehicleDefinition =
            racerDefinition.hasConfiguredVehicle
                ? racerDefinition.configuredVehicle
                : race_.vehicles.at(racerDefinition.vehicle);
        if (projectile.type == 17U)
        {
            const auto wheelCount =
                vehicleDefinition.physics.wheels.size();
            if (wheelCount == 0U ||
                vehicles[owner].contactCount < wheelCount)
                return;
        }
        --racers_[owner].hyperCharge;
        hyperCooldown_[owner] =
            std::max(weapon.shotDelay, 0.0F);
        const Vec3 position = vehicles[owner].body.position;
        const float duration =
            projectile.minimumLife > 0.0F
                ? sampleSourceRange(
                      projectile.minimumLife,
                      projectile.maximumLife)
                : (projectile.type == 17U ? 0.5F : 2.0F);
        if (projectile.type == 17U)
        {
            velocityRequests_.push_back(
                {owner,
                 rotate(
                     vehicles[owner].body.rotation,
                     {0.0F, 0.0F, projectile.speed})});
            racers_[owner].springLockSeconds = 1.5F;
            if (owner < vehicleInputs_.size())
                vehicleInputs_[owner].springLocked = true;
        }
        else
        {
            velocityRequests_.push_back(
                {owner,
                 rotate(
                     vehicles[owner].body.rotation,
                     {projectile.speed, 0.0F, 0.0F})});
        }
        const Transform weaponTransform =
            directWeaponWorldTransform(
                owner, racers_[owner].hyperWeapon);
        if (projectile.type == 1U)
        {
            Transform localProjectile;
            localProjectile.position = projectile.position;
            localProjectile.rotation = projectile.rotation;
            const Transform projectileTransform =
                compose(weaponTransform, localProjectile);
            ProjectileRuntime runtimeProjectile;
            runtimeProjectile.owner = owner;
            runtimeProjectile.weapon =
                racers_[owner].hyperWeapon;
            runtimeProjectile.projectile = 0U;
            runtimeProjectile.position =
                projectileTransform.position;
            runtimeProjectile.direction = normalized3(
                rotate(
                    projectileTransform.rotation,
                    {1.0F, 0.0F, 0.0F}));
            runtimeProjectile.rotation =
                projectileTransform.rotation;
            runtimeProjectile.lifeSeconds = duration;
            runtimeProjectile.attached = true;
            runtimeProjectile.directWeapon = true;
            projectiles_.push_back(runtimeProjectile);
        }
        events_.push_back(
            {RaceEventKind::HyperActivated, owner,
             racers_[owner].hyperWeapon, position, duration});
        pushShotEffect(
            racers_[owner].hyperWeapon,
            weaponTransform,
            projectile);
    };
    if (humanControl.useHyper)
        activateHyper(0);

    std::vector<MineRuntime> spawnedMines;
    auto spawnMineDeathEffect = [&](const MineRuntime& mine) {
        if (mine.weapon >= race_.weapons.size() ||
            mine.projectile >=
                race_.weapons[mine.weapon].projectiles.size())
            return;
        const auto& definition =
            race_.weapons[mine.weapon].projectiles[mine.projectile];
        const DeathEffectDefinition* death =
            &definition.deathEffect;
        std::uint8_t deathVariant = 3U;
        if (mine.visualVariant == 1U &&
            definition.secondaryProjectile.valid)
        {
            death = &definition.secondaryProjectile.deathEffect;
            deathVariant = 5U;
        }
        else if (mine.visualVariant == 2U &&
                 definition.tertiaryProjectile.valid)
        {
            death = &definition.tertiaryProjectile.deathEffect;
            deathVariant = 6U;
        }
        pushEffectSound(
            death->visual.soundPaths,
            add(mine.position, death->position), mine.owner);
        if (death->visual.visualNodes.empty() &&
            death->visual.particleEmitters.empty())
            return;
        RaceEffect impact;
        impact.kind = RaceEventKind::ProjectileImpact;
        impact.origin = add(mine.position, death->position);
        impact.target = add(impact.origin, {0.0F, 0.0F, 1.0F});
        const auto timing = sourceEffectTiming(death->visual, 0.7F);
        impact.totalSeconds = timing.visibleSeconds;
        impact.seconds = impact.totalSeconds;
        impact.emissionEndSeconds = timing.emissionSeconds;
        impact.weapon = mine.weapon;
        impact.projectile = mine.projectile;
        impact.visualVariant = deathVariant;
        impact.ignoreRotation = death->ignoreRotation;
        effects_.push_back(std::move(impact));
    };
    for (auto& mine : mines_)
    {
        if (!mine.active)
            continue;
        mine.seconds += seconds;
        if (length2(mine.velocity) > 0.0F ||
            std::abs(mine.velocity.z) > 0.0F)
        {
            mine.position = add(
                mine.position, multiply(mine.velocity, seconds));
            mine.velocity.z -= 20.0F * seconds;
            const auto trackHit = raycastTrackPlane(
                race_, add(mine.position, {0.0F, 0.0F, 2.0F}));
            const float bottomOffset = std::max(
                -mine.collision.center.z +
                    mine.collision.halfExtents.z,
                0.01F);
            if (trackHit.hit &&
                mine.position.z <
                    trackHit.position.z + bottomOffset)
            {
                mine.position.z =
                    trackHit.position.z + bottomOffset;
                mine.velocity = {};
            }
        }
        if (mine.maximumLife > 0.0F &&
            mine.seconds > mine.maximumLife)
        {
            spawnMineDeathEffect(mine);
            mine.active = false;
            continue;
        }
        if (mine.type == 12U)
        {
            const auto& projectile =
                race_.weapons[mine.weapon].projectiles.front();
            const float splitTime =
                projectile.angularSpeed > 0.0F
                    ? projectile.angularSpeed
                    : 2.0F;
            if (mine.seconds >= splitTime)
            {
                if (projectile.secondaryProjectile.valid)
                {
                    const auto& source =
                        projectile.secondaryProjectile;
                    MineRuntime core = mine;
                    core.owner = RacerRuntime::invalidWeapon;
                    core.linkedToOwner = false;
                    core.type = source.type;
                    core.visualVariant = 1U;
                    core.damage = source.damage;
                    core.impulseSpeed = source.speed;
                    core.seconds = 0.0F;
                    core.maximumLife =
                        source.minimumLife > 0.0F
                            ? sampleSourceRange(
                                  source.minimumLife,
                                  source.maximumLife)
                            : -1.0F;
                    core.velocity = {};
                    core.collision = source.collision;
                    spawnedMines.push_back(core);
                }
                if (projectile.tertiaryProjectile.valid)
                {
                    const auto& source =
                        projectile.tertiaryProjectile;
                    for (std::size_t piece = 0; piece < 5U; ++piece)
                    {
                        MineRuntime fragment = mine;
                        fragment.owner =
                            RacerRuntime::invalidWeapon;
                        fragment.linkedToOwner = false;
                        fragment.type = source.type;
                        fragment.visualVariant = 2U;
                        fragment.damage = source.damage;
                        fragment.impulseSpeed = source.speed;
                        fragment.seconds = 0.0F;
                        fragment.maximumLife =
                            source.minimumLife > 0.0F
                                ? sampleSourceRange(
                                      source.minimumLife,
                                      source.maximumLife)
                                : -1.0F;
                        fragment.collision = source.collision;
                        fragment.velocity =
                            sourceMineRipFragmentVelocity();
                        spawnedMines.push_back(fragment);
                    }
                }
                spawnMineDeathEffect(mine);
                mine.active = false;
                continue;
            }
        }
        for (std::size_t racer = 0;
             racer < vehicles.size() && racer < racers_.size(); ++racer)
        {
            if (racers_[racer].destroyed)
                continue;
            if (mine.ignoreOwnerCollision && racer == mine.owner)
                continue;
            const bool armingOwner =
                mine.linkedToOwner &&
                racer == mine.owner &&
                mine.seconds < 0.25F;
            const bool targetMineLocked =
                racers_[racer].mineLockSeconds > 0.0F;
            bool sourceContactLocked = false;
            if (mine.type == 10U)
            {
                sourceContactLocked =
                    mine.seconds < 0.25F ||
                    targetMineLocked;
            }
            else if (mine.type != 20U)
            {
                const bool testsMineLock =
                    mine.type == 11U || mine.type == 12U;
                sourceContactLocked =
                    armingOwner ||
                    (testsMineLock && enableMineBug_ &&
                     targetMineLocked);
            }
            Transform mineTransform;
            mineTransform.position = mine.position;
            mineTransform.rotation = mine.rotation;
            const auto& vehicleDefinition =
                race_.racers[racer].hasConfiguredVehicle
                    ? race_.racers[racer].configuredVehicle
                    : race_.vehicles.at(
                          race_.racers[racer].vehicle);
            const OrientedBox targetBox =
                vehicleBox(
                    vehicles[racer],
                    vehicleDefinition.physics);
            const OrientedBox mineBox =
                orientedBox(mineTransform, mine.collision);
            if (sourceContactLocked ||
                !boxesOverlap(targetBox, mineBox))
                continue;
            const Vec3 contactPoint =
                closestPoint(targetBox, mineBox.center);
            if (mine.type == 10U)
            {
                if (racers_[racer].clutchSeconds > 0.0F ||
                    length3(
                        vehicles[racer].linearVelocity) <= 3.0F)
                    continue;
                if (clutchImmune(racer))
                    continue;
                const Vec3 direction = normalized2(
                    forward(vehicles[racer].body.rotation));
                const Vec3 right{-direction.y, direction.x, 0.0F};
                const float side = dot2(
                    right,
                    subtract(mine.position,
                             vehicles[racer].body.position));
                const float strength =
                    std::abs(side) > 0.1F && side > 0.0F
                        ? -mine.damage
                        : mine.damage;
                racers_[racer].clutchSeconds = 0.38F;
                angularVelocityRequests_.push_back(
                    {racer, {0.0F, 0.0F, strength}});
                continue;
            }
            applyRacerDamage(
                racer, mine.owner, contactPoint,
                std::max(
                    mine.type == 20U
                        ? mine.damage * seconds
                        : mine.damage,
                    0.0F),
                DamageType::Mine);
            if (mine.type != 20U &&
                mine.impulseSpeed != 0.0F)
            {
                const float targetMass =
                    std::max(
                        vehicleDefinition.physics.mass, 1.0F);
                const Vec3 impulse{
                    0.0F, 0.0F, mine.impulseSpeed};
                velocityRequests_.push_back(
                    {racer,
                     multiply(impulse, 1.0F / targetMass)});
                const Vec3 lever = subtract(
                    contactPoint,
                    vehicles[racer].body.position);
                const Vec3 worldTorque =
                    cross(lever, impulse);
                const Quat inverseRotation{
                    -vehicles[racer].body.rotation.x,
                    -vehicles[racer].body.rotation.y,
                    -vehicles[racer].body.rotation.z,
                    vehicles[racer].body.rotation.w};
                const Vec3 localTorque =
                    rotate(inverseRotation, worldTorque);
                const Vec3 half =
                    vehicleDefinition.physics.halfExtents;
                const Vec3 inertia{
                    targetMass *
                        (half.y * half.y +
                         half.z * half.z) /
                        3.0F,
                    targetMass *
                        (half.x * half.x +
                         half.z * half.z) /
                        3.0F,
                    targetMass *
                        (half.x * half.x +
                         half.y * half.y) /
                        3.0F};
                const Vec3 localAngularDelta{
                    localTorque.x /
                        std::max(inertia.x, 0.001F),
                    localTorque.y /
                        std::max(inertia.y, 0.001F),
                    localTorque.z /
                        std::max(inertia.z, 0.001F)};
                angularVelocityRequests_.push_back(
                    {racer,
                     rotate(
                         vehicles[racer].body.rotation,
                         localAngularDelta)});
            }
            if (mine.type != 20U)
                spawnMineDeathEffect(mine);
            if (mine.type != 20U)
            {
                mine.active = false;
                break;
            }
        }
    }
    mines_.insert(mines_.end(), spawnedMines.begin(),
                  spawnedMines.end());
    mines_.erase(
        std::remove_if(mines_.begin(), mines_.end(),
                       [](const MineRuntime& mine) {
                           return !mine.active ||
                                  (mine.maximumLife > 0.0F &&
                                   mine.seconds > mine.maximumLife);
                       }),
        mines_.end());

    auto spawnBonusDeathEffect = [&](std::size_t bonusIndex) {
        if (bonusIndex >= race_.bonuses.size())
            return;
        const auto& bonus = race_.bonuses[bonusIndex];
        const auto& visual = bonus.deathEffect.visual;
        if (visual.record.empty() &&
            visual.visualNodes.empty() &&
            visual.particleEmitters.empty() &&
            visual.soundPaths.empty())
            return;
        pushEffectSound(
            visual.soundPaths,
            add(bonus.transform.position,
                bonus.deathEffect.position),
            RacerRuntime::invalidWeapon);
        RaceEffect impact;
        impact.kind = RaceEventKind::ProjectileImpact;
        impact.origin = add(
            bonus.transform.position,
            bonus.deathEffect.position);
        impact.target = add(
            bonus.transform.position, {0.0F, 0.0F, 2.0F});
        const auto timing = sourceEffectTiming(visual, 0.7F);
        impact.totalSeconds = timing.visibleSeconds;
        impact.seconds = impact.totalSeconds;
        impact.emissionEndSeconds = timing.emissionSeconds;
        impact.weapon = race_.weapons.size();
        impact.bonus = bonusIndex;
        impact.ignoreRotation =
            bonus.deathEffect.ignoreRotation;
        effects_.push_back(std::move(impact));
    };

    for (std::size_t bonusIndex = 0;
         bonusIndex < race_.bonuses.size(); ++bonusIndex)
    {
        if (!bonusActive_[bonusIndex])
            continue;
        for (std::size_t racer = 0;
             racer < vehicles.size() && racer < racers_.size(); ++racer)
        {
            auto& runtime = racers_[racer];
            if (runtime.destroyed)
                continue;
            const auto& bonus = race_.bonuses[bonusIndex];
            const auto& racerDefinition = race_.racers[racer];
            const auto& vehicleDefinition =
                racerDefinition.hasConfiguredVehicle
                    ? racerDefinition.configuredVehicle
                    : race_.vehicles.at(racerDefinition.vehicle);
            const OrientedBox targetBox =
                vehicleBox(
                    vehicles[racer],
                    vehicleDefinition.physics);
            const OrientedBox bonusBox =
                orientedBox(
                    bonus.transform, bonus.collision);
            if (!boxesOverlap(targetBox, bonusBox))
                continue;
            const Vec3 contactPoint =
                closestPoint(targetBox, bonusBox.center);

            if (bonus.kind == BonusKind::Speed)
            {
                const Vec3 wanted = multiply(
                    normalized3(forward(bonus.transform.rotation)),
                    bonus.value);
                velocityRequests_.push_back(
                    {racer,
                     subtract(wanted, vehicles[racer].linearVelocity)});
                continue;
            }
            if (bonus.kind == BonusKind::SlowHazard)
            {
                const float speed =
                    length3(vehicles[racer].linearVelocity);
                if (speed > 1.0F && speed > bonus.value)
                {
                    const Vec3 wanted = multiply(
                        normalized3(vehicles[racer].linearVelocity),
                        bonus.value);
                    velocityRequests_.push_back(
                        {racer,
                         subtract(wanted,
                                  vehicles[racer].linearVelocity)});
                }
                continue;
            }
            if (bonus.kind == BonusKind::OilHazard)
            {
                if (runtime.mineLockSeconds <= 0.0F &&
                    runtime.clutchSeconds <= 0.0F &&
                    length3(vehicles[racer].linearVelocity) >
                        3.0F &&
                    !clutchImmune(racer))
                {
                    const Vec3 direction = normalized2(
                        forward(vehicles[racer].body.rotation));
                    const Vec3 right{-direction.y, direction.x, 0.0F};
                    const float side = dot2(
                        right,
                        subtract(bonus.transform.position,
                                 vehicles[racer].body.position));
                    const float strength =
                        std::abs(side) > 0.1F && side > 0.0F
                            ? -bonus.value
                            : bonus.value;
                    runtime.clutchSeconds = 0.38F;
                    angularVelocityRequests_.push_back(
                        {racer, {0.0F, 0.0F, strength}});
                }
                continue;
            }
            if (bonus.kind == BonusKind::MineHazard)
            {
                if (enableMineBug_ &&
                    runtime.mineLockSeconds > 0.0F)
                    continue;
                applyRacerDamage(
                    racer, RacerRuntime::invalidWeapon,
                    contactPoint,
                    std::max(bonus.value, 0.0F),
                    DamageType::Mine);
                spawnBonusDeathEffect(bonusIndex);
                const float mass =
                    std::max(vehicleDefinition.physics.mass, 1.0F);
                if (bonus.speed > 0.0F)
                {
                    const Vec3 impulse{
                        0.0F, 0.0F, bonus.speed};
                    velocityRequests_.push_back(
                        {racer,
                         multiply(impulse, 1.0F / mass)});
                    const Vec3 lever = subtract(
                        contactPoint,
                        vehicles[racer].body.position);
                    const Vec3 worldTorque =
                        cross(lever, impulse);
                    const Quat inverseRotation{
                        -vehicles[racer].body.rotation.x,
                        -vehicles[racer].body.rotation.y,
                        -vehicles[racer].body.rotation.z,
                        vehicles[racer].body.rotation.w};
                    const Vec3 localTorque =
                        rotate(inverseRotation, worldTorque);
                    const Vec3 half =
                        vehicleDefinition.physics.halfExtents;
                    const Vec3 inertia{
                        mass *
                            (half.y * half.y +
                             half.z * half.z) /
                            3.0F,
                        mass *
                            (half.x * half.x +
                             half.z * half.z) /
                            3.0F,
                        mass *
                            (half.x * half.x +
                             half.y * half.y) /
                            3.0F};
                    const Vec3 localAngularDelta{
                        localTorque.x /
                            std::max(inertia.x, 0.001F),
                        localTorque.y /
                            std::max(inertia.y, 0.001F),
                        localTorque.z /
                            std::max(inertia.z, 0.001F)};
                    angularVelocityRequests_.push_back(
                        {racer,
                         rotate(
                             vehicles[racer].body.rotation,
                             localAngularDelta)});
                }
                bonusActive_[bonusIndex] = false;
                break;
            }

            PickSlot pickSlot = PickSlot::None;
            switch (bonus.kind)
            {
            case BonusKind::Money:
                runtime.pickedMoney +=
                    static_cast<std::uint32_t>(std::max(bonus.value, 0.0F));
                break;
            case BonusKind::Medpack:
                runtime.life = std::min(
                    runtime.life +
                        (bonus.value > 0.0F
                             ? bonus.value
                             : runtime.maximumLife),
                    runtime.maximumLife);
                break;
            case BonusKind::Ammunition:
            {
                struct RechargeTarget
                {
                    std::uint32_t* current = nullptr;
                    std::uint32_t capacity = 0;
                    std::size_t weapon =
                        RacerRuntime::invalidWeapon;
                    PickSlot pickSlot = PickSlot::None;
                };
                std::vector<RechargeTarget> targets;
                for (std::size_t slot = 0;
                     slot < runtime.weaponSlots.size(); ++slot)
                {
                    const auto weapon = runtime.weaponSlots[slot];
                    if (weapon == RacerRuntime::invalidWeapon ||
                        weapon >= race_.weapons.size() ||
                        runtime.weaponCharges[slot] >=
                            runtime.weaponCapacity[slot])
                        continue;
                    targets.push_back(
                        {&runtime.weaponCharges[slot],
                         runtime.weaponCapacity[slot], weapon,
                         PickSlot::Primary});
                }
                if (runtime.hyperWeapon !=
                        RacerRuntime::invalidWeapon &&
                    runtime.hyperWeapon < race_.weapons.size() &&
                    runtime.hyperCharge < runtime.hyperCapacity)
                {
                    targets.push_back(
                        {&runtime.hyperCharge, runtime.hyperCapacity,
                         runtime.hyperWeapon, PickSlot::Hyper});
                }
                if (runtime.mineWeapon !=
                        RacerRuntime::invalidWeapon &&
                    runtime.mineWeapon < race_.weapons.size() &&
                    runtime.mines < runtime.mineCapacity)
                {
                    targets.push_back(
                        {&runtime.mines, runtime.mineCapacity,
                         runtime.mineWeapon, PickSlot::Mine});
                }
                if (!targets.empty())
                {
                    auto& target = targets[
                        sourceRoundedRandomIndex(
                            targets.size(),
                            sourceRandomUnit())];
                    const auto maximumCharge =
                        race_.weapons[target.weapon].maximumCharge;
                    const auto amount =
                        sourceBonusCharge(
                            maximumCharge, bonus.value);
                    *target.current = std::min(
                        *target.current + amount, target.capacity);
                    pickSlot = target.pickSlot;
                }
                syncSelectedWeapon(runtime);
                break;
            }
            case BonusKind::Shield:
                if (runtime.shieldSeconds <= 0.0F)
                {
                    runtime.shieldEffectSeconds = 0.0F;
                    runtime.shieldFadeInSeconds = 0.0F;
                    runtime.shieldFadeOutSeconds = -1.0F;
                    runtime.shieldDamageSeconds = -1.0F;
                }
                runtime.shieldSeconds = std::max(bonus.value, 0.0F);
                break;
            case BonusKind::Speed:
            case BonusKind::SlowHazard:
            case BonusKind::OilHazard:
            case BonusKind::MineHazard:
            case BonusKind::Unknown:
                break;
            }
            bonusActive_[bonusIndex] = false;
            spawnBonusDeathEffect(bonusIndex);
            events_.push_back({RaceEventKind::Bonus, racer, bonusIndex,
                               bonus.transform.position, bonus.value,
                               pickSlot});
            break;
        }
    }

    auto fireWeapon =
        [&](std::size_t shooter, float minimumCooldown = 0.03F,
            std::size_t requestedTarget =
                RacerRuntime::invalidWeapon) {
        if (shooter >= vehicles.size() ||
            shooter >= racers_.size() || racers_[shooter].finished ||
            racers_[shooter].destroyed)
            return;
        auto& runtime = racers_[shooter];
        syncSelectedWeapon(runtime);
        if (runtime.selectedWeapon == RacerRuntime::invalidWeapon ||
            runtime.selectedWeapon >= race_.weapons.size() ||
            runtime.selectedWeaponSlot >= runtime.weaponCharges.size() ||
            weaponCooldown_[shooter][runtime.selectedWeaponSlot] > 0.0F ||
            runtime.weaponCharges[runtime.selectedWeaponSlot] == 0)
            return;
        const std::size_t firedSlot = runtime.selectedWeaponSlot;
        const std::size_t firedWeapon = runtime.selectedWeapon;
        const auto* weapon =
            &race_.weapons[firedWeapon];
        if (weapon->slot == WeaponSlot::Support)
            return;
        --runtime.weaponCharges[firedSlot];
        syncSelectedWeapon(runtime);
        weaponCooldown_[shooter][firedSlot] =
            std::max(weapon->shotDelay, minimumCooldown);
        const Vec3 eventOrigin = weaponWorldTransform(
            shooter, firedWeapon, firedSlot).position;
        std::size_t target = racers_.size();
        for (std::size_t projectileIndex = 0;
             projectileIndex < weapon->projectiles.size();
             ++projectileIndex)
        {
            const auto& projectile =
                weapon->projectiles[projectileIndex];
            if (projectile.spawnOnParentDeath)
                continue;
            const auto shotTransform = projectileWorldTransform(
                shooter, firedWeapon, firedSlot, projectile);
            const Vec3 projectileOrigin = shotTransform.position;
            const Vec3 sourceDirection = normalized3(
                rotate(shotTransform.rotation,
                       {1.0F, 0.0F, 0.0F}));
            Vec3 launchDirection = sourceDirection;
            // Proj::CalcSpeed levels only projectiles prepared through
            // RocketPrepare. Laser/FrostRay/Drobilka keep the weapon actor's
            // full 3D direction; applying CalcSpeed to them changed both ray
            // hits and visible beam alignment on slopes.
            const bool rocketPrepared =
                projectile.type == 0U || projectile.type == 2U ||
                projectile.type == 14U || projectile.type == 16U ||
                projectile.type == 19U || projectile.type == 21U ||
                projectile.type == 22U || projectile.type == 23U;
            if (rocketPrepared &&
                std::abs(launchDirection.z) < 0.707F)
                launchDirection = normalized2(launchDirection);
            const float forwardVehicleSpeed = std::max(
                dot3(launchDirection,
                     vehicles[shooter].linearVelocity),
                0.0F);
            // HumanPlayer::Shot(WeaponType) asks Player for the closest
            // enemy in pi/5.5, except sphereGun which passes viewAngle=0.
            const float homingViewAngle =
                recordName(weapon->record) == "sphereGun"
                    ? 0.0F
                    : 3.14159265358979323846F / 5.5F;
            const std::size_t homingTarget =
                requestedTarget < racers_.size()
                    ? requestedTarget
                    : findClosestEnemy(shooter, homingViewAngle);
            const bool rayProjectile =
                projectile.speed <= 0.0F;
            const bool attachedProjectile =
                projectile.type == 3U ||
                projectile.type == 14U ||
                projectile.type == 15U ||
                projectile.type == 18U;
            const float projectileDistance =
                projectile.maximumDistance > 0.0F
                    ? projectile.maximumDistance
                    : 100.0F;
            float targetDistance = projectileDistance;
            std::size_t projectileTarget = racers_.size();
            if (rayProjectile && !attachedProjectile)
            {
                const auto rayHit = raycastWorld(
                    race_, vehicles, racers_, shooter,
                    add(projectileOrigin, projectile.sizeAddPx),
                    sourceDirection, projectileDistance);
                if (rayHit.hit)
                {
                    targetDistance = rayHit.distance;
                    projectileTarget = rayHit.vehicle;
                }
            }
            Vec3 end = add(
                projectileOrigin,
                multiply(
                    rayProjectile || attachedProjectile
                        ? sourceDirection
                        : launchDirection,
                    rayProjectile && !attachedProjectile
                        ? targetDistance
                        : projectileDistance));
            if (attachedProjectile)
            {
                ProjectileRuntime runtimeProjectile;
                runtimeProjectile.owner = shooter;
                runtimeProjectile.weapon = firedWeapon;
                runtimeProjectile.projectile = projectileIndex;
                runtimeProjectile.mountSlot = firedSlot;
                runtimeProjectile.position = projectileOrigin;
                runtimeProjectile.direction = sourceDirection;
                runtimeProjectile.rotation = shotTransform.rotation;
                if (projectile.type == 14U)
                {
                    // FireUpdate copies the mounted weapon actor velocity on
                    // every source tick before relocating the contact box.
                    runtimeProjectile.velocity =
                        vehicles[shooter].linearVelocity;
                }
                runtimeProjectile.maximumDistance =
                    projectileDistance;
                runtimeProjectile.damage = projectile.damage;
                runtimeProjectile.angularSpeed =
                    projectile.angularSpeed;
                runtimeProjectile.lifeSeconds = sampleSourceRange(
                    projectile.minimumLife,
                    projectile.maximumLife);
                runtimeProjectile.attached = true;
                projectiles_.push_back(runtimeProjectile);
            }
            else if (!rayProjectile)
            {
                float speed = projectile.speed;
                if (projectile.relativeSpeed)
                {
                    speed += forwardVehicleSpeed;
                }
                else if (projectile.relativeSpeedMinimum > 0.0F)
                {
                    speed = std::max(
                        speed,
                        projectile.relativeSpeedMinimum +
                            forwardVehicleSpeed);
                }
                ProjectileRuntime runtimeProjectile;
                runtimeProjectile.owner = shooter;
                runtimeProjectile.weapon = firedWeapon;
                runtimeProjectile.projectile = projectileIndex;
                runtimeProjectile.mountSlot = firedSlot;
                runtimeProjectile.position = projectileOrigin;
                runtimeProjectile.direction = launchDirection;
                runtimeProjectile.rotation = shotTransform.rotation;
                runtimeProjectile.speed = speed;
                runtimeProjectile.velocity =
                    multiply(launchDirection, speed);
                runtimeProjectile.maximumDistance =
                    projectileDistance;
                runtimeProjectile.damage = projectile.damage;
                runtimeProjectile.angularSpeed =
                    projectile.angularSpeed;
                runtimeProjectile.lifeSeconds = std::max(
                    projectile.speed > 0.0F
                        ? projectile.maximumDistance /
                              projectile.speed
                        : 0.0F,
                    sampleSourceRange(
                        projectile.minimumLife,
                        projectile.maximumLife));
                runtimeProjectile.ballistic =
                    projectile.type == 19U;
                if (projectile.type == 2U ||
                    projectile.type == 21U)
                {
                    runtimeProjectile.homingDelay = 0.4F;
                    runtimeProjectile.target = homingTarget;
                }
                projectiles_.push_back(runtimeProjectile);
                end = add(
                    projectileOrigin,
                    multiply(
                        launchDirection,
                        std::min(std::max(speed * 0.03F, 0.5F),
                                 projectileDistance)));
            }
            else if (projectileTarget < racers_.size())
            {
                target = projectileTarget;
                applyRacerDamage(
                    target, shooter, end,
                    std::max(projectile.damage, 0.0F),
                    sourceProjectileDamageType(projectile.type));
            }
            effects_.push_back(
                {RaceEventKind::WeaponFired, projectileOrigin, end,
                 (rayProjectile || attachedProjectile) ? 0.12F : 0.03F,
                 (rayProjectile || attachedProjectile) ? 0.12F : 0.03F,
                 firedWeapon, projectileIndex, 0U,
                 RacerRuntime::invalidWeapon, false,
                 RacerRuntime::invalidWeapon,
                 RacerRuntime::invalidWeapon, {}});
            pushShotEffect(
                firedWeapon,
                weaponWorldTransform(
                    shooter, firedWeapon, firedSlot),
                projectile);
        }
        events_.push_back({RaceEventKind::WeaponFired, shooter, target,
                           eventOrigin, 5.0F,
                           PickSlot::None, firedWeapon});
    };
    if (humanControl.useWeapon)
        fireWeapon(0);
    if (humanControl.fireWeaponSlot >= 0 && !racers_.empty() &&
        humanControl.fireWeaponSlot <
            static_cast<int>(PlayerProfile::weaponSlotCount))
    {
        auto& runtime = racers_.front();
        const auto requested =
            static_cast<std::size_t>(humanControl.fireWeaponSlot);
        if (runtime.weaponSlots[requested] !=
            RacerRuntime::invalidWeapon)
        {
            const auto selected = runtime.selectedWeaponSlot;
            runtime.selectedWeaponSlot = requested;
            syncSelectedWeapon(runtime);
            fireWeapon(0);
            runtime.selectedWeaponSlot = selected;
            syncSelectedWeapon(runtime);
        }
    }
    if (humanControl.useAllWeapons && !racers_.empty())
    {
        auto& runtime = racers_.front();
        const auto selected = runtime.selectedWeaponSlot;
        for (std::size_t slot = 0;
             slot < runtime.weaponSlots.size(); ++slot)
        {
            if (runtime.weaponSlots[slot] ==
                    RacerRuntime::invalidWeapon ||
                runtime.weaponCharges[slot] == 0U)
                continue;
            runtime.selectedWeaponSlot = slot;
            syncSelectedWeapon(runtime);
            fireWeapon(0);
        }
        runtime.selectedWeaponSlot = selected;
        syncSelectedWeapon(runtime);
    }
    auto raceProgress = [&](std::size_t racer) {
        TraceNodeRef node =
            racer < lastTraceNodes_.size() &&
                    lastTraceNodes_[racer].valid()
                ? lastTraceNodes_[racer]
                : racerTraceNode(racer);
        if (!node.valid())
            return 0.0F;
        const float pathLength = tracePathLength(node.path);
        if (pathLength <= 0.0001F)
            return 0.0F;
        const float coordinate =
            racer < lastPathCoordinates_.size()
                ? lastPathCoordinates_[racer]
                : 0.0F;
        return std::clamp(
            traceDistance(node, coordinate) / pathLength,
            0.0F, 1.0F);
    };
    for (std::size_t racer = 1;
         racer < racers_.size() && racer < vehicles.size(); ++racer)
    {
        auto& runtime = racers_[racer];
        // AttackState::Update returns immediately for curTile == NULL.  A
        // retained lastNode is valid for progress/respawn, not for aiming,
        // mine placement or Hyper activation.
        if (runtime.destroyed ||
            racer >= currentTraceNodes_.size() ||
            !currentTraceNodes_[racer].valid())
            continue;
        const Vec3 carDirection =
            normalized2(forward(vehicles[racer].body.rotation));
        const Vec3 carDirection3 =
            normalized3(forward(vehicles[racer].body.rotation));
        auto carRadius = [&](std::size_t target) {
            const auto& definition = race_.racers[target];
            const auto& vehicle =
                definition.hasConfiguredVehicle
                    ? definition.configuredVehicle
                    : race_.vehicles.at(definition.vehicle);
            const auto half = vehicle.physics.halfExtents;
            return std::max(
                std::sqrt(half.x * half.x + half.y * half.y +
                          half.z * half.z),
                0.5F);
        };
        auto zLevelContains = [&](std::size_t target) {
            TraceNodeRef node = currentTraceNodes_[racer];
            const auto& path = tracePathAt(node.path);
            node.node = std::min(node.node, path.size() - 2U);
            const auto& start = tracePoint(node.path, node.node);
            const auto& end = tracePoint(node.path, node.node + 1U);
            const Vec3 direction = normalized2(
                subtract(end.position, start.position));
            const float segmentLength = std::max(
                length2(subtract(end.position, start.position)),
                0.0001F);
            const float part = std::clamp(
                dot2(subtract(
                         vehicles[target].body.position,
                         start.position),
                     direction) /
                    segmentLength,
                0.0F, 1.0F);
            const float level =
                start.position.z +
                (end.position.z - start.position.z) * part;
            const float height =
                (start.width +
                 (end.width - start.width) * part) *
                0.5F;
            return level - vehicles[target].body.position.z <
                   height;
        };
        auto findEnemy =
            [&](int direction,
                std::size_t currentTarget) {
                std::size_t enemy =
                    RacerRuntime::invalidWeapon;
                float minimumPlaneDistance = 0.0F;
                for (std::size_t target = 0;
                     target < vehicles.size() &&
                     target < racers_.size(); ++target)
                {
                    if (target == racer ||
                        racers_[target].finished ||
                        racers_[target].destroyed ||
                        !zLevelContains(target))
                        continue;
                    const Vec3 difference = subtract(
                        vehicles[target].body.position,
                        vehicles[racer].body.position);
                    const float distance = length3(difference);
                    if (distance <= 0.0001F)
                        continue;
                    const float alignment = dot3(
                        carDirection3,
                        multiply(difference, 1.0F / distance));
                    const bool inView =
                        direction > 0
                            ? alignment >= 0.70710678F
                            : alignment <= -0.70710678F;
                    const float planeDistance = std::abs(
                        dot3(carDirection3, difference));
                    if (!inView ||
                        (enemy != RacerRuntime::invalidWeapon &&
                         planeDistance >= minimumPlaneDistance))
                        continue;
                    enemy = target;
                    minimumPlaneDistance = planeDistance;
                }
                if (currentTarget == enemy)
                    return enemy;
                const bool currentValid =
                    currentTarget < racers_.size() &&
                    currentTarget < vehicles.size() &&
                    !racers_[currentTarget].finished &&
                    !racers_[currentTarget].destroyed &&
                    zLevelContains(currentTarget);
                if (enemy == RacerRuntime::invalidWeapon)
                {
                    return currentValid
                               ? currentTarget
                               : RacerRuntime::invalidWeapon;
                }
                if (!currentValid ||
                    length2(subtract(
                        vehicles[currentTarget].body.position,
                        vehicles[enemy].body.position)) >
                        carRadius(currentTarget) * 2.0F)
                    return enemy;
                return currentTarget;
            };
        aiFrontTargets_[racer] =
            findEnemy(1, aiFrontTargets_[racer]);
        aiBackTargets_[racer] =
            findEnemy(-1, aiBackTargets_[racer]);
        const std::size_t frontTarget =
            aiFrontTargets_[racer];
        const std::size_t backTarget =
            aiBackTargets_[racer];
        const float backDistance =
            backTarget < racers_.size()
                ? length2(subtract(
                      vehicles[backTarget].body.position,
                      vehicles[racer].body.position))
                : 0.0F;

        auto shotByEnemy = [&](std::size_t enemy) {
            if (enemy >= racers_.size() ||
                enemy >= vehicles.size())
                return;
            const Vec3 difference = subtract(
                vehicles[enemy].body.position,
                vehicles[racer].body.position);
            const float enemyRadius = carRadius(enemy);
            const float sourceRadius = carRadius(racer);
            const float lineDistance = std::abs(
                carDirection.x * difference.y -
                carDirection.y * difference.x);
            if (lineDistance >= enemyRadius ||
                std::abs(difference.z) >=
                    std::min(sourceRadius, enemyRadius))
                return;
            const float targetDistance = length2(difference);
            const float targetForwardDistance =
                dot2(carDirection, difference);
            {
                std::vector<std::size_t> usableSlots;
                std::size_t chargedWeapons = 0;
                for (std::size_t slot = 0;
                     slot < runtime.weaponSlots.size(); ++slot)
                {
                    const auto weaponIndex =
                        runtime.weaponSlots[slot];
                    if (weaponIndex ==
                            RacerRuntime::invalidWeapon ||
                        weaponIndex >= race_.weapons.size())
                        continue;
                    const auto& weapon =
                        race_.weapons[weaponIndex];
                    if (weaponCooldown_[racer][slot] > 0.0F)
                        return;
                    if (runtime.weaponCharges[slot] > 0U)
                        ++chargedWeapons;
                    const bool inRange =
                        weapon.maximumDistance <= 0.0F ||
                        targetDistance <
                            std::min(
                                weapon.maximumDistance, 100.0F);
                    const bool forwardOrTorpedo =
                        weapon.projectileType == 2U ||
                        targetForwardDistance >
                            sourceRadius * 0.5F;
                    if (runtime.weaponCharges[slot] > 0U &&
                        inRange && forwardOrTorpedo)
                        usableSlots.push_back(slot);
                }
                std::stable_sort(
                    usableSlots.begin(), usableSlots.end(),
                    [&](std::size_t first,
                        std::size_t second) {
                        return race_
                                   .weapons[runtime.weaponSlots[first]]
                                   .maximumDistance <
                               race_
                                   .weapons[runtime.weaponSlots[second]]
                                   .maximumDistance;
                    });
                if (usableSlots.empty())
                    return;
                const auto slot =
                    sourceRandomUnit() < 0.25F
                        ? usableSlots[sourceUniformRandomIndex(
                              usableSlots.size(),
                              sourceUniformRandomUnit())]
                        : usableSlots.front();
                const float summedPart = std::clamp(
                    raceProgress(racer) / 0.7F, 0.0F, 1.0F);
                const float weaponPart =
                    chargedWeapons > 0U
                        ? 1.0F /
                              static_cast<float>(chargedWeapons)
                        : 1.0F;
                const float part =
                    std::min(summedPart / weaponPart, 1.0F);
                const float ammunition = std::max(
                    static_cast<float>(
                        runtime.weaponCharges[slot]) -
                        (1.0F - part) *
                            static_cast<float>(
                                runtime.weaponCapacity[slot]),
                    0.0F);
                if (runtime.weaponCapacity[slot] != 0U &&
                    ammunition <= 0.0F)
                    return;
                runtime.selectedWeaponSlot = slot;
                syncSelectedWeapon(runtime);
                fireWeapon(racer, 0.25F, enemy);
            }
        };
        shotByEnemy(frontTarget);
        shotByEnemy(backTarget);

        if (runtime.hyperCharge > 0 &&
            hyperCooldown_[racer] <= 0.0F &&
            vehicles[racer].speed > 1.0F &&
            runtime.hyperWeapon < race_.weapons.size())
        {
            TraceNodeRef node = currentTraceNodes_[racer];
            const auto& path = tracePathAt(node.path);
            node.node = std::min(node.node, path.size() - 2U);
            const auto& start = tracePoint(node.path, node.node);
            const auto& target = tracePoint(node.path, node.node + 1U);
            const auto currentDirection = normalized2(
                subtract(target.position, start.position));
            float turnAngle = 0.0F;
            if (node.node + 2U < path.size())
            {
                const auto& following =
                    tracePoint(node.path, node.node + 2U);
                const auto nextDirection = normalized2(
                    subtract(following.position, target.position));
                turnAngle = std::acos(std::clamp(
                    dot2(currentDirection, nextDirection),
                    -1.0F, 1.0F));
            }
            const auto tile = projectTraceTile(
                node.path, node.node, vehicles[racer].body.position);
            const float distanceToTurn =
                length2(subtract(target.position, start.position)) *
                (1.0F - tile.coordinate);
            const float hyperDistance =
                race_.weapons[runtime.hyperWeapon].projectileSpeed +
                vehicles[racer].speed;
            const bool safeDistance =
                node.node + 2U >= path.size() ||
                turnAngle < 3.14159265358979323846F / 6.0F ||
                distanceToTurn > hyperDistance;
            const float summedPart = std::clamp(
                raceProgress(racer) / 0.7F, 0.0F, 1.0F);
            const float ammunition = std::max(
                static_cast<float>(runtime.hyperCharge) -
                    (1.0F - summedPart) *
                        static_cast<float>(runtime.hyperCapacity),
                0.0F);
            if (safeDistance && !aiBrake_[racer] &&
                (runtime.hyperCapacity == 0U || ammunition > 0.0F))
                activateHyper(racer);
        }
        // AttackState::Update invokes RunHyper before PlaceMine.  Keep the
        // order because both shots may emit effects and consume slots in a
        // single source progress tick.
        if (runtime.mines > 0 &&
            mineCooldown_[racer] <= 0.0F)
        {
            if (aiMineRandom_[racer] == -1.0F)
            {
                aiMineRandom_[racer] =
                    -0.5F + sourceRandomUnit() * 0.5F;
            }
            float summedPart = std::clamp(
                (raceProgress(racer) - 0.05F) / 0.9F,
                0.0F, 1.0F);
            if (summedPart > 0.0F && summedPart < 1.0F)
            {
                if (backTarget < racers_.size() &&
                    backDistance < 30.0F)
                    summedPart += 0.3F;
                summedPart = std::clamp(
                    summedPart + aiMineRandom_[racer],
                    0.0F, 1.0F);
            }
            std::uint32_t maximumUsedCharge = 3U;
            if (runtime.mineWeapon < race_.weapons.size() &&
                race_.weapons[runtime.mineWeapon].record.find("maslo") !=
                    std::string::npos)
                maximumUsedCharge = 2U;
            const auto capacity =
                std::min(runtime.mineCapacity, maximumUsedCharge);
            const auto spent =
                runtime.mineCapacity > runtime.mines
                    ? runtime.mineCapacity - runtime.mines
                    : 0U;
            const auto current =
                capacity - std::min(spent, capacity);
            const float ammunition = std::max(
                static_cast<float>(current) -
                    (1.0F - summedPart) *
                        static_cast<float>(capacity),
                0.0F);
            if ((capacity == 0U || ammunition > 0.0F) &&
                vehicles[racer].speed > 5.0F)
            {
                placeMine(racer);
                aiMineRandom_[racer] =
                    -0.5F + sourceRandomUnit() * 0.5F;
            }
        }
    }

    const std::size_t collisionRacers =
        std::min(vehicles.size(), racers_.size());
    for (std::size_t first = 0; first < collisionRacers; ++first)
    {
        if (racers_[first].destroyed)
            continue;
        for (const auto& contact : vehicles[first].bodyContacts)
        {
            if (contact.surface !=
                    r3d::physics::CollisionSurface::Vehicle ||
                contact.otherVehicle <= first ||
                contact.otherVehicle >= collisionRacers)
                continue;
            const std::size_t second = contact.otherVehicle;
            if (racers_[second].destroyed)
                continue;
            const std::size_t cooldownIndex =
                first * collisionRacers + second;
            if (cooldownIndex >= touchCooldown_.size() ||
                touchCooldown_[cooldownIndex] > 0.0F)
                continue;
            float forcePart = 0.0F;
            const float damage = damageFromContact(
                race_.touchCarDamage, race_.touchCarDamageForce,
                contact.force, forcePart);
            if (forcePart <= 0.0F || damage <= 0.0F)
                continue;
            touchCooldown_[cooldownIndex] = 0.25F;
            auto kineticEnergy = [&](std::size_t racer) {
                const auto& source = race_.racers[racer];
                const auto& definition =
                    source.hasConfiguredVehicle
                        ? source.configuredVehicle
                        : race_.vehicles.at(source.vehicle);
                return 0.5F * definition.physics.mass *
                       vehicles[racer].speed *
                       vehicles[racer].speed;
            };
            const float firstEnergy = kineticEnergy(first);
            const float secondEnergy = kineticEnergy(second);
            const std::size_t target =
                firstEnergy > secondEnergy ? second : first;
            const std::size_t attacker =
                target == first ? second : first;
            applyTouchDamage(
                target, attacker, damage,
                vehicles[target].body.position);
        }
    }

    for (std::size_t racer = 0;
         racer < vehicles.size() && racer < racers_.size(); ++racer)
    {
        if (racers_[racer].destroyed || vehicles[racer].speed < 8.0F)
            continue;
        const auto& racerDefinition = race_.racers[racer];
        const auto& vehicleDefinition =
            racerDefinition.hasConfiguredVehicle
                ? racerDefinition.configuredVehicle
                : race_.vehicles.at(racerDefinition.vehicle);
        ProjectileCollisionBox vehicleCollision;
        vehicleCollision.center =
            vehicleDefinition.physics.shapePosition;
        vehicleCollision.halfExtents =
            vehicleDefinition.physics.halfExtents;
        damageDecorationWithBox(
            vehicles[racer].body, vehicleCollision,
            vehicles[racer].speed * 2.0F, racer);
    }
}

void OriginalRaceSession::completeAchievement(
    std::size_t achievement)
{
    if (achievement >= race_.achievements.size() ||
        achievement >= achievementIterations_.size())
        return;
    const auto& definition = race_.achievements[achievement];
    if (++achievementIterations_[achievement] <
        definition.iterationCount)
        return;
    achievementIterations_[achievement] = 0U;
    // AchievmentCondition::Complete passes the serialized reward to
    // AchievmentModel::AddPoints.  That method ignores it in skirmish and
    // applies the profile difficulty multiplier in championship.
    if (campaign_)
    {
        achievementPoints_ += static_cast<std::uint32_t>(
            std::floor(static_cast<float>(definition.reward) *
                       achievementMultiplier_));
    }
    events_.push_back(
        {RaceEventKind::Achievement, 0, achievement, {},
         static_cast<float>(definition.reward)});
}

void OriginalRaceSession::updateAchievements(float seconds)
{
    for (std::size_t index = 0;
         index < race_.achievements.size(); ++index)
    {
        if (race_.achievements[index].classId != 2U)
            continue;
        auto& timer = achievementConditionTimers_[index];
        if (timer > 0.0F && (timer -= seconds) <= 0.0F)
        {
            timer = 0.0F;
            achievementConditionCounters_[index] = 0U;
        }
    }

    const std::size_t sourceEventCount = events_.size();
    for (std::size_t eventIndex = 0;
         eventIndex < sourceEventCount; ++eventIndex)
    {
        const RaceEvent event = events_[eventIndex];
        const bool humanKill =
            event.kind == RaceEventKind::Kill &&
            event.killCredit && event.racer == 0U;
        const bool humanDeath =
            event.kind == RaceEventKind::Kill && event.target == 0U;
        const bool humanLap =
            event.kind == RaceEventKind::Lap && event.racer == 0U;
        const bool humanFinish =
            event.kind == RaceEventKind::Finish && event.racer == 0U;

        for (std::size_t index = 0;
             index < race_.achievements.size(); ++index)
        {
            const auto& definition = race_.achievements[index];
            auto& counter = achievementConditionCounters_[index];
            switch (definition.classId)
            {
            case 1U:
                if (event.kind == RaceEventKind::Bonus &&
                    event.racer == 0U &&
                    event.target < race_.bonuses.size() &&
                    race_.bonuses[event.target].kind ==
                        definition.bonusKind)
                {
                    auto& total =
                        achievementConditionTotals_[index];
                    if (total == 0U)
                    {
                        const auto& sourceRecord =
                            race_.bonuses[event.target].record;
                        total = static_cast<std::uint32_t>(
                            std::count_if(
                                race_.bonuses.begin(),
                                race_.bonuses.end(),
                                [&](const BonusInstance& bonus) {
                                    return bonus.record ==
                                           sourceRecord;
                                }));
                    }
                    if (total > 0U && ++counter >= total)
                    {
                        counter = 0U;
                        completeAchievement(index);
                    }
                }
                break;
            case 2U:
                if (humanKill)
                {
                    if (++counter >=
                        std::max(definition.killsNumber, 1U))
                    {
                        counter = 0U;
                        achievementConditionTimers_[index] = 0.0F;
                        completeAchievement(index);
                    }
                    else
                    {
                        achievementConditionTimers_[index] =
                            definition.killsTime;
                    }
                }
                break;
            case 3U:
                if (humanKill &&
                    ++counter >=
                        std::max(definition.killsNumber, 1U))
                {
                    counter = 0U;
                    completeAchievement(index);
                }
                break;
            case 4U:
                if (humanLap && !racers_.empty() &&
                    racers_.front().place == 1U)
                    ++counter;
                if (humanFinish && counter >= race_.lapCount)
                {
                    counter = 0U;
                    completeAchievement(index);
                }
                break;
            case 5U:
                if ((event.kind == RaceEventKind::Damage &&
                     event.target == 0U && event.value > 0.0F) ||
                    humanDeath)
                    ++counter;
                if (humanLap && !racers_.empty())
                {
                    if (counter == 0U &&
                        racers_.front().completedLaps == 1U)
                        completeAchievement(index);
                    counter = 0U;
                }
                break;
            case 6U:
                if (humanLap && !racers_.empty())
                {
                    const auto newPlace = racers_.front().place;
                    if (racers_.front().completedLaps >=
                            race_.lapCount &&
                        static_cast<int>(achievementPreviousLapPlace_) -
                                static_cast<int>(newPlace) >=
                            static_cast<int>(racers_.size()) - 1)
                        completeAchievement(index);
                }
                break;
            case 7U:
                if (humanDeath)
                    ++counter;
                if (humanLap && !racers_.empty() &&
                    racers_.front().completedLaps ==
                        race_.lapCount - 1U &&
                    counter == 0U)
                    completeAchievement(index);
                break;
            case 8U:
                if (humanKill && achievementGlobalKills_ == 0U)
                    completeAchievement(index);
                break;
            case 9U:
                if (event.kind == RaceEventKind::Kill &&
                    event.target != 0U && event.racer == 0U &&
                    (event.damageType == DamageType::Touch ||
                     event.damageType == DamageType::DeathPlane))
                    completeAchievement(index);
                break;
            default:
                break;
            }
        }
        if (event.kind == RaceEventKind::Kill &&
            event.killCredit)
            ++achievementGlobalKills_;
        if (humanLap && !racers_.empty())
            achievementPreviousLapPlace_ = racers_.front().place;
    }
}

void OriginalRaceSession::completeRemainingRacers(
    const std::vector<r3d::physics::VehicleState>& vehicles)
{
    // Race::CompleteRace(const Results*) preserves racers that crossed the
    // line, then sorts everybody else by their last source-path position.
    // CompleteRace(Player*) assigns the next result place and the planet
    // reward in precisely that order.
    std::vector<std::size_t> remaining;
    remaining.reserve(racers_.size());
    for (std::size_t racer = 0U; racer < racers_.size(); ++racer)
    {
        if (!racers_[racer].finished)
            remaining.push_back(racer);
    }
    std::stable_sort(
        remaining.begin(), remaining.end(),
        [&](std::size_t first, std::size_t second) {
            const float firstPlace =
                first < vehicles.size()
                    ? lapPosition(first, vehicles[first])
                    : lastCorrectLapPosition(first);
            const float secondPlace =
                second < vehicles.size()
                    ? lapPosition(second, vehicles[second])
                    : lastCorrectLapPosition(second);
            return firstPlace > secondPlace;
        });
    std::size_t completed = static_cast<std::size_t>(std::count_if(
        racers_.begin(), racers_.end(),
        [](const RacerRuntime& racer) { return racer.finished; }));
    for (const std::size_t racer : remaining)
    {
        auto& runtime = racers_[racer];
        runtime.finished = true;
        runtime.finishTime =
            elapsedSeconds_ + static_cast<float>(completed) * 0.001F;
        runtime.place = static_cast<std::uint32_t>(++completed);
        const std::size_t reward = std::min<std::size_t>(
            runtime.place > 0U ? runtime.place - 1U : 0U,
            race_.rewardMoney.size() - 1U);
        runtime.rewardMoney = race_.rewardMoney[reward];
        runtime.rewardPoints = race_.rewardPoints[reward];
        if (racer < vehicleInputs_.size())
            vehicleInputs_[racer] = {};
    }
}

void OriginalRaceSession::update(
    float seconds,
    const std::vector<r3d::physics::VehicleState>& vehicles,
    const RaceControl& humanControl)
{
    seconds = std::clamp(seconds, 0.0F, 0.1F);
    events_.clear();
    std::fill(vehicleInputs_.begin(), vehicleInputs_.end(),
              r3d::physics::VehicleInput{});
    if (phase_ == RacePhase::Paused)
        return;
    for (auto& effect : effects_)
    {
        effect.seconds -= seconds;
        effect.ageSeconds += seconds;
    }
    effects_.erase(
        std::remove_if(effects_.begin(), effects_.end(),
                       [](const RaceEffect& effect) {
                           return effect.seconds <= 0.0F;
                       }),
        effects_.end());
    if (phase_ == RacePhase::Countdown)
    {
        countdownSeconds_ -= seconds;
        const int display =
            std::max(0, static_cast<int>(std::ceil(countdownSeconds_)));
        if (display != countdownDisplay_)
        {
            countdownDisplay_ = display;
            events_.push_back({RaceEventKind::CountdownChanged, 0, 0, {},
                               static_cast<float>(display)});
        }
        if (countdownSeconds_ <= 0.0F)
        {
            countdownSeconds_ = 0.0F;
            phase_ = RacePhase::Racing;
            events_.push_back(
                {RaceEventKind::CountdownChanged, 0, 0, {}, 0.0F});
        }
        return;
    }

    const bool finishTimerRunning =
        phase_ == RacePhase::Finished && finishSecondsRemaining_ > 0.0F;
    if (phase_ == RacePhase::Finished && !finishTimerRunning)
        return;

    if (finishTimerRunning)
    {
        finishSecondsRemaining_ =
            std::max(0.0F, finishSecondsRemaining_ - seconds);
    }

    elapsedSeconds_ += seconds;
    if (!vehicleInputs_.empty() && !racers_.front().finished)
    {
        vehicleInputs_[0] = humanControl.driving;
        if (racers_[0].speedBoostSeconds > 0.0F)
            vehicleInputs_[0].throttle = 1.0F;
    }
    // Player::CarState::Update resolves curTile/lastNode before AICar and
    // Race consume them.  Doing the same here prevents AI from steering for
    // the previous branch for one frame and keeps HUD/place state coherent.
    for (std::size_t racer = 0U;
         racer < racers_.size() && racer < vehicles.size(); ++racer)
        updateProgress(racer, vehicles[racer], seconds);
    updateAiTracks(vehicles);
    const auto difficultyIndex =
        initialPlayerProfile_.difficulty == "gdEasy"
            ? 0U
            : initialPlayerProfile_.difficulty == "gdHard" ? 2U : 1U;
    static constexpr std::array<float, 3> easingMinimumDistance{
        20.0F, 20.0F, 20.0F};
    static constexpr std::array<float, 3> easingMaximumDistance{
        200.0F, 200.0F, 200.0F};
    static constexpr std::array<float, 3> easingMinimumSpeed{
        95.0F * 1000.0F / 3600.0F,
        110.0F * 1000.0F / 3600.0F,
        125.0F * 1000.0F / 3600.0F};
    static constexpr std::array<float, 3> easingMaximumSpeed{
        55.0F * 1000.0F / 3600.0F,
        65.0F * 1000.0F / 3600.0F,
        75.0F * 1000.0F / 3600.0F};
    static constexpr std::array<float, 3> cheatMinimumTorque{
        1.05F, 1.20F, 1.30F};
    static constexpr std::array<float, 3> cheatMaximumTorque{
        1.30F, 1.65F, 1.85F};
    const float pathLength = [&]() {
        float result = 0.0F;
        for (std::size_t node = 1U;
             node < race_.tracePath.size(); ++node)
        {
            result += length2(subtract(
                tracePoint(node).position,
                tracePoint(node - 1U).position));
        }
        return std::max(result, 1.0F);
    }();
    for (std::size_t racer = 1;
         racer < racers_.size() && racer < vehicles.size(); ++racer)
    {
        vehicleInputs_[racer] =
            aiInput(racer, vehicles[racer], seconds);
    }

    // Player::CheatUpdate runs for both HumanPlayer and every AIPlayer.  It
    // compares each car with the racer having the greatest signed lap lead,
    // not only with the human.  HumanPlayer enables the faster bit; AIPlayer
    // enables both faster and slower bits.
    for (std::size_t racer = 0U;
         racer < racers_.size() && racer < vehicles.size(); ++racer)
    {
        if (racers_[racer].destroyed)
            continue;
        const float racerLap =
            this->lapPosition(racer, vehicles[racer]);
        std::size_t opponent = RacerRuntime::invalidWeapon;
        float maximumLapDistance = 0.0F;
        for (std::size_t candidate = 0U;
             candidate < racers_.size() &&
             candidate < vehicles.size(); ++candidate)
        {
            if (candidate == racer)
                continue;
            const float lapDistance =
                this->lapPosition(candidate, vehicles[candidate]) -
                racerLap;
            if (opponent == RacerRuntime::invalidWeapon ||
                lapDistance > maximumLapDistance)
            {
                opponent = candidate;
                maximumLapDistance = lapDistance;
            }
        }
        if (opponent == RacerRuntime::invalidWeapon)
            continue;
        const float opponentLap =
            this->lapPosition(opponent, vehicles[opponent]);
        float distance = std::abs(racerLap - opponentLap);
        distance -= std::floor(distance);
        const TraceNodeRef trace = racerTraceNode(racer);
        const float racerPathLength =
            trace.valid()
                ? std::max(tracePathLength(trace.path), 1.0F)
                : pathLength;
        distance = std::min(distance, 1.0F - distance) *
                   racerPathLength;
        const float distancePart = std::clamp(
            (distance - easingMinimumDistance[difficultyIndex]) /
                (easingMaximumDistance[difficultyIndex] -
                 easingMinimumDistance[difficultyIndex]),
            0.0F, 1.0F);
        auto& control = vehicleInputs_[racer];
        if (racerLap > opponentLap &&
            distance >
                easingMinimumDistance[difficultyIndex])
        {
            // HumanPlayer does not enable cCheatEnableSlower.
            if (racer != 0U)
            {
                const float speedLimit =
                    easingMinimumSpeed[difficultyIndex] +
                    (easingMaximumSpeed[difficultyIndex] -
                     easingMinimumSpeed[difficultyIndex]) *
                        distancePart;
                if (vehicles[racer].speed > speedLimit &&
                    control.brake <= 0.0F)
                {
                    control.throttle = 0.0F;
                    control.reverse = 0.0F;
                }
            }
        }
        else if (distance >
                 easingMinimumDistance[difficultyIndex])
        {
            const float torqueScale =
                cheatMinimumTorque[difficultyIndex] +
                (cheatMaximumTorque[difficultyIndex] -
                 cheatMinimumTorque[difficultyIndex]) *
                    distancePart;
            control.motorTorqueScale = torqueScale;
            control.lateralGripScale = torqueScale;
        }
    }

    updateGameplay(seconds, vehicles, humanControl);
    updatePlaces(vehicles);
    updateAchievements(seconds);
    if (phase_ == RacePhase::Finished &&
        finishSecondsRemaining_ == 0.0F)
    {
        completeRemainingRacers(vehicles);
        updatePlaces(vehicles);
    }
}

bool runOriginalRaceSessionSmokeTest(const Race& race, std::string& error)
{
    try
    {
        if (sourceUniformRandomIndex(4U, 0.0) != 0U ||
            sourceUniformRandomIndex(4U, 0.249999) != 0U ||
            sourceUniformRandomIndex(4U, 0.25) != 1U ||
            sourceUniformRandomIndex(4U, 0.5) != 2U ||
            sourceUniformRandomIndex(4U, 0.999999) != 3U ||
            sourceRoundedRandomIndex(4U, 0.0F) != 0U ||
            sourceRoundedRandomIndex(4U, 0.16F) != 0U ||
            sourceRoundedRandomIndex(4U, 0.5F) != 2U ||
            sourceRoundedRandomIndex(4U, 1.0F) != 3U ||
            sourceBonusCharge(3U, 0.5F) != 1U ||
            sourceBonusCharge(6U, 0.5F) != 3U ||
            sourceBonusCharge(10U, 0.0F) != 1U)
        {
            throw std::runtime_error(
                "source RandomRange/Player::TakeBonus formula failed");
        }
        OriginalRaceSession session(race);
        auto point = [&](std::size_t pathNode) -> const TracePoint& {
            const std::uint32_t id = race.tracePath.at(pathNode);
            const auto found = std::find_if(
                race.tracePoints.begin(), race.tracePoints.end(),
                [id](const TracePoint& value) { return value.id == id; });
            if (found == race.tracePoints.end())
                throw std::runtime_error("unresolved smoke-test trace point");
            return *found;
        };
        std::vector<r3d::physics::VehicleState> vehicles(race.racers.size());
        for (std::size_t index = 0; index < vehicles.size(); ++index)
        {
            vehicles[index].body.position =
                point(0).position;
            vehicles[index].body.position.z += 2.0F;
            vehicles[index].contactCount = 4;
        }
        RaceControl input;
        input.driving.throttle = 1.0F;
        for (int frame = 0; frame < 190; ++frame)
            session.update(1.0F / 60.0F, vehicles, input);
        if (session.phase() != RacePhase::Racing ||
            session.vehicleInputs().empty() ||
            session.vehicleInputs().front().throttle < 0.9F)
            throw std::runtime_error("countdown/control transition failed");

        {
            OriginalRaceSession traceSession(race);
            auto traceVehicles = vehicles;
            for (int frame = 0; frame < 190; ++frame)
                traceSession.update(
                    1.0F / 60.0F, traceVehicles, input);
            const Vec3 direction = normalized2(subtract(
                point(1U).position, point(0U).position));
            auto placeOnFirstTile = [&](float distance,
                                        bool reverseDirection) {
                const float length = std::max(
                    length2(subtract(
                        point(1U).position, point(0U).position)),
                    0.0001F);
                const float coordinate =
                    std::clamp(distance / length, 0.0F, 1.0F);
                traceVehicles[0].body.position = add(
                    point(0U).position, multiply(direction, distance));
                traceVehicles[0].body.position.z =
                    point(0U).position.z +
                    (point(1U).position.z - point(0U).position.z) *
                        coordinate +
                    2.0F;
                traceVehicles[0].body.rotation = shortestArcFromX(
                    reverseDirection ? multiply(direction, -1.0F)
                                     : direction);
                traceVehicles[0].speed = 10.0F;
                traceSession.update(
                    1.0F / 60.0F, traceVehicles, input);
            };
            placeOnFirstTile(40.0F, true);
            placeOnFirstTile(25.0F, true);
            if (traceSession.racers().front().wrongWay)
            {
                throw std::runtime_error(
                    "source moveInverse fired before 20 metres");
            }
            placeOnFirstTile(15.0F, true);
            if (!traceSession.racers().front().wrongWay)
            {
                throw std::runtime_error(
                    "source moveInverse distance transition failed");
            }
            placeOnFirstTile(16.0F, false);
            if (traceSession.racers().front().wrongWay)
            {
                throw std::runtime_error(
                    "source moveInverse did not clear on forward tile");
            }

            const Vec3 lastValidMapPosition =
                traceSession.mapPosition(0U);
            traceVehicles[0].body.position.x += 1000.0F;
            traceVehicles[0].body.position.y += 1000.0F;
            traceSession.update(
                1.0F / 60.0F, traceVehicles, input);
            if (distanceSquared(traceSession.mapPosition(0U),
                                lastValidMapPosition) > 0.0001F)
            {
                throw std::runtime_error(
                    "source GetMapPos did not retain the last trace position");
            }

            if (race.tracePath.size() > 3U)
            {
                OriginalRaceSession skipSession(race);
                auto skipVehicles = vehicles;
                for (int frame = 0; frame < 190; ++frame)
                    skipSession.update(
                        1.0F / 60.0F, skipVehicles, input);
                const Vec3 skippedStart = point(2U).position;
                const Vec3 skippedEnd = point(3U).position;
                skipVehicles[0].body.position = multiply(
                    add(skippedStart, skippedEnd), 0.5F);
                skipVehicles[0].body.position.z += 2.0F;
                skipSession.update(
                    1.0F / 60.0F, skipVehicles, input);
                if (skipSession.racers().front().completedLaps != 0U)
                {
                    throw std::runtime_error(
                        "first source trace tile incorrectly counted a lap");
                }
            }
        }

        {
            // The first shipped map has a single WayPath and cannot regress
            // branch tracking.  Reproduce a source-style alternate path with
            // shared start/end WayPoints and verify map, wrong-way and lap
            // state across it.
            Race branchRace = race;
            const Vec3 base = point(0U).position;
            std::uint32_t id = 100000U;
            for (const auto& tracePoint : race.tracePoints)
                id = std::max(id, tracePoint.id + 1U);
            branchRace.tracePoints = {
                {id + 0U, add(base, {0.0F, 0.0F, 0.0F}), 30.0F},
                {id + 1U, add(base, {80.0F, 0.0F, 0.0F}), 30.0F},
                {id + 2U, add(base, {160.0F, 0.0F, 0.0F}), 30.0F},
                {id + 3U, add(base, {160.0F, 100.0F, 0.0F}), 30.0F},
                {id + 4U, add(base, {0.0F, 100.0F, 0.0F}), 30.0F},
                {id + 5U, add(base, {100.0F, 50.0F, 0.0F}), 30.0F},
            };
            branchRace.tracePaths = {
                {id + 0U, id + 1U, id + 2U, id + 3U,
                 id + 4U, id + 0U},
                {id + 1U, id + 5U, id + 3U},
            };
            branchRace.tracePath = branchRace.tracePaths.front();
            branchRace.lapCount = std::max(branchRace.lapCount, 2U);
            auto branchVehicles = vehicles;
            auto branchPoint = [&](std::size_t path,
                                   std::size_t node)
                -> const TracePoint& {
                const std::uint32_t wanted =
                    branchRace.tracePaths.at(path).at(node);
                const auto found = std::find_if(
                    branchRace.tracePoints.begin(),
                    branchRace.tracePoints.end(),
                    [wanted](const TracePoint& value) {
                        return value.id == wanted;
                    });
                if (found == branchRace.tracePoints.end())
                    throw std::runtime_error(
                        "unresolved branch smoke-test trace point");
                return *found;
            };
            auto placeOnBranch = [&](OriginalRaceSession& branchSession,
                                     std::size_t racer,
                                     std::size_t path,
                                     std::size_t node,
                                     float coordinate,
                                     bool reverseDirection) {
                const auto& start = branchPoint(path, node);
                const auto& end = branchPoint(path, node + 1U);
                const Vec3 direction = normalized2(
                    subtract(end.position, start.position));
                branchVehicles[racer].body.position = add(
                    start.position,
                    multiply(subtract(end.position, start.position),
                             coordinate));
                branchVehicles[racer].body.position.z += 2.0F;
                branchVehicles[racer].body.rotation = shortestArcFromX(
                    reverseDirection ? multiply(direction, -1.0F)
                                     : direction);
                branchVehicles[racer].speed = 10.0F;
                branchSession.update(
                    1.0F / 60.0F, branchVehicles, input);
            };

            OriginalRaceSession branchSession(branchRace);
            for (int frame = 0; frame < 190; ++frame)
                branchSession.update(
                    1.0F / 60.0F, branchVehicles, input);
            placeOnBranch(branchSession, 0U, 0U, 0U, 0.5F, false);
            placeOnBranch(branchSession, 0U, 1U, 0U, 0.5F, false);
            const Vec3 expectedBranchMap = multiply(
                add(branchPoint(1U, 0U).position,
                    branchPoint(1U, 1U).position),
                0.5F);
            if (distanceSquared(branchSession.mapPosition(0U),
                                expectedBranchMap) > 0.01F)
            {
                throw std::runtime_error(
                    "alternate WayPath map projection failed");
            }
            placeOnBranch(branchSession, 0U, 1U, 1U, 0.5F, false);
            placeOnBranch(branchSession, 0U, 0U, 3U, 0.5F, false);
            placeOnBranch(branchSession, 0U, 0U, 4U, 0.5F, false);
            // WayPath first releases the final tile once the car leaves its
            // half-width, then reacquires the first tile on the next update.
            placeOnBranch(branchSession, 0U, 0U, 0U, 0.25F, false);
            placeOnBranch(branchSession, 0U, 0U, 0U, 0.5F, false);
            if (branchSession.racers().front().completedLaps != 1U)
            {
                throw std::runtime_error(
                    "alternate WayPath lap transition failed");
            }

            OriginalRaceSession branchWrongWay(branchRace);
            for (int frame = 0; frame < 190; ++frame)
                branchWrongWay.update(
                    1.0F / 60.0F, branchVehicles, input);
            placeOnBranch(
                branchWrongWay, 0U, 1U, 0U, 0.9F, true);
            placeOnBranch(
                branchWrongWay, 0U, 1U, 0U, 0.4F, true);
            if (!branchWrongWay.racers().front().wrongWay)
            {
                throw std::runtime_error(
                    "alternate WayPath wrong-way distance failed");
            }
        }

        if (vehicles.size() > 1U)
        {
            if (vehicles.size() > 2U)
            {
                Race laneRace = race;
                laneRace.racers.resize(3U);
                OriginalRaceSession laneSession(laneRace);
                auto laneVehicles = vehicles;
                laneVehicles.resize(3U);
                const Vec3 laneDirection = normalized2(
                    subtract(point(1U).position,
                             point(0U).position));
                const Quat laneRotation =
                    shortestArcFromX(laneDirection);
                for (auto& laneVehicle : laneVehicles)
                {
                    laneVehicle.body.position =
                        point(0U).position;
                    laneVehicle.body.position.z += 2.0F;
                    laneVehicle.body.rotation = laneRotation;
                    laneVehicle.speed = 0.0F;
                }
                for (int frame = 0; frame < 190; ++frame)
                    laneSession.update(
                        1.0F / 60.0F, laneVehicles, input);
                laneSession.update(
                    1.0F / 60.0F, laneVehicles, input);
                if (std::abs(
                        laneSession.vehicleInputs()[1].steering -
                        laneSession.vehicleInputs()[2].steering) <
                    0.05F)
                {
                    throw std::runtime_error(
                        "source AISystem four-track chain assignment failed");
                }
            }

            const auto& cornerVehicleDefinition =
                race.racers[1].hasConfiguredVehicle
                    ? race.racers[1].configuredVehicle
                    : race.vehicles.at(race.racers[1].vehicle);
            const Vec3 cornerHalf =
                cornerVehicleDefinition.physics.halfExtents;
            const float cornerCarSize =
                2.0F * std::sqrt(
                           cornerHalf.x * cornerHalf.x +
                           cornerHalf.y * cornerHalf.y +
                           cornerHalf.z * cornerHalf.z);
            std::size_t cornerNode = 0U;
            bool cornerTurnsLeft = false;
            for (std::size_t node = 1U;
                 node + 1U < race.tracePath.size(); ++node)
            {
                const Vec3 incoming = normalized2(
                    subtract(point(node).position,
                             point(node - 1U).position));
                const Vec3 outgoing = normalized2(
                    subtract(point(node + 1U).position,
                             point(node).position));
                const float angle = std::acos(std::clamp(
                    dot2(incoming, outgoing), -1.0F, 1.0F));
                const float incomingLength = length2(subtract(
                    point(node).position,
                    point(node - 1U).position));
                if (angle >
                        3.14159265358979323846F / 12.0F &&
                    incomingLength > cornerCarSize * 3.0F)
                {
                    cornerNode = node;
                    cornerTurnsLeft =
                        incoming.x * outgoing.y -
                            incoming.y * outgoing.x >
                        0.0F;
                    break;
                }
            }
            if (cornerNode == 0U)
            {
                throw std::runtime_error(
                    "source trace has no usable AICar corner regression");
            }
            {
                OriginalRaceSession cornerSession(race);
                auto cornerVehicles = vehicles;
                for (auto& state : cornerVehicles)
                    state.speed = 5.0F;
                for (int frame = 0; frame < 190; ++frame)
                {
                    cornerSession.update(
                        1.0F / 60.0F, cornerVehicles, input);
                }
                for (std::size_t node = 1U;
                     node < cornerNode; ++node)
                {
                    const Vec3 direction = normalized2(
                        subtract(point(node).position,
                                 point(node - 1U).position));
                    for (auto& state : cornerVehicles)
                    {
                        state.body.position = point(node).position;
                        state.body.position.z += 2.0F;
                        state.body.rotation =
                            shortestArcFromX(direction);
                    }
                    cornerSession.update(
                        1.0F / 60.0F, cornerVehicles, input);
                }
                const Vec3 incoming = normalized2(
                    subtract(point(cornerNode).position,
                             point(cornerNode - 1U).position));
                const float incomingLength = length2(subtract(
                    point(cornerNode).position,
                    point(cornerNode - 1U).position));
                const float approachDistance = std::min(
                    incomingLength * 0.4F,
                    std::max(cornerCarSize * 2.0F, 6.0F));
                for (auto& state : cornerVehicles)
                {
                    state.body.position = subtract(
                        point(cornerNode).position,
                        multiply(incoming, approachDistance));
                    state.body.position.z =
                        point(cornerNode).position.z + 2.0F;
                    state.body.rotation =
                        shortestArcFromX(incoming);
                }
                cornerSession.update(
                    1.0F / 60.0F, cornerVehicles, input);
                const float cornerSteering =
                    cornerSession.vehicleInputs()[1].steering;
                if ((cornerTurnsLeft && cornerSteering <= 0.02F) ||
                    (!cornerTurnsLeft && cornerSteering >= -0.02F))
                {
                    throw std::runtime_error(
                        "source AICar inner-corner track switch failed");
                }
            }

            const auto aiHyperdrive = std::find_if(
                race.weapons.begin(), race.weapons.end(),
                [](const WeaponDefinition& weapon) {
                    return weapon.slot == WeaponSlot::Hyper &&
                           recordName(weapon.record) == "hyperdrive";
                });
            if (aiHyperdrive == race.weapons.end())
            {
                throw std::runtime_error(
                    "source hyperdrive is missing for AICar regressions");
            }
            {
                Race brakeHyperRace = race;
                brakeHyperRace.racers.resize(2U);
                brakeHyperRace.racers[1].loadout.clear();
                brakeHyperRace.racers[1].loadout.push_back(
                    {aiHyperdrive->record, "stHyper", 2U});
                OriginalRaceSession brakeHyperSession(
                    brakeHyperRace);
                auto brakeHyperVehicles = vehicles;
                brakeHyperVehicles.resize(2U);
                for (std::size_t racer = 0U;
                     racer < brakeHyperVehicles.size(); ++racer)
                {
                    brakeHyperVehicles[racer].body.position = {
                        100000.0F + static_cast<float>(racer) * 1000.0F,
                        100000.0F, 1000.0F};
                    brakeHyperVehicles[racer].speed = 0.0F;
                }
                for (int frame = 0; frame < 190; ++frame)
                {
                    brakeHyperSession.update(
                        1.0F / 60.0F,
                        brakeHyperVehicles, input);
                }
                const Vec3 incoming = normalized2(subtract(
                    point(cornerNode).position,
                    point(cornerNode - 1U).position));
                brakeHyperVehicles[1].body.position = subtract(
                    point(cornerNode).position,
                    multiply(incoming, 30.0F));
                brakeHyperVehicles[1].body.position.z =
                    point(cornerNode).position.z + 2.0F;
                brakeHyperVehicles[1].body.rotation =
                    shortestArcFromX(incoming);
                brakeHyperVehicles[1].speed = 10.0F;
                brakeHyperSession.update(
                    1.0F / 60.0F,
                    brakeHyperVehicles, input);
                if (brakeHyperSession.vehicleInputs()[1].brake < 0.9F ||
                    brakeHyperSession.racers()[1].hyperCharge != 2U)
                {
                    throw std::runtime_error(
                        "source AICar corner brake/Hyper exclusion failed");
                }
            }
            {
                Race offTraceAttackRace = race;
                offTraceAttackRace.racers.resize(2U);
                offTraceAttackRace.racers[1].loadout.clear();
                offTraceAttackRace.racers[1].loadout.push_back(
                    {aiHyperdrive->record, "stHyper", 2U});
                OriginalRaceSession offTraceAttackSession(
                    offTraceAttackRace);
                auto offTraceAttackVehicles = vehicles;
                offTraceAttackVehicles.resize(2U);
                for (std::size_t racer = 0U;
                     racer < offTraceAttackVehicles.size(); ++racer)
                {
                    offTraceAttackVehicles[racer].body.position = {
                        100000.0F + static_cast<float>(racer) * 1000.0F,
                        100000.0F, 1000.0F};
                    offTraceAttackVehicles[racer].speed = 0.0F;
                }
                for (int frame = 0; frame < 190; ++frame)
                {
                    offTraceAttackSession.update(
                        1.0F / 60.0F,
                        offTraceAttackVehicles, input);
                }
                const Vec3 firstDirection = normalized2(subtract(
                    point(1U).position, point(0U).position));
                offTraceAttackVehicles[1].body.position = multiply(
                    add(point(0U).position, point(1U).position),
                    0.5F);
                offTraceAttackVehicles[1].body.position.z =
                    (point(0U).position.z + point(1U).position.z) *
                        0.5F +
                    2.0F;
                offTraceAttackVehicles[1].body.rotation =
                    shortestArcFromX(firstDirection);
                offTraceAttackSession.update(
                    1.0F / 60.0F,
                    offTraceAttackVehicles, input);
                offTraceAttackSession.takeVelocityRequests();
                offTraceAttackVehicles[1].body.position.z += 1000.0F;
                offTraceAttackVehicles[1].speed = 2.0F;
                offTraceAttackSession.update(
                    1.0F / 60.0F,
                    offTraceAttackVehicles, input);
                if (offTraceAttackSession.racers()[1].hyperCharge != 2U ||
                    !offTraceAttackSession.takeVelocityRequests().empty())
                {
                    throw std::runtime_error(
                        "source AICar curTile-null attack suppression failed");
                }
            }

            const auto sourceTorpedo = std::find_if(
                race.weapons.begin(), race.weapons.end(),
                [](const WeaponDefinition& weapon) {
                    return weapon.slot == WeaponSlot::Primary &&
                           weapon.projectileType == 2U;
                });
            if (sourceTorpedo == race.weapons.end())
            {
                throw std::runtime_error(
                    "source ptTorpeda is missing for AI back-target regression");
            }
            {
                Race backTargetRace = race;
                backTargetRace.racers.resize(2U);
                backTargetRace.racers[1].loadout.clear();
                backTargetRace.racers[1].loadout.push_back(
                    {sourceTorpedo->record, "stWeapon1", 10U});
                OriginalRaceSession backTargetSession(
                    backTargetRace);
                auto backTargetVehicles = vehicles;
                backTargetVehicles.resize(2U);
                const Vec3 direction = normalized2(
                    subtract(point(1U).position,
                             point(0U).position));
                const Quat rotation =
                    shortestArcFromX(direction);
                for (auto& state : backTargetVehicles)
                {
                    state.body.position = point(0U).position;
                    state.body.position.z += 2.0F;
                    state.body.rotation = rotation;
                    state.speed = 0.0F;
                }
                for (int frame = 0; frame < 190; ++frame)
                    backTargetSession.update(
                        1.0F / 60.0F,
                        backTargetVehicles, input);
                backTargetVehicles[1].body.position = multiply(
                    add(point(0U).position, point(1U).position),
                    0.5F);
                backTargetVehicles[1].body.position.z =
                    (point(0U).position.z + point(1U).position.z) *
                        0.5F +
                    2.0F;
                backTargetVehicles[0].body.position =
                    subtract(
                        backTargetVehicles[1].body.position,
                        multiply(direction, 10.0F));
                backTargetSession.update(
                    1.0F / 60.0F,
                    backTargetVehicles, input);
                const auto firedBack = std::any_of(
                    backTargetSession.events().begin(),
                    backTargetSession.events().end(),
                    [&](const RaceEvent& event) {
                        return event.kind ==
                                   RaceEventKind::WeaponFired &&
                               event.racer == 1U &&
                               event.weapon ==
                                   static_cast<std::size_t>(
                                       sourceTorpedo -
                                       race.weapons.begin());
                    });
                if (!firedBack)
                {
                    throw std::runtime_error(
                        "source AICar ptTorpeda back-target shot failed");
                }
            }

            if (vehicles.size() > 2U)
            {
                Race pitchedTargetRace = race;
                pitchedTargetRace.racers.resize(3U);
                pitchedTargetRace.racers[1].loadout.clear();
                pitchedTargetRace.racers[1].loadout.push_back(
                    {sourceTorpedo->record, "stWeapon1", 10U});
                OriginalRaceSession pitchedTargetSession(
                    pitchedTargetRace);
                auto pitchedTargetVehicles = vehicles;
                pitchedTargetVehicles.resize(3U);
                for (std::size_t racer = 0;
                     racer < pitchedTargetVehicles.size(); ++racer)
                {
                    pitchedTargetVehicles[racer].body.position = {
                        100000.0F + static_cast<float>(racer) * 1000.0F,
                        100000.0F, 1000.0F};
                    pitchedTargetVehicles[racer].speed = 5.0F;
                }
                for (int frame = 0; frame < 190; ++frame)
                {
                    pitchedTargetSession.update(
                        1.0F / 60.0F,
                        pitchedTargetVehicles, input);
                }
                const Vec3 direction = normalized2(
                    subtract(point(1U).position,
                             point(0U).position));
                Vec3 base = multiply(
                    add(point(0U).position, point(1U).position),
                    0.5F);
                base.z += 2.0F;
                pitchedTargetVehicles[1].body.position = base;
                pitchedTargetVehicles[1].body.rotation =
                    shortestArcFromX(normalized3(add(
                        multiply(direction, 0.70710678F),
                        {0.0F, 0.0F, 0.70710678F})));
                // Candidate zero is nearer in the old XY-only plane but is
                // outside the source 3D forward cone of the pitched car.
                pitchedTargetVehicles[0].body.position = add(
                    add(base, direction), {0.0F, 0.0F, -0.5F});
                // Candidate two is farther in XY but inside the true 3D
                // cone and within ShotByEnemy's vertical car-radius test.
                pitchedTargetVehicles[2].body.position = add(
                    add(base, multiply(direction, 2.0F)),
                    {0.0F, 0.0F, 0.7F});
                pitchedTargetSession.update(
                    1.0F / 60.0F,
                    pitchedTargetVehicles, input);
                const auto pitchedShot = std::find_if(
                    pitchedTargetSession.projectiles().begin(),
                    pitchedTargetSession.projectiles().end(),
                    [](const ProjectileRuntime& projectile) {
                        return projectile.owner == 1U &&
                               projectile.projectile == 0U &&
                               projectile.target == 2U;
                    });
                if (pitchedShot ==
                    pitchedTargetSession.projectiles().end())
                {
                    throw std::runtime_error(
                        "source AICar 3D target-plane selection failed");
                }
            }

            OriginalRaceSession aiControlSession(race);
            for (int frame = 0; frame < 190; ++frame)
                aiControlSession.update(
                    1.0F / 60.0F, vehicles, input);
            for (int frame = 0; frame < 61; ++frame)
                aiControlSession.update(
                    1.0F / 60.0F, vehicles, input);
            if (aiControlSession.vehicleInputs()[1].reverse < 0.9F ||
                aiControlSession.vehicleInputs()[1].throttle > 0.1F)
            {
                throw std::runtime_error(
                    "source AICar blocked reverse transition failed");
            }
            for (int frame = 0; frame < 61; ++frame)
                aiControlSession.update(
                    1.0F / 60.0F, vehicles, input);
            if (aiControlSession.vehicleInputs()[1].throttle < 0.9F ||
                aiControlSession.vehicleInputs()[1].reverse > 0.1F)
            {
                throw std::runtime_error(
                    "source AICar reverse/forward alternation failed");
            }

            OriginalRaceSession aiOffTraceSession(race);
            auto offTraceVehicles = vehicles;
            for (int frame = 0; frame < 190; ++frame)
            {
                aiOffTraceSession.update(
                    1.0F / 60.0F, offTraceVehicles, input);
            }
            offTraceVehicles[1].body.position = {
                100000.0F, 100000.0F, 1000.0F};
            offTraceVehicles[1].speed = 10.0F;
            offTraceVehicles[1].linearVelocity =
                {10.0F, 0.0F, 0.0F};
            bool offTraceReset = false;
            for (int frame = 0; frame < 240 && !offTraceReset; ++frame)
            {
                aiOffTraceSession.update(
                    1.0F / 60.0F, offTraceVehicles, input);
                offTraceReset = std::any_of(
                    aiOffTraceSession.events().begin(),
                    aiOffTraceSession.events().end(),
                    [](const RaceEvent& event) {
                        return event.kind == RaceEventKind::Respawn &&
                               event.racer == 1U;
                    });
            }
            if (!offTraceReset)
            {
                throw std::runtime_error(
                    "source AICar moving off-trace reset transition failed");
            }

            OriginalRaceSession aiCheatSession(race);
            auto cheatVehicles = vehicles;
            for (int frame = 0; frame < 190; ++frame)
                aiCheatSession.update(
                    1.0F / 60.0F, cheatVehicles, input);
            cheatVehicles[0].body.position = point(1U).position;
            aiCheatSession.update(
                1.0F / 60.0F, cheatVehicles, input);
            aiCheatSession.update(
                1.0F / 60.0F, cheatVehicles, input);
            if (aiCheatSession.vehicleInputs()[1]
                        .motorTorqueScale <= 1.0F ||
                aiCheatSession.vehicleInputs()[1]
                        .lateralGripScale <= 1.0F)
            {
                throw std::runtime_error(
                    "source Player::CheatUpdate AI catch-up failed");
            }

            if (vehicles.size() > 2U)
            {
                Race fieldCheatRace = race;
                fieldCheatRace.racers.resize(3U);
                OriginalRaceSession fieldCheatSession(
                    fieldCheatRace);
                auto fieldCheatVehicles = vehicles;
                fieldCheatVehicles.resize(3U);
                for (std::size_t racer = 0U;
                     racer < fieldCheatVehicles.size(); ++racer)
                {
                    fieldCheatVehicles[racer].body.position = {
                        100000.0F + static_cast<float>(racer) * 1000.0F,
                        100000.0F, 1000.0F};
                    fieldCheatVehicles[racer].speed = 5.0F;
                }
                for (int frame = 0; frame < 190; ++frame)
                {
                    fieldCheatSession.update(
                        1.0F / 60.0F,
                        fieldCheatVehicles, input);
                }
                auto placeAt = [&](std::size_t racer,
                                   std::size_t segment,
                                   float coordinate) {
                    const Vec3 direction = normalized2(subtract(
                        point(segment + 1U).position,
                        point(segment).position));
                    fieldCheatVehicles[racer].body.position = add(
                        point(segment).position,
                        multiply(
                            subtract(point(segment + 1U).position,
                                     point(segment).position),
                            coordinate));
                    fieldCheatVehicles[racer].body.position.z += 2.0F;
                    fieldCheatVehicles[racer].body.rotation =
                        shortestArcFromX(direction);
                };
                placeAt(0U, 0U, 0.1F);
                placeAt(1U, 1U, 0.2F);
                placeAt(2U, 2U, 0.2F);
                fieldCheatSession.update(
                    1.0F / 60.0F,
                    fieldCheatVehicles, input);
                if (fieldCheatSession.vehicleInputs()[0]
                            .motorTorqueScale <= 1.0F ||
                    fieldCheatSession.vehicleInputs()[1]
                            .motorTorqueScale <= 1.0F)
                {
                    throw std::runtime_error(
                        "source Player::CheatUpdate full-field opponent "
                        "selection failed");
                }
            }
        }

        const auto sourceDestruction = std::find_if(
            race.decorationInstances.begin(),
            race.decorationInstances.end(),
            [&](const ObjectInstance& instance) {
                return instance.definition <
                           race.decorationDefinitions.size() &&
                       recordName(race.decorationDefinitions[
                                      instance.definition]
                                      .record) == "crush1";
            });
        if (sourceDestruction == race.decorationInstances.end())
        {
            throw std::runtime_error(
                "source crush1 instance is missing from map1");
        }
        {
            OriginalRaceSession destructionSession(race);
            RaceControl destructionInput;
            for (int frame = 0; frame < 190; ++frame)
                destructionSession.update(
                    1.0F / 60.0F, vehicles, destructionInput);
            const std::size_t instance = static_cast<std::size_t>(
                sourceDestruction - race.decorationInstances.begin());
            Vec3 sourceContact = sourceDestruction->transform.position;
            for (std::size_t meshIndex = 0;
                 meshIndex < race.collisionMeshes.size() &&
                 meshIndex <
                     race.collisionMeshDecorationInstances.size();
                 ++meshIndex)
            {
                if (race.collisionMeshDecorationInstances[meshIndex] !=
                    instance)
                    continue;
                const auto& mesh = race.collisionMeshes[meshIndex];
                if (mesh.indices.size() < 3U ||
                    mesh.indices[0] >= mesh.vertices.size() ||
                    mesh.indices[1] >= mesh.vertices.size() ||
                    mesh.indices[2] >= mesh.vertices.size())
                    continue;
                sourceContact = multiply(
                    add(
                        add(
                            transformPoint(
                                mesh.transform,
                                mesh.vertices[mesh.indices[0]]),
                            transformPoint(
                                mesh.transform,
                                mesh.vertices[mesh.indices[1]])),
                        transformPoint(
                            mesh.transform,
                            mesh.vertices[mesh.indices[2]])),
                    1.0F / 3.0F);
                break;
            }
            const auto& playerDefinition =
                race.racers.front().hasConfiguredVehicle
                    ? race.racers.front().configuredVehicle
                    : race.vehicles.at(
                          race.racers.front().vehicle);
            vehicles[0].body.position = subtract(
                sourceContact,
                playerDefinition.physics.shapePosition);
            vehicles[0].speed = 10.0F;
            destructionSession.update(
                1.0F / 60.0F, vehicles, destructionInput);
            const bool hasSourceEvent = std::any_of(
                destructionSession.events().begin(),
                destructionSession.events().end(),
                [instance](const RaceEvent& event) {
                    return event.kind ==
                               RaceEventKind::DecorationDestroyed &&
                           event.target == instance;
                });
            const bool hasInventedFallback = std::any_of(
                destructionSession.effects().begin(),
                destructionSession.effects().end(),
                [](const RaceEffect& effect) {
                    return effect.kind ==
                           RaceEventKind::DecorationDestroyed;
                });
            if (destructionSession.decorationActive()[instance] ||
                !hasSourceEvent || hasInventedFallback)
            {
                const auto ownedMeshes = std::count(
                    race.collisionMeshDecorationInstances.begin(),
                    race.collisionMeshDecorationInstances.end(),
                    instance);
                const auto& sourceDefinition =
                    race.decorationDefinitions.at(
                        sourceDestruction->definition);
                throw std::runtime_error(
                    "source gotDestrObj separation transition failed: "
                    "active=" +
                    std::string(
                        destructionSession.decorationActive()[instance]
                            ? "true"
                            : "false") +
                    ", event=" +
                    std::string(hasSourceEvent ? "true" : "false") +
                    ", inventedFallback=" +
                    std::string(
                        hasInventedFallback ? "true" : "false") +
                    ", sourceShapes=" +
                    std::to_string(
                        sourceDefinition.collisionShapes.size()) +
                    ", ownedMeshes=" +
                    std::to_string(ownedMeshes));
            }
            vehicles[0].speed = 0.0F;
        }

        const auto mapMine = std::find_if(
            race.bonuses.begin(), race.bonuses.end(),
            [](const BonusInstance& bonus) {
                return bonus.kind == BonusKind::MineHazard;
            });
        if (mapMine != race.bonuses.end())
        {
            const std::size_t mineIndex = static_cast<std::size_t>(
                mapMine - race.bonuses.begin());
            if (mapMine->projectileType != 11U ||
                std::abs(mapMine->value - 11.0F) > 0.001F ||
                std::abs(mapMine->speed - 3000.0F) > 0.001F ||
                (mapMine->deathEffect.visual.visualNodes.empty() &&
                 mapMine->deathEffect.visual.particleEmitters.empty()))
            {
                throw std::runtime_error(
                    "source map mine projectile data was not preserved");
            }
            OriginalRaceSession hazardSession(race);
            RaceControl hazardInput;
            for (int frame = 0; frame < 190; ++frame)
                hazardSession.update(
                    1.0F / 60.0F, vehicles, hazardInput);
            vehicles[0].body.position = mapMine->transform.position;
            vehicles[0].body.position.z += 1.0F;
            const float lifeBeforeMine =
                hazardSession.racers().front().life;
            hazardSession.update(
                1.0F / 60.0F, vehicles, hazardInput);
            const auto mineVelocity =
                hazardSession.takeVelocityRequests();
            const bool hasDamageEvent = std::any_of(
                hazardSession.events().begin(),
                hazardSession.events().end(),
                [](const RaceEvent& event) {
                    return event.kind == RaceEventKind::Damage &&
                           event.racer == 0U;
                });
            const bool hasFalsePickup = std::any_of(
                hazardSession.events().begin(),
                hazardSession.events().end(),
                [](const RaceEvent& event) {
                    return event.kind == RaceEventKind::Bonus;
                });
            const bool hasSourceDeathEffect = std::any_of(
                hazardSession.effects().begin(),
                hazardSession.effects().end(),
                [mineIndex](const RaceEffect& effect) {
                    return effect.kind == RaceEventKind::ProjectileImpact &&
                           effect.bonus == mineIndex;
                });
            if (hazardSession.bonusActive()[mineIndex] ||
                hazardSession.racers().front().life >= lifeBeforeMine ||
                mineVelocity.empty() ||
                mineVelocity.front().delta.z <= 0.0F ||
                !hasDamageEvent || hasFalsePickup ||
                !hasSourceDeathEffect)
            {
                throw std::runtime_error(
                    "source map mine hazard contact transition failed");
            }
        }

        const auto sourcePickup = std::find_if(
            race.bonuses.begin(), race.bonuses.end(),
            [](const BonusInstance& bonus) {
                return bonus.kind != BonusKind::Speed &&
                       bonus.kind != BonusKind::SlowHazard &&
                       bonus.kind != BonusKind::OilHazard &&
                       bonus.kind != BonusKind::MineHazard &&
                       bonus.kind != BonusKind::Unknown &&
                       !bonus.deathEffect.visual.soundPaths.empty();
            });
        if (sourcePickup == race.bonuses.end())
        {
            throw std::runtime_error(
                "source pickup DeathEffect sound is missing");
        }
        {
            Race pickupRace = race;
            pickupRace.bonuses.assign(1U, *sourcePickup);
            pickupRace.bonuses.front().transform.position =
                vehicles.front().body.position;
            pickupRace.bonuses.front().transform.position.z +=
                100.0F;
            OriginalRaceSession pickupSession(pickupRace);
            auto pickupVehicles = vehicles;
            RaceControl pickupInput;
            pickupVehicles[0].speed = 0.0F;
            pickupVehicles[0].linearVelocity = {};
            pickupVehicles[0].bodyContacts.clear();
            for (int frame = 0; frame < 190; ++frame)
                pickupSession.update(
                    1.0F / 60.0F, pickupVehicles, pickupInput);
            pickupVehicles[0].body.position =
                pickupRace.bonuses.front().transform.position;
            pickupSession.update(
                1.0F / 60.0F, pickupVehicles, pickupInput);
            const bool hasPickupEvent = std::any_of(
                pickupSession.events().begin(),
                pickupSession.events().end(),
                [](const RaceEvent& event) {
                    return event.kind == RaceEventKind::Bonus &&
                           event.target == 0U;
                });
            const bool hasSourceDeathEffect = std::any_of(
                pickupSession.effects().begin(),
                pickupSession.effects().end(),
                [](const RaceEffect& effect) {
                    return effect.kind ==
                               RaceEventKind::ProjectileImpact &&
                           effect.bonus == 0U;
                });
            const bool hasSourceDeathSound = std::any_of(
                pickupSession.events().begin(),
                pickupSession.events().end(),
                [&](const RaceEvent& event) {
                    return event.kind == RaceEventKind::EffectSound &&
                           std::find(
                               sourcePickup->deathEffect.visual.soundPaths.begin(),
                               sourcePickup->deathEffect.visual.soundPaths.end(),
                               event.soundPath) !=
                               sourcePickup->deathEffect.visual.soundPaths.end();
                });
            if (pickupSession.bonusActive().front() ||
                !hasPickupEvent || !hasSourceDeathEffect ||
                !hasSourceDeathSound)
            {
                throw std::runtime_error(
                    "source Player::TakeBonus DeathEffect sound/transition failed");
            }
        }

        const auto sourceMoney = std::find_if(
            race.bonuses.begin(), race.bonuses.end(),
            [](const BonusInstance& bonus) {
                return bonus.kind == BonusKind::Money;
            });
        if (sourceMoney == race.bonuses.end())
        {
            throw std::runtime_error(
                "source money bonus is missing for achievement regression");
        }
        {
            Race achievementRace = race;
            achievementRace.bonuses.assign(1U, *sourceMoney);
            achievementRace.bonuses.front().transform.position =
                vehicles.front().body.position;
            achievementRace.bonuses.front().transform.position.z +=
                100.0F;
            AchievementDefinition condition;
            condition.name = "sourceRewardRegression";
            condition.classId = 1U;
            condition.reward = 7U;
            condition.iterationCount = 1U;
            condition.bonusKind = BonusKind::Money;
            achievementRace.achievements.assign(1U, condition);

            ProfileState inputProfile;
            inputProfile.achievementPoints = 100U;
            inputProfile.player.difficulty = "gdHard";
            OriginalRaceSession achievementSession(achievementRace);
            achievementSession.setCampaign(true);
            achievementSession.applyAchievementProfile(inputProfile);
            auto achievementVehicles = vehicles;
            RaceControl achievementInput;
            achievementVehicles[0].speed = 0.0F;
            achievementVehicles[0].linearVelocity = {};
            achievementVehicles[0].bodyContacts.clear();
            for (int frame = 0; frame < 190; ++frame)
                achievementSession.update(
                    1.0F / 60.0F, achievementVehicles,
                    achievementInput);
            achievementVehicles[0].body.position =
                achievementRace.bonuses.front().transform.position;
            achievementSession.update(
                1.0F / 60.0F, achievementVehicles,
                achievementInput);
            ProfileState outputProfile;
            achievementSession.writeAchievementProfile(outputProfile);
            const bool completed = std::any_of(
                achievementSession.events().begin(),
                achievementSession.events().end(),
                [](const RaceEvent& event) {
                    return event.kind ==
                               RaceEventKind::Achievement &&
                           event.target == 0U &&
                           event.value == 7.0F;
                });
            if (!completed ||
                outputProfile.achievementPoints != 110U)
            {
                throw std::runtime_error(
                    "source campaign achievement multiplier failed");
            }

            OriginalRaceSession skirmishSession(achievementRace);
            skirmishSession.setCampaign(false);
            skirmishSession.applyAchievementProfile(inputProfile);
            achievementVehicles = vehicles;
            achievementVehicles[0].speed = 0.0F;
            achievementVehicles[0].linearVelocity = {};
            achievementVehicles[0].bodyContacts.clear();
            for (int frame = 0; frame < 190; ++frame)
                skirmishSession.update(
                    1.0F / 60.0F, achievementVehicles,
                    achievementInput);
            achievementVehicles[0].body.position =
                achievementRace.bonuses.front().transform.position;
            skirmishSession.update(
                1.0F / 60.0F, achievementVehicles,
                achievementInput);
            ProfileState skirmishProfile;
            skirmishSession.writeAchievementProfile(skirmishProfile);
            const bool skirmishCompleted = std::any_of(
                skirmishSession.events().begin(),
                skirmishSession.events().end(),
                [](const RaceEvent& event) {
                    return event.kind ==
                               RaceEventKind::Achievement &&
                           event.target == 0U &&
                           event.value == 7.0F;
                });
            if (!skirmishCompleted ||
                skirmishProfile.achievementPoints != 100U)
            {
                throw std::runtime_error(
                    "source skirmish achievement points suppression failed");
            }
        }

        const float lifeBeforeBorder = session.racers().front().life;
        vehicles[0].speed = 25.0F;
        vehicles[0].linearVelocity = {-25.0F, 5.0F, 0.0F};
        vehicles[0].bodyContacts = {
            {r3d::physics::CollisionSurface::TrackBorder,
             std::numeric_limits<std::size_t>::max(),
             {1.0F, 0.0F, 0.0F}, 25.0F, 4000000.0F}};
        session.update(1.0F / 60.0F, vehicles, input);
        if (session.takeVelocityRequests().empty() ||
            session.racers().front().life >= lifeBeforeBorder)
            throw std::runtime_error(
                "source spring-border contact transition failed");

        session.setSpringBorders(false);
        session.update(1.0F / 60.0F, vehicles, input);
        if (!session.takeVelocityRequests().empty())
            throw std::runtime_error(
                "disabled spring-border still changed velocity");
        session.setSpringBorders(true);
        vehicles[0].bodyContacts.clear();

        {
            OriginalRaceSession contactSession(race);
            auto contactVehicles = vehicles;
            RaceControl contactInput;
            for (auto& vehicle : contactVehicles)
                vehicle.bodyContacts.clear();
            for (int frame = 0; frame < 190; ++frame)
            {
                contactSession.update(
                    1.0F / 60.0F, contactVehicles,
                    contactInput);
            }
            r3d::physics::BodyContact contact;
            contact.surface =
                r3d::physics::CollisionSurface::TrackBorder;
            contact.normal = {1.0F, 0.0F, 0.0F};
            contact.normalSpeed = 20.0F;
            contact.force = 1200000.0F;
            contact.otherActor = 77U;
            contact.frictionForce = 12001.0F;
            contact.point = add(
                contactVehicles[0].body.position,
                {0.5F, 0.0F, 0.5F});
            contact.points = {
                contact.point,
                add(contact.point, {0.0F, 0.25F, 0.0F})};
            contact.hasPoint = true;
            contactVehicles[0].bodyContacts = {contact};
            contactSession.update(
                1.0F / 60.0F, contactVehicles,
                contactInput);
            const auto contactEffectCount =
                static_cast<std::size_t>(std::count_if(
                    contactSession.effects().begin(),
                    contactSession.effects().end(),
                    [](const RaceEffect& effect) {
                        return effect.kind ==
                               RaceEventKind::ContactImpact;
                    }));
            const bool hasContactSound = std::any_of(
                contactSession.events().begin(),
                contactSession.events().end(),
                [&](const RaceEvent& event) {
                    return event.kind == RaceEventKind::EffectSound &&
                           std::find(
                               race.contactSoundPaths.begin(),
                               race.contactSoundPaths.end(),
                               event.soundPath) !=
                               race.contactSoundPaths.end();
                });
            const bool sourceContactMetadata = std::all_of(
                contactSession.effects().begin(),
                contactSession.effects().end(),
                [](const RaceEffect& effect) {
                    return effect.kind != RaceEventKind::ContactImpact ||
                           (effect.contactActor == 77U &&
                            std::abs(
                                effect.emissionEndSeconds - 0.1F) <
                                0.0001F &&
                            std::abs(effect.totalSeconds - 0.8F) <
                                0.0001F);
                });
            if (contactEffectCount != 2U ||
                !hasContactSound || !sourceContactMetadata)
            {
                throw std::runtime_error(
                    "source PairPxContactEffect create/sound failed");
            }
            contactVehicles[0].bodyContacts.clear();
            for (int step = 0; step < 6; ++step)
                contactSession.update(
                    0.1F, contactVehicles, contactInput);
            const bool particlesStillAlive = std::any_of(
                contactSession.effects().begin(),
                contactSession.effects().end(),
                [](const RaceEffect& effect) {
                    return effect.kind == RaceEventKind::ContactImpact;
                });
            for (int step = 0; step < 3; ++step)
                contactSession.update(
                    0.1F, contactVehicles, contactInput);
            const bool released = std::none_of(
                contactSession.effects().begin(),
                contactSession.effects().end(),
                [](const RaceEffect& effect) {
                    return effect.kind == RaceEventKind::ContactImpact;
                });
            if (!particlesStillAlive || !released)
            {
                throw std::runtime_error(
                    "source PairPxContactEffect release/waiting-end failed");
            }
        }

        {
            OriginalRaceSession lowLifeSession(race);
            auto lowLifeVehicles = vehicles;
            RaceControl lowLifeInput;
            for (int frame = 0; frame < 190; ++frame)
                lowLifeSession.update(
                    1.0F / 60.0F, lowLifeVehicles, lowLifeInput);
            const auto& sourceRacer = race.racers.front();
            const auto& sourceVehicle =
                sourceRacer.hasConfiguredVehicle
                    ? sourceRacer.configuredVehicle
                    : race.vehicles.at(sourceRacer.vehicle);
            lowLifeVehicles[0].speed = 25.0F;
            lowLifeVehicles[0].bodyContacts = {
                {r3d::physics::CollisionSurface::TrackBorder,
                 std::numeric_limits<std::size_t>::max(),
                 {1.0F, 0.0F, 0.0F}, 25.0F, 4000000.0F}};
            for (int frame = 0;
                 frame < 100 &&
                 lowLifeSession.racers().front().life /
                         lowLifeSession.racers().front().maximumLife >=
                     sourceVehicle.lowLifeLevel;
                 ++frame)
            {
                lowLifeSession.update(
                    1.0F / 60.0F, lowLifeVehicles, lowLifeInput);
            }
            lowLifeVehicles[0].bodyContacts.clear();
            lowLifeVehicles[0].speed = 0.0F;
            lowLifeSession.update(
                1.0F / 60.0F, lowLifeVehicles, lowLifeInput);
            const bool hasLowLifeEvent = std::any_of(
                lowLifeSession.events().begin(),
                lowLifeSession.events().end(),
                [](const RaceEvent& event) {
                    return event.kind == RaceEventKind::LowLife &&
                           event.racer == 0U;
                });
            if (!lowLifeSession.racers().front().lowLife ||
                lowLifeSession.racers().front().destroyed ||
                lowLifeSession.racers().front().life <= 0.0F ||
                lowLifeSession.racers().front().lowLifeEffectSeconds <=
                    0.0F ||
                !hasLowLifeEvent)
            {
                throw std::runtime_error(
                    "source LowLifePoints/smoke6 transition failed");
            }
            lowLifeSession.update(
                1.0F / 60.0F, lowLifeVehicles, lowLifeInput);
            if (std::any_of(
                    lowLifeSession.events().begin(),
                    lowLifeSession.events().end(),
                    [](const RaceEvent& event) {
                        return event.kind == RaceEventKind::LowLife;
                    }))
            {
                throw std::runtime_error(
                    "source LowLifePoints event repeated while active");
            }
        }

        {
            Race shieldRace = race;
            BonusInstance shieldBonus;
            shieldBonus.record =
                "world\\db\\root\\ctBonuses\\shield";
            shieldBonus.kind = BonusKind::Shield;
            shieldBonus.value = 10.0F;
            shieldBonus.size = {1.0F, 1.0F, 1.0F};
            shieldBonus.collision.halfExtents =
                {0.5F, 0.5F, 0.5F};
            shieldBonus.transform.position =
                vehicles.front().body.position;
            shieldBonus.transform.position.z += 100.0F;
            shieldRace.bonuses.push_back(std::move(shieldBonus));

            OriginalRaceSession shieldSession(shieldRace);
            auto shieldVehicles = vehicles;
            RaceControl shieldInput;
            shieldVehicles[0].speed = 0.0F;
            shieldVehicles[0].linearVelocity = {};
            shieldVehicles[0].bodyContacts.clear();
            for (int frame = 0; frame < 190; ++frame)
                shieldSession.update(
                    1.0F / 60.0F, shieldVehicles, shieldInput);
            shieldVehicles[0].body.position =
                shieldRace.bonuses.back().transform.position;
            shieldVehicles[0].body.position.x += 3.25F;
            shieldSession.update(
                1.0F / 60.0F, shieldVehicles, shieldInput);
            if (shieldSession.racers().front().shieldSeconds > 0.0F ||
                !shieldSession.bonusActive().back())
            {
                throw std::runtime_error(
                    "source projectile box rejected a separating axis");
            }
            shieldVehicles[0].body.position =
                shieldRace.bonuses.back().transform.position;
            shieldSession.update(
                1.0F / 60.0F, shieldVehicles, shieldInput);
            const auto& picked = shieldSession.racers().front();
            if (std::abs(picked.shieldSeconds - 10.0F) > 0.001F ||
                picked.shieldEffectSeconds != 0.0F ||
                picked.shieldFadeInSeconds != 0.0F ||
                picked.shieldFadeOutSeconds >= 0.0F)
            {
                throw std::runtime_error(
                    "source ImmortalEffect activation transition failed");
            }

            const float lifeBeforeShieldDamage = picked.life;
            shieldVehicles[0].speed = 25.0F;
            shieldVehicles[0].linearVelocity =
                {-25.0F, 5.0F, 0.0F};
            shieldVehicles[0].bodyContacts = {
                {r3d::physics::CollisionSurface::TrackBorder,
                 std::numeric_limits<std::size_t>::max(),
                 {1.0F, 0.0F, 0.0F}, 25.0F, 4000000.0F}};
            shieldSession.update(0.1F, shieldVehicles, shieldInput);
            const bool hasImmortalDamage = std::any_of(
                shieldSession.events().begin(),
                shieldSession.events().end(),
                [](const RaceEvent& event) {
                    return event.kind == RaceEventKind::Damage &&
                           event.racer == 0U &&
                           event.value > 0.0F &&
                           event.damageType == DamageType::Touch;
                });
            const auto& damaged = shieldSession.racers().front();
            if (damaged.life != lifeBeforeShieldDamage ||
                !hasImmortalDamage ||
                damaged.shieldDamageSeconds != 0.0F ||
                std::abs(damaged.shieldFadeInSeconds - 0.1F) >
                    0.001F ||
                std::abs(damaged.shieldEffectSeconds - 0.1F) >
                    0.001F)
            {
                throw std::runtime_error(
                    "source ImmortalEffect damage flash transition failed");
            }

            shieldVehicles[0].speed = 0.0F;
            shieldVehicles[0].linearVelocity = {};
            shieldVehicles[0].bodyContacts.clear();
            unsigned expirySteps = 0U;
            while (shieldSession.racers().front().shieldSeconds > 0.0F &&
                   expirySteps++ < 110U)
            {
                shieldSession.update(
                    0.1F, shieldVehicles, shieldInput);
            }
            const auto& fading = shieldSession.racers().front();
            if (fading.shieldSeconds != 0.0F ||
                fading.shieldFadeOutSeconds < 0.0F ||
                fading.shieldFadeOutSeconds >= 0.5F ||
                fading.shieldEffectSeconds <= 0.0F)
            {
                throw std::runtime_error(
                    "source ImmortalEffect fade-out did not start");
            }
            while (shieldSession.racers().front()
                       .shieldFadeOutSeconds >= 0.0F &&
                   expirySteps++ < 120U)
            {
                shieldSession.update(
                    0.1F, shieldVehicles, shieldInput);
            }
            const auto& faded = shieldSession.racers().front();
            if (faded.shieldFadeOutSeconds >= 0.0F ||
                faded.shieldEffectSeconds != 0.0F)
            {
                throw std::runtime_error(
                    "source ImmortalEffect fade-out did not free effect");
            }
        }

        {
            OriginalRaceSession deathSession(race);
            auto deathVehicles = vehicles;
            RaceControl deathInput;
            for (int frame = 0; frame < 190; ++frame)
                deathSession.update(
                    1.0F / 60.0F, deathVehicles, deathInput);
            deathVehicles[0].speed = 25.0F;
            deathVehicles[0].linearVelocity = {-25.0F, 0.0F, 0.0F};
            deathVehicles[0].bodyContacts = {
                {r3d::physics::CollisionSurface::TrackBorder,
                 std::numeric_limits<std::size_t>::max(),
                 {1.0F, 0.0F, 0.0F}, 25.0F, 4000000.0F}};
            for (int frame = 0;
                 frame < 100 &&
                 !deathSession.racers().front().destroyed;
                 ++frame)
            {
                deathSession.update(
                    1.0F / 60.0F, deathVehicles, deathInput);
            }
            const auto& sourceRacer = race.racers.front();
            const auto& sourceVehicle =
                sourceRacer.hasConfiguredVehicle
                    ? sourceRacer.configuredVehicle
                    : race.vehicles.at(sourceRacer.vehicle);
            const auto sourceDeathEffectCount = static_cast<std::size_t>(
                std::count_if(
                    deathSession.effects().begin(),
                    deathSession.effects().end(),
                    [](const RaceEffect& effect) {
                        return effect.kind ==
                               RaceEventKind::VehicleDestroyed;
                    }));
            const auto sourceDeathSoundCount = static_cast<std::size_t>(
                std::count_if(
                    deathSession.events().begin(),
                    deathSession.events().end(),
                    [](const RaceEvent& event) {
                        return event.kind == RaceEventKind::EffectSound &&
                               event.soundPath.find("carcrash05.ogg") !=
                                   std::string::npos;
                    }));
            const auto expectedDeathSoundCount = static_cast<std::size_t>(
                std::count_if(
                    sourceVehicle.deathEffects.begin(),
                    sourceVehicle.deathEffects.end(),
                    [](const DeathEffectDefinition& effect) {
                        return !effect.visual.soundPaths.empty();
                    }));
            if (!deathSession.racers().front().destroyed ||
                deathSession.racers().front().life != 0.0F ||
                deathSession.racers().front().lowLife ||
                !deathSession.takeRespawns().empty() ||
                sourceVehicle.deathEffects.size() != 2U ||
                sourceDeathEffectCount !=
                    sourceVehicle.deathEffects.size() ||
                sourceDeathSoundCount != expectedDeathSoundCount)
            {
                throw std::runtime_error(
                    "source vehicle death effects/sounds/immediate removal failed");
            }
            deathVehicles[0].bodyContacts.clear();
            deathVehicles[0].speed = 0.0F;
            deathVehicles[0].linearVelocity = {};
            for (int step = 0; step < 19; ++step)
                deathSession.update(0.1F, deathVehicles, deathInput);
            if (!deathSession.racers().front().destroyed ||
                !deathSession.takeRespawns().empty())
            {
                throw std::runtime_error(
                    "source two-second vehicle restore fired early");
            }
            std::vector<RespawnRequest> deathRespawns;
            for (int step = 0;
                 step < 2 && deathRespawns.empty(); ++step)
            {
                deathSession.update(0.1F, deathVehicles, deathInput);
                deathRespawns = deathSession.takeRespawns();
            }
            if (deathRespawns.size() != 1U ||
                deathSession.racers().front().life !=
                    deathSession.racers().front().maximumLife ||
                !deathSession.racers().front().destroyed)
            {
                throw std::runtime_error(
                    "source two-second vehicle restore was not queued");
            }
            deathSession.update(0.1F, deathVehicles, deathInput);
            if (deathSession.racers().front().destroyed)
            {
                throw std::runtime_error(
                    "source restored vehicle did not re-enter gameplay");
            }
        }

        if (vehicles.size() > 1U)
        {
            {
                OriginalRaceSession overboardSession(race);
                auto overboardVehicles = vehicles;
                RaceControl overboardInput;
                for (int frame = 0; frame < 190; ++frame)
                    overboardSession.update(
                        1.0F / 60.0F, overboardVehicles,
                        overboardInput);
                overboardVehicles[0].speed = 20.0F;
                overboardVehicles[1].speed = 5.0F;
                overboardVehicles[0].bodyContacts = {
                    {r3d::physics::CollisionSurface::Vehicle, 1U,
                     {-1.0F, 0.0F, 0.0F}, 20.0F,
                     1200000.0F}};
                overboardSession.update(
                    1.0F / 60.0F, overboardVehicles,
                    overboardInput);
                if (overboardSession.racers()[1].touchAttacker != 0U ||
                    overboardSession.racers()[1]
                            .touchAttributionSeconds <= 0.0F)
                {
                    throw std::runtime_error(
                        "source three-second touch attribution was not set");
                }
                overboardVehicles[0].bodyContacts.clear();
                overboardVehicles[1].body.position.z = -20.0F;
                overboardSession.update(
                    1.0F / 60.0F, overboardVehicles,
                    overboardInput);
                const bool attributedOverboard = std::any_of(
                    overboardSession.events().begin(),
                    overboardSession.events().end(),
                    [](const RaceEvent& event) {
                        return event.kind == RaceEventKind::Kill &&
                               event.target == 1U &&
                               event.racer == 0U &&
                               !event.killCredit &&
                               event.damageType ==
                                   DamageType::DeathPlane;
                    });
                if (!overboardSession.racers()[1].destroyed ||
                    !attributedOverboard ||
                    !overboardSession.takeRespawns().empty())
                {
                    throw std::runtime_error(
                        "source TouchDeath/death-plane attribution failed");
                }
            }

            vehicles[1].body.position = vehicles[0].body.position;
            vehicles[1].speed = 5.0F;
            const float firstLife = session.racers()[0].life;
            const float secondLife = session.racers()[1].life;
            session.update(1.0F / 60.0F, vehicles, input);
            if (session.racers()[0].life != firstLife ||
                session.racers()[1].life != secondLife)
                throw std::runtime_error(
                    "car proximity caused damage without a body contact");

            vehicles[0].bodyContacts = {
                {r3d::physics::CollisionSurface::Vehicle, 1U,
                 {-1.0F, 0.0F, 0.0F}, 20.0F, 1200000.0F}};
            session.update(1.0F / 60.0F, vehicles, input);
            if (session.racers()[1].life >= secondLife)
                throw std::runtime_error(
                    "source car-contact damage transition failed");
            vehicles[0].bodyContacts.clear();
        }

        OriginalRaceSession lapSession(race);
        auto lapVehicles = vehicles;
        for (auto& vehicle : lapVehicles)
        {
            vehicle.bodyContacts.clear();
            vehicle.speed = 0.0F;
            vehicle.linearVelocity = {};
        }
        for (int frame = 0; frame < 190; ++frame)
            lapSession.update(
                1.0F / 60.0F, lapVehicles, input);
        auto placeOnMainSegment = [&](std::size_t segment) {
            const auto& start = point(segment);
            const auto& end = point(segment + 1U);
            lapVehicles[0].body.position = multiply(
                add(start.position, end.position), 0.5F);
            lapVehicles[0].body.position.z =
                (start.position.z + end.position.z) * 0.5F + 2.0F;
            lapVehicles[0].body.rotation = shortestArcFromX(
                normalized2(subtract(end.position, start.position)));
            lapSession.update(
                1.0F / 60.0F, lapVehicles, input);
        };
        for (std::size_t segment = 0U;
             segment + 1U < race.tracePath.size(); ++segment)
            placeOnMainSegment(segment);
        // The source WayPath can retain its final tile at the shared finish
        // point for one update.  Losing curTile and reacquiring the linked
        // first node is the exact CarState lap transition.
        lapVehicles[0].body.position.x += 1000.0F;
        lapVehicles[0].body.position.y += 1000.0F;
        lapSession.update(
            1.0F / 60.0F, lapVehicles, input);
        placeOnMainSegment(0U);
        if (lapSession.racers().front().completedLaps != 1 ||
            lapSession.racers().front().nextPathNode != 1)
            throw std::runtime_error("checkpoint/lap transition failed");
        lapSession.update(
            1.0F / 60.0F, lapVehicles, input);
        if (lapSession.racers().front().completedLaps != 1U)
            throw std::runtime_error("finish tile counted the same lap twice");
        // Preserve the shared fixture position used by the subsequent
        // projectile/mine source regressions.
        vehicles[0].body.position = point(
            race.tracePath.size() - 1U).position;
        vehicles[0].speed = 0.0F;
        vehicles[0].linearVelocity = {};

        input.useWeapon = true;
        if (vehicles.size() > 1)
        {
            const auto ammunition =
                session.racers().front().ammunition;
            vehicles[1].body.position =
                add(vehicles[0].body.position, {10.0F, 0.0F, 0.0F});
            session.update(1.0F / 60.0F, vehicles, input);
            if (ammunition == 0 ||
                session.racers().front().ammunition + 1U != ammunition)
                throw std::runtime_error("weapon/ammunition transition failed");
        }
        input.useWeapon = false;

        std::vector<const WeaponDefinition*> primaryWeapons;
        for (const auto& weapon : race.weapons)
        {
            if (weapon.slot == WeaponSlot::Primary)
                primaryWeapons.push_back(&weapon);
            if (primaryWeapons.size() == 2U)
                break;
        }
        if (primaryWeapons.size() == 2U)
        {
            OriginalRaceSession weaponSession(race);
            PlayerProfile weaponProfile;
            for (std::size_t slot = 0; slot < 2U; ++slot)
            {
                auto& profileSlot = weaponProfile.slots[
                    PlayerProfile::firstWeaponSlot + slot];
                profileSlot.record = primaryWeapons[slot]->record;
                profileSlot.charge = std::max(
                    primaryWeapons[slot]->reloadCharge, 2U);
                profileSlot.hasCharge = true;
            }
            weaponSession.applyPlayerProfile(weaponProfile);
            RaceControl weaponInput;
            for (int frame = 0; frame < 190; ++frame)
                weaponSession.update(
                    1.0F / 60.0F, vehicles, weaponInput);
            weaponInput.changeWeapon = true;
            weaponInput.weaponChange = 1;
            weaponSession.update(
                1.0F / 60.0F, vehicles, weaponInput);
            if (weaponSession.racers().front().selectedWeaponSlot != 1U)
                throw std::runtime_error(
                    "source next-weapon transition failed");
            weaponInput.weaponChange = -1;
            weaponSession.update(
                1.0F / 60.0F, vehicles, weaponInput);
            if (weaponSession.racers().front().selectedWeaponSlot != 0U)
                throw std::runtime_error(
                    "source previous-weapon transition failed");
            weaponInput = {};
            const auto beforeAll =
                weaponSession.racers().front().weaponCharges;
            weaponInput.useAllWeapons = true;
            weaponSession.update(
                1.0F / 60.0F, vehicles, weaponInput);
            const auto afterAll =
                weaponSession.racers().front().weaponCharges;
            if (afterAll[0] + 1U != beforeAll[0] ||
                afterAll[1] + 1U != beforeAll[1])
            {
                throw std::runtime_error(
                    "source ShotAll/per-weapon cooldown transition failed");
            }
            for (const auto* sourceWeapon : primaryWeapons)
            {
                if (sourceWeapon->shotEffect.visual.visualNodes.empty() &&
                    sourceWeapon->shotEffect.visual.particleEmitters.empty())
                    continue;
                const auto weaponIndex = static_cast<std::size_t>(
                    sourceWeapon - race.weapons.data());
                const bool emitted = std::any_of(
                    weaponSession.effects().begin(),
                    weaponSession.effects().end(),
                    [&](const RaceEffect& effect) {
                        return effect.kind ==
                                   RaceEventKind::WeaponShotEffect &&
                               effect.weapon == weaponIndex &&
                               std::abs(
                               effect.totalSeconds -
                                   sourceEffectTiming(
                                       sourceWeapon->shotEffect.visual,
                                       sourceWeapon->shotEffect.duration)
                                       .visibleSeconds) <
                                   0.001F;
                    });
                if (!emitted)
                {
                    throw std::runtime_error(
                        "source ctWeapon ShotEffect was not emitted");
                }
            }
            weaponInput = {};
            for (int frame = 0; frame < 60; ++frame)
                weaponSession.update(
                    1.0F / 60.0F, vehicles, weaponInput);
            const auto beforeDirect =
                weaponSession.racers().front().weaponCharges;
            weaponInput.fireWeaponSlot = 1;
            weaponSession.update(
                1.0F / 60.0F, vehicles, weaponInput);
            const auto afterDirect =
                weaponSession.racers().front().weaponCharges;
            if (afterDirect[0] != beforeDirect[0] ||
                afterDirect[1] + 1U != beforeDirect[1] ||
                weaponSession.racers().front().selectedWeaponSlot != 0U)
            {
                throw std::runtime_error(
                    "source Shot1..4 direct-slot transition failed");
            }
        }

        const auto hyperdrive = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return recordName(weapon.record) == "hyperdrive";
            });
        const auto spring = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return recordName(weapon.record) == "spring";
            });
        if (hyperdrive == race.weapons.end() ||
            hyperdrive->projectiles.size() != 1U ||
            hyperdrive->projectiles.front().type != 1U ||
            std::abs(
                hyperdrive->projectiles.front().speed - 10.0F) >
                0.001F ||
            std::abs(hyperdrive->shotDelay - 1.5F) > 0.001F ||
            spring == race.weapons.end() ||
            spring->projectiles.size() != 1U ||
            spring->projectiles.front().type != 17U ||
            std::abs(spring->projectiles.front().speed - 17.0F) >
                0.001F)
        {
            throw std::runtime_error(
                "source hyperdrive/spring definitions were not preserved");
        }
        auto hyperVehicles = vehicles;
        hyperVehicles[0].body.rotation = {};
        {
            OriginalRaceSession hyperSession(race);
            PlayerProfile hyperProfile;
            auto& slot =
                hyperProfile.slots[PlayerProfile::hyperSlot];
            slot.record = hyperdrive->record;
            slot.charge = 2U;
            slot.hasCharge = true;
            hyperSession.applyPlayerProfile(hyperProfile);
            RaceControl hyperInput;
            for (int frame = 0; frame < 190; ++frame)
                hyperSession.update(
                    1.0F / 60.0F, hyperVehicles, hyperInput);
            hyperSession.takeVelocityRequests();
            hyperInput.useHyper = true;
            hyperSession.update(
                1.0F / 60.0F, hyperVehicles, hyperInput);
            const auto requests =
                hyperSession.takeVelocityRequests();
            const bool attachedSourceEffect = std::any_of(
                hyperSession.projectiles().begin(),
                hyperSession.projectiles().end(),
                [&](const ProjectileRuntime& projectile) {
                    return projectile.weapon ==
                               static_cast<std::size_t>(
                                   hyperdrive -
                                   race.weapons.begin()) &&
                           projectile.projectile == 0U &&
                           projectile.attached &&
                           projectile.directWeapon;
                });
            const bool syntheticHyperEffect = std::any_of(
                hyperSession.effects().begin(),
                hyperSession.effects().end(),
                [](const RaceEffect& effect) {
                    return effect.kind ==
                           RaceEventKind::HyperActivated;
                });
            if (requests.size() != 1U ||
                std::abs(requests.front().delta.x - 10.0F) >
                    0.001F ||
                std::abs(requests.front().delta.y) > 0.001F ||
                std::abs(requests.front().delta.z) > 0.001F ||
                hyperSession.racers().front().hyperCharge != 1U ||
                !attachedSourceEffect || syntheticHyperEffect)
            {
                throw std::runtime_error(
                    "source ptHyper local impulse/linked visual failed");
            }
            hyperSession.update(
                1.0F / 60.0F, hyperVehicles, hyperInput);
            if (hyperSession.racers().front().hyperCharge != 1U)
            {
                throw std::runtime_error(
                    "source ptHyper shotDelay cooldown failed");
            }
        }
        {
            OriginalRaceSession springSession(race);
            PlayerProfile springProfile;
            auto& slot =
                springProfile.slots[PlayerProfile::hyperSlot];
            slot.record = spring->record;
            slot.charge = 1U;
            slot.hasCharge = true;
            springSession.applyPlayerProfile(springProfile);
            RaceControl springInput;
            const auto& playerDefinition =
                race.racers.front().hasConfiguredVehicle
                    ? race.racers.front().configuredVehicle
                    : race.vehicles.at(
                          race.racers.front().vehicle);
            hyperVehicles[0].contactCount =
                static_cast<std::uint32_t>(
                    playerDefinition.physics.wheels.size());
            for (int frame = 0; frame < 190; ++frame)
                springSession.update(
                    1.0F / 60.0F, hyperVehicles, springInput);
            springSession.takeVelocityRequests();
            springInput.useHyper = true;
            springSession.update(
                1.0F / 60.0F, hyperVehicles, springInput);
            const auto requests =
                springSession.takeVelocityRequests();
            if (requests.size() != 1U ||
                std::abs(requests.front().delta.x) > 0.001F ||
                std::abs(requests.front().delta.y) > 0.001F ||
                std::abs(requests.front().delta.z - 17.0F) >
                    0.001F ||
                springSession.racers().front().hyperCharge != 0U ||
                springSession.racers().front().springLockSeconds <
                    1.49F ||
                !springSession.vehicleInputs().front().springLocked)
            {
                throw std::runtime_error(
                    "source ptSpring wheel contact/local impulse/lock failed");
            }
        }
        {
            OriginalRaceSession airborneSpringSession(race);
            PlayerProfile springProfile;
            auto& slot =
                springProfile.slots[PlayerProfile::hyperSlot];
            slot.record = spring->record;
            slot.charge = 1U;
            slot.hasCharge = true;
            airborneSpringSession.applyPlayerProfile(springProfile);
            RaceControl springInput;
            hyperVehicles[0].contactCount = 0U;
            for (int frame = 0; frame < 190; ++frame)
                airborneSpringSession.update(
                    1.0F / 60.0F, hyperVehicles, springInput);
            airborneSpringSession.takeVelocityRequests();
            springInput.useHyper = true;
            airborneSpringSession.update(
                1.0F / 60.0F, hyperVehicles, springInput);
            if (airborneSpringSession.racers()
                    .front()
                    .hyperCharge != 1U ||
                !airborneSpringSession
                     .takeVelocityRequests()
                     .empty())
            {
                throw std::runtime_error(
                    "source ptSpring airborne PrepareProj rejection failed");
            }
        }

        const auto drobilka = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return recordName(weapon.record) == "drobilka";
            });
        if (drobilka == race.weapons.end() ||
            drobilka->projectiles.size() != 1U ||
            drobilka->projectiles.front().type != 15U ||
            std::abs(
                drobilka->projectiles.front().angularSpeed -
                12.56637061435917295384F) > 0.001F ||
            std::abs(
                drobilka->projectiles.front().minimumLife - 4.0F) >
                0.001F ||
            (drobilka->projectiles.front().visual.visualNodes.empty() &&
             drobilka->projectiles.front()
                 .visual.particleEmitters.empty()))
        {
            throw std::runtime_error(
                "source ptDrobilka definition was not preserved");
        }
        if (vehicles.size() > 1U)
        {
            OriginalRaceSession drobilkaSession(race);
            PlayerProfile drobilkaProfile;
            auto& drobilkaSlot = drobilkaProfile.slots[
                PlayerProfile::firstWeaponSlot];
            drobilkaSlot.record = drobilka->record;
            drobilkaSlot.charge = 2U;
            drobilkaSlot.hasCharge = true;
            drobilkaSession.applyPlayerProfile(drobilkaProfile);
            auto drobilkaVehicles = vehicles;
            RaceControl drobilkaInput;
            for (int frame = 0; frame < 190; ++frame)
            {
                drobilkaSession.update(
                    1.0F / 60.0F, drobilkaVehicles,
                    drobilkaInput);
            }
            drobilkaInput.useWeapon = true;
            drobilkaSession.update(
                1.0F / 60.0F, drobilkaVehicles,
                drobilkaInput);
            drobilkaInput.useWeapon = false;
            const std::size_t drobilkaWeapon =
                static_cast<std::size_t>(
                    drobilka - race.weapons.begin());
            const auto activeDrobilka = std::find_if(
                drobilkaSession.projectiles().begin(),
                drobilkaSession.projectiles().end(),
                [drobilkaWeapon](
                    const ProjectileRuntime& projectile) {
                    return projectile.owner == 0U &&
                           projectile.weapon == drobilkaWeapon &&
                           projectile.projectile == 0U &&
                           projectile.attached &&
                           projectile.active;
                });
            if (activeDrobilka ==
                    drobilkaSession.projectiles().end() ||
                std::abs(
                    activeDrobilka->angularSpeed -
                    drobilka->projectiles.front().angularSpeed) >
                    0.001F)
            {
                throw std::runtime_error(
                    "source ptDrobilka attached contact actor was not "
                    "created");
            }
            const auto& targetDefinition =
                race.racers[1].hasConfiguredVehicle
                    ? race.racers[1].configuredVehicle
                    : race.vehicles.at(race.racers[1].vehicle);
            Transform drobilkaTransform;
            drobilkaTransform.position =
                activeDrobilka->position;
            drobilkaTransform.rotation =
                activeDrobilka->rotation;
            const OrientedBox drobilkaBox = orientedBox(
                drobilkaTransform,
                drobilka->projectiles.front().collision);
            drobilkaVehicles[1].body.rotation = {};
            drobilkaVehicles[1].body.position = subtract(
                drobilkaBox.center,
                targetDefinition.physics.shapePosition);
            const float lifeBeforeDrobilka =
                drobilkaSession.racers()[1].life;
            drobilkaSession.update(
                1.0F / 60.0F, drobilkaVehicles,
                drobilkaInput);
            const auto sourceContactEffect = std::find_if(
                drobilkaSession.effects().begin(),
                drobilkaSession.effects().end(),
                [drobilkaWeapon](const RaceEffect& effect) {
                    return effect.kind ==
                               RaceEventKind::ProjectileImpact &&
                           effect.weapon == drobilkaWeapon &&
                           effect.projectile == 0U &&
                           effect.visualVariant == 4U &&
                           effect.racer == 0U &&
                           effect.mountSlot == 0U &&
                           std::abs(effect.totalSeconds - 0.5F) <
                               0.001F;
                });
            const float expectedSpin =
                drobilka->projectiles.front().angularSpeed / 60.0F;
            if (sourceContactEffect ==
                    drobilkaSession.effects().end() ||
                drobilkaSession.racers()[1].life >=
                    lifeBeforeDrobilka ||
                std::abs(
                    drobilkaSession.racers()[0]
                            .weaponSpinRadians[0] -
                    expectedSpin) > 0.001F)
            {
                throw std::runtime_error(
                    "source DrobilkaContact/DrobilkaUpdate transition "
                    "failed");
            }
            drobilkaVehicles[1].body.position = {
                100000.0F, 100000.0F, 100000.0F};
            for (int frame = 0; frame < 31; ++frame)
            {
                drobilkaSession.update(
                    1.0F / 60.0F, drobilkaVehicles,
                    drobilkaInput);
            }
            const bool staleContactEffect = std::any_of(
                drobilkaSession.effects().begin(),
                drobilkaSession.effects().end(),
                [drobilkaWeapon](const RaceEffect& effect) {
                    return effect.kind ==
                               RaceEventKind::ProjectileImpact &&
                           effect.weapon == drobilkaWeapon &&
                           effect.visualVariant == 4U &&
                           effect.racer == 0U;
                });
            if (staleContactEffect)
            {
                throw std::runtime_error(
                    "source ptDrobilka contact model did not expire "
                    "after 0.5 seconds");
            }
        }

        const auto thunder = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return recordName(weapon.record) == "sonar";
            });
        const auto resonator = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return recordName(weapon.record) == "rezonator";
            });
        const auto rocketLauncher = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return recordName(weapon.record) ==
                       "rocketLauncher";
            });
        if (thunder == race.weapons.end() ||
            thunder->projectiles.size() != 1U ||
            thunder->projectiles.front().type != 22U ||
            std::abs(thunder->projectiles.front().speed - 40.0F) >
                0.001F ||
            std::abs(
                thunder->projectiles.front().maximumDistance -
                200.0F) > 0.001F ||
            resonator == race.weapons.end() ||
            resonator->projectiles.size() != 1U ||
            resonator->projectiles.front().type != 23U ||
            std::abs(
                resonator->projectiles.front().angularSpeed -
                32.0F) > 0.001F ||
            rocketLauncher == race.weapons.end() ||
            rocketLauncher->projectiles.empty() ||
            rocketLauncher->projectiles.front().type != 0U)
        {
            throw std::runtime_error(
                "source Rocket/Thunder/Resonanse definitions were not "
                "preserved");
        }
        {
            bool testedBorder = false;
            for (const auto& mesh : race.collisionMeshes)
            {
                if (mesh.surface !=
                        r3d::physics::CollisionSurface::TrackBorder ||
                    mesh.indices.size() < 3U)
                    continue;
                const auto firstIndex = mesh.indices[0];
                const auto secondIndex = mesh.indices[1];
                const auto thirdIndex = mesh.indices[2];
                if (firstIndex >= mesh.vertices.size() ||
                    secondIndex >= mesh.vertices.size() ||
                    thirdIndex >= mesh.vertices.size())
                    continue;
                const Vec3 first = transformPoint(
                    mesh.transform, mesh.vertices[firstIndex]);
                const Vec3 second = transformPoint(
                    mesh.transform, mesh.vertices[secondIndex]);
                const Vec3 third = transformPoint(
                    mesh.transform, mesh.vertices[thirdIndex]);
                ProjectileCollisionBox testCollision;
                testCollision.halfExtents = {0.1F, 0.1F, 0.1F};
                Transform testTransform;
                testTransform.position = multiply(
                    add(add(first, second), third), 1.0F / 3.0F);
                Vec3 normal;
                if (!trackBorderContact(
                        race,
                        orientedBox(testTransform, testCollision),
                        normal))
                    continue;
                const Vec3 incoming = multiply(normal, -10.0F);
                const Vec3 reflected =
                    thunderReflection(incoming, normal);
                if (dot3(reflected, normal) <= 0.0F ||
                    std::abs(length3(reflected) - 10.0F) > 0.001F)
                {
                    throw std::runtime_error(
                        "source ThunderContact normal reflection failed");
                }
                testedBorder = true;
                break;
            }
            if (!testedBorder)
            {
                throw std::runtime_error(
                    "source cdgShotTransparency track border was not "
                    "available to ThunderContact");
            }
        }
        {
            OriginalRaceSession thunderSession(race);
            PlayerProfile thunderProfile;
            auto& slot = thunderProfile.slots[
                PlayerProfile::firstWeaponSlot];
            slot.record = thunder->record;
            slot.charge = 2U;
            slot.hasCharge = true;
            thunderSession.applyPlayerProfile(thunderProfile);
            auto outsideVehicles = vehicles;
            for (std::size_t index = 0;
                 index < outsideVehicles.size(); ++index)
            {
                outsideVehicles[index].body.position = {
                    100000.0F + static_cast<float>(index) * 10000.0F,
                    100000.0F, 1000.0F};
                outsideVehicles[index].body.rotation = {};
                outsideVehicles[index].linearVelocity = {};
            }
            outsideVehicles[0].linearVelocity = {
                80.0F, 0.0F, 0.0F};
            constexpr float sourcePitch = 0.25F;
            outsideVehicles[0].body.rotation = {
                0.0F, std::sin(sourcePitch * 0.5F), 0.0F,
                std::cos(sourcePitch * 0.5F)};
            RaceControl thunderInput;
            for (int frame = 0; frame < 190; ++frame)
            {
                thunderSession.update(
                    1.0F / 60.0F, outsideVehicles,
                    thunderInput);
            }
            thunderInput.useWeapon = true;
            thunderSession.update(
                1.0F / 60.0F, outsideVehicles, thunderInput);
            thunderInput.useWeapon = false;
            const std::size_t thunderWeapon =
                static_cast<std::size_t>(
                    thunder - race.weapons.begin());
            auto sourceProjectile = std::find_if(
                thunderSession.projectiles().begin(),
                thunderSession.projectiles().end(),
                [thunderWeapon](
                    const ProjectileRuntime& projectile) {
                    return projectile.owner == 0U &&
                           projectile.weapon == thunderWeapon &&
                           projectile.projectile == 0U;
                });
            if (sourceProjectile ==
                    thunderSession.projectiles().end() ||
                std::abs(sourceProjectile->speed - 120.0F) > 0.001F ||
                std::abs(sourceProjectile->lifeSeconds - 5.0F) >
                    0.001F ||
                std::abs(sourceProjectile->direction.z) > 0.001F ||
                std::abs(rotate(
                             sourceProjectile->rotation,
                             {1.0F, 0.0F, 0.0F})
                             .z) < 0.05F)
            {
                throw std::runtime_error(
                    "source RocketPrepare direction/speed/lifetime failed");
            }
            for (int frame = 0; frame < 121; ++frame)
            {
                thunderSession.update(
                    1.0F / 60.0F, outsideVehicles,
                    thunderInput);
            }
            sourceProjectile = std::find_if(
                thunderSession.projectiles().begin(),
                thunderSession.projectiles().end(),
                [thunderWeapon](
                    const ProjectileRuntime& projectile) {
                    return projectile.owner == 0U &&
                           projectile.weapon == thunderWeapon &&
                           projectile.projectile == 0U;
                });
            if (sourceProjectile ==
                    thunderSession.projectiles().end() ||
                sourceProjectile->distance <=
                    thunder->projectiles.front().maximumDistance)
            {
                throw std::runtime_error(
                    "source maxDist/speed lifetime was replaced by a "
                    "distance clamp");
            }
        }
        {
            OriginalRaceSession resonatorSession(race);
            PlayerProfile resonatorProfile;
            auto& slot = resonatorProfile.slots[
                PlayerProfile::firstWeaponSlot];
            slot.record = resonator->record;
            slot.charge = 1U;
            slot.hasCharge = true;
            resonatorSession.applyPlayerProfile(resonatorProfile);
            auto outsideVehicles = vehicles;
            for (std::size_t index = 0;
                 index < outsideVehicles.size(); ++index)
            {
                outsideVehicles[index].body.position = {
                    100000.0F + static_cast<float>(index) * 10000.0F,
                    -100000.0F, 1000.0F};
                outsideVehicles[index].body.rotation = {};
                outsideVehicles[index].linearVelocity = {};
            }
            RaceControl resonatorInput;
            for (int frame = 0; frame < 190; ++frame)
            {
                resonatorSession.update(
                    1.0F / 60.0F, outsideVehicles,
                    resonatorInput);
            }
            resonatorInput.useWeapon = true;
            resonatorSession.update(
                1.0F / 60.0F, outsideVehicles,
                resonatorInput);
            resonatorInput.useWeapon = false;
            const std::size_t resonatorWeapon =
                static_cast<std::size_t>(
                    resonator - race.weapons.begin());
            auto projectile = std::find_if(
                resonatorSession.projectiles().begin(),
                resonatorSession.projectiles().end(),
                [resonatorWeapon](
                    const ProjectileRuntime& value) {
                    return value.owner == 0U &&
                           value.weapon == resonatorWeapon &&
                           value.projectile == 0U;
                });
            if (projectile ==
                resonatorSession.projectiles().end())
            {
                throw std::runtime_error(
                    "source ptResonanse projectile was not created");
            }
            const Quat beforeRotation = projectile->rotation;
            resonatorSession.update(
                1.0F / 60.0F, outsideVehicles,
                resonatorInput);
            projectile = std::find_if(
                resonatorSession.projectiles().begin(),
                resonatorSession.projectiles().end(),
                [resonatorWeapon](
                    const ProjectileRuntime& value) {
                    return value.owner == 0U &&
                           value.weapon == resonatorWeapon &&
                           value.projectile == 0U;
                });
            const float halfAngle =
                resonator->projectiles.front().angularSpeed /
                120.0F;
            const Quat expectedRotation = multiply(
                beforeRotation,
                {std::sin(halfAngle), 0.0F, 0.0F,
                 std::cos(halfAngle)});
            if (projectile ==
                    resonatorSession.projectiles().end() ||
                std::abs(
                    projectile->rotation.x - expectedRotation.x) >
                    0.001F ||
                std::abs(
                    projectile->rotation.y - expectedRotation.y) >
                    0.001F ||
                std::abs(
                    projectile->rotation.z - expectedRotation.z) >
                    0.001F ||
                std::abs(
                    projectile->rotation.w - expectedRotation.w) >
                    0.001F)
            {
                throw std::runtime_error(
                    "source ResonanseUpdate actor rotation failed");
            }
        }
        {
            OriginalRaceSession rocketSession(race);
            PlayerProfile rocketProfile;
            auto& slot = rocketProfile.slots[
                PlayerProfile::firstWeaponSlot];
            slot.record = rocketLauncher->record;
            slot.charge = 1U;
            slot.hasCharge = true;
            rocketSession.applyPlayerProfile(rocketProfile);
            auto rocketVehicles = vehicles;
            for (std::size_t index = 1;
                 index < rocketVehicles.size(); ++index)
            {
                rocketVehicles[index].body.position = {
                    100000.0F + static_cast<float>(index) * 1000.0F,
                    100000.0F, 1000.0F};
            }
            RaceControl rocketInput;
            for (int frame = 0; frame < 190; ++frame)
            {
                rocketSession.update(
                    1.0F / 60.0F, rocketVehicles,
                    rocketInput);
            }
            rocketInput.useWeapon = true;
            rocketSession.update(
                1.0F / 60.0F, rocketVehicles, rocketInput);
            rocketInput.useWeapon = false;
            rocketSession.update(
                1.0F / 60.0F, rocketVehicles, rocketInput);
            const std::size_t rocketWeapon =
                static_cast<std::size_t>(
                    rocketLauncher - race.weapons.begin());
            const auto projectile = std::find_if(
                rocketSession.projectiles().begin(),
                rocketSession.projectiles().end(),
                [rocketWeapon](
                    const ProjectileRuntime& value) {
                    return value.owner == 0U &&
                           value.weapon == rocketWeapon &&
                           value.projectile == 0U;
                });
            if (projectile ==
                    rocketSession.projectiles().end() ||
                projectile->trackClearance <= 0.0F)
            {
                throw std::runtime_error(
                    "source RocketUpdate TrackPlane clearance failed");
            }
        }

        const auto phaseImpulse = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return recordName(weapon.record) == "phaseImpulse";
            });
        if (phaseImpulse == race.weapons.end() ||
            phaseImpulse->projectiles.size() != 1U ||
            phaseImpulse->projectiles.front().type != 21U ||
            std::abs(
                phaseImpulse->projectiles.front().damage - 8.0F) >
                0.001F)
        {
            throw std::runtime_error(
                "source ptImpulse definition was not preserved");
        }
        if (vehicles.size() > 3U)
        {
            OriginalRaceSession impulseSession(race);
            PlayerProfile impulseProfile;
            auto& slot = impulseProfile.slots[
                PlayerProfile::firstWeaponSlot];
            slot.record = phaseImpulse->record;
            slot.charge = 1U;
            slot.hasCharge = true;
            impulseSession.applyPlayerProfile(impulseProfile);
            auto impulseVehicles = vehicles;
            const Vec3 base = vehicles[0].body.position;
            for (std::size_t index = 0;
                 index < impulseVehicles.size(); ++index)
            {
                impulseVehicles[index].body.rotation = {};
                impulseVehicles[index].linearVelocity = {};
                impulseVehicles[index].body.position = {
                    base.x + 1000.0F +
                        static_cast<float>(index) * 100.0F,
                    base.y, base.z};
            }
            impulseVehicles[0].body.position = base;
            impulseVehicles[1].body.position = {
                base.x + 10.0F, base.y, base.z};
            // FindClosestEnemy(pi/2) deliberately prefers candidate 2 by
            // plane distance (2) even though candidate 3 is much closer in
            // Euclidean distance.  This distinguishes the Windows rule
            // from the former projectile-direction heuristic.
            impulseVehicles[2].body.position = {
                base.x + 12.0F, base.y + 100.0F, base.z};
            impulseVehicles[3].body.position = {
                base.x + 20.0F, base.y, base.z};
            RaceControl impulseInput;
            for (int frame = 0; frame < 190; ++frame)
            {
                impulseSession.update(
                    1.0F / 60.0F, impulseVehicles,
                    impulseInput);
            }
            impulseInput.useWeapon = true;
            impulseSession.update(
                1.0F / 60.0F, impulseVehicles, impulseInput);
            impulseInput.useWeapon = false;
            const std::size_t impulseWeapon =
                static_cast<std::size_t>(
                    phaseImpulse - race.weapons.begin());
            bool handedOff = false;
            for (int frame = 0; frame < 60 && !handedOff; ++frame)
            {
                impulseSession.update(
                    1.0F / 60.0F, impulseVehicles,
                    impulseInput);
                const auto projectile = std::find_if(
                    impulseSession.projectiles().begin(),
                    impulseSession.projectiles().end(),
                    [impulseWeapon](
                        const ProjectileRuntime& value) {
                        return value.owner == 0U &&
                               value.weapon == impulseWeapon &&
                               value.projectile == 0U;
                    });
                handedOff =
                    projectile !=
                        impulseSession.projectiles().end() &&
                    projectile->hitCount == 1U &&
                    projectile->target == 2U;
            }
            if (!handedOff ||
                impulseSession.racers()[1].life >=
                    impulseSession.racers()[1].maximumLife)
            {
                throw std::runtime_error(
                    "source ImpulseContact FindClosestEnemy(pi/2) "
                    "handoff failed");
            }
        }

        const auto sphereGun = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return recordName(weapon.record) == "sphereGun";
            });
        if (sphereGun == race.weapons.end() ||
            sphereGun->projectiles.size() != 1U ||
            sphereGun->projectiles.front().type != 2U ||
            sphereGun->projectiles.front().relativeSpeed ||
            std::abs(
                sphereGun->projectiles.front().relativeSpeedMinimum -
                20.0F) > 0.001F ||
            std::abs(
                sphereGun->projectiles.front().angularSpeed -
                12.5664F) > 0.001F)
        {
            throw std::runtime_error(
                "source ptTorpeda definition was not preserved");
        }
        if (vehicles.size() > 1U)
        {
            OriginalRaceSession torpedaSession(race);
            PlayerProfile torpedaProfile;
            auto& slot = torpedaProfile.slots[
                PlayerProfile::firstWeaponSlot];
            slot.record = sphereGun->record;
            slot.charge = 1U;
            slot.hasCharge = true;
            torpedaSession.applyPlayerProfile(torpedaProfile);
            auto torpedaVehicles = vehicles;
            const Vec3 base{100000.0F, -100000.0F, 1000.0F};
            for (std::size_t index = 0;
                 index < torpedaVehicles.size(); ++index)
            {
                torpedaVehicles[index].body.position = {
                    base.x + 1000.0F +
                        static_cast<float>(index) * 100.0F,
                    base.y, base.z};
                torpedaVehicles[index].body.rotation = {};
                torpedaVehicles[index].linearVelocity = {};
            }
            torpedaVehicles[0].body.position = base;
            // sphereGun passes viewAngle=0, so this off-axis target must
            // still be selected.
            torpedaVehicles[1].body.position = {
                base.x + 100.0F, base.y + 100.0F, base.z};
            RaceControl torpedaInput;
            for (int frame = 0; frame < 190; ++frame)
            {
                torpedaSession.update(
                    1.0F / 60.0F, torpedaVehicles,
                    torpedaInput);
            }
            torpedaInput.useWeapon = true;
            torpedaSession.update(
                1.0F / 60.0F, torpedaVehicles,
                torpedaInput);
            torpedaInput.useWeapon = false;
            const std::size_t sphereWeapon =
                static_cast<std::size_t>(
                    sphereGun - race.weapons.begin());
            auto findSphereProjectile = [&]()
                -> const ProjectileRuntime* {
                const auto found = std::find_if(
                    torpedaSession.projectiles().begin(),
                    torpedaSession.projectiles().end(),
                    [sphereWeapon](
                        const ProjectileRuntime& value) {
                        return value.owner == 0U &&
                               value.weapon == sphereWeapon &&
                               value.projectile == 0U;
                    });
                return found ==
                               torpedaSession.projectiles().end()
                           ? nullptr
                           : &*found;
            };
            const auto* projectile = findSphereProjectile();
            if (projectile == nullptr || projectile->target != 1U)
            {
                throw std::runtime_error(
                    "source sphereGun viewAngle=0 target selection failed");
            }
            for (int frame = 0;
                 frame < 30 && projectile != nullptr &&
                 projectile->homingDelay >
                     (1.0F / 60.0F + 0.00001F);
                 ++frame)
            {
                torpedaSession.update(
                    1.0F / 60.0F, torpedaVehicles,
                    torpedaInput);
                projectile = findSphereProjectile();
            }
            if (projectile == nullptr)
            {
                throw std::runtime_error(
                    "source ptTorpeda expired before homing");
            }
            const ProjectileRuntime before = *projectile;
            const Vec3 targetDirection = normalized3(subtract(
                torpedaVehicles[1].body.position,
                before.position));
            const Quat targetRotation =
                shortestArcFromX(targetDirection);
            const Quat expectedRotation = quaternionSlerp(
                before.rotation, targetRotation,
                sphereGun->projectiles.front().angularSpeed /
                    60.0F);
            const Vec3 expectedDirection = normalized3(rotate(
                expectedRotation, {1.0F, 0.0F, 0.0F}));
            const float expectedSpeed = std::max(
                dot3(before.velocity, expectedDirection),
                sphereGun->projectiles.front().speed);
            torpedaSession.update(
                1.0F / 60.0F, torpedaVehicles,
                torpedaInput);
            projectile = findSphereProjectile();
            if (projectile == nullptr ||
                std::abs(
                    projectile->rotation.x - expectedRotation.x) >
                    0.001F ||
                std::abs(
                    projectile->rotation.y - expectedRotation.y) >
                    0.001F ||
                std::abs(
                    projectile->rotation.z - expectedRotation.z) >
                    0.001F ||
                std::abs(
                    projectile->rotation.w - expectedRotation.w) >
                    0.001F ||
                std::abs(projectile->speed - expectedSpeed) >
                    0.001F)
            {
                throw std::runtime_error(
                    "source TorpedaUpdate shortest-arc/slerp/speed "
                    "transition failed");
            }
        }

        const auto frostRay = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return recordName(weapon.record) == "asyncFrost";
            });
        const auto tankLaser = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return recordName(weapon.record) == "tankLaser";
            });
        const auto fireGun = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return recordName(weapon.record) == "fireGun";
            });
        if (frostRay == race.weapons.end() ||
            frostRay->projectiles.size() != 1U ||
            frostRay->projectiles.front().type != 18U ||
            std::abs(
                frostRay->projectiles.front().minimumLife - 1.0F) >
                0.001F ||
            std::abs(
                frostRay->projectiles.front()
                        .tertiaryVisual.maximumTimeLife -
                1.0F) > 0.001F ||
            tankLaser == race.weapons.end() ||
            tankLaser->projectiles.size() != 1U ||
            tankLaser->projectiles.front().type != 3U ||
            std::abs(
                tankLaser->projectiles.front().minimumLife - 1.0F) >
                0.001F ||
            std::abs(tankLaser->shotDelay - 1.1F) > 0.001F ||
            fireGun == race.weapons.end() ||
            fireGun->projectiles.size() != 1U ||
            fireGun->projectiles.front().type != 14U ||
            std::abs(
                fireGun->projectiles.front().minimumLife - 1.6F) >
                0.001F)
        {
            throw std::runtime_error(
                "source Laser/FrostRay/Fire lifetimes were not preserved");
        }
        if (vehicles.size() > 1U)
        {
            OriginalRaceSession frostSession(race);
            PlayerProfile frostProfile;
            auto& slot = frostProfile.slots[
                PlayerProfile::firstWeaponSlot];
            slot.record = frostRay->record;
            slot.charge = 1U;
            slot.hasCharge = true;
            frostSession.applyPlayerProfile(frostProfile);
            auto frostVehicles = vehicles;
            for (std::size_t index = 0;
                 index < frostVehicles.size(); ++index)
            {
                frostVehicles[index].body.position = {
                    100000.0F + static_cast<float>(index) * 1000.0F,
                    100000.0F, 1000.0F};
                frostVehicles[index].body.rotation = {};
                frostVehicles[index].linearVelocity = {};
            }
            constexpr float sourcePitch = 0.25F;
            frostVehicles[0].body.rotation = {
                0.0F, std::sin(sourcePitch * 0.5F), 0.0F,
                std::cos(sourcePitch * 0.5F)};
            RaceControl frostInput;
            for (int frame = 0; frame < 190; ++frame)
            {
                frostSession.update(
                    1.0F / 60.0F, frostVehicles, frostInput);
            }
            frostInput.useWeapon = true;
            frostSession.update(
                1.0F / 60.0F, frostVehicles, frostInput);
            frostInput.useWeapon = false;
            const std::size_t frostWeapon =
                static_cast<std::size_t>(
                    frostRay - race.weapons.begin());
            const auto sourceRay = std::find_if(
                frostSession.projectiles().begin(),
                frostSession.projectiles().end(),
                [frostWeapon](
                    const ProjectileRuntime& projectile) {
                    return projectile.owner == 0U &&
                           projectile.weapon == frostWeapon &&
                           projectile.projectile == 0U &&
                           projectile.attached;
                });
            if (sourceRay == frostSession.projectiles().end() ||
                std::abs(sourceRay->lifeSeconds - 1.0F) > 0.001F ||
                std::abs(sourceRay->direction.z) < 0.05F)
            {
                throw std::runtime_error(
                    "source attached minTimeLife/full weapon direction "
                    "was not preserved");
            }
            const auto& targetDefinition =
                race.racers[1].hasConfiguredVehicle
                    ? race.racers[1].configuredVehicle
                    : race.vehicles.at(race.racers[1].vehicle);
            const Vec3 targetCenter = add(
                sourceRay->position,
                multiply(sourceRay->direction, 5.0F));
            frostVehicles[1].body.position = subtract(
                targetCenter,
                targetDefinition.physics.shapePosition);
            const float lifeBeforeFrost =
                frostSession.racers()[1].life;
            frostSession.update(
                1.0F / 60.0F, frostVehicles, frostInput);
            if (frostSession.racers()[1].life >= lifeBeforeFrost ||
                std::abs(
                    frostSession.racers()[1].slowSeconds - 1.0F) >
                    0.001F ||
                frostSession.racers()[1].slowWeapon != frostWeapon ||
                frostSession.racers()[1].slowProjectile != 0U)
            {
                throw std::runtime_error(
                    "source FrostRay SlowEffect child was not created");
            }
            const auto energyDamageCount = [&]() {
                return std::count_if(
                    frostSession.effects().begin(),
                    frostSession.effects().end(),
                    [](const RaceEffect& effect) {
                        return effect.kind ==
                                   RaceEventKind::VehicleEnergyDamage &&
                               effect.racer == 1U &&
                               std::abs(effect.totalSeconds - 0.5F) <
                                   0.001F;
                    });
            };
            if (energyDamageCount() != 1)
            {
                throw std::runtime_error(
                    "source dtEnergy DamageEffect was not created");
            }
            frostSession.update(
                1.0F / 60.0F, frostVehicles, frostInput);
            if (frostSession.racers()[1].slowSeconds >= 0.99F ||
                energyDamageCount() != 1)
            {
                throw std::runtime_error(
                    "source EventEffect lifetime was reset by every ray "
                    "contact");
            }
            frostVehicles[1].body.position = {
                200000.0F, 200000.0F, 2000.0F};
            for (int frame = 0; frame < 60; ++frame)
            {
                frostSession.update(
                    1.0F / 60.0F, frostVehicles, frostInput);
            }
            if (frostSession.racers()[1].slowSeconds > 0.0F ||
                frostSession.racers()[1].slowWeapon !=
                    RacerRuntime::invalidWeapon ||
                frostSession.racers()[1].slowProjectile !=
                    RacerRuntime::invalidWeapon ||
                energyDamageCount() != 0)
            {
                throw std::runtime_error(
                    "source Frost SlowEffect did not expire with its "
                    "model");
            }
        }

        const auto childDeathWeapon = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return recordName(weapon.record) == "bulletGun";
            });
        if (childDeathWeapon == race.weapons.end() ||
            childDeathWeapon->projectiles.empty() ||
            !childDeathWeapon->projectiles.front()
                 .deathEffect.targetChild)
        {
            throw std::runtime_error(
                "source projectile targetChild DeathEffect was not loaded");
        }
        if (vehicles.size() > 1U)
        {
            OriginalRaceSession childEffectSession(race);
            PlayerProfile childEffectProfile;
            auto& childEffectSlot = childEffectProfile.slots[
                PlayerProfile::firstWeaponSlot];
            childEffectSlot.record = childDeathWeapon->record;
            childEffectSlot.charge = 2U;
            childEffectSlot.hasCharge = true;
            childEffectSession.applyPlayerProfile(childEffectProfile);
            auto childEffectVehicles = vehicles;
            RaceControl childEffectInput;
            for (int frame = 0; frame < 190; ++frame)
            {
                childEffectSession.update(
                    1.0F / 60.0F, childEffectVehicles,
                    childEffectInput);
            }
            childEffectInput.useWeapon = true;
            childEffectSession.update(
                1.0F / 60.0F, childEffectVehicles,
                childEffectInput);
            childEffectInput.useWeapon = false;
            const std::size_t childEffectWeapon =
                static_cast<std::size_t>(
                    childDeathWeapon - race.weapons.begin());
            const auto launchedChildProjectile = std::find_if(
                childEffectSession.projectiles().begin(),
                childEffectSession.projectiles().end(),
                [childEffectWeapon](
                    const ProjectileRuntime& projectile) {
                    return projectile.owner == 0U &&
                           projectile.weapon == childEffectWeapon &&
                           projectile.projectile == 0U &&
                           projectile.active;
                });
            if (launchedChildProjectile ==
                childEffectSession.projectiles().end())
            {
                throw std::runtime_error(
                    "source targetChild test projectile was not launched");
            }
            const auto launched = *launchedChildProjectile;
            constexpr float contactStep = 1.0F / 600.0F;
            Vec3 nextVelocity = launched.velocity;
            if (launched.ballistic)
                nextVelocity.z -= 20.0F * contactStep;
            const Vec3 movement =
                launched.ballistic
                    ? multiply(nextVelocity, contactStep)
                    : multiply(
                          launched.direction,
                          std::max(launched.speed, 1.0F) *
                              contactStep);
            Transform expectedProjectile;
            expectedProjectile.position =
                add(launched.position, movement);
            expectedProjectile.rotation = launched.rotation;
            const auto projectileBox = orientedBox(
                expectedProjectile,
                childDeathWeapon->projectiles.front().collision);
            const auto& childTargetDefinition =
                race.racers[1].hasConfiguredVehicle
                    ? race.racers[1].configuredVehicle
                    : race.vehicles.at(race.racers[1].vehicle);
            childEffectVehicles[1].body.rotation = {};
            childEffectVehicles[1].body.position = subtract(
                projectileBox.center,
                childTargetDefinition.physics.shapePosition);
            childEffectSession.update(
                contactStep, childEffectVehicles, childEffectInput);
            const auto attachedDeath = std::find_if(
                childEffectSession.effects().begin(),
                childEffectSession.effects().end(),
                [childEffectWeapon](const RaceEffect& effect) {
                    return effect.kind ==
                               RaceEventKind::ProjectileImpact &&
                           effect.weapon == childEffectWeapon &&
                           effect.visualVariant == 3U;
                });
            if (attachedDeath == childEffectSession.effects().end() ||
                attachedDeath->parentRacer != 1U)
            {
                throw std::runtime_error(
                    "source DeathEffect targetChild did not attach to the "
                    "contacted car");
            }
            const Vec3 attachedWorld = compose(
                childEffectVehicles[1].body,
                attachedDeath->transform).position;
            if (length3(subtract(
                    attachedWorld, attachedDeath->origin)) > 0.001F)
            {
                throw std::runtime_error(
                    "source DeathEffect target-local transform mismatch");
            }
        }

        const auto mortar = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return recordName(weapon.record) == "mortira";
            });
        if (mortar == race.weapons.end() ||
            mortar->projectiles.empty() ||
            mortar->projectiles.front().type != 19U ||
            mortar->projectiles.front().deathEffect.visual.record.empty() ||
            !mortar->projectiles.front().deathEffect.ignoreRotation ||
            mortar->projectiles.front().deathProjectile ==
                ProjectileDefinition::invalidProjectile ||
            mortar->projectiles.front().deathProjectile >=
                mortar->projectiles.size())
        {
            throw std::runtime_error(
                "source mortar DeathEffect projectile was not loaded");
        }
        const auto craterIndex =
            mortar->projectiles.front().deathProjectile;
        const std::size_t mortarWeapon =
            static_cast<std::size_t>(
                mortar - race.weapons.begin());
        const auto& craterDefinition =
            mortar->projectiles[craterIndex];
        if (craterDefinition.type != 20U ||
            !craterDefinition.spawnOnParentDeath ||
            craterDefinition.minimumLife != 3.0F ||
            craterDefinition.damage != 10.0F)
        {
            throw std::runtime_error(
                "source ptCrater definition was not preserved");
        }
        if (vehicles.size() > 1U)
        {
            OriginalRaceSession mortarSession(race);
            auto mortarVehicles = vehicles;
            PlayerProfile mortarProfile;
            auto& mortarSlot = mortarProfile.slots[
                PlayerProfile::firstWeaponSlot];
            mortarSlot.record = mortar->record;
            mortarSlot.charge = 2U;
            mortarSlot.hasCharge = true;
            mortarSession.applyPlayerProfile(mortarProfile);
            RaceControl mortarInput;
            for (int frame = 0; frame < 190; ++frame)
                mortarSession.update(
                    1.0F / 60.0F, mortarVehicles, mortarInput);
            mortarInput.useWeapon = true;
            mortarSession.update(
                1.0F / 60.0F, mortarVehicles, mortarInput);
            mortarInput.useWeapon = false;
            const auto launchedProjectile = std::find_if(
                mortarSession.projectiles().begin(),
                mortarSession.projectiles().end(),
                [mortarWeapon](
                    const ProjectileRuntime& projectile) {
                    return projectile.owner == 0U &&
                           projectile.weapon == mortarWeapon &&
                           projectile.projectile == 0U &&
                           projectile.active;
                });
            if (launchedProjectile ==
                mortarSession.projectiles().end())
            {
                throw std::runtime_error(
                    "source mortar did not launch its live projectile");
            }
            const auto launched = *launchedProjectile;
            Vec3 nextVelocity = launched.velocity;
            nextVelocity.z -= 20.0F / 60.0F;
            const Vec3 nextProjectilePosition = add(
                launched.position,
                multiply(nextVelocity, 1.0F / 60.0F));
            const Vec3 nextProjectileCenter = add(
                nextProjectilePosition,
                rotate(
                    launched.rotation,
                    mortar->projectiles.front()
                        .collision.center));
            const auto& targetDefinition =
                race.racers[1].hasConfiguredVehicle
                    ? race.racers[1].configuredVehicle
                    : race.vehicles.at(race.racers[1].vehicle);
            mortarVehicles[1].body.rotation = {};
            mortarVehicles[1].body.position = subtract(
                nextProjectileCenter,
                targetDefinition.physics.shapePosition);
            Transform expectedProjectileTransform;
            expectedProjectileTransform.position =
                nextProjectilePosition;
            expectedProjectileTransform.rotation =
                launched.rotation;
            const bool expectedOverlap = boxesOverlap(
                orientedBox(
                    expectedProjectileTransform,
                    mortar->projectiles.front().collision),
                vehicleBox(
                    mortarVehicles[1], targetDefinition.physics));
            const float lifeBeforeCrater =
                mortarSession.racers()[1].life;
            mortarSession.update(
                1.0F / 60.0F, mortarVehicles, mortarInput);
            const auto crater = std::find_if(
                mortarSession.mines().begin(),
                mortarSession.mines().end(),
                [](const MineRuntime& mine) {
                    return mine.type == 20U;
                });
            if (crater == mortarSession.mines().end() ||
                crater->projectile != craterIndex ||
                !crater->ignoreOwnerCollision ||
                std::abs(crater->maximumLife - 3.0F) > 0.001F ||
                std::abs(crater->collision.halfExtents.x - 3.0F) >
                    0.001F ||
                std::abs(crater->collision.halfExtents.y - 3.0F) >
                    0.001F ||
                std::abs(crater->collision.halfExtents.z - 0.05F) >
                    0.001F ||
                mortarSession.racers()[1].life >= lifeBeforeCrater)
            {
                throw std::runtime_error(
                    "source mortar ptCrater contact field was not spawned: "
                    "mines=" +
                    std::to_string(mortarSession.mines().size()) +
                    ", projectiles=" +
                    std::to_string(
                        mortarSession.projectiles().size()) +
                    ", expectedOverlap=" +
                    std::string(
                        expectedOverlap ? "true" : "false") +
                    ", projectile=" +
                    (crater == mortarSession.mines().end()
                         ? std::string("missing")
                         : std::to_string(crater->projectile)) +
                    ", lifeBefore=" +
                    std::to_string(lifeBeforeCrater) +
                    ", lifeAfter=" +
                    std::to_string(
                        mortarSession.racers()[1].life));
            }
            const bool hasSourceMortarDeath = std::any_of(
                mortarSession.effects().begin(),
                mortarSession.effects().end(),
                [mortarWeapon](const RaceEffect& effect) {
                    return effect.kind == RaceEventKind::ProjectileImpact &&
                           effect.weapon == mortarWeapon &&
                           effect.visualVariant == 3U &&
                           effect.ignoreRotation;
                });
            if (!hasSourceMortarDeath)
            {
                throw std::runtime_error(
                    "source mortar DeathEffect transform was not emitted");
            }
            const float firstCraterLife =
                mortarSession.racers()[1].life;
            const float ownerLifeBeforeCrater =
                mortarSession.racers()[0].life;
            mortarVehicles[0].body = mortarVehicles[1].body;
            mortarSession.update(
                1.0F / 60.0F, mortarVehicles, mortarInput);
            if (mortarSession.mines().empty() ||
                mortarSession.racers()[1].life >= firstCraterLife ||
                std::abs(
                    mortarSession.racers()[0].life -
                    ownerLifeBeforeCrater) > 0.001F)
            {
                throw std::runtime_error(
                    "source ptCrater continuous damage/owner ignore-pair "
                    "mismatch");
            }
        }

        const auto mineRip = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return recordName(weapon.record) == "mine2";
            });
        if (mineRip == race.weapons.end() ||
            mineRip->projectiles.empty() ||
            mineRip->projectiles.front().type != 12U ||
            mineRip->projectiles.front().deathEffect.visual.record.empty() ||
            !mineRip->projectiles.front().deathEffect.ignoreRotation ||
            mineRip->projectiles.front()
                    .secondaryCollision.halfExtents.x <= 0.0F ||
            mineRip->projectiles.front()
                    .secondaryCollision.halfExtents.y <= 0.0F ||
            mineRip->projectiles.front()
                    .tertiaryCollision.halfExtents.x <= 0.0F ||
            mineRip->projectiles.front()
                    .tertiaryCollision.halfExtents.y <= 0.0F ||
            !mineRip->projectiles.front()
                 .secondaryProjectile.valid ||
            mineRip->projectiles.front()
                    .secondaryProjectile.type != 11U ||
            std::abs(
                mineRip->projectiles.front()
                    .secondaryProjectile.damage -
                10.0F) > 0.001F ||
            std::abs(
                mineRip->projectiles.front()
                    .secondaryProjectile.speed -
                3000.0F) > 0.001F ||
            std::abs(
                mineRip->projectiles.front()
                    .secondaryProjectile.minimumLife -
                4.0F) > 0.001F ||
            std::abs(
                mineRip->projectiles.front()
                    .secondaryProjectile.maximumLife -
                4.5F) > 0.001F ||
            mineRip->projectiles.front()
                .secondaryProjectile.deathEffect.visual.record.empty() ||
            !mineRip->projectiles.front()
                 .tertiaryProjectile.valid ||
            mineRip->projectiles.front()
                    .tertiaryProjectile.type != 13U ||
            std::abs(
                mineRip->projectiles.front()
                    .tertiaryProjectile.damage -
                4.0F) > 0.001F ||
            std::abs(
                mineRip->projectiles.front()
                    .tertiaryProjectile.speed -
                3000.0F) > 0.001F ||
            std::abs(
                mineRip->projectiles.front()
                    .tertiaryProjectile.minimumLife -
                4.0F) > 0.001F ||
            std::abs(
                mineRip->projectiles.front()
                    .tertiaryProjectile.maximumLife -
                4.5F) > 0.001F ||
            mineRip->projectiles.front()
                .tertiaryProjectile.deathEffect.visual.record.empty() ||
            std::abs(
                mineRip->projectiles.front().deathEffect.position.z -
                0.5F) > 0.001F)
        {
            throw std::runtime_error(
                "source MineRip DeathEffect metadata was not preserved");
        }
        {
            OriginalRaceSession mineSession(race);
            auto mineVehicles = vehicles;
            for (std::size_t racer = 1U;
                 racer < mineVehicles.size(); ++racer)
            {
                mineVehicles[racer].body.position.x +=
                    1000.0F + 100.0F *
                        static_cast<float>(racer);
                mineVehicles[racer].body.position.y +=
                    1000.0F;
            }
            PlayerProfile mineProfile;
            auto& mineSlot =
                mineProfile.slots[PlayerProfile::mineSlot];
            mineSlot.record = mineRip->record;
            mineSlot.charge = 1U;
            mineSlot.hasCharge = true;
            mineSession.applyPlayerProfile(mineProfile);
            RaceControl mineInput;
            for (int frame = 0; frame < 190; ++frame)
                mineSession.update(
                    1.0F / 60.0F, mineVehicles, mineInput);
            mineInput.useMine = true;
            mineSession.update(
                1.0F / 60.0F, mineVehicles, mineInput);
            mineInput.useMine = false;
            if (mineSession.mines().empty())
                throw std::runtime_error("source MineRip was not placed");
            const auto& sourceProjectile =
                mineRip->projectiles.front();
            const Transform sourceWeaponTransform = compose(
                mineVehicles[0].body, mineRip->visual.transform);
            Transform sourceProjectileTransform;
            sourceProjectileTransform.position =
                sourceProjectile.position;
            sourceProjectileTransform.rotation =
                sourceProjectile.rotation;
            const Vec3 sourceRayPosition =
                compose(sourceWeaponTransform,
                        sourceProjectileTransform).position;
            const auto sourceHit = raycastTrackPlane(
                race,
                add(sourceRayPosition, {0.0F, 0.0F, 2.0F}));
            const float sourceOffset = std::max(
                -(sourceProjectile.collision.center.z -
                  sourceProjectile.collision.halfExtents.z),
                0.01F);
            const Vec3 expectedMinePosition =
                add(sourceHit.position,
                    {0.0F, 0.0F, sourceOffset});
            const auto& placedMine = mineSession.mines().front();
            const Vec3 placedUp = normalized3(
                rotate(placedMine.rotation,
                       {0.0F, 0.0F, 1.0F}));
            if (!sourceHit.hit ||
                distanceSquared(
                    placedMine.position, expectedMinePosition) >
                    0.000001F ||
                dot3(placedUp, sourceHit.normal) < 0.999F)
            {
                throw std::runtime_error(
                    "source MinePrepare track raycast transform was not "
                    "preserved");
            }
            const Vec3 minePosition = mineSession.mines().front().position;
            mineVehicles[0].body.position = minePosition;
            for (int frame = 0; frame < 30; ++frame)
                mineSession.update(
                    1.0F / 60.0F, mineVehicles, mineInput);
            const std::size_t mineWeapon =
                static_cast<std::size_t>(mineRip - race.weapons.begin());
            const bool hasSourceMineDeath = std::any_of(
                mineSession.effects().begin(),
                mineSession.effects().end(),
                [mineWeapon, minePosition](const RaceEffect& effect) {
                    return effect.kind == RaceEventKind::ProjectileImpact &&
                           effect.weapon == mineWeapon &&
                           effect.visualVariant == 3U &&
                           effect.ignoreRotation &&
                           std::abs(effect.origin.z -
                                    (minePosition.z + 0.5F)) < 0.001F;
                });
            if (!mineSession.mines().empty() || !hasSourceMineDeath)
            {
                throw std::runtime_error(
                    "source MineRip contact DeathEffect was not emitted");
            }
        }
        {
            OriginalRaceSession rejectedMineSession(race);
            PlayerProfile mineProfile;
            auto& mineSlot =
                mineProfile.slots[PlayerProfile::mineSlot];
            mineSlot.record = mineRip->record;
            mineSlot.charge = 1U;
            mineSlot.hasCharge = true;
            rejectedMineSession.applyPlayerProfile(mineProfile);
            auto outsideVehicles = vehicles;
            outsideVehicles[0].body.position = {
                100000.0F, 100000.0F, 10.0F};
            RaceControl mineInput;
            for (int frame = 0; frame < 190; ++frame)
            {
                rejectedMineSession.update(
                    1.0F / 60.0F, outsideVehicles, mineInput);
            }
            mineInput.useMine = true;
            rejectedMineSession.update(
                1.0F / 60.0F, outsideVehicles, mineInput);
            if (!rejectedMineSession.mines().empty() ||
                rejectedMineSession.racers().front().mines != 1U)
            {
                throw std::runtime_error(
                    "source MinePrepare failed raycast consumed a mine");
            }
        }

        const auto mineSpike = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return recordName(weapon.record) == "mine1";
            });
        const auto mineProton = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return recordName(weapon.record) == "mine3";
            });
        const auto oilMine = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return recordName(weapon.record) == "maslo";
            });
        if (mineSpike == race.weapons.end() ||
            mineSpike->projectiles.empty() ||
            mineSpike->projectiles.front().type != 11U ||
            std::abs(
                mineSpike->projectiles.front().speed -
                3000.0F) > 0.001F ||
            mineProton == race.weapons.end() ||
            mineProton->projectiles.empty() ||
            mineProton->projectiles.front().type != 24U ||
            std::abs(
                mineProton->projectiles.front().speed -
                3000.0F) > 0.001F ||
            oilMine == race.weapons.end() ||
            oilMine->projectiles.empty() ||
            oilMine->projectiles.front().type != 10U)
        {
            throw std::runtime_error(
                "source mine/oil workshop records were not preserved");
        }
        auto isolatedMineVehicles = [&]() {
            auto result = vehicles;
            for (std::size_t racer = 1U;
                 racer < result.size(); ++racer)
            {
                result[racer].body.position.x +=
                    1000.0F + 100.0F *
                        static_cast<float>(racer);
                result[racer].body.position.y += 1000.0F;
                result[racer].linearVelocity = {};
                result[racer].speed = 0.0F;
            }
            return result;
        };
        auto mineProfileFor =
            [](const WeaponDefinition& weapon) {
                PlayerProfile profile;
                auto& slot =
                    profile.slots[PlayerProfile::mineSlot];
                slot.record = weapon.record;
                slot.charge = 1U;
                slot.hasCharge = true;
                return profile;
            };
        auto advanceMineCountdown =
            [](OriginalRaceSession& sourceSession,
               std::vector<r3d::physics::VehicleState>&
                   sourceVehicles) {
                RaceControl sourceInput;
                for (int frame = 0; frame < 190; ++frame)
                {
                    sourceSession.update(
                        1.0F / 60.0F, sourceVehicles,
                        sourceInput);
                }
            };
        auto expectedMineTransform =
            [&](const WeaponDefinition& weapon,
                const std::vector<
                    r3d::physics::VehicleState>&
                    sourceVehicles) {
                const auto& projectile =
                    weapon.projectiles.front();
                const Transform weaponTransform = compose(
                    sourceVehicles[0].body,
                    weapon.visual.transform);
                Transform projectileTransform;
                projectileTransform.position =
                    projectile.position;
                projectileTransform.rotation =
                    projectile.rotation;
                const Vec3 rayPosition = compose(
                    weaponTransform,
                    projectileTransform).position;
                const auto hit = raycastTrackPlane(
                    race,
                    add(rayPosition, {0.0F, 0.0F, 2.0F}));
                if (!hit.hit)
                {
                    throw std::runtime_error(
                        "source mine regression ray missed track");
                }
                const float offset = std::max(
                    -(projectile.collision.center.z -
                      projectile.collision.halfExtents.z),
                    0.01F);
                Transform result;
                result.position = add(
                    hit.position, {0.0F, 0.0F, offset});
                result.rotation = rotationWithUp(hit.normal);
                return result;
            };

        {
            OriginalRaceSession contactSession(race);
            contactSession.applyPlayerProfile(
                mineProfileFor(*mineSpike));
            auto sourceVehicles = isolatedMineVehicles();
            advanceMineCountdown(
                contactSession, sourceVehicles);
            const Transform mineTransform =
                expectedMineTransform(
                    *mineSpike, sourceVehicles);
            const auto& targetRacer = race.racers[1];
            const auto& targetDefinition =
                targetRacer.hasConfiguredVehicle
                    ? targetRacer.configuredVehicle
                    : race.vehicles.at(targetRacer.vehicle);
            const Vec3 mineCenter = add(
                mineTransform.position,
                rotate(
                    mineTransform.rotation,
                    mineSpike->projectiles.front()
                        .collision.center));
            sourceVehicles[1].body.rotation = {};
            sourceVehicles[1].body.position = subtract(
                add(mineCenter, {0.0F, 0.25F, 0.0F}),
                targetDefinition.physics.shapePosition);
            const float lifeBefore =
                contactSession.racers()[1].life;
            RaceControl sourceInput;
            sourceInput.useMine = true;
            contactSession.update(
                1.0F / 60.0F, sourceVehicles, sourceInput);
            const auto impulses =
                contactSession.takeVelocityRequests();
            const bool hasVerticalImpulse = std::any_of(
                impulses.begin(), impulses.end(),
                [](const VelocityRequest& request) {
                    return request.racer == 1U &&
                           request.delta.z > 0.0F;
                });
            if (!contactSession.mines().empty() ||
                contactSession.racers()[1].life >=
                    lifeBefore ||
                !hasVerticalImpulse ||
                contactSession.racers()[0]
                        .mineLockSeconds <= 0.0F)
            {
                throw std::runtime_error(
                    "source MineContact early non-owner contact/impulse "
                    "was not preserved");
            }
        }

        {
            OriginalRaceSession protonSession(race);
            protonSession.applyPlayerProfile(
                mineProfileFor(*mineProton));
            auto sourceVehicles = isolatedMineVehicles();
            advanceMineCountdown(
                protonSession, sourceVehicles);
            const float lifeBefore =
                protonSession.racers()[0].life;
            RaceControl sourceInput;
            sourceInput.useMine = true;
            protonSession.update(
                1.0F / 60.0F, sourceVehicles, sourceInput);
            sourceInput.useMine = false;
            if (protonSession.mines().size() != 1U ||
                protonSession.mines().front().type != 24U ||
                protonSession.racers()[0].life != lifeBefore)
            {
                throw std::runtime_error(
                    "source ptMineProton owner arming window failed");
            }
            for (int frame = 0; frame < 30; ++frame)
            {
                protonSession.update(
                    1.0F / 60.0F, sourceVehicles,
                    sourceInput);
            }
            if (!protonSession.mines().empty() ||
                protonSession.racers()[0].life >= lifeBefore)
            {
                throw std::runtime_error(
                    "source ptMineProton contact was not dispatched");
            }
        }

        {
            OriginalRaceSession oilSession(race);
            oilSession.applyPlayerProfile(
                mineProfileFor(*oilMine));
            auto sourceVehicles = isolatedMineVehicles();
            sourceVehicles[0].linearVelocity =
                {10.0F, 0.0F, 0.0F};
            sourceVehicles[0].speed = 10.0F;
            advanceMineCountdown(
                oilSession, sourceVehicles);
            RaceControl sourceInput;
            sourceInput.useMine = true;
            oilSession.update(
                1.0F / 60.0F, sourceVehicles, sourceInput);
            sourceInput.useMine = false;
            const auto earlyAngular =
                oilSession.takeAngularVelocityRequests();
            for (int frame = 0; frame < 30; ++frame)
            {
                oilSession.update(
                    1.0F / 60.0F, sourceVehicles,
                    sourceInput);
            }
            const auto armedAngular =
                oilSession.takeAngularVelocityRequests();
            const bool oilLockedClutch = std::any_of(
                armedAngular.begin(), armedAngular.end(),
                [](const AngularVelocityRequest& request) {
                    return request.racer == 0U &&
                           std::abs(request.delta.z) > 0.0F;
                });
            if (!earlyAngular.empty() ||
                oilSession.mines().size() != 1U ||
                !oilLockedClutch ||
                oilSession.racers()[0].clutchSeconds <= 0.0F)
            {
                throw std::runtime_error(
                    "source Maslo arming/mine-lock/clutch lifecycle "
                    "failed");
            }
        }

        {
            OriginalRaceSession splitSession(race);
            splitSession.applyPlayerProfile(
                mineProfileFor(*mineRip));
            auto sourceVehicles = isolatedMineVehicles();
            advanceMineCountdown(
                splitSession, sourceVehicles);
            RaceControl sourceInput;
            sourceInput.useMine = true;
            splitSession.update(
                1.0F / 60.0F, sourceVehicles, sourceInput);
            sourceInput.useMine = false;
            if (splitSession.mines().size() != 1U)
            {
                throw std::runtime_error(
                    "source MineRip split regression was not placed");
            }
            sourceVehicles[0].body.position.x += 1000.0F;
            sourceVehicles[0].body.position.y += 1000.0F;
            bool splitObserved = false;
            for (int frame = 0; frame < 130; ++frame)
            {
                splitSession.update(
                    1.0F / 60.0F, sourceVehicles,
                    sourceInput);
                const auto coreCount = std::count_if(
                    splitSession.mines().begin(),
                    splitSession.mines().end(),
                    [](const MineRuntime& mine) {
                        return mine.visualVariant == 1U;
                    });
                const auto pieceCount = std::count_if(
                    splitSession.mines().begin(),
                    splitSession.mines().end(),
                    [](const MineRuntime& mine) {
                        return mine.visualVariant == 2U;
                    });
                if (coreCount == 1 && pieceCount == 5)
                {
                    splitObserved = true;
                    break;
                }
            }
            bool sourceFragments = splitObserved;
            for (const auto& mine : splitSession.mines())
            {
                if (mine.visualVariant == 1U)
                {
                    sourceFragments =
                        sourceFragments &&
                        mine.owner ==
                            RacerRuntime::invalidWeapon &&
                        !mine.linkedToOwner &&
                        mine.type == 11U &&
                        std::abs(mine.damage - 10.0F) <
                            0.001F &&
                        std::abs(
                            mine.impulseSpeed - 3000.0F) <
                            0.001F &&
                        mine.maximumLife >= 4.0F &&
                        mine.maximumLife <= 4.5F;
                }
                else if (mine.visualVariant == 2U)
                {
                    sourceFragments =
                        sourceFragments &&
                        mine.owner ==
                            RacerRuntime::invalidWeapon &&
                        !mine.linkedToOwner &&
                        mine.type == 13U &&
                        std::abs(mine.damage - 4.0F) <
                            0.001F &&
                        std::abs(
                            mine.impulseSpeed - 3000.0F) <
                            0.001F &&
                        mine.maximumLife >= 4.0F &&
                        mine.maximumLife <= 4.5F &&
                        std::abs(
                            length3(mine.velocity) - 10.0F) <
                            0.01F &&
                        mine.velocity.z > 0.0F;
                }
            }
            if (!sourceFragments)
            {
                throw std::runtime_error(
                    "source MineRip nested projectile values/impulses "
                    "were not preserved");
            }
            bool secondaryDeath = false;
            bool tertiaryDeath = false;
            for (int frame = 0; frame < 310; ++frame)
            {
                splitSession.update(
                    1.0F / 60.0F, sourceVehicles,
                    sourceInput);
                for (const auto& effect :
                     splitSession.effects())
                {
                    secondaryDeath =
                        secondaryDeath ||
                        (effect.kind ==
                             RaceEventKind::ProjectileImpact &&
                         effect.visualVariant == 5U);
                    tertiaryDeath =
                        tertiaryDeath ||
                        (effect.kind ==
                             RaceEventKind::ProjectileImpact &&
                         effect.visualVariant == 6U);
                }
            }
            if (!splitSession.mines().empty() ||
                !secondaryDeath || !tertiaryDeath)
            {
                throw std::runtime_error(
                    "source MineRip nested minTimeLife/DeathEffect "
                    "lifecycle failed");
            }
        }

        {
            std::size_t resetNode = 0U;
            float resetSegmentLength = 0.0F;
            for (std::size_t node = 0U;
                 node + 1U < race.tracePath.size(); ++node)
            {
                const float length = length2(subtract(
                    point(node + 1U).position,
                    point(node).position));
                if (length > 20.0F)
                {
                    resetNode = node;
                    resetSegmentLength = length;
                    break;
                }
            }
            if (resetSegmentLength <= 20.0F)
            {
                throw std::runtime_error(
                    "source trace has no usable ResetCar regression "
                    "segment");
            }
            OriginalRaceSession resetSession(race);
            auto resetVehicles = vehicles;
            RaceControl resetInput;
            for (int frame = 0; frame < 190; ++frame)
            {
                resetSession.update(
                    1.0F / 60.0F, resetVehicles, resetInput);
            }
            for (std::size_t node = 1U; node <= resetNode; ++node)
            {
                resetVehicles[0].body.position =
                    point(node).position;
                resetVehicles[0].body.position.z += 2.0F;
                resetVehicles[0].body.rotation =
                    shortestArcFromX(normalized2(subtract(
                        point(node).position,
                        point(node - 1U).position)));
                resetVehicles[0].speed = 5.0F;
                resetSession.update(
                    1.0F / 60.0F, resetVehicles, resetInput);
            }
            constexpr float resetCoordinate = 0.75F;
            const auto& resetStart = point(resetNode);
            const auto& resetEnd = point(resetNode + 1U);
            const Vec3 resetSegment =
                subtract(resetEnd.position, resetStart.position);
            const Vec3 resetDirection =
                normalized2(resetSegment);
            resetVehicles[0].body.position = add(
                resetStart.position,
                multiply(resetSegment, resetCoordinate));
            resetVehicles[0].body.position.z += 2.0F;
            resetVehicles[0].body.rotation =
                shortestArcFromX(resetDirection);
            resetVehicles[0].speed = 5.0F;
            resetInput.reset = true;
            resetSession.update(
                1.0F / 60.0F, resetVehicles, resetInput);
            const auto resetRequests =
                resetSession.takeRespawns();
            if (resetRequests.size() != 1U)
            {
                throw std::runtime_error(
                    "source ResetCar request count failed");
            }
            const float resetDistance = dot2(
                subtract(
                    resetRequests.front().position,
                    resetStart.position),
                resetDirection);
            if (std::abs(
                    resetDistance -
                    resetSegmentLength * resetCoordinate) >
                    0.25F ||
                dot2(
                    resetRequests.front().direction,
                    resetDirection) <
                    0.999F)
            {
                throw std::runtime_error(
                    "source ResetCar retained tile coordinate/direction "
                    "failed");
            }
        }

        input.reset = true;
        session.update(1.0F / 60.0F, vehicles, input);
        if (session.takeRespawns().empty())
            throw std::runtime_error("reset/respawn transition failed");
        error.clear();
        return true;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return false;
    }
}

} // namespace r3d::game::originalrace
