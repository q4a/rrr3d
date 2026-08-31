#include "OriginalPlayer.h"

#include "OriginalMap.h"
#include "OriginalRace.h"
#include "OriginalWeapon.h"

#include <algorithm>
#include <cmath>

namespace r3d::game::originalrace::source
{

namespace
{

Weapon::Desc makeWeaponDescription(
    const WeaponDefinition& definition)
{
    Weapon::Desc result;
    result.shotDelay = definition.shotDelay;
    result.projectiles.reserve(definition.projectiles.size());
    for (const auto& projectile : definition.projectiles)
    {
        if (!projectile.spawnOnParentDeath)
            result.projectiles.push_back(projectile);
    }
    return result;
}

} // namespace

class Player::LowLifeBehavior final : public Behavior
{
public:
    LowLifeBehavior(Behaviors* owner, Player* player) noexcept
        : Behavior(owner), player_(player)
    {
    }

    void OnProgress(float deltaTime) noexcept override
    {
        if (player_ == nullptr)
            return;
        const auto result = state_.OnProgress(
            player_->gameCar, deltaTime, this);
        player_->lowLifeActivated_ =
            player_->lowLifeActivated_ || result.activated;
        player_->lowLifeReleased_ =
            player_->lowLifeReleased_ || result.released;
        if (result.spawn.createEffect)
            player_->lowLifeEffectSpawn_ = result.spawn;
    }

    LowLifePoints& State() noexcept { return state_; }
    const LowLifePoints& State() const noexcept { return state_; }

private:
    Player* player_ = nullptr;
    LowLifePoints state_;
};

class Player::EnergyDamageBehavior final : public Behavior
{
public:
    EnergyDamageBehavior(Behaviors* owner, Player* player) noexcept
        : Behavior(owner), player_(player),
          state_(DamageType::Energy, 0.5F)
    {
    }

    void OnProgress(float deltaTime) noexcept override
    {
        if (player_ != nullptr)
            state_.OnProgress(deltaTime);
    }

    void OnDamage(GameObject&, float, DamageType damageType) noexcept override
    {
        if (player_ != nullptr)
        {
            const bool created =
                state_.OnDamage(damageType);
            if (created)
            {
                const auto spawn = state_.GetSpawnResult(true);
                if (spawn.createEffect)
                    player_->energyDamageEffectSpawn_ = spawn;
            }
        }
    }

    DamageEffect& State() noexcept { return state_; }
    const DamageEffect& State() const noexcept { return state_; }

private:
    Player* player_ = nullptr;
    DamageEffect state_;
};

class Player::PlayerImmortalBehavior final : public Behavior
{
public:
    PlayerImmortalBehavior(Behaviors* owner, Player* player) noexcept
        : Behavior(owner), player_(player)
    {
    }

    void OnProgress(float deltaTime) noexcept override
    {
        if (player_ != nullptr)
            state_.OnProgress(deltaTime);
    }

    void OnDamage(GameObject&, float, DamageType) noexcept override
    {
        if (player_ != nullptr)
            state_.OnDamage();
    }

    ImmortalEffect& State() noexcept { return state_; }
    const ImmortalEffect& State() const noexcept { return state_; }

protected:
    void OnImmortalStatus(bool status) noexcept override
    {
        if (player_ != nullptr)
            state_.OnImmortalStatus(status);
    }

private:
    Player* player_ = nullptr;
    ImmortalEffect state_;
};

class Player::SlowBehavior final : public Behavior
{
public:
    SlowBehavior(Behaviors* owner, Player* player) noexcept
        : Behavior(owner), player_(player)
    {
    }

    void OnProgress(float deltaTime) noexcept override
    {
        if (player_ == nullptr)
            return;
        const auto result = state_.OnProgress(
            deltaTime, player_->behaviorLinearSpeed_);
        player_->slowSpeedLimited_ =
            player_->slowSpeedLimited_ || result.limitSpeed;
        player_->slowReleased_ =
            player_->slowReleased_ || result.released;
        if (result.released)
        {
            player_->slowBehavior_ = nullptr;
            Remove();
        }
    }

    SlowEffect& State() noexcept { return state_; }
    const SlowEffect& State() const noexcept { return state_; }

private:
    Player* player_ = nullptr;
    SlowEffect state_;
};

Player::Player()
{
    BindSourceBehaviors();
}

Player::~Player()
{
    // Player::~Player in the Windows source tears down retained bonus
    // projectiles before releasing the car and presentation resources.  The
    // portable GameCar is an embedded object, so its listener must also be
    // detached while every Player member is still alive; otherwise its own
    // destructor would call back into a partially destroyed Player.
    ClearBonusProjectiles();
    SetHeadlight(HeadLightMode::None);
    FreeCar(true);
    carRecord_ = nullptr;
    FreeColorMaterial();
}

void Player::BindSourceBehaviors()
{
    gameCar.SetEventSink(this);
    gameCar.ClearListenerList();
    gameCar.InsertListener(this);
    auto& behaviors = gameCar.GetBehaviors();
    vehicleDeathEffects_.clear();
    lowLifeBehavior_ = nullptr;
    energyDamageBehavior_ = nullptr;
    immortalBehavior_ = nullptr;
    slowBehavior_ = nullptr;
    behaviors.Clear();
    lowLifeBehavior_ = &behaviors.Add<LowLifeBehavior>(
        BehaviorType::LowLifePoints, this);
    // DataBase::LoadCar inserts every serialized DeathEffect immediately
    // after LowLifePoints. Keep duplicate type-6 entries: each owns a
    // different effect record and all of them receive the same Death event.
    if (carRecord_ != nullptr)
    {
        vehicleDeathEffects_.reserve(
            carRecord_->deathEffects.size());
        for (const auto& effect : carRecord_->deathEffects)
        {
            auto& behavior = behaviors.Add<DeathEffect>(
                BehaviorType::DeathEffect,
                effect.effectPhysicsIgnoreSenderCar,
                effect.targetChild);
            behavior.ConfigureSource(
                &effect.visual,
                {effect.position.x, effect.position.y,
                 effect.position.z},
                {effect.impulse.x, effect.impulse.y,
                 effect.impulse.z},
                effect.ignoreRotation,
                effect.visual.soundPaths);
            vehicleDeathEffects_.push_back(&behavior);
        }
    }
    // DataBase::LoadCar inserts ImmortalEffect before DamageEffect.
    immortalBehavior_ = &behaviors.Add<PlayerImmortalBehavior>(
        BehaviorType::ImmortalEffect, this);
    energyDamageBehavior_ = &behaviors.Add<EnergyDamageBehavior>(
        BehaviorType::DamageEffect, this);
    // Creating/respawning a car rebuilds the serialized behavior collection.
    // The concrete behavior therefore reloads its own record state here,
    // rather than relying on Player fields to survive graph replacement.
    if (carRecord_ != nullptr)
    {
        lowLifeBehavior_->State().Configure(
            &carRecord_->lowLifeEffect,
            {carRecord_->lowLifeEffectPosition.x,
             carRecord_->lowLifeEffectPosition.y,
             carRecord_->lowLifeEffectPosition.z},
            carRecord_->lowLifeLevel);
        energyDamageBehavior_->State().Configure(
            &carRecord_->energyDamageEffect,
            DamageType::Energy,
            carRecord_->energyDamageEffect.maximumTimeLife > 0.0F
                ? carRecord_->energyDamageEffect.maximumTimeLife
                : 0.5F);
        energyDamageBehavior_->State().ConfigureSounds(
            carRecord_->energyDamageSoundPaths);
        immortalBehavior_->State().Configure(
            &carRecord_->shieldEffect,
            {carRecord_->shieldEffectScale.x,
             carRecord_->shieldEffectScale.y,
             carRecord_->shieldEffectScale.z});
        immortalBehavior_->State().ConfigureSounds(
            carRecord_->shieldSoundPaths);
    }
}

void Player::ClearSlowBehavior() noexcept
{
    if (slowBehavior_ != nullptr)
        slowBehavior_->State().Reset();
    slowEffectSpawn_.reset();
    auto& behaviors = gameCar.GetBehaviors();
    if (auto* behavior = behaviors.Find(BehaviorType::SlowEffect))
        behaviors.Delete(behavior);
    slowBehavior_ = nullptr;
}

void Player::AttachWeaponMapObjects() noexcept
{
    auto& weapons = gameCar.GetWeapons();
    for (std::size_t slot = 0U;
         slot < PlayerSlotRack::slotCount; ++slot)
    {
        auto& item = slotRack_.GetSlot(
            static_cast<PlayerSlotType>(slot)).GetItem();
        auto* weaponItem = item.IsWeaponItem();
        if (weaponItem == nullptr)
            continue;
        std::string record = weaponItem->GetMapObjRecord();
        if (record.empty())
            record = weaponItem->GetRecord();
        auto& mapObject = weapons.Add(
            weaponItem->GetWpnDesc(), std::move(record));
        mapObject.GetGameObj().SetPos(weaponItem->GetPos());
        mapObject.GetGameObj().SetRot(weaponItem->GetRot());
        weaponItem->AttachWeapon(mapObject.GetWeapon());
        weaponItem->OnCreateCar();
    }
}

void Player::DetachWeaponMapObjects() noexcept
{
    for (std::size_t slot = 0U;
         slot < PlayerSlotRack::slotCount; ++slot)
    {
        auto& item = slotRack_.GetSlot(
            static_cast<PlayerSlotType>(slot)).GetItem();
        if (item.IsWeaponItem() != nullptr)
            item.OnDestroyCar();
    }
    gameCar.GetWeapons().Clear();
}

void Player::InitLight(
    std::size_t light, PresentationVector position,
    PresentationQuaternion rotation) noexcept
{
    if (light >= presentation_.headLights.size())
        return;
    auto& state = presentation_.headLights[light];
    if (!state.created)
    {
        state = {};
        state.created = true;
        state.highQualityShadow = light == 0U;
    }
    state.position = position;
    state.rotation = rotation;
    state.enabled = carPresent_;
}

void Player::FreeLight(std::size_t light) noexcept
{
    if (light < presentation_.headLights.size())
        presentation_.headLights[light] = {};
}

void Player::CreateNightLights(bool attach) noexcept
{
    if (!presentation_.nightFlareCreated)
        return;
    presentation_.nightLights.clear();
    presentation_.nightFlareAttached = attach && carPresent_;
    if (!presentation_.nightFlareAttached || carRecord_ == nullptr)
        return;
    presentation_.nightLights.reserve(carRecord_->nightLights.size());
    for (const auto& light : carRecord_->nightLights)
    {
        presentation_.nightLights.push_back(
            {light.head,
             {light.position.x, light.position.y, light.position.z},
             light.size});
    }
}

void Player::SetLightsParent(bool attach) noexcept
{
    for (auto& light : presentation_.headLights)
    {
        if (light.created)
            light.enabled = attach && carPresent_;
    }
    CreateNightLights(attach);
}

void Player::ApplyReflScene() noexcept
{
    presentation_.reflectionScene = reflScene_;
}

void Player::FreeColorMaterial() noexcept
{
    presentation_.colorMaterialCreated = false;
    presentation_.colorMaterialAttached = false;
}

void Player::ApplyColorMaterial() noexcept
{
    FreeColorMaterial();
    if (carPresent_ && carRecord_ != nullptr &&
        !carRecord_->bodyVisuals.empty() &&
        !carRecord_->bodyVisuals.front().meshPath.empty() &&
        !carRecord_->disableColor)
    {
        presentation_.colorMaterialCreated = true;
        presentation_.colorMaterialAttached = true;
    }
    ApplyColor();
}

void Player::ApplyColor() noexcept
{
    if (presentation_.colorMaterialCreated)
        presentation_.color = color_;
}

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
    BindSourceBehaviors();
    gameCar.ResetGameObject(std::max(newMaximumLife, 1.0F));
    SetPlace(initialPlace);
    car.Reset(trace);
}

GameObject::DamageResult Player::Damage(
    std::size_t senderPlayerId, float value,
    DamageType damageType) noexcept
{
    PrepareVehicleDeathEffects();
    return gameCar.Damage(senderPlayerId, value, damageType);
}

GameObject::DamageResult Player::Damage(
    std::size_t senderPlayerId, float value, float newLife,
    bool death, DamageType damageType) noexcept
{
    PrepareVehicleDeathEffects();
    return gameCar.Damage(
        senderPlayerId, value, newLife, death, damageType);
}

bool Player::Death(
    DamageType damageType, GameObject* target) noexcept
{
    PrepareVehicleDeathEffects();
    return gameCar.Death(damageType, target);
}

bool Player::Resc() noexcept { return gameCar.Resc(); }
void Player::Healt(float value) noexcept { gameCar.Healt(value); }
void Player::Immortal(float time) noexcept { gameCar.Immortal(time); }
void Player::SetImmortalFlag(bool value) noexcept
{
    gameCar.SetImmortalFlag(value);
}
bool Player::IsImmortal() const noexcept { return gameCar.IsImmortal(); }
float Player::GetLife() const noexcept { return gameCar.GetLife(); }
void Player::SetLife(float value) noexcept { gameCar.SetLife(value); }
float Player::GetMaxLife() const noexcept
{
    return gameCar.GetMaxLife();
}
void Player::SetMaxLife(float value) noexcept
{
    gameCar.SetMaxLife(value);
}
float Player::GetShieldSeconds() const noexcept
{
    return gameCar.shieldSeconds;
}
bool Player::IsDestroyed() const noexcept
{
    return gameCar.destroyed;
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
    ApplyColor();
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
    ApplyColor();
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
    if (headLight_ == value)
        return;
    headLight_ = value;
    constexpr PresentationQuaternion rotation{
        0.0009F, 0.344F, -0.029F, 0.939F};
    switch (headLight_)
    {
    case HeadLightMode::None:
        FreeLight(0U);
        FreeLight(1U);
        break;
    case HeadLightMode::One:
        InitLight(0U, {0.3F, 0.0F, 3.190F}, rotation);
        FreeLight(1U);
        break;
    case HeadLightMode::Two:
        InitLight(0U, {0.3F, 1.0F, 3.190F}, rotation);
        InitLight(1U, {0.3F, -1.0F, 3.190F}, rotation);
        break;
    }
    if (!presentation_.nightFlareCreated &&
        headLight_ != HeadLightMode::None)
    {
        presentation_.nightFlareCreated = true;
        CreateNightLights(carPresent_);
    }
    else if (presentation_.nightFlareCreated &&
             headLight_ == HeadLightMode::None)
    {
        presentation_.nightLights.clear();
        presentation_.nightFlareAttached = false;
        presentation_.nightFlareCreated = false;
    }
}

bool Player::HasCar() const noexcept
{
    return mapOwner_ != nullptr ? carMapObj_ != nullptr : carPresent_;
}

bool Player::HasAttachedLights() const noexcept
{
    return std::any_of(
        presentation_.headLights.begin(),
        presentation_.headLights.end(),
        [](const HeadLightState& light) { return light.enabled; });
}

const Player::PresentationState&
Player::GetPresentationState() const noexcept
{
    return presentation_;
}

bool Player::GetReflScene() const noexcept
{
    return reflScene_;
}

void Player::SetReflScene(bool value) noexcept
{
    if (reflScene_ == value)
        return;
    reflScene_ = value;
    ApplyReflScene();
}

const Vehicle* Player::GetCarRecord() const noexcept
{
    return carRecord_;
}

void Player::BindMap(Map* value, std::size_t sourceIndex) noexcept
{
    if (mapOwner_ == value && mapSourceIndex_ == sourceIndex)
        return;
    FreeCar(true);
    mapOwner_ = value;
    mapSourceIndex_ = sourceIndex;
}

MapObj* Player::GetCarMapObj() noexcept { return carMapObj_; }

const MapObj* Player::GetCarMapObj() const noexcept
{
    return carMapObj_;
}

void Player::BindWheelSlipCatalog(
    const ObjectDefinition* trailEffect,
    const ObjectDefinition* smokeEffect) noexcept
{
    wheelTrailEffect_ = trailEffect;
    wheelSmokeEffect_ = smokeEffect;
}

void Player::SetCar(const Vehicle* record) noexcept
{
    if (carRecord_ == record)
        return;
    // Object::ReplaceRef in the Windows code releases the live MapObj and
    // clears the complete CarState before replacing its MapObjRec pointer.
    FreeCar(true);
    carRecord_ = record;
    if (carRecord_ != nullptr)
    {
        GetLowLifePoints().Configure(
            &carRecord_->lowLifeEffect,
            {carRecord_->lowLifeEffectPosition.x,
             carRecord_->lowLifeEffectPosition.y,
             carRecord_->lowLifeEffectPosition.z},
            carRecord_->lowLifeLevel);
        GetEnergyDamageEffect().Configure(
            &carRecord_->energyDamageEffect,
            DamageType::Energy,
            carRecord_->energyDamageEffect.maximumTimeLife > 0.0F
                ? carRecord_->energyDamageEffect.maximumTimeLife
                : 0.5F);
        GetEnergyDamageEffect().ConfigureSounds(
            carRecord_->energyDamageSoundPaths);
        GetImmortalEffect().Configure(
            &carRecord_->shieldEffect,
            {carRecord_->shieldEffectScale.x,
             carRecord_->shieldEffectScale.y,
             carRecord_->shieldEffectScale.z});
        GetImmortalEffect().ConfigureSounds(carRecord_->shieldSoundPaths);
    }
    else
    {
        GetLowLifePoints().Configure(nullptr, {}, 0.35F);
        GetEnergyDamageEffect().Configure(
            nullptr, DamageType::Energy, 0.5F);
        GetEnergyDamageEffect().ConfigureSounds({});
        GetImmortalEffect().Configure(
            nullptr, {1.0F, 1.0F, 1.0F});
        GetImmortalEffect().ConfigureSounds({});
    }
}

void Player::CreateCar(bool newRace)
{
    if (!carPresent_)
    {
        if (mapOwner_ != nullptr && carRecord_ != nullptr)
        {
            auto& mapObject = mapOwner_->AddMapObj(
                MapObjCategory::Car, GameObjType::RockCar,
                carRecord_->record, mapSourceIndex_);
            mapObject.BindGameObj(gameCar);
            mapObject.SetPlayer(this);
            mapObject.GetGameObj().ResetGameObject(
                carRecord_->maximumLife);
            carMapObj_ = &mapObject;
        }
        carPresent_ = true;
        // Windows creates a new GameObject from the car record here. Rebuild
        // its behavior graph so EventEffect one-live state never leaks from
        // the previous destroyed actor into the respawned car.
        BindSourceBehaviors();
        if (carRecord_ != nullptr)
        {
            gameCar.ConfigureMotor({
                carRecord_->physics.brakeTorque,
                carRecord_->physics.differentialRatio,
                carRecord_->physics.maximumRpm,
                carRecord_->physics.idlingRpm,
                carRecord_->physics.maximumTorque,
                carRecord_->physics.torqueEfficiency,
                carRecord_->physics.restBrakeTorque,
                carRecord_->physics.maximumSpeed,
                carRecord_->physics.automaticGears});
            gameCar.BindSoundMotor(
                carRecord_->rpmVolumeRange,
                carRecord_->rpmFrequencyRange);
            gameCar.BindWheels(
                carRecord_->wheelSlipBehaviors,
                carRecord_->wheelSlipEffects,
                wheelTrailEffect_, wheelSmokeEffect_);
            std::vector<GameCar::WheelDynamics> wheelDynamics;
            wheelDynamics.reserve(carRecord_->physics.wheels.size());
            for (std::size_t wheelIndex = 0U;
                 wheelIndex < carRecord_->physics.wheels.size();
                 ++wheelIndex)
            {
                const auto& wheel =
                    carRecord_->physics.wheels[wheelIndex];
                wheelDynamics.push_back(
                    {wheel.position.x, wheel.driven, wheel.steering,
                     wheelIndex < carRecord_->wheelInverted.size() &&
                         carRecord_->wheelInverted[wheelIndex],
                     wheel.radius,
                     wheelIndex < carRecord_->wheelVisualOffsets.size()
                         ? std::array<float, 3U>{
                               carRecord_->wheelVisualOffsets[wheelIndex].x,
                               carRecord_->wheelVisualOffsets[wheelIndex].y,
                               carRecord_->wheelVisualOffsets[wheelIndex].z}
                         : std::array<float, 3U>{}});
            }
            gameCar.ConfigureDynamics(
                {{carRecord_->physics.angularDamping.x,
                  carRecord_->physics.angularDamping.y,
                  carRecord_->physics.angularDamping.z},
                 carRecord_->physics.airbornePitchAcceleration,
                 carRecord_->physics.clampRollAngle,
                 carRecord_->physics.clampPitchAngle,
                 carRecord_->physics.steerAngle,
                 carRecord_->physics.steerSpeed,
                 carRecord_->physics.steerRotation,
                 carRecord_->physics.gravitySteering,
                 carRecord_->physics.steeringControl,
                 carRecord_->physics.clutchImmunity,
                 carRecord_->physics.tireSpring,
                 carRecord_->disableColor},
                wheelDynamics);
            std::vector<int> cushionTargetTags;
            cushionTargetTags.reserve(
                carRecord_->cushionVisuals.size());
            for (const auto& visual : carRecord_->cushionVisuals)
                cushionTargetTags.push_back(visual.tag);
            gameCar.BindAnimationChildren(
                !carRecord_->trackVisuals.empty(),
                cushionTargetTags);
        }
        car.OnCreateCar(newRace);
        Resc();
        AttachWeaponMapObjects();
        for (std::size_t slot = 0U;
             slot < PlayerSlotRack::slotCount; ++slot)
        {
            auto& item = slotRack_.GetSlot(
                static_cast<PlayerSlotType>(slot)).GetItem();
            if (item.IsWeaponItem() == nullptr)
                item.OnCreateCar();
        }
        SetLightsParent(true);
        ApplyReflScene();
        ApplyColorMaterial();
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
        for (std::size_t slot = 0U;
             slot < PlayerSlotRack::slotCount; ++slot)
        {
            auto& item = slotRack_.GetSlot(
                static_cast<PlayerSlotType>(slot)).GetItem();
            if (item.IsWeaponItem() == nullptr)
                item.OnDestroyCar();
        }
        DetachWeaponMapObjects();
        SetLightsParent(false);
        presentation_.colorMaterialAttached = false;
    }
    carPresent_ = false;
    gameCar.RemoveListener(this);
    gameCar.ReleaseAnimationChildren();
    gameCar.ReleaseWheels();
    gameCar.ReleaseSoundMotor();
    car.OnFreeCar(freeState);
    if (carMapObj_ != nullptr)
    {
        auto* mapObject = carMapObj_;
        carMapObj_ = nullptr;
        if (mapOwner_ != nullptr)
            mapOwner_->DelMapObj(mapObject);
    }
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
    (void)weaponDefinitionCount;
}

void Player::BindWeaponItems(
    std::span<const WeaponDefinition> definitions,
    const WeaponLoadout& loadout) noexcept
{
    const bool rebuildLiveWeapons = carPresent_;
    if (rebuildLiveWeapons)
        DetachWeaponMapObjects();

    weaponSlots = loadout.primary;
    hyperWeapon = loadout.hyper;
    mineWeapon = loadout.mine;

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
    auto bind = [&](WeaponItem* item, std::size_t definitionIndex,
                    std::uint32_t countCharge,
                    std::uint32_t* currentCharge) {
        if (item == nullptr)
            return;
        if (definitionIndex == invalidWeapon ||
            definitionIndex >= definitions.size())
        {
            item->Bind(nullptr, 0U, 0U, nullptr);
            item->SetMapObjRecord({});
            item->SetShotEffectDefinition({});
            return;
        }
        const auto& definition = definitions[definitionIndex];
        item->Bind(
            nullptr, definition.maximumCharge, countCharge,
            currentCharge, definition.chargeStep, definition.damage,
            definition.chargeCost);
        item->SetMapObjRecord(definition.record);
        item->SetWpnDesc(makeWeaponDescription(definition));
        item->SetShotEffectDefinition(definition.shotEffect);
    };

    for (std::size_t slot = 0U; slot < weaponSlotCount; ++slot)
    {
        const auto physicalType = static_cast<PlayerSlotType>(
            static_cast<std::size_t>(PlayerSlotType::Weapon1) + slot);
        auto* item = ensureItem(physicalType, weaponSlots[slot]);
        auto currentCharge = loadout.primaryCurrentCharge[slot];
        bind(item, weaponSlots[slot],
             loadout.primaryCountCharge[slot], &currentCharge);
        const std::size_t definitionIndex = weaponSlots[slot];
        if (definitionIndex == invalidWeapon ||
            definitionIndex >= definitions.size())
            continue;
        const auto& definition = definitions[definitionIndex];
        if (auto* droid = dynamic_cast<DroidItem*>(item))
        {
            droid->Bind(
                nullptr,
                definition.maximumCharge,
                loadout.primaryCountCharge[slot], &currentCharge,
                definition.repairValue,
                definition.repairPeriod);
        }
        else if (auto* reflector = dynamic_cast<ReflectorItem*>(item))
        {
            reflector->Bind(
                nullptr,
                definition.maximumCharge,
                loadout.primaryCountCharge[slot], &currentCharge,
                definition.reflectValue);
        }
        item->SetMapObjRecord(definition.record);
        item->SetWpnDesc(makeWeaponDescription(definition));
        item->SetShotEffectDefinition(definition.shotEffect);
        item->SetChargeStep(definition.chargeStep);
        item->SetDamage(definition.damage);
        item->SetChargeCost(definition.chargeCost);
    }
    auto hyperCurrentCharge = loadout.hyperCurrentCharge;
    bind(ensureItem(PlayerSlotType::Hyper, hyperWeapon),
         hyperWeapon, loadout.hyperCountCharge,
         &hyperCurrentCharge);
    auto mineCurrentCharge = loadout.mineCurrentCharge;
    bind(ensureItem(PlayerSlotType::Mine, mineWeapon),
         mineWeapon, loadout.mineCountCharge, &mineCurrentCharge);

    if (rebuildLiveWeapons)
        AttachWeaponMapObjects();
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

bool Player::Shot(
    WeaponItem& item,
    std::span<const Weapon::ShotContext> contexts,
    bool mineSlot, std::uint32_t projectileId,
    int newCharge, Weapon::ProjList* projectiles)
{
    Weapon::ProjList localProjectiles;
    auto* output = projectiles != nullptr
        ? projectiles
        : &localProjectiles;
    const bool result = item.Shot(contexts, newCharge, output);
    if (result && mineSlot && !output->empty())
        InsertBonusProjectile(output->front(), projectileId);
    return result;
}

RockCar::Weapons& Player::GetWeaponRack() noexcept
{
    return gameCar.GetWeapons();
}

const RockCar::Weapons& Player::GetWeaponRack() const noexcept
{
    return gameCar.GetWeapons();
}

void Player::BindSlots(
    const std::vector<OriginalWorkshopItem>& workshop,
    const std::vector<RacerSlot>& loadout)
{
    if (carPresent_)
    {
        for (std::size_t slot = 0U;
             slot < PlayerSlotRack::slotCount; ++slot)
        {
            auto& item = slotRack_.GetSlot(
                static_cast<PlayerSlotType>(slot)).GetItem();
            if (item.IsWeaponItem() == nullptr)
                item.OnDestroyCar();
        }
        DetachWeaponMapObjects();
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
        AttachWeaponMapObjects();
        for (std::size_t slot = 0U;
             slot < PlayerSlotRack::slotCount; ++slot)
        {
            auto& item = slotRack_.GetSlot(
                static_cast<PlayerSlotType>(slot)).GetItem();
            if (item.IsWeaponItem() == nullptr)
                item.OnCreateCar();
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
        auto& oldItem = slot.GetItem();
        if (auto* weaponItem = oldItem.IsWeaponItem())
        {
            auto* weapon = weaponItem->GetWeapon();
            auto* mapObject =
                weapon != nullptr ? weapon->GetMapObj() : nullptr;
            oldItem.OnDestroyCar();
            gameCar.GetWeapons().Remove(mapObject);
        }
        else
        {
            oldItem.OnDestroyCar();
        }
    }
    slot.SetRecord(record);
    if (record == nullptr)
        return;
    slot.GetItem().SetPos(position);
    slot.GetItem().SetRot(rotation);
    if (carPresent_)
    {
        auto& item = slot.GetItem();
        if (auto* weaponItem = item.IsWeaponItem())
        {
            std::string mapRecord = weaponItem->GetMapObjRecord();
            if (mapRecord.empty())
                mapRecord = weaponItem->GetRecord();
            auto& mapObject = gameCar.GetWeapons().Add(
                weaponItem->GetWpnDesc(), std::move(mapRecord));
            mapObject.GetGameObj().SetPos(position);
            mapObject.GetGameObj().SetRot(rotation);
            weaponItem->AttachWeapon(mapObject.GetWeapon());
        }
        item.OnCreateCar();
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
    const float previous = GetLife();
    Healt(value);
    return {PlayerBonusSlot::None, invalidWeapon,
            static_cast<std::uint32_t>(
                std::max(GetLife() - previous, 0.0F))};
}

PlayerBonusResult Player::TakeImmortal(float value) noexcept
{
    Immortal(std::max(value, 0.0F));
    return {PlayerBonusSlot::None, invalidWeapon,
            static_cast<std::uint32_t>(GetShieldSeconds())};
}

PlayerBonusResult Player::TakeAmmunition(
    float value,
    const std::vector<std::uint32_t>& maximumCharges,
    float randomUnit) noexcept
{
    struct RechargeTarget
    {
        WeaponItem* item = nullptr;
        std::size_t weapon = invalidWeapon;
        PlayerBonusSlot slot = PlayerBonusSlot::None;
    };
    std::vector<RechargeTarget> targets;
    targets.reserve(weaponSlots.size() + 2U);
    auto* hyperItem = GetHyperWeaponItem();
    if (hyperItem != nullptr && hyperWeapon != invalidWeapon &&
        hyperWeapon < maximumCharges.size() &&
        hyperItem->GetCurCharge() < hyperItem->GetCntCharge())
    {
        targets.push_back(
            {hyperItem, hyperWeapon, PlayerBonusSlot::Hyper});
    }
    auto* mineItem = GetMineWeaponItem();
    if (mineItem != nullptr && mineWeapon != invalidWeapon &&
        mineWeapon < maximumCharges.size() &&
        mineItem->GetCurCharge() < mineItem->GetCntCharge())
    {
        targets.push_back(
            {mineItem, mineWeapon, PlayerBonusSlot::Mine});
    }
    const auto primaryItems = GetPrimaryWeaponItems();
    for (std::size_t slot = 0U; slot < weaponSlots.size(); ++slot)
    {
        const auto weapon = weaponSlots[slot];
        auto* item = primaryItems[slot];
        if (item == nullptr || weapon == invalidWeapon ||
            weapon >= maximumCharges.size() ||
            item->GetCurCharge() >= item->GetCntCharge())
            continue;
        targets.push_back(
            {item, weapon, PlayerBonusSlot::Primary});
    }
    PlayerBonusResult result;
    if (!targets.empty())
    {
        auto& target = targets[RoundedRandomIndex(
            targets.size(), randomUnit)];
        const auto amount = BonusCharge(
            maximumCharges[target.weapon], value);
        target.item->SetCurCharge(std::min(
            target.item->GetCurCharge() + amount,
            target.item->GetCntCharge()));
        result = {target.slot, target.weapon, amount};
    }
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
    PrepareBehaviors(lowLifeLevel, linearSpeed);
    const auto carProgress = gameCar.OnProgress(deltaTime);
    auto result = FinishBehaviorProgress(deltaTime);
    result.gameObject = carProgress.gameObject;
    return result;
}

void Player::PrepareBehaviors(
    float lowLifeLevel, float linearSpeed) noexcept
{
    GetLowLifePoints().SetLifeLevel(lowLifeLevel);
    lowLifeActivated_ = false;
    lowLifeReleased_ = false;
    lowLifeEffectSpawn_.reset();
    behaviorLinearSpeed_ = linearSpeed;
    slowSpeedLimited_ = false;
    slowReleased_ = false;
}

Player::BehaviorProgressResult Player::FinishBehaviorProgress(
    float deltaTime) noexcept
{
    BehaviorProgressResult result;
    for (std::size_t slot = 0U; slot < weaponSlotCount; ++slot)
    {
        auto& item = slotRack_.GetSlot(
            static_cast<PlayerSlotType>(
                static_cast<std::size_t>(PlayerSlotType::Weapon1) + slot))
                         .GetItem();
        if (auto* droid = dynamic_cast<DroidItem*>(&item))
            droid->OnProgress(
                deltaTime, gameCar.life, gameCar.maximumLife,
                gameCar.destroyed);
    }
    result.lowLifeActivated = lowLifeActivated_;
    result.lowLifeReleased = lowLifeReleased_;
    result.lowLifeSpawn = lowLifeEffectSpawn_;
    result.slowSpeedLimited = slowSpeedLimited_;
    result.slowReleased = slowReleased_;
    return result;
}

void Player::PrepareVehicleDeathEffects() noexcept
{
    const bool logicAvailable = gameCar.GetLogic() != nullptr;
    for (auto* behavior : vehicleDeathEffects_)
    {
        if (behavior != nullptr)
            behavior->SetSpawnContext(logicAvailable, false);
    }
}

std::size_t Player::GetVehicleDeathEffectCount() const noexcept
{
    return vehicleDeathEffects_.size();
}

std::vector<DeathEffect::SpawnResult>
Player::ConsumeVehicleDeathEffectSpawns() noexcept
{
    std::vector<DeathEffect::SpawnResult> result;
    result.reserve(vehicleDeathEffects_.size());
    for (auto* behavior : vehicleDeathEffects_)
    {
        result.push_back(
            behavior != nullptr
                ? behavior->ConsumeSpawnResult()
                : DeathEffect::SpawnResult{});
    }
    return result;
}

bool Player::AttachSlowEffect(
    const ObjectDefinition* effectDefinition,
    float maximumTimeLife, std::size_t weapon,
    std::size_t projectile) noexcept
{
    auto& behaviors = gameCar.GetBehaviors();
    // Proj::FrostRayUpdate checks Find<SlowEffect>() before adding. Repeated
    // ray contacts therefore neither replace the model nor restart lifetime.
    if (behaviors.Find(BehaviorType::SlowEffect) != nullptr)
        return false;
    auto& behavior = behaviors.Add<SlowBehavior>(
        BehaviorType::SlowEffect, this);
    if (!behavior.State().Attach(
            effectDefinition, maximumTimeLife, weapon, projectile))
    {
        behaviors.Delete(&behavior);
        return false;
    }
    slowBehavior_ = &behavior;
    slowEffectSpawn_ = behavior.State().GetSpawnResult(true);
    return true;
}

std::optional<EventEffect::SpawnResult>
Player::ConsumeSlowEffectSpawn() noexcept
{
    auto result = slowEffectSpawn_;
    slowEffectSpawn_.reset();
    return result;
}

void Player::NotifySlowEffectDestroyed() noexcept
{
    if (slowBehavior_ != nullptr)
        slowBehavior_->State().Reset();
    slowEffectSpawn_.reset();
    slowSpeedLimited_ = false;
    slowReleased_ = true;
    if (auto* behavior =
            gameCar.GetBehaviors().Find(BehaviorType::SlowEffect))
    {
        behavior->Remove();
    }
    slowBehavior_ = nullptr;
}

LowLifePoints& Player::GetLowLifePoints() noexcept
{
    return lowLifeBehavior_->State();
}

const LowLifePoints& Player::GetLowLifePoints() const noexcept
{
    return lowLifeBehavior_->State();
}

DamageEffect& Player::GetEnergyDamageEffect() noexcept
{
    return energyDamageBehavior_->State();
}

const DamageEffect& Player::GetEnergyDamageEffect() const noexcept
{
    return energyDamageBehavior_->State();
}

ImmortalEffect& Player::GetImmortalEffect() noexcept
{
    return immortalBehavior_->State();
}

const ImmortalEffect& Player::GetImmortalEffect() const noexcept
{
    return immortalBehavior_->State();
}

SlowEffect* Player::GetSlowEffect() noexcept
{
    return slowBehavior_ != nullptr ? &slowBehavior_->State() : nullptr;
}

const SlowEffect* Player::GetSlowEffect() const noexcept
{
    return slowBehavior_ != nullptr ? &slowBehavior_->State() : nullptr;
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
    float deltaTime, std::uint32_t cheatMask, std::size_t playerId,
    std::size_t difficulty,
    const std::vector<CheatPlayerView>& players)
{
    ProgressResult result;
    if (HasCar())
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
            candidate->IsDestroyed() || candidate->disconnected)
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
    InsertBonusProjectile(nullptr, projectileId);
}

void Player::InsertBonusProjectile(
    Proj* projectile, std::uint32_t projectileId)
{
    // Player::InsertBonusProj is reached only from Player::Shot(stMine).
    // Ordinary weapon/hyper shots reuse the current RPC id and do not advance
    // this owner-specific sequence.
    nextBonusProjectileId_ = projectileId + 1U;
    if (projectile != nullptr)
        projectile->InsertListener(this);
    bonusProjectiles_.push_back({projectile, projectileId});
}

bool Player::RemoveBonusProjectile(
    std::uint32_t projectileId) noexcept
{
    const auto found = std::find_if(
        bonusProjectiles_.begin(), bonusProjectiles_.end(),
        [projectileId](const BonusProjectileRef& value) {
            return value.id == projectileId;
        });
    if (found == bonusProjectiles_.end())
        return false;
    if (found->projectile != nullptr)
        found->projectile->RemoveListener(this);
    bonusProjectiles_.erase(found);
    return true;
}

void Player::ClearBonusProjectiles() noexcept
{
    while (!bonusProjectiles_.empty())
    {
        auto& projectile = bonusProjectiles_.back();
        if (projectile.projectile != nullptr)
            projectile.projectile->RemoveListener(this);
        bonusProjectiles_.pop_back();
    }
}

bool Player::HasBonusProjectile(
    std::uint32_t projectileId) const noexcept
{
    return GetBonusProjectile(projectileId) != nullptr ||
           std::any_of(
               bonusProjectiles_.begin(), bonusProjectiles_.end(),
               [projectileId](const BonusProjectileRef& value) {
                   return value.projectile == nullptr &&
                          value.id == projectileId;
               });
}

Proj* Player::GetBonusProjectile(
    std::uint32_t projectileId) noexcept
{
    return const_cast<Proj*>(
        static_cast<const Player*>(this)->GetBonusProjectile(projectileId));
}

const Proj* Player::GetBonusProjectile(
    std::uint32_t projectileId) const noexcept
{
    const auto found = std::find_if(
        bonusProjectiles_.begin(), bonusProjectiles_.end(),
        [projectileId](const BonusProjectileRef& value) {
            return value.id == projectileId &&
                   value.projectile != nullptr &&
                   value.projectile->GetLiveState() ==
                       GameObject::LiveState::Live;
        });
    return found == bonusProjectiles_.end()
               ? nullptr
               : found->projectile;
}

std::uint32_t Player::GetBonusProjectileId(
    const Proj* projectile) const noexcept
{
    const auto found = std::find_if(
        bonusProjectiles_.begin(), bonusProjectiles_.end(),
        [projectile](const BonusProjectileRef& value) {
            return value.projectile == projectile &&
                   value.projectile != nullptr &&
                   value.projectile->GetLiveState() ==
                       GameObject::LiveState::Live;
        });
    return found == bonusProjectiles_.end() ? 0U : found->id;
}

std::uint32_t Player::GetNextBonusProjectileId() const noexcept
{
    return nextBonusProjectileId_;
}

bool Player::ConsumeEnergyDamageEffectCreated() noexcept
{
    return ConsumeEnergyDamageEffectSpawn().has_value();
}

std::optional<EventEffect::SpawnResult>
Player::ConsumeEnergyDamageEffectSpawn() noexcept
{
    auto result = energyDamageEffectSpawn_;
    energyDamageEffectSpawn_.reset();
    return result;
}

std::vector<PlayerGameEvent> Player::TakeGameEvents() noexcept
{
    std::vector<PlayerGameEvent> result;
    result.swap(gameEvents_);
    return result;
}

void Player::OnDestroy(GameObject& sender) noexcept
{
    if (&sender == &gameCar)
        FreeCar(false);
    else if (auto* projectile = sender.IsProj())
    {
        const auto found = std::find_if(
            bonusProjectiles_.begin(), bonusProjectiles_.end(),
            [projectile](const BonusProjectileRef& value) {
                return value.projectile == projectile;
            });
        if (found != bonusProjectiles_.end())
        {
            projectile->RemoveListener(this);
            bonusProjectiles_.erase(found);
        }
    }
}

void Player::OnLowLife(
    GameObject& sender, Behavior*) noexcept
{
    if (&sender != &gameCar)
        return;
    lowLifeActivated_ = true;
}

void Player::OnDeath(
    GameObject& sender, DamageType damageType,
    GameObject* target) noexcept
{
    (void)target;
    if (&sender != &gameCar)
        return;
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
        {PlayerGameEventKind::Death, gameCar.GetTouchPlayerId(), 0.0F,
         damageType});
}

void Player::OnRockCarDamageDispatch(
    std::size_t senderPlayerId, float value,
    DamageType damageType) noexcept
{
    gameEvents_.push_back(
        {PlayerGameEventKind::Damage, senderPlayerId, value,
         damageType});
}

void Player::OnRockCarKillDispatch(
    std::size_t senderPlayerId, float value,
    DamageType damageType) noexcept
{
    gameEvents_.push_back(
        {PlayerGameEventKind::Kill, senderPlayerId, value,
         damageType});
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

void Player::Destroy() noexcept
{
    SetLife(0.0F);
    Death();
    GetLowLifePoints().Reset(GetLowLifePoints().GetLifeLevel());
    GetEnergyDamageEffect().Reset();
    GetImmortalEffect().Reset();
    ClearSlowBehavior();
    gameCar.Reset();
    Immortal(0.0F);
    gameCar.touchAttacker = undefinedPlayerId;
    gameCar.touchAttributionSeconds = 0.0F;
    restoreSeconds = 0.0F;
    // GameObject::Death destroys the MapObj immediately afterwards. The
    // portable owner combines that OnDestroy callback here so render/audio
    // adapters cannot retain child state during the restore delay.
    FreeCar(false);
}

PlayerRestoreStep Player::ProgressRestore(float seconds)
{
    if (HasCar())
        return PlayerRestoreStep::None;
    restoreSeconds += std::max(seconds, 0.0F);
    // Player.cpp uses a strict `> cTimeRestoreCar` comparison. CreateCar and
    // ResetCar belong to the same source callback; there is no intermediate
    // frame with a resurrected GameObject but no car MapObj.
    if (restoreSeconds <= restoreCarSeconds)
        return PlayerRestoreStep::None;
    restoreSeconds = 0.0F;
    CreateCar(false);
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
    GetLowLifePoints().Reset(GetLowLifePoints().GetLifeLevel());
    GetEnergyDamageEffect().Reset();
    GetImmortalEffect().Reset();
    ClearSlowBehavior();
    gameCar.Reset();
    restoreSeconds = 0.0F;
    Immortal(0.0F);
    gameCar.touchAttacker = undefinedPlayerId;
    gameCar.touchAttributionSeconds = 0.0F;
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
