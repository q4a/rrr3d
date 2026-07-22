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

    bool writeColor = true;
    bool writeDepth = true;
    bool depthTest = true;
    FaceCulling faceCulling = FaceCulling::Clockwise;
    bool alphaBlend = false;
    bool multisampling = true;
};

struct Camera
{
    std::array<float, 16> view{};
    std::array<float, 16> projection{};
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

    virtual void destroy(Shader shader) = 0;
    virtual void destroy(Mesh mesh) = 0;
    virtual void destroy(Texture texture) = 0;

    virtual void beginFrame(const Camera& camera, std::uint32_t clearRgba) = 0;
    virtual void draw(Mesh mesh, Shader shader, Texture texture,
                      const Transform& transform,
                      const PipelineState& pipeline,
                      DrawRange range = {}) = 0;
    virtual void endFrame() = 0;

    virtual std::string_view backendName() const noexcept = 0;
    virtual bool usesHomogeneousDepth() const noexcept = 0;
};

} // namespace r3d::renderer
