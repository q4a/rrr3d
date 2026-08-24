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

enum class Command : std::uint8_t
{
    SinglePlayer,
    Network,
    Options,
    Authors,
    Exit,
    Back,
};

inline constexpr std::array<Command, 5> itemCommands{
    Command::SinglePlayer, Command::Network, Command::Options,
    Command::Authors, Command::Exit};

constexpr const char* commandName(Command command) noexcept
{
    switch (command)
    {
    case Command::SinglePlayer:
        return "SinglePlayer";
    case Command::Network:
        return "Network";
    case Command::Options:
        return "Options";
    case Command::Authors:
        return "Authors";
    case Command::Exit:
        return "Exit";
    case Command::Back:
        return "Back";
    }
    return "Unknown";
}

// The Windows GUI manager projects directly into the active backbuffer
// resolution. Keep these values in drawable pixels: the shipped GUI images
// and fixed-pixel metrics otherwise become twice as large and blurry in a
// Retina window whose SDL logical size is half its Metal drawable size.
inline float virtualWidth = 1920.0F;
inline float virtualHeight = 1100.0F;
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
