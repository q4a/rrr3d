#include "OriginalGameCar.h"
#include "OriginalRace.h"

#include <algorithm>
#include <cmath>

namespace r3d::game::originalrace::source
{

namespace
{

using SyncVector = GameObjectFrameSync::Vector;
using SyncQuaternion = GameObjectFrameSync::Quaternion;

SyncQuaternion normalizedSync(SyncQuaternion value) noexcept
{
    const float length = std::sqrt(
        value.x * value.x + value.y * value.y +
        value.z * value.z + value.w * value.w);
    if (length <= 0.000001F)
        return {};
    value.x /= length;
    value.y /= length;
    value.z /= length;
    value.w /= length;
    return value;
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

SyncQuaternion axisAngleSync(
    SyncVector axis, float angle) noexcept
{
    const float halfAngle = angle * 0.5F;
    const float sine = std::sin(halfAngle);
    return normalizedSync({
        axis.x * sine, axis.y * sine, axis.z * sine,
        std::cos(halfAngle)});
}

SyncVector rotateSync(
    SyncVector value, SyncQuaternion rotation) noexcept
{
    rotation = normalizedSync(rotation);
    const SyncVector axis{rotation.x, rotation.y, rotation.z};
    const SyncVector cross{
        axis.y * value.z - axis.z * value.y,
        axis.z * value.x - axis.x * value.z,
        axis.x * value.y - axis.y * value.x};
    const SyncVector cross2{
        axis.y * cross.z - axis.z * cross.y,
        axis.z * cross.x - axis.x * cross.z,
        axis.x * cross.y - axis.y * cross.x};
    return {
        value.x + 2.0F * (rotation.w * cross.x + cross2.x),
        value.y + 2.0F * (rotation.w * cross.y + cross2.y),
        value.z + 2.0F * (rotation.w * cross.z + cross2.z)};
}

using ContactVector = GameCar::ContactVector;

float contactLength(ContactVector value) noexcept
{
    return std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z);
}

ContactVector normalizedContact(ContactVector value) noexcept
{
    const float length = contactLength(value);
    if (length <= 0.000001F)
        return {};
    return {value.x / length, value.y / length, value.z / length};
}

float contactDot(ContactVector left, ContactVector right) noexcept
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

ContactVector contactCross(
    ContactVector left, ContactVector right) noexcept
{
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x};
}

ContactVector contactAdd(
    ContactVector left, ContactVector right) noexcept
{
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

ContactVector contactMultiply(ContactVector value, float scale) noexcept
{
    return {value.x * scale, value.y * scale, value.z * scale};
}

float contactDamage(
    const std::array<float, 2U>& damage,
    const std::array<float, 2U>& forceRange,
    float force, float& forceAlpha) noexcept
{
    forceAlpha = 0.0F;
    if (forceRange[1U] > forceRange[0U])
    {
        forceAlpha = std::clamp(
            (force - forceRange[0U]) /
                (forceRange[1U] - forceRange[0U]),
            0.0F, 1.0F);
    }
    else if (force > forceRange[0U])
    {
        forceAlpha = 0.5F;
    }
    return damage[0U] +
           (damage[1U] - damage[0U]) * forceAlpha;
}

} // namespace

GameCar::GameCar()
{
    RegFixedStepEvent();
}

GameCar::GameCar(const GameCar& other) : GameObject(other)
{
    RegFixedStepEvent();
    *this = other;
}

GameCar& GameCar::operator=(const GameCar& other) noexcept
{
    if (this == &other)
        return *this;
    ReleaseAnimationChildren();
    ReleaseWheels();
    ReleaseSoundMotor();
    GameObject::operator=(other);
    clutchStrength_ = other.clutchStrength_;
    clutchTime_ = other.clutchTime_;
    springTime_ = other.springTime_;
    mineTime_ = other.mineTime_;
    signedSpeed_ = other.signedSpeed_;
    motor_ = other.motor_;
    dynamics_ = other.dynamics_;
    moveCar_ = other.moveCar_;
    steerWheel_ = other.steerWheel_;
    currentGear_ = other.currentGear_;
    steeringAngle_ = other.steeringAngle_;
    steeringControl_ = other.steeringControl_;
    maximumSpeed_ = other.maximumSpeed_;
    tireSpring_ = other.tireSpring_;
    motorTorqueK_ = other.motorTorqueK_;
    wheelSteerK_ = other.wheelSteerK_;
    anyWheelContact_ = other.anyWheelContact_;
    wheelsContact_ = other.wheelsContact_;
    bodyContact_ = other.bodyContact_;
    clutchImmunity_ = other.clutchImmunity_;
    disableColor_ = other.disableColor_;
    rpmVolumeRange_ = other.rpmVolumeRange_;
    rpmFrequencyRange_ = other.rpmFrequencyRange_;
    soundMotorMix_ = other.soundMotorMix_;
    if (other.HasSoundMotor())
    {
        BindSoundMotor(rpmVolumeRange_, rpmFrequencyRange_);
        *soundMotor_ = *other.soundMotor_;
        soundMotorMix_ = other.soundMotorMix_;
    }
    else
    {
        ReleaseSoundMotor();
    }
    std::vector<bool> slipEffects;
    std::vector<bool> slipSounds;
    slipEffects.reserve(other.wheels_.size());
    slipSounds.reserve(other.wheels_.size());
    for (const auto& wheel : other.wheels_)
    {
        slipEffects.push_back(
            wheel != nullptr && wheel->HasSlipEffect());
        slipSounds.push_back(
            wheel != nullptr && wheel->HasSlipSound());
    }
    BindWheels(slipEffects, slipSounds);
    for (std::size_t index = 0U; index < wheels_.size(); ++index)
    {
        if (wheels_[index] != nullptr &&
            other.wheels_[index] != nullptr)
        {
            *wheels_[index] = *other.wheels_[index];
            wheels_[index]->SetParent(this);
        }
    }
    for (const auto& child : other.animationChildren_)
    {
        if (child == nullptr)
            continue;
        auto copy = std::make_unique<CarAnimationChild>(*child);
        copy->SetParent(this);
        animationChildren_.push_back(std::move(copy));
    }
    return *this;
}

GameCar::GameCar(GameCar&& other) : GameCar(other) {}

GameCar& GameCar::operator=(GameCar&& other) noexcept
{
    return *this = static_cast<const GameCar&>(other);
}

GameCar::~GameCar()
{
    UnregFixedStepEvent();
    // GameCar::~GameCar in the Windows source repeats Destroy after
    // RockCar::~RockCar; the idempotent guard makes this safe for a bare
    // GameCar while retaining the original listener lifecycle.
    DestroyObject();
    ReleaseAnimationChildren();
    ReleaseWheels();
    ReleaseSoundMotor();
}

GameCar* GameCar::IsCar() noexcept { return this; }
const GameCar* GameCar::IsCar() const noexcept { return this; }

GameCar::ContactResult GameCar::OnContact(
    const ContactInput& contact, const ContactRules& rules) noexcept
{
    ContactResult result;
    bodyContact_ = true;
    result.bodyContact = true;

    const float forceLength = contactLength(contact.normalForce);
    if (contact.target == ContactTarget::Track)
    {
        float forceAlpha = 0.0F;
        const float damage = contactDamage(
            rules.borderDamage, rules.borderDamageForce,
            forceLength, forceAlpha);

        if (!rules.springBorders && forceAlpha == 0.0F)
            return result;

        if (forceLength > 0.01F)
        {
            ContactVector normal = normalizedContact(contact.normalForce);
            if (contact.invertNormal)
                normal = contactMultiply(normal, -1.0F);

            const bool borderContact =
                std::abs(normal.z) < 0.5F && contact.shotTransparency;
            if (borderContact)
            {
                CancelClutch();
                result.cancelClutch = true;
            }

            if (borderContact && contactLength(contact.linearVelocity) > 16.0F)
            {
                if (rules.springBorders)
                {
                    ContactVector tangent =
                        normalizedContact(contact.linearVelocity);
                    const float tangentDot =
                        std::abs(contactDot(tangent, normal));
                    const ContactVector direction =
                        normalizedContact(contact.forward);
                    const float directionDot =
                        contactDot(direction, normal);
                    const float directionTravelDot =
                        contactDot(direction, tangent);

                    if (tangentDot > 0.1F &&
                        (directionDot < 0.707F ||
                         directionTravelDot < -0.707F))
                    {
                        if (tangentDot < 0.995F)
                        {
                            const ContactVector binormal =
                                contactCross(normal, tangent);
                            tangent = contact.frictionForce;
                            tangent.z = binormal.z > 0.0F
                                ? std::abs(tangent.z)
                                : -std::abs(tangent.z);
                            tangent = contactCross(
                                normalizedContact(tangent), normal);
                        }
                        else
                        {
                            tangent = {};
                        }

                        const float normalVelocity = std::clamp(
                            std::abs(contactDot(
                                normal, contact.linearVelocity)),
                            4.0F, 14.0F);
                        const float tangentVelocity =
                            contactDot(tangent, contact.linearVelocity) *
                            0.5F;
                        result.linearVelocity = contactAdd(
                            contactMultiply(normal, normalVelocity),
                            contactMultiply(tangent, tangentVelocity));
                        result.linearVelocity.z = 0.0F;
                        result.setLinearVelocity = true;
                    }
                }

                if (forceAlpha > 0.0F && damage > 0.0F)
                {
                    result.damageTarget = ContactDamageTarget::Source;
                    result.attackerPlayerId = contact.sourcePlayerId;
                    result.damage = damage;
                }
            }
        }
        return result;
    }

    if (contact.target == ContactTarget::Car)
    {
        if (!contact.targetDynamic)
            return result;
        float forceAlpha = 0.0F;
        const float damage = contactDamage(
            rules.carDamage, rules.carDamageForce,
            forceLength, forceAlpha);
        if (forceAlpha <= 0.0F || damage <= 0.0F)
            return result;
        if (contact.sourceKineticEnergy > contact.targetKineticEnergy)
        {
            result.damageTarget = ContactDamageTarget::Target;
            result.attackerPlayerId = contact.sourcePlayerId;
        }
        else
        {
            result.damageTarget = ContactDamageTarget::Source;
            result.attackerPlayerId = contact.targetPlayerId;
        }
        result.damage = damage;
        return result;
    }

    if (contact.target == ContactTarget::Other)
    {
        result.attackerPlayerId = contact.sourcePlayerId;
        result.touchTarget = true;
    }
    return result;
}

void GameCar::Reset() noexcept
{
    clutchStrength_ = 0.0F;
    clutchTime_ = 0.0F;
    springTime_ = 0.0F;
    mineTime_ = 0.0F;
    signedSpeed_ = 0.0F;
    moveCar_ = MoveCarState::None;
    steerWheel_ = SteerWheelState::None;
    currentGear_ = -1;
    steeringAngle_ = 0.0F;
    motorTorqueK_ = 1.0F;
    wheelSteerK_ = 1.0F;
    anyWheelContact_ = false;
    wheelsContact_ = false;
    bodyContact_ = false;
    for (auto& wheel : wheels_)
    {
        if (wheel != nullptr)
        {
            wheel->ResetMotion();
            wheel->SetContact(false, 0.0F, 0.0F, 0.0F, 0.0F);
        }
    }
    SetSyncFrameEvent(false);
    SetBodyProgressEvent(false);
    GetFrameSync().Reset();
    if (soundMotor_ != nullptr)
        soundMotor_->Reset();
    soundMotorMix_ = {};
}

GameCar::ProgressResult GameCar::OnProgress(float deltaTime) noexcept
{
    ProgressResult result;
    const auto gameObject = GameObject::OnProgress(deltaTime);
    result.gameObject = gameObject;
    result.behaviorsProgressed = gameObject.behaviorsProgressed;
    result.behaviorsRemoved = gameObject.behaviorsRemoved;
    for (auto& child : animationChildren_)
    {
        if (child == nullptr)
            continue;
        const auto childResult = child->OnProgress(deltaTime);
        ++result.animationChildrenProgressed;
        result.animationBehaviorsProgressed +=
            childResult.behaviorsProgressed;
    }
    for (auto& wheel : wheels_)
    {
        if (wheel == nullptr)
            continue;
        const auto wheelResult = wheel->OnProgress(deltaTime);
        ++result.wheelsProgressed;
        result.wheelBehaviorsProgressed +=
            wheelResult.behaviorsProgressed;
    }
    return result;
}

GameCar::PxSyncState GameCar::OnPxSync(
    PxSyncPose physicalBody,
    const std::vector<PxSyncPose>& physicalWheels,
    float deltaTime, float physicsAlpha) noexcept
{
    PxSyncState state;
    state.body = GetFrameSync().OnFrame(
        physicalBody, deltaTime, physicsAlpha);
    // GameObject::OnPxSync in the Windows source writes the interpolated
    // PhysX pose into the graph actor before GameCar synchronizes its wheel
    // children.  GameObject's portable transform is that graph-actor state:
    // publish it here instead of leaving the source owner at its spawn pose.
    SetWorldPos({state.body.position.x, state.body.position.y,
                 state.body.position.z});
    SetWorldRot({state.body.rotation.x, state.body.rotation.y,
                 state.body.rotation.z, state.body.rotation.w});
    const std::size_t count = std::min(
        wheels_.size(), physicalWheels.size());
    state.wheels.reserve(count);
    for (std::size_t index = 0U; index < count; ++index)
    {
        if (wheels_[index] == nullptr)
            continue;
        auto& wheel = *wheels_[index];
        const auto wheelPose = wheel.PxSyncWheel(
            physicalBody, state.body, physicalWheels[index],
            deltaTime, physicsAlpha);
        // CarWheel::PxSyncWheel writes its local graph actor in Windows.
        // SetWorld* performs the equivalent parent-relative conversion while
        // retaining the already computed renderer-facing world pose.
        wheel.SetWorldPos({wheelPose.position.x, wheelPose.position.y,
                           wheelPose.position.z});
        wheel.SetWorldRot({wheelPose.rotation.x, wheelPose.rotation.y,
                           wheelPose.rotation.z, wheelPose.rotation.w});
        state.wheels.push_back(wheelPose);
    }
    return state;
}

GameCar::PxSyncState GameCar::DispatchPxSync(
    WorldEventPump& world, PxSyncPose physicalBody,
    const std::vector<PxSyncPose>& physicalWheels,
    float deltaTime, float physicsAlpha) noexcept
{
    pendingPhysicalBody_ = physicalBody;
    pendingPhysicalWheels_ = &physicalWheels;
    pxSyncPending_ = true;
    const bool dispatched = world.DispatchFrameEvent(
        this, deltaTime, physicsAlpha);
    pxSyncPending_ = false;
    pendingPhysicalWheels_ = nullptr;
    if (dispatched && pxSyncInitialized_)
        return lastPxSyncState_;
    if (pxSyncInitialized_)
        return lastPxSyncState_;

    // An object which has never woken has no source frame registration.
    // Preserve its physical pose without advancing correction state.
    PxSyncState state;
    state.body = physicalBody;
    state.wheels = physicalWheels;
    return state;
}

GameObjectFrameSync::NetworkCorrection GameCar::SynchronizeNetworkPose(
    GameObjectFrameSync::Vector physicsPosition,
    GameObjectFrameSync::Vector graphPosition,
    GameObjectFrameSync::Quaternion graphRotation,
    GameObjectFrameSync::Vector targetPosition,
    GameObjectFrameSync::Quaternion targetRotation) noexcept
{
    auto result = GetFrameSync().OnNetworkPose(
        physicsPosition, graphPosition, graphRotation,
        targetPosition, targetRotation);
    if (GetFrameSync().HasActiveCorrection())
        SetSyncFrameEvent(true);
    return result;
}

void GameCar::SynchronizePhysicsState(
    GameObjectFrameSync::Pose pose,
    GameObjectFrameSync::Vector linearVelocity,
    bool awake,
    const std::vector<GameObjectFrameSync::Pose>& physicalWheels) noexcept
{
    GetFrameSync().OnPhysicsState(pose, linearVelocity, awake);
    SetBodyProgressEvent(GetFrameSync().IsBodyProgressEvent());
    const std::size_t count = std::min(
        wheels_.size(), physicalWheels.size());
    for (std::size_t index = 0U; index < count; ++index)
    {
        if (wheels_[index] != nullptr)
        {
            wheels_[index]->SynchronizePhysicsState(
                pose, physicalWheels[index], awake);
        }
    }
}

void GameCar::ConfigureMotor(MotorDescription description) noexcept
{
    SetMotorDesc(description);
    currentGear_ = -1;
    moveCar_ = MoveCarState::None;
}

void GameCar::SetMotorDesc(MotorDescription value) noexcept
{
    motor_ = value;
    motor_.brakeTorque = std::max(motor_.brakeTorque, 0.0F);
    motor_.differentialRatio = std::max(
        motor_.differentialRatio, 0.0F);
    motor_.maximumRpm = std::max(motor_.maximumRpm, 1.0F);
    motor_.idlingRpm = std::clamp(
        motor_.idlingRpm, 0.0F, motor_.maximumRpm);
    motor_.maximumTorque = std::max(motor_.maximumTorque, 0.0F);
    motor_.torqueEfficiency = std::max(
        motor_.torqueEfficiency, 0.0F);
    motor_.restBrakeTorque = std::max(
        motor_.restBrakeTorque, 0.0F);
    motor_.maximumSpeed = std::max(motor_.maximumSpeed, 0.0F);
    maximumSpeed_ = motor_.maximumSpeed;
    currentGear_ = std::min(currentGear_, 5);
}

void GameCar::ConfigureDynamics(
    DynamicsDescription description,
    const std::vector<WheelDynamics>& wheels) noexcept
{
    dynamics_ = description;
    dynamics_.clampRollAngle =
        std::max(dynamics_.clampRollAngle, 0.0F);
    dynamics_.clampPitchAngle =
        std::max(dynamics_.clampPitchAngle, 0.0F);
    dynamics_.maximumSteerAngle =
        std::max(dynamics_.maximumSteerAngle, 0.0F);
    dynamics_.steerSpeed = std::max(dynamics_.steerSpeed, 0.0F);
    steeringControl_ = dynamics_.steeringControl;
    clutchImmunity_ = dynamics_.clutchImmunity;
    tireSpring_ = dynamics_.tireSpring;
    disableColor_ = dynamics_.disableColor;
    steeringAngle_ = 0.0F;
    const std::size_t count = std::min(wheels_.size(), wheels.size());
    for (std::size_t index = 0U; index < count; ++index)
    {
        if (wheels_[index] == nullptr)
            continue;
        wheels_[index]->ConfigureDynamics(
            wheels[index].positionX, wheels[index].driven,
            wheels[index].steering, wheels[index].inverted,
            wheels[index].radius, wheels[index].visualOffset);
    }
}

GameCar::DriveCommand GameCar::OnFixedStepDrive(
    float deltaTime, FixedStepInput input,
    FixedStepState state) noexcept
{
    constexpr std::array<float, 6U> gearRatios{
        1.5F, 2.66F, 1.78F, 1.30F, 1.00F, 0.74F};
    constexpr float radiansPerRevolution =
        6.28318530717958647692F;
    constexpr float directionDeadZone = 0.1F;
    deltaTime = std::max(deltaTime, 0.0F);
    SynchronizeSpeed(state.signedSpeed);
    // GameCar::OnFixedStep resets these flags before WheelsProgress and
    // OnContact fills bodyContact during the solver step. The backend gives
    // us the previous completed solver state at this call boundary; the
    // outer session refreshes it again after the current step completes.
    anyWheelContact_ = state.anyWheelContact;
    wheelsContact_ = state.allWheelContact;
    bodyContact_ = state.bodyContact;
    DriveCommand command;
    if (clutchTime_ > 0.0F)
    {
        clutchTime_ -= deltaTime;
        if (clutchTime_ < 0.0F)
            clutchTime_ = 0.0F;
        command.clutchReleased = clutchTime_ == 0.0F;
    }
    if (mineTime_ > 0.0F)
    {
        mineTime_ -= deltaTime;
        if (mineTime_ < 0.0F)
            mineTime_ = 0.0F;
        command.mineReleased = mineTime_ == 0.0F;
    }
    auto calcRpm = [&](int gear) noexcept {
        if (gear < 0)
            return motor_.idlingRpm;
        const auto ratio = gearRatios[static_cast<std::size_t>(
            std::clamp(gear, 0,
                       static_cast<int>(gearRatios.size() - 1U)))];
        return std::min(
            std::abs(state.drivenWheelAngularSpeed) * ratio *
                motor_.differentialRatio * 60.0F /
                radiansPerRevolution,
            motor_.maximumRpm);
    };
    auto calcTorque = [&](int gear) noexcept {
        const auto ratio = gearRatios[static_cast<std::size_t>(
            std::clamp(gear, 0,
                       static_cast<int>(gearRatios.size() - 1U)))];
        return motor_.maximumTorque * ratio *
               motor_.differentialRatio * motor_.torqueEfficiency;
    };

    input.throttle = std::clamp(input.throttle, 0.0F, 1.0F);
    input.reverse = std::clamp(input.reverse, 0.0F, 1.0F);
    input.brake = std::clamp(input.brake, 0.0F, 1.0F);
    input.steering = std::clamp(input.steering, -1.0F, 1.0F);
    if (input.brake > 0.0001F)
        SetMoveCar(MoveCarState::Brake);
    else if (input.reverse > 0.0001F)
        SetMoveCar(MoveCarState::Back);
    else if (input.throttle > 0.0001F)
        SetMoveCar(MoveCarState::Accel);
    else
        SetMoveCar(MoveCarState::None);

    command.brakeTorque = motor_.restBrakeTorque;
    switch (moveCar_)
    {
    case MoveCarState::None:
        command.rpm = calcRpm(currentGear_);
        break;
    case MoveCarState::Brake:
        SetCurGear(-1);
        command.rpm = calcRpm(currentGear_);
        command.brakeTorque = motor_.brakeTorque * input.brake;
        break;
    case MoveCarState::Back:
        if (state.signedSpeed > directionDeadZone)
        {
            command.rpm = calcRpm(currentGear_);
            command.brakeTorque = motor_.brakeTorque;
        }
        else
        {
            SetCurGear(0);
            command.rpm = calcRpm(currentGear_);
            if (command.rpm < motor_.maximumRpm)
            {
                command.motorTorque =
                    -calcTorque(currentGear_) * input.reverse;
            }
        }
        break;
    case MoveCarState::Accel:
        if (state.signedSpeed < -directionDeadZone)
        {
            SetCurGear(-1);
            command.rpm = calcRpm(currentGear_);
            command.brakeTorque = motor_.brakeTorque;
        }
        else
        {
            if (currentGear_ <= 0)
                SetCurGear(1);
            command.rpm = calcRpm(currentGear_);
            command.motorTorque = calcTorque(currentGear_) *
                input.throttle * motorTorqueK_;
        }
        break;
    }

    // Source TransmissionProgress runs after MotorProgress, so the torque
    // and RPM above still belong to the gear used for this fixed step.
    if (state.drivenWheelContact && motor_.automaticGears &&
        currentGear_ > 0)
    {
        if (command.rpm < motor_.maximumRpm / 1.8F &&
            currentGear_ > 1)
            GearDown();
        if (command.rpm >= motor_.maximumRpm &&
            currentGear_ < static_cast<int>(gearRatios.size() - 1U))
            GearUp();
    }
    if (maximumSpeed_ > 0.0F &&
        state.absoluteSpeed > maximumSpeed_)
        command.motorTorque = command.brakeTorque;
    command.gear = currentGear_;
    command.lateralGripScale = wheelSteerK_;

    const float targetSteering =
        input.steering * dynamics_.maximumSteerAngle;
    if (std::abs(input.steering) <= 0.0001F)
        SetSteerWheel(SteerWheelState::None);
    else if (input.manualSteering)
        SetSteerWheel(SteerWheelState::Manual);
    else if (input.steering > 0.0F)
        SetSteerWheel(SteerWheelState::OnLeft);
    else
        SetSteerWheel(SteerWheelState::OnRight);

    if (steerWheel_ == SteerWheelState::Manual)
    {
        SetSteerWheelAngle(targetSteering);
    }
    else if (steerWheel_ == SteerWheelState::OnLeft)
    {
        SetSteerWheelAngle(dynamics_.steerSpeed > 0.0F
            ? std::min(
                  std::max(steeringAngle_, 0.0F) +
                      dynamics_.steerSpeed * deltaTime,
                  dynamics_.maximumSteerAngle)
            : dynamics_.maximumSteerAngle);
    }
    else if (steerWheel_ == SteerWheelState::OnRight)
    {
        SetSteerWheelAngle(dynamics_.steerSpeed > 0.0F
            ? std::max(
                  std::min(steeringAngle_, 0.0F) -
                      dynamics_.steerSpeed * deltaTime,
                  -dynamics_.maximumSteerAngle)
            : -dynamics_.maximumSteerAngle);
    }
    else
        SetSteerWheelAngle(0.0F);
    command.steeringAngle = steeringAngle_;
    command.rearWheelX = 0.0F;
    for (auto& wheel : wheels_)
    {
        if (wheel == nullptr)
            continue;
        command.rearWheelX = std::min(
            command.rearWheelX, wheel->GetPositionX());
        if (wheel->IsSteering())
            wheel->SetSteerAngle(steeringAngle_);
    }
    const bool steeringContact = dynamics_.gravitySteering
        ? state.anyWheelContact : state.drivenWheelContact;
    if (steeringContact && !IsClutchLocked() &&
        std::abs(steeringAngle_) > 0.0001F &&
        dynamics_.maximumSteerAngle > 0.0001F)
    {
        const float alpha = std::clamp(
            state.signedSpeed / 10.0F, -1.0F, 1.0F);
        command.steeringYaw =
            alpha * (steeringAngle_ / dynamics_.maximumSteerAngle) *
            dynamics_.steerRotation * deltaTime;
    }

    command.angularDamping = dynamics_.angularDamping;
    if (IsClutchLocked())
        command.angularDamping[2U] = 1.0F;
    command.clampRollAngle = dynamics_.clampRollAngle;
    command.clampPitchAngle = dynamics_.clampPitchAngle;
    if (springTime_ > 0.0F)
    {
        springTime_ = std::max(springTime_ - deltaTime, 0.0F);
        command.springReleased = springTime_ == 0.0F;
    }
    command.applyExtraGravity = !state.anyWheelContact;
    if (command.applyExtraGravity && state.horizontalSpeed > 1.0F &&
        !IsSpringLocked())
    {
        command.airbornePitchAcceleration =
            dynamics_.airbornePitchAcceleration;
    }
    GetBehaviors().OnMotor(
        deltaTime, command.rpm,
        motor_.idlingRpm, motor_.maximumRpm);
    return command;
}

GameCar::DriveCommand GameCar::DispatchFixedStepDrive(
    WorldEventPump& world, float deltaTime,
    FixedStepInput input, FixedStepState state) noexcept
{
    pendingFixedStepInput_ = input;
    pendingFixedStepState_ = state;
    pendingFixedStepCommand_ = {};
    fixedStepPending_ = true;
    fixedStepDispatched_ = false;
    world.DispatchFixedStepEvent(this, deltaTime);
    fixedStepPending_ = false;
    return fixedStepDispatched_ ? pendingFixedStepCommand_ : DriveCommand{};
}

void GameCar::OnFixedStep(float deltaTime) noexcept
{
    if (!fixedStepPending_)
        return;
    pendingFixedStepCommand_ = OnFixedStepDrive(
        deltaTime, pendingFixedStepInput_, pendingFixedStepState_);
    fixedStepDispatched_ = true;
}

void GameCar::OnFrame(float deltaTime, float physicsAlpha) noexcept
{
    if (!pxSyncPending_ || pendingPhysicalWheels_ == nullptr)
        return;
    lastPxSyncState_ = OnPxSync(
        pendingPhysicalBody_, *pendingPhysicalWheels_,
        deltaTime, physicsAlpha);
    pxSyncInitialized_ = true;
}

int GameCar::GearUp() noexcept
{
    if (currentGear_ < 5)
        ++currentGear_;
    return currentGear_;
}

int GameCar::GearDown() noexcept
{
    if (currentGear_ > -1)
        --currentGear_;
    return currentGear_;
}

const GameCar::MotorDescription& GameCar::GetMotorDesc() const noexcept
{
    return motor_;
}

GameCar::MoveCarState GameCar::GetMoveCar() const noexcept
{
    return moveCar_;
}

void GameCar::SetMoveCar(MoveCarState value) noexcept
{
    moveCar_ = value;
}

GameCar::SteerWheelState GameCar::GetSteerWheel() const noexcept
{
    return steerWheel_;
}

void GameCar::SetSteerWheel(SteerWheelState value) noexcept
{
    steerWheel_ = value;
}

float GameCar::GetSteerWheelAngle() const noexcept
{
    return steeringAngle_;
}

void GameCar::SetSteerWheelAngle(float value) noexcept
{
    steeringAngle_ = std::clamp(
        value, -dynamics_.maximumSteerAngle,
        dynamics_.maximumSteerAngle);
}

int GameCar::GetCurGear() const noexcept
{
    return currentGear_;
}

void GameCar::SetCurGear(int value) noexcept
{
    currentGear_ = std::clamp(value, -1, 5);
}

float GameCar::GetKSteerControl() const noexcept
{
    return steeringControl_;
}

void GameCar::SetKSteerControl(float value) noexcept
{
    steeringControl_ = value;
    dynamics_.steeringControl = value;
}

float GameCar::GetSteerSpeed() const noexcept
{
    return dynamics_.steerSpeed;
}

void GameCar::SetSteerSpeed(float value) noexcept
{
    dynamics_.steerSpeed = value;
}

float GameCar::GetSteerRot() const noexcept
{
    return dynamics_.steerRotation;
}

void GameCar::SetSteerRot(float value) noexcept
{
    dynamics_.steerRotation = value;
}

std::array<float, 3U> GameCar::GetAngDamping() const noexcept
{
    return dynamics_.angularDamping;
}

void GameCar::SetAngDamping(std::array<float, 3U> value) noexcept
{
    dynamics_.angularDamping = value;
}

float GameCar::GetFlyYTorque() const noexcept
{
    return dynamics_.airbornePitchAcceleration;
}

void GameCar::SetFlyYTourque(float value) noexcept
{
    dynamics_.airbornePitchAcceleration = value;
}

float GameCar::GetClampXTorque() const noexcept
{
    return dynamics_.clampRollAngle;
}

void GameCar::SetClampXTourque(float value) noexcept
{
    dynamics_.clampRollAngle = value;
}

float GameCar::GetClampYTorque() const noexcept
{
    return dynamics_.clampPitchAngle;
}

void GameCar::SetClampYTourque(float value) noexcept
{
    dynamics_.clampPitchAngle = value;
}

float GameCar::GetMotorTorqueK() const noexcept
{
    return motorTorqueK_;
}

void GameCar::SetMotorTorqueK(float value) noexcept
{
    motorTorqueK_ = value;
}

float GameCar::GetWheelSteerK() const noexcept
{
    return wheelSteerK_;
}

void GameCar::SetWheelSteerK(float value) noexcept
{
    if (wheelSteerK_ == value)
        return;
    // WheelShape::GetLateralTireForceFunction returns its serialized base
    // descriptor in the Windows engine; ApplyWheelSteerK only changes the
    // live PhysX descriptor. Keeping an absolute scale here reproduces that
    // behavior without compounding the multiplier between frames.
    wheelSteerK_ = value;
}

void GameCar::SynchronizeSpeed(float signedSpeed) noexcept
{
    signedSpeed_ = std::abs(signedSpeed) < 1.0F
                       ? 0.0F
                       : signedSpeed;
}

float GameCar::GetSpeed() const noexcept
{
    return signedSpeed_;
}

float GameCar::GetLeadWheelSpeed() const noexcept
{
    const auto found = std::find_if(
        wheels_.begin(), wheels_.end(),
        [](const auto& wheel) {
            return wheel != nullptr && wheel->IsDriven();
        });
    if (found == wheels_.end())
        return 0.0F;
    const float speed =
        (*found)->GetAxleSpeed() * (*found)->GetRadius();
    return std::abs(speed) > 0.1F ? speed : 0.0F;
}

float GameCar::GetDrivenWheelSpeed() const noexcept
{
    // Preserve the source's historical name: this deliberately reads the
    // first wheel outside GetLeadGroup, i.e. the first non-driven wheel.
    const auto found = std::find_if(
        wheels_.begin(), wheels_.end(),
        [](const auto& wheel) {
            return wheel != nullptr && !wheel->IsDriven();
        });
    if (found == wheels_.end())
        return 0.0F;
    const float speed =
        (*found)->GetAxleSpeed() * (*found)->GetRadius();
    return std::abs(speed) > 0.1F ? speed : 0.0F;
}

float GameCar::GetRPM() const noexcept
{
    constexpr std::array<float, 6U> gearRatios{
        1.5F, 2.66F, 1.78F, 1.30F, 1.00F, 0.74F};
    constexpr float radiansPerRevolution =
        6.28318530717958647692F;
    if (currentGear_ < 0)
        return motor_.idlingRpm;
    const auto found = std::find_if(
        wheels_.begin(), wheels_.end(),
        [](const auto& wheel) {
            return wheel != nullptr && wheel->IsDriven();
        });
    if (found == wheels_.end())
        return motor_.idlingRpm;
    const float ratio = gearRatios[static_cast<std::size_t>(
        std::clamp(currentGear_, 0,
                   static_cast<int>(gearRatios.size() - 1U)))];
    const float rpm =
        std::abs((*found)->GetAxleSpeed()) * ratio *
        motor_.differentialRatio * 60.0F / radiansPerRevolution;
    return std::min(rpm, motor_.maximumRpm);
}

bool GameCar::IsGravEngine() const noexcept
{
    return dynamics_.gravitySteering;
}

void GameCar::SetGravEngine(bool value) noexcept
{
    dynamics_.gravitySteering = value;
}

bool GameCar::IsClutchImmunity() const noexcept
{
    return clutchImmunity_;
}

void GameCar::SetClutchImmunity(bool value) noexcept
{
    clutchImmunity_ = value;
    dynamics_.clutchImmunity = value;
}

float GameCar::GetMaxSpeed() const noexcept
{
    return maximumSpeed_;
}

void GameCar::SetMaxSpeed(float value) noexcept
{
    maximumSpeed_ = value;
    motor_.maximumSpeed = value;
}

float GameCar::GetTireSpring() const noexcept
{
    return tireSpring_;
}

void GameCar::SetTireSpring(float value) noexcept
{
    tireSpring_ = value;
    dynamics_.tireSpring = value;
}

bool GameCar::GetDisableColor() const noexcept
{
    return disableColor_;
}

void GameCar::SetDisableColor(bool value) noexcept
{
    disableColor_ = value;
    dynamics_.disableColor = value;
}

void GameCar::BindSoundMotor(
    const std::array<float, 2>& rpmVolumeRange,
    const std::array<float, 2>& rpmFrequencyRange)
{
    ReleaseSoundMotor();
    rpmVolumeRange_ = rpmVolumeRange;
    rpmFrequencyRange_ = rpmFrequencyRange;
    soundMotor_ = &static_cast<SoundMotor&>(
        GetBehaviors().Add(BehaviorType::SoundMotor));
}

void GameCar::ReleaseSoundMotor() noexcept
{
    if (auto* behavior =
            GetBehaviors().Find(BehaviorType::SoundMotor))
        GetBehaviors().Delete(behavior);
    soundMotor_ = nullptr;
    soundMotorMix_ = {};
}

SoundMotorMix GameCar::OnMotor(
    float deltaTime, float rpm, float minimumRpm,
    float maximumRpm) noexcept
{
    GetBehaviors().OnMotor(
        deltaTime, rpm, minimumRpm, maximumRpm);
    return soundMotorMix_;
}

const SoundMotorMix& GameCar::GetSoundMotorMix() const noexcept
{
    return soundMotorMix_;
}

bool GameCar::HasSoundMotor() const noexcept
{
    return soundMotor_ != nullptr &&
           GetBehaviors().Find(BehaviorType::SoundMotor) != nullptr;
}

void GameCar::BindWheels(
    const std::vector<bool>& slipEffects,
    const std::vector<bool>& slipSounds)
{
    ReleaseWheels();
    const std::size_t count = std::max(
        slipEffects.size(), slipSounds.size());
    wheels_.reserve(count);
    for (std::size_t index = 0U; index < count; ++index)
    {
        auto wheel = std::make_unique<CarWheel>(
            index < slipEffects.size() && slipEffects[index],
            index < slipSounds.size() && slipSounds[index]);
        wheel->SetParent(this);
        wheels_.push_back(std::move(wheel));
    }
}

void GameCar::BindWheels(
    const std::vector<std::vector<WheelSlipEffectDefinition>>&
        slipBehaviors,
    const std::vector<bool>& enabled,
    const ObjectDefinition* trailDefinition,
    const ObjectDefinition* smokeDefinition)
{
    ReleaseWheels();
    const std::size_t count = std::max(
        slipBehaviors.size(), enabled.size());
    wheels_.reserve(count);
    for (std::size_t index = 0U; index < count; ++index)
    {
        static const std::vector<WheelSlipEffectDefinition> empty;
        const auto& definitions =
            index < slipBehaviors.size()
                ? slipBehaviors[index]
                : empty;
        auto wheel = std::make_unique<CarWheel>(
            definitions,
            index < enabled.size() && enabled[index],
            trailDefinition, smokeDefinition);
        wheel->SetParent(this);
        wheels_.push_back(std::move(wheel));
    }
}

void GameCar::ReleaseWheels() noexcept
{
    for (auto& wheel : wheels_)
        if (wheel != nullptr)
            wheel->SetParent(nullptr);
    wheels_.clear();
}

bool GameCar::SetWheelContact(
    std::size_t wheel, bool hasContact,
    float longitudinalSlip, float lateralSlip,
    float normalReaction, float normalImpulse,
    std::array<float, 3U> contactPosition) noexcept
{
    auto* target = GetWheel(wheel);
    if (target == nullptr)
        return false;
    target->SetContact(
        hasContact, longitudinalSlip, lateralSlip,
        normalReaction, normalImpulse, contactPosition);
    return true;
}

void GameCar::UpdateContactState(bool bodyContact) noexcept
{
    anyWheelContact_ = false;
    wheelsContact_ = true;
    for (const auto& wheel : wheels_)
    {
        const bool contact = wheel != nullptr && wheel->HasContact();
        anyWheelContact_ = anyWheelContact_ || contact;
        if (!contact)
            wheelsContact_ = false;
    }
    bodyContact_ = bodyContact;
}

bool GameCar::IsAnyWheelContact() const noexcept
{
    return anyWheelContact_;
}

bool GameCar::IsWheelsContact() const noexcept
{
    return wheelsContact_;
}

bool GameCar::IsBodyContact() const noexcept
{
    return bodyContact_;
}

WheelSlipProgress GameCar::GetWheelSlipResult(
    std::size_t wheel) const noexcept
{
    const auto* target = GetWheel(wheel);
    return target != nullptr ? target->GetSlipResult()
                             : WheelSlipProgress{};
}

const std::vector<WheelSlipProgress>&
GameCar::GetWheelSlipResults(std::size_t wheel) const noexcept
{
    static const std::vector<WheelSlipProgress> empty;
    const auto* target = GetWheel(wheel);
    return target != nullptr ? target->GetSlipResults() : empty;
}

std::size_t GameCar::GetWheelCount() const noexcept
{
    return wheels_.size();
}

CarWheel* GameCar::GetWheel(std::size_t wheel) noexcept
{
    return wheel < wheels_.size() ? wheels_[wheel].get() : nullptr;
}

const CarWheel* GameCar::GetWheel(std::size_t wheel) const noexcept
{
    return wheel < wheels_.size() ? wheels_[wheel].get() : nullptr;
}

std::size_t GameCar::GetLeadWheelCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        wheels_.begin(), wheels_.end(),
        [](const auto& wheel) {
            return wheel != nullptr && wheel->GetLead();
        }));
}

CarWheel* GameCar::GetLeadWheel(std::size_t wheel) noexcept
{
    const auto found = std::find_if(
        wheels_.begin(), wheels_.end(),
        [&wheel](const auto& candidate) {
            if (candidate == nullptr || !candidate->GetLead())
                return false;
            if (wheel == 0U)
                return true;
            --wheel;
            return false;
        });
    return found != wheels_.end() ? found->get() : nullptr;
}

const CarWheel* GameCar::GetLeadWheel(std::size_t wheel) const noexcept
{
    const auto found = std::find_if(
        wheels_.begin(), wheels_.end(),
        [&wheel](const auto& candidate) {
            if (candidate == nullptr || !candidate->GetLead())
                return false;
            if (wheel == 0U)
                return true;
            --wheel;
            return false;
        });
    return found != wheels_.end() ? found->get() : nullptr;
}

std::size_t GameCar::GetSteerWheelCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        wheels_.begin(), wheels_.end(),
        [](const auto& wheel) {
            return wheel != nullptr && wheel->GetSteer();
        }));
}

CarWheel* GameCar::GetSteerGroupWheel(std::size_t wheel) noexcept
{
    const auto found = std::find_if(
        wheels_.begin(), wheels_.end(),
        [&wheel](const auto& candidate) {
            if (candidate == nullptr || !candidate->GetSteer())
                return false;
            if (wheel == 0U)
                return true;
            --wheel;
            return false;
        });
    return found != wheels_.end() ? found->get() : nullptr;
}

const CarWheel* GameCar::GetSteerGroupWheel(
    std::size_t wheel) const noexcept
{
    const auto found = std::find_if(
        wheels_.begin(), wheels_.end(),
        [&wheel](const auto& candidate) {
            if (candidate == nullptr || !candidate->GetSteer())
                return false;
            if (wheel == 0U)
                return true;
            --wheel;
            return false;
        });
    return found != wheels_.end() ? found->get() : nullptr;
}

void GameCar::BindAnimationChildren(
    bool trackAnimation, std::size_t cushionAnimations)
{
    BindAnimationChildren(
        trackAnimation, std::vector<int>(cushionAnimations, 0));
}

void GameCar::BindAnimationChildren(
    bool trackAnimation, const std::vector<int>& cushionTargetTags)
{
    ReleaseAnimationChildren();
    if (trackAnimation)
    {
        auto child = std::make_unique<CarAnimationChild>(true, 0U);
        child->SetParent(this);
        animationChildren_.push_back(std::move(child));
    }
    if (!cushionTargetTags.empty())
    {
        auto child = std::make_unique<CarAnimationChild>(
            false, cushionTargetTags);
        child->SetParent(this);
        animationChildren_.push_back(std::move(child));
    }
}

void GameCar::ReleaseAnimationChildren() noexcept
{
    for (auto& child : animationChildren_)
        if (child != nullptr)
            child->SetParent(nullptr);
    animationChildren_.clear();
}

float GameCar::GetTrackTextureOffset() const noexcept
{
    const auto found = std::find_if(
        animationChildren_.begin(), animationChildren_.end(),
        [](const auto& child) {
            return child != nullptr && child->HasTrackAnimation();
        });
    return found != animationChildren_.end()
               ? (*found)->GetTrackTextureOffset()
               : 1.0F;
}

float GameCar::GetCushionAngle(std::size_t index) const noexcept
{
    for (const auto& child : animationChildren_)
    {
        if (child != nullptr &&
            index < child->GetCushionAnimationCount())
        {
            return child->GetCushionAngle(index);
        }
    }
    return 0.0F;
}

float GameCar::GetCushionAngleForTag(int targetTag) const noexcept
{
    for (const auto& child : animationChildren_)
    {
        if (child == nullptr)
            continue;
        for (std::size_t index = 0U;
             index < child->GetCushionAnimationCount(); ++index)
        {
            if (child->GetCushionTargetTag(index) == targetTag)
                return child->GetCushionAngleForTag(targetTag);
        }
    }
    return 0.0F;
}

std::size_t GameCar::GetAnimationChildCount() const noexcept
{
    return animationChildren_.size();
}

CarAnimationChild* GameCar::GetAnimationChild(
    std::size_t index) noexcept
{
    return index < animationChildren_.size()
               ? animationChildren_[index].get() : nullptr;
}

const CarAnimationChild* GameCar::GetAnimationChild(
    std::size_t index) const noexcept
{
    return index < animationChildren_.size()
               ? animationChildren_[index].get() : nullptr;
}

bool GameCar::LockClutch(float strength) noexcept
{
    if (clutchImmunity_ || clutchTime_ > 0.0F)
        return false;
    clutchTime_ = clutchLockSeconds;
    clutchStrength_ = strength;
    return true;
}

void GameCar::CancelClutch() noexcept
{
    clutchTime_ = 0.0F;
}

bool GameCar::IsClutchLocked() const noexcept
{
    return clutchTime_ > 0.0F;
}

float GameCar::GetClutchTime() const noexcept
{
    return clutchTime_;
}

float GameCar::ConsumeClutchStrength() noexcept
{
    const float result = clutchStrength_;
    clutchStrength_ = 0.0F;
    return result;
}

void GameCar::LockSpring() noexcept
{
    springTime_ = springLockSeconds;
}

bool GameCar::IsSpringLocked() const noexcept
{
    return springTime_ > 0.0F;
}

float GameCar::GetSpringTime() const noexcept
{
    return springTime_;
}

void GameCar::LockMine(float time) noexcept
{
    mineTime_ = time;
}

bool GameCar::IsMineLocked() const noexcept
{
    return mineTime_ > 0.0F;
}

float GameCar::GetMineTime() const noexcept
{
    return mineTime_;
}

SoundMotor::SoundMotor() noexcept : Behavior(nullptr) {}

SoundMotor::SoundMotor(Behaviors* owner) noexcept
    : Behavior(owner)
{
}

SoundMotor::SoundMotor(
    Behaviors* owner, GameCar* car) noexcept
    : Behavior(owner), car_(car)
{
}

SoundMotor::SoundMotor(const SoundMotor& other) noexcept
    : Behavior(nullptr), currentRpm_(other.currentRpm_)
{
}

SoundMotor& SoundMotor::operator=(
    const SoundMotor& other) noexcept
{
    if (this != &other)
        currentRpm_ = other.currentRpm_;
    return *this;
}

void SoundMotor::OnProgress(float) noexcept
{
    // Windows SoundMotor::OnProgress updates the two Source3d positions.
    // SDL performs that backend operation from the owning car pose.
}

void SoundMotor::OnMotor(
    float deltaTime, float rpm,
    float minimumRpm, float maximumRpm) noexcept
{
    auto* car = car_ != nullptr
        ? car_
        : dynamic_cast<GameCar*>(GetGameObj());
    if (car == nullptr)
        return;
    car->soundMotorMix_ = OnMotor(
        deltaTime, rpm, minimumRpm, maximumRpm,
        car->rpmVolumeRange_, car->rpmFrequencyRange_);
}

void SoundMotor::Reset() noexcept
{
    currentRpm_ = 0.0F;
}

SoundMotor::Mix SoundMotor::OnMotor(
    float deltaTime, float rpm, float minimumRpm, float maximumRpm,
    const std::array<float, 2>& rpmVolumeRange,
    const std::array<float, 2>& rpmFrequencyRange) noexcept
{
    const float distanceRpm = rpm - currentRpm_;
    const float rpmDelta = motorLag * deltaTime *
        (distanceRpm > 0.0F ? 1.0F : -1.0F);
    currentRpm_ += std::clamp(
        rpmDelta, -std::abs(distanceRpm), std::abs(distanceRpm));

    minimumRpm = std::max(minimumRpm, 1.0F);
    maximumRpm = std::max(maximumRpm, minimumRpm + 1.0F);
    const float idleAlpha = std::clamp(
        0.5F * (currentRpm_ - minimumRpm) / minimumRpm,
        0.0F, 1.0F);
    const float rpmAlpha = std::clamp(
        (currentRpm_ - minimumRpm) /
            (maximumRpm - minimumRpm),
        0.0F, 1.0F);

    Mix result;
    result.currentRpm = currentRpm_;
    result.idleVolume = 1.0F - idleAlpha;
    result.rpmVolume =
        (rpmVolumeRange[0] + rpmAlpha *
            (rpmVolumeRange[1] - rpmVolumeRange[0])) *
        idleAlpha;
    result.rpmFrequencyRatio =
        rpmFrequencyRange[0] + rpmAlpha *
            (rpmFrequencyRange[1] - rpmFrequencyRange[0]);
    return result;
}

float SoundMotor::GetCurrentRpm() const noexcept
{
    return currentRpm_;
}

PxWheelSlipEffect::PxWheelSlipEffect() noexcept : EventEffect() {}

PxWheelSlipEffect::PxWheelSlipEffect(Behaviors* owner) noexcept
    : EventEffect(owner)
{
}

PxWheelSlipEffect::PxWheelSlipEffect(
    Behaviors* owner, CarWheel* wheel,
    std::size_t effect) noexcept
    : EventEffect(owner), wheel_(wheel), effect_(effect)
{
}

void PxWheelSlipEffect::BindWheel(
    CarWheel* wheel, std::size_t effect) noexcept
{
    wheel_ = wheel;
    effect_ = effect;
}

PxWheelSlipEffect::PxWheelSlipEffect(
    const PxWheelSlipEffect& other)
    : EventEffect(other)
{
}

PxWheelSlipEffect& PxWheelSlipEffect::operator=(
    const PxWheelSlipEffect& other)
{
    EventEffect::operator=(other);
    return *this;
}

void PxWheelSlipEffect::OnProgress(float deltaTime) noexcept
{
    EventEffect::OnProgress(deltaTime);
    if (wheel_ == nullptr || effect_ >= wheel_->slipResults_.size())
        return;
    // eff9338 GameBase.cpp keeps base EventEffect progress in both builds,
    // but gates the entire slip visual/sound branch with #if !_DEBUG.
    // Only the explicit comparison mode mirrors that compile-time guard.
    if (wheel_->legacyWindowsDebug_)
        return;
    wheel_->slipResults_[effect_] = OnProgress(
        wheel_->hasContact_, wheel_->longitudinalSlip_,
        wheel_->lateralSlip_, wheel_->contactPosition_);
}

void PxWheelSlipEffect::Reset() noexcept
{
    EventEffect::Reset();
}

void PxWheelSlipEffect::Configure(
    const ObjectDefinition* definition,
    const std::vector<std::string>& soundPaths,
    std::array<float, 3U> position,
    std::array<float, 3U> impulse,
    bool ignoreRotation) noexcept
{
    EventEffect::Configure(
        definition, position, impulse, ignoreRotation);
    EventEffect::ConfigureSounds(soundPaths);
}

float PxWheelSlipEffect::SourceSlip(
    bool hasContact, float longitudinalSlip,
    float lateralSlip) noexcept
{
    if (!hasContact)
        return 0.0F;
    return std::max(
               std::abs(lateralSlip) - lateralThreshold, 0.0F) +
           std::max(
               std::abs(longitudinalSlip) - longitudinalThreshold,
               0.0F);
}

PxWheelSlipEffect::ProgressResult PxWheelSlipEffect::OnProgress(
    bool hasContact, float longitudinalSlip, float lateralSlip,
    std::array<float, 3U> contactPosition,
    bool hasSound) noexcept
{
    ProgressResult result;
    result.owner = this;
    const auto* definition = EventEffect::GetEffectDefinition();
    result.definition = definition;
    result.position = EventEffect::GetPosition();
    result.worldPosition = {
        contactPosition[0U] + result.position[0U],
        contactPosition[1U] + result.position[1U],
        contactPosition[2U] + result.position[2U]};
    result.impulse = EventEffect::GetImpulse();
    result.ignoreRotation = EventEffect::GetIgnoreRotation();
    if (hasSound && !EventEffect::GetSoundPaths().empty())
        result.soundPath = &EventEffect::GetSoundPaths().front();
    result.slip = SourceSlip(
        hasContact, longitudinalSlip, lateralSlip);
    result.volume = std::clamp(result.slip * volumeScale, 0.0F, 1.0F);
    result.active = result.slip > 0.0F;
    if (result.active)
    {
        result.makeEffect = EventEffect::MakeEffect();
        result.effectId = EventEffect::GetMakeEffectId();
        result.playSound = result.soundPath != nullptr;
    }
    else
    {
        result.effectId = EventEffect::GetMakeEffectId();
        result.freeEffect = EventEffect::FreeEffect(true);
        // PxWheelSlipEffect calls Source3d::Stop even when no visual actor
        // was active, provided this behavior owns a sound.
        result.stopSound = result.soundPath != nullptr;
    }
    return result;
}

bool PxWheelSlipEffect::IsEffectMaked() const noexcept
{
    return EventEffect::IsEffectMaked();
}

const ObjectDefinition*
PxWheelSlipEffect::GetEffectDefinition() const noexcept
{
    return EventEffect::GetEffectDefinition();
}

const std::vector<std::string>&
PxWheelSlipEffect::GetSoundPaths() const noexcept
{
    return EventEffect::GetSoundPaths();
}

CarWheel::CarWheel() = default;

CarWheel::CarWheel(bool slipEffect, bool slipSound)
{
    Configure(slipEffect, slipSound);
}

CarWheel::CarWheel(
    const std::vector<WheelSlipEffectDefinition>& slipEffects,
    bool enabled,
    const ObjectDefinition* trailDefinition,
    const ObjectDefinition* smokeDefinition)
{
    Configure(
        slipEffects, enabled, trailDefinition, smokeDefinition);
}

CarWheel::CarWheel(const CarWheel& other) : GameObject(other)
{
    *this = other;
}

CarWheel& CarWheel::operator=(const CarWheel& other) noexcept
{
    if (this == &other)
        return *this;
    GameObject::operator=(other);
    slipBehaviors_.clear();
    slipResults_ = other.slipResults_;
    longitudinalSlip_ = other.longitudinalSlip_;
    lateralSlip_ = other.lateralSlip_;
    normalReaction_ = other.normalReaction_;
    normalImpulse_ = other.normalImpulse_;
    contactPosition_ = other.contactPosition_;
    hasContact_ = other.hasContact_;
    legacyWindowsDebug_ = other.legacyWindowsDebug_;
    slipEffectEnabled_ = other.slipEffectEnabled_;
    slipSoundEnabled_ = other.slipSoundEnabled_;
    pxFrameSync_ = other.pxFrameSync_;
    pxSyncPose_ = other.pxSyncPose_;
    positionX_ = other.positionX_;
    radius_ = other.radius_;
    offset_ = other.offset_;
    steerAngle_ = other.steerAngle_;
    axleSpeed_ = other.axleSpeed_;
    summAngle_ = other.summAngle_;
    driven_ = other.driven_;
    steering_ = other.steering_;
    inverted_ = other.inverted_;
    slipBehaviors_.reserve(other.slipBehaviors_.size());
    for (std::size_t effect = 0U;
         effect < other.slipBehaviors_.size(); ++effect)
    {
        auto& behavior = static_cast<PxWheelSlipEffect&>(
            GetBehaviors().Add(BehaviorType::PxWheelSlipEffect));
        behavior.BindWheel(this, effect);
        if (other.slipBehaviors_[effect] != nullptr)
            behavior = *other.slipBehaviors_[effect];
        slipBehaviors_.push_back(&behavior);
        if (effect < slipResults_.size() &&
            slipResults_[effect].soundPath != nullptr &&
            !behavior.GetSoundPaths().empty())
        {
            slipResults_[effect].soundPath =
                &behavior.GetSoundPaths().front();
        }
        if (effect < slipResults_.size())
            slipResults_[effect].owner = &behavior;
    }
    return *this;
}

CarWheel::CarWheel(CarWheel&& other) : CarWheel(other) {}

CarWheel& CarWheel::operator=(CarWheel&& other) noexcept
{
    return *this = static_cast<const CarWheel&>(other);
}

CarWheel::~CarWheel()
{
    GetBehaviors().Clear();
    SetParent(nullptr);
}

void CarWheel::Configure(bool slipEffect, bool slipSound)
{
    GetBehaviors().Clear();
    slipBehaviors_.clear();
    slipResults_.clear();
    hasContact_ = false;
    longitudinalSlip_ = 0.0F;
    lateralSlip_ = 0.0F;
    normalReaction_ = 0.0F;
    normalImpulse_ = 0.0F;
    contactPosition_ = {};
    slipEffectEnabled_ = slipEffect;
    slipSoundEnabled_ = slipEffect && slipSound;
    pxSyncPose_ = {};
    ResetMotion();
    if (slipEffectEnabled_)
    {
        auto& behavior = static_cast<PxWheelSlipEffect&>(
            GetBehaviors().Add(BehaviorType::PxWheelSlipEffect));
        behavior.BindWheel(this, 0U);
        behavior.Configure(
            nullptr,
            slipSound ? std::vector<std::string>{"SkidAsphalt.ogg"}
                      : std::vector<std::string>{});
        slipBehaviors_.push_back(&behavior);
        slipResults_.resize(1U);
    }
}

void CarWheel::Configure(
    const std::vector<WheelSlipEffectDefinition>& slipEffects,
    bool enabled,
    const ObjectDefinition* trailDefinition,
    const ObjectDefinition* smokeDefinition)
{
    GetBehaviors().Clear();
    slipBehaviors_.clear();
    slipResults_.clear();
    hasContact_ = false;
    longitudinalSlip_ = 0.0F;
    lateralSlip_ = 0.0F;
    normalReaction_ = 0.0F;
    normalImpulse_ = 0.0F;
    slipEffectEnabled_ = enabled && !slipEffects.empty();
    slipSoundEnabled_ = false;
    pxSyncPose_ = {};
    ResetMotion();
    if (!slipEffectEnabled_)
        return;
    slipBehaviors_.reserve(slipEffects.size());
    slipResults_.resize(slipEffects.size());
    for (std::size_t effect = 0U;
         effect < slipEffects.size(); ++effect)
    {
        const auto& definition = slipEffects[effect];
        const ObjectDefinition* canonicalDefinition =
            &definition.visual;
        if (trailDefinition != nullptr &&
            trailDefinition->record == definition.visual.record)
        {
            canonicalDefinition = trailDefinition;
        }
        else if (smokeDefinition != nullptr &&
                 smokeDefinition->record == definition.visual.record)
        {
            canonicalDefinition = smokeDefinition;
        }
        auto& behavior = static_cast<PxWheelSlipEffect&>(
            GetBehaviors().Add(BehaviorType::PxWheelSlipEffect));
        behavior.BindWheel(this, effect);
        behavior.Configure(
            canonicalDefinition, definition.soundPaths,
            {definition.position.x, definition.position.y,
             definition.position.z},
            {definition.impulse.x, definition.impulse.y,
             definition.impulse.z},
            definition.ignoreRotation);
        slipSoundEnabled_ =
            slipSoundEnabled_ || !definition.soundPaths.empty();
        slipBehaviors_.push_back(&behavior);
    }
}

const GameObjectFrameSync::Pose& CarWheel::PxSyncWheel(
    GameObjectFrameSync::Pose physicalBody,
    GameObjectFrameSync::Pose graphBody,
    GameObjectFrameSync::Pose physicalWheel,
    float deltaTime, float physicsAlpha) noexcept
{
    const auto localPose = pxFrameSync_.OnFrame(
        SourceLocalPose(physicalBody, physicalWheel),
        deltaTime, physicsAlpha);
    const SyncVector rotated = rotateSync(
        localPose.position, graphBody.rotation);
    pxSyncPose_.position = {
        graphBody.position.x + rotated.x,
        graphBody.position.y + rotated.y,
        graphBody.position.z + rotated.z};
    const SyncVector graphOffset = rotateSync(
        {offset_[0U], offset_[1U], offset_[2U]},
        graphBody.rotation);
    pxSyncPose_.position.x += graphOffset.x;
    pxSyncPose_.position.y += graphOffset.y;
    pxSyncPose_.position.z += graphOffset.z;
    pxSyncPose_.rotation = multiplySync(
        graphBody.rotation, localPose.rotation);
    return pxSyncPose_;
}

GameObjectFrameSync::Pose CarWheel::SourceLocalPose(
    GameObjectFrameSync::Pose physicalBody,
    GameObjectFrameSync::Pose physicalWheel) const noexcept
{
    const SyncQuaternion physicalRotation =
        normalizedSync(physicalBody.rotation);
    const SyncQuaternion inversePhysicalRotation{
        -physicalRotation.x, -physicalRotation.y,
        -physicalRotation.z, physicalRotation.w};
    const SyncVector relative{
        physicalWheel.position.x - physicalBody.position.x,
        physicalWheel.position.y - physicalBody.position.y,
        physicalWheel.position.z - physicalBody.position.z};
    GameObjectFrameSync::Pose result;
    result.position = rotateSync(relative, inversePhysicalRotation);
    // Windows CarWheel::PxSyncWheel never copies the PhysX wheel shape
    // orientation into the graph. The source object builds its local graph
    // rotation from steering about Z and accumulated axle spin about Y.
    // Jolt remains authoritative only for the suspension centre position.
    const SyncQuaternion steering = axisAngleSync(
        {0.0F, 0.0F, 1.0F}, steerAngle_);
    const SyncQuaternion spin = axisAngleSync(
        {0.0F, 1.0F, 0.0F}, summAngle_);
    SyncQuaternion localRotation = multiplySync(steering, spin);
    if (inverted_)
    {
        localRotation = multiplySync(
            localRotation,
            axisAngleSync(
                {0.0F, 0.0F, 1.0F},
                3.14159265358979323846F));
    }
    result.rotation = localRotation;
    return result;
}

void CarWheel::SynchronizePhysicsState(
    GameObjectFrameSync::Pose physicalBody,
    GameObjectFrameSync::Pose physicalWheel,
    bool awake) noexcept
{
    pxFrameSync_.OnPhysicsState(
        SourceLocalPose(physicalBody, physicalWheel), {}, awake);
}

const GameObjectFrameSync::Pose& CarWheel::GetPxSyncPose() const noexcept
{
    return pxSyncPose_;
}

void CarWheel::ConfigureDynamics(
    float positionX, bool driven, bool steering,
    bool inverted, float radius,
    std::array<float, 3U> visualOffset) noexcept
{
    positionX_ = positionX;
    radius_ = std::max(radius, 0.0F);
    SetLead(driven);
    SetSteer(steering);
    SetInvertWheel(inverted);
    SetOffset(visualOffset);
    ResetMotion();
}

void CarWheel::SetSteerAngle(float value) noexcept
{
    steerAngle_ = value;
}

void CarWheel::SetAxleSpeed(float value) noexcept
{
    axleSpeed_ = value;
}

void CarWheel::ResetMotion() noexcept
{
    steerAngle_ = 0.0F;
    axleSpeed_ = 0.0F;
    summAngle_ = 0.0F;
    pxFrameSync_.Reset();
}

float CarWheel::GetSteerAngle() const noexcept
{
    return steerAngle_;
}

float CarWheel::GetAxleSpeed() const noexcept
{
    return axleSpeed_;
}

float CarWheel::GetSummAngle() const noexcept
{
    return summAngle_;
}

float CarWheel::GetPositionX() const noexcept
{
    return positionX_;
}

float CarWheel::GetRadius() const noexcept
{
    return radius_;
}

float CarWheel::GetLongSlip() const noexcept
{
    return longitudinalSlip_;
}

float CarWheel::GetLatSlip() const noexcept
{
    return lateralSlip_;
}

bool CarWheel::GetLead() const noexcept
{
    return driven_;
}

void CarWheel::SetLead(bool value) noexcept
{
    driven_ = value;
}

bool CarWheel::GetSteer() const noexcept
{
    return steering_;
}

void CarWheel::SetSteer(bool value) noexcept
{
    steering_ = value;
}

const std::array<float, 3U>& CarWheel::GetOffset() const noexcept
{
    return offset_;
}

void CarWheel::SetOffset(std::array<float, 3U> value) noexcept
{
    offset_ = value;
}

bool CarWheel::GetInvertWheel() const noexcept
{
    return inverted_;
}

void CarWheel::SetInvertWheel(bool value) noexcept
{
    inverted_ = value;
}

bool CarWheel::IsDriven() const noexcept
{
    return GetLead();
}

bool CarWheel::IsSteering() const noexcept
{
    return GetSteer();
}

void CarWheel::SetContact(
    bool hasContact, float longitudinalSlip,
    float lateralSlip, float normalReaction,
    float normalImpulse,
    std::array<float, 3U> contactPosition) noexcept
{
    hasContact_ = hasContact;
    longitudinalSlip_ = longitudinalSlip;
    lateralSlip_ = lateralSlip;
    normalReaction_ = hasContact ? normalReaction : 0.0F;
    normalImpulse_ = hasContact ? normalImpulse : 0.0F;
    contactPosition_ = hasContact
        ? contactPosition
        : std::array<float, 3U>{};
}

bool CarWheel::HasContact() const noexcept
{
    return hasContact_;
}

float CarWheel::GetNormalReaction() const noexcept
{
    return normalReaction_;
}

float CarWheel::GetNormalImpulse() const noexcept
{
    return normalImpulse_;
}

void CarWheel::SetLegacyWindowsDebug(bool value) noexcept
{
    legacyWindowsDebug_ = value;
}

GameObject::ProgressResult CarWheel::OnProgress(
    float deltaTime) noexcept
{
    std::fill(
        slipResults_.begin(), slipResults_.end(),
        WheelSlipProgress{});
    auto result = GameObject::OnProgress(deltaTime);
    summAngle_ += axleSpeed_ * std::max(deltaTime, 0.0F);
    return result;
}

const WheelSlipProgress& CarWheel::GetSlipResult() const noexcept
{
    static const WheelSlipProgress empty;
    return slipResults_.empty() ? empty : slipResults_.front();
}

const std::vector<WheelSlipProgress>&
CarWheel::GetSlipResults() const noexcept
{
    return slipResults_;
}

std::size_t CarWheel::GetSlipEffectCount() const noexcept
{
    return slipBehaviors_.size();
}

const ObjectDefinition* CarWheel::GetSlipEffectDefinition(
    std::size_t effect) const noexcept
{
    return effect < slipBehaviors_.size() &&
                   slipBehaviors_[effect] != nullptr
               ? slipBehaviors_[effect]->GetEffectDefinition()
               : nullptr;
}

bool CarWheel::HasSlipEffect() const noexcept
{
    return slipEffectEnabled_ &&
           GetBehaviors().Find(
               BehaviorType::PxWheelSlipEffect) != nullptr;
}

bool CarWheel::HasSlipSound() const noexcept
{
    return slipSoundEnabled_;
}

GusenizaAnim::GusenizaAnim() noexcept : Behavior(nullptr) {}

GusenizaAnim::GusenizaAnim(Behaviors* owner) noexcept
    : Behavior(owner)
{
}

GusenizaAnim::GusenizaAnim(
    Behaviors* owner, CarAnimationChild* child) noexcept
    : Behavior(owner), child_(child)
{
}

GusenizaAnim::GusenizaAnim(
    const GusenizaAnim& other) noexcept
    : Behavior(nullptr), xAnimationOffset_(other.xAnimationOffset_)
{
}

GusenizaAnim& GusenizaAnim::operator=(
    const GusenizaAnim& other) noexcept
{
    if (this != &other)
        xAnimationOffset_ = other.xAnimationOffset_;
    return *this;
}

void GusenizaAnim::OnProgress(float deltaTime) noexcept
{
    auto* child = child_ != nullptr
        ? child_
        : dynamic_cast<CarAnimationChild*>(GetGameObj());
    if (child == nullptr)
        return;
    const auto* car =
        dynamic_cast<const GameCar*>(child->GetParent());
    OnProgress(
        deltaTime, car != nullptr ? car->GetLeadWheelSpeed() : 0.0F);
}

void GusenizaAnim::Reset() noexcept
{
    xAnimationOffset_ = 0.0F;
}

float GusenizaAnim::OnProgress(
    float deltaTime, float leadWheelSpeed) noexcept
{
    xAnimationOffset_ -=
        leadWheelSpeed * deltaTime / trackLength;
    xAnimationOffset_ -= std::floor(xAnimationOffset_);
    return GetTextureOffset();
}

float GusenizaAnim::GetTextureOffset() const noexcept
{
    return 1.0F - xAnimationOffset_;
}

PodushkaAnim::PodushkaAnim() noexcept : Behavior(nullptr) {}

PodushkaAnim::PodushkaAnim(Behaviors* owner) noexcept
    : Behavior(owner)
{
}

PodushkaAnim::PodushkaAnim(
    Behaviors* owner, CarAnimationChild* child,
    int targetTag) noexcept
    : Behavior(owner), child_(child), targetTag_(targetTag)
{
}

PodushkaAnim::PodushkaAnim(
    const PodushkaAnim& other) noexcept
    : Behavior(nullptr), angle_(other.angle_),
      targetTag_(other.targetTag_)
{
}

PodushkaAnim& PodushkaAnim::operator=(
    const PodushkaAnim& other) noexcept
{
    if (this != &other)
    {
        angle_ = other.angle_;
        targetTag_ = other.targetTag_;
    }
    return *this;
}

void PodushkaAnim::OnProgress(float deltaTime) noexcept
{
    auto* child = child_ != nullptr
        ? child_
        : dynamic_cast<CarAnimationChild*>(GetGameObj());
    if (child == nullptr)
        return;
    const auto* car =
        dynamic_cast<const GameCar*>(child->GetParent());
    OnProgress(
        deltaTime, car != nullptr ? car->GetLeadWheelSpeed() : 0.0F);
}

void PodushkaAnim::Reset() noexcept
{
    angle_ = 0.0F;
}

void PodushkaAnim::SetTargetTag(int value) noexcept
{
    targetTag_ = value;
}

int PodushkaAnim::GetTargetTag() const noexcept
{
    return targetTag_;
}

float PodushkaAnim::OnProgress(
    float deltaTime, float leadWheelSpeed) noexcept
{
    if (std::abs(leadWheelSpeed) > minimumWheelSpeed)
    {
        constexpr float pi = 3.14159265358979323846F;
        angle_ += pi * deltaTime * leadWheelSpeed * 0.1F;
    }
    return angle_;
}

float PodushkaAnim::GetAngle() const noexcept
{
    return angle_;
}

CarAnimationChild::CarAnimationChild() = default;

CarAnimationChild::CarAnimationChild(
    bool trackAnimation, std::size_t cushionAnimations)
{
    Configure(trackAnimation, cushionAnimations);
}

CarAnimationChild::CarAnimationChild(
    bool trackAnimation, const std::vector<int>& cushionTargetTags)
{
    Configure(trackAnimation, cushionTargetTags);
}

CarAnimationChild::CarAnimationChild(
    const CarAnimationChild& other) : GameObject(other)
{
    *this = other;
}

CarAnimationChild& CarAnimationChild::operator=(
    const CarAnimationChild& other) noexcept
{
    if (this == &other)
        return *this;
    GameObject::operator=(other);
    std::vector<int> targetTags;
    targetTags.reserve(other.cushionBehaviors_.size());
    for (const auto* behavior : other.cushionBehaviors_)
    {
        targetTags.push_back(
            behavior != nullptr
                ? behavior->GetTargetTag() : 0);
    }
    BindBehaviors(other.trackBehavior_ != nullptr, targetTags);
    if (trackBehavior_ != nullptr && other.trackBehavior_ != nullptr)
        *trackBehavior_ = *other.trackBehavior_;
    for (std::size_t index = 0U;
         index < cushionBehaviors_.size() &&
         index < other.cushionBehaviors_.size(); ++index)
    {
        if (cushionBehaviors_[index] != nullptr &&
            other.cushionBehaviors_[index] != nullptr)
        {
            *cushionBehaviors_[index] =
                *other.cushionBehaviors_[index];
        }
    }
    return *this;
}

CarAnimationChild::CarAnimationChild(
    CarAnimationChild&& other) : CarAnimationChild(other) {}

CarAnimationChild& CarAnimationChild::operator=(
    CarAnimationChild&& other) noexcept
{
    return *this = static_cast<const CarAnimationChild&>(other);
}

CarAnimationChild::~CarAnimationChild()
{
    GetBehaviors().Clear();
    SetParent(nullptr);
}

void CarAnimationChild::BindBehaviors(
    bool trackAnimation, const std::vector<int>& cushionTargetTags)
{
    GetBehaviors().Clear();
    trackBehavior_ = nullptr;
    cushionBehaviors_.clear();
    if (trackAnimation)
    {
        trackBehavior_ = &static_cast<GusenizaAnim&>(
            GetBehaviors().Add(BehaviorType::GusenizaAnim));
    }
    cushionBehaviors_.reserve(cushionTargetTags.size());
    for (std::size_t index = 0U;
         index < cushionTargetTags.size(); ++index)
    {
        auto& behavior = static_cast<PodushkaAnim&>(
            GetBehaviors().Add(BehaviorType::PodushkaAnim));
        behavior.SetTargetTag(cushionTargetTags[index]);
        cushionBehaviors_.push_back(&behavior);
    }
}

void CarAnimationChild::Configure(
    bool trackAnimation, std::size_t cushionAnimations)
{
    Configure(
        trackAnimation, std::vector<int>(cushionAnimations, 0));
}

void CarAnimationChild::Configure(
    bool trackAnimation, const std::vector<int>& cushionTargetTags)
{
    BindBehaviors(trackAnimation, cushionTargetTags);
}

GameObject::ProgressResult CarAnimationChild::OnProgress(
    float deltaTime) noexcept
{
    return GameObject::OnProgress(deltaTime);
}

bool CarAnimationChild::HasTrackAnimation() const noexcept
{
    return trackBehavior_ != nullptr &&
           GetBehaviors().Find(BehaviorType::GusenizaAnim) != nullptr;
}

std::size_t CarAnimationChild::GetCushionAnimationCount() const noexcept
{
    return cushionBehaviors_.size();
}

float CarAnimationChild::GetTrackTextureOffset() const noexcept
{
    return trackBehavior_ != nullptr
               ? trackBehavior_->GetTextureOffset() : 1.0F;
}

float CarAnimationChild::GetCushionAngle(
    std::size_t index) const noexcept
{
    return index < cushionBehaviors_.size() &&
                   cushionBehaviors_[index] != nullptr
               ? cushionBehaviors_[index]->GetAngle() : 0.0F;
}

int CarAnimationChild::GetCushionTargetTag(
    std::size_t index) const noexcept
{
    return index < cushionBehaviors_.size() &&
                   cushionBehaviors_[index] != nullptr
               ? cushionBehaviors_[index]->GetTargetTag() : 0;
}

float CarAnimationChild::GetCushionAngleForTag(
    int targetTag) const noexcept
{
    const auto found = std::find_if(
        cushionBehaviors_.begin(), cushionBehaviors_.end(),
        [targetTag](const PodushkaAnim* behavior) {
            return behavior != nullptr &&
                   behavior->GetTargetTag() == targetTag;
        });
    return found != cushionBehaviors_.end()
               ? (*found)->GetAngle() : 0.0F;
}

} // namespace r3d::game::originalrace::source
