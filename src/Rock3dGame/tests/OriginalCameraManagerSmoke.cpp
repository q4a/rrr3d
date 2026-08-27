#include "OriginalCameraManager.h"

#include <cmath>
#include <iostream>

namespace source = r3d::game::originalrace::source;

namespace
{

bool near(float first, float second, float tolerance = 0.001F)
{
    return std::abs(first - second) <= tolerance;
}

float distance(r3d::physics::Vec3 first,
               r3d::physics::Vec3 second)
{
    const float x = first.x - second.x;
    const float y = first.y - second.y;
    const float z = first.z - second.z;
    return std::sqrt(x * x + y * y + z * z);
}

} // namespace

int main()
{
    source::CameraManager camera;
    source::CameraTarget target;
    target.position = {10.0F, 20.0F, 1.0F};
    target.rotation = {};
    target.linearVelocity = {-10.0F, 0.0F, 0.0F};
    target.drivenWheelSpeed = 0.0F;

    const auto thirdPerson = camera.OnFrame(
        target, source::CameraStyle::ThirdPerson, 16.0F / 9.0F,
        1.25F, 120.0F, 1.0F / 60.0F);
    if (!camera.IsInitialized() ||
        camera.GetStyle() != source::CameraStyle::ThirdPerson ||
        thirdPerson.projection !=
            source::CameraProjection::Perspective ||
        !near(thirdPerson.verticalFovDegrees, 75.0F) ||
        !near(thirdPerson.farDistance, 120.0F) ||
        thirdPerson.direction.x < 0.999F ||
        !near(thirdPerson.position.x, 4.4F) ||
        !near(thirdPerson.position.y, 20.0F) ||
        !near(thirdPerson.position.z, 3.4F))
        return 1;

    camera.Reset();
    target.linearVelocity = {};
    const auto isometric = camera.OnFrame(
        target, source::CameraStyle::Isometric, 16.0F / 9.0F,
        1.25F, 120.0F, 1.0F / 60.0F);
    if (isometric.projection !=
            source::CameraProjection::Orthographic ||
        !near(isometric.orthographicWidth, 35.0F) ||
        !near(isometric.nearDistance, 1.0F) ||
        !near(isometric.farDistance, 150.0F) ||
        !near(isometric.pointSpriteScale, 0.75F) ||
        !near(distance(isometric.position, {
            target.position.x +
                (isometric.position.x +
                 isometric.direction.x * 20.0F - target.position.x),
            target.position.y +
                (isometric.position.y +
                 isometric.direction.y * 20.0F - target.position.y),
            target.position.z +
                (isometric.position.z +
                 isometric.direction.z * 20.0F - target.position.z)}),
            20.0F))
        return 2;

    const auto beforeTeleport = isometric.position;
    target.position.x += 100.0F;
    const auto teleported = camera.OnFrame(
        target, source::CameraStyle::Isometric, 16.0F / 9.0F,
        1.25F, 120.0F, 0.0F);
    if (distance(beforeTeleport, teleported.position) > 0.001F)
        return 3;
    const auto caughtUp = camera.OnFrame(
        target, source::CameraStyle::Isometric, 16.0F / 9.0F,
        1.25F, 120.0F, 0.5F);
    if (caughtUp.position.x - teleported.position.x < 90.0F)
        return 4;

    camera.Reset();
    const auto freeView = camera.OnFrame(
        target, source::CameraStyle::FreeView, 16.0F / 9.0F,
        1.25F, 220.0F, 0.0F);
    const auto retainedPosition = freeView.position;
    camera.MoveDebugCamera(
        source::CameraStyle::FreeView, 1.0F, 0.0F, 0.25F);
    if (distance(retainedPosition, camera.GetPosition()) < 4.9F)
        return 5;
    const auto direction = camera.GetDirection();
    camera.RotateDebugCamera(20.0F, 10.0F);
    if (distance(direction, camera.GetDirection()) < 0.01F)
        return 6;

    const auto flyStart = camera.GetPosition();
    const r3d::physics::Vec3 flyTarget{
        flyStart.x + 20.0F, flyStart.y - 4.0F,
        flyStart.z + 3.0F};
    camera.FlyTo(flyTarget, {}, 0.5F);
    if (!camera.InFly())
        return 7;
    auto flying = camera.OnFrame(
        target, source::CameraStyle::FreeView, 16.0F / 9.0F,
        1.25F, 220.0F, 0.25F);
    if (!camera.InFly() ||
        distance(flying.position, flyStart) <= 0.001F ||
        distance(flying.position, flyTarget) <= 0.001F)
        return 8;
    for (int frame = 0; frame < 20 && camera.InFly(); ++frame)
    {
        flying = camera.OnFrame(
            target, source::CameraStyle::FreeView, 16.0F / 9.0F,
            1.25F, 220.0F, 0.1F);
    }
    if (camera.InFly() || distance(flying.position, flyTarget) > 0.001F)
        return 9;

    source::CameraFrame projection;
    projection.position = {0.0F, 0.0F, 10.0F};
    projection.direction = {1.0F, 0.0F, -1.0F};
    const float directionLength = std::sqrt(2.0F);
    projection.direction.x /= directionLength;
    projection.direction.z /= directionLength;
    projection.up = {1.0F / directionLength, 0.0F,
                     1.0F / directionLength};
    projection.rotation = {
        0.0F, std::sin(3.14159265358979323846F / 8.0F), 0.0F,
        std::cos(3.14159265358979323846F / 8.0F)};
    projection.nearDistance = 1.0F;
    projection.farDistance = 101.0F;
    const source::CameraScreenPoint screen{640.0F, 400.0F};
    const auto world = source::CameraManager::ScreenToWorld(
        projection, 1280.0F, 800.0F, screen, 0.25F);
    const auto roundTrip = source::CameraManager::WorldToScreen(
        projection, 1280.0F, 800.0F, world);
    const auto ray = source::CameraManager::ScreenToRay(
        projection, 1280.0F, 800.0F, screen);
    r3d::physics::Vec3 planeIntersection;
    if (!near(roundTrip.x, screen.x, 0.01F) ||
        !near(roundTrip.y, screen.y, 0.01F) ||
        distance(ray.direction, projection.direction) > 0.001F ||
        !source::CameraManager::ScreenPixelRayCastWithPlaneXY(
            projection, 1280.0F, 800.0F, screen,
            planeIntersection) ||
        !near(planeIntersection.z, 0.0F))
        return 10;

    source::AutoObserver observer;
    source::ObserverPose observerSource{
        {-12.0F, 0.0F, 3.0F}, {}};
    source::ObserverConfig observerConfig;
    observerConfig.angularSpeed = 1.0F;
    observerConfig.positiveYawClamp = 0.2F;
    observerConfig.negativeYawClamp = 0.1F;
    const auto observerStart = observer.OnFrame(
        observerSource, 0.0F, observerConfig);
    const auto observerTurned = observer.OnFrame(
        observerSource, 0.25F, observerConfig);
    if (!observer.IsInitialized() || observer.IsDragging() ||
        observer.GetDirection() >= 0.0F ||
        distance(observerStart.position, observerTurned.position) < 0.1F)
        return 11;
    observer.PointerDown(100.0F, 100.0F);
    observer.PointerMove(108.0F, 108.0F);
    if (observer.IsDragging())
        return 12;
    observer.PointerMove(130.0F, 100.0F);
    if (!observer.IsDragging())
        return 13;
    observer.PointerUp(130.0F, 100.0F);
    if (observer.IsDragging())
        return 14;
    observer.Reset();
    if (observer.IsInitialized())
        return 15;

    camera.Reset();
    if (camera.IsInitialized())
        return 16;

    std::cout << "original CameraManager race/observer/fly/ray state passed\n";
    return 0;
}
