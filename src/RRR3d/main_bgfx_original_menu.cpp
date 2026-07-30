#include "CoreTextRasterizer.h"
#include "OriginalAudioSpec.h"
#include "OriginalMainMenu.h"
#ifdef RRR3D_PHYSICS
#include "OriginalGarage.h"
#include "OriginalProfile.h"
#include "OriginalRace.h"
#include "OriginalRaceHud.h"
#include "OriginalRaceRenderer.h"
#include "OriginalRaceSession.h"
#include "OriginalWorkshopRenderer.h"
#include "physics/OriginalVehiclePhysics.h"
#endif
#include "renderer/BgfxGraphicsDevice.h"
#include "resource/ResourceFileSystem.h"
#include "xplatform.h"

#ifdef RRR3D_GAMEPAD_INPUT
#include "SdlInputManager.h"
#include "SdlInputSmoke.h"
#endif
#ifdef RRR3D_AUDIO
#include "OriginalMenuMusic.h"
#ifdef RRR3D_PHYSICS
#include "OriginalRaceCommentator.h"
#endif
#include "SdlAudioBackend.h"
#include "SdlAudioSmoke.h"
#include "audio/AudioBackend.h"
#endif

#include <SDL3/SDL.h>
#include <bx/math.h>

#include "rrr3d_fs_static_scene.bin.h"
#include "rrr3d_vs_static_scene.bin.h"
#ifdef RRR3D_PHYSICS
#include "rrr3d_fs_original_race.bin.h"
#include "rrr3d_vs_original_race.bin.h"
#endif

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using namespace r3d::renderer;
namespace menu = r3d::game::mainmenu2;
namespace originalaudio = r3d::game::originalaudio;

constexpr int initialWidth = 1280;
constexpr int initialHeight = 733;
constexpr std::string_view smokePrefix = "--smoke-test-frames=";
constexpr std::string_view dataPrefix = "--data-dir=";
constexpr std::string_view languagePrefix = "--language=";
constexpr std::string_view trackPrefix = "--track=";
constexpr std::string_view carPrefix = "--car=";
constexpr std::string_view weatherPrefix = "--weather=";

struct Options
{
    std::uint32_t smokeFrames = 0;
    std::filesystem::path dataDirectory;
    std::string language;
    bool languageSelected = false;
    bool verifyResources = false;
#ifdef RRR3D_PHYSICS
    bool physicsSmokeTest = false;
    bool raceRenderSmokeTest = false;
    std::uint32_t trackIndex = 0;
    bool trackSelected = false;
    std::string car;
    bool carSelected = false;
    std::string weather = "fair";
    bool weatherSelected = false;
#endif
#ifdef RRR3D_GAMEPAD_INPUT
    bool inputSmokeTest = false;
#endif
#ifdef RRR3D_AUDIO
    bool audioSmokeTest = false;
#endif
};

#ifdef RRR3D_PHYSICS
struct OriginalAchievementVisual
{
    std::string_view name;
    std::string_view lockedImage;
    std::string_view openedImage;
    float x;
    float y;
};

// RaceMenu2::AchievmentFrame::UpdateAchievments keeps these arrays in Box
// order.  armor4 intentionally uses the original musicTrack artwork.
constexpr std::array<OriginalAchievementVisual, 9>
    originalAchievementVisuals{{
        {"viper", "Data/GUI/Rewards/viperLock.png",
         "Data/GUI/Rewards/viper.png", 200.0F, 105.0F},
        {"buggi", "Data/GUI/Rewards/buggiLock.png",
         "Data/GUI/Rewards/buggi.png", 405.0F, 155.0F},
        {"airblade", "Data/GUI/Rewards/airbladeLock.png",
         "Data/GUI/Rewards/airblade.png", 0.0F, 245.0F},
        {"reflector", "Data/GUI/Rewards/reflectorLock.png",
         "Data/GUI/Rewards/reflector.png", -190.0F, 125.0F},
        {"droid", "Data/GUI/Rewards/droidLock.png",
         "Data/GUI/Rewards/droid.png", -380.0F, 95.0F},
        {"tankchetti", "Data/GUI/Rewards/tankchettiLock.png",
         "Data/GUI/Rewards/tankchetti.png", -325.0F, 265.0F},
        {"phaser", "Data/GUI/Rewards/phaserLock.png",
         "Data/GUI/Rewards/phaser.png", -190.0F, 400.0F},
        {"mustang", "Data/GUI/Rewards/mustangLock.png",
         "Data/GUI/Rewards/mustang.png", 205.0F, 375.0F},
        {"armor4", "Data/GUI/Rewards/musicTrackLock.png",
         "Data/GUI/Rewards/musicTrack.png", 445.0F, 315.0F},
    }};

constexpr std::size_t originalAchievementBack =
    originalAchievementVisuals.size();
constexpr std::size_t originalAchievementNoTarget =
    originalAchievementBack + 1U;

// Menu::NavDir order is left, right, up, down.  This is the literal
// AchievmentFrame navigation graph; locked buttons are skipped by following
// the same direction, as Menu::NavElementFind did on Windows.
constexpr std::array<std::array<std::size_t, 4>, 10>
    originalAchievementNavigation{{
        {3U, 1U, originalAchievementBack, 2U}, // viper
        {0U, 4U, originalAchievementBack, 8U}, // buggi
        {5U, 8U, 3U, 6U},                     // airblade
        {4U, 0U, originalAchievementBack, 5U}, // reflector
        {1U, 3U, originalAchievementBack, 5U}, // droid
        {8U, 2U, 4U, 6U},                     // tankchetti
        {5U, 7U, 2U, originalAchievementBack}, // phaser
        {6U, 8U, 2U, originalAchievementBack}, // mustang
        {7U, 5U, 1U, originalAchievementBack}, // musicTrack
        {originalAchievementNoTarget, originalAchievementNoTarget, 6U,
         6U}, // Back
    }};
#endif

struct TextVisual
{
    Texture texture;
    float width = 0.0F;
    float height = 0.0F;
};

std::string_view recordName(std::string_view record)
{
    const auto separator = record.find_last_of("\\/");
    return record.substr(separator == std::string_view::npos
                             ? 0
                             : separator + 1);
}

#ifdef RRR3D_AUDIO
template <std::size_t Count>
std::vector<r3d::game::MusicCatTrack> musicTracks(
    const std::array<originalaudio::TrackSpec, Count>& source)
{
    std::vector<r3d::game::MusicCatTrack> result;
    result.reserve(source.size());
    for (const auto& track : source)
        result.push_back(
            {track.path, track.name, track.band, track.group});
    return result;
}

std::vector<std::size_t> musicPlaylist(std::string_view source,
                                       std::size_t trackCount)
{
    std::vector<std::size_t> result;
    while (!source.empty())
    {
        const auto separator = source.find(',');
        const auto token = source.substr(0, separator);
        std::size_t index = 0;
        const auto parsed = std::from_chars(
            token.data(), token.data() + token.size(), index);
        if (parsed.ec == std::errc{} &&
            parsed.ptr == token.data() + token.size() &&
            index < trackCount &&
            std::find(result.begin(), result.end(), index) ==
                result.end())
        {
            result.push_back(index);
        }
        if (separator == std::string_view::npos)
            break;
        source.remove_prefix(separator + 1U);
    }
    return result;
}
#endif

std::optional<Options> parseOptions(int argc, char** argv)
{
    Options options;
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);
        if (argument == "--verify-resources")
        {
            options.verifyResources = true;
            continue;
        }
#ifdef RRR3D_PHYSICS
        if (argument.substr(0, trackPrefix.size()) == trackPrefix)
        {
            const auto value = argument.substr(trackPrefix.size());
            const auto result = std::from_chars(
                value.data(), value.data() + value.size(),
                options.trackIndex);
            if (result.ec != std::errc{} ||
                result.ptr != value.data() + value.size())
                return std::nullopt;
            options.trackSelected = true;
            continue;
        }
        if (argument.substr(0, carPrefix.size()) == carPrefix)
        {
            options.car = argument.substr(carPrefix.size());
            if (options.car.empty())
                return std::nullopt;
            options.carSelected = true;
            continue;
        }
        if (argument.substr(0, weatherPrefix.size()) == weatherPrefix)
        {
            options.weather = argument.substr(weatherPrefix.size());
            if (options.weather != "fair" &&
                options.weather != "night" &&
                options.weather != "cloudy" &&
                options.weather != "rainy" &&
                options.weather != "sahara" &&
                options.weather != "hell" &&
                options.weather != "snow")
                return std::nullopt;
            options.weatherSelected = true;
            continue;
        }
        if (argument == "--physics-smoke-test")
        {
            options.physicsSmokeTest = true;
            continue;
        }
        if (argument == "--race-render-smoke-test")
        {
            options.raceRenderSmokeTest = true;
            if (options.smokeFrames == 0)
                options.smokeFrames = 240;
            continue;
        }
#endif
#ifdef RRR3D_GAMEPAD_INPUT
        if (argument == "--input-smoke-test")
        {
            options.inputSmokeTest = true;
            if (options.smokeFrames == 0)
                options.smokeFrames = 120;
            continue;
        }
#endif
#ifdef RRR3D_AUDIO
        if (argument == "--audio-smoke-test")
        {
            options.audioSmokeTest = true;
            if (options.smokeFrames == 0)
                options.smokeFrames = 180;
            continue;
        }
#endif
        if (argument.substr(0, dataPrefix.size()) == dataPrefix)
        {
            const auto value = argument.substr(dataPrefix.size());
            if (value.empty())
                return std::nullopt;
            options.dataDirectory = std::filesystem::path(value);
            continue;
        }
        if (argument.substr(0, languagePrefix.size()) == languagePrefix)
        {
            options.language = argument.substr(languagePrefix.size());
            if (options.language.empty())
                return std::nullopt;
            options.languageSelected = true;
            continue;
        }
        if (argument.substr(0, smokePrefix.size()) == smokePrefix)
        {
            const auto value = argument.substr(smokePrefix.size());
            const auto result = std::from_chars(
                value.data(), value.data() + value.size(),
                options.smokeFrames);
            if (result.ec != std::errc{} ||
                result.ptr != value.data() + value.size() ||
                options.smokeFrames == 0)
                return std::nullopt;
            continue;
        }
        return std::nullopt;
    }
    if (options.language.empty())
        options.language = rrr3d::macos::preferredGameLanguage();
    return options;
}

bool valid(Texture value)
{
    return value.value != invalid_resource;
}

bool valid(Mesh value)
{
    return value.vertices.value != invalid_resource &&
           value.indices.value != invalid_resource;
}

bool valid(Shader value)
{
    return value.value != invalid_resource;
}

Camera makeCamera(const GraphicsDevice& device)
{
    Camera camera;
    bx::mtxIdentity(camera.view.data());
    bx::mtxOrtho(camera.projection.data(), 0.0F, menu::virtualWidth,
                 menu::virtualHeight, 0.0F, 0.0F, 100.0F, 0.0F,
                 device.usesHomogeneousDepth());
    return camera;
}

#ifdef RRR3D_PHYSICS
r3d::physics::VehicleState makeGarageVehicleState(
    const r3d::game::originalrace::Vehicle& vehicle)
{
    r3d::physics::VehicleState result;
    float wheelOffset = 0.0F;
    const std::size_t wheelCount = std::min(
        vehicle.physics.wheels.size(),
        vehicle.wheelVisualOffsets.size());
    for (std::size_t index = 0; index < wheelCount; ++index)
    {
        const auto& wheel = vehicle.physics.wheels[index];
        const auto& offset = vehicle.wheelVisualOffsets[index];
        const float adjustedZ =
            wheel.position.z - 0.5F * wheel.suspensionTravel +
            offset.z;
        // CarFrame::SetCar overwrites this on every wheel; four-wheel
        // source cars therefore use the final serialized wheel exactly.
        wheelOffset =
            std::abs(adjustedZ) + wheel.radius + offset.z;
    }
    result.body.position = {0.0F, 0.0F, wheelOffset - 0.71F};
    result.wheels.reserve(wheelCount);
    result.wheelAngularSpeeds.assign(wheelCount, 0.0F);
    result.wheelContacts.resize(wheelCount);
    for (std::size_t index = 0; index < wheelCount; ++index)
    {
        const auto& wheel = vehicle.physics.wheels[index];
        r3d::physics::Transform state;
        state.position = {
            wheel.position.x,
            wheel.position.y,
            result.body.position.z + wheel.position.z -
                0.5F * wheel.suspensionTravel};
        if (index < vehicle.wheelVisualTransforms.size() &&
            vehicle.wheelVisualTransforms[index].scale.y < 0.0F)
        {
            // CarWheel::invertWheel in CarFrame::SetCar.
            state.rotation = {0.0F, 0.0F, 1.0F, 0.0F};
        }
        result.wheels.push_back(state);
    }
    return result;
}

r3d::game::originalrace::PresentationCamera
makeWorkshopPresentationCamera(
    const r3d::game::originalrace::PresentationCamera& current) noexcept
{
    using Camera =
        r3d::game::originalrace::PresentationCamera;
    using Vec3 = r3d::physics::Vec3;
    using Quat = r3d::physics::Quat;
    constexpr std::array<Vec3, 8> positions{{
        {-5.6156259F, 4.3894496F, 1.3072476F},
        {1.0063084F, 6.9253764F, 1.7360222F},
        {5.6724834F, 4.9537153F, 1.3952403F},
        {7.0655332F, -1.0402107F, 1.2024049F},
        {5.4610982F, -5.3067584F, 1.2650701F},
        {-1.1062316F, -7.5020962F, 1.1599010F},
        {-5.9399834F, -4.8825927F, 1.0367264F},
        {-7.4102926F, 0.61909121F, 1.1492375F},
    }};
    constexpr std::array<Quat, 8> rotations{{
        {0.021078700F, 0.10266567F, -0.20001189F, 0.97417259F},
        {0.093931124F, 0.099822313F, -0.67882264F, 0.72140080F},
        {0.098612130F, 0.053653944F, -0.87284911F, 0.47491166F},
        {0.10475823F, 0.0032271212F, -0.99402952F, 0.030623097F},
        {0.10137362F, -0.026610103F, -0.96191424F, -0.25249690F},
        {0.070100352F, -0.063905962F, -0.73568070F, -0.67066783F},
        {0.041057255F, -0.077143900F, -0.46803388F, -0.87939179F},
        {0.0077582477F, -0.092043117F, -0.083636492F, -0.99221748F},
    }};
    auto length = [](const Quat& value) {
        return std::sqrt(
            value.x * value.x + value.y * value.y +
            value.z * value.z + value.w * value.w);
    };
    const float currentLength = std::max(length(current.rotation), 0.0001F);
    std::size_t nearest = 0U;
    float smallestAngle = std::numeric_limits<float>::max();
    for (std::size_t index = 0U; index < rotations.size(); ++index)
    {
        const auto& candidate = rotations[index];
        const float dot =
            current.rotation.x * candidate.x +
            current.rotation.y * candidate.y +
            current.rotation.z * candidate.z +
            current.rotation.w * candidate.w;
        const float cosine = std::clamp(
            std::abs(dot) /
                (currentLength * std::max(length(candidate), 0.0001F)),
            0.0F, 1.0F);
        const float angle = std::acos(cosine) * 2.0F;
        if (angle < smallestAngle)
        {
            smallestAngle = angle;
            nearest = index;
        }
    }
    Camera result = current;
    result.position = positions[nearest];
    result.rotation = rotations[nearest];
    return result;
}
#endif

Transform makeTransform(float width, float height, float centerX,
                        float centerY, float depth)
{
    Transform transform;
    bx::mtxSRT(transform.matrix.data(), width, height, 1.0F, 0.0F, 0.0F,
               0.0F, centerX, centerY, depth);
    return transform;
}

Texture createImageTexture(GraphicsDevice& device, const menu::Image& image)
{
    if (image.storage == menu::ImageStorage::EncodedContainer)
    {
        return device.createTextureContainer(image.bytes.data(),
                                             image.bytes.size(),
                                             image.virtualPath);
    }
    return device.createTextureRgba8(image.width, image.height,
                                     image.bytes.data(), image.bytes.size());
}

TextVisual createText(GraphicsDevice& device, std::string_view text,
                      float pointSize, bool bold, menu::Rgba8 color,
                      std::string& resolvedFont)
{
    auto bitmap = rrr3d::macos::rasterizeText(
        text, menu::fontFace, pointSize, bold, color);
    if (resolvedFont.empty())
        resolvedFont = bitmap.resolvedFontName;
    const Texture texture = device.createTextureRgba8(
        bitmap.width, bitmap.height, bitmap.rgba.data(), bitmap.rgba.size());
    return {texture, static_cast<float>(bitmap.width),
            static_cast<float>(bitmap.height)};
}

void drawQuad(GraphicsDevice& device, Mesh quad, Shader shader,
              Texture texture, float width, float height, float centerX,
              float centerY, float depth, const PipelineState& pipeline)
{
    device.draw(quad, shader, texture,
                makeTransform(width, height, centerX, centerY, depth),
                pipeline);
}

void drawQuadRotated(GraphicsDevice& device, Mesh quad, Shader shader,
                     Texture texture, float width, float height,
                     float centerX, float centerY, float depth,
                     float rotation, const PipelineState& pipeline)
{
    Transform transform;
    bx::mtxSRT(
        transform.matrix.data(), width, height, 1.0F, 0.0F, 0.0F,
        rotation, centerX, centerY, depth);
    device.draw(quad, shader, texture, transform, pipeline);
}

#ifdef RRR3D_GAMEPAD_INPUT
std::optional<std::size_t> hoveredItem(SDL_Window* window, float windowX,
                                       float windowY, std::size_t itemCount,
                                       float itemWidth, float itemHeight)
{
    int windowWidth = 0;
    int windowHeight = 0;
    if (!SDL_GetWindowSize(window, &windowWidth, &windowHeight) ||
        windowWidth <= 0 || windowHeight <= 0)
        return std::nullopt;

    const float virtualX =
        windowX * menu::virtualWidth / static_cast<float>(windowWidth);
    const float virtualY =
        windowY * menu::virtualHeight / static_cast<float>(windowHeight);
    const float centerX =
        menu::virtualWidth * 0.5F + menu::itemCenterOffsetX;
    if (std::abs(virtualX - centerX) > itemWidth * 0.5F)
        return std::nullopt;
    for (std::size_t index = 0; index < itemCount; ++index)
    {
        const float centerY = menu::virtualHeight * 0.5F +
                              menu::firstItemOffsetY +
                              static_cast<float>(index) * menu::itemSpacing;
        if (std::abs(virtualY - centerY) <= itemHeight * 0.5F)
            return index;
    }
    return std::nullopt;
}
#endif

#ifdef RRR3D_AUDIO
std::string dataAudioPath(std::string_view legacyPath)
{
    std::string path = "Data\\";
    path.append(legacyPath);
    return path;
}
#endif

#ifdef RRR3D_PHYSICS
void applyWeather(
    r3d::game::originalrace::EnvironmentDescription& environment,
    std::string_view weather)
{
    using r3d::game::originalrace::Weather;
    environment.rain = false;
    if (weather == "night")
    {
        environment.weather = Weather::Night;
        environment.skyTexturePath = "Data/Misc/nightSky.dds";
        environment.fogColor = {15.0F / 255.0F, 25.0F / 255.0F,
                                31.0F / 255.0F, 1.0F};
        environment.ambientColor =
            {138.0F / 255.0F, 144.0F / 255.0F,
             174.0F / 255.0F, 1.0F};
        environment.fogIntensity = 1.0F;
    }
    else if (weather == "cloudy" || weather == "rainy")
    {
        environment.weather =
            weather == "rainy" ? Weather::Rainy : Weather::Cloudy;
        environment.skyTexturePath =
            "Data/World2/texture/skyTex1.dds";
        environment.fogColor =
            {192.0F / 255.0F, 189.0F / 255.0F,
             184.0F / 255.0F, 0.0F};
        environment.ambientColor = {0.0F, 0.0F, 0.0F, 1.0F};
        environment.fogIntensity = 1.0F;
        environment.rain = weather == "rainy";
    }
    else if (weather == "sahara")
    {
        environment.weather = Weather::Sahara;
        environment.skyTexturePath =
            "Data/World3/Texture/skyTex1.dds";
        environment.fogColor =
            {87.0F / 255.0F, 81.0F / 255.0F,
             115.0F / 255.0F, 1.0F};
        environment.ambientColor = {0.0F, 0.0F, 0.0F, 1.0F};
        environment.fogIntensity = 0.5F;
    }
    else if (weather == "hell")
    {
        environment.weather = Weather::Hell;
        environment.skyTexturePath =
            "Data/World4/Texture/skyTex1.dds";
        environment.fogColor =
            {82.0F / 255.0F, 12.0F / 255.0F,
             8.0F / 255.0F, 1.0F};
        environment.ambientColor = {0.0F, 0.0F, 0.0F, 1.0F};
        environment.fogIntensity = 0.5F;
    }
    else if (weather == "snow")
    {
        environment.weather = Weather::Snow;
        environment.skyTexturePath =
            "Data/World5/Texture/sky_text.dds";
        environment.fogColor =
            {156.0F / 255.0F, 166.0F / 255.0F,
             181.0F / 255.0F, 1.0F};
        environment.ambientColor = {0.0F, 0.0F, 0.0F, 1.0F};
        environment.fogIntensity = 0.5F;
    }
    else
    {
        environment.weather = Weather::Fair;
        environment.skyTexturePath =
            "Data/World1/Texture/skyTex1.dds";
        environment.fogColor =
            {148.0F / 255.0F, 193.0F / 255.0F,
             235.0F / 255.0F, 1.0F};
        environment.ambientColor = {0.0F, 0.0F, 0.0F, 1.0F};
        environment.fogIntensity = 0.5F;
    }
}
#endif

} // namespace

int main(int argc, char** argv)
{
    const auto options = parseOptions(argc, argv);
    if (!options)
    {
        std::cerr << "Usage: RRR3d [--data-dir=PATH] "
                     "[--language=english|russian] [--verify-resources] "
                     "[--smoke-test-frames=N]"
#ifdef RRR3D_GAMEPAD_INPUT
                     " [--input-smoke-test]"
#endif
#ifdef RRR3D_AUDIO
                     " [--audio-smoke-test]"
#endif
#ifdef RRR3D_PHYSICS
                     " [--track=0..87] [--car=garage-record] "
                     "[--weather=fair|night|cloudy|rainy|sahara|hell|snow] "
                     "[--physics-smoke-test] [--race-render-smoke-test]"
#endif
                     "\n";
        return EXIT_FAILURE;
    }

    std::string directoryError;
    if (!rrr3d::platform::ensure_application_directories(directoryError))
    {
        std::cerr << "Application-directory setup failed: "
                  << directoryError << '\n';
        return EXIT_FAILURE;
    }

    const auto dataDirectory =
        options->dataDirectory.empty()
            ? r3d::resource::defaultGameDataDirectory()
            : options->dataDirectory;

    std::optional<r3d::resource::ResourceFileSystem> resources;
    std::optional<menu::Model> model;
    std::string activeLanguage = options->language;
#ifdef RRR3D_PHYSICS
    std::optional<r3d::game::originalrace::Race> originalRace;
    std::optional<r3d::game::originalrace::Race> originalGarageScene;
    std::optional<r3d::game::originalrace::Race> originalAngarScene;
    std::optional<r3d::game::originalrace::OriginalGarageCatalog>
        originalGarage;
    std::optional<r3d::physics::WorldDescription> physicsDescription;
    r3d::game::originalrace::OriginalProfileStore profileStore(
        rrr3d::platform::save_directory(), dataDirectory);
    std::string profileWarning;
    auto profileState = profileStore.load(profileWarning);
    if (!profileWarning.empty())
        std::cerr << "Profile import warning: " << profileWarning << '\n';
    if (options->physicsSmokeTest)
    {
        profileState =
            r3d::game::originalrace::makeOriginalDefaultProfileState();
    }
    if (options->languageSelected)
        profileState.config.language = options->language;
    else
        activeLanguage = profileState.config.language;
    std::size_t selectedTrack =
        options->trackSelected ? options->trackIndex : 0U;
    const std::string selectedCar =
        options->carSelected ? options->car
                             : profileState.player.currentCar;
#endif
    try
    {
        resources.emplace(dataDirectory);
        model.emplace(
            menu::loadOriginalMainMenu(*resources, activeLanguage));
#ifdef RRR3D_PHYSICS
        originalGarage.emplace(
            r3d::game::originalrace::loadOriginalGarage(*resources));
        originalRace.emplace(r3d::game::originalrace::loadOriginalRace(
            *resources, selectedTrack, selectedCar));
        // The provenance/physics smoke has exact World1/map1 assertions and
        // must not depend on whichever tournament track a prior GUI run
        // persisted in user.xml.
        if (!options->trackSelected && !options->physicsSmokeTest)
        {
            selectedTrack =
                r3d::game::originalrace::resolveOriginalTournamentTrack(
                    *originalRace, profileState.player);
            if (selectedTrack != 0U)
            {
                originalRace.emplace(
                    r3d::game::originalrace::loadOriginalRace(
                        *resources, selectedTrack, selectedCar));
            }
        }
        if (options->weatherSelected)
            applyWeather(originalRace->environment, options->weather);
        profileState.player.currentCar = originalRace->vehicle.record;
        if (!originalRace->racers.empty())
            originalRace->racers.front().name =
                profileState.player.name;
        r3d::game::originalrace::applyOriginalPlayerProfile(
            *originalRace, *resources, profileState.player);
        r3d::game::originalrace::writeOriginalTournamentSelection(
            *originalRace, selectedTrack, profileState.player);
        originalGarageScene.emplace(
            r3d::game::originalrace::loadOriginalGarageScene(
                *resources, *originalRace));
        originalAngarScene.emplace(
            r3d::game::originalrace::loadOriginalAngarScene(
                *resources));
        physicsDescription.emplace(
            r3d::game::originalrace::makePhysicsDescription(
                *originalRace, *resources));
#endif
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Original menu resource loading failed: "
                  << exception.what() << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "Original game-data root: " << resources->root()
              << '\n'
              << "MainMenu2 language: " << model->language << '\n'
              << "MainMenu2 strings:";
    for (const auto& item : model->items)
        std::cout << " [" << item << ']';
    std::cout << '\n'
              << "Original asset audit: " << model->audit.catalogFiles
              << " files (" << model->audit.catalogBytes << " bytes), "
              << model->audit.guiImages << " GUI images, "
              << model->audit.guiMeshes << " GUI meshes, "
              << model->audit.localizedStrings << " localized strings\n";

    if (options->verifyResources)
    {
        std::cout << "Milestone 6 original resource/MainMenu2 specification "
                     "verification passed\n";
#ifdef RRR3D_PHYSICS
        if (originalRace->vehicle.physics.maximumTorque <= 0.0F ||
            originalRace->vehicle.maximumLife <= 1.0F)
        {
            std::cerr
                << "Original player mobility loadout audit failed: torque="
                << originalRace->vehicle.physics.maximumTorque
                << ", life=" << originalRace->vehicle.maximumLife << '\n';
            return EXIT_FAILURE;
        }
        std::cout << "Milestone 9 selected race audit passed: track "
                  << selectedTrack << ' ' << originalRace->levelPath
                  << ", " << originalRace->trackInstances.size()
                  << " track objects, "
                  << originalRace->decorationInstances.size()
                  << " decorations, " << originalRace->bonuses.size()
                  << " bonuses, " << originalRace->racers.size()
                  << " racers, car "
                  << recordName(originalRace->vehicle.record)
                  << ", torque "
                  << originalRace->vehicle.physics.maximumTorque
                  << ", life " << originalRace->vehicle.maximumLife
                  << '\n';
#endif
        return EXIT_SUCCESS;
    }

#ifdef RRR3D_PHYSICS
    if (options->physicsSmokeTest)
    {
        std::string physicsError;
        if (!r3d::game::originalrace::runOriginalRaceResourceSmokeTest(
                *originalRace, *resources, physicsError) ||
            !r3d::game::originalrace::runOriginalGarageSmokeTest(
                *resources, physicsError) ||
            !r3d::game::originalrace::runOriginalRaceSessionSmokeTest(
                *originalRace, physicsError) ||
            !r3d::physics::runOriginalVehiclePhysicsSmokeTest(
                *physicsDescription, physicsError))
        {
            std::cerr << "Milestone 9 original race/physics smoke failed: "
                      << physicsError << '\n';
            return EXIT_FAILURE;
        }
        std::size_t collisionTriangles = 0;
        for (const auto& mesh : physicsDescription->collisionMeshes)
            collisionTriangles += mesh.indices.size() / 3U;
        std::cout << "Milestone 9 original race smoke passed: "
                  << originalRace->levelPath << ", "
                  << originalRace->trackInstances.size()
                  << " placed track objects, " << collisionTriangles
                  << " original collision triangles, "
                  << recordName(originalRace->vehicle.record)
                  << "/Jolt vehicle"
                     " source torque/gears/reverse, braking, steering,"
                     " airborne/stabilization, suspension/tire contacts,"
                     " trace reset, countdown, checkpoint/lap/finish,"
                     " source border/car contacts, weapon/damage, bonus,"
                     " garage/workshop, and respawn state passed\n";
        return EXIT_SUCCESS;
    }
#endif

    if (!SDL_SetAppMetadata("Motor Rock", "1.3.1",
                            "org.rrr3d.motorrock"))
    {
        std::cerr << "Unable to set SDL metadata: " << SDL_GetError()
                  << '\n';
        return EXIT_FAILURE;
    }
    SDL_InitFlags sdlFlags = SDL_INIT_VIDEO;
#ifdef RRR3D_GAMEPAD_INPUT
    sdlFlags |= SDL_INIT_GAMEPAD;
#endif
#ifdef RRR3D_AUDIO
    sdlFlags |= SDL_INIT_AUDIO;
#endif
    if (!SDL_Init(sdlFlags))
    {
        std::cerr << "Unable to initialize SDL3: " << SDL_GetError()
                  << '\n';
        return EXIT_FAILURE;
    }

#ifdef RRR3D_GAMEPAD_INPUT
    rrr3d::input::SdlInputManager input;
    std::string inputError;
    if (!input.initialize(inputError))
    {
        std::cerr << "Input initialization failed: " << inputError << '\n';
        SDL_Quit();
        return EXIT_FAILURE;
    }
#ifdef RRR3D_PHYSICS
    input.applyKeyboardBindings(
        profileState.config.keyboardControls);
    input.applyGamepadBindings(
        profileState.config.gamepadControls);
#endif
    bool runInputSmoke = options->inputSmokeTest;
#ifdef RRR3D_AUDIO
    runInputSmoke = runInputSmoke || options->audioSmokeTest;
#endif
    if (runInputSmoke &&
        (!rrr3d::input::runSdlInputSmokeTest(input, inputError) ||
         !menu::runOriginalMainMenuInputSmoke(inputError)))
    {
        std::cerr << "Milestone 7 input smoke test failed: " << inputError
                  << '\n';
        input.shutdown();
        SDL_Quit();
        return EXIT_FAILURE;
    }
    if (runInputSmoke)
    {
        std::cout << "Milestone 7 input smoke: keyboard, mouse, focus reset, "
                     "gamepad hot-plug, axes, dead zones, buttons, rumble, "
                     "and MainMenu2 command order passed\n";
        input.applyKeyboardBindings(
            profileState.config.keyboardControls);
        input.applyGamepadBindings(
            profileState.config.gamepadControls);
    }
#endif

    SDL_Window* window = SDL_CreateWindow(
#ifdef RRR3D_PHYSICS
        "Motor Rock - Original Race Physics (Milestone 9)", initialWidth,
#elif defined(RRR3D_AUDIO)
        "Motor Rock - Original MainMenu2 Audio (Milestone 8)", initialWidth,
#elif defined(RRR3D_GAMEPAD_INPUT)
        "Motor Rock - Original MainMenu2 Input (Milestone 7)", initialWidth,
#else
        "Motor Rock - Original MainMenu2 (Milestone 6)", initialWidth,
#endif
        initialHeight, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr)
    {
        std::cerr << "Unable to create window: " << SDL_GetError() << '\n';
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }

    const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    void* nativeWindow = SDL_GetPointerProperty(
        properties, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    int pixelWidth = 0;
    int pixelHeight = 0;
    if (nativeWindow == nullptr ||
        !SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight))
    {
        std::cerr << "Unable to obtain Cocoa drawable: " << SDL_GetError()
                  << '\n';
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }

    auto device = createBgfxGraphicsDevice();
    std::string rendererError;
    if (!device->initialize(
            {{nativeWindow}, static_cast<std::uint32_t>(pixelWidth),
             static_cast<std::uint32_t>(pixelHeight), true},
            rendererError))
    {
        std::cerr << "Renderer initialization failed: " << rendererError
                  << '\n';
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }

    constexpr std::array<Vertex, 4> quadVertices{{
        {-0.5F, -0.5F, 0.0F, 0xffffffffU, 0.0F, 0.0F},
        {0.5F, -0.5F, 0.0F, 0xffffffffU, 1.0F, 0.0F},
        {-0.5F, 0.5F, 0.0F, 0xffffffffU, 0.0F, 1.0F},
        {0.5F, 0.5F, 0.0F, 0xffffffffU, 1.0F, 1.0F},
    }};
    constexpr std::array<std::uint16_t, 6> quadIndices{{0, 1, 2, 1, 3, 2}};

    const Shader shader = device->createShader(
        {rrr3d_vs_static_scene, sizeof(rrr3d_vs_static_scene)},
        {rrr3d_fs_static_scene, sizeof(rrr3d_fs_static_scene)},
        "original-main-menu");
#ifdef RRR3D_PHYSICS
    const Shader raceShader = device->createShader(
        {rrr3d_vs_original_race, sizeof(rrr3d_vs_original_race)},
        {rrr3d_fs_original_race, sizeof(rrr3d_fs_original_race)},
        "original-race-r3d");
#endif
    const Mesh quad = device->createMesh(
        quadVertices.data(), quadVertices.size(), quadIndices.data(),
        quadIndices.size());
    const Texture background =
        createImageTexture(*device, model->backgroundImage);
    const Texture topPanel = createImageTexture(*device, model->topPanelImage);
    const Texture bottomPanel =
        createImageTexture(*device, model->bottomPanelImage);
    const Texture selection =
        createImageTexture(*device, model->selectionImage);
    const Texture cursor = createImageTexture(*device, model->cursorImage);
#ifdef RRR3D_PHYSICS
    const auto acceptFrameImage = menu::loadOriginalImage(
        *resources, "Data/GUI/dlgFrame1.png");
    const auto acceptButtonImage = menu::loadOriginalImage(
        *resources, "Data/GUI/dlgButton1.png");
    const auto acceptButtonSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/dlgButtonSel1.png");
    const Texture acceptFrame =
        createImageTexture(*device, acceptFrameImage);
    const Texture acceptButton =
        createImageTexture(*device, acceptButtonImage);
    const Texture acceptButtonSelected =
        createImageTexture(*device, acceptButtonSelectedImage);
    const auto optionsBackgroundImage = menu::loadOriginalImage(
        *resources, "Data/GUI/optionsBg.png");
    const auto optionsRowImage = menu::loadOriginalImage(
        *resources, "Data/GUI/labelBg1.png");
    const auto controlsRowImage = menu::loadOriginalImage(
        *resources, "Data/GUI/labelBg2.png");
    const auto optionsArrowImage = menu::loadOriginalImage(
        *resources, "Data/GUI/arrow3.png");
    const auto optionsArrowSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/arrowSel3.png");
    const auto optionsBarBackgroundImage = menu::loadOriginalImage(
        *resources, "Data/GUI/optBarBg.png");
    const auto optionsBarImage = menu::loadOriginalImage(
        *resources, "Data/GUI/optBar.png");
    const auto optionsButtonImage = menu::loadOriginalImage(
        *resources, "Data/GUI/buttonBg4.png");
    const auto optionsButtonSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/buttonBgSel4.png");
    const auto optionsKeyImage = menu::loadOriginalImage(
        *resources, "Data/GUI/keyBg.png");
    const auto optionsKeySelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/keyBgSel.png");
    const auto keyboardIconImage = menu::loadOriginalImage(
        *resources, "Data/GUI/ctKeyboard.png");
    const auto gamepadIconImage = menu::loadOriginalImage(
        *resources, "Data/GUI/ctGamepad.png");
    const Texture optionsBackground =
        createImageTexture(*device, optionsBackgroundImage);
    const Texture optionsRow =
        createImageTexture(*device, optionsRowImage);
    const Texture controlsRow =
        createImageTexture(*device, controlsRowImage);
    const Texture optionsArrow =
        createImageTexture(*device, optionsArrowImage);
    const Texture optionsArrowSelected =
        createImageTexture(*device, optionsArrowSelectedImage);
    const Texture optionsBarBackground =
        createImageTexture(*device, optionsBarBackgroundImage);
    const Texture optionsBar =
        createImageTexture(*device, optionsBarImage);
    const Texture optionsButton =
        createImageTexture(*device, optionsButtonImage);
    const Texture optionsButtonSelected =
        createImageTexture(*device, optionsButtonSelectedImage);
    const Texture optionsKey =
        createImageTexture(*device, optionsKeyImage);
    const Texture optionsKeySelected =
        createImageTexture(*device, optionsKeySelectedImage);
    const Texture keyboardIcon =
        createImageTexture(*device, keyboardIconImage);
    const Texture gamepadIcon =
        createImageTexture(*device, gamepadIconImage);
    const std::array<std::uint8_t, 4> optionsMaskPixel{
        0U, 0U, 0U, 204U};
    const Texture optionsMask = device->createTextureRgba8(
        1U, 1U, optionsMaskPixel.data(), optionsMaskPixel.size());
    const auto raceTopPanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/topPanel.png");
    const auto raceBottomPanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/bottomPanel.png");
    const auto raceMenuButtonImage = menu::loadOriginalImage(
        *resources, "Data/GUI/buttonBg1.png");
    const auto raceMenuButtonSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/buttonBgSel1.png");
    const auto raceMoneyImage = menu::loadOriginalImage(
        *resources, "Data/GUI/moneyBg.png");
    const auto raceStatsImage = menu::loadOriginalImage(
        *resources, "Data/GUI/statFrame.png");
    const auto raceImageFrameImage = menu::loadOriginalImage(
        *resources, "Data/GUI/imageFrame1.png");
    const auto raceChargeBarImage = menu::loadOriginalImage(
        *resources, "Data/GUI/chargeBar1.png");
    const std::array<menu::Image, 7> raceMenuIconImages{
        menu::loadOriginalImage(*resources, "Data/GUI/icoStart.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/icoWorkshop.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/icoGarage.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/icoSpace.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/icoAchivment.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/icoOptions.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/icoExit.png")};
    const std::array<menu::Image, 4> raceWeatherImages{
        menu::loadOriginalImage(*resources, "Data/GUI/fair.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/night.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/cloudy.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/rainy.png")};
    const Texture raceTopPanel =
        createImageTexture(*device, raceTopPanelImage);
    const Texture raceBottomPanel =
        createImageTexture(*device, raceBottomPanelImage);
    const Texture raceMenuButton =
        createImageTexture(*device, raceMenuButtonImage);
    const Texture raceMenuButtonSelected =
        createImageTexture(*device, raceMenuButtonSelectedImage);
    const Texture raceMoney =
        createImageTexture(*device, raceMoneyImage);
    const Texture raceStats =
        createImageTexture(*device, raceStatsImage);
    const Texture raceImageFrame =
        createImageTexture(*device, raceImageFrameImage);
    const Texture raceChargeBar =
        createImageTexture(*device, raceChargeBarImage);
    std::array<Texture, 7> raceMenuIcons{};
    for (std::size_t index = 0U; index < raceMenuIcons.size(); ++index)
        raceMenuIcons[index] =
            createImageTexture(*device, raceMenuIconImages[index]);
    std::array<Texture, 4> raceWeatherIcons{};
    for (std::size_t index = 0U; index < raceWeatherIcons.size(); ++index)
        raceWeatherIcons[index] =
            createImageTexture(*device, raceWeatherImages[index]);
    const auto garageTopPanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/topPanel2.png");
    const auto garageBottomPanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/bottomPanel2.png");
    const auto garageSidePanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/rightPanel2.png");
    const auto garageMoneyImage = menu::loadOriginalImage(
        *resources, "Data/GUI/moneyBg.png");
    const auto garageStatsImage = menu::loadOriginalImage(
        *resources, "Data/GUI/statFrame2.png");
    const auto garageStatBarImage = menu::loadOriginalImage(
        *resources, "Data/GUI/statBar2.png");
    const auto garageCarBoxImage = menu::loadOriginalImage(
        *resources, "Data/GUI/carBox.png");
    const auto garageCarBoxSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/carBoxSel.png");
    const auto garageLockImage = menu::loadOriginalImage(
        *resources, "Data/GUI/lock.png");
    const auto garageColorBoxImage = menu::loadOriginalImage(
        *resources, "Data/GUI/colorBox.png");
    const auto garageColorBoxBackgroundImage = menu::loadOriginalImage(
        *resources, "Data/GUI/colorBoxBg.png");
    const auto garageColorBoxSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/colorBoxBgSel.png");
    const auto garageArrowImage = menu::loadOriginalImage(
        *resources, "Data/GUI/arrow1.png");
    const auto garageArrowSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/arrowSel1.png");
    const auto garageBackImage = menu::loadOriginalImage(
        *resources, "Data/GUI/buttonBg2.png");
    const auto garageBackSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/buttonBgSel2.png");
    const std::string garageLanguage =
        activeLanguage == "russian" ? "russian" : "english";
    const auto garageBuyImage = menu::loadOriginalImage(
        *resources,
        "Data/GUI/buyButton_" + garageLanguage + ".png");
    const auto garageBuySelectedImage = menu::loadOriginalImage(
        *resources,
        "Data/GUI/buyButtonSel_" + garageLanguage + ".png");
    const Texture garageTopPanel =
        createImageTexture(*device, garageTopPanelImage);
    const Texture garageBottomPanel =
        createImageTexture(*device, garageBottomPanelImage);
    const Texture garageSidePanel =
        createImageTexture(*device, garageSidePanelImage);
    const Texture garageMoney =
        createImageTexture(*device, garageMoneyImage);
    const Texture garageStats =
        createImageTexture(*device, garageStatsImage);
    const Texture garageStatBar =
        createImageTexture(*device, garageStatBarImage);
    const Texture garageCarBox =
        createImageTexture(*device, garageCarBoxImage);
    const Texture garageCarBoxSelected =
        createImageTexture(*device, garageCarBoxSelectedImage);
    const Texture garageLock =
        createImageTexture(*device, garageLockImage);
    const Texture garageColorBox =
        createImageTexture(*device, garageColorBoxImage);
    const Texture garageColorBoxBackground =
        createImageTexture(*device, garageColorBoxBackgroundImage);
    const Texture garageColorBoxSelected =
        createImageTexture(*device, garageColorBoxSelectedImage);
    const Texture garageArrow =
        createImageTexture(*device, garageArrowImage);
    const Texture garageArrowSelected =
        createImageTexture(*device, garageArrowSelectedImage);
    const Texture garageBack =
        createImageTexture(*device, garageBackImage);
    const Texture garageBackSelected =
        createImageTexture(*device, garageBackSelectedImage);
    const Texture garageBuy =
        createImageTexture(*device, garageBuyImage);
    const Texture garageBuySelected =
        createImageTexture(*device, garageBuySelectedImage);
    const auto workshopTopPanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/topPanel3.png");
    const auto workshopBottomPanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/bottomPanel3.png");
    const auto workshopLeftPanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/leftPanel3.png");
    const auto workshopSlotImage = menu::loadOriginalImage(
        *resources, "Data/GUI/slot2.png");
    const auto workshopSlotFrameImage = menu::loadOriginalImage(
        *resources, "Data/GUI/slot2Frame.png");
    const auto workshopChargeBoxImage = menu::loadOriginalImage(
        *resources, "Data/GUI/chargeBox.png");
    const auto workshopChargeBarImage = menu::loadOriginalImage(
        *resources, "Data/GUI/chargeBar.png");
    const auto workshopChargeButtonImage = menu::loadOriginalImage(
        *resources, "Data/GUI/chargeButton.png");
    const auto workshopChargeButtonSelectedImage =
        menu::loadOriginalImage(
            *resources, "Data/GUI/chargeButtonSel.png");
    const auto workshopStatBarPlusImage = menu::loadOriginalImage(
        *resources, "Data/GUI/statBar2Plus.png");
    const std::array<menu::Image, 3> workshopUpgradeImages{
        menu::loadOriginalImage(*resources, "Data/GUI/upLevel1.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/upLevel2.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/upLevel3.png")};
    const std::array<menu::Image, 4> workshopSlotIconImages{
        menu::loadOriginalImage(*resources, "Data/GUI/hyperSlot.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/mineSlot.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/wpnSlot.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/wpnSlotSel.png")};
    const auto workshopInfoFrameImage = menu::loadOriginalImage(
        *resources, "Data/GUI/dlgFrame3.png");
    const Texture workshopTopPanel =
        createImageTexture(*device, workshopTopPanelImage);
    const Texture workshopBottomPanel =
        createImageTexture(*device, workshopBottomPanelImage);
    const Texture workshopLeftPanel =
        createImageTexture(*device, workshopLeftPanelImage);
    const Texture workshopSlot =
        createImageTexture(*device, workshopSlotImage);
    const Texture workshopSlotFrame =
        createImageTexture(*device, workshopSlotFrameImage);
    const Texture workshopChargeBox =
        createImageTexture(*device, workshopChargeBoxImage);
    const Texture workshopChargeBar =
        createImageTexture(*device, workshopChargeBarImage);
    const Texture workshopChargeButton =
        createImageTexture(*device, workshopChargeButtonImage);
    const Texture workshopChargeButtonSelected =
        createImageTexture(*device, workshopChargeButtonSelectedImage);
    const Texture workshopStatBarPlus =
        createImageTexture(*device, workshopStatBarPlusImage);
    std::array<Texture, 3> workshopUpgradeTextures{};
    for (std::size_t index = 0U;
         index < workshopUpgradeTextures.size(); ++index)
    {
        workshopUpgradeTextures[index] =
            createImageTexture(*device, workshopUpgradeImages[index]);
    }
    std::array<Texture, 4> workshopSlotIconTextures{};
    for (std::size_t index = 0U;
         index < workshopSlotIconTextures.size(); ++index)
    {
        workshopSlotIconTextures[index] =
            createImageTexture(*device, workshopSlotIconImages[index]);
    }
    const Texture workshopInfoFrame =
        createImageTexture(*device, workshopInfoFrameImage);
    const auto angarBottomPanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/bottomPanel6.png");
    const auto angarPlanetInfoImage = menu::loadOriginalImage(
        *resources, "Data/GUI/planetInfo.png");
    const auto angarCloseImage = menu::loadOriginalImage(
        *resources, "Data/GUI/buttonBg6.png");
    const auto angarCloseSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/buttonBgSel6.png");
    const auto angarDoorSlotImage = menu::loadOriginalImage(
        *resources, "Data/GUI/doorSlot.png");
    const auto angarDoorSlotSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/doorSlotSel.png");
    const auto angarDoorDownImage = menu::loadOriginalImage(
        *resources, "Data/GUI/doorDown.png");
    const auto angarDoorUpImage = menu::loadOriginalImage(
        *resources, "Data/GUI/doorUp.png");
    const Texture angarBottomPanel =
        createImageTexture(*device, angarBottomPanelImage);
    const Texture angarPlanetInfo =
        createImageTexture(*device, angarPlanetInfoImage);
    const Texture angarClose =
        createImageTexture(*device, angarCloseImage);
    const Texture angarCloseSelected =
        createImageTexture(*device, angarCloseSelectedImage);
    const Texture angarDoorSlot =
        createImageTexture(*device, angarDoorSlotImage);
    const Texture angarDoorSlotSelected =
        createImageTexture(*device, angarDoorSlotSelectedImage);
    const Texture angarDoorDown =
        createImageTexture(*device, angarDoorDownImage);
    const Texture angarDoorUp =
        createImageTexture(*device, angarDoorUpImage);
    std::vector<menu::Image> angarBossImages;
    std::vector<Texture> angarBossTextures;
    angarBossImages.reserve(originalGarage->planets.size());
    angarBossTextures.reserve(originalGarage->planets.size());
    for (const auto& planet : originalGarage->planets)
    {
        angarBossImages.push_back(
            menu::loadOriginalImage(
                *resources, planet.bossPhotoPath));
        angarBossTextures.push_back(
            createImageTexture(*device, angarBossImages.back()));
    }
    const auto achievementBackgroundImage = menu::loadOriginalImage(
        *resources, "Data/GUI/achievmentBg.dds");
    const auto achievementBottomPanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/achievmentBottomPanel.png");
    const auto achievementPanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/achievmentPanel.png");
    const auto achievementCloseImage = menu::loadOriginalImage(
        *resources, "Data/GUI/closeBut.png");
    const auto achievementOkImage = menu::loadOriginalImage(
        *resources, "Data/GUI/okBut.png");
    const auto achievementOkSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/okButSel.png");
    const Texture achievementBackground =
        createImageTexture(*device, achievementBackgroundImage);
    const Texture achievementBottomPanel =
        createImageTexture(*device, achievementBottomPanelImage);
    const Texture achievementPanel =
        createImageTexture(*device, achievementPanelImage);
    const Texture achievementClose =
        createImageTexture(*device, achievementCloseImage);
    const Texture achievementOk =
        createImageTexture(*device, achievementOkImage);
    const Texture achievementOkSelected =
        createImageTexture(*device, achievementOkSelectedImage);
    std::vector<menu::Image> achievementLockedImages;
    std::vector<menu::Image> achievementOpenedImages;
    std::vector<Texture> achievementLockedTextures;
    std::vector<Texture> achievementOpenedTextures;
    achievementLockedImages.reserve(originalAchievementVisuals.size());
    achievementOpenedImages.reserve(originalAchievementVisuals.size());
    achievementLockedTextures.reserve(originalAchievementVisuals.size());
    achievementOpenedTextures.reserve(originalAchievementVisuals.size());
    for (const auto& visual : originalAchievementVisuals)
    {
        achievementLockedImages.push_back(
            menu::loadOriginalImage(
                *resources, std::string(visual.lockedImage)));
        achievementOpenedImages.push_back(
            menu::loadOriginalImage(
                *resources, std::string(visual.openedImage)));
        achievementLockedTextures.push_back(
            createImageTexture(*device, achievementLockedImages.back()));
        achievementOpenedTextures.push_back(
            createImageTexture(*device, achievementOpenedImages.back()));
    }
    std::vector<menu::Image> garageCarImages;
    std::vector<Texture> garageCarTextures;
    garageCarImages.reserve(originalGarage->cars.size());
    garageCarTextures.reserve(originalGarage->cars.size());
    for (const auto& car : originalGarage->cars)
    {
        garageCarImages.push_back(menu::loadOriginalImage(
            *resources,
            "Data/GUI/Cars/" +
                std::string(recordName(car.record)) + ".png"));
        garageCarTextures.push_back(
            createImageTexture(*device, garageCarImages.back()));
    }
    constexpr std::array<std::array<std::uint8_t, 4>, 14>
        garageColorPixels{{
            {255U, 255U, 255U, 255U},
            {0U, 0U, 255U, 255U},
            {255U, 0U, 0U, 255U},
            {0U, 255U, 0U, 255U},
            {255U, 255U, 0U, 255U},
            {255U, 144U, 0U, 255U},
            {51U, 51U, 51U, 255U},
            {6U, 175U, 250U, 255U},
            {183U, 11U, 174U, 255U},
            {177U, 201U, 3U, 255U},
            {169U, 57U, 0U, 255U},
            {48U, 55U, 61U, 255U},
            {0U, 159U, 21U, 255U},
            {112U, 118U, 156U, 255U},
        }};
    std::array<Texture, garageColorPixels.size()>
        garageColorTextures{};
    for (std::size_t index = 0U;
         index < garageColorTextures.size(); ++index)
    {
        garageColorTextures[index] = device->createTextureRgba8(
            1U, 1U, garageColorPixels[index].data(),
            garageColorPixels[index].size());
    }
#endif

    struct MenuPageVisual
    {
        std::vector<std::string> labels;
        std::vector<TextVisual> normal;
        std::vector<TextVisual> selected;
    };
    auto localized = [&](std::string_view key) {
        const auto found =
            model->localizedStrings.find(std::string(key));
        return found == model->localizedStrings.end()
                   ? std::string(key)
                   : found->second;
    };
    auto labels = [&](std::initializer_list<const char*> keys) {
        std::vector<std::string> output;
        output.reserve(keys.size());
        for (const char* key : keys)
            output.push_back(localized(key));
        return output;
    };
    MenuPageVisual mainPage;
    MenuPageVisual gameModePage;
    MenuPageVisual tournamentPage;
    MenuPageVisual difficultyPage;
    MenuPageVisual profilePage;
    MenuPageVisual networkPage;
    MenuPageVisual optionsPage;
    MenuPageVisual creditsPage;
#ifdef RRR3D_PHYSICS
    MenuPageVisual raceMenuPage;
    MenuPageVisual raceMainHeadersPage;
    MenuPageVisual raceMainInfoPage;
    MenuPageVisual garagePage;
    MenuPageVisual garageInfoPage;
    MenuPageVisual garageStatsPage;
    MenuPageVisual garagePurchasePage;
    MenuPageVisual workshopPage;
    MenuPageVisual workshopControlsPage;
    MenuPageVisual workshopStatsPage;
    MenuPageVisual workshopHintPage;
    MenuPageVisual workshopInfoPage;
    MenuPageVisual workshopConfirmationPage;
    MenuPageVisual planetsPage;
    MenuPageVisual angarInfoPage;
    MenuPageVisual achievementsPage;
    MenuPageVisual achievementPricePage;
    MenuPageVisual gameOptionsPage;
    MenuPageVisual graphicsOptionsPage;
    MenuPageVisual soundOptionsPage;
    MenuPageVisual controlsOptionsPage;
    MenuPageVisual gameOptionNamesPage;
    MenuPageVisual graphicsOptionNamesPage;
    MenuPageVisual soundOptionNamesPage;
    MenuPageVisual controlsKeyboardValuesPage;
    MenuPageVisual controlsGamepadValuesPage;
    MenuPageVisual optionsHeaderPage;
    MenuPageVisual optionsStatePage;
    MenuPageVisual optionsActionPage;
    MenuPageVisual finishPage;
#endif
    std::string resolvedFont;
    auto createPage = [&](std::vector<std::string> pageLabels) {
        MenuPageVisual page;
        page.labels = std::move(pageLabels);
        for (const auto& item : page.labels)
        {
            page.normal.push_back(createText(
                *device, item, menu::headerFontHeight, false,
                menu::normalTextColor, resolvedFont));
            page.selected.push_back(createText(
                *device, item, menu::headerFontHeight, false,
                menu::selectedTextColor, resolvedFont));
        }
        return page;
    };
    auto createStyledPage =
        [&](std::vector<std::string> pageLabels, float pointSize,
            menu::Rgba8 normalColor, menu::Rgba8 selectedColor) {
            MenuPageVisual page;
            page.labels = std::move(pageLabels);
            for (const auto& item : page.labels)
            {
                page.normal.push_back(createText(
                    *device, item, pointSize, false, normalColor,
                    resolvedFont));
                page.selected.push_back(createText(
                    *device, item, pointSize, false, selectedColor,
                    resolvedFont));
            }
            return page;
        };
    auto destroyPage = [&](const MenuPageVisual& page) {
        for (const auto& item : page.selected)
            device->destroy(item.texture);
        for (const auto& item : page.normal)
            device->destroy(item.texture);
    };
#ifdef RRR3D_PHYSICS
    constexpr menu::Rgba8 optionsTextColor{175, 175, 175, 255};
    constexpr menu::Rgba8 optionsStateSelectedColor{235, 115, 62, 255};
    constexpr menu::Rgba8 raceTextColor{214, 214, 214, 255};
    constexpr menu::Rgba8 raceInfoColor{214, 184, 164, 255};
    auto optionsDraftConfig = profileState.config;
    auto optionsDraftDifficulty = profileState.player.difficulty;
    std::vector<std::pair<std::uint32_t, std::uint32_t>>
        originalDisplayModes;
    int displayModeCount = 0;
    SDL_DisplayMode** displayModes = SDL_GetFullscreenDisplayModes(
        SDL_GetDisplayForWindow(window), &displayModeCount);
    for (int index = 0; displayModes != nullptr &&
                        index < displayModeCount;
         ++index)
    {
        if (displayModes[index] == nullptr ||
            displayModes[index]->w <= 0 ||
            displayModes[index]->h <= 0)
        {
            continue;
        }
        const auto mode = std::pair{
            static_cast<std::uint32_t>(displayModes[index]->w),
            static_cast<std::uint32_t>(displayModes[index]->h)};
        if (std::find(
                originalDisplayModes.begin(), originalDisplayModes.end(),
                mode) == originalDisplayModes.end())
        {
            originalDisplayModes.push_back(mode);
        }
    }
    SDL_free(displayModes);
    const auto configuredDisplayMode = std::pair{
        optionsDraftConfig.resolutionWidth,
        optionsDraftConfig.resolutionHeight};
    if (std::find(
            originalDisplayModes.begin(), originalDisplayModes.end(),
            configuredDisplayMode) == originalDisplayModes.end())
    {
        originalDisplayModes.push_back(configuredDisplayMode);
    }
    if (originalDisplayModes.empty())
        originalDisplayModes.push_back({initialWidth, initialHeight});
    std::sort(
        originalDisplayModes.begin(), originalDisplayModes.end(),
        [](const auto& first, const auto& second) {
            return first.first * first.second <
                   second.first * second.second;
        });
    auto onOff = [&](bool value) {
        return localized(value ? "svOn" : "svOff");
    };
    auto qualityName = [&](std::uint32_t value) {
        return localized(
            std::array<std::string_view, 3>{
                "svLow", "svMiddle", "svHigh"}
                [std::min<std::uint32_t>(value, 2U)]);
    };
    auto volumeName = [](float value) {
        return std::to_string(
                   static_cast<int>(std::lround(
                       std::clamp(value, 0.0F, 2.0F) * 50.0F))) +
               "%";
    };
    auto distanceName = [](float value) {
        std::ostringstream stream;
        stream.setf(std::ios::fixed);
        stream.precision(2);
        stream << value << " x";
        return stream.str();
    };
    static constexpr std::array<std::string_view, 18>
        originalControlActions{
            "gaAccel", "gaBreak", "gaWheelLeft", "gaWheelRight",
            "gaShot", "gaShot1", "gaShot2", "gaShot3", "gaShot4",
            "gaShotAll", "gaHyper", "gaMine", "gaWeaponDown",
            "gaWeaponUp", "gaViewSwitch", "gaAction", "gaEscape",
            "gaResetCar"};
    static constexpr std::size_t controlsVisibleRows = 6U;
    bool controlsUseGamepad = false;
    auto gameOptionsLabels = [&]() {
        return std::vector<std::string>{
            localized(
                    optionsDraftConfig.preferredCamera ==
                            r3d::game::originalrace::
                                PreferredCamera::ThirdPerson
                        ? "svCameraSecView"
                        : "svCameraOrtho"),
            distanceName(optionsDraftConfig.cameraDistance),
            onOff(optionsDraftConfig.enableHud),
            localized(optionsDraftDifficulty),
            onOff(optionsDraftConfig.springBorders),
            std::to_string(
                std::min(optionsDraftConfig.upgradeMaxLevel, 2U) + 1U),
            std::to_string(std::clamp(
                optionsDraftConfig.weaponMaxLevel, 1U, 4U)),
            std::to_string(std::clamp(
                optionsDraftConfig.maxPlayers, 2U, 6U)),
            std::to_string(optionsDraftConfig.maxComputers),
            std::to_string(optionsDraftConfig.lapsCount),
            onOff(optionsDraftConfig.enableMineBug),
            onOff(optionsDraftConfig.disableVideo),
            localized("svBack"), localized("svApply")};
    };
    auto graphicsOptionsLabels = [&]() {
        // Preserve OptionsMenu.cpp's shipped ordering verbatim: its
        // Filtering row uses aaLevel and Multisampling uses msLevel.
        static constexpr std::array<std::string_view, 4>
            filteringNames{
                "none", "aa 2x", "aa 4x", "aa 8x"};
        static constexpr std::array<std::string_view, 3> msaaNames{
            "linear", "af 2x", "af 4x"};
        const std::string resolution =
            std::to_string(optionsDraftConfig.resolutionWidth) + " x " +
            std::to_string(optionsDraftConfig.resolutionHeight);
        return std::vector<std::string>{
            resolution,
            std::string(filteringNames[
                    std::min<std::uint32_t>(
                        optionsDraftConfig.quality.filtering,
                        filteringNames.size() - 1U)]),
            std::string(msaaNames[
                    std::min<std::uint32_t>(
                        optionsDraftConfig.quality.msaa,
                        msaaNames.size() - 1U)]),
            qualityName(optionsDraftConfig.quality.shadow),
            qualityName(optionsDraftConfig.quality.environment),
            qualityName(optionsDraftConfig.quality.light),
            qualityName(optionsDraftConfig.quality.postEffect),
            onOff(!optionsDraftConfig.fullScreen),
            localized("svBack"), localized("svApply")};
    };
    auto soundOptionsLabels = [&]() {
        return std::vector<std::string>{
            localized(optionsDraftConfig.language == "russian"
                              ? "svRussian"
                              : "svEnglish"),
            localized(
                    optionsDraftConfig.commentatorStyle == "russian"
                        ? "svRussian"
                        : "svEnglish"),
            volumeName(optionsDraftConfig.musicVolume),
            volumeName(optionsDraftConfig.effectsVolume),
            volumeName(optionsDraftConfig.voiceVolume),
            localized("svBack"), localized("svApply")};
    };
    auto controlsOptionsLabels = [&]() {
        std::vector<std::string> output;
        output.reserve(originalControlActions.size() + 2U);
        for (const auto action : originalControlActions)
            output.push_back(localized(action));
        output.push_back(localized("svBack"));
        output.push_back(localized("svApply"));
        return output;
    };
    auto controlValues = [&](bool gamepad) {
        std::vector<std::string> output;
        output.reserve(originalControlActions.size());
        const auto& bindings =
            gamepad ? optionsDraftConfig.gamepadControls
                    : optionsDraftConfig.keyboardControls;
        for (const auto action : originalControlActions)
        {
            const auto found = bindings.find(std::string(action));
            output.push_back(
                found == bindings.end() ? localized("svNull")
                                        : found->second);
        }
        return output;
    };
    auto formatOriginalRaceInfo =
        [](std::string pattern, const std::string& textValue,
           std::uint32_t firstValue, std::uint32_t secondValue) {
            auto replaceFirst = [&](std::string_view token,
                                    const std::string& value) {
                const auto position = pattern.find(token);
                if (position != std::string::npos)
                    pattern.replace(position, token.size(), value);
            };
            replaceFirst("%s", textValue);
            replaceFirst("%d", std::to_string(firstValue));
            replaceFirst("%d", std::to_string(secondValue));
            return pattern;
        };
    auto raceMainInfoLabels = [&]() {
        const auto trackIndex = std::min(
            selectedTrack, originalRace->trackCatalog.size() - 1U);
        const auto& selected =
            originalRace->trackCatalog[trackIndex];
        std::uint32_t trackNumber = 0U;
        std::uint32_t trackCount = 0U;
        for (std::size_t index = 0U;
             index < originalRace->trackCatalog.size(); ++index)
        {
            const auto& candidate =
                originalRace->trackCatalog[index];
            if (candidate.planetIndex != selected.planetIndex ||
                candidate.racePass != selected.racePass)
            {
                continue;
            }
            ++trackCount;
            if (index <= trackIndex)
                ++trackNumber;
        }
        const std::string planetName =
            selected.planetIndex < originalGarage->planets.size()
                ? localized(
                      originalGarage
                          ->planets[selected.planetIndex]
                          .name)
                : selected.worldType;
        const auto pass = std::max(selected.racePass, 1U);
        const auto required =
            pass <= originalRace->requiredPoints.size()
                ? originalRace->requiredPoints[pass - 1U]
                : 0U;
        const std::string passInfo = formatOriginalRaceInfo(
            localized("svPassInfo"), planetName,
            std::max(trackNumber, 1U),
            std::max(trackCount, 1U));
        const std::string tournamentInfo =
            formatOriginalRaceInfo(
                localized("svTournamentInfo"),
                pass <= 1U ? "B" : "A", required,
                profileState.player.points);
        const std::string bossName =
            originalRace->racers.size() > 1U
                ? localized(originalRace->racers[1].name)
                : localized("svNull");
        return std::vector<std::string>{
            profileState.player.name, bossName, passInfo,
            tournamentInfo,
            "$" + std::to_string(profileState.player.money)};
    };
#endif
    try
    {
        mainPage = createPage(model->items);
        gameModePage = createPage(labels(
            {"svChampionship", "svSkirmish", "svBack"}));
        tournamentPage = createPage(labels(
            {"svContinue", "svNewGame", "svLoad", "svBack"}));
        difficultyPage = createPage(
            labels({"gdEasy", "gdNormal", "gdHard", "svBack"}));
        std::vector<std::string> profileLabels;
#ifdef RRR3D_PHYSICS
        profileLabels = profileState.profiles;
#endif
        if (profileLabels.empty())
            profileLabels.push_back("profile1");
        profileLabels.push_back(localized("svBack"));
        profilePage = createPage(std::move(profileLabels));
        networkPage = createPage(
            labels({"svNetCreate", "svConnect", "svBack"}));
        optionsPage = createPage(labels(
            {"svGame", "svGraphic", "svSound", "svControls",
             "svBack"}));
        creditsPage = createPage(labels({"svBack"}));
#ifdef RRR3D_PHYSICS
        raceMenuPage = createPage(
            {"Start race", localized("svWorkshop"),
             localized("svGarage"), "Planets",
             localized("svRewards"), localized("svOptions"),
             localized("svExit")});
        raceMainHeadersPage = createStyledPage(
            labels(
                {"svPlayer", "svPassing", "svTournament",
                 "svWeapons", "svBossName"}),
            menu::smallFontHeight, raceTextColor,
            menu::selectedTextColor);
        raceMainInfoPage = createStyledPage(
            raceMainInfoLabels(), menu::smallFontHeight,
            raceInfoColor, menu::selectedTextColor);
        garagePage = createPage(
            {localized("svGarage"), localized("svMoney"), "-",
             localized("svBack"), localized("svBuy")});
        garageInfoPage = createStyledPage(
            {" "}, menu::smallFontHeight, raceTextColor,
            menu::selectedTextColor);
        garageStatsPage = createStyledPage(
            {"0/0", "0/0", "0/300"},
            menu::smallFontHeight, raceTextColor,
            menu::selectedTextColor);
        garagePurchasePage = createStyledPage(
            {localized("svBuyCar")}, menu::smallFontHeight,
            menu::normalTextColor, menu::selectedTextColor);
        workshopPage = createStyledPage(
            {localized("svWorkshop")},
            menu::headerFontHeight, raceTextColor,
            menu::selectedTextColor);
        workshopControlsPage = createStyledPage(
            {localized("svBack"),
             std::to_string(profileState.player.money)},
            menu::smallFontHeight, raceTextColor,
            menu::selectedTextColor);
        workshopStatsPage = createStyledPage(
            {"0/0", "0/0", "0/300"},
            menu::smallFontHeight, raceTextColor,
            menu::selectedTextColor);
        workshopHintPage = createStyledPage(
            {" "}, menu::smallFontHeight, raceTextColor,
            menu::selectedTextColor);
        workshopInfoPage = createStyledPage(
            {" ", " ", " "}, menu::smallFontHeight, raceTextColor,
            menu::selectedTextColor);
        workshopConfirmationPage = createStyledPage(
            {" "}, menu::smallFontHeight,
            menu::normalTextColor, menu::selectedTextColor);
        planetsPage = createPage(
            {"Planets", localized("svBack")});
        angarInfoPage = createStyledPage(
            {" ", " ", " "}, menu::smallFontHeight,
            menu::Rgba8{118, 206, 242, 255},
            menu::selectedTextColor);
        achievementsPage = createPage(
            {"viper", "buggi", "airblade", "reflector", "droid",
             "tankchetti", "phaser", "mustang", "armor4",
             localized("svBack")});
        achievementPricePage = createStyledPage(
            {"0", "0", "0", "0", "0", "0", "0", "0", "0"},
            menu::smallFontHeight,
            menu::Rgba8{195, 194, 192, 255},
            menu::Rgba8{195, 194, 192, 255});
        gameOptionsPage = createStyledPage(
            gameOptionsLabels(), menu::smallFontHeight,
            optionsTextColor, menu::selectedTextColor);
        graphicsOptionsPage = createStyledPage(
            graphicsOptionsLabels(), menu::smallFontHeight,
            optionsTextColor, menu::selectedTextColor);
        soundOptionsPage = createStyledPage(
            soundOptionsLabels(), menu::smallFontHeight,
            optionsTextColor, menu::selectedTextColor);
        controlsOptionsPage = createStyledPage(
            controlsOptionsLabels(), menu::smallFontHeight,
            optionsTextColor, menu::selectedTextColor);
        gameOptionNamesPage = createStyledPage(
            labels(
                {"svCamera", "svCameraDist", "svEnableHUD",
                 "svDifficulty", "svSpringBorders",
                 "svUpgradeMaxLevel", "svWeaponMaxLevel",
                 "svMaxPlayers", "svMaxComputers", "svLapsCount",
                 "svEnableMineBug", "svDisableVideo"}),
            menu::smallFontHeight, optionsTextColor,
            menu::selectedTextColor);
        graphicsOptionNamesPage = createStyledPage(
            labels(
                {"svResolution", "svFiltering", "svMultisampling",
                 "svShadow", "svEnv", "svLight", "svPostProcess",
                 "svWindowMode"}),
            menu::smallFontHeight, optionsTextColor,
            menu::selectedTextColor);
        soundOptionNamesPage = createStyledPage(
            labels(
                {"svLanguage", "svCommentator", "svMusic",
                 "svSound", "svSoundDicter"}),
            menu::smallFontHeight, optionsTextColor,
            menu::selectedTextColor);
        controlsKeyboardValuesPage = createStyledPage(
            controlValues(false), menu::smallFontHeight,
            optionsTextColor, menu::selectedTextColor);
        controlsGamepadValuesPage = createStyledPage(
            controlValues(true), menu::smallFontHeight,
            optionsTextColor, menu::selectedTextColor);
        optionsHeaderPage = createStyledPage(
            labels({"svOptions"}), menu::headerFontHeight,
            optionsTextColor, menu::selectedTextColor);
        optionsStatePage = createStyledPage(
            labels({"svGame", "svGraphic", "svNetwork", "svControls"}),
            menu::headerFontHeight, optionsTextColor,
            optionsStateSelectedColor);
        optionsActionPage = createStyledPage(
            labels({"svBack", "svApply"}), menu::headerFontHeight,
            optionsTextColor, menu::selectedTextColor);
        finishPage = createPage(
            labels({"svContinue", "svBack"}));
#endif
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Original MainMenu2 font creation failed: "
                  << exception.what() << '\n';
    }
    const TextVisual version = createText(
        *device, model->versionText, menu::smallFontHeight, true,
        menu::selectedTextColor, resolvedFont);
    const TextVisual credits = createText(
        *device, localized("svCredits"), menu::smallFontHeight, false,
        menu::normalTextColor, resolvedFont);
#ifdef RRR3D_PHYSICS
    TextVisual finishSummary = createText(
        *device, localized("svFinish"), menu::smallFontHeight, false,
        menu::normalTextColor, resolvedFont);
    const TextVisual exitRaceMessage = createText(
        *device, localized("svHintExitRace"), menu::smallFontHeight,
        false, menu::normalTextColor, resolvedFont);
    const TextVisual exitRaceYes = createText(
        *device, localized("svYes"), menu::smallFontHeight,
        false, menu::normalTextColor, resolvedFont);
    const TextVisual exitRaceYesSelected = createText(
        *device, localized("svYes"), menu::smallFontHeight,
        false, menu::selectedTextColor, resolvedFont);
    const TextVisual exitRaceNo = createText(
        *device, localized("svNo"), menu::smallFontHeight,
        false, menu::normalTextColor, resolvedFont);
    const TextVisual exitRaceNoSelected = createText(
        *device, localized("svNo"), menu::smallFontHeight,
        false, menu::selectedTextColor, resolvedFont);
    TextVisual angarTravelMessage = createText(
        *device, localized("svYouReadyStayPlanet"),
        menu::smallFontHeight, false, menu::normalTextColor,
        resolvedFont);
    const TextVisual angarWarningMessage = createText(
        *device, localized("svHintCantFlyPlanet"),
        menu::smallFontHeight, false, menu::normalTextColor,
        resolvedFont);
    const TextVisual angarOk = createText(
        *device, localized("svOk"), menu::smallFontHeight,
        false, menu::selectedTextColor, resolvedFont);
    const TextVisual achievementRewards = createText(
        *device, localized("svRewards"), menu::headerFontHeight,
        false, menu::normalTextColor, resolvedFont);
    TextVisual achievementPoints = createText(
        *device,
        localized("svPoints") + " " +
            std::to_string(profileState.achievementPoints),
        menu::headerFontHeight, false,
        menu::Rgba8{250, 88, 0, 255}, resolvedFont);
    const TextVisual achievementPurchaseMessage = createText(
        *device, localized("svBuyReward"), menu::smallFontHeight,
        false, menu::normalTextColor, resolvedFont);
    const TextVisual achievementNotEnoughPoints = createText(
        *device, localized("svHintCantPoints"), menu::smallFontHeight,
        false, menu::normalTextColor, resolvedFont);
#endif

    auto pageValid = [](const MenuPageVisual& page) {
        return !page.labels.empty() &&
               page.normal.size() == page.labels.size() &&
               page.selected.size() == page.labels.size() &&
               std::all_of(
                   page.normal.begin(), page.normal.end(),
                   [](const TextVisual& item) {
                       return valid(item.texture);
                   }) &&
               std::all_of(
                   page.selected.begin(), page.selected.end(),
                   [](const TextVisual& item) {
                       return valid(item.texture);
                   });
    };

    const bool gpuResourcesValid =
        valid(shader) &&
#ifdef RRR3D_PHYSICS
        valid(raceShader) &&
#endif
        valid(quad) && valid(background) && valid(topPanel) &&
        valid(bottomPanel) && valid(selection) && valid(cursor) &&
        valid(version.texture) && valid(credits.texture) &&
        pageValid(mainPage) && pageValid(gameModePage) &&
        pageValid(tournamentPage) && pageValid(difficultyPage) &&
        pageValid(profilePage) && pageValid(networkPage) &&
        pageValid(optionsPage) && pageValid(creditsPage);
#ifdef RRR3D_PHYSICS
    const bool optionsResourcesValid =
        pageValid(raceMenuPage) &&
        pageValid(raceMainHeadersPage) &&
        pageValid(raceMainInfoPage) &&
        pageValid(garagePage) &&
        pageValid(garageInfoPage) &&
        pageValid(garageStatsPage) &&
        pageValid(garagePurchasePage) &&
        pageValid(workshopPage) &&
        pageValid(workshopInfoPage) &&
        pageValid(planetsPage) &&
        pageValid(angarInfoPage) &&
        pageValid(achievementsPage) &&
        pageValid(achievementPricePage) &&
        pageValid(gameOptionsPage) &&
        pageValid(graphicsOptionsPage) &&
        pageValid(soundOptionsPage) &&
        pageValid(controlsOptionsPage) &&
        pageValid(gameOptionNamesPage) &&
        pageValid(graphicsOptionNamesPage) &&
        pageValid(soundOptionNamesPage) &&
        pageValid(controlsKeyboardValuesPage) &&
        pageValid(controlsGamepadValuesPage) &&
        pageValid(optionsHeaderPage) &&
        pageValid(optionsStatePage) &&
        pageValid(optionsActionPage) &&
        pageValid(finishPage) &&
        valid(finishSummary.texture) && valid(acceptFrame) &&
        valid(acceptButton) && valid(acceptButtonSelected) &&
        valid(optionsBackground) && valid(optionsRow) &&
        valid(controlsRow) && valid(optionsArrow) &&
        valid(optionsArrowSelected) &&
        valid(optionsBarBackground) && valid(optionsBar) &&
        valid(optionsButton) && valid(optionsButtonSelected) &&
        valid(optionsKey) && valid(optionsKeySelected) &&
        valid(keyboardIcon) && valid(gamepadIcon) &&
        valid(optionsMask) &&
        valid(raceTopPanel) && valid(raceBottomPanel) &&
        valid(raceMenuButton) &&
        valid(raceMenuButtonSelected) &&
        valid(raceMoney) && valid(raceStats) &&
        valid(raceImageFrame) && valid(raceChargeBar) &&
        std::all_of(
            raceMenuIcons.begin(), raceMenuIcons.end(),
            [](Texture texture) { return valid(texture); }) &&
        std::all_of(
            raceWeatherIcons.begin(), raceWeatherIcons.end(),
            [](Texture texture) { return valid(texture); }) &&
        valid(garageTopPanel) && valid(garageBottomPanel) &&
        valid(garageSidePanel) && valid(garageMoney) &&
        valid(garageStats) && valid(garageStatBar) &&
        valid(garageCarBox) && valid(garageCarBoxSelected) &&
        valid(garageLock) && valid(garageColorBox) &&
        valid(garageColorBoxBackground) &&
        valid(garageColorBoxSelected) && valid(garageArrow) &&
        valid(garageArrowSelected) && valid(garageBack) &&
        valid(garageBackSelected) && valid(garageBuy) &&
        valid(garageBuySelected) && valid(workshopTopPanel) &&
        valid(workshopBottomPanel) && valid(workshopLeftPanel) &&
        valid(workshopSlot) && valid(workshopSlotFrame) &&
        valid(workshopChargeBox) && valid(workshopChargeBar) &&
        valid(workshopChargeButton) &&
        valid(workshopChargeButtonSelected) &&
        valid(workshopStatBarPlus) && valid(workshopInfoFrame) &&
        valid(angarBottomPanel) && valid(angarPlanetInfo) &&
        valid(angarClose) && valid(angarCloseSelected) &&
        valid(angarDoorSlot) && valid(angarDoorSlotSelected) &&
        valid(angarDoorDown) && valid(angarDoorUp) &&
        valid(achievementBackground) &&
        valid(achievementBottomPanel) && valid(achievementPanel) &&
        valid(achievementClose) && valid(achievementOk) &&
        valid(achievementOkSelected) &&
        achievementLockedTextures.size() ==
            originalAchievementVisuals.size() &&
        achievementOpenedTextures.size() ==
            originalAchievementVisuals.size() &&
        std::all_of(
            achievementLockedTextures.begin(),
            achievementLockedTextures.end(),
            [](Texture texture) { return valid(texture); }) &&
        std::all_of(
            achievementOpenedTextures.begin(),
            achievementOpenedTextures.end(),
            [](Texture texture) { return valid(texture); }) &&
        angarBossTextures.size() ==
            originalGarage->planets.size() &&
        std::all_of(
            angarBossTextures.begin(), angarBossTextures.end(),
            [](Texture texture) { return valid(texture); }) &&
        std::all_of(
            workshopUpgradeTextures.begin(),
            workshopUpgradeTextures.end(),
            [](Texture texture) { return valid(texture); }) &&
        std::all_of(
            workshopSlotIconTextures.begin(),
            workshopSlotIconTextures.end(),
            [](Texture texture) { return valid(texture); }) &&
        garageCarTextures.size() == originalGarage->cars.size() &&
        std::all_of(
            garageCarTextures.begin(), garageCarTextures.end(),
            [](Texture texture) { return valid(texture); }) &&
        std::all_of(
            garageColorTextures.begin(),
            garageColorTextures.end(),
            [](Texture texture) { return valid(texture); }) &&
        valid(exitRaceMessage.texture) && valid(exitRaceYes.texture) &&
        valid(exitRaceYesSelected.texture) && valid(exitRaceNo.texture) &&
        valid(exitRaceNoSelected.texture) &&
        valid(angarTravelMessage.texture) &&
        valid(angarWarningMessage.texture) &&
        valid(angarOk.texture) &&
        valid(achievementRewards.texture) &&
        valid(achievementPoints.texture) &&
        valid(achievementPurchaseMessage.texture) &&
        valid(achievementNotEnoughPoints.texture);
#else
    const bool optionsResourcesValid = true;
#endif

    auto releaseResources = [&]() {
#ifdef RRR3D_PHYSICS
        device->destroy(exitRaceNoSelected.texture);
        device->destroy(exitRaceNo.texture);
        device->destroy(exitRaceYesSelected.texture);
        device->destroy(exitRaceYes.texture);
        device->destroy(exitRaceMessage.texture);
        device->destroy(angarTravelMessage.texture);
        device->destroy(angarWarningMessage.texture);
        device->destroy(angarOk.texture);
        device->destroy(achievementNotEnoughPoints.texture);
        device->destroy(achievementPurchaseMessage.texture);
        device->destroy(achievementPoints.texture);
        device->destroy(achievementRewards.texture);
        device->destroy(finishSummary.texture);
#endif
        device->destroy(credits.texture);
        device->destroy(version.texture);
#ifdef RRR3D_PHYSICS
        destroyPage(garagePurchasePage);
        destroyPage(garageStatsPage);
        destroyPage(garageInfoPage);
        destroyPage(achievementPricePage);
        destroyPage(achievementsPage);
        destroyPage(angarInfoPage);
        destroyPage(planetsPage);
        destroyPage(workshopConfirmationPage);
        destroyPage(workshopInfoPage);
        destroyPage(workshopHintPage);
        destroyPage(workshopStatsPage);
        destroyPage(workshopControlsPage);
        destroyPage(workshopPage);
        destroyPage(garagePage);
        destroyPage(raceMainInfoPage);
        destroyPage(raceMainHeadersPage);
        destroyPage(raceMenuPage);
        destroyPage(finishPage);
        destroyPage(controlsOptionsPage);
        destroyPage(soundOptionsPage);
        destroyPage(graphicsOptionsPage);
        destroyPage(gameOptionsPage);
        destroyPage(optionsActionPage);
        destroyPage(optionsStatePage);
        destroyPage(optionsHeaderPage);
        destroyPage(controlsGamepadValuesPage);
        destroyPage(controlsKeyboardValuesPage);
        destroyPage(soundOptionNamesPage);
        destroyPage(graphicsOptionNamesPage);
        destroyPage(gameOptionNamesPage);
        device->destroy(optionsMask);
        for (const auto texture : garageColorTextures)
            device->destroy(texture);
        for (const auto texture : garageCarTextures)
            device->destroy(texture);
        for (const auto texture : workshopSlotIconTextures)
            device->destroy(texture);
        for (const auto texture : workshopUpgradeTextures)
            device->destroy(texture);
        for (const auto texture : angarBossTextures)
            device->destroy(texture);
        for (const auto texture : achievementOpenedTextures)
            device->destroy(texture);
        for (const auto texture : achievementLockedTextures)
            device->destroy(texture);
        device->destroy(achievementOkSelected);
        device->destroy(achievementOk);
        device->destroy(achievementClose);
        device->destroy(achievementPanel);
        device->destroy(achievementBottomPanel);
        device->destroy(achievementBackground);
        device->destroy(angarDoorUp);
        device->destroy(angarDoorDown);
        device->destroy(angarDoorSlotSelected);
        device->destroy(angarDoorSlot);
        device->destroy(angarCloseSelected);
        device->destroy(angarClose);
        device->destroy(angarPlanetInfo);
        device->destroy(angarBottomPanel);
        device->destroy(workshopStatBarPlus);
        device->destroy(workshopInfoFrame);
        device->destroy(workshopChargeButtonSelected);
        device->destroy(workshopChargeButton);
        device->destroy(workshopChargeBar);
        device->destroy(workshopChargeBox);
        device->destroy(workshopSlotFrame);
        device->destroy(workshopSlot);
        device->destroy(workshopLeftPanel);
        device->destroy(workshopBottomPanel);
        device->destroy(workshopTopPanel);
        device->destroy(garageBuySelected);
        device->destroy(garageBuy);
        device->destroy(garageBackSelected);
        device->destroy(garageBack);
        device->destroy(garageArrowSelected);
        device->destroy(garageArrow);
        device->destroy(garageColorBoxSelected);
        device->destroy(garageColorBoxBackground);
        device->destroy(garageColorBox);
        device->destroy(garageLock);
        device->destroy(garageCarBoxSelected);
        device->destroy(garageCarBox);
        device->destroy(garageStatBar);
        device->destroy(garageStats);
        device->destroy(garageMoney);
        device->destroy(garageSidePanel);
        device->destroy(garageBottomPanel);
        device->destroy(garageTopPanel);
        for (const auto texture : raceWeatherIcons)
            device->destroy(texture);
        for (const auto texture : raceMenuIcons)
            device->destroy(texture);
        device->destroy(raceChargeBar);
        device->destroy(raceImageFrame);
        device->destroy(raceStats);
        device->destroy(raceMoney);
        device->destroy(raceMenuButtonSelected);
        device->destroy(raceMenuButton);
        device->destroy(raceBottomPanel);
        device->destroy(raceTopPanel);
        device->destroy(gamepadIcon);
        device->destroy(keyboardIcon);
        device->destroy(optionsKeySelected);
        device->destroy(optionsKey);
        device->destroy(optionsButtonSelected);
        device->destroy(optionsButton);
        device->destroy(optionsBar);
        device->destroy(optionsBarBackground);
        device->destroy(optionsArrowSelected);
        device->destroy(optionsArrow);
        device->destroy(controlsRow);
        device->destroy(optionsRow);
        device->destroy(optionsBackground);
        device->destroy(acceptButtonSelected);
        device->destroy(acceptButton);
        device->destroy(acceptFrame);
#endif
        destroyPage(creditsPage);
        destroyPage(optionsPage);
        destroyPage(networkPage);
        destroyPage(profilePage);
        destroyPage(difficultyPage);
        destroyPage(tournamentPage);
        destroyPage(gameModePage);
        destroyPage(mainPage);
        device->destroy(cursor);
        device->destroy(selection);
        device->destroy(bottomPanel);
        device->destroy(topPanel);
        device->destroy(background);
        device->destroy(quad);
#ifdef RRR3D_PHYSICS
        device->destroy(raceShader);
#endif
        device->destroy(shader);
    };

    if (!gpuResourcesValid || !optionsResourcesValid)
    {
        std::cerr << "Unable to create original MainMenu2 GPU resources\n";
        releaseResources();
        device.reset();
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }

    std::cout << "RRR3D renderer backend: bgfx/" << device->backendName()
              << '\n'
              << "MainMenu2 artwork: " << model->backgroundImage.virtualPath
              << ", " << model->topPanelImage.virtualPath << ", "
              << model->selectionImage.virtualPath << '\n'
              << "MainMenu2 source: shared specification used by legacy "
                 "MainMenu2.cpp\n"
              << "MainMenu2 font: requested " << menu::fontFace
              << ", resolved " << resolvedFont << '\n';
#ifdef RRR3D_GAMEPAD_INPUT
    std::cout << "Input: SDL3 keyboard/mouse/gamepad, "
              << input.connectedGamepadCount() << " gamepad(s)\n";
#endif

#ifdef RRR3D_PHYSICS
    std::string physicsError;
    auto physicsWorld = r3d::physics::createOriginalVehicleWorld(
        *physicsDescription, physicsError);
    r3d::game::originalrace::OriginalRaceSession raceSession(*originalRace);
    raceSession.setCampaign(true);
    raceSession.applyPlayerProfile(profileState.player);
    raceSession.applyAchievementProfile(profileState);
    raceSession.setEnableMineBug(profileState.config.enableMineBug);
    raceSession.setSpringBorders(profileState.config.springBorders);
    rrr3d::race::OriginalRaceRenderer raceRenderer;
    rrr3d::race::OriginalRaceRenderer garageRenderer;
    rrr3d::race::OriginalRaceRenderer angarRenderer;
    rrr3d::race::OriginalWorkshopRenderer workshopRenderer;
    rrr3d::race::OriginalRaceHud raceHud;
    if (!physicsWorld ||
        !raceRenderer.initialize(*device, *resources, *originalRace,
                                 static_cast<std::uint32_t>(pixelWidth),
                                 static_cast<std::uint32_t>(pixelHeight),
                                 physicsError) ||
        !garageRenderer.initialize(
            *device, *resources, *originalGarageScene,
            static_cast<std::uint32_t>(pixelWidth),
            static_cast<std::uint32_t>(pixelHeight), physicsError) ||
        !angarRenderer.initialize(
            *device, *resources, *originalAngarScene,
            static_cast<std::uint32_t>(pixelWidth),
            static_cast<std::uint32_t>(pixelHeight), physicsError) ||
        !workshopRenderer.initialize(
            *device, *resources, *originalGarage, *originalRace,
            physicsError) ||
        !raceHud.initialize(*device, *resources, *originalRace,
                            activeLanguage,
                            profileState.player.difficulty,
                            true,
                            physicsError))
    {
        std::cerr << "Original race initialization failed: " << physicsError
                  << '\n';
        workshopRenderer.shutdown(*device);
        angarRenderer.shutdown(*device);
        garageRenderer.shutdown(*device);
        raceRenderer.shutdown(*device);
        raceHud.shutdown(*device);
        releaseResources();
        device.reset();
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }
    if (options->raceRenderSmokeTest)
    {
        std::string resizeError;
        const auto smokeWidth = static_cast<std::uint32_t>(
            std::max(pixelWidth / 2, 320));
        const auto smokeHeight = static_cast<std::uint32_t>(
            std::max(pixelHeight / 2, 200));
        if (!raceRenderer.resize(
                *device, smokeWidth, smokeHeight, resizeError) ||
            !raceRenderer.resize(
                *device, static_cast<std::uint32_t>(pixelWidth),
                static_cast<std::uint32_t>(pixelHeight), resizeError) ||
            !garageRenderer.resize(
                *device, smokeWidth, smokeHeight, resizeError) ||
            !garageRenderer.resize(
                *device, static_cast<std::uint32_t>(pixelWidth),
                static_cast<std::uint32_t>(pixelHeight), resizeError))
        {
            std::cerr << "M9.3 renderer target resize round-trip failed: "
                      << resizeError << '\n';
            workshopRenderer.shutdown(*device);
            angarRenderer.shutdown(*device);
            garageRenderer.shutdown(*device);
            raceRenderer.shutdown(*device);
            raceHud.shutdown(*device);
            releaseResources();
            device.reset();
            SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
            input.shutdown();
#endif
            SDL_Quit();
            return EXIT_FAILURE;
        }
    }
    std::vector<r3d::physics::VehicleState> garageVehicles;
    garageVehicles.reserve(originalGarageScene->vehicles.size());
    for (const auto& vehicle : originalGarageScene->vehicles)
        garageVehicles.push_back(makeGarageVehicleState(vehicle));
    std::vector<r3d::game::originalrace::RacerRuntime>
        garageRacerRuntime(originalGarageScene->racers.size());
    std::vector<bool> garageDecorationActive{true, false};
    const std::vector<
        r3d::game::originalrace::DecorationFragmentState>
        garageDecorationFragments;
    const std::vector<
        r3d::game::originalrace::VehicleDeathFragmentState>
        garageVehicleDeathFragments;
    const std::vector<bool> garageBonusActive;
    const std::vector<r3d::game::originalrace::RaceEffect>
        garageEffects;
    const std::vector<r3d::game::originalrace::MineRuntime>
        garageMines;
    const std::vector<r3d::game::originalrace::ProjectileRuntime>
        garageProjectiles;
    const std::vector<r3d::physics::VehicleState> angarVehicles;
    const std::vector<
        r3d::game::originalrace::RacerRuntime> angarRacerRuntime;
    std::vector<bool> angarDecorationActive{true, true};
    float garageSceneSeconds = 0.0F;
    std::cout << "Milestone 9 race: " << originalRace->levelPath << ", "
              << originalRace->lapCount << " laps, "
              << originalRace->trackInstances.size()
              << " original track placements, car "
              << originalRace->vehicle.record
              << ", Jolt backend with original db.xml parameters\n";
#endif

#ifdef RRR3D_AUDIO
    rrr3d::audio::SdlAudioBackend audio;
    std::string audioError;
    if (!audio.initialize(audioError))
    {
        std::cerr << "Audio initialization failed: " << audioError << '\n';
        releaseResources();
        device.reset();
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }

    std::cout << "Audio: SDL3/" << audio.driverName()
              << ", 48 kHz stereo float mixer, default output '"
              << audio.outputDeviceName() << "'\n";
    if (options->audioSmokeTest &&
        !rrr3d::audio::runSdlAudioSmokeTest(audio, *resources, audioError))
    {
        std::cerr << "Milestone 8 audio smoke test failed: " << audioError
                  << '\n';
        audio.shutdown();
        releaseResources();
        device.reset();
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }
    if (options->audioSmokeTest)
    {
        std::cout << "Milestone 8 audio smoke: 182 original Ogg containers; "
                     "music, MainMenu2 click, and gameplay decode; PCM "
                     "signal, mixing, legacy volumes, pause/resume, loop "
                     "boundary, device events, and release passed\n";
    }

    if (options->audioSmokeTest &&
        !r3d::game::runMusicCatSmokeTest(audioError))
    {
        std::cerr << "Milestone 8 MusicCat policy smoke test failed: "
                  << audioError << '\n';
        audio.shutdown();
        releaseResources();
        device.reset();
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }

    auto loadAudio = [&](std::string_view legacyPath,
                         r3d::audio::SoundInfo& info) {
        try
        {
            return audio.loadOgg(
                resources->resolve(dataAudioPath(legacyPath)), info,
                audioError);
        }
        catch (const std::exception& exception)
        {
            audioError = exception.what();
            return r3d::audio::invalidSound;
        }
    };

    r3d::audio::SoundInfo clickInfo;
    const auto clickSound =
        loadAudio(originalaudio::mainButtonClick, clickInfo);
    if (clickSound == r3d::audio::invalidSound)
    {
        std::cerr << "Original MainMenu2 audio loading failed: "
                  << audioError << '\n';
        audio.shutdown();
        releaseResources();
        device.reset();
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }

    audio.setMasterVolume(1.0F);
#ifdef RRR3D_PHYSICS
    audio.setBusVolume(r3d::audio::Bus::Music,
                       profileState.config.musicVolume);
    audio.setBusVolume(r3d::audio::Bus::Effects,
                       profileState.config.effectsVolume);
    audio.setBusVolume(r3d::audio::Bus::Voice,
                       profileState.config.voiceVolume);
#else
    audio.setBusVolume(r3d::audio::Bus::Music,
                       originalaudio::defaultMusicVolume);
    audio.setBusVolume(r3d::audio::Bus::Effects,
                       originalaudio::defaultEffectsVolume);
    audio.setBusVolume(r3d::audio::Bus::Voice,
                       originalaudio::defaultVoiceVolume);
#endif

    auto musicStatePath =
        rrr3d::platform::save_directory() / "menu-music.state";
    if (options->audioSmokeTest)
    {
        musicStatePath =
            rrr3d::platform::save_directory() / "menu-music-smoke.state";
        std::error_code removeError;
        std::filesystem::remove(musicStatePath, removeError);
        auto temporary = musicStatePath;
        temporary += ".tmp";
        std::filesystem::remove(temporary, removeError);
    }
    rrr3d::audio::OriginalMenuMusic music(
        audio, *resources, musicStatePath,
        options->audioSmokeTest ? 0x4d75736963436174ULL
                                : rrr3d::platform::steady_nanoseconds(),
        true);
    if (!music.initialize(audioError))
    {
        std::cerr << "Original MusicCat initialization failed: "
                  << audioError << '\n';
        music.shutdown();
        audio.unloadSound(clickSound);
        audio.shutdown();
        releaseResources();
        device.reset();
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }

#ifdef RRR3D_PHYSICS
    const auto gameMusicStatePath =
        rrr3d::platform::save_directory() / "game-music.state";
    rrr3d::audio::OriginalMenuMusic gameMusic(
        audio, *resources, gameMusicStatePath,
        rrr3d::platform::steady_nanoseconds() ^
            0x47616d654d757369ULL,
        true, musicTracks(originalaudio::gameTracks),
        musicPlaylist(profileState.config.gameMusicPlaylist,
                      originalaudio::gameTracks.size()));
    if (!gameMusic.initialize(audioError) ||
        !gameMusic.pause(true, audioError))
    {
        std::cerr << "Original game MusicCat initialization failed: "
                  << audioError << '\n';
        gameMusic.shutdown();
        music.shutdown();
        audio.unloadSound(clickSound);
        audio.shutdown();
        releaseResources();
        device.reset();
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }
#endif

    std::cout << "Original MusicCat: background decode, shuffled playlist, "
                 "auto Next, pause/resume, state "
              << musicStatePath << "\nOriginal menu tracks:";
    for (const auto& track : originalaudio::menuTracks)
        std::cout << " [" << track.band << " - " << track.name
                  << ": Data/" << track.path << ']';
    std::cout << "\nMainMenu2 ssButton1: Data/"
              << originalaudio::mainButtonClick << '\n';

    auto playMainButtonClick = [&]() {
        r3d::audio::PlayOptions clickOptions;
        clickOptions.bus = r3d::audio::Bus::Effects;
        std::string clickError;
        if (audio.play(clickSound, clickOptions, clickError) ==
            r3d::audio::invalidVoice)
        {
            SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO,
                        "Unable to play original MainMenu2 click: %s",
                        clickError.c_str());
            return false;
        }
        return true;
    };
#ifdef RRR3D_PHYSICS
    struct EngineAudio
    {
        r3d::audio::SoundHandle idle = r3d::audio::invalidSound;
        r3d::audio::SoundHandle rpm = r3d::audio::invalidSound;
        r3d::audio::VoiceHandle idleVoice = r3d::audio::invalidVoice;
        r3d::audio::VoiceHandle rpmVoice = r3d::audio::invalidVoice;
    };
    std::map<std::string, r3d::audio::SoundHandle> engineSounds;
    std::vector<EngineAudio> engineAudio(originalRace->racers.size());
    std::vector<r3d::audio::SoundHandle> weaponAudio(
        originalRace->weapons.size(), r3d::audio::invalidSound);
    std::vector<r3d::audio::SoundHandle> bonusDeathAudio;
    auto loadEngineSound = [&](const std::string& path) {
        const auto found = engineSounds.find(path);
        if (found != engineSounds.end())
            return found->second;
        r3d::audio::SoundInfo info;
        const auto sound =
            audio.loadOgg(resources->resolve(path), info, audioError);
        if (sound != r3d::audio::invalidSound)
            engineSounds.emplace(path, sound);
        return sound;
    };
    bool engineAudioValid = true;
    for (std::size_t racer = 0;
         racer < originalRace->racers.size(); ++racer)
    {
        const auto& sourceRacer = originalRace->racers[racer];
        const auto& vehicle =
            sourceRacer.hasConfiguredVehicle
                ? sourceRacer.configuredVehicle
                : originalRace->vehicles.at(sourceRacer.vehicle);
        engineAudio[racer].idle =
            loadEngineSound(vehicle.idleSoundPath);
        engineAudio[racer].rpm =
            loadEngineSound(vehicle.rpmSoundPath);
        engineAudioValid =
            engineAudioValid &&
            engineAudio[racer].idle != r3d::audio::invalidSound &&
            engineAudio[racer].rpm != r3d::audio::invalidSound;
    }
    for (std::size_t weapon = 0;
         weapon < originalRace->weapons.size(); ++weapon)
    {
        const auto& sounds =
            originalRace->weapons[weapon].shotEffect.soundPaths;
        if (!sounds.empty())
        {
            weaponAudio[weapon] = loadEngineSound(sounds.front());
            engineAudioValid =
                engineAudioValid &&
                weaponAudio[weapon] != r3d::audio::invalidSound;
        }
    }
    auto reloadBonusDeathAudio = [&]() {
        bonusDeathAudio.assign(
            originalRace->bonuses.size(),
            r3d::audio::invalidSound);
        bool valid = true;
        for (std::size_t bonus = 0;
             bonus < originalRace->bonuses.size(); ++bonus)
        {
            const auto& sounds =
                originalRace->bonuses[bonus]
                    .deathEffect.visual.soundPaths;
            if (sounds.empty())
                continue;
            bonusDeathAudio[bonus] =
                loadEngineSound(sounds.front());
            valid =
                valid &&
                bonusDeathAudio[bonus] !=
                    r3d::audio::invalidSound;
        }
        return valid;
    };
    const bool bonusDeathAudioValid =
        reloadBonusDeathAudio();
    engineAudioValid =
        engineAudioValid && bonusDeathAudioValid;
    const auto acceptanceAudio =
        loadEngineSound("Data/Sounds/UI/acception.ogg");
    const auto crashAudio =
        loadEngineSound("Data/Sounds/carcrash05.ogg");
    std::string destructionSoundPath;
    if (!originalRace->racers.empty())
    {
        const auto& sourceRacer = originalRace->racers.front();
        const auto& vehicle =
            sourceRacer.hasConfiguredVehicle
                ? sourceRacer.configuredVehicle
                : originalRace->vehicles.at(sourceRacer.vehicle);
        for (const auto& effect : vehicle.deathEffects)
        {
            if (effect.visual.soundPaths.empty())
                continue;
            destructionSoundPath = effect.visual.soundPaths.front();
            break;
        }
    }
    if (destructionSoundPath.empty())
        destructionSoundPath = "Data/Sounds/carcrash05.ogg";
    const auto destructionAudio =
        loadEngineSound(destructionSoundPath);
    std::array<r3d::audio::SoundHandle, 5> impactAudio{};
    std::vector<float> damageAudioCooldown(
        originalRace->racers.size(), 0.0F);
    for (std::size_t index = 0; index < impactAudio.size(); ++index)
    {
        impactAudio[index] = loadEngineSound(
            "Data/Sounds/light_impact0" +
            std::to_string(index + 1U) + ".ogg");
    }
    rrr3d::audio::OriginalRaceCommentator commentator(
        audio, *resources);
    const bool commentatorValid = commentator.initialize(
        profileState.config.commentatorStyle, audioError);
    engineAudioValid =
        engineAudioValid &&
        acceptanceAudio != r3d::audio::invalidSound &&
        crashAudio != r3d::audio::invalidSound &&
        destructionAudio != r3d::audio::invalidSound &&
        std::all_of(
            impactAudio.begin(), impactAudio.end(),
            [](r3d::audio::SoundHandle sound) {
                return sound != r3d::audio::invalidSound;
            }) &&
        commentatorValid;
    if (!engineAudioValid)
    {
        std::cerr << "Original race engine audio loading failed: "
                  << audioError << '\n';
        commentator.shutdown();
        gameMusic.shutdown();
        music.shutdown();
        for (const auto& [path, sound] : engineSounds)
        {
            static_cast<void>(path);
            audio.unloadSound(sound);
        }
        audio.unloadSound(clickSound);
        audio.shutdown();
        raceHud.shutdown(*device);
        workshopRenderer.shutdown(*device);
        angarRenderer.shutdown(*device);
        garageRenderer.shutdown(*device);
        raceRenderer.shutdown(*device);
        releaseResources();
        device.reset();
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }
    auto startRaceAudio = [&]() {
        audio.setBusVolume(r3d::audio::Bus::Effects,
                           profileState.config.effectsVolume);
        music.pause(true, audioError);
        gameMusic.pause(false, audioError);
        commentator.pause(false);
        commentator.reset();
        std::fill(damageAudioCooldown.begin(),
                  damageAudioCooldown.end(), 0.0F);
        for (std::size_t racer = 0; racer < engineAudio.size();
             ++racer)
        {
            r3d::audio::PlayOptions options;
            options.bus = r3d::audio::Bus::Effects;
            options.loop = true;
            options.volume = racer == 0 ? 0.5F : 0.0F;
            engineAudio[racer].idleVoice = audio.play(
                engineAudio[racer].idle, options, audioError);
            options.volume = racer == 0 ? 0.2F : 0.0F;
            engineAudio[racer].rpmVoice = audio.play(
                engineAudio[racer].rpm, options, audioError);
        }
    };
    auto stopRaceAudio = [&](bool advanceGameTrack = true) {
        for (auto& engine : engineAudio)
        {
            audio.stop(engine.idleVoice);
            audio.stop(engine.rpmVoice);
            engine.idleVoice = r3d::audio::invalidVoice;
            engine.rpmVoice = r3d::audio::invalidVoice;
        }
        commentator.pause(true);
        gameMusic.pause(true, audioError);
        if (advanceGameTrack)
            gameMusic.next(audioError);
        music.pause(false, audioError);
    };
#endif
#endif

    PipelineState opaque;
    opaque.faceCulling = PipelineState::FaceCulling::None;
    opaque.writeDepth = false;
    opaque.depthTest = false;
    PipelineState transparent = opaque;
    transparent.alphaBlend = true;
#ifdef RRR3D_PHYSICS
    PipelineState racePipeline;
    // ContextInfo.cpp uses MatrixLookAtRH/MatrixPerspectiveFovRH and the
    // default D3D9 state D3DCULL_CW.  Preserve that state directly; mirrored
    // nodes carry their original cullMode/invertCullFace overrides.
    racePipeline.faceCulling =
        PipelineState::FaceCulling::Clockwise;
#endif
    const Camera camera = makeCamera(*device);

    bool running = true;
    bool runtimeSmokeFailed = false;
    std::uint32_t renderedFrames = 0;
    enum class MenuScreen
    {
        Main,
        GameMode,
        Tournament,
        Difficulty,
        Profiles,
        Network,
        Options,
        Credits,
#ifdef RRR3D_PHYSICS
        RaceMenu,
        Garage,
        Workshop,
        Planets,
        Achievements,
        GameOptions,
        GraphicsOptions,
        SoundOptions,
        ControlsOptions,
        Finish,
#endif
    };
    std::vector<MenuScreen> menuStack{MenuScreen::Main};
    std::size_t menuSelection = 0;
    bool championshipMode = true;
    bool newTournamentProfile = false;
#ifdef RRR3D_PHYSICS
    struct GarageCarView
    {
        std::size_t catalogIndex = 0U;
        bool locked = false;
    };
    std::vector<GarageCarView> garageCarOrder;
    std::size_t garageCarIndex = 0;
    std::size_t garageViewIndex = 0;
    bool garagePurchaseDialogVisible = false;
    bool garagePurchaseYesFocused = true;
    for (std::size_t index = 0;
         index < originalGarage->cars.size(); ++index)
    {
        if (originalGarage->cars[index].record ==
            profileState.player.currentCar)
        {
            garageCarIndex = index;
            break;
        }
    }
    struct WorkshopDrag
    {
        r3d::game::originalrace::ProfileSlot item;
        std::optional<r3d::game::originalrace::GarageSlotType> origin;

        bool active() const noexcept
        {
            return !item.record.empty();
        }
    };
    WorkshopDrag workshopDrag;
    enum class WorkshopConfirmation
    {
        None,
        Buy,
        Sell
    };
    WorkshopConfirmation workshopConfirmation =
        WorkshopConfirmation::None;
    const r3d::game::originalrace::OriginalWorkshopItem*
        workshopPendingPurchase = nullptr;
    bool workshopConfirmationYesFocused = true;
    float workshopDragX = menu::virtualWidth * 0.5F;
    float workshopDragY = menu::virtualHeight * 0.5F;
    std::vector<const r3d::game::originalrace::OriginalWorkshopItem*>
        workshopGoods;
    std::size_t workshopGoodScroll = 0U;
    bool achievementPurchaseDialogVisible = false;
    bool achievementPurchaseYesFocused = true;
    bool achievementPointsWarningVisible = false;
    std::size_t achievementPendingPurchase =
        originalAchievementNoTarget;
#endif
    auto activeMenuPage = [&]() -> MenuPageVisual& {
        switch (menuStack.back())
        {
        case MenuScreen::Main:
            return mainPage;
        case MenuScreen::GameMode:
            return gameModePage;
        case MenuScreen::Tournament:
            return tournamentPage;
        case MenuScreen::Difficulty:
            return difficultyPage;
        case MenuScreen::Profiles:
            return profilePage;
        case MenuScreen::Network:
            return networkPage;
        case MenuScreen::Options:
            return optionsPage;
        case MenuScreen::Credits:
            return creditsPage;
#ifdef RRR3D_PHYSICS
        case MenuScreen::RaceMenu:
            return raceMenuPage;
        case MenuScreen::Garage:
            return garagePage;
        case MenuScreen::Workshop:
            return workshopPage;
        case MenuScreen::Planets:
            return planetsPage;
        case MenuScreen::Achievements:
            return achievementsPage;
        case MenuScreen::GameOptions:
            return gameOptionsPage;
        case MenuScreen::GraphicsOptions:
            return graphicsOptionsPage;
        case MenuScreen::SoundOptions:
            return soundOptionsPage;
        case MenuScreen::ControlsOptions:
            return controlsOptionsPage;
        case MenuScreen::Finish:
            return finishPage;
#endif
        }
        return mainPage;
    };
    auto pushMenu = [&](MenuScreen screen) {
        menuStack.push_back(screen);
        menuSelection = 0;
    };
    auto backMenu = [&]() {
        if (menuStack.size() > 1U)
            menuStack.pop_back();
        menuSelection = 0;
    };
#ifdef RRR3D_PHYSICS
    std::optional<std::string> bindingCaptureAction;
    bool bindingCaptureGamepad = false;
    auto originalKeyName = [](SDL_Scancode scancode) {
        switch (scancode)
        {
        case SDL_SCANCODE_UP:
            return std::string("Up Arrow");
        case SDL_SCANCODE_DOWN:
            return std::string("Down Arrow");
        case SDL_SCANCODE_LEFT:
            return std::string("Left Arrow");
        case SDL_SCANCODE_RIGHT:
            return std::string("Right Arrow");
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER:
            return std::string("Enter");
        default:
            return std::string(SDL_GetScancodeName(scancode));
        }
    };
    auto originalGamepadButtonName = [](SDL_GamepadButton button)
        -> std::optional<std::string> {
        switch (button)
        {
        case SDL_GAMEPAD_BUTTON_SOUTH:
            return "A";
        case SDL_GAMEPAD_BUTTON_EAST:
            return "B";
        case SDL_GAMEPAD_BUTTON_WEST:
            return "X";
        case SDL_GAMEPAD_BUTTON_NORTH:
            return "Y";
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
            return "DPad Up";
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
            return "DPad Down";
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            return "DPad Left";
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            return "DPad Right";
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
            return "Left Shoulder";
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
            return "Right Shoulder";
        case SDL_GAMEPAD_BUTTON_LEFT_STICK:
            return "L.Thumb Press";
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
            return "R.Thumb Press";
        case SDL_GAMEPAD_BUTTON_BACK:
            return "Back";
        case SDL_GAMEPAD_BUTTON_START:
            return "Start";
        default:
            return std::nullopt;
        }
    };
    auto originalGamepadAxisName = [](SDL_GamepadAxis axis, Sint16 value)
        -> std::optional<std::string> {
        if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER && value > 15000)
            return "Left Trigger";
        if (axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER && value > 15000)
            return "Right Trigger";
        if (std::abs(static_cast<int>(value)) < 20000)
            return std::nullopt;
        switch (axis)
        {
        case SDL_GAMEPAD_AXIS_LEFTX:
            return value < 0 ? "L.Thumb Left" : "L.Thumb Right";
        case SDL_GAMEPAD_AXIS_LEFTY:
            return value < 0 ? "L.Thumb Up" : "L.Thumb Down";
        case SDL_GAMEPAD_AXIS_RIGHTX:
            return value < 0 ? "R.Thumb Left" : "R.Thumb Right";
        case SDL_GAMEPAD_AXIS_RIGHTY:
            return value < 0 ? "R.Thumb Up" : "R.Thumb Down";
        default:
            return std::nullopt;
        }
    };
#endif
#ifdef RRR3D_PHYSICS
    bool inRace = false;
    bool exitRaceDialogVisible = false;
    bool exitRaceYesFocused = true;
    r3d::physics::VehicleInput raceInput;
    bool raceUseWeaponRequested = false;
    bool raceUseAllWeaponsRequested = false;
    bool raceUseMine = false;
    bool raceUseHyper = false;
    bool raceChangeWeaponRequested = false;
    int raceWeaponChangeDirection = 1;
    int raceFireWeaponSlotRequested = -1;
    bool raceResetRequested = false;
    std::vector<r3d::physics::VehicleState> raceVehicles(
        physicsWorld->vehicleCount());
    struct DecorationDebrisBinding
    {
        std::size_t instance = 0;
        std::size_t piece = 0;
        std::size_t debris = 0;
    };
    std::vector<DecorationDebrisBinding> decorationDebrisBindings;
    std::vector<r3d::game::originalrace::DecorationFragmentState>
        decorationFragments;
    struct VehicleDebrisBinding
    {
        std::size_t racer = 0;
        std::size_t effect = 0;
        std::size_t debris = 0;
    };
    std::vector<VehicleDebrisBinding> vehicleDebrisBindings;
    std::vector<r3d::game::originalrace::VehicleDeathFragmentState>
        vehicleDeathFragments;
    for (std::size_t index = 0; index < physicsWorld->vehicleCount();
         ++index)
        raceVehicles[index] = physicsWorld->vehicle(index);
    float raceElapsedSeconds = 0.0F;
    std::uint64_t previousFrameTicks = SDL_GetTicksNS();
    bool integratedRaceStartObserved = !options->raceRenderSmokeTest;
    bool raceGarageFrameObserved = !options->raceRenderSmokeTest;
    bool raceGarage3DObserved = !options->raceRenderSmokeTest;
    bool raceWorkshopFrameObserved = !options->raceRenderSmokeTest;
    bool raceWorkshop3DObserved = !options->raceRenderSmokeTest;
    bool raceAngarFrameObserved = !options->raceRenderSmokeTest;
    bool raceAngar3DObserved = !options->raceRenderSmokeTest;
    bool raceAchievementFrameObserved =
        !options->raceRenderSmokeTest;
    bool racePauseDialogObserved = !options->raceRenderSmokeTest;
    bool racePauseResumeObserved = !options->raceRenderSmokeTest;
    bool racePauseFrozenObserved = !options->raceRenderSmokeTest;
    bool racePlayerDestroyedObserved = false;
    float minimumRacePlayerLife =
        std::numeric_limits<float>::max();
    std::uint32_t racePauseSmokeStep = 0U;
    float racePauseElapsedSnapshot = -1.0F;
    r3d::physics::Vec3 racePausePositionSnapshot;
    float maximumRaceSmokeSpeed = 0.0F;
    std::uint32_t maximumRaceSmokeContacts = 0;
    std::array<std::uint32_t, r3d::renderer::renderPassCount>
        maximumRacePassBegins{};
    std::array<std::uint32_t, r3d::renderer::renderPassCount>
        maximumRacePassDraws{};
    std::array<std::uint32_t, 7> maximumRaceLightingDraws{};
    std::uint32_t maximumEnvironmentMappedDraws = 0;
    std::uint32_t maximumNormalMappedDraws = 0;
    std::uint32_t maximumTransientDraws = 0;
    std::array<bool, 2> raceCameraStylesObserved{};
    bool raceProgressSaved = false;
    bool finishMenuShown = false;
    std::uint32_t raceSmokeMenuStep = 0;
    std::uint32_t raceSmokeNextMenuFrame = 0;
    bool raceSmokeAccelerateQueued = false;
    bool racePlanetChampion = false;
    int angarPlanetIndex = -1;
    int angarPreviousPlanetIndex = -1;
    float angarDoorTime = -1.0F;
    float angarSceneSeconds = 0.0F;
    bool angarTravelDialogVisible = false;
    bool angarWarningVisible = false;
    bool angarTravelYesFocused = true;
    std::size_t angarTravelTarget = 0U;
    auto saveRaceProfile = [&]() {
        if (!raceSession.racers().empty())
        {
            raceSession.writePlayerProfile(profileState.player);
            raceSession.writeAchievementProfile(profileState);
            if (raceSession.racers().front().finished &&
                !raceProgressSaved)
            {
                const auto completedTrack = selectedTrack;
                const auto advance =
                    r3d::game::originalrace::
                        completeOriginalTournamentTrack(
                            *originalRace, selectedTrack, profileState);
                selectedTrack = advance.trackIndex;
                racePlanetChampion = advance.planetChampion;
                raceProgressSaved = true;
                std::cout
                    << "Original Tournament::CompleteTrack: track "
                    << completedTrack << " -> " << advance.trackIndex
                    << ", passComplete=" << advance.passComplete
                    << ", passChampion=" << advance.passChampion
                    << ", planetChampion=" << advance.planetChampion
                    << '\n';
            }
        }
        std::string profileError;
        if (!profileStore.save(profileState, profileError))
            std::cerr << "Unable to save original profile: "
                      << profileError << '\n';
    };
    auto reloadCurrentRace = [&]() {
        try
        {
#ifdef RRR3D_AUDIO
            stopRaceAudio(false);
#endif
            raceHud.shutdown(*device);
            raceRenderer.shutdown(*device);
            *originalRace =
                r3d::game::originalrace::loadOriginalRace(
                    *resources, selectedTrack,
                    profileState.player.currentCar);
            if (options->weatherSelected)
                applyWeather(
                    originalRace->environment, options->weather);
            profileState.player.currentCar =
                originalRace->vehicle.record;
            if (!originalRace->racers.empty())
                originalRace->racers.front().name =
                    profileState.player.name;
            r3d::game::originalrace::applyOriginalPlayerProfile(
                *originalRace, *resources, profileState.player);
            r3d::game::originalrace::
                writeOriginalTournamentSelection(
                    *originalRace, selectedTrack,
                    profileState.player);
            if (!championshipMode)
            {
                // GameMode::StartRace applies these GameFrame values only
                // to rmSkirmish.  Championship keeps the tournament's own
                // lap and six-racer definitions.
                originalRace->lapCount =
                    std::clamp<std::uint32_t>(
                        profileState.config.lapsCount, 1U, 8U);
                const auto skirmishRacers =
                    std::min<std::size_t>(
                        originalRace->racers.size(),
                        std::clamp<std::uint32_t>(
                            profileState.config.maxComputers,
                            0U, 5U) +
                            1U);
                originalRace->racers.resize(
                    std::max<std::size_t>(
                        skirmishRacers, 1U));
            }
            *physicsDescription =
                r3d::game::originalrace::makePhysicsDescription(
                    *originalRace, *resources);
            std::string reloadError;
            physicsWorld =
                r3d::physics::createOriginalVehicleWorld(
                    *physicsDescription, reloadError);
            decorationDebrisBindings.clear();
            decorationFragments.clear();
            vehicleDebrisBindings.clear();
            vehicleDeathFragments.clear();
            if (!physicsWorld ||
                !raceRenderer.initialize(
                    *device, *resources, *originalRace,
                    static_cast<std::uint32_t>(pixelWidth),
                    static_cast<std::uint32_t>(pixelHeight),
                    reloadError) ||
                !raceHud.initialize(
                    *device, *resources, *originalRace,
                    activeLanguage, profileState.player.difficulty,
                    championshipMode,
                    reloadError))
            {
                std::cerr
                    << "Unable to reload original tournament race: "
                    << reloadError << '\n';
                return false;
            }
            raceSession.reset();
            raceSession.setCampaign(championshipMode);
            raceSession.applyPlayerProfile(profileState.player);
            raceSession.applyAchievementProfile(profileState);
            raceSession.setEnableMineBug(
                profileState.config.enableMineBug);
            raceSession.setSpringBorders(
                profileState.config.springBorders);
#ifdef RRR3D_AUDIO
            engineAudio.assign(
                originalRace->racers.size(), EngineAudio{});
            for (std::size_t racer = 0;
                 racer < originalRace->racers.size(); ++racer)
            {
                const auto vehicleIndex =
                    originalRace->racers[racer].vehicle;
                if (vehicleIndex >= originalRace->vehicles.size())
                    continue;
                const auto& vehicle =
                    originalRace->vehicles[vehicleIndex];
                engineAudio[racer].idle =
                    loadEngineSound(vehicle.idleSoundPath);
                engineAudio[racer].rpm =
                    loadEngineSound(vehicle.rpmSoundPath);
            }
            damageAudioCooldown.assign(
                originalRace->racers.size(), 0.0F);
            if (!reloadBonusDeathAudio())
            {
                std::cerr
                    << "Unable to reload original bonus DeathEffect "
                       "audio\n";
                return false;
            }
#endif
            raceVehicles.resize(physicsWorld->vehicleCount());
            for (std::size_t index = 0;
                 index < physicsWorld->vehicleCount(); ++index)
                raceVehicles[index] = physicsWorld->vehicle(index);
            return true;
        }
        catch (const std::exception& exception)
        {
            std::cerr
                << "Unable to reload original tournament data: "
                << exception.what() << '\n';
            return false;
        }
    };
    auto startCurrentRace = [&]() {
        if (!reloadCurrentRace())
        {
            runtimeSmokeFailed = true;
            running = false;
            return;
        }
        raceRenderer.resetCamera();
        raceInput = {};
        raceUseWeaponRequested = false;
        raceUseAllWeaponsRequested = false;
        raceUseMine = false;
        raceUseHyper = false;
        raceChangeWeaponRequested = false;
        raceWeaponChangeDirection = 1;
        raceFireWeaponSlotRequested = -1;
        raceResetRequested = false;
        exitRaceDialogVisible = false;
        exitRaceYesFocused = true;
        racePauseElapsedSnapshot = -1.0F;
        raceElapsedSeconds = 0.0F;
        raceProgressSaved = false;
        finishMenuShown = false;
        raceVehicles.resize(physicsWorld->vehicleCount());
        for (std::size_t index = 0;
             index < physicsWorld->vehicleCount(); ++index)
            raceVehicles[index] = physicsWorld->vehicle(index);
        inRace = true;
        if (options->raceRenderSmokeTest)
            integratedRaceStartObserved = true;
        previousFrameTicks = SDL_GetTicksNS();
#ifdef RRR3D_AUDIO
        startRaceAudio();
#endif
        std::cout << "MainMenu2 -> original race: "
                  << originalRace->levelPath << '\n';
    };
    auto clearRaceControls = [&]() {
        raceInput = {};
        raceUseWeaponRequested = false;
        raceUseAllWeaponsRequested = false;
        raceUseMine = false;
        raceUseHyper = false;
        raceChangeWeaponRequested = false;
        raceWeaponChangeDirection = 1;
        raceFireWeaponSlotRequested = -1;
        raceResetRequested = false;
    };
    auto closeExitRaceDialog = [&]() {
        if (options->raceRenderSmokeTest &&
            racePauseElapsedSnapshot >= 0.0F &&
            physicsWorld->vehicleCount() > 0U)
        {
            const auto currentPosition =
                physicsWorld->vehicle().body.position;
            const float dx = currentPosition.x -
                             racePausePositionSnapshot.x;
            const float dy = currentPosition.y -
                             racePausePositionSnapshot.y;
            const float dz = currentPosition.z -
                             racePausePositionSnapshot.z;
            racePauseFrozenObserved =
                std::abs(raceSession.elapsedSeconds() -
                         racePauseElapsedSnapshot) < 0.0001F &&
                dx * dx + dy * dy + dz * dz < 0.000001F;
        }
        exitRaceDialogVisible = false;
        raceSession.setPaused(false);
        clearRaceControls();
#ifdef RRR3D_AUDIO
        commentator.pause(false);
#endif
        previousFrameTicks = SDL_GetTicksNS();
        racePauseResumeObserved = true;
    };
    auto openExitRaceDialog = [&]() {
        exitRaceDialogVisible = true;
        exitRaceYesFocused = true;
        raceSession.setPaused(true);
        clearRaceControls();
        racePauseElapsedSnapshot = raceSession.elapsedSeconds();
        if (physicsWorld->vehicleCount() > 0U)
            racePausePositionSnapshot =
                physicsWorld->vehicle().body.position;
#ifdef RRR3D_AUDIO
        r3d::audio::PlayOptions acceptOptions;
        acceptOptions.bus = r3d::audio::Bus::Effects;
        audio.play(acceptanceAudio, acceptOptions, audioError);
        commentator.pause(true);
#endif
        racePauseDialogObserved = true;
    };
    auto leaveCurrentRace = [&]() {
        saveRaceProfile();
        raceSession.setPaused(false);
        exitRaceDialogVisible = false;
        inRace = false;
        clearRaceControls();
#ifdef RRR3D_AUDIO
        stopRaceAudio();
#endif
        previousFrameTicks = SDL_GetTicksNS();
        std::cout << "Original HudMenu accept: Race -> RaceMenu2\n";
    };
    auto replaceOptionsPage =
        [&](MenuPageVisual& page,
            std::vector<std::string> pageLabels) {
            auto replacement = createStyledPage(
                std::move(pageLabels), menu::smallFontHeight,
                optionsTextColor, menu::selectedTextColor);
            destroyPage(page);
            page = std::move(replacement);
            menuSelection =
                std::min(menuSelection, page.labels.size() - 1U);
        };
    auto originalCurrency = [](std::uint32_t value) {
        std::string result = std::to_string(value);
        for (std::ptrdiff_t index =
                 static_cast<std::ptrdiff_t>(result.size()) - 3;
             index > 0; index -= 3)
        {
            result.insert(static_cast<std::size_t>(index), ",");
        }
        return result;
    };
    auto wrapGarageInfo = [](std::string_view value) {
        constexpr std::size_t maximumCharacters = 78U;
        std::vector<std::string> lines;
        std::istringstream words{std::string(value)};
        std::string line;
        std::string word;
        while (words >> word)
        {
            if (!line.empty() &&
                line.size() + 1U + word.size() >
                    maximumCharacters)
            {
                lines.push_back(std::move(line));
                line.clear();
            }
            if (!line.empty())
                line.push_back(' ');
            line += word;
        }
        if (!line.empty())
            lines.push_back(std::move(line));
        if (lines.empty())
            lines.push_back(" ");
        if (lines.size() > 4U)
            lines.resize(4U);
        return lines;
    };
    auto rebuildGarageCarOrder = [&]() {
        garageCarOrder.clear();
        std::vector<GarageCarView> available;
        std::vector<GarageCarView> secret;
        std::vector<GarageCarView> locked;
        for (std::size_t index = 0U;
             index < originalGarage->cars.size(); ++index)
        {
            const auto& car = originalGarage->cars[index];
            const bool isSecret = std::none_of(
                originalGarage->carUnlocks.begin(),
                originalGarage->carUnlocks.end(),
                [&](const auto& rule) {
                    return rule.record == car.record;
                });
            if (championshipMode && isSecret)
                continue;
            const bool achievement =
                r3d::game::originalrace::
                    originalRecordAchievementUnlocked(
                        profileState, car.record);
            const bool unlocked =
                r3d::game::originalrace::originalCarUnlocked(
                    *originalGarage, profileState, car,
                    championshipMode);
            const GarageCarView view{index, !unlocked};
            if (isSecret && achievement)
                secret.push_back(view);
            else if (unlocked && achievement)
                available.push_back(view);
            else
                locked.push_back(view);
        }
        garageCarOrder.insert(
            garageCarOrder.end(), available.begin(), available.end());
        garageCarOrder.insert(
            garageCarOrder.end(), secret.begin(), secret.end());
        garageCarOrder.insert(
            garageCarOrder.end(), locked.begin(), locked.end());
        if (garageCarOrder.empty())
            return;
        const auto selected = std::find_if(
            garageCarOrder.begin(), garageCarOrder.end(),
            [&](const auto& view) {
                return originalGarage->cars[view.catalogIndex].record ==
                       profileState.player.currentCar;
            });
        garageViewIndex =
            selected == garageCarOrder.end()
                ? std::min(
                      garageViewIndex,
                      garageCarOrder.size() - 1U)
                : static_cast<std::size_t>(
                      std::distance(
                          garageCarOrder.begin(), selected));
        garageCarIndex =
            garageCarOrder[garageViewIndex].catalogIndex;
    };
    auto refreshGaragePage = [&]() {
        if (originalGarage->cars.empty())
            return;
        if (garageCarOrder.empty())
            rebuildGarageCarOrder();
        if (garageCarOrder.empty())
            return;
        garageViewIndex =
            std::min(garageViewIndex,
                     garageCarOrder.size() - 1U);
        garageCarIndex =
            garageCarOrder[garageViewIndex].catalogIndex;
        garageCarIndex =
            std::min(garageCarIndex,
                     originalGarage->cars.size() - 1U);
        const auto& car = originalGarage->cars[garageCarIndex];
        const bool locked =
            garageCarOrder[garageViewIndex].locked;
        auto replacement = createPage(
            {localized(
                 locked ? "svLockedCarName" : car.name),
             originalCurrency(profileState.player.money),
             locked ? "-" : originalCurrency(car.cost),
             localized("svBack"), localized("svBuy")});
        destroyPage(garagePage);
        garagePage = std::move(replacement);

        auto infoReplacement = createStyledPage(
            wrapGarageInfo(localized(
                locked ? "svLockedCarInfo" : car.info)),
            menu::smallFontHeight, raceTextColor,
            menu::selectedTextColor);
        destroyPage(garageInfoPage);
        garageInfoPage = std::move(infoReplacement);

        const auto stats =
            r3d::game::originalrace::originalGarageStats(
                *originalGarage, car);
        auto statValue = [](float value) {
            return std::to_string(
                static_cast<int>(std::lround(value)));
        };
        auto statsReplacement = createStyledPage(
            {locked
                 ? "0/0"
                 : statValue(stats.armor) + "/" +
                       statValue(stats.maximumArmor),
             locked
                 ? "0/0"
                 : statValue(stats.damage) + "/" +
                       statValue(stats.maximumDamage),
             locked
                 ? "0/300"
                 : statValue(stats.speedProgress * 300.0F) +
                       "/300"},
            menu::smallFontHeight, raceTextColor,
            menu::selectedTextColor);
        destroyPage(garageStatsPage);
        garageStatsPage = std::move(statsReplacement);

        std::string purchase = localized("svBuyCar");
        if (const auto marker = purchase.find("%s");
            marker != std::string::npos)
        {
            purchase.replace(
                marker, 2U, originalCurrency(car.cost));
        }
        auto purchaseReplacement = createStyledPage(
            {purchase}, menu::smallFontHeight,
            menu::normalTextColor, menu::selectedTextColor);
        destroyPage(garagePurchasePage);
        garagePurchasePage = std::move(purchaseReplacement);
    };
    auto refreshWorkshopPage = [&]() {
        workshopGoods.clear();
        for (const auto& item : originalGarage->workshop)
        {
            // Workshop::_items contains pass rewards.  The four mobility
            // families are exposed only by the installed-slot level button.
            if (item.type <= 4U ||
                !r3d::game::originalrace::
                    originalWorkshopItemUnlocked(
                        *originalGarage, profileState, item))
            {
                continue;
            }
            workshopGoods.push_back(&item);
        }
        std::stable_sort(
            workshopGoods.begin(), workshopGoods.end(),
            [](const auto* left, const auto* right) {
                return left->cost < right->cost;
            });
        const std::size_t rowCount =
            (workshopGoods.size() + 2U) / 3U;
        const std::size_t maximumScroll =
            rowCount > 4U ? rowCount - 4U : 0U;
        workshopGoodScroll =
            std::min(workshopGoodScroll, maximumScroll);

        auto pageReplacement = createStyledPage(
            {localized("svWorkshop")},
            menu::headerFontHeight, raceTextColor,
            menu::selectedTextColor);
        destroyPage(workshopPage);
        workshopPage = std::move(pageReplacement);
        auto controlsReplacement = createStyledPage(
            {localized("svBack"),
             originalCurrency(profileState.player.money)},
            menu::smallFontHeight, raceTextColor,
            menu::selectedTextColor);
        destroyPage(workshopControlsPage);
        workshopControlsPage = std::move(controlsReplacement);

        if (const auto* car = originalGarage->findCar(
                profileState.player.currentCar))
        {
            const auto stats =
                r3d::game::originalrace::originalGarageStats(
                    *originalGarage, *car, profileState.player);
            auto statValue = [](float value) {
                return std::to_string(
                    static_cast<int>(std::lround(value)));
            };
            auto statsReplacement = createStyledPage(
                {statValue(stats.damage) + "/" +
                     statValue(stats.maximumDamage),
                 statValue(stats.armor) + "/" +
                     statValue(stats.maximumArmor),
                 statValue(stats.speedProgress * 300.0F) +
                     "/300"},
                menu::smallFontHeight, raceTextColor,
                menu::selectedTextColor);
            destroyPage(workshopStatsPage);
            workshopStatsPage =
                std::move(statsReplacement);

            auto hintReplacement = createStyledPage(
                wrapGarageInfo(localized(car->info)),
                menu::smallFontHeight, raceTextColor,
                menu::selectedTextColor);
            destroyPage(workshopHintPage);
            workshopHintPage =
                std::move(hintReplacement);
        }

        const r3d::game::originalrace::OriginalWorkshopItem* selected =
            nullptr;
        constexpr std::size_t firstGoodFocus = 1U;
        constexpr std::size_t visibleGoods = 12U;
        constexpr std::size_t firstSlotFocus =
            firstGoodFocus + visibleGoods;
        if (menuSelection >= firstGoodFocus &&
            menuSelection < firstSlotFocus)
        {
            const std::size_t itemIndex =
                workshopGoodScroll * 3U +
                (menuSelection - firstGoodFocus);
            if (itemIndex < workshopGoods.size())
                selected = workshopGoods[itemIndex];
        }
        else if (
            menuSelection >= firstSlotFocus &&
            menuSelection <
                firstSlotFocus +
                    static_cast<std::size_t>(
                        r3d::game::originalrace::
                            GarageSlotType::Count))
        {
            const auto slotIndex =
                menuSelection - firstSlotFocus;
            selected = originalGarage->findItem(
                profileState.player.slots[slotIndex].record);
        }
        if (selected == nullptr && workshopDrag.active())
            selected =
                originalGarage->findItem(workshopDrag.item.record);

        std::vector<std::string> info;
        if (selected != nullptr)
        {
            info.push_back(localized(selected->name));
            auto description =
                wrapGarageInfo(localized(selected->info));
            info.insert(info.end(), description.begin(),
                        description.end());
            info.push_back(originalCurrency(selected->cost));
            if (selected->projectileDamage > 0.0F)
            {
                info.push_back(
                    std::to_string(static_cast<int>(std::lround(
                        selected->projectileDamage))));
            }
        }
        if (info.empty())
            info.push_back(" ");
        auto infoReplacement = createStyledPage(
            std::move(info), menu::smallFontHeight, raceTextColor,
            menu::selectedTextColor);
        destroyPage(workshopInfoPage);
        workshopInfoPage = std::move(infoReplacement);
    };
    auto workshopSlotCenters = [&]() {
        std::array<std::array<float, 2>,
                   static_cast<std::size_t>(
                       r3d::game::originalrace::
                           GarageSlotType::Count)>
            result{};
        const float scale = menu::virtualHeight / 720.0F;
        const float centerX = menu::virtualWidth * 0.63F;
        const float centerY = menu::virtualHeight * 0.451F;
        const float leftPanelRight =
            30.0F +
            static_cast<float>(workshopLeftPanelImage.width);
        const float leftOffset = std::max(
            centerX - 330.0F * scale, leftPanelRight + 80.0F);
        const float rightOffset = std::min(
            centerX + 330.0F * scale,
            menu::virtualWidth - 120.0F);
        const float topOffset = std::max(
            centerY - 180.0F * scale,
            static_cast<float>(workshopTopPanelImage.height) +
                65.0F);
        const float bottomOffset = std::min(
            centerY + 180.0F * scale,
            menu::virtualHeight -
                static_cast<float>(
                    workshopBottomPanelImage.height) -
                65.0F);
        const float slotWidth =
            static_cast<float>(workshopSlotImage.width);
        const float slotHeight =
            static_cast<float>(workshopSlotImage.height);
        constexpr float slotSpace = 15.0F;
        result = {{
            {leftOffset,
             centerY + slotHeight * 0.5F + slotSpace},
            {leftOffset,
             centerY - slotHeight * 0.5F - slotSpace},
            {centerX - slotWidth * 0.5F - 6.0F * slotSpace,
             bottomOffset},
            {centerX + slotWidth * 0.5F + 6.0F * slotSpace,
             bottomOffset},
            {rightOffset,
             centerY - slotHeight * 0.5F - slotSpace},
            {rightOffset,
             centerY + slotHeight * 0.5F + slotSpace},
            {centerX -
                 (slotWidth + 4.5F * slotSpace) * 1.5F,
             topOffset},
            {centerX -
                 (slotWidth + 4.5F * slotSpace) * 0.5F,
             topOffset},
            {centerX +
                 (slotWidth + 4.5F * slotSpace) * 0.5F,
             topOffset},
            {centerX +
                 (slotWidth + 4.5F * slotSpace) * 1.5F,
             topOffset},
        }};
        return result;
    };
    auto workshopGoodCenters = [&]() {
        std::array<std::array<float, 2>, 12> result{};
        const float panelCenterY =
            (static_cast<float>(workshopTopPanelImage.height) -
                 30.0F +
             menu::virtualHeight -
             static_cast<float>(
                 workshopBottomPanelImage.height)) *
            0.5F;
        const float panelTop =
            panelCenterY -
            static_cast<float>(workshopLeftPanelImage.height) *
                0.5F;
        constexpr float cell = 100.0F;
        const float firstX = 30.0F + 22.0F + 39.0F;
        const float firstY = panelTop + 11.0F + 38.0F;
        for (std::size_t index = 0U; index < result.size(); ++index)
        {
            result[index] = {
                firstX + static_cast<float>(index % 3U) * cell,
                firstY + static_cast<float>(index / 3U) * cell};
        }
        return result;
    };
    auto workshopSlotAccepts =
        [&](std::size_t slotIndex,
            const r3d::game::originalrace::ProfileSlot& item) {
            const auto* car = originalGarage->findCar(
                profileState.player.currentCar);
            if (car == nullptr ||
                slotIndex >= car->placements.size())
                return false;
            const auto& placement = car->placements[slotIndex];
            if (!placement.active || placement.locked)
                return false;
            if (!championshipMode &&
                slotIndex >=
                    r3d::game::originalrace::PlayerProfile::
                        firstWeaponSlot &&
                slotIndex -
                        r3d::game::originalrace::PlayerProfile::
                            firstWeaponSlot >=
                    profileState.config.weaponMaxLevel)
            {
                return false;
            }
            return std::find(
                       placement.supportedItems.begin(),
                       placement.supportedItems.end(),
                       item.record) !=
                   placement.supportedItems.end();
        };
    auto showWorkshopConfirmation =
        [&](WorkshopConfirmation confirmation,
            const r3d::game::originalrace::
                OriginalWorkshopItem& item,
            std::uint32_t value) {
            std::string message = localized(
                confirmation == WorkshopConfirmation::Buy
                    ? "svBuyWeapon"
                    : "svSellWeapon");
            if (const auto marker = message.find("%s");
                marker != std::string::npos)
            {
                message.replace(
                    marker, 2U, originalCurrency(value));
            }
            auto replacement = createStyledPage(
                {message}, menu::smallFontHeight,
                menu::normalTextColor,
                menu::selectedTextColor);
            destroyPage(workshopConfirmationPage);
            workshopConfirmationPage =
                std::move(replacement);
            workshopConfirmation = confirmation;
            workshopPendingPurchase =
                confirmation == WorkshopConfirmation::Buy
                    ? &item
                    : nullptr;
            workshopConfirmationYesFocused = true;
        };
    auto buyWorkshopGood =
        [&](const r3d::game::originalrace::
                OriginalWorkshopItem& item) {
            r3d::game::originalrace::ProfileSlot purchased;
            std::string workshopError;
            if (!r3d::game::originalrace::
                    buyOriginalWorkshopItem(
                        *originalGarage, profileState, item,
                        championshipMode, purchased,
                        workshopError))
            {
                std::cerr
                    << "Original WorkshopFrame buy: "
                    << workshopError << '\n';
                return false;
            }
            workshopDrag.item = std::move(purchased);
            workshopDrag.origin.reset();
            saveRaceProfile();
            refreshWorkshopPage();
            return true;
        };
    auto stopWorkshopDrag =
        [&](bool intoGoods, bool accepted = false) {
        if (!workshopDrag.active())
            return true;
        std::string workshopError;
        if (!intoGoods && workshopDrag.origin)
        {
            const auto originIndex =
                static_cast<std::size_t>(*workshopDrag.origin);
            if (originIndex < profileState.player.slots.size() &&
                profileState.player.slots[originIndex].record.empty())
            {
                r3d::game::originalrace::ProfileSlot replaced;
                if (!r3d::game::originalrace::
                        installOriginalWorkshopSlot(
                            *originalGarage, profileState,
                            *workshopDrag.origin, workshopDrag.item,
                            replaced, workshopError))
                {
                    std::cerr
                        << "Original WorkshopFrame restore: "
                        << workshopError << '\n';
                    return false;
                }
                workshopDrag = {};
                saveRaceProfile();
                refreshWorkshopPage();
                return true;
            }
        }
        const bool discount = workshopDrag.origin.has_value();
        if (championshipMode && discount && !accepted)
        {
            const auto* item = originalGarage->findItem(
                workshopDrag.item.record);
            if (item == nullptr)
                return false;
            showWorkshopConfirmation(
                WorkshopConfirmation::Sell, *item,
                r3d::game::originalrace::
                    originalWorkshopSellValue(
                        *originalGarage,
                        workshopDrag.item, true));
            return false;
        }
        if (!r3d::game::originalrace::sellOriginalWorkshopItem(
                *originalGarage, profileState, workshopDrag.item,
                discount, championshipMode, workshopError))
        {
            std::cerr << "Original WorkshopFrame sell: "
                      << workshopError << '\n';
            return false;
        }
        workshopDrag = {};
        saveRaceProfile();
        refreshWorkshopPage();
        return true;
    };
    auto activateWorkshopFocus =
        [&](bool pointerSlotPlane) {
            constexpr std::size_t firstGoodFocus = 1U;
            constexpr std::size_t visibleGoods = 12U;
            constexpr std::size_t firstSlotFocus =
                firstGoodFocus + visibleGoods;
            if (menuSelection == 0U)
            {
                if (workshopDrag.active())
                    stopWorkshopDrag(false);
                else
                    backMenu();
                return;
            }
            if (menuSelection < firstSlotFocus)
            {
                if (workshopDrag.active())
                {
                    stopWorkshopDrag(true);
                    return;
                }
                const std::size_t itemIndex =
                    workshopGoodScroll * 3U +
                    menuSelection - firstGoodFocus;
                if (itemIndex >= workshopGoods.size())
                    return;
                const auto* item = workshopGoods[itemIndex];
                const bool hasCompatibleSlot = [&]() {
                    for (std::size_t slot = 0U;
                         slot < profileState.player.slots.size();
                         ++slot)
                    {
                        if (workshopSlotAccepts(
                                slot,
                                r3d::game::originalrace::ProfileSlot{
                                    item->record, item->defaultCharge,
                                    item->maximumCharge > 0U}))
                            return true;
                    }
                    return false;
                }();
                if (!hasCompatibleSlot)
                {
                    std::cerr
                        << "Original WorkshopFrame: "
                        << localized("svHintWeaponNotSupport")
                        << '\n';
                    return;
                }
                if (championshipMode)
                {
                    showWorkshopConfirmation(
                        WorkshopConfirmation::Buy, *item,
                        item->cost);
                    return;
                }
                buyWorkshopGood(*item);
                return;
            }
            const std::size_t slotIndex =
                menuSelection - firstSlotFocus;
            if (slotIndex >= profileState.player.slots.size())
                return;
            const auto slotType =
                static_cast<r3d::game::originalrace::
                                GarageSlotType>(slotIndex);
            if (workshopDrag.active())
            {
                if (!workshopSlotAccepts(
                        slotIndex, workshopDrag.item))
                    return;
                r3d::game::originalrace::ProfileSlot replaced;
                std::string workshopError;
                if (!r3d::game::originalrace::
                        installOriginalWorkshopSlot(
                            *originalGarage, profileState, slotType,
                            workshopDrag.item, replaced,
                            workshopError))
                {
                    std::cerr
                        << "Original WorkshopFrame install: "
                        << workshopError << '\n';
                    return;
                }
                if (replaced.record.empty())
                    workshopDrag = {};
                else
                {
                    workshopDrag.item = std::move(replaced);
                    workshopDrag.origin = slotType;
                }
                saveRaceProfile();
                refreshWorkshopPage();
                return;
            }

            auto& installed = profileState.player.slots[slotIndex];
            const auto* item =
                originalGarage->findItem(installed.record);
            if (item == nullptr)
                return;
            if (pointerSlotPlane)
            {
                workshopDrag.item = installed;
                workshopDrag.origin = slotType;
                installed = {};
                saveRaceProfile();
                refreshWorkshopPage();
                return;
            }
            std::string workshopError;
            bool changed = false;
            if (item->maximumCharge > 0U)
            {
                changed = r3d::game::originalrace::
                    rechargeOriginalWorkshopItem(
                        *originalGarage, profileState, slotType,
                        championshipMode, workshopError);
            }
            else if (slotIndex < 4U)
            {
                const auto* car = originalGarage->findCar(
                    profileState.player.currentCar);
                const int level =
                    r3d::game::originalrace::
                        originalWorkshopUpgradeLevel(
                            installed.record, slotType);
                const auto* upgrade =
                    car == nullptr
                        ? nullptr
                        : r3d::game::originalrace::
                              originalWorkshopUpgradeItem(
                                  *originalGarage, *car, slotType,
                                  level + 1);
                if (upgrade != nullptr)
                {
                    changed = r3d::game::originalrace::
                        installOriginalWorkshopItem(
                            *originalGarage, profileState, slotType,
                            *upgrade, championshipMode,
                            workshopError);
                }
            }
            if (!changed && !workshopError.empty())
                std::cerr << "Original WorkshopFrame upgrade: "
                          << workshopError << '\n';
            if (changed)
                saveRaceProfile();
            refreshWorkshopPage();
        };
    auto refreshPlanetsPage = [&]() {
        std::vector<std::string> output;
        const auto count = std::min(
            originalGarage->planets.size(),
            profileState.player.planets.size());
        output.reserve(count + 1U);
        const std::size_t nextPlanet =
            std::min<std::size_t>(
                profileState.player.currentPlanet + 1U,
                count);
        for (std::size_t index = 0; index < count; ++index)
        {
            const auto& progress =
                profileState.player.planets[index];
            std::string state;
            if (progress.state == 0U ||
                (racePlanetChampion && index == nextPlanet))
            {
                state = localized("svOpen");
            }
            else if (progress.state == 1U)
            {
                const auto& points =
                    originalGarage->planets[index].requestPoints;
                const auto pass =
                    std::max<std::uint32_t>(progress.pass, 1U);
                const auto required =
                    pass <= points.size() ? points[pass - 1U] : 0U;
                state = localized("svRequestPoints");
                const auto marker = state.find("%d");
                if (marker != std::string::npos)
                {
                    state.replace(
                        marker, 2U, std::to_string(required));
                }
            }
            else if (progress.state == 2U)
            {
                state =
                    championshipMode && index == nextPlanet
                        ? localized("svUnavailableTitulA")
                        : localized("svClosed");
            }
            else
            {
                state = localized("svCompleted");
            }
            output.push_back(std::move(state));
        }
        output.push_back(localized("svBack"));
        auto replacement = createStyledPage(
            std::move(output), menu::smallFontHeight,
            menu::Rgba8{255, 255, 255, 255},
            menu::selectedTextColor);
        destroyPage(planetsPage);
        planetsPage = std::move(replacement);
        menuSelection = std::min(
            menuSelection, planetsPage.labels.size() - 1U);

        std::vector<std::string> info{" ", " ", " "};
        if (angarPlanetIndex >= 0 &&
            static_cast<std::size_t>(angarPlanetIndex) < count)
        {
            const auto& planet = originalGarage->planets[
                static_cast<std::size_t>(angarPlanetIndex)];
            info = {localized(planet.name),
                    localized(planet.bossName)};
            std::istringstream words(localized(planet.info));
            std::string line;
            std::string word;
            while (words >> word)
            {
                if (!line.empty() &&
                    line.size() + word.size() + 1U > 34U)
                {
                    info.push_back(std::move(line));
                    line.clear();
                }
                if (!line.empty())
                    line.push_back(' ');
                line += word;
            }
            if (!line.empty())
                info.push_back(std::move(line));
            if (info.size() > 10U)
                info.resize(10U);
        }
        auto infoReplacement = createStyledPage(
            std::move(info), menu::smallFontHeight,
            menu::Rgba8{118, 206, 242, 255},
            menu::selectedTextColor);
        destroyPage(angarInfoPage);
        angarInfoPage = std::move(infoReplacement);
    };
    auto selectAngarPlanet = [&](int index) {
        if (index == angarPlanetIndex)
            return;
        angarPreviousPlanetIndex = angarPlanetIndex;
        angarPlanetIndex = index;
        angarDoorTime = 0.0F;
        refreshPlanetsPage();
    };
    auto persistAngarProfile = [&]() {
        std::string profileError;
        if (!profileStore.save(profileState, profileError))
        {
            std::cerr << "Unable to save AngarFrame planet: "
                      << profileError << '\n';
        }
    };
    auto changeAngarPlanet = [&](std::size_t index) {
        const auto count = std::min(
            originalGarage->planets.size(),
            profileState.player.planets.size());
        if (index >= count)
            return;
        auto& progress = profileState.player.planets[index];
        if (progress.state == 1U || progress.state == 2U)
        {
            // Planet::Unlock followed by Tournament::ChangePlanet/Open.
            progress.state = 0U;
            progress.pass = 1U;
        }
        profileState.player.currentPlanet =
            static_cast<std::uint32_t>(index);
        profileState.player.currentPass =
            std::max<std::uint32_t>(progress.pass, 1U);
        profileState.player.currentTrack = 0U;
        selectedTrack =
            r3d::game::originalrace::resolveOriginalTournamentTrack(
                *originalRace, profileState.player);
        racePlanetChampion = false;
        persistAngarProfile();
        angarTravelDialogVisible = false;
        backMenu();
    };
    auto requestAngarTravel = [&](std::size_t index) {
        angarTravelTarget = index;
        angarTravelYesFocused = true;
        const auto key =
            index == profileState.player.currentPlanet
                ? "svYouReadyStayPlanet"
                : "svYouReadyFlyPlanet";
        auto replacement = createText(
            *device, localized(key), menu::smallFontHeight,
            false, menu::normalTextColor, resolvedFont);
        device->destroy(angarTravelMessage.texture);
        angarTravelMessage = replacement;
        angarTravelDialogVisible = true;
    };
    auto refreshAchievementsPage = [&]() {
        std::vector<std::string> prices;
        prices.reserve(originalAchievementVisuals.size());
        for (const auto& visual : originalAchievementVisuals)
        {
            const auto item =
                profileState.achievementItems.find(
                    std::string(visual.name));
            if (item == profileState.achievementItems.end())
            {
                prices.emplace_back(" ");
                continue;
            }
            const auto state = item->second.values.find("state");
            const auto price = item->second.values.find("price");
            prices.push_back(
                state != item->second.values.end() &&
                        state->second == "asUnlocked" &&
                        price != item->second.values.end()
                    ? originalCurrency(
                          static_cast<std::uint32_t>(
                              std::max(
                                  std::strtol(
                                      price->second.c_str(), nullptr, 10),
                                  0L)))
                    : " ");
        }
        auto replacement = createStyledPage(
            std::move(prices), menu::smallFontHeight,
            menu::Rgba8{195, 194, 192, 255},
            menu::Rgba8{195, 194, 192, 255});
        destroyPage(achievementPricePage);
        achievementPricePage = std::move(replacement);
        auto pointsReplacement = createText(
            *device,
            localized("svPoints") + " " +
                originalCurrency(profileState.achievementPoints),
            menu::headerFontHeight, false,
            menu::Rgba8{250, 88, 0, 255}, resolvedFont);
        device->destroy(achievementPoints.texture);
        achievementPoints = pointsReplacement;
        menuSelection =
            std::min(menuSelection, originalAchievementBack);
    };
    auto achievementState =
        [&](std::size_t index) -> std::string_view {
            if (index >= originalAchievementVisuals.size())
                return {};
            const auto item =
                profileState.achievementItems.find(
                    std::string(
                        originalAchievementVisuals[index].name));
            if (item == profileState.achievementItems.end())
                return {};
            const auto state = item->second.values.find("state");
            return state == item->second.values.end()
                       ? std::string_view{}
                       : std::string_view(state->second);
        };
    auto achievementPrice = [&](std::size_t index) {
        std::uint32_t price = 0U;
        if (index >= originalAchievementVisuals.size())
            return price;
        const auto item =
            profileState.achievementItems.find(
                std::string(originalAchievementVisuals[index].name));
        if (item == profileState.achievementItems.end())
            return price;
        const auto value = item->second.values.find("price");
        if (value == item->second.values.end())
            return price;
        const auto parsed = std::from_chars(
            value->second.data(),
            value->second.data() + value->second.size(), price);
        return parsed.ec == std::errc{} ? price : 0U;
    };
    auto achievementFocusable = [&](std::size_t index) {
        return index == originalAchievementBack ||
               achievementState(index) == "asUnlocked";
    };
    auto moveAchievementFocus = [&](std::size_t direction) {
        if (direction >= 4U ||
            menuSelection > originalAchievementBack)
            return;
        std::size_t next =
            originalAchievementNavigation[menuSelection][direction];
        for (std::size_t attempts = 0U;
             attempts < originalAchievementNavigation.size();
             ++attempts)
        {
            if (next == originalAchievementNoTarget)
                return;
            if (achievementFocusable(next))
            {
                menuSelection = next;
                return;
            }
            next = originalAchievementNavigation[next][direction];
        }
    };
    auto refreshRaceMainInfoPage = [&]() {
        auto replacement = createStyledPage(
            raceMainInfoLabels(), menu::smallFontHeight,
            raceInfoColor, menu::selectedTextColor);
        destroyPage(raceMainInfoPage);
        raceMainInfoPage = std::move(replacement);
    };
    auto showOriginalRaceMenu = [&]() {
        refreshRaceMainInfoPage();
        menuStack.push_back(MenuScreen::RaceMenu);
        menuSelection = 0;
    };
    auto refreshCurrentOptionsPage = [&]() {
        switch (menuStack.back())
        {
        case MenuScreen::GameOptions:
            replaceOptionsPage(
                gameOptionsPage, gameOptionsLabels());
            break;
        case MenuScreen::GraphicsOptions:
            replaceOptionsPage(
                graphicsOptionsPage, graphicsOptionsLabels());
            break;
        case MenuScreen::SoundOptions:
            replaceOptionsPage(
                soundOptionsPage, soundOptionsLabels());
            break;
        case MenuScreen::ControlsOptions:
            replaceOptionsPage(
                controlsOptionsPage, controlsOptionsLabels());
            replaceOptionsPage(
                controlsKeyboardValuesPage,
                controlValues(false));
            replaceOptionsPage(
                controlsGamepadValuesPage,
                controlValues(true));
            break;
        default:
            break;
        }
    };
    auto isOriginalOptionsScreen = [](MenuScreen screen) {
        return screen == MenuScreen::GameOptions ||
               screen == MenuScreen::GraphicsOptions ||
               screen == MenuScreen::SoundOptions ||
               screen == MenuScreen::ControlsOptions;
    };
    auto optionsStateIndex = [](MenuScreen screen) {
        switch (screen)
        {
        case MenuScreen::GraphicsOptions:
            return 1U;
        case MenuScreen::SoundOptions:
            return 2U;
        case MenuScreen::ControlsOptions:
            return 3U;
        default:
            return 0U;
        }
    };
    auto setOptionsState = [&](std::size_t state) {
        menuStack.back() =
            std::array{
                MenuScreen::GameOptions,
                MenuScreen::GraphicsOptions,
                MenuScreen::SoundOptions,
                MenuScreen::ControlsOptions}
                [std::min<std::size_t>(state, 3U)];
        menuSelection = 0U;
        bindingCaptureAction.reset();
        refreshCurrentOptionsPage();
    };
    auto beginOriginalOptions = [&]() {
        optionsDraftConfig = profileState.config;
        optionsDraftDifficulty = profileState.player.difficulty;
        controlsUseGamepad = false;
        bindingCaptureAction.reset();
        pushMenu(MenuScreen::GameOptions);
        refreshCurrentOptionsPage();
    };
    auto cancelOriginalOptions = [&]() {
#ifdef RRR3D_AUDIO
        audio.setBusVolume(
            r3d::audio::Bus::Music,
            profileState.config.musicVolume);
        audio.setBusVolume(
            r3d::audio::Bus::Effects,
            profileState.config.effectsVolume);
        audio.setBusVolume(
            r3d::audio::Bus::Voice,
            profileState.config.voiceVolume);
#endif
        optionsDraftConfig = profileState.config;
        optionsDraftDifficulty = profileState.player.difficulty;
        bindingCaptureAction.reset();
        backMenu();
    };
    auto applyOriginalOptions = [&]() {
        const auto previousConfig = profileState.config;
        profileState.config = optionsDraftConfig;
        profileState.player.difficulty = optionsDraftDifficulty;

        if (profileState.config.fullScreen !=
            previousConfig.fullScreen)
        {
            if (!SDL_SetWindowFullscreen(
                    window, profileState.config.fullScreen))
            {
                std::cerr
                    << "Unable to apply original window mode: "
                    << SDL_GetError() << '\n';
                profileState.config.fullScreen =
                    previousConfig.fullScreen;
                optionsDraftConfig.fullScreen =
                    previousConfig.fullScreen;
            }
        }
        if (!profileState.config.fullScreen &&
            (profileState.config.resolutionWidth !=
                 previousConfig.resolutionWidth ||
             profileState.config.resolutionHeight !=
                 previousConfig.resolutionHeight))
        {
            SDL_SetWindowSize(
                window,
                static_cast<int>(
                    profileState.config.resolutionWidth),
                static_cast<int>(
                    profileState.config.resolutionHeight));
        }
        raceRenderer.resetCamera();
        raceSession.setSpringBorders(
            profileState.config.springBorders);
        raceSession.setEnableMineBug(
            profileState.config.enableMineBug);
#ifdef RRR3D_AUDIO
        audio.setBusVolume(
            r3d::audio::Bus::Music,
            profileState.config.musicVolume);
        audio.setBusVolume(
            r3d::audio::Bus::Effects,
            profileState.config.effectsVolume);
        audio.setBusVolume(
            r3d::audio::Bus::Voice,
            profileState.config.voiceVolume);
        if (profileState.config.commentatorStyle !=
            previousConfig.commentatorStyle)
        {
            commentator.shutdown();
            if (!commentator.initialize(
                    profileState.config.commentatorStyle,
                    audioError))
            {
                std::cerr
                    << "Unable to switch original commentator: "
                    << audioError << '\n';
            }
        }
#endif
#ifdef RRR3D_GAMEPAD_INPUT
        input.applyKeyboardBindings(
            profileState.config.keyboardControls);
        input.applyGamepadBindings(
            profileState.config.gamepadControls);
#endif
        saveRaceProfile();
        bindingCaptureAction.reset();
        backMenu();
    };
    auto cycleValue = [](std::uint32_t value,
                         std::uint32_t count, int direction) {
        if (count == 0U)
            return 0U;
        const int normalized =
            (static_cast<int>(value % count) + direction +
             static_cast<int>(count)) %
            static_cast<int>(count);
        return static_cast<std::uint32_t>(normalized);
    };
    auto adjustCurrentOption = [&](int direction) {
        direction = direction < 0 ? -1 : 1;
        switch (menuStack.back())
        {
        case MenuScreen::GameOptions:
            switch (menuSelection)
            {
            case 0:
                optionsDraftConfig.preferredCamera =
                    optionsDraftConfig.preferredCamera ==
                            r3d::game::originalrace::
                                PreferredCamera::ThirdPerson
                        ? r3d::game::originalrace::
                              PreferredCamera::Isometric
                        : r3d::game::originalrace::
                              PreferredCamera::ThirdPerson;
                break;
            case 1:
                optionsDraftConfig.cameraDistance =
                    std::clamp(
                        optionsDraftConfig.cameraDistance +
                            static_cast<float>(direction) * 0.25F,
                        1.0F, 2.0F);
                break;
            case 2:
                optionsDraftConfig.enableHud =
                    !optionsDraftConfig.enableHud;
                break;
            case 3: {
                static constexpr std::array<std::string_view, 3>
                    difficulties{
                        "gdEasy", "gdNormal", "gdHard"};
                const auto found = std::find(
                    difficulties.begin(), difficulties.end(),
                    optionsDraftDifficulty);
                const auto index =
                    found == difficulties.end()
                        ? 1U
                        : static_cast<std::uint32_t>(
                              found - difficulties.begin());
                optionsDraftDifficulty =
                    difficulties[cycleValue(index, 3U, direction)];
                break;
            }
            case 4:
                optionsDraftConfig.springBorders =
                    !optionsDraftConfig.springBorders;
                break;
            case 5:
                optionsDraftConfig.upgradeMaxLevel =
                    cycleValue(
                        optionsDraftConfig.upgradeMaxLevel,
                        3U, direction);
                break;
            case 6:
                optionsDraftConfig.weaponMaxLevel =
                    cycleValue(
                        std::clamp(
                            optionsDraftConfig.weaponMaxLevel,
                            1U, 4U) -
                            1U,
                        4U, direction) +
                    1U;
                break;
            case 7:
                optionsDraftConfig.maxPlayers =
                    cycleValue(
                        std::clamp(
                            optionsDraftConfig.maxPlayers,
                            2U, 6U) -
                            2U,
                        5U, direction) +
                    2U;
                break;
            case 8:
                optionsDraftConfig.maxComputers =
                    cycleValue(
                        optionsDraftConfig.maxComputers,
                        6U, direction);
                break;
            case 9:
                optionsDraftConfig.lapsCount =
                    std::clamp(
                        static_cast<int>(
                            optionsDraftConfig.lapsCount) +
                            direction,
                        1, 8);
                break;
            case 10:
                optionsDraftConfig.enableMineBug =
                    !optionsDraftConfig.enableMineBug;
                break;
            case 11:
                optionsDraftConfig.disableVideo =
                    !optionsDraftConfig.disableVideo;
                break;
            default:
                return;
            }
            break;
        case MenuScreen::GraphicsOptions:
            switch (menuSelection)
            {
            case 0: {
                const auto current = std::find(
                    originalDisplayModes.begin(),
                    originalDisplayModes.end(),
                    std::pair{
                        optionsDraftConfig.resolutionWidth,
                        optionsDraftConfig.resolutionHeight});
                const auto index =
                    current == originalDisplayModes.end()
                        ? 0U
                        : static_cast<std::uint32_t>(
                              current - originalDisplayModes.begin());
                const auto& mode = originalDisplayModes[cycleValue(
                    index,
                    static_cast<std::uint32_t>(
                        originalDisplayModes.size()),
                    direction)];
                optionsDraftConfig.resolutionWidth = mode.first;
                optionsDraftConfig.resolutionHeight = mode.second;
                break;
            }
            case 1:
                optionsDraftConfig.quality.filtering =
                    cycleValue(
                        optionsDraftConfig.quality.filtering,
                        4U, direction);
                break;
            case 2:
                optionsDraftConfig.quality.msaa =
                    cycleValue(
                        optionsDraftConfig.quality.msaa,
                        3U, direction);
                break;
            case 3:
                optionsDraftConfig.quality.shadow =
                    cycleValue(
                        optionsDraftConfig.quality.shadow,
                        3U, direction);
                break;
            case 4:
                optionsDraftConfig.quality.environment =
                    cycleValue(
                        optionsDraftConfig.quality.environment,
                        3U, direction);
                break;
            case 5:
                optionsDraftConfig.quality.light =
                    cycleValue(
                        optionsDraftConfig.quality.light,
                        3U, direction);
                break;
            case 6:
                optionsDraftConfig.quality.postEffect =
                    cycleValue(
                        optionsDraftConfig.quality.postEffect,
                        3U, direction);
                break;
            case 7:
                optionsDraftConfig.fullScreen =
                    !optionsDraftConfig.fullScreen;
                break;
            default:
                return;
            }
            break;
        case MenuScreen::SoundOptions:
            switch (menuSelection)
            {
            case 0:
                optionsDraftConfig.language =
                    optionsDraftConfig.language == "russian"
                        ? "english"
                        : "russian";
                break;
            case 1:
                optionsDraftConfig.commentatorStyle =
                    optionsDraftConfig.commentatorStyle == "russian"
                        ? "english"
                        : "russian";
                break;
            case 2:
                optionsDraftConfig.musicVolume =
                    std::clamp(
                        optionsDraftConfig.musicVolume +
                            static_cast<float>(direction) * 0.1F,
                        0.0F, 2.0F);
#ifdef RRR3D_AUDIO
                audio.setBusVolume(
                    r3d::audio::Bus::Music,
                    optionsDraftConfig.musicVolume);
#endif
                break;
            case 3:
                optionsDraftConfig.effectsVolume =
                    std::clamp(
                        optionsDraftConfig.effectsVolume +
                            static_cast<float>(direction) * 0.1F,
                        0.0F, 2.0F);
#ifdef RRR3D_AUDIO
                audio.setBusVolume(
                    r3d::audio::Bus::Effects,
                    optionsDraftConfig.effectsVolume);
#endif
                break;
            case 4:
                optionsDraftConfig.voiceVolume =
                    std::clamp(
                        optionsDraftConfig.voiceVolume +
                            static_cast<float>(direction) * 0.1F,
                        0.0F, 2.0F);
#ifdef RRR3D_AUDIO
                audio.setBusVolume(
                    r3d::audio::Bus::Voice,
                    optionsDraftConfig.voiceVolume);
#endif
                break;
            default:
                return;
            }
            break;
        default:
            return;
        }
        refreshCurrentOptionsPage();
    };
    auto showFinishMenu = [&]() {
        if (finishMenuShown || raceSession.racers().empty())
            return;
        finishMenuShown = true;
        saveRaceProfile();
#ifdef RRR3D_AUDIO
        stopRaceAudio();
#endif
        std::vector<std::size_t> order(
            raceSession.racers().size());
        for (std::size_t index = 0; index < order.size(); ++index)
            order[index] = index;
        std::stable_sort(
            order.begin(), order.end(),
            [&](std::size_t first, std::size_t second) {
                return raceSession.racers()[first].place <
                       raceSession.racers()[second].place;
            });
        std::ostringstream summary;
        summary << localized("svFinish") << '\n';
        for (const auto racer : order)
        {
            const auto& result = raceSession.racers()[racer];
            const float time =
                result.finishTime >= 0.0F
                    ? result.finishTime
                    : raceSession.elapsedSeconds();
            const auto minutes =
                static_cast<unsigned>(time) / 60U;
            const float seconds =
                time - static_cast<float>(minutes * 60U);
            summary << result.place << ". "
                    << originalRace->racers[racer].name << "  "
                    << minutes << ':' << std::fixed
                    << std::setprecision(2) << std::setw(5)
                    << std::setfill('0') << seconds << '\n';
        }
        const auto& player = raceSession.racers().front();
        summary << localized("svMoney") << ": +"
                << player.rewardMoney + player.pickedMoney << "   "
                << localized("svPoints") << ": +"
                << player.rewardPoints;
        try
        {
            auto replacement = createText(
                *device, summary.str(), menu::smallFontHeight,
                false, menu::normalTextColor, resolvedFont);
            device->destroy(finishSummary.texture);
            finishSummary = replacement;
        }
        catch (const std::exception& exception)
        {
            std::cerr << "Unable to create FinishMenu results: "
                      << exception.what() << '\n';
        }
        inRace = false;
        raceInput = {};
        raceUseWeaponRequested = false;
        raceUseAllWeaponsRequested = false;
        raceUseMine = false;
        raceUseHyper = false;
        raceChangeWeaponRequested = false;
        raceWeaponChangeDirection = 1;
        raceFireWeaponSlotRequested = -1;
        raceResetRequested = false;
        menuStack = {MenuScreen::Main, MenuScreen::Finish};
        menuSelection = 0;
        std::cout << "Original FinishMenu: place "
                  << player.place << ", money +"
                  << player.rewardMoney + player.pickedMoney
                  << ", points +" << player.rewardPoints << '\n';
    };
#endif
#ifdef RRR3D_AUDIO
    bool integratedAudioInputObserved = !options->audioSmokeTest;
    enum class MusicSmokePhase
    {
        WaitingForDecode,
        WaitingWhilePaused,
        WaitingForResume,
        WaitingForAutomaticNext,
        WaitingForManualNext,
        Complete
    };
    MusicSmokePhase musicSmokePhase = MusicSmokePhase::WaitingForDecode;
    std::array<std::size_t, 3> smokeTrackOrder{};
    std::uint64_t smokePausePosition = 0;
    std::uint64_t smokePhaseTicks = SDL_GetTicks();
    const std::uint64_t musicSmokeDeadline = SDL_GetTicks() + 30000;
#endif
#if defined(RRR3D_AUDIO) && defined(RRR3D_GAMEPAD_INPUT)
    if (options->audioSmokeTest)
    {
        SDL_Event down{};
        down.key.type = SDL_EVENT_KEY_DOWN;
        down.key.down = true;
        down.key.scancode = SDL_SCANCODE_DOWN;
        SDL_Event confirm{};
        confirm.key.type = SDL_EVENT_KEY_DOWN;
        confirm.key.down = true;
        confirm.key.scancode = SDL_SCANCODE_RETURN;
        if (!SDL_PushEvent(&down) || !SDL_PushEvent(&confirm))
        {
            std::cerr << "Unable to queue integrated M8 menu/audio events: "
                      << SDL_GetError() << '\n';
            runtimeSmokeFailed = true;
        }
    }
#endif
    while (running)
    {
#if defined(RRR3D_PHYSICS) && defined(RRR3D_GAMEPAD_INPUT)
        // Do not enqueue all confirms before the event loop.  SDL's input
        // layer intentionally suppresses repeats while a key is held, and
        // the old batch therefore never exercised Main -> GameMode ->
        // Tournament -> Continue -> RaceMenu -> WorkshopFrame ->
        // GarageFrame -> AngarFrame -> AchievmentFrame -> Race.
        // Advance one real press/release pair per rendered menu frame.
        if (options->raceRenderSmokeTest && !inRace &&
            raceSmokeMenuStep < 24U &&
            renderedFrames >= raceSmokeNextMenuFrame)
        {
            constexpr std::array<SDL_Scancode, 24> smokeKeys{
                SDL_SCANCODE_RETURN, SDL_SCANCODE_RETURN,
                SDL_SCANCODE_RETURN, SDL_SCANCODE_RIGHT,
                SDL_SCANCODE_RETURN, SDL_SCANCODE_ESCAPE,
                SDL_SCANCODE_RIGHT, SDL_SCANCODE_RIGHT,
                SDL_SCANCODE_RETURN, SDL_SCANCODE_RIGHT,
                SDL_SCANCODE_ESCAPE, SDL_SCANCODE_RIGHT,
                SDL_SCANCODE_RIGHT, SDL_SCANCODE_RIGHT,
                SDL_SCANCODE_RETURN, SDL_SCANCODE_LEFT,
                SDL_SCANCODE_ESCAPE, SDL_SCANCODE_RIGHT,
                SDL_SCANCODE_RIGHT, SDL_SCANCODE_RIGHT,
                SDL_SCANCODE_RIGHT, SDL_SCANCODE_RETURN,
                SDL_SCANCODE_ESCAPE,
                SDL_SCANCODE_RETURN};
            SDL_Event press{};
            press.key.type = SDL_EVENT_KEY_DOWN;
            press.key.down = true;
            press.key.scancode = smokeKeys[raceSmokeMenuStep];
            SDL_Event release = press;
            release.key.type = SDL_EVENT_KEY_UP;
            release.key.down = false;
            if (!SDL_PushEvent(&press) ||
                !SDL_PushEvent(&release))
            {
                std::cerr << "Unable to queue integrated M9 menu step "
                          << raceSmokeMenuStep << ": "
                          << SDL_GetError() << '\n';
                runtimeSmokeFailed = true;
            }
            ++raceSmokeMenuStep;
            raceSmokeNextMenuFrame = renderedFrames + 1U;
        }
#endif
#ifdef RRR3D_PHYSICS
        if (options->raceRenderSmokeTest && inRace &&
            !raceSmokeAccelerateQueued)
        {
            SDL_Event accelerate{};
            accelerate.key.type = SDL_EVENT_KEY_DOWN;
            accelerate.key.down = true;
            accelerate.key.scancode = SDL_SCANCODE_UP;
            if (!SDL_PushEvent(&accelerate))
            {
                std::cerr
                    << "Unable to queue integrated Up Arrow acceleration: "
                    << SDL_GetError() << '\n';
                runtimeSmokeFailed = true;
            }
            raceSmokeAccelerateQueued = true;
        }
        if (options->raceRenderSmokeTest && inRace &&
            renderedFrames >= 20U + racePauseSmokeStep &&
            racePauseSmokeStep < 3U)
        {
            if (racePauseSmokeStep == 0U)
                openExitRaceDialog();
            else if (racePauseSmokeStep == 1U)
                exitRaceYesFocused = false;
            else
                closeExitRaceDialog();
            ++racePauseSmokeStep;
        }
#endif
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
#ifdef RRR3D_AUDIO
            if ((event.type == SDL_EVENT_AUDIO_DEVICE_ADDED ||
                 event.type == SDL_EVENT_AUDIO_DEVICE_REMOVED ||
                 event.type == SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED) &&
                !event.adevice.recording)
            {
                if (event.type == SDL_EVENT_AUDIO_DEVICE_ADDED)
                {
                    audio.notifyPlaybackDeviceEvent(
                        r3d::audio::PlaybackDeviceEvent::Added,
                        event.adevice.which);
                }
                else if (event.type == SDL_EVENT_AUDIO_DEVICE_REMOVED)
                {
                    audio.notifyPlaybackDeviceEvent(
                        r3d::audio::PlaybackDeviceEvent::Removed,
                        event.adevice.which);
                }
                else
                {
                    audio.notifyPlaybackDeviceEvent(
                        r3d::audio::PlaybackDeviceEvent::FormatChanged,
                        event.adevice.which);
                }
            }
#endif
#ifdef RRR3D_GAMEPAD_INPUT
#ifdef RRR3D_PHYSICS
            if (!inRace && bindingCaptureAction)
            {
                std::optional<std::string> bindingName;
                bool consumedCaptureEvent = false;
                if (event.type == SDL_EVENT_KEY_DOWN &&
                    !event.key.repeat)
                {
                    consumedCaptureEvent = true;
                    if (event.key.scancode == SDL_SCANCODE_BACKSPACE ||
                        event.key.scancode == SDL_SCANCODE_DELETE)
                    {
                        bindingName = "None";
                    }
                    else if (!bindingCaptureGamepad)
                    {
                        bindingName =
                            originalKeyName(event.key.scancode);
                    }
                }
                else if (bindingCaptureGamepad &&
                         event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
                {
                    consumedCaptureEvent = true;
                    bindingName = originalGamepadButtonName(
                        static_cast<SDL_GamepadButton>(
                            event.gbutton.button));
                }
                else if (bindingCaptureGamepad &&
                         event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION)
                {
                    bindingName = originalGamepadAxisName(
                        static_cast<SDL_GamepadAxis>(event.gaxis.axis),
                        event.gaxis.value);
                    consumedCaptureEvent = bindingName.has_value();
                }
                if (bindingName && !bindingName->empty())
                {
                    auto& bindings =
                        bindingCaptureGamepad
                            ? optionsDraftConfig.gamepadControls
                            : optionsDraftConfig.keyboardControls;
                    bindings[*bindingCaptureAction] = *bindingName;
                    std::cout
                        << "Original ControlsFrame: "
                        << *bindingCaptureAction << " -> "
                        << *bindingName << '\n';
                    bindingCaptureAction.reset();
                    refreshCurrentOptionsPage();
                }
                if (consumedCaptureEvent)
                    continue;
            }
#endif
            bool pointerTargetsExitChoice = true;
#ifdef RRR3D_PHYSICS
            if (inRace && exitRaceDialogVisible &&
                (event.type == SDL_EVENT_MOUSE_MOTION ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN))
            {
                int windowWidth = 0;
                int windowHeight = 0;
                const float mouseX =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.x
                        : event.button.x;
                const float mouseY =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.y
                        : event.button.y;
                std::optional<bool> hoveredYes;
                if (SDL_GetWindowSize(
                        window, &windowWidth, &windowHeight) &&
                    windowWidth > 0 && windowHeight > 0)
                {
                    const float virtualX =
                        mouseX * menu::virtualWidth /
                        static_cast<float>(windowWidth);
                    const float virtualY =
                        mouseY * menu::virtualHeight /
                        static_cast<float>(windowHeight);
                    const float buttonY =
                        menu::virtualHeight * 0.5F + 32.0F;
                    const float yesX =
                        menu::virtualWidth * 0.5F - 70.0F;
                    const float noX =
                        menu::virtualWidth * 0.5F + 70.0F;
                    if (std::abs(virtualY - buttonY) <= 19.0F &&
                        std::abs(virtualX - yesX) <= 45.0F)
                    {
                        hoveredYes = true;
                    }
                    else if (
                        std::abs(virtualY - buttonY) <= 19.0F &&
                        std::abs(virtualX - noX) <= 45.0F)
                    {
                        hoveredYes = false;
                    }
                }
                if (hoveredYes)
                    exitRaceYesFocused = *hoveredYes;
                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                    event.button.button == SDL_BUTTON_LEFT)
                {
                    pointerTargetsExitChoice =
                        hoveredYes.has_value();
                }
            }
#endif
            bool pointerTargetsItem = true;
            bool pointerHandledOriginalOptions = false;
            bool workshopPointerSlotPlane = false;
#ifdef RRR3D_PHYSICS
            if (!inRace &&
                menuStack.back() == MenuScreen::Garage &&
                (event.type == SDL_EVENT_MOUSE_MOTION ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN))
            {
                int windowWidth = 0;
                int windowHeight = 0;
                const float pointerX =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.x
                        : event.button.x;
                const float pointerY =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.y
                        : event.button.y;
                std::optional<std::size_t> hoveredGarageItem;
                if (SDL_GetWindowSize(
                        window, &windowWidth, &windowHeight) &&
                    windowWidth > 0 && windowHeight > 0)
                {
                    const float virtualX =
                        pointerX * menu::virtualWidth /
                        static_cast<float>(windowWidth);
                    const float virtualY =
                        pointerY * menu::virtualHeight /
                        static_cast<float>(windowHeight);
                    if (garagePurchaseDialogVisible)
                    {
                        const float buttonY =
                            menu::virtualHeight * 0.5F + 32.0F;
                        if (std::abs(virtualY - buttonY) <= 24.0F &&
                            std::abs(
                                virtualX -
                                (menu::virtualWidth * 0.5F -
                                 70.0F)) <= 55.0F)
                        {
                            garagePurchaseYesFocused = true;
                            hoveredGarageItem = 1U;
                        }
                        else if (
                            std::abs(virtualY - buttonY) <= 24.0F &&
                            std::abs(
                                virtualX -
                                (menu::virtualWidth * 0.5F +
                                 70.0F)) <= 55.0F)
                        {
                            garagePurchaseYesFocused = false;
                            hoveredGarageItem = 1U;
                        }
                    }
                    else
                    {
                        const float bottomCenterY =
                            menu::virtualHeight -
                            static_cast<float>(
                                garageBottomPanelImage.height) *
                                0.5F;
                        if (virtualX <= 250.0F &&
                            std::abs(
                                virtualY -
                                (bottomCenterY + 8.0F)) <= 45.0F)
                        {
                            hoveredGarageItem = 0U;
                        }
                        else if (
                            std::abs(
                                virtualX -
                                (menu::virtualWidth * 0.5F +
                                 15.0F)) <= 135.0F &&
                            std::abs(
                                virtualY -
                                (menu::virtualHeight -
                                 static_cast<float>(
                                     garageBottomPanelImage.height))) <=
                                40.0F)
                        {
                            hoveredGarageItem = 1U;
                        }
                        else if (
                            std::abs(virtualX - 181.0F) <= 58.0F &&
                            std::abs(
                                virtualY -
                                menu::virtualHeight * 0.5F) <= 80.0F)
                        {
                            hoveredGarageItem = 2U;
                        }
                        else if (
                            std::abs(
                                virtualX -
                                (menu::virtualWidth - 181.0F)) <=
                                58.0F &&
                            std::abs(
                                virtualY -
                                menu::virtualHeight * 0.5F) <= 80.0F)
                        {
                            hoveredGarageItem = 3U;
                        }
                        else
                        {
                            const float firstColorY =
                                menu::virtualHeight * 0.5F -
                                144.0F;
                            for (std::size_t index = 0U;
                                 index < 7U; ++index)
                            {
                                const float colorY =
                                    firstColorY +
                                    static_cast<float>(index) *
                                        48.0F;
                                if (std::abs(virtualY - colorY) >
                                    22.0F)
                                {
                                    continue;
                                }
                                if (virtualX <= 120.0F)
                                    hoveredGarageItem = 4U + index;
                                else if (
                                    virtualX >=
                                    menu::virtualWidth - 120.0F)
                                {
                                    hoveredGarageItem =
                                        11U + index;
                                }
                                if (hoveredGarageItem)
                                    break;
                            }
                        }
                    }
                }
                if (hoveredGarageItem &&
                    !garagePurchaseDialogVisible)
                {
                    menuSelection = *hoveredGarageItem;
                }
                pointerTargetsItem =
                    hoveredGarageItem.has_value() ||
                    event.type == SDL_EVENT_MOUSE_MOTION ||
                    event.button.button != SDL_BUTTON_LEFT;
            }
            else if (
                !inRace &&
                menuStack.back() == MenuScreen::Workshop &&
                (event.type == SDL_EVENT_MOUSE_MOTION ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN))
            {
                int windowWidth = 0;
                int windowHeight = 0;
                const float pointerX =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.x
                        : event.button.x;
                const float pointerY =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.y
                        : event.button.y;
                std::optional<std::size_t> hoveredWorkshopItem;
                if (SDL_GetWindowSize(
                        window, &windowWidth, &windowHeight) &&
                    windowWidth > 0 && windowHeight > 0)
                {
                    const float virtualX =
                        pointerX * menu::virtualWidth /
                        static_cast<float>(windowWidth);
                    const float virtualY =
                        pointerY * menu::virtualHeight /
                        static_cast<float>(windowHeight);
                    workshopDragX = virtualX;
                    workshopDragY = virtualY;
                    if (workshopConfirmation !=
                        WorkshopConfirmation::None)
                    {
                        const float dialogCenterX =
                            menu::virtualWidth * 0.5F;
                        const float choiceY =
                            menu::virtualHeight * 0.5F +
                            32.0F;
                        if (std::abs(
                                virtualY - choiceY) <=
                                30.0F &&
                            (std::abs(
                                 virtualX -
                                 (dialogCenterX - 70.0F)) <=
                                 55.0F ||
                             std::abs(
                                 virtualX -
                                 (dialogCenterX + 70.0F)) <=
                                 55.0F))
                        {
                            workshopConfirmationYesFocused =
                                virtualX < dialogCenterX;
                            hoveredWorkshopItem = 0U;
                        }
                    }
                    else
                    {
                    const float backY =
                        menu::virtualHeight -
                        static_cast<float>(
                            workshopBottomPanelImage.height) +
                        40.0F;
                    if (virtualX <= 250.0F &&
                        std::abs(virtualY - backY) <= 45.0F)
                    {
                        hoveredWorkshopItem = 0U;
                    }
                    const auto goodCenters =
                        workshopGoodCenters();
                    for (std::size_t index = 0U;
                         !hoveredWorkshopItem &&
                         index < goodCenters.size();
                         ++index)
                    {
                        if (std::abs(
                                virtualX -
                                goodCenters[index][0]) <= 48.0F &&
                            std::abs(
                                virtualY -
                                goodCenters[index][1]) <= 48.0F)
                        {
                            hoveredWorkshopItem = 1U + index;
                        }
                    }
                    const auto slotCenters =
                        workshopSlotCenters();
                    for (std::size_t index = 0U;
                         !hoveredWorkshopItem &&
                         index < slotCenters.size();
                         ++index)
                    {
                        if (std::abs(
                                virtualX -
                                slotCenters[index][0]) <=
                                static_cast<float>(
                                    workshopSlotImage.width) *
                                    0.5F &&
                            std::abs(
                                virtualY -
                                slotCenters[index][1]) <=
                                static_cast<float>(
                                    workshopSlotImage.height) *
                                    0.5F)
                        {
                            hoveredWorkshopItem = 13U + index;
                            workshopPointerSlotPlane =
                                std::abs(
                                    virtualX -
                                    (slotCenters[index][0] +
                                     51.0F)) > 25.0F ||
                                std::abs(
                                    virtualY -
                                    (slotCenters[index][1] +
                                     38.0F)) > 24.0F;
                        }
                    }
                    if (event.type ==
                            SDL_EVENT_MOUSE_BUTTON_DOWN &&
                        event.button.button == SDL_BUTTON_LEFT)
                    {
                        const float panelCenterY =
                            (static_cast<float>(
                                 workshopTopPanelImage.height) -
                                 30.0F +
                             menu::virtualHeight -
                             static_cast<float>(
                                 workshopBottomPanelImage.height)) *
                            0.5F;
                        const float upY =
                            panelCenterY -
                            static_cast<float>(
                                workshopLeftPanelImage.height) *
                                0.5F +
                            65.0F;
                        const float downY =
                            panelCenterY +
                            static_cast<float>(
                                workshopLeftPanelImage.height) *
                                0.5F -
                            42.0F;
                        const float arrowX =
                            30.0F +
                            static_cast<float>(
                                workshopLeftPanelImage.width) *
                                0.5F;
                        const std::size_t rowCount =
                            (workshopGoods.size() + 2U) / 3U;
                        const std::size_t maximumScroll =
                            rowCount > 4U ? rowCount - 4U : 0U;
                        if (std::abs(virtualX - arrowX) <= 35.0F &&
                            std::abs(virtualY - upY) <= 35.0F &&
                            workshopGoodScroll > 0U)
                        {
                            --workshopGoodScroll;
                            refreshWorkshopPage();
                            pointerHandledOriginalOptions = true;
                        }
                        else if (
                            std::abs(virtualX - arrowX) <= 35.0F &&
                            std::abs(virtualY - downY) <= 35.0F &&
                            workshopGoodScroll < maximumScroll)
                        {
                            ++workshopGoodScroll;
                            refreshWorkshopPage();
                            pointerHandledOriginalOptions = true;
                        }
                    }
                    }
                }
                if (hoveredWorkshopItem)
                {
                    menuSelection = *hoveredWorkshopItem;
                    refreshWorkshopPage();
                }
                pointerTargetsItem =
                    hoveredWorkshopItem.has_value() ||
                    pointerHandledOriginalOptions ||
                    event.type == SDL_EVENT_MOUSE_MOTION ||
                    event.button.button != SDL_BUTTON_LEFT;
            }
            else if (
                !inRace &&
                menuStack.back() == MenuScreen::Planets &&
                (event.type == SDL_EVENT_MOUSE_MOTION ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN))
            {
                int windowWidth = 0;
                int windowHeight = 0;
                const float pointerX =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.x
                        : event.button.x;
                const float pointerY =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.y
                        : event.button.y;
                std::optional<std::size_t> hoveredAngarItem;
                if (SDL_GetWindowSize(
                        window, &windowWidth, &windowHeight) &&
                    windowWidth > 0 && windowHeight > 0)
                {
                    const float virtualX =
                        pointerX * menu::virtualWidth /
                        static_cast<float>(windowWidth);
                    const float virtualY =
                        pointerY * menu::virtualHeight /
                        static_cast<float>(windowHeight);
                    const auto planetCount = std::min(
                        originalGarage->planets.size(),
                        profileState.player.planets.size());
                    if (angarWarningVisible)
                    {
                        const float okY =
                            menu::virtualHeight * 0.5F +
                            32.0F;
                        if (std::abs(
                                virtualX -
                                menu::virtualWidth * 0.5F) <=
                                55.0F &&
                            std::abs(virtualY - okY) <= 30.0F)
                        {
                            hoveredAngarItem = 0U;
                        }
                    }
                    else if (angarTravelDialogVisible)
                    {
                        const float choiceY =
                            menu::virtualHeight * 0.5F +
                            32.0F;
                        if (std::abs(virtualY - choiceY) <= 30.0F &&
                            (std::abs(
                                 virtualX -
                                 (menu::virtualWidth * 0.5F -
                                  70.0F)) <= 55.0F ||
                             std::abs(
                                 virtualX -
                                 (menu::virtualWidth * 0.5F +
                                  70.0F)) <= 55.0F))
                        {
                            angarTravelYesFocused =
                                virtualX <
                                menu::virtualWidth * 0.5F;
                            hoveredAngarItem = 0U;
                        }
                    }
                    else
                    {
                        const float panelCenterX =
                            menu::virtualWidth * 0.5F;
                        const float panelCenterY =
                            menu::virtualHeight -
                            static_cast<float>(
                                angarBottomPanelImage.height) *
                                0.5F -
                            20.0F;
                        const float firstPlanetX =
                            panelCenterX -
                            static_cast<float>(
                                angarBottomPanelImage.width) *
                                0.5F +
                            125.0F;
                        const float planetY =
                            panelCenterY -
                            static_cast<float>(
                                angarBottomPanelImage.height) *
                                0.5F +
                            90.0F;
                        for (std::size_t index = 0U;
                             index < planetCount; ++index)
                        {
                            const float centerX =
                                firstPlanetX +
                                static_cast<float>(index) *
                                    224.0F;
                            if (std::abs(virtualX - centerX) <=
                                    90.0F &&
                                std::abs(virtualY - planetY) <=
                                    90.0F)
                            {
                                hoveredAngarItem = index;
                                break;
                            }
                        }
                        const float backX =
                            static_cast<float>(
                                garageBackImage.width) *
                            0.5F;
                        if (!hoveredAngarItem &&
                            std::abs(virtualX - backX) <=
                                static_cast<float>(
                                    garageBackImage.width) *
                                    0.5F &&
                            std::abs(virtualY - 40.0F) <= 40.0F)
                        {
                            hoveredAngarItem = planetCount;
                        }
                        if (event.type ==
                                SDL_EVENT_MOUSE_BUTTON_DOWN &&
                            event.button.button == SDL_BUTTON_LEFT &&
                            angarPlanetIndex >= 0)
                        {
                            const auto selected =
                                static_cast<std::size_t>(
                                    angarPlanetIndex);
                            const float selectedX =
                                firstPlanetX +
                                static_cast<float>(selected) *
                                    224.0F;
                            const float infoWidth =
                                static_cast<float>(
                                    angarPlanetInfoImage.width);
                            const float infoX = std::clamp(
                                selectedX, infoWidth * 0.5F,
                                menu::virtualWidth -
                                    infoWidth * 0.5F);
                            const float infoY =
                                panelCenterY - 260.0F;
                            const float closeX =
                                infoX + 180.0F;
                            const float closeY =
                                infoY -
                                static_cast<float>(
                                    angarPlanetInfoImage.height) *
                                    0.5F +
                                15.0F;
                            if (std::abs(virtualX - closeX) <=
                                    16.0F &&
                                std::abs(virtualY - closeY) <=
                                    16.0F)
                            {
                                menuSelection = planetCount;
                                selectAngarPlanet(-1);
                                pointerHandledOriginalOptions = true;
                            }
                        }
                    }
                }
                if (hoveredAngarItem &&
                    !angarTravelDialogVisible &&
                    !angarWarningVisible &&
                    !pointerHandledOriginalOptions)
                {
                    menuSelection = *hoveredAngarItem;
                    selectAngarPlanet(
                        menuSelection <
                                originalGarage->planets.size()
                            ? static_cast<int>(menuSelection)
                            : -1);
                }
                pointerTargetsItem =
                    hoveredAngarItem.has_value() ||
                    pointerHandledOriginalOptions ||
                    event.type == SDL_EVENT_MOUSE_MOTION ||
                    event.button.button != SDL_BUTTON_LEFT;
            }
            else if (
                !inRace &&
                menuStack.back() == MenuScreen::Achievements &&
                (event.type == SDL_EVENT_MOUSE_MOTION ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN))
            {
                int windowWidth = 0;
                int windowHeight = 0;
                const float pointerX =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.x
                        : event.button.x;
                const float pointerY =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.y
                        : event.button.y;
                std::optional<std::size_t> hoveredAchievement;
                if (SDL_GetWindowSize(
                        window, &windowWidth, &windowHeight) &&
                    windowWidth > 0 && windowHeight > 0)
                {
                    const float virtualX =
                        pointerX * menu::virtualWidth /
                        static_cast<float>(windowWidth);
                    const float virtualY =
                        pointerY * menu::virtualHeight /
                        static_cast<float>(windowHeight);
                    if (achievementPointsWarningVisible)
                    {
                        const float okY =
                            menu::virtualHeight * 0.5F + 32.0F;
                        if (std::abs(
                                virtualX -
                                menu::virtualWidth * 0.5F) <= 55.0F &&
                            std::abs(virtualY - okY) <= 30.0F)
                        {
                            hoveredAchievement = 0U;
                        }
                    }
                    else if (achievementPurchaseDialogVisible)
                    {
                        const float choiceY =
                            menu::virtualHeight * 0.5F + 32.0F;
                        if (std::abs(virtualY - choiceY) <= 30.0F &&
                            (std::abs(
                                 virtualX -
                                 (menu::virtualWidth * 0.5F -
                                  70.0F)) <= 55.0F ||
                             std::abs(
                                 virtualX -
                                 (menu::virtualWidth * 0.5F +
                                  70.0F)) <= 55.0F))
                        {
                            achievementPurchaseYesFocused =
                                virtualX <
                                menu::virtualWidth * 0.5F;
                            hoveredAchievement = 0U;
                        }
                    }
                    else
                    {
                        const float scale = std::min(
                            menu::virtualWidth / 1090.0F,
                            menu::virtualHeight / 720.0F);
                        for (std::size_t reverse = 0U;
                             reverse <
                             originalAchievementVisuals.size();
                             ++reverse)
                        {
                            const std::size_t index =
                                originalAchievementVisuals.size() -
                                1U - reverse;
                            const auto& visual =
                                originalAchievementVisuals[index];
                            const auto& image =
                                achievementState(index) == "asOpened"
                                    ? achievementOpenedImages[index]
                                    : achievementLockedImages[index];
                            const float centerX =
                                menu::virtualWidth * 0.5F +
                                (visual.x - 30.0F) * scale;
                            const float centerY = visual.y * scale;
                            if (std::abs(virtualX - centerX) <=
                                    static_cast<float>(image.width) *
                                        scale * 0.5F &&
                                std::abs(virtualY - centerY) <=
                                    static_cast<float>(image.height) *
                                        scale * 0.5F)
                            {
                                hoveredAchievement = index;
                                break;
                            }
                        }
                        const float backX =
                            static_cast<float>(
                                garageBackImage.width) *
                            0.5F;
                        const float backY =
                            menu::virtualHeight - 80.0F + 17.0F +
                            static_cast<float>(
                                garageBackImage.height) *
                                0.5F;
                        if (!hoveredAchievement &&
                            std::abs(virtualX - backX) <=
                                static_cast<float>(
                                    garageBackImage.width) *
                                    0.5F &&
                            std::abs(virtualY - backY) <=
                                static_cast<float>(
                                    garageBackImage.height) *
                                    0.5F)
                        {
                            hoveredAchievement =
                                originalAchievementBack;
                        }
                    }
                }
                if (hoveredAchievement &&
                    !achievementPurchaseDialogVisible &&
                    !achievementPointsWarningVisible)
                {
                    menuSelection = *hoveredAchievement;
                }
                pointerTargetsItem =
                    hoveredAchievement.has_value() ||
                    event.type == SDL_EVENT_MOUSE_MOTION ||
                    event.button.button != SDL_BUTTON_LEFT;
            }
            else if (!inRace &&
                menuStack.back() == MenuScreen::RaceMenu &&
                (event.type == SDL_EVENT_MOUSE_MOTION ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN))
            {
                int windowWidth = 0;
                int windowHeight = 0;
                const float pointerX =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.x
                        : event.button.x;
                const float pointerY =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.y
                        : event.button.y;
                std::optional<std::size_t> hoveredRaceMenuItem;
                if (SDL_GetWindowSize(
                        window, &windowWidth, &windowHeight) &&
                    windowWidth > 0 && windowHeight > 0)
                {
                    const float virtualX =
                        pointerX * menu::virtualWidth /
                        static_cast<float>(windowWidth);
                    const float virtualY =
                        pointerY * menu::virtualHeight /
                        static_cast<float>(windowHeight);
                    constexpr float itemWidth = 110.0F;
                    constexpr float itemSpacing = 50.0F;
                    const float firstX =
                        menu::virtualWidth * 0.5F -
                        (7.0F * itemWidth +
                         6.0F * itemSpacing) *
                            0.5F +
                        itemWidth * 0.5F;
                    const float itemY =
                        menu::virtualHeight -
                        static_cast<float>(
                            raceBottomPanelImage.height) *
                            0.5F -
                        72.0F;
                    for (std::size_t index = 0U;
                         index < raceMenuIcons.size(); ++index)
                    {
                        const float itemX =
                            firstX +
                            static_cast<float>(index) *
                                (itemWidth + itemSpacing);
                        if (std::abs(virtualX - itemX) <= 68.0F &&
                            std::abs(virtualY - itemY) <= 62.0F)
                        {
                            hoveredRaceMenuItem = index;
                            break;
                        }
                    }
                }
                if (hoveredRaceMenuItem)
                    menuSelection = *hoveredRaceMenuItem;
                pointerTargetsItem =
                    hoveredRaceMenuItem.has_value() ||
                    event.type == SDL_EVENT_MOUSE_MOTION ||
                    event.button.button != SDL_BUTTON_LEFT;
            }
            else if (!inRace &&
                isOriginalOptionsScreen(menuStack.back()) &&
                (event.type == SDL_EVENT_MOUSE_MOTION ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN))
            {
                int windowWidth = 0;
                int windowHeight = 0;
                const float pointerX =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.x
                        : event.button.x;
                const float pointerY =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.y
                        : event.button.y;
                std::optional<std::size_t> hoveredOption;
                std::optional<std::size_t> hoveredState;
                if (SDL_GetWindowSize(
                        window, &windowWidth, &windowHeight) &&
                    windowWidth > 0 && windowHeight > 0)
                {
                    const float virtualX =
                        pointerX * menu::virtualWidth /
                        static_cast<float>(windowWidth);
                    const float virtualY =
                        pointerY * menu::virtualHeight /
                        static_cast<float>(windowHeight);
                    const float centerX =
                        menu::virtualWidth * 0.5F;
                    const float centerY =
                        menu::virtualHeight * 0.5F;

                    for (std::size_t state = 0U; state < 4U;
                         ++state)
                    {
                        const float stateY =
                            centerY - 125.0F +
                            static_cast<float>(state) * 100.0F;
                        if (virtualX >= centerX - 600.0F &&
                            virtualX <= centerX - 250.0F &&
                            std::abs(virtualY - stateY) <= 45.0F)
                        {
                            hoveredState = state;
                            break;
                        }
                    }

                    const std::size_t rowCount =
                        menuStack.back() == MenuScreen::GameOptions
                            ? gameOptionNamesPage.labels.size()
                        : menuStack.back() ==
                                  MenuScreen::GraphicsOptions
                            ? graphicsOptionNamesPage.labels.size()
                        : menuStack.back() ==
                                  MenuScreen::SoundOptions
                            ? soundOptionNamesPage.labels.size()
                            : originalControlActions.size();
                    const std::size_t visibleRows =
                        menuStack.back() == MenuScreen::GameOptions
                            ? 7U
                        : menuStack.back() ==
                                  MenuScreen::ControlsOptions
                            ? controlsVisibleRows
                            : rowCount;
                    const std::size_t scrollAnchor =
                        menuSelection < rowCount
                            ? menuSelection
                            : rowCount - 1U;
                    const std::size_t firstVisible =
                        scrollAnchor < visibleRows
                            ? 0U
                            : std::min(
                                  scrollAnchor - visibleRows + 1U,
                                  rowCount - visibleRows);
                    const float firstRowY =
                        centerY -
                        (menuStack.back() ==
                                 MenuScreen::ControlsOptions
                             ? 130.0F
                         : menuStack.back() ==
                                   MenuScreen::GameOptions
                             ? 178.0F
                             : 174.0F);
                    if (virtualX >= centerX - 420.0F &&
                        virtualX <= centerX + 520.0F)
                    {
                        for (std::size_t slot = 0U;
                             slot < visibleRows; ++slot)
                        {
                            const float rowY =
                                firstRowY +
                                static_cast<float>(slot) * 50.0F;
                            if (std::abs(virtualY - rowY) <= 23.0F)
                            {
                                hoveredOption =
                                    firstVisible + slot;
                                if (menuStack.back() ==
                                        MenuScreen::
                                            ControlsOptions &&
                                    virtualX >= centerX + 165.0F)
                                {
                                    controlsUseGamepad =
                                        virtualX >=
                                        centerX + 345.0F;
                                }
                                break;
                            }
                        }
                    }
                    const float actionY = centerY + 240.0F;
                    if (std::abs(virtualY - actionY) <= 50.0F)
                    {
                        if (virtualX >= centerX - 80.0F &&
                            virtualX <= centerX + 280.0F)
                            hoveredOption = rowCount;
                        else if (
                            virtualX >= centerX + 280.0F &&
                            virtualX <= centerX + 650.0F)
                            hoveredOption = rowCount + 1U;
                    }
                    if (event.type ==
                            SDL_EVENT_MOUSE_BUTTON_DOWN &&
                        event.button.button == SDL_BUTTON_LEFT)
                    {
                        const float upY =
                            centerY -
                            (menuStack.back() ==
                                     MenuScreen::ControlsOptions
                                 ? 150.0F
                                 : 200.0F);
                        const float downY = centerY + 195.0F;
                        if (std::abs(
                                virtualX -
                                (centerX + 150.0F)) <= 35.0F &&
                            std::abs(virtualY - upY) <= 35.0F &&
                            menuSelection > 0U)
                        {
                            --menuSelection;
                            pointerHandledOriginalOptions = true;
                        }
                        else if (
                            std::abs(
                                virtualX -
                                (centerX + 150.0F)) <= 35.0F &&
                            std::abs(virtualY - downY) <= 35.0F &&
                            menuSelection + 1U < rowCount)
                        {
                            ++menuSelection;
                            pointerHandledOriginalOptions = true;
                        }
                        else if (hoveredState)
                        {
                            setOptionsState(*hoveredState);
                            pointerHandledOriginalOptions = true;
                        }
                    }
                }
                if (hoveredOption)
                    menuSelection = *hoveredOption;
                pointerTargetsItem =
                    hoveredOption.has_value() ||
                    hoveredState.has_value() ||
                    event.type == SDL_EVENT_MOUSE_MOTION ||
                    event.button.button != SDL_BUTTON_LEFT;
            }
            else
#endif
            if (
#ifdef RRR3D_PHYSICS
                !inRace &&
#endif
                event.type == SDL_EVENT_MOUSE_MOTION)
            {
                const auto hovered = hoveredItem(
                    window, event.motion.x, event.motion.y,
                    activeMenuPage().labels.size(),
                    static_cast<float>(model->selectionImage.width),
                    static_cast<float>(model->selectionImage.height));
                if (hovered)
                    menuSelection = *hovered;
            }
            else if (
#ifdef RRR3D_PHYSICS
                !inRace &&
#endif
                event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            {
                const auto hovered = hoveredItem(
                    window, event.button.x, event.button.y,
                    activeMenuPage().labels.size(),
                    static_cast<float>(model->selectionImage.width),
                    static_cast<float>(model->selectionImage.height));
                pointerTargetsItem = hovered.has_value() ||
                                     event.button.button != SDL_BUTTON_LEFT;
                if (hovered)
                    menuSelection = *hovered;
            }
            const auto inputEvents = input.processEvent(event);
            for (const auto& inputEvent : inputEvents)
            {
#ifdef RRR3D_PHYSICS
                if (inRace && exitRaceDialogVisible)
                {
                    if (!inputEvent.active || inputEvent.repeated)
                        continue;
                    if (inputEvent.action ==
                            rrr3d::input::Action::TurnLeft ||
                        inputEvent.action ==
                            rrr3d::input::Action::MenuUp)
                    {
                        exitRaceYesFocused = true;
                    }
                    else if (inputEvent.action ==
                                 rrr3d::input::Action::TurnRight ||
                             inputEvent.action ==
                                 rrr3d::input::Action::MenuDown)
                    {
                        exitRaceYesFocused = false;
                    }
                    else if (inputEvent.action ==
                                 rrr3d::input::Action::MenuBack ||
                             inputEvent.action ==
                                 rrr3d::input::Action::Pause)
                    {
                        closeExitRaceDialog();
                        racePauseResumeObserved = true;
                    }
                    else if (inputEvent.action ==
                             rrr3d::input::Action::MenuConfirm)
                    {
                        if (inputEvent.source ==
                                rrr3d::input::Source::Mouse &&
                            !pointerTargetsExitChoice)
                            continue;
                        if (exitRaceYesFocused)
                            leaveCurrentRace();
                        else
                        {
                            closeExitRaceDialog();
                            racePauseResumeObserved = true;
                        }
                    }
                    continue;
                }
                if (inRace)
                {
                    switch (inputEvent.action)
                    {
                    case rrr3d::input::Action::Accelerate:
                        raceInput.throttle = inputEvent.active
                                                 ? inputEvent.value
                                                 : 0.0F;
                        break;
                    case rrr3d::input::Action::Brake:
                        raceInput.reverse = inputEvent.active
                                                ? inputEvent.value
                                                : 0.0F;
                        break;
                    case rrr3d::input::Action::TurnLeft:
                        if (inputEvent.active)
                            raceInput.steering = inputEvent.value;
                        else if (raceInput.steering > 0.0F)
                            raceInput.steering = 0.0F;
                        break;
                    case rrr3d::input::Action::TurnRight:
                        if (inputEvent.active)
                            raceInput.steering = -inputEvent.value;
                        else if (raceInput.steering < 0.0F)
                            raceInput.steering = 0.0F;
                        break;
                    case rrr3d::input::Action::UseWeapon:
                        if (inputEvent.active && !inputEvent.repeated)
                            raceUseWeaponRequested = true;
                        break;
                    case rrr3d::input::Action::UseAllWeapons:
                        if (inputEvent.active && !inputEvent.repeated)
                            raceUseAllWeaponsRequested = true;
                        break;
                    case rrr3d::input::Action::UseMine:
                        raceUseMine = inputEvent.active;
                        break;
                    case rrr3d::input::Action::UseHyper:
                        raceUseHyper = inputEvent.active;
                        break;
                    case rrr3d::input::Action::ChangeWeapon:
                    case rrr3d::input::Action::NextWeapon:
                        if (inputEvent.active && !inputEvent.repeated)
                        {
                            raceChangeWeaponRequested = true;
                            raceWeaponChangeDirection = 1;
                        }
                        break;
                    case rrr3d::input::Action::PreviousWeapon:
                        if (inputEvent.active && !inputEvent.repeated)
                        {
                            raceChangeWeaponRequested = true;
                            raceWeaponChangeDirection = -1;
                        }
                        break;
                    case rrr3d::input::Action::SelectWeapon1:
                    case rrr3d::input::Action::SelectWeapon2:
                    case rrr3d::input::Action::SelectWeapon3:
                    case rrr3d::input::Action::SelectWeapon4:
                        if (inputEvent.active && !inputEvent.repeated)
                        {
                            raceFireWeaponSlotRequested =
                                static_cast<int>(inputEvent.action) -
                                static_cast<int>(
                                    rrr3d::input::Action::SelectWeapon1);
                        }
                        break;
                    case rrr3d::input::Action::ToggleCamera:
                        if (inputEvent.active && !inputEvent.repeated)
                        {
                            using CameraStyle =
                                r3d::game::originalrace::PreferredCamera;
                            profileState.config.preferredCamera =
                                profileState.config.preferredCamera ==
                                        CameraStyle::Isometric
                                    ? CameraStyle::ThirdPerson
                                    : CameraStyle::Isometric;
                            raceRenderer.resetCamera();
                            saveRaceProfile();
                        }
                        break;
                    case rrr3d::input::Action::ResetVehicle:
                        if (inputEvent.active && !inputEvent.repeated)
                            raceResetRequested = true;
                        break;
                    case rrr3d::input::Action::Pause:
                        if (inputEvent.active && !inputEvent.repeated)
                            openExitRaceDialog();
                        break;
                    case rrr3d::input::Action::MenuBack:
                        // GUI Back is deliberately ignored during gameplay.
                        // The source exits through gaEscape/Start; on the
                        // default gamepad B is the brake and must never leave
                        // the race merely because it is also GUI Back.
                        break;
                    default:
                        break;
                    }
                    continue;
                }
#endif
                if (pointerHandledOriginalOptions &&
                    inputEvent.source ==
                        rrr3d::input::Source::Mouse &&
                    inputEvent.action ==
                        rrr3d::input::Action::MenuConfirm)
                {
                    continue;
                }
                if (!pointerTargetsItem &&
                    inputEvent.source == rrr3d::input::Source::Mouse &&
                    inputEvent.action ==
                        rrr3d::input::Action::MenuConfirm)
                    continue;
                if (!inputEvent.active)
                    continue;
#ifdef RRR3D_PHYSICS
                if (menuStack.back() == MenuScreen::Garage)
                {
                    if (garagePurchaseDialogVisible)
                    {
                        if (inputEvent.action ==
                                rrr3d::input::Action::TurnLeft ||
                            inputEvent.action ==
                                rrr3d::input::Action::MenuUp)
                        {
                            garagePurchaseYesFocused = true;
                        }
                        else if (
                            inputEvent.action ==
                                rrr3d::input::Action::TurnRight ||
                            inputEvent.action ==
                                rrr3d::input::Action::MenuDown)
                        {
                            garagePurchaseYesFocused = false;
                        }
                        else if (
                            !inputEvent.repeated &&
                            (inputEvent.action ==
                                 rrr3d::input::Action::MenuBack ||
                             inputEvent.action ==
                                 rrr3d::input::Action::Pause))
                        {
                            garagePurchaseDialogVisible = false;
                        }
                        else if (
                            !inputEvent.repeated &&
                            inputEvent.action ==
                                rrr3d::input::Action::MenuConfirm)
                        {
#ifdef RRR3D_AUDIO
                            playMainButtonClick();
#endif
                            if (!garagePurchaseYesFocused)
                            {
                                garagePurchaseDialogVisible = false;
                            }
                            else
                            {
                                const auto& car =
                                    originalGarage
                                        ->cars[garageCarIndex];
                                std::string garageError;
                                if (r3d::game::originalrace::
                                        selectOriginalGarageCar(
                                            *originalGarage,
                                            profileState, car,
                                            championshipMode,
                                            garageError))
                                {
                                    garagePurchaseDialogVisible =
                                        false;
                                    saveRaceProfile();
                                    backMenu();
                                }
                                else
                                {
                                    std::cerr
                                        << "Original GarageFrame: "
                                        << garageError << '\n';
                                    garagePurchaseDialogVisible =
                                        false;
                                    refreshGaragePage();
                                }
                            }
                        }
                        continue;
                    }

                    constexpr std::size_t garageFocusCount = 18U;
                    if (inputEvent.action ==
                        rrr3d::input::Action::MenuUp)
                    {
                        menuSelection =
                            menuSelection == 0U
                                ? garageFocusCount - 1U
                                : menuSelection - 1U;
                        continue;
                    }
                    if (inputEvent.action ==
                        rrr3d::input::Action::MenuDown)
                    {
                        menuSelection =
                            (menuSelection + 1U) %
                            garageFocusCount;
                        continue;
                    }
                    if (inputEvent.action ==
                            rrr3d::input::Action::TurnLeft ||
                        inputEvent.action ==
                            rrr3d::input::Action::TurnRight)
                    {
                        if (!inputEvent.repeated &&
                            !garageCarOrder.empty())
                        {
                            if (inputEvent.action ==
                                    rrr3d::input::Action::
                                        TurnLeft &&
                                garageViewIndex > 0U)
                            {
                                --garageViewIndex;
                            }
                            else if (
                                inputEvent.action ==
                                    rrr3d::input::Action::
                                        TurnRight &&
                                garageViewIndex + 1U <
                                    garageCarOrder.size())
                            {
                                ++garageViewIndex;
                            }
                            refreshGaragePage();
                        }
                        continue;
                    }
                    if (inputEvent.repeated)
                        continue;
                    if (inputEvent.action ==
                            rrr3d::input::Action::MenuBack ||
                        inputEvent.action ==
                            rrr3d::input::Action::Pause)
                    {
#ifdef RRR3D_AUDIO
                        playMainButtonClick();
#endif
                        backMenu();
                        continue;
                    }
                    if (inputEvent.action !=
                        rrr3d::input::Action::MenuConfirm)
                    {
                        continue;
                    }
#ifdef RRR3D_AUDIO
                    playMainButtonClick();
#endif
                    if (menuSelection == 0U)
                    {
                        backMenu();
                    }
                    else if (
                        menuSelection == 1U &&
                        !garageCarOrder[garageViewIndex].locked)
                    {
                        const auto& car =
                            originalGarage->cars[garageCarIndex];
                        if (car.record ==
                            profileState.player.currentCar)
                        {
                            backMenu();
                        }
                        else if (championshipMode)
                        {
                            garagePurchaseYesFocused = true;
                            garagePurchaseDialogVisible = true;
                        }
                        else
                        {
                            std::string garageError;
                            if (r3d::game::originalrace::
                                    selectOriginalGarageCar(
                                        *originalGarage,
                                        profileState, car, false,
                                        garageError))
                            {
                                saveRaceProfile();
                                backMenu();
                            }
                            else
                            {
                                std::cerr
                                    << "Original GarageFrame: "
                                    << garageError << '\n';
                            }
                        }
                    }
                    else if (menuSelection == 2U &&
                             garageViewIndex > 0U)
                    {
                        --garageViewIndex;
                        refreshGaragePage();
                    }
                    else if (
                        menuSelection == 3U &&
                        garageViewIndex + 1U <
                            garageCarOrder.size())
                    {
                        ++garageViewIndex;
                        refreshGaragePage();
                    }
                    else if (
                        menuSelection >= 4U &&
                        menuSelection < garageFocusCount)
                    {
                        const std::size_t colorIndex =
                            menuSelection - 4U;
                        for (std::size_t component = 0U;
                             component < 4U; ++component)
                        {
                            profileState.player.color[component] =
                                static_cast<float>(
                                    garageColorPixels[colorIndex]
                                                      [component]) /
                                255.0F;
                        }
                        saveRaceProfile();
                    }
                    continue;
                }
                if (menuStack.back() == MenuScreen::Workshop)
                {
                    if (workshopConfirmation !=
                        WorkshopConfirmation::None)
                    {
                        if (inputEvent.action ==
                                rrr3d::input::Action::TurnLeft ||
                            inputEvent.action ==
                                rrr3d::input::Action::MenuUp)
                        {
                            workshopConfirmationYesFocused = true;
                        }
                        else if (
                            inputEvent.action ==
                                rrr3d::input::Action::TurnRight ||
                            inputEvent.action ==
                                rrr3d::input::Action::MenuDown)
                        {
                            workshopConfirmationYesFocused = false;
                        }
                        else if (
                            !inputEvent.repeated &&
                            (inputEvent.action ==
                                 rrr3d::input::Action::MenuBack ||
                             inputEvent.action ==
                                 rrr3d::input::Action::Pause))
                        {
                            workshopConfirmation =
                                WorkshopConfirmation::None;
                            workshopPendingPurchase = nullptr;
                        }
                        else if (
                            !inputEvent.repeated &&
                            inputEvent.action ==
                                rrr3d::input::Action::MenuConfirm)
                        {
#ifdef RRR3D_AUDIO
                            playMainButtonClick();
#endif
                            const auto confirmation =
                                workshopConfirmation;
                            const auto* pending =
                                workshopPendingPurchase;
                            const bool accepted =
                                workshopConfirmationYesFocused;
                            workshopConfirmation =
                                WorkshopConfirmation::None;
                            workshopPendingPurchase = nullptr;
                            if (accepted)
                            {
                                if (confirmation ==
                                        WorkshopConfirmation::Buy &&
                                    pending != nullptr)
                                {
                                    buyWorkshopGood(*pending);
                                }
                                else if (
                                    confirmation ==
                                    WorkshopConfirmation::Sell)
                                {
                                    stopWorkshopDrag(true, true);
                                }
                            }
                        }
                        continue;
                    }
                    constexpr std::size_t workshopFocusCount = 23U;
                    if (inputEvent.action ==
                        rrr3d::input::Action::MenuUp)
                    {
                        menuSelection =
                            menuSelection == 0U
                                ? workshopFocusCount - 1U
                                : menuSelection - 1U;
                        refreshWorkshopPage();
                        continue;
                    }
                    if (inputEvent.action ==
                        rrr3d::input::Action::MenuDown)
                    {
                        menuSelection =
                            (menuSelection + 1U) %
                            workshopFocusCount;
                        refreshWorkshopPage();
                        continue;
                    }
                    if (inputEvent.action ==
                            rrr3d::input::Action::TurnLeft ||
                        inputEvent.action ==
                            rrr3d::input::Action::TurnRight)
                    {
                        if (!inputEvent.repeated)
                        {
                            const std::size_t rowCount =
                                (workshopGoods.size() + 2U) / 3U;
                            const std::size_t maximumScroll =
                                rowCount > 4U ? rowCount - 4U : 0U;
                            if (inputEvent.action ==
                                    rrr3d::input::Action::TurnLeft &&
                                workshopGoodScroll > 0U)
                            {
                                --workshopGoodScroll;
                            }
                            else if (
                                inputEvent.action ==
                                    rrr3d::input::Action::TurnRight &&
                                workshopGoodScroll < maximumScroll)
                            {
                                ++workshopGoodScroll;
                            }
                            refreshWorkshopPage();
                        }
                        continue;
                    }
                    if (inputEvent.repeated)
                        continue;
                    if (inputEvent.action ==
                            rrr3d::input::Action::MenuBack ||
                        inputEvent.action ==
                            rrr3d::input::Action::Pause)
                    {
#ifdef RRR3D_AUDIO
                        playMainButtonClick();
#endif
                        if (workshopDrag.active())
                            stopWorkshopDrag(false);
                        else
                            backMenu();
                        continue;
                    }
                    if (inputEvent.action ==
                        rrr3d::input::Action::MenuConfirm)
                    {
#ifdef RRR3D_AUDIO
                        playMainButtonClick();
#endif
                        activateWorkshopFocus(
                            inputEvent.source ==
                                rrr3d::input::Source::Mouse &&
                            workshopPointerSlotPlane);
                    }
                    continue;
                }
                if (menuStack.back() == MenuScreen::Achievements)
                {
                    if (achievementPointsWarningVisible)
                    {
                        if (!inputEvent.repeated &&
                            (inputEvent.action ==
                                 rrr3d::input::Action::MenuConfirm ||
                             inputEvent.action ==
                                 rrr3d::input::Action::MenuBack ||
                             inputEvent.action ==
                                 rrr3d::input::Action::Pause))
                        {
                            achievementPointsWarningVisible = false;
                        }
                        continue;
                    }
                    if (achievementPurchaseDialogVisible)
                    {
                        if (inputEvent.action ==
                                rrr3d::input::Action::TurnLeft ||
                            inputEvent.action ==
                                rrr3d::input::Action::MenuUp)
                        {
                            achievementPurchaseYesFocused = true;
                        }
                        else if (
                            inputEvent.action ==
                                rrr3d::input::Action::TurnRight ||
                            inputEvent.action ==
                                rrr3d::input::Action::MenuDown)
                        {
                            achievementPurchaseYesFocused = false;
                        }
                        else if (
                            !inputEvent.repeated &&
                            (inputEvent.action ==
                                 rrr3d::input::Action::MenuBack ||
                             inputEvent.action ==
                                 rrr3d::input::Action::Pause))
                        {
                            achievementPurchaseDialogVisible = false;
                            achievementPendingPurchase =
                                originalAchievementNoTarget;
                        }
                        else if (
                            !inputEvent.repeated &&
                            inputEvent.action ==
                                rrr3d::input::Action::MenuConfirm)
                        {
#ifdef RRR3D_AUDIO
                            playMainButtonClick();
#endif
                            const auto pending =
                                achievementPendingPurchase;
                            const bool accepted =
                                achievementPurchaseYesFocused;
                            achievementPurchaseDialogVisible = false;
                            achievementPendingPurchase =
                                originalAchievementNoTarget;
                            if (accepted &&
                                pending <
                                    originalAchievementVisuals.size() &&
                                achievementState(pending) ==
                                    "asUnlocked")
                            {
                                const auto price =
                                    achievementPrice(pending);
                                if (profileState.achievementPoints <
                                    price)
                                {
                                    achievementPointsWarningVisible =
                                        true;
                                }
                                else
                                {
                                    profileState.achievementPoints -=
                                        price;
                                    profileState
                                        .achievementItems[
                                            std::string(
                                                originalAchievementVisuals
                                                    [pending]
                                                    .name)]
                                        .values["state"] = "asOpened";
                                    saveRaceProfile();
                                    refreshAchievementsPage();
                                    std::cout
                                        << "Original AchievmentFrame "
                                           "reward opened: "
                                        << originalAchievementVisuals
                                               [pending]
                                               .name
                                        << '\n';
                                }
                            }
                        }
                        continue;
                    }
                    if (inputEvent.action ==
                        rrr3d::input::Action::TurnLeft)
                    {
                        moveAchievementFocus(0U);
                        continue;
                    }
                    if (inputEvent.action ==
                        rrr3d::input::Action::TurnRight)
                    {
                        moveAchievementFocus(1U);
                        continue;
                    }
                    if (inputEvent.action ==
                        rrr3d::input::Action::MenuUp)
                    {
                        moveAchievementFocus(2U);
                        continue;
                    }
                    if (inputEvent.action ==
                        rrr3d::input::Action::MenuDown)
                    {
                        moveAchievementFocus(3U);
                        continue;
                    }
                    if (inputEvent.repeated)
                        continue;
                    if (inputEvent.action ==
                            rrr3d::input::Action::MenuBack ||
                        inputEvent.action ==
                            rrr3d::input::Action::Pause)
                    {
#ifdef RRR3D_AUDIO
                        playMainButtonClick();
#endif
                        backMenu();
                        continue;
                    }
                    if (inputEvent.action ==
                        rrr3d::input::Action::MenuConfirm)
                    {
#ifdef RRR3D_AUDIO
                        playMainButtonClick();
#endif
                        if (menuSelection ==
                            originalAchievementBack)
                        {
                            backMenu();
                        }
                        else if (
                            achievementState(menuSelection) ==
                            "asUnlocked")
                        {
                            achievementPendingPurchase =
                                menuSelection;
                            achievementPurchaseYesFocused = true;
                            achievementPurchaseDialogVisible = true;
                        }
                    }
                    continue;
                }
                if (menuStack.back() == MenuScreen::Planets)
                {
                    const auto planetCount = std::min(
                        originalGarage->planets.size(),
                        profileState.player.planets.size());
                    if (angarWarningVisible)
                    {
                        if (!inputEvent.repeated &&
                            (inputEvent.action ==
                                 rrr3d::input::Action::MenuConfirm ||
                             inputEvent.action ==
                                 rrr3d::input::Action::MenuBack ||
                             inputEvent.action ==
                                 rrr3d::input::Action::Pause))
                        {
                            angarWarningVisible = false;
                        }
                        continue;
                    }
                    if (angarTravelDialogVisible)
                    {
                        if (inputEvent.action ==
                                rrr3d::input::Action::TurnLeft ||
                            inputEvent.action ==
                                rrr3d::input::Action::MenuUp)
                        {
                            angarTravelYesFocused = true;
                        }
                        else if (
                            inputEvent.action ==
                                rrr3d::input::Action::TurnRight ||
                            inputEvent.action ==
                                rrr3d::input::Action::MenuDown)
                        {
                            angarTravelYesFocused = false;
                        }
                        else if (
                            !inputEvent.repeated &&
                            (inputEvent.action ==
                                 rrr3d::input::Action::MenuBack ||
                             inputEvent.action ==
                                 rrr3d::input::Action::Pause))
                        {
                            angarTravelDialogVisible = false;
                        }
                        else if (
                            !inputEvent.repeated &&
                            inputEvent.action ==
                                rrr3d::input::Action::MenuConfirm)
                        {
                            if (angarTravelYesFocused)
                                changeAngarPlanet(angarTravelTarget);
                            else
                                angarTravelDialogVisible = false;
                        }
                        continue;
                    }
                    if (inputEvent.action ==
                            rrr3d::input::Action::TurnLeft ||
                        inputEvent.action ==
                            rrr3d::input::Action::TurnRight)
                    {
                        if (!inputEvent.repeated && planetCount > 0U)
                        {
                            if (menuSelection >= planetCount)
                            {
                                menuSelection = 0U;
                            }
                            else if (
                                inputEvent.action ==
                                rrr3d::input::Action::TurnLeft)
                            {
                                menuSelection =
                                    menuSelection == 0U
                                        ? planetCount - 1U
                                        : menuSelection - 1U;
                            }
                            else
                            {
                                menuSelection =
                                    (menuSelection + 1U) %
                                    planetCount;
                            }
                            selectAngarPlanet(
                                static_cast<int>(menuSelection));
                        }
                        continue;
                    }
                    if (inputEvent.action ==
                            rrr3d::input::Action::MenuUp ||
                        inputEvent.action ==
                            rrr3d::input::Action::MenuDown)
                    {
                        menuSelection =
                            menuSelection < planetCount
                                ? planetCount
                                : 0U;
                        selectAngarPlanet(
                            menuSelection < planetCount
                                ? static_cast<int>(menuSelection)
                                : -1);
                        continue;
                    }
                    if (inputEvent.repeated)
                        continue;
                    const bool backRequested =
                        inputEvent.action ==
                            rrr3d::input::Action::MenuBack ||
                        inputEvent.action ==
                            rrr3d::input::Action::Pause ||
                        (inputEvent.action ==
                             rrr3d::input::Action::MenuConfirm &&
                         menuSelection >= planetCount);
                    if (backRequested)
                    {
                        if (racePlanetChampion)
                        {
                            requestAngarTravel(
                                std::min<std::size_t>(
                                    profileState.player.currentPlanet,
                                    planetCount - 1U));
                        }
                        else
                        {
                            backMenu();
                        }
                        continue;
                    }
                    if (inputEvent.action !=
                            rrr3d::input::Action::MenuConfirm ||
                        menuSelection >= planetCount)
                    {
                        continue;
                    }
                    const auto index = menuSelection;
                    const bool current =
                        index == profileState.player.currentPlanet;
                    const bool next =
                        racePlanetChampion &&
                        index ==
                            profileState.player.currentPlanet + 1U;
                    if (racePlanetChampion && (current || next))
                    {
                        requestAngarTravel(index);
                    }
                    else if (
                        !championshipMode &&
                        profileState.player.planets[index].state == 0U)
                    {
                        changeAngarPlanet(index);
                    }
                    else if (current)
                    {
                        backMenu();
                    }
                    else
                    {
                        angarWarningVisible = true;
                        std::cout
                            << "Original AngarFrame warning: "
                            << localized("svHintCantFlyPlanet")
                            << '\n';
                    }
                    continue;
                }
#endif
                auto& page = activeMenuPage();
                if (inputEvent.action ==
                    rrr3d::input::Action::MenuUp)
                {
                    menuSelection =
                        menuSelection == 0U
                            ? page.labels.size() - 1U
                            : menuSelection - 1U;
                    continue;
                }
                if (inputEvent.action ==
                    rrr3d::input::Action::MenuDown)
                {
                    menuSelection =
                        (menuSelection + 1U) % page.labels.size();
                    continue;
                }
#ifdef RRR3D_PHYSICS
                if (menuStack.back() == MenuScreen::RaceMenu &&
                    (inputEvent.action ==
                         rrr3d::input::Action::TurnLeft ||
                     inputEvent.action ==
                         rrr3d::input::Action::TurnRight))
                {
                    if (!inputEvent.repeated)
                    {
                        if (inputEvent.action ==
                            rrr3d::input::Action::TurnLeft)
                        {
                            menuSelection =
                                menuSelection == 0U
                                    ? raceMenuIcons.size() - 1U
                                    : menuSelection - 1U;
                        }
                        else
                        {
                            menuSelection =
                                (menuSelection + 1U) %
                                raceMenuIcons.size();
                        }
                    }
                    continue;
                }
                if (menuStack.back() == MenuScreen::Garage &&
                    (inputEvent.action ==
                         rrr3d::input::Action::TurnLeft ||
                     inputEvent.action ==
                         rrr3d::input::Action::TurnRight))
                {
                    if (!inputEvent.repeated &&
                        !originalGarage->cars.empty())
                    {
                        if (inputEvent.action ==
                            rrr3d::input::Action::TurnLeft)
                        {
                            garageCarIndex =
                                garageCarIndex == 0U
                                    ? originalGarage->cars.size() - 1U
                                    : garageCarIndex - 1U;
                        }
                        else
                        {
                            garageCarIndex =
                                (garageCarIndex + 1U) %
                                originalGarage->cars.size();
                        }
                        refreshGaragePage();
                    }
                    continue;
                }
                const bool adjustableOptions =
                    menuStack.back() == MenuScreen::GameOptions ||
                    menuStack.back() ==
                        MenuScreen::GraphicsOptions ||
                    menuStack.back() == MenuScreen::SoundOptions;
                if (adjustableOptions &&
                    (inputEvent.action ==
                         rrr3d::input::Action::TurnLeft ||
                     inputEvent.action ==
                         rrr3d::input::Action::TurnRight))
                {
                    if (!inputEvent.repeated)
                    {
                        adjustCurrentOption(
                            inputEvent.action ==
                                    rrr3d::input::Action::TurnLeft
                                ? -1
                                : 1);
                    }
                    continue;
                }
                if (menuStack.back() ==
                        MenuScreen::ControlsOptions &&
                    (inputEvent.action ==
                         rrr3d::input::Action::TurnLeft ||
                     inputEvent.action ==
                         rrr3d::input::Action::TurnRight))
                {
                    if (!inputEvent.repeated &&
                        menuSelection <
                            originalControlActions.size())
                    {
                        controlsUseGamepad =
                            inputEvent.action ==
                            rrr3d::input::Action::TurnRight;
                    }
                    continue;
                }
#endif
                if (inputEvent.repeated)
                    continue;
                if (inputEvent.action ==
                        rrr3d::input::Action::MenuBack ||
                    inputEvent.action ==
                        rrr3d::input::Action::Pause)
                {
#ifdef RRR3D_AUDIO
                    playMainButtonClick();
#endif
                    if (
#ifdef RRR3D_PHYSICS
                        isOriginalOptionsScreen(menuStack.back()))
                    {
                        cancelOriginalOptions();
                    }
                    else if (
#endif
                        menuStack.size() == 1U)
                        running = false;
                    else
                        backMenu();
                    continue;
                }
                if (inputEvent.action !=
                    rrr3d::input::Action::MenuConfirm)
                    continue;
#ifdef RRR3D_AUDIO
                // MainMenu2 creates these buttons with ssButton1. The legacy
                // scheme plays click.ogg on press and has no navigation or
                // hover sound, so only a successful confirm reaches here.
                const bool clickStarted = playMainButtonClick();
#if defined(RRR3D_GAMEPAD_INPUT)
                if (options->audioSmokeTest && clickStarted &&
                    menuStack.back() == MenuScreen::Main &&
                    menuSelection == 1U)
                {
                    integratedAudioInputObserved = true;
                }
#endif
#endif
                std::cout << "MainMenu2 selection: "
                          << page.labels[menuSelection] << " via "
                          << rrr3d::input::sourceName(inputEvent.source)
                          << '\n';
                switch (menuStack.back())
                {
                case MenuScreen::Main:
                    if (menuSelection == 0U)
                        pushMenu(MenuScreen::GameMode);
                    else if (menuSelection == 1U)
                        pushMenu(MenuScreen::Network);
                    else if (menuSelection == 2U)
                    {
#ifdef RRR3D_PHYSICS
                        beginOriginalOptions();
#else
                        pushMenu(MenuScreen::Options);
#endif
                    }
                    else if (menuSelection == 3U)
                        pushMenu(MenuScreen::Credits);
                    else
                        running = false;
                    break;
                case MenuScreen::GameMode:
                    if (menuSelection == 0U)
                    {
                        championshipMode = true;
                        pushMenu(MenuScreen::Tournament);
                    }
                    else if (menuSelection == 1U)
                    {
                        championshipMode = false;
                        newTournamentProfile = false;
                        pushMenu(MenuScreen::Difficulty);
                    }
                    else
                        backMenu();
                    break;
                case MenuScreen::Tournament:
                    if (menuSelection == 0U)
                    {
#ifdef RRR3D_PHYSICS
                        showOriginalRaceMenu();
#endif
                    }
                    else if (menuSelection == 1U)
                    {
                        championshipMode = true;
                        newTournamentProfile = true;
                        pushMenu(MenuScreen::Difficulty);
                    }
                    else if (menuSelection == 2U)
                        pushMenu(MenuScreen::Profiles);
                    else
                        backMenu();
                    break;
                case MenuScreen::Difficulty:
                    if (menuSelection >= 3U)
                    {
                        backMenu();
                        break;
                    }
#ifdef RRR3D_PHYSICS
                    profileState.player.difficulty =
                        std::array<std::string, 3>{
                            "gdEasy", "gdNormal", "gdHard"}
                            [menuSelection];
                    if (championshipMode && newTournamentProfile)
                    {
                        const auto profileName =
                            profileState.player.name;
                        const auto color =
                            profileState.player.color;
                        // Profile::Enter resets the source subsystems and
                        // SnProfile::EnterGame then opens planet zero.  Reuse
                        // the portable equivalent instead of a value-
                        // initialized PlayerProfile, whose planets are all
                        // psUnavailable and whose Workshop would stay empty.
                        profileState.player =
                            r3d::game::originalrace::
                                makeOriginalDefaultProfileState()
                                    .player;
                        profileState.player.name = profileName;
                        profileState.player.color = color;
                        profileState.player.difficulty =
                            std::array<std::string, 3>{
                                "gdEasy", "gdNormal", "gdHard"}
                                [menuSelection];
                        selectedTrack = 0U;
                    }
                    saveRaceProfile();
                    showOriginalRaceMenu();
#endif
                    break;
                case MenuScreen::Profiles:
                    if (menuSelection + 1U >= page.labels.size())
                        backMenu();
                    else
                    {
#ifdef RRR3D_PHYSICS
                        // Persist the profile being left before replacing
                        // PlayerProfile.  Merely changing its name used to
                        // save the current car/progress into another file
                        // and never loaded the chosen Windows profile.
                        saveRaceProfile();
                        std::string profileError;
                        if (!profileStore.selectProfile(
                                profileState,
                                page.labels[menuSelection],
                                profileError))
                        {
                            std::cerr
                                << "Unable to load original profile: "
                                << profileError << '\n';
                            break;
                        }
                        selectedTrack =
                            r3d::game::originalrace::
                                resolveOriginalTournamentTrack(
                                    *originalRace,
                                    profileState.player);
                        if (!reloadCurrentRace())
                        {
                            runtimeSmokeFailed = true;
                            running = false;
                            break;
                        }
                        input.applyKeyboardBindings(
                            profileState.config.keyboardControls);
                        input.applyGamepadBindings(
                            profileState.config.gamepadControls);
                        saveRaceProfile();
#endif
                        backMenu();
                    }
                    break;
                case MenuScreen::Network:
                    if (menuSelection + 1U >= page.labels.size())
                        backMenu();
                    else
                        std::cout
                            << "Network mode requires the pending "
                               "non-Windows NetLib transport port\n";
                    break;
                case MenuScreen::Options:
#ifdef RRR3D_PHYSICS
                    beginOriginalOptions();
#else
                    backMenu();
#endif
                    break;
                case MenuScreen::Credits:
                    backMenu();
                    break;
#ifdef RRR3D_PHYSICS
                case MenuScreen::RaceMenu:
                    if (menuSelection == 0U)
                    {
                        startCurrentRace();
                    }
                    else if (menuSelection == 1U)
                    {
                        workshopDrag = {};
                        workshopGoodScroll = 0U;
                        pushMenu(MenuScreen::Workshop);
                        refreshWorkshopPage();
                    }
                    else if (menuSelection == 2U)
                    {
                        rebuildGarageCarOrder();
                        refreshGaragePage();
                        pushMenu(MenuScreen::Garage);
                    }
                    else if (menuSelection == 3U)
                    {
                        pushMenu(MenuScreen::Planets);
                        const auto planetCount = std::min(
                            originalGarage->planets.size(),
                            profileState.player.planets.size());
                        if (racePlanetChampion &&
                            profileState.player.currentPlanet + 1U <
                                planetCount)
                        {
                            angarPlanetIndex = static_cast<int>(
                                profileState.player.currentPlanet + 1U);
                            menuSelection =
                                static_cast<std::size_t>(
                                    angarPlanetIndex);
                        }
                        else
                        {
                            angarPlanetIndex = -1;
                            menuSelection = planetCount;
                        }
                        angarPreviousPlanetIndex = -1;
                        angarDoorTime = -1.0F;
                        angarTravelDialogVisible = false;
                        angarWarningVisible = false;
                        refreshPlanetsPage();
                    }
                    else if (menuSelection == 4U)
                    {
                        achievementPurchaseDialogVisible = false;
                        achievementPointsWarningVisible = false;
                        achievementPendingPurchase =
                            originalAchievementNoTarget;
                        refreshAchievementsPage();
                        pushMenu(MenuScreen::Achievements);
                    }
                    else if (menuSelection == 5U)
                    {
                        beginOriginalOptions();
                    }
                    else
                    {
                        saveRaceProfile();
                        menuStack = {MenuScreen::Main};
                        menuSelection = 0;
                    }
                    break;
                case MenuScreen::Garage:
                    if (menuSelection == 0U)
                    {
                        garageCarIndex =
                            (garageCarIndex + 1U) %
                            originalGarage->cars.size();
                        refreshGaragePage();
                    }
                    else if (menuSelection == 3U)
                    {
                        const auto& car =
                            originalGarage->cars[garageCarIndex];
                        std::string garageError;
                        if (!r3d::game::originalrace::
                                selectOriginalGarageCar(
                                    *originalGarage, profileState,
                                    car, championshipMode,
                                    garageError))
                        {
                            std::cerr
                                << "Original GarageFrame: "
                                << garageError << '\n';
                        }
                        else
                        {
                            saveRaceProfile();
                        }
                        refreshGaragePage();
                    }
                    else if (menuSelection + 1U >=
                             page.labels.size())
                    {
                        backMenu();
                    }
                    break;
                case MenuScreen::Workshop:
                    break;
                case MenuScreen::Planets:
                    break;
                case MenuScreen::Achievements:
                    break;
                case MenuScreen::GameOptions:
                    if (menuSelection == 12U)
                        cancelOriginalOptions();
                    else if (menuSelection == 13U)
                        applyOriginalOptions();
                    else
                        adjustCurrentOption(1);
                    break;
                case MenuScreen::GraphicsOptions:
                    if (menuSelection == 8U)
                        cancelOriginalOptions();
                    else if (menuSelection == 9U)
                        applyOriginalOptions();
                    else
                        adjustCurrentOption(1);
                    break;
                case MenuScreen::SoundOptions:
                    if (menuSelection == 5U)
                        cancelOriginalOptions();
                    else if (menuSelection == 6U)
                        applyOriginalOptions();
                    else
                        adjustCurrentOption(1);
                    break;
                case MenuScreen::ControlsOptions:
                {
                    if (menuSelection <
                        originalControlActions.size())
                    {
                        bindingCaptureAction =
                            originalControlActions[menuSelection];
                        bindingCaptureGamepad = controlsUseGamepad;
                        std::cout
                            << "Original ControlsFrame: "
                            << localized("svPressKey") << " ("
                            << *bindingCaptureAction << ")\n";
                    }
                    else if (
                        menuSelection ==
                        originalControlActions.size())
                        cancelOriginalOptions();
                    else
                        applyOriginalOptions();
                    break;
                }
                case MenuScreen::Finish:
                    if (menuSelection == 0U)
                    {
                        menuStack =
                            championshipMode
                                ? std::vector<MenuScreen>{
                                      MenuScreen::Main,
                                      MenuScreen::GameMode,
                                      MenuScreen::Tournament,
                                      MenuScreen::RaceMenu}
                                : std::vector<MenuScreen>{
                                      MenuScreen::Main,
                                      MenuScreen::GameMode,
                                      MenuScreen::RaceMenu};
                        menuSelection = 0;
                    }
                    else
                    {
                        menuStack = {MenuScreen::Main};
                        menuSelection = 0;
                    }
                    break;
#endif
                }
            }
#endif
            if (event.type == SDL_EVENT_QUIT ||
                event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                running = false;
            }
            else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
            {
                pixelWidth = std::max(event.window.data1, 1);
                pixelHeight = std::max(event.window.data2, 1);
                device->resize(static_cast<std::uint32_t>(pixelWidth),
                               static_cast<std::uint32_t>(pixelHeight));
#ifdef RRR3D_PHYSICS
                std::string resizeError;
                if (!raceRenderer.resize(
                        *device,
                        static_cast<std::uint32_t>(pixelWidth),
                        static_cast<std::uint32_t>(pixelHeight),
                        resizeError) ||
                    !garageRenderer.resize(
                        *device,
                        static_cast<std::uint32_t>(pixelWidth),
                        static_cast<std::uint32_t>(pixelHeight),
                        resizeError) ||
                    !angarRenderer.resize(
                        *device,
                        static_cast<std::uint32_t>(pixelWidth),
                        static_cast<std::uint32_t>(pixelHeight),
                        resizeError))
                {
                    std::cerr
                        << "Unable to resize M9.3 render targets: "
                        << resizeError << '\n';
                    running = false;
                }
#endif
            }
        }

#ifdef RRR3D_PHYSICS
        const std::uint64_t currentFrameTicks = SDL_GetTicksNS();
        float frameSeconds = std::clamp(
            static_cast<float>(currentFrameTicks - previousFrameTicks) /
                1000000000.0F,
            0.0F, 0.1F);
        if (options->raceRenderSmokeTest)
            frameSeconds = 1.0F / 60.0F;
        previousFrameTicks = currentFrameTicks;
        if (inRace)
        {
            // Windows ControlManager::GetGameActionState is polled every
            // update.  Keeping only KEY_DOWN/KEY_UP events here lost an
            // already-held accelerator across menu/race, countdown, pause,
            // and focus transitions.
            raceInput.throttle = input.heldValue(
                rrr3d::input::Action::Accelerate);
            raceInput.reverse = input.heldValue(
                rrr3d::input::Action::Brake);
            raceInput.brake = 0.0F;
            raceInput.steering =
                input.heldValue(rrr3d::input::Action::TurnLeft) -
                input.heldValue(rrr3d::input::Action::TurnRight);
            if (options->raceRenderSmokeTest)
            {
                raceInput.steering = renderedFrames >= 90 &&
                                             renderedFrames < 180
                                         ? 0.35F
                                         : 0.0F;
            }
            r3d::game::originalrace::RaceControl control;
            control.driving = raceInput;
            control.useWeapon = raceUseWeaponRequested;
            control.useAllWeapons = raceUseAllWeaponsRequested;
            control.useMine = raceUseMine;
            control.useHyper = raceUseHyper;
            control.changeWeapon = raceChangeWeaponRequested;
            control.weaponChange = raceWeaponChangeDirection;
            control.fireWeaponSlot = raceFireWeaponSlotRequested;
            control.reset = raceResetRequested;
            raceSession.update(frameSeconds, raceVehicles, control);
            if (options->raceRenderSmokeTest &&
                !raceSession.racers().empty())
                minimumRacePlayerLife = std::min(
                    minimumRacePlayerLife,
                    raceSession.racers().front().life);
            raceUseWeaponRequested = false;
            raceUseAllWeaponsRequested = false;
            raceChangeWeaponRequested = false;
            raceWeaponChangeDirection = 1;
            raceFireWeaponSlotRequested = -1;
            raceResetRequested = false;
            for (const auto& event : raceSession.events())
            {
                if (options->raceRenderSmokeTest &&
                    event.kind ==
                        r3d::game::originalrace::RaceEventKind::Kill &&
                    event.target == 0U)
                    racePlayerDestroyedObserved = true;
                if (event.kind ==
                        r3d::game::originalrace::RaceEventKind::Kill &&
                    event.target < originalRace->racers.size() &&
                    event.target < raceVehicles.size())
                {
                    const auto& sourceRacer =
                        originalRace->racers[event.target];
                    const auto& vehicle =
                        sourceRacer.hasConfiguredVehicle
                            ? sourceRacer.configuredVehicle
                            : originalRace->vehicles.at(
                                  sourceRacer.vehicle);
                    for (std::size_t effectIndex = 0;
                         effectIndex < vehicle.deathEffects.size();
                         ++effectIndex)
                    {
                        const auto& effect =
                            vehicle.deathEffects[effectIndex];
                        if (!effect.visual.dynamicBody)
                            continue;
                        const auto runtimeEffect = std::find_if(
                            raceSession.effects().begin(),
                            raceSession.effects().end(),
                            [&](const auto& value) {
                                return value.kind ==
                                           r3d::game::originalrace::
                                               RaceEventKind::
                                                   VehicleDestroyed &&
                                       value.racer == event.target &&
                                       value.vehicleEffect == effectIndex;
                            });
                        r3d::physics::DebrisDescription debris;
                        debris.transform =
                            runtimeEffect != raceSession.effects().end()
                                ? runtimeEffect->transform
                                : raceVehicles[event.target].body;
                        debris.shapePosition =
                            effect.visual.bodyShapePosition;
                        debris.shapeRotation =
                            effect.visual.bodyShapeRotation;
                        debris.halfExtents =
                            effect.visual.bodyHalfExtents;
                        debris.localImpulse = effect.impulse;
                        debris.mass = effect.visual.bodyMass;
                        debris.lifetime =
                            effect.visual.maximumTimeLife;
                        const auto debrisIndex =
                            physicsWorld->addDebris(debris);
                        if (debrisIndex ==
                            std::numeric_limits<std::size_t>::max())
                            continue;
                        vehicleDebrisBindings.push_back(
                            {event.target, effectIndex, debrisIndex});
                    }
                    physicsWorld->setVehicleEnabled(
                        event.target, false);
                }
                if (event.kind !=
                        r3d::game::originalrace::RaceEventKind::
                            DecorationDestroyed ||
                    event.target >=
                        originalRace->decorationInstances.size())
                    continue;
                const auto& instance =
                    originalRace->decorationInstances[event.target];
                const auto& definition =
                    originalRace->decorationDefinitions.at(
                        instance.definition);
                for (std::size_t pieceIndex = 0;
                     pieceIndex < definition.destructionPieces.size();
                     ++pieceIndex)
                {
                    const auto& piece =
                        definition.destructionPieces[pieceIndex];
                    if (!piece.dynamic)
                        continue;
                    r3d::physics::DebrisDescription debris;
                    debris.transform = instance.transform;
                    debris.shapePosition = {
                        piece.shapePosition.x *
                            instance.transform.scale.x,
                        piece.shapePosition.y *
                            instance.transform.scale.y,
                        piece.shapePosition.z *
                            instance.transform.scale.z};
                    debris.shapeRotation = piece.shapeRotation;
                    debris.halfExtents = {
                        std::abs(piece.halfExtents.x *
                                 instance.transform.scale.x),
                        std::abs(piece.halfExtents.y *
                                 instance.transform.scale.y),
                        std::abs(piece.halfExtents.z *
                                 instance.transform.scale.z)};
                    debris.mass = piece.mass;
                    const auto debrisIndex =
                        physicsWorld->addDebris(debris);
                    if (debrisIndex ==
                        std::numeric_limits<std::size_t>::max())
                        continue;
                    decorationDebrisBindings.push_back(
                        {event.target, pieceIndex, debrisIndex});
                }
            }
#ifdef RRR3D_AUDIO
            if (!raceVehicles.empty())
            {
                const auto listener =
                    raceVehicles.front().body.position;
                const auto listenerRotation =
                    raceVehicles.front().body.rotation;
                const r3d::physics::Vec3 listenerRight{
                    2.0F * (listenerRotation.x * listenerRotation.y -
                            listenerRotation.w * listenerRotation.z),
                    1.0F -
                        2.0F *
                            (listenerRotation.x * listenerRotation.x +
                             listenerRotation.z * listenerRotation.z),
                    0.0F};
                auto playSpatial =
                    [&](r3d::audio::SoundHandle sound,
                        const r3d::physics::Vec3& source,
                        const r3d::physics::Vec3& sourceVelocity,
                        float gain) {
                    if (sound == r3d::audio::invalidSound)
                        return;
                    const float dx = source.x - listener.x;
                    const float dy = source.y - listener.y;
                    const float distance =
                        std::sqrt(dx * dx + dy * dy);
                    const float attenuation =
                        std::clamp(1.0F - distance / 70.0F,
                                   0.0F, 1.0F);
                    float pan = 0.0F;
                    if (distance > 0.001F)
                        pan = std::clamp(
                            (dx * listenerRight.x +
                             dy * listenerRight.y) /
                                distance,
                            -1.0F, 1.0F);
                    float pitch = 1.0F;
                    if (distance > 0.001F)
                    {
                        constexpr float speedOfSound = 343.0F;
                        const float nx = dx / distance;
                        const float ny = dy / distance;
                        const auto& listenerVelocity =
                            raceVehicles.front().linearVelocity;
                        const float listenerRadial =
                            listenerVelocity.x * nx +
                            listenerVelocity.y * ny;
                        const float sourceRadial =
                            sourceVelocity.x * nx +
                            sourceVelocity.y * ny;
                        pitch = std::clamp(
                            (speedOfSound + listenerRadial) /
                                std::max(speedOfSound + sourceRadial,
                                         1.0F),
                            0.8F, 1.25F);
                    }
                    r3d::audio::PlayOptions playOptions;
                    playOptions.bus = r3d::audio::Bus::Effects;
                    playOptions.volume = gain * attenuation;
                    const auto voice =
                        audio.play(sound, playOptions, audioError);
                    if (voice != r3d::audio::invalidVoice)
                        audio.setVoiceParameters(
                            voice, gain * attenuation, pitch, pan);
                };
                auto eventVelocity =
                    [&](std::size_t racer) {
                        return racer < raceVehicles.size()
                                   ? raceVehicles[racer].linearVelocity
                                   : r3d::physics::Vec3{};
                    };
                for (auto& cooldown : damageAudioCooldown)
                    cooldown =
                        std::max(0.0F, cooldown - frameSeconds);
                for (const auto& event : raceSession.events())
                {
                    if (event.kind ==
                            r3d::game::originalrace::RaceEventKind::
                                WeaponFired &&
                        event.racer < raceSession.racers().size())
                    {
                        const auto weapon = event.weapon;
                        if (weapon < weaponAudio.size())
                            playSpatial(weaponAudio[weapon],
                                        event.position,
                                        eventVelocity(event.racer),
                                        0.9F);
                    }
                    else if (event.kind ==
                                 r3d::game::originalrace::RaceEventKind::
                                     MinePlaced &&
                             event.target < weaponAudio.size())
                    {
                        playSpatial(weaponAudio[event.target],
                                    event.position,
                                    eventVelocity(event.racer), 0.8F);
                    }
                    else if (event.kind ==
                                 r3d::game::originalrace::RaceEventKind::
                                     HyperActivated &&
                             event.target < weaponAudio.size())
                    {
                        playSpatial(weaponAudio[event.target],
                                    event.position,
                                    eventVelocity(event.racer), 0.9F);
                    }
                    else if (event.kind ==
                                 r3d::game::originalrace::RaceEventKind::
                                     Bonus &&
                             event.target < bonusDeathAudio.size())
                    {
                        playSpatial(
                            bonusDeathAudio[event.target],
                            event.position,
                            eventVelocity(event.racer), 0.75F);
                    }
                    else if (event.kind ==
                                 r3d::game::originalrace::RaceEventKind::
                                     Damage)
                    {
                        if (event.racer <
                                damageAudioCooldown.size() &&
                            damageAudioCooldown[event.racer] > 0.0F)
                            continue;
                        if (event.racer <
                            damageAudioCooldown.size())
                            damageAudioCooldown[event.racer] =
                                event.touchDamage ? 0.2F : 0.08F;
                        const auto sound =
                            event.touchDamage
                                ? crashAudio
                                : impactAudio[
                                      (event.racer + event.target) %
                                      impactAudio.size()];
                        playSpatial(
                            sound, event.position,
                            eventVelocity(event.racer),
                            event.touchDamage ? 0.75F : 0.62F);
                    }
                    else if (event.kind ==
                             r3d::game::originalrace::RaceEventKind::Kill)
                    {
                        playSpatial(
                            destructionAudio, event.position,
                            eventVelocity(event.target),
                            0.9F);
                    }
                    else if (event.kind ==
                                 r3d::game::originalrace::RaceEventKind::
                                     DecorationDestroyed)
                    {
                        playSpatial(
                            crashAudio, event.position,
                            eventVelocity(event.racer), 0.72F);
                    }
                }
                commentator.update(
                    *originalRace, raceSession, audioError);
            }
#endif
            if (raceSession.phase() !=
                r3d::game::originalrace::RacePhase::Paused)
            {
                for (const auto& respawn : raceSession.takeRespawns())
                    physicsWorld->resetVehicle(
                        respawn.racer, respawn.position,
                        respawn.direction);
                for (const auto& velocity :
                     raceSession.takeVelocityRequests())
                {
                    physicsWorld->addLinearVelocity(
                        velocity.racer, velocity.delta);
                }
                for (const auto& velocity :
                     raceSession.takeAngularVelocityRequests())
                {
                    physicsWorld->addAngularVelocity(
                        velocity.racer, velocity.delta);
                }
                for (std::size_t racer = 0;
                     racer < raceSession.racers().size(); ++racer)
                {
                    physicsWorld->setWheelTractionEnabled(
                        racer,
                        raceSession.racers()[racer].clutchSeconds <= 0.0F);
                    if (raceSession.racers()[racer].slowSeconds > 0.0F)
                        physicsWorld->clampLinearSpeed(racer, 20.0F);
                }
                physicsWorld->step(
                    frameSeconds, raceSession.vehicleInputs());
                for (std::size_t index = 0;
                     index < physicsWorld->vehicleCount(); ++index)
                    raceVehicles[index] = physicsWorld->vehicle(index);
            }
            decorationFragments.clear();
            decorationFragments.reserve(
                decorationDebrisBindings.size());
            for (const auto& binding : decorationDebrisBindings)
            {
                if (binding.debris >= physicsWorld->debrisCount())
                    continue;
                const auto& debris =
                    physicsWorld->debris(binding.debris);
                if (!debris.active)
                    continue;
                decorationFragments.push_back(
                    {binding.instance, binding.piece,
                     debris.body});
            }
            vehicleDeathFragments.clear();
            vehicleDeathFragments.reserve(
                vehicleDebrisBindings.size());
            for (const auto& binding : vehicleDebrisBindings)
            {
                if (binding.debris >= physicsWorld->debrisCount())
                    continue;
                const auto& debris =
                    physicsWorld->debris(binding.debris);
                if (!debris.active)
                    continue;
                vehicleDeathFragments.push_back(
                    {binding.racer, binding.effect, debris.body});
            }
            raceElapsedSeconds = raceSession.elapsedSeconds();
#ifdef RRR3D_AUDIO
            if (!raceVehicles.empty())
            {
                const auto& listener =
                    raceVehicles.front().body.position;
                const auto& rotation =
                    raceVehicles.front().body.rotation;
                const r3d::physics::Vec3 listenerRight{
                    2.0F * (rotation.x * rotation.y -
                            rotation.w * rotation.z),
                    1.0F - 2.0F *
                               (rotation.x * rotation.x +
                                rotation.z * rotation.z),
                    0.0F};
                const bool audioPaused =
                    raceSession.phase() ==
                    r3d::game::originalrace::RacePhase::Paused;
                for (std::size_t racer = 0;
                     racer < engineAudio.size() &&
                     racer < raceVehicles.size(); ++racer)
                {
                    const auto& definition =
                        originalRace->vehicles.at(
                            originalRace->racers[racer].vehicle);
                    const float rpm = std::clamp(
                        raceVehicles[racer].engineRpm /
                            std::max(
                                definition.physics.maximumRpm, 1.0F),
                        0.0F, 1.0F);
                    float attenuation = 1.0F;
                    float pan = 0.0F;
                    if (racer != 0)
                    {
                        const auto& source =
                            raceVehicles[racer].body.position;
                        const float dx = source.x - listener.x;
                        const float dy = source.y - listener.y;
                        const float distance =
                            std::sqrt(dx * dx + dy * dy);
                        attenuation =
                            std::clamp(1.0F - distance / 50.0F,
                                       0.0F, 1.0F) *
                            0.65F;
                        if (distance > 0.001F)
                            pan = std::clamp(
                                (dx * listenerRight.x +
                                 dy * listenerRight.y) /
                                    distance,
                                -1.0F, 1.0F);
                    }
                    const float idleVolume =
                        attenuation * (0.55F - rpm * 0.42F);
                    const float rpmVolume =
                        attenuation * (0.10F + rpm * 0.58F);
                    const float pitch = 0.70F + rpm * 0.65F;
                    audio.setVoiceParameters(
                        engineAudio[racer].idleVoice, idleVolume,
                        0.92F + rpm * 0.12F, pan);
                    audio.setVoiceParameters(
                        engineAudio[racer].rpmVoice, rpmVolume, pitch,
                        pan);
                    audio.setVoicePaused(
                        engineAudio[racer].idleVoice, audioPaused);
                    audio.setVoicePaused(
                        engineAudio[racer].rpmVoice, audioPaused);
                }
            }
#endif
            maximumRaceSmokeSpeed = std::max(
                maximumRaceSmokeSpeed, physicsWorld->vehicle().speed);
            maximumRaceSmokeContacts = std::max(
                maximumRaceSmokeContacts,
                physicsWorld->vehicle().contactCount);
            if (!raceSession.racers().empty() &&
                raceSession.racers().front().finished)
                showFinishMenu();
        }
#endif

#ifdef RRR3D_AUDIO
#ifdef RRR3D_PHYSICS
        if (inRace && !gameMusic.update(audioError))
        {
            std::cerr << "Original game MusicCat runtime failed: "
                      << audioError << '\n';
            runtimeSmokeFailed = true;
            running = false;
        }
#endif
        if (!music.update(audioError))
        {
            std::cerr << "Original MusicCat runtime failed: " << audioError
                      << '\n';
            runtimeSmokeFailed = true;
            running = false;
        }

        if (running && options->audioSmokeTest &&
            musicSmokePhase != MusicSmokePhase::Complete)
        {
            if (SDL_GetTicks() >= musicSmokeDeadline)
            {
                std::cerr << "Milestone 8 MusicCat transition smoke timed "
                             "out while the render loop remained active\n";
                runtimeSmokeFailed = true;
                running = false;
            }
            else if (musicSmokePhase ==
                         MusicSmokePhase::WaitingForDecode &&
                     music.allTracksLoaded() &&
                     music.currentVoiceActive())
            {
                bool metadataValid = renderedFrames > 1;
                for (std::size_t index = 0;
                     index < originalaudio::menuTracks.size(); ++index)
                {
                    const auto* info = music.trackInfo(index);
                    metadataValid = metadataValid && info != nullptr &&
                                    info->sourceSampleRate == 44100 &&
                                    info->sourceChannels == 2 &&
                                    info->durationSeconds > 60.0 &&
                                    info->mixerFrames != 0 &&
                                    info->peakAmplitude > 0.0F &&
                                    info->rmsAmplitude > 0.0F;
                }
                const auto current = music.currentTrack();
                if (!metadataValid || !current ||
                    !music.pause(true, audioError))
                {
                    std::cerr << "Milestone 8 background decode/pause "
                                 "verification failed: "
                              << (audioError.empty()
                                      ? "invalid three-track metadata"
                                      : audioError)
                              << '\n';
                    runtimeSmokeFailed = true;
                    running = false;
                }
                else
                {
                    smokeTrackOrder[0] = *current;
                    smokePausePosition =
                        music.currentPositionFrames();
                    smokePhaseTicks = SDL_GetTicks();
                    musicSmokePhase =
                        MusicSmokePhase::WaitingWhilePaused;
                }
            }
            else if (musicSmokePhase ==
                         MusicSmokePhase::WaitingWhilePaused &&
                     SDL_GetTicks() - smokePhaseTicks >= 100)
            {
                if (!music.paused() ||
                    music.currentPositionFrames() != smokePausePosition ||
                    !music.pause(false, audioError))
                {
                    std::cerr << "Milestone 8 MusicCat pause/resume "
                                 "verification failed: "
                              << audioError << '\n';
                    runtimeSmokeFailed = true;
                    running = false;
                }
                else
                {
                    musicSmokePhase =
                        MusicSmokePhase::WaitingForResume;
                }
            }
            else if (musicSmokePhase ==
                         MusicSmokePhase::WaitingForResume &&
                     music.currentPositionFrames() > smokePausePosition)
            {
                const auto* info =
                    music.trackInfo(smokeTrackOrder[0]);
                const std::uint64_t tailFrames = 4800;
                if (info == nullptr || info->mixerFrames <= tailFrames ||
                    !music.seekCurrent(info->mixerFrames - tailFrames,
                                       audioError))
                {
                    std::cerr << "Milestone 8 MusicCat resume/automatic-Next "
                                 "setup failed: "
                              << audioError << '\n';
                    runtimeSmokeFailed = true;
                    running = false;
                }
                else
                {
                    musicSmokePhase =
                        MusicSmokePhase::WaitingForAutomaticNext;
                }
            }
            else if (musicSmokePhase ==
                         MusicSmokePhase::WaitingForAutomaticNext)
            {
                const auto current = music.currentTrack();
                if (current && *current != smokeTrackOrder[0] &&
                    music.currentVoiceActive())
                {
                    smokeTrackOrder[1] = *current;
                    if (music.transitionCount() == 0 ||
                        !music.next(audioError))
                    {
                        std::cerr << "Milestone 8 MusicCat automatic/manual "
                                     "Next verification failed: "
                                  << audioError << '\n';
                        runtimeSmokeFailed = true;
                        running = false;
                    }
                    else
                    {
                        musicSmokePhase =
                            MusicSmokePhase::WaitingForManualNext;
                    }
                }
            }
            else if (musicSmokePhase ==
                         MusicSmokePhase::WaitingForManualNext)
            {
                const auto current = music.currentTrack();
                if (current && *current != smokeTrackOrder[1] &&
                    music.currentVoiceActive())
                {
                    smokeTrackOrder[2] = *current;
                    const bool allDistinct =
                        smokeTrackOrder[0] != smokeTrackOrder[1] &&
                        smokeTrackOrder[0] != smokeTrackOrder[2] &&
                        smokeTrackOrder[1] != smokeTrackOrder[2];
                    if (!allDistinct ||
                        !music.verifySavedState(audioError))
                    {
                        std::cerr << "Milestone 8 MusicCat shuffle/state "
                                     "verification failed: "
                                  << (audioError.empty()
                                          ? "track order repeated"
                                          : audioError)
                                  << '\n';
                        runtimeSmokeFailed = true;
                        running = false;
                    }
                    else
                    {
                        musicSmokePhase = MusicSmokePhase::Complete;
                        std::cout
                            << "Milestone 8 MusicCat follow-up smoke: all "
                               "three original menu tracks decoded in the "
                               "background while rendering; shuffle, pause/"
                               "resume, automatic Next, manual Next, and "
                               "state round-trip passed\n";
                    }
                }
            }
        }
#endif

#ifdef RRR3D_PHYSICS
        if (inRace)
        {
            const float raceRenderSeconds =
                exitRaceDialogVisible ? 0.0F : frameSeconds;
            auto cameraStyle =
                profileState.config.preferredCamera;
            if (options->raceRenderSmokeTest)
            {
                cameraStyle =
                    renderedFrames < options->smokeFrames / 2U
                        ? r3d::game::originalrace::
                              PreferredCamera::Isometric
                        : r3d::game::originalrace::
                              PreferredCamera::ThirdPerson;
                raceCameraStylesObserved[
                    cameraStyle ==
                            r3d::game::originalrace::
                                PreferredCamera::ThirdPerson
                        ? 0U
                        : 1U] = true;
            }
            const auto raceCamera = raceRenderer.makeCamera(
                *device, physicsWorld->vehicle(),
                static_cast<std::uint32_t>(pixelWidth),
                static_cast<std::uint32_t>(pixelHeight),
                cameraStyle,
                profileState.config.cameraDistance, raceRenderSeconds);
            raceRenderer.renderFrame(
                *device, raceShader, raceCamera, 0x6b91b8ffU,
                *originalRace, raceVehicles, racePipeline,
                raceSession.decorationActive(),
                decorationFragments, vehicleDeathFragments,
                raceSession.bonusActive(),
                raceSession.racers(),
                raceSession.effects(), raceSession.mines(),
                raceSession.projectiles(), raceElapsedSeconds,
                profileState.config.quality);
            raceHud.update(*device, *originalRace, raceSession,
                           raceVehicles, raceCamera, raceRenderSeconds);
            device->beginOverlay(camera);
            if (profileState.config.enableHud)
                raceHud.draw(*device, quad, shader, raceShader);
            if (exitRaceDialogVisible)
            {
                const float centerX = menu::virtualWidth * 0.5F;
                const float centerY = menu::virtualHeight * 0.5F;
                drawQuad(
                    *device, quad, shader, acceptFrame,
                    static_cast<float>(acceptFrameImage.width),
                    static_cast<float>(acceptFrameImage.height),
                    centerX, centerY, 15.0F, transparent);
                const float messageScale = std::min(
                    {1.0F,
                     325.0F / std::max(exitRaceMessage.width, 1.0F),
                     65.0F / std::max(exitRaceMessage.height, 1.0F)});
                drawQuad(
                    *device, quad, shader, exitRaceMessage.texture,
                    exitRaceMessage.width * messageScale,
                    exitRaceMessage.height * messageScale,
                    centerX, centerY - 25.0F, 8.0F, transparent);
                auto drawChoice = [&](bool yes, float x) {
                    const bool selectedChoice =
                        exitRaceYesFocused == yes;
                    drawQuad(
                        *device, quad, shader,
                        selectedChoice ? acceptButtonSelected
                                       : acceptButton,
                        static_cast<float>(acceptButtonImage.width),
                        static_cast<float>(acceptButtonImage.height),
                        x, centerY + 32.0F, 7.0F, transparent);
                    const auto& label =
                        yes ? (selectedChoice
                                   ? exitRaceYesSelected
                                   : exitRaceYes)
                            : (selectedChoice
                                   ? exitRaceNoSelected
                                   : exitRaceNo);
                    drawQuad(
                        *device, quad, shader, label.texture,
                        label.width, label.height, x,
                        centerY + 32.0F, 4.0F, transparent);
                };
                drawChoice(true, centerX - 70.0F);
                drawChoice(false, centerX + 70.0F);
            }
            device->endFrame();
            const auto& telemetry = device->renderTelemetry();
            for (std::size_t pass = 0;
                 pass < r3d::renderer::renderPassCount; ++pass)
            {
                maximumRacePassBegins[pass] = std::max(
                    maximumRacePassBegins[pass],
                    telemetry.beginCount[pass]);
                maximumRacePassDraws[pass] = std::max(
                    maximumRacePassDraws[pass],
                    telemetry.drawCount[pass]);
            }
            for (std::size_t mode = 0;
                 mode < maximumRaceLightingDraws.size(); ++mode)
            {
                maximumRaceLightingDraws[mode] = std::max(
                    maximumRaceLightingDraws[mode],
                    telemetry.lightingDrawCount[mode]);
            }
            maximumEnvironmentMappedDraws = std::max(
                maximumEnvironmentMappedDraws,
                telemetry.environmentMappedDrawCount);
            maximumNormalMappedDraws = std::max(
                maximumNormalMappedDraws,
                telemetry.normalMappedDrawCount);
            maximumTransientDraws = std::max(
                maximumTransientDraws,
                telemetry.transientDrawCount);
        }
        else
        {
#endif
#ifdef RRR3D_PHYSICS
        const bool drawingOriginalOptions =
            isOriginalOptionsScreen(menuStack.back());
        const bool drawingOriginalRaceMenu =
            menuStack.back() == MenuScreen::RaceMenu;
        const bool drawingOriginalGarage =
            menuStack.back() == MenuScreen::Garage;
        const bool drawingOriginalWorkshop =
            menuStack.back() == MenuScreen::Workshop;
        const bool drawingOriginalAngar =
            menuStack.back() == MenuScreen::Planets;
        const bool drawingOriginalAchievements =
            menuStack.back() == MenuScreen::Achievements;
        const r3d::game::originalrace::OriginalGarageCar*
            presentationCar = nullptr;
        bool presentationCarLocked = false;
        if (drawingOriginalGarage && !garageCarOrder.empty())
        {
            const auto& selectedView =
                garageCarOrder[garageViewIndex];
            presentationCar =
                &originalGarage->cars[selectedView.catalogIndex];
            presentationCarLocked = selectedView.locked;
        }
        else if (drawingOriginalWorkshop)
        {
            presentationCar = originalGarage->findCar(
                profileState.player.currentCar);
        }
        if (drawingOriginalAngar)
        {
            angarSceneSeconds += frameSeconds;
            const float redTime =
                std::fmod(angarSceneSeconds, 3.0F);
            const float redIntensity = 0.7F * (
                std::clamp(
                    (redTime - 1.5F) / 0.15F, 0.0F, 1.0F) -
                std::clamp(
                    (redTime - 2.85F) / 0.15F, 0.0F, 1.0F));
            auto& redLamp = originalAngarScene->environment.lamps[1];
            redLamp.enabled = redTime >= 1.5F;
            redLamp.color = {
                redIntensity, 0.0F, 0.0F, redIntensity};
            auto angarSourceCamera =
                originalAngarScene->presentationCamera;
            if (angarSceneSeconds >= 3.0F)
            {
                // CameraManager::csAutoObserver waits three seconds, then
                // left-multiplies the source camera by dRotZ at pi/96 rad/s
                // and recomputes position from the exact 50-unit target.
                const float angle =
                    (angarSceneSeconds - 3.0F) *
                    bx::kPi / 96.0F;
                const float sine = std::sin(angle);
                const float cosine = std::cos(angle);
                const auto base =
                    originalAngarScene->presentationCamera;
                angarSourceCamera.position = {
                    cosine * base.position.x -
                        sine * base.position.y,
                    sine * base.position.x +
                        cosine * base.position.y,
                    base.position.z};
                const float halfSine =
                    std::sin(angle * 0.5F);
                const float halfCosine =
                    std::cos(angle * 0.5F);
                angarSourceCamera.rotation = {
                    halfCosine * base.rotation.x -
                        halfSine * base.rotation.y,
                    halfCosine * base.rotation.y +
                        halfSine * base.rotation.x,
                    halfCosine * base.rotation.z +
                        halfSine * base.rotation.w,
                    halfCosine * base.rotation.w -
                        halfSine * base.rotation.z};
            }
            const auto angarCamera =
                angarRenderer.makePresentationCamera(
                    *device, angarSourceCamera,
                    static_cast<std::uint32_t>(pixelWidth),
                    static_cast<std::uint32_t>(pixelHeight));
            angarRenderer.renderFrame(
                *device, raceShader, angarCamera, 0x040818ffU,
                *originalAngarScene, angarVehicles, racePipeline,
                angarDecorationActive, garageDecorationFragments,
                garageVehicleDeathFragments, garageBonusActive,
                angarRacerRuntime, garageEffects, garageMines,
                garageProjectiles, angarSceneSeconds,
                profileState.config.quality);
            const auto& telemetry = device->renderTelemetry();
            const auto scenePass = static_cast<std::size_t>(
                r3d::renderer::RenderPass::Scene);
            raceAngar3DObserved =
                raceAngar3DObserved ||
                telemetry.drawCount[scenePass] > 0U;
            // AngarFrame contains nested ViewPort3d widgets. The source
            // GUI renders them against a fresh viewport depth buffer; keep
            // the completed HDR scene color but clear its world depth.
            device->beginPass(
                r3d::renderer::RenderPass::Overlay, {}, camera,
                0U, false, true);
        }
        else if (presentationCar != nullptr)
        {
            garageSceneSeconds += frameSeconds;
            const auto& selectedCar = *presentationCar;
            const auto selectedVehicle = std::find_if(
                originalGarageScene->vehicles.begin(),
                originalGarageScene->vehicles.end(),
                [&](const auto& vehicle) {
                    return vehicle.record == selectedCar.record ||
                           recordName(vehicle.record) ==
                               recordName(selectedCar.record);
                });
            const std::size_t selectedRacer =
                selectedVehicle ==
                        originalGarageScene->vehicles.end()
                    ? originalGarageScene->racers.size()
                    : static_cast<std::size_t>(
                          std::distance(
                              originalGarageScene->vehicles.begin(),
                              selectedVehicle));
            for (std::size_t racer = 0;
                 racer < garageRacerRuntime.size(); ++racer)
            {
                auto& runtime = garageRacerRuntime[racer];
                runtime.destroyed =
                    presentationCarLocked ||
                    racer != selectedRacer;
                runtime.weaponSlots.fill(
                    r3d::game::originalrace::RacerRuntime::
                        invalidWeapon);
                if (racer < originalGarageScene->racers.size())
                    originalGarageScene->racers[racer].color =
                        profileState.player.color;
            }
            if (!presentationCarLocked &&
                selectedRacer < garageRacerRuntime.size())
            {
                auto& runtime =
                    garageRacerRuntime[selectedRacer];
                constexpr std::size_t firstWeaponPlacement =
                    static_cast<std::size_t>(
                        r3d::game::originalrace::GarageSlotType::
                            Weapon1);
                for (std::size_t slot = 0;
                     slot < runtime.weaponSlots.size(); ++slot)
                {
                    const auto& weaponRecord =
                        drawingOriginalWorkshop
                            ? profileState.player
                                  .slots[firstWeaponPlacement + slot]
                                  .record
                            : selectedCar
                                  .placements[
                                      firstWeaponPlacement + slot]
                                  .defaultItem;
                    const auto weapon = std::find_if(
                        originalGarageScene->weapons.begin(),
                        originalGarageScene->weapons.end(),
                        [&](const auto& candidate) {
                            return candidate.record == weaponRecord ||
                                   recordName(candidate.record) ==
                                       recordName(weaponRecord);
                        });
                    if (weapon !=
                        originalGarageScene->weapons.end())
                    {
                        runtime.weaponSlots[slot] =
                            static_cast<std::size_t>(
                                std::distance(
                                    originalGarageScene->weapons.begin(),
                                    weapon));
                    }
                }
            }
            garageDecorationActive[1] = presentationCarLocked;
            const float halfQuestionAngle =
                bx::kPi * garageSceneSeconds * 0.1F;
            originalGarageScene->decorationInstances[1]
                .transform.rotation = {
                    0.0F, 0.0F,
                    std::sin(halfQuestionAngle),
                    std::cos(halfQuestionAngle)};
            const auto presentationSourceCamera =
                drawingOriginalWorkshop
                    ? makeWorkshopPresentationCamera(
                          originalGarageScene
                              ->presentationCamera)
                    : originalGarageScene
                          ->presentationCamera;
            const auto garageCamera =
                garageRenderer.makePresentationCamera(
                    *device, presentationSourceCamera,
                    static_cast<std::uint32_t>(pixelWidth),
                    static_cast<std::uint32_t>(pixelHeight));
            garageRenderer.renderFrame(
                *device, raceShader, garageCamera, 0x040818ffU,
                *originalGarageScene, garageVehicles, racePipeline,
                garageDecorationActive, garageDecorationFragments,
                garageVehicleDeathFragments, garageBonusActive,
                garageRacerRuntime, garageEffects, garageMines,
                garageProjectiles, garageSceneSeconds,
                profileState.config.quality);
            const auto& garageTelemetry =
                device->renderTelemetry();
            const auto scenePass = static_cast<std::size_t>(
                r3d::renderer::RenderPass::Scene);
            const bool observedPresentation3D =
                garageTelemetry.drawCount[scenePass] > 0U &&
                std::any_of(
                    garageTelemetry.lightingDrawCount.begin(),
                    garageTelemetry.lightingDrawCount.end(),
                    [](std::uint32_t draws) {
                        return draws > 0U;
                    });
            if (drawingOriginalWorkshop)
            {
                raceWorkshop3DObserved =
                    raceWorkshop3DObserved ||
                    observedPresentation3D;
                device->beginPass(
                    r3d::renderer::RenderPass::Overlay, {}, camera,
                    0U, false, true);
            }
            else
            {
                raceGarage3DObserved =
                    raceGarage3DObserved ||
                    observedPresentation3D;
                device->beginOverlay(camera);
            }
        }
        else
#endif
        {
            device->beginFrame(camera, 0x040818ffU);
            drawQuad(
                *device, quad, shader, background,
                menu::virtualWidth, menu::virtualHeight,
                menu::virtualWidth * 0.5F,
                menu::virtualHeight * 0.5F, 90.0F, opaque);
            drawQuad(
                *device, quad, shader, topPanel,
                static_cast<float>(model->topPanelImage.width),
                static_cast<float>(model->topPanelImage.height),
                menu::virtualWidth * 0.5F, 200.0F, 70.0F,
                transparent);
        }

        auto& activePage = activeMenuPage();
#ifdef RRR3D_PHYSICS
        if (drawingOriginalOptions)
        {
            const float optionsCenterX = menu::virtualWidth * 0.5F;
            const float optionsCenterY = menu::virtualHeight * 0.5F;
            drawQuad(
                *device, quad, shader, optionsMask,
                menu::virtualWidth, menu::virtualHeight,
                optionsCenterX, optionsCenterY, 65.0F, transparent);
            drawQuad(
                *device, quad, shader, optionsBackground,
                static_cast<float>(optionsBackgroundImage.width),
                static_cast<float>(optionsBackgroundImage.height),
                optionsCenterX, optionsCenterY, 60.0F, transparent);

            const auto& header = optionsHeaderPage.normal.front();
            drawQuad(
                *device, quad, shader, header.texture,
                header.width, header.height, optionsCenterX,
                optionsCenterY - 317.0F, 20.0F, transparent);

            const auto state = optionsStateIndex(menuStack.back());
            for (std::size_t index = 0;
                 index < optionsStatePage.labels.size(); ++index)
            {
                const bool selectedState = index == state;
                const float buttonX = optionsCenterX - 558.0F;
                const float buttonY =
                    optionsCenterY - 125.0F +
                    static_cast<float>(index) * 100.0F;
                const auto& stateText =
                    selectedState
                        ? optionsStatePage.selected[index]
                        : optionsStatePage.normal[index];
                drawQuad(
                    *device, quad, shader,
                    selectedState ? optionsButtonSelected
                                  : optionsButton,
                    static_cast<float>(
                        selectedState
                            ? optionsButtonSelectedImage.width
                            : optionsButtonImage.width),
                    static_cast<float>(
                        selectedState
                            ? optionsButtonSelectedImage.height
                            : optionsButtonImage.height),
                    buttonX, buttonY, 45.0F, transparent);
                drawQuad(
                    *device, quad, shader, stateText.texture,
                    stateText.width, stateText.height,
                    buttonX + 62.0F + stateText.width * 0.5F,
                    buttonY, 20.0F, transparent);
            }

            const MenuPageVisual* names = nullptr;
            std::size_t rowCount = 0U;
            std::size_t visibleRows = 0U;
            float firstRowY = optionsCenterY - 174.0F;
            if (menuStack.back() == MenuScreen::GameOptions)
            {
                names = &gameOptionNamesPage;
                rowCount = gameOptionNamesPage.labels.size();
                visibleRows = 7U;
                firstRowY = optionsCenterY - 178.0F;
            }
            else if (
                menuStack.back() == MenuScreen::GraphicsOptions)
            {
                names = &graphicsOptionNamesPage;
                rowCount = graphicsOptionNamesPage.labels.size();
                visibleRows = rowCount;
            }
            else if (
                menuStack.back() == MenuScreen::SoundOptions)
            {
                names = &soundOptionNamesPage;
                rowCount = soundOptionNamesPage.labels.size();
                visibleRows = rowCount;
            }
            else
            {
                names = &controlsOptionsPage;
                rowCount = originalControlActions.size();
                visibleRows = controlsVisibleRows;
                firstRowY = optionsCenterY - 130.0F;
                drawQuad(
                    *device, quad, shader, keyboardIcon,
                    static_cast<float>(keyboardIconImage.width),
                    static_cast<float>(keyboardIconImage.height),
                    optionsCenterX + 255.0F,
                    optionsCenterY - 190.0F, 25.0F, transparent);
                drawQuad(
                    *device, quad, shader, gamepadIcon,
                    static_cast<float>(gamepadIconImage.width),
                    static_cast<float>(gamepadIconImage.height),
                    optionsCenterX + 435.0F,
                    optionsCenterY - 190.0F, 25.0F, transparent);
            }
            visibleRows = std::min(visibleRows, rowCount);
            const std::size_t scrollAnchor =
                menuSelection < rowCount ? menuSelection
                                         : rowCount - 1U;
            const std::size_t firstVisible =
                scrollAnchor < visibleRows
                    ? 0U
                    : std::min(
                          scrollAnchor - visibleRows + 1U,
                          rowCount - visibleRows);

            auto drawTextAt = [&](const TextVisual& text, float x,
                                  float y, float maxWidth) {
                const float scale = std::min(
                    1.0F,
                    maxWidth / std::max(text.width, 1.0F));
                drawQuad(
                    *device, quad, shader, text.texture,
                    text.width * scale, text.height * scale,
                    x + text.width * scale * 0.5F, y, 15.0F,
                    transparent);
            };
            for (std::size_t slot = 0U; slot < visibleRows; ++slot)
            {
                const std::size_t index = firstVisible + slot;
                const bool selectedRow = index == menuSelection;
                const float rowY =
                    firstRowY + static_cast<float>(slot) * 50.0F;
                const bool controls =
                    menuStack.back() == MenuScreen::ControlsOptions;
                drawQuad(
                    *device, quad, shader,
                    controls ? controlsRow : optionsRow,
                    static_cast<float>(
                        controls ? controlsRowImage.width
                                 : optionsRowImage.width),
                    static_cast<float>(
                        controls ? controlsRowImage.height
                                 : optionsRowImage.height),
                    optionsCenterX - 235.0F, rowY, 45.0F,
                    transparent);
                const auto& name =
                    selectedRow ? names->selected[index]
                                : names->normal[index];
                drawTextAt(
                    name,
                    controls ? optionsCenterX - 374.0F
                             : optionsCenterX - 200.0F,
                    rowY, controls ? 300.0F : 420.0F);

                if (controls)
                {
                    const std::array<const MenuPageVisual*, 2>
                        valuePages{
                            &controlsKeyboardValuesPage,
                            &controlsGamepadValuesPage};
                    for (std::size_t controller = 0U;
                         controller < valuePages.size(); ++controller)
                    {
                        const bool selectedKey =
                            selectedRow &&
                            controlsUseGamepad ==
                                (controller == 1U);
                        const float keyX =
                            optionsCenterX + 255.0F +
                            static_cast<float>(controller) * 180.0F;
                        drawQuad(
                            *device, quad, shader,
                            selectedKey ? optionsKeySelected
                                        : optionsKey,
                            static_cast<float>(
                                selectedKey
                                    ? optionsKeySelectedImage.width
                                    : optionsKeyImage.width),
                            static_cast<float>(
                                selectedKey
                                    ? optionsKeySelectedImage.height
                                    : optionsKeyImage.height),
                            keyX, rowY, 35.0F, transparent);
                        const auto& value =
                            selectedKey
                                ? valuePages[controller]
                                      ->selected[index]
                                : valuePages[controller]
                                      ->normal[index];
                        const float scale = std::min(
                            1.0F,
                            135.0F /
                                std::max(value.width, 1.0F));
                        drawQuad(
                            *device, quad, shader, value.texture,
                            value.width * scale,
                            value.height * scale, keyX, rowY,
                            12.0F, transparent);
                    }
                    continue;
                }

                const bool volumeRow =
                    menuStack.back() == MenuScreen::SoundOptions &&
                    index >= 2U;
                if (volumeRow)
                {
                    const float volume =
                        std::array{
                            optionsDraftConfig.musicVolume,
                            optionsDraftConfig.effectsVolume,
                            optionsDraftConfig.voiceVolume}
                            [index - 2U];
                    const float normalized =
                        std::clamp(volume * 0.5F, 0.0F, 1.0F);
                    const float barX = optionsCenterX + 260.0F;
                    const float barWidth =
                        static_cast<float>(
                            optionsBarBackgroundImage.width);
                    drawQuad(
                        *device, quad, shader,
                        optionsBarBackground, barWidth,
                        static_cast<float>(
                            optionsBarBackgroundImage.height),
                        barX, rowY, 35.0F, transparent);
                    if (normalized > 0.0F)
                    {
                        drawQuad(
                            *device, quad, shader, optionsBar,
                            barWidth * normalized,
                            static_cast<float>(
                                optionsBarImage.height),
                            barX - barWidth * 0.5F +
                                barWidth * normalized * 0.5F,
                            rowY, 30.0F, transparent);
                    }
                }
                else
                {
                    const auto& value =
                        selectedRow
                            ? activePage.selected[index]
                            : activePage.normal[index];
                    drawQuad(
                        *device, quad, shader, value.texture,
                        value.width, value.height,
                        optionsCenterX + 260.0F, rowY, 12.0F,
                        transparent);
                }
                const Texture arrow =
                    selectedRow ? optionsArrowSelected
                                : optionsArrow;
                drawQuad(
                    *device, quad, shader, arrow,
                    static_cast<float>(
                        selectedRow
                            ? optionsArrowSelectedImage.width
                            : optionsArrowImage.width),
                    static_cast<float>(
                        selectedRow
                            ? optionsArrowSelectedImage.height
                            : optionsArrowImage.height),
                    optionsCenterX + 140.0F, rowY, 20.0F,
                    transparent);
                drawQuadRotated(
                    *device, quad, shader, arrow,
                    static_cast<float>(
                        selectedRow
                            ? optionsArrowSelectedImage.width
                            : optionsArrowImage.width),
                    static_cast<float>(
                        selectedRow
                            ? optionsArrowSelectedImage.height
                            : optionsArrowImage.height),
                    optionsCenterX + 380.0F, rowY, 20.0F,
                    bx::kPi, transparent);
            }

            if (firstVisible > 0U)
            {
                drawQuadRotated(
                    *device, quad, shader, optionsArrowSelected,
                    30.0F, 30.0F, optionsCenterX + 150.0F,
                    optionsCenterY -
                        (menuStack.back() ==
                                 MenuScreen::ControlsOptions
                             ? 150.0F
                             : 200.0F),
                    20.0F, bx::kPiHalf, transparent);
            }
            if (firstVisible + visibleRows < rowCount)
            {
                drawQuadRotated(
                    *device, quad, shader, optionsArrowSelected,
                    30.0F, 30.0F, optionsCenterX + 150.0F,
                    optionsCenterY + 195.0F, 20.0F,
                    -bx::kPiHalf, transparent);
            }

            for (std::size_t action = 0U; action < 2U; ++action)
            {
                const std::size_t selectionIndex = rowCount + action;
                const bool selectedAction =
                    menuSelection == selectionIndex;
                const float buttonX =
                    optionsCenterX - 30.0F +
                    static_cast<float>(action) * 360.0F;
                const float buttonY = optionsCenterY + 240.0F;
                drawQuad(
                    *device, quad, shader,
                    selectedAction ? optionsButtonSelected
                                   : optionsButton,
                    static_cast<float>(
                        selectedAction
                            ? optionsButtonSelectedImage.width
                            : optionsButtonImage.width),
                    static_cast<float>(
                        selectedAction
                            ? optionsButtonSelectedImage.height
                            : optionsButtonImage.height),
                    buttonX, buttonY, 40.0F, transparent);
                const auto& actionText =
                    selectedAction
                        ? optionsActionPage.selected[action]
                        : optionsActionPage.normal[action];
                drawQuad(
                    *device, quad, shader, actionText.texture,
                    actionText.width, actionText.height,
                    buttonX + 62.0F +
                        actionText.width * 0.5F,
                    buttonY, 15.0F, transparent);
            }
        }
        else if (drawingOriginalRaceMenu)
        {
            const float centerX = menu::virtualWidth * 0.5F;
            const float topCenterY =
                static_cast<float>(raceTopPanelImage.height) * 0.5F;
            const float bottomCenterY =
                menu::virtualHeight -
                static_cast<float>(raceBottomPanelImage.height) *
                    0.5F;
            drawQuad(
                *device, quad, shader, raceTopPanel,
                static_cast<float>(raceTopPanelImage.width),
                static_cast<float>(raceTopPanelImage.height),
                centerX, topCenterY, 60.0F, transparent);
            drawQuad(
                *device, quad, shader, raceBottomPanel,
                static_cast<float>(raceBottomPanelImage.width),
                static_cast<float>(raceBottomPanelImage.height),
                centerX, bottomCenterY, 60.0F, transparent);
            drawQuad(
                *device, quad, shader, raceStats,
                static_cast<float>(raceStatsImage.width),
                static_cast<float>(raceStatsImage.height),
                static_cast<float>(raceStatsImage.width) * 0.5F,
                menu::virtualHeight -
                    static_cast<float>(raceStatsImage.height) *
                        0.5F,
                45.0F, transparent);
            drawQuad(
                *device, quad, shader, raceMoney,
                static_cast<float>(raceMoneyImage.width),
                static_cast<float>(raceMoneyImage.height),
                menu::virtualWidth -
                    static_cast<float>(raceMoneyImage.width) *
                        0.5F,
                menu::virtualHeight -
                    static_cast<float>(raceMoneyImage.height) *
                        0.5F,
                45.0F, transparent);

            constexpr std::array<float, 5> headerOffsets{
                -548.0F, -273.0F, 2.0F, 277.0F, 552.0F};
            for (std::size_t index = 0U;
                 index < raceMainHeadersPage.normal.size(); ++index)
            {
                const auto& text =
                    raceMainHeadersPage.normal[index];
                drawQuad(
                    *device, quad, shader, text.texture,
                    text.width, text.height,
                    centerX + headerOffsets[index],
                    topCenterY + 18.0F, 20.0F, transparent);
            }

            constexpr std::array<float, 3> frameOffsets{
                -550.0F, 475.0F, 590.0F};
            for (const float offset : frameOffsets)
            {
                drawQuad(
                    *device, quad, shader, raceImageFrame,
                    static_cast<float>(raceImageFrameImage.width),
                    static_cast<float>(raceImageFrameImage.height),
                    centerX + offset, topCenterY + 87.0F,
                    35.0F, transparent);
            }
            const auto& playerName = raceMainInfoPage.normal[0];
            const auto& bossName = raceMainInfoPage.normal[1];
            const auto& passInfo = raceMainInfoPage.normal[2];
            const auto& tournamentInfo =
                raceMainInfoPage.normal[3];
            const auto& money = raceMainInfoPage.normal[4];
            drawQuad(
                *device, quad, shader, playerName.texture,
                playerName.width, playerName.height,
                centerX - 550.0F, topCenterY + 87.0F,
                15.0F, transparent);
            drawQuad(
                *device, quad, shader, bossName.texture,
                bossName.width, bossName.height,
                centerX + 475.0F, topCenterY + 87.0F,
                15.0F, transparent);
            auto drawInfoLeft =
                [&](const TextVisual& text, float x, float y,
                    float maximumWidth) {
                    const float scale = std::min(
                        1.0F,
                        maximumWidth /
                            std::max(text.width, 1.0F));
                    drawQuad(
                        *device, quad, shader, text.texture,
                        text.width * scale,
                        text.height * scale,
                        x + text.width * scale * 0.5F, y,
                        15.0F, transparent);
                };
            drawInfoLeft(
                passInfo, centerX - 377.0F,
                topCenterY + 86.0F, 260.0F);
            drawInfoLeft(
                tournamentInfo, centerX - 102.0F,
                topCenterY + 86.0F, 260.0F);
            drawQuad(
                *device, quad, shader, money.texture,
                money.width, money.height,
                menu::virtualWidth - 53.0F -
                    money.width * 0.5F,
                menu::virtualHeight - 29.0F, 15.0F,
                transparent);

            std::size_t weatherIndex = 0U;
            using Weather =
                r3d::game::originalrace::Weather;
            if (originalRace->environment.weather == Weather::Night)
                weatherIndex = 1U;
            else if (
                originalRace->environment.weather ==
                    Weather::Cloudy ||
                originalRace->environment.weather == Weather::Hell)
                weatherIndex = 2U;
            else if (
                originalRace->environment.weather ==
                Weather::Rainy)
                weatherIndex = 3U;
            drawQuad(
                *device, quad, shader,
                raceWeatherIcons[weatherIndex],
                static_cast<float>(
                    raceWeatherImages[weatherIndex].width),
                static_cast<float>(
                    raceWeatherImages[weatherIndex].height),
                centerX - 282.0F, topCenterY + 112.0F,
                15.0F, transparent);

            constexpr std::array<std::size_t, 6> sourceChargeSlots{
                6U, 7U, 8U, 9U, 4U, 5U};
            std::size_t visibleCharge = 0U;
            for (const auto slotIndex : sourceChargeSlots)
            {
                if (slotIndex >= profileState.player.slots.size())
                    continue;
                const auto& slot =
                    profileState.player.slots[slotIndex];
                if (slot.record.empty())
                    continue;
                const float progress = std::clamp(
                    static_cast<float>(
                        slot.hasCharge ? slot.charge : 0U) /
                        7.0F,
                    0.0F, 1.0F);
                const float chargeX =
                    centerX + 129.0F +
                    static_cast<float>(visibleCharge) * 54.0F;
                const float fullHeight =
                    static_cast<float>(raceChargeBarImage.height);
                if (progress > 0.0F)
                {
                    drawQuad(
                        *device, quad, shader, raceChargeBar,
                        static_cast<float>(
                            raceChargeBarImage.width),
                        fullHeight * progress, chargeX,
                        topCenterY + 120.0F +
                            fullHeight * (1.0F - progress) *
                                0.5F,
                        18.0F, transparent);
                }
                ++visibleCharge;
            }

            constexpr float itemWidth = 110.0F;
            constexpr float itemSpacing = 50.0F;
            const float firstX =
                centerX -
                (7.0F * itemWidth + 6.0F * itemSpacing) *
                    0.5F +
                itemWidth * 0.5F;
            const float itemY = bottomCenterY - 72.0F;
            for (std::size_t index = 0U;
                 index < raceMenuIcons.size(); ++index)
            {
                const bool selectedItem = index == menuSelection;
                const float itemX =
                    firstX +
                    static_cast<float>(index) *
                        (itemWidth + itemSpacing);
                drawQuad(
                    *device, quad, shader,
                    selectedItem ? raceMenuButtonSelected
                                 : raceMenuButton,
                    static_cast<float>(
                        selectedItem
                            ? raceMenuButtonSelectedImage.width
                            : raceMenuButtonImage.width),
                    static_cast<float>(
                        selectedItem
                            ? raceMenuButtonSelectedImage.height
                            : raceMenuButtonImage.height),
                    itemX, itemY, 35.0F, transparent);
                drawQuad(
                    *device, quad, shader, raceMenuIcons[index],
                    static_cast<float>(
                        raceMenuIconImages[index].width),
                    static_cast<float>(
                        raceMenuIconImages[index].height),
                    itemX, itemY, 20.0F, transparent);
            }
        }
        else if (drawingOriginalGarage)
        {
            raceGarageFrameObserved = true;
            const float centerX = menu::virtualWidth * 0.5F;
            const float centerY = menu::virtualHeight * 0.5F;
            const float topCenterY =
                static_cast<float>(garageTopPanelImage.height) *
                0.5F;
            const float bottomCenterY =
                menu::virtualHeight -
                static_cast<float>(
                    garageBottomPanelImage.height) *
                    0.5F;
            const float sideCenterY =
                (static_cast<float>(garageTopPanelImage.height) +
                 menu::virtualHeight -
                 static_cast<float>(
                     garageBottomPanelImage.height)) *
                0.5F;
            drawQuad(
                *device, quad, shader, garageTopPanel,
                static_cast<float>(garageTopPanelImage.width),
                static_cast<float>(garageTopPanelImage.height),
                centerX, topCenterY, 60.0F, transparent);
            drawQuad(
                *device, quad, shader, garageBottomPanel,
                static_cast<float>(garageBottomPanelImage.width),
                static_cast<float>(
                    garageBottomPanelImage.height),
                centerX, bottomCenterY, 60.0F, transparent);
            drawQuadRotated(
                *device, quad, shader, garageSidePanel,
                static_cast<float>(garageSidePanelImage.width),
                static_cast<float>(garageSidePanelImage.height),
                static_cast<float>(garageSidePanelImage.width) *
                    0.5F,
                sideCenterY, 55.0F, bx::kPi, transparent);
            drawQuad(
                *device, quad, shader, garageSidePanel,
                static_cast<float>(garageSidePanelImage.width),
                static_cast<float>(garageSidePanelImage.height),
                menu::virtualWidth -
                    static_cast<float>(
                        garageSidePanelImage.width) *
                        0.5F,
                sideCenterY, 55.0F, transparent);

            drawQuad(
                *device, quad, shader, garageMoney,
                static_cast<float>(garageMoneyImage.width),
                static_cast<float>(garageMoneyImage.height),
                menu::virtualWidth -
                    static_cast<float>(garageMoneyImage.width) *
                        0.5F,
                menu::virtualHeight -
                    static_cast<float>(
                        garageMoneyImage.height) *
                        0.5F,
                42.0F, transparent);
            constexpr float statsLeft = 418.0F;
            constexpr float statsTop = 889.0F + 60.0F;
            drawQuad(
                *device, quad, shader, garageStats,
                static_cast<float>(garageStatsImage.width),
                static_cast<float>(garageStatsImage.height),
                statsLeft +
                    static_cast<float>(garageStatsImage.width) *
                        0.5F,
                statsTop +
                    static_cast<float>(garageStatsImage.height) *
                        0.5F,
                42.0F, transparent);

            const auto& selectedCar =
                originalGarage->cars[garageCarIndex];
            const bool selectedLocked =
                garageCarOrder[garageViewIndex].locked;
            const auto selectedStats =
                r3d::game::originalrace::originalGarageStats(
                    *originalGarage, selectedCar);
            const std::array<float, 3> statProgress{
                selectedLocked ? 0.0F
                               : selectedStats.damageProgress,
                selectedLocked ? 0.0F
                               : selectedStats.armorProgress,
                selectedLocked ? 0.0F
                               : selectedStats.speedProgress};
            constexpr std::array<float, 3> statOffsetY{
                3.0F, 41.0F, 78.0F};
            for (std::size_t index = 0U;
                 index < statProgress.size(); ++index)
            {
                const float progress = std::clamp(
                    statProgress[index], 0.0F, 1.0F);
                if (progress > 0.0F)
                {
                    const float fullWidth = static_cast<float>(
                        garageStatBarImage.width);
                    drawQuad(
                        *device, quad, shader, garageStatBar,
                        fullWidth * progress,
                        static_cast<float>(
                            garageStatBarImage.height),
                        statsLeft + 48.0F +
                            fullWidth * progress * 0.5F,
                        statsTop + statOffsetY[index] +
                            static_cast<float>(
                                garageStatBarImage.height) *
                                0.5F,
                        30.0F, transparent);
                }
                const auto& value =
                    garageStatsPage.normal[index];
                drawQuad(
                    *device, quad, shader, value.texture,
                    value.width, value.height,
                    statsLeft + 48.0F +
                        static_cast<float>(
                            garageStatBarImage.width) -
                        value.width * 0.5F - 4.0F,
                    statsTop + statOffsetY[index] + 13.0F,
                    15.0F, transparent);
            }

            const std::size_t visibleCars = std::max<std::size_t>(
                1U,
                static_cast<std::size_t>(
                    (menu::virtualWidth - 10.0F) /
                    static_cast<float>(
                        garageCarBoxImage.width)));
            std::size_t firstVisible = 0U;
            if (garageCarOrder.size() > visibleCars)
            {
                const std::size_t right = std::min(
                    garageViewIndex + visibleCars / 2U,
                    garageCarOrder.size() - 1U);
                firstVisible =
                    right + 1U > visibleCars
                        ? right + 1U - visibleCars
                        : 0U;
                firstVisible = std::min(
                    firstVisible,
                    garageCarOrder.size() - visibleCars);
            }
            const std::size_t visibleEnd = std::min(
                firstVisible + visibleCars,
                garageCarOrder.size());
            const float visibleWidth =
                static_cast<float>(
                    visibleEnd - firstVisible) *
                static_cast<float>(garageCarBoxImage.width);
            const float firstCarX =
                (menu::virtualWidth - visibleWidth) * 0.5F +
                static_cast<float>(garageCarBoxImage.width) *
                    0.5F;
            for (std::size_t view = firstVisible;
                 view < visibleEnd; ++view)
            {
                const auto& carView = garageCarOrder[view];
                const bool selected = view == garageViewIndex;
                const float x =
                    firstCarX +
                    static_cast<float>(view - firstVisible) *
                        static_cast<float>(
                            garageCarBoxImage.width);
                drawQuad(
                    *device, quad, shader,
                    selected ? garageCarBoxSelected
                             : garageCarBox,
                    static_cast<float>(
                        selected
                            ? garageCarBoxSelectedImage.width
                            : garageCarBoxImage.width),
                    static_cast<float>(
                        selected
                            ? garageCarBoxSelectedImage.height
                            : garageCarBoxImage.height),
                    x, 71.5F, 35.0F, transparent);
                if (carView.locked)
                {
                    drawQuad(
                        *device, quad, shader, garageLock,
                        static_cast<float>(garageLockImage.width),
                        static_cast<float>(
                            garageLockImage.height),
                        x, 71.5F, 20.0F, transparent);
                }
                else
                {
                    const auto& image =
                        garageCarImages[carView.catalogIndex];
                    drawQuad(
                        *device, quad, shader,
                        garageCarTextures[
                            carView.catalogIndex],
                        static_cast<float>(image.width),
                        static_cast<float>(image.height),
                        x, 71.5F, 20.0F, transparent);
                }
            }

            const bool leftArrowVisible = garageViewIndex > 0U;
            const bool rightArrowVisible =
                garageViewIndex + 1U < garageCarOrder.size();
            if (leftArrowVisible)
            {
                const bool selected = menuSelection == 2U;
                drawQuad(
                    *device, quad, shader,
                    selected ? garageArrowSelected
                             : garageArrow,
                    static_cast<float>(
                        selected
                            ? garageArrowSelectedImage.width
                            : garageArrowImage.width),
                    static_cast<float>(
                        selected
                            ? garageArrowSelectedImage.height
                            : garageArrowImage.height),
                    181.0F, centerY, 35.0F, transparent);
            }
            if (rightArrowVisible)
            {
                const bool selected = menuSelection == 3U;
                drawQuadRotated(
                    *device, quad, shader,
                    selected ? garageArrowSelected
                             : garageArrow,
                    static_cast<float>(
                        selected
                            ? garageArrowSelectedImage.width
                            : garageArrowImage.width),
                    static_cast<float>(
                        selected
                            ? garageArrowSelectedImage.height
                            : garageArrowImage.height),
                    menu::virtualWidth - 181.0F, centerY,
                    35.0F, bx::kPi, transparent);
            }

            const float firstColorY = centerY - 144.0F;
            for (std::size_t side = 0U; side < 2U; ++side)
            {
                const float colorX =
                    side == 0U
                        ? static_cast<float>(
                              garageSidePanelImage.width) *
                              0.5F
                        : menu::virtualWidth -
                              static_cast<float>(
                                  garageSidePanelImage.width) *
                                  0.5F;
                for (std::size_t index = 0U; index < 7U;
                     ++index)
                {
                    const std::size_t colorIndex =
                        side * 7U + index;
                    const std::size_t focusIndex =
                        4U + colorIndex;
                    const bool focused =
                        menuSelection == focusIndex;
                    bool activeColor = true;
                    for (std::size_t component = 0U;
                         component < 4U; ++component)
                    {
                        const int profileComponent =
                            static_cast<int>(std::lround(
                                std::clamp(
                                    profileState.player
                                        .color[component],
                                    0.0F, 1.0F) *
                                255.0F));
                        activeColor =
                            activeColor &&
                            profileComponent ==
                                garageColorPixels[colorIndex]
                                                  [component];
                    }
                    const float colorY =
                        firstColorY +
                        static_cast<float>(index) * 48.0F;
                    drawQuad(
                        *device, quad, shader,
                        focused || activeColor
                            ? garageColorBoxSelected
                            : garageColorBoxBackground,
                        static_cast<float>(
                            focused || activeColor
                                ? garageColorBoxSelectedImage.width
                                : garageColorBoxBackgroundImage.width),
                        static_cast<float>(
                            focused || activeColor
                                ? garageColorBoxSelectedImage.height
                                : garageColorBoxBackgroundImage.height),
                        colorX, colorY, 34.0F, transparent);
                    drawQuad(
                        *device, quad, shader,
                        garageColorTextures[colorIndex],
                        static_cast<float>(
                            garageColorBoxImage.width),
                        static_cast<float>(
                            garageColorBoxImage.height),
                        colorX, colorY, 20.0F, transparent);
                    drawQuad(
                        *device, quad, shader, garageColorBox,
                        static_cast<float>(
                            garageColorBoxImage.width),
                        static_cast<float>(
                            garageColorBoxImage.height),
                        colorX, colorY, 18.0F, transparent);
                }
            }

            const float carNameY = bottomCenterY - 155.0F;
            const auto& carName = garagePage.normal[0];
            drawQuad(
                *device, quad, shader, carName.texture,
                carName.width, carName.height, centerX + 10.0F,
                carNameY, 18.0F, transparent);
            for (std::size_t line = 0U;
                 line < garageInfoPage.normal.size(); ++line)
            {
                const auto& text = garageInfoPage.normal[line];
                drawQuad(
                    *device, quad, shader, text.texture,
                    text.width, text.height, centerX,
                    bottomCenterY - 76.0F +
                        static_cast<float>(line) * 20.0F,
                    18.0F, transparent);
            }
            const auto& money = garagePage.normal[1];
            drawQuad(
                *device, quad, shader, money.texture,
                money.width, money.height,
                menu::virtualWidth - 53.0F -
                    money.width * 0.5F,
                menu::virtualHeight - 29.0F, 15.0F,
                transparent);
            const auto& price = garagePage.normal[2];
            drawQuad(
                *device, quad, shader, price.texture,
                price.width, price.height, centerX - 523.0F,
                bottomCenterY - 80.0F, 15.0F, transparent);

            const bool backFocused = menuSelection == 0U;
            const float backX =
                static_cast<float>(garageBackImage.width) * 0.5F;
            const float backY =
                menu::virtualHeight -
                static_cast<float>(
                    garageBottomPanelImage.height) +
                14.0F;
            drawQuad(
                *device, quad, shader,
                backFocused ? garageBackSelected : garageBack,
                static_cast<float>(
                    backFocused
                        ? garageBackSelectedImage.width
                        : garageBackImage.width),
                static_cast<float>(
                    backFocused
                        ? garageBackSelectedImage.height
                        : garageBackImage.height),
                backX, backY, 35.0F, transparent);
            const auto& backText =
                backFocused ? garagePage.selected[3]
                            : garagePage.normal[3];
            drawQuad(
                *device, quad, shader, backText.texture,
                backText.width, backText.height, backX, backY,
                15.0F, transparent);

            const bool buyFocused = menuSelection == 1U;
            const float buyX = centerX + 15.0F;
            const float buyY =
                menu::virtualHeight -
                static_cast<float>(
                    garageBottomPanelImage.height) +
                static_cast<float>(garageBuyImage.height) *
                    0.5F;
            drawQuad(
                *device, quad, shader,
                buyFocused && !selectedLocked
                    ? garageBuySelected
                    : garageBuy,
                static_cast<float>(
                    buyFocused && !selectedLocked
                        ? garageBuySelectedImage.width
                        : garageBuyImage.width),
                static_cast<float>(
                    buyFocused && !selectedLocked
                        ? garageBuySelectedImage.height
                        : garageBuyImage.height),
                buyX, buyY, 35.0F, transparent);

            if (garagePurchaseDialogVisible)
            {
                drawQuad(
                    *device, quad, shader, acceptFrame,
                    static_cast<float>(acceptFrameImage.width),
                    static_cast<float>(acceptFrameImage.height),
                    centerX, centerY, 10.0F, transparent);
                const auto& message =
                    garagePurchasePage.normal.front();
                const float scale = std::min(
                    1.0F,
                    300.0F / std::max(message.width, 1.0F));
                drawQuad(
                    *device, quad, shader, message.texture,
                    message.width * scale,
                    message.height * scale, centerX,
                    centerY - 35.0F, 5.0F, transparent);
                auto drawChoice =
                    [&](bool yes, float x) {
                        const bool selected =
                            garagePurchaseYesFocused == yes;
                        drawQuad(
                            *device, quad, shader,
                            selected ? acceptButtonSelected
                                     : acceptButton,
                            static_cast<float>(
                                selected
                                    ? acceptButtonSelectedImage.width
                                    : acceptButtonImage.width),
                            static_cast<float>(
                                selected
                                    ? acceptButtonSelectedImage.height
                                    : acceptButtonImage.height),
                            x, centerY + 32.0F, 4.0F,
                            transparent);
                        const auto& label =
                            yes
                                ? (selected
                                       ? exitRaceYesSelected
                                       : exitRaceYes)
                                : (selected
                                       ? exitRaceNoSelected
                                       : exitRaceNo);
                        drawQuad(
                            *device, quad, shader, label.texture,
                            label.width, label.height, x,
                            centerY + 32.0F, 3.0F,
                            transparent);
                    };
                drawChoice(true, centerX - 70.0F);
                drawChoice(false, centerX + 70.0F);
            }
        }
        else if (drawingOriginalWorkshop)
        {
            raceWorkshopFrameObserved = true;
            const float centerX = menu::virtualWidth * 0.5F;
            const float topCenterY =
                static_cast<float>(
                    workshopTopPanelImage.height) *
                0.5F;
            const float bottomCenterY =
                menu::virtualHeight -
                static_cast<float>(
                    workshopBottomPanelImage.height) *
                    0.5F;
            const float leftPanelCenterY =
                (static_cast<float>(
                     workshopTopPanelImage.height) -
                     30.0F +
                 menu::virtualHeight -
                 static_cast<float>(
                     workshopBottomPanelImage.height)) *
                0.5F;
            const float leftPanelCenterX =
                30.0F +
                static_cast<float>(
                    workshopLeftPanelImage.width) *
                    0.5F;
            drawQuad(
                *device, quad, shader, workshopTopPanel,
                static_cast<float>(
                    workshopTopPanelImage.width),
                static_cast<float>(
                    workshopTopPanelImage.height),
                centerX, topCenterY, 60.0F, transparent);
            drawQuad(
                *device, quad, shader, workshopBottomPanel,
                static_cast<float>(
                    workshopBottomPanelImage.width),
                static_cast<float>(
                    workshopBottomPanelImage.height),
                centerX, bottomCenterY, 60.0F, transparent);
            drawQuad(
                *device, quad, shader, workshopLeftPanel,
                static_cast<float>(
                    workshopLeftPanelImage.width),
                static_cast<float>(
                    workshopLeftPanelImage.height),
                leftPanelCenterX, leftPanelCenterY, 55.0F,
                transparent);

            drawQuad(
                *device, quad, shader, garageMoney,
                static_cast<float>(garageMoneyImage.width),
                static_cast<float>(garageMoneyImage.height),
                menu::virtualWidth -
                    static_cast<float>(
                        garageMoneyImage.width) *
                        0.5F,
                menu::virtualHeight -
                    static_cast<float>(
                        garageMoneyImage.height) *
                        0.5F,
                42.0F, transparent);
            const float statsLeft =
                menu::virtualWidth * 0.5F -
                static_cast<float>(
                    garageStatsImage.width) -
                15.0F;
            const float statsTop =
                menu::virtualHeight -
                static_cast<float>(
                    workshopBottomPanelImage.height) +
                25.0F;
            drawQuad(
                *device, quad, shader, garageStats,
                static_cast<float>(garageStatsImage.width),
                static_cast<float>(garageStatsImage.height),
                statsLeft +
                    static_cast<float>(
                        garageStatsImage.width) *
                        0.5F,
                statsTop +
                    static_cast<float>(
                        garageStatsImage.height) *
                        0.5F,
                42.0F, transparent);

            const auto* currentCar =
                originalGarage->findCar(
                    profileState.player.currentCar);
            const auto baseStats =
                currentCar == nullptr
                    ? r3d::game::originalrace::
                          OriginalGarageStats{}
                    : r3d::game::originalrace::
                          originalGarageStats(
                              *originalGarage, *currentCar,
                              profileState.player);
            const r3d::game::originalrace::
                OriginalWorkshopItem* previewItem = nullptr;
            r3d::game::originalrace::ProfileSlot previewProfileSlot;
            constexpr std::size_t firstGoodFocus = 1U;
            constexpr std::size_t firstSlotFocus = 13U;
            if (workshopDrag.active())
            {
                previewItem = originalGarage->findItem(
                    workshopDrag.item.record);
                previewProfileSlot = workshopDrag.item;
            }
            else if (
                menuSelection >= firstGoodFocus &&
                menuSelection < firstSlotFocus)
            {
                const std::size_t itemIndex =
                    workshopGoodScroll * 3U +
                    menuSelection - firstGoodFocus;
                if (itemIndex < workshopGoods.size())
                {
                    previewItem = workshopGoods[itemIndex];
                    previewProfileSlot = {
                        previewItem->record,
                        previewItem->defaultCharge,
                        previewItem->maximumCharge > 0U};
                }
            }
            else if (
                currentCar != nullptr &&
                menuSelection >= firstSlotFocus &&
                menuSelection < firstSlotFocus + 4U)
            {
                const std::size_t slotIndex =
                    menuSelection - firstSlotFocus;
                const auto slotType =
                    static_cast<r3d::game::originalrace::
                                    GarageSlotType>(slotIndex);
                const int level =
                    r3d::game::originalrace::
                        originalWorkshopUpgradeLevel(
                            profileState.player
                                .slots[slotIndex]
                                .record,
                            slotType);
                previewItem =
                    r3d::game::originalrace::
                        originalWorkshopUpgradeItem(
                            *originalGarage, *currentCar,
                            slotType, level + 1);
                if (previewItem != nullptr)
                {
                    previewProfileSlot = {
                        previewItem->record,
                        previewItem->defaultCharge,
                        previewItem->maximumCharge > 0U};
                }
            }

            std::optional<std::size_t> previewSlot;
            float leastInstalledDamage =
                std::numeric_limits<float>::max();
            if (previewItem != nullptr &&
                !previewProfileSlot.record.empty())
            {
                for (std::size_t slot = 0U;
                     slot <
                     profileState.player.slots.size();
                     ++slot)
                {
                    if (!workshopSlotAccepts(
                            slot, previewProfileSlot))
                        continue;
                    const auto* installed =
                        originalGarage->findItem(
                            profileState.player
                                .slots[slot]
                                .record);
                    const float installedDamage =
                        installed == nullptr
                            ? 0.0F
                            : installed->projectileDamage;
                    if (!previewSlot ||
                        installedDamage <
                            leastInstalledDamage)
                    {
                        previewSlot = slot;
                        leastInstalledDamage =
                            installedDamage;
                    }
                    if (installed == nullptr)
                        break;
                }
            }

            auto bonusStats = baseStats;
            if (currentCar != nullptr && previewSlot)
            {
                auto previewPlayer = profileState.player;
                previewPlayer.slots[*previewSlot] =
                    previewProfileSlot;
                bonusStats =
                    r3d::game::originalrace::
                        originalGarageStats(
                            *originalGarage, *currentCar,
                            previewPlayer);
            }
            const std::array<float, 3> baseProgress{
                baseStats.damageProgress,
                baseStats.armorProgress,
                baseStats.speedProgress};
            const std::array<float, 3> bonusProgress{
                bonusStats.damageProgress,
                bonusStats.armorProgress,
                bonusStats.speedProgress};
            constexpr std::array<float, 3> statOffsetY{
                3.0F, 41.0F, 78.0F};
            const float fullStatWidth =
                static_cast<float>(
                    garageStatBarImage.width);
            for (std::size_t index = 0U;
                 index < baseProgress.size(); ++index)
            {
                const float preview = std::clamp(
                    bonusProgress[index], 0.0F, 1.0F);
                if (preview > 0.0F)
                {
                    drawQuad(
                        *device, quad, shader,
                        workshopStatBarPlus,
                        fullStatWidth * preview,
                        static_cast<float>(
                            garageStatBarImage.height),
                        statsLeft + 48.0F +
                            fullStatWidth * preview * 0.5F,
                        statsTop + statOffsetY[index] +
                            static_cast<float>(
                                garageStatBarImage.height) *
                                0.5F,
                        32.0F, transparent);
                }
                const float installed = std::clamp(
                    baseProgress[index], 0.0F, 1.0F);
                if (installed > 0.0F)
                {
                    drawQuad(
                        *device, quad, shader,
                        garageStatBar,
                        fullStatWidth * installed,
                        static_cast<float>(
                            garageStatBarImage.height),
                        statsLeft + 48.0F +
                            fullStatWidth * installed *
                                0.5F,
                        statsTop + statOffsetY[index] +
                            static_cast<float>(
                                garageStatBarImage.height) *
                                0.5F,
                        30.0F, transparent);
                }
                if (index <
                    workshopStatsPage.normal.size())
                {
                    const auto& value =
                        workshopStatsPage.normal[index];
                    drawQuad(
                        *device, quad, shader,
                        value.texture, value.width,
                        value.height,
                        statsLeft + 48.0F +
                            fullStatWidth -
                            value.width * 0.5F - 4.0F,
                        statsTop + statOffsetY[index] +
                            13.0F,
                        15.0F, transparent);
                }
            }

            const auto goodCenters =
                workshopGoodCenters();
            for (std::size_t visible = 0U;
                 visible < goodCenters.size(); ++visible)
            {
                const std::size_t itemIndex =
                    workshopGoodScroll * 3U + visible;
                drawQuad(
                    *device, quad, shader,
                    workshopSlotFrame,
                    static_cast<float>(
                        workshopSlotFrameImage.width),
                    static_cast<float>(
                        workshopSlotFrameImage.height),
                    goodCenters[visible][0],
                    goodCenters[visible][1], 35.0F,
                    transparent);
                if (itemIndex >= workshopGoods.size())
                    continue;
                float itemRotation =
                    2.0F * bx::kPi *
                    static_cast<float>(itemIndex) / 12.0F;
                if (menuSelection == 1U + visible)
                {
                    itemRotation +=
                        garageSceneSeconds * bx::kPi * 0.5F;
                }
                workshopRenderer.drawItem(
                    *device, raceShader,
                    *workshopGoods[itemIndex],
                    goodCenters[visible][0],
                    goodCenters[visible][1],
                    static_cast<float>(
                        workshopSlotFrameImage.width),
                    static_cast<float>(
                        workshopSlotFrameImage.height),
                    itemRotation, racePipeline);
            }

            const auto slotCenters =
                workshopSlotCenters();
            if (currentCar != nullptr)
            {
                for (std::size_t slot = 0U;
                     slot < slotCenters.size(); ++slot)
                {
                    const auto& placement =
                        currentCar->placements[slot];
                    const bool enabled =
                        placement.active &&
                        (championshipMode ||
                         slot <
                             r3d::game::originalrace::
                                 PlayerProfile::
                                     firstWeaponSlot ||
                         slot -
                                 r3d::game::originalrace::
                                     PlayerProfile::
                                         firstWeaponSlot <
                             profileState.config
                                 .weaponMaxLevel);
                    if (!enabled)
                        continue;
                    const float x = slotCenters[slot][0];
                    const float y = slotCenters[slot][1];
                    drawQuad(
                        *device, quad, shader, workshopSlot,
                        static_cast<float>(
                            workshopSlotImage.width),
                        static_cast<float>(
                            workshopSlotImage.height),
                        x, y, 48.0F, transparent);

                    const auto& installed =
                        profileState.player.slots[slot];
                    const auto* installedItem =
                        originalGarage->findItem(
                            installed.record);
                    const bool compatible =
                        previewItem != nullptr &&
                        workshopSlotAccepts(
                            slot, previewProfileSlot);
                    if (installedItem != nullptr)
                    {
                        const float rotation =
                            menuSelection ==
                                    firstSlotFocus + slot
                                ? garageSceneSeconds *
                                      bx::kPi * 0.5F
                                : 0.0F;
                        const float viewportSize = std::min(
                            static_cast<float>(
                                workshopSlotImage.width),
                            static_cast<float>(
                                workshopSlotImage.height));
                        workshopRenderer.drawItem(
                            *device, raceShader,
                            *installedItem, x, y,
                            viewportSize, viewportSize,
                            rotation, racePipeline);
                    }

                    if (slot >= 4U &&
                        (installedItem == nullptr ||
                         compatible))
                    {
                        std::size_t iconIndex =
                            slot == 4U
                                ? 0U
                                : slot == 5U ? 1U : 2U;
                        if (slot >= 6U && compatible)
                            iconIndex = 3U;
                        drawQuad(
                            *device, quad, shader,
                            workshopSlotIconTextures[
                                iconIndex],
                            static_cast<float>(
                                workshopSlotIconImages[
                                    iconIndex]
                                    .width),
                            static_cast<float>(
                                workshopSlotIconImages[
                                    iconIndex]
                                    .height),
                            x, y, 20.0F, transparent);
                    }

                    if (installedItem == nullptr)
                        continue;
                    if (installedItem->maximumCharge == 0U)
                    {
                        const auto slotType =
                            static_cast<
                                r3d::game::originalrace::
                                    GarageSlotType>(slot);
                        const int level =
                            std::clamp(
                                r3d::game::originalrace::
                                    originalWorkshopUpgradeLevel(
                                        installed.record,
                                        slotType),
                                0, 2);
                        const bool focused =
                            menuSelection ==
                            firstSlotFocus + slot;
                        const std::size_t imageIndex =
                            static_cast<std::size_t>(
                                std::min(
                                    level +
                                        (focused ? 1 : 0),
                                    2));
                        drawQuad(
                            *device, quad, shader,
                            workshopUpgradeTextures[
                                imageIndex],
                            static_cast<float>(
                                workshopUpgradeImages[
                                    imageIndex]
                                    .width),
                            static_cast<float>(
                                workshopUpgradeImages[
                                    imageIndex]
                                    .height),
                            x + 51.0F, y + 38.0F,
                            18.0F, transparent);
                    }
                    else
                    {
                        const float chargeX = x + 68.0F;
                        const float chargeY = y - 12.0F;
                        drawQuad(
                            *device, quad, shader,
                            workshopChargeBox,
                            static_cast<float>(
                                workshopChargeBoxImage.width),
                            static_cast<float>(
                                workshopChargeBoxImage.height),
                            chargeX, chargeY, 22.0F,
                            transparent);
                        const float progress =
                            std::clamp(
                                static_cast<float>(
                                    installed.hasCharge
                                        ? installed.charge
                                        : installedItem
                                              ->defaultCharge) /
                                    static_cast<float>(
                                        std::max(
                                            installedItem
                                                ->maximumCharge,
                                            1U)),
                                0.0F, 1.0F);
                        const float barHeight =
                            static_cast<float>(
                                workshopChargeBarImage.height) *
                            progress;
                        if (barHeight > 0.0F)
                        {
                            const float barBottom =
                                chargeY + 24.0F;
                            drawQuad(
                                *device, quad, shader,
                                workshopChargeBar,
                                static_cast<float>(
                                    workshopChargeBarImage.width),
                                barHeight, chargeX,
                                barBottom -
                                    barHeight * 0.5F,
                                18.0F, transparent);
                        }
                        const bool focused =
                            menuSelection ==
                            firstSlotFocus + slot;
                        drawQuad(
                            *device, quad, shader,
                            focused
                                ? workshopChargeButtonSelected
                                : workshopChargeButton,
                            static_cast<float>(
                                focused
                                    ? workshopChargeButtonSelectedImage
                                          .width
                                    : workshopChargeButtonImage
                                          .width),
                            static_cast<float>(
                                focused
                                    ? workshopChargeButtonSelectedImage
                                          .height
                                    : workshopChargeButtonImage
                                          .height),
                            chargeX, chargeY + 39.0F,
                            16.0F, transparent);
                    }
                }
            }

            const std::size_t rowCount =
                (workshopGoods.size() + 2U) / 3U;
            const std::size_t maximumScroll =
                rowCount > 4U ? rowCount - 4U : 0U;
            const float upY =
                leftPanelCenterY -
                static_cast<float>(
                    workshopLeftPanelImage.height) *
                    0.5F +
                65.0F;
            const float downY =
                leftPanelCenterY +
                static_cast<float>(
                    workshopLeftPanelImage.height) *
                    0.5F -
                42.0F;
            if (workshopGoodScroll > 0U)
            {
                drawQuadRotated(
                    *device, quad, shader, garageArrow,
                    30.0F, 30.0F, leftPanelCenterX, upY,
                    18.0F, bx::kPi * 0.5F, transparent);
            }
            if (workshopGoodScroll < maximumScroll)
            {
                drawQuadRotated(
                    *device, quad, shader, garageArrow,
                    30.0F, 30.0F, leftPanelCenterX, downY,
                    18.0F, -bx::kPi * 0.5F, transparent);
            }

            const auto& header = workshopPage.normal[0];
            drawQuad(
                *device, quad, shader, header.texture,
                header.width, header.height,
                20.0F + header.width * 0.5F, 40.0F,
                15.0F, transparent);
            const auto& money =
                workshopControlsPage.normal[1];
            drawQuad(
                *device, quad, shader, money.texture,
                money.width, money.height,
                menu::virtualWidth - 53.0F -
                    money.width * 0.5F,
                menu::virtualHeight - 29.0F,
                15.0F, transparent);

            const bool backFocused =
                menuSelection == 0U;
            const float backX =
                static_cast<float>(
                    garageBackImage.width) *
                0.5F;
            const float backY =
                menu::virtualHeight -
                static_cast<float>(
                    workshopBottomPanelImage.height) +
                40.0F;
            drawQuad(
                *device, quad, shader,
                backFocused ? garageBackSelected
                            : garageBack,
                static_cast<float>(
                    backFocused
                        ? garageBackSelectedImage.width
                        : garageBackImage.width),
                static_cast<float>(
                    backFocused
                        ? garageBackSelectedImage.height
                        : garageBackImage.height),
                backX, backY, 35.0F, transparent);
            const auto& backText =
                backFocused
                    ? workshopControlsPage.selected[0]
                    : workshopControlsPage.normal[0];
            drawQuad(
                *device, quad, shader, backText.texture,
                backText.width, backText.height, backX,
                backY, 15.0F, transparent);

            for (std::size_t line = 0U;
                 line < workshopHintPage.normal.size();
                 ++line)
            {
                const auto& hint =
                    workshopHintPage.normal[line];
                drawQuad(
                    *device, quad, shader, hint.texture,
                    hint.width, hint.height, 480.0F,
                    menu::virtualHeight - 93.0F +
                        static_cast<float>(line) *
                            19.0F,
                    15.0F, transparent);
            }

            if (previewItem != nullptr &&
                !workshopDrag.active() &&
                !workshopInfoPage.normal.empty())
            {
                float anchorX = centerX;
                float anchorY =
                    menu::virtualHeight * 0.5F;
                if (menuSelection >= 1U &&
                    menuSelection < 13U)
                {
                    const auto visible =
                        menuSelection - 1U;
                    anchorX =
                        goodCenters[visible][0] + 130.0F;
                    anchorY =
                        goodCenters[visible][1] + 95.0F;
                }
                else if (menuSelection >= 13U &&
                         menuSelection < 23U)
                {
                    const auto slot =
                        menuSelection - 13U;
                    anchorX =
                        slotCenters[slot][0] + 130.0F;
                    anchorY =
                        slotCenters[slot][1] + 95.0F;
                }
                const float halfInfoWidth =
                    static_cast<float>(
                        workshopInfoFrameImage.width) *
                    0.5F;
                const float halfInfoHeight =
                    static_cast<float>(
                        workshopInfoFrameImage.height) *
                    0.5F;
                anchorX = std::clamp(
                    anchorX, halfInfoWidth,
                    menu::virtualWidth - halfInfoWidth);
                anchorY = std::clamp(
                    anchorY, halfInfoHeight,
                    menu::virtualHeight - halfInfoHeight);
                drawQuad(
                    *device, quad, shader,
                    workshopInfoFrame,
                    static_cast<float>(
                        workshopInfoFrameImage.width),
                    static_cast<float>(
                        workshopInfoFrameImage.height),
                    anchorX, anchorY, 12.0F,
                    transparent);

                const bool hasDamage =
                    previewItem->projectileDamage > 0.0F;
                const std::size_t trailing =
                    hasDamage ? 2U : 1U;
                const std::size_t descriptionCount =
                    workshopInfoPage.normal.size() >
                            1U + trailing
                        ? workshopInfoPage.normal.size() -
                              1U - trailing
                        : 0U;
                const auto& name =
                    workshopInfoPage.normal[0];
                drawQuad(
                    *device, quad, shader, name.texture,
                    name.width, name.height, anchorX,
                    anchorY - 58.0F, 8.0F,
                    transparent);
                for (std::size_t line = 0U;
                     line < descriptionCount; ++line)
                {
                    const auto& info =
                        workshopInfoPage.normal[1U + line];
                    const float scale = std::min(
                        1.0F,
                        280.0F /
                            std::max(info.width, 1.0F));
                    drawQuad(
                        *device, quad, shader,
                        info.texture,
                        info.width * scale,
                        info.height * scale, anchorX,
                        anchorY - 23.0F +
                            static_cast<float>(line) *
                                17.0F,
                        8.0F, transparent);
                }
                const std::size_t costIndex =
                    1U + descriptionCount;
                if (costIndex <
                    workshopInfoPage.normal.size())
                {
                    const auto& cost =
                        workshopInfoPage.normal[costIndex];
                    drawQuad(
                        *device, quad, shader,
                        cost.texture, cost.width,
                        cost.height, anchorX - 60.0F,
                        anchorY + 54.0F, 8.0F,
                        transparent);
                }
                if (hasDamage &&
                    costIndex + 1U <
                        workshopInfoPage.normal.size())
                {
                    const auto& damage =
                        workshopInfoPage
                            .normal[costIndex + 1U];
                    drawQuad(
                        *device, quad, shader,
                        damage.texture, damage.width,
                        damage.height, anchorX + 80.0F,
                        anchorY + 54.0F, 8.0F,
                        transparent);
                }
            }

            if (workshopDrag.active())
            {
                if (const auto* dragged =
                        originalGarage->findItem(
                            workshopDrag.item.record))
                {
                    const float viewportSize = std::min(
                        static_cast<float>(
                            workshopSlotImage.width),
                        static_cast<float>(
                            workshopSlotImage.height));
                    workshopRenderer.drawItem(
                        *device, raceShader, *dragged,
                        workshopDragX, workshopDragY,
                        viewportSize, viewportSize, 0.0F,
                        racePipeline);
                }
            }
            if (workshopConfirmation !=
                WorkshopConfirmation::None)
            {
                const float dialogY =
                    menu::virtualHeight * 0.5F;
                drawQuad(
                    *device, quad, shader, acceptFrame,
                    static_cast<float>(
                        acceptFrameImage.width),
                    static_cast<float>(
                        acceptFrameImage.height),
                    centerX, dialogY, 6.0F, transparent);
                if (!workshopConfirmationPage.normal.empty())
                {
                    const auto& message =
                        workshopConfirmationPage.normal[0];
                    const float scale = std::min(
                        1.0F,
                        300.0F /
                            std::max(message.width, 1.0F));
                    drawQuad(
                        *device, quad, shader,
                        message.texture,
                        message.width * scale,
                        message.height * scale, centerX,
                        dialogY - 35.0F, 4.0F,
                        transparent);
                }
                auto drawWorkshopChoice =
                    [&](bool yes, float x) {
                        const bool selected =
                            workshopConfirmationYesFocused ==
                            yes;
                        drawQuad(
                            *device, quad, shader,
                            selected
                                ? acceptButtonSelected
                                : acceptButton,
                            static_cast<float>(
                                selected
                                    ? acceptButtonSelectedImage
                                          .width
                                    : acceptButtonImage.width),
                            static_cast<float>(
                                selected
                                    ? acceptButtonSelectedImage
                                          .height
                                    : acceptButtonImage.height),
                            x, dialogY + 32.0F, 3.0F,
                            transparent);
                        const auto& label =
                            yes
                                ? (selected
                                       ? exitRaceYesSelected
                                       : exitRaceYes)
                                : (selected
                                       ? exitRaceNoSelected
                                       : exitRaceNo);
                        drawQuad(
                            *device, quad, shader,
                            label.texture, label.width,
                            label.height, x,
                            dialogY + 32.0F, 2.0F,
                            transparent);
                    };
                drawWorkshopChoice(
                    true, centerX - 70.0F);
                drawWorkshopChoice(
                    false, centerX + 70.0F);
            }
        }
        else if (drawingOriginalAngar)
        {
            raceAngarFrameObserved = true;
            const auto planetCount = std::min(
                originalGarage->planets.size(),
                profileState.player.planets.size());
            const float panelCenterX = menu::virtualWidth * 0.5F;
            const float panelCenterY =
                menu::virtualHeight -
                static_cast<float>(angarBottomPanelImage.height) *
                    0.5F -
                20.0F;
            drawQuad(
                *device, quad, shader, angarBottomPanel,
                static_cast<float>(angarBottomPanelImage.width),
                static_cast<float>(angarBottomPanelImage.height),
                panelCenterX, panelCenterY, 60.0F, transparent);

            float doorAlpha = 1.0F;
            if (angarDoorTime >= 0.0F)
            {
                doorAlpha = std::clamp(
                    angarDoorTime / 0.25F, 0.0F, 1.0F);
                angarDoorTime += frameSeconds;
                if (doorAlpha >= 1.0F)
                    angarDoorTime = -1.0F;
            }
            constexpr float planetSpacing = 224.0F;
            const float firstPlanetX =
                panelCenterX -
                static_cast<float>(
                    angarBottomPanelImage.width) *
                    0.5F +
                35.0F + 90.0F;
            const float planetY =
                panelCenterY -
                static_cast<float>(
                    angarBottomPanelImage.height) *
                    0.5F +
                90.0F;
            const float slotY = panelCenterY + 67.0F;
            for (std::size_t index = 0U;
                 index < planetCount; ++index)
            {
                const float x =
                    firstPlanetX +
                    static_cast<float>(index) * planetSpacing;
                const bool selected =
                    static_cast<int>(index) == angarPlanetIndex;
                const bool previous =
                    static_cast<int>(index) ==
                    angarPreviousPlanetIndex;
                const float animation =
                    selected
                        ? angarSceneSeconds * bx::kPi / 24.0F
                        : 0.0F;
                workshopRenderer.drawPlanet(
                    *device, raceShader,
                    originalGarage->planets[index],
                    x, planetY, 180.0F, 180.0F,
                    animation, racePipeline);

                const bool focused =
                    menuSelection == index && selected;
                drawQuad(
                    *device, quad, shader,
                    focused ? angarDoorSlotSelected
                            : angarDoorSlot,
                    static_cast<float>(
                        focused
                            ? angarDoorSlotSelectedImage.width
                            : angarDoorSlotImage.width),
                    static_cast<float>(
                        focused
                            ? angarDoorSlotSelectedImage.height
                            : angarDoorSlotImage.height),
                    x, slotY, 35.0F, transparent);

                float alpha = selected ? 1.0F : 0.0F;
                if (angarDoorTime >= 0.0F ||
                    doorAlpha < 1.0F)
                {
                    if (selected)
                        alpha = doorAlpha;
                    else if (previous)
                        alpha = 1.0F - doorAlpha;
                }
                const float offset = 16.0F * alpha;
                drawQuad(
                    *device, quad, shader, angarDoorDown,
                    static_cast<float>(angarDoorDownImage.width),
                    static_cast<float>(angarDoorDownImage.height),
                    x - 1.0F,
                    slotY -
                        static_cast<float>(
                            angarDoorSlotImage.height) *
                            0.5F +
                        static_cast<float>(
                            angarDoorDownImage.height) *
                            0.5F -
                        4.0F + offset,
                    24.0F, transparent);
                drawQuad(
                    *device, quad, shader, angarDoorUp,
                    static_cast<float>(angarDoorUpImage.width),
                    static_cast<float>(angarDoorUpImage.height),
                    x - 1.0F,
                    slotY +
                        static_cast<float>(
                            angarDoorSlotImage.height) *
                            0.5F -
                        static_cast<float>(
                            angarDoorUpImage.height) *
                            0.5F +
                        4.0F - offset,
                    24.0F, transparent);
                if (index < planetsPage.normal.size())
                {
                    const auto& status =
                        focused
                            ? planetsPage.selected[index]
                            : planetsPage.normal[index];
                    const float scale = std::min(
                        1.0F,
                        150.0F /
                            std::max(status.width, 1.0F));
                    drawQuad(
                        *device, quad, shader,
                        status.texture,
                        status.width * scale,
                        status.height * scale,
                        x, slotY, 15.0F, transparent);
                }
            }

            const std::size_t backIndex = planetCount;
            const bool backFocused =
                menuSelection == backIndex;
            const float backX =
                static_cast<float>(garageBackImage.width) * 0.5F;
            const float backY = 40.0F;
            drawQuad(
                *device, quad, shader,
                backFocused ? garageBackSelected : garageBack,
                static_cast<float>(
                    backFocused
                        ? garageBackSelectedImage.width
                        : garageBackImage.width),
                static_cast<float>(
                    backFocused
                        ? garageBackSelectedImage.height
                        : garageBackImage.height),
                backX, backY, 35.0F, transparent);
            if (backIndex < planetsPage.normal.size())
            {
                const auto& backText =
                    backFocused
                        ? planetsPage.selected[backIndex]
                        : planetsPage.normal[backIndex];
                drawQuad(
                    *device, quad, shader, backText.texture,
                    backText.width, backText.height,
                    backX, backY, 15.0F, transparent);
            }

            if (angarPlanetIndex >= 0 &&
                static_cast<std::size_t>(angarPlanetIndex) <
                    planetCount)
            {
                const auto index =
                    static_cast<std::size_t>(angarPlanetIndex);
                const float planetX =
                    firstPlanetX +
                    static_cast<float>(index) * planetSpacing;
                const float infoWidth =
                    static_cast<float>(angarPlanetInfoImage.width);
                const float infoHeight =
                    static_cast<float>(angarPlanetInfoImage.height);
                const float infoX = std::clamp(
                    planetX, infoWidth * 0.5F,
                    menu::virtualWidth - infoWidth * 0.5F);
                const float infoY = panelCenterY - 260.0F;
                const float infoLeft = infoX - infoWidth * 0.5F;
                const float infoTop = infoY - infoHeight * 0.5F;
                drawQuad(
                    *device, quad, shader, angarPlanetInfo,
                    infoWidth, infoHeight, infoX, infoY,
                    13.0F, transparent);
                drawQuad(
                    *device, quad, shader, angarClose,
                    static_cast<float>(angarCloseImage.width),
                    static_cast<float>(angarCloseImage.height),
                    infoX + 180.0F, infoTop + 15.0F,
                    10.0F, transparent);

                const auto& photoImage = angarBossImages[index];
                const float photoScale = std::min(
                    110.0F /
                        std::max<float>(
                            static_cast<float>(photoImage.width),
                            1.0F),
                    85.0F /
                        std::max<float>(
                            static_cast<float>(photoImage.height),
                            1.0F));
                drawQuad(
                    *device, quad, shader,
                    angarBossTextures[index],
                    static_cast<float>(photoImage.width) *
                        photoScale,
                    static_cast<float>(photoImage.height) *
                        photoScale,
                    infoX + 107.0F, infoTop + 89.0F,
                    9.0F, transparent);
                workshopRenderer.drawCar(
                    *device, raceShader,
                    originalGarage->planets[index]
                        .bossCarRecord,
                    infoX + 107.0F, infoTop + 202.0F,
                    110.0F, 110.0F,
                    angarSceneSeconds * bx::kPi * 0.5F,
                    racePipeline);

                if (!angarInfoPage.normal.empty())
                {
                    const auto& name =
                        angarInfoPage.normal[0];
                    drawQuad(
                        *device, quad, shader,
                        name.texture, name.width,
                        name.height, infoX,
                        infoTop + 11.0F, 7.0F,
                        transparent);
                }
                if (angarInfoPage.normal.size() > 1U)
                {
                    const auto& boss =
                        angarInfoPage.normal[1];
                    drawQuad(
                        *device, quad, shader,
                        boss.texture, boss.width,
                        boss.height, infoX - 60.0F,
                        infoTop + 42.0F, 7.0F,
                        transparent);
                }
                for (std::size_t line = 2U;
                     line < angarInfoPage.normal.size();
                     ++line)
                {
                    const auto& text =
                        angarInfoPage.normal[line];
                    drawQuad(
                        *device, quad, shader,
                        text.texture, text.width,
                        text.height,
                        infoLeft + 15.0F +
                            text.width * 0.5F,
                        infoTop + 64.0F +
                            static_cast<float>(line - 2U) *
                                18.0F,
                        7.0F, transparent);
                }
            }

            if (angarWarningVisible)
            {
                const float centerX =
                    menu::virtualWidth * 0.5F;
                const float centerY =
                    menu::virtualHeight * 0.5F;
                drawQuad(
                    *device, quad, shader, acceptFrame,
                    static_cast<float>(acceptFrameImage.width),
                    static_cast<float>(acceptFrameImage.height),
                    centerX, centerY, 6.0F, transparent);
                const float messageScale = std::min(
                    1.0F,
                    300.0F /
                        std::max(angarWarningMessage.width, 1.0F));
                drawQuad(
                    *device, quad, shader,
                    angarWarningMessage.texture,
                    angarWarningMessage.width * messageScale,
                    angarWarningMessage.height * messageScale,
                    centerX, centerY - 35.0F,
                    4.0F, transparent);
                drawQuad(
                    *device, quad, shader,
                    acceptButtonSelected,
                    static_cast<float>(
                        acceptButtonSelectedImage.width),
                    static_cast<float>(
                        acceptButtonSelectedImage.height),
                    centerX, centerY + 32.0F,
                    3.0F, transparent);
                drawQuad(
                    *device, quad, shader, angarOk.texture,
                    angarOk.width, angarOk.height,
                    centerX, centerY + 32.0F,
                    2.0F, transparent);
            }
            else if (angarTravelDialogVisible)
            {
                const float centerX =
                    menu::virtualWidth * 0.5F;
                const float centerY =
                    menu::virtualHeight * 0.5F;
                drawQuad(
                    *device, quad, shader, acceptFrame,
                    static_cast<float>(acceptFrameImage.width),
                    static_cast<float>(acceptFrameImage.height),
                    centerX, centerY, 6.0F, transparent);
                const float messageScale = std::min(
                    1.0F,
                    300.0F /
                        std::max(angarTravelMessage.width, 1.0F));
                drawQuad(
                    *device, quad, shader,
                    angarTravelMessage.texture,
                    angarTravelMessage.width * messageScale,
                    angarTravelMessage.height * messageScale,
                    centerX, centerY - 35.0F,
                    4.0F, transparent);
                auto drawAngarChoice =
                    [&](bool yes, float x) {
                        const bool selected =
                            angarTravelYesFocused == yes;
                        drawQuad(
                            *device, quad, shader,
                            selected ? acceptButtonSelected
                                     : acceptButton,
                            static_cast<float>(
                                selected
                                    ? acceptButtonSelectedImage.width
                                    : acceptButtonImage.width),
                            static_cast<float>(
                                selected
                                    ? acceptButtonSelectedImage.height
                                    : acceptButtonImage.height),
                            x, centerY + 32.0F, 3.0F,
                            transparent);
                        const auto& label =
                            yes
                                ? (selected
                                       ? exitRaceYesSelected
                                       : exitRaceYes)
                                : (selected
                                       ? exitRaceNoSelected
                                       : exitRaceNo);
                        drawQuad(
                            *device, quad, shader,
                            label.texture, label.width,
                            label.height, x,
                            centerY + 32.0F, 2.0F,
                            transparent);
                    };
                drawAngarChoice(true, centerX - 70.0F);
                drawAngarChoice(false, centerX + 70.0F);
            }
        }
        else if (drawingOriginalAchievements)
        {
            raceAchievementFrameObserved = true;
            const float centerX = menu::virtualWidth * 0.5F;
            const float centerY = menu::virtualHeight * 0.5F;
            const float scale = std::min(
                menu::virtualWidth / 1090.0F,
                menu::virtualHeight / 720.0F);
            drawQuad(
                *device, quad, shader, achievementBackground,
                menu::virtualWidth, menu::virtualHeight,
                centerX, centerY, 85.0F, transparent);
            drawQuad(
                *device, quad, shader, achievementPanel,
                static_cast<float>(achievementPanelImage.width) *
                    scale,
                static_cast<float>(achievementPanelImage.height) *
                    scale,
                centerX,
                static_cast<float>(achievementPanelImage.height) *
                    scale * 0.5F,
                60.0F, transparent);

            const float bottomPanelPositionY =
                menu::virtualHeight - 80.0F;
            drawQuad(
                *device, quad, shader, achievementBottomPanel,
                menu::virtualWidth,
                static_cast<float>(
                    achievementBottomPanelImage.height),
                centerX,
                bottomPanelPositionY -
                    static_cast<float>(
                        achievementBottomPanelImage.height) *
                        0.5F,
                60.0F, transparent);

            drawQuad(
                *device, quad, shader, achievementRewards.texture,
                achievementRewards.width * scale,
                achievementRewards.height * scale, centerX,
                555.0F * scale, 15.0F, transparent);
            drawQuad(
                *device, quad, shader, achievementPoints.texture,
                achievementPoints.width, achievementPoints.height,
                centerX, bottomPanelPositionY + 40.0F,
                15.0F, transparent);

            for (std::size_t index = 0U;
                 index < originalAchievementVisuals.size(); ++index)
            {
                const auto state = achievementState(index);
                const bool opened = state == "asOpened";
                const bool unlocked = state == "asUnlocked";
                const auto& visual =
                    originalAchievementVisuals[index];
                const auto& image =
                    opened ? achievementOpenedImages[index]
                           : achievementLockedImages[index];
                const Texture imageTexture =
                    opened ? achievementOpenedTextures[index]
                           : achievementLockedTextures[index];
                const float imageX =
                    centerX + (visual.x - 30.0F) * scale;
                const float imageY = visual.y * scale;
                const float imageWidth =
                    static_cast<float>(image.width) * scale;
                const float imageHeight =
                    static_cast<float>(image.height) * scale;
                drawQuad(
                    *device, quad, shader, imageTexture,
                    imageWidth, imageHeight, imageX, imageY,
                    45.0F, transparent);

                const bool focused =
                    menuSelection == index && unlocked;
                const Texture buttonTexture =
                    opened
                        ? achievementOkSelected
                        : unlocked
                              ? (focused
                                     ? achievementOkSelected
                                     : achievementOk)
                              : achievementClose;
                const auto& buttonImage =
                    opened || (unlocked && focused)
                        ? achievementOkSelectedImage
                        : unlocked ? achievementOkImage
                                   : achievementCloseImage;
                const float buttonX =
                    imageX +
                    (static_cast<float>(image.width) * 0.5F -
                     15.0F) *
                        scale;
                const float buttonY =
                    imageY +
                    (static_cast<float>(image.height) * 0.5F -
                     25.0F) *
                        scale;
                drawQuad(
                    *device, quad, shader, buttonTexture,
                    static_cast<float>(buttonImage.width) * scale,
                    static_cast<float>(buttonImage.height) * scale,
                    buttonX, buttonY, 30.0F, transparent);
                if (unlocked &&
                    index < achievementPricePage.normal.size())
                {
                    const auto& price =
                        achievementPricePage.normal[index];
                    drawQuad(
                        *device, quad, shader, price.texture,
                        price.width * scale,
                        price.height * scale, imageX, buttonY,
                        18.0F, transparent);
                }
            }

            const bool backFocused =
                menuSelection == originalAchievementBack;
            const float backX =
                static_cast<float>(garageBackImage.width) * 0.5F;
            const float backY =
                bottomPanelPositionY + 17.0F +
                static_cast<float>(garageBackImage.height) * 0.5F;
            drawQuad(
                *device, quad, shader,
                backFocused ? garageBackSelected : garageBack,
                static_cast<float>(
                    backFocused ? garageBackSelectedImage.width
                                : garageBackImage.width),
                static_cast<float>(
                    backFocused ? garageBackSelectedImage.height
                                : garageBackImage.height),
                backX, backY, 35.0F, transparent);
            const auto& backText =
                backFocused
                    ? achievementsPage.selected[
                          originalAchievementBack]
                    : achievementsPage.normal[
                          originalAchievementBack];
            drawQuad(
                *device, quad, shader, backText.texture,
                backText.width, backText.height, backX, backY,
                15.0F, transparent);

            if (achievementPointsWarningVisible)
            {
                drawQuad(
                    *device, quad, shader, acceptFrame,
                    static_cast<float>(acceptFrameImage.width),
                    static_cast<float>(acceptFrameImage.height),
                    centerX, centerY, 6.0F, transparent);
                const float messageScale = std::min(
                    1.0F,
                    300.0F /
                        std::max(
                            achievementNotEnoughPoints.width, 1.0F));
                drawQuad(
                    *device, quad, shader,
                    achievementNotEnoughPoints.texture,
                    achievementNotEnoughPoints.width * messageScale,
                    achievementNotEnoughPoints.height * messageScale,
                    centerX, centerY - 35.0F,
                    4.0F, transparent);
                drawQuad(
                    *device, quad, shader,
                    acceptButtonSelected,
                    static_cast<float>(
                        acceptButtonSelectedImage.width),
                    static_cast<float>(
                        acceptButtonSelectedImage.height),
                    centerX, centerY + 32.0F,
                    3.0F, transparent);
                drawQuad(
                    *device, quad, shader, angarOk.texture,
                    angarOk.width, angarOk.height,
                    centerX, centerY + 32.0F,
                    2.0F, transparent);
            }
            else if (achievementPurchaseDialogVisible)
            {
                drawQuad(
                    *device, quad, shader, acceptFrame,
                    static_cast<float>(acceptFrameImage.width),
                    static_cast<float>(acceptFrameImage.height),
                    centerX, centerY, 6.0F, transparent);
                const float messageScale = std::min(
                    1.0F,
                    300.0F /
                        std::max(
                            achievementPurchaseMessage.width, 1.0F));
                drawQuad(
                    *device, quad, shader,
                    achievementPurchaseMessage.texture,
                    achievementPurchaseMessage.width * messageScale,
                    achievementPurchaseMessage.height * messageScale,
                    centerX, centerY - 35.0F,
                    4.0F, transparent);
                auto drawAchievementChoice =
                    [&](bool yes, float x) {
                        const bool selected =
                            achievementPurchaseYesFocused == yes;
                        drawQuad(
                            *device, quad, shader,
                            selected ? acceptButtonSelected
                                     : acceptButton,
                            static_cast<float>(
                                selected
                                    ? acceptButtonSelectedImage.width
                                    : acceptButtonImage.width),
                            static_cast<float>(
                                selected
                                    ? acceptButtonSelectedImage.height
                                    : acceptButtonImage.height),
                            x, centerY + 32.0F, 3.0F,
                            transparent);
                        const auto& label =
                            yes
                                ? (selected
                                       ? exitRaceYesSelected
                                       : exitRaceYes)
                                : (selected
                                       ? exitRaceNoSelected
                                       : exitRaceNo);
                        drawQuad(
                            *device, quad, shader,
                            label.texture, label.width,
                            label.height, x,
                            centerY + 32.0F, 2.0F,
                            transparent);
                    };
                drawAchievementChoice(true, centerX - 70.0F);
                drawAchievementChoice(false, centerX + 70.0F);
            }
        }
        else
#endif
        {
            for (std::size_t index = 0;
                 index < activePage.normal.size(); ++index)
            {
                const float centerX =
                    menu::virtualWidth * 0.5F +
                    menu::itemCenterOffsetX;
                const float centerY =
                    menu::virtualHeight * 0.5F +
                    menu::firstItemOffsetY +
                    static_cast<float>(index) *
                        menu::itemSpacing;
                if (index == menuSelection)
                {
                    drawQuad(
                        *device, quad, shader, selection,
                        static_cast<float>(
                            model->selectionImage.width),
                        static_cast<float>(
                            model->selectionImage.height),
                        centerX, centerY, 50.0F, transparent);
                }
                const auto& text =
                    index == menuSelection
                        ? activePage.selected[index]
                        : activePage.normal[index];
                drawQuad(
                    *device, quad, shader, text.texture,
                    text.width, text.height, centerX, centerY,
                    25.0F, transparent);
            }
        }
        if (menuStack.back() == MenuScreen::Credits)
        {
            const float scroll =
                std::fmod(
                    static_cast<float>(SDL_GetTicks()) * 0.02F,
                    credits.height + 400.0F);
            drawQuad(
                *device, quad, shader, credits.texture,
                std::min(credits.width, 700.0F), credits.height,
                menu::virtualWidth * 0.5F - 260.0F,
                menu::virtualHeight + credits.height * 0.5F -
                    scroll,
                30.0F, transparent);
        }
#ifdef RRR3D_PHYSICS
        if (menuStack.back() == MenuScreen::Finish)
        {
            const float scale = std::min(
                {1.0F,
                 1050.0F / std::max(finishSummary.width, 1.0F),
                 430.0F / std::max(finishSummary.height, 1.0F)});
            drawQuad(
                *device, quad, shader, finishSummary.texture,
                finishSummary.width * scale,
                finishSummary.height * scale,
                menu::virtualWidth * 0.5F,
                menu::virtualHeight * 0.70F,
                30.0F, transparent);
        }
#endif

#ifdef RRR3D_PHYSICS
        if (!drawingOriginalOptions && !drawingOriginalRaceMenu &&
            !drawingOriginalGarage && !drawingOriginalWorkshop &&
            !drawingOriginalAngar && !drawingOriginalAchievements)
#endif
        {
            const float versionX =
                menu::virtualWidth - 25.0F -
                version.width * 0.5F;
            const float versionY =
                menu::virtualHeight - 25.0F -
                version.height * 0.5F;
            drawQuad(
                *device, quad, shader, version.texture,
                version.width, version.height, versionX,
                versionY, 25.0F, transparent);
        }
        device->endFrame();
#ifdef RRR3D_PHYSICS
        }
#endif

        ++renderedFrames;
        if (options->smokeFrames != 0 &&
            renderedFrames >= options->smokeFrames
#ifdef RRR3D_AUDIO
            && (!options->audioSmokeTest ||
                musicSmokePhase == MusicSmokePhase::Complete)
#endif
        )
        {
#ifdef RRR3D_PHYSICS
            if (options->raceRenderSmokeTest)
            {
                auto passObserved =
                    [&](r3d::renderer::RenderPass pass,
                        bool requireDraw = true) {
                        const auto index =
                            static_cast<std::size_t>(pass);
                        return maximumRacePassBegins[index] > 0U &&
                               (!requireDraw ||
                                maximumRacePassDraws[index] > 0U);
                    };
                bool renderGraphComplete =
                    passObserved(
                        r3d::renderer::RenderPass::
                            EnvironmentPositiveX) &&
                    passObserved(
                        r3d::renderer::RenderPass::
                            EnvironmentNegativeX) &&
                    passObserved(
                        r3d::renderer::RenderPass::
                            EnvironmentPositiveY) &&
                    passObserved(
                        r3d::renderer::RenderPass::
                            EnvironmentNegativeY) &&
                    passObserved(
                        r3d::renderer::RenderPass::
                            EnvironmentPositiveZ) &&
                    passObserved(
                        r3d::renderer::RenderPass::
                            EnvironmentNegativeZ) &&
                    passObserved(r3d::renderer::RenderPass::Shadow) &&
                    passObserved(
                        r3d::renderer::RenderPass::ShadowFar) &&
                    passObserved(r3d::renderer::RenderPass::Scene) &&
                    passObserved(
                        r3d::renderer::RenderPass::Luminance64) &&
                    passObserved(
                        r3d::renderer::RenderPass::Luminance16) &&
                    passObserved(
                        r3d::renderer::RenderPass::Luminance4) &&
                    passObserved(
                        r3d::renderer::RenderPass::Luminance1) &&
                    passObserved(
                        r3d::renderer::RenderPass::LuminanceAdapt) &&
                    passObserved(
                        r3d::renderer::RenderPass::BloomExtract) &&
                    passObserved(
                        r3d::renderer::RenderPass::BloomHorizontal) &&
                    passObserved(
                        r3d::renderer::RenderPass::BloomVertical) &&
                    passObserved(
                        r3d::renderer::RenderPass::Composite) &&
                    passObserved(
                        r3d::renderer::RenderPass::Overlay, false);
                const bool expectsReflection =
                    originalRace->environment.planarReflection ||
                    originalRace->environment.surface ==
                        r3d::game::originalrace::
                            EnvironmentSurface::Water;
                const bool expectsWater =
                    originalRace->environment.surface ==
                    r3d::game::originalrace::
                        EnvironmentSurface::Water;
                const bool expectsBumpMapping =
                    std::any_of(
                        originalRace->trackDefinitions.begin(),
                        originalRace->trackDefinitions.end(),
                        [](const auto& definition) {
                            return definition.lighting ==
                                   r3d::game::originalrace::
                                       LightingMode::Bump;
                        });
                // FxTrail is driven by the source per-wheel slip thresholds,
                // not by vehicle speed. The canonical World1 smoke validates
                // its render path; a high-grip surface is allowed to finish
                // this short run without inventing a skid solely for test
                // coverage.
                const bool expectsWheelSlipTrail =
                    originalRace->levelPath ==
                    "Data/Map/World1/map1.r3dMap";
                if (expectsReflection)
                    renderGraphComplete =
                        renderGraphComplete &&
                        passObserved(
                            r3d::renderer::RenderPass::Reflection);
                if (expectsWater)
                    renderGraphComplete =
                        renderGraphComplete &&
                        passObserved(
                            r3d::renderer::RenderPass::Water);
                if (!integratedRaceStartObserved || !inRace ||
                    !raceGarageFrameObserved ||
                    !raceGarage3DObserved ||
                    !raceWorkshopFrameObserved ||
                    !raceWorkshop3DObserved ||
                    !raceAngarFrameObserved ||
                    !raceAngar3DObserved ||
                    !raceAchievementFrameObserved ||
                    !racePauseDialogObserved ||
                    !racePauseResumeObserved ||
                    !racePauseFrozenObserved ||
                    racePlayerDestroyedObserved ||
                    minimumRacePlayerLife <= 0.0F ||
                    maximumRaceSmokeContacts == 0 ||
                    maximumRaceSmokeSpeed < 0.2F ||
                    raceVehicles.size() < 2U ||
                    !raceCameraStylesObserved[0] ||
                    !raceCameraStylesObserved[1] ||
                    !renderGraphComplete ||
                    maximumEnvironmentMappedDraws == 0U ||
                    (expectsBumpMapping &&
                     maximumNormalMappedDraws == 0U) ||
                    (expectsWheelSlipTrail &&
                     maximumTransientDraws == 0U))
                {
                    std::cerr
                        << "Milestone 9 integrated Single Player/race render "
                           "verification failed: started="
                        << integratedRaceStartObserved << ", inRace="
                        << inRace << ", garage="
                        << raceGarageFrameObserved << '/'
                        << raceGarage3DObserved
                        << ", workshop="
                        << raceWorkshopFrameObserved << '/'
                        << raceWorkshop3DObserved
                        << ", angar="
                        << raceAngarFrameObserved << '/'
                        << raceAngar3DObserved
                        << ", achievement="
                        << raceAchievementFrameObserved
                        << ", pause="
                        << racePauseDialogObserved << '/'
                        << racePauseResumeObserved << '/'
                        << racePauseFrozenObserved << ", destroyed="
                        << racePlayerDestroyedObserved << ", minLife="
                        << minimumRacePlayerLife << ", contacts="
                        << maximumRaceSmokeContacts << ", maxSpeed="
                        << maximumRaceSmokeSpeed
                        << ", renderGraph="
                        << renderGraphComplete
                        << ", envMapped="
                        << maximumEnvironmentMappedDraws
                        << ", normalMapped="
                        << maximumNormalMappedDraws
                        << ", transient="
                        << maximumTransientDraws
                        << ", cars=" << raceVehicles.size()
                        << ", cameras="
                        << raceCameraStylesObserved[0] << '/'
                        << raceCameraStylesObserved[1] << '\n';
                    runtimeSmokeFailed = true;
                }
                else
                {
                    std::cout
                        << "Milestone 9.5 original Single Player/"
                        << originalRace->levelPath << '/'
                        << recordName(originalRace->vehicle.record)
                        << "/Jolt/bgfx/Metal smoke test completed after "
                        << renderedFrames << " frames; max speed "
                        << maximumRaceSmokeSpeed << ", wheel contacts "
                        << maximumRaceSmokeContacts
                        << ", renderer passes cube6/shadow2/scene/HDR64-1/"
                           "adapt/bloom/composite/HUD"
                        << (expectsReflection ? "/reflection" : "")
                        << (expectsWater ? "/water" : "")
                        << " verified; cube reflection "
                        << maximumEnvironmentMappedDraws
                        << ", normal map "
                        << maximumNormalMappedDraws
                        << ", FxTrail "
                        << maximumTransientDraws << "; "
                        << raceVehicles.size()
                        << " cars, both original camera modes and "
                           "an unshielded surviving player start (minimum "
                           "life "
                        << minimumRacePlayerLife << "), "
                           "source HudMenu pause/accept/frozen-world, "
                           "source WorkshopFrame/GarageFrame/3D CarFrame/"
                           "SpaceshipFrame/AngarFrame/AchievmentFrame and "
                           "render-target "
                           "resize "
                           "round-trip passed\n";
                }
            }
            else
#endif
#ifdef RRR3D_AUDIO
            if (!integratedAudioInputObserved)
            {
                std::cerr << "Milestone 8 integrated input/MainMenu2/audio "
                             "dispatch was not observed\n";
                runtimeSmokeFailed = true;
            }
            else
            {
                std::cout << "Milestone 8 original MainMenu2/input/audio/"
                             "MusicCat/bgfx/Metal smoke test completed after "
                          << renderedFrames << " frames\n";
            }
#elif defined(RRR3D_GAMEPAD_INPUT)
            std::cout << "Milestone 7 original MainMenu2/input/bgfx/Metal "
                         "smoke test completed after "
#else
            std::cout << "Milestone 6 original MainMenu2/bgfx/Metal smoke "
                         "test completed after "
#endif
#ifndef RRR3D_AUDIO
                      << renderedFrames << " frames\n";
#endif
            running = false;
        }
    }

#ifdef RRR3D_AUDIO
#ifdef RRR3D_PHYSICS
    stopRaceAudio(false);
    commentator.shutdown();
    gameMusic.shutdown();
    for (const auto& [path, sound] : engineSounds)
    {
        static_cast<void>(path);
        audio.unloadSound(sound);
    }
#endif
    music.shutdown();
    audio.unloadSound(clickSound);
    audio.shutdown();
    if (options->audioSmokeTest)
    {
        std::error_code removeError;
        std::filesystem::remove(musicStatePath, removeError);
        auto temporary = musicStatePath;
        temporary += ".tmp";
        std::filesystem::remove(temporary, removeError);
    }
#endif
#ifdef RRR3D_PHYSICS
    saveRaceProfile();
    physicsWorld.reset();
    raceHud.shutdown(*device);
    workshopRenderer.shutdown(*device);
    angarRenderer.shutdown(*device);
    garageRenderer.shutdown(*device);
    raceRenderer.shutdown(*device);
#endif
    releaseResources();
    device.reset();
    SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
    input.shutdown();
#endif
    SDL_Quit();
    return runtimeSmokeFailed ? EXIT_FAILURE : EXIT_SUCCESS;
}
