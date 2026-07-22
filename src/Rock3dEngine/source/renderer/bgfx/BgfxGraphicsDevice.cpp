#include "renderer/BgfxGraphicsDevice.h"

#include <bgfx/bgfx.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <string>

namespace r3d::renderer
{
namespace
{

constexpr bgfx::ViewId scene_view = 0;

bool valid(VertexBuffer handle) noexcept
{
    return handle.value != invalid_resource;
}

bool valid(IndexBuffer handle) noexcept
{
    return handle.value != invalid_resource;
}

bool valid(Shader handle) noexcept
{
    return handle.value != invalid_resource;
}

bool valid(Texture handle) noexcept
{
    return handle.value != invalid_resource;
}

class BgfxGraphicsDevice final : public GraphicsDevice
{
public:
    ~BgfxGraphicsDevice() override
    {
        if (initialized_)
        {
            if (bgfx::isValid(texture_sampler_))
                bgfx::destroy(texture_sampler_);
            bgfx::shutdown();
        }
    }

    bool initialize(const SwapChain& swapChain, std::string& error) override
    {
        if (initialized_)
        {
            error = "bgfx graphics device is already initialized";
            return false;
        }
        if (swapChain.window.value == nullptr || swapChain.width == 0 ||
            swapChain.height == 0)
        {
            error = "bgfx requires a native window and non-zero drawable size";
            return false;
        }

        width_ = swapChain.width;
        height_ = swapChain.height;
        reset_flags_ = swapChain.vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;

        bgfx::Init init;
        init.type = bgfx::RendererType::Metal;
        init.vendorId = BGFX_PCI_ID_NONE;
        init.fallback = false;
        init.platformData.nwh = swapChain.window.value;
        init.resolution.width = width_;
        init.resolution.height = height_;
        init.resolution.reset = reset_flags_;

        if (!bgfx::init(init))
        {
            error = "bgfx::init failed for the Metal backend";
            return false;
        }
        initialized_ = true;

        if (bgfx::getRendererType() != bgfx::RendererType::Metal)
        {
            error = std::string("bgfx initialized an unexpected backend: ") +
                    bgfx::getRendererName(bgfx::getRendererType());
            bgfx::shutdown();
            initialized_ = false;
            return false;
        }

        color_vertex_layout_.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();

        static_vertex_layout_.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();

        texture_sampler_ = bgfx::createUniform(
            "s_texColor", bgfx::UniformType::Sampler);
        if (!bgfx::isValid(texture_sampler_))
        {
            error = "bgfx could not create the scene texture sampler";
            bgfx::shutdown();
            initialized_ = false;
            return false;
        }

        bgfx::setViewName(scene_view, "Motor Rock .r3d static scene");
        return true;
    }

    void resize(std::uint32_t width, std::uint32_t height) override
    {
        width_ = std::max(width, 1U);
        height_ = std::max(height, 1U);
        bgfx::reset(width_, height_, reset_flags_);
    }

    Shader createShader(ShaderBinary vertex, ShaderBinary fragment,
                        std::string_view name) override
    {
        if (vertex.data == nullptr || fragment.data == nullptr ||
            vertex.size == 0 || fragment.size == 0 ||
            vertex.size > std::numeric_limits<std::uint32_t>::max() ||
            fragment.size > std::numeric_limits<std::uint32_t>::max())
            return {};

        const bgfx::ShaderHandle vertex_shader = bgfx::createShader(bgfx::copy(
            vertex.data, static_cast<std::uint32_t>(vertex.size)));
        const bgfx::ShaderHandle fragment_shader = bgfx::createShader(bgfx::copy(
            fragment.data, static_cast<std::uint32_t>(fragment.size)));
        if (!bgfx::isValid(vertex_shader) || !bgfx::isValid(fragment_shader))
        {
            if (bgfx::isValid(vertex_shader))
                bgfx::destroy(vertex_shader);
            if (bgfx::isValid(fragment_shader))
                bgfx::destroy(fragment_shader);
            return {};
        }

        const std::string owned_name(name);
        bgfx::setName(vertex_shader, (owned_name + " vertex").c_str());
        bgfx::setName(fragment_shader, (owned_name + " fragment").c_str());
        const bgfx::ProgramHandle program = bgfx::createProgram(
            vertex_shader, fragment_shader, true);
        return {program.idx};
    }

    Mesh createMesh(const void* vertices, std::size_t vertexCount,
                    VertexLayout layout, const void* indices,
                    std::size_t indexCount, IndexType indexType) override
    {
        const std::size_t vertex_stride =
            layout == VertexLayout::PositionNormalTexcoord
                ? sizeof(StaticMeshVertex)
                : sizeof(Vertex);
        const std::size_t index_stride =
            indexType == IndexType::UInt32 ? sizeof(std::uint32_t)
                                           : sizeof(std::uint16_t);
        if (vertices == nullptr || indices == nullptr || vertexCount == 0 ||
            indexCount == 0 ||
            vertexCount > std::numeric_limits<std::uint32_t>::max() /
                              vertex_stride ||
            indexCount > std::numeric_limits<std::uint32_t>::max() /
                             index_stride)
            return {};

        const auto vertex_bytes = static_cast<std::uint32_t>(
            vertexCount * vertex_stride);
        const auto index_bytes = static_cast<std::uint32_t>(
            indexCount * index_stride);
        const bgfx::VertexLayout& bgfx_layout =
            layout == VertexLayout::PositionNormalTexcoord
                ? static_vertex_layout_
                : color_vertex_layout_;
        const bgfx::VertexBufferHandle vertex_buffer =
            bgfx::createVertexBuffer(bgfx::copy(vertices, vertex_bytes),
                                     bgfx_layout);
        const bgfx::IndexBufferHandle index_buffer =
            bgfx::createIndexBuffer(
                bgfx::copy(indices, index_bytes),
                indexType == IndexType::UInt32 ? BGFX_BUFFER_INDEX32
                                               : BGFX_BUFFER_NONE);
        if (!bgfx::isValid(vertex_buffer) || !bgfx::isValid(index_buffer))
        {
            if (bgfx::isValid(vertex_buffer))
                bgfx::destroy(vertex_buffer);
            if (bgfx::isValid(index_buffer))
                bgfx::destroy(index_buffer);
            return {};
        }
        return {{vertex_buffer.idx}, {index_buffer.idx}};
    }

    Texture createTextureRgba8(std::uint16_t width, std::uint16_t height,
                               const std::uint8_t* pixels,
                               std::size_t byteCount) override
    {
        const std::size_t expected =
            static_cast<std::size_t>(width) * height * 4U;
        if (pixels == nullptr || width == 0 || height == 0 ||
            byteCount != expected ||
            byteCount > std::numeric_limits<std::uint32_t>::max())
            return {};

        const bgfx::TextureHandle texture = bgfx::createTexture2D(
            width, height, false, 1, bgfx::TextureFormat::RGBA8,
            BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT |
                BGFX_SAMPLER_MIP_POINT,
            bgfx::copy(pixels, static_cast<std::uint32_t>(byteCount)));
        return {texture.idx};
    }

    Texture createTextureContainer(const std::uint8_t* data,
                                   std::size_t byteCount,
                                   std::string_view name) override
    {
        if (data == nullptr || byteCount == 0 ||
            byteCount > std::numeric_limits<std::uint32_t>::max())
            return {};

        bgfx::TextureInfo info{};
        const bgfx::TextureHandle texture = bgfx::createTexture(
            bgfx::copy(data, static_cast<std::uint32_t>(byteCount)),
            BGFX_SAMPLER_MIN_ANISOTROPIC |
                BGFX_SAMPLER_MAG_ANISOTROPIC,
            0, &info);
        if (bgfx::isValid(texture))
        {
            const std::string owned_name(name);
            bgfx::setName(texture, owned_name.c_str());
        }
        return {texture.idx};
    }

    void destroy(Shader shader) override
    {
        if (valid(shader))
            bgfx::destroy(bgfx::ProgramHandle{shader.value});
    }

    void destroy(Mesh mesh) override
    {
        if (valid(mesh.vertices))
            bgfx::destroy(bgfx::VertexBufferHandle{mesh.vertices.value});
        if (valid(mesh.indices))
            bgfx::destroy(bgfx::IndexBufferHandle{mesh.indices.value});
    }

    void destroy(Texture texture) override
    {
        if (valid(texture))
            bgfx::destroy(bgfx::TextureHandle{texture.value});
    }

    void beginFrame(const Camera& camera, std::uint32_t clearRgba) override
    {
        bgfx::setViewRect(scene_view, 0, 0,
                          static_cast<std::uint16_t>(
                              std::min(width_, std::uint32_t(UINT16_MAX))),
                          static_cast<std::uint16_t>(
                              std::min(height_, std::uint32_t(UINT16_MAX))));
        bgfx::setViewClear(scene_view, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                           clearRgba, 1.0F, 0);
        bgfx::setViewTransform(scene_view, camera.view.data(),
                               camera.projection.data());
        bgfx::touch(scene_view);
    }

    void draw(Mesh mesh, Shader shader, Texture texture,
              const Transform& transform,
              const PipelineState& pipeline, DrawRange range) override
    {
        if (!valid(mesh.vertices) || !valid(mesh.indices) || !valid(shader) ||
            !valid(texture))
            return;

        std::uint64_t state = 0;
        if (pipeline.writeColor)
            state |= BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;
        if (pipeline.writeDepth)
            state |= BGFX_STATE_WRITE_Z;
        if (pipeline.depthTest)
            state |= BGFX_STATE_DEPTH_TEST_LESS;
        if (pipeline.faceCulling == PipelineState::FaceCulling::Clockwise)
            state |= BGFX_STATE_CULL_CW;
        else if (pipeline.faceCulling ==
                 PipelineState::FaceCulling::CounterClockwise)
            state |= BGFX_STATE_CULL_CCW;
        if (pipeline.alphaBlend)
            state |= BGFX_STATE_BLEND_ALPHA;
        if (pipeline.multisampling)
            state |= BGFX_STATE_MSAA;

        bgfx::setTransform(transform.matrix.data());
        bgfx::setVertexBuffer(0,
            bgfx::VertexBufferHandle{mesh.vertices.value});
        if (range.indexCount == 0)
        {
            bgfx::setIndexBuffer(bgfx::IndexBufferHandle{mesh.indices.value});
        }
        else
        {
            bgfx::setIndexBuffer(bgfx::IndexBufferHandle{mesh.indices.value},
                                 range.firstIndex, range.indexCount);
        }
        bgfx::setTexture(0, texture_sampler_,
                         bgfx::TextureHandle{texture.value});
        bgfx::setState(state);
        bgfx::submit(scene_view, bgfx::ProgramHandle{shader.value});
    }

    void endFrame() override
    {
        bgfx::frame();
    }

    std::string_view backendName() const noexcept override
    {
        return initialized_ ? bgfx::getRendererName(bgfx::getRendererType())
                            : "uninitialized";
    }

    bool usesHomogeneousDepth() const noexcept override
    {
        return initialized_ && bgfx::getCaps()->homogeneousDepth;
    }

private:
    bool initialized_ = false;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::uint32_t reset_flags_ = BGFX_RESET_NONE;
    bgfx::VertexLayout color_vertex_layout_;
    bgfx::VertexLayout static_vertex_layout_;
    bgfx::UniformHandle texture_sampler_ = BGFX_INVALID_HANDLE;
};

} // namespace

std::unique_ptr<GraphicsDevice> createBgfxGraphicsDevice()
{
    return std::make_unique<BgfxGraphicsDevice>();
}

} // namespace r3d::renderer
