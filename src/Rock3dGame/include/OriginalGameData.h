#pragma once

#include "MusicCat.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
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

struct MusicCatalog
{
    std::vector<MusicCatTrack> menu;
    std::vector<MusicCatTrack> game;
};

struct Catalog
{
    std::vector<Language> languages;
    std::vector<std::string> commentatorStyles;
    MusicCatalog music;
    Commentator commentator;
};

// Mirrors the complete GameMode::LoadGameData call: languages, commentator
// styles, both MusicCat::LoadGame catalogs and Commentator::LoadGame.
// Language/style/track order is serialized gameplay state and is significant.
Catalog loadOriginalGameDataCatalog(
    const resource::ResourceFileSystem& resources);

const Language* findLanguage(const Catalog& catalog,
                             std::string_view name) noexcept;

// Platform-independent ownership and lookup semantics of the original
// ResourceManager::StringLibrary.  The Windows class returns the id itself
// when a key is absent or explicitly mapped to an empty string.
class StringLibrary
{
public:
    std::size_t size() const noexcept;
    std::string get(std::string_view id) const;
    void set(std::string id, std::string value);
    bool has(std::string_view id) const;

private:
    std::map<std::string, std::string, std::less<>> strings_;
};

// Literal ResourceManager::StringLibrary::Load token-stream behavior over
// the shipped UTF-16LE files, including its malformed-line recovery and
// last-duplicate-wins Set semantics.
StringLibrary loadOriginalStringLibrary(
    const resource::ResourceFileSystem& resources,
    const Language& language);

} // namespace r3d::game::originalgamedata
