#include "OriginalDataBase.h"

#include "OriginalGameObject.h"
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

void DataBase::Configure(const Race& race)
{
    Clear();

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
