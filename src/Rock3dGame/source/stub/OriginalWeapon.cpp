#include "OriginalWeapon.h"

#include <algorithm>
#include <cmath>

namespace r3d::game::originalrace::source
{

namespace
{

constexpr std::uint32_t masloProjectileType = 10U;

float length(Proj::Vec3 value) noexcept
{
    return std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z);
}

Proj::Vec3 normalized(Proj::Vec3 value) noexcept
{
    const float magnitude = length(value);
    if (magnitude <= 0.000001F)
        return {};
    return {value.x / magnitude, value.y / magnitude,
            value.z / magnitude};
}

float dot(Proj::Vec3 first, Proj::Vec3 second) noexcept
{
    return first.x * second.x + first.y * second.y +
           first.z * second.z;
}

Proj::Vec3 cross(Proj::Vec3 first, Proj::Vec3 second) noexcept
{
    return {first.y * second.z - first.z * second.y,
            first.z * second.x - first.x * second.z,
            first.x * second.y - first.y * second.x};
}

Proj::Quat normalized(Proj::Quat value) noexcept
{
    const float magnitude = std::sqrt(
        value.x * value.x + value.y * value.y +
        value.z * value.z + value.w * value.w);
    if (magnitude <= 0.000001F)
        return {};
    return {value.x / magnitude, value.y / magnitude,
            value.z / magnitude, value.w / magnitude};
}

Proj::Quat multiply(Proj::Quat first, Proj::Quat second) noexcept
{
    return {first.w * second.x + first.x * second.w +
                first.y * second.z - first.z * second.y,
            first.w * second.y - first.x * second.z +
                first.y * second.w + first.z * second.x,
            first.w * second.z + first.x * second.y -
                first.y * second.x + first.z * second.w,
            first.w * second.w - first.x * second.x -
                first.y * second.y - first.z * second.z};
}

Proj::Vec3 rotate(Proj::Quat rotation, Proj::Vec3 value) noexcept
{
    rotation = normalized(rotation);
    const Proj::Vec3 quaternionVector{
        rotation.x, rotation.y, rotation.z};
    auto twiceCross = cross(quaternionVector, value);
    twiceCross = {
        twiceCross.x * 2.0F,
        twiceCross.y * 2.0F,
        twiceCross.z * 2.0F};
    const auto secondCross = cross(quaternionVector, twiceCross);
    return {value.x + rotation.w * twiceCross.x + secondCross.x,
            value.y + rotation.w * twiceCross.y + secondCross.y,
            value.z + rotation.w * twiceCross.z + secondCross.z};
}

Proj::Quat shortestArcFromX(Proj::Vec3 direction) noexcept
{
    direction = normalized(direction);
    const float cosine = std::clamp(direction.x, -1.0F, 1.0F);
    if (cosine < -0.999999F)
        return {0.0F, 0.0F, 1.0F, 0.0F};
    const float scale = std::sqrt((1.0F + cosine) * 2.0F);
    const float inverseScale =
        scale > 0.000001F ? 1.0F / scale : 0.0F;
    return normalized(Proj::Quat{
        0.0F, -direction.z * inverseScale,
        direction.y * inverseScale, scale * 0.5F});
}

Proj::Quat slerp(
    Proj::Quat first, Proj::Quat second, float alpha) noexcept
{
    first = normalized(first);
    second = normalized(second);
    float cosine = first.x * second.x + first.y * second.y +
                   first.z * second.z + first.w * second.w;
    if (cosine < 0.0F)
    {
        second = {-second.x, -second.y, -second.z, -second.w};
        cosine = -cosine;
    }
    cosine = std::clamp(cosine, -1.0F, 1.0F);
    if (cosine > 0.9995F)
    {
        return normalized(Proj::Quat{
            first.x + (second.x - first.x) * alpha,
            first.y + (second.y - first.y) * alpha,
            first.z + (second.z - first.z) * alpha,
            first.w + (second.w - first.w) * alpha});
    }
    const float angle = std::acos(cosine);
    const float sine = std::sin(angle);
    const float firstWeight =
        std::sin((1.0F - alpha) * angle) / sine;
    const float secondWeight = std::sin(alpha * angle) / sine;
    return normalized(Proj::Quat{
        first.x * firstWeight + second.x * secondWeight,
        first.y * firstWeight + second.y * secondWeight,
        first.z * firstWeight + second.z * secondWeight,
        first.w * firstWeight + second.w * secondWeight});
}

} // namespace

Proj::Proj()
{
    ResetGameObject(-1.0F);
}

void Proj::ConfigureDeathEffect(
    bool effectPhysicsIgnoreSenderCar,
    bool targetChild) noexcept
{
    GetBehaviors().Clear();
    ResetGameObject(-1.0F);
    deathEffect_ = &GetBehaviors().Add<DeathEffectBehavior>(
        BehaviorType::DeathEffect,
        effectPhysicsIgnoreSenderCar, targetChild);
}

DeathEffect::SpawnResult Proj::DestroyWithEffect(
    GameObject* target, bool logicAvailable,
    bool senderIsWeaponProjectile) noexcept
{
    if (deathEffect_ == nullptr)
    {
        Death(DamageType::Simple, target);
        return {};
    }
    deathEffect_->SetSpawnContext(
        logicAvailable, senderIsWeaponProjectile);
    Death(DamageType::Simple, target);
    return deathEffect_->ConsumeSpawnResult();
}

DeathEffectBehavior* Proj::GetDeathEffectBehavior() noexcept
{
    return deathEffect_;
}

const DeathEffectBehavior* Proj::GetDeathEffectBehavior() const noexcept
{
    return deathEffect_;
}

Proj::ContactResult Proj::SpeedArrowContact(
    Vec3 worldDirection, float damage) noexcept
{
    const auto direction = normalized(worldDirection);
    return {{direction.x * damage, direction.y * damage,
             direction.z * damage},
            0.0F, true, false, true};
}

Proj::ContactResult Proj::LushaContact(
    Vec3 linearVelocity, float damage) noexcept
{
    ContactResult result;
    const float speed = length(linearVelocity);
    if (speed > 1.0F && speed > damage)
    {
        const auto direction = normalized(linearVelocity);
        result.linearVelocity = {
            direction.x * damage, direction.y * damage,
            direction.z * damage};
        result.setLinearVelocity = true;
    }
    return result;
}

Proj::ContactResult Proj::MasloContact(
    Vec3 carPosition, Vec3 carWorldRight, Vec3 oilPosition,
    Vec3 linearVelocity, float damage, bool arming,
    bool mineLocked, bool clutchLocked,
    bool clutchImmune) noexcept
{
    ContactResult result;
    if (arming || mineLocked || clutchLocked || clutchImmune ||
        length(linearVelocity) <= 3.0F)
    {
        return result;
    }
    const Vec3 offset{
        oilPosition.x - carPosition.x,
        oilPosition.y - carPosition.y,
        oilPosition.z - carPosition.z};
    const float distance =
        carWorldRight.x * offset.x +
        carWorldRight.y * offset.y +
        carWorldRight.z * offset.z;
    result.clutchStrength =
        std::abs(distance) > 0.1F && distance > 0.0F
            ? -damage
            : damage;
    result.lockClutch = true;
    return result;
}

Proj::RocketUpdateResult Proj::RocketUpdate(
    float projectileZ, float trackZ, float boxHalfExtentZ,
    float clearance, bool trackHit) noexcept
{
    RocketUpdateResult result{projectileZ, clearance};
    if (!trackHit)
        return result;

    const float height = std::max(
        projectileZ - trackZ, boxHalfExtentZ);
    if (result.clearance == 0.0F ||
        result.clearance - height > 0.1F)
    {
        result.clearance = height;
    }
    result.positionZ = trackZ + result.clearance;
    return result;
}

Proj::TorpedaUpdateResult Proj::TorpedaUpdate(
    float deltaTime, Vec3 position, Quat rotation,
    Vec3 storedVelocity, float homingDelay, bool hasTarget,
    Vec3 targetPosition, float sourceSpeed, bool speedRelative,
    float angleSpeed) noexcept
{
    TorpedaUpdateResult result;
    result.rotation = rotation;
    result.direction = normalized(rotate(rotation, {1.0F, 0.0F, 0.0F}));
    result.linearVelocity = storedVelocity;
    result.homingDelay = std::max(homingDelay - deltaTime, 0.0F);
    if (!hasTarget || result.homingDelay != 0.0F)
        return result;

    Vec3 direction{
        targetPosition.x - position.x,
        targetPosition.y - position.y,
        targetPosition.z - position.z};
    const float distance = length(direction);
    direction = distance > 1.0F
                    ? normalized(direction)
                    : result.direction;

    const Quat targetRotation = shortestArcFromX(direction);
    result.rotation = angleSpeed > 0.0F
                          ? slerp(rotation, targetRotation,
                                  deltaTime * angleSpeed)
                          : targetRotation;
    result.direction = normalized(
        rotate(result.rotation, {1.0F, 0.0F, 0.0F}));
    const float speed = speedRelative
                            ? length(storedVelocity)
                            : std::max(
                                  dot(storedVelocity, result.direction),
                                  sourceSpeed);
    const float actorSpeed = std::max(sourceSpeed, speed);
    result.linearVelocity = {
        result.direction.x * actorSpeed,
        result.direction.y * actorSpeed,
        result.direction.z * actorSpeed};
    result.setLinearVelocity = true;
    return result;
}

float Proj::ThunderUpdate(
    float reflectionCooldown, float deltaTime) noexcept
{
    return reflectionCooldown - deltaTime;
}

Proj::ThunderContactResult Proj::ThunderContact(
    Vec3 linearVelocity, Vec3 contactNormal,
    float reflectionCooldown,
    bool shotTransparencyContact) noexcept
{
    ThunderContactResult result{
        linearVelocity, reflectionCooldown, false};
    if (reflectionCooldown > 0.0F ||
        !shotTransparencyContact || length(linearVelocity) <= 5.0F)
    {
        return result;
    }

    result.reflectionCooldown = 0.1F;
    contactNormal = normalized(contactNormal);
    const auto velocityNormal = normalized(linearVelocity);
    if (std::abs(dot(velocityNormal, contactNormal)) > 0.1F)
    {
        const float projection = dot(linearVelocity, contactNormal);
        result.linearVelocity = {
            linearVelocity.x - 2.0F * projection * contactNormal.x,
            linearVelocity.y - 2.0F * projection * contactNormal.y,
            linearVelocity.z - 2.0F * projection * contactNormal.z};
    }
    else
    {
        result.linearVelocity = {
            -linearVelocity.x, -linearVelocity.y, -linearVelocity.z};
    }
    result.setLinearVelocity = true;
    return result;
}

Proj::Quat Proj::ResonanseUpdate(
    Quat rotation, float angleSpeed, float deltaTime) noexcept
{
    const float halfAngle = angleSpeed * deltaTime * 0.5F;
    return normalized(multiply(
        rotation,
        {std::sin(halfAngle), 0.0F, 0.0F, std::cos(halfAngle)}));
}

Proj::TorqueResult Proj::RocketContactTorque(
    Vec3 contactPoint, Vec3 linearVelocity, float mass) noexcept
{
    TorqueResult result;
    if (length(linearVelocity) <= 1.0F)
        return result;
    const Vec3 direction = normalized(linearVelocity);
    Vec3 contactDirection = cross(contactPoint, direction);
    if (length(contactDirection) <= 0.01F)
        return result;
    contactDirection = normalized(contactDirection);
    result.localVelocityChange = {
        contactDirection.x * mass * 0.2F,
        contactDirection.y * mass * 0.2F,
        contactDirection.z * mass * 0.2F};
    result.apply = true;
    return result;
}

Proj::LaunchResult Proj::CalcSpeed(
    Vec3 worldDirection, Vec3 weaponVelocity, float sourceSpeed,
    float speedRelativeMinimum, bool speedRelative) noexcept
{
    LaunchResult result;
    result.direction = normalized(worldDirection);
    result.speed = sourceSpeed;
    const float forwardSpeed =
        std::max(dot(result.direction, weaponVelocity), 0.0F);
    if (speedRelative)
        result.speed += forwardSpeed;
    else if (speedRelativeMinimum > 0.0F)
    {
        result.speed = std::max(
            result.speed, speedRelativeMinimum + forwardSpeed);
    }

    if (std::abs(result.direction.z) < 0.707F)
    {
        result.direction.z = 0.0F;
        result.direction = normalized(result.direction);
    }
    result.linearVelocity = {
        result.direction.x * result.speed,
        result.direction.y * result.speed,
        result.direction.z * result.speed};
    return result;
}

Proj::MineUpdateResult Proj::MineUpdate(
    float timer, float deltaTime, float delay) noexcept
{
    MineUpdateResult result;
    result.timer = timer;
    if (timer < 0.0F)
        return result;

    result.timer += deltaTime;
    result.visualScale = std::clamp(
        delay > 0.0F ? result.timer / delay : 1.0F,
        0.0F, 1.0F);
    if (result.visualScale == 1.0F)
        result.timer = -1.0F;
    result.armed = result.timer == -1.0F;
    return result;
}

bool Proj::MineContactAllowed(
    bool hasTarget, bool testMineLock, bool mineBugEnabled,
    bool targetMineLocked, float armingTimer,
    bool targetIsOwner) noexcept
{
    if (!hasTarget ||
        (testMineLock && targetMineLocked && mineBugEnabled))
    {
        return false;
    }
    return armingTimer == -1.0F || !targetIsOwner;
}

bool Proj::MineRipUpdate(
    float timeLife, float splitTime, bool death) noexcept
{
    return timeLife > splitTime && !death;
}

Proj::ImpulseContactResult Proj::ImpulseContact(
    bool hasContactActor, bool hasTarget, bool contactIsTarget,
    std::uint32_t hitCount, float damage) noexcept
{
    ImpulseContactResult result;
    result.hitCount = hitCount;
    if (!hasContactActor)
        return result;
    if (hasTarget && contactIsTarget)
    {
        result.damage = damage /
                        static_cast<float>(hitCount + 1U);
        result.applyDamage = true;
        result.hitCount = hitCount + 1U;
        if (result.hitCount > 2U)
            result.destroy = true;
        else
            result.findNextTarget = true;
        return result;
    }
    if (!hasTarget)
    {
        result.damage = damage;
        result.applyDamage = true;
        result.destroy = true;
    }
    return result;
}

float Proj::PrepareMaximumLife(
    float speed, float maximumDistance,
    float sampledMinimumLife) noexcept
{
    const float travelLife =
        speed > 0.0F ? maximumDistance / speed : 0.0F;
    return std::max(travelLife, sampledMinimumLife);
}

Proj::LaserUpdateResult Proj::LaserUpdate(
    float maximumDistance, bool hit, float hitDistance,
    float deltaTime, float damage, bool distort,
    float timeLife, float maximumTimeLife) noexcept
{
    LaserUpdateResult result;
    result.distance = maximumDistance;
    if (hit)
    {
        result.distance = std::min(hitDistance, maximumDistance);
        if (result.distance < maximumDistance)
        {
            result.damage = deltaTime * damage;
            result.applyDamage = true;
        }
    }
    result.textureScale = result.distance / 10.0F;
    if (distort)
    {
        const float lifeAlpha = std::clamp(
            timeLife / maximumTimeLife, 0.0F, 1.0F);
        const float fadeIn = std::clamp(
            lifeAlpha / 0.5F * 1.5F + 0.5F, 0.0F, 2.0F);
        const float fadeOut = std::clamp(
            (lifeAlpha - 0.6F) / 0.4F * 2.0F,
            0.0F, 2.0F);
        result.beamWidthScale = fadeIn - fadeOut;
    }
    return result;
}

Proj::ContinuousContactResult Proj::FireContact(
    bool hasTarget, float damage, float deltaTime) noexcept
{
    ContinuousContactResult result;
    if (hasTarget)
        result.damage = damage * deltaTime;
    return result;
}

Proj::ContinuousContactResult Proj::DrobilkaContact(
    bool hasTarget, float damage, float deltaTime) noexcept
{
    return FireContact(hasTarget, damage, deltaTime);
}

Proj::ContinuousContactResult Proj::SonarContact(
    bool hasTarget, Vec3 linearVelocity, float mass,
    float damage, float deltaTime) noexcept
{
    auto result = FireContact(hasTarget, damage, deltaTime);
    if (!hasTarget)
        return result;
    result.impulse = {
        linearVelocity.x * mass,
        linearVelocity.y * mass,
        linearVelocity.z * mass};
    result.applyImpulse = true;
    return result;
}

Proj::SpringPrepareResult Proj::SpringPrepare(
    bool hasCar, bool wheelsContact, float speed) noexcept
{
    SpringPrepareResult result;
    if (!hasCar || !wheelsContact)
        return result;
    result.localVelocityChange = {0.0F, 0.0F, speed};
    result.prepared = true;
    result.lockSpring = true;
    return result;
}

Proj::TypeRules Proj::GetTypeRules(std::uint32_t type) noexcept
{
    TypeRules result;
    switch (type)
    {
    case 0U:  // ptRocket
        result.rocketPrepare = true;
        break;
    case 1U:  // ptHyper
        result.attached = true;
        result.linkedToWeapon = true;
        break;
    case 2U:  // ptTorpeda
        result.rocketPrepare = true;
        result.homing = true;
        break;
    case 3U:  // ptLaser
        result.attached = true;
        result.linkedToWeapon = true;
        result.ray = true;
        break;
    case 11U: // ptMine
    case 12U: // ptMineRip
        result.mineTestsLock = true;
        break;
    case 14U: // ptFire
        result.rocketPrepare = true;
        result.attached = true;
        break;
    case 15U: // ptDrobilka
        result.attached = true;
        break;
    case 16U: // ptSonar
        result.rocketPrepare = true;
        break;
    case 17U: // ptSpring
        result.attached = true;
        result.linkedToWeapon = true;
        break;
    case 18U: // ptFrostRay
        result.attached = true;
        result.linkedToWeapon = true;
        result.ray = true;
        break;
    case 19U: // ptMortira
        result.rocketPrepare = true;
        result.ballistic = true;
        break;
    case 21U: // ptImpulse
        result.rocketPrepare = true;
        result.homing = true;
        break;
    case 22U: // ptThunder
    case 23U: // ptResonanse
        result.rocketPrepare = true;
        break;
    default:
        break;
    }
    return result;
}

Proj::DestroyResult Proj::OnDestroy(
    bool senderIsWeapon, bool parentIsWeapon,
    bool senderIsTarget) noexcept
{
    DestroyResult result;
    if (senderIsWeapon)
    {
        result.destroy = parentIsWeapon;
        result.clearWeapon = true;
    }
    result.clearTarget = senderIsTarget;
    return result;
}

Proj::BonusContactResult Proj::BonusContact(
    std::uint32_t type, bool hasTarget, float damage,
    float targetMaximumLife) noexcept
{
    BonusContactResult result;
    if (!hasTarget)
        return result;
    switch (type)
    {
    case 4U: // ptMedpack
        result.type = BonusContactType::Medpack;
        result.value = damage > 0.0F ? damage : targetMaximumLife;
        break;
    case 5U: // ptCharge
        result.type = BonusContactType::Charge;
        result.value = damage;
        break;
    case 6U: // ptMoney
        result.type = BonusContactType::Money;
        result.value = damage;
        break;
    case 7U: // ptImmortal
        result.type = BonusContactType::Immortal;
        result.value = damage;
        break;
    default:
        return result;
    }
    result.take = true;
    return result;
}

bool AutoProj::UsesMineUpdate(std::uint32_t type) noexcept
{
    switch (type)
    {
    case 10U: // ptMaslo
    case 11U: // ptMine
    case 12U: // ptMineRip
    case 20U: // ptCrater
    case 24U: // ptMineProton
        return true;
    default:
        return false;
    }
}

void AutoProj::Reset(std::uint32_t type) noexcept
{
    LogicReleased();
    type_ = type;
    armingTimer_ = -1.0F;
    modelScale_ = -1.0F;
    if (GetLogic() != nullptr)
        LogicInited();
}

AutoProj::~AutoProj()
{
    LogicReleased();
}

void AutoProj::LogicInited() noexcept
{
    // AutoProj::InitProj is idempotent and does nothing until GetLogic()
    // succeeds.  PrepareProj(NULL, ctx) preserves the map object's position
    // and rotation; those transforms remain owned by BonusInstance here.
    if (prepared_ || GetLogic() == nullptr)
        return;
    prepared_ = true;
    if (UsesMineUpdate(type_))
        armingTimer_ = 0.0F;
    if (type_ == masloType)
        modelScale_ = 0.0F;
}

void AutoProj::LogicReleased() noexcept
{
    prepared_ = false;
}

void AutoProj::OnProgress(float deltaTime) noexcept
{
    if (!prepared_ || !UsesMineUpdate(type_))
        return;
    const auto result = Proj::MineUpdate(
        armingTimer_, deltaTime);
    armingTimer_ = result.timer;
    if (type_ == masloType && result.visualScale >= 0.0F)
        modelScale_ = result.visualScale;
}

bool AutoProj::IsPrepared() const noexcept
{
    return prepared_;
}

bool AutoProj::IsArming() const noexcept
{
    return prepared_ && armingTimer_ >= 0.0F;
}

float AutoProj::GetModelScale() const noexcept
{
    return modelScale_;
}

std::uint32_t AutoProj::GetType() const noexcept
{
    return type_;
}

void ShotEffect::Reset() noexcept
{
    shotCount_ = 0U;
}

void ShotEffect::OnShot() noexcept
{
    ++shotCount_;
}

std::uint64_t ShotEffect::GetShotCount() const noexcept
{
    return shotCount_;
}

ShotEffectBehavior::ShotEffectBehavior(Behaviors* owner) noexcept
    : Behavior(owner)
{
}

void ShotEffectBehavior::OnProgress(float) noexcept {}

void ShotEffectBehavior::Reset() noexcept
{
    state_.Reset();
    lastShotPosition_ = {};
}

const ShotEffect& ShotEffectBehavior::GetState() const noexcept
{
    return state_;
}

const std::array<float, 3U>&
ShotEffectBehavior::GetLastShotPosition() const noexcept
{
    return lastShotPosition_;
}

void ShotEffectBehavior::CopyStateFrom(
    const ShotEffectBehavior& value) noexcept
{
    state_ = value.state_;
    lastShotPosition_ = value.lastShotPosition_;
}

void ShotEffectBehavior::OnShot(
    const std::array<float, 3U>& position) noexcept
{
    lastShotPosition_ = position;
    state_.OnShot();
}

Weapon::Weapon() : desc_(std::make_shared<Desc>())
{
    ResetGameObject(-1.0F);
    BindSourceBehaviors();
}

Weapon::Weapon(const Desc& desc)
    : desc_(std::make_shared<Desc>(desc))
{
    ResetGameObject(-1.0F);
    BindSourceBehaviors();
}

Weapon::Weapon(const Weapon& other)
    : GameObject(other), desc_(other.desc_), shotTime_(other.shotTime_)
{
    BindSourceBehaviors();
    if (other.shotEffect_ != nullptr)
        shotEffect_->CopyStateFrom(*other.shotEffect_);
}

Weapon& Weapon::operator=(const Weapon& other)
{
    if (this == &other)
        return *this;
    GameObject::operator=(other);
    desc_ = other.desc_;
    shotTime_ = other.shotTime_;
    BindSourceBehaviors();
    if (other.shotEffect_ != nullptr)
        shotEffect_->CopyStateFrom(*other.shotEffect_);
    return *this;
}

Weapon::Weapon(Weapon&& other)
    : Weapon(static_cast<const Weapon&>(other))
{
}

Weapon& Weapon::operator=(Weapon&& other)
{
    return *this = static_cast<const Weapon&>(other);
}

void Weapon::BindSourceBehaviors()
{
    GetBehaviors().Clear();
    shotEffect_ = &GetBehaviors().Add<ShotEffectBehavior>(
        BehaviorType::ShotEffect);
}

const ProjectileDefinition& Weapon::Desc::Front() const noexcept
{
    static const ProjectileDefinition empty;
    return projectiles.empty() ? empty : projectiles.front();
}

void Weapon::Reset() noexcept
{
    shotTime_ = 0.0F;
    ResetGameObject(-1.0F);
    if (shotEffect_ != nullptr)
        shotEffect_->Reset();
}

void Weapon::OnProgress(float deltaTime) noexcept
{
    GameObject::OnProgress(deltaTime);
    shotTime_ += deltaTime;
}

float Weapon::GetShotTime() const noexcept
{
    return shotTime_;
}

bool Weapon::IsReadyShot(float delay) const noexcept
{
    // Weapon.cpp uses a strict comparison, which matters on the first frame
    // and for analog mine bindings with a zero threshold.
    return shotTime_ > delay;
}

bool Weapon::IsReadyShot() const noexcept
{
    return IsReadyShot(desc_->shotDelay);
}

bool Weapon::IsMaslo() const noexcept
{
    return !desc_->projectiles.empty() &&
           desc_->Front().type == masloProjectileType;
}

void Weapon::OnShot(bool projectileCreated) noexcept
{
    // Weapon::CreateShot resets _shotTime only after PrepareProj succeeds.
    if (projectileCreated)
        shotTime_ = 0.0F;
}

void Weapon::OnProjectilePrepared(
    const std::array<float, 3U>& position) noexcept
{
    // Behaviors::OnShot is inside Weapon::CreateShot's projectile loop in
    // the Windows source, after each successful PrepareProj call.
    GetBehaviors().OnShot(position);
}

const Weapon::Desc& Weapon::GetDesc() const noexcept
{
    return *desc_;
}

Weapon::DescHandle Weapon::GetDescHandle() const noexcept
{
    return desc_;
}

void Weapon::SetDesc(const Desc& value)
{
    desc_ = std::make_shared<Desc>(value);
}

void Weapon::SetDescHandle(DescHandle value) noexcept
{
    if (value != nullptr)
        desc_ = std::move(value);
}

void Weapon::SetDesc(
    float shotDelay,
    std::span<const std::uint32_t> projectileTypes)
{
    Desc description;
    description.shotDelay = shotDelay;
    description.projectiles.reserve(projectileTypes.size());
    for (const auto type : projectileTypes)
    {
        ProjectileDefinition projectile;
        projectile.type = type;
        description.projectiles.push_back(std::move(projectile));
    }
    SetDesc(description);
}

void Weapon::SetDesc(
    float shotDelay,
    std::span<const ProjectileDefinition> projectiles)
{
    Desc description;
    description.shotDelay = shotDelay;
    description.projectiles.assign(
        projectiles.begin(), projectiles.end());
    SetDesc(description);
}

const ShotEffect& Weapon::GetShotEffect() const noexcept
{
    static const ShotEffect empty;
    return shotEffect_ != nullptr ? shotEffect_->GetState() : empty;
}

const std::array<float, 3U>& Weapon::GetLastShotPosition() const noexcept
{
    static const std::array<float, 3U> empty{};
    return shotEffect_ != nullptr
        ? shotEffect_->GetLastShotPosition()
        : empty;
}

WeaponItem::WeaponItem(SlotType type) noexcept : SlotItem(type) {}

WeaponItem::WeaponItem(
    Weapon* weapon, std::uint32_t maximumCharge,
    std::uint32_t countCharge, std::uint32_t* currentCharge,
    std::uint32_t chargeStep, float damage, int chargeCost) noexcept
    : SlotItem(SlotType::Weapon)
{
    Bind(weapon, maximumCharge, countCharge, currentCharge,
         chargeStep, damage, chargeCost);
}

WeaponItem* WeaponItem::IsWeaponItem() noexcept { return this; }

const WeaponItem* WeaponItem::IsWeaponItem() const noexcept
{
    return this;
}

void WeaponItem::OnCreateCar() noexcept
{
    carAttached_ = true;
    if (weapon_ != nullptr)
        weapon_->SetDescHandle(weaponDesc_);
}

void WeaponItem::OnDestroyCar() noexcept
{
    carAttached_ = false;
}

void WeaponItem::Bind(
    Weapon* weapon, std::uint32_t maximumCharge,
    std::uint32_t countCharge, std::uint32_t* currentCharge,
    std::uint32_t chargeStep, float damage, int chargeCost) noexcept
{
    weapon_ = weapon;
    maximumCharge_ = maximumCharge;
    countCharge_ = countCharge;
    currentCharge_ = currentCharge != nullptr ? *currentCharge : 0U;
    chargeStep_ = chargeStep;
    damage_ = damage;
    chargeCost_ = chargeCost;
    weaponDesc_ = weapon != nullptr
                      ? weapon->GetDescHandle()
                      : std::make_shared<Weapon::Desc>();
}

bool WeaponItem::Shot(bool projectileCreated, int newCharge) noexcept
{
    bool result = false;
    if (currentCharge_ > 0U || maximumCharge_ == 0U)
    {
        result = carAttached_ && weapon_ != nullptr && projectileCreated;
        if (newCharge == -1)
        {
            newCharge = result
                            ? static_cast<int>(currentCharge_) - 1
                            : static_cast<int>(currentCharge_);
        }
    }

    // Player.cpp applies this even when the charge gate or projectile
    // preparation failed. That detail is required by NetPlayer::DoShot.
    currentCharge_ = static_cast<std::uint32_t>(
        std::max(newCharge, 0));
    if (weapon_ != nullptr)
        weapon_->OnShot(result);
    return result;
}

void WeaponItem::Reload() noexcept
{
    currentCharge_ = countCharge_;
}

bool WeaponItem::IsReadyShot(float delay) const noexcept
{
    return carAttached_ && weapon_ != nullptr &&
           weapon_->IsReadyShot(delay);
}

bool WeaponItem::IsReadyShot() const noexcept
{
    return carAttached_ && weapon_ != nullptr &&
           weapon_->IsReadyShot();
}

bool WeaponItem::IsInstalled() const noexcept
{
    return carAttached_ && weapon_ != nullptr;
}

bool WeaponItem::HasShotCharge() const noexcept
{
    return currentCharge_ > 0U || maximumCharge_ == 0U;
}

std::uint32_t WeaponItem::GetMaxCharge() const noexcept
{
    return maximumCharge_;
}

void WeaponItem::SetMaxCharge(std::uint32_t value) noexcept
{
    maximumCharge_ = value;
}

std::uint32_t WeaponItem::GetCntCharge() const noexcept
{
    return countCharge_;
}

void WeaponItem::SetCntCharge(std::uint32_t value) noexcept
{
    countCharge_ = value;
}

std::uint32_t WeaponItem::GetCurCharge() const noexcept
{
    return currentCharge_;
}

void WeaponItem::SetCurCharge(std::uint32_t value) noexcept
{
    currentCharge_ = value;
}

std::uint32_t WeaponItem::GetChargeStep() const noexcept
{
    return chargeStep_;
}

void WeaponItem::SetChargeStep(std::uint32_t value) noexcept
{
    chargeStep_ = value;
}

float WeaponItem::GetDamage(bool statisticsDamage) const noexcept
{
    (void)statisticsDamage;
    float damage = 0.0F;
    for (const auto& projectile : weaponDesc_->projectiles)
        damage += projectile.damage;
    return damage;
}

void WeaponItem::SetDamage(float value) noexcept
{
    // Player.cpp labels the serialized field invalid. GetDamage sums the
    // projectile descriptors, exactly as the Windows implementation does.
    damage_ = value;
}

int WeaponItem::GetChargeCost() const noexcept
{
    return chargeCost_;
}

void WeaponItem::SetChargeCost(int value) noexcept
{
    chargeCost_ = value;
}

const Weapon::Desc& WeaponItem::GetWpnDesc() const noexcept
{
    return *weaponDesc_;
}

void WeaponItem::SetWpnDesc(const Weapon::Desc& value)
{
    weaponDesc_ = std::make_shared<Weapon::Desc>(value);
    if (carAttached_ && weapon_ != nullptr)
        weapon_->SetDescHandle(weaponDesc_);
}

Weapon* WeaponItem::GetWeapon() const noexcept
{
    return carAttached_ ? weapon_ : nullptr;
}

Weapon::Desc WeaponItem::GetDesc() const
{
    const auto* weapon = GetWeapon();
    return weapon != nullptr ? weapon->GetDesc() : *weaponDesc_;
}

DroidItem::DroidItem() noexcept : WeaponItem(SlotType::Droid) {}

DroidItem::DroidItem(
    Weapon* weapon, std::uint32_t maximumCharge,
    std::uint32_t countCharge, std::uint32_t* currentCharge,
    float repairValue, float repairPeriod) noexcept
    : WeaponItem(SlotType::Droid)
{
    Bind(weapon, maximumCharge, countCharge, currentCharge,
         repairValue, repairPeriod);
}

void DroidItem::Bind(
    Weapon* weapon, std::uint32_t maximumCharge,
    std::uint32_t countCharge, std::uint32_t* currentCharge,
    float repairValue, float repairPeriod) noexcept
{
    WeaponItem::Bind(
        weapon, maximumCharge, countCharge, currentCharge);
    repairValue_ = repairValue;
    repairPeriod_ = repairPeriod;
    time_ = 0.0F;
    progressRegistered_ = false;
}

void DroidItem::OnCreateCar() noexcept
{
    WeaponItem::OnCreateCar();
    time_ = 0.0F;
    progressRegistered_ = true;
}

void DroidItem::OnDestroyCar() noexcept
{
    // The source unregisters the progress event here. _time is reset by the
    // next OnCreateCar, not by OnDestroyCar itself.
    progressRegistered_ = false;
    WeaponItem::OnDestroyCar();
}

float DroidItem::OnProgress(
    float deltaTime, float& life, float maximumLife, bool death) noexcept
{
    if (!progressRegistered_)
        return 0.0F;
    if (life >= maximumLife || death)
    {
        time_ = 0.0F;
        return 0.0F;
    }
    if ((time_ += deltaTime) > repairPeriod_)
    {
        time_ -= repairPeriod_;
        const float previousLife = life;
        // Player.cpp intentionally uses a literal rather than _repairValue.
        life = std::min(maximumLife, life + 5.0F);
        return life - previousLife;
    }
    return 0.0F;
}

float DroidItem::GetRepairValue() const noexcept
{
    return repairValue_;
}

void DroidItem::SetRepairValue(float value) noexcept
{
    repairValue_ = value;
}

float DroidItem::GetRepairPeriod() const noexcept
{
    return repairPeriod_;
}

void DroidItem::SetRepairPeriod(float value) noexcept
{
    repairPeriod_ = value;
}

float DroidItem::GetRepairTime() const noexcept
{
    return time_;
}

bool DroidItem::IsProgressRegistered() const noexcept
{
    return progressRegistered_;
}

ReflectorItem::ReflectorItem() noexcept
    : WeaponItem(SlotType::Reflector)
{
}

ReflectorItem::ReflectorItem(
    Weapon* weapon, std::uint32_t maximumCharge,
    std::uint32_t countCharge, std::uint32_t* currentCharge,
    float reflectValue) noexcept
    : WeaponItem(SlotType::Reflector)
{
    Bind(weapon, maximumCharge, countCharge, currentCharge,
         reflectValue);
}

void ReflectorItem::Bind(
    Weapon* weapon, std::uint32_t maximumCharge,
    std::uint32_t countCharge, std::uint32_t* currentCharge,
    float reflectValue) noexcept
{
    WeaponItem::Bind(
        weapon, maximumCharge, countCharge, currentCharge);
    reflectValue_ = reflectValue;
}

float ReflectorItem::GetReflectValue() const noexcept
{
    return reflectValue_;
}

void ReflectorItem::SetReflectValue(float value) noexcept
{
    reflectValue_ = value;
}

float ReflectorItem::Reflect(float damage) const noexcept
{
    return damage * std::clamp(1.0F - reflectValue_, 0.0F, 1.0F);
}

void WeaponRack::Reset() noexcept
{
    for (auto& weapon : primary)
        weapon.Reset();
    hyper.Reset();
    mine.Reset();
}

void WeaponRack::OnProgress(float deltaTime) noexcept
{
    for (auto& weapon : primary)
        weapon.OnProgress(deltaTime);
    hyper.OnProgress(deltaTime);
    mine.OnProgress(deltaTime);
}

} // namespace r3d::game::originalrace::source
