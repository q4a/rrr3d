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

float dot3(Vec3 first, Vec3 second)
{
    return dot2(first, second) + first.z * second.z;
}

float length2(Vec3 value)
{
    return std::sqrt(dot2(value, value));
}

float length3(Vec3 value)
{
    return std::sqrt(dot3(value, value));
}

Vec3 normalized2(Vec3 value)
{
    const float length = length2(value);
    if (length <= 0.0001F)
        return {1.0F, 0.0F, 0.0F};
    return {value.x / length, value.y / length, 0.0F};
}

Vec3 normalized3(Vec3 value)
{
    const float length = length3(value);
    if (length <= 0.0001F)
        return {1.0F, 0.0F, 0.0F};
    return {value.x / length, value.y / length, value.z / length};
}

Vec3 rotate(const Quat& q, Vec3 value)
{
    const Vec3 twiceCross{
        2.0F * (q.y * value.z - q.z * value.y),
        2.0F * (q.z * value.x - q.x * value.z),
        2.0F * (q.x * value.y - q.y * value.x)};
    return {value.x + q.w * twiceCross.x +
                        (q.y * twiceCross.z - q.z * twiceCross.y),
            value.y + q.w * twiceCross.y +
                        (q.z * twiceCross.x - q.x * twiceCross.z),
            value.z + q.w * twiceCross.z +
                        (q.x * twiceCross.y - q.y * twiceCross.x)};
}

Quat multiply(const Quat& first, const Quat& second)
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

Transform compose(const Transform& parent, const Transform& local)
{
    Transform result;
    const auto offset = rotate(
        parent.rotation,
        {local.position.x * parent.scale.x,
         local.position.y * parent.scale.y,
         local.position.z * parent.scale.z});
    result.position = add(parent.position, offset);
    result.scale = {parent.scale.x * local.scale.x,
                    parent.scale.y * local.scale.y,
                    parent.scale.z * local.scale.z};
    result.rotation = multiply(parent.rotation, local.rotation);
    return result;
}

Vec3 forward(const Quat& q)
{
    return {1.0F - 2.0F * (q.y * q.y + q.z * q.z),
            2.0F * (q.x * q.y + q.w * q.z),
            2.0F * (q.x * q.z - q.w * q.y)};
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

std::string_view recordName(std::string_view value)
{
    const auto slash = value.find_last_of("\\/");
    return slash == std::string_view::npos ? value
                                            : value.substr(slash + 1U);
}

std::string workshopReference(std::string_view record)
{
    return "world\\race\\workshopRoot\\workshop\\" +
           std::string(record);
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
    weaponCooldown_.assign(race_.racers.size(), {});
    mineCooldown_.assign(race_.racers.size(), 0.0F);
    hyperCooldown_.assign(race_.racers.size(), 0.0F);
    repairSeconds_.assign(race_.racers.size(), 0.0F);
    stuckSeconds_.assign(race_.racers.size(), 0.0F);
    touchCooldown_.assign(
        race_.racers.size() * race_.racers.size(), 0.0F);
    aiBrake_.assign(race_.racers.size(), false);
    previousPositions_.assign(race_.racers.size(), {});
    decorationActive_.assign(race_.decorationInstances.size(), true);
    decorationLife_.clear();
    decorationLife_.reserve(race_.decorationInstances.size());
    for (const auto& instance : race_.decorationInstances)
    {
        const auto& definition =
            race_.decorationDefinitions.at(instance.definition);
        decorationLife_.push_back(
            definition.maximumLife > 0.0F
                ? definition.maximumLife
                : (definition.destructible ? 1.0F : -1.0F));
    }
    bonusActive_.assign(race_.bonuses.size(), true);
    events_.clear();
    effects_.clear();
    mines_.clear();
    projectiles_.clear();
    respawns_.clear();
    velocityRequests_.clear();
    angularVelocityRequests_.clear();
    achievementPoints_ = initialAchievementPoints_;
    achievementIterations_.assign(race_.achievements.size(), 0U);
    achievementConditionCounters_.assign(
        race_.achievements.size(), 0U);
    achievementConditionTimers_.assign(
        race_.achievements.size(), 0.0F);
    achievementGlobalKills_ = 0U;
    achievementPreviousLapPlace_ = 0U;
    for (std::size_t index = 0;
         index < race_.achievements.size(); ++index)
    {
        const auto found = initialAchievementIterations_.find(
            race_.achievements[index].name);
        if (found != initialAchievementIterations_.end())
            achievementIterations_[index] = found->second;
    }
    for (std::size_t index = 0; index < racers_.size(); ++index)
    {
        const auto& sourceRacer = race_.racers[index];
        const std::size_t vehicleIndex = sourceRacer.vehicle;
        const auto& vehicle =
            sourceRacer.hasConfiguredVehicle
                ? sourceRacer.configuredVehicle
                : race_.vehicles.at(std::min(
                      vehicleIndex, race_.vehicles.size() - 1U));
        racers_[index].maximumLife = std::max(vehicle.maximumLife, 1.0F);
        racers_[index].life = racers_[index].maximumLife;
        racers_[index].place = static_cast<std::uint32_t>(index + 1U);
        for (std::size_t weaponIndex = 0;
             weaponIndex < race_.weapons.size(); ++weaponIndex)
        {
            const auto& weapon = race_.weapons[weaponIndex];
            if ((weapon.slot == WeaponSlot::Primary ||
                 weapon.slot == WeaponSlot::Support) &&
                racers_[index].weaponSlots[0] ==
                    RacerRuntime::invalidWeapon)
            {
                racers_[index].weaponSlots[0] = weaponIndex;
                racers_[index].weaponCapacity[0] =
                    std::max(weapon.reloadCharge, 1U);
                racers_[index].weaponCharges[0] =
                    racers_[index].weaponCapacity[0];
            }
            else if (weapon.slot == WeaponSlot::Hyper &&
                     racers_[index].hyperWeapon ==
                         RacerRuntime::invalidWeapon)
            {
                racers_[index].hyperWeapon = weaponIndex;
                racers_[index].hyperCapacity =
                    std::max(weapon.reloadCharge, 1U);
                racers_[index].hyperCharge =
                    racers_[index].hyperCapacity;
            }
            else if (weapon.slot == WeaponSlot::Mine &&
                     racers_[index].mineWeapon ==
                         RacerRuntime::invalidWeapon)
            {
                racers_[index].mineWeapon = weaponIndex;
                racers_[index].mineCapacity =
                    std::max(weapon.reloadCharge, 1U);
                racers_[index].mines =
                    racers_[index].mineCapacity;
            }
        }
        if (index == 0)
        {
            auto& runtime = racers_[index];
            runtime.money = initialPlayerProfile_.money;
            runtime.points = initialPlayerProfile_.points;
            const bool hasProfileLoadout = std::any_of(
                initialPlayerProfile_.slots.begin(),
                initialPlayerProfile_.slots.end(),
                [](const ProfileSlot& slot) {
                    return !slot.record.empty();
                });
            if (hasProfileLoadout)
            {
                runtime.weaponSlots.fill(
                    RacerRuntime::invalidWeapon);
                runtime.weaponCharges.fill(0U);
                runtime.weaponCapacity.fill(0U);
                runtime.hyperWeapon =
                    RacerRuntime::invalidWeapon;
                runtime.hyperCharge = 0;
                runtime.hyperCapacity = 0;
                runtime.mineWeapon =
                    RacerRuntime::invalidWeapon;
                runtime.mines = 0;
                runtime.mineCapacity = 0;
            }
            for (std::size_t slot = 0;
                 slot < PlayerProfile::weaponSlotCount; ++slot)
            {
                const auto& profileSlot =
                    initialPlayerProfile_.slots[
                        PlayerProfile::firstWeaponSlot + slot];
                const auto weapon =
                    findWeapon(profileSlot.record, WeaponSlot::Primary);
                if (weapon != RacerRuntime::invalidWeapon)
                {
                    runtime.weaponSlots[slot] = weapon;
                    runtime.weaponCapacity[slot] =
                        profileSlot.hasCharge
                            ? profileSlot.charge
                            : std::max(
                                  race_.weapons[weapon].reloadCharge, 1U);
                    runtime.weaponCharges[slot] =
                        runtime.weaponCapacity[slot];
                }
            }
            const auto& hyper =
                initialPlayerProfile_.slots[PlayerProfile::hyperSlot];
            const auto hyperWeapon =
                findWeapon(hyper.record, WeaponSlot::Hyper);
            if (hyperWeapon != RacerRuntime::invalidWeapon)
            {
                runtime.hyperWeapon = hyperWeapon;
                runtime.hyperCapacity =
                    hyper.hasCharge
                        ? hyper.charge
                        : std::max(
                              race_.weapons[hyperWeapon].reloadCharge, 1U);
                runtime.hyperCharge = runtime.hyperCapacity;
            }
            const auto& mine =
                initialPlayerProfile_.slots[PlayerProfile::mineSlot];
            const auto mineWeapon =
                findWeapon(mine.record, WeaponSlot::Mine);
            if (mineWeapon != RacerRuntime::invalidWeapon)
            {
                runtime.mineWeapon = mineWeapon;
                runtime.mineCapacity =
                    mine.hasCharge
                        ? mine.charge
                        : std::max(
                              race_.weapons[mineWeapon].reloadCharge, 1U);
                runtime.mines = runtime.mineCapacity;
            }
        }
        else if (!sourceRacer.loadout.empty())
        {
            auto& runtime = racers_[index];
            runtime.weaponSlots.fill(RacerRuntime::invalidWeapon);
            runtime.weaponCharges.fill(0U);
            runtime.weaponCapacity.fill(0U);
            runtime.hyperWeapon = RacerRuntime::invalidWeapon;
            runtime.hyperCharge = 0;
            runtime.hyperCapacity = 0;
            runtime.mineWeapon = RacerRuntime::invalidWeapon;
            runtime.mines = 0;
            runtime.mineCapacity = 0;
            for (const auto& slot : sourceRacer.loadout)
            {
                if (slot.type.rfind("stWeapon", 0) == 0)
                {
                    const auto number =
                        slot.type.substr(std::string_view("stWeapon").size());
                    if (number.size() != 1U ||
                        number.front() < '1' ||
                        number.front() > '4')
                        continue;
                    const auto target =
                        static_cast<std::size_t>(number.front() - '1');
                    const auto weapon =
                        findWeapon(slot.record, WeaponSlot::Primary);
                    if (weapon != RacerRuntime::invalidWeapon)
                    {
                        runtime.weaponSlots[target] = weapon;
                        runtime.weaponCapacity[target] = slot.charge;
                        runtime.weaponCharges[target] =
                            runtime.weaponCapacity[target];
                    }
                }
                else if (slot.type == "stHyper")
                {
                    runtime.hyperWeapon =
                        findWeapon(slot.record, WeaponSlot::Hyper);
                    runtime.hyperCapacity = slot.charge;
                    runtime.hyperCharge = runtime.hyperCapacity;
                }
                else if (slot.type == "stMine")
                {
                    runtime.mineWeapon =
                        findWeapon(slot.record, WeaponSlot::Mine);
                    runtime.mineCapacity = slot.charge;
                    runtime.mines = runtime.mineCapacity;
                }
            }
        }
        syncSelectedWeapon(racers_[index]);
    }
    events_.push_back(
        {RaceEventKind::CountdownChanged, 0, 0, {}, 3.0F});
}

void OriginalRaceSession::applyPlayerProfile(
    const PlayerProfile& profile)
{
    initialPlayerProfile_ = profile;
    reset();
}

void OriginalRaceSession::writePlayerProfile(
    PlayerProfile& profile) const
{
    if (racers_.empty())
        return;
    const auto& runtime = racers_.front();
    profile.money = runtime.money;
    profile.points = runtime.points;
    auto writeWeapon = [&](std::size_t profileSlot,
                           std::size_t weapon,
                           std::uint32_t charge) {
        auto& slot = profile.slots[profileSlot];
        if (weapon == RacerRuntime::invalidWeapon ||
            weapon >= race_.weapons.size())
        {
            slot = {};
            return;
        }
        slot.record = workshopReference(race_.weapons[weapon].record);
        slot.charge = charge;
        slot.hasCharge = true;
    };
    writeWeapon(PlayerProfile::hyperSlot, runtime.hyperWeapon,
                runtime.hyperCapacity);
    writeWeapon(PlayerProfile::mineSlot, runtime.mineWeapon,
                runtime.mineCapacity);
    for (std::size_t slot = 0;
         slot < PlayerProfile::weaponSlotCount; ++slot)
    {
        writeWeapon(PlayerProfile::firstWeaponSlot + slot,
                    runtime.weaponSlots[slot],
                    runtime.weaponCapacity[slot]);
    }
}

void OriginalRaceSession::applyAchievementProfile(
    const ProfileState& profile)
{
    initialAchievementPoints_ = profile.achievementPoints;
    initialAchievementIterations_ =
        profile.achievementIterations;
    achievementMultiplier_ =
        profile.player.difficulty == "gdHard"
            ? 1.5F
            : (profile.player.difficulty == "gdEasy" ? 1.0F : 1.2F);
    achievementPoints_ = initialAchievementPoints_;
    achievementIterations_.assign(race_.achievements.size(), 0U);
    for (std::size_t index = 0;
         index < race_.achievements.size(); ++index)
    {
        const auto found = initialAchievementIterations_.find(
            race_.achievements[index].name);
        if (found != initialAchievementIterations_.end())
            achievementIterations_[index] = found->second;
    }
}

void OriginalRaceSession::writeAchievementProfile(
    ProfileState& profile) const
{
    profile.achievementPoints = achievementPoints_;
    profile.achievementIterations.clear();
    for (std::size_t index = 0;
         index < race_.achievements.size() &&
         index < achievementIterations_.size();
         ++index)
    {
        profile.achievementIterations[
            race_.achievements[index].name] =
            achievementIterations_[index];
    }
}

void OriginalRaceSession::setEnableMineBug(bool enabled) noexcept
{
    enableMineBug_ = enabled;
}

void OriginalRaceSession::setSpringBorders(bool enabled) noexcept
{
    springBorders_ = enabled;
}

std::size_t OriginalRaceSession::findWeapon(
    std::string_view record, WeaponSlot slot) const noexcept
{
    if (record.empty())
        return RacerRuntime::invalidWeapon;
    const auto wanted = recordName(record);
    for (std::size_t index = 0; index < race_.weapons.size(); ++index)
    {
        const auto& weapon = race_.weapons[index];
        const bool slotMatches =
            weapon.slot == slot ||
            (slot == WeaponSlot::Primary &&
             weapon.slot == WeaponSlot::Support);
        if (slotMatches &&
            (weapon.record == record ||
             recordName(weapon.record) == wanted))
            return index;
    }
    return RacerRuntime::invalidWeapon;
}

void OriginalRaceSession::syncSelectedWeapon(
    RacerRuntime& racer) const noexcept
{
    for (std::size_t offset = 0;
         offset < racer.weaponSlots.size(); ++offset)
    {
        const auto slot =
            (racer.selectedWeaponSlot + offset) %
            racer.weaponSlots.size();
        const auto weapon = racer.weaponSlots[slot];
        if (weapon == RacerRuntime::invalidWeapon ||
            weapon >= race_.weapons.size())
            continue;
        racer.selectedWeaponSlot = slot;
        racer.selectedWeapon = weapon;
        racer.ammunition = racer.weaponCharges[slot];
        return;
    }
    racer.selectedWeapon = RacerRuntime::invalidWeapon;
    racer.ammunition = 0;
}

float OriginalRaceSession::damageAfterSupport(
    std::size_t racer, float damage, bool touchDamage) const noexcept
{
    if (touchDamage || racer >= racers_.size())
        return damage;
    float result = damage;
    for (const auto weaponIndex : racers_[racer].weaponSlots)
    {
        if (weaponIndex == RacerRuntime::invalidWeapon ||
            weaponIndex >= race_.weapons.size())
            continue;
        const auto& weapon = race_.weapons[weaponIndex];
        if (weapon.slot == WeaponSlot::Support &&
            weapon.reflectValue > 0.0F)
        {
            result *= std::clamp(
                1.0F - weapon.reflectValue, 0.0F, 1.0F);
        }
    }
    return result;
}

bool OriginalRaceSession::damageDecorationAlongSegment(
    Vec3 origin, Vec3 target, float damage,
    std::size_t attacker, float radius)
{
    const Vec3 segment = subtract(target, origin);
    const float segmentLengthSquared =
        std::max(dot3(segment, segment), 0.0001F);
    std::size_t hit = decorationActive_.size();
    float hitCoordinate = std::numeric_limits<float>::max();
    for (std::size_t index = 0;
         index < race_.decorationInstances.size() &&
         index < decorationActive_.size(); ++index)
    {
        if (!decorationActive_[index])
            continue;
        const auto& instance = race_.decorationInstances[index];
        const auto& definition =
            race_.decorationDefinitions.at(instance.definition);
        if (!definition.destructible)
            continue;
        const Vec3 toObject =
            subtract(instance.transform.position, origin);
        const float coordinate = std::clamp(
            dot3(toObject, segment) / segmentLengthSquared,
            0.0F, 1.0F);
        if (coordinate >= hitCoordinate)
            continue;
        const Vec3 closest =
            add(origin, multiply(segment, coordinate));
        if (distanceSquared(closest, instance.transform.position) >
            radius * radius)
            continue;
        hit = index;
        hitCoordinate = coordinate;
    }
    if (hit >= decorationActive_.size())
        return false;
    decorationLife_[hit] -= std::max(damage, 0.0F);
    if (decorationLife_[hit] > 0.0F)
        return true;
    decorationActive_[hit] = false;
    const Vec3 position =
        race_.decorationInstances[hit].transform.position;
    events_.push_back(
        {RaceEventKind::DecorationDestroyed, attacker, hit,
         position, damage});
    effects_.push_back(
        {RaceEventKind::DecorationDestroyed, position,
         add(position, {0.0F, 0.0F, 3.0F}),
         0.5F, 0.5F, race_.weapons.size()});
    return true;
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

const std::vector<MineRuntime>& OriginalRaceSession::mines() const noexcept
{
    return mines_;
}

const std::vector<ProjectileRuntime>&
OriginalRaceSession::projectiles() const noexcept
{
    return projectiles_;
}

std::vector<RespawnRequest> OriginalRaceSession::takeRespawns()
{
    auto result = std::move(respawns_);
    respawns_.clear();
    return result;
}

std::vector<VelocityRequest>
OriginalRaceSession::takeVelocityRequests()
{
    auto result = std::move(velocityRequests_);
    velocityRequests_.clear();
    return result;
}

std::vector<AngularVelocityRequest>
OriginalRaceSession::takeAngularVelocityRequests()
{
    auto result = std::move(angularVelocityRequests_);
    angularVelocityRequests_.clear();
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
        const auto finishedBefore = std::count_if(
            racers_.begin(), racers_.end(),
            [&](const RacerRuntime& candidate) {
                return &candidate != &runtime && candidate.finished;
            });
        runtime.place =
            static_cast<std::uint32_t>(finishedBefore + 1U);
        const std::size_t reward =
            std::min<std::size_t>(
                runtime.place > 0 ? runtime.place - 1U : 0U,
                race_.rewardMoney.size() - 1U);
        runtime.rewardMoney = race_.rewardMoney[reward];
        runtime.rewardPoints = race_.rewardPoints[reward];
        vehicleInputs_[racer] = {};
        events_.push_back({RaceEventKind::Finish, racer, 0,
                           vehicle.body.position, runtime.finishTime});
        if (racer == 0)
        {
            runtime.money += runtime.rewardMoney + runtime.pickedMoney;
            runtime.points += runtime.rewardPoints;
            phase_ = RacePhase::Finished;
        }
    }
}

r3d::physics::VehicleInput OriginalRaceSession::aiInput(
    std::size_t racer, const r3d::physics::VehicleState& vehicle,
    float seconds)
{
    if (racers_[racer].finished)
        return {};
    constexpr float steerAngleBias =
        3.14159265358979323846F / 128.0F;
    constexpr float maximumSpeedBlocking = 0.5F;
    constexpr float maximumTimeBlocking = 1.0F;
    constexpr float steerControl = 1.0F;

    const auto& target = tracePoint(racers_[racer].nextPathNode);
    const Vec3 wanted = normalized2(subtract(target.position,
                                             vehicle.body.position));
    const Vec3 carForward = normalized2(forward(vehicle.body.rotation));
    const float cross = carForward.x * wanted.y - carForward.y * wanted.x;
    const float alignment =
        std::clamp(dot2(carForward, wanted), -1.0F, 1.0F);
    float steeringAngle = std::acos(alignment);
    if (steeringAngle > steerAngleBias)
        steeringAngle = cross > 0.0F ? steeringAngle : -steeringAngle;
    else
        steeringAngle = 0.0F;

    const std::size_t previousNode =
        racers_[racer].nextPathNode > 0U
            ? racers_[racer].nextPathNode - 1U
            : 0U;
    const auto& previous = tracePoint(previousNode);
    const auto& following = tracePoint(std::min(
        racers_[racer].nextPathNode + 1U,
        race_.tracePath.size() - 1U));
    const Vec3 currentDirection =
        normalized2(subtract(target.position, previous.position));
    const Vec3 followingDirection =
        normalized2(subtract(following.position, target.position));
    const float turnAngle = std::acos(std::clamp(
        dot2(currentDirection, followingDirection), -1.0F, 1.0F));
    if (turnAngle > 3.14159265358979323846F / 12.0F)
    {
        const float distanceToTurn = std::max(
            dot2(subtract(target.position, vehicle.body.position),
                 currentDirection),
            0.0F);
        const float brakeDistance = distanceToTurn + target.width;
        const float rotation =
            1.0F - std::max(dot2(carForward, followingDirection), 0.0F);
        const float demand =
            vehicle.speed * vehicle.speed * steerControl *
            steerControl * rotation;
        if (!aiBrake_[racer] && demand > 1.5F * brakeDistance)
            aiBrake_[racer] = true;
        else if (aiBrake_[racer] && demand < brakeDistance)
            aiBrake_[racer] = false;
    }
    else
    {
        aiBrake_[racer] = false;
    }

    if (std::abs(vehicle.speed) < maximumSpeedBlocking)
        stuckSeconds_[racer] += seconds;
    else
        stuckSeconds_[racer] = 0.0F;

    r3d::physics::VehicleInput input;
    const auto& racerDefinition = race_.racers[racer];
    const auto& vehicleDefinition =
        racerDefinition.hasConfiguredVehicle
            ? racerDefinition.configuredVehicle
            : race_.vehicles.at(racerDefinition.vehicle);
    input.steering = clampSteering(
        steeringAngle /
        std::max(vehicleDefinition.physics.steerAngle, 0.01F));
    input.throttle = aiBrake_[racer] ? 0.0F : 1.0F;
    input.brake = aiBrake_[racer] ? 1.0F : 0.0F;
    if (racers_[racer].speedBoostSeconds > 0.0F)
        input.throttle = 1.0F;
    if (stuckSeconds_[racer] > 3.0F * maximumTimeBlocking)
        input = {};
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
    if (!racers_.empty() && humanControl.weaponSlot >= 0 &&
        humanControl.weaponSlot <
            static_cast<int>(PlayerProfile::weaponSlotCount))
    {
        const auto slot =
            static_cast<std::size_t>(humanControl.weaponSlot);
        if (racers_[0].weaponSlots[slot] !=
            RacerRuntime::invalidWeapon)
        {
            racers_[0].selectedWeaponSlot = slot;
            syncSelectedWeapon(racers_[0]);
        }
    }
    if (humanControl.changeWeapon && !racers_.empty())
    {
        auto& runtime = racers_[0];
        std::vector<std::size_t> usable;
        for (std::size_t slot = 0;
             slot < runtime.weaponSlots.size(); ++slot)
        {
            if (runtime.weaponSlots[slot] ==
                RacerRuntime::invalidWeapon)
                continue;
            usable.push_back(slot);
        }
        if (!usable.empty())
        {
            const auto current = std::find(
                usable.begin(), usable.end(),
                runtime.selectedWeaponSlot);
            const auto currentIndex =
                current == usable.end()
                    ? 0
                    : static_cast<int>(current - usable.begin());
            const int wanted = std::clamp(
                currentIndex +
                    (humanControl.weaponChange < 0 ? -1 : 1),
                0, static_cast<int>(usable.size()) - 1);
            runtime.selectedWeaponSlot =
                usable[static_cast<std::size_t>(wanted)];
            syncSelectedWeapon(runtime);
        }
    }
    auto projectileWorldTransform =
        [&](std::size_t owner, std::size_t weaponIndex,
            std::size_t mountSlot,
            const ProjectileDefinition& projectile) {
            Transform result = vehicles[owner].body;
            const auto& racerDefinition = race_.racers[owner];
            const auto& vehicleDefinition =
                racerDefinition.hasConfiguredVehicle
                    ? racerDefinition.configuredVehicle
                    : race_.vehicles.at(racerDefinition.vehicle);
            if (mountSlot < vehicleDefinition.weaponMounts.size())
            {
                const auto& mount =
                    vehicleDefinition.weaponMounts[mountSlot];
                Transform local;
                local.position = mount.position;
                const auto wanted =
                    recordName(race_.weapons[weaponIndex].record);
                const auto placement = std::find_if(
                    mount.placements.begin(),
                    mount.placements.end(),
                    [&](const VehicleWeaponPlacement& item) {
                        return recordName(item.record) == wanted;
                    });
                if (placement != mount.placements.end())
                {
                    local.position = add(
                        local.position, placement->offset);
                    local.rotation = placement->rotation;
                }
                result = compose(result, local);
            }
            result = compose(
                result, race_.weapons[weaponIndex].visual.transform);
            Transform localProjectile;
            localProjectile.position = projectile.position;
            localProjectile.rotation = projectile.rotation;
            return compose(result, localProjectile);
        };
    for (std::size_t racer = 0; racer < racers_.size(); ++racer)
    {
        auto& runtime = racers_[racer];
        runtime.shieldSeconds =
            std::max(0.0F, runtime.shieldSeconds - seconds);
        runtime.speedBoostSeconds =
            std::max(0.0F, runtime.speedBoostSeconds - seconds);
        runtime.slowSeconds =
            std::max(0.0F, runtime.slowSeconds - seconds);
        runtime.clutchSeconds =
            std::max(0.0F, runtime.clutchSeconds - seconds);
        for (auto& cooldown : weaponCooldown_[racer])
            cooldown = std::max(0.0F, cooldown - seconds);
        mineCooldown_[racer] =
            std::max(0.0F, mineCooldown_[racer] - seconds);
        hyperCooldown_[racer] =
            std::max(0.0F, hyperCooldown_[racer] - seconds);
        const WeaponDefinition* repair = nullptr;
        for (const auto weaponIndex : runtime.weaponSlots)
        {
            if (weaponIndex == RacerRuntime::invalidWeapon ||
                weaponIndex >= race_.weapons.size())
                continue;
            const auto& candidate = race_.weapons[weaponIndex];
            if (candidate.slot == WeaponSlot::Support &&
                candidate.repairPeriod > 0.0F)
            {
                repair = &candidate;
                break;
            }
        }
        if (repair == nullptr ||
            runtime.life >= runtime.maximumLife)
        {
            repairSeconds_[racer] = 0.0F;
        }
        else if ((repairSeconds_[racer] += seconds) >
                 repair->repairPeriod)
        {
            repairSeconds_[racer] -= repair->repairPeriod;
            runtime.life = std::min(
                runtime.maximumLife,
                runtime.life +
                    (repair->repairValue > 0.0F
                         ? repair->repairValue
                         : 5.0F));
        }
        if (racer < vehicles.size())
            updateProgress(racer, vehicles[racer]);
    }

    auto damageFromContact =
        [](const std::array<float, 2>& damage,
           const std::array<float, 2>& forceRange,
           float force, float& forcePart) {
        forcePart = 0.0F;
        if (forceRange[1] > forceRange[0])
        {
            forcePart = std::clamp(
                (force - forceRange[0]) /
                    (forceRange[1] - forceRange[0]),
                0.0F, 1.0F);
        }
        else if (force > forceRange[0])
        {
            forcePart = 0.5F;
        }
        return damage[0] + (damage[1] - damage[0]) * forcePart;
    };
    auto applyTouchDamage =
        [&](std::size_t target, std::size_t attacker,
            float damage, Vec3 position) {
        if (target >= racers_.size() || damage <= 0.0F)
            return;
        const float applied =
            racers_[target].shieldSeconds > 0.0F ? 0.0F : damage;
        racers_[target].life =
            std::max(0.0F, racers_[target].life - applied);
        events_.push_back(
            {RaceEventKind::Damage, target, attacker, position, applied,
             PickSlot::None, RacerRuntime::invalidWeapon, true});
        if (racers_[target].life > 0.0F)
            return;
        events_.push_back(
            {RaceEventKind::Kill, attacker, target, position, 0.0F,
             PickSlot::None, RacerRuntime::invalidWeapon, true});
        racers_[target].life = racers_[target].maximumLife;
        queueRespawn(target, vehicles[target]);
    };

    for (std::size_t racer = 0;
         racer < vehicles.size() && racer < racers_.size(); ++racer)
    {
        for (const auto& contact : vehicles[racer].bodyContacts)
        {
            if (contact.surface !=
                    r3d::physics::CollisionSurface::TrackBorder ||
                std::abs(contact.normal.z) >= 0.5F)
                continue;
            racers_[racer].clutchSeconds = 0.0F;
            float forcePart = 0.0F;
            const float damage = damageFromContact(
                race_.touchBorderDamage, race_.touchBorderDamageForce,
                contact.force, forcePart);
            if ((!springBorders_ && forcePart == 0.0F) ||
                vehicles[racer].speed <= 16.0F)
                continue;

            if (springBorders_)
            {
                const Vec3 normal = normalized3(contact.normal);
                const Vec3 velocity = vehicles[racer].linearVelocity;
                Vec3 tangent = subtract(
                    velocity, multiply(normal, dot3(normal, velocity)));
                const float tangentLength = length3(tangent);
                if (tangentLength > 0.0001F)
                    tangent = multiply(tangent, 1.0F / tangentLength);
                else
                    tangent = {};
                const Vec3 travel = normalized3(velocity);
                const float tangentDot =
                    std::abs(dot3(travel, normal));
                const Vec3 direction =
                    normalized3(forward(vehicles[racer].body.rotation));
                const float directionDot = dot3(direction, normal);
                const float directionTravelDot =
                    dot3(direction, travel);
                if (tangentDot > 0.1F &&
                    (directionDot < 0.707F ||
                     directionTravelDot < -0.707F))
                {
                    const float normalVelocity = std::clamp(
                        std::abs(dot3(normal, velocity)), 4.0F, 14.0F);
                    const float tangentVelocity =
                        dot3(tangent, velocity) * 0.5F;
                    Vec3 wanted = add(
                        multiply(normal, normalVelocity),
                        multiply(tangent, tangentVelocity));
                    wanted.z = 0.0F;
                    velocityRequests_.push_back(
                        {racer, subtract(wanted, velocity)});
                }
            }
            if (forcePart > 0.0F && damage > 0.0F)
                applyTouchDamage(
                    racer, racer, damage,
                    vehicles[racer].body.position);
        }
    }
    for (auto& cooldown : touchCooldown_)
        cooldown = std::max(0.0F, cooldown - seconds);

    auto spawnProjectileImpact =
        [&](const ProjectileRuntime& projectile, const Vec3& position) {
            if (projectile.weapon >= race_.weapons.size() ||
                projectile.projectile >=
                    race_.weapons[projectile.weapon]
                        .projectiles.size())
                return;
            const auto& definition =
                race_.weapons[projectile.weapon]
                    .projectiles[projectile.projectile];
            auto addVisual =
                [&](const ObjectDefinition& visual,
                    std::uint8_t variant) {
                    if (visual.visualNodes.empty() &&
                        visual.particleEmitters.empty())
                        return;
                    effects_.push_back(
                        {RaceEventKind::ProjectileImpact, position,
                         add(position, projectile.direction), 0.9F,
                         0.9F, projectile.weapon,
                         projectile.projectile, variant});
                };
            addVisual(definition.secondaryVisual, 1U);
            addVisual(definition.tertiaryVisual, 2U);
        };

    for (auto& projectile : projectiles_)
    {
        if (!projectile.active ||
            projectile.weapon >= race_.weapons.size() ||
            projectile.projectile >=
                race_.weapons[projectile.weapon].projectiles.size())
            continue;
        const auto& projectileDefinition =
            race_.weapons[projectile.weapon]
                .projectiles[projectile.projectile];
        projectile.ageSeconds += seconds;
        if (projectile.attached)
        {
            if (projectile.owner >= vehicles.size() ||
                projectile.owner >= racers_.size())
            {
                projectile.active = false;
                continue;
            }
            projectile.lifeSeconds -= seconds;
            const auto shotTransform = projectileWorldTransform(
                projectile.owner, projectile.weapon,
                projectile.mountSlot, projectileDefinition);
            projectile.position = shotTransform.position;
            projectile.direction = normalized3(
                rotate(shotTransform.rotation,
                       {1.0F, 0.0F, 0.0F}));
            const float maximumDistance =
                projectile.maximumDistance > 0.0F
                    ? projectile.maximumDistance
                    : 3.0F;
            projectile.impactDistance = maximumDistance;
            const Vec3 end = add(
                projectile.position,
                multiply(projectile.direction, maximumDistance));
            effects_.push_back(
                {RaceEventKind::WeaponFired, projectile.position, end,
                 std::max(seconds, 0.03F),
                 std::max(seconds, 0.03F), projectile.weapon,
                 projectile.projectile});
            for (std::size_t target = 0;
                 target < vehicles.size() &&
                 target < racers_.size(); ++target)
            {
                if (target == projectile.owner ||
                    racers_[target].finished)
                    continue;
                const Vec3 difference = subtract(
                    vehicles[target].body.position,
                    projectile.position);
                const float forwardDistance =
                    dot3(projectile.direction, difference);
                if (forwardDistance < 0.0F ||
                    forwardDistance > maximumDistance)
                    continue;
                const auto& racerDefinition = race_.racers[target];
                const auto& vehicleDefinition =
                    racerDefinition.hasConfiguredVehicle
                        ? racerDefinition.configuredVehicle
                        : race_.vehicles.at(racerDefinition.vehicle);
                const float radius = std::max(
                    {vehicleDefinition.physics.halfExtents.x,
                     vehicleDefinition.physics.halfExtents.y, 0.5F});
                const float lateralDistance = length3(subtract(
                    difference,
                    multiply(projectile.direction,
                             forwardDistance)));
                if (lateralDistance >= radius)
                    continue;
                projectile.impactDistance =
                    std::min(projectile.impactDistance,
                             forwardDistance);
                const float damage =
                    racers_[target].shieldSeconds > 0.0F
                        ? 0.0F
                        : damageAfterSupport(
                              target,
                              std::max(
                                  projectileDefinition.damage * seconds,
                                  0.0F),
                              false);
                racers_[target].life =
                    std::max(0.0F, racers_[target].life - damage);
                events_.push_back(
                    {RaceEventKind::Damage, target, projectile.owner,
                     vehicles[target].body.position, damage});
                if (projectileDefinition.type == 18U)
                {
                    racers_[target].slowSeconds = std::max(
                        racers_[target].slowSeconds,
                        std::max(projectileDefinition.minimumLife, 1.0F));
                }
                if (racers_[target].life <= 0.0F)
                {
                    events_.push_back(
                        {RaceEventKind::Kill, projectile.owner, target,
                         vehicles[target].body.position, 0.0F});
                    racers_[target].life =
                        racers_[target].maximumLife;
                    queueRespawn(target, vehicles[target]);
                }
            }
            damageDecorationAlongSegment(
                projectile.position, end,
                std::max(projectileDefinition.damage * seconds,
                         0.0F),
                projectile.owner,
                std::max(projectileDefinition.size.y * 0.5F,
                         1.0F));
            if (projectile.lifeSeconds <= 0.0F)
                projectile.active = false;
            continue;
        }
        if ((projectileDefinition.type == 2U ||
             projectileDefinition.type == 21U) &&
            projectile.target < vehicles.size() &&
            projectile.target < racers_.size() &&
            !racers_[projectile.target].finished)
        {
            projectile.homingDelay =
                std::max(0.0F, projectile.homingDelay - seconds);
            if (projectile.homingDelay <= 0.0F)
            {
                const Vec3 targetDirection = normalized3(subtract(
                    vehicles[projectile.target].body.position,
                    projectile.position));
                if (length3(targetDirection) > 0.0F)
                {
                    if (projectile.angularSpeed > 0.0F)
                    {
                        const float alpha = std::clamp(
                            seconds * projectile.angularSpeed,
                            0.0F, 1.0F);
                        projectile.direction = normalized3(add(
                            multiply(projectile.direction, 1.0F - alpha),
                            multiply(targetDirection, alpha)));
                    }
                    else
                    {
                        projectile.direction = targetDirection;
                    }
                    projectile.velocity = multiply(
                        projectile.direction, projectile.speed);
                }
            }
        }
        const Vec3 previous = projectile.position;
        const float speed = std::max(projectile.speed, 1.0F);
        const float maximumDistance =
            projectile.maximumDistance > 0.0F
                ? projectile.maximumDistance
                : 100.0F;
        if (projectile.ballistic)
            projectile.velocity.z -= 20.0F * seconds;
        Vec3 movement = projectile.ballistic
                            ? multiply(projectile.velocity, seconds)
                            : multiply(projectile.direction,
                                       speed * seconds);
        const float remaining =
            std::max(maximumDistance - projectile.distance, 0.0F);
        float step = length3(movement);
        if (step > remaining && step > 0.0F)
        {
            movement = multiply(movement, remaining / step);
            step = remaining;
        }
        projectile.position = add(projectile.position, movement);
        if (length3(movement) > 0.0001F)
            projectile.direction = normalized3(movement);
        projectile.distance += step;
        projectile.reflectionCooldown =
            std::max(0.0F,
                     projectile.reflectionCooldown - seconds);
        if (projectileDefinition.type == 22U &&
            projectile.reflectionCooldown <= 0.0F)
        {
            float nearestDistance =
                std::numeric_limits<float>::max();
            Vec3 nearestPoint;
            float nearestWidth = 0.0F;
            for (std::size_t node = 1U;
                 node < race_.tracePath.size(); ++node)
            {
                const auto& first = tracePoint(node - 1U);
                const auto& second = tracePoint(node);
                const Vec3 segment =
                    subtract(second.position, first.position);
                const float segmentLengthSquared =
                    std::max(dot2(segment, segment), 0.0001F);
                const float coordinate = std::clamp(
                    dot2(subtract(projectile.position,
                                  first.position),
                         segment) /
                        segmentLengthSquared,
                    0.0F, 1.0F);
                const Vec3 closest = add(
                    first.position,
                    multiply(segment, coordinate));
                const float distance = length2(subtract(
                    projectile.position, closest));
                if (distance >= nearestDistance)
                    continue;
                nearestDistance = distance;
                nearestPoint = closest;
                nearestWidth =
                    first.width +
                    (second.width - first.width) * coordinate;
            }
            if (nearestDistance >
                std::max(nearestWidth * 0.5F, 2.0F))
            {
                const Vec3 normal = normalized2(subtract(
                    projectile.position, nearestPoint));
                const float alignment =
                    dot2(projectile.direction, normal);
                if (alignment > 0.0F)
                {
                    if (std::abs(alignment) > 0.1F)
                    {
                        projectile.direction = normalized3(
                            subtract(
                                projectile.direction,
                                multiply(normal,
                                         2.0F * alignment)));
                    }
                    else
                    {
                        projectile.direction = multiply(
                            projectile.direction, -1.0F);
                    }
                    projectile.velocity = multiply(
                        projectile.direction, projectile.speed);
                    projectile.position = previous;
                    projectile.reflectionCooldown = 0.1F;
                }
            }
        }
        effects_.push_back(
            {RaceEventKind::WeaponFired, previous,
             projectile.position, std::max(seconds, 0.03F),
             std::max(seconds, 0.03F), projectile.weapon,
             projectile.projectile});

        for (std::size_t target = 0;
             target < vehicles.size() && target < racers_.size();
             ++target)
        {
            if (target == projectile.owner ||
                racers_[target].finished)
                continue;
            if (projectileDefinition.type == 21U &&
                projectile.target < racers_.size() &&
                target != projectile.target)
                continue;
            const auto segment =
                subtract(projectile.position, previous);
            const float segmentLengthSquared =
                std::max(dot3(segment, segment), 0.0001F);
            const auto toTarget =
                subtract(vehicles[target].body.position, previous);
            const float coordinate = std::clamp(
                dot3(toTarget, segment) / segmentLengthSquared,
                0.0F, 1.0F);
            const auto closest =
                add(previous, multiply(segment, coordinate));
            const auto& racerDefinition = race_.racers[target];
            const auto& vehicleDefinition =
                racerDefinition.hasConfiguredVehicle
                    ? racerDefinition.configuredVehicle
                    : race_.vehicles.at(racerDefinition.vehicle);
            const float radius = std::max(
                {vehicleDefinition.physics.halfExtents.x,
                 vehicleDefinition.physics.halfExtents.y, 0.5F});
            if (distanceSquared(closest,
                                vehicles[target].body.position) >
                radius * radius)
                continue;
            const float damage =
                racers_[target].shieldSeconds > 0.0F
                    ? 0.0F
                    : damageAfterSupport(
                          target,
                          std::max(projectile.damage, 0.0F),
                          false);
            racers_[target].life =
                std::max(0.0F, racers_[target].life - damage);
            events_.push_back(
                {RaceEventKind::Damage, target, projectile.owner,
                 projectile.position, damage});
            spawnProjectileImpact(
                projectile, projectile.position);
            if (racers_[target].life <= 0.0F)
            {
                events_.push_back(
                    {RaceEventKind::Kill, projectile.owner, target,
                     projectile.position, 0.0F});
                racers_[target].life =
                    racers_[target].maximumLife;
                queueRespawn(target, vehicles[target]);
            }
            if (projectileDefinition.type == 21U &&
                projectile.target < racers_.size() &&
                ++projectile.hitCount <= 2U)
            {
                std::size_t nextTarget = RacerRuntime::invalidWeapon;
                float nextDistance = std::numeric_limits<float>::max();
                for (std::size_t candidate = 0;
                     candidate < vehicles.size() &&
                     candidate < racers_.size();
                     ++candidate)
                {
                    if (candidate == projectile.owner ||
                        candidate == target ||
                        racers_[candidate].finished)
                        continue;
                    const Vec3 difference = subtract(
                        vehicles[candidate].body.position,
                        projectile.position);
                    const float candidateDistance = length2(difference);
                    if (candidateDistance <= 0.001F ||
                        candidateDistance >= nextDistance)
                        continue;
                    const Vec3 candidateDirection =
                        normalized2(difference);
                    if (dot2(projectile.direction,
                             candidateDirection) < 0.0F)
                        continue;
                    nextTarget = candidate;
                    nextDistance = candidateDistance;
                }
                if (nextTarget != RacerRuntime::invalidWeapon)
                {
                    projectile.target = nextTarget;
                    projectile.homingDelay = 0.0F;
                    projectile.damage =
                        projectileDefinition.damage /
                        static_cast<float>(projectile.hitCount + 1U);
                    break;
                }
            }
            projectile.active = false;
            break;
        }
        if (projectile.active &&
            damageDecorationAlongSegment(
                previous, projectile.position,
                projectile.damage, projectile.owner,
                std::max(
                    {projectileDefinition.size.x,
                     projectileDefinition.size.y,
                     projectileDefinition.size.z, 1.0F}) *
                    0.5F))
        {
            spawnProjectileImpact(
                projectile, projectile.position);
            projectile.active = false;
        }
        if (projectile.active &&
            projectile.distance >= maximumDistance)
        {
            spawnProjectileImpact(
                projectile, projectile.position);
            projectile.active = false;
        }
    }
    projectiles_.erase(
        std::remove_if(
            projectiles_.begin(), projectiles_.end(),
            [](const ProjectileRuntime& projectile) {
                return !projectile.active;
            }),
        projectiles_.end());

    if (!vehicles.empty())
    {
        if (humanControl.reset)
            queueRespawn(0, vehicles[0]);
        if (vehicles[0].contactCount == 0 &&
            vehicles[0].body.position.z < -5.0F)
            queueRespawn(0, vehicles[0]);
        previousPositions_[0] = vehicles[0].body.position;
    }
    for (std::size_t racer = 1;
         racer < vehicles.size() && racer < racers_.size(); ++racer)
    {
        if (stuckSeconds_[racer] > 3.0F)
            queueRespawn(racer, vehicles[racer]);
    }

    auto placeMine = [&](std::size_t owner) {
        if (owner >= vehicles.size() || owner >= racers_.size() ||
            racers_[owner].mines == 0 || mineCooldown_[owner] > 0.0F)
            return;
        const std::size_t weapon = racers_[owner].mineWeapon;
        if (weapon == RacerRuntime::invalidWeapon ||
            weapon >= race_.weapons.size())
            return;
        const auto direction =
            normalized2(forward(vehicles[owner].body.rotation));
        const Vec3 position =
            add(vehicles[owner].body.position,
                multiply(direction, -2.2F));
        --racers_[owner].mines;
        mineCooldown_[owner] = 0.75F;
        const auto& projectiles = race_.weapons[weapon].projectiles;
        const auto* projectile =
            projectiles.empty() ? nullptr : &projectiles.front();
        MineRuntime mine;
        mine.owner = owner;
        mine.weapon = weapon;
        mine.projectile = 0U;
        mine.position = position;
        if (projectile != nullptr)
        {
            mine.damage = projectile->damage;
            mine.type = projectile->type;
            if (projectile->minimumLife > 0.0F)
                mine.maximumLife = projectile->minimumLife;
        }
        mines_.push_back(mine);
        events_.push_back({RaceEventKind::MinePlaced, owner, weapon,
                           position, 0.0F});
    };
    if (humanControl.useMine)
        placeMine(0);
    auto activateHyper = [&](std::size_t owner) {
        if (owner >= racers_.size() ||
            racers_[owner].hyperCharge == 0 ||
            racers_[owner].hyperWeapon ==
                RacerRuntime::invalidWeapon ||
            racers_[owner].hyperWeapon >= race_.weapons.size() ||
            hyperCooldown_[owner] > 0.0F)
            return;
        const auto& weapon =
            race_.weapons[racers_[owner].hyperWeapon];
        if (weapon.projectiles.empty())
            return;
        const auto& projectile = weapon.projectiles.front();
        if (projectile.type == 17U &&
            (owner >= vehicles.size() ||
             vehicles[owner].contactCount == 0U))
            return;
        --racers_[owner].hyperCharge;
        hyperCooldown_[owner] = 0.75F;
        const Vec3 position =
            owner < vehicles.size() ? vehicles[owner].body.position
                                    : Vec3{};
        const float duration =
            projectile.minimumLife > 0.0F
                ? projectile.minimumLife
                : (projectile.type == 17U ? 0.5F : 2.0F);
        if (owner < vehicles.size())
        {
            if (projectile.type == 17U)
            {
                velocityRequests_.push_back(
                    {owner, {0.0F, 0.0F, projectile.speed}});
            }
            else
            {
                const Vec3 direction = normalized2(
                    forward(vehicles[owner].body.rotation));
                velocityRequests_.push_back(
                    {owner, multiply(direction, projectile.speed)});
            }
        }
        events_.push_back(
            {RaceEventKind::HyperActivated, owner,
             racers_[owner].hyperWeapon, position, duration});
        effects_.push_back(
            {RaceEventKind::HyperActivated, position,
             add(position, {0.0F, 0.0F, 2.5F}), 0.4F, 0.4F,
             racers_[owner].hyperWeapon, 0U});
    };
    if (humanControl.useHyper)
        activateHyper(0);

    std::vector<MineRuntime> spawnedMines;
    for (auto& mine : mines_)
    {
        if (!mine.active)
            continue;
        mine.seconds += seconds;
        if (length2(mine.velocity) > 0.0F ||
            std::abs(mine.velocity.z) > 0.0F)
        {
            mine.position = add(
                mine.position, multiply(mine.velocity, seconds));
            mine.velocity.z -= 20.0F * seconds;
            if (mine.position.z < 0.05F)
            {
                mine.position.z = 0.05F;
                mine.velocity = {};
            }
        }
        if (mine.type == 12U)
        {
            const auto& projectile =
                race_.weapons[mine.weapon].projectiles.front();
            const float splitTime =
                projectile.angularSpeed > 0.0F
                    ? projectile.angularSpeed
                    : 2.0F;
            if (mine.seconds >= splitTime)
            {
                MineRuntime core = mine;
                core.type = 11U;
                core.visualVariant = 1U;
                core.damage = 10.0F;
                core.seconds = 0.0F;
                core.maximumLife = 4.25F;
                core.velocity = {};
                spawnedMines.push_back(core);
                constexpr float pi =
                    3.14159265358979323846F;
                for (std::size_t piece = 0; piece < 5U; ++piece)
                {
                    const float angle =
                        2.0F * pi *
                        static_cast<float>(piece) / 5.0F;
                    MineRuntime fragment = core;
                    fragment.type = 13U;
                    fragment.visualVariant = 2U;
                    fragment.damage = 4.0F;
                    fragment.seconds = 0.0F;
                    fragment.velocity = {
                        std::cos(angle) * 8.0F,
                        std::sin(angle) * 8.0F, 5.0F};
                    spawnedMines.push_back(fragment);
                }
                effects_.push_back(
                    {RaceEventKind::DecorationDestroyed,
                     mine.position,
                     add(mine.position, {0.0F, 0.0F, 3.0F}),
                     0.5F, 0.5F, race_.weapons.size()});
                mine.active = false;
                continue;
            }
        }
        if (mine.seconds < 0.25F)
            continue;
        for (std::size_t racer = 0;
             racer < vehicles.size() && racer < racers_.size(); ++racer)
        {
            const bool ownerLocked =
                racer == mine.owner && mine.seconds < 0.4F &&
                (mine.type == 10U ||
                 (enableMineBug_ &&
                  (mine.type == 11U || mine.type == 12U)));
            if (ownerLocked ||
                distanceSquared(vehicles[racer].body.position,
                                mine.position) > 12.25F)
                continue;
            if (mine.type == 10U)
            {
                if (racers_[racer].clutchSeconds > 0.0F ||
                    vehicles[racer].speed <= 3.0F)
                    continue;
                const Vec3 direction = normalized2(
                    forward(vehicles[racer].body.rotation));
                const Vec3 right{-direction.y, direction.x, 0.0F};
                const float side = dot2(
                    right,
                    subtract(mine.position,
                             vehicles[racer].body.position));
                const float strength =
                    std::abs(side) > 0.1F && side > 0.0F
                        ? -mine.damage
                        : mine.damage;
                racers_[racer].clutchSeconds = 0.38F;
                angularVelocityRequests_.push_back(
                    {racer, {0.0F, 0.0F, strength}});
                events_.push_back(
                    {RaceEventKind::Damage, racer, mine.owner,
                     mine.position, 0.0F});
                continue;
            }
            const float damage =
                racers_[racer].shieldSeconds > 0.0F
                    ? 0.0F
                    : damageAfterSupport(
                          racer,
                          std::max(mine.damage, 0.0F),
                          false);
            racers_[racer].life =
                std::max(0.0F, racers_[racer].life - damage);
            events_.push_back({RaceEventKind::Damage, racer, mine.owner,
                               mine.position, damage});
            effects_.push_back(
                {RaceEventKind::DecorationDestroyed, mine.position,
                 add(mine.position, {0.0F, 0.0F, 3.0F}), 0.5F,
                 0.5F, race_.weapons.size()});
            if (racers_[racer].life <= 0.0F)
            {
                events_.push_back(
                    {RaceEventKind::Kill, mine.owner, racer,
                     mine.position, 0.0F});
                racers_[racer].life =
                    racers_[racer].maximumLife;
                queueRespawn(racer, vehicles[racer]);
            }
            mine.active = false;
            break;
        }
    }
    mines_.insert(mines_.end(), spawnedMines.begin(),
                  spawnedMines.end());
    mines_.erase(
        std::remove_if(mines_.begin(), mines_.end(),
                       [](const MineRuntime& mine) {
                           return !mine.active ||
                                  (mine.maximumLife > 0.0F &&
                                   mine.seconds > mine.maximumLife);
                       }),
        mines_.end());

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
            PickSlot pickSlot = PickSlot::None;
            switch (bonus.kind)
            {
            case BonusKind::Money:
                runtime.pickedMoney +=
                    static_cast<std::uint32_t>(std::max(bonus.value, 0.0F));
                break;
            case BonusKind::Medpack:
                runtime.life = runtime.maximumLife;
                break;
            case BonusKind::Ammunition:
            {
                struct RechargeTarget
                {
                    std::uint32_t* current = nullptr;
                    std::uint32_t capacity = 0;
                    std::size_t weapon =
                        RacerRuntime::invalidWeapon;
                    PickSlot pickSlot = PickSlot::None;
                };
                std::vector<RechargeTarget> targets;
                for (std::size_t slot = 0;
                     slot < runtime.weaponSlots.size(); ++slot)
                {
                    const auto weapon = runtime.weaponSlots[slot];
                    if (weapon == RacerRuntime::invalidWeapon ||
                        weapon >= race_.weapons.size() ||
                        runtime.weaponCharges[slot] >=
                            runtime.weaponCapacity[slot])
                        continue;
                    targets.push_back(
                        {&runtime.weaponCharges[slot],
                         runtime.weaponCapacity[slot], weapon,
                         PickSlot::Primary});
                }
                if (runtime.hyperWeapon !=
                        RacerRuntime::invalidWeapon &&
                    runtime.hyperWeapon < race_.weapons.size() &&
                    runtime.hyperCharge < runtime.hyperCapacity)
                {
                    targets.push_back(
                        {&runtime.hyperCharge, runtime.hyperCapacity,
                         runtime.hyperWeapon, PickSlot::Hyper});
                }
                if (runtime.mineWeapon !=
                        RacerRuntime::invalidWeapon &&
                    runtime.mineWeapon < race_.weapons.size() &&
                    runtime.mines < runtime.mineCapacity)
                {
                    targets.push_back(
                        {&runtime.mines, runtime.mineCapacity,
                         runtime.mineWeapon, PickSlot::Mine});
                }
                if (!targets.empty())
                {
                    auto& target = targets[
                        (bonusIndex + racer) % targets.size()];
                    const auto maximumCharge =
                        race_.weapons[target.weapon].maximumCharge;
                    const auto amount = std::max(
                        1U, static_cast<std::uint32_t>(std::ceil(
                                static_cast<float>(maximumCharge) *
                                bonus.value)));
                    *target.current = std::min(
                        *target.current + amount, target.capacity);
                    pickSlot = target.pickSlot;
                }
                syncSelectedWeapon(runtime);
                break;
            }
            case BonusKind::Mine:
                runtime.mines =
                    runtime.mineCapacity > 0
                        ? std::min(runtime.mines + 1U,
                                   runtime.mineCapacity)
                        : runtime.mines + 1U;
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
                               bonus.transform.position, bonus.value,
                               pickSlot});
            break;
        }
    }

    auto fireWeapon = [&](std::size_t shooter) {
        if (shooter >= vehicles.size() ||
            shooter >= racers_.size() || racers_[shooter].finished)
            return;
        auto& runtime = racers_[shooter];
        syncSelectedWeapon(runtime);
        if (runtime.selectedWeapon == RacerRuntime::invalidWeapon ||
            runtime.selectedWeapon >= race_.weapons.size() ||
            runtime.selectedWeaponSlot >= runtime.weaponCharges.size() ||
            weaponCooldown_[shooter][runtime.selectedWeaponSlot] > 0.0F ||
            runtime.weaponCharges[runtime.selectedWeaponSlot] == 0)
            return;
        const std::size_t firedSlot = runtime.selectedWeaponSlot;
        const std::size_t firedWeapon = runtime.selectedWeapon;
        const auto* weapon =
            &race_.weapons[firedWeapon];
        if (weapon->slot == WeaponSlot::Support)
            return;
        --runtime.weaponCharges[firedSlot];
        syncSelectedWeapon(runtime);
        weaponCooldown_[shooter][firedSlot] =
            std::max(weapon->shotDelay, 0.03F);
        const Vec3 eventOrigin = vehicles[shooter].body.position;
        std::size_t target = racers_.size();
        for (std::size_t projectileIndex = 0;
             projectileIndex < weapon->projectiles.size();
             ++projectileIndex)
        {
            const auto& projectile =
                weapon->projectiles[projectileIndex];
            const auto shotTransform = projectileWorldTransform(
                shooter, firedWeapon, firedSlot, projectile);
            const Vec3 projectileOrigin = shotTransform.position;
            Vec3 direction = normalized3(
                rotate(shotTransform.rotation,
                       {1.0F, 0.0F, 0.0F}));
            if (std::abs(direction.z) < 0.707F)
                direction = normalized2(direction);
            const float forwardVehicleSpeed = std::max(
                dot3(direction, vehicles[shooter].linearVelocity),
                0.0F);
            auto findHomingTarget = [&]() {
                std::size_t result = RacerRuntime::invalidWeapon;
                float nearest = std::numeric_limits<float>::max();
                constexpr float minimumDirectionDot = 0.84125353F;
                for (std::size_t candidate = 0;
                     candidate < vehicles.size() &&
                     candidate < racers_.size();
                     ++candidate)
                {
                    if (candidate == shooter ||
                        racers_[candidate].finished)
                        continue;
                    const Vec3 difference = subtract(
                        vehicles[candidate].body.position,
                        projectileOrigin);
                    const float candidateDistance =
                        length3(difference);
                    if (candidateDistance <= 0.001F ||
                        candidateDistance >= nearest)
                        continue;
                    if (dot3(direction, normalized3(difference)) <
                        minimumDirectionDot)
                        continue;
                    result = candidate;
                    nearest = candidateDistance;
                }
                return result;
            };
            const std::size_t homingTarget = findHomingTarget();
            const bool rayProjectile =
                projectile.speed <= 0.0F;
            const bool attachedProjectile =
                projectile.type == 3U ||
                projectile.type == 14U ||
                projectile.type == 15U ||
                projectile.type == 18U;
            const float projectileDistance =
                projectile.maximumDistance > 0.0F
                    ? projectile.maximumDistance
                    : 100.0F;
            float targetDistance = projectileDistance;
            std::size_t projectileTarget = racers_.size();
            if (rayProjectile && !attachedProjectile)
            {
                for (std::size_t candidate = 0;
                     candidate < vehicles.size() &&
                     candidate < racers_.size();
                     ++candidate)
                {
                    if (candidate == shooter ||
                        racers_[candidate].finished)
                        continue;
                    const auto difference = subtract(
                        vehicles[candidate].body.position,
                        projectileOrigin);
                    const float distance = length3(difference);
                    if (distance >= targetDistance ||
                        distance <= 0.001F)
                        continue;
                    const auto& racerDefinition =
                        race_.racers[candidate];
                    const auto& vehicleDefinition =
                        racerDefinition.hasConfiguredVehicle
                            ? racerDefinition.configuredVehicle
                            : race_.vehicles.at(
                                  racerDefinition.vehicle);
                    const float radius = std::max(
                        {vehicleDefinition.physics.halfExtents.x,
                         vehicleDefinition.physics.halfExtents.y,
                         0.5F});
                    const float forwardDistance =
                        dot3(direction, difference);
                    const float lateralDistance = length3(subtract(
                        difference,
                        multiply(direction, forwardDistance)));
                    if (forwardDistance <= 0.0F ||
                        lateralDistance >= radius ||
                        forwardDistance >= projectileDistance)
                        continue;
                    projectileTarget = candidate;
                    targetDistance = distance;
                }
            }
            Vec3 end = add(
                projectileOrigin,
                multiply(direction, projectileDistance));
            if (attachedProjectile)
            {
                ProjectileRuntime runtimeProjectile;
                runtimeProjectile.owner = shooter;
                runtimeProjectile.weapon = firedWeapon;
                runtimeProjectile.projectile = projectileIndex;
                runtimeProjectile.mountSlot =
                    runtime.selectedWeaponSlot;
                runtimeProjectile.position = projectileOrigin;
                runtimeProjectile.direction = direction;
                runtimeProjectile.maximumDistance =
                    projectileDistance;
                runtimeProjectile.damage = projectile.damage;
                runtimeProjectile.lifeSeconds = std::max(
                    projectile.minimumLife,
                    std::max(weapon->shotDelay, 0.1F));
                runtimeProjectile.attached = true;
                projectiles_.push_back(runtimeProjectile);
            }
            else if (!rayProjectile)
            {
                float speed = projectile.speed;
                if (projectile.relativeSpeed)
                {
                    speed += forwardVehicleSpeed;
                }
                else if (projectile.relativeSpeedMinimum > 0.0F)
                {
                    speed = std::max(
                        speed,
                        projectile.relativeSpeedMinimum +
                            forwardVehicleSpeed);
                }
                ProjectileRuntime runtimeProjectile;
                runtimeProjectile.owner = shooter;
                runtimeProjectile.weapon = firedWeapon;
                runtimeProjectile.projectile = projectileIndex;
                runtimeProjectile.mountSlot =
                    runtime.selectedWeaponSlot;
                runtimeProjectile.position = projectileOrigin;
                runtimeProjectile.direction = direction;
                runtimeProjectile.speed = speed;
                runtimeProjectile.velocity =
                    multiply(direction, speed);
                runtimeProjectile.maximumDistance =
                    projectileDistance;
                runtimeProjectile.damage = projectile.damage;
                runtimeProjectile.angularSpeed =
                    projectile.angularSpeed;
                runtimeProjectile.ballistic =
                    projectile.type == 19U;
                if (projectile.type == 2U ||
                    projectile.type == 21U)
                {
                    runtimeProjectile.homingDelay = 0.4F;
                    runtimeProjectile.target = homingTarget;
                }
                projectiles_.push_back(runtimeProjectile);
                end = add(
                    projectileOrigin,
                    multiply(
                        direction,
                        std::min(std::max(speed * 0.03F, 0.5F),
                                 projectileDistance)));
            }
            else if (projectileTarget < racers_.size())
            {
                target = projectileTarget;
                end = vehicles[target].body.position;
                const float damage =
                    racers_[target].shieldSeconds > 0.0F
                        ? 0.0F
                        : damageAfterSupport(
                              target,
                              std::max(projectile.damage, 0.0F),
                              false);
                racers_[target].life =
                    std::max(0.0F, racers_[target].life - damage);
                events_.push_back(
                    {RaceEventKind::Damage, target, shooter, end,
                     damage});
                if (racers_[target].life <= 0.0F)
                {
                    events_.push_back(
                        {RaceEventKind::Kill, shooter, target, end,
                         0.0F});
                    racers_[target].life =
                        racers_[target].maximumLife;
                    queueRespawn(target, vehicles[target]);
                }
            }
            effects_.push_back(
                {RaceEventKind::WeaponFired, projectileOrigin, end,
                 (rayProjectile || attachedProjectile) ? 0.12F : 0.03F,
                 (rayProjectile || attachedProjectile) ? 0.12F : 0.03F,
                 firedWeapon, projectileIndex});
        }
        events_.push_back({RaceEventKind::WeaponFired, shooter, target,
                           eventOrigin, 5.0F,
                           PickSlot::None, firedWeapon});
    };
    if (humanControl.useWeapon)
        fireWeapon(0);
    if (humanControl.fireWeaponSlot >= 0 && !racers_.empty() &&
        humanControl.fireWeaponSlot <
            static_cast<int>(PlayerProfile::weaponSlotCount))
    {
        auto& runtime = racers_.front();
        const auto requested =
            static_cast<std::size_t>(humanControl.fireWeaponSlot);
        if (runtime.weaponSlots[requested] !=
            RacerRuntime::invalidWeapon)
        {
            const auto selected = runtime.selectedWeaponSlot;
            runtime.selectedWeaponSlot = requested;
            syncSelectedWeapon(runtime);
            fireWeapon(0);
            runtime.selectedWeaponSlot = selected;
            syncSelectedWeapon(runtime);
        }
    }
    if (humanControl.useAllWeapons && !racers_.empty())
    {
        auto& runtime = racers_.front();
        const auto selected = runtime.selectedWeaponSlot;
        for (std::size_t slot = 0;
             slot < runtime.weaponSlots.size(); ++slot)
        {
            if (runtime.weaponSlots[slot] ==
                    RacerRuntime::invalidWeapon ||
                runtime.weaponCharges[slot] == 0U)
                continue;
            runtime.selectedWeaponSlot = slot;
            syncSelectedWeapon(runtime);
            fireWeapon(0);
        }
        runtime.selectedWeaponSlot = selected;
        syncSelectedWeapon(runtime);
    }
    auto raceProgress = [&](const RacerRuntime& runtime) {
        const float pathLength = static_cast<float>(
            std::max<std::size_t>(race_.tracePath.size() - 1U, 1U));
        const float total =
            pathLength * static_cast<float>(std::max(race_.lapCount, 1U));
        const float current =
            static_cast<float>(runtime.completedLaps) * pathLength +
            static_cast<float>(runtime.nextPathNode);
        return std::clamp(current / total, 0.0F, 1.0F);
    };
    for (std::size_t racer = 1;
         racer < racers_.size() && racer < vehicles.size(); ++racer)
    {
        auto& runtime = racers_[racer];
        const Vec3 carDirection =
            normalized2(forward(vehicles[racer].body.rotation));
        std::size_t frontTarget = racers_.size();
        std::size_t backTarget = racers_.size();
        float frontDistance = 100.0F;
        float backDistance = 100.0F;
        for (std::size_t target = 0;
             target < vehicles.size() && target < racers_.size();
             ++target)
        {
            if (target == racer || racers_[target].finished)
                continue;
            const auto difference = subtract(
                vehicles[target].body.position,
                vehicles[racer].body.position);
            const float distance = length2(difference);
            if (distance <= 0.001F || distance >= 100.0F)
                continue;
            const float alignment =
                dot2(carDirection, normalized2(difference));
            const auto& targetDefinition = race_.racers[target];
            const auto& targetVehicle =
                targetDefinition.hasConfiguredVehicle
                    ? targetDefinition.configuredVehicle
                    : race_.vehicles.at(targetDefinition.vehicle);
            const float radius = std::max(
                {targetVehicle.physics.halfExtents.x,
                 targetVehicle.physics.halfExtents.y, 0.5F});
            const float lineDistance = std::abs(
                carDirection.x * difference.y -
                carDirection.y * difference.x);
            if (lineDistance >= radius ||
                std::abs(difference.z) >= radius)
                continue;
            if (alignment > 0.70710678F && distance < frontDistance)
            {
                frontTarget = target;
                frontDistance = distance;
            }
            else if (alignment < -0.70710678F &&
                     distance < backDistance)
            {
                backTarget = target;
                backDistance = distance;
            }
        }

        if (frontTarget < racers_.size())
        {
            std::vector<std::size_t> usableSlots;
            std::size_t chargedWeapons = 0;
            for (std::size_t slot = 0;
                 slot < runtime.weaponSlots.size(); ++slot)
            {
                const auto weaponIndex = runtime.weaponSlots[slot];
                if (weaponIndex == RacerRuntime::invalidWeapon ||
                    weaponIndex >= race_.weapons.size())
                    continue;
                const auto& weapon = race_.weapons[weaponIndex];
                if (runtime.weaponCharges[slot] > 0U)
                    ++chargedWeapons;
                const float range =
                    weapon.maximumDistance <= 0.0F
                        ? 100.0F
                        : std::min(weapon.maximumDistance, 100.0F);
                if (runtime.weaponCharges[slot] > 0U &&
                    weaponCooldown_[racer][slot] <= 0.0F &&
                    frontDistance < range)
                    usableSlots.push_back(slot);
            }
            std::stable_sort(
                usableSlots.begin(), usableSlots.end(),
                [&](std::size_t first, std::size_t second) {
                    const auto firstWeapon =
                        runtime.weaponSlots[first];
                    const auto secondWeapon =
                        runtime.weaponSlots[second];
                    const float firstRange =
                        race_.weapons[firstWeapon].maximumDistance <= 0.0F
                            ? 100.0F
                            : race_.weapons[firstWeapon].maximumDistance;
                    const float secondRange =
                        race_.weapons[secondWeapon].maximumDistance <= 0.0F
                            ? 100.0F
                            : race_.weapons[secondWeapon].maximumDistance;
                    return firstRange < secondRange;
                });
            if (!usableSlots.empty())
            {
                const auto slot = usableSlots.front();
                const float summedPart = std::clamp(
                    raceProgress(runtime) / 0.7F, 0.0F, 1.0F);
                const float weaponPart =
                    chargedWeapons > 0U
                        ? 1.0F / static_cast<float>(chargedWeapons)
                        : 1.0F;
                const float part =
                    std::min(summedPart / weaponPart, 1.0F);
                const float ammunition = std::max(
                    static_cast<float>(runtime.weaponCharges[slot]) -
                        (1.0F - part) *
                            static_cast<float>(
                                runtime.weaponCapacity[slot]),
                    0.0F);
                if (runtime.weaponCapacity[slot] == 0U ||
                    ammunition > 0.0F)
                {
                    runtime.selectedWeaponSlot = slot;
                    syncSelectedWeapon(runtime);
                    fireWeapon(racer);
                }
            }
        }

        if (runtime.mines > 0 &&
            mineCooldown_[racer] <= 0.0F)
        {
            float summedPart = std::clamp(
                (raceProgress(runtime) - 0.05F) / 0.9F,
                0.0F, 1.0F);
            if (backTarget < racers_.size() && backDistance < 30.0F)
                summedPart = std::clamp(summedPart + 0.3F, 0.0F, 1.0F);
            std::uint32_t maximumUsedCharge = 3U;
            if (runtime.mineWeapon < race_.weapons.size() &&
                race_.weapons[runtime.mineWeapon].record.find("maslo") !=
                    std::string::npos)
                maximumUsedCharge = 2U;
            const auto capacity =
                std::min(runtime.mineCapacity, maximumUsedCharge);
            const auto spent =
                runtime.mineCapacity > runtime.mines
                    ? runtime.mineCapacity - runtime.mines
                    : 0U;
            const auto current =
                capacity - std::min(spent, capacity);
            const float ammunition = std::max(
                static_cast<float>(current) -
                    (1.0F - summedPart) *
                        static_cast<float>(capacity),
                0.0F);
            if ((capacity == 0U || ammunition > 0.0F) &&
                vehicles[racer].speed > 5.0F)
            {
                placeMine(racer);
            }
        }
        if (runtime.hyperCharge > 0 &&
            hyperCooldown_[racer] <= 0.0F &&
            vehicles[racer].speed > 1.0F &&
            runtime.hyperWeapon < race_.weapons.size())
        {
            const auto& target =
                tracePoint(runtime.nextPathNode);
            const auto& following = tracePoint(std::min(
                runtime.nextPathNode + 1U,
                race_.tracePath.size() - 1U));
            const auto currentDirection = normalized2(
                subtract(target.position, vehicles[racer].body.position));
            const auto nextDirection =
                normalized2(subtract(following.position,
                                     target.position));
            const float turnAngle = std::acos(std::clamp(
                dot2(currentDirection, nextDirection),
                -1.0F, 1.0F));
            const float distanceToTurn = length2(
                subtract(target.position,
                         vehicles[racer].body.position));
            const float hyperDistance =
                race_.weapons[runtime.hyperWeapon].projectileSpeed +
                vehicles[racer].speed;
            const bool safeDistance =
                runtime.nextPathNode + 1U >=
                    race_.tracePath.size() ||
                turnAngle < 3.14159265358979323846F / 6.0F ||
                distanceToTurn > hyperDistance;
            const float summedPart = std::clamp(
                raceProgress(runtime) / 0.7F, 0.0F, 1.0F);
            const float ammunition = std::max(
                static_cast<float>(runtime.hyperCharge) -
                    (1.0F - summedPart) *
                        static_cast<float>(runtime.hyperCapacity),
                0.0F);
            if (safeDistance &&
                (runtime.hyperCapacity == 0U || ammunition > 0.0F))
                activateHyper(racer);
        }
    }

    const std::size_t collisionRacers =
        std::min(vehicles.size(), racers_.size());
    for (std::size_t first = 0; first < collisionRacers; ++first)
    {
        for (const auto& contact : vehicles[first].bodyContacts)
        {
            if (contact.surface !=
                    r3d::physics::CollisionSurface::Vehicle ||
                contact.otherVehicle <= first ||
                contact.otherVehicle >= collisionRacers)
                continue;
            const std::size_t second = contact.otherVehicle;
            const std::size_t cooldownIndex =
                first * collisionRacers + second;
            if (cooldownIndex >= touchCooldown_.size() ||
                touchCooldown_[cooldownIndex] > 0.0F)
                continue;
            float forcePart = 0.0F;
            const float damage = damageFromContact(
                race_.touchCarDamage, race_.touchCarDamageForce,
                contact.force, forcePart);
            if (forcePart <= 0.0F || damage <= 0.0F)
                continue;
            touchCooldown_[cooldownIndex] = 0.25F;
            auto kineticEnergy = [&](std::size_t racer) {
                const auto& source = race_.racers[racer];
                const auto& definition =
                    source.hasConfiguredVehicle
                        ? source.configuredVehicle
                        : race_.vehicles.at(source.vehicle);
                return 0.5F * definition.physics.mass *
                       vehicles[racer].speed *
                       vehicles[racer].speed;
            };
            const float firstEnergy = kineticEnergy(first);
            const float secondEnergy = kineticEnergy(second);
            const std::size_t target =
                firstEnergy > secondEnergy ? second : first;
            const std::size_t attacker =
                target == first ? second : first;
            applyTouchDamage(
                target, attacker, damage,
                vehicles[target].body.position);
        }
    }

    for (std::size_t racer = 0;
         racer < vehicles.size() && racer < racers_.size(); ++racer)
    {
        if (vehicles[racer].speed < 8.0F)
            continue;
        damageDecorationAlongSegment(
            vehicles[racer].body.position,
            vehicles[racer].body.position,
            vehicles[racer].speed * 2.0F,
            racer, 3.0F);
    }
}

void OriginalRaceSession::completeAchievement(
    std::size_t achievement)
{
    if (achievement >= race_.achievements.size() ||
        achievement >= achievementIterations_.size())
        return;
    const auto& definition = race_.achievements[achievement];
    if (++achievementIterations_[achievement] <
        definition.iterationCount)
        return;
    achievementIterations_[achievement] = 0U;
    achievementPoints_ += static_cast<std::uint32_t>(
        std::floor(static_cast<float>(definition.reward) *
                   achievementMultiplier_));
    events_.push_back(
        {RaceEventKind::Achievement, 0, achievement, {},
         static_cast<float>(definition.reward)});
}

void OriginalRaceSession::updateAchievements(float seconds)
{
    for (std::size_t index = 0;
         index < race_.achievements.size(); ++index)
    {
        if (race_.achievements[index].classId != 2U)
            continue;
        auto& timer = achievementConditionTimers_[index];
        if (timer > 0.0F && (timer -= seconds) <= 0.0F)
        {
            timer = 0.0F;
            achievementConditionCounters_[index] = 0U;
        }
    }

    const std::size_t sourceEventCount = events_.size();
    for (std::size_t eventIndex = 0;
         eventIndex < sourceEventCount; ++eventIndex)
    {
        const RaceEvent event = events_[eventIndex];
        const bool humanKill =
            event.kind == RaceEventKind::Kill && event.racer == 0U;
        const bool humanDeath =
            event.kind == RaceEventKind::Kill && event.target == 0U;
        const bool humanLap =
            event.kind == RaceEventKind::Lap && event.racer == 0U;
        const bool humanFinish =
            event.kind == RaceEventKind::Finish && event.racer == 0U;

        for (std::size_t index = 0;
             index < race_.achievements.size(); ++index)
        {
            const auto& definition = race_.achievements[index];
            auto& counter = achievementConditionCounters_[index];
            switch (definition.classId)
            {
            case 1U:
                if (event.kind == RaceEventKind::Bonus &&
                    event.racer == 0U &&
                    event.target < race_.bonuses.size() &&
                    race_.bonuses[event.target].kind ==
                        definition.bonusKind)
                {
                    const auto total = static_cast<std::uint32_t>(
                        std::count_if(
                            race_.bonuses.begin(), race_.bonuses.end(),
                            [&](const BonusInstance& bonus) {
                                return bonus.kind ==
                                       definition.bonusKind;
                            }));
                    if (total > 0U && ++counter >= total)
                    {
                        counter = 0U;
                        completeAchievement(index);
                    }
                }
                break;
            case 2U:
                if (humanKill)
                {
                    if (++counter >=
                        std::max(definition.killsNumber, 1U))
                    {
                        counter = 0U;
                        achievementConditionTimers_[index] = 0.0F;
                        completeAchievement(index);
                    }
                    else
                    {
                        achievementConditionTimers_[index] =
                            definition.killsTime;
                    }
                }
                break;
            case 3U:
                if (humanKill &&
                    ++counter >=
                        std::max(definition.killsNumber, 1U))
                {
                    counter = 0U;
                    completeAchievement(index);
                }
                break;
            case 4U:
                if (humanLap && !racers_.empty() &&
                    racers_.front().place == definition.place)
                    ++counter;
                if (humanFinish && counter >= race_.lapCount)
                {
                    counter = 0U;
                    completeAchievement(index);
                }
                break;
            case 5U:
                if ((event.kind == RaceEventKind::Damage &&
                     event.racer == 0U && event.value > 0.0F) ||
                    humanDeath)
                    ++counter;
                if (humanLap && !racers_.empty())
                {
                    if (counter == 0U &&
                        racers_.front().completedLaps == 1U)
                        completeAchievement(index);
                    counter = 0U;
                }
                break;
            case 6U:
                if (humanLap && !racers_.empty())
                {
                    const auto newPlace = racers_.front().place;
                    if (racers_.front().completedLaps >=
                            race_.lapCount &&
                        static_cast<int>(achievementPreviousLapPlace_) -
                                static_cast<int>(newPlace) >=
                            static_cast<int>(racers_.size()) - 1)
                        completeAchievement(index);
                }
                break;
            case 7U:
                if (humanDeath)
                    ++counter;
                if (humanLap && !racers_.empty() &&
                    racers_.front().completedLaps ==
                        race_.lapCount - 1U &&
                    counter == 0U)
                    completeAchievement(index);
                break;
            case 8U:
                if (humanKill && achievementGlobalKills_ == 0U)
                    completeAchievement(index);
                break;
            case 9U:
                if (humanDeath && event.racer != 0U &&
                    event.touchDamage)
                    completeAchievement(index);
                break;
            default:
                break;
            }
        }
        if (event.kind == RaceEventKind::Kill)
            ++achievementGlobalKills_;
        if (humanLap && !racers_.empty())
            achievementPreviousLapPlace_ = racers_.front().place;
    }
}

void OriginalRaceSession::update(
    float seconds,
    const std::vector<r3d::physics::VehicleState>& vehicles,
    const RaceControl& humanControl)
{
    seconds = std::clamp(seconds, 0.0F, 0.1F);
    events_.clear();
    std::fill(vehicleInputs_.begin(), vehicleInputs_.end(),
              r3d::physics::VehicleInput{});
    if (phase_ == RacePhase::Paused)
        return;
    for (auto& effect : effects_)
        effect.seconds -= seconds;
    effects_.erase(
        std::remove_if(effects_.begin(), effects_.end(),
                       [](const RaceEffect& effect) {
                           return effect.seconds <= 0.0F;
                       }),
        effects_.end());
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
        vehicleInputs_[racer] =
            aiInput(racer, vehicles[racer], seconds);

    updateGameplay(seconds, vehicles, humanControl);
    updatePlaces(vehicles);
    updateAchievements(seconds);
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

        const float lifeBeforeBorder = session.racers().front().life;
        vehicles[0].speed = 25.0F;
        vehicles[0].linearVelocity = {-25.0F, 5.0F, 0.0F};
        vehicles[0].bodyContacts = {
            {r3d::physics::CollisionSurface::TrackBorder,
             std::numeric_limits<std::size_t>::max(),
             {1.0F, 0.0F, 0.0F}, 25.0F, 4000000.0F}};
        session.update(1.0F / 60.0F, vehicles, input);
        if (session.takeVelocityRequests().empty() ||
            session.racers().front().life >= lifeBeforeBorder)
            throw std::runtime_error(
                "source spring-border contact transition failed");

        session.setSpringBorders(false);
        session.update(1.0F / 60.0F, vehicles, input);
        if (!session.takeVelocityRequests().empty())
            throw std::runtime_error(
                "disabled spring-border still changed velocity");
        session.setSpringBorders(true);
        vehicles[0].bodyContacts.clear();

        if (vehicles.size() > 1U)
        {
            vehicles[1].body.position = vehicles[0].body.position;
            vehicles[1].speed = 5.0F;
            const float firstLife = session.racers()[0].life;
            const float secondLife = session.racers()[1].life;
            session.update(1.0F / 60.0F, vehicles, input);
            if (session.racers()[0].life != firstLife ||
                session.racers()[1].life != secondLife)
                throw std::runtime_error(
                    "car proximity caused damage without a body contact");

            vehicles[0].bodyContacts = {
                {r3d::physics::CollisionSurface::Vehicle, 1U,
                 {-1.0F, 0.0F, 0.0F}, 20.0F, 1200000.0F}};
            session.update(1.0F / 60.0F, vehicles, input);
            if (session.racers()[1].life >= secondLife)
                throw std::runtime_error(
                    "source car-contact damage transition failed");
            vehicles[0].bodyContacts.clear();
        }

        vehicles[0].speed = 0.0F;
        vehicles[0].linearVelocity = {};
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
            const auto ammunition =
                session.racers().front().ammunition;
            vehicles[1].body.position =
                add(vehicles[0].body.position, {10.0F, 0.0F, 0.0F});
            session.update(1.0F / 60.0F, vehicles, input);
            if (ammunition == 0 ||
                session.racers().front().ammunition + 1U != ammunition)
                throw std::runtime_error("weapon/ammunition transition failed");
        }
        input.useWeapon = false;

        std::vector<const WeaponDefinition*> primaryWeapons;
        for (const auto& weapon : race.weapons)
        {
            if (weapon.slot == WeaponSlot::Primary)
                primaryWeapons.push_back(&weapon);
            if (primaryWeapons.size() == 2U)
                break;
        }
        if (primaryWeapons.size() == 2U)
        {
            OriginalRaceSession weaponSession(race);
            PlayerProfile weaponProfile;
            for (std::size_t slot = 0; slot < 2U; ++slot)
            {
                auto& profileSlot = weaponProfile.slots[
                    PlayerProfile::firstWeaponSlot + slot];
                profileSlot.record = primaryWeapons[slot]->record;
                profileSlot.charge = std::max(
                    primaryWeapons[slot]->reloadCharge, 2U);
                profileSlot.hasCharge = true;
            }
            weaponSession.applyPlayerProfile(weaponProfile);
            RaceControl weaponInput;
            for (int frame = 0; frame < 190; ++frame)
                weaponSession.update(
                    1.0F / 60.0F, vehicles, weaponInput);
            weaponInput.changeWeapon = true;
            weaponInput.weaponChange = 1;
            weaponSession.update(
                1.0F / 60.0F, vehicles, weaponInput);
            if (weaponSession.racers().front().selectedWeaponSlot != 1U)
                throw std::runtime_error(
                    "source next-weapon transition failed");
            weaponInput.weaponChange = -1;
            weaponSession.update(
                1.0F / 60.0F, vehicles, weaponInput);
            if (weaponSession.racers().front().selectedWeaponSlot != 0U)
                throw std::runtime_error(
                    "source previous-weapon transition failed");
            weaponInput = {};
            const auto beforeAll =
                weaponSession.racers().front().weaponCharges;
            weaponInput.useAllWeapons = true;
            weaponSession.update(
                1.0F / 60.0F, vehicles, weaponInput);
            const auto afterAll =
                weaponSession.racers().front().weaponCharges;
            if (afterAll[0] + 1U != beforeAll[0] ||
                afterAll[1] + 1U != beforeAll[1])
            {
                throw std::runtime_error(
                    "source ShotAll/per-weapon cooldown transition failed");
            }
            weaponInput = {};
            for (int frame = 0; frame < 60; ++frame)
                weaponSession.update(
                    1.0F / 60.0F, vehicles, weaponInput);
            const auto beforeDirect =
                weaponSession.racers().front().weaponCharges;
            weaponInput.fireWeaponSlot = 1;
            weaponSession.update(
                1.0F / 60.0F, vehicles, weaponInput);
            const auto afterDirect =
                weaponSession.racers().front().weaponCharges;
            if (afterDirect[0] != beforeDirect[0] ||
                afterDirect[1] + 1U != beforeDirect[1] ||
                weaponSession.racers().front().selectedWeaponSlot != 0U)
            {
                throw std::runtime_error(
                    "source Shot1..4 direct-slot transition failed");
            }
        }

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
