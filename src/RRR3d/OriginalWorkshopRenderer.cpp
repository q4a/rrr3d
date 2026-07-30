#include "OriginalWorkshopRenderer.h"

#include "OriginalMainMenu.h"
#include "resource/ResourceFileSystem.h"

#include <algorithm>
#include <array>
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

std::vector<StaticMeshVertex> vertices(
    const r3d::resource::R3DMeshAsset& mesh)
{
    std::vector<StaticMeshVertex> result;
    result.reserve(mesh.vertices.size());
    for (const auto& vertex : mesh.vertices)
    {
        result.push_back(
            {vertex.position[0], vertex.position[1], vertex.position[2],
             vertex.normal[0], vertex.normal[1], vertex.normal[2],
             vertex.texcoord[0], vertex.texcoord[1], vertex.tangent[0],
             vertex.tangent[1], vertex.tangent[2], vertex.bitangent[0],
             vertex.bitangent[1], vertex.bitangent[2]});
    }
    return result;
}

struct Quat
{
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float w = 1.0F;
};

Quat multiply(const Quat& left, const Quat& right) noexcept
{
    return {
        left.w * right.x + left.x * right.w +
            left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z +
            left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y -
            left.y * right.x + left.z * right.w,
        left.w * right.w - left.x * right.x -
            left.y * right.y - left.z * right.z};
}

Quat axisAngle(float x, float y, float z, float radians) noexcept
{
    const float half = radians * 0.5F;
    const float sine = std::sin(half);
    return {x * sine, y * sine, z * sine, std::cos(half)};
}

std::array<float, 3> rotate(
    const Quat& rotation, const std::array<float, 3>& point) noexcept
{
    const std::array<float, 3> q{rotation.x, rotation.y, rotation.z};
    const std::array<float, 3> cross1{
        q[1] * point[2] - q[2] * point[1],
        q[2] * point[0] - q[0] * point[2],
        q[0] * point[1] - q[1] * point[0]};
    const std::array<float, 3> cross2{
        q[1] * cross1[2] - q[2] * cross1[1],
        q[2] * cross1[0] - q[0] * cross1[2],
        q[0] * cross1[1] - q[1] * cross1[0]};
    return {
        point[0] + 2.0F *
                           (rotation.w * cross1[0] + cross2[0]),
        point[1] + 2.0F *
                           (rotation.w * cross1[1] + cross2[1]),
        point[2] + 2.0F *
                           (rotation.w * cross1[2] + cross2[2])};
}

Quat sourceRotation(float animation) noexcept
{
    constexpr float pi = 3.14159265358979323846F;
    // Menu::GetIsoRot returns rotX(-pi/3) * rotY(0) * rotZ(-2pi/3).
    const auto iso = multiply(
        axisAngle(1.0F, 0.0F, 0.0F, -pi / 3.0F),
        axisAngle(0.0F, 0.0F, 1.0F, -2.0F * pi / 3.0F));
    return multiply(
        iso, axisAngle(0.0F, 0.0F, 1.0F, animation));
}

Transform sourceViewTransform(
    const r3d::resource::R3DMeshAsset& mesh, float centerX,
    float centerY, float width, float height, float animation) noexcept
{
    const auto rotation = sourceRotation(animation);
    std::array<float, 3> minimum{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()};
    std::array<float, 3> maximum{
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()};
    for (std::size_t corner = 0; corner < 8U; ++corner)
    {
        const std::array<float, 3> point{
            (corner & 1U) != 0U ? mesh.maximum[0] : mesh.minimum[0],
            (corner & 2U) != 0U ? mesh.maximum[1] : mesh.minimum[1],
            (corner & 4U) != 0U ? mesh.maximum[2] : mesh.minimum[2]};
        const auto rotated = rotate(rotation, point);
        for (std::size_t axis = 0; axis < 3U; ++axis)
        {
            minimum[axis] = std::min(minimum[axis], rotated[axis]);
            maximum[axis] = std::max(maximum[axis], rotated[axis]);
        }
    }
    std::array<float, 3> size{};
    std::array<float, 3> center{};
    float sizeLengthSquared = 0.0F;
    float centerLengthSquared = 0.0F;
    for (std::size_t axis = 0; axis < 3U; ++axis)
    {
        size[axis] = maximum[axis] - minimum[axis];
        center[axis] = (maximum[axis] + minimum[axis]) * 0.5F;
        sizeLengthSquared += size[axis] * size[axis];
        centerLengthSquared += center[axis] * center[axis];
    }
    // View3d defaults to align=false.  Context::DrawView3d therefore keeps
    // the mesh origin fixed and adds the rotated AABB center length.
    const float maximumScale =
        std::max(std::sqrt(sizeLengthSquared) +
                     std::sqrt(centerLengthSquared),
                 0.0001F);
    const float scaleX = width / maximumScale;
    const float scaleY = -height / maximumScale;
    const float scaleZ =
        ((width + height) * 0.5F) / maximumScale;

    const float xx = rotation.x * rotation.x;
    const float yy = rotation.y * rotation.y;
    const float zz = rotation.z * rotation.z;
    const float xy = rotation.x * rotation.y;
    const float xz = rotation.x * rotation.z;
    const float yz = rotation.y * rotation.z;
    const float wx = rotation.w * rotation.x;
    const float wy = rotation.w * rotation.y;
    const float wz = rotation.w * rotation.z;

    Transform result;
    result.matrix = {
        scaleX * (1.0F - 2.0F * (yy + zz)),
        -scaleX * (2.0F * (xy + wz)),
        scaleX * (2.0F * (xz - wy)), 0.0F,
        scaleY * (2.0F * (xy - wz)),
        -scaleY * (1.0F - 2.0F * (xx + zz)),
        scaleY * (2.0F * (yz + wx)), 0.0F,
        scaleZ * (2.0F * (xz + wy)),
        -scaleZ * (2.0F * (yz - wx)),
        scaleZ * (1.0F - 2.0F * (xx + yy)), 0.0F,
        centerX, centerY, 40.0F, 1.0F};
    return result;
}

} // namespace

bool OriginalWorkshopRenderer::initialize(
    GraphicsDevice& device,
    const r3d::resource::ResourceFileSystem& resources,
    const r3d::game::originalrace::OriginalGarageCatalog& catalog,
    std::string& error)
{
    error.clear();
    shutdown(device);
    try
    {
        assets_.reserve(catalog.workshop.size());
        for (const auto& item : catalog.workshop)
        {
            if (item.meshPath.empty() || item.texturePath.empty())
            {
                throw r3d::resource::ResourceError(
                    "workshop.xml item has no original mesh/texture: " +
                    item.record);
            }
            Asset asset;
            asset.record = item.record;
            asset.source = r3d::resource::loadR3DMeshAsset(
                resources, item.meshPath);
            const auto gpuVertices = vertices(asset.source);
            asset.mesh = device.createMesh(
                gpuVertices.data(), gpuVertices.size(),
                asset.source.indices.data(), asset.source.indices.size());
            const auto image =
                r3d::game::mainmenu2::loadOriginalImage(
                    resources, item.texturePath);
            asset.texture =
                image.storage ==
                        r3d::game::mainmenu2::ImageStorage::EncodedContainer
                    ? device.createTextureContainer(
                          image.bytes.data(), image.bytes.size(),
                          image.virtualPath)
                    : device.createTextureRgba8(
                          image.width, image.height, image.bytes.data(),
                          image.bytes.size());
            if (!valid(asset.mesh) || !valid(asset.texture))
            {
                throw r3d::resource::ResourceError(
                    "unable to upload original WorkshopFrame item " +
                    item.record);
            }
            assets_.push_back(std::move(asset));
        }
        return true;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        shutdown(device);
        return false;
    }
}

void OriginalWorkshopRenderer::shutdown(
    GraphicsDevice& device) noexcept
{
    for (auto& asset : assets_)
    {
        if (valid(asset.mesh))
            device.destroy(asset.mesh);
        if (valid(asset.texture))
            device.destroy(asset.texture);
    }
    assets_.clear();
}

void OriginalWorkshopRenderer::drawItem(
    GraphicsDevice& device, Shader shader,
    const r3d::game::originalrace::OriginalWorkshopItem& item,
    float centerX, float centerY, float width, float height,
    float rotationRadians, const PipelineState& sourcePipeline) const
{
    const auto found = std::find_if(
        assets_.begin(), assets_.end(),
        [&](const auto& asset) { return asset.record == item.record; });
    if (found == assets_.end())
        return;
    auto pipeline = sourcePipeline;
    pipeline.writeDepth = true;
    pipeline.depthTest = true;
    pipeline.faceCulling = PipelineState::FaceCulling::None;
    pipeline.blendMode = PipelineState::BlendMode::Opaque;
    pipeline.alphaBlend = false;
    MaterialState material;
    material.ignoreFog = true;
    material.specular = 0.0F;
    const auto transform = sourceViewTransform(
        found->source, centerX, centerY, width, height,
        rotationRadians);
    if (found->source.materialGroups.empty())
    {
        device.draw(found->mesh, shader, found->texture, transform,
                    pipeline, {}, material);
        return;
    }
    for (const auto& group : found->source.materialGroups)
    {
        device.draw(
            found->mesh, shader, found->texture, transform, pipeline,
            {group.firstIndex, group.indexCount}, material);
    }
}

} // namespace rrr3d::race
