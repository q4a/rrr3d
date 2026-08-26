#include "OriginalGameObject.h"
#include "OriginalLogic.h"
#include "OriginalMapObj.h"
#include "OriginalPlayer.h"
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
    source::Logic logic;
    parent.ResetGameObject(100.0F);
    parent.SetLogic(&logic);
    source::MapObjects objects(&parent);
    objects.Reserve(4U);
    source::Player player;

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
        objects.GetLiveCount() != 0U || objects.GetSlotCount() != 4U ||
        !parent.GetChildren().empty())
        return 7;

    objects.Clear();
    if (objects.GetSlotCount() != 0U)
        return 8;

    std::cout << "original MapObj/MapObjects ownership and progress rules "
                 "passed\n";
    return 0;
}
