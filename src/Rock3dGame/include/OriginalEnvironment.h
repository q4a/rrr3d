#pragma once

#include "OriginalProfile.h"
#include "OriginalRace.h"

#include <cstdint>
#include <string_view>

namespace r3d::game::originalrace::source
{

enum class EnvironmentWorldType
{
    World1,
    World2,
    World3,
    World4,
    World5,
    World6,
    Garage,
    Angar,
};

// Backend-neutral result of Environment::ApplyQuality/SetGraphOption.  The
// renderer executes these decisions but does not recreate their thresholds.
struct EnvironmentRenderPolicy
{
    std::uint32_t environmentQuality = 0U;
    std::uint32_t lightQuality = 0U;
    bool directionalShadows = false;
    bool spotShadows = false;
    bool shadows = false;
    bool pixelLighting = false;
    bool bumpMapping = false;
    bool reflectionMapping = false;
    bool trueReflections = false;
    bool planarReflections = false;
    bool bloom = false;
    bool refraction = false;
    bool hdr = false;
    bool sunShaft = false;
    bool sky = false;
    bool fog = false;
    bool highQualityWater = false;
    bool volumeFog = false;
    bool sceneDepthSurface = false;
    bool environmentReflection = false;
    bool grassField = false;
};

// Portable owner for the platform-independent part of Environment.cpp.
// GraphManager lights/options and MapObj rain creation are represented as a
// description, a render policy and a scene-lifetime state respectively.
class Environment
{
public:
    static Weather WeatherFromToken(std::string_view token) noexcept;
    static std::string_view WeatherToken(Weather weather) noexcept;
    static EnvironmentWorldType WorldTypeFromLevelPath(
        std::string_view levelPath) noexcept;

    static void ApplyWeather(EnvironmentDescription& description,
                             Weather weather,
                             EnvironmentWorldType worldType);
    static void ApplyWeatherToken(EnvironmentDescription& description,
                                  std::string_view token,
                                  std::string_view levelPath);
    static void ApplyWorldType(EnvironmentDescription& description,
                               EnvironmentWorldType worldType);
    static void ApplyPresentation(EnvironmentDescription& description,
                                  EnvironmentWorldType worldType);
    static EnvironmentRenderPolicy ApplyQuality(
        const EnvironmentDescription& description,
        const QualityConfig& quality, bool isometricCamera) noexcept;
    static float CameraFar(const EnvironmentDescription& description,
                           bool isometricCamera,
                           bool editMode = false) noexcept;

    void StartScene(const EnvironmentDescription& description,
                    bool isometricCamera,
                    const r3d::physics::Vec3& cameraPosition) noexcept;
    void ReleaseScene() noexcept;
    void ProcessScene(const EnvironmentDescription& description,
                      bool isometricCamera,
                      const r3d::physics::Vec3& cameraPosition) noexcept;

    bool SceneStarted() const noexcept;
    bool RainVisible() const noexcept;
    bool IsometricRain() const noexcept;
    const r3d::physics::Vec3& RainPosition() const noexcept;

private:
    void ApplyRain(const EnvironmentDescription& description,
                   bool isometricCamera,
                   const r3d::physics::Vec3& cameraPosition) noexcept;

    r3d::physics::Vec3 rainPosition_;
    bool sceneStarted_ = false;
    bool rainVisible_ = false;
    bool isometricRain_ = false;
};

} // namespace r3d::game::originalrace::source
