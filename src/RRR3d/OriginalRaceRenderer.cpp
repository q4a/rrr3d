#include "OriginalRaceRenderer.h"

#include "resource/ResourceFileSystem.h"

#include <bx/math.h>

#include <algorithm>
#include <cmath>
#include <exception>

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
        result.push_back({vertex.position[0], vertex.position[1],
                          vertex.position[2], vertex.normal[0],
                          vertex.normal[1], vertex.normal[2],
                          vertex.texcoord[0], vertex.texcoord[1]});
    }
    return result;
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

void drawGroups(GraphicsDevice& device,
                const OriginalRaceRenderer::Asset& asset, Shader shader,
                const Transform& model, const PipelineState& pipeline)
{
    if (asset.source.materialGroups.empty())
    {
        device.draw(asset.mesh, shader, asset.texture, model, pipeline);
        return;
    }
    for (const auto& group : asset.source.materialGroups)
        device.draw(asset.mesh, shader, asset.texture, model, pipeline,
                    {group.firstIndex, group.indexCount});
}

} // namespace

bool OriginalRaceRenderer::initialize(
    GraphicsDevice& device,
    const r3d::resource::ResourceFileSystem& resources,
    const r3d::game::originalrace::Race& race, std::string& error)
{
    try
    {
        auto load = [&](Asset& asset, std::string_view meshPath,
                        std::string_view texturePath) {
            asset.source = r3d::resource::loadR3DMeshAsset(resources,
                                                           meshPath);
            const auto gpuVertices = vertices(asset.source);
            asset.mesh = device.createMesh(
                gpuVertices.data(), gpuVertices.size(),
                asset.source.indices.data(), asset.source.indices.size());
            const auto texture = resources.readBinary(texturePath);
            asset.texture = device.createTextureContainer(
                texture.data(), texture.size(), texturePath);
            if (!valid(asset.mesh) || !valid(asset.texture))
                throw r3d::resource::ResourceError(
                    "Unable to upload original race asset " +
                    std::string(meshPath));
        };

        tracks_.resize(race.trackDefinitions.size());
        for (std::size_t index = 0; index < tracks_.size(); ++index)
        {
            const auto& definition = race.trackDefinitions[index];
            load(tracks_[index], definition.visualMeshPath,
                 definition.texturePath);
        }
        load(body_, race.vehicle.bodyMeshPath, race.vehicle.texturePath);
        load(wheel_, race.vehicle.wheelMeshPath, race.vehicle.texturePath);
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
    auto release = [&](Asset& asset) {
        if (valid(asset.texture))
            device.destroy(asset.texture);
        if (valid(asset.mesh))
            device.destroy(asset.mesh);
        asset = {};
    };
    release(wheel_);
    release(body_);
    for (auto& track : tracks_)
        release(track);
    tracks_.clear();
}

Camera OriginalRaceRenderer::makeCamera(
    const GraphicsDevice& device, const r3d::physics::VehicleState& vehicle,
    std::uint32_t width, std::uint32_t height) const noexcept
{
    const auto forward = rotateX(vehicle.body.rotation);
    const auto& position = vehicle.body.position;
    const bx::Vec3 eye{position.x - forward.x * 12.0F,
                       position.y - forward.y * 12.0F,
                       position.z + 6.0F};
    const bx::Vec3 at{position.x + forward.x * 5.0F,
                      position.y + forward.y * 5.0F,
                      position.z + 1.0F};
    Camera camera;
    bx::mtxLookAt(camera.view.data(), eye, at, {0.0F, 0.0F, 1.0F},
                  bx::Handedness::Right);
    bx::mtxProj(camera.projection.data(), 62.0F,
                static_cast<float>(std::max(width, 1U)) /
                    static_cast<float>(std::max(height, 1U)),
                0.1F, 600.0F, device.usesHomogeneousDepth(),
                bx::Handedness::Right);
    return camera;
}

void OriginalRaceRenderer::draw(
    GraphicsDevice& device, Shader shader,
    const r3d::game::originalrace::Race& race,
    const r3d::physics::VehicleState& vehicle,
    const PipelineState& pipeline) const
{
    for (const auto& instance : race.trackInstances)
    {
        const auto& definition =
            race.trackDefinitions.at(instance.definition);
        drawGroups(device, tracks_.at(instance.definition), shader,
                   transform(compose(instance.transform,
                                     definition.visualTransform)),
                   pipeline);
    }
    drawGroups(device, body_, shader,
               transform(compose(vehicle.body,
                                 race.vehicle.bodyVisualTransform)),
               pipeline);
    for (std::size_t index = 0; index < vehicle.wheels.size(); ++index)
    {
        auto wheel = vehicle.wheels[index];
        const auto offset = rotate(
            vehicle.body.rotation,
            race.vehicle.wheelVisualOffsets.at(index));
        wheel.position.x += offset.x;
        wheel.position.y += offset.y;
        wheel.position.z += offset.z;
        drawGroups(device, wheel_, shader,
                   transform(compose(
                       wheel,
                       race.vehicle.wheelVisualTransforms.at(index))),
                   pipeline);
    }
}

} // namespace rrr3d::race
