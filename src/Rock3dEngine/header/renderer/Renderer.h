#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace r3d::renderer
{

constexpr std::uint16_t invalid_resource = UINT16_MAX;

struct NativeWindowHandle
{
    void* value = nullptr;
};

struct SwapChain
{
    NativeWindowHandle window;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool vsync = true;
};

struct Vertex
{
    float x;
    float y;
    float z;
    std::uint32_t color;
    float u;
    float v;
};

// Vertex layout used by the original .r3d mesh format.  Keep this separate
// from the colour vertex used by the early renderer spike: the game stores
// normals and texture coordinates, not baked per-vertex colours.
struct StaticMeshVertex
{
    float x;
    float y;
    float z;
    float normalX;
    float normalY;
    float normalZ;
    float u;
    float v;
    float tangentX = 0.0F;
    float tangentY = 0.0F;
    float tangentZ = 0.0F;
    float bitangentX = 0.0F;
    float bitangentY = 0.0F;
    float bitangentZ = 0.0F;
};

enum class VertexLayout
{
    PositionColorTexcoord,
    PositionNormalTexcoord,
};

enum class IndexType
{
    UInt16,
    UInt32,
};

struct VertexBuffer
{
    std::uint16_t value = invalid_resource;
};

struct IndexBuffer
{
    std::uint16_t value = invalid_resource;
};

struct Shader
{
    std::uint16_t value = invalid_resource;
};

struct Texture
{
    std::uint16_t value = invalid_resource;
};

struct Sampler
{
    std::uint16_t value = invalid_resource;
};

struct RenderTarget
{
    std::uint16_t value = invalid_resource;
};

enum class RenderTargetFormat : std::uint8_t
{
    Rgba8,
    Rgba16F,
    R32F,
};

// Fixed ordering mirrors the original GraphManager pipeline while keeping
// D3D9/bgfx types out of game code.
enum class RenderPass : std::uint8_t
{
    Shadow,
    ShadowFar,
    ShadowThird,
    EnvironmentPositiveX,
    EnvironmentNegativeX,
    EnvironmentPositiveY,
    EnvironmentNegativeY,
    EnvironmentPositiveZ,
    EnvironmentNegativeZ,
    Reflection,
    Scene,
    Water,
    Luminance64,
    Luminance16,
    Luminance4,
    Luminance1,
    LuminanceAdapt,
    BloomExtract,
    BloomHorizontal,
    BloomVertical,
    Composite,
    Overlay,
    Count,
};

inline constexpr std::size_t renderPassCount =
    static_cast<std::size_t>(RenderPass::Count);

struct RenderTelemetry
{
    std::array<std::uint32_t, renderPassCount> beginCount{};
    std::array<std::uint32_t, renderPassCount> drawCount{};
    std::array<std::uint32_t, 7> lightingDrawCount{};
    std::uint32_t environmentMappedDrawCount = 0;
    std::uint32_t normalMappedDrawCount = 0;
    std::uint32_t transientDrawCount = 0;
    std::uint32_t activeSpotLightCount = 0;
};

struct DepthBuffer
{
    std::uint16_t value = invalid_resource;
};

struct Mesh
{
    VertexBuffer vertices;
    IndexBuffer indices;
};

struct DrawRange
{
    std::uint32_t firstIndex = 0;
    // Zero means the complete index buffer.  This keeps the common path
    // compact while allowing the material groups stored in .r3d meshes to be
    // submitted without rewriting their indices.
    std::uint32_t indexCount = 0;
};

struct ShaderBinary
{
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
};

struct PipelineState
{
    enum class FaceCulling
    {
        None,
        Clockwise,
        CounterClockwise,
    };

    enum class BlendMode
    {
        Opaque,
        Alpha,
        Additive,
    };

    bool writeColor = true;
    bool writeDepth = true;
    bool depthTest = true;
    FaceCulling faceCulling = FaceCulling::Clockwise;
    BlendMode blendMode = BlendMode::Opaque;
    // Compatibility flag for the menu/early portable renderers.
    bool alphaBlend = false;
    bool multisampling = true;
};

struct MaterialState
{
    std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
    // xy scales source UVs and zw offsets them.  Original LibMaterial uses
    // this for animated texture atlases and scrolling effect materials.
    std::array<float, 4> textureTransform{1.0F, 1.0F, 0.0F, 0.0F};
    float alphaReference = 0.0F;
    float emissive = 0.0F;
    float specular = 0.0F;
    float shininess = 128.0F;
    bool ignoreFog = false;
    float reflectionStrength = 0.0F;
    bool receivesShadow = true;
    // Optional per-draw override.  The sky cube uses the static source DDS
    // while reflective scene materials fall back to the current pass cube.
    Texture environmentTexture;
    // Optional second LibMaterial sampler used by BumpMapShader.
    Texture normalTexture;
    // Generic parameters used by the source-derived post-process shaders.
    std::array<float, 4> postParameters{};
};

struct Camera
{
    std::array<float, 16> view{};
    std::array<float, 16> projection{};
};

struct SceneLighting
{
    // Six racers require seven headlights (two for the human and one per AI),
    // while Environment owns three additional fixed slots.  Keep two spare
    // slots so source data can add lights without silently dropping a car.
    static constexpr std::size_t maximumSpotLights = 12U;

    // w is the source directional-light enable flag.
    std::array<float, 4> lightDirection{-0.45F, -0.35F, 0.82F, 1.0F};
    std::array<float, 4> ambient{0.22F, 0.22F, 0.22F, 1.0F};
    std::array<float, 4> fogColor{0.58F, 0.76F, 0.92F, 0.5F};
    std::array<float, 4> cameraPosition{0.0F, 0.0F, 0.0F, 1.0F};
    // Environment uses up to three fixed spots; Player::SetHeadlight adds
    // two moving spots for the human and one for each AI in ewNight.
    // Position.w is range; direction.w is the enabled flag. Cone.x/y are
    // cos(phi/2) and cos(theta/2), matching D3DLIGHT9 spotlight semantics.
    std::array<std::array<float, 4>, maximumSpotLights> lampPositions{};
    std::array<std::array<float, 4>, maximumSpotLights> lampDirections{};
    std::array<std::array<float, 4>, maximumSpotLights> lampColors{};
    std::array<std::array<float, 4>, maximumSpotLights> lampCones{};
};

struct RenderPassState
{
    Texture reflectionTexture;
    Texture shadowTexture;
    Texture shadowTextureFar;
    // Cube texture used by ReflMappShader/ReflBumpMappShader.
    Texture environmentTexture;
    std::array<float, 16> reflectionViewProjection{
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F};
    std::array<float, 16> shadowViewProjection{
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F};
    std::array<float, 16> shadowViewProjectionFar{
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F};
    std::array<float, 4> clipPlane{0.0F, 0.0F, 1.0F, 0.0F};
    bool clipPlaneEnabled = false;
    bool shadowsEnabled = false;
    // Environment::EnableLamp creates one unsplit shadow map per spot
    // light. In that mode the three fixed environment lamps use the near,
    // far and otherwise-idle reflection texture/matrix slots.
    bool spotShadows = false;
    bool invertCulling = false;
    float shadowStrength = 0.62F;
    float shadowSplitDistance = 60.0F;
    float shadowMapSize = 2048.0F;
    float shadowDepthBias = 0.0015F;
};

struct CubeRenderTarget
{
    Texture texture;
    std::array<RenderTarget, 6> faces{};
};

struct Transform
{
    std::array<float, 16> matrix{};
};

class FontRenderer
{
public:
    virtual ~FontRenderer() = default;
};

class Effect
{
public:
    virtual ~Effect() = default;
};

class GraphicsDevice
{
public:
    virtual ~GraphicsDevice() = default;

    virtual bool initialize(const SwapChain& swapChain, std::string& error) = 0;
    virtual void resize(std::uint32_t width, std::uint32_t height) = 0;

    virtual Shader createShader(ShaderBinary vertex, ShaderBinary fragment,
                                std::string_view name) = 0;
    virtual Mesh createMesh(const void* vertices, std::size_t vertexCount,
                            VertexLayout layout, const void* indices,
                            std::size_t indexCount, IndexType indexType) = 0;

    Mesh createMesh(const Vertex* vertices, std::size_t vertexCount,
                    const std::uint16_t* indices, std::size_t indexCount)
    {
        return createMesh(vertices, vertexCount,
                          VertexLayout::PositionColorTexcoord, indices,
                          indexCount, IndexType::UInt16);
    }

    Mesh createMesh(const StaticMeshVertex* vertices,
                    std::size_t vertexCount, const std::uint32_t* indices,
                    std::size_t indexCount)
    {
        return createMesh(vertices, vertexCount,
                          VertexLayout::PositionNormalTexcoord, indices,
                          indexCount, IndexType::UInt32);
    }

    virtual Texture createTextureRgba8(std::uint16_t width, std::uint16_t height,
                                       const std::uint8_t* pixels,
                                       std::size_t byteCount) = 0;
    // Creates a texture from an encoded game resource such as DDS.  Decoding
    // belongs to the backend so compressed formats and mip levels are not
    // discarded by the engine abstraction.
    virtual Texture createTextureContainer(const std::uint8_t* data,
                                           std::size_t byteCount,
                                           std::string_view name) = 0;
    virtual RenderTarget createRenderTarget(
        std::uint16_t width, std::uint16_t height,
        RenderTargetFormat format, bool depth,
        std::string_view name) = 0;
    virtual CubeRenderTarget createCubeRenderTarget(
        std::uint16_t size, RenderTargetFormat format, bool depth,
        std::string_view name) = 0;
    virtual Texture renderTargetTexture(
        RenderTarget target, std::uint8_t attachment = 0) const = 0;

    virtual void destroy(Shader shader) = 0;
    virtual void destroy(Mesh mesh) = 0;
    virtual void destroy(Texture texture) = 0;
    virtual void destroy(RenderTarget target) = 0;
    virtual void destroy(CubeRenderTarget target) = 0;

    virtual void beginFrame(const Camera& camera, std::uint32_t clearRgba) = 0;
    virtual void resetRenderTelemetry() noexcept = 0;
    virtual const RenderTelemetry& renderTelemetry() const noexcept = 0;
    virtual void beginPass(RenderPass pass, RenderTarget target,
                           const Camera& camera, std::uint32_t clearRgba,
                           bool clearColor, bool clearDepth) = 0;
    // Selects a second, non-clearing view for HUD/UI draws in the same frame.
    virtual void beginOverlay(const Camera& camera) = 0;
    virtual void setPassState(const RenderPassState& state) = 0;
    virtual void setSceneLighting(const SceneLighting& lighting) = 0;
    virtual void draw(Mesh mesh, Shader shader, Texture texture,
                      const Transform& transform,
                      const PipelineState& pipeline,
                      DrawRange range = {},
                      const MaterialState& material = {}) = 0;
    // Per-frame geometry path used by the original FxTrailManager strip.
    virtual void drawTransient(
        const StaticMeshVertex* vertices, std::size_t vertexCount,
        const std::uint32_t* indices, std::size_t indexCount,
        Shader shader, Texture texture, const Transform& transform,
        const PipelineState& pipeline,
        const MaterialState& material = {}) = 0;
    virtual void endFrame() = 0;

    virtual std::string_view backendName() const noexcept = 0;
    virtual bool usesHomogeneousDepth() const noexcept = 0;
};

} // namespace r3d::renderer
