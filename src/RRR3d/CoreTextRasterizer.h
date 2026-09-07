#pragma once

#include "MainMenu2Spec.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace rrr3d::macos
{

enum class TextAlignment { Left, Center, Right };

struct TextBitmap
{
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::vector<std::uint8_t> rgba;
    std::string resolvedFontName;
};

TextBitmap rasterizeText(std::string_view utf8,
                         std::string_view requestedFont,
                         float pointSize, bool bold,
                         r3d::game::mainmenu2::Rgba8 color,
                         TextAlignment alignment = TextAlignment::Center);

// Portable GetUserDefaultUILanguage/PRIMARYLANGID boundary used by the
// source GameMode::AutodetectLanguage policy.
int preferredGamePrimaryLanguageId();

} // namespace rrr3d::macos
