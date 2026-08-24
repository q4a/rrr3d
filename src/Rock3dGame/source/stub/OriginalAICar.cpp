#include "OriginalAICar.h"

#include <algorithm>
#include <array>
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

TraceVec3 subtract(TraceVec3 first, TraceVec3 second) noexcept
{
    return {first.x - second.x, first.y - second.y,
            first.z - second.z};
}

TraceVec3 multiply(TraceVec3 value, float scale) noexcept
{
    return {value.x * scale, value.y * scale, value.z * scale};
}

float dot(TraceVec3 first, TraceVec3 second) noexcept
{
    return first.x * second.x + first.y * second.y +
           first.z * second.z;
}

float length(TraceVec3 value) noexcept
{
    return std::sqrt(dot(value, value));
}

TraceVec3 normalized(TraceVec3 value) noexcept
{
    const float magnitude = length(value);
    return magnitude > epsilon ? multiply(value, 1.0F / magnitude)
                               : TraceVec3{};
}

float randomUnit(AICar::RandomSource source,
                 float fallback = 1.0F) noexcept
{
    return source != nullptr
               ? std::clamp(source(), 0.0F, 1.0F)
               : fallback;
}

std::size_t uniformRandomIndex(
    std::size_t count, AICar::UniformRandomSource source) noexcept
{
    if (count <= 1U)
        return 0U;
    const double value = source != nullptr ? source() : 0.0;
    const double unit = std::clamp(
        value, 0.0, std::nextafter(1.0, 0.0));
    return std::min(
        static_cast<std::size_t>(
            std::floor(unit * static_cast<double>(count))),
        count - 1U);
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

void AICar::AttackState::Reset() noexcept
{
    target = invalidIndex;
    backTarget = invalidIndex;
    placeMineRandom = -1.0F;
}

void AICar::AttackState::DisposeTarget(std::size_t player) noexcept
{
    if (target == player)
        target = invalidIndex;
    if (backTarget == player)
        backTarget = invalidIndex;
}

std::size_t AICar::AttackState::FindEnemy(
    const Player::CarState& car, const VehicleState& vehicle,
    const AttackContext& context, int direction,
    std::size_t currentEnemy) const
{
    const WayNode* liveTile = car.GetLiveTile();
    if (liveTile == nullptr)
        return invalidIndex;

    const TraceVec3 direction3 = normalized(vehicle.direction3);
    std::size_t enemy = invalidIndex;
    float minimumPlaneDistance = 0.0F;
    for (std::size_t candidate = 0U;
         candidate < context.targets.size(); ++candidate)
    {
        const auto& targetState = context.targets[candidate];
        if (candidate == context.owner || !targetState.active ||
            !liveTile->GetTile().IsZLevelContains(
                targetState.position))
        {
            continue;
        }
        const TraceVec3 difference = subtract(
            targetState.position, vehicle.position);
        const float distance = length(difference);
        if (distance <= epsilon)
            continue;
        const float alignment = dot(
            normalized(difference), direction3);
        const bool inView =
            direction > 0 ? alignment >= 0.70710678F
                          : alignment <= -0.70710678F;
        const float planeDistance =
            std::abs(dot(direction3, difference));
        if (!inView ||
            (enemy != invalidIndex &&
             planeDistance >= minimumPlaneDistance))
        {
            continue;
        }
        enemy = candidate;
        minimumPlaneDistance = planeDistance;
    }

    if (currentEnemy == enemy)
        return enemy;
    const bool currentValid =
        currentEnemy < context.targets.size() &&
        context.targets[currentEnemy].active &&
        liveTile->GetTile().IsZLevelContains(
            context.targets[currentEnemy].position);
    if (enemy == invalidIndex)
        return currentValid ? currentEnemy : invalidIndex;
    if (!currentValid ||
        length(subtract(
            xy(context.targets[currentEnemy].position),
            xy(context.targets[enemy].position))) >
            context.targets[currentEnemy].size)
    {
        return enemy;
    }
    return currentEnemy;
}

std::size_t AICar::AttackState::ShotByEnemy(
    const Player::CarState& car, const VehicleState& vehicle,
    const AttackContext& context, std::size_t enemy) const
{
    if (enemy >= context.targets.size() ||
        !context.targets[enemy].active)
    {
        return invalidIndex;
    }
    const auto& enemyState = context.targets[enemy];
    const TraceVec2 direction = normalized(xy(vehicle.direction));
    const TraceVec2 difference = subtract(
        xy(enemyState.position), xy(vehicle.position));
    const float lateralDistance =
        std::abs(cross(direction, difference));
    const float sourceRadius = vehicle.size * 0.5F;
    if (lateralDistance >= enemyState.radius ||
        std::abs(vehicle.position.z - enemyState.position.z) >=
            std::min(sourceRadius, enemyState.radius))
    {
        return invalidIndex;
    }

    const float targetDistance = length(difference);
    const float forwardDistance = dot(direction, difference);
    std::array<const AttackWeapon*, 4> usable{};
    std::size_t usableCount = 0U;
    std::size_t chargedWeapons = 0U;
    for (const auto& weapon : context.weapons)
    {
        // Proj::ptHyper and ptMine do not participate in stWeapon1..4.
        if (weapon.projectileType == 1U ||
            weapon.projectileType == 11U)
        {
            continue;
        }
        // The source returns immediately if any installed ordinary weapon
        // has not reached max(shotDelay, 0.25).
        if (!weapon.ready)
            return invalidIndex;
        if (weapon.charge > 0U)
            ++chargedWeapons;
        const bool inRange =
            weapon.maximumDistance <= 0.0F ||
            targetDistance <
                std::min(weapon.maximumDistance, 100.0F);
        const bool hasCharge =
            weapon.capacity == 0U || weapon.charge > 0U;
        const bool forwardOrTorpedo =
            weapon.projectileType == 2U ||
            forwardDistance > sourceRadius * 0.5F;
        if (inRange && hasCharge && forwardOrTorpedo &&
            usableCount < usable.size())
        {
            usable[usableCount++] = &weapon;
        }
    }
    if (usableCount == 0U)
        return invalidIndex;

    std::stable_sort(
        usable.begin(), usable.begin() +
                            static_cast<std::ptrdiff_t>(usableCount),
        [](const AttackWeapon* first, const AttackWeapon* second) {
            return first->maximumDistance < second->maximumDistance;
        });
    const AttackWeapon* weapon = usable.front();
    if (randomUnit(context.randomSource) < 0.25F)
    {
        weapon = usable[uniformRandomIndex(
            usableCount, context.uniformRandomSource)];
    }

    const float roadDistance = [&]() {
        const float pathLength = car.GetPathLength(true);
        return pathLength > epsilon
                   ? car.GetDist(true) / pathLength
                   : 0.0F;
    }();
    const float summedPart = std::clamp(
        roadDistance / 0.7F, 0.0F, 1.0F);
    const float weaponPart =
        chargedWeapons > 0U
            ? 1.0F / static_cast<float>(chargedWeapons)
            : 1.0F;
    const float part = std::min(summedPart / weaponPart, 1.0F);
    const float ammunition = std::max(
        static_cast<float>(weapon->charge) -
            (1.0F - part) *
                static_cast<float>(weapon->capacity),
        0.0F);
    return weapon->capacity == 0U || ammunition > 0.0F
               ? weapon->slot
               : invalidIndex;
}

AICar::AttackDecision AICar::AttackState::Update(
    const Player::CarState& car, const VehicleState& vehicle,
    const PathState& path, const AttackContext& context)
{
    AttackDecision decision;
    const WayNode* liveTile = car.GetLiveTile();
    if (liveTile == nullptr)
        return decision;

    target = FindEnemy(car, vehicle, context, 1, target);
    backTarget = FindEnemy(car, vehicle, context, -1, backTarget);

    const std::size_t frontSlot = ShotByEnemy(
        car, vehicle, context, target);
    if (frontSlot != invalidIndex && context.enabled)
    {
        decision.weaponSlot = frontSlot;
        decision.weaponTarget = target;
    }
    else
    {
        const std::size_t backSlot = ShotByEnemy(
            car, vehicle, context, backTarget);
        if (backSlot != invalidIndex && context.enabled)
        {
            decision.weaponSlot = backSlot;
            decision.weaponTarget = backTarget;
        }
    }

    if (context.hyper.installed && vehicle.speed > 1.0F &&
        !path.brake)
    {
        const float coordinate =
            liveTile->GetTile().ComputeCoordX(xy(vehicle.position));
        const float distanceToTurn =
            liveTile->GetTile().ComputeLength(1.0F - coordinate);
        const float maximumHyperDistance =
            context.hyper.projectileSpeed + vehicle.speed;
        const bool safeDistance =
            path.nextTile == nullptr ||
            path.nextTile->GetTile().GetTurnAngle() < pi / 6.0F ||
            distanceToTurn > maximumHyperDistance;
        const float pathLength = car.GetPathLength(true);
        const float roadDistance =
            pathLength > epsilon ? car.GetDist(true) / pathLength
                                 : 0.0F;
        const float summedPart = std::clamp(
            roadDistance / 0.7F, 0.0F, 1.0F);
        const float ammunition = std::max(
            static_cast<float>(context.hyper.charge) -
                (1.0F - summedPart) *
                    static_cast<float>(context.hyper.capacity),
            0.0F);
        decision.useHyper =
            context.enabled && safeDistance &&
            (context.hyper.capacity == 0U || ammunition > 0.0F);
    }

    if (context.mine.installed && vehicle.speed > 5.0F)
    {
        if (placeMineRandom == -1.0F)
        {
            placeMineRandom =
                -0.5F + randomUnit(context.randomSource) * 0.5F;
        }
        const float pathLength = car.GetPathLength(true);
        const float roadDistance =
            pathLength > epsilon ? car.GetDist(true) / pathLength
                                 : 0.0F;
        float summedPart = std::clamp(
            (roadDistance - 0.05F) / 0.9F, 0.0F, 1.0F);
        if (summedPart > 0.0F && summedPart < 1.0F)
        {
            if (backTarget < context.targets.size() &&
                context.targets[backTarget].active &&
                length(subtract(
                    xy(context.targets[backTarget].position),
                    xy(vehicle.position))) < 30.0F)
            {
                summedPart += 0.3F;
            }
            summedPart = std::clamp(
                summedPart + placeMineRandom, 0.0F, 1.0F);
        }
        const std::uint32_t maximumCharge =
            context.mine.oil ? 2U : 3U;
        const std::uint32_t capacity =
            std::min(context.mine.capacity, maximumCharge);
        const std::uint32_t spent =
            context.mine.capacity > context.mine.charge
                ? context.mine.capacity - context.mine.charge
                : 0U;
        const std::uint32_t current =
            capacity - std::min(spent, capacity);
        const float ammunition = std::max(
            static_cast<float>(current) -
                (1.0F - summedPart) *
                    static_cast<float>(capacity),
            0.0F);
        if ((capacity == 0U || ammunition > 0.0F) &&
            context.enabled)
        {
            decision.useMine = true;
            placeMineRandom =
                -0.5F + randomUnit(context.randomSource) * 0.5F;
        }
    }
    return decision;
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
    attack.Reset();
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

AIPlayer::AIPlayer(std::uint32_t trackCount)
    : car_(trackCount), trackCount_(std::max(trackCount, 1U))
{
}

AIPlayer::AIPlayer(Player* player, bool human,
                   std::uint32_t trackCount)
    : AIPlayer(trackCount)
{
    Reset(player, human, trackCount);
}

void AIPlayer::Reset(Player* player, bool human,
                     std::uint32_t trackCount)
{
    FreeCar();
    player_ = player;
    trackCount_ = std::max(trackCount, 1U);
    cheat_ = human
                 ? cheatDisabled
                 : cheatEnableFaster | cheatEnableSlower;
    enabled_ = true;
}

void AIPlayer::CreateCar()
{
    if (carCreated_ || player_ == nullptr)
        return;
    car_.Reset(trackCount_);
    carCreated_ = true;
}

void AIPlayer::FreeCar()
{
    if (!carCreated_)
        return;
    car_.Reset(trackCount_);
    carCreated_ = false;
}

AICar::Command AIPlayer::OnProgress(
    float deltaTime, const AICar::VehicleState& vehicle,
    AICar::RandomSource randomSource)
{
    if (!carCreated_ || player_ == nullptr)
        return {};
    return car_.Update(
        deltaTime, player_->car, vehicle, enabled_, randomSource);
}

AICar::AttackDecision AIPlayer::UpdateAttack(
    const AICar::VehicleState& vehicle,
    const AICar::AttackContext& context)
{
    if (!carCreated_ || player_ == nullptr)
        return {};
    AICar::AttackContext enabledContext = context;
    enabledContext.enabled = enabled_ && context.enabled;
    return car_.attack.Update(
        player_->car, vehicle, car_.path, enabledContext);
}

void AIPlayer::DisposeTarget(std::size_t player) noexcept
{
    car_.attack.DisposeTarget(player);
}

bool AIPlayer::TakeResetCar() noexcept
{
    return carCreated_ && car_.TakeResetCar();
}

void AIPlayer::SetEnabled(bool value) noexcept { enabled_ = value; }
bool AIPlayer::IsEnabled() const noexcept { return enabled_; }
bool AIPlayer::HasCar() const noexcept { return carCreated_; }
std::uint32_t AIPlayer::GetCheat() const noexcept { return cheat_; }
Player* AIPlayer::GetPlayer() noexcept { return player_; }
const Player* AIPlayer::GetPlayer() const noexcept { return player_; }
AICar* AIPlayer::GetCar() noexcept
{
    return carCreated_ ? &car_ : nullptr;
}
const AICar* AIPlayer::GetCar() const noexcept
{
    return carCreated_ ? &car_ : nullptr;
}

AISystem::AISystem(std::uint32_t trackCount) noexcept
{
    Reset(trackCount);
}

void AISystem::Reset(std::uint32_t trackCount) noexcept
{
    trackCount_ = std::max(trackCount, 1U);
}

void AISystem::ComputeTracks(std::span<Entry> entries) const
{
    struct Link
    {
        std::size_t entry = 0U;
        float directionDistance = 0.0F;
    };

    std::vector<Link> links;
    links.reserve(entries.size());
    for (std::size_t entryIndex = 0U;
         entryIndex < entries.size(); ++entryIndex)
    {
        auto& entry = entries[entryIndex];
        if (!entry.active || entry.aiCar == nullptr ||
            entry.car == nullptr ||
            entry.car->GetLiveTile() == nullptr ||
            entry.car->GetCurNode() == nullptr)
        {
            continue;
        }

        const WayNode* tile = entry.car->GetLiveTile();
        const TraceVec2 direction = tile->GetTile().GetDir();
        const TraceVec2 clockwiseNormal{direction.y, -direction.x};
        links.push_back({
            entryIndex,
            lineDistance(clockwiseNormal, tile->GetPos2(),
                         xy(entry.position))});
        // AISystem clears lane availability for every inserted AICar before
        // it builds any chain, including a car which has no neighbour.
        entry.aiCar->path.ResetTracks();
    }

    if (links.size() < 2U)
        return;

    // The Windows Container removes each outer link after comparing it with
    // all still-present links.  Preserve that ordered, asymmetric test: the
    // first car's curNode tile decides whether the second car can join.
    std::vector<std::size_t> parent(links.size());
    for (std::size_t index = 0U; index < parent.size(); ++index)
        parent[index] = index;
    const auto findRoot = [&](std::size_t index) {
        while (parent[index] != index)
        {
            parent[index] = parent[parent[index]];
            index = parent[index];
        }
        return index;
    };

    for (std::size_t first = 0U; first < links.size(); ++first)
    {
        const Entry& sourceEntry = entries[links[first].entry];
        const WayNode* currentNode = sourceEntry.car->GetCurNode();
        const TraceVec2 direction =
            sourceEntry.car->GetLiveTile()->GetTile().GetDir();
        for (std::size_t second = first + 1U;
             second < links.size(); ++second)
        {
            const Entry& targetEntry = entries[links[second].entry];
            const float normalDistance = dot(
                direction,
                subtract(xy(targetEntry.position),
                         xy(sourceEntry.position)));
            const float combinedRadius =
                sourceEntry.radius + targetEntry.radius;
            const bool lowerRange =
                normalDistance + combinedRadius > 0.0F;
            const bool higherRange =
                normalDistance - combinedRadius < 0.0F;
            if (!lowerRange || !higherRange ||
                !currentNode->GetTile().IsContains(
                    targetEntry.position, false))
            {
                continue;
            }

            const std::size_t firstRoot = findRoot(first);
            const std::size_t secondRoot = findRoot(second);
            if (firstRoot != secondRoot)
                parent[secondRoot] = firstRoot;
        }
    }

    std::vector<std::vector<std::size_t>> chains(links.size());
    for (std::size_t index = 0U; index < links.size(); ++index)
        chains[findRoot(index)].push_back(index);

    for (auto& chain : chains)
    {
        if (chain.size() < 2U)
            continue;
        std::stable_sort(
            chain.begin(), chain.end(),
            [&](std::size_t first, std::size_t second) {
                return links[first].directionDistance <
                       links[second].directionDistance;
            });

        const std::uint32_t occupiedTracks =
            static_cast<std::uint32_t>(
                std::min<std::size_t>(chain.size(), trackCount_));
        std::uint32_t selectedTrack = 0U;
        const Entry* previousEntry = nullptr;
        std::uint32_t previousSelectedTrack = 0U;
        for (std::size_t index = 0U; index < chain.size(); ++index)
        {
            Entry& entry = entries[links[chain[index]].entry];
            const std::uint32_t lower =
                std::min(selectedTrack, trackCount_ - 1U);
            const std::uint32_t upper =
                trackCount_ -
                (occupiedTracks -
                 static_cast<std::uint32_t>(index % trackCount_));
            const std::uint32_t currentTrack =
                std::min(entry.car->GetTrack(), trackCount_ - 1U);
            // This is lsl::ClampValue rather than std::clamp: for chains
            // longer than the lane count the source can pass lower > upper.
            selectedTrack =
                currentTrack > lower
                    ? (currentTrack < upper ? currentTrack : upper)
                    : lower;

            for (const std::size_t other : chain)
            {
                Entry& otherEntry =
                    entries[links[other].entry];
                if (&otherEntry != &entry)
                    otherEntry.aiCar->path.LockTrack(selectedTrack);
            }
            if (previousEntry != nullptr &&
                previousEntry->car->GetTrack() ==
                    entry.car->GetTrack() &&
                previousSelectedTrack > 0U)
            {
                entry.aiCar->path.LockTrack(
                    previousSelectedTrack - 1U);
            }
            previousEntry = &entry;
            previousSelectedTrack = selectedTrack;
            ++selectedTrack;
        }
    }
}

} // namespace r3d::game::originalrace::source
