#include "OriginalResourceCatalog.h"

#include <array>
#include <cstddef>
#include <iostream>
#include <string>
#include <unordered_map>

namespace
{

template <typename Declaration>
std::unordered_map<std::string, std::size_t> pathCounts(
    const std::vector<Declaration>& declarations)
{
    std::unordered_map<std::string, std::size_t> result;
    for (const auto& declaration : declarations)
        ++result[std::string(declaration.path)];
    return result;
}

} // namespace

int main()
{
    using namespace r3d::game::originalresources;
    const auto& meshes = originalMeshResourceCatalog();
    const auto& images = originalImageResourceCatalog();
    const auto meshPaths = pathCounts(meshes);
    const auto imagePaths = pathCounts(images);

    const auto countMeshFlag = [&](auto member) {
        std::size_t result = 0U;
        for (const auto& mesh : meshes)
            result += mesh.*member ? 1U : 0U;
        return result;
    };
    const auto countImageFlag = [&](auto member) {
        std::size_t result = 0U;
        for (const auto& image : images)
            result += image.*member ? 1U : 0U;
        return result;
    };
    std::array<std::size_t, 6> meshLoadWorlds{};
    std::array<std::size_t, 6> meshInitializeWorlds{};
    std::array<std::size_t, 6> imageInitializeWorlds{};
    std::size_t generatedMipImages = 0U;
    for (const auto& mesh : meshes)
    {
        if (mesh.worldType >= 0 && mesh.worldType < 6)
        {
            if (mesh.loadDataOnWorldLoad)
                ++meshLoadWorlds[static_cast<std::size_t>(mesh.worldType)];
            if (mesh.initializeVertexBufferOnWorldLoad)
                ++meshInitializeWorlds[
                    static_cast<std::size_t>(mesh.worldType)];
        }
    }
    for (const auto& image : images)
    {
        generatedMipImages += image.levelCount == 0U ? 1U : 0U;
        if (image.worldType >= 0 && image.worldType < 6 &&
            image.initializeTexture2DOnWorldLoad)
        {
            ++imageInitializeWorlds[
                static_cast<std::size_t>(image.worldType)];
        }
    }

    const std::array<std::size_t, 6> expectedMeshLoadWorlds{
        9U, 7U, 6U, 9U, 7U, 3U};
    const std::array<std::size_t, 6> expectedMeshInitializeWorlds{
        31U, 47U, 20U, 29U, 22U, 24U};
    const std::array<std::size_t, 6> expectedImageInitializeWorlds{
        12U, 29U, 10U, 16U, 14U, 10U};

    if (meshes.size() != 324U || meshPaths.size() != 322U ||
        images.size() != 490U || imagePaths.size() != 489U ||
        meshPaths.at("Data/World6/Track/most.r3d") != 2U ||
        meshPaths.at("Data/Car/monstertruckBossWheel.r3d") != 2U ||
        imagePaths.at("Data/World5/Texture/piece.dds") != 2U ||
        countMeshFlag(&MeshResourceDeclaration::buildTangentSpace) != 11U ||
        countMeshFlag(&MeshResourceDeclaration::loadData) != 29U ||
        countMeshFlag(
            &MeshResourceDeclaration::initializeVertexBuffer) != 29U ||
        countMeshFlag(
            &MeshResourceDeclaration::loadDataOnWorldLoad) != 41U ||
        countMeshFlag(
            &MeshResourceDeclaration::initializeVertexBufferOnWorldLoad) !=
            173U ||
        meshLoadWorlds != expectedMeshLoadWorlds ||
        meshInitializeWorlds != expectedMeshInitializeWorlds ||
        generatedMipImages != 201U ||
        countImageFlag(&ImageResourceDeclaration::initializeTexture2D) !=
            140U ||
        countImageFlag(&ImageResourceDeclaration::initializeCubeTexture) !=
            0U ||
        countImageFlag(
            &ImageResourceDeclaration::initializeTexture2DOnWorldLoad) !=
            91U ||
        countImageFlag(
            &ImageResourceDeclaration::initializeCubeTextureOnWorldLoad) !=
            0U ||
        countImageFlag(&ImageResourceDeclaration::gui) != 205U ||
        imageInitializeWorlds != expectedImageInitializeWorlds)
    {
        std::cerr << "original ComplexMesh/Image catalog mismatch: "
                  << meshes.size() << '/' << meshPaths.size() << " meshes, "
                  << images.size() << '/' << imagePaths.size()
                  << " images\n";
        return 1;
    }

    std::cout << "original resource catalog smoke passed: 324/322 meshes, "
                 "490/489 images\n";
    return 0;
}
