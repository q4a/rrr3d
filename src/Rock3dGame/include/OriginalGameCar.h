#pragma once

#include "OriginalGameObject.h"

#include <array>
#include <memory>
#include <vector>

namespace r3d::game::originalrace::source
{

class SoundMotor;
class CarWheel;
class CarAnimationChild;

struct SoundMotorMix
{
    float currentRpm = 0.0F;
    float idleVolume = 1.0F;
    float rpmVolume = 0.0F;
    float rpmFrequencyRatio = 1.0F;
};

struct WheelSlipProgress
{
    float slip = 0.0F;
    float volume = 0.0F;
    bool active = false;
    bool makeEffect = false;
    bool freeEffect = false;
    bool playSound = false;
    bool stopSound = false;
};

// Gameplay-owned lock state from GameCar. Wheel/actor operations are Jolt
// adapter responsibilities; timer ownership and source gates stay here.
class GameCar : public GameObject
{
public:
    static constexpr float clutchLockSeconds = 0.38F;
    static constexpr float springLockSeconds = 1.5F;

    struct ProgressResult
    {
        bool clutchReleased = false;
        bool springReleased = false;
        bool mineReleased = false;
        std::size_t behaviorsProgressed = 0U;
        std::size_t behaviorsRemoved = 0U;
        std::size_t wheelsProgressed = 0U;
        std::size_t wheelBehaviorsProgressed = 0U;
        std::size_t animationChildrenProgressed = 0U;
        std::size_t animationBehaviorsProgressed = 0U;
    };

    GameCar();
    GameCar(const GameCar& other);
    GameCar& operator=(const GameCar& other) noexcept;
    GameCar(GameCar&& other);
    GameCar& operator=(GameCar&& other) noexcept;
    ~GameCar() override;

    void Reset() noexcept;
    ProgressResult OnProgress(float deltaTime) noexcept;

    void BindSoundMotor(
        const std::array<float, 2>& rpmVolumeRange,
        const std::array<float, 2>& rpmFrequencyRange);
    void ReleaseSoundMotor() noexcept;
    SoundMotorMix OnMotor(
        float deltaTime, float rpm, float minimumRpm,
        float maximumRpm) noexcept;
    const SoundMotorMix& GetSoundMotorMix() const noexcept;
    bool HasSoundMotor() const noexcept;
    void BindWheels(
        const std::vector<bool>& slipEffects,
        const std::vector<bool>& slipSounds);
    void ReleaseWheels() noexcept;
    bool SetWheelContact(
        std::size_t wheel, bool hasContact,
        float longitudinalSlip, float lateralSlip) noexcept;
    WheelSlipProgress GetWheelSlipResult(
        std::size_t wheel) const noexcept;
    std::size_t GetWheelCount() const noexcept;
    CarWheel* GetWheel(std::size_t wheel) noexcept;
    const CarWheel* GetWheel(std::size_t wheel) const noexcept;
    void BindAnimationChildren(
        bool trackAnimation, std::size_t cushionAnimations);
    void ReleaseAnimationChildren() noexcept;
    void SetLeadWheelSpeed(float value) noexcept;
    float GetLeadWheelSpeed() const noexcept;
    float GetTrackTextureOffset() const noexcept;
    float GetCushionAngle(std::size_t index) const noexcept;
    std::size_t GetAnimationChildCount() const noexcept;
    CarAnimationChild* GetAnimationChild(std::size_t index) noexcept;
    const CarAnimationChild* GetAnimationChild(
        std::size_t index) const noexcept;

    bool LockClutch(float strength, bool clutchImmunity) noexcept;
    void CancelClutch() noexcept;
    bool IsClutchLocked() const noexcept;
    float GetClutchTime() const noexcept;
    float ConsumeClutchStrength() noexcept;

    void LockSpring() noexcept;
    bool IsSpringLocked() const noexcept;
    float GetSpringTime() const noexcept;

    void LockMine(float time) noexcept;
    bool IsMineLocked() const noexcept;
    float GetMineTime() const noexcept;

private:
    class SoundMotorBehavior;

    float clutchStrength_ = 0.0F;
    float clutchTime_ = 0.0F;
    float springTime_ = 0.0F;
    float mineTime_ = 0.0F;
    std::unique_ptr<SoundMotor> soundMotor_;
    SoundMotorMix soundMotorMix_;
    std::array<float, 2> rpmVolumeRange_{0.0F, 1.0F};
    std::array<float, 2> rpmFrequencyRange_{0.0F, 1.0F};
    std::vector<std::unique_ptr<CarWheel>> wheels_;
    std::vector<std::unique_ptr<CarAnimationChild>> animationChildren_;
    float leadWheelSpeed_ = 0.0F;
};

// Backend-neutral owner for GameBase.cpp::SoundMotor. SDL owns the two
// voices, while the source RPM lag and layer mix remain gameplay behavior.
class SoundMotor
{
public:
    static constexpr float motorLag = 10000.0F;
    using Mix = SoundMotorMix;

    void Reset() noexcept;
    Mix OnMotor(
        float deltaTime, float rpm, float minimumRpm,
        float maximumRpm, const std::array<float, 2>& rpmVolumeRange,
        const std::array<float, 2>& rpmFrequencyRange) noexcept;
    float GetCurrentRpm() const noexcept;

private:
    float currentRpm_ = 0.0F;
};

// Source PxWheelSlipEffect state. The graphics/audio backends realize the
// spawned effect and Source3d, but both consume this exact slip calculation
// and MakeEffect/FreeEffect transition.
class PxWheelSlipEffect
{
public:
    static constexpr float longitudinalThreshold = 0.4F;
    static constexpr float lateralThreshold = 0.7F;
    static constexpr float volumeScale = 4.0F;

    using ProgressResult = WheelSlipProgress;

    void Reset() noexcept;
    ProgressResult OnProgress(
        bool hasContact, float longitudinalSlip, float lateralSlip,
        bool hasSound) noexcept;
    static float SourceSlip(
        bool hasContact, float longitudinalSlip,
        float lateralSlip) noexcept;
    bool IsEffectMaked() const noexcept;

private:
    bool effectMaked_ = false;
};

// GameCar owns one source CarWheel GameObject per serialized wheel. The Jolt
// adapter supplies NxWheelContactData-equivalent values; the wheel's concrete
// behavior remains the sole authority for visual and audio slip transitions.
class CarWheel : public GameObject
{
public:
    CarWheel();
    CarWheel(bool slipEffect, bool slipSound);
    CarWheel(const CarWheel& other);
    CarWheel& operator=(const CarWheel& other) noexcept;
    CarWheel(CarWheel&& other);
    CarWheel& operator=(CarWheel&& other) noexcept;
    ~CarWheel() override;

    void Configure(bool slipEffect, bool slipSound);
    void SetContact(bool hasContact, float longitudinalSlip,
                    float lateralSlip) noexcept;
    GameObject::ProgressResult OnProgress(float deltaTime) noexcept;
    const WheelSlipProgress& GetSlipResult() const noexcept;
    bool HasSlipEffect() const noexcept;
    bool HasSlipSound() const noexcept;

private:
    class WheelSlipBehavior;

    PxWheelSlipEffect slipEffect_;
    WheelSlipProgress slipResult_;
    float longitudinalSlip_ = 0.0F;
    float lateralSlip_ = 0.0F;
    bool hasContact_ = false;
    bool slipEffectEnabled_ = false;
    bool slipSoundEnabled_ = false;
};

class GusenizaAnim
{
public:
    static constexpr float trackLength = 5.0F;

    void Reset() noexcept;
    float OnProgress(float deltaTime, float leadWheelSpeed) noexcept;
    float GetTextureOffset() const noexcept;

private:
    float xAnimationOffset_ = 0.0F;
};

class PodushkaAnim
{
public:
    static constexpr float minimumWheelSpeed = 1.0F;

    void Reset() noexcept;
    float OnProgress(float deltaTime, float leadWheelSpeed) noexcept;
    float GetAngle() const noexcept;

private:
    float angle_ = 0.0F;
};

// Serialized car include actors keep their own behavior owners. Guseniza has
// one type-13 behavior; Podushka has two type-14 behaviors targeting tags 1
// and 2 on the same included actor in the shipped database.
class CarAnimationChild : public GameObject
{
public:
    CarAnimationChild();
    CarAnimationChild(bool trackAnimation,
                      std::size_t cushionAnimations);
    CarAnimationChild(const CarAnimationChild& other);
    CarAnimationChild& operator=(
        const CarAnimationChild& other) noexcept;
    CarAnimationChild(CarAnimationChild&& other);
    CarAnimationChild& operator=(CarAnimationChild&& other) noexcept;
    ~CarAnimationChild() override;

    void Configure(bool trackAnimation,
                   std::size_t cushionAnimations);
    GameObject::ProgressResult OnProgress(float deltaTime) noexcept;
    bool HasTrackAnimation() const noexcept;
    std::size_t GetCushionAnimationCount() const noexcept;
    float GetTrackTextureOffset() const noexcept;
    float GetCushionAngle(std::size_t index) const noexcept;

private:
    class TrackBehavior;
    class CushionBehavior;
    void BindBehaviors();

    GusenizaAnim trackAnimation_;
    std::vector<PodushkaAnim> cushionAnimations_;
    bool hasTrackAnimation_ = false;
};

} // namespace r3d::game::originalrace::source
