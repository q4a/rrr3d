#include "OriginalResourceManager.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{

class FakeGraphicsDevice final : public r3d::renderer::GraphicsDevice
{
public:
    bool initialize(const r3d::renderer::SwapChain&, std::string&) override
    {
        return true;
    }

    void resize(std::uint32_t, std::uint32_t) override {}
    void configureQuality(std::uint32_t, std::uint32_t) override {}

    r3d::renderer::Shader createShader(
        r3d::renderer::ShaderBinary, r3d::renderer::ShaderBinary,
        std::string_view) override
    {
        return {next_++};
    }

    r3d::renderer::Mesh createMesh(
        const void*, std::size_t, r3d::renderer::VertexLayout,
        const void*, std::size_t, r3d::renderer::IndexType) override
    {
        return {{next_++}, {next_++}};
    }

    r3d::renderer::Texture createTextureRgba8(
        std::uint16_t, std::uint16_t, const std::uint8_t*,
        std::size_t) override
    {
        return {next_++};
    }

    r3d::renderer::Texture createTextureContainer(
        const std::uint8_t*, std::size_t, std::string_view) override
    {
        return {next_++};
    }

    r3d::renderer::RenderTarget createRenderTarget(
        std::uint16_t, std::uint16_t, r3d::renderer::RenderTargetFormat,
        bool, std::string_view) override
    {
        return {next_++};
    }

    r3d::renderer::CubeRenderTarget createCubeRenderTarget(
        std::uint16_t, r3d::renderer::RenderTargetFormat, bool,
        std::string_view) override
    {
        r3d::renderer::CubeRenderTarget result;
        result.texture = {next_++};
        for (auto& face : result.faces)
            face = {next_++};
        return result;
    }

    r3d::renderer::Texture renderTargetTexture(
        r3d::renderer::RenderTarget target, std::uint8_t) const override
    {
        return {target.value};
    }

    void destroy(r3d::renderer::Shader shader) override
    {
        destroyedShaders.push_back(shader.value);
    }

    void destroy(r3d::renderer::Mesh mesh) override
    {
        destroyedMeshes.push_back(mesh.vertices.value);
    }

    void destroy(r3d::renderer::Texture texture) override
    {
        destroyedTextures.push_back(texture.value);
    }

    void destroy(r3d::renderer::RenderTarget target) override
    {
        destroyedTargets.push_back(target.value);
    }

    void destroy(r3d::renderer::CubeRenderTarget target) override
    {
        destroyedCubeTextures.push_back(target.texture.value);
    }

    void beginFrame(const r3d::renderer::Camera&, std::uint32_t) override {}
    void resetRenderTelemetry() noexcept override { telemetry_ = {}; }
    const r3d::renderer::RenderTelemetry& renderTelemetry() const noexcept override
    {
        return telemetry_;
    }
    void beginPass(
        r3d::renderer::RenderPass, r3d::renderer::RenderTarget,
        const r3d::renderer::Camera&, std::uint32_t, bool, bool) override {}
    void beginOverlay(const r3d::renderer::Camera&) override {}
    void setPassState(const r3d::renderer::RenderPassState&) override {}
    void setSceneLighting(const r3d::renderer::SceneLighting&) override {}
    void draw(
        r3d::renderer::Mesh, r3d::renderer::Shader,
        r3d::renderer::Texture, const r3d::renderer::Transform&,
        const r3d::renderer::PipelineState&, r3d::renderer::DrawRange,
        const r3d::renderer::MaterialState&) override {}
    void drawTransient(
        const r3d::renderer::StaticMeshVertex*, std::size_t,
        const std::uint32_t*, std::size_t, r3d::renderer::Shader,
        r3d::renderer::Texture, const r3d::renderer::Transform&,
        const r3d::renderer::PipelineState&,
        const r3d::renderer::MaterialState&) override {}
    void endFrame() override {}
    std::string_view backendName() const noexcept override { return "fake"; }
    bool usesHomogeneousDepth() const noexcept override { return false; }

    std::vector<std::uint16_t> destroyedShaders;
    std::vector<std::uint16_t> destroyedMeshes;
    std::vector<std::uint16_t> destroyedTextures;
    std::vector<std::uint16_t> destroyedTargets;
    std::vector<std::uint16_t> destroyedCubeTextures;

private:
    std::uint16_t next_ = 1U;
    r3d::renderer::RenderTelemetry telemetry_;
};

class FakeAudioBackend final : public r3d::audio::AudioBackend
{
public:
    bool initialize(std::string&) override { return true; }
    void shutdown() noexcept override {}

    r3d::audio::SoundHandle loadOgg(
        const std::filesystem::path& path, r3d::audio::SoundInfo& info,
        std::string& error) override
    {
        if (!std::filesystem::is_regular_file(path))
        {
            error = "missing fake sound";
            return r3d::audio::invalidSound;
        }
        info.sourceSampleRate = 44100;
        loaded.push_back(next_);
        return next_++;
    }

    bool unloadSound(r3d::audio::SoundHandle sound) noexcept override
    {
        unloaded.push_back(sound);
        return true;
    }

    r3d::audio::VoiceHandle play(
        r3d::audio::SoundHandle, const r3d::audio::PlayOptions&,
        std::string&) override
    {
        return 1U;
    }
    bool stop(r3d::audio::VoiceHandle) noexcept override { return true; }
    void stopAll() noexcept override {}
    bool setVoicePaused(r3d::audio::VoiceHandle, bool) noexcept override
    {
        return true;
    }
    bool setVoiceParameters(
        r3d::audio::VoiceHandle, float, float, float) noexcept override
    {
        return true;
    }
    bool isVoiceActive(r3d::audio::VoiceHandle) const noexcept override
    {
        return false;
    }
    std::uint64_t voicePositionFrames(
        r3d::audio::VoiceHandle) const noexcept override
    {
        return 0U;
    }
    void setPaused(bool paused) noexcept override { paused_ = paused; }
    bool paused() const noexcept override { return paused_; }
    void setMasterVolume(float volume) noexcept override
    {
        masterVolume_ = volume;
    }
    float masterVolume() const noexcept override { return masterVolume_; }
    void setBusVolume(r3d::audio::Bus, float) noexcept override {}
    float busVolume(r3d::audio::Bus) const noexcept override { return 1.0F; }
    void notifyPlaybackDeviceEvent(
        r3d::audio::PlaybackDeviceEvent, std::uint32_t) noexcept override {}
    std::string driverName() const override { return "fake"; }
    std::string outputDeviceName() const override { return "fake"; }
    r3d::audio::Statistics statistics() const noexcept override { return {}; }

    std::vector<r3d::audio::SoundHandle> loaded;
    std::vector<r3d::audio::SoundHandle> unloaded;

private:
    r3d::audio::SoundHandle next_ = 1U;
    bool paused_ = false;
    float masterVolume_ = 1.0F;
};

int fail(const char* message)
{
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main()
{
    const auto unique = std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count();
    const auto root = std::filesystem::temp_directory_path() /
                      ("rrr3d-resource-manager-" + std::to_string(unique));
    std::filesystem::create_directories(root);
    {
        std::ofstream image(root / "panel.rgba", std::ios::binary);
        image << "rgba";
        std::ofstream sound(root / "tone.ogg", std::ios::binary);
        sound << "ogg";
    }

    r3d::resource::ResourceFileSystem fileSystem(root);
    FakeGraphicsDevice device;
    FakeAudioBackend firstAudio;
    FakeAudioBackend secondAudio;

    {
        rrr3d::race::OriginalResourceManager resources(device, fileSystem);
        if (resources.GetTextFontCount() != 5U)
            return fail("TextFontLib does not contain five source fonts");
        const auto &header = resources.GetTextFont("Header");
        const auto &item = resources.GetTextFont("Item");
        const auto &small = resources.GetTextFont("Small");
        const auto &verySmall = resources.GetTextFont("VerySmall");
        const auto &verySmallThink = resources.GetTextFont("VerySmallThink");
        if (header.height != 44 || item.height != 32 || small.height != 24 || verySmall.height != 18 ||
            !verySmall.bold() || verySmallThink.height != 18 || verySmallThink.bold() || header.faceName != "Verdana")
            return fail("TextFontLib source descriptors differ");
        resources.SetFontCharset(r3d::game::originalgamedata::LanguageCharset::Russian);
        if (resources.GetFontCharset() != r3d::game::originalgamedata::LanguageCharset::Russian ||
            resources.GetTextFont("Header").charset != r3d::game::originalgamedata::LanguageCharset::Russian ||
            resources.ResolveTextFont(18.0F, true).name != "VerySmall" ||
            resources.ResolveTextFont(18.0F, false).name != "VerySmallThink" ||
            resources.ResolveTextFont(17.0F, false).name != "<dynamic>" ||
            resources.ResolveTextFont(17.0F, false).charset != r3d::game::originalgamedata::LanguageCharset::Russian)
            return fail("TextFontLib charset/lookup semantics differ");
        r3d::game::mainmenu2::Image image;
        image.virtualPath = "panel.rgba";
        image.width = 1U;
        image.height = 1U;
        image.bytes = {255U, 255U, 255U, 255U};

        const auto texture = resources.GetTexture(image).texture;
        const auto cachedTexture = resources.GetTexture(image).texture;
        if (texture.value != cachedTexture.value ||
            resources.GetTextureCount() != 1U ||
            resources.GetCacheHitCount() != 1U ||
            !resources.Owns(texture))
            return fail("ImageLib did not retain canonical GUI texture");

        resources.Release(texture);
        if (!device.destroyedTextures.empty())
            return fail("caller released ResourceManager-owned texture");
        resources.Release(r3d::renderer::Texture{500U});
        if (device.destroyedTextures != std::vector<std::uint16_t>{500U})
            return fail("foreign texture did not use backend release");

        resources.AttachAudio(firstAudio);
        const auto firstSound = resources.GetSound("tone.ogg", 0.5F).sound;
        const auto& cachedSound = resources.GetSound("tone.ogg", 1.75F);
        if (cachedSound.sound != firstSound || cachedSound.volume != 1.75F ||
            resources.GetSoundCount() != 1U ||
            resources.GetSoundCacheHitCount() != 1U)
            return fail("SoundLib cache/SetVolume semantics differ");

        resources.AttachAudio(secondAudio);
        if (firstAudio.unloaded !=
                std::vector<r3d::audio::SoundHandle>{firstSound} ||
            resources.GetSoundCount() != 0U)
            return fail("SoundLib did not unload before backend rebind");
        const auto secondSound = resources.GetSound("tone.ogg", 0.8F).sound;

        resources.Shutdown();
        if (secondAudio.unloaded !=
                std::vector<r3d::audio::SoundHandle>{secondSound} ||
            device.destroyedTextures !=
                std::vector<std::uint16_t>{500U, texture.value})
            return fail("ResourceManager shutdown ownership order differs");
        resources.Shutdown();
    }

    if (device.destroyedTextures.size() != 2U)
        return fail("ResourceManager destructor repeated texture release");

    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::cout << "original ResourceManager identity libraries passed\n";
    return 0;
}
