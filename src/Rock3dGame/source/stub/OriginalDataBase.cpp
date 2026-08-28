#include "OriginalDataBase.h"

#include "OriginalGameObject.h"
#include "OriginalLogic.h"
#include "OriginalRace.h"
#include "OriginalWeapon.h"

#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace r3d::game::originalrace::source
{
namespace
{

void applyProxyTransform(
    GameObject& object, const Transform& transform) noexcept
{
    object.SetPos({
        transform.position.x, transform.position.y,
        transform.position.z});
    object.SetScale({
        transform.scale.x, transform.scale.y,
        transform.scale.z});
    object.SetRot({
        transform.rotation.x, transform.rotation.y,
        transform.rotation.z, transform.rotation.w});
}

bool belongsToCategory(
    std::string_view record, std::string_view category) noexcept
{
    const auto categoryPosition = record.find(category);
    if (categoryPosition == std::string_view::npos)
        return false;
    const auto end = categoryPosition + category.size();
    return (categoryPosition == 0U ||
            record[categoryPosition - 1U] == '\\' ||
            record[categoryPosition - 1U] == '/') &&
           (end == record.size() || record[end] == '\\' ||
            record[end] == '/');
}

void loadObjectDefinition(
    MapObj& mapObject, const ObjectDefinition& definition)
{
    auto& object = mapObject.GetGameObj();
    object.ResetGameObject(
        definition.maximumLife >= 0.0F
            ? definition.maximumLife
            : -1.0F);
    object.SetMaxTimeLife(definition.maximumTimeLife);
}

} // namespace

DataBase::DataBase()
{
    for (std::size_t index = 0U;
         index < recordLibraries_.size(); ++index)
    {
        recordLibraries_[index].SetCategory(
            static_cast<MapObjCategory>(index));
    }
}

std::size_t DataBase::CategoryIndex(MapObjCategory value) noexcept
{
    const auto index = static_cast<std::size_t>(value);
    return index < 7U ? index : 0U;
}

void DataBase::Clear() noexcept
{
    for (auto& library : recordLibraries_)
        library.Clear();
}

void DataBase::Configure(const Race& race, Logic& logic)
{
    Clear();

    // LoadEffects/LoadWeapons populate the source record libraries before
    // DataBase::Init installs PairPxContactEffect. Race contains the active
    // parsed subset of those records; keep their concrete source type here
    // rather than letting the session construct anonymous gameplay objects.
    auto& effectLibrary = GetMapObjLib(MapObjCategory::Effects);
    std::unordered_set<std::string> projectileEffects;
    for (const auto& weapon : race.weapons)
    {
        for (const auto& projectile : weapon.projectiles)
        {
            if (projectile.visual.record.empty() ||
                !belongsToCategory(
                    projectile.visual.record, "ctEffects") ||
                !projectileEffects.insert(
                    projectile.visual.record).second)
            {
                continue;
            }
            const auto* sourceProjectile = &projectile;
            effectLibrary.DefineRecord(
                projectile.visual.record, GameObjType::Proj,
                [sourceProjectile](MapObj& mapObject) {
                    auto* object = mapObject.GetAutoProj();
                    if (object == nullptr)
                    {
                        throw std::runtime_error(
                            "DataBase projectile effect record created "
                            "wrong type");
                    }
                    object->Reset(*sourceProjectile);
                });
        }
    }

    std::unordered_set<std::string> registeredEffects;
    const auto defineEffect = [&](const ObjectDefinition& definition) {
        if (definition.record.empty() ||
            !belongsToCategory(definition.record, "ctEffects") ||
            projectileEffects.contains(definition.record) ||
            !registeredEffects.insert(definition.record).second)
        {
            return;
        }
        const auto* sourceDefinition = &definition;
        effectLibrary.DefineRecord(
            definition.record, GameObjType::GameObj,
            [sourceDefinition](MapObj& mapObject) {
                loadObjectDefinition(mapObject, *sourceDefinition);
            });
    };
    defineEffect(race.rainEffect);
    defineEffect(race.wheelTrailEffect);
    defineEffect(race.wheelSmokeEffect);
    defineEffect(race.contactEffect);
    const auto defineVehicleEffects = [&](const Vehicle& vehicle) {
        defineEffect(vehicle.lowLifeEffect);
        defineEffect(vehicle.energyDamageEffect);
        defineEffect(vehicle.shieldEffect);
        for (const auto& effect : vehicle.deathEffects)
            defineEffect(effect.visual);
    };
    for (const auto& vehicle : race.vehicles)
        defineVehicleEffects(vehicle);
    for (const auto& racer : race.racers)
    {
        if (racer.hasConfiguredVehicle)
            defineVehicleEffects(racer.configuredVehicle);
    }
    defineVehicleEffects(race.vehicle);
    for (const auto& weapon : race.weapons)
    {
        defineEffect(weapon.shotEffect.visual);
        for (const auto& projectile : weapon.projectiles)
        {
            defineEffect(projectile.secondaryVisual);
            defineEffect(projectile.tertiaryVisual);
            defineEffect(projectile.deathEffect.visual);
        }
    }
    for (const auto& bonus : race.bonuses)
    {
        defineEffect(bonus.visual);
        defineEffect(bonus.deathEffect.visual);
    }

    auto& decorationLibrary =
        GetMapObjLib(MapObjCategory::Decoration);
    for (const auto& definition : race.decorationDefinitions)
    {
        const auto* sourceDefinition = &definition;
        decorationLibrary.DefineRecord(
            definition.record, GameObjType::DestrObj,
            [sourceDefinition](MapObj& mapObject) {
                auto* object = mapObject.GetDestrObj();
                if (object == nullptr)
                    throw std::runtime_error(
                        "DataBase decoration record created wrong type");
                object->ResetGameObject(
                    sourceDefinition->maximumLife >= 0.0F
                        ? sourceDefinition->maximumLife
                        : -1.0F);
                object->SetMaxTimeLife(
                    sourceDefinition->maximumTimeLife);
                object->GetDestrList().Reserve(
                    sourceDefinition->destructionPieces.size());
                for (const auto& piece :
                     sourceDefinition->destructionPieces)
                {
                    auto& fragment = object->GetDestrList().Add(
                        GameObjType::GameObj, "obj");
                    applyProxyTransform(
                        fragment.GetGameObj(), piece.transform);
                    fragment.GetGameObj().ResetGameObject(-1.0F);
                }
            });
    }

    auto& trackLibrary = GetMapObjLib(MapObjCategory::Track);
    for (const auto& definition : race.trackDefinitions)
    {
        const auto* sourceDefinition = &definition;
        trackLibrary.DefineRecord(
            definition.record, GameObjType::GameObj,
            [sourceDefinition](MapObj& mapObject) {
                auto& object = mapObject.GetGameObj();
                object.ResetGameObject(
                    sourceDefinition->maximumLife >= 0.0F
                        ? sourceDefinition->maximumLife
                        : -1.0F);
                object.SetMaxTimeLife(
                    sourceDefinition->maximumTimeLife);
            });
    }

    auto& bonusLibrary = GetMapObjLib(MapObjCategory::Bonus);
    std::unordered_set<std::string> registeredBonuses;
    for (const auto& bonus : race.bonuses)
    {
        if (!registeredBonuses.insert(bonus.record).second)
            continue;
        const auto* sourceBonus = &bonus;
        bonusLibrary.DefineRecord(
            bonus.record, GameObjType::Proj,
            [sourceBonus](MapObj& mapObject) {
                auto* projectile = mapObject.GetAutoProj();
                if (projectile == nullptr)
                    throw std::runtime_error(
                        "DataBase bonus record created wrong type");
                mapObject.GetGameObj().ResetGameObject(-1.0F);
                ProjectileDefinition description;
                description.type = sourceBonus->projectileType;
                description.visual = sourceBonus->visual;
                description.deathEffect = sourceBonus->deathEffect;
                description.size = sourceBonus->size;
                description.offset = sourceBonus->offset;
                description.collision = sourceBonus->collision;
                description.modelBounds = sourceBonus->modelBounds;
                description.modelBoundsValid =
                    sourceBonus->modelBoundsValid;
                description.speed = sourceBonus->speed;
                description.damage = sourceBonus->value;
                description.modelSize = sourceBonus->modelSize;
                projectile->Reset(description);
            });
    }

    auto& carLibrary = GetMapObjLib(MapObjCategory::Car);
    for (const auto& vehicle : race.vehicles)
    {
        const auto* sourceVehicle = &vehicle;
        carLibrary.DefineRecord(
            vehicle.record, GameObjType::RockCar,
            [sourceVehicle](MapObj& mapObject) {
                mapObject.GetGameObj().ResetGameObject(
                    sourceVehicle->maximumLife);
            });
    }
    for (const auto& racer : race.racers)
    {
        if (!racer.hasConfiguredVehicle)
            continue;
        const auto* sourceVehicle = &racer.configuredVehicle;
        carLibrary.DefineRecord(
            sourceVehicle->record, GameObjType::RockCar,
            [sourceVehicle](MapObj& mapObject) {
                mapObject.GetGameObj().ResetGameObject(
                    sourceVehicle->maximumLife);
            });
    }

    auto& weaponLibrary = GetMapObjLib(MapObjCategory::Weapon);
    for (const auto& definition : race.weapons)
    {
        const auto* sourceDefinition = &definition;
        weaponLibrary.DefineRecord(
            definition.record, GameObjType::Weapon,
            [sourceDefinition](MapObj& mapObject) {
                auto* weapon = mapObject.GetWeapon();
                if (weapon == nullptr)
                    throw std::runtime_error(
                        "DataBase weapon record created wrong type");
                weapon->Reset();
                weapon->SetDesc(
                    sourceDefinition->shotDelay,
                    sourceDefinition->projectiles);
                weapon->ConfigureShotEffect(
                    sourceDefinition->shotEffect);
            });
    }

    auto& contactBehavior =
        logic.GetBehaviors().AddPairPxContactEffect();
    const auto* contactRecord = GetRecord(
        MapObjCategory::Effects, race.contactEffect.record, false);
    contactBehavior.Configure(
        contactRecord != nullptr ? contactRecord->GetPath()
                                 : race.contactEffect.record,
        race.contactSoundPaths);
}

MapObjRecordLibrary& DataBase::GetMapObjLib(
    MapObjCategory category) noexcept
{
    return recordLibraries_[CategoryIndex(category)];
}

const MapObjRecordLibrary& DataBase::GetMapObjLib(
    MapObjCategory category) const noexcept
{
    return recordLibraries_[CategoryIndex(category)];
}

MapObjRecord* DataBase::GetRecord(
    MapObjCategory category, std::string_view name, bool assertFind)
{
    return const_cast<MapObjRecord*>(
        static_cast<const DataBase*>(this)->GetRecord(
            category, name, assertFind));
}

const MapObjRecord* DataBase::GetRecord(
    MapObjCategory category, std::string_view name,
    bool assertFind) const
{
    const auto* record = GetMapObjLib(category).FindRecord(name);
    if (assertFind && record == nullptr)
    {
        throw std::runtime_error(
            std::string("MapObj record ") + std::string(name) +
            " does not exist");
    }
    return record;
}

std::size_t DataBase::GetRecordCount() const noexcept
{
    return std::accumulate(
        recordLibraries_.begin(), recordLibraries_.end(),
        std::size_t{0U},
        [](std::size_t total, const auto& library) {
            return total + library.GetRecordCount();
        });
}

} // namespace r3d::game::originalrace::source
