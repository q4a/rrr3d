#include "renderer/BgfxGraphicsDevice.h"

#include <bgfx/bgfx.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>

namespace r3d::renderer
{
namespace
{

constexpr bgfx::ViewId viewId(RenderPass pass) noexcept
{
    return static_cast<bgfx::ViewId>(pass);
}

constexpr bgfx::ViewId scene_view = viewId(RenderPass::Scene);
constexpr bgfx::ViewId overlay_view = viewId(RenderPass::Overlay);

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
            for (const auto& [handle, info] : render_targets_)
            {
                static_cast<void>(info);
                bgfx::destroy(bgfx::FrameBufferHandle{handle});
            }
            render_targets_.clear();
            for (const auto& [handle, info] : cube_targets_)
            {
                static_cast<void>(handle);
                for (const auto depth : info.depthTextures)
                    if (bgfx::isValid(depth))
                        bgfx::destroy(depth);
                if (bgfx::isValid(info.colorTexture))
                    bgfx::destroy(info.colorTexture);
            }
            cube_targets_.clear();
            if (bgfx::isValid(texture_sampler_))
                bgfx::destroy(texture_sampler_);
            if (bgfx::isValid(reflection_sampler_))
                bgfx::destroy(reflection_sampler_);
            if (bgfx::isValid(shadow_sampler_))
                bgfx::destroy(shadow_sampler_);
            if (bgfx::isValid(shadow_far_sampler_))
                bgfx::destroy(shadow_far_sampler_);
            if (bgfx::isValid(environment_sampler_))
                bgfx::destroy(environment_sampler_);
            if (bgfx::isValid(normal_sampler_))
                bgfx::destroy(normal_sampler_);
            if (bgfx::isValid(scene_light_direction_))
                bgfx::destroy(scene_light_direction_);
            if (bgfx::isValid(scene_sun_position_))
                bgfx::destroy(scene_sun_position_);
            if (bgfx::isValid(scene_lamp_positions_))
                bgfx::destroy(scene_lamp_positions_);
            if (bgfx::isValid(scene_lamp_directions_))
                bgfx::destroy(scene_lamp_directions_);
            if (bgfx::isValid(scene_lamp_colors_))
                bgfx::destroy(scene_lamp_colors_);
            if (bgfx::isValid(scene_lamp_cones_))
                bgfx::destroy(scene_lamp_cones_);
            if (bgfx::isValid(scene_ambient_))
                bgfx::destroy(scene_ambient_);
            if (bgfx::isValid(scene_fog_))
                bgfx::destroy(scene_fog_);
            if (bgfx::isValid(scene_camera_))
                bgfx::destroy(scene_camera_);
            if (bgfx::isValid(material_color_))
                bgfx::destroy(material_color_);
            if (bgfx::isValid(material_mapping_color_))
                bgfx::destroy(material_mapping_color_);
            if (bgfx::isValid(material_parameters_))
                bgfx::destroy(material_parameters_);
            if (bgfx::isValid(material_options_))
                bgfx::destroy(material_options_);
            if (bgfx::isValid(texture_transform_))
                bgfx::destroy(texture_transform_);
            if (bgfx::isValid(clip_plane_))
                bgfx::destroy(clip_plane_);
            if (bgfx::isValid(reflection_view_projection_))
                bgfx::destroy(reflection_view_projection_);
            if (bgfx::isValid(shadow_view_projection_))
                bgfx::destroy(shadow_view_projection_);
            if (bgfx::isValid(shadow_view_projection_far_))
                bgfx::destroy(shadow_view_projection_far_);
            if (bgfx::isValid(shadow_parameters_))
                bgfx::destroy(shadow_parameters_);
            if (bgfx::isValid(post_parameters_))
                bgfx::destroy(post_parameters_);
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
            .add(bgfx::Attrib::Tangent, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Bitangent, 3, bgfx::AttribType::Float)
            .end();

        texture_sampler_ = bgfx::createUniform(
            "s_texColor", bgfx::UniformType::Sampler);
        reflection_sampler_ = bgfx::createUniform(
            "s_texReflection", bgfx::UniformType::Sampler);
        shadow_sampler_ = bgfx::createUniform(
            "s_texShadow", bgfx::UniformType::Sampler);
        shadow_far_sampler_ = bgfx::createUniform(
            "s_texShadowFar", bgfx::UniformType::Sampler);
        environment_sampler_ = bgfx::createUniform(
            "s_texEnvironment", bgfx::UniformType::Sampler);
        normal_sampler_ = bgfx::createUniform(
            "s_texNormal", bgfx::UniformType::Sampler);
        scene_light_direction_ = bgfx::createUniform(
            "u_sceneLightDirection", bgfx::UniformType::Vec4);
        scene_sun_position_ = bgfx::createUniform(
            "u_sceneSunPosition", bgfx::UniformType::Vec4);
        scene_lamp_positions_ = bgfx::createUniform(
            "u_sceneLampPositions", bgfx::UniformType::Vec4,
            SceneLighting::maximumSpotLights);
        scene_lamp_directions_ = bgfx::createUniform(
            "u_sceneLampDirections", bgfx::UniformType::Vec4,
            SceneLighting::maximumSpotLights);
        scene_lamp_colors_ = bgfx::createUniform(
            "u_sceneLampColors", bgfx::UniformType::Vec4,
            SceneLighting::maximumSpotLights);
        scene_lamp_cones_ = bgfx::createUniform(
            "u_sceneLampCones", bgfx::UniformType::Vec4,
            SceneLighting::maximumSpotLights);
        scene_ambient_ = bgfx::createUniform(
            "u_sceneAmbient", bgfx::UniformType::Vec4);
        scene_fog_ = bgfx::createUniform(
            "u_sceneFog", bgfx::UniformType::Vec4);
        scene_camera_ = bgfx::createUniform(
            "u_sceneCamera", bgfx::UniformType::Vec4);
        material_color_ = bgfx::createUniform(
            "u_materialColor", bgfx::UniformType::Vec4);
        material_mapping_color_ = bgfx::createUniform(
            "u_materialMappingColor", bgfx::UniformType::Vec4);
        material_parameters_ = bgfx::createUniform(
            "u_materialParams", bgfx::UniformType::Vec4);
        material_options_ = bgfx::createUniform(
            "u_materialOptions", bgfx::UniformType::Vec4);
        texture_transform_ = bgfx::createUniform(
            "u_textureTransform", bgfx::UniformType::Vec4);
        clip_plane_ = bgfx::createUniform(
            "u_clipPlane", bgfx::UniformType::Vec4);
        reflection_view_projection_ = bgfx::createUniform(
            "u_reflectionViewProj", bgfx::UniformType::Mat4);
        shadow_view_projection_ = bgfx::createUniform(
            "u_shadowViewProj", bgfx::UniformType::Mat4);
        shadow_view_projection_far_ = bgfx::createUniform(
            "u_shadowViewProjFar", bgfx::UniformType::Mat4);
        shadow_parameters_ = bgfx::createUniform(
            "u_shadowParams", bgfx::UniformType::Vec4);
        post_parameters_ = bgfx::createUniform(
            "u_postParams", bgfx::UniformType::Vec4);
        if (!bgfx::isValid(texture_sampler_) ||
            !bgfx::isValid(reflection_sampler_) ||
            !bgfx::isValid(shadow_sampler_) ||
            !bgfx::isValid(shadow_far_sampler_) ||
            !bgfx::isValid(environment_sampler_) ||
            !bgfx::isValid(normal_sampler_) ||
            !bgfx::isValid(scene_light_direction_) ||
            !bgfx::isValid(scene_sun_position_) ||
            !bgfx::isValid(scene_lamp_positions_) ||
            !bgfx::isValid(scene_lamp_directions_) ||
            !bgfx::isValid(scene_lamp_colors_) ||
            !bgfx::isValid(scene_lamp_cones_) ||
            !bgfx::isValid(scene_ambient_) ||
            !bgfx::isValid(scene_fog_) ||
            !bgfx::isValid(scene_camera_) ||
            !bgfx::isValid(material_color_) ||
            !bgfx::isValid(material_mapping_color_) ||
            !bgfx::isValid(material_parameters_) ||
            !bgfx::isValid(material_options_) ||
            !bgfx::isValid(texture_transform_) ||
            !bgfx::isValid(clip_plane_) ||
            !bgfx::isValid(reflection_view_projection_) ||
            !bgfx::isValid(shadow_view_projection_) ||
            !bgfx::isValid(shadow_view_projection_far_) ||
            !bgfx::isValid(shadow_parameters_) ||
            !bgfx::isValid(post_parameters_))
        {
            error = "bgfx could not create the scene texture sampler";
            bgfx::shutdown();
            initialized_ = false;
            return false;
        }

        bgfx::setViewName(viewId(RenderPass::EnvironmentPositiveX),
                          "Motor Rock environment +X");
        bgfx::setViewName(viewId(RenderPass::EnvironmentNegativeX),
                          "Motor Rock environment -X");
        bgfx::setViewName(viewId(RenderPass::EnvironmentPositiveY),
                          "Motor Rock environment +Y");
        bgfx::setViewName(viewId(RenderPass::EnvironmentNegativeY),
                          "Motor Rock environment -Y");
        bgfx::setViewName(viewId(RenderPass::EnvironmentPositiveZ),
                          "Motor Rock environment +Z");
        bgfx::setViewName(viewId(RenderPass::EnvironmentNegativeZ),
                          "Motor Rock environment -Z");
        bgfx::setViewName(viewId(RenderPass::Reflection),
                          "Motor Rock planar reflection");
        bgfx::setViewName(viewId(RenderPass::Shadow),
                          "Motor Rock shadow map near split");
        bgfx::setViewName(viewId(RenderPass::ShadowFar),
                          "Motor Rock shadow map far split");
        bgfx::setViewName(viewId(RenderPass::ShadowThird),
                          "Motor Rock third spot shadow map");
        bgfx::setViewName(scene_view, "Motor Rock HDR scene");
        bgfx::setViewName(viewId(RenderPass::Water),
                          "Motor Rock water/refraction");
        bgfx::setViewName(viewId(RenderPass::Luminance64),
                          "Motor Rock luminance 64");
        bgfx::setViewName(viewId(RenderPass::Luminance16),
                          "Motor Rock luminance 16");
        bgfx::setViewName(viewId(RenderPass::Luminance4),
                          "Motor Rock luminance 4");
        bgfx::setViewName(viewId(RenderPass::Luminance1),
                          "Motor Rock luminance 1");
        bgfx::setViewName(viewId(RenderPass::LuminanceAdapt),
                          "Motor Rock luminance adaptation");
        bgfx::setViewName(viewId(RenderPass::BloomExtract),
                          "Motor Rock bloom extract");
        bgfx::setViewName(viewId(RenderPass::BloomHorizontal),
                          "Motor Rock bloom horizontal");
        bgfx::setViewName(viewId(RenderPass::BloomVertical),
                          "Motor Rock bloom vertical");
        bgfx::setViewName(viewId(RenderPass::Composite),
                          "Motor Rock tone map");
        bgfx::setViewName(overlay_view, "Motor Rock HUD");
        return true;
    }

    void resize(std::uint32_t width, std::uint32_t height) override
    {
        width_ = std::max(width, 1U);
        height_ = std::max(height, 1U);
        bgfx::reset(width_, height_, reset_flags_);
    }

    void configureQuality(std::uint32_t filtering,
                          std::uint32_t multisampling) override
    {
        inherited_filter_flags_ = filtering == 0U
            ? 0U
            : static_cast<std::uint32_t>(
                  BGFX_SAMPLER_MIN_ANISOTROPIC |
                  BGFX_SAMPLER_MAG_ANISOTROPIC);

        const std::uint32_t preserved =
            reset_flags_ & ~(BGFX_RESET_MSAA_X2 | BGFX_RESET_MSAA_X4 |
                             BGFX_RESET_MSAA_X8 | BGFX_RESET_MSAA_X16);
        switch (multisampling)
        {
        case 1U:
            reset_flags_ = preserved | BGFX_RESET_MSAA_X2;
            break;
        case 2U:
            reset_flags_ = preserved | BGFX_RESET_MSAA_X4;
            break;
        case 3U:
            reset_flags_ = preserved | BGFX_RESET_MSAA_X8;
            break;
        default:
            reset_flags_ = preserved;
            break;
        }
        if (initialized_)
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

    RenderTarget createRenderTarget(
        std::uint16_t width, std::uint16_t height,
        RenderTargetFormat format, bool depth,
        std::string_view name) override
    {
        if (width == 0 || height == 0)
            return {};
        bgfx::TextureFormat::Enum colorFormat =
            bgfx::TextureFormat::RGBA8;
        if (format == RenderTargetFormat::Rgba16F)
            colorFormat = bgfx::TextureFormat::RGBA16F;
        else if (format == RenderTargetFormat::R32F)
            colorFormat = bgfx::TextureFormat::R32F;
        const std::uint64_t flags =
            BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP |
            BGFX_SAMPLER_V_CLAMP;
        std::array<bgfx::TextureHandle, 2> attachments{
            bgfx::createTexture2D(width, height, false, 1,
                                  colorFormat, flags),
            BGFX_INVALID_HANDLE};
        std::uint8_t count = 1;
        if (depth)
        {
            attachments[1] = bgfx::createTexture2D(
                width, height, false, 1, bgfx::TextureFormat::D32F,
                BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP |
                    BGFX_SAMPLER_V_CLAMP);
            count = 2;
        }
        if (!bgfx::isValid(attachments[0]) ||
            (depth && !bgfx::isValid(attachments[1])))
        {
            for (const auto attachment : attachments)
                if (bgfx::isValid(attachment))
                    bgfx::destroy(attachment);
            return {};
        }
        const auto framebuffer =
            bgfx::createFrameBuffer(count, attachments.data(), true);
        if (!bgfx::isValid(framebuffer))
        {
            for (std::uint8_t index = 0; index < count; ++index)
                bgfx::destroy(attachments[index]);
            return {};
        }
        const std::string ownedName(name);
        bgfx::setName(framebuffer, ownedName.c_str());
        render_targets_.emplace(
            framebuffer.idx, TargetInfo{width, height});
        return {framebuffer.idx};
    }

    CubeRenderTarget createCubeRenderTarget(
        std::uint16_t size, RenderTargetFormat format, bool depth,
        std::string_view name) override
    {
        if (size == 0)
            return {};
        // Metal exposes BGRA8 as the renderable 8-bit cube format even
        // though regular 2D render targets also accept RGBA8.
        bgfx::TextureFormat::Enum colorFormat =
            bgfx::TextureFormat::BGRA8;
        if (format == RenderTargetFormat::Rgba16F)
            colorFormat = bgfx::TextureFormat::RGBA16F;
        else if (format == RenderTargetFormat::R32F)
            colorFormat = bgfx::TextureFormat::R32F;
        const std::uint64_t flags = BGFX_TEXTURE_RT;
        const bgfx::TextureHandle color = bgfx::createTextureCube(
            size, false, 1, colorFormat, flags);
        if (!bgfx::isValid(color))
            return {};

        CubeRenderTarget result;
        result.texture = {color.idx};
        CubeTargetInfo info;
        info.colorTexture = color;
        const std::string ownedName(name);
        bgfx::setName(color, (ownedName + " texture").c_str());
        for (std::uint16_t face = 0; face < result.faces.size(); ++face)
        {
            if (depth)
            {
                info.depthTextures[face] = bgfx::createTexture2D(
                    size, size, false, 1, bgfx::TextureFormat::D32F,
                    BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP |
                        BGFX_SAMPLER_V_CLAMP);
                if (!bgfx::isValid(info.depthTextures[face]))
                {
                    for (std::uint16_t previous = 0; previous < face;
                         ++previous)
                    {
                        bgfx::destroy(bgfx::FrameBufferHandle{
                            result.faces[previous].value});
                        render_targets_.erase(
                            result.faces[previous].value);
                        if (bgfx::isValid(info.depthTextures[previous]))
                            bgfx::destroy(info.depthTextures[previous]);
                    }
                    bgfx::destroy(color);
                    return {};
                }
            }
            std::array<bgfx::Attachment, 2> attachments;
            attachments[0].init(color, bgfx::Access::Write, face);
            std::uint8_t count = 1;
            if (depth)
            {
                attachments[1].init(
                    info.depthTextures[face], bgfx::Access::Write,
                    0, 1, 0, BGFX_RESOLVE_NONE);
                count = 2;
            }
            const auto framebuffer =
                bgfx::createFrameBuffer(count, attachments.data(), false);
            if (!bgfx::isValid(framebuffer))
            {
                if (bgfx::isValid(info.depthTextures[face]))
                    bgfx::destroy(info.depthTextures[face]);
                for (std::uint16_t previous = 0; previous < face;
                     ++previous)
                {
                    bgfx::destroy(bgfx::FrameBufferHandle{
                        result.faces[previous].value});
                    render_targets_.erase(result.faces[previous].value);
                    if (bgfx::isValid(info.depthTextures[previous]))
                        bgfx::destroy(info.depthTextures[previous]);
                }
                bgfx::destroy(color);
                return {};
            }
            bgfx::setName(
                framebuffer,
                (ownedName + " face " + std::to_string(face)).c_str());
            result.faces[face] = {framebuffer.idx};
            render_targets_.emplace(
                framebuffer.idx, TargetInfo{size, size});
        }
        cube_targets_.emplace(color.idx, info);
        return result;
    }

    Texture renderTargetTexture(
        RenderTarget target, std::uint8_t attachment) const override
    {
        if (target.value == invalid_resource ||
            !render_targets_.contains(target.value) ||
            attachment > 1U)
            return {};
        const auto texture = bgfx::getTexture(
            bgfx::FrameBufferHandle{target.value}, attachment);
        return bgfx::isValid(texture) ? Texture{texture.idx} : Texture{};
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

    void destroy(RenderTarget target) override
    {
        const auto found = render_targets_.find(target.value);
        if (found == render_targets_.end())
            return;
        bgfx::destroy(bgfx::FrameBufferHandle{target.value});
        render_targets_.erase(found);
    }

    void destroy(CubeRenderTarget target) override
    {
        const auto found = cube_targets_.find(target.texture.value);
        if (found == cube_targets_.end())
            return;
        for (const auto face : target.faces)
        {
            if (render_targets_.contains(face.value))
            {
                bgfx::destroy(bgfx::FrameBufferHandle{face.value});
                render_targets_.erase(face.value);
            }
        }
        for (const auto depth : found->second.depthTextures)
            if (bgfx::isValid(depth))
                bgfx::destroy(depth);
        if (bgfx::isValid(found->second.colorTexture))
            bgfx::destroy(found->second.colorTexture);
        cube_targets_.erase(found);
    }

    void beginFrame(const Camera& camera, std::uint32_t clearRgba) override
    {
        resetRenderTelemetry();
        setPassState({});
        beginPass(RenderPass::Scene, {}, camera, clearRgba, true, true);
    }

    void resetRenderTelemetry() noexcept override
    {
        telemetry_ = {};
    }

    const RenderTelemetry& renderTelemetry() const noexcept override
    {
        return telemetry_;
    }

    void beginPass(RenderPass pass, RenderTarget target,
                   const Camera& camera, std::uint32_t clearRgba,
                   bool clearColor, bool clearDepth) override
    {
        current_pass_ = pass;
        ++telemetry_.beginCount[static_cast<std::size_t>(pass)];
        current_view_ = viewId(pass);
        std::uint16_t viewWidth = static_cast<std::uint16_t>(
            std::min(width_, std::uint32_t(UINT16_MAX)));
        std::uint16_t viewHeight = static_cast<std::uint16_t>(
            std::min(height_, std::uint32_t(UINT16_MAX)));
        const auto found = render_targets_.find(target.value);
        if (found != render_targets_.end())
        {
            viewWidth = found->second.width;
            viewHeight = found->second.height;
            bgfx::setViewFrameBuffer(
                current_view_,
                bgfx::FrameBufferHandle{target.value});
        }
        else
        {
            bgfx::setViewFrameBuffer(
                current_view_, BGFX_INVALID_HANDLE);
        }
        bgfx::setViewRect(current_view_, 0, 0, viewWidth, viewHeight);
        std::uint16_t clearFlags = BGFX_CLEAR_NONE;
        if (clearColor)
            clearFlags |= BGFX_CLEAR_COLOR;
        if (clearDepth)
            clearFlags |= BGFX_CLEAR_DEPTH;
        bgfx::setViewClear(current_view_, clearFlags, clearRgba, 1.0F, 0);
        bgfx::setViewTransform(current_view_, camera.view.data(),
                               camera.projection.data());
        bgfx::setViewMode(current_view_, bgfx::ViewMode::Sequential);
        bgfx::touch(current_view_);
    }

    void beginOverlay(const Camera& camera) override
    {
        setPassState({});
        beginPass(RenderPass::Overlay, {}, camera, 0, false, false);
    }

    void setSceneLighting(const SceneLighting& lighting) override
    {
        scene_lighting_ = lighting;
        const auto active = static_cast<std::uint32_t>(
            std::count_if(
                lighting.lampDirections.begin(),
                lighting.lampDirections.end(),
                [](const auto& direction) {
                    return direction[3] > 0.5F;
                }));
        telemetry_.activeSpotLightCount =
            std::max(telemetry_.activeSpotLightCount, active);
    }

    void setPassState(const RenderPassState& state) override
    {
        pass_state_ = state;
    }

    void draw(Mesh mesh, Shader shader, Texture texture,
              const Transform& transform,
              const PipelineState& pipeline, DrawRange range,
              const MaterialState& material) override
    {
        if (!valid(mesh.vertices) || !valid(mesh.indices) || !valid(shader) ||
            !valid(texture))
            return;

        bgfx::setVertexBuffer(
            0, bgfx::VertexBufferHandle{mesh.vertices.value});
        if (range.indexCount == 0)
        {
            bgfx::setIndexBuffer(
                bgfx::IndexBufferHandle{mesh.indices.value});
        }
        else
        {
            bgfx::setIndexBuffer(
                bgfx::IndexBufferHandle{mesh.indices.value},
                range.firstIndex, range.indexCount);
        }
        submit(shader, texture, transform, pipeline, material);
    }

    void drawTransient(
        const StaticMeshVertex* vertices, std::size_t vertexCount,
        const std::uint32_t* indices, std::size_t indexCount,
        Shader shader, Texture texture, const Transform& transform,
        const PipelineState& pipeline,
        const MaterialState& material) override
    {
        if (vertices == nullptr || indices == nullptr || vertexCount == 0 ||
            indexCount == 0 || !valid(shader) || !valid(texture) ||
            vertexCount > std::numeric_limits<std::uint32_t>::max() ||
            indexCount > std::numeric_limits<std::uint32_t>::max())
            return;
        bgfx::TransientVertexBuffer vertexBuffer;
        bgfx::TransientIndexBuffer indexBuffer;
        if (!bgfx::allocTransientBuffers(
                &vertexBuffer, static_vertex_layout_,
                static_cast<std::uint32_t>(vertexCount), &indexBuffer,
                static_cast<std::uint32_t>(indexCount), true))
            return;
        std::memcpy(vertexBuffer.data, vertices,
                    vertexCount * sizeof(StaticMeshVertex));
        std::memcpy(indexBuffer.data, indices,
                    indexCount * sizeof(std::uint32_t));
        bgfx::setVertexBuffer(0, &vertexBuffer);
        bgfx::setIndexBuffer(&indexBuffer);
        ++telemetry_.transientDrawCount;
        submit(shader, texture, transform, pipeline, material);
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
    void submit(Shader shader, Texture texture,
                const Transform& transform,
                const PipelineState& pipeline,
                const MaterialState& material)
    {
        std::uint64_t state = 0;
        if (pipeline.writeColor)
            state |= BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;
        if (pipeline.writeDepth)
            state |= BGFX_STATE_WRITE_Z;
        if (pipeline.depthTest)
            state |= BGFX_STATE_DEPTH_TEST_LESS;
        auto culling = pipeline.faceCulling;
        if (pass_state_.invertCulling)
        {
            if (culling == PipelineState::FaceCulling::Clockwise)
                culling = PipelineState::FaceCulling::CounterClockwise;
            else if (culling ==
                     PipelineState::FaceCulling::CounterClockwise)
                culling = PipelineState::FaceCulling::Clockwise;
        }
        if (culling == PipelineState::FaceCulling::Clockwise)
            state |= BGFX_STATE_CULL_CW;
        else if (culling ==
                 PipelineState::FaceCulling::CounterClockwise)
            state |= BGFX_STATE_CULL_CCW;
        if (pipeline.blendMode == PipelineState::BlendMode::Additive)
        {
            // Material::ApplyBlending(bmAdditive) in the original D3D9
            // renderer uses SRCALPHA, ONE. BGFX_STATE_BLEND_ADD is ONE, ONE
            // and therefore makes RGB stored under transparent DDS texels
            // visible as opaque white mesh faces.
            state |= BGFX_STATE_BLEND_FUNC(
                BGFX_STATE_BLEND_SRC_ALPHA,
                BGFX_STATE_BLEND_ONE);
        }
        else if (pipeline.alphaBlend ||
                 pipeline.blendMode ==
                     PipelineState::BlendMode::Alpha)
            state |= BGFX_STATE_BLEND_ALPHA;
        if (pipeline.multisampling)
            state |= BGFX_STATE_MSAA;

        bgfx::setTransform(transform.matrix.data());
        auto samplerFlags = [&](
            MaterialState::TextureFilter filter,
            MaterialState::TextureAddress address) {
            const bool inheritedFilter =
                filter == MaterialState::TextureFilter::Inherited;
            std::uint32_t flags =
                inheritedFilter ? inherited_filter_flags_ : 0U;
            switch (filter)
            {
            case MaterialState::TextureFilter::Point:
                flags |= static_cast<std::uint32_t>(
                    BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT |
                    BGFX_SAMPLER_MIP_POINT);
                break;
            case MaterialState::TextureFilter::Linear:
                // No bgfx filter flags means source D3DTEXF_LINEAR for
                // minification, magnification and mip interpolation.
                break;
            case MaterialState::TextureFilter::Inherited:
                break;
            }
            switch (address)
            {
            case MaterialState::TextureAddress::Clamp:
                flags |= BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
                break;
            case MaterialState::TextureAddress::Mirror:
                flags |= BGFX_SAMPLER_U_MIRROR | BGFX_SAMPLER_V_MIRROR;
                break;
            case MaterialState::TextureAddress::Wrap:
            case MaterialState::TextureAddress::Inherited:
                break;
            }
            return flags;
        };
        bgfx::setTexture(
            0, texture_sampler_, bgfx::TextureHandle{texture.value},
            samplerFlags(material.textureFilter,
                         material.textureAddress));
        const auto reflectionTexture =
            valid(pass_state_.reflectionTexture)
                ? pass_state_.reflectionTexture
                : texture;
        const auto shadowTexture =
            valid(pass_state_.shadowTexture)
                ? pass_state_.shadowTexture
                : texture;
        const auto shadowTextureFar =
            valid(pass_state_.shadowTextureFar)
                ? pass_state_.shadowTextureFar
                : shadowTexture;
        bgfx::setTexture(
            1, reflection_sampler_,
            bgfx::TextureHandle{reflectionTexture.value},
            samplerFlags(material.reflectionTextureFilter,
                         material.reflectionTextureAddress));
        bgfx::setTexture(
            2, shadow_sampler_,
            bgfx::TextureHandle{shadowTexture.value});
        bgfx::setTexture(
            5, shadow_far_sampler_,
            bgfx::TextureHandle{shadowTextureFar.value});
        const auto environmentTexture =
            valid(material.environmentTexture)
                ? material.environmentTexture
                : (valid(pass_state_.environmentTexture)
                       ? pass_state_.environmentTexture
                       : texture);
        const auto normalTexture =
            valid(material.normalTexture)
                ? material.normalTexture
                : texture;
        bgfx::setTexture(
            3, environment_sampler_,
            bgfx::TextureHandle{environmentTexture.value});
        bgfx::setTexture(
            4, normal_sampler_,
            bgfx::TextureHandle{normalTexture.value});
        bgfx::setUniform(scene_light_direction_,
                         scene_lighting_.lightDirection.data());
        bgfx::setUniform(scene_sun_position_,
                         scene_lighting_.sunPosition.data());
        bgfx::setUniform(
            scene_lamp_positions_,
            scene_lighting_.lampPositions.front().data(),
            SceneLighting::maximumSpotLights);
        bgfx::setUniform(
            scene_lamp_directions_,
            scene_lighting_.lampDirections.front().data(),
            SceneLighting::maximumSpotLights);
        bgfx::setUniform(
            scene_lamp_colors_,
            scene_lighting_.lampColors.front().data(),
            SceneLighting::maximumSpotLights);
        bgfx::setUniform(
            scene_lamp_cones_,
            scene_lighting_.lampCones.front().data(),
            SceneLighting::maximumSpotLights);
        bgfx::setUniform(scene_ambient_, scene_lighting_.ambient.data());
        bgfx::setUniform(scene_fog_, scene_lighting_.fogColor.data());
        bgfx::setUniform(scene_camera_,
                         scene_lighting_.cameraPosition.data());
        const std::array<float, 4> materialParameters{
            material.alphaReference, material.emissive,
            material.specular, material.shininess};
        const std::array<float, 4> materialOptions{
            material.ignoreFog ? 1.0F : 0.0F,
            material.reflectionStrength,
            pass_state_.shadowsEnabled && material.receivesShadow
                ? pass_state_.shadowStrength
                : 0.0F,
            pass_state_.clipPlaneEnabled ? 1.0F : 0.0F};
        bgfx::setUniform(material_color_, material.color.data());
        bgfx::setUniform(material_mapping_color_,
                         material.mappingColor.data());
        bgfx::setUniform(material_parameters_,
                         materialParameters.data());
        bgfx::setUniform(material_options_, materialOptions.data());
        bgfx::setUniform(texture_transform_,
                         material.textureTransform.data());
        bgfx::setUniform(clip_plane_,
                         pass_state_.clipPlane.data());
        bgfx::setUniform(reflection_view_projection_,
                         pass_state_.reflectionViewProjection.data());
        bgfx::setUniform(shadow_view_projection_,
                         pass_state_.shadowViewProjection.data());
        bgfx::setUniform(shadow_view_projection_far_,
                         pass_state_.shadowViewProjectionFar.data());
        const std::array<float, 4> shadowParameters{
            pass_state_.shadowSplitDistance,
            pass_state_.shadowMapSize,
            pass_state_.shadowDepthBias,
            pass_state_.spotShadows ? 1.0F : 0.0F};
        bgfx::setUniform(shadow_parameters_, shadowParameters.data());
        bgfx::setUniform(post_parameters_,
                         material.postParameters.data());
        bgfx::setState(state);
        bgfx::submit(current_view_, bgfx::ProgramHandle{shader.value});
        ++telemetry_.drawCount[
            static_cast<std::size_t>(current_pass_)];
        const bool sceneGeometry =
            (current_pass_ >= RenderPass::EnvironmentPositiveX &&
             current_pass_ <= RenderPass::EnvironmentNegativeZ) ||
            current_pass_ == RenderPass::Reflection ||
            current_pass_ == RenderPass::Scene ||
            current_pass_ == RenderPass::Refraction;
        if (sceneGeometry)
        {
            const int mode = std::clamp(
                static_cast<int>(std::lround(material.postParameters[3])),
                0, 6);
            ++telemetry_.lightingDrawCount[
                static_cast<std::size_t>(mode)];
            if (mode == 3 && valid(pass_state_.environmentTexture))
                ++telemetry_.environmentMappedDrawCount;
            if (mode == 4 && valid(material.normalTexture))
                ++telemetry_.normalMappedDrawCount;
        }
    }

    struct TargetInfo
    {
        std::uint16_t width = 0;
        std::uint16_t height = 0;
    };
    struct CubeTargetInfo
    {
        bgfx::TextureHandle colorTexture = BGFX_INVALID_HANDLE;
        std::array<bgfx::TextureHandle, 6> depthTextures{
            bgfx::TextureHandle{bgfx::kInvalidHandle},
            bgfx::TextureHandle{bgfx::kInvalidHandle},
            bgfx::TextureHandle{bgfx::kInvalidHandle},
            bgfx::TextureHandle{bgfx::kInvalidHandle},
            bgfx::TextureHandle{bgfx::kInvalidHandle},
            bgfx::TextureHandle{bgfx::kInvalidHandle}};
    };

    bool initialized_ = false;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::uint32_t reset_flags_ = BGFX_RESET_NONE;
    std::uint32_t inherited_filter_flags_ = 0U;
    bgfx::ViewId current_view_ = scene_view;
    SceneLighting scene_lighting_;
    RenderPassState pass_state_;
    RenderPass current_pass_ = RenderPass::Scene;
    RenderTelemetry telemetry_;
    std::unordered_map<std::uint16_t, TargetInfo> render_targets_;
    std::unordered_map<std::uint16_t, CubeTargetInfo> cube_targets_;
    bgfx::UniformHandle scene_light_direction_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle scene_sun_position_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle scene_lamp_positions_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle scene_lamp_directions_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle scene_lamp_colors_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle scene_lamp_cones_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle scene_ambient_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle scene_fog_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle scene_camera_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle material_color_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle material_mapping_color_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle material_parameters_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle material_options_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle texture_transform_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle clip_plane_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle reflection_view_projection_ =
        BGFX_INVALID_HANDLE;
    bgfx::UniformHandle shadow_view_projection_ =
        BGFX_INVALID_HANDLE;
    bgfx::UniformHandle shadow_view_projection_far_ =
        BGFX_INVALID_HANDLE;
    bgfx::UniformHandle shadow_parameters_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle post_parameters_ = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout color_vertex_layout_;
    bgfx::VertexLayout static_vertex_layout_;
    bgfx::UniformHandle texture_sampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle reflection_sampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle shadow_sampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle shadow_far_sampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle environment_sampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle normal_sampler_ = BGFX_INVALID_HANDLE;
};

} // namespace

std::unique_ptr<GraphicsDevice> createBgfxGraphicsDevice()
{
    return std::make_unique<BgfxGraphicsDevice>();
}

} // namespace r3d::renderer
