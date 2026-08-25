#include "OriginalGameObject.h"

#include <algorithm>

namespace r3d::game::originalrace::source
{

void GameObject::ResetGameObject(float maximumLifeValue) noexcept
{
    maximumLife = maximumLifeValue;
    life = maximumLife;
    timeLife = 0.0F;
    maximumTimeLife = -1.0F;
    shieldSeconds = 0.0F;
    touchAttacker = undefinedPlayerId;
    touchAttributionSeconds = 0.0F;
    immortalFlag = false;
    destroyed = false;
}

GameObject::ProgressResult GameObject::OnProgress(
    float deltaTime) noexcept
{
    ProgressResult result;
    timeLife += deltaTime;
    if (shieldSeconds > 0.0F)
    {
        shieldSeconds -= deltaTime;
        if (shieldSeconds <= 0.0F)
        {
            shieldSeconds = 0.0F;
            result.immortalityEnded = true;
        }
    }
    if (touchAttributionSeconds > 0.0F &&
        (touchAttributionSeconds -= deltaTime) <= 0.0F)
    {
        touchAttacker = undefinedPlayerId;
        touchAttributionSeconds = 0.0F;
        result.touchAttributionEnded = true;
    }
    if (maximumTimeLife > 0.0F && timeLife > maximumTimeLife)
        result.lifetimeDeath = Death();
    return result;
}

GameObject::DamageResult GameObject::Damage(
    std::size_t senderPlayerId, float value,
    DamageType damageType) noexcept
{
    const bool immortal = IsImmortal();
    const float newLife = immortal ? life : life - value;
    return Damage(senderPlayerId, value, newLife,
                  !immortal && newLife <= 0.0F, damageType);
}

GameObject::DamageResult GameObject::Damage(
    std::size_t senderPlayerId, float value, float newLife,
    bool death, DamageType damageType) noexcept
{
    DamageResult result;
    result.previousLife = life;
    result.newLife = newLife;
    result.damage = value;
    result.damageType = damageType;

    // GameObject.cpp assigns authoritative life before checking lsDeath.
    life = newLife;
    result.wasLive = !destroyed;
    if (!result.wasLive)
        return result;

    if (senderPlayerId != undefinedPlayerId &&
        damageType == DamageType::Touch)
    {
        touchAttacker = senderPlayerId;
        touchAttributionSeconds = 3.0F;
    }
    if (!death)
        return result;

    if (damageType != DamageType::Touch)
    {
        touchAttacker = undefinedPlayerId;
        touchAttributionSeconds = 0.0F;
    }
    destroyed = true;
    result.death = true;
    result.killCredit = senderPlayerId != undefinedPlayerId &&
                        damageType != DamageType::Mine;
    return result;
}

bool GameObject::Death(DamageType damageType) noexcept
{
    if (destroyed)
        return false;
    if (damageType != DamageType::Touch)
    {
        touchAttacker = undefinedPlayerId;
        touchAttributionSeconds = 0.0F;
    }
    destroyed = true;
    return true;
}

bool GameObject::Resc() noexcept
{
    if (!destroyed)
        return false;
    destroyed = false;
    timeLife = 0.0F;
    life = maximumLife;
    return true;
}

void GameObject::Healt(float value) noexcept
{
    life = std::min(life + value, maximumLife);
}

void GameObject::SetImmortalFlag(bool value) noexcept
{
    immortalFlag = value;
}
bool GameObject::GetImmortalFlag() const noexcept { return immortalFlag; }
void GameObject::Immortal(float time) noexcept { shieldSeconds = time; }
bool GameObject::IsImmortal() const noexcept
{
    return maximumLife < 0.0F || shieldSeconds > 0.0F || immortalFlag;
}
bool GameObject::IsTimedImmortal() const noexcept
{
    return shieldSeconds > 0.0F;
}

GameObject::LiveState GameObject::GetLiveState() const noexcept
{
    return destroyed ? LiveState::Death : LiveState::Live;
}
float GameObject::GetMaxLife() const noexcept { return maximumLife; }
void GameObject::SetMaxLife(float value) noexcept
{
    life = maximumLife = value;
}
float GameObject::GetLife() const noexcept { return life; }
void GameObject::SetLife(float value) noexcept { life = value; }
float GameObject::GetTimeLife() const noexcept { return timeLife; }
void GameObject::SetTimeLife(float value) noexcept { timeLife = value; }
float GameObject::GetMaxTimeLife() const noexcept
{
    return maximumTimeLife;
}
void GameObject::SetMaxTimeLife(float value) noexcept
{
    maximumTimeLife = value;
}
std::size_t GameObject::GetTouchPlayerId() const noexcept
{
    return touchAttacker;
}

} // namespace r3d::game::originalrace::source
