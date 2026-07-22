#include "PortableMenu.h"

#include <bimg/decode.h>
#include <bx/allocator.h>
#include <bx/error.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <string_view>

namespace r3d::portable
{
namespace
{

using KeyValue = std::pair<std::string, std::string>;

std::string trim(std::string_view value)
{
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.front())) != 0)
    {
        value.remove_prefix(1);
    }
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.back())) != 0)
    {
        value.remove_suffix(1);
    }
    return std::string(value);
}

std::vector<KeyValue> parseConfig(std::string_view contents,
                                  std::string_view resource_name)
{
    std::vector<KeyValue> entries;
    std::istringstream lines{std::string(contents)};
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(lines, line))
    {
        ++line_number;
        const auto comment = line.find('#');
        if (comment != std::string::npos)
            line.erase(comment);

        line = trim(line);
        if (line.empty())
            continue;

        const auto separator = line.find('=');
        if (separator == std::string::npos)
        {
            throw resource::ResourceError(
                std::string(resource_name) + ":" +
                std::to_string(line_number) + ": expected key=value");
        }
        auto key = trim(std::string_view(line).substr(0, separator));
        auto value = trim(std::string_view(line).substr(separator + 1));
        if (key.empty() || value.empty())
        {
            throw resource::ResourceError(
                std::string(resource_name) + ":" +
                std::to_string(line_number) + ": empty key or value");
        }
        entries.emplace_back(std::move(key), std::move(value));
    }
    return entries;
}

std::string requireSingleton(const std::vector<KeyValue>& entries,
                             std::string_view key,
                             std::string_view resource_name)
{
    std::string result;
    for (const auto& [entry_key, value] : entries)
    {
        if (entry_key != key)
            continue;
        if (!result.empty())
        {
            throw resource::ResourceError(
                std::string(resource_name) + ": duplicate key " +
                std::string(key));
        }
        result = value;
    }
    if (result.empty())
    {
        throw resource::ResourceError(std::string(resource_name) +
                                      ": missing key " + std::string(key));
    }
    return result;
}

std::uint8_t parseHexByte(std::string_view value,
                          std::string_view resource_name)
{
    unsigned int result = 0;
    const auto conversion = std::from_chars(value.data(),
                                            value.data() + value.size(),
                                            result,
                                            16);
    if (conversion.ec != std::errc{} ||
        conversion.ptr != value.data() + value.size() || result > 255)
    {
        throw resource::ResourceError(std::string(resource_name) +
                                      ": invalid RGB color");
    }
    return static_cast<std::uint8_t>(result);
}

RgbColor parseColor(std::string_view value, std::string_view resource_name)
{
    if (value.size() != 6)
    {
        throw resource::ResourceError(std::string(resource_name) +
                                      ": RGB colors must use RRGGBB");
    }
    return {parseHexByte(value.substr(0, 2), resource_name),
            parseHexByte(value.substr(2, 2), resource_name),
            parseHexByte(value.substr(4, 2), resource_name)};
}

std::uintmax_t parseUnsigned(std::string_view value,
                             std::string_view resource_name)
{
    std::uintmax_t result = 0;
    const auto conversion = std::from_chars(value.data(),
                                            value.data() + value.size(),
                                            result);
    if (conversion.ec != std::errc{} ||
        conversion.ptr != value.data() + value.size())
    {
        throw resource::ResourceError(std::string(resource_name) +
                                      ": invalid unsigned integer");
    }
    return result;
}

std::string bimgErrorMessage(const bx::Error& error)
{
    const auto& message = error.getMessage();
    return std::string(message.getCPtr(),
                       static_cast<std::size_t>(message.getLength()));
}

PortableImage loadImage(const resource::ResourceFileSystem& resources,
                        std::string_view resource_name)
{
    const auto bytes = resources.readBinary(resource_name);
    if (bytes.empty() ||
        bytes.size() > std::numeric_limits<std::uint32_t>::max())
    {
        throw resource::ResourceError(std::string(resource_name) +
                                      ": image file is empty or too large");
    }

    static bx::DefaultAllocator allocator;
    bx::Error decode_error;
    using ImagePointer =
        std::unique_ptr<bimg::ImageContainer,
                        void (*)(bimg::ImageContainer*)>;
    ImagePointer decoded(
        bimg::imageParse(&allocator, bytes.data(),
                         static_cast<std::uint32_t>(bytes.size()),
                         bimg::TextureFormat::RGBA8, &decode_error),
        &bimg::imageFree);
    if (!decoded)
    {
        throw resource::ResourceError(
            std::string(resource_name) + ": image decode failed: " +
            bimgErrorMessage(decode_error));
    }

    if (decoded->m_width == 0 || decoded->m_height == 0 ||
        decoded->m_width > std::numeric_limits<std::uint16_t>::max() ||
        decoded->m_height > std::numeric_limits<std::uint16_t>::max() ||
        decoded->m_depth > 1 || decoded->m_numLayers != 1 ||
        decoded->m_cubeMap ||
        decoded->m_orientation != bimg::Orientation::R0)
    {
        throw resource::ResourceError(std::string(resource_name) +
                                      ": unsupported image shape/orientation");
    }

    bimg::ImageMip mip;
    if (!bimg::imageGetRawData(*decoded, 0, 0, decoded->m_data,
                               decoded->m_size, mip) ||
        mip.m_format != bimg::TextureFormat::RGBA8)
    {
        throw resource::ResourceError(std::string(resource_name) +
                                      ": decoded RGBA8 pixels are unavailable");
    }

    const std::size_t pixel_count =
        static_cast<std::size_t>(decoded->m_width) * decoded->m_height;
    const std::size_t expected_size = pixel_count * 4U;
    if (mip.m_data == nullptr || mip.m_size < expected_size)
    {
        throw resource::ResourceError(std::string(resource_name) +
                                      ": decoded pixel buffer is truncated");
    }
    PortableImage image;
    image.width = static_cast<std::uint16_t>(decoded->m_width);
    image.height = static_cast<std::uint16_t>(decoded->m_height);
    image.rgba.assign(mip.m_data, mip.m_data + expected_size);
    return image;
}

struct AssetCatalogSummary
{
    std::size_t count = 0;
    std::uintmax_t bytes = 0;
};

AssetCatalogSummary validateAssetCatalog(
    const resource::ResourceFileSystem& resources,
    std::string_view resource_name)
{
    const std::set<std::string> mandatory_assets{
        "Data/GUI/mainFrame.dds",
        "Data/GUI/topPanel5.png",
        "Data/GUI/bottomPanel5.png",
        "Data/GUI/mainItemSel5.png",
        "Data/english.txt",
        "Data/russian.txt",
        "game.xml",
        "db.xml"};
    std::set<std::string> seen;
    AssetCatalogSummary summary;
    std::istringstream lines{resources.readText(resource_name)};
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(lines, line))
    {
        ++line_number;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line.front() == '#')
            continue;

        const auto separator = line.find('\t');
        if (separator == std::string::npos)
        {
            throw resource::ResourceError(
                std::string(resource_name) + ":" +
                std::to_string(line_number) +
                ": expected size<TAB>path");
        }
        const auto expected_size = parseUnsigned(
            std::string_view(line).substr(0, separator), resource_name);
        const auto asset = trim(
            std::string_view(line).substr(separator + 1));
        if (asset.empty() || !seen.insert(asset).second)
        {
            throw resource::ResourceError(
                std::string(resource_name) + ": duplicate/empty asset at line " +
                std::to_string(line_number));
        }
        const auto actual_size = resources.fileSize(asset);
        if (actual_size != expected_size)
        {
            throw resource::ResourceError(
                "Resource size differs from catalog: " + asset);
        }
        ++summary.count;
        summary.bytes += actual_size;
    }

    if (summary.count < 1000)
    {
        throw resource::ResourceError(
            std::string(resource_name) +
            ": catalog is incomplete (expected the full game data set)");
    }
    for (const auto& mandatory : mandatory_assets)
    {
        if (seen.find(mandatory) == seen.end())
        {
            throw resource::ResourceError(
                std::string(resource_name) +
                ": mandatory game asset is absent: " + mandatory);
        }
    }
    return summary;
}

std::map<char, BitmapGlyph> loadFontGlyphs(
    const resource::ResourceFileSystem& resources,
    std::string_view resource_name)
{
    std::map<char, BitmapGlyph> glyphs;
    const auto entries =
        parseConfig(resources.readText(resource_name), resource_name);
    for (const auto& [key, value] : entries)
    {
        if (key.size() != 1 || key[0] == ' ')
        {
            throw resource::ResourceError(std::string(resource_name) +
                                          ": glyph keys must be one character");
        }
        if (glyphs.find(key[0]) != glyphs.end())
        {
            throw resource::ResourceError(std::string(resource_name) +
                                          ": duplicate glyph " + key);
        }

        BitmapGlyph glyph;
        std::size_t row_index = 0;
        std::size_t start = 0;
        while (start <= value.size())
        {
            const auto separator = value.find('/', start);
            const auto row = value.substr(
                start,
                separator == std::string::npos ? std::string::npos
                                               : separator - start);
            if (row_index >= BitmapGlyph::row_count || row.size() != 5)
            {
                throw resource::ResourceError(
                    std::string(resource_name) +
                    ": glyphs must contain seven 5-bit rows");
            }
            std::uint8_t bits = 0;
            for (const char pixel : row)
            {
                if (pixel != '0' && pixel != '1')
                {
                    throw resource::ResourceError(
                        std::string(resource_name) +
                        ": glyph pixels must be zero or one");
                }
                bits = static_cast<std::uint8_t>((bits << 1U) |
                                                 (pixel == '1' ? 1U : 0U));
            }
            glyph.rows[row_index++] = bits;
            if (separator == std::string::npos)
                break;
            start = separator + 1;
        }
        if (row_index != BitmapGlyph::row_count)
        {
            throw resource::ResourceError(std::string(resource_name) +
                                          ": glyph must have exactly seven rows");
        }
        glyphs.emplace(key[0], glyph);
    }
    return glyphs;
}

void validateFontText(const BitmapFont& font,
                      std::string_view text,
                      std::string_view field)
{
    for (const char character : text)
    {
        if (character != ' ' && !font.contains(character))
        {
            throw resource::ResourceError("Font lacks glyph '" +
                                          std::string(1, character) +
                                          "' used by " + std::string(field));
        }
    }
}

} // namespace

struct PortableMenuLoader
{
    static BitmapFont loadBitmapFont(
        const resource::ResourceFileSystem& resources,
        std::string_view resource_name)
    {
        BitmapFont font;
        font.glyphs_ = loadFontGlyphs(resources, resource_name);
        return font;
    }
};

const BitmapGlyph& BitmapFont::glyph(char character) const
{
    const auto entry = glyphs_.find(character);
    if (entry == glyphs_.end())
        throw resource::ResourceError("Bitmap font lacks requested glyph");
    return entry->second;
}

bool BitmapFont::contains(char character) const noexcept
{
    return glyphs_.find(character) != glyphs_.end();
}

PortableMenuData loadPortableMenu(
    const resource::ResourceFileSystem& resources)
{
    constexpr std::string_view manifest_name = "manifest.cfg";
    const auto manifest =
        parseConfig(resources.readText(manifest_name), manifest_name);
    const auto format = requireSingleton(manifest, "format", manifest_name);
    if (format != "rrr3d-game-data-v2")
        throw resource::ResourceError("manifest.cfg: unsupported format " +
                                      format);

    const auto menu_resource =
        requireSingleton(manifest, "menu", manifest_name);
    const auto catalog_resource =
        requireSingleton(manifest, "asset_catalog", manifest_name);
    std::set<std::string> declared_resources;
    bool menu_is_declared = false;
    for (const auto& [key, value] : manifest)
    {
        if (key != "format" && key != "menu" &&
            key != "asset_catalog" && key != "resource")
        {
            throw resource::ResourceError("manifest.cfg: unknown key " + key);
        }
        if (key == "resource")
        {
            resources.resolve(value);
            declared_resources.insert(value);
            menu_is_declared = menu_is_declared || value == menu_resource;
        }
    }
    if (!menu_is_declared)
        throw resource::ResourceError(
            "manifest.cfg: menu must also be declared as a resource");
    if (declared_resources.find(catalog_resource) ==
        declared_resources.end())
    {
        throw resource::ResourceError(
            "manifest.cfg: asset_catalog must be declared as a resource");
    }

    const auto menu =
        parseConfig(resources.readText(menu_resource), menu_resource);
    const std::set<std::string> allowed_menu_keys{
        "title",       "subtitle",      "version",
        "background",  "top_panel",     "bottom_panel",
        "selection",   "font",          "item",
        "title_color", "text_color",    "selected_text_color"};
    for (const auto& [key, value] : menu)
    {
        static_cast<void>(value);
        if (allowed_menu_keys.find(key) == allowed_menu_keys.end())
        {
            throw resource::ResourceError(menu_resource + ": unknown key " +
                                          key);
        }
    }

    PortableMenuData data;
    data.title = requireSingleton(menu, "title", menu_resource);
    data.subtitle = requireSingleton(menu, "subtitle", menu_resource);
    data.version = requireSingleton(menu, "version", menu_resource);
    data.background_resource =
        requireSingleton(menu, "background", menu_resource);
    data.top_panel_resource =
        requireSingleton(menu, "top_panel", menu_resource);
    data.bottom_panel_resource =
        requireSingleton(menu, "bottom_panel", menu_resource);
    data.selection_resource =
        requireSingleton(menu, "selection", menu_resource);
    data.font_resource = requireSingleton(menu, "font", menu_resource);
    const std::array<std::string_view, 5> menu_resources{
        data.background_resource, data.top_panel_resource,
        data.bottom_panel_resource, data.selection_resource,
        data.font_resource};
    for (const auto menu_asset : menu_resources)
    {
        if (declared_resources.find(std::string(menu_asset)) ==
            declared_resources.end())
        {
            throw resource::ResourceError(
                menu_resource + ": menu asset is not declared in manifest: " +
                std::string(menu_asset));
        }
    }
    data.title_color = parseColor(
        requireSingleton(menu, "title_color", menu_resource), menu_resource);
    data.text_color = parseColor(
        requireSingleton(menu, "text_color", menu_resource), menu_resource);
    data.selected_text_color = parseColor(
        requireSingleton(menu, "selected_text_color", menu_resource),
        menu_resource);
    for (const auto& [key, value] : menu)
    {
        if (key == "item")
            data.items.push_back(value);
    }
    if (data.items.size() < 2 || data.items.size() > 12)
        throw resource::ResourceError(menu_resource +
                                      ": expected 2 to 12 menu items");

    data.font = PortableMenuLoader::loadBitmapFont(resources,
                                                   data.font_resource);
    const auto catalog = validateAssetCatalog(resources, catalog_resource);
    data.legacy_asset_count = catalog.count;
    data.legacy_asset_bytes = catalog.bytes;
    data.background = loadImage(resources, data.background_resource);
    data.top_panel = loadImage(resources, data.top_panel_resource);
    data.bottom_panel = loadImage(resources, data.bottom_panel_resource);
    data.selection = loadImage(resources, data.selection_resource);
    validateFontText(data.font, data.title, "title");
    validateFontText(data.font, data.subtitle, "subtitle");
    validateFontText(data.font, data.version, "version");
    for (const auto& item : data.items)
        validateFontText(data.font, item, "menu item");
    return data;
}

} // namespace r3d::portable
