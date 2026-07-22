#pragma once

#include "resource/ResourceFileSystem.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace r3d::portable
{

struct RgbColor
{
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
};

struct PortableImage
{
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::vector<std::uint8_t> rgba;
};

struct BitmapGlyph
{
    static constexpr std::size_t row_count = 7;
    std::array<std::uint8_t, row_count> rows{};
};

class BitmapFont
{
public:
    const BitmapGlyph& glyph(char character) const;
    bool contains(char character) const noexcept;

private:
    std::map<char, BitmapGlyph> glyphs_;

    friend struct PortableMenuLoader;
};

struct PortableMenuData
{
    std::string title;
    std::string subtitle;
    std::string version;
    std::vector<std::string> items;
    std::string background_resource;
    std::string top_panel_resource;
    std::string bottom_panel_resource;
    std::string selection_resource;
    std::string font_resource;
    RgbColor title_color;
    RgbColor text_color;
    RgbColor selected_text_color;
    BitmapFont font;
    PortableImage background;
    PortableImage top_panel;
    PortableImage bottom_panel;
    PortableImage selection;
    std::size_t legacy_asset_count = 0;
    std::uintmax_t legacy_asset_bytes = 0;
};

PortableMenuData loadPortableMenu(
    const resource::ResourceFileSystem& resources);

} // namespace r3d::portable
