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
        r3d::renderer::Texture texture;
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
              const r3d::physics::VehicleState& vehicle,
              const r3d::renderer::PipelineState& pipeline) const;

private:
    std::vector<Asset> tracks_;
    Asset body_;
    Asset wheel_;
};

} // namespace rrr3d::race
