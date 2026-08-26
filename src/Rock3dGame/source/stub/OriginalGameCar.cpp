#include "OriginalGameCar.h"

#include <algorithm>
#include <cmath>

namespace r3d::game::originalrace::source
{

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
    ReleaseWheels();
    ReleaseSoundMotor();
    GameObject::operator=(other);
    clutchStrength_ = other.clutchStrength_;
    clutchTime_ = other.clutchTime_;
    springTime_ = other.springTime_;
    mineTime_ = other.mineTime_;
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
    return *this;
}

GameCar::GameCar(GameCar&& other) : GameCar(other) {}

GameCar& GameCar::operator=(GameCar&& other) noexcept
{
    return *this = static_cast<const GameCar&>(other);
}

GameCar::~GameCar()
{
    ReleaseWheels();
    ReleaseSoundMotor();
}

void GameCar::Reset() noexcept
{
    clutchStrength_ = 0.0F;
    clutchTime_ = 0.0F;
    springTime_ = 0.0F;
    mineTime_ = 0.0F;
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
    for (auto& wheel : wheels_)
    {
        if (wheel == nullptr)
            continue;
        const auto wheelResult = wheel->OnProgress(deltaTime);
        ++result.wheelsProgressed;
        result.wheelBehaviorsProgressed +=
            wheelResult.behaviorsProgressed;
    }
    if (clutchTime_ > 0.0F)
    {
        clutchTime_ -= deltaTime;
        if (clutchTime_ < 0.0F)
            clutchTime_ = 0.0F;
        result.clutchReleased = clutchTime_ == 0.0F;
    }
    if (mineTime_ > 0.0F)
    {
        mineTime_ -= deltaTime;
        if (mineTime_ < 0.0F)
            mineTime_ = 0.0F;
        result.mineReleased = mineTime_ == 0.0F;
    }
    if (springTime_ > 0.0F)
    {
        springTime_ = std::max(springTime_ - deltaTime, 0.0F);
        result.springReleased = springTime_ == 0.0F;
    }
    return result;
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
    float longitudinalSlip, float lateralSlip) noexcept
{
    auto* target = GetWheel(wheel);
    if (target == nullptr)
        return false;
    target->SetContact(
        hasContact, longitudinalSlip, lateralSlip);
    return true;
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
    hasContact_ = other.hasContact_;
    slipEffectEnabled_ = other.slipEffectEnabled_;
    slipSoundEnabled_ = other.slipSoundEnabled_;
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
    slipEffectEnabled_ = slipEffect;
    slipSoundEnabled_ = slipEffect && slipSound;
    if (slipEffectEnabled_)
    {
        GetBehaviors().Add<WheelSlipBehavior>(
            BehaviorType::PxWheelSlipEffect, this);
    }
}

void CarWheel::SetContact(
    bool hasContact, float longitudinalSlip,
    float lateralSlip) noexcept
{
    hasContact_ = hasContact;
    longitudinalSlip_ = longitudinalSlip;
    lateralSlip_ = lateralSlip;
}

GameObject::ProgressResult CarWheel::OnProgress(
    float deltaTime) noexcept
{
    slipResult_ = {};
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

} // namespace r3d::game::originalrace::source
