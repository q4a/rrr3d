#include "OriginalGameCar.h"

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

} // namespace

class GameCar::SoundMotorBehavior final : public Behavior
{
public:
    SoundMotorBehavior(Behaviors* owner, GameCar* car) noexcept
        : Behavior(owner), car_(car)
    {
    }

    void OnProgress(float) noexcept override
    {
        // Windows SoundMotor::OnProgress only updates Source3d world
        // positions. SDL applies that backend boundary from the car pose.
    }

protected:
    void OnMotor(float deltaTime, float rpm,
                 float minimumRpm, float maximumRpm) noexcept override
    {
        if (car_ == nullptr || car_->soundMotor_ == nullptr)
            return;
        car_->soundMotorMix_ = car_->soundMotor_->OnMotor(
            deltaTime, rpm, minimumRpm, maximumRpm,
            car_->rpmVolumeRange_, car_->rpmFrequencyRange_);
    }

private:
    GameCar* car_ = nullptr;
};

GameCar::GameCar() = default;

GameCar::GameCar(const GameCar& other) : GameObject(other)
{
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
    leadWheelSpeed_ = other.leadWheelSpeed_;
    motor_ = other.motor_;
    dynamics_ = other.dynamics_;
    moveCar_ = other.moveCar_;
    steerWheel_ = other.steerWheel_;
    currentGear_ = other.currentGear_;
    steeringAngle_ = other.steeringAngle_;
    motorTorqueK_ = other.motorTorqueK_;
    wheelSteerK_ = other.wheelSteerK_;
    anyWheelContact_ = other.anyWheelContact_;
    wheelsContact_ = other.wheelsContact_;
    bodyContact_ = other.bodyContact_;
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
    ReleaseAnimationChildren();
    ReleaseWheels();
    ReleaseSoundMotor();
}

void GameCar::Reset() noexcept
{
    clutchStrength_ = 0.0F;
    clutchTime_ = 0.0F;
    springTime_ = 0.0F;
    mineTime_ = 0.0F;
    leadWheelSpeed_ = 0.0F;
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
    GetFrameSync().Reset();
    if (soundMotor_ != nullptr)
        soundMotor_->Reset();
    soundMotorMix_ = {};
}

GameCar::ProgressResult GameCar::OnProgress(float deltaTime) noexcept
{
    ProgressResult result;
    const auto gameObject = GameObject::OnProgress(deltaTime);
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
    float deltaTime) noexcept
{
    PxSyncState state;
    state.body = GetFrameSync().OnFrame(physicalBody, deltaTime);
    const std::size_t count = std::min(
        wheels_.size(), physicalWheels.size());
    state.wheels.reserve(count);
    for (std::size_t index = 0U; index < count; ++index)
    {
        if (wheels_[index] == nullptr)
            continue;
        state.wheels.push_back(wheels_[index]->PxSyncWheel(
            physicalBody, state.body, physicalWheels[index]));
    }
    return state;
}

void GameCar::ConfigureMotor(MotorDescription description) noexcept
{
    motor_ = description;
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
    currentGear_ = -1;
    moveCar_ = MoveCarState::None;
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
    steeringAngle_ = 0.0F;
    const std::size_t count = std::min(wheels_.size(), wheels.size());
    for (std::size_t index = 0U; index < count; ++index)
    {
        if (wheels_[index] == nullptr)
            continue;
        wheels_[index]->ConfigureDynamics(
            wheels[index].positionX, wheels[index].driven,
            wheels[index].steering, wheels[index].inverted);
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
        moveCar_ = MoveCarState::Brake;
    else if (input.reverse > 0.0001F)
        moveCar_ = MoveCarState::Back;
    else if (input.throttle > 0.0001F)
        moveCar_ = MoveCarState::Accel;
    else
        moveCar_ = MoveCarState::None;

    command.brakeTorque = motor_.restBrakeTorque;
    switch (moveCar_)
    {
    case MoveCarState::None:
        command.rpm = calcRpm(currentGear_);
        break;
    case MoveCarState::Brake:
        currentGear_ = -1;
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
            currentGear_ = 0;
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
            currentGear_ = -1;
            command.rpm = calcRpm(currentGear_);
            command.brakeTorque = motor_.brakeTorque;
        }
        else
        {
            if (currentGear_ <= 0)
                currentGear_ = 1;
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
            --currentGear_;
        if (command.rpm >= motor_.maximumRpm &&
            currentGear_ < static_cast<int>(gearRatios.size() - 1U))
            ++currentGear_;
    }
    if (motor_.maximumSpeed > 0.0F &&
        state.absoluteSpeed > motor_.maximumSpeed)
        command.motorTorque = command.brakeTorque;
    command.gear = currentGear_;
    command.lateralGripScale = wheelSteerK_;

    const float targetSteering =
        input.steering * dynamics_.maximumSteerAngle;
    if (std::abs(input.steering) <= 0.0001F)
        steerWheel_ = SteerWheelState::None;
    else if (input.manualSteering)
        steerWheel_ = SteerWheelState::Manual;
    else if (input.steering > 0.0F)
        steerWheel_ = SteerWheelState::OnLeft;
    else
        steerWheel_ = SteerWheelState::OnRight;

    if (steerWheel_ == SteerWheelState::Manual)
    {
        steeringAngle_ = targetSteering;
    }
    else if (steerWheel_ == SteerWheelState::OnLeft)
    {
        steeringAngle_ = dynamics_.steerSpeed > 0.0F
            ? std::min(
                  std::max(steeringAngle_, 0.0F) +
                      dynamics_.steerSpeed * deltaTime,
                  dynamics_.maximumSteerAngle)
            : dynamics_.maximumSteerAngle;
    }
    else if (steerWheel_ == SteerWheelState::OnRight)
    {
        steeringAngle_ = dynamics_.steerSpeed > 0.0F
            ? std::max(
                  std::min(steeringAngle_, 0.0F) -
                      dynamics_.steerSpeed * deltaTime,
                  -dynamics_.maximumSteerAngle)
            : -dynamics_.maximumSteerAngle;
    }
    else
        steeringAngle_ = 0.0F;
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

GameCar::MoveCarState GameCar::GetMoveCar() const noexcept
{
    return moveCar_;
}

GameCar::SteerWheelState GameCar::GetSteerWheel() const noexcept
{
    return steerWheel_;
}

float GameCar::GetSteerWheelAngle() const noexcept
{
    return steeringAngle_;
}

int GameCar::GetCurGear() const noexcept
{
    return currentGear_;
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

void GameCar::BindSoundMotor(
    const std::array<float, 2>& rpmVolumeRange,
    const std::array<float, 2>& rpmFrequencyRange)
{
    ReleaseSoundMotor();
    rpmVolumeRange_ = rpmVolumeRange;
    rpmFrequencyRange_ = rpmFrequencyRange;
    soundMotor_ = std::make_unique<SoundMotor>();
    GetBehaviors().Add<SoundMotorBehavior>(
        BehaviorType::SoundMotor, this);
}

void GameCar::ReleaseSoundMotor() noexcept
{
    GetBehaviors().Clear();
    soundMotor_.reset();
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
    float normalReaction, float normalImpulse) noexcept
{
    auto* target = GetWheel(wheel);
    if (target == nullptr)
        return false;
    target->SetContact(
        hasContact, longitudinalSlip, lateralSlip,
        normalReaction, normalImpulse);
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

void GameCar::BindAnimationChildren(
    bool trackAnimation, std::size_t cushionAnimations)
{
    ReleaseAnimationChildren();
    if (trackAnimation)
    {
        auto child = std::make_unique<CarAnimationChild>(true, 0U);
        child->SetParent(this);
        animationChildren_.push_back(std::move(child));
    }
    if (cushionAnimations > 0U)
    {
        auto child = std::make_unique<CarAnimationChild>(
            false, cushionAnimations);
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

void GameCar::SetLeadWheelSpeed(float value) noexcept
{
    leadWheelSpeed_ = std::abs(value) > 0.1F ? value : 0.0F;
}

float GameCar::GetLeadWheelSpeed() const noexcept
{
    return leadWheelSpeed_;
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

bool GameCar::LockClutch(float strength, bool clutchImmunity) noexcept
{
    if (clutchImmunity || clutchTime_ > 0.0F)
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

void PxWheelSlipEffect::Reset() noexcept
{
    effectMaked_ = false;
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
    bool hasSound) noexcept
{
    ProgressResult result;
    result.slip = SourceSlip(
        hasContact, longitudinalSlip, lateralSlip);
    result.volume = std::clamp(result.slip * volumeScale, 0.0F, 1.0F);
    result.active = result.slip > 0.0F;
    if (result.active)
    {
        result.makeEffect = !effectMaked_;
        effectMaked_ = true;
        result.playSound = hasSound;
    }
    else
    {
        result.freeEffect = effectMaked_;
        effectMaked_ = false;
        // PxWheelSlipEffect calls Source3d::Stop even when no visual actor
        // was active, provided this behavior owns a sound.
        result.stopSound = hasSound;
    }
    return result;
}

bool PxWheelSlipEffect::IsEffectMaked() const noexcept
{
    return effectMaked_;
}

class CarWheel::WheelSlipBehavior final : public Behavior
{
public:
    WheelSlipBehavior(Behaviors* owner, CarWheel* wheel) noexcept
        : Behavior(owner), wheel_(wheel)
    {
    }

    void OnProgress(float) noexcept override
    {
        if (wheel_ == nullptr)
            return;
        wheel_->slipResult_ = wheel_->slipEffect_.OnProgress(
            wheel_->hasContact_, wheel_->longitudinalSlip_,
            wheel_->lateralSlip_, wheel_->slipSoundEnabled_);
    }

private:
    CarWheel* wheel_ = nullptr;
};

CarWheel::CarWheel() = default;

CarWheel::CarWheel(bool slipEffect, bool slipSound)
{
    Configure(slipEffect, slipSound);
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
    slipEffect_ = other.slipEffect_;
    slipResult_ = other.slipResult_;
    longitudinalSlip_ = other.longitudinalSlip_;
    lateralSlip_ = other.lateralSlip_;
    normalReaction_ = other.normalReaction_;
    normalImpulse_ = other.normalImpulse_;
    hasContact_ = other.hasContact_;
    slipEffectEnabled_ = other.slipEffectEnabled_;
    slipSoundEnabled_ = other.slipSoundEnabled_;
    pxSyncPose_ = other.pxSyncPose_;
    positionX_ = other.positionX_;
    steerAngle_ = other.steerAngle_;
    axleSpeed_ = other.axleSpeed_;
    summAngle_ = other.summAngle_;
    driven_ = other.driven_;
    steering_ = other.steering_;
    inverted_ = other.inverted_;
    if (slipEffectEnabled_)
    {
        GetBehaviors().Add<WheelSlipBehavior>(
            BehaviorType::PxWheelSlipEffect, this);
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
    slipEffect_.Reset();
    slipResult_ = {};
    hasContact_ = false;
    longitudinalSlip_ = 0.0F;
    lateralSlip_ = 0.0F;
    normalReaction_ = 0.0F;
    normalImpulse_ = 0.0F;
    slipEffectEnabled_ = slipEffect;
    slipSoundEnabled_ = slipEffect && slipSound;
    pxSyncPose_ = {};
    ResetMotion();
    if (slipEffectEnabled_)
    {
        GetBehaviors().Add<WheelSlipBehavior>(
            BehaviorType::PxWheelSlipEffect, this);
    }
}

const GameObjectFrameSync::Pose& CarWheel::PxSyncWheel(
    GameObjectFrameSync::Pose physicalBody,
    GameObjectFrameSync::Pose graphBody,
    GameObjectFrameSync::Pose physicalWheel) noexcept
{
    const SyncQuaternion physicalRotation =
        normalizedSync(physicalBody.rotation);
    const SyncQuaternion graphFromPhysical = multiplySync(
        graphBody.rotation,
        {-physicalRotation.x, -physicalRotation.y,
         -physicalRotation.z, physicalRotation.w});
    const SyncVector relative{
        physicalWheel.position.x - physicalBody.position.x,
        physicalWheel.position.y - physicalBody.position.y,
        physicalWheel.position.z - physicalBody.position.z};
    const SyncVector rotated = rotateSync(
        relative, graphFromPhysical);
    pxSyncPose_.position = {
        graphBody.position.x + rotated.x,
        graphBody.position.y + rotated.y,
        graphBody.position.z + rotated.z};
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
    pxSyncPose_.rotation = multiplySync(
        graphBody.rotation, localRotation);
    return pxSyncPose_;
}

const GameObjectFrameSync::Pose& CarWheel::GetPxSyncPose() const noexcept
{
    return pxSyncPose_;
}

void CarWheel::ConfigureDynamics(
    float positionX, bool driven, bool steering,
    bool inverted) noexcept
{
    positionX_ = positionX;
    driven_ = driven;
    steering_ = steering;
    inverted_ = inverted;
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

bool CarWheel::IsDriven() const noexcept
{
    return driven_;
}

bool CarWheel::IsSteering() const noexcept
{
    return steering_;
}

void CarWheel::SetContact(
    bool hasContact, float longitudinalSlip,
    float lateralSlip, float normalReaction,
    float normalImpulse) noexcept
{
    hasContact_ = hasContact;
    longitudinalSlip_ = longitudinalSlip;
    lateralSlip_ = lateralSlip;
    normalReaction_ = hasContact ? normalReaction : 0.0F;
    normalImpulse_ = hasContact ? normalImpulse : 0.0F;
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

GameObject::ProgressResult CarWheel::OnProgress(
    float deltaTime) noexcept
{
    slipResult_ = {};
    summAngle_ += axleSpeed_ * std::max(deltaTime, 0.0F);
    return GameObject::OnProgress(deltaTime);
}

const WheelSlipProgress& CarWheel::GetSlipResult() const noexcept
{
    return slipResult_;
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

void PodushkaAnim::Reset() noexcept
{
    angle_ = 0.0F;
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

class CarAnimationChild::TrackBehavior final : public Behavior
{
public:
    TrackBehavior(Behaviors* owner, CarAnimationChild* child) noexcept
        : Behavior(owner), child_(child)
    {
    }

    void OnProgress(float deltaTime) noexcept override
    {
        if (child_ == nullptr)
            return;
        const auto* car = dynamic_cast<const GameCar*>(child_->GetParent());
        child_->trackAnimation_.OnProgress(
            deltaTime, car != nullptr ? car->GetLeadWheelSpeed() : 0.0F);
    }

private:
    CarAnimationChild* child_ = nullptr;
};

class CarAnimationChild::CushionBehavior final : public Behavior
{
public:
    CushionBehavior(Behaviors* owner, CarAnimationChild* child,
                    std::size_t index) noexcept
        : Behavior(owner), child_(child), index_(index)
    {
    }

    void OnProgress(float deltaTime) noexcept override
    {
        if (child_ == nullptr ||
            index_ >= child_->cushionAnimations_.size())
            return;
        const auto* car = dynamic_cast<const GameCar*>(child_->GetParent());
        child_->cushionAnimations_[index_].OnProgress(
            deltaTime, car != nullptr ? car->GetLeadWheelSpeed() : 0.0F);
    }

private:
    CarAnimationChild* child_ = nullptr;
    std::size_t index_ = 0U;
};

CarAnimationChild::CarAnimationChild() = default;

CarAnimationChild::CarAnimationChild(
    bool trackAnimation, std::size_t cushionAnimations)
{
    Configure(trackAnimation, cushionAnimations);
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
    trackAnimation_ = other.trackAnimation_;
    cushionAnimations_ = other.cushionAnimations_;
    hasTrackAnimation_ = other.hasTrackAnimation_;
    BindBehaviors();
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

void CarAnimationChild::BindBehaviors()
{
    GetBehaviors().Clear();
    if (hasTrackAnimation_)
    {
        GetBehaviors().Add<TrackBehavior>(
            BehaviorType::GusenizaAnim, this);
    }
    for (std::size_t index = 0U;
         index < cushionAnimations_.size(); ++index)
    {
        GetBehaviors().Add<CushionBehavior>(
            BehaviorType::PodushkaAnim, this, index);
    }
}

void CarAnimationChild::Configure(
    bool trackAnimation, std::size_t cushionAnimations)
{
    trackAnimation_.Reset();
    cushionAnimations_.assign(cushionAnimations, PodushkaAnim{});
    hasTrackAnimation_ = trackAnimation;
    BindBehaviors();
}

GameObject::ProgressResult CarAnimationChild::OnProgress(
    float deltaTime) noexcept
{
    return GameObject::OnProgress(deltaTime);
}

bool CarAnimationChild::HasTrackAnimation() const noexcept
{
    return hasTrackAnimation_ &&
           GetBehaviors().Find(BehaviorType::GusenizaAnim) != nullptr;
}

std::size_t CarAnimationChild::GetCushionAnimationCount() const noexcept
{
    return cushionAnimations_.size();
}

float CarAnimationChild::GetTrackTextureOffset() const noexcept
{
    return trackAnimation_.GetTextureOffset();
}

float CarAnimationChild::GetCushionAngle(
    std::size_t index) const noexcept
{
    return index < cushionAnimations_.size()
               ? cushionAnimations_[index].GetAngle() : 0.0F;
}

} // namespace r3d::game::originalrace::source
