#pragma once

#include "physics/OriginalVehiclePhysics.h"

#include <cstdint>

namespace r3d::game::originalrace::source
{

// Source CameraManager::Style values which are reachable in the retail race
// and in the separately enabled Windows _DEBUG camera cycle.
enum class CameraStyle : std::uint8_t
{
    ThirdPerson,
    Isometric,
    Lights,
    IsometricView,
    FreeView,
};

enum class CameraProjection : std::uint8_t
{
    Perspective,
    Orthographic,
};

struct CameraTarget
{
    r3d::physics::Vec3 position;
    r3d::physics::Quat rotation;
    r3d::physics::Vec3 linearVelocity;
    float drivenWheelSpeed = 0.0F;
};

// Backend-neutral output of CameraManager::Control::OnInputFrame. bgfx owns
// only the conversion of this source pose/projection policy to view and
// projection matrices.
struct CameraFrame
{
    r3d::physics::Vec3 position;
    r3d::physics::Vec3 direction{1.0F, 0.0F, 0.0F};
    r3d::physics::Vec3 up{0.0F, 0.0F, 1.0F};
    r3d::physics::Quat rotation;
    CameraProjection projection = CameraProjection::Perspective;
    float orthographicWidth = 0.0F;
    float verticalFovDegrees = 75.0F;
    float nearDistance = 1.0F;
    float farDistance = 120.0F;
    float pointSpriteScale = 0.25F;
};

struct CameraScreenPoint
{
    float x = 0.0F;
    float y = 0.0F;
};

struct CameraRay
{
    r3d::physics::Vec3 origin;
    r3d::physics::Vec3 direction{1.0F, 0.0F, 0.0F};
};

struct ObserverPose
{
    r3d::physics::Vec3 position;
    r3d::physics::Quat rotation;
};

struct ObserverConfig
{
    float angularSpeed = 3.14159265358979323846F / 48.0F;
    float stablePitch = 75.0F * 3.14159265358979323846F / 180.0F;
    float minimumPitch = 25.0F * 3.14159265358979323846F / 180.0F;
    float maximumPitch = 80.0F * 3.14159265358979323846F / 180.0F;
    float positiveYawClamp = 0.0F;
    float negativeYawClamp = 0.0F;
};

// CameraManager::csAutoObserver input/state without the Windows ControlManager
// dependency. SDL translates pointer events to these three calls; all camera
// angles, idle restoration, clamps and interpolation stay source-owned.
class AutoObserver
{
public:
    void PointerDown(float x, float y) noexcept;
    void PointerUp(float x, float y) noexcept;
    void PointerMove(float x, float y) noexcept;
    ObserverPose OnFrame(
        const ObserverPose& source, float deltaTime,
        const ObserverConfig& config = {}) noexcept;
    void Reset() noexcept;

    bool IsInitialized() const noexcept;
    bool IsDragging() const noexcept;
    float GetDirection() const noexcept;

private:
    ObserverPose source_;
    r3d::physics::Quat targetRotation_;
    r3d::physics::Quat cameraRotation_;
    float yaw_ = 0.0F;
    float pitch_ = 0.0F;
    float direction_ = 1.0F;
    float idleSeconds_ = 3.0F;
    float anchorX_ = 0.0F;
    float anchorY_ = 0.0F;
    float lastX_ = 0.0F;
    float lastY_ = 0.0F;
    bool initialized_ = false;
    bool leftDown_ = false;
    bool dragging_ = false;
};

// Gameplay owner transcribed from CameraManager.cpp. Matrix construction
// remains at the renderer boundary; camera state, interpolation, fly and
// screen/ray policy live here exactly once.
class CameraManager
{
public:
    CameraFrame OnFrame(
        const CameraTarget& target, CameraStyle style, float aspect,
        float cameraDistance, float perspectiveFarDistance,
        float deltaTime) noexcept;

    void MoveDebugCamera(
        CameraStyle style, float forward, float right,
        float deltaTime) noexcept;
    void RotateDebugCamera(float deltaX, float deltaY) noexcept;
    void FlyTo(
        r3d::physics::Vec3 position, r3d::physics::Quat rotation,
        float time) noexcept;
    void StopFly() noexcept;
    bool InFly() const noexcept;

    static r3d::physics::Vec3 ScreenToWorld(
        const CameraFrame& frame, float viewportWidth,
        float viewportHeight, CameraScreenPoint point, float z) noexcept;
    static CameraScreenPoint WorldToScreen(
        const CameraFrame& frame, float viewportWidth,
        float viewportHeight, r3d::physics::Vec3 point) noexcept;
    static CameraRay ScreenToRay(
        const CameraFrame& frame, float viewportWidth,
        float viewportHeight, CameraScreenPoint point) noexcept;
    static bool ScreenPixelRayCastWithPlaneXY(
        const CameraFrame& frame, float viewportWidth,
        float viewportHeight, CameraScreenPoint point,
        r3d::physics::Vec3& intersection) noexcept;
    void Reset() noexcept;

    bool IsInitialized() const noexcept;
    CameraStyle GetStyle() const noexcept;
    const r3d::physics::Vec3& GetPosition() const noexcept;
    const r3d::physics::Vec3& GetDirection() const noexcept;
    const r3d::physics::Quat& GetRotation() const noexcept;

private:
    CameraFrame progressFly(float deltaTime) noexcept;

    r3d::physics::Vec3 cameraLead_;
    r3d::physics::Vec3 previousTarget_;
    r3d::physics::Vec3 position_;
    r3d::physics::Vec3 direction_{1.0F, 0.0F, 0.0F};
    r3d::physics::Vec3 jumpDirection_;
    r3d::physics::Quat rotation_;
    r3d::physics::Quat thirdPersonRotation_;
    CameraStyle style_ = CameraStyle::Isometric;
    float jumpDistance_ = 0.0F;
    float jumpSpeed_ = 0.0F;
    float thirdPersonPullback_ = 0.0F;
    CameraFrame lastFrame_;
    r3d::physics::Vec3 flyStartPosition_;
    r3d::physics::Quat flyStartRotation_;
    r3d::physics::Vec3 flyPosition_;
    r3d::physics::Quat flyRotation_;
    float flyCurrentTime_ = -1.0F;
    float flyTime_ = 0.0F;
    float flyAlpha_ = 0.0F;
    bool initialized_ = false;
    bool styleInitialized_ = false;
};

} // namespace r3d::game::originalrace::source
