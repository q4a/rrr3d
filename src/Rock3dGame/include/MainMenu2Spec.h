#pragma once

#include <array>
#include <cstdint>

namespace r3d::game::mainmenu2
{

// Renderer-neutral constants extracted from the original MainMenu2 and
// ResourceManager implementation. The legacy D3D9 UI and the macOS renderer
// slice deliberately consume this same specification.
inline constexpr char background[] = "GUI\\mainFrame.dds";
inline constexpr char topPanel[] = "GUI\\topPanel5.png";
inline constexpr char bottomPanel[] = "GUI\\bottomPanel5.png";
inline constexpr char selection[] = "GUI\\mainItemSel5.png";
inline constexpr char cursor[] = "GUI\\cursor.png";

inline constexpr char headerFont[] = "Header";
inline constexpr char smallFont[] = "VerySmall";
inline constexpr char fontFace[] = "Verdana";
inline constexpr float headerFontHeight = 44.0F;
inline constexpr float smallFontHeight = 18.0F;

inline constexpr char version[] = "v. 1.2.0";
inline constexpr std::array<const char*, 5> itemStringKeys{
    "svSingleGame", "svNetGame", "svOptions", "svAuthors", "svExit"};

inline constexpr float virtualWidth = 1920.0F;
inline constexpr float virtualHeight = 1100.0F;
inline constexpr float itemCenterOffsetX = 5.0F;
inline constexpr float firstItemOffsetY = -100.0F;
inline constexpr float itemSpacing = 53.0F;

struct Rgba8
{
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
    std::uint8_t alpha;
};

inline constexpr Rgba8 normalTextColor{255, 138, 112, 255};
inline constexpr Rgba8 selectedTextColor{255, 255, 255, 255};

} // namespace r3d::game::mainmenu2
