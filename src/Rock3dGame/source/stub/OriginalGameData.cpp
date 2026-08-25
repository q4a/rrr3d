#include "OriginalGameData.h"

#include "resource/ResourceFileSystem.h"

#include <tinyxml.h>

#include <array>
#include <charconv>
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

} // namespace r3d::game::originalgamedata
