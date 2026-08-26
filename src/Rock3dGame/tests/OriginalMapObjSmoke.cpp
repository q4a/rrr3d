#include "OriginalGameObject.h"
#include "OriginalMapObj.h"
#include "OriginalWeapon.h"

#include <cmath>
#include <iostream>
#include <string>

namespace source = r3d::game::originalrace::source;

int main()
{
    if (std::string(source::GameObjTypeName(
            source::GameObjType::Proj)) != "gotProj" ||
        std::string(source::MapObjCategoryName(
            source::MapObjCategory::Bonus)) != "ctBonus")
        return 1;

    source::GameObject parent;
    parent.ResetGameObject(100.0F);
    source::MapObjects objects(&parent);
    objects.Reserve(4U);

    auto& crush = objects.Add(
        source::GameObjType::GameObj,
        source::MapObjCategory::Decoration,
        "Data\\Crush\\pregrada", 41U);
    crush.GetGameObj().ResetGameObject(25.0F);
    crush.GetGameObj().SetLife(17.0F);
    crush.SetType(source::GameObjType::DestrObj);
    if (crush.GetOwner() != &objects || crush.GetParent() != &parent ||
        crush.GetId() != 41U || !crush.IsSpecial() ||
        crush.GetDestrObj() == nullptr ||
        std::abs(crush.GetGameObj().GetLife() - 17.0F) > 0.0001F)
        return 2;

    auto& architecture = objects.Add(
        source::GameObjType::DestrObj,
        source::MapObjCategory::Decoration,
        "Data\\Architecture\\tower", 42U);
    architecture.GetGameObj().ResetGameObject(10.0F);
    architecture.GetGameObj().SetMaxTimeLife(0.01F);

    auto& projectile = objects.Add(
        source::GameObjType::Proj,
        source::MapObjCategory::Bonus,
        "Bonus\\maslo", 43U);
    projectile.GetGameObj().ResetGameObject(-1.0F);
    auto* autoProjectile = projectile.GetAutoProj();
    if (autoProjectile == nullptr || projectile.IsSpecial())
        return 3;
    autoProjectile->Reset(source::AutoProj::masloType);
    autoProjectile->LogicInited();

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

    auto& duplicate = objects.Add(
        source::GameObjType::GameObj, "Bonus\\maslo");
    if (duplicate.GetName() == projectile.GetName())
        return 6;

    objects.Death(
        static_cast<int>(r3d::game::originalrace::DamageType::DeathPlane),
        &parent);
    const auto dead = objects.OnProgress(0.0F);
    if (dead.progressed != 2U || dead.removed != 2U ||
        objects.GetLiveCount() != 0U || objects.GetSlotCount() != 4U)
        return 7;

    objects.Clear();
    if (objects.GetSlotCount() != 0U)
        return 8;

    std::cout << "original MapObj/MapObjects ownership and progress rules "
                 "passed\n";
    return 0;
}
