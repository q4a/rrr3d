#include "OriginalGameObject.h"

#include <algorithm>
#include <cmath>

namespace r3d::game::originalrace::source
{

namespace
{

using SyncVector = GameObjectFrameSync::Vector;
using SyncQuaternion = GameObjectFrameSync::Quaternion;

constexpr float syncPi = 3.14159265358979323846F;

SyncVector subtractSync(SyncVector left, SyncVector right) noexcept
{
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

float lengthSync(SyncVector value) noexcept
{
    return std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z);
}

SyncVector normalizedSync(SyncVector value) noexcept
{
    const float valueLength = lengthSync(value);
    if (valueLength <= 0.000001F)
        return {};
    return {
        value.x / valueLength, value.y / valueLength,
        value.z / valueLength};
}

SyncQuaternion normalizedSync(SyncQuaternion value) noexcept
{
    const float valueLength = std::sqrt(
        value.x * value.x + value.y * value.y +
        value.z * value.z + value.w * value.w);
    if (valueLength <= 0.000001F)
        return {};
    return {
        value.x / valueLength, value.y / valueLength,
        value.z / valueLength, value.w / valueLength};
}

SyncQuaternion multiplySync(
    SyncQuaternion left, SyncQuaternion right) noexcept
{
    return normalizedSync({
        left.w * right.x + left.x * right.w +
            left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z +
            left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y -
            left.y * right.x + left.z * right.w,
        left.w * right.w - left.x * right.x -
            left.y * right.y - left.z * right.z});
}

SyncQuaternion inverseSync(SyncQuaternion value) noexcept
{
    value = normalizedSync(value);
    return {-value.x, -value.y, -value.z, value.w};
}

SyncQuaternion rotationSync(
    SyncQuaternion current, SyncQuaternion next) noexcept
{
    return multiplySync(next, inverseSync(current));
}

float angleSync(SyncQuaternion value) noexcept
{
    value = normalizedSync(value);
    return 2.0F * std::acos(std::clamp(value.w, -1.0F, 1.0F));
}

SyncVector axisSync(SyncQuaternion value) noexcept
{
    value = normalizedSync(value);
    const float divisor = std::sqrt(std::max(
        1.0F - value.w * value.w, 0.0F));
    if (divisor <= 0.000001F)
        return {1.0F, 0.0F, 0.0F};
    return {
        value.x / divisor, value.y / divisor,
        value.z / divisor};
}

SyncQuaternion angleAxisSync(float angle, SyncVector axis) noexcept
{
    axis = normalizedSync(axis);
    if (lengthSync(axis) <= 0.000001F)
        return {};
    const float sine = std::sin(angle * 0.5F);
    return normalizedSync({
        axis.x * sine, axis.y * sine, axis.z * sine,
        std::cos(angle * 0.5F)});
}

float shortestSignedAngle(float angle) noexcept
{
    const float magnitude = std::abs(angle);
    if (magnitude <= syncPi)
        return angle;
    return (2.0F * syncPi - magnitude) *
           (angle > 0.0F ? -1.0F : 1.0F);
}

} // namespace

GameObject::GameObject(const GameObject& other) noexcept
{
    *this = other;
}

GameObject& GameObject::operator=(const GameObject& other) noexcept
{
    if (this == &other)
        return *this;
    life = other.life;
    maximumLife = other.maximumLife;
    timeLife = other.timeLife;
    maximumTimeLife = other.maximumTimeLife;
    shieldSeconds = other.shieldSeconds;
    touchAttacker = other.touchAttacker;
    touchAttributionSeconds = other.touchAttributionSeconds;
    immortalFlag = other.immortalFlag;
    destroyed = other.destroyed;
    objectDestroyed_ = other.objectDestroyed_;
    // GameObject::Assign does not copy the legacy listener container. Its
    // entries point at behaviors owned by the concrete source object.
    listeners_.clear();
    return *this;
}

GameObject::GameObject(GameObject&& other) noexcept
{
    *this = other;
}

GameObject& GameObject::operator=(GameObject&& other) noexcept
{
    return *this = static_cast<const GameObject&>(other);
}

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
    objectDestroyed_ = false;
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
            OnImmortalStatusEvent(false);
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

    // GameObject::Damage dispatches listeners immediately after assigning
    // authoritative life, before touch attribution and death events.
    OnDamageEvent(value, damageType);
    const auto damageListeners = listeners_;
    for (auto* listener : damageListeners)
    {
        if (listener != nullptr)
            listener->OnDamage(*this, value, damageType);
    }

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
    SendDeath(damageType, nullptr);
    return result;
}

bool GameObject::Death(
    DamageType damageType, GameObject* target) noexcept
{
    if (destroyed)
        return false;
    // Direct Death does not clear _touchPlayerId in the Windows source.
    // TouchDeath relies on that distinction so an earlier car contact can
    // still be attributed when the victim crosses the death plane.
    destroyed = true;
    SendDeath(damageType, target);
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

void GameObject::LowLife() noexcept
{
    OnLowLifeEvent();
    const auto lowLifeListeners = listeners_;
    for (auto* listener : lowLifeListeners)
    {
        if (listener != nullptr)
            listener->OnLowLife(*this);
    }
}

bool GameObject::InsertListener(GameObjectListener* value) noexcept
{
    if (value == nullptr ||
        std::find(listeners_.begin(), listeners_.end(), value) !=
            listeners_.end())
    {
        return false;
    }
    listeners_.push_back(value);
    return true;
}

bool GameObject::RemoveListener(GameObjectListener* value) noexcept
{
    const auto found = std::find(
        listeners_.begin(), listeners_.end(), value);
    if (found == listeners_.end())
        return false;
    listeners_.erase(found);
    return true;
}

void GameObject::ClearListenerList() noexcept
{
    listeners_.clear();
}

std::size_t GameObject::GetListenerCount() const noexcept
{
    return listeners_.size();
}

bool GameObject::DestroyObject() noexcept
{
    if (objectDestroyed_)
        return false;
    objectDestroyed_ = true;
    OnDestroyEvent();
    const auto destroyListeners = listeners_;
    for (auto* listener : destroyListeners)
    {
        if (listener != nullptr)
            listener->OnDestroy(*this);
    }
    return true;
}

bool GameObject::IsObjectDestroyed() const noexcept
{
    return objectDestroyed_;
}

void GameObject::SendDeath(
    DamageType damageType, GameObject* target) noexcept
{
    OnDeathEvent(damageType, target);
    const auto deathListeners = listeners_;
    for (auto* listener : deathListeners)
    {
        if (listener != nullptr)
            listener->OnDeath(*this, damageType, target);
    }
}

void GameObject::SetImmortalFlag(bool value) noexcept
{
    immortalFlag = value;
}
bool GameObject::GetImmortalFlag() const noexcept { return immortalFlag; }
void GameObject::Immortal(float time) noexcept
{
    const bool turnOn = shieldSeconds <= 0.0F && time > 0.0F;
    shieldSeconds = time;
    if (turnOn)
        OnImmortalStatusEvent(true);
}
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

void GameObjectFrameSync::Reset() noexcept
{
    *this = {};
}

void GameObjectFrameSync::SetPosSync(Vector value) noexcept
{
    posSync_ = value;
    posSyncDirection_ = normalizedSync(value);
    posSyncLength_ = lengthSync(value);
}

void GameObjectFrameSync::SetRotSync(Quaternion value) noexcept
{
    rotSync_ = normalizedSync(value);
    rotSyncAxis_ = axisSync(rotSync_);
    rotSyncAngle_ = shortestSignedAngle(angleSync(rotSync_));
}

void GameObjectFrameSync::SetPosSync2(
    Vector current, Vector next) noexcept
{
    posSyncDirection2_ = subtractSync(current, next);
    posSyncDistance2_ = lengthSync(posSyncDirection2_);
    posSyncDirection2_ = normalizedSync(posSyncDirection2_);
    posSync2_ = next;
    posSyncLength2_ = lengthSync(posSync2_);
}

void GameObjectFrameSync::SetRotSync2(
    Quaternion current, Quaternion next) noexcept
{
    const Quaternion difference = rotationSync(next, current);
    rotSyncAxis2_ = axisSync(difference);
    rotSyncAngle2_ = shortestSignedAngle(angleSync(difference));
    rotSync2_ = normalizedSync(next);
    rotSyncLength2_ = angleSync(rotSync2_);
}

GameObjectFrameSync::NetworkCorrection
GameObjectFrameSync::OnNetworkPose(
    Vector physicsPosition, Vector graphPosition,
    Quaternion graphRotation, Vector targetPosition,
    Quaternion targetRotation) noexcept
{
    NetworkCorrection result;
    const Vector positionDifference =
        subtractSync(targetPosition, physicsPosition);
    if (lengthSync(positionDifference) > 4.0F)
    {
        SetPosSync(subtractSync(targetPosition, graphPosition));
        result.snapPosition = true;
    }

    const Quaternion rotationDifference =
        rotationSync(graphRotation, targetRotation);
    const float rotationAngle = std::abs(
        shortestSignedAngle(angleSync(rotationDifference)));
    if (rotationAngle > syncPi / 24.0F)
    {
        SetRotSync(rotationDifference);
        result.snapRotation = true;
    }
    return result;
}

GameObjectFrameSync::Pose GameObjectFrameSync::OnFrame(
    Pose physicsPose, float deltaTime) noexcept
{
    deltaTime = std::max(deltaTime, 0.0F);
    Pose result = physicsPose;
    if (posSyncLength_ > 0.0F && posSyncLength_ < 5.0F)
    {
        posSyncLength_ = std::max(
            posSyncLength_ - 5.0F * deltaTime, 0.0F);
        result.position.x -= posSyncDirection_.x * posSyncLength_;
        result.position.y -= posSyncDirection_.y * posSyncLength_;
        result.position.z -= posSyncDirection_.z * posSyncLength_;
    }
    else
    {
        posSyncLength_ = 0.0F;
    }

    if (rotSyncAngle_ != 0.0F)
    {
        if (rotSyncAngle_ > 0.0F)
        {
            rotSyncAngle_ = std::max(
                rotSyncAngle_ - 1.3F * syncPi * deltaTime,
                0.0F);
        }
        else
        {
            rotSyncAngle_ = std::min(
                rotSyncAngle_ + 1.3F * syncPi * deltaTime,
                0.0F);
        }
        result.rotation = multiplySync(
            angleAxisSync(-rotSyncAngle_, rotSyncAxis_),
            result.rotation);
    }

    if (posSyncDistance2_ > 0.0F && posSyncDistance2_ < 5.0F)
    {
        posSyncDistance2_ = std::max(
            posSyncDistance2_ - 5.0F * deltaTime, 0.0F);
        result.position.x +=
            posSync2_.x + posSyncDirection2_.x * posSyncDistance2_;
        result.position.y +=
            posSync2_.y + posSyncDirection2_.y * posSyncDistance2_;
        result.position.z +=
            posSync2_.z + posSyncDirection2_.z * posSyncDistance2_;
    }
    else if (posSyncLength2_ > 0.0F)
    {
        posSyncDistance2_ = 0.0F;
        result.position.x += posSync2_.x;
        result.position.y += posSync2_.y;
        result.position.z += posSync2_.z;
    }

    if (rotSyncAngle2_ != 0.0F)
    {
        if (rotSyncAngle2_ > 0.0F)
        {
            rotSyncAngle2_ = std::max(
                rotSyncAngle2_ - 1.3F * syncPi * deltaTime,
                0.0F);
        }
        else
        {
            rotSyncAngle2_ = std::min(
                rotSyncAngle2_ + 1.3F * syncPi * deltaTime,
                0.0F);
        }
        result.rotation = multiplySync(
            angleAxisSync(-rotSyncAngle2_, rotSyncAxis2_),
            multiplySync(rotSync2_, result.rotation));
    }
    else if (rotSyncLength2_ != 0.0F)
    {
        rotSyncAngle2_ = 0.0F;
        result.rotation = multiplySync(rotSync2_, result.rotation);
    }
    return result;
}

const GameObjectFrameSync::Vector&
GameObjectFrameSync::GetPosSync() const noexcept
{
    return posSync_;
}

const GameObjectFrameSync::Quaternion&
GameObjectFrameSync::GetRotSync() const noexcept
{
    return rotSync_;
}

const GameObjectFrameSync::Vector&
GameObjectFrameSync::GetPosSync2() const noexcept
{
    return posSync2_;
}

const GameObjectFrameSync::Quaternion&
GameObjectFrameSync::GetRotSync2() const noexcept
{
    return rotSync2_;
}

bool GameObjectFrameSync::HasActiveCorrection() const noexcept
{
    return posSyncLength_ != 0.0F || rotSyncAngle_ != 0.0F ||
           posSyncDistance2_ != 0.0F || posSyncLength2_ != 0.0F ||
           rotSyncAngle2_ != 0.0F || rotSyncLength2_ != 0.0F;
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

bool DestrObj::Death(
    DamageType damageType, GameObject* target) noexcept
{
    const bool died = GameObject::Death(damageType, target);
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

void FxSystemSrcSpeed::Reset() noexcept
{
    sourceSpeed_ = {};
}

bool FxSystemSrcSpeed::OnProgress(
    bool physicsActorAvailable, Vector actorLinearVelocity,
    const ParentTransform* parent) noexcept
{
    // The Windows behavior returns before touching FxParticleSystem when the
    // GameObject does not resolve to an NxActor.  Preserve the last value in
    // that case instead of replacing it with zero.
    if (!physicsActorAvailable)
        return false;

    if (parent != nullptr)
    {
        const auto& rotation = parent->rotation;
        const float lengthSquared =
            rotation.x * rotation.x + rotation.y * rotation.y +
            rotation.z * rotation.z + rotation.w * rotation.w;
        Quaternion inverse;
        if (lengthSquared > 0.0000001F)
        {
            inverse = {-rotation.x / lengthSquared,
                       -rotation.y / lengthSquared,
                       -rotation.z / lengthSquared,
                       rotation.w / lengthSquared};
        }

        const Vector twiceCross{
            2.0F * (inverse.y * actorLinearVelocity.z -
                    inverse.z * actorLinearVelocity.y),
            2.0F * (inverse.z * actorLinearVelocity.x -
                    inverse.x * actorLinearVelocity.z),
            2.0F * (inverse.x * actorLinearVelocity.y -
                    inverse.y * actorLinearVelocity.x)};
        Vector local{
            actorLinearVelocity.x + inverse.w * twiceCross.x +
                (inverse.y * twiceCross.z - inverse.z * twiceCross.y),
            actorLinearVelocity.y + inverse.w * twiceCross.y +
                (inverse.z * twiceCross.x - inverse.x * twiceCross.z),
            actorLinearVelocity.z + inverse.w * twiceCross.z +
                (inverse.x * twiceCross.y - inverse.y * twiceCross.x)};

        // Vec3TransformNormal(GetInvWorldMat()) includes inverse scale and
        // excludes only translation.
        const auto inverseScale = [](float value, float scale) noexcept {
            return std::abs(scale) > 0.0000001F ? value / scale : 0.0F;
        };
        local.x = inverseScale(local.x, parent->scale.x);
        local.y = inverseScale(local.y, parent->scale.y);
        local.z = inverseScale(local.z, parent->scale.z);
        sourceSpeed_ = local;
    }
    else
    {
        sourceSpeed_ = actorLinearVelocity;
    }
    return true;
}

const FxSystemSrcSpeed::Vector&
FxSystemSrcSpeed::GetSourceSpeed() const noexcept
{
    return sourceSpeed_;
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

DeathEffect::DeathEffect(bool effectPhysicsIgnoreSenderCar,
                         bool targetChild) noexcept
{
    Reset(effectPhysicsIgnoreSenderCar, targetChild);
}

void DeathEffect::Reset(bool effectPhysicsIgnoreSenderCar,
                        bool targetChild) noexcept
{
    EventEffect::Reset();
    effectPhysicsIgnoreSenderCar_ = effectPhysicsIgnoreSenderCar;
    targetChild_ = targetChild;
}

DeathEffect::SpawnResult DeathEffect::OnDeath(
    bool logicAvailable, bool hasTarget,
    bool senderIsWeaponProjectile) noexcept
{
    SpawnResult result;
    if (!logicAvailable || !MakeEffect())
        return result;
    result.createEffect = true;
    result.targetChild = targetChild_ && hasTarget;
    result.ignoreSenderCar =
        effectPhysicsIgnoreSenderCar_ && senderIsWeaponProjectile;
    return result;
}

bool DeathEffect::GetEffectPxIgnoreSenderCar() const noexcept
{
    return effectPhysicsIgnoreSenderCar_;
}

void DeathEffect::SetEffectPxIgnoreSenderCar(bool value) noexcept
{
    effectPhysicsIgnoreSenderCar_ = value;
}

bool DeathEffect::GetTargetChild() const noexcept
{
    return targetChild_;
}

void DeathEffect::SetTargetChild(bool value) noexcept
{
    targetChild_ = value;
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
    GameObject& gameObject, float deltaTime) noexcept
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
        if (!eventEffect_.IsEffectMaked())
            gameObject.LowLife();
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
