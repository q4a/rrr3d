#include "OriginalWeapon.h"

#include "OriginalLogic.h"
#include "OriginalMapObj.h"
#include "OriginalPlayer.h"

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

Proj::Quat inverse(Proj::Quat value) noexcept
{
    value = normalized(value);
    return {-value.x, -value.y, -value.z, value.w};
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

Proj::~Proj()
{
    FreeSourceModel(true, false);
    FreeSourceModel(false, false);
    SetSourceTarget(nullptr);
    SetSourceWeapon(nullptr);
}

void Proj::PrepareSource(
    const ProjectileDefinition& description,
    GameObject* weapon, const ShotContext& context) noexcept
{
    FreeSourceModel(true, true);
    FreeSourceModel(false, true);
    SetSourceTarget(nullptr);
    SetSourceWeapon(nullptr);
    description_ = description;
    playerId_ = context.playerId;
    ResetSourceRuntimeState();
    externalLifetimeManaged_ = true;
    ResetGameObject(-1.0F);
    SetMaxTimeLife(context.maximumLife);
    SetTimeLife(0.0F);
    SetSourceWeapon(weapon, false);
    SetShot(context.shot);
    const auto route = PreparationRouteFor(description.type);
    if (!route.valid || !context.preparationAccepted)
    {
        prepared_ = false;
        return;
    }
    if (route.linkedToWeapon)
        LinkToSourceWeapon(context.position, context.rotation);
    else
        SyncSourceTransform(context.position, context.rotation);
    // Every successful PrepareProj path calls InitModel except Spring and
    // Drobilka. Drobilka creates its model lazily on the first contact.
    if (route.initializeModel)
        InitSourceModel(false);
    if (route.initializeSecondaryModel)
        InitSourceModel(true);
    ApplySourcePreparationState(context);
    prepared_ = true;
}

void Proj::ApplySourcePreparationState(
    const ShotContext& context) noexcept
{
    const auto route = PreparationRouteFor(description_.type);
    // RocketPrepare, LaserPrepare and DrobilkaPrepare all disable collision
    // against their own weapon actor. This flag belongs to concrete Proj,
    // not to a particular session spawn path.
    ignoreContactProj_ = route.ignoreWeaponContact;

    if (route.homing)
    {
        // TorpedaPrepare initializes _time1 and asks RocketPrepare to retain
        // the initial actor velocity in _vec1. ImpulsePrepare shares it.
        sourceTimer_ = 0.4F;
        sourceVector_ = context.launchVelocity;
    }

    if (route.handler == PrepareHandler::Maslo &&
        sourceModel_ != nullptr)
    {
        sourceModel_->GetGameObj().SetScale(
            {0.0F, 0.0F, 0.0F});
    }
    if (route.minePlacement)
    {
        // MinePrepare changes its pre-placement -1 sentinel to 0 once the
        // surface actor exists; MineUpdate then performs the arming fade.
        sourceTimer_ = 0.0F;
    }
    else if (route.handler == PrepareHandler::MinePiece)
    {
        sourceTimer_ = -1.0F;
    }
}

void Proj::SetSourceWeapon(
    GameObject* value, bool linkToWeapon) noexcept
{
    if (weapon_ == value)
    {
        if (weapon_ != nullptr && linkToWeapon && GetParent() != weapon_)
        {
            const auto position = GetWorldPos();
            const auto rotation = GetWorldRot();
            LinkToSourceWeapon(
                {position[0], position[1], position[2]},
                {rotation[0], rotation[1], rotation[2], rotation[3]});
        }
        return;
    }
    if (weapon_ != nullptr)
    {
        weapon_->RemoveListener(this);
        if (GetParent() == weapon_)
            SetParent(nullptr);
    }
    weapon_ = value;
    if (weapon_ == nullptr)
    {
        playerId_ = GameObject::undefinedPlayerId;
        return;
    }
    weapon_->InsertListener(this);
    if (linkToWeapon)
    {
        const auto position = GetWorldPos();
        const auto rotation = GetWorldRot();
        LinkToSourceWeapon(
            {position[0], position[1], position[2]},
            {rotation[0], rotation[1], rotation[2], rotation[3]});
    }
}

void Proj::SetSourceTarget(GameObject* value) noexcept
{
    if (target_ == value)
        return;
    if (target_ != nullptr)
        target_->RemoveListener(this);
    target_ = value;
    if (target_ != nullptr)
        target_->InsertListener(this);
}

void Proj::SetShot(const ShotDesc& value) noexcept
{
    shotTarget_ = value.target;
    SetSourceTarget(value.targetMapObject);
}

Proj::ShotDesc Proj::GetShot() const noexcept
{
    return {target_, shotTarget_};
}

void Proj::SyncSourceTransform(
    const Vec3& position, const Quat& rotation) noexcept
{
    if (weapon_ != nullptr && GetParent() == weapon_)
    {
        LinkToSourceWeapon(position, rotation);
        return;
    }
    SetWorldPos({position.x, position.y, position.z});
    SetWorldRot({rotation.x, rotation.y, rotation.z, rotation.w});
}

void Proj::SyncSourceWeaponTransform(
    const Vec3& position, const Quat& rotation) noexcept
{
    if (weapon_ == nullptr || weapon_ == this)
        return;
    weapon_->SetWorldPos({position.x, position.y, position.z});
    weapon_->SetWorldRot(
        {rotation.x, rotation.y, rotation.z, rotation.w});
}

void Proj::LinkToSourceWeapon(
    const Vec3& worldPosition, const Quat& worldRotation) noexcept
{
    if (weapon_ == nullptr || weapon_ == this)
    {
        SetWorldPos(
            {worldPosition.x, worldPosition.y, worldPosition.z});
        SetWorldRot(
            {worldRotation.x, worldRotation.y,
             worldRotation.z, worldRotation.w});
        return;
    }

    // Weapon.cpp::LocateProj first obtains the projectile world transform.
    // LinkToWeapon then reparents it and restores the serialized local pose.
    // The Jolt adapter gives us the already-composed projectile transform, so
    // recover the corresponding weapon transform before applying that exact
    // local pose. This also keeps the live source Weapon actor synchronized
    // with the renderer/physics mount while the linked projectile is alive.
    const Quat localRotation{
        description_.rotation.x, description_.rotation.y,
        description_.rotation.z, description_.rotation.w};
    const Quat weaponWorldRotation = normalized(multiply(
        worldRotation, inverse(localRotation)));
    const auto weaponWorldScale = weapon_->GetWorldScale();
    const Vec3 scaledOffset{
        description_.position.x * weaponWorldScale[0],
        description_.position.y * weaponWorldScale[1],
        description_.position.z * weaponWorldScale[2]};
    const Vec3 worldOffset = rotate(
        weaponWorldRotation, scaledOffset);

    weapon_->SetWorldRot(
        {weaponWorldRotation.x, weaponWorldRotation.y,
         weaponWorldRotation.z, weaponWorldRotation.w});
    weapon_->SetWorldPos(
        {worldPosition.x - worldOffset.x,
         worldPosition.y - worldOffset.y,
         worldPosition.z - worldOffset.z});
    if (GetParent() != weapon_)
        SetParent(weapon_);
    SetPos(
        {description_.position.x, description_.position.y,
         description_.position.z});
    SetRot(
        {description_.rotation.x, description_.rotation.y,
         description_.rotation.z, description_.rotation.w});
}

void Proj::ResetSourceRuntimeState() noexcept
{
    sourceTick_ = 0U;
    sourceTimer_ = 0.0F;
    sourceState_ = false;
    sourceVector_ = {};
    ignoreContactProj_ = false;
}

float Proj::GetSourceTimer() const noexcept { return sourceTimer_; }
void Proj::SetSourceTimer(float value) noexcept { sourceTimer_ = value; }
const Proj::Vec3& Proj::GetSourceVector() const noexcept
{
    return sourceVector_;
}
void Proj::SetSourceVector(Vec3 value) noexcept
{
    sourceVector_ = value;
}
std::uint32_t Proj::GetSourceTick() const noexcept { return sourceTick_; }
void Proj::SetSourceTick(std::uint32_t value) noexcept
{
    sourceTick_ = value;
}
bool Proj::GetSourceState() const noexcept { return sourceState_; }
void Proj::SetSourceState(bool value) noexcept { sourceState_ = value; }
bool Proj::GetIgnoreContactProj() const noexcept
{
    return ignoreContactProj_;
}
void Proj::SetIgnoreContactProj(bool value) noexcept
{
    ignoreContactProj_ = value;
}

void Proj::SetExternalLifetimeManaged(bool value) noexcept
{
    externalLifetimeManaged_ = value;
}

bool Proj::IsExternalLifetimeManaged() const noexcept
{
    return externalLifetimeManaged_;
}

bool Proj::InitSourceModel(bool secondary)
{
    auto*& model = secondary ? sourceModel2_ : sourceModel_;
    if (model != nullptr)
        return false;
    const auto& definition = secondary
        ? description_.secondaryVisual
        : description_.visual;
    if (definition.record.empty())
        return false;
    model = &GetIncludeList().Add(
        GameObjType::GameObj, definition.record);
    auto& object = model->GetGameObj();
    object.ResetGameObject(definition.maximumLife);
    object.SetMaxTimeLife(definition.maximumTimeLife);
    object.InsertListener(this);
    return true;
}

bool Proj::FreeSourceModel(
    bool secondary, bool remove) noexcept
{
    auto*& model = secondary ? sourceModel2_ : sourceModel_;
    if (model == nullptr)
        return false;
    auto* released = model;
    released->GetGameObj().RemoveListener(this);
    model = nullptr;
    if (remove)
        GetIncludeList().Remove(released);
    return true;
}

MapObj* Proj::GetSourceModel() noexcept { return sourceModel_; }
const MapObj* Proj::GetSourceModel() const noexcept
{
    return sourceModel_;
}
MapObj* Proj::GetSourceModel2() noexcept { return sourceModel2_; }
const MapObj* Proj::GetSourceModel2() const noexcept
{
    return sourceModel2_;
}

const ProjectileDefinition& Proj::GetDesc() const noexcept
{
    return description_;
}

ProjectileCollisionBox Proj::ComputeAABB(bool onlyModel) const noexcept
{
    return ComputeAABB(description_, onlyModel);
}

GameObject* Proj::GetSourceWeapon() const noexcept { return weapon_; }
GameObject* Proj::GetSourceTarget() const noexcept { return target_; }
std::size_t Proj::GetSourcePlayerId() const noexcept { return playerId_; }
bool Proj::IsPrepared() const noexcept { return prepared_; }

void Proj::OnDestroy(GameObject& sender) noexcept
{
    if (sourceModel_ != nullptr &&
        &sourceModel_->GetGameObj() == &sender)
        FreeSourceModel(false, false);
    if (sourceModel2_ != nullptr &&
        &sourceModel2_->GetGameObj() == &sender)
        FreeSourceModel(true, false);
    if (&sender == weapon_)
    {
        const bool linked = GetParent() == weapon_;
        if (linked)
            Death();
        SetSourceWeapon(nullptr);
    }
    if (&sender == target_)
        SetSourceTarget(nullptr);
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

Proj::RocketUpdateResult Proj::ProgressRocket(
    float projectileZ, float trackZ, float boxHalfExtentZ,
    bool trackHit) noexcept
{
    auto result = RocketUpdate(
        projectileZ, trackZ, boxHalfExtentZ,
        sourceVector_.z, trackHit);
    sourceVector_.z = result.clearance;
    return result;
}

Proj::TorpedaUpdateResult Proj::ProgressTorpeda(
    float deltaTime, Vec3 position, Quat rotation,
    bool hasTarget, Vec3 targetPosition,
    float sourceSpeed, bool speedRelative,
    float angleSpeed) noexcept
{
    auto result = TorpedaUpdate(
        deltaTime, position, rotation, sourceVector_, sourceTimer_,
        hasTarget, targetPosition, sourceSpeed, speedRelative,
        angleSpeed);
    sourceTimer_ = result.homingDelay;
    if (result.setLinearVelocity)
        sourceVector_ = result.linearVelocity;
    return result;
}

Proj::MineUpdateResult Proj::ProgressMine(
    float deltaTime, float delay) noexcept
{
    auto result = MineUpdate(sourceTimer_, deltaTime, delay);
    sourceTimer_ = result.timer;
    if (description_.type == masloProjectileType &&
        result.visualScale >= 0.0F && sourceModel_ != nullptr)
    {
        sourceModel_->GetGameObj().SetScale(
            {result.visualScale, result.visualScale,
             result.visualScale});
    }
    return result;
}

float Proj::ProgressThunder(float deltaTime) noexcept
{
    sourceTimer_ = ThunderUpdate(sourceTimer_, deltaTime);
    return sourceTimer_;
}

Proj::ThunderContactResult Proj::ContactThunder(
    Vec3 linearVelocity, Vec3 contactNormal,
    bool shotTransparencyContact) noexcept
{
    auto result = ThunderContact(
        linearVelocity, contactNormal, sourceTimer_,
        shotTransparencyContact);
    if (result.setLinearVelocity)
        sourceTimer_ = result.reflectionCooldown;
    return result;
}

Proj::ImpulseContactResult Proj::ContactImpulse(
    bool hasContactActor, bool hasTarget,
    bool contactIsTarget, float damage) noexcept
{
    auto result = ImpulseContact(
        hasContactActor, hasTarget, contactIsTarget,
        sourceTick_, damage);
    sourceTick_ = result.hitCount;
    return result;
}

Player* Proj::FindNextTarget(
    Player* currentTarget, Player* weaponOwner,
    std::span<Player* const> players, float viewAngle) noexcept
{
    // Weapon.cpp::FindNextTaget starts from the player represented by the
    // current ShotDesc target. Race/MapObj lookup is an adapter concern, so
    // the portable caller supplies that already-resolved source player. The
    // Jolt session currently removes a dead car MapObj before returning from
    // its damage adapter; that removal clears target_. The captured Player is
    // the exact source callback target and intentionally remains valid for
    // this synchronous post-damage search, matching Windows deferred cleanup.
    if (currentTarget == nullptr)
        return nullptr;

    Player* next = currentTarget->FindClosestEnemy(
        viewAngle, false, players);
    if (next == nullptr)
        return nullptr;

    // The first search is centred on the contacted player and can select the
    // projectile owner. Windows skips that owner exactly once by searching
    // again from it, then rejects a cycle back to the contacted player.
    if (weaponOwner != nullptr && next == weaponOwner)
    {
        next = weaponOwner->FindClosestEnemy(
            viewAngle, false, players);
        if (next == nullptr || next == currentTarget)
            return nullptr;
    }
    return next;
}

void Proj::RetargetImpulse(GameObject* target) noexcept
{
    SetSourceTarget(target);
    sourceTimer_ = 0.0F;
}

Proj::LaserUpdateResult Proj::ProgressLaser(
    float maximumDistance, bool hit, float hitDistance,
    float deltaTime, float damage, bool distort,
    float timeLife, float maximumTimeLife,
    Vec3 worldDirection) noexcept
{
    auto result = LaserUpdate(
        maximumDistance, hit, hitDistance, deltaTime, damage,
        distort, timeLife, maximumTimeLife);
    if (sourceModel2_ != nullptr)
    {
        if (hit)
        {
            const auto position = GetWorldPos();
            sourceModel2_->GetGameObj().SetWorldPos(
                {position[0] + worldDirection.x * result.distance,
                 position[1] + worldDirection.y * result.distance,
                 position[2] + worldDirection.z * result.distance});
        }
        else
        {
            sourceModel2_->GetGameObj().SetPos(
                {result.distance, 0.0F, 0.0F});
        }
    }
    return result;
}

Proj::ContinuousContactResult Proj::ContactDrobilka(
    bool hasTarget, float damage, float deltaTime,
    Vec3 contactPoint) noexcept
{
    auto result = DrobilkaContact(hasTarget, damage, deltaTime);
    if (!hasTarget)
        return result;
    sourceTimer_ = 0.5F;
    InitSourceModel(false);
    if (sourceModel_ != nullptr)
    {
        sourceModel_->GetGameObj().SetWorldPos(
            {contactPoint.x, contactPoint.y, contactPoint.z});
    }
    return result;
}

void Proj::ProgressDrobilka(float deltaTime) noexcept
{
    if (weapon_ != nullptr)
    {
        const float halfAngle = description_.angularSpeed * deltaTime * 0.5F;
        const Quat delta{
            std::sin(halfAngle), 0.0F, 0.0F,
            std::cos(halfAngle)};
        const auto rotation = weapon_->GetRot();
        const Quat local{
            rotation[0], rotation[1], rotation[2], rotation[3]};
        const auto next = normalized(multiply(delta, local));
        weapon_->SetRot({next.x, next.y, next.z, next.w});
    }
    if (sourceModel_ == nullptr ||
        sourceModel_->GetGameObj().GetLiveState() == LiveState::Death)
        return;
    sourceTimer_ -= deltaTime;
    if (sourceTimer_ <= 0.0F)
    {
        sourceModel_->GetGameObj().Death();
        FreeSourceModel(false, false);
    }
}

Proj::ContactRoute Proj::RouteContact(
    bool targetDestroyed) const noexcept
{
    return ContactRouteFor(
        description_.type, destroyed, targetDestroyed);
}

Proj::ProgressRoute Proj::RouteProgress() const noexcept
{
    return ProgressRouteFor(description_.type);
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

DamageType Proj::DamageTypeFor(std::uint32_t type) noexcept
{
    switch (static_cast<ProjectileType>(type))
    {
    case ProjectileType::Laser:
    case ProjectileType::Sonar:
    case ProjectileType::FrostRay:
    case ProjectileType::Impulse:
        return DamageType::Energy;
    case ProjectileType::Mine:
    case ProjectileType::MineRip:
    case ProjectileType::MinePiece:
    case ProjectileType::Crater:
    case ProjectileType::MineProton:
        return DamageType::Mine;
    default:
        return DamageType::Simple;
    }
}

Proj::ContactRoute Proj::ContactRouteFor(
    std::uint32_t type, bool projectileDestroyed,
    bool targetDestroyed) noexcept
{
    ContactRoute result;
    result.damageType = DamageTypeFor(type);
    // This is the source Proj::OnContact live-state guard. A missing target
    // is allowed by Windows (border/transparent contacts still reach the
    // selected handler), but an already-dead concrete target is not.
    if (projectileDestroyed || targetDestroyed)
        return result;

    switch (static_cast<ProjectileType>(type))
    {
    case ProjectileType::Rocket:
        result.handler = ContactHandler::Rocket;
        result.rocketResponse = true;
        result.appliesDamage = true;
        break;
    case ProjectileType::Torpeda:
        result.handler = ContactHandler::Torpeda;
        result.rocketResponse = true;
        result.appliesDamage = true;
        break;
    case ProjectileType::Medpack:
        result.handler = ContactHandler::Medpack;
        break;
    case ProjectileType::Charge:
        result.handler = ContactHandler::Charge;
        break;
    case ProjectileType::Money:
        result.handler = ContactHandler::Money;
        break;
    case ProjectileType::Immortal:
        result.handler = ContactHandler::Immortal;
        break;
    case ProjectileType::SpeedArrow:
        result.handler = ContactHandler::SpeedArrow;
        break;
    case ProjectileType::Lusha:
        result.handler = ContactHandler::Lusha;
        break;
    case ProjectileType::Maslo:
        result.handler = ContactHandler::Maslo;
        break;
    case ProjectileType::Mine:
        result.handler = ContactHandler::Mine;
        result.testMineLock = true;
        result.appliesDamage = true;
        break;
    case ProjectileType::MineRip:
        result.handler = ContactHandler::MineRip;
        result.testMineLock = true;
        result.appliesDamage = true;
        break;
    case ProjectileType::MinePiece:
        result.handler = ContactHandler::MinePiece;
        result.appliesDamage = true;
        break;
    case ProjectileType::Fire:
        result.handler = ContactHandler::Fire;
        result.appliesDamage = true;
        break;
    case ProjectileType::Drobilka:
        result.handler = ContactHandler::Drobilka;
        result.appliesDamage = true;
        break;
    case ProjectileType::Sonar:
        result.handler = ContactHandler::Sonar;
        result.appliesDamage = true;
        break;
    case ProjectileType::Mortira:
        result.handler = ContactHandler::Mortira;
        result.rocketResponse = true;
        result.appliesDamage = true;
        break;
    case ProjectileType::Crater:
        result.handler = ContactHandler::Crater;
        result.appliesDamage = true;
        break;
    case ProjectileType::Impulse:
        result.handler = ContactHandler::Impulse;
        result.appliesDamage = true;
        break;
    case ProjectileType::Thunder:
        result.handler = ContactHandler::Thunder;
        result.rocketResponse = true;
        result.appliesDamage = true;
        break;
    case ProjectileType::Resonanse:
        result.handler = ContactHandler::Resonanse;
        result.rocketResponse = true;
        result.appliesDamage = true;
        break;
    case ProjectileType::MineProton:
        result.handler = ContactHandler::MineProton;
        result.appliesDamage = true;
        break;
    case ProjectileType::Hyper:
    case ProjectileType::Laser:
    case ProjectileType::Spring:
    case ProjectileType::FrostRay:
    default:
        break;
    }
    return result;
}

Proj::ProgressRoute Proj::ProgressRouteFor(
    std::uint32_t type) noexcept
{
    ProgressRoute result;
    switch (static_cast<ProjectileType>(type))
    {
    case ProjectileType::Rocket:
        result.handler = ProgressHandler::Rocket;
        result.rocketHeight = true;
        break;
    case ProjectileType::Torpeda:
        result.handler = ProgressHandler::Torpeda;
        result.homing = true;
        break;
    case ProjectileType::Laser:
        result.handler = ProgressHandler::Laser;
        result.attached = true;
        result.ray = true;
        break;
    case ProjectileType::Fire:
        result.handler = ProgressHandler::Fire;
        result.attached = true;
        break;
    case ProjectileType::Maslo:
        result.handler = ProgressHandler::Maslo;
        result.mineArming = true;
        break;
    case ProjectileType::Mine:
        result.handler = ProgressHandler::Mine;
        result.mineArming = true;
        break;
    case ProjectileType::MineRip:
        result.handler = ProgressHandler::MineRip;
        result.mineArming = true;
        break;
    case ProjectileType::MineProton:
        result.handler = ProgressHandler::MineProton;
        result.mineArming = true;
        break;
    case ProjectileType::Drobilka:
        result.handler = ProgressHandler::Drobilka;
        result.attached = true;
        break;
    case ProjectileType::Spring:
        result.handler = ProgressHandler::Spring;
        result.attached = true;
        break;
    case ProjectileType::FrostRay:
        result.handler = ProgressHandler::FrostRay;
        result.attached = true;
        result.ray = true;
        break;
    case ProjectileType::Impulse:
        result.handler = ProgressHandler::Impulse;
        result.homing = true;
        break;
    case ProjectileType::Thunder:
        result.handler = ProgressHandler::Thunder;
        result.rocketHeight = true;
        break;
    case ProjectileType::Resonanse:
        result.handler = ProgressHandler::Resonanse;
        result.rocketHeight = true;
        break;
    default:
        break;
    }
    return result;
}

Proj::PreparationRoute Proj::PreparationRouteFor(
    std::uint32_t type) noexcept
{
    PreparationRoute result;
    if (type > static_cast<std::uint32_t>(ProjectileType::MineProton))
        return result;

    result.valid = true;
    // PrepareHandler keeps the source ProjectileType order after None.
    result.handler = static_cast<PrepareHandler>(type + 1U);
    result.initializeModel =
        type != static_cast<std::uint32_t>(ProjectileType::Drobilka) &&
        type != static_cast<std::uint32_t>(ProjectileType::Spring);

    switch (static_cast<ProjectileType>(type))
    {
    case ProjectileType::Rocket:
        result.rocketPrepare = true;
        result.requiresWeapon = true;
        break;
    case ProjectileType::Hyper:
        result.attached = true;
        result.linkedToWeapon = true;
        result.requiresWeapon = true;
        break;
    case ProjectileType::Torpeda:
        result.rocketPrepare = true;
        result.homing = true;
        result.requiresWeapon = true;
        break;
    case ProjectileType::Laser:
        result.attached = true;
        result.linkedToWeapon = true;
        result.ray = true;
        result.initializeSecondaryModel = true;
        result.requiresWeapon = true;
        break;
    case ProjectileType::Medpack:
    case ProjectileType::Charge:
    case ProjectileType::Money:
    case ProjectileType::Immortal:
    case ProjectileType::SpeedArrow:
        result.requiresWeapon = true;
        break;
    case ProjectileType::Lusha:
        break;
    case ProjectileType::Maslo:
        result.minePlacement = true;
        result.lockMineOnPlacement = true;
        break;
    case ProjectileType::Mine:
    case ProjectileType::MineRip:
        result.minePlacement = true;
        result.lockMineOnPlacement = true;
        result.mineTestsLock = true;
        break;
    case ProjectileType::MinePiece:
        break;
    case ProjectileType::Fire:
        result.rocketPrepare = true;
        result.attached = true;
        result.requiresWeapon = true;
        break;
    case ProjectileType::Drobilka:
        result.attached = true;
        result.requiresWeapon = true;
        break;
    case ProjectileType::Sonar:
        result.rocketPrepare = true;
        result.requiresWeapon = true;
        break;
    case ProjectileType::Spring:
        result.attached = true;
        result.linkedToWeapon = true;
        result.requiresWeapon = true;
        break;
    case ProjectileType::FrostRay:
        result.attached = true;
        result.linkedToWeapon = true;
        result.ray = true;
        result.initializeSecondaryModel = true;
        result.requiresWeapon = true;
        break;
    case ProjectileType::Mortira:
        result.rocketPrepare = true;
        result.ballistic = true;
        result.requiresWeapon = true;
        break;
    case ProjectileType::Crater:
        result.minePlacement = true;
        result.requiresWeapon = true;
        break;
    case ProjectileType::Impulse:
        result.rocketPrepare = true;
        result.homing = true;
        result.requiresWeapon = true;
        break;
    case ProjectileType::Thunder:
    case ProjectileType::Resonanse:
        result.rocketPrepare = true;
        break;
    case ProjectileType::MineProton:
        result.minePlacement = true;
        result.lockMineOnPlacement = true;
        break;
    }
    result.ignoreWeaponContact =
        result.rocketPrepare || result.ray ||
        result.handler == PrepareHandler::Drobilka;
    return result;
}

Proj::TypeRules Proj::GetTypeRules(std::uint32_t type) noexcept
{
    const auto route = PreparationRouteFor(type);
    return {route.rocketPrepare, route.attached,
            route.linkedToWeapon, route.ray, route.homing,
            route.ballistic, route.mineTestsLock};
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

ProjectileCollisionBox Proj::ComputeAABB(
    const ProjectileDefinition& description,
    bool onlyModel) noexcept
{
    const auto serializedBox = [&]() {
        ProjectileCollisionBox box;
        box.center = description.offset;
        box.halfExtents = {
            std::max(description.size.x * 0.5F, 0.0F),
            std::max(description.size.y * 0.5F, 0.0F),
            std::max(description.size.z * 0.5F, 0.0F)};
        return box;
    };
    if (!description.modelSize || !description.modelBoundsValid)
    {
        if (!onlyModel)
            return serializedBox();
        // AABB(IdentityVector * 0.1f) in the original constructor.
        return {{}, {0.05F, 0.05F, 0.05F}};
    }

    ProjectileCollisionBox result = onlyModel
        ? ProjectileCollisionBox{}
        : serializedBox();
    const Vec3 firstMinimum{
        result.center.x - result.halfExtents.x,
        result.center.y - result.halfExtents.y,
        result.center.z - result.halfExtents.z};
    const Vec3 firstMaximum{
        result.center.x + result.halfExtents.x,
        result.center.y + result.halfExtents.y,
        result.center.z + result.halfExtents.z};
    const Vec3 modelMinimum{
        description.modelBounds.center.x -
            description.modelBounds.halfExtents.x,
        description.modelBounds.center.y -
            description.modelBounds.halfExtents.y,
        description.modelBounds.center.z -
            description.modelBounds.halfExtents.z};
    const Vec3 modelMaximum{
        description.modelBounds.center.x +
            description.modelBounds.halfExtents.x,
        description.modelBounds.center.y +
            description.modelBounds.halfExtents.y,
        description.modelBounds.center.z +
            description.modelBounds.halfExtents.z};
    const Vec3 minimum{
        std::min(firstMinimum.x, modelMinimum.x),
        std::min(firstMinimum.y, modelMinimum.y),
        std::min(firstMinimum.z, modelMinimum.z)};
    const Vec3 maximum{
        std::max(firstMaximum.x, modelMaximum.x),
        std::max(firstMaximum.y, modelMaximum.y),
        std::max(firstMaximum.z, modelMaximum.z)};
    result.center = {
        (minimum.x + maximum.x) * 0.5F,
        (minimum.y + maximum.y) * 0.5F,
        (minimum.z + maximum.z) * 0.5F};
    result.halfExtents = {
        (maximum.x - minimum.x) * 0.5F,
        (maximum.y - minimum.y) * 0.5F,
        (maximum.z - minimum.z) * 0.5F};
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
    ProjectileDefinition description;
    description.type = type;
    Reset(description);
}

void AutoProj::Reset(
    const ProjectileDefinition& description) noexcept
{
    LogicReleased();
    autoDescription_ = description;
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

    const auto position = GetWorldPos();
    const auto rotation = GetWorldRot();
    const float maximumLife = GetMaxLife();
    const float life = GetLife();
    const float maximumTimeLife = GetMaxTimeLife();
    const float timeLife = GetTimeLife();

    ShotContext context;
    context.logic = GetLogic();
    context.maximumLife = maximumTimeLife;
    context.position = {position[0], position[1], position[2]};
    context.rotation = {
        rotation[0], rotation[1], rotation[2], rotation[3]};
    PrepareSource(autoDescription_, nullptr, context);
    // AutoProj belongs to a MapObjects category rather than Logic's
    // transient registry. Restore the serialized proxy lifetime which
    // PrepareProj leaves on the map record and use normal auto-expiry.
    SetExternalLifetimeManaged(false);
    SetMaxLife(maximumLife);
    SetLife(life);
    SetMaxTimeLife(maximumTimeLife);
    SetTimeLife(timeLife);
    prepared_ = true;
    if (UsesMineUpdate(autoDescription_.type))
        SetSourceTimer(0.0F);
    else
        SetSourceTimer(-1.0F);
    if (autoDescription_.type == masloType)
    {
        modelScale_ = 0.0F;
        if (auto* model = GetSourceModel())
            model->GetGameObj().SetScale({0.0F, 0.0F, 0.0F});
    }
}

void AutoProj::LogicReleased() noexcept
{
    prepared_ = false;
}

void AutoProj::OnProgress(float deltaTime) noexcept
{
    if (!prepared_ || !UsesMineUpdate(autoDescription_.type))
        return;
    const auto result = Proj::MineUpdate(
        GetSourceTimer(), deltaTime);
    SetSourceTimer(result.timer);
    if (autoDescription_.type == masloType &&
        result.visualScale >= 0.0F)
    {
        modelScale_ = result.visualScale;
        if (auto* model = GetSourceModel())
        {
            model->GetGameObj().SetScale(
                {modelScale_, modelScale_, modelScale_});
        }
    }
}

bool AutoProj::IsPrepared() const noexcept
{
    return prepared_;
}

bool AutoProj::IsArming() const noexcept
{
    return prepared_ && GetSourceTimer() >= 0.0F;
}

float AutoProj::GetModelScale() const noexcept
{
    return modelScale_;
}

std::uint32_t AutoProj::GetType() const noexcept
{
    return autoDescription_.type;
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

std::vector<Weapon::ShotContext> Weapon::MakeShotContexts(
    const ShotDesc& shot)
{
    std::vector<ShotContext> contexts;
    contexts.reserve(desc_->projectiles.size());
    const auto weaponPosition = GetWorldPos();
    const auto weaponRotation = GetWorldRot();
    const auto weaponScale = GetWorldScale();
    const Proj::Quat worldRotation{
        weaponRotation[0], weaponRotation[1],
        weaponRotation[2], weaponRotation[3]};
    for (const auto& projectile : desc_->projectiles)
    {
        ShotContext context;
        context.logic = GetLogic();
        context.shot = shot;
        context.maximumLife = Proj::PrepareMaximumLife(
            projectile.speed, projectile.maximumDistance,
            projectile.minimumLife);
        const Proj::Vec3 localPosition{
            projectile.position.x * weaponScale[0],
            projectile.position.y * weaponScale[1],
            projectile.position.z * weaponScale[2]};
        const auto worldOffset = rotate(
            worldRotation, localPosition);
        context.position = {
            weaponPosition[0] + worldOffset.x,
            weaponPosition[1] + worldOffset.y,
            weaponPosition[2] + worldOffset.z};
        const Proj::Quat localRotation{
            projectile.rotation.x, projectile.rotation.y,
            projectile.rotation.z, projectile.rotation.w};
        context.rotation = normalized(multiply(
            worldRotation, localRotation));
        const auto route = Proj::PreparationRouteFor(projectile.type);
        if (route.rocketPrepare)
        {
            const auto direction = normalized(rotate(
                context.rotation, {1.0F, 0.0F, 0.0F}));
            const auto launch = Proj::CalcSpeed(
                direction, {}, projectile.speed,
                projectile.relativeSpeedMinimum,
                projectile.relativeSpeed);
            context.launchVelocity = launch.linearVelocity;
        }
        contexts.push_back(context);
    }
    return contexts;
}

bool Weapon::Shot(
    const ShotDesc& shot, ProjList* projectiles)
{
    const auto contexts = MakeShotContexts(shot);
    return CreateShot(this, *desc_, contexts, projectiles);
}

bool Weapon::Shot(Proj::Vec3 target, ProjList* projectiles)
{
    ShotDesc shot;
    shot.target = target;
    return Shot(shot, projectiles);
}

bool Weapon::Shot(GameObject* target, ProjList* projectiles)
{
    ShotDesc shot;
    shot.targetMapObject = target;
    return Shot(shot, projectiles);
}

bool Weapon::Shot(ProjList* projectiles)
{
    return Shot(ShotDesc{}, projectiles);
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

Proj* Weapon::CreateShot(
    Weapon* weapon, const ProjectileDefinition& description,
    const ShotContext& context)
{
    const auto route = Proj::PreparationRouteFor(description.type);
    if (context.logic == nullptr || !context.preparationAccepted ||
        !route.valid || (weapon == nullptr && route.requiresWeapon))
        return nullptr;

    auto* projectile = new Proj();
    projectile->PrepareSource(description, weapon, context);
    context.logic->RegGameObj(projectile);

    if (weapon != nullptr)
    {
        weapon->OnShot(true);
        weapon->OnProjectilePrepared(
            {description.position.x, description.position.y,
             description.position.z});
    }
    return projectile;
}

bool Weapon::CanCreateWithoutWeapon(
    std::uint32_t projectileType) noexcept
{
    const auto route = Proj::PreparationRouteFor(projectileType);
    return route.valid && !route.requiresWeapon;
}

bool Weapon::CreateShot(
    Weapon* weapon, const Desc& description,
    std::span<const ShotContext> contexts,
    ProjList* projectiles)
{
    bool created = false;
    for (std::size_t index = 0U;
         index < description.projectiles.size(); ++index)
    {
        const auto& projectileDescription =
            description.projectiles[index];
        if (weapon == nullptr &&
            !CanCreateWithoutWeapon(projectileDescription.type))
            continue;
        if (index >= contexts.size())
            continue;
        auto* projectile = CreateShot(
            weapon, projectileDescription, contexts[index]);
        if (projectile == nullptr)
            continue;
        if (projectiles != nullptr)
            projectiles->push_back(projectile);
        created = true;
    }
    return created;
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
    carAttached_ = weapon_ != nullptr;
    if (weapon_ != nullptr)
        weapon_->SetDescHandle(weaponDesc_);
}

void WeaponItem::OnDestroyCar() noexcept
{
    carAttached_ = false;
    weapon_ = nullptr;
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

void WeaponItem::AttachWeapon(Weapon* weapon) noexcept
{
    weapon_ = weapon;
    if (weapon_ != nullptr)
        weapon_->SetDescHandle(weaponDesc_);
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

const std::string& WeaponItem::GetMapObjRecord() const noexcept
{
    return mapObjRecord_;
}

void WeaponItem::SetMapObjRecord(std::string value)
{
    mapObjRecord_ = std::move(value);
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

} // namespace r3d::game::originalrace::source
