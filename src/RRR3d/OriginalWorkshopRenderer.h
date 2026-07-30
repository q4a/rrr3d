#pragma once

#include "OriginalGarage.h"
#include "renderer/Renderer.h"
#include "resource/R3DMeshAsset.h"

#include <string>
#include <vector>

namespace r3d::resource
{
class ResourceFileSystem;
}

namespace rrr3d::race
{

// Portable implementation of gui::ViewPort3d/WorkshopFrame mesh boxes.  It
// consumes the mesh and texture references serialized in workshop.xml and
// applies Menu::GetIsoRot plus Context::DrawView3d's source fitting formula.
class OriginalWorkshopRenderer
{
public:
    bool initialize(
        r3d::renderer::GraphicsDevice& device,
        const r3d::resource::ResourceFileSystem& resources,
        const r3d::game::originalrace::OriginalGarageCatalog& catalog,
        std::string& error);
    void shutdown(r3d::renderer::GraphicsDevice& device) noexcept;

    void drawItem(
        r3d::renderer::GraphicsDevice& device,
        r3d::renderer::Shader shader,
        const r3d::game::originalrace::OriginalWorkshopItem& item,
        float centerX, float centerY, float width, float height,
        float rotationRadians,
        const r3d::renderer::PipelineState& pipeline) const;

private:
    struct Asset
    {
        std::string record;
        r3d::resource::R3DMeshAsset source;
        r3d::renderer::Mesh mesh;
        r3d::renderer::Texture texture;
    };

    std::vector<Asset> assets_;
};

} // namespace rrr3d::race
