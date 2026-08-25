#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace r3d::resource
{
class ResourceFileSystem;
}

namespace r3d::game::originalgamedata
{

enum class LanguageCharset : std::uint8_t
{
    Default,
    EastEurope,
    Russian,
    Baltic,
};

struct Language
{
    std::string name;
    std::string file;
    std::string locale;
    LanguageCharset charset = LanguageCharset::Default;
    int primaryId = 0;
};

struct Catalog
{
    std::vector<Language> languages;
    std::vector<std::string> commentatorStyles;
};

// Mirrors GameMode::LoadGameData for the non-audio language/style records.
// Order is significant because both OptionsMenu and StartOptionsMenu use the
// serialized vector index directly in their steppers.
Catalog loadOriginalGameDataCatalog(
    const resource::ResourceFileSystem& resources);

const Language* findLanguage(const Catalog& catalog,
                             std::string_view name) noexcept;

} // namespace r3d::game::originalgamedata
