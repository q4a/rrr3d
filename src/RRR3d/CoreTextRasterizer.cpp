#include "CoreTextRasterizer.h"

#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rrr3d::macos
{
namespace
{

struct CfReleaser
{
    template <typename Type>
    void operator()(Type value) const
    {
        CFRelease(value);
    }
};

struct ColorReleaser
{
    void operator()(CGColorRef value) const { CGColorRelease(value); }
};

struct ColorSpaceReleaser
{
    void operator()(CGColorSpaceRef value) const
    {
        CGColorSpaceRelease(value);
    }
};

struct ContextReleaser
{
    void operator()(CGContextRef value) const { CGContextRelease(value); }
};

template <typename Type, typename Releaser = CfReleaser>
class ScopedObject
{
public:
    explicit ScopedObject(Type value = nullptr) : value_(value) {}
    ScopedObject(ScopedObject&& source) noexcept : value_(source.value_)
    {
        source.value_ = nullptr;
    }
    ScopedObject& operator=(ScopedObject&& source) noexcept
    {
        if (this == &source)
            return *this;
        if (value_ != nullptr)
            Releaser{}(value_);
        value_ = source.value_;
        source.value_ = nullptr;
        return *this;
    }
    ~ScopedObject()
    {
        if (value_ != nullptr)
            Releaser{}(value_);
    }
    ScopedObject(const ScopedObject&) = delete;
    ScopedObject& operator=(const ScopedObject&) = delete;
    Type get() const noexcept { return value_; }
    operator Type() const noexcept { return value_; }

private:
    Type value_;
};

using ScopedString = ScopedObject<CFStringRef>;
using ScopedFont = ScopedObject<CTFontRef>;
using ScopedColor = ScopedObject<CGColorRef, ColorReleaser>;
using ScopedDictionary = ScopedObject<CFDictionaryRef>;
using ScopedAttributedString = ScopedObject<CFAttributedStringRef>;
using ScopedLine = ScopedObject<CTLineRef>;
using ScopedArray = ScopedObject<CFArrayRef>;
using ScopedColorSpace =
    ScopedObject<CGColorSpaceRef, ColorSpaceReleaser>;
using ScopedContext = ScopedObject<CGContextRef, ContextReleaser>;

ScopedString makeString(std::string_view value)
{
    return ScopedString(CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(value.data()),
        static_cast<CFIndex>(value.size()), kCFStringEncodingUTF8, false));
}

std::string toUtf8(CFStringRef value)
{
    if (value == nullptr)
        return {};
    const CFIndex length = CFStringGetLength(value);
    const CFIndex capacity = CFStringGetMaximumSizeForEncoding(
                                 length, kCFStringEncodingUTF8) +
                             1;
    std::vector<char> buffer(static_cast<std::size_t>(capacity));
    if (!CFStringGetCString(value, buffer.data(), capacity,
                            kCFStringEncodingUTF8))
        return {};
    return buffer.data();
}

} // namespace

int preferredGamePrimaryLanguageId()
{
    // Windows PRIMARYLANGID values used by the serialized game.xml primId
    // fields: German=7, English=9, Spanish=10, French=12,
    // Portuguese=22, Russian=25.
    ScopedArray languages(CFLocaleCopyPreferredLanguages());
    if (languages.get() == nullptr || CFArrayGetCount(languages.get()) == 0)
        return 9;
    const auto language = static_cast<CFStringRef>(
        CFArrayGetValueAtIndex(languages.get(), 0));
    std::string value = toUtf8(language);
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    if (value.rfind("de", 0) == 0)
        return 7;
    if (value.rfind("es", 0) == 0)
        return 10;
    if (value.rfind("fr", 0) == 0)
        return 12;
    if (value.rfind("pt", 0) == 0)
        return 22;
    if (value.rfind("ru", 0) == 0)
        return 25;
    return 9;
}

TextBitmap rasterizeText(std::string_view utf8,
                         std::string_view requestedFont,
                         float pointSize, bool bold,
                         r3d::game::mainmenu2::Rgba8 color,
                         TextAlignment alignment)
{
    if (utf8.empty() || requestedFont.empty() || pointSize <= 0.0F)
        throw std::runtime_error("Invalid CoreText rasterization request");

    const std::string fontName =
        std::string(requestedFont) + (bold ? " Bold" : "");
    const auto requestedName = makeString(fontName);
    if (requestedName.get() == nullptr)
        throw std::runtime_error("Unable to create CoreFoundation string");

    ScopedFont font(
        CTFontCreateWithName(requestedName.get(), pointSize, nullptr));
    if (font.get() == nullptr)
        throw std::runtime_error("Unable to create CoreText font");

    // TextFont::DoInit passes a positive D3DXCreateFont height: the Windows
    // character CELL (ascent + descent), not the em/point size CoreText uses.
    // Passing it directly enlarged every label and its multiline spacing.
    const CGFloat cellHeight = CTFontGetAscent(font.get()) + CTFontGetDescent(font.get());
    if (cellHeight > 0.0)
        font = ScopedFont(CTFontCreateCopyWithAttributes(
            font.get(), pointSize * pointSize / cellHeight, nullptr, nullptr));

    const CGFloat components[] = {
        color.red / 255.0, color.green / 255.0, color.blue / 255.0,
        color.alpha / 255.0};
    const auto colorSpace = ScopedColorSpace(CGColorSpaceCreateDeviceRGB());
    ScopedColor textColor(CGColorCreate(colorSpace.get(), components));
    if (textColor.get() == nullptr)
        throw std::runtime_error("Unable to create CoreText color");

    const void* keys[] = {kCTFontAttributeName,
                          kCTForegroundColorAttributeName};
    const void* values[] = {font.get(), textColor.get()};
    ScopedDictionary attributes(CFDictionaryCreate(
        kCFAllocatorDefault, keys, values, 2,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));
    struct TextLine
    {
        ScopedLine line;
        double width = 0.0;
    };
    std::vector<TextLine> lines;
    std::size_t lineStart = 0U;
    double typographicWidth = 0.0;
    while (lineStart <= utf8.size())
    {
        const auto lineEnd = utf8.find('\n', lineStart);
        const auto count =
            (lineEnd == std::string_view::npos ? utf8.size() : lineEnd) -
            lineStart;
        const auto lineText = makeString(utf8.substr(lineStart, count));
        if (lineText.get() == nullptr)
            throw std::runtime_error("Unable to create text line");
        ScopedAttributedString attributed(CFAttributedStringCreate(
            kCFAllocatorDefault, lineText.get(), attributes.get()));
        ScopedLine line(CTLineCreateWithAttributedString(attributed.get()));
        if (line.get() == nullptr)
            throw std::runtime_error("Unable to shape menu text");
        const double lineWidth =
            CTLineGetTypographicBounds(line.get(), nullptr, nullptr, nullptr);
        typographicWidth = std::max(typographicWidth, lineWidth);
        lines.push_back({std::move(line), lineWidth});
        if (lineEnd == std::string_view::npos)
            break;
        lineStart = lineEnd + 1U;
    }
    const CGFloat descent = CTFontGetDescent(font.get());
    const CGFloat lineHeight = pointSize;
    const std::size_t width = static_cast<std::size_t>(
        std::ceil(std::max(typographicWidth, 1.0))) + 4U;
    const std::size_t height = static_cast<std::size_t>(
        std::ceil(lineHeight * static_cast<CGFloat>(lines.size()))) + 4U;
    if (width > UINT16_MAX || height > UINT16_MAX ||
        width > std::numeric_limits<std::size_t>::max() / height / 4U)
        throw std::runtime_error("Rasterized menu text is too large");

    TextBitmap result;
    result.width = static_cast<std::uint16_t>(width);
    result.height = static_cast<std::uint16_t>(height);
    result.rgba.assign(width * height * 4U, 0U);

    ScopedContext context(CGBitmapContextCreate(
        result.rgba.data(), width, height, 8, width * 4U,
        colorSpace.get(),
        static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) |
            static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big)));
    if (context.get() == nullptr)
        throw std::runtime_error("Unable to create CoreText bitmap context");
    CGContextSetTextMatrix(context.get(), CGAffineTransformIdentity);
    CGContextSetShouldAntialias(context.get(), true);
    CGContextSetShouldSmoothFonts(context.get(), true);
    for (std::size_t index = 0U; index < lines.size(); ++index)
    {
        const CGFloat alignFactor = alignment == TextAlignment::Left ? 0.0
            : alignment == TextAlignment::Right ? 1.0 : 0.5;
        const CGFloat x =
            2.0 + (typographicWidth - lines[index].width) * alignFactor;
        const CGFloat y =
            2.0 + descent +
            lineHeight * static_cast<CGFloat>(lines.size() - index - 1U);
        CGContextSetTextPosition(context.get(), x, y);
        CTLineDraw(lines[index].line.get(), context.get());
    }

    // CGBitmapContext memory order already matches the texture coordinates
    // used by the bgfx UI quad. Convert only premultiplied alpha to straight
    // alpha; a row flip here would invert every glyph.
    for (std::size_t pixel = 0; pixel < width * height; ++pixel)
    {
        auto* rgba = result.rgba.data() + pixel * 4U;
        const unsigned alpha = rgba[3];
        if (alpha == 0U)
        {
            rgba[0] = rgba[1] = rgba[2] = 0U;
            continue;
        }
        for (int channel = 0; channel < 3; ++channel)
        {
            rgba[channel] = static_cast<std::uint8_t>(std::min(
                255U, (static_cast<unsigned>(rgba[channel]) * 255U +
                       alpha / 2U) /
                          alpha));
        }
    }

    ScopedString resolvedName(CTFontCopyPostScriptName(font.get()));
    result.resolvedFontName = toUtf8(resolvedName.get());
    return result;
}

} // namespace rrr3d::macos
