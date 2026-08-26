#include "OriginalGameObject.h"
#include "OriginalMap.h"

#include <iostream>
#include <stdexcept>

namespace source = r3d::game::originalrace::source;

int main()
{
    source::Map map;
    auto& semaphore = map.AddMapObj(
        source::MapObjCategory::Decoration,
        source::GameObjType::DestrObj,
        "world\\db\\root\\ctDecoration\\Misc\\semaphore",
        7U, 2U);
    semaphore.GetGameObj().ResetGameObject(10.0F);
    auto& track = map.AddMapObj(
        source::MapObjCategory::Track,
        source::GameObjType::GameObj,
        "world\\db\\root\\ctTrack\\track1", 9U, 4U);
    track.GetGameObj().ResetGameObject(-1.0F);

    if (map.GetMapObj(source::Map::defaultMapObjId) != nullptr ||
        map.GetMapObj(7U) != &semaphore ||
        map.GetMapObj(9U) != &track ||
        map.GetSemaphore() != &semaphore ||
        map.GetLastId() != 9U || map.GetObjects().size() != 2U ||
        semaphore.GetSourceIndex() != 2U ||
        map.GetMapObjCount(
            "world\\db\\root\\ctTrack\\track1",
            source::MapObjCategory::Track) != 1U)
        return 1;

    auto& bonus = map.AddMapObj(
        source::MapObjCategory::Bonus,
        source::GameObjType::Proj,
        "world\\db\\root\\ctBonus\\money", 5U);
    if (bonus.GetId() != 10U || map.GetMapObj(10U) != &bonus ||
        bonus.GetSourceIndex() != 5U)
        return 2;

    bool duplicateRejected = false;
    try
    {
        map.AddMapObj(
            source::MapObjCategory::Effects,
            source::GameObjType::GameObj, "Effect\\smoke", 9U, 0U);
    }
    catch (const std::invalid_argument&)
    {
        duplicateRejected = true;
    }
    if (!duplicateRejected || map.GetObjects().size() != 3U)
        return 3;

    // Map::GetMapObj hides lsDeath by default, while includeDead preserves
    // access until MapObjects finishes the progress callback and removal.
    semaphore.GetGameObj().Death(
        r3d::game::originalrace::DamageType::Simple);
    if (map.GetMapObj(7U) != nullptr ||
        map.GetMapObj(7U, true) != &semaphore)
        return 4;
    const auto special = map.GetMapObjList(
        source::MapObjCategory::Decoration).OnProgressSpecial(0.0F);
    if (special.progressed != 1U || special.removed != 1U ||
        map.GetMapObj(7U, true) != nullptr ||
        map.GetObjects().size() != 2U || map.GetSemaphore() != nullptr)
        return 5;

    if (!map.DelMapObj(&track) || map.DelMapObj(&track) ||
        map.GetMapObj(9U, true) != nullptr ||
        map.GetObjects().size() != 1U)
        return 6;

    map.Clear();
    if (!map.GetObjects().empty() || map.GetLastId() != 0U)
        return 7;
    auto& first = map.AddMapObj(
        source::MapObjCategory::Effects,
        source::GameObjType::GameObj, "Effect\\smoke", 0U);
    if (first.GetId() != 1U || map.GetMapObj(1U) != &first)
        return 8;

    map.Clear();
    map.ReserveIdsThrough(293U);
    auto& firstCar = map.AddMapObj(
        source::MapObjCategory::Car,
        source::GameObjType::RockCar, "Car\\marauder", 0U);
    if (firstCar.GetId() != 294U)
        return 9;

    std::cout << "original Map category registry and global ID rules "
                 "passed\n";
    return 0;
}
