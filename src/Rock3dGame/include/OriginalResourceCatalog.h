#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace r3d::game::originalresources
{

// Declarative equivalents of the source ComplexMeshLib/ComplexImageLib
// records.  The renderer-specific payload is intentionally not represented
// here: D3D9's IVB/Tex resources are materialized lazily by the bgfx owner.
struct MeshResourceDeclaration
{
    std::string_view path;
    bool buildTangentSpace = false;
    bool loadData = false;
    bool initializeVertexBuffer = false;
    int worldType = -1;
    bool loadDataOnWorldLoad = false;
    bool initializeVertexBufferOnWorldLoad = false;
};

struct ImageResourceDeclaration
{
    std::string_view path;
    std::uint32_t levelCount = 1U;
    bool initializeTexture2D = false;
    bool initializeCubeTexture = false;
    int worldType = -1;
    bool initializeTexture2DOnWorldLoad = false;
    bool initializeCubeTextureOnWorldLoad = false;
    bool gui = false;
};

// These vectors preserve every call and its order in the original Windows
// ResourceManager::Load* methods, including three repeated declarations.
const std::vector<MeshResourceDeclaration>& originalMeshResourceCatalog();
const std::vector<ImageResourceDeclaration>& originalImageResourceCatalog();

} // namespace r3d::game::originalresources
