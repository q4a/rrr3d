#include "OriginalAICar.h"

#include <algorithm>
#include <cmath>

namespace r3d::game::originalrace::source
{

namespace
{

constexpr float pi = 3.14159265358979323846F;
constexpr float steerAngleBias = pi / 128.0F;
constexpr float maximumSpeedBlocking = 0.5F;
constexpr float maximumTimeBlocking = 1.0F;
constexpr float epsilon = 0.0001F;

TraceVec2 xy(const TraceVec3& value) noexcept
{
    return {value.x, value.y};
}

TraceVec2 add(TraceVec2 first, TraceVec2 second) noexcept
{
    return {first.x + second.x, first.y + second.y};
}

TraceVec2 subtract(TraceVec2 first, TraceVec2 second) noexcept
{
    return {first.x - second.x, first.y - second.y};
}

TraceVec2 multiply(TraceVec2 value, float scale) noexcept
{
    return {value.x * scale, value.y * scale};
}

float dot(TraceVec2 first, TraceVec2 second) noexcept
{
    return first.x * second.x + first.y * second.y;
}

float cross(TraceVec2 first, TraceVec2 second) noexcept
{
    return first.x * second.y - first.y * second.x;
}

float length(TraceVec2 value) noexcept
{
    return std::sqrt(dot(value, value));
}

TraceVec2 normalized(TraceVec2 value) noexcept
{
    const float magnitude = length(value);
    return magnitude > epsilon ? multiply(value, 1.0F / magnitude)
                               : TraceVec2{};
}

float lineDistance(TraceVec2 normal, TraceVec2 point,
                   TraceVec2 value) noexcept
{
    return dot(normal, subtract(value, point));
}

} // namespace

AICar::PathState::PathState(std::uint32_t trackCount)
{
    Reset(trackCount);
}

void AICar::PathState::Reset(std::uint32_t trackCount)
{
    trackCount_ = std::max(trackCount, 1U);
    curTile = nullptr;
    nextTile = nullptr;
    curNode = nullptr;
    dirArea = 0.0F;
    moveDir = {};
    brake = false;
    freeTracks.assign(trackCount_, true);
    lockTracks.assign(trackCount_, false);
}

void AICar::PathState::ResetTracks()
{
    std::fill(freeTracks.begin(), freeTracks.end(), true);
    std::fill(lockTracks.begin(), lockTracks.end(), false);
}

void AICar::PathState::LockTrack(std::uint32_t track)
{
    if (track >= trackCount_)
        return;
    freeTracks[track] = false;
    lockTracks[track] = true;
}

bool AICar::PathState::FindFirstUnlockTrack(
    std::uint32_t track, std::uint32_t target,
    std::uint32_t& result) const
{
    if (track >= trackCount_ || target >= trackCount_)
        return false;
    const int increment = target > track ? 1 : -1;
    const int distance = std::abs(static_cast<int>(track) -
                                  static_cast<int>(target));
    for (int index = 1; index < distance + 1; ++index)
    {
        const auto testTrack = static_cast<std::uint32_t>(
            static_cast<int>(track) + index * increment);
        if (!lockTracks[testTrack])
        {
            result = testTrack;
            return true;
        }
    }
    return false;
}

bool AICar::PathState::FindLastUnlockTrack(
    std::uint32_t track, std::uint32_t target,
    std::uint32_t& result) const
{
    if (track >= trackCount_ || target >= trackCount_)
        return false;
    const int increment = target > track ? 1 : -1;
    const int distance = std::abs(static_cast<int>(track) -
                                  static_cast<int>(target));
    for (int index = 1; index < distance + 1; ++index)
    {
        const auto testTrack = static_cast<std::uint32_t>(
            static_cast<int>(track) + index * increment);
        if (lockTracks[testTrack])
            return index > 1;
        result = testTrack;
    }
    return track != target;
}

bool AICar::PathState::FindFirstSiblingUnlock(
    std::uint32_t track, std::uint32_t& result) const
{
    if (track >= trackCount_)
        return false;
    const int down = static_cast<int>(track) - 1;
    if (down >= 0 && !lockTracks[static_cast<std::size_t>(down)])
    {
        result = static_cast<std::uint32_t>(down);
        return true;
    }
    const std::uint32_t up = track + 1U;
    if (up < trackCount_ && !lockTracks[up])
    {
        result = up;
        return true;
    }
    return false;
}

bool AICar::PathState::FindLastSiblingUnlock(
    std::uint32_t track, std::uint32_t& result) const
{
    if (track >= trackCount_)
        return false;
    for (std::uint32_t index = 1U; index < trackCount_; ++index)
    {
        const int down = static_cast<int>(track) -
                         static_cast<int>(index);
        if (down >= 0 && !lockTracks[static_cast<std::size_t>(down)])
        {
            result = static_cast<std::uint32_t>(down);
            return true;
        }
        const std::uint32_t up = track + index;
        if (up < trackCount_ && !lockTracks[up])
        {
            result = up;
            return true;
        }
    }
    return false;
}

void AICar::PathState::ComputeMovDir(
    const Player::CarState& car, const VehicleState& vehicle)
{
    if (curTile == nullptr || nextTile == nullptr)
        return;

    std::uint32_t newTrack =
        std::min(car.GetTrack(), trackCount_ - 1U);
    const bool onLeft =
        cross(curTile->GetTile().GetDir(),
              nextTile->GetTile().GetDir()) > 0.0F;
    WayNode* movementNode = curTile;
    dirArea = 5.0F + std::abs(vehicle.speed) *
                         vehicle.steeringControl * 10.0F;

    if (nextTile->GetTile().GetTurnAngle() > pi / 12.0F)
    {
        const auto edgeNormal = nextTile->GetTile().GetEdgeNorm();
        const auto edgePoint = add(
            nextTile->GetPos2(),
            multiply(edgeNormal,
                     nextTile->GetTile().GetNodeRadius()));
        const float edgeDistance = lineDistance(
            edgeNormal, edgePoint, xy(vehicle.position));
        if (edgeDistance < vehicle.size)
        {
            const float tileWidth =
                nextTile->GetPoint()->GetSize() /
                static_cast<float>(trackCount_);
            const TraceVec2 targetPoint = subtract(
                add(nextTile->GetPos2(),
                    multiply(edgeNormal,
                             tileWidth *
                                 static_cast<float>(trackCount_) /
                                 2.0F)),
                xy(vehicle.position));
            const float projection = dot(
                curTile->GetTile().GetDir(), targetPoint);
            if (projection < vehicle.size)
                movementNode = nextTile;
            else
            {
                dirArea = projection;
                newTrack = onLeft ? 0U : trackCount_ - 1U;
            }
        }
    }

    WayNode* currentBrakeTile = curNode != nullptr ? curNode : nextTile;
    if (currentBrakeTile->GetTile().GetTurnAngle() > pi / 12.0F)
    {
        const float longCoefficient =
            vehicle.steeringControl * vehicle.steeringControl;
        const float normalDistance = lineDistance(
            currentBrakeTile->GetTile().GetMidDir(),
            currentBrakeTile->GetPos2(), xy(vehicle.position));
        const float brakeDistance =
            -normalDistance + currentBrakeTile->GetPoint()->GetSize();
        float rotation = dot(
            xy(vehicle.direction),
            currentBrakeTile->GetTile().GetDir());
        rotation = 1.0F - std::max(rotation, 0.0F);
        const float demand = vehicle.speed * vehicle.speed *
                             longCoefficient * rotation;
        if (!brake && demand > 1.5F * brakeDistance)
            brake = true;
        else if (brake && demand < brakeDistance)
            brake = false;
    }
    else
    {
        brake = false;
    }

    std::uint32_t selectedTrack =
        std::min(car.GetTrack(), trackCount_ - 1U);
    if (FindLastUnlockTrack(selectedTrack, newTrack, selectedTrack))
    {
        // The source intentionally keeps the last continuous unlocked lane.
    }
    else if (lockTracks[std::min(car.GetTrack(), trackCount_ - 1U)])
    {
        FindFirstSiblingUnlock(
            std::min(car.GetTrack(), trackCount_ - 1U), selectedTrack);
    }

    const TraceVec2 direction = movementNode->GetTile().GetDir();
    TraceVec2 target = add(
        xy(vehicle.position), multiply(direction, dirArea));
    target = add(
        target,
        movementNode->GetTile().ComputeTrackNormOff(
            target, selectedTrack));
    moveDir = normalized(subtract(target, xy(vehicle.position)));
}

void AICar::PathState::Update(
    float deltaTime, const Player::CarState& car,
    const VehicleState& vehicle, RandomSource randomSource)
{
    (void)deltaTime;
    WayNode* liveTile = const_cast<WayNode*>(car.GetLiveTile());
    if (liveTile != curTile)
    {
        WayNode* lastNode = const_cast<WayNode*>(car.GetLastNode());
        if (lastNode != nullptr &&
            (liveTile == nullptr ||
             (lastNode->GetNext() != liveTile &&
              lastNode != liveTile &&
              lastNode->GetTile().IsContains(
                  vehicle.position, true, nullptr, 5.0F))))
        {
            curTile = lastNode;
        }
        else
        {
            curTile = liveTile;
        }
        nextTile = nullptr;
    }

    if (curTile != nullptr && nextTile == nullptr)
    {
        nextTile = curTile->GetNext() != nullptr
                       ? curTile->GetNext()
                       : curTile->GetPoint()->GetRandomNode(
                             curTile, true,
                             randomSource != nullptr
                                 ? randomSource()
                                 : 0.0F);
    }
    curNode = curTile != nullptr &&
                      curTile->IsContains2(xy(vehicle.position))
                  ? curTile
                  : nullptr;
    moveDir = {};
    if (nextTile != nullptr)
        ComputeMovDir(car, vehicle);
}

void AICar::ControlState::Reset() noexcept
{
    steerAngle = 0.0F;
    timeBlocking = 0.0F;
    blocking = false;
    backMovingMode = false;
    backMoving = false;
    timeBackMoving = 0.0F;
    timeResetBlockCar = 0.0F;
}

bool AICar::ControlState::UpdateResetCar(
    float deltaTime, const Player::CarState& car,
    const VehicleState& vehicle) noexcept
{
    if (vehicle.mapObject &&
        (blocking || car.GetLiveTile() == nullptr ||
         std::abs(vehicle.speed) < maximumSpeedBlocking))
    {
        timeResetBlockCar += deltaTime;
        if (timeResetBlockCar > 3.0F * maximumTimeBlocking)
        {
            timeResetBlockCar = 0.0F;
            return true;
        }
    }
    else
    {
        timeResetBlockCar = 0.0F;
    }
    return false;
}

AICar::Command AICar::ControlState::Update(
    float deltaTime, const VehicleState& vehicle,
    const PathState& path, bool enabled) noexcept
{
    const TraceVec2 direction = normalized(xy(vehicle.direction));
    steerAngle = std::abs(std::acos(std::clamp(
        dot(direction, path.moveDir), -1.0F, 1.0F)));
    if (steerAngle > steerAngleBias)
        steerAngle = cross(direction, path.moveDir) > 0.0F
                         ? steerAngle
                         : -steerAngle;
    else
        steerAngle = 0.0F;

    if (std::abs(vehicle.speed) < maximumSpeedBlocking)
    {
        timeBlocking += deltaTime;
        if (timeBlocking > maximumTimeBlocking)
        {
            timeBlocking = 0.0F;
            blocking = true;
        }
    }
    else
    {
        timeBlocking = 0.0F;
        blocking = false;
    }

    if (!backMovingMode)
    {
        backMovingMode = blocking;
        backMoving = blocking;
    }
    if (backMovingMode)
    {
        timeBackMoving += deltaTime;
        if (timeBackMoving > maximumTimeBlocking ||
            (backMoving && std::abs(steerAngle) < steerAngleBias &&
             timeBackMoving > 0.5F * maximumTimeBlocking))
        {
            backMoving = !backMoving;
            timeBackMoving = 0.0F;
            backMovingMode = blocking;
        }
        if (backMoving)
            steerAngle = -steerAngle;
    }

    Command command;
    command.steeringAngle = enabled ? steerAngle : 0.0F;
    if (!enabled)
        return command;
    if (path.brake)
        command.move = MoveCarState::Brake;
    else if (vehicle.cheatSlower)
        command.move = MoveCarState::None;
    else if (backMoving)
        command.move = MoveCarState::Reverse;
    else
        command.move = MoveCarState::Accelerate;
    return command;
}

AICar::AICar(std::uint32_t trackCount) : path(trackCount) {}

void AICar::Reset(std::uint32_t trackCount)
{
    path.Reset(trackCount);
    control.Reset();
    resetCar_ = false;
}

AICar::Command AICar::Update(
    float deltaTime, const Player::CarState& car,
    const VehicleState& vehicle, bool enabled,
    RandomSource randomSource)
{
    path.Update(deltaTime, car, vehicle, randomSource);
    Command command = control.Update(
        deltaTime, vehicle, path, enabled);
    if (enabled)
    {
        command.resetCar = control.UpdateResetCar(
            deltaTime, car, vehicle);
        resetCar_ = resetCar_ || command.resetCar;
    }
    return command;
}

bool AICar::TakeResetCar() noexcept
{
    const bool result = resetCar_;
    resetCar_ = false;
    return result;
}

} // namespace r3d::game::originalrace::source
