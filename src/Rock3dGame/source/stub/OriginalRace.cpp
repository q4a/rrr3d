#include "OriginalRace.h"

#include "resource/R3DMeshAsset.h"
#include "resource/ResourceFileSystem.h"

#include <tinyxml.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
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

void selectRacers(Race& race, TiXmlElement* planet,
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
    race.racers.push_back({"Human", humanVehicle->second, true});
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
        race.racers.push_back(
            {text(opponent, "name", "tournamet.xml/player"),
             vehicle->second, false});
    }
    if (race.racers.size() < 2)
        throw resource::ResourceError(
            "tournamet.xml: selected race has no AI opponents");
}

MaterialDefinition materialDefinition(
    const resource::ResourceFileSystem& resources, std::string_view legacy)
{
    const std::string record(legacy);
    struct Mapping
    {
        std::string_view record;
        std::string_view texture;
        MaterialBlend blend;
    };
    // These are the exact ResourceManager::LoadWorld1/2/3 and LoadCrush
    // mappings used by map1. Several meshes deliberately share an atlas.
    static constexpr Mapping mappings[] = {
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
        {"World3\\stone", "Data/World3/Texture/stone.dds",
         MaterialBlend::Opaque},
        {"World3\\naves", "Data/World3/Track/Texture/track1.dds",
         MaterialBlend::Opaque},
        {"World3\\Track\\track",
         "Data/World3/Track/Texture/track1.dds",
         MaterialBlend::Opaque},
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
        {"Bonus\\money", "Data/Bonus/money.dds", MaterialBlend::AlphaTest},
        {"Bonus\\medpack", "Data/Bonus/medpack.dds",
         MaterialBlend::AlphaTest},
        {"Bonus\\ammo", "Data/Bonus/ammo.dds", MaterialBlend::AlphaTest},
        {"Bonus\\mineSpike", "Data/Bonus/mineSpike.dds",
         MaterialBlend::AlphaTest},
        {"Bonus\\shield", "Data/Bonus/shield.dds",
         MaterialBlend::Transparency},
        {"Bonus\\speedArrow", "Data/Bonus/speedArrow.dds",
         MaterialBlend::AlphaTest},
    };
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
            return {record, texture, mapping.blend,
                    mapping.blend == MaterialBlend::AlphaTest ? 0.1F : 0.0F};
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
            return MaterialDefinition{record, path, MaterialBlend::Opaque,
                                      0.0F};
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
            return {record, candidate,
                    transparent ? MaterialBlend::AlphaTest
                                : MaterialBlend::Opaque,
                    transparent ? 0.1F : 0.0F};
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
    for (auto* item = items->FirstChildElement(); item != nullptr;
         item = item->NextSiblingElement())
    {
        auto* mesh = child(item, "mesh");
        if (mesh == nullptr || mesh->Attribute("item") == nullptr)
            continue;
        VisualNode node;
        node.meshPath =
            canonicalDataPath(resources, mesh->Attribute("item"));
        node.transform = elementTransform(item, source);
        if (auto* meshId = child(item, "meshId");
            meshId != nullptr && meshId->GetText() != nullptr)
        {
            std::istringstream stream(meshId->GetText());
            stream >> node.subMesh;
        }
        if (!textureOverride.empty())
        {
            node.materials.push_back(
                {"", std::string(textureOverride), MaterialBlend::Opaque,
                 0.0F});
        }
        else if (auto* materials = child(item, "materials"))
        {
            for (auto* material = materials->FirstChildElement();
                 material != nullptr;
                 material = material->NextSiblingElement())
            {
                const char* itemName = material->Attribute("item");
                if (itemName != nullptr && *itemName != '\0')
                    node.materials.push_back(
                        materialDefinition(resources, itemName));
            }
        }
        if (node.materials.empty())
            throw resource::ResourceError(std::string(source) +
                                          ": visual node has no material");
        result.push_back(std::move(node));
    }
    return result;
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
    result.visualNodes = visualNodes(resources, dbRecord, source);
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
    auto* path = require(map, "trace/pathes/path0", race.levelPath);
    for (auto* node = path->FirstChildElement(); node != nullptr;
         node = node->NextSiblingElement())
    {
        if (node->GetText() != nullptr)
            race.tracePath.push_back(
                unsignedValue(node->GetText(), race.levelPath));
    }
    race.environment.sunPosition = vector3(map, "sunPos", race.levelPath);
    race.environment.sunRotation = quaternion(map, "sunRot", race.levelPath);
    race.environment.weather = Weather::Fair;
    if (race.levelPath.find("World1") != std::string::npos)
        race.environment.skyTexturePath =
            "Data/World1/Texture/skyTex1.dds";
    else if (race.levelPath.find("World2") != std::string::npos)
        race.environment.skyTexturePath =
            "Data/World2/texture/skyTex1.dds";
    else if (race.levelPath.find("World3") != std::string::npos)
        race.environment.skyTexturePath =
            "Data/World3/Texture/skyTex1.dds";
    else if (race.levelPath.find("World4") != std::string::npos)
        race.environment.skyTexturePath =
            "Data/World4/Texture/skyTex1.dds";
    else if (race.levelPath.find("World5") != std::string::npos)
        race.environment.skyTexturePath =
            "Data/World5/Texture/sky_text.dds";
    else
        race.environment.skyTexturePath =
            "Data/World1/Texture/skyTex1.dds";
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
    auto* tournament = tournamentDocument.RootElement();
    auto* database = databaseDocument.RootElement();

    Race race;
    auto* firstPlanet = require(tournament, "planets/planet0", "tournamet.xml");
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
    race.environment.weather = Weather::Fair;
    race.environment.skyTexturePath =
        "Data/World1/Texture/skyTex1.dds";

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
    selectRacers(race, firstPlanet, 1U, carRecord);
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
    selectRacers(result, selectedPlanet,
                 result.trackCatalog[trackIndex].racePass,
                 result.vehicle.record);
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

    auto appendCollision = [&](const ObjectDefinition& definition,
                               const ObjectInstance& instance) {
        for (const auto& shape : definition.collisionShapes)
        {
            const auto mesh = resource::loadR3DMeshAsset(resources,
                                                         shape.meshPath);
            r3d::physics::TriangleMesh collision;
            collision.transform = instance.transform;
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
                        instance);
    }
    for (const auto& instance : race.decorationInstances)
    {
        appendCollision(
            race.decorationDefinitions.at(instance.definition), instance);
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
            result.spawns.push_back(
                {prepareVehicle(race.vehicles.at(racer.vehicle)),
                 position, result.startDirection});
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
        for (const auto& mesh : physics.collisionMeshes)
            triangleCount += mesh.indices.size() / 3U;
        const auto near = [](float first, float second) {
            return std::abs(first - second) <= 0.0001F;
        };
        if (race.levelPath != "Data/Map/World1/map1.r3dMap" ||
            race.lapCount != 4 || race.vehicle.record.find("marauder") ==
                                      std::string::npos ||
            race.trackDefinitions.size() != 4 ||
            race.trackInstances.size() != 52 || race.tracePoints.size() != 5 ||
            race.tracePath.size() != 6 ||
            race.decorationInstances.size() != 234 ||
            race.bonuses.size() != 7 ||
            race.trackCatalog.size() != 88 ||
            physics.collisionMeshes.empty() || triangleCount < 591 ||
            !near(physics.vehicle.mass, 2000.0F) ||
            !near(physics.vehicle.shapePosition.x, 0.0654583F) ||
            !near(physics.vehicle.centerOfMass.z, -0.75F) ||
            !near(physics.vehicle.brakeTorque, 7500.0F) ||
            !near(physics.vehicle.differentialRatio, 3.42F) ||
            !near(physics.vehicle.maximumRpm, 7000.0F) ||
            !near(physics.vehicle.maximumTorque, 2000.0F) ||
            physics.vehicle.wheels.size() != 4 ||
            !near(physics.vehicle.wheels[0].radius, 0.42947F) ||
            !near(physics.vehicle.wheels[0].suspensionTravel, 0.3F) ||
            !near(physics.vehicle.wheels[0].spring, 140000.0F) ||
            !near(physics.vehicle.wheels[0].damper, 1000.0F) ||
            !physics.vehicle.wheels[0].driven ||
            !physics.vehicle.wheels[0].steering ||
            physics.vehicle.wheels[2].driven ||
            physics.vehicle.wheels[2].steering ||
            !near(race.vehicle.bodyVisualTransform.scale.z, 0.95F) ||
            race.vehicle.wheelVisualTransforms.size() != 4 ||
            race.vehicle.wheelVisualOffsets.size() != 4 ||
            !near(race.vehicle.wheelVisualTransforms[1].scale.y, -1.0F) ||
            !near(race.vehicle.wheelVisualOffsets[0].x, 0.05F))
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
