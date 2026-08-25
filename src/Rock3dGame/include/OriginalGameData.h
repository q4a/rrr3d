#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
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

enum class CommentatorBusyAction : std::uint8_t
{
    Skip,
    Queue,
    Replace,
};

struct CommentatorVoice
{
    float weight = 0.0F;
    bool startPlayer = false;
    bool endPlayer = false;
    bool humanOnly = false;
    std::string sound;
};

struct CommentatorComment
{
    float chance = 0.0F;
    float delay = 0.0F;
    CommentatorBusyAction busy = CommentatorBusyAction::Skip;
    bool repeatPlayer = true;
    std::vector<CommentatorVoice> voices;
};

struct Commentator
{
    float delay = 0.0F;
    std::map<std::string, CommentatorComment> comments;
};

struct Catalog
{
    std::vector<Language> languages;
    std::vector<std::string> commentatorStyles;
    Commentator commentator;
};

// Mirrors GameMode::LoadGameData for languages, commentator styles and the
// complete Commentator::LoadGame table. Language/style order is significant
// because both OptionsMenu and StartOptionsMenu use vector indices directly.
Catalog loadOriginalGameDataCatalog(
    const resource::ResourceFileSystem& resources);

const Language* findLanguage(const Catalog& catalog,
                             std::string_view name) noexcept;

using StringLibrary =
    std::unordered_map<std::string, std::string>;

// Literal ResourceManager::StringLibrary::Load token-stream behavior over
// the shipped UTF-16LE files, including its malformed-line recovery and
// last-duplicate-wins Set semantics.
StringLibrary loadOriginalStringLibrary(
    const resource::ResourceFileSystem& resources,
    const Language& language);

} // namespace r3d::game::originalgamedata
