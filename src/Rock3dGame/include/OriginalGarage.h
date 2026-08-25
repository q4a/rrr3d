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
    struct CarFunction
    {
        struct Tire
        {
            float extremumSlip = 0.0F;
            float extremumValue = 0.0F;
            float asymptoteSlip = 0.0F;
            float asymptoteValue = 0.0F;
        };

        std::string car;
        Tire longitudinalTire;
        Tire lateralTire;
        float maximumTorque = 0.0F;
        float life = 0.0F;
        float maximumSpeed = 0.0F;
        float tireSpring = 0.0F;
    };

    std::string record;
    std::string name;
    std::string info;
    std::string meshPath;
    std::string texturePath;
    std::array<float, 3> visualPosition{};
    std::array<float, 4> visualRotation{0.0F, 0.0F, 0.0F, 1.0F};
    std::uint32_t type = 0;
    std::uint32_t cost = 0;
    std::uint32_t maximumCharge = 0;
    std::uint32_t defaultCharge = 0;
    std::uint32_t chargeStep = 1;
    std::uint32_t chargeCost = 0;
    float projectileDamage = 0.0F;
    std::vector<CarFunction> carFunctions;
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
    std::string record;
    std::string name;
    std::string info;
    std::string worldType;
    std::string meshPath;
    std::string texturePath;
    std::vector<std::uint32_t> requestPoints;
    std::uint32_t bossId = 0;
    std::string bossName;
    std::string bossBonus;
    std::string bossPhotoPath;
    std::string bossCarRecord;
};

struct OriginalGarageCatalog
{
    std::vector<OriginalGarageCar> cars;
    std::vector<OriginalWorkshopItem> workshop;
    std::vector<OriginalGaragePlanet> planets;
    std::vector<OriginalGaragePlanet> gamers;
    std::vector<OriginalUnlockRule> carUnlocks;
    std::vector<OriginalUnlockRule> workshopUnlocks;

    const OriginalGarageCar* findCar(std::string_view record) const noexcept;
    const OriginalWorkshopItem* findItem(
        std::string_view record) const noexcept;
};

struct OriginalGarageStats
{
    float armor = 0.0F;
    float maximumArmor = 0.0F;
    float damage = 0.0F;
    float maximumDamage = 0.0F;
    float armorProgress = 0.0F;
    float damageProgress = 0.0F;
    float speedProgress = 0.0F;
};

OriginalGarageCatalog loadOriginalGarage(
    const resource::ResourceFileSystem& resources);
std::vector<OriginalWorkshopItem> loadOriginalWorkshop(
    const resource::ResourceFileSystem& resources);

OriginalGarageStats originalGarageStats(
    const OriginalGarageCatalog& catalog,
    const OriginalGarageCar& car) noexcept;
OriginalGarageStats originalGarageStats(
    const OriginalGarageCatalog& catalog,
    const OriginalGarageCar& car,
    const PlayerProfile& player,
    bool armor4Opened = false) noexcept;

bool originalRecordAchievementUnlocked(
    const ProfileState& profile, std::string_view record) noexcept;
bool originalGamerUnlocked(
    const ProfileState& profile, std::uint32_t gamerId) noexcept;
bool originalCarUnlocked(const OriginalGarageCatalog& catalog,
                         const ProfileState& profile,
                         const OriginalGarageCar& car,
                         bool championship) noexcept;
bool originalWorkshopItemUnlocked(
    const OriginalGarageCatalog& catalog, const ProfileState& profile,
    const OriginalWorkshopItem& item) noexcept;
int originalWorkshopUpgradeLevel(
    std::string_view record, GarageSlotType slot) noexcept;
const OriginalWorkshopItem* originalWorkshopUpgradeItem(
    const OriginalGarageCatalog& catalog, const OriginalGarageCar& car,
    GarageSlotType slot, int level) noexcept;

bool selectOriginalGarageCar(const OriginalGarageCatalog& catalog,
                             ProfileState& profile,
                             const OriginalGarageCar& car,
                             bool championship, std::string& error);
bool installOriginalWorkshopItem(
    const OriginalGarageCatalog& catalog, ProfileState& profile,
    GarageSlotType slot, const OriginalWorkshopItem& item,
    bool championship, std::string& error);
bool buyOriginalWorkshopItem(
    const OriginalGarageCatalog& catalog, ProfileState& profile,
    const OriginalWorkshopItem& item, bool championship,
    ProfileSlot& purchased, std::string& error);
bool installOriginalWorkshopSlot(
    const OriginalGarageCatalog& catalog, ProfileState& profile,
    GarageSlotType slot, const ProfileSlot& item, ProfileSlot& replaced,
    std::string& error);
std::uint32_t originalWorkshopSellValue(
    const OriginalGarageCatalog& catalog, const ProfileSlot& item,
    bool discount) noexcept;
bool sellOriginalWorkshopItem(
    const OriginalGarageCatalog& catalog, ProfileState& profile,
    const ProfileSlot& item, bool discount, bool championship,
    std::string& error);
bool rechargeOriginalWorkshopItem(
    const OriginalGarageCatalog& catalog, ProfileState& profile,
    GarageSlotType slot, bool championship, std::string& error);

bool runOriginalGarageSmokeTest(
    const resource::ResourceFileSystem& resources, std::string& error);

} // namespace r3d::game::originalrace
