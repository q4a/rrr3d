#include "OriginalGameData.h"

#include "resource/ResourceFileSystem.h"

#include <tinyxml.h>

#include <array>
#include <charconv>
#include <cctype>
#include <exception>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace r3d::game::originalgamedata
{
namespace
{

const TiXmlElement* requiredChild(const TiXmlElement* parent,
                                  const char* name,
                                  std::string_view source)
{
    const auto* result =
        parent != nullptr ? parent->FirstChildElement(name) : nullptr;
    if (result == nullptr)
    {
        throw resource::ResourceError(
            std::string(source) + ": missing " + name);
    }
    return result;
}

std::string requiredText(const TiXmlElement* parent, const char* name,
                         std::string_view source)
{
    const auto* element = requiredChild(parent, name, source);
    const char* value = element->GetText();
    if (value == nullptr || *value == '\0')
    {
        throw resource::ResourceError(
            std::string(source) + ": empty " + name);
    }
    return value;
}

LanguageCharset parseCharset(std::string_view value,
                             std::string_view language)
{
    constexpr std::array<std::pair<std::string_view, LanguageCharset>, 4>
        values{{
            {"lcDefault", LanguageCharset::Default},
            {"lcEastEurope", LanguageCharset::EastEurope},
            {"lcRussian", LanguageCharset::Russian},
            {"lcBaltic", LanguageCharset::Baltic},
        }};
    for (const auto& [name, charset] : values)
    {
        if (value == name)
            return charset;
    }
    throw resource::ResourceError(
        "game.xml/languages/" + std::string(language) +
        ": invalid charset " + std::string(value));
}

int parsePrimaryId(std::string_view value, std::string_view language)
{
    int result = 0;
    const auto parsed = std::from_chars(
        value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} ||
        parsed.ptr != value.data() + value.size() || result <= 0)
    {
        throw resource::ResourceError(
            "game.xml/languages/" + std::string(language) +
            ": invalid primId");
    }
    return result;
}

int parseInteger(std::string_view value, std::string_view source,
                 std::string_view field)
{
    int result = 0;
    const auto parsed = std::from_chars(
        value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} ||
        parsed.ptr != value.data() + value.size())
    {
        throw resource::ResourceError(
            std::string(source) + ": invalid " + std::string(field));
    }
    return result;
}

std::string trim(std::string value)
{
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.front())) != 0)
        value.erase(value.begin());
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.back())) != 0)
        value.pop_back();
    return value;
}

float parseFloat(const TiXmlElement* parent, const char* name,
                 std::string_view source)
{
    const std::string value = trim(requiredText(parent, name, source));
    try
    {
        std::size_t consumed = 0U;
        const float result = std::stof(value, &consumed);
        if (consumed != value.size())
            throw std::invalid_argument("trailing data");
        return result;
    }
    catch (const std::exception&)
    {
        throw resource::ResourceError(
            std::string(source) + ": invalid " + name);
    }
}

bool parseBool(const TiXmlElement* parent, const char* name,
               std::string_view source)
{
    const std::string value = trim(requiredText(parent, name, source));
    if (value == "true" || value == "1")
        return true;
    if (value == "false" || value == "0")
        return false;
    throw resource::ResourceError(
        std::string(source) + ": invalid " + name);
}

CommentatorBusyAction parseBusy(const TiXmlElement* parent,
                                std::string_view source)
{
    const std::string value = trim(requiredText(parent, "busy", source));
    if (value == "baSkip")
        return CommentatorBusyAction::Skip;
    if (value == "baQueue")
        return CommentatorBusyAction::Queue;
    if (value == "baReplace")
        return CommentatorBusyAction::Replace;
    throw resource::ResourceError(
        std::string(source) + ": invalid busy action " + value);
}

std::vector<MusicCatTrack> loadMusicTracks(
    const resource::ResourceFileSystem& resources,
    const TiXmlElement* root, const char* catalogName)
{
    const std::string catalogSource =
        std::string("game.xml/") + catalogName;
    const auto* catalog =
        requiredChild(root, catalogName, "game.xml");
    const auto* tracks =
        requiredChild(catalog, "tracks", catalogSource);
    std::vector<MusicCatTrack> result;
    for (auto* entry = tracks->FirstChildElement(); entry != nullptr;
         entry = entry->NextSiblingElement())
    {
        const std::string source =
            catalogSource + "/" + entry->Value();
        const auto* item = requiredChild(entry, "item", source);
        const char* path = item->Attribute("item");
        if (path == nullptr || *path == '\0')
        {
            throw resource::ResourceError(
                source + ": track has no serialized item reference");
        }
        MusicCatTrack track;
        track.path = path;
        track.name = requiredText(entry, "name", source);
        track.band = requiredText(entry, "band", source);
        track.group = parseInteger(
            trim(requiredText(entry, "group", source)), source,
            "group");
        const std::string dataPath = "Data\\" + track.path;
        if (!resources.exists(dataPath))
        {
            throw resource::ResourceError(
                source + ": missing " + dataPath);
        }
        result.push_back(std::move(track));
    }
    if (result.empty())
    {
        throw resource::ResourceError(
            catalogSource + ": empty track catalog");
    }
    return result;
}

void appendUtf8(std::string& result, std::uint32_t codePoint)
{
    if (codePoint <= 0x7fU)
    {
        result.push_back(static_cast<char>(codePoint));
    }
    else if (codePoint <= 0x7ffU)
    {
        result.push_back(static_cast<char>(0xc0U | (codePoint >> 6U)));
        result.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
    }
    else if (codePoint <= 0xffffU)
    {
        result.push_back(static_cast<char>(0xe0U | (codePoint >> 12U)));
        result.push_back(
            static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3fU)));
        result.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
    }
    else
    {
        result.push_back(static_cast<char>(0xf0U | (codePoint >> 18U)));
        result.push_back(
            static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3fU)));
        result.push_back(
            static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3fU)));
        result.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
    }
}

std::string decodeUtf16Le(const std::vector<std::uint8_t>& bytes,
                          std::string_view path)
{
    if (bytes.size() < 2U || (bytes.size() % 2U) != 0U ||
        bytes[0] != 0xffU || bytes[1] != 0xfeU)
    {
        throw resource::ResourceError(
            std::string(path) + ": expected UTF-16LE with BOM");
    }
    std::string result;
    result.reserve(bytes.size());
    for (std::size_t offset = 2U; offset < bytes.size(); offset += 2U)
    {
        const std::uint16_t first = static_cast<std::uint16_t>(
            bytes[offset] |
            (static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U));
        std::uint32_t codePoint = first;
        if (first >= 0xd800U && first <= 0xdbffU)
        {
            if (offset + 3U >= bytes.size())
            {
                throw resource::ResourceError(
                    std::string(path) + ": truncated surrogate pair");
            }
            offset += 2U;
            const std::uint16_t second = static_cast<std::uint16_t>(
                bytes[offset] |
                (static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U));
            if (second < 0xdc00U || second > 0xdfffU)
            {
                throw resource::ResourceError(
                    std::string(path) + ": invalid surrogate pair");
            }
            codePoint = 0x10000U +
                        ((static_cast<std::uint32_t>(first) - 0xd800U)
                         << 10U) +
                        (static_cast<std::uint32_t>(second) - 0xdc00U);
        }
        else if (first >= 0xdc00U && first <= 0xdfffU)
        {
            throw resource::ResourceError(
                std::string(path) + ": unexpected low surrogate");
        }
        appendUtf8(result, codePoint);
    }
    return result;
}

} // namespace

Catalog loadOriginalGameDataCatalog(
    const resource::ResourceFileSystem& resources)
{
    TiXmlDocument document;
    const std::string xml = resources.readText("game.xml");
    document.Parse(xml.c_str(), nullptr, TIXML_ENCODING_UTF8);
    if (document.Error())
    {
        throw resource::ResourceError(
            "Cannot parse serialized GameMode catalog: " +
            std::string(document.ErrorDesc()));
    }
    const auto* root = document.RootElement();
    if (root == nullptr)
        throw resource::ResourceError("game.xml has no root element");

    Catalog result;
    const auto* languages = requiredChild(root, "languages", "game.xml");
    for (auto* entry = languages->FirstChildElement(); entry != nullptr;
         entry = entry->NextSiblingElement())
    {
        Language language;
        language.name = entry->Value();
        if (language.name.empty())
        {
            throw resource::ResourceError(
                "game.xml/languages contains an unnamed language");
        }
        language.file = requiredText(entry, "file", language.name);
        language.locale = requiredText(entry, "locale", language.name);
        language.charset = parseCharset(
            requiredText(entry, "charset", language.name), language.name);
        language.primaryId = parsePrimaryId(
            requiredText(entry, "primId", language.name), language.name);
        if (!resources.exists(language.file))
        {
            throw resource::ResourceError(
                "game.xml/languages/" + language.name +
                ": missing " + language.file);
        }
        result.languages.push_back(std::move(language));
    }
    if (result.languages.empty())
    {
        throw resource::ResourceError(
            "game.xml/languages contains no language records");
    }

    const auto* commentators =
        requiredChild(root, "commentators", "game.xml");
    for (auto* entry = commentators->FirstChildElement(); entry != nullptr;
         entry = entry->NextSiblingElement())
    {
        const std::string name = entry->Value();
        if (name.empty())
        {
            throw resource::ResourceError(
                "game.xml/commentators contains an unnamed style");
        }
        result.commentatorStyles.push_back(name);
    }
    if (result.commentatorStyles.empty())
    {
        throw resource::ResourceError(
            "game.xml/commentators contains no styles");
    }

    result.music.menu = loadMusicTracks(resources, root, "menuMusic");
    result.music.game = loadMusicTracks(resources, root, "gameMusic");

    const auto* commentator =
        requiredChild(root, "commentator", "game.xml");
    result.commentator.delay =
        parseFloat(commentator, "delay", "game.xml/commentator");
    const auto* comments = requiredChild(
        commentator, "comments", "game.xml/commentator");
    for (auto* entry = comments->FirstChildElement(); entry != nullptr;
         entry = entry->NextSiblingElement())
    {
        const std::string name = entry->Value();
        if (name.empty())
        {
            throw resource::ResourceError(
                "game.xml/commentator contains an unnamed comment");
        }
        const std::string source =
            "game.xml/commentator/comments/" + name;
        CommentatorComment comment;
        comment.chance = parseFloat(entry, "chance", source);
        comment.delay = parseFloat(entry, "delay", source);
        comment.busy = parseBusy(entry, source);
        comment.repeatPlayer = parseBool(entry, "repeatPlayer", source);
        const auto* voices = requiredChild(entry, "voices", source);
        for (auto* voice = voices->FirstChildElement(); voice != nullptr;
             voice = voice->NextSiblingElement())
        {
            const std::string voiceSource =
                source + "/" + voice->Value();
            CommentatorVoice item;
            item.weight = parseFloat(voice, "weight", voiceSource);
            item.startPlayer = parseBool(voice, "sPlayer", voiceSource);
            item.endPlayer = parseBool(voice, "ePlayer", voiceSource);
            item.humanOnly = parseBool(voice, "forHuman", voiceSource);
            item.sound = trim(requiredText(voice, "sound", voiceSource));
            comment.voices.push_back(std::move(item));
        }
        if (comment.voices.empty())
        {
            throw resource::ResourceError(
                source + ": comment contains no voices");
        }
        const auto [stored, inserted] =
            result.commentator.comments.emplace(name, std::move(comment));
        static_cast<void>(stored);
        if (!inserted)
        {
            throw resource::ResourceError(
                source + ": duplicate comment name");
        }
    }
    if (result.commentator.comments.empty())
    {
        throw resource::ResourceError(
            "game.xml/commentator/comments is empty");
    }
    return result;
}

const Language* findLanguage(const Catalog& catalog,
                             std::string_view name) noexcept
{
    for (const auto& language : catalog.languages)
    {
        if (language.name == name)
            return &language;
    }
    return nullptr;
}

std::size_t StringLibrary::size() const noexcept
{
    return strings_.size();
}

std::string StringLibrary::get(std::string_view id) const
{
    const auto found = strings_.find(id);
    if (found != strings_.end() && !found->second.empty())
        return found->second;
    return std::string(id);
}

void StringLibrary::set(std::string id, std::string value)
{
    strings_[std::move(id)] = std::move(value);
}

bool StringLibrary::has(std::string_view id) const
{
    const auto found = strings_.find(id);
    return found != strings_.end() && !found->second.empty();
}

StringLibrary loadOriginalStringLibrary(
    const resource::ResourceFileSystem& resources,
    const Language& language)
{
    const auto decoded =
        decodeUtf16Le(resources.readBinary(language.file), language.file);
    StringLibrary strings;
    std::istringstream stream(decoded);
    std::string id;
    while (stream)
    {
        std::string token;
        stream >> token;
        if (token.empty())
            continue;
        if (token.front() != '"')
        {
            id = std::move(token);
            continue;
        }

        token.erase(0, 1);
        const auto quote = token.find('"');
        if (quote != std::string::npos)
        {
            token.erase(quote, 1);
        }
        else
        {
            char value = '\0';
            while (stream.get(value) && value != '"')
                token.push_back(value);
        }
        if (id.empty())
            continue;

        std::size_t escapedNewline = 0U;
        while ((escapedNewline = token.find("\\n", escapedNewline)) !=
               std::string::npos)
        {
            token.replace(escapedNewline, 2U, 1U, '\n');
            ++escapedNewline;
        }
        strings.set(std::move(id), std::move(token));
        id.clear();
    }
    return strings;
}

} // namespace r3d::game::originalgamedata
