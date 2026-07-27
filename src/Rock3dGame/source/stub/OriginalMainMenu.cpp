#include "OriginalMainMenu.h"

#include "resource/R3DMeshAsset.h"
#include "resource/ResourceFileSystem.h"

#include <bimg/decode.h>
#include <bx/allocator.h>
#include <bx/error.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace r3d::game::mainmenu2
{
namespace
{

struct CatalogEntry
{
    std::string path;
    std::uintmax_t bytes = 0;
};

using ImagePointer =
    std::unique_ptr<bimg::ImageContainer, void (*)(bimg::ImageContainer*)>;

bx::DefaultAllocator imageAllocator;

std::string bimgErrorMessage(const bx::Error& error)
{
    const auto& message = error.getMessage();
    return std::string(message.getCPtr(),
                       static_cast<std::size_t>(message.getLength()));
}

std::string dataPath(std::string_view legacyPath)
{
    std::string result = "Data/";
    result.reserve(result.size() + legacyPath.size());
    for (const char value : legacyPath)
        result.push_back(value == '\\' ? '/' : value);
    return result;
}

std::string lowerExtension(std::string_view path)
{
    const auto dot = path.find_last_of('.');
    std::string result(dot == std::string_view::npos
                           ? std::string_view{}
                           : path.substr(dot));
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::tolower(value));
                   });
    return result;
}

std::uintmax_t parseSize(std::string_view value,
                         std::string_view resourceName,
                         std::size_t lineNumber)
{
    std::uintmax_t result = 0;
    const auto conversion = std::from_chars(
        value.data(), value.data() + value.size(), result);
    if (conversion.ec != std::errc{} ||
        conversion.ptr != value.data() + value.size())
    {
        throw resource::ResourceError(
            std::string(resourceName) + ":" + std::to_string(lineNumber) +
            ": invalid byte count");
    }
    return result;
}

std::vector<CatalogEntry> loadAndValidateCatalog(
    const resource::ResourceFileSystem& resources, ResourceAudit& audit)
{
    constexpr std::string_view catalogPath = "legacy-assets.catalog";
    std::vector<CatalogEntry> entries;
    std::istringstream lines(resources.readText(catalogPath));
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(lines, line))
    {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line.front() == '#')
            continue;

        const auto separator = line.find('\t');
        if (separator == std::string::npos || separator + 1 == line.size())
        {
            throw resource::ResourceError(
                std::string(catalogPath) + ":" +
                std::to_string(lineNumber) +
                ": expected size<TAB>resource");
        }
        CatalogEntry entry;
        entry.bytes = parseSize(std::string_view(line).substr(0, separator),
                                catalogPath, lineNumber);
        entry.path = line.substr(separator + 1);
        const auto actualSize = resources.fileSize(entry.path);
        if (actualSize != entry.bytes)
        {
            throw resource::ResourceError(
                "Resource size differs from original catalog: " +
                entry.path);
        }
        audit.catalogBytes += entry.bytes;
        entries.push_back(std::move(entry));
    }
    audit.catalogFiles = entries.size();
    if (entries.size() < 1000)
    {
        throw resource::ResourceError(
            "legacy-assets.catalog does not describe the full game data");
    }
    return entries;
}

ImagePointer parseImage(const std::vector<std::uint8_t>& bytes,
                        bimg::TextureFormat::Enum outputFormat,
                        std::string_view path)
{
    if (bytes.empty() ||
        bytes.size() > std::numeric_limits<std::uint32_t>::max())
    {
        throw resource::ResourceError(std::string(path) +
                                      ": empty or oversized image");
    }
    bx::Error error;
    ImagePointer image(
        bimg::imageParse(&imageAllocator, bytes.data(),
                         static_cast<std::uint32_t>(bytes.size()),
                         outputFormat, &error),
        &bimg::imageFree);
    if (!image)
    {
        throw resource::ResourceError(std::string(path) +
                                      ": image decode failed: " +
                                      bimgErrorMessage(error));
    }
    if (image->m_width == 0 || image->m_height == 0 ||
        image->m_width > UINT16_MAX || image->m_height > UINT16_MAX ||
        image->m_depth > 1 || image->m_numLayers != 1 || image->m_cubeMap ||
        image->m_orientation != bimg::Orientation::R0)
    {
        throw resource::ResourceError(std::string(path) +
                                      ": unsupported image shape");
    }
    return image;
}

void validateGuiResources(const resource::ResourceFileSystem& resources,
                          const std::vector<CatalogEntry>& catalog,
                          ResourceAudit& audit)
{
    for (const auto& entry : catalog)
    {
        if (entry.path.rfind("Data/GUI/", 0) != 0)
            continue;

        audit.guiBytes += entry.bytes;
        const auto extension = lowerExtension(entry.path);
        if (extension == ".png" || extension == ".dds")
        {
            const auto bytes = resources.readBinary(entry.path);
            static_cast<void>(parseImage(
                bytes, bimg::TextureFormat::Count, entry.path));
            ++audit.guiImages;
        }
        else if (extension == ".r3d")
        {
            static_cast<void>(
                resource::loadR3DMeshAsset(resources, entry.path));
            ++audit.guiMeshes;
        }
        else
        {
            throw resource::ResourceError(
                "Unexpected original GUI resource type: " + entry.path);
        }
    }
    if (audit.guiImages == 0 || audit.guiMeshes == 0)
    {
        throw resource::ResourceError(
            "Original GUI image/mesh catalog is incomplete");
    }
}

Image loadImage(const resource::ResourceFileSystem& resources,
                std::string virtualPath)
{
    Image result;
    result.virtualPath = std::move(virtualPath);
    const auto encoded = resources.readBinary(result.virtualPath);
    const auto extension = lowerExtension(result.virtualPath);

    if (extension == ".dds")
    {
        const auto parsed = parseImage(encoded, bimg::TextureFormat::Count,
                                       result.virtualPath);
        result.width = static_cast<std::uint16_t>(parsed->m_width);
        result.height = static_cast<std::uint16_t>(parsed->m_height);
        result.storage = ImageStorage::EncodedContainer;
        result.bytes = encoded;
        return result;
    }

    const auto parsed = parseImage(encoded, bimg::TextureFormat::RGBA8,
                                   result.virtualPath);
    bimg::ImageMip mip;
    if (!bimg::imageGetRawData(*parsed, 0, 0, parsed->m_data,
                               parsed->m_size, mip) ||
        mip.m_format != bimg::TextureFormat::RGBA8)
    {
        throw resource::ResourceError(result.virtualPath +
                                      ": RGBA pixels are unavailable");
    }
    result.width = static_cast<std::uint16_t>(parsed->m_width);
    result.height = static_cast<std::uint16_t>(parsed->m_height);
    const std::size_t expected =
        static_cast<std::size_t>(result.width) * result.height * 4U;
    if (mip.m_data == nullptr || mip.m_size < expected)
    {
        throw resource::ResourceError(result.virtualPath +
                                      ": decoded pixels are truncated");
    }
    result.bytes.assign(mip.m_data, mip.m_data + expected);
    return result;
}

void appendUtf8(std::string& result, std::uint32_t codePoint)
{
    if (codePoint <= 0x7fU)
    {
        result.push_back(static_cast<char>(codePoint));
    }
    else if (codePoint <= 0x7ffU)
    {
        result.push_back(static_cast<char>(0xc0U | (codePoint >> 6U)));
        result.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
    }
    else if (codePoint <= 0xffffU)
    {
        result.push_back(static_cast<char>(0xe0U | (codePoint >> 12U)));
        result.push_back(
            static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3fU)));
        result.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
    }
    else
    {
        result.push_back(static_cast<char>(0xf0U | (codePoint >> 18U)));
        result.push_back(
            static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3fU)));
        result.push_back(
            static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3fU)));
        result.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
    }
}

std::string decodeUtf16Le(const std::vector<std::uint8_t>& bytes,
                          std::string_view path)
{
    if (bytes.size() < 2 || (bytes.size() % 2U) != 0U || bytes[0] != 0xffU ||
        bytes[1] != 0xfeU)
    {
        throw resource::ResourceError(std::string(path) +
                                      ": expected UTF-16LE with BOM");
    }

    std::string result;
    result.reserve(bytes.size());
    for (std::size_t offset = 2; offset < bytes.size(); offset += 2)
    {
        const std::uint16_t first = static_cast<std::uint16_t>(
            bytes[offset] | (static_cast<std::uint16_t>(bytes[offset + 1])
                             << 8U));
        std::uint32_t codePoint = first;
        if (first >= 0xd800U && first <= 0xdbffU)
        {
            if (offset + 3 >= bytes.size())
                throw resource::ResourceError(std::string(path) +
                                              ": truncated surrogate pair");
            offset += 2;
            const std::uint16_t second = static_cast<std::uint16_t>(
                bytes[offset] |
                (static_cast<std::uint16_t>(bytes[offset + 1]) << 8U));
            if (second < 0xdc00U || second > 0xdfffU)
                throw resource::ResourceError(std::string(path) +
                                              ": invalid surrogate pair");
            codePoint = 0x10000U +
                        ((static_cast<std::uint32_t>(first) - 0xd800U)
                         << 10U) +
                        (static_cast<std::uint32_t>(second) - 0xdc00U);
        }
        else if (first >= 0xdc00U && first <= 0xdfffU)
        {
            throw resource::ResourceError(std::string(path) +
                                          ": unexpected low surrogate");
        }
        appendUtf8(result, codePoint);
    }
    return result;
}

std::unordered_map<std::string, std::string> loadStringLibrary(
    const resource::ResourceFileSystem& resources, std::string_view path)
{
    const auto decoded = decodeUtf16Le(resources.readBinary(path), path);
    std::unordered_map<std::string, std::string> strings;
    std::istringstream lines(decoded);
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(lines, line))
    {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;

        const auto keyEnd = line.find_first_of(" \t");
        const auto quoteBegin = line.find('"', keyEnd);
        const auto quoteEnd = line.find_last_of('"');
        if (keyEnd == std::string::npos || quoteBegin == std::string::npos ||
            quoteEnd == quoteBegin)
        {
            throw resource::ResourceError(
                std::string(path) + ":" + std::to_string(lineNumber) +
                ": malformed localized string");
        }
        const std::string key = line.substr(0, keyEnd);
        std::string value =
            line.substr(quoteBegin + 1, quoteEnd - quoteBegin - 1);
        std::size_t escapedNewline = 0;
        while ((escapedNewline = value.find("\\n", escapedNewline)) !=
               std::string::npos)
        {
            value.replace(escapedNewline, 2, 1, '\n');
            ++escapedNewline;
        }
        // The legacy StringLibrary::Set uses map assignment, so the last
        // duplicate definition wins (russian.txt contains one intentionally).
        strings[key] = std::move(value);
    }
    return strings;
}

std::string languagePath(const resource::ResourceFileSystem& resources,
                         std::string& language)
{
    std::transform(language.begin(), language.end(), language.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::tolower(value));
                   });
    if (language.empty())
        language = "english";
    if (language != "english" && language != "russian")
    {
        throw resource::ResourceError(
            "Milestone 6 language must be english or russian");
    }

    const std::string file = "Data/" + language + ".txt";
    const auto gameDefinition = resources.readText("game.xml");
    const std::string legacyFile = "Data\\" + language + ".txt";
    if (gameDefinition.find("<" + language + ">") == std::string::npos ||
        gameDefinition.find(legacyFile) == std::string::npos)
    {
        throw resource::ResourceError(
            "game.xml does not declare requested language: " + language);
    }
    return file;
}

} // namespace

Controller::Controller(std::size_t itemCount) : itemCount_(itemCount)
{
    if (itemCount_ != itemCommands.size())
        throw std::invalid_argument(
            "MainMenu2 controller item count differs from shared spec");
}

std::size_t Controller::selectedItem() const noexcept
{
    return selectedItem_;
}

bool Controller::select(std::size_t item) noexcept
{
    if (item >= itemCount_)
        return false;
    selectedItem_ = item;
    return true;
}

std::optional<Command> Controller::handle(
    const rrr3d::input::ActionEvent& event)
{
    if (!event.active)
        return std::nullopt;

    switch (event.action)
    {
    case rrr3d::input::Action::MenuUp:
        selectedItem_ =
            selectedItem_ == 0 ? itemCount_ - 1 : selectedItem_ - 1;
        break;
    case rrr3d::input::Action::MenuDown:
        selectedItem_ = (selectedItem_ + 1) % itemCount_;
        break;
    case rrr3d::input::Action::MenuConfirm:
        if (!event.repeated)
            return itemCommands[selectedItem_];
        break;
    case rrr3d::input::Action::MenuBack:
        if (!event.repeated)
            return Command::Back;
        break;
    default:
        break;
    }
    return std::nullopt;
}

bool runOriginalMainMenuInputSmoke(std::string& error)
{
    Controller controller(itemCommands.size());
    using rrr3d::input::Action;
    using rrr3d::input::ActionEvent;
    using rrr3d::input::Source;

    controller.handle(
        {Action::MenuDown, 1.0F, true, false, Source::Keyboard, 0});
    controller.handle(
        {Action::MenuDown, 1.0F, true, true, Source::Keyboard, 0});
    if (controller.selectedItem() != 2)
    {
        error = "MainMenu2 down/repeat navigation failed";
        return false;
    }
    controller.handle(
        {Action::MenuUp, 1.0F, true, false, Source::GamepadButton, 1});
    if (controller.selectedItem() != 1)
    {
        error = "MainMenu2 up navigation failed";
        return false;
    }
    if (!controller.select(4) || controller.select(itemCommands.size()))
    {
        error = "MainMenu2 pointer selection bounds failed";
        return false;
    }
    const auto command = controller.handle(
        {Action::MenuConfirm, 1.0F, true, false, Source::Mouse, 0});
    if (command != Command::Exit)
    {
        error = "MainMenu2 command dispatch differs from legacy item order";
        return false;
    }
    const auto back = controller.handle(
        {Action::MenuBack, 1.0F, true, false, Source::GamepadButton, 1});
    if (back != Command::Back)
    {
        error = "MainMenu2 back dispatch failed";
        return false;
    }
    error.clear();
    return true;
}

Image loadOriginalImage(const resource::ResourceFileSystem& resources,
                        std::string virtualPath)
{
    return loadImage(resources, std::move(virtualPath));
}

Model loadOriginalMainMenu(const resource::ResourceFileSystem& resources,
                           std::string language)
{
    Model model;
    const auto catalog = loadAndValidateCatalog(resources, model.audit);
    validateGuiResources(resources, catalog, model.audit);

    auto strings =
        loadStringLibrary(resources, languagePath(resources, language));
    model.language = std::move(language);
    model.audit.localizedStrings = strings.size();
    for (const char* key : itemStringKeys)
    {
        const auto entry = strings.find(key);
        if (entry == strings.end())
            throw resource::ResourceError("Localized string is missing: " +
                                          std::string(key));
        model.items.push_back(entry->second);
    }
    model.versionText = version;

    model.backgroundImage = loadImage(resources, dataPath(background));
    model.topPanelImage = loadImage(resources, dataPath(topPanel));
    model.bottomPanelImage = loadImage(resources, dataPath(bottomPanel));
    model.selectionImage = loadImage(resources, dataPath(selection));
    model.cursorImage = loadImage(resources, dataPath(cursor));
    model.localizedStrings = std::move(strings);
    return model;
}

} // namespace r3d::game::mainmenu2
