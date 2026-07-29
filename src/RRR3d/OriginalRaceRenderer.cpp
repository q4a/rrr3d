#include "OriginalRaceRenderer.h"

#include "OriginalMainMenu.h"
#include "resource/ResourceFileSystem.h"
#include "rrr3d_fs_bloom_blur.bin.h"
#include "rrr3d_fs_bloom_extract.bin.h"
#include "rrr3d_fs_copy.bin.h"
#include "rrr3d_fs_luminance_adapt.bin.h"
#include "rrr3d_fs_luminance_downsample.bin.h"
#include "rrr3d_fs_luminance_log.bin.h"
#include "rrr3d_fs_shadow_map.bin.h"
#include "rrr3d_fs_skybox.bin.h"
#include "rrr3d_fs_tone_map.bin.h"
#include "rrr3d_fs_water.bin.h"
#include "rrr3d_vs_post_process.bin.h"
#include "rrr3d_vs_shadow_map.bin.h"
#include "rrr3d_vs_skybox.bin.h"
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

std::vector<StaticMeshVertex> vertices(
    const r3d::resource::R3DMeshAsset& mesh)
{
    std::vector<StaticMeshVertex> result;
    result.reserve(mesh.vertices.size());
    for (const auto& vertex : mesh.vertices)
    {
        result.push_back({vertex.position[0], vertex.position[1],
                          vertex.position[2], vertex.normal[0],
                          vertex.normal[1], vertex.normal[2],
                          vertex.texcoord[0], vertex.texcoord[1],
                          vertex.tangent[0], vertex.tangent[1],
                          vertex.tangent[2], vertex.bitangent[0],
                          vertex.bitangent[1], vertex.bitangent[2]});
    }
    return result;
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

r3d::physics::Vec3 cross(const r3d::physics::Vec3& first,
                         const r3d::physics::Vec3& second) noexcept
{
    return {
        first.y * second.z - first.z * second.y,
        first.z * second.x - first.x * second.z,
        first.x * second.y - first.y * second.x};
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

r3d::physics::Quat normalizedLerp(
    r3d::physics::Quat first, r3d::physics::Quat second,
    float amount)
{
    amount = std::clamp(amount, 0.0F, 1.0F);
    const float dot = first.x * second.x + first.y * second.y +
                      first.z * second.z + first.w * second.w;
    if (dot < 0.0F)
    {
        second.x = -second.x;
        second.y = -second.y;
        second.z = -second.z;
        second.w = -second.w;
    }
    r3d::physics::Quat result{
        first.x + (second.x - first.x) * amount,
        first.y + (second.y - first.y) * amount,
        first.z + (second.z - first.z) * amount,
        first.w + (second.w - first.w) * amount};
    const float length = std::sqrt(
        result.x * result.x + result.y * result.y +
        result.z * result.z + result.w * result.w);
    if (length <= 0.0001F)
        return {};
    result.x /= length;
    result.y /= length;
    result.z /= length;
    result.w /= length;
    return result;
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
        const auto& minimum = asset.nodes[index].source.minimum;
        const auto& maximum = asset.nodes[index].source.maximum;
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

std::array<float, 4> atlasFrame(std::uint16_t columns,
                                std::uint16_t rows,
                                std::uint32_t frame)
{
    columns = std::max<std::uint16_t>(columns, 1);
    rows = std::max<std::uint16_t>(rows, 1);
    const std::uint32_t count =
        static_cast<std::uint32_t>(columns) * rows;
    frame %= count;
    const float scaleX = 1.0F / static_cast<float>(columns);
    const float scaleY = 1.0F / static_cast<float>(rows);
    return {scaleX, scaleY,
            static_cast<float>(frame % columns) * scaleX,
            static_cast<float>(frame / columns) * scaleY};
}

std::array<float, 4> animatedAtlas(std::uint16_t columns,
                                   std::uint16_t rows, float seconds,
                                   float rate = 24.0F)
{
    return atlasFrame(
        columns, rows,
        static_cast<std::uint32_t>(
            std::max(std::floor(seconds * rate), 0.0F)));
}

float visualAnimationFrame(
    const r3d::game::originalrace::VisualNode& node,
    float seconds) noexcept
{
    using Mode =
        r3d::game::originalrace::VisualNode::AnimationMode;
    if (node.animationMode == Mode::None ||
        node.animationMode == Mode::Manual ||
        node.animationMode == Mode::Inheritance)
    {
        return std::clamp(node.animationFrame, 0.0F, 1.0F);
    }
    const float normalized =
        std::max(seconds, 0.0F) /
        std::max(node.animationDuration, 0.0001F);
    switch (node.animationMode)
    {
    case Mode::Once:
        return std::clamp(normalized, 0.0F, 1.0F);
    case Mode::Repeat:
    case Mode::Tile:
        return normalized - std::floor(normalized);
    case Mode::TwoSide:
    {
        const float cycle =
            normalized - std::floor(normalized * 0.5F) * 2.0F;
        return cycle <= 1.0F ? cycle : 2.0F - cycle;
    }
    case Mode::None:
    case Mode::Manual:
    case Mode::Inheritance:
        break;
    }
    return std::clamp(node.animationFrame, 0.0F, 1.0F);
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
                const std::array<float, 4>* tint = nullptr)
{
    if (asset.textures.empty())
        return;
    const auto geometryPipeline = nodePipeline(pipeline, node);
    auto materialState =
        [&asset, elapsedSeconds, reflectionStrength, lighting, node,
         opacity, tint](
            const auto& material, std::size_t materialIndex) {
            MaterialState state;
            state.color = material.color;
            if (tint != nullptr)
            {
                state.color[0] *= (*tint)[0];
                state.color[1] *= (*tint)[1];
                state.color[2] *= (*tint)[2];
                state.color[3] *= (*tint)[3];
            }
            state.color[3] *= opacity;
            state.alphaReference = material.alphaReference;
            state.emissive = material.emissive;
            state.specular = material.specular;
            state.shininess = material.shininess;
            state.ignoreFog = material.ignoreFog;
            state.reflectionStrength = reflectionStrength;
            state.postParameters[3] =
                static_cast<float>(lighting);
            state.receivesShadow =
                !isBlended(material.blend) &&
                material.emissive < 0.999F;
            if (materialIndex < asset.normalTextures.size())
                state.normalTexture =
                    asset.normalTextures[materialIndex];
            state.textureTransform = animatedAtlas(
                material.atlasColumns, material.atlasRows, elapsedSeconds,
                material.animationRate);
            const float frame =
                node != nullptr
                    ? visualAnimationFrame(*node, elapsedSeconds)
                    : 0.0F;
            state.textureTransform[2] +=
                material.textureOffsetMinimum.x +
                (material.textureOffsetMaximum.x -
                 material.textureOffsetMinimum.x) * frame;
            state.textureTransform[3] +=
                material.textureOffsetMinimum.y +
                (material.textureOffsetMaximum.y -
                 material.textureOffsetMinimum.y) * frame;
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
    if (asset.source.materialGroups.empty())
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
            asset.source.materialGroups.size())
    {
        if (!includeMaterial(0U))
            return;
        const auto& group =
            asset.source.materialGroups[static_cast<std::size_t>(
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
    for (std::size_t index = 0; index < asset.source.materialGroups.size();
         ++index)
    {
        const auto& group = asset.source.materialGroups[index];
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
    auto stateFor = [elapsedSeconds](const auto& material) {
        MaterialState state;
        state.alphaReference = material.alphaReference;
        state.textureTransform = animatedAtlas(
            material.atlasColumns, material.atlasRows, elapsedSeconds,
            material.animationRate);
        state.receivesShadow = false;
        return state;
    };
    if (asset.source.materialGroups.empty())
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
            asset.source.materialGroups.size())
    {
        const auto& group = asset.source.materialGroups[
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
         index < asset.source.materialGroups.size(); ++index)
    {
        const auto& group = asset.source.materialGroups[index];
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
    if (!valid(hdrTarget_) || !valid(waterSceneTarget_) ||
        !valid(reflectionTarget_) || !valid(shadowTarget_) ||
        !valid(shadowTargetFar_) ||
        !valid(luminance64Target_) || !valid(luminance16Target_) ||
        !valid(luminance4Target_) || !valid(luminance1Target_) ||
        !valid(adaptedLuminanceTargetA_) ||
        !valid(adaptedLuminanceTargetB_) ||
        !valid(bloomTargetA_) || !valid(bloomTargetB_))
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
    if (valid(bloomTargetB_))
        device.destroy(bloomTargetB_);
    if (valid(bloomTargetA_))
        device.destroy(bloomTargetA_);
    if (valid(shadowTarget_))
        device.destroy(shadowTarget_);
    if (valid(shadowTargetFar_))
        device.destroy(shadowTargetFar_);
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
    bloomTargetB_ = {};
    bloomTargetA_ = {};
    shadowTarget_ = {};
    shadowTargetFar_ = {};
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
    const r3d::resource::ResourceFileSystem& resources,
    const r3d::game::originalrace::Race& race,
    std::uint32_t width, std::uint32_t height, std::string& error)
{
    try
    {
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
            !valid(luminanceLogShader_) ||
            !valid(luminanceDownsampleShader_) ||
            !valid(luminanceAdaptShader_) ||
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
                ", luminance=" +
                    std::to_string(valid(luminanceLogShader_) &&
                                   valid(luminanceDownsampleShader_) &&
                                   valid(luminanceAdaptShader_)) +
                ", postMesh=" +
                    std::to_string(valid(postProcessMesh_));
            throw r3d::resource::ResourceError(
                error);
        }
        if (!createFrameTargets(device, width, height, error))
            throw r3d::resource::ResourceError(error);
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
                asset.source = r3d::resource::loadR3DMeshAsset(
                    resources, node.meshPath);
                const auto gpuVertices = vertices(asset.source);
                asset.mesh = device.createMesh(
                    gpuVertices.data(), gpuVertices.size(),
                    asset.source.indices.data(),
                    asset.source.indices.size());
            }
            asset.materials = node.materials;
            asset.subMesh = node.subMesh;
            for (const auto& material : node.materials)
            {
                if (material.texturePath.empty())
                {
                    constexpr std::array<std::uint8_t, 4> white{
                        255, 255, 255, 255};
                    asset.textures.push_back(
                        device.createTextureRgba8(
                            1, 1, white.data(), white.size()));
                }
                else
                {
                    const auto texture =
                        resources.readBinary(material.texturePath);
                    asset.textures.push_back(
                        device.createTextureContainer(
                            texture.data(), texture.size(),
                            material.texturePath));
                }
                if (material.normalTexturePath.empty())
                {
                    asset.normalTextures.push_back({});
                }
                else
                {
                    const auto normal =
                        resources.readBinary(material.normalTexturePath);
                    asset.normalTextures.push_back(
                        device.createTextureContainer(
                            normal.data(), normal.size(),
                            material.normalTexturePath));
                }
            }
            bool normalTexturesValid =
                asset.normalTextures.size() == node.materials.size();
            for (std::size_t index = 0;
                 index < node.materials.size() && normalTexturesValid;
                 ++index)
            {
                normalTexturesValid =
                    node.materials[index].normalTexturePath.empty() ||
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
                for (std::size_t emitter = 0;
                     emitter < emitters.size(); ++emitter)
                {
                    auto& output = asset.particleTextures[emitter];
                    for (const auto& material :
                         emitters[emitter].materials)
                    {
                        if (material.texturePath.empty())
                        {
                            constexpr std::array<std::uint8_t, 4>
                                white{{255, 255, 255, 255}};
                            output.push_back(
                                device.createTextureRgba8(
                                    1, 1, white.data(),
                                    white.size()));
                        }
                        else
                        {
                            const auto texture = resources.readBinary(
                                material.texturePath);
                            output.push_back(
                                device.createTextureContainer(
                                    texture.data(), texture.size(),
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
        vehicleLowLifeEffects_.resize(race.racers.size());
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
            loadDefinition(
                vehicleLowLifeEffects_[racer],
                vehicle.lowLifeEffect);
            loadDefinition(
                vehicleShieldEffects_[racer],
                vehicle.shieldEffect);
            const r3d::physics::Transform identity;
            const auto bodyBounds = objectBounds(
                vehicleBodies_[racer], vehicle.bodyVisuals, identity);
            const auto shieldBounds = objectBounds(
                vehicleShieldEffects_[racer],
                vehicle.shieldEffect.visualNodes, identity);
            if (!bodyBounds.valid || !shieldBounds.valid)
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
            // ImmortalEffect::OnImmortalStatus fits the effect AABB around
            // the source car AABB, then applies DataBase::LoadCar's scaleK.
            vehicleShieldScales_[racer] = {
                bodySize.x / shieldSize.x * vehicle.shieldEffectScale.x,
                bodySize.y / shieldSize.y * vehicle.shieldEffectScale.y,
                bodySize.z / shieldSize.z * vehicle.shieldEffectScale.z};
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
        const auto skyBytes =
            resources.readBinary(race.environment.skyTexturePath);
        skyTexture_ = device.createTextureContainer(
            skyBytes.data(), skyBytes.size(),
            race.environment.skyTexturePath);
        if (!valid(skyMesh_) || !valid(skyTexture_))
            throw r3d::resource::ResourceError(
                "Unable to upload original sky " +
                race.environment.skyTexturePath);
        effectMesh_ = device.createMesh(
            effectVertices.data(), effectVertices.size(),
            effectIndices.data(), effectIndices.size());
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
            const auto surfaceBytes = resources.readBinary(surfacePath);
            environmentSurfaceTexture_ =
                device.createTextureContainer(
                    surfaceBytes.data(), surfaceBytes.size(), surfacePath);
            if (race.environment.surface ==
                r3d::game::originalrace::EnvironmentSurface::Water)
            {
                constexpr std::string_view normalPath =
                    "Data/Misc/water00.png";
                const auto normalImage =
                    r3d::game::mainmenu2::loadOriginalImage(
                        resources, std::string(normalPath));
                waterNormalTexture_ =
                    device.createTextureRgba8(
                        normalImage.width, normalImage.height,
                        normalImage.bytes.data(),
                        normalImage.bytes.size());
            }

            float minimumX = 0.0F;
            float maximumX = 0.0F;
            float minimumY = 0.0F;
            float maximumY = 0.0F;
            bool firstPoint = true;
            for (const auto& point : race.tracePoints)
            {
                const float radius = std::max(point.width * 0.5F, 1.0F);
                if (firstPoint)
                {
                    minimumX = point.position.x - radius;
                    maximumX = point.position.x + radius;
                    minimumY = point.position.y - radius;
                    maximumY = point.position.y + radius;
                    firstPoint = false;
                }
                else
                {
                    minimumX =
                        std::min(minimumX, point.position.x - radius);
                    maximumX =
                        std::max(maximumX, point.position.x + radius);
                    minimumY =
                        std::min(minimumY, point.position.y - radius);
                    maximumY =
                        std::max(maximumY, point.position.y + radius);
                }
            }
            environmentSurfaceCenter_ = {
                (minimumX + maximumX) * 0.5F,
                (minimumY + maximumY) * 0.5F,
                race.environment.surfaceHeight};
            environmentSurfaceSize_ = {
                std::max(maximumX - minimumX + 300.0F, 300.0F),
                std::max(maximumY - minimumY + 300.0F, 300.0F),
                1.0F};
            if (race.environment.surface ==
                r3d::game::originalrace::EnvironmentSurface::Grass)
                environmentSurfaceCenter_.x = 0.0F;
        }
        auto loadEffectTexture = [&](std::string_view path) {
            const auto bytes = resources.readBinary(path);
            return device.createTextureContainer(
                bytes.data(), bytes.size(), path);
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
             !valid(waterNormalTexture_)))
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
    destroyFrameTargets(device);
    if (valid(environmentReflectionTarget_))
        device.destroy(environmentReflectionTarget_);
    if (valid(luminanceAdaptShader_))
        device.destroy(luminanceAdaptShader_);
    if (valid(luminanceDownsampleShader_))
        device.destroy(luminanceDownsampleShader_);
    if (valid(luminanceLogShader_))
        device.destroy(luminanceLogShader_);
    if (valid(waterShader_))
        device.destroy(waterShader_);
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
    luminanceLogShader_ = {};
    luminanceDownsampleShader_ = {};
    luminanceAdaptShader_ = {};
    bloomBlurShader_ = {};
    bloomExtractShader_ = {};
    shadowShader_ = {};
    skyShader_ = {};
    environmentReflectionTarget_ = {};
    postProcessMesh_ = {};
    auto release = [&](Asset& asset) {
        for (const auto texture : asset.normalTextures)
            if (valid(texture))
                device.destroy(texture);
        for (const auto texture : asset.textures)
            if (valid(texture))
                device.destroy(texture);
        if (valid(asset.mesh))
            device.destroy(asset.mesh);
        asset = {};
    };
    auto releaseObject = [&](ObjectAsset& object) {
        for (auto& node : object.nodes)
            release(node);
        object.nodes.clear();
        for (auto& emitter : object.particleTextures)
        {
            for (const auto texture : emitter)
                if (valid(texture))
                    device.destroy(texture);
            emitter.clear();
        }
        object.particleTextures.clear();
    };
    for (auto& body : vehicleBodies_)
        releaseObject(body);
    for (auto& wheels : vehicleWheels_)
        for (auto& wheel : wheels)
            releaseObject(wheel);
    for (auto& effect : vehicleLowLifeEffects_)
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
    vehicleBodies_.clear();
    vehicleWheels_.clear();
    vehicleLowLifeEffects_.clear();
    vehicleShieldEffects_.clear();
    vehicleShieldScales_.clear();
    vehicleDeathEffects_.clear();
    bonuses_.clear();
    bonusDeathEffects_.clear();
    weapons_.clear();
    weaponShotEffects_.clear();
    projectiles_.clear();
    decorations_.clear();
    decorationPieces_.clear();
    tracks_.clear();
    if (valid(skyTexture_))
        device.destroy(skyTexture_);
    if (valid(skyMesh_))
        device.destroy(skyMesh_);
    if (valid(vehicleLightTexture_))
        device.destroy(vehicleLightTexture_);
    if (valid(environmentSurfaceTexture_))
        device.destroy(environmentSurfaceTexture_);
    if (valid(waterNormalTexture_))
        device.destroy(waterNormalTexture_);
    if (valid(effectMesh_))
        device.destroy(effectMesh_);
    skyTexture_ = {};
    skyMesh_ = {};
    vehicleLightTexture_ = {};
    environmentSurfaceTexture_ = {};
    waterNormalTexture_ = {};
    effectMesh_ = {};
    wheelTrailPaths_.clear();
    wheelTrailTimes_.clear();
    wheelTrailResetCounts_.clear();
    trackCullOpacityTimes_.clear();
    decorationCullOpacityTimes_.clear();
    wheelTrailUpdateSeconds_ = -1.0F;
    environmentSurfaceCenter_ = {};
    environmentSurfaceSize_ = {};
}

Camera OriginalRaceRenderer::makeCamera(
    const GraphicsDevice& device, const r3d::physics::VehicleState& vehicle,
    std::uint32_t width, std::uint32_t height,
    r3d::game::originalrace::PreferredCamera style,
    float cameraDistance, float seconds) noexcept
{
    const float aspect =
        static_cast<float>(std::max(width, 1U)) /
        static_cast<float>(std::max(height, 1U));
    auto carForward = rotateX(vehicle.body.rotation);
    carForward.z = 0.0F;
    const float carForwardLength = std::sqrt(
        carForward.x * carForward.x + carForward.y * carForward.y);
    if (carForwardLength > 0.0001F)
    {
        carForward.x /= carForwardLength;
        carForward.y /= carForwardLength;
    }
    auto velocityForward = carForward;
    const float velocityLength = std::sqrt(
        vehicle.linearVelocity.x * vehicle.linearVelocity.x +
        vehicle.linearVelocity.y * vehicle.linearVelocity.y);
    velocityForward.x += vehicle.linearVelocity.x * 0.1F;
    velocityForward.y += vehicle.linearVelocity.y * 0.1F;
    const float forwardLength =
        std::sqrt(velocityForward.x * velocityForward.x +
                  velocityForward.y * velocityForward.y);
    if (forwardLength > 0.0001F)
    {
        velocityForward.x /= forwardLength;
        velocityForward.y /= forwardLength;
    }
    const auto& position = vehicle.body.position;
    if (style ==
        r3d::game::originalrace::PreferredCamera::Isometric)
    {
        // CameraManager::csIsometric: Y=15.5 degrees, Z=45 degrees,
        // target distance 20 and an orthographic width of
        // 28 * GameMode::cameraDistance.
        constexpr float radians = 3.14159265358979323846F / 180.0F;
        const float elevation = 15.5F * radians;
        const float azimuth = 45.0F * radians;
        const r3d::physics::Quat rotationY{
            0.0F, std::sin(elevation * 0.5F), 0.0F,
            std::cos(elevation * 0.5F)};
        const r3d::physics::Quat rotationZ{
            0.0F, 0.0F, std::sin(azimuth * 0.5F),
            std::cos(azimuth * 0.5F)};
        const auto isoRotation = multiply(rotationZ, rotationY);
        const r3d::physics::Quat inverseIso{
            -isoRotation.x, -isoRotation.y, -isoRotation.z,
            isoRotation.w};
        const auto isoDirection =
            rotate(isoRotation, {1.0F, 0.0F, 0.0F});
        cameraViewDirection_ = isoDirection;

        // This follows CameraManager::csIsometric's projection into camera
        // space, border clipping and transform back into world space.
        auto localDirection = rotate(inverseIso, carForward);
        r3d::physics::Vec3 projected{
            localDirection.y, localDirection.z, 0.0F};
        const float projectedLength = std::sqrt(
            projected.x * projected.x + projected.y * projected.y);
        if (projectedLength > 0.0001F)
        {
            projected.x /= projectedLength;
            projected.y /= projectedLength;
        }
        const float yTargetDot = projected.y;
        const float cameraWidth =
            28.0F * std::clamp(cameraDistance, 0.6F, 2.5F);
        const float halfWidth = cameraWidth * 0.5F;
        const float halfHeight = halfWidth / aspect;
        const float cameraSize =
            std::sqrt(halfWidth * halfWidth + halfHeight * halfHeight);
        projected.x *= cameraSize;
        projected.y *= cameraSize;
        constexpr float border = 5.0F;
        if (std::abs(projected.y) > 0.1F &&
            std::abs(projected.x / projected.y) < aspect &&
            std::abs(yTargetDot) > 0.0001F)
        {
            const float y = std::clamp(
                projected.y, -border / aspect, border / aspect);
            const float radius = std::abs(y / yTargetDot);
            float x = std::sqrt(
                std::max(radius * radius - y * y, 0.0F));
            if (projected.x < 0.0F)
                x = -x;
            projected.x = x;
            projected.y = y;
        }
        else
        {
            const float x =
                std::clamp(projected.x, -border, border);
            const float denominator =
                std::sqrt(std::max(1.0F - yTargetDot * yTargetDot,
                                   0.0001F));
            const float radius = std::abs(x) / denominator;
            float y = std::sqrt(
                std::max(radius * radius - x * x, 0.0F));
            if (projected.y < 0.0F)
                y = -y;
            projected.x = x;
            projected.y = y;
        }
        const auto desiredLead =
            rotate(isoRotation,
                   {0.0F, projected.x, projected.y});
        const float blend = std::clamp(seconds, 0.0F, 1.0F);
        cameraLead_.x += (desiredLead.x - cameraLead_.x) * blend;
        cameraLead_.y += (desiredLead.y - cameraLead_.y) * blend;
        cameraLead_.z += (desiredLead.z - cameraLead_.z) * blend;

        // Preserve CameraManager's teleport compensation so respawning does
        // not create a one-frame camera jump across the map.
        r3d::physics::Vec3 target = position;
        if (!cameraInitialized_)
        {
            previousCameraTarget_ = target;
            cameraInitialized_ = true;
        }
        const auto jump = r3d::physics::Vec3{
            target.x - previousCameraTarget_.x,
            target.y - previousCameraTarget_.y,
            target.z - previousCameraTarget_.z};
        const float jumpLength = std::sqrt(
            jump.x * jump.x + jump.y * jump.y + jump.z * jump.z);
        if (jumpLength > 6.0F)
        {
            if (cameraJumpDistance_ == 0.0F)
                cameraJumpSpeed_ = jumpLength / 0.5F;
            cameraJumpDistance_ = jumpLength;
            cameraJumpDirection_ = {
                jump.x / jumpLength, jump.y / jumpLength,
                jump.z / jumpLength};
        }
        previousCameraTarget_ = target;

        cameraJumpDistance_ = std::max(
            cameraJumpDistance_ - cameraJumpSpeed_ * seconds, 0.0F);
        const bx::Vec3 at{
            target.x + cameraLead_.x -
                cameraJumpDirection_.x * cameraJumpDistance_,
            target.y + cameraLead_.y -
                cameraJumpDirection_.y * cameraJumpDistance_,
            target.z + cameraLead_.z -
                cameraJumpDirection_.z * cameraJumpDistance_};
        const bx::Vec3 eye{
            at.x - isoDirection.x * 20.0F,
            at.y - isoDirection.y * 20.0F,
            at.z - isoDirection.z * 20.0F};
        cameraPosition_ = {eye.x, eye.y, eye.z};
        Camera camera;
        bx::mtxLookAt(camera.view.data(), eye, at,
                      {0.0F, 0.0F, 1.0F},
                      bx::Handedness::Right);
        const float cameraHeight = cameraWidth / aspect;
        bx::mtxOrtho(camera.projection.data(), -cameraWidth * 0.5F,
                     cameraWidth * 0.5F, -cameraHeight * 0.5F,
                     cameraHeight * 0.5F, 1.0F, 150.0F, 0.0F,
                     device.usesHomogeneousDepth(),
                     bx::Handedness::Right);
        return camera;
    }

    const float speedFactor =
        std::clamp(velocityLength / (150.0F / 3.6F), 0.0F, 1.0F);
    // CameraManager::csThirdPerson uses cCamTargetOff(-4.6, 0, 2.4),
    // an additional -1 m offset, and up to 1.5 m of speed pull-back.
    const float directionBlend = std::clamp(seconds * 6.0F, 0.0F, 1.0F);
    thirdPersonDirection_.x +=
        (velocityForward.x - thirdPersonDirection_.x) * directionBlend;
    thirdPersonDirection_.y +=
        (velocityForward.y - thirdPersonDirection_.y) * directionBlend;
    const float thirdPersonLength = std::sqrt(
        thirdPersonDirection_.x * thirdPersonDirection_.x +
        thirdPersonDirection_.y * thirdPersonDirection_.y);
    if (thirdPersonLength > 0.0001F)
    {
        thirdPersonDirection_.x /= thirdPersonLength;
        thirdPersonDirection_.y /= thirdPersonLength;
    }
    const float pullbackTarget = speedFactor * speedFactor * 1.5F;
    thirdPersonPullback_ +=
        (pullbackTarget - thirdPersonPullback_) *
        std::clamp(seconds * 5.0F, 0.0F, 1.0F);
    const float distance = 5.6F + thirdPersonPullback_;
    const bx::Vec3 eye{
        position.x - thirdPersonDirection_.x * distance,
        position.y - thirdPersonDirection_.y * distance,
                       position.z + 2.4F};
    const bx::Vec3 at{position.x + thirdPersonDirection_.x * 8.0F,
                      position.y + thirdPersonDirection_.y * 8.0F,
                      position.z + 2.4F};
    cameraPosition_ = {eye.x, eye.y, eye.z};
    cameraViewDirection_ = {
        thirdPersonDirection_.x, thirdPersonDirection_.y, 0.0F};
    previousCameraTarget_ = position;
    cameraInitialized_ = true;
    Camera camera;
    bx::mtxLookAt(camera.view.data(), eye, at, {0.0F, 0.0F, 1.0F},
                  bx::Handedness::Right);
    bx::mtxProj(camera.projection.data(), 75.0F,
                aspect,
                1.0F, 120.0F, device.usesHomogeneousDepth(),
                bx::Handedness::Right);
    return camera;
}

void OriginalRaceRenderer::resetCamera() noexcept
{
    cameraLead_ = {};
    previousCameraTarget_ = {};
    cameraPosition_ = {};
    cameraViewDirection_ = {1.0F, 0.0F, 0.0F};
    cameraJumpDirection_ = {};
    thirdPersonDirection_ = {1.0F, 0.0F, 0.0F};
    cameraJumpDistance_ = 0.0F;
    cameraJumpSpeed_ = 0.0F;
    thirdPersonPullback_ = 0.0F;
    cameraInitialized_ = false;
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
    const std::vector<r3d::game::originalrace::RacerRuntime>& racerRuntime,
    const std::vector<r3d::game::originalrace::RaceEffect>& effects,
    const std::vector<r3d::game::originalrace::MineRuntime>& mines,
    const std::vector<
        r3d::game::originalrace::ProjectileRuntime>& projectiles,
    float elapsedSeconds, bool reflectionPass,
    bool omitEnvironmentSurface)
{
    SceneLighting sceneLighting;
    const auto sun = race.environment.sunPosition;
    const float sunLength =
        std::sqrt(sun.x * sun.x + sun.y * sun.y + sun.z * sun.z);
    if (sunLength > 0.0001F)
    {
        sceneLighting.lightDirection =
            {sun.x / sunLength, sun.y / sunLength, sun.z / sunLength,
             0.0F};
    }
    sceneLighting.ambient = race.environment.ambientColor;
    sceneLighting.fogColor = race.environment.fogColor;
    sceneLighting.fogColor[3] = race.environment.fogIntensity;
    if (!vehicles.empty())
    {
        sceneLighting.cameraPosition =
            {cameraPosition_.x, cameraPosition_.y, cameraPosition_.z,
             1.0F};
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
        sky.position = vehicles.front().body.position;
        // SkyBox.cpp applied this left-handed source -> right-handed render
        // conversion before sampling every original cubemap.
        constexpr float sinHalfRightAngle = 0.7071067811865476F;
        sky.rotation = {sinHalfRightAngle, 0.0F, 0.0F,
                        sinHalfRightAngle};
        sky.scale = {90.0F, 90.0F, 90.0F};
        MaterialState skyMaterial;
        skyMaterial.environmentTexture = skyTexture_;
        skyMaterial.receivesShadow = false;
        device.draw(skyMesh_, skyShader_, skyTexture_, transform(sky),
                    skyPipeline, {}, skyMaterial);
    }
    device.setSceneLighting(sceneLighting);

    bool deferredEnvironmentSurface = false;
    PipelineState deferredSurfacePipeline;
    MaterialState deferredSurfaceMaterial;
    Transform deferredSurfaceTransform;
    if (!reflectionPass && !omitEnvironmentSurface &&
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
        if (race.environment.surface ==
                r3d::game::originalrace::EnvironmentSurface::GroundFog ||
            race.environment.surface ==
                r3d::game::originalrace::EnvironmentSurface::Magma)
        {
            surfacePipeline.blendMode =
                PipelineState::BlendMode::Alpha;
            surfacePipeline.writeDepth = false;
            surfaceMaterial.color =
                race.environment.surface ==
                        r3d::game::originalrace::
                            EnvironmentSurface::Magma
                    ? std::array<float, 4>{1.0F, 1.0F, 1.0F, 0.72F}
                    : std::array<float, 4>{
                          race.environment.fogColor[0],
                          race.environment.fogColor[1],
                          race.environment.fogColor[2], 0.28F};
            const float offset = std::fmod(
                elapsedSeconds * race.environment.surfaceScroll, 1.0F);
            surfaceMaterial.textureTransform =
                {1.0F, 1.0F, offset, 0.0F};
        }
        r3d::physics::Transform surface;
        surface.position = environmentSurfaceCenter_;
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
    auto drawObject = [&](const ObjectAsset& asset,
                          const std::vector<
                              r3d::game::originalrace::VisualNode>& nodes,
                          const r3d::physics::Transform& parent,
                          r3d::game::originalrace::GraphOrder graphOrder,
                          bool cullOpacityActor,
                          float opacity,
                          const std::array<float, 4>* tint,
                          float objectAnimationSeconds = -1.0F) {
        const float animationSeconds =
            objectAnimationSeconds >= 0.0F
                ? objectAnimationSeconds
                : elapsedSeconds;
        const std::size_t count = std::min(asset.nodes.size(), nodes.size());
        for (std::size_t index = 0; index < count; ++index)
        {
            const auto world =
                compose(parent, nodes[index].transform);
            auto model = transform(world);
            if (nodes[index].plane)
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
                asset.planarReflection && !reflectionPass
                    ? 0.58F
                    : 0.0F;
            const bool deferredActor =
                cullOpacityActor ||
                graphOrder !=
                    r3d::game::originalrace::GraphOrder::Default;
            if (!deferredActor)
            {
                drawGroups(device, asset.nodes[index], shader, model,
                           pipeline, animationSeconds, reflectionStrength,
                           asset.lighting, DrawLayer::Opaque,
                           &nodes[index], 1.0F, tint);
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
                     reflectionStrength, asset.lighting,
                     dx * dx + dy * dy + dz * dz, opacity,
                     renderStage(graphOrder, cullOpacityActor),
                     deferredActor ? DrawLayer::All
                                   : DrawLayer::Transparency,
                     animationSeconds,
                     tint != nullptr
                         ? *tint
                         : std::array<float, 4>{
                               1.0F, 1.0F, 1.0F, 1.0F},
                     tint != nullptr});
            }
        }
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
            std::uint32_t seed) {
            const float x = unitNoise(seed);
            const float y = unitNoise(seed + 0x9e3779b9U);
            const float z = unitNoise(seed + 0x3c6ef372U);
            return r3d::physics::Vec3{
                minimum.x + (maximum.x - minimum.x) * x,
                minimum.y + (maximum.y - minimum.y) * y,
                minimum.z + (maximum.z - minimum.z) * z};
        };
    auto drawParticles =
        [&](const ObjectAsset& asset,
            const r3d::game::originalrace::ObjectDefinition& definition,
            const r3d::physics::Transform& parent, float age,
            float sourceSpeed,
            const std::vector<r3d::physics::Vec3>*
                trailOverride,
            float opacity, bool forceNoDepth) {
            const std::size_t emitterCount = std::min(
                asset.particleTextures.size(),
                definition.particleEmitters.size());
            for (std::size_t emitterIndex = 0;
                 emitterIndex < emitterCount; ++emitterIndex)
            {
                const auto& emitter =
                    definition.particleEmitters[emitterIndex];
                const auto& textures =
                    asset.particleTextures[emitterIndex];
                if (textures.empty() || emitter.materials.empty())
                    continue;
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
                const float distanceSpeed =
                    std::max(sourceSpeed, 0.0F);
                const float scheduleAge =
                    emitter.distanceTriggered
                        ? age * distanceSpeed
                        : age;
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
                     nextBirth <= scheduleAge + 0.0001F;
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
                const auto emitterWorld =
                    compose(parent, emitter.transform);
                std::vector<r3d::physics::Vec3> trailPoints;
                MaterialState trailMaterial;
                PipelineState trailPipeline;
                Texture trailTexture;
                bool trailConfigured = false;
                std::uint32_t submittedParticles = 0;
                constexpr std::uint32_t renderParticleLimit = 96U;
                for (const auto& scheduled : scheduledGroups)
                {
                    const float particleAge =
                        std::max(age - scheduled.birth, 0.0F);
                    const std::uint32_t groupSeed =
                        scheduled.index * 747796405U +
                        static_cast<std::uint32_t>(
                            emitterIndex) *
                            2891336453U;
                    for (std::uint32_t groupParticle = 0;
                         groupParticle < scheduled.particleCount &&
                         submittedParticles < renderParticleLimit;
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
                            emitter.startPositionMaximum, seed + 31U);
                        position.x +=
                            emitter.rangePositionMinimum.x +
                            (emitter.rangePositionMaximum.x -
                             emitter.rangePositionMinimum.x) *
                                rangeFrame;
                        position.y +=
                            emitter.rangePositionMinimum.y +
                            (emitter.rangePositionMaximum.y -
                             emitter.rangePositionMinimum.y) *
                                rangeFrame;
                        position.z +=
                            emitter.rangePositionMinimum.z +
                            (emitter.rangePositionMaximum.z -
                             emitter.rangePositionMinimum.z) *
                                rangeFrame;
                        const auto velocity = rangeVector(
                            emitter.velocityMinimum,
                            emitter.velocityMaximum, seed + 67U);
                        auto acceleration = rangeVector(
                            emitter.accelerationMinimum,
                            emitter.accelerationMaximum, seed + 101U);
                        acceleration.x += emitter.gravity.x;
                        acceleration.y += emitter.gravity.y;
                        acceleration.z += emitter.gravity.z;
                        position.x +=
                            velocity.x * particleAge +
                            acceleration.x * particleAge *
                                particleAge * 0.5F;
                        position.y +=
                            velocity.y * particleAge +
                            acceleration.y * particleAge *
                                particleAge * 0.5F;
                        position.z +=
                            velocity.z * particleAge +
                            acceleration.z * particleAge *
                                particleAge * 0.5F;
                        auto scale = rangeVector(
                            emitter.startScaleMinimum,
                            emitter.startScaleMaximum, seed + 149U);
                        scale.x +=
                            emitter.rangeScaleMinimum.x +
                            (emitter.rangeScaleMaximum.x -
                             emitter.rangeScaleMinimum.x) *
                                rangeFrame;
                        scale.y +=
                            emitter.rangeScaleMinimum.y +
                            (emitter.rangeScaleMaximum.y -
                             emitter.rangeScaleMinimum.y) *
                                rangeFrame;
                        scale.z +=
                            emitter.rangeScaleMinimum.z +
                            (emitter.rangeScaleMaximum.z -
                             emitter.rangeScaleMinimum.z) *
                                rangeFrame;
                        const auto scaleVelocity = rangeVector(
                            emitter.scaleVelocityMinimum,
                            emitter.scaleVelocityMaximum, seed + 193U);
                        scale.x = std::max(
                            scale.x +
                                scaleVelocity.x * particleAge,
                            0.01F);
                        scale.y = std::max(
                            scale.y +
                                scaleVelocity.y * particleAge,
                            0.01F);
                        scale.z = std::max(
                            scale.z +
                                scaleVelocity.z * particleAge,
                            0.01F);
                        r3d::physics::Transform particle;
                        particle.position = position;
                        particle.scale = scale;
                        particle.rotation = multiply(
                            normalizedLerp(
                                emitter.startRotationMinimum,
                                emitter.startRotationMaximum,
                                unitNoise(seed + 211U)),
                            normalizedLerp(
                                emitter.rangeRotationMinimum,
                                emitter.rangeRotationMaximum,
                                rangeFrame));
                        const auto rotationVelocity =
                            normalizedLerp(
                                emitter.rotationVelocityMinimum,
                                emitter.rotationVelocityMaximum,
                                unitNoise(seed + 223U));
                        particle.rotation = multiply(
                            normalizedLerp(
                                {}, rotationVelocity,
                                std::fmod(
                                    std::max(particleAge, 0.0F),
                                    1.0F)),
                            particle.rotation);
                        auto world = compose(emitterWorld, particle);
                        if (emitter.worldCoordinates)
                        {
                            const auto sourceDirection =
                                rotateX(parent.rotation);
                            world.position.x -= sourceDirection.x *
                                                sourceSpeed *
                                                particleAge;
                            world.position.y -= sourceDirection.y *
                                                sourceSpeed *
                                                particleAge;
                            world.position.z -= sourceDirection.z *
                                                sourceSpeed *
                                                particleAge;
                        }
                        auto direction = rotate(
                            emitterWorld.rotation,
                            {velocity.x +
                                 acceleration.x * particleAge,
                             velocity.y +
                                 acceleration.y * particleAge,
                             velocity.z +
                                 acceleration.z * particleAge});
                        const auto unitDirection =
                            normalize(direction);
                        const float turnAngle =
                            emitter.autoRotate
                                ? std::acos(std::clamp(
                                      unitDirection.x, -1.0F, 1.0F))
                                : 0.0F;
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
                                world.position, world.scale,
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
                        MaterialState material;
                        material.color = sourceMaterial.color;
                        material.color[3] *= opacity;
                        material.alphaReference =
                            sourceMaterial.alphaReference;
                        material.emissive = sourceMaterial.emissive;
                        material.specular = sourceMaterial.specular;
                        material.shininess = sourceMaterial.shininess;
                        material.ignoreFog = sourceMaterial.ignoreFog;
                        material.receivesShadow = false;
                        material.textureTransform = animatedAtlas(
                            sourceMaterial.atlasColumns,
                            sourceMaterial.atlasRows, particleAge,
                            sourceMaterial.animationRate);
                        auto particlePipeline = pipeline;
                        if (forceNoDepth)
                            particlePipeline.writeDepth = false;
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
                        particlePipeline.faceCulling =
                            PipelineState::FaceCulling::None;
                        if (emitter.renderMode ==
                            r3d::game::originalrace::
                                ParticleRenderMode::Trail)
                        {
                            trailPoints.push_back(world.position);
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
                    if (submittedParticles >= renderParticleLimit)
                        break;
                }
                if (trailConfigured && trailOverride != nullptr &&
                    emitter.renderMode ==
                        r3d::game::originalrace::
                            ParticleRenderMode::Trail)
                {
                    trailPoints.clear();
                    trailPoints.push_back(emitterWorld.position);
                    for (auto point = trailOverride->rbegin();
                         point != trailOverride->rend(); ++point)
                    {
                        const auto& previous = trailPoints.back();
                        const float dx = point->x - previous.x;
                        const float dy = point->y - previous.y;
                        const float dz = point->z - previous.z;
                        if (dx * dx + dy * dy + dz * dz > 0.0001F)
                            trailPoints.push_back(*point);
                    }
                }
                else if (trailConfigured && !trailPoints.empty())
                {
                    trailPoints.insert(
                        trailPoints.begin(), emitterWorld.position);
                }
                if (trailConfigured && trailPoints.size() >= 2U)
                {
                    std::vector<StaticMeshVertex> trailVertices;
                    std::vector<std::uint32_t> trailIndices;
                    trailVertices.reserve(trailPoints.size() * 2U);
                    trailIndices.reserve(
                        (trailPoints.size() - 1U) * 6U);
                    const auto fixedUp =
                        normalize(emitter.trailFixedUp);
                    for (std::size_t point = 0;
                         point < trailPoints.size(); ++point)
                    {
                        const auto previous =
                            trailPoints[
                                point == 0U ? point : point - 1U];
                        const auto next =
                            trailPoints[
                                std::min(
                                    point + 1U,
                                    trailPoints.size() - 1U)];
                        auto direction = normalize(
                            {next.x - previous.x,
                             next.y - previous.y,
                             next.z - previous.z});
                        if (std::abs(direction.x) +
                                std::abs(direction.y) +
                                std::abs(direction.z) <
                            0.0001F)
                        {
                            direction = {1.0F, 0.0F, 0.0F};
                        }
                        auto side =
                            emitter.trailFixedUpEnabled
                                ? normalize(
                                      cross(fixedUp, direction))
                                : normalize(cross(
                                      direction,
                                      normalize(
                                          {cameraPosition_.x -
                                               trailPoints[point].x,
                                           cameraPosition_.y -
                                               trailPoints[point].y,
                                           cameraPosition_.z -
                                               trailPoints[point].z})));
                        if (std::abs(side.x) + std::abs(side.y) +
                                std::abs(side.z) <
                            0.0001F)
                        {
                            side = {0.0F, 1.0F, 0.0F};
                        }
                        const float width =
                            std::max(emitter.trailWidth, 0.001F);
                        const float pathV =
                            trailPoints.size() > 1U
                                ? static_cast<float>(point) /
                                      static_cast<float>(
                                          trailPoints.size() - 1U)
                                : 0.0F;
                        const auto& position = trailPoints[point];
                        trailVertices.push_back(
                            {position.x + side.x * width,
                             position.y + side.y * width,
                             position.z + side.z * width,
                             fixedUp.x, fixedUp.y, fixedUp.z,
                             0.0F, pathV,
                             direction.x, direction.y, direction.z,
                             side.x, side.y, side.z});
                        trailVertices.push_back(
                            {position.x - side.x * width,
                             position.y - side.y * width,
                             position.z - side.z * width,
                             fixedUp.x, fixedUp.y, fixedUp.z,
                             1.0F, pathV,
                             direction.x, direction.y, direction.z,
                             side.x, side.y, side.z});
                        if (point + 1U < trailPoints.size())
                        {
                            const auto first =
                                static_cast<std::uint32_t>(
                                    point * 2U);
                            trailIndices.insert(
                                trailIndices.end(),
                                {first, first + 1U, first + 2U,
                                 first + 1U, first + 3U,
                                 first + 2U});
                        }
                    }
                    Transform trailTransform;
                    trailTransform.matrix = identityMatrix();
                    device.drawTransient(
                        trailVertices.data(),
                        trailVertices.size(),
                        trailIndices.data(),
                        trailIndices.size(), shader,
                        trailTexture, trailTransform,
                        trailPipeline, trailMaterial);
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
        float sourceSpeed = 0.0F;
        const std::vector<r3d::physics::Vec3>* trailOverride =
            nullptr;
        float opacity = 1.0F;
        RenderStage stage = RenderStage::Opacity;
        float distanceSquared = 0.0F;
    };
    std::vector<DeferredParticleDraw> deferredParticles;
    auto drawDefinition =
        [&](const ObjectAsset& asset,
            const r3d::game::originalrace::ObjectDefinition& definition,
            const r3d::physics::Transform& parent, float age,
            float sourceSpeed,
            const std::vector<r3d::physics::Vec3>*
                trailOverride = nullptr,
            float opacity = 1.0F) {
            // gpCullOpacity only makes an actor a RayUser after the
            // camera-to-player cast hits it.  ActorManager renders every
            // other actor through its normal graph-order/depth pass.
            // Deferring every tagged actor unconditionally changed depth
            // ordering across the whole map and caused visible popping.
            const bool cullOpacityActor =
                !reflectionPass && definition.cullOpacity &&
                opacity < 0.999F;
            drawObject(asset, definition.visualNodes, parent,
                       definition.graphOrder, cullOpacityActor, opacity,
                       nullptr, age);
            if (!definition.particleEmitters.empty())
            {
                const float dx =
                    parent.position.x - cameraPosition_.x;
                const float dy =
                    parent.position.y - cameraPosition_.y;
                const float dz =
                    parent.position.z - cameraPosition_.z;
                deferredParticles.push_back(
                    {&asset, &definition, parent, age, sourceSpeed,
                     trailOverride, opacity,
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
            instance.transform, elapsedSeconds, 0.0F, nullptr,
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
            instance.transform, elapsedSeconds, 0.0F, nullptr,
            opacity);
    }

    for (std::size_t index = 0; index < race.bonuses.size(); ++index)
    {
        if (index < bonusActive.size() && !bonusActive[index])
            continue;
        auto animated = race.bonuses[index].transform;
        animated.position.z +=
            0.35F * std::sin(elapsedSeconds * 2.0F +
                            static_cast<float>(index));
        const float halfAngle = elapsedSeconds * 0.75F;
        animated.rotation = multiply(
            animated.rotation,
            {0.0F, 0.0F, std::sin(halfAngle),
             std::cos(halfAngle)});
        drawDefinition(
            bonuses_.at(index), race.bonuses[index].visual,
            animated, elapsedSeconds, 0.0F);
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
            racerRuntime[racer].destroyed)
            continue;
        const auto vehicleIndex = race.racers[racer].vehicle;
        if (vehicleIndex >= race.vehicles.size() ||
            racer >= vehicleBodies_.size())
            continue;
        const auto& definition =
            race.racers[racer].hasConfiguredVehicle
                ? race.racers[racer].configuredVehicle
                : race.vehicles[vehicleIndex];
        const auto& state = vehicles[racer];
        drawObject(vehicleBodies_[racer],
                   definition.bodyVisuals, state.body,
                   r3d::game::originalrace::GraphOrder::Default,
                   false, 1.0F, &race.racers[racer].color);
        if (racer < racerRuntime.size())
        {
            const auto& runtime = racerRuntime[racer];
            if (runtime.slowSeconds > 0.0F &&
                runtime.slowWeapon < race.weapons.size() &&
                runtime.slowWeapon < projectiles_.size() &&
                runtime.slowProjectile <
                    race.weapons[runtime.slowWeapon]
                        .projectiles.size() &&
                runtime.slowProjectile <
                    projectiles_[runtime.slowWeapon].size())
            {
                const auto& slowDefinition =
                    race.weapons[runtime.slowWeapon]
                        .projectiles[runtime.slowProjectile];
                const auto& slowAssets =
                    projectiles_[runtime.slowWeapon]
                                [runtime.slowProjectile];
                if (!slowDefinition.tertiaryVisual.visualNodes.empty() ||
                    !slowDefinition.tertiaryVisual
                         .particleEmitters.empty())
                {
                    const float total =
                        slowDefinition.tertiaryVisual.maximumTimeLife >
                                0.0F
                            ? slowDefinition.tertiaryVisual
                                  .maximumTimeLife
                            : 1.0F;
                    drawDefinition(
                        slowAssets.tertiaryVisual,
                        slowDefinition.tertiaryVisual, state.body,
                        std::max(total - runtime.slowSeconds, 0.0F),
                        std::abs(state.speed));
                }
            }
            for (std::size_t slot = 0;
                 slot < runtime.weaponSlots.size() &&
                 slot < definition.weaponMounts.size(); ++slot)
            {
                const auto weaponIndex = runtime.weaponSlots[slot];
                const auto& mount = definition.weaponMounts[slot];
                if (!mount.active || !mount.show ||
                    weaponIndex == r3d::game::originalrace::
                                       RacerRuntime::invalidWeapon ||
                    weaponIndex >= race.weapons.size() ||
                    weaponIndex >= weapons_.size() ||
                    weapons_[weaponIndex].nodes.empty())
                    continue;
                r3d::physics::Transform local;
                local.position = mount.position;
                const auto wanted =
                    recordName(race.weapons[weaponIndex].record);
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
                const auto& weaponNode =
                    race.weapons[weaponIndex].visual;
                auto weaponTransform = weaponNode.transform;
                if (slot < runtime.weaponSpinRadians.size())
                {
                    const float halfAngle =
                        runtime.weaponSpinRadians[slot] * 0.5F;
                    const r3d::physics::Quat sourceSpin{
                        std::sin(halfAngle), 0.0F, 0.0F,
                        std::cos(halfAngle)};
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
        const bool showNightLights =
            race.environment.weather ==
                r3d::game::originalrace::Weather::Night;
        if (showNightLights)
        {
            for (const auto& source : definition.nightLights)
            {
                r3d::physics::Transform local;
                local.position = source.position;
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
             definition.wheelVisualOffsets.size(),
             vehicleWheels_[racer].size()});
        for (std::size_t wheelIndex = 0; wheelIndex < wheelCount;
             ++wheelIndex)
        {
            auto wheel = state.wheels[wheelIndex];
            const auto offset = rotate(
                state.body.rotation,
                definition.wheelVisualOffsets[wheelIndex]);
            wheel.position.x += offset.x;
            wheel.position.y += offset.y;
            wheel.position.z += offset.z;
            const auto& wheelAsset =
                vehicleWheels_[racer][wheelIndex];
            if (!wheelAsset.nodes.empty())
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
            const bool slipping =
                contact != nullptr && contact->hasContact &&
                (std::abs(contact->longitudinalSlip) > 0.4F ||
                 std::abs(contact->lateralSlip) > 0.6F);
            const auto* trailPath =
                racer < wheelTrailPaths_.size() &&
                        wheelIndex < wheelTrailPaths_[racer].size()
                    ? &wheelTrailPaths_[racer][wheelIndex]
                    : nullptr;
            if (slipping ||
                (trailPath != nullptr && !trailPath->empty()))
            {
                auto trailParent = wheel;
                trailParent.rotation = state.body.rotation;
                if (slipping)
                {
                    trailParent.position = contact->position;
                    trailParent.position.z += 0.001F;
                }
                else
                {
                    // FxSystemWaitingEnd keeps the existing particles alive
                    // after slip stops; anchor it at the final sample so a
                    // new strip is not stretched to the moving wheel.
                    trailParent.position = trailPath->back();
                }
                drawDefinition(
                    wheelTrailEffect_, race.wheelTrailEffect,
                    trailParent, elapsedSeconds,
                    std::abs(state.speed),
                    trailPath);
            }
        }
        if (racer < racerRuntime.size() &&
            racer < vehicleLowLifeEffects_.size() &&
            racerRuntime[racer].lowLife)
        {
            r3d::physics::Transform local;
            local.position = definition.lowLifeEffectPosition;
            drawDefinition(
                vehicleLowLifeEffects_[racer],
                definition.lowLifeEffect,
                compose(state.body, local),
                racerRuntime[racer].lowLifeEffectSeconds,
                std::abs(state.speed));
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
        const auto& definition =
            race.weapons[projectile.weapon]
                .projectiles[projectile.projectile];
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
                        : projectile.maximumDistance,
                    0.1F);
            parent.position.x +=
                projectile.direction.x * distance * 0.5F;
            parent.position.y +=
                projectile.direction.y * distance * 0.5F;
            parent.position.z +=
                projectile.direction.z * distance * 0.5F;
            parent.scale.x = distance;
            if (definition.type == 3U &&
                definition.minimumLife > 0.0F)
            {
                const float alphaTime = std::clamp(
                    projectile.ageSeconds / definition.minimumLife,
                    0.0F, 1.0F);
                const float fadeIn = std::clamp(
                    alphaTime / 0.5F * 1.5F + 0.5F,
                    0.0F, 2.0F);
                const float fadeOut = std::clamp(
                    (alphaTime - 0.6F) / 0.4F * 2.0F,
                    0.0F, 2.0F);
                parent.scale.y = fadeIn - fadeOut;
            }
        }
        drawDefinition(
            asset, definition.visual, parent,
            projectile.ageSeconds,
            std::sqrt(projectile.velocity.x *
                          projectile.velocity.x +
                      projectile.velocity.y *
                          projectile.velocity.y +
                      projectile.velocity.z *
                          projectile.velocity.z));
        if (projectile.attached)
        {
            const float distance =
                std::max(
                    projectile.impactDistance > 0.0F
                        ? projectile.impactDistance
                        : projectile.maximumDistance,
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
                    projectile.ageSeconds, 0.0F);
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
        const auto& definition =
            race.weapons[mine.weapon].projectiles[mine.projectile];
        const auto& assets =
            projectiles_[mine.weapon][mine.projectile];
        const ObjectAsset* asset = &assets.visual;
        const r3d::game::originalrace::ObjectDefinition* visual =
            &definition.visual;
        if (mine.visualVariant == 1U)
        {
            asset = &assets.secondaryVisual;
            visual = &definition.secondaryVisual;
        }
        else if (mine.visualVariant == 2U)
        {
            asset = &assets.tertiaryVisual;
            visual = &definition.tertiaryVisual;
        }
        if (asset->nodes.empty() &&
            asset->particleTextures.empty())
            continue;
        r3d::physics::Transform parent;
        parent.position = mine.position;
        parent.rotation = mine.rotation;
        if (mine.type == 10U)
        {
            const float scale =
                std::clamp(mine.seconds / 0.25F, 0.0F, 1.0F);
            parent.scale = {scale, scale, scale};
        }
        drawDefinition(*asset, *visual, parent, mine.seconds,
                       std::sqrt(
                           mine.velocity.x * mine.velocity.x +
                           mine.velocity.y * mine.velocity.y +
                           mine.velocity.z * mine.velocity.z));
    }

    for (const auto& effect : effects)
    {
        if (effect.kind ==
                r3d::game::originalrace::RaceEventKind::
                    VehicleDestroyed &&
            effect.racer < race.racers.size() &&
            effect.racer < vehicleDeathEffects_.size())
        {
            const auto& sourceRacer = race.racers[effect.racer];
            const auto& vehicle =
                sourceRacer.hasConfiguredVehicle
                    ? sourceRacer.configuredVehicle
                    : race.vehicles.at(sourceRacer.vehicle);
            if (effect.vehicleEffect >= vehicle.deathEffects.size() ||
                effect.vehicleEffect >=
                    vehicleDeathEffects_[effect.racer].size())
                continue;
            const auto& definition =
                vehicle.deathEffects[effect.vehicleEffect].visual;
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
                effect.totalSeconds - effect.seconds, 0.0F);
            continue;
        }
        if (effect.kind ==
                r3d::game::originalrace::RaceEventKind::ProjectileImpact &&
            effect.bonus < race.bonuses.size() &&
            effect.bonus < bonusDeathEffects_.size())
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
                race.bonuses[effect.bonus].deathEffect.visual, parent,
                effect.totalSeconds - effect.seconds, 0.0F);
            continue;
        }
        if (effect.kind ==
            r3d::game::originalrace::RaceEventKind::WeaponShotEffect)
        {
            if (effect.weapon < race.weapons.size() &&
                effect.weapon < weaponShotEffects_.size())
            {
                const auto& definition =
                    race.weapons[effect.weapon].shotEffect.visual;
                if (!definition.visualNodes.empty() ||
                    !definition.particleEmitters.empty())
                {
                    drawDefinition(
                        weaponShotEffects_[effect.weapon], definition,
                        effect.transform,
                        effect.totalSeconds - effect.seconds, 0.0F);
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
                const auto* definition =
                    effect.visualVariant == 6U
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
                               : &projectileDefinition.secondaryVisual))));
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
                parent.position = effect.origin;
                if (!effect.ignoreRotation)
                {
                    parent.rotation = directionRotation(
                        {effect.target.x - effect.origin.x,
                         effect.target.y - effect.origin.y,
                         effect.target.z - effect.origin.z});
                }
                drawDefinition(
                    *asset, *definition, parent,
                    effect.totalSeconds - effect.seconds, 0.0F);
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
            racerRuntime[racer].destroyed ||
            racer >= vehicleShieldEffects_.size() ||
            racer >= vehicleShieldScales_.size())
            continue;
        const auto& runtime = racerRuntime[racer];
        if (runtime.shieldSeconds <= 0.0F &&
            runtime.shieldFadeOutSeconds < 0.0F)
            continue;
        const auto& sourceRacer = race.racers[racer];
        const auto& definition =
            sourceRacer.hasConfiguredVehicle
                ? sourceRacer.configuredVehicle
                : race.vehicles.at(sourceRacer.vehicle);
        float fade = 1.0F;
        if (runtime.shieldFadeInSeconds >= 0.0F)
        {
            fade = std::clamp(
                runtime.shieldFadeInSeconds / 0.5F, 0.0F, 1.0F);
        }
        else if (runtime.shieldFadeOutSeconds >= 0.0F)
        {
            fade = 1.0F -
                   std::clamp(
                       runtime.shieldFadeOutSeconds / 0.5F,
                       0.0F, 1.0F);
        }
        r3d::physics::Transform shield = vehicles[racer].body;
        shield.scale = {
            shield.scale.x * vehicleShieldScales_[racer].x * fade,
            shield.scale.y * vehicleShieldScales_[racer].y * fade,
            shield.scale.z * vehicleShieldScales_[racer].z * fade};
        float damageAlpha = 1.0F;
        if (runtime.shieldDamageSeconds >= 0.0F)
        {
            const float damageFrame = std::clamp(
                runtime.shieldDamageSeconds / 0.25F, 0.0F, 1.0F);
            damageAlpha = 1.0F + 2.5F * (1.0F - damageFrame);
        }
        const std::array<float, 4> shieldTint{
            1.0F, 1.0F, 1.0F, damageAlpha};
        drawObject(
            vehicleShieldEffects_[racer],
            definition.shieldEffect.visualNodes, shield,
            definition.shieldEffect.graphOrder, false, 1.0F,
            &shieldTint, runtime.shieldEffectSeconds);
    }

    if (race.environment.rain && !vehicles.empty())
    {
        r3d::physics::Transform rainParent;
        rainParent.position = cameraPosition_;
        drawDefinition(rainEffect_, race.rainEffect, rainParent,
                       elapsedSeconds, 0.0F);
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
                       deferred.hasTint ? &deferred.tint : nullptr);
        }
        for (const auto& deferred : deferredParticles)
        {
            if (deferred.stage != stage)
                continue;
            drawParticles(*deferred.asset, *deferred.definition,
                          deferred.parent, deferred.age,
                          deferred.sourceSpeed,
                          deferred.trailOverride,
                          deferred.opacity, forceNoDepth);
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
                    transform(compose(parent, nodes[index].transform)),
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
            racerRuntime[racer].destroyed)
            continue;
        const auto vehicleIndex = race.racers[racer].vehicle;
        if (vehicleIndex >= race.vehicles.size())
            continue;
        const auto& definition =
            race.racers[racer].hasConfiguredVehicle
                ? race.racers[racer].configuredVehicle
                : race.vehicles[vehicleIndex];
        const auto& state = vehicles[racer];
        drawObject(vehicleBodies_[racer], definition.bodyVisuals,
                   state.body);
        const auto wheelCount = std::min(
            {state.wheels.size(), definition.wheelVisuals.size(),
             definition.wheelVisualOffsets.size(),
             vehicleWheels_[racer].size()});
        for (std::size_t wheelIndex = 0; wheelIndex < wheelCount;
             ++wheelIndex)
        {
            auto wheel = state.wheels[wheelIndex];
            const auto offset = rotate(
                state.body.rotation,
                definition.wheelVisualOffsets[wheelIndex]);
            wheel.position.x += offset.x;
            wheel.position.y += offset.y;
            wheel.position.z += offset.z;
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
        const auto& sourceRacer = race.racers[fragment.racer];
        const auto& vehicle =
            sourceRacer.hasConfiguredVehicle
                ? sourceRacer.configuredVehicle
                : race.vehicles.at(sourceRacer.vehicle);
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
    const std::vector<r3d::game::originalrace::RacerRuntime>& racerRuntime,
    const std::vector<r3d::game::originalrace::RaceEffect>& effects,
    const std::vector<r3d::game::originalrace::MineRuntime>& mines,
    const std::vector<
        r3d::game::originalrace::ProjectileRuntime>& projectiles,
    float elapsedSeconds,
    const r3d::game::originalrace::QualityConfig& quality)
{
    if (wheelTrailUpdateSeconds_ < 0.0F ||
        elapsedSeconds < wheelTrailUpdateSeconds_)
    {
        wheelTrailPaths_.clear();
        wheelTrailTimes_.clear();
        wheelTrailResetCounts_.clear();
    }
    wheelTrailUpdateSeconds_ = elapsedSeconds;
    wheelTrailPaths_.resize(vehicles.size());
    wheelTrailTimes_.resize(vehicles.size());
    wheelTrailResetCounts_.resize(vehicles.size());
    const auto& trailEmitters = race.wheelTrailEffect.particleEmitters;
    const float trailLife =
        trailEmitters.empty()
            ? 10.0F
            : std::max({trailEmitters.front().lifeMinimum,
                        trailEmitters.front().lifeMaximum, 0.1F});
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
            race.racers[racer].hasConfiguredVehicle
                ? race.racers[racer].configuredVehicle
                : race.vehicles[vehicleIndex];
        const auto& state = vehicles[racer];
        const auto wheelCount = std::min(
            {state.wheels.size(), state.wheelContacts.size(),
             definition.wheelVisualOffsets.size()});
        auto& paths = wheelTrailPaths_[racer];
        auto& times = wheelTrailTimes_[racer];
        if (wheelTrailResetCounts_[racer] != state.resetCount)
        {
            paths.clear();
            times.clear();
            wheelTrailResetCounts_[racer] = state.resetCount;
        }
        paths.resize(wheelCount);
        times.resize(wheelCount);
        for (std::size_t wheel = 0; wheel < wheelCount; ++wheel)
        {
            const auto& contact = state.wheelContacts[wheel];
            auto position = contact.position;
            position.z += 0.001F;
            auto& path = paths[wheel];
            auto& sampleTimes = times[wheel];
            while (!sampleTimes.empty() &&
                   elapsedSeconds - sampleTimes.front() > trailLife)
            {
                sampleTimes.erase(sampleTimes.begin());
                path.erase(path.begin());
            }
            const bool slipping =
                contact.hasContact &&
                (std::abs(contact.longitudinalSlip) > 0.4F ||
                 std::abs(contact.lateralSlip) > 0.6F);
            if (!slipping)
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
    r3d::physics::Vec3 rayTarget{};
    float rayTargetSize = 0.0F;
    if (!vehicles.empty())
    {
        rayTarget = vehicles.front().body.position;
        if (!race.racers.empty() && !vehicleBodies_.empty())
        {
            const auto vehicleIndex = race.racers.front().vehicle;
            if (vehicleIndex < race.vehicles.size())
            {
                const auto& vehicle =
                    race.racers.front().hasConfiguredVehicle
                        ? race.racers.front().configuredVehicle
                        : race.vehicles[vehicleIndex];
                const auto bounds = objectBounds(
                    vehicleBodies_.front(), vehicle.bodyVisuals,
                    vehicles.front().body);
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
    }
    // ActorManager::PullInRayTargetGroup does not cast from the camera
    // position. It projects the player to the near plane and unprojects the
    // same screen point, producing a ray parallel to the orthographic view
    // direction. Casting from cameraPosition_ incorrectly included the
    // isometric lead offset and missed large gpCullOpacity actors directly
    // over the car.
    const r3d::physics::Vec3 rayStart{
        rayTarget.x - cameraViewDirection_.x * 150.0F,
        rayTarget.y - cameraViewDirection_.y * 150.0F,
        rayTarget.z - cameraViewDirection_.z * 150.0F};
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
        const bool overlap =
            isometricCamera && definition.cullOpacity &&
            !vehicles.empty() &&
            lineIntersectsBoundsBeforeTarget(
                objectBounds(tracks_.at(instance.definition),
                             definition.visualNodes,
                             instance.transform),
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
        const bool overlap =
            active && isometricCamera && definition.cullOpacity &&
            !vehicles.empty() &&
            lineIntersectsBoundsBeforeTarget(
                objectBounds(decorations_.at(instance.definition),
                             definition.visualNodes,
                             instance.transform),
                rayStart, rayTarget, rayTargetSize);
        updateCullTime(
            decorationCullOpacityTimes_[index], overlap);
    }

    device.resetRenderTelemetry();
    // Environment.cpp maps the three original quality levels to graph
    // options.  Keep those thresholds here instead of silently rendering the
    // high-quality graph for every profile.
    const bool shadowsEnabled = quality.shadow >= 1U;
    const bool trueReflectionsEnabled = quality.light >= 2U;
    const bool planarReflectionsEnabled = quality.light >= 2U;
    const bool weatherAllowsPostEffects =
        race.environment.weather !=
        r3d::game::originalrace::Weather::Night;
    const bool bloomEnabled =
        quality.postEffect >= 1U && weatherAllowsPostEffects;
    const bool hdrEnabled =
        quality.postEffect >= 2U && weatherAllowsPostEffects;
    const bool hasWater =
        race.environment.surface ==
        r3d::game::originalrace::EnvironmentSurface::Water;
    const bool hasReflection =
        planarReflectionsEnabled &&
        (race.environment.planarReflection || hasWater);
    const auto reflectionCamera =
        reflectedCamera(camera, race.environment.surfaceHeight);

    r3d::physics::Vec3 renderCenter{};
    if (!vehicles.empty())
        renderCenter = vehicles.front().body.position;
    else if (!race.tracePoints.empty())
        renderCenter = race.tracePoints.front().position;
    auto sun = race.environment.sunPosition;
    const float sunLength = std::sqrt(
        sun.x * sun.x + sun.y * sun.y + sun.z * sun.z);
    if (sunLength < 0.001F)
        sun = {45.0F, 30.0F, 60.0F};
    const float shadowFarDistance = isometricCamera ? 55.0F : 60.0F;
    const float shadowSplitDistance =
        sourceShadowSplit(isometricCamera);
    const float cameraFarDistance = isometricCamera ? 150.0F : 120.0F;
    const auto lightCamera = shadowCamera(
        device, camera, sun, 1.0F, shadowSplitDistance,
        cameraFarDistance);
    const auto lightCameraFar = shadowCamera(
        device, camera, sun, shadowSplitDistance,
        shadowFarDistance, cameraFarDistance);
    if (shadowsEnabled)
    {
        device.setPassState({});
        device.beginPass(RenderPass::Shadow, shadowTarget_, lightCamera,
                         0xffffffffU, true, true);
        drawShadowCasters(device, race, vehicles, pipeline,
                          decorationActive, decorationFragments,
                          vehicleDeathFragments, racerRuntime,
                          elapsedSeconds);
        device.setPassState({});
        device.beginPass(
            RenderPass::ShadowFar, shadowTargetFar_, lightCameraFar,
            0xffffffffU, true, true);
        drawShadowCasters(device, race, vehicles, pipeline,
                          decorationActive, decorationFragments,
                          vehicleDeathFragments, racerRuntime,
                          elapsedSeconds);
    }

    auto environmentCenter = renderCenter;
    environmentCenter.z += 1.0F;
    if (trueReflectionsEnabled)
    {
        for (std::size_t face = 0; face < environmentPasses.size(); ++face)
        {
            RenderPassState environmentState;
            // Prevent feedback while the dynamic cube is being populated.
            // This matches InitRefl's source sky sampler before InitTrueRefl
            // replaces it for the main scene.
            environmentState.environmentTexture = skyTexture_;
            device.setPassState(environmentState);
            device.beginPass(
                environmentPasses[face],
                environmentReflectionTarget_.faces[face],
                environmentCamera(device, environmentCenter, face),
                clearRgba, true, true);
            draw(device, sceneShader, race, vehicles, pipeline,
                 decorationActive, decorationFragments,
                 vehicleDeathFragments, bonusActive,
                 racerRuntime, effects, mines, projectiles,
                 elapsedSeconds, true, true);
        }
    }

    if (hasReflection)
    {
        RenderPassState reflectionState;
        reflectionState.environmentTexture =
            environmentReflectionTarget_.texture;
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
             vehicleDeathFragments, bonusActive,
             racerRuntime, effects, mines, projectiles,
             elapsedSeconds, true);
    }

    RenderPassState sceneState;
    if (trueReflectionsEnabled)
    {
        sceneState.environmentTexture =
            environmentReflectionTarget_.texture;
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
            device.renderTargetTexture(shadowTargetFar_);
        sceneState.shadowViewProjection = viewProjection(lightCamera);
        sceneState.shadowViewProjectionFar =
            viewProjection(lightCameraFar);
        sceneState.shadowsEnabled = true;
        sceneState.shadowStrength = 0.62F;
        sceneState.shadowSplitDistance = shadowSplitDistance;
        sceneState.shadowMapSize = 2048.0F;
        sceneState.shadowDepthBias = 0.0015F;
    }
    device.setPassState(sceneState);
    device.beginPass(
        RenderPass::Scene,
        hasWater ? waterSceneTarget_ : hdrTarget_,
        camera, clearRgba, true, true);
    draw(device, sceneShader, race, vehicles, pipeline,
         decorationActive, decorationFragments,
         vehicleDeathFragments, bonusActive,
         racerRuntime, effects, mines, projectiles, elapsedSeconds,
         false, hasWater);

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

    if (hasWater)
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

        RenderPassState waterState;
        waterState.reflectionTexture = waterNormalTexture_;
        waterState.shadowTexture = reflectionTexture;
        device.setPassState(waterState);
        auto waterPipeline = pipeline;
        waterPipeline.writeDepth = false;
        waterPipeline.depthTest = false;
        waterPipeline.faceCulling =
            PipelineState::FaceCulling::None;
        waterPipeline.blendMode =
            PipelineState::BlendMode::Alpha;
        MaterialState waterMaterial;
        waterMaterial.color = {0.0F, 0.2F, 0.5F, 1.0F};
        waterMaterial.alphaReference =
            std::fmod(std::max(elapsedSeconds, 0.0F) * 0.15F,
                      1.0F);
        waterMaterial.emissive = 0.1F;
        waterMaterial.textureTransform =
            {1.0F, 1.0F, 0.0F, 0.0F};
        r3d::physics::Transform water;
        water.position = environmentSurfaceCenter_;
        water.scale = environmentSurfaceSize_;
        device.draw(effectMesh_, waterShader_, sourceDepth,
                    transform(water), waterPipeline, {},
                    waterMaterial);
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
    device.beginPass(RenderPass::Composite, {}, postCamera, clearRgba,
                     false, false);
    device.draw(postProcessMesh_, toneMapShader_, hdrTexture,
                postTransform, postPipeline, {}, postMaterial);
}

} // namespace rrr3d::race
