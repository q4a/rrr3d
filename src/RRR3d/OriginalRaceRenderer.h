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
        std::vector<r3d::renderer::Texture> normalTextures;
        std::vector<r3d::game::originalrace::MaterialDefinition> materials;
        int subMesh = -1;
    };

    struct ObjectAsset
    {
        std::vector<Asset> nodes;
        std::vector<std::vector<r3d::renderer::Texture>>
            particleTextures;
        // FxNodeManager owns ordinary mesh nodes instead of sprite
        // materials. The outer index remains aligned with particleEmitters.
        std::vector<std::vector<Asset>> particleNodes;
        bool planarReflection = false;
        bool castsShadow = false;
        r3d::game::originalrace::LightingMode lighting =
            r3d::game::originalrace::LightingMode::Standard;
    };

    struct ProjectileAsset
    {
        ObjectAsset visual;
        ObjectAsset secondaryVisual;
        ObjectAsset tertiaryVisual;
        ObjectAsset deathVisual;
        ObjectAsset secondaryDeathVisual;
        ObjectAsset tertiaryDeathVisual;
    };

    bool initialize(r3d::renderer::GraphicsDevice& device,
                    const r3d::resource::ResourceFileSystem& resources,
                    const r3d::game::originalrace::Race& race,
                    std::uint32_t width, std::uint32_t height,
                    std::string& error);
    bool resize(r3d::renderer::GraphicsDevice& device,
                std::uint32_t width, std::uint32_t height,
                std::string& error);
    void shutdown(r3d::renderer::GraphicsDevice& device) noexcept;

    r3d::renderer::Camera makeCamera(
        const r3d::renderer::GraphicsDevice& device,
        const r3d::physics::VehicleState& vehicle, std::uint32_t width,
        std::uint32_t height,
        r3d::game::originalrace::PreferredCamera style,
        float cameraDistance, float seconds) noexcept;
    r3d::renderer::Camera makePresentationCamera(
        const r3d::renderer::GraphicsDevice& device,
        const r3d::game::originalrace::PresentationCamera& source,
        std::uint32_t width, std::uint32_t height) noexcept;
    void resetCamera() noexcept;
    void draw(r3d::renderer::GraphicsDevice& device,
              r3d::renderer::Shader shader,
              const r3d::game::originalrace::Race& race,
              const std::vector<r3d::physics::VehicleState>& vehicles,
              const r3d::renderer::PipelineState& pipeline,
              const std::vector<bool>& decorationActive,
              const std::vector<
                  r3d::game::originalrace::DecorationFragmentState>&
                  decorationFragments,
              const std::vector<
                  r3d::game::originalrace::VehicleDeathFragmentState>&
                  vehicleDeathFragments,
              const std::vector<bool>& bonusActive,
              const std::vector<
                  r3d::game::originalrace::RacerRuntime>& racerRuntime,
              const std::vector<
                  r3d::game::originalrace::RaceEffect>& effects,
              const std::vector<
                  r3d::game::originalrace::MineRuntime>& mines,
              const std::vector<
                  r3d::game::originalrace::ProjectileRuntime>& projectiles,
              float elapsedSeconds,
              bool reflectionPass = false,
              bool omitEnvironmentSurface = false);
    void renderFrame(
        r3d::renderer::GraphicsDevice& device,
        r3d::renderer::Shader sceneShader,
        const r3d::renderer::Camera& camera,
        std::uint32_t clearRgba,
        const r3d::game::originalrace::Race& race,
        const std::vector<r3d::physics::VehicleState>& vehicles,
        const r3d::renderer::PipelineState& pipeline,
        const std::vector<bool>& decorationActive,
        const std::vector<
            r3d::game::originalrace::DecorationFragmentState>&
            decorationFragments,
        const std::vector<
            r3d::game::originalrace::VehicleDeathFragmentState>&
            vehicleDeathFragments,
        const std::vector<bool>& bonusActive,
        const std::vector<
            r3d::game::originalrace::RacerRuntime>& racerRuntime,
        const std::vector<
            r3d::game::originalrace::RaceEffect>& effects,
        const std::vector<
            r3d::game::originalrace::MineRuntime>& mines,
        const std::vector<
            r3d::game::originalrace::ProjectileRuntime>& projectiles,
        float elapsedSeconds,
        const r3d::game::originalrace::QualityConfig& quality);

private:
    bool createFrameTargets(r3d::renderer::GraphicsDevice& device,
                            std::uint32_t width, std::uint32_t height,
                            std::string& error);
    void destroyFrameTargets(
        r3d::renderer::GraphicsDevice& device) noexcept;
    void drawShadowCasters(
        r3d::renderer::GraphicsDevice& device,
        const r3d::game::originalrace::Race& race,
        const std::vector<r3d::physics::VehicleState>& vehicles,
        const r3d::renderer::PipelineState& pipeline,
        const std::vector<bool>& decorationActive,
        const std::vector<
            r3d::game::originalrace::DecorationFragmentState>&
            decorationFragments,
        const std::vector<
            r3d::game::originalrace::VehicleDeathFragmentState>&
            vehicleDeathFragments,
        const std::vector<
            r3d::game::originalrace::RacerRuntime>& racerRuntime,
        float elapsedSeconds) const;
    std::vector<ObjectAsset> tracks_;
    std::vector<ObjectAsset> decorations_;
    std::vector<std::vector<ObjectAsset>> decorationPieces_;
    std::vector<ObjectAsset> bonuses_;
    std::vector<ObjectAsset> bonusDeathEffects_;
    std::vector<ObjectAsset> vehicleBodies_;
    std::vector<std::vector<ObjectAsset>> vehicleWheels_;
    std::vector<ObjectAsset> vehicleTrackVisuals_;
    std::vector<ObjectAsset> vehicleCushionVisuals_;
    std::vector<ObjectAsset> vehicleLowLifeEffects_;
    std::vector<ObjectAsset> vehicleEnergyDamageEffects_;
    std::vector<ObjectAsset> vehicleShieldEffects_;
    std::vector<r3d::physics::Vec3> vehicleShieldScales_;
    std::vector<std::vector<ObjectAsset>> vehicleDeathEffects_;
    std::vector<ObjectAsset> weapons_;
    std::vector<ObjectAsset> weaponShotEffects_;
    std::vector<std::vector<ProjectileAsset>> projectiles_;
    ObjectAsset rainEffect_;
    ObjectAsset wheelTrailEffect_;
    ObjectAsset wheelSmokeEffect_;
    ObjectAsset contactEffect_;
    r3d::renderer::Texture skyTexture_;
    r3d::renderer::Mesh skyMesh_;
    r3d::renderer::Texture vehicleLightTexture_;
    r3d::renderer::Texture environmentSurfaceTexture_;
    r3d::renderer::Texture waterNormalTexture_;
    r3d::renderer::Mesh effectMesh_;
    r3d::renderer::Mesh postProcessMesh_;
    r3d::renderer::Shader shadowShader_;
    r3d::renderer::Shader skyShader_;
    r3d::renderer::Shader bloomExtractShader_;
    r3d::renderer::Shader bloomBlurShader_;
    r3d::renderer::Shader toneMapShader_;
    r3d::renderer::Shader copyShader_;
    r3d::renderer::Shader waterShader_;
    r3d::renderer::Shader luminanceLogShader_;
    r3d::renderer::Shader luminanceDownsampleShader_;
    r3d::renderer::Shader luminanceAdaptShader_;
    r3d::renderer::RenderTarget hdrTarget_;
    r3d::renderer::RenderTarget waterSceneTarget_;
    r3d::renderer::RenderTarget reflectionTarget_;
    r3d::renderer::CubeRenderTarget environmentReflectionTarget_;
    r3d::renderer::RenderTarget shadowTarget_;
    r3d::renderer::RenderTarget shadowTargetFar_;
    r3d::renderer::RenderTarget shadowTargetThird_;
    r3d::renderer::RenderTarget luminance64Target_;
    r3d::renderer::RenderTarget luminance16Target_;
    r3d::renderer::RenderTarget luminance4Target_;
    r3d::renderer::RenderTarget luminance1Target_;
    r3d::renderer::RenderTarget adaptedLuminanceTargetA_;
    r3d::renderer::RenderTarget adaptedLuminanceTargetB_;
    r3d::renderer::RenderTarget bloomTargetA_;
    r3d::renderer::RenderTarget bloomTargetB_;
    std::uint32_t frameWidth_ = 0;
    std::uint32_t frameHeight_ = 0;
    r3d::physics::Vec3 environmentSurfaceCenter_;
    r3d::physics::Vec3 environmentSurfaceSize_;
    r3d::physics::Vec3 cameraLead_;
    r3d::physics::Vec3 previousCameraTarget_;
    r3d::physics::Vec3 cameraPosition_;
    r3d::physics::Vec3 cameraViewDirection_{1.0F, 0.0F, 0.0F};
    r3d::physics::Vec3 cameraJumpDirection_;
    r3d::physics::Quat cameraRotation_;
    r3d::physics::Quat thirdPersonRotation_;
    r3d::game::originalrace::PreferredCamera cameraStyle_ =
        r3d::game::originalrace::PreferredCamera::Isometric;
    std::vector<std::vector<std::vector<r3d::physics::Vec3>>>
        wheelTrailPaths_;
    std::vector<std::vector<std::vector<float>>> wheelTrailTimes_;
    std::vector<std::uint32_t> wheelTrailResetCounts_;
    std::vector<std::vector<float>> wheelSmokeStartTimes_;
    std::vector<std::vector<float>> wheelSmokeEndTimes_;
    std::vector<float> vehicleTrackAnimationOffsets_;
    std::vector<float> vehicleCushionAnimationAngles_;
    // ActorManager::RayUser fade timers for original gpCullOpacity actors.
    std::vector<float> trackCullOpacityTimes_;
    std::vector<float> decorationCullOpacityTimes_;
    float cameraJumpDistance_ = 0.0F;
    float cameraJumpSpeed_ = 0.0F;
    float thirdPersonPullback_ = 0.0F;
    float previousRenderSeconds_ = 0.0F;
    // FxPointSpritesManager multiplies the particle scale length by 0.75
    // for the orthographic camera and by 0.25 for perspective cameras.
    float pointSpriteScale_ = 0.25F;
    float wheelTrailUpdateSeconds_ = -1.0F;
    float vehicleAnimationUpdateSeconds_ = -1.0F;
    bool adaptedLuminanceAIsCurrent_ = false;
    bool luminanceAdaptationInitialized_ = false;
    bool cameraInitialized_ = false;
    bool cameraStyleInitialized_ = false;
};

} // namespace rrr3d::race
