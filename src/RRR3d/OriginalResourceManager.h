#pragma once

#include "renderer/Renderer.h"
#include "resource/R3DMeshAsset.h"
#include "resource/ResourceFileSystem.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace rrr3d::race
{

// Backend implementation of the source ResourceManager identity libraries.
// Logical records remain in game code; this owner resolves each physical
// mesh/image exactly once and retains its decoded and Metal/bgfx identity.
class OriginalResourceManager
{
public:
    struct MeshResource
    {
        std::string name;
        std::shared_ptr<const r3d::resource::R3DMeshAsset> source;
        r3d::renderer::Mesh mesh;
    };

    struct TextureResource
    {
        std::string name;
        r3d::renderer::Texture texture;
        std::uint16_t width = 0U;
        std::uint16_t height = 0U;
    };

    OriginalResourceManager(
        r3d::renderer::GraphicsDevice& device,
        const r3d::resource::ResourceFileSystem& resources) noexcept;
    ~OriginalResourceManager() = default;

    OriginalResourceManager(const OriginalResourceManager&) = delete;
    OriginalResourceManager& operator=(
        const OriginalResourceManager&) = delete;

    const MeshResource& GetMesh(std::string_view sourceName);
    const TextureResource& GetTexture(std::string_view sourceName);
    const TextureResource& GetWhiteTexture();
    const r3d::resource::ResourceFileSystem& GetFileSystem() const;

    std::size_t GetMeshCount() const noexcept;
    std::size_t GetTextureCount() const noexcept;
    std::size_t GetRequestCount() const noexcept;
    std::size_t GetCacheHitCount() const noexcept;
    void Shutdown() noexcept;

private:
    std::string ResolveKey(std::string_view sourceName) const;

    r3d::renderer::GraphicsDevice* device_ = nullptr;
    const r3d::resource::ResourceFileSystem* resources_ = nullptr;
    std::unordered_map<std::string, MeshResource> meshes_;
    std::unordered_map<std::string, TextureResource> textures_;
    std::size_t requests_ = 0U;
    std::size_t cacheHits_ = 0U;
};

} // namespace rrr3d::race
