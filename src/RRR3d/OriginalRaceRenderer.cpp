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

constexpr std::array<StaticMeshVertex, 4> effectVertices{{
    {-0.5F, -0.5F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F},
    {0.5F, -0.5F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F},
    {-0.5F, 0.5F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F},
    {0.5F, 0.5F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 0.0F},
}};

constexpr std::array<std::uint32_t, 6> effectIndices{{0, 1, 2, 1, 3, 2}};

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

std::array<float, 4> effectTextureTransform(std::string_view path,
                                            float seconds)
{
    const auto name = recordName(path);
    if (name == "engine1.dds" || name == "shield1.dds")
        return animatedAtlas(5, 2, seconds);
    if (name == "explosion2.dds")
        return animatedAtlas(4, 4, seconds);
    if (name == "explosion3.dds")
        return animatedAtlas(6, 6, seconds);
    if (name == "explosion4.dds")
        return animatedAtlas(7, 7, seconds);
    if (name == "gunEff2.dds")
        return animatedAtlas(4, 1, seconds);
    return {1.0F, 1.0F, 0.0F, 0.0F};
}

void drawGroups(GraphicsDevice& device,
                const OriginalRaceRenderer::Asset& asset, Shader shader,
                const Transform& model, const PipelineState& pipeline,
                float elapsedSeconds = 0.0F)
{
    if (asset.textures.empty())
        return;
    auto materialState = [elapsedSeconds](const auto& material) {
        MaterialState state;
        state.color = material.color;
        state.alphaReference = material.alphaReference;
        state.emissive = material.emissive;
        state.specular = material.specular;
        state.shininess = material.shininess;
        state.ignoreFog = material.ignoreFog;
        state.textureTransform = animatedAtlas(
            material.atlasColumns, material.atlasRows, elapsedSeconds,
            material.animationRate);
        return state;
    };
    auto materialPipeline = [&](std::size_t index) {
        auto result = pipeline;
        if (asset.materials.empty())
            return result;
        const auto blend =
            asset.materials[std::min(
                index, asset.materials.size() - 1U)].blend;
        if (blend ==
                r3d::game::originalrace::MaterialBlend::Transparency ||
            blend == r3d::game::originalrace::MaterialBlend::Additive)
        {
            result.blendMode =
                blend ==
                        r3d::game::originalrace::MaterialBlend::Additive
                    ? PipelineState::BlendMode::Additive
                    : PipelineState::BlendMode::Alpha;
            result.writeDepth = false;
            result.faceCulling = PipelineState::FaceCulling::None;
        }
        return result;
    };
    if (asset.source.materialGroups.empty())
    {
        const auto material =
            asset.materials.empty()
                ? MaterialState{}
                : materialState(asset.materials.front());
        device.draw(asset.mesh, shader, asset.textures.front(), model,
                    materialPipeline(0U), {}, material);
        return;
    }
    if (asset.subMesh >= 0 &&
        static_cast<std::size_t>(asset.subMesh) <
            asset.source.materialGroups.size())
    {
        const auto& group =
            asset.source.materialGroups[static_cast<std::size_t>(
                asset.subMesh)];
        const auto material =
            asset.materials.empty()
                ? MaterialState{}
                : materialState(asset.materials.front());
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
        const std::size_t textureIndex =
            std::min(index, asset.textures.size() - 1U);
        const auto groupPipeline = materialPipeline(materialIndex);
        const auto material =
            materialState(asset.materials[materialIndex]);
        device.draw(asset.mesh, shader, asset.textures[textureIndex], model,
                    groupPipeline,
                    {group.firstIndex, group.indexCount}, material);
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
                loadObject(asset, definition.visualNodes);
                loadParticleTextures(
                    asset, definition.particleEmitters);
            };

        tracks_.resize(race.trackDefinitions.size());
        for (std::size_t index = 0; index < tracks_.size(); ++index)
            loadDefinition(tracks_[index],
                           race.trackDefinitions[index]);
        decorations_.resize(race.decorationDefinitions.size());
        for (std::size_t index = 0; index < decorations_.size(); ++index)
            loadDefinition(decorations_[index],
                           race.decorationDefinitions[index]);
        bonuses_.resize(race.bonuses.size());
        for (std::size_t index = 0; index < bonuses_.size(); ++index)
            loadDefinition(bonuses_[index],
                           race.bonuses[index].visual);

        weapons_.resize(race.weapons.size());
        for (std::size_t index = 0; index < weapons_.size(); ++index)
        {
            weapons_[index].nodes.resize(1);
            load(weapons_[index].nodes.front(),
                 race.weapons[index].visual);
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
            }
        }

        // A tournament opponent can use a wheel/upgrade set that differs
        // from another racer driving the same base car.  The Windows game
        // applies those slots per Player, so GPU assets are keyed by racer
        // rather than by the shared garage record.
        vehicleBodies_.resize(race.racers.size());
        vehicleWheels_.resize(race.racers.size());
        for (std::size_t racer = 0; racer < race.racers.size(); ++racer)
        {
            const auto& sourceRacer = race.racers[racer];
            const auto& vehicle =
                sourceRacer.hasConfiguredVehicle
                    ? sourceRacer.configuredVehicle
                    : race.vehicles.at(sourceRacer.vehicle);
            loadObject(vehicleBodies_[racer], vehicle.bodyVisuals);
            vehicleWheels_[racer].resize(vehicle.wheelVisuals.size());
            for (std::size_t wheel = 0;
                 wheel < vehicle.wheelVisuals.size(); ++wheel)
            {
                auto& object = vehicleWheels_[racer][wheel];
                object.nodes.resize(1);
                load(object.nodes.front(), vehicle.wheelVisuals[wheel]);
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
        weaponEffectTextures_.reserve(race.weapons.size());
        for (const auto& weapon : race.weapons)
        {
            const auto bytes =
                resources.readBinary(weapon.effectTexturePath);
            weaponEffectTextures_.push_back(
                device.createTextureContainer(
                    bytes.data(), bytes.size(),
                    weapon.effectTexturePath));
        }
        auto loadEffectTexture = [&](std::string_view path) {
            const auto bytes = resources.readBinary(path);
            return device.createTextureContainer(
                bytes.data(), bytes.size(), path);
        };
        destructionEffectTexture_ =
            loadEffectTexture("Data/Effect/explosion2.dds");
        engineSmokeTexture_ =
            loadEffectTexture("Data/Effect/smoke2.dds");
        shieldEffectTexture_ =
            loadEffectTexture("Data/Effect/shield1.dds");
        vehicleLightTexture_ =
            loadEffectTexture("Data/Effect/flare2b.dds");
        constexpr std::array<std::uint8_t, 16> shadowPixels{{
            0, 0, 0, 90, 0, 0, 0, 90,
            0, 0, 0, 90, 0, 0, 0, 90,
        }};
        shadowTexture_ = device.createTextureRgba8(
            2, 2, shadowPixels.data(), shadowPixels.size());
        if (!valid(rainMesh_) || !valid(rainTexture_) ||
            !valid(effectMesh_) ||
            !valid(destructionEffectTexture_) ||
            !valid(engineSmokeTexture_) ||
            !valid(shieldEffectTexture_) ||
            !valid(shadowTexture_) ||
            !valid(vehicleLightTexture_) ||
            (race.environment.surface !=
                     r3d::game::originalrace::EnvironmentSurface::None &&
             !valid(environmentSurfaceTexture_)) ||
            std::any_of(weaponEffectTextures_.begin(),
                        weaponEffectTextures_.end(),
                        [](Texture value) { return !valid(value); }))
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
    for (auto& bonus : bonuses_)
        releaseObject(bonus);
    for (auto& weapon : weapons_)
        releaseObject(weapon);
    for (auto& weapon : projectiles_)
        for (auto& projectile : weapon)
        {
            releaseObject(projectile.visual);
            releaseObject(projectile.secondaryVisual);
            releaseObject(projectile.tertiaryVisual);
        }
    for (auto& decoration : decorations_)
        releaseObject(decoration);
    for (auto& track : tracks_)
        releaseObject(track);
    vehicleBodies_.clear();
    vehicleWheels_.clear();
    bonuses_.clear();
    weapons_.clear();
    projectiles_.clear();
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
    for (const auto texture : weaponEffectTextures_)
        if (valid(texture))
            device.destroy(texture);
    if (valid(destructionEffectTexture_))
        device.destroy(destructionEffectTexture_);
    if (valid(engineSmokeTexture_))
        device.destroy(engineSmokeTexture_);
    if (valid(shieldEffectTexture_))
        device.destroy(shieldEffectTexture_);
    if (valid(shadowTexture_))
        device.destroy(shadowTexture_);
    if (valid(vehicleLightTexture_))
        device.destroy(vehicleLightTexture_);
    if (valid(environmentSurfaceTexture_))
        device.destroy(environmentSurfaceTexture_);
    if (valid(effectMesh_))
        device.destroy(effectMesh_);
    skyTexture_ = {};
    skyMesh_ = {};
    rainTexture_ = {};
    rainMesh_ = {};
    weaponEffectTextures_.clear();
    destructionEffectTexture_ = {};
    engineSmokeTexture_ = {};
    shieldEffectTexture_ = {};
    shadowTexture_ = {};
    vehicleLightTexture_ = {};
    environmentSurfaceTexture_ = {};
    effectMesh_ = {};
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
    const std::vector<bool>& bonusActive,
    const std::vector<r3d::game::originalrace::RacerRuntime>& racerRuntime,
    const std::vector<r3d::game::originalrace::RaceEffect>& effects,
    const std::vector<r3d::game::originalrace::MineRuntime>& mines,
    const std::vector<
        r3d::game::originalrace::ProjectileRuntime>& projectiles,
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
        sky.scale = {90.0F, 90.0F, 90.0F};
        device.draw(skyMesh_, shader, skyTexture_, transform(sky),
                    skyPipeline);
    }
    device.setSceneLighting(sceneLighting);

    if (valid(environmentSurfaceTexture_) &&
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
        device.draw(
            effectMesh_, shader, environmentSurfaceTexture_,
            transform(surface), surfacePipeline, {}, surfaceMaterial);
    }

    auto drawObject = [&](const ObjectAsset& asset,
                          const std::vector<
                              r3d::game::originalrace::VisualNode>& nodes,
                          const r3d::physics::Transform& parent) {
        const std::size_t count = std::min(asset.nodes.size(), nodes.size());
        for (std::size_t index = 0; index < count; ++index)
            drawGroups(device, asset.nodes[index], shader,
                       transform(compose(parent, nodes[index].transform)),
                       pipeline, elapsedSeconds);
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
            float sourceSpeed) {
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
                const float life = std::max(
                    (emitter.lifeMinimum + emitter.lifeMaximum) *
                        0.5F,
                    0.03F);
                float interval = std::max(
                    (emitter.startTimeMinimum +
                     emitter.startTimeMaximum) *
                        0.5F,
                    0.02F);
                if (emitter.distanceTriggered)
                    interval /= std::max(sourceSpeed, 1.0F);
                const float density = std::max(
                    (emitter.densityMinimum +
                     emitter.densityMaximum) *
                        0.5F,
                    1.0F);
                const float birthStep =
                    std::max(interval / density, 0.005F);
                const auto currentBirth =
                    static_cast<std::uint32_t>(
                        std::max(std::floor(age / birthStep), 0.0F));
                std::uint32_t particleCount =
                    static_cast<std::uint32_t>(
                        std::ceil(life / birthStep)) +
                    1U;
                if (emitter.maximumParticles > 0U)
                    particleCount = std::min(
                        particleCount,
                        emitter.maximumParticles);
                particleCount = std::min(particleCount, 32U);
                particleCount =
                    std::min(particleCount, currentBirth + 1U);
                const auto emitterWorld =
                    compose(parent, emitter.transform);
                for (std::uint32_t particleIndex = 0;
                     particleIndex < particleCount; ++particleIndex)
                {
                    const auto birthIndex =
                        currentBirth - particleIndex;
                    const float birth =
                        static_cast<float>(birthIndex) * birthStep;
                    if (emitter.startDuration > 0.0F &&
                        birth > emitter.startDuration)
                        continue;
                    const float particleAge =
                        std::max(age - birth, 0.0F);
                    const std::uint32_t seed =
                        birthIndex * 747796405U +
                        static_cast<std::uint32_t>(
                            emitterIndex) *
                            2891336453U;
                    const float sampledLife =
                        emitter.lifeMinimum +
                        (emitter.lifeMaximum -
                         emitter.lifeMinimum) *
                            unitNoise(seed + 17U);
                    const float activeLife =
                        std::max(sampledLife, 0.03F);
                    if (particleAge > activeLife)
                        continue;
                    auto position = rangeVector(
                        emitter.startPositionMinimum,
                        emitter.startPositionMaximum, seed + 31U);
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
                    if (emitter.worldCoordinates)
                        position.x -= sourceSpeed * particleAge;
                    auto scale = rangeVector(
                        emitter.startScaleMinimum,
                        emitter.startScaleMaximum, seed + 149U);
                    const auto scaleVelocity = rangeVector(
                        emitter.scaleVelocityMinimum,
                        emitter.scaleVelocityMaximum, seed + 193U);
                    scale.x = std::max(
                        scale.x + scaleVelocity.x * particleAge,
                        0.01F);
                    scale.y = std::max(
                        scale.y + scaleVelocity.y * particleAge,
                        0.01F);
                    scale.z = std::max(
                        scale.z + scaleVelocity.z * particleAge,
                        0.01F);
                    r3d::physics::Transform particle;
                    particle.position = position;
                    particle.scale = scale;
                    if (emitter.autoRotate)
                        particle.rotation =
                            directionRotation(velocity);
                    const std::size_t materialIndex =
                        particleIndex % emitter.materials.size();
                    const std::size_t textureIndex =
                        particleIndex % textures.size();
                    const auto& sourceMaterial =
                        emitter.materials[materialIndex];
                    MaterialState material;
                    material.color = sourceMaterial.color;
                    material.color[3] *= std::clamp(
                        1.0F - particleAge / activeLife,
                        0.0F, 1.0F);
                    material.alphaReference =
                        sourceMaterial.alphaReference;
                    material.emissive = sourceMaterial.emissive;
                    material.specular = sourceMaterial.specular;
                    material.shininess = sourceMaterial.shininess;
                    material.ignoreFog = sourceMaterial.ignoreFog;
                    material.textureTransform = animatedAtlas(
                        sourceMaterial.atlasColumns,
                        sourceMaterial.atlasRows, particleAge,
                        sourceMaterial.animationRate);
                    auto particlePipeline = pipeline;
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
                    particlePipeline.faceCulling =
                        PipelineState::FaceCulling::None;
                    device.draw(
                        effectMesh_, shader, textures[textureIndex],
                        transform(compose(emitterWorld, particle)),
                        particlePipeline, {}, material);
                }
            }
        };
    auto drawDefinition =
        [&](const ObjectAsset& asset,
            const r3d::game::originalrace::ObjectDefinition& definition,
            const r3d::physics::Transform& parent, float age,
            float sourceSpeed) {
            drawObject(asset, definition.visualNodes, parent);
            drawParticles(
                asset, definition, parent, age, sourceSpeed);
        };
    for (const auto& instance : race.trackInstances)
    {
        const auto& definition =
            race.trackDefinitions.at(instance.definition);
        drawDefinition(
            tracks_.at(instance.definition), definition,
            instance.transform, elapsedSeconds, 0.0F);
    }

    for (std::size_t index = 0; index < race.decorationInstances.size();
         ++index)
    {
        if (index < decorationActive.size() && !decorationActive[index])
            continue;
        const auto& instance = race.decorationInstances[index];
        const auto& definition =
            race.decorationDefinitions.at(instance.definition);
        drawDefinition(
            decorations_.at(instance.definition), definition,
            instance.transform, elapsedSeconds, 0.0F);
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
    auto shadowPipeline = pipeline;
    shadowPipeline.blendMode = PipelineState::BlendMode::Alpha;
    shadowPipeline.writeDepth = false;
    shadowPipeline.faceCulling = PipelineState::FaceCulling::None;
    MaterialState shadowMaterial;
    shadowMaterial.color = {1.0F, 1.0F, 1.0F, 0.36F};
    shadowMaterial.emissive = 1.0F;
    shadowMaterial.specular = 0.0F;
    auto lightPipeline = pipeline;
    lightPipeline.blendMode = PipelineState::BlendMode::Additive;
    lightPipeline.writeDepth = false;
    lightPipeline.faceCulling = PipelineState::FaceCulling::None;
    for (std::size_t racer = 0; racer < racerCount; ++racer)
    {
        const auto vehicleIndex = race.racers[racer].vehicle;
        if (vehicleIndex >= race.vehicles.size() ||
            racer >= vehicleBodies_.size())
            continue;
        const auto& definition =
            race.racers[racer].hasConfiguredVehicle
                ? race.racers[racer].configuredVehicle
                : race.vehicles[vehicleIndex];
        const auto& state = vehicles[racer];
        r3d::physics::Transform shadow = state.body;
        shadow.position.z -=
            std::max(definition.physics.halfExtents.z, 0.2F) - 0.04F;
        shadow.scale = {
            std::max(definition.physics.halfExtents.x * 2.15F, 1.0F),
            std::max(definition.physics.halfExtents.y * 2.15F, 0.8F),
            1.0F};
        device.draw(effectMesh_, shader, shadowTexture_,
                    transform(shadow), shadowPipeline, {},
                    shadowMaterial);
        drawObject(vehicleBodies_[racer],
                   definition.bodyVisuals, state.body);
        if (racer < racerRuntime.size())
        {
            const auto& runtime = racerRuntime[racer];
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
                drawGroups(
                    device, weapons_[weaponIndex].nodes.front(), shader,
                    transform(compose(
                        state.body,
                        compose(local,
                                race.weapons[weaponIndex]
                                    .visual.transform))),
                    pipeline, elapsedSeconds);
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
                    pipeline, elapsedSeconds);
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
        if (asset.nodes.empty() && asset.particleTextures.empty())
            continue;
        r3d::physics::Transform parent;
        parent.position = projectile.position;
        parent.rotation = directionRotation(projectile.direction);
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
            if (definition.type == 18U &&
                (!definition.tertiaryVisual.visualNodes.empty() ||
                 !definition.tertiaryVisual.particleEmitters.empty()))
            {
                drawDefinition(
                    projectileAssets.tertiaryVisual,
                    definition.tertiaryVisual, impact,
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
        if (mine.type == 10U)
        {
            const float scale =
                std::clamp(mine.seconds / 0.25F, 0.0F, 1.0F);
            parent.scale = {scale, scale, scale};
        }
        if (mine.velocity.x * mine.velocity.x +
                mine.velocity.y * mine.velocity.y +
                mine.velocity.z * mine.velocity.z >
            0.0001F)
            parent.rotation = directionRotation(mine.velocity);
        drawDefinition(*asset, *visual, parent, mine.seconds,
                       std::sqrt(
                           mine.velocity.x * mine.velocity.x +
                           mine.velocity.y * mine.velocity.y +
                           mine.velocity.z * mine.velocity.z));
    }

    auto effectPipeline = pipeline;
    effectPipeline.blendMode = PipelineState::BlendMode::Additive;
    effectPipeline.writeDepth = false;
    effectPipeline.faceCulling = PipelineState::FaceCulling::None;
    MaterialState glowMaterial;
    glowMaterial.emissive = 1.0F;
    glowMaterial.specular = 0.0F;
    glowMaterial.ignoreFog = true;
    for (const auto& effect : effects)
    {
        const float dx = effect.target.x - effect.origin.x;
        const float dy = effect.target.y - effect.origin.y;
        const float dz = effect.target.z - effect.origin.z;
        const float distance =
            std::sqrt(dx * dx + dy * dy + dz * dz);
        const float progress =
            effect.totalSeconds <= 0.0F
                ? 1.0F
                : 1.0F - effect.seconds / effect.totalSeconds;
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
                    effect.visualVariant == 2U
                        ? &projectileDefinition.tertiaryVisual
                        : &projectileDefinition.secondaryVisual;
                const auto* asset =
                    effect.visualVariant == 2U
                        ? &projectileAssets.tertiaryVisual
                        : &projectileAssets.secondaryVisual;
                r3d::physics::Transform parent;
                parent.position = effect.origin;
                parent.rotation = directionRotation(
                    {effect.target.x - effect.origin.x,
                     effect.target.y - effect.origin.y,
                     effect.target.z - effect.origin.z});
                drawDefinition(
                    *asset, *definition, parent,
                    effect.totalSeconds - effect.seconds, 0.0F);
                continue;
            }
            if (effect.kind ==
                r3d::game::originalrace::RaceEventKind::
                    HyperActivated)
            {
                if (!projectileDefinition.visual.visualNodes.empty() ||
                    !projectileDefinition.visual.particleEmitters.empty())
                {
                    r3d::physics::Transform parent;
                    parent.position = effect.origin;
                    parent.rotation = directionRotation(
                        {effect.target.x - effect.origin.x,
                         effect.target.y - effect.origin.y,
                         effect.target.z - effect.origin.z});
                    drawDefinition(
                        projectileAssets.visual,
                        projectileDefinition.visual, parent,
                        effect.totalSeconds - effect.seconds,
                        projectileDefinition.speed);
                    continue;
                }
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
        r3d::physics::Transform visual;
        Texture texture = destructionEffectTexture_;
        if (effect.kind ==
                r3d::game::originalrace::RaceEventKind::WeaponFired &&
            effect.weapon < weaponEffectTextures_.size())
        {
            visual.position = {
                (effect.origin.x + effect.target.x) * 0.5F,
                (effect.origin.y + effect.target.y) * 0.5F,
                (effect.origin.z + effect.target.z) * 0.5F + 0.6F};
            visual.scale = {std::max(distance, 0.5F), 0.32F, 1.0F};
            const float angle = std::atan2(dy, dx) * 0.5F;
            visual.rotation = {0.0F, 0.0F, std::sin(angle),
                               std::cos(angle)};
            texture = weaponEffectTextures_[effect.weapon];
        }
        else if (effect.kind ==
                     r3d::game::originalrace::RaceEventKind::
                         HyperActivated &&
                 effect.weapon < weaponEffectTextures_.size())
        {
            visual.position = effect.origin;
            visual.position.z += 0.55F;
            const float size = 2.0F + progress * 3.0F;
            visual.scale = {size, size, size};
            texture = weaponEffectTextures_[effect.weapon];
        }
        else
        {
            visual.position = effect.origin;
            visual.position.z += 0.8F;
            const float size = 1.5F + progress * 5.5F;
            visual.scale = {size, size, size};
        }
        MaterialState effectMaterial = glowMaterial;
        effectMaterial.color[3] =
            std::clamp(effect.seconds /
                           std::max(effect.totalSeconds, 0.001F),
                       0.0F, 1.0F);
        if (effect.kind ==
                r3d::game::originalrace::RaceEventKind::WeaponFired &&
            effect.weapon < race.weapons.size())
        {
            const auto type = race.weapons[effect.weapon].projectileType;
            if (type == 22U)
            {
                effectMaterial.color[0] =
                    (236.0F / 255.0F) * (1.0F - progress);
                effectMaterial.color[1] = 0.0F;
                effectMaterial.color[2] =
                    140.0F / 255.0F +
                    (1.0F - 140.0F / 255.0F) * progress;
            }
            else if (type == 14U)
            {
                effectMaterial.color[1] = 1.0F - progress;
                effectMaterial.color[2] = 1.0F - progress;
            }
            effectMaterial.textureTransform = effectTextureTransform(
                race.weapons[effect.weapon].effectTexturePath,
                effect.totalSeconds - effect.seconds);
        }
        else if (effect.kind ==
                     r3d::game::originalrace::RaceEventKind::
                         HyperActivated &&
                 effect.weapon < race.weapons.size())
        {
            effectMaterial.textureTransform = effectTextureTransform(
                race.weapons[effect.weapon].effectTexturePath,
                effect.totalSeconds - effect.seconds);
        }
        else
        {
            const auto frame = static_cast<std::uint32_t>(
                std::clamp(progress, 0.0F, 0.9999F) * 16.0F);
            effectMaterial.textureTransform = atlasFrame(4, 4, frame);
        }
        device.draw(effectMesh_, shader, texture, transform(visual),
                    effectPipeline, {}, effectMaterial);
    }

    auto smokePipeline = pipeline;
    smokePipeline.blendMode = PipelineState::BlendMode::Alpha;
    smokePipeline.writeDepth = false;
    smokePipeline.faceCulling = PipelineState::FaceCulling::None;
    MaterialState smokeMaterial;
    smokeMaterial.specular = 0.0F;
    smokeMaterial.emissive = 1.0F;
    smokeMaterial.ignoreFog = true;
    smokeMaterial.color = {0.72F, 0.64F, 0.55F, 0.28F};
    for (std::size_t racer = 0; racer < racerCount; ++racer)
    {
        if (vehicles[racer].engineRpm >= 900.0F)
        {
            const auto direction =
                rotateX(vehicles[racer].body.rotation);
            r3d::physics::Transform smoke;
            smoke.position = vehicles[racer].body.position;
            smoke.position.x -= direction.x * 1.5F;
            smoke.position.y -= direction.y * 1.5F;
            smoke.position.z += 0.45F;
            const float pulse =
                0.8F + 0.25F *
                           std::sin(elapsedSeconds * 8.0F +
                                    static_cast<float>(racer));
            smoke.scale = {pulse * 1.8F, pulse, 1.0F};
            device.draw(effectMesh_, shader, engineSmokeTexture_,
                        transform(smoke), smokePipeline, {},
                        smokeMaterial);
        }
        if (racer < racerRuntime.size() &&
            racerRuntime[racer].shieldSeconds > 0.0F)
        {
            r3d::physics::Transform shield = vehicles[racer].body;
            shield.position.z += 0.8F;
            const float size =
                3.2F + 0.12F *
                           std::sin(elapsedSeconds * 9.0F);
            shield.scale = {size, size, size};
            MaterialState shieldMaterial = glowMaterial;
            shieldMaterial.color = {0.35F, 0.72F, 1.0F, 0.72F};
            shieldMaterial.textureTransform =
                animatedAtlas(5, 2, elapsedSeconds);
            device.draw(effectMesh_, shader, shieldEffectTexture_,
                        transform(shield), effectPipeline, {},
                        shieldMaterial);
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
