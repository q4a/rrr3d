#include "CoreTextRasterizer.h"
#include "OriginalAudioSpec.h"
#include "OriginalMainMenu.h"
#ifdef RRR3D_PHYSICS
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
    bool verifyResources = false;
#ifdef RRR3D_PHYSICS
    bool physicsSmokeTest = false;
    bool raceRenderSmokeTest = false;
    std::uint32_t trackIndex = 0;
    std::string car;
    std::string weather = "fair";
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
            continue;
        }
        if (argument.substr(0, carPrefix.size()) == carPrefix)
        {
            options.car = argument.substr(carPrefix.size());
            if (options.car.empty())
                return std::nullopt;
            continue;
        }
        if (argument.substr(0, weatherPrefix.size()) == weatherPrefix)
        {
            options.weather = argument.substr(weatherPrefix.size());
            if (options.weather != "fair" &&
                options.weather != "night" &&
                options.weather != "cloudy" &&
                options.weather != "rainy")
                return std::nullopt;
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
             184.0F / 255.0F, 1.0F};
        environment.ambientColor = {0.2F, 0.2F, 0.2F, 1.0F};
        environment.fogIntensity = 1.0F;
        environment.rain = weather == "rainy";
    }
    else
    {
        environment.weather = Weather::Fair;
        environment.fogColor =
            {148.0F / 255.0F, 193.0F / 255.0F,
             235.0F / 255.0F, 1.0F};
        environment.ambientColor = {0.18F, 0.18F, 0.18F, 1.0F};
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
                     "[--weather=fair|night|cloudy|rainy] "
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
#ifdef RRR3D_PHYSICS
    std::optional<r3d::game::originalrace::Race> originalRace;
    std::optional<r3d::physics::WorldDescription> physicsDescription;
#endif
    try
    {
        resources.emplace(dataDirectory);
        model.emplace(menu::loadOriginalMainMenu(*resources,
                                                 options->language));
#ifdef RRR3D_PHYSICS
        originalRace.emplace(r3d::game::originalrace::loadOriginalRace(
            *resources, options->trackIndex, options->car));
        applyWeather(originalRace->environment, options->weather);
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
                  << options->trackIndex << ' ' << originalRace->levelPath
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
    rrr3d::race::OriginalRaceRenderer raceRenderer;
    rrr3d::race::OriginalRaceHud raceHud;
    if (!physicsWorld ||
        !raceRenderer.initialize(*device, *resources, *originalRace,
                                 physicsError) ||
        !raceHud.initialize(*device, *resources, physicsError))
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
    audio.setBusVolume(r3d::audio::Bus::Music,
                       originalaudio::defaultMusicVolume);
    audio.setBusVolume(r3d::audio::Bus::Effects,
                       originalaudio::defaultEffectsVolume);
    audio.setBusVolume(r3d::audio::Bus::Voice,
                       originalaudio::defaultVoiceVolume);

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
    if (!engineAudioValid)
    {
        std::cerr << "Original race engine audio loading failed: "
                  << audioError << '\n';
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
        input.shutdown();
        SDL_Quit();
        return EXIT_FAILURE;
    }
    auto startRaceAudio = [&]() {
        music.pause(true, audioError);
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
    auto stopRaceAudio = [&]() {
        for (auto& engine : engineAudio)
        {
            audio.stop(engine.idleVoice);
            audio.stop(engine.rpmVoice);
            engine.idleVoice = r3d::audio::invalidVoice;
            engine.rpmVoice = r3d::audio::invalidVoice;
        }
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
    racePipeline.faceCulling = PipelineState::FaceCulling::None;
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
                        }
                        break;
                    case rrr3d::input::Action::MenuBack:
                        if (inputEvent.active && !inputEvent.repeated)
                        {
                            inRace = false;
                            raceInput = {};
                            raceUseWeapon = false;
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
                    raceInput = {};
                    raceUseWeapon = false;
                    raceResetRequested = false;
                    raceElapsedSeconds = 0.0F;
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
            control.reset = raceResetRequested;
            raceSession.update(frameSeconds, raceVehicles, control);
            raceResetRequested = false;
            for (const auto& respawn : raceSession.takeRespawns())
                physicsWorld->resetVehicle(
                    respawn.racer, respawn.position, respawn.direction);
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
            const auto raceCamera = raceRenderer.makeCamera(
                *device, physicsWorld->vehicle(),
                static_cast<std::uint32_t>(pixelWidth),
                static_cast<std::uint32_t>(pixelHeight));
            device->beginFrame(raceCamera, 0x6b91b8ffU);
            raceRenderer.draw(*device, raceShader, *originalRace,
                              raceVehicles, racePipeline,
                              raceSession.decorationActive(),
                              raceSession.bonusActive(),
                              raceElapsedSeconds);
            raceHud.update(*device, *originalRace, raceSession,
                           physicsWorld->vehicle());
            device->beginOverlay(camera);
            raceHud.draw(*device, quad, shader);
            device->endFrame();
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
                if (!integratedRaceStartObserved || !inRace ||
                    maximumRaceSmokeContacts == 0 ||
                    maximumRaceSmokeSpeed < 0.2F)
                {
                    std::cerr
                        << "Milestone 9 integrated Single Player/race render "
                           "verification failed: started="
                        << integratedRaceStartObserved << ", inRace="
                        << inRace << ", contacts="
                        << maximumRaceSmokeContacts << ", maxSpeed="
                        << maximumRaceSmokeSpeed << '\n';
                    runtimeSmokeFailed = true;
                }
                else
                {
                    std::cout
                        << "Milestone 9 original Single Player/"
                        << originalRace->levelPath << '/'
                        << recordName(originalRace->vehicle.record)
                        << "/Jolt/bgfx/Metal smoke test completed after "
                        << renderedFrames << " frames; max speed "
                        << maximumRaceSmokeSpeed << ", wheel contacts "
                        << maximumRaceSmokeContacts << '\n';
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
    stopRaceAudio();
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
