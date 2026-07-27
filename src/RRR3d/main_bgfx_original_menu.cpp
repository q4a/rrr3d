#include "CoreTextRasterizer.h"
#include "OriginalAudioSpec.h"
#include "OriginalMainMenu.h"
#ifdef RRR3D_PHYSICS
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
#include <iostream>
#include <map>
#include <optional>
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
    std::optional<r3d::physics::WorldDescription> physicsDescription;
    r3d::game::originalrace::OriginalProfileStore profileStore(
        rrr3d::platform::save_directory(), dataDirectory);
    std::string profileWarning;
    auto profileState = profileStore.load(profileWarning);
    if (!profileWarning.empty())
        std::cerr << "Profile import warning: " << profileWarning << '\n';
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
        std::cout << "Milestone 9 selected race audit passed: track "
                  << selectedTrack << ' ' << originalRace->levelPath
                  << ", " << originalRace->trackInstances.size()
                  << " track objects, "
                  << originalRace->decorationInstances.size()
                  << " decorations, " << originalRace->bonuses.size()
                  << " bonuses, " << originalRace->racers.size()
                  << " racers, car "
                  << recordName(originalRace->vehicle.record) << '\n';
#endif
        return EXIT_SUCCESS;
    }

#ifdef RRR3D_PHYSICS
    if (options->physicsSmokeTest)
    {
        std::string physicsError;
        if (!r3d::game::originalrace::runOriginalRaceResourceSmokeTest(
                *originalRace, *resources, physicsError) ||
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
                     " acceleration, braking, steering, suspension contacts,"
                     " trace reset, countdown, checkpoint/lap/finish,"
                     " weapon/damage, bonus, and respawn state passed\n";
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

    std::vector<TextVisual> normalItems;
    std::vector<TextVisual> selectedItems;
    std::string resolvedFont;
    try
    {
        for (const auto& item : model->items)
        {
            normalItems.push_back(createText(
                *device, item, menu::headerFontHeight, false,
                menu::normalTextColor, resolvedFont));
            selectedItems.push_back(createText(
                *device, item, menu::headerFontHeight, false,
                menu::selectedTextColor, resolvedFont));
        }
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Original MainMenu2 font creation failed: "
                  << exception.what() << '\n';
    }
    const TextVisual version = createText(
        *device, model->versionText, menu::smallFontHeight, true,
        menu::selectedTextColor, resolvedFont);

    const bool gpuResourcesValid =
        valid(shader) &&
#ifdef RRR3D_PHYSICS
        valid(raceShader) &&
#endif
        valid(quad) && valid(background) && valid(topPanel) &&
        valid(bottomPanel) && valid(selection) && valid(cursor) &&
        valid(version.texture) && normalItems.size() == model->items.size() &&
        selectedItems.size() == model->items.size() &&
        std::all_of(normalItems.begin(), normalItems.end(),
                    [](const TextVisual& item) { return valid(item.texture); }) &&
        std::all_of(selectedItems.begin(), selectedItems.end(),
                    [](const TextVisual& item) { return valid(item.texture); });

    auto releaseResources = [&]() {
        device->destroy(version.texture);
        for (const auto& item : selectedItems)
            device->destroy(item.texture);
        for (const auto& item : normalItems)
            device->destroy(item.texture);
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

    if (!gpuResourcesValid)
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
        const auto& vehicle = originalRace->vehicles.at(
            originalRace->racers[racer].vehicle);
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
        weaponAudio[weapon] =
            loadEngineSound(originalRace->weapons[weapon].soundPath);
        engineAudioValid =
            engineAudioValid &&
            weaponAudio[weapon] != r3d::audio::invalidSound;
    }
    const auto pickupAudio =
        loadEngineSound("Data/Sounds/UI/pickup_up.ogg");
    const auto shieldAudio =
        loadEngineSound("Data/Sounds/shieldOn.ogg");
    const auto crashAudio =
        loadEngineSound("Data/Sounds/carcrash05.ogg");
    const auto destructionAudio =
        loadEngineSound("Data/Sounds/spherePulseDeath.ogg");
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
    menu::Controller controller(model->items.size());
#ifdef RRR3D_PHYSICS
    bool inRace = false;
    r3d::physics::VehicleInput raceInput;
    bool raceUseWeapon = false;
    bool raceUseMine = false;
    bool raceUseHyper = false;
    bool raceChangeWeaponRequested = false;
    int raceWeaponSlotRequested = -1;
    bool raceResetRequested = false;
    std::vector<r3d::physics::VehicleState> raceVehicles(
        physicsWorld->vehicleCount());
    for (std::size_t index = 0; index < physicsWorld->vehicleCount();
         ++index)
        raceVehicles[index] = physicsWorld->vehicle(index);
    float raceElapsedSeconds = 0.0F;
    std::uint64_t previousFrameTicks = SDL_GetTicksNS();
    bool integratedRaceStartObserved = !options->raceRenderSmokeTest;
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
    auto saveRaceProfile = [&]() {
        if (!raceSession.racers().empty())
        {
            raceSession.writePlayerProfile(profileState.player);
            raceSession.writeAchievementProfile(profileState);
            if (raceSession.racers().front().finished &&
                !raceProgressSaved)
            {
                const auto advance =
                    r3d::game::originalrace::
                        completeOriginalTournamentTrack(
                            *originalRace, selectedTrack, profileState);
                raceProgressSaved = true;
                std::cout
                    << "Original Tournament::CompleteTrack: track "
                    << selectedTrack << " -> " << advance.trackIndex
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
#if defined(RRR3D_PHYSICS) && defined(RRR3D_GAMEPAD_INPUT)
    if (options->raceRenderSmokeTest)
    {
        SDL_Event confirm{};
        confirm.key.type = SDL_EVENT_KEY_DOWN;
        confirm.key.down = true;
        confirm.key.scancode = SDL_SCANCODE_RETURN;
        if (!SDL_PushEvent(&confirm))
        {
            std::cerr << "Unable to queue integrated M9 Single Player event: "
                      << SDL_GetError() << '\n';
            runtimeSmokeFailed = true;
        }
    }
#endif
    while (running)
    {
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
            bool pointerTargetsItem = true;
            if (
#ifdef RRR3D_PHYSICS
                !inRace &&
#endif
                event.type == SDL_EVENT_MOUSE_MOTION)
            {
                const auto hovered = hoveredItem(
                    window, event.motion.x, event.motion.y,
                    model->items.size(),
                    static_cast<float>(model->selectionImage.width),
                    static_cast<float>(model->selectionImage.height));
                if (hovered)
                    controller.select(*hovered);
            }
            else if (
#ifdef RRR3D_PHYSICS
                !inRace &&
#endif
                event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            {
                const auto hovered = hoveredItem(
                    window, event.button.x, event.button.y,
                    model->items.size(),
                    static_cast<float>(model->selectionImage.width),
                    static_cast<float>(model->selectionImage.height));
                pointerTargetsItem = hovered.has_value() ||
                                     event.button.button != SDL_BUTTON_LEFT;
                if (hovered)
                    controller.select(*hovered);
            }
            const auto inputEvents = input.processEvent(event);
            for (const auto& inputEvent : inputEvents)
            {
#ifdef RRR3D_PHYSICS
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
                        raceInput.brake = inputEvent.active
                                              ? inputEvent.value
                                              : 0.0F;
                        break;
                    case rrr3d::input::Action::TurnLeft:
                        if (inputEvent.active)
                            raceInput.steering = -inputEvent.value;
                        else if (raceInput.steering < 0.0F)
                            raceInput.steering = 0.0F;
                        break;
                    case rrr3d::input::Action::TurnRight:
                        if (inputEvent.active)
                            raceInput.steering = inputEvent.value;
                        else if (raceInput.steering > 0.0F)
                            raceInput.steering = 0.0F;
                        break;
                    case rrr3d::input::Action::UseWeapon:
                        raceUseWeapon = inputEvent.active;
                        break;
                    case rrr3d::input::Action::UseMine:
                        raceUseMine = inputEvent.active;
                        break;
                    case rrr3d::input::Action::UseHyper:
                        raceUseHyper = inputEvent.active;
                        break;
                    case rrr3d::input::Action::ChangeWeapon:
                        if (inputEvent.active && !inputEvent.repeated)
                            raceChangeWeaponRequested = true;
                        break;
                    case rrr3d::input::Action::SelectWeapon1:
                    case rrr3d::input::Action::SelectWeapon2:
                    case rrr3d::input::Action::SelectWeapon3:
                    case rrr3d::input::Action::SelectWeapon4:
                        if (inputEvent.active && !inputEvent.repeated)
                        {
                            raceWeaponSlotRequested =
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
                        {
                            const bool paused =
                                raceSession.phase() !=
                                r3d::game::originalrace::RacePhase::Paused;
                            raceSession.setPaused(paused);
#ifdef RRR3D_AUDIO
                            gameMusic.pause(paused, audioError);
                            commentator.pause(paused);
#endif
                        }
                        break;
                    case rrr3d::input::Action::MenuBack:
                        if (inputEvent.active && !inputEvent.repeated)
                        {
                            saveRaceProfile();
                            inRace = false;
                            raceInput = {};
                            raceUseWeapon = false;
                            raceUseMine = false;
                            raceUseHyper = false;
                            raceChangeWeaponRequested = false;
                            raceWeaponSlotRequested = -1;
                            raceResetRequested = false;
#ifdef RRR3D_AUDIO
                            stopRaceAudio();
#endif
                            std::cout << "Race -> MainMenu2\n";
                        }
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
                const auto command = controller.handle(inputEvent);
                if (!command)
                    continue;
#ifdef RRR3D_AUDIO
                // MainMenu2 creates these buttons with ssButton1. The legacy
                // scheme plays click.ogg on press and has no navigation or
                // hover sound, so only a successful confirm reaches here.
                const bool clickStarted = playMainButtonClick();
#if defined(RRR3D_GAMEPAD_INPUT)
                if (options->audioSmokeTest && clickStarted &&
                    *command == menu::Command::Network &&
                    controller.selectedItem() == 1)
                {
                    integratedAudioInputObserved = true;
                }
#endif
#endif
                std::cout << "MainMenu2 command: "
                          << menu::commandName(*command) << " via "
                          << rrr3d::input::sourceName(inputEvent.source)
                          << '\n';
                if (*command == menu::Command::Exit ||
                    *command == menu::Command::Back)
                {
                    running = false;
                }
#ifdef RRR3D_PHYSICS
                else if (*command == menu::Command::SinglePlayer)
                {
                    physicsWorld->reset();
                    raceSession.reset();
                    raceRenderer.resetCamera();
                    raceInput = {};
                    raceUseWeapon = false;
                    raceUseMine = false;
                    raceUseHyper = false;
                    raceChangeWeaponRequested = false;
                    raceWeaponSlotRequested = -1;
                    raceResetRequested = false;
                    raceElapsedSeconds = 0.0F;
                    raceProgressSaved = false;
                    for (std::size_t index = 0;
                         index < physicsWorld->vehicleCount(); ++index)
                        raceVehicles[index] =
                            physicsWorld->vehicle(index);
                    inRace = true;
                    if (options->raceRenderSmokeTest)
                        integratedRaceStartObserved = true;
                    previousFrameTicks = SDL_GetTicksNS();
#ifdef RRR3D_AUDIO
                    startRaceAudio();
#endif
                    std::cout << "MainMenu2 -> original race: "
                              << originalRace->levelPath << '\n';
                }
#endif
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
        if (inRace && options->raceRenderSmokeTest)
        {
            raceInput.throttle = 1.0F;
            raceInput.brake = 0.0F;
            raceInput.steering = renderedFrames >= 90 &&
                                         renderedFrames < 180
                                     ? 0.35F
                                     : 0.0F;
        }
        if (inRace)
        {
            r3d::game::originalrace::RaceControl control;
            control.driving = raceInput;
            control.useWeapon = raceUseWeapon;
            control.useMine = raceUseMine;
            control.useHyper = raceUseHyper;
            control.changeWeapon = raceChangeWeaponRequested;
            control.weaponSlot = raceWeaponSlotRequested;
            control.reset = raceResetRequested;
            raceSession.update(frameSeconds, raceVehicles, control);
            raceChangeWeaponRequested = false;
            raceWeaponSlotRequested = -1;
            raceResetRequested = false;
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
                                 r3d::game::originalrace::RaceEventKind::
                                     Kill ||
                             event.kind ==
                                 r3d::game::originalrace::RaceEventKind::
                                     Respawn)
                    {
                        playSpatial(
                            destructionAudio, event.position,
                            eventVelocity(
                                event.kind ==
                                        r3d::game::originalrace::
                                            RaceEventKind::Kill
                                    ? event.target
                                    : event.racer),
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
            for (const auto& respawn : raceSession.takeRespawns())
                physicsWorld->resetVehicle(
                    respawn.racer, respawn.position, respawn.direction);
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
                if (raceSession.racers()[racer].slowSeconds > 0.0F)
                    physicsWorld->clampLinearSpeed(racer, 20.0F);
            }
            physicsWorld->step(frameSeconds,
                               raceSession.vehicleInputs());
            raceElapsedSeconds = raceSession.elapsedSeconds();
            for (std::size_t index = 0;
                 index < physicsWorld->vehicleCount(); ++index)
                raceVehicles[index] = physicsWorld->vehicle(index);
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
                profileState.config.cameraDistance, frameSeconds);
            raceRenderer.renderFrame(
                *device, raceShader, raceCamera, 0x6b91b8ffU,
                *originalRace, raceVehicles, racePipeline,
                raceSession.decorationActive(),
                raceSession.bonusActive(), raceSession.racers(),
                raceSession.effects(), raceSession.mines(),
                raceSession.projectiles(), raceElapsedSeconds,
                profileState.config.quality);
            raceHud.update(*device, *originalRace, raceSession,
                           raceVehicles, raceCamera, frameSeconds);
            device->beginOverlay(camera);
            if (profileState.config.enableHud)
                raceHud.draw(*device, quad, shader, raceShader);
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

        for (std::size_t index = 0; index < normalItems.size(); ++index)
        {
            const float centerX = menu::virtualWidth * 0.5F +
                                  menu::itemCenterOffsetX;
            const float centerY = menu::virtualHeight * 0.5F +
                                  menu::firstItemOffsetY +
                                  static_cast<float>(index) *
                                      menu::itemSpacing;
            if (index == controller.selectedItem())
            {
                drawQuad(*device, quad, shader, selection,
                         static_cast<float>(model->selectionImage.width),
                         static_cast<float>(model->selectionImage.height),
                         centerX, centerY, 50.0F, transparent);
            }
            const auto& text = index == controller.selectedItem()
                                   ? selectedItems[index]
                                   : normalItems[index];
            drawQuad(*device, quad, shader, text.texture, text.width,
                     text.height, centerX, centerY, 25.0F, transparent);
        }

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
                    maximumRaceSmokeContacts == 0 ||
                    maximumRaceSmokeSpeed < 0.2F ||
                    raceVehicles.size() < 2U ||
                    !raceCameraStylesObserved[0] ||
                    !raceCameraStylesObserved[1] ||
                    !renderGraphComplete ||
                    maximumEnvironmentMappedDraws == 0U ||
                    (expectsBumpMapping &&
                     maximumNormalMappedDraws == 0U) ||
                    maximumTransientDraws == 0U)
                {
                    std::cerr
                        << "Milestone 9 integrated Single Player/race render "
                           "verification failed: started="
                        << integratedRaceStartObserved << ", inRace="
                        << inRace << ", contacts="
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
                        << "Milestone 9.4 original Single Player/"
                        << originalRace->levelPath << '/'
                        << recordName(originalRace->vehicle.record)
                        << "/Jolt/bgfx/Metal smoke test completed after "
                        << renderedFrames << " frames; max speed "
                        << maximumRaceSmokeSpeed << ", wheel contacts "
                        << maximumRaceSmokeContacts
                        << ", renderer passes cube6/shadow/scene/HDR64-1/"
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
