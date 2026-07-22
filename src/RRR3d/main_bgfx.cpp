#include "renderer/BgfxGraphicsDevice.h"
#include "resource/R3DMeshAsset.h"
#include "resource/ResourceFileSystem.h"

#include <SDL3/SDL.h>
#include <bx/math.h>

#include "rrr3d_fs_static_scene.bin.h"
#include "rrr3d_vs_static_scene.bin.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using namespace r3d::renderer;

constexpr int initial_width = 1280;
constexpr int initial_height = 720;
constexpr std::string_view smoke_prefix = "--smoke-test-frames=";
constexpr std::string_view data_prefix = "--data-dir=";
constexpr std::string_view body_mesh_path = "Data/Car/buggi.r3d";
constexpr std::string_view wheel_mesh_path = "Data/Car/buggiWheel.r3d";
constexpr std::string_view wheel_positions_path = "Data/Car/buggiWheel.txt";
constexpr std::string_view car_texture_path = "Data/Car/buggi.dds";

struct Options
{
    std::uint32_t smokeFrames = 0;
    std::filesystem::path dataDirectory;
    bool verifyAssets = false;
};

struct SceneAssets
{
    r3d::resource::R3DMeshAsset body;
    r3d::resource::R3DMeshAsset wheel;
    std::array<std::array<float, 3>, 4> wheelPositions{};
    std::vector<std::uint8_t> texture;
};

std::optional<Options> parseOptions(int argc, char** argv)
{
    Options options;
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);
        if (argument == "--verify-assets")
        {
            options.verifyAssets = true;
            continue;
        }
        if (argument.substr(0, data_prefix.size()) == data_prefix)
        {
            const std::string_view value = argument.substr(data_prefix.size());
            if (value.empty())
            {
                std::cerr << "--data-dir requires a path\n";
                return std::nullopt;
            }
            options.dataDirectory = std::filesystem::path(value);
            continue;
        }
        if (argument.substr(0, smoke_prefix.size()) == smoke_prefix)
        {
            const std::string_view value = argument.substr(smoke_prefix.size());
            const auto result = std::from_chars(
                value.data(), value.data() + value.size(), options.smokeFrames);
            if (result.ec != std::errc{} ||
                result.ptr != value.data() + value.size() ||
                options.smokeFrames == 0)
            {
                std::cerr << "Invalid smoke-test frame count: " << value << '\n';
                return std::nullopt;
            }
            continue;
        }

        std::cerr << "Unknown argument: " << argument << '\n';
        return std::nullopt;
    }
    return options;
}

SceneAssets loadSceneAssets(const r3d::resource::ResourceFileSystem& resources)
{
    SceneAssets assets;
    assets.body = r3d::resource::loadR3DMeshAsset(resources, body_mesh_path);
    assets.wheel = r3d::resource::loadR3DMeshAsset(resources, wheel_mesh_path);
    assets.texture = resources.readBinary(car_texture_path);
    if (assets.texture.size() < 4 || assets.texture[0] != 'D' ||
        assets.texture[1] != 'D' || assets.texture[2] != 'S' ||
        assets.texture[3] != ' ')
    {
        throw r3d::resource::ResourceError(
            std::string(car_texture_path) + ": invalid DDS header");
    }

    std::istringstream coordinates(resources.readText(wheel_positions_path));
    for (auto& position : assets.wheelPositions)
    {
        if (!(coordinates >> position[0] >> position[1] >> position[2]))
        {
            throw r3d::resource::ResourceError(
                std::string(wheel_positions_path) +
                ": expected four three-component wheel positions");
        }
    }
    float unexpected = 0.0F;
    if (coordinates >> unexpected)
    {
        throw r3d::resource::ResourceError(
            std::string(wheel_positions_path) +
            ": contains more than four wheel positions");
    }
    return assets;
}

std::vector<StaticMeshVertex> rendererVertices(
    const r3d::resource::R3DMeshAsset& mesh)
{
    std::vector<StaticMeshVertex> vertices;
    vertices.reserve(mesh.vertices.size());
    for (const r3d::resource::R3DVertex& vertex : mesh.vertices)
    {
        vertices.push_back({
            vertex.position[0], vertex.position[1], vertex.position[2],
            vertex.normal[0], vertex.normal[1], vertex.normal[2],
            vertex.texcoord[0], vertex.texcoord[1]});
    }
    return vertices;
}

bool valid(Shader shader)
{
    return shader.value != invalid_resource;
}

bool valid(Texture texture)
{
    return texture.value != invalid_resource;
}

bool valid(Mesh mesh)
{
    return mesh.vertices.value != invalid_resource &&
           mesh.indices.value != invalid_resource;
}

Camera makeCamera(const GraphicsDevice& device, std::uint32_t width,
                  std::uint32_t height)
{
    Camera camera;
    const bx::Vec3 eye = {4.4F, -5.4F, 2.8F};
    const bx::Vec3 at = {0.0F, 0.0F, 0.1F};
    const bx::Vec3 up = {0.0F, 0.0F, 1.0F};
    bx::mtxLookAt(camera.view.data(), eye, at, up,
                  bx::Handedness::Right);
    bx::mtxProj(camera.projection.data(), 46.0F,
                static_cast<float>(std::max(width, 1U)) /
                    static_cast<float>(std::max(height, 1U)),
                0.1F, 100.0F, device.usesHomogeneousDepth(),
                bx::Handedness::Right);
    return camera;
}

Transform makeTransform(float scaleX, float scaleY, float scaleZ,
                        float x, float y, float z)
{
    Transform transform;
    bx::mtxSRT(transform.matrix.data(), scaleX, scaleY, scaleZ,
               0.0F, 0.0F, 0.0F, x, y, z);
    return transform;
}

void drawMaterialGroups(GraphicsDevice& device, Mesh gpuMesh, Shader shader,
                        Texture texture, const Transform& transform,
                        const PipelineState& pipeline,
                        const r3d::resource::R3DMeshAsset& source)
{
    if (source.materialGroups.empty())
    {
        device.draw(gpuMesh, shader, texture, transform, pipeline);
        return;
    }
    for (const r3d::resource::R3DMaterialGroup& group :
         source.materialGroups)
    {
        device.draw(gpuMesh, shader, texture, transform, pipeline,
                    {group.firstIndex, group.indexCount});
    }
}

void printAssetSummary(const SceneAssets& assets,
                       const r3d::resource::ResourceFileSystem& resources)
{
    std::cout << "Original Motor Rock renderer slice:\n"
              << "  root: " << resources.root() << '\n'
              << "  " << body_mesh_path << ": "
              << assets.body.vertices.size() << " vertices, "
              << assets.body.indices.size() / 3U << " faces, "
              << assets.body.materialGroups.size() << " material groups\n"
              << "  " << wheel_mesh_path << ": "
              << assets.wheel.vertices.size() << " vertices, "
              << assets.wheel.indices.size() / 3U << " faces, "
              << assets.wheel.materialGroups.size() << " material groups\n"
              << "  " << wheel_positions_path << ": "
              << assets.wheelPositions.size() << " original wheel positions\n"
              << "  " << car_texture_path << ": "
              << assets.texture.size() << " encoded DDS bytes\n";
}

} // namespace

int main(int argc, char** argv)
{
    const auto options = parseOptions(argc, argv);
    if (!options)
        return EXIT_FAILURE;

    const std::filesystem::path data_directory =
        options->dataDirectory.empty()
            ? r3d::resource::defaultGameDataDirectory()
            : options->dataDirectory;

    std::optional<r3d::resource::ResourceFileSystem> resources;
    std::optional<SceneAssets> assets;
    try
    {
        resources.emplace(data_directory);
        assets.emplace(loadSceneAssets(*resources));
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Original game asset loading failed: "
                  << exception.what() << '\n';
        return EXIT_FAILURE;
    }
    printAssetSummary(*assets, *resources);
    if (options->verifyAssets)
    {
        std::cout << "R3D asset verification passed\n";
        return EXIT_SUCCESS;
    }

    if (!SDL_SetAppMetadata("Motor Rock", "1.3.1", "org.rrr3d.motorrock"))
    {
        std::cerr << "Unable to set SDL metadata: " << SDL_GetError() << '\n';
        return EXIT_FAILURE;
    }
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << "Unable to initialize SDL3: " << SDL_GetError() << '\n';
        return EXIT_FAILURE;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Motor Rock — original .r3d renderer slice", initial_width,
        initial_height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr)
    {
        std::cerr << "Unable to create window: " << SDL_GetError() << '\n';
        SDL_Quit();
        return EXIT_FAILURE;
    }

    const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    void* native_window = SDL_GetPointerProperty(
        properties, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    int pixel_width = 0;
    int pixel_height = 0;
    if (native_window == nullptr ||
        !SDL_GetWindowSizeInPixels(window, &pixel_width, &pixel_height))
    {
        std::cerr << "Unable to obtain Cocoa window/drawable size: "
                  << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    auto device = createBgfxGraphicsDevice();
    std::string renderer_error;
    if (!device->initialize(
            {{native_window}, static_cast<std::uint32_t>(pixel_width),
             static_cast<std::uint32_t>(pixel_height), true},
            renderer_error))
    {
        std::cerr << "Renderer initialization failed: " << renderer_error << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    const Shader shader = device->createShader(
        {rrr3d_vs_static_scene, sizeof(rrr3d_vs_static_scene)},
        {rrr3d_fs_static_scene, sizeof(rrr3d_fs_static_scene)},
        "legacy .r3d material");
    const auto body_vertices = rendererVertices(assets->body);
    const auto wheel_vertices = rendererVertices(assets->wheel);
    const Mesh body = device->createMesh(
        body_vertices.data(), body_vertices.size(), assets->body.indices.data(),
        assets->body.indices.size());
    const Mesh wheel = device->createMesh(
        wheel_vertices.data(), wheel_vertices.size(),
        assets->wheel.indices.data(), assets->wheel.indices.size());
    const Texture car_texture = device->createTextureContainer(
        assets->texture.data(), assets->texture.size(), "Car/buggi.dds");

    if (!valid(shader) || !valid(body) || !valid(wheel) ||
        !valid(car_texture))
    {
        std::cerr << "Unable to upload original Motor Rock render resources\n";
        if (valid(car_texture))
            device->destroy(car_texture);
        if (valid(wheel))
            device->destroy(wheel);
        if (valid(body))
            device->destroy(body);
        if (valid(shader))
            device->destroy(shader);
        device.reset();
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    std::cout << "RRR3D renderer backend: bgfx/" << device->backendName()
              << "\nStatic scene source: original ResourceManager paths, "
                 ".r3d geometry/material groups, DDS texture\n";

    bool running = true;
    std::uint32_t rendered_frames = 0;
    Camera camera = makeCamera(*device,
        static_cast<std::uint32_t>(pixel_width),
        static_cast<std::uint32_t>(pixel_height));
    const PipelineState body_pipeline;
    PipelineState mirrored_wheel_pipeline = body_pipeline;
    mirrored_wheel_pipeline.faceCulling =
        PipelineState::FaceCulling::CounterClockwise;
    const Transform body_transform =
        makeTransform(1.0F, 1.0F, 1.0F, 0.0F, 0.0F, 0.0F);

    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT ||
                event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
                (event.type == SDL_EVENT_KEY_DOWN &&
                 event.key.key == SDLK_ESCAPE))
            {
                running = false;
            }
            else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
            {
                pixel_width = std::max(event.window.data1, 1);
                pixel_height = std::max(event.window.data2, 1);
                device->resize(static_cast<std::uint32_t>(pixel_width),
                               static_cast<std::uint32_t>(pixel_height));
                camera = makeCamera(*device,
                    static_cast<std::uint32_t>(pixel_width),
                    static_cast<std::uint32_t>(pixel_height));
            }
        }

        device->beginFrame(camera, 0x111722ff);
        drawMaterialGroups(*device, body, shader, car_texture, body_transform,
                           body_pipeline, assets->body);
        for (const auto& position : assets->wheelPositions)
        {
            const bool mirrored = position[1] < 0.0F;
            const Transform wheel_transform = makeTransform(
                1.0F, mirrored ? -1.0F : 1.0F, 1.0F,
                position[0], position[1], position[2]);
            drawMaterialGroups(*device, wheel, shader, car_texture,
                               wheel_transform,
                               mirrored ? mirrored_wheel_pipeline
                                        : body_pipeline,
                               assets->wheel);
        }
        device->endFrame();
        ++rendered_frames;

        if (options->smokeFrames != 0 &&
            rendered_frames >= options->smokeFrames)
        {
            std::cout << "bgfx/Metal original-asset smoke test completed after "
                      << rendered_frames << " frames\n";
            running = false;
        }
    }

    device->destroy(car_texture);
    device->destroy(wheel);
    device->destroy(body);
    device->destroy(shader);
    device.reset();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}
