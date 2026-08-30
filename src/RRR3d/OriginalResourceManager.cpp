#include "OriginalResourceManager.h"

#include "OriginalMainMenu.h"

#include <algorithm>
#include <cctype>
#include <vector>

namespace rrr3d::race
{
namespace
{

constexpr std::string_view whiteResourceKey = "<source-white>";

bool valid(r3d::renderer::Mesh value) noexcept
{
    return value.vertices.value != r3d::renderer::invalid_resource &&
           value.indices.value != r3d::renderer::invalid_resource;
}

bool valid(r3d::renderer::Texture value) noexcept
{
    return value.value != r3d::renderer::invalid_resource;
}

std::vector<r3d::renderer::StaticMeshVertex> makeVertices(
    const r3d::resource::R3DMeshAsset& mesh)
{
    std::vector<r3d::renderer::StaticMeshVertex> result;
    result.reserve(mesh.vertices.size());
    for (const auto& vertex : mesh.vertices)
    {
        result.push_back({
            vertex.position[0], vertex.position[1], vertex.position[2],
            vertex.normal[0], vertex.normal[1], vertex.normal[2],
            vertex.texcoord[0], vertex.texcoord[1], vertex.tangent[0],
            vertex.tangent[1], vertex.tangent[2], vertex.bitangent[0],
            vertex.bitangent[1], vertex.bitangent[2]});
    }
    return result;
}

} // namespace

OriginalResourceManager::OriginalResourceManager(
    r3d::renderer::GraphicsDevice& device,
    const r3d::resource::ResourceFileSystem& resources) noexcept
    : device_(&device), resources_(&resources)
{
}

OriginalResourceManager::~OriginalResourceManager()
{
    Shutdown();
}

std::string OriginalResourceManager::ResolveKey(
    std::string_view sourceName) const
{
    if (resources_ == nullptr)
        throw r3d::resource::ResourceError(
            "Original ResourceManager is shut down");
    return resources_->resolve(sourceName).generic_string();
}

const OriginalResourceManager::MeshResource&
OriginalResourceManager::GetMesh(std::string_view sourceName)
{
    ++requests_;
    const auto key = ResolveKey(sourceName);
    if (const auto found = meshes_.find(key); found != meshes_.end())
    {
        ++cacheHits_;
        return found->second;
    }

    auto source = std::make_shared<r3d::resource::R3DMeshAsset>(
        r3d::resource::loadR3DMeshAsset(*resources_, sourceName));
    const auto vertices = makeVertices(*source);
    const auto mesh = device_->createMesh(
        vertices.data(), vertices.size(), source->indices.data(),
        source->indices.size());
    if (!valid(mesh))
        throw r3d::resource::ResourceError(
            "Unable to upload original mesh " + std::string(sourceName));

    auto [inserted, created] = meshes_.emplace(
        key, MeshResource{std::string(sourceName), std::move(source), mesh});
    static_cast<void>(created);
    managedMeshes_.insert(mesh.vertices.value);
    return inserted->second;
}

const OriginalResourceManager::TextureResource&
OriginalResourceManager::GetTexture(std::string_view sourceName)
{
    ++requests_;
    const auto key = ResolveKey(sourceName);
    if (const auto found = textures_.find(key); found != textures_.end())
    {
        ++cacheHits_;
        return found->second;
    }

    std::string lower(sourceName);
    std::transform(
        lower.begin(), lower.end(), lower.begin(),
        [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
    r3d::renderer::Texture texture;
    std::uint16_t width = 0U;
    std::uint16_t height = 0U;
    if (lower.ends_with(".dds"))
    {
        // ComplexImage kept CubeImage/TexCube separate from Image/Tex2D.
        // Passing the original DDS container to bgfx preserves that shape;
        // decoding a cube as a 2D menu image rejects valid sky resources.
        try
        {
            const auto image = r3d::game::mainmenu2::loadOriginalImage(
                *resources_, std::string(sourceName));
            width = image.width;
            height = image.height;
            texture = device_->createTextureContainer(
                image.bytes.data(), image.bytes.size(), image.virtualPath);
        }
        catch (const r3d::resource::ResourceError&)
        {
            const auto bytes = resources_->readBinary(sourceName);
            texture = device_->createTextureContainer(
                bytes.data(), bytes.size(), sourceName);
        }
    }
    else
    {
        const auto image = r3d::game::mainmenu2::loadOriginalImage(
            *resources_, std::string(sourceName));
        width = image.width;
        height = image.height;
        texture =
            image.storage ==
                    r3d::game::mainmenu2::ImageStorage::EncodedContainer
                ? device_->createTextureContainer(
                      image.bytes.data(), image.bytes.size(),
                      image.virtualPath)
                : device_->createTextureRgba8(
                      image.width, image.height, image.bytes.data(),
                      image.bytes.size());
    }
    if (!valid(texture))
        throw r3d::resource::ResourceError(
            "Unable to upload original texture " +
            std::string(sourceName));

    auto [inserted, created] = textures_.emplace(
        key, TextureResource{
                 std::string(sourceName), texture, width, height});
    static_cast<void>(created);
    managedTextures_.insert(texture.value);
    return inserted->second;
}

const OriginalResourceManager::TextureResource&
OriginalResourceManager::GetTexture(
    const r3d::game::mainmenu2::Image& sourceImage)
{
    ++requests_;
    const auto key = ResolveKey(sourceImage.virtualPath);
    if (const auto found = textures_.find(key); found != textures_.end())
    {
        ++cacheHits_;
        return found->second;
    }

    const auto texture =
        sourceImage.storage ==
                r3d::game::mainmenu2::ImageStorage::EncodedContainer
            ? device_->createTextureContainer(
                  sourceImage.bytes.data(), sourceImage.bytes.size(),
                  sourceImage.virtualPath)
            : device_->createTextureRgba8(
                  sourceImage.width, sourceImage.height,
                  sourceImage.bytes.data(), sourceImage.bytes.size());
    if (!valid(texture))
        throw r3d::resource::ResourceError(
            "Unable to upload original texture " +
            sourceImage.virtualPath);
    auto [inserted, created] = textures_.emplace(
        key, TextureResource{
                 sourceImage.virtualPath, texture,
                 sourceImage.width, sourceImage.height});
    static_cast<void>(created);
    managedTextures_.insert(texture.value);
    return inserted->second;
}

const OriginalResourceManager::TextureResource&
OriginalResourceManager::GetWhiteTexture()
{
    ++requests_;
    const std::string key(whiteResourceKey);
    if (const auto found = textures_.find(key); found != textures_.end())
    {
        ++cacheHits_;
        return found->second;
    }
    constexpr std::uint8_t white[]{255U, 255U, 255U, 255U};
    const auto texture = device_->createTextureRgba8(
        1U, 1U, white, sizeof(white));
    if (!valid(texture))
        throw r3d::resource::ResourceError(
            "Unable to create source white texture");
    auto [inserted, created] = textures_.emplace(
        key, TextureResource{key, texture, 1U, 1U});
    static_cast<void>(created);
    managedTextures_.insert(texture.value);
    return inserted->second;
}

void OriginalResourceManager::AttachAudio(
    r3d::audio::AudioBackend& audio) noexcept
{
    if (audio_ == &audio)
        return;
    ShutdownSounds();
    audio_ = &audio;
}

const OriginalResourceManager::SoundResource&
OriginalResourceManager::GetSound(
    std::string_view sourceName, float volume)
{
    ++soundRequests_;
    const auto key = ResolveKey(sourceName);
    if (const auto found = sounds_.find(key); found != sounds_.end())
    {
        ++soundCacheHits_;
        // ResourceManager::LoadSound calls SetVolume after SoundLib::Find,
        // so the shared resource always receives the latest descriptor.
        found->second.volume = volume;
        return found->second;
    }
    if (audio_ == nullptr)
        throw r3d::resource::ResourceError(
            "Original ResourceManager SoundLib is not attached");

    r3d::audio::SoundInfo info;
    std::string error;
    const auto sound = audio_->loadOgg(key, info, error);
    if (sound == r3d::audio::invalidSound)
    {
        throw r3d::resource::ResourceError(
            "Unable to load original sound " + std::string(sourceName) +
            (error.empty() ? std::string{} : ": " + error));
    }
    auto [inserted, created] = sounds_.emplace(
        key, SoundResource{
                 std::string(sourceName), sound, info, volume});
    static_cast<void>(created);
    return inserted->second;
}

const r3d::resource::ResourceFileSystem&
OriginalResourceManager::GetFileSystem() const
{
    if (resources_ == nullptr)
        throw r3d::resource::ResourceError(
            "Original ResourceManager is shut down");
    return *resources_;
}

std::size_t OriginalResourceManager::GetMeshCount() const noexcept
{
    return meshes_.size();
}
std::size_t OriginalResourceManager::GetTextureCount() const noexcept
{
    return textures_.size();
}
std::size_t OriginalResourceManager::GetRequestCount() const noexcept
{
    return requests_;
}
std::size_t OriginalResourceManager::GetCacheHitCount() const noexcept
{
    return cacheHits_;
}

std::size_t OriginalResourceManager::GetSoundCount() const noexcept
{
    return sounds_.size();
}

std::size_t OriginalResourceManager::GetSoundRequestCount() const noexcept
{
    return soundRequests_;
}

std::size_t OriginalResourceManager::GetSoundCacheHitCount() const noexcept
{
    return soundCacheHits_;
}

bool OriginalResourceManager::Owns(
    r3d::renderer::Mesh resource) const noexcept
{
    return managedMeshes_.contains(resource.vertices.value);
}

bool OriginalResourceManager::Owns(
    r3d::renderer::Texture resource) const noexcept
{
    return managedTextures_.contains(resource.value);
}

void OriginalResourceManager::Release(
    r3d::renderer::Shader resource) noexcept
{
    if (device_ != nullptr)
        device_->destroy(resource);
}

void OriginalResourceManager::Release(
    r3d::renderer::Mesh resource) noexcept
{
    if (device_ != nullptr && !Owns(resource))
        device_->destroy(resource);
}

void OriginalResourceManager::Release(
    r3d::renderer::Texture resource) noexcept
{
    if (device_ != nullptr && !Owns(resource))
        device_->destroy(resource);
}

void OriginalResourceManager::Release(
    r3d::renderer::RenderTarget resource) noexcept
{
    if (device_ != nullptr)
        device_->destroy(resource);
}

void OriginalResourceManager::Release(
    r3d::renderer::CubeRenderTarget resource) noexcept
{
    if (device_ != nullptr)
        device_->destroy(resource);
}

void OriginalResourceManager::ShutdownSounds() noexcept
{
    if (audio_ != nullptr)
    {
        for (const auto& [key, resource] : sounds_)
        {
            static_cast<void>(key);
            if (resource.sound != r3d::audio::invalidSound)
                audio_->unloadSound(resource.sound);
        }
    }
    sounds_.clear();
    audio_ = nullptr;
}

void OriginalResourceManager::Shutdown() noexcept
{
    ShutdownSounds();
    if (device_ == nullptr)
        return;
    for (const auto& [key, resource] : textures_)
    {
        static_cast<void>(key);
        if (valid(resource.texture))
            device_->destroy(resource.texture);
    }
    for (const auto& [key, resource] : meshes_)
    {
        static_cast<void>(key);
        if (valid(resource.mesh))
            device_->destroy(resource.mesh);
    }
    textures_.clear();
    meshes_.clear();
    managedTextures_.clear();
    managedMeshes_.clear();
    device_ = nullptr;
    resources_ = nullptr;
}

} // namespace rrr3d::race
