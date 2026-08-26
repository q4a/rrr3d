#include "OriginalRockCar.h"

#include <utility>

namespace r3d::game::originalrace::source
{

namespace
{

constexpr std::uint32_t hyperProjectileType = 1U;
constexpr std::uint32_t mineProjectileType = 11U;

} // namespace

RockCar::Weapons::Weapons(RockCar* owner) noexcept
    : MapObjects(owner)
{
}

MapObj& RockCar::Weapons::Add(
    const Weapon::Desc& description, std::string record)
{
    auto& result = MapObjects::Add(
        GameObjType::Weapon, MapObjCategory::Weapon,
        std::move(record),
        static_cast<std::uint32_t>(GetSlotCount()));
    auto* weapon = result.GetWeapon();
    weapon->SetDesc(description);
    weapon->Reset();
    RefreshSpecial(*weapon);
    return result;
}

Weapon* RockCar::Weapons::GetWeapon(std::size_t slot) noexcept
{
    auto* object = Get(slot);
    return object != nullptr ? object->GetWeapon() : nullptr;
}

const Weapon* RockCar::Weapons::GetWeapon(
    std::size_t slot) const noexcept
{
    const auto* object = Get(slot);
    return object != nullptr ? object->GetWeapon() : nullptr;
}

Weapon* RockCar::Weapons::GetHyperDrive() noexcept
{
    return hyperDrive_;
}

const Weapon* RockCar::Weapons::GetHyperDrive() const noexcept
{
    return hyperDrive_;
}

Weapon* RockCar::Weapons::GetMines() noexcept { return mines_; }

const Weapon* RockCar::Weapons::GetMines() const noexcept
{
    return mines_;
}

void RockCar::Weapons::CopyFrom(const Weapons& value)
{
    Clear();
    Reserve(value.GetLiveCount());
    for (std::size_t slot = 0U;
         slot < value.GetSlotCount(); ++slot)
    {
        const auto* sourceObject = value.Get(slot);
        const auto* sourceWeapon = value.GetWeapon(slot);
        if (sourceObject == nullptr || sourceWeapon == nullptr)
            continue;
        auto& destination = Add(
            sourceWeapon->GetDesc(), sourceObject->GetRecord());
        destination.SetName(sourceObject->GetName());
        destination.SetSourceIndex(sourceObject->GetSourceIndex());
        destination.SetRecord(
            sourceObject->GetRecord(), sourceObject->GetCategory(),
            sourceObject->GetRecordParent());
        *destination.GetWeapon() = *sourceWeapon;
        RefreshSpecial(*destination.GetWeapon());
    }
}

void RockCar::Weapons::InsertItem(MapObj& value)
{
    MapObjects::InsertItem(value);
    if (auto* weapon = value.GetWeapon())
        RefreshSpecial(*weapon);
}

void RockCar::Weapons::RemoveItem(MapObj& value) noexcept
{
    auto* weapon = value.GetWeapon();
    MapObjects::RemoveItem(value);
    if (hyperDrive_ == weapon)
        hyperDrive_ = nullptr;
    if (mines_ == weapon)
        mines_ = nullptr;
}

void RockCar::Weapons::RefreshSpecial(Weapon& value) noexcept
{
    if (value.GetDesc().projectiles.empty())
        return;
    const auto type = value.GetDesc().Front().type;
    if (type == hyperProjectileType)
        hyperDrive_ = &value;
    if (type == mineProjectileType)
        mines_ = &value;
}

RockCar::RockCar() : weapons_(std::make_unique<Weapons>(this)) {}

RockCar::RockCar(const RockCar& value)
    : GameCar(value), weapons_(std::make_unique<Weapons>(this))
{
    weapons_->CopyFrom(*value.weapons_);
}

RockCar& RockCar::operator=(const RockCar& value)
{
    if (this == &value)
        return *this;
    GameCar::operator=(value);
    if (weapons_ == nullptr)
        weapons_ = std::make_unique<Weapons>(this);
    weapons_->CopyFrom(*value.weapons_);
    eventSink_ = nullptr;
    return *this;
}

RockCar::RockCar(RockCar&& value) noexcept
    : RockCar(static_cast<const RockCar&>(value))
{
}

RockCar& RockCar::operator=(RockCar&& value) noexcept
{
    return *this = static_cast<const RockCar&>(value);
}

RockCar::~RockCar() = default;

GameCar::ProgressResult RockCar::OnProgress(float deltaTime) noexcept
{
    auto result = GameCar::OnProgress(deltaTime);
    weapons_->OnProgress(deltaTime);
    return result;
}

RockCar::Weapons& RockCar::GetWeapons() noexcept
{
    return *weapons_;
}

const RockCar::Weapons& RockCar::GetWeapons() const noexcept
{
    return *weapons_;
}

void RockCar::SetEventSink(RockCarEventSink* value) noexcept
{
    eventSink_ = value;
}

RockCarEventSink* RockCar::GetEventSink() noexcept
{
    return eventSink_;
}

const RockCarEventSink* RockCar::GetEventSink() const noexcept
{
    return eventSink_;
}

void RockCar::OnDamageDispatchEvent(
    std::size_t senderPlayerId, float value,
    DamageType damageType) noexcept
{
    if (eventSink_ != nullptr)
        eventSink_->OnRockCarDamageDispatch(
            senderPlayerId, value, damageType);
}

void RockCar::OnKillDispatchEvent(
    std::size_t senderPlayerId, float value,
    DamageType damageType) noexcept
{
    if (eventSink_ != nullptr)
        eventSink_->OnRockCarKillDispatch(
            senderPlayerId, value, damageType);
}

} // namespace r3d::game::originalrace::source
