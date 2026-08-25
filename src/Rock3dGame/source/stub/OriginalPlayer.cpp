#include "OriginalPlayer.h"

#include "OriginalRace.h"
#include "OriginalWeapon.h"

#include <algorithm>
#include <cmath>

namespace r3d::game::originalrace::source
{

const std::array<float, 3> Player::humanEasingMinimumDistance{
    20.0F, 20.0F, 20.0F};
const std::array<float, 3> Player::humanEasingMaximumDistance{
    200.0F, 200.0F, 200.0F};
const std::array<float, 3> Player::humanEasingMinimumSpeed{
    95.0F * 1000.0F / 3600.0F,
    110.0F * 1000.0F / 3600.0F,
    125.0F * 1000.0F / 3600.0F};
const std::array<float, 3> Player::humanEasingMaximumSpeed{
    55.0F * 1000.0F / 3600.0F,
    65.0F * 1000.0F / 3600.0F,
    75.0F * 1000.0F / 3600.0F};
const std::array<float, 3> Player::computerCheatMinimumTorque{
    1.05F, 1.20F, 1.30F};
const std::array<float, 3> Player::computerCheatMaximumTorque{
    1.30F, 1.65F, 1.85F};
const std::array<float, 3> Player::humanArmorScale{
    2.0F, 1.75F, 1.5F};

namespace
{

TraceVec2 xy(const TraceVec3& value) noexcept
{
    return {value.x, value.y};
}

TraceVec3 normalized2(TraceVec3 value) noexcept
{
    const float length = std::sqrt(value.x * value.x + value.y * value.y);
    if (length <= 0.0001F)
        return {1.0F, 0.0F, 0.0F};
    return {value.x / length, value.y / length, 0.0F};
}

TraceVec3 normalized3(TraceVec3 value) noexcept
{
    const float length = std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z);
    if (length <= 0.0001F)
        return {1.0F, 0.0F, 0.0F};
    return {value.x / length, value.y / length, value.z / length};
}

float dot2(const TraceVec2& first, const TraceVec3& second) noexcept
{
    return first.x * second.x + first.y * second.y;
}

} // namespace

void Player::CarState::Reset(Trace* trace) noexcept
{
    trace_ = trace;
    position_ = {};
    direction_ = {1.0F, 0.0F, 0.0F};
    direction3_ = {1.0F, 0.0F, 0.0F};
    speed_ = 0.0F;
    size_ = 0.0F;
    radius_ = 0.0F;
    curTile_ = nullptr;
    curNode_ = nullptr;
    lastNode_ = nullptr;
    lastNodeCoordX_ = 0.5F;
    track_ = 0U;
    numLaps = 0U;
    moveInverse = false;
    cheatSlower = false;
    cheatFaster = false;
    moveInverseStart_ = -1.0F;
    maximumSpeed_ = 0.0F;
    maximumSpeedTime_ = 0.0F;
}

void Player::CarState::OnCreateCar(bool newRace) noexcept
{
    // Player::CreateCar resets per-actor movement diagnostics every time the
    // physical car is recreated. Retaining these values across a death makes
    // the new actor immediately inherit wrong-way/lost-control history.
    moveInverse = false;
    moveInverseStart_ = -1.0F;
    maximumSpeed_ = 0.0F;
    maximumSpeedTime_ = 0.0F;

    if (newRace && trace_ != nullptr)
    {
        auto* path = trace_->GetPath(0U);
        if (path != nullptr && path->GetFirst() != nullptr)
        {
            auto* first = path->GetFirst();
            SetCurTile(first);
            SetCurNode(first);
            SetLastNode(first);
        }
    }
}

void Player::CarState::OnFreeCar(bool freeState) noexcept
{
    if (!freeState)
        return;
    SetCurTile(nullptr);
    SetCurNode(nullptr);
    SetLastNode(nullptr);
    numLaps = 0U;
}

Player::CarState::UpdateResult Player::CarState::Update(
    Trace& trace, const TraceVec3& position,
    const TraceVec3& direction, float vehicleSpeed,
    float deltaTime)
{
    trace_ = &trace;
    position_ = position;
    direction3_ = normalized3(direction);
    direction_ = normalized2(direction);
    speed_ = vehicleSpeed;

    UpdateResult result;
    result.previousLast = GetLastNodeRef();
    SetCurTile(trace.IsTileContains(position_, curTile_));
    if (curTile_ != nullptr && curTile_->GetNext() != nullptr &&
        curTile_->GetNext()->IsContains(position_))
        SetCurNode(curTile_->GetNext());
    else
        SetCurNode(curTile_);

    if (curTile_ != nullptr)
        track_ = curTile_->GetTile().ComputeTrackInd(xy(position_));

    const bool changed = curTile_ != nullptr && lastNode_ != curTile_;
    const bool firstNode = changed && lastNode_ == nullptr;
    const bool linkedNode =
        changed && lastNode_ != nullptr &&
        ((lastNode_->GetNext() != nullptr &&
          lastNode_->GetNext()->GetPoint()->IsFind(curTile_)) ||
         lastNode_->GetPoint()->IsFind(curTile_, lastNode_) ||
         (lastNode_->GetPath()->GetFirst() != nullptr &&
          lastNode_->GetPath()->GetFirst()->GetPoint()->IsFind(
              curTile_, lastNode_->GetPath()->GetFirst())));
    if (firstNode || linkedNode)
    {
        const auto* mainPath = trace.GetPath(0U);
        result.lapPassed =
            lastNode_ != nullptr && mainPath != nullptr &&
            curTile_ == mainPath->GetFirst();
        SetLastNode(curTile_);
        result.lastNodeChanged = true;
    }

    if (lastNode_ != nullptr &&
        lastNode_->GetTile().IsContains(position_))
    {
        lastNodeCoordX_ =
            lastNode_->GetTile().ComputeCoordX(xy(position_));
    }

    if (curTile_ != nullptr &&
        dot2(curTile_->GetTile().GetDir(), direction_) < 0.0F)
    {
        if (moveInverseStart_ < 0.0F)
            moveInverseStart_ = GetDist();
        if (!moveInverse && moveInverseStart_ - GetDist() > 20.0F)
        {
            moveInverse = true;
            result.moveInverseStarted = true;
        }
    }
    else if (curTile_ != nullptr)
    {
        moveInverse = false;
        moveInverseStart_ = -1.0F;
    }

    maximumSpeedTime_ += deltaTime;
    if (speed_ > maximumSpeed_ || maximumSpeedTime_ > 1.0F)
    {
        maximumSpeed_ = speed_;
        maximumSpeedTime_ = 0.0F;
    }
    else if (std::abs(maximumSpeed_ - speed_) < 5.0F)
    {
        maximumSpeedTime_ = 0.0F;
    }
    if (maximumSpeed_ > 0.0F && maximumSpeed_ - speed_ > 80.0F &&
        maximumSpeedTime_ <= 1.0F)
    {
        maximumSpeed_ = speed_;
        maximumSpeedTime_ = 0.0F;
        result.lostControl = true;
    }

    result.currentTile = GetLiveTileRef();
    result.lastNode = GetLastNodeRef();
    return result;
}

WayNode* Player::CarState::GetCurTile(bool lastCorrect) noexcept
{
    return const_cast<WayNode*>(
        static_cast<const CarState*>(this)->GetCurTile(lastCorrect));
}

const WayNode* Player::CarState::GetCurTile(
    bool lastCorrect) const noexcept
{
    return ((lastCorrect || curTile_ == nullptr) && lastNode_ != nullptr)
               ? lastNode_
               : curTile_;
}

WayNode* Player::CarState::GetLiveTile() noexcept { return curTile_; }
const WayNode* Player::CarState::GetLiveTile() const noexcept
{
    return curTile_;
}

WayNode* Player::CarState::GetCurNode() noexcept { return curNode_; }
const WayNode* Player::CarState::GetCurNode() const noexcept
{
    return curNode_;
}
WayNode* Player::CarState::GetLastNode() noexcept { return lastNode_; }
const WayNode* Player::CarState::GetLastNode() const noexcept
{
    return lastNode_;
}

WayNode* Player::CarState::EnsureLastNode() noexcept
{
    // Player::GetLastNode initializes an absent reset anchor from the first
    // node of the main path.  It does not guess from the next checkpoint.
    if (lastNode_ == nullptr && trace_ != nullptr)
    {
        auto* path = trace_->GetPath(0U);
        if (path != nullptr)
            SetLastNode(path->GetFirst());
    }
    return lastNode_;
}

Trace::NodeRef Player::CarState::GetCurTileRef(
    bool lastCorrect) const noexcept
{
    return trace_ != nullptr ? trace_->GetNodeRef(GetCurTile(lastCorrect))
                             : Trace::NodeRef{};
}

Trace::NodeRef Player::CarState::GetLiveTileRef() const noexcept
{
    return trace_ != nullptr ? trace_->GetNodeRef(curTile_)
                             : Trace::NodeRef{};
}

Trace::NodeRef Player::CarState::GetCurNodeRef() const noexcept
{
    return trace_ != nullptr ? trace_->GetNodeRef(curNode_)
                             : Trace::NodeRef{};
}

Trace::NodeRef Player::CarState::GetLastNodeRef() const noexcept
{
    return trace_ != nullptr ? trace_->GetNodeRef(lastNode_)
                             : Trace::NodeRef{};
}

std::int32_t Player::CarState::GetPathIndex(
    bool lastCorrect) const noexcept
{
    const auto reference = GetCurTileRef(lastCorrect);
    return reference.valid() ? static_cast<std::int32_t>(reference.path)
                             : -1;
}

bool Player::CarState::IsMainPath(bool lastCorrect) const noexcept
{
    return GetPathIndex(lastCorrect) == 0;
}

float Player::CarState::GetPathLength(bool lastCorrect) const noexcept
{
    const auto* tile = GetCurTile(lastCorrect);
    if (tile != nullptr)
        return tile->GetPath()->GetLength();
    const auto* mainPath = trace_ != nullptr ? trace_->GetPath(0U) : nullptr;
    return mainPath != nullptr ? mainPath->GetLength() : 1.0F;
}

float Player::CarState::GetDist(bool lastCorrect) const noexcept
{
    const auto* tile = GetCurTile(lastCorrect);
    if (tile == nullptr)
        return 0.0F;
    return tile->GetPath()->GetLength() -
           (tile->GetTile().GetFinishDist() -
            tile->GetTile().GetLength(xy(position_)));
}

float Player::CarState::GetLap(bool lastCorrect) const noexcept
{
    const float pathLength = GetPathLength(lastCorrect);
    return static_cast<float>(numLaps) +
           (pathLength > 0.0001F ? GetDist(lastCorrect) / pathLength
                                 : 0.0F);
}

float Player::CarState::GetSpeed() const noexcept
{
    return speed_;
}

void Player::CarState::SetSize(float value) noexcept
{
    size_ = std::max(value, 0.0F);
    radius_ = size_ * 0.5F;
}

float Player::CarState::GetSize() const noexcept
{
    return size_;
}

float Player::CarState::GetRadius() const noexcept
{
    return radius_;
}

TraceVec3 Player::CarState::GetPosition() const noexcept
{
    return position_;
}

TraceVec3 Player::CarState::GetDirection3() const noexcept
{
    return direction3_;
}

TraceVec3 Player::CarState::GetMapPos() const noexcept
{
    if (curTile_ != nullptr)
    {
        return curTile_->GetTile().GetPoint(
            curTile_->GetTile().ComputeCoordX(xy(position_)));
    }
    if (lastNode_ != nullptr)
        return lastNode_->GetTile().GetPoint(lastNodeCoordX_);
    return {};
}

float Player::CarState::GetLastNodeCoordX() const noexcept
{
    return lastNodeCoordX_;
}

std::uint32_t Player::CarState::GetTrack() const noexcept
{
    return track_;
}

void Player::CarState::SetCurTile(WayNode* value) noexcept
{
    curTile_ = value;
}

void Player::CarState::SetCurNode(WayNode* value) noexcept
{
    curNode_ = value;
}

void Player::CarState::SetLastNode(WayNode* value) noexcept
{
    if (lastNode_ == value)
        return;
    lastNode_ = value;
    lastNodeCoordX_ = 0.5F;
}

void Player::Reset(float newMaximumLife,
                   std::uint32_t initialPlace,
                   Trace* trace) noexcept
{
    *this = Player{};
    ResetGameObject(std::max(newMaximumLife, 1.0F));
    SetPlace(initialPlace);
    car.Reset(trace);
}

void Player::ConfigureIdentity(
    int playerId, int sourceGamerId, unsigned sourceNetSlot,
    std::string sourceName, std::string sourceNetName,
    const std::array<float, 4>& sourceColor)
{
    id_ = playerId;
    gamerId_ = sourceGamerId;
    netSlot_ = sourceNetSlot;
    name_ = std::move(sourceName);
    netName_ = std::move(sourceNetName);
    color_ = sourceColor;
}

int Player::GetId() const noexcept { return id_; }

int Player::GetGamerId() const noexcept { return gamerId_; }

unsigned Player::GetNetSlot() const noexcept { return netSlot_; }

const std::string& Player::GetNetName() const noexcept { return netName_; }

const std::string& Player::GetName() const noexcept
{
    return netName_.empty() ? name_ : netName_;
}

const std::array<float, 4>& Player::GetColor() const noexcept
{
    return color_;
}

void Player::SetId(int value) noexcept { id_ = value; }

void Player::SetGamerId(int value) noexcept { gamerId_ = value; }

void Player::SetNetSlot(unsigned value) noexcept { netSlot_ = value; }

void Player::SetNetName(std::string value)
{
    netName_ = std::move(value);
}

void Player::SetName(std::string value) { name_ = std::move(value); }

void Player::SetColor(const std::array<float, 4>& value) noexcept
{
    color_ = value;
}

bool Player::IsHuman() const noexcept { return id_ == humanId; }

bool Player::IsComputer() const noexcept
{
    return (id_ & computerMask) != 0;
}

bool Player::IsOpponent() const noexcept
{
    return (id_ & opponentMask) != 0;
}

bool Player::IsHumanOrOpponent() const noexcept
{
    return IsHuman() || IsOpponent();
}

std::uint32_t Player::GetCheat() const noexcept
{
    return cheatEnable_;
}

void Player::SetCheat(std::uint32_t value) noexcept
{
    cheatEnable_ = value;
}

Player::HeadLightMode Player::GetHeadLight() const noexcept
{
    return headLight_;
}

void Player::SetHeadlight(HeadLightMode value) noexcept
{
    headLight_ = value;
}

bool Player::HasCar() const noexcept
{
    return carPresent_;
}

bool Player::HasAttachedLights() const noexcept
{
    return carPresent_ && headLight_ != HeadLightMode::None;
}

bool Player::GetReflScene() const noexcept
{
    return reflScene_;
}

void Player::SetReflScene(bool value) noexcept
{
    reflScene_ = value;
}

const Vehicle* Player::GetCarRecord() const noexcept
{
    return carRecord_;
}

void Player::SetCar(const Vehicle* record) noexcept
{
    if (carRecord_ == record)
        return;
    // Object::ReplaceRef in the Windows code releases the live MapObj and
    // clears the complete CarState before replacing its MapObjRec pointer.
    FreeCar(true);
    carRecord_ = record;
}

void Player::CreateCar(bool newRace) noexcept
{
    carPresent_ = true;
    car.OnCreateCar(newRace);
    Resc();
    for (std::size_t slot = 0U; slot < weaponSlotCount; ++slot)
    {
        auto& item = slotRack_.GetSlot(
            static_cast<PlayerSlotType>(
                static_cast<std::size_t>(PlayerSlotType::Weapon1) + slot))
                         .GetItem();
        if (auto* droid = dynamic_cast<DroidItem*>(&item))
            droid->OnCreateCar();
    }
    if (!newRace)
        return;
    ClearBonusProjectiles();
    nextBonusProjectileId_ = 1U;
    restoreSeconds = 0.0F;
}

void Player::FreeCar(bool freeState) noexcept
{
    if (carPresent_)
    {
        for (std::size_t slot = 0U; slot < weaponSlotCount; ++slot)
        {
            auto& item = slotRack_.GetSlot(
                static_cast<PlayerSlotType>(
                    static_cast<std::size_t>(PlayerSlotType::Weapon1) +
                    slot)).GetItem();
            if (auto* droid = dynamic_cast<DroidItem*>(&item))
                droid->OnDestroyCar();
        }
    }
    carPresent_ = false;
    car.OnFreeCar(freeState);
}

void Player::OnLapPass(std::size_t weaponDefinitionCount) noexcept
{
    ++car.numLaps;
    ReloadWeapons(weaponDefinitionCount);
}

void Player::ReloadWeapons(
    std::size_t weaponDefinitionCount) noexcept
{
    // Player.cpp iterates the six physical weapon Slots and reloads their
    // resident WeaponItem objects. The portable Player now has the same
    // ownership, so no temporary charge-only wrappers are required.
    if (auto* item = GetHyperWeaponItem())
        item->Reload();
    if (auto* item = GetMineWeaponItem())
        item->Reload();
    for (auto* item : GetPrimaryWeaponItems())
    {
        if (item != nullptr)
            item->Reload();
    }
    SyncSelectedWeapon(weaponDefinitionCount);
}

void Player::BindWeaponItems(
    std::span<const WeaponDefinition> definitions) noexcept
{
    auto itemType = [](const WeaponDefinition& definition) noexcept {
        return static_cast<SlotType>(definition.itemType);
    };
    auto ensureItem = [&](PlayerSlotType physicalType,
                          std::size_t definitionIndex)
        -> WeaponItem* {
        auto& physicalSlot = slotRack_.GetSlot(physicalType);
        if (definitionIndex == invalidWeapon ||
            definitionIndex >= definitions.size())
        {
            return physicalSlot.GetItem().IsWeaponItem();
        }
        const auto expectedType = itemType(definitions[definitionIndex]);
        auto* item = physicalSlot.GetItem().IsWeaponItem();
        if (item == nullptr || physicalSlot.GetType() != expectedType)
            item = physicalSlot.CreateItem(expectedType).IsWeaponItem();
        return item;
    };
    auto bind = [&](WeaponItem* item, Weapon* weapon,
                    std::size_t definitionIndex,
                    std::uint32_t countCharge,
                    std::uint32_t* currentCharge) {
        if (item == nullptr)
            return;
        if (definitionIndex == invalidWeapon ||
            definitionIndex >= definitions.size())
        {
            item->Bind(nullptr, 0U, 0U, nullptr);
            return;
        }
        const auto& definition = definitions[definitionIndex];
        item->Bind(
            weapon, definition.maximumCharge, countCharge,
            currentCharge, definition.chargeStep, definition.damage);
    };

    for (std::size_t slot = 0U; slot < weaponSlotCount; ++slot)
    {
        const auto physicalType = static_cast<PlayerSlotType>(
            static_cast<std::size_t>(PlayerSlotType::Weapon1) + slot);
        auto* item = ensureItem(physicalType, weaponSlots[slot]);
        bind(item, &weaponRack_.primary[slot],
             weaponSlots[slot], weaponCapacity[slot],
             &weaponCharges[slot]);
        const std::size_t definitionIndex = weaponSlots[slot];
        if (definitionIndex == invalidWeapon ||
            definitionIndex >= definitions.size())
            continue;
        const auto& definition = definitions[definitionIndex];
        if (auto* droid = dynamic_cast<DroidItem*>(item))
        {
            droid->Bind(
                &weaponRack_.primary[slot],
                definition.maximumCharge, weaponCapacity[slot],
                &weaponCharges[slot], definition.repairValue,
                definition.repairPeriod);
        }
        else if (auto* reflector = dynamic_cast<ReflectorItem*>(item))
        {
            reflector->Bind(
                &weaponRack_.primary[slot],
                definition.maximumCharge, weaponCapacity[slot],
                &weaponCharges[slot], definition.reflectValue);
        }
    }
    bind(ensureItem(PlayerSlotType::Hyper, hyperWeapon),
         &weaponRack_.hyper, hyperWeapon, hyperCapacity, &hyperCharge);
    bind(ensureItem(PlayerSlotType::Mine, mineWeapon),
         &weaponRack_.mine, mineWeapon, mineCapacity, &mines);
}

std::array<WeaponItem*, Player::weaponSlotCount>
Player::GetPrimaryWeaponItems() noexcept
{
    std::array<WeaponItem*, weaponSlotCount> result{};
    for (std::size_t slot = 0U; slot < result.size(); ++slot)
    {
        result[slot] = slotRack_.GetSlot(
            static_cast<PlayerSlotType>(
                static_cast<std::size_t>(PlayerSlotType::Weapon1) + slot))
                           .GetItem().IsWeaponItem();
    }
    return result;
}

std::array<const WeaponItem*, Player::weaponSlotCount>
Player::GetPrimaryWeaponItems() const noexcept
{
    std::array<const WeaponItem*, weaponSlotCount> result{};
    for (std::size_t slot = 0U; slot < result.size(); ++slot)
    {
        result[slot] = slotRack_.GetSlot(
            static_cast<PlayerSlotType>(
                static_cast<std::size_t>(PlayerSlotType::Weapon1) + slot))
                           .GetItem().IsWeaponItem();
    }
    return result;
}

WeaponItem* Player::GetHyperWeaponItem() noexcept
{
    return slotRack_.GetSlot(PlayerSlotType::Hyper)
        .GetItem().IsWeaponItem();
}

const WeaponItem* Player::GetHyperWeaponItem() const noexcept
{
    return slotRack_.GetSlot(PlayerSlotType::Hyper)
        .GetItem().IsWeaponItem();
}

WeaponItem* Player::GetMineWeaponItem() noexcept
{
    return slotRack_.GetSlot(PlayerSlotType::Mine)
        .GetItem().IsWeaponItem();
}

const WeaponItem* Player::GetMineWeaponItem() const noexcept
{
    return slotRack_.GetSlot(PlayerSlotType::Mine)
        .GetItem().IsWeaponItem();
}

float Player::ReflectDamage(float value) const noexcept
{
    const auto* slot = GetSlotInst(SlotType::Reflector);
    const auto* reflector = slot == nullptr
                                ? nullptr
                                : dynamic_cast<const ReflectorItem*>(
                                      &slot->GetItem());
    return reflector == nullptr ? value : reflector->Reflect(value);
}

void Player::SyncSelectedWeapon(
    std::size_t weaponDefinitionCount) noexcept
{
    for (std::size_t offset = 0U; offset < weaponSlots.size(); ++offset)
    {
        const auto slot =
            (selectedWeaponSlot + offset) % weaponSlots.size();
        const auto weapon = weaponSlots[slot];
        if (weapon == invalidWeapon || weapon >= weaponDefinitionCount)
            continue;
        selectedWeaponSlot = slot;
        selectedWeapon = weapon;
        ammunition = weaponCharges[slot];
        return;
    }
    selectedWeapon = invalidWeapon;
    ammunition = 0U;
}

bool Player::Shot(WeaponItem& item, bool projectileCreated,
                  bool mineSlot, std::uint32_t projectileId,
                  int newCharge) noexcept
{
    // Player::Shot is the transaction owner in the Windows source. The
    // renderer/physics adapter reports whether Weapon::CreateShot prepared
    // at least one projectile; WeaponItem then commits the exact local or
    // replicated charge. Only a successfully prepared stMine projectile is
    // retained in Player::_bonusProjs and advances _nextBonusProjId.
    const bool result = item.Shot(projectileCreated, newCharge);
    if (result && mineSlot)
        InsertBonusProjectile(projectileId);
    return result;
}

WeaponRack& Player::GetWeaponRack() noexcept
{
    return weaponRack_;
}

const WeaponRack& Player::GetWeaponRack() const noexcept
{
    return weaponRack_;
}

void Player::BindSlots(
    const std::vector<OriginalWorkshopItem>& workshop,
    const std::vector<RacerSlot>& loadout)
{
    if (carPresent_)
    {
        for (auto* item : GetPrimaryWeaponItems())
        {
            if (auto* droid = dynamic_cast<DroidItem*>(item))
                droid->OnDestroyCar();
        }
    }
    slotRack_.Bind(workshop, loadout);
    if (carRecord_ != nullptr)
    {
        for (std::size_t slot = 0U;
             slot < PlayerSlotRack::slotCount; ++slot)
        {
            auto& physicalSlot = slotRack_.GetSlot(
                static_cast<PlayerSlotType>(slot));
            const auto* record = physicalSlot.GetRecord();
            if (record == nullptr)
                continue;
            const auto& mount = carRecord_->slotMounts[slot];
            const auto slash = record->record.find_last_of("\\/");
            std::string_view wanted = record->record;
            wanted.remove_prefix(
                slash == std::string::npos ? 0U : slash + 1U);
            const auto placement = std::find_if(
                mount.placements.begin(), mount.placements.end(),
                [&](const VehicleSlotPlacement& item) {
                    const auto itemSlash =
                        item.record.find_last_of("\\/");
                    return item.record.substr(
                               itemSlash == std::string::npos
                                   ? 0U
                                   : itemSlash + 1U) == wanted;
                });
            if (placement == mount.placements.end())
                continue;
            auto& item = physicalSlot.GetItem();
            item.SetPos(
                {mount.position.x + placement->offset.x,
                 mount.position.y + placement->offset.y,
                 mount.position.z + placement->offset.z});
            item.SetRot(
                {placement->rotation.x, placement->rotation.y,
                 placement->rotation.z, placement->rotation.w});
        }
    }
    if (carPresent_)
    {
        for (auto* item : GetPrimaryWeaponItems())
        {
            if (auto* droid = dynamic_cast<DroidItem*>(item))
                droid->OnCreateCar();
        }
    }
}

void Player::SetSlot(
    PlayerSlotType type, const OriginalWorkshopItem* record,
    const std::array<float, 3>& position,
    const std::array<float, 4>& rotation) noexcept
{
    auto& slot = slotRack_.GetSlot(type);
    if (carPresent_)
    {
        if (auto* droid = dynamic_cast<DroidItem*>(&slot.GetItem()))
            droid->OnDestroyCar();
    }
    slot.SetRecord(record);
    if (record == nullptr)
        return;
    slot.GetItem().SetPos(position);
    slot.GetItem().SetRot(rotation);
    if (carPresent_)
    {
        if (auto* droid = dynamic_cast<DroidItem*>(&slot.GetItem()))
            droid->OnCreateCar();
    }
}

void Player::ApplyMobility(
    Vehicle& vehicle, std::string_view difficulty,
    bool humanOrOpponent, bool armor4Opened) noexcept
{
    slotRack_.ApplyMobility(
        vehicle, difficulty, humanOrOpponent, armor4Opened);
}

const OriginalWorkshopItem* Player::GetSlot(
    PlayerSlotType type) const noexcept
{
    return slotRack_.GetSlot(type).GetRecord();
}

Slot* Player::GetSlotInst(PlayerSlotType type) noexcept
{
    auto& slot = slotRack_.GetSlot(type);
    return slot.GetRecord() == nullptr ? nullptr : &slot;
}

const Slot* Player::GetSlotInst(PlayerSlotType type) const noexcept
{
    const auto& slot = slotRack_.GetSlot(type);
    return slot.GetRecord() == nullptr ? nullptr : &slot;
}

Slot* Player::GetSlotInst(SlotType type) noexcept
{
    return slotRack_.GetSlotInst(type);
}

const Slot* Player::GetSlotInst(SlotType type) const noexcept
{
    return slotRack_.GetSlotInst(type);
}

std::uint32_t Player::GetMoney() const noexcept { return money_; }

void Player::SetMoney(std::uint32_t value) noexcept { money_ = value; }

std::uint32_t Player::GetPoints() const noexcept { return points_; }

void Player::SetPoints(std::uint32_t value) noexcept { points_ = value; }

std::uint32_t Player::GetPickMoney() const noexcept
{
    return pickedMoney_;
}

void Player::ResetPickMoney() noexcept { pickedMoney_ = 0U; }

std::uint32_t Player::GetPlace() const noexcept { return place_; }

void Player::SetPlace(std::uint32_t value) noexcept { place_ = value; }

bool Player::GetFinished() const noexcept { return finished_; }

PlayerBonusResult Player::TakeMoney(float value) noexcept
{
    const auto amount = static_cast<std::uint32_t>(
        std::max(value, 0.0F));
    pickedMoney_ += amount;
    return {PlayerBonusSlot::None, invalidWeapon, amount};
}

PlayerBonusResult Player::TakeMedpack(float value) noexcept
{
    const float previous = life;
    Healt(value);
    return {PlayerBonusSlot::None, invalidWeapon,
            static_cast<std::uint32_t>(
                std::max(life - previous, 0.0F))};
}

PlayerBonusResult Player::TakeImmortal(float value) noexcept
{
    Immortal(std::max(value, 0.0F));
    return {PlayerBonusSlot::None, invalidWeapon,
            static_cast<std::uint32_t>(shieldSeconds)};
}

PlayerBonusResult Player::TakeAmmunition(
    float value,
    const std::vector<std::uint32_t>& maximumCharges,
    float randomUnit) noexcept
{
    struct RechargeTarget
    {
        std::uint32_t* current = nullptr;
        std::uint32_t capacity = 0U;
        std::size_t weapon = invalidWeapon;
        PlayerBonusSlot slot = PlayerBonusSlot::None;
    };
    std::vector<RechargeTarget> targets;
    targets.reserve(weaponSlots.size() + 2U);
    if (hyperWeapon != invalidWeapon &&
        hyperWeapon < maximumCharges.size() &&
        hyperCharge < hyperCapacity)
    {
        targets.push_back(
            {&hyperCharge, hyperCapacity, hyperWeapon,
             PlayerBonusSlot::Hyper});
    }
    if (mineWeapon != invalidWeapon &&
        mineWeapon < maximumCharges.size() &&
        mines < mineCapacity)
    {
        targets.push_back(
            {&mines, mineCapacity, mineWeapon,
             PlayerBonusSlot::Mine});
    }
    for (std::size_t slot = 0U; slot < weaponSlots.size(); ++slot)
    {
        const auto weapon = weaponSlots[slot];
        if (weapon == invalidWeapon || weapon >= maximumCharges.size() ||
            weaponCharges[slot] >= weaponCapacity[slot])
            continue;
        targets.push_back(
            {&weaponCharges[slot], weaponCapacity[slot], weapon,
             PlayerBonusSlot::Primary});
    }
    PlayerBonusResult result;
    if (!targets.empty())
    {
        auto& target = targets[RoundedRandomIndex(
            targets.size(), randomUnit)];
        const auto amount = BonusCharge(
            maximumCharges[target.weapon], value);
        *target.current = std::min(
            *target.current + amount, target.capacity);
        result = {target.slot, target.weapon, amount};
    }
    SyncSelectedWeapon(maximumCharges.size());
    return result;
}

PlayerBonusResult Player::TakeBonus(
    PlayerBonusType type, float value,
    const std::vector<std::uint32_t>& maximumCharges,
    float randomUnit) noexcept
{
    switch (type)
    {
    case PlayerBonusType::Money:
        return TakeMoney(value);
    case PlayerBonusType::Charge:
        return TakeAmmunition(
            value, maximumCharges, randomUnit);
    case PlayerBonusType::Medpack:
        return TakeMedpack(value);
    case PlayerBonusType::Immortal:
        return TakeImmortal(value);
    }
    return {};
}

PlayerBonusResult Player::TakeBonus(
    GameObject& bonus, PlayerBonusType type, float value,
    const std::vector<std::uint32_t>& maximumCharges,
    float randomUnit) noexcept
{
    // Player::TakeBonus owns this transition in the source.  In particular,
    // bonus death listeners run before money/life/charge state is changed.
    bonus.Death();
    return TakeBonus(type, value, maximumCharges, randomUnit);
}

Player::BehaviorProgressResult Player::ProgressBehaviors(
    float deltaTime, float lowLifeLevel, float linearSpeed) noexcept
{
    BehaviorProgressResult result;
    result.gameObject = GameObject::OnProgress(deltaTime);
    for (std::size_t slot = 0U; slot < weaponSlotCount; ++slot)
    {
        auto& item = slotRack_.GetSlot(
            static_cast<PlayerSlotType>(
                static_cast<std::size_t>(PlayerSlotType::Weapon1) + slot))
                         .GetItem();
        if (auto* droid = dynamic_cast<DroidItem*>(&item))
            droid->OnProgress(
                deltaTime, life, maximumLife, destroyed);
    }
    energyDamageEffect.OnProgress(deltaTime);
    immortalEffect.OnProgress(deltaTime);
    lowLifePoints.SetLifeLevel(lowLifeLevel);
    const auto lowLife = lowLifePoints.OnProgress(*this, deltaTime);
    result.lowLifeActivated = lowLife.activated;
    result.lowLifeReleased = lowLife.released;
    const auto slow = slowEffect.OnProgress(deltaTime, linearSpeed);
    result.slowSpeedLimited = slow.limitSpeed;
    result.slowReleased = slow.released;
    return result;
}

Player::CheatResult Player::CheatUpdate(
    std::uint32_t cheatMask, std::size_t playerId,
    std::size_t difficulty,
    const std::vector<CheatPlayerView>& players) noexcept
{
    difficulty = std::min<std::size_t>(difficulty, 2U);
    car.cheatFaster = false;
    car.cheatSlower = false;
    CheatResult result;
    if (cheatMask == cheatDisabled)
        return result;

    const float ownLap = car.GetLap();
    const CheatPlayerView* opponent = nullptr;
    float maximumLapDistance = 0.0F;
    for (const auto& player : players)
    {
        // Race::PlayerList contains computers too, but Player::CheatUpdate
        // only accepts cHuman and cOpponentMask roles as references.
        if (!player.active || player.playerId == playerId ||
            !player.humanOrOpponent)
            continue;
        const float lapDistance = player.lap - ownLap;
        if (opponent == nullptr || maximumLapDistance < lapDistance)
        {
            opponent = &player;
            maximumLapDistance = lapDistance;
        }
    }
    if (opponent == nullptr)
        return result;

    float distance = std::abs(ownLap - opponent->lap);
    distance -= std::floor(distance);
    distance = std::min(distance, 1.0F - distance) *
               std::max(car.GetPathLength(), 1.0F);
    if (distance <= humanEasingMinimumDistance[difficulty])
        return result;
    const float distancePart = std::clamp(
        (distance - humanEasingMinimumDistance[difficulty]) /
            (humanEasingMaximumDistance[difficulty] -
             humanEasingMinimumDistance[difficulty]),
        0.0F, 1.0F);
    if (ownLap > opponent->lap &&
        (cheatMask & cheatEnableSlower) != 0U)
    {
        result.speedLimit =
            humanEasingMinimumSpeed[difficulty] +
            (humanEasingMaximumSpeed[difficulty] -
             humanEasingMinimumSpeed[difficulty]) *
                distancePart;
        if (car.GetSpeed() > result.speedLimit)
        {
            result.slower = true;
            car.cheatSlower = true;
        }
    }
    else if ((cheatMask & cheatEnableFaster) != 0U)
    {
        result.torqueScale =
            computerCheatMinimumTorque[difficulty] +
            (computerCheatMaximumTorque[difficulty] -
             computerCheatMinimumTorque[difficulty]) *
                distancePart;
        result.steeringScale = result.torqueScale;
        result.faster = true;
        car.cheatFaster = true;
    }
    return result;
}

Player::ProgressResult Player::OnProgress(
    float deltaTime, bool carPresent,
    std::uint32_t cheatMask, std::size_t playerId,
    std::size_t difficulty,
    const std::vector<CheatPlayerView>& players) noexcept
{
    ProgressResult result;
    if (carPresent)
    {
        // CarState::Update is executed by the physics adapter immediately
        // before this call.  The remaining order is the original
        // Player::OnProgress order: CheatUpdate, restore/reset fallback and
        // finally the finish/start block move.
        result.cheat = CheatUpdate(
            cheatMask, playerId, difficulty, players);
    }
    else
    {
        result.restore = ProgressRestore(deltaTime);
        car.cheatFaster = false;
        car.cheatSlower = false;
    }

    result.blockMove = ProgressBlock(deltaTime);
    return result;
}

Player* Player::FindClosestEnemy(
    float viewAngle, bool zTest,
    std::span<Player* const> players) noexcept
{
    // The Windows method tests CarState::mapObj here. During a lethal contact
    // callback that MapObj still exists until deferred object destruction,
    // even though portable GameObject damage state has already flipped. The
    // retained CarState pose is therefore the backend-neutral live-mapObj
    // boundary for the source player in this synchronous search.
    const TraceVec3 carPosition = car.GetPosition();
    const TraceVec3 carDirection = car.GetDirection3();
    const WayNode* liveTile = car.GetLiveTile();
    Player* enemy = nullptr;
    float minimumPlaneDistance = 0.0F;
    constexpr float halfPi = 1.57079632679489661923F;

    for (Player* candidate : players)
    {
        if (candidate == nullptr || candidate == this ||
            candidate->destroyed || candidate->disconnected)
        {
            continue;
        }
        const TraceVec3 enemyPosition = candidate->car.GetPosition();
        if (zTest && liveTile != nullptr &&
            !liveTile->GetTile().IsZLevelContains(enemyPosition))
        {
            continue;
        }

        const TraceVec3 difference{
            enemyPosition.x - carPosition.x,
            enemyPosition.y - carPosition.y,
            enemyPosition.z - carPosition.z};
        const float length = std::sqrt(
            difference.x * difference.x +
            difference.y * difference.y +
            difference.z * difference.z);
        if (length <= 0.0001F)
            continue;
        const float alignment =
            (difference.x * carDirection.x +
             difference.y * carDirection.y +
             difference.z * carDirection.z) /
            length;
        const float planeDistance = std::abs(
            difference.x * carDirection.x +
            difference.y * carDirection.y +
            difference.z * carDirection.z);
        const bool nearest =
            enemy == nullptr || planeDistance < minimumPlaneDistance;
        const bool insideView =
            viewAngle == 0.0F ||
            (viewAngle > 0.0F
                 ? alignment >= std::cos(viewAngle)
                 : alignment <= std::cos(halfPi - viewAngle));
        if (!nearest || !insideView)
            continue;
        enemy = candidate;
        minimumPlaneDistance = planeDistance;
    }
    return enemy;
}

void Player::InsertBonusProjectile(std::uint32_t projectileId)
{
    // Player::InsertBonusProj is reached only from Player::Shot(stMine).
    // Ordinary weapon/hyper shots reuse the current RPC id and do not advance
    // this owner-specific sequence.
    nextBonusProjectileId_ = projectileId + 1U;
    bonusProjectileIds_.push_back(projectileId);
}

bool Player::RemoveBonusProjectile(
    std::uint32_t projectileId) noexcept
{
    const auto found = std::find(
        bonusProjectileIds_.begin(), bonusProjectileIds_.end(),
        projectileId);
    if (found == bonusProjectileIds_.end())
        return false;
    bonusProjectileIds_.erase(found);
    return true;
}

void Player::ClearBonusProjectiles() noexcept
{
    bonusProjectileIds_.clear();
}

bool Player::HasBonusProjectile(
    std::uint32_t projectileId) const noexcept
{
    return std::find(
               bonusProjectileIds_.begin(), bonusProjectileIds_.end(),
               projectileId) != bonusProjectileIds_.end();
}

std::uint32_t Player::GetNextBonusProjectileId() const noexcept
{
    return nextBonusProjectileId_;
}

bool Player::ConsumeEnergyDamageEffectCreated() noexcept
{
    const bool result = energyDamageEffectCreated_;
    energyDamageEffectCreated_ = false;
    return result;
}

std::vector<PlayerGameEvent> Player::TakeGameEvents() noexcept
{
    std::vector<PlayerGameEvent> result;
    result.swap(gameEvents_);
    return result;
}

void Player::OnDeathEvent(
    DamageType damageType, GameObject* target) noexcept
{
    (void)target;
    if (damageType == DamageType::DeathPlane)
    {
        gameEvents_.push_back(
            {PlayerGameEventKind::Overboard,
             GameObject::undefinedPlayerId, 0.0F, damageType});
    }
    else if (damageType == DamageType::Mine)
    {
        gameEvents_.push_back(
            {PlayerGameEventKind::DeathMine,
             GameObject::undefinedPlayerId, 0.0F, damageType});
    }
    gameEvents_.push_back(
        {PlayerGameEventKind::Death, GetTouchPlayerId(), 0.0F,
         damageType});
}

void Player::OnDamageEvent(
    float value, DamageType damageType) noexcept
{
    (void)value;
    // Behaviors are GameObjListeners in the source and receive even damage
    // absorbed by timed or permanent immortality.
    immortalEffect.OnDamage();
    energyDamageEffectCreated_ =
        energyDamageEffect.OnDamage(damageType);
}

void Player::OnDamageDispatchEvent(
    std::size_t senderPlayerId, float value,
    DamageType damageType) noexcept
{
    gameEvents_.push_back(
        {PlayerGameEventKind::Damage, senderPlayerId, value,
         damageType});
}

void Player::OnKillDispatchEvent(
    std::size_t senderPlayerId, float value,
    DamageType damageType) noexcept
{
    gameEvents_.push_back(
        {PlayerGameEventKind::Kill, senderPlayerId, value,
         damageType});
}

void Player::OnImmortalStatusEvent(bool status) noexcept
{
    immortalEffect.OnImmortalStatus(status);
}

void Player::SetFinished(bool value, float time) noexcept
{
    finished_ = value;
    SetImmortalFlag(value);
    if (value)
        finishTime = time;
    else
        finishTime = -1.0F;
}

void Player::Complete(std::uint32_t resultPlace,
                      std::uint32_t resultMoney,
                      std::uint32_t resultPoints,
                      float time) noexcept
{
    SetFinished(true, time);
    SetPlace(resultPlace);
    rewardMoney = resultMoney;
    rewardPoints = resultPoints;
}

void Player::AddMoney(std::int32_t value) noexcept
{
    const auto result = static_cast<std::int64_t>(GetMoney()) + value;
    SetMoney(static_cast<std::uint32_t>(
        std::max<std::int64_t>(result, 0)));
}

void Player::AddPoints(std::int32_t value) noexcept
{
    const auto result = static_cast<std::int64_t>(GetPoints()) + value;
    SetPoints(static_cast<std::uint32_t>(
        std::max<std::int64_t>(result, 0)));
}

void Player::ApplyRaceReward() noexcept
{
    AddMoney(static_cast<std::int32_t>(
        rewardMoney + GetPickMoney()));
    AddPoints(static_cast<std::int32_t>(rewardPoints));
}

void Player::Destroy() noexcept
{
    SetLife(0.0F);
    Death();
    lowLifePoints.Reset(lowLifePoints.GetLifeLevel());
    energyDamageEffect.Reset();
    immortalEffect.Reset();
    slowEffect.Reset();
    gameCar.Reset();
    Immortal(0.0F);
    touchAttacker = undefinedPlayerId;
    touchAttributionSeconds = 0.0F;
    restoreSeconds = restoreCarSeconds;
    // GameObject::Death destroys the MapObj immediately afterwards. The
    // portable owner combines that OnDestroy callback here so render/audio
    // adapters cannot retain child state during the restore delay.
    FreeCar(false);
}

PlayerRestoreStep Player::ProgressRestore(float seconds) noexcept
{
    if (!destroyed)
        return PlayerRestoreStep::None;
    if (restoreSeconds < 0.0F)
    {
        restoreSeconds = 0.0F;
        CreateCar(false);
        return PlayerRestoreStep::ActivateCar;
    }
    restoreSeconds = std::max(0.0F, restoreSeconds - seconds);
    if (restoreSeconds > 0.0F)
        return PlayerRestoreStep::None;
    SetLife(maximumLife);
    restoreSeconds = -1.0F;
    return PlayerRestoreStep::QueueRespawn;
}

ResetCarPose Player::ResetCar(const ResetCarRayCast& rayCast)
{
    ResetCarPose result;
    WayNode* node = car.EnsureLastNode();
    if (node == nullptr || node->GetNext() == nullptr)
        return result;

    float distance = node->GetTile().ComputeLength(
        std::clamp(car.GetLastNodeCoordX(), 0.0F, 1.0F));
    constexpr std::array<float, 3> offsets{0.0F, -2.0F, 2.0F};
    bool initialDeathPlane = false;

    for (int attempt = 0; attempt < 5; ++attempt)
    {
        bool found = false;
        TraceVec3 candidatePosition{};
        TraceVec3 candidateDirection{1.0F, 0.0F, 0.0F};
        int sample = 0;
        while (sample < static_cast<int>(offsets.size()))
        {
            const auto& tile = node->GetTile();
            const float coordinate = tile.ComputeCoordX(
                distance + offsets[static_cast<std::size_t>(sample)]);
            TraceVec3 rayPosition = tile.GetPoint(coordinate);
            // The Windows code deliberately samples the tile height at its
            // midpoint for all three longitudinal rays.  Using the current
            // coordinate moves the ray origin on tapered trace tiles.
            rayPosition.z += tile.ComputeHeight(0.5F) * 0.5F;
            const auto tileDirection = tile.GetDir();
            const TraceVec3 direction{
                tileDirection.x, tileDirection.y, 0.0F};

            if (attempt == 0 && sample == 0)
            {
                result.valid = true;
                result.position = rayPosition;
                result.direction = direction;
                if (const auto* trace = node->GetPath()->GetTrace())
                    result.node = trace->GetNodeRef(node);
            }
            if (sample == 0)
            {
                candidatePosition = rayPosition;
                candidateDirection = direction;
            }

            const ResetCarRayKind hit =
                rayCast ? rayCast(rayPosition) : ResetCarRayKind::None;
            if (!initialDeathPlane && attempt == 0 && sample == 0 &&
                (hit == ResetCarRayKind::None ||
                 hit == ResetCarRayKind::DeathPlane))
            {
                // ResetCar restarts the same three samples at the beginning
                // of the tile after the initial position lies over a hole.
                initialDeathPlane = true;
                distance = 0.0F;
                sample = 0;
                continue;
            }
            if (hit != ResetCarRayKind::TrackPlane &&
                hit != ResetCarRayKind::OwnCar)
            {
                break;
            }
            if (sample == static_cast<int>(offsets.size()) - 1)
            {
                found = true;
                result.position = candidatePosition;
                result.direction = candidateDirection;
                if (const auto* trace = node->GetPath()->GetTrace())
                    result.node = trace->GetNodeRef(node);
            }
            ++sample;
        }
        if (found)
            break;

        const float previousDistance = distance - 6.0F;
        if (previousDistance < 0.0F)
        {
            if (node->GetPrev() == nullptr)
                break;
            node = node->GetPrev();
            distance = std::max(
                node->GetTile().GetDirLength() + previousDistance, 0.0F);
        }
        else
        {
            distance = previousDistance;
        }
    }
    return result;
}

void Player::Disconnect() noexcept
{
    disconnected = true;
    SetFinished(false);
    ResetBlock(false);
    SetLife(0.0F);
    Death();
    lowLifePoints.Reset(lowLifePoints.GetLifeLevel());
    energyDamageEffect.Reset();
    immortalEffect.Reset();
    slowEffect.Reset();
    gameCar.Reset();
    restoreSeconds = 0.0F;
    Immortal(0.0F);
    touchAttacker = undefinedPlayerId;
    touchAttributionSeconds = 0.0F;
    ClearBonusProjectiles();
    FreeCar(true);
}

void Player::ResetBlock(bool block) noexcept
{
    blockSeconds = block ? 0.0F : -1.0F;
}

bool Player::IsBlock() const noexcept
{
    return blockSeconds >= 0.0F;
}

float Player::GetBlockTime() const noexcept
{
    return blockSeconds;
}

void Player::SetBlockTime(float value) noexcept
{
    blockSeconds = value;
}

PlayerBlockMove Player::ProgressBlock(float seconds) noexcept
{
    if (!IsBlock())
        return PlayerBlockMove::Unblocked;
    blockSeconds = std::max(blockSeconds - std::max(seconds, 0.0F), 0.0F);
    return blockSeconds == 0.0F ? PlayerBlockMove::Brake
                                : PlayerBlockMove::Coast;
}

float Player::FinishBrake(float elapsedSeconds) const noexcept
{
    return GetFinished() &&
                   elapsedSeconds - finishTime >= finishBlockSeconds
               ? 1.0F
               : 0.0F;
}

std::size_t Player::RoundedRandomIndex(
    std::size_t count, float randomUnit) noexcept
{
    if (count <= 1U)
        return 0U;
    const float value = static_cast<float>(count - 1U) *
                        std::clamp(randomUnit, 0.0F, 1.0F);
    const float floorValue = std::floor(value);
    const float rounded = value - 0.5F < floorValue
                              ? floorValue
                              : floorValue + 1.0F;
    return std::min(static_cast<std::size_t>(rounded), count - 1U);
}

std::uint32_t Player::BonusCharge(
    std::uint32_t maximumCharge, float value) noexcept
{
    return static_cast<std::uint32_t>(std::max(
        static_cast<float>(maximumCharge) * value, 1.0F));
}

} // namespace r3d::game::originalrace::source
