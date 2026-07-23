#include "OriginalRace.h"

#include "resource/R3DMeshAsset.h"
#include "resource/ResourceFileSystem.h"

#include <tinyxml.h>

#include <algorithm>
#include <cmath>
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

std::string trackTexture(std::string_view material)
{
    // ResourceManager::LoadWorld1 maps every first-world track material used
    // by map1 to the shipped track atlas below.
    if (material.rfind("World1\\Track\\", 0) == 0)
        return "Data/World1/Track/Texture/track1.dds";
    throw resource::ResourceError("No original texture mapping for " +
                                  std::string(material));
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
    race.levelPath = dataPath(text(firstTrack, "level", "tournamet.xml"));
    race.lapCount = unsignedValue(text(firstTrack, "numLaps", "tournamet.xml"),
                                  "tournamet.xml/numLaps");

    const std::string carRecord =
        text(firstPlanet, "cars/car0/record", "tournamet.xml");
    const std::string carName = basename(carRecord);
    auto* car = databaseRecord(database, carRecord);
    race.vehicle.record = carRecord;
    auto* bodyVisual = require(
        car, "grActor/nodes/items/item0", "db.xml/marauder");
    race.vehicle.bodyMeshPath = dataPath(itemAttribute(
        bodyVisual, "mesh", "db.xml/marauder"));
    race.vehicle.bodyVisualTransform.position = vector3(
        bodyVisual, "pos", "db.xml/marauder/body visual");
    race.vehicle.bodyVisualTransform.scale = vector3(
        bodyVisual, "scale", "db.xml/marauder/body visual");
    race.vehicle.bodyVisualTransform.rotation = quaternion(
        bodyVisual, "rot", "db.xml/marauder/body visual");
    race.vehicle.wheelMeshPath = dataPath(itemAttribute(
        require(garageDocument.RootElement(), "cars/car0", "garage.xml"),
        "wheel", "garage.xml/car0"));
    race.vehicle.texturePath = dataPath(itemAttribute(
        require(garageDocument.RootElement(), "cars/car0", "garage.xml"),
        "bodyMeshes/bodyMesh0/texture", "garage.xml/car0"));
    race.vehicle.idleSoundPath = dataPath(itemAttribute(
        car, "behaviors/items/item5/sndIdle", "db.xml/marauder"));
    race.vehicle.rpmSoundPath = dataPath(itemAttribute(
        car, "behaviors/items/item5/sndRPM", "db.xml/marauder"));

    auto& vehicle = race.vehicle.physics;
    vehicle.mass = scalar(car, "pxActor/body/mass", "db.xml/marauder");
    vehicle.halfExtents = vector3(
        car, "pxActor/shapes/items/item0/dimensions", "db.xml/marauder");
    vehicle.shapePosition = vector3(
        car, "pxActor/shapes/items/item0/pos", "db.xml/marauder");
    {
        const std::string pose = text(
            car, "pxActor/body/massLocalPose", "db.xml/marauder");
        std::istringstream stream(pose);
        float ignored = 0.0F;
        for (int index = 0; index < 9; ++index)
            if (!(stream >> ignored))
                throw resource::ResourceError(
                    "db.xml/marauder: invalid massLocalPose");
        if (!(stream >> vehicle.centerOfMass.x >> vehicle.centerOfMass.y >>
              vehicle.centerOfMass.z))
            throw resource::ResourceError(
                "db.xml/marauder: invalid massLocalPose translation");
    }
    vehicle.brakeTorque = scalar(car, "motor/brakeTorque", "db.xml/marauder");
    vehicle.differentialRatio = scalar(car, "motor/gearDiff", "db.xml/marauder");
    vehicle.maximumRpm = scalar(car, "motor/maxRPM", "db.xml/marauder");
    vehicle.maximumTorque = scalar(car, "motor/maxTorque", "db.xml/marauder");
    // GameCar::cMaxSteerAngle is the legacy wheel limit. steerRot controls
    // yaw assistance and is not a wheel angle.
    vehicle.steerAngle = 3.14159265358979323846F / 6.0F;

    auto* wheels = require(car, "wheels/items", "db.xml/marauder");
    for (auto* item = wheels->FirstChildElement(); item != nullptr;
         item = item->NextSiblingElement())
    {
        r3d::physics::WheelDescription wheel;
        wheel.position = vector3(item, "pos", "db.xml/marauder/wheel");
        auto* shape = require(item, "pxActor/shapes/items/item0",
                              "db.xml/marauder/wheel");
        wheel.radius = scalar(shape, "radius", "db.xml/marauder/wheel");
        wheel.suspensionTravel = scalar(
            shape, "suspensionTravel", "db.xml/marauder/wheel");
        wheel.spring = scalar(
            shape, "suspension/spring", "db.xml/marauder/wheel");
        wheel.damper = scalar(
            shape, "suspension/damper", "db.xml/marauder/wheel");
        wheel.inverseMass = scalar(
            shape, "inverseWheelMass", "db.xml/marauder/wheel");
        wheel.driven = boolean(item, "lead", "db.xml/marauder/wheel");
        wheel.steering = boolean(item, "steer", "db.xml/marauder/wheel");
        vehicle.wheels.push_back(wheel);
        auto* wheelVisual = require(
            item, "grActor/nodes/items/item0", "db.xml/marauder/wheel");
        Transform visual;
        visual.position = vector3(
            wheelVisual, "pos", "db.xml/marauder/wheel visual");
        visual.scale = vector3(
            wheelVisual, "scale", "db.xml/marauder/wheel visual");
        visual.rotation = quaternion(
            wheelVisual, "rot", "db.xml/marauder/wheel visual");
        race.vehicle.wheelVisualTransforms.push_back(visual);
        race.vehicle.wheelVisualOffsets.push_back(
            vector3(item, "offset", "db.xml/marauder/wheel"));
    }
    if (vehicle.wheels.size() != 4)
        throw resource::ResourceError("db.xml/marauder: expected four wheels");

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
            ObjectDefinition definition;
            definition.record = record;
            auto* dbRecord = databaseRecord(database, record);
            auto* meshNode = require(dbRecord, "grActor/nodes/items/item0",
                                     "db.xml/track");
            definition.visualMeshPath = dataPath(
                itemAttribute(meshNode, "mesh", "db.xml/track"));
            definition.visualTransform.position = vector3(
                meshNode, "pos", "db.xml/track visual");
            definition.visualTransform.scale = vector3(
                meshNode, "scale", "db.xml/track visual");
            definition.visualTransform.rotation = quaternion(
                meshNode, "rot", "db.xml/track visual");
            auto* material = require(meshNode, "materials/material_0",
                                     "db.xml/track");
            definition.texturePath = trackTexture(
                material->Attribute("item") == nullptr
                    ? ""
                    : material->Attribute("item"));
            auto* shapes = require(dbRecord, "pxActor/shapes/items",
                                   "db.xml/track");
            for (auto* shape = shapes->FirstChildElement(); shape != nullptr;
                 shape = shape->NextSiblingElement())
            {
                auto* mesh = child(shape, "mesh");
                auto* meshId = child(shape, "meshId");
                if (mesh == nullptr || mesh->Attribute("item") == nullptr ||
                    meshId == nullptr || meshId->GetText() == nullptr)
                    continue;
                definition.collisionShapes.push_back(
                    {dataPath(mesh->Attribute("item")),
                     unsignedValue(meshId->GetText(), "db.xml/meshId")});
            }
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
    if (race.trackInstances.empty() || race.tracePath.size() < 2 ||
        race.vehicle.record.find(carName) == std::string::npos)
        throw resource::ResourceError(race.levelPath +
                                      ": incomplete original race data");
    return race;
}

r3d::physics::WorldDescription makePhysicsDescription(
    const Race& race, const resource::ResourceFileSystem& resources)
{
    r3d::physics::WorldDescription result;
    result.vehicle = race.vehicle.physics;
    const auto wheelMesh = resource::loadR3DMeshAsset(
        resources, race.vehicle.wheelMeshPath);
    const float wheelWidth =
        std::max(wheelMesh.maximum[1] - wheelMesh.minimum[1], 0.05F);
    for (auto& wheel : result.vehicle.wheels)
        wheel.width = wheelWidth;

    for (const auto& instance : race.trackInstances)
    {
        const auto& definition = race.trackDefinitions.at(instance.definition);
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
            race.tracePath.size() != 6 || physics.collisionMeshes.empty() ||
            triangleCount != 591 || !near(physics.vehicle.mass, 2000.0F) ||
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
