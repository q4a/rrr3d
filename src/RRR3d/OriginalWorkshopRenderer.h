#pragma once

#include "OriginalGarage.h"
#include "OriginalRace.h"
#include "OriginalResourceManager.h"
#include "renderer/Renderer.h"
#include "resource/R3DMeshAsset.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

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
        OriginalResourceManager& resources,
        const r3d::game::originalrace::OriginalGarageCatalog& catalog,
        const r3d::game::originalrace::Race& race,
        std::string& error);
    void shutdown(r3d::renderer::GraphicsDevice& device) noexcept;

    void drawItem(
        r3d::renderer::GraphicsDevice& device,
        r3d::renderer::Shader shader,
        const r3d::game::originalrace::OriginalWorkshopItem& item,
        float centerX, float centerY, float width, float height,
        float rotationRadians,
        const r3d::renderer::PipelineState& pipeline) const;
    void drawPlanet(
        r3d::renderer::GraphicsDevice& device,
        r3d::renderer::Shader shader,
        const r3d::game::originalrace::OriginalGaragePlanet& planet,
        float centerX, float centerY, float width, float height,
        float rotationRadians,
        const r3d::renderer::PipelineState& pipeline) const;
    void drawCar(
        r3d::renderer::GraphicsDevice& device,
        r3d::renderer::Shader shader, std::string_view record,
        float centerX, float centerY, float width, float height,
        float rotationRadians,
        const r3d::renderer::PipelineState& pipeline,
        const std::array<float, 4>* color = nullptr) const;

private:
    struct NodeAsset
    {
        std::shared_ptr<const r3d::resource::R3DMeshAsset> source;
        r3d::renderer::Mesh mesh;
        std::vector<r3d::renderer::Texture> textures;
        r3d::game::originalrace::Transform local;
    };

    struct ModelAsset
    {
        std::string record;
        std::vector<NodeAsset> nodes;
        std::array<float, 3> minimum{};
        std::array<float, 3> maximum{};
    };

    std::vector<ModelAsset> workshopAssets_;
    std::vector<ModelAsset> planetAssets_;
    std::vector<ModelAsset> carAssets_;
};

} // namespace rrr3d::race
