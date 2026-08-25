#include "OriginalAudioSpec.h"

#include "resource/ResourceFileSystem.h"

#include <tinyxml.h>

#include <charconv>
#include <string>
#include <string_view>
#include <utility>

namespace r3d::game::originalaudio
{
namespace
{

const TiXmlElement *requiredChild(
	const TiXmlElement *parent, const char *name,
	std::string_view source)
{
	const auto *result =
		parent != nullptr ? parent->FirstChildElement(name) : nullptr;
	if (result == nullptr)
	{
		throw resource::ResourceError(
			std::string(source) + ": missing " + name);
	}
	return result;
}

std::string requiredText(
	const TiXmlElement *parent, const char *name,
	std::string_view source)
{
	const auto *element = requiredChild(parent, name, source);
	const char *value = element->GetText();
	if (value == nullptr)
	{
		throw resource::ResourceError(
			std::string(source) + ": empty " + name);
	}
	return value;
}

std::vector<MusicCatTrack> loadTracks(
	const resource::ResourceFileSystem &resources,
	const TiXmlElement *root, const char *catalogName)
{
	const auto *catalog = requiredChild(root, catalogName, "game.xml");
	const auto *tracks = requiredChild(catalog, "tracks", catalogName);
	std::vector<MusicCatTrack> result;
	for (auto *entry = tracks->FirstChildElement(); entry != nullptr;
	     entry = entry->NextSiblingElement())
	{
		const auto *item = requiredChild(entry, "item", catalogName);
		const char *path = item->Attribute("item");
		if (path == nullptr || *path == '\0')
		{
			throw resource::ResourceError(
				std::string("game.xml/") + catalogName +
				": track has no serialized item reference");
		}
		MusicCatTrack track;
		track.path = path;
		track.name = requiredText(entry, "name", catalogName);
		track.band = requiredText(entry, "band", catalogName);
		const std::string group =
			requiredText(entry, "group", catalogName);
		const auto parsed = std::from_chars(
			group.data(), group.data() + group.size(), track.group);
		if (parsed.ec != std::errc{} ||
		    parsed.ptr != group.data() + group.size())
		{
			throw resource::ResourceError(
				std::string("game.xml/") + catalogName +
				": invalid track group");
		}
		std::string dataPath = "Data\\" + track.path;
		if (!resources.exists(dataPath))
		{
			throw resource::ResourceError(
				std::string("game.xml/") + catalogName +
				": missing " + dataPath);
		}
		result.push_back(std::move(track));
	}
	if (result.empty())
	{
		throw resource::ResourceError(
			std::string("game.xml/") + catalogName +
			": empty track catalog");
	}
	return result;
}

} // namespace

MusicCatalog loadOriginalMusicCatalog(
	const resource::ResourceFileSystem &resources)
{
	TiXmlDocument document;
	const std::string xml = resources.readText("game.xml");
	document.Parse(xml.c_str(), nullptr, TIXML_ENCODING_UTF8);
	if (document.Error())
	{
		throw resource::ResourceError(
			"Cannot parse serialized MusicCat catalog: " +
			std::string(document.ErrorDesc()));
	}
	const auto *root = document.RootElement();
	if (root == nullptr)
		throw resource::ResourceError("game.xml has no root element");

	MusicCatalog result;
	result.menu = loadTracks(resources, root, "menuMusic");
	result.game = loadTracks(resources, root, "gameMusic");
	return result;
}

} // namespace r3d::game::originalaudio
