#include "OriginalRace.h"

#include "OriginalProfile.h"
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

void applyMobilityLoadout(
    Vehicle& vehicle, const resource::ResourceFileSystem& resources,
    TiXmlElement* workshop, const std::vector<RacerSlot>& loadout,
    std::string_view difficulty, bool updateWheelVisual)
{
    const float baseMaximumSpeed = vehicle.physics.maximumSpeed;
    float maximumTorque = 0.0F;
    float maximumLife = 0.0F;
    float maximumSpeed = 0.0F;
    float tireSpring = 0.0F;
    r3d::physics::WheelDescription::TireFunction longitudinalTire;
    r3d::physics::WheelDescription::TireFunction lateralTire;
    auto addTire = [&](TiXmlElement* function, const char* name,
                       auto& output) {
        auto* tire = child(function, name);
        if (tire == nullptr)
            return;
        output.extremumSlip +=
            optionalScalar(tire, "extremumSlip", 0.0F);
        output.extremumValue +=
            optionalScalar(tire, "extremumValue", 0.0F);
        output.asymptoteSlip +=
            optionalScalar(tire, "asymptoteSlip", 0.0F);
        output.asymptoteValue +=
            optionalScalar(tire, "asymptoteValue", 0.0F);
    };

    for (const auto& slot : loadout)
    {
        const auto itemName = basename(slot.record);
        if (itemName.empty())
            continue;
        TiXmlElement* entry = nullptr;
        for (auto* candidate = workshop->FirstChildElement();
             candidate != nullptr;
             candidate = candidate->NextSiblingElement())
        {
            if (std::string_view(candidate->Value()) == itemName)
            {
                entry = candidate;
                break;
            }
        }
        if (entry == nullptr)
            continue;
        auto* item = child(entry, "item");
        auto* functions = child(item, "carFuncMap");
        if (functions == nullptr)
            continue;
        TiXmlElement* function = nullptr;
        for (auto* candidate = functions->FirstChildElement();
             candidate != nullptr;
             candidate = candidate->NextSiblingElement())
        {
            auto* carElement = child(candidate, "car");
            const char* car =
                carElement == nullptr ? nullptr : carElement->GetText();
            if (car != nullptr &&
                (vehicle.record == car ||
                 basename(vehicle.record) == basename(car)))
            {
                function = candidate;
                break;
            }
        }
        if (function == nullptr)
            continue;

        maximumTorque += optionalScalar(function, "maxTorque", 0.0F);
        maximumLife += optionalScalar(function, "life", 0.0F);
        maximumSpeed = std::max(
            maximumSpeed, optionalScalar(function, "maxSpeed", 0.0F));
        tireSpring += optionalScalar(function, "tireSpring", 0.0F);
        addTire(function, "longTire", longitudinalTire);
        addTire(function, "latTire", lateralTire);

        if (updateWheelVisual && slot.type == "stWheel")
        {
            auto* mesh = child(item, "mesh");
            auto* texture = child(item, "texture");
            if (mesh != nullptr && texture != nullptr &&
                mesh->Attribute("item") != nullptr &&
                texture->Attribute("item") != nullptr)
            {
                const auto meshPath = canonicalDataPath(
                    resources, mesh->Attribute("item"));
                const auto texturePath = canonicalDataPath(
                    resources, texture->Attribute("item"));
                vehicle.wheelMeshPath = meshPath;
                for (auto& visual : vehicle.wheelVisuals)
                {
                    visual.meshPath = meshPath;
                    visual.materials = {
                        {"Upgrade\\" + itemName, texturePath,
                         MaterialBlend::Opaque, 0.0F}};
                }
            }
        }
    }

    // Player::ApplyMobility resets these values, accumulates every installed
    // mobility slot, and then restores only the car's base maximum speed.
    vehicle.physics.maximumTorque = maximumTorque;
    vehicle.physics.maximumSpeed = baseMaximumSpeed + maximumSpeed;
    float armorScale = 1.75F;
    if (difficulty == "gdEasy")
        armorScale = 2.0F;
    else if (difficulty == "gdHard")
        armorScale = 1.5F;
    vehicle.maximumLife = maximumLife * armorScale;
    for (auto& wheel : vehicle.physics.wheels)
    {
        wheel.spring += tireSpring;
        wheel.longitudinalTire = longitudinalTire;
        wheel.lateralTire = lateralTire;
    }
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
    race.racers.push_back(
        {"Human", {}, humanVehicle->second, true, {}, {}, false});
    race.racers.back().configuredVehicle =
        race.vehicles[humanVehicle->second];
    race.racers.back().hasConfiguredVehicle = true;
    auto* opponentEntries =
        require(planet, "players", "tournamet.xml/planet");
    const std::string requestedPass = std::to_string(racePass);
    for (auto* opponent = opponentEntries->FirstChildElement();
         opponent != nullptr; opponent = opponent->NextSiblingElement())
    {
        auto* choices =
            require(opponent, "cars", "tournamet.xml/player");
        TiXmlElement* selected = nullptr;
        for (auto* choice = choices->FirstChildElement();
             choice != nullptr; choice = choice->NextSiblingElement())
        {
            if (text(choice, "pass", "tournamet.xml/player car") ==
                requestedPass)
            {
                selected = choice;
                break;
            }
        }
        if (selected == nullptr)
            continue;
        const std::string record =
            text(selected, "record", "tournamet.xml/player car");
        const auto vehicle = vehicleIndices.find(record);
        if (vehicle == vehicleIndices.end())
            throw resource::ResourceError(
                "garage.xml: AI tournament car is missing: " + record);
        Racer racer;
        racer.name = text(opponent, "name", "tournamet.xml/player");
        if (auto* photo = child(opponent, "photo");
            photo != nullptr && photo->Attribute("item") != nullptr)
        {
            racer.photoPath =
                canonicalDataPath(resources, photo->Attribute("item"));
        }
        racer.vehicle = vehicle->second;
        racer.configuredVehicle = race.vehicles[vehicle->second];
        racer.hasConfiguredVehicle = true;
        if (auto* slots = child(opponent, "slots"))
        {
            for (auto* slot = slots->FirstChildElement(); slot != nullptr;
                 slot = slot->NextSiblingElement())
            {
                if (text(slot, "pass", "tournamet.xml/player slot") !=
                    requestedPass)
                    continue;
                racer.loadout.push_back(
                    {text(slot, "record",
                          "tournamet.xml/player slot"),
                     text(slot, "type", "tournamet.xml/player slot"),
                     unsignedValue(
                         text(slot, "charge",
                              "tournamet.xml/player slot"),
                         "tournamet.xml/player slot/charge")});
            }
        }
        race.racers.push_back(std::move(racer));
    }
    if (race.racers.size() < 2)
        throw resource::ResourceError(
            "tournamet.xml: selected race has no AI opponents");
    static constexpr std::array<std::array<float, 4>, 8> aiColors{{
        {91.0F / 255.0F, 41.0F / 255.0F, 165.0F / 255.0F, 1.0F},
        {158.0F / 255.0F, 158.0F / 255.0F, 158.0F / 255.0F, 1.0F},
        {1.0F, 128.0F / 255.0F, 192.0F / 255.0F, 1.0F},
        {131.0F / 255.0F, 247.0F / 255.0F, 204.0F / 255.0F, 1.0F},
        {131.0F / 255.0F, 229.0F / 255.0F, 0.0F, 1.0F},
        {216.0F / 255.0F, 229.0F / 255.0F, 133.0F / 255.0F, 1.0F},
        {97.0F / 255.0F, 0.0F, 185.0F / 255.0F, 1.0F},
        {0.0F, 108.0F / 255.0F, 164.0F / 255.0F, 1.0F},
    }};
    for (std::size_t index = 1; index < race.racers.size(); ++index)
        race.racers[index].color =
            aiColors[(index - 1U) % aiColors.size()];
}

MaterialDefinition materialDefinition(
    const resource::ResourceFileSystem& resources, std::string_view legacy)
{
    const std::string record(legacy);
    auto tune = [&](MaterialDefinition material) {
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
                material.color = {0.25F, 0.25F, 0.25F, 1.0F};
            else if (record == "Effect\\smoke2")
                material.color = {0.5F, 0.32F, 0.25F, 1.0F};
            else if (record == "Effect\\flare1" ||
                     record == "Effect\\flare3")
                material.color = {1.0F, 0.58F, 0.36F, 1.0F};
            else if (record == "Effect\\flare2" ||
                     record == "Effect\\flare7Red")
                material.color = {1.0F, 0.0F, 0.0F, 1.0F};
            else if (record == "Effect\\dust_smoke_06")
                material.color = {0.2F, 0.2F, 1.0F, 1.0F};
            else if (record == "Effect\\ExplosionRay" ||
                     record == "Effect\\lens1")
                material.color = {0.0F, 0.0F, 1.0F, 1.0F};
            else if (record == "Effect\\ExplosionRing")
                material.color = {1.0F, 1.0F, 0.0F, 1.0F};
            else if (record == "Effect\\thunder1")
                material.color =
                    {236.0F / 255.0F, 0.0F, 140.0F / 255.0F, 1.0F};
        }
        if (record == "Car\\blend")
        {
            material.blend = MaterialBlend::Additive;
            material.color[3] = 0.7F;
            material.emissive = 1.0F;
            material.ignoreFog = true;
        }
        if (record == "Bonus\\shield")
        {
            material.emissive = 1.0F;
            material.ignoreFog = true;
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
        }
        // ResourceManager::LoadBumpLibMat binds a second sampler whose
        // shipped name follows the diffuse texture with `_norm`. This is
        // active for the World2 track and bridge materials.
        if (!material.texturePath.empty() &&
            material.normalTexturePath.empty())
        {
            const auto extension = material.texturePath.find_last_of('.');
            if (extension != std::string::npos)
            {
                auto candidate = material.texturePath;
                candidate.insert(extension, "_norm");
                if (resources.exists(candidate))
                {
                    material.normalTexturePath = std::move(candidate);
                    // LoadBumpLibMat uses this exact D3D material state.
                    material.specular = 0.5F;
                    material.shininess = 128.0F;
                }
            }
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
        {"Bonus\\mineSpike", "Data/Bonus/mineSpike.dds",
         MaterialBlend::AlphaTest},
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
    };
    if (record == "Effect\\gravBall" ||
        record == "Weapon\\mortiraBall")
    {
        MaterialDefinition material;
        material.record = record;
        material.blend = MaterialBlend::Opaque;
        material.color =
            record == "Effect\\gravBall"
                ? std::array<float, 4>{1.0F, 0.0F, 0.0F, 1.0F}
                : std::array<float, 4>{0.0F, 0.0F, 0.0F, 1.0F};
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

    auto directMapping = [&](std::string_view prefix,
                             std::string_view directory)
        -> std::optional<MaterialDefinition> {
        if (record.rfind(prefix, 0) != 0)
            return std::nullopt;
        std::string name = record.substr(prefix.size());
        std::string path(directory);
        path += name;
        path += ".dds";
        path = canonicalDataPath(resources, path);
        if (resources.exists(path))
        {
            MaterialBlend blend = MaterialBlend::Opaque;
            if (prefix == "Effect\\")
            {
                const bool transparency =
                    name.find("smoke") != std::string::npos ||
                    name.find("frost") != std::string::npos ||
                    name == "asphaltMarks" || name == "drop" ||
                    name == "crater" || name == "boom1" ||
                    name == "boom2";
                blend = transparency ? MaterialBlend::Transparency
                                     : MaterialBlend::Additive;
            }
            else if (prefix == "Bonus\\")
            {
                blend = MaterialBlend::Opaque;
            }
            return tune(MaterialDefinition{
                record, path, blend, 0.0F});
        }
        return std::nullopt;
    };
    if (auto value = directMapping("Car\\", "Data/Car/"))
        return *value;
    if (auto value = directMapping("Weapon\\", "Data/Weapon/"))
        return *value;
    if (auto value = directMapping("Effect\\", "Data/Effect/"))
        return *value;
    if (auto value = directMapping("Bonus\\", "Data/Bonus/"))
        return *value;

    const auto slash = record.find('\\');
    if (slash != std::string::npos)
    {
        const std::string world = record.substr(0, slash);
        const std::string name = basename(record);
        std::vector<std::string> candidates;
        if (record.find("\\Track\\") != std::string::npos)
        {
            candidates.push_back("Data/" + world +
                                 "/Track/Texture/" + name + ".dds");
            candidates.push_back("Data/" + world +
                                 "/Track/texture/" + name + ".dds");
            candidates.push_back("Data/" + world +
                                 "/Track/Texture/track1.dds");
            candidates.push_back("Data/" + world +
                                 "/Track/texture/track1.dds");
        }
        candidates.push_back("Data/" + world + "/Texture/" + name +
                             ".dds");
        candidates.push_back("Data/" + world + "/texture/" + name +
                             ".dds");
        candidates.push_back("Data/" + world + "/" + name + ".dds");
        for (const auto& requested : candidates)
        {
            const auto candidate =
                canonicalDataPath(resources, requested);
            if (!resources.exists(candidate))
                continue;
            const bool transparent =
                name.find("tree") != std::string::npos ||
                name.find("palma") != std::string::npos ||
                name.find("grass") != std::string::npos ||
                name.find("bush") != std::string::npos ||
                name.find("fern") != std::string::npos ||
                name.find("tree") != std::string::npos ||
                name.find("Tree") != std::string::npos ||
                name.find("elka") != std::string::npos ||
                name.find("poplar") != std::string::npos;
            return tune({record, candidate,
                         transparent ? MaterialBlend::AlphaTest
                                     : MaterialBlend::Opaque,
                         transparent ? 0.1F : 0.0F});
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
            type != nullptr && std::string_view(type) == "ntSprite" &&
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

void appendParticleEmitters(
    const resource::ResourceFileSystem& resources,
    TiXmlElement* record, const Transform& parentTransform,
    ObjectDefinition& definition, std::string_view source)
{
    auto* nodes = child(record, "grActor/nodes/items");
    if (nodes == nullptr)
        return;
    for (auto* node = nodes->FirstChildElement(); node != nullptr;
         node = node->NextSiblingElement())
    {
        const char* type = node->Attribute("type");
        if (type == nullptr ||
            std::string_view(type) != "ntParticleSystem")
            continue;
        const Transform nodeTransform =
            compose(parentTransform, elementTransform(node, source));
        bool fixedDirection = false;
        ParticleRenderMode renderMode = ParticleRenderMode::Sprite;
        if (auto* manager = child(node, "fxManager");
            manager != nullptr && manager->GetText() != nullptr)
        {
            const std::string_view managerName(manager->GetText());
            fixedDirection =
                managerName.find("fxDirSpriteManager") !=
                std::string_view::npos;
            if (managerName.find("fxPointSpritesManager") !=
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
            else if (managerName.find("fxNodeManager") !=
                     std::string_view::npos)
                renderMode = ParticleRenderMode::Node;
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
        if (materials.empty())
            continue;
        auto* emitters = child(node, "emitters/items");
        if (emitters == nullptr)
            continue;
        for (auto* sourceEmitter = emitters->FirstChildElement();
             sourceEmitter != nullptr;
             sourceEmitter = sourceEmitter->NextSiblingElement())
        {
            auto* part = child(sourceEmitter, "partDesc");
            auto* flow = child(sourceEmitter, "flowDesc");
            if (part == nullptr || flow == nullptr)
                continue;
            ParticleEmitterDefinition emitter;
            emitter.transform = nodeTransform;
            emitter.materials = materials;
            emitter.fixedDirection = fixedDirection;
            emitter.renderMode = renderMode;
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
            emitter.startScaleMinimum = optionalParticleVector(
                part, "startScale/min", {1.0F, 1.0F, 1.0F},
                source);
            emitter.startScaleMaximum = optionalParticleVector(
                part, "startScale/max",
                emitter.startScaleMinimum, source);
            emitter.startRotationMinimum =
                optionalParticleQuaternion(
                    part, "startRot/min", {}, source);
            emitter.startRotationMaximum =
                optionalParticleQuaternion(
                    part, "startRot/max",
                    emitter.startRotationMinimum, source);
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
            emitter.rangeScaleMinimum = optionalParticleVector(
                part, "rangeScale/min", {}, source);
            emitter.rangeScaleMaximum = optionalParticleVector(
                part, "rangeScale/max",
                emitter.rangeScaleMinimum, source);
            emitter.rangeRotationMinimum =
                optionalParticleQuaternion(
                    part, "rangeRot/min", {}, source);
            emitter.rangeRotationMaximum =
                optionalParticleQuaternion(
                    part, "rangeRot/max",
                    emitter.rangeRotationMinimum, source);
            emitter.velocityMinimum = optionalParticleVector(
                flow, "speedPos/min", {}, source);
            emitter.velocityMaximum = optionalParticleVector(
                flow, "speedPos/max", emitter.velocityMinimum,
                source);
            emitter.rotationVelocityMinimum =
                optionalParticleQuaternion(
                    flow, "speedRot/min", {}, source);
            emitter.rotationVelocityMaximum =
                optionalParticleQuaternion(
                    flow, "speedRot/max",
                    emitter.rotationVelocityMinimum, source);
            emitter.scaleVelocityMinimum = optionalParticleVector(
                flow, "speedScale/min", {}, source);
            emitter.scaleVelocityMaximum = optionalParticleVector(
                flow, "speedScale/max",
                emitter.scaleVelocityMinimum, source);
            emitter.accelerationMinimum = optionalParticleVector(
                flow, "acceleration/min", {}, source);
            emitter.accelerationMaximum = optionalParticleVector(
                flow, "acceleration/max",
                emitter.accelerationMinimum, source);
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
    }
}

void appendIncludedEffects(
    const resource::ResourceFileSystem& resources,
    TiXmlElement* database, TiXmlElement* record,
    const Transform& parentTransform, ObjectDefinition& definition,
    std::string_view source, std::uint32_t depth)
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
        if (reference == nullptr || reference->GetText() == nullptr)
            continue;
        const Transform includeTransform =
            compose(parentTransform, elementTransform(include, source));
        auto* includedRecord =
            databaseRecord(database, reference->GetText());
        auto nodes = visualNodes(resources, includedRecord, source);
        for (auto& node : nodes)
        {
            node.transform =
                compose(includeTransform, node.transform);
            definition.visualNodes.push_back(std::move(node));
        }
        appendParticleEmitters(
            resources, includedRecord, includeTransform, definition,
            source);
        appendIncludedEffects(
            resources, database, includedRecord, includeTransform,
            definition, source, depth + 1U);
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
    appendParticleEmitters(
        resources, dbRecord, Transform{}, result, source);
    appendIncludedEffects(
        resources, database, dbRecord, Transform{}, result, source, 0U);
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
                for (auto& node : nodes)
                {
                    node.transform =
                        compose(pieceTransform, node.transform);
                    result.visualNodes.push_back(std::move(node));
                }
                loadCollisionShapes(resources, result, piece, source);
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
    if (auto* life = child(dbRecord, "maxLife");
        life != nullptr && life->GetText() != nullptr)
    {
        std::istringstream stream(life->GetText());
        stream >> result.maximumLife;
    }
    result.destructible =
        std::string(record).find("\\Crush\\") != std::string::npos ||
        result.maximumLife > 0.0F;
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

std::string weaponEffectTexture(
    const resource::ResourceFileSystem& resources,
    std::uint32_t projectileType)
{
    static constexpr std::array<std::string_view, 25> effects{{
        "Data/Effect/bullet.dds",
        "Data/Effect/engine1.dds",
        "Data/Effect/rocketAir.dds",
        "Data/Effect/laser3-blue.dds",
        "Data/Effect/smoke1.dds",
        "Data/Effect/smoke1.dds",
        "Data/Effect/rad_add.dds",
        "Data/Effect/shield1.dds",
        "Data/Effect/engine1.dds",
        "Data/Effect/drop.dds",
        "Data/Effect/smoke3.dds",
        "Data/Effect/explosion2.dds",
        "Data/Effect/explosion3.dds",
        "Data/Effect/spark1.dds",
        "Data/Effect/firePatron.dds",
        "Data/Effect/gunEff2.dds",
        "Data/Effect/sonar.dds",
        "Data/Effect/ring1.dds",
        "Data/Effect/frostRay.dds",
        "Data/Effect/explosion4.dds",
        "Data/Effect/crater.dds",
        "Data/Effect/phaseRing.dds",
        "Data/Effect/thunder1.dds",
        "Data/Effect/protonRing.dds",
        "Data/Effect/protonRay.dds",
    }};
    const auto path =
        effects[std::min<std::size_t>(projectileType,
                                      effects.size() - 1U)];
    return resources.exists(path) ? std::string(path)
                                  : "Data/Effect/bullet.dds";
}

std::string weaponSoundPath(std::string_view record,
                            std::uint32_t projectileType)
{
    std::string name(record);
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::tolower(value));
                   });
    if (name.find("sonar") != std::string::npos)
        return "Data/Sounds/sonar.ogg";
    if (name.find("turel") != std::string::npos)
        return "Data/Sounds/turel.ogg";
    if (name.find("pulsator") != std::string::npos)
        return "Data/Sounds/pulsator.ogg";
    if (name.find("mortira") != std::string::npos)
        return "Data/Sounds/mortira.ogg";
    if (name.find("frost") != std::string::npos ||
        projectileType == 18U)
        return "Data/Sounds/frost_ray.ogg";
    if (name.find("rezonator") != std::string::npos)
        return "Data/Sounds/rezonator.ogg";
    if (name.find("drobilka") != std::string::npos)
        return "Data/Sounds/shredder.ogg";
    if (name.find("sphere") != std::string::npos)
        return "Data/Sounds/phalanx_shot_a.ogg";
    if (name.find("rocket") != std::string::npos ||
        projectileType == 2U)
        return "Data/Sounds/missile_launch.ogg";
    if (name.find("phase") != std::string::npos ||
        projectileType == 21U)
        return "Data/Sounds/fazowij_izluchatel.ogg";
    if (name.find("laser") != std::string::npos ||
        projectileType == 3U)
        return "Data/Sounds/laserGuseniza.ogg";
    return "Data/Sounds/fireGun.ogg";
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
        if (child(projectile, "offset") != nullptr)
        {
            definition.offset = vector3(
                projectile, "offset", "workshop.xml/weapon/projectile");
        }
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
        definition.mass = optionalScalar(projectile, "mass", 100.0F);
        definition.damage = optionalScalar(projectile, "damage", 0.0F);
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
                    weapon.projectiles[projectileIndex].deathVisual =
                        objectDefinition(
                            resources, database, effectRecord,
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
            projectile.damage = weapon.damage;
            weapon.projectiles.push_back(projectile);
        }
        const auto& firstProjectile = weapon.projectiles.front();
        weapon.projectileType = firstProjectile.type;
        weapon.projectileSpeed = firstProjectile.speed;
        weapon.maximumDistance = firstProjectile.maximumDistance;
        weapon.effectTexturePath =
            weaponEffectTexture(resources, weapon.projectileType);
        weapon.soundPath =
            weaponSoundPath(weapon.record, weapon.projectileType);
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
            else if (type == "btMine")
                definition.bonusKind = BonusKind::Mine;
            else if (type == "btImmortal")
                definition.bonusKind = BonusKind::Shield;
            else if (type == "btSpeedArrow")
                definition.bonusKind = BonusKind::Speed;
        }
        race.achievements.push_back(std::move(definition));
    }
}

void loadRewards(TiXmlElement* planet, Race& race)
{
    race.rewardMoney.fill(0U);
    race.rewardPoints.fill(0U);
    race.requiredPoints.clear();
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
    for (std::size_t mountIndex = 0;
         mountIndex < result.weaponMounts.size(); ++mountIndex)
    {
        const std::string mountName =
            "stWeapon" + std::to_string(mountIndex + 1U);
        auto* mount = child(garageDefinition, mountName);
        if (mount == nullptr)
            continue;
        auto& output = result.weaponMounts[mountIndex];
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
            VehicleWeaponPlacement placement;
            placement.record =
                text(item, "record", "garage.xml/" + mountName);
            placement.rotation =
                quaternion(item, "rot", "garage.xml/" + mountName);
            placement.offset =
                vector3(item, "offset", "garage.xml/" + mountName);
            output.placements.push_back(std::move(placement));
        }
    }

    auto* engineSound =
        require(car, "behaviors/items/item5", source + "/engine sound");
    result.idleSoundPath = canonicalDataPath(
        resources,
        itemAttribute(engineSound, "sndIdle", source + "/engine sound"));
    result.rpmSoundPath = canonicalDataPath(
        resources,
        itemAttribute(engineSound, "sndRPM", source + "/engine sound"));

    auto& vehicle = result.physics;
    vehicle.mass = scalar(car, "pxActor/body/mass", source);
    vehicle.halfExtents =
        vector3(car, "pxActor/shapes/items/item0/dimensions", source);
    vehicle.shapePosition =
        vector3(car, "pxActor/shapes/items/item0/pos", source);
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
        vehicle.wheels.push_back(wheel);
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
    return result;
}

void applyWeatherDescription(
    const resource::ResourceFileSystem& resources, Race& race,
    Weather weather)
{
    auto set = [&](Weather weather, std::string_view sky,
                   std::array<float, 4> fog, float intensity,
                   std::array<float, 4> ambient) {
        race.environment.weather = weather;
        race.environment.skyTexturePath =
            canonicalDataPath(resources, sky);
        race.environment.fogColor = fog;
        race.environment.fogIntensity = intensity;
        race.environment.ambientColor = ambient;
        race.environment.rain = weather == Weather::Rainy;
    };
    switch (weather)
    {
    case Weather::Night:
        set(Weather::Night, "Data/Misc/nightSky.dds",
            {15.0F / 255.0F, 25.0F / 255.0F, 31.0F / 255.0F, 1.0F},
            1.0F,
            {138.0F / 255.0F, 144.0F / 255.0F,
             174.0F / 255.0F, 1.0F});
        break;
    case Weather::Cloudy:
        set(Weather::Cloudy, "Data/World2/texture/skyTex1.dds",
            {192.0F / 255.0F, 189.0F / 255.0F, 184.0F / 255.0F,
             0.0F},
            1.0F, {0.0F, 0.0F, 0.0F, 1.0F});
        break;
    case Weather::Rainy:
        set(Weather::Rainy, "Data/World2/texture/skyTex1.dds",
            {192.0F / 255.0F, 189.0F / 255.0F, 184.0F / 255.0F,
             0.0F},
            1.0F, {0.0F, 0.0F, 0.0F, 1.0F});
        break;
    case Weather::Sahara:
        set(Weather::Sahara, "Data/World3/Texture/skyTex1.dds",
            {87.0F / 255.0F, 81.0F / 255.0F, 115.0F / 255.0F,
             1.0F},
            0.5F, {0.0F, 0.0F, 0.0F, 1.0F});
        break;
    case Weather::Hell:
        set(Weather::Hell, "Data/World4/Texture/skyTex1.dds",
            {82.0F / 255.0F, 12.0F / 255.0F, 8.0F / 255.0F, 1.0F},
            0.5F, {0.0F, 0.0F, 0.0F, 1.0F});
        break;
    case Weather::Snow:
        set(Weather::Snow, "Data/World5/Texture/sky_text.dds",
            {156.0F / 255.0F, 166.0F / 255.0F, 181.0F / 255.0F,
             1.0F},
            0.5F, {0.0F, 0.0F, 0.0F, 1.0F});
        break;
    case Weather::Fair:
        set(Weather::Fair, "Data/World1/Texture/skyTex1.dds",
            {148.0F / 255.0F, 193.0F / 255.0F, 235.0F / 255.0F,
             1.0F},
            0.5F, {0.0F, 0.0F, 0.0F, 1.0F});
        break;
    }
}

Weather weatherFromToken(std::string_view token)
{
    if (token == "ewNight")
        return Weather::Night;
    if (token == "ewClody")
        return Weather::Cloudy;
    if (token == "ewRainy")
        return Weather::Rainy;
    if (token == "ewSahara")
        return Weather::Sahara;
    if (token == "ewHell")
        return Weather::Hell;
    if (token == "ewSnow")
        return Weather::Snow;
    return Weather::Fair;
}

void applyOriginalEnvironment(
    const resource::ResourceFileSystem& resources, Race& race)
{
    race.environment.surface = EnvironmentSurface::None;
    race.environment.planarReflection = false;
    if (race.levelPath.find("World1") != std::string::npos)
    {
        race.environment.surface = EnvironmentSurface::Grass;
        race.environment.hdrLuminanceKey = 1.1F;
        race.environment.hdrBrightThreshold = 1.5F;
        race.environment.hdrGaussianScalar = 30.0F;
        race.environment.hdrExposure = 15.0F;
    }
    else if (race.levelPath.find("World2") != std::string::npos)
    {
        race.environment.surface = EnvironmentSurface::Water;
        race.environment.hdrLuminanceKey = 1.7F;
        race.environment.hdrBrightThreshold = 1.9F;
        race.environment.hdrGaussianScalar = 30.0F;
        race.environment.hdrExposure = 8.0F;
    }
    else if (race.levelPath.find("World3") != std::string::npos)
    {
        race.environment.surface = EnvironmentSurface::GroundFog;
        race.environment.surfaceHeight = 3.0F;
        race.environment.surfaceScroll = 0.02F;
        race.environment.hdrLuminanceKey = 4.0F;
        race.environment.hdrBrightThreshold = 4.5F;
        race.environment.hdrGaussianScalar = 20.0F;
        race.environment.hdrExposure = 3.0F;
    }
    else if (race.levelPath.find("World4") != std::string::npos)
    {
        race.environment.surface = EnvironmentSurface::Magma;
        race.environment.surfaceHeight = 0.5F;
        race.environment.surfaceScroll = 0.01F;
        race.environment.hdrLuminanceKey = 1.9F;
        race.environment.hdrBrightThreshold = 1.9F;
        race.environment.hdrGaussianScalar = 30.0F;
        race.environment.hdrExposure = 8.0F;
    }
    else if (race.levelPath.find("World5") != std::string::npos)
    {
        race.environment.planarReflection = true;
        race.environment.hdrLuminanceKey = 1.1F;
        race.environment.hdrBrightThreshold = 1.3F;
        race.environment.hdrGaussianScalar = 30.0F;
        race.environment.hdrExposure = 15.0F;
    }
    else if (race.levelPath.find("World6") != std::string::npos)
    {
        race.environment.surface = EnvironmentSurface::GroundFog;
        race.environment.surfaceHeight = 3.0F;
        race.environment.surfaceScroll = 0.02F;
        race.environment.hdrLuminanceKey = 1.7F;
        race.environment.hdrBrightThreshold = 1.9F;
        race.environment.hdrGaussianScalar = 30.0F;
        race.environment.hdrExposure = 8.0F;
    }
    Weather weather = Weather::Fair;
    if (race.levelPath.find("World2") != std::string::npos ||
        race.levelPath.find("World6") != std::string::npos)
        weather = Weather::Cloudy;
    else if (race.levelPath.find("World3") != std::string::npos)
        weather = Weather::Sahara;
    else if (race.levelPath.find("World4") != std::string::npos)
        weather = Weather::Hell;
    else if (race.levelPath.find("World5") != std::string::npos)
        weather = Weather::Snow;
    applyWeatherDescription(resources, race, weather);
}

void applyPlanetEnvironment(
    const resource::ResourceFileSystem& resources,
    TiXmlElement* planet, Race& race)
{
    auto* weatherItems = child(planet, "wheaters");
    TiXmlElement* selected = nullptr;
    float maximumChance = -1.0F;
    if (weatherItems != nullptr)
    {
        for (auto* item = weatherItems->FirstChildElement();
             item != nullptr; item = item->NextSiblingElement())
        {
            const float chance = optionalScalar(item, "chance", 0.0F);
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
        race.trackInstances.push_back(
            {entry->second, elementTransform(item, race.levelPath)});
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
        race.decorationInstances.push_back(
            {entry->second, elementTransform(item, race.levelPath)});
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
        bonus.transform = elementTransform(item, race.levelPath);
        const std::string name = basename(bonus.record);
        if (name == "money")
            bonus.kind = BonusKind::Money;
        else if (name == "medpack")
            bonus.kind = BonusKind::Medpack;
        else if (name == "ammo")
            bonus.kind = BonusKind::Ammunition;
        else if (name.rfind("mine", 0) == 0)
            bonus.kind = BonusKind::Mine;
        else if (name == "shield")
            bonus.kind = BonusKind::Shield;
        else if (name == "speedArrow")
            bonus.kind = BonusKind::Speed;
        bonus.value = scalar(bonusRecord, "proj/damage", "db.xml/bonus");
        race.bonuses.push_back(std::move(bonus));
    }

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

Race loadFirstOriginalRace(const resource::ResourceFileSystem& resources)
{
    auto tournamentDocument = parseXml(resources, "tournamet.xml");
    auto databaseDocument = parseXml(resources, "db.xml");
    auto garageDocument = parseXml(resources, "garage.xml");
    auto workshopDocument = parseXml(resources, "workshop.xml");
    auto* tournament = tournamentDocument.RootElement();
    auto* database = databaseDocument.RootElement();

    Race race;
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
    loadRewards(firstPlanet, race);
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
        race.decorationInstances.push_back(
            {entry->second, elementTransform(item, race.levelPath)});
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
        bonus.transform = elementTransform(item, race.levelPath);
        const std::string name = basename(bonus.record);
        if (name == "money")
            bonus.kind = BonusKind::Money;
        else if (name == "medpack")
            bonus.kind = BonusKind::Medpack;
        else if (name == "ammo")
            bonus.kind = BonusKind::Ammunition;
        else if (name.rfind("mine", 0) == 0)
            bonus.kind = BonusKind::Mine;
        else if (name == "shield")
            bonus.kind = BonusKind::Shield;
        else if (name == "speedArrow")
            bonus.kind = BonusKind::Speed;
        bonus.value = scalar(bonusRecord, "proj/damage", "db.xml/bonus");
        race.bonuses.push_back(std::move(bonus));
    }

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
    auto* path = require(map, "trace/pathes/path0", race.levelPath);
    for (auto* node = path->FirstChildElement(); node != nullptr;
         node = node->NextSiblingElement())
    {
        if (node->GetText() != nullptr)
            race.tracePath.push_back(unsignedValue(node->GetText(),
                                                   race.levelPath));
    }
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

    auto* planets = require(tournament, "planets", "tournamet.xml");
    std::uint32_t planetIndex = 0;
    for (auto* planet = planets->FirstChildElement(); planet != nullptr;
         planet = planet->NextSiblingElement(), ++planetIndex)
    {
        const std::string worldType =
            text(planet, "worldType", "tournamet.xml");
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
    const auto humanVehicle = vehicleIndices.find(carRecord);
    if (humanVehicle == vehicleIndices.end())
        throw resource::ResourceError(
            "garage.xml: tournament player car is missing");
    race.vehicle = race.vehicles[humanVehicle->second];
    selectRacers(race, resources, firstPlanet, 1U, carRecord);
    if (race.trackInstances.empty() || race.tracePath.size() < 2 ||
        race.vehicle.record.find(carName) == std::string::npos ||
        race.vehicles.size() != 17 || race.racers.size() < 2)
        throw resource::ResourceError(race.levelPath +
                                      ": incomplete original race data");
    return race;
}

Race loadOriginalRace(const resource::ResourceFileSystem& resources,
                      std::size_t trackIndex,
                      std::string_view playerCar)
{
    Race result = loadFirstOriginalRace(resources);
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
    loadRewards(selectedPlanet, result);
    applyPlanetEnvironment(resources, selectedPlanet, result);
    selectRacers(result, resources, selectedPlanet,
                 result.trackCatalog[trackIndex].racePass,
                 result.vehicle.record);
    return result;
}

namespace
{

std::uint32_t normalizedRacePass(const Race& race,
                                 std::uint32_t planet,
                                 std::uint32_t pass) noexcept
{
    std::uint32_t maximumPass = 0U;
    for (const auto& track : race.trackCatalog)
    {
        if (track.planetIndex == planet)
            maximumPass = std::max(maximumPass, track.racePass);
    }
    if (maximumPass == 0U)
        return 1U;
    const auto oneBasedPass = std::max(pass, 1U);
    return ((oneBasedPass - 1U) % maximumPass) + 1U;
}

} // namespace

std::size_t resolveOriginalTournamentTrack(
    const Race& race, const PlayerProfile& profile) noexcept
{
    if (race.trackCatalog.empty())
        return 0U;
    const auto wantedPass = normalizedRacePass(
        race, profile.currentPlanet, profile.currentPass);
    std::size_t localTrack = 0U;
    std::size_t firstMatch = race.trackCatalog.size();
    for (std::size_t index = 0; index < race.trackCatalog.size(); ++index)
    {
        const auto& track = race.trackCatalog[index];
        if (track.planetIndex != profile.currentPlanet ||
            track.racePass != wantedPass)
            continue;
        if (firstMatch == race.trackCatalog.size())
            firstMatch = index;
        if (localTrack == profile.currentTrack)
            return index;
        ++localTrack;
    }
    return firstMatch < race.trackCatalog.size() ? firstMatch : 0U;
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

TournamentAdvance completeOriginalTournamentTrack(
    const Race& race, std::size_t trackIndex,
    ProfileState& profile) noexcept
{
    TournamentAdvance result;
    if (trackIndex >= race.trackCatalog.size())
        return result;
    const auto& selected = race.trackCatalog[trackIndex];
    const auto planet = selected.planetIndex;
    const auto pass = selected.racePass;

    for (std::size_t index = trackIndex + 1U;
         index < race.trackCatalog.size(); ++index)
    {
        const auto& candidate = race.trackCatalog[index];
        if (candidate.planetIndex == planet &&
            candidate.racePass == pass)
        {
            result.trackIndex = index;
            writeOriginalTournamentSelection(
                race, result.trackIndex, profile.player);
            return result;
        }
    }

    result.passComplete = true;
    const auto required =
        pass > 0U && pass <= race.requiredPoints.size()
            ? race.requiredPoints[pass - 1U]
            : 0U;
    if (required > 0U && profile.player.points >= required)
    {
        result.passChampion = true;
        const auto nextPass = pass + 1U;
        profile.player.currentPass = nextPass;
        if (planet < profile.player.planets.size())
        {
            auto& progress = profile.player.planets[planet];
            progress.pass = nextPass;
            if (nextPass >= 3U)
            {
                progress.state = 3U;
                result.planetChampion = true;
                if (std::find(profile.planetsCompleted.begin(),
                              profile.planetsCompleted.end(),
                              planet) ==
                    profile.planetsCompleted.end())
                {
                    profile.planetsCompleted.push_back(planet);
                }
            }
        }
    }
    profile.player.points = 0U;
    result.trackIndex =
        resolveOriginalTournamentTrack(race, profile.player);
    writeOriginalTournamentSelection(
        race, result.trackIndex, profile.player);
    if (result.planetChampion)
    {
        profile.player.currentPass = pass + 1U;
        if (planet < profile.player.planets.size())
            profile.player.planets[planet].pass = pass + 1U;
    }
    return result;
}

void applyOriginalPlayerProfile(
    Race& race, const resource::ResourceFileSystem& resources,
    const PlayerProfile& profile)
{
    if (race.racers.empty() ||
        race.racers.front().vehicle >= race.vehicles.size())
        return;
    auto workshopDocument = parseXml(resources, "workshop.xml");
    auto* workshop = require(workshopDocument.RootElement(), "workshop",
                             "workshop.xml");
    auto& human = race.racers.front();
    human.color = profile.color;
    auto tournamentDocument = parseXml(resources, "tournamet.xml");
    if (auto* gamers =
            child(tournamentDocument.RootElement(), "gamers"))
    {
        TiXmlElement* fallback = nullptr;
        bool matchedPlayer = false;
        for (auto* gamer = gamers->FirstChildElement(); gamer != nullptr;
             gamer = gamer->NextSiblingElement())
        {
            auto* players = child(gamer, "players");
            if (players == nullptr)
                continue;
            for (auto* player = players->FirstChildElement();
                 player != nullptr;
                 player = player->NextSiblingElement())
            {
                if (fallback == nullptr)
                    fallback = player;
                const auto id = unsignedValue(
                    text(player, "id", "tournamet.xml/gamer"),
                    "tournamet.xml/gamer/id");
                if (id != profile.gamerId && id != profile.playerId)
                    continue;
                fallback = player;
                matchedPlayer = true;
                break;
            }
            if (matchedPlayer)
                break;
        }
        if (fallback != nullptr)
        {
            human.name =
                text(fallback, "name", "tournamet.xml/gamer");
            if (auto* photo = child(fallback, "photo");
                photo != nullptr &&
                photo->Attribute("item") != nullptr)
            {
                human.photoPath = canonicalDataPath(
                    resources, photo->Attribute("item"));
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
    applyMobilityLoadout(human.configuredVehicle, resources, workshop,
                         human.loadout, profile.difficulty, true);
    race.vehicle = human.configuredVehicle;

    for (std::size_t index = 1; index < race.racers.size(); ++index)
    {
        auto& racer = race.racers[index];
        racer.configuredVehicle = race.vehicles[racer.vehicle];
        racer.hasConfiguredVehicle = true;
        applyMobilityLoadout(racer.configuredVehicle, resources, workshop,
                             racer.loadout, profile.difficulty, true);
    }
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

    auto appendCollision = [&](const ObjectDefinition& definition,
                               const ObjectInstance& instance,
                               bool track) {
        for (const auto& shape : definition.collisionShapes)
        {
            const auto mesh = resource::loadR3DMeshAsset(resources,
                                                         shape.meshPath);
            r3d::physics::TriangleMesh collision;
            collision.transform = instance.transform;
            collision.surface =
                track && shape.materialGroup == 1U
                    ? r3d::physics::CollisionSurface::TrackBorder
                    : (track
                           ? r3d::physics::CollisionSurface::TrackPlane
                           : r3d::physics::CollisionSurface::Decoration);
            collision.vertices.reserve(mesh.vertices.size());
            for (const auto& vertex : mesh.vertices)
                collision.vertices.push_back({vertex.position[0],
                                              vertex.position[1],
                                              vertex.position[2]});
            if (shape.materialGroup < mesh.materialGroups.size())
            {
                const auto& group = mesh.materialGroups[shape.materialGroup];
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
                result.collisionMeshes.push_back(std::move(collision));
        }
    };
    for (const auto& instance : race.trackInstances)
    {
        appendCollision(race.trackDefinitions.at(instance.definition),
                        instance, true);
    }
    for (const auto& instance : race.decorationInstances)
    {
        appendCollision(
            race.decorationDefinitions.at(instance.definition), instance,
            false);
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
    const Vec3 lineDirection{-result.startDirection.y,
                             result.startDirection.x, 0.0F};
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
        const auto physics = makePhysicsDescription(race, resources);
        std::size_t triangleCount = 0;
        std::size_t borderMeshCount = 0;
        for (const auto& mesh : physics.collisionMeshes)
        {
            triangleCount += mesh.indices.size() / 3U;
            if (mesh.surface ==
                r3d::physics::CollisionSurface::TrackBorder)
                ++borderMeshCount;
        }
        const auto near = [](float first, float second) {
            return std::abs(first - second) <= 0.0001F;
        };
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
        if (race.levelPath != "Data/Map/World1/map1.r3dMap" ||
            race.lapCount != 4 || race.vehicle.record.find("marauder") ==
                                      std::string::npos ||
            race.trackDefinitions.size() != 4 ||
            race.trackInstances.size() != 52 || race.tracePoints.size() != 5 ||
            race.tracePath.size() != 6 ||
            race.decorationInstances.size() != 234 ||
            race.bonuses.size() != 7 ||
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
            !near(physics.vehicle.brakeTorque, 7500.0F) ||
            !near(physics.vehicle.differentialRatio, 3.42F) ||
            !near(physics.vehicle.maximumRpm, 7000.0F) ||
            // Original defaults install truba1 + engine1: 100 + 900.
            !near(physics.vehicle.maximumTorque, 1000.0F) ||
            !near(race.vehicle.maximumLife, 70.0F) ||
            physics.vehicle.wheels.size() != 4 ||
            !near(physics.vehicle.wheels[0].radius, 0.42947F) ||
            !near(physics.vehicle.wheels[0].suspensionTravel, 0.3F) ||
            // wheel1 adds 2 to the 140000 base tire spring.
            !near(physics.vehicle.wheels[0].spring, 140002.0F) ||
            !near(physics.vehicle.wheels[0].damper, 1000.0F) ||
            !physics.vehicle.wheels[0].driven ||
            !physics.vehicle.wheels[0].steering ||
            physics.vehicle.wheels[2].driven ||
            physics.vehicle.wheels[2].steering ||
            !near(race.vehicle.bodyVisualTransform.scale.z, 0.95F) ||
            race.vehicle.wheelVisuals.size() != 4 ||
            race.vehicle.wheelVisualTransforms.size() != 4 ||
            race.vehicle.wheelVisualOffsets.size() != 4 ||
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
            error = "original tournament/map/db/garage provenance mismatch";
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
