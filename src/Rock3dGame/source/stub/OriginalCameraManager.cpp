#include "OriginalCameraManager.h"

#include <algorithm>
#include <cmath>

namespace r3d::game::originalrace::source
{
namespace
{

using r3d::physics::Quat;
using r3d::physics::Vec3;

constexpr float pi = 3.14159265358979323846F;

Vec3 normalized(Vec3 value) noexcept
{
    const float length = std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z);
    if (length <= 0.0001F)
        return {};
    return {value.x / length, value.y / length, value.z / length};
}

Quat normalized(Quat value) noexcept
{
    const float length = std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z +
        value.w * value.w);
    if (length <= 0.0001F)
        return {};
    return {value.x / length, value.y / length, value.z / length,
            value.w / length};
}

Vec3 cross(Vec3 first, Vec3 second) noexcept
{
    return {first.y * second.z - first.z * second.y,
            first.z * second.x - first.x * second.z,
            first.x * second.y - first.y * second.x};
}

Quat multiply(Quat first, Quat second) noexcept
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

Vec3 rotate(Quat rotation, Vec3 value) noexcept
{
    rotation = normalized(rotation);
    const Vec3 twiceCross{
        2.0F * (rotation.y * value.z - rotation.z * value.y),
        2.0F * (rotation.z * value.x - rotation.x * value.z),
        2.0F * (rotation.x * value.y - rotation.y * value.x)};
    return {value.x + rotation.w * twiceCross.x +
                        (rotation.y * twiceCross.z -
                         rotation.z * twiceCross.y),
            value.y + rotation.w * twiceCross.y +
                        (rotation.z * twiceCross.x -
                         rotation.x * twiceCross.z),
            value.z + rotation.w * twiceCross.z +
                        (rotation.x * twiceCross.y -
                         rotation.y * twiceCross.x)};
}

Quat axesRotation(Vec3 xAxis, Vec3 yAxis, Vec3 zAxis) noexcept
{
    xAxis = normalized(xAxis);
    yAxis = normalized(yAxis);
    zAxis = normalized(zAxis);
    const float m00 = xAxis.x;
    const float m01 = yAxis.x;
    const float m02 = zAxis.x;
    const float m10 = xAxis.y;
    const float m11 = yAxis.y;
    const float m12 = zAxis.y;
    const float m20 = xAxis.z;
    const float m21 = yAxis.z;
    const float m22 = zAxis.z;
    Quat result;
    const float trace = m00 + m11 + m22;
    if (trace > 0.0F)
    {
        const float scale = std::sqrt(trace + 1.0F) * 2.0F;
        result.w = 0.25F * scale;
        result.x = (m21 - m12) / scale;
        result.y = (m02 - m20) / scale;
        result.z = (m10 - m01) / scale;
    }
    else if (m00 > m11 && m00 > m22)
    {
        const float scale =
            std::sqrt(1.0F + m00 - m11 - m22) * 2.0F;
        result.w = (m21 - m12) / scale;
        result.x = 0.25F * scale;
        result.y = (m01 + m10) / scale;
        result.z = (m02 + m20) / scale;
    }
    else if (m11 > m22)
    {
        const float scale =
            std::sqrt(1.0F + m11 - m00 - m22) * 2.0F;
        result.w = (m02 - m20) / scale;
        result.x = (m01 + m10) / scale;
        result.y = 0.25F * scale;
        result.z = (m12 + m21) / scale;
    }
    else
    {
        const float scale =
            std::sqrt(1.0F + m22 - m00 - m11) * 2.0F;
        result.w = (m10 - m01) / scale;
        result.x = (m02 + m20) / scale;
        result.y = (m12 + m21) / scale;
        result.z = 0.25F * scale;
    }
    return normalized(result);
}

Quat sourceSphericalMix(Quat first, Quat second, float amount) noexcept
{
    first = normalized(first);
    second = normalized(second);
    float cosine = first.x * second.x + first.y * second.y +
                   first.z * second.z + first.w * second.w;
    if (cosine < 0.0F)
    {
        cosine = -cosine;
        second = {-second.x, -second.y, -second.z, -second.w};
    }
    cosine = std::clamp(cosine, -1.0F, 1.0F);
    if (cosine > 0.9995F)
    {
        return normalized({
            first.x + (second.x - first.x) * amount,
            first.y + (second.y - first.y) * amount,
            first.z + (second.z - first.z) * amount,
            first.w + (second.w - first.w) * amount});
    }
    const float angle = std::acos(cosine);
    const float sine = std::sin(angle);
    const float firstWeight =
        std::sin((1.0F - amount) * angle) / sine;
    const float secondWeight = std::sin(amount * angle) / sine;
    return normalized({
        first.x * firstWeight + second.x * secondWeight,
        first.y * firstWeight + second.y * secondWeight,
        first.z * firstWeight + second.z * secondWeight,
        first.w * firstWeight + second.w * secondWeight});
}

float length(Vec3 value) noexcept
{
    return std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z);
}

} // namespace

CameraFrame CameraManager::OnFrame(
    const CameraTarget& target, CameraStyle style, float aspect,
    float cameraDistance, float perspectiveFarDistance,
    float deltaTime) noexcept
{
    aspect = std::max(aspect, 0.0001F);
    deltaTime = std::max(deltaTime, 0.0F);
    perspectiveFarDistance = std::max(perspectiveFarDistance, 1.0F);
    const auto bodyRotation = normalized(target.rotation);
    const auto carForward = normalized(
        rotate(bodyRotation, {1.0F, 0.0F, 0.0F}));
    auto isometricForward = carForward;
    isometricForward.z = 0.0F;
    isometricForward = normalized(isometricForward);

    // Original CameraManager suppresses backwards body motion while the
    // first non-lead driven wheel is stopped or rotating in reverse.
    auto targetVelocity = target.linearVelocity;
    if (target.drivenWheelSpeed < 0.1F)
    {
        const Quat inverseBody{
            -bodyRotation.x, -bodyRotation.y, -bodyRotation.z,
            bodyRotation.w};
        auto localVelocity = rotate(inverseBody, targetVelocity);
        localVelocity.x = std::max(localVelocity.x, 0.0F);
        targetVelocity = rotate(bodyRotation, localVelocity);
    }
    auto velocityForward = normalized(Vec3{
        carForward.x + targetVelocity.x * 0.1F,
        carForward.y + targetVelocity.y * 0.1F,
        carForward.z + targetVelocity.z * 0.1F});
    if (length(velocityForward) <= 0.0001F)
        velocityForward = carForward;

    if (!styleInitialized_)
    {
        style_ = style;
        styleInitialized_ = true;
    }
    else if (style_ != style)
    {
        if (style == CameraStyle::Isometric)
        {
            cameraLead_ = {};
            previousTarget_ = target.position;
            jumpDirection_ = {};
            jumpDistance_ = 0.0F;
            jumpSpeed_ = 0.0F;
        }
        else
        {
            // CameraManager::ChangeStyle retains the current camera pose;
            // ThirdPerson slerps from it on the first new frame.
            thirdPersonRotation_ = rotation_;
            thirdPersonPullback_ = 0.0F;
        }
        style_ = style;
    }

    CameraFrame frame;
    if (style == CameraStyle::Isometric)
    {
        const float elevation = 15.5F * pi / 180.0F;
        const float azimuth = 45.0F * pi / 180.0F;
        const Quat rotationY{
            0.0F, std::sin(elevation * 0.5F), 0.0F,
            std::cos(elevation * 0.5F)};
        const Quat rotationZ{
            0.0F, 0.0F, std::sin(azimuth * 0.5F),
            std::cos(azimuth * 0.5F)};
        const auto isoRotation = multiply(rotationZ, rotationY);
        const Quat inverseIso{
            -isoRotation.x, -isoRotation.y, -isoRotation.z,
            isoRotation.w};
        const auto isoDirection = normalized(
            rotate(isoRotation, {1.0F, 0.0F, 0.0F}));

        auto localDirection = rotate(inverseIso, isometricForward);
        Vec3 projected{localDirection.y, localDirection.z, 0.0F};
        const float projectedLength = std::sqrt(
            projected.x * projected.x + projected.y * projected.y);
        if (projectedLength > 0.0001F)
        {
            projected.x /= projectedLength;
            projected.y /= projectedLength;
        }
        const float yTargetDot = projected.y;
        const float cameraWidth = 28.0F * cameraDistance;
        const float halfWidth = cameraWidth * 0.5F;
        const float halfHeight = halfWidth / aspect;
        const float cameraSize =
            std::sqrt(halfWidth * halfWidth + halfHeight * halfHeight);
        projected.x *= cameraSize;
        projected.y *= cameraSize;
        constexpr float border = 5.0F;
        if (std::abs(projected.y) > 0.1F &&
            std::abs(projected.x / projected.y) < aspect &&
            std::abs(yTargetDot) > 0.0001F)
        {
            const float y = std::clamp(
                projected.y, -border / aspect, border / aspect);
            const float radius = std::abs(y / yTargetDot);
            float x = std::sqrt(
                std::max(radius * radius - y * y, 0.0F));
            if (projected.x < 0.0F)
                x = -x;
            projected = {x, y, 0.0F};
        }
        else
        {
            const float x = std::clamp(projected.x, -border, border);
            const float denominator = std::sqrt(
                std::max(1.0F - yTargetDot * yTargetDot, 0.0001F));
            const float radius = std::abs(x) / denominator;
            float y = std::sqrt(
                std::max(radius * radius - x * x, 0.0F));
            if (projected.y < 0.0F)
                y = -y;
            projected = {x, y, 0.0F};
        }
        const auto desiredLead = rotate(
            isoRotation, {0.0F, projected.x, projected.y});
        const float blend = std::clamp(deltaTime, 0.0F, 1.0F);
        cameraLead_.x += (desiredLead.x - cameraLead_.x) * blend;
        cameraLead_.y += (desiredLead.y - cameraLead_.y) * blend;
        cameraLead_.z += (desiredLead.z - cameraLead_.z) * blend;

        if (!initialized_)
        {
            previousTarget_ = target.position;
            initialized_ = true;
        }
        const Vec3 jump{
            target.position.x - previousTarget_.x,
            target.position.y - previousTarget_.y,
            target.position.z - previousTarget_.z};
        const float jumpLength = length(jump);
        if (jumpLength > 6.0F)
        {
            if (jumpDistance_ == 0.0F)
                jumpSpeed_ = jumpLength / 0.5F;
            jumpDistance_ = jumpLength;
            jumpDirection_ = {
                jump.x / jumpLength, jump.y / jumpLength,
                jump.z / jumpLength};
        }
        previousTarget_ = target.position;
        jumpDistance_ = std::max(
            jumpDistance_ - jumpSpeed_ * deltaTime, 0.0F);
        const Vec3 cameraTarget{
            target.position.x + cameraLead_.x -
                jumpDirection_.x * jumpDistance_,
            target.position.y + cameraLead_.y -
                jumpDirection_.y * jumpDistance_,
            target.position.z + cameraLead_.z -
                jumpDirection_.z * jumpDistance_};
        position_ = {
            cameraTarget.x - isoDirection.x * 20.0F,
            cameraTarget.y - isoDirection.y * 20.0F,
            cameraTarget.z - isoDirection.z * 20.0F};
        direction_ = isoDirection;
        rotation_ = isoRotation;
        frame.position = position_;
        frame.direction = direction_;
        frame.rotation = rotation_;
        frame.projection = CameraProjection::Orthographic;
        frame.orthographicWidth = cameraWidth;
        frame.farDistance = 150.0F;
        frame.pointSpriteScale = 0.75F;
        return frame;
    }

    if (style != CameraStyle::ThirdPerson)
    {
        const float elevation = 15.5F * pi / 180.0F;
        const float azimuth = 45.0F * pi / 180.0F;
        const Quat rotationY{
            0.0F, std::sin(elevation * 0.5F), 0.0F,
            std::cos(elevation * 0.5F)};
        const Quat rotationZ{
            0.0F, 0.0F, std::sin(azimuth * 0.5F),
            std::cos(azimuth * 0.5F)};
        const auto isoRotation = multiply(rotationZ, rotationY);
        if (!initialized_)
        {
            direction_ = normalized(
                rotate(isoRotation, {1.0F, 0.0F, 0.0F}));
            position_ = {
                target.position.x - direction_.x * 20.0F,
                target.position.y - direction_.y * 20.0F,
                target.position.z - direction_.z * 20.0F};
            rotation_ = isoRotation;
            initialized_ = true;
        }
        if (style == CameraStyle::IsometricView)
        {
            rotation_ = isoRotation;
            direction_ = normalized(
                rotate(isoRotation, {1.0F, 0.0F, 0.0F}));
            position_.z = std::max(position_.z, 10.0F);
        }
        frame.position = position_;
        frame.direction = direction_;
        frame.up = normalized(
            rotate(rotation_, {0.0F, 0.0F, 1.0F}));
        frame.rotation = rotation_;
        frame.verticalFovDegrees = 90.0F;
        frame.farDistance = style == CameraStyle::IsometricView
                                ? 150.0F
                                : perspectiveFarDistance;
        if (style == CameraStyle::IsometricView)
        {
            frame.projection = CameraProjection::Orthographic;
            frame.orthographicWidth = 28.0F * cameraDistance;
            frame.pointSpriteScale = 0.75F;
        }
        return frame;
    }

    const float velocityLength = length(targetVelocity);
    const float speedFactor = std::clamp(
        velocityLength / (150.0F / 3.6F), 0.0F, 1.0F);
    auto yAxis = normalized(cross({0.0F, 0.0F, 1.0F}, velocityForward));
    if (length(yAxis) <= 0.0001F)
        yAxis = normalized(rotate(bodyRotation, {0.0F, 1.0F, 0.0F}));
    const auto zAxis = normalized(cross(velocityForward, yAxis));
    const auto desiredRotation =
        axesRotation(velocityForward, yAxis, zAxis);
    if (!initialized_)
        thirdPersonRotation_ = desiredRotation;
    thirdPersonRotation_ = sourceSphericalMix(
        thirdPersonRotation_, desiredRotation, 6.0F * deltaTime);
    const float pullbackTarget = speedFactor * speedFactor * 1.5F;
    thirdPersonPullback_ +=
        (pullbackTarget - thirdPersonPullback_) *
        std::clamp(deltaTime * 5.0F, 0.0F, 1.0F);
    direction_ = normalized(rotate(
        thirdPersonRotation_, {1.0F, 0.0F, 0.0F}));
    const auto cameraOffset = rotate(
        thirdPersonRotation_, {-5.6F, 0.0F, 2.4F});
    position_ = {
        target.position.x + cameraOffset.x -
            direction_.x * thirdPersonPullback_,
        target.position.y + cameraOffset.y -
            direction_.y * thirdPersonPullback_,
        target.position.z + cameraOffset.z -
            direction_.z * thirdPersonPullback_};
    rotation_ = thirdPersonRotation_;
    previousTarget_ = target.position;
    initialized_ = true;
    frame.position = position_;
    frame.direction = direction_;
    frame.up = normalized(
        rotate(rotation_, {0.0F, 0.0F, 1.0F}));
    frame.rotation = rotation_;
    frame.farDistance = perspectiveFarDistance;
    return frame;
}

void CameraManager::MoveDebugCamera(
    CameraStyle style, float forward, float right,
    float deltaTime) noexcept
{
    if (!initialized_)
        return;
    const float step = 20.0F * std::clamp(deltaTime, 0.0F, 0.25F);
    const auto moveDirection =
        style == CameraStyle::IsometricView
            ? normalized(Vec3{1.0F, 1.0F, 0.0F})
            : direction_;
    const auto moveRight =
        style == CameraStyle::IsometricView
            ? normalized(Vec3{-1.0F, 1.0F, 0.0F})
            : normalized(rotate(rotation_, {0.0F, 1.0F, 0.0F}));
    position_.x +=
        moveDirection.x * forward * step + moveRight.x * right * step;
    position_.y +=
        moveDirection.y * forward * step + moveRight.y * right * step;
    position_.z +=
        moveDirection.z * forward * step + moveRight.z * right * step;
}

void CameraManager::RotateDebugCamera(
    float deltaX, float deltaY) noexcept
{
    if (!initialized_)
        return;
    const auto right = normalized(
        rotate(rotation_, {0.0F, 1.0F, 0.0F}));
    const float yawAngle = -0.005F * deltaX;
    const float pitchAngle = 0.005F * deltaY;
    const Quat yaw{0.0F, 0.0F, std::sin(yawAngle * 0.5F),
                   std::cos(yawAngle * 0.5F)};
    const float pitchSin = std::sin(pitchAngle * 0.5F);
    const Quat pitch{
        right.x * pitchSin, right.y * pitchSin,
        right.z * pitchSin, std::cos(pitchAngle * 0.5F)};
    rotation_ = normalized(multiply(multiply(yaw, pitch), rotation_));
    direction_ = normalized(rotate(
        rotation_, {1.0F, 0.0F, 0.0F}));
}

void CameraManager::Reset() noexcept
{
    cameraLead_ = {};
    previousTarget_ = {};
    position_ = {};
    direction_ = {1.0F, 0.0F, 0.0F};
    jumpDirection_ = {};
    rotation_ = {};
    thirdPersonRotation_ = {};
    style_ = CameraStyle::Isometric;
    jumpDistance_ = 0.0F;
    jumpSpeed_ = 0.0F;
    thirdPersonPullback_ = 0.0F;
    initialized_ = false;
    styleInitialized_ = false;
}

bool CameraManager::IsInitialized() const noexcept { return initialized_; }
CameraStyle CameraManager::GetStyle() const noexcept { return style_; }
const r3d::physics::Vec3& CameraManager::GetPosition() const noexcept
{
    return position_;
}
const r3d::physics::Vec3& CameraManager::GetDirection() const noexcept
{
    return direction_;
}
const r3d::physics::Quat& CameraManager::GetRotation() const noexcept
{
    return rotation_;
}

} // namespace r3d::game::originalrace::source
