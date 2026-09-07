#pragma once

#include "OriginalCameraManager.h"
#include "OriginalEnvironment.h"
#include "OriginalProfile.h"
#include "OriginalRace.h"
#include "OriginalRaceSession.h"
#include "OriginalResourceManager.h"
#include "OriginalTraceGfx.h"
#include "OriginalTrailGeometry.h"
#include "physics/OriginalVehiclePhysics.h"
#include "renderer/Renderer.h"
#include "resource/R3DMeshAsset.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rrr3d::race
{

using RaceCameraStyle =
    r3d::game::originalrace::source::CameraStyle;

class OriginalRaceRenderer
{
public:
    struct Asset
    {
        std::shared_ptr<const r3d::resource::R3DMeshAsset> source;
        r3d::renderer::Mesh mesh;
        std::vector<r3d::renderer::Texture> textures;
        std::vector<r3d::renderer::Texture> normalTextures;
        std::vector<std::reference_wrapper<const
            r3d::game::originalrace::MaterialDefinition>> materials;
        int subMesh = -1;
        bool sharedMesh = false;
    };

    struct ObjectAsset
    {
        std::vector<Asset> nodes;
        std::vector<std::vector<r3d::renderer::Texture>>
            particleTextures;
        std::vector<std::vector<std::reference_wrapper<const
            r3d::game::originalrace::MaterialDefinition>>>
            particleMaterials;
        // FxNodeManager owns ordinary mesh nodes instead of sprite
        // materials. The outer index remains aligned with particleEmitters.
        std::vector<std::vector<Asset>> particleNodes;
        bool planarReflection = false;
        bool castsShadow = false;
        r3d::physics::Vec3 graphVector1;
        r3d::physics::Vec3 graphVector3;
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
                    OriginalResourceManager& resources,
                    const r3d::game::originalrace::Race& race,
                    std::uint32_t width, std::uint32_t height,
                    std::string& error,
                    r3d::game::originalrace::source::Environment*
                        sourceEnvironment = nullptr);
    bool resize(r3d::renderer::GraphicsDevice& device,
                std::uint32_t width, std::uint32_t height,
                std::string& error);
    void shutdown(r3d::renderer::GraphicsDevice& device) noexcept;

    r3d::renderer::Camera makeCamera(
        const r3d::renderer::GraphicsDevice& device,
        const r3d::physics::VehicleState& vehicle,
        float sourceFreeWheelSpeed, std::uint32_t width,
        std::uint32_t height,
        r3d::game::originalrace::PreferredCamera style,
        float cameraDistance, float seconds) noexcept;
    r3d::renderer::Camera makeCamera(
        const r3d::renderer::GraphicsDevice& device,
        const r3d::physics::VehicleState& vehicle,
        float sourceFreeWheelSpeed, std::uint32_t width,
        std::uint32_t height, RaceCameraStyle style,
        float cameraDistance, float seconds) noexcept;
    r3d::renderer::Camera makePresentationCamera(
        const r3d::renderer::GraphicsDevice& device,
        const r3d::game::originalrace::PresentationCamera& source,
        std::uint32_t width, std::uint32_t height) noexcept;
    void moveDebugCamera(RaceCameraStyle style, float forward,
                         float right, float seconds) noexcept;
    void rotateDebugCamera(float deltaX, float deltaY) noexcept;
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
              const std::vector<float>& bonusScales,
              const std::vector<
                  r3d::game::originalrace::RacerRuntime>& racerRuntime,
              const std::vector<
                  r3d::game::originalrace::RaceEffect>& effects,
              const std::vector<
                  r3d::game::originalrace::MineRuntime>& mines,
              const std::vector<
                  r3d::game::originalrace::ProjectileRuntime>& projectiles,
              float elapsedSeconds, std::int32_t countdownStage,
              bool reflectionPass = false,
              bool omitEnvironmentSurface = false,
              bool refractionPass = false,
              bool environmentReflectionPass = false,
              const r3d::renderer::Camera* cullingCamera = nullptr);
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
        const std::vector<float>& bonusScales,
        const std::vector<
            r3d::game::originalrace::RacerRuntime>& racerRuntime,
        const std::vector<
            r3d::game::originalrace::RaceEffect>& effects,
        const std::vector<
            r3d::game::originalrace::MineRuntime>& mines,
        const std::vector<
            r3d::game::originalrace::ProjectileRuntime>& projectiles,
        float elapsedSeconds,
        const r3d::game::originalrace::QualityConfig& quality,
        std::int32_t countdownStage = 4,
        const r3d::game::originalrace::source::TraceGfx*
            debugTrace = nullptr);

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
    r3d::renderer::Texture grassTexture_;
    r3d::renderer::Mesh effectMesh_;
    r3d::renderer::Mesh grassMesh_;
    r3d::renderer::Mesh postProcessMesh_;
    r3d::renderer::Texture debugTraceTexture_;
    r3d::renderer::Shader shadowShader_;
    r3d::renderer::Shader skyShader_;
    r3d::renderer::Shader bloomExtractShader_;
    r3d::renderer::Shader bloomBlurShader_;
    r3d::renderer::Shader toneMapShader_;
    r3d::renderer::Shader copyShader_;
    r3d::renderer::Shader waterShader_;
    r3d::renderer::Shader fogPlaneShader_;
    r3d::renderer::Shader grassShader_;
    r3d::renderer::Shader luminanceLogShader_;
    r3d::renderer::Shader luminanceDownsampleShader_;
    r3d::renderer::Shader luminanceAdaptShader_;
    r3d::renderer::Shader sunShaftPrepareShader_;
    r3d::renderer::Shader sunShaftCompositeShader_;
    r3d::renderer::Shader refractionShader_;
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
    r3d::renderer::RenderTarget sunShaftSceneTarget_;
    r3d::renderer::RenderTarget sunShaftBlurTargetA_;
    r3d::renderer::RenderTarget sunShaftBlurTargetB_;
    std::uint32_t frameWidth_ = 0;
    std::uint32_t frameHeight_ = 0;
    r3d::physics::Vec3 environmentSurfaceCenter_;
    r3d::physics::Vec3 environmentSurfaceSize_;
    r3d::physics::Vec3 sceneWorldCenter_;
    r3d::physics::Vec3 sceneWorldMinimum_;
    r3d::physics::Vec3 sceneWorldMaximum_;
    std::vector<r3d::physics::Vec3> grassFieldOffsets_;
    r3d::game::originalrace::source::CameraManager sourceCamera_;
    r3d::game::originalrace::source::Environment
        ownedSourceEnvironment_;
    r3d::game::originalrace::source::Environment* sourceEnvironment_ =
        &ownedSourceEnvironment_;
    r3d::game::originalrace::source::EnvironmentRenderPolicy
        activeEnvironmentPolicy_;
    r3d::physics::Vec3 cameraPosition_;
    r3d::physics::Vec3 cameraViewDirection_{1.0F, 0.0F, 0.0F};
    r3d::physics::Quat cameraRotation_;
    RaceCameraStyle cameraStyle_ = RaceCameraStyle::Isometric;
    // Each source group retains its own material age in the Metal batch.
    std::vector<TrailSample> wheelTrailBatchPoints_;
    // ActorManager::RayUser fade timers for original gpCullOpacity actors.
    std::vector<float> trackCullOpacityTimes_;
    std::vector<float> decorationCullOpacityTimes_;
    float previousRenderSeconds_ = 0.0F;
    float perspectiveFarDistance_ = 120.0F;
    float activeCameraFarDistance_ = 120.0F;
    float activeCameraNearDistance_ = 1.0F;
    float cameraDepthOffset_ = 0.0F;
    std::uint32_t activeEnvironmentQuality_ = 2U;
    std::uint32_t activeLightQuality_ = 2U;
    // FxPointSpritesManager multiplies the particle scale length by 0.75
    // for the orthographic camera and by 0.25 for perspective cameras.
    float pointSpriteScale_ = 0.25F;
    bool adaptedLuminanceAIsCurrent_ = false;
    bool luminanceAdaptationInitialized_ = false;
    bool sunShaftResourcesEnabled_ = false;
};

} // namespace rrr3d::race
