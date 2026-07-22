#include "CoreTextRasterizer.h"
#include "OriginalAudioSpec.h"
#include "OriginalMainMenu.h"
#include "renderer/BgfxGraphicsDevice.h"
#include "resource/ResourceFileSystem.h"
#include "xplatform.h"

#ifdef RRR3D_GAMEPAD_INPUT
#include "SdlInputManager.h"
#include "SdlInputSmoke.h"
#endif
#ifdef RRR3D_AUDIO
#include "SdlAudioBackend.h"
#include "SdlAudioSmoke.h"
#include "audio/AudioBackend.h"
#endif

#include <SDL3/SDL.h>
#include <bx/math.h>

#include "rrr3d_fs_static_scene.bin.h"
#include "rrr3d_vs_static_scene.bin.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
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

struct Options
{
    std::uint32_t smokeFrames = 0;
    std::filesystem::path dataDirectory;
    std::string language;
    bool verifyResources = false;
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
    try
    {
        resources.emplace(dataDirectory);
        model.emplace(menu::loadOriginalMainMenu(*resources,
                                                 options->language));
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
        return EXIT_SUCCESS;
    }

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
#ifdef RRR3D_AUDIO
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
        valid(shader) && valid(quad) && valid(background) && valid(topPanel) &&
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

    r3d::audio::SoundInfo musicInfo;
    r3d::audio::SoundInfo clickInfo;
    const auto musicSound =
        loadAudio(originalaudio::menuTracks[0].path, musicInfo);
    const auto clickSound =
        musicSound == r3d::audio::invalidSound
            ? r3d::audio::invalidSound
            : loadAudio(originalaudio::mainButtonClick, clickInfo);
    if (musicSound == r3d::audio::invalidSound ||
        clickSound == r3d::audio::invalidSound)
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
    r3d::audio::PlayOptions musicOptions;
    musicOptions.bus = r3d::audio::Bus::Music;
    musicOptions.loop = true;
    const auto musicVoice = audio.play(musicSound, musicOptions, audioError);
    if (musicVoice == r3d::audio::invalidVoice)
    {
        std::cerr << "Original menu music start failed: " << audioError
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

    std::cout << "Original menu music: Data/"
              << originalaudio::menuTracks[0].path << " ("
              << originalaudio::menuTracks[0].band << " - "
              << originalaudio::menuTracks[0].name << "), "
              << musicInfo.durationSeconds << " s; MainMenu2 ssButton1: Data/"
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
#endif

    PipelineState opaque;
    opaque.faceCulling = PipelineState::FaceCulling::None;
    opaque.writeDepth = false;
    opaque.depthTest = false;
    PipelineState transparent = opaque;
    transparent.alphaBlend = true;
    const Camera camera = makeCamera(*device);

    bool running = true;
    bool runtimeSmokeFailed = false;
    std::uint32_t renderedFrames = 0;
    menu::Controller controller(model->items.size());
#ifdef RRR3D_AUDIO
    bool integratedAudioInputObserved = !options->audioSmokeTest;
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
            if (event.type == SDL_EVENT_MOUSE_MOTION)
            {
                const auto hovered = hoveredItem(
                    window, event.motion.x, event.motion.y,
                    model->items.size(),
                    static_cast<float>(model->selectionImage.width),
                    static_cast<float>(model->selectionImage.height));
                if (hovered)
                    controller.select(*hovered);
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
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

        ++renderedFrames;
        if (options->smokeFrames != 0 &&
            renderedFrames >= options->smokeFrames)
        {
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
                             "bgfx/Metal smoke test completed after "
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
    audio.shutdown();
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
