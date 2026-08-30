#pragma once

#include "renderer/Renderer.h"
#include "resource/R3DMeshAsset.h"
#include "resource/ResourceFileSystem.h"
#include "audio/AudioBackend.h"
#include "OriginalMainMenu.h"
#include "OriginalRace.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

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
        int tag = -1;
    };

    struct TextureResource
    {
        std::string name;
        r3d::renderer::Texture texture;
        std::uint16_t width = 0U;
        std::uint16_t height = 0U;
        int tag = -1;
    };

    struct SoundResource
    {
        std::string name;
        r3d::audio::SoundHandle sound = r3d::audio::invalidSound;
        r3d::audio::SoundInfo info;
        float volume = 1.0F;
    };

    struct TextFontResource
    {
        std::string name;
        int height = 0;
        unsigned weight = 0U;
        bool italic = false;
        r3d::game::originalgamedata::LanguageCharset charset = r3d::game::originalgamedata::LanguageCharset::Default;
        std::string faceName;

        bool bold() const noexcept
        {
            return weight >= 700U;
        }
    };

    OriginalResourceManager(
        r3d::renderer::GraphicsDevice& device,
        const r3d::resource::ResourceFileSystem& resources) noexcept;
    ~OriginalResourceManager();

    OriginalResourceManager(const OriginalResourceManager&) = delete;
    OriginalResourceManager& operator=(
        const OriginalResourceManager&) = delete;

    const MeshResource& GetMesh(std::string_view sourceName);
    const TextureResource& GetTexture(std::string_view sourceName);
    const TextureResource& GetTexture(
        const r3d::game::mainmenu2::Image& sourceImage);
    const TextureResource& GetWhiteTexture();
    const TextFontResource &GetTextFont(std::string_view name) const;
    TextFontResource ResolveTextFont(float height, bool bold) const;
    void SetFontCharset(r3d::game::originalgamedata::LanguageCharset value) noexcept;
    r3d::game::originalgamedata::LanguageCharset GetFontCharset() const noexcept;
    const r3d::game::originalrace::MaterialDefinition& RegisterMaterial(
        const r3d::game::originalrace::MaterialDefinition& material);
    const r3d::game::originalrace::MaterialDefinition& GetMaterial(
        std::string_view name) const;
    static constexpr int worldTypeCount = 6;
    void LoadWorld(int worldType);
    void LoadWorld(std::string_view worldType);
    int GetWorldType() const noexcept;
    void AttachAudio(r3d::audio::AudioBackend& audio) noexcept;
    const SoundResource& GetSound(
        std::string_view sourceName, float volume = 1.0F);
    const r3d::resource::ResourceFileSystem& GetFileSystem() const;

    std::size_t GetMeshCount() const noexcept;
    std::size_t GetTextureCount() const noexcept;
    std::size_t GetLoadedMeshCount() const noexcept;
    std::size_t GetLoadedTextureCount() const noexcept;
    std::size_t GetRequestCount() const noexcept;
    std::size_t GetCacheHitCount() const noexcept;
    std::size_t GetSoundCount() const noexcept;
    std::size_t GetTextFontCount() const noexcept;
    std::size_t GetMaterialCount() const noexcept;
    std::size_t GetSoundRequestCount() const noexcept;
    std::size_t GetSoundCacheHitCount() const noexcept;
    bool Owns(r3d::renderer::Mesh resource) const noexcept;
    bool Owns(r3d::renderer::Texture resource) const noexcept;
    void Release(r3d::renderer::Shader resource) noexcept;
    void Release(r3d::renderer::Mesh resource) noexcept;
    void Release(r3d::renderer::Texture resource) noexcept;
    void Release(r3d::renderer::RenderTarget resource) noexcept;
    void Release(r3d::renderer::CubeRenderTarget resource) noexcept;
    void ShutdownSounds() noexcept;
    void Shutdown() noexcept;

private:
    std::string ResolveKey(std::string_view sourceName) const;

    r3d::renderer::GraphicsDevice* device_ = nullptr;
    const r3d::resource::ResourceFileSystem* resources_ = nullptr;
    r3d::audio::AudioBackend* audio_ = nullptr;
    std::unordered_map<std::string, MeshResource> meshes_;
    std::unordered_map<std::string, TextureResource> textures_;
    std::unordered_map<std::string, SoundResource> sounds_;
    std::unordered_map<std::string, TextFontResource> textFonts_;
    std::unordered_map<
        std::string, r3d::game::originalrace::MaterialDefinition>
        materials_;
    std::unordered_set<std::uint32_t> managedMeshes_;
    std::unordered_set<std::uint32_t> managedTextures_;
    std::size_t requests_ = 0U;
    std::size_t cacheHits_ = 0U;
    std::size_t soundRequests_ = 0U;
    std::size_t soundCacheHits_ = 0U;
    r3d::game::originalgamedata::LanguageCharset fontCharset_ = r3d::game::originalgamedata::LanguageCharset::Default;
    int worldType_ = worldTypeCount;
};

} // namespace rrr3d::race
