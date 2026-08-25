#include "OriginalLogic.h"

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
    std::span<const WeaponItem> primaryWeapons,
    bool human) noexcept
{
    ShotPlan result;
    result.humanShotEvent = human;
    const std::size_t count = std::min(
        primaryWeapons.size(), WeaponRack::primarySlotCount);
    for (std::size_t slot = 0U; slot < count; ++slot)
    {
        if (!primaryWeapons[slot].IsReadyShot())
            continue;
        result.slots[slot +
                     static_cast<std::size_t>(SlotType::Weapon1)] = true;
        ++result.shotCount;
    }
    return result;
}

float Logic::ResolveDamage(
    const PlayerItemRack* targetItems, float value,
    DamageType damageType) noexcept
{
    if (targetItems == nullptr || damageType == DamageType::Touch)
        return value;
    // Player::GetSlotInst(stReflector) returns the first physical slot.
    return targetItems->Reflect(value);
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

} // namespace r3d::game::originalrace::source
