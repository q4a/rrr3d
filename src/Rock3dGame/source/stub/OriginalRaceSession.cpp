#include "OriginalRaceSession.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace r3d::game::originalrace
{
namespace
{

Vec3 subtract(Vec3 first, Vec3 second)
{
    return {first.x - second.x, first.y - second.y, first.z - second.z};
}

Vec3 add(Vec3 first, Vec3 second)
{
    return {first.x + second.x, first.y + second.y, first.z + second.z};
}

Vec3 multiply(Vec3 value, float scale)
{
    return {value.x * scale, value.y * scale, value.z * scale};
}

float dot2(Vec3 first, Vec3 second)
{
    return first.x * second.x + first.y * second.y;
}

float length2(Vec3 value)
{
    return std::sqrt(dot2(value, value));
}

Vec3 normalized2(Vec3 value)
{
    const float length = length2(value);
    if (length <= 0.0001F)
        return {1.0F, 0.0F, 0.0F};
    return {value.x / length, value.y / length, 0.0F};
}

Vec3 forward(const Quat& q)
{
    return {1.0F - 2.0F * (q.y * q.y + q.z * q.z),
            2.0F * (q.x * q.y + q.w * q.z), 0.0F};
}

float distanceSquared(Vec3 first, Vec3 second)
{
    const auto difference = subtract(first, second);
    return difference.x * difference.x + difference.y * difference.y +
           difference.z * difference.z;
}

float clampSteering(float value)
{
    return std::clamp(value, -1.0F, 1.0F);
}

} // namespace

OriginalRaceSession::OriginalRaceSession(const Race& race) : race_(race)
{
    if (race_.tracePath.size() < 2 || race_.tracePoints.empty() ||
        race_.racers.empty())
        throw std::invalid_argument("Original race session data is incomplete");
    reset();
}

void OriginalRaceSession::reset()
{
    phase_ = RacePhase::Countdown;
    phaseBeforePause_ = phase_;
    countdownSeconds_ = 3.0F;
    countdownDisplay_ = 3;
    elapsedSeconds_ = 0.0F;
    racers_.assign(race_.racers.size(), {});
    vehicleInputs_.assign(race_.racers.size(), {});
    weaponCooldown_.assign(race_.racers.size(), 0.0F);
    stuckSeconds_.assign(race_.racers.size(), 0.0F);
    previousPositions_.assign(race_.racers.size(), {});
    decorationActive_.assign(race_.decorationInstances.size(), true);
    bonusActive_.assign(race_.bonuses.size(), true);
    events_.clear();
    effects_.clear();
    respawns_.clear();
    for (std::size_t index = 0; index < racers_.size(); ++index)
    {
        const std::size_t vehicleIndex = race_.racers[index].vehicle;
        const auto& vehicle =
            race_.vehicles.at(std::min(vehicleIndex,
                                      race_.vehicles.size() - 1U));
        racers_[index].maximumLife = std::max(vehicle.maximumLife, 1.0F);
        racers_[index].life = racers_[index].maximumLife;
        racers_[index].place = static_cast<std::uint32_t>(index + 1U);
    }
    events_.push_back(
        {RaceEventKind::CountdownChanged, 0, 0, {}, 3.0F});
}

void OriginalRaceSession::setPaused(bool paused) noexcept
{
    if (paused && phase_ != RacePhase::Paused)
    {
        phaseBeforePause_ = phase_;
        phase_ = RacePhase::Paused;
    }
    else if (!paused && phase_ == RacePhase::Paused)
    {
        phase_ = phaseBeforePause_;
    }
}

RacePhase OriginalRaceSession::phase() const noexcept
{
    return phase_;
}

float OriginalRaceSession::countdownSeconds() const noexcept
{
    return countdownSeconds_;
}

float OriginalRaceSession::elapsedSeconds() const noexcept
{
    return elapsedSeconds_;
}

const std::vector<r3d::physics::VehicleInput>&
OriginalRaceSession::vehicleInputs() const noexcept
{
    return vehicleInputs_;
}

const std::vector<RacerRuntime>& OriginalRaceSession::racers() const noexcept
{
    return racers_;
}

const std::vector<bool>& OriginalRaceSession::decorationActive() const noexcept
{
    return decorationActive_;
}

const std::vector<bool>& OriginalRaceSession::bonusActive() const noexcept
{
    return bonusActive_;
}

const std::vector<RaceEvent>& OriginalRaceSession::events() const noexcept
{
    return events_;
}

const std::vector<RaceEffect>& OriginalRaceSession::effects() const noexcept
{
    return effects_;
}

std::vector<RespawnRequest> OriginalRaceSession::takeRespawns()
{
    auto result = std::move(respawns_);
    respawns_.clear();
    return result;
}

const TracePoint& OriginalRaceSession::tracePoint(
    std::size_t pathNode) const
{
    const std::uint32_t id =
        race_.tracePath.at(pathNode % race_.tracePath.size());
    const auto found = std::find_if(
        race_.tracePoints.begin(), race_.tracePoints.end(),
        [id](const TracePoint& point) { return point.id == id; });
    if (found == race_.tracePoints.end())
        throw std::runtime_error("Original race trace path is unresolved");
    return *found;
}

void OriginalRaceSession::updateProgress(
    std::size_t racer, const r3d::physics::VehicleState& vehicle)
{
    auto& runtime = racers_[racer];
    if (runtime.finished || runtime.nextPathNode >= race_.tracePath.size())
        return;

    const auto& target = tracePoint(runtime.nextPathNode);
    const float radius = std::max(target.width * 0.55F, 7.0F);
    const auto expectedDirection = normalized2(
        subtract(target.position,
                 tracePoint(runtime.nextPathNode - 1U).position));
    runtime.wrongWay =
        vehicle.speed > 3.0F &&
        dot2(normalized2(forward(vehicle.body.rotation)),
             expectedDirection) < -0.25F;

    if (distanceSquared(vehicle.body.position, target.position) >
        radius * radius)
        return;

    events_.push_back({RaceEventKind::Checkpoint, racer,
                       runtime.nextPathNode, target.position, 0.0F});
    ++runtime.nextPathNode;
    if (runtime.nextPathNode < race_.tracePath.size())
        return;

    ++runtime.completedLaps;
    events_.push_back({RaceEventKind::Lap, racer, runtime.completedLaps,
                       vehicle.body.position,
                       static_cast<float>(runtime.completedLaps)});
    runtime.nextPathNode = 1;
    if (runtime.completedLaps >= race_.lapCount)
    {
        runtime.finished = true;
        runtime.finishTime = elapsedSeconds_;
        vehicleInputs_[racer] = {};
        events_.push_back({RaceEventKind::Finish, racer, 0,
                           vehicle.body.position, runtime.finishTime});
        if (racer == 0)
            phase_ = RacePhase::Finished;
    }
}

r3d::physics::VehicleInput OriginalRaceSession::aiInput(
    std::size_t racer, const r3d::physics::VehicleState& vehicle) const
{
    if (racers_[racer].finished)
        return {};
    const auto& target = tracePoint(racers_[racer].nextPathNode);
    const Vec3 wanted = normalized2(subtract(target.position,
                                             vehicle.body.position));
    const Vec3 carForward = normalized2(forward(vehicle.body.rotation));
    const float cross = carForward.x * wanted.y - carForward.y * wanted.x;
    const float alignment = dot2(carForward, wanted);
    const float targetDistance =
        length2(subtract(target.position, vehicle.body.position));

    r3d::physics::VehicleInput input;
    input.steering = clampSteering(cross * 2.6F);
    const float corner =
        std::clamp(std::abs(input.steering), 0.0F, 1.0F);
    input.throttle = alignment < -0.15F ? 0.15F : 1.0F - corner * 0.42F;
    if (targetDistance < 16.0F && corner > 0.55F && vehicle.speed > 16.0F)
        input.brake = std::clamp(corner * 0.7F, 0.0F, 0.7F);
    if (racers_[racer].speedBoostSeconds > 0.0F)
        input.throttle = 1.0F;
    return input;
}

void OriginalRaceSession::updatePlaces(
    const std::vector<r3d::physics::VehicleState>& vehicles)
{
    std::vector<std::size_t> order(racers_.size());
    std::iota(order.begin(), order.end(), 0U);
    auto score = [&](std::size_t racer) {
        const auto& runtime = racers_[racer];
        const float base =
            static_cast<float>(runtime.completedLaps *
                                   (race_.tracePath.size() - 1U) +
                               runtime.nextPathNode);
        if (racer >= vehicles.size() || runtime.finished)
            return base + (runtime.finished ? 100000.0F -
                                                 runtime.finishTime
                                           : 0.0F);
        const auto& target = tracePoint(runtime.nextPathNode);
        const auto& previous = tracePoint(runtime.nextPathNode - 1U);
        const float segment =
            std::max(length2(subtract(target.position, previous.position)),
                     1.0F);
        return base - std::clamp(
                          length2(subtract(target.position,
                                           vehicles[racer].body.position)) /
                              segment,
                          0.0F, 1.0F);
    };
    std::stable_sort(order.begin(), order.end(),
                     [&](std::size_t first, std::size_t second) {
                         return score(first) > score(second);
                     });
    for (std::size_t place = 0; place < order.size(); ++place)
        racers_[order[place]].place =
            static_cast<std::uint32_t>(place + 1U);
}

void OriginalRaceSession::queueRespawn(
    std::size_t racer, const r3d::physics::VehicleState& vehicle)
{
    const auto& runtime = racers_[racer];
    const std::size_t previousNode =
        runtime.nextPathNode > 0 ? runtime.nextPathNode - 1U : 0U;
    const auto& point = tracePoint(previousNode);
    const auto& next = tracePoint(
        std::min(previousNode + 1U, race_.tracePath.size() - 1U));
    const Vec3 direction =
        normalized2(subtract(next.position, point.position));
    const Vec3 position = add(point.position, {0.0F, 0.0F, 2.0F});
    respawns_.push_back({racer, position, direction});
    previousPositions_[racer] = vehicle.body.position;
    stuckSeconds_[racer] = 0.0F;
    events_.push_back(
        {RaceEventKind::Respawn, racer, previousNode, position, 0.0F});
}

void OriginalRaceSession::updateGameplay(
    float seconds,
    const std::vector<r3d::physics::VehicleState>& vehicles,
    const RaceControl& humanControl)
{
    for (std::size_t racer = 0; racer < racers_.size(); ++racer)
    {
        auto& runtime = racers_[racer];
        runtime.shieldSeconds =
            std::max(0.0F, runtime.shieldSeconds - seconds);
        runtime.speedBoostSeconds =
            std::max(0.0F, runtime.speedBoostSeconds - seconds);
        weaponCooldown_[racer] =
            std::max(0.0F, weaponCooldown_[racer] - seconds);
        if (racer < vehicles.size())
            updateProgress(racer, vehicles[racer]);
    }

    if (!vehicles.empty())
    {
        if (humanControl.reset)
            queueRespawn(0, vehicles[0]);
        const float moved =
            length2(subtract(vehicles[0].body.position,
                             previousPositions_[0]));
        if (vehicles[0].contactCount == 0 &&
            vehicles[0].body.position.z < -5.0F)
            stuckSeconds_[0] = 10.0F;
        else if (vehicles[0].speed < 0.35F && moved < 0.15F)
            stuckSeconds_[0] += seconds;
        else
            stuckSeconds_[0] = 0.0F;
        previousPositions_[0] = vehicles[0].body.position;
        if (stuckSeconds_[0] > 5.0F)
            queueRespawn(0, vehicles[0]);
    }

    for (std::size_t bonusIndex = 0;
         bonusIndex < race_.bonuses.size(); ++bonusIndex)
    {
        if (!bonusActive_[bonusIndex])
            continue;
        for (std::size_t racer = 0;
             racer < vehicles.size() && racer < racers_.size(); ++racer)
        {
            if (distanceSquared(vehicles[racer].body.position,
                                race_.bonuses[bonusIndex].transform.position) >
                12.25F)
                continue;
            auto& runtime = racers_[racer];
            const auto& bonus = race_.bonuses[bonusIndex];
            switch (bonus.kind)
            {
            case BonusKind::Money:
                runtime.money +=
                    static_cast<std::uint32_t>(std::max(bonus.value, 0.0F));
                break;
            case BonusKind::Medpack:
                runtime.life = runtime.maximumLife;
                break;
            case BonusKind::Ammunition:
                runtime.ammunition =
                    std::min(runtime.ammunition +
                                 std::max(1U, static_cast<std::uint32_t>(
                                                  std::ceil(bonus.value * 10.0F))),
                             99U);
                break;
            case BonusKind::Mine:
                ++runtime.mines;
                break;
            case BonusKind::Shield:
                runtime.shieldSeconds = std::max(bonus.value, 5.0F);
                break;
            case BonusKind::Speed:
                runtime.speedBoostSeconds = std::max(bonus.value, 3.0F);
                break;
            case BonusKind::Unknown:
                break;
            }
            bonusActive_[bonusIndex] = false;
            events_.push_back({RaceEventKind::Bonus, racer, bonusIndex,
                               bonus.transform.position, bonus.value});
            break;
        }
    }

    auto fireWeapon = [&](std::size_t shooter) {
        if (shooter >= vehicles.size() || racers_[shooter].ammunition == 0 ||
            weaponCooldown_[shooter] > 0.0F ||
            racers_[shooter].finished)
            return;
        --racers_[shooter].ammunition;
        weaponCooldown_[shooter] = 0.18F;
        const Vec3 origin = vehicles[shooter].body.position;
        const Vec3 direction =
            normalized2(forward(vehicles[shooter].body.rotation));
        std::size_t target = racers_.size();
        float targetDistance = 85.0F;
        for (std::size_t candidate = 0;
             candidate < vehicles.size() && candidate < racers_.size();
             ++candidate)
        {
            if (candidate == shooter || racers_[candidate].finished)
                continue;
            const auto difference =
                subtract(vehicles[candidate].body.position, origin);
            const float distance = length2(difference);
            if (distance >= targetDistance || distance <= 0.001F)
                continue;
            const float alignment =
                dot2(direction, normalized2(difference));
            if (alignment > 0.94F)
            {
                target = candidate;
                targetDistance = distance;
            }
        }
        Vec3 end = add(origin, multiply(direction, 85.0F));
        if (target < racers_.size())
        {
            end = vehicles[target].body.position;
            const float damage =
                racers_[target].shieldSeconds > 0.0F ? 0.0F : 5.0F;
            racers_[target].life =
                std::max(0.0F, racers_[target].life - damage);
            events_.push_back({RaceEventKind::Damage, target, shooter, end,
                               damage});
            if (racers_[target].life <= 0.0F)
            {
                racers_[target].life = racers_[target].maximumLife;
                queueRespawn(target, vehicles[target]);
            }
        }
        events_.push_back({RaceEventKind::WeaponFired, shooter, target,
                           origin, 5.0F});
        effects_.push_back(
            {RaceEventKind::WeaponFired, origin, end, 0.12F});
    };
    if (humanControl.useWeapon)
        fireWeapon(0);
    for (std::size_t racer = 1;
         racer < racers_.size() && racer < vehicles.size(); ++racer)
    {
        if (racers_[racer].ammunition != 0 &&
            weaponCooldown_[racer] <= 0.0F)
        {
            const auto direction =
                normalized2(forward(vehicles[racer].body.rotation));
            for (std::size_t target = 0; target < vehicles.size(); ++target)
            {
                if (target == racer)
                    continue;
                const auto difference = subtract(
                    vehicles[target].body.position,
                    vehicles[racer].body.position);
                if (length2(difference) < 45.0F &&
                    dot2(direction, normalized2(difference)) > 0.985F)
                {
                    fireWeapon(racer);
                    break;
                }
            }
        }
    }

    for (std::size_t first = 0; first < vehicles.size(); ++first)
    {
        for (std::size_t second = first + 1U;
             second < vehicles.size(); ++second)
        {
            if (distanceSquared(vehicles[first].body.position,
                                vehicles[second].body.position) > 6.25F)
                continue;
            const float relativeSpeed = std::abs(
                vehicles[first].speed - vehicles[second].speed);
            if (relativeSpeed < 7.0F)
                continue;
            const float damage = std::min(relativeSpeed * 0.025F, 2.0F);
            if (racers_[first].shieldSeconds <= 0.0F)
                racers_[first].life =
                    std::max(0.0F, racers_[first].life - damage);
            if (racers_[second].shieldSeconds <= 0.0F)
                racers_[second].life =
                    std::max(0.0F, racers_[second].life - damage);
        }
    }

    if (!vehicles.empty())
    {
        for (std::size_t index = 0;
             index < race_.decorationInstances.size(); ++index)
        {
            if (!decorationActive_[index])
                continue;
            const auto& instance = race_.decorationInstances[index];
            const auto& definition =
                race_.decorationDefinitions.at(instance.definition);
            if (!definition.destructible ||
                distanceSquared(vehicles[0].body.position,
                                instance.transform.position) > 9.0F ||
                vehicles[0].speed < 8.0F)
                continue;
            decorationActive_[index] = false;
            events_.push_back(
                {RaceEventKind::DecorationDestroyed, 0, index,
                 instance.transform.position, vehicles[0].speed});
            effects_.push_back(
                {RaceEventKind::DecorationDestroyed,
                 instance.transform.position,
                 add(instance.transform.position, {0.0F, 0.0F, 3.0F}),
                 0.5F});
        }
    }
}

void OriginalRaceSession::update(
    float seconds,
    const std::vector<r3d::physics::VehicleState>& vehicles,
    const RaceControl& humanControl)
{
    seconds = std::clamp(seconds, 0.0F, 0.1F);
    events_.clear();
    for (auto& effect : effects_)
        effect.seconds -= seconds;
    effects_.erase(
        std::remove_if(effects_.begin(), effects_.end(),
                       [](const RaceEffect& effect) {
                           return effect.seconds <= 0.0F;
                       }),
        effects_.end());
    std::fill(vehicleInputs_.begin(), vehicleInputs_.end(),
              r3d::physics::VehicleInput{});
    if (phase_ == RacePhase::Paused)
        return;

    if (phase_ == RacePhase::Countdown)
    {
        countdownSeconds_ -= seconds;
        const int display =
            std::max(0, static_cast<int>(std::ceil(countdownSeconds_)));
        if (display != countdownDisplay_)
        {
            countdownDisplay_ = display;
            events_.push_back({RaceEventKind::CountdownChanged, 0, 0, {},
                               static_cast<float>(display)});
        }
        if (countdownSeconds_ <= 0.0F)
        {
            countdownSeconds_ = 0.0F;
            phase_ = RacePhase::Racing;
            events_.push_back(
                {RaceEventKind::CountdownChanged, 0, 0, {}, 0.0F});
        }
        return;
    }

    if (phase_ == RacePhase::Finished)
        return;

    elapsedSeconds_ += seconds;
    if (!vehicleInputs_.empty())
    {
        vehicleInputs_[0] = humanControl.driving;
        if (racers_[0].speedBoostSeconds > 0.0F)
            vehicleInputs_[0].throttle = 1.0F;
    }
    for (std::size_t racer = 1;
         racer < racers_.size() && racer < vehicles.size(); ++racer)
        vehicleInputs_[racer] = aiInput(racer, vehicles[racer]);

    updateGameplay(seconds, vehicles, humanControl);
    updatePlaces(vehicles);
}

bool runOriginalRaceSessionSmokeTest(const Race& race, std::string& error)
{
    try
    {
        OriginalRaceSession session(race);
        auto point = [&](std::size_t pathNode) -> const TracePoint& {
            const std::uint32_t id = race.tracePath.at(pathNode);
            const auto found = std::find_if(
                race.tracePoints.begin(), race.tracePoints.end(),
                [id](const TracePoint& value) { return value.id == id; });
            if (found == race.tracePoints.end())
                throw std::runtime_error("unresolved smoke-test trace point");
            return *found;
        };
        std::vector<r3d::physics::VehicleState> vehicles(race.racers.size());
        for (std::size_t index = 0; index < vehicles.size(); ++index)
        {
            vehicles[index].body.position =
                point(0).position;
            vehicles[index].body.position.z += 2.0F;
            vehicles[index].contactCount = 4;
        }
        RaceControl input;
        input.driving.throttle = 1.0F;
        for (int frame = 0; frame < 190; ++frame)
            session.update(1.0F / 60.0F, vehicles, input);
        if (session.phase() != RacePhase::Racing ||
            session.vehicleInputs().empty() ||
            session.vehicleInputs().front().throttle < 0.9F)
            throw std::runtime_error("countdown/control transition failed");

        for (std::size_t node = 1; node < race.tracePath.size(); ++node)
        {
            vehicles[0].body.position = point(node).position;
            session.update(1.0F / 60.0F, vehicles, input);
        }
        if (session.racers().front().completedLaps != 1 ||
            session.racers().front().nextPathNode != 1)
            throw std::runtime_error("checkpoint/lap transition failed");

        input.useWeapon = true;
        if (vehicles.size() > 1)
        {
            vehicles[1].body.position =
                add(vehicles[0].body.position, {10.0F, 0.0F, 0.0F});
            session.update(1.0F / 60.0F, vehicles, input);
            if (session.racers().front().ammunition != 9)
                throw std::runtime_error("weapon/ammunition transition failed");
        }
        input.useWeapon = false;
        input.reset = true;
        session.update(1.0F / 60.0F, vehicles, input);
        if (session.takeRespawns().empty())
            throw std::runtime_error("reset/respawn transition failed");
        error.clear();
        return true;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return false;
    }
}

} // namespace r3d::game::originalrace
