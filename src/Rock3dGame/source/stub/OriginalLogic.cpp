#include "OriginalLogic.h"

#include "OriginalMap.h"

#include <algorithm>
#include <cmath>

namespace r3d::game::originalrace::source
{

bool PairPxContactEffect::Key::operator<(
    const Key& other) const noexcept
{
    return actor1 == other.actor1 ? actor2 < other.actor2
                                  : actor1 < other.actor1;
}

void PairPxContactEffect::Reset(std::size_t soundCount) noexcept
{
    contacts_.clear();
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
        const bool createdEffect = !contact.effect;
        contact.effect = true;
        contact.point = point;
        contact.time = 0.0F;
        result.points.push_back(
            {key, point, static_cast<std::uint8_t>(node.last),
             createdEffect});
        ++node.last;
    }
    result.playSound = node.sound != invalidSound &&
                       !result.points.empty();
    return result;
}

std::vector<PairPxContactEffect::Release>
PairPxContactEffect::OnProgress(float deltaTime)
{
    std::vector<Release> released;
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
                if (contact.effect)
                {
                    released.push_back(
                        {nodeIterator->first,
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
    return released;
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
        primaryWeapons.size(), WeaponRack::primarySlotCount);
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
    float randomUnit) noexcept
{
    if (player == nullptr || bonus == nullptr || player->destroyed ||
        bonus->destroyed)
        return {};
    return {player->TakeBonus(
                *bonus, type, value, maximumCharges, randomUnit),
            true};
}

Logic::~Logic()
{
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
        object->OnProgress(deltaTime);
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

void Logic::ResetContactBehavior(std::size_t soundCount) noexcept
{
    pairPxContactEffect_.Reset(soundCount);
}

PairPxContactEffect& Logic::GetPairPxContactEffect() noexcept
{
    return pairPxContactEffect_;
}
const PairPxContactEffect& Logic::GetPairPxContactEffect() const noexcept
{
    return pairPxContactEffect_;
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
