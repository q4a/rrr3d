#include "OriginalLogic.h"

#include "OriginalMap.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace r3d::game::originalrace::source
{

LogicBehavior::LogicBehavior(
    LogicBehaviors* owner, LogicBehaviorType type) noexcept
    : owner_(owner), type_(type)
{
}

LogicBehaviorType LogicBehavior::GetType() const noexcept
{
    return type_;
}

LogicBehaviors* LogicBehavior::GetOwner() noexcept { return owner_; }

const LogicBehaviors* LogicBehavior::GetOwner() const noexcept
{
    return owner_;
}

Logic* LogicBehavior::GetLogic() noexcept
{
    return owner_ != nullptr ? owner_->GetLogic() : nullptr;
}

const Logic* LogicBehavior::GetLogic() const noexcept
{
    return owner_ != nullptr ? owner_->GetLogic() : nullptr;
}

Map* LogicBehavior::GetMap() noexcept
{
    auto* logic = GetLogic();
    return logic != nullptr ? logic->GetMap() : nullptr;
}

const Map* LogicBehavior::GetMap() const noexcept
{
    const auto* logic = GetLogic();
    return logic != nullptr ? logic->GetMap() : nullptr;
}

void LogicBehavior::RegProgressEvent()
{
    if (auto* logic = GetLogic())
        logic->RegProgressEvent(this);
}

void LogicBehavior::UnregProgressEvent() noexcept
{
    if (auto* logic = GetLogic())
        logic->UnregProgressEvent(this);
}

LogicEventEffect::LogicEventEffect(
    LogicBehaviors* owner, LogicBehaviorType type) noexcept
    : LogicBehavior(owner, type)
{
}

LogicEventEffect::~LogicEventEffect()
{
    ClearEffects();
}

const std::string& LogicEventEffect::GetEffectRecord() const noexcept
{
    return effectRecord_;
}

void LogicEventEffect::SetEffectRecord(std::string value)
{
    effectRecord_ = std::move(value);
}

const LogicEventEffect::Vector3& LogicEventEffect::GetPos() const noexcept
{
    return position_;
}

void LogicEventEffect::SetPos(Vector3 value) noexcept
{
    position_ = value;
}

bool LogicEventEffect::HasEffect(EffectId effect) const noexcept
{
    return std::any_of(
        effects_.begin(), effects_.end(),
        [effect](const EffectInstance& value) {
            return value.id == effect;
        });
}

bool LogicEventEffect::IsEffectDying(EffectId effect) const noexcept
{
    const auto found = std::find_if(
        effects_.begin(), effects_.end(),
        [effect](const EffectInstance& value) {
            return value.id == effect;
        });
    return found != effects_.end() && found->dying;
}

LogicEventEffect::Vector3 LogicEventEffect::GetEffectPosition(
    EffectId effect) const noexcept
{
    const auto found = std::find_if(
        effects_.begin(), effects_.end(),
        [effect](const EffectInstance& value) {
            return value.id == effect;
        });
    return found != effects_.end() ? found->position : Vector3{};
}

std::size_t LogicEventEffect::GetEffectCount() const noexcept
{
    return effects_.size();
}

void LogicEventEffect::NotifyEffectDestroyed(EffectId effect) noexcept
{
    std::erase_if(effects_, [effect](const EffectInstance& value) {
        return value.id == effect;
    });
}

LogicEventEffect::EffectId LogicEventEffect::CreateEffect(
    Vector3 position)
{
    EffectInstance effect;
    effect.id = nextEffectId_++;
    if (effect.id == invalidEffect)
        effect.id = nextEffectId_++;
    effect.position = {
        position_.x + position.x,
        position_.y + position.y,
        position_.z + position.z};
    effects_.push_back(effect);
    return effect.id;
}

void LogicEventEffect::SetEffectPosition(
    EffectId effect, Vector3 position) noexcept
{
    const auto found = std::find_if(
        effects_.begin(), effects_.end(),
        [effect](const EffectInstance& value) {
            return value.id == effect;
        });
    if (found == effects_.end())
        return;
    found->position = {
        position_.x + position.x,
        position_.y + position.y,
        position_.z + position.z};
}

void LogicEventEffect::BeginDeleteEffect(EffectId effect) noexcept
{
    const auto found = std::find_if(
        effects_.begin(), effects_.end(),
        [effect](const EffectInstance& value) {
            return value.id == effect;
        });
    if (found != effects_.end())
        found->dying = true;
}

void LogicEventEffect::ClearEffects() noexcept
{
    effects_.clear();
    nextEffectId_ = 1U;
}

PairPxContactEffect::PairPxContactEffect(LogicBehaviors* owner)
    : LogicEventEffect(owner, LogicBehaviorType::PairPxContactEffect)
{
    RegProgressEvent();
}

PairPxContactEffect::~PairPxContactEffect()
{
    UnregProgressEvent();
    Reset();
}

void PairPxContactEffect::NotifyEffectDestroyed(
    EffectId effect) noexcept
{
    LogicEventEffect::NotifyEffectDestroyed(effect);
    for (auto& [key, node] : contacts_)
    {
        static_cast<void>(key);
        for (auto& contact : node.contacts)
        {
            if (contact.effect == effect)
                contact.effect = invalidEffect;
        }
    }
}

LogicBehaviors::LogicBehaviors(Logic* logic)
    : logic_(logic),
      pairPxContactEffect_(
          std::make_unique<PairPxContactEffect>(this))
{
}

LogicBehaviors::~LogicBehaviors() = default;

Logic* LogicBehaviors::GetLogic() noexcept { return logic_; }

const Logic* LogicBehaviors::GetLogic() const noexcept { return logic_; }

std::size_t LogicBehaviors::GetCount() const noexcept
{
    return pairPxContactEffect_ != nullptr ? 1U : 0U;
}

LogicBehavior* LogicBehaviors::Find(LogicBehaviorType type) noexcept
{
    return const_cast<LogicBehavior*>(
        static_cast<const LogicBehaviors*>(this)->Find(type));
}

const LogicBehavior* LogicBehaviors::Find(
    LogicBehaviorType type) const noexcept
{
    return type == LogicBehaviorType::PairPxContactEffect
               ? pairPxContactEffect_.get()
               : nullptr;
}

PairPxContactEffect::ContactResult LogicBehaviors::OnContact(
    PairPxContactEffect::Key key, float frictionForce,
    bool firstShapeIsWheel, bool secondShapeIsWheel,
    std::span<const PairPxContactEffect::Point> points,
    float randomUnit)
{
    return pairPxContactEffect_->OnContact(
        key, frictionForce, firstShapeIsWheel,
        secondShapeIsWheel, points, randomUnit);
}

void LogicBehaviors::AttachWorld()
{
    if (pairPxContactEffect_ != nullptr)
        pairPxContactEffect_->RegProgressEvent();
}

void LogicBehaviors::DetachWorld() noexcept
{
    if (pairPxContactEffect_ != nullptr)
        pairPxContactEffect_->UnregProgressEvent();
}

bool PairPxContactEffect::Key::operator<(
    const Key& other) const noexcept
{
    return actor1 == other.actor1 ? actor2 < other.actor2
                                  : actor1 < other.actor1;
}

void PairPxContactEffect::Reset(std::size_t soundCount) noexcept
{
    contacts_.clear();
    pendingReleases_.clear();
    ClearEffects();
    soundCount_ = soundCount;
}

PairPxContactEffect::ContactResult PairPxContactEffect::OnContact(
    Key key, float frictionForce, bool firstShapeIsWheel,
    bool secondShapeIsWheel, std::span<const Point> points,
    float randomUnit)
{
    ContactResult result;
    if (frictionForce <= minimumFrictionForce || firstShapeIsWheel ||
        secondShapeIsWheel)
    {
        return result;
    }

    result.accepted = true;
    auto [nodeIterator, inserted] = contacts_.try_emplace(key);
    auto& node = nodeIterator->second;
    result.pairCreated = inserted;
    if (inserted && soundCount_ > 0U)
    {
        const float unit = std::clamp(randomUnit, 0.0F, 1.0F);
        node.sound = std::min(
            static_cast<std::size_t>(
                static_cast<float>(soundCount_) * unit),
            soundCount_ - 1U);
    }
    result.sound = node.sound;

    result.points.reserve(std::min(points.size(), maximumPoints));
    for (const auto& point : points)
    {
        if (node.last == node.contacts.size())
        {
            if (node.contacts.size() >= maximumPoints)
                break;
            node.contacts.emplace_back();
        }
        auto& contact = node.contacts[node.last];
        const bool createdEffect = contact.effect == invalidEffect;
        if (createdEffect)
        {
            contact.effect = CreateEffect(
                {point.x, point.y, point.z});
        }
        else
        {
            SetEffectPosition(
                contact.effect, {point.x, point.y, point.z});
        }
        contact.point = point;
        contact.time = 0.0F;
        result.points.push_back(
            {key, point, contact.effect,
             static_cast<std::uint8_t>(node.last), createdEffect});
        ++node.last;
    }
    result.playSound = node.sound != invalidSound &&
                       !result.points.empty();
    return result;
}

void PairPxContactEffect::OnProgress(float deltaTime)
{
    for (auto nodeIterator = contacts_.begin();
         nodeIterator != contacts_.end();)
    {
        auto& node = nodeIterator->second;
        std::size_t eraseBegin = node.last;
        for (std::size_t index = node.last;
             index < node.contacts.size(); ++index)
        {
            auto& contact = node.contacts[index];
            contact.time += deltaTime;
            if (contact.time > contactReleaseSeconds)
            {
                if (contact.effect != invalidEffect)
                {
                    BeginDeleteEffect(contact.effect);
                    pendingReleases_.push_back(
                        {nodeIterator->first,
                         contact.effect,
                         static_cast<std::uint8_t>(index), true});
                }
            }
            else
            {
                eraseBegin = index + 1U;
            }
        }
        if (eraseBegin < node.contacts.size())
            node.contacts.erase(
                node.contacts.begin() +
                    static_cast<std::ptrdiff_t>(eraseBegin),
                node.contacts.end());
        node.last = 0U;
        if (node.contacts.empty())
            nodeIterator = contacts_.erase(nodeIterator);
        else
            ++nodeIterator;
    }
}

std::vector<PairPxContactEffect::Release>
PairPxContactEffect::TakeReleases()
{
    std::vector<Release> result;
    result.swap(pendingReleases_);
    return result;
}

std::size_t PairPxContactEffect::GetPairCount() const noexcept
{
    return contacts_.size();
}

std::size_t PairPxContactEffect::GetContactCount(Key key) const noexcept
{
    const auto found = contacts_.find(key);
    return found == contacts_.end() ? 0U : found->second.contacts.size();
}

bool Logic::ShotPlan::Get(SlotType type) const noexcept
{
    return slots[static_cast<std::size_t>(type)];
}

Logic::ShotPlan Logic::Shot(
    const WeaponItem* weapon, SlotType type, bool human) noexcept
{
    ShotPlan result;
    result.humanShotEvent = human && type != SlotType::Hyper;
    if (weapon != nullptr && weapon->IsReadyShot())
    {
        result.slots[static_cast<std::size_t>(type)] = true;
        result.shotCount = 1U;
    }
    return result;
}

Logic::ShotPlan Logic::ShotAll(
    std::span<WeaponItem* const> primaryWeapons,
    bool human) noexcept
{
    ShotPlan result;
    result.humanShotEvent = human;
    const std::size_t count = std::min(
        primaryWeapons.size(), std::size_t{4U});
    for (std::size_t slot = 0U; slot < count; ++slot)
    {
        if (primaryWeapons[slot] == nullptr ||
            !primaryWeapons[slot]->IsReadyShot())
            continue;
        result.slots[slot +
                     static_cast<std::size_t>(SlotType::Weapon1)] = true;
        ++result.shotCount;
    }
    return result;
}

float Logic::ResolveDamage(
    const Player* targetPlayer, float value,
    DamageType damageType) noexcept
{
    if (targetPlayer == nullptr || damageType == DamageType::Touch)
        return value;
    // Player::GetSlotInst(stReflector) returns the first physical slot.
    return targetPlayer->ReflectDamage(value);
}

GameObject::DamageResult Logic::Damage(
    GameObject& target, std::size_t senderPlayerId,
    float supportedValue, DamageType damageType) noexcept
{
    return target.Damage(senderPlayerId, supportedValue, damageType);
}

GameObject::DamageResult Logic::Damage(
    GameObject& target, std::size_t senderPlayerId,
    float supportedValue, float authoritativeLife,
    bool authoritativeDeath, DamageType damageType) noexcept
{
    return target.Damage(senderPlayerId, supportedValue,
                         authoritativeLife, authoritativeDeath,
                         damageType);
}

Logic::TakeBonusResult Logic::TakeBonus(
    Player* player, GameObject* bonus, PlayerBonusType type,
    float value,
    const std::vector<std::uint32_t>& maximumCharges,
    float randomUnit, bool networkGame,
    bool senderNetworkPlayerAvailable) noexcept
{
    if (player == nullptr || bonus == nullptr || player->IsDestroyed() ||
        bonus->destroyed)
        return {};
    if (networkGame)
    {
        if (!senderNetworkPlayerAvailable)
            return {};
        // NetPlayer::TakeBonus emits the RPC only from its owner. Logic still
        // returns true on every peer and does not mutate Player or bonus.
        return {{}, true, false, true};
    }
    return {player->TakeBonus(
                *bonus, type, value, maximumCharges, randomUnit),
            true, true, false};
}

Logic::MineContactResult Logic::MineContact(
    Proj* sender, GameObject* target, bool networkGame,
    bool targetNetworkPlayerAvailable) noexcept
{
    if (sender == nullptr || target == nullptr)
        return {};
    if (networkGame)
    {
        if (!targetNetworkPlayerAvailable)
            return {};
        return {true, true, false};
    }
    return {true, false, true};
}

Logic::Logic()
    : behaviors_(std::make_unique<LogicBehaviors>(this))
{
}

Logic::~Logic()
{
    DetachWorld();
    CleanGameObjs();
}

void Logic::RegGameObj(GameObject* value)
{
    if (value == nullptr)
        return;
    const auto duplicate = std::find_if(
        gameObjects_.begin(), gameObjects_.end(),
        [&](const auto& object) { return object.get() == value; });
    if (duplicate != gameObjects_.end())
        return;
    value->SetLogic(this);
    gameObjects_.emplace_back(value);
}

bool Logic::HasGameObj(const GameObject* value) const noexcept
{
    return value != nullptr &&
           std::any_of(
               gameObjects_.begin(), gameObjects_.end(),
               [&](const auto& object) {
                   return object.get() == value;
               });
}

void Logic::CleanGameObjs() noexcept
{
    for (auto& object : gameObjects_)
    {
        if (object != nullptr)
            object->SetLogic(nullptr);
    }
    gameObjects_.clear();
}

Logic::GameObjectProgress Logic::ProgressGameObjs(
    float deltaTime) noexcept
{
    GameObjectProgress result;
    for (auto iterator = gameObjects_.begin();
         iterator != gameObjects_.end();)
    {
        auto& object = *iterator;
        if (object == nullptr)
        {
            iterator = gameObjects_.erase(iterator);
            ++result.removed;
            continue;
        }
        ++result.progressed;
        const auto* projectile = dynamic_cast<const Proj*>(object.get());
        object->OnProgress(
            deltaTime,
            projectile == nullptr ||
                !projectile->IsExternalLifetimeManaged());
        if (object->GetLiveState() == GameObject::LiveState::Death)
        {
            object->SetLogic(nullptr);
            iterator = gameObjects_.erase(iterator);
            ++result.removed;
        }
        else
        {
            ++iterator;
        }
    }
    return result;
}

Logic::ProgressResult Logic::OnProgress(float deltaTime) noexcept
{
    ProgressResult result;
    const auto convert = [](MapObjects::ProgressResult value) {
        return GameObjectProgress{value.progressed, value.removed};
    };
    if (map_ != nullptr)
    {
        result.decoration = convert(
            map_->GetMapObjList(MapObjCategory::Decoration)
                .OnProgressSpecial(deltaTime));
        result.effects = convert(
            map_->GetMapObjList(MapObjCategory::Effects)
                .OnProgress(deltaTime));
        result.cars = convert(
            map_->GetMapObjList(MapObjCategory::Car)
                .OnProgress(deltaTime));
        result.bonuses = convert(
            map_->GetMapObjList(MapObjCategory::Bonus)
                .OnProgress(deltaTime));
    }
    result.transient = ProgressGameObjs(deltaTime);
    return result;
}

std::size_t Logic::GetGameObjCount() const noexcept
{
    return gameObjects_.size();
}

void Logic::SetMap(Map* value) noexcept { map_ = value; }
Map* Logic::GetMap() noexcept { return map_; }
const Map* Logic::GetMap() const noexcept { return map_; }

void Logic::AttachWorld(WorldEventPump* world) noexcept
{
    if (world_ == world)
        return;
    DetachWorld();
    world_ = world;
    if (world_ == nullptr)
        return;
    world_->SetHost(this);
    behaviors_->AttachWorld();
}

void Logic::DetachWorld() noexcept
{
    if (world_ == nullptr)
        return;
    behaviors_->DetachWorld();
    world_->SetHost(nullptr);
    world_ = nullptr;
}

WorldEventPump* Logic::GetWorld() noexcept { return world_; }
const WorldEventPump* Logic::GetWorld() const noexcept { return world_; }

void Logic::RegFixedStepEvent(FixedStepEvent* event)
{
    if (world_ != nullptr)
        world_->RegFixedStepEvent(event);
}

void Logic::UnregFixedStepEvent(FixedStepEvent* event) noexcept
{
    if (world_ != nullptr)
        world_->UnregFixedStepEvent(event);
}

void Logic::RegProgressEvent(ProgressEvent* event)
{
    if (world_ != nullptr)
        world_->RegProgressEvent(event);
}

void Logic::UnregProgressEvent(ProgressEvent* event) noexcept
{
    if (world_ != nullptr)
        world_->UnregProgressEvent(event);
}

void Logic::RegLateProgressEvent(LateProgressEvent* event)
{
    if (world_ != nullptr)
        world_->RegLateProgressEvent(event);
}

void Logic::UnregLateProgressEvent(LateProgressEvent* event) noexcept
{
    if (world_ != nullptr)
        world_->UnregLateProgressEvent(event);
}

void Logic::RegFrameEvent(FrameEvent* event)
{
    if (world_ != nullptr)
        world_->RegFrameEvent(event);
}

void Logic::UnregFrameEvent(FrameEvent* event) noexcept
{
    if (world_ != nullptr)
        world_->UnregFrameEvent(event);
}

void Logic::OnLogicProgress(float deltaTime)
{
    lastProgressResult_ = OnProgress(deltaTime);
}

const Logic::ProgressResult& Logic::GetLastProgressResult() const noexcept
{
    return lastProgressResult_;
}

void Logic::ResetContactBehavior(
    std::size_t soundCount, std::string effectRecord) noexcept
{
    auto& behavior = GetPairPxContactEffect();
    behavior.Reset(soundCount);
    behavior.SetEffectRecord(std::move(effectRecord));
}

PairPxContactEffect::ContactResult Logic::OnContact(
    PairPxContactEffect::Key key, float frictionForce,
    bool firstShapeIsWheel, bool secondShapeIsWheel,
    std::span<const PairPxContactEffect::Point> points,
    float randomUnit)
{
    return behaviors_->OnContact(
        key, frictionForce, firstShapeIsWheel,
        secondShapeIsWheel, points, randomUnit);
}

LogicBehaviors& Logic::GetBehaviors() noexcept
{
    return *behaviors_;
}

const LogicBehaviors& Logic::GetBehaviors() const noexcept
{
    return *behaviors_;
}

PairPxContactEffect& Logic::GetPairPxContactEffect() noexcept
{
    return *static_cast<PairPxContactEffect*>(
        behaviors_->Find(LogicBehaviorType::PairPxContactEffect));
}
const PairPxContactEffect& Logic::GetPairPxContactEffect() const noexcept
{
    return *static_cast<const PairPxContactEffect*>(
        behaviors_->Find(LogicBehaviorType::PairPxContactEffect));
}

const Logic::ContactRange& Logic::GetTouchBorderDamage() const noexcept
{
    return touchBorderDamage_;
}
void Logic::SetTouchBorderDamage(ContactRange value) noexcept
{
    touchBorderDamage_ = value;
}
const Logic::ContactRange&
Logic::GetTouchBorderDamageForce() const noexcept
{
    return touchBorderDamageForce_;
}
void Logic::SetTouchBorderDamageForce(ContactRange value) noexcept
{
    touchBorderDamageForce_ = value;
}
const Logic::ContactRange& Logic::GetTouchCarDamage() const noexcept
{
    return touchCarDamage_;
}
void Logic::SetTouchCarDamage(ContactRange value) noexcept
{
    touchCarDamage_ = value;
}
const Logic::ContactRange& Logic::GetTouchCarDamageForce() const noexcept
{
    return touchCarDamageForce_;
}
void Logic::SetTouchCarDamageForce(ContactRange value) noexcept
{
    touchCarDamageForce_ = value;
}

} // namespace r3d::game::originalrace::source
