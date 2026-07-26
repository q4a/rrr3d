#pragma once

#include "OriginalProfile.h"
#include "OriginalRace.h"
#include "OriginalRaceSession.h"
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
        std::vector<std::vector<r3d::renderer::Texture>>
            particleTextures;
    };

    struct ProjectileAsset
    {
        ObjectAsset visual;
        ObjectAsset secondaryVisual;
        ObjectAsset tertiaryVisual;
    };

    bool initialize(r3d::renderer::GraphicsDevice& device,
                    const r3d::resource::ResourceFileSystem& resources,
                    const r3d::game::originalrace::Race& race,
                    std::string& error);
    void shutdown(r3d::renderer::GraphicsDevice& device) noexcept;

    r3d::renderer::Camera makeCamera(
        const r3d::renderer::GraphicsDevice& device,
        const r3d::physics::VehicleState& vehicle, std::uint32_t width,
        std::uint32_t height,
        r3d::game::originalrace::PreferredCamera style,
        float cameraDistance, float seconds) noexcept;
    void resetCamera() noexcept;
    void draw(r3d::renderer::GraphicsDevice& device,
              r3d::renderer::Shader shader,
              const r3d::game::originalrace::Race& race,
              const std::vector<r3d::physics::VehicleState>& vehicles,
              const r3d::renderer::PipelineState& pipeline,
              const std::vector<bool>& decorationActive,
              const std::vector<bool>& bonusActive,
              const std::vector<
                  r3d::game::originalrace::RacerRuntime>& racerRuntime,
              const std::vector<
                  r3d::game::originalrace::RaceEffect>& effects,
              const std::vector<
                  r3d::game::originalrace::MineRuntime>& mines,
              const std::vector<
                  r3d::game::originalrace::ProjectileRuntime>& projectiles,
              float elapsedSeconds) const;

private:
    std::vector<ObjectAsset> tracks_;
    std::vector<ObjectAsset> decorations_;
    std::vector<ObjectAsset> bonuses_;
    std::vector<ObjectAsset> vehicleBodies_;
    std::vector<std::vector<ObjectAsset>> vehicleWheels_;
    std::vector<ObjectAsset> weapons_;
    std::vector<std::vector<ProjectileAsset>> projectiles_;
    r3d::renderer::Texture skyTexture_;
    r3d::renderer::Mesh skyMesh_;
    r3d::renderer::Texture rainTexture_;
    r3d::renderer::Mesh rainMesh_;
    std::vector<r3d::renderer::Texture> weaponEffectTextures_;
    r3d::renderer::Texture destructionEffectTexture_;
    r3d::renderer::Texture engineSmokeTexture_;
    r3d::renderer::Texture shieldEffectTexture_;
    r3d::renderer::Texture shadowTexture_;
    r3d::renderer::Texture vehicleLightTexture_;
    r3d::renderer::Texture environmentSurfaceTexture_;
    r3d::renderer::Mesh effectMesh_;
    r3d::physics::Vec3 environmentSurfaceCenter_;
    r3d::physics::Vec3 environmentSurfaceSize_;
    r3d::physics::Vec3 cameraLead_;
    r3d::physics::Vec3 previousCameraTarget_;
    r3d::physics::Vec3 cameraPosition_;
    r3d::physics::Vec3 cameraJumpDirection_;
    r3d::physics::Vec3 thirdPersonDirection_{1.0F, 0.0F, 0.0F};
    float cameraJumpDistance_ = 0.0F;
    float cameraJumpSpeed_ = 0.0F;
    float thirdPersonPullback_ = 0.0F;
    bool cameraInitialized_ = false;
};

} // namespace rrr3d::race
