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
        std::size_t behaviorsProgressed = 0U;
        std::size_t behaviorsRemoved = 0U;
        std::size_t wheelsProgressed = 0U;
        std::size_t wheelBehaviorsProgressed = 0U;
        std::size_t animationChildrenProgressed = 0U;
        std::size_t animationBehaviorsProgressed = 0U;
    };

    using PxSyncPose = GameObjectFrameSync::Pose;
    struct PxSyncState
    {
        PxSyncPose body;
        std::vector<PxSyncPose> wheels;
    };

    enum class MoveCarState
    {
        None,
        Brake,
        Back,
        Accel,
    };

    enum class SteerWheelState
    {
        None,
        OnLeft,
        OnRight,
        Manual,
    };

    struct MotorDescription
    {
        float brakeTorque = 7500.0F;
        float differentialRatio = 3.42F;
        float maximumRpm = 7000.0F;
        float idlingRpm = 1000.0F;
        float maximumTorque = 2000.0F;
        float torqueEfficiency = 0.805F;
        float restBrakeTorque = 400.0F;
        float maximumSpeed = 0.0F;
        bool automaticGears = true;
    };

    struct FixedStepInput
    {
        float throttle = 0.0F;
        float reverse = 0.0F;
        float brake = 0.0F;
        float steering = 0.0F;
        bool manualSteering = false;
    };

    struct FixedStepState
    {
        float signedSpeed = 0.0F;
        float absoluteSpeed = 0.0F;
        float horizontalSpeed = 0.0F;
        float drivenWheelAngularSpeed = 0.0F;
        bool anyWheelContact = false;
        bool drivenWheelContact = false;
        bool allWheelContact = false;
        bool bodyContact = false;
    };

    struct DynamicsDescription
    {
        std::array<float, 3U> angularDamping{1.0F, 1.0F, 1.0F};
        float airbornePitchAcceleration = 0.0F;
        float clampRollAngle = 0.0F;
        float clampPitchAngle = 0.0F;
        float maximumSteerAngle = 0.0F;
        float steerSpeed = 0.0F;
        float steerRotation = 0.0F;
        bool gravitySteering = false;
        float steeringControl = 1.0F;
        bool clutchImmunity = false;
        float tireSpring = 0.0F;
        bool disableColor = false;
    };

    struct WheelDynamics
    {
        float positionX = 0.0F;
        bool driven = false;
        bool steering = false;
        bool inverted = false;
        float radius = 0.0F;
        std::array<float, 3U> visualOffset{};
    };

    struct DriveCommand
    {
        float motorTorque = 0.0F;
        float brakeTorque = 0.0F;
        float rpm = 0.0F;
        int gear = -1;
        float steeringAngle = 0.0F;
        float steeringYaw = 0.0F;
        float rearWheelX = 0.0F;
        // Windows Player::SetCheatK changes the live GameCar wheel tire
        // function. The backend consumes the resulting source-owned scale.
        float lateralGripScale = 1.0F;
        std::array<float, 3U> angularDamping{1.0F, 1.0F, 1.0F};
        float clampRollAngle = 0.0F;
        float clampPitchAngle = 0.0F;
        bool applyExtraGravity = false;
        float airbornePitchAcceleration = 0.0F;
        bool clutchReleased = false;
        bool springReleased = false;
        bool mineReleased = false;
    };

    GameCar();
    GameCar(const GameCar& other);
    GameCar& operator=(const GameCar& other) noexcept;
    GameCar(GameCar&& other);
    GameCar& operator=(GameCar&& other) noexcept;
    ~GameCar() override;

    void Reset() noexcept;
    ProgressResult OnProgress(float deltaTime) noexcept;
    // Windows GameCar::OnPxSync owns graph synchronization for both the
    // car actor and every child CarWheel. The Jolt adapter supplies completed
    // world poses; this source object returns the poses consumed by bgfx.
    PxSyncState OnPxSync(
        PxSyncPose physicalBody,
        const std::vector<PxSyncPose>& physicalWheels,
        float deltaTime) noexcept;
    void ConfigureMotor(MotorDescription description) noexcept;
    void ConfigureDynamics(
        DynamicsDescription description,
        const std::vector<WheelDynamics>& wheels) noexcept;
    DriveCommand OnFixedStepDrive(
        float deltaTime, FixedStepInput input,
        FixedStepState state) noexcept;
    int GearUp() noexcept;
    int GearDown() noexcept;
    const MotorDescription& GetMotorDesc() const noexcept;
    void SetMotorDesc(MotorDescription value) noexcept;
    MoveCarState GetMoveCar() const noexcept;
    void SetMoveCar(MoveCarState value) noexcept;
    SteerWheelState GetSteerWheel() const noexcept;
    void SetSteerWheel(SteerWheelState value) noexcept;
    float GetSteerWheelAngle() const noexcept;
    void SetSteerWheelAngle(float value) noexcept;
    int GetCurGear() const noexcept;
    void SetCurGear(int value) noexcept;
    float GetKSteerControl() const noexcept;
    void SetKSteerControl(float value) noexcept;
    float GetSteerSpeed() const noexcept;
    void SetSteerSpeed(float value) noexcept;
    float GetSteerRot() const noexcept;
    void SetSteerRot(float value) noexcept;
    std::array<float, 3U> GetAngDamping() const noexcept;
    void SetAngDamping(std::array<float, 3U> value) noexcept;
    float GetFlyYTorque() const noexcept;
    void SetFlyYTourque(float value) noexcept;
    float GetClampXTorque() const noexcept;
    void SetClampXTourque(float value) noexcept;
    float GetClampYTorque() const noexcept;
    void SetClampYTourque(float value) noexcept;
    float GetMotorTorqueK() const noexcept;
    void SetMotorTorqueK(float value) noexcept;
    float GetWheelSteerK() const noexcept;
    void SetWheelSteerK(float value) noexcept;
    // The Windows getters read the live PhysX actor/wheel shapes. The
    // portable backend synchronizes that same physical state into GameCar,
    // which remains the gameplay-facing telemetry authority.
    void SynchronizeSpeed(float signedSpeed) noexcept;
    float GetSpeed() const noexcept;
    float GetLeadWheelSpeed() const noexcept;
    float GetDrivenWheelSpeed() const noexcept;
    float GetRPM() const noexcept;
    bool IsGravEngine() const noexcept;
    void SetGravEngine(bool value) noexcept;
    bool IsClutchImmunity() const noexcept;
    void SetClutchImmunity(bool value) noexcept;
    float GetMaxSpeed() const noexcept;
    void SetMaxSpeed(float value) noexcept;
    float GetTireSpring() const noexcept;
    void SetTireSpring(float value) noexcept;
    bool GetDisableColor() const noexcept;
    void SetDisableColor(bool value) noexcept;

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
        float longitudinalSlip, float lateralSlip,
        float normalReaction = 0.0F,
        float normalImpulse = 0.0F) noexcept;
    void UpdateContactState(bool bodyContact) noexcept;
    bool IsAnyWheelContact() const noexcept;
    bool IsWheelsContact() const noexcept;
    bool IsBodyContact() const noexcept;
    WheelSlipProgress GetWheelSlipResult(
        std::size_t wheel) const noexcept;
    std::size_t GetWheelCount() const noexcept;
    CarWheel* GetWheel(std::size_t wheel) noexcept;
    const CarWheel* GetWheel(std::size_t wheel) const noexcept;
    std::size_t GetLeadWheelCount() const noexcept;
    CarWheel* GetLeadWheel(std::size_t wheel) noexcept;
    const CarWheel* GetLeadWheel(std::size_t wheel) const noexcept;
    std::size_t GetSteerWheelCount() const noexcept;
    CarWheel* GetSteerGroupWheel(std::size_t wheel) noexcept;
    const CarWheel* GetSteerGroupWheel(
        std::size_t wheel) const noexcept;
    void BindAnimationChildren(
        bool trackAnimation, std::size_t cushionAnimations);
    void ReleaseAnimationChildren() noexcept;
    float GetTrackTextureOffset() const noexcept;
    float GetCushionAngle(std::size_t index) const noexcept;
    std::size_t GetAnimationChildCount() const noexcept;
    CarAnimationChild* GetAnimationChild(std::size_t index) noexcept;
    const CarAnimationChild* GetAnimationChild(
        std::size_t index) const noexcept;

    bool LockClutch(float strength) noexcept;
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
    float signedSpeed_ = 0.0F;
    MotorDescription motor_;
    DynamicsDescription dynamics_;
    MoveCarState moveCar_ = MoveCarState::None;
    SteerWheelState steerWheel_ = SteerWheelState::None;
    int currentGear_ = -1;
    float steeringAngle_ = 0.0F;
    float steeringControl_ = 1.0F;
    float maximumSpeed_ = 0.0F;
    float tireSpring_ = 0.0F;
    // Source GameCar::_motorTorqueK/_wheelSteerK. They are runtime state,
    // not serialized ctCar parameters and default to the neutral multiplier.
    float motorTorqueK_ = 1.0F;
    float wheelSteerK_ = 1.0F;
    bool anyWheelContact_ = false;
    bool wheelsContact_ = false;
    bool bodyContact_ = false;
    bool clutchImmunity_ = false;
    bool disableColor_ = false;
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
    const GameObjectFrameSync::Pose& PxSyncWheel(
        GameObjectFrameSync::Pose physicalBody,
        GameObjectFrameSync::Pose graphBody,
        GameObjectFrameSync::Pose physicalWheel) noexcept;
    const GameObjectFrameSync::Pose& GetPxSyncPose() const noexcept;
    void ConfigureDynamics(
        float positionX, bool driven, bool steering,
        bool inverted, float radius,
        std::array<float, 3U> visualOffset = {}) noexcept;
    void SetSteerAngle(float value) noexcept;
    void SetAxleSpeed(float value) noexcept;
    void ResetMotion() noexcept;
    float GetSteerAngle() const noexcept;
    float GetAxleSpeed() const noexcept;
    float GetSummAngle() const noexcept;
    float GetPositionX() const noexcept;
    float GetRadius() const noexcept;
    float GetLongSlip() const noexcept;
    float GetLatSlip() const noexcept;
    bool GetLead() const noexcept;
    void SetLead(bool value) noexcept;
    bool GetSteer() const noexcept;
    void SetSteer(bool value) noexcept;
    const std::array<float, 3U>& GetOffset() const noexcept;
    void SetOffset(std::array<float, 3U> value) noexcept;
    bool GetInvertWheel() const noexcept;
    void SetInvertWheel(bool value) noexcept;
    bool IsDriven() const noexcept;
    bool IsSteering() const noexcept;
    void SetContact(bool hasContact, float longitudinalSlip,
                    float lateralSlip, float normalReaction,
                    float normalImpulse) noexcept;
    bool HasContact() const noexcept;
    float GetNormalReaction() const noexcept;
    float GetNormalImpulse() const noexcept;
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
    // Windows CarWheel::_nReac is written by MyContactModify before the
    // resulting normalForce is clamped. Keep the raw ratio and the solver
    // impulse distinct at the source-object boundary.
    float normalReaction_ = 0.0F;
    float normalImpulse_ = 0.0F;
    bool hasContact_ = false;
    bool slipEffectEnabled_ = false;
    bool slipSoundEnabled_ = false;
    GameObjectFrameSync::Pose pxSyncPose_;
    float positionX_ = 0.0F;
    float radius_ = 0.0F;
    std::array<float, 3U> offset_{};
    float steerAngle_ = 0.0F;
    float axleSpeed_ = 0.0F;
    float summAngle_ = 0.0F;
    bool driven_ = false;
    bool steering_ = false;
    bool inverted_ = false;
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
