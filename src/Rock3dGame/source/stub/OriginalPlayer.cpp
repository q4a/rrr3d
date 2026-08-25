#include "OriginalPlayer.h"

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
    speed_ = 0.0F;
    curTile_ = nullptr;
    curNode_ = nullptr;
    lastNode_ = nullptr;
    lastNodeCoordX_ = 0.5F;
    track_ = 0U;
    numLaps = 0U;
    moveInverse = false;
    moveInverseStart_ = -1.0F;
    maximumSpeed_ = 0.0F;
    maximumSpeedTime_ = 0.0F;
    fallbackMapPosition_ = {};
    if (trace_ != nullptr)
    {
        const auto* path = trace_->GetPath(0U);
        if (path != nullptr && path->GetFirst() != nullptr)
            fallbackMapPosition_ = path->GetFirst()->GetPos();
    }
}

Player::CarState::UpdateResult Player::CarState::Update(
    Trace& trace, const TraceVec3& position,
    const TraceVec3& direction, float vehicleSpeed,
    float deltaTime)
{
    trace_ = &trace;
    position_ = position;
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

TraceVec3 Player::CarState::GetMapPos() const noexcept
{
    if (curTile_ != nullptr)
    {
        return curTile_->GetTile().GetPoint(
            curTile_->GetTile().ComputeCoordX(xy(position_)));
    }
    if (lastNode_ != nullptr)
        return lastNode_->GetTile().GetPoint(lastNodeCoordX_);
    return fallbackMapPosition_;
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
    place = initialPlace;
    car.Reset(trace);
}

void Player::ReloadWeapons(
    std::size_t weaponDefinitionCount) noexcept
{
    for (std::size_t slot = 0U; slot < weaponCharges.size(); ++slot)
    {
        if (weaponSlots[slot] != invalidWeapon &&
            weaponSlots[slot] < weaponDefinitionCount)
        {
            WeaponItem item(
                nullptr, 0U, weaponCapacity[slot],
                &weaponCharges[slot]);
            item.Reload();
        }
    }
    if (hyperWeapon != invalidWeapon &&
        hyperWeapon < weaponDefinitionCount)
    {
        WeaponItem item(nullptr, 0U, hyperCapacity, &hyperCharge);
        item.Reload();
    }
    if (mineWeapon != invalidWeapon &&
        mineWeapon < weaponDefinitionCount)
    {
        WeaponItem item(nullptr, 0U, mineCapacity, &mines);
        item.Reload();
    }
    SyncSelectedWeapon(weaponDefinitionCount);
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

PlayerBonusResult Player::TakeMoney(float value) noexcept
{
    const auto amount = static_cast<std::uint32_t>(
        std::max(value, 0.0F));
    pickedMoney += amount;
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
    const bool onStatus = !IsTimedImmortal();
    Immortal(std::max(value, 0.0F));
    if (onStatus)
        immortalEffect.OnImmortalStatus(true);
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

Player::BehaviorProgressResult Player::ProgressBehaviors(
    float deltaTime, float lowLifeLevel, float linearSpeed) noexcept
{
    BehaviorProgressResult result;
    result.gameObject = GameObject::OnProgress(deltaTime);
    if (result.gameObject.immortalityEnded)
        immortalEffect.OnImmortalStatus(false);
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

bool Player::OnDamageBehaviors(DamageType damageType) noexcept
{
    // GameObject dispatches listeners for every damage message, including a
    // hit absorbed by immortality.
    immortalEffect.OnDamage();
    return energyDamageEffect.OnDamage(damageType);
}

void Player::SetFinished(bool value, float time) noexcept
{
    finished = value;
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
    place = resultPlace;
    rewardMoney = resultMoney;
    rewardPoints = resultPoints;
}

void Player::AddMoney(std::int32_t value) noexcept
{
    const auto result = static_cast<std::int64_t>(money) + value;
    money = static_cast<std::uint32_t>(std::max<std::int64_t>(result, 0));
}

void Player::AddPoints(std::int32_t value) noexcept
{
    const auto result = static_cast<std::int64_t>(points) + value;
    points = static_cast<std::uint32_t>(
        std::max<std::int64_t>(result, 0));
}

void Player::ApplyRaceReward() noexcept
{
    AddMoney(static_cast<std::int32_t>(rewardMoney + pickedMoney));
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
}

PlayerRestoreStep Player::ProgressRestore(float seconds) noexcept
{
    if (!destroyed)
        return PlayerRestoreStep::None;
    if (restoreSeconds < 0.0F)
    {
        restoreSeconds = 0.0F;
        Resc();
        return PlayerRestoreStep::ActivateCar;
    }
    restoreSeconds = std::max(0.0F, restoreSeconds - seconds);
    if (restoreSeconds > 0.0F)
        return PlayerRestoreStep::None;
    SetLife(maximumLife);
    restoreSeconds = -1.0F;
    return PlayerRestoreStep::QueueRespawn;
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
    return finished && elapsedSeconds - finishTime >= finishBlockSeconds
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
