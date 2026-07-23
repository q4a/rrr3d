#pragma once

#include "OriginalMainMenu.h"
#include "OriginalRaceSession.h"
#include "renderer/Renderer.h"

#include <string>

namespace r3d::resource
{
class ResourceFileSystem;
}

namespace rrr3d::race
{

class OriginalRaceHud
{
public:
    bool initialize(
        r3d::renderer::GraphicsDevice& device,
        const r3d::resource::ResourceFileSystem& resources,
        std::string& error);
    void shutdown(r3d::renderer::GraphicsDevice& device) noexcept;
    void update(r3d::renderer::GraphicsDevice& device,
                const r3d::game::originalrace::Race& race,
                const r3d::game::originalrace::OriginalRaceSession& session,
                const r3d::physics::VehicleState& vehicle);
    void draw(r3d::renderer::GraphicsDevice& device,
              r3d::renderer::Mesh quad,
              r3d::renderer::Shader shader) const;

private:
    struct ImageAsset
    {
        r3d::renderer::Texture texture;
        float width = 0.0F;
        float height = 0.0F;
    };

    struct TextAsset
    {
        r3d::renderer::Texture texture;
        float width = 0.0F;
        float height = 0.0F;
        std::string value;
    };

    bool loadImage(r3d::renderer::GraphicsDevice& device,
                   const r3d::resource::ResourceFileSystem& resources,
                   std::string path, ImageAsset& output,
                   std::string& error);
    void setText(r3d::renderer::GraphicsDevice& device,
                 TextAsset& output, std::string value, float pointSize,
                 bool bold,
                 r3d::game::mainmenu2::Rgba8 color);

    ImageAsset placeFrame_;
    ImageAsset lifeBack_;
    ImageAsset lifeBar_;
    ImageAsset lapBack_;
    TextAsset place_;
    TextAsset lap_;
    TextAsset telemetry_;
    TextAsset status_;
    float lifeFraction_ = 1.0F;
};

} // namespace rrr3d::race
