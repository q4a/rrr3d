#include "OriginalGarage.h"

#include "resource/ResourceFileSystem.h"

#include <tinyxml.h>

#include <algorithm>
#include <array>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace r3d::game::originalrace
{
namespace
{

constexpr std::array<std::string_view,
                     static_cast<std::size_t>(GarageSlotType::Count)>
    placementNames{
        "stWheel", "stTruba", "stArmor", "stMotor", "stHyper",
        "stMine", "stWeapon1", "stWeapon2", "stWeapon3",
        "stWeapon4"};

TiXmlDocument parseXml(const resource::ResourceFileSystem& resources,
                       std::string_view path)
{
    TiXmlDocument document;
    const auto source = resources.readText(path);
    document.Parse(source.c_str(), nullptr, TIXML_ENCODING_UTF8);
    if (document.Error() || document.RootElement() == nullptr)
        throw resource::ResourceError(std::string(path) + ": " +
                                      document.ErrorDesc());
    return document;
}

TiXmlElement* child(TiXmlElement* parent, const char* name)
{
    return parent == nullptr ? nullptr : parent->FirstChildElement(name);
}

std::string text(TiXmlElement* parent, const char* name,
                 std::string fallback = {})
{
    auto* element = child(parent, name);
    if (element == nullptr || element->GetText() == nullptr)
        return fallback;
    return element->GetText();
}

bool boolean(TiXmlElement* parent, const char* name, bool fallback)
{
    const auto value = text(parent, name);
    if (value == "true" || value == "1")
        return true;
    if (value == "false" || value == "0")
        return false;
    return fallback;
}

std::uint32_t number(TiXmlElement* parent, const char* name,
                     std::uint32_t fallback = 0)
{
    const auto value = text(parent, name);
    if (value.empty())
        return fallback;
    try
    {
        const auto parsed = std::stoul(value);
        if (parsed > std::numeric_limits<std::uint32_t>::max())
            return fallback;
        return static_cast<std::uint32_t>(parsed);
    }
    catch (const std::exception&)
    {
        return fallback;
    }
}

float real(TiXmlElement* parent, const char* name,
           float fallback = 0.0F)
{
    const auto value = text(parent, name);
    if (value.empty())
        return fallback;
    try
    {
        return std::stof(value);
    }
    catch (const std::exception&)
    {
        return fallback;
    }
}

template <std::size_t Count>
std::array<float, Count> realVector(
    TiXmlElement* parent, const char* name,
    std::array<float, Count> fallback = {})
{
    const auto value = text(parent, name);
    if (value.empty())
        return fallback;
    std::istringstream stream(value);
    std::array<float, Count> result{};
    for (float& component : result)
    {
        if (!(stream >> component))
            return fallback;
    }
    return result;
}

std::string resourcePath(TiXmlElement* parent, const char* name)
{
    auto* element = child(parent, name);
    if (element == nullptr)
        return {};
    const char* item = element->Attribute("item");
    if (item == nullptr || *item == '\0')
        return {};
    std::string result = "Data/";
    result += item;
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

std::string leaf(std::string_view record)
{
    const auto slash = record.find_last_of("\\/");
    return std::string(slash == std::string_view::npos
                           ? record
                           : record.substr(slash + 1U));
}

std::string workshopRecord(std::string_view name)
{
    return "world\\race\\workshopRoot\\workshop\\" +
           std::string(name);
}

bool contains(const std::vector<std::string>& records,
              std::string_view record)
{
    return std::find(records.begin(), records.end(), record) !=
           records.end();
}

bool unlockRuleSatisfied(const OriginalUnlockRule& rule,
                         const ProfileState& profile) noexcept
{
    if (rule.planet >= profile.player.planets.size())
        return false;
    const auto& progress = profile.player.planets[rule.planet];
    // Planet::SetPass completes the previous pass before changing _pass.
    // psUnavailable is serialized as 2 and does not complete pass zero.
    return progress.state != 2U && progress.pass > rule.pass;
}

bool unlockedByRules(const std::vector<OriginalUnlockRule>& rules,
                     const ProfileState& profile,
                     std::string_view record) noexcept
{
    return std::any_of(
        rules.begin(), rules.end(),
        [&](const auto& rule) {
            return rule.record == record &&
                   unlockRuleSatisfied(rule, profile);
        });
}

bool hasRule(const std::vector<OriginalUnlockRule>& rules,
             std::string_view record) noexcept
{
    return std::any_of(
        rules.begin(), rules.end(),
        [&](const auto& rule) { return rule.record == record; });
}

int upgradeLevel(std::string_view record, GarageSlotType slot)
{
    const auto name = leaf(record);
    std::string_view prefix;
    switch (slot)
    {
    case GarageSlotType::Wheel:
        prefix = name.rfind("gusWheel", 0) == 0 ? "gusWheel" : "wheel";
        break;
    case GarageSlotType::Exhaust:
        prefix = "truba";
        break;
    case GarageSlotType::Armor:
        prefix = "armor";
        break;
    case GarageSlotType::Engine:
        prefix = "engine";
        break;
    default:
        return -1;
    }
    if (name.size() != prefix.size() + 1U ||
        name.substr(0, prefix.size()) != prefix ||
        name.back() < '1' || name.back() > '3')
        return -1;
    return name.back() - '1';
}

ProfileSlot defaultProfileSlot(const OriginalGarageCatalog& catalog,
                               std::string_view record,
                               bool maximumCharge)
{
    ProfileSlot result;
    result.record = std::string(record);
    if (const auto* item = catalog.findItem(record);
        item != nullptr && item->maximumCharge > 0U)
    {
        result.hasCharge = true;
        result.charge = maximumCharge ? item->maximumCharge
                                     : item->defaultCharge;
    }
    return result;
}

void applyUpgradeLevel(const OriginalGarageCatalog& catalog,
                       const OriginalGarageCar& car,
                       PlayerProfile& player, std::uint32_t level,
                       bool maximumCharge)
{
    const auto clamped = std::min(level, 2U);
    for (std::size_t index = 0; index < 4U; ++index)
    {
        const auto type = static_cast<GarageSlotType>(index);
        const auto& placement = car.placements[index];
        const auto found = std::find_if(
            placement.supportedItems.begin(),
            placement.supportedItems.end(),
            [&](const auto& record) {
                return upgradeLevel(record, type) ==
                       static_cast<int>(clamped);
            });
        if (found != placement.supportedItems.end())
            player.slots[index] =
                defaultProfileSlot(catalog, *found, maximumCharge);
    }
}

void loadGarageCars(TiXmlElement* root, OriginalGarageCatalog& catalog)
{
    auto* cars = child(root, "cars");
    if (cars == nullptr)
        throw resource::ResourceError("garage.xml: missing cars");
    for (auto* node = cars->FirstChildElement(); node != nullptr;
         node = node->NextSiblingElement())
    {
        OriginalGarageCar car;
        car.record = text(node, "record");
        if (car.record.empty())
            continue;
        car.name = text(node, "name", leaf(car.record));
        car.info = text(node, "desc");
        car.cost = number(node, "cost");
        car.initialUpgradeSet = number(node, "initialUpgradeSet");
        for (std::size_t index = 0; index < car.placements.size();
             ++index)
        {
            auto* placementNode =
                child(node, std::string(placementNames[index]).c_str());
            if (placementNode == nullptr)
                continue;
            auto& placement = car.placements[index];
            placement.active =
                boolean(placementNode, "active", false);
            placement.visible =
                boolean(placementNode, "show", false);
            placement.locked =
                boolean(placementNode, "lock", false);
            placement.defaultItem =
                text(placementNode, "defItem");
            if (auto* items = child(placementNode, "items"))
            {
                for (auto* item = items->FirstChildElement();
                     item != nullptr; item = item->NextSiblingElement())
                {
                    const auto record = text(item, "record");
                    if (!record.empty() &&
                        !contains(placement.supportedItems, record))
                        placement.supportedItems.push_back(record);
                }
            }
            if (!placement.defaultItem.empty() &&
                !contains(placement.supportedItems,
                          placement.defaultItem))
                placement.supportedItems.push_back(
                    placement.defaultItem);
        }
        catalog.cars.push_back(std::move(car));
    }
    if (catalog.cars.empty())
        throw resource::ResourceError(
            "garage.xml: no original cars");
}

void loadWorkshop(TiXmlElement* root, OriginalGarageCatalog& catalog)
{
    auto* workshop = child(root, "workshop");
    if (workshop == nullptr)
        throw resource::ResourceError(
            "workshop.xml: missing workshop");
    for (auto* node = workshop->FirstChildElement(); node != nullptr;
         node = node->NextSiblingElement())
    {
        auto* itemNode = child(node, "item");
        if (itemNode == nullptr)
            continue;
        OriginalWorkshopItem item;
        item.record = workshopRecord(node->Value());
        item.type = number(node, "type");
        item.name = text(itemNode, "name", node->Value());
        item.info = text(itemNode, "info");
        item.meshPath = resourcePath(itemNode, "mesh");
        item.texturePath = resourcePath(itemNode, "texture");
        item.visualPosition =
            realVector<3>(itemNode, "pos", item.visualPosition);
        item.visualRotation =
            realVector<4>(itemNode, "rot", item.visualRotation);
        item.cost = number(itemNode, "cost");
        item.maximumCharge = number(itemNode, "maxCharge");
        item.defaultCharge = number(itemNode, "cntCharge");
        item.chargeStep =
            std::max(number(itemNode, "chargeStep", 1U), 1U);
        item.chargeCost = number(itemNode, "chargeCost");
        if (auto* functions = child(itemNode, "carFuncMap"))
        {
            for (auto* function = functions->FirstChildElement();
                 function != nullptr;
                 function = function->NextSiblingElement())
            {
                OriginalWorkshopItem::CarFunction value;
                value.car = text(function, "car");
                value.maximumTorque =
                    real(function, "maxTorque");
                value.life = real(function, "life");
                value.longExtremumValue =
                    real(child(function, "longTire"),
                         "extremumValue");
                value.lateralExtremumValue =
                    real(child(function, "latTire"),
                         "extremumValue");
                if (!value.car.empty())
                    item.carFunctions.push_back(
                        std::move(value));
            }
        }
        if (auto* projectiles = child(itemNode, "projList"))
        {
            for (auto* projectile =
                     projectiles->FirstChildElement();
                 projectile != nullptr;
                 projectile = projectile->NextSiblingElement())
            {
                item.projectileDamage +=
                    real(projectile, "damage");
            }
        }
        catalog.workshop.push_back(std::move(item));
    }
    if (catalog.workshop.empty())
        throw resource::ResourceError(
            "workshop.xml: no original workshop items");
}

void loadUnlocks(TiXmlElement* root, OriginalGarageCatalog& catalog)
{
    auto* planets = child(root, "planets");
    if (planets == nullptr)
        throw resource::ResourceError(
            "tournamet.xml: missing planets");
    auto readPlanet = [&](TiXmlElement* planet) {
        OriginalGaragePlanet planetValue;
        planetValue.record = planet->Value();
        planetValue.name =
            text(planet, "name", planetValue.record);
        planetValue.info = text(planet, "info");
        planetValue.worldType = text(planet, "worldType");
        planetValue.meshPath = resourcePath(planet, "mesh");
        planetValue.texturePath = resourcePath(planet, "texture");
        if (auto* points = child(planet, "points"))
        {
            for (auto* point = points->FirstChildElement();
                 point != nullptr;
                 point = point->NextSiblingElement())
            {
                const auto pass = number(point, "place");
                if (pass == 0U)
                    continue;
                if (planetValue.requestPoints.size() < pass)
                    planetValue.requestPoints.resize(pass, 0U);
                planetValue.requestPoints[pass - 1U] =
                    number(point, "value");
            }
        }
        if (auto* players = child(planet, "players"))
        {
            if (auto* boss = players->FirstChildElement())
            {
                planetValue.bossId = number(boss, "id");
                planetValue.bossName =
                    text(boss, "name", boss->Value());
                planetValue.bossBonus = text(boss, "bonus");
                planetValue.bossPhotoPath =
                    resourcePath(boss, "photo");
                if (auto* cars = child(boss, "cars"))
                {
                    if (auto* car = cars->FirstChildElement())
                    {
                        planetValue.bossCarRecord =
                            text(car, "record");
                    }
                }
            }
        }
        return planetValue;
    };
    std::size_t planetIndex = 0;
    for (auto* planet = planets->FirstChildElement(); planet != nullptr;
         planet = planet->NextSiblingElement(), ++planetIndex)
    {
        catalog.planets.push_back(readPlanet(planet));
        auto readRules = [&](const char* groupName,
                             std::vector<OriginalUnlockRule>& output) {
            auto* group = child(planet, groupName);
            if (group == nullptr)
                return;
            for (auto* entry = group->FirstChildElement();
                 entry != nullptr; entry = entry->NextSiblingElement())
            {
                const auto record = text(entry, "record");
                if (!record.empty())
                    output.push_back(
                        {record, planetIndex,
                         number(entry, "pass")});
            }
        };
        readRules("cars", catalog.carUnlocks);
        readRules("slots", catalog.workshopUnlocks);
    }

    auto* gamers = child(root, "gamers");
    if (gamers == nullptr)
        throw resource::ResourceError(
            "tournamet.xml: missing gamers");
    for (auto* gamer = gamers->FirstChildElement(); gamer != nullptr;
         gamer = gamer->NextSiblingElement())
    {
        catalog.gamers.push_back(readPlanet(gamer));
    }
    if (catalog.gamers.empty())
        throw resource::ResourceError(
            "tournamet.xml: no original gamers");
}

const OriginalWorkshopItem::CarFunction* carFunction(
    const OriginalWorkshopItem* item,
    std::string_view car) noexcept
{
    if (item == nullptr)
        return nullptr;
    const auto found = std::find_if(
        item->carFunctions.begin(), item->carFunctions.end(),
        [&](const auto& value) { return value.car == car; });
    return found == item->carFunctions.end() ? nullptr : &*found;
}

float mobilitySkill(
    const OriginalWorkshopItem::CarFunction* function) noexcept
{
    if (function == nullptr)
        return 0.0F;
    return function->maximumTorque +
           std::max(function->longExtremumValue - 5.0F, 0.0F) *
               300.0F +
           std::max(function->lateralExtremumValue - 1.5F,
                    0.0F) *
               2000.0F;
}

const OriginalWorkshopItem* upgradeItem(
    const OriginalGarageCatalog& catalog,
    const OriginalGarageCar& car, GarageSlotType slot,
    int level) noexcept
{
    if (level < 0 || level > 2)
        return nullptr;
    const auto slotIndex = static_cast<std::size_t>(slot);
    if (slotIndex >= car.placements.size())
        return nullptr;
    const auto& placement = car.placements[slotIndex];
    const auto found = std::find_if(
        placement.supportedItems.begin(),
        placement.supportedItems.end(),
        [&](const auto& record) {
            return upgradeLevel(record, slot) == level;
        });
    return found == placement.supportedItems.end()
               ? nullptr
               : catalog.findItem(*found);
}

float defaultMobilitySkill(
    const OriginalGarageCatalog& catalog,
    const OriginalGarageCar& car, GarageSlotType slot) noexcept
{
    const auto slotIndex = static_cast<std::size_t>(slot);
    if (slotIndex >= car.placements.size())
        return 0.0F;
    const auto* item =
        catalog.findItem(car.placements[slotIndex].defaultItem);
    return mobilitySkill(carFunction(item, car.record));
}

float maximumMobilitySkill(
    const OriginalGarageCatalog& catalog,
    const OriginalGarageCar& car, GarageSlotType slot) noexcept
{
    return mobilitySkill(carFunction(
        upgradeItem(catalog, car, slot, 2), car.record));
}

float defaultWeaponDamage(
    const OriginalGarageCatalog& catalog,
    const OriginalGaragePlacement& placement) noexcept
{
    const auto* item = catalog.findItem(placement.defaultItem);
    return item == nullptr ? 0.0F : item->projectileDamage;
}

float maximumWeaponDamage(
    const OriginalGarageCatalog& catalog,
    const OriginalGaragePlacement& placement) noexcept
{
    float result = 0.0F;
    for (const auto& record : placement.supportedItems)
    {
        if (const auto* item = catalog.findItem(record))
            result = std::max(result, item->projectileDamage);
    }
    return result;
}

} // namespace

const OriginalGarageCar* OriginalGarageCatalog::findCar(
    std::string_view record) const noexcept
{
    const auto found = std::find_if(
        cars.begin(), cars.end(),
        [&](const auto& car) { return car.record == record; });
    return found == cars.end() ? nullptr : &*found;
}

const OriginalWorkshopItem* OriginalGarageCatalog::findItem(
    std::string_view record) const noexcept
{
    const auto found = std::find_if(
        workshop.begin(), workshop.end(),
        [&](const auto& item) { return item.record == record; });
    return found == workshop.end() ? nullptr : &*found;
}

OriginalGarageCatalog loadOriginalGarage(
    const resource::ResourceFileSystem& resources)
{
    OriginalGarageCatalog catalog;
    auto garage = parseXml(resources, "garage.xml");
    auto workshop = parseXml(resources, "workshop.xml");
    auto tournament = parseXml(resources, "tournamet.xml");
    loadGarageCars(garage.RootElement(), catalog);
    loadWorkshop(workshop.RootElement(), catalog);
    loadUnlocks(tournament.RootElement(), catalog);
    return catalog;
}

OriginalGarageStats originalGarageStats(
    const OriginalGarageCatalog& catalog,
    const OriginalGarageCar& car) noexcept
{
    OriginalGarageStats result;

    float maximumSpeed = 0.0F;
    for (const auto& candidate : catalog.cars)
    {
        const float speed =
            maximumMobilitySkill(
                catalog, candidate, GarageSlotType::Engine) +
            maximumMobilitySkill(
                catalog, candidate, GarageSlotType::Exhaust) +
            maximumMobilitySkill(
                catalog, candidate, GarageSlotType::Wheel);
        maximumSpeed = std::max(maximumSpeed, speed);

        const auto* maximumArmorItem = upgradeItem(
            catalog, candidate, GarageSlotType::Armor, 2);
        const auto* maximumArmorFunction =
            carFunction(maximumArmorItem, candidate.record);
        if (maximumArmorFunction != nullptr)
            result.maximumArmor =
                std::max(result.maximumArmor,
                         maximumArmorFunction->life);

        float candidateDamage = 0.0F;
        for (std::size_t slot =
                 static_cast<std::size_t>(
                     GarageSlotType::Weapon1);
             slot <= static_cast<std::size_t>(
                         GarageSlotType::Weapon4);
             ++slot)
        {
            const auto& placement = candidate.placements[slot];
            if (!placement.active)
                continue;
            candidateDamage +=
                placement.locked &&
                        !placement.defaultItem.empty()
                    ? defaultWeaponDamage(catalog, placement)
                    : maximumWeaponDamage(catalog, placement);
        }
        result.maximumDamage =
            std::max(result.maximumDamage, candidateDamage);
    }

    const auto& armorPlacement = car.placements[
        static_cast<std::size_t>(GarageSlotType::Armor)];
    if (const auto* function = carFunction(
            catalog.findItem(armorPlacement.defaultItem),
            car.record))
    {
        result.armor = function->life;
    }

    for (std::size_t slot =
             static_cast<std::size_t>(
                 GarageSlotType::Weapon1);
         slot <= static_cast<std::size_t>(
                     GarageSlotType::Weapon4);
         ++slot)
    {
        result.damage += defaultWeaponDamage(
            catalog, car.placements[slot]);
    }

    const float speed =
        defaultMobilitySkill(
            catalog, car, GarageSlotType::Engine) +
        defaultMobilitySkill(
            catalog, car, GarageSlotType::Exhaust) +
        defaultMobilitySkill(
            catalog, car, GarageSlotType::Wheel);
    result.armorProgress =
        result.maximumArmor == 0.0F
            ? 1.0F
            : result.armor / result.maximumArmor;
    result.damageProgress =
        result.maximumDamage == 0.0F
            ? 1.0F
            : result.damage / result.maximumDamage;
    result.speedProgress =
        maximumSpeed == 0.0F ? 1.0F : speed / maximumSpeed;
    return result;
}

OriginalGarageStats originalGarageStats(
    const OriginalGarageCatalog& catalog,
    const OriginalGarageCar& car,
    const PlayerProfile& player,
    bool armor4Opened) noexcept
{
    auto result = originalGarageStats(catalog, car);
    result.armor = 0.0F;
    result.damage = 0.0F;

    const auto armorIndex =
        static_cast<std::size_t>(GarageSlotType::Armor);
    if (const auto* function = carFunction(
            catalog.findItem(player.slots[armorIndex].record),
            car.record))
    {
        result.armor = function->life;
        if (armor4Opened &&
            player.slots[armorIndex].record == workshopRecord("armor3"))
            result.armor += 10.0F;
    }

    for (std::size_t slot = PlayerProfile::firstWeaponSlot;
         slot < player.slots.size(); ++slot)
    {
        if (const auto* item =
                catalog.findItem(player.slots[slot].record))
        {
            result.damage += item->projectileDamage;
        }
    }

    float speed = 0.0F;
    for (const auto slot : {
             GarageSlotType::Wheel, GarageSlotType::Exhaust,
             GarageSlotType::Engine})
    {
        const auto index = static_cast<std::size_t>(slot);
        speed += mobilitySkill(carFunction(
            catalog.findItem(player.slots[index].record),
            car.record));
    }

    result.armorProgress =
        result.maximumArmor == 0.0F
            ? 1.0F
            : result.armor / result.maximumArmor;
    result.damageProgress =
        result.maximumDamage == 0.0F
            ? 1.0F
            : result.damage / result.maximumDamage;

    float maximumSpeed = 0.0F;
    for (const auto& candidate : catalog.cars)
    {
        maximumSpeed = std::max(
            maximumSpeed,
            maximumMobilitySkill(
                catalog, candidate, GarageSlotType::Engine) +
                maximumMobilitySkill(
                    catalog, candidate, GarageSlotType::Exhaust) +
                maximumMobilitySkill(
                    catalog, candidate, GarageSlotType::Wheel));
    }
    result.speedProgress =
        maximumSpeed == 0.0F ? 1.0F : speed / maximumSpeed;
    return result;
}

bool originalRecordAchievementUnlocked(
    const ProfileState& profile, std::string_view record) noexcept
{
    for (const auto& [name, item] : profile.achievementItems)
    {
        (void)name;
        if (item.classId != 1U)
            continue;
        const bool containsRecord = std::any_of(
            item.records.begin(), item.records.end(),
            [&](const auto& candidate) {
                return candidate.record == record;
            });
        if (!containsRecord)
            continue;
        const auto state = item.values.find("state");
        if (state == item.values.end() ||
            state->second != "asOpened")
            return false;
    }
    return true;
}

bool originalGamerUnlocked(
    const ProfileState& profile, std::uint32_t gamerId) noexcept
{
    for (const auto& [name, item] : profile.achievementItems)
    {
        (void)name;
        if (item.classId != 2U)
            continue;
        const auto id = item.values.find("gamerId");
        if (id == item.values.end())
            continue;
        try
        {
            if (std::stoul(id->second) != gamerId)
                continue;
        }
        catch (const std::exception&)
        {
            continue;
        }
        const auto state = item.values.find("state");
        if (state == item.values.end() ||
            state->second != "asOpened")
            return false;
    }
    return true;
}

bool originalCarUnlocked(const OriginalGarageCatalog& catalog,
                         const ProfileState& profile,
                         const OriginalGarageCar& car,
                         bool championship) noexcept
{
    if (car.record == profile.player.currentCar)
        return true;
    if (!originalRecordAchievementUnlocked(profile, car.record))
        return false;
    // Race::SetMode(rmSkirmish) exposes the whole garage catalog.  The
    // tournament's Garage::_items list gates regular cars only in campaign.
    if (!championship)
        return true;
    const bool regular = hasRule(catalog.carUnlocks, car.record);
    if (!regular)
        return false;
    return unlockedByRules(catalog.carUnlocks, profile, car.record);
}

bool originalWorkshopItemUnlocked(
    const OriginalGarageCatalog& catalog, const ProfileState& profile,
    const OriginalWorkshopItem& item) noexcept
{
    if (!originalRecordAchievementUnlocked(profile, item.record))
        return false;
    // WorkshopFrame exposes wheel/exhaust/armor/engine through its dedicated
    // level button.  Those four upgrade families do not have to be present
    // in Workshop::_items (the tournament lists them mainly in AI loadouts).
    if (item.type >= 1U && item.type <= 4U)
        return true;
    return unlockedByRules(
        catalog.workshopUnlocks, profile, item.record);
}

int originalWorkshopUpgradeLevel(
    std::string_view record, GarageSlotType slot) noexcept
{
    return upgradeLevel(record, slot);
}

const OriginalWorkshopItem* originalWorkshopUpgradeItem(
    const OriginalGarageCatalog& catalog, const OriginalGarageCar& car,
    GarageSlotType slot, int level) noexcept
{
    return upgradeItem(catalog, car, slot, level);
}

bool selectOriginalGarageCar(const OriginalGarageCatalog& catalog,
                             ProfileState& profile,
                             const OriginalGarageCar& car,
                             bool championship, std::string& error)
{
    error.clear();
    if (!originalCarUnlocked(catalog, profile, car, championship))
    {
        error = "original car is locked: " + car.record;
        return false;
    }
    if (profile.player.currentCar == car.record)
        return true;
    if (championship && profile.player.money < car.cost)
    {
        error = "not enough money for original car: " + car.record;
        return false;
    }

    const auto* oldCar =
        catalog.findCar(profile.player.currentCar);
    const auto oldSlots = profile.player.slots;
    std::array<ProfileSlot, PlayerProfile::slotCount> newSlots{};
    std::array<bool, PlayerProfile::slotCount> used{};
    for (std::size_t source = 0; source < oldSlots.size(); ++source)
    {
        if (oldSlots[source].record.empty())
            continue;
        const auto* item = catalog.findItem(oldSlots[source].record);
        if (item == nullptr)
            continue;
        auto compatible = [&](std::size_t destination) {
            const auto& placement = car.placements[destination];
            if (!placement.active || placement.locked ||
                used[destination] ||
                !contains(placement.supportedItems,
                          oldSlots[source].record))
                return false;
            if (oldCar != nullptr &&
                oldCar->placements[source].locked)
                return false;
            if (!placement.defaultItem.empty())
            {
                const auto* defaultItem =
                    catalog.findItem(placement.defaultItem);
                if (defaultItem != nullptr &&
                    item->cost < defaultItem->cost)
                    return false;
            }
            return true;
        };
        std::size_t destination = source;
        if (!compatible(destination))
        {
            destination = oldSlots.size();
            for (std::size_t candidate = 0;
                 candidate < oldSlots.size(); ++candidate)
            {
                if (candidate != source && compatible(candidate))
                {
                    destination = candidate;
                    break;
                }
            }
        }
        if (destination < oldSlots.size())
        {
            newSlots[destination] = oldSlots[source];
            used[destination] = true;
        }
    }
    for (std::size_t index = 0; index < newSlots.size(); ++index)
    {
        const auto& placement = car.placements[index];
        if (newSlots[index].record.empty() && placement.active &&
            !placement.defaultItem.empty())
        {
            newSlots[index] = defaultProfileSlot(
                catalog, placement.defaultItem, !championship);
        }
    }

    if (championship)
    {
        std::uint64_t money = profile.player.money - car.cost;
        for (std::size_t source = 0; source < oldSlots.size(); ++source)
        {
            const auto& oldSlot = oldSlots[source];
            if (oldSlot.record.empty() ||
                (oldCar != nullptr &&
                 oldCar->placements[source].locked))
            {
                continue;
            }
            const bool retained = std::any_of(
                newSlots.begin(), newSlots.end(),
                [&](const ProfileSlot& slot) {
                    return slot.record == oldSlot.record;
                });
            if (!retained)
                money += originalWorkshopSellValue(
                    catalog, oldSlot, true);
        }
        profile.player.money = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(
                money, std::numeric_limits<std::uint32_t>::max()));
    }
    profile.player.carChanged =
        championship && !profile.player.currentCar.empty();
    profile.player.currentCar = car.record;
    profile.player.slots = std::move(newSlots);
    if (championship && car.initialUpgradeSet > 0U)
    {
        applyUpgradeLevel(catalog, car, profile.player,
                          car.initialUpgradeSet - 1U, false);
    }
    else if (!championship)
    {
        applyUpgradeLevel(
            catalog, car, profile.player,
            std::min(profile.config.upgradeMaxLevel, 2U), true);
        for (std::size_t index = PlayerProfile::firstWeaponSlot;
             index < profile.player.slots.size(); ++index)
        {
            if (index - PlayerProfile::firstWeaponSlot >=
                profile.config.weaponMaxLevel)
                profile.player.slots[index] = {};
        }
    }
    return true;
}

bool installOriginalWorkshopItem(
    const OriginalGarageCatalog& catalog, ProfileState& profile,
    GarageSlotType slot, const OriginalWorkshopItem& item,
    bool championship, std::string& error)
{
    error.clear();
    const auto slotIndex = static_cast<std::size_t>(slot);
    if (slotIndex >= profile.player.slots.size())
    {
        error = "invalid original workshop slot";
        return false;
    }
    const auto* car = catalog.findCar(profile.player.currentCar);
    if (car == nullptr)
    {
        error = "current original car is absent from garage.xml";
        return false;
    }
    const auto& placement = car->placements[slotIndex];
    if (!placement.active ||
        !contains(placement.supportedItems, item.record))
    {
        error = "workshop item is unsupported by current car";
        return false;
    }
    if (!originalWorkshopItemUnlocked(catalog, profile, item) &&
        item.record != placement.defaultItem)
    {
        error = "original workshop item is locked";
        return false;
    }
    if (!championship && slotIndex >= PlayerProfile::firstWeaponSlot &&
        slotIndex - PlayerProfile::firstWeaponSlot >=
            profile.config.weaponMaxLevel)
    {
        error = "original weapon mount is disabled by game options";
        return false;
    }
    if (placement.locked)
    {
        if (slotIndex >= 4U)
        {
            error = "original fixed workshop slot cannot be replaced";
            return false;
        }
        const int current =
            upgradeLevel(profile.player.slots[slotIndex].record, slot);
        const int requested = upgradeLevel(item.record, slot);
        const int maximum =
            championship
                ? 2
                : static_cast<int>(
                      std::min(profile.config.upgradeMaxLevel, 2U));
        if (requested != current + 1 || requested > maximum)
        {
            error = "original upgrades must be installed one level at a "
                    "time";
            return false;
        }
    }
    if (profile.player.slots[slotIndex].record == item.record)
        return true;
    if (championship && profile.player.money < item.cost)
    {
        error = "not enough money for original workshop item";
        return false;
    }
    if (championship)
        profile.player.money -= item.cost;
    profile.player.slots[slotIndex] =
        defaultProfileSlot(catalog, item.record, !championship);
    return true;
}

bool buyOriginalWorkshopItem(
    const OriginalGarageCatalog& catalog, ProfileState& profile,
    const OriginalWorkshopItem& item, bool championship,
    ProfileSlot& purchased, std::string& error)
{
    error.clear();
    purchased = {};
    const auto* catalogItem = catalog.findItem(item.record);
    if (catalogItem == nullptr)
    {
        error = "workshop item is absent from original workshop.xml";
        return false;
    }
    if (!originalWorkshopItemUnlocked(catalog, profile, *catalogItem))
    {
        error = "original workshop item is locked";
        return false;
    }
    if (championship && profile.player.money < catalogItem->cost)
    {
        error = "not enough money for original workshop item";
        return false;
    }
    if (championship)
        profile.player.money -= catalogItem->cost;
    purchased = defaultProfileSlot(
        catalog, catalogItem->record, !championship);
    return true;
}

bool installOriginalWorkshopSlot(
    const OriginalGarageCatalog& catalog, ProfileState& profile,
    GarageSlotType slot, const ProfileSlot& item, ProfileSlot& replaced,
    std::string& error)
{
    error.clear();
    const auto slotIndex = static_cast<std::size_t>(slot);
    if (slotIndex >= profile.player.slots.size())
    {
        error = "invalid original workshop slot";
        return false;
    }
    const auto* car = catalog.findCar(profile.player.currentCar);
    if (car == nullptr)
    {
        error = "current original car is absent from garage.xml";
        return false;
    }
    const auto& placement = car->placements[slotIndex];
    if (!placement.active || placement.locked ||
        item.record.empty() ||
        !contains(placement.supportedItems, item.record))
    {
        error = "workshop item is unsupported by selected original slot";
        return false;
    }
    if (catalog.findItem(item.record) == nullptr)
    {
        error = "workshop item is absent from original workshop.xml";
        return false;
    }
    replaced = profile.player.slots[slotIndex];
    profile.player.slots[slotIndex] = item;
    return true;
}

std::uint32_t originalWorkshopSellValue(
    const OriginalGarageCatalog& catalog, const ProfileSlot& slot,
    bool discount) noexcept
{
    const auto* item = catalog.findItem(slot.record);
    if (item == nullptr)
        return 0U;
    std::uint64_t cost = item->cost;
    if (item->maximumCharge > 0U)
    {
        const std::uint32_t charge =
            slot.hasCharge ? slot.charge : item->defaultCharge;
        if (charge > 1U)
        {
            cost += static_cast<std::uint64_t>(charge - 1U) *
                    item->chargeCost;
        }
    }
    if (discount)
        cost /= 2U;
    return static_cast<std::uint32_t>(
        std::min<std::uint64_t>(
            cost, std::numeric_limits<std::uint32_t>::max()));
}

bool sellOriginalWorkshopItem(
    const OriginalGarageCatalog& catalog, ProfileState& profile,
    const ProfileSlot& item, bool discount, bool championship,
    std::string& error)
{
    error.clear();
    if (catalog.findItem(item.record) == nullptr)
    {
        error = "workshop item is absent from original workshop.xml";
        return false;
    }
    if (!championship)
        return true;
    const auto value =
        originalWorkshopSellValue(catalog, item, discount);
    profile.player.money =
        value > std::numeric_limits<std::uint32_t>::max() -
                    profile.player.money
            ? std::numeric_limits<std::uint32_t>::max()
            : profile.player.money + value;
    return true;
}

bool rechargeOriginalWorkshopItem(
    const OriginalGarageCatalog& catalog, ProfileState& profile,
    GarageSlotType slot, bool championship, std::string& error)
{
    error.clear();
    const auto slotIndex = static_cast<std::size_t>(slot);
    if (slotIndex >= profile.player.slots.size())
    {
        error = "invalid original workshop slot";
        return false;
    }
    auto& installed = profile.player.slots[slotIndex];
    const auto* item = catalog.findItem(installed.record);
    if (item == nullptr || item->maximumCharge == 0U)
    {
        error = "installed original item has no charge";
        return false;
    }
    const auto charge = installed.hasCharge ? installed.charge
                                            : item->defaultCharge;
    const auto amount =
        charge >= item->maximumCharge
            ? 0U
            : std::min(item->chargeStep,
                       item->maximumCharge - charge);
    if (amount == 0U)
        return true;
    const auto cost = item->chargeCost * amount;
    if (championship && profile.player.money < cost)
    {
        error = "not enough money for original ammunition";
        return false;
    }
    if (championship)
        profile.player.money -= cost;
    installed.hasCharge = true;
    installed.charge = charge + amount;
    return true;
}

bool runOriginalGarageSmokeTest(
    const resource::ResourceFileSystem& resources, std::string& error)
{
    error.clear();
    try
    {
        const auto catalog = loadOriginalGarage(resources);
        const auto* marauder = catalog.findCar(
            "world\\db\\root\\ctCar\\marauder");
        const auto* dirtdevil = catalog.findCar(
            "world\\db\\root\\ctCar\\dirtdevil");
        const auto* wheel2 = catalog.findItem(
            workshopRecord("wheel2"));
        const auto* bullet = catalog.findItem(
            workshopRecord("bulletGun"));
        const auto* rocket = catalog.findItem(
            workshopRecord("rocketLauncher"));
        const auto* pulsator = catalog.findItem(
            workshopRecord("pulsator"));
        if (catalog.cars.size() < 13U ||
            catalog.workshop.size() < 30U ||
            catalog.planets.size() != 6U ||
            catalog.gamers.size() != 7U ||
            marauder == nullptr || dirtdevil == nullptr ||
            wheel2 == nullptr || bullet == nullptr ||
            rocket == nullptr || pulsator == nullptr ||
            marauder->cost != 18000U ||
            dirtdevil->cost != 20000U ||
            wheel2->meshPath != "Data/Upgrade/wheel2.r3d" ||
            wheel2->texturePath != "Data/Upgrade/wheel2.dds" ||
            bullet->meshPath != "Data/Weapon/bulletGun.r3d" ||
            bullet->texturePath != "Data/Car/marauder.dds" ||
            bullet->maximumCharge != 28U ||
            bullet->projectileDamage != 6.0F ||
            catalog.planets.front().record != "planet0" ||
            catalog.planets.front().meshPath !=
                "Data/GUI/planet.r3d" ||
            catalog.planets.front().texturePath !=
                "Data/GUI/intaria.dds" ||
            catalog.planets.front().requestPoints !=
                std::vector<std::uint32_t>{1400U, 1600U} ||
            catalog.planets.front().bossName != "scMardock" ||
            catalog.planets.front().bossPhotoPath !=
                "Data/GUI/Chars/mardock.png" ||
            catalog.planets.front().bossCarRecord !=
                "world\\db\\root\\ctCar\\manticoraBoss" ||
            catalog.gamers.front().record != "gamer6" ||
            catalog.gamers.front().bossId != 10U ||
            catalog.gamers.front().name != "svTyler" ||
            catalog.gamers.front().bossBonus != "svTylerBonus" ||
            catalog.gamers.front().bossPhotoPath !=
                "Data/GUI/Chars/tyler.png" ||
            catalog.gamers.back().record != "gamer5" ||
            catalog.gamers.back().bossId != 9U ||
            marauder->placements[0].defaultItem !=
                workshopRecord("wheel1"))
        {
            error = "original garage/workshop catalog mismatch";
            return false;
        }
        auto defaultProfile = makeOriginalDefaultProfileState();
        defaultProfile.achievementItems["viper"].classId = 2U;
        defaultProfile.achievementItems["viper"]
            .values["gamerId"] = "9";
        defaultProfile.achievementItems["viper"]
            .values["state"] = "asUnlocked";
        if (defaultProfile.player.planets.front().state != 0U ||
            defaultProfile.player.planets.front().pass != 1U ||
            !originalGamerUnlocked(defaultProfile, 10U) ||
            originalGamerUnlocked(defaultProfile, 9U) ||
            !originalWorkshopItemUnlocked(
                catalog, defaultProfile, *rocket) ||
            !originalWorkshopItemUnlocked(
                catalog, defaultProfile, *pulsator))
        {
            error =
                "source Profile::Enter/Workshop pass-zero assortment "
                "mismatch";
            return false;
        }
        auto openedViperProfile = defaultProfile;
        openedViperProfile.achievementItems["viper"]
            .values["state"] = "asOpened";
        if (!originalGamerUnlocked(openedViperProfile, 9U))
        {
            error = "source AchievementModel::CheckGamerId mismatch";
            return false;
        }
        const auto stats = originalGarageStats(catalog, *marauder);
        if (stats.maximumArmor <= 0.0F ||
            stats.maximumDamage <= 0.0F ||
            stats.armorProgress <= 0.0F ||
            stats.armorProgress > 1.0F ||
            stats.damageProgress <= 0.0F ||
            stats.damageProgress > 1.0F ||
            stats.speedProgress <= 0.0F ||
            stats.speedProgress > 1.0F)
        {
            error = "original Garage::UpdateStats behavior mismatch";
            return false;
        }
        auto armorPlayer = defaultProfile.player;
        armorPlayer.slots[static_cast<std::size_t>(
            GarageSlotType::Armor)].record = workshopRecord("armor3");
        const auto standardArmorStats = originalGarageStats(
            catalog, *marauder, armorPlayer, false);
        const auto armor4Stats = originalGarageStats(
            catalog, *marauder, armorPlayer, true);
        if (std::abs(
                armor4Stats.armor - standardArmorStats.armor - 10.0F) >
            0.001F)
        {
            error = "source ArmorItem::CalcLife garage armor4 mismatch";
            return false;
        }

        ProfileState profile;
        for (auto& planet : profile.player.planets)
            planet = {0U, 3U};
        profile.player.currentCar = marauder->record;
        profile.player.money = dirtdevil->cost;
        profile.player.slots[0].record = workshopRecord("wheel1");
        profile.player.slots[6] = {
            workshopRecord("bulletGun"), 4U, true};
        if (!originalCarUnlocked(
                catalog, profile, *dirtdevil, true) ||
            !selectOriginalGarageCar(
                catalog, profile, *dirtdevil, true, error) ||
            profile.player.currentCar != dirtdevil->record ||
            profile.player.money != 0U ||
            profile.player.slots[6].record !=
                workshopRecord("rifleWeapon"))
        {
            if (error.empty())
                error = "original Garage::BuyCar behavior mismatch";
            return false;
        }

        profile.player.money = wheel2->cost;
        if (!installOriginalWorkshopItem(
                catalog, profile, GarageSlotType::Wheel, *wheel2,
                true, error) ||
            profile.player.slots[0].record != wheel2->record ||
            profile.player.money != 0U)
        {
            if (error.empty())
                error = "original workshop upgrade behavior mismatch";
            return false;
        }

        ProfileState transactionProfile;
        for (auto& planet : transactionProfile.player.planets)
            planet = {0U, 99U};
        transactionProfile.player.currentCar =
            dirtdevil->record;
        transactionProfile.player.money = 1000000U;
        const auto startingMoney =
            transactionProfile.player.money;
        ProfileSlot purchasedRocket;
        ProfileSlot purchasedPulsator;
        ProfileSlot replaced;
        if (!buyOriginalWorkshopItem(
                catalog, transactionProfile, *rocket, true,
                purchasedRocket, error) ||
            !installOriginalWorkshopSlot(
                catalog, transactionProfile,
                GarageSlotType::Weapon2, purchasedRocket,
                replaced, error) ||
            !replaced.record.empty() ||
            !buyOriginalWorkshopItem(
                catalog, transactionProfile, *pulsator, true,
                purchasedPulsator, error) ||
            !installOriginalWorkshopSlot(
                catalog, transactionProfile,
                GarageSlotType::Weapon2, purchasedPulsator,
                replaced, error) ||
            replaced.record != rocket->record)
        {
            if (error.empty())
                error =
                    "original WorkshopFrame buy/install/swap behavior "
                    "mismatch";
            return false;
        }
        const auto discountedRocket =
            originalWorkshopSellValue(
                catalog, replaced, true);
        if (!sellOriginalWorkshopItem(
                catalog, transactionProfile, replaced, true,
                true, error) ||
            transactionProfile.player.slots[7].record !=
                pulsator->record ||
            transactionProfile.player.money !=
                startingMoney - rocket->cost -
                    pulsator->cost + discountedRocket)
        {
            if (error.empty())
                error =
                    "original WorkshopFrame discounted sale behavior "
                    "mismatch";
            return false;
        }

        ProfileState carSwitchProfile;
        for (auto& planet : carSwitchProfile.player.planets)
            planet = {0U, 99U};
        carSwitchProfile.player.currentCar = dirtdevil->record;
        carSwitchProfile.player.money = marauder->cost;
        carSwitchProfile.player.slots[7] = purchasedRocket;
        const auto switchedRocketValue =
            originalWorkshopSellValue(
                catalog, purchasedRocket, true);
        if (!selectOriginalGarageCar(
                catalog, carSwitchProfile, *marauder, true, error) ||
            carSwitchProfile.player.money != switchedRocketValue)
        {
            if (error.empty())
                error =
                    "Garage::BuyCar did not sell incompatible installed "
                    "equipment";
            return false;
        }
        return true;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return false;
    }
}

} // namespace r3d::game::originalrace
