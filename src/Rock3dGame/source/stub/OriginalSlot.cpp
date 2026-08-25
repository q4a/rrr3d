#include "OriginalSlot.h"

#include "OriginalWeapon.h"

#include "OriginalPlayer.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace r3d::game::originalrace::source
{
namespace
{

std::string_view basename(std::string_view value) noexcept
{
    const auto slash = value.find_last_of("\\/");
    return value.substr(slash == std::string_view::npos ? 0U : slash + 1U);
}

bool sameRecord(std::string_view first, std::string_view second) noexcept
{
    return first == second || basename(first) == basename(second);
}

SlotType slotType(std::uint32_t value) noexcept
{
    return value < static_cast<std::uint32_t>(SlotType::Count)
               ? static_cast<SlotType>(value)
               : SlotType::Base;
}

std::size_t physicalSlot(std::string_view value) noexcept
{
    if (value == "stWheel")
        return static_cast<std::size_t>(PlayerSlotType::Wheel);
    if (value == "stTruba")
        return static_cast<std::size_t>(PlayerSlotType::Truba);
    if (value == "stArmor")
        return static_cast<std::size_t>(PlayerSlotType::Armor);
    if (value == "stMotor")
        return static_cast<std::size_t>(PlayerSlotType::Motor);
    if (value == "stHyper")
        return static_cast<std::size_t>(PlayerSlotType::Hyper);
    if (value == "stMine")
        return static_cast<std::size_t>(PlayerSlotType::Mine);
    constexpr std::string_view weaponPrefix = "stWeapon";
    if (value.starts_with(weaponPrefix) &&
        value.size() == weaponPrefix.size() + 1U &&
        value.back() >= '1' && value.back() <= '4')
    {
        return static_cast<std::size_t>(PlayerSlotType::Weapon1) +
               static_cast<std::size_t>(value.back() - '1');
    }
    return PlayerSlotRack::slotCount;
}

void addTire(const MobilityItem::Tire& input,
             r3d::physics::WheelDescription::TireFunction& output) noexcept
{
    output.extremumSlip += input.extremumSlip;
    output.extremumValue += input.extremumValue;
    output.asymptoteSlip += input.asymptoteSlip;
    output.asymptoteValue += input.asymptoteValue;
}

} // namespace

SlotItem::SlotItem(SlotType type) noexcept : type_(type) {}

MobilityItem* SlotItem::IsMobilityItem() noexcept { return nullptr; }
const MobilityItem* SlotItem::IsMobilityItem() const noexcept
{
    return nullptr;
}
WeaponItem* SlotItem::IsWeaponItem() noexcept { return nullptr; }
const WeaponItem* SlotItem::IsWeaponItem() const noexcept
{
    return nullptr;
}

SlotType SlotItem::GetType() const noexcept { return type_; }
const std::string& SlotItem::GetRecord() const noexcept { return record_; }
const std::string& SlotItem::GetName() const noexcept { return name_; }
const std::string& SlotItem::GetInfo() const noexcept { return info_; }
std::uint32_t SlotItem::GetCost() const noexcept { return cost_; }
const std::string& SlotItem::GetMeshPath() const noexcept
{
    return meshPath_;
}
const std::string& SlotItem::GetTexturePath() const noexcept
{
    return texturePath_;
}
const std::array<float, 3>& SlotItem::GetPos() const noexcept
{
    return position_;
}
void SlotItem::SetPos(const std::array<float, 3>& value) noexcept
{
    position_ = value;
}
const std::array<float, 4>& SlotItem::GetRot() const noexcept
{
    return rotation_;
}
void SlotItem::SetRot(const std::array<float, 4>& value) noexcept
{
    rotation_ = value;
}

void SlotItem::Load(const OriginalWorkshopItem& item)
{
    record_ = item.record;
    name_ = item.name;
    info_ = item.info;
    cost_ = item.cost;
    meshPath_ = item.meshPath;
    texturePath_ = item.texturePath;
    position_ = item.visualPosition;
    rotation_ = item.visualRotation;
}

MobilityItem::MobilityItem(SlotType type) noexcept : SlotItem(type) {}
MobilityItem* MobilityItem::IsMobilityItem() noexcept { return this; }
const MobilityItem* MobilityItem::IsMobilityItem() const noexcept
{
    return this;
}

void MobilityItem::Load(const OriginalWorkshopItem& item)
{
    SlotItem::Load(item);
    carFuncMap_.clear();
    carFuncMap_.reserve(item.carFunctions.size());
    for (const auto& input : item.carFunctions)
    {
        CarFunc output;
        output.car = input.car;
        output.longTire = {
            input.longitudinalTire.extremumSlip,
            input.longitudinalTire.extremumValue,
            input.longitudinalTire.asymptoteSlip,
            input.longitudinalTire.asymptoteValue};
        output.latTire = {
            input.lateralTire.extremumSlip,
            input.lateralTire.extremumValue,
            input.lateralTire.asymptoteSlip,
            input.lateralTire.asymptoteValue};
        output.maxTorque = input.maximumTorque;
        output.life = input.life;
        output.maxSpeed = input.maximumSpeed;
        output.tireSpring = input.tireSpring;
        carFuncMap_.push_back(std::move(output));
    }
}

const MobilityItem::CarFunc* MobilityItem::FindCarFunc(
    std::string_view car) const noexcept
{
    const auto found = std::find_if(
        carFuncMap_.begin(), carFuncMap_.end(),
        [&](const CarFunc& function) {
            return sameRecord(function.car, car);
        });
    return found == carFuncMap_.end() ? nullptr : &*found;
}

float MobilityItem::CalcLife(const CarFunc& function) const noexcept
{
    return function.life;
}

const std::vector<MobilityItem::CarFunc>&
MobilityItem::GetCarFuncMap() const noexcept
{
    return carFuncMap_;
}

WheelItem::WheelItem() noexcept : MobilityItem(SlotType::Wheel) {}
TrubaItem::TrubaItem() noexcept : MobilityItem(SlotType::Truba) {}
MotorItem::MotorItem() noexcept : MobilityItem(SlotType::Motor) {}
ArmorItem::ArmorItem() noexcept : MobilityItem(SlotType::Armor) {}

void ArmorItem::SetAchievementOpened(bool value) noexcept
{
    achievementOpened_ = value;
}
void ArmorItem::SetHuman(bool value) noexcept { human_ = value; }

bool ArmorItem::CheckArmor4(bool ignorePlayers) const noexcept
{
    return basename(record_) == "armor3" && achievementOpened_ &&
           (ignorePlayers || human_);
}

bool ArmorItem::IsArmor4Installed() const noexcept
{
    return armor4Installed_;
}

void ArmorItem::InstalArmor4(bool install) noexcept
{
    armor4Installed_ = install;
}

const std::string& ArmorItem::GetName() const noexcept
{
    static const std::string armor4Name = "scArmor4";
    return armor4Installed_ || CheckArmor4(true)
               ? armor4Name
               : MobilityItem::GetName();
}

const std::string& ArmorItem::GetInfo() const noexcept
{
    static const std::string armor4Info = "scArmor4Info";
    return armor4Installed_ || CheckArmor4(true)
               ? armor4Info
               : MobilityItem::GetInfo();
}

const std::string& ArmorItem::GetMeshPath() const noexcept
{
    static const std::string armor4Mesh = "Data/Upgrade/armor4.r3d";
    return armor4Installed_ || CheckArmor4(true)
               ? armor4Mesh
               : MobilityItem::GetMeshPath();
}

const std::string& ArmorItem::GetTexturePath() const noexcept
{
    static const std::string armor4Texture = "Data/Upgrade/armor4.dds";
    return armor4Installed_ || CheckArmor4(true)
               ? armor4Texture
               : MobilityItem::GetTexturePath();
}

float ArmorItem::CalcLife(const CarFunc& function) const noexcept
{
    return function.life +
           ((armor4Installed_ || CheckArmor4()) ? 10.0F : 0.0F);
}

Slot::Slot() { CreateItem(SlotType::Base); }

Slot::Slot(const Slot& other)
{
    SetRecord(other.record_);
    GetItem().SetPos(other.GetItem().GetPos());
    GetItem().SetRot(other.GetItem().GetRot());
    const auto* sourceArmor =
        dynamic_cast<const ArmorItem*>(&other.GetItem());
    auto* targetArmor = dynamic_cast<ArmorItem*>(&GetItem());
    if (sourceArmor != nullptr && targetArmor != nullptr)
        targetArmor->InstalArmor4(sourceArmor->IsArmor4Installed());
}

Slot& Slot::operator=(const Slot& other)
{
    if (this == &other)
        return *this;
    SetRecord(other.record_);
    GetItem().SetPos(other.GetItem().GetPos());
    GetItem().SetRot(other.GetItem().GetRot());
    const auto* sourceArmor =
        dynamic_cast<const ArmorItem*>(&other.GetItem());
    auto* targetArmor = dynamic_cast<ArmorItem*>(&GetItem());
    if (sourceArmor != nullptr && targetArmor != nullptr)
        targetArmor->InstalArmor4(sourceArmor->IsArmor4Installed());
    return *this;
}

SlotItem& Slot::CreateItem(SlotType type)
{
    switch (type)
    {
    case SlotType::Wheel:
        item_ = std::make_unique<WheelItem>();
        break;
    case SlotType::Truba:
        item_ = std::make_unique<TrubaItem>();
        break;
    case SlotType::Armor:
        item_ = std::make_unique<ArmorItem>();
        break;
    case SlotType::Motor:
        item_ = std::make_unique<MotorItem>();
        break;
    case SlotType::Hyper:
        item_ = std::make_unique<HyperItem>();
        break;
    case SlotType::Mine:
        item_ = std::make_unique<MineItem>();
        break;
    case SlotType::Weapon:
        item_ = std::make_unique<WeaponItem>();
        break;
    case SlotType::Droid:
        item_ = std::make_unique<DroidItem>();
        break;
    case SlotType::Reflector:
        item_ = std::make_unique<ReflectorItem>();
        break;
    default:
        item_ = std::make_unique<SlotItem>(type);
        break;
    }
    return *item_;
}

void Slot::SetRecord(const OriginalWorkshopItem* record)
{
    record_ = record;
    if (record_ == nullptr)
    {
        CreateItem(SlotType::Base);
        return;
    }
    auto& item = CreateItem(slotType(record_->type));
    item.Load(*record_);
}

SlotItem& Slot::GetItem() noexcept { return *item_; }
const SlotItem& Slot::GetItem() const noexcept { return *item_; }
const OriginalWorkshopItem* Slot::GetRecord() const noexcept
{
    return record_;
}
SlotType Slot::GetType() const noexcept { return item_->GetType(); }

void PlayerSlotRack::Bind(
    const std::vector<OriginalWorkshopItem>& workshop,
    const std::vector<RacerSlot>& loadout)
{
    for (auto& slot : slots_)
        slot.SetRecord(nullptr);
    for (const auto& loadoutSlot : loadout)
    {
        const auto found = std::find_if(
            workshop.begin(), workshop.end(),
            [&](const OriginalWorkshopItem& item) {
                return sameRecord(item.record, loadoutSlot.record);
            });
        if (found == workshop.end())
            continue;
        const auto type = slotType(found->type);
        std::size_t target = physicalSlot(loadoutSlot.type);
        if (target >= slots_.size())
        {
            switch (type)
            {
            case SlotType::Wheel:
                target = static_cast<std::size_t>(PlayerSlotType::Wheel);
                break;
            case SlotType::Truba:
                target = static_cast<std::size_t>(PlayerSlotType::Truba);
                break;
            case SlotType::Armor:
                target = static_cast<std::size_t>(PlayerSlotType::Armor);
                break;
            case SlotType::Motor:
                target = static_cast<std::size_t>(PlayerSlotType::Motor);
                break;
            case SlotType::Hyper:
                target = static_cast<std::size_t>(PlayerSlotType::Hyper);
                break;
            case SlotType::Mine:
                target = static_cast<std::size_t>(PlayerSlotType::Mine);
                break;
            case SlotType::Weapon:
            case SlotType::Droid:
            case SlotType::Reflector:
                target = static_cast<std::size_t>(PlayerSlotType::Weapon1);
                while (target < slots_.size() &&
                       slots_[target].GetRecord() != nullptr)
                    ++target;
                break;
            default:
                break;
            }
        }
        if (target < slots_.size())
            slots_[target].SetRecord(&*found);
    }
}

void PlayerSlotRack::ApplyMobility(
    Vehicle& vehicle, std::string_view difficulty,
    bool humanOrOpponent, bool armor4Opened) noexcept
{
    const float baseMaximumSpeed = vehicle.physics.maximumSpeed;
    const float baseTireSpring = vehicle.physics.tireSpring;
    float maximumTorque = 0.0F;
    float maximumLife = 0.0F;
    float maximumSpeed = 0.0F;
    float tireSpring = baseTireSpring;
    r3d::physics::WheelDescription::TireFunction longitudinalTire;
    r3d::physics::WheelDescription::TireFunction lateralTire;

    for (auto& slot : slots_)
    {
        auto* mobility = slot.GetItem().IsMobilityItem();
        if (mobility == nullptr)
            continue;
        if (auto* armor = dynamic_cast<ArmorItem*>(mobility))
        {
            armor->SetAchievementOpened(armor4Opened);
            armor->SetHuman(humanOrOpponent);
        }
        const auto* function = mobility->FindCarFunc(vehicle.record);
        if (function == nullptr)
            continue;
        maximumTorque += function->maxTorque;
        maximumLife += mobility->CalcLife(*function);
        maximumSpeed = std::max(maximumSpeed, function->maxSpeed);
        tireSpring += function->tireSpring;
        addTire(function->longTire, longitudinalTire);
        addTire(function->latTire, lateralTire);
    }

    if (humanOrOpponent)
    {
        const std::size_t difficultyIndex =
            difficulty == "gdEasy" ? 0U
            : difficulty == "gdHard" ? 2U
                                     : 1U;
        maximumLife *= Player::humanArmorScale[difficultyIndex];
    }
    vehicle.physics.maximumTorque = maximumTorque;
    vehicle.physics.maximumSpeed = baseMaximumSpeed + maximumSpeed;
    vehicle.physics.tireSpring = tireSpring;
    vehicle.maximumLife = maximumLife;
    for (auto& wheel : vehicle.physics.wheels)
    {
        wheel.longitudinalTire = longitudinalTire;
        wheel.lateralTire = lateralTire;
    }
}

Slot& PlayerSlotRack::GetSlot(PlayerSlotType type) noexcept
{
    return slots_[static_cast<std::size_t>(type)];
}
const Slot& PlayerSlotRack::GetSlot(PlayerSlotType type) const noexcept
{
    return slots_[static_cast<std::size_t>(type)];
}

Slot* PlayerSlotRack::GetSlotInst(SlotType type) noexcept
{
    const auto found = std::find_if(
        slots_.begin(), slots_.end(),
        [type](const Slot& slot) { return slot.GetType() == type; });
    return found == slots_.end() ? nullptr : &*found;
}

const Slot* PlayerSlotRack::GetSlotInst(SlotType type) const noexcept
{
    const auto found = std::find_if(
        slots_.begin(), slots_.end(),
        [type](const Slot& slot) { return slot.GetType() == type; });
    return found == slots_.end() ? nullptr : &*found;
}

} // namespace r3d::game::originalrace::source
