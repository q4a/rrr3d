#include "OriginalEnvironment.h"
#include "OriginalWorld.h"

#include <cmath>
#include <iostream>

namespace source = r3d::game::originalrace::source;
using namespace r3d::game::originalrace;

namespace
{

bool near(float first, float second, float tolerance = 0.0001F)
{
    return std::abs(first - second) <= tolerance;
}

int fail(const char* message)
{
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main()
{
    if (source::Environment::WeatherFromToken("ewClody") !=
            Weather::Cloudy ||
        source::Environment::WeatherFromToken("rainy") !=
            Weather::Rainy ||
        source::Environment::WeatherToken(Weather::Hell) != "hell" ||
        source::Environment::WorldTypeFromLevelPath(
            "Data/Map/World5/map0.r3dMap") !=
            source::EnvironmentWorldType::World5)
        return fail("Environment source token mapping differs");

    EnvironmentDescription environment;
    source::Environment::ApplyWorldType(
        environment, source::EnvironmentWorldType::World4);
    source::Environment::ApplyWeather(
        environment, Weather::Hell,
        source::EnvironmentWorldType::World4);
    if (environment.surface != EnvironmentSurface::Magma ||
        !near(environment.surfaceHeight, 0.5F) ||
        !near(environment.surfaceScroll, 0.01F) ||
        !near(environment.surfaceTileScale, 25.0F) ||
        !near(environment.surfaceCloudIntensity, 1.0F) ||
        environment.skyTexturePath !=
            "Data/World4/Texture/skyTex1.dds" ||
        !near(environment.fogColor[0], 82.0F / 255.0F) ||
        !near(environment.surfaceCloudColor[0], 1.0F) ||
        !near(environment.hdrLuminanceKey, 1.9F) ||
        !near(environment.hdrExposure, 8.0F) ||
        !near(environment.perspectiveFarDistance, 100.0F))
        return fail("Environment World4/Hell application differs");

    source::Environment::ApplyWorldType(
        environment, source::EnvironmentWorldType::World5);
    source::Environment::ApplyWeather(
        environment, Weather::Snow,
        source::EnvironmentWorldType::World5);
    if (!environment.planarReflection ||
        environment.directionalShadowMinimumQuality != 2U ||
        environment.skyTexturePath !=
            "Data/World5/Texture/sky_text.dds" ||
        !near(source::Environment::CameraFar(environment, false),
              100.0F) ||
        !near(source::Environment::CameraFar(environment, true),
              150.0F) ||
        !near(source::Environment::CameraFar(
                  environment, false, true),
              1000.0F))
        return fail("Environment snow/camera far rules differ");

    source::Environment::ApplyPresentation(
        environment, source::EnvironmentWorldType::Garage);
    if (environment.skyEnabled || environment.fogEnabled ||
        environment.directionalLightEnabled ||
        environment.dynamicReflectionsEnabled ||
        !environment.lamps[0].enabled ||
        !environment.lamps[1].enabled ||
        environment.lamps[2].enabled ||
        !near(environment.lamps[0].range, 20.0F) ||
        !near(environment.perspectiveFarDistance, 20.0F) ||
        !near(environment.hdrBrightThreshold, 4.0F))
        return fail("Environment Garage application differs");

    EnvironmentDescription water;
    source::Environment::ApplyWorldType(
        water, source::EnvironmentWorldType::World2);
    source::Environment::ApplyWeather(
        water, Weather::Fair,
        source::EnvironmentWorldType::World2);
    QualityConfig quality;
    quality.shadow = 1U;
    quality.environment = 1U;
    quality.light = 1U;
    quality.postEffect = 1U;
    auto policy = source::Environment::ApplyQuality(
        water, quality, false);
    if (!policy.directionalShadows || !policy.shadows ||
        !policy.pixelLighting || policy.bumpMapping ||
        !policy.reflectionMapping || policy.trueReflections ||
        policy.planarReflections || !policy.bloom ||
        !policy.refraction || policy.hdr || policy.sunShaft ||
        !policy.sky || !policy.fog || !policy.highQualityWater ||
        !policy.sceneDepthSurface || policy.environmentReflection)
        return fail("Environment Middle quality graph differs");

    quality.shadow = 2U;
    quality.environment = 2U;
    quality.light = 2U;
    quality.postEffect = 2U;
    policy = source::Environment::ApplyQuality(water, quality, false);
    if (!policy.bumpMapping || !policy.trueReflections ||
        !policy.planarReflections || !policy.hdr ||
        !policy.sunShaft || !policy.environmentReflection)
        return fail("Environment High quality graph differs");
    const auto isometric = source::Environment::ApplyQuality(
        water, quality, true);
    if (isometric.fog || isometric.sunShaft)
        return fail("Environment isometric exclusions differ");

    source::Environment::ApplyWeather(
        water, Weather::Night,
        source::EnvironmentWorldType::World2);
    policy = source::Environment::ApplyQuality(water, quality, false);
    if (policy.directionalShadows || policy.bloom || policy.hdr ||
        policy.sunShaft || !policy.refraction)
        return fail("Environment night graph differs");

    source::Environment::ApplyWeather(
        water, Weather::Rainy,
        source::EnvironmentWorldType::World2);
    source::Environment scene;
    scene.ProcessScene(water, false, {1.0F, 2.0F, 3.0F});
    if (scene.SceneStarted() || scene.RainVisible())
        return fail("Environment processed rain before StartScene");
    scene.StartScene(water, false, {1.0F, 2.0F, 3.0F});
    if (!scene.SceneStarted() || !scene.RainVisible() ||
        !near(scene.RainPosition().x, 1.0F))
        return fail("Environment StartScene rain differs");
    scene.ProcessScene(water, true, {4.0F, 5.0F, 6.0F});
    if (scene.RainVisible() || !scene.IsometricRain())
        return fail("Environment isometric rain recreation differs");
    scene.ProcessScene(water, false, {7.0F, 8.0F, 9.0F});
    if (!scene.RainVisible() || scene.IsometricRain() ||
        !near(scene.RainPosition().z, 9.0F))
        return fail("Environment perspective rain follow differs");
    scene.ReleaseScene();
    if (scene.SceneStarted() || scene.RainVisible())
        return fail("Environment ReleaseScene differs");

    source::WorldEventPump world;
    world.SetEnvironment(&scene);
    scene.PrepareScene(water, false, {10.0F, 11.0F, 12.0F});
    scene.PrepareScene(water, false, {13.0F, 14.0F, 15.0F});
    if (!near(scene.RainPosition().x, 10.0F))
        return fail("Environment advanced outside World::FrameStep");
    world.FrameStep(1.0F / 60.0F, 0.5F);
    if (!near(scene.RainPosition().x, 13.0F))
        return fail("World::FrameStep did not process Environment");
    world.Pause(true);
    scene.PrepareScene(water, false, {16.0F, 17.0F, 18.0F});
    world.FrameStep(1.0F / 60.0F, 0.5F);
    if (!near(scene.RainPosition().x, 13.0F))
        return fail("paused World advanced Environment");
    scene.ReleaseScene();

    std::cout << "original Environment source rules passed\n";
    return 0;
}
