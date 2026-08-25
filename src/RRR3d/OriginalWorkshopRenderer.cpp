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
using SourceTransform = r3d::game::originalrace::Transform;

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

using Quat = r3d::game::originalrace::Quat;
using Vec3 = r3d::game::originalrace::Vec3;

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

Vec3 rotate(const Quat& rotation, const Vec3& point) noexcept
{
    const Vec3 twiceCross{
        2.0F * (rotation.y * point.z - rotation.z * point.y),
        2.0F * (rotation.z * point.x - rotation.x * point.z),
        2.0F * (rotation.x * point.y - rotation.y * point.x)};
    return {
        point.x + rotation.w * twiceCross.x +
            (rotation.y * twiceCross.z -
             rotation.z * twiceCross.y),
        point.y + rotation.w * twiceCross.y +
            (rotation.z * twiceCross.x -
             rotation.x * twiceCross.z),
        point.z + rotation.w * twiceCross.z +
            (rotation.x * twiceCross.y -
             rotation.y * twiceCross.x)};
}

Vec3 transformPoint(const SourceTransform& transform,
                    const Vec3& point) noexcept
{
    const auto rotated = rotate(
        transform.rotation,
        {point.x * transform.scale.x,
         point.y * transform.scale.y,
         point.z * transform.scale.z});
    return {rotated.x + transform.position.x,
            rotated.y + transform.position.y,
            rotated.z + transform.position.z};
}

enum class SourceView
{
    Workshop,
    Planet,
    Car,
};

Quat sourceRotation(SourceView view, float animation) noexcept
{
    constexpr float pi = 3.14159265358979323846F;
    const auto animated =
        axisAngle(0.0F, 0.0F, 1.0F, animation);
    if (view == SourceView::Planet)
    {
        return multiply(
            axisAngle(1.0F, 0.0F, 0.0F, pi * 0.5F),
            animated);
    }
    if (view == SourceView::Car)
        return animated;
    // Menu::GetIsoRot returns rotX(-pi/3) * rotY(0) * rotZ(-2pi/3).
    const auto iso = multiply(
        axisAngle(1.0F, 0.0F, 0.0F, -pi / 3.0F),
        axisAngle(0.0F, 0.0F, 1.0F, -2.0F * pi / 3.0F));
    return multiply(iso, animated);
}

Transform sourceViewTransform(
    const std::array<float, 3>& sourceMinimum,
    const std::array<float, 3>& sourceMaximum,
    float centerX, float centerY, float width, float height,
    SourceView view, float animation) noexcept
{
    const auto rotation = sourceRotation(view, animation);
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
        const auto rotated = rotate(
            rotation,
            {(corner & 1U) != 0U ? sourceMaximum[0]
                                 : sourceMinimum[0],
             (corner & 2U) != 0U ? sourceMaximum[1]
                                 : sourceMinimum[1],
             (corner & 4U) != 0U ? sourceMaximum[2]
                                 : sourceMinimum[2]});
        const std::array<float, 3> value{
            rotated.x, rotated.y, rotated.z};
        for (std::size_t axis = 0; axis < 3U; ++axis)
        {
            minimum[axis] = std::min(minimum[axis], value[axis]);
            maximum[axis] = std::max(maximum[axis], value[axis]);
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
    // View3d defaults to align=false. Context::DrawView3d therefore keeps
    // the model origin and includes the rotated AABB centre in the fit.
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

    // ContextInfo's csViewPort projection maps the source -500..500 camera
    // range into depth.  The portable overlay camera instead spans 0..100.
    // Compress the completed transform's output Z row, not the model's local
    // Z scale: Planet applies a 90-degree X rotation, so scaling local Z
    // flattens the sphere vertically instead of reducing viewport depth.
    result.matrix[2] *= 0.1F;
    result.matrix[6] *= 0.1F;
    result.matrix[10] *= 0.1F;
    result.matrix[14] = 50.0F;
    return result;
}

Transform localTransform(const SourceTransform& source) noexcept
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

Transform compose(const Transform& parent,
                  const Transform& local) noexcept
{
    Transform result;
    for (std::size_t column = 0; column < 4U; ++column)
    {
        for (std::size_t row = 0; row < 4U; ++row)
        {
            float value = 0.0F;
            for (std::size_t index = 0; index < 4U; ++index)
            {
                value += parent.matrix[index * 4U + row] *
                         local.matrix[column * 4U + index];
            }
            result.matrix[column * 4U + row] = value;
        }
    }
    return result;
}

Texture uploadTexture(
    GraphicsDevice& device,
    const r3d::resource::ResourceFileSystem& resources,
    const std::string& path)
{
    const auto image =
        r3d::game::mainmenu2::loadOriginalImage(resources, path);
    return image.storage ==
                   r3d::game::mainmenu2::ImageStorage::EncodedContainer
               ? device.createTextureContainer(
                     image.bytes.data(), image.bytes.size(),
                     image.virtualPath)
               : device.createTextureRgba8(
                     image.width, image.height, image.bytes.data(),
                     image.bytes.size());
}

} // namespace

bool OriginalWorkshopRenderer::initialize(
    GraphicsDevice& device,
    const r3d::resource::ResourceFileSystem& resources,
    const r3d::game::originalrace::OriginalGarageCatalog& catalog,
    const r3d::game::originalrace::Race& race,
    std::string& error)
{
    error.clear();
    shutdown(device);
    try
    {
        auto addNode =
            [&](ModelAsset& model, std::string_view meshPath,
                const std::vector<std::string>& textures,
                const SourceTransform& local) {
                if (meshPath.empty() || textures.empty())
                {
                    throw r3d::resource::ResourceError(
                        model.record +
                        ": original viewport node has no mesh/material");
                }
                NodeAsset node;
                node.source = r3d::resource::loadR3DMeshAsset(
                    resources, meshPath);
                const auto gpuVertices = vertices(node.source);
                node.mesh = device.createMesh(
                    gpuVertices.data(), gpuVertices.size(),
                    node.source.indices.data(),
                    node.source.indices.size());
                for (const auto& texture : textures)
                    node.textures.push_back(
                        uploadTexture(device, resources, texture));
                node.local = local;
                if (!valid(node.mesh) || node.textures.empty() ||
                    std::any_of(
                        node.textures.begin(), node.textures.end(),
                        [](Texture value) { return !valid(value); }))
                {
                    throw r3d::resource::ResourceError(
                        model.record +
                        ": unable to upload original viewport node");
                }
                model.nodes.push_back(std::move(node));
            };
        auto finishBounds = [&](ModelAsset& model) {
            model.minimum.fill(std::numeric_limits<float>::max());
            model.maximum.fill(std::numeric_limits<float>::lowest());
            for (const auto& node : model.nodes)
            {
                for (std::size_t corner = 0; corner < 8U; ++corner)
                {
                    const auto point = transformPoint(
                        node.local,
                        {(corner & 1U) != 0U
                             ? node.source.maximum[0]
                             : node.source.minimum[0],
                         (corner & 2U) != 0U
                             ? node.source.maximum[1]
                             : node.source.minimum[1],
                         (corner & 4U) != 0U
                             ? node.source.maximum[2]
                             : node.source.minimum[2]});
                    const std::array<float, 3> value{
                        point.x, point.y, point.z};
                    for (std::size_t axis = 0; axis < 3U; ++axis)
                    {
                        model.minimum[axis] =
                            std::min(model.minimum[axis], value[axis]);
                        model.maximum[axis] =
                            std::max(model.maximum[axis], value[axis]);
                    }
                }
            }
            if (model.nodes.empty())
            {
                throw r3d::resource::ResourceError(
                    model.record +
                    ": original viewport model has no nodes");
            }
        };

        workshopAssets_.reserve(catalog.workshop.size());
        for (const auto& item : catalog.workshop)
        {
            ModelAsset model;
            model.record = item.record;
            addNode(
                model, item.meshPath, {item.texturePath}, {});
            finishBounds(model);
            workshopAssets_.push_back(std::move(model));
        }

        planetAssets_.reserve(
            catalog.planets.size() + catalog.gamers.size());
        auto addPlanet = [&](const auto& planet) {
            ModelAsset model;
            model.record = planet.record;
            addNode(
                model, planet.meshPath, {planet.texturePath}, {});
            finishBounds(model);
            planetAssets_.push_back(std::move(model));
        };
        for (const auto& planet : catalog.planets)
            addPlanet(planet);
        for (const auto& gamer : catalog.gamers)
            addPlanet(gamer);

        carAssets_.reserve(race.vehicles.size());
        for (const auto& car : race.vehicles)
        {
            ModelAsset model;
            model.record = car.record;
            auto addVisual =
                [&](const auto& visual,
                    const SourceTransform& local) {
                std::vector<std::string> textures;
                for (const auto& material : visual.materials)
                {
                    if (!material.texturePath.empty())
                        textures.push_back(material.texturePath);
                }
                if (textures.empty() && !car.texturePath.empty())
                    textures.push_back(car.texturePath);
                addNode(
                    model, visual.meshPath, textures,
                    local);
            };
            for (const auto& visual : car.bodyVisuals)
            {
                // RaceMenu::CreateCar creates Garage::BodyMeshes directly;
                // it does not apply the ctCar actor-node transform used by
                // the in-race renderer.
                addVisual(visual, {});
            }
            for (std::size_t index = 0U;
                 index < car.wheelVisuals.size(); ++index)
            {
                SourceTransform wheel;
                if (index < car.physics.wheels.size())
                {
                    wheel.position =
                        car.physics.wheels[index].position;
                    // RaceMenu2 mirrors the wheels on the negative-Y side.
                    if (wheel.position.y < 0.0F)
                        wheel.scale.y = -1.0F;
                }
                addVisual(car.wheelVisuals[index], wheel);
            }
            finishBounds(model);
            carAssets_.push_back(std::move(model));
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
    auto destroy = [&](auto& models) {
        for (auto& model : models)
        {
            for (auto& node : model.nodes)
            {
                if (valid(node.mesh))
                    device.destroy(node.mesh);
                for (const auto texture : node.textures)
                {
                    if (valid(texture))
                        device.destroy(texture);
                }
            }
        }
        models.clear();
    };
    destroy(carAssets_);
    destroy(planetAssets_);
    destroy(workshopAssets_);
}

namespace
{

template <typename Models>
void drawModel(
    GraphicsDevice& device, Shader shader,
    const Models& models, std::string_view record,
    float centerX, float centerY, float width, float height,
    SourceView view, float rotationRadians,
    const PipelineState& sourcePipeline,
    const std::array<float, 4>* firstNodeColor = nullptr)
{
    const auto found = std::find_if(
        models.begin(), models.end(),
        [&](const auto& asset) { return asset.record == record; });
    if (found == models.end())
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
    const auto viewTransform = sourceViewTransform(
        found->minimum, found->maximum, centerX, centerY,
        width, height, view, rotationRadians);
    for (std::size_t nodeIndex = 0U;
         nodeIndex < found->nodes.size(); ++nodeIndex)
    {
        const auto& node = found->nodes[nodeIndex];
        auto nodeMaterial = material;
        if (nodeIndex == 0U && firstNodeColor != nullptr)
            nodeMaterial.color = *firstNodeColor;
        const auto transform =
            compose(viewTransform, localTransform(node.local));
        if (node.source.materialGroups.empty())
        {
            device.draw(
                node.mesh, shader, node.textures.front(), transform,
                pipeline, {}, nodeMaterial);
            continue;
        }
        for (std::size_t index = 0;
             index < node.source.materialGroups.size(); ++index)
        {
            const auto& group = node.source.materialGroups[index];
            device.draw(
                node.mesh, shader,
                node.textures[std::min(
                    index, node.textures.size() - 1U)],
                transform, pipeline,
                {group.firstIndex, group.indexCount}, nodeMaterial);
        }
    }
}

} // namespace

void OriginalWorkshopRenderer::drawItem(
    GraphicsDevice& device, Shader shader,
    const r3d::game::originalrace::OriginalWorkshopItem& item,
    float centerX, float centerY, float width, float height,
    float rotationRadians, const PipelineState& sourcePipeline) const
{
    drawModel(
        device, shader, workshopAssets_, item.record, centerX, centerY,
        width, height, SourceView::Workshop, rotationRadians,
        sourcePipeline);
}

void OriginalWorkshopRenderer::drawPlanet(
    GraphicsDevice& device, Shader shader,
    const r3d::game::originalrace::OriginalGaragePlanet& planet,
    float centerX, float centerY, float width, float height,
    float rotationRadians, const PipelineState& sourcePipeline) const
{
    drawModel(
        device, shader, planetAssets_, planet.record, centerX, centerY,
        width, height, SourceView::Planet, rotationRadians,
        sourcePipeline);
}

void OriginalWorkshopRenderer::drawCar(
    GraphicsDevice& device, Shader shader, std::string_view record,
    float centerX, float centerY, float width, float height,
    float rotationRadians, const PipelineState& sourcePipeline,
    const std::array<float, 4>* color) const
{
    drawModel(
        device, shader, carAssets_, record, centerX, centerY,
        width, height, SourceView::Car, rotationRadians,
        sourcePipeline, color);
}

} // namespace rrr3d::race
