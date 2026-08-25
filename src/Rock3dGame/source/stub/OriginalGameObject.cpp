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
    // Direct Death does not clear _touchPlayerId in the Windows source.
    // TouchDeath relies on that distinction so an earlier car contact can
    // still be attributed when the victim crosses the death plane.
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

bool DestrObj::Death(DamageType damageType) noexcept
{
    const bool died = GameObject::Death(damageType);
    checkDestruction_ = checkDestruction_ || died;
    return died;
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

bool TouchDeath::OnContact(GameObject* target) const noexcept
{
    return target != nullptr && target->Death(DamageType::DeathPlane);
}

void ResurrectObj::Reset() noexcept
{
    resurrect_ = false;
}

bool ResurrectObj::OnDeath(GameObject& owner) noexcept
{
    if (resurrect_)
        return false;
    resurrect_ = true;
    owner.Resc();
    return true;
}

bool ResurrectObj::IsResurrect() const noexcept
{
    return resurrect_;
}

void FxSystemWaitingEnd::Reset() noexcept
{
    ResurrectObj::Reset();
    fading_ = false;
}

FxSystemWaitingEnd::ProgressResult FxSystemWaitingEnd::OnDeath(
    GameObject& owner) noexcept
{
    ProgressResult result;
    result.beginFading = ResurrectObj::OnDeath(owner);
    if (result.beginFading)
        fading_ = true;
    return result;
}

FxSystemWaitingEnd::ProgressResult FxSystemWaitingEnd::OnProgress(
    GameObject& owner, std::size_t liveParticles) noexcept
{
    ProgressResult result;
    if (!IsResurrect() || liveParticles != 0U || owner.destroyed)
        return result;
    result.finalDeath = owner.Death();
    return result;
}

bool FxSystemWaitingEnd::IsFading() const noexcept
{
    return fading_;
}

void EventEffect::Reset() noexcept
{
    effectMaked_ = false;
}

bool EventEffect::MakeEffect() noexcept
{
    if (effectMaked_)
        return false;
    effectMaked_ = true;
    return true;
}

bool EventEffect::FreeEffect() noexcept
{
    if (!effectMaked_)
        return false;
    effectMaked_ = false;
    return true;
}

bool EventEffect::OnDestroyEffect() noexcept
{
    return FreeEffect();
}

bool EventEffect::IsEffectMaked() const noexcept
{
    return effectMaked_;
}

void LifeEffect::Reset() noexcept
{
    EventEffect::Reset();
    play_ = false;
}

bool LifeEffect::OnProgress(bool sourceAvailable) noexcept
{
    if (play_ || !sourceAvailable)
        return false;
    play_ = true;
    return true;
}

bool LifeEffect::HasPlayed() const noexcept
{
    return play_;
}

LowLifePoints::LowLifePoints(float lifeLevel) noexcept
{
    Reset(lifeLevel);
}

void LowLifePoints::Reset(float lifeLevel) noexcept
{
    lifeLevel_ = lifeLevel;
    effectSeconds_ = 0.0F;
    eventEffect_.Reset();
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
        if (eventEffect_.MakeEffect())
        {
            result.activated = true;
        }
        effectSeconds_ += deltaTime;
    }
    else if (eventEffect_.FreeEffect())
    {
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
    return eventEffect_.IsEffectMaked();
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
    eventEffect_.Reset();
}

bool DamageEffect::OnDamage(DamageType damageType) noexcept
{
    if (damageType_ != damageType)
        return false;
    const bool created = eventEffect_.MakeEffect();
    if (created)
        effectSeconds_ = 0.0F;
    return created;
}

void DamageEffect::OnProgress(float deltaTime) noexcept
{
    if (!eventEffect_.IsEffectMaked())
        return;
    effectSeconds_ += deltaTime;
    if (maximumTimeLife_ > 0.0F &&
        effectSeconds_ > maximumTimeLife_)
    {
        effectSeconds_ = 0.0F;
        eventEffect_.FreeEffect();
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
    return eventEffect_.IsEffectMaked();
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
    eventEffect_.Reset();
}

void ImmortalEffect::OnImmortalStatus(bool status) noexcept
{
    if (status)
    {
        // EventEffect::MakeEffect keeps an existing fading actor. The source
        // deliberately does not cancel fadeOutTime_ when a new shield starts.
        eventEffect_.MakeEffect();
        fadeInTime_ = 0.0F;
    }
    else
    {
        fadeOutTime_ = 0.0F;
    }
}

void ImmortalEffect::OnDamage() noexcept
{
    if (eventEffect_.IsEffectMaked())
        damageTime_ = 0.0F;
}

void ImmortalEffect::OnProgress(float deltaTime) noexcept
{
    if (eventEffect_.IsEffectMaked() && damageTime_ >= 0.0F)
    {
        const float alpha = std::clamp(
            damageTime_ / damageSeconds, 0.0F, 1.0F);
        if (alpha >= 1.0F)
            damageTime_ = -1.0F;
        else
            damageTime_ += deltaTime;
    }
    if (eventEffect_.IsEffectMaked() && fadeInTime_ >= 0.0F)
    {
        fadeInTime_ += deltaTime;
        if (fadeInTime_ / fadeSeconds >= 1.0F)
            fadeInTime_ = -1.0F;
    }
    if (eventEffect_.IsEffectMaked() && fadeOutTime_ >= 0.0F)
    {
        fadeOutTime_ += deltaTime;
        if (fadeOutTime_ / fadeSeconds >= 1.0F)
        {
            fadeOutTime_ = -1.0F;
            damageTime_ = -1.0F;
            effectSeconds_ = 0.0F;
            eventEffect_.FreeEffect();
        }
    }
    if (eventEffect_.IsEffectMaked())
        effectSeconds_ += deltaTime;
}

bool ImmortalEffect::IsEffectMaked() const noexcept
{
    return eventEffect_.IsEffectMaked();
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

void SlowEffect::Reset() noexcept
{
    maximumTimeLife_ = -1.0F;
    timeLife_ = 0.0F;
    weapon_ = GameObject::undefinedPlayerId;
    projectile_ = GameObject::undefinedPlayerId;
    eventEffect_.Reset();
}

bool SlowEffect::Attach(
    float maximumTimeLife, std::size_t weapon,
    std::size_t projectile) noexcept
{
    // FrostRayUpdate only adds the behavior when Find<SlowEffect>() fails;
    // repeated ray contacts do not refresh the child effect's lifetime.
    if (eventEffect_.IsEffectMaked())
        return false;
    maximumTimeLife_ = maximumTimeLife;
    timeLife_ = 0.0F;
    weapon_ = weapon;
    projectile_ = projectile;
    eventEffect_.MakeEffect();
    return true;
}

SlowEffect::ProgressResult SlowEffect::OnProgress(
    float deltaTime, float linearSpeed) noexcept
{
    ProgressResult result;
    if (!eventEffect_.IsEffectMaked())
        return result;
    // SlowEffect::OnProgress normalizes and clamps the actor velocity to 20
    // only while it is moving faster than both source thresholds.
    result.limitSpeed =
        linearSpeed > 1.0F && linearSpeed > maximumSpeed;
    timeLife_ += deltaTime;
    if (maximumTimeLife_ > 0.0F && timeLife_ > maximumTimeLife_)
    {
        Reset();
        result.released = true;
    }
    return result;
}

bool SlowEffect::IsEffectMaked() const noexcept
{
    return eventEffect_.IsEffectMaked();
}

float SlowEffect::GetRemainingSeconds() const noexcept
{
    if (!eventEffect_.IsEffectMaked() || maximumTimeLife_ <= 0.0F)
        return 0.0F;
    return std::max(maximumTimeLife_ - timeLife_, 0.0F);
}

std::size_t SlowEffect::GetWeapon() const noexcept
{
    return weapon_;
}

std::size_t SlowEffect::GetProjectile() const noexcept
{
    return projectile_;
}

} // namespace r3d::game::originalrace::source
