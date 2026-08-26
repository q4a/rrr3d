#include "OriginalGameObject.h"
#include "OriginalLogic.h"
#include "OriginalMapObj.h"
#include "OriginalPlayer.h"
#include "OriginalRockCar.h"
#include "OriginalWeapon.h"

#include <cmath>
#include <iostream>
#include <string>

namespace source = r3d::game::originalrace::source;

namespace
{

class LockedRemovalProbe final : public source::GameObjectListener
{
public:
    LockedRemovalProbe(source::MapObjects& owner, std::size_t slot)
        : owner_(owner), slot_(slot)
    {
    }

    void OnDeath(source::GameObject&, r3d::game::originalrace::DamageType,
                 source::GameObject*) noexcept override
    {
        called = true;
        observedLocked = owner_.IsLocked();
        removedInsideCallback = owner_.Remove(slot_);
    }

    bool called = false;
    bool observedLocked = false;
    bool removedInsideCallback = false;

private:
    source::MapObjects& owner_;
    std::size_t slot_ = 0U;
};

} // namespace

int main()
{
    if (std::string(source::GameObjTypeName(
            source::GameObjType::Proj)) != "gotProj" ||
        std::string(source::MapObjCategoryName(
            source::MapObjCategory::Bonus)) != "ctBonus")
        return 1;

    source::GameObject parent;
    source::Logic logic;
    parent.ResetGameObject(100.0F);
    parent.SetLogic(&logic);
    source::MapObjects objects(&parent);
    objects.Reserve(4U);
    source::Player player;

    // Windows MapObj::ClassList constructs a concrete instance for every
    // serialized GameObjType. Verify that the portable factory and progress
    // dispatch do not collapse cars and weapons to GameObject placeholders.
    source::RockCar liveRockCar;
    source::MapObjects typedObjects;
    auto& gameCarObject = typedObjects.Add(
        source::GameObjType::GameCar,
        source::MapObjCategory::Car, "Car\\gameCar", 38U);
    auto& rockCarObject = typedObjects.Add(
        source::GameObjType::RockCar,
        source::MapObjCategory::Car, "Car\\rockCar", 39U);
    auto& weaponObject = typedObjects.Add(
        source::GameObjType::Weapon,
        source::MapObjCategory::Weapon, "Weapon\\laser", 40U);
    rockCarObject.BindGameObj(liveRockCar);
    if (gameCarObject.GetGameCar() == nullptr ||
        gameCarObject.GetRockCar() != nullptr ||
        rockCarObject.GetGameCar() == nullptr ||
        rockCarObject.GetRockCar() != &liveRockCar ||
        liveRockCar.GetMapObj() != &rockCarObject ||
        weaponObject.GetWeapon() == nullptr)
        return 12;
    source::Weapon::Desc mapWeaponDescription;
    mapWeaponDescription.shotDelay = 0.1F;
    weaponObject.GetWeapon()->SetDesc(mapWeaponDescription);
    weaponObject.GetWeapon()->Reset();
    auto& carWeaponObject =
        rockCarObject.GetRockCar()->GetWeapons().Add(
            mapWeaponDescription, "Weapon\\carLaser");
    auto* carWeapon = carWeaponObject.GetWeapon();
    if (typedObjects.ProgressOne(2U, 0.11F) ||
        !weaponObject.GetWeapon()->IsReadyShot())
        return 13;
    if (typedObjects.ProgressOne(1U, 0.11F) ||
        carWeapon == nullptr || !carWeapon->IsReadyShot() ||
        carWeapon->GetParent() != rockCarObject.GetRockCar())
        return 14;
    typedObjects.Clear();
    if (liveRockCar.GetMapObj() != nullptr ||
        liveRockCar.GetLogic() != nullptr)
        return 15;

    auto& crush = objects.Add(
        source::GameObjType::GameObj,
        source::MapObjCategory::Decoration,
        "Data\\Crush\\pregrada", 41U);
    crush.GetGameObj().ResetGameObject(25.0F);
    crush.GetGameObj().SetLife(17.0F);
    crush.SetPlayer(&player);
    crush.SetType(source::GameObjType::DestrObj);
    if (crush.GetOwner() != &objects || crush.GetParent() != nullptr ||
        !parent.GetChildren().empty() || !crush.GetName().empty() ||
        crush.GetId() != 41U || !crush.IsSpecial() ||
        crush.GetPlayer() != &player ||
        crush.GetGameObj().GetMapObj() != &crush ||
        crush.GetDestrObj() == nullptr ||
        crush.GetGameObj().GetLogic() != &logic ||
        crush.GetGameObj().GetLife() != -1.0F)
        return 2;
    // Record loading follows concrete construction in MapObj::LoadSource.
    // Reapply the serialized life after verifying CreateGameObj's reset.
    crush.GetGameObj().ResetGameObject(25.0F);

    auto& architecture = objects.Add(
        source::GameObjType::DestrObj,
        source::MapObjCategory::Decoration,
        "Data\\Architecture\\tower", 42U);
    architecture.GetGameObj().ResetGameObject(10.0F);
    architecture.GetGameObj().SetMaxTimeLife(0.01F);
    if (architecture.GetName() != "tower0")
        return 10;

    auto& projectile = objects.Add(
        source::GameObjType::Proj,
        source::MapObjCategory::Bonus,
        "Bonus\\maslo", 43U);
    projectile.GetGameObj().ResetGameObject(-1.0F);
    auto* autoProjectile = projectile.GetAutoProj();
    if (autoProjectile == nullptr || projectile.GetName() != "maslo0" ||
        projectile.IsSpecial() ||
        parent.GetChildren().size() != 2U ||
        autoProjectile->GetLogic() != &logic)
        return 3;
    autoProjectile->Reset(source::AutoProj::masloType);
    if (!autoProjectile->IsPrepared())
        return 9;

    // Only Decoration/Misc and Decoration/Crush belong to the source
    // special list. The architecture lifetime and oil arming timer must not
    // advance on this path.
    crush.GetDestrObj()->Damage(
        0U, 30.0F, r3d::game::originalrace::DamageType::Simple);
    const auto special = objects.OnProgressSpecial(0.02F);
    if (special.progressed != 1U || special.removed != 1U ||
        objects.Get(0U) != nullptr || objects.Get(1U) == nullptr ||
        architecture.GetGameObj().GetLiveState() !=
            source::GameObject::LiveState::Live ||
        !autoProjectile->IsArming())
        return 4;

    // The normal list progresses every surviving object, executes the
    // callback first, and only then removes entries in lsDeath.
    const auto normal = objects.OnProgress(0.02F);
    if (normal.progressed != 2U || normal.removed != 1U ||
        objects.Get(1U) != nullptr || objects.Get(2U) == nullptr ||
        autoProjectile->GetModelScale() <= 0.0F)
        return 5;

    // RemoveItem is forbidden while the object's OnProgress callback is
    // executing. The timed death listener observes the source container lock;
    // removal happens exactly once after the callback returns.
    auto& lockedObject = objects.Add(
        source::GameObjType::GameObj,
        source::MapObjCategory::Effects,
        "Effect\\lockedRemoval", 44U);
    lockedObject.GetGameObj().ResetGameObject(-1.0F);
    lockedObject.GetGameObj().SetMaxTimeLife(0.01F);
    LockedRemovalProbe lockedProbe(objects, 3U);
    lockedObject.GetGameObj().InsertListener(&lockedProbe);
    const auto lockedProgress = objects.ProgressOne(3U, 0.02F);
    if (!lockedProgress || !lockedProbe.called ||
        !lockedProbe.observedLocked || lockedProbe.removedInsideCallback ||
        objects.Get(3U) != nullptr)
        return 11;

    auto& duplicate = objects.Add(
        source::GameObjType::GameObj, "Bonus\\maslo");
    if (duplicate.GetName() != "Bonus\\maslo0" ||
        duplicate.GetName() == projectile.GetName())
        return 6;

    objects.Death(
        static_cast<int>(r3d::game::originalrace::DamageType::DeathPlane),
        &parent);
    const auto dead = objects.OnProgress(0.0F);
    if (dead.progressed != 2U || dead.removed != 2U ||
        objects.GetLiveCount() != 0U || objects.GetSlotCount() != 5U ||
        !parent.GetChildren().empty())
        return 7;

    objects.Clear();
    if (objects.GetSlotCount() != 0U)
        return 8;

    std::cout << "original MapObj/MapObjects ownership and progress rules "
                 "passed\n";
    return 0;
}
