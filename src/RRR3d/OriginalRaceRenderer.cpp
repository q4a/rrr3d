#include "OriginalRaceRenderer.h"

#include "OriginalGameCar.h"
#include "OriginalGameObject.h"
#include "OriginalMainMenu.h"
#include "resource/ResourceFileSystem.h"
#include "rrr3d_fs_bloom_blur.bin.h"
#include "rrr3d_fs_bloom_extract.bin.h"
#include "rrr3d_fs_copy.bin.h"
#include "rrr3d_fs_fog_plane.bin.h"
#include "rrr3d_fs_grass_field.bin.h"
#include "rrr3d_fs_luminance_adapt.bin.h"
#include "rrr3d_fs_luminance_downsample.bin.h"
#include "rrr3d_fs_luminance_log.bin.h"
#include "rrr3d_fs_refraction.bin.h"
#include "rrr3d_fs_shadow_map.bin.h"
#include "rrr3d_fs_skybox.bin.h"
#include "rrr3d_fs_sun_shaft_composite.bin.h"
#include "rrr3d_fs_sun_shaft_prepare.bin.h"
#include "rrr3d_fs_tone_map.bin.h"
#include "rrr3d_fs_water.bin.h"
#include "rrr3d_vs_post_process.bin.h"
#include "rrr3d_vs_refraction.bin.h"
#include "rrr3d_vs_shadow_map.bin.h"
#include "rrr3d_vs_skybox.bin.h"
#include "rrr3d_vs_grass_field.bin.h"
#include "rrr3d_vs_water.bin.h"

#include <bx/math.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>

namespace rrr3d::race
{
namespace
{

using namespace r3d::renderer;

bool valid(Mesh value) noexcept
{
    return value.vertices.value != invalid_resource &&
           value.indices.value != invalid_resource;
}

bool valid(Texture value) noexcept
{
    return value.value != invalid_resource;
}

bool valid(Shader value) noexcept
{
    return value.value != invalid_resource;
}

bool valid(RenderTarget value) noexcept
{
    return value.value != invalid_resource;
}

bool valid(const CubeRenderTarget& value) noexcept
{
    return valid(value.texture) &&
           std::all_of(value.faces.begin(), value.faces.end(),
                       [](RenderTarget face) { return valid(face); });
}

std::string_view recordName(std::string_view value)
{
    const auto slash = value.find_last_of("\\/");
    return slash == std::string_view::npos ? value
                                            : value.substr(slash + 1U);
}

const r3d::game::originalrace::Vehicle& activeVehicleDefinition(
    const r3d::game::originalrace::Race& race,
    const std::vector<r3d::game::originalrace::RacerRuntime>& players,
    std::size_t racer)
{
    if (racer < players.size())
    {
        if (const auto* record = players[racer].GetCarRecord())
            return *record;
    }
    const auto& descriptor = race.racers.at(racer);
    return descriptor.hasConfiguredVehicle
               ? descriptor.configuredVehicle
               : race.vehicles.at(descriptor.vehicle);
}

const r3d::game::originalrace::ProjectileDefinition*
runtimeProjectileDefinition(
    const r3d::game::originalrace::ProjectileRuntime& projectile) noexcept
{
    return projectile.sourceObject != nullptr
               ? &projectile.sourceObject->GetDesc()
               : nullptr;
}

const r3d::game::originalrace::ProjectileDefinition*
runtimeProjectileDefinition(
    const r3d::game::originalrace::MineRuntime& mine) noexcept
{
    return mine.sourceObject != nullptr
               ? &mine.sourceObject->GetDesc()
               : nullptr;
}

const r3d::resource::R3DMeshAsset& sourceAsset(
    const OriginalRaceRenderer::Asset& asset) noexcept
{
    static const r3d::resource::R3DMeshAsset empty;
    return asset.source != nullptr ? *asset.source : empty;
}

std::vector<StaticMeshVertex> skyVertices()
{
    constexpr float n = -1.0F;
    constexpr float p = 1.0F;
    constexpr float normalZ = -1.0F;
    return {
        {p,n,n, 0,0,normalZ, 0,1}, {p,p,n, 0,0,normalZ, 1,1},
        {p,n,p, 0,0,normalZ, 0,0}, {p,p,p, 0,0,normalZ, 1,0},
        {n,p,n, 0,0,normalZ, 0,1}, {n,n,n, 0,0,normalZ, 1,1},
        {n,p,p, 0,0,normalZ, 0,0}, {n,n,p, 0,0,normalZ, 1,0},
        {n,n,n, 0,0,normalZ, 0,1}, {p,n,n, 0,0,normalZ, 1,1},
        {n,n,p, 0,0,normalZ, 0,0}, {p,n,p, 0,0,normalZ, 1,0},
        {p,p,n, 0,0,normalZ, 0,1}, {n,p,n, 0,0,normalZ, 1,1},
        {p,p,p, 0,0,normalZ, 0,0}, {n,p,p, 0,0,normalZ, 1,0},
        {n,n,p, 0,0,normalZ, 0,1}, {p,n,p, 0,0,normalZ, 1,1},
        {n,p,p, 0,0,normalZ, 0,0}, {p,p,p, 0,0,normalZ, 1,0},
        {n,p,n, 0,0,normalZ, 0,1}, {p,p,n, 0,0,normalZ, 1,1},
        {n,n,n, 0,0,normalZ, 0,0}, {p,n,n, 0,0,normalZ, 1,0},
    };
}

std::vector<std::uint32_t> skyIndices()
{
    std::vector<std::uint32_t> result;
    result.reserve(36);
    for (std::uint32_t face = 0; face < 6; ++face)
    {
        const std::uint32_t first = face * 4U;
        result.insert(result.end(),
                      {first, first + 1U, first + 2U,
                       first + 1U, first + 3U, first + 2U});
    }
    return result;
}

constexpr std::array<StaticMeshVertex, 4> effectVertices{{
    {-0.5F, -0.5F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F},
    {0.5F, -0.5F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F},
    {-0.5F, 0.5F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F},
    {0.5F, 0.5F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 0.0F},
}};

constexpr std::array<std::uint32_t, 6> effectIndices{{0, 1, 2, 1, 3, 2}};

constexpr std::array<Vertex, 4> postProcessVertices{{
    {-1.0F, -1.0F, 0.0F, 0xffffffffU, 0.0F, 1.0F},
    {1.0F, -1.0F, 0.0F, 0xffffffffU, 1.0F, 1.0F},
    {-1.0F, 1.0F, 0.0F, 0xffffffffU, 0.0F, 0.0F},
    {1.0F, 1.0F, 0.0F, 0xffffffffU, 1.0F, 0.0F},
}};

constexpr std::array<std::uint16_t, 6> postProcessIndices{
    {0, 1, 2, 1, 3, 2}};

std::array<float, 16> identityMatrix() noexcept
{
    return {1.0F, 0.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F, 0.0F,
            0.0F, 0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 0.0F, 1.0F};
}

std::array<float, 16> multiplyMatrix(
    const std::array<float, 16>& first,
    const std::array<float, 16>& second) noexcept
{
    std::array<float, 16> result{};
    for (std::size_t column = 0; column < 4; ++column)
        for (std::size_t row = 0; row < 4; ++row)
            for (std::size_t item = 0; item < 4; ++item)
                result[column * 4 + row] +=
                    first[item * 4 + row] *
                    second[column * 4 + item];
    return result;
}

std::array<float, 16> viewProjection(const Camera& camera) noexcept
{
    return multiplyMatrix(camera.projection, camera.view);
}

Camera reflectedCamera(const Camera& camera, float height) noexcept
{
    Camera result = camera;
    auto reflection = identityMatrix();
    reflection[10] = -1.0F;
    reflection[14] = 2.0F * height;
    result.view = multiplyMatrix(camera.view, reflection);
    return result;
}

Camera shadowCamera(const GraphicsDevice& device,
                    const Camera& sceneCamera,
                    const r3d::physics::Vec3& sun,
                    float sliceNear, float sliceFar,
                    float cameraFar) noexcept
{
    const auto sceneViewProjection = viewProjection(sceneCamera);
    std::array<float, 16> inverseViewProjection{};
    bx::mtxInverse(inverseViewProjection.data(),
                   sceneViewProjection.data());
    auto unproject = [&](float x, float y, float z) {
        const auto& matrix = inverseViewProjection;
        const float worldX = matrix[0] * x + matrix[4] * y +
                             matrix[8] * z + matrix[12];
        const float worldY = matrix[1] * x + matrix[5] * y +
                             matrix[9] * z + matrix[13];
        const float worldZ = matrix[2] * x + matrix[6] * y +
                             matrix[10] * z + matrix[14];
        const float worldW = matrix[3] * x + matrix[7] * y +
                             matrix[11] * z + matrix[15];
        const float inverseW =
            std::abs(worldW) > 0.000001F ? 1.0F / worldW : 1.0F;
        return r3d::physics::Vec3{
            worldX * inverseW, worldY * inverseW,
            worldZ * inverseW};
    };
    const float nearDepth =
        device.usesHomogeneousDepth() ? -1.0F : 0.0F;
    const float denominator = std::max(cameraFar - 1.0F, 0.001F);
    const float nearFrame =
        std::clamp((sliceNear - 1.0F) / denominator, 0.0F, 1.0F);
    const float farFrame =
        std::clamp((sliceFar - 1.0F) / denominator, 0.0F, 1.0F);
    std::array<r3d::physics::Vec3, 8> corners{};
    std::size_t corner = 0U;
    for (const float y : {-1.0F, 1.0F})
    {
        for (const float x : {-1.0F, 1.0F})
        {
            const auto nearPoint = unproject(x, y, nearDepth);
            const auto farPoint = unproject(x, y, 1.0F);
            auto interpolate = [&](float frame) {
                return r3d::physics::Vec3{
                    nearPoint.x + (farPoint.x - nearPoint.x) * frame,
                    nearPoint.y + (farPoint.y - nearPoint.y) * frame,
                    nearPoint.z + (farPoint.z - nearPoint.z) * frame};
            };
            corners[corner++] = interpolate(nearFrame);
            corners[corner++] = interpolate(farFrame);
        }
    }
    r3d::physics::Vec3 target{};
    for (const auto& point : corners)
    {
        target.x += point.x;
        target.y += point.y;
        target.z += point.z;
    }
    target.x /= static_cast<float>(corners.size());
    target.y /= static_cast<float>(corners.size());
    target.z /= static_cast<float>(corners.size());
    Camera camera;
    const float length =
        std::max(std::sqrt(sun.x * sun.x + sun.y * sun.y +
                           sun.z * sun.z),
                 0.001F);
    const bx::Vec3 center{target.x, target.y, target.z};
    const bx::Vec3 eye{
        target.x + sun.x / length * 100.0F,
        target.y + sun.y / length * 100.0F,
        target.z + sun.z / length * 100.0F};
    const bx::Vec3 up =
        std::abs(sun.z / length) > 0.95F
            ? bx::Vec3{0.0F, 1.0F, 0.0F}
            : bx::Vec3{0.0F, 0.0F, 1.0F};
    bx::mtxLookAt(camera.view.data(), eye, center, up,
                  bx::Handedness::Right);
    auto lightPoint = [&](const r3d::physics::Vec3& point) {
        const auto& matrix = camera.view;
        return r3d::physics::Vec3{
            matrix[0] * point.x + matrix[4] * point.y +
                matrix[8] * point.z + matrix[12],
            matrix[1] * point.x + matrix[5] * point.y +
                matrix[9] * point.z + matrix[13],
            matrix[2] * point.x + matrix[6] * point.y +
                matrix[10] * point.z + matrix[14]};
    };
    auto first = lightPoint(corners.front());
    float minimumX = first.x;
    float maximumX = first.x;
    float minimumY = first.y;
    float maximumY = first.y;
    float minimumDepth = -first.z;
    float maximumDepth = -first.z;
    for (std::size_t index = 1U; index < corners.size(); ++index)
    {
        const auto point = lightPoint(corners[index]);
        minimumX = std::min(minimumX, point.x);
        maximumX = std::max(maximumX, point.x);
        minimumY = std::min(minimumY, point.y);
        maximumY = std::max(maximumY, point.y);
        minimumDepth = std::min(minimumDepth, -point.z);
        maximumDepth = std::max(maximumDepth, -point.z);
    }
    constexpr float cropMargin = 2.0F;
    float extentX =
        std::max((maximumX - minimumX) * 0.5F + cropMargin, 1.0F);
    float extentY =
        std::max((maximumY - minimumY) * 0.5F + cropMargin, 1.0F);
    float centerX = (minimumX + maximumX) * 0.5F;
    float centerY = (minimumY + maximumY) * 0.5F;
    // Snap the crop to shadow texels so sub-pixel camera motion does not
    // shimmer the projected map.
    const float texelX = extentX * 2.0F / 2048.0F;
    const float texelY = extentY * 2.0F / 2048.0F;
    centerX = std::round(centerX / texelX) * texelX;
    centerY = std::round(centerY / texelY) * texelY;
    const float depthNear = std::max(minimumDepth - 50.0F, 0.1F);
    const float depthFar =
        std::max(maximumDepth + 50.0F, depthNear + 1.0F);
    bx::mtxOrtho(camera.projection.data(), centerX - extentX,
                 centerX + extentX, centerY - extentY,
                 centerY + extentY, depthNear, depthFar, 0.0F,
                 device.usesHomogeneousDepth(),
                 bx::Handedness::Right);
    return camera;
}

float sourceShadowSplit(bool orthographic) noexcept
{
    // ShadowMapRender::BuildViewProj uses the standard logarithmic/uniform
    // blend with lambda 0.1 for the isometric camera and 0.7 for the
    // perspective camera. CameraManager limits shadow depth to 55/60 m.
    constexpr float nearDistance = 1.0F;
    const float farDistance = orthographic ? 55.0F : 60.0F;
    const float lambda = orthographic ? 0.1F : 0.7F;
    const float logarithmic =
        nearDistance * std::sqrt(farDistance / nearDistance);
    const float uniform = nearDistance +
                          (farDistance - nearDistance) * 0.5F;
    return logarithmic * lambda + uniform * (1.0F - lambda);
}

constexpr std::array<RenderPass, 6> environmentPasses{{
    RenderPass::EnvironmentPositiveX,
    RenderPass::EnvironmentNegativeX,
    RenderPass::EnvironmentPositiveY,
    RenderPass::EnvironmentNegativeY,
    RenderPass::EnvironmentPositiveZ,
    RenderPass::EnvironmentNegativeZ,
}};

Camera environmentCamera(const GraphicsDevice& device,
                         const r3d::physics::Vec3& center,
                         std::size_t face) noexcept
{
    static constexpr std::array<r3d::physics::Vec3, 6> directions{{
        {-1.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
        {0.0F, -1.0F, 0.0F}, {0.0F, 1.0F, 0.0F},
        {0.0F, 0.0F, -1.0F}, {0.0F, 0.0F, 1.0F},
    }};
    static constexpr std::array<r3d::physics::Vec3, 6> ups{{
        {0.0F, -1.0F, 0.0F}, {0.0F, -1.0F, 0.0F},
        {0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, -1.0F},
        {0.0F, -1.0F, 0.0F}, {0.0F, -1.0F, 0.0F},
    }};
    const auto index = std::min(face, directions.size() - 1U);
    const auto& direction = directions[index];
    const auto& up = ups[index];
    const bx::Vec3 eye{center.x, center.y, center.z};
    const bx::Vec3 target{center.x + direction.x,
                          center.y + direction.y,
                          center.z + direction.z};
    Camera camera;
    bx::mtxLookAt(camera.view.data(), eye, target,
                  {up.x, up.y, up.z}, bx::Handedness::Right);
    bx::mtxProj(camera.projection.data(), 90.0F, 1.0F,
                1.0F, 100.0F, device.usesHomogeneousDepth(),
                bx::Handedness::Right);
    return camera;
}

Transform transform(const r3d::physics::Transform& source)
{
    const auto& q = source.rotation;
    const float xx = q.x * q.x;
    const float yy = q.y * q.y;
    const float zz = q.z * q.z;
    const float xy = q.x * q.y;
    const float xz = q.x * q.z;
    const float yz = q.y * q.z;
    const float wx = q.w * q.x;
    const float wy = q.w * q.y;
    const float wz = q.w * q.z;
    Transform result;
    result.matrix = {
        (1.0F - 2.0F * (yy + zz)) * source.scale.x,
        (2.0F * (xy + wz)) * source.scale.x,
        (2.0F * (xz - wy)) * source.scale.x, 0.0F,
        (2.0F * (xy - wz)) * source.scale.y,
        (1.0F - 2.0F * (xx + zz)) * source.scale.y,
        (2.0F * (yz + wx)) * source.scale.y, 0.0F,
        (2.0F * (xz + wy)) * source.scale.z,
        (2.0F * (yz - wx)) * source.scale.z,
        (1.0F - 2.0F * (xx + yy)) * source.scale.z, 0.0F,
        source.position.x, source.position.y, source.position.z, 1.0F};
    return result;
}

r3d::physics::Vec3 rotateX(const r3d::physics::Quat& q)
{
    return {1.0F - 2.0F * (q.y * q.y + q.z * q.z),
            2.0F * (q.x * q.y + q.w * q.z),
            2.0F * (q.x * q.z - q.w * q.y)};
}

r3d::physics::Quat directionRotation(r3d::physics::Vec3 direction)
{
    const float length = std::sqrt(
        direction.x * direction.x + direction.y * direction.y +
        direction.z * direction.z);
    if (length <= 0.0001F)
        return {};
    direction.x /= length;
    direction.y /= length;
    direction.z /= length;
    if (direction.x < -0.9999F)
        return {0.0F, 0.0F, 1.0F, 0.0F};
    r3d::physics::Quat result{
        0.0F, -direction.z, direction.y, 1.0F + direction.x};
    const float quaternionLength = std::sqrt(
        result.x * result.x + result.y * result.y +
        result.z * result.z + result.w * result.w);
    result.x /= quaternionLength;
    result.y /= quaternionLength;
    result.z /= quaternionLength;
    result.w /= quaternionLength;
    return result;
}

r3d::physics::Vec3 normalize(r3d::physics::Vec3 value) noexcept;
r3d::physics::Quat normalizeQuaternion(
    r3d::physics::Quat value);

r3d::physics::Quat axesRotation(
    r3d::physics::Vec3 xAxis, r3d::physics::Vec3 yAxis,
    r3d::physics::Vec3 zAxis) noexcept
{
    xAxis = normalize(xAxis);
    yAxis = normalize(yAxis);
    zAxis = normalize(zAxis);
    const float m00 = xAxis.x;
    const float m01 = yAxis.x;
    const float m02 = zAxis.x;
    const float m10 = xAxis.y;
    const float m11 = yAxis.y;
    const float m12 = zAxis.y;
    const float m20 = xAxis.z;
    const float m21 = yAxis.z;
    const float m22 = zAxis.z;
    r3d::physics::Quat result;
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
    return normalizeQuaternion(result);
}

r3d::physics::Vec3 rotate(const r3d::physics::Quat& q,
                          r3d::physics::Vec3 value)
{
    const r3d::physics::Vec3 twiceCross{
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

r3d::physics::Vec3 normalize(r3d::physics::Vec3 value) noexcept
{
    const float length = std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z);
    if (length <= 0.0001F)
        return {};
    value.x /= length;
    value.y /= length;
    value.z /= length;
    return value;
}

Camera spotShadowCamera(
    const GraphicsDevice& device,
    const r3d::game::originalrace::EnvironmentLamp& lamp) noexcept
{
    // Environment::EnableLamp creates an uncropped, single-split shadow
    // camera with the source light's local +X axis, nearDist=1 and the
    // weather-specific farDist (20 for Garage, 80/100 for Angar).
    const auto direction = normalize(rotate(
        lamp.rotation, {1.0F, 0.0F, 0.0F}));
    auto up = normalize(rotate(
        lamp.rotation, {0.0F, 0.0F, 1.0F}));
    const float alignment = std::abs(
        direction.x * up.x + direction.y * up.y +
        direction.z * up.z);
    if (alignment > 0.99F)
        up = {0.0F, 1.0F, 0.0F};
    const bx::Vec3 eye{
        lamp.position.x, lamp.position.y, lamp.position.z};
    const bx::Vec3 target{
        lamp.position.x + direction.x,
        lamp.position.y + direction.y,
        lamp.position.z + direction.z};
    Camera camera;
    bx::mtxLookAt(camera.view.data(), eye, target,
                  {up.x, up.y, up.z}, bx::Handedness::Right);
    // LightSource defaults to phi=pi/2, i.e. a 90-degree outer cone.
    bx::mtxProj(camera.projection.data(), 90.0F, 1.0F, 1.0F,
                std::max(lamp.range, 1.001F),
                device.usesHomogeneousDepth(),
                bx::Handedness::Right);
    return camera;
}

r3d::physics::Vec3 cross(const r3d::physics::Vec3& first,
                         const r3d::physics::Vec3& second) noexcept
{
    return {
        first.y * second.z - first.z * second.y,
        first.z * second.x - first.x * second.z,
        first.x * second.y - first.y * second.x};
}

bool sourceCameraMathValid() noexcept
{
    const auto xAxis = normalize({0.80F, 0.20F, 0.40F});
    const auto yAxis = normalize(cross(
        {0.0F, 0.0F, 1.0F}, xAxis));
    const auto zAxis = normalize(cross(xAxis, yAxis));
    const auto rotation = axesRotation(xAxis, yAxis, zAxis);
    const auto rotatedX = rotate(
        rotation, {1.0F, 0.0F, 0.0F});
    const auto rotatedY = rotate(
        rotation, {0.0F, 1.0F, 0.0F});
    const auto rotatedZ = rotate(
        rotation, {0.0F, 0.0F, 1.0F});
    const auto close = [](const r3d::physics::Vec3& first,
                          const r3d::physics::Vec3& second) {
        const float dx = first.x - second.x;
        const float dy = first.y - second.y;
        const float dz = first.z - second.z;
        return dx * dx + dy * dy + dz * dz < 0.00001F;
    };
    return close(rotatedX, xAxis) && close(rotatedY, yAxis) &&
           close(rotatedZ, zAxis);
}

Transform billboardTransform(
    const r3d::physics::Vec3& position,
    const r3d::physics::Vec3& scale,
    const r3d::physics::Vec3& cameraPosition,
    float turnAngle,
    const r3d::physics::Vec3* fixedDirection = nullptr) noexcept
{
    r3d::physics::Vec3 xAxis;
    r3d::physics::Vec3 yAxis;
    r3d::physics::Vec3 zAxis;
    if (fixedDirection != nullptr)
    {
        xAxis = normalize(*fixedDirection);
        const auto view = normalize(
            {position.x - cameraPosition.x,
             position.y - cameraPosition.y,
             position.z - cameraPosition.z});
        yAxis = normalize(cross(xAxis, view));
        zAxis = normalize(cross(xAxis, yAxis));
    }
    if (fixedDirection == nullptr ||
        (xAxis.x == 0.0F && xAxis.y == 0.0F && xAxis.z == 0.0F) ||
        (yAxis.x == 0.0F && yAxis.y == 0.0F && yAxis.z == 0.0F))
    {
        zAxis = normalize(
            {cameraPosition.x - position.x,
             cameraPosition.y - position.y,
             cameraPosition.z - position.z});
        auto worldUp = r3d::physics::Vec3{0.0F, 0.0F, 1.0F};
        if (std::abs(zAxis.z) > 0.98F)
            worldUp = {0.0F, 1.0F, 0.0F};
        const auto right = normalize(cross(worldUp, zAxis));
        const auto up = normalize(cross(zAxis, right));
        const float cosine = std::cos(turnAngle);
        const float sine = std::sin(turnAngle);
        xAxis = {
            right.x * cosine + up.x * sine,
            right.y * cosine + up.y * sine,
            right.z * cosine + up.z * sine};
        yAxis = {
            up.x * cosine - right.x * sine,
            up.y * cosine - right.y * sine,
            up.z * cosine - right.z * sine};
    }
    Transform result;
    result.matrix = {
        xAxis.x * scale.x, xAxis.y * scale.x, xAxis.z * scale.x, 0.0F,
        yAxis.x * scale.y, yAxis.y * scale.y, yAxis.z * scale.y, 0.0F,
        zAxis.x * scale.z, zAxis.y * scale.z, zAxis.z * scale.z, 0.0F,
        position.x, position.y, position.z, 1.0F};
    return result;
}

r3d::physics::Quat multiply(const r3d::physics::Quat& first,
                            const r3d::physics::Quat& second)
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

r3d::physics::Quat normalizeQuaternion(r3d::physics::Quat value)
{
    const float length = std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z +
        value.w * value.w);
    if (length <= 0.0001F)
        return {};
    value.x /= length;
    value.y /= length;
    value.z /= length;
    value.w /= length;
    return value;
}

r3d::physics::Quat sphericalMix(
    r3d::physics::Quat first, r3d::physics::Quat second,
    float amount)
{
    amount = std::clamp(amount, 0.0F, 1.0F);
    first = normalizeQuaternion(first);
    second = normalizeQuaternion(second);
    const float cosine = std::clamp(
        first.x * second.x + first.y * second.y +
            first.z * second.z + first.w * second.w,
        -1.0F, 1.0F);
    const float angle = std::acos(cosine);
    const float sine = std::sin(angle);
    if (std::abs(sine) <= 0.0001F)
    {
        return normalizeQuaternion({
            first.x + (second.x - first.x) * amount,
            first.y + (second.y - first.y) * amount,
            first.z + (second.z - first.z) * amount,
            first.w + (second.w - first.w) * amount});
    }
    const float firstWeight = std::sin((1.0F - amount) * angle) / sine;
    const float secondWeight = std::sin(amount * angle) / sine;
    return {first.x * firstWeight + second.x * secondWeight,
            first.y * firstWeight + second.y * secondWeight,
            first.z * firstWeight + second.z * secondWeight,
            first.w * firstWeight + second.w * secondWeight};
}

float quaternionAngle(r3d::physics::Quat value)
{
    value = normalizeQuaternion(value);
    return 2.0F * std::acos(std::clamp(value.w, -1.0F, 1.0F));
}

r3d::physics::Vec3 quaternionAxis(r3d::physics::Quat value)
{
    value = normalizeQuaternion(value);
    const float sine = std::sqrt(
        std::max(1.0F - value.w * value.w, 0.0F));
    if (sine <= 0.0001F)
        return {0.0F, 0.0F, 1.0F};
    return {value.x / sine, value.y / sine, value.z / sine};
}

r3d::physics::Quat angleAxis(float angle,
                             const r3d::physics::Vec3& axis)
{
    const float halfAngle = angle * 0.5F;
    const float sine = std::sin(halfAngle);
    return {axis.x * sine, axis.y * sine, axis.z * sine,
            std::cos(halfAngle)};
}

r3d::physics::Quat integrateRotation(
    const r3d::physics::Quat& angularVelocity, float seconds)
{
    const auto axis = quaternionAxis(angularVelocity);
    return angleAxis(quaternionAngle(angularVelocity) * seconds, axis);
}

r3d::physics::Transform compose(
    const r3d::physics::Transform& parent,
    const r3d::physics::Transform& local)
{
    r3d::physics::Transform result;
    const auto localPosition = rotate(
        parent.rotation,
        {local.position.x * parent.scale.x,
         local.position.y * parent.scale.y,
         local.position.z * parent.scale.z});
    result.position = {parent.position.x + localPosition.x,
                       parent.position.y + localPosition.y,
                       parent.position.z + localPosition.z};
    result.scale = {parent.scale.x * local.scale.x,
                    parent.scale.y * local.scale.y,
                    parent.scale.z * local.scale.z};
    result.rotation = multiply(parent.rotation, local.rotation);
    return result;
}

r3d::physics::Transform worldCoordinateParticle(
    const r3d::physics::Transform& emitter,
    const r3d::physics::Transform& particle)
{
    auto result = compose(emitter, particle);
    // FxEmitter::UpdateParticle transforms only startPos through
    // LocalToWorldCoord. Rotation and scale remain particle properties and
    // FxNode/FxPlane managers render them against IdentityMatrix.
    result.rotation = particle.rotation;
    result.scale = particle.scale;
    return result;
}

r3d::physics::Vec3 transformNormal(
    const r3d::physics::Transform& transform,
    const r3d::physics::Vec3& value)
{
    // BaseSceneNode::LocalToWorldNorm uses Vec3TransformNormal with the
    // complete world matrix, including its scale but excluding translation.
    return rotate(
        transform.rotation,
        {value.x * transform.scale.x,
         value.y * transform.scale.y,
         value.z * transform.scale.z});
}

struct WorldBounds
{
    r3d::physics::Vec3 minimum{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()};
    r3d::physics::Vec3 maximum{
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()};
    bool valid = false;
};

r3d::physics::Vec3 transformPoint(
    const Transform& value, const r3d::physics::Vec3& point) noexcept
{
    const auto& matrix = value.matrix;
    return {
        matrix[0] * point.x + matrix[4] * point.y +
            matrix[8] * point.z + matrix[12],
        matrix[1] * point.x + matrix[5] * point.y +
            matrix[9] * point.z + matrix[13],
        matrix[2] * point.x + matrix[6] * point.y +
            matrix[10] * point.z + matrix[14]};
}

void include(WorldBounds& bounds,
             const r3d::physics::Vec3& point) noexcept
{
    bounds.minimum.x = std::min(bounds.minimum.x, point.x);
    bounds.minimum.y = std::min(bounds.minimum.y, point.y);
    bounds.minimum.z = std::min(bounds.minimum.z, point.z);
    bounds.maximum.x = std::max(bounds.maximum.x, point.x);
    bounds.maximum.y = std::max(bounds.maximum.y, point.y);
    bounds.maximum.z = std::max(bounds.maximum.z, point.z);
    bounds.valid = true;
}

void include(WorldBounds& bounds, const WorldBounds& other) noexcept
{
    if (!other.valid)
        return;
    include(bounds, other.minimum);
    include(bounds, other.maximum);
}

WorldBounds objectBounds(
    const OriginalRaceRenderer::ObjectAsset& asset,
    const std::vector<r3d::game::originalrace::VisualNode>& nodes,
    const r3d::physics::Transform& parent) noexcept
{
    WorldBounds result;
    const std::size_t count = std::min(asset.nodes.size(), nodes.size());
    for (std::size_t index = 0; index < count; ++index)
    {
        const auto model = transform(compose(parent, nodes[index].transform));
        const auto& source = sourceAsset(asset.nodes[index]);
        const auto& minimum = source.minimum;
        const auto& maximum = source.maximum;
        for (unsigned corner = 0; corner < 8U; ++corner)
        {
            include(
                result,
                transformPoint(
                    model,
                    {(corner & 1U) != 0U ? maximum[0] : minimum[0],
                     (corner & 2U) != 0U ? maximum[1] : minimum[1],
                     (corner & 4U) != 0U ? maximum[2] : minimum[2]}));
        }
    }
    return result;
}

bool boundsVisible(
    const WorldBounds& bounds,
    const std::array<float, 16>& viewProjectionMatrix,
    bool homogeneousDepth) noexcept
{
    if (!bounds.valid)
        return true;
    const auto& m = viewProjectionMatrix;
    // ActorManager::Culling tests an actor AABB against the active scene
    // frustum before OctreeRender submits it. The portable loader flattens
    // source child actors (most visibly wheels and destruction-list pieces)
    // into their parent object. Expand that computed parent AABB
    // conservatively so the flattened children are not clipped as the parent
    // crosses a camera edge.
    constexpr float childActorPadding = 2.0F;
    auto outside = [&](float a, float b, float c, float d) {
        const float x = a >= 0.0F
                            ? bounds.maximum.x + childActorPadding
                            : bounds.minimum.x - childActorPadding;
        const float y = b >= 0.0F
                            ? bounds.maximum.y + childActorPadding
                            : bounds.minimum.y - childActorPadding;
        const float z = c >= 0.0F
                            ? bounds.maximum.z + childActorPadding
                            : bounds.minimum.z - childActorPadding;
        return a * x + b * y + c * z + d < -0.01F;
    };
    const auto planeOutside = [&](std::size_t row, float sign) {
        return outside(
            m[3] + sign * m[row],
            m[7] + sign * m[4U + row],
            m[11] + sign * m[8U + row],
            m[15] + sign * m[12U + row]);
    };
    if (planeOutside(0U, 1.0F) || planeOutside(0U, -1.0F) ||
        planeOutside(1U, 1.0F) || planeOutside(1U, -1.0F))
    {
        return false;
    }
    if (homogeneousDepth)
    {
        if (planeOutside(2U, 1.0F))
            return false;
    }
    else if (outside(m[2], m[6], m[10], m[14]))
    {
        return false;
    }
    return !planeOutside(2U, -1.0F);
}

struct GrassFieldGeometry
{
    std::vector<StaticMeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<r3d::physics::Vec3> fieldOffsets;
};

GrassFieldGeometry sourceGrassField(float width, float height)
{
    GrassFieldGeometry result;
    width = std::max(width, 1.0F);
    height = std::max(height, 1.0F);

    // GrassField::Rebuild uses this multiplication (rather than a struct
    // size) to keep one reusable field mesh below cMaxBufSize.
    constexpr float maximumBufferBytes = 4194304.0F;
    constexpr float sourceGrassBytes =
        static_cast<float>(sizeof(float) * 4U * sizeof(float) * 2U);
    constexpr float density = 1.0F;
    const float aspect = width / height;
    const float grassCount = width * height * density;
    const float fieldCapacity = maximumBufferBytes / sourceGrassBytes;
    const float fieldCount = grassCount / fieldCapacity;
    const int fieldCountY = std::max(
        static_cast<int>(std::ceil(std::sqrt(fieldCount / aspect))), 1);
    const int fieldCountX = std::max(
        static_cast<int>(std::ceil(
            static_cast<float>(fieldCountY) * aspect)),
        1);
    const float fieldWidth = width / static_cast<float>(fieldCountX);
    const float fieldHeight = height / static_cast<float>(fieldCountY);
    for (int x = 0; x < fieldCountX; ++x)
    {
        for (int y = 0; y < fieldCountY; ++y)
        {
            result.fieldOffsets.push_back({
                (static_cast<float>(x) + 0.5F) * fieldWidth - width * 0.5F,
                (static_cast<float>(y) + 0.5F) * fieldHeight - height * 0.5F,
                0.0F});
        }
    }

    const int countX = std::max(static_cast<int>(fieldWidth * density), 1);
    const int countY = std::max(static_cast<int>(fieldHeight * density), 1);
    const std::size_t maximumSprites =
        static_cast<std::size_t>(countX) * static_cast<std::size_t>(countY);
    std::vector<r3d::physics::Vec3> positions(maximumSprites);
    const r3d::physics::Vec3 step{
        fieldWidth / static_cast<float>(countX),
        fieldHeight / static_cast<float>(countY), 0.0F};
    std::size_t positionIndex = 0U;
    for (int x = -static_cast<int>(std::floor(countX / 2.0F));
         x < static_cast<int>(std::ceil(countX / 2.0F)) - 1; ++x)
    {
        for (int y = -static_cast<int>(std::floor(countY / 2.0F));
             y < static_cast<int>(std::ceil(countY / 2.0F)) - 1; ++y)
        {
            if (positionIndex >= positions.size())
                break;
            positions[positionIndex++] = {
                step.x * 0.5F + static_cast<float>(x) * step.x,
                step.y * 0.5F + static_cast<float>(y) * step.y, 0.0F};
        }
    }

    // The Windows field is intentionally random on every process start.
    // Use the same shuffle/random distribution with a fixed renderer-local
    // sequence so a captured macOS frame remains reproducible.
    std::uint32_t randomState = 0x4d6f746fU;
    auto randomUnit = [&]() {
        randomState = randomState * 214013U + 2531011U;
        return static_cast<float>((randomState >> 16U) & 0x7fffU) /
               32767.0F;
    };
    for (std::size_t index = positions.size(); index > 1U; --index)
    {
        const std::size_t selected = std::min(
            static_cast<std::size_t>(randomUnit() * index), index - 1U);
        std::swap(positions[index - 1U], positions[selected]);
    }

    struct Tile
    {
        float weight;
        std::array<float, 4> uv;
    };
    constexpr std::array<Tile, 4> tiles{{
        {2.0F, {0.0F, 0.5F, 0.5F, 0.0F}},
        {1.0F, {0.5F, 0.5F, 1.0F, 0.0F}},
        {10.0F, {0.0F, 1.0F, 0.5F, 0.5F}},
        {1.0F, {0.5F, 1.0F, 1.0F, 0.5F}},
    }};
    constexpr std::array<std::array<float, 2>, 4> corners{{
        {-1.0F, -1.0F}, {1.0F, -1.0F},
        {1.0F, 1.0F}, {-1.0F, 1.0F},
    }};
    constexpr std::array<std::uint32_t, 6> cornerIndices{
        0U, 1U, 2U, 0U, 2U, 3U};
    constexpr float weightSum = 14.0F;
    const float spritesPerWeight =
        static_cast<float>(maximumSprites) / weightSum;
    std::size_t spriteOffset = 0U;
    for (const auto& tile : tiles)
    {
        std::size_t tileSprites = static_cast<std::size_t>(
            std::floor(tile.weight * spritesPerWeight + 0.5F));
        tileSprites = std::min(tileSprites, maximumSprites - spriteOffset);
        for (std::size_t item = 0; item < tileSprites; ++item)
        {
            auto center = positions[spriteOffset + item];
            center.x += 2.0F * randomUnit();
            center.y += 2.0F * randomUnit();
            const std::uint32_t first =
                static_cast<std::uint32_t>(result.vertices.size());
            for (std::size_t corner = 0; corner < corners.size(); ++corner)
            {
                const float u = (corner == 0U || corner == 3U)
                                    ? tile.uv[0]
                                    : tile.uv[2];
                const float v = corner < 2U ? tile.uv[1] : tile.uv[3];
                result.vertices.push_back({
                    center.x, center.y, center.z,
                    corners[corner][0], corners[corner][1], 0.0F,
                    u, v});
            }
            for (const auto corner : cornerIndices)
                result.indices.push_back(first + corner);
        }
        spriteOffset += tileSprites;
    }
    return result;
}

bool lineIntersectsBoundsBeforeTarget(
    const WorldBounds& bounds, const r3d::physics::Vec3& start,
    const r3d::physics::Vec3& target, float targetSize) noexcept
{
    if (!bounds.valid)
        return false;
    auto direction = r3d::physics::Vec3{
        target.x - start.x, target.y - start.y, target.z - start.z};
    const float length = std::sqrt(
        direction.x * direction.x + direction.y * direction.y +
        direction.z * direction.z);
    if (length <= 0.0001F)
        return false;
    direction.x /= length;
    direction.y /= length;
    direction.z /= length;
    float nearDistance = 0.0F;
    float farDistance = length;
    for (unsigned axis = 0; axis < 3U; ++axis)
    {
        const float origin =
            axis == 0U ? start.x : (axis == 1U ? start.y : start.z);
        const float ray =
            axis == 0U ? direction.x
                       : (axis == 1U ? direction.y : direction.z);
        const float minimum =
            axis == 0U
                ? bounds.minimum.x
                : (axis == 1U ? bounds.minimum.y : bounds.minimum.z);
        const float maximum =
            axis == 0U
                ? bounds.maximum.x
                : (axis == 1U ? bounds.maximum.y : bounds.maximum.z);
        if (std::abs(ray) <= 0.000001F)
        {
            if (origin < minimum || origin > maximum)
                return false;
            continue;
        }
        float first = (minimum - origin) / ray;
        float second = (maximum - origin) / ray;
        if (first > second)
            std::swap(first, second);
        nearDistance = std::max(nearDistance, first);
        farDistance = std::min(farDistance, second);
        if (nearDistance > farDistance)
            return false;
    }
    // ActorManager::PullInRayTargetGroup keeps actors containing/behind the
    // target opaque; only geometry safely in front of the car fades.
    return farDistance >= 0.0F &&
           length - farDistance > targetSize * 1.5F;
}

float cullOpacity(float time) noexcept
{
    constexpr float duration = 0.25F;
    constexpr float minimumOpacity = 0.3F;
    return 1.0F -
           std::clamp(time / duration, 0.0F, 1.0F) *
               (1.0F - minimumOpacity);
}

std::array<float, 4> materialTextureTransform(
    const r3d::game::originalrace::MaterialDefinition& material,
    float frame)
{
    const auto columns =
        std::max<std::uint16_t>(material.atlasColumns, 1);
    const auto rows = std::max<std::uint16_t>(material.atlasRows, 1);
    const std::uint32_t count =
        static_cast<std::uint32_t>(columns) * rows;
    const float normalized = std::clamp(frame, 0.0F, 1.0F);
    const std::uint32_t index =
        normalized >= 1.0F
            ? count - 1U
            : static_cast<std::uint32_t>(normalized * count);
    const float scaleX =
        (material.textureCoordinateMaximum.x -
         material.textureCoordinateMinimum.x -
         material.textureCoordinateInset.x * 2.0F) /
        static_cast<float>(columns);
    const float scaleY =
        (material.textureCoordinateMaximum.y -
         material.textureCoordinateMinimum.y -
         material.textureCoordinateInset.y * 2.0F) /
        static_cast<float>(rows);
    std::array<float, 4> result{
        scaleX, scaleY,
        material.textureCoordinateMinimum.x +
            material.textureCoordinateInset.x +
            static_cast<float>(index % columns) * scaleX,
        material.textureCoordinateMinimum.y +
            material.textureCoordinateInset.y +
            static_cast<float>(index / columns) * scaleY};
    result[2] +=
        material.textureOffsetMinimum.x +
        (material.textureOffsetMaximum.x -
         material.textureOffsetMinimum.x) * normalized;
    result[3] +=
        material.textureOffsetMinimum.y +
        (material.textureOffsetMaximum.y -
         material.textureOffsetMinimum.y) * normalized;
    return result;
}

float sourceAnimationFrame(
    r3d::game::originalrace::VisualNode::AnimationMode mode,
    float duration, float storedFrame, float time) noexcept
{
    using Mode =
        r3d::game::originalrace::VisualNode::AnimationMode;
    if (mode == Mode::None || mode == Mode::Manual ||
        mode == Mode::Inheritance)
    {
        return storedFrame;
    }
    const float normalized =
        std::max(time, 0.0F) / std::max(duration, 0.0001F);
    switch (mode)
    {
    case Mode::Once:
        return std::clamp(normalized, 0.0F, 1.0F);
    case Mode::Repeat:
        return normalized - std::floor(normalized);
    case Mode::Tile:
        return normalized;
    case Mode::TwoSide:
    {
        const float cycle = normalized - std::floor(normalized);
        return (cycle > 0.5F ? 1.0F - cycle : cycle) * 2.0F;
    }
    case Mode::None:
    case Mode::Manual:
    case Mode::Inheritance:
        break;
    }
    return storedFrame;
}

float visualAnimationFrame(
    const r3d::game::originalrace::VisualNode& node,
    float seconds) noexcept
{
    return sourceAnimationFrame(
        node.animationMode, node.animationDuration,
        node.animationFrame, seconds);
}

r3d::physics::Transform sourceAnimatedNodeTransform(
    r3d::physics::Transform result,
    r3d::game::originalrace::VisualNode::AnimationMode mode,
    const r3d::physics::Vec3& speedPosition,
    const r3d::physics::Vec3& speedScale,
    r3d::physics::Quat speedRotation, float seconds) noexcept
{
    using Mode =
        r3d::game::originalrace::VisualNode::AnimationMode;
    // BaseSceneNode::OnProgress gates all three transform velocities on
    // animMode != amNone.  Once only clamps the graph frame; it does not
    // stop the transform velocity before the owning object dies.
    if (mode == Mode::None)
        return result;
    seconds = std::max(seconds, 0.0F);
    result.position.x += speedPosition.x * seconds;
    result.position.y += speedPosition.y * seconds;
    result.position.z += speedPosition.z * seconds;
    result.scale.x += speedScale.x * seconds;
    result.scale.y += speedScale.y * seconds;
    result.scale.z += speedScale.z * seconds;

    const float quaternionLength = std::sqrt(
        speedRotation.x * speedRotation.x +
        speedRotation.y * speedRotation.y +
        speedRotation.z * speedRotation.z +
        speedRotation.w * speedRotation.w);
    if (quaternionLength <= 0.0001F)
        return result;
    speedRotation.x /= quaternionLength;
    speedRotation.y /= quaternionLength;
    speedRotation.z /= quaternionLength;
    speedRotation.w /= quaternionLength;
    const float axisLength = std::sqrt(
        speedRotation.x * speedRotation.x +
        speedRotation.y * speedRotation.y +
        speedRotation.z * speedRotation.z);
    if (axisLength <= 0.0001F)
        return result;
    const float angle =
        2.0F * std::acos(std::clamp(speedRotation.w, -1.0F, 1.0F));
    const float halfAngle = angle * seconds * 0.5F;
    const float sine = std::sin(halfAngle);
    const r3d::physics::Quat delta{
        speedRotation.x / axisLength * sine,
        speedRotation.y / axisLength * sine,
        speedRotation.z / axisLength * sine,
        std::cos(halfAngle)};
    result.rotation = multiply(delta, result.rotation);
    return result;
}

r3d::physics::Transform sourceAnimatedNodeTransform(
    const r3d::game::originalrace::VisualNode& node,
    float seconds) noexcept
{
    return sourceAnimatedNodeTransform(
        node.transform, node.animationMode, node.speedPosition,
        node.speedScale, node.speedRotation, seconds);
}

r3d::physics::Transform sourceAnimatedNodeTransform(
    const r3d::game::originalrace::ParticleEmitterDefinition& emitter,
    float seconds) noexcept
{
    return sourceAnimatedNodeTransform(
        emitter.transform, emitter.animationMode,
        emitter.nodeSpeedPosition, emitter.nodeSpeedScale,
        emitter.nodeSpeedRotation, seconds);
}

std::array<float, 4> materialColor(
    const r3d::game::originalrace::MaterialDefinition& material,
    float frame) noexcept
{
    std::array<float, 4> result{};
    for (std::size_t component = 0; component < result.size(); ++component)
    {
        result[component] =
            material.color[component] +
            (material.colorMaximum[component] -
             material.color[component]) * frame;
    }
    result[3] =
        material.alphaMinimum +
        (material.alphaMaximum - material.alphaMinimum) * frame;
    return result;
}

enum class DrawLayer
{
    All,
    Opaque,
    Transparency,
};

bool isBlended(
    r3d::game::originalrace::MaterialBlend blend) noexcept
{
    return blend ==
               r3d::game::originalrace::MaterialBlend::Transparency ||
           blend ==
               r3d::game::originalrace::MaterialBlend::Additive;
}

PipelineState nodePipeline(
    const PipelineState& source,
    const r3d::game::originalrace::VisualNode* node)
{
    auto result = source;
    if (node == nullptr)
        return result;
    using CullMode =
        r3d::game::originalrace::VisualNode::CullMode;
    switch (node->cullMode)
    {
    case CullMode::Clockwise:
        result.faceCulling =
            PipelineState::FaceCulling::Clockwise;
        break;
    case CullMode::CounterClockwise:
        result.faceCulling =
            PipelineState::FaceCulling::CounterClockwise;
        break;
    case CullMode::None:
        result.faceCulling = PipelineState::FaceCulling::None;
        break;
    case CullMode::Inherit:
        break;
    }
    if (node->invertCullFace)
    {
        if (result.faceCulling ==
            PipelineState::FaceCulling::Clockwise)
        {
            result.faceCulling =
                PipelineState::FaceCulling::CounterClockwise;
        }
        else if (result.faceCulling ==
                 PipelineState::FaceCulling::CounterClockwise)
        {
            result.faceCulling =
                PipelineState::FaceCulling::Clockwise;
        }
    }
    return result;
}

r3d::game::originalrace::LightingMode lightingForQuality(
    r3d::game::originalrace::LightingMode lighting,
    std::uint32_t quality) noexcept
{
    using Lighting = r3d::game::originalrace::LightingMode;
    quality = std::min(quality, 2U);
    if (quality == 0U)
    {
        if (lighting == Lighting::Pixel ||
            lighting == Lighting::Reflection ||
            lighting == Lighting::Bump ||
            lighting == Lighting::PlanarReflection)
            return Lighting::Standard;
    }
    else if (quality == 1U)
    {
        if (lighting == Lighting::Bump ||
            lighting == Lighting::PlanarReflection)
            return Lighting::Pixel;
    }
    return lighting;
}

float sourceTextureDiffuseScale(
    const OriginalRaceRenderer::ObjectAsset& asset,
    const r3d::physics::Transform& actor,
    const std::array<float, 4>& lightDirection) noexcept
{
    const auto& adjustment = asset.graphVector3;
    if (lightDirection[3] < 0.5F ||
        std::abs(adjustment.y) + std::abs(adjustment.z) <= 0.0F)
        return 1.0F;
    const auto& localNormal = asset.graphVector1;
    if (localNormal.z >= 0.99F)
        return 1.0F;
    const auto worldNormal = normalize(rotate(actor.rotation, localNormal));
    const auto worldRight =
        normalize(rotate(actor.rotation, {0.0F, 1.0F, 0.0F}));
    const r3d::physics::Vec3 binormal{
        worldRight.y, -worldRight.x, 0.0F};
    const float normalSide =
        binormal.x * worldNormal.x +
        binormal.y * worldNormal.y +
        binormal.z * worldNormal.z;
    const float lightSide =
        binormal.x * lightDirection[0] +
        binormal.y * lightDirection[1] +
        binormal.z * lightDirection[2];
    const bool side = (normalSide > 0.0F) == (lightSide > 0.0F);
    return 1.0F + (side ? adjustment.z : adjustment.y);
}

void drawGroups(GraphicsDevice& device,
                const OriginalRaceRenderer::Asset& asset, Shader shader,
                const Transform& model, const PipelineState& pipeline,
                float elapsedSeconds = 0.0F,
                float reflectionStrength = 0.0F,
                r3d::game::originalrace::LightingMode lighting =
                    r3d::game::originalrace::LightingMode::Standard,
                DrawLayer layer = DrawLayer::All,
                const r3d::game::originalrace::VisualNode* node = nullptr,
                float opacity = 1.0F,
                const std::array<float, 4>* tint = nullptr,
                float textureOffsetX = 0.0F,
                float textureDiffuseScale = 1.0F,
                float textureScaleX = 1.0F)
{
    if (asset.textures.empty())
        return;
    const auto geometryPipeline = nodePipeline(pipeline, node);
    auto materialState =
        [&asset, elapsedSeconds, reflectionStrength, lighting, node,
         opacity, tint, textureOffsetX, textureDiffuseScale,
         textureScaleX](
            const auto& material, std::size_t materialIndex) {
            MaterialState state;
            const float frame =
                node != nullptr
                    ? visualAnimationFrame(*node, elapsedSeconds)
                    : 0.0F;
            state.color = materialColor(material, frame);
            if (tint != nullptr &&
                lighting ==
                    r3d::game::originalrace::LightingMode::Reflection)
            {
                // Player::SetColor writes D3DTSS_CONSTANT. reflMapp.fx uses
                // that value only to fill transparent body-texture texels;
                // it does not multiply the complete car material.
                state.mappingColor[0] = (*tint)[0];
                state.mappingColor[1] = (*tint)[1];
                state.mappingColor[2] = (*tint)[2];
                state.color[3] *= (*tint)[3];
            }
            else if (tint != nullptr)
            {
                state.color[0] *= (*tint)[0];
                state.color[1] *= (*tint)[1];
                state.color[2] *= (*tint)[2];
                state.color[3] *= (*tint)[3];
            }
            state.color[3] *= opacity;
            state.mappingColor[3] = textureDiffuseScale;
            state.alphaReference = material.alphaReference;
            state.emissive = material.emissive;
            state.specular = material.specular;
            state.shininess = material.shininess;
            state.ignoreFog = material.ignoreFog;
            state.reflectionStrength = reflectionStrength;
            state.postParameters[3] =
                static_cast<float>(lighting);
            if (lighting ==
                r3d::game::originalrace::LightingMode::Refraction)
            {
                // RefrShader::DoBeginDraw supplies (1 - Context::frame).
                // The frame belongs to refr1's amOnce sprite, not to the
                // flattened parent death effect.
                state.postParameters[0] = 1.0F - frame;
                state.textureFilter =
                    MaterialState::TextureFilter::Linear;
                state.reflectionTextureFilter =
                    MaterialState::TextureFilter::Linear;
                state.textureAddress =
                    MaterialState::TextureAddress::Wrap;
                state.reflectionTextureAddress =
                    MaterialState::TextureAddress::Mirror;
            }
            else
            {
                state.postParameters[0] =
                    material.reflectionTextureCoordinates ? 1.0F : 0.0F;
            }
            if (lighting == r3d::game::originalrace::
                                LightingMode::PlanarReflection)
            {
                state.reflectionTextureFilter =
                    MaterialState::TextureFilter::Linear;
                state.reflectionTextureAddress =
                    MaterialState::TextureAddress::Mirror;
            }
            state.receivesShadow =
                !isBlended(material.blend) &&
                material.emissive < 0.999F;
            if (materialIndex < asset.normalTextures.size())
                state.normalTexture =
                    asset.normalTextures[materialIndex];
            state.textureTransform =
                materialTextureTransform(material, frame);
            state.textureTransform[0] *= textureScaleX;
            state.textureTransform[2] += textureOffsetX;
            return state;
        };
    auto materialPipeline = [&](std::size_t index) {
        auto result = geometryPipeline;
        if (asset.materials.empty())
        {
            if (opacity < 0.999F)
            {
                result.blendMode = PipelineState::BlendMode::Alpha;
                result.writeDepth = false;
            }
            return result;
        }
        const auto& material =
            asset.materials[std::min(
                index, asset.materials.size() - 1U)];
        const auto blend = material.blend;
        result.writeDepth = result.writeDepth && material.writeDepth;
        if (isBlended(blend))
        {
            result.blendMode =
                blend ==
                        r3d::game::originalrace::MaterialBlend::Additive
                    ? PipelineState::BlendMode::Additive
                    : PipelineState::BlendMode::Alpha;
            result.writeDepth = false;
            result.faceCulling = PipelineState::FaceCulling::None;
        }
        if (opacity < 0.999F)
        {
            // Material::Apply enables source-alpha blending and
            // GraphManager disables Z writes while RenderRayUsers submits
            // gpCullOpacity actors.
            result.blendMode = PipelineState::BlendMode::Alpha;
            result.writeDepth = false;
        }
        if (lighting ==
            r3d::game::originalrace::LightingMode::Refraction)
        {
            result.blendMode = PipelineState::BlendMode::Opaque;
            result.alphaBlend = false;
            result.writeDepth = false;
            result.depthTest = true;
            result.faceCulling = PipelineState::FaceCulling::None;
        }
        return result;
    };
    auto includeMaterial = [&](std::size_t index) {
        const bool transparent =
            !asset.materials.empty() &&
            isBlended(
                asset.materials[std::min(
                    index, asset.materials.size() - 1U)].blend);
        return layer == DrawLayer::All ||
               (layer == DrawLayer::Transparency && transparent) ||
               (layer == DrawLayer::Opaque && !transparent);
    };
    const auto& source = sourceAsset(asset);
    if (source.materialGroups.empty())
    {
        if (!includeMaterial(0U))
            return;
        const auto material =
            asset.materials.empty()
                ? MaterialState{}
                : materialState(asset.materials.front(), 0U);
        device.draw(asset.mesh, shader, asset.textures.front(), model,
                    materialPipeline(0U), {}, material);
        return;
    }
    if (asset.subMesh >= 0 &&
        static_cast<std::size_t>(asset.subMesh) <
            source.materialGroups.size())
    {
        if (!includeMaterial(0U))
            return;
        const auto& group =
            source.materialGroups[static_cast<std::size_t>(
                asset.subMesh)];
        const auto material =
            asset.materials.empty()
                ? MaterialState{}
                : materialState(asset.materials.front(), 0U);
        device.draw(asset.mesh, shader, asset.textures.front(), model,
                    materialPipeline(0U),
                    {group.firstIndex, group.indexCount},
                    material);
        return;
    }
    for (std::size_t index = 0; index < source.materialGroups.size();
         ++index)
    {
        const auto& group = source.materialGroups[index];
        const std::size_t materialIndex =
            std::min(index, asset.materials.size() - 1U);
        if (!includeMaterial(materialIndex))
            continue;
        const std::size_t textureIndex =
            std::min(index, asset.textures.size() - 1U);
        const auto groupPipeline = materialPipeline(materialIndex);
        const auto material =
            materialState(asset.materials[materialIndex],
                          materialIndex);
        device.draw(asset.mesh, shader, asset.textures[textureIndex], model,
                    groupPipeline,
                    {group.firstIndex, group.indexCount}, material);
    }
}

r3d::physics::Vec3 meshGroupCenter(
    const OriginalRaceRenderer::Asset& asset) noexcept
{
    const auto& source = sourceAsset(asset);
    if (asset.subMesh < 0 ||
        static_cast<std::size_t>(asset.subMesh) >=
            source.materialGroups.size())
    {
        return {(source.minimum[0] + source.maximum[0]) * 0.5F,
                (source.minimum[1] + source.maximum[1]) * 0.5F,
                (source.minimum[2] + source.maximum[2]) * 0.5F};
    }
    const auto& group = source.materialGroups[
        static_cast<std::size_t>(asset.subMesh)];
    r3d::physics::Vec3 minimum{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()};
    r3d::physics::Vec3 maximum{
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()};
    bool found = false;
    const std::size_t end = std::min<std::size_t>(
        static_cast<std::size_t>(group.firstIndex) + group.indexCount,
        source.indices.size());
    for (std::size_t index = group.firstIndex; index < end; ++index)
    {
        const auto vertexIndex = source.indices[index];
        if (vertexIndex >= source.vertices.size())
            continue;
        const auto& position =
            source.vertices[vertexIndex].position;
        minimum.x = std::min(minimum.x, position[0]);
        minimum.y = std::min(minimum.y, position[1]);
        minimum.z = std::min(minimum.z, position[2]);
        maximum.x = std::max(maximum.x, position[0]);
        maximum.y = std::max(maximum.y, position[1]);
        maximum.z = std::max(maximum.z, position[2]);
        found = true;
    }
    if (!found)
        return {};
    return {(minimum.x + maximum.x) * 0.5F,
            (minimum.y + maximum.y) * 0.5F,
            (minimum.z + maximum.z) * 0.5F};
}

void drawShadowGroups(GraphicsDevice& device,
                      const OriginalRaceRenderer::Asset& asset,
                      Shader shader, const Transform& model,
                      const PipelineState& pipeline,
                      float elapsedSeconds,
                      const r3d::game::originalrace::VisualNode* node)
{
    if (asset.textures.empty())
        return;
    const auto geometryPipeline = nodePipeline(pipeline, node);
    auto stateFor = [elapsedSeconds, node](const auto& material) {
        MaterialState state;
        state.alphaReference = material.alphaReference;
        const float frame =
            node != nullptr
                ? visualAnimationFrame(*node, elapsedSeconds)
                : 0.0F;
        state.textureTransform = materialTextureTransform(material, frame);
        state.receivesShadow = false;
        return state;
    };
    const auto& source = sourceAsset(asset);
    if (source.materialGroups.empty())
    {
        const auto state =
            asset.materials.empty()
                ? MaterialState{}
                : stateFor(asset.materials.front());
        device.draw(asset.mesh, shader, asset.textures.front(), model,
                    geometryPipeline, {}, state);
        return;
    }
    if (asset.subMesh >= 0 &&
        static_cast<std::size_t>(asset.subMesh) <
            source.materialGroups.size())
    {
        const auto& group = source.materialGroups[
            static_cast<std::size_t>(asset.subMesh)];
        const auto state =
            asset.materials.empty()
                ? MaterialState{}
                : stateFor(asset.materials.front());
        device.draw(asset.mesh, shader, asset.textures.front(), model,
                    geometryPipeline,
                    {group.firstIndex, group.indexCount}, state);
        return;
    }
    for (std::size_t index = 0;
         index < source.materialGroups.size(); ++index)
    {
        const auto& group = source.materialGroups[index];
        const auto materialIndex =
            asset.materials.empty()
                ? 0U
                : std::min(index, asset.materials.size() - 1U);
        const auto textureIndex =
            std::min(index, asset.textures.size() - 1U);
        const auto state =
            asset.materials.empty()
                ? MaterialState{}
                : stateFor(asset.materials[materialIndex]);
        device.draw(asset.mesh, shader, asset.textures[textureIndex], model,
                    geometryPipeline,
                    {group.firstIndex, group.indexCount}, state);
    }
}

} // namespace

bool OriginalRaceRenderer::createFrameTargets(
    GraphicsDevice& device, std::uint32_t width,
    std::uint32_t height, std::string& error)
{
    destroyFrameTargets(device);
    frameWidth_ = std::max(width, 1U);
    frameHeight_ = std::max(height, 1U);
    const auto targetWidth = static_cast<std::uint16_t>(
        std::min(frameWidth_, std::uint32_t(UINT16_MAX)));
    const auto targetHeight = static_cast<std::uint16_t>(
        std::min(frameHeight_, std::uint32_t(UINT16_MAX)));
    const auto halfWidth = static_cast<std::uint16_t>(
        std::max<std::uint32_t>(targetWidth / 2U, 1U));
    const auto halfHeight = static_cast<std::uint16_t>(
        std::max<std::uint32_t>(targetHeight / 2U, 1U));
    hdrTarget_ = device.createRenderTarget(
        targetWidth, targetHeight, RenderTargetFormat::Rgba16F,
        true, "Motor Rock HDR color");
    waterSceneTarget_ = device.createRenderTarget(
        targetWidth, targetHeight, RenderTargetFormat::Rgba16F,
        true, "Motor Rock water source color/depth");
    reflectionTarget_ = device.createRenderTarget(
        halfWidth, halfHeight, RenderTargetFormat::Rgba8,
        true, "Motor Rock planar reflection");
    shadowTarget_ = device.createRenderTarget(
        2048, 2048, RenderTargetFormat::R32F,
        true, "Motor Rock directional shadow near split");
    shadowTargetFar_ = device.createRenderTarget(
        2048, 2048, RenderTargetFormat::R32F,
        true, "Motor Rock directional shadow far split");
    shadowTargetThird_ = device.createRenderTarget(
        2048, 2048, RenderTargetFormat::R32F,
        true, "Motor Rock third spot shadow map");
    luminance64Target_ = device.createRenderTarget(
        64, 64, RenderTargetFormat::Rgba16F, false,
        "Motor Rock luminance 64");
    luminance16Target_ = device.createRenderTarget(
        16, 16, RenderTargetFormat::Rgba16F, false,
        "Motor Rock luminance 16");
    luminance4Target_ = device.createRenderTarget(
        4, 4, RenderTargetFormat::Rgba16F, false,
        "Motor Rock luminance 4");
    luminance1Target_ = device.createRenderTarget(
        1, 1, RenderTargetFormat::Rgba16F, false,
        "Motor Rock luminance current");
    adaptedLuminanceTargetA_ = device.createRenderTarget(
        1, 1, RenderTargetFormat::Rgba16F, false,
        "Motor Rock luminance adapted A");
    adaptedLuminanceTargetB_ = device.createRenderTarget(
        1, 1, RenderTargetFormat::Rgba16F, false,
        "Motor Rock luminance adapted B");
    bloomTargetA_ = device.createRenderTarget(
        128, 128, RenderTargetFormat::Rgba16F,
        false, "Motor Rock bloom A");
    bloomTargetB_ = device.createRenderTarget(
        128, 128, RenderTargetFormat::Rgba16F,
        false, "Motor Rock bloom B");
    if (sunShaftResourcesEnabled_)
    {
        sunShaftSceneTarget_ = device.createRenderTarget(
            targetWidth, targetHeight, RenderTargetFormat::Rgba8,
            false, "Motor Rock sun shaft source color");
        sunShaftBlurTargetA_ = device.createRenderTarget(
            640, 512, RenderTargetFormat::Rgba8,
            false, "Motor Rock sun shaft blur 2x");
        sunShaftBlurTargetB_ = device.createRenderTarget(
            320, 256, RenderTargetFormat::Rgba8,
            false, "Motor Rock sun shaft blur 4x");
    }
    if (!valid(hdrTarget_) || !valid(waterSceneTarget_) ||
        !valid(reflectionTarget_) || !valid(shadowTarget_) ||
        !valid(shadowTargetFar_) || !valid(shadowTargetThird_) ||
        !valid(luminance64Target_) || !valid(luminance16Target_) ||
        !valid(luminance4Target_) || !valid(luminance1Target_) ||
        !valid(adaptedLuminanceTargetA_) ||
        !valid(adaptedLuminanceTargetB_) ||
        !valid(bloomTargetA_) || !valid(bloomTargetB_) ||
        (sunShaftResourcesEnabled_ &&
         (!valid(sunShaftSceneTarget_) ||
          !valid(sunShaftBlurTargetA_) ||
          !valid(sunShaftBlurTargetB_))))
    {
        error = "bgfx/Metal could not create M9.5 render targets";
        destroyFrameTargets(device);
        return false;
    }
    error.clear();
    return true;
}

void OriginalRaceRenderer::destroyFrameTargets(
    GraphicsDevice& device) noexcept
{
    if (valid(sunShaftBlurTargetB_))
        device.destroy(sunShaftBlurTargetB_);
    if (valid(sunShaftBlurTargetA_))
        device.destroy(sunShaftBlurTargetA_);
    if (valid(sunShaftSceneTarget_))
        device.destroy(sunShaftSceneTarget_);
    if (valid(bloomTargetB_))
        device.destroy(bloomTargetB_);
    if (valid(bloomTargetA_))
        device.destroy(bloomTargetA_);
    if (valid(shadowTarget_))
        device.destroy(shadowTarget_);
    if (valid(shadowTargetFar_))
        device.destroy(shadowTargetFar_);
    if (valid(shadowTargetThird_))
        device.destroy(shadowTargetThird_);
    if (valid(adaptedLuminanceTargetB_))
        device.destroy(adaptedLuminanceTargetB_);
    if (valid(adaptedLuminanceTargetA_))
        device.destroy(adaptedLuminanceTargetA_);
    if (valid(luminance1Target_))
        device.destroy(luminance1Target_);
    if (valid(luminance4Target_))
        device.destroy(luminance4Target_);
    if (valid(luminance16Target_))
        device.destroy(luminance16Target_);
    if (valid(luminance64Target_))
        device.destroy(luminance64Target_);
    if (valid(reflectionTarget_))
        device.destroy(reflectionTarget_);
    if (valid(hdrTarget_))
        device.destroy(hdrTarget_);
    if (valid(waterSceneTarget_))
        device.destroy(waterSceneTarget_);
    sunShaftBlurTargetB_ = {};
    sunShaftBlurTargetA_ = {};
    sunShaftSceneTarget_ = {};
    bloomTargetB_ = {};
    bloomTargetA_ = {};
    shadowTarget_ = {};
    shadowTargetFar_ = {};
    shadowTargetThird_ = {};
    adaptedLuminanceTargetB_ = {};
    adaptedLuminanceTargetA_ = {};
    luminance1Target_ = {};
    luminance4Target_ = {};
    luminance16Target_ = {};
    luminance64Target_ = {};
    reflectionTarget_ = {};
    hdrTarget_ = {};
    waterSceneTarget_ = {};
    adaptedLuminanceAIsCurrent_ = false;
    luminanceAdaptationInitialized_ = false;
    previousRenderSeconds_ = 0.0F;
    frameWidth_ = 0;
    frameHeight_ = 0;
}

bool OriginalRaceRenderer::resize(
    GraphicsDevice& device, std::uint32_t width,
    std::uint32_t height, std::string& error)
{
    if (frameWidth_ == std::max(width, 1U) &&
        frameHeight_ == std::max(height, 1U))
    {
        error.clear();
        return true;
    }
    return createFrameTargets(device, width, height, error);
}

bool OriginalRaceRenderer::initialize(
    GraphicsDevice& device,
    OriginalResourceManager& resources,
    const r3d::game::originalrace::Race& race,
    std::uint32_t width, std::uint32_t height, std::string& error)
{
    try
    {
        if (!sourceCameraMathValid())
        {
            error = "source CameraManager axis rotation regression";
            return false;
        }
        perspectiveFarDistance_ =
            std::max(race.environment.perspectiveFarDistance, 1.0F);
        // GraphManager creates SunShaft only for daytime worlds with an
        // active directional light. Garage, Angar and Night therefore do
        // not consume its three render-target handles.
        sunShaftResourcesEnabled_ =
            race.environment.directionalLightEnabled &&
            race.environment.weather !=
                r3d::game::originalrace::Weather::Night;
        shadowShader_ = device.createShader(
            {rrr3d_vs_shadow_map, sizeof(rrr3d_vs_shadow_map)},
            {rrr3d_fs_shadow_map, sizeof(rrr3d_fs_shadow_map)},
            "original-shadow-map");
        skyShader_ = device.createShader(
            {rrr3d_vs_skybox, sizeof(rrr3d_vs_skybox)},
            {rrr3d_fs_skybox, sizeof(rrr3d_fs_skybox)},
            "original-cube-sky");
        bloomExtractShader_ = device.createShader(
            {rrr3d_vs_post_process,
             sizeof(rrr3d_vs_post_process)},
            {rrr3d_fs_bloom_extract,
             sizeof(rrr3d_fs_bloom_extract)},
            "original-bloom-extract");
        bloomBlurShader_ = device.createShader(
            {rrr3d_vs_post_process,
             sizeof(rrr3d_vs_post_process)},
            {rrr3d_fs_bloom_blur,
             sizeof(rrr3d_fs_bloom_blur)},
            "original-bloom-blur");
        toneMapShader_ = device.createShader(
            {rrr3d_vs_post_process,
             sizeof(rrr3d_vs_post_process)},
            {rrr3d_fs_tone_map, sizeof(rrr3d_fs_tone_map)},
            "original-tone-map");
        copyShader_ = device.createShader(
            {rrr3d_vs_post_process,
             sizeof(rrr3d_vs_post_process)},
            {rrr3d_fs_copy, sizeof(rrr3d_fs_copy)},
            "original-scene-copy");
        waterShader_ = device.createShader(
            {rrr3d_vs_water, sizeof(rrr3d_vs_water)},
            {rrr3d_fs_water, sizeof(rrr3d_fs_water)},
            "original-water-plane");
        fogPlaneShader_ = device.createShader(
            {rrr3d_vs_water, sizeof(rrr3d_vs_water)},
            {rrr3d_fs_fog_plane, sizeof(rrr3d_fs_fog_plane)},
            "original-volume-fog-plane");
        grassShader_ = device.createShader(
            {rrr3d_vs_grass_field, sizeof(rrr3d_vs_grass_field)},
            {rrr3d_fs_grass_field, sizeof(rrr3d_fs_grass_field)},
            "original-grass-field");
        luminanceLogShader_ = device.createShader(
            {rrr3d_vs_post_process,
             sizeof(rrr3d_vs_post_process)},
            {rrr3d_fs_luminance_log,
             sizeof(rrr3d_fs_luminance_log)},
            "original-hdr-luminance-log");
        luminanceDownsampleShader_ = device.createShader(
            {rrr3d_vs_post_process,
             sizeof(rrr3d_vs_post_process)},
            {rrr3d_fs_luminance_downsample,
             sizeof(rrr3d_fs_luminance_downsample)},
            "original-hdr-luminance-downsample");
        luminanceAdaptShader_ = device.createShader(
            {rrr3d_vs_post_process,
             sizeof(rrr3d_vs_post_process)},
            {rrr3d_fs_luminance_adapt,
             sizeof(rrr3d_fs_luminance_adapt)},
            "original-hdr-luminance-adapt");
        sunShaftPrepareShader_ = device.createShader(
            {rrr3d_vs_post_process,
             sizeof(rrr3d_vs_post_process)},
            {rrr3d_fs_sun_shaft_prepare,
             sizeof(rrr3d_fs_sun_shaft_prepare)},
            "original-sun-shaft-prepare");
        sunShaftCompositeShader_ = device.createShader(
            {rrr3d_vs_post_process,
             sizeof(rrr3d_vs_post_process)},
            {rrr3d_fs_sun_shaft_composite,
             sizeof(rrr3d_fs_sun_shaft_composite)},
            "original-sun-shaft-composite");
        refractionShader_ = device.createShader(
            {rrr3d_vs_refraction, sizeof(rrr3d_vs_refraction)},
            {rrr3d_fs_refraction, sizeof(rrr3d_fs_refraction)},
            "original-refraction");
        postProcessMesh_ = device.createMesh(
            postProcessVertices.data(), postProcessVertices.size(),
            postProcessIndices.data(), postProcessIndices.size());
        environmentReflectionTarget_ = device.createCubeRenderTarget(
            512, RenderTargetFormat::Rgba8, true,
            "Motor Rock true reflection");
        if (!valid(shadowShader_) || !valid(skyShader_) ||
            !valid(environmentReflectionTarget_) ||
            !valid(bloomExtractShader_) ||
            !valid(bloomBlurShader_) || !valid(toneMapShader_) ||
            !valid(copyShader_) || !valid(waterShader_) ||
            !valid(fogPlaneShader_) || !valid(grassShader_) ||
            !valid(luminanceLogShader_) ||
            !valid(luminanceDownsampleShader_) ||
            !valid(luminanceAdaptShader_) ||
            !valid(sunShaftPrepareShader_) ||
            !valid(sunShaftCompositeShader_) ||
            !valid(refractionShader_) ||
            !valid(postProcessMesh_))
        {
            error =
                "Unable to create original multipass resources: "
                "shadow=" + std::to_string(valid(shadowShader_)) +
                ", sky=" + std::to_string(valid(skyShader_)) +
                ", cube=" +
                    std::to_string(valid(environmentReflectionTarget_)) +
                ", bloomExtract=" +
                    std::to_string(valid(bloomExtractShader_)) +
                ", bloomBlur=" +
                    std::to_string(valid(bloomBlurShader_)) +
                ", toneMap=" + std::to_string(valid(toneMapShader_)) +
                ", copy=" + std::to_string(valid(copyShader_)) +
                ", water=" + std::to_string(valid(waterShader_)) +
                ", fogPlane=" +
                    std::to_string(valid(fogPlaneShader_)) +
                ", grass=" + std::to_string(valid(grassShader_)) +
                ", luminance=" +
                    std::to_string(valid(luminanceLogShader_) &&
                                   valid(luminanceDownsampleShader_) &&
                                   valid(luminanceAdaptShader_)) +
                ", sunShaft=" +
                    std::to_string(valid(sunShaftPrepareShader_) &&
                                   valid(sunShaftCompositeShader_)) +
                ", postMesh=" +
                    std::to_string(valid(postProcessMesh_));
            throw r3d::resource::ResourceError(
                error);
        }
        if (!createFrameTargets(device, width, height, error))
            throw r3d::resource::ResourceError(error);
        auto uploadOriginalTexture =
            [&](std::string_view path) {
                return resources.GetTexture(path).texture;
            };
        auto load = [&](Asset& asset,
                        const r3d::game::originalrace::VisualNode& node) {
            if (node.plane)
            {
                asset.mesh = device.createMesh(
                    effectVertices.data(), effectVertices.size(),
                    effectIndices.data(), effectIndices.size());
            }
            else
            {
                const auto& shared = resources.GetMesh(node.meshPath);
                asset.source = shared.source;
                asset.mesh = shared.mesh;
                asset.sharedMesh = true;
            }
            asset.materials = node.materials;
            asset.subMesh = node.subMesh;
            for (const auto& material : node.materials)
            {
                if (material.texturePath.empty())
                {
                    asset.textures.push_back(
                        resources.GetWhiteTexture().texture);
                }
                else
                {
                    asset.textures.push_back(
                        uploadOriginalTexture(material.texturePath));
                }
                const auto& auxiliaryTexturePath =
                    !material.normalTexturePath.empty()
                        ? material.normalTexturePath
                        : material.reflectionTexturePath;
                if (auxiliaryTexturePath.empty())
                {
                    asset.normalTextures.push_back({});
                }
                else
                {
                    asset.normalTextures.push_back(
                        uploadOriginalTexture(
                            auxiliaryTexturePath));
                }
            }
            bool normalTexturesValid =
                asset.normalTextures.size() == node.materials.size();
            for (std::size_t index = 0;
                 index < node.materials.size() && normalTexturesValid;
                 ++index)
            {
                normalTexturesValid =
                    (node.materials[index].normalTexturePath.empty() &&
                     node.materials[index]
                         .reflectionTexturePath.empty()) ||
                    valid(asset.normalTextures[index]);
            }
            if (!valid(asset.mesh) || asset.textures.empty() ||
                std::any_of(asset.textures.begin(), asset.textures.end(),
                            [](Texture value) { return !valid(value); }) ||
                !normalTexturesValid)
                throw r3d::resource::ResourceError(
                    "Unable to upload original race asset " +
                    node.meshPath);
        };
        auto loadObject = [&](ObjectAsset& asset,
                              const std::vector<
                                  r3d::game::originalrace::VisualNode>&
                                  nodes) {
            asset.nodes.resize(nodes.size());
            for (std::size_t index = 0; index < nodes.size(); ++index)
                load(asset.nodes[index], nodes[index]);
        };
        auto loadParticleTextures =
            [&](ObjectAsset& asset,
                const std::vector<
                    r3d::game::originalrace::
                        ParticleEmitterDefinition>& emitters) {
                asset.particleTextures.resize(emitters.size());
                asset.particleNodes.resize(emitters.size());
                for (std::size_t emitter = 0;
                     emitter < emitters.size(); ++emitter)
                {
                    if (emitters[emitter].renderMode ==
                        r3d::game::originalrace::
                            ParticleRenderMode::Node)
                    {
                        auto& output = asset.particleNodes[emitter];
                        output.resize(
                            emitters[emitter].nodeVisuals.size());
                        for (std::size_t node = 0;
                             node < output.size(); ++node)
                        {
                            load(output[node],
                                 emitters[emitter]
                                     .nodeVisuals[node]);
                        }
                        if (output.empty())
                        {
                            throw r3d::resource::ResourceError(
                                "Original FxNodeManager has no node "
                                "visuals");
                        }
                        continue;
                    }
                    auto& output = asset.particleTextures[emitter];
                    for (const auto& material :
                         emitters[emitter].materials)
                    {
                        if (material.texturePath.empty())
                        {
                            output.push_back(
                                resources.GetWhiteTexture().texture);
                        }
                        else
                        {
                            output.push_back(
                                uploadOriginalTexture(
                                    material.texturePath));
                        }
                    }
                    if (output.empty() ||
                        std::any_of(
                            output.begin(), output.end(),
                            [](Texture value) {
                                return !valid(value);
                            }))
                    {
                        throw r3d::resource::ResourceError(
                            "Unable to upload original particle "
                            "materials");
                    }
                }
            };
        auto loadDefinition =
            [&](ObjectAsset& asset,
                const r3d::game::originalrace::ObjectDefinition&
                    definition) {
                asset.planarReflection = definition.planarReflection;
                asset.castsShadow = definition.castsShadow;
                asset.graphVector1 = definition.graphVector1;
                asset.graphVector3 = definition.graphVector3;
                asset.lighting = definition.lighting;
                loadObject(asset, definition.visualNodes);
                loadParticleTextures(
                    asset, definition.particleEmitters);
            };

        tracks_.resize(race.trackDefinitions.size());
        for (std::size_t index = 0; index < tracks_.size(); ++index)
            loadDefinition(tracks_[index],
                           race.trackDefinitions[index]);
        decorations_.resize(race.decorationDefinitions.size());
        decorationPieces_.resize(race.decorationDefinitions.size());
        for (std::size_t index = 0; index < decorations_.size(); ++index)
        {
            loadDefinition(decorations_[index],
                           race.decorationDefinitions[index]);
            const auto& pieces =
                race.decorationDefinitions[index].destructionPieces;
            decorationPieces_[index].resize(pieces.size());
            for (std::size_t piece = 0; piece < pieces.size(); ++piece)
            {
                auto& asset = decorationPieces_[index][piece];
                asset.castsShadow =
                    race.decorationDefinitions[index].castsShadow;
                asset.lighting =
                    race.decorationDefinitions[index].lighting;
                loadObject(asset, pieces[piece].visualNodes);
            }
        }
        bonuses_.resize(race.bonuses.size());
        bonusDeathEffects_.resize(race.bonuses.size());
        for (std::size_t index = 0; index < bonuses_.size(); ++index)
        {
            loadDefinition(bonuses_[index],
                           race.bonuses[index].visual);
            if (!race.bonuses[index].deathEffect.visual.visualNodes.empty() ||
                !race.bonuses[index].deathEffect.visual.particleEmitters.empty())
            {
                loadDefinition(bonusDeathEffects_[index],
                               race.bonuses[index].deathEffect.visual);
            }
        }
        loadDefinition(rainEffect_, race.rainEffect);
        loadDefinition(wheelTrailEffect_, race.wheelTrailEffect);
        loadDefinition(wheelSmokeEffect_, race.wheelSmokeEffect);
        loadDefinition(contactEffect_, race.contactEffect);

        weapons_.resize(race.weapons.size());
        weaponShotEffects_.resize(race.weapons.size());
        for (std::size_t index = 0; index < weapons_.size(); ++index)
        {
            weapons_[index].nodes.resize(1);
            load(weapons_[index].nodes.front(),
                 race.weapons[index].visual);
            const auto& shot = race.weapons[index].shotEffect.visual;
            if (!shot.visualNodes.empty() ||
                !shot.particleEmitters.empty())
            {
                loadDefinition(weaponShotEffects_[index], shot);
            }
        }
        projectiles_.resize(race.weapons.size());
        for (std::size_t weapon = 0; weapon < race.weapons.size();
             ++weapon)
        {
            const auto& definitions = race.weapons[weapon].projectiles;
            projectiles_[weapon].resize(definitions.size());
            for (std::size_t projectile = 0;
                 projectile < definitions.size(); ++projectile)
            {
                const auto& definition = definitions[projectile];
                auto& assets = projectiles_[weapon][projectile];
                if (!definition.visual.visualNodes.empty() ||
                    !definition.visual.particleEmitters.empty())
                {
                    loadDefinition(assets.visual,
                                   definition.visual);
                }
                if (!definition.secondaryVisual.visualNodes.empty() ||
                    !definition.secondaryVisual.particleEmitters.empty())
                {
                    loadDefinition(assets.secondaryVisual,
                                   definition.secondaryVisual);
                }
                if (!definition.tertiaryVisual.visualNodes.empty() ||
                    !definition.tertiaryVisual.particleEmitters.empty())
                {
                    loadDefinition(assets.tertiaryVisual,
                                   definition.tertiaryVisual);
                }
                if (!definition.deathEffect.visual.visualNodes.empty() ||
                    !definition.deathEffect.visual.particleEmitters.empty())
                {
                    loadDefinition(assets.deathVisual,
                                   definition.deathEffect.visual);
                }
                if (definition.secondaryProjectile.valid &&
                    (!definition.secondaryProjectile.deathEffect.visual
                          .visualNodes.empty() ||
                     !definition.secondaryProjectile.deathEffect.visual
                          .particleEmitters.empty()))
                {
                    loadDefinition(
                        assets.secondaryDeathVisual,
                        definition.secondaryProjectile.deathEffect.visual);
                }
                if (definition.tertiaryProjectile.valid &&
                    (!definition.tertiaryProjectile.deathEffect.visual
                          .visualNodes.empty() ||
                     !definition.tertiaryProjectile.deathEffect.visual
                          .particleEmitters.empty()))
                {
                    loadDefinition(
                        assets.tertiaryDeathVisual,
                        definition.tertiaryProjectile.deathEffect.visual);
                }
            }
        }

        // A tournament opponent can use a wheel/upgrade set that differs
        // from another racer driving the same base car.  The Windows game
        // applies those slots per Player, so GPU assets are keyed by racer
        // rather than by the shared garage record.
        vehicleBodies_.resize(race.racers.size());
        vehicleWheels_.resize(race.racers.size());
        vehicleTrackVisuals_.resize(race.racers.size());
        vehicleCushionVisuals_.resize(race.racers.size());
        vehicleLowLifeEffects_.resize(race.racers.size());
        vehicleEnergyDamageEffects_.resize(race.racers.size());
        vehicleShieldEffects_.resize(race.racers.size());
        vehicleShieldScales_.resize(race.racers.size());
        vehicleDeathEffects_.resize(race.racers.size());
        for (std::size_t racer = 0; racer < race.racers.size(); ++racer)
        {
            const auto& sourceRacer = race.racers[racer];
            const auto& vehicle =
                sourceRacer.hasConfiguredVehicle
                    ? sourceRacer.configuredVehicle
                    : race.vehicles.at(sourceRacer.vehicle);
            loadObject(vehicleBodies_[racer], vehicle.bodyVisuals);
            vehicleBodies_[racer].lighting = vehicle.lighting;
            loadObject(
                vehicleTrackVisuals_[racer], vehicle.trackVisuals);
            vehicleTrackVisuals_[racer].lighting =
                r3d::game::originalrace::LightingMode::Standard;
            loadObject(
                vehicleCushionVisuals_[racer],
                vehicle.cushionVisuals);
            vehicleCushionVisuals_[racer].lighting =
                r3d::game::originalrace::LightingMode::Reflection;
            loadDefinition(
                vehicleLowLifeEffects_[racer],
                vehicle.lowLifeEffect);
            loadDefinition(
                vehicleEnergyDamageEffects_[racer],
                vehicle.energyDamageEffect);
            const bool hasShieldEffect =
                !vehicle.shieldEffect.visualNodes.empty() ||
                !vehicle.shieldEffect.particleEmitters.empty();
            if (hasShieldEffect)
            {
                loadDefinition(
                    vehicleShieldEffects_[racer],
                    vehicle.shieldEffect);
            }
            const r3d::physics::Transform identity;
            const auto bodyBounds = objectBounds(
                vehicleBodies_[racer], vehicle.bodyVisuals, identity);
            if (!bodyBounds.valid)
            {
                throw r3d::resource::ResourceError(
                    "Unable to calculate source vehicle bounds for " +
                    vehicle.record);
            }
            if (hasShieldEffect)
            {
                const auto shieldBounds = objectBounds(
                    vehicleShieldEffects_[racer],
                    vehicle.shieldEffect.visualNodes, identity);
                if (!shieldBounds.valid)
                {
                    throw r3d::resource::ResourceError(
                        "Unable to calculate source ImmortalEffect bounds for " +
                        vehicle.record);
                }
                const r3d::physics::Vec3 bodySize{
                    bodyBounds.maximum.x - bodyBounds.minimum.x,
                    bodyBounds.maximum.y - bodyBounds.minimum.y,
                    bodyBounds.maximum.z - bodyBounds.minimum.z};
                const r3d::physics::Vec3 shieldSize{
                    shieldBounds.maximum.x - shieldBounds.minimum.x,
                    shieldBounds.maximum.y - shieldBounds.minimum.y,
                    shieldBounds.maximum.z - shieldBounds.minimum.z};
                if (shieldSize.x <= 0.0001F ||
                    shieldSize.y <= 0.0001F ||
                    shieldSize.z <= 0.0001F)
                {
                    throw r3d::resource::ResourceError(
                        "Source ImmortalEffect has empty bounds for " +
                        vehicle.record);
                }
                // ImmortalEffect::OnImmortalStatus fits the effect AABB
                // around the source car AABB, then applies
                // DataBase::LoadCar's scaleK.
                vehicleShieldScales_[racer] = {
                    bodySize.x / shieldSize.x,
                    bodySize.y / shieldSize.y,
                    bodySize.z / shieldSize.z};
            }
            vehicleWheels_[racer].resize(vehicle.wheelVisuals.size());
            for (std::size_t wheel = 0;
                 wheel < vehicle.wheelVisuals.size(); ++wheel)
            {
                auto& object = vehicleWheels_[racer][wheel];
                object.nodes.resize(1);
                load(object.nodes.front(), vehicle.wheelVisuals[wheel]);
            }
            vehicleDeathEffects_[racer].resize(
                vehicle.deathEffects.size());
            for (std::size_t effect = 0;
                 effect < vehicle.deathEffects.size(); ++effect)
            {
                loadDefinition(
                    vehicleDeathEffects_[racer][effect],
                    vehicle.deathEffects[effect].visual);
            }
        }
        const auto skyCpuVertices = skyVertices();
        const auto skyCpuIndices = skyIndices();
        skyMesh_ = device.createMesh(
            skyCpuVertices.data(), skyCpuVertices.size(),
            skyCpuIndices.data(), skyCpuIndices.size());
        skyTexture_ = resources.GetTexture(
            race.environment.skyTexturePath).texture;
        if (!valid(skyMesh_) || !valid(skyTexture_))
            throw r3d::resource::ResourceError(
                "Unable to upload original sky " +
                race.environment.skyTexturePath);
        effectMesh_ = device.createMesh(
            effectVertices.data(), effectVertices.size(),
            effectIndices.data(), effectIndices.size());
        debugTraceTexture_ = resources.GetWhiteTexture().texture;
        if (!valid(debugTraceTexture_))
            throw r3d::resource::ResourceError(
                "Unable to load original TraceGfx material texture");

        // GraphManager::BuildOctree derives _groundAABB and the world AABB
        // used by SunShaft from all map actors before cars are created.
        WorldBounds sceneBounds;
        for (const auto& instance : race.trackInstances)
        {
            if (instance.definition >= tracks_.size() ||
                instance.definition >= race.trackDefinitions.size())
                continue;
            include(
                sceneBounds,
                objectBounds(
                    tracks_[instance.definition],
                    race.trackDefinitions[instance.definition].visualNodes,
                    instance.transform));
        }
        for (const auto& instance : race.decorationInstances)
        {
            if (instance.definition >= decorations_.size() ||
                instance.definition >= race.decorationDefinitions.size())
                continue;
            include(
                sceneBounds,
                objectBounds(
                    decorations_[instance.definition],
                    race.decorationDefinitions[instance.definition]
                        .visualNodes,
                    instance.transform));
        }
        for (std::size_t index = 0;
             index < race.bonuses.size() && index < bonuses_.size();
             ++index)
        {
            include(
                sceneBounds,
                objectBounds(
                    bonuses_[index], race.bonuses[index].visual.visualNodes,
                    race.bonuses[index].transform));
        }
        if (!sceneBounds.valid)
        {
            for (const auto& point : race.tracePoints)
            {
                const float radius = std::max(point.width * 0.5F, 1.0F);
                include(sceneBounds,
                        r3d::physics::Vec3{
                            point.position.x - radius,
                            point.position.y - radius, 0.0F});
                include(sceneBounds,
                        r3d::physics::Vec3{
                            point.position.x + radius,
                            point.position.y + radius, 0.0F});
            }
        }
        sceneWorldCenter_ = {
            (sceneBounds.minimum.x + sceneBounds.maximum.x) * 0.5F,
            (sceneBounds.minimum.y + sceneBounds.maximum.y) * 0.5F,
            (sceneBounds.minimum.z + sceneBounds.maximum.z) * 0.5F};
        if (race.environment.surface !=
            r3d::game::originalrace::EnvironmentSurface::None)
        {
            std::string_view surfacePath;
            switch (race.environment.surface)
            {
            case r3d::game::originalrace::EnvironmentSurface::Grass:
                surfacePath = "Data/Misc/StadiumGrass1.dds";
                break;
            case r3d::game::originalrace::EnvironmentSurface::Water:
                surfacePath = "Data/Effect/waterColor.dds";
                break;
            case r3d::game::originalrace::EnvironmentSurface::GroundFog:
                surfacePath = "Data/Effect/clouds.dds";
                break;
            case r3d::game::originalrace::EnvironmentSurface::Magma:
                surfacePath = "Data/Effect/magma.dds";
                break;
            case r3d::game::originalrace::EnvironmentSurface::None:
                break;
            }
            environmentSurfaceTexture_ =
                resources.GetTexture(surfacePath).texture;
            if (race.environment.surface ==
                r3d::game::originalrace::EnvironmentSurface::Water)
            {
                constexpr std::string_view normalPath =
                    "Data/Misc/water00.png";
                waterNormalTexture_ =
                    resources.GetTexture(normalPath).texture;
            }

            environmentSurfaceCenter_ = {
                (sceneBounds.minimum.x + sceneBounds.maximum.x) * 0.5F,
                (sceneBounds.minimum.y + sceneBounds.maximum.y) * 0.5F,
                race.environment.surfaceHeight};
            environmentSurfaceSize_ = {
                std::max(sceneBounds.maximum.x - sceneBounds.minimum.x +
                             300.0F,
                         300.0F),
                std::max(sceneBounds.maximum.y - sceneBounds.minimum.y +
                             300.0F,
                         300.0F),
                1.0F};
            if (race.environment.surface ==
                r3d::game::originalrace::EnvironmentSurface::Grass)
            {
                environmentSurfaceCenter_.x = 0.0F;
                grassTexture_ = resources.GetTexture(
                    "Data/Misc/flower2.dds").texture;
                auto grass = sourceGrassField(
                    environmentSurfaceSize_.x,
                    environmentSurfaceSize_.y);
                grassMesh_ = device.createMesh(
                    grass.vertices.data(), grass.vertices.size(),
                    grass.indices.data(), grass.indices.size());
                grassFieldOffsets_ = std::move(grass.fieldOffsets);
            }
        }
        auto loadEffectTexture = [&](std::string_view path) {
            return resources.GetTexture(path).texture;
        };
        vehicleLightTexture_ =
            loadEffectTexture("Data/Effect/flare2b.dds");
        if (!valid(effectMesh_) ||
            !valid(vehicleLightTexture_) ||
            (race.environment.surface !=
                     r3d::game::originalrace::EnvironmentSurface::None &&
             !valid(environmentSurfaceTexture_)) ||
            (race.environment.surface ==
                     r3d::game::originalrace::EnvironmentSurface::Water &&
             !valid(waterNormalTexture_)) ||
            (race.environment.surface ==
                     r3d::game::originalrace::EnvironmentSurface::Grass &&
             (!valid(grassTexture_) || !valid(grassMesh_) ||
              grassFieldOffsets_.empty())))
            throw r3d::resource::ResourceError(
                "Unable to create original material/effect particles");
        error.clear();
        return true;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        shutdown(device);
        return false;
    }
}

void OriginalRaceRenderer::shutdown(GraphicsDevice& device) noexcept
{
    sourceEnvironment_.ReleaseScene();
    destroyFrameTargets(device);
    if (valid(environmentReflectionTarget_))
        device.destroy(environmentReflectionTarget_);
    if (valid(refractionShader_))
        device.destroy(refractionShader_);
    if (valid(sunShaftCompositeShader_))
        device.destroy(sunShaftCompositeShader_);
    if (valid(sunShaftPrepareShader_))
        device.destroy(sunShaftPrepareShader_);
    if (valid(luminanceAdaptShader_))
        device.destroy(luminanceAdaptShader_);
    if (valid(luminanceDownsampleShader_))
        device.destroy(luminanceDownsampleShader_);
    if (valid(luminanceLogShader_))
        device.destroy(luminanceLogShader_);
    if (valid(waterShader_))
        device.destroy(waterShader_);
    if (valid(fogPlaneShader_))
        device.destroy(fogPlaneShader_);
    if (valid(grassShader_))
        device.destroy(grassShader_);
    if (valid(copyShader_))
        device.destroy(copyShader_);
    if (valid(toneMapShader_))
        device.destroy(toneMapShader_);
    if (valid(bloomBlurShader_))
        device.destroy(bloomBlurShader_);
    if (valid(bloomExtractShader_))
        device.destroy(bloomExtractShader_);
    if (valid(shadowShader_))
        device.destroy(shadowShader_);
    if (valid(skyShader_))
        device.destroy(skyShader_);
    if (valid(postProcessMesh_))
        device.destroy(postProcessMesh_);
    toneMapShader_ = {};
    copyShader_ = {};
    waterShader_ = {};
    fogPlaneShader_ = {};
    grassShader_ = {};
    luminanceLogShader_ = {};
    luminanceDownsampleShader_ = {};
    luminanceAdaptShader_ = {};
    sunShaftPrepareShader_ = {};
    sunShaftCompositeShader_ = {};
    refractionShader_ = {};
    bloomBlurShader_ = {};
    bloomExtractShader_ = {};
    shadowShader_ = {};
    skyShader_ = {};
    environmentReflectionTarget_ = {};
    postProcessMesh_ = {};
    debugTraceTexture_ = {};
    auto release = [&](Asset& asset) {
        if (valid(asset.mesh) && !asset.sharedMesh)
            device.destroy(asset.mesh);
        asset = {};
    };
    auto releaseObject = [&](ObjectAsset& object) {
        for (auto& node : object.nodes)
            release(node);
        object.nodes.clear();
        for (auto& emitter : object.particleTextures)
        {
            emitter.clear();
        }
        object.particleTextures.clear();
        for (auto& emitter : object.particleNodes)
        {
            for (auto& node : emitter)
                release(node);
            emitter.clear();
        }
        object.particleNodes.clear();
    };
    for (auto& body : vehicleBodies_)
        releaseObject(body);
    for (auto& wheels : vehicleWheels_)
        for (auto& wheel : wheels)
            releaseObject(wheel);
    for (auto& track : vehicleTrackVisuals_)
        releaseObject(track);
    for (auto& cushion : vehicleCushionVisuals_)
        releaseObject(cushion);
    for (auto& effect : vehicleLowLifeEffects_)
        releaseObject(effect);
    for (auto& effect : vehicleEnergyDamageEffects_)
        releaseObject(effect);
    for (auto& effect : vehicleShieldEffects_)
        releaseObject(effect);
    for (auto& effects : vehicleDeathEffects_)
        for (auto& effect : effects)
            releaseObject(effect);
    for (auto& bonus : bonuses_)
        releaseObject(bonus);
    for (auto& effect : bonusDeathEffects_)
        releaseObject(effect);
    for (auto& weapon : weapons_)
        releaseObject(weapon);
    for (auto& effect : weaponShotEffects_)
        releaseObject(effect);
    for (auto& weapon : projectiles_)
        for (auto& projectile : weapon)
        {
            releaseObject(projectile.visual);
            releaseObject(projectile.secondaryVisual);
            releaseObject(projectile.tertiaryVisual);
            releaseObject(projectile.deathVisual);
            releaseObject(projectile.secondaryDeathVisual);
            releaseObject(projectile.tertiaryDeathVisual);
        }
    for (auto& decoration : decorations_)
        releaseObject(decoration);
    for (auto& definition : decorationPieces_)
        for (auto& piece : definition)
            releaseObject(piece);
    for (auto& track : tracks_)
        releaseObject(track);
    releaseObject(rainEffect_);
    releaseObject(wheelTrailEffect_);
    releaseObject(wheelSmokeEffect_);
    releaseObject(contactEffect_);
    vehicleBodies_.clear();
    vehicleWheels_.clear();
    vehicleTrackVisuals_.clear();
    vehicleCushionVisuals_.clear();
    vehicleLowLifeEffects_.clear();
    vehicleEnergyDamageEffects_.clear();
    vehicleShieldEffects_.clear();
    vehicleShieldScales_.clear();
    vehicleDeathEffects_.clear();
    wheelSmokeStartTimes_.clear();
    wheelSmokeEndTimes_.clear();
    wheelSmokePositions_.clear();
    bonuses_.clear();
    bonusDeathEffects_.clear();
    weapons_.clear();
    weaponShotEffects_.clear();
    projectiles_.clear();
    decorations_.clear();
    decorationPieces_.clear();
    tracks_.clear();
    if (valid(skyMesh_))
        device.destroy(skyMesh_);
    if (valid(effectMesh_))
        device.destroy(effectMesh_);
    if (valid(grassMesh_))
        device.destroy(grassMesh_);
    skyTexture_ = {};
    skyMesh_ = {};
    vehicleLightTexture_ = {};
    environmentSurfaceTexture_ = {};
    waterNormalTexture_ = {};
    grassTexture_ = {};
    effectMesh_ = {};
    grassMesh_ = {};
    wheelTrailPaths_.clear();
    wheelTrailTimes_.clear();
    wheelTrailResetCounts_.clear();
    trackCullOpacityTimes_.clear();
    decorationCullOpacityTimes_.clear();
    wheelTrailUpdateSeconds_ = -1.0F;
    environmentSurfaceCenter_ = {};
    environmentSurfaceSize_ = {};
    sceneWorldCenter_ = {};
    grassFieldOffsets_.clear();
    activeCameraFarDistance_ = 120.0F;
    perspectiveFarDistance_ = 120.0F;
    activeEnvironmentQuality_ = 2U;
    activeLightQuality_ = 2U;
    activeEnvironmentPolicy_ = {};
    sunShaftResourcesEnabled_ = false;
}

Camera OriginalRaceRenderer::makeCamera(
    const GraphicsDevice& device, const r3d::physics::VehicleState& vehicle,
    float sourceFreeWheelSpeed, std::uint32_t width,
    std::uint32_t height,
    r3d::game::originalrace::PreferredCamera style,
    float cameraDistance, float seconds) noexcept
{
    return makeCamera(
        device, vehicle, sourceFreeWheelSpeed, width, height,
        style == r3d::game::originalrace::PreferredCamera::Isometric
            ? RaceCameraStyle::Isometric
            : RaceCameraStyle::ThirdPerson,
        cameraDistance, seconds);
}

Camera OriginalRaceRenderer::makeCamera(
    const GraphicsDevice& device, const r3d::physics::VehicleState& vehicle,
    float sourceFreeWheelSpeed, std::uint32_t width,
    std::uint32_t height, RaceCameraStyle style,
    float cameraDistance, float seconds) noexcept
{
    const float sourceAspect =
        static_cast<float>(std::max(width, 1U)) /
        static_cast<float>(std::max(height, 1U));
    r3d::game::originalrace::source::CameraTarget target;
    target.position = vehicle.body.position;
    target.rotation = vehicle.body.rotation;
    target.linearVelocity = vehicle.linearVelocity;
    target.drivenWheelSpeed = sourceFreeWheelSpeed;
    const auto frame = sourceCamera_.OnFrame(
        target, style, sourceAspect, cameraDistance,
        perspectiveFarDistance_, seconds);

    cameraPosition_ = frame.position;
    cameraViewDirection_ = frame.direction;
    cameraRotation_ = frame.rotation;
    cameraStyle_ = style;
    activeCameraFarDistance_ = frame.farDistance;
    pointSpriteScale_ = frame.pointSpriteScale;

    const bx::Vec3 eye{
        frame.position.x, frame.position.y, frame.position.z};
    const bx::Vec3 at{
        eye.x + frame.direction.x,
        eye.y + frame.direction.y,
        eye.z + frame.direction.z};
    Camera camera;
    bx::mtxLookAt(
        camera.view.data(), eye, at,
        {frame.up.x, frame.up.y, frame.up.z},
        bx::Handedness::Right);
    if (frame.projection ==
        r3d::game::originalrace::source::CameraProjection::Orthographic)
    {
        const float cameraHeight =
            frame.orthographicWidth / sourceAspect;
        bx::mtxOrtho(
            camera.projection.data(),
            -frame.orthographicWidth * 0.5F,
            frame.orthographicWidth * 0.5F,
            -cameraHeight * 0.5F, cameraHeight * 0.5F,
            frame.nearDistance, frame.farDistance, 0.0F,
            device.usesHomogeneousDepth(), bx::Handedness::Right);
    }
    else
    {
        bx::mtxProj(
            camera.projection.data(), frame.verticalFovDegrees,
            sourceAspect, frame.nearDistance, frame.farDistance,
            device.usesHomogeneousDepth(), bx::Handedness::Right);
    }
    return camera;

}

Camera OriginalRaceRenderer::makePresentationCamera(
    const GraphicsDevice& device,
    const r3d::game::originalrace::PresentationCamera& source,
    std::uint32_t width, std::uint32_t height) noexcept
{
    pointSpriteScale_ = 0.25F;
    const float aspect =
        static_cast<float>(std::max(width, 1U)) /
        static_cast<float>(std::max(height, 1U));
    const auto direction = normalize(rotate(
        source.rotation, {1.0F, 0.0F, 0.0F}));
    auto up = normalize(rotate(
        source.rotation, {0.0F, 0.0F, 1.0F}));
    if (std::abs(
            direction.x * up.x + direction.y * up.y +
            direction.z * up.z) > 0.999F)
        up = {0.0F, 0.0F, 1.0F};
    const bx::Vec3 eye{
        source.position.x, source.position.y, source.position.z};
    const bx::Vec3 at{
        eye.x + direction.x, eye.y + direction.y,
        eye.z + direction.z};
    Camera camera;
    bx::mtxLookAt(
        camera.view.data(), eye, at, {up.x, up.y, up.z},
        bx::Handedness::Right);
    bx::mtxProj(
        camera.projection.data(), source.verticalFovDegrees, aspect,
        source.nearDistance, source.farDistance,
        device.usesHomogeneousDepth(), bx::Handedness::Right);
    cameraPosition_ = source.position;
    cameraViewDirection_ = direction;
    cameraStyle_ = RaceCameraStyle::ThirdPerson;
    activeCameraFarDistance_ = std::max(source.farDistance, 1.0F);
    sourceCamera_.Reset();
    return camera;
}

void OriginalRaceRenderer::moveDebugCamera(
    RaceCameraStyle style, float forwardAmount, float rightAmount,
    float seconds) noexcept
{
    sourceCamera_.MoveDebugCamera(
        style, forwardAmount, rightAmount, seconds);
}

void OriginalRaceRenderer::rotateDebugCamera(
    float deltaX, float deltaY) noexcept
{
    sourceCamera_.RotateDebugCamera(deltaX, deltaY);
}

void OriginalRaceRenderer::resetCamera() noexcept
{
    sourceCamera_.Reset();
    cameraPosition_ = {};
    cameraViewDirection_ = {1.0F, 0.0F, 0.0F};
    cameraRotation_ = {};
}

void OriginalRaceRenderer::draw(
    GraphicsDevice& device, Shader shader,
    const r3d::game::originalrace::Race& race,
    const std::vector<r3d::physics::VehicleState>& vehicles,
    const PipelineState& pipeline,
    const std::vector<bool>& decorationActive,
    const std::vector<
        r3d::game::originalrace::DecorationFragmentState>&
        decorationFragments,
    const std::vector<
        r3d::game::originalrace::VehicleDeathFragmentState>&
        vehicleDeathFragments,
    const std::vector<bool>& bonusActive,
    const std::vector<float>& bonusScales,
    const std::vector<r3d::game::originalrace::RacerRuntime>& racerRuntime,
    const std::vector<r3d::game::originalrace::RaceEffect>& effects,
    const std::vector<r3d::game::originalrace::MineRuntime>& mines,
    const std::vector<
        r3d::game::originalrace::ProjectileRuntime>& projectiles,
    float elapsedSeconds, std::int32_t countdownStage,
    bool reflectionPass,
    bool omitEnvironmentSurface, bool refractionPass,
    bool environmentReflectionPass, const Camera* cullingCamera)
{
    SceneLighting sceneLighting;
    const auto sourceSunRay = normalize(rotate(
        race.environment.sunRotation, {1.0F, 0.0F, 0.0F}));
    // D3DLIGHT_DIRECTIONAL.Direction points along the emitted ray, while
    // the portable lighting equation expects the vector from the surface
    // toward the light.
    const r3d::physics::Vec3 sun{
        -sourceSunRay.x, -sourceSunRay.y, -sourceSunRay.z};
    const float sunLength =
        std::sqrt(sun.x * sun.x + sun.y * sun.y + sun.z * sun.z);
    if (race.environment.directionalLightEnabled &&
        sunLength > 0.0001F)
    {
        sceneLighting.lightDirection =
            {sun.x / sunLength, sun.y / sunLength, sun.z / sunLength,
             1.0F};
    }
    else
        sceneLighting.lightDirection = {1.0F, 0.0F, 0.0F, 0.0F};
    sceneLighting.sunPosition = {
        race.environment.sunPosition.x,
        race.environment.sunPosition.y,
        race.environment.sunPosition.z, 1.0F};
    sceneLighting.ambient = race.environment.ambientColor;
    sceneLighting.fogColor = race.environment.fogColor;
    sceneLighting.fogColor[3] =
        activeEnvironmentPolicy_.fog
            ? race.environment.fogIntensity
            : 0.0F;
    std::size_t lampIndex = 0U;
    for (const auto& lamp : race.environment.lamps)
    {
        if (!lamp.enabled ||
            lampIndex >= SceneLighting::maximumSpotLights)
            continue;
        const auto direction = normalize(rotate(
            lamp.rotation, {1.0F, 0.0F, 0.0F}));
        sceneLighting.lampPositions[lampIndex] = {
            lamp.position.x, lamp.position.y, lamp.position.z,
            lamp.range};
        sceneLighting.lampDirections[lampIndex] = {
            direction.x, direction.y, direction.z, 1.0F};
        sceneLighting.lampColors[lampIndex] = lamp.color;
        // LightSource defaults: phi=pi/2 and theta=pi/4.
        sceneLighting.lampCones[lampIndex] = {
            0.7071067812F, 0.9238795325F, 0.0F, 0.0F};
        ++lampIndex;
    }

    // Player::InitLight/SetLightParent own creation, local transforms and
    // attachment. Metal only realizes each enabled source light against the
    // current Jolt body transform.
    const std::size_t headlightRacerCount =
        std::min(vehicles.size(), racerRuntime.size());
    for (std::size_t racerIndex = 0;
         racerIndex < headlightRacerCount &&
         lampIndex < SceneLighting::maximumSpotLights;
         ++racerIndex)
    {
        const auto& presentation =
            racerRuntime[racerIndex].GetPresentationState();
        for (const auto& headlight : presentation.headLights)
        {
            if (!headlight.created || !headlight.enabled ||
                lampIndex >= SceneLighting::maximumSpotLights)
                continue;
            r3d::physics::Transform local;
            local.position = {
                headlight.position.x,
                headlight.position.y,
                headlight.position.z};
            local.rotation = {
                headlight.rotation.x,
                headlight.rotation.y,
                headlight.rotation.z,
                headlight.rotation.w};
            const auto world = compose(
                vehicles[racerIndex].body, local);
            const auto direction = normalize(rotate(
                world.rotation, {1.0F, 0.0F, 0.0F}));
            sceneLighting.lampPositions[lampIndex] = {
                world.position.x, world.position.y, world.position.z,
                headlight.farDistance};
            sceneLighting.lampDirections[lampIndex] = {
                direction.x, direction.y, direction.z, 1.0F};
            sceneLighting.lampColors[lampIndex] = headlight.diffuse;
            sceneLighting.lampCones[lampIndex] = {
                std::cos(headlight.phi * 0.5F),
                std::cos(headlight.theta * 0.5F), 0.0F, 0.0F};
            ++lampIndex;
        }
    }
    sceneLighting.cameraPosition =
        {cameraPosition_.x, cameraPosition_.y, cameraPosition_.z,
         activeCameraFarDistance_};
    if (!refractionPass && !vehicles.empty())
    {
        if (activeEnvironmentPolicy_.sky)
        {
            SceneLighting skyLighting = sceneLighting;
            skyLighting.lightDirection = {0.0F, 0.0F, 1.0F, 0.0F};
            skyLighting.ambient = {1.0F, 1.0F, 1.0F, 1.0F};
            skyLighting.fogColor[3] = 0.0F;
            device.setSceneLighting(skyLighting);
            auto skyPipeline = pipeline;
            skyPipeline.writeDepth = false;
            skyPipeline.depthTest = false;
            skyPipeline.faceCulling = PipelineState::FaceCulling::None;
            r3d::physics::Transform sky;
            // SkyBox strips view translation. Keeping the cube on the
            // actual camera is the equivalent geometry implementation and
            // avoids parallax/edge clipping when camera lead moves away
            // from the player's body.
            sky.position = cameraPosition_;
            // SkyBox.cpp applied this left-handed source -> right-handed
            // render conversion before sampling every original cubemap.
            constexpr float sinHalfRightAngle = 0.7071067811865476F;
            sky.rotation = {sinHalfRightAngle, 0.0F, 0.0F,
                            sinHalfRightAngle};
            sky.scale = {90.0F, 90.0F, 90.0F};
            MaterialState skyMaterial;
            skyMaterial.environmentTexture = skyTexture_;
            skyMaterial.receivesShadow = false;
            device.draw(
                skyMesh_, skyShader_, skyTexture_, transform(sky),
                skyPipeline, {}, skyMaterial);
        }
    }
    device.setSceneLighting(sceneLighting);

    bool deferredEnvironmentSurface = false;
    PipelineState deferredSurfacePipeline;
    MaterialState deferredSurfaceMaterial;
    Transform deferredSurfaceTransform;
    if (!refractionPass && !reflectionPass &&
        !omitEnvironmentSurface &&
        valid(environmentSurfaceTexture_) &&
        race.environment.surface !=
            r3d::game::originalrace::EnvironmentSurface::None)
    {
        auto surfacePipeline = pipeline;
        surfacePipeline.faceCulling =
            PipelineState::FaceCulling::None;
        MaterialState surfaceMaterial;
        surfaceMaterial.emissive = 1.0F;
        surfaceMaterial.specular = 0.0F;
        if (race.environment.surface ==
                r3d::game::originalrace::EnvironmentSurface::Water ||
            race.environment.planarReflection)
        {
            surfaceMaterial.reflectionStrength = 0.62F;
        }
        surfaceMaterial.color = race.environment.surfaceCloudColor;
        const float tileScale =
            std::max(race.environment.surfaceTileScale, 0.0001F);
        // PlaneNode::FillDataPlane uses its local size as the UV range.
        // GraphManager then scales the node by 4/25/50, so the original
        // texture repeats at that world-space interval instead of being
        // stretched once across the complete map.
        surfaceMaterial.textureTransform = {
            environmentSurfaceSize_.x / tileScale,
            environmentSurfaceSize_.y / tileScale, 0.0F, 0.0F};
        r3d::physics::Transform surface;
        surface.position = environmentSurfaceCenter_;
        if (activeEnvironmentQuality_ == 0U &&
            (race.environment.surface ==
                 r3d::game::originalrace::EnvironmentSurface::GroundFog ||
             race.environment.surface ==
                 r3d::game::originalrace::EnvironmentSurface::Magma))
        {
            // GraphManager::UpdateFogPlane preserves this source Low-quality
            // quirk: the simple actor stays at ground AABB Z, while the
            // depth-aware FogPlane uses Environment cloudHeight.
            surface.position.z = 0.0F;
        }
        surface.scale = environmentSurfaceSize_;
        if (surfacePipeline.blendMode ==
            PipelineState::BlendMode::Opaque)
        {
            device.draw(
                effectMesh_, shader, environmentSurfaceTexture_,
                transform(surface), surfacePipeline, {},
                surfaceMaterial);
        }
        else
        {
            deferredEnvironmentSurface = true;
            deferredSurfacePipeline = surfacePipeline;
            deferredSurfaceMaterial = surfaceMaterial;
            deferredSurfaceTransform = transform(surface);
        }
    }

    if (!refractionPass && !reflectionPass &&
        activeEnvironmentPolicy_.grassField &&
        valid(grassMesh_) && valid(grassTexture_))
    {
        auto grassPipeline = pipeline;
        grassPipeline.faceCulling = PipelineState::FaceCulling::None;
        grassPipeline.blendMode = PipelineState::BlendMode::Opaque;
        grassPipeline.alphaBlend = false;
        grassPipeline.writeDepth = true;
        grassPipeline.depthTest = true;
        MaterialState grassMaterial;
        grassMaterial.alphaReference = 17.0F / 255.0F;
        grassMaterial.color =
            race.environment.weather ==
                    r3d::game::originalrace::Weather::Night
                ? race.environment.ambientColor
                : std::array<float, 4>{1.0F, 1.0F, 1.0F, 1.0F};
        for (const auto& offset : grassFieldOffsets_)
        {
            r3d::physics::Transform grass;
            grass.position = {
                environmentSurfaceCenter_.x + offset.x * 1.5F,
                environmentSurfaceCenter_.y + offset.y * 1.5F, 0.9F};
            grass.scale = {1.5F, 1.5F, 1.5F};
            device.draw(
                grassMesh_, grassShader_, grassTexture_, transform(grass),
                grassPipeline, {}, grassMaterial);
        }
    }

    enum class RenderStage
    {
        Opacity,
        CullOpacity,
        Effect,
        Last,
    };
    auto renderStage = [](r3d::game::originalrace::GraphOrder order,
                          bool cullOpacityActor) {
        if (cullOpacityActor)
            return RenderStage::CullOpacity;
        switch (order)
        {
        case r3d::game::originalrace::GraphOrder::Opacity:
            return RenderStage::Opacity;
        case r3d::game::originalrace::GraphOrder::Effect:
            return RenderStage::Effect;
        case r3d::game::originalrace::GraphOrder::Last:
            return RenderStage::Last;
        case r3d::game::originalrace::GraphOrder::Default:
            return RenderStage::Opacity;
        }
        return RenderStage::Opacity;
    };
    struct DeferredVisualDraw
    {
        const Asset* asset = nullptr;
        const r3d::game::originalrace::VisualNode* node = nullptr;
        Transform model;
        float reflectionStrength = 0.0F;
        float textureDiffuseScale = 1.0F;
        float textureScaleX = 1.0F;
        r3d::game::originalrace::LightingMode lighting =
            r3d::game::originalrace::LightingMode::Standard;
        float distanceSquared = 0.0F;
        float opacity = 1.0F;
        RenderStage stage = RenderStage::Opacity;
        DrawLayer layer = DrawLayer::Transparency;
        float animationSeconds = 0.0F;
        std::array<float, 4> tint{1.0F, 1.0F, 1.0F, 1.0F};
        bool hasTint = false;
    };
    std::vector<DeferredVisualDraw> deferredVisuals;
    const auto cullingViewProjection =
        cullingCamera != nullptr ? viewProjection(*cullingCamera)
                                 : identityMatrix();
    auto drawObject = [&](const ObjectAsset& asset,
                          const std::vector<
                              r3d::game::originalrace::VisualNode>& nodes,
                          const r3d::physics::Transform& parent,
                          r3d::game::originalrace::GraphOrder graphOrder,
                          bool cullOpacityActor,
                          float opacity,
                          const std::array<float, 4>* tint,
                          float objectAnimationSeconds = -1.0F,
                          const std::array<
                              std::array<float, 4>, 4>*
                              nodeTints = nullptr,
                          float textureScaleX = 1.0F) {
        if (cullingCamera != nullptr &&
            !boundsVisible(
                objectBounds(asset, nodes, parent),
                cullingViewProjection, device.usesHomogeneousDepth()))
        {
            return false;
        }
        const float animationSeconds =
            objectAnimationSeconds >= 0.0F
                ? objectAnimationSeconds
                : elapsedSeconds;
        const float textureDiffuseScale = sourceTextureDiffuseScale(
            asset, parent, sceneLighting.lightDirection);
        const std::size_t count = std::min(asset.nodes.size(), nodes.size());
        for (std::size_t index = 0; index < count; ++index)
        {
            if (nodes[index].maximumTimeLife > 0.0F &&
                animationSeconds >= nodes[index].maximumTimeLife)
            {
                continue;
            }
            const auto sourceNodeLighting =
                nodes[index].overridesLighting
                    ? nodes[index].lighting
                    : asset.lighting;
            const bool nodeIsRefraction =
                sourceNodeLighting ==
                r3d::game::originalrace::LightingMode::Refraction;
            if (nodeIsRefraction != refractionPass)
                continue;
            const auto nodeLighting = lightingForQuality(
                sourceNodeLighting, activeLightQuality_);
            const auto nodeGraphOrder =
                nodes[index].overridesGraphOrder
                    ? nodes[index].graphOrder
                    : graphOrder;
            // Included gtEffect records are flattened into the owning
            // ObjectDefinition by the portable loader, but remained separate
            // actors without gpReflScene/gpReflWater in the source graph.
            if (reflectionPass &&
                nodeGraphOrder ==
                    r3d::game::originalrace::GraphOrder::Effect)
            {
                continue;
            }
            const auto world = compose(
                parent,
                sourceAnimatedNodeTransform(
                    nodes[index], animationSeconds));
            auto model = transform(world);
            if (nodes[index].billboard)
            {
                const float turnAngle =
                    2.0F * std::acos(std::clamp(
                        world.rotation.w, -1.0F, 1.0F));
                const auto fixed = rotateX(world.rotation);
                model = billboardTransform(
                    world.position, world.scale, cameraPosition_,
                    turnAngle,
                    nodes[index].fixedDirection ? &fixed : nullptr);
            }
            const float reflectionStrength =
                nodeLighting == r3d::game::originalrace::
                                    LightingMode::PlanarReflection &&
                        !reflectionPass
                    ? 0.4F
                    : 0.0F;
            const bool deferredActor =
                cullOpacityActor ||
                nodeGraphOrder !=
                    r3d::game::originalrace::GraphOrder::Default;
            const auto* nodeTint =
                nodeTints != nullptr && index < nodeTints->size()
                    ? &(*nodeTints)[index]
                    : tint;
            if (!deferredActor)
            {
                drawGroups(device, asset.nodes[index], shader, model,
                           pipeline, animationSeconds, reflectionStrength,
                           nodeLighting, DrawLayer::Opaque,
                           &nodes[index], 1.0F, nodeTint, 0.0F,
                           textureDiffuseScale, textureScaleX);
            }
            if (deferredActor ||
                std::any_of(
                    asset.nodes[index].materials.begin(),
                    asset.nodes[index].materials.end(),
                    [](const auto& material) {
                        return isBlended(material.blend);
                    }))
            {
                const float dx =
                    model.matrix[12] - cameraPosition_.x;
                const float dy =
                    model.matrix[13] - cameraPosition_.y;
                const float dz =
                    model.matrix[14] - cameraPosition_.z;
                deferredVisuals.push_back(
                    {&asset.nodes[index], &nodes[index], model,
                     reflectionStrength, textureDiffuseScale,
                     textureScaleX, nodeLighting,
                     dx * dx + dy * dy + dz * dz, opacity,
                     renderStage(nodeGraphOrder, cullOpacityActor),
                     deferredActor ? DrawLayer::All
                                   : DrawLayer::Transparency,
                     animationSeconds,
                     nodeTint != nullptr
                         ? *nodeTint
                         : std::array<float, 4>{
                               1.0F, 1.0F, 1.0F, 1.0F},
                     nodeTint != nullptr});
            }
        }
        return true;
    };
    auto unitNoise = [](std::uint32_t seed) {
        seed ^= seed >> 16U;
        seed *= 0x7feb352dU;
        seed ^= seed >> 15U;
        seed *= 0x846ca68bU;
        seed ^= seed >> 16U;
        return static_cast<float>(seed & 0x00ffffffU) /
               static_cast<float>(0x01000000U);
    };
    auto rangeVector =
        [&](const r3d::physics::Vec3& minimum,
            const r3d::physics::Vec3& maximum,
            r3d::game::originalrace::ParticleDistribution distribution,
            const std::array<std::uint32_t, 3>& frequency,
            float range) {
            range = std::clamp(range, 0.0F, 1.0F);
            if (distribution ==
                r3d::game::originalrace::ParticleDistribution::Linear)
            {
                return r3d::physics::Vec3{
                    minimum.x + (maximum.x - minimum.x) * range,
                    minimum.y + (maximum.y - minimum.y) * range,
                    minimum.z + (maximum.z - minimum.z) * range};
            }
            const std::uint32_t xFrequency =
                std::max(frequency[0], 1U);
            const std::uint32_t yFrequency =
                std::max(frequency[1], 1U);
            const std::uint32_t zFrequency =
                std::max(frequency[2], 1U);
            const std::uint64_t volume =
                static_cast<std::uint64_t>(xFrequency) * yFrequency *
                zFrequency;
            const std::uint64_t cellNumber =
                range == 1.0F
                    ? volume - 1U
                    : std::min(
                          static_cast<std::uint64_t>(volume * range),
                          volume - 1U);
            const std::uint32_t xCell = static_cast<std::uint32_t>(
                cellNumber % xFrequency);
            const std::uint32_t yCell = static_cast<std::uint32_t>(
                (cellNumber / xFrequency) % yFrequency);
            const std::uint32_t zCell = static_cast<std::uint32_t>(
                (cellNumber /
                 (static_cast<std::uint64_t>(xFrequency) * yFrequency)) %
                zFrequency);
            const r3d::physics::Vec3 step{
                xFrequency > 1U
                    ? (maximum.x - minimum.x) /
                          static_cast<float>(xFrequency - 1U)
                    : 0.0F,
                yFrequency > 1U
                    ? (maximum.y - minimum.y) /
                          static_cast<float>(yFrequency - 1U)
                    : 0.0F,
                zFrequency > 1U
                    ? (maximum.z - minimum.z) /
                          static_cast<float>(zFrequency - 1U)
                    : 0.0F};
            return r3d::physics::Vec3{
                minimum.x + step.x * static_cast<float>(xCell),
                minimum.y + step.y * static_cast<float>(yCell),
                minimum.z + step.z * static_cast<float>(zCell)};
        };
    auto rangeQuaternion =
        [&](const r3d::physics::Quat& minimum,
            const r3d::physics::Quat& maximum,
            r3d::game::originalrace::ParticleDistribution distribution,
            const std::array<std::uint32_t, 2>& frequency,
            float range) {
            range = std::clamp(range, 0.0F, 1.0F);
            if (distribution ==
                r3d::game::originalrace::ParticleDistribution::Linear)
                return sphericalMix(minimum, maximum, range);
            const std::uint32_t xFrequency =
                std::max(frequency[0], 1U);
            const std::uint32_t yFrequency =
                std::max(frequency[1], 1U);
            const std::uint64_t volume =
                static_cast<std::uint64_t>(xFrequency) * yFrequency;
            const std::uint64_t cellNumber =
                range == 1.0F
                    ? volume - 1U
                    : std::min(
                          static_cast<std::uint64_t>(volume * range),
                          volume - 1U);
            const std::uint32_t xCell = static_cast<std::uint32_t>(
                cellNumber % xFrequency);
            const std::uint32_t yCell = static_cast<std::uint32_t>(
                (cellNumber / xFrequency) % yFrequency);
            const float xStep =
                xFrequency > 1U
                    ? (maximum.x - minimum.x) /
                          static_cast<float>(xFrequency - 1U)
                    : 0.0F;
            const float yStep =
                yFrequency > 1U
                    ? (maximum.y - minimum.y) /
                          static_cast<float>(yFrequency - 1U)
                    : 0.0F;
            r3d::physics::Vec3 axis{
                minimum.x + xStep * static_cast<float>(xCell),
                minimum.y + yStep * static_cast<float>(yCell), 0.0F};
            axis.z = std::sqrt(std::max(
                1.0F - axis.x * axis.x - axis.y * axis.y, 0.0F));
            if (range > 0.5F)
                axis.z = -axis.z;
            return angleAxis(
                quaternionAngle(minimum) +
                    (quaternionAngle(maximum) -
                     quaternionAngle(minimum)) *
                        range,
                axis);
        };
    auto drawParticles =
        [&](const ObjectAsset& asset,
            const r3d::game::originalrace::ObjectDefinition& definition,
            const r3d::physics::Transform& parent, float age,
            const r3d::physics::Vec3& sourceVelocity,
            const std::vector<r3d::physics::Vec3>*
                trailOverride,
            float opacity, bool forceNoDepth,
            float emissionEndSeconds) {
            const std::size_t emitterCount = std::min(
                asset.particleTextures.size(),
                definition.particleEmitters.size());
            struct ParticleSystemInstance
            {
                r3d::physics::Transform parent;
                float age = 0.0F;
                r3d::physics::Vec3 velocity;
            };
            std::vector<std::vector<ParticleSystemInstance>>
                liveParentParticles(emitterCount);
            for (std::size_t emitterIndex = 0;
                 emitterIndex < emitterCount; ++emitterIndex)
            {
                const auto& emitter =
                    definition.particleEmitters[emitterIndex];
                const auto& textures =
                    asset.particleTextures[emitterIndex];
                const auto* nodeAssets =
                    emitterIndex < asset.particleNodes.size()
                        ? &asset.particleNodes[emitterIndex]
                        : nullptr;
                const bool nodeEmitter =
                    emitter.renderMode ==
                    r3d::game::originalrace::
                        ParticleRenderMode::Node;
                if ((!nodeEmitter &&
                     (textures.empty() || emitter.materials.empty())) ||
                    (nodeEmitter &&
                     (nodeAssets == nullptr || nodeAssets->empty() ||
                      nodeAssets->size() !=
                          emitter.nodeVisuals.size())))
                    continue;
                std::vector<ParticleSystemInstance> instances;
                if (emitter.parentEmitter >= 0)
                {
                    const auto owner = static_cast<std::size_t>(
                        emitter.parentEmitter);
                    if (owner < liveParentParticles.size())
                        instances = liveParentParticles[owner];
                }
                else
                {
                    instances.push_back(
                        {parent, age, sourceVelocity});
                }
                for (const auto& instance : instances)
                {
                const auto& parent = instance.parent;
                const float age = instance.age;
                const auto& sourceVelocity = instance.velocity;
                struct ScheduledGroup
                {
                    std::uint32_t index = 0;
                    std::uint32_t firstParticle = 0;
                    std::uint32_t particleCount = 0;
                    float birth = 0.0F;
                    float life = 0.0F;
                };
                struct LiveGroup
                {
                    float death = 0.0F;
                    std::uint32_t particleCount = 0;
                };
                std::vector<ScheduledGroup> scheduledGroups;
                std::vector<LiveGroup> liveGroups;
                // sotDist observes the newest particle in emitter-local
                // space. FxSystemSrcSpeed cancels owner motion there, so the
                // source cadence is driven by the particle's own flow speed,
                // not by the car/projectile speed.
                const auto meanSquareRange =
                    [](float minimum, float maximum) noexcept {
                        // E[x^2] for the source's uniform range. Using the
                        // RMS speed also handles symmetric spark/smoke
                        // ranges whose mean vector is zero although every
                        // emitted particle is moving.
                        return (minimum * minimum +
                                minimum * maximum +
                                maximum * maximum) / 3.0F;
                    };
                const float distanceSpeed = std::sqrt(
                    meanSquareRange(
                        emitter.velocityMinimum.x,
                        emitter.velocityMaximum.x) +
                    meanSquareRange(
                        emitter.velocityMinimum.y,
                        emitter.velocityMaximum.y) +
                    meanSquareRange(
                        emitter.velocityMinimum.z,
                        emitter.velocityMaximum.z));
                const float scheduleAge =
                    emitter.distanceTriggered
                        ? age * distanceSpeed
                        : age;
                float emitterEmissionEnd = emissionEndSeconds;
                if (emitter.emissionDuration > 0.0F)
                {
                    emitterEmissionEnd = std::min(
                        emitterEmissionEnd,
                        emitter.emissionDuration);
                }
                const float scheduleEnd =
                    emitter.distanceTriggered
                        ? emitterEmissionEnd * distanceSpeed
                        : emitterEmissionEnd;
                const float effectiveScheduleAge =
                    std::min(scheduleAge, scheduleEnd);
                const std::uint32_t sourceMaximum =
                    emitter.maximumParticles;
                std::uint32_t createdParticles = 0U;
                std::uint32_t liveParticles = 0U;
                float densityAccumulator = 0.0F;
                float nextBirth = 0.0F;
                // FxEmitter is stateful in the D3D9 graph. Replaying its
                // deterministic schedule keeps the portable renderer free
                // of pass-local state while preserving variable intervals,
                // fractional density and mnaWaitingFree capacity.
                for (std::uint32_t groupIndex = 0U;
                     groupIndex < 4096U &&
                     nextBirth <= effectiveScheduleAge + 0.0001F;
                     ++groupIndex)
                {
                    const float birth =
                        emitter.distanceTriggered
                            ? (distanceSpeed > 0.0001F
                                   ? nextBirth / distanceSpeed
                                   : 0.0F)
                            : nextBirth;
                    if (emitter.startDuration > 0.0F &&
                        birth >= emitter.startDuration)
                        break;
                    liveGroups.erase(
                        std::remove_if(
                            liveGroups.begin(), liveGroups.end(),
                            [&](const LiveGroup& group) {
                                if (group.death <= birth)
                                {
                                    liveParticles -= group.particleCount;
                                    return true;
                                }
                                return false;
                            }),
                        liveGroups.end());
                    const std::uint32_t groupSeed =
                        groupIndex * 747796405U +
                        static_cast<std::uint32_t>(emitterIndex) *
                            2891336453U;
                    const float sampledDensity =
                        emitter.densityMinimum +
                        (emitter.densityMaximum -
                         emitter.densityMinimum) *
                            unitNoise(groupSeed + 23U);
                    densityAccumulator += std::max(sampledDensity, 0.0F);
                    const auto requested =
                        static_cast<std::uint32_t>(
                            std::floor(densityAccumulator));
                    densityAccumulator -=
                        static_cast<float>(requested);
                    const std::uint32_t capacity =
                        sourceMaximum == 0U
                            ? requested
                            : std::min(
                                  requested,
                                  sourceMaximum -
                                      std::min(liveParticles,
                                               sourceMaximum));
                    if (capacity > 0U)
                    {
                        const float rangeFrame =
                            sourceMaximum > 1U
                                ? static_cast<float>(
                                      createdParticles % sourceMaximum) /
                                      static_cast<float>(sourceMaximum - 1U)
                                : unitNoise(groupSeed + 19U);
                        float activeLife =
                            emitter.lifeMinimum +
                            (emitter.lifeMaximum -
                             emitter.lifeMinimum) *
                                unitNoise(groupSeed + 17U);
                        activeLife +=
                            emitter.rangeLifeMinimum +
                            (emitter.rangeLifeMaximum -
                             emitter.rangeLifeMinimum) *
                                rangeFrame;
                        if (activeLife > 0.0F)
                        {
                            liveGroups.push_back(
                                {birth + activeLife, capacity});
                            liveParticles += capacity;
                        }
                        else
                        {
                            liveGroups.push_back(
                                {std::numeric_limits<float>::infinity(),
                                 capacity});
                            liveParticles += capacity;
                        }
                        const float particleAge =
                            std::max(age - birth, 0.0F);
                        if (activeLife <= 0.0F ||
                            particleAge <= activeLife)
                        {
                            scheduledGroups.push_back(
                                {groupIndex, createdParticles, capacity,
                                 birth, activeLife});
                        }
                        createdParticles += capacity;
                    }
                    const float interval = std::max(
                        emitter.startTimeMinimum +
                            (emitter.startTimeMaximum -
                             emitter.startTimeMinimum) *
                                unitNoise(groupSeed + 29U),
                        0.001F);
                    nextBirth += interval;
                    if (sourceMaximum > 0U &&
                        liveParticles >= sourceMaximum &&
                        std::all_of(
                            liveGroups.begin(), liveGroups.end(),
                            [](const LiveGroup& group) {
                                return !std::isfinite(group.death);
                            }))
                        break;
                }
                const auto emitterWorld = compose(
                    parent,
                    sourceAnimatedNodeTransform(emitter, age));
                r3d::physics::Vec3 inheritedSourceVelocity{};
                if (emitter.sourceSpeedBehavior)
                {
                    // Type-3 FxSystemSrcSpeed now progresses on the source
                    // effect GameObject. Its backend-facing world value is
                    // already the local source speed transformed through the
                    // owning graph, so render passes only consume it.
                    inheritedSourceVelocity = sourceVelocity;
                }
                std::vector<r3d::physics::Vec3> trailPoints;
                struct TrailStyle
                {
                    MaterialState material;
                    PipelineState pipeline;
                    Texture texture;
                };
                std::vector<TrailStyle> trailStyles;
                MaterialState trailMaterial;
                PipelineState trailPipeline;
                Texture trailTexture;
                bool trailConfigured = false;
                std::uint32_t submittedParticles = 0;
                for (const auto& scheduled : scheduledGroups)
                {
                    const float particleAge =
                        std::max(age - scheduled.birth, 0.0F);
                    const std::uint32_t groupSeed =
                        scheduled.index * 747796405U +
                        static_cast<std::uint32_t>(
                            emitterIndex) *
                            2891336453U;
                    const auto particleEmitterWorld =
                        emitter.worldCoordinates
                            ? compose(
                                  parent,
                                  sourceAnimatedNodeTransform(
                                      emitter, scheduled.birth))
                            : emitterWorld;
                    for (std::uint32_t groupParticle = 0;
                         groupParticle < scheduled.particleCount;
                         ++groupParticle, ++submittedParticles)
                    {
                        const std::uint32_t seed =
                            groupSeed + groupParticle * 2246822519U;
                        const std::uint32_t particleIndex =
                            scheduled.firstParticle + groupParticle;
                        const float rangeFrame =
                            sourceMaximum > 1U
                                ? static_cast<float>(
                                      particleIndex % sourceMaximum) /
                                      static_cast<float>(
                                          sourceMaximum - 1U)
                                : unitNoise(seed + 13U);
                        auto position = rangeVector(
                            emitter.startPositionMinimum,
                            emitter.startPositionMaximum,
                            emitter.startPositionDistribution,
                            emitter.startPositionFrequency,
                            unitNoise(seed + 31U));
                        const auto rangePosition = rangeVector(
                            emitter.rangePositionMinimum,
                            emitter.rangePositionMaximum,
                            emitter.rangePositionDistribution,
                            emitter.rangePositionFrequency, rangeFrame);
                        position.x += rangePosition.x;
                        position.y += rangePosition.y;
                        position.z += rangePosition.z;
                        const auto velocity = rangeVector(
                            emitter.velocityMinimum,
                            emitter.velocityMaximum,
                            emitter.velocityDistribution,
                            emitter.velocityFrequency,
                            unitNoise(seed + 67U));
                        auto acceleration = rangeVector(
                            emitter.accelerationMinimum,
                            emitter.accelerationMaximum,
                            emitter.accelerationDistribution,
                            emitter.accelerationFrequency,
                            unitNoise(seed + 101U));
                        auto integratedAcceleration = acceleration;
                        if (!emitter.worldCoordinates)
                        {
                            integratedAcceleration.x +=
                                emitter.gravity.x;
                            integratedAcceleration.y +=
                                emitter.gravity.y;
                            integratedAcceleration.z +=
                                emitter.gravity.z;
                        }
                        position.x +=
                            velocity.x * particleAge +
                            integratedAcceleration.x * particleAge *
                                particleAge * 0.5F;
                        position.y +=
                            velocity.y * particleAge +
                            integratedAcceleration.y * particleAge *
                                particleAge * 0.5F;
                        position.z +=
                            velocity.z * particleAge +
                            integratedAcceleration.z * particleAge *
                                particleAge * 0.5F;
                        auto scale = rangeVector(
                            emitter.startScaleMinimum,
                            emitter.startScaleMaximum,
                            emitter.startScaleDistribution,
                            emitter.startScaleFrequency,
                            unitNoise(seed + 149U));
                        const auto rangeScale = rangeVector(
                            emitter.rangeScaleMinimum,
                            emitter.rangeScaleMaximum,
                            emitter.rangeScaleDistribution,
                            emitter.rangeScaleFrequency, rangeFrame);
                        scale.x += rangeScale.x;
                        scale.y += rangeScale.y;
                        scale.z += rangeScale.z;
                        const auto scaleVelocity = rangeVector(
                            emitter.scaleVelocityMinimum,
                            emitter.scaleVelocityMaximum,
                            emitter.scaleVelocityDistribution,
                            emitter.scaleVelocityFrequency,
                            unitNoise(seed + 193U));
                        scale.x += scaleVelocity.x * particleAge;
                        scale.y += scaleVelocity.y * particleAge;
                        scale.z += scaleVelocity.z * particleAge;
                        r3d::physics::Transform particle;
                        particle.position = position;
                        particle.scale = scale;
                        particle.rotation = multiply(
                            rangeQuaternion(
                                emitter.startRotationMinimum,
                                emitter.startRotationMaximum,
                                emitter.startRotationDistribution,
                                emitter.startRotationFrequency,
                                unitNoise(seed + 211U)),
                            rangeQuaternion(
                                emitter.rangeRotationMinimum,
                                emitter.rangeRotationMaximum,
                                emitter.rangeRotationDistribution,
                                emitter.rangeRotationFrequency,
                                rangeFrame));
                        const auto rotationVelocity =
                            rangeQuaternion(
                                emitter.rotationVelocityMinimum,
                                emitter.rotationVelocityMaximum,
                                emitter.rotationVelocityDistribution,
                                emitter.rotationVelocityFrequency,
                                unitNoise(seed + 223U));
                        const r3d::physics::Vec3 localParticleVelocity{
                            velocity.x +
                                integratedAcceleration.x * particleAge,
                            velocity.y +
                                integratedAcceleration.y * particleAge,
                            velocity.z +
                                integratedAcceleration.z * particleAge};
                        auto particleVelocity = transformNormal(
                            particleEmitterWorld,
                            localParticleVelocity);
                        if (emitter.worldCoordinates)
                        {
                            particleVelocity.x +=
                                emitter.gravity.x * particleAge;
                            particleVelocity.y +=
                                emitter.gravity.y * particleAge;
                            particleVelocity.z +=
                                emitter.gravity.z * particleAge;
                        }
                        if (emitter.sourceSpeedBehavior)
                        {
                            // FxFlowEmitter adds FxSystem::srcSpeed to the
                            // particle velocity at birth.  SrcSpeed is the
                            // owning PhysX actor's full linear velocity; it
                            // only set by FxSystemSrcSpeed. It is separate
                            // from the world-coordinate birth-position
                            // reconstruction above.
                            particleVelocity.x +=
                                inheritedSourceVelocity.x;
                            particleVelocity.y +=
                                inheritedSourceVelocity.y;
                            particleVelocity.z +=
                                inheritedSourceVelocity.z;
                        }
                        if (emitter.autoRotate)
                        {
                            const auto rotationDirection =
                                emitter.worldCoordinates
                                    ? particleVelocity
                                    : localParticleVelocity;
                            particle.rotation = multiply(
                                rotationVelocity,
                                directionRotation(rotationDirection));
                        }
                        else
                        {
                            particle.rotation = multiply(
                                integrateRotation(
                                    rotationVelocity, particleAge),
                                particle.rotation);
                        }
                        auto world =
                            emitter.worldCoordinates
                                ? worldCoordinateParticle(
                                      particleEmitterWorld, particle)
                                : compose(emitterWorld, particle);
                        if (emitter.worldCoordinates)
                        {
                            // FxEmitter transforms the initial particle
                            // position at birth. Reconstruct that earlier
                            // emitter position from the current source
                            // velocity; the serialized gravity vector is
                            // already world-space and is intentionally not
                            // rotated by FxFlowEmitter.
                            world.position.x -=
                                sourceVelocity.x * particleAge;
                            world.position.y -=
                                sourceVelocity.y * particleAge;
                            world.position.z -=
                                sourceVelocity.z * particleAge;
                            world.position.x +=
                                emitter.gravity.x * particleAge *
                                particleAge * 0.5F;
                            world.position.y +=
                                emitter.gravity.y * particleAge *
                                particleAge * 0.5F;
                            world.position.z +=
                                emitter.gravity.z * particleAge *
                                particleAge * 0.5F;
                        }
                        if (emitter.sourceSpeedBehavior)
                        {
                            world.position.x +=
                                inheritedSourceVelocity.x * particleAge;
                            world.position.y +=
                                inheritedSourceVelocity.y * particleAge;
                            world.position.z +=
                                inheritedSourceVelocity.z * particleAge;
                        }
                        // FxParticleSystem::OnUpdateParticle only copies the
                        // particle's world position into its child node; the
                        // child keeps identity rotation/scale and advances
                        // from the parent's particle lifetime.
                        r3d::physics::Transform childParent;
                        childParent.position = world.position;
                        liveParentParticles[emitterIndex].push_back(
                            {childParent, particleAge,
                             particleVelocity});
                        if (nodeEmitter)
                        {
                            auto particlePipeline = pipeline;
                            if (forceNoDepth)
                                particlePipeline.writeDepth = false;
                            for (std::size_t node = 0;
                                 node < nodeAssets->size(); ++node)
                            {
                                const auto nodeWorld = compose(
                                    world,
                                    sourceAnimatedNodeTransform(
                                        emitter.nodeVisuals[node],
                                        particleAge));
                                drawGroups(
                                    device, (*nodeAssets)[node], shader,
                                    transform(nodeWorld),
                                    particlePipeline, particleAge, 0.0F,
                                    lightingForQuality(
                                        asset.lighting,
                                        activeLightQuality_),
                                    DrawLayer::All,
                                    &emitter.nodeVisuals[node], opacity);
                            }
                            continue;
                        }
                        // FxSpritesManager derives both sprite modes from
                        // the particle quaternion: dirSprite rotates the X
                        // vector, while the ordinary mode uses its angle.
                        // It also renders the particle's own scale against
                        // IdentityMatrix, independent of emitter scale.
                        auto direction = rotateX(particle.rotation);
                        const float turnAngle =
                            quaternionAngle(particle.rotation);
                        Transform model;
                        if (emitter.renderMode ==
                                r3d::game::originalrace::
                                    ParticleRenderMode::Plane ||
                            emitter.renderMode ==
                                r3d::game::originalrace::
                                    ParticleRenderMode::Node)
                        {
                            model = transform(world);
                        }
                        else
                        {
                            auto spriteScale = particle.scale;
                            if (emitter.renderMode ==
                                r3d::game::originalrace::
                                    ParticleRenderMode::PointSprite)
                            {
                                const float pointSize = std::sqrt(
                                    spriteScale.x * spriteScale.x +
                                    spriteScale.y * spriteScale.y +
                                    spriteScale.z * spriteScale.z) *
                                    pointSpriteScale_;
                                spriteScale =
                                    {pointSize, pointSize, pointSize};
                            }
                            const bool directed =
                                emitter.fixedDirection ||
                                emitter.renderMode ==
                                    r3d::game::originalrace::
                                        ParticleRenderMode::
                                            DirectionalSprite ||
                                emitter.renderMode ==
                                    r3d::game::originalrace::
                                        ParticleRenderMode::Trail;
                            model = billboardTransform(
                                world.position, spriteScale,
                                cameraPosition_, turnAngle,
                                directed ? &direction : nullptr);
                        }
                        const std::size_t materialIndex =
                            submittedParticles %
                            emitter.materials.size();
                        const std::size_t textureIndex =
                            submittedParticles % textures.size();
                        const auto& sourceMaterial =
                            emitter.materials[materialIndex];
                        float materialFrame =
                            scheduled.life > 0.0F
                                ? std::clamp(
                                      particleAge / scheduled.life,
                                      0.0F, 1.0F)
                                : 0.0F;
                        if (emitter.animationMode !=
                            r3d::game::originalrace::VisualNode::
                                AnimationMode::None)
                        {
                            materialFrame = sourceAnimationFrame(
                                emitter.animationMode,
                                emitter.animationDuration,
                                emitter.animationFrame,
                                materialFrame);
                        }
                        MaterialState material;
                        material.color = materialColor(
                            sourceMaterial, materialFrame);
                        material.color[3] *= opacity;
                        material.alphaReference =
                            sourceMaterial.alphaReference;
                        material.emissive = sourceMaterial.emissive;
                        material.specular = sourceMaterial.specular;
                        material.shininess = sourceMaterial.shininess;
                        material.ignoreFog = sourceMaterial.ignoreFog;
                        material.receivesShadow = false;
                        material.textureTransform =
                            materialTextureTransform(
                                sourceMaterial, materialFrame);
                        auto particlePipeline = pipeline;
                        if (forceNoDepth)
                            particlePipeline.writeDepth = false;
                        particlePipeline.writeDepth =
                            particlePipeline.writeDepth &&
                            sourceMaterial.writeDepth;
                        if (sourceMaterial.blend ==
                            r3d::game::originalrace::
                                MaterialBlend::Additive)
                        {
                            particlePipeline.blendMode =
                                PipelineState::BlendMode::Additive;
                            particlePipeline.writeDepth = false;
                        }
                        else if (sourceMaterial.blend ==
                                 r3d::game::originalrace::
                                     MaterialBlend::Transparency)
                        {
                            particlePipeline.blendMode =
                                PipelineState::BlendMode::Alpha;
                            particlePipeline.writeDepth = false;
                        }
                        if (opacity < 0.999F)
                        {
                            particlePipeline.blendMode =
                                PipelineState::BlendMode::Alpha;
                            particlePipeline.writeDepth = false;
                        }
                        if (emitter.renderMode ==
                            r3d::game::originalrace::
                                ParticleRenderMode::PointSprite)
                        {
                            // FxPointSpritesManager forces Z writes off for
                            // the complete point-sprite system.
                            particlePipeline.writeDepth = false;
                        }
                        particlePipeline.faceCulling =
                            PipelineState::FaceCulling::None;
                        if (emitter.renderMode ==
                            r3d::game::originalrace::
                                ParticleRenderMode::Trail)
                        {
                            trailPoints.push_back(world.position);
                            trailStyles.push_back(
                                {material, particlePipeline,
                                 textures[textureIndex]});
                            if (!trailConfigured)
                            {
                                trailMaterial = material;
                                trailPipeline = particlePipeline;
                                trailTexture = textures[textureIndex];
                                trailConfigured = true;
                            }
                            continue;
                        }
                        device.draw(
                            effectMesh_, shader,
                            textures[textureIndex], model,
                            particlePipeline, {}, material);
                    }
                }
                if (trailConfigured && trailOverride != nullptr &&
                    emitter.renderMode ==
                        r3d::game::originalrace::
                            ParticleRenderMode::Trail)
                {
                    trailPoints.clear();
                    trailStyles.clear();
                    for (const auto& point : *trailOverride)
                    {
                        if (trailPoints.empty())
                        {
                            trailPoints.push_back(point);
                            continue;
                        }
                        const auto& previous = trailPoints.back();
                        const float dx = point.x - previous.x;
                        const float dy = point.y - previous.y;
                        const float dz = point.z - previous.z;
                        if (dx * dx + dy * dy + dz * dz > 0.0001F)
                            trailPoints.push_back(point);
                    }
                    if (trailPoints.empty() ||
                        std::abs(trailPoints.back().x -
                                 emitterWorld.position.x) +
                                std::abs(trailPoints.back().y -
                                         emitterWorld.position.y) +
                                std::abs(trailPoints.back().z -
                                         emitterWorld.position.z) >
                            0.0001F)
                        trailPoints.push_back(emitterWorld.position);
                }
                else if (trailConfigured && !trailPoints.empty())
                {
                    // FxTrailManager appends the current system position to
                    // the particles ordered from oldest to newest.
                    trailPoints.push_back(emitterWorld.position);
                }
                if (trailConfigured && trailPoints.size() >= 2U)
                {
                    std::vector<StaticMeshVertex> trailVertices;
                    trailVertices.reserve(trailPoints.size() * 2U);
                    const auto fixedUp =
                        normalize(emitter.trailFixedUp);
                    auto direction = normalize(
                        {trailPoints[1].x - trailPoints[0].x,
                         trailPoints[1].y - trailPoints[0].y,
                         trailPoints[1].z - trailPoints[0].z});
                    if (std::abs(direction.x) +
                            std::abs(direction.y) +
                            std::abs(direction.z) <
                        0.0001F)
                        direction = {1.0F, 0.0F, 0.0F};
                    r3d::physics::Vec3 lastPosition{
                        trailPoints.front().x - direction.x,
                        trailPoints.front().y - direction.y,
                        trailPoints.front().z - direction.z};
                    for (std::size_t point = 0;
                         point < trailPoints.size(); ++point)
                    {
                        const auto& position = trailPoints[point];
                        auto side =
                            emitter.trailFixedUpEnabled
                                ? normalize(
                                      cross(fixedUp, direction))
                                : normalize(cross(
                                      direction,
                                      normalize(
                                          {position.x -
                                               cameraPosition_.x,
                                           position.y -
                                               cameraPosition_.y,
                                           position.z -
                                               cameraPosition_.z})));
                        if (std::abs(side.x) + std::abs(side.y) +
                                std::abs(side.z) <
                            0.0001F)
                        {
                            side = {0.0F, 1.0F, 0.0F};
                        }
                        const float width =
                            std::max(emitter.trailWidth, 0.001F);
                        const float pathTexture =
                            static_cast<float>(point % 2U);
                        trailVertices.push_back(
                            {position.x + side.x * width,
                             position.y + side.y * width,
                             position.z + side.z * width,
                             fixedUp.x, fixedUp.y, fixedUp.z,
                             pathTexture, 0.0F,
                             direction.x, direction.y, direction.z,
                             side.x, side.y, side.z});
                        trailVertices.push_back(
                            {position.x - side.x * width,
                             position.y - side.y * width,
                             position.z - side.z * width,
                             fixedUp.x, fixedUp.y, fixedUp.z,
                             pathTexture, 1.0F,
                             direction.x, direction.y, direction.z,
                             side.x, side.y, side.z});
                        const auto nextDirection = normalize(
                            {position.x - lastPosition.x,
                             position.y - lastPosition.y,
                             position.z - lastPosition.z});
                        if (std::abs(nextDirection.x) +
                                std::abs(nextDirection.y) +
                                std::abs(nextDirection.z) >=
                            0.0001F)
                        {
                            direction = nextDirection;
                        }
                        lastPosition = position;
                    }
                    Transform trailTransform;
                    trailTransform.matrix = identityMatrix();
                    static constexpr std::array<std::uint32_t, 6>
                        trailIndices{0U, 1U, 2U, 1U, 3U, 2U};
                    if (trailStyles.empty())
                    {
                        // FxTrailManager submits one strip for a wheel trail.
                        // Submitting every one of its (up to 100) segments as
                        // an individual bgfx draw made the command count grow
                        // for ten seconds on every slipping wheel and caused
                        // the severe mid-race slowdown.  Wheel-trail
                        // overrides have one source material, so preserve the
                        // same geometry in one transient triangle list.
                        std::vector<std::uint32_t> indices;
                        indices.reserve(
                            (trailPoints.size() - 1U) *
                            trailIndices.size());
                        for (std::size_t segment = 0;
                             segment + 1U < trailPoints.size(); ++segment)
                        {
                            const auto base = static_cast<std::uint32_t>(
                                segment * 2U);
                            for (const auto index : trailIndices)
                                indices.push_back(base + index);
                        }
                        device.drawTransient(
                            trailVertices.data(), trailVertices.size(),
                            indices.data(), indices.size(), shader,
                            trailTexture, trailTransform, trailPipeline,
                            trailMaterial);
                        continue;
                    }
                    for (std::size_t segment = 0;
                         segment + 1U < trailPoints.size(); ++segment)
                    {
                        const auto* style =
                            segment < trailStyles.size()
                                ? &trailStyles[segment]
                                : nullptr;
                        device.drawTransient(
                            trailVertices.data() + segment * 2U, 4U,
                            trailIndices.data(), trailIndices.size(),
                            shader,
                            style != nullptr ? style->texture
                                             : trailTexture,
                            trailTransform,
                            style != nullptr ? style->pipeline
                                             : trailPipeline,
                            style != nullptr ? style->material
                                             : trailMaterial);
                    }
                }
                }
            }
        };
    struct DeferredParticleDraw
    {
        const ObjectAsset* asset = nullptr;
        const r3d::game::originalrace::ObjectDefinition* definition =
            nullptr;
        r3d::physics::Transform parent;
        float age = 0.0F;
        r3d::physics::Vec3 sourceVelocity;
        const std::vector<r3d::physics::Vec3>* trailOverride =
            nullptr;
        float opacity = 1.0F;
        float emissionEndSeconds =
            std::numeric_limits<float>::infinity();
        RenderStage stage = RenderStage::Opacity;
        float distanceSquared = 0.0F;
    };
    std::vector<DeferredParticleDraw> deferredParticles;
    // GameMode::GoRace treats the four semaphore graph nodes as the base,
    // red, yellow and green sections.  The source writes white to every
    // inactive node and colors just the active lamp for the current stage.
    std::array<std::array<float, 4>, 4> semaphoreNodeTints{};
    semaphoreNodeTints.fill({1.0F, 1.0F, 1.0F, 1.0F});
    const auto sourceCountdownStage = std::clamp(countdownStage, 0, 4);
    const std::size_t semaphoreNode =
        sourceCountdownStage <= 1 ? 1U
                                  : sourceCountdownStage <= 3 ? 2U : 3U;
    semaphoreNodeTints[semaphoreNode] =
        sourceCountdownStage <= 1
            ? std::array<float, 4>{1.0F, 0.0F, 0.0F, 1.0F}
            : sourceCountdownStage <= 3
                  ? std::array<float, 4>{1.0F, 1.0F, 0.0F, 1.0F}
                  : std::array<float, 4>{0.0F, 1.0F, 0.0F, 1.0F};
    auto drawDefinition =
        [&](const ObjectAsset& asset,
            const r3d::game::originalrace::ObjectDefinition& definition,
            const r3d::physics::Transform& parent, float age,
            const r3d::physics::Vec3& sourceVelocity,
            const std::vector<r3d::physics::Vec3>*
                trailOverride = nullptr,
            float opacity = 1.0F,
            float emissionEndSeconds =
                std::numeric_limits<float>::infinity(),
            float textureScaleX = 1.0F) {
            // DataBase::AddToGraph gives gtEffect only gpColor. It never
            // registers effect actors in either osViewCubeMap or
            // osReflWater, so GraphManager does not render their geometry or
            // advance their stateful particle systems for reflection passes.
            // Replaying every wheel-smoke/trail emitter for six cube faces
            // multiplied the CPU work as soon as all racers began moving.
            if (reflectionPass &&
                definition.graphOrder ==
                    r3d::game::originalrace::GraphOrder::Effect)
            {
                return;
            }
            // gpCullOpacity only makes an actor a RayUser after the
            // camera-to-player cast hits it.  ActorManager renders every
            // other actor through its normal graph-order/depth pass.
            // Deferring every tagged actor unconditionally changed depth
            // ordering across the whole map and caused visible popping.
            const bool cullOpacityActor =
                !reflectionPass && definition.cullOpacity &&
                opacity < 0.999F;
            const auto* nodeTints =
                recordName(definition.record) == "semaphore"
                    ? &semaphoreNodeTints
                    : nullptr;
            const bool actorVisible = drawObject(
                asset, definition.visualNodes, parent,
                definition.graphOrder, cullOpacityActor, opacity,
                nullptr, age, nodeTints, textureScaleX);
            if (!actorVisible)
                return;
            if (!refractionPass && !reflectionPass &&
                !definition.particleEmitters.empty())
            {
                const float dx =
                    parent.position.x - cameraPosition_.x;
                const float dy =
                    parent.position.y - cameraPosition_.y;
                const float dz =
                    parent.position.z - cameraPosition_.z;
                deferredParticles.push_back(
                    {&asset, &definition, parent, age, sourceVelocity,
                     trailOverride, opacity, emissionEndSeconds,
                     renderStage(definition.graphOrder,
                                 cullOpacityActor),
                     dx * dx + dy * dy + dz * dz});
            }
        };
    for (std::size_t index = 0; index < race.trackInstances.size();
         ++index)
    {
        const auto& instance = race.trackInstances[index];
        const auto& definition =
            race.trackDefinitions.at(instance.definition);
        const float opacity =
            !reflectionPass && definition.cullOpacity &&
                    index < trackCullOpacityTimes_.size()
                ? cullOpacity(trackCullOpacityTimes_[index])
                : 1.0F;
        drawDefinition(
            tracks_.at(instance.definition), definition,
            instance.transform, elapsedSeconds,
            r3d::physics::Vec3{}, nullptr,
            opacity);
    }

    for (std::size_t index = 0; index < race.decorationInstances.size();
         ++index)
    {
        const auto& instance = race.decorationInstances[index];
        const auto& definition =
            race.decorationDefinitions.at(instance.definition);
        if (index < decorationActive.size() && !decorationActive[index])
        {
            if (instance.definition >= decorationPieces_.size())
                continue;
            const auto& pieceAssets =
                decorationPieces_[instance.definition];
            for (std::size_t piece = 0;
                 piece < definition.destructionPieces.size() &&
                 piece < pieceAssets.size(); ++piece)
            {
                const auto& pieceDefinition =
                    definition.destructionPieces[piece];
                r3d::physics::Transform parent = instance.transform;
                if (pieceDefinition.dynamic)
                {
                    const auto fragment = std::find_if(
                        decorationFragments.begin(),
                        decorationFragments.end(),
                        [index, piece](const auto& value) {
                            return value.instance == index &&
                                   value.piece == piece;
                        });
                    if (fragment == decorationFragments.end())
                        continue;
                    parent = fragment->transform;
                }
                drawObject(
                    pieceAssets[piece], pieceDefinition.visualNodes,
                    parent, definition.graphOrder, false, 1.0F,
                    nullptr);
            }
            continue;
        }
        const float opacity =
            !reflectionPass && definition.cullOpacity &&
                    index < decorationCullOpacityTimes_.size()
                ? cullOpacity(decorationCullOpacityTimes_[index])
                : 1.0F;
        drawDefinition(
            decorations_.at(instance.definition), definition,
            instance.transform, elapsedSeconds,
            r3d::physics::Vec3{}, nullptr,
            opacity);
    }

    for (std::size_t index = 0; index < race.bonuses.size(); ++index)
    {
        if (index < bonusActive.size() && !bonusActive[index])
            continue;
        // The shipped ctBonus actors have amNone and zero transform
        // velocities. They remain at their serialized map transform; the
        // former bob/spin was a portable invention and made pickup collision
        // appear detached from the visible object.
        auto bonusTransform = race.bonuses[index].transform;
        if (index < bonusScales.size() && bonusScales[index] >= 0.0F)
        {
            const float sourceScale = bonusScales[index];
            bonusTransform.scale = {
                sourceScale, sourceScale, sourceScale};
        }
        drawDefinition(
            bonuses_.at(index), race.bonuses[index].visual,
            bonusTransform, elapsedSeconds,
            r3d::physics::Vec3{});
    }

    const std::size_t racerCount =
        std::min(vehicles.size(), race.racers.size());
    auto lightPipeline = pipeline;
    lightPipeline.blendMode = PipelineState::BlendMode::Additive;
    lightPipeline.writeDepth = false;
    lightPipeline.faceCulling = PipelineState::FaceCulling::None;
    for (std::size_t racer = 0; racer < racerCount; ++racer)
    {
        if (racer < racerRuntime.size() &&
            racerRuntime[racer].IsDestroyed())
            continue;
        // HumanPlayer disables gpReflScene on the source car. AI cars remain
        // in the cube map, and every car remains eligible for gpReflWater.
        if (environmentReflectionPass && racer < racerRuntime.size() &&
            !racerRuntime[racer]
                 .GetPresentationState().reflectionScene)
            continue;
        const auto vehicleIndex = race.racers[racer].vehicle;
        if (vehicleIndex >= race.vehicles.size() ||
            racer >= vehicleBodies_.size())
            continue;
        const auto& definition =
            activeVehicleDefinition(race, racerRuntime, racer);
        const auto& state = vehicles[racer];
        const auto& sourceColor = racer < racerRuntime.size()
            ? racerRuntime[racer].GetPresentationState().color
            : race.racers[racer].color;
        std::array<std::array<float, 4>, 4> sourceNodeColors{};
        for (auto& color : sourceNodeColors)
            color = {1.0F, 1.0F, 1.0F, 1.0F};
        sourceNodeColors.front() = sourceColor;
        const auto* sourceNodeColorOverride =
            racer < racerRuntime.size() &&
                    racerRuntime[racer]
                        .GetPresentationState().colorMaterialAttached
                ? &sourceNodeColors
                : nullptr;
        // Player::ApplyColorMat clones and colors only the first IVBMeshNode
        // material of the root car actor. Included track/cushion actors and
        // any later body node retain their source material unchanged.
        if (!drawObject(
                vehicleBodies_[racer], definition.bodyVisuals, state.body,
                r3d::game::originalrace::GraphOrder::Default,
                false, 1.0F, nullptr, -1.0F,
                sourceNodeColorOverride))
        {
            continue;
        }
        if (!refractionPass &&
            racer < vehicleTrackVisuals_.size() &&
            racer < racerRuntime.size())
        {
            const auto& animatedAsset =
                vehicleTrackVisuals_[racer];
            const auto count = std::min(
                animatedAsset.nodes.size(),
                definition.trackVisuals.size());
            const float sourceTextureOffset =
                racerRuntime[racer].gameCar.GetTrackTextureOffset();
            for (std::size_t index = 0; index < count; ++index)
            {
                const auto& node = definition.trackVisuals[index];
                drawGroups(
                    device, animatedAsset.nodes[index], shader,
                    transform(compose(state.body, node.transform)),
                    pipeline, elapsedSeconds, 0.0F,
                    lightingForQuality(animatedAsset.lighting,
                                       activeLightQuality_),
                    DrawLayer::All, &node,
                    1.0F, nullptr,
                    sourceTextureOffset);
            }
        }
        if (!refractionPass &&
            racer < vehicleCushionVisuals_.size() &&
            racer < racerRuntime.size())
        {
            const auto& animatedAsset =
                vehicleCushionVisuals_[racer];
            const auto count = std::min(
                animatedAsset.nodes.size(),
                definition.cushionVisuals.size());
            for (std::size_t index = 0; index < count; ++index)
            {
                const float halfAngle =
                    racerRuntime[racer].gameCar
                        .GetCushionAngle(index) * 0.5F;
                const auto& node = definition.cushionVisuals[index];
                const auto center =
                    meshGroupCenter(animatedAsset.nodes[index]);
                r3d::physics::Transform toCenter;
                toCenter.position = center;
                r3d::physics::Transform rotation;
                rotation.rotation = {
                    std::sin(halfAngle), 0.0F, 0.0F,
                    std::cos(halfAngle)};
                r3d::physics::Transform fromCenter;
                fromCenter.position = {
                    -center.x, -center.y, -center.z};
                const auto local = compose(
                    node.transform,
                    compose(toCenter,
                            compose(rotation, fromCenter)));
                drawGroups(
                    device, animatedAsset.nodes[index], shader,
                    transform(compose(state.body, local)), pipeline,
                    elapsedSeconds, 0.0F,
                    lightingForQuality(animatedAsset.lighting,
                                       activeLightQuality_),
                    DrawLayer::All, &node);
            }
        }
        if (racer < racerRuntime.size())
        {
            const auto& runtime = racerRuntime[racer];
            const std::size_t firstVisibleSlot =
                static_cast<std::size_t>(
                    r3d::game::originalrace::GarageSlotType::Hyper);
            for (std::size_t physicalSlot = firstVisibleSlot;
                 physicalSlot < definition.slotMounts.size();
                 ++physicalSlot)
            {
                const std::size_t primarySlot =
                    physicalSlot >= static_cast<std::size_t>(
                                        r3d::game::originalrace::
                                            GarageSlotType::Weapon1)
                        ? physicalSlot - static_cast<std::size_t>(
                                             r3d::game::originalrace::
                                                 GarageSlotType::Weapon1)
                        : runtime.weaponSlots.size();
                const auto weaponIndex =
                    physicalSlot == static_cast<std::size_t>(
                                        r3d::game::originalrace::
                                            GarageSlotType::Hyper)
                        ? runtime.hyperWeapon
                    : physicalSlot == static_cast<std::size_t>(
                                          r3d::game::originalrace::
                                              GarageSlotType::Mine)
                        ? runtime.mineWeapon
                    : primarySlot < runtime.weaponSlots.size()
                        ? runtime.weaponSlots[primarySlot]
                        : r3d::game::originalrace::
                              RacerRuntime::invalidWeapon;
                const auto& mount = definition.slotMounts[physicalSlot];
                if (refractionPass || !mount.active || !mount.show ||
                    weaponIndex == r3d::game::originalrace::
                                       RacerRuntime::invalidWeapon ||
                    weaponIndex >= race.weapons.size() ||
                    weaponIndex >= weapons_.size() ||
                    weapons_[weaponIndex].nodes.empty())
                    continue;
                r3d::physics::Transform local;
                const auto wanted =
                    recordName(race.weapons[weaponIndex].record);
                const r3d::game::originalrace::source::Weapon*
                    mountedWeapon = nullptr;
                const auto* installed = runtime.GetSlotInst(
                    static_cast<r3d::game::originalrace::source::
                                    PlayerSlotType>(physicalSlot));
                if (installed != nullptr &&
                    recordName(installed->GetItem().GetRecord()) == wanted)
                {
                    const auto& position = installed->GetItem().GetPos();
                    const auto& rotation = installed->GetItem().GetRot();
                    if (const auto* item =
                            installed->GetItem().IsWeaponItem())
                        mountedWeapon = item->GetWeapon();
                    local.position = {
                        position[0], position[1], position[2]};
                    local.rotation = {
                        rotation[0], rotation[1], rotation[2],
                        rotation[3]};
                }
                else
                {
                    local.position = mount.position;
                    const auto placement = std::find_if(
                        mount.placements.begin(), mount.placements.end(),
                        [&](const auto& item) {
                            return recordName(item.record) == wanted;
                        });
                    if (placement != mount.placements.end())
                    {
                        local.position.x += placement->offset.x;
                        local.position.y += placement->offset.y;
                        local.position.z += placement->offset.z;
                        local.rotation = placement->rotation;
                    }
                }
                const auto& weaponNode =
                    race.weapons[weaponIndex].visual;
                auto weaponTransform = weaponNode.transform;
                if (mountedWeapon != nullptr)
                {
                    const auto rotation =
                        mountedWeapon->GetDrobilkaRotation();
                    const r3d::physics::Quat sourceSpin{
                        rotation.x, rotation.y,
                        rotation.z, rotation.w};
                    weaponTransform.rotation = multiply(
                        sourceSpin, weaponTransform.rotation);
                }
                drawGroups(
                    device, weapons_[weaponIndex].nodes.front(), shader,
                    transform(compose(
                        state.body,
                        compose(local, weaponTransform))),
                    pipeline, elapsedSeconds, 0.0F,
                    r3d::game::originalrace::LightingMode::Standard,
                    DrawLayer::All, &weaponNode);
            }
        }
        // Player::_nightFlare is created only while headlights are enabled
        // and CreateNightLights clears/detaches it in ReleaseCar. Its
        // GraphDesc has neither gpReflScene nor gpReflWater, so it must not
        // be submitted into cube/water reflection or refraction passes.
        const auto* presentation = racer < racerRuntime.size()
            ? &racerRuntime[racer].GetPresentationState()
            : nullptr;
        const bool showNightLights = presentation != nullptr &&
            presentation->nightFlareCreated &&
            presentation->nightFlareAttached;
        if (!reflectionPass && !refractionPass && showNightLights)
        {
            for (const auto& source : presentation->nightLights)
            {
                r3d::physics::Transform local;
                local.position = {
                    source.position.x,
                    source.position.y,
                    source.position.z};
                local.scale = {
                    std::max(source.size[0] * 0.32F, 0.1F),
                    std::max(source.size[1] * 0.32F, 0.1F), 1.0F};
                const auto light = compose(state.body, local);
                MaterialState material;
                material.color =
                    source.head
                        ? std::array<float, 4>{
                              0.85F, 0.92F, 1.0F, 0.8F}
                        : std::array<float, 4>{
                              1.0F, 0.08F, 0.03F, 0.8F};
                material.emissive = 1.0F;
                material.specular = 0.0F;
                material.ignoreFog = true;
                device.draw(effectMesh_, shader, vehicleLightTexture_,
                            transform(light), lightPipeline, {},
                            material);
            }
        }
        const std::size_t wheelCount = std::min(
            {state.wheels.size(), definition.wheelVisuals.size(),
             vehicleWheels_[racer].size()});
        for (std::size_t wheelIndex = 0; wheelIndex < wheelCount;
             ++wheelIndex)
        {
            const auto& wheel = state.wheels[wheelIndex];
            const auto& wheelAsset =
                vehicleWheels_[racer][wheelIndex];
            if (!refractionPass && !wheelAsset.nodes.empty())
                drawGroups(
                    device, wheelAsset.nodes.front(), shader,
                    transform(compose(
                        wheel,
                        definition.wheelVisuals[wheelIndex].transform)),
                    pipeline, elapsedSeconds, 0.0F,
                    r3d::game::originalrace::LightingMode::Standard,
                    DrawLayer::All,
                    &definition.wheelVisuals[wheelIndex]);
            // CarWheel::OnProgress and PxWheelSlipEffect use the PhysX
            // contact point and per-wheel slip, not an aggregate vehicle
            // contact/speed approximation.  The old approximation emitted
            // four permanent strips for every moving car, compounding the
            // visual clutter around player and AI wheels.
            const auto* contact =
                wheelIndex < state.wheelContacts.size()
                    ? &state.wheelContacts[wheelIndex]
                    : nullptr;
            const auto* sourceWheel =
                racer < racerRuntime.size()
                    ? racerRuntime[racer].gameCar.GetWheel(wheelIndex)
                    : nullptr;
            const bool wheelEffectEnabled =
                sourceWheel != nullptr && sourceWheel->HasSlipEffect();
            const auto& slipResults =
                racer < racerRuntime.size()
                    ? racerRuntime[racer].gameCar
                          .GetWheelSlipResults(wheelIndex)
                    : std::vector<r3d::game::originalrace::source::
                                      WheelSlipProgress>{};
            const auto findSlip = [&](const r3d::game::originalrace::
                                           ObjectDefinition& target)
                -> const r3d::game::originalrace::source::
                    WheelSlipProgress* {
                const auto found = std::find_if(
                    slipResults.begin(), slipResults.end(),
                    [&](const auto& result) {
                        return result.definition != nullptr &&
                               result.definition->record == target.record;
                    });
                return found != slipResults.end() ? &*found : nullptr;
            };
            const auto* trailSlip = findSlip(race.wheelTrailEffect);
            const auto* smokeSlip = findSlip(race.wheelSmokeEffect);
            const bool trailSlipping =
                wheelEffectEnabled && contact != nullptr &&
                trailSlip != nullptr && trailSlip->active;
            const auto* trailPath =
                racer < wheelTrailPaths_.size() &&
                        wheelIndex < wheelTrailPaths_[racer].size()
                    ? &wheelTrailPaths_[racer][wheelIndex]
                    : nullptr;
            if (trailSlipping ||
                (trailPath != nullptr && !trailPath->empty()))
            {
                auto trailParent = wheel;
                trailParent.rotation = state.body.rotation;
                if (trailSlipping)
                {
                    trailParent.position = contact->position;
                    trailParent.position.x += trailSlip->position[0U];
                    trailParent.position.y += trailSlip->position[1U];
                    trailParent.position.z += trailSlip->position[2U];
                }
                else
                {
                    // FxSystemWaitingEnd keeps the existing particles alive
                    // after slip stops; anchor it at the final sample so a
                    // new strip is not stretched to the moving wheel.
                    trailParent.position = trailPath->back();
                }
                drawDefinition(
                    wheelTrailEffect_,
                    trailSlip != nullptr &&
                            trailSlip->definition != nullptr
                        ? *trailSlip->definition
                        : race.wheelTrailEffect,
                    trailParent, elapsedSeconds,
                    state.linearVelocity,
                    trailPath);
            }
            if (wheelEffectEnabled &&
                racer < wheelSmokeStartTimes_.size() &&
                wheelIndex < wheelSmokeStartTimes_[racer].size())
            {
                const float smokeStart =
                    wheelSmokeStartTimes_[racer][wheelIndex];
                const float smokeEnd =
                    wheelSmokeEndTimes_[racer][wheelIndex];
                if (smokeStart >= 0.0F)
                {
                    auto smokeParent = wheel;
                    smokeParent.rotation = state.body.rotation;
                    const bool hasStoredSmokePosition =
                        racer < wheelSmokePositions_.size() &&
                        wheelIndex <
                            wheelSmokePositions_[racer].size();
                    if (hasStoredSmokePosition)
                    {
                        smokeParent.position =
                            wheelSmokePositions_[racer][wheelIndex];
                    }
                    else if (trailPath != nullptr &&
                             !trailPath->empty())
                    {
                        smokeParent.position = trailPath->back();
                    }
                    drawDefinition(
                        wheelSmokeEffect_,
                        smokeSlip != nullptr &&
                                smokeSlip->definition != nullptr
                            ? *smokeSlip->definition
                            : race.wheelSmokeEffect,
                        smokeParent, elapsedSeconds - smokeStart,
                        state.linearVelocity, nullptr, 1.0F,
                        smokeEnd >= 0.0F
                            ? smokeEnd
                            : std::numeric_limits<float>::infinity());
                }
            }
        }
    }

    for (const auto& projectile : projectiles)
    {
        if (!projectile.active ||
            projectile.weapon >= race.weapons.size() ||
            projectile.weapon >= projectiles_.size() ||
            projectile.projectile >=
                race.weapons[projectile.weapon].projectiles.size() ||
            projectile.projectile >=
                projectiles_[projectile.weapon].size())
            continue;
        const auto* runtimeDefinition =
            runtimeProjectileDefinition(projectile);
        if (runtimeDefinition == nullptr)
            continue;
        const auto& definition = *runtimeDefinition;
        const auto& asset =
            projectiles_[projectile.weapon][projectile.projectile].visual;
        // ptDrobilka's primary model is created lazily by DrobilkaContact;
        // it is not a continuously visible projectile model.
        if (definition.type == 15U)
            continue;
        if (asset.nodes.empty() && asset.particleTextures.empty())
            continue;
        r3d::physics::Transform parent;
        parent.position = projectile.position;
        parent.rotation = projectile.rotation;
        const auto sourceParent = parent;
        if (projectile.attached &&
            (definition.type == 3U || definition.type == 18U))
        {
            const float distance =
                std::max(
                    projectile.impactDistance > 0.0F
                        ? projectile.impactDistance
                        : (definition.maximumDistance > 0.0F
                               ? definition.maximumDistance
                               : 100.0F),
                    0.1F);
            parent.position.x +=
                projectile.direction.x * distance * 0.5F;
            parent.position.y +=
                projectile.direction.y * distance * 0.5F;
            parent.position.z +=
                projectile.direction.z * distance * 0.5F;
            parent.scale.x = distance;
            if (definition.type == 3U)
                parent.scale.y = projectile.beamWidthScale;
        }
        drawDefinition(
            asset, definition.visual, parent,
            projectile.sourceObject->GetTimeLife(),
            projectile.velocity,
            nullptr, 1.0F,
            std::numeric_limits<float>::infinity(),
            projectile.beamTextureScale);
        if (projectile.attached)
        {
            const float distance =
                std::max(
                    projectile.impactDistance > 0.0F
                        ? projectile.impactDistance
                        : (definition.maximumDistance > 0.0F
                               ? definition.maximumDistance
                               : 100.0F),
                    0.1F);
            auto impact = sourceParent;
            impact.position.x +=
                projectile.direction.x * distance;
            impact.position.y +=
                projectile.direction.y * distance;
            impact.position.z +=
                projectile.direction.z * distance;
            const auto& projectileAssets =
                projectiles_[projectile.weapon]
                            [projectile.projectile];
            if (!definition.secondaryVisual.visualNodes.empty() ||
                !definition.secondaryVisual.particleEmitters.empty())
            {
                drawDefinition(
                    projectileAssets.secondaryVisual,
                    definition.secondaryVisual, impact,
                    projectile.sourceObject->GetTimeLife(),
                    r3d::physics::Vec3{});
            }
        }
    }

    for (const auto& mine : mines)
    {
        if (!mine.active || mine.weapon >= race.weapons.size() ||
            mine.weapon >= projectiles_.size() ||
            mine.projectile >=
                race.weapons[mine.weapon].projectiles.size() ||
            mine.projectile >= projectiles_[mine.weapon].size())
            continue;
        const auto* runtimeDefinition =
            runtimeProjectileDefinition(mine);
        if (runtimeDefinition == nullptr)
            continue;
        const auto& definition = *runtimeDefinition;
        const auto& assets =
            projectiles_[mine.weapon][mine.projectile];
        const ObjectAsset* asset = &assets.visual;
        const r3d::game::originalrace::ObjectDefinition* visual =
            &definition.visual;
        if (mine.visualVariant == 1U)
        {
            asset = &assets.secondaryVisual;
        }
        else if (mine.visualVariant == 2U)
        {
            asset = &assets.tertiaryVisual;
        }
        if (asset->nodes.empty() &&
            asset->particleTextures.empty())
            continue;
        r3d::physics::Transform parent;
        parent.position = mine.position;
        parent.rotation = mine.rotation;
        if (definition.type == 10U)
        {
            const float scale = mine.armingAlpha;
            parent.scale = {scale, scale, scale};
        }
        drawDefinition(
            *asset, *visual, parent,
            mine.sourceObject->GetTimeLife(), mine.velocity);
    }

    for (const auto& effect : effects)
    {
        const float effectEmissionEnd =
            effect.emissionEndSeconds >= 0.0F
                ? effect.emissionEndSeconds
                : std::numeric_limits<float>::infinity();
        if (effect.kind ==
            r3d::game::originalrace::RaceEventKind::ContactImpact)
        {
            r3d::physics::Transform parent;
            parent.position = effect.origin;
            drawDefinition(
                contactEffect_, race.contactEffect, parent,
                effect.ageSeconds, r3d::physics::Vec3{}, nullptr, 1.0F,
                effectEmissionEnd);
            continue;
        }
        if (effect.kind ==
                r3d::game::originalrace::RaceEventKind::
                    VehicleLowLife &&
            effect.racer < vehicles.size() &&
            effect.racer < vehicleLowLifeEffects_.size() &&
            effect.sourceDefinition != nullptr)
        {
            drawDefinition(
                vehicleLowLifeEffects_[effect.racer],
                *effect.sourceDefinition,
                compose(vehicles[effect.racer].body, effect.transform),
                effect.ageSeconds,
                vehicles[effect.racer].linearVelocity,
                nullptr, 1.0F, effectEmissionEnd);
            continue;
        }
        if (effect.kind ==
                r3d::game::originalrace::RaceEventKind::
                    VehicleEnergyDamage &&
            effect.racer < vehicles.size() &&
            effect.racer < vehicleEnergyDamageEffects_.size() &&
            effect.sourceDefinition != nullptr)
        {
            drawDefinition(
                vehicleEnergyDamageEffects_[effect.racer],
                *effect.sourceDefinition,
                compose(vehicles[effect.racer].body, effect.transform),
                effect.totalSeconds - effect.seconds,
                vehicles[effect.racer].linearVelocity,
                nullptr, 1.0F, effectEmissionEnd);
            continue;
        }
        if (effect.kind ==
                r3d::game::originalrace::RaceEventKind::
                    VehicleSlowEffect &&
            effect.racer < vehicles.size() &&
            effect.weapon < projectiles_.size() &&
            effect.projectile < projectiles_[effect.weapon].size() &&
            effect.sourceDefinition != nullptr)
        {
            drawDefinition(
                projectiles_[effect.weapon][effect.projectile]
                    .tertiaryVisual,
                *effect.sourceDefinition,
                compose(vehicles[effect.racer].body, effect.transform),
                effect.totalSeconds - effect.seconds,
                vehicles[effect.racer].linearVelocity,
                nullptr, 1.0F, effectEmissionEnd);
            continue;
        }
        if (effect.kind ==
                r3d::game::originalrace::RaceEventKind::
                    VehicleDestroyed &&
            effect.racer < race.racers.size() &&
            effect.racer < vehicleDeathEffects_.size() &&
            effect.sourceDefinition != nullptr)
        {
            const auto& vehicle = activeVehicleDefinition(
                race, racerRuntime, effect.racer);
            if (effect.vehicleEffect >= vehicle.deathEffects.size() ||
                effect.vehicleEffect >=
                    vehicleDeathEffects_[effect.racer].size())
                continue;
            const auto& definition = *effect.sourceDefinition;
            auto parent = effect.transform;
            if (definition.dynamicBody)
            {
                const auto fragment = std::find_if(
                    vehicleDeathFragments.begin(),
                    vehicleDeathFragments.end(),
                    [&](const auto& value) {
                        return value.racer == effect.racer &&
                               value.effect == effect.vehicleEffect;
                    });
                if (fragment == vehicleDeathFragments.end())
                    continue;
                parent = fragment->transform;
            }
            drawDefinition(
                vehicleDeathEffects_[effect.racer]
                                    [effect.vehicleEffect],
                definition, parent,
                effect.totalSeconds - effect.seconds,
                effect.sourceVelocity, nullptr, 1.0F,
                effectEmissionEnd);
            continue;
        }
        if (effect.kind ==
            r3d::game::originalrace::RaceEventKind::ProjectileImpact &&
            effect.bonus < race.bonuses.size() &&
            effect.bonus < bonusDeathEffects_.size() &&
            effect.sourceDefinition != nullptr)
        {
            r3d::physics::Transform parent;
            parent.position = effect.origin;
            if (!effect.ignoreRotation)
            {
                parent.rotation = directionRotation(
                    {effect.target.x - effect.origin.x,
                     effect.target.y - effect.origin.y,
                     effect.target.z - effect.origin.z});
            }
            drawDefinition(
                bonusDeathEffects_[effect.bonus],
                *effect.sourceDefinition, parent,
                effect.totalSeconds - effect.seconds,
                r3d::physics::Vec3{}, nullptr, 1.0F,
                effectEmissionEnd);
            continue;
        }
        if (effect.kind ==
            r3d::game::originalrace::RaceEventKind::WeaponShotEffect)
        {
            if (effect.weapon < weaponShotEffects_.size() &&
                effect.sourceDefinition != nullptr)
            {
                const auto& definition =
                    *effect.sourceDefinition;
                if (!definition.visualNodes.empty() ||
                    !definition.particleEmitters.empty())
                {
                    auto parent = effect.transform;
                    r3d::physics::Vec3 parentVelocity =
                        effect.sourceVelocity;
                    if (effect.parentRacer < vehicles.size())
                    {
                        parent = compose(
                            vehicles[effect.parentRacer].body,
                            effect.transform);
                        parentVelocity =
                            vehicles[effect.parentRacer].linearVelocity;
                    }
                    drawDefinition(
                        weaponShotEffects_[effect.weapon], definition,
                        parent,
                        effect.totalSeconds - effect.seconds,
                        parentVelocity, nullptr, 1.0F,
                        effectEmissionEnd);
                }
            }
            continue;
        }
        if (effect.weapon < race.weapons.size() &&
            effect.weapon < projectiles_.size() &&
            effect.projectile <
                race.weapons[effect.weapon].projectiles.size() &&
            effect.projectile <
                projectiles_[effect.weapon].size())
        {
            const auto& projectileDefinition =
                race.weapons[effect.weapon]
                    .projectiles[effect.projectile];
            const auto& projectileAssets =
                projectiles_[effect.weapon][effect.projectile];
            if (effect.kind ==
                r3d::game::originalrace::RaceEventKind::
                    ProjectileImpact)
            {
                const auto* definition = effect.sourceDefinition != nullptr
                    ? effect.sourceDefinition
                    : (effect.visualVariant == 6U
                        ? &projectileDefinition.tertiaryProjectile
                               .deathEffect.visual
                    : (effect.visualVariant == 5U
                        ? &projectileDefinition.secondaryProjectile
                               .deathEffect.visual
                    : (effect.visualVariant == 4U
                        ? &projectileDefinition.visual
                        : (effect.visualVariant == 3U
                        ? &projectileDefinition.deathEffect.visual
                        : (effect.visualVariant == 2U
                               ? &projectileDefinition.tertiaryVisual
                               : &projectileDefinition.secondaryVisual)))));
                const auto* asset =
                    effect.visualVariant == 6U
                        ? &projectileAssets.tertiaryDeathVisual
                    : (effect.visualVariant == 5U
                        ? &projectileAssets.secondaryDeathVisual
                    : (effect.visualVariant == 4U
                        ? &projectileAssets.visual
                        : (effect.visualVariant == 3U
                        ? &projectileAssets.deathVisual
                        : (effect.visualVariant == 2U
                               ? &projectileAssets.tertiaryVisual
                               : &projectileAssets.secondaryVisual))));
                r3d::physics::Transform parent;
                r3d::physics::Vec3 parentVelocity;
                if (effect.parentRacer < vehicles.size())
                {
                    parent = compose(
                        vehicles[effect.parentRacer].body,
                        effect.transform);
                    parentVelocity =
                        vehicles[effect.parentRacer].linearVelocity;
                }
                else
                {
                    parent = effect.sourceDefinitionOwner != nullptr
                        ? effect.transform
                        : r3d::physics::Transform{};
                    parent.position = effect.origin;
                    parentVelocity = effect.sourceVelocity;
                    if (!effect.ignoreRotation &&
                        effect.sourceDefinitionOwner == nullptr)
                    {
                        parent.rotation = directionRotation(
                            {effect.target.x - effect.origin.x,
                             effect.target.y - effect.origin.y,
                             effect.target.z - effect.origin.z});
                    }
                }
                drawDefinition(
                    *asset, *definition, parent,
                    effect.totalSeconds - effect.seconds,
                    parentVelocity, nullptr, 1.0F,
                    effectEmissionEnd);
                continue;
            }
            if (effect.kind ==
                    r3d::game::originalrace::RaceEventKind::
                        WeaponFired &&
                (!projectileDefinition.visual.visualNodes.empty() ||
                 !projectileDefinition.visual.particleEmitters.empty()))
            {
                continue;
            }
        }
        if (effect.kind ==
            r3d::game::originalrace::RaceEventKind::WeaponFired)
        {
            // A projectile without a serialized visual is invisible in the
            // Windows code.  Do not synthesize a beam or activation sphere.
            continue;
        }
    }

    for (std::size_t racer = 0; racer < racerCount; ++racer)
    {
        if (racer >= racerRuntime.size() ||
            racerRuntime[racer].IsDestroyed() ||
            racer >= vehicleShieldEffects_.size() ||
            racer >= vehicleShieldScales_.size())
            continue;
        const auto& runtime = racerRuntime[racer];
        if (!runtime.immortalEffect.IsEffectMaked())
            continue;
        const auto* definition =
            runtime.immortalEffect.GetEffectDefinition();
        if (definition == nullptr)
            continue;
        const auto& scaleK = runtime.immortalEffect.GetScaleK();
        const float fade = runtime.immortalEffect.GetScale();
        r3d::physics::Transform shield = vehicles[racer].body;
        shield.scale = {
            shield.scale.x * vehicleShieldScales_[racer].x *
                scaleK[0] * fade,
            shield.scale.y * vehicleShieldScales_[racer].y *
                scaleK[1] * fade,
            shield.scale.z * vehicleShieldScales_[racer].z *
                scaleK[2] * fade};
        const float damageAlpha =
            runtime.immortalEffect.GetDamageAlpha();
        const std::array<float, 4> shieldTint{
            1.0F, 1.0F, 1.0F, damageAlpha};
        drawObject(
            vehicleShieldEffects_[racer],
            definition->visualNodes, shield,
            definition->graphOrder, false, 1.0F,
            &shieldTint,
            runtime.immortalEffect.GetEffectSeconds());
    }

    if (sourceEnvironment_.RainVisible() && !vehicles.empty())
    {
        r3d::physics::Transform rainParent;
        rainParent.position = sourceEnvironment_.RainPosition();
        drawDefinition(rainEffect_, race.rainEffect, rainParent,
                       elapsedSeconds, r3d::physics::Vec3{});
    }

    // GraphManager::RenderScenes renders surfaces, material-opacity actors,
    // ray-cull actors, effect actors with Z writes disabled, and finally
    // goLast actors.  Keep those queue boundaries instead of allowing
    // particles to interleave with map geometry.
    if (deferredEnvironmentSurface)
    {
        device.draw(
            effectMesh_, shader, environmentSurfaceTexture_,
            deferredSurfaceTransform, deferredSurfacePipeline, {},
            deferredSurfaceMaterial);
    }
    const std::array stages{
        RenderStage::Opacity, RenderStage::CullOpacity,
        RenderStage::Effect, RenderStage::Last};
    std::stable_sort(
        deferredVisuals.begin(), deferredVisuals.end(),
        [](const auto& first, const auto& second) {
            if (first.stage != second.stage)
                return static_cast<int>(first.stage) <
                       static_cast<int>(second.stage);
            return first.distanceSquared > second.distanceSquared;
        });
    std::stable_sort(
        deferredParticles.begin(), deferredParticles.end(),
        [](const auto& first, const auto& second) {
            if (first.stage != second.stage)
                return static_cast<int>(first.stage) <
                       static_cast<int>(second.stage);
            return first.distanceSquared > second.distanceSquared;
        });
    for (const auto stage : stages)
    {
        auto stagePipeline = pipeline;
        const bool forceNoDepth =
            stage == RenderStage::CullOpacity ||
            stage == RenderStage::Effect;
        if (forceNoDepth)
            stagePipeline.writeDepth = false;
        for (const auto& deferred : deferredVisuals)
        {
            if (deferred.stage != stage)
                continue;
            drawGroups(device, *deferred.asset, shader,
                       deferred.model, stagePipeline,
                       deferred.animationSeconds,
                       deferred.reflectionStrength,
                       deferred.lighting, deferred.layer,
                       deferred.node, deferred.opacity,
                       deferred.hasTint ? &deferred.tint : nullptr,
                       0.0F, deferred.textureDiffuseScale,
                       deferred.textureScaleX);
        }
        for (const auto& deferred : deferredParticles)
        {
            if (deferred.stage != stage)
                continue;
            drawParticles(*deferred.asset, *deferred.definition,
                          deferred.parent, deferred.age,
                          deferred.sourceVelocity,
                          deferred.trailOverride,
                          deferred.opacity, forceNoDepth,
                          deferred.emissionEndSeconds);
        }
    }
}

void OriginalRaceRenderer::drawShadowCasters(
    GraphicsDevice& device,
    const r3d::game::originalrace::Race& race,
    const std::vector<r3d::physics::VehicleState>& vehicles,
    const PipelineState& pipeline,
    const std::vector<bool>& decorationActive,
    const std::vector<
        r3d::game::originalrace::DecorationFragmentState>&
        decorationFragments,
    const std::vector<
        r3d::game::originalrace::VehicleDeathFragmentState>&
        vehicleDeathFragments,
    const std::vector<r3d::game::originalrace::RacerRuntime>& racerRuntime,
    float elapsedSeconds) const
{
    auto shadowPipeline = pipeline;
    shadowPipeline.alphaBlend = false;
    shadowPipeline.blendMode = PipelineState::BlendMode::Opaque;
    shadowPipeline.writeColor = true;
    shadowPipeline.writeDepth = true;
    shadowPipeline.depthTest = true;

    auto drawObject =
        [&](const ObjectAsset& asset,
            const std::vector<
                r3d::game::originalrace::VisualNode>& nodes,
            const r3d::physics::Transform& parent) {
            const auto count = std::min(asset.nodes.size(), nodes.size());
            for (std::size_t index = 0; index < count; ++index)
            {
                drawShadowGroups(
                    device, asset.nodes[index], shadowShader_,
                    transform(compose(
                        parent,
                        sourceAnimatedNodeTransform(
                            nodes[index], elapsedSeconds))),
                    shadowPipeline, elapsedSeconds, &nodes[index]);
            }
        };

    for (const auto& instance : race.trackInstances)
    {
        const auto& asset = tracks_.at(instance.definition);
        if (!asset.castsShadow)
            continue;
        drawObject(asset,
                   race.trackDefinitions.at(instance.definition).visualNodes,
                   instance.transform);
    }
    for (std::size_t index = 0;
         index < race.decorationInstances.size(); ++index)
    {
        const auto& instance = race.decorationInstances[index];
        const auto& definition =
            race.decorationDefinitions.at(instance.definition);
        if (index < decorationActive.size() && !decorationActive[index])
        {
            if (!definition.castsShadow ||
                instance.definition >= decorationPieces_.size())
                continue;
            const auto& pieceAssets =
                decorationPieces_[instance.definition];
            for (std::size_t piece = 0;
                 piece < definition.destructionPieces.size() &&
                 piece < pieceAssets.size(); ++piece)
            {
                const auto& pieceDefinition =
                    definition.destructionPieces[piece];
                r3d::physics::Transform parent = instance.transform;
                if (pieceDefinition.dynamic)
                {
                    const auto fragment = std::find_if(
                        decorationFragments.begin(),
                        decorationFragments.end(),
                        [index, piece](const auto& value) {
                            return value.instance == index &&
                                   value.piece == piece;
                        });
                    if (fragment == decorationFragments.end())
                        continue;
                    parent = fragment->transform;
                }
                drawObject(pieceAssets[piece],
                           pieceDefinition.visualNodes, parent);
            }
            continue;
        }
        const auto& asset = decorations_.at(instance.definition);
        if (!asset.castsShadow)
            continue;
        drawObject(
            asset,
            definition.visualNodes,
            instance.transform);
    }

    const auto racerCount =
        std::min({vehicles.size(), race.racers.size(),
                  vehicleBodies_.size(), vehicleWheels_.size()});
    for (std::size_t racer = 0; racer < racerCount; ++racer)
    {
        if (racer < racerRuntime.size() &&
            racerRuntime[racer].IsDestroyed())
            continue;
        const auto vehicleIndex = race.racers[racer].vehicle;
        if (vehicleIndex >= race.vehicles.size())
            continue;
        const auto& definition =
            activeVehicleDefinition(race, racerRuntime, racer);
        const auto& state = vehicles[racer];
        drawObject(vehicleBodies_[racer], definition.bodyVisuals,
                   state.body);
        if (racer < vehicleTrackVisuals_.size())
        {
            const auto count = std::min(
                vehicleTrackVisuals_[racer].nodes.size(),
                definition.trackVisuals.size());
            for (std::size_t index = 0; index < count; ++index)
            {
                const auto& node = definition.trackVisuals[index];
                drawShadowGroups(
                    device, vehicleTrackVisuals_[racer].nodes[index],
                    shadowShader_,
                    transform(compose(state.body, node.transform)),
                    shadowPipeline, elapsedSeconds, &node);
            }
        }
        if (racer < vehicleCushionVisuals_.size() &&
            racer < racerRuntime.size())
        {
            const auto count = std::min(
                vehicleCushionVisuals_[racer].nodes.size(),
                definition.cushionVisuals.size());
            for (std::size_t index = 0; index < count; ++index)
            {
                const float halfAngle =
                    racerRuntime[racer].gameCar
                        .GetCushionAngle(index) * 0.5F;
                const auto& node = definition.cushionVisuals[index];
                const auto center = meshGroupCenter(
                    vehicleCushionVisuals_[racer].nodes[index]);
                r3d::physics::Transform toCenter;
                toCenter.position = center;
                r3d::physics::Transform rotation;
                rotation.rotation = {
                    std::sin(halfAngle), 0.0F, 0.0F,
                    std::cos(halfAngle)};
                r3d::physics::Transform fromCenter;
                fromCenter.position = {
                    -center.x, -center.y, -center.z};
                const auto local = compose(
                    node.transform,
                    compose(toCenter,
                            compose(rotation, fromCenter)));
                drawShadowGroups(
                    device,
                    vehicleCushionVisuals_[racer].nodes[index],
                    shadowShader_, transform(compose(state.body, local)),
                    shadowPipeline, elapsedSeconds, &node);
            }
        }
        const auto wheelCount = std::min(
            {state.wheels.size(), definition.wheelVisuals.size(),
             vehicleWheels_[racer].size()});
        for (std::size_t wheelIndex = 0; wheelIndex < wheelCount;
             ++wheelIndex)
        {
            const auto& wheel = state.wheels[wheelIndex];
            const auto& object = vehicleWheels_[racer][wheelIndex];
            if (!object.nodes.empty())
            {
                drawShadowGroups(
                    device, object.nodes.front(), shadowShader_,
                    transform(compose(
                        wheel,
                        definition.wheelVisuals[wheelIndex].transform)),
                    shadowPipeline, elapsedSeconds,
                    &definition.wheelVisuals[wheelIndex]);
            }
        }
    }
    for (const auto& fragment : vehicleDeathFragments)
    {
        if (fragment.racer >= race.racers.size() ||
            fragment.racer >= vehicleDeathEffects_.size())
            continue;
        const auto& vehicle = activeVehicleDefinition(
            race, racerRuntime, fragment.racer);
        if (fragment.effect >= vehicle.deathEffects.size() ||
            fragment.effect >=
                vehicleDeathEffects_[fragment.racer].size())
            continue;
        const auto& asset =
            vehicleDeathEffects_[fragment.racer][fragment.effect];
        if (!asset.castsShadow)
            continue;
        drawObject(
            asset, vehicle.deathEffects[fragment.effect].visual.visualNodes,
            fragment.transform);
    }
}

void OriginalRaceRenderer::renderFrame(
    GraphicsDevice& device, Shader sceneShader, const Camera& camera,
    std::uint32_t clearRgba,
    const r3d::game::originalrace::Race& race,
    const std::vector<r3d::physics::VehicleState>& vehicles,
    const PipelineState& pipeline,
    const std::vector<bool>& decorationActive,
    const std::vector<
        r3d::game::originalrace::DecorationFragmentState>&
        decorationFragments,
    const std::vector<
        r3d::game::originalrace::VehicleDeathFragmentState>&
        vehicleDeathFragments,
    const std::vector<bool>& bonusActive,
    const std::vector<float>& bonusScales,
    const std::vector<r3d::game::originalrace::RacerRuntime>& racerRuntime,
    const std::vector<r3d::game::originalrace::RaceEffect>& effects,
    const std::vector<r3d::game::originalrace::MineRuntime>& mines,
    const std::vector<
        r3d::game::originalrace::ProjectileRuntime>& projectiles,
    float elapsedSeconds,
    const r3d::game::originalrace::QualityConfig& quality,
    std::int32_t countdownStage,
    const r3d::game::originalrace::source::TraceGfx* debugTrace)
{
    if (wheelTrailUpdateSeconds_ < 0.0F ||
        elapsedSeconds < wheelTrailUpdateSeconds_)
    {
        wheelTrailPaths_.clear();
        wheelTrailTimes_.clear();
        wheelTrailResetCounts_.clear();
        wheelSmokeStartTimes_.clear();
        wheelSmokeEndTimes_.clear();
        wheelSmokePositions_.clear();
    }
    wheelTrailUpdateSeconds_ = elapsedSeconds;
    wheelTrailPaths_.resize(vehicles.size());
    wheelTrailTimes_.resize(vehicles.size());
    wheelTrailResetCounts_.resize(vehicles.size());
    wheelSmokeStartTimes_.resize(vehicles.size());
    wheelSmokeEndTimes_.resize(vehicles.size());
    wheelSmokePositions_.resize(vehicles.size());
    const auto& trailEmitters = race.wheelTrailEffect.particleEmitters;
    const float trailLife =
        trailEmitters.empty()
            ? 10.0F
            : std::max({trailEmitters.front().lifeMinimum,
                        trailEmitters.front().lifeMaximum, 0.1F});
    const auto& smokeEmitters = race.wheelSmokeEffect.particleEmitters;
    const float smokeLife =
        smokeEmitters.empty()
            ? 0.6F
            : std::max({smokeEmitters.front().lifeMinimum,
                        smokeEmitters.front().lifeMaximum, 0.1F});
    const float trailSpacing =
        trailEmitters.empty()
            ? 1.0F
            : std::max(
                  (trailEmitters.front().startTimeMinimum +
                   trailEmitters.front().startTimeMaximum) *
                      0.5F,
                  0.05F);
    const std::size_t maximumTrailPoints =
        trailEmitters.empty() ||
                trailEmitters.front().maximumParticles == 0U
            ? 100U
            : trailEmitters.front().maximumParticles;
    const std::size_t trailRacerCount =
        std::min(vehicles.size(), race.racers.size());
    for (std::size_t racer = 0; racer < trailRacerCount; ++racer)
    {
        const auto vehicleIndex = race.racers[racer].vehicle;
        if (vehicleIndex >= race.vehicles.size())
            continue;
        const auto& definition =
            activeVehicleDefinition(race, racerRuntime, racer);
        const auto& state = vehicles[racer];
        const auto wheelCount = std::min(
            {state.wheels.size(), state.wheelContacts.size(),
             definition.wheelVisualOffsets.size()});
        auto& paths = wheelTrailPaths_[racer];
        auto& times = wheelTrailTimes_[racer];
        auto& smokeStarts = wheelSmokeStartTimes_[racer];
        auto& smokeEnds = wheelSmokeEndTimes_[racer];
        auto& smokePositions = wheelSmokePositions_[racer];
        if (wheelTrailResetCounts_[racer] != state.resetCount)
        {
            paths.clear();
            times.clear();
            smokeStarts.clear();
            smokeEnds.clear();
            smokePositions.clear();
            wheelTrailResetCounts_[racer] = state.resetCount;
        }
        paths.resize(wheelCount);
        times.resize(wheelCount);
        smokeStarts.resize(wheelCount, -1.0F);
        smokeEnds.resize(wheelCount, -1.0F);
        smokePositions.resize(wheelCount);
        for (std::size_t wheel = 0; wheel < wheelCount; ++wheel)
        {
            const auto& contact = state.wheelContacts[wheel];
            auto position = contact.position;
            const auto& slipResults =
                racer < racerRuntime.size()
                    ? racerRuntime[racer].gameCar
                          .GetWheelSlipResults(wheel)
                    : std::vector<r3d::game::originalrace::source::
                                      WheelSlipProgress>{};
            const auto findSlip = [&](const r3d::game::originalrace::
                                           ObjectDefinition& target)
                -> const r3d::game::originalrace::source::
                    WheelSlipProgress* {
                const auto found = std::find_if(
                    slipResults.begin(), slipResults.end(),
                    [&](const auto& result) {
                        return result.definition != nullptr &&
                               result.definition->record == target.record;
                    });
                return found != slipResults.end() ? &*found : nullptr;
            };
            const auto* trailSlip = findSlip(race.wheelTrailEffect);
            const auto* smokeSlip = findSlip(race.wheelSmokeEffect);
            const bool trailSlipping =
                trailSlip != nullptr && trailSlip->active;
            const bool smokeSlipping =
                smokeSlip != nullptr && smokeSlip->active;
            if (trailSlip != nullptr)
            {
                position.x += trailSlip->position[0U];
                position.y += trailSlip->position[1U];
                position.z += trailSlip->position[2U];
            }
            auto& path = paths[wheel];
            auto& sampleTimes = times[wheel];
            while (!sampleTimes.empty() &&
                   elapsedSeconds - sampleTimes.front() > trailLife)
            {
                sampleTimes.erase(sampleTimes.begin());
                path.erase(path.begin());
            }
            if (smokeSlipping)
            {
                smokePositions[wheel] = contact.position;
                smokePositions[wheel].x += smokeSlip->position[0U];
                smokePositions[wheel].y += smokeSlip->position[1U];
                smokePositions[wheel].z += smokeSlip->position[2U];
                if (smokeStarts[wheel] < 0.0F)
                    smokeStarts[wheel] = elapsedSeconds;
                smokeEnds[wheel] = -1.0F;
            }
            else if (smokeStarts[wheel] >= 0.0F)
            {
                if (smokeEnds[wheel] < 0.0F)
                    smokeEnds[wheel] =
                        elapsedSeconds - smokeStarts[wheel];
                if (elapsedSeconds - smokeStarts[wheel] >
                    smokeEnds[wheel] + smokeLife)
                {
                    smokeStarts[wheel] = -1.0F;
                    smokeEnds[wheel] = -1.0F;
                }
            }
            if (!trailSlipping)
                continue;
            bool addPoint = path.empty();
            if (!path.empty())
            {
                const float dx = position.x - path.back().x;
                const float dy = position.y - path.back().y;
                const float dz = position.z - path.back().z;
                addPoint =
                    dx * dx + dy * dy + dz * dz >=
                    trailSpacing * trailSpacing;
                if (dx * dx + dy * dy + dz * dz > 100.0F)
                {
                    path.clear();
                    sampleTimes.clear();
                    addPoint = true;
                }
            }
            if (addPoint)
            {
                path.push_back(position);
                sampleTimes.push_back(elapsedSeconds);
            }
            while (path.size() > maximumTrailPoints)
            {
                path.erase(path.begin());
                sampleTimes.erase(sampleTimes.begin());
            }
        }
    }

    // CameraManager only pulls gpCullOpacity actors for the isometric
    // (orthographic) camera.  ActorManager then animates the fade over 0.25 s
    // while an actor's AABB intersects the camera-to-player ray.
    const float cullDelta =
        previousRenderSeconds_ > 0.0F &&
                elapsedSeconds >= previousRenderSeconds_
            ? elapsedSeconds - previousRenderSeconds_
            : 0.0F;
    const bool isometricCamera =
        std::abs(camera.projection[15]) > 0.5F;
    if (!sourceEnvironment_.SceneStarted())
    {
        sourceEnvironment_.StartScene(
            race.environment, isometricCamera, cameraPosition_);
    }
    sourceEnvironment_.ProcessScene(
        race.environment, isometricCamera, cameraPosition_);
    activeEnvironmentPolicy_ =
        r3d::game::originalrace::source::Environment::ApplyQuality(
            race.environment, quality, isometricCamera);
    const auto humanPosition = std::find_if(
        racerRuntime.begin(), racerRuntime.end(),
        [](const auto& player) { return player.IsHuman(); });
    const std::size_t humanRacer =
        humanPosition == racerRuntime.end()
            ? racerRuntime.size()
            : static_cast<std::size_t>(
                  humanPosition - racerRuntime.begin());
    const bool hasHumanVehicle =
        humanRacer < vehicles.size() &&
        humanRacer < race.racers.size() &&
        humanRacer < vehicleBodies_.size();
    r3d::physics::Vec3 rayTarget{};
    float rayTargetSize = 0.0F;
    if (hasHumanVehicle)
    {
        rayTarget = vehicles[humanRacer].body.position;
        const auto vehicleIndex = race.racers[humanRacer].vehicle;
        if (vehicleIndex < race.vehicles.size())
        {
            const auto& vehicle = activeVehicleDefinition(
                race, racerRuntime, humanRacer);
            const auto bounds = objectBounds(
                vehicleBodies_[humanRacer], vehicle.bodyVisuals,
                vehicles[humanRacer].body);
            if (bounds.valid)
            {
                const float x =
                    bounds.maximum.x - bounds.minimum.x;
                const float y =
                    bounds.maximum.y - bounds.minimum.y;
                const float z =
                    bounds.maximum.z - bounds.minimum.z;
                rayTargetSize = std::sqrt(x * x + y * y + z * z);
            }
            if (rayTargetSize <= 0.0001F)
            {
                const auto& half = vehicle.physics.halfExtents;
                rayTargetSize =
                    2.0F * std::sqrt(
                        half.x * half.x + half.y * half.y +
                        half.z * half.z);
            }
        }
    }
    // ActorManager::PullInRayTargetGroup projects the target onto the near
    // plane and casts only from that point to the player.  Use the actual
    // one-metre orthographic near plane instead of extending the segment
    // behind the camera to the far distance.
    const r3d::physics::Vec3 cameraToTarget{
        rayTarget.x - cameraPosition_.x,
        rayTarget.y - cameraPosition_.y,
        rayTarget.z - cameraPosition_.z};
    const float targetDepth =
        cameraToTarget.x * cameraViewDirection_.x +
        cameraToTarget.y * cameraViewDirection_.y +
        cameraToTarget.z * cameraViewDirection_.z;
    const float targetToNear = std::max(targetDepth - 1.0F, 0.0F);
    const r3d::physics::Vec3 rayStart{
        rayTarget.x - cameraViewDirection_.x * targetToNear,
        rayTarget.y - cameraViewDirection_.y * targetToNear,
        rayTarget.z - cameraViewDirection_.z * targetToNear};
    const auto opacityViewProjection = viewProjection(camera);
    auto updateCullTime = [cullDelta](float& time, bool overlap) {
        constexpr float duration = 0.25F;
        if (overlap)
            time = std::clamp(time + cullDelta, 0.0F, duration);
        else
            time = std::max(time - cullDelta, 0.0F);
    };
    trackCullOpacityTimes_.resize(race.trackInstances.size(), 0.0F);
    for (std::size_t index = 0; index < race.trackInstances.size();
         ++index)
    {
        const auto& instance = race.trackInstances[index];
        const auto& definition =
            race.trackDefinitions.at(instance.definition);
        const auto bounds = objectBounds(
            tracks_.at(instance.definition), definition.visualNodes,
            instance.transform);
        const bool overlap =
            isometricCamera && definition.cullOpacity &&
            hasHumanVehicle &&
            boundsVisible(
                bounds, opacityViewProjection,
                device.usesHomogeneousDepth()) &&
            lineIntersectsBoundsBeforeTarget(
                bounds,
                rayStart, rayTarget, rayTargetSize);
        updateCullTime(trackCullOpacityTimes_[index], overlap);
    }
    decorationCullOpacityTimes_.resize(
        race.decorationInstances.size(), 0.0F);
    for (std::size_t index = 0;
         index < race.decorationInstances.size(); ++index)
    {
        const auto& instance = race.decorationInstances[index];
        const auto& definition =
            race.decorationDefinitions.at(instance.definition);
        const bool active =
            index >= decorationActive.size() ||
            decorationActive[index];
        const auto bounds = objectBounds(
            decorations_.at(instance.definition), definition.visualNodes,
            instance.transform);
        const bool overlap =
            active && isometricCamera && definition.cullOpacity &&
            hasHumanVehicle &&
            boundsVisible(
                bounds, opacityViewProjection,
                device.usesHomogeneousDepth()) &&
            lineIntersectsBoundsBeforeTarget(
                bounds,
                rayStart, rayTarget, rayTargetSize);
        updateCullTime(
            decorationCullOpacityTimes_[index], overlap);
    }

    device.resetRenderTelemetry();
    activeEnvironmentQuality_ =
        activeEnvironmentPolicy_.environmentQuality;
    activeLightQuality_ = activeEnvironmentPolicy_.lightQuality;
    // source::Environment owns the Environment.cpp graph-option thresholds;
    // this renderer only maps the returned policy to bgfx passes.
    const bool directionalShadowsEnabled =
        activeEnvironmentPolicy_.directionalShadows;
    std::array<
        const r3d::game::originalrace::EnvironmentLamp*, 3>
        shadowLamps{};
    std::size_t shadowLampCount = 0U;
    for (const auto& lamp : race.environment.lamps)
    {
        if (!lamp.enabled || shadowLampCount >= shadowLamps.size())
            continue;
        shadowLamps[shadowLampCount++] = &lamp;
    }
    const bool spotShadowsEnabled =
        activeEnvironmentPolicy_.spotShadows;
    const bool shadowsEnabled = activeEnvironmentPolicy_.shadows;
    const bool trueReflectionsEnabled =
        activeEnvironmentPolicy_.trueReflections;
    const bool bloomEnabled = activeEnvironmentPolicy_.bloom;
    const bool refractionEnabled = activeEnvironmentPolicy_.refraction;
    const bool hdrEnabled = activeEnvironmentPolicy_.hdr;
    const bool sunShaftEnabled = activeEnvironmentPolicy_.sunShaft;
    const bool hasHighQualityWater =
        activeEnvironmentPolicy_.highQualityWater;
    const bool usesSceneDepthSurface =
        activeEnvironmentPolicy_.sceneDepthSurface;
    const bool hasReflection =
        activeEnvironmentPolicy_.environmentReflection;
    const auto reflectionCamera =
        reflectedCamera(camera, race.environment.surfaceHeight);

    r3d::physics::Vec3 renderCenter{};
    if (hasHumanVehicle)
        renderCenter = vehicles[humanRacer].body.position;
    else if (!race.tracePoints.empty())
        renderCenter = race.tracePoints.front().position;
    const auto sourceSunRay = normalize(rotate(
        race.environment.sunRotation, {1.0F, 0.0F, 0.0F}));
    r3d::physics::Vec3 sun{
        -sourceSunRay.x, -sourceSunRay.y, -sourceSunRay.z};
    const float sunLength = std::sqrt(
        sun.x * sun.x + sun.y * sun.y + sun.z * sun.z);
    if (sunLength < 0.001F)
        sun = {45.0F, 30.0F, 60.0F};
    const float shadowFarDistance = isometricCamera ? 55.0F : 60.0F;
    const float shadowSplitDistance =
        sourceShadowSplit(isometricCamera);
    const float cameraFarDistance = activeCameraFarDistance_;
    Camera lightCamera;
    Camera lightCameraFar;
    Camera lightCameraThird;
    if (directionalShadowsEnabled)
    {
        lightCamera = shadowCamera(
            device, camera, sun, 1.0F, shadowSplitDistance,
            cameraFarDistance);
        lightCameraFar = shadowCamera(
            device, camera, sun, shadowSplitDistance,
            shadowFarDistance, cameraFarDistance);
        lightCameraThird = lightCameraFar;
    }
    else if (spotShadowsEnabled)
    {
        lightCamera = spotShadowCamera(device, *shadowLamps[0]);
        lightCameraFar = shadowLampCount > 1U
                             ? spotShadowCamera(
                                   device, *shadowLamps[1])
                             : lightCamera;
        lightCameraThird = shadowLampCount > 2U
                               ? spotShadowCamera(
                                     device, *shadowLamps[2])
                               : lightCameraFar;
    }
    if (shadowsEnabled)
    {
        device.setPassState({});
        device.beginPass(RenderPass::Shadow, shadowTarget_, lightCamera,
                         0xffffffffU, true, true);
        drawShadowCasters(device, race, vehicles, pipeline,
                          decorationActive, decorationFragments,
                          vehicleDeathFragments, racerRuntime,
                          elapsedSeconds);
        if (directionalShadowsEnabled || shadowLampCount > 1U)
        {
            device.setPassState({});
            device.beginPass(
                RenderPass::ShadowFar, shadowTargetFar_, lightCameraFar,
                0xffffffffU, true, true);
            drawShadowCasters(device, race, vehicles, pipeline,
                              decorationActive, decorationFragments,
                              vehicleDeathFragments, racerRuntime,
                              elapsedSeconds);
        }
        if (spotShadowsEnabled && shadowLampCount > 2U)
        {
            device.setPassState({});
            device.beginPass(
                RenderPass::ShadowThird, shadowTargetThird_,
                lightCameraThird, 0xffffffffU, true, true);
            drawShadowCasters(device, race, vehicles, pipeline,
                              decorationActive, decorationFragments,
                              vehicleDeathFragments, racerRuntime,
                              elapsedSeconds);
        }
    }

    auto environmentCenter = renderCenter;
    environmentCenter.z += 1.0F;
    if (trueReflectionsEnabled)
    {
        for (std::size_t face = 0; face < environmentPasses.size(); ++face)
        {
            const auto faceCamera =
                environmentCamera(device, environmentCenter, face);
            RenderPassState environmentState;
            // Prevent feedback while the dynamic cube is being populated.
            // This matches InitRefl's source sky sampler before InitTrueRefl
            // replaces it for the main scene.
            environmentState.environmentTexture = skyTexture_;
            device.setPassState(environmentState);
            device.beginPass(
                environmentPasses[face],
                environmentReflectionTarget_.faces[face],
                faceCamera,
                clearRgba, true, true);
            draw(device, sceneShader, race, vehicles, pipeline,
                 decorationActive, decorationFragments,
                 vehicleDeathFragments, bonusActive, bonusScales,
                 racerRuntime, effects, mines, projectiles,
                 elapsedSeconds, countdownStage, true, true, false, true,
                 &faceCamera);
        }
    }

    if (hasReflection)
    {
        RenderPassState reflectionState;
        reflectionState.environmentTexture =
            trueReflectionsEnabled
                ? environmentReflectionTarget_.texture
                : skyTexture_;
        reflectionState.clipPlane = {
            0.0F, 0.0F, 1.0F, -race.environment.surfaceHeight};
        reflectionState.clipPlaneEnabled = true;
        reflectionState.invertCulling = true;
        device.setPassState(reflectionState);
        device.beginPass(
            RenderPass::Reflection, reflectionTarget_,
            reflectionCamera, clearRgba, true, true);
        draw(device, sceneShader, race, vehicles, pipeline,
             decorationActive, decorationFragments,
             vehicleDeathFragments, bonusActive, bonusScales,
             racerRuntime, effects, mines, projectiles,
             elapsedSeconds, countdownStage, true, false, false, false,
             &reflectionCamera);
    }

    RenderPassState sceneState;
    if (activeLightQuality_ >= 1U)
    {
        sceneState.environmentTexture =
            trueReflectionsEnabled
                ? environmentReflectionTarget_.texture
                : skyTexture_;
    }
    if (hasReflection)
    {
        sceneState.reflectionTexture =
            device.renderTargetTexture(reflectionTarget_);
        sceneState.reflectionViewProjection =
            viewProjection(reflectionCamera);
    }
    if (shadowsEnabled)
    {
        sceneState.shadowTexture =
            device.renderTargetTexture(shadowTarget_);
        sceneState.shadowTextureFar =
            device.renderTargetTexture(
                directionalShadowsEnabled || shadowLampCount > 1U
                    ? shadowTargetFar_
                    : shadowTarget_);
        sceneState.shadowViewProjection = viewProjection(lightCamera);
        sceneState.shadowViewProjectionFar =
            viewProjection(lightCameraFar);
        sceneState.shadowsEnabled = true;
        sceneState.spotShadows = spotShadowsEnabled;
        sceneState.shadowStrength =
            spotShadowsEnabled ? 1.0F : 0.62F;
        sceneState.shadowSplitDistance = shadowSplitDistance;
        sceneState.shadowMapSize = 2048.0F;
        // The D3D9 spot-light path first projected its shadow caster pass
        // into a filtered light map.  Comparing the portable depth target
        // directly needs the same receiver separation as the directional
        // path; zero bias produces the dense self-shadow grid visible on
        // garage cars and barrels.
        sceneState.shadowDepthBias = 0.0015F;
        if (spotShadowsEnabled)
        {
            // Garage/Angar never enable planar reflection, so the original
            // reflection pair is available for Environment lamp #3 without
            // adding another varying or sampler to the legacy mesh shader.
            sceneState.reflectionTexture =
                device.renderTargetTexture(
                    shadowLampCount > 2U
                        ? shadowTargetThird_
                        : shadowTargetFar_);
            sceneState.reflectionViewProjection =
                viewProjection(lightCameraThird);
        }
    }
    device.setPassState(sceneState);
    device.beginPass(
        RenderPass::Scene,
        usesSceneDepthSurface ? waterSceneTarget_ : hdrTarget_,
        camera, clearRgba, true, true);
    draw(device, sceneShader, race, vehicles, pipeline,
         decorationActive, decorationFragments,
         vehicleDeathFragments, bonusActive, bonusScales,
         racerRuntime, effects, mines, projectiles, elapsedSeconds,
         countdownStage,
         false, usesSceneDepthSurface, false, false, &camera);
    if (debugTrace != nullptr && valid(debugTraceTexture_))
    {
        // TraceGfx owns its selections, exact waypoint/path/tile/link
        // geometry and material policy. Metal only submits transient
        // triangles because D3D9 DrawPrimitiveUP is the platform boundary.
        const auto& sourcePolicy = debugTrace->GetMaterialPolicy();
        PipelineState debugPipeline = pipeline;
        debugPipeline.writeDepth = sourcePolicy.depthWrite;
        debugPipeline.depthTest = sourcePolicy.depthTest;
        debugPipeline.faceCulling = sourcePolicy.cull
                                        ? pipeline.faceCulling
                                        : PipelineState::FaceCulling::None;
        debugPipeline.blendMode = sourcePolicy.transparency
                                      ? PipelineState::BlendMode::Alpha
                                      : PipelineState::BlendMode::Opaque;
        r3d::physics::Transform identity;
        const auto records = debugTrace->BuildDrawList();
        for (const auto& record : records)
        {
            std::vector<r3d::game::originalrace::source::TraceVec3>
                sourceVertices = record.vertices;
            std::vector<std::uint32_t> sourceIndices = record.indices;
            if (record.primitive ==
                r3d::game::originalrace::source::
                    TraceGfxPrimitive::PointLink)
            {
                const auto direction = normalize({
                    record.linkStart.x - record.linkEnd.x,
                    record.linkStart.y - record.linkEnd.y,
                    record.linkStart.z - record.linkEnd.z});
                auto side = normalize(cross(direction,
                                            cameraViewDirection_));
                if (std::abs(side.x) + std::abs(side.y) +
                        std::abs(side.z) < 0.001F)
                    side = normalize(cross(direction, {0.0F, 0.0F, 1.0F}));
                side = {
                    side.x * record.linkWidth * 0.5F,
                    side.y * record.linkWidth * 0.5F,
                    side.z * record.linkWidth * 0.5F};
                sourceVertices = {
                    {record.linkStart.x + side.x,
                     record.linkStart.y + side.y,
                     record.linkStart.z + side.z},
                    {record.linkStart.x - side.x,
                     record.linkStart.y - side.y,
                     record.linkStart.z - side.z},
                    {record.linkEnd.x + side.x,
                     record.linkEnd.y + side.y,
                     record.linkEnd.z + side.z},
                    {record.linkEnd.x - side.x,
                     record.linkEnd.y - side.y,
                     record.linkEnd.z - side.z}};
                sourceIndices = {0U, 1U, 2U, 1U, 3U, 2U};
            }
            if (sourceVertices.empty() || sourceIndices.empty())
                continue;
            std::vector<StaticMeshVertex> vertices;
            vertices.reserve(sourceVertices.size());
            for (const auto& vertex : sourceVertices)
            {
                vertices.push_back({
                    vertex.x, vertex.y, vertex.z,
                    0.0F, 0.0F, 1.0F, 0.0F, 0.0F});
            }
            MaterialState debugMaterial;
            debugMaterial.color = {
                record.color.red, record.color.green,
                record.color.blue,
                std::min(record.color.alpha, sourcePolicy.alpha)};
            debugMaterial.emissive = sourcePolicy.lighting ? 0.0F : 1.0F;
            debugMaterial.ignoreFog = sourcePolicy.ignoreFog;
            debugMaterial.receivesShadow = false;
            device.drawTransient(
                vertices.data(), vertices.size(), sourceIndices.data(),
                sourceIndices.size(), sceneShader, debugTraceTexture_,
                transform(identity), debugPipeline, debugMaterial);
        }
    }

    Camera postCamera;
    postCamera.view = identityMatrix();
    postCamera.projection = identityMatrix();
    Transform postTransform;
    postTransform.matrix = identityMatrix();
    PipelineState postPipeline;
    postPipeline.writeDepth = false;
    postPipeline.depthTest = false;
    postPipeline.faceCulling = PipelineState::FaceCulling::None;
    postPipeline.multisampling = false;

    if (refractionEnabled)
    {
        const auto sceneTarget =
            usesSceneDepthSurface ? waterSceneTarget_ : hdrTarget_;
        const auto cleanSceneTarget =
            usesSceneDepthSurface ? hdrTarget_ : waterSceneTarget_;
        const auto sceneColor =
            device.renderTargetTexture(sceneTarget);

        // GraphManager copies _scRenderTex to _cleanScTex immediately after
        // RenderScenes, then renders only osColorRefr back into the original
        // scene target.  This separate texture is required by Metal as well:
        // reading and writing the same color attachment is undefined.
        device.setPassState({});
        device.beginPass(
            RenderPass::RefractionCopy, cleanSceneTarget,
            postCamera, 0x000000ffU, true, false);
        MaterialState copyMaterial;
        copyMaterial.textureFilter =
            MaterialState::TextureFilter::Point;
        device.draw(
            postProcessMesh_, copyShader_, sceneColor,
            postTransform, postPipeline, {}, copyMaterial);

        auto refractionState = sceneState;
        refractionState.reflectionTexture =
            device.renderTargetTexture(cleanSceneTarget);
        device.setPassState(refractionState);
        device.beginPass(
            RenderPass::Refraction, sceneTarget, camera,
            clearRgba, false, false);
        draw(device, refractionShader_, race, vehicles, pipeline,
             decorationActive, decorationFragments,
             vehicleDeathFragments, bonusActive, bonusScales,
             racerRuntime, effects, mines, projectiles,
             elapsedSeconds, countdownStage, false,
             usesSceneDepthSurface, true, false, &camera);
    }

    if (usesSceneDepthSurface)
    {
        const auto sourceColor =
            device.renderTargetTexture(waterSceneTarget_);
        const auto sourceDepth =
            device.renderTargetTexture(waterSceneTarget_, 1);
        const auto reflectionTexture =
            device.renderTargetTexture(reflectionTarget_);

        device.setPassState({});
        // The copied scene uses a vertex shader that writes clip-space
        // positions directly, while WaterVS needs the actual race camera
        // for its world-space plane.  A bgfx view has one view/projection
        // pair for all submissions, so using postCamera here transformed
        // the hundreds-of-metres water plane as clip-space geometry and
        // covered the frame with flashing, washed-out triangles.
        device.beginPass(RenderPass::Water, hdrTarget_, camera,
                         clearRgba, true, true);
        device.draw(postProcessMesh_, copyShader_, sourceColor,
                    postTransform, postPipeline);

        auto surfacePipeline = pipeline;
        surfacePipeline.writeDepth = false;
        surfacePipeline.depthTest = false;
        surfacePipeline.faceCulling =
            PipelineState::FaceCulling::None;
        surfacePipeline.blendMode =
            PipelineState::BlendMode::Alpha;
        MaterialState surfaceMaterial;
        surfaceMaterial.textureTransform = {
            environmentSurfaceSize_.x /
                std::max(race.environment.surfaceTileScale, 0.0001F),
            environmentSurfaceSize_.y /
                std::max(race.environment.surfaceTileScale, 0.0001F),
            0.0F, 0.0F};
        r3d::physics::Transform surface;
        surface.position = environmentSurfaceCenter_;
        surface.scale = environmentSurfaceSize_;
        if (hasHighQualityWater)
        {
            RenderPassState waterState;
            waterState.reflectionTexture = waterNormalTexture_;
            waterState.shadowTexture = reflectionTexture;
            device.setPassState(waterState);
            surfaceMaterial.color = {0.0F, 0.2F, 0.5F, 1.0F};
            surfaceMaterial.alphaReference =
                std::fmod(std::max(elapsedSeconds, 0.0F) * 0.15F,
                          1.0F);
            surfaceMaterial.emissive =
                race.environment.surfaceCloudIntensity;
            device.draw(effectMesh_, waterShader_, sourceDepth,
                        transform(surface), surfacePipeline, {},
                        surfaceMaterial);
        }
        else
        {
            RenderPassState fogState;
            fogState.reflectionTexture = sourceDepth;
            device.setPassState(fogState);
            surfaceMaterial.color = race.environment.surfaceCloudColor;
            surfaceMaterial.alphaReference = std::fmod(
                std::max(elapsedSeconds, 0.0F) *
                    race.environment.surfaceScroll,
                1.0F);
            surfaceMaterial.emissive =
                race.environment.surfaceCloudIntensity;
            device.draw(effectMesh_, fogPlaneShader_,
                        environmentSurfaceTexture_, transform(surface),
                        surfacePipeline, {}, surfaceMaterial);
        }
    }

    const auto hdrTexture = device.renderTargetTexture(hdrTarget_);
    MaterialState postMaterial;
    postMaterial.receivesShadow = false;

    if (!hdrEnabled)
    {
        if (!bloomEnabled)
        {
            device.setPassState({});
            device.beginPass(RenderPass::Composite, {}, postCamera,
                             clearRgba, false, false);
            device.draw(postProcessMesh_, copyShader_, hdrTexture,
                        postTransform, postPipeline, {}, postMaterial);
            previousRenderSeconds_ = elapsedSeconds;
            luminanceAdaptationInitialized_ = false;
            return;
        }

        const auto bloomA =
            device.renderTargetTexture(bloomTargetA_);
        const auto bloomB =
            device.renderTargetTexture(bloomTargetB_);
        device.setPassState({});
        postMaterial.postParameters = {
            race.environment.hdrLuminanceKey,
            race.environment.hdrBrightThreshold, 1.0F, 0.0F};
        device.beginPass(RenderPass::BloomExtract, bloomTargetA_,
                         postCamera, 0x000000ffU, true, false);
        device.draw(postProcessMesh_, bloomExtractShader_, hdrTexture,
                    postTransform, postPipeline, {}, postMaterial);

        postMaterial.postParameters = {
            1.0F / 128.0F, 0.0F,
            race.environment.hdrGaussianScalar, 0.0F};
        device.beginPass(RenderPass::BloomHorizontal, bloomTargetB_,
                         postCamera, 0x000000ffU, true, false);
        device.draw(postProcessMesh_, bloomBlurShader_, bloomA,
                    postTransform, postPipeline, {}, postMaterial);

        postMaterial.postParameters = {
            0.0F, 1.0F / 128.0F,
            race.environment.hdrGaussianScalar, 0.0F};
        device.beginPass(RenderPass::BloomVertical, bloomTargetA_,
                         postCamera, 0x000000ffU, true, false);
        device.draw(postProcessMesh_, bloomBlurShader_, bloomB,
                    postTransform, postPipeline, {}, postMaterial);

        RenderPassState compositeState;
        compositeState.reflectionTexture = bloomA;
        device.setPassState(compositeState);
        postMaterial.postParameters = {
            race.environment.hdrGaussianScalar,
            race.environment.hdrExposure, 0.5F, 1.0F};
        device.beginPass(RenderPass::Composite, {}, postCamera,
                         clearRgba, false, false);
        device.draw(postProcessMesh_, toneMapShader_, hdrTexture,
                    postTransform, postPipeline, {}, postMaterial);
        previousRenderSeconds_ = elapsedSeconds;
        luminanceAdaptationInitialized_ = false;
        return;
    }

    device.setPassState({});
    postMaterial.postParameters = {
        1.0F / static_cast<float>(std::max(frameWidth_, 1U)),
        1.0F / static_cast<float>(std::max(frameHeight_, 1U)),
        0.0F, 0.0F};
    device.beginPass(RenderPass::Luminance64, luminance64Target_,
                     postCamera, 0x000000ffU, true, false);
    device.draw(postProcessMesh_, luminanceLogShader_, hdrTexture,
                postTransform, postPipeline, {}, postMaterial);

    const auto luminance64 =
        device.renderTargetTexture(luminance64Target_);
    postMaterial.postParameters = {
        1.0F / 64.0F, 1.0F / 64.0F, 0.0F, 0.0F};
    device.beginPass(RenderPass::Luminance16, luminance16Target_,
                     postCamera, 0x000000ffU, true, false);
    device.draw(postProcessMesh_, luminanceDownsampleShader_,
                luminance64, postTransform, postPipeline, {},
                postMaterial);

    const auto luminance16 =
        device.renderTargetTexture(luminance16Target_);
    postMaterial.postParameters = {
        1.0F / 16.0F, 1.0F / 16.0F, 0.0F, 0.0F};
    device.beginPass(RenderPass::Luminance4, luminance4Target_,
                     postCamera, 0x000000ffU, true, false);
    device.draw(postProcessMesh_, luminanceDownsampleShader_,
                luminance16, postTransform, postPipeline, {},
                postMaterial);

    const auto luminance4 =
        device.renderTargetTexture(luminance4Target_);
    postMaterial.postParameters = {
        1.0F / 4.0F, 1.0F / 4.0F, 1.0F, 0.0F};
    device.beginPass(RenderPass::Luminance1, luminance1Target_,
                     postCamera, 0x000000ffU, true, false);
    device.draw(postProcessMesh_, luminanceDownsampleShader_,
                luminance4, postTransform, postPipeline, {},
                postMaterial);

    const float elapsedDelta =
        previousRenderSeconds_ > 0.0F &&
                elapsedSeconds >= previousRenderSeconds_
            ? elapsedSeconds - previousRenderSeconds_
            : 1.0F / 60.0F;
    previousRenderSeconds_ = elapsedSeconds;
    const auto currentLuminance =
        device.renderTargetTexture(luminance1Target_);
    const auto previousAdapted =
        luminanceAdaptationInitialized_
            ? device.renderTargetTexture(
                  adaptedLuminanceAIsCurrent_
                      ? adaptedLuminanceTargetA_
                      : adaptedLuminanceTargetB_)
            : currentLuminance;
    const auto adaptedTarget =
        adaptedLuminanceAIsCurrent_
            ? adaptedLuminanceTargetB_
            : adaptedLuminanceTargetA_;
    RenderPassState adaptationState;
    adaptationState.reflectionTexture = previousAdapted;
    device.setPassState(adaptationState);
    postMaterial.postParameters = {
        std::clamp(elapsedDelta, 0.0F, 0.1F) / 2.0F,
        0.0F, 0.0F, 0.0F};
    device.beginPass(RenderPass::LuminanceAdapt, adaptedTarget,
                     postCamera, 0x000000ffU, true, false);
    device.draw(postProcessMesh_, luminanceAdaptShader_,
                currentLuminance, postTransform, postPipeline, {},
                postMaterial);
    adaptedLuminanceAIsCurrent_ =
        !adaptedLuminanceAIsCurrent_;
    luminanceAdaptationInitialized_ = true;
    const auto adaptedLuminance =
        device.renderTargetTexture(adaptedTarget);

    const auto bloomA = device.renderTargetTexture(bloomTargetA_);
    const auto bloomB = device.renderTargetTexture(bloomTargetB_);
    RenderPassState bloomState;
    bloomState.reflectionTexture = adaptedLuminance;
    device.setPassState(bloomState);
    postMaterial.postParameters = {
        race.environment.hdrLuminanceKey,
        race.environment.hdrBrightThreshold, 0.0F, 0.0F};
    device.beginPass(RenderPass::BloomExtract, bloomTargetA_,
                     postCamera, 0x000000ffU, true, false);
    device.draw(postProcessMesh_, bloomExtractShader_, hdrTexture,
                postTransform, postPipeline, {}, postMaterial);

    postMaterial.postParameters = {
        1.0F / 128.0F, 0.0F,
        race.environment.hdrGaussianScalar, 0.0F};
    device.beginPass(RenderPass::BloomHorizontal, bloomTargetB_,
                     postCamera, 0x000000ffU, true, false);
    device.draw(postProcessMesh_, bloomBlurShader_, bloomA,
                postTransform, postPipeline, {}, postMaterial);

    postMaterial.postParameters = {
        0.0F, 1.0F / 128.0F,
        race.environment.hdrGaussianScalar, 0.0F};
    device.beginPass(RenderPass::BloomVertical, bloomTargetA_,
                     postCamera, 0x000000ffU, true, false);
    device.draw(postProcessMesh_, bloomBlurShader_, bloomB,
                postTransform, postPipeline, {}, postMaterial);

    RenderPassState compositeState;
    compositeState.reflectionTexture = bloomA;
    compositeState.shadowTexture = adaptedLuminance;
    device.setPassState(compositeState);
    postMaterial.postParameters = {
        race.environment.hdrGaussianScalar,
        race.environment.hdrExposure, 0.5F, 0.0F};
    if (!sunShaftEnabled)
    {
        device.beginPass(RenderPass::Composite, {}, postCamera, clearRgba,
                         false, false);
        device.draw(postProcessMesh_, toneMapShader_, hdrTexture,
                    postTransform, postPipeline, {}, postMaterial);
        return;
    }

    // GraphManager runs SunShaft after HDR/Bloom/ToneMapping.  Preserve the
    // tone-mapped scene because PrepareShafts and GenShafts both sample it.
    device.beginPass(RenderPass::ToneMap, sunShaftSceneTarget_, postCamera,
                     0x000000ffU, true, false);
    device.draw(postProcessMesh_, toneMapShader_, hdrTexture,
                postTransform, postPipeline, {}, postMaterial);
    const auto toneMappedScene =
        device.renderTargetTexture(sunShaftSceneTarget_);
    const auto sceneDepth = device.renderTargetTexture(
        usesSceneDepthSurface ? waterSceneTarget_ : hdrTarget_, 1);

    RenderPassState shaftPrepareState;
    shaftPrepareState.reflectionTexture = sceneDepth;
    device.setPassState(shaftPrepareState);
    postMaterial.textureFilter =
        MaterialState::TextureFilter::Point;
    postMaterial.reflectionTextureFilter =
        MaterialState::TextureFilter::Point;
    device.beginPass(RenderPass::SunShaftPrepare,
                     sunShaftBlurTargetA_, postCamera,
                     0x000000ffU, true, false);
    device.draw(postProcessMesh_, sunShaftPrepareShader_, toneMappedScene,
                postTransform, postPipeline, {}, postMaterial);

    // SunShaftRender creates fixed 1280/2 and 1280/4 targets and performs
    // eight alternating LINEAR resamples before the radial accumulation.
    device.setPassState({});
    postMaterial.textureFilter =
        MaterialState::TextureFilter::Linear;
    postMaterial.reflectionTextureFilter =
        MaterialState::TextureFilter::Inherited;
    auto shaftBlur = device.renderTargetTexture(sunShaftBlurTargetA_);
    for (std::uint32_t pass = 0; pass < 8U; ++pass)
    {
        const auto target =
            pass % 2U == 0U ? sunShaftBlurTargetB_
                            : sunShaftBlurTargetA_;
        device.beginPass(RenderPass::SunShaftBlur, target, postCamera,
                         0x000000ffU, false, false);
        device.draw(postProcessMesh_, copyShader_, shaftBlur,
                    postTransform, postPipeline, {}, postMaterial);
        shaftBlur = device.renderTargetTexture(target);
    }

    auto lightDirection = normalize(
        {race.environment.sunPosition.x - sceneWorldCenter_.x,
         race.environment.sunPosition.y - sceneWorldCenter_.y,
         race.environment.sunPosition.z - sceneWorldCenter_.z});
    auto radialAxis = cross(
        {0.0F, 0.0F, 1.0F}, lightDirection);
    const float radialAxisLength = std::sqrt(
        radialAxis.x * radialAxis.x + radialAxis.y * radialAxis.y +
        radialAxis.z * radialAxis.z);
    if (radialAxisLength < 0.1F)
        radialAxis = {1.0F, 0.0F, 0.0F};
    constexpr float sourceShaftAngle =
        3.14159265358979323846F / 2.5F;
    const float shaftSin = std::sin(sourceShaftAngle * 0.5F);
    auto radialRotation = normalizeQuaternion(
        {radialAxis.x * shaftSin,
         radialAxis.y * shaftSin,
         radialAxis.z * shaftSin,
         std::cos(sourceShaftAngle * 0.5F)});
    auto radialPosition = rotate(
        radialRotation, {0.0F, 0.0F, 1.0F});
    radialPosition.x *= 1000.0F;
    radialPosition.y *= 1000.0F;
    radialPosition.z *= 1000.0F;
    const auto cameraViewProjection = viewProjection(camera);
    const float clipX =
        cameraViewProjection[0] * radialPosition.x +
        cameraViewProjection[4] * radialPosition.y +
        cameraViewProjection[8] * radialPosition.z +
        cameraViewProjection[12];
    const float clipY =
        cameraViewProjection[1] * radialPosition.x +
        cameraViewProjection[5] * radialPosition.y +
        cameraViewProjection[9] * radialPosition.z +
        cameraViewProjection[13];
    const float clipW =
        cameraViewProjection[3] * radialPosition.x +
        cameraViewProjection[7] * radialPosition.y +
        cameraViewProjection[11] * radialPosition.z +
        cameraViewProjection[15];
    const float inverseClipW =
        std::abs(clipW) > 0.000001F ? 1.0F / clipW : 0.0F;

    RenderPassState shaftCompositeState;
    shaftCompositeState.reflectionTexture = shaftBlur;
    device.setPassState(shaftCompositeState);
    postMaterial.textureFilter =
        MaterialState::TextureFilter::Point;
    postMaterial.reflectionTextureFilter =
        MaterialState::TextureFilter::Linear;
    postMaterial.postParameters = {
        clipX * inverseClipW * 0.5F,
        clipY * inverseClipW * 0.5F,
        clipW > 0.0F ? 1.0F : 0.0F, 0.0F};
    device.beginPass(RenderPass::Composite, {}, postCamera, clearRgba,
                     false, false);
    device.draw(postProcessMesh_, sunShaftCompositeShader_,
                toneMappedScene, postTransform, postPipeline, {},
                postMaterial);
}

} // namespace rrr3d::race
