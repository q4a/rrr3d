#include "OriginalRace.h"

#include "OriginalEnvironment.h"
#include "OriginalPlayer.h"
#include "OriginalProfile.h"
#include "OriginalSlot.h"
#include "OriginalTournament.h"
#include "OriginalWeapon.h"
#include "resource/R3DMeshAsset.h"
#include "resource/ResourceFileSystem.h"

#include <tinyxml.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace r3d::game::originalrace
{
namespace
{

TiXmlDocument parseXml(const resource::ResourceFileSystem& resources,
                       std::string_view path)
{
    TiXmlDocument document;
    const std::string text = resources.readText(path);
    document.Parse(text.c_str(), nullptr, TIXML_ENCODING_UTF8);
    if (document.Error() || document.RootElement() == nullptr)
        throw resource::ResourceError(std::string(path) + ": " +
                                      document.ErrorDesc());
    return document;
}

TiXmlElement* child(TiXmlElement* parent, std::string_view path)
{
    std::size_t begin = 0;
    while (parent != nullptr && begin < path.size())
    {
        const auto end = path.find('/', begin);
        const auto name = path.substr(
            begin, end == std::string_view::npos ? path.size() - begin
                                                 : end - begin);
        parent = parent->FirstChildElement(std::string(name).c_str());
        if (end == std::string_view::npos)
            return parent;
        begin = end + 1;
    }
    return parent;
}

TiXmlElement* require(TiXmlElement* parent, std::string_view path,
                      std::string_view source)
{
    if (auto* value = child(parent, path))
        return value;
    throw resource::ResourceError(std::string(source) + ": missing " +
                                  std::string(path));
}

std::string text(TiXmlElement* parent, std::string_view path,
                 std::string_view source)
{
    auto* value = require(parent, path, source);
    const char* content = value->GetText();
    if (content == nullptr)
        throw resource::ResourceError(std::string(source) + ": empty " +
                                      std::string(path));
    return content;
}

std::string itemAttribute(TiXmlElement* parent, std::string_view path,
                          std::string_view source)
{
    auto* value = require(parent, path, source);
    const char* item = value->Attribute("item");
    if (item == nullptr || *item == '\0')
        throw resource::ResourceError(std::string(source) +
                                      ": missing item on " +
                                      std::string(path));
    return item;
}

float scalar(TiXmlElement* parent, std::string_view path,
             std::string_view source)
{
    const std::string value = text(parent, path, source);
    std::istringstream stream(value);
    float result = 0.0F;
    if (!(stream >> result) || (stream >> std::ws && !stream.eof()))
        throw resource::ResourceError(std::string(source) + ": invalid " +
                                      std::string(path));
    return result;
}

Vec3 vector3(TiXmlElement* parent, std::string_view path,
             std::string_view source)
{
    const std::string value = text(parent, path, source);
    std::istringstream stream(value);
    Vec3 result;
    if (!(stream >> result.x >> result.y >> result.z) ||
        (stream >> std::ws && !stream.eof()))
        throw resource::ResourceError(std::string(source) + ": invalid " +
                                      std::string(path));
    return result;
}

std::array<float, 4> vector4(TiXmlElement* parent,
                            std::string_view path,
                            std::string_view source)
{
    const std::string value = text(parent, path, source);
    std::istringstream stream(value);
    std::array<float, 4> result{};
    if (!(stream >> result[0] >> result[1] >> result[2] >> result[3]) ||
        (stream >> std::ws && !stream.eof()))
        throw resource::ResourceError(std::string(source) + ": invalid " +
                                      std::string(path));
    return result;
}

std::array<float, 2> vector2(TiXmlElement* parent, std::string_view path,
                            std::string_view source)
{
    const std::string value = text(parent, path, source);
    std::istringstream stream(value);
    std::array<float, 2> result{};
    if (!(stream >> result[0] >> result[1]) ||
        (stream >> std::ws && !stream.eof()))
        throw resource::ResourceError(std::string(source) + ": invalid " +
                                      std::string(path));
    return result;
}

Quat quaternion(TiXmlElement* parent, std::string_view path,
                std::string_view source)
{
    const std::string value = text(parent, path, source);
    std::istringstream stream(value);
    Quat result;
    if (!(stream >> result.x >> result.y >> result.z >> result.w) ||
        (stream >> std::ws && !stream.eof()))
        throw resource::ResourceError(std::string(source) + ": invalid " +
                                      std::string(path));
    return result;
}

bool boolean(TiXmlElement* parent, std::string_view path,
             std::string_view source)
{
    const auto value = text(parent, path, source);
    if (value == "true")
        return true;
    if (value == "false")
        return false;
    throw resource::ResourceError(std::string(source) + ": invalid " +
                                  std::string(path));
}

std::string dataPath(std::string_view legacy)
{
    std::string result;
    if (legacy.rfind("Data\\", 0) != 0 && legacy.rfind("Data/", 0) != 0)
        result = "Data/";
    for (const char value : legacy)
        result.push_back(value == '\\' ? '/' : value);
    return result;
}

std::string canonicalDataPath(
    const resource::ResourceFileSystem& resources, std::string_view legacy)
{
    const std::string requested = dataPath(legacy);
    if (resources.exists(requested))
        return requested;

    auto lower = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char character) {
                           return static_cast<char>(
                               std::tolower(character));
                       });
        return value;
    };

    std::filesystem::path current = resources.root();
    std::filesystem::path corrected;
    for (const auto& component : std::filesystem::path(requested))
    {
        if (component == ".")
            continue;

        const std::string expected = component.string();
        std::optional<std::filesystem::path> exact;
        std::optional<std::filesystem::path> insensitive;
        std::error_code error;
        for (const auto& entry :
             std::filesystem::directory_iterator(current, error))
        {
            const auto filename = entry.path().filename();
            if (filename == component)
            {
                exact = filename;
                break;
            }
            if (lower(filename.string()) == lower(expected))
            {
                if (insensitive)
                    return requested;
                insensitive = filename;
            }
        }
        if (error)
            return requested;
        const auto matched = exact ? exact : insensitive;
        if (!matched)
            return requested;
        corrected /= *matched;
        current /= *matched;
    }

    const std::string result = corrected.generic_string();
    return resources.exists(result) ? result : requested;
}

std::string recordTail(std::string_view record)
{
    const auto root = record.find("root\\");
    if (root == std::string_view::npos)
        throw resource::ResourceError("Unsupported database record: " +
                                      std::string(record));
    return std::string(record.substr(root + 5));
}

TiXmlElement* databaseRecord(TiXmlElement* database,
                             std::string_view record)
{
    std::string path = recordTail(record);
    std::replace(path.begin(), path.end(), '\\', '/');
    return require(database, path, "db.xml");
}

std::string basename(std::string_view record)
{
    const auto slash = record.find_last_of("\\/");
    return std::string(record.substr(slash == std::string_view::npos
                                         ? 0
                                         : slash + 1));
}

std::uint32_t unsignedValue(std::string_view value,
                            std::string_view source);
float optionalScalar(TiXmlElement* parent, std::string_view path,
                     float fallback);
bool optionalBoolean(TiXmlElement* parent, std::string_view path,
                     bool fallback);

void applyMobilityLoadout(
    Vehicle& vehicle,
    const std::vector<OriginalWorkshopItem>& workshop,
    const std::vector<RacerSlot>& loadout,
    std::string_view difficulty, bool humanOrOpponent,
    bool armor4Opened = false)
{
    source::Player player;
    player.BindSlots(workshop, loadout);
    player.ApplyMobility(
        vehicle, difficulty, humanOrOpponent, armor4Opened);
}

void appendPlayerIdentities(
    Race& race, const resource::ResourceFileSystem& resources,
    TiXmlElement* players, int planetIndex)
{
    if (players == nullptr)
        return;
    for (auto* player = players->FirstChildElement();
         player != nullptr; player = player->NextSiblingElement())
    {
        PlayerIdentity identity;
        identity.id = static_cast<int>(unsignedValue(
            text(player, "id", "tournamet.xml/player identity"),
            "tournamet.xml/player identity/id"));
        identity.name =
            text(player, "name", "tournamet.xml/player identity");
        identity.planetIndex = planetIndex;
        if (auto* photo = child(player, "photo");
            photo != nullptr && photo->Attribute("item") != nullptr)
        {
            identity.photoPath = canonicalDataPath(
                resources, photo->Attribute("item"));
        }
        race.playerIdentities.push_back(std::move(identity));
    }
}

void loadPlayerIdentities(
    Race& race, const resource::ResourceFileSystem& resources,
    TiXmlElement* tournament)
{
    race.playerIdentities.clear();
    if (auto* gamers = child(tournament, "gamers"))
    {
        for (auto* gamer = gamers->FirstChildElement(); gamer != nullptr;
             gamer = gamer->NextSiblingElement())
        {
            appendPlayerIdentities(
                race, resources, child(gamer, "players"), -1);
        }
    }
    if (auto* planets = child(tournament, "planets"))
    {
        int planetIndex = 0;
        for (auto* planet = planets->FirstChildElement(); planet != nullptr;
             planet = planet->NextSiblingElement(), ++planetIndex)
        {
            appendPlayerIdentities(
                race, resources, child(planet, "players"), planetIndex);
        }
    }
}

constexpr std::array<std::array<float, 4>, 8> sourcePlayerColors{{
    {91.0F / 255.0F, 41.0F / 255.0F, 165.0F / 255.0F, 1.0F},
    {158.0F / 255.0F, 158.0F / 255.0F, 158.0F / 255.0F, 1.0F},
    {1.0F, 128.0F / 255.0F, 192.0F / 255.0F, 1.0F},
    {131.0F / 255.0F, 247.0F / 255.0F, 204.0F / 255.0F, 1.0F},
    {131.0F / 255.0F, 229.0F / 255.0F, 0.0F, 1.0F},
    {216.0F / 255.0F, 229.0F / 255.0F, 133.0F / 255.0F, 1.0F},
    {97.0F / 255.0F, 0.0F, 185.0F / 255.0F, 1.0F},
    {0.0F, 108.0F / 255.0F, 164.0F / 255.0F, 1.0F},
}};

const std::array<float, 4>& sourcePlayerColor(
    std::size_t computerIndex) noexcept
{
    return sourcePlayerColors[computerIndex % sourcePlayerColors.size()];
}

void selectRacers(Race& race,
                  const resource::ResourceFileSystem& resources,
                  TiXmlElement* planet,
                  std::uint32_t racePass,
                  std::string_view humanRecord)
{
    std::unordered_map<std::string, std::size_t> vehicleIndices;
    for (std::size_t index = 0; index < race.vehicles.size(); ++index)
        vehicleIndices.emplace(race.vehicles[index].record, index);

    const auto humanVehicle =
        vehicleIndices.find(std::string(humanRecord));
    if (humanVehicle == vehicleIndices.end())
        throw resource::ResourceError(
            "garage.xml: tournament player car is missing");

    race.racers.clear();
    Racer human;
    human.name = "Human";
    human.playerId = source::Player::humanId;
    human.vehicle = humanVehicle->second;
    human.human = true;
    race.racers.push_back(std::move(human));
    race.racers.back().configuredVehicle =
        race.vehicles[humanVehicle->second];
    race.racers.back().hasConfiguredVehicle = true;
    source::Planet sourcePlanet;
    auto* opponentEntries =
        require(planet, "players", "tournamet.xml/planet");
    for (auto* opponent = opponentEntries->FirstChildElement();
         opponent != nullptr; opponent = opponent->NextSiblingElement())
    {
        source::Planet::PlayerData player;
        player.id = static_cast<int>(unsignedValue(
            text(opponent, "id", "tournamet.xml/player"),
            "tournamet.xml/player/id"));
        player.name = text(opponent, "name", "tournamet.xml/player");
        player.maxPass = static_cast<int>(unsignedValue(
            text(opponent, "maxPass", "tournamet.xml/player"),
            "tournamet.xml/player/maxPass"));
        if (auto* bonus = child(opponent, "bonus");
            bonus != nullptr && bonus->GetText() != nullptr)
        {
            player.bonus = bonus->GetText();
        }
        if (auto* photo = child(opponent, "photo");
            photo != nullptr && photo->Attribute("item") != nullptr)
        {
            player.photoPath =
                canonicalDataPath(resources, photo->Attribute("item"));
        }
        auto* choices = require(
            opponent, "cars", "tournamet.xml/player");
        for (auto* choice = choices->FirstChildElement();
             choice != nullptr; choice = choice->NextSiblingElement())
        {
            player.cars.push_back(
                {text(choice, "record", "tournamet.xml/player car"),
                 static_cast<int>(unsignedValue(
                     text(choice, "pass", "tournamet.xml/player car"),
                     "tournamet.xml/player car/pass"))});
        }
        if (auto* slots = child(opponent, "slots"))
        {
            for (auto* slot = slots->FirstChildElement(); slot != nullptr;
                 slot = slot->NextSiblingElement())
            {
                player.slots.push_back(
                    {text(slot, "record", "tournamet.xml/player slot"),
                     unsignedValue(
                         text(slot, "charge",
                              "tournamet.xml/player slot"),
                         "tournamet.xml/player slot/charge"),
                     text(slot, "type", "tournamet.xml/player slot"),
                     static_cast<int>(unsignedValue(
                         text(slot, "pass", "tournamet.xml/player slot"),
                         "tournamet.xml/player slot/pass"))});
            }
        }
        sourcePlanet.InsertPlayer(std::move(player));
    }

    for (const auto& player : sourcePlanet.GetPlayers())
    {
        source::Planet::PlayerState state;
        state.id = player.id;
        state.computer = true;
        sourcePlanet.StartPass(
            static_cast<int>(racePass), state, true);
        if (state.car.empty())
            continue;
        const auto vehicle = vehicleIndices.find(state.car);
        if (vehicle == vehicleIndices.end())
            throw resource::ResourceError(
                "garage.xml: AI tournament car is missing: " + state.car);
        Racer racer;
        racer.playerId = static_cast<int>(race.racers.size());
        racer.gamerId = static_cast<std::uint32_t>(player.id);
        racer.name = player.name;
        racer.photoPath = player.photoPath;
        racer.vehicle = vehicle->second;
        racer.configuredVehicle = race.vehicles[vehicle->second];
        racer.hasConfiguredVehicle = true;
        racer.loadout.reserve(state.slots.size());
        for (const auto& slot : state.slots)
        {
            racer.loadout.push_back(
                {slot.record, slot.type, slot.charge});
        }
        race.racers.push_back(std::move(racer));
    }
    if (race.racers.size() < 2)
        throw resource::ResourceError(
            "tournamet.xml: selected race has no AI opponents");
    for (std::size_t index = 1; index < race.racers.size(); ++index)
        race.racers[index].color =
            sourcePlayerColor(index - 1U);
    race.computerTemplates.assign(
        race.racers.begin() + 1, race.racers.end());
}

MaterialDefinition materialDefinition(
    const resource::ResourceFileSystem& resources, std::string_view legacy,
    std::vector<MaterialDefinition>* catalog = nullptr)
{
    const std::string record(legacy);
    auto tune = [&](MaterialDefinition material) {
        auto setAlphaRange = [&](float minimum, float maximum) {
            material.alphaMinimum = minimum;
            material.alphaMaximum = maximum;
        };
        auto setColorRange = [&material](
                                 std::array<float, 4> minimum,
                                 std::array<float, 4> maximum) {
            material.color = minimum;
            material.colorMaximum = maximum;
        };
        auto setColor = [&](std::array<float, 4> value) {
            setColorRange(value, value);
        };
        if (material.blend == MaterialBlend::AlphaTest)
        {
            // ResourceManager::cAlphaTestRef is 0.933, but the D3D9
            // Material::Apply path writes (1 - alphaRef) * 255 to
            // D3DRS_ALPHAREF and uses D3DCMP_GREATEREQUAL.  Store the
            // resulting normalized comparison threshold used by Metal.
            material.alphaReference = 1.0F - 0.933F;
        }
        static constexpr std::string_view carMaterials[] = {
            "Car\\marauder", "Car\\buggi", "Car\\dirtdevil",
            "Car\\tankchetti", "Car\\manticora", "Car\\airblade",
            "Car\\guseniza", "Car\\gusenizaBoss",
            "Car\\monstertruck", "Car\\podushka",
            "Car\\podushkaBoss", "Car\\monstertruckBoss",
            "Car\\manticoraBoss", "Car\\devildriver",
            "Car\\devildriverBoss", "Car\\mustang", "Car\\xCar",
        };
        if (std::find(
                std::begin(carMaterials), std::end(carMaterials),
                record) != std::end(carMaterials))
        {
            material.specular = 1.0F;
            material.shininess = 64.0F;
        }
        if (record.rfind("Effect\\", 0) == 0)
        {
            // These five records are source 3D materials (sprite=false).
            // All remaining Effect materials disable lighting/Z-write/fog in
            // ComplexMatLib::LoadLibMat because they are sprites.
            const bool litGeometry =
                record == "Effect\\wheel" ||
                record == "Effect\\truba" ||
                record == "Effect\\destrCar" ||
                record == "Effect\\pieces" ||
                record == "Effect\\frost";
            if (!litGeometry)
            {
                material.emissive = 1.0F;
                material.specular = 0.0F;
                material.ignoreFog = true;
                material.writeDepth = false;
            }
            if (record == "Effect\\smoke1")
                setColor({0.25F, 0.25F, 0.25F, 1.0F});
            else if (record == "Effect\\smoke2")
                setColorRange(
                    {0.5F, 0.32F, 0.25F, 1.0F},
                    {0.25F, 0.25F, 0.25F, 1.0F});
            else if (record == "Effect\\flare1" ||
                     record == "Effect\\flare3")
                setColor({1.0F, 0.58F, 0.36F, 1.0F});
            else if (record == "Effect\\flare2" ||
                     record == "Effect\\flare7Red")
                setColor({1.0F, 0.0F, 0.0F, 1.0F});
            else if (record == "Effect\\dust_smoke_06")
                setColor({0.2F, 0.2F, 1.0F, 1.0F});
            else if (record == "Effect\\ExplosionRay" ||
                     record == "Effect\\lens1")
                setColor({0.0F, 0.0F, 1.0F, 1.0F});
            else if (record == "Effect\\ExplosionRing")
                setColor({1.0F, 1.0F, 0.0F, 1.0F});
            else if (record == "Effect\\fireTrail" ||
                     record == "Effect\\fire1" ||
                     record == "Effect\\fire2")
                setColorRange(
                    {1.0F, 1.0F, 1.0F, 1.0F},
                    {1.0F, 0.0F, 0.0F, 1.0F});
            else if (record == "Effect\\thunder1")
                setColorRange(
                    {236.0F / 255.0F, 0.0F,
                     140.0F / 255.0F, 1.0F},
                    {0.0F, 0.0F, 1.0F, 1.0F});
            else if (record == "Effect\\smoke6")
                setColorRange(
                    {1.0F, 1.0F, 1.0F, 1.0F},
                    {0.0F, 0.0F, 0.0F, 1.0F});

            struct AlphaRange
            {
                std::string_view record;
                float minimum;
                float maximum;
            };
            // Exact FloatRange arguments from ResourceManager::LoadEffect.
            static constexpr AlphaRange alphaRanges[] = {
                {"Effect\\frost", 0.5F, 0.0F},
                {"Effect\\smoke1", 0.5F, 0.0F},
                {"Effect\\smoke2", 0.8F, 0.0F},
                {"Effect\\smoke3", 1.0F, 0.0F},
                {"Effect\\smoke7", 1.0F, 0.0F},
                {"Effect\\asphaltMarks", 1.0F, 0.0F},
                {"Effect\\drop", 0.8F, 0.8F},
                {"Effect\\frostRay", 0.0F, 1.0F},
                {"Effect\\frostSmoke", 0.0F, 0.5F},
                {"Effect\\frostHit", 1.0F, 0.0F},
                {"Effect\\crater", 1.0F, 0.0F},
                {"Effect\\flare1", 0.5F, 0.5F},
                {"Effect\\flare2", 0.0F, 1.0F},
                {"Effect\\flare3", 1.0F, 0.0F},
                {"Effect\\blaster", 1.0F, 0.0F},
                {"Effect\\heatTrail", 0.8F, 0.0F},
                {"Effect\\laser3-red2", 0.0F, 1.0F},
                {"Effect\\dust_smoke_06", 1.0F, 0.0F},
                {"Effect\\sonar", 1.0F, 0.0F},
                {"Effect\\ExplosionRay", 1.0F, 0.0F},
                {"Effect\\ExplosionRing", 1.0F, 0.0F},
                {"Effect\\streak1", 1.0F, 0.0F},
                {"Effect\\blink", 1.0F, 0.0F},
                {"Effect\\lightning1", 1.0F, 0.0F},
                {"Effect\\trail1", 1.0F, 0.0F},
                {"Effect\\ring1", 1.0F, 0.0F},
                {"Effect\\ring2", 0.0F, 1.0F},
                {"Effect\\frostLine", 0.0F, 1.0F},
                {"Effect\\firePatron", 1.0F, 0.0F},
                {"Effect\\fireTrail", 1.0F, 0.0F},
                {"Effect\\protonRay", 1.0F, 0.0F},
                {"Effect\\protonRing", 1.0F, 0.0F},
                {"Effect\\thunder1", 1.0F, 0.0F},
                {"Effect\\flareLaser1", 0.7F, 0.0F},
                {"Effect\\flareLaser2", 1.0F, 0.0F},
                {"Effect\\flareLaser3", 1.0F, 0.0F},
                {"Effect\\smoke6", 1.0F, 0.1F},
                {"Effect\\spark1", 1.0F, 0.0F},
                {"Effect\\boomSpark1", 1.0F, 0.0F},
                {"Effect\\boomSpark2", 1.0F, 0.0F},
                {"Effect\\fire1", 1.0F, 0.0F},
                {"Effect\\gunEff2", 1.0F, 0.0F},
                {"Effect\\shield1", 0.0F, 1.0F},
            };
            const auto alphaRange = std::find_if(
                std::begin(alphaRanges), std::end(alphaRanges),
                [&](const AlphaRange& value) {
                    return value.record == record;
                });
            if (alphaRange != std::end(alphaRanges))
                setAlphaRange(
                    alphaRange->minimum, alphaRange->maximum);

            // ResourceManager::LoadImage2dLibMatAnim creates the three
            // shield2 materials with alpha 0.4 and translates the sampler
            // over the node's normalized animation frame.
            if (record == "Effect\\shield2" ||
                record == "Effect\\shield2Hor" ||
                record == "Effect\\shield2Vert")
            {
                setAlphaRange(0.4F, 0.4F);
                if (record != "Effect\\shield2Vert")
                    material.textureOffsetMaximum.x = 1.0F;
                if (record != "Effect\\shield2Hor")
                    material.textureOffsetMaximum.y = 1.0F;
            }
            else if (record == "Effect\\phaserBolt")
            {
                material.textureOffsetMaximum =
                    {1.0F, 1.0F, 1.0F};
            }
            else if (record == "Effect\\frostRay")
            {
                material.textureOffsetMaximum.x = -1.0F;
            }
            else if (record == "Effect\\laserRay")
            {
                material.textureOffsetMaximum.x = -2.5F;
            }
        }
        if (record == "Car\\blend")
        {
            material.blend = MaterialBlend::Additive;
            setAlphaRange(0.7F, 0.7F);
        }
        if (record == "GUI\\question")
        {
            // ResourceManager::LoadGUI creates this mesh material through
            // LoadSpecLibMat rather than the ordinary image-material path.
            material.specular = 1.0F;
            material.shininess = 64.0F;
        }
        // These are the non-Effect records for which ResourceManager passes
        // sprite=true or explicitly disables lighting/fog.  ComplexMatLib
        // also disables Z writes for the five Bonus sprites.
        const bool bonusSprite =
            record == "Bonus\\speedArrow" ||
            record == "Bonus\\strelkaAnim" ||
            record == "Bonus\\lusha" ||
            record == "Bonus\\snowLusha" ||
            record == "Bonus\\hellLusha";
        if (bonusSprite || record == "GUI\\space2")
        {
            material.emissive = 1.0F;
            material.ignoreFog = true;
            if (bonusSprite)
                material.writeDepth = false;
        }
        if (record == "Bonus\\maslo")
        {
            // ResourceManager::LoadBonus builds the oil material manually:
            // transparent/no-Z-write, specular and a second reflection-vector
            // sampler that replaces RGB with maslo_top.
            material.blend = MaterialBlend::Transparency;
            material.writeDepth = false;
            material.specular = 1.0F;
            material.shininess = 64.0F;
            material.reflectionTexturePath = canonicalDataPath(
                resources, "Data/Bonus/maslo_top.dds");
            material.reflectionTextureCoordinates = true;
        }
        struct Atlas
        {
            std::string_view record;
            std::uint16_t columns;
            std::uint16_t rows;
        };
        static constexpr Atlas atlases[] = {
            {"Effect\\smoke6", 4, 1},
            {"Effect\\explosion2", 4, 4},
            {"Effect\\explosion3", 6, 6},
            {"Effect\\explosion4", 7, 7},
            {"Effect\\boom1", 8, 4},
            {"Effect\\boom2", 8, 8},
            {"Effect\\fire1", 4, 3},
            {"Effect\\fire2", 5, 5},
            {"Effect\\gunEff2", 4, 1},
            {"Effect\\engine1", 5, 2},
            {"Effect\\shield1", 5, 2},
            {"Bonus\\strelkaAnim", 3, 2},
        };
        const auto atlas = std::find_if(
            std::begin(atlases), std::end(atlases),
            [&](const Atlas& value) { return value.record == record; });
        if (atlas != std::end(atlases))
        {
            material.atlasColumns = atlas->columns;
            material.atlasRows = atlas->rows;
            if (record == "Effect\\gunEff2")
                material.textureCoordinateMaximum.y = 0.25F;

            // Sampler2d::BuildAnimByOff uses the source image dimensions to
            // inset the atlas region by half a texel.  All animated source
            // materials are DDS files, whose height/width live at offsets
            // 12/16 in the standard header.
            const auto bytes = resources.readBinary(material.texturePath);
            if (bytes.size() < 20U || bytes[0] != 'D' ||
                bytes[1] != 'D' || bytes[2] != 'S' || bytes[3] != ' ')
            {
                throw resource::ResourceError(
                    "Animated original material is not a DDS texture: " +
                    material.texturePath);
            }
            const auto littleEndian32 = [&](std::size_t offset) {
                return static_cast<std::uint32_t>(bytes[offset]) |
                       (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U) |
                       (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U) |
                       (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
            };
            const auto height = littleEndian32(12U);
            const auto width = littleEndian32(16U);
            if (width == 0U || height == 0U)
                throw resource::ResourceError(
                    "Animated original DDS has zero dimensions: " +
                    material.texturePath);
            material.textureCoordinateInset = {
                0.5F / static_cast<float>(width),
                0.5F / static_cast<float>(height), 0.0F};
        }
        // ResourceManager::LoadWorld2 calls LoadBumpLibMat for exactly these
        // two materials. Do not infer bump mapping from filenames.
        if (record == "World2\\Track\\track1" ||
            record == "World2\\Track\\most")
        {
            material.normalTexturePath = canonicalDataPath(
                resources,
                record == "World2\\Track\\track1"
                    ? "Data/World2/Track/Texture/track1_norm.dds"
                    : "Data/World2/Track/Texture/most_norm.dds");
            material.specular = 0.5F;
            material.shininess = 128.0F;
        }
        return material;
    };
    struct Mapping
    {
        std::string_view record;
        std::string_view texture;
        MaterialBlend blend;
    };
    // Exact exceptions and blending states from ResourceManager.cpp.
    // Direct-name materials are resolved below; this table preserves every
    // material whose texture name or render state differs from that rule.
    static constexpr Mapping mappings[] = {
        {"Effect\\wheel", "Data/Effect/wheel.dds",
         MaterialBlend::Opaque},
        {"Effect\\truba", "Data/Effect/truba.dds",
         MaterialBlend::Opaque},
        {"Effect\\destrCar", "Data/Effect/destrCar.dds",
         MaterialBlend::Opaque},
        {"Effect\\pieces", "Data/Effect/pieces.dds",
         MaterialBlend::Opaque},
        {"Effect\\j_swell", "Data/Effect/j_swell.dds",
         MaterialBlend::Opaque},
        {"GUI\\question", "Data/GUI/question.png",
         MaterialBlend::Opaque},
        {"GUI\\garage1", "Data/GUI/garage1.dds",
         MaterialBlend::Opaque},
        {"GUI\\garage2", "Data/GUI/garage2.dds",
         MaterialBlend::Opaque},
        {"GUI\\angar1", "Data/GUI/angar1.dds",
         MaterialBlend::Opaque},
        {"GUI\\angar2", "Data/GUI/angar2.dds",
         MaterialBlend::Opaque},
        {"World1\\Track\\track1", "Data/World1/Track/Texture/track1.dds",
         MaterialBlend::Opaque},
        {"World1\\Track\\most", "Data/World1/Track/Texture/most.dds",
         MaterialBlend::Opaque},
        {"World1\\stone", "Data/World1/Texture/stone.dds",
         MaterialBlend::Opaque},
        {"World1\\wand", "Data/World1/Texture/wand.dds",
         MaterialBlend::Opaque},
        {"World1\\zdanie1", "Data/World1/Texture/zdanie1.dds",
         MaterialBlend::AlphaTest},
        {"World1\\zdanie2", "Data/World1/Texture/zdanie2.dds",
         MaterialBlend::AlphaTest},
        {"World1\\berge", "Data/World1/Texture/berge.dds",
         MaterialBlend::AlphaTest},
        {"World1\\eldertree", "Data/World1/Texture/eldertree.dds",
         MaterialBlend::AlphaTest},
        {"World1\\lowpalma", "Data/World1/Texture/lowpalma.dds",
         MaterialBlend::AlphaTest},
        {"World1\\naves", "Data/World1/Texture/naves.dds",
         MaterialBlend::AlphaTest},
        {"World1\\palma", "Data/World1/Texture/palma.dds",
         MaterialBlend::AlphaTest},
        {"World1\\paporotnik", "Data/World1/Texture/paporotnik.dds",
         MaterialBlend::AlphaTest},
        {"World2\\semaphore", "Data/World2/texture/semaphore.dds",
         MaterialBlend::Opaque},
        {"World2\\elka", "Data/World2/Texture/elka.dds",
         MaterialBlend::AlphaTest},
        {"World2\\deadtree1", "Data/World2/Texture/deadtree1.dds",
         MaterialBlend::AlphaTest},
        {"World2\\deadtree2", "Data/World2/Texture/deadtree2.dds",
         MaterialBlend::AlphaTest},
        {"World2\\poplar1", "Data/World2/Texture/poplar1.dds",
         MaterialBlend::AlphaTest},
        {"World3\\stone", "Data/World3/Texture/stone.dds",
         MaterialBlend::Opaque},
        {"World3\\naves", "Data/World3/Track/Texture/track1.dds",
         MaterialBlend::Opaque},
        {"World3\\Track\\track",
         "Data/World3/Track/Texture/track1.dds",
         MaterialBlend::Opaque},
        {"World3\\grass", "Data/World3/Texture/grass.dds",
         MaterialBlend::AlphaTest},
        {"World4\\Track\\most2",
         "Data/World4/Track/Texture/most2.dds",
         MaterialBlend::AlphaTest},
        {"World5\\cannon", "Data/World5/Texture/cannon.dds",
         MaterialBlend::AlphaTest},
        {"World5\\cannon3", "Data/World5/Texture/cannon3.dds",
         MaterialBlend::AlphaTest},
        {"World5\\mountain", "Data/World5/Texture/mountain.dds",
         MaterialBlend::AlphaTest},
        {"World5\\naves", "Data/World5/Texture/naves.dds",
         MaterialBlend::AlphaTest},
        {"World5\\snowstone", "Data/World5/Texture/snowstone.dds",
         MaterialBlend::AlphaTest},
        {"World5\\treeSnow", "Data/World5/Texture/treeSnow.dds",
         MaterialBlend::AlphaTest},
        {"World5\\Track\\track1",
         "Data/World5/Track/Texture/track1.dds",
         MaterialBlend::AlphaTest},
        {"Crush\\pregrada", "Data/Crush/pregrada.dds",
         MaterialBlend::Opaque},
        {"Crush\\crush1", "Data/Crush/crush1.dds",
         MaterialBlend::Opaque},
        {"Crush\\crush2", "Data/Crush/crush2.dds",
         MaterialBlend::Opaque},
        {"Crush\\bochka", "Data/Crush/bochka.dds",
         MaterialBlend::Opaque},
        {"Crush\\reklama", "Data/Crush/reklama.dds",
         MaterialBlend::Opaque},
        {"Crush\\reklama2", "Data/Crush/reklama2.dds",
         MaterialBlend::Opaque},
        {"Crush\\reklama3", "Data/Crush/reklama3.dds",
         MaterialBlend::Opaque},
        {"Crush\\znak", "Data/Crush/znak.dds", MaterialBlend::Opaque},
        {"Crush\\box", "Data/Crush/box.dds", MaterialBlend::Opaque},
        {"Car\\marauderWheel", "Data/Car/marauder.dds",
         MaterialBlend::Opaque},
        {"Car\\buggiWheel", "Data/Car/buggi.dds",
         MaterialBlend::Opaque},
        {"Car\\dirtdevilWheel", "Data/Car/dirtdevil.dds",
         MaterialBlend::Opaque},
        {"Car\\tankchettiWheel", "Data/Car/tankchetti.dds",
         MaterialBlend::Opaque},
        {"Car\\manticoraWheel", "Data/Car/manticora.dds",
         MaterialBlend::Opaque},
        {"Car\\airbladeWheel", "Data/Car/airblade.dds",
         MaterialBlend::Opaque},
        {"Car\\monstertruckWheel", "Data/Car/monstertruck.dds",
         MaterialBlend::Opaque},
        {"Car\\monstertruckBossWheel",
         "Data/Car/monstertruckBoss.dds", MaterialBlend::Opaque},
        {"Car\\manticoraBossWheel", "Data/Car/manticoraBoss.dds",
         MaterialBlend::Opaque},
        {"Car\\mustangWheel", "Data/Car/mustang.dds",
         MaterialBlend::Opaque},
        {"Car\\gusenizaChain", "Data/Car/gusenizaChain.dds",
         MaterialBlend::AlphaTest},
        {"Weapon\\bulletGun", "Data/Car/marauder.dds",
         MaterialBlend::Opaque},
        {"Weapon\\blasterGun", "Data/Car/manticora.dds",
         MaterialBlend::Opaque},
        {"Weapon\\rifleWeapon", "Data/Car/dirtdevil.dds",
         MaterialBlend::Opaque},
        {"Weapon\\airWeapon", "Data/Car/airblade.dds",
         MaterialBlend::Opaque},
        {"Weapon\\asyncFrost", "Data/Car/monstertruck.dds",
         MaterialBlend::Opaque},
        {"Weapon\\asyncFrost2", "Data/Car/monstertruckBoss.dds",
         MaterialBlend::Opaque},
        {"Effect\\frostHit", "Data/Effect/frostSmoke.dds",
         MaterialBlend::Transparency},
        {"Effect\\flare2", "Data/Effect/flare1.dds",
         MaterialBlend::Additive},
        {"Effect\\flare3", "Data/Effect/flare1.dds",
         MaterialBlend::Additive},
        {"Effect\\flare4", "Data/Effect/flare2.dds",
         MaterialBlend::Additive},
        {"Effect\\flare5", "Data/Effect/flare3.dds",
         MaterialBlend::Additive},
        {"Effect\\flare6", "Data/Effect/flare4.dds",
         MaterialBlend::Additive},
        {"Effect\\flare7Red", "Data/Effect/flare5.dds",
         MaterialBlend::Additive},
        {"Effect\\flare7White", "Data/Effect/flare5.dds",
         MaterialBlend::Additive},
        {"Effect\\dust_smoke_06", "Data/Effect/flare1.dds",
         MaterialBlend::Additive},
        {"Effect\\flareLaser1", "Data/Effect/flare2b.dds",
         MaterialBlend::Additive},
        {"Effect\\flareLaser2", "Data/Effect/flare1_tc.dds",
         MaterialBlend::Additive},
        {"Effect\\flareLaser3", "Data/Effect/flare2a.dds",
         MaterialBlend::Additive},
        {"Effect\\boom1", "Data/Effect/fireblast09anim2.dds",
         MaterialBlend::Transparency},
        {"Effect\\boom2", "Data/Effect/blueboom1_add.dds",
         MaterialBlend::Transparency},
        {"Effect\\boomSpark1", "Data/Effect/szikra_group_6.dds",
         MaterialBlend::Additive},
        {"Effect\\boomSpark2", "Data/Effect/szikra_group_7.dds",
         MaterialBlend::Additive},
        {"Effect\\shield2Hor", "Data/Effect/shield2.dds",
         MaterialBlend::Additive},
        {"Effect\\shield2Vert", "Data/Effect/shield2.dds",
         MaterialBlend::Additive},
        {"Effect\\phaserBolt", "Data/Effect/shield2.dds",
         MaterialBlend::Additive},
        {"Effect\\laserRay",
         "Data/Effect/lazerbeam1_blue1_blend7b.dds",
         MaterialBlend::Additive},
        {"Bonus\\money", "Data/Bonus/money.dds", MaterialBlend::Opaque},
        {"Bonus\\medpack", "Data/Bonus/medpack.dds",
         MaterialBlend::Opaque},
        {"Bonus\\ammo", "Data/Bonus/ammo.dds", MaterialBlend::Opaque},
        {"Bonus\\shield", "Data/Bonus/shield.dds",
         MaterialBlend::Opaque},
        {"Bonus\\speedArrow", "Data/Bonus/speedArrow.dds",
         MaterialBlend::Transparency},
        {"Bonus\\strelkaAnim", "Data/Bonus/strelkaAnim.dds",
         MaterialBlend::Transparency},
        {"Bonus\\lusha", "Data/Bonus/lusha.dds",
         MaterialBlend::Transparency},
        {"Bonus\\snowLusha", "Data/Bonus/snowLusha.dds",
         MaterialBlend::Transparency},
        {"Bonus\\hellLusha", "Data/Bonus/hellLusha.dds",
         MaterialBlend::Transparency},
        {"Bonus\\maslo", "Data/Bonus/maslo.dds",
         MaterialBlend::Transparency},
        {"Effect\\frostRay", "Data/Effect/frostRay.dds",
         MaterialBlend::Transparency},
        {"GUI\\space2", "Data/GUI/space2.dds",
         MaterialBlend::Opaque},
        {"Car\\airblade", "Data/Car/airblade.dds",
         MaterialBlend::Opaque},
        {"Car\\airbladeCrush", "Data/Car/airbladeCrush.dds",
         MaterialBlend::Opaque},
        {"Car\\buggi", "Data/Car/buggi.dds",
         MaterialBlend::Opaque},
        {"Car\\devildriver", "Data/Car/devildriver.dds",
         MaterialBlend::Opaque},
        {"Car\\devildriverBoss", "Data/Car/devildriverBoss.dds",
         MaterialBlend::Opaque},
        {"Car\\devildriverCrush", "Data/Car/devildriverCrush.dds",
         MaterialBlend::Opaque},
        {"Car\\dirtdevil", "Data/Car/dirtdevil.dds",
         MaterialBlend::Opaque},
        {"Car\\dirtdevilCrush", "Data/Car/dirtdevilCrush.dds",
         MaterialBlend::Opaque},
        {"Car\\guseniza", "Data/Car/guseniza.dds",
         MaterialBlend::Opaque},
        {"Car\\gusenizaBoss", "Data/Car/gusenizaBoss.dds",
         MaterialBlend::Opaque},
        {"Car\\gusenizaCrush", "Data/Car/gusenizaCrush.dds",
         MaterialBlend::Opaque},
        {"Car\\manticora", "Data/Car/manticora.dds",
         MaterialBlend::Opaque},
        {"Car\\manticoraBoss", "Data/Car/manticoraBoss.dds",
         MaterialBlend::Opaque},
        {"Car\\manticoraCrush", "Data/Car/manticoraCrush.dds",
         MaterialBlend::Opaque},
        {"Car\\marauder", "Data/Car/marauder.dds",
         MaterialBlend::Opaque},
        {"Car\\marauderCrush", "Data/Car/marauderCrush.dds",
         MaterialBlend::Opaque},
        {"Car\\monstertruck", "Data/Car/monstertruck.dds",
         MaterialBlend::Opaque},
        {"Car\\monstertruckBoss", "Data/Car/monstertruckBoss.dds",
         MaterialBlend::Opaque},
        {"Car\\monstertruckCrush", "Data/Car/monstertruckCrush.dds",
         MaterialBlend::Opaque},
        {"Car\\mustang", "Data/Car/mustang.dds",
         MaterialBlend::Opaque},
        {"Car\\podushka", "Data/Car/podushka.dds",
         MaterialBlend::Opaque},
        {"Car\\podushkaBoss", "Data/Car/podushkaBoss.dds",
         MaterialBlend::Opaque},
        {"Car\\podushkaCrush", "Data/Car/podushkaCrush.dds",
         MaterialBlend::Opaque},
        {"Car\\tankchetti", "Data/Car/tankchetti.dds",
         MaterialBlend::Opaque},
        {"Car\\xCar", "Data/Car/xCar.dds",
         MaterialBlend::Opaque},
        {"Effect\\ExplosionRay", "Data/Effect/ExplosionRay.dds",
         MaterialBlend::Additive},
        {"Effect\\ExplosionRing", "Data/Effect/ExplosionRing.dds",
         MaterialBlend::Additive},
        {"Effect\\asphaltMarks", "Data/Effect/asphaltMarks.dds",
         MaterialBlend::Transparency},
        {"Effect\\blaster", "Data/Effect/blaster.dds",
         MaterialBlend::Additive},
        {"Effect\\blaster2", "Data/Effect/blaster2.dds",
         MaterialBlend::Additive},
        {"Effect\\blink", "Data/Effect/blink.dds",
         MaterialBlend::Additive},
        {"Effect\\bullet", "Data/Effect/bullet.dds",
         MaterialBlend::Additive},
        {"Effect\\crater", "Data/Effect/crater.dds",
         MaterialBlend::Transparency},
        {"Effect\\drop", "Data/Effect/drop.dds",
         MaterialBlend::Transparency},
        {"Effect\\engine1", "Data/Effect/engine1.dds",
         MaterialBlend::Additive},
        {"Effect\\explosion2", "Data/Effect/explosion2.dds",
         MaterialBlend::Additive},
        {"Effect\\explosion3", "Data/Effect/explosion3.dds",
         MaterialBlend::Additive},
        {"Effect\\explosion4", "Data/Effect/explosion4.dds",
         MaterialBlend::Additive},
        {"Effect\\fire1", "Data/Effect/fire1.dds",
         MaterialBlend::Additive},
        {"Effect\\fire2", "Data/Effect/fire2.dds",
         MaterialBlend::Additive},
        {"Effect\\firePatron", "Data/Effect/firePatron.dds",
         MaterialBlend::Additive},
        {"Effect\\fireTrail", "Data/Effect/fireTrail.dds",
         MaterialBlend::Additive},
        {"Effect\\flare1", "Data/Effect/flare1.dds",
         MaterialBlend::Additive},
        {"Effect\\flash1", "Data/Effect/flash1.dds",
         MaterialBlend::Additive},
        {"Effect\\flash2", "Data/Effect/flash2.dds",
         MaterialBlend::Additive},
        {"Effect\\frost", "Data/Effect/frost.dds",
         MaterialBlend::Transparency},
        {"Effect\\frostLine", "Data/Effect/frostLine.dds",
         MaterialBlend::Additive},
        {"Effect\\frostSmoke", "Data/Effect/frostSmoke.dds",
         MaterialBlend::Transparency},
        {"Effect\\gunEff2", "Data/Effect/gunEff2.dds",
         MaterialBlend::Additive},
        {"Effect\\heatTrail", "Data/Effect/heatTrail.dds",
         MaterialBlend::Additive},
        {"Effect\\laser3-blue", "Data/Effect/laser3-blue.dds",
         MaterialBlend::Additive},
        {"Effect\\laser3-red2", "Data/Effect/laser3-red2.dds",
         MaterialBlend::Additive},
        {"Effect\\lens1", "Data/Effect/lens1.dds",
         MaterialBlend::Additive},
        {"Effect\\lightning1", "Data/Effect/lightning1.dds",
         MaterialBlend::Additive},
        {"Effect\\phaseRing", "Data/Effect/phaseRing.dds",
         MaterialBlend::Additive},
        {"Effect\\protonRay", "Data/Effect/protonRay.dds",
         MaterialBlend::Additive},
        {"Effect\\protonRing", "Data/Effect/protonRing.dds",
         MaterialBlend::Additive},
        {"Effect\\rad_add", "Data/Effect/rad_add.dds",
         MaterialBlend::Additive},
        {"Effect\\ring1", "Data/Effect/ring1.dds",
         MaterialBlend::Additive},
        {"Effect\\ring2", "Data/Effect/ring2.dds",
         MaterialBlend::Additive},
        {"Effect\\shield1", "Data/Effect/shield1.dds",
         MaterialBlend::Additive},
        {"Effect\\shield2", "Data/Effect/shield2.dds",
         MaterialBlend::Additive},
        {"Effect\\smoke1", "Data/Effect/smoke1.dds",
         MaterialBlend::Transparency},
        {"Effect\\smoke2", "Data/Effect/smoke2.dds",
         MaterialBlend::Transparency},
        {"Effect\\smoke3", "Data/Effect/smoke3.dds",
         MaterialBlend::Transparency},
        {"Effect\\smoke6", "Data/Effect/smoke6.dds",
         MaterialBlend::Transparency},
        {"Effect\\smoke7", "Data/Effect/smoke7.dds",
         MaterialBlend::Transparency},
        {"Effect\\sonar", "Data/Effect/sonar.dds",
         MaterialBlend::Additive},
        {"Effect\\spark1", "Data/Effect/spark1.dds",
         MaterialBlend::Additive},
        {"Effect\\streak1", "Data/Effect/streak1.dds",
         MaterialBlend::Additive},
        {"Effect\\thunder1", "Data/Effect/thunder1.dds",
         MaterialBlend::Additive},
        {"Effect\\trail1", "Data/Effect/trail1.dds",
         MaterialBlend::Additive},
        {"Weapon\\blaster1", "Data/Weapon/blaster1.dds",
         MaterialBlend::Opaque},
        {"Weapon\\drobilka", "Data/Weapon/drobilka.dds",
         MaterialBlend::Opaque},
        {"Weapon\\droid", "Data/Weapon/droid.dds",
         MaterialBlend::Opaque},
        {"Weapon\\fireGun", "Data/Weapon/fireGun.dds",
         MaterialBlend::Opaque},
        {"Weapon\\gun", "Data/Weapon/gun.dds",
         MaterialBlend::Opaque},
        {"Weapon\\hyperBlaster", "Data/Weapon/hyperBlaster.dds",
         MaterialBlend::Opaque},
        {"Weapon\\hyperdrive", "Data/Weapon/hyperdrive.dds",
         MaterialBlend::Opaque},
        {"Weapon\\maslo", "Data/Weapon/maslo.dds",
         MaterialBlend::Opaque},
        {"Weapon\\mine1", "Data/Weapon/mine1.dds",
         MaterialBlend::Opaque},
        {"Weapon\\mine2", "Data/Weapon/mine2.dds",
         MaterialBlend::Opaque},
        {"Weapon\\mine3", "Data/Weapon/mine3.dds",
         MaterialBlend::Opaque},
        {"Weapon\\mortira", "Data/Weapon/mortira.dds",
         MaterialBlend::Opaque},
        {"Weapon\\phaseImpulse", "Data/Weapon/phaseImpulse.dds",
         MaterialBlend::Opaque},
        {"Weapon\\pulsator", "Data/Weapon/pulsator.dds",
         MaterialBlend::Opaque},
        {"Weapon\\reflector", "Data/Weapon/reflector.dds",
         MaterialBlend::Opaque},
        {"Weapon\\rezonator", "Data/Weapon/rezonator.dds",
         MaterialBlend::Opaque},
        {"Weapon\\rifleProj", "Data/Weapon/rifleProj.dds",
         MaterialBlend::Opaque},
        {"Weapon\\rocket", "Data/Weapon/rocket.dds",
         MaterialBlend::Opaque},
        {"Weapon\\rocketAir", "Data/Weapon/rocketAir.dds",
         MaterialBlend::Opaque},
        {"Weapon\\rocketLauncher", "Data/Weapon/rocketLauncher.dds",
         MaterialBlend::Opaque},
        {"Weapon\\shotBall", "Data/Weapon/shotBall.dds",
         MaterialBlend::Opaque},
        {"Weapon\\spring", "Data/Weapon/spring.dds",
         MaterialBlend::Opaque},
        {"Weapon\\tankLaser", "Data/Weapon/tankLaser.dds",
         MaterialBlend::Opaque},
        {"Weapon\\torpeda", "Data/Weapon/torpeda.dds",
         MaterialBlend::Opaque},
        {"Weapon\\turel", "Data/Weapon/turel.dds",
         MaterialBlend::Opaque},
        {"World2\\Haus3", "Data/World2/Texture/Haus3.dds",
         MaterialBlend::Opaque},
        {"World2\\Track\\most", "Data/World2/Track/Texture/most.dds",
         MaterialBlend::Opaque},
        {"World2\\Track\\track1", "Data/World2/Track/Texture/track1.dds",
         MaterialBlend::Opaque},
        {"World2\\atom", "Data/World2/Texture/atom.dds",
         MaterialBlend::Opaque},
        {"World2\\bochki", "Data/World2/Texture/bochki.dds",
         MaterialBlend::Opaque},
        {"World2\\deadtree3", "Data/World2/Texture/deadtree3.dds",
         MaterialBlend::Opaque},
        {"World2\\factory", "Data/World2/Texture/factory.dds",
         MaterialBlend::Opaque},
        {"World2\\haus1", "Data/World2/Texture/haus1.dds",
         MaterialBlend::Opaque},
        {"World2\\isle1", "Data/World2/Texture/isle1.dds",
         MaterialBlend::Opaque},
        {"World2\\machineFactory", "Data/World2/Texture/machineFactory.dds",
         MaterialBlend::Opaque},
        {"World2\\metal1", "Data/World2/Texture/metal1.dds",
         MaterialBlend::Opaque},
        {"World2\\naves1", "Data/World2/Texture/naves1.dds",
         MaterialBlend::Opaque},
        {"World2\\pregrada", "Data/World2/Texture/pregrada.dds",
         MaterialBlend::Opaque},
        {"World2\\projektor", "Data/World2/Texture/projektor.dds",
         MaterialBlend::Opaque},
        {"World2\\pumpjack", "Data/World2/Texture/pumpjack.dds",
         MaterialBlend::Opaque},
        {"World2\\skelet1", "Data/World2/Texture/skelet1.dds",
         MaterialBlend::Opaque},
        {"World2\\strelka1", "Data/World2/Texture/strelka1.dds",
         MaterialBlend::Opaque},
        {"World2\\tramplin1", "Data/World2/Texture/tramplin1.dds",
         MaterialBlend::Opaque},
        {"World2\\truba1", "Data/World2/Texture/truba1.dds",
         MaterialBlend::Opaque},
        {"World2\\truba2", "Data/World2/Texture/truba2.dds",
         MaterialBlend::Opaque},
        {"World2\\truba3", "Data/World2/Texture/truba3.dds",
         MaterialBlend::Opaque},
        {"World2\\truba4", "Data/World2/Texture/truba4.dds",
         MaterialBlend::Opaque},
        {"World3\\Track\\most", "Data/World3/Track/Texture/most.dds",
         MaterialBlend::Opaque},
        {"World3\\fabrika", "Data/World3/Texture/fabrika.dds",
         MaterialBlend::Opaque},
        {"World3\\tower", "Data/World3/Texture/tower.dds",
         MaterialBlend::Opaque},
        {"World3\\tower2", "Data/World3/Texture/tower2.dds",
         MaterialBlend::Opaque},
        {"World3\\ventil1", "Data/World3/Texture/ventil1.dds",
         MaterialBlend::Opaque},
        {"World3\\ventil2", "Data/World3/Texture/ventil2.dds",
         MaterialBlend::Opaque},
        {"World3\\windmil", "Data/World3/Texture/windmil.dds",
         MaterialBlend::Opaque},
        {"World4\\Track\\track1", "Data/World4/Track/Texture/track1.dds",
         MaterialBlend::Opaque},
        {"World4\\Track\\track2", "Data/World4/Track/Texture/track2.dds",
         MaterialBlend::Opaque},
        {"World4\\architect1", "Data/World4/Texture/architect1.dds",
         MaterialBlend::Opaque},
        {"World4\\architect2", "Data/World4/Texture/architect2.dds",
         MaterialBlend::Opaque},
        {"World4\\architect3", "Data/World4/Texture/architect3.dds",
         MaterialBlend::Opaque},
        {"World4\\architect4", "Data/World4/Texture/architect4.dds",
         MaterialBlend::Opaque},
        {"World4\\build", "Data/World4/Texture/build.dds",
         MaterialBlend::Opaque},
        {"World4\\crystals", "Data/World4/Texture/crystals.dds",
         MaterialBlend::Opaque},
        {"World4\\gora1", "Data/World4/Texture/gora1.dds",
         MaterialBlend::Opaque},
        {"World4\\gora2", "Data/World4/Texture/gora2.dds",
         MaterialBlend::Opaque},
        {"World4\\kolba", "Data/World4/Texture/kolba.dds",
         MaterialBlend::Opaque},
        {"World4\\lavaplace", "Data/World4/Texture/lavaplace.dds",
         MaterialBlend::Opaque},
        {"World4\\naves", "Data/World4/Texture/naves.dds",
         MaterialBlend::Opaque},
        {"World4\\pushka", "Data/World4/Texture/pushka.dds",
         MaterialBlend::Opaque},
        {"World4\\volcano", "Data/World4/Texture/volcano.dds",
         MaterialBlend::Opaque},
        {"World5\\Track\\most", "Data/World5/Track/Texture/most.dds",
         MaterialBlend::Opaque},
        {"World5\\cannon2", "Data/World5/Texture/cannon2.dds",
         MaterialBlend::Opaque},
        {"World5\\piece", "Data/World5/Texture/piece.dds",
         MaterialBlend::Opaque},
        {"World5\\snowPlate", "Data/World5/Texture/snowPlate.dds",
         MaterialBlend::Opaque},
        {"World5\\snowstone2", "Data/World5/Texture/snowstone2.dds",
         MaterialBlend::Opaque},
        {"World5\\transportship", "Data/World5/Texture/transportship.dds",
         MaterialBlend::Opaque},
        {"World6\\Track\\tonnel", "Data/World6/Track/Texture/tonnel.dds",
         MaterialBlend::Opaque},
        {"World6\\Track\\track1", "Data/World6/Track/Texture/track1.dds",
         MaterialBlend::Opaque},
        {"World6\\Track\\tramplin3", "Data/World6/Track/Texture/tramplin3.dds",
         MaterialBlend::Opaque},
        {"World6\\haus1", "Data/World6/Texture/haus1.dds",
         MaterialBlend::Opaque},
        {"World6\\haus2", "Data/World6/Texture/haus2.dds",
         MaterialBlend::Opaque},
        {"World6\\haus3", "Data/World6/Texture/haus3.dds",
         MaterialBlend::Opaque},
        {"World6\\haus4", "Data/World6/Texture/haus4.dds",
         MaterialBlend::Opaque},
        {"World6\\naves", "Data/World6/Texture/naves.dds",
         MaterialBlend::Opaque},
        {"World6\\nuke", "Data/World6/Texture/nuke.dds",
         MaterialBlend::Opaque},
        {"World6\\stone", "Data/World6/Texture/stone.dds",
         MaterialBlend::Opaque},
    };
    if (catalog != nullptr)
    {
        catalog->reserve(catalog->size() + std::size(mappings) + 3U);
        for (const auto& mapping : mappings)
            catalog->push_back(
                materialDefinition(resources, mapping.record));
        for (const auto record : {
                 "Effect\\gravBall", "Weapon\\mortiraBall",
                 "Car\\blend"})
        {
            catalog->push_back(materialDefinition(resources, record));
        }
        return {};
    }
    if (record == "Effect\\gravBall" ||
        record == "Weapon\\mortiraBall" ||
        record == "Car\\blend")
    {
        MaterialDefinition material;
        material.record = record;
        material.blend = record == "Car\\blend"
                             ? MaterialBlend::Additive
                             : MaterialBlend::Opaque;
        material.color = record == "Effect\\gravBall"
                             ? std::array<float, 4>{
                                   1.0F, 0.0F, 0.0F, 1.0F}
                         : (record == "Weapon\\mortiraBall"
                                ? std::array<float, 4>{
                                      0.0F, 0.0F, 0.0F, 1.0F}
                                : std::array<float, 4>{
                                      1.0F, 1.0F, 1.0F, 1.0F});
        material.colorMaximum = material.color;
        return tune(material);
    }
    for (const auto& mapping : mappings)
    {
        if (mapping.record == record)
        {
            const auto texture =
                canonicalDataPath(resources, mapping.texture);
            if (!resources.exists(texture))
                throw resource::ResourceError(
                    "Mapped original texture is missing: " +
                    std::string(mapping.texture));
            return tune({record, texture, mapping.blend,
                         mapping.blend == MaterialBlend::AlphaTest
                             ? 0.1F
                             : 0.0F});
        }
    }

    throw resource::ResourceError("No original material mapping for " +
                                  record);
}

Transform elementTransform(TiXmlElement* element, std::string_view source)
{
    Transform result;
    result.position = vector3(element, "pos", source);
    result.scale = vector3(element, "scale", source);
    result.rotation = quaternion(element, "rot", source);
    return result;
}

Vec3 rotate(const Quat& q, Vec3 value)
{
    const Vec3 twiceCross{
        2.0F * (q.y * value.z - q.z * value.y),
        2.0F * (q.z * value.x - q.x * value.z),
        2.0F * (q.x * value.y - q.y * value.x)};
    return {value.x + q.w * twiceCross.x +
                        (q.y * twiceCross.z - q.z * twiceCross.y),
            value.y + q.w * twiceCross.y +
                        (q.z * twiceCross.x - q.x * twiceCross.z),
            value.z + q.w * twiceCross.z +
                        (q.x * twiceCross.y - q.y * twiceCross.x)};
}

Quat multiply(const Quat& first, const Quat& second)
{
    return {first.w * second.x + first.x * second.w +
                first.y * second.z - first.z * second.y,
            first.w * second.y - first.x * second.z +
                first.y * second.w + first.z * second.x,
            first.w * second.z + first.x * second.y -
                first.y * second.x + first.z * second.w,
            first.w * second.w - first.x * second.x -
                first.y * second.y - first.z * second.z};
}

Transform compose(const Transform& parent, const Transform& local)
{
    Transform result;
    const auto offset = rotate(
        parent.rotation,
        {local.position.x * parent.scale.x,
         local.position.y * parent.scale.y,
         local.position.z * parent.scale.z});
    result.position = {parent.position.x + offset.x,
                       parent.position.y + offset.y,
                       parent.position.z + offset.z};
    result.scale = {parent.scale.x * local.scale.x,
                    parent.scale.y * local.scale.y,
                    parent.scale.z * local.scale.z};
    result.rotation = multiply(parent.rotation, local.rotation);
    return result;
}

Quat inverseRotation(const Quat& value)
{
    const float norm = value.x * value.x + value.y * value.y +
                       value.z * value.z + value.w * value.w;
    if (norm <= 0.000001F)
        return {};
    return {-value.x / norm, -value.y / norm, -value.z / norm,
            value.w / norm};
}

Vec3 parentVector(const Transform& parent, const Vec3& value)
{
    return rotate(
        parent.rotation,
        {value.x * parent.scale.x, value.y * parent.scale.y,
         value.z * parent.scale.z});
}

Quat parentRotationVelocity(const Transform& parent, const Quat& value)
{
    // Flatten parent * (delta * child) into
    // (parent * delta * inverse(parent)) * (parent * child).
    return multiply(
        multiply(parent.rotation, value),
        inverseRotation(parent.rotation));
}

void flattenVisualNode(VisualNode& node, const Transform& parent)
{
    node.speedPosition = parentVector(parent, node.speedPosition);
    node.speedScale = {
        node.speedScale.x * parent.scale.x,
        node.speedScale.y * parent.scale.y,
        node.speedScale.z * parent.scale.z};
    node.speedRotation =
        parentRotationVelocity(parent, node.speedRotation);
    node.transform = compose(parent, node.transform);
}

std::vector<VisualNode> visualNodes(
    const resource::ResourceFileSystem& resources, TiXmlElement* record,
    std::string_view source, std::string_view textureOverride = {})
{
    std::vector<VisualNode> result;
    auto* items = child(record, "grActor/nodes/items");
    if (items == nullptr)
        return result;
    bool actorInvertCullFace = false;
    if (auto* invert = child(record, "grActor/invertCullFace");
        invert != nullptr && invert->GetText() != nullptr)
    {
        actorInvertCullFace =
            std::string_view(invert->GetText()) == "true";
    }
    for (auto* item = items->FirstChildElement(); item != nullptr;
         item = item->NextSiblingElement())
    {
        auto* mesh = child(item, "mesh");
        const char* type = item->Attribute("type");
        const bool plane =
            type != nullptr &&
            (std::string_view(type) == "ntPlane" ||
             std::string_view(type) == "ntSprite");
        if (!plane &&
            (mesh == nullptr || mesh->Attribute("item") == nullptr))
            continue;
        VisualNode node;
        node.plane = plane;
        node.billboard =
            type != nullptr && std::string_view(type) == "ntSprite";
        node.invertCullFace = actorInvertCullFace;
        if (auto* invert = child(item, "invertCullFace");
            invert != nullptr && invert->GetText() != nullptr)
        {
            node.invertCullFace =
                node.invertCullFace ||
                std::string_view(invert->GetText()) == "true";
        }
        if (auto* cullMode = child(item, "cullMode");
            cullMode != nullptr && cullMode->GetText() != nullptr)
        {
            unsigned mode = 0U;
            std::istringstream stream(cullMode->GetText());
            stream >> mode;
            switch (mode)
            {
            case 1U:
                node.cullMode = VisualNode::CullMode::None;
                break;
            case 2U:
                node.cullMode = VisualNode::CullMode::Clockwise;
                break;
            case 3U:
                node.cullMode =
                    VisualNode::CullMode::CounterClockwise;
                break;
            default:
                node.cullMode = VisualNode::CullMode::Inherit;
                break;
            }
        }
        node.fixedDirection =
            node.billboard &&
            child(item, "fixDirection") != nullptr &&
            child(item, "fixDirection")->GetText() != nullptr &&
            std::string_view(child(item, "fixDirection")->GetText()) ==
                "true";
        if (!plane)
        {
            node.meshPath =
                canonicalDataPath(resources, mesh->Attribute("item"));
        }
        node.transform = elementTransform(item, source);
        if (auto* mode = child(item, "animMode");
            mode != nullptr && mode->GetText() != nullptr)
        {
            const auto value = unsignedValue(mode->GetText(), source);
            if (value <= static_cast<unsigned>(
                             VisualNode::AnimationMode::Inheritance))
            {
                node.animationMode =
                    static_cast<VisualNode::AnimationMode>(value);
            }
        }
        if (auto* duration = child(item, "animDuration");
            duration != nullptr && duration->GetText() != nullptr)
        {
            node.animationDuration = scalar(item, "animDuration", source);
        }
        if (auto* frame = child(item, "frame");
            frame != nullptr && frame->GetText() != nullptr)
        {
            node.animationFrame = scalar(item, "frame", source);
        }
        if (child(item, "speedPos") != nullptr)
            node.speedPosition = vector3(item, "speedPos", source);
        if (child(item, "speedScale") != nullptr)
            node.speedScale = vector3(item, "speedScale", source);
        if (child(item, "speedRot") != nullptr)
            node.speedRotation = quaternion(item, "speedRot", source);
        if (plane)
        {
            auto* size = child(item, "size");
            if (size == nullptr)
                size = child(item, "sizes");
            if (size != nullptr && size->GetText() != nullptr)
            {
                std::istringstream stream(size->GetText());
                float width = 1.0F;
                float height = 1.0F;
                if (stream >> width >> height)
                {
                    node.transform.scale.x *= width;
                    node.transform.scale.y *= height;
                }
            }
        }
        if (auto* meshId = child(item, "meshId");
            meshId != nullptr && meshId->GetText() != nullptr)
        {
            std::istringstream stream(meshId->GetText());
            stream >> node.subMesh;
        }
        if (auto* tag = child(item, "tag");
            tag != nullptr && tag->GetText() != nullptr)
        {
            std::istringstream stream(tag->GetText());
            stream >> node.tag;
        }
        if (auto* materials = child(item, "materials"))
        {
            for (auto* material = materials->FirstChildElement();
                 material != nullptr;
                 material = material->NextSiblingElement())
            {
                const char* itemName = material->Attribute("item");
                if (itemName != nullptr && *itemName != '\0')
                {
                    auto definition =
                        materialDefinition(resources, itemName);
                    if (!textureOverride.empty() &&
                        !definition.texturePath.empty())
                    {
                        // Player/Garage skins replace the color texture, not
                        // the LibMaterial.  Preserve Car\\blend, per-group
                        // blending, specular state, alpha-test, and all other
                        // source material behavior.
                        definition.texturePath = textureOverride;
                        definition.normalTexturePath.clear();
                        definition.reflectionTexturePath.clear();
                        definition.reflectionTextureCoordinates = false;
                    }
                    node.materials.push_back(std::move(definition));
                }
            }
        }
        if (node.materials.empty() && !textureOverride.empty())
        {
            node.materials.push_back(
                {"", std::string(textureOverride), MaterialBlend::Opaque,
                 0.0F});
        }
        if (node.materials.empty())
            throw resource::ResourceError(std::string(source) +
                                          ": visual node has no material");
        result.push_back(std::move(node));
    }
    return result;
}

float optionalParticleScalar(TiXmlElement* parent,
                             std::string_view path,
                             float fallback,
                             std::string_view source)
{
    auto* item = child(parent, path);
    return item != nullptr && item->GetText() != nullptr
               ? scalar(parent, path, source)
               : fallback;
}

Vec3 optionalParticleVector(TiXmlElement* parent,
                            std::string_view path, Vec3 fallback,
                            std::string_view source)
{
    auto* item = child(parent, path);
    return item != nullptr && item->GetText() != nullptr
               ? vector3(parent, path, source)
               : fallback;
}

Quat optionalParticleQuaternion(TiXmlElement* parent,
                                std::string_view path,
                                Quat fallback,
                                std::string_view source)
{
    auto* item = child(parent, path);
    return item != nullptr && item->GetText() != nullptr
               ? quaternion(parent, path, source)
               : fallback;
}

ParticleDistribution particleDistribution(TiXmlElement* parent,
                                          std::string_view path)
{
    auto* value = child(parent, std::string(path) + "/distrib");
    return value != nullptr && value->GetText() != nullptr &&
                   std::string_view(value->GetText()) == "vdVolume"
               ? ParticleDistribution::Volume
               : ParticleDistribution::Linear;
}

template <std::size_t Size>
std::array<std::uint32_t, Size> particleFrequency(
    TiXmlElement* parent, std::string_view path,
    std::string_view source)
{
    std::array<std::uint32_t, Size> result{};
    result.fill(100U);
    auto* value = child(parent, std::string(path) + "/freq");
    if (value == nullptr || value->GetText() == nullptr)
        return result;
    std::istringstream stream(value->GetText());
    for (auto& component : result)
    {
        if (!(stream >> component) || component == 0U)
            throw resource::ResourceError(
                std::string(source) + ": invalid " +
                std::string(path) + "/freq");
    }
    if (stream >> std::ws && !stream.eof())
        throw resource::ResourceError(
            std::string(source) + ": invalid " +
            std::string(path) + "/freq");
    return result;
}

bool hasBehaviorType(TiXmlElement* record, std::string_view wanted)
{
    auto* behaviors = child(record, "behaviors/items");
    if (behaviors == nullptr)
        return false;
    for (auto* behavior = behaviors->FirstChildElement();
         behavior != nullptr; behavior = behavior->NextSiblingElement())
    {
        const char* type = behavior->Attribute("type");
        if (type != nullptr && std::string_view(type) == wanted)
            return true;
    }
    return false;
}

void appendBehaviorSounds(
    const resource::ResourceFileSystem& resources,
    TiXmlElement* record, std::string_view wanted,
    std::vector<std::string>& output)
{
    auto* behaviors = child(record, "behaviors/items");
    if (behaviors == nullptr)
        return;
    for (auto* behavior = behaviors->FirstChildElement();
         behavior != nullptr; behavior = behavior->NextSiblingElement())
    {
        const char* type = behavior->Attribute("type");
        if (type == nullptr || std::string_view(type) != wanted)
            continue;
        auto* sounds = child(behavior, "sounds");
        if (sounds == nullptr)
            continue;
        for (auto* sound = sounds->FirstChildElement(); sound != nullptr;
             sound = sound->NextSiblingElement())
        {
            const char* path = sound->Attribute("item");
            if (path != nullptr)
                output.push_back(canonicalDataPath(resources, path));
        }
    }
}

float positiveTimeLife(TiXmlElement* record)
{
    return std::max(optionalScalar(record, "maxTimeLife", -1.0F),
                    -1.0F);
}

float earlierPositiveTimeLife(float parent, float child)
{
    if (parent > 0.0F && child > 0.0F)
        return std::min(parent, child);
    return parent > 0.0F ? parent : child;
}

void appendParticleEmitters(
    const resource::ResourceFileSystem& resources,
    TiXmlElement* record, const Transform& parentTransform,
    const Transform& sourceOwnerTransform,
    ObjectDefinition& definition, std::string_view source,
    float ownerMaximumTimeLife, TiXmlElement* nestedNodes = nullptr,
    std::int32_t parentEmitter = -1)
{
    // BehaviorType: 2 = FxSystemWaitingEnd, 3 = FxSystemSrcSpeed.
    // These are properties of the source GameObject, not of the flattened
    // parent definition.
    const bool waitForParticleEnd = hasBehaviorType(record, "2");
    const bool sourceSpeedBehavior = hasBehaviorType(record, "3");
    auto* nodes = nestedNodes != nullptr
                      ? nestedNodes
                      : child(record, "grActor/nodes/items");
    if (nodes == nullptr)
        return;
    for (auto* node = nodes->FirstChildElement(); node != nullptr;
         node = node->NextSiblingElement())
    {
        const Transform nodeTransform =
            compose(parentTransform, elementTransform(node, source));
        const char* type = node->Attribute("type");
        if (type == nullptr ||
            std::string_view(type) != "ntParticleSystem")
        {
            if (auto* childNode = child(node, "child");
                childNode != nullptr)
            {
                if (auto* childNodes =
                        child(childNode, "nodes/items");
                    childNodes != nullptr)
                {
                    appendParticleEmitters(
                        resources, record,
                        compose(nodeTransform,
                                elementTransform(childNode, source)),
                        sourceOwnerTransform,
                        definition, source, ownerMaximumTimeLife,
                        childNodes, parentEmitter);
                }
            }
            continue;
        }
        const Vec3 nodeSpeedPosition =
            child(node, "speedPos") != nullptr
                ? parentVector(
                      parentTransform,
                      vector3(node, "speedPos", source))
                : Vec3{};
        const Vec3 rawNodeSpeedScale =
            child(node, "speedScale") != nullptr
                ? vector3(node, "speedScale", source)
                : Vec3{};
        const Vec3 nodeSpeedScale{
            rawNodeSpeedScale.x * parentTransform.scale.x,
            rawNodeSpeedScale.y * parentTransform.scale.y,
            rawNodeSpeedScale.z * parentTransform.scale.z};
        const Quat nodeSpeedRotation =
            child(node, "speedRot") != nullptr
                ? parentRotationVelocity(
                      parentTransform,
                      quaternion(node, "speedRot", source))
                : Quat{};
        VisualNode::AnimationMode animationMode =
            VisualNode::AnimationMode::None;
        float animationDuration = 1.0F;
        float animationFrame = 0.0F;
        if (auto* mode = child(node, "animMode");
            mode != nullptr && mode->GetText() != nullptr)
        {
            const auto value = unsignedValue(mode->GetText(), source);
            if (value <= static_cast<unsigned>(
                             VisualNode::AnimationMode::Inheritance))
            {
                animationMode =
                    static_cast<VisualNode::AnimationMode>(value);
            }
        }
        if (auto* duration = child(node, "animDuration");
            duration != nullptr && duration->GetText() != nullptr)
        {
            animationDuration =
                scalar(node, "animDuration", source);
        }
        if (auto* frame = child(node, "frame");
            frame != nullptr && frame->GetText() != nullptr)
        {
            animationFrame = scalar(node, "frame", source);
        }
        bool fixedDirection = false;
        ParticleRenderMode renderMode = ParticleRenderMode::Sprite;
        std::vector<VisualNode> nodeVisuals;
        if (auto* manager = child(node, "fxManager");
            manager != nullptr && manager->GetText() != nullptr)
        {
            const std::string_view managerName(manager->GetText());
            fixedDirection =
                managerName.find("fxDirSpriteManager") !=
                std::string_view::npos;
            if (managerName.find("fxPSpriteManager") !=
                    std::string_view::npos ||
                managerName.find("fxPointSpritesManager") !=
                    std::string_view::npos)
                renderMode = ParticleRenderMode::PointSprite;
            else if (fixedDirection)
                renderMode = ParticleRenderMode::DirectionalSprite;
            else if (managerName.find("fxPlaneManager") !=
                     std::string_view::npos)
                renderMode = ParticleRenderMode::Plane;
            else if (managerName.find("fxTrailManager") !=
                     std::string_view::npos)
                renderMode = ParticleRenderMode::Trail;
            else if (managerName.find("fxWheelManager") !=
                     std::string_view::npos)
            {
                renderMode = ParticleRenderMode::Node;
                VisualNode visual;
                visual.meshPath = canonicalDataPath(
                    resources, "Effect\\wheel.r3d");
                visual.materials.push_back(
                    materialDefinition(resources, "Effect\\wheel"));
                nodeVisuals.push_back(std::move(visual));
            }
            else if (managerName.find("fxTrubaManager") !=
                     std::string_view::npos)
            {
                renderMode = ParticleRenderMode::Node;
                VisualNode visual;
                visual.meshPath = canonicalDataPath(
                    resources, "Effect\\truba.r3d");
                visual.materials.push_back(
                    materialDefinition(resources, "Effect\\truba"));
                nodeVisuals.push_back(std::move(visual));
            }
            else if (managerName.find("fxPiecesManager") !=
                     std::string_view::npos)
            {
                renderMode = ParticleRenderMode::Node;
                VisualNode visual;
                visual.meshPath = canonicalDataPath(
                    resources, "Effect\\pieces1.r3d");
                visual.materials.push_back(
                    materialDefinition(resources, "Effect\\pieces"));
                nodeVisuals.push_back(std::move(visual));
            }
            else if (managerName.find("fxNodeManager") !=
                     std::string_view::npos)
            {
                renderMode = ParticleRenderMode::Node;
            }
        }
        std::vector<MaterialDefinition> materials;
        if (auto* sourceMaterials = child(node, "materials"))
        {
            for (auto* material =
                     sourceMaterials->FirstChildElement();
                 material != nullptr;
                 material = material->NextSiblingElement())
            {
                const char* item = material->Attribute("item");
                if (item != nullptr && *item != '\0')
                    materials.push_back(
                        materialDefinition(resources, item));
            }
        }
        if (materials.empty() && nodeVisuals.empty())
            continue;
        auto* emitters = child(node, "emitters/items");
        if (emitters == nullptr)
            continue;
        const std::size_t firstEmitter =
            definition.particleEmitters.size();
        for (auto* sourceEmitter = emitters->FirstChildElement();
             sourceEmitter != nullptr;
             sourceEmitter = sourceEmitter->NextSiblingElement())
        {
            auto* part = child(sourceEmitter, "partDesc");
            auto* flow = child(sourceEmitter, "flowDesc");
            if (part == nullptr || flow == nullptr)
                continue;
            ParticleEmitterDefinition emitter;
            emitter.sourceRecord = record->Value() != nullptr
                                       ? record->Value()
                                       : std::string{};
            emitter.transform = nodeTransform;
            emitter.materials = materials;
            emitter.nodeVisuals = nodeVisuals;
            emitter.parentEmitter = parentEmitter;
            emitter.sourceOwnerTransform = sourceOwnerTransform;
            emitter.fixedDirection = fixedDirection;
            emitter.renderMode = renderMode;
            emitter.animationMode = animationMode;
            emitter.animationDuration = animationDuration;
            emitter.animationFrame = animationFrame;
            emitter.nodeSpeedPosition = nodeSpeedPosition;
            emitter.nodeSpeedScale = nodeSpeedScale;
            emitter.nodeSpeedRotation = nodeSpeedRotation;
            if (renderMode == ParticleRenderMode::Trail)
            {
                // DataBase::Init uses one FxTrailManager with width 0.3,
                // fixedUp=true and ZVector for both source trail records.
                emitter.trailWidth = 0.3F;
                emitter.trailFixedUp = {0.0F, 0.0F, 1.0F};
                emitter.trailFixedUpEnabled = true;
            }
            emitter.maximumParticles = static_cast<std::uint32_t>(
                std::max(optionalParticleScalar(
                             part, "maxNum", 0.0F, source),
                         0.0F));
            emitter.lifeMinimum = optionalParticleScalar(
                part, "life/min", 0.0F, source);
            emitter.lifeMaximum = optionalParticleScalar(
                part, "life/max", emitter.lifeMinimum, source);
            emitter.startTimeMinimum = optionalParticleScalar(
                part, "startTime/min", 0.0F, source);
            emitter.startTimeMaximum = optionalParticleScalar(
                part, "startTime/max",
                emitter.startTimeMinimum, source);
            emitter.startDuration = optionalParticleScalar(
                part, "startDuration", 0.0F, source);
            emitter.densityMinimum = optionalParticleScalar(
                part, "density/min", 1.0F, source);
            emitter.densityMaximum = optionalParticleScalar(
                part, "density/max",
                emitter.densityMinimum, source);
            emitter.startPositionMinimum = optionalParticleVector(
                part, "startPos/min", {}, source);
            emitter.startPositionMaximum = optionalParticleVector(
                part, "startPos/max",
                emitter.startPositionMinimum, source);
            emitter.startPositionDistribution =
                particleDistribution(part, "startPos");
            emitter.startPositionFrequency =
                particleFrequency<3U>(part, "startPos", source);
            emitter.startScaleMinimum = optionalParticleVector(
                part, "startScale/min", {1.0F, 1.0F, 1.0F},
                source);
            emitter.startScaleMaximum = optionalParticleVector(
                part, "startScale/max",
                emitter.startScaleMinimum, source);
            emitter.startScaleDistribution =
                particleDistribution(part, "startScale");
            emitter.startScaleFrequency =
                particleFrequency<3U>(part, "startScale", source);
            emitter.startRotationMinimum =
                optionalParticleQuaternion(
                    part, "startRot/min", {}, source);
            emitter.startRotationMaximum =
                optionalParticleQuaternion(
                    part, "startRot/max",
                    emitter.startRotationMinimum, source);
            emitter.startRotationDistribution =
                particleDistribution(part, "startRot");
            emitter.startRotationFrequency =
                particleFrequency<2U>(part, "startRot", source);
            emitter.rangeLifeMinimum = optionalParticleScalar(
                part, "rangeLife/min", 0.0F, source);
            emitter.rangeLifeMaximum = optionalParticleScalar(
                part, "rangeLife/max",
                emitter.rangeLifeMinimum, source);
            emitter.rangePositionMinimum = optionalParticleVector(
                part, "rangePos/min", {}, source);
            emitter.rangePositionMaximum = optionalParticleVector(
                part, "rangePos/max",
                emitter.rangePositionMinimum, source);
            emitter.rangePositionDistribution =
                particleDistribution(part, "rangePos");
            emitter.rangePositionFrequency =
                particleFrequency<3U>(part, "rangePos", source);
            emitter.rangeScaleMinimum = optionalParticleVector(
                part, "rangeScale/min", {}, source);
            emitter.rangeScaleMaximum = optionalParticleVector(
                part, "rangeScale/max",
                emitter.rangeScaleMinimum, source);
            emitter.rangeScaleDistribution =
                particleDistribution(part, "rangeScale");
            emitter.rangeScaleFrequency =
                particleFrequency<3U>(part, "rangeScale", source);
            emitter.rangeRotationMinimum =
                optionalParticleQuaternion(
                    part, "rangeRot/min", {}, source);
            emitter.rangeRotationMaximum =
                optionalParticleQuaternion(
                    part, "rangeRot/max",
                    emitter.rangeRotationMinimum, source);
            emitter.rangeRotationDistribution =
                particleDistribution(part, "rangeRot");
            emitter.rangeRotationFrequency =
                particleFrequency<2U>(part, "rangeRot", source);
            emitter.velocityMinimum = optionalParticleVector(
                flow, "speedPos/min", {}, source);
            emitter.velocityMaximum = optionalParticleVector(
                flow, "speedPos/max", emitter.velocityMinimum,
                source);
            emitter.velocityDistribution =
                particleDistribution(flow, "speedPos");
            emitter.velocityFrequency =
                particleFrequency<3U>(flow, "speedPos", source);
            emitter.rotationVelocityMinimum =
                optionalParticleQuaternion(
                    flow, "speedRot/min", {}, source);
            emitter.rotationVelocityMaximum =
                optionalParticleQuaternion(
                    flow, "speedRot/max",
                    emitter.rotationVelocityMinimum, source);
            emitter.rotationVelocityDistribution =
                particleDistribution(flow, "speedRot");
            emitter.rotationVelocityFrequency =
                particleFrequency<2U>(flow, "speedRot", source);
            emitter.scaleVelocityMinimum = optionalParticleVector(
                flow, "speedScale/min", {}, source);
            emitter.scaleVelocityMaximum = optionalParticleVector(
                flow, "speedScale/max",
                emitter.scaleVelocityMinimum, source);
            emitter.scaleVelocityDistribution =
                particleDistribution(flow, "speedScale");
            emitter.scaleVelocityFrequency =
                particleFrequency<3U>(flow, "speedScale", source);
            emitter.accelerationMinimum = optionalParticleVector(
                flow, "acceleration/min", {}, source);
            emitter.accelerationMaximum = optionalParticleVector(
                flow, "acceleration/max",
                emitter.accelerationMinimum, source);
            emitter.accelerationDistribution =
                particleDistribution(flow, "acceleration");
            emitter.accelerationFrequency =
                particleFrequency<3U>(flow, "acceleration", source);
            emitter.gravity = optionalParticleVector(
                flow, "gravitation", {}, source);
            if (auto* coordinates =
                    child(sourceEmitter, "worldCoordSys");
                coordinates != nullptr &&
                coordinates->GetText() != nullptr)
            {
                emitter.worldCoordinates =
                    std::string_view(coordinates->GetText()) ==
                    "true";
            }
            emitter.sourceSpeedBehavior = sourceSpeedBehavior;
            emitter.waitForParticleEnd = waitForParticleEnd;
            emitter.emissionDuration = ownerMaximumTimeLife;
            if (auto* rotateNode = child(sourceEmitter, "autoRot");
                rotateNode != nullptr &&
                rotateNode->GetText() != nullptr)
            {
                emitter.autoRotate =
                    std::string_view(rotateNode->GetText()) ==
                    "true";
            }
            if (auto* startType = child(part, "startType");
                startType != nullptr &&
                startType->GetText() != nullptr)
            {
                emitter.distanceTriggered =
                    std::string_view(startType->GetText()) ==
                    "sotDist";
            }
            if (auto* maximumAction =
                    child(part, "maxNumAction");
                maximumAction != nullptr &&
                maximumAction->GetText() != nullptr)
            {
                emitter.maximumAction =
                    std::string_view(maximumAction->GetText()) ==
                            "mnaReplaceLatest"
                        ? ParticleMaximumAction::ReplaceLatest
                        : ParticleMaximumAction::WaitForFree;
            }
            definition.particleEmitters.push_back(
                std::move(emitter));
        }
        // FxParticleSystem::OnCreateParticle instantiates/proxies the child
        // SceneNode once for every parent particle, then OnUpdateParticle
        // updates that child node's world position. Keep the child systems
        // linked to each owning source emitter instead of flattening them at
        // the effect origin.
        if (auto* childNode = child(node, "child");
            childNode != nullptr)
        {
            if (auto* childNodes = child(childNode, "nodes/items");
                childNodes != nullptr)
            {
                const std::size_t lastEmitter =
                    definition.particleEmitters.size();
                for (std::size_t owner = firstEmitter;
                     owner < lastEmitter; ++owner)
                {
                    appendParticleEmitters(
                        resources, record,
                        elementTransform(childNode, source),
                        sourceOwnerTransform, definition, source,
                        ownerMaximumTimeLife, childNodes,
                        static_cast<std::int32_t>(owner));
                }
            }
        }
    }
}

void appendIncludedEffects(
    const resource::ResourceFileSystem& resources,
    TiXmlElement* database, TiXmlElement* record,
    const Transform& parentTransform, ObjectDefinition& definition,
    std::string_view source, std::uint32_t depth,
    float parentMaximumTimeLife)
{
    if (depth >= 8U)
        throw resource::ResourceError(
            std::string(source) + ": effect include depth exceeded");
    auto* includes = child(record, "includeList/items");
    if (includes == nullptr)
        return;
    for (auto* include = includes->FirstChildElement();
         include != nullptr; include = include->NextSiblingElement())
    {
        auto* reference = child(include, "record");
        const Transform includeTransform =
            compose(parentTransform, elementTransform(include, source));
        // IncludeList can contain either a record instance or a complete
        // anonymous MapObj. rifleProj's source trail is the shipped example
        // of the latter; skipping it drops the complete particle system.
        auto* includedRecord =
            reference != nullptr && reference->GetText() != nullptr
                ? databaseRecord(database, reference->GetText())
                : include;
        const float includeTimeLife =
            optionalScalar(include, "maxTimeLife", -1.0F) > 0.0F
                ? optionalScalar(include, "maxTimeLife", -1.0F)
                : positiveTimeLife(includedRecord);
        const float ownerMaximumTimeLife = earlierPositiveTimeLife(
            parentMaximumTimeLife, includeTimeLife);
        LightingMode includedLighting = LightingMode::Standard;
        GraphOrder includedGraphOrder = GraphOrder::Default;
        if (auto* lighting =
                child(includedRecord, "grActor/graphLighting");
            lighting != nullptr && lighting->GetText() != nullptr)
        {
            const std::string_view value(lighting->GetText());
            if (value == "glNone")
                includedLighting = LightingMode::None;
            else if (value == "glPix")
                includedLighting = LightingMode::Pixel;
            else if (value == "glRefl")
                includedLighting = LightingMode::Reflection;
            else if (value == "glBump")
                includedLighting = LightingMode::Bump;
            else if (value == "glRefr")
                includedLighting = LightingMode::Refraction;
            else if (value == "glPlanarRefl")
                includedLighting = LightingMode::PlanarReflection;
        }
        if (auto* order = child(includedRecord, "grActor/graphOrder");
            order != nullptr && order->GetText() != nullptr)
        {
            const std::string_view value(order->GetText());
            // Preserve SReadEnum's source string-table/runtime enum mismatch.
            if (value == "goOpacity")
                includedGraphOrder = GraphOrder::Effect;
            else if (value == "goEffect")
                includedGraphOrder = GraphOrder::Opacity;
            else if (value == "goLast")
                includedGraphOrder = GraphOrder::Last;
        }
        auto nodes = visualNodes(resources, includedRecord, source);
        for (auto& node : nodes)
        {
            flattenVisualNode(node, includeTransform);
            node.lighting = includedLighting;
            node.overridesLighting = true;
            node.graphOrder = includedGraphOrder;
            node.overridesGraphOrder = true;
            node.maximumTimeLife = ownerMaximumTimeLife;
            definition.visualNodes.push_back(std::move(node));
        }
        const std::size_t firstEmitter =
            definition.particleEmitters.size();
        appendParticleEmitters(
            resources, includedRecord, includeTransform,
            includeTransform, definition, source,
            ownerMaximumTimeLife);
        // Serialized include instances carry their own Behavior list.  It
        // is authoritative for anonymous objects and can add runtime
        // behaviors to a referenced record (rocket/smoke2 is one such
        // source instance). Preserve the behavior on every flattened leaf.
        const bool inlineWaitingEnd = hasBehaviorType(include, "2");
        const bool inlineSourceSpeed = hasBehaviorType(include, "3");
        for (std::size_t emitter = firstEmitter;
             emitter < definition.particleEmitters.size(); ++emitter)
        {
            definition.particleEmitters[emitter].waitForParticleEnd =
                definition.particleEmitters[emitter].waitForParticleEnd ||
                inlineWaitingEnd;
            definition.particleEmitters[emitter].sourceSpeedBehavior =
                definition.particleEmitters[emitter]
                    .sourceSpeedBehavior || inlineSourceSpeed;
        }

        // Only LifeEffect (BehaviorType 7) starts its sound by itself when
        // the included object begins progressing. Death/Shot/Immortal
        // behaviors are dispatched by their owning callback elsewhere.
        auto* inlineBehaviors = child(include, "behaviors/items");
        const bool hasInlineBehaviors =
            inlineBehaviors != nullptr &&
            inlineBehaviors->FirstChildElement() != nullptr;
        appendBehaviorSounds(
            resources,
            hasInlineBehaviors ? include : includedRecord,
            "7", definition.soundPaths);
        appendIncludedEffects(
            resources, database, includedRecord, includeTransform,
            definition, source, depth + 1U,
            ownerMaximumTimeLife);
    }
}

void loadCollisionShapes(const resource::ResourceFileSystem& resources,
                         ObjectDefinition& definition, TiXmlElement* record,
                         std::string_view source)
{
    auto* shapes = child(record, "pxActor/shapes/items");
    if (shapes == nullptr)
        return;
    for (auto* shape = shapes->FirstChildElement(); shape != nullptr;
         shape = shape->NextSiblingElement())
    {
        auto* mesh = child(shape, "mesh");
        auto* meshId = child(shape, "meshId");
        if (mesh == nullptr || mesh->Attribute("item") == nullptr ||
            meshId == nullptr || meshId->GetText() == nullptr)
            continue;
        definition.collisionShapes.push_back(
            {canonicalDataPath(resources, mesh->Attribute("item")),
             unsignedValue(meshId->GetText(), source)});
    }
}

ObjectDefinition objectDefinition(
    const resource::ResourceFileSystem& resources, TiXmlElement* database,
    std::string_view record, std::string_view source)
{
    ObjectDefinition result;
    result.record = record;
    auto* dbRecord = databaseRecord(database, record);
    if (auto* lighting = child(dbRecord, "grActor/graphLighting");
        lighting != nullptr && lighting->GetText() != nullptr)
    {
        const std::string_view value(lighting->GetText());
        if (value == "glNone")
            result.lighting = LightingMode::None;
        else if (value == "glPix")
            result.lighting = LightingMode::Pixel;
        else if (value == "glRefl")
            result.lighting = LightingMode::Reflection;
        else if (value == "glBump")
            result.lighting = LightingMode::Bump;
        else if (value == "glRefr")
            result.lighting = LightingMode::Refraction;
        else if (value == "glPlanarRefl")
            result.lighting = LightingMode::PlanarReflection;
        result.planarReflection =
            result.lighting == LightingMode::PlanarReflection;
    }
    if (auto* properties = child(dbRecord, "grActor/graphProps");
        properties != nullptr && properties->GetText() != nullptr)
    {
        const std::string_view graphProperties(properties->GetText());
        result.castsShadow =
            graphProperties.find("gpShadowCast") != std::string_view::npos;
        result.cullOpacity =
            graphProperties.find("gpCullOpacity") != std::string_view::npos;
    }
    if (child(dbRecord, "grActor/vec1") != nullptr)
    {
        const auto value = vector4(dbRecord, "grActor/vec1", source);
        result.graphVector1 = {value[0], value[1], value[2]};
    }
    if (child(dbRecord, "grActor/vec3") != nullptr)
    {
        const auto value = vector4(dbRecord, "grActor/vec3", source);
        result.graphVector3 = {value[0], value[1], value[2]};
    }
    if (auto* order = child(dbRecord, "grActor/graphOrder");
        order != nullptr && order->GetText() != nullptr)
    {
        const std::string_view value(order->GetText());
        // IActor::Order is {goDefault, goEffect, goOpacity, goFirst,
        // goLast}, while cGraphOrderStr is
        // {"goDefault", "goOpacity", "goEffect", "goLast"}.  SReadEnum
        // converts by the string-table index, so the two middle serialized
        // names intentionally map to the opposite runtime queues.
        if (value == "goOpacity")
            result.graphOrder = GraphOrder::Effect;
        else if (value == "goEffect")
            result.graphOrder = GraphOrder::Opacity;
        else if (value == "goLast")
            result.graphOrder = GraphOrder::Last;
    }
    result.visualNodes = visualNodes(resources, dbRecord, source);
    const float rootMaximumTimeLife = positiveTimeLife(dbRecord);
    appendParticleEmitters(
        resources, dbRecord, Transform{}, Transform{}, result, source,
        rootMaximumTimeLife);
    appendIncludedEffects(
        resources, database, dbRecord, Transform{}, result, source, 0U,
        rootMaximumTimeLife);
    // LifeEffect is the only serialized object behavior which begins
    // playback merely because this object exists. Event-driven behavior
    // sounds are read by their specific callback definitions.
    appendBehaviorSounds(resources, dbRecord, "7", result.soundPaths);
    if (result.visualNodes.empty())
    {
        // gotDestrObj records keep their intact render pieces in destrList.
        // The Windows MapObj loader instantiates those children at creation;
        // preserve the same hierarchy instead of dropping the parent.
        if (auto* pieces = child(dbRecord, "destrList/items"))
        {
            for (auto* piece = pieces->FirstChildElement(); piece != nullptr;
                 piece = piece->NextSiblingElement())
            {
                const Transform pieceTransform =
                    elementTransform(piece, source);
                auto nodes = visualNodes(resources, piece, source);
                DestructionPieceDefinition destructionPiece;
                destructionPiece.transform = pieceTransform;
                for (auto& node : nodes)
                {
                    flattenVisualNode(node, pieceTransform);
                    destructionPiece.visualNodes.push_back(node);
                    result.visualNodes.push_back(std::move(node));
                }
                const auto collisionBegin = result.collisionShapes.size();
                loadCollisionShapes(resources, result, piece, source);
                destructionPiece.collisionShapes.insert(
                    destructionPiece.collisionShapes.end(),
                    result.collisionShapes.begin() +
                        static_cast<std::ptrdiff_t>(collisionBegin),
                    result.collisionShapes.end());
                auto* body = child(piece, "pxActor/body");
                auto* shapes = child(piece, "pxActor/shapes/items");
                if (body != nullptr && shapes != nullptr)
                {
                    destructionPiece.mass =
                        optionalScalar(body, "mass", 0.0F);
                    for (auto* shape = shapes->FirstChildElement();
                         shape != nullptr;
                         shape = shape->NextSiblingElement())
                    {
                        const char* type = shape->Attribute("type");
                        if (type == nullptr || std::string_view(type) != "1" ||
                            child(shape, "dimensions") == nullptr)
                            continue;
                        Transform shapeTransform;
                        shapeTransform.position =
                            child(shape, "pos") != nullptr
                                ? vector3(shape, "pos", source)
                                : Vec3{};
                        shapeTransform.rotation =
                            child(shape, "rot") != nullptr
                                ? quaternion(shape, "rot", source)
                                : Quat{};
                        shapeTransform =
                            compose(pieceTransform, shapeTransform);
                        destructionPiece.shapePosition =
                            shapeTransform.position;
                        destructionPiece.shapeRotation =
                            shapeTransform.rotation;
                        destructionPiece.halfExtents =
                            vector3(shape, "dimensions", source);
                        destructionPiece.halfExtents.x *=
                            pieceTransform.scale.x;
                        destructionPiece.halfExtents.y *=
                            pieceTransform.scale.y;
                        destructionPiece.halfExtents.z *=
                            pieceTransform.scale.z;
                        destructionPiece.dynamic =
                            destructionPiece.mass > 0.0F;
                        break;
                    }
                }
                result.destructionPieces.push_back(
                    std::move(destructionPiece));
            }
        }
    }
    if (!result.visualNodes.empty())
    {
        result.visualMeshPath = result.visualNodes.front().meshPath;
        result.visualTransform = result.visualNodes.front().transform;
        result.texturePath =
            result.visualNodes.front().materials.front().texturePath;
    }
    loadCollisionShapes(resources, result, dbRecord, source);
    if (auto* body = child(dbRecord, "pxActor/body"))
    {
        result.bodyMass = optionalScalar(body, "mass", 0.0F);
        if (auto* shapes = child(dbRecord, "pxActor/shapes/items"))
        {
            for (auto* shape = shapes->FirstChildElement(); shape != nullptr;
                 shape = shape->NextSiblingElement())
            {
                const char* type = shape->Attribute("type");
                if (type == nullptr || std::string_view(type) != "1" ||
                    child(shape, "dimensions") == nullptr)
                    continue;
                result.bodyShapePosition =
                    child(shape, "pos") != nullptr
                        ? vector3(shape, "pos", source)
                        : Vec3{};
                result.bodyShapeRotation =
                    child(shape, "rot") != nullptr
                        ? quaternion(shape, "rot", source)
                        : Quat{};
                result.bodyHalfExtents =
                    vector3(shape, "dimensions", source);
                result.dynamicBody = result.bodyMass > 0.0F;
                break;
            }
        }
    }
    if (auto* life = child(dbRecord, "maxLife");
        life != nullptr && life->GetText() != nullptr)
    {
        std::istringstream stream(life->GetText());
        stream >> result.maximumLife;
    }
    if (auto* timeLife = child(dbRecord, "maxTimeLife");
        timeLife != nullptr && timeLife->GetText() != nullptr)
    {
        std::istringstream stream(timeLife->GetText());
        stream >> result.maximumTimeLife;
    }
    const char* objectType = dbRecord->Attribute("type");
    result.destructible =
        (objectType != nullptr &&
         std::string_view(objectType) == "gotDestrObj") ||
        result.maximumLife > 0.0F;
    return result;
}

std::vector<DeathEffectDefinition> deathEffectDefinitions(
    const resource::ResourceFileSystem& resources, TiXmlElement* database,
    std::string_view modelRecord, std::string_view source)
{
    std::vector<DeathEffectDefinition> results;
    auto* model = databaseRecord(database, modelRecord);
    auto* behaviors = child(model, "behaviors/items");
    if (behaviors == nullptr)
        return results;
    for (auto* behavior = behaviors->FirstChildElement(); behavior != nullptr;
         behavior = behavior->NextSiblingElement())
    {
        const char* type = behavior->Attribute("type");
        if (type == nullptr || std::string_view(type) != "6")
            continue;
        auto* effect = child(behavior, "effect");
        if (effect == nullptr || effect->GetText() == nullptr)
            continue;
        DeathEffectDefinition result;
        result.visual = objectDefinition(
            resources, database, effect->GetText(), source);
        if (child(behavior, "pos") != nullptr)
            result.position = vector3(behavior, "pos", source);
        if (child(behavior, "impulse") != nullptr)
            result.impulse = vector3(behavior, "impulse", source);
        if (auto* ignore = child(behavior, "ignoreRot");
            ignore != nullptr && ignore->GetText() != nullptr)
        {
            result.ignoreRotation =
                std::string_view(ignore->GetText()) == "true" ||
                std::string_view(ignore->GetText()) == "1";
        }
        if (auto* targetChild = child(behavior, "targetChild");
            targetChild != nullptr && targetChild->GetText() != nullptr)
        {
            result.targetChild =
                std::string_view(targetChild->GetText()) == "true" ||
                std::string_view(targetChild->GetText()) == "1";
        }
        if (auto* ignoreSender =
                child(behavior, "effectPxIgnoreSenderCar");
            ignoreSender != nullptr && ignoreSender->GetText() != nullptr)
        {
            result.effectPhysicsIgnoreSenderCar =
                std::string_view(ignoreSender->GetText()) == "true" ||
                std::string_view(ignoreSender->GetText()) == "1";
        }
        results.push_back(std::move(result));
    }
    return results;
}

DeathEffectDefinition deathEffectDefinition(
    const resource::ResourceFileSystem& resources, TiXmlElement* database,
    std::string_view modelRecord, std::string_view source)
{
    auto definitions = deathEffectDefinitions(
        resources, database, modelRecord, source);
    return definitions.empty() ? DeathEffectDefinition{}
                               : std::move(definitions.front());
}

float effectiveEffectDuration(TiXmlElement* database,
                              TiXmlElement* record,
                              std::uint32_t depth = 0U)
{
    if (record == nullptr || depth > 16U)
        return 0.0F;
    float result = std::max(
        optionalScalar(record, "maxTimeLife", -1.0F), 0.0F);
    auto* includes = child(record, "includeList/items");
    if (includes == nullptr)
        return result;
    for (auto* include = includes->FirstChildElement();
         include != nullptr;
         include = include->NextSiblingElement())
    {
        result = std::max(
            result,
            std::max(optionalScalar(
                         include, "maxTimeLife", -1.0F),
                     0.0F));
        auto* reference = child(include, "record");
        if (reference == nullptr || reference->GetText() == nullptr)
            continue;
        result = std::max(
            result,
            effectiveEffectDuration(
                database,
                databaseRecord(database, reference->GetText()),
                depth + 1U));
    }
    return result;
}

ShotEffectDefinition shotEffectDefinition(
    const resource::ResourceFileSystem& resources,
    TiXmlElement* database, std::string_view weaponRecord,
    std::string_view source)
{
    ShotEffectDefinition result;
    auto* weapon = databaseRecord(database, weaponRecord);
    auto* behaviors = child(weapon, "behaviors/items");
    if (behaviors == nullptr)
        return result;
    for (auto* behavior = behaviors->FirstChildElement();
         behavior != nullptr;
         behavior = behavior->NextSiblingElement())
    {
        const char* type = behavior->Attribute("type");
        if (type == nullptr || std::string_view(type) != "10")
            continue;
        if (auto* effect = child(behavior, "effect");
            effect != nullptr && effect->GetText() != nullptr)
        {
            auto* effectRecord =
                databaseRecord(database, effect->GetText());
            result.visual = objectDefinition(
                resources, database, effect->GetText(), source);
            result.duration = effectiveEffectDuration(
                database, effectRecord);
        }
        if (auto* sounds = child(behavior, "sounds"))
        {
            for (auto* sound = sounds->FirstChildElement();
                 sound != nullptr;
                 sound = sound->NextSiblingElement())
            {
                const char* path = sound->Attribute("item");
                if (path != nullptr)
                {
                    result.soundPaths.push_back(
                        canonicalDataPath(resources, path));
                }
            }
        }
        if (child(behavior, "pos") != nullptr)
            result.position = vector3(behavior, "pos", source);
        if (child(behavior, "impulse") != nullptr)
            result.impulse = vector3(behavior, "impulse", source);
        result.ignoreRotation = optionalBoolean(
            behavior, "ignoreRot", false);
        break;
    }
    return result;
}

TiXmlElement* garageCar(TiXmlElement* garage, std::string_view record)
{
    auto* cars = require(garage, "cars", "garage.xml");
    for (auto* car = cars->FirstChildElement(); car != nullptr;
         car = car->NextSiblingElement())
    {
        if (text(car, "record", "garage.xml") == record)
            return car;
    }
    throw resource::ResourceError(
        "garage.xml: no car definition for " + std::string(record));
}

float baseArmor(std::string_view record)
{
    const std::string name = basename(record);
    if (name == "marauder")
        return 50.0F;
    if (name == "dirtdevil")
        return 35.0F;
    if (name == "manticora" || name == "manticoraBoss")
        return 50.0F;
    if (name == "guseniza" || name == "gusenizaBoss")
        return 75.0F;
    if (name == "podushka" || name == "podushkaBoss")
        return 65.0F;
    if (name == "monstertruck" || name == "monstertruckBoss")
        return 100.0F;
    if (name == "devildriver" || name == "devildriverBoss")
        return 110.0F;
    return 50.0F;
}

float optionalScalar(TiXmlElement* parent, std::string_view path,
                     float fallback)
{
    auto* value = child(parent, path);
    if (value == nullptr || value->GetText() == nullptr)
        return fallback;
    std::istringstream stream(value->GetText());
    float result = fallback;
    return stream >> result ? result : fallback;
}

std::uint32_t optionalUnsigned(TiXmlElement* parent,
                               std::string_view path,
                               std::uint32_t fallback)
{
    auto* value = child(parent, path);
    if (value == nullptr || value->GetText() == nullptr)
        return fallback;
    std::istringstream stream(value->GetText());
    std::uint32_t result = fallback;
    return stream >> result ? result : fallback;
}

bool optionalBoolean(TiXmlElement* parent, std::string_view path,
                     bool fallback)
{
    auto* value = child(parent, path);
    if (value == nullptr || value->GetText() == nullptr)
        return fallback;
    const std::string_view textValue(value->GetText());
    if (textValue == "true" || textValue == "1")
        return true;
    if (textValue == "false" || textValue == "0")
        return false;
    return fallback;
}

struct LocalBounds
{
    Vec3 minimum;
    Vec3 maximum;
    bool valid = false;
};

void includePoint(LocalBounds& bounds, Vec3 point)
{
    if (!bounds.valid)
    {
        bounds.minimum = point;
        bounds.maximum = point;
        bounds.valid = true;
        return;
    }
    bounds.minimum.x = std::min(bounds.minimum.x, point.x);
    bounds.minimum.y = std::min(bounds.minimum.y, point.y);
    bounds.minimum.z = std::min(bounds.minimum.z, point.z);
    bounds.maximum.x = std::max(bounds.maximum.x, point.x);
    bounds.maximum.y = std::max(bounds.maximum.y, point.y);
    bounds.maximum.z = std::max(bounds.maximum.z, point.z);
}

Vec3 transformPoint(const Transform& transform, Vec3 point)
{
    point = {point.x * transform.scale.x,
             point.y * transform.scale.y,
             point.z * transform.scale.z};
    const Vec3 rotated = rotate(transform.rotation, point);
    return {transform.position.x + rotated.x,
            transform.position.y + rotated.y,
            transform.position.z + rotated.z};
}

LocalBounds objectLocalBounds(
    const resource::ResourceFileSystem& resources,
    const ObjectDefinition& definition)
{
    LocalBounds bounds;
    for (const auto& node : definition.visualNodes)
    {
        std::array<float, 3> minimum{-0.5F, -0.5F, 0.0F};
        std::array<float, 3> maximum{0.5F, 0.5F, 0.0F};
        if (!node.meshPath.empty())
        {
            const auto mesh =
                resource::loadR3DMeshAsset(resources, node.meshPath);
            minimum = mesh.minimum;
            maximum = mesh.maximum;
        }
        else if (!node.plane)
        {
            continue;
        }

        for (std::uint32_t corner = 0; corner < 8U; ++corner)
        {
            includePoint(
                bounds,
                transformPoint(
                    node.transform,
                    {(corner & 1U) != 0U ? maximum[0] : minimum[0],
                     (corner & 2U) != 0U ? maximum[1] : minimum[1],
                     (corner & 4U) != 0U ? maximum[2] : minimum[2]}));
        }
    }
    return bounds;
}

ProjectileCollisionBox projectileCollisionBox(
    const resource::ResourceFileSystem& resources,
    const ObjectDefinition& visual, Vec3 size, Vec3 offset,
    bool modelSize, ProjectileCollisionBox* modelBounds = nullptr,
    bool* modelBoundsValid = nullptr)
{
    // Exact Proj::ComputeAABB(false) construction: serialized size is a full
    // box dimension, offset moves that box, and modelSize adds the actor's
    // transformed local visual AABB.  AABB::Add also preserves the serialized
    // size on axes where it is zero.
    ProjectileDefinition description;
    description.size = size;
    description.offset = offset;
    description.modelSize = modelSize;
    const LocalBounds model = objectLocalBounds(resources, visual);
    if (model.valid)
    {
        description.modelBoundsValid = true;
        description.modelBounds.center = {
            (model.minimum.x + model.maximum.x) * 0.5F,
            (model.minimum.y + model.maximum.y) * 0.5F,
            (model.minimum.z + model.maximum.z) * 0.5F};
        description.modelBounds.halfExtents = {
            (model.maximum.x - model.minimum.x) * 0.5F,
            (model.maximum.y - model.minimum.y) * 0.5F,
            (model.maximum.z - model.minimum.z) * 0.5F};
    }
    if (modelBounds != nullptr)
        *modelBounds = description.modelBounds;
    if (modelBoundsValid != nullptr)
        *modelBoundsValid = description.modelBoundsValid;
    return source::Proj::ComputeAABB(description, false);
}

void loadWeapons(const resource::ResourceFileSystem& resources,
                 TiXmlElement* workshopRoot, TiXmlElement* database,
                 Race& race)
{
    auto projectileVisual = [&](TiXmlElement* projectile,
                                std::string_view elementName) {
        ObjectDefinition result;
        auto* model = child(projectile, elementName);
        if (model == nullptr || model->GetText() == nullptr)
            return result;
        const std::string record = model->GetText();
        result = objectDefinition(
            resources, database, record,
            "workshop.xml/weapon/projectile visual");
        if (!result.visualNodes.empty())
            return result;
        auto* indirect = databaseRecord(database, record);
        auto* nested = child(indirect, "proj/model");
        if (nested != nullptr && nested->GetText() != nullptr)
        {
            result = objectDefinition(
                resources, database, nested->GetText(),
                "db.xml/projectile visual");
        }
        return result;
    };
    auto readProjectile = [&](TiXmlElement* projectile) {
        ProjectileDefinition definition;
        definition.type = optionalUnsigned(projectile, "type", 0U);
        definition.visual = projectileVisual(projectile, "model");
        definition.secondaryVisual =
            projectileVisual(projectile, "model2");
        definition.tertiaryVisual =
            projectileVisual(projectile, "model3");
        if (child(projectile, "pos") != nullptr)
        {
            definition.position = vector3(
                projectile, "pos", "workshop.xml/weapon/projectile");
        }
        if (child(projectile, "size") != nullptr)
        {
            definition.size = vector3(
                projectile, "size", "workshop.xml/weapon/projectile");
        }
        if (child(projectile, "sizeAddPx") != nullptr)
        {
            definition.sizeAddPx = vector3(
                projectile, "sizeAddPx",
                "workshop.xml/weapon/projectile");
        }
        if (child(projectile, "offset") != nullptr)
        {
            definition.offset = vector3(
                projectile, "offset", "workshop.xml/weapon/projectile");
        }
        definition.modelSize = optionalBoolean(
            projectile, "modelSize", true);
        if (child(projectile, "rot") != nullptr)
        {
            definition.rotation = quaternion(
                projectile, "rot", "workshop.xml/weapon/projectile");
        }
        definition.speed = optionalScalar(projectile, "speed", 0.0F);
        definition.relativeSpeedMinimum = optionalScalar(
            projectile, "speedRelativeMin", 13.0F);
        if (auto* relative = child(projectile, "speedRelative");
            relative != nullptr && relative->GetText() != nullptr)
        {
            definition.relativeSpeed =
                std::string_view(relative->GetText()) == "true" ||
                std::string_view(relative->GetText()) == "1";
        }
        definition.angularSpeed =
            optionalScalar(projectile, "angleSpeed", 0.0F);
        definition.maximumDistance =
            optionalScalar(projectile, "maxDist", 0.0F);
        definition.minimumLife =
            optionalScalar(projectile, "minTimeLife/min", 0.0F);
        definition.maximumLife = optionalScalar(
            projectile, "minTimeLife/max", definition.minimumLife);
        definition.mass = optionalScalar(projectile, "mass", 100.0F);
        definition.damage = optionalScalar(projectile, "damage", 0.0F);
        definition.collision = projectileCollisionBox(
            resources, definition.visual, definition.size,
            definition.offset, definition.modelSize,
            &definition.modelBounds, &definition.modelBoundsValid);
        const auto placementBounds =
            source::Proj::ComputeAABB(definition, true);
        definition.surfacePlacementOffset = std::max(
            -(placementBounds.center.z -
              placementBounds.halfExtents.z),
            0.01F);
        auto nestedProjectile =
            [&](std::string_view elementName,
                const ObjectDefinition& visual) {
                NestedProjectileDefinition result;
                Vec3 size = definition.size;
                Vec3 offset = definition.offset;
                bool modelSize = definition.modelSize;
                auto* model = child(projectile, elementName);
                if (model != nullptr && model->GetText() != nullptr)
                {
                    auto* record =
                        databaseRecord(database, model->GetText());
                    if (auto* nested = child(record, "proj"))
                    {
                        result.valid = true;
                        result.type = optionalUnsigned(
                            nested, "type", definition.type);
                        result.speed = optionalScalar(
                            nested, "speed", definition.speed);
                        result.minimumLife = optionalScalar(
                            nested, "minTimeLife/min", 0.0F);
                        result.maximumLife = optionalScalar(
                            nested, "minTimeLife/max",
                            result.minimumLife);
                        result.damage = optionalScalar(
                            nested, "damage", definition.damage);
                        if (child(nested, "size") != nullptr)
                            size = vector3(
                                nested, "size",
                                "db.xml/nested projectile");
                        if (child(nested, "offset") != nullptr)
                            offset = vector3(
                                nested, "offset",
                                "db.xml/nested projectile");
                        modelSize = optionalBoolean(
                            nested, "modelSize", true);
                        if (auto* nestedModel = child(nested, "model");
                            nestedModel != nullptr &&
                            nestedModel->GetText() != nullptr)
                        {
                            result.deathEffect = deathEffectDefinition(
                                resources, database,
                                nestedModel->GetText(),
                                "db.xml/nested projectile death effect");
                        }
                    }
                }
                result.collision = projectileCollisionBox(
                    resources, visual, size, offset, modelSize);
                return result;
            };
        definition.secondaryProjectile =
            nestedProjectile("model2", definition.secondaryVisual);
        definition.tertiaryProjectile =
            nestedProjectile("model3", definition.tertiaryVisual);
        definition.secondaryCollision =
            definition.secondaryProjectile.collision;
        definition.tertiaryCollision =
            definition.tertiaryProjectile.collision;
        return definition;
    };
    race.weapons.clear();
    auto* workshop = require(workshopRoot, "workshop", "workshop.xml");
    for (auto* entry = workshop->FirstChildElement(); entry != nullptr;
         entry = entry->NextSiblingElement())
    {
        const std::uint32_t type =
            optionalUnsigned(entry, "type", 0U);
        if (type < 5U || type > 9U)
            continue;
        auto* item = child(entry, "item");
        auto* mesh = child(item, "mesh");
        auto* texture = child(item, "texture");
        if (item == nullptr || mesh == nullptr || texture == nullptr ||
            mesh->Attribute("item") == nullptr ||
            texture->Attribute("item") == nullptr)
            continue;

        WeaponDefinition weapon;
        weapon.record = entry->Value();
        weapon.name = text(item, "name", "workshop.xml/weapon");
        weapon.itemType = static_cast<WeaponItemType>(type);
        weapon.slot = type == 5U
                          ? WeaponSlot::Hyper
                          : (type == 6U ? WeaponSlot::Mine
                                        : (type == 7U
                                               ? WeaponSlot::Primary
                                               : WeaponSlot::Support));
        weapon.visual.meshPath =
            canonicalDataPath(resources, mesh->Attribute("item"));
        weapon.visual.transform.position =
            child(item, "pos") != nullptr
                ? vector3(item, "pos", "workshop.xml/weapon")
                : Vec3{};
        weapon.visual.transform.rotation =
            child(item, "rot") != nullptr
                ? quaternion(item, "rot", "workshop.xml/weapon")
                : Quat{};
        weapon.visual.materials.push_back(
            {"Weapon\\" + weapon.record,
             canonicalDataPath(resources, texture->Attribute("item")),
             MaterialBlend::Opaque, 0.0F});
        weapon.damage = optionalScalar(item, "damage", 0.0F);
        weapon.maximumCharge =
            optionalUnsigned(item, "maxCharge", 0U);
        weapon.reloadCharge =
            optionalUnsigned(item, "cntCharge", 1U);
        weapon.chargeStep =
            optionalUnsigned(item, "chargeStep", 1U);
        weapon.chargeCost = static_cast<int>(
            optionalUnsigned(item, "chargeCost", 0U));
        weapon.shotDelay =
            optionalScalar(item, "shotDelay", 0.1F);
        weapon.repairPeriod =
            optionalScalar(item, "repairPeriod", 0.0F);
        weapon.repairValue =
            optionalScalar(item, "repairValue", 0.0F);
        weapon.reflectValue =
            optionalScalar(item, "reflectValue", 0.0F);
        if (auto* projectiles = child(item, "projList"))
        {
            for (auto* projectile = projectiles->FirstChildElement();
                 projectile != nullptr;
                 projectile = projectile->NextSiblingElement())
            {
                auto definition = readProjectile(projectile);
                if (child(projectile, "damage") == nullptr)
                    definition.damage = weapon.damage;
                const std::size_t projectileIndex =
                    weapon.projectiles.size();
                definition.weaponListIndex = projectileIndex;
                weapon.projectiles.push_back(std::move(definition));

                // DeathEffect::OnDeath instantiates the effect record attached
                // to the projectile's model.  Most records are explosions;
                // mortiraBallDeath is a gotProj and creates a live ptCrater.
                const auto& visualRecord =
                    weapon.projectiles[projectileIndex].visual.record;
                if (visualRecord.empty())
                    continue;
                auto* modelRecord = databaseRecord(database, visualRecord);
                auto* behaviors = child(modelRecord, "behaviors/items");
                if (behaviors == nullptr)
                    continue;
                for (auto* behavior = behaviors->FirstChildElement();
                     behavior != nullptr;
                     behavior = behavior->NextSiblingElement())
                {
                    const char* behaviorType = behavior->Attribute("type");
                    if (behaviorType == nullptr ||
                        std::string_view(behaviorType) != "6")
                        continue;
                    auto* effect = child(behavior, "effect");
                    if (effect == nullptr || effect->GetText() == nullptr)
                        continue;
                    const std::string effectRecord = effect->GetText();
                    weapon.projectiles[projectileIndex].deathEffect =
                        deathEffectDefinition(
                            resources, database, visualRecord,
                            "db.xml/projectile death effect");
                    auto* effectSource =
                        databaseRecord(database, effectRecord);
                    auto* spawnedProjectile = child(effectSource, "proj");
                    if (spawnedProjectile != nullptr)
                    {
                        auto spawned = readProjectile(spawnedProjectile);
                        spawned.spawnOnParentDeath = true;
                        const std::size_t spawnedIndex =
                            weapon.projectiles.size();
                        spawned.weaponListIndex = spawnedIndex;
                        weapon.projectiles[projectileIndex]
                            .deathProjectile = spawnedIndex;
                        weapon.projectiles.push_back(std::move(spawned));
                    }
                    break;
                }
            }
        }
        if (weapon.projectiles.empty())
        {
            ProjectileDefinition projectile;
            projectile.weaponListIndex = 0U;
            projectile.damage = weapon.damage;
            weapon.projectiles.push_back(projectile);
        }
        const auto& firstProjectile = weapon.projectiles.front();
        weapon.projectileType = firstProjectile.type;
        weapon.projectileSpeed = firstProjectile.speed;
        weapon.maximumDistance = firstProjectile.maximumDistance;
        auto* mapObject = child(item, "mapObj");
        if (mapObject != nullptr && mapObject->GetText() != nullptr)
        {
            weapon.shotEffect = shotEffectDefinition(
                resources, database, mapObject->GetText(),
                "db.xml/ctWeapon ShotEffect");
        }
        else if (weapon.slot != WeaponSlot::Support)
        {
            throw resource::ResourceError(
                "workshop.xml/weapon: missing source mapObj for firing "
                "weapon " + weapon.record);
        }
        race.weapons.push_back(std::move(weapon));
    }
    if (race.weapons.empty() ||
        std::none_of(race.weapons.begin(), race.weapons.end(),
                     [](const WeaponDefinition& weapon) {
                         return weapon.slot == WeaponSlot::Primary;
                     }))
    {
        throw resource::ResourceError(
            "workshop.xml: no original weapon catalog");
    }
}

void loadAchievements(
    const resource::ResourceFileSystem& resources, Race& race)
{
    auto document = parseXml(resources, "achievment.xml");
    auto* conditions = require(
        document.RootElement(), "conditions", "achievment.xml");
    race.achievements.clear();
    for (auto* item = conditions->FirstChildElement(); item != nullptr;
         item = item->NextSiblingElement())
    {
        AchievementDefinition definition;
        definition.name = item->Value();
        if (const char* classId = item->Attribute("classId"))
        {
            definition.classId =
                unsignedValue(classId, "achievment.xml/classId");
        }
        definition.reward = optionalUnsigned(item, "reward", 0U);
        definition.iterationCount =
            std::max(optionalUnsigned(item, "iterCount", 1U), 1U);
        definition.killsNumber =
            optionalUnsigned(item, "killsNum", 0U);
        definition.killsTime =
            optionalScalar(item, "killsTime", 0.0F);
        definition.place = optionalUnsigned(item, "place", 1U);
        if (auto* bonus = child(item, "bonusType");
            bonus != nullptr && bonus->GetText() != nullptr)
        {
            const std::string_view type = bonus->GetText();
            if (type == "btMoney")
                definition.bonusKind = BonusKind::Money;
            else if (type == "btMedpack")
                definition.bonusKind = BonusKind::Medpack;
            else if (type == "btCharge")
                definition.bonusKind = BonusKind::Ammunition;
            else if (type == "btImmortal")
                definition.bonusKind = BonusKind::Shield;
            else if (type == "btSpeedArrow")
                definition.bonusKind = BonusKind::Speed;
        }
        race.achievements.push_back(std::move(definition));
    }
}

void loadRewards(TiXmlElement* planet, std::uint32_t planetIndex,
                 Race& race)
{
    race.rewardMoney.fill(0U);
    race.rewardPoints.fill(0U);
    race.requiredPoints.clear();
    race.tournamentPlanetIndex = planetIndex;
    race.tournamentCarRewards.clear();
    race.tournamentSlotRewards.clear();
    if (auto* points = child(planet, "points"))
    {
        for (auto* point = points->FirstChildElement();
             point != nullptr;
             point = point->NextSiblingElement())
        {
            const auto pass = optionalUnsigned(point, "place", 0U);
            if (pass == 0U)
                continue;
            if (race.requiredPoints.size() < pass)
                race.requiredPoints.resize(pass, 0U);
            race.requiredPoints[pass - 1U] =
                optionalUnsigned(point, "value", 0U);
        }
    }
    const auto loadPassRewards = [&](const char* group,
                                     std::vector<TournamentReward>& output) {
        auto* rewards = child(planet, group);
        if (rewards == nullptr)
            return;
        for (auto* reward = rewards->FirstChildElement(); reward != nullptr;
             reward = reward->NextSiblingElement())
        {
            TournamentReward value;
            value.record = text(
                reward, "record", "tournamet.xml/pass reward");
            value.pass = optionalUnsigned(reward, "pass", 0U);
            value.charge = optionalUnsigned(reward, "charge", 0U);
            if (auto* type = child(reward, "type");
                type != nullptr && type->GetText() != nullptr)
            {
                value.slotType = type->GetText();
            }
            output.push_back(std::move(value));
        }
    };
    loadPassRewards("cars", race.tournamentCarRewards);
    loadPassRewards("slots", race.tournamentSlotRewards);
    auto* prices = child(planet, "prices");
    if (prices == nullptr)
        return;
    std::size_t place = 0;
    for (auto* price = prices->FirstChildElement();
         price != nullptr && place < race.rewardMoney.size();
         price = price->NextSiblingElement(), ++place)
    {
        race.rewardMoney[place] =
            optionalUnsigned(price, "money", 0U);
        race.rewardPoints[place] =
            optionalUnsigned(price, "points", 0U);
    }
}

Vehicle loadVehicle(const resource::ResourceFileSystem& resources,
                    TiXmlElement* database, TiXmlElement* garage,
                    std::string_view record)
{
    const std::string source = "db.xml/" + basename(record);
    auto* car = databaseRecord(database, record);
    auto* garageDefinition = garageCar(garage, record);

    Vehicle result;
    result.record = std::string(record);
    result.maximumLife = baseArmor(record);
    result.disableColor = optionalBoolean(
        car, "motor/disableColor", false);
    if (auto* lighting = child(car, "grActor/graphLighting");
        lighting != nullptr && lighting->GetText() != nullptr)
    {
        const std::string_view value(lighting->GetText());
        if (value == "glNone")
            result.lighting = LightingMode::None;
        else if (value == "glPix")
            result.lighting = LightingMode::Pixel;
        else if (value == "glRefl")
            result.lighting = LightingMode::Reflection;
        else if (value == "glBump")
            result.lighting = LightingMode::Bump;
        else if (value == "glRefr")
            result.lighting = LightingMode::Refraction;
        else if (value == "glPlanarRefl")
            result.lighting = LightingMode::PlanarReflection;
    }
    if (auto* garageWheel = child(garageDefinition, "wheel");
        garageWheel != nullptr &&
        garageWheel->Attribute("item") != nullptr)
    {
        result.wheelMeshPath =
            canonicalDataPath(resources, garageWheel->Attribute("item"));
    }
    auto* garageBodies =
        require(garageDefinition, "bodyMeshes", "garage.xml/car");
    auto* firstGarageBody = garageBodies->FirstChildElement();
    if (firstGarageBody == nullptr)
        throw resource::ResourceError(
            "garage.xml/car: missing body mesh");
    result.texturePath = canonicalDataPath(
        resources,
        itemAttribute(firstGarageBody, "texture", "garage.xml/car"));
    result.bodyVisuals =
        visualNodes(resources, car, source + "/body", result.texturePath);
    if (result.bodyVisuals.empty())
        throw resource::ResourceError(source + ": no body visual");
    result.bodyMeshPath = result.bodyVisuals.front().meshPath;
    result.bodyVisualTransform = result.bodyVisuals.front().transform;

    if (auto* lights = child(garageDefinition, "nightLights"))
    {
        for (auto* item = lights->FirstChildElement(); item != nullptr;
             item = item->NextSiblingElement())
        {
            VehicleNightLight light;
            light.head = boolean(item, "head", "garage.xml/nightLight");
            light.position =
                vector3(item, "pos", "garage.xml/nightLight");
            std::istringstream size(
                text(item, "size", "garage.xml/nightLight"));
            if (!(size >> light.size[0] >> light.size[1]))
                throw resource::ResourceError(
                    "garage.xml/nightLight: invalid size");
            result.nightLights.push_back(light);
        }
    }
    static constexpr std::array<std::string_view,
                                static_cast<std::size_t>(
                                    GarageSlotType::Count)>
        slotNames{
            "stWheel", "stTruba", "stArmor", "stMotor", "stHyper",
            "stMine", "stWeapon1", "stWeapon2", "stWeapon3",
            "stWeapon4"};
    for (std::size_t mountIndex = 0;
         mountIndex < result.slotMounts.size(); ++mountIndex)
    {
        const std::string mountName(slotNames[mountIndex]);
        auto* mount = child(garageDefinition, mountName);
        if (mount == nullptr)
            continue;
        auto& output = result.slotMounts[mountIndex];
        output.active =
            boolean(mount, "active", "garage.xml/" + mountName);
        output.show =
            boolean(mount, "show", "garage.xml/" + mountName);
        output.position =
            vector3(mount, "pos", "garage.xml/" + mountName);
        auto* items = child(mount, "items");
        if (items == nullptr)
            continue;
        for (auto* item = items->FirstChildElement(); item != nullptr;
             item = item->NextSiblingElement())
        {
            VehicleSlotPlacement placement;
            placement.record =
                text(item, "record", "garage.xml/" + mountName);
            placement.rotation =
                quaternion(item, "rot", "garage.xml/" + mountName);
            placement.offset =
                vector3(item, "offset", "garage.xml/" + mountName);
            output.placements.push_back(std::move(placement));
        }
    }

    auto* engineSound = static_cast<TiXmlElement*>(nullptr);
    if (auto* behaviors = child(car, "behaviors/items"))
    {
        for (auto* behavior = behaviors->FirstChildElement();
             behavior != nullptr;
             behavior = behavior->NextSiblingElement())
        {
            const char* type = behavior->Attribute("type");
            if (type != nullptr && std::string_view(type) == "12")
            {
                engineSound = behavior;
                break;
            }
        }
    }
    if (engineSound == nullptr)
        throw resource::ResourceError(source +
                                      ": missing SoundMotor behavior");
    result.idleSoundPath = canonicalDataPath(
        resources,
        itemAttribute(engineSound, "sndIdle", source + "/engine sound"));
    result.rpmSoundPath = canonicalDataPath(
        resources,
        itemAttribute(engineSound, "sndRPM", source + "/engine sound"));
    const auto readSoundRange = [&](std::string_view name) {
        std::array<float, 2> range{};
        std::istringstream stream(text(engineSound, name, source));
        if (!(stream >> range[0] >> range[1]))
            throw resource::ResourceError(
                source + ": invalid SoundMotor " + std::string(name));
        return range;
    };
    result.rpmVolumeRange = readSoundRange("rpmVolumeRange");
    result.rpmFrequencyRange = readSoundRange("rpmFreqRange");

    // These actors are constructed by DataBase::LoadCar and serialized in
    // each affected ctCar record. Preserve their child transform, submesh,
    // material and behavior tag rather than redrawing the entire body as a
    // guessed overlay.
    if (auto* includes = child(car, "includeList/items"))
    {
        for (auto* include = includes->FirstChildElement();
             include != nullptr; include = include->NextSiblingElement())
        {
            bool trackAnimation = false;
            std::vector<int> cushionTags;
            if (auto* behaviors = child(include, "behaviors/items"))
            {
                for (auto* behavior = behaviors->FirstChildElement();
                     behavior != nullptr;
                     behavior = behavior->NextSiblingElement())
                {
                    const char* type = behavior->Attribute("type");
                    if (type == nullptr)
                        continue;
                    if (std::string_view(type) == "13")
                    {
                        trackAnimation = true;
                    }
                    else if (std::string_view(type) == "14")
                    {
                        auto* target = child(behavior, "targetTag");
                        if (target == nullptr || target->GetText() == nullptr)
                            throw resource::ResourceError(
                                source +
                                ": PodushkaAnim has no targetTag");
                        cushionTags.push_back(static_cast<int>(
                            unsignedValue(target->GetText(), source)));
                    }
                }
            }
            if (!trackAnimation && cushionTags.empty())
                continue;
            const Transform includeTransform =
                elementTransform(include, source + "/animated child");
            auto nodes = visualNodes(
                resources, include, source + "/animated child");
            for (auto& node : nodes)
            {
                flattenVisualNode(node, includeTransform);
                if (trackAnimation)
                    result.trackVisuals.push_back(node);
                if (std::find(cushionTags.begin(), cushionTags.end(),
                              node.tag) != cushionTags.end())
                    result.cushionVisuals.push_back(std::move(node));
            }
        }
    }
    // DataBase::LoadCar attaches the same source LowLifePoints behavior to
    // every car; it is constructed at runtime rather than serialized in the
    // individual ctCar record.
    result.lowLifeEffect = objectDefinition(
        resources, database, "world\\db\\root\\ctEffects\\smoke6",
        source + "/LowLifePoints/smoke6");
    // DataBase::LoadCar constructs this record from the actual car mesh,
    // body visual transform and Effect\\shield1, then attaches a
    // DamageEffect whose filter is dtEnergy.
    result.energyDamageEffect = objectDefinition(
        resources, database,
        "world\\db\\root\\ctEffects\\damageEnergy" + basename(record),
        source + "/DamageEffect/damageEnergy");
    // DataBase::LoadCar attaches ImmortalEffect to every car with this
    // source record and the fixed non-uniform scale coefficient.
    result.shieldEffect = objectDefinition(
        resources, database, "world\\db\\root\\ctEffects\\shield1",
        source + "/ImmortalEffect/shield1");
    result.shieldEffectScale = {1.3F, 1.7F, 1.7F};
    result.deathEffects = deathEffectDefinitions(
        resources, database, record, source + "/death effects");

    auto& vehicle = result.physics;
    vehicle.mass = scalar(car, "pxActor/body/mass", source);
    vehicle.halfExtents =
        vector3(car, "pxActor/shapes/items/item0/dimensions", source);
    vehicle.shapePosition =
        vector3(car, "pxActor/shapes/items/item0/pos", source);
    vehicle.angularDamping = vector3(car, "angDamping", source);
    const auto bodyMaterial = static_cast<unsigned>(
        optionalScalar(
            car, "pxActor/shapes/items/item0/materialIndex", 1.0F));
    vehicle.bodyFriction = bodyMaterial == 2U ? 0.02F : 0.08F;
    {
        const std::string pose =
            text(car, "pxActor/body/massLocalPose", source);
        std::istringstream stream(pose);
        float ignored = 0.0F;
        for (int index = 0; index < 9; ++index)
            if (!(stream >> ignored))
                throw resource::ResourceError(
                    source + ": invalid massLocalPose");
        if (!(stream >> vehicle.centerOfMass.x >>
              vehicle.centerOfMass.y >> vehicle.centerOfMass.z))
            throw resource::ResourceError(
                source + ": invalid massLocalPose translation");
    }
    vehicle.brakeTorque = scalar(car, "motor/brakeTorque", source);
    vehicle.differentialRatio = scalar(car, "motor/gearDiff", source);
    vehicle.maximumRpm = scalar(car, "motor/maxRPM", source);
    vehicle.maximumTorque = scalar(car, "motor/maxTorque", source);
    vehicle.torqueEfficiency =
        scalar(car, "motor/SEM", source) * 1.15F;
    vehicle.steeringControl =
        scalar(car, "motor/kSteerControl", source);
    vehicle.steerSpeed = scalar(car, "motor/steerSpeed", source);
    vehicle.steerRotation = scalar(car, "motor/steerRot", source);
    vehicle.airbornePitchAcceleration =
        scalar(car, "motor/flyYTorque", source);
    vehicle.clampRollAngle =
        scalar(car, "motor/clampXTorque", source);
    vehicle.clampPitchAngle =
        scalar(car, "motor/clampYTorque", source);
    vehicle.maximumSpeed = scalar(car, "motor/maxSpeed", source);
    vehicle.tireSpring = scalar(car, "motor/tireSpring", source);
    vehicle.automaticGears = boolean(car, "motor/autoGear", source);
    vehicle.gravitySteering =
        boolean(car, "motor/gravEngine", source);
    vehicle.clutchImmunity =
        boolean(car, "motor/clutchImmunity", source);
    // These are CarMotorDesc constructor values and are not serialized per
    // ctCar record by the original game.
    vehicle.idlingRpm = 1000.0F;
    vehicle.restBrakeTorque = 400.0F;
    // GameCar::cMaxSteerAngle from the Windows implementation.
    vehicle.steerAngle = 3.14159265358979323846F / 6.0F;

    auto* wheels = require(car, "wheels/items", source);
    for (auto* item = wheels->FirstChildElement(); item != nullptr;
         item = item->NextSiblingElement())
    {
        r3d::physics::WheelDescription wheel;
        wheel.position = vector3(item, "pos", source + "/wheel");
        auto* shape = require(item, "pxActor/shapes/items/item0",
                              source + "/wheel");
        wheel.radius = scalar(shape, "radius", source + "/wheel");
        wheel.suspensionTravel =
            scalar(shape, "suspensionTravel", source + "/wheel");
        wheel.spring =
            scalar(shape, "suspension/spring", source + "/wheel");
        wheel.damper =
            scalar(shape, "suspension/damper", source + "/wheel");
        wheel.inverseMass =
            scalar(shape, "inverseWheelMass", source + "/wheel");
        wheel.driven = boolean(item, "lead", source + "/wheel");
        wheel.steering = boolean(item, "steer", source + "/wheel");
        std::vector<WheelSlipEffectDefinition> slipBehaviors;
        if (auto* behaviors = child(item, "behaviors/items"))
        {
            for (auto* behavior = behaviors->FirstChildElement();
                 behavior != nullptr;
                 behavior = behavior->NextSiblingElement())
            {
                const char* type = behavior->Attribute("type");
                if (type == nullptr || std::string_view(type) != "9")
                    continue;
                auto* effect = child(behavior, "effect");
                if (effect == nullptr || effect->GetText() == nullptr)
                    continue;
                WheelSlipEffectDefinition definition;
                definition.visual = objectDefinition(
                    resources, database, effect->GetText(),
                    source + "/wheel/PxWheelSlipEffect");
                if (auto* sounds = child(behavior, "sounds"))
                {
                    for (auto* sound = sounds->FirstChildElement();
                         sound != nullptr;
                         sound = sound->NextSiblingElement())
                    {
                        const char* path = sound->Attribute("item");
                        if (path != nullptr)
                        {
                            definition.soundPaths.push_back(
                                canonicalDataPath(resources, path));
                        }
                    }
                }
                if (child(behavior, "pos") != nullptr)
                    definition.position = vector3(
                        behavior, "pos", source + "/wheel");
                if (child(behavior, "impulse") != nullptr)
                    definition.impulse = vector3(
                        behavior, "impulse", source + "/wheel");
                definition.ignoreRotation = optionalBoolean(
                    behavior, "ignoreRot", false);
                slipBehaviors.push_back(std::move(definition));
            }
        }
        const bool hasSlipEffect = !slipBehaviors.empty();
        vehicle.wheels.push_back(wheel);
        result.wheelInverted.push_back(
            boolean(item, "invertWheel", source + "/wheel"));
        result.wheelSlipEffects.push_back(hasSlipEffect);
        // DataBase::LoadCar creates the sound only for source wheel zero;
        // the generated db.xml preserves that resulting Source3d reference.
        // Read the artifact exactly instead of inferring sound ownership from
        // the presence of a visual effect.
        result.wheelSlipSounds.push_back(std::any_of(
            slipBehaviors.begin(), slipBehaviors.end(),
            [](const WheelSlipEffectDefinition& definition) {
                return !definition.soundPaths.empty();
            }));
        result.wheelSlipBehaviors.push_back(
            std::move(slipBehaviors));
        result.wheelVisualOffsets.push_back(
            vector3(item, "offset", source + "/wheel"));
        auto wheelNodes = visualNodes(
            resources, item, source + "/wheel", result.texturePath);
        if (wheelNodes.size() > 1)
            throw resource::ResourceError(
                source + ": wheel visual count mismatch");
        if (!wheelNodes.empty())
        {
            result.wheelVisualTransforms.push_back(
                wheelNodes.front().transform);
            if (result.wheelMeshPath.empty())
                result.wheelMeshPath = wheelNodes.front().meshPath;
            result.wheelVisuals.push_back(std::move(wheelNodes.front()));
        }
    }
    if (vehicle.wheels.size() != 4)
        throw resource::ResourceError(source + ": expected four wheels");

    // Player::ComputeCarBBSize asks the car graph actor for its transformed
    // local AABB and uses the diagonal length. The body VisualNodes are the
    // backend-neutral form of that actor; collision dimensions are not an
    // equivalent source for this value.
    ObjectDefinition carActor;
    carActor.visualNodes = result.bodyVisuals;
    const LocalBounds bounds = objectLocalBounds(resources, carActor);
    if (bounds.valid)
    {
        const Vec3 dimensions{
            bounds.maximum.x - bounds.minimum.x,
            bounds.maximum.y - bounds.minimum.y,
            bounds.maximum.z - bounds.minimum.z};
        result.boundingSize = std::max(
            std::sqrt(dimensions.x * dimensions.x +
                      dimensions.y * dimensions.y +
                      dimensions.z * dimensions.z),
            0.001F);
    }
    else
    {
        const Vec3 half = vehicle.halfExtents;
        result.boundingSize = std::max(
            2.0F * std::sqrt(half.x * half.x + half.y * half.y +
                             half.z * half.z),
            0.001F);
    }
    result.boundingRadius = result.boundingSize * 0.5F;
    return result;
}

void applyWeatherDescription(
    const resource::ResourceFileSystem& resources, Race& race,
    Weather weather)
{
    source::Environment::ApplyWeather(
        race.environment, weather,
        source::Environment::WorldTypeFromLevelPath(race.levelPath));
    race.environment.skyTexturePath = canonicalDataPath(
        resources, race.environment.skyTexturePath);
}

Weather weatherFromToken(std::string_view token)
{
    return source::Environment::WeatherFromToken(token);
}

void applyOriginalEnvironment(
    const resource::ResourceFileSystem& resources, Race& race)
{
    const auto worldType =
        source::Environment::WorldTypeFromLevelPath(race.levelPath);
    source::Environment::ApplyWorldType(race.environment, worldType);
    Weather weather = Weather::Fair;
    if (worldType == source::EnvironmentWorldType::World2 ||
        worldType == source::EnvironmentWorldType::World6)
        weather = Weather::Cloudy;
    else if (worldType == source::EnvironmentWorldType::World3)
        weather = Weather::Sahara;
    else if (worldType == source::EnvironmentWorldType::World4)
        weather = Weather::Hell;
    else if (worldType == source::EnvironmentWorldType::World5)
        weather = Weather::Snow;
    applyWeatherDescription(resources, race, weather);
}

void applyPlanetEnvironment(
    const resource::ResourceFileSystem& resources,
    TiXmlElement* planet, Race& race)
{
    auto* weatherItems = child(planet, "wheaters");
    race.environment.weatherChances.clear();
    TiXmlElement* selected = nullptr;
    float maximumChance = -1.0F;
    if (weatherItems != nullptr)
    {
        for (auto* item = weatherItems->FirstChildElement();
             item != nullptr; item = item->NextSiblingElement())
        {
            const float chance = optionalScalar(item, "chance", 0.0F);
            race.environment.weatherChances.push_back(
                {weatherFromToken(text(item, "type",
                                      "tournamet.xml/wheaters")),
                 chance});
            if (chance > maximumChance)
            {
                maximumChance = chance;
                selected = item;
            }
        }
    }
    if (selected != nullptr)
    {
        applyWeatherDescription(
            resources, race,
            weatherFromToken(text(selected, "type",
                                  "tournamet.xml/wheaters")));
    }
}

std::vector<r3d::physics::TriangleMesh> loadCollisionMeshes(
    const Race& race,
    const resource::ResourceFileSystem& resources,
    std::vector<std::size_t>& decorationInstances)
{
    std::vector<r3d::physics::TriangleMesh> result;
    decorationInstances.clear();
    auto appendCollision = [&](const ObjectDefinition& definition,
                               const ObjectInstance& instance,
                               bool track,
                               std::size_t decorationInstance) {
        for (const auto& shape : definition.collisionShapes)
        {
            const auto mesh = resource::loadR3DMeshAsset(
                resources, shape.meshPath);
            r3d::physics::TriangleMesh collision;
            collision.transform = instance.transform;
            collision.surface =
                track && shape.materialGroup == 1U
                    ? r3d::physics::CollisionSurface::TrackBorder
                    : (track
                           ? r3d::physics::CollisionSurface::TrackPlane
                           : r3d::physics::CollisionSurface::Decoration);
            collision.decorationInstance = decorationInstance;
            collision.vertices.reserve(mesh.vertices.size());
            for (const auto& vertex : mesh.vertices)
            {
                collision.vertices.push_back(
                    {vertex.position[0], vertex.position[1],
                     vertex.position[2]});
            }
            if (shape.materialGroup < mesh.materialGroups.size())
            {
                const auto& group =
                    mesh.materialGroups[shape.materialGroup];
                collision.indices.insert(
                    collision.indices.end(),
                    mesh.indices.begin() + group.firstIndex,
                    mesh.indices.begin() + group.firstIndex +
                        group.indexCount);
            }
            else
            {
                collision.indices = mesh.indices;
            }
            if (!collision.indices.empty())
            {
                result.push_back(std::move(collision));
                decorationInstances.push_back(decorationInstance);
            }
        }
    };
    for (const auto& instance : race.trackInstances)
    {
        appendCollision(
            race.trackDefinitions.at(instance.definition), instance,
            true, std::numeric_limits<std::size_t>::max());
    }
    for (std::size_t index = 0;
         index < race.decorationInstances.size(); ++index)
    {
        const auto& instance = race.decorationInstances[index];
        appendCollision(
            race.decorationDefinitions.at(instance.definition), instance,
            false, index);
    }
    return result;
}

std::uint32_t mapItemCount(TiXmlElement* map, std::string_view category,
                           std::string_view source)
{
    const std::string path = std::string(category) + "/items";
    auto* items = require(map, path, source);
    std::uint32_t count = 0U;
    for (auto* item = items->FirstChildElement(); item != nullptr;
         item = item->NextSiblingElement())
        ++count;
    return count;
}

void assignStaticMapObjectIds(TiXmlElement* map, Race& race)
{
    // Map::Load visits MapObjLib::Category in this exact enum order and
    // MapObjList::InsertItem increments _lastId for every object.  Rebuild
    // the same global identity even for categories the portable renderer
    // does not otherwise need to materialize.
    std::uint32_t next = 1U;
    next += mapItemCount(map, "ctEffects", race.levelPath);
    const auto decorationCount =
        mapItemCount(map, "ctDecoration", race.levelPath);
    if (decorationCount != race.decorationInstances.size())
        throw resource::ResourceError(
            race.levelPath + ": ctDecoration MapObj count mismatch");
    for (auto& instance : race.decorationInstances)
        instance.mapObjectId = next++;

    const auto trackCount = mapItemCount(map, "ctTrack", race.levelPath);
    if (trackCount != race.trackInstances.size())
        throw resource::ResourceError(
            race.levelPath + ": ctTrack MapObj count mismatch");
    for (auto& instance : race.trackInstances)
        instance.mapObjectId = next++;

    next += mapItemCount(map, "ctWeapon", race.levelPath);
    next += mapItemCount(map, "ctCar", race.levelPath);
    next += mapItemCount(map, "ctWaypoint", race.levelPath);
    const auto bonusCount = mapItemCount(map, "ctBonus", race.levelPath);
    if (bonusCount != race.bonuses.size())
        throw resource::ResourceError(
            race.levelPath + ": ctBonus MapObj count mismatch");
    for (auto& bonus : race.bonuses)
        bonus.mapObjectId = next++;
    race.firstDynamicMapObjectId = next;
}

void assignRacerMapObjectIds(Race& race)
{
    std::uint32_t next = race.firstDynamicMapObjectId;
    for (auto& racer : race.racers)
        racer.mapObjectId = next++;
}

void loadMapProxyState(TiXmlElement* item, ObjectInstance& instance)
{
    instance.name = item->Value() != nullptr ? item->Value() : "";
    instance.life = optionalScalar(item, "life", -1.0F);
    instance.maximumTimeLife =
        optionalScalar(item, "maxTimeLife", -1.0F);
    instance.timeLife = optionalScalar(item, "timeLife", 0.0F);
    instance.hasProxyState = true;
}

void loadMapProxyState(TiXmlElement* item, BonusInstance& instance)
{
    instance.name = item->Value() != nullptr ? item->Value() : "";
    instance.life = optionalScalar(item, "life", -1.0F);
    instance.maximumTimeLife =
        optionalScalar(item, "maxTimeLife", -1.0F);
    instance.timeLife = optionalScalar(item, "timeLife", 0.0F);
    instance.hasProxyState = true;
}

void loadMap(const resource::ResourceFileSystem& resources,
             TiXmlElement* database, Race& race)
{
    race.trackDefinitions.clear();
    race.trackInstances.clear();
    race.decorationDefinitions.clear();
    race.decorationInstances.clear();
    race.bonuses.clear();
    race.tracePoints.clear();
    race.tracePath.clear();
    race.tracePaths.clear();

    auto mapDocument = parseXml(resources, race.levelPath);
    auto* map = require(mapDocument.RootElement(), "map", race.levelPath);
    std::unordered_map<std::string, std::uint32_t> definitions;
    auto* trackItems = require(map, "ctTrack/items", race.levelPath);
    for (auto* item = trackItems->FirstChildElement(); item != nullptr;
         item = item->NextSiblingElement())
    {
        const std::string record = text(item, "record", race.levelPath);
        auto [entry, inserted] = definitions.emplace(
            record, static_cast<std::uint32_t>(
                        race.trackDefinitions.size()));
        if (inserted)
        {
            auto definition = objectDefinition(
                resources, database, record, "db.xml/track");
            if (definition.collisionShapes.empty())
                throw resource::ResourceError(record +
                                              ": no collision mesh");
            race.trackDefinitions.push_back(std::move(definition));
        }
        ObjectInstance instance;
        instance.definition = entry->second;
        instance.transform = elementTransform(item, race.levelPath);
        loadMapProxyState(item, instance);
        race.trackInstances.push_back(std::move(instance));
    }

    definitions.clear();
    auto* decorationItems =
        require(map, "ctDecoration/items", race.levelPath);
    for (auto* item = decorationItems->FirstChildElement(); item != nullptr;
         item = item->NextSiblingElement())
    {
        const std::string record = text(item, "record", race.levelPath);
        auto [entry, inserted] = definitions.emplace(
            record, static_cast<std::uint32_t>(
                        race.decorationDefinitions.size()));
        if (inserted)
        {
            auto definition = objectDefinition(
                resources, database, record, "db.xml/decoration");
            if (definition.visualNodes.empty())
                throw resource::ResourceError(
                    record + ": decoration has no original visual");
            race.decorationDefinitions.push_back(std::move(definition));
        }
        ObjectInstance instance;
        instance.definition = entry->second;
        instance.transform = elementTransform(item, race.levelPath);
        loadMapProxyState(item, instance);
        race.decorationInstances.push_back(std::move(instance));
    }

    auto* bonusItems = require(map, "ctBonus/items", race.levelPath);
    for (auto* item = bonusItems->FirstChildElement(); item != nullptr;
         item = item->NextSiblingElement())
    {
        BonusInstance bonus;
        bonus.record = text(item, "record", race.levelPath);
        auto* bonusRecord = databaseRecord(database, bonus.record);
        const std::string modelRecord =
            text(bonusRecord, "proj/model", "db.xml/bonus");
        bonus.visual = objectDefinition(
            resources, database, modelRecord, "db.xml/bonus model");
        bonus.deathEffect = deathEffectDefinition(
            resources, database, modelRecord,
            "db.xml/map projectile death effect");
        bonus.transform = elementTransform(item, race.levelPath);
        loadMapProxyState(item, bonus);
        bonus.projectileType = optionalUnsigned(
            bonusRecord, "proj/type", 0U);
        if (child(bonusRecord, "proj/size") != nullptr)
            bonus.size = vector3(bonusRecord, "proj/size", "db.xml/bonus");
        if (child(bonusRecord, "proj/offset") != nullptr)
            bonus.offset =
                vector3(bonusRecord, "proj/offset", "db.xml/bonus");
        bonus.modelSize = optionalBoolean(
            bonusRecord, "proj/modelSize", true);
        bonus.collision = projectileCollisionBox(
            resources, bonus.visual, bonus.size, bonus.offset,
            bonus.modelSize, &bonus.modelBounds,
            &bonus.modelBoundsValid);
        bonus.speed = optionalScalar(bonusRecord, "proj/speed", 0.0F);
        if (bonus.projectileType == 6U)
            bonus.kind = BonusKind::Money;
        else if (bonus.projectileType == 4U)
            bonus.kind = BonusKind::Medpack;
        else if (bonus.projectileType == 5U)
            bonus.kind = BonusKind::Ammunition;
        else if (bonus.projectileType == 7U)
            bonus.kind = BonusKind::Shield;
        else if (bonus.projectileType == 8U)
            bonus.kind = BonusKind::Speed;
        else if (bonus.projectileType == 9U)
            bonus.kind = BonusKind::SlowHazard;
        else if (bonus.projectileType == 10U)
            bonus.kind = BonusKind::OilHazard;
        else if (bonus.projectileType == 11U ||
                 bonus.projectileType == 12U ||
                 bonus.projectileType == 13U ||
                 bonus.projectileType == 24U)
            bonus.kind = BonusKind::MineHazard;
        bonus.value = scalar(bonusRecord, "proj/damage", "db.xml/bonus");
        race.bonuses.push_back(std::move(bonus));
    }
    assignStaticMapObjectIds(map, race);

    auto* points = require(map, "trace/points", race.levelPath);
    for (auto* point = points->FirstChildElement(); point != nullptr;
         point = point->NextSiblingElement())
    {
        const char* id = point->Attribute("id");
        if (id == nullptr)
            throw resource::ResourceError(
                race.levelPath + ": trace point has no id");
        race.tracePoints.push_back(
            {unsignedValue(id, race.levelPath),
             vector3(point, "pos", race.levelPath),
             scalar(point, "size", race.levelPath)});
    }
    auto* paths = require(map, "trace/pathes", race.levelPath);
    for (auto* path = paths->FirstChildElement(); path != nullptr;
         path = path->NextSiblingElement())
    {
        std::vector<std::uint32_t> result;
        for (auto* node = path->FirstChildElement(); node != nullptr;
             node = node->NextSiblingElement())
        {
            if (node->GetText() != nullptr)
                result.push_back(
                    unsignedValue(node->GetText(), race.levelPath));
        }
        if (result.size() > 1U)
            race.tracePaths.push_back(std::move(result));
    }
    if (race.tracePaths.empty())
        throw resource::ResourceError(
            race.levelPath + ": trace has no paths");
    race.tracePath = race.tracePaths.front();
    race.environment.sunPosition = vector3(map, "sunPos", race.levelPath);
    race.environment.sunRotation = quaternion(map, "sunRot", race.levelPath);
    applyOriginalEnvironment(resources, race);
    race.collisionMeshes = loadCollisionMeshes(
        race, resources, race.collisionMeshDecorationInstances);
}

std::uint32_t unsignedValue(std::string_view value,
                            std::string_view source)
{
    std::uint32_t result = 0;
    std::istringstream stream{std::string(value)};
    if (!(stream >> result) || (stream >> std::ws && !stream.eof()))
        throw resource::ResourceError(std::string(source) +
                                      ": invalid unsigned value");
    return result;
}

} // namespace

std::vector<MaterialDefinition> loadOriginalMaterialCatalog(
    const resource::ResourceFileSystem& resources)
{
    std::vector<MaterialDefinition> result;
    materialDefinition(resources, {}, &result);
    return result;
}

const PlayerIdentity* findOriginalPlayerIdentity(
    const Race& race, int gamerId) noexcept
{
    const auto global = std::find_if(
        race.playerIdentities.begin(), race.playerIdentities.end(),
        [gamerId](const PlayerIdentity& identity) {
            return identity.planetIndex < 0 && identity.id == gamerId;
        });
    if (global != race.playerIdentities.end())
        return &*global;
    const int currentPlanet =
        static_cast<int>(race.tournamentPlanetIndex);
    const auto local = std::find_if(
        race.playerIdentities.begin(), race.playerIdentities.end(),
        [gamerId, currentPlanet](const PlayerIdentity& identity) {
            return identity.planetIndex == currentPlanet &&
                   identity.id == gamerId;
        });
    return local != race.playerIdentities.end() ? &*local : nullptr;
}

Race loadFirstOriginalRace(const resource::ResourceFileSystem& resources,
                           bool legacyWindowsDebug)
{
    auto tournamentDocument = parseXml(resources, "tournamet.xml");
    auto databaseDocument = parseXml(resources, "db.xml");
    auto garageDocument = parseXml(resources, "garage.xml");
    auto workshopDocument = parseXml(resources, "workshop.xml");
    auto* tournament = tournamentDocument.RootElement();
    auto* database = databaseDocument.RootElement();

    Race race;
    race.workshop = loadOriginalWorkshop(resources);
    loadPlayerIdentities(race, resources, tournament);
    race.touchBorderDamage = vector2(
        garageDocument.RootElement(), "touchBorderDamage", "garage.xml");
    race.touchBorderDamageForce = vector2(
        garageDocument.RootElement(), "touchBorderDamageForce", "garage.xml");
    race.touchCarDamage = vector2(
        garageDocument.RootElement(), "touchCarDamage", "garage.xml");
    race.touchCarDamageForce = vector2(
        garageDocument.RootElement(), "touchCarDamageForce", "garage.xml");
    loadWeapons(resources, workshopDocument.RootElement(), database, race);
    loadAchievements(resources, race);
    auto* firstPlanet = require(tournament, "planets/planet0", "tournamet.xml");
    loadRewards(firstPlanet, 0U, race);
    auto* firstTrack = require(firstPlanet,
                               "trackMap/tracks0/track0", "tournamet.xml");
    race.levelPath = canonicalDataPath(
        resources, text(firstTrack, "level", "tournamet.xml"));
    race.lapCount = unsignedValue(text(firstTrack, "numLaps", "tournamet.xml"),
                                  "tournamet.xml/numLaps");

    const std::string carRecord =
        text(firstPlanet, "cars/car0/record", "tournamet.xml");
    const std::string carName = basename(carRecord);
    race.vehicle = loadVehicle(
        resources, database, garageDocument.RootElement(), carRecord);

    auto mapDocument = parseXml(resources, race.levelPath);
    auto* map = require(mapDocument.RootElement(), "map", race.levelPath);
    auto* trackItems = require(map, "ctTrack/items", race.levelPath);
    std::unordered_map<std::string, std::uint32_t> definitions;
    for (auto* item = trackItems->FirstChildElement(); item != nullptr;
         item = item->NextSiblingElement())
    {
        const std::string record = text(item, "record", race.levelPath);
        auto [entry, inserted] = definitions.emplace(
            record, static_cast<std::uint32_t>(race.trackDefinitions.size()));
        if (inserted)
        {
            ObjectDefinition definition = objectDefinition(
                resources, database, record, "db.xml/track");
            if (definition.collisionShapes.empty())
                throw resource::ResourceError(record +
                                              ": no collision mesh");
            race.trackDefinitions.push_back(std::move(definition));
        }
        ObjectInstance instance;
        instance.definition = entry->second;
        instance.transform.position = vector3(item, "pos", race.levelPath);
        instance.transform.scale = vector3(item, "scale", race.levelPath);
        instance.transform.rotation = quaternion(item, "rot", race.levelPath);
        loadMapProxyState(item, instance);
        race.trackInstances.push_back(instance);
    }

    auto* decorationItems =
        require(map, "ctDecoration/items", race.levelPath);
    definitions.clear();
    for (auto* item = decorationItems->FirstChildElement(); item != nullptr;
         item = item->NextSiblingElement())
    {
        const std::string record = text(item, "record", race.levelPath);
        auto [entry, inserted] = definitions.emplace(
            record,
            static_cast<std::uint32_t>(race.decorationDefinitions.size()));
        if (inserted)
        {
            auto definition = objectDefinition(
                resources, database, record, "db.xml/decoration");
            if (definition.visualNodes.empty())
                throw resource::ResourceError(
                    record + ": decoration has no original visual");
            race.decorationDefinitions.push_back(std::move(definition));
        }
        ObjectInstance instance;
        instance.definition = entry->second;
        instance.transform = elementTransform(item, race.levelPath);
        loadMapProxyState(item, instance);
        race.decorationInstances.push_back(std::move(instance));
    }

    auto* bonusItems = require(map, "ctBonus/items", race.levelPath);
    for (auto* item = bonusItems->FirstChildElement(); item != nullptr;
         item = item->NextSiblingElement())
    {
        BonusInstance bonus;
        bonus.record = text(item, "record", race.levelPath);
        auto* bonusRecord = databaseRecord(database, bonus.record);
        const std::string modelRecord =
            text(bonusRecord, "proj/model", "db.xml/bonus");
        bonus.visual = objectDefinition(
            resources, database, modelRecord, "db.xml/bonus model");
        bonus.deathEffect = deathEffectDefinition(
            resources, database, modelRecord,
            "db.xml/map projectile death effect");
        bonus.transform = elementTransform(item, race.levelPath);
        loadMapProxyState(item, bonus);
        bonus.projectileType = optionalUnsigned(
            bonusRecord, "proj/type", 0U);
        if (child(bonusRecord, "proj/size") != nullptr)
            bonus.size = vector3(bonusRecord, "proj/size", "db.xml/bonus");
        if (child(bonusRecord, "proj/offset") != nullptr)
            bonus.offset =
                vector3(bonusRecord, "proj/offset", "db.xml/bonus");
        bonus.modelSize = optionalBoolean(
            bonusRecord, "proj/modelSize", true);
        bonus.collision = projectileCollisionBox(
            resources, bonus.visual, bonus.size, bonus.offset,
            bonus.modelSize, &bonus.modelBounds,
            &bonus.modelBoundsValid);
        bonus.speed = optionalScalar(bonusRecord, "proj/speed", 0.0F);
        if (bonus.projectileType == 6U)
            bonus.kind = BonusKind::Money;
        else if (bonus.projectileType == 4U)
            bonus.kind = BonusKind::Medpack;
        else if (bonus.projectileType == 5U)
            bonus.kind = BonusKind::Ammunition;
        else if (bonus.projectileType == 7U)
            bonus.kind = BonusKind::Shield;
        else if (bonus.projectileType == 8U)
            bonus.kind = BonusKind::Speed;
        else if (bonus.projectileType == 9U)
            bonus.kind = BonusKind::SlowHazard;
        else if (bonus.projectileType == 10U)
            bonus.kind = BonusKind::OilHazard;
        else if (bonus.projectileType == 11U ||
                 bonus.projectileType == 12U ||
                 bonus.projectileType == 13U ||
                 bonus.projectileType == 24U)
            bonus.kind = BonusKind::MineHazard;
        bonus.value = scalar(bonusRecord, "proj/damage", "db.xml/bonus");
        race.bonuses.push_back(std::move(bonus));
    }
    assignStaticMapObjectIds(map, race);

    auto* points = require(map, "trace/points", race.levelPath);
    for (auto* point = points->FirstChildElement(); point != nullptr;
         point = point->NextSiblingElement())
    {
        const char* id = point->Attribute("id");
        if (id == nullptr)
            throw resource::ResourceError(race.levelPath +
                                          ": trace point has no id");
        race.tracePoints.push_back(
            {unsignedValue(id, race.levelPath),
             vector3(point, "pos", race.levelPath),
             scalar(point, "size", race.levelPath)});
    }
    auto* paths = require(map, "trace/pathes", race.levelPath);
    for (auto* path = paths->FirstChildElement(); path != nullptr;
         path = path->NextSiblingElement())
    {
        std::vector<std::uint32_t> tracePath;
        for (auto* node = path->FirstChildElement(); node != nullptr;
             node = node->NextSiblingElement())
        {
            if (node->GetText() != nullptr)
                tracePath.push_back(unsignedValue(
                    node->GetText(), race.levelPath));
        }
        if (tracePath.size() > 1U)
            race.tracePaths.push_back(std::move(tracePath));
    }
    if (race.tracePaths.empty())
        throw resource::ResourceError(
            race.levelPath + ": trace has no paths");
    race.tracePath = race.tracePaths.front();
    race.environment.sunPosition =
        vector3(map, "sunPos", race.levelPath);
    race.environment.sunRotation =
        quaternion(map, "sunRot", race.levelPath);
    applyOriginalEnvironment(resources, race);
    race.rainEffect = objectDefinition(
        resources, database, "world\\db\\root\\ctEffects\\rain",
        "db.xml/original rain");
    race.wheelTrailEffect = objectDefinition(
        resources, database, "world\\db\\root\\ctEffects\\trail",
        "db.xml/original wheel trail");
    race.wheelSmokeEffect = objectDefinition(
        resources, database, "world\\db\\root\\ctEffects\\smoke7",
        "db.xml/original wheel smoke");
    race.wheelSlipSoundPath = canonicalDataPath(
        resources, "Data/Sounds/SkidAsphalt.ogg");
    race.contactEffect = objectDefinition(
        resources, database, "world\\db\\root\\ctEffects\\spark2",
        "db.xml/PairPxContactEffect");
    race.contactSoundPaths.clear();
    for (std::uint32_t index = 1U; index <= 5U; ++index)
    {
        race.contactSoundPaths.push_back(canonicalDataPath(
            resources,
            "Data/Sounds/light_impact0" + std::to_string(index) +
                ".ogg"));
    }

    auto* planets = require(tournament, "planets", "tournamet.xml");
    std::uint32_t planetIndex = 0;
    for (auto* planet = planets->FirstChildElement(); planet != nullptr;
         planet = planet->NextSiblingElement(), ++planetIndex)
    {
        const std::string worldType =
            text(planet, "worldType", "tournamet.xml");
        // Race.cpp adds these two tracks before Planet::LoadFrom when either
        // _DEBUG or DEBUG_PX is defined. Keep them behind an explicit runtime
        // mode so a Debug macOS build still has the shipped 88-track catalog.
        if (legacyWindowsDebug && planetIndex == 1U)
        {
            race.trackCatalog.push_back(
                {canonicalDataPath(
                     resources, "Data/Map/debugTrack.r3dMap"),
                 99U, worldType, planetIndex, 1U});
        }
        else if (legacyWindowsDebug && planetIndex == 3U)
        {
            race.trackCatalog.push_back(
                {canonicalDataPath(
                     resources, "Data/Map/World5/map0.r3dMap"),
                 99U, worldType, planetIndex, 1U});
        }
        auto* trackMap = require(planet, "trackMap", "tournamet.xml");
        for (auto* group = trackMap->FirstChildElement(); group != nullptr;
             group = group->NextSiblingElement())
        {
            const char* passAttribute = group->Attribute("pass");
            const std::uint32_t racePass =
                passAttribute == nullptr
                    ? 1U
                    : unsignedValue(passAttribute,
                                    "tournamet.xml/trackMap/pass");
            for (auto* track = group->FirstChildElement(); track != nullptr;
                 track = track->NextSiblingElement())
            {
                race.trackCatalog.push_back(
                    {canonicalDataPath(
                         resources,
                         text(track, "level", "tournamet.xml")),
                     unsignedValue(
                         text(track, "numLaps", "tournamet.xml"),
                         "tournamet.xml/numLaps"),
                     worldType, planetIndex, racePass});
            }
        }
    }
    std::unordered_map<std::string, std::size_t> vehicleIndices;
    auto* garageCars =
        require(garageDocument.RootElement(), "cars", "garage.xml");
    for (auto* garageEntry = garageCars->FirstChildElement();
         garageEntry != nullptr;
         garageEntry = garageEntry->NextSiblingElement())
    {
        const std::string record =
            text(garageEntry, "record", "garage.xml/car");
        vehicleIndices.emplace(record, race.vehicles.size());
        race.vehicles.push_back(loadVehicle(
            resources, database, garageDocument.RootElement(), record));
    }
    if (legacyWindowsDebug)
    {
        // PxWheelSlipEffect::OnProgress and PairPxContactEffect::OnContact
        // are both compiled out by Windows _DEBUG. Keep the serialized
        // objects loaded, but prevent portable wheel effects/audio from
        // running in the compatibility mode.
        for (auto& vehicle : race.vehicles)
        {
            std::fill(vehicle.wheelSlipEffects.begin(),
                      vehicle.wheelSlipEffects.end(), false);
            std::fill(vehicle.wheelSlipSounds.begin(),
                      vehicle.wheelSlipSounds.end(), false);
        }
    }
    const auto humanVehicle = vehicleIndices.find(carRecord);
    if (humanVehicle == vehicleIndices.end())
        throw resource::ResourceError(
            "garage.xml: tournament player car is missing");
    race.vehicle = race.vehicles[humanVehicle->second];
    selectRacers(race, resources, firstPlanet, 1U, carRecord);
    assignRacerMapObjectIds(race);
    race.collisionMeshes = loadCollisionMeshes(
        race, resources, race.collisionMeshDecorationInstances);
    if (race.trackInstances.empty() || race.tracePath.size() < 2 ||
        race.vehicle.record.find(carName) == std::string::npos ||
        race.vehicles.size() != 17 || race.racers.size() < 2)
        throw resource::ResourceError(race.levelPath +
                                      ": incomplete original race data");
    return race;
}

Race loadOriginalRace(const resource::ResourceFileSystem& resources,
                      std::size_t trackIndex,
                      std::string_view playerCar,
                      bool legacyWindowsDebug)
{
    Race result = loadFirstOriginalRace(resources, legacyWindowsDebug);
    if (trackIndex >= result.trackCatalog.size())
        throw resource::ResourceError(
            "Requested tournament track index is out of range");
    if (trackIndex != 0)
    {
        const auto& selected = result.trackCatalog[trackIndex];
        result.levelPath = selected.levelPath;
        result.lapCount = selected.lapCount;
        auto databaseDocument = parseXml(resources, "db.xml");
        loadMap(resources, databaseDocument.RootElement(), result);
    }
    if (!playerCar.empty())
    {
        const auto found = std::find_if(
            result.vehicles.begin(), result.vehicles.end(),
            [&](const Vehicle& vehicle) {
                return vehicle.record == playerCar ||
                       basename(vehicle.record) == playerCar;
            });
        if (found == result.vehicles.end())
            throw resource::ResourceError(
                "Requested garage car is unavailable: " +
                std::string(playerCar));
        const std::size_t index =
            static_cast<std::size_t>(found - result.vehicles.begin());
        result.vehicle = *found;
        result.racers.front().vehicle = index;
    }

    auto tournamentDocument = parseXml(resources, "tournamet.xml");
    auto* planets =
        require(tournamentDocument.RootElement(), "planets",
                "tournamet.xml");
    auto* selectedPlanet = planets->FirstChildElement();
    for (std::uint32_t index = 0;
         selectedPlanet != nullptr &&
         index < result.trackCatalog[trackIndex].planetIndex;
         ++index)
    {
        selectedPlanet = selectedPlanet->NextSiblingElement();
    }
    if (selectedPlanet == nullptr)
        throw resource::ResourceError(
            "tournamet.xml: selected planet is unavailable");
    loadRewards(selectedPlanet,
                result.trackCatalog[trackIndex].planetIndex, result);
    applyPlanetEnvironment(resources, selectedPlanet, result);
    selectRacers(result, resources, selectedPlanet,
                 result.trackCatalog[trackIndex].racePass,
                 result.vehicle.record);
    assignRacerMapObjectIds(result);
    return result;
}

void selectOriginalWeather(
    const resource::ResourceFileSystem& resources, Race& race,
    bool allowNight, bool mostProbable, float randomUnit)
{
    const auto& chances = race.environment.weatherChances;
    if (chances.empty())
        return;
    source::Planet planet;
    source::Planet::Wheaters wheaters;
    wheaters.reserve(chances.size());
    for (const auto& item : chances)
        wheaters.push_back(
            {static_cast<int>(item.weather), item.chance});
    planet.SetWheaters(std::move(wheaters));
    const auto selected = planet.GenerateWheater(
        allowNight, mostProbable, randomUnit);
    applyWeatherDescription(
        resources, race, static_cast<Weather>(selected.type));
}

Race loadOriginalGarageScene(
    const resource::ResourceFileSystem& resources,
    const Race& sourceRace)
{
    auto databaseDocument = parseXml(resources, "db.xml");
    auto* database = databaseDocument.RootElement();

    Race result;
    result.levelPath = "RaceMenu2::CarFrame";
    result.vehicles = sourceRace.vehicles;
    for (auto& vehicle : result.vehicles)
    {
        // CarFrame only creates the body, wheels and default mounted weapon
        // actors.  Runtime damage/shield/death actors do not belong to this
        // scene and would merely duplicate unused GPU resources.
        vehicle.lowLifeEffect = {};
        vehicle.shieldEffect = {};
        vehicle.deathEffects.clear();
        vehicle.nightLights.clear();
    }
    result.weapons = sourceRace.weapons;
    for (auto& weapon : result.weapons)
    {
        weapon.shotEffect = {};
        weapon.projectiles.clear();
    }

    constexpr std::array<std::string_view, 2> decorationRecords{
        "world\\db\\root\\ctDecoration\\Misc\\garage",
        "world\\db\\root\\ctDecoration\\Misc\\question"};
    for (const auto record : decorationRecords)
    {
        auto definition = objectDefinition(
            resources, database, record, "db.xml/RaceMenu2::CarFrame");
        if (definition.visualNodes.empty())
            throw resource::ResourceError(
                std::string(record) +
                ": CarFrame decoration has no original visual");
        result.decorationDefinitions.push_back(std::move(definition));
    }
    result.decorationInstances.push_back(
        {0U, {}, 0U, {}, -1.0F, -1.0F, 0.0F, false});
    Transform question;
    question.position = {0.0F, 0.0F, 0.39F};
    result.decorationInstances.push_back(
        {1U, question, 0U, {}, -1.0F, -1.0F, 0.0F, false});

    result.racers.reserve(result.vehicles.size());
    for (std::size_t index = 0; index < result.vehicles.size(); ++index)
    {
        Racer racer;
        racer.name = result.vehicles[index].record;
        racer.playerId = static_cast<int>(index);
        racer.vehicle = index;
        result.racers.push_back(std::move(racer));
    }
    if (!result.vehicles.empty())
        result.vehicle = result.vehicles.front();

    // Environment::ewGarage + Environment::wtGarage.
    source::Environment::ApplyPresentation(
        result.environment, source::EnvironmentWorldType::Garage);
    result.environment.skyTexturePath = canonicalDataPath(
        resources, result.environment.skyTexturePath);

    // RaceMenu2.cpp CarFrame::OnShow.  glm::quat takes (w, x, y, z);
    // portable Quat stores (x, y, z, w).
    result.environment.lamps[0] = {
        {-0.94673467F, 3.0021181F, 2.9447727F},
        {0.19748747F, 0.34095559F, -0.46066701F, 0.79532242F},
        {1.0F, 1.0F, 1.0F, 1.0F}, 20.0F, true};
    result.environment.lamps[1] = {
        {6.0344887F, -5.2521329F, 1.6322796F},
        {-0.17059785F, 0.045529708F, 0.95100683F, 0.25379914F},
        {1.0F, 1.0F, 1.0F, 1.0F}, 20.0F, true};

    result.presentationCamera.position = {
        2.9176455F, 3.8489482F, 1.2934232F};
    result.presentationCamera.rotation = {
        0.11683256F, 0.058045074F, -0.88791621F, 0.44111854F};
    result.presentationCamera.verticalFovDegrees = 90.0F;
    result.presentationCamera.nearDistance = 1.0F;
    result.presentationCamera.farDistance = 20.0F;
    result.presentationCamera.valid = true;

    if (result.vehicles.size() != 17U ||
        result.racers.size() != result.vehicles.size() ||
        result.decorationDefinitions.size() != 2U ||
        !result.presentationCamera.valid)
    {
        throw resource::ResourceError(
            "RaceMenu2::CarFrame source scene is incomplete");
    }
    return result;
}

Race loadOriginalAngarScene(
    const resource::ResourceFileSystem& resources)
{
    auto databaseDocument = parseXml(resources, "db.xml");
    auto* database = databaseDocument.RootElement();

    Race result;
    result.levelPath = "RaceMenu2::SpaceshipFrame";
    constexpr std::array<std::string_view, 2> decorationRecords{
        "world\\db\\root\\ctDecoration\\Misc\\space2",
        "world\\db\\root\\ctDecoration\\Misc\\angar"};
    for (const auto record : decorationRecords)
    {
        auto definition = objectDefinition(
            resources, database, record,
            "db.xml/RaceMenu2::SpaceshipFrame");
        if (definition.visualNodes.empty())
        {
            throw resource::ResourceError(
                std::string(record) +
                ": SpaceshipFrame decoration has no original visual");
        }
        result.decorationDefinitions.push_back(std::move(definition));
    }

    // SpaceshipFrame mutates the source plane node after AddMapObj.  The
    // bundled GUI\space2 texture is 1920x1200, hence the exact 1.6 aspect.
    auto& space = result.decorationDefinitions.front().visualNodes.front();
    space.transform.position = {63.0F, 0.0F, 23.0F};
    space.transform.scale = {112.0F, 70.0F, 1.0F};
    constexpr float sine45 = 0.70710678118654752440F;
    const Quat roll{sine45, 0.0F, 0.0F, sine45};
    const Quat pitch{0.0F, sine45, 0.0F, sine45};
    // BaseSceneNode builds Eul_(+pi/2,+pi/2,0,EulOrdXYZs) for the two
    // source setters SetPitchAngle(-pi/2), SetRollAngle(-pi/2).
    space.transform.rotation = multiply(roll, pitch);

    result.decorationInstances.push_back(
        {0U, {}, 0U, {}, -1.0F, -1.0F, 0.0F, false});
    result.decorationInstances.push_back(
        {1U, {}, 0U, {}, -1.0F, -1.0F, 0.0F, false});

    // Environment::ewAngar + Environment::wtAngar.
    source::Environment::ApplyPresentation(
        result.environment, source::EnvironmentWorldType::Angar);
    result.environment.skyTexturePath = canonicalDataPath(
        resources, result.environment.skyTexturePath);

    result.environment.lamps[0] = {
        {22.169474F, -5.9075522F, 35.802311F},
        {-0.47304764F, 0.077942163F, 0.86590993F, 0.14267041F},
        {1.0F, 1.0F, 1.0F, 1.0F}, 80.0F, true};
    result.environment.lamps[1] = {
        {-20.881384F, -21.184746F, 26.121809F},
        {-0.16464995F, 0.34524971F, 0.39772648F, 0.83397770F},
        {0.0F, 0.0F, 0.0F, 0.0F}, 80.0F, false};
    result.environment.lamps[2] = {
        {52.307316F, 24.327570F, 32.772705F},
        {0.21948183F, 0.18329506F, -0.73549527F, 0.61423278F},
        {136.0F / 255.0F, 254.0F / 255.0F,
         254.0F / 255.0F, 1.0F},
        100.0F, true};

    result.presentationCamera.position = {
        -43.756214F, -11.786510F, 21.129881F};
    result.presentationCamera.rotation = {
        -0.028391786F, 0.21455817F, 0.12807286F, 0.96786171F};
    result.presentationCamera.verticalFovDegrees = 90.0F;
    result.presentationCamera.nearDistance = 1.0F;
    result.presentationCamera.farDistance = 130.0F;
    result.presentationCamera.valid = true;

    if (result.decorationDefinitions.size() != 2U ||
        result.decorationInstances.size() != 2U ||
        result.decorationDefinitions.front().visualNodes.front()
            .materials.empty() ||
        !result.presentationCamera.valid)
    {
        throw resource::ResourceError(
            "RaceMenu2::SpaceshipFrame source scene is incomplete");
    }
    return result;
}

namespace
{

source::Tournament makeSourceTournament(
    const Race& race, const PlayerProfile& profile)
{
    std::size_t planetCount = profile.planets.size();
    for (const auto& track : race.trackCatalog)
        planetCount = std::max(
            planetCount,
            static_cast<std::size_t>(track.planetIndex) + 1U);

    source::Tournament tournament(true);
    source::Planet::RequestPoints requestPoints;
    for (std::size_t index = 0U; index < race.requiredPoints.size(); ++index)
    {
        requestPoints.emplace(
            static_cast<int>(index + 1U),
            static_cast<int>(std::min<std::uint32_t>(
                race.requiredPoints[index],
                static_cast<std::uint32_t>(
                    std::numeric_limits<int>::max()))));
    }
    for (std::size_t index = 0U; index < planetCount; ++index)
    {
        auto& planet = tournament.AddPlanet();
        planet.SetRequestPoints(requestPoints);
        if (index < profile.planets.size())
        {
            const auto& saved = profile.planets[index];
            planet.Restore(
                static_cast<source::Planet::State>(
                    std::min<std::uint32_t>(
                        saved.state,
                        static_cast<std::uint32_t>(
                            source::Planet::psCompleted))),
                static_cast<int>(std::min<std::uint32_t>(
                    saved.pass,
                    static_cast<std::uint32_t>(
                        std::numeric_limits<int>::max()))));
        }
    }
    for (std::size_t index = 0U; index < race.trackCatalog.size(); ++index)
    {
        const auto& track = race.trackCatalog[index];
        auto* planet = tournament.GetPlanet(track.planetIndex);
        if (planet != nullptr)
        {
            planet->AddTrack(
                static_cast<int>(track.racePass), index,
                track.lapCount);
        }
    }
    if (auto* rewardPlanet =
            tournament.GetPlanet(race.tournamentPlanetIndex))
    {
        for (const auto& reward : race.tournamentCarRewards)
        {
            rewardPlanet->InsertCar(
                {reward.record, static_cast<int>(reward.pass)});
        }
        for (const auto& reward : race.tournamentSlotRewards)
        {
            rewardPlanet->InsertSlot(
                {reward.record, reward.charge, reward.slotType,
                 static_cast<int>(reward.pass)});
        }
    }
    return tournament;
}

std::uint32_t sourceHumanOrOpponentCount(const Race& race) noexcept
{
    return static_cast<std::uint32_t>(std::max<std::size_t>(
        std::count_if(
            race.racers.begin(), race.racers.end(),
            [](const Racer& racer) { return racer.human; }),
        1U));
}

} // namespace

std::size_t resolveOriginalTournamentTrack(
    const Race& race, const PlayerProfile& profile) noexcept
{
    if (race.trackCatalog.empty())
        return 0U;
    try
    {
        auto tournament = makeSourceTournament(race, profile);
        if (tournament.Select(
                profile.currentPlanet,
                static_cast<int>(profile.currentPass),
                profile.currentTrack))
        {
            const auto* track = tournament.GetCurTrack();
            if (track != nullptr)
                return track->catalogIndex;
        }
    }
    catch (...)
    {
        // This function is used while recovering incomplete profile data.
    }
    return 0U;
}

std::uint32_t originalTournamentTrackIndexInPlanet(
    const Race& race, std::size_t trackIndex) noexcept
{
    if (trackIndex >= race.trackCatalog.size())
        return 0U;
    const auto planet = race.trackCatalog[trackIndex].planetIndex;
    return static_cast<std::uint32_t>(std::count_if(
        race.trackCatalog.begin(),
        race.trackCatalog.begin() +
            static_cast<std::ptrdiff_t>(trackIndex),
        [planet](const TrackCatalogEntry& entry) {
            return entry.planetIndex == planet;
        }));
}

void writeOriginalTournamentSelection(
    const Race& race, std::size_t trackIndex,
    PlayerProfile& profile) noexcept
{
    if (trackIndex >= race.trackCatalog.size())
        return;
    const auto& selected = race.trackCatalog[trackIndex];
    profile.currentPlanet = selected.planetIndex;
    profile.currentPass = selected.racePass;
    if (selected.planetIndex < profile.planets.size())
        profile.planets[selected.planetIndex].pass = selected.racePass;
    profile.currentTrack = 0U;
    for (std::size_t index = 0; index < trackIndex; ++index)
    {
        const auto& candidate = race.trackCatalog[index];
        if (candidate.planetIndex == selected.planetIndex &&
            candidate.racePass == selected.racePass)
            ++profile.currentTrack;
    }
}

bool changeOriginalTournamentPlanet(
    const Race& race, std::size_t planetIndex,
    PlayerProfile& profile, bool planetChampion) noexcept
{
    try
    {
        auto tournament = makeSourceTournament(race, profile);
        if (!tournament.Select(
                profile.currentPlanet,
                static_cast<int>(profile.currentPass),
                profile.currentTrack))
        {
            return false;
        }
        auto* planet = tournament.GetPlanet(planetIndex);
        if (planet == nullptr)
            return false;

        // GameMode::ChangePlanet unlocks exactly one destination: the next
        // planet after Race::GetPlanetChampion. Tournament::ChangePlanet then
        // opens it. A closed planet opens without this step, while an
        // unavailable non-next planet stays unavailable and merely receives
        // the source pass-one recovery value.
        if (planetChampion &&
            planet == tournament.GetNextPlanet())
            planet->Unlock();
        if (!tournament.ChangePlanet(planetIndex))
            return false;

        const auto* selectedPlanet = tournament.GetCurPlanet();
        const auto* selectedTrack = tournament.GetCurTrack();
        if (selectedPlanet == nullptr || selectedTrack == nullptr)
            return false;

        profile.currentPlanet = static_cast<std::uint32_t>(planetIndex);
        profile.currentTrack = tournament.GetCurTrackIndex();
        profile.currentPass = static_cast<std::uint32_t>(
            std::max(selectedPlanet->GetPass(), 0));
        if (planetIndex < profile.planets.size())
        {
            profile.planets[planetIndex].state =
                static_cast<std::uint32_t>(selectedPlanet->GetState());
            profile.planets[planetIndex].pass = profile.currentPass;
        }
        return selectedTrack->catalogIndex < race.trackCatalog.size();
    }
    catch (...)
    {
        return false;
    }
}

int originalTournamentRequestPoints(
    const Race& race, std::uint32_t pass) noexcept
{
    try
    {
        source::Planet planet;
        source::Planet::RequestPoints requestPoints;
        for (std::size_t index = 0U;
             index < race.requiredPoints.size(); ++index)
        {
            requestPoints.emplace(
                static_cast<int>(index + 1U),
                static_cast<int>(std::min<std::uint32_t>(
                    race.requiredPoints[index],
                    static_cast<std::uint32_t>(
                        std::numeric_limits<int>::max()))));
        }
        planet.SetRequestPoints(std::move(requestPoints));
        return planet.GetRequestPoints(
            static_cast<int>(pass), sourceHumanOrOpponentCount(race));
    }
    catch (...)
    {
        return -1;
    }
}

TournamentAdvance completeOriginalTournamentTrack(
    const Race& race, std::size_t trackIndex,
    ProfileState& profile) noexcept
{
    return completeOriginalTournamentTrack(
        race, trackIndex, profile, profile.player.points,
        sourceHumanOrOpponentCount(race));
}

TournamentAdvance completeOriginalTournamentTrack(
    const Race& race, std::size_t trackIndex,
    ProfileState& profile, std::uint32_t totalPoints,
    std::uint32_t humanOrOpponentCount) noexcept
{
    TournamentAdvance result;
    if (trackIndex >= race.trackCatalog.size())
        return result;
    const auto& selected = race.trackCatalog[trackIndex];
    const auto planet = selected.planetIndex;
    try
    {
        auto tournament = makeSourceTournament(race, profile.player);
        if (!tournament.Select(
                planet, static_cast<int>(selected.racePass),
                profile.player.currentTrack))
        {
            return result;
        }
        // Select by the actual catalog entry as well.  This keeps recovery
        // correct if a profile's local track index was stale.
        tournament.SetCurTrack(trackIndex);
        const auto advance = tournament.CompleteTrack(
            static_cast<int>(std::min<std::uint32_t>(
                totalPoints,
                static_cast<std::uint32_t>(
                    std::numeric_limits<int>::max()))),
            humanOrOpponentCount);
        result.trackIndex = advance.trackIndex;
        result.passComplete = advance.passComplete;
        result.passChampion = advance.passChampion;
        result.planetChampion = advance.planetChampion;
        result.unlockedSlots = advance.unlockedSlots;
        result.unlockedCars = advance.unlockedCars;

        writeOriginalTournamentSelection(
            race, result.trackIndex, profile.player);
        if (advance.passComplete)
            profile.player.points = 0U;
        profile.player.currentPass = static_cast<std::uint32_t>(
            std::max(advance.planetPass, 0));
        if (planet < profile.player.planets.size())
        {
            auto& progress = profile.player.planets[planet];
            const auto* sourcePlanet = tournament.GetPlanet(planet);
            if (sourcePlanet != nullptr)
            {
                progress.pass = static_cast<std::uint32_t>(
                    std::max(sourcePlanet->GetPass(), 0));
                progress.state = static_cast<std::uint32_t>(
                    sourcePlanet->GetState());
            }
        }

        if (result.planetChampion)
            completeOriginalPlanet(profile, planet);
    }
    catch (...)
    {
        return {};
    }
    return result;
}

FinishTransition originalFinishTransition(
    const TournamentAdvance& advance, std::uint32_t currentPlanet,
    bool campaign) noexcept
{
    if (!campaign)
        return FinishTransition::RaceMenu;
    if (advance.planetChampion)
    {
        return currentPlanet + 1U >= originalTournamentPlanetCount
                   ? FinishTransition::Final
                   : FinishTransition::PlanetCompleted;
    }
    if (advance.passChampion)
        return FinishTransition::PassCompleted;
    if (advance.passComplete)
        return FinishTransition::PassFailed;
    return FinishTransition::RaceMenu;
}

bool runOriginalTournamentProgressSmokeTest(std::string& error)
{
    Race race;
    race.requiredPoints = {100U, 200U};
    race.trackCatalog = {
        {"Data/Map/World5/map15.r3dMap", 4U, "wtWorld5", 4U, 2U},
    };
    race.tournamentPlanetIndex = 4U;
    race.tournamentCarRewards = {{"reward-car", 2U, 0U, {}}};
    race.tournamentSlotRewards = {
        {"reward-slot", 2U, 0U, {}}};
    auto profile = makeOriginalDefaultProfileState();
    profile.player.currentPlanet = 4U;
    profile.player.currentPass = 2U;
    profile.player.currentTrack = 0U;
    profile.player.points = 200U;
    profile.player.planets[4].state = 0U;
    profile.player.planets[4].pass = 2U;
    profile.planetsCompleted.clear();

    auto insufficientTeamProfile = profile;
    insufficientTeamProfile.player.points = 0U;
    const auto insufficientTeamAdvance =
        completeOriginalTournamentTrack(
            race, 0U, insufficientTeamProfile, 299U, 2U);
    if (!insufficientTeamAdvance.passComplete ||
        insufficientTeamAdvance.passChampion ||
        insufficientTeamAdvance.planetChampion)
    {
        error =
            "source network tournament request-point scaling mismatch";
        return false;
    }

    auto exactTeamProfile = profile;
    exactTeamProfile.player.points = 0U;
    const auto exactTeamAdvance =
        completeOriginalTournamentTrack(
            race, 0U, exactTeamProfile, 300U, 2U);
    if (!exactTeamAdvance.passComplete ||
        !exactTeamAdvance.passChampion ||
        !exactTeamAdvance.planetChampion)
    {
        error =
            "Race::GetTotalPoints opponent contribution was not used";
        return false;
    }

    const auto finalAdvance =
        completeOriginalTournamentTrack(race, 0U, profile);
    const auto completed = [&](std::uint32_t planet) {
        return std::find(
                   profile.planetsCompleted.begin(),
                   profile.planetsCompleted.end(),
                   planet) != profile.planetsCompleted.end();
    };
    if (!finalAdvance.passComplete ||
        !finalAdvance.passChampion ||
        !finalAdvance.planetChampion ||
        finalAdvance.unlockedCars !=
            std::vector<std::string>{"reward-car"} ||
        finalAdvance.unlockedSlots !=
            std::vector<std::string>{"reward-slot"} ||
        profile.player.planets[4].state != 3U ||
        !completed(4U) || !completed(5U) ||
        originalFinishTransition(
            finalAdvance, 4U, true) !=
            FinishTransition::Final)
    {
        error =
            "source final-planet completion/unlock transition mismatch";
        return false;
    }

    Race navigationRace;
    navigationRace.trackCatalog = {
        {"Data/Map/World1/map1.r3dMap", 4U, "wtWorld1", 0U, 1U},
        {"Data/Map/World2/map1.r3dMap", 4U, "wtWorld2", 1U, 1U},
        {"Data/Map/World3/map1.r3dMap", 4U, "wtWorld3", 2U, 1U},
        {"Data/Map/World2/map2.r3dMap", 4U, "wtWorld2", 1U, 1U},
    };
    if (originalTournamentTrackIndexInPlanet(
            navigationRace, 0U) != 0U ||
        originalTournamentTrackIndexInPlanet(
            navigationRace, 1U) != 0U ||
        originalTournamentTrackIndexInPlanet(
            navigationRace, 3U) != 1U ||
        originalTournamentTrackIndexInPlanet(
            navigationRace, 99U) != 0U)
    {
        error = "source Tournament::GetCurTrackIndex mapping mismatch";
        return false;
    }
    auto navigationProfile = makeOriginalDefaultProfileState().player;
    navigationProfile.planets[1] = {2U, 0U};
    if (!changeOriginalTournamentPlanet(
            navigationRace, 1U, navigationProfile, true) ||
        navigationProfile.currentPlanet != 1U ||
        navigationProfile.currentTrack != 0U ||
        navigationProfile.currentPass != 1U ||
        navigationProfile.planets[1].state != 0U ||
        navigationProfile.planets[1].pass != 1U ||
        resolveOriginalTournamentTrack(
            navigationRace, navigationProfile) != 1U)
    {
        error = "source Tournament::ChangePlanet transition mismatch";
        return false;
    }

    auto unavailableProfile = makeOriginalDefaultProfileState().player;
    unavailableProfile.planets[1] = {2U, 0U};
    unavailableProfile.planets[2] = {2U, 0U};
    if (!changeOriginalTournamentPlanet(
            navigationRace, 2U, unavailableProfile, false) ||
        unavailableProfile.currentPlanet != 2U ||
        unavailableProfile.currentPass != 1U ||
        unavailableProfile.planets[2].state != 2U ||
        unavailableProfile.planets[2].pass != 1U)
    {
        error =
            "GameMode::ChangePlanet opened an unavailable non-next planet";
        return false;
    }

    auto currentProfile = makeOriginalDefaultProfileState().player;
    currentProfile.planets[0] = {1U, 1U};
    if (!changeOriginalTournamentPlanet(
            navigationRace, 0U, currentProfile, true) ||
        currentProfile.currentPlanet != 0U ||
        currentProfile.planets[0].state != 1U)
    {
        error =
            "GameMode::ChangePlanet mutated the already-current planet";
        return false;
    }

    TournamentAdvance transition;
    transition.passComplete = true;
    if (originalFinishTransition(
            transition, 0U, true) !=
            FinishTransition::PassFailed)
    {
        error = "source failed-pass finish transition mismatch";
        return false;
    }
    transition.passChampion = true;
    if (originalFinishTransition(
            transition, 0U, true) !=
            FinishTransition::PassCompleted)
    {
        error = "source pass-champion finish transition mismatch";
        return false;
    }
    transition.planetChampion = true;
    if (originalFinishTransition(
            transition, 3U, true) !=
            FinishTransition::PlanetCompleted ||
        originalFinishTransition(
            transition, 3U, false) !=
            FinishTransition::RaceMenu)
    {
        error = "source planet/skirmish finish transition mismatch";
        return false;
    }
    return true;
}

void applyOriginalPlayerProfile(
    Race& race, const resource::ResourceFileSystem& resources,
    const PlayerProfile& profile, bool armor4Opened)
{
    if (race.racers.empty() ||
        race.racers.front().vehicle >= race.vehicles.size())
        return;
    if (race.workshop.empty())
        race.workshop = loadOriginalWorkshop(resources);
    const auto& workshop = race.workshop;
    auto& human = race.racers.front();
    human.color = profile.color;
    human.gamerId = profile.gamerId;
    if (!race.playerIdentities.empty())
    {
        // Race::StartRace assigns each ordinary computer its numeric player
        // id, then replaces a duplicate gamer id with the first unused entry
        // from Tournament::GetGamers().  This matters when the human selected
        // Snake/Tarquin and prevents two copies of that character.
        for (std::size_t index = 1U; index < race.racers.size(); ++index)
        {
            auto& racer = race.racers[index];
            if (racer.human)
                continue;
            const bool duplicate = std::any_of(
                race.racers.begin(), race.racers.end(),
                [&](const Racer& other) {
                    return &other != &racer &&
                           other.gamerId == racer.gamerId;
                });
            if (!duplicate)
                continue;
            const auto replacement = std::find_if(
                race.playerIdentities.begin(), race.playerIdentities.end(),
                [&](const PlayerIdentity& candidate) {
                    return candidate.planetIndex < 0 && std::none_of(
                        race.racers.begin(), race.racers.end(),
                        [&](const Racer& other) {
                            return other.gamerId ==
                                   static_cast<std::uint32_t>(candidate.id);
                        });
                });
            if (replacement != race.playerIdentities.end())
                racer.gamerId = static_cast<std::uint32_t>(replacement->id);
        }

        // Tournament::GetPlayerData searches the global gamer catalog before
        // the current planet.  IDs 4 and 5 therefore mean Snake and Tarquin;
        // scComp4/scComp5 are only fallback car/loadout records and must never
        // leak into HUD or FinishMenu.
        for (auto& racer : race.racers)
        {
            const auto* identity = findOriginalPlayerIdentity(
                race, static_cast<int>(racer.gamerId));
            if (identity != nullptr)
            {
                racer.name = identity->name;
                racer.photoPath = identity->photoPath;
            }
        }
    }
    human.configuredVehicle = race.vehicles[human.vehicle];
    human.hasConfiguredVehicle = true;
    std::vector<RacerSlot> humanLoadout;
    humanLoadout.reserve(4);
    for (std::size_t profileSlot = 0; profileSlot < 4U;
         ++profileSlot)
    {
        static constexpr std::array<std::string_view, 4> types{
            "stWheel", "stTruba", "stArmor", "stMotor"};
        humanLoadout.push_back(
            {profile.slots[profileSlot].record,
             std::string(types[profileSlot]),
             profile.slots[profileSlot].charge});
    }
    human.loadout = humanLoadout;
    applyMobilityLoadout(human.configuredVehicle, workshop,
                         human.loadout, profile.difficulty,
                         human.human, armor4Opened);
    race.vehicle = human.configuredVehicle;

    for (std::size_t index = 1; index < race.racers.size(); ++index)
    {
        auto& racer = race.racers[index];
        racer.configuredVehicle = race.vehicles[racer.vehicle];
        racer.hasConfiguredVehicle = true;
        applyMobilityLoadout(racer.configuredVehicle, workshop,
                             racer.loadout, profile.difficulty,
                             racer.human);
    }
}

void reconcileOriginalPlayerRoster(
    Race& race, std::uint32_t computerCount, bool campaign)
{
    if (race.racers.empty())
        return;

    computerCount = std::min(
        computerCount, originalMaximumComputers);
    const auto targetCount =
        static_cast<std::size_t>(computerCount) + 1U;
    if (race.racers.size() > targetCount)
        race.racers.resize(targetCount);

    while (race.racers.size() < targetCount)
    {
        const auto playerId = static_cast<std::uint32_t>(
            race.racers.size());
        std::uint32_t templateId = playerId;
        if (templateId > originalComputerDefinitionCount)
        {
            // Literal Planet::StartPass mapping for Player IDs beyond
            // cComputer5. Campaign excludes the boss from the reuse ring;
            // skirmish cycles through all five serialized computer records.
            templateId = campaign
                             ? (templateId - 1U) %
                                       (originalComputerDefinitionCount - 1U) +
                                   2U
                             : (templateId - 1U) %
                                       originalComputerDefinitionCount +
                                   1U;
        }
        const Racer* source = nullptr;
        if (templateId > 0U &&
            templateId <= race.computerTemplates.size())
        {
            source = &race.computerTemplates[templateId - 1U];
        }
        else
        {
            const auto active = std::find_if(
                race.racers.begin(), race.racers.end(),
                [templateId](const Racer& racer) {
                    return racer.playerId == static_cast<int>(templateId);
                });
            if (active != race.racers.end())
                source = &*active;
        }
        if (source == nullptr)
            break;

        Racer racer = *source;
        racer.playerId = static_cast<int>(playerId);
        racer.gamerId = playerId;
        racer.netSlot = 0U;
        racer.netName.clear();
        racer.human = false;
        racer.color = sourcePlayerColor(playerId - 1U);
        racer.mapObjectId = race.firstDynamicMapObjectId + playerId;
        if (const auto* identity = findOriginalPlayerIdentity(
                race, static_cast<int>(racer.gamerId)))
        {
            racer.name = identity->name;
            racer.photoPath = identity->photoPath;
        }
        race.racers.push_back(std::move(racer));
    }

    // Race::CreatePlayers/DelPlayer changes the dynamic MapObj creation
    // order together with the active list. Keep the canonical contiguous IDs
    // when a menu stepper shrinks and later expands the roster.
    for (std::size_t index = 0U; index < race.racers.size(); ++index)
        race.racers[index].mapObjectId =
            race.firstDynamicMapObjectId +
            static_cast<std::uint32_t>(index);
}

std::uint32_t originalEffectiveLapCount(
    const Race& race, bool campaign,
    std::uint32_t configuredLaps) noexcept
{
    return campaign ? race.lapCount : configuredLaps;
}

void applyOriginalSkirmishComputerConfig(
    Race& race, const OriginalGarageCatalog& garage,
    std::uint32_t upgradeMaxLevel, std::uint32_t weaponMaxLevel,
    std::string_view difficulty)
{
    static constexpr std::array<std::string_view,
                                static_cast<std::size_t>(
                                    GarageSlotType::Count)>
        slotTypes{
            "stWheel", "stTruba", "stArmor", "stMotor", "stHyper",
            "stMine", "stWeapon1", "stWeapon2", "stWeapon3",
            "stWeapon4"};
    const auto maximumUpgrade = static_cast<int>(
        std::min<std::uint32_t>(upgradeMaxLevel, 2U));
    const auto primaryMountCount =
        std::min<std::uint32_t>(weaponMaxLevel, 4U);

    for (auto& racer : race.racers)
    {
        // Player::IsComputer tests the low byte of the source id.  A remote
        // cHuman uses cOpponentBit and must keep its network loadout.
        if ((racer.playerId & source::Player::computerMask) == 0 ||
            racer.vehicle >= race.vehicles.size())
        {
            continue;
        }
        const auto& baseVehicle = race.vehicles[racer.vehicle];
        const auto* car = garage.findCar(baseVehicle.record);
        if (car == nullptr)
            continue;

        for (std::size_t index = 0U; index < 4U; ++index)
        {
            const auto type = static_cast<GarageSlotType>(index);
            const auto* upgrade = originalWorkshopUpgradeItem(
                garage, *car, type, maximumUpgrade);
            if (upgrade == nullptr)
                continue;
            const auto slot = std::find_if(
                racer.loadout.begin(), racer.loadout.end(),
                [&](const RacerSlot& value) {
                    return value.type == slotTypes[index];
                });
            if (slot != racer.loadout.end())
                slot->record = upgrade->record;
            else
                racer.loadout.push_back(
                    {upgrade->record, std::string(slotTypes[index]), 0U});
        }

        // Garage::UpgradeCar(..., true) refills every installed WeaponItem,
        // including Hyper and Mine, before StartPass removes disabled primary
        // mounts.  Preserve that source order and its maximum-charge values.
        for (auto& slot : racer.loadout)
        {
            const auto* item = garage.findItem(slot.record);
            if (item != nullptr && item->maximumCharge > 0U)
                slot.charge = item->maximumCharge;
        }
        racer.loadout.erase(
            std::remove_if(
                racer.loadout.begin(), racer.loadout.end(),
                [&](const RacerSlot& slot) {
                    for (std::size_t index = 0U; index < 4U; ++index)
                    {
                        if (slot.type == slotTypes[
                                             static_cast<std::size_t>(
                                                 GarageSlotType::Weapon1) +
                                             index])
                            return index >= primaryMountCount;
                    }
                    return false;
                }),
            racer.loadout.end());

        racer.configuredVehicle = baseVehicle;
        racer.hasConfiguredVehicle = true;
        applyMobilityLoadout(
            racer.configuredVehicle, race.workshop, racer.loadout,
            difficulty, false);
    }
}

std::vector<DecorationDebrisDefinition> makeDecorationDestruction(
    const Race& race, const resource::ResourceFileSystem& resources,
    std::size_t instanceIndex)
{
    if (instanceIndex >= race.decorationInstances.size())
    {
        throw resource::ResourceError(
            race.levelPath + ": invalid destruction instance " +
            std::to_string(instanceIndex));
    }
    const auto& instance = race.decorationInstances[instanceIndex];
    if (instance.definition >= race.decorationDefinitions.size())
    {
        throw resource::ResourceError(
            race.levelPath + ": invalid destruction definition " +
            std::to_string(instance.definition));
    }
    const auto& definition =
        race.decorationDefinitions[instance.definition];
    std::vector<DecorationDebrisDefinition> result;
    result.reserve(definition.destructionPieces.size());
    for (std::size_t pieceIndex = 0;
         pieceIndex < definition.destructionPieces.size(); ++pieceIndex)
    {
        const auto& piece = definition.destructionPieces[pieceIndex];
        r3d::physics::DebrisDescription debris;
        // DestrObj::OnProgress explicitly overwrites each detached child's
        // world position and rotation with its former parent's pose.  The
        // child's serialized transform remains local in its visual/shape.
        debris.transform = instance.transform;
        debris.dynamic = piece.dynamic;
        debris.shapePosition = {
            piece.shapePosition.x * instance.transform.scale.x,
            piece.shapePosition.y * instance.transform.scale.y,
            piece.shapePosition.z * instance.transform.scale.z};
        debris.shapeRotation = piece.shapeRotation;
        debris.halfExtents = {
            std::abs(piece.halfExtents.x * instance.transform.scale.x),
            std::abs(piece.halfExtents.y * instance.transform.scale.y),
            std::abs(piece.halfExtents.z * instance.transform.scale.z)};
        debris.mass = piece.mass;
        if (!piece.dynamic)
        {
            for (const auto& shape : piece.collisionShapes)
            {
                const auto sourceMesh =
                    resource::loadR3DMeshAsset(resources, shape.meshPath);
                r3d::physics::TriangleMesh collision;
                collision.surface =
                    r3d::physics::CollisionSurface::Decoration;
                collision.transform = piece.transform;
                collision.transform.position.x *=
                    instance.transform.scale.x;
                collision.transform.position.y *=
                    instance.transform.scale.y;
                collision.transform.position.z *=
                    instance.transform.scale.z;
                collision.transform.scale.x *= instance.transform.scale.x;
                collision.transform.scale.y *= instance.transform.scale.y;
                collision.transform.scale.z *= instance.transform.scale.z;
                collision.vertices.reserve(sourceMesh.vertices.size());
                for (const auto& vertex : sourceMesh.vertices)
                {
                    collision.vertices.push_back(
                        {vertex.position[0], vertex.position[1],
                         vertex.position[2]});
                }
                if (shape.materialGroup < sourceMesh.materialGroups.size())
                {
                    const auto& group =
                        sourceMesh.materialGroups[shape.materialGroup];
                    collision.indices.insert(
                        collision.indices.end(),
                        sourceMesh.indices.begin() + group.firstIndex,
                        sourceMesh.indices.begin() + group.firstIndex +
                            group.indexCount);
                }
                else
                {
                    collision.indices = sourceMesh.indices;
                }
                if (!collision.indices.empty())
                    debris.collisionMeshes.push_back(std::move(collision));
            }
            // A detached source child without a PhysX shape remains visual
            // only.  It therefore needs no backend body or render binding.
            if (debris.collisionMeshes.empty())
                continue;
        }
        // Destruction-list children have neither an added launch impulse nor
        // maxTimeLife in the Windows catalog.  DebrisDescription defaults
        // deliberately preserve both facts.
        result.push_back({pieceIndex, std::move(debris)});
    }
    return result;
}

r3d::physics::WorldDescription makePhysicsDescription(
    const Race& race, const resource::ResourceFileSystem& resources)
{
    r3d::physics::WorldDescription result;
    result.vehicle = race.vehicle.physics;
    auto prepareVehicle = [&](const Vehicle& source) {
        auto vehicle = source.physics;
        float wheelWidth = 0.2F;
        if (!source.wheelMeshPath.empty())
        {
            const auto wheelMesh = resource::loadR3DMeshAsset(
                resources, source.wheelMeshPath);
            wheelWidth =
                std::max(wheelMesh.maximum[1] - wheelMesh.minimum[1],
                         0.05F);
        }
        else if (!vehicle.wheels.empty())
        {
            wheelWidth = std::max(vehicle.wheels.front().radius * 0.5F,
                                  0.05F);
        }
        for (auto& wheel : vehicle.wheels)
            wheel.width = wheelWidth;
        return vehicle;
    };
    result.vehicle = prepareVehicle(race.vehicle);
    result.collisionMeshes = race.collisionMeshes;
    if (result.collisionMeshes.size() !=
        race.collisionMeshDecorationInstances.size())
    {
        throw resource::ResourceError(
            race.levelPath + ": collision decoration owner mismatch");
    }
    for (std::size_t index = 0; index < result.collisionMeshes.size();
         ++index)
    {
        result.collisionMeshes[index].decorationInstance =
            race.collisionMeshDecorationInstances[index];
    }
    result.decorations.reserve(race.decorationInstances.size());
    for (const auto& instance : race.decorationInstances)
    {
        const auto& definition =
            race.decorationDefinitions.at(instance.definition);
        r3d::physics::DecorationDescription decoration;
        decoration.transform = instance.transform;
        decoration.shapePosition = {
            definition.bodyShapePosition.x * instance.transform.scale.x,
            definition.bodyShapePosition.y * instance.transform.scale.y,
            definition.bodyShapePosition.z * instance.transform.scale.z};
        decoration.shapeRotation = definition.bodyShapeRotation;
        decoration.halfExtents = {
            std::abs(definition.bodyHalfExtents.x *
                     instance.transform.scale.x),
            std::abs(definition.bodyHalfExtents.y *
                     instance.transform.scale.y),
            std::abs(definition.bodyHalfExtents.z *
                     instance.transform.scale.z)};
        decoration.mass = definition.bodyMass;
        decoration.hasBodyShape =
            decoration.halfExtents.x > 0.0F &&
            decoration.halfExtents.y > 0.0F &&
            decoration.halfExtents.z > 0.0F;
        decoration.dynamic =
            definition.dynamicBody && decoration.hasBodyShape;
        // Actor::InitRootNxActor merges every destruction-list child shape
        // into the parent actor. DestrObj gives that actor
        // NX_AF_DISABLE_RESPONSE: it reports the zero-damage touch that kills
        // the object, while OnDeath detaches the pieces into their own actors.
        decoration.collisionResponse = !definition.destructible;
        if (definition.destructible)
        {
            for (const auto& piece : definition.destructionPieces)
            {
                if (piece.halfExtents.x <= 0.0F ||
                    piece.halfExtents.y <= 0.0F ||
                    piece.halfExtents.z <= 0.0F)
                {
                    continue;
                }
                r3d::physics::DecorationDescription::ChildShape childShape;
                childShape.position = {
                    piece.shapePosition.x * instance.transform.scale.x,
                    piece.shapePosition.y * instance.transform.scale.y,
                    piece.shapePosition.z * instance.transform.scale.z};
                childShape.rotation = piece.shapeRotation;
                childShape.halfExtents = {
                    std::abs(piece.halfExtents.x *
                             instance.transform.scale.x),
                    std::abs(piece.halfExtents.y *
                             instance.transform.scale.y),
                    std::abs(piece.halfExtents.z *
                             instance.transform.scale.z)};
                decoration.childShapes.push_back(childShape);
            }
        }
        result.decorations.push_back(std::move(decoration));
    }

    auto findPoint = [&](std::uint32_t id) -> const TracePoint& {
        const auto found = std::find_if(
            race.tracePoints.begin(), race.tracePoints.end(),
            [id](const TracePoint& point) { return point.id == id; });
        if (found == race.tracePoints.end())
            throw resource::ResourceError(race.levelPath +
                                          ": unresolved trace point");
        return *found;
    };
    const auto& first = findPoint(race.tracePath[0]);
    const auto& next = findPoint(race.tracePath[1]);
    result.startPosition = first.position;
    result.startPosition.z += 2.0F; // Race::ResetCarPos legacy contract.
    result.startDirection = {next.position.x - first.position.x,
                             next.position.y - first.position.y, 0.0F};
    const float length = std::sqrt(
        result.startDirection.x * result.startDirection.x +
        result.startDirection.y * result.startDirection.y);
    if (length <= 0.0001F)
        throw resource::ResourceError(race.levelPath +
                                      ": zero start direction");
    result.startDirection.x /= length;
    result.startDirection.y /= length;
    // WayNode::Tile::ApplyChanges uses Vec2NormCW(dir), i.e. (y, -x).
    // Keeping the same lateral normal preserves the Windows grid identity
    // order instead of mirroring every row across the track centre line.
    const Vec3 lineDirection{result.startDirection.y,
                             -result.startDirection.x, 0.0F};
    std::vector<float> racerWidths;
    racerWidths.reserve(race.racers.size());
    for (const auto& racer : race.racers)
    {
        const auto& source = race.vehicles.at(racer.vehicle);
        const auto bodyMesh = resource::loadR3DMeshAsset(
            resources, source.bodyMeshPath);
        racerWidths.push_back(
            std::max((bodyMesh.maximum[1] - bodyMesh.minimum[1]) *
                         std::abs(source.bodyVisualTransform.scale.y),
                     1.0F));
    }
    constexpr std::size_t rowLength = 4;
    constexpr float rowSpace = 7.0F;
    for (std::size_t rowBegin = 0; rowBegin < race.racers.size();
         rowBegin += rowLength)
    {
        const std::size_t rowEnd =
            std::min(rowBegin + rowLength, race.racers.size());
        float rowWidth = 0.0F;
        for (std::size_t index = rowBegin; index < rowEnd; ++index)
            rowWidth += racerWidths[index];
        const float lateralSpace = std::max(
            (first.width - rowWidth) /
                (static_cast<float>(rowEnd - rowBegin) + 1.0F),
            0.0F);
        float lateralStep = 0.0F;
        for (std::size_t index = rowBegin; index < rowEnd; ++index)
        {
            Vec3 position = result.startPosition;
            position.x += result.startDirection.x *
                          (0.1F -
                           static_cast<float>(index / rowLength) *
                               rowSpace);
            position.y += result.startDirection.y *
                          (0.1F -
                           static_cast<float>(index / rowLength) *
                               rowSpace);
            const float lateral =
                -first.width * 0.5F + lateralSpace +
                racerWidths[index] * 0.5F + lateralStep;
            position.x += lineDirection.x * lateral;
            position.y += lineDirection.y * lateral;
            lateralStep += racerWidths[index] + lateralSpace;
            const auto& racer = race.racers[index];
            const auto& vehicle =
                racer.hasConfiguredVehicle
                    ? racer.configuredVehicle
                    : race.vehicles.at(racer.vehicle);
            result.spawns.push_back(
                {prepareVehicle(vehicle), position, result.startDirection});
        }
    }
    return result;
}

bool runOriginalRaceResourceSmokeTest(
    const Race& race, const resource::ResourceFileSystem& resources,
    std::string& error)
{
    try
    {
        const auto workshop = loadOriginalWorkshop(resources);
        const auto garage = loadOriginalGarage(resources);
        if (race.workshop.size() != workshop.size() ||
            race.workshop.empty())
        {
            error =
                "active Race did not retain the source workshop catalog";
            return false;
        }
        const auto manticoraVehicle = std::find_if(
            race.vehicles.begin(), race.vehicles.end(),
            [](const Vehicle& vehicle) {
                return basename(vehicle.record) == "manticora";
            });
        if (manticoraVehicle == race.vehicles.end() ||
            !manticoraVehicle->slotMounts[
                 static_cast<std::size_t>(GarageSlotType::Hyper)].active ||
            manticoraVehicle->slotMounts[
                 static_cast<std::size_t>(GarageSlotType::Hyper)]
                    .placements.empty() ||
            std::abs(
                manticoraVehicle->slotMounts[
                    static_cast<std::size_t>(GarageSlotType::Hyper)]
                    .placements.front().offset.x +
                1.5F) > 0.001F)
        {
            error =
                "source Garage::Car Hyper/Mine physical mounts were not "
                "retained";
            return false;
        }
        const auto windowsDebugRace =
            loadFirstOriginalRace(resources, true);
        const auto patagonisDebugTrack = std::find_if(
            windowsDebugRace.trackCatalog.begin(),
            windowsDebugRace.trackCatalog.end(),
            [](const TrackCatalogEntry& track) {
                return track.levelPath ==
                           "Data/Map/debugTrack.r3dMap" &&
                       track.lapCount == 99U &&
                       track.planetIndex == 1U &&
                       track.racePass == 1U;
            });
        const auto nhoDebugTrack = std::find_if(
            windowsDebugRace.trackCatalog.begin(),
            windowsDebugRace.trackCatalog.end(),
            [](const TrackCatalogEntry& track) {
                return track.levelPath ==
                           "Data/Map/World5/map0.r3dMap" &&
                       track.lapCount == 99U &&
                       track.planetIndex == 3U &&
                       track.racePass == 1U;
            });
        if (windowsDebugRace.trackCatalog.size() != 90U ||
            patagonisDebugTrack ==
                windowsDebugRace.trackCatalog.end() ||
            nhoDebugTrack == windowsDebugRace.trackCatalog.end() ||
            patagonisDebugTrack ==
                windowsDebugRace.trackCatalog.begin() ||
            std::prev(patagonisDebugTrack)->planetIndex == 1U ||
            nhoDebugTrack == windowsDebugRace.trackCatalog.begin() ||
            std::prev(nhoDebugTrack)->planetIndex == 3U)
        {
            error =
                "Windows _DEBUG 99-lap track injection/order mismatch";
            return false;
        }
        if (race.racers.size() < 2U)
        {
            error = "source Player::ApplyMobility role regression has no AI";
            return false;
        }
        if (race.racers.front().playerId != source::Player::humanId ||
            !std::all_of(
                race.racers.begin() + 1U, race.racers.end(),
                [&](const Racer& racer) {
                    const auto index = static_cast<int>(
                        &racer - race.racers.data());
                    return racer.playerId == index;
                }))
        {
            error = "source Race::AddPlayer identifiers were not preserved";
            return false;
        }
        const auto unresolvedComputer = std::find_if(
            race.racers.begin(), race.racers.end(),
            [](const Racer& racer) {
                return racer.name == "scComp4" ||
                       racer.name == "scComp5";
            });
        const auto snake = std::find_if(
            race.racers.begin(), race.racers.end(),
            [](const Racer& racer) {
                return racer.gamerId == 4U;
            });
        const auto tarquin = std::find_if(
            race.racers.begin(), race.racers.end(),
            [](const Racer& racer) {
                return racer.gamerId == 5U;
            });
        const auto* globalSnake = findOriginalPlayerIdentity(race, 4);
        const auto* intariaPlayer = findOriginalPlayerIdentity(race, 1);
        auto patagonisIdentityRace = race;
        patagonisIdentityRace.tournamentPlanetIndex = 1U;
        const auto* patagonisPlayer =
            findOriginalPlayerIdentity(patagonisIdentityRace, 1);
        if (unresolvedComputer != race.racers.end() ||
            snake == race.racers.end() || snake->name != "scSnake" ||
            snake->photoPath.find("snake.png") == std::string::npos ||
            tarquin == race.racers.end() ||
            tarquin->name != "scTarquin" ||
            tarquin->photoPath.find("tarquin.png") == std::string::npos ||
            globalSnake == nullptr || globalSnake->name != "scSnake" ||
            globalSnake->planetIndex >= 0 || intariaPlayer == nullptr ||
            intariaPlayer->name != "scMardock" ||
            intariaPlayer->planetIndex != 0 ||
            patagonisPlayer == nullptr ||
            patagonisPlayer->name != "scStinkle" ||
            patagonisPlayer->planetIndex != 1)
        {
            error =
                "source Tournament::GetPlayerData computer identity "
                "priority was not preserved";
            return false;
        }
        auto duplicateIdentityRace = race;
        auto snakeProfile = makeOriginalDefaultProfileState().player;
        snakeProfile.gamerId = 4U;
        applyOriginalPlayerProfile(
            duplicateIdentityRace, resources, snakeProfile);
        const bool uniqueGamerIds = std::all_of(
            duplicateIdentityRace.racers.begin(),
            duplicateIdentityRace.racers.end(),
            [&](const Racer& racer) {
                return std::count_if(
                           duplicateIdentityRace.racers.begin(),
                           duplicateIdentityRace.racers.end(),
                           [&](const Racer& other) {
                               return other.gamerId == racer.gamerId;
                           }) == 1;
            });
        const auto replacement = std::find_if(
            duplicateIdentityRace.racers.begin() + 1,
            duplicateIdentityRace.racers.end(),
            [](const Racer& racer) {
                return racer.gamerId == 10U &&
                       racer.name == "svTyler" &&
                       racer.photoPath.find("tyler.png") !=
                           std::string::npos;
            });
        if (!uniqueGamerIds ||
            replacement == duplicateIdentityRace.racers.end())
        {
            error =
                "source Race::StartRace duplicate gamer replacement was "
                "not preserved";
            return false;
        }
        auto skirmishRoster = race;
        reconcileOriginalPlayerRoster(
            skirmishRoster, originalMaximumComputers, false);
        const auto skirmishLoadoutMatches =
            skirmishRoster.racers.size() == originalMaximumPlayers &&
            !skirmishRoster.racers[6].loadout.empty() &&
            !skirmishRoster.racers[7].loadout.empty() &&
            skirmishRoster.racers[6].vehicle == race.racers[1].vehicle &&
            skirmishRoster.racers[6].loadout.front().record ==
                race.racers[1].loadout.front().record &&
            skirmishRoster.racers[7].vehicle == race.racers[2].vehicle &&
            skirmishRoster.racers[7].loadout.front().record ==
                race.racers[2].loadout.front().record;
        if (!skirmishLoadoutMatches ||
            skirmishRoster.racers[6].playerId != 6 ||
            skirmishRoster.racers[6].gamerId != 6U ||
            skirmishRoster.racers[6].name != "scGarry" ||
            skirmishRoster.racers[7].playerId != 7 ||
            skirmishRoster.racers[7].gamerId != 7U ||
            skirmishRoster.racers[7].name != "svKristoph" ||
            skirmishRoster.racers.back().mapObjectId !=
                race.firstDynamicMapObjectId + 7U ||
            std::abs(skirmishRoster.racers[6].color[0] -
                     216.0F / 255.0F) > 0.001F ||
            originalEffectiveLapCount(race, true, 8U) !=
                race.lapCount ||
            originalEffectiveLapCount(race, false, 8U) != 8U)
        {
            error =
                "source eight-player Race::CreatePlayers/Track laps "
                "contract mismatch";
            return false;
        }
        auto campaignOverflowRoster = race;
        reconcileOriginalPlayerRoster(
            campaignOverflowRoster, originalMaximumComputers, true);
        if (campaignOverflowRoster.racers.size() !=
                originalMaximumPlayers ||
            campaignOverflowRoster.racers[6].vehicle !=
                race.racers[3].vehicle ||
            campaignOverflowRoster.racers[7].vehicle !=
                race.racers[4].vehicle)
        {
            error =
                "source campaign cComputer5 overflow mapping mismatch";
            return false;
        }
        reconcileOriginalPlayerRoster(skirmishRoster, 2U, false);
        if (skirmishRoster.racers.size() != 3U ||
            skirmishRoster.racers.back().playerId != 2 ||
            skirmishRoster.racers.back().mapObjectId !=
                race.firstDynamicMapObjectId + 2U)
        {
            error = "source Race::CreatePlayers shrink mismatch";
            return false;
        }
        reconcileOriginalPlayerRoster(
            skirmishRoster, originalMaximumComputers, false);
        if (skirmishRoster.racers.size() != originalMaximumPlayers ||
            skirmishRoster.racers[6].vehicle != race.racers[1].vehicle ||
            skirmishRoster.racers[7].vehicle != race.racers[2].vehicle)
        {
            error =
                "source Race::CreatePlayers regrow from Planet data "
                "mismatch";
            return false;
        }
        auto humanEasy = race.vehicles.at(race.racers.front().vehicle);
        auto humanNormal = humanEasy;
        auto humanHard = humanEasy;
        applyMobilityLoadout(
            humanEasy, workshop, race.racers.front().loadout,
            "gdEasy", true);
        applyMobilityLoadout(
            humanNormal, workshop, race.racers.front().loadout,
            "gdNormal", true);
        applyMobilityLoadout(
            humanHard, workshop, race.racers.front().loadout,
            "gdHard", true);
        auto computerEasy = race.vehicles.at(race.racers[1].vehicle);
        auto computerHard = computerEasy;
        applyMobilityLoadout(
            computerEasy, workshop, race.racers[1].loadout,
            "gdEasy", false);
        applyMobilityLoadout(
            computerHard, workshop, race.racers[1].loadout,
            "gdHard", false);
        const auto nearMobility = [](float first, float second) {
            return std::abs(first - second) <= 0.0001F;
        };
        const bool sourceArmorRoleScaling =
            humanHard.maximumLife > 0.0F &&
            computerHard.maximumLife > 0.0F &&
            nearMobility(
                humanEasy.maximumLife,
                humanHard.maximumLife * (2.0F / 1.5F)) &&
            nearMobility(
                humanNormal.maximumLife,
                humanHard.maximumLife * (1.75F / 1.5F)) &&
            nearMobility(
                computerEasy.maximumLife,
                computerHard.maximumLife) &&
            race.racers[1].hasConfiguredVehicle &&
            nearMobility(
                race.racers[1].configuredVehicle.maximumLife,
                computerHard.maximumLife);
        if (!sourceArmorRoleScaling)
        {
            error =
                "source Player::ApplyMobility cHumanArmorK role mismatch";
            return false;
        }
        auto skirmishRace = race;
        const auto humanLoadout = skirmishRace.racers.front().loadout;
        applyOriginalSkirmishComputerConfig(
            skirmishRace, garage, 1U, 1U, "gdHard");
        const bool humanUntouched =
            skirmishRace.racers.front().loadout.size() ==
                humanLoadout.size() &&
            std::equal(
                skirmishRace.racers.front().loadout.begin(),
                skirmishRace.racers.front().loadout.end(),
                humanLoadout.begin(),
                [](const RacerSlot& left, const RacerSlot& right) {
                    return left.record == right.record &&
                           left.type == right.type &&
                           left.charge == right.charge;
                });
        const bool computerConfigMatches = std::all_of(
            skirmishRace.racers.begin() + 1U,
            skirmishRace.racers.end(),
            [&](const Racer& racer) {
                if ((racer.playerId & source::Player::computerMask) == 0 ||
                    racer.vehicle >= skirmishRace.vehicles.size())
                    return true;
                const auto* car = garage.findCar(
                    skirmishRace.vehicles[racer.vehicle].record);
                if (car == nullptr || !racer.hasConfiguredVehicle)
                    return false;
                for (std::size_t index = 0U; index < 4U; ++index)
                {
                    const auto type = static_cast<GarageSlotType>(index);
                    const auto* expected = originalWorkshopUpgradeItem(
                        garage, *car, type, 1);
                    if (expected == nullptr)
                        continue;
                    const auto slot = std::find_if(
                        racer.loadout.begin(), racer.loadout.end(),
                        [&](const RacerSlot& value) {
                            return value.type ==
                                       std::array<std::string_view, 4>{
                                           "stWheel", "stTruba", "stArmor",
                                           "stMotor"}[index] &&
                                   value.record == expected->record;
                        });
                    if (slot == racer.loadout.end())
                        return false;
                }
                for (const auto& slot : racer.loadout)
                {
                    if (slot.type == "stWeapon2" ||
                        slot.type == "stWeapon3" ||
                        slot.type == "stWeapon4")
                        return false;
                    const auto* item = garage.findItem(slot.record);
                    if (item != nullptr && item->maximumCharge > 0U &&
                        slot.charge != item->maximumCharge)
                        return false;
                }
                return true;
            });
        if (!humanUntouched || !computerConfigMatches)
        {
            error =
                "source Planet::StartPass skirmish computer configuration "
                "mismatch";
            return false;
        }
        auto armorProfile = makeOriginalDefaultProfileState().player;
        armorProfile.difficulty = "gdHard";
        armorProfile.slots[2].record =
            "world\\race\\workshopRoot\\workshop\\armor3";
        auto standardArmorRace = race;
        auto armor4Race = race;
        applyOriginalPlayerProfile(
            standardArmorRace, resources, armorProfile, false);
        applyOriginalPlayerProfile(
            armor4Race, resources, armorProfile, true);
        if (standardArmorRace.racers.empty() ||
            armor4Race.racers.empty() ||
            std::abs(
                armor4Race.racers.front()
                        .configuredVehicle.maximumLife -
                    standardArmorRace.racers.front()
                        .configuredVehicle.maximumLife -
                    15.0F) > 0.0001F)
        {
            error =
                "ArmorItem::CheckArmor4 human hard-life bonus mismatch";
            return false;
        }
        const auto physics = makePhysicsDescription(race, resources);
        if (physics.spawns.size() >= 2U)
        {
            const float firstToSecondX =
                physics.spawns[1].position.x -
                physics.spawns[0].position.x;
            const float firstToSecondY =
                physics.spawns[1].position.y -
                physics.spawns[0].position.y;
            const float clockwiseLateral =
                firstToSecondX * physics.startDirection.y -
                firstToSecondY * physics.startDirection.x;
            if (clockwiseLateral <= 0.0F)
            {
                error =
                    "WayNode::Tile clockwise start-grid order was mirrored";
                return false;
            }
        }
        const auto playerWheel = resource::loadR3DMeshAsset(
            resources, race.vehicle.wheelMeshPath);
        const float playerWheelRadius = std::max(
            {std::abs(playerWheel.minimum[0]),
             std::abs(playerWheel.maximum[0]),
             std::abs(playerWheel.minimum[2]),
             std::abs(playerWheel.maximum[2])});
        bool usesWorkshopPreviewWheel = false;
        for (const auto& racer : race.racers)
        {
            if (!racer.hasConfiguredVehicle)
                continue;
            usesWorkshopPreviewWheel =
                usesWorkshopPreviewWheel || std::any_of(
                    racer.configuredVehicle.wheelVisuals.begin(),
                    racer.configuredVehicle.wheelVisuals.end(),
                    [](const VisualNode& visual) {
                        return visual.meshPath.find("Data/Upgrade/wheel") !=
                               std::string::npos;
                    });
        }
        for (const auto& weapon : race.weapons)
        {
            for (std::size_t index = 0U;
                 index < weapon.projectiles.size(); ++index)
            {
                if (weapon.projectiles[index].weaponListIndex != index)
                {
                    error =
                        "Weapon::Desc projectile list identity mismatch";
                    return false;
                }
            }
        }
        std::size_t fixedPlaneCount = 0U;
        std::size_t billboardCount = 0U;
        bool invalidBillboard = false;
        std::size_t inheritedVelocityEmitterCount = 0U;
        bool sawFire2Emitter = false;
        bool invalidParticleBehavior = false;
        const auto auditVisual = [&](const VisualNode& visual) {
            invalidBillboard =
                invalidBillboard || (visual.billboard && !visual.plane) ||
                (visual.fixedDirection && !visual.billboard);
            if (visual.billboard)
                ++billboardCount;
            else if (visual.plane)
                ++fixedPlaneCount;
        };
        const auto auditDefinition = [&](const ObjectDefinition& definition) {
            for (const auto& visual : definition.visualNodes)
                auditVisual(visual);
            for (const auto& emitter : definition.particleEmitters)
            {
                const bool fire2 = emitter.sourceRecord == "fire2";
                sawFire2Emitter = sawFire2Emitter || fire2;
                if (emitter.sourceSpeedBehavior)
                    ++inheritedVelocityEmitterCount;
                invalidParticleBehavior = invalidParticleBehavior ||
                    (emitter.sourceSpeedBehavior != fire2) ||
                    (emitter.sourceSpeedBehavior &&
                     !emitter.waitForParticleEnd);
            }
            for (const auto& piece : definition.destructionPieces)
            {
                for (const auto& visual : piece.visualNodes)
                    auditVisual(visual);
            }
        };
        for (const auto& definition : race.trackDefinitions)
            auditDefinition(definition);
        for (const auto& definition : race.decorationDefinitions)
            auditDefinition(definition);
        for (const auto& bonus : race.bonuses)
        {
            auditDefinition(bonus.visual);
            auditDefinition(bonus.deathEffect.visual);
        }
        for (const auto& weapon : race.weapons)
        {
            auditVisual(weapon.visual);
            auditDefinition(weapon.shotEffect.visual);
            for (const auto& projectile : weapon.projectiles)
            {
                auditDefinition(projectile.visual);
                auditDefinition(projectile.secondaryVisual);
                auditDefinition(projectile.tertiaryVisual);
                auditDefinition(projectile.deathEffect.visual);
            }
        }
        for (const auto& visual : race.vehicle.bodyVisuals)
            auditVisual(visual);
        for (const auto& visual : race.vehicle.wheelVisuals)
            auditVisual(visual);
        auditDefinition(race.vehicle.lowLifeEffect);
        auditDefinition(race.vehicle.shieldEffect);
        for (const auto& effect : race.vehicle.deathEffects)
            auditDefinition(effect.visual);
        auditDefinition(race.rainEffect);
        auditDefinition(race.wheelTrailEffect);
        auditDefinition(race.wheelSmokeEffect);
        auditDefinition(race.contactEffect);
        if (!sawFire2Emitter || inheritedVelocityEmitterCount == 0U ||
            invalidParticleBehavior)
        {
            error =
                "source FxSystemSrcSpeed/FxSystemWaitingEnd provenance "
                "mismatch";
            return false;
        }
        if (invalidBillboard || fixedPlaneCount == 0U ||
            billboardCount == 0U)
        {
            error = "source ntPlane/ntSprite provenance mismatch: planes=" +
                    std::to_string(fixedPlaneCount) +
                    ", billboards=" + std::to_string(billboardCount) +
                    ", invalid=" +
                    (invalidBillboard ? "true" : "false");
            return false;
        }
        std::size_t triangleCount = 0;
        std::size_t borderMeshCount = 0;
        std::size_t ownedDecorationMeshCount = 0;
        bool hasUnownedDecorationMesh = false;
        for (const auto& mesh : physics.collisionMeshes)
        {
            triangleCount += mesh.indices.size() / 3U;
            if (mesh.surface ==
                r3d::physics::CollisionSurface::TrackBorder)
                ++borderMeshCount;
            if (mesh.surface ==
                r3d::physics::CollisionSurface::Decoration)
            {
                if (mesh.decorationInstance < physics.decorations.size())
                    ++ownedDecorationMeshCount;
                else
                    hasUnownedDecorationMesh = true;
            }
        }
        const auto dynamicDecorationBodyCount = static_cast<std::size_t>(
            std::count_if(
                physics.decorations.begin(), physics.decorations.end(),
                [](const r3d::physics::DecorationDescription& decoration) {
                    return decoration.hasBodyShape && decoration.dynamic;
                }));
        const auto near = [](float first, float second) {
            return std::abs(first - second) <= 0.0001F;
        };
        const auto materialCatalog =
            loadOriginalMaterialCatalog(resources);
        std::unordered_set<std::string> materialNames;
        for (const auto& material : materialCatalog)
            materialNames.insert(material.record);
        if (materialCatalog.size() != 257U ||
            materialNames.size() != materialCatalog.size())
        {
            error = "source ComplexMatLib catalog mismatch: " +
                    std::to_string(materialCatalog.size()) + "/" +
                    std::to_string(materialNames.size());
            return false;
        }
        const auto gunFlashMaterial =
            materialDefinition(resources, "Effect\\gunEff2");
        const auto speedArrowMaterial =
            materialDefinition(resources, "Bonus\\speedArrow");
        const auto shieldBonusMaterial =
            materialDefinition(resources, "Bonus\\shield");
        const auto garageSpaceMaterial =
            materialDefinition(resources, "GUI\\space2");
        const auto carBlendMaterial =
            materialDefinition(resources, "Car\\blend");
        const bool sourceMaterialLibraryMatches =
            gunFlashMaterial.blend == MaterialBlend::Additive &&
            gunFlashMaterial.atlasColumns == 4U &&
            gunFlashMaterial.atlasRows == 1U &&
            near(gunFlashMaterial.textureCoordinateMaximum.y, 0.25F) &&
            near(gunFlashMaterial.textureCoordinateInset.x,
                 0.5F / 256.0F) &&
            near(gunFlashMaterial.textureCoordinateInset.y,
                 0.5F / 256.0F) &&
            speedArrowMaterial.emissive > 0.999F &&
            speedArrowMaterial.ignoreFog &&
            !speedArrowMaterial.writeDepth &&
            shieldBonusMaterial.emissive < 0.001F &&
            !shieldBonusMaterial.ignoreFog &&
            shieldBonusMaterial.writeDepth &&
            garageSpaceMaterial.emissive > 0.999F &&
            garageSpaceMaterial.ignoreFog &&
            garageSpaceMaterial.writeDepth &&
            carBlendMaterial.blend == MaterialBlend::Additive &&
            near(carBlendMaterial.alphaMinimum, 0.7F) &&
            near(carBlendMaterial.alphaMaximum, 0.7F) &&
            carBlendMaterial.emissive < 0.001F &&
            !carBlendMaterial.ignoreFog;
        if (!sourceMaterialLibraryMatches)
        {
            error =
                "source ComplexMatLib sprite/atlas provenance mismatch";
            return false;
        }
        const auto definitionNamed = [&](std::string_view name) {
            const auto found = std::find_if(
                race.decorationDefinitions.begin(),
                race.decorationDefinitions.end(),
                [name](const ObjectDefinition& definition) {
                    return definition.record.size() >= name.size() &&
                           definition.record.compare(
                               definition.record.size() - name.size(),
                               name.size(), name) == 0;
                });
            return found == race.decorationDefinitions.end()
                       ? static_cast<const ObjectDefinition*>(nullptr)
                       : &*found;
        };
        const auto* crush1 = definitionNamed("crush1");
        const auto* reklama = definitionNamed("reklama");
        const auto* bochka = definitionNamed("bochka");
        const auto crush1Instance = std::find_if(
            race.decorationInstances.begin(),
            race.decorationInstances.end(),
            [&](const ObjectInstance& instance) {
                return instance.definition <
                           race.decorationDefinitions.size() &&
                       &race.decorationDefinitions[instance.definition] ==
                           crush1;
            });
        bool destructionBodiesMatch = false;
        bool intactDestructionBodiesMatch = false;
        std::size_t destructionBodyCount = 0U;
        std::size_t destructionDynamicCount = 0U;
        if (crush1Instance != race.decorationInstances.end())
        {
            const auto instanceIndex = static_cast<std::size_t>(
                crush1Instance - race.decorationInstances.begin());
            intactDestructionBodiesMatch =
                instanceIndex < physics.decorations.size() &&
                !physics.decorations[instanceIndex].collisionResponse &&
                physics.decorations[instanceIndex].childShapes.size() == 12U;
            const auto bodies = makeDecorationDestruction(
                race, resources, instanceIndex);
            destructionBodyCount = bodies.size();
            destructionDynamicCount = static_cast<std::size_t>(
                std::count_if(
                    bodies.begin(), bodies.end(),
                    [](const DecorationDebrisDefinition& body) {
                        return body.physics.dynamic;
                    }));
            destructionBodiesMatch =
                bodies.size() == 14U && destructionDynamicCount == 12U &&
                std::all_of(
                    bodies.begin(), bodies.end(),
                    [&](const DecorationDebrisDefinition& body) {
                        if (body.piece >= crush1->destructionPieces.size())
                            return false;
                        const auto& source =
                            crush1->destructionPieces[body.piece];
                        const auto& physicsBody = body.physics;
                        const bool parentPose =
                            near(physicsBody.transform.position.x,
                                 crush1Instance->transform.position.x) &&
                            near(physicsBody.transform.position.y,
                                 crush1Instance->transform.position.y) &&
                            near(physicsBody.transform.position.z,
                                 crush1Instance->transform.position.z) &&
                            near(physicsBody.transform.rotation.x,
                                 crush1Instance->transform.rotation.x) &&
                            near(physicsBody.transform.rotation.y,
                                 crush1Instance->transform.rotation.y) &&
                            near(physicsBody.transform.rotation.z,
                                 crush1Instance->transform.rotation.z) &&
                            near(physicsBody.transform.rotation.w,
                                 crush1Instance->transform.rotation.w);
                        const bool sourceLifetimeAndImpulse =
                            physicsBody.lifetime < 0.0F &&
                            near(physicsBody.localImpulse.x, 0.0F) &&
                            near(physicsBody.localImpulse.y, 0.0F) &&
                            near(physicsBody.localImpulse.z, 0.0F);
                        return parentPose && sourceLifetimeAndImpulse &&
                               physicsBody.dynamic == source.dynamic &&
                               (source.dynamic
                                    ? physicsBody.collisionMeshes.empty() &&
                                          near(physicsBody.mass, 200.0F)
                                    : !physicsBody.collisionMeshes.empty());
                    });
        }
        const bool hasDestructibleWithoutSourcePieces =
            std::any_of(
                race.decorationDefinitions.begin(),
                race.decorationDefinitions.end(),
                [](const ObjectDefinition& definition) {
                    return definition.destructible &&
                           definition.destructionPieces.empty();
                });
        const bool hasDestructibleWithoutSourceCollision =
            std::any_of(
                race.decorationDefinitions.begin(),
                race.decorationDefinitions.end(),
                [](const ObjectDefinition& definition) {
                    return definition.destructible &&
                           definition.collisionShapes.empty() &&
                           (definition.bodyHalfExtents.x <= 0.0F ||
                            definition.bodyHalfExtents.y <= 0.0F ||
                            definition.bodyHalfExtents.z <= 0.0F);
                });
        const auto sourcePiecesMatch = [&](const ObjectDefinition* definition,
                                           std::size_t pieces,
                                           std::size_t dynamicPieces) {
            if (definition == nullptr || !definition->destructible ||
                definition->destructionPieces.size() != pieces)
                return false;
            std::size_t dynamicCount = 0;
            for (const auto& piece : definition->destructionPieces)
            {
                if (piece.visualNodes.empty())
                    return false;
                if (!piece.dynamic)
                {
                    if (piece.collisionShapes.empty())
                        return false;
                    continue;
                }
                ++dynamicCount;
                if (!near(piece.mass, 200.0F) ||
                    piece.halfExtents.x <= 0.0F ||
                    piece.halfExtents.y <= 0.0F ||
                    piece.halfExtents.z <= 0.0F)
                    return false;
            }
            return dynamicCount == dynamicPieces;
        };
        if (!sourcePiecesMatch(crush1, 14U, 12U) ||
            !sourcePiecesMatch(reklama, 11U, 10U) ||
            bochka == nullptr || bochka->destructible ||
            !bochka->destructionPieces.empty() ||
            !intactDestructionBodiesMatch ||
            !destructionBodiesMatch ||
            hasDestructibleWithoutSourcePieces ||
            hasDestructibleWithoutSourceCollision ||
            race.collisionMeshes.size() !=
                race.collisionMeshDecorationInstances.size() ||
            physics.decorations.size() !=
                race.decorationInstances.size() ||
            ownedDecorationMeshCount == 0U ||
            hasUnownedDecorationMesh ||
            dynamicDecorationBodyCount != 8U)
        {
            const auto audit = [](const ObjectDefinition* definition) {
                if (definition == nullptr)
                    return std::string("missing");
                const auto dynamicCount = std::count_if(
                    definition->destructionPieces.begin(),
                    definition->destructionPieces.end(),
                    [](const DestructionPieceDefinition& piece) {
                        return piece.dynamic;
                    });
                const auto emptyVisualCount = std::count_if(
                    definition->destructionPieces.begin(),
                    definition->destructionPieces.end(),
                    [](const DestructionPieceDefinition& piece) {
                        return piece.visualNodes.empty();
                    });
                return std::to_string(
                           definition->destructionPieces.size()) +
                       " pieces/" + std::to_string(dynamicCount) +
                       " dynamic/" + std::to_string(emptyVisualCount) +
                       " empty visuals/destructible=" +
                       (definition->destructible ? "true" : "false");
            };
            error = "source gotDestrObj provenance mismatch: crush1=" +
                    audit(crush1) + ", reklama=" + audit(reklama) +
                    ", bochka=" + audit(bochka) +
                    ", ownedMeshes=" +
                    std::to_string(ownedDecorationMeshCount) +
                    ", detachedBodies=" +
                    std::to_string(destructionBodyCount) + "/" +
                    std::to_string(destructionDynamicCount) + " dynamic" +
                    ", dynamicBodies=" +
                    std::to_string(dynamicDecorationBodyCount);
            return false;
        }
        const auto recordEndsWith = [](std::string_view record,
                                       std::string_view name) {
            return record.size() >= name.size() &&
                   record.compare(record.size() - name.size(),
                                  name.size(), name) == 0;
        };
        const auto weaponNamed = [&](std::string_view name) {
            const auto found = std::find_if(
                race.weapons.begin(), race.weapons.end(),
                [&](const WeaponDefinition& weapon) {
                    return recordEndsWith(weapon.record, name);
                });
            return found == race.weapons.end()
                       ? static_cast<const WeaponDefinition*>(nullptr)
                       : &*found;
        };
        const auto* bulletGun = weaponNamed("bulletGun");
        const auto* rifleWeapon = weaponNamed("rifleWeapon");
        const auto* rocketLauncher = weaponNamed("rocketLauncher");
        const auto* sphereGun = weaponNamed("sphereGun");
        const auto* turel = weaponNamed("turel");
        const auto* drobilka = weaponNamed("drobilka");
        const auto* tankLaser = weaponNamed("tankLaser");
        const auto* mortar = weaponNamed("mortira");
        const bool bulletShotMatchesSource =
            bulletGun != nullptr &&
            bulletGun->chargeCost == 2750 &&
            !bulletGun->projectiles.empty() &&
            near(bulletGun->projectiles.front().damage, 6.0F) &&
            recordEndsWith(
                bulletGun->shotEffect.visual.record, "shotEff1") &&
            bulletGun->shotEffect.soundPaths.size() == 1U &&
            recordEndsWith(
                bulletGun->shotEffect.soundPaths.front(),
                "phalanx_shot_a.ogg") &&
            near(bulletGun->shotEffect.duration, 0.15F);
        const bool sphereSoundMatchesSource =
            sphereGun != nullptr &&
            sphereGun->shotEffect.visual.record.empty() &&
            sphereGun->shotEffect.soundPaths.size() == 1U &&
            recordEndsWith(
                sphereGun->shotEffect.soundPaths.front(),
                "gun_podushkat.ogg");
        const bool turelShotMatchesSource =
            turel != nullptr &&
            recordEndsWith(
                turel->shotEffect.visual.record, "powerShot1") &&
            turel->shotEffect.soundPaths.size() == 1U &&
            recordEndsWith(
                turel->shotEffect.soundPaths.front(), "turel.ogg") &&
            near(turel->shotEffect.position.x, 1.4F) &&
            near(turel->shotEffect.position.z, 0.1F) &&
            near(turel->shotEffect.duration, 1.0F);
        const bool silentWeaponMatchesSource =
            drobilka != nullptr &&
            drobilka->shotEffect.visual.record.empty() &&
            drobilka->shotEffect.soundPaths.empty();
        const bool laserRayMatchesSource =
            tankLaser != nullptr &&
            tankLaser->projectiles.size() == 1U &&
            tankLaser->projectiles.front().type == 3U &&
            near(
                tankLaser->projectiles.front().sizeAddPx.z,
                -0.3F);
        const bool anonymousIncludeMatchesSource =
            rifleWeapon != nullptr &&
            rifleWeapon->chargeCost == 9000 &&
            rifleWeapon->projectiles.size() >= 2U &&
            near(rifleWeapon->projectiles[0].damage +
                     rifleWeapon->projectiles[1].damage,
                 12.0F) &&
            !rifleWeapon->projectiles.empty() &&
            std::any_of(
                rifleWeapon->projectiles.front()
                    .visual.particleEmitters.begin(),
                rifleWeapon->projectiles.front()
                    .visual.particleEmitters.end(),
                [&](const ParticleEmitterDefinition& emitter) {
                    return emitter.sourceRecord == "obj0" &&
                           emitter.waitForParticleEnd &&
                           emitter.distanceTriggered &&
                           !emitter.worldCoordinates &&
                           near(emitter.transform.position.x, -0.4F) &&
                           near(emitter.lifeMinimum, 0.5F) &&
                           near(emitter.startTimeMinimum, 0.25F) &&
                           near(emitter.velocityMinimum.x, -5.0F) &&
                           !emitter.materials.empty() &&
                           recordEndsWith(
                               emitter.materials.front().texturePath,
                               "flare1.dds");
                });
        const bool inlineBehaviorMatchesSource =
            rocketLauncher != nullptr &&
            !rocketLauncher->projectiles.empty() &&
            std::any_of(
                rocketLauncher->projectiles.front()
                    .visual.particleEmitters.begin(),
                rocketLauncher->projectiles.front()
                    .visual.particleEmitters.end(),
                [](const ParticleEmitterDefinition& emitter) {
                    return emitter.sourceRecord == "smoke2" &&
                           emitter.waitForParticleEnd;
                });
        const bool deathEffectFlagsMatchSource =
            bulletGun != nullptr &&
            !bulletGun->projectiles.empty() &&
            bulletGun->projectiles.front().deathEffect.targetChild &&
            !bulletGun->projectiles.front().deathEffect
                 .effectPhysicsIgnoreSenderCar &&
            mortar != nullptr && !mortar->projectiles.empty() &&
            !mortar->projectiles.front().deathEffect.targetChild &&
            mortar->projectiles.front().deathEffect.ignoreRotation &&
            mortar->projectiles.front().deathEffect
                .effectPhysicsIgnoreSenderCar;
        const bool nestedLifeSoundMatchesSource =
            mortar != nullptr && !mortar->projectiles.empty() &&
            std::any_of(
                mortar->projectiles.front()
                    .deathEffect.visual.soundPaths.begin(),
                mortar->projectiles.front()
                    .deathEffect.visual.soundPaths.end(),
                [&](const std::string& path) {
                    return recordEndsWith(path, "carcrash05.ogg");
                });
        if (!bulletShotMatchesSource || !sphereSoundMatchesSource ||
            !turelShotMatchesSource || !silentWeaponMatchesSource ||
            !laserRayMatchesSource || !anonymousIncludeMatchesSource ||
            !inlineBehaviorMatchesSource ||
            !deathEffectFlagsMatchSource ||
            !nestedLifeSoundMatchesSource)
        {
            error =
                "source ctWeapon/include behaviors/ShotEffect/DeathEffect/"
                "sounds provenance mismatch: anonymous=" +
                std::string(anonymousIncludeMatchesSource ? "true" :
                                                           "false") +
                ", inlineBehavior=" +
                std::string(inlineBehaviorMatchesSource ? "true" :
                                                         "false") +
                ", nestedLifeSound=" +
                std::string(nestedLifeSoundMatchesSource ? "true" :
                                                          "false");
            return false;
        }
        if (!recordEndsWith(
                race.vehicle.lowLifeEffect.record, "smoke6") ||
            !near(race.vehicle.lowLifeLevel, 0.35F) ||
            !near(race.vehicle.lowLifeEffectPosition.z, 0.5F) ||
            race.vehicle.lowLifeEffect.particleEmitters.size() != 1U ||
            // goOpacity is mapped to the source runtime goEffect queue by
            // the original mismatched enum string table.
            race.vehicle.lowLifeEffect.graphOrder != GraphOrder::Effect ||
            race.vehicle.lowLifeEffect.particleEmitters.front()
                    .materials.empty() ||
            !recordEndsWith(
                race.vehicle.lowLifeEffect.particleEmitters.front()
                    .materials.front().texturePath,
                "smoke6.dds") ||
            !near(race.vehicle.lowLifeEffect.particleEmitters.front()
                      .materials.front().alphaMinimum,
                  1.0F) ||
            !near(race.vehicle.lowLifeEffect.particleEmitters.front()
                      .materials.front().alphaMaximum,
                  0.1F) ||
            !near(race.vehicle.lowLifeEffect.particleEmitters.front()
                      .materials.front().colorMaximum[0],
                  0.0F))
        {
            const auto& effect = race.vehicle.lowLifeEffect;
            const std::string texture =
                effect.particleEmitters.empty() ||
                        effect.particleEmitters.front().materials.empty()
                    ? "<none>"
                    : effect.particleEmitters.front()
                          .materials.front().texturePath;
            error = "source LowLifePoints/smoke6 provenance mismatch: " +
                    effect.record + ", emitters=" +
                    std::to_string(effect.particleEmitters.size()) +
                    ", graphOrder=" +
                    std::to_string(static_cast<int>(effect.graphOrder)) +
                    ", texture=" + texture;
            return false;
        }
        const auto& shield = race.vehicle.shieldEffect;
        static constexpr std::array<float, 3> shieldScales{
            1.0F, 0.975F, 0.95F};
        static constexpr std::array<float, 3> shieldDurations{
            5.5F, 6.0F, 6.5F};
        static constexpr std::array<std::string_view, 3>
            shieldMaterials{
                "shield2", "shield2Hor", "shield2Vert"};
        bool shieldNodesMatchSource =
            recordEndsWith(shield.record, "shield1") &&
            shield.graphOrder == GraphOrder::Effect &&
            shield.visualNodes.size() == shieldScales.size() &&
            near(race.vehicle.shieldEffectScale.x, 1.3F) &&
            near(race.vehicle.shieldEffectScale.y, 1.7F) &&
            near(race.vehicle.shieldEffectScale.z, 1.7F);
        for (std::size_t index = 0;
             index < shield.visualNodes.size() &&
             shieldNodesMatchSource; ++index)
        {
            const auto& node = shield.visualNodes[index];
            shieldNodesMatchSource =
                recordEndsWith(node.meshPath, "sphere.r3d") &&
                near(node.transform.scale.x, shieldScales[index]) &&
                near(node.transform.scale.y, shieldScales[index]) &&
                near(node.transform.scale.z, shieldScales[index]) &&
                node.animationMode ==
                    VisualNode::AnimationMode::Repeat &&
                near(node.animationDuration, shieldDurations[index]) &&
                node.materials.size() == 1U &&
                recordEndsWith(
                    node.materials.front().record,
                    shieldMaterials[index]) &&
                recordEndsWith(
                    node.materials.front().texturePath,
                    "shield2.dds") &&
                node.materials.front().blend ==
                    MaterialBlend::Additive &&
                near(node.materials.front().alphaMinimum, 0.4F) &&
                near(node.materials.front().alphaMaximum, 0.4F);
            if (!shieldNodesMatchSource)
                break;
            const auto& maximum =
                node.materials.front().textureOffsetMaximum;
            const Vec3 wanted =
                index == 0U
                    ? Vec3{1.0F, 1.0F, 0.0F}
                    : (index == 1U
                           ? Vec3{1.0F, 0.0F, 0.0F}
                           : Vec3{0.0F, 1.0F, 0.0F});
            shieldNodesMatchSource =
                near(maximum.x, wanted.x) &&
                near(maximum.y, wanted.y) &&
                near(maximum.z, wanted.z);
        }
        if (!shieldNodesMatchSource)
        {
            error =
                "source ImmortalEffect/shield1 provenance mismatch: " +
                shield.record + ", nodes=" +
                std::to_string(shield.visualNodes.size()) +
                ", graphOrder=" +
                std::to_string(static_cast<int>(shield.graphOrder));
            return false;
        }
        const auto& energyDamage = race.vehicle.energyDamageEffect;
        const bool energyDamageMatchesSource =
            recordEndsWith(
                energyDamage.record, "damageEnergymarauder") &&
            near(energyDamage.maximumTimeLife, 0.5F) &&
            energyDamage.graphOrder == GraphOrder::Effect &&
            energyDamage.visualNodes.size() == 1U &&
            recordEndsWith(
                energyDamage.visualNodes.front().meshPath,
                "marauder.r3d") &&
            near(energyDamage.visualNodes.front().transform.scale.x,
                 1.1F) &&
            energyDamage.visualNodes.front().animationMode ==
                VisualNode::AnimationMode::TwoSide &&
            near(energyDamage.visualNodes.front().animationDuration,
                 0.5F) &&
            energyDamage.visualNodes.front().materials.size() == 1U &&
            recordEndsWith(
                energyDamage.visualNodes.front()
                    .materials.front().record,
                "shield1");
        if (!energyDamageMatchesSource)
        {
            error =
                "source dtEnergy DamageEffect/damageEnergymarauder "
                "provenance mismatch: " +
                energyDamage.record + ", nodes=" +
                std::to_string(energyDamage.visualNodes.size());
            return false;
        }
        const auto& sourceDeathVisual =
            race.vehicle.deathEffects.front().visual;
        const auto particleNodeIndex =
            [&](std::string_view meshName) {
                const auto found = std::find_if(
                    sourceDeathVisual.particleEmitters.begin(),
                    sourceDeathVisual.particleEmitters.end(),
                    [&](const ParticleEmitterDefinition& emitter) {
                        return emitter.renderMode ==
                                   ParticleRenderMode::Node &&
                               emitter.nodeVisuals.size() == 1U &&
                               recordEndsWith(
                                   emitter.nodeVisuals.front().meshPath,
                                   meshName);
                    });
                return found ==
                               sourceDeathVisual.particleEmitters.end()
                           ? -1
                           : static_cast<int>(std::distance(
                                 sourceDeathVisual.particleEmitters.begin(),
                                 found));
            };
        const int piecesEmitter = particleNodeIndex("pieces1.r3d");
        const int wheelEmitter = particleNodeIndex("wheel.r3d");
        const int trubaEmitter = particleNodeIndex("truba.r3d");
        const auto volumeNodeRangesMatchSource =
            [&](int index) {
                if (index < 0)
                    return false;
                const auto& emitter =
                    sourceDeathVisual.particleEmitters[
                        static_cast<std::size_t>(index)];
                return emitter.startPositionDistribution ==
                           ParticleDistribution::Volume &&
                       emitter.velocityDistribution ==
                           ParticleDistribution::Volume &&
                       emitter.rotationVelocityDistribution ==
                           ParticleDistribution::Volume &&
                       emitter.startPositionFrequency ==
                           std::array<std::uint32_t, 3>{100U, 100U,
                                                        100U} &&
                       emitter.velocityFrequency ==
                           std::array<std::uint32_t, 3>{100U, 100U,
                                                        100U} &&
                       emitter.rotationVelocityFrequency ==
                           std::array<std::uint32_t, 2>{100U, 100U};
            };
        const auto nestedPointEmitterCount =
            static_cast<std::size_t>(std::count_if(
                sourceDeathVisual.particleEmitters.begin(),
                sourceDeathVisual.particleEmitters.end(),
                [&](const ParticleEmitterDefinition& emitter) {
                    return emitter.renderMode ==
                               ParticleRenderMode::PointSprite &&
                           emitter.parentEmitter == piecesEmitter &&
                           emitter.sourceRecord == "death2";
                }));
        const auto refractionNode = std::find_if(
            sourceDeathVisual.visualNodes.begin(),
            sourceDeathVisual.visualNodes.end(),
            [&](const VisualNode& node) {
                return node.materials.size() == 1U &&
                       recordEndsWith(
                           node.materials.front().record, "j_swell");
            });
        const bool refractionNodeMatchesSource =
            refractionNode != sourceDeathVisual.visualNodes.end() &&
            refractionNode->overridesLighting &&
            refractionNode->lighting == LightingMode::Refraction &&
            refractionNode->overridesGraphOrder &&
            refractionNode->graphOrder == GraphOrder::Default &&
            refractionNode->animationMode ==
                VisualNode::AnimationMode::Once &&
            near(refractionNode->animationDuration, 0.5F) &&
            near(refractionNode->maximumTimeLife, 0.5F) &&
            near(refractionNode->speedScale.x, 100.0F) &&
            near(refractionNode->speedScale.y, 100.0F) &&
            near(refractionNode->speedScale.z, 100.0F);
        if (race.vehicle.deathEffects.size() != 2U ||
            !recordEndsWith(
                race.vehicle.deathEffects[0].visual.record, "death2") ||
            !race.vehicle.deathEffects[0].ignoreRotation ||
            race.vehicle.deathEffects[0].visual.maximumTimeLife != 10.0F ||
            race.vehicle.deathEffects[0].visual.particleEmitters.empty() ||
            piecesEmitter < 0 || wheelEmitter < 0 || trubaEmitter < 0 ||
            !volumeNodeRangesMatchSource(wheelEmitter) ||
            !volumeNodeRangesMatchSource(trubaEmitter) ||
            nestedPointEmitterCount != 2U ||
            !refractionNodeMatchesSource ||
            race.vehicle.deathEffects[0].visual.soundPaths.empty() ||
            !recordEndsWith(
                race.vehicle.deathEffects[0].visual.soundPaths.front(),
                "carcrash05.ogg") ||
            !recordEndsWith(
                race.vehicle.deathEffects[1].visual.record,
                "marauderCrush") ||
            race.vehicle.deathEffects[1].ignoreRotation ||
            !near(race.vehicle.deathEffects[1].impulse.x, 20000.0F) ||
            !race.vehicle.deathEffects[1].visual.dynamicBody ||
            !near(race.vehicle.deathEffects[1].visual.bodyMass, 1200.0F) ||
            !near(
                race.vehicle.deathEffects[1].visual.bodyHalfExtents.x,
                1.46261F) ||
            !near(
                race.vehicle.deathEffects[1].visual.bodyHalfExtents.y,
                0.745738F) ||
            !near(
                race.vehicle.deathEffects[1].visual.bodyHalfExtents.z,
                0.534895F) ||
            race.vehicle.deathEffects[1].visual.maximumTimeLife != 10.0F ||
            race.vehicle.deathEffects[1].visual.visualNodes.empty() ||
            race.vehicle.deathEffects[1].visual.particleEmitters.size() < 2U)
        {
            error =
                "source vehicle DeathEffect/death2 FxNode/child graph/"
                "marauderCrush provenance mismatch: node emitters=" +
                std::to_string(piecesEmitter >= 0 ? 1 : 0) + "/" +
                std::to_string(wheelEmitter >= 0 ? 1 : 0) + "/" +
                std::to_string(trubaEmitter >= 0 ? 1 : 0) +
                ", nested point emitters=" +
                std::to_string(nestedPointEmitterCount) +
                ", refr1=" +
                (refractionNodeMatchesSource ? "1" : "0") +
                ", volume ranges=" +
                (volumeNodeRangesMatchSource(wheelEmitter) ? "1" : "0") +
                "/" +
                (volumeNodeRangesMatchSource(trubaEmitter) ? "1" : "0");
            return false;
        }
        const auto expandingExplosion = std::find_if(
            race.vehicle.deathEffects[0].visual.visualNodes.begin(),
            race.vehicle.deathEffects[0].visual.visualNodes.end(),
            [&](const VisualNode& node) {
                return node.materials.size() == 1U &&
                       recordEndsWith(
                           node.materials.front().record,
                           "explosion2");
            });
        if (expandingExplosion ==
                race.vehicle.deathEffects[0].visual.visualNodes.end() ||
            expandingExplosion->animationMode !=
                VisualNode::AnimationMode::Once ||
            !near(expandingExplosion->animationDuration, 1.0F) ||
            !near(expandingExplosion->speedScale.x, 25.0F) ||
            !near(expandingExplosion->speedScale.y, 25.0F) ||
            !near(expandingExplosion->speedScale.z, 25.0F))
        {
            error =
                "source BaseSceneNode speedScale/explosion2 "
                "provenance mismatch";
            return false;
        }
        std::size_t alphaTestMaterialCount = 0;
        bool alphaTestThresholdMatchesSource = true;
        const auto auditAlphaTestMaterials =
            [&](const std::vector<ObjectDefinition>& definitions) {
                for (const auto& definition : definitions)
                {
                    for (const auto& node : definition.visualNodes)
                    {
                        for (const auto& material : node.materials)
                        {
                            if (material.blend != MaterialBlend::AlphaTest)
                                continue;
                            ++alphaTestMaterialCount;
                            alphaTestThresholdMatchesSource &=
                                near(material.alphaReference, 1.0F - 0.933F);
                        }
                    }
                }
            };
        auditAlphaTestMaterials(race.trackDefinitions);
        auditAlphaTestMaterials(race.decorationDefinitions);
        const auto cullOpacityDefinitionCount =
            std::count_if(
                race.trackDefinitions.begin(),
                race.trackDefinitions.end(),
                [](const ObjectDefinition& definition) {
                    return definition.cullOpacity;
                }) +
            std::count_if(
                race.decorationDefinitions.begin(),
                race.decorationDefinitions.end(),
                [](const ObjectDefinition& definition) {
                    return definition.cullOpacity;
                });
        if (race.vehicle.bodyVisuals.empty() ||
            race.vehicle.bodyVisuals.front().materials.empty())
        {
            error = "marauder body material is missing";
            return false;
        }
        if (!near(
                race.vehicle.bodyVisuals.front()
                    .materials.front().specular,
                1.0F))
        {
            error = "marauder body source specular mismatch";
            return false;
        }
        if (race.vehicle.wheelVisuals.empty() ||
            race.vehicle.wheelVisuals.front().materials.empty())
        {
            error = "marauder wheel material is missing";
            return false;
        }
        if (!near(
                race.vehicle.wheelVisuals.front()
                    .materials.front().specular,
                0.0F))
        {
            error = "marauder wheel source specular mismatch";
            return false;
        }
        const bool bonusCollisionBoxesMatchSource = std::all_of(
            race.bonuses.begin(), race.bonuses.end(),
            [](const BonusInstance& bonus) {
                return bonus.collision.halfExtents.x > 0.0F &&
                       bonus.collision.halfExtents.y > 0.0F &&
                       bonus.collision.halfExtents.z > 0.0F;
            });
        const auto mineSpike = std::find_if(
            race.bonuses.begin(), race.bonuses.end(),
            [](const BonusInstance& bonus) {
                return bonus.record.size() >= 9U &&
                       bonus.record.compare(
                           bonus.record.size() - 9U, 9U,
                           "mineSpike") == 0;
            });
        // World1/map1 does not place an oil hazard. Validate the material
        // library record itself so the source-only Bonus::maslo path remains
        // covered independently of the selected map's ctBonus instances.
        const auto sourceOilMaterial =
            materialDefinition(resources, "Bonus\\maslo");
        const auto* oilMaterial = &sourceOilMaterial;
        if (!bonusCollisionBoxesMatchSource ||
            mineSpike == race.bonuses.end() ||
            !mineSpike->modelSize ||
            !near(mineSpike->size.x, 0.0F) ||
            !near(mineSpike->size.y, 0.0F) ||
            !near(mineSpike->size.z, 1.7F) ||
            mineSpike->collision.halfExtents.x >= 3.5F ||
            mineSpike->collision.halfExtents.y >= 3.5F)
        {
            error =
                "source Proj::ComputeAABB/CreatePxBox bonus collision "
                "provenance mismatch";
            return false;
        }
        if (oilMaterial == nullptr ||
            oilMaterial->record != "Bonus\\maslo" ||
            oilMaterial->blend != MaterialBlend::Transparency ||
            oilMaterial->writeDepth ||
            !near(oilMaterial->specular, 1.0F) ||
            !near(oilMaterial->shininess, 64.0F) ||
            !oilMaterial->reflectionTextureCoordinates ||
            !recordEndsWith(
                oilMaterial->reflectionTexturePath,
                "maslo_top.dds"))
        {
            error =
                "source Bonus\\\\maslo two-stage reflection material "
                "provenance mismatch: record=" +
                (oilMaterial != nullptr ? oilMaterial->record : "<missing>") +
                ", blend=" +
                std::to_string(
                    oilMaterial != nullptr
                        ? static_cast<int>(oilMaterial->blend)
                        : -1) +
                ", writeDepth=" +
                std::to_string(
                    oilMaterial != nullptr && oilMaterial->writeDepth) +
                ", specular=" +
                std::to_string(
                    oilMaterial != nullptr ? oilMaterial->specular : -1.0F) +
                ", shininess=" +
                std::to_string(
                    oilMaterial != nullptr ? oilMaterial->shininess : -1.0F) +
                ", reflectionCoordinates=" +
                std::to_string(
                    oilMaterial != nullptr &&
                    oilMaterial->reflectionTextureCoordinates) +
                ", reflection=" +
                (oilMaterial != nullptr
                     ? oilMaterial->reflectionTexturePath
                     : std::string{"<missing>"});
            return false;
        }
        const bool contactEffectMatchesSource =
            recordEndsWith(race.contactEffect.record, "spark2") &&
            race.contactEffect.maximumTimeLife < 0.0F &&
            race.contactEffect.graphOrder == GraphOrder::Effect &&
            race.contactEffect.particleEmitters.size() == 1U &&
            race.contactSoundPaths.size() == 5U;
        if (contactEffectMatchesSource)
        {
            const auto& emitter =
                race.contactEffect.particleEmitters.front();
            if (emitter.maximumParticles != 0U ||
                !near(emitter.lifeMinimum, 0.3F) ||
                !near(emitter.lifeMaximum, 0.7F) ||
                !near(emitter.startTimeMinimum, 0.1F) ||
                !near(emitter.startTimeMaximum, 0.1F) ||
                !near(emitter.densityMinimum, 7.0F) ||
                !near(emitter.densityMaximum, 10.0F) ||
                !near(emitter.gravity.z, -2.0F) ||
                emitter.materials.size() != 1U ||
                !recordEndsWith(
                    emitter.materials.front().record,
                    "Effect\\spark1"))
            {
                error =
                    "source PairPxContactEffect/spark2 emitter provenance "
                    "mismatch";
                return false;
            }
        }
        if (!contactEffectMatchesSource)
        {
            error =
                "source DataBase::Init PairPxContactEffect provenance "
                "mismatch";
            return false;
        }
        for (std::size_t index = 0;
             index < race.contactSoundPaths.size(); ++index)
        {
            if (!recordEndsWith(
                    race.contactSoundPaths[index],
                    "light_impact0" + std::to_string(index + 1U) +
                        ".ogg"))
            {
                error =
                    "source PairPxContactEffect sound catalog mismatch";
                return false;
            }
        }
        const auto vehicleNamed = [&](std::string_view name) {
            const auto found = std::find_if(
                race.vehicles.begin(), race.vehicles.end(),
                [&](const Vehicle& vehicle) {
                    return recordEndsWith(vehicle.record, name);
                });
            return found == race.vehicles.end()
                       ? static_cast<const Vehicle*>(nullptr)
                       : &*found;
        };
        const auto* guseniza = vehicleNamed("guseniza");
        const auto* gusenizaBoss = vehicleNamed("gusenizaBoss");
        const auto* podushka = vehicleNamed("podushka");
        const auto* podushkaBoss = vehicleNamed("podushkaBoss");
        const auto trackMatchesSource = [&](const Vehicle* vehicle) {
            return vehicle != nullptr &&
                   vehicle->trackVisuals.size() == 1U &&
                   vehicle->cushionVisuals.empty() &&
                   vehicle->trackVisuals.front().subMesh == 1 &&
                   vehicle->trackVisuals.front().materials.size() == 1U &&
                   recordEndsWith(
                       vehicle->trackVisuals.front()
                           .materials.front().record,
                       "gusenizaChain") &&
                   std::count(vehicle->wheelSlipEffects.begin(),
                              vehicle->wheelSlipEffects.end(), true) == 2;
        };
        const auto cushionMatchesSource = [&](const Vehicle* vehicle) {
            return vehicle != nullptr && vehicle->trackVisuals.empty() &&
                   vehicle->cushionVisuals.size() == 2U &&
                   vehicle->cushionVisuals[0].tag == 1 &&
                   vehicle->cushionVisuals[1].tag == 2 &&
                   vehicle->cushionVisuals[0].subMesh == 1 &&
                   vehicle->cushionVisuals[1].subMesh == 2 &&
                   near(vehicle->cushionVisuals[0]
                            .transform.position.z,
                        -0.09F) &&
                   near(vehicle->cushionVisuals[1]
                            .transform.position.z,
                        -0.09F) &&
                   std::count(vehicle->wheelSlipEffects.begin(),
                              vehicle->wheelSlipEffects.end(), true) == 2;
        };
        std::string slipBehaviorMismatch;
        const bool motorRangesMatchSource = std::all_of(
            race.vehicles.begin(), race.vehicles.end(),
            [&](const Vehicle& vehicle) {
                const bool hasSlipVisual = std::any_of(
                    vehicle.wheelSlipEffects.begin(),
                    vehicle.wheelSlipEffects.end(),
                    [](bool enabled) { return enabled; });
                const auto slipSoundCount = static_cast<std::size_t>(
                    std::count(
                        vehicle.wheelSlipSounds.begin(),
                        vehicle.wheelSlipSounds.end(), true));
                bool exactSlipBehaviors =
                    vehicle.wheelSlipBehaviors.size() ==
                    vehicle.physics.wheels.size();
                std::size_t sourceSlipSoundCount = 0U;
                for (std::size_t wheel = 0U;
                     wheel < vehicle.wheelSlipBehaviors.size(); ++wheel)
                {
                    const auto& behaviors =
                        vehicle.wheelSlipBehaviors[wheel];
                    const bool wheelHasSound = std::any_of(
                        behaviors.begin(), behaviors.end(),
                        [](const WheelSlipEffectDefinition& behavior) {
                            return !behavior.soundPaths.empty();
                        });
                    sourceSlipSoundCount += wheelHasSound ? 1U : 0U;
                    exactSlipBehaviors = exactSlipBehaviors &&
                        wheel < vehicle.wheelSlipSounds.size() &&
                        vehicle.wheelSlipSounds[wheel] == wheelHasSound;
                    if (vehicle.wheelSlipEffects[wheel])
                    {
                        const bool hasTrail =
                            behaviors.size() == 2U &&
                            recordEndsWith(
                                behaviors.front().visual.record, "trail");
                        const bool hasSmoke =
                            !behaviors.empty() &&
                            recordEndsWith(
                                behaviors.back().visual.record, "smoke7");
                        const bool soundMatches = std::all_of(
                            behaviors.begin(), behaviors.end(),
                            [&](const WheelSlipEffectDefinition& behavior) {
                                return behavior.soundPaths.empty() ||
                                       (wheel == 0U &&
                                        recordEndsWith(
                                            behavior.visual.record,
                                            "trail"));
                            });
                        exactSlipBehaviors = exactSlipBehaviors &&
                            (behaviors.size() == 1U ||
                             behaviors.size() == 2U) &&
                            hasSmoke && soundMatches &&
                            (!hasTrail ||
                             near(behaviors.front().position.z, 0.01F));
                    }
                    else
                    {
                        exactSlipBehaviors =
                            exactSlipBehaviors && behaviors.empty();
                    }
                }
                if (!exactSlipBehaviors && slipBehaviorMismatch.empty())
                {
                    slipBehaviorMismatch = vehicle.record + ":" +
                        std::to_string(vehicle.wheelSlipBehaviors.size()) +
                        "/" +
                        std::to_string(vehicle.physics.wheels.size());
                    for (const auto& behaviors :
                         vehicle.wheelSlipBehaviors)
                    {
                        slipBehaviorMismatch +=
                            ":" + std::to_string(behaviors.size());
                    }
                }
                const bool valid = !vehicle.idleSoundPath.empty() &&
                       !vehicle.rpmSoundPath.empty() &&
                       near(vehicle.rpmVolumeRange[0], 0.0F) &&
                       near(vehicle.rpmVolumeRange[1], 1.0F) &&
                       near(vehicle.rpmFrequencyRange[0], 0.0F) &&
                       near(vehicle.rpmFrequencyRange[1], 1.0F) &&
                       vehicle.wheelSlipEffects.size() ==
                           vehicle.physics.wheels.size() &&
                       vehicle.wheelSlipSounds.size() ==
                           vehicle.physics.wheels.size() &&
                       slipSoundCount == sourceSlipSoundCount &&
                       exactSlipBehaviors;
                if (!valid && slipBehaviorMismatch.empty())
                {
                    slipBehaviorMismatch = vehicle.record +
                        ": effects=" +
                        std::to_string(vehicle.wheelSlipEffects.size()) +
                        ", behaviors=" +
                        std::to_string(vehicle.wheelSlipBehaviors.size()) +
                        ", physics=" +
                        std::to_string(vehicle.physics.wheels.size()) +
                        ", soundFlags=" +
                        std::to_string(vehicle.wheelSlipSounds.size()) +
                        ", soundCount=" +
                        std::to_string(slipSoundCount) +
                        ", hasVisual=" +
                        std::to_string(hasSlipVisual) +
                        ", firstSound=" +
                        std::to_string(
                            !vehicle.wheelSlipSounds.empty() &&
                            vehicle.wheelSlipSounds.front()) +
                        ", exact=" +
                        std::to_string(exactSlipBehaviors);
                }
                return valid;
            });
        const bool colorMaterialMatchesSource = std::all_of(
            race.vehicles.begin(), race.vehicles.end(),
            [](const Vehicle& vehicle) {
                return !vehicle.disableColor &&
                       !vehicle.bodyVisuals.empty();
            });
        const bool smokeMatchesSource =
            recordEndsWith(race.wheelSmokeEffect.record, "smoke7") &&
            race.wheelSmokeEffect.graphOrder == GraphOrder::Effect &&
            race.wheelSmokeEffect.particleEmitters.size() == 1U &&
            race.wheelSmokeEffect.particleEmitters.front()
                .distanceTriggered &&
            near(race.wheelSmokeEffect.particleEmitters.front()
                     .lifeMinimum,
                 0.5F) &&
            near(race.wheelSmokeEffect.particleEmitters.front()
                     .lifeMaximum,
                 0.6F) &&
            recordEndsWith(
                race.wheelSlipSoundPath, "SkidAsphalt.ogg");
        const bool trailMatchesSource =
            recordEndsWith(race.wheelTrailEffect.record, "trail") &&
            race.wheelTrailEffect.particleEmitters.size() == 1U &&
            race.wheelTrailEffect.particleEmitters.front().renderMode ==
                ParticleRenderMode::Trail &&
            race.wheelTrailEffect.particleEmitters.front()
                .distanceTriggered &&
            race.wheelTrailEffect.particleEmitters.front()
                    .maximumParticles == 100U &&
            near(race.wheelTrailEffect.particleEmitters.front()
                     .lifeMinimum,
                 10.0F) &&
            near(race.wheelTrailEffect.particleEmitters.front()
                     .lifeMaximum,
                 10.0F) &&
            near(race.wheelTrailEffect.particleEmitters.front()
                     .startTimeMinimum,
                 1.0F) &&
            near(race.wheelTrailEffect.particleEmitters.front()
                     .trailWidth,
                 0.3F) &&
            race.wheelTrailEffect.particleEmitters.front()
                .trailFixedUpEnabled;
        const auto slopedTrack = std::find_if(
            race.trackDefinitions.begin(), race.trackDefinitions.end(),
            [&](const ObjectDefinition& definition) {
                return recordEndsWith(definition.record,
                                      "World1\\tramp2");
            });
        const bool actorLightingVectorsMatchSource =
            slopedTrack != race.trackDefinitions.end() &&
            slopedTrack->lighting == LightingMode::Pixel &&
            near(slopedTrack->graphVector1.x, -0.258819F) &&
            near(slopedTrack->graphVector1.z, 0.965926F) &&
            near(slopedTrack->graphVector3.y, -0.15F) &&
            near(slopedTrack->graphVector3.z, 0.075F);
        if (!trackMatchesSource(guseniza) ||
            !trackMatchesSource(gusenizaBoss) ||
            !cushionMatchesSource(podushka) ||
            !cushionMatchesSource(podushkaBoss) ||
            !motorRangesMatchSource || !colorMaterialMatchesSource ||
            !smokeMatchesSource ||
            !trailMatchesSource || !actorLightingVectorsMatchSource)
        {
            error =
                "source GusenizaAnim/PodushkaAnim/SoundMotor/"
                "Player::ApplyColorMat/PxWheelSlipEffect/FxTrailManager/"
                "GraphManager texDiffK "
                "provenance mismatch: motor=" +
                std::to_string(motorRangesMatchSource) +
                ", slip=" + slipBehaviorMismatch;
            return false;
        }
        if (race.levelPath != "Data/Map/World1/map1.r3dMap" ||
            race.lapCount != 4 || race.vehicle.record.find("marauder") ==
                                      std::string::npos ||
            race.trackDefinitions.size() != 4 ||
            race.trackInstances.size() != 52 || race.tracePoints.size() != 5 ||
            race.tracePath.size() != 6 ||
            race.decorationInstances.size() != 234 ||
            race.bonuses.size() != 7 ||
            race.decorationInstances.front().name != "semaphore0" ||
            !race.decorationInstances.front().hasProxyState ||
            !near(race.decorationInstances.front().life, -1.0F) ||
            race.trackInstances.front().name != "track20" ||
            !race.trackInstances.front().hasProxyState ||
            race.bonuses.front().name != "money0" ||
            !race.bonuses.front().hasProxyState ||
            !near(race.bonuses.front().maximumTimeLife, 0.0F) ||
            race.decorationInstances.front().mapObjectId != 1U ||
            race.decorationInstances.back().mapObjectId != 234U ||
            race.trackInstances.front().mapObjectId != 235U ||
            race.trackInstances.back().mapObjectId != 286U ||
            race.bonuses.front().mapObjectId != 287U ||
            race.bonuses.back().mapObjectId != 293U ||
            race.firstDynamicMapObjectId != 294U ||
            race.racers.empty() ||
            race.racers.front().mapObjectId != 294U ||
            race.racers.back().mapObjectId !=
                293U + race.racers.size() ||
            race.trackCatalog.size() != 88 ||
            physics.collisionMeshes.empty() || borderMeshCount == 0 ||
            triangleCount < 591 ||
            !near(race.touchBorderDamage[0], 5.0F) ||
            !near(race.touchBorderDamage[1], 5.0F) ||
            !near(race.touchBorderDamageForce[0], 3800000.0F) ||
            !near(race.touchBorderDamageForce[1], 3800000.0F) ||
            !near(race.touchCarDamage[0], 5.0F) ||
            !near(race.touchCarDamage[1], 5.0F) ||
            !near(race.touchCarDamageForce[0], 1100000.0F) ||
            !near(race.touchCarDamageForce[1], 1100000.0F) ||
            !near(physics.vehicle.mass, 2000.0F) ||
            !near(physics.vehicle.shapePosition.x, 0.0654583F) ||
            !near(physics.vehicle.centerOfMass.z, -0.75F) ||
            !near(physics.vehicle.angularDamping.x, 1.0F) ||
            !near(physics.vehicle.angularDamping.y, 1.0F) ||
            !near(physics.vehicle.angularDamping.z, 0.0F) ||
            !near(physics.vehicle.bodyFriction, 0.08F) ||
            !near(physics.vehicle.brakeTorque, 7500.0F) ||
            !near(physics.vehicle.differentialRatio, 3.42F) ||
            !near(physics.vehicle.maximumRpm, 7000.0F) ||
            // Original defaults install truba1 + engine1: 100 + 900.
            !near(physics.vehicle.maximumTorque, 1000.0F) ||
            !near(physics.vehicle.idlingRpm, 1000.0F) ||
            !near(physics.vehicle.torqueEfficiency, 0.805F) ||
            !near(physics.vehicle.restBrakeTorque, 400.0F) ||
            !near(physics.vehicle.maximumSpeed, 42.0F) ||
            race.vehicle.boundingSize <= 0.0F ||
            !near(race.vehicle.boundingRadius,
                  race.vehicle.boundingSize * 0.5F) ||
            !near(physics.vehicle.airbornePitchAcceleration, 0.523599F) ||
            !near(physics.vehicle.clampRollAngle, 0.261799F) ||
            !near(physics.vehicle.clampPitchAngle, 0.523599F) ||
            !near(physics.vehicle.steerSpeed, 3.5F) ||
            !near(physics.vehicle.steerRotation, 3.5F) ||
            !physics.vehicle.automaticGears ||
            physics.vehicle.gravitySteering ||
            physics.vehicle.clutchImmunity ||
            !near(race.vehicle.maximumLife, 70.0F) ||
            physics.vehicle.wheels.size() != 4 ||
            !near(physics.vehicle.wheels[0].radius, 0.42947F) ||
            race.vehicle.wheelMeshPath !=
                "Data/Car/marauderWheel.r3d" ||
            playerWheelRadius < 0.44F || playerWheelRadius > 0.46F ||
            usesWorkshopPreviewWheel ||
            !near(physics.vehicle.wheels[0].suspensionTravel, 0.3F) ||
            // wheel1 adds a 2g tire-reaction cutoff, not coil stiffness.
            !near(physics.vehicle.wheels[0].spring, 140000.0F) ||
            !near(physics.vehicle.wheels[0].damper, 1000.0F) ||
            !near(physics.vehicle.tireSpring, 2.0F) ||
            !physics.vehicle.wheels[0].driven ||
            !physics.vehicle.wheels[0].steering ||
            physics.vehicle.wheels[2].driven ||
            physics.vehicle.wheels[2].steering ||
            !near(race.vehicle.bodyVisualTransform.scale.z, 0.95F) ||
            race.vehicle.wheelVisuals.size() != 4 ||
            race.vehicle.wheelVisualTransforms.size() != 4 ||
            race.vehicle.wheelVisualOffsets.size() != 4 ||
            race.vehicle.wheelInverted.size() != 4 ||
            race.vehicle.wheelInverted[0] ||
            !near(race.vehicle.wheelVisualTransforms[1].scale.y, -1.0F) ||
            race.vehicle.wheelVisuals[0].cullMode !=
                VisualNode::CullMode::Inherit ||
            race.vehicle.wheelVisuals[1].cullMode !=
                VisualNode::CullMode::CounterClockwise ||
            !near(race.vehicle.wheelVisualOffsets[0].x, 0.05F) ||
            race.racers.size() < 2U ||
            !near(race.racers[1].color[0], 91.0F / 255.0F) ||
            !near(race.racers[1].color[1], 41.0F / 255.0F) ||
            !near(race.racers[1].color[2], 165.0F / 255.0F) ||
            alphaTestMaterialCount == 0 ||
            !alphaTestThresholdMatchesSource ||
            cullOpacityDefinitionCount == 0 ||
            race.rainEffect.graphOrder != GraphOrder::Effect ||
            race.wheelTrailEffect.graphOrder != GraphOrder::Effect)
        {
            error =
                "original tournament/map/db/garage provenance mismatch: "
                "level=" + race.levelPath +
                ", laps=" + std::to_string(race.lapCount) +
                ", tracks=" + std::to_string(race.trackDefinitions.size()) +
                "/" + std::to_string(race.trackInstances.size()) +
                ", trace=" + std::to_string(race.tracePoints.size()) +
                "/" + std::to_string(race.tracePath.size()) +
                ", decorations=" +
                std::to_string(race.decorationInstances.size()) +
                ", bonuses=" + std::to_string(race.bonuses.size()) +
                ", catalog=" + std::to_string(race.trackCatalog.size()) +
                ", triangles=" + std::to_string(triangleCount) +
                ", borders=" + std::to_string(borderMeshCount) +
                ", alpha=" + std::to_string(alphaTestMaterialCount) +
                "/" +
                (alphaTestThresholdMatchesSource ? "source" : "mismatch") +
                ", cull=" +
                std::to_string(cullOpacityDefinitionCount) +
                ", vehicleMass=" + std::to_string(physics.vehicle.mass) +
                ", torque=" +
                std::to_string(physics.vehicle.maximumTorque) +
                ", bodyScaleZ=" +
                std::to_string(race.vehicle.bodyVisualTransform.scale.z) +
                ", wheels=" +
                std::to_string(physics.vehicle.wheels.size()) + "/" +
                std::to_string(race.vehicle.wheelVisuals.size()) +
                ", rainOrder=" +
                std::to_string(static_cast<int>(race.rainEffect.graphOrder)) +
                ", trailOrder=" + std::to_string(
                    static_cast<int>(race.wheelTrailEffect.graphOrder));
            return false;
        }
        error.clear();
        return true;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return false;
    }
}

} // namespace r3d::game::originalrace
