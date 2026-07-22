#pragma once

#include "MainMenu2Spec.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace r3d::resource
{
class ResourceFileSystem;
}

namespace r3d::game::mainmenu2
{

enum class ImageStorage
{
    Rgba8,
    EncodedContainer,
};

struct Image
{
    std::string virtualPath;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    ImageStorage storage = ImageStorage::Rgba8;
    std::vector<std::uint8_t> bytes;
};

struct ResourceAudit
{
    std::size_t catalogFiles = 0;
    std::uintmax_t catalogBytes = 0;
    std::size_t guiImages = 0;
    std::size_t guiMeshes = 0;
    std::uintmax_t guiBytes = 0;
    std::size_t localizedStrings = 0;
};

struct Model
{
    std::string language;
    std::vector<std::string> items;
    std::string versionText;
    Image backgroundImage;
    Image topPanelImage;
    Image bottomPanelImage;
    Image selectionImage;
    Image cursorImage;
    ResourceAudit audit;
};

// Loads the assets and localized strings named by the original
// ResourceManager/MainMenu2 code. No portable menu.cfg or replacement bitmap
// font participates in this path.
Model loadOriginalMainMenu(const resource::ResourceFileSystem& resources,
                           std::string language);

} // namespace r3d::game::mainmenu2
