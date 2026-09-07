#include "CoreTextRasterizer.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

int main()
{
    using namespace rrr3d::macos;
    constexpr r3d::game::mainmenu2::Rgba8 white{255, 255, 255, 255};
    try
    {
        for (const float height : {18.0F, 24.0F, 32.0F, 44.0F})
        {
            const auto single = rasterizeText("Money", "Verdana", height, false, white);
            const auto lines = rasterizeText("Money\nPoints", "Verdana", height, false, white);
            if (single.height != height + 4 || lines.height != 2 * height + 4)
                throw std::runtime_error("D3DX positive cell height/line spacing differs");
        }
        const auto left = rasterizeText("MMMMMMMM\nMM", "Verdana", 24, false, white, TextAlignment::Left);
        const auto center = rasterizeText("MMMMMMMM\nMM", "Verdana", 24, false, white);
        const auto right = rasterizeText("MMMMMMMM\nMM", "Verdana", 24, false, white, TextAlignment::Right);
        auto firstInk = [](const TextBitmap& bitmap) {
            int result = bitmap.width;
            for (int y = 26; y < bitmap.height; ++y)
                for (int x = 0; x < bitmap.width; ++x)
                    if (bitmap.rgba[(y * bitmap.width + x) * 4 + 3] > 32)
                        result = std::min(result, x);
            return result;
        };
        if (!(firstInk(left) < firstInk(center) && firstInk(center) < firstInk(right)))
            throw std::runtime_error("multiline horizontal alignment differs");
        const auto russian = rasterizeText("Деньги\nОчки", "Verdana", 44, false, white);
        if (russian.width == 0 || russian.height != 92)
            throw std::runtime_error("Russian result label layout differs");
        std::cout << "CoreText Windows cell-height and multiline alignment passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
