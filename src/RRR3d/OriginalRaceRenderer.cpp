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
    if (asset.textures.empty())
        return;
    if (asset.source.materialGroups.empty())
    {
        device.draw(asset.mesh, shader, asset.textures.front(), model,
                    pipeline);
        return;
    }
    if (asset.subMesh >= 0 &&
        static_cast<std::size_t>(asset.subMesh) <
            asset.source.materialGroups.size())
    {
        const auto& group =
            asset.source.materialGroups[static_cast<std::size_t>(
                asset.subMesh)];
        device.draw(asset.mesh, shader, asset.textures.front(), model,
                    pipeline, {group.firstIndex, group.indexCount});
        return;
    }
    for (std::size_t index = 0; index < asset.source.materialGroups.size();
         ++index)
    {
        const auto& group = asset.source.materialGroups[index];
        const std::size_t materialIndex =
            std::min(index, asset.materials.size() - 1U);
        const std::size_t textureIndex =
            std::min(index, asset.textures.size() - 1U);
        auto groupPipeline = pipeline;
        const auto blend = asset.materials[materialIndex].blend;
        if (blend == r3d::game::originalrace::MaterialBlend::Transparency ||
            blend == r3d::game::originalrace::MaterialBlend::Additive)
        {
            groupPipeline.alphaBlend = true;
            groupPipeline.writeDepth = false;
            groupPipeline.faceCulling = PipelineState::FaceCulling::None;
        }
        device.draw(asset.mesh, shader, asset.textures[textureIndex], model,
                    groupPipeline,
                    {group.firstIndex, group.indexCount});
    }
}

} // namespace

bool OriginalRaceRenderer::initialize(
    GraphicsDevice& device,
    const r3d::resource::ResourceFileSystem& resources,
    const r3d::game::originalrace::Race& race, std::string& error)
{
    try
    {
        auto load = [&](Asset& asset,
                        const r3d::game::originalrace::VisualNode& node) {
            asset.source = r3d::resource::loadR3DMeshAsset(resources,
                                                           node.meshPath);
            const auto gpuVertices = vertices(asset.source);
            asset.mesh = device.createMesh(
                gpuVertices.data(), gpuVertices.size(),
                asset.source.indices.data(), asset.source.indices.size());
            asset.materials = node.materials;
            asset.subMesh = node.subMesh;
            for (const auto& material : node.materials)
            {
                const auto texture =
                    resources.readBinary(material.texturePath);
                asset.textures.push_back(device.createTextureContainer(
                    texture.data(), texture.size(),
                    material.texturePath));
            }
            if (!valid(asset.mesh) || asset.textures.empty() ||
                std::any_of(asset.textures.begin(), asset.textures.end(),
                            [](Texture value) { return !valid(value); }))
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

        tracks_.resize(race.trackDefinitions.size());
        for (std::size_t index = 0; index < tracks_.size(); ++index)
            loadObject(tracks_[index],
                       race.trackDefinitions[index].visualNodes);
        decorations_.resize(race.decorationDefinitions.size());
        for (std::size_t index = 0; index < decorations_.size(); ++index)
            loadObject(decorations_[index],
                       race.decorationDefinitions[index].visualNodes);
        bonuses_.resize(race.bonuses.size());
        for (std::size_t index = 0; index < bonuses_.size(); ++index)
            loadObject(bonuses_[index], race.bonuses[index].visual.visualNodes);

        vehicleBodies_.resize(race.vehicles.size());
        vehicleWheels_.resize(race.vehicles.size());
        for (std::size_t vehicle = 0; vehicle < race.vehicles.size();
             ++vehicle)
        {
            loadObject(vehicleBodies_[vehicle],
                       race.vehicles[vehicle].bodyVisuals);
            vehicleWheels_[vehicle].resize(
                race.vehicles[vehicle].wheelVisuals.size());
            for (std::size_t wheel = 0;
                 wheel < race.vehicles[vehicle].wheelVisuals.size(); ++wheel)
            {
                auto& object = vehicleWheels_[vehicle][wheel];
                object.nodes.resize(1);
                load(object.nodes.front(),
                     race.vehicles[vehicle].wheelVisuals[wheel]);
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
        constexpr std::array<StaticMeshVertex, 4> rainVertices{{
            {-0.025F, 0.0F, -1.0F, 0.0F, -1.0F, 0.0F, 0.0F, 1.0F},
            {0.025F, 0.0F, -1.0F, 0.0F, -1.0F, 0.0F, 1.0F, 1.0F},
            {-0.025F, 0.0F, 1.0F, 0.0F, -1.0F, 0.0F, 0.0F, 0.0F},
            {0.025F, 0.0F, 1.0F, 0.0F, -1.0F, 0.0F, 1.0F, 0.0F},
        }};
        constexpr std::array<std::uint32_t, 6> rainIndices{
            {0, 1, 2, 1, 3, 2}};
        constexpr std::array<std::uint8_t, 16> rainPixels{{
            190, 215, 235, 145, 190, 215, 235, 145,
            190, 215, 235, 145, 190, 215, 235, 145,
        }};
        rainMesh_ = device.createMesh(
            rainVertices.data(), rainVertices.size(),
            rainIndices.data(), rainIndices.size());
        rainTexture_ = device.createTextureRgba8(
            2, 2, rainPixels.data(), rainPixels.size());
        if (!valid(rainMesh_) || !valid(rainTexture_))
            throw r3d::resource::ResourceError(
                "Unable to create original rainy-weather particles");
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
    };
    for (auto& body : vehicleBodies_)
        releaseObject(body);
    for (auto& wheels : vehicleWheels_)
        for (auto& wheel : wheels)
            releaseObject(wheel);
    for (auto& bonus : bonuses_)
        releaseObject(bonus);
    for (auto& decoration : decorations_)
        releaseObject(decoration);
    for (auto& track : tracks_)
        releaseObject(track);
    vehicleBodies_.clear();
    vehicleWheels_.clear();
    bonuses_.clear();
    decorations_.clear();
    tracks_.clear();
    if (valid(skyTexture_))
        device.destroy(skyTexture_);
    if (valid(skyMesh_))
        device.destroy(skyMesh_);
    if (valid(rainTexture_))
        device.destroy(rainTexture_);
    if (valid(rainMesh_))
        device.destroy(rainMesh_);
    skyTexture_ = {};
    skyMesh_ = {};
    rainTexture_ = {};
    rainMesh_ = {};
}

Camera OriginalRaceRenderer::makeCamera(
    const GraphicsDevice& device, const r3d::physics::VehicleState& vehicle,
    std::uint32_t width, std::uint32_t height) const noexcept
{
    auto forward = rotateX(vehicle.body.rotation);
    const float velocityLength = std::sqrt(
        vehicle.linearVelocity.x * vehicle.linearVelocity.x +
        vehicle.linearVelocity.y * vehicle.linearVelocity.y);
    forward.x += vehicle.linearVelocity.x * 0.1F;
    forward.y += vehicle.linearVelocity.y * 0.1F;
    const float forwardLength =
        std::sqrt(forward.x * forward.x + forward.y * forward.y);
    if (forwardLength > 0.0001F)
    {
        forward.x /= forwardLength;
        forward.y /= forwardLength;
    }
    const auto& position = vehicle.body.position;
    const float speedFactor =
        std::clamp(velocityLength / (150.0F / 3.6F), 0.0F, 1.0F);
    // CameraManager::csThirdPerson uses cCamTargetOff(-4.6, 0, 2.4),
    // an additional -1 m offset, and up to 1.5 m of speed pull-back.
    const float distance = 5.6F + speedFactor * speedFactor * 1.5F;
    const bx::Vec3 eye{position.x - forward.x * distance,
                       position.y - forward.y * distance,
                       position.z + 2.4F};
    const bx::Vec3 at{position.x + forward.x * 8.0F,
                      position.y + forward.y * 8.0F,
                      position.z + 2.4F};
    Camera camera;
    bx::mtxLookAt(camera.view.data(), eye, at, {0.0F, 0.0F, 1.0F},
                  bx::Handedness::Right);
    bx::mtxProj(camera.projection.data(), 75.0F,
                static_cast<float>(std::max(width, 1U)) /
                    static_cast<float>(std::max(height, 1U)),
                1.0F, 120.0F, device.usesHomogeneousDepth(),
                bx::Handedness::Right);
    return camera;
}

void OriginalRaceRenderer::draw(
    GraphicsDevice& device, Shader shader,
    const r3d::game::originalrace::Race& race,
    const std::vector<r3d::physics::VehicleState>& vehicles,
    const PipelineState& pipeline,
    const std::vector<bool>& decorationActive,
    const std::vector<bool>& bonusActive,
    float elapsedSeconds) const
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
            {vehicles.front().body.position.x,
             vehicles.front().body.position.y,
             vehicles.front().body.position.z + 2.4F, 1.0F};
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
        sky.scale = {90.0F, 90.0F, 90.0F};
        device.draw(skyMesh_, shader, skyTexture_, transform(sky),
                    skyPipeline);
    }
    device.setSceneLighting(sceneLighting);

    auto drawObject = [&](const ObjectAsset& asset,
                          const std::vector<
                              r3d::game::originalrace::VisualNode>& nodes,
                          const r3d::physics::Transform& parent) {
        const std::size_t count = std::min(asset.nodes.size(), nodes.size());
        for (std::size_t index = 0; index < count; ++index)
            drawGroups(device, asset.nodes[index], shader,
                       transform(compose(parent, nodes[index].transform)),
                       pipeline);
    };
    for (const auto& instance : race.trackInstances)
    {
        const auto& definition =
            race.trackDefinitions.at(instance.definition);
        drawObject(tracks_.at(instance.definition),
                   definition.visualNodes, instance.transform);
    }

    for (std::size_t index = 0; index < race.decorationInstances.size();
         ++index)
    {
        if (index < decorationActive.size() && !decorationActive[index])
            continue;
        const auto& instance = race.decorationInstances[index];
        const auto& definition =
            race.decorationDefinitions.at(instance.definition);
        drawObject(decorations_.at(instance.definition),
                   definition.visualNodes, instance.transform);
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
        drawObject(bonuses_.at(index),
                   race.bonuses[index].visual.visualNodes, animated);
    }

    const std::size_t racerCount =
        std::min(vehicles.size(), race.racers.size());
    for (std::size_t racer = 0; racer < racerCount; ++racer)
    {
        const auto vehicleIndex = race.racers[racer].vehicle;
        if (vehicleIndex >= race.vehicles.size() ||
            vehicleIndex >= vehicleBodies_.size())
            continue;
        const auto& definition = race.vehicles[vehicleIndex];
        const auto& state = vehicles[racer];
        drawObject(vehicleBodies_[vehicleIndex],
                   definition.bodyVisuals, state.body);
        const std::size_t wheelCount = std::min(
            {state.wheels.size(), definition.wheelVisuals.size(),
             definition.wheelVisualOffsets.size(),
             vehicleWheels_[vehicleIndex].size()});
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
                vehicleWheels_[vehicleIndex][wheelIndex];
            if (!wheelAsset.nodes.empty())
                drawGroups(
                    device, wheelAsset.nodes.front(), shader,
                    transform(compose(
                        wheel,
                        definition.wheelVisuals[wheelIndex].transform)),
                    pipeline);
        }
    }

    if (race.environment.rain && !vehicles.empty())
    {
        auto rainPipeline = pipeline;
        rainPipeline.alphaBlend = true;
        rainPipeline.writeDepth = false;
        rainPipeline.faceCulling = PipelineState::FaceCulling::None;
        const auto center = vehicles.front().body.position;
        for (std::size_t index = 0; index < 48; ++index)
        {
            const float seed = static_cast<float>(index);
            r3d::physics::Transform drop;
            drop.position.x =
                center.x + std::sin(seed * 12.9898F) * 18.0F;
            drop.position.y =
                center.y + std::sin(seed * 78.233F) * 18.0F;
            const float fall =
                std::fmod(elapsedSeconds * 24.0F + seed * 3.7F, 22.0F);
            drop.position.z = center.z + 14.0F - fall;
            drop.scale = {1.0F, 1.0F, 1.8F};
            device.draw(rainMesh_, shader, rainTexture_,
                        transform(drop), rainPipeline);
        }
    }
}

} // namespace rrr3d::race
