#include "OriginalResourceManager.h"

#include "OriginalMainMenu.h"
#include "OriginalResourceCatalog.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <vector>

namespace rrr3d::race
{
namespace
{

constexpr std::string_view whiteResourceKey = "<source-white>";
constexpr unsigned boldFontWeight = 700U;
constexpr std::array<std::string_view, 6> sourceWorldTypes{
    "wtWorld1", "wtWorld2", "wtWorld3", "wtWorld4", "wtWorld5",
    "wtWorld6"};

bool valid(r3d::renderer::Mesh value) noexcept
{
    return value.vertices.value != r3d::renderer::invalid_resource &&
           value.indices.value != r3d::renderer::invalid_resource;
}

bool valid(r3d::renderer::Texture value) noexcept
{
    return value.value != r3d::renderer::invalid_resource;
}

int resourceWorldTag(std::string_view sourceName) noexcept
{
    std::string path(sourceName);
    std::transform(
        path.begin(), path.end(), path.begin(), [](unsigned char value) {
            if (value == '\\')
                return static_cast<char>('/');
            return static_cast<char>(std::tolower(value));
        });
    for (int world = 0; world < 6; ++world)
    {
        const auto segment =
            "/world" + std::to_string(world + 1) + "/";
        const auto prefix = segment.substr(1);
        if (path.find(segment) != std::string::npos ||
            path.starts_with(prefix))
        {
            return world;
        }
    }
    return -1;
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
    const auto addFont = [&](std::string name, int height,
                             unsigned weight) {
        auto key = name;
        textFonts_.emplace(
            std::move(key),
            TextFontResource{
                std::move(name), height, weight, false,
                fontCharset_, "Verdana"});
    };
    // ResourceManager::LoadGUI creates this exact TextFontLib catalog.
    addFont("Header", 44, 0U);
    addFont("Item", 32, 0U);
    addFont("Small", 24, 0U);
    addFont("VerySmall", 18, boldFontWeight);
    addFont("VerySmallThink", 18, 0U);
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
    // Complex*Lib names records before their source files are opened.  Keep a
    // case-insensitive logical key here too: the shipped Windows catalog even
    // contains one unused GUI declaration whose image is absent.  Actual
    // path validation still happens when a renderer/audio payload is asked
    // for and ResourceFileSystem opens the source.
    std::string key(sourceName);
    std::transform(
        key.begin(), key.end(), key.begin(), [](unsigned char value) {
            if (value == '\\')
                return '/';
            return static_cast<char>(std::tolower(value));
        });
    while (key.find("//") != std::string::npos)
        key.erase(key.find("//"), 1U);
    while (key.starts_with("./"))
        key.erase(0U, 2U);
    return key;
}

const OriginalResourceManager::MeshResource&
OriginalResourceManager::GetMesh(std::string_view sourceName)
{
    ++requests_;
    const auto key = ResolveKey(sourceName);
    auto [record, created] = meshes_.try_emplace(
        key, MeshResource{
                 std::string(sourceName), {}, {},
                 resourceWorldTag(sourceName)});
    if (!created)
    {
        ++cacheHits_;
        if (valid(record->second.mesh))
            return record->second;
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

    record->second.source = std::move(source);
    record->second.mesh = mesh;
    managedMeshes_.insert(mesh.vertices.value);
    return record->second;
}

const OriginalResourceManager::TextureResource&
OriginalResourceManager::GetTexture(std::string_view sourceName)
{
    ++requests_;
    const auto key = ResolveKey(sourceName);
    auto [record, created] = textures_.try_emplace(
        key, TextureResource{
                 std::string(sourceName), {}, 0U, 0U,
                 resourceWorldTag(sourceName)});
    if (!created)
    {
        ++cacheHits_;
        if (valid(record->second.texture))
            return record->second;
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

    record->second.texture = texture;
    record->second.width = width;
    record->second.height = height;
    managedTextures_.insert(texture.value);
    return record->second;
}

const OriginalResourceManager::TextureResource&
OriginalResourceManager::GetTexture(
    const r3d::game::mainmenu2::Image& sourceImage)
{
    ++requests_;
    const auto key = ResolveKey(sourceImage.virtualPath);
    auto [record, created] = textures_.try_emplace(
        key, TextureResource{
                 sourceImage.virtualPath, {}, sourceImage.width,
                 sourceImage.height,
                 resourceWorldTag(sourceImage.virtualPath)});
    if (!created)
    {
        ++cacheHits_;
        if (valid(record->second.texture))
            return record->second;
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
    record->second.texture = texture;
    record->second.width = sourceImage.width;
    record->second.height = sourceImage.height;
    managedTextures_.insert(texture.value);
    return record->second;
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
        key, TextureResource{key, texture, 1U, 1U, -1});
    static_cast<void>(created);
    managedTextures_.insert(texture.value);
    return inserted->second;
}

const OriginalResourceManager::TextFontResource &OriginalResourceManager::GetTextFont(std::string_view name) const
{
    const auto found = textFonts_.find(std::string(name));
    if (found == textFonts_.end())
        throw r3d::resource::ResourceError("TextFont" + std::string(name) + " does not exist");
    return found->second;
}

OriginalResourceManager::TextFontResource OriginalResourceManager::ResolveTextFont(float height, bool bold) const
{
    const auto roundedHeight = static_cast<int>(height + 0.5F);
    for (const auto &[name, font] : textFonts_)
    {
        static_cast<void>(name);
        if (font.height == roundedHeight && font.bold() == bold)
            return font;
    }

    // A small number of backend diagnostics use non-library point sizes.
    // Preserve those sizes while inheriting the active source charset/face;
    // all game widgets resolve to one of the five records above.
    return {"<dynamic>", roundedHeight, bold ? boldFontWeight : 0U, false, fontCharset_, "Verdana"};
}

void OriginalResourceManager::SetFontCharset(r3d::game::originalgamedata::LanguageCharset value) noexcept
{
    if (fontCharset_ == value)
        return;
    fontCharset_ = value;
    for (auto &[name, font] : textFonts_)
    {
        static_cast<void>(name);
        font.charset = value;
    }
}

r3d::game::originalgamedata::LanguageCharset OriginalResourceManager::GetFontCharset() const noexcept
{
    return fontCharset_;
}

const r3d::game::originalrace::MaterialDefinition&
OriginalResourceManager::RegisterMaterial(
    const r3d::game::originalrace::MaterialDefinition& material)
{
    if (material.record.empty())
        throw r3d::resource::ResourceError(
            "ComplexMatLib cannot register an unnamed LibMaterial");
    if (const auto found = materials_.find(material.record);
        found != materials_.end())
    {
        return found->second;
    }
    return materials_.emplace(material.record, material).first->second;
}

const r3d::game::originalrace::MaterialDefinition&
OriginalResourceManager::GetMaterial(std::string_view name) const
{
    const auto found = materials_.find(std::string(name));
    if (found == materials_.end())
        throw r3d::resource::ResourceError(
            "LibMaterial" + std::string(name) + " does not exist");
    return found->second;
}

void OriginalResourceManager::Load()
{
    if (loaded_)
        return;
    if (resources_ == nullptr)
        throw r3d::resource::ResourceError(
            "Original ResourceManager is shut down");
    const auto registerMesh = [this](
                                  const auto& declaration) {
        const auto key = ResolveKey(declaration.path);
        auto [record, created] = meshes_.try_emplace(
            key,
            MeshResource{
                std::string(declaration.path), {}, {},
                declaration.worldType,
                declaration.buildTangentSpace,
                declaration.loadData,
                declaration.initializeVertexBuffer,
                declaration.loadDataOnWorldLoad,
                declaration.initializeVertexBufferOnWorldLoad});
        if (!created)
        {
            const auto& mesh = record->second;
            if (mesh.tag != declaration.worldType ||
                mesh.buildTangentSpace != declaration.buildTangentSpace ||
                mesh.loadData != declaration.loadData ||
                mesh.initializeVertexBuffer !=
                    declaration.initializeVertexBuffer ||
                mesh.loadDataOnWorldLoad !=
                    declaration.loadDataOnWorldLoad ||
                mesh.initializeVertexBufferOnWorldLoad !=
                    declaration.initializeVertexBufferOnWorldLoad)
            {
                throw r3d::resource::ResourceError(
                    "Conflicting source ComplexMesh declaration " +
                    std::string(declaration.path));
            }
        }
    };
    const auto registerImage = [this](
                                   const auto& declaration) {
        const auto key = ResolveKey(declaration.path);
        auto [record, created] = textures_.try_emplace(
            key,
            TextureResource{
                std::string(declaration.path), {}, 0U, 0U,
                declaration.worldType, declaration.levelCount,
                declaration.initializeTexture2D,
                declaration.initializeCubeTexture,
                declaration.initializeTexture2DOnWorldLoad,
                declaration.initializeCubeTextureOnWorldLoad,
                declaration.gui});
        if (!created)
        {
            const auto& image = record->second;
            if (image.tag != declaration.worldType ||
                image.levelCount != declaration.levelCount ||
                image.initializeTexture2D !=
                    declaration.initializeTexture2D ||
                image.initializeCubeTexture !=
                    declaration.initializeCubeTexture ||
                image.initializeTexture2DOnWorldLoad !=
                    declaration.initializeTexture2DOnWorldLoad ||
                image.initializeCubeTextureOnWorldLoad !=
                    declaration.initializeCubeTextureOnWorldLoad ||
                image.gui != declaration.gui)
            {
                throw r3d::resource::ResourceError(
                    "Conflicting source ComplexImage declaration " +
                    std::string(declaration.path));
            }
        }
    };
    for (const auto& mesh :
         r3d::game::originalresources::originalMeshResourceCatalog())
    {
        registerMesh(mesh);
    }
    for (const auto& image :
         r3d::game::originalresources::originalImageResourceCatalog())
    {
        registerImage(image);
    }
    for (const auto& material :
         r3d::game::originalrace::loadOriginalMaterialCatalog(*resources_))
    {
        RegisterMaterial(material);
    }
    loaded_ = true;
}

void OriginalResourceManager::LoadWorld(int worldType)
{
    if (worldType < 0 || worldType > worldTypeCount)
        throw r3d::resource::ResourceError(
            "ResourceManager received an invalid world type");
    if (worldType_ == worldType)
        return;
    if (device_ == nullptr)
        throw r3d::resource::ResourceError(
            "Original ResourceManager is shut down");

    for (auto& [key, resource] : meshes_)
    {
        static_cast<void>(key);
        if (resource.tag != worldType_ || !valid(resource.mesh))
            continue;
        managedMeshes_.erase(resource.mesh.vertices.value);
        device_->destroy(resource.mesh);
        resource.mesh = {};
        resource.source.reset();
    }
    for (auto& [key, resource] : textures_)
    {
        static_cast<void>(key);
        if (resource.tag != worldType_ || !valid(resource.texture))
            continue;
        managedTextures_.erase(resource.texture.value);
        device_->destroy(resource.texture);
        resource.texture = {};
        resource.width = 0U;
        resource.height = 0U;
    }
    // D3D9 eagerly initialized the records of the new tag here. The Metal
    // adapter materializes them on the first renderer request, before a
    // frame is submitted, while retaining the same Complex* record.
    worldType_ = worldType;
}

void OriginalResourceManager::LoadWorld(std::string_view worldType)
{
    const auto found = std::find(
        sourceWorldTypes.begin(), sourceWorldTypes.end(), worldType);
    if (found == sourceWorldTypes.end())
        throw r3d::resource::ResourceError(
            "ResourceManager received unknown source world type " +
            std::string(worldType));
    LoadWorld(static_cast<int>(
        std::distance(sourceWorldTypes.begin(), found)));
}

int OriginalResourceManager::GetWorldType() const noexcept
{
    return worldType_;
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
    // SoundBackend consumes a physical filesystem path, whereas Mesh/Image
    // retain their logical Complex*Lib identity until materialization.
    const auto sound =
        audio_->loadOgg(resources_->resolve(sourceName), info, error);
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

std::size_t OriginalResourceManager::GetLoadedMeshCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        meshes_.begin(), meshes_.end(), [](const auto& item) {
            return valid(item.second.mesh);
        }));
}

std::size_t OriginalResourceManager::GetLoadedTextureCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        textures_.begin(), textures_.end(), [](const auto& item) {
            return valid(item.second.texture);
        }));
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

std::size_t OriginalResourceManager::GetTextFontCount() const noexcept
{
    return textFonts_.size();
}

std::size_t OriginalResourceManager::GetMaterialCount() const noexcept
{
    return materials_.size();
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
    textFonts_.clear();
    materials_.clear();
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
    worldType_ = worldTypeCount;
    loaded_ = false;
}

} // namespace rrr3d::race
