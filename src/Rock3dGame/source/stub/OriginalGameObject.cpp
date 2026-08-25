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

GameObject::DamageResult DestrObj::Damage(
    std::size_t senderPlayerId, float value,
    DamageType damageType) noexcept
{
    auto result = GameObject::Damage(
        senderPlayerId, value, damageType);
    checkDestruction_ = checkDestruction_ || result.death;
    return result;
}

GameObject::DamageResult DestrObj::Damage(
    std::size_t senderPlayerId, float value, float newLife,
    bool death, DamageType damageType) noexcept
{
    auto result = GameObject::Damage(
        senderPlayerId, value, newLife, death, damageType);
    checkDestruction_ = checkDestruction_ || result.death;
    return result;
}

bool DestrObj::OnProgress(float deltaTime) noexcept
{
    GameObject::OnProgress(deltaTime);
    if (!checkDestruction_)
        return false;
    checkDestruction_ = false;
    return true;
}

bool DestrObj::HasPendingDestruction() const noexcept
{
    return checkDestruction_;
}

LowLifePoints::LowLifePoints(float lifeLevel) noexcept
{
    Reset(lifeLevel);
}

void LowLifePoints::Reset(float lifeLevel) noexcept
{
    lifeLevel_ = lifeLevel;
    effectSeconds_ = 0.0F;
    effectMaked_ = false;
}

LowLifePoints::ProgressResult LowLifePoints::OnProgress(
    const GameObject& gameObject, float deltaTime) noexcept
{
    ProgressResult result;
    const float maximumLife = gameObject.GetMaxLife();
    const float life = gameObject.GetLife();
    const bool lowLife =
        gameObject.GetLiveState() != GameObject::LiveState::Death &&
        maximumLife > 0.0F && life > 0.0F &&
        life / maximumLife < lifeLevel_;
    if (lowLife)
    {
        if (!effectMaked_)
        {
            effectMaked_ = true;
            result.activated = true;
        }
        effectSeconds_ += deltaTime;
    }
    else if (effectMaked_)
    {
        effectMaked_ = false;
        effectSeconds_ = 0.0F;
        result.released = true;
    }
    return result;
}

float LowLifePoints::GetLifeLevel() const noexcept
{
    return lifeLevel_;
}

void LowLifePoints::SetLifeLevel(float value) noexcept
{
    lifeLevel_ = value;
}

bool LowLifePoints::IsEffectMaked() const noexcept
{
    return effectMaked_;
}

float LowLifePoints::GetEffectSeconds() const noexcept
{
    return effectSeconds_;
}

DamageEffect::DamageEffect(
    DamageType damageType, float maximumTimeLife) noexcept
    : damageType_(damageType), maximumTimeLife_(maximumTimeLife)
{
}

void DamageEffect::Reset() noexcept
{
    effectSeconds_ = 0.0F;
    effectMaked_ = false;
}

bool DamageEffect::OnDamage(DamageType damageType) noexcept
{
    if (damageType_ != damageType)
        return false;
    const bool created = !effectMaked_;
    if (created)
    {
        effectMaked_ = true;
        effectSeconds_ = 0.0F;
    }
    return created;
}

void DamageEffect::OnProgress(float deltaTime) noexcept
{
    if (!effectMaked_)
        return;
    effectSeconds_ += deltaTime;
    if (maximumTimeLife_ > 0.0F &&
        effectSeconds_ > maximumTimeLife_)
    {
        effectSeconds_ = 0.0F;
        effectMaked_ = false;
    }
}

DamageType DamageEffect::GetDamageType() const noexcept
{
    return damageType_;
}

void DamageEffect::SetDamageType(DamageType value) noexcept
{
    damageType_ = value;
}

bool DamageEffect::IsEffectMaked() const noexcept
{
    return effectMaked_;
}

float DamageEffect::GetEffectSeconds() const noexcept
{
    return effectSeconds_;
}

void ImmortalEffect::Reset() noexcept
{
    fadeInTime_ = -1.0F;
    fadeOutTime_ = -1.0F;
    damageTime_ = -1.0F;
    effectSeconds_ = 0.0F;
    effectMaked_ = false;
}

void ImmortalEffect::OnImmortalStatus(bool status) noexcept
{
    if (status)
    {
        // EventEffect::MakeEffect keeps an existing fading actor. The source
        // deliberately does not cancel fadeOutTime_ when a new shield starts.
        effectMaked_ = true;
        fadeInTime_ = 0.0F;
    }
    else
    {
        fadeOutTime_ = 0.0F;
    }
}

void ImmortalEffect::OnDamage() noexcept
{
    if (effectMaked_)
        damageTime_ = 0.0F;
}

void ImmortalEffect::OnProgress(float deltaTime) noexcept
{
    if (effectMaked_ && damageTime_ >= 0.0F)
    {
        const float alpha = std::clamp(
            damageTime_ / damageSeconds, 0.0F, 1.0F);
        if (alpha >= 1.0F)
            damageTime_ = -1.0F;
        else
            damageTime_ += deltaTime;
    }
    if (effectMaked_ && fadeInTime_ >= 0.0F)
    {
        fadeInTime_ += deltaTime;
        if (fadeInTime_ / fadeSeconds >= 1.0F)
            fadeInTime_ = -1.0F;
    }
    if (effectMaked_ && fadeOutTime_ >= 0.0F)
    {
        fadeOutTime_ += deltaTime;
        if (fadeOutTime_ / fadeSeconds >= 1.0F)
        {
            fadeOutTime_ = -1.0F;
            damageTime_ = -1.0F;
            effectSeconds_ = 0.0F;
            effectMaked_ = false;
        }
    }
    if (effectMaked_)
        effectSeconds_ += deltaTime;
}

bool ImmortalEffect::IsEffectMaked() const noexcept
{
    return effectMaked_;
}

float ImmortalEffect::GetEffectSeconds() const noexcept
{
    return effectSeconds_;
}

float ImmortalEffect::GetFadeInTime() const noexcept
{
    return fadeInTime_;
}

float ImmortalEffect::GetFadeOutTime() const noexcept
{
    return fadeOutTime_;
}

float ImmortalEffect::GetDamageTime() const noexcept
{
    return damageTime_;
}

float ImmortalEffect::GetScale() const noexcept
{
    // OnProgress applies fade-in first and fade-out second, so an overlapping
    // fade-out owns the final actor scale exactly as in GameBase.cpp.
    if (fadeOutTime_ >= 0.0F)
    {
        return 1.0F - std::clamp(
            fadeOutTime_ / fadeSeconds, 0.0F, 1.0F);
    }
    if (fadeInTime_ >= 0.0F)
    {
        return std::clamp(
            fadeInTime_ / fadeSeconds, 0.0F, 1.0F);
    }
    return 1.0F;
}

float ImmortalEffect::GetDamageAlpha() const noexcept
{
    if (damageTime_ < 0.0F)
        return 1.0F;
    const float alpha = std::clamp(
        damageTime_ / damageSeconds, 0.0F, 1.0F);
    return 1.0F + 2.5F * (1.0F - alpha);
}

} // namespace r3d::game::originalrace::source
