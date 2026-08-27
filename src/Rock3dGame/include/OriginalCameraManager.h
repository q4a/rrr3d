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

// Gameplay owner transcribed from CameraManager.cpp. Window coordinates,
// ray casts and matrix construction remain at their platform/renderer
// boundaries; race camera state, interpolation and style transitions live
// here exactly once.
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
    void Reset() noexcept;

    bool IsInitialized() const noexcept;
    CameraStyle GetStyle() const noexcept;
    const r3d::physics::Vec3& GetPosition() const noexcept;
    const r3d::physics::Vec3& GetDirection() const noexcept;
    const r3d::physics::Quat& GetRotation() const noexcept;

private:
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
    bool initialized_ = false;
    bool styleInitialized_ = false;
};

} // namespace r3d::game::originalrace::source
