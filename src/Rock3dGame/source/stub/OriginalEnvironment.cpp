#include "OriginalEnvironment.h"

#include <algorithm>
#include <array>

namespace r3d::game::originalrace::source
{
namespace
{

constexpr std::array<float, 4> black{
    0.0F, 0.0F, 0.0F, 1.0F};
constexpr std::array<float, 4> garageAmbient{
    0.6F, 0.6F, 0.6F, 1.0F};

void ResetWorldOptions(EnvironmentDescription& description) noexcept
{
    description.surface = EnvironmentSurface::None;
    description.planarReflection = false;
    description.surfaceHeight = 0.0F;
    description.surfaceScroll = 0.0F;
    description.surfaceTileScale = 1.0F;
    description.surfaceCloudIntensity = 0.0F;
    description.dynamicReflectionsEnabled = true;
    for (auto& lamp : description.lamps)
        lamp.enabled = false;
}

std::size_t EnabledLampCount(
    const EnvironmentDescription& description) noexcept
{
    return static_cast<std::size_t>(std::count_if(
        description.lamps.begin(), description.lamps.end(),
        [](const EnvironmentLamp& lamp) { return lamp.enabled; }));
}

} // namespace

Weather Environment::WeatherFromToken(std::string_view token) noexcept
{
    if (token == "night" || token == "ewNight")
        return Weather::Night;
    if (token == "cloudy" || token == "ewClody")
        return Weather::Cloudy;
    if (token == "rainy" || token == "ewRainy")
        return Weather::Rainy;
    if (token == "sahara" || token == "ewSahara")
        return Weather::Sahara;
    if (token == "hell" || token == "ewHell")
        return Weather::Hell;
    if (token == "snow" || token == "ewSnow")
        return Weather::Snow;
    return Weather::Fair;
}

std::string_view Environment::WeatherToken(Weather weather) noexcept
{
    switch (weather)
    {
    case Weather::Night:
        return "night";
    case Weather::Cloudy:
        return "cloudy";
    case Weather::Rainy:
        return "rainy";
    case Weather::Sahara:
        return "sahara";
    case Weather::Hell:
        return "hell";
    case Weather::Snow:
        return "snow";
    case Weather::Fair:
        return "fair";
    }
    return "fair";
}

EnvironmentWorldType Environment::WorldTypeFromLevelPath(
    std::string_view levelPath) noexcept
{
    if (levelPath.find("World2") != std::string_view::npos)
        return EnvironmentWorldType::World2;
    if (levelPath.find("World3") != std::string_view::npos)
        return EnvironmentWorldType::World3;
    if (levelPath.find("World4") != std::string_view::npos)
        return EnvironmentWorldType::World4;
    if (levelPath.find("World5") != std::string_view::npos)
        return EnvironmentWorldType::World5;
    if (levelPath.find("World6") != std::string_view::npos)
        return EnvironmentWorldType::World6;
    if (levelPath.find("CarFrame") != std::string_view::npos ||
        levelPath.find("Garage") != std::string_view::npos)
        return EnvironmentWorldType::Garage;
    if (levelPath.find("SpaceshipFrame") != std::string_view::npos ||
        levelPath.find("Angar") != std::string_view::npos)
        return EnvironmentWorldType::Angar;
    return EnvironmentWorldType::World1;
}

void Environment::ApplyWeather(EnvironmentDescription& description,
                               Weather weather,
                               EnvironmentWorldType worldType)
{
    description.weather = weather;
    description.rain = weather == Weather::Rainy;
    description.skyEnabled = true;
    description.fogEnabled = true;
    description.directionalLightEnabled = weather != Weather::Night;
    description.directionalShadowMinimumQuality =
        weather == Weather::Snow ? 2U : 1U;
    description.ambientColor = black;
    switch (weather)
    {
    case Weather::Night:
        description.skyTexturePath = "Data/Misc/nightSky.dds";
        description.fogColor = {
            15.0F / 255.0F, 25.0F / 255.0F,
            31.0F / 255.0F, 1.0F};
        description.ambientColor = {
            138.0F / 255.0F, 144.0F / 255.0F,
            174.0F / 255.0F, 1.0F};
        description.fogIntensity = 1.0F;
        description.perspectiveFarDistance = 120.0F;
        break;
    case Weather::Cloudy:
    case Weather::Rainy:
        description.skyTexturePath =
            "Data/World2/texture/skyTex1.dds";
        description.fogColor = {
            192.0F / 255.0F, 189.0F / 255.0F,
            184.0F / 255.0F, 0.0F};
        description.fogIntensity = 1.0F;
        description.perspectiveFarDistance =
            weather == Weather::Rainy ? 100.0F : 120.0F;
        break;
    case Weather::Sahara:
        description.skyTexturePath =
            "Data/World3/Texture/skyTex1.dds";
        description.fogColor = {
            87.0F / 255.0F, 81.0F / 255.0F,
            115.0F / 255.0F, 1.0F};
        description.fogIntensity = 0.5F;
        description.perspectiveFarDistance = 100.0F;
        break;
    case Weather::Hell:
        description.skyTexturePath =
            "Data/World4/Texture/skyTex1.dds";
        description.fogColor = {
            82.0F / 255.0F, 12.0F / 255.0F,
            8.0F / 255.0F, 1.0F};
        description.fogIntensity = 0.5F;
        description.perspectiveFarDistance = 100.0F;
        break;
    case Weather::Snow:
        description.skyTexturePath =
            "Data/World5/Texture/sky_text.dds";
        description.fogColor = {
            156.0F / 255.0F, 166.0F / 255.0F,
            181.0F / 255.0F, 1.0F};
        description.fogIntensity = 0.5F;
        description.perspectiveFarDistance = 100.0F;
        break;
    case Weather::Fair:
        description.skyTexturePath =
            "Data/World1/Texture/skyTex1.dds";
        description.fogColor = {
            148.0F / 255.0F, 193.0F / 255.0F,
            235.0F / 255.0F, 1.0F};
        description.fogIntensity = 0.5F;
        description.perspectiveFarDistance = 120.0F;
        break;
    }
    description.surfaceCloudColor = description.fogColor;
    if (worldType == EnvironmentWorldType::World3)
    {
        description.surfaceCloudColor = {
            87.0F / 255.0F, 81.0F / 255.0F,
            115.0F / 255.0F, 1.0F};
    }
    else if (worldType == EnvironmentWorldType::World4)
    {
        description.surfaceCloudColor =
            {1.0F, 1.0F, 1.0F, 1.0F};
    }
}

void Environment::ApplyWeatherToken(
    EnvironmentDescription& description, std::string_view token,
    std::string_view levelPath)
{
    ApplyWeather(description, WeatherFromToken(token),
                 WorldTypeFromLevelPath(levelPath));
}

void Environment::ApplyWorldType(EnvironmentDescription& description,
                                 EnvironmentWorldType worldType)
{
    ResetWorldOptions(description);
    switch (worldType)
    {
    case EnvironmentWorldType::World1:
        description.surface = EnvironmentSurface::Grass;
        description.hdrLuminanceKey = 1.1F;
        description.hdrBrightThreshold = 1.5F;
        description.hdrGaussianScalar = 30.0F;
        description.hdrExposure = 15.0F;
        break;
    case EnvironmentWorldType::World2:
        description.surface = EnvironmentSurface::Water;
        description.surfaceTileScale = 4.0F;
        description.surfaceCloudIntensity = 0.1F;
        description.hdrLuminanceKey = 1.7F;
        description.hdrBrightThreshold = 1.9F;
        description.hdrGaussianScalar = 30.0F;
        description.hdrExposure = 8.0F;
        break;
    case EnvironmentWorldType::World3:
        description.surface = EnvironmentSurface::GroundFog;
        description.surfaceHeight = 3.0F;
        description.surfaceScroll = 0.02F;
        description.surfaceTileScale = 50.0F;
        description.surfaceCloudIntensity = 0.1F;
        description.hdrLuminanceKey = 4.0F;
        description.hdrBrightThreshold = 4.5F;
        description.hdrGaussianScalar = 20.0F;
        description.hdrExposure = 3.0F;
        break;
    case EnvironmentWorldType::World4:
        description.surface = EnvironmentSurface::Magma;
        description.surfaceHeight = 0.5F;
        description.surfaceScroll = 0.01F;
        description.surfaceTileScale = 25.0F;
        description.surfaceCloudIntensity = 1.0F;
        description.hdrLuminanceKey = 1.9F;
        description.hdrBrightThreshold = 1.9F;
        description.hdrGaussianScalar = 30.0F;
        description.hdrExposure = 8.0F;
        break;
    case EnvironmentWorldType::World5:
        description.planarReflection = true;
        description.hdrLuminanceKey = 1.1F;
        description.hdrBrightThreshold = 1.3F;
        description.hdrGaussianScalar = 30.0F;
        description.hdrExposure = 15.0F;
        break;
    case EnvironmentWorldType::World6:
        description.surface = EnvironmentSurface::GroundFog;
        description.surfaceHeight = 3.0F;
        description.surfaceScroll = 0.02F;
        description.surfaceTileScale = 50.0F;
        description.surfaceCloudIntensity = 0.1F;
        description.hdrLuminanceKey = 1.7F;
        description.hdrBrightThreshold = 1.9F;
        description.hdrGaussianScalar = 30.0F;
        description.hdrExposure = 8.0F;
        break;
    case EnvironmentWorldType::Garage:
        description.dynamicReflectionsEnabled = false;
        description.lamps[0].enabled = true;
        description.lamps[0].range = 20.0F;
        description.lamps[1].enabled = true;
        description.lamps[1].range = 20.0F;
        description.hdrLuminanceKey = 2.0F;
        description.hdrBrightThreshold = 4.0F;
        description.hdrGaussianScalar = 25.0F;
        description.hdrExposure = 2.0F;
        break;
    case EnvironmentWorldType::Angar:
        description.dynamicReflectionsEnabled = false;
        description.lamps[0].enabled = true;
        description.lamps[0].range = 80.0F;
        description.lamps[1].enabled = true;
        description.lamps[1].range = 80.0F;
        description.lamps[2].enabled = true;
        description.lamps[2].range = 100.0F;
        description.hdrLuminanceKey = 3.0F;
        description.hdrBrightThreshold = 3.5F;
        description.hdrGaussianScalar = 20.0F;
        description.hdrExposure = 5.0F;
        break;
    }
}

void Environment::ApplyPresentation(
    EnvironmentDescription& description,
    EnvironmentWorldType worldType)
{
    ApplyWeather(description, Weather::Fair, worldType);
    ApplyWorldType(description, worldType);
    description.ambientColor = garageAmbient;
    description.fogIntensity = 1.0F;
    description.perspectiveFarDistance =
        worldType == EnvironmentWorldType::Garage ? 20.0F : 130.0F;
    description.skyEnabled = false;
    description.fogEnabled = false;
    description.directionalLightEnabled = false;
    description.rain = false;
}

EnvironmentRenderPolicy Environment::ApplyQuality(
    const EnvironmentDescription& description,
    const QualityConfig& quality, bool isometricCamera) noexcept
{
    EnvironmentRenderPolicy result;
    result.environmentQuality = std::min(quality.environment, 2U);
    result.lightQuality = std::min(quality.light, 2U);
    result.directionalShadows =
        quality.shadow >=
            description.directionalShadowMinimumQuality &&
        description.directionalLightEnabled;
    result.spotShadows =
        quality.shadow >= 1U &&
        !description.directionalLightEnabled &&
        EnabledLampCount(description) > 0U;
    result.shadows = result.directionalShadows || result.spotShadows;
    result.pixelLighting = result.lightQuality >= 1U;
    result.bumpMapping = result.lightQuality >= 2U;
    result.reflectionMapping = result.lightQuality >= 1U;
    result.trueReflections =
        result.lightQuality >= 2U &&
        description.dynamicReflectionsEnabled;
    result.planarReflections = result.lightQuality >= 2U;
    const bool weatherAllowsPostEffects =
        description.weather != Weather::Night;
    result.bloom = quality.postEffect >= 1U &&
                   weatherAllowsPostEffects;
    result.refraction = quality.postEffect >= 1U;
    result.hdr = quality.postEffect >= 2U &&
                 weatherAllowsPostEffects;
    result.sunShaft = quality.postEffect >= 2U &&
                      weatherAllowsPostEffects &&
                      description.directionalLightEnabled &&
                      !isometricCamera;
    result.sky = description.skyEnabled &&
                 result.environmentQuality >= 1U;
    result.fog = description.fogEnabled && !isometricCamera;
    const bool hasWater =
        description.surface == EnvironmentSurface::Water;
    result.highQualityWater =
        hasWater && result.environmentQuality >= 1U;
    result.volumeFog =
        result.environmentQuality >= 1U &&
        (description.surface == EnvironmentSurface::GroundFog ||
         description.surface == EnvironmentSurface::Magma);
    result.sceneDepthSurface =
        result.highQualityWater || result.volumeFog;
    result.environmentReflection =
        result.planarReflections &&
        (description.planarReflection || result.highQualityWater);
    result.grassField =
        result.environmentQuality >= 1U &&
        description.surface == EnvironmentSurface::Grass;
    return result;
}

float Environment::CameraFar(
    const EnvironmentDescription& description, bool isometricCamera,
    bool editMode) noexcept
{
    if (editMode)
        return 1000.0F;
    return isometricCamera ? 150.0F
                           : description.perspectiveFarDistance;
}

void Environment::StartScene(
    const EnvironmentDescription& description, bool isometricCamera,
    const r3d::physics::Vec3& cameraPosition) noexcept
{
    if (sceneStarted_)
        return;
    sceneStarted_ = true;
    ApplyRain(description, isometricCamera, cameraPosition);
}

void Environment::ReleaseScene() noexcept
{
    if (!sceneStarted_)
        return;
    sceneStarted_ = false;
    rainVisible_ = false;
}

void Environment::ProcessScene(
    const EnvironmentDescription& description, bool isometricCamera,
    const r3d::physics::Vec3& cameraPosition) noexcept
{
    if (!sceneStarted_)
        return;
    if (isometricRain_ != isometricCamera ||
        rainVisible_ != (description.rain && !isometricCamera))
    {
        ApplyRain(description, isometricCamera, cameraPosition);
    }
    if (rainVisible_)
        rainPosition_ = cameraPosition;
}

bool Environment::SceneStarted() const noexcept
{
    return sceneStarted_;
}

bool Environment::RainVisible() const noexcept
{
    return rainVisible_;
}

bool Environment::IsometricRain() const noexcept
{
    return isometricRain_;
}

const r3d::physics::Vec3& Environment::RainPosition() const noexcept
{
    return rainPosition_;
}

void Environment::ApplyRain(
    const EnvironmentDescription& description, bool isometricCamera,
    const r3d::physics::Vec3& cameraPosition) noexcept
{
    isometricRain_ = isometricCamera;
    rainVisible_ =
        sceneStarted_ && description.rain && !isometricCamera;
    if (rainVisible_)
        rainPosition_ = cameraPosition;
}

} // namespace r3d::game::originalrace::source
