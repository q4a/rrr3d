#pragma once

#include "OriginalRace.h"
#include "physics/OriginalVehiclePhysics.h"
#include "renderer/Renderer.h"
#include "resource/R3DMeshAsset.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rrr3d::race
{

class OriginalRaceRenderer
{
public:
    struct Asset
    {
        r3d::resource::R3DMeshAsset source;
        r3d::renderer::Mesh mesh;
        std::vector<r3d::renderer::Texture> textures;
        std::vector<r3d::game::originalrace::MaterialDefinition> materials;
        int subMesh = -1;
    };

    struct ObjectAsset
    {
        std::vector<Asset> nodes;
    };

    bool initialize(r3d::renderer::GraphicsDevice& device,
                    const r3d::resource::ResourceFileSystem& resources,
                    const r3d::game::originalrace::Race& race,
                    std::string& error);
    void shutdown(r3d::renderer::GraphicsDevice& device) noexcept;

    r3d::renderer::Camera makeCamera(
        const r3d::renderer::GraphicsDevice& device,
        const r3d::physics::VehicleState& vehicle, std::uint32_t width,
        std::uint32_t height) const noexcept;
    void draw(r3d::renderer::GraphicsDevice& device,
              r3d::renderer::Shader shader,
              const r3d::game::originalrace::Race& race,
              const std::vector<r3d::physics::VehicleState>& vehicles,
              const r3d::renderer::PipelineState& pipeline,
              const std::vector<bool>& decorationActive,
              const std::vector<bool>& bonusActive,
              float elapsedSeconds) const;

private:
    std::vector<ObjectAsset> tracks_;
    std::vector<ObjectAsset> decorations_;
    std::vector<ObjectAsset> bonuses_;
    std::vector<ObjectAsset> vehicleBodies_;
    std::vector<std::vector<ObjectAsset>> vehicleWheels_;
    r3d::renderer::Texture skyTexture_;
    r3d::renderer::Mesh skyMesh_;
    r3d::renderer::Texture rainTexture_;
    r3d::renderer::Mesh rainMesh_;
};

} // namespace rrr3d::race
