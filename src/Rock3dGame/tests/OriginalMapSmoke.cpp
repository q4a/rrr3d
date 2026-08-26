#include "OriginalGameObject.h"
#include "OriginalLogic.h"
#include "OriginalMap.h"

#include <iostream>
#include <stdexcept>

namespace source = r3d::game::originalrace::source;

int main()
{
    source::Logic logic;
    source::Map map(&logic);
    if (map.GetTrace().GetTrackCount() != 4U ||
        map.GetLogic() != &logic ||
        map.GetGround().GetId() != source::Map::defaultMapObjId ||
        map.GetGround().GetOwner() != nullptr ||
        map.GetGround().GetGameObj().GetLogic() != nullptr ||
        !map.GetObjects().empty())
        return 1;

    auto* tracePoint = map.GetTrace().AddPoint(17U);
    auto* tracePath = map.GetTrace().AddPath();
    tracePath->Add(tracePoint);

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
        semaphore.GetGameObj().GetLogic() != &logic ||
        semaphore.GetRecordProxy() == nullptr ||
        semaphore.GetRecordProxy()->GetCategory() !=
            source::MapObjCategory::Decoration ||
        semaphore.GetRecordProxy()->GetType() !=
            source::GameObjType::DestrObj ||
        semaphore.GetRecordProxy()->GetName() != "semaphore" ||
        semaphore.GetName() != "semaphore0" ||
        track.GetRecordProxy()->GetName() != "track1" ||
        track.GetName() != "track10" ||
        map.GetRecordLib(source::MapObjCategory::Decoration)
                .FindRecord(semaphore.GetRecord()) !=
            semaphore.GetRecordProxy() ||
        map.GetMapObjCount(
            "world\\db\\root\\ctTrack\\track1",
            source::MapObjCategory::Track) != 1U)
        return 2;
    auto& decorationRecords =
        map.GetRecordLib(source::MapObjCategory::Decoration);
    const auto& decorationRoot = decorationRecords.GetRootNode();
    const auto* miscNode = decorationRoot.FindNode("Misc");
    if (decorationRoot.GetName() != "ctDecoration" ||
        decorationRoot.GetParent() != nullptr ||
        decorationRoot.GetLibrary() != &decorationRecords ||
        decorationRoot.GetNodeCount() != 1U || miscNode == nullptr ||
        miscNode->GetParent() != &decorationRoot ||
        miscNode->GetLibrary() != &decorationRecords ||
        miscNode->GetRecordCount() != 1U ||
        miscNode->FindRecord("semaphore") != semaphore.GetRecordProxy() ||
        semaphore.GetRecordProxy()->GetParentNode() != miscNode ||
        semaphore.GetRecordProxy()->GetLibrary() != &decorationRecords ||
        decorationRecords.FindRecord("Misc\\semaphore") !=
            semaphore.GetRecordProxy() ||
        decorationRecords.FindRecord("semaphore") != nullptr)
        return 15;
    auto& relativeSemaphore = decorationRecords.GetOrCreateRecord(
        "Misc\\semaphore", source::GameObjType::DestrObj);
    if (&relativeSemaphore != semaphore.GetRecordProxy() ||
        decorationRecords.GetRecordCount() != 1U)
        return 17;
    auto& sharedTrackRecord =
        map.GetRecordLib(source::MapObjCategory::Track)
            .GetOrCreateRecord(
                track.GetRecord(), source::GameObjType::GameObj);
    if (&sharedTrackRecord != track.GetRecordProxy() ||
        map.GetRecordLib(source::MapObjCategory::Track)
                .GetRecordCount() != 1U)
        return 13;
    bool recordTypeMismatchRejected = false;
    try
    {
        map.GetRecordLib(source::MapObjCategory::Track)
            .GetOrCreateRecord(
                track.GetRecord(), source::GameObjType::Proj);
    }
    catch (const std::invalid_argument&)
    {
        recordTypeMismatchRejected = true;
    }
    if (!recordTypeMismatchRejected)
        return 14;

    source::MapObjRecordLibrary collisionLibrary(
        source::MapObjCategory::Decoration);
    collisionLibrary.GetOrCreateRecord(
        "world\\db\\root\\ctDecoration\\Misc\\sign",
        source::GameObjType::GameObj);
    bool recordOverNodeRejected = false;
    bool nodeOverRecordRejected = false;
    try
    {
        collisionLibrary.GetOrCreateRecord(
            "world\\db\\root\\ctDecoration\\Misc",
            source::GameObjType::GameObj);
    }
    catch (const std::invalid_argument&)
    {
        recordOverNodeRejected = true;
    }
    collisionLibrary.GetOrCreateRecord(
        "world\\db\\root\\ctDecoration\\single",
        source::GameObjType::GameObj);
    try
    {
        collisionLibrary.GetOrCreateRecord(
            "world\\db\\root\\ctDecoration\\single\\child",
            source::GameObjType::GameObj);
    }
    catch (const std::invalid_argument&)
    {
        nodeOverRecordRejected = true;
    }
    if (!recordOverNodeRejected || !nodeOverRecordRejected)
        return 16;
    track.SetType(source::GameObjType::DestrObj);
    if (track.GetGameObj().GetLogic() != &logic ||
        track.GetDestrObj() == nullptr ||
        track.GetRecordProxy() != &sharedTrackRecord)
        return 12;

    auto& bonus = map.AddMapObj(
        source::MapObjCategory::Bonus,
        source::GameObjType::Proj,
        "world\\db\\root\\ctBonus\\money", 5U);
    if (bonus.GetId() != 10U || map.GetMapObj(10U) != &bonus ||
        bonus.GetSourceIndex() != 5U || bonus.GetName() != "money0")
        return 3;

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
        return 4;

    // Map::GetMapObj hides lsDeath by default, while includeDead preserves
    // access until MapObjects finishes the progress callback and removal.
    semaphore.GetGameObj().Death(
        r3d::game::originalrace::DamageType::Simple);
    if (map.GetMapObj(7U) != nullptr ||
        map.GetMapObj(7U, true) != &semaphore)
        return 5;
    const auto special = map.GetMapObjList(
        source::MapObjCategory::Decoration).OnProgressSpecial(0.0F);
    if (special.progressed != 1U || special.removed != 1U ||
        map.GetMapObj(7U, true) != nullptr ||
        map.GetObjects().size() != 2U || map.GetSemaphore() != nullptr)
        return 6;

    if (!map.DelMapObj(&track) || map.DelMapObj(&track) ||
        map.GetMapObj(9U, true) != nullptr ||
        map.GetObjects().size() != 1U)
        return 7;

    map.Clear();
    const auto retainedTrackRecords =
        map.GetRecordLib(source::MapObjCategory::Track).GetRecordCount();
    if (!map.GetObjects().empty() || map.GetLastId() != 0U ||
        map.GetTrace().GetPathCount() != 1U ||
        map.GetTrace().FindPoint(17U) != tracePoint ||
        retainedTrackRecords != 1U)
        return 8;
    auto& first = map.AddMapObj(
        source::MapObjCategory::Effects,
        source::GameObjType::GameObj, "Effect\\smoke", 0U);
    auto& sameLeafInAnotherCategory = map.AddMapObj(
        source::MapObjCategory::Decoration,
        source::GameObjType::GameObj, "Decoration\\smoke", 1U);
    if (first.GetId() != 1U || map.GetMapObj(1U) != &first ||
        first.GetName() != "smoke0" ||
        sameLeafInAnotherCategory.GetName() != "smoke1")
        return 9;

    map.Clear();
    map.ReserveIdsThrough(293U);
    auto& firstCar = map.AddMapObj(
        source::MapObjCategory::Car,
        source::GameObjType::RockCar, "Car\\marauder", 0U);
    auto& secondCar = map.AddMapObj(
        source::MapObjCategory::Car,
        source::GameObjType::RockCar, "Car\\marauder", 1U);
    if (firstCar.GetId() != 294U || firstCar.GetName() != "marauder0" ||
        secondCar.GetId() != 295U || secondCar.GetName() != "marauder1")
        return 10;

    source::GameObject fallingObject;
    fallingObject.ResetGameObject(25.0F);
    if (!map.GetGroundTouchDeath().OnContact(&fallingObject) ||
        fallingObject.GetLiveState() !=
            source::GameObject::LiveState::Death ||
        map.GetGroundTouchDeath().OnContact(nullptr))
        return 11;

    std::cout << "original Map ownership, trace, death plane, category "
                 "registry and global ID rules passed\n";
    return 0;
}
