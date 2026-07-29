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
    MenuPageVisual garagePage;
    MenuPageVisual workshopSlotsPage;
    MenuPageVisual workshopItemsPage;
    MenuPageVisual planetsPage;
    MenuPageVisual achievementsPage;
    MenuPageVisual gameOptionsPage;
    MenuPageVisual graphicsOptionsPage;
    MenuPageVisual soundOptionsPage;
    MenuPageVisual controlsOptionsPage;
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
    auto destroyPage = [&](const MenuPageVisual& page) {
        for (const auto& item : page.selected)
            device->destroy(item.texture);
        for (const auto& item : page.normal)
            device->destroy(item.texture);
    };
#ifdef RRR3D_PHYSICS
    auto optionValue = [&](std::string_view key,
                           const std::string& value) {
        return localized(key) + ": " + value;
    };
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
    static constexpr std::size_t controlsPerPage = 4U;
    static constexpr std::size_t controlsPageCount =
        (originalControlActions.size() + controlsPerPage - 1U) /
        controlsPerPage;
    std::size_t controlsPageIndex = 0U;
    bool controlsUseGamepad = false;
    auto gameOptionsLabels = [&]() {
        return std::vector<std::string>{
            optionValue(
                "svCamera",
                localized(
                    profileState.config.preferredCamera ==
                            r3d::game::originalrace::
                                PreferredCamera::ThirdPerson
                        ? "svCameraSecView"
                        : "svCameraOrtho")),
            optionValue(
                "svCameraDist",
                distanceName(profileState.config.cameraDistance)),
            optionValue(
                "svEnableHUD",
                onOff(profileState.config.enableHud)),
            optionValue(
                "svDifficulty",
                localized(profileState.player.difficulty)),
            optionValue(
                "svSpringBorders",
                onOff(profileState.config.springBorders)),
            optionValue(
                "svUpgradeMaxLevel",
                std::to_string(
                    std::min(profileState.config.upgradeMaxLevel, 2U) +
                    1U)),
            optionValue(
                "svWeaponMaxLevel",
                std::to_string(
                    std::clamp(
                        profileState.config.weaponMaxLevel, 1U, 4U))),
            optionValue(
                "svMaxPlayers",
                std::to_string(
                    std::clamp(
                        profileState.config.maxPlayers, 2U, 6U))),
            optionValue(
                "svMaxComputers",
                std::to_string(profileState.config.maxComputers)),
            optionValue(
                "svLapsCount",
                std::to_string(profileState.config.lapsCount)),
            optionValue(
                "svEnableMineBug",
                onOff(profileState.config.enableMineBug)),
            optionValue(
                "svDisableVideo",
                onOff(profileState.config.disableVideo)),
            localized("svBack")};
    };
    auto graphicsOptionsLabels = [&]() {
        static constexpr std::array<std::string_view, 4>
            filteringNames{
                "linear", "af 2x", "af 4x", "af 8x"};
        static constexpr std::array<std::string_view, 4> msaaNames{
            "none", "aa 2x", "aa 4x", "aa 8x"};
        return std::vector<std::string>{
            optionValue(
                "svFiltering",
                std::string(filteringNames[
                    std::min<std::uint32_t>(
                        profileState.config.quality.filtering,
                        filteringNames.size() - 1U)])),
            optionValue(
                "svMultisampling",
                std::string(msaaNames[
                    std::min<std::uint32_t>(
                        profileState.config.quality.msaa,
                        msaaNames.size() - 1U)])),
            optionValue(
                "svShadow",
                qualityName(profileState.config.quality.shadow)),
            optionValue(
                "svEnv",
                qualityName(profileState.config.quality.environment)),
            optionValue(
                "svLight",
                qualityName(profileState.config.quality.light)),
            optionValue(
                "svPostProcess",
                qualityName(profileState.config.quality.postEffect)),
            optionValue(
                "svWindowMode",
                onOff(profileState.config.fullScreen)),
            localized("svBack")};
    };
    auto soundOptionsLabels = [&]() {
        return std::vector<std::string>{
            optionValue(
                "svLanguage",
                localized(profileState.config.language == "russian"
                              ? "svRussian"
                              : "svEnglish")),
            optionValue(
                "svCommentator",
                localized(
                    profileState.config.commentatorStyle == "russian"
                        ? "svRussian"
                        : "svEnglish")),
            optionValue(
                "svMusic",
                volumeName(profileState.config.musicVolume)),
            optionValue(
                "svSound",
                volumeName(profileState.config.effectsVolume)),
            optionValue(
                "svSoundDicter",
                volumeName(profileState.config.voiceVolume)),
            localized("svBack")};
    };
    auto controlsOptionsLabels = [&]() {
        std::vector<std::string> output;
        output.reserve(8U);
        output.push_back(
            std::string("Controller: ") +
            (controlsUseGamepad ? "Gamepad" : "Keyboard"));
        const auto& bindings =
            controlsUseGamepad
                ? profileState.config.gamepadControls
                : profileState.config.keyboardControls;
        const std::size_t first =
            controlsPageIndex * controlsPerPage;
        const std::size_t end = std::min(
            first + controlsPerPage, originalControlActions.size());
        for (std::size_t index = first; index < end; ++index)
        {
            const auto action = originalControlActions[index];
            const auto found =
                bindings.find(std::string(action));
            output.push_back(
                optionValue(
                    action,
                    found == bindings.end()
                        ? localized("svNull")
                        : found->second));
        }
        output.push_back(
            "Previous page  [" +
            std::to_string(controlsPageIndex + 1U) + "/" +
            std::to_string(controlsPageCount) + "]");
        output.push_back(
            "Next page  [" +
            std::to_string(controlsPageIndex + 1U) + "/" +
            std::to_string(controlsPageCount) + "]");
        output.push_back(localized("svBack"));
        return output;
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
        garagePage = createPage(
            {localized("svGarage"), localized("svMoney"),
             localized("svBuy"), localized("svBack")});
        workshopSlotsPage = createPage(
            {localized("svWorkshop"), localized("svBack")});
        workshopItemsPage = createPage(
            {localized("svWorkshop"), localized("svBack")});
        planetsPage = createPage(
            {"Planets", localized("svBack")});
        achievementsPage = createPage(
            {localized("svRewards"), localized("svBack")});
        gameOptionsPage = createPage(gameOptionsLabels());
        graphicsOptionsPage =
            createPage(graphicsOptionsLabels());
        soundOptionsPage = createPage(soundOptionsLabels());
        controlsOptionsPage =
            createPage(controlsOptionsLabels());
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
        pageValid(garagePage) &&
        pageValid(workshopSlotsPage) &&
        pageValid(workshopItemsPage) &&
        pageValid(planetsPage) &&
        pageValid(achievementsPage) &&
        pageValid(gameOptionsPage) &&
        pageValid(graphicsOptionsPage) &&
        pageValid(soundOptionsPage) &&
        pageValid(controlsOptionsPage) &&
        pageValid(finishPage) &&
        valid(finishSummary.texture) && valid(acceptFrame) &&
        valid(acceptButton) && valid(acceptButtonSelected) &&
        valid(exitRaceMessage.texture) && valid(exitRaceYes.texture) &&
        valid(exitRaceYesSelected.texture) && valid(exitRaceNo.texture) &&
        valid(exitRaceNoSelected.texture);
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
        device->destroy(finishSummary.texture);
#endif
        device->destroy(credits.texture);
        device->destroy(version.texture);
#ifdef RRR3D_PHYSICS
        destroyPage(achievementsPage);
        destroyPage(planetsPage);
        destroyPage(workshopItemsPage);
        destroyPage(workshopSlotsPage);
        destroyPage(garagePage);
        destroyPage(raceMenuPage);
        destroyPage(finishPage);
        destroyPage(controlsOptionsPage);
        destroyPage(soundOptionsPage);
        destroyPage(graphicsOptionsPage);
        destroyPage(gameOptionsPage);
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
    raceSession.applyPlayerProfile(profileState.player);
    raceSession.applyAchievementProfile(profileState);
    raceSession.setEnableMineBug(profileState.config.enableMineBug);
    raceSession.setSpringBorders(profileState.config.springBorders);
    rrr3d::race::OriginalRaceRenderer raceRenderer;
    rrr3d::race::OriginalRaceHud raceHud;
    if (!physicsWorld ||
        !raceRenderer.initialize(*device, *resources, *originalRace,
                                 static_cast<std::uint32_t>(pixelWidth),
                                 static_cast<std::uint32_t>(pixelHeight),
                                 physicsError) ||
        !raceHud.initialize(*device, *resources, *originalRace,
                            activeLanguage,
                            profileState.player.difficulty,
                            physicsError))
    {
        std::cerr << "Original race initialization failed: " << physicsError
                  << '\n';
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
                static_cast<std::uint32_t>(pixelHeight), resizeError))
        {
            std::cerr << "M9.3 renderer target resize round-trip failed: "
                      << resizeError << '\n';
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
    const auto pickupAudio =
        loadEngineSound("Data/Sounds/UI/pickup_up.ogg");
    const auto acceptanceAudio =
        loadEngineSound("Data/Sounds/UI/acception.ogg");
    const auto shieldAudio =
        loadEngineSound("Data/Sounds/shieldOn.ogg");
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
        pickupAudio != r3d::audio::invalidSound &&
        acceptanceAudio != r3d::audio::invalidSound &&
        shieldAudio != r3d::audio::invalidSound &&
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
        WorkshopSlots,
        WorkshopItems,
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
    std::size_t garageCarIndex = 0;
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
    r3d::game::originalrace::GarageSlotType workshopSlot =
        r3d::game::originalrace::GarageSlotType::Wheel;
    std::vector<const r3d::game::originalrace::OriginalWorkshopItem*>
        workshopItemChoices;
    std::vector<std::string> achievementChoices;
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
        case MenuScreen::WorkshopSlots:
            return workshopSlotsPage;
        case MenuScreen::WorkshopItems:
            return workshopItemsPage;
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
                    reloadError))
            {
                std::cerr
                    << "Unable to reload original tournament race: "
                    << reloadError << '\n';
                return false;
            }
            raceSession.reset();
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
    auto replacePage = [&](MenuPageVisual& page,
                           std::vector<std::string> pageLabels) {
        auto replacement = createPage(std::move(pageLabels));
        destroyPage(page);
        page = std::move(replacement);
        menuSelection =
            std::min(menuSelection, page.labels.size() - 1U);
    };
    auto currency = [](std::uint32_t value) {
        return "$" + std::to_string(value);
    };
    auto itemLabel = [&](std::string_view record) {
        const auto* item = originalGarage->findItem(record);
        return item == nullptr ? std::string(record)
                               : localized(item->name);
    };
    auto refreshGaragePage = [&]() {
        if (originalGarage->cars.empty())
            return;
        garageCarIndex =
            std::min(garageCarIndex,
                     originalGarage->cars.size() - 1U);
        const auto& car = originalGarage->cars[garageCarIndex];
        const bool current =
            car.record == profileState.player.currentCar;
        const bool unlocked =
            r3d::game::originalrace::originalCarUnlocked(
                *originalGarage, profileState, car,
                championshipMode);
        std::string state =
            current ? "Selected"
                    : unlocked ? currency(car.cost)
                               : localized("svLockedCarName");
        replacePage(
            garagePage,
            {std::to_string(garageCarIndex + 1U) + "/" +
                 std::to_string(originalGarage->cars.size()) + "  " +
                 localized(car.name),
             localized("svMoney") + ": " +
                 currency(profileState.player.money),
             state,
             current
                 ? "Selected"
                 : unlocked ? localized("svBuy")
                            : localized("svLockedCarName"),
             localized("svBack")});
    };
    auto refreshWorkshopSlotsPage = [&]() {
        static constexpr std::array<std::string_view, 10> names{
            "Wheels", "Exhaust", "Armor", "Engine", "Hyper",
            "Mine", "Weapon 1", "Weapon 2", "Weapon 3",
            "Weapon 4"};
        std::vector<std::string> output;
        output.reserve(names.size() + 1U);
        for (std::size_t index = 0; index < names.size(); ++index)
        {
            const auto& slot = profileState.player.slots[index];
            std::string value =
                slot.record.empty() ? "-" : itemLabel(slot.record);
            if (slot.hasCharge)
                value += "  " + std::to_string(slot.charge);
            output.push_back(
                std::string(names[index]) + ": " + value);
        }
        output.push_back(localized("svBack"));
        replacePage(workshopSlotsPage, std::move(output));
    };
    auto refreshWorkshopItemsPage = [&]() {
        workshopItemChoices.clear();
        const auto slotIndex =
            static_cast<std::size_t>(workshopSlot);
        const auto* car =
            originalGarage->findCar(profileState.player.currentCar);
        if (car != nullptr &&
            slotIndex < car->placements.size())
        {
            const auto& placement = car->placements[slotIndex];
            for (const auto& record : placement.supportedItems)
            {
                const auto* item =
                    originalGarage->findItem(record);
                if (item == nullptr)
                    continue;
                if (record == profileState.player.slots[slotIndex].record ||
                    record == placement.defaultItem ||
                    r3d::game::originalrace::
                        originalWorkshopItemUnlocked(
                            *originalGarage, profileState, *item))
                {
                    workshopItemChoices.push_back(item);
                }
            }
        }
        std::vector<std::string> output;
        output.reserve(workshopItemChoices.size() + 2U);
        for (const auto* item : workshopItemChoices)
        {
            std::string label = localized(item->name);
            if (profileState.player.slots[slotIndex].record ==
                item->record)
                label += "  [selected]";
            else
                label += "  " + currency(item->cost);
            output.push_back(std::move(label));
        }
        const auto& installed =
            profileState.player.slots[slotIndex];
        if (const auto* item =
                originalGarage->findItem(installed.record);
            item != nullptr && item->maximumCharge > 0U)
        {
            const auto charge =
                installed.hasCharge ? installed.charge
                                    : item->defaultCharge;
            const auto amount =
                charge >= item->maximumCharge
                    ? 0U
                    : std::min(item->chargeStep,
                               item->maximumCharge - charge);
            output.push_back(
                "Ammunition " + std::to_string(charge) + "/" +
                std::to_string(item->maximumCharge) + "  " +
                currency(item->chargeCost * amount));
        }
        output.push_back(localized("svBack"));
        replacePage(workshopItemsPage, std::move(output));
    };
    auto refreshPlanetsPage = [&]() {
        std::vector<std::string> output;
        const auto count = std::min(
            originalGarage->planets.size(),
            profileState.player.planets.size());
        output.reserve(count + 1U);
        for (std::size_t index = 0; index < count; ++index)
        {
            const auto& progress =
                profileState.player.planets[index];
            std::string state =
                progress.state == 2U
                    ? localized("svLockedCarName")
                    : index == profileState.player.currentPlanet
                          ? "Selected"
                          : "Pass " +
                                std::to_string(progress.pass);
            output.push_back(
                localized(originalGarage->planets[index].name) +
                "  [" + state + "]");
        }
        output.push_back(localized("svBack"));
        replacePage(planetsPage, std::move(output));
    };
    auto refreshAchievementsPage = [&]() {
        achievementChoices.clear();
        std::vector<std::string> output;
        for (const auto& [name, item] :
             profileState.achievementItems)
        {
            achievementChoices.push_back(name);
            const auto state = item.values.find("state");
            const auto price = item.values.find("price");
            std::string status =
                state != item.values.end() &&
                        state->second == "asOpened"
                    ? "Opened"
                    : state != item.values.end() &&
                              state->second == "asLocked"
                          ? "Locked"
                          : price == item.values.end()
                                ? "Unlocked"
                                : price->second + " points";
            output.push_back(name + "  [" + status + "]");
        }
        output.push_back(
            "Points: " +
            std::to_string(profileState.achievementPoints));
        output.push_back(localized("svBack"));
        replacePage(achievementsPage, std::move(output));
    };
    auto showOriginalRaceMenu = [&]() {
        menuStack.push_back(MenuScreen::RaceMenu);
        menuSelection = 0;
    };
    auto refreshCurrentOptionsPage = [&]() {
        switch (menuStack.back())
        {
        case MenuScreen::GameOptions:
            replacePage(gameOptionsPage, gameOptionsLabels());
            break;
        case MenuScreen::GraphicsOptions:
            replacePage(
                graphicsOptionsPage, graphicsOptionsLabels());
            break;
        case MenuScreen::SoundOptions:
            replacePage(soundOptionsPage, soundOptionsLabels());
            break;
        case MenuScreen::ControlsOptions:
            replacePage(
                controlsOptionsPage, controlsOptionsLabels());
            break;
        default:
            break;
        }
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
                profileState.config.preferredCamera =
                    profileState.config.preferredCamera ==
                            r3d::game::originalrace::
                                PreferredCamera::ThirdPerson
                        ? r3d::game::originalrace::
                              PreferredCamera::Isometric
                        : r3d::game::originalrace::
                              PreferredCamera::ThirdPerson;
                raceRenderer.resetCamera();
                break;
            case 1:
                profileState.config.cameraDistance =
                    std::clamp(
                        profileState.config.cameraDistance +
                            static_cast<float>(direction) * 0.25F,
                        1.0F, 2.0F);
                break;
            case 2:
                profileState.config.enableHud =
                    !profileState.config.enableHud;
                break;
            case 3: {
                static constexpr std::array<std::string_view, 3>
                    difficulties{
                        "gdEasy", "gdNormal", "gdHard"};
                const auto found = std::find(
                    difficulties.begin(), difficulties.end(),
                    profileState.player.difficulty);
                const auto index =
                    found == difficulties.end()
                        ? 1U
                        : static_cast<std::uint32_t>(
                              found - difficulties.begin());
                profileState.player.difficulty =
                    difficulties[cycleValue(index, 3U, direction)];
                break;
            }
            case 4:
                profileState.config.springBorders =
                    !profileState.config.springBorders;
                raceSession.setSpringBorders(
                    profileState.config.springBorders);
                break;
            case 5:
                profileState.config.upgradeMaxLevel =
                    cycleValue(
                        profileState.config.upgradeMaxLevel,
                        3U, direction);
                break;
            case 6:
                profileState.config.weaponMaxLevel =
                    cycleValue(
                        std::clamp(
                            profileState.config.weaponMaxLevel,
                            1U, 4U) -
                            1U,
                        4U, direction) +
                    1U;
                break;
            case 7:
                profileState.config.maxPlayers =
                    cycleValue(
                        std::clamp(
                            profileState.config.maxPlayers,
                            2U, 6U) -
                            2U,
                        5U, direction) +
                    2U;
                break;
            case 8:
                profileState.config.maxComputers =
                    cycleValue(
                        profileState.config.maxComputers,
                        6U, direction);
                break;
            case 9:
                profileState.config.lapsCount =
                    std::clamp(
                        static_cast<int>(
                            profileState.config.lapsCount) +
                            direction,
                        1, 8);
                break;
            case 10:
                profileState.config.enableMineBug =
                    !profileState.config.enableMineBug;
                raceSession.setEnableMineBug(
                    profileState.config.enableMineBug);
                break;
            case 11:
                profileState.config.disableVideo =
                    !profileState.config.disableVideo;
                break;
            default:
                return;
            }
            break;
        case MenuScreen::GraphicsOptions:
            switch (menuSelection)
            {
            case 0:
                profileState.config.quality.filtering =
                    cycleValue(
                        profileState.config.quality.filtering,
                        4U, direction);
                break;
            case 1:
                profileState.config.quality.msaa =
                    cycleValue(
                        profileState.config.quality.msaa,
                        4U, direction);
                break;
            case 2:
                profileState.config.quality.shadow =
                    cycleValue(
                        profileState.config.quality.shadow,
                        3U, direction);
                break;
            case 3:
                profileState.config.quality.environment =
                    cycleValue(
                        profileState.config.quality.environment,
                        3U, direction);
                break;
            case 4:
                profileState.config.quality.light =
                    cycleValue(
                        profileState.config.quality.light,
                        3U, direction);
                break;
            case 5:
                profileState.config.quality.postEffect =
                    cycleValue(
                        profileState.config.quality.postEffect,
                        3U, direction);
                break;
            case 6:
                profileState.config.fullScreen =
                    !profileState.config.fullScreen;
                if (!SDL_SetWindowFullscreen(
                        window, profileState.config.fullScreen))
                {
                    std::cerr
                        << "Unable to change original window mode: "
                        << SDL_GetError() << '\n';
                    profileState.config.fullScreen =
                        !profileState.config.fullScreen;
                }
                break;
            default:
                return;
            }
            break;
        case MenuScreen::SoundOptions:
            switch (menuSelection)
            {
            case 0:
                profileState.config.language =
                    profileState.config.language == "russian"
                        ? "english"
                        : "russian";
                break;
            case 1:
                profileState.config.commentatorStyle =
                    profileState.config.commentatorStyle == "russian"
                        ? "english"
                        : "russian";
#ifdef RRR3D_AUDIO
                commentator.shutdown();
                if (!commentator.initialize(
                        profileState.config.commentatorStyle,
                        audioError))
                {
                    std::cerr
                        << "Unable to switch commentator: "
                        << audioError << '\n';
                }
#endif
                break;
            case 2:
                profileState.config.musicVolume =
                    std::clamp(
                        profileState.config.musicVolume +
                            static_cast<float>(direction) * 0.1F,
                        0.0F, 2.0F);
#ifdef RRR3D_AUDIO
                audio.setBusVolume(
                    r3d::audio::Bus::Music,
                    profileState.config.musicVolume);
#endif
                break;
            case 3:
                profileState.config.effectsVolume =
                    std::clamp(
                        profileState.config.effectsVolume +
                            static_cast<float>(direction) * 0.1F,
                        0.0F, 2.0F);
#ifdef RRR3D_AUDIO
                audio.setBusVolume(
                    r3d::audio::Bus::Effects,
                    profileState.config.effectsVolume);
#endif
                break;
            case 4:
                profileState.config.voiceVolume =
                    std::clamp(
                        profileState.config.voiceVolume +
                            static_cast<float>(direction) * 0.1F,
                        0.0F, 2.0F);
#ifdef RRR3D_AUDIO
                audio.setBusVolume(
                    r3d::audio::Bus::Voice,
                    profileState.config.voiceVolume);
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
        std::string optionSaveError;
        if (!profileStore.save(profileState, optionSaveError))
            std::cerr << "Unable to save original options: "
                      << optionSaveError << '\n';
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
        // Tournament -> Continue -> RaceMenu/Start.  Advance one real
        // press/release pair per
        // rendered menu frame instead.
        if (options->raceRenderSmokeTest && !inRace &&
            raceSmokeMenuStep < 4U &&
            renderedFrames >= raceSmokeNextMenuFrame)
        {
            SDL_Event confirm{};
            confirm.key.type = SDL_EVENT_KEY_DOWN;
            confirm.key.down = true;
            confirm.key.scancode = SDL_SCANCODE_RETURN;
            SDL_Event release = confirm;
            release.key.type = SDL_EVENT_KEY_UP;
            release.key.down = false;
            if (!SDL_PushEvent(&confirm) ||
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
                            ? profileState.config.gamepadControls
                            : profileState.config.keyboardControls;
                    bindings[*bindingCaptureAction] = *bindingName;
                    if (bindingCaptureGamepad)
                    {
                        input.applyGamepadBindings(
                            profileState.config.gamepadControls);
                    }
                    else
                    {
                        input.applyKeyboardBindings(
                            profileState.config.keyboardControls);
                    }
                    std::cout
                        << "Original ControlsFrame: "
                        << *bindingCaptureAction << " -> "
                        << *bindingName << '\n';
                    bindingCaptureAction.reset();
                    refreshCurrentOptionsPage();
                    std::string bindingSaveError;
                    if (!profileStore.save(
                            profileState, bindingSaveError))
                    {
                        std::cerr
                            << "Unable to save control binding: "
                            << bindingSaveError << '\n';
                    }
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
                if (!pointerTargetsItem &&
                    inputEvent.source == rrr3d::input::Source::Mouse &&
                    inputEvent.action ==
                        rrr3d::input::Action::MenuConfirm)
                    continue;
                if (!inputEvent.active)
                    continue;
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
                    if (menuStack.size() == 1U)
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
                        pushMenu(MenuScreen::Options);
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
                        profileState.player =
                            r3d::game::originalrace::PlayerProfile{};
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
                    if (menuSelection == 0U)
                        pushMenu(MenuScreen::GameOptions);
                    else if (menuSelection == 1U)
                        pushMenu(MenuScreen::GraphicsOptions);
                    else if (menuSelection == 2U)
                        pushMenu(MenuScreen::SoundOptions);
                    else if (menuSelection == 3U)
                    {
                        pushMenu(MenuScreen::ControlsOptions);
                        refreshCurrentOptionsPage();
                    }
                    else
                        backMenu();
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
                        refreshWorkshopSlotsPage();
                        pushMenu(MenuScreen::WorkshopSlots);
                    }
                    else if (menuSelection == 2U)
                    {
                        refreshGaragePage();
                        pushMenu(MenuScreen::Garage);
                    }
                    else if (menuSelection == 3U)
                    {
                        refreshPlanetsPage();
                        pushMenu(MenuScreen::Planets);
                    }
                    else if (menuSelection == 4U)
                    {
                        refreshAchievementsPage();
                        pushMenu(MenuScreen::Achievements);
                    }
                    else if (menuSelection == 5U)
                    {
                        pushMenu(MenuScreen::Options);
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
                case MenuScreen::WorkshopSlots:
                    if (menuSelection <
                        static_cast<std::size_t>(
                            r3d::game::originalrace::
                                GarageSlotType::Count))
                    {
                        workshopSlot =
                            static_cast<r3d::game::originalrace::
                                            GarageSlotType>(
                                menuSelection);
                        refreshWorkshopItemsPage();
                        pushMenu(MenuScreen::WorkshopItems);
                    }
                    else
                    {
                        backMenu();
                    }
                    break;
                case MenuScreen::WorkshopItems:
                {
                    const auto slotIndex =
                        static_cast<std::size_t>(workshopSlot);
                    if (menuSelection <
                        workshopItemChoices.size())
                    {
                        std::string workshopError;
                        if (!r3d::game::originalrace::
                                installOriginalWorkshopItem(
                                    *originalGarage, profileState,
                                    workshopSlot,
                                    *workshopItemChoices[
                                        menuSelection],
                                    championshipMode,
                                    workshopError))
                        {
                            std::cerr
                                << "Original WorkshopFrame: "
                                << workshopError << '\n';
                        }
                        else
                        {
                            saveRaceProfile();
                            refreshWorkshopSlotsPage();
                        }
                        refreshWorkshopItemsPage();
                        break;
                    }
                    const auto* installed =
                        originalGarage->findItem(
                            profileState.player
                                .slots[slotIndex]
                                .record);
                    const bool hasAmmunition =
                        installed != nullptr &&
                        installed->maximumCharge > 0U;
                    if (hasAmmunition &&
                        menuSelection ==
                            workshopItemChoices.size())
                    {
                        std::string workshopError;
                        if (!r3d::game::originalrace::
                                rechargeOriginalWorkshopItem(
                                    *originalGarage, profileState,
                                    workshopSlot,
                                    championshipMode,
                                    workshopError))
                        {
                            std::cerr
                                << "Original WorkshopFrame: "
                                << workshopError << '\n';
                        }
                        else
                        {
                            saveRaceProfile();
                            refreshWorkshopSlotsPage();
                        }
                        refreshWorkshopItemsPage();
                    }
                    else
                    {
                        backMenu();
                    }
                    break;
                }
                case MenuScreen::Planets:
                    if (menuSelection <
                        std::min(
                            originalGarage->planets.size(),
                            profileState.player.planets.size()))
                    {
                        const auto& progress =
                            profileState.player
                                .planets[menuSelection];
                        if (progress.state != 2U)
                        {
                            profileState.player.currentPlanet =
                                static_cast<std::uint32_t>(
                                    menuSelection);
                            profileState.player.currentTrack = 0U;
                            selectedTrack =
                                r3d::game::originalrace::
                                    resolveOriginalTournamentTrack(
                                        *originalRace,
                                        profileState.player);
                            saveRaceProfile();
                            refreshPlanetsPage();
                        }
                    }
                    else
                    {
                        backMenu();
                    }
                    break;
                case MenuScreen::Achievements:
                    if (menuSelection <
                        achievementChoices.size())
                    {
                        auto& achievement =
                            profileState.achievementItems[
                                achievementChoices[menuSelection]];
                        auto& state =
                            achievement.values["state"];
                        std::uint32_t price = 0U;
                        if (const auto found =
                                achievement.values.find("price");
                            found != achievement.values.end())
                        {
                            const auto parsed = std::from_chars(
                                found->second.data(),
                                found->second.data() +
                                    found->second.size(),
                                price);
                            if (parsed.ec != std::errc{})
                                price = 0U;
                        }
                        if (state == "asUnlocked" &&
                            profileState.achievementPoints >= price)
                        {
                            profileState.achievementPoints -= price;
                            state = "asOpened";
                            saveRaceProfile();
                        }
                        refreshAchievementsPage();
                    }
                    else if (menuSelection + 1U >=
                             page.labels.size())
                    {
                        backMenu();
                    }
                    break;
                case MenuScreen::GameOptions:
                case MenuScreen::GraphicsOptions:
                case MenuScreen::SoundOptions:
                    if (menuSelection + 1U >= page.labels.size())
                        backMenu();
                    else
                        adjustCurrentOption(1);
                    break;
                case MenuScreen::ControlsOptions:
                {
                    const std::size_t first =
                        controlsPageIndex * controlsPerPage;
                    const std::size_t actionCount = std::min(
                        controlsPerPage,
                        originalControlActions.size() - first);
                    const std::size_t previousRow = 1U + actionCount;
                    const std::size_t nextRow = previousRow + 1U;
                    const std::size_t backRow = nextRow + 1U;
                    if (menuSelection == 0U)
                    {
                        controlsUseGamepad = !controlsUseGamepad;
                        refreshCurrentOptionsPage();
                    }
                    else if (menuSelection <= actionCount)
                    {
                        bindingCaptureAction =
                            originalControlActions[
                                first + menuSelection - 1U];
                        bindingCaptureGamepad = controlsUseGamepad;
                        std::cout
                            << "Original ControlsFrame: "
                            << localized("svPressKey") << " ("
                            << *bindingCaptureAction << ")\n";
                    }
                    else if (menuSelection == previousRow)
                    {
                        controlsPageIndex =
                            (controlsPageIndex + controlsPageCount - 1U) %
                            controlsPageCount;
                        menuSelection = 0U;
                        refreshCurrentOptionsPage();
                    }
                    else if (menuSelection == nextRow)
                    {
                        controlsPageIndex =
                            (controlsPageIndex + 1U) % controlsPageCount;
                        menuSelection = 0U;
                        refreshCurrentOptionsPage();
                    }
                    else if (menuSelection == backRow)
                    {
                        bindingCaptureAction.reset();
                        backMenu();
                    }
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
                             event.target < originalRace->bonuses.size())
                    {
                        const auto kind =
                            originalRace->bonuses[event.target].kind;
                        playSpatial(
                            kind ==
                                    r3d::game::originalrace::BonusKind::
                                        Shield
                                ? shieldAudio
                                : pickupAudio,
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
        device->beginFrame(camera, 0x040818ffU);
        drawQuad(*device, quad, shader, background, menu::virtualWidth,
                 menu::virtualHeight, menu::virtualWidth * 0.5F,
                 menu::virtualHeight * 0.5F, 90.0F, opaque);
        drawQuad(*device, quad, shader, topPanel,
                 static_cast<float>(model->topPanelImage.width),
                 static_cast<float>(model->topPanelImage.height),
                 menu::virtualWidth * 0.5F, 200.0F, 70.0F, transparent);

        auto& activePage = activeMenuPage();
        for (std::size_t index = 0;
             index < activePage.normal.size(); ++index)
        {
            const float centerX = menu::virtualWidth * 0.5F +
                                  menu::itemCenterOffsetX;
            const float centerY = menu::virtualHeight * 0.5F +
                                  menu::firstItemOffsetY +
                                  static_cast<float>(index) *
                                      menu::itemSpacing;
            if (index == menuSelection)
            {
                drawQuad(*device, quad, shader, selection,
                         static_cast<float>(model->selectionImage.width),
                         static_cast<float>(model->selectionImage.height),
                         centerX, centerY, 50.0F, transparent);
            }
            const auto& text = index == menuSelection
                                   ? activePage.selected[index]
                                   : activePage.normal[index];
            drawQuad(*device, quad, shader, text.texture, text.width,
                     text.height, centerX, centerY, 25.0F, transparent);
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

        const float versionX = menu::virtualWidth - 25.0F -
                               version.width * 0.5F;
        const float versionY = menu::virtualHeight - 25.0F -
                               version.height * 0.5F;
        drawQuad(*device, quad, shader, version.texture, version.width,
                 version.height, versionX, versionY, 25.0F, transparent);
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
                        << inRace << ", pause="
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
                           "source HudMenu pause/accept/frozen-world and "
                           "render-target resize round-trip passed\n";
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
