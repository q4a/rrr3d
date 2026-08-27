#include "OriginalGameObject.h"
#include "OriginalLogic.h"
#include "OriginalMap.h"

#include <cmath>
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

    bool semaphoreSourceLoaded = false;
    auto& semaphoreRecord =
        map.GetRecordLib(source::MapObjCategory::Decoration)
            .DefineRecord(
                "world\\db\\root\\ctDecoration\\Misc\\semaphore",
                source::GameObjType::DestrObj,
                [&](source::MapObj& value) {
                    semaphoreSourceLoaded = true;
                    value.GetGameObj().ResetGameObject(10.0F);
                    value.GetGameObj().SetMaxTimeLife(6.0F);
                });

    auto& semaphore = map.AddMapObj(
        source::MapObjCategory::Decoration,
        source::GameObjType::DestrObj,
        "world\\db\\root\\ctDecoration\\Misc\\semaphore",
        7U, 2U);
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
        !semaphoreSourceLoaded ||
        semaphore.GetRecordProxy() != &semaphoreRecord ||
        semaphore.GetGameObj().GetMaxLife() != 10.0F ||
        semaphore.GetGameObj().GetMaxTimeLife() != 6.0F ||
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
    const auto* touchDeath = map.GetGround().GetGameObj()
        .GetBehaviors().Find(source::BehaviorType::TouchDeath);
    map.GetGround().GetGameObj().OnContact(&fallingObject);
    map.GetGround().GetGameObj().OnContact(nullptr);
    if (touchDeath != &map.GetGroundTouchDeath() ||
        map.GetGround().GetGameObj().GetListenerCount() != 1U ||
        fallingObject.GetLiveState() !=
            source::GameObject::LiveState::Death)
        return 11;

    source::Map cloneMap(&logic);
    auto& cloneSource = cloneMap.AddMapObj(
        source::MapObjCategory::Decoration,
        source::GameObjType::DestrObj,
        "world\\db\\root\\ctDecoration\\Crush\\crate", 4U);
    cloneSource.SetSourceIndex(77U);
    cloneSource.GetGameObj().ResetGameObject(30.0F);
    cloneSource.GetGameObj().SetLife(18.0F);
    cloneSource.GetGameObj().SetPos({1.0F, 2.0F, 3.0F});
    cloneSource.GetGameObj().SetScale({2.0F, 3.0F, 4.0F});
    cloneSource.GetGameObj().SetRot({0.0F, 0.0F, 0.5F, 0.8660254F});
    cloneSource.GetGameObj().SetMaxTimeLife(9.0F);
    cloneSource.GetGameObj().SetTimeLife(1.5F);
    auto& childRecord =
        cloneMap.GetRecordLib(source::MapObjCategory::Effects)
            .GetOrCreateRecord(
                "world\\db\\root\\ctEffects\\spark2",
                source::GameObjType::GameObj);
    auto& cloneSourceChild = cloneSource.GetGameObj().GetIncludeList().Add(
        childRecord, 4U);
    cloneSourceChild.SetName("sparkChild");
    cloneSourceChild.GetGameObj().ResetGameObject(-1.0F);
    cloneSourceChild.GetGameObj().SetPos({5.0F, 6.0F, 7.0F});

    auto& clone = cloneMap.AddMapObj(cloneSource);
    const auto* clonedChild = clone.GetGameObj().GetIncludeList().Get(0U);
    if (cloneSource.GetId() != 1U || cloneSource.GetName() != "crate0" ||
        clone.GetId() != 2U || clone.GetName() != "crate1" ||
        clone.GetRecordProxy() != cloneSource.GetRecordProxy() ||
        clone.GetPlayer() != nullptr ||
        clone.GetSourceIndex() != source::Map::invalidSourceIndex ||
        clone.GetGameObj().GetLogic() != &logic ||
        clone.GetGameObj().GetMaxLife() != 30.0F ||
        clone.GetGameObj().GetLife() != 18.0F ||
        clone.GetGameObj().GetPos() !=
            source::GameObject::Vector3{1.0F, 2.0F, 3.0F} ||
        clone.GetGameObj().GetScale() !=
            source::GameObject::Vector3{2.0F, 3.0F, 4.0F} ||
        clone.GetGameObj().GetRot() !=
            source::GameObject::Quaternion{
                0.0F, 0.0F, 0.5F, 0.8660254F} ||
        clone.GetGameObj().GetMaxTimeLife() != 9.0F ||
        clone.GetGameObj().GetTimeLife() != 1.5F ||
        clone.GetGameObj().GetIncludeList().GetLiveCount() != 1U ||
        clonedChild == nullptr || clonedChild->GetName() != "sparkChild" ||
        clonedChild->GetId() != 4U ||
        clonedChild->GetRecordProxy() != &childRecord ||
        clonedChild->GetParent() != &clone.GetGameObj() ||
        clonedChild->GetGameObj().GetLogic() != &logic ||
        clonedChild->GetGameObj().GetPos() !=
            source::GameObject::Vector3{5.0F, 6.0F, 7.0F})
        return 18;

    source::Map transferMap(&logic);
    source::GameObject fragmentOwner;
    fragmentOwner.SetLogic(&logic);
    auto& fragmentRecord =
        transferMap.GetRecordLib(source::MapObjCategory::Decoration)
            .GetOrCreateRecord(
                "world\\db\\root\\ctDecoration\\Crush\\fragment",
                source::GameObjType::DestrObj);
    auto& fragment = fragmentOwner.GetIncludeList().Add(
        fragmentRecord, 91U);
    fragment.SetName("oldFragmentName");
    fragment.GetGameObj().ResetGameObject(15.0F);
    fragment.GetGameObj().SetPos({8.0F, 9.0F, 10.0F});
    auto extracted = fragmentOwner.GetIncludeList().Extract(&fragment);
    if (extracted == nullptr || extracted.get() != &fragment ||
        fragmentOwner.GetIncludeList().GetLiveCount() != 0U ||
        !fragmentOwner.GetChildren().empty() ||
        fragment.GetOwner() != nullptr || fragment.GetParent() != nullptr ||
        fragment.GetGameObj().IsObjectDestroyed())
        return 19;
    auto& insertedFragment = transferMap.InsertMapObj(
        std::move(extracted));
    if (&insertedFragment != &fragment || insertedFragment.GetId() != 1U ||
        insertedFragment.GetName() != "item0" ||
        insertedFragment.GetCategory() !=
            source::MapObjCategory::Decoration ||
        insertedFragment.GetRecordProxy() != &fragmentRecord ||
        insertedFragment.GetOwner() !=
            &transferMap.GetMapObjList(
                source::MapObjCategory::Decoration) ||
        insertedFragment.GetParent() != nullptr ||
        insertedFragment.GetGameObj().GetLogic() != &logic ||
        insertedFragment.GetGameObj().GetMaxLife() != 15.0F ||
        insertedFragment.GetGameObj().GetPos() !=
            source::GameObject::Vector3{8.0F, 9.0F, 10.0F} ||
        transferMap.GetMapObj(1U) != &insertedFragment)
        return 20;
    auto recordless = std::make_unique<source::MapObj>();
    recordless->SetType(source::GameObjType::GameObj);
    auto& insertedRecordless = transferMap.InsertMapObj(
        std::move(recordless));
    if (insertedRecordless.GetId() != 2U ||
        insertedRecordless.GetName() != "item1" ||
        insertedRecordless.GetRecordProxy() != nullptr ||
        insertedRecordless.GetCategory() !=
            source::MapObjCategory::Decoration ||
        transferMap.GetMapObj(2U) != &insertedRecordless)
        return 21;

    source::Map destructionMap(&logic);
    auto& destructibleMapObject = destructionMap.AddMapObj(
        source::MapObjCategory::Decoration,
        source::GameObjType::DestrObj,
        "world\\db\\root\\ctDecoration\\Crush\\barricade", 0U);
    auto* destructible = destructibleMapObject.GetDestrObj();
    destructible->ResetGameObject(10.0F);
    destructible->SetPos({12.0F, 13.0F, 14.0F});
    destructible->SetRot({0.0F, 0.0F, 0.7071068F, 0.7071068F});
    auto& firstPiece = destructible->GetDestrList().Add(
        source::GameObjType::GameObj, "obj");
    firstPiece.GetGameObj().ResetGameObject(-1.0F);
    firstPiece.GetGameObj().SetPos({1.0F, 0.0F, 0.0F});
    firstPiece.GetGameObj().SetScale({2.0F, 2.0F, 2.0F});
    auto& secondPiece = destructible->GetDestrList().Add(
        source::GameObjType::GameObj, "obj");
    secondPiece.GetGameObj().ResetGameObject(-1.0F);
    secondPiece.GetGameObj().SetPos({-1.0F, 0.0F, 0.0F});
    if (destructible->GetChildren().size() != 2U ||
        destructible->GetDestrList().GetLiveCount() != 2U ||
        firstPiece.GetName() != "obj0" || secondPiece.GetName() != "obj1")
        return 22;
    destructible->Damage(
        0U, 10.0F,
        r3d::game::originalrace::DamageType::Simple);
    const auto destructibleMapObjectId = destructibleMapObject.GetId();
    auto& destructionObjects = destructionMap.GetMapObjList(
        source::MapObjCategory::Decoration);
    if (!destructionObjects.ProgressOne(0U, 0.0F) ||
        destructionMap.GetMapObj(destructibleMapObjectId, true) !=
            nullptr ||
        destructionObjects.GetLiveCount() != 2U ||
        destructionObjects.GetSlotCount() != 3U ||
        destructionMap.GetObjects().size() != 2U)
        return 23;
    const auto* releasedFirst = destructionObjects.Get(1U);
    const auto* releasedSecond = destructionObjects.Get(2U);
    if (releasedFirst == nullptr || releasedSecond == nullptr ||
        releasedFirst != &firstPiece || releasedSecond != &secondPiece ||
        releasedFirst->GetId() != 2U || releasedSecond->GetId() != 3U ||
        releasedFirst->GetName() != "item0" ||
        releasedSecond->GetName() != "item1" ||
        releasedFirst->GetRecordProxy() != nullptr ||
        releasedFirst->GetCategory() !=
            source::MapObjCategory::Decoration ||
        releasedFirst->GetParent() != nullptr ||
        releasedFirst->GetGameObj().GetLogic() != &logic ||
        releasedFirst->GetGameObj().GetPos() !=
            source::GameObject::Vector3{12.0F, 13.0F, 14.0F} ||
        releasedFirst->GetGameObj().GetRot() !=
            source::GameObject::Quaternion{
                0.0F, 0.0F, 0.7071068F, 0.7071068F} ||
        releasedFirst->GetGameObj().GetScale() !=
            source::GameObject::Vector3{2.0F, 2.0F, 2.0F} ||
        releasedFirst->GetGameObj().IsObjectDestroyed())
        return 24;

    source::Map resurrectionMap(&logic);
    auto& effectParentMapObject = resurrectionMap.AddMapObj(
        source::MapObjCategory::Decoration,
        source::GameObjType::GameObj,
        "world\\db\\root\\ctDecoration\\parent", 0U);
    auto& effectParent = effectParentMapObject.GetGameObj();
    effectParent.ResetGameObject(-1.0F);
    effectParent.SetPos({10.0F, 20.0F, 30.0F});
    effectParent.SetScale({2.0F, 2.0F, 2.0F});
    effectParent.SetRot({0.0F, 0.0F, 0.7071068F, 0.7071068F});
    auto& effectRecord =
        resurrectionMap.GetRecordLib(source::MapObjCategory::Effects)
            .GetOrCreateRecord(
                "world\\db\\root\\ctEffects\\spark2",
                source::GameObjType::GameObj);
    auto& includedEffect = effectParent.GetIncludeList().Add(
        effectRecord, 77U);
    auto& effectObject = includedEffect.GetGameObj();
    effectObject.ResetGameObject(-1.0F);
    effectObject.SetPos({1.0F, 0.0F, 0.0F});
    effectObject.SetScale({0.5F, 1.0F, 1.0F});
    const auto worldEffectPosition = effectObject.GetWorldPos();
    const auto worldEffectScale = effectObject.GetWorldScale();
    const auto worldEffectRotation = effectObject.GetWorldRot();
    const auto near = [](float first, float second) {
        return std::abs(first - second) < 0.0001F;
    };
    if (!near(worldEffectPosition[0], 10.0F) ||
        !near(worldEffectPosition[1], 22.0F) ||
        !near(worldEffectPosition[2], 30.0F) ||
        !near(worldEffectScale[0], 1.0F) ||
        !near(worldEffectScale[1], 2.0F) ||
        !near(worldEffectScale[2], 2.0F) ||
        !near(worldEffectRotation[2], 0.7071068F) ||
        !near(worldEffectRotation[3], 0.7071068F))
        return 25;
    auto& waitingEnd = effectObject.GetBehaviors()
        .Add<source::FxSystemWaitingEndBehavior>(
            source::BehaviorType::FxSystemWaitingEnd);
    waitingEnd.SetLiveParticleCount(1U);
    if (!effectObject.Death())
        return 26;
    const auto detachedEffectId = includedEffect.GetId();
    if (!waitingEnd.ConsumeBeginFading() ||
        !waitingEnd.IsFading() ||
        !waitingEnd.IsResurrect() || effectObject.destroyed ||
        effectParent.GetIncludeList().GetLiveCount() != 0U ||
        !effectParent.GetChildren().empty() ||
        includedEffect.GetOwner() !=
            &resurrectionMap.GetMapObjList(
                source::MapObjCategory::Effects) ||
        includedEffect.GetParent() != nullptr ||
        includedEffect.GetName() != "item0" ||
        detachedEffectId != 2U ||
        resurrectionMap.GetMapObj(detachedEffectId, true) !=
            &includedEffect ||
        effectObject.GetLogic() != &logic ||
        !near(effectObject.GetPos()[0], worldEffectPosition[0]) ||
        !near(effectObject.GetPos()[1], worldEffectPosition[1]) ||
        !near(effectObject.GetPos()[2], worldEffectPosition[2]) ||
        !near(effectObject.GetRot()[2], worldEffectRotation[2]) ||
        !near(effectObject.GetRot()[3], worldEffectRotation[3]))
        return 27;
    effectObject.OnProgress(0.0F);
    if (effectObject.destroyed ||
        waitingEnd.ConsumeFinalDeath())
        return 28;
    waitingEnd.SetLiveParticleCount(0U);
    effectObject.OnProgress(0.0F);
    if (!waitingEnd.ConsumeFinalDeath() || !effectObject.destroyed)
        return 28;
    const auto removedEffects =
        resurrectionMap.GetMapObjList(source::MapObjCategory::Effects)
            .OnProgress(0.0F);
    if (removedEffects.removed != 1U ||
        resurrectionMap.GetMapObj(detachedEffectId, true) != nullptr)
        return 29;

    source::Logic progressLogic;
    {
        source::Map progressMap(&progressLogic);
        const auto addTimed = [&progressMap](
                                  source::MapObjCategory category,
                                  std::string record) -> source::MapObj& {
            auto& value = progressMap.AddMapObj(
                category, source::GameObjType::GameObj,
                std::move(record), 0U);
            value.GetGameObj().ResetGameObject(-1.0F);
            value.GetGameObj().SetMaxTimeLife(0.01F);
            return value;
        };
        addTimed(
            source::MapObjCategory::Decoration,
            "world\\db\\root\\ctDecoration\\Misc\\timedSpecial");
        auto& ordinaryDecoration = addTimed(
            source::MapObjCategory::Decoration,
            "world\\db\\root\\ctDecoration\\Architecture\\timedOrdinary");
        addTimed(source::MapObjCategory::Effects,
                 "world\\db\\root\\ctEffects\\timedEffect");
        addTimed(source::MapObjCategory::Car,
                 "world\\db\\root\\ctCar\\timedCar");
        addTimed(source::MapObjCategory::Bonus,
                 "world\\db\\root\\ctBonus\\timedBonus");
        auto& trackNotProgressed = addTimed(
            source::MapObjCategory::Track,
            "world\\db\\root\\ctTrack\\timedTrack");
        auto& weaponNotProgressed = addTimed(
            source::MapObjCategory::Weapon,
            "world\\db\\root\\ctWeapon\\timedWeapon");
        auto& waypointNotProgressed = addTimed(
            source::MapObjCategory::Waypoint,
            "world\\db\\root\\ctWaypoint\\timedWaypoint");
        auto* transient = new source::GameObject();
        transient->ResetGameObject(-1.0F);
        transient->SetMaxTimeLife(0.01F);
        progressLogic.RegGameObj(transient);
        const auto progress = progressLogic.OnProgress(0.02F);
        if (progressLogic.GetMap() != &progressMap ||
            progress.decoration.progressed != 1U ||
            progress.decoration.removed != 1U ||
            progress.effects.progressed != 1U ||
            progress.effects.removed != 1U ||
            progress.cars.progressed != 1U ||
            progress.cars.removed != 1U ||
            progress.bonuses.progressed != 1U ||
            progress.bonuses.removed != 1U ||
            progress.transient.progressed != 1U ||
            progress.transient.removed != 1U ||
            progressMap.GetObjects().size() != 4U ||
            ordinaryDecoration.GetGameObj().GetLiveState() !=
                source::GameObject::LiveState::Live ||
            trackNotProgressed.GetGameObj().GetLiveState() !=
                source::GameObject::LiveState::Live ||
            weaponNotProgressed.GetGameObj().GetLiveState() !=
                source::GameObject::LiveState::Live ||
            waypointNotProgressed.GetGameObj().GetLiveState() !=
                source::GameObject::LiveState::Live ||
            progressLogic.GetGameObjCount() != 0U)
            return 30;
    }
    if (progressLogic.GetMap() != nullptr)
        return 31;

    std::cout << "original Map ownership, trace, death plane, category "
                 "registry and global ID rules passed\n";
    return 0;
}
