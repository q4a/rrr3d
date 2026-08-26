#include "OriginalRaceSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <numeric>
#include <optional>
#include <stdexcept>

namespace r3d::game::originalrace
{
namespace
{

const ProjectileDefinition* runtimeProjectileDefinition(
    const Race& race, const ProjectileRuntime& projectile) noexcept
{
    if (projectile.weaponDescription != nullptr &&
        projectile.descriptionProjectile <
            projectile.weaponDescription->projectiles.size())
    {
        return &projectile.weaponDescription
                    ->projectiles[projectile.descriptionProjectile];
    }
    if (projectile.weapon >= race.weapons.size() ||
        projectile.projectile >=
            race.weapons[projectile.weapon].projectiles.size())
        return nullptr;
    return &race.weapons[projectile.weapon]
                .projectiles[projectile.projectile];
}

const ProjectileDefinition* runtimeProjectileDefinition(
    const Race& race, const MineRuntime& mine) noexcept
{
    if (mine.weaponDescription != nullptr &&
        mine.descriptionProjectile <
            mine.weaponDescription->projectiles.size())
    {
        return &mine.weaponDescription
                    ->projectiles[mine.descriptionProjectile];
    }
    if (mine.weapon >= race.weapons.size() ||
        mine.projectile >= race.weapons[mine.weapon].projectiles.size())
        return nullptr;
    return &race.weapons[mine.weapon].projectiles[mine.projectile];
}

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

source::Proj::Vec3 sourceVec(Vec3 value)
{
    return {value.x, value.y, value.z};
}

Vec3 runtimeVec(source::Proj::Vec3 value)
{
    return {value.x, value.y, value.z};
}

source::Proj::Quat sourceQuat(Quat value)
{
    return {value.x, value.y, value.z, value.w};
}

Quat runtimeQuat(source::Proj::Quat value)
{
    return {value.x, value.y, value.z, value.w};
}

void applySourceProxyTransform(
    source::GameObject& object, const Transform& transform) noexcept
{
    object.SetPos({
        transform.position.x, transform.position.y,
        transform.position.z});
    object.SetScale({
        transform.scale.x, transform.scale.y,
        transform.scale.z});
    object.SetRot({
        transform.rotation.x, transform.rotation.y,
        transform.rotation.z, transform.rotation.w});
}

struct EffectTiming
{
    float emissionSeconds = 0.0F;
    float visibleSeconds = 0.0F;
    bool waitForParticleEnd = false;
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
        result.waitForParticleEnd = true;
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

void applySourceEffectTiming(RaceEffect& effect,
                             const EffectTiming& timing)
{
    effect.totalSeconds = timing.visibleSeconds;
    effect.seconds = timing.visibleSeconds;
    effect.emissionEndSeconds = timing.emissionSeconds;
    effect.waitForParticleEnd = timing.waitForParticleEnd;
    effect.effectOwner.ResetGameObject(-1.0F);
    effect.waitingEnd.Reset();
}

void attachSourceLifeEffect(
    RaceEffect& effect, const std::vector<std::string>& sounds,
    std::size_t racer = RacerRuntime::invalidWeapon,
    std::size_t followRacer = RacerRuntime::invalidWeapon)
{
    effect.lifeEffect.Reset();
    effect.lifeSoundPaths = sounds;
    effect.lifeSoundRacer = racer;
    effect.lifeSoundFollowRacer = followRacer;
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

Transform relativeTransform(
    const Transform& parent, const Transform& world)
{
    const Quat inverseRotation{
        -parent.rotation.x, -parent.rotation.y,
        -parent.rotation.z, parent.rotation.w};
    const Vec3 unscaled = rotate(
        inverseRotation, subtract(world.position, parent.position));
    const auto divide = [](float value, float scale) {
        return std::abs(scale) > 0.000001F ? value / scale : value;
    };
    Transform result;
    result.position = {
        divide(unscaled.x, parent.scale.x),
        divide(unscaled.y, parent.scale.y),
        divide(unscaled.z, parent.scale.z)};
    result.scale = {
        divide(world.scale.x, parent.scale.x),
        divide(world.scale.y, parent.scale.y),
        divide(world.scale.z, parent.scale.z)};
    result.rotation = multiply(inverseRotation, world.rotation);
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
    std::size_t decoration = RacerRuntime::invalidWeapon;
    bool hit = false;
};

WorldRayHit raycastWorld(
    const Race& race,
    const std::vector<bool>& decorationActive,
    const std::vector<r3d::physics::VehicleState>& vehicles,
    const std::vector<RacerRuntime>& racers, std::size_t ignoredVehicle,
    Vec3 origin, Vec3 direction, float maximumDistance)
{
    WorldRayHit result;
    result.distance = maximumDistance;
    direction = normalized3(direction);
    for (std::size_t meshIndex = 0U;
         meshIndex < race.collisionMeshes.size(); ++meshIndex)
    {
        const auto& mesh = race.collisionMeshes[meshIndex];
        const std::size_t decoration =
            meshIndex < race.collisionMeshDecorationInstances.size()
                ? race.collisionMeshDecorationInstances[meshIndex]
                : RacerRuntime::invalidWeapon;
        // DestrObj::OnDeath removes its PhysX actor.  Keeping its old triangle
        // mesh in the ray query made Laser/FrostRay stop on invisible debris.
        if (decoration != RacerRuntime::invalidWeapon &&
            decoration < decorationActive.size() &&
            !decorationActive[decoration])
        {
            continue;
        }
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
            result.decoration = decoration;
        }
    }
    for (std::size_t vehicle = 0;
         vehicle < vehicles.size() && vehicle < racers.size(); ++vehicle)
    {
        if (vehicle == ignoredVehicle || racers[vehicle].destroyed)
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
        result.decoration = RacerRuntime::invalidWeapon;
    }
    return result;
}

struct ResetRayHit
{
    source::ResetCarRayKind kind = source::ResetCarRayKind::None;
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
        result.kind = source::ResetCarRayKind::DeathPlane;
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
                    ? source::ResetCarRayKind::TrackPlane
                    : source::ResetCarRayKind::Blocked;
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
            vehicle == ownVehicle ? source::ResetCarRayKind::OwnCar
                                  : source::ResetCarRayKind::Blocked;
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

OriginalRaceSession::OriginalRaceSession(
    const Race& race, bool legacyWindowsDebug)
    : legacyWindowsDebug_(legacyWindowsDebug), race_(race), map_(&logic_)
{
    if (race_.tracePath.size() < 2 || race_.tracePoints.empty() ||
        race_.racers.empty())
        throw std::invalid_argument("Original race session data is incomplete");
    buildSourceTrace();
    reset();
}

source::MapObjects& OriginalRaceSession::decorationObjects() noexcept
{
    return map_.GetMapObjList(source::MapObjCategory::Decoration);
}

source::MapObjects& OriginalRaceSession::bonusObjects() noexcept
{
    return map_.GetMapObjList(source::MapObjCategory::Bonus);
}

void OriginalRaceSession::reset()
{
    gameModeRaceState_.Reset(legacyWindowsDebug_);
    // DEBUG_PX makes GameMode::DoStartRace call GoRace(cGoRace) directly,
    // bypassing cGoRaceWait and the three visible countdown stages.
    phase_ = gameModeRaceState_.IsRaceGo() ? RacePhase::Racing
                                          : RacePhase::Countdown;
    phaseBeforePause_ = phase_;
    networkFinishControlled_ = false;
    networkGameplayEnabled_ = false;
    networkGameplayHost_ = false;
    debugHumanAiControl_ = false;
    humanRacer_ = RacerRuntime::invalidWeapon;
    networkOwnedRacers_.clear();
    elapsedSeconds_ = 0.0F;
    // AIPlayer::~AIPlayer writes the source-owned cheat flag back to its
    // Player. Release these owners before replacing the Player vector.
    aiPlayers_.clear();
    racers_.clear();
    racers_.resize(race_.racers.size());
    racerMapObjects_.assign(race_.racers.size(), nullptr);
    vehicleInputs_.assign(race_.racers.size(), {});
    humanPlayer_.SetCurWeapon(0);
    aiPlayers_.reserve(race_.racers.size());
    aiSystem_.Reset(map_.GetTrace().GetTrackCount());
    aiSystemEntriesScratch_.clear();
    aiSystemEntriesScratch_.reserve(race_.racers.size());
    aiAttackTargetsScratch_.assign(race_.racers.size(), {});
    previousPositions_.assign(race_.racers.size(), {});
    racePlaceModel_.Reset();
    map_.Clear();
    decorationActive_.assign(race_.decorationInstances.size(), true);
    decorationLife_.clear();
    decorationLife_.reserve(race_.decorationInstances.size());
    decorationObjects().Reserve(race_.decorationInstances.size());
    for (std::size_t index = 0U;
         index < race_.decorationInstances.size(); ++index)
    {
        const auto& instance = race_.decorationInstances[index];
        const auto& definition =
            race_.decorationDefinitions.at(instance.definition);
        auto& mapObject = map_.AddMapObj(
            source::MapObjCategory::Decoration,
            source::GameObjType::DestrObj, definition.record,
            instance.mapObjectId, index);
        auto* object = mapObject.GetDestrObj();
        applySourceProxyTransform(*object, instance.transform);
        object->GetDestrList().Reserve(
            definition.destructionPieces.size());
        for (const auto& piece : definition.destructionPieces)
        {
            auto& fragment = object->GetDestrList().Add(
                source::GameObjType::GameObj, "obj");
            applySourceProxyTransform(
                fragment.GetGameObj(), piece.transform);
            fragment.GetGameObj().ResetGameObject(-1.0F);
        }
        object->ResetGameObject(
            definition.maximumLife >= 0.0F
                ? definition.maximumLife
                : -1.0F);
        if (instance.hasProxyState)
        {
            object->SetLife(instance.life);
            object->SetMaxTimeLife(instance.maximumTimeLife);
            object->SetTimeLife(instance.timeLife);
        }
        if (!instance.name.empty())
            mapObject.SetName(instance.name);
        decorationLife_.push_back(object->GetLife());
    }
    auto& trackObjects =
        map_.GetMapObjList(source::MapObjCategory::Track);
    trackObjects.Reserve(race_.trackInstances.size());
    for (std::size_t index = 0U;
         index < race_.trackInstances.size(); ++index)
    {
        const auto& instance = race_.trackInstances[index];
        const auto& definition =
            race_.trackDefinitions.at(instance.definition);
        auto& mapObject = map_.AddMapObj(
            source::MapObjCategory::Track,
            source::GameObjType::GameObj, definition.record,
            instance.mapObjectId, index);
        auto& object = mapObject.GetGameObj();
        applySourceProxyTransform(object, instance.transform);
        object.ResetGameObject(-1.0F);
        if (instance.hasProxyState)
        {
            object.SetLife(instance.life);
            object.SetMaxTimeLife(instance.maximumTimeLife);
            object.SetTimeLife(instance.timeLife);
        }
        if (!instance.name.empty())
            mapObject.SetName(instance.name);
    }
    bonusActive_.assign(race_.bonuses.size(), true);
    bonusObjects().Reserve(race_.bonuses.size());
    bonusScales_.assign(race_.bonuses.size(), -1.0F);
    for (std::size_t index = 0U; index < race_.bonuses.size(); ++index)
    {
        const auto& bonus = race_.bonuses[index];
        auto& mapObject = map_.AddMapObj(
            source::MapObjCategory::Bonus,
            source::GameObjType::Proj, bonus.record,
            bonus.mapObjectId, index);
        auto& object = mapObject.GetGameObj();
        applySourceProxyTransform(object, bonus.transform);
        object.ResetGameObject(-1.0F);
        if (bonus.hasProxyState)
        {
            object.SetLife(bonus.life);
            object.SetMaxTimeLife(bonus.maximumTimeLife);
            object.SetTimeLife(bonus.timeLife);
        }
        if (!bonus.name.empty())
            mapObject.SetName(bonus.name);
        auto* projectile = mapObject.GetAutoProj();
        projectile->Reset(bonus.projectileType);
        bonusScales_[index] = projectile->GetModelScale();
    }
    bonusNetworkPendingContact_.assign(
        race_.bonuses.size(), RacerRuntime::invalidWeapon);
    events_.clear();
    effects_.clear();
    logic_.SetTouchBorderDamage(race_.touchBorderDamage);
    logic_.SetTouchBorderDamageForce(race_.touchBorderDamageForce);
    logic_.SetTouchCarDamage(race_.touchCarDamage);
    logic_.SetTouchCarDamageForce(race_.touchCarDamageForce);
    logic_.ResetContactBehavior(race_.contactSoundPaths.size());
    mines_.clear();
    projectiles_.clear();
    respawns_.clear();
    velocityRequests_.clear();
    angularVelocityRequests_.clear();
    angularMomentumRequests_.clear();
    pendingNetworkShots_.clear();
    pendingNetworkBonuses_.clear();
    pendingNetworkMineContacts_.clear();
    const float achievementMultiplier =
        initialPlayerProfile_.difficulty == "gdHard"
            ? 1.5F
            : initialPlayerProfile_.difficulty == "gdNormal" ? 1.2F
                                                               : 1.0F;
    achievementModel_.Configure(
        &race_.achievements, initialAchievementPoints_,
        initialAchievementIterations_, achievementMultiplier);
    achievementModel_.SetCampaign(campaign_);
    campaignRewardsApplied_ = false;
    raceLifecycle_.Reset();
    raceRunState_.Reset();
    // Static categories which the active backend does not need still
    // consumed MapObj IDs during Map::Load. Player::CreateCar then asks Map
    // for the next ID; it never trusts an ID stored on a player descriptor.
    if (race_.firstDynamicMapObjectId > 0U)
        map_.ReserveIdsThrough(race_.firstDynamicMapObjectId - 1U);
    for (std::size_t index = 0; index < racers_.size(); ++index)
    {
        const auto& sourceRacer = race_.racers[index];
        const std::size_t vehicleIndex = sourceRacer.vehicle;
        const auto& vehicle =
            sourceRacer.hasConfiguredVehicle
                ? sourceRacer.configuredVehicle
                : race_.vehicles.at(std::min(
                      vehicleIndex, race_.vehicles.size() - 1U));
        racers_[index].Reset(
            vehicle.maximumLife,
            static_cast<std::uint32_t>(index + 1U), &map_.GetTrace());
        const int playerId =
            sourceRacer.playerId != source::Player::undefinedId
                ? sourceRacer.playerId
                : (sourceRacer.human && index == 0U
                       ? source::Player::humanId
                       : static_cast<int>(index));
        racers_[index].ConfigureIdentity(
            playerId, static_cast<int>(sourceRacer.gamerId),
            sourceRacer.netSlot, sourceRacer.name,
            sourceRacer.netName, sourceRacer.color);
        racers_[index].SetCar(&vehicle);
        createRacerMapObject(index);
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
            runtime.SetMoney(initialPlayerProfile_.money);
            runtime.SetPoints(initialPlayerProfile_.points);
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
        auto activeLoadout = sourceRacer.loadout;
        const bool bindProfileSlots =
            index == 0U && std::any_of(
                initialPlayerProfile_.slots.begin(),
                initialPlayerProfile_.slots.end(),
                [](const ProfileSlot& slot) {
                    return !slot.record.empty();
                });
        if (bindProfileSlots)
        {
            static constexpr std::array<std::string_view,
                                        PlayerProfile::slotCount>
                physicalNames{
                    "stWheel", "stTruba", "stArmor", "stMotor",
                    "stHyper", "stMine", "stWeapon1", "stWeapon2",
                    "stWeapon3", "stWeapon4"};
            activeLoadout.clear();
            for (std::size_t slot = 0U;
                 slot < initialPlayerProfile_.slots.size(); ++slot)
            {
                const auto& profileSlot =
                    initialPlayerProfile_.slots[slot];
                if (profileSlot.record.empty())
                    continue;
                activeLoadout.push_back(
                    {profileSlot.record, std::string(physicalNames[slot]),
                     profileSlot.charge});
            }
        }
        racers_[index].BindSlots(race_.workshop, activeLoadout);
        racers_[index].SyncSelectedWeapon(race_.weapons.size());
        auto configureWeapon = [&](source::Weapon& runtimeWeapon,
                                   std::size_t weaponIndex) {
            if (weaponIndex == RacerRuntime::invalidWeapon ||
                weaponIndex >= race_.weapons.size())
            {
                runtimeWeapon.SetDesc(source::Weapon::Desc{});
                runtimeWeapon.Reset();
                return;
            }
            const auto& definition = race_.weapons[weaponIndex];
            std::vector<ProjectileDefinition> projectiles;
            projectiles.reserve(definition.projectiles.size());
            for (const auto& projectile : definition.projectiles)
            {
                if (projectile.spawnOnParentDeath)
                    continue;
                projectiles.push_back(projectile);
            }
            runtimeWeapon.SetDesc(
                definition.shotDelay,
                projectiles);
            runtimeWeapon.Reset();
        };
        auto& weaponRack = racers_[index].GetWeaponRack();
        for (std::size_t slot = 0U;
             slot < PlayerProfile::weaponSlotCount; ++slot)
        {
            configureWeapon(
                weaponRack.primary[slot],
                racers_[index].weaponSlots[slot]);
        }
        configureWeapon(
            weaponRack.hyper,
            racers_[index].hyperWeapon);
        configureWeapon(
            weaponRack.mine,
            racers_[index].mineWeapon);
        racers_[index].BindWeaponItems(race_.weapons);
        racers_[index].CreateCar(true);
        racers_[index].car.SetSize(vehicle.boundingSize);
    }
    const auto humanPosition = std::find_if(
        racers_.begin(), racers_.end(),
        [](const source::Player& player) { return player.IsHuman(); });
    if (humanPosition != racers_.end())
    {
        humanRacer_ = static_cast<std::size_t>(
            humanPosition - racers_.begin());
        // HumanPlayer's constructor disables gpReflScene only for the local
        // owner. Remote NetPlayer opponents retain the Player default.
        humanPosition->SetReflScene(false);
    }
    source::Player* human =
        humanPosition == racers_.end() ? nullptr : &*humanPosition;
    for (std::size_t index = 0U; index < racers_.size(); ++index)
    {
        auto& player = racers_[index];
        if (player.IsComputer() || player.IsHuman())
        {
            aiPlayers_.emplace_back(
                &player, player.IsHuman(), map_.GetTrace().GetTrackCount());
            // The release/debug build distinction is represented by enabled
            // state. Keeping a dormant Human AI owner permits DEBUG_PX to be
            // selected at runtime without giving Human any cheat flags.
            aiPlayers_.back().CreateCar();
            if (player.IsHuman())
                aiPlayers_.back().SetEnabled(false);
        }
        else
        {
            // Net opponents receive authoritative vehicle snapshots and do
            // not own an AIPlayer in the Windows NetPlayer constructor.
            aiPlayers_.emplace_back(map_.GetTrace().GetTrackCount());
        }
    }
    raceRunState_.StartRace(
        racers_, human,
        race_.environment.weather == Weather::Night,
        race_.weapons.size());
    if (legacyWindowsDebug_)
        raceRunState_.GoRace(human);
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
    if (humanRacer_ >= racers_.size())
        return;
    const auto& runtime = racers_[humanRacer_];
    profile.money = runtime.GetMoney();
    profile.points = runtime.GetPoints();
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
    const auto* hyperItem = runtime.GetHyperWeaponItem();
    const auto* mineItem = runtime.GetMineWeaponItem();
    writeWeapon(
        PlayerProfile::hyperSlot, runtime.hyperWeapon,
        hyperItem != nullptr ? hyperItem->GetCntCharge() : 0U);
    writeWeapon(
        PlayerProfile::mineSlot, runtime.mineWeapon,
        mineItem != nullptr ? mineItem->GetCntCharge() : 0U);
    const auto primaryItems = runtime.GetPrimaryWeaponItems();
    for (std::size_t slot = 0;
         slot < PlayerProfile::weaponSlotCount; ++slot)
    {
        writeWeapon(PlayerProfile::firstWeaponSlot + slot,
                    runtime.weaponSlots[slot],
                    primaryItems[slot] != nullptr
                        ? primaryItems[slot]->GetCntCharge()
                        : 0U);
    }
}

void OriginalRaceSession::applyAchievementProfile(
    const ProfileState& profile)
{
    initialAchievementPoints_ = profile.achievementPoints;
    initialAchievementIterations_ =
        profile.achievementIterations;
    const float achievementMultiplier =
        profile.player.difficulty == "gdHard"
            ? 1.5F
            : profile.player.difficulty == "gdNormal" ? 1.2F : 1.0F;
    achievementModel_.Configure(
        &race_.achievements, initialAchievementPoints_,
        initialAchievementIterations_, achievementMultiplier);
    achievementModel_.SetCampaign(campaign_);
}

void OriginalRaceSession::setCampaign(bool campaign) noexcept
{
    campaign_ = campaign;
    achievementModel_.SetCampaign(campaign);
}

void OriginalRaceSession::writeAchievementProfile(
    ProfileState& profile) const
{
    profile.achievementPoints = achievementModel_.GetPoints();
    profile.achievementIterations = achievementModel_.GetIterations();
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

const Vehicle& OriginalRaceSession::vehicleForRacer(
    std::size_t racer) const noexcept
{
    if (racer < racers_.size())
    {
        if (const auto* active = racers_[racer].GetCarRecord())
            return *active;
    }
    const auto& descriptor = race_.racers.at(racer);
    return descriptor.hasConfiguredVehicle
               ? descriptor.configuredVehicle
               : race_.vehicles.at(descriptor.vehicle);
}

void OriginalRaceSession::setNetworkGameplayRole(
    bool enabled, bool host, std::vector<bool> ownedRacers)
{
    networkGameplayEnabled_ = enabled;
    networkGameplayHost_ = enabled && host;
    networkOwnedRacers_ = std::move(ownedRacers);
    networkOwnedRacers_.resize(racers_.size(), false);
}

void OriginalRaceSession::releaseRacerProjectileReferences(
    std::size_t racer) noexcept
{
    for (auto& projectile : projectiles_)
    {
        const bool senderIsWeapon = projectile.owner == racer;
        const bool senderIsTarget = projectile.target == racer;
        bool linkedToWeapon = false;
        if (senderIsWeapon)
        {
            const auto* definition = runtimeProjectileDefinition(
                race_, projectile);
            linkedToWeapon = definition != nullptr &&
                source::Proj::GetTypeRules(definition->type)
                    .linkedToWeapon;
        }
        const auto result = source::Proj::OnDestroy(
            senderIsWeapon, linkedToWeapon, senderIsTarget);
        if (result.clearTarget)
            projectile.target = RacerRuntime::invalidWeapon;
        if (!result.clearWeapon)
            continue;
        projectile.damageOwner = RacerRuntime::invalidWeapon;
        projectile.ownerCollisionArmed = true;
        if (result.destroy)
        {
            projectile.active = false;
            continue;
        }
        if (projectile.attached)
        {
            projectile.attached = false;
            projectile.detachedFromWeapon = true;
        }
    }
    for (auto& mine : mines_)
    {
        if (mine.owner != racer)
            continue;
        // Mine/Maslo are not parented by LinkToWeapon. OnDestroy therefore
        // leaves the world actor alive and only clears the weapon/car ref.
        mine.damageOwner = RacerRuntime::invalidWeapon;
        mine.linkedToOwner = false;
        mine.ignoreOwnerCollision = false;
    }
}

bool OriginalRaceSession::disconnectNetworkRacer(
    std::size_t racer) noexcept
{
    if (racer >= racers_.size() || racers_[racer].disconnected)
        return false;

    auto& runtime = racers_[racer];
    runtime.Disconnect();
    freeRacerMapObject(racer);
    releaseRacerProjectileReferences(racer);
    if (racer < vehicleInputs_.size())
        vehicleInputs_[racer] = {};
    if (racer < networkOwnedRacers_.size())
        networkOwnedRacers_[racer] = false;
    if (racer < aiPlayers_.size())
        aiPlayers_[racer].FreeCar();
    for (auto& aiPlayer : aiPlayers_)
        aiPlayer.DisposeTarget(racer);

    std::erase_if(respawns_, [racer](const auto& request) {
        return request.racer == racer;
    });
    std::erase_if(velocityRequests_, [racer](const auto& request) {
        return request.racer == racer;
    });
    std::erase_if(angularVelocityRequests_, [racer](const auto& request) {
        return request.racer == racer;
    });
    std::erase_if(angularMomentumRequests_, [racer](const auto& request) {
        return request.racer == racer;
    });
    // FreeCar removes car-owned listener/effect objects. Independent fired
    // projectiles and mines remain world objects in the source.
    std::erase_if(effects_, [racer](const RaceEffect& effect) {
        return effect.racer == racer || effect.parentRacer == racer;
    });
    return true;
}

bool OriginalRaceSession::synchronizePlayerPresentation(
    std::size_t racer, int gamerId,
    const std::array<float, 4>& color) noexcept
{
    if (racer >= racers_.size())
        return false;
    racers_[racer].SetGamerId(gamerId);
    racers_[racer].SetColor(color);
    return true;
}

void OriginalRaceSession::pushDamageEvent(
    std::size_t target, std::size_t attacker, const Vec3& position,
    float damage, DamageType damageType, bool networkReplicated)
{
    RaceEvent event;
    event.kind = RaceEventKind::Damage;
    event.racer = target;
    event.target = attacker;
    event.position = position;
    event.value = damage;
    event.touchDamage = damageType == DamageType::Touch;
    event.damageType = damageType;
    event.networkReplicated = networkReplicated;
    events_.push_back(std::move(event));
}

void OriginalRaceSession::appendPlayerGameEvents(
    std::size_t racer, const Vec3& position,
    bool networkReplicated)
{
    if (racer >= racers_.size())
        return;
    for (const auto& sourceEvent : racers_[racer].TakeGameEvents())
    {
        RaceEvent event;
        event.racer = racer;
        event.target = sourceEvent.otherPlayerId;
        event.position = position;
        event.value = sourceEvent.value;
        event.touchDamage =
            sourceEvent.damageType == DamageType::Touch;
        event.damageType = sourceEvent.damageType;
        event.networkReplicated = networkReplicated;
        switch (sourceEvent.kind)
        {
        case source::PlayerGameEventKind::Damage:
            event.kind = RaceEventKind::Damage;
            event.authoritativeLife = racers_[racer].life;
            event.authoritativeDeath = racers_[racer].destroyed;
            break;
        case source::PlayerGameEventKind::Kill:
            event.kind = RaceEventKind::Kill;
            event.racer = sourceEvent.otherPlayerId;
            event.target = racer;
            event.killCredit = true;
            break;
        case source::PlayerGameEventKind::Overboard:
            event.kind = RaceEventKind::Overboard;
            event.killCredit = false;
            break;
        case source::PlayerGameEventKind::DeathMine:
            event.kind = RaceEventKind::DeathMine;
            event.killCredit = false;
            break;
        case source::PlayerGameEventKind::Death:
            event.kind = RaceEventKind::Death;
            event.killCredit = false;
            break;
        }
        events_.push_back(std::move(event));
    }
}

bool OriginalRaceSession::applyRacerDamageInternal(
    std::size_t target, std::size_t attacker, const Vec3& position,
    float sourceDamage, DamageType damageType,
    const r3d::physics::VehicleState& vehicle,
    bool incomingAlreadySupported, bool synchronizeState,
    float targetLife, bool death, bool networkReplicated)
{
    if (target >= racers_.size() || racers_[target].destroyed)
        return false;
    if (networkGameplayEnabled_ && !networkReplicated)
    {
        // NetRace::Damage rejects AI/opponent senders. On a client it also
        // accepts only damage produced by that peer's owned NetPlayer.
        if (attacker == RacerRuntime::invalidWeapon)
        {
            if (!networkGameplayHost_)
                return false;
        }
        else if (attacker >= racers_.size() ||
                 !racers_[attacker].IsHumanOrOpponent() ||
                 (!networkGameplayHost_ &&
                  (attacker >= networkOwnedRacers_.size() ||
                   !networkOwnedRacers_[attacker])))
        {
            return false;
        }
    }
    // Logic::Damage applies the first reflector before entering NetRace.
    // Consequently authoritative RPC values must not pass through the
    // reflector a second time on the host or receiving client.
    const float incoming = incomingAlreadySupported
                               ? sourceDamage
                               : source::Logic::ResolveDamage(
                                     target < racers_.size()
                                         ? &racers_[target]
                                         : nullptr,
                                     sourceDamage, damageType);
    // The Windows client does not call GameObject::Damage while producing
    // NetRace::Damage. It waits for the host's targetLife/death packet.
    if (networkGameplayEnabled_ && !networkGameplayHost_ &&
        !synchronizeState)
    {
        const std::size_t damageEvent = events_.size();
        pushDamageEvent(
            target, attacker, position, incoming, damageType,
            networkReplicated);
        events_[damageEvent].authoritativeLife = racers_[target].life;
        events_[damageEvent].authoritativeDeath = racers_[target].destroyed;
        return false;
    }

    auto& runtime = racers_[target];
    const auto damageResult = synchronizeState
        ? source::Logic::Damage(
              runtime, attacker, incoming, targetLife, death,
              damageType)
        : source::Logic::Damage(
              runtime, attacker, incoming, damageType);
    // Listener dispatch belongs after GameObject::Damage. In particular, a
    // non-authoritative Windows client waits for the host packet and must not
    // start local shield/damage effects for its outbound request.
    const bool makeEnergyEffect =
        runtime.ConsumeEnergyDamageEffectCreated();
    if (makeEnergyEffect && target < race_.racers.size())
    {
        const auto& vehicleDefinition = vehicleForRacer(target);
        const auto& visual = vehicleDefinition.energyDamageEffect;
        if (!visual.visualNodes.empty() ||
            !visual.particleEmitters.empty())
        {
            RaceEffect effect;
            effect.kind = RaceEventKind::VehicleEnergyDamage;
            effect.racer = target;
            effect.origin = position;
            const auto timing = sourceEffectTiming(visual, 0.5F);
            applySourceEffectTiming(effect, timing);
            attachSourceLifeEffect(effect, visual.soundPaths, target,
                                   target);
            effects_.push_back(std::move(effect));
        }
    }
    // Drain the actual GameObject/Player dispatch queue. Its lethal order is
    // Damage, Kill, then the Player listener's special death event and Death.
    appendPlayerGameEvents(target, position, networkReplicated);
    if (!damageResult.death)
        return false;
    destroyRacer(target, position, vehicle, true);
    return true;
}

NetworkDamageResult OriginalRaceSession::applyNetworkPlayerDamage(
    std::size_t target, std::size_t attacker, Vec3 position,
    float value, DamageType damageType,
    const r3d::physics::VehicleState& vehicle,
    bool synchronizeState, float targetLife, bool death)
{
    applyRacerDamageInternal(
        target, attacker, position, value, damageType, vehicle,
        true, synchronizeState, targetLife, death, true);
    if (target >= racers_.size())
        return {};
    return {racers_[target].life, racers_[target].destroyed};
}

NetworkDamageResult OriginalRaceSession::applyNetworkMapObjectDamage(
    std::uint32_t targetObjectId, std::size_t attacker,
    float value, DamageType damageType,
    bool synchronizeState, float targetLife, bool death)
{
    const auto target = decorationForMapObjectId(targetObjectId);
    if (target == RacerRuntime::invalidWeapon)
        return {};
    return applyDecorationDamageInternal(
        target, value, attacker, damageType, synchronizeState,
        targetLife, death, true);
}

void OriginalRaceSession::queueNetworkShot(ReplicatedShot shot)
{
    if (shot.racer >= racers_.size() || shot.slotMask == 0U)
        return;
    pendingNetworkShots_.push_back(std::move(shot));
}

void OriginalRaceSession::queueNetworkBonus(ReplicatedBonus bonus)
{
    if (bonus.racer >= racers_.size() ||
        bonus.bonus >= race_.bonuses.size())
        return;
    pendingNetworkBonuses_.push_back(std::move(bonus));
}

void OriginalRaceSession::queueNetworkMineContact(
    ReplicatedMineContact contact)
{
    if (contact.racer >= racers_.size() || contact.projectileId == 0U)
        return;
    if (!contact.mapProjectile &&
        contact.projectileOwner >= racers_.size())
        return;
    pendingNetworkMineContacts_.push_back(std::move(contact));
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
        decorationObjects().Get(hit) == nullptr ||
        !decorationActive_[hit])
        return false;
    const auto definition = race_.decorationInstances[hit].definition;
    if (definition >= race_.decorationDefinitions.size() ||
        !race_.decorationDefinitions[definition].destructible)
        return false;
    applyDecorationDamageInternal(
        hit, damage, attacker, DamageType::Simple,
        false, 0.0F, false, false);
    return true;
}

NetworkDamageResult
OriginalRaceSession::applyDecorationDamageInternal(
    std::size_t hit, float damage, std::size_t attacker,
    DamageType damageType, bool synchronizeState,
    float targetLife, bool death, bool networkReplicated)
{
    if (hit >= decorationActive_.size() ||
        hit >= race_.decorationInstances.size() ||
        !decorationActive_[hit])
        return {};
    const auto& instance = race_.decorationInstances[hit];
    if (instance.definition >= race_.decorationDefinitions.size() ||
        !race_.decorationDefinitions[instance.definition].destructible)
        return {decorationLife_[hit], false};

    // Logic::Damage in a network race accepts map damage only from a human
    // NetPlayer. A client sends its local request without mutating the
    // object; AI damage is deliberately ignored by NetRace::Damage.
    if (networkGameplayEnabled_ && !networkReplicated)
    {
        if (attacker >= racers_.size() ||
            !racers_[attacker].IsHumanOrOpponent())
            return {decorationLife_[hit], false};
        if (!networkGameplayHost_)
        {
            if (attacker >= networkOwnedRacers_.size() ||
                !networkOwnedRacers_[attacker])
                return {decorationLife_[hit], false};
            RaceEvent request;
            request.kind = RaceEventKind::MapObjectDamage;
            request.racer = attacker;
            request.target = hit;
            request.position = instance.transform.position;
            request.value = std::max(damage, 0.0F);
            request.damageType = damageType;
            events_.push_back(std::move(request));
            return {decorationLife_[hit], false};
        }
    }

    const float appliedDamage = damage;
    auto* mapObject = decorationObjects().Get(hit);
    if (mapObject == nullptr || mapObject->GetDestrObj() == nullptr)
        return {decorationLife_[hit], false};
    auto& object = *mapObject->GetDestrObj();
    const auto damageResult = synchronizeState
        ? object.Damage(attacker, appliedDamage, targetLife, death,
                        damageType)
        : object.Damage(attacker, appliedDamage, damageType);
    decorationLife_[hit] = object.GetLife();
    const bool destroyed = damageResult.death;

    RaceEvent damageEvent;
    damageEvent.kind = RaceEventKind::MapObjectDamage;
    damageEvent.racer = attacker;
    damageEvent.target = hit;
    damageEvent.position = instance.transform.position;
    damageEvent.value = appliedDamage;
    damageEvent.damageType = damageType;
    damageEvent.authoritativeLife = decorationLife_[hit];
    damageEvent.authoritativeDeath = destroyed;
    damageEvent.networkReplicated = networkReplicated;
    events_.push_back(std::move(damageEvent));
    if (!destroyed)
        return {decorationLife_[hit], false};

    decorationActive_[hit] = false;
    if (!object.HasPendingDestruction())
        return {decorationLife_[hit], true};
    decorationObjects().ProgressOne(hit, 0.0F);
    const Vec3 position =
        race_.decorationInstances[hit].transform.position;
    RaceEvent destroyedEvent;
    destroyedEvent.kind = RaceEventKind::DecorationDestroyed;
    destroyedEvent.racer = attacker;
    destroyedEvent.target = hit;
    destroyedEvent.position = position;
    destroyedEvent.value = appliedDamage;
    destroyedEvent.damageType = damageType;
    destroyedEvent.networkReplicated = networkReplicated;
    events_.push_back(std::move(destroyedEvent));
    return {decorationLife_[hit], true};
}

void OriginalRaceSession::setPaused(bool paused) noexcept
{
    gameModeRaceState_.Pause(paused);
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

void OriginalRaceSession::synchronizeNetworkCountdown(
    std::int32_t stage) noexcept
{
    const auto transition =
        gameModeRaceState_.SynchronizeCountdown(stage);
    if (!transition)
        return;

    const RacePhase targetPhase =
        transition->raceStarted ? RacePhase::Racing
                                : RacePhase::Countdown;
    if (phase_ == RacePhase::Paused)
        phaseBeforePause_ = targetPhase;
    else
        phase_ = targetPhase;
    if (transition->raceStarted)
    {
        const auto humanPosition = std::find_if(
            racers_.begin(), racers_.end(),
            [](const source::Player& player) {
                return player.IsHuman();
            });
        source::Player* human =
            humanPosition == racers_.end() ? nullptr : &*humanPosition;
        raceRunState_.GoRace(human);
    }

    events_.push_back(
        {RaceEventKind::CountdownChanged, 0, 0, {},
         gameModeRaceState_.CountdownSeconds()});
}

void OriginalRaceSession::setNetworkFinishControlled(
    bool controlled) noexcept
{
    networkFinishControlled_ = controlled;
    if (controlled && phase_ == RacePhase::Finished)
        gameModeRaceState_.CancelFinishTimer();
}

void OriginalRaceSession::startNetworkFinishTimer() noexcept
{
    if (networkFinishControlled_ && phase_ == RacePhase::Finished &&
        !gameModeRaceState_.IsFinishTimerRunning() &&
        !gameModeRaceState_.IsFinishPresentationReady())
        gameModeRaceState_.RunFinishTimer();
}

void OriginalRaceSession::synchronizeNetworkFinishResults(
    const std::vector<ReplicatedRaceResult>& results) noexcept
{
    networkFinishControlled_ = true;
    phase_ = RacePhase::Finished;
    phaseBeforePause_ = phase_;
    std::vector<source::RaceResult> sourceResults;
    sourceResults.reserve(results.size());
    for (const auto& result : results)
    {
        if (result.racer >= racers_.size())
            continue;
        sourceResults.push_back(
            {result.racer, result.place,
             static_cast<std::uint32_t>(
                 std::max(result.rewardMoney, 0)),
             static_cast<std::uint32_t>(
                 std::max(result.rewardPoints, 0)),
             static_cast<std::uint32_t>(
                 std::max(result.pickedMoney, 0)),
             1.5F});
    }
    raceLifecycle_.LoadResults(std::move(sourceResults));
    for (const auto& result : raceLifecycle_.GetResults())
        completeRacer(result, elapsedSeconds_);
    gameModeRaceState_.FinishImmediately();
}

RacePhase OriginalRaceSession::phase() const noexcept
{
    return phase_;
}

float OriginalRaceSession::countdownSeconds() const noexcept
{
    return gameModeRaceState_.CountdownSeconds();
}

std::int32_t OriginalRaceSession::countdownStage() const noexcept
{
    return gameModeRaceState_.CountdownStage();
}

float OriginalRaceSession::elapsedSeconds() const noexcept
{
    return elapsedSeconds_;
}

bool OriginalRaceSession::finishPresentationReady() const noexcept
{
    return phase_ == RacePhase::Finished &&
           gameModeRaceState_.IsFinishPresentationReady();
}

bool OriginalRaceSession::effectsMuted() const noexcept
{
    return gameModeRaceState_.IsPaused();
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

source::SoundMotorMix OriginalRaceSession::racerMotorMix(
    std::size_t racer) const noexcept
{
    if (racer >= racers_.size())
        return {};
    return racers_[racer].gameCar.GetSoundMotorMix();
}

std::size_t OriginalRaceSession::humanRacer() const noexcept
{
    return humanRacer_;
}

std::uint32_t OriginalRaceSession::humanOrOpponentCount() const noexcept
{
    return static_cast<std::uint32_t>(std::count_if(
        racers_.begin(), racers_.end(),
        [](const RacerRuntime& racer) {
            return !racer.disconnected && racer.IsHumanOrOpponent();
        }));
}

std::uint32_t OriginalRaceSession::totalHumanOrOpponentPoints() const noexcept
{
    std::uint64_t total = 0U;
    for (const auto& racer : racers_)
    {
        if (!racer.disconnected && racer.IsHumanOrOpponent())
            total += racer.GetPoints();
    }
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(
        total, std::numeric_limits<std::uint32_t>::max()));
}

void OriginalRaceSession::resetTournamentPassPoints() noexcept
{
    for (auto& racer : racers_)
    {
        // A disconnected portable tombstone represents a Player already
        // removed by NetPlayer::~NetPlayer and is not in Race::_playerList.
        if (!racer.disconnected)
            racer.SetPoints(0U);
    }
}

const std::vector<source::RaceResult>&
OriginalRaceSession::results() const noexcept
{
    return raceLifecycle_.GetResults();
}

const source::RaceResult* OriginalRaceSession::resultForRacer(
    std::size_t racer) const noexcept
{
    return raceLifecycle_.GetResult(racer);
}

Vec3 OriginalRaceSession::mapPosition(std::size_t racer) const noexcept
{
    return racer < racers_.size() ? racers_[racer].car.GetMapPos()
                                  : Vec3{};
}

const std::vector<bool>& OriginalRaceSession::decorationActive() const noexcept
{
    return decorationActive_;
}

const std::vector<float>& OriginalRaceSession::decorationLife() const noexcept
{
    return decorationLife_;
}

const std::vector<bool>& OriginalRaceSession::bonusActive() const noexcept
{
    return bonusActive_;
}

const std::vector<float>& OriginalRaceSession::bonusScales() const noexcept
{
    return bonusScales_;
}

const source::Map& OriginalRaceSession::sourceMap() const noexcept
{
    return map_;
}

bool OriginalRaceSession::racerHasAiController(
    std::size_t racer) const noexcept
{
    return racer < aiPlayers_.size() && aiPlayers_[racer].HasCar();
}

std::uint32_t OriginalRaceSession::racerMapObjectId(
    std::size_t racer) const noexcept
{
    return racer < racerMapObjects_.size() &&
                   racerMapObjects_[racer] != nullptr
        ? racerMapObjects_[racer]->GetId()
        : source::Map::defaultMapObjId;
}

std::size_t OriginalRaceSession::racerForMapObjectId(
    std::uint32_t mapObjectId) const noexcept
{
    const auto* object = map_.GetMapObj(mapObjectId);
    if (object == nullptr ||
        object->GetCategory() != source::MapObjCategory::Car ||
        object->GetPlayer() == nullptr)
        return RacerRuntime::invalidWeapon;
    const auto found = std::find_if(
        racers_.begin(), racers_.end(),
        [player = object->GetPlayer()](const source::Player& candidate) {
            return &candidate == player;
        });
    return found != racers_.end()
        ? static_cast<std::size_t>(found - racers_.begin())
        : RacerRuntime::invalidWeapon;
}

std::size_t OriginalRaceSession::decorationForMapObjectId(
    std::uint32_t mapObjectId) const noexcept
{
    const auto* object = map_.GetMapObj(mapObjectId);
    if (object == nullptr ||
        object->GetCategory() !=
            source::MapObjCategory::Decoration ||
        object->GetSourceIndex() >=
            race_.decorationInstances.size())
        return RacerRuntime::invalidWeapon;
    return object->GetSourceIndex();
}

std::size_t OriginalRaceSession::bonusForMapObjectId(
    std::uint32_t mapObjectId) const noexcept
{
    const auto* object = map_.GetMapObj(mapObjectId);
    if (object == nullptr ||
        object->GetCategory() != source::MapObjCategory::Bonus ||
        object->GetSourceIndex() >= race_.bonuses.size())
        return RacerRuntime::invalidWeapon;
    return object->GetSourceIndex();
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

std::vector<AngularMomentumRequest>
OriginalRaceSession::takeAngularMomentumRequests()
{
    auto result = std::move(angularMomentumRequests_);
    angularMomentumRequests_.clear();
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

void OriginalRaceSession::buildSourceTrace()
{
    auto& sourceTrace = map_.GetTrace();
    sourceTrace.Clear();
    for (const auto& pointData : race_.tracePoints)
    {
        auto* point = sourceTrace.AddPoint(pointData.id);
        point->SetPos(pointData.position);
        point->SetSize(pointData.width);
    }
    const auto appendPath = [&](const std::vector<std::uint32_t>& nodes) {
        if (nodes.size() < 2U)
            return;
        auto* path = sourceTrace.AddPath();
        for (const auto id : nodes)
        {
            auto* point = sourceTrace.FindPoint(id);
            if (point == nullptr)
                throw std::runtime_error(
                    "Original race trace path is unresolved");
            path->Add(point);
        }
    };
    if (!race_.tracePaths.empty())
    {
        for (const auto& path : race_.tracePaths)
            appendPath(path);
    }
    else
    {
        appendPath(race_.tracePath);
    }
    if (sourceTrace.GetPathCount() == 0U)
        throw std::runtime_error("Original race trace has no paths");
}

OriginalRaceSession::TraceNodeRef
OriginalRaceSession::racerTraceNode(std::size_t racer) const noexcept
{
    return racer < racers_.size() ? racers_[racer].car.GetCurTileRef()
                                  : TraceNodeRef{};
}

float OriginalRaceSession::tracePathLength(std::size_t path) const
{
    const auto* value = map_.GetTrace().GetPath(path);
    return value != nullptr ? value->GetLength() : 0.0F;
}

float OriginalRaceSession::lapPosition(
    std::size_t racer,
    const r3d::physics::VehicleState&) const
{
    return racer < racers_.size() ? racers_[racer].car.GetLap()
                                  : 0.0F;
}

float OriginalRaceSession::lastCorrectLapPosition(
    std::size_t racer) const
{
    return racer < racers_.size()
               ? racers_[racer].car.GetLap(true)
               : 0.0F;
}

void OriginalRaceSession::updateProgress(
    std::size_t racer, const r3d::physics::VehicleState& vehicle,
    float seconds)
{
    auto& runtime = racers_[racer];
    if (runtime.GetFinished() || runtime.destroyed)
        return;

    const auto state = runtime.car.Update(
        map_.GetTrace(), vehicle.body.position,
        normalized3(forward(vehicle.body.rotation)),
        vehicle.speed, seconds);
    if (state.lostControl)
    {
        events_.push_back({RaceEventKind::LostControl, racer, 0U,
                           vehicle.body.position, vehicle.speed});
    }
    if (state.moveInverseStarted)
    {
        events_.push_back({RaceEventKind::MoveInverse, racer, 0U,
                           vehicle.body.position,
                           runtime.car.GetDist()});
    }
    if (!state.currentTile.valid() || !state.lastNodeChanged)
        return;

    const auto& tilePath = tracePathAt(state.lastNode.path);
    const auto& tileStart =
        tracePoint(state.lastNode.path, state.lastNode.node);
    if (state.previousLast.valid())
    {
        const std::size_t checkpoint =
            state.lapPassed ? tracePathAt(0U).size() - 1U
                            : state.lastNode.node;
        events_.push_back({RaceEventKind::Checkpoint, racer,
                           checkpoint, tileStart.position, 0.0F});
    }
    if (state.lastNode.path == 0U && !state.lapPassed)
    {
        runtime.nextPathNode = std::clamp<std::size_t>(
            state.lastNode.node + 1U, 1U, tilePath.size() - 1U);
    }
    if (!state.lapPassed)
        return;

    runtime.OnLapPass(race_.weapons.size());
    runtime.nextPathNode = 1;

    const auto leader = std::min_element(
        racers_.begin(), racers_.end(),
        [](const RacerRuntime& first, const RacerRuntime& second) {
            return first.GetPlace() < second.GetPlace();
        });
    const std::size_t leaderIndex =
        leader == racers_.end()
            ? racer
            : static_cast<std::size_t>(leader - racers_.begin());
    const std::size_t activePlayerCount =
        static_cast<std::size_t>(std::count_if(
            racers_.begin(), racers_.end(),
            [](const RacerRuntime& candidate) {
                return !candidate.disconnected;
            }));
    const bool localHuman = runtime.IsHuman();
    const bool hasHuman = std::any_of(
        racers_.begin(), racers_.end(),
        [](const RacerRuntime& candidate) {
            return !candidate.disconnected && candidate.IsHuman();
        });
    source::RaceLifecyclePlayer player;
    player.playerId = racer;
    player.human = localHuman;
    player.opponent = runtime.IsOpponent();
    player.disconnected = runtime.disconnected;
    player.finished = runtime.GetFinished();
    player.laps = runtime.car.numLaps;
    player.pickedMoney = runtime.GetPickMoney();
    const auto sourceResult = raceLifecycle_.OnLapPass(
        player, race_.lapCount, activePlayerCount, hasHuman,
        leaderIndex, race_.rewardMoney, race_.rewardPoints);
    if (sourceResult.completed)
    {
        completeRacer(*sourceResult.completed, elapsedSeconds_);
        // Portable-only notification used by network/render adapters. The
        // source public events below retain their exact order independently.
        events_.push_back({RaceEventKind::Finish, racer, 0,
                           vehicle.body.position, runtime.finishTime});
    }
    for (const auto& sourceEvent : sourceResult.events)
    {
        RaceEventKind kind = RaceEventKind::Lap;
        switch (sourceEvent.kind)
        {
        case source::RaceLifecycleEventKind::LeadFinish:
            kind = RaceEventKind::LeadFinish;
            break;
        case source::RaceLifecycleEventKind::SecondFinish:
            kind = RaceEventKind::SecondFinish;
            break;
        case source::RaceLifecycleEventKind::ThirdFinish:
            kind = RaceEventKind::ThirdFinish;
            break;
        case source::RaceLifecycleEventKind::LastFinish:
            kind = RaceEventKind::LastFinish;
            break;
        case source::RaceLifecycleEventKind::RaceFinish:
            kind = RaceEventKind::RaceFinish;
            phase_ = RacePhase::Finished;
            if (networkFinishControlled_)
                gameModeRaceState_.CancelFinishTimer();
            else
                gameModeRaceState_.RunFinishTimer();
            break;
        case source::RaceLifecycleEventKind::PassLap:
            kind = RaceEventKind::Lap;
            break;
        case source::RaceLifecycleEventKind::LastLap:
            kind = RaceEventKind::LastLap;
            break;
        }
        const auto eventRacer = sourceEvent.playerId;
        events_.push_back(
            {kind, eventRacer,
             kind == RaceEventKind::Lap ? runtime.car.numLaps : 0U,
             vehicle.body.position,
             kind == RaceEventKind::Lap
                 ? static_cast<float>(runtime.car.numLaps)
                 : runtime.finishTime});
    }
}

std::vector<source::Player::ProgressResult>
OriginalRaceSession::progressPlayers(
    float seconds,
    const std::vector<r3d::physics::VehicleState>& vehicles)
{
    // Race::OnFixedStep calls every Player::OnProgress first and only then
    // AISystem::OnProgress. Keep physics-backed CarState::Update adjacent to
    // its Player owner while leaving Jolt pose queries in this adapter.
    for (std::size_t racer = 0U;
         racer < racers_.size() && racer < vehicles.size(); ++racer)
    {
        if (!racers_[racer].disconnected)
            updateProgress(racer, vehicles[racer], seconds);
    }

    const auto difficultyIndex =
        initialPlayerProfile_.difficulty == "gdEasy"
            ? 0U
            : initialPlayerProfile_.difficulty == "gdHard" ? 2U : 1U;
    std::vector<source::Player::CheatPlayerView> cheatPlayers;
    cheatPlayers.reserve(racers_.size());
    for (std::size_t racer = 0U; racer < racers_.size(); ++racer)
    {
        cheatPlayers.push_back(
            {racer,
             racers_[racer].IsHumanOrOpponent(),
             !racers_[racer].disconnected,
             racers_[racer].car.GetLap()});
    }

    std::vector<source::Player::ProgressResult> results(racers_.size());
    for (std::size_t racer = 0U;
         racer < racers_.size() && racer < vehicles.size(); ++racer)
    {
        auto& runtime = racers_[racer];
        if (runtime.disconnected)
            continue;
        results[racer] = runtime.OnProgress(
            seconds, !runtime.destroyed, runtime.GetCheat(), racer,
            difficultyIndex, cheatPlayers);

        if (results[racer].restore ==
            source::PlayerRestoreStep::ActivateCar)
        {
            createRacerMapObject(racer);
        }

        if (results[racer].restore ==
            source::PlayerRestoreStep::QueueRespawn)
        {
            queueRespawn(racer, vehicles);
        }

        // Player only calls GameCar::SetMoveCar while its car object exists.
        if (runtime.destroyed ||
            results[racer].blockMove ==
                source::PlayerBlockMove::Unblocked ||
            racer >= vehicleInputs_.size())
        {
            continue;
        }
        auto& control = vehicleInputs_[racer];
        control.throttle = 0.0F;
        control.reverse = 0.0F;
        control.steering = 0.0F;
        control.brake =
            results[racer].blockMove == source::PlayerBlockMove::Brake
                ? 1.0F
                : 0.0F;
    }
    return results;
}

void OriginalRaceSession::updateAiTracks(
    const std::vector<r3d::physics::VehicleState>& vehicles)
{
    aiSystemEntriesScratch_.clear();
    for (std::size_t racer = 0U;
         racer < racers_.size() && racer < vehicles.size() &&
         racer < aiPlayers_.size(); ++racer)
    {
        if (!racers_[racer].IsComputer() ||
            (networkGameplayEnabled_ &&
             (racer >= networkOwnedRacers_.size() ||
              !networkOwnedRacers_[racer])) ||
            racers_[racer].destroyed || racers_[racer].GetFinished())
            continue;
        const auto& carState = racers_[racer].car;
        // AISystem::ComputeTracks only inserts AI players whose live
        // CarState::curTile/mapObj exists. PathState's lastNode fallback is
        // intentionally excluded from lane ownership.
        if (carState.GetLiveTile() == nullptr ||
            carState.GetCurNode() == nullptr)
            continue;
        if (auto* aiCar = aiPlayers_[racer].GetCar())
        {
            aiSystemEntriesScratch_.push_back({
                racer, aiCar, &carState,
                vehicles[racer].body.position,
                carState.GetRadius(), true});
        }
    }
    aiSystem_.ComputeTracks(aiSystemEntriesScratch_);
}

r3d::physics::VehicleInput OriginalRaceSession::aiInput(
    std::size_t racer, const r3d::physics::VehicleState& vehicle,
    float seconds)
{
    if (racer >= racers_.size() || racer >= aiPlayers_.size() ||
        racers_[racer].GetFinished() || racers_[racer].destroyed)
        return {};

    const auto& vehicleDefinition = vehicleForRacer(racer);
    source::AICar::VehicleState sourceVehicle;
    sourceVehicle.position = vehicle.body.position;
    sourceVehicle.direction =
        normalized2(forward(vehicle.body.rotation));
    sourceVehicle.direction3 =
        normalized3(forward(vehicle.body.rotation));
    sourceVehicle.speed = vehicle.speed;
    sourceVehicle.size = racers_[racer].car.GetSize();
    sourceVehicle.steeringControl =
        vehicleDefinition.physics.steeringControl;
    sourceVehicle.mapObject = true;
    sourceVehicle.cheatSlower = racers_[racer].car.cheatSlower;

    const auto command = aiPlayers_[racer].OnProgress(
        seconds, sourceVehicle, &sourceRandomUnit);
    r3d::physics::VehicleInput input;
    input.steering = clampSteering(
        command.steeringAngle /
        std::max(vehicleDefinition.physics.steerAngle, 0.01F));
    switch (command.move)
    {
    case source::AICar::MoveCarState::Accelerate:
        input.throttle = 1.0F;
        break;
    case source::AICar::MoveCarState::Brake:
        input.brake = 1.0F;
        break;
    case source::AICar::MoveCarState::Reverse:
        input.reverse = 1.0F;
        break;
    case source::AICar::MoveCarState::None:
        break;
    }
    return input;
}
void OriginalRaceSession::updatePlaces(
    const std::vector<r3d::physics::VehicleState>& vehicles)
{
    std::vector<source::RacePlacePlayer> players;
    players.reserve(racers_.size());
    for (std::size_t racer = 0U; racer < racers_.size(); ++racer)
    {
        const auto& runtime = racers_[racer];
        source::RacePlacePlayer player;
        player.playerId = racer;
        player.disconnected = runtime.disconnected;
        player.finished = runtime.GetFinished();
        player.place = runtime.GetPlace();
        player.lap =
            racer < vehicles.size()
                ? lapPosition(racer, vehicles[racer])
                : runtime.car.GetLap();
        player.lastCorrectLap = lastCorrectLapPosition(racer);
        player.lastCorrectPathLength =
            std::max(runtime.car.GetPathLength(true), 1.0F);
        player.lastCorrectMainPath = runtime.car.IsMainPath(true);
        players.push_back(player);
    }

    const auto sourceUpdate = racePlaceModel_.Update(
        players, !raceLifecycle_.GetResults().empty());
    for (std::size_t place = 0; place < sourceUpdate.order.size(); ++place)
        racers_[sourceUpdate.order[place]].SetPlace(
            static_cast<std::uint32_t>(place + 1U));
    std::uint32_t disconnectedPlace =
        static_cast<std::uint32_t>(sourceUpdate.order.size() + 1U);
    for (auto& racer : racers_)
    {
        if (racer.disconnected)
            racer.SetPlace(disconnectedPlace++);
    }
    for (const auto& sourceEvent : sourceUpdate.events)
    {
        RaceEventKind kind = RaceEventKind::LeadChanged;
        switch (sourceEvent.kind)
        {
        case source::RacePlaceEventKind::LeadChanged:
            kind = RaceEventKind::LeadChanged;
            break;
        case source::RacePlaceEventKind::ThirdChanged:
            kind = RaceEventKind::ThirdChanged;
            break;
        case source::RacePlaceEventKind::LastFar:
            kind = RaceEventKind::LastFar;
            break;
        case source::RacePlaceEventKind::Domination:
            kind = RaceEventKind::Domination;
            break;
        case source::RacePlaceEventKind::ThirdFar:
            kind = RaceEventKind::ThirdFar;
            break;
        }
        const auto racer = sourceEvent.playerId;
        const Vec3 position =
            racer < vehicles.size()
                ? vehicles[racer].body.position
                : Vec3{};
        events_.push_back(
            {kind, racer, sourceEvent.otherPlayerId, position, 0.0F});
    }
}

void OriginalRaceSession::queueRespawn(
    std::size_t racer,
    const std::vector<r3d::physics::VehicleState>& vehicles)
{
    if (racer >= racers_.size() || racer >= vehicles.size())
        return;
    auto& runtime = racers_[racer];
    const auto reset = runtime.ResetCar(
        [&](const source::TraceVec3& position) {
            return raycastResetWorld(
                       race_, decorationActive_, vehicles, racers_,
                       racer, position)
                .kind;
        });
    if (!reset.valid)
        return;

    respawns_.push_back(
        {racer, reset.position, reset.direction});
    previousPositions_[racer] = vehicles[racer].body.position;
    events_.push_back(
        {RaceEventKind::Respawn, racer,
         reset.node.valid() ? reset.node.node : 0U,
         reset.position, 0.0F});
}

void OriginalRaceSession::createRacerMapObject(std::size_t racer)
{
    if (racer >= racers_.size() || racer >= racerMapObjects_.size() ||
        racerMapObjects_[racer] != nullptr)
        return;
    const auto* vehicle = racers_[racer].GetCarRecord();
    if (vehicle == nullptr)
        return;
    auto& mapObject = map_.AddMapObj(
        source::MapObjCategory::Car,
        source::GameObjType::RockCar, vehicle->record, racer);
    mapObject.SetPlayer(&racers_[racer]);
    mapObject.GetGameObj().ResetGameObject(vehicle->maximumLife);
    racerMapObjects_[racer] = &mapObject;
}

void OriginalRaceSession::freeRacerMapObject(
    std::size_t racer) noexcept
{
    if (racer >= racerMapObjects_.size() ||
        racerMapObjects_[racer] == nullptr)
        return;
    map_.DelMapObj(racerMapObjects_[racer]);
    racerMapObjects_[racer] = nullptr;
}

void OriginalRaceSession::destroyRacer(
    std::size_t racer, Vec3 position,
    const r3d::physics::VehicleState& vehicle,
    bool gameObjectAlreadyDestroyed)
{
    if (racer >= racers_.size() || racer >= race_.racers.size() ||
        (racers_[racer].destroyed && !gameObjectAlreadyDestroyed))
        return;
    auto& runtime = racers_[racer];
    // Player::OnDeath/OnDestroy begins the exact cTimeRestoreCar lifecycle.
    runtime.Destroy();
    freeRacerMapObject(racer);
    appendPlayerGameEvents(racer, position, false);
    releaseRacerProjectileReferences(racer);
    if (racer < vehicleInputs_.size())
        vehicleInputs_[racer] = {};
    const auto& definition = vehicleForRacer(racer);
    for (std::size_t index = 0;
         index < definition.deathEffects.size(); ++index)
    {
        const auto& source = definition.deathEffects[index];
        const auto timing = sourceEffectTiming(source.visual, 0.7F);
        RaceEffect effect;
        effect.kind = RaceEventKind::VehicleDestroyed;
        effect.origin = add(vehicle.body.position, source.position);
        effect.target = add(effect.origin, {1.0F, 0.0F, 0.0F});
        applySourceEffectTiming(effect, timing);
        effect.ignoreRotation = source.ignoreRotation;
        effect.racer = racer;
        effect.vehicleEffect = index;
        effect.transform = vehicle.body;
        effect.transform.position = effect.origin;
        if (source.ignoreRotation)
            effect.transform.rotation = {};
        attachSourceLifeEffect(effect, source.visual.soundPaths, racer);
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
        const auto& vehicle = vehicleForRacer(racer);
        return vehicle.physics.clutchImmunity;
    };
    auto installedWeaponSlot =
        [&](std::size_t owner, std::size_t weaponIndex,
            std::optional<std::size_t> primaryMount)
        -> const source::Slot* {
            if (owner >= racers_.size() ||
                weaponIndex >= race_.weapons.size())
                return nullptr;
            std::optional<source::PlayerSlotType> physicalType;
            if (primaryMount &&
                *primaryMount < PlayerProfile::weaponSlotCount)
            {
                physicalType = static_cast<source::PlayerSlotType>(
                    static_cast<std::size_t>(
                        source::PlayerSlotType::Weapon1) +
                    *primaryMount);
            }
            else if (racers_[owner].hyperWeapon == weaponIndex)
            {
                physicalType = source::PlayerSlotType::Hyper;
            }
            else if (racers_[owner].mineWeapon == weaponIndex)
            {
                physicalType = source::PlayerSlotType::Mine;
            }
            if (!physicalType)
                return nullptr;
            const auto* slot =
                racers_[owner].GetSlotInst(*physicalType);
            if (slot == nullptr ||
                recordName(slot->GetItem().GetRecord()) !=
                    recordName(race_.weapons[weaponIndex].record))
                return nullptr;
            return slot;
        };
    auto installedSlotTransform = [](const source::Slot& slot) {
        Transform result;
        const auto& position = slot.GetItem().GetPos();
        const auto& rotation = slot.GetItem().GetRot();
        result.position = {position[0], position[1], position[2]};
        result.rotation = {
            rotation[0], rotation[1], rotation[2], rotation[3]};
        return result;
    };
    auto directWeaponWorldTransform =
        [&](std::size_t owner, std::size_t weaponIndex) {
            Transform result = vehicles[owner].body;
            if (const auto* slot = installedWeaponSlot(
                    owner, weaponIndex, std::nullopt))
                result = compose(result, installedSlotTransform(*slot));
            return compose(
                result, race_.weapons[weaponIndex].visual.transform);
        };
    auto weaponWorldTransform =
        [&](std::size_t owner, std::size_t weaponIndex,
            std::size_t mountSlot) {
            Transform result = vehicles[owner].body;
            const auto& vehicleDefinition = vehicleForRacer(owner);
            if (const auto* slot = installedWeaponSlot(
                    owner, weaponIndex, mountSlot))
            {
                result = compose(result, installedSlotTransform(*slot));
            }
            else
            {
                const std::size_t physicalMount =
                    static_cast<std::size_t>(GarageSlotType::Weapon1) +
                    mountSlot;
                if (physicalMount < vehicleDefinition.slotMounts.size())
                {
                    const auto& mount =
                        vehicleDefinition.slotMounts[physicalMount];
                    Transform local;
                    local.position = mount.position;
                    const auto wanted =
                        recordName(race_.weapons[weaponIndex].record);
                    const auto placement = std::find_if(
                        mount.placements.begin(),
                        mount.placements.end(),
                        [&](const VehicleSlotPlacement& item) {
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
    std::vector<source::Player*> playerList;
    playerList.reserve(racers_.size());
    for (auto& racer : racers_)
        playerList.push_back(&racer);
    auto findClosestEnemy =
        [&](std::size_t source, float viewAngle, bool zTest = false) {
            if (source >= vehicles.size() ||
                source >= racers_.size())
                return RacerRuntime::invalidWeapon;
            const auto* enemy = racers_[source].FindClosestEnemy(
                viewAngle, zTest, playerList);
            return enemy == nullptr
                       ? RacerRuntime::invalidWeapon
                       : static_cast<std::size_t>(
                             enemy - racers_.data());
        };
    for (std::size_t racer = 0; racer < racers_.size(); ++racer)
    {
        auto& runtime = racers_[racer];
        const auto& vehicleDefinition = vehicleForRacer(racer);
        const auto behaviorProgress = runtime.ProgressBehaviors(
            seconds, vehicleDefinition.lowLifeLevel,
            racer < vehicles.size()
                ? length3(vehicles[racer].linearVelocity)
                : 0.0F);
        runtime.speedBoostSeconds =
            std::max(0.0F, runtime.speedBoostSeconds - seconds);
        if (behaviorProgress.slowSpeedLimited &&
            racer < vehicles.size())
        {
            const Vec3 wanted = multiply(
                normalized3(vehicles[racer].linearVelocity),
                source::SlowEffect::maximumSpeed);
            velocityRequests_.push_back(
                {racer,
                 subtract(wanted, vehicles[racer].linearVelocity)});
        }
        if (racer < vehicles.size())
        {
            runtime.gameCar.OnMotor(
                seconds, vehicles[racer].engineRpm,
                vehicleDefinition.physics.idlingRpm,
                vehicleDefinition.physics.maximumRpm);
            const auto wheelCount = std::min(
                runtime.gameCar.GetWheelCount(),
                vehicles[racer].wheelContacts.size());
            for (std::size_t wheel = 0U;
                 wheel < wheelCount; ++wheel)
            {
                const auto& contact =
                    vehicles[racer].wheelContacts[wheel];
                runtime.gameCar.SetWheelContact(
                    wheel, contact.hasContact,
                    contact.longitudinalSlip,
                    contact.lateralSlip);
            }
        }
        runtime.gameCar.OnProgress(seconds);
        if (racer < vehicleInputs_.size())
        {
            vehicleInputs_[racer].springLocked =
                runtime.gameCar.IsSpringLocked();
        }
        if (runtime.destroyed)
            continue;
        if (behaviorProgress.lowLifeActivated)
        {
            events_.push_back(
                {RaceEventKind::LowLife, racer, 0U,
                 racer < vehicles.size()
                     ? vehicles[racer].body.position
                     : Vec3{},
                 runtime.life / runtime.maximumLife});
        }
    }

    auto applyRacerDamage =
        [&](std::size_t target, std::size_t attacker,
            const Vec3& position, float sourceDamage,
            DamageType damageType) {
            if (target >= racers_.size() ||
                target >= vehicles.size() ||
                racers_[target].destroyed)
                return false;
            return applyRacerDamageInternal(
                target, attacker, position, sourceDamage, damageType,
                vehicles[target], false, false, 0.0F, false, false);
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
    constexpr float sourceContactParticleLife = 0.7F;
    const auto pairContactKey = [](
        std::size_t racer,
        r3d::physics::CollisionSurface surface,
        std::uint32_t actor) {
        return source::PairPxContactEffect::Key{
            static_cast<std::uint64_t>(racer),
            (static_cast<std::uint64_t>(surface) << 32U) |
                static_cast<std::uint64_t>(actor)};
    };
    for (std::size_t racer = 0;
         !legacyWindowsDebug_ && racer < vehicles.size() &&
         racer < racers_.size(); ++racer)
    {
        if (racers_[racer].destroyed)
            continue;
        for (const auto& contact : vehicles[racer].bodyContacts)
        {
            if (contact.surface ==
                     r3d::physics::CollisionSurface::Vehicle &&
                contact.otherVehicle < racer)
            {
                continue;
            }
            std::array<source::PairPxContactEffect::Point, 2> points{};
            std::size_t pointCount = std::min<std::size_t>(
                contact.points.size(), points.size());
            for (std::size_t index = 0; index < pointCount; ++index)
            {
                points[index] = {
                    contact.points[index].x,
                    contact.points[index].y,
                    contact.points[index].z};
            }
            if (pointCount == 0U)
            {
                const Vec3 point = contact.hasPoint
                    ? contact.point
                    : vehicles[racer].body.position;
                points[0] = {point.x, point.y, point.z};
                pointCount = 1U;
            }
            const auto contactResult =
                logic_.GetPairPxContactEffect().OnContact(
                    pairContactKey(
                        racer, contact.surface, contact.otherActor),
                    contact.frictionForce, false, false,
                    std::span<const source::PairPxContactEffect::Point>{
                        points.data(), pointCount},
                    sourceRandomUnit());
            if (!contactResult.accepted)
                continue;
            if (contactResult.pairCreated &&
                contactResult.playSound &&
                contactResult.sound < race_.contactSoundPaths.size())
            {
                // PairPxContactEffect::GetOrCreateContact selects one Sound
                // with floor(size * Random()) and keeps that Source3d until
                // the actor-pair node is released. This differs subtly from
                // the RAND_MAX+1 RandomRange used by EventEffect sound lists.
                RaceEvent sound;
                sound.kind = RaceEventKind::EffectSound;
                sound.racer = racer;
                sound.position = {
                    contactResult.points.front().point.x,
                    contactResult.points.front().point.y,
                    contactResult.points.front().point.z};
                sound.soundPath = race_.contactSoundPaths[
                    contactResult.sound];
                sound.soundContactActor = contact.otherActor;
                sound.soundContactSurface = contact.surface;
                events_.push_back(std::move(sound));
            }
            for (const auto& point : contactResult.points)
            {
                auto effect = std::find_if(
                    effects_.begin(), effects_.end(),
                    [&](const RaceEffect& value) {
                        return value.kind ==
                                   RaceEventKind::ContactImpact &&
                               !value.waitingEnd.IsResurrect() &&
                               !value.effectOwner.destroyed &&
                               value.racer == racer &&
                               value.contactSurface == contact.surface &&
                               value.contactActor == contact.otherActor &&
                               value.contactIndex == point.slot;
                    });
                if (effect == effects_.end())
                {
                    RaceEffect created;
                    created.kind = RaceEventKind::ContactImpact;
                    created.racer = racer;
                    created.contactSurface = contact.surface;
                    created.contactActor = contact.otherActor;
                    created.contactIndex = point.slot;
                    created.totalSeconds =
                        source::PairPxContactEffect::
                            contactReleaseSeconds +
                        sourceContactParticleLife;
                    created.seconds = created.totalSeconds;
                    created.ageSeconds = 0.0F;
                    created.emissionEndSeconds =
                        source::PairPxContactEffect::
                            contactReleaseSeconds;
                    created.waitForParticleEnd = true;
                    created.effectOwner.ResetGameObject(-1.0F);
                    created.waitingEnd.Reset();
                    effects_.push_back(std::move(created));
                    effect = std::prev(effects_.end());
                }
                effect->origin = {
                    point.point.x, point.point.y, point.point.z};
                effect->target = add(effect->origin, contact.normal);
                effect->seconds =
                    source::PairPxContactEffect::
                        contactReleaseSeconds +
                    sourceContactParticleLife;
                effect->emissionEndSeconds =
                    effect->ageSeconds +
                    source::PairPxContactEffect::
                        contactReleaseSeconds;
            }
        }
    }
    for (const auto& released :
         logic_.GetPairPxContactEffect().OnProgress(seconds))
    {
        const auto effect = std::find_if(
            effects_.begin(), effects_.end(),
            [&](const RaceEffect& value) {
                if (value.kind != RaceEventKind::ContactImpact ||
                    value.contactIndex != released.slot ||
                    value.waitingEnd.IsResurrect() ||
                    value.effectOwner.destroyed)
                    return false;
                return pairContactKey(
                           value.racer, value.contactSurface,
                           value.contactActor) == released.key;
            });
        if (effect == effects_.end())
            continue;
        effect->emissionEndSeconds = effect->ageSeconds;
        effect->effectOwner.Death();
        effect->waitingEnd.OnDeath(effect->effectOwner);
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
            racers_[racer].gameCar.CancelClutch();
            float forcePart = 0.0F;
            const float damage = damageFromContact(
                logic_.GetTouchBorderDamage(),
                logic_.GetTouchBorderDamageForce(),
                contact.force, forcePart);
            if ((!springBorders_ && forcePart == 0.0F) ||
                vehicles[racer].speed <= 16.0F)
                continue;

            if (springBorders_)
            {
                const Vec3 normal = normalized3(contact.normal);
                const Vec3 velocity = vehicles[racer].linearVelocity;
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
                    Vec3 tangent{};
                    if (tangentDot < 0.995F)
                    {
                        // GameCar::OnContact first uses norm x travel only
                        // to choose the vertical sign.  It then applies that
                        // sign to PhysX sumFrictionForce and crosses the
                        // normalized result with the contact normal.
                        const Vec3 binormal = cross(normal, travel);
                        tangent = contact.frictionForceVector;
                        tangent.z = binormal.z > 0.0F
                                        ? std::abs(tangent.z)
                                        : -std::abs(tangent.z);
                        if (length3(tangent) <= 0.0001F)
                        {
                            // A resting/first-frame Jolt manifold can have
                            // no solved friction impulse. Preserve the same
                            // geometric direction without inventing force.
                            tangent = binormal;
                        }
                        tangent = cross(normalized3(tangent), normal);
                    }
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
    auto spawnProjectileImpact =
        [&](ProjectileRuntime& projectile, const Vec3& position,
            std::size_t targetRacer) {
            const auto* definition = runtimeProjectileDefinition(
                race_, projectile);
            if (definition == nullptr ||
                projectile.weapon >= race_.weapons.size())
                return;
            const bool hasDeathEffect =
                !definition->deathEffect.visual.record.empty() ||
                !definition->deathEffect.visual.visualNodes.empty() ||
                !definition->deathEffect.visual.particleEmitters.empty() ||
                !definition->deathEffect.visual.soundPaths.empty();
            const auto deathPlan = hasDeathEffect
                ? projectile.deathEffect.OnDeath(
                      true, targetRacer < vehicles.size(),
                      projectile.damageOwner < vehicles.size())
                : source::DeathEffect::SpawnResult{};
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
                    const auto timing =
                        sourceEffectTiming(visual, 0.9F);
                    if (visual.visualNodes.empty() &&
                        visual.particleEmitters.empty() &&
                        visual.soundPaths.empty())
                        return;
                    RaceEffect impact;
                    impact.kind = RaceEventKind::ProjectileImpact;
                    impact.origin = effectOrigin;
                    impact.target =
                        add(impact.origin, projectile.direction);
                    applySourceEffectTiming(impact, timing);
                    impact.weapon = projectile.weapon;
                    impact.projectile = projectile.projectile;
                    impact.visualVariant = variant;
                    impact.ignoreRotation = ignoreRotation;
                    if (targetChild && targetRacer < vehicles.size())
                    {
                        impact.parentRacer = targetRacer;
                        impact.transform = effectTransform;
                    }
                    attachSourceLifeEffect(
                        impact, visual.soundPaths, projectile.owner,
                        targetChild && targetRacer < vehicles.size()
                            ? targetRacer
                            : RacerRuntime::invalidWeapon);
                    effects_.push_back(std::move(impact));
                };
            addVisual(definition->secondaryVisual, 1U);
            addVisual(definition->tertiaryVisual, 2U);
            if (deathPlan.createEffect)
            {
                addVisual(definition->deathEffect.visual, 3U,
                          definition->deathEffect.position,
                          definition->deathEffect.ignoreRotation,
                          deathPlan.targetChild);
            }

            if (!deathPlan.createEffect ||
                definition->deathProjectile ==
                    ProjectileDefinition::invalidProjectile ||
                definition->deathProjectile >=
                    race_.weapons[projectile.weapon]
                        .projectiles.size())
                return;
            const auto& spawned =
                race_.weapons[projectile.weapon]
                    .projectiles[definition->deathProjectile];
            if (spawned.type != 20U)
                return;
            MineRuntime crater;
            crater.owner = projectile.owner;
            crater.damageOwner = projectile.damageOwner;
            crater.weapon = projectile.weapon;
            crater.projectile = definition->deathProjectile;
            crater.position = add(position, spawned.position);
            crater.damage = spawned.damage;
            crater.maximumLife = sampleSourceRange(
                spawned.minimumLife, spawned.maximumLife);
            crater.collision = spawned.collision;
            crater.type = spawned.type;
            crater.impulseSpeed = spawned.speed;
            crater.ignoreOwnerCollision =
                deathPlan.ignoreSenderCar;
            mines_.push_back(crater);
        };

    for (auto& projectile : projectiles_)
    {
        if (!projectile.active)
            continue;
        const auto* runtimeDefinition = runtimeProjectileDefinition(
            race_, projectile);
        if (runtimeDefinition == nullptr)
            continue;
        const auto& projectileDefinition = *runtimeDefinition;
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
                          race_, decorationActive_, vehicles, racers_,
                          projectile.owner, rayOrigin,
                          projectile.direction, maximumDistance)
                    : WorldRayHit{};
            const auto laserUpdate = sourceRay
                ? source::Proj::LaserUpdate(
                      maximumDistance, rayHit.hit, rayHit.distance,
                      seconds, projectileDefinition.damage,
                      projectileDefinition.type == 3U,
                      projectile.ageSeconds,
                      projectile.maximumLifeSeconds)
                : source::Proj::LaserUpdateResult{};
            projectile.impactDistance =
                sourceRay ? laserUpdate.distance : 0.0F;
            projectile.beamWidthScale =
                sourceRay ? laserUpdate.beamWidthScale : 1.0F;
            projectile.beamTextureScale =
                sourceRay && laserUpdate.textureScale > 0.0F
                    ? laserUpdate.textureScale
                    : 1.0F;
            const Vec3 end = add(
                projectile.position,
                multiply(projectile.direction,
                         sourceRay ? projectile.impactDistance : 0.0F));
            RaceEffect fired;
            fired.kind = RaceEventKind::WeaponFired;
            fired.origin = projectile.position;
            fired.target = end;
            fired.seconds = std::max(seconds, 0.03F);
            fired.totalSeconds = fired.seconds;
            fired.weapon = projectile.weapon;
            fired.projectile = projectile.projectile;
            effects_.push_back(std::move(fired));
            if (sourceRay &&
                laserUpdate.applyDamage &&
                rayHit.vehicle < vehicles.size() &&
                rayHit.vehicle < racers_.size())
            {
                const std::size_t target = rayHit.vehicle;
                applyRacerDamage(
                    target, projectile.damageOwner, end,
                    std::max(laserUpdate.damage, 0.0F),
                    sourceProjectileDamageType(
                        projectileDefinition.type));
                if (projectileDefinition.type == 18U)
                {
                    const float duration =
                        projectileDefinition.tertiaryVisual
                                    .maximumTimeLife >
                                0.0F
                            ? projectileDefinition.tertiaryVisual
                                  .maximumTimeLife
                            : 1.0F;
                    racers_[target].AttachSlowEffect(
                        duration, projectile.weapon,
                        projectile.projectile);
                }
            }
            else if (sourceRay &&
                     laserUpdate.applyDamage &&
                     rayHit.decoration < decorationActive_.size())
            {
                // RaycastClosestShape reports one concrete PhysX actor.  Do
                // not perform a second broad ray query here: that used to
                // damage a different decoration behind the actual hit.
                damageDecoration(
                    rayHit.decoration,
                    std::max(laserUpdate.damage, 0.0F),
                    projectile.damageOwner);
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
                        racers_[target].destroyed)
                        continue;
                    const auto& vehicleDefinition =
                        vehicleForRacer(target);
                    const OrientedBox targetBox = vehicleBox(
                        vehicles[target],
                        vehicleDefinition.physics);
                    if (!boxesOverlap(projectileBox, targetBox))
                        continue;
                    const Vec3 contactPoint =
                        closestPoint(targetBox, projectileBox.center);
                    const auto contact =
                        projectileDefinition.type == 15U
                            ? source::Proj::DrobilkaContact(
                                  true, projectileDefinition.damage,
                                  seconds)
                            : source::Proj::FireContact(
                                  true, projectileDefinition.damage,
                                  seconds);
                    refreshDrobilkaContact(contactPoint);
                    applyRacerDamage(
                        target, projectile.damageOwner,
                        contactPoint,
                        std::max(contact.damage, 0.0F),
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
                        projectile.damageOwner, &decorationContact))
                {
                    refreshDrobilkaContact(decorationContact);
                }
            }
            const auto decorationContact =
                projectileDefinition.type == 15U
                    ? source::Proj::DrobilkaContact(
                          true, projectileDefinition.damage, seconds)
                    : source::Proj::FireContact(
                          true, projectileDefinition.damage, seconds);
            const float decorationDamage =
                std::max(decorationContact.damage, 0.0F);
            if (sourceContact &&
                     projectileDefinition.type != 15U)
            {
                damageDecorationWithBox(
                    shotTransform, projectileDefinition.collision,
                    decorationDamage, projectile.damageOwner);
            }
            // GameObject::OnProgress expires only after _timeLife becomes
            // strictly greater than _maxTimeLife.
            if (projectile.maximumLifeSeconds > 0.0F &&
                projectile.ageSeconds > projectile.maximumLifeSeconds)
            {
                spawnProjectileImpact(
                    projectile, projectile.position,
                    RacerRuntime::invalidWeapon);
                projectile.active = false;
            }
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
            projectile.target < vehicles.size())
        {
            const bool hasTarget =
                projectile.target < racers_.size() &&
                !racers_[projectile.target].destroyed;
            const auto update = source::Proj::TorpedaUpdate(
                seconds, sourceVec(projectile.position),
                sourceQuat(projectile.rotation),
                sourceVec(projectile.velocity),
                projectile.homingDelay, hasTarget,
                sourceVec(vehicles[projectile.target].body.position),
                projectileDefinition.speed,
                projectileDefinition.relativeSpeed,
                projectile.angularSpeed);
            projectile.homingDelay = update.homingDelay;
            if (update.setLinearVelocity)
            {
                projectile.rotation = runtimeQuat(update.rotation);
                projectile.direction = runtimeVec(update.direction);
                projectile.velocity = runtimeVec(update.linearVelocity);
                projectile.speed = length3(projectile.velocity);
            }
        }
        const Vec3 previous = projectile.position;
        const float speed = std::max(projectile.speed, 1.0F);
        projectile.lifeSeconds -= seconds;
        const bool detachedGravity =
            projectile.detachedFromWeapon &&
            projectileDefinition.type == 15U;
        if (projectile.ballistic || detachedGravity)
            projectile.velocity.z -= 20.0F * seconds;
        Vec3 movement =
            (projectile.ballistic || projectile.detachedFromWeapon)
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
            const auto update = source::Proj::RocketUpdate(
                projectile.position.z, trackHit.position.z,
                projectileDefinition.collision.halfExtents.z,
                projectile.trackClearance, trackHit.hit);
            projectile.position.z = update.positionZ;
            projectile.trackClearance = update.clearance;
        }
        if (projectileDefinition.type == 23U &&
            std::abs(projectileDefinition.angularSpeed) > 0.0001F)
        {
            projectile.rotation = runtimeQuat(source::Proj::ResonanseUpdate(
                sourceQuat(projectile.rotation),
                projectileDefinition.angularSpeed, seconds));
        }
        projectile.reflectionCooldown = source::Proj::ThunderUpdate(
            projectile.reflectionCooldown, seconds);
        if (projectileDefinition.type == 22U &&
            projectile.reflectionCooldown <= 0.0F)
        {
            Transform thunderTransform;
            thunderTransform.position = projectile.position;
            thunderTransform.rotation = projectile.rotation;
            Vec3 normal;
            const bool borderContact =
                length3(projectile.velocity) > 5.0F &&
                trackBorderContact(
                    race_, orientedBox(
                               thunderTransform,
                               projectileDefinition.collision),
                    normal);
            const auto contact = source::Proj::ThunderContact(
                sourceVec(projectile.velocity), sourceVec(normal),
                projectile.reflectionCooldown, borderContact);
            if (contact.setLinearVelocity)
            {
                projectile.velocity = runtimeVec(contact.linearVelocity);
                projectile.direction =
                    normalized3(projectile.velocity);
                // ThunderContact only changes PhysX linear velocity. The
                // projectile actor/model rotation remains the shot rotation
                // (and Resonanse is the only RocketUpdate variant that spins
                // its actor explicitly).
                projectile.reflectionCooldown =
                    contact.reflectionCooldown;
            }
        }
        RaceEffect fired;
        fired.kind = RaceEventKind::WeaponFired;
        fired.origin = previous;
        fired.target = projectile.position;
        fired.seconds = std::max(seconds, 0.03F);
        fired.totalSeconds = fired.seconds;
        fired.weapon = projectile.weapon;
        fired.projectile = projectile.projectile;
        effects_.push_back(std::move(fired));

        Transform projectileTransform;
        projectileTransform.position = projectile.position;
        projectileTransform.rotation = projectile.rotation;
        const OrientedBox projectileBox = orientedBox(
            projectileTransform, projectileDefinition.collision);
        if (!projectile.ownerCollisionArmed &&
            projectile.damageOwner < vehicles.size() &&
            projectile.damageOwner < race_.racers.size())
        {
            const auto& ownerVehicle =
                vehicleForRacer(projectile.damageOwner);
            // PhysX ignores only the projectile/weapon actor pair, not the
            // owning car forever.  Arm owner contacts after the shot has
            // cleared our coarser portable vehicle box, so reflected and
            // homing projectiles can return to their shooter.
            projectile.ownerCollisionArmed = !boxesOverlap(
                projectileBox,
                vehicleBox(
                    vehicles[projectile.damageOwner],
                    ownerVehicle.physics));
        }

        for (std::size_t target = 0;
             target < vehicles.size() && target < racers_.size();
             ++target)
        {
            if ((target == projectile.damageOwner &&
                 !projectile.ownerCollisionArmed) ||
                racers_[target].destroyed)
                continue;
            if (projectileDefinition.type == 21U &&
                projectile.target < racers_.size() &&
                target != projectile.target)
                continue;
            const auto& vehicleDefinition = vehicleForRacer(target);
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
            const auto impulseContact =
                projectileDefinition.type == 21U
                    ? source::Proj::ImpulseContact(
                          true, targetedImpulse,
                          !targetedImpulse ||
                              target == projectile.target,
                          projectile.hitCount,
                          projectileDefinition.damage)
                    : source::Proj::ImpulseContactResult{};
            if (projectileDefinition.type == 21U &&
                !impulseContact.applyDamage)
            {
                continue;
            }
            const auto sonarResult = sonarContact
                ? source::Proj::SonarContact(
                      true, sourceVec(projectile.velocity),
                      projectileDefinition.mass,
                      projectileDefinition.damage, seconds)
                : source::Proj::ContinuousContactResult{};
            const float sourceDamage =
                projectileDefinition.type == 21U
                    ? impulseContact.damage
                    : (sonarContact
                           ? sonarResult.damage
                           : projectile.damage);
            applyRacerDamage(
                target, projectile.damageOwner, contactPoint,
                std::max(sourceDamage, 0.0F),
                sourceProjectileDamageType(
                    projectileDefinition.type));
            if (sonarContact)
            {
                const float targetMass =
                    std::max(vehicleDefinition.physics.mass, 1.0F);
                const Vec3 impulse = runtimeVec(sonarResult.impulse);
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
                const auto torque = source::Proj::RocketContactTorque(
                    sourceVec(contactPoint),
                    sourceVec(projectile.velocity),
                    projectileDefinition.mass);
                if (torque.apply)
                {
                    angularVelocityRequests_.push_back(
                        {target,
                         rotate(
                             vehicles[target].body.rotation,
                             runtimeVec(torque.localVelocityChange))});
                }
            }
            if (sonarContact)
            {
                continue;
            }
            if (projectileDefinition.type == 21U)
            {
                projectile.hitCount = impulseContact.hitCount;
                if (impulseContact.destroy)
                {
                    spawnProjectileImpact(
                        projectile, projectile.position, target);
                    projectile.active = false;
                    break;
                }
                if (!impulseContact.findNextTarget)
                    break;
                std::size_t nextTarget = findClosestEnemy(
                    target, 1.57079632679489661923F);
                if (nextTarget == projectile.damageOwner)
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
                projectile.damage * seconds,
                projectile.damageOwner);
        }
        else if (projectile.active &&
                 !(projectileDefinition.type == 21U &&
                   projectile.target < racers_.size()) &&
                 damageDecorationWithBox(
                     liveProjectileTransform,
                     projectileDefinition.collision,
                     projectile.damage,
                     projectile.damageOwner))
        {
            spawnProjectileImpact(
                projectile, projectile.position,
                RacerRuntime::invalidWeapon);
            projectile.active = false;
        }
        if (projectile.active &&
            projectile.maximumLifeSeconds > 0.0F &&
            projectile.ageSeconds > projectile.maximumLifeSeconds)
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

    if (humanRacer_ < vehicles.size() && humanRacer_ < racers_.size())
    {
        if (humanControl.reset &&
            source::HumanPlayer::ResetCar(
                !racers_[humanRacer_].destroyed,
                vehicles[humanRacer_].contactCount > 0U,
                !vehicles[humanRacer_].bodyContacts.empty()))
            queueRespawn(humanRacer_, vehicles);
        previousPositions_[humanRacer_] =
            vehicles[humanRacer_].body.position;
    }
    for (std::size_t racer = 0;
         racer < vehicles.size() && racer < racers_.size(); ++racer)
    {
        if (racers_[racer].destroyed)
            continue;
        const auto& definition = vehicleForRacer(racer);
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
        if (!map_.GetGroundTouchDeath().OnContact(&racers_[racer]))
            continue;
        destroyRacer(
            racer, vehicles[racer].body.position,
            vehicles[racer], true);
    }
    for (std::size_t racer = 0U;
         racer < vehicles.size() && racer < racers_.size(); ++racer)
    {
        if (racers_[racer].IsComputer() &&
            (!networkGameplayEnabled_ ||
             (racer < networkOwnedRacers_.size() &&
              networkOwnedRacers_[racer])) &&
            racer < aiPlayers_.size() &&
            aiPlayers_[racer].TakeResetCar() &&
            !racers_[racer].destroyed)
            queueRespawn(racer, vehicles);
    }

    auto pushShotEffect =
        [&](std::size_t owner, std::size_t weapon,
            std::size_t soundSource,
            const Transform& weaponTransform,
            const ProjectileDefinition& projectile) {
            if (weapon >= race_.weapons.size())
                return;
            if (owner < racers_.size())
            {
                auto& rack = racers_[owner].GetWeaponRack();
                source::Weapon* sourceWeapon = nullptr;
                if (soundSource < PlayerProfile::weaponSlotCount)
                {
                    sourceWeapon =
                        &rack.primary[soundSource];
                }
                else if (soundSource == PlayerProfile::weaponSlotCount)
                {
                    sourceWeapon = &rack.hyper;
                }
                else if (soundSource ==
                         PlayerProfile::weaponSlotCount + 1U)
                {
                    sourceWeapon = &rack.mine;
                }
                if (sourceWeapon != nullptr)
                    sourceWeapon->OnProjectilePrepared();
            }
            const auto& source = race_.weapons[weapon].shotEffect;
            // Weapon::CreateShot calls Behaviors::OnShot once for every
            // projectile which PrepareProj accepted. ShotEffect then uses
            // GiveSource3d(), whose RandomRange chooses one of the serialized
            // sounds independently of whether a visual effect exists.
            if (!source.soundPaths.empty())
            {
                RaceEvent sound;
                sound.kind = RaceEventKind::EffectSound;
                sound.racer = owner;
                sound.target = weapon;
                sound.position = weaponTransform.position;
                sound.soundPath = source.soundPaths[
                    sourceUniformRandomIndex(
                        source.soundPaths.size(),
                        sourceUniformRandomUnit())];
                sound.soundSource = soundSource;
                events_.push_back(std::move(sound));
            }
            if ((source.visual.visualNodes.empty() &&
                 source.visual.particleEmitters.empty() &&
                 source.visual.soundPaths.empty()) ||
                source.duration <= 0.0F)
                return;
            Transform local;
            local.position = add(projectile.position, source.position);
            RaceEffect effect;
            effect.kind = RaceEventKind::WeaponShotEffect;
            effect.transform = compose(
                weaponTransform, local);
            effect.origin = effect.transform.position;
            effect.target = add(
                effect.origin,
                rotate(effect.transform.rotation,
                       {1.0F, 0.0F, 0.0F}));
            const auto timing = sourceEffectTiming(
                source.visual, source.duration);
            applySourceEffectTiming(effect, timing);
            effect.weapon = weapon;
            effect.ignoreRotation = source.ignoreRotation;
            if (owner < vehicles.size())
            {
                // ShotEffect::EffectDesc::child attaches the spawned actor
                // to the Weapon GameObject.  Store the equivalent car-local
                // pose so it follows the mount until ResurrectObj detaches
                // a waiting particle system at the end of emission.
                effect.transform = relativeTransform(
                    vehicles[owner].body, effect.transform);
                effect.parentRacer = owner;
            }
            attachSourceLifeEffect(
                effect, source.visual.soundPaths, owner, owner);
            effects_.push_back(std::move(effect));
        };

    // Windows obtains these objects from Player::_slot[].  They now remain
    // resident in the active Player instead of being reconstructed for each
    // readiness query and shot transaction.
    auto primaryWeaponItem = [&](std::size_t owner,
                                 std::size_t slot)
        -> source::WeaponItem* {
        if (owner >= racers_.size() ||
            slot >= PlayerProfile::weaponSlotCount)
            return nullptr;
        const auto items = racers_[owner].GetPrimaryWeaponItems();
        return items[slot];
    };
    auto hyperWeaponItem = [&](std::size_t owner)
        -> source::WeaponItem* {
        return owner < racers_.size()
                   ? racers_[owner].GetHyperWeaponItem()
                   : nullptr;
    };
    auto mineWeaponItem = [&](std::size_t owner)
        -> source::WeaponItem* {
        return owner < racers_.size()
                   ? racers_[owner].GetMineWeaponItem()
                   : nullptr;
    };
    auto emitHumanShot = [&](const source::Logic::ShotPlan& plan) {
        if (!plan.humanShotEvent || humanRacer_ >= racers_.size())
            return;
        RaceEvent event;
        event.kind = RaceEventKind::HumanShot;
        event.racer = humanRacer_;
        events_.push_back(std::move(event));
    };

    auto humanPrimaryItems = [&]() {
        return humanRacer_ < racers_.size()
                   ? racers_[humanRacer_].GetPrimaryWeaponItems()
                   : std::array<source::WeaponItem*,
                                PlayerProfile::weaponSlotCount>{};
    };
    if (humanRacer_ < racers_.size() && humanControl.weaponSlot >= 0 &&
        humanControl.weaponSlot <
            static_cast<int>(PlayerProfile::weaponSlotCount))
    {
        const auto slot =
            static_cast<std::size_t>(humanControl.weaponSlot);
        auto items = humanPrimaryItems();
        if (items[slot] != nullptr && items[slot]->IsInstalled())
        {
            humanPlayer_.SetCurWeapon(
                static_cast<int>(slot));
            racers_[humanRacer_].selectedWeaponSlot = slot;
            racers_[humanRacer_].SyncSelectedWeapon(race_.weapons.size());
        }
    }
    if (humanControl.changeWeapon && humanRacer_ < racers_.size())
    {
        auto items = humanPrimaryItems();
        humanPlayer_.ChangeWeapon(
            humanControl.weaponChange, items);
        racers_[humanRacer_].selectedWeaponSlot = static_cast<std::size_t>(
            std::max(humanPlayer_.GetCurWeapon(), 0));
        racers_[humanRacer_].SyncSelectedWeapon(race_.weapons.size());
    }

    auto placeMine = [&](
        std::size_t owner, const Vec3* replicatedPosition = nullptr,
        std::uint32_t replicatedProjectileId = 0U,
        bool networkReplicated = false,
        bool sourceReadinessOverride = false) {
        if (owner >= vehicles.size() || owner >= racers_.size() ||
            racers_[owner].destroyed)
            return;
        const std::size_t weapon = racers_[owner].mineWeapon;
        if (weapon == RacerRuntime::invalidWeapon ||
            weapon >= race_.weapons.size())
            return;
        auto* item = mineWeaponItem(owner);
        if (item == nullptr || !item->IsInstalled())
            return;
        if (!networkReplicated && !sourceReadinessOverride &&
            !item->IsReadyShot())
            return;
        const int newCharge =
            networkReplicated
                ? static_cast<int>(item->GetCurCharge()) - 1
                : -1;
        const auto* liveWeapon = item->GetWeapon();
        const auto description = liveWeapon->GetDescHandle();
        const auto& projectiles = description->projectiles;
        const auto* projectile =
            projectiles.empty() ? nullptr : &projectiles.front();
        if (projectile == nullptr)
        {
            racers_[owner].Shot(
                *item, false, true,
                replicatedProjectileId, newCharge);
            return;
        }
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
        {
            racers_[owner].Shot(
                *item, false, true,
                replicatedProjectileId, newCharge);
            return;
        }
        // Source MinePrepare uses ComputeAABB(true), while CreatePxBox uses
        // ComputeAABB(false).  Using the contact box here lifted the maslo
        // plane by 0.85 m even though its source model offset is only 0.05 m.
        const float offset = projectile->surfacePlacementOffset;
        const Vec3 position =
            replicatedPosition != nullptr
                ? *replicatedPosition
                : add(hit.position, {0.0F, 0.0F, offset});
        const std::uint32_t networkProjectileId =
            networkReplicated && replicatedProjectileId != 0U
                ? replicatedProjectileId
                : racers_[owner].GetNextBonusProjectileId();
        if (!racers_[owner].Shot(
                *item, true, true, networkProjectileId, newCharge))
            return;
        racers_[owner].gameCar.LockMine(0.4F);
        MineRuntime mine;
        mine.owner = owner;
        mine.damageOwner = owner;
        mine.weapon = weapon;
        mine.weaponDescription = description;
        mine.descriptionProjectile = 0U;
        const auto sourceProjectile = std::find_if(
            race_.weapons[weapon].projectiles.begin(),
            race_.weapons[weapon].projectiles.end(),
            [](const ProjectileDefinition& candidate) {
                return !candidate.spawnOnParentDeath;
            });
        mine.projectile = sourceProjectile ==
                                  race_.weapons[weapon].projectiles.end()
                              ? 0U
                              : static_cast<std::size_t>(std::distance(
                                    race_.weapons[weapon].projectiles.begin(),
                                    sourceProjectile));
        mine.position = position;
        mine.rotation = rotationWithUp(hit.normal);
        mine.damage = projectile->damage;
        mine.impulseSpeed = projectile->speed;
        mine.type = projectile->type;
        mine.networkProjectileId = networkProjectileId;
        mine.collision = projectile->collision;
        if (projectile->minimumLife > 0.0F)
        {
            mine.maximumLife = sampleSourceRange(
                projectile->minimumLife,
                projectile->maximumLife);
        }
        pushShotEffect(
            owner, weapon, PlayerProfile::weaponSlotCount + 1U,
            weaponTransform, *projectile);
        mines_.push_back(mine);
        RaceEvent mineEvent;
        mineEvent.kind = RaceEventKind::MinePlaced;
        mineEvent.racer = owner;
        mineEvent.target = weapon;
        mineEvent.position = position;
        mineEvent.networkReplicated = networkReplicated;
        mineEvent.networkSlotMask = 0x02U;
        mineEvent.networkProjectileId = networkProjectileId;
        mineEvent.networkCoordinates.push_back(position);
        events_.push_back(std::move(mineEvent));
    };
    if (humanControl.useMine)
    {
        auto* item = mineWeaponItem(humanRacer_);
        const auto plan = source::Logic::Shot(
            item != nullptr && item->IsInstalled() ? item : nullptr,
            source::Logic::SlotType::Mine, true);
        emitHumanShot(plan);
        if (plan.Get(source::Logic::SlotType::Mine))
            placeMine(humanRacer_, nullptr, 0U, false, true);
    }
    if (humanRacer_ < racers_.size() && humanControl.mineHeld > 0.0F &&
        racers_[humanRacer_].mineWeapon != RacerRuntime::invalidWeapon &&
        racers_[humanRacer_].mineWeapon < race_.weapons.size())
    {
        const bool maslo =
            racers_[humanRacer_].GetWeaponRack().mine.IsMaslo();
        if (humanControl.mineAnalogBinding || maslo)
        {
            const float alpha =
                std::clamp(humanControl.mineHeld, 0.0F, 1.0F);
            const float sourceDelay = (1.0F - alpha) * 0.6F;
            if (racers_[humanRacer_].GetWeaponRack().mine.IsReadyShot(
                    sourceDelay))
            {
                source::Logic::ShotPlan humanShot;
                humanShot.humanShotEvent = true;
                emitHumanShot(humanShot);
                placeMine(humanRacer_, nullptr, 0U, false, true);
            }
        }
    }
    auto activateHyper = [&](
        std::size_t owner, const Vec3* replicatedPosition = nullptr,
        std::uint32_t replicatedProjectileId = 0U,
        bool networkReplicated = false) {
        if (owner >= racers_.size() ||
            racers_[owner].destroyed ||
            racers_[owner].hyperWeapon ==
                RacerRuntime::invalidWeapon ||
            racers_[owner].hyperWeapon >= race_.weapons.size() ||
            (!networkReplicated &&
             !racers_[owner].GetWeaponRack().hyper.IsReadyShot()))
            return;
        auto* item = hyperWeaponItem(owner);
        if (item == nullptr || !item->IsInstalled())
            return;
        const int newCharge =
            networkReplicated
                ? static_cast<int>(item->GetCurCharge()) - 1
                : -1;
        const auto* liveWeapon = item->GetWeapon();
        const auto description = liveWeapon->GetDescHandle();
        const auto& projectiles = description->projectiles;
        if (projectiles.empty())
        {
            racers_[owner].Shot(
                *item, false, false,
                replicatedProjectileId, newCharge);
            return;
        }
        const auto& projectile = projectiles.front();
        if (owner >= vehicles.size())
        {
            racers_[owner].Shot(
                *item, false, false,
                replicatedProjectileId, newCharge);
            return;
        }
        const auto& vehicleDefinition = vehicleForRacer(owner);
        source::Proj::SpringPrepareResult springPreparation;
        if (projectile.type == 17U)
        {
            const auto wheelCount =
                vehicleDefinition.physics.wheels.size();
            springPreparation = source::Proj::SpringPrepare(
                true,
                wheelCount > 0U &&
                    vehicles[owner].contactCount >= wheelCount,
                projectile.speed);
            if (!springPreparation.prepared)
            {
                racers_[owner].Shot(
                    *item, false, false,
                    replicatedProjectileId, newCharge);
                return;
            }
        }
        if (!racers_[owner].Shot(
                *item, true, false,
                replicatedProjectileId, newCharge))
            return;
        const std::uint32_t networkProjectileId =
            networkReplicated && replicatedProjectileId != 0U
                ? replicatedProjectileId
                : racers_[owner].GetNextBonusProjectileId();
        const Vec3 position = vehicles[owner].body.position;
        const float sampledMinimumLife = sampleSourceRange(
            projectile.minimumLife, projectile.maximumLife);
        const float duration = source::Proj::PrepareMaximumLife(
            projectile.speed, projectile.maximumDistance,
            sampledMinimumLife);
        if (projectile.type == 17U)
        {
            velocityRequests_.push_back(
                {owner,
                 rotate(
                     vehicles[owner].body.rotation,
                     runtimeVec(
                         springPreparation.localVelocityChange))});
            if (springPreparation.lockSpring)
                racers_[owner].gameCar.LockSpring();
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
            auto projectileTransform =
                compose(weaponTransform, localProjectile);
            if (replicatedPosition != nullptr)
                projectileTransform.position = *replicatedPosition;
            ProjectileRuntime runtimeProjectile;
            runtimeProjectile.owner = owner;
            runtimeProjectile.damageOwner = owner;
            runtimeProjectile.weapon =
                racers_[owner].hyperWeapon;
            runtimeProjectile.weaponDescription = description;
            runtimeProjectile.descriptionProjectile = 0U;
            const auto& sourceProjectiles =
                race_.weapons[racers_[owner].hyperWeapon].projectiles;
            const auto sourceProjectile = std::find_if(
                sourceProjectiles.begin(), sourceProjectiles.end(),
                [](const ProjectileDefinition& candidate) {
                    return !candidate.spawnOnParentDeath;
                });
            runtimeProjectile.projectile =
                sourceProjectile == sourceProjectiles.end()
                    ? 0U
                    : static_cast<std::size_t>(std::distance(
                          sourceProjectiles.begin(), sourceProjectile));
            runtimeProjectile.position =
                projectileTransform.position;
            runtimeProjectile.direction = normalized3(
                rotate(
                    projectileTransform.rotation,
                    {1.0F, 0.0F, 0.0F}));
            runtimeProjectile.rotation =
                projectileTransform.rotation;
            runtimeProjectile.lifeSeconds = duration;
            runtimeProjectile.maximumLifeSeconds = duration;
            runtimeProjectile.attached = true;
            runtimeProjectile.directWeapon = true;
            runtimeProjectile.deathEffect.Reset(
                projectile.deathEffect.effectPhysicsIgnoreSenderCar,
                projectile.deathEffect.targetChild);
            projectiles_.push_back(runtimeProjectile);
        }
        RaceEvent hyperEvent;
        hyperEvent.kind = RaceEventKind::HyperActivated;
        hyperEvent.racer = owner;
        hyperEvent.target = racers_[owner].hyperWeapon;
        hyperEvent.position = position;
        hyperEvent.value = duration;
        hyperEvent.networkReplicated = networkReplicated;
        hyperEvent.networkSlotMask = 0x01U;
        hyperEvent.networkProjectileId = networkProjectileId;
        hyperEvent.networkCoordinates.push_back(
            replicatedPosition != nullptr
                ? *replicatedPosition
                : weaponTransform.position);
        events_.push_back(std::move(hyperEvent));
        pushShotEffect(
            owner, racers_[owner].hyperWeapon,
            PlayerProfile::weaponSlotCount,
            weaponTransform,
            projectile);
    };
    if (humanControl.useHyper)
    {
        auto* item = hyperWeaponItem(humanRacer_);
        const auto plan = source::Logic::Shot(
            item != nullptr && item->IsInstalled() ? item : nullptr,
            source::Logic::SlotType::Hyper, true);
        if (plan.Get(source::Logic::SlotType::Hyper))
            activateHyper(humanRacer_);
    }

    std::vector<MineRuntime> spawnedMines;
    auto spawnMineDeathEffect = [&](const MineRuntime& mine) {
        const auto* runtimeDefinition = runtimeProjectileDefinition(
            race_, mine);
        if (runtimeDefinition == nullptr)
            return;
        const auto& definition = *runtimeDefinition;
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
        const auto timing =
            sourceEffectTiming(death->visual, 0.7F);
        if (death->visual.visualNodes.empty() &&
            death->visual.particleEmitters.empty() &&
            death->visual.soundPaths.empty())
            return;
        RaceEffect impact;
        impact.kind = RaceEventKind::ProjectileImpact;
        impact.origin = add(mine.position, death->position);
        impact.target = add(impact.origin, {0.0F, 0.0F, 1.0F});
        applySourceEffectTiming(impact, timing);
        impact.weapon = mine.weapon;
        impact.projectile = mine.projectile;
        impact.visualVariant = deathVariant;
        impact.ignoreRotation = death->ignoreRotation;
        attachSourceLifeEffect(
            impact, death->visual.soundPaths, mine.owner);
        effects_.push_back(std::move(impact));
    };
    auto applyMasloContact = [&](
        std::size_t racer, const Vec3& oilPosition, float damage,
        bool arming) {
        if (racer >= vehicles.size() || racer >= racers_.size())
            return false;
        const auto carRight = rotate(
            vehicles[racer].body.rotation, {0.0F, 1.0F, 0.0F});
        const auto sourceResult = source::Proj::MasloContact(
            {vehicles[racer].body.position.x,
             vehicles[racer].body.position.y,
             vehicles[racer].body.position.z},
            {carRight.x, carRight.y, carRight.z},
            {oilPosition.x, oilPosition.y, oilPosition.z},
            {vehicles[racer].linearVelocity.x,
             vehicles[racer].linearVelocity.y,
             vehicles[racer].linearVelocity.z},
            damage, arming,
            racers_[racer].gameCar.IsMineLocked(),
            racers_[racer].gameCar.IsClutchLocked(),
            clutchImmune(racer));
        if (!sourceResult.lockClutch)
            return false;
        if (!racers_[racer].gameCar.LockClutch(
                sourceResult.clutchStrength,
                clutchImmune(racer)))
            return false;
        const auto& vehicleDefinition = vehicleForRacer(racer);
        const Quat inverseRotation{
            -vehicles[racer].body.rotation.x,
            -vehicles[racer].body.rotation.y,
            -vehicles[racer].body.rotation.z,
            vehicles[racer].body.rotation.w};
        Vec3 localMomentum = rotate(
            inverseRotation, vehicles[racer].angularMomentum);
        localMomentum.z = racers_[racer].gameCar
                              .ConsumeClutchStrength() *
                          std::max(
                              vehicleDefinition.physics.mass, 0.0F);
        angularMomentumRequests_.push_back(
            {racer,
             rotate(vehicles[racer].body.rotation, localMomentum)});
        return true;
    };
    auto deactivateMine = [&](MineRuntime& mine) {
        if (!mine.active)
            return;
        if (mine.owner < racers_.size() &&
            mine.networkProjectileId != 0U)
        {
            // Proj destruction notifies Player::OnDestroy, which removes the
            // retained BonusProj listener entry without rewinding the id.
            racers_[mine.owner].RemoveBonusProjectile(
                mine.networkProjectileId);
        }
        mine.active = false;
    };
    auto applyMineContact = [&](MineRuntime& mine, std::size_t racer,
                                const Vec3& contactPoint) {
        if (!mine.active || racer >= vehicles.size() ||
            racer >= racers_.size() || racers_[racer].destroyed)
            return false;
        const auto& vehicleDefinition = vehicleForRacer(racer);
        if (mine.type == 10U)
        {
            return applyMasloContact(
                racer, mine.position, mine.damage,
                mine.armingTime >= 0.0F);
        }
        applyRacerDamage(
            racer, mine.damageOwner, contactPoint,
            std::max(
                mine.type == 20U ? mine.damage * seconds : mine.damage,
                0.0F),
            DamageType::Mine);
        if (mine.type != 20U && mine.impulseSpeed != 0.0F)
        {
            const float targetMass =
                std::max(vehicleDefinition.physics.mass, 1.0F);
            const Vec3 impulse{0.0F, 0.0F, mine.impulseSpeed};
            velocityRequests_.push_back(
                {racer, multiply(impulse, 1.0F / targetMass)});
            const Vec3 lever = subtract(
                contactPoint, vehicles[racer].body.position);
            const Vec3 worldTorque = cross(lever, impulse);
            const Quat inverseRotation{
                -vehicles[racer].body.rotation.x,
                -vehicles[racer].body.rotation.y,
                -vehicles[racer].body.rotation.z,
                vehicles[racer].body.rotation.w};
            const Vec3 localTorque =
                rotate(inverseRotation, worldTorque);
            const Vec3 half = vehicleDefinition.physics.halfExtents;
            const Vec3 inertia{
                targetMass *
                    (half.y * half.y + half.z * half.z) / 3.0F,
                targetMass *
                    (half.x * half.x + half.z * half.z) / 3.0F,
                targetMass *
                    (half.x * half.x + half.y * half.y) / 3.0F};
            const Vec3 localAngularDelta{
                localTorque.x / std::max(inertia.x, 0.001F),
                localTorque.y / std::max(inertia.y, 0.001F),
                localTorque.z / std::max(inertia.z, 0.001F)};
            angularVelocityRequests_.push_back(
                {racer,
                 rotate(vehicles[racer].body.rotation,
                        localAngularDelta)});
        }
        if (mine.type != 20U)
        {
            spawnMineDeathEffect(mine);
            deactivateMine(mine);
        }
        return true;
    };

    std::vector<ReplicatedMineContact> pendingMapMineContacts;
    for (const auto& contact : pendingNetworkMineContacts_)
    {
        if (contact.mapProjectile)
        {
            pendingMapMineContacts.push_back(contact);
            continue;
        }
        const auto found = std::find_if(
            mines_.begin(), mines_.end(),
            [&](const MineRuntime& mine) {
                return mine.active &&
                       mine.owner == contact.projectileOwner &&
                       mine.owner < racers_.size() &&
                       racers_[mine.owner].HasBonusProjectile(
                           contact.projectileId) &&
                       mine.networkProjectileId == contact.projectileId;
            });
        if (found == mines_.end())
            continue;
        found->networkPendingContact = RacerRuntime::invalidWeapon;
        applyMineContact(*found, contact.racer, contact.point);
    }
    for (auto& mine : mines_)
    {
        if (!mine.active)
            continue;
        mine.seconds += seconds;
        if (mine.type == 10U || mine.type == 11U ||
            mine.type == 12U || mine.type == 24U)
        {
            const auto arming = source::Proj::MineUpdate(
                mine.armingTime, seconds);
            mine.armingTime = arming.timer;
            if (arming.visualScale >= 0.0F)
                mine.armingAlpha = arming.visualScale;
        }
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
            deactivateMine(mine);
            continue;
        }
        if (mine.type == 12U)
        {
            const auto* runtimeDefinition = runtimeProjectileDefinition(
                race_, mine);
            if (runtimeDefinition == nullptr)
            {
                deactivateMine(mine);
                continue;
            }
            const auto& projectile = *runtimeDefinition;
            if (source::Proj::MineRipUpdate(
                    mine.seconds, projectile.angularSpeed, false))
            {
                if (projectile.secondaryProjectile.valid)
                {
                    const auto& source =
                        projectile.secondaryProjectile;
                    MineRuntime core = mine;
                    core.owner = RacerRuntime::invalidWeapon;
                    core.damageOwner = RacerRuntime::invalidWeapon;
                    core.linkedToOwner = false;
                    core.type = source.type;
                    core.visualVariant = 1U;
                    core.damage = source.damage;
                    core.impulseSpeed = source.speed;
                    core.seconds = 0.0F;
                    core.armingTime = 0.0F;
                    core.armingAlpha = 0.0F;
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
                        fragment.damageOwner =
                            RacerRuntime::invalidWeapon;
                        fragment.linkedToOwner = false;
                        fragment.type = source.type;
                        fragment.visualVariant = 2U;
                        fragment.damage = source.damage;
                        fragment.impulseSpeed = source.speed;
                        fragment.seconds = 0.0F;
                        fragment.armingTime = -1.0F;
                        fragment.armingAlpha = 1.0F;
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
                deactivateMine(mine);
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
            const bool targetMineLocked =
                racers_[racer].gameCar.IsMineLocked();
            bool sourceContactAllowed = true;
            if (mine.type == 10U)
            {
                sourceContactAllowed =
                    mine.armingTime == -1.0F &&
                    !targetMineLocked;
            }
            else if (mine.type != 20U)
            {
                const bool testsMineLock =
                    mine.type == 11U || mine.type == 12U;
                sourceContactAllowed =
                    source::Proj::MineContactAllowed(
                        true, testsMineLock, enableMineBug_,
                        targetMineLocked, mine.armingTime,
                        mine.linkedToOwner && racer == mine.owner);
            }
            Transform mineTransform;
            mineTransform.position = mine.position;
            mineTransform.rotation = mine.rotation;
            const auto& vehicleDefinition = vehicleForRacer(racer);
            const OrientedBox targetBox =
                vehicleBox(
                    vehicles[racer],
                    vehicleDefinition.physics);
            const OrientedBox mineBox =
                orientedBox(mineTransform, mine.collision);
            if (!sourceContactAllowed ||
                !boxesOverlap(targetBox, mineBox))
                continue;
            const Vec3 contactPoint =
                closestPoint(targetBox, mineBox.center);
            if (networkGameplayEnabled_)
            {
                if (racer >= networkOwnedRacers_.size() ||
                    !networkOwnedRacers_[racer] ||
                    mine.networkPendingContact !=
                        RacerRuntime::invalidWeapon)
                    continue;
                // Logic::MineContact is owned by the contacted NetPlayer.
                // It broadcasts the projectile identity and all peers call
                // Proj::MineContact only after that reliable RPC arrives.
                if (mine.owner == RacerRuntime::invalidWeapon ||
                    mine.networkProjectileId == 0U)
                    continue;
                mine.networkPendingContact = racer;
                RaceEvent contact;
                contact.kind = RaceEventKind::MineContact;
                contact.racer = racer;
                contact.target = mine.owner;
                contact.position = contactPoint;
                contact.networkProjectileId =
                    mine.networkProjectileId;
                events_.push_back(std::move(contact));
                continue;
            }
            applyMineContact(mine, racer, contactPoint);
            if (!mine.active)
                break;
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
        const auto timing = sourceEffectTiming(visual, 0.7F);
        RaceEffect impact;
        impact.kind = RaceEventKind::ProjectileImpact;
        impact.origin = add(
            bonus.transform.position,
            bonus.deathEffect.position);
        impact.target = add(
            bonus.transform.position, {0.0F, 0.0F, 2.0F});
        applySourceEffectTiming(impact, timing);
        impact.weapon = race_.weapons.size();
        impact.bonus = bonusIndex;
        impact.ignoreRotation =
            bonus.deathEffect.ignoreRotation;
        attachSourceLifeEffect(impact, visual.soundPaths);
        effects_.push_back(std::move(impact));
    };
    auto applyMapMineContact = [&](std::size_t bonusIndex,
                                   std::size_t racer,
                                   const Vec3& contactPoint) {
        if (bonusIndex >= race_.bonuses.size() ||
            bonusIndex >= bonusActive_.size() ||
            !bonusActive_[bonusIndex] ||
            racer >= racers_.size() || racer >= vehicles.size() ||
            racers_[racer].destroyed)
            return false;
        const auto& bonus = race_.bonuses[bonusIndex];
        const auto bonusRules =
            source::Proj::GetTypeRules(bonus.projectileType);
        if (bonus.kind != BonusKind::MineHazard ||
            (bonusRules.mineTestsLock && enableMineBug_ &&
             racers_[racer].gameCar.IsMineLocked()))
            return false;
        const auto& vehicleDefinition = vehicleForRacer(racer);
        applyRacerDamage(
            racer, RacerRuntime::invalidWeapon, contactPoint,
            std::max(bonus.value, 0.0F), DamageType::Mine);
        spawnBonusDeathEffect(bonusIndex);
        const float mass =
            std::max(vehicleDefinition.physics.mass, 1.0F);
        if (bonus.speed > 0.0F)
        {
            const Vec3 impulse{0.0F, 0.0F, bonus.speed};
            velocityRequests_.push_back(
                {racer, multiply(impulse, 1.0F / mass)});
            const Vec3 lever = subtract(
                contactPoint, vehicles[racer].body.position);
            const Vec3 worldTorque = cross(lever, impulse);
            const Quat inverseRotation{
                -vehicles[racer].body.rotation.x,
                -vehicles[racer].body.rotation.y,
                -vehicles[racer].body.rotation.z,
                vehicles[racer].body.rotation.w};
            const Vec3 localTorque =
                rotate(inverseRotation, worldTorque);
            const Vec3 half = vehicleDefinition.physics.halfExtents;
            const Vec3 inertia{
                mass * (half.y * half.y + half.z * half.z) / 3.0F,
                mass * (half.x * half.x + half.z * half.z) / 3.0F,
                mass * (half.x * half.x + half.y * half.y) / 3.0F};
            const Vec3 localAngularDelta{
                localTorque.x / std::max(inertia.x, 0.001F),
                localTorque.y / std::max(inertia.y, 0.001F),
                localTorque.z / std::max(inertia.z, 0.001F)};
            angularVelocityRequests_.push_back(
                {racer,
                 rotate(vehicles[racer].body.rotation,
                        localAngularDelta)});
        }
        bonusActive_[bonusIndex] = false;
        if (auto* object = bonusObjects().Get(bonusIndex))
            object->GetGameObj().Death(DamageType::Mine);
        return true;
    };

    for (const auto& contact : pendingMapMineContacts)
    {
        const auto bonus = bonusForMapObjectId(contact.projectileId);
        if (bonus == RacerRuntime::invalidWeapon ||
            bonus >= bonusNetworkPendingContact_.size())
            continue;
        bonusNetworkPendingContact_[bonus] =
            RacerRuntime::invalidWeapon;
        applyMapMineContact(bonus, contact.racer, contact.point);
    }
    pendingNetworkMineContacts_.clear();

    auto takeBonus = [&](
        std::size_t racer, std::size_t bonusIndex, BonusKind kind,
        float value, bool networkReplicated) {
        if (racer >= racers_.size() ||
            bonusIndex >= race_.bonuses.size() ||
            bonusIndex >= bonusActive_.size() ||
            bonusObjects().Get(bonusIndex) == nullptr ||
            !bonusActive_[bonusIndex] || racers_[racer].destroyed)
            return false;
        auto& runtime = racers_[racer];
        const auto sourceBonus = source::Proj::BonusContact(
            race_.bonuses[bonusIndex].projectileType, true, value,
            runtime.maximumLife);
        const float sourceValue =
            sourceBonus.take ? sourceBonus.value : value;
        source::PlayerBonusType sourceType;
        switch (kind)
        {
        case BonusKind::Money:
            sourceType = source::PlayerBonusType::Money;
            break;
        case BonusKind::Ammunition:
            sourceType = source::PlayerBonusType::Charge;
            break;
        case BonusKind::Medpack:
            sourceType = source::PlayerBonusType::Medpack;
            break;
        case BonusKind::Shield:
            sourceType = source::PlayerBonusType::Immortal;
            break;
        case BonusKind::Speed:
        case BonusKind::SlowHazard:
        case BonusKind::OilHazard:
        case BonusKind::MineHazard:
        case BonusKind::Unknown:
            return false;
        }
        std::vector<std::uint32_t> weaponMaximumCharges;
        weaponMaximumCharges.reserve(race_.weapons.size());
        for (const auto& weapon : race_.weapons)
            weaponMaximumCharges.push_back(weapon.maximumCharge);
        // The Windows code advances rand() only for the ammunition branch.
        // Consuming it for money/medpack/shield changes every later AI and
        // gameplay random decision in the shared source sequence.
        const float bonusRandomUnit =
            sourceType == source::PlayerBonusType::Charge
                ? sourceRandomUnit()
                : 0.0F;
        auto* bonusObject = bonusObjects().Get(bonusIndex);
        const auto result = source::Logic::TakeBonus(
            &runtime, &bonusObject->GetGameObj(), sourceType,
            sourceValue, weaponMaximumCharges, bonusRandomUnit);
        if (!result.taken)
            return false;
        PickSlot pickSlot = PickSlot::None;
        switch (result.player.slot)
        {
        case source::PlayerBonusSlot::Primary:
            pickSlot = PickSlot::Primary;
            break;
        case source::PlayerBonusSlot::Hyper:
            pickSlot = PickSlot::Hyper;
            break;
        case source::PlayerBonusSlot::Mine:
            pickSlot = PickSlot::Mine;
            break;
        case source::PlayerBonusSlot::None:
            break;
        }
        bonusActive_[bonusIndex] =
            !bonusObject->GetGameObj().destroyed;
        spawnBonusDeathEffect(bonusIndex);
        RaceEvent event;
        event.kind = RaceEventKind::Bonus;
        event.racer = racer;
        event.target = bonusIndex;
        event.position = race_.bonuses[bonusIndex].transform.position;
        event.value = sourceValue;
        event.pickSlot = pickSlot;
        event.networkReplicated = networkReplicated;
        events_.push_back(std::move(event));
        return true;
    };

    for (const auto& bonus : pendingNetworkBonuses_)
    {
        takeBonus(
            bonus.racer, bonus.bonus, bonus.kind,
            bonus.value, true);
    }
    pendingNetworkBonuses_.clear();

    for (std::size_t bonusIndex = 0;
         bonusIndex < race_.bonuses.size(); ++bonusIndex)
    {
        if (!bonusActive_[bonusIndex])
            continue;
        for (std::size_t racer = 0;
             racer < vehicles.size() && racer < racers_.size(); ++racer)
        {
            auto& runtime = racers_[racer];
            if ((networkGameplayEnabled_ &&
                 (racer >= networkOwnedRacers_.size() ||
                  !networkOwnedRacers_[racer])) ||
                runtime.destroyed)
                continue;
            const auto& bonus = race_.bonuses[bonusIndex];
            const auto& vehicleDefinition = vehicleForRacer(racer);
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
                const Vec3 direction =
                    forward(bonus.transform.rotation);
                const auto sourceResult =
                    source::Proj::SpeedArrowContact(
                        {direction.x, direction.y, direction.z},
                        bonus.value);
                const Vec3 wanted{
                    sourceResult.linearVelocity.x,
                    sourceResult.linearVelocity.y,
                    sourceResult.linearVelocity.z};
                velocityRequests_.push_back(
                    {racer,
                     subtract(wanted, vehicles[racer].linearVelocity)});
                events_.push_back(
                    {RaceEventKind::SpeedArrow, racer, bonusIndex,
                     contactPoint, bonus.value});
                continue;
            }
            if (bonus.kind == BonusKind::SlowHazard)
            {
                const auto& velocity =
                    vehicles[racer].linearVelocity;
                const auto sourceResult =
                    source::Proj::LushaContact(
                        {velocity.x, velocity.y, velocity.z},
                        bonus.value);
                if (sourceResult.setLinearVelocity)
                {
                    const Vec3 wanted{
                        sourceResult.linearVelocity.x,
                        sourceResult.linearVelocity.y,
                        sourceResult.linearVelocity.z};
                    velocityRequests_.push_back(
                        {racer,
                         subtract(wanted,
                                  vehicles[racer].linearVelocity)});
                }
                continue;
            }
            if (bonus.kind == BonusKind::OilHazard)
            {
                applyMasloContact(
                    racer, bonus.transform.position,
                    bonus.value,
                    bonusObjects().Get(bonusIndex) != nullptr &&
                        bonusObjects().Get(bonusIndex)
                            ->GetAutoProj() != nullptr &&
                        bonusObjects().Get(bonusIndex)
                            ->GetAutoProj()->IsArming());
                continue;
            }
            if (bonus.kind == BonusKind::MineHazard)
            {
                const auto bonusRules =
                    source::Proj::GetTypeRules(
                        bonus.projectileType);
                if (bonusRules.mineTestsLock && enableMineBug_ &&
                    runtime.gameCar.IsMineLocked())
                    continue;
                if (networkGameplayEnabled_)
                {
                    if (bonusIndex >=
                            bonusNetworkPendingContact_.size() ||
                        bonusNetworkPendingContact_[bonusIndex] !=
                            RacerRuntime::invalidWeapon)
                        continue;
                    bonusNetworkPendingContact_[bonusIndex] = racer;
                    RaceEvent contact;
                    contact.kind = RaceEventKind::MineContact;
                    contact.racer = racer;
                    contact.target = bonusIndex;
                    contact.position = contactPoint;
                    contact.networkMapObject = true;
                    contact.networkProjectileId = bonus.mapObjectId;
                    events_.push_back(std::move(contact));
                    continue;
                }
                applyMapMineContact(bonusIndex, racer, contactPoint);
                break;
            }

            takeBonus(
                racer, bonusIndex, bonus.kind,
                bonus.value, false);
            break;
        }
    }

    auto fireWeapon =
        [&](std::size_t shooter,
            std::size_t requestedTarget =
                RacerRuntime::invalidWeapon,
            const Vec3* replicatedOrigin = nullptr,
            std::uint32_t replicatedProjectileId = 0U,
            bool networkReplicated = false,
            bool sourceReadinessOverride = false) {
        if (shooter >= vehicles.size() ||
            shooter >= racers_.size() ||
            racers_[shooter].GetFinished() ||
            racers_[shooter].destroyed)
            return;
        auto& runtime = racers_[shooter];
        runtime.SyncSelectedWeapon(race_.weapons.size());
        if (runtime.selectedWeapon == RacerRuntime::invalidWeapon ||
            runtime.selectedWeapon >= race_.weapons.size() ||
            runtime.selectedWeaponSlot >= PlayerProfile::weaponSlotCount)
            return;
        const std::size_t firedSlot = runtime.selectedWeaponSlot;
        const std::size_t firedWeapon = runtime.selectedWeapon;
        auto* item = primaryWeaponItem(shooter, firedSlot);
        if (item == nullptr || !item->IsInstalled())
            return;
        if (!networkReplicated && !sourceReadinessOverride &&
            !item->IsReadyShot())
            return;
        const auto* weapon =
            &race_.weapons[firedWeapon];
        if (weapon->slot == WeaponSlot::Support)
            return;
        const auto* liveWeapon = item->GetWeapon();
        const auto shotDescription = liveWeapon->GetDescHandle();
        const auto& itemProjectiles =
            shotDescription->projectiles;
        const bool projectileCreated = !itemProjectiles.empty();
        const int newCharge =
            networkReplicated
                ? static_cast<int>(item->GetCurCharge()) - 1
                : -1;
        if (!runtime.Shot(
                *item, projectileCreated, false,
                replicatedProjectileId, newCharge))
            return;
        const std::uint32_t networkProjectileId =
            networkReplicated && replicatedProjectileId != 0U
                ? replicatedProjectileId
                : runtime.GetNextBonusProjectileId();
        runtime.SyncSelectedWeapon(race_.weapons.size());
        const Vec3 eventOrigin = weaponWorldTransform(
            shooter, firedWeapon, firedSlot).position;
        std::size_t target = racers_.size();
        std::vector<Vec3> networkCoordinates;
        std::size_t sourceProjectileIndex = 0U;
        for (std::size_t projectileIndex = 0;
             projectileIndex < itemProjectiles.size();
             ++projectileIndex)
        {
            const auto& projectile =
                itemProjectiles[projectileIndex];
            while (sourceProjectileIndex < weapon->projectiles.size() &&
                   weapon->projectiles[sourceProjectileIndex]
                       .spawnOnParentDeath)
                ++sourceProjectileIndex;
            const std::size_t backendProjectileIndex =
                sourceProjectileIndex < weapon->projectiles.size()
                    ? sourceProjectileIndex
                    : projectileIndex;
            ++sourceProjectileIndex;
            auto shotTransform = projectileWorldTransform(
                shooter, firedWeapon, firedSlot, projectile);
            if (replicatedOrigin != nullptr)
                shotTransform.position = *replicatedOrigin;
            const Vec3 projectileOrigin = shotTransform.position;
            if (networkCoordinates.empty())
                networkCoordinates.push_back(projectileOrigin);
            const Vec3 sourceDirection = normalized3(
                rotate(shotTransform.rotation,
                       {1.0F, 0.0F, 0.0F}));
            // Proj::CalcSpeed levels only projectiles prepared through
            // RocketPrepare. Laser/FrostRay/Drobilka keep the weapon actor's
            // full 3D direction; applying CalcSpeed to them changed both ray
            // hits and visible beam alignment on slopes.
            const auto projectileRules =
                source::Proj::GetTypeRules(projectile.type);
            const bool rocketPrepared =
                projectileRules.rocketPrepare;
            const auto sourceLaunch = source::Proj::CalcSpeed(
                sourceVec(sourceDirection),
                sourceVec(vehicles[shooter].linearVelocity),
                projectile.speed, projectile.relativeSpeedMinimum,
                projectile.relativeSpeed);
            const Vec3 launchDirection =
                rocketPrepared
                    ? runtimeVec(sourceLaunch.direction)
                    : sourceDirection;
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
            const bool rayProjectile = projectileRules.ray;
            const bool attachedProjectile =
                projectileRules.attached;
            const float projectileDistance =
                projectile.maximumDistance > 0.0F
                    ? projectile.maximumDistance
                    : 100.0F;
            float targetDistance = projectileDistance;
            std::size_t projectileTarget = racers_.size();
            std::size_t projectileDecoration =
                RacerRuntime::invalidWeapon;
            if (rayProjectile && !attachedProjectile)
            {
                const auto rayHit = raycastWorld(
                    race_, decorationActive_, vehicles, racers_, shooter,
                    add(projectileOrigin, projectile.sizeAddPx),
                    sourceDirection, projectileDistance);
                if (rayHit.hit)
                {
                    targetDistance = rayHit.distance;
                    projectileTarget = rayHit.vehicle;
                    projectileDecoration = rayHit.decoration;
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
                runtimeProjectile.damageOwner = shooter;
                runtimeProjectile.weapon = firedWeapon;
                runtimeProjectile.projectile = backendProjectileIndex;
                runtimeProjectile.weaponDescription = shotDescription;
                runtimeProjectile.descriptionProjectile =
                    projectileIndex;
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
                const float sampledMinimumLife = sampleSourceRange(
                    projectile.minimumLife,
                    projectile.maximumLife);
                runtimeProjectile.maximumLifeSeconds =
                    source::Proj::PrepareMaximumLife(
                        projectile.speed, projectile.maximumDistance,
                        sampledMinimumLife);
                runtimeProjectile.lifeSeconds =
                    runtimeProjectile.maximumLifeSeconds;
                runtimeProjectile.attached = true;
                runtimeProjectile.deathEffect.Reset(
                    projectile.deathEffect.effectPhysicsIgnoreSenderCar,
                    projectile.deathEffect.targetChild);
                projectiles_.push_back(runtimeProjectile);
            }
            else if (!rayProjectile)
            {
                const float speed =
                    rocketPrepared
                        ? sourceLaunch.speed
                        : projectile.speed;
                ProjectileRuntime runtimeProjectile;
                runtimeProjectile.owner = shooter;
                runtimeProjectile.damageOwner = shooter;
                runtimeProjectile.weapon = firedWeapon;
                runtimeProjectile.projectile = backendProjectileIndex;
                runtimeProjectile.weaponDescription = shotDescription;
                runtimeProjectile.descriptionProjectile =
                    projectileIndex;
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
                const float sampledMinimumLife = sampleSourceRange(
                    projectile.minimumLife, projectile.maximumLife);
                runtimeProjectile.maximumLifeSeconds =
                    source::Proj::PrepareMaximumLife(
                        projectile.speed, projectile.maximumDistance,
                        sampledMinimumLife);
                runtimeProjectile.lifeSeconds =
                    runtimeProjectile.maximumLifeSeconds;
                runtimeProjectile.ballistic =
                    projectileRules.ballistic;
                runtimeProjectile.deathEffect.Reset(
                    projectile.deathEffect.effectPhysicsIgnoreSenderCar,
                    projectile.deathEffect.targetChild);
                if (projectileRules.homing)
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
            else if (projectileDecoration < decorationActive_.size())
            {
                damageDecoration(
                    projectileDecoration,
                    std::max(projectile.damage, 0.0F), shooter);
            }
            RaceEffect fired;
            fired.kind = RaceEventKind::WeaponFired;
            fired.origin = projectileOrigin;
            fired.target = end;
            fired.seconds =
                (rayProjectile || attachedProjectile) ? 0.12F : 0.03F;
            fired.totalSeconds = fired.seconds;
            fired.weapon = firedWeapon;
            fired.projectile = backendProjectileIndex;
            effects_.push_back(std::move(fired));
            pushShotEffect(
                shooter, firedWeapon, firedSlot,
                weaponWorldTransform(
                    shooter, firedWeapon, firedSlot),
                projectile);
        }
        RaceEvent shotEvent;
        shotEvent.kind = RaceEventKind::WeaponFired;
        shotEvent.racer = shooter;
        shotEvent.target = target;
        shotEvent.position =
            networkCoordinates.empty()
                ? eventOrigin
                : networkCoordinates.front();
        shotEvent.value = 5.0F;
        shotEvent.weapon = firedWeapon;
        shotEvent.networkReplicated = networkReplicated;
        shotEvent.networkSlotMask = static_cast<std::uint8_t>(
            1U << (firedSlot + 2U));
        shotEvent.networkProjectileId = networkProjectileId;
        shotEvent.networkCoordinates =
            std::move(networkCoordinates);
        events_.push_back(std::move(shotEvent));
    };
    if (humanControl.useWeapon && humanRacer_ < racers_.size())
    {
        auto& runtime = racers_[humanRacer_];
        auto items = humanPrimaryItems();
        humanPlayer_.SetCurWeapon(
            static_cast<int>(runtime.selectedWeaponSlot));
        const auto selection = humanPlayer_.SelectWeapon(items);
        runtime.selectedWeaponSlot = selection.slot;
        runtime.SyncSelectedWeapon(race_.weapons.size());
        if (selection.found)
        {
            auto* item = items[selection.slot];
            const auto slotType =
                static_cast<source::Logic::SlotType>(
                    static_cast<std::size_t>(
                        source::Logic::SlotType::Weapon1) +
                    selection.slot);
            const auto plan = source::Logic::Shot(
                item, slotType, true);
            emitHumanShot(plan);
            if (plan.Get(slotType))
            {
                fireWeapon(humanRacer_, RacerRuntime::invalidWeapon,
                           nullptr, 0U, false, true);
                if (item->GetCurCharge() == 0U)
                {
                    const auto next = humanPlayer_.SelectWeapon(items);
                    runtime.selectedWeaponSlot = next.slot;
                    runtime.SyncSelectedWeapon(
                        race_.weapons.size());
                }
            }
        }
    }
    if (humanControl.fireWeaponSlot >= 0 &&
        humanRacer_ < racers_.size() &&
        humanControl.fireWeaponSlot <
            static_cast<int>(PlayerProfile::weaponSlotCount))
    {
        auto& runtime = racers_[humanRacer_];
        const auto requestedOrdinal =
            static_cast<std::size_t>(humanControl.fireWeaponSlot);
        auto items = humanPrimaryItems();
        const std::size_t requested =
            humanPlayer_.GetWeaponByIndex(
                static_cast<int>(requestedOrdinal), items);
        if (requested < items.size())
        {
            const auto selected = runtime.selectedWeaponSlot;
            runtime.selectedWeaponSlot = requested;
            runtime.SyncSelectedWeapon(race_.weapons.size());
            auto* item = primaryWeaponItem(humanRacer_, requested);
            const auto slotType =
                static_cast<source::Logic::SlotType>(
                    static_cast<std::size_t>(
                        source::Logic::SlotType::Weapon1) + requested);
            const auto plan = source::Logic::Shot(
                item != nullptr && item->IsInstalled() ? item : nullptr,
                slotType, true);
            emitHumanShot(plan);
            if (plan.Get(slotType))
                fireWeapon(humanRacer_, RacerRuntime::invalidWeapon,
                           nullptr, 0U, false, true);
            runtime.selectedWeaponSlot = selected;
            runtime.SyncSelectedWeapon(race_.weapons.size());
        }
    }
    if (humanControl.useAllWeapons && humanRacer_ < racers_.size())
    {
        auto& runtime = racers_[humanRacer_];
        const auto selected = runtime.selectedWeaponSlot;
        auto items = humanPrimaryItems();
        const auto plan = source::Logic::ShotAll(items, true);
        emitHumanShot(plan);
        for (std::size_t slot = 0;
             slot < runtime.weaponSlots.size(); ++slot)
        {
            const auto slotType =
                static_cast<source::Logic::SlotType>(
                    static_cast<std::size_t>(
                        source::Logic::SlotType::Weapon1) + slot);
            if (!plan.Get(slotType))
                continue;
            runtime.selectedWeaponSlot = slot;
            runtime.SyncSelectedWeapon(race_.weapons.size());
            fireWeapon(humanRacer_, RacerRuntime::invalidWeapon,
                       nullptr, 0U, false, true);
        }
        runtime.selectedWeaponSlot = selected;
        runtime.SyncSelectedWeapon(race_.weapons.size());
    }
    for (const auto& shot : pendingNetworkShots_)
    {
        if (shot.racer >= racers_.size() ||
            shot.racer >= vehicles.size())
            continue;
        auto& runtime = racers_[shot.racer];
        const auto selected = runtime.selectedWeaponSlot;
        std::size_t coordinateIndex = 0U;
        for (std::size_t bit = 0U; bit < 6U; ++bit)
        {
            if ((shot.slotMask & (1U << bit)) == 0U)
                continue;
            const Vec3* origin =
                coordinateIndex < shot.coordinates.size()
                    ? &shot.coordinates[coordinateIndex]
                    : nullptr;
            ++coordinateIndex;
            // ShotSlots preserves the Windows bitfield order:
            // Hyper, Mine, stWeapon1..stWeapon4.
            if (bit == 0U)
            {
                activateHyper(
                    shot.racer, origin, shot.projectileId, true);
                continue;
            }
            if (bit == 1U)
            {
                placeMine(
                    shot.racer, origin, shot.projectileId, true);
                continue;
            }
            const std::size_t slot = bit - 2U;
            if (slot >= runtime.weaponSlots.size() ||
                runtime.weaponSlots[slot] ==
                    RacerRuntime::invalidWeapon)
                continue;
            runtime.selectedWeaponSlot = slot;
            runtime.SyncSelectedWeapon(race_.weapons.size());
            fireWeapon(
                shot.racer, shot.target, origin,
                shot.projectileId, true);
        }
        runtime.selectedWeaponSlot = selected;
        runtime.SyncSelectedWeapon(race_.weapons.size());
    }
    pendingNetworkShots_.clear();
    auto& attackTargets = aiAttackTargetsScratch_;
    attackTargets.resize(racers_.size());
    for (std::size_t target = 0U; target < racers_.size(); ++target)
    {
        source::AICar::AttackTarget state;
        state.active =
            target < vehicles.size() &&
            !racers_[target].destroyed &&
            !racers_[target].disconnected;
        if (target < vehicles.size())
            state.position = vehicles[target].body.position;
        state.radius = racers_[target].car.GetRadius();
        state.size = racers_[target].car.GetSize();
        attackTargets[target] = state;
    }
    for (std::size_t racer = 0U;
         racer < racers_.size() && racer < vehicles.size(); ++racer)
    {
        auto& runtime = racers_[racer];
        // AICar::AttackState::Update returns before changing retained targets
        // or RNG state while the source CarState has no live curTile.
        if (!racers_[racer].IsComputer() ||
            (networkGameplayEnabled_ &&
             (racer >= networkOwnedRacers_.size() ||
              !networkOwnedRacers_[racer])) ||
            runtime.destroyed ||
            racer >= aiPlayers_.size() ||
            runtime.car.GetLiveTile() == nullptr)
        {
            continue;
        }

        const auto& vehicleDefinition = vehicleForRacer(racer);
        source::AICar::VehicleState sourceVehicle;
        sourceVehicle.position = vehicles[racer].body.position;
        sourceVehicle.direction =
            normalized2(forward(vehicles[racer].body.rotation));
        sourceVehicle.direction3 =
            normalized3(forward(vehicles[racer].body.rotation));
        sourceVehicle.speed = vehicles[racer].speed;
        sourceVehicle.size = runtime.car.GetSize();
        sourceVehicle.steeringControl =
            vehicleDefinition.physics.steeringControl;
        sourceVehicle.mapObject = true;

        std::array<source::AICar::AttackWeapon,
                   PlayerProfile::weaponSlotCount> attackWeapons{};
        const auto primaryItems = runtime.GetPrimaryWeaponItems();
        std::size_t attackWeaponCount = 0U;
        for (std::size_t slot = 0U;
             slot < runtime.weaponSlots.size(); ++slot)
        {
            const std::size_t weaponIndex =
                runtime.weaponSlots[slot];
            if (weaponIndex == RacerRuntime::invalidWeapon ||
                weaponIndex >= race_.weapons.size())
            {
                continue;
            }
            const auto* item = primaryItems[slot];
            if (item == nullptr)
                continue;
            const auto* liveWeapon = item->GetWeapon();
            if (liveWeapon == nullptr)
                continue;
            const auto& description = liveWeapon->GetDesc();
            const auto& projectile = description.Front();
            source::AICar::AttackWeapon state;
            state.slot = slot;
            state.projectileType = projectile.type;
            state.maximumDistance = projectile.maximumDistance;
            state.capacity = item->GetCntCharge();
            state.charge = item->GetCurCharge();
            state.ready = liveWeapon->IsReadyShot(
                std::max(description.shotDelay, 0.25F));
            attackWeapons[attackWeaponCount++] = state;
        }

        source::AICar::AttackContext context;
        context.owner = racer;
        context.targets = attackTargets;
        context.weapons = std::span<const source::AICar::AttackWeapon>(
            attackWeapons.data(), attackWeaponCount);
        context.enabled = true;
        context.randomSource = &sourceRandomUnit;
        context.uniformRandomSource = &sourceUniformRandomUnit;
        if (runtime.hyperWeapon != RacerRuntime::invalidWeapon &&
            runtime.hyperWeapon < race_.weapons.size())
        {
            const auto* item = runtime.GetHyperWeaponItem();
            const auto* liveWeapon =
                item != nullptr ? item->GetWeapon() : nullptr;
            context.hyper.installed = liveWeapon != nullptr;
            if (liveWeapon != nullptr)
            {
                context.hyper.projectileSpeed =
                    liveWeapon->GetDesc().Front().speed;
                context.hyper.capacity = item->GetCntCharge();
                context.hyper.charge = item->GetCurCharge();
            }
        }
        if (runtime.mineWeapon != RacerRuntime::invalidWeapon &&
            runtime.mineWeapon < race_.weapons.size())
        {
            const auto* item = runtime.GetMineWeaponItem();
            const auto* liveWeapon =
                item != nullptr ? item->GetWeapon() : nullptr;
            context.mine.installed = liveWeapon != nullptr;
            if (liveWeapon != nullptr)
            {
                context.mine.oil =
                    liveWeapon->GetDesc().Front().type ==
                    source::AutoProj::masloType;
                context.mine.capacity = item->GetCntCharge();
                context.mine.charge = item->GetCurCharge();
            }
        }

        const auto decision = aiPlayers_[racer].UpdateAttack(
            sourceVehicle, context);
        if (decision.hasWeaponShot())
        {
            runtime.selectedWeaponSlot = decision.weaponSlot;
            runtime.SyncSelectedWeapon(race_.weapons.size());
            fireWeapon(racer, decision.weaponTarget);
        }
        if (decision.useHyper)
            activateHyper(racer);
        if (decision.useMine)
            placeMine(racer);
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
            float forcePart = 0.0F;
            const float damage = damageFromContact(
                logic_.GetTouchCarDamage(),
                logic_.GetTouchCarDamageForce(),
                contact.force, forcePart);
            if (forcePart <= 0.0F || damage <= 0.0F)
                continue;
            auto kineticEnergy = [&](std::size_t racer) {
                if (std::isfinite(vehicles[racer].kineticEnergy) &&
                    vehicles[racer].kineticEnergy >= 0.0F)
                {
                    return vehicles[racer].kineticEnergy;
                }
                const auto& definition = vehicleForRacer(racer);
                const float translational =
                    0.5F * definition.physics.mass *
                    dot3(vehicles[racer].linearVelocity,
                         vehicles[racer].linearVelocity);
                const Vec3 localMomentum = rotate(
                    {-vehicles[racer].body.rotation.x,
                     -vehicles[racer].body.rotation.y,
                     -vehicles[racer].body.rotation.z,
                     vehicles[racer].body.rotation.w},
                    vehicles[racer].angularMomentum);
                const Vec3 extent = definition.physics.halfExtents;
                const float inertiaX = std::max(
                    definition.physics.mass / 3.0F *
                        (extent.y * extent.y + extent.z * extent.z),
                    0.0001F);
                const float inertiaY = std::max(
                    definition.physics.mass / 3.0F *
                        (extent.x * extent.x + extent.z * extent.z),
                    0.0001F);
                const float inertiaZ = std::max(
                    definition.physics.mass / 3.0F *
                        (extent.x * extent.x + extent.y * extent.y),
                    0.0001F);
                const float rotational = 0.5F *
                    (localMomentum.x * localMomentum.x / inertiaX +
                     localMomentum.y * localMomentum.y / inertiaY +
                     localMomentum.z * localMomentum.z / inertiaZ);
                return translational + rotational;
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
        if (racers_[racer].destroyed)
            continue;
        for (const auto& contact : vehicles[racer].bodyContacts)
        {
            if (contact.surface !=
                    r3d::physics::CollisionSurface::Decoration ||
                contact.otherDecoration >=
                    race_.decorationInstances.size() ||
                contact.otherDecoration >= decorationActive_.size())
                continue;
            const auto& instance =
                race_.decorationInstances[contact.otherDecoration];
            if (instance.definition >=
                    race_.decorationDefinitions.size() ||
                !race_.decorationDefinitions[instance.definition]
                     .destructible)
                continue;
            // GameCar::OnContact calls target->Damage(playerId, 0, dtTouch)
            // for every non-car GameObject. A gotDestrObj has maxLife == 0,
            // so that zero-value contact is immediately lethal even though
            // its NX_AF_DISABLE_RESPONSE actor never blocks the car.
            damageDecoration(contact.otherDecoration, 0.0F, racer);
        }
    }
}

void OriginalRaceSession::updateAchievements(float seconds)
{
    const std::size_t sourceEventCount = events_.size();
    std::vector<source::AchievmentEvent> sourceEvents;
    sourceEvents.reserve(sourceEventCount);
    for (std::size_t index = 0U; index < sourceEventCount; ++index)
    {
        const auto& event = events_[index];
        source::AchievmentEvent sourceEvent;
        switch (event.kind)
        {
        case RaceEventKind::Bonus:
            if (event.target >= race_.bonuses.size())
                continue;
            sourceEvent.kind = source::AchievmentEventKind::Bonus;
            sourceEvent.playerId = event.racer;
            sourceEvent.bonusKind = race_.bonuses[event.target].kind;
            {
                const auto& sourceRecord =
                    race_.bonuses[event.target].record;
                sourceEvent.bonusTotalCount =
                    static_cast<std::uint32_t>(std::count_if(
                        race_.bonuses.begin(), race_.bonuses.end(),
                        [&](const BonusInstance& bonus) {
                            return bonus.record == sourceRecord;
                        }));
            }
            break;
        case RaceEventKind::Kill:
            if (!event.killCredit)
                continue;
            sourceEvent.kind = source::AchievmentEventKind::Kill;
            sourceEvent.playerId = event.racer;
            sourceEvent.targetPlayerId = event.target;
            sourceEvent.damageType = event.damageType;
            break;
        case RaceEventKind::Damage:
            sourceEvent.kind = source::AchievmentEventKind::Damage;
            sourceEvent.playerId = event.target;
            sourceEvent.targetPlayerId = event.racer;
            sourceEvent.value = event.value;
            sourceEvent.damageType = event.damageType;
            break;
        case RaceEventKind::Lap:
            sourceEvent.kind = source::AchievmentEventKind::Lap;
            sourceEvent.playerId = event.racer;
            break;
        case RaceEventKind::RaceFinish:
            sourceEvent.kind = source::AchievmentEventKind::RaceFinish;
            sourceEvent.hasPlayer = false;
            break;
        case RaceEventKind::Death:
            sourceEvent.kind = source::AchievmentEventKind::Death;
            sourceEvent.playerId = event.racer;
            sourceEvent.targetPlayerId = event.target;
            sourceEvent.damageType = event.damageType;
            break;
        default:
            continue;
        }
        sourceEvents.push_back(sourceEvent);
    }

    source::AchievmentRaceState raceState;
    raceState.lapCount = race_.lapCount;
    raceState.playerCount = static_cast<std::uint32_t>(std::count_if(
        racers_.begin(), racers_.end(),
        [](const RacerRuntime& racer) {
            return !racer.disconnected;
        }));
    if (humanRacer_ < racers_.size())
    {
        raceState.humanPlace = racers_[humanRacer_].GetPlace();
        raceState.humanLaps = racers_[humanRacer_].car.numLaps;
    }
    for (const std::size_t achievement : achievementModel_.Process(
             seconds, sourceEvents, raceState))
    {
        if (achievement >= race_.achievements.size())
            continue;
        events_.push_back(
            {RaceEventKind::Achievement, humanRacer_, achievement, {},
             static_cast<float>(race_.achievements[achievement].reward)});
    }
}

void OriginalRaceSession::completeRemainingRacers(
    const std::vector<r3d::physics::VehicleState>& vehicles)
{
    std::vector<source::RaceLifecyclePlayer> players;
    players.reserve(racers_.size());
    for (std::size_t racer = 0U; racer < racers_.size(); ++racer)
    {
        source::RaceLifecyclePlayer player;
        player.playerId = racer;
        player.human = racers_[racer].IsHuman();
        player.opponent = racers_[racer].IsOpponent();
        player.disconnected = racers_[racer].disconnected;
        player.finished = racers_[racer].GetFinished();
        player.laps = racers_[racer].car.numLaps;
        player.lapPosition =
            racer < vehicles.size()
                ? lapPosition(racer, vehicles[racer])
                : lastCorrectLapPosition(racer);
        player.pickedMoney = racers_[racer].GetPickMoney();
        players.push_back(player);
    }
    const auto completed = raceLifecycle_.CompleteRemaining(
        std::move(players), race_.lapCount,
        race_.rewardMoney, race_.rewardPoints);
    std::size_t finishOffset = 0U;
    for (const auto& result : completed)
    {
        const float finishTime =
            elapsedSeconds_ + static_cast<float>(finishOffset++) * 0.001F;
        completeRacer(result, finishTime);
    }
    applyCampaignRewards();
}

void OriginalRaceSession::completeRacer(
    const source::RaceResult& result, float finishTime) noexcept
{
    const std::size_t racer = result.playerId;
    if (racer >= racers_.size())
        return;

    auto& runtime = racers_[racer];
    runtime.Complete(
        result.place, result.money, result.points, finishTime);
    // Race::CompleteRace captures this value in Result and immediately calls
    // Player::ResetPickMoney. Finish UI and network use RaceLifecycle::Result.
    runtime.ResetPickMoney();
    // Race::CompleteRace always follows SetFinished/SetPlace with this
    // finite control block. Player::OnProgress owns the countdown and then
    // leaves the surviving physical car under mcBrake.
    runtime.SetBlockTime(source::Player::finishBlockSeconds);
    if (racer < vehicleInputs_.size())
        vehicleInputs_[racer] = {};

    // AIPlayer::FreeCar deletes AICar only. The Player::GameCar and its
    // renderer/physics actor remain in the race and are governed by the
    // block state above. A normal Windows build has no AIPlayer for the
    // human; the separately selectable legacy debug build does.
    if (racer < aiPlayers_.size() &&
        (racer >= racers_.size() || racers_[racer].IsComputer() ||
         legacyWindowsDebug_))
    {
        aiPlayers_[racer].FreeCar();
    }
}

void OriginalRaceSession::applyCampaignRewards() noexcept
{
    if (!campaign_ || campaignRewardsApplied_)
        return;
    for (const auto& result : raceLifecycle_.GetResults())
    {
        if (result.playerId >= racers_.size())
            continue;
        auto& runtime = racers_[result.playerId];
        if (runtime.disconnected || !runtime.GetFinished())
            continue;
        runtime.AddMoney(static_cast<std::int32_t>(
            result.money + result.pickedMoney));
        runtime.AddPoints(static_cast<std::int32_t>(result.points));
    }
    campaignRewardsApplied_ = true;
}

void OriginalRaceSession::completeRaceForExit(
    const std::vector<r3d::physics::VehicleState>& vehicles)
{
    // Race::ExitRace unconditionally begins with CompleteRace(results), even
    // when the local human accepted HudMenu's exit dialog before finishing.
    // Jolt actor and bgfx scene destruction remain backend boundaries, but
    // Race's AI/Player/object graph teardown belongs to this source owner.
    completeRemainingRacers(vehicles);
    updatePlaces(vehicles);
    if (raceRunState_.ExitRace(racers_))
    {
        for (auto& aiPlayer : aiPlayers_)
            aiPlayer.FreeCar();
        for (auto& player : racers_)
            player.ClearBonusProjectiles();
        achievementModel_.ResetRaceState();

        // World::Logic::CleanGameObjs and Map::Clear destroy these source
        // objects on Windows. Keeping them in the portable session allowed
        // stale effects and sound owners to survive behind FinishMenu.
        effects_.clear();
        mines_.clear();
        projectiles_.clear();
        respawns_.clear();
        velocityRequests_.clear();
        angularVelocityRequests_.clear();
        angularMomentumRequests_.clear();
        pendingNetworkShots_.clear();
        pendingNetworkBonuses_.clear();
        pendingNetworkMineContacts_.clear();
        std::fill(decorationActive_.begin(), decorationActive_.end(), false);
        std::fill(bonusActive_.begin(), bonusActive_.end(), false);
        map_.Clear();
        std::fill(
            racerMapObjects_.begin(), racerMapObjects_.end(), nullptr);
        std::fill(vehicleInputs_.begin(), vehicleInputs_.end(),
                  r3d::physics::VehicleInput{});
        logic_.ResetContactBehavior(race_.contactSoundPaths.size());
    }
    phase_ = RacePhase::Finished;
    phaseBeforePause_ = phase_;
    gameModeRaceState_.FinishImmediately();
}

void OriginalRaceSession::update(
    float seconds,
    const std::vector<r3d::physics::VehicleState>& vehicles,
    const RaceControl& humanControl)
{
    seconds = std::clamp(seconds, 0.0F, 7.0F / 60.0F);
    events_.clear();
    std::fill(vehicleInputs_.begin(), vehicleInputs_.end(),
              r3d::physics::VehicleInput{});
    if (phase_ == RacePhase::Paused)
        return;
    // Weapon is a registered GameObject in Windows, so _shotTime advances
    // during the visible countdown as well as during active racing.
    for (auto& player : racers_)
        player.GetWeaponRack().OnProgress(seconds);
    // AutoProj is a registered GameObject before GoRace. Its MineUpdate
    // therefore advances during the visible countdown even though race time
    // itself has not started. This is most visible on ptMaslo, whose model
    // grows from scale zero during the original 0.25-second arming window.
    // Logic.cpp progresses exactly Decoration::_specialList, then Effects,
    // Car, Bonus and finally its separately registered transient objects.
    logic_.OnProgress(seconds);
    for (std::size_t index = 0U;
         index < bonusObjects().GetSlotCount(); ++index)
    {
        if (index >= bonusActive_.size() || !bonusActive_[index])
            continue;
        const auto* mapObject = bonusObjects().Get(index);
        if (mapObject == nullptr || mapObject->GetAutoProj() == nullptr)
            continue;
        if (index < bonusScales_.size())
        {
            bonusScales_[index] =
                mapObject->GetAutoProj()->GetModelScale();
        }
    }
    for (auto& effect : effects_)
    {
        effect.seconds -= seconds;
        effect.ageSeconds += seconds;
        if (effect.lifeEffect.OnProgress(
                !effect.lifeSoundPaths.empty()))
        {
            RaceEvent sound;
            sound.kind = RaceEventKind::EffectSound;
            sound.racer = effect.lifeSoundRacer;
            sound.position = effect.origin;
            if (effect.parentRacer < vehicles.size())
            {
                sound.position = compose(
                    vehicles[effect.parentRacer].body,
                    effect.transform).position;
            }
            sound.soundPath = effect.lifeSoundPaths[
                sourceUniformRandomIndex(
                    effect.lifeSoundPaths.size(),
                    sourceUniformRandomUnit())];
            sound.soundLifetimeSeconds =
                std::max(effect.seconds, 0.001F);
            sound.soundFollowRacer =
                effect.parentRacer < vehicles.size()
                    ? effect.parentRacer
                    : effect.lifeSoundFollowRacer;
            events_.push_back(std::move(sound));
        }
        if (effect.waitForParticleEnd &&
            effect.kind != RaceEventKind::ContactImpact &&
            !effect.waitingEnd.IsResurrect() &&
            effect.emissionEndSeconds >= 0.0F &&
            effect.ageSeconds > effect.emissionEndSeconds)
        {
            effect.effectOwner.Death();
            const auto transition =
                effect.waitingEnd.OnDeath(effect.effectOwner);
            if (transition.beginFading &&
                effect.parentRacer < vehicles.size())
            {
                // ResurrectObj::Resurrect removes a child MapObj from its
                // include list and reinserts it into the world while keeping
                // the current world pose. Preserve that source transition at
                // the backend-neutral RaceEffect boundary.
                effect.transform = compose(
                    vehicles[effect.parentRacer].body,
                    effect.transform);
                effect.origin = effect.transform.position;
                effect.target = add(
                    effect.origin,
                    rotate(effect.transform.rotation,
                           {1.0F, 0.0F, 0.0F}));
                effect.detachedSourceVelocity =
                    vehicles[effect.parentRacer].linearVelocity;
                effect.parentRacer = RacerRuntime::invalidWeapon;
            }
        }
        if (effect.waitForParticleEnd && effect.seconds <= 0.0F)
            effect.waitingEnd.OnProgress(effect.effectOwner, 0U);
    }
    effects_.erase(
        std::remove_if(effects_.begin(), effects_.end(),
                       [](const RaceEffect& effect) {
                           if (effect.waitForParticleEnd)
                           {
                               return effect.waitingEnd.IsResurrect() &&
                                      effect.effectOwner.destroyed;
                           }
                           return effect.seconds <= 0.0F;
                       }),
        effects_.end());
    const auto gameModeAdvance = gameModeRaceState_.OnFrame(seconds);
    if (phase_ == RacePhase::Countdown)
    {
        // Windows Race::OnFixedStep progresses Player state throughout the
        // countdown; only AISystem is gated by GoRace.
        progressPlayers(seconds, vehicles);
        if (gameModeAdvance.countdownStage)
            events_.push_back(
                {RaceEventKind::CountdownChanged, 0, 0, {},
                 gameModeRaceState_.CountdownSeconds()});
        if (gameModeAdvance.raceStarted)
        {
            const auto humanPosition = std::find_if(
                racers_.begin(), racers_.end(),
                [](const source::Player& player) {
                    return player.IsHuman();
                });
            source::Player* human =
                humanPosition == racers_.end() ? nullptr : &*humanPosition;
            raceRunState_.GoRace(human);
            phase_ = RacePhase::Racing;
        }
        return;
    }

    const bool finishTimerRunning =
        phase_ == RacePhase::Finished &&
        (gameModeRaceState_.IsFinishTimerRunning() ||
         gameModeAdvance.finishTimeEnded);
    if (phase_ == RacePhase::Finished && !finishTimerRunning)
    {
        progressPlayers(seconds, vehicles);
        return;
    }

    RaceControl sourceHumanControl = humanControl;
    const bool humanCarPresent =
        humanRacer_ < racers_.size() && humanRacer_ < vehicles.size() &&
        !racers_[humanRacer_].destroyed &&
        !racers_[humanRacer_].disconnected;
    const auto humanGate = source::HumanPlayer::EvaluateControl(
        humanRacer_ >= racers_.size() || racers_[humanRacer_].IsBlock(),
        humanCarPresent, humanControl.chatMode,
        debugHumanAiControl_);
    if (!humanGate.driving)
        sourceHumanControl.driving = {};
    if (!humanGate.inputActions)
    {
        sourceHumanControl.useWeapon = false;
        sourceHumanControl.useAllWeapons = false;
        sourceHumanControl.useMine = false;
        sourceHumanControl.changeWeapon = false;
        sourceHumanControl.weaponSlot = -1;
        sourceHumanControl.fireWeaponSlot = -1;
        sourceHumanControl.reset = false;
    }
    if (!humanGate.progressWeapons)
    {
        sourceHumanControl.mineHeld = 0.0F;
        sourceHumanControl.useHyper = false;
    }

    elapsedSeconds_ += seconds;
    if (humanRacer_ < vehicleInputs_.size() &&
        humanRacer_ < racers_.size() &&
        !racers_[humanRacer_].GetFinished())
    {
        vehicleInputs_[humanRacer_] = sourceHumanControl.driving;
        if (racers_[humanRacer_].speedBoostSeconds > 0.0F)
            vehicleInputs_[humanRacer_].throttle = 1.0F;
    }
    const auto playerProgress = progressPlayers(seconds, vehicles);

    updateAiTracks(vehicles);
    for (std::size_t racer = 0U;
         racer < racers_.size() && racer < vehicles.size(); ++racer)
    {
        if (racers_[racer].IsComputer() &&
            racer < aiPlayers_.size() &&
            aiPlayers_[racer].HasCar() &&
            (!networkGameplayEnabled_ ||
             (racer < networkOwnedRacers_.size() &&
              networkOwnedRacers_[racer])))
        {
            vehicleInputs_[racer] =
                aiInput(racer, vehicles[racer], seconds);
        }
    }
    if (debugHumanAiControl_ && humanRacer_ < vehicleInputs_.size() &&
        humanRacer_ < aiPlayers_.size() &&
        aiPlayers_[humanRacer_].HasCar() &&
        !racers_[humanRacer_].GetFinished() &&
        !racers_[humanRacer_].destroyed &&
        humanRacer_ < vehicles.size())
    {
        // AIDebug F7 flips AICar::_enbAI for the human car. Reuse the same
        // portable AICar path controller as opponents instead of creating a
        // synthetic racer or a second physics vehicle.
        vehicleInputs_[humanRacer_] =
            aiInput(humanRacer_, vehicles[humanRacer_], seconds);
    }

    for (std::size_t racer = 0U;
         racer < playerProgress.size() &&
         racer < vehicleInputs_.size(); ++racer)
    {
        if (playerProgress[racer].cheat.faster)
        {
            vehicleInputs_[racer].motorTorqueScale =
                playerProgress[racer].cheat.torqueScale;
            vehicleInputs_[racer].lateralGripScale =
                playerProgress[racer].cheat.steeringScale;
        }
    }

    updateGameplay(seconds, vehicles, sourceHumanControl);
    updatePlaces(vehicles);
    updateAchievements(seconds);
    if (phase_ == RacePhase::Finished &&
        gameModeRaceState_.IsFinishPresentationReady())
    {
        completeRemainingRacers(vehicles);
        updatePlaces(vehicles);
    }
}

void OriginalRaceSession::setDebugHumanAiControl(bool enabled) noexcept
{
    debugHumanAiControl_ = enabled;
    if (humanRacer_ < aiPlayers_.size())
        aiPlayers_[humanRacer_].SetEnabled(enabled);
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
            source::Player::RoundedRandomIndex(4U, 0.0F) != 0U ||
            source::Player::RoundedRandomIndex(4U, 0.16F) != 0U ||
            source::Player::RoundedRandomIndex(4U, 0.5F) != 2U ||
            source::Player::RoundedRandomIndex(4U, 1.0F) != 3U ||
            source::Player::BonusCharge(3U, 0.5F) != 1U ||
            source::Player::BonusCharge(6U, 0.5F) != 3U ||
            source::Player::BonusCharge(10U, 0.0F) != 1U)
        {
            throw std::runtime_error(
                "source RandomRange/Player::TakeBonus formula failed");
        }
        OriginalRaceSession session(race);
        const auto sourceTransformMatches = [](
            const source::GameObject& object,
            const Transform& transform) {
            const auto& position = object.GetPos();
            const auto& scale = object.GetScale();
            const auto& rotation = object.GetRot();
            return std::abs(position[0] - transform.position.x) < 0.001F &&
                   std::abs(position[1] - transform.position.y) < 0.001F &&
                   std::abs(position[2] - transform.position.z) < 0.001F &&
                   std::abs(scale[0] - transform.scale.x) < 0.001F &&
                   std::abs(scale[1] - transform.scale.y) < 0.001F &&
                   std::abs(scale[2] - transform.scale.z) < 0.001F &&
                   std::abs(rotation[0] - transform.rotation.x) < 0.001F &&
                   std::abs(rotation[1] - transform.rotation.y) < 0.001F &&
                   std::abs(rotation[2] - transform.rotation.z) < 0.001F &&
                   std::abs(rotation[3] - transform.rotation.w) < 0.001F;
        };
        const auto verifyPlacedProxy = [&](const auto& instance) {
            const auto* object = session.sourceMap().GetMapObj(
                instance.mapObjectId, true);
            return object != nullptr &&
                   sourceTransformMatches(
                       object->GetGameObj(), instance.transform);
        };
        if ((!race.decorationInstances.empty() &&
             !verifyPlacedProxy(race.decorationInstances.front())) ||
            (!race.trackInstances.empty() &&
             !verifyPlacedProxy(race.trackInstances.front())) ||
            (!race.bonuses.empty() &&
             !verifyPlacedProxy(race.bonuses.front())))
        {
            throw std::runtime_error(
                "GameObject::LoadProxy placement transform was not bound");
        }
        for (std::size_t racer = 0U;
             racer < session.racers().size(); ++racer)
        {
            const auto* vehicle =
                session.racers()[racer].GetCarRecord();
            if (vehicle == nullptr)
            {
                throw std::runtime_error(
                    "Player::SetCar active vehicle record was not bound");
            }
            if (std::abs(
                    session.racers()[racer].car.GetSize() -
                    vehicle->boundingSize) > 0.001F ||
                std::abs(
                    session.racers()[racer].car.GetRadius() -
                    vehicle->boundingRadius) > 0.001F)
            {
                throw std::runtime_error(
                    "Player::ComputeCarBBSize visual bounds owner mismatch");
            }
        }
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
        const auto droidDefinition = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return weapon.itemType == WeaponItemType::Droid;
            });
        const auto reflectorDefinition = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return weapon.itemType == WeaponItemType::Reflector;
            });
        if (droidDefinition == race.weapons.end() ||
            reflectorDefinition == race.weapons.end() ||
            droidDefinition->record != "droid" ||
            reflectorDefinition->record != "reflector" ||
            std::abs(droidDefinition->repairPeriod - 5.0F) > 0.001F ||
            std::abs(droidDefinition->repairValue - 5.0F) > 0.001F ||
            std::abs(reflectorDefinition->reflectValue - 0.4F) > 0.001F)
        {
            throw std::runtime_error(
                "source DroidItem/ReflectorItem workshop types were not "
                "loaded");
        }
        {
            OriginalRaceSession supportSession(race, true);
            PlayerProfile supportProfile;
            auto& droidSlot = supportProfile.slots[
                PlayerProfile::firstWeaponSlot];
            droidSlot.record = "droid";
            droidSlot.charge = 1U;
            droidSlot.hasCharge = true;
            auto& reflectorSlot = supportProfile.slots[
                PlayerProfile::firstWeaponSlot + 1U];
            reflectorSlot.record = "reflector";
            reflectorSlot.charge = 1U;
            reflectorSlot.hasCharge = true;
            supportSession.applyPlayerProfile(supportProfile);
            const auto& supportPlayer =
                supportSession.racers().front();
            const auto* droid = dynamic_cast<const source::DroidItem*>(
                supportPlayer.GetSlotInst(source::SlotType::Droid) == nullptr
                    ? nullptr
                    : &supportPlayer
                           .GetSlotInst(source::SlotType::Droid)
                           ->GetItem());
            const auto* reflector =
                dynamic_cast<const source::ReflectorItem*>(
                    supportPlayer.GetSlotInst(
                        source::SlotType::Reflector) == nullptr
                        ? nullptr
                        : &supportPlayer
                               .GetSlotInst(source::SlotType::Reflector)
                               ->GetItem());
            if (droid == nullptr || reflector == nullptr ||
                std::abs(supportPlayer.ReflectDamage(100.0F) - 60.0F) >
                    0.001F)
            {
                throw std::runtime_error(
                    "source Player physical support slots were not bound");
            }

            auto supportVehicles = vehicles;
            for (std::size_t index = 1U;
                 index < supportVehicles.size(); ++index)
            {
                supportVehicles[index].body.position.x +=
                    static_cast<float>(index) * 100.0F;
            }
            const float maximumLife =
                supportSession.racers().front().maximumLife;
            supportSession.applyNetworkPlayerDamage(
                0U, supportVehicles.size() > 1U ? 1U : 0U,
                supportVehicles.front().body.position, 20.0F,
                DamageType::Simple, supportVehicles.front());
            const float damagedLife =
                supportSession.racers().front().life;
            if (std::abs(damagedLife - (maximumLife - 20.0F)) > 0.001F)
            {
                throw std::runtime_error(
                    "source DroidItem smoke damage setup failed");
            }
            RaceControl supportInput;
            for (int frame = 0; frame < 44; ++frame)
            {
                supportSession.update(
                    7.0F / 60.0F, supportVehicles, supportInput);
            }
            if (std::abs(supportSession.racers().front().life -
                         (damagedLife + 5.0F)) > 0.001F)
            {
                throw std::runtime_error(
                    "source DroidItem registered progress did not heal");
            }
        }
        RaceControl input;
        input.driving.throttle = 1.0F;
        OriginalRaceSession windowsDebugSession(race, true);
        windowsDebugSession.update(0.1F, vehicles, input);
        if (windowsDebugSession.countdownStage() != 4 ||
            windowsDebugSession.countdownSeconds() != 0.0F ||
            windowsDebugSession.phase() != RacePhase::Racing ||
            windowsDebugSession.racers().front().IsBlock() ||
            windowsDebugSession.vehicleInputs().front().throttle < 0.9F)
        {
            throw std::runtime_error(
                "Windows DEBUG_PX immediate cGoRace was not preserved");
        }
        if (session.countdownStage() != 0 ||
            session.countdownSeconds() != 4.0F ||
            !session.racers().front().IsBlock())
        {
            throw std::runtime_error(
                "offline cGoRaceWait source stage was not initialized");
        }
        session.setPaused(true);
        session.update(0.1F, vehicles, input);
        if (session.phase() != RacePhase::Paused ||
            !session.effectsMuted() ||
            session.countdownSeconds() != 4.0F)
        {
            throw std::runtime_error(
                "GameMode::Pause world/effects state mismatch");
        }
        session.setPaused(false);
        if (session.phase() != RacePhase::Countdown ||
            session.effectsMuted())
        {
            throw std::runtime_error(
                "GameMode::Pause effects restore mismatch");
        }
        for (int frame = 0; frame < 11; ++frame)
            session.update(0.1F, vehicles, input);
        if (session.countdownStage() != 1 ||
            session.phase() != RacePhase::Countdown ||
            session.vehicleInputs().front().throttle != 0.0F ||
            session.vehicleInputs().front().brake < 0.9F ||
            !session.racers().front().IsBlock())
        {
            throw std::runtime_error(
                "offline cGoRace1 source stage was not applied");
        }
        for (int frame = 0; frame < 10; ++frame)
            session.update(0.1F, vehicles, input);
        if (session.countdownStage() != 2 ||
            session.phase() != RacePhase::Countdown)
        {
            throw std::runtime_error(
                "offline cGoRace2 source stage was not applied");
        }
        for (int frame = 0; frame < 10; ++frame)
            session.update(0.1F, vehicles, input);
        if (session.countdownStage() != 3 ||
            session.phase() != RacePhase::Countdown)
        {
            throw std::runtime_error(
                "offline cGoRace3 source stage was not applied");
        }
        for (int frame = 0; frame < 10; ++frame)
            session.update(0.1F, vehicles, input);
        session.update(0.1F, vehicles, input);
        if (session.countdownStage() != 4 ||
            session.phase() != RacePhase::Racing ||
            session.racers().front().IsBlock() ||
            session.vehicleInputs().front().throttle < 0.9F)
        {
            throw std::runtime_error(
                "offline cGoRace did not release vehicle control");
        }

        session.reset();
        for (int frame = 0; frame < 250; ++frame)
            session.update(1.0F / 60.0F, vehicles, input);
        if (session.phase() != RacePhase::Racing ||
            session.vehicleInputs().empty() ||
            session.racers().front().IsBlock() ||
            session.vehicleInputs().front().throttle < 0.9F)
            throw std::runtime_error("countdown/control transition failed");

        {
            // NetRace keeps the canonical network roster order.  The local
            // HumanPlayer pointer therefore does not have to occupy slot 0.
            // Exercise the source Player ID/role lookup directly so gameplay,
            // HUD, camera and profile ownership cannot regress to front().
            Race reorderedRace = race;
            if (reorderedRace.racers.size() < 2U)
            {
                throw std::runtime_error(
                    "human owner regression needs two racers");
            }
            std::swap(
                reorderedRace.racers[0], reorderedRace.racers[1]);
            reorderedRace.racers[0].human = true;
            reorderedRace.racers[0].playerId =
                2 << source::Player::opponentBit;
            reorderedRace.racers[0].netSlot = 2U;
            reorderedRace.racers[0].netName = "remote-owner-test";
            reorderedRace.racers[1].human = true;
            reorderedRace.racers[1].playerId =
                source::Player::humanId;
            reorderedRace.racers[1].netSlot =
                source::Player::defaultNetSlot;

            OriginalRaceSession reorderedSession(reorderedRace, true);
            RaceControl reorderedInput;
            reorderedInput.driving.throttle = 1.0F;
            reorderedSession.update(
                1.0F / 60.0F, vehicles, reorderedInput);
            if (reorderedSession.humanRacer() != 1U ||
                reorderedSession.vehicleInputs().size() < 2U ||
                reorderedSession.vehicleInputs()[1].throttle < 0.9F ||
                reorderedSession.vehicleInputs()[0].throttle != 0.0F ||
                !reorderedSession.racers()[1].IsHuman() ||
                !reorderedSession.racers()[0].IsOpponent())
            {
                throw std::runtime_error(
                    "source HumanPlayer owner still depended on racer 0");
            }

            auto& reorderedRacers =
                const_cast<std::vector<RacerRuntime>&>(
                    reorderedSession.racers());
            reorderedRacers[0].SetPoints(235U);
            reorderedRacers[1].SetMoney(4321U);
            reorderedRacers[1].SetPoints(765U);
            PlayerProfile reorderedProfile;
            reorderedSession.writePlayerProfile(reorderedProfile);
            if (reorderedProfile.money != 4321U ||
                reorderedProfile.points != 765U ||
                reorderedSession.humanOrOpponentCount() != 2U ||
                reorderedSession.totalHumanOrOpponentPoints() != 1000U)
            {
                throw std::runtime_error(
                    "source HumanPlayer/opponent total points owner "
                    "mismatch");
            }
            reorderedRacers[0].disconnected = true;
            if (reorderedSession.humanOrOpponentCount() != 1U ||
                reorderedSession.totalHumanOrOpponentPoints() != 765U)
            {
                throw std::runtime_error(
                    "disposed NetPlayer remained in source tournament "
                    "totals");
            }
            reorderedSession.resetTournamentPassPoints();
            if (reorderedSession.humanOrOpponentCount() != 1U ||
                reorderedSession.totalHumanOrOpponentPoints() != 0U ||
                reorderedRacers[0].GetPoints() != 235U ||
                reorderedRacers[1].GetPoints() != 0U)
            {
                throw std::runtime_error(
                    "source pass completion did not clear active Player "
                    "points exactly once");
            }
        }

        {
            OriginalRaceSession exitSession(race);
            auto& exitRacers = const_cast<std::vector<RacerRuntime>&>(
                exitSession.racers());
            exitRacers.front().TakeMoney(17.0F);
            exitSession.completeRaceForExit(vehicles);
            const auto* humanResult = exitSession.resultForRacer(0U);
            const auto activeCount = static_cast<std::size_t>(std::count_if(
                exitRacers.begin(), exitRacers.end(),
                [](const RacerRuntime& racer) {
                    return !racer.disconnected;
                }));
            if (!exitSession.finishPresentationReady() ||
                exitSession.results().size() != activeCount ||
                humanResult == nullptr || humanResult->pickedMoney != 17U ||
                !exitRacers.front().GetFinished() ||
                exitRacers.front().GetPickMoney() != 0U ||
                exitRacers.front().car.GetLastNode() != nullptr ||
                std::any_of(
                    exitSession.decorationActive().begin(),
                    exitSession.decorationActive().end(),
                    [](bool active) { return active; }) ||
                std::any_of(
                    exitSession.bonusActive().begin(),
                    exitSession.bonusActive().end(),
                    [](bool active) { return active; }) ||
                std::any_of(
                    exitRacers.begin(), exitRacers.end(),
                    [&](const RacerRuntime& racer) {
                        return exitSession.racerHasAiController(
                            static_cast<std::size_t>(
                                &racer - exitRacers.data()));
                    }) ||
                !exitSession.effects().empty() ||
                !exitSession.mines().empty() ||
                !exitSession.projectiles().empty() ||
                exitRacers.front().GetMoney() !=
                    humanResult->money + humanResult->pickedMoney ||
                exitRacers.front().GetPoints() != humanResult->points)
            {
                throw std::runtime_error(
                    "Race::ExitRace did not complete/rank/save all players");
            }
            const auto settledMoney = exitRacers.front().GetMoney();
            exitSession.completeRaceForExit(vehicles);
            if (exitRacers.front().GetMoney() != settledMoney ||
                exitSession.results().size() != activeCount)
            {
                throw std::runtime_error(
                    "Race::ExitRace completion was applied more than once");
            }
        }

        input.reset = true;
        vehicles[0].contactCount = 0U;
        vehicles[0].bodyContacts.clear();
        session.update(1.0F / 60.0F, vehicles, input);
        const auto airborneReset = std::any_of(
            session.events().begin(), session.events().end(),
            [](const RaceEvent& event) {
                return event.kind == RaceEventKind::Respawn &&
                       event.racer == 0U;
            });
        vehicles[0].contactCount = 1U;
        session.update(1.0F / 60.0F, vehicles, input);
        const auto groundedReset = std::any_of(
            session.events().begin(), session.events().end(),
            [](const RaceEvent& event) {
                return event.kind == RaceEventKind::Respawn &&
                       event.racer == 0U;
            });
        input.reset = false;
        vehicles[0].contactCount = 4U;
        if (airborneReset || !groundedReset)
        {
            throw std::runtime_error(
                "HumanPlayer::ResetCar contact gate mismatch");
        }

        OriginalRaceSession networkCountdownSession(race);
        const auto computerCheat =
            source::Player::cheatEnableFaster |
            source::Player::cheatEnableSlower;
        if (networkCountdownSession.racers().front().GetCheat() !=
                source::Player::cheatDisabled ||
            (networkCountdownSession.racers().size() > 1U &&
             networkCountdownSession.racers()[1].IsComputer() &&
             networkCountdownSession.racers()[1].GetCheat() !=
                 computerCheat))
        {
            throw std::runtime_error(
                "AIPlayer did not configure Player-owned cheat flags");
        }
        const std::array<float, 4> synchronizedColor{
            0.15F, 0.35F, 0.55F, 1.0F};
        if (!networkCountdownSession.synchronizePlayerPresentation(
                0U, 23, synchronizedColor) ||
            networkCountdownSession.racers()[0].GetGamerId() != 23 ||
            networkCountdownSession.racers()[0].GetColor() !=
                synchronizedColor ||
            networkCountdownSession.synchronizePlayerPresentation(
                networkCountdownSession.racers().size(), 24,
                synchronizedColor))
        {
            throw std::runtime_error(
                "NetPlayer runtime gamer/color owner mismatch");
        }
        networkCountdownSession.synchronizeNetworkCountdown(0);
        for (int frame = 0; frame < 300; ++frame)
        {
            networkCountdownSession.update(
                1.0F / 60.0F, vehicles, input);
        }
        if (networkCountdownSession.phase() != RacePhase::Countdown ||
            networkCountdownSession.countdownSeconds() != 3.0F ||
            networkCountdownSession.vehicleInputs().empty() ||
            !networkCountdownSession.racers().front().IsBlock() ||
            networkCountdownSession.vehicleInputs().front().brake <
                0.9F ||
            networkCountdownSession.vehicleInputs().front().throttle !=
                0.0F)
        {
            throw std::runtime_error(
                "network cGoRaceWait did not hold vehicle control");
        }
        networkCountdownSession.synchronizeNetworkCountdown(1);
        networkCountdownSession.synchronizeNetworkCountdown(2);
        if (networkCountdownSession.countdownSeconds() != 2.0F)
            throw std::runtime_error("network cGoRace2 was not applied");
        networkCountdownSession.synchronizeNetworkCountdown(3);
        if (networkCountdownSession.countdownSeconds() != 1.0F)
            throw std::runtime_error("network cGoRace3 was not applied");
        networkCountdownSession.synchronizeNetworkCountdown(4);
        networkCountdownSession.update(
            1.0F / 60.0F, vehicles, input);
        if (networkCountdownSession.phase() != RacePhase::Racing ||
            networkCountdownSession.countdownSeconds() != 0.0F ||
            networkCountdownSession.racers().front().IsBlock() ||
            networkCountdownSession.vehicleInputs().front().throttle <
                0.9F)
        {
            throw std::runtime_error(
                "network cGoRace did not release vehicle control");
        }
        std::vector<ReplicatedRaceResult> networkResults{
            {0U, 300, 25, 1U, 10},
            {1U, 200, 0, 2U, 7}};
        networkCountdownSession.setNetworkFinishControlled(true);
        networkCountdownSession.synchronizeNetworkFinishResults(
            networkResults);
        if (!networkCountdownSession.finishPresentationReady() ||
            !networkCountdownSession.racers()[0].GetFinished() ||
            networkCountdownSession.racers()[0].GetPlace() != 1U ||
            networkCountdownSession.racers()[0].rewardMoney != 300U ||
            networkCountdownSession.racers()[0].GetPickMoney() != 0U ||
            networkCountdownSession.resultForRacer(0U) == nullptr ||
            networkCountdownSession.resultForRacer(0U)->pickedMoney != 25U ||
            networkCountdownSession.racers()[1].GetPlace() != 2U)
        {
            throw std::runtime_error(
                "network ExitRace results were not applied to FinishMenu");
        }

        {
            OriginalRaceSession traceSession(race);
            auto traceVehicles = vehicles;
            for (int frame = 0; frame < 250; ++frame)
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
            if (traceSession.racers().front().car.moveInverse)
            {
                throw std::runtime_error(
                    "source moveInverse fired before 20 metres");
            }
            placeOnFirstTile(15.0F, true);
            if (!traceSession.racers().front().car.moveInverse)
            {
                throw std::runtime_error(
                    "source moveInverse distance transition failed");
            }
            placeOnFirstTile(16.0F, false);
            if (traceSession.racers().front().car.moveInverse)
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
                for (int frame = 0; frame < 250; ++frame)
                    skipSession.update(
                        1.0F / 60.0F, skipVehicles, input);
                const Vec3 skippedStart = point(2U).position;
                const Vec3 skippedEnd = point(3U).position;
                skipVehicles[0].body.position = multiply(
                    add(skippedStart, skippedEnd), 0.5F);
                skipVehicles[0].body.position.z += 2.0F;
                skipSession.update(
                    1.0F / 60.0F, skipVehicles, input);
                if (skipSession.racers().front().car.numLaps != 0U)
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
            for (int frame = 0; frame < 250; ++frame)
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
            // WayPath first releases the final tile once the car crosses its
            // end plane, then reacquires the first WayNode by its spherical
            // fallback.  The point is deliberately behind both segment
            // planes: requiring firstTile.contains here used to strand AI on
            // the preceding lap indefinitely.
            branchVehicles[0].body.position =
                add(branchPoint(0U, 0U).position,
                    {-1.0F, -1.0F, 2.0F});
            branchVehicles[0].body.rotation = shortestArcFromX(
                normalized2(subtract(
                    branchPoint(0U, 1U).position,
                    branchPoint(0U, 0U).position)));
            branchVehicles[0].speed = 10.0F;
            branchSession.update(
                1.0F / 60.0F, branchVehicles, input);
            placeOnBranch(branchSession, 0U, 0U, 0U, 0.5F, false);
            if (branchSession.racers().front().car.numLaps != 1U)
            {
                throw std::runtime_error(
                    "alternate WayPath lap transition failed");
            }

            OriginalRaceSession branchWrongWay(branchRace);
            for (int frame = 0; frame < 250; ++frame)
                branchWrongWay.update(
                    1.0F / 60.0F, branchVehicles, input);
            placeOnBranch(
                branchWrongWay, 0U, 1U, 0U, 0.9F, true);
            placeOnBranch(
                branchWrongWay, 0U, 1U, 0U, 0.4F, true);
            if (!branchWrongWay.racers().front().car.moveInverse)
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
                for (int frame = 0; frame < 250; ++frame)
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
                for (int frame = 0; frame < 250; ++frame)
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
                for (int frame = 0; frame < 250; ++frame)
                {
                    brakeHyperSession.update(
                        1.0F / 60.0F,
                        brakeHyperVehicles, input);
                }
                const Vec3 incoming = normalized2(subtract(
                    point(cornerNode).position,
                    point(cornerNode - 1U).position));
                const Vec3 outgoing = normalized2(subtract(
                    point(cornerNode + 1U).position,
                    point(cornerNode).position));
                brakeHyperVehicles[1].body.position = subtract(
                    point(cornerNode).position,
                    multiply(incoming, 30.0F));
                brakeHyperVehicles[1].body.position.z =
                    point(cornerNode).position.z + 2.0F;
                brakeHyperVehicles[1].body.rotation =
                    shortestArcFromX(incoming);
                const Vec3 middleDirection = normalized2(
                    add(incoming, outgoing));
                const float brakeDistance =
                    30.0F * dot2(middleDirection, incoming) +
                    point(cornerNode).width;
                const float brakeRotation =
                    1.0F - std::max(dot2(incoming, outgoing), 0.0F);
                const auto& brakeRacer = brakeHyperRace.racers[1];
                const auto& brakeVehicle =
                    brakeRacer.hasConfiguredVehicle
                        ? brakeRacer.configuredVehicle
                        : brakeHyperRace.vehicles.at(brakeRacer.vehicle);
                const float brakeCoefficient = std::max(
                    brakeVehicle.physics.steeringControl *
                        brakeVehicle.physics.steeringControl *
                        brakeRotation,
                    0.000001F);
                // Cross AICar's serialized 1.5 * kBreak threshold using the
                // source kSteerControl, rather than assuming it equals one.
                brakeHyperVehicles[1].speed = std::sqrt(
                    1.6F * std::max(brakeDistance, 0.0F) /
                    brakeCoefficient);
                brakeHyperSession.update(
                    1.0F / 60.0F,
                    brakeHyperVehicles, input);
                if (brakeHyperSession.vehicleInputs()[1].brake < 0.9F ||
                    brakeHyperSession.racers()[1]
                            .GetHyperWeaponItem()->GetCurCharge() != 2U)
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
                for (int frame = 0; frame < 250; ++frame)
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
                if (offTraceAttackSession.racers()[1]
                            .GetHyperWeaponItem()->GetCurCharge() != 2U ||
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
                for (int frame = 0; frame < 250; ++frame)
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
                for (int frame = 0; frame < 250; ++frame)
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
            for (int frame = 0; frame < 250; ++frame)
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
            for (int frame = 0; frame < 250; ++frame)
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
            for (int frame = 0; frame < 250; ++frame)
                aiCheatSession.update(
                    1.0F / 60.0F, cheatVehicles, input);
            // Trace::WayNode::Tile uses strict miter-plane containment at a
            // node boundary. Sample the interior of the following source
            // tile instead of relying on the old session epsilon.
            cheatVehicles[0].body.position = add(
                point(1U).position,
                multiply(subtract(point(2U).position,
                                  point(1U).position),
                         0.5F));
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
                for (int frame = 0; frame < 250; ++frame)
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
                            .motorTorqueScale > 1.0001F ||
                    fieldCheatSession.vehicleInputs()[1]
                            .motorTorqueScale > 1.0001F)
                {
                    throw std::runtime_error(
                        "source Player::CheatUpdate accepted Computer as "
                        "a Human/Opponent reference");
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
            for (int frame = 0; frame < 250; ++frame)
                destructionSession.update(
                    1.0F / 60.0F, vehicles, destructionInput);
            const std::size_t instance = static_cast<std::size_t>(
                sourceDestruction - race.decorationInstances.begin());
            const auto& destructionDefinition =
                race.decorationDefinitions.at(
                    sourceDestruction->definition);
            const auto* destructionMapObject =
                destructionSession.sourceMap().GetMapObj(
                    sourceDestruction->mapObjectId, true);
            if (destructionMapObject == nullptr ||
                destructionMapObject->GetDestrObj() == nullptr ||
                destructionMapObject->GetDestrObj()
                        ->GetDestrList().GetLiveCount() !=
                    destructionDefinition.destructionPieces.size())
            {
                throw std::runtime_error(
                    "source DestrObj serialized destruction list was not "
                    "instantiated");
            }
            const std::size_t mapObjectsBefore =
                destructionSession.sourceMap().GetObjects().size();
            const std::uint32_t lastMapIdBefore =
                destructionSession.sourceMap().GetLastId();
            Vec3 sourceContact = sourceDestruction->transform.position;
            Vec3 sourceRayOrigin;
            Vec3 sourceRayDirection;
            bool foundOwnedRayShape = false;
            const std::vector<r3d::physics::VehicleState> noRayVehicles;
            const std::vector<RacerRuntime> noRayRacers;
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
                for (std::size_t triangle = 0U;
                     triangle + 2U < mesh.indices.size();
                     triangle += 3U)
                {
                    const auto firstIndex = mesh.indices[triangle];
                    const auto secondIndex = mesh.indices[triangle + 1U];
                    const auto thirdIndex = mesh.indices[triangle + 2U];
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
                    const Vec3 normal = normalized3(cross(
                        subtract(second, first),
                        subtract(third, first)));
                    if (length3(normal) < 0.5F)
                        continue;
                    const Vec3 center = multiply(
                        add(add(first, second), third), 1.0F / 3.0F);
                    const Vec3 origin = add(
                        center, multiply(normal, 0.1F));
                    const auto rayHit = raycastWorld(
                        race, destructionSession.decorationActive(),
                        noRayVehicles, noRayRacers,
                        RacerRuntime::invalidWeapon, origin,
                        multiply(normal, -1.0F), 0.2F);
                    if (!rayHit.hit || rayHit.decoration != instance)
                        continue;
                    sourceContact = center;
                    sourceRayOrigin = origin;
                    sourceRayDirection = multiply(normal, -1.0F);
                    foundOwnedRayShape = true;
                    break;
                }
                if (foundOwnedRayShape)
                    break;
            }
            if (!foundOwnedRayShape)
            {
                throw std::runtime_error(
                    "source destructible PhysX actor was not returned by "
                    "the world ray query");
            }
            const auto& playerDefinition =
                race.racers.front().hasConfiguredVehicle
                    ? race.racers.front().configuredVehicle
                    : race.vehicles.at(
                          race.racers.front().vehicle);
            vehicles[0].body.position = subtract(
                sourceContact,
                playerDefinition.physics.shapePosition);
            r3d::physics::BodyContact sourceTouch;
            sourceTouch.surface =
                r3d::physics::CollisionSurface::Decoration;
            sourceTouch.otherDecoration = instance;
            sourceTouch.point = sourceContact;
            sourceTouch.points = {sourceContact};
            sourceTouch.hasPoint = true;
            vehicles[0].bodyContacts = {sourceTouch};
            destructionSession.update(
                1.0F / 60.0F, vehicles, destructionInput);
            vehicles[0].bodyContacts.clear();
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
            const auto& separatedMap = destructionSession.sourceMap();
            if (separatedMap.GetMapObj(
                    sourceDestruction->mapObjectId, true) != nullptr ||
                separatedMap.GetObjects().size() !=
                    mapObjectsBefore - 1U +
                        destructionDefinition.destructionPieces.size() ||
                separatedMap.GetLastId() !=
                    lastMapIdBefore +
                        destructionDefinition.destructionPieces.size())
            {
                throw std::runtime_error(
                    "source DestrObj children were not transferred into "
                    "the runtime Map");
            }
            for (std::size_t piece = 0U;
                 piece < destructionDefinition.destructionPieces.size();
                 ++piece)
            {
                const auto* separated = separatedMap.GetMapObj(
                    lastMapIdBefore +
                        static_cast<std::uint32_t>(piece) + 1U,
                    true);
                if (separated == nullptr ||
                    separated->GetName() !=
                        "item" + std::to_string(piece) ||
                    separated->GetCategory() !=
                        source::MapObjCategory::Decoration ||
                    separated->GetParent() != nullptr ||
                    separated->GetGameObj().GetPos() !=
                        source::GameObject::Vector3{
                            sourceDestruction->transform.position.x,
                            sourceDestruction->transform.position.y,
                            sourceDestruction->transform.position.z})
                {
                    throw std::runtime_error(
                        "source DestrObj transferred child proxy state "
                        "mismatch");
                }
            }
            const auto removedActorRay = raycastWorld(
                race, destructionSession.decorationActive(),
                noRayVehicles, noRayRacers,
                RacerRuntime::invalidWeapon, sourceRayOrigin,
                sourceRayDirection, 0.2F);
            if (removedActorRay.decoration == instance)
            {
                throw std::runtime_error(
                    "destroyed decoration actor remained in the source "
                    "Laser/FrostRay collision group");
            }
            destructionSession.update(
                1.0F / 60.0F, vehicles, destructionInput);
            const bool repeatedDestruction = std::any_of(
                destructionSession.events().begin(),
                destructionSession.events().end(),
                [instance](const RaceEvent& event) {
                    return event.kind ==
                               RaceEventKind::DecorationDestroyed &&
                           event.target == instance;
                });
            if (repeatedDestruction)
            {
                throw std::runtime_error(
                    "source gotDestrObj separation was emitted more than "
                    "once");
            }
            vehicles[0].speed = 0.0F;
        }

        if (!race.bonuses.empty())
        {
            Race autoProjRace = race;
            autoProjRace.bonuses.assign(1U, race.bonuses.front());
            auto& sourceOil = autoProjRace.bonuses.front();
            sourceOil.kind = BonusKind::OilHazard;
            sourceOil.projectileType = 10U;
            sourceOil.transform.position = {
                100000.0F, 100000.0F, 1000.0F};
            OriginalRaceSession autoProjSession(autoProjRace);
            auto autoProjVehicles = vehicles;
            RaceControl autoProjInput;
            if (autoProjSession.bonusScales().size() != 1U ||
                autoProjSession.bonusScales().front() != 0.0F)
            {
                throw std::runtime_error(
                    "source AutoProj oil did not start at scale zero");
            }
            autoProjSession.update(
                0.1F, autoProjVehicles, autoProjInput);
            if (autoProjSession.phase() != RacePhase::Countdown ||
                std::abs(
                    autoProjSession.bonusScales().front() - 0.4F) >
                    0.001F)
            {
                throw std::runtime_error(
                    "source AutoProj did not progress during countdown");
            }
            autoProjSession.update(
                0.1F, autoProjVehicles, autoProjInput);
            autoProjSession.update(
                0.1F, autoProjVehicles, autoProjInput);
            if (std::abs(
                    autoProjSession.bonusScales().front() - 1.0F) >
                0.001F)
            {
                throw std::runtime_error(
                    "source AutoProj oil arming scale did not finish");
            }
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
            for (int frame = 0; frame < 250; ++frame)
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
            for (int frame = 0; frame < 250; ++frame)
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
            // DeathEffect inserts the spawned MapObj during this update.
            // Its own LifeEffect receives the first OnProgress callback on
            // the following map pass, exactly like MapObjects::OnProgress.
            pickupVehicles[0].body.position.x += 1000.0F;
            pickupSession.update(
                1.0F / 60.0F, pickupVehicles, pickupInput);
            const bool hasSourceDeathSound = std::any_of(
                pickupSession.events().begin(),
                pickupSession.events().end(),
                [&](const RaceEvent& event) {
                    return event.kind == RaceEventKind::EffectSound &&
                           event.soundLifetimeSeconds > 0.0F &&
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

        const auto sourceMedpack = std::find_if(
            race.bonuses.begin(), race.bonuses.end(),
            [](const BonusInstance& bonus) {
                return bonus.kind == BonusKind::Medpack;
            });
        if (sourceMedpack == race.bonuses.end())
        {
            throw std::runtime_error(
                "source medpack is missing for TakeBonus regression");
        }
        {
            Race medpackRace = race;
            medpackRace.bonuses.assign(1U, *sourceMedpack);
            medpackRace.bonuses.front().transform.position =
                vehicles.front().body.position;
            medpackRace.bonuses.front().transform.position.z +=
                100.0F;
            OriginalRaceSession medpackSession(medpackRace);
            auto medpackVehicles = vehicles;
            RaceControl medpackInput;
            medpackVehicles[0].speed = 0.0F;
            medpackVehicles[0].linearVelocity = {};
            medpackVehicles[0].bodyContacts.clear();
            for (int frame = 0; frame < 250; ++frame)
                medpackSession.update(
                    1.0F / 60.0F, medpackVehicles, medpackInput);
            medpackSession.applyNetworkPlayerDamage(
                0U, RacerRuntime::invalidWeapon,
                medpackVehicles[0].body.position, 5.0F,
                DamageType::Simple, medpackVehicles[0]);
            const float lifeBefore =
                medpackSession.racers().front().life;
            const float sourceMedpackValue =
                sourceMedpack->value > 0.0F
                    ? sourceMedpack->value
                    : medpackSession.racers().front().maximumLife;
            const float expectedLife = std::min(
                lifeBefore + sourceMedpackValue,
                medpackSession.racers().front().maximumLife);
            medpackVehicles[0].body.position =
                medpackRace.bonuses.front().transform.position;
            medpackSession.update(
                1.0F / 60.0F, medpackVehicles, medpackInput);
            const bool picked = std::any_of(
                medpackSession.events().begin(),
                medpackSession.events().end(),
                [](const RaceEvent& event) {
                    return event.kind == RaceEventKind::Bonus &&
                           event.target == 0U;
                });
            if (medpackSession.bonusActive().front() || !picked ||
                std::abs(
                    medpackSession.racers().front().life -
                    expectedLife) > 0.001F)
            {
                throw std::runtime_error(
                    "source Player::TakeBonus medpack Healt(value) failed");
            }
        }

        const auto sourceAmmunition = std::find_if(
            race.bonuses.begin(), race.bonuses.end(),
            [](const BonusInstance& bonus) {
                return bonus.kind == BonusKind::Ammunition;
            });
        if (sourceAmmunition == race.bonuses.end())
        {
            throw std::runtime_error(
                "source ammunition is missing for TakeBonus regression");
        }
        {
            Race ammunitionRace = race;
            ammunitionRace.racers.resize(1U);
            ammunitionRace.bonuses.assign(1U, *sourceAmmunition);
            ammunitionRace.bonuses.front().transform.position =
                vehicles.front().body.position;
            ammunitionRace.bonuses.front().transform.position.z +=
                100.0F;
            OriginalRaceSession ammunitionSession(ammunitionRace);
            auto ammunitionVehicles = vehicles;
            ammunitionVehicles.resize(1U);
            RaceControl ammunitionInput;
            ammunitionVehicles[0].speed = 0.0F;
            ammunitionVehicles[0].linearVelocity = {};
            ammunitionVehicles[0].bodyContacts.clear();
            for (int frame = 0; frame < 250; ++frame)
                ammunitionSession.update(
                    1.0F / 60.0F, ammunitionVehicles,
                    ammunitionInput);
            auto& ammunitionRuntime = const_cast<RacerRuntime&>(
                ammunitionSession.racers().front());
            if (ammunitionRuntime.hyperWeapon ==
                    RacerRuntime::invalidWeapon ||
                ammunitionRuntime.mineWeapon ==
                    RacerRuntime::invalidWeapon ||
                ammunitionRuntime.weaponSlots[0] ==
                    RacerRuntime::invalidWeapon ||
                ammunitionRuntime.hyperCapacity == 0U ||
                ammunitionRuntime.mineCapacity == 0U ||
                ammunitionRuntime.weaponCapacity[0] == 0U)
            {
                throw std::runtime_error(
                    "source TakeBonus slot-order regression lacks loadout");
            }
            ammunitionRuntime.GetHyperWeaponItem()->SetCurCharge(0U);
            ammunitionRuntime.GetMineWeaponItem()->SetCurCharge(0U);
            ammunitionRuntime.GetPrimaryWeaponItems()[0]
                ->SetCurCharge(0U);
            unsigned sourceSeed = 0U;
            for (; sourceSeed < 4096U; ++sourceSeed)
            {
                std::srand(sourceSeed);
                if (source::Player::RoundedRandomIndex(
                        3U, sourceRandomUnit()) == 0U)
                    break;
            }
            if (sourceSeed == 4096U)
            {
                throw std::runtime_error(
                    "source TakeBonus rounded RNG seed was not found");
            }
            std::srand(sourceSeed);
            ammunitionVehicles[0].body.position =
                ammunitionRace.bonuses.front().transform.position;
            ammunitionSession.update(
                1.0F / 60.0F, ammunitionVehicles,
                ammunitionInput);
            const auto ammunitionEvent = std::find_if(
                ammunitionSession.events().begin(),
                ammunitionSession.events().end(),
                [](const RaceEvent& event) {
                    return event.kind == RaceEventKind::Bonus &&
                           event.target == 0U;
                });
            if (ammunitionEvent == ammunitionSession.events().end() ||
                ammunitionEvent->pickSlot != PickSlot::Hyper ||
                ammunitionRuntime.GetHyperWeaponItem()
                        ->GetCurCharge() == 0U ||
                ammunitionRuntime.GetMineWeaponItem()
                        ->GetCurCharge() != 0U ||
                ammunitionRuntime.GetPrimaryWeaponItems()[0]
                        ->GetCurCharge() != 0U)
            {
                throw std::runtime_error(
                    "source Player::TakeBonus stHyper..stWeapon4 order failed");
            }
            std::srand(1);
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
            for (int frame = 0; frame < 250; ++frame)
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
            for (int frame = 0; frame < 250; ++frame)
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
        vehicles[0].bodyContacts.front().frictionForceVector =
            {0.0F, 1.0F, 2.0F};
        session.update(1.0F / 60.0F, vehicles, input);
        const auto springBorderRequests =
            session.takeVelocityRequests();
        const bool hasFrictionVectorRedirect = std::any_of(
            springBorderRequests.begin(), springBorderRequests.end(),
            [](const VelocityRequest& request) {
                // Source clears the final Z velocity after using the
                // friction vector to choose its tangent.  For this fixture
                // that vector produces wanted Y=2 (delta Y=-3), whereas the
                // former velocity projection produced delta Y=-2.5.
                return request.racer == 0U &&
                       request.delta.y < -2.75F &&
                       request.delta.y > -3.25F;
            });
        if (!hasFrictionVectorRedirect ||
            session.racers().front().life >= lifeBeforeBorder)
            throw std::runtime_error(
                "source friction-vector spring-border transition failed");

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
            for (int frame = 0; frame < 250; ++frame)
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
                           event.soundContactActor == 77U &&
                           event.soundContactSurface ==
                               r3d::physics::CollisionSurface::TrackBorder &&
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
            contactSession.update(
                0.1F, contactVehicles, contactInput);
            contactSession.update(
                0.001F, contactVehicles, contactInput);
            contact.points.resize(1U);
            contactVehicles[0].bodyContacts = {contact};
            contactSession.update(
                1.0F / 60.0F, contactVehicles, contactInput);
            const auto fadingContactCount =
                static_cast<std::size_t>(std::count_if(
                    contactSession.effects().begin(),
                    contactSession.effects().end(),
                    [](const RaceEffect& effect) {
                        return effect.kind ==
                                   RaceEventKind::ContactImpact &&
                               effect.waitingEnd.IsResurrect();
                    }));
            const auto liveContactCount =
                static_cast<std::size_t>(std::count_if(
                    contactSession.effects().begin(),
                    contactSession.effects().end(),
                    [](const RaceEffect& effect) {
                        return effect.kind ==
                                   RaceEventKind::ContactImpact &&
                               !effect.waitingEnd.IsResurrect();
                    }));
            if (fadingContactCount != 2U || liveContactCount != 1U)
            {
                throw std::runtime_error(
                    "source PairPxContactEffect reused a released effect");
            }
            contactVehicles[0].bodyContacts.clear();
            for (int step = 0; step < 5; ++step)
                contactSession.update(
                    0.1F, contactVehicles, contactInput);
            const bool particlesStillAlive = std::any_of(
                contactSession.effects().begin(),
                contactSession.effects().end(),
                [](const RaceEffect& effect) {
                    return effect.kind == RaceEventKind::ContactImpact;
                });
            for (int step = 0; step < 5; ++step)
                contactSession.update(
                    0.1F, contactVehicles, contactInput);
            const bool released = std::none_of(
                contactSession.effects().begin(),
                contactSession.effects().end(),
                [](const RaceEffect& effect) {
                    return effect.kind == RaceEventKind::ContactImpact;
                });
            if (!particlesStillAlive)
                throw std::runtime_error(
                    "source PairPxContactEffect particles expired early");
            if (!released)
            {
                const auto remainingContactCount =
                    static_cast<std::size_t>(std::count_if(
                        contactSession.effects().begin(),
                        contactSession.effects().end(),
                        [](const RaceEffect& effect) {
                            return effect.kind ==
                                   RaceEventKind::ContactImpact;
                        }));
                throw std::runtime_error(
                    "source PairPxContactEffect waiting-end did not finish: " +
                    std::to_string(remainingContactCount));
            }
        }

        {
            OriginalRaceSession lowLifeSession(race);
            auto lowLifeVehicles = vehicles;
            RaceControl lowLifeInput;
            for (int frame = 0; frame < 250; ++frame)
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
            if (!lowLifeSession.racers().front()
                     .lowLifePoints.IsEffectMaked() ||
                lowLifeSession.racers().front().destroyed ||
                lowLifeSession.racers().front().life <= 0.0F ||
                lowLifeSession.racers().front()
                        .lowLifePoints.GetEffectSeconds() <= 0.0F ||
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
            // This fixture extends serialized ctBonus, so Map::Load assigns
            // its ID before Player::CreateCar allocates dynamic car IDs.
            shieldBonus.mapObjectId =
                shieldRace.firstDynamicMapObjectId++;
            for (std::size_t index = 0U;
                 index < shieldRace.racers.size(); ++index)
            {
                shieldRace.racers[index].mapObjectId =
                    shieldRace.firstDynamicMapObjectId +
                    static_cast<std::uint32_t>(index);
            }
            shieldRace.bonuses.push_back(std::move(shieldBonus));

            OriginalRaceSession shieldSession(shieldRace);
            auto shieldVehicles = vehicles;
            RaceControl shieldInput;
            shieldVehicles[0].speed = 0.0F;
            shieldVehicles[0].linearVelocity = {};
            shieldVehicles[0].bodyContacts.clear();
            for (int frame = 0; frame < 250; ++frame)
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
                picked.immortalEffect.GetEffectSeconds() != 0.0F ||
                picked.immortalEffect.GetFadeInTime() != 0.0F ||
                picked.immortalEffect.GetFadeOutTime() >= 0.0F)
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
                damaged.immortalEffect.GetDamageTime() != 0.0F ||
                std::abs(
                    damaged.immortalEffect.GetFadeInTime() - 0.1F) >
                    0.001F ||
                std::abs(
                    damaged.immortalEffect.GetEffectSeconds() - 0.1F) >
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
                fading.immortalEffect.GetFadeOutTime() < 0.0F ||
                fading.immortalEffect.GetFadeOutTime() >= 0.5F ||
                fading.immortalEffect.GetEffectSeconds() <= 0.0F)
            {
                throw std::runtime_error(
                    "source ImmortalEffect fade-out did not start");
            }
            while (shieldSession.racers().front()
                       .immortalEffect.GetFadeOutTime() >= 0.0F &&
                   expirySteps++ < 120U)
            {
                shieldSession.update(
                    0.1F, shieldVehicles, shieldInput);
            }
            const auto& faded = shieldSession.racers().front();
            if (faded.immortalEffect.GetFadeOutTime() >= 0.0F ||
                faded.immortalEffect.GetEffectSeconds() != 0.0F)
            {
                throw std::runtime_error(
                    "source ImmortalEffect fade-out did not free effect");
            }
        }

        {
            OriginalRaceSession deathSession(race);
            auto deathVehicles = vehicles;
            RaceControl deathInput;
            for (int frame = 0; frame < 250; ++frame)
                deathSession.update(
                    1.0F / 60.0F, deathVehicles, deathInput);
            const auto deathInitialMapObjectId =
                deathSession.racerMapObjectId(0U);
            if (deathInitialMapObjectId ==
                source::Map::defaultMapObjId)
            {
                throw std::runtime_error(
                    "source Player::CreateCar has no initial MapObj ID");
            }
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
            // The DeathEffect actors are inserted by the lethal contact;
            // their LifeEffect sound begins on their first subsequent
            // MapObjects progress pass.
            deathSession.update(
                1.0F / 60.0F, deathVehicles, deathInput);
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
                               event.soundLifetimeSeconds > 0.0F &&
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
                deathSession.racers().front()
                    .lowLifePoints.IsEffectMaked() ||
                deathSession.racerMapObjectId(0U) !=
                    source::Map::defaultMapObjId ||
                deathSession.racerForMapObjectId(
                    deathInitialMapObjectId) !=
                    RacerRuntime::invalidWeapon ||
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
                !deathSession.racers().front().destroyed ||
                deathSession.racerMapObjectId(0U) !=
                    source::Map::defaultMapObjId)
            {
                throw std::runtime_error(
                    "source two-second vehicle restore was not queued");
            }
            deathSession.update(0.1F, deathVehicles, deathInput);
            const auto deathRestoredMapObjectId =
                deathSession.racerMapObjectId(0U);
            if (deathSession.racers().front().destroyed ||
                deathRestoredMapObjectId <= deathInitialMapObjectId ||
                deathSession.racerForMapObjectId(
                    deathRestoredMapObjectId) != 0U ||
                deathSession.racerForMapObjectId(
                    deathInitialMapObjectId) !=
                    RacerRuntime::invalidWeapon)
            {
                throw std::runtime_error(
                    "source restored vehicle did not receive a new MapObj");
            }
        }

        if (vehicles.size() > 1U)
        {
            {
                OriginalRaceSession overboardSession(race);
                auto overboardVehicles = vehicles;
                RaceControl overboardInput;
                for (int frame = 0; frame < 250; ++frame)
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
                        return event.kind == RaceEventKind::Death &&
                               event.racer == 1U &&
                               event.target == 0U &&
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

            {
                OriginalRaceSession repeatedContactSession(race);
                auto repeatedVehicles = vehicles;
                for (auto& state : repeatedVehicles)
                    state.bodyContacts.clear();
                for (int frame = 0; frame < 250; ++frame)
                {
                    repeatedContactSession.update(
                        1.0F / 60.0F, repeatedVehicles, input);
                }
                repeatedVehicles[0].kineticEnergy = 100.0F;
                repeatedVehicles[1].kineticEnergy = 10.0F;
                repeatedVehicles[0].bodyContacts = {
                    {r3d::physics::CollisionSurface::Vehicle, 1U,
                     {-1.0F, 0.0F, 0.0F}, 20.0F, 1200000.0F}};
                const float beforeFirstContact =
                    repeatedContactSession.racers()[1].life;
                repeatedContactSession.update(
                    1.0F / 60.0F, repeatedVehicles, input);
                const float afterFirstContact =
                    repeatedContactSession.racers()[1].life;
                repeatedContactSession.update(
                    1.0F / 60.0F, repeatedVehicles, input);
                const float afterSecondContact =
                    repeatedContactSession.racers()[1].life;
                if (!(afterFirstContact < beforeFirstContact) ||
                    !(afterSecondContact < afterFirstContact))
                {
                    throw std::runtime_error(
                        "source car contact was incorrectly rate-limited");
                }
            }

            {
                OriginalRaceSession energySession(race);
                auto energyVehicles = vehicles;
                for (auto& state : energyVehicles)
                    state.bodyContacts.clear();
                for (int frame = 0; frame < 250; ++frame)
                {
                    energySession.update(
                        1.0F / 60.0F, energyVehicles, input);
                }
                // The slower second car has more rotational energy. PhysX
                // computeKineticEnergy therefore attributes touch damage to
                // it and damages the first car.
                energyVehicles[0].speed = 40.0F;
                energyVehicles[1].speed = 0.0F;
                energyVehicles[0].kineticEnergy = 100.0F;
                energyVehicles[1].kineticEnergy = 1000.0F;
                energyVehicles[0].bodyContacts = {
                    {r3d::physics::CollisionSurface::Vehicle, 1U,
                     {-1.0F, 0.0F, 0.0F}, 20.0F, 1200000.0F}};
                const float firstEnergyLife =
                    energySession.racers()[0].life;
                const float secondEnergyLife =
                    energySession.racers()[1].life;
                energySession.update(
                    1.0F / 60.0F, energyVehicles, input);
                if (!(energySession.racers()[0].life < firstEnergyLife) ||
                    std::abs(
                        energySession.racers()[1].life -
                        secondEnergyLife) > 0.001F)
                {
                    throw std::runtime_error(
                        "source total kinetic-energy attribution failed");
                }
            }
        }

        OriginalRaceSession lapSession(race);
        auto lapVehicles = vehicles;
        for (auto& vehicle : lapVehicles)
        {
            vehicle.bodyContacts.clear();
            vehicle.speed = 0.0F;
            vehicle.linearVelocity = {};
        }
        for (int frame = 0; frame < 250; ++frame)
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
        if (lapSession.racers().front().car.numLaps != 1 ||
            lapSession.racers().front().nextPathNode != 1)
            throw std::runtime_error("checkpoint/lap transition failed");
        lapSession.update(
            1.0F / 60.0F, lapVehicles, input);
        if (lapSession.racers().front().car.numLaps != 1U)
            throw std::runtime_error("finish tile counted the same lap twice");

        if (race.racers.size() >= 4U &&
            vehicles.size() >= race.racers.size())
        {
            // Campaign Race::CreatePlayers installs five AI players.  Drive
            // every one of them (including the source Rip/Shred slots) over
            // a complete trace and prove CompleteRace(Player*) removes its
            // AICar control after the configured lap count.
            Race aiFinishRace = race;
            aiFinishRace.lapCount = 1U;
            OriginalRaceSession aiFinishSession(aiFinishRace);
            auto aiFinishVehicles = vehicles;
            for (auto& vehicle : aiFinishVehicles)
            {
                vehicle.bodyContacts.clear();
                vehicle.speed = 10.0F;
                vehicle.linearVelocity = {};
            }
            for (int frame = 0; frame < 250; ++frame)
                aiFinishSession.update(
                    1.0F / 60.0F, aiFinishVehicles, input);

            auto placeAiOnMainSegment =
                [&](std::size_t racer, std::size_t segment) {
                    const auto& start = point(segment);
                    const auto& end = point(segment + 1U);
                    aiFinishVehicles[racer].body.position = multiply(
                        add(start.position, end.position), 0.5F);
                    aiFinishVehicles[racer].body.position.z =
                        (start.position.z + end.position.z) * 0.5F +
                        2.0F;
                    aiFinishVehicles[racer].body.rotation =
                        shortestArcFromX(normalized2(subtract(
                            end.position, start.position)));
                    aiFinishSession.update(
                        1.0F / 60.0F, aiFinishVehicles, input);
                };

            for (std::size_t racer = 1U;
                 racer < aiFinishRace.racers.size(); ++racer)
            {
                for (std::size_t segment = 0U;
                     segment + 1U < aiFinishRace.tracePath.size();
                     ++segment)
                {
                    placeAiOnMainSegment(racer, segment);
                }
                aiFinishVehicles[racer].body.position.x += 1000.0F;
                aiFinishVehicles[racer].body.position.y += 1000.0F;
                aiFinishSession.update(
                    1.0F / 60.0F, aiFinishVehicles, input);
                placeAiOnMainSegment(racer, 0U);

                const auto& runtime = aiFinishSession.racers()[racer];
                const bool emittedFinish = std::any_of(
                    aiFinishSession.events().begin(),
                    aiFinishSession.events().end(),
                    [racer](const RaceEvent& event) {
                        return event.kind == RaceEventKind::Finish &&
                               event.racer == racer;
                    });
                const auto& coasting =
                    aiFinishSession.vehicleInputs()[racer];
                if (!runtime.GetFinished() ||
                    runtime.car.numLaps != 1U ||
                    aiFinishSession.racerHasAiController(racer) ||
                    !emittedFinish || coasting.throttle > 0.1F ||
                    coasting.reverse > 0.1F ||
                    std::abs(coasting.steering) > 0.1F)
                {
                    throw std::runtime_error(
                        "campaign AI did not finish and release AICar");
                }
                if (runtime.GetPlace() > 3U &&
                    (runtime.rewardMoney != 0U ||
                     runtime.rewardPoints != 0U))
                {
                    throw std::runtime_error(
                        "Race::OnLapPass awarded third-place reward to "
                        "a lower place");
                }

                for (int frame = 0; frame < 4; ++frame)
                    aiFinishSession.update(
                        0.1F, aiFinishVehicles, input);
                if (aiFinishSession.vehicleInputs()[racer].brake <
                        0.9F ||
                    aiFinishSession.racers()[racer].car.numLaps !=
                        1U)
                {
                    throw std::runtime_error(
                        "finished campaign AI continued driving");
                }
            }

            for (std::size_t segment = 0U;
                 segment + 1U < aiFinishRace.tracePath.size(); ++segment)
            {
                placeAiOnMainSegment(0U, segment);
            }
            aiFinishVehicles[0].body.position.x += 1000.0F;
            aiFinishVehicles[0].body.position.y += 1000.0F;
            aiFinishSession.update(
                1.0F / 60.0F, aiFinishVehicles, input);
            placeAiOnMainSegment(0U, 0U);
            std::vector<std::uint32_t> expectedMoney;
            std::vector<std::uint32_t> expectedPoints;
            expectedMoney.reserve(aiFinishSession.racers().size());
            expectedPoints.reserve(aiFinishSession.racers().size());
            for (std::size_t racer = 0U;
                 racer < aiFinishSession.racers().size(); ++racer)
            {
                const auto* result =
                    aiFinishSession.resultForRacer(racer);
                expectedMoney.push_back(
                    result != nullptr
                        ? result->money + result->pickedMoney
                        : 0U);
                expectedPoints.push_back(
                    result != nullptr ? result->points : 0U);
            }
            for (int frame = 0; frame < 31; ++frame)
            {
                aiFinishSession.update(
                    0.1F, aiFinishVehicles, input);
            }
            if (!aiFinishSession.finishPresentationReady())
            {
                throw std::runtime_error(
                    "campaign CompleteRace result timer failed");
            }
            for (std::size_t racer = 0U;
                 racer < aiFinishSession.racers().size(); ++racer)
            {
                const auto& runtime = aiFinishSession.racers()[racer];
                if (runtime.GetMoney() != expectedMoney[racer] ||
                    runtime.GetPoints() != expectedPoints[racer])
                {
                    throw std::runtime_error(
                        "campaign result was not awarded to every racer");
                }
            }
            aiFinishSession.update(
                0.1F, aiFinishVehicles, input);
            for (std::size_t racer = 0U;
                 racer < aiFinishSession.racers().size(); ++racer)
            {
                if (aiFinishSession.racers()[racer].GetMoney() !=
                        expectedMoney[racer] ||
                    aiFinishSession.racers()[racer].GetPoints() !=
                        expectedPoints[racer])
                {
                    throw std::runtime_error(
                        "campaign result was awarded more than once");
                }
            }
        }

        const auto lapPrimary = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return weapon.slot == WeaponSlot::Primary &&
                       !weapon.projectiles.empty();
            });
        const auto lapHyper = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return weapon.slot == WeaponSlot::Hyper &&
                       !weapon.projectiles.empty();
            });
        const auto lapMine = std::find_if(
            race.weapons.begin(), race.weapons.end(),
            [](const WeaponDefinition& weapon) {
                return weapon.slot == WeaponSlot::Mine &&
                       !weapon.projectiles.empty();
            });
        if (lapPrimary == race.weapons.end() ||
            lapHyper == race.weapons.end() ||
            lapMine == race.weapons.end())
        {
            throw std::runtime_error(
                "source lap reload weapon fixtures are incomplete");
        }
        {
            Race reloadRace = race;
            reloadRace.racers.resize(1U);
            OriginalRaceSession reloadSession(reloadRace);
            PlayerProfile reloadProfile;
            auto setReloadSlot = [&](std::size_t slot,
                                     const WeaponDefinition& weapon) {
                reloadProfile.slots[slot].record = weapon.record;
                reloadProfile.slots[slot].charge = 2U;
                reloadProfile.slots[slot].hasCharge = true;
            };
            setReloadSlot(
                PlayerProfile::firstWeaponSlot, *lapPrimary);
            setReloadSlot(PlayerProfile::hyperSlot, *lapHyper);
            setReloadSlot(PlayerProfile::mineSlot, *lapMine);
            reloadSession.applyPlayerProfile(reloadProfile);
            auto reloadVehicles = vehicles;
            reloadVehicles.resize(1U);
            reloadVehicles[0].bodyContacts.clear();
            reloadVehicles[0].speed = 0.0F;
            reloadVehicles[0].linearVelocity = {};
            RaceControl reloadInput;
            for (int frame = 0; frame < 250; ++frame)
            {
                reloadSession.update(
                    1.0F / 60.0F,
                    reloadVehicles, reloadInput);
            }
            reloadInput.fireWeaponSlot = 0;
            reloadSession.update(
                1.0F / 60.0F,
                reloadVehicles, reloadInput);
            reloadInput = {};
            reloadInput.useHyper = true;
            reloadSession.update(
                1.0F / 60.0F,
                reloadVehicles, reloadInput);
            reloadSession.takeVelocityRequests();
            reloadInput = {};
            reloadInput.useMine = true;
            reloadSession.update(
                1.0F / 60.0F,
                reloadVehicles, reloadInput);
            const auto& reloadRacer = reloadSession.racers()[0];
            if (reloadRacer.GetPrimaryWeaponItems()[0]->GetCurCharge() !=
                    1U ||
                reloadRacer.GetHyperWeaponItem()->GetCurCharge() != 1U ||
                reloadRacer.GetMineWeaponItem()->GetCurCharge() != 1U)
            {
                throw std::runtime_error(
                    "source lap reload precondition shot failed");
            }
            reloadInput = {};
            auto placeReloadOnSegment = [&](std::size_t segment) {
                const auto& start = point(segment);
                const auto& end = point(segment + 1U);
                reloadVehicles[0].body.position = multiply(
                    add(start.position, end.position), 0.5F);
                reloadVehicles[0].body.position.z =
                    (start.position.z + end.position.z) * 0.5F + 2.0F;
                reloadVehicles[0].body.rotation = shortestArcFromX(
                    normalized2(subtract(
                        end.position, start.position)));
                reloadSession.update(
                    1.0F / 60.0F,
                    reloadVehicles, reloadInput);
            };
            for (std::size_t segment = 0U;
                 segment + 1U < race.tracePath.size(); ++segment)
                placeReloadOnSegment(segment);
            reloadVehicles[0].body.position.x += 1000.0F;
            reloadVehicles[0].body.position.y += 1000.0F;
            reloadSession.update(
                1.0F / 60.0F,
                reloadVehicles, reloadInput);
            placeReloadOnSegment(0U);
            if (reloadSession.racers()[0].car.numLaps != 1U ||
                reloadRacer.GetPrimaryWeaponItems()[0]->GetCurCharge() !=
                    2U ||
                reloadRacer.GetHyperWeaponItem()->GetCurCharge() != 2U ||
                reloadRacer.GetMineWeaponItem()->GetCurCharge() != 2U)
            {
                throw std::runtime_error(
                    "source Player::OnLapPass ReloadWeapons failed");
            }
        }
        {
            Race finishImmortalRace = race;
            finishImmortalRace.lapCount = 1U;
            finishImmortalRace.racers.resize(2U);
            OriginalRaceSession finishImmortalSession(
                finishImmortalRace);
            auto finishVehicles = vehicles;
            finishVehicles.resize(2U);
            for (auto& state : finishVehicles)
            {
                state.bodyContacts.clear();
                state.speed = 0.0F;
                state.linearVelocity = {};
            }
            RaceControl finishInput;
            for (int frame = 0; frame < 250; ++frame)
            {
                finishImmortalSession.update(
                    1.0F / 60.0F,
                    finishVehicles, finishInput);
            }
            auto placeFinishOnSegment = [&](std::size_t segment) {
                const auto& start = point(segment);
                const auto& end = point(segment + 1U);
                finishVehicles[0].body.position = multiply(
                    add(start.position, end.position), 0.5F);
                finishVehicles[0].body.position.z =
                    (start.position.z + end.position.z) * 0.5F + 2.0F;
                finishVehicles[0].body.rotation = shortestArcFromX(
                    normalized2(subtract(
                        end.position, start.position)));
                finishImmortalSession.update(
                    1.0F / 60.0F,
                    finishVehicles, finishInput);
            };
            for (std::size_t segment = 0U;
                 segment + 1U < race.tracePath.size(); ++segment)
                placeFinishOnSegment(segment);
            finishVehicles[0].body.position.x += 1000.0F;
            finishVehicles[0].body.position.y += 1000.0F;
            finishImmortalSession.update(
                1.0F / 60.0F,
                finishVehicles, finishInput);
            placeFinishOnSegment(0U);
            if (!finishImmortalSession.racers()[0].GetFinished())
            {
                throw std::runtime_error(
                    "source finished-immortality precondition failed");
            }
            const float finishedLife =
                finishImmortalSession.racers()[0].life;
            finishVehicles[1].body.position = add(
                finishVehicles[0].body.position,
                {1000.0F, 1000.0F, 0.0F});
            finishVehicles[1].speed = 10.0F;
            finishVehicles[0].bodyContacts = {
                {r3d::physics::CollisionSurface::Vehicle, 1U,
                 {-1.0F, 0.0F, 0.0F}, 20.0F, 1200000.0F}};
            finishImmortalSession.update(
                1.0F / 60.0F,
                finishVehicles, finishInput);
            const bool finishedDamageEvent = std::any_of(
                finishImmortalSession.events().begin(),
                finishImmortalSession.events().end(),
                [](const RaceEvent& event) {
                    return event.kind == RaceEventKind::Damage &&
                           event.racer == 0U;
                });
            if (finishImmortalSession.racers()[0].destroyed ||
                std::abs(
                    finishImmortalSession.racers()[0].life -
                    finishedLife) > 0.001F ||
                !finishedDamageEvent)
            {
                throw std::runtime_error(
                    "source Player::SetFinished immortal damage event failed");
            }
            finishVehicles[0].bodyContacts.clear();
            for (int frame = 0; frame < 20; ++frame)
            {
                finishImmortalSession.update(
                    1.0F / 60.0F,
                    finishVehicles, finishInput);
            }
            if (finishImmortalSession.vehicleInputs()[0].brake < 0.9F ||
                finishImmortalSession.vehicleInputs()[0].throttle > 0.1F ||
                finishImmortalSession.vehicleInputs()[0].reverse > 0.1F)
            {
                throw std::runtime_error(
                    "source CompleteRace 0.3-second brake transition failed");
            }
        }
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
            {
                OriginalRaceSession descriptorSession(race);
                PlayerProfile descriptorProfile;
                auto& descriptorSlot = descriptorProfile.slots[
                    PlayerProfile::firstWeaponSlot];
                descriptorSlot.record = primaryWeapons[0]->record;
                descriptorSlot.charge = 2U;
                descriptorSlot.hasCharge = true;
                descriptorSession.applyPlayerProfile(descriptorProfile);
                auto& descriptorRacer = const_cast<RacerRuntime&>(
                    descriptorSession.racers().front());
                auto* descriptorItem =
                    descriptorRacer.GetPrimaryWeaponItems()[0];
                if (descriptorItem == nullptr ||
                    descriptorItem->GetWpnDesc().projectiles.empty())
                {
                    throw std::runtime_error(
                        "source WeaponItem full WpnDesc fixture failed");
                }
                auto changedDescription =
                    descriptorItem->GetWpnDesc();
                auto& changedProjectile =
                    changedDescription.projectiles.front();
                changedProjectile.type = 0U;
                changedProjectile.speed = 77.0F;
                changedProjectile.relativeSpeed = false;
                changedProjectile.maximumDistance = 321.0F;
                changedProjectile.damage = 9.25F;
                descriptorItem->SetWpnDesc(changedDescription);
                auto descriptorVehicles = vehicles;
                descriptorVehicles.resize(1U);
                descriptorVehicles[0].speed = 0.0F;
                descriptorVehicles[0].linearVelocity = {};
                RaceControl descriptorInput;
                for (int frame = 0; frame < 250; ++frame)
                {
                    descriptorSession.update(
                        1.0F / 60.0F, descriptorVehicles,
                        descriptorInput);
                }
                descriptorInput.fireWeaponSlot = 0;
                descriptorSession.update(
                    1.0F / 60.0F, descriptorVehicles,
                    descriptorInput);
                const auto firedProjectile = std::find_if(
                    descriptorSession.projectiles().begin(),
                    descriptorSession.projectiles().end(),
                    [](const ProjectileRuntime& projectile) {
                        return projectile.owner == 0U;
                    });
                if (firedProjectile ==
                        descriptorSession.projectiles().end() ||
                    std::abs(firedProjectile->speed - 77.0F) >
                        0.001F ||
                    std::abs(
                        firedProjectile->maximumDistance - 321.0F) >
                        0.001F ||
                    std::abs(firedProjectile->damage - 9.25F) >
                        0.001F)
                {
                    throw std::runtime_error(
                        "source WeaponItem full WpnDesc shot ownership "
                        "failed");
                }
                const auto firedDescription =
                    firedProjectile->weaponDescription;
                if (firedDescription == nullptr ||
                    firedProjectile->descriptionProjectile >=
                        firedDescription->projectiles.size())
                {
                    throw std::runtime_error(
                        "source projectile descriptor snapshot missing");
                }
                auto replacementDescription =
                    descriptorItem->GetWpnDesc();
                auto& replacementProjectile =
                    replacementDescription.projectiles.front();
                replacementProjectile.speed = 5.0F;
                replacementProjectile.maximumDistance = 6.0F;
                replacementProjectile.damage = 1.0F;
                descriptorItem->SetWpnDesc(replacementDescription);
                const auto replacementHandle =
                    descriptorItem->GetWeapon()->GetDescHandle();
                const auto& retainedProjectile =
                    firedDescription->projectiles[
                        firedProjectile->descriptionProjectile];
                if (firedDescription == replacementHandle ||
                    std::abs(retainedProjectile.speed - 77.0F) >
                        0.001F ||
                    std::abs(
                        retainedProjectile.maximumDistance - 321.0F) >
                        0.001F ||
                    std::abs(retainedProjectile.damage - 9.25F) >
                        0.001F)
                {
                    throw std::runtime_error(
                        "source projectile descriptor snapshot lifetime "
                        "failed");
                }
            }
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
            const auto installedItems =
                weaponSession.racers().front().GetPrimaryWeaponItems();
            for (std::size_t slot = 0U; slot < 2U; ++slot)
            {
                const auto* item = installedItems[slot];
                const auto& definition = *primaryWeapons[slot];
                const float expectedDamage = std::accumulate(
                    definition.projectiles.begin(),
                    definition.projectiles.end(), 0.0F,
                    [](float value,
                       const ProjectileDefinition& projectile) {
                        return projectile.spawnOnParentDeath
                                   ? value
                                   : value + projectile.damage;
                    });
                const auto expectedProjectileCount =
                    static_cast<std::size_t>(std::count_if(
                        definition.projectiles.begin(),
                        definition.projectiles.end(),
                        [](const ProjectileDefinition& projectile) {
                            return !projectile.spawnOnParentDeath;
                        }));
                if (item == nullptr || item->GetWeapon() == nullptr ||
                    item->GetChargeCost() != definition.chargeCost ||
                    item->GetWpnDesc().projectiles.size() !=
                        expectedProjectileCount ||
                    std::abs(item->GetDamage(true) - expectedDamage) >
                        0.001F ||
                    item->GetDesc().Front().type !=
                        definition.projectiles.front().type)
                {
                    throw std::runtime_error(
                        "source WeaponItem WpnDesc/damage/chargeCost binding "
                        "failed");
                }
            }
            RaceControl weaponInput;
            for (int frame = 0; frame < 250; ++frame)
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
            const auto readPrimaryCharges = [](const RacerRuntime& racer) {
                std::array<std::uint32_t, PlayerProfile::weaponSlotCount>
                    charges{};
                const auto items = racer.GetPrimaryWeaponItems();
                for (std::size_t slot = 0U; slot < items.size(); ++slot)
                {
                    charges[slot] = items[slot] != nullptr
                                        ? items[slot]->GetCurCharge()
                                        : 0U;
                }
                return charges;
            };
            const auto beforeAll =
                readPrimaryCharges(weaponSession.racers().front());
            weaponInput.useAllWeapons = true;
            weaponSession.update(
                1.0F / 60.0F, vehicles, weaponInput);
            const auto afterAll =
                readPrimaryCharges(weaponSession.racers().front());
            if (afterAll[0] + 1U != beforeAll[0] ||
                afterAll[1] + 1U != beforeAll[1])
            {
                throw std::runtime_error(
                    "source ShotAll/per-weapon cooldown transition failed");
            }
            for (const auto* sourceWeapon : primaryWeapons)
            {
                const auto weaponIndex = static_cast<std::size_t>(
                    sourceWeapon - race.weapons.data());
                if (!sourceWeapon->shotEffect.soundPaths.empty())
                {
                    const bool emittedSound = std::any_of(
                        weaponSession.events().begin(),
                        weaponSession.events().end(),
                        [&](const RaceEvent& event) {
                            return event.kind ==
                                       RaceEventKind::EffectSound &&
                                   event.racer == 0U &&
                                   event.target == weaponIndex &&
                                   std::find(
                                       sourceWeapon->shotEffect.soundPaths.begin(),
                                       sourceWeapon->shotEffect.soundPaths.end(),
                                       event.soundPath) !=
                                       sourceWeapon->shotEffect.soundPaths.end();
                        });
                    if (!emittedSound)
                    {
                        throw std::runtime_error(
                            "source ctWeapon ShotEffect sound was not emitted");
                    }
                }
                if (sourceWeapon->shotEffect.visual.visualNodes.empty() &&
                    sourceWeapon->shotEffect.visual.particleEmitters.empty())
                    continue;
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
                readPrimaryCharges(weaponSession.racers().front());
            weaponInput.fireWeaponSlot = 1;
            weaponSession.update(
                1.0F / 60.0F, vehicles, weaponInput);
            const auto afterDirect =
                readPrimaryCharges(weaponSession.racers().front());
            if (afterDirect[0] != beforeDirect[0] ||
                afterDirect[1] + 1U != beforeDirect[1] ||
                weaponSession.racers().front().selectedWeaponSlot != 0U)
            {
                throw std::runtime_error(
                    "source Shot1..4 direct-slot transition failed");
            }

            // Logic::Shot sends cHumanShot for a human request even if the
            // selected Weapon is still inside its strict shot delay.
            weaponInput = {};
            weaponInput.fireWeaponSlot = 1;
            weaponSession.update(
                1.0F / 60.0F, vehicles, weaponInput);
            const bool dryHumanShot = std::any_of(
                weaponSession.events().begin(),
                weaponSession.events().end(),
                [](const RaceEvent& event) {
                    return event.kind == RaceEventKind::HumanShot &&
                           event.racer == 0U;
                });
            const bool dryProjectile = std::any_of(
                weaponSession.events().begin(),
                weaponSession.events().end(),
                [](const RaceEvent& event) {
                    return event.kind == RaceEventKind::WeaponFired &&
                           event.racer == 0U;
                });
            if (!dryHumanShot || dryProjectile)
            {
                throw std::runtime_error(
                    "source Logic::Shot dry human event failed");
            }

            Race infiniteRace = race;
            infiniteRace.racers.resize(1U);
            const auto infiniteDefinition = std::find_if(
                infiniteRace.weapons.begin(),
                infiniteRace.weapons.end(),
                [&](const WeaponDefinition& weapon) {
                    return weapon.record == primaryWeapons.front()->record;
                });
            if (infiniteDefinition == infiniteRace.weapons.end())
            {
                throw std::runtime_error(
                    "source infinite WeaponItem fixture missing");
            }
            infiniteDefinition->maximumCharge = 0U;
            infiniteDefinition->reloadCharge = 0U;
            OriginalRaceSession infiniteSession(infiniteRace);
            PlayerProfile infiniteProfile;
            auto& infiniteSlot = infiniteProfile.slots[
                PlayerProfile::firstWeaponSlot];
            infiniteSlot.record = infiniteDefinition->record;
            infiniteSlot.charge = 0U;
            infiniteSlot.hasCharge = true;
            infiniteSession.applyPlayerProfile(infiniteProfile);
            auto infiniteVehicles = vehicles;
            infiniteVehicles.resize(1U);
            RaceControl infiniteInput;
            for (int frame = 0; frame < 250; ++frame)
            {
                infiniteSession.update(
                    1.0F / 60.0F,
                    infiniteVehicles, infiniteInput);
            }
            infiniteInput.fireWeaponSlot = 0;
            infiniteSession.update(
                1.0F / 60.0F,
                infiniteVehicles, infiniteInput);
            const bool infiniteShot = std::any_of(
                infiniteSession.events().begin(),
                infiniteSession.events().end(),
                [](const RaceEvent& event) {
                    return event.kind == RaceEventKind::WeaponFired &&
                           event.racer == 0U;
                });
            if (!infiniteShot ||
                infiniteSession.racers()
                        .front()
                        .GetPrimaryWeaponItems()[0]
                        ->GetCurCharge() != 0U)
            {
                throw std::runtime_error(
                    "source WeaponItem maxCharge==0 shot failed");
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
            Race hyperRace = race;
            const auto hyperVehicleIndex =
                hyperRace.racers.front().vehicle;
            auto& hyperVehicleDefinition =
                hyperRace.racers.front().hasConfiguredVehicle
                    ? hyperRace.racers.front().configuredVehicle
                    : hyperRace.vehicles[hyperVehicleIndex];
            auto& hyperMount =
                hyperVehicleDefinition.slotMounts[
                    static_cast<std::size_t>(GarageSlotType::Hyper)];
            hyperMount.active = true;
            hyperMount.show = true;
            hyperMount.position = {3.0F, 2.0F, 1.0F};
            hyperMount.placements = {
                {hyperdrive->record, {}, {0.25F, -0.5F, 0.75F}}};
            OriginalRaceSession hyperSession(hyperRace);
            PlayerProfile hyperProfile;
            auto& slot =
                hyperProfile.slots[PlayerProfile::hyperSlot];
            slot.record = hyperdrive->record;
            slot.charge = 2U;
            slot.hasCharge = true;
            hyperSession.applyPlayerProfile(hyperProfile);
            RaceControl hyperInput;
            for (int frame = 0; frame < 250; ++frame)
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
            Transform sourceSlotTransform;
            sourceSlotTransform.position = {3.25F, 1.5F, 1.75F};
            Transform sourceProjectileTransform;
            sourceProjectileTransform.position =
                hyperdrive->projectiles.front().position;
            sourceProjectileTransform.rotation =
                hyperdrive->projectiles.front().rotation;
            const Vec3 expectedAttachedPosition = compose(
                hyperVehicles[0].body,
                compose(sourceSlotTransform,
                        compose(hyperdrive->visual.transform,
                                sourceProjectileTransform)))
                                                     .position;
            const bool attachedPositionMatches = std::any_of(
                hyperSession.projectiles().begin(),
                hyperSession.projectiles().end(),
                [&](const ProjectileRuntime& projectile) {
                    return projectile.weapon ==
                               static_cast<std::size_t>(
                                   hyperdrive - race.weapons.begin()) &&
                           distanceSquared(
                               projectile.position,
                               expectedAttachedPosition) < 0.000001F;
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
                hyperSession.racers()
                        .front()
                        .GetHyperWeaponItem()->GetCurCharge() != 1U ||
                !attachedSourceEffect || !attachedPositionMatches ||
                syntheticHyperEffect)
            {
                const auto* installedSlot =
                    hyperSession.racers().front().GetSlotInst(
                        source::PlayerSlotType::Hyper);
                const auto installedPosition =
                    installedSlot != nullptr
                        ? installedSlot->GetItem().GetPos()
                        : std::array<float, 3>{};
                const auto actualPosition =
                    hyperSession.projectiles().empty()
                        ? Vec3{}
                        : hyperSession.projectiles().back().position;
                throw std::runtime_error(
                    "source ptHyper slot transform/local impulse/linked "
                    "visual failed: requests=" +
                    std::to_string(requests.size()) + " charge=" +
                    std::to_string(
                        hyperSession.racers()
                            .front()
                            .GetHyperWeaponItem()->GetCurCharge()) +
                    " attached=" +
                    std::to_string(attachedSourceEffect) + " position=" +
                    std::to_string(attachedPositionMatches) + " slot=(" +
                    std::to_string(installedPosition[0]) + "," +
                    std::to_string(installedPosition[1]) + "," +
                    std::to_string(installedPosition[2]) + ") actual=(" +
                    std::to_string(actualPosition.x) + "," +
                    std::to_string(actualPosition.y) + "," +
                    std::to_string(actualPosition.z) + ") expected=(" +
                    std::to_string(expectedAttachedPosition.x) + "," +
                    std::to_string(expectedAttachedPosition.y) + "," +
                    std::to_string(expectedAttachedPosition.z) + ")");
            }
            hyperSession.update(
                1.0F / 60.0F, hyperVehicles, hyperInput);
            if (hyperSession.racers()
                    .front()
                    .GetHyperWeaponItem()->GetCurCharge() != 1U)
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
            for (int frame = 0; frame < 250; ++frame)
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
                springSession.racers()
                        .front()
                        .GetHyperWeaponItem()->GetCurCharge() != 0U ||
                springSession.racers().front()
                        .gameCar.GetSpringTime() < 1.49F ||
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
            for (int frame = 0; frame < 250; ++frame)
                airborneSpringSession.update(
                    1.0F / 60.0F, hyperVehicles, springInput);
            airborneSpringSession.takeVelocityRequests();
            springInput.useHyper = true;
            airborneSpringSession.update(
                1.0F / 60.0F, hyperVehicles, springInput);
            if (airborneSpringSession.racers()
                    .front()
                    .GetHyperWeaponItem()->GetCurCharge() != 1U ||
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
            for (int frame = 0; frame < 250; ++frame)
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
        if (vehicles.size() > 1U)
        {
            Race legacySonarRace = race;
            const std::size_t legacySonarWeapon =
                static_cast<std::size_t>(
                    rocketLauncher - race.weapons.begin());
            auto& legacySonarDefinition =
                legacySonarRace.weapons[legacySonarWeapon];
            legacySonarDefinition.record = "sourceLegacySonar";
            legacySonarDefinition.name = "source legacy ptSonar";
            legacySonarDefinition.projectiles.resize(1U);
            auto& legacySonarProjectile =
                legacySonarDefinition.projectiles.front();
            legacySonarProjectile.type = 16U;
            legacySonarProjectile.damage = 10.0F;
            legacySonarProjectile.mass = 2.0F;
            legacySonarProjectile.maximumDistance = 10000.0F;
            legacySonarProjectile.collision.center = {};
            legacySonarProjectile.collision.halfExtents = {
                100.0F, 100.0F, 100.0F};

            OriginalRaceSession legacySonarSession(legacySonarRace);
            PlayerProfile legacySonarProfile;
            auto& slot = legacySonarProfile.slots[
                PlayerProfile::firstWeaponSlot];
            slot.record = legacySonarDefinition.record;
            slot.charge = 2U;
            slot.hasCharge = true;
            legacySonarSession.applyPlayerProfile(legacySonarProfile);
            auto legacySonarVehicles = vehicles;
            for (std::size_t index = 0U;
                 index < legacySonarVehicles.size(); ++index)
            {
                legacySonarVehicles[index].body.position = {
                    100000.0F + static_cast<float>(index) * 1000.0F,
                    100000.0F, 1000.0F};
                legacySonarVehicles[index].body.rotation = {};
                legacySonarVehicles[index].linearVelocity = {};
                legacySonarVehicles[index].bodyContacts.clear();
            }
            RaceControl legacySonarInput;
            for (int frame = 0; frame < 250; ++frame)
            {
                legacySonarSession.update(
                    1.0F / 60.0F, legacySonarVehicles,
                    legacySonarInput);
            }
            legacySonarInput.useWeapon = true;
            legacySonarSession.update(
                1.0F / 60.0F, legacySonarVehicles,
                legacySonarInput);
            legacySonarInput.useWeapon = false;
            const auto activeLegacySonar = std::find_if(
                legacySonarSession.projectiles().begin(),
                legacySonarSession.projectiles().end(),
                [legacySonarWeapon](
                    const ProjectileRuntime& projectile) {
                    return projectile.owner == 0U &&
                           projectile.weapon == legacySonarWeapon &&
                           projectile.projectile == 0U &&
                           projectile.active;
                });
            if (activeLegacySonar ==
                legacySonarSession.projectiles().end())
            {
                throw std::runtime_error(
                    "source ptSonar integration projectile was not created");
            }
            legacySonarVehicles[1].body.position =
                activeLegacySonar->position;
            const float lifeBeforeLegacySonar =
                legacySonarSession.racers()[1].life;
            constexpr float legacySonarStep = 0.1F;
            legacySonarSession.update(
                legacySonarStep, legacySonarVehicles,
                legacySonarInput);
            const float legacySonarDamage =
                lifeBeforeLegacySonar -
                legacySonarSession.racers()[1].life;
            const auto legacySonarImpulse =
                legacySonarSession.takeVelocityRequests();
            // SonarContact already multiplies damage by contact.deltaTime.
            // The old adapter multiplied that returned value a second time,
            // reducing 10 * 0.1 to 0.1 (or less after a reflector).
            if (legacySonarDamage <= 0.2F ||
                legacySonarDamage >
                    legacySonarProjectile.damage * legacySonarStep +
                        0.001F ||
                legacySonarImpulse.empty())
            {
                throw std::runtime_error(
                    "source ptSonar continuous damage/impulse transition "
                    "failed");
            }
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
            for (int frame = 0; frame < 250; ++frame)
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
            // Proj::CalcSpeed adds the vehicle projection before it levels
            // shallow launch directions onto the XY plane.
            const float expectedSourceSpeed =
                thunder->projectiles.front().speed +
                80.0F * std::cos(sourcePitch);
            if (sourceProjectile ==
                    thunderSession.projectiles().end() ||
                std::abs(
                    sourceProjectile->speed - expectedSourceSpeed) >
                    0.001F ||
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
                    thunder->projectiles.front().maximumDistance ||
                !sourceProjectile->ownerCollisionArmed)
            {
                throw std::runtime_error(
                    "source maxDist/speed lifetime was replaced by a "
                    "distance clamp or owner contact never armed");
            }
            const float stepSeconds = 1.0F / 60.0F;
            Transform returnedProjectile;
            returnedProjectile.position = add(
                sourceProjectile->position,
                multiply(
                    sourceProjectile->direction,
                    std::max(sourceProjectile->speed, 1.0F) *
                        stepSeconds));
            returnedProjectile.rotation = sourceProjectile->rotation;
            const OrientedBox returnedBox = orientedBox(
                returnedProjectile,
                thunder->projectiles.front().collision);
            const auto& ownerRacer = race.racers.front();
            const auto& ownerVehicle =
                ownerRacer.hasConfiguredVehicle
                    ? ownerRacer.configuredVehicle
                    : race.vehicles.at(ownerRacer.vehicle);
            outsideVehicles[0].body.rotation = {};
            outsideVehicles[0].body.position = subtract(
                returnedBox.center,
                ownerVehicle.physics.shapePosition);
            const float ownerLife =
                thunderSession.racers().front().life;
            thunderSession.update(
                stepSeconds, outsideVehicles, thunderInput);
            sourceProjectile = std::find_if(
                thunderSession.projectiles().begin(),
                thunderSession.projectiles().end(),
                [thunderWeapon](
                    const ProjectileRuntime& projectile) {
                    return projectile.owner == 0U &&
                           projectile.weapon == thunderWeapon &&
                           projectile.projectile == 0U;
                });
            if (sourceProjectile !=
                    thunderSession.projectiles().end() ||
                thunderSession.racers().front().life >= ownerLife)
            {
                throw std::runtime_error(
                    "source projectile-to-owner contact after launch "
                    "separation failed");
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
            for (int frame = 0; frame < 250; ++frame)
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
            for (int frame = 0; frame < 250; ++frame)
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
            Race impulseRace = race;
            auto& lethalTarget = impulseRace.racers[1];
            if (!lethalTarget.hasConfiguredVehicle)
            {
                lethalTarget.configuredVehicle =
                    impulseRace.vehicles.at(lethalTarget.vehicle);
                lethalTarget.hasConfiguredVehicle = true;
            }
            // ImpulseContact continues FindClosestEnemy from the contacted
            // player even when Damage killed that player in the same
            // callback.
            lethalTarget.configuredVehicle.maximumLife = 1.0F;
            OriginalRaceSession impulseSession(impulseRace);
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
            for (int frame = 0; frame < 250; ++frame)
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
                !impulseSession.racers()[1].destroyed ||
                impulseSession.racers()[1].life >=
                    impulseSession.racers()[1].maximumLife)
            {
                throw std::runtime_error(
                    "source lethal ImpulseContact FindClosestEnemy(pi/2) "
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
            for (int frame = 0; frame < 250; ++frame)
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
            for (int frame = 0; frame < 250; ++frame)
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
                std::abs(frostSession.racers()[1]
                             .slowEffect.GetRemainingSeconds() - 1.0F) >
                    0.001F ||
                frostSession.racers()[1].slowEffect.GetWeapon() !=
                    frostWeapon ||
                frostSession.racers()[1]
                        .slowEffect.GetProjectile() != 0U)
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
            if (frostSession.racers()[1]
                    .slowEffect.GetRemainingSeconds() >= 0.99F ||
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
            if (frostSession.racers()[1]
                    .slowEffect.IsEffectMaked() ||
                frostSession.racers()[1].slowEffect.GetWeapon() !=
                    RacerRuntime::invalidWeapon ||
                frostSession.racers()[1]
                        .slowEffect.GetProjectile() !=
                    RacerRuntime::invalidWeapon ||
                energyDamageCount() != 0)
            {
                throw std::runtime_error(
                    "source Frost SlowEffect did not expire with its "
                    "model");
            }
        }
        {
            Race lifetimeRace = race;
            const std::size_t lifetimeWeapon =
                static_cast<std::size_t>(
                    tankLaser - race.weapons.begin());
            auto& lifetimeDefinition =
                lifetimeRace.weapons[lifetimeWeapon]
                    .projectiles.front();
            lifetimeDefinition.minimumLife = 0.05F;
            lifetimeDefinition.maximumLife = 0.05F;
            OriginalRaceSession lifetimeSession(lifetimeRace);
            PlayerProfile lifetimeProfile;
            auto& slot = lifetimeProfile.slots[
                PlayerProfile::firstWeaponSlot];
            slot.record = tankLaser->record;
            slot.charge = 1U;
            slot.hasCharge = true;
            lifetimeSession.applyPlayerProfile(lifetimeProfile);
            auto lifetimeVehicles = vehicles;
            for (std::size_t index = 0U;
                 index < lifetimeVehicles.size(); ++index)
            {
                lifetimeVehicles[index].body.position = {
                    300000.0F + static_cast<float>(index) * 1000.0F,
                    300000.0F, 3000.0F};
                lifetimeVehicles[index].body.rotation = {};
                lifetimeVehicles[index].linearVelocity = {};
            }
            RaceControl lifetimeInput;
            for (int frame = 0; frame < 250; ++frame)
            {
                lifetimeSession.update(
                    1.0F / 60.0F, lifetimeVehicles,
                    lifetimeInput);
            }
            lifetimeInput.useWeapon = true;
            lifetimeSession.update(
                1.0F / 60.0F, lifetimeVehicles, lifetimeInput);
            lifetimeInput.useWeapon = false;
            const auto isLifetimeRay =
                [lifetimeWeapon](const ProjectileRuntime& projectile) {
                    return projectile.owner == 0U &&
                           projectile.weapon == lifetimeWeapon &&
                           projectile.projectile == 0U &&
                           projectile.attached;
                };
            auto lifetimeRay = std::find_if(
                lifetimeSession.projectiles().begin(),
                lifetimeSession.projectiles().end(), isLifetimeRay);
            if (lifetimeRay == lifetimeSession.projectiles().end() ||
                lifetimeRay->lifeSeconds <= 0.0F)
            {
                throw std::runtime_error(
                    "source GameObject lifetime was not sampled");
            }
            const float sampledLifetime = lifetimeRay->lifeSeconds;
            lifetimeSession.update(
                sampledLifetime, lifetimeVehicles, lifetimeInput);
            lifetimeRay = std::find_if(
                lifetimeSession.projectiles().begin(),
                lifetimeSession.projectiles().end(), isLifetimeRay);
            if (lifetimeRay == lifetimeSession.projectiles().end() ||
                std::abs(lifetimeRay->lifeSeconds) > 0.001F ||
                lifetimeRay->impactDistance <= 0.0F ||
                std::abs(
                    lifetimeRay->beamTextureScale -
                    lifetimeRay->impactDistance / 10.0F) > 0.001F)
            {
                throw std::runtime_error(
                    "source GameObject lifetime/laser sampler state "
                    "failed at equality");
            }
            lifetimeSession.update(
                0.001F, lifetimeVehicles, lifetimeInput);
            lifetimeRay = std::find_if(
                lifetimeSession.projectiles().begin(),
                lifetimeSession.projectiles().end(), isLifetimeRay);
            if (lifetimeRay != lifetimeSession.projectiles().end())
            {
                throw std::runtime_error(
                    "source GameObject strict lifetime did not expire "
                    "after crossing the limit");
            }

            OriginalRaceSession destroySession(lifetimeRace);
            destroySession.applyPlayerProfile(lifetimeProfile);
            for (int frame = 0; frame < 250; ++frame)
            {
                destroySession.update(
                    1.0F / 60.0F, lifetimeVehicles,
                    lifetimeInput);
            }
            lifetimeInput.useWeapon = true;
            destroySession.update(
                1.0F / 60.0F, lifetimeVehicles,
                lifetimeInput);
            lifetimeInput.useWeapon = false;
            if (std::none_of(
                    destroySession.projectiles().begin(),
                    destroySession.projectiles().end(),
                    isLifetimeRay))
            {
                throw std::runtime_error(
                    "source linked projectile destroy regression was not "
                    "created");
            }
            destroySession.disconnectNetworkRacer(0U);
            destroySession.update(
                1.0F / 60.0F, lifetimeVehicles,
                lifetimeInput);
            if (std::any_of(
                    destroySession.projectiles().begin(),
                    destroySession.projectiles().end(),
                    isLifetimeRay))
            {
                throw std::runtime_error(
                    "source Proj::OnDestroy retained LinkToWeapon child");
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
            for (int frame = 0; frame < 250; ++frame)
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
            for (int frame = 0; frame < 250; ++frame)
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
            for (int frame = 0; frame < 250; ++frame)
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
            const float sourceOffset =
                sourceProjectile.surfacePlacementOffset;
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
            for (int frame = 0; frame < 250; ++frame)
            {
                rejectedMineSession.update(
                    1.0F / 60.0F, outsideVehicles, mineInput);
            }
            mineInput.useMine = true;
            rejectedMineSession.update(
                1.0F / 60.0F, outsideVehicles, mineInput);
            if (!rejectedMineSession.mines().empty() ||
                rejectedMineSession.racers()
                        .front()
                        .GetMineWeaponItem()->GetCurCharge() != 1U)
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
            oilMine->projectiles.front().type != 10U ||
            std::abs(
                oilMine->projectiles.front().surfacePlacementOffset -
                0.05F) > 0.0001F ||
            oilMine->projectiles.front().collision.halfExtents.z < 0.8F)
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
                for (int frame = 0; frame < 250; ++frame)
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
                !contactSession.racers()[0]
                     .gameCar.IsMineLocked())
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
            sourceVehicles[0].angularMomentum =
                {11.0F, 22.0F, 33.0F};
            sourceVehicles[0].speed = 10.0F;
            advanceMineCountdown(
                oilSession, sourceVehicles);
            RaceControl sourceInput;
            sourceInput.useMine = true;
            oilSession.update(
                1.0F / 60.0F, sourceVehicles, sourceInput);
            sourceInput.useMine = false;
            const auto earlyMomentum =
                oilSession.takeAngularMomentumRequests();
            for (int frame = 0; frame < 30; ++frame)
            {
                oilSession.update(
                    1.0F / 60.0F, sourceVehicles,
                    sourceInput);
            }
            const auto armedMomentum =
                oilSession.takeAngularMomentumRequests();
            const auto& oilRacer = race.racers.front();
            const auto& oilVehicle =
                oilRacer.hasConfiguredVehicle
                    ? oilRacer.configuredVehicle
                    : race.vehicles.at(oilRacer.vehicle);
            const float expectedOilYawMomentum =
                std::abs(
                    oilMine->projectiles.front().damage *
                    oilVehicle.physics.mass);
            const bool oilLockedClutch = std::any_of(
                armedMomentum.begin(), armedMomentum.end(),
                [&](const AngularMomentumRequest& request) {
                    return request.racer == 0U &&
                           std::abs(request.momentum.x - 11.0F) < 0.001F &&
                           std::abs(request.momentum.y - 22.0F) < 0.001F &&
                           std::abs(
                               std::abs(request.momentum.z) -
                               expectedOilYawMomentum) < 0.01F;
                });
            if (!earlyMomentum.empty() ||
                oilSession.mines().size() != 1U ||
                !oilLockedClutch ||
                !oilSession.racers()[0]
                     .gameCar.IsClutchLocked())
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
            for (int frame = 0; frame < 250; ++frame)
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

        {
            Race networkRace = race;
            if (networkRace.racers.size() < 2U || vehicles.size() < 2U)
            {
                throw std::runtime_error(
                    "source network gameplay regression needs two racers");
            }
            networkRace.racers[1].human = true;
            networkRace.racers[1].playerId =
                2 << source::Player::opponentBit;
            networkRace.racers[1].netSlot = 2U;
            OriginalRaceSession networkSession(networkRace);
            if (networkSession.racers()[0].GetCheat() !=
                    source::Player::cheatDisabled ||
                networkSession.racers()[1].GetCheat() !=
                    source::Player::cheatDisabled ||
                networkSession.racerHasAiController(1U))
            {
                throw std::runtime_error(
                    "NetPlayer human/opponent received non-source AI cheat");
            }
            std::vector<bool> clientOwned(
                networkRace.racers.size(), false);
            clientOwned[0] = true;
            networkSession.setNetworkGameplayRole(
                true, false, clientOwned);

            const float initialLife =
                networkSession.racers().front().life;
            const auto deferred =
                networkSession.applyNetworkPlayerDamage(
                    0U, 1U, vehicles[0].body.position, 7.0F,
                    DamageType::Simple, vehicles[0]);
            if (std::abs(deferred.life - initialLife) > 0.001F ||
                networkSession.events().empty() ||
                !networkSession.events().back().networkReplicated)
            {
                throw std::runtime_error(
                    "source client NetRace::Damage changed life before "
                    "the host response");
            }

            std::vector<bool> hostOwned(
                networkRace.racers.size(), true);
            networkSession.setNetworkGameplayRole(
                true, true, hostOwned);
            const auto authoritative =
                networkSession.applyNetworkPlayerDamage(
                    0U, 1U, vehicles[0].body.position, 7.0F,
                    DamageType::Simple, vehicles[0]);
            if (std::abs(
                    authoritative.life - (initialLife - 7.0F)) >
                    0.001F || authoritative.death)
            {
                throw std::runtime_error(
                    "source host NetRace::Damage authority failed");
            }

            networkSession.setNetworkGameplayRole(
                true, false, clientOwned);
            const auto synchronized =
                networkSession.applyNetworkPlayerDamage(
                    0U, 1U, vehicles[0].body.position, 2.0F,
                    DamageType::Energy, vehicles[0], true,
                    23.0F, false);
            if (std::abs(synchronized.life - 23.0F) > 0.001F ||
                synchronized.death)
            {
                throw std::runtime_error(
                    "source client authoritative targetLife sync failed");
            }

            const auto destructible = std::find_if(
                networkRace.decorationInstances.begin(),
                networkRace.decorationInstances.end(),
                [&](const ObjectInstance& instance) {
                    return instance.definition <
                               networkRace.decorationDefinitions.size() &&
                           networkRace.decorationDefinitions[
                               instance.definition].destructible &&
                           instance.mapObjectId != 0U;
                });
            if (destructible ==
                networkRace.decorationInstances.end())
            {
                throw std::runtime_error(
                    "source network Damage2 regression has no MapObj");
            }
            const auto decorationIndex = static_cast<std::size_t>(
                destructible -
                networkRace.decorationInstances.begin());
            OriginalRaceSession mapHost(networkRace);
            mapHost.setNetworkGameplayRole(true, true, hostOwned);
            if (mapHost.decorationForMapObjectId(
                    destructible->mapObjectId) != decorationIndex ||
                mapHost.racerForMapObjectId(
                    networkRace.racers.front().mapObjectId) != 0U ||
                (!networkRace.bonuses.empty() &&
                 mapHost.bonusForMapObjectId(
                     networkRace.bonuses.front().mapObjectId) != 0U))
            {
                throw std::runtime_error(
                    "source Map global ID registry did not resolve "
                    "decoration/car/bonus owners");
            }
            const float decorationInitial =
                mapHost.decorationLife()[decorationIndex];
            const auto mapAuthoritative =
                mapHost.applyNetworkMapObjectDamage(
                    destructible->mapObjectId, 0U, 1.0F,
                    DamageType::Simple);
            const float decorationExpected =
                decorationInitial - 1.0F;
            const bool decorationExpectedDeath =
                decorationExpected <= 0.0F;
            if (std::abs(mapAuthoritative.life -
                         decorationExpected) > 0.001F ||
                mapAuthoritative.death != decorationExpectedDeath)
            {
                throw std::runtime_error(
                    "source host NetRace::Damage2 authority failed");
            }
            OriginalRaceSession mapClient(networkRace);
            mapClient.setNetworkGameplayRole(
                true, false, clientOwned);
            const auto mapSynchronized =
                mapClient.applyNetworkMapObjectDamage(
                    destructible->mapObjectId, 0U, 1.0F,
                    DamageType::Energy, true, 0.0F, true);
            if (!mapSynchronized.death ||
                mapClient.decorationActive()[decorationIndex] ||
                mapClient.decorationForMapObjectId(
                    destructible->mapObjectId) !=
                    RacerRuntime::invalidWeapon)
            {
                throw std::runtime_error(
                    "source client NetRace::Damage2 death sync failed");
            }

            RaceControl networkInput;
            networkSession.synchronizeNetworkCountdown(4);
            networkSession.update(
                1.0F / 60.0F, vehicles, networkInput);
            const auto& remoteInput =
                networkSession.vehicleInputs()[1];
            if (std::abs(remoteInput.throttle) > 0.0001F ||
                std::abs(remoteInput.reverse) > 0.0001F ||
                std::abs(remoteInput.brake) > 0.0001F ||
                std::abs(remoteInput.steering) > 0.0001F)
            {
                throw std::runtime_error(
                    "remote human received local AI control");
            }
            const auto disconnectedMapObjectId =
                networkSession.racerMapObjectId(1U);
            if (!networkSession.disconnectNetworkRacer(1U) ||
                networkSession.disconnectNetworkRacer(1U) ||
                !networkSession.racers()[1].disconnected ||
                !networkSession.racers()[1].destroyed ||
                networkSession.racerHasAiController(1U) ||
                networkSession.racerMapObjectId(1U) !=
                    source::Map::defaultMapObjId ||
                networkSession.racerForMapObjectId(
                    disconnectedMapObjectId) !=
                    RacerRuntime::invalidWeapon)
            {
                throw std::runtime_error(
                    "NetPlayer destructor racer removal state failed");
            }
            networkSession.update(
                1.0F / 60.0F, vehicles, networkInput);
            const auto& disconnectedInput =
                networkSession.vehicleInputs()[1];
            if (std::abs(disconnectedInput.throttle) > 0.0001F ||
                std::abs(disconnectedInput.reverse) > 0.0001F ||
                std::abs(disconnectedInput.brake) > 0.0001F ||
                std::abs(disconnectedInput.steering) > 0.0001F ||
                !networkSession.takeRespawns().empty())
            {
                throw std::runtime_error(
                    "disconnected NetPlayer resumed gameplay");
            }

            OriginalRaceSession shotSource(networkRace);
            shotSource.setNetworkGameplayRole(
                true, true, hostOwned);
            shotSource.synchronizeNetworkCountdown(4);
            std::size_t shotSlot =
                RacerRuntime::invalidWeapon;
            for (std::size_t slot = 0U;
                 slot < shotSource.racers()[0].weaponSlots.size(); ++slot)
            {
                if (shotSource.racers()[0].weaponSlots[slot] !=
                        RacerRuntime::invalidWeapon &&
                    shotSource.racers()[0]
                            .GetPrimaryWeaponItems()[slot]
                            ->GetCurCharge() > 0U)
                {
                    shotSlot = slot;
                    break;
                }
            }
            if (shotSlot == RacerRuntime::invalidWeapon)
            {
                throw std::runtime_error(
                    "source network shot regression has no charged slot");
            }
            // synchronizeNetworkCountdown(4) intentionally jumps directly
            // to cGoRace. In the real network flow Weapon::OnProgress has
            // already accumulated time during cGoRaceWait/cGoRace1..3, so
            // warm the local sender through its serialized strict delay.
            const std::size_t shotWeapon =
                shotSource.racers()[0].weaponSlots[shotSlot];
            const float shotDelay =
                shotWeapon < networkRace.weapons.size()
                    ? networkRace.weapons[shotWeapon].shotDelay
                    : 0.0F;
            RaceControl warmupInput;
            const int warmupFrames = std::max(
                1, static_cast<int>(std::ceil(
                       std::max(shotDelay, 0.0F) * 60.0F)) + 1);
            for (int frame = 0; frame < warmupFrames; ++frame)
            {
                shotSource.update(
                    1.0F / 60.0F, vehicles, warmupInput);
            }
            RaceControl shotInput;
            shotInput.fireWeaponSlot =
                static_cast<int>(shotSlot);
            shotSource.update(
                1.0F / 60.0F, vehicles, shotInput);
            const auto sourceShot = std::find_if(
                shotSource.events().begin(),
                shotSource.events().end(),
                [](const RaceEvent& event) {
                    return event.kind == RaceEventKind::WeaponFired &&
                           event.racer == 0U &&
                           !event.networkReplicated &&
                           event.networkSlotMask != 0U &&
                           !event.networkCoordinates.empty();
                });
            if (sourceShot == shotSource.events().end() ||
                sourceShot->networkProjectileId != 1U ||
                shotSource.racers()[0]
                        .GetNextBonusProjectileId() != 1U)
            {
                throw std::runtime_error(
                    "source NetPlayer::DoShot packet metadata/id owner "
                    "missing");
            }

            OriginalRaceSession shotTarget(networkRace);
            shotTarget.setNetworkGameplayRole(
                true, false, clientOwned);
            shotTarget.synchronizeNetworkCountdown(4);
            ReplicatedShot replicated;
            replicated.racer = 0U;
            replicated.slotMask = sourceShot->networkSlotMask;
            replicated.projectileId =
                sourceShot->networkProjectileId;
            replicated.coordinates =
                sourceShot->networkCoordinates;
            shotTarget.queueNetworkShot(std::move(replicated));
            RaceControl noShotInput;
            shotTarget.update(
                1.0F / 60.0F, vehicles, noShotInput);
            const auto targetShot = std::find_if(
                shotTarget.events().begin(),
                shotTarget.events().end(),
                [&](const RaceEvent& event) {
                    return event.kind == RaceEventKind::WeaponFired &&
                           event.racer == 0U &&
                           event.networkReplicated &&
                           event.networkSlotMask ==
                               sourceShot->networkSlotMask &&
                           event.networkProjectileId ==
                               sourceShot->networkProjectileId &&
                           !event.networkCoordinates.empty() &&
                           length3(subtract(
                               event.networkCoordinates.front(),
                               sourceShot->networkCoordinates.front())) <
                               0.001F;
                });
            if (targetShot == shotTarget.events().end() ||
                shotTarget.racers()[0]
                        .GetNextBonusProjectileId() != 1U)
            {
                throw std::runtime_error(
                    "source NetPlayer::DoShot replay failed");
            }

            const auto sourceBonus = std::find_if(
                networkRace.bonuses.begin(),
                networkRace.bonuses.end(),
                [](const BonusInstance& bonus) {
                    return bonus.kind == BonusKind::Medpack;
                });
            if (sourceBonus == networkRace.bonuses.end())
            {
                throw std::runtime_error(
                    "source network bonus regression has no pickup");
            }
            const std::size_t bonusIndex =
                static_cast<std::size_t>(std::distance(
                    networkRace.bonuses.begin(), sourceBonus));
            OriginalRaceSession bonusTarget(networkRace);
            bonusTarget.setNetworkGameplayRole(
                true, false, clientOwned);
            bonusTarget.synchronizeNetworkCountdown(4);
            const float synchronizedLife = std::max(
                bonusTarget.racers().front().maximumLife - 5.0F,
                1.0F);
            bonusTarget.applyNetworkPlayerDamage(
                0U, RacerRuntime::invalidWeapon,
                vehicles[0].body.position, 5.0F,
                DamageType::Simple, vehicles[0], true,
                synchronizedLife, false);
            const float lifeBeforeBonus =
                bonusTarget.racers().front().life;
            const float sourceNetworkBonusValue =
                sourceBonus->value > 0.0F
                    ? sourceBonus->value
                    : bonusTarget.racers().front().maximumLife;
            const float expectedBonusLife = std::min(
                lifeBeforeBonus + sourceNetworkBonusValue,
                bonusTarget.racers().front().maximumLife);
            bonusTarget.queueNetworkBonus(
                {0U, bonusIndex, sourceBonus->kind,
                 sourceNetworkBonusValue});
            bonusTarget.update(
                1.0F / 60.0F, vehicles, noShotInput);
            const auto bonusEvent = std::find_if(
                bonusTarget.events().begin(),
                bonusTarget.events().end(),
                [&](const RaceEvent& event) {
                    return event.kind == RaceEventKind::Bonus &&
                           event.racer == 0U &&
                           event.target == bonusIndex &&
                           event.networkReplicated;
                });
            if (bonusEvent == bonusTarget.events().end() ||
                bonusTarget.bonusActive()[bonusIndex] ||
                std::abs(
                    bonusTarget.racers().front().life -
                    expectedBonusLife) > 0.001F)
            {
                throw std::runtime_error(
                    "source NetPlayer::OnTakeBonus replay failed");
            }

            OriginalRaceSession mineTarget(networkRace);
            mineTarget.applyPlayerProfile(
                mineProfileFor(*mineSpike));
            std::vector<bool> targetOwned(
                networkRace.racers.size(), false);
            targetOwned[1] = true;
            mineTarget.setNetworkGameplayRole(
                true, false, targetOwned);
            mineTarget.synchronizeNetworkCountdown(4);
            auto mineVehicles = isolatedMineVehicles();
            const auto mineTransform = expectedMineTransform(
                *mineSpike, mineVehicles);
            const auto& mineTargetRacer = networkRace.racers[1];
            const auto& mineTargetDefinition =
                mineTargetRacer.hasConfiguredVehicle
                    ? mineTargetRacer.configuredVehicle
                    : networkRace.vehicles.at(
                          mineTargetRacer.vehicle);
            const Vec3 mineCenter = add(
                mineTransform.position,
                rotate(
                    mineTransform.rotation,
                    mineSpike->projectiles.front().collision.center));
            mineVehicles[1].body.rotation = {};
            mineVehicles[1].body.position = subtract(
                add(mineCenter, {0.0F, 0.25F, 0.0F}),
                mineTargetDefinition.physics.shapePosition);
            ReplicatedShot replicatedMine;
            replicatedMine.racer = 0U;
            replicatedMine.slotMask = 0x02U;
            replicatedMine.projectileId = 77U;
            replicatedMine.coordinates.push_back(
                mineTransform.position);
            mineTarget.queueNetworkShot(std::move(replicatedMine));
            mineTarget.update(
                1.0F / 60.0F, mineVehicles, noShotInput);
            mineTarget.update(
                1.0F / 60.0F, mineVehicles, noShotInput);
            const auto mineContact = std::find_if(
                mineTarget.events().begin(),
                mineTarget.events().end(),
                [](const RaceEvent& event) {
                    return event.kind == RaceEventKind::MineContact &&
                           event.racer == 1U && event.target == 0U &&
                           event.networkProjectileId == 77U &&
                           !event.networkMapObject;
                });
            if (mineContact == mineTarget.events().end() ||
                mineTarget.mines().size() != 1U ||
                !mineTarget.racers()[0].HasBonusProjectile(77U) ||
                mineTarget.racers()[0]
                        .GetNextBonusProjectileId() != 78U)
            {
                throw std::runtime_error(
                    "source target-owned MineContact request failed");
            }
            ReplicatedMineContact replicatedContact;
            replicatedContact.racer = 1U;
            replicatedContact.projectileOwner = 0U;
            replicatedContact.projectileId = 77U;
            replicatedContact.point = mineContact->position;
            mineTarget.queueNetworkMineContact(
                std::move(replicatedContact));
            mineTarget.update(
                1.0F / 60.0F, mineVehicles, noShotInput);
            if (!mineTarget.mines().empty() ||
                mineTarget.racers()[0].HasBonusProjectile(77U) ||
                mineTarget.racers()[0]
                        .GetNextBonusProjectileId() != 78U)
            {
                throw std::runtime_error(
                    "source NetPlayer::OnMineContact replay failed");
            }
        }

        const auto manualResetMapObjectId =
            session.racerMapObjectId(session.humanRacer());
        input.reset = true;
        session.update(1.0F / 60.0F, vehicles, input);
        if (session.takeRespawns().empty() ||
            session.racerMapObjectId(session.humanRacer()) !=
                manualResetMapObjectId)
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
