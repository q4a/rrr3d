#include "OriginalCameraManager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

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

float dot(Vec3 first, Vec3 second) noexcept
{
    return first.x * second.x + first.y * second.y + first.z * second.z;
}

Quat angleAxis(float angle, Vec3 axis) noexcept
{
    axis = normalized(axis);
    if (length(axis) <= 0.0001F)
        return {};
    const float sine = std::sin(angle * 0.5F);
    return normalized({axis.x * sine, axis.y * sine, axis.z * sine,
                       std::cos(angle * 0.5F)});
}

} // namespace

void AutoObserver::PointerDown(float x, float y) noexcept
{
    leftDown_ = true;
    dragging_ = false;
    anchorX_ = lastX_ = x;
    anchorY_ = lastY_ = y;
}

void AutoObserver::PointerUp(float x, float y) noexcept
{
    leftDown_ = false;
    dragging_ = false;
    lastX_ = x;
    lastY_ = y;
}

void AutoObserver::PointerMove(float x, float y) noexcept
{
    if (!leftDown_)
    {
        anchorX_ = lastX_ = x;
        anchorY_ = lastY_ = y;
        return;
    }
    if (!dragging_ && std::hypot(x - anchorX_, y - anchorY_) > 15.0F)
        dragging_ = true;
    if (dragging_)
    {
        yaw_ += std::clamp(
            (x - lastX_) * pi * 0.001F, -pi * 0.5F, pi * 0.5F);
        pitch_ += std::clamp(
            -(y - lastY_) * pi * 0.001F, -pi * 0.5F, pi * 0.5F);
        idleSeconds_ = 0.0F;
    }
    lastX_ = x;
    lastY_ = y;
}

ObserverPose AutoObserver::OnFrame(
    const ObserverPose& source, float deltaTime,
    const ObserverConfig& config) noexcept
{
    deltaTime = std::max(deltaTime, 0.0F);
    if (!initialized_)
    {
        source_ = source;
        targetRotation_ = normalized(source.rotation);
        cameraRotation_ = targetRotation_;
        idleSeconds_ = 3.0F;
        direction_ = 1.0F;
        initialized_ = true;
    }
    if (!dragging_)
        idleSeconds_ += deltaTime;
    if (idleSeconds_ >= 3.0F)
    {
        yaw_ += config.angularSpeed * direction_ * deltaTime;
        pitch_ = 0.0F;
    }
    if (config.positiveYawClamp > 0.0F ||
        config.negativeYawClamp > 0.0F)
    {
        if (yaw_ >= config.positiveYawClamp)
        {
            yaw_ = config.positiveYawClamp;
            direction_ = -1.0F;
        }
        else if (yaw_ <= -config.negativeYawClamp)
        {
            yaw_ = -config.negativeYawClamp;
            direction_ = 1.0F;
        }
    }
    pitch_ = std::clamp(
        pitch_, config.minimumPitch - config.stablePitch,
        config.maximumPitch - config.stablePitch);
    const auto yawRotation = angleAxis(yaw_, {0.0F, 0.0F, 1.0F});
    const auto yawedSource = multiply(yawRotation, source_.rotation);
    const auto localY = rotate(yawedSource, {0.0F, 1.0F, 0.0F});
    targetRotation_ = multiply(
        angleAxis(pitch_, localY), yawedSource);
    cameraRotation_ = sourceSphericalMix(
        cameraRotation_, targetRotation_,
        std::clamp(6.0F * deltaTime, 0.0F, 1.0F));

    const float distance = length(source.position);
    const auto direction = rotate(
        cameraRotation_, {1.0F, 0.0F, 0.0F});
    return {{-direction.x * distance, -direction.y * distance,
             -direction.z * distance},
            cameraRotation_};
}

void AutoObserver::Reset() noexcept
{
    source_ = {};
    targetRotation_ = {};
    cameraRotation_ = {};
    yaw_ = 0.0F;
    pitch_ = 0.0F;
    direction_ = 1.0F;
    idleSeconds_ = 3.0F;
    anchorX_ = anchorY_ = lastX_ = lastY_ = 0.0F;
    initialized_ = false;
    leftDown_ = false;
    dragging_ = false;
}

bool AutoObserver::IsInitialized() const noexcept { return initialized_; }
bool AutoObserver::IsDragging() const noexcept { return dragging_; }
float AutoObserver::GetDirection() const noexcept { return direction_; }

CameraFrame CameraManager::OnFrame(
    const CameraTarget& target, CameraStyle style, float aspect,
    float cameraDistance, float perspectiveFarDistance,
    float deltaTime) noexcept
{
    aspect = std::max(aspect, 0.0001F);
    deltaTime = std::max(deltaTime, 0.0F);
    perspectiveFarDistance = std::max(perspectiveFarDistance, 1.0F);
    if (InFly() && initialized_)
        return progressFly(deltaTime);
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
        lastFrame_ = frame;
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
        lastFrame_ = frame;
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
    lastFrame_ = frame;
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

void CameraManager::FlyTo(
    Vec3 position, Quat rotation, float time) noexcept
{
    flyStartPosition_ = position_;
    flyStartRotation_ = rotation_;
    flyPosition_ = position;
    flyRotation_ = normalized(rotation);
    flyTime_ = std::max(time, 0.0001F);
    flyCurrentTime_ = 0.0F;
    flyAlpha_ = 0.0F;
}

void CameraManager::StopFly() noexcept { flyCurrentTime_ = -1.0F; }
bool CameraManager::InFly() const noexcept { return flyCurrentTime_ >= 0.0F; }

CameraFrame CameraManager::progressFly(float deltaTime) noexcept
{
    deltaTime = std::max(deltaTime, 0.0F);
    flyCurrentTime_ += deltaTime;
    const float elapsed = std::clamp(
        flyCurrentTime_, 0.0F, flyTime_);
    flyAlpha_ += std::max(
        (elapsed - flyAlpha_) * deltaTime * 4.0F, 0.001F);
    const float alpha = std::clamp(flyAlpha_ / flyTime_, 0.0F, 1.0F);
    position_ = {
        flyStartPosition_.x + (flyPosition_.x - flyStartPosition_.x) * alpha,
        flyStartPosition_.y + (flyPosition_.y - flyStartPosition_.y) * alpha,
        flyStartPosition_.z + (flyPosition_.z - flyStartPosition_.z) * alpha};
    rotation_ = sourceSphericalMix(
        flyStartRotation_, flyRotation_, alpha);
    direction_ = normalized(rotate(
        rotation_, {1.0F, 0.0F, 0.0F}));
    lastFrame_.position = position_;
    lastFrame_.rotation = rotation_;
    lastFrame_.direction = direction_;
    lastFrame_.up = normalized(rotate(
        rotation_, {0.0F, 0.0F, 1.0F}));
    if (alpha >= 1.0F)
        StopFly();
    return lastFrame_;
}

CameraFrame CameraManager::AdjustViewOrtho(
    const CameraFrame& frame, float aspect,
    Vec3 worldMinimum, Vec3 worldMaximum) noexcept
{
    if (frame.projection != CameraProjection::Orthographic ||
        frame.orthographicWidth <= 0.0F || aspect <= 0.0F ||
        worldMinimum.x > worldMaximum.x ||
        worldMinimum.y > worldMaximum.y ||
        worldMinimum.z > worldMaximum.z)
        return frame;

    const auto direction = normalized(frame.direction);
    const auto right = normalized(cross(direction, frame.up));
    const auto up = normalized(cross(right, direction));
    const float halfWidth = frame.orthographicWidth * 0.5F;
    const float halfHeight = halfWidth / aspect;
    const auto relative = [&](Vec3 p) {
        return Vec3{p.x - frame.position.x, p.y - frame.position.y,
                    p.z - frame.position.z};
    };
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();
    bool found = false;
    const auto include = [&](Vec3 p) {
        const auto v = relative(p);
        if (std::abs(dot(v, right)) <= halfWidth + 0.001F &&
            std::abs(dot(v, up)) <= halfHeight + 0.001F)
        {
            const float z = dot(v, direction);
            minZ = std::min(minZ, z);
            maxZ = std::max(maxZ, z);
            found = true;
        }
    };
    std::array<Vec3, 8U> corners;
    for (unsigned i = 0U; i < corners.size(); ++i)
    {
        corners[i] = {i & 1U ? worldMaximum.x : worldMinimum.x,
                      i & 2U ? worldMaximum.y : worldMinimum.y,
                      i & 4U ? worldMaximum.z : worldMinimum.z};
        include(corners[i]);
    }
    // CameraCI::ComputeZBounds: AABB corners, AABB edges crossing the four
    // lateral frustum planes, and the four infinite corner lines through
    // the AABB. Do not clip those lines at the *old* near/far planes.
    for (unsigned i = 0U; i < corners.size(); ++i)
    {
        for (unsigned bit : {1U, 2U, 4U})
        {
            if (i & bit)
                continue;
            const auto a = corners[i];
            const auto b = corners[i | bit];
            for (unsigned axis = 0U; axis < 2U; ++axis)
            {
                const auto normal = axis == 0U ? right : up;
                const float half = axis == 0U ? halfWidth : halfHeight;
                const float start = dot(relative(a), normal);
                const float end = dot(relative(b), normal);
                if (std::abs(end - start) <= 0.000001F)
                    continue;
                for (float sign : {-1.0F, 1.0F})
                {
                    const float t = (sign * half - start) / (end - start);
                    if (t >= 0.0F && t <= 1.0F)
                        include({a.x + (b.x - a.x) * t,
                                 a.y + (b.y - a.y) * t,
                                 a.z + (b.z - a.z) * t});
                }
            }
        }
    }
    for (float x : {-halfWidth, halfWidth})
        for (float y : {-halfHeight, halfHeight})
        {
            const Vec3 origin{
                frame.position.x + right.x * x + up.x * y,
                frame.position.y + right.y * x + up.y * y,
                frame.position.z + right.z * x + up.z * y};
            const std::array origins{origin.x, origin.y, origin.z};
            const std::array directions{direction.x, direction.y, direction.z};
            const std::array minimum{worldMinimum.x, worldMinimum.y, worldMinimum.z};
            const std::array maximum{worldMaximum.x, worldMaximum.y, worldMaximum.z};
            float near = std::numeric_limits<float>::lowest();
            float far = std::numeric_limits<float>::max();
            bool intersects = true;
            for (unsigned axis = 0U; axis < 3U; ++axis)
            {
                if (std::abs(directions[axis]) <= 0.000001F)
                {
                    if (origins[axis] < minimum[axis] ||
                        origins[axis] > maximum[axis])
                        intersects = false;
                    continue;
                }
                float a = (minimum[axis] - origins[axis]) / directions[axis];
                float b = (maximum[axis] - origins[axis]) / directions[axis];
                if (a > b)
                    std::swap(a, b);
                near = std::max(near, a);
                far = std::min(far, b);
            }
            if (intersects && near <= far)
            {
                minZ = std::min(minZ, near);
                maxZ = std::max(maxZ, far);
                found = true;
            }
        }
    if (!found)
        return frame;
    // eff9338 GraphManager.cpp:1996-2005. Moving only along the view
    // direction leaves orthographic XY and apparent object sizes unchanged.
    const float offset = std::min(minZ - frame.nearDistance, 0.0F);
    auto adjusted = frame;
    adjusted.position = {frame.position.x + direction.x * offset,
                         frame.position.y + direction.y * offset,
                         frame.position.z + direction.z * offset};
    adjusted.nearDistance = minZ - offset;
    adjusted.farDistance = std::max(maxZ, frame.farDistance) - offset;
    return adjusted;
}

Vec3 CameraManager::ScreenToWorld(
    const CameraFrame& frame, float viewportWidth,
    float viewportHeight, CameraScreenPoint point, float z) noexcept
{
    viewportWidth = std::max(viewportWidth, 1.0F);
    viewportHeight = std::max(viewportHeight, 1.0F);
    z = std::clamp(z, 0.0F, 1.0F);
    const float aspect = viewportWidth / viewportHeight;
    const float ndcX = point.x * 2.0F / viewportWidth - 1.0F;
    const float ndcY = 1.0F - point.y * 2.0F / viewportHeight;
    const auto direction = normalized(frame.direction);
    const auto right = normalized(rotate(
        frame.rotation, {0.0F, 1.0F, 0.0F}));
    const auto up = normalized(cross(
        normalized(cross(direction, frame.up)), direction));
    const float depth = frame.nearDistance +
                        (frame.farDistance - frame.nearDistance) * z;
    float halfWidth = frame.orthographicWidth * 0.5F;
    float halfHeight = halfWidth / aspect;
    if (frame.projection == CameraProjection::Perspective)
    {
        halfHeight = std::tan(
            frame.verticalFovDegrees * pi / 360.0F) * depth;
        halfWidth = halfHeight * aspect;
    }
    return {
        frame.position.x + direction.x * depth +
            right.x * ndcX * halfWidth + up.x * ndcY * halfHeight,
        frame.position.y + direction.y * depth +
            right.y * ndcX * halfWidth + up.y * ndcY * halfHeight,
        frame.position.z + direction.z * depth +
            right.z * ndcX * halfWidth + up.z * ndcY * halfHeight};
}

CameraScreenPoint CameraManager::WorldToScreen(
    const CameraFrame& frame, float viewportWidth,
    float viewportHeight, Vec3 point) noexcept
{
    viewportWidth = std::max(viewportWidth, 1.0F);
    viewportHeight = std::max(viewportHeight, 1.0F);
    const float aspect = viewportWidth / viewportHeight;
    const auto direction = normalized(frame.direction);
    const auto right = normalized(rotate(
        frame.rotation, {0.0F, 1.0F, 0.0F}));
    const auto up = normalized(cross(
        normalized(cross(direction, frame.up)), direction));
    const Vec3 relative{point.x - frame.position.x,
                        point.y - frame.position.y,
                        point.z - frame.position.z};
    const float depth = dot(relative, direction);
    float halfWidth = frame.orthographicWidth * 0.5F;
    float halfHeight = halfWidth / aspect;
    if (frame.projection == CameraProjection::Perspective)
    {
        halfHeight = std::tan(
            frame.verticalFovDegrees * pi / 360.0F) *
                     std::max(depth, 0.0001F);
        halfWidth = halfHeight * aspect;
    }
    const float ndcX = halfWidth > 0.0001F
                           ? dot(relative, right) / halfWidth
                           : 0.0F;
    const float ndcY = halfHeight > 0.0001F
                           ? dot(relative, up) / halfHeight
                           : 0.0F;
    return {(ndcX + 1.0F) * viewportWidth * 0.5F,
            (1.0F - ndcY) * viewportHeight * 0.5F};
}

CameraRay CameraManager::ScreenToRay(
    const CameraFrame& frame, float viewportWidth,
    float viewportHeight, CameraScreenPoint point) noexcept
{
    const auto origin = ScreenToWorld(
        frame, viewportWidth, viewportHeight, point, 0.0F);
    const auto end = ScreenToWorld(
        frame, viewportWidth, viewportHeight, point, 1.0F);
    const Vec3 direction{end.x - origin.x, end.y - origin.y,
                         end.z - origin.z};
    return {origin, normalized(direction)};
}

bool CameraManager::ScreenPixelRayCastWithPlaneXY(
    const CameraFrame& frame, float viewportWidth,
    float viewportHeight, CameraScreenPoint point,
    Vec3& intersection) noexcept
{
    const auto ray = ScreenToRay(
        frame, viewportWidth, viewportHeight, point);
    if (std::abs(ray.direction.z) <= 0.0001F)
        return false;
    const float distance = -ray.origin.z / ray.direction.z;
    if (distance < 0.0F)
        return false;
    intersection = {
        ray.origin.x + ray.direction.x * distance,
        ray.origin.y + ray.direction.y * distance, 0.0F};
    return true;
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
    lastFrame_ = {};
    flyStartPosition_ = {};
    flyStartRotation_ = {};
    flyPosition_ = {};
    flyRotation_ = {};
    flyCurrentTime_ = -1.0F;
    flyTime_ = 0.0F;
    flyAlpha_ = 0.0F;
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
