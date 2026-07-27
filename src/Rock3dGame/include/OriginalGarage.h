#pragma once

#include "OriginalProfile.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace r3d::resource
{
class ResourceFileSystem;
}

namespace r3d::game::originalrace
{

enum class GarageSlotType : std::uint8_t
{
    Wheel,
    Exhaust,
    Armor,
    Engine,
    Hyper,
    Mine,
    Weapon1,
    Weapon2,
    Weapon3,
    Weapon4,
    Count,
};

struct OriginalWorkshopItem
{
    std::string record;
    std::string name;
    std::string info;
    std::uint32_t type = 0;
    std::uint32_t cost = 0;
    std::uint32_t maximumCharge = 0;
    std::uint32_t defaultCharge = 0;
    std::uint32_t chargeStep = 1;
    std::uint32_t chargeCost = 0;
};

struct OriginalGaragePlacement
{
    bool active = false;
    bool visible = false;
    bool locked = false;
    std::string defaultItem;
    std::vector<std::string> supportedItems;
};

struct OriginalGarageCar
{
    std::string record;
    std::string name;
    std::string info;
    std::uint32_t cost = 0;
    std::uint32_t initialUpgradeSet = 0;
    std::array<OriginalGaragePlacement,
               static_cast<std::size_t>(GarageSlotType::Count)>
        placements;
};

struct OriginalUnlockRule
{
    std::string record;
    std::size_t planet = 0;
    std::uint32_t pass = 0;
};

struct OriginalGaragePlanet
{
    std::string name;
    std::string info;
    std::string worldType;
};

struct OriginalGarageCatalog
{
    std::vector<OriginalGarageCar> cars;
    std::vector<OriginalWorkshopItem> workshop;
    std::vector<OriginalGaragePlanet> planets;
    std::vector<OriginalUnlockRule> carUnlocks;
    std::vector<OriginalUnlockRule> workshopUnlocks;

    const OriginalGarageCar* findCar(std::string_view record) const noexcept;
    const OriginalWorkshopItem* findItem(
        std::string_view record) const noexcept;
};

OriginalGarageCatalog loadOriginalGarage(
    const resource::ResourceFileSystem& resources);

bool originalRecordAchievementUnlocked(
    const ProfileState& profile, std::string_view record) noexcept;
bool originalCarUnlocked(const OriginalGarageCatalog& catalog,
                         const ProfileState& profile,
                         const OriginalGarageCar& car,
                         bool championship) noexcept;
bool originalWorkshopItemUnlocked(
    const OriginalGarageCatalog& catalog, const ProfileState& profile,
    const OriginalWorkshopItem& item) noexcept;

bool selectOriginalGarageCar(const OriginalGarageCatalog& catalog,
                             ProfileState& profile,
                             const OriginalGarageCar& car,
                             bool championship, std::string& error);
bool installOriginalWorkshopItem(
    const OriginalGarageCatalog& catalog, ProfileState& profile,
    GarageSlotType slot, const OriginalWorkshopItem& item,
    bool championship, std::string& error);
bool rechargeOriginalWorkshopItem(
    const OriginalGarageCatalog& catalog, ProfileState& profile,
    GarageSlotType slot, bool championship, std::string& error);

bool runOriginalGarageSmokeTest(
    const resource::ResourceFileSystem& resources, std::string& error);

} // namespace r3d::game::originalrace
