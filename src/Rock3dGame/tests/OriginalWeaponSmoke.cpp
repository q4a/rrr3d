#include "OriginalGameCar.h"
#include "OriginalLogic.h"
#include "OriginalMapObj.h"

#include <array>
#include <cmath>
#include <iostream>

namespace source = r3d::game::originalrace::source;

namespace
{

bool nearVector(
    const source::GameObject::Vector3& value,
    const source::GameObject::Vector3& expected) noexcept
{
    return std::abs(value[0] - expected[0]) < 0.0001F &&
           std::abs(value[1] - expected[1]) < 0.0001F &&
           std::abs(value[2] - expected[2]) < 0.0001F;
}

struct BonusDeathOrder final : source::GameObjectListener
{
    source::Player* player = nullptr;
    std::uint32_t moneyAtDeath = 0U;

    void OnDeath(
        source::GameObject&,
        r3d::game::originalrace::DamageType,
        source::GameObject*) noexcept override
    {
        moneyAtDeath =
            player == nullptr ? 0U : player->GetPickMoney();
    }
};

} // namespace

int main()
{
    const std::array<std::uint32_t, 2> projectiles{10U, 11U};
    source::Weapon weapon;
    weapon.SetDesc(0.1F, projectiles);
    auto* shotBehavior = dynamic_cast<source::ShotEffectBehavior*>(
        weapon.GetBehaviors().Find(source::BehaviorType::ShotEffect));
    if (shotBehavior == nullptr ||
        shotBehavior->GetGameObj() != &weapon ||
        weapon.GetBehaviors().GetCount() != 1U ||
        weapon.GetListenerCount() != 1U ||
        weapon.IsReadyShot() || !weapon.IsMaslo() ||
        std::abs(weapon.GetShotTime()) > 0.0001F)
        return 1;

    weapon.OnProgress(0.1F);
    if (weapon.IsReadyShot())
        return 2;
    weapon.OnProgress(0.001F);
    if (!weapon.IsReadyShot())
        return 3;

    weapon.OnShot(false);
    if (!weapon.IsReadyShot() ||
        weapon.GetShotEffect().GetShotCount() != 0U)
        return 4;
    weapon.OnShot(true);
    if (weapon.IsReadyShot() ||
        std::abs(weapon.GetShotTime()) > 0.0001F ||
        weapon.GetShotEffect().GetShotCount() != 0U)
        return 5;
    weapon.OnProjectilePrepared();
    weapon.OnProjectilePrepared({1.0F, 2.0F, 3.0F});
    if (weapon.GetShotEffect().GetShotCount() != 2U ||
        weapon.GetLastShotPosition() !=
            std::array<float, 3U>{1.0F, 2.0F, 3.0F})
        return 6;
    source::Weapon copiedWeapon = weapon;
    const auto* copiedShotBehavior =
        dynamic_cast<const source::ShotEffectBehavior*>(
            copiedWeapon.GetBehaviors().Find(
                source::BehaviorType::ShotEffect));
    if (copiedShotBehavior == nullptr ||
        copiedShotBehavior->GetGameObj() != &copiedWeapon ||
        copiedWeapon.GetListenerCount() != 1U ||
        copiedWeapon.GetShotEffect().GetShotCount() != 2U ||
        copiedWeapon.GetLastShotPosition() !=
            std::array<float, 3U>{1.0F, 2.0F, 3.0F})
        return 6;

    r3d::game::originalrace::ProjectileDefinition sourceDescription;
    sourceDescription.type = 3U;
    sourceDescription.damage = 9.0F;
    sourceDescription.position = {1.0F, 2.0F, 3.0F};
    sourceDescription.rotation =
        {0.0F, 0.0F, 0.70710677F, 0.70710677F};
    sourceDescription.visual.record = "Effect\\laserModel";
    sourceDescription.secondaryVisual.record = "Effect\\laserModel2";
    source::GameObject sourceCar;
    source::Weapon sourceWeapon;
    sourceWeapon.SetParent(&sourceCar);
    source::GameObject sourceTarget;
    source::Proj linkedProjectile;
    source::Proj::ShotContext linkedContext;
    linkedContext.shot.targetMapObject = &sourceTarget;
    linkedContext.playerId = 7U;
    linkedContext.maximumLife = 2.5F;
    linkedContext.position = {4.0F, 5.0F, 6.0F};
    linkedContext.rotation = {0.0F, 0.0F, 0.0F, 1.0F};
    linkedProjectile.PrepareSource(
        sourceDescription, &sourceWeapon, linkedContext);
    if (!linkedProjectile.IsPrepared() ||
        linkedProjectile.GetDesc().type != 3U ||
        linkedProjectile.GetSourceWeapon() != &sourceWeapon ||
        linkedProjectile.GetSourceTarget() != &sourceTarget ||
        linkedProjectile.GetSourcePlayerId() != 7U ||
        !linkedProjectile.GetIgnoreContactProj() ||
        linkedProjectile.GetParent() != &sourceWeapon ||
        linkedProjectile.GetPos() !=
            source::GameObject::Vector3{1.0F, 2.0F, 3.0F} ||
        linkedProjectile.GetRot() !=
            source::GameObject::Quaternion{
                0.0F, 0.0F, 0.70710677F, 0.70710677F} ||
        !nearVector(
            linkedProjectile.GetWorldPos(), {4.0F, 5.0F, 6.0F}) ||
        linkedProjectile.GetMaxTimeLife() != 2.5F ||
        linkedProjectile.GetSourceModel() == nullptr ||
        linkedProjectile.GetSourceModel2() == nullptr ||
        linkedProjectile.GetIncludeList().GetLiveCount() != 2U ||
        linkedProjectile.GetSourceModel()->GetParent() !=
            &linkedProjectile ||
        linkedProjectile.GetSourceModel2()->GetParent() !=
            &linkedProjectile ||
        sourceWeapon.GetListenerCount() != 2U ||
        sourceTarget.GetListenerCount() != 1U)
        return 65;
    linkedProjectile.SyncSourceTransform(
        {14.0F, 15.0F, 16.0F},
        {0.0F, 0.0F, 0.0F, 1.0F});
    if (linkedProjectile.GetPos() !=
            source::GameObject::Vector3{1.0F, 2.0F, 3.0F} ||
        linkedProjectile.GetRot() !=
            source::GameObject::Quaternion{
                0.0F, 0.0F, 0.70710677F, 0.70710677F} ||
        !nearVector(
            linkedProjectile.GetWorldPos(), {14.0F, 15.0F, 16.0F}))
        return 109;
    const auto concreteLaser = linkedProjectile.ProgressLaser(
        100.0F, true, 40.0F, 0.1F, 9.0F, true,
        1.0F, 2.5F, {1.0F, 0.0F, 0.0F});
    if (concreteLaser.distance != 40.0F ||
        linkedProjectile.GetSourceModel2() == nullptr ||
        !nearVector(
            linkedProjectile.GetSourceModel2()->GetGameObj().GetWorldPos(),
            {54.0F, 15.0F, 16.0F}))
        return 120;
    linkedProjectile.ProgressLaser(
        100.0F, false, 0.0F, 0.1F, 9.0F, true,
        1.0F, 2.5F, {1.0F, 0.0F, 0.0F});
    if (linkedProjectile.GetSourceModel2()->GetGameObj().GetPos() !=
        source::GameObject::Vector3{100.0F, 0.0F, 0.0F})
        return 121;
    linkedProjectile.SetSourceTimer(0.4F);
    linkedProjectile.SetSourceVector({7.0F, 8.0F, 9.0F});
    linkedProjectile.SetSourceTick(2U);
    linkedProjectile.SetSourceState(true);
    linkedProjectile.SetIgnoreContactProj(true);
    if (linkedProjectile.GetSourceTimer() != 0.4F ||
        linkedProjectile.GetSourceVector() !=
            source::Proj::Vec3{7.0F, 8.0F, 9.0F} ||
        linkedProjectile.GetSourceTick() != 2U ||
        !linkedProjectile.GetSourceState() ||
        !linkedProjectile.GetIgnoreContactProj())
        return 98;
    linkedProjectile.ResetSourceRuntimeState();
    if (linkedProjectile.GetSourceTimer() != 0.0F ||
        linkedProjectile.GetSourceVector() != source::Proj::Vec3{} ||
        linkedProjectile.GetSourceTick() != 0U ||
        linkedProjectile.GetSourceState() ||
        linkedProjectile.GetIgnoreContactProj())
        return 99;
    auto* primaryModel = linkedProjectile.GetSourceModel();
    primaryModel->GetGameObj().DestroyObject();
    if (linkedProjectile.GetSourceModel() != nullptr ||
        !linkedProjectile.FreeSourceModel(true, true) ||
        linkedProjectile.GetSourceModel2() != nullptr ||
        linkedProjectile.GetIncludeList().GetLiveCount() != 1U)
        return 100;
    sourceTarget.DestroyObject();
    if (linkedProjectile.GetSourceTarget() != nullptr)
        return 66;
    sourceWeapon.DestroyObject();
    if (linkedProjectile.GetSourceWeapon() != nullptr ||
        linkedProjectile.GetParent() != nullptr ||
        linkedProjectile.GetLiveState() !=
            source::GameObject::LiveState::Death)
        return 67;

    source::Weapon mineSourceWeapon;
    source::Proj unlinkedProjectile;
    auto unlinkedDescription = sourceDescription;
    unlinkedDescription.type = 4U;
    source::Proj::ShotContext unlinkedContext;
    unlinkedContext.playerId = 4U;
    unlinkedContext.position = {7.0F, 8.0F, 9.0F};
    unlinkedProjectile.PrepareSource(
        unlinkedDescription, &mineSourceWeapon, unlinkedContext);
    if (unlinkedProjectile.GetPos() !=
            source::GameObject::Vector3{7.0F, 8.0F, 9.0F} ||
        unlinkedProjectile.GetParent() != nullptr)
        return 110;
    mineSourceWeapon.DestroyObject();
    if (unlinkedProjectile.GetSourceWeapon() != nullptr ||
        unlinkedProjectile.GetParent() != nullptr ||
        unlinkedProjectile.GetLiveState() ==
            source::GameObject::LiveState::Death)
        return 68;

    auto drobilkaDescription = sourceDescription;
    drobilkaDescription.type = 15U;
    drobilkaDescription.damage = 10.0F;
    drobilkaDescription.angularSpeed = 3.14159265358979323846F;
    drobilkaDescription.secondaryVisual = {};
    source::Weapon drobilkaWeapon;
    source::Proj drobilkaProjectile;
    source::Proj::ShotContext drobilkaContext;
    drobilkaContext.maximumLife = 4.0F;
    drobilkaProjectile.PrepareSource(
        drobilkaDescription, &drobilkaWeapon, drobilkaContext);
    if (drobilkaProjectile.GetSourceModel() != nullptr ||
        !drobilkaProjectile.GetIgnoreContactProj() ||
        !drobilkaProjectile.InitSourceModel() ||
        drobilkaProjectile.GetSourceModel() == nullptr ||
        drobilkaProjectile.GetIncludeList().GetLiveCount() != 1U)
        return 101;
    source::GameObject drobilkaTarget;
    drobilkaTarget.ResetGameObject(100.0F);
    const auto concreteDrobilka = drobilkaProjectile.ContactDrobilka(
        &drobilkaTarget, 0.1F, {7.0F, 8.0F, 9.0F});
    drobilkaProjectile.ProgressDrobilka(0.25F);
    const auto spunWeapon = drobilkaWeapon.GetRot();
    if (concreteDrobilka.damage != 1.0F ||
        drobilkaProjectile.GetSourceModel() == nullptr ||
        drobilkaProjectile.GetSourceModel()->GetGameObj().GetWorldPos() !=
            source::GameObject::Vector3{7.0F, 8.0F, 9.0F} ||
        std::abs(spunWeapon[0] - 0.38268343F) > 0.001F ||
        std::abs(drobilkaProjectile.GetSourceTimer() - 0.25F) > 0.001F)
        return 122;
    drobilkaProjectile.ProgressDrobilka(0.3F);
    if (drobilkaProjectile.GetSourceModel() != nullptr)
        return 123;

    auto torpedaDescription = sourceDescription;
    torpedaDescription.type = 2U;
    torpedaDescription.secondaryVisual = {};
    source::Proj::ShotContext torpedaContext;
    torpedaContext.launchVelocity = {3.0F, 4.0F, 5.0F};
    source::Proj torpedaProjectile;
    torpedaProjectile.PrepareSource(
        torpedaDescription, nullptr, torpedaContext);
    if (torpedaProjectile.GetSourceTimer() != 0.4F ||
        torpedaProjectile.GetSourceVector() !=
            source::Proj::Vec3{3.0F, 4.0F, 5.0F} ||
        !torpedaProjectile.GetIgnoreContactProj())
        return 111;

    auto oilDescription = sourceDescription;
    oilDescription.type = 10U;
    oilDescription.secondaryVisual = {};
    source::Proj oilProjectile;
    oilProjectile.PrepareSource(
        oilDescription, nullptr, source::Proj::ShotContext{});
    if (oilProjectile.GetSourceTimer() != 0.0F ||
        oilProjectile.GetSourceModel() == nullptr ||
        oilProjectile.GetSourceModel()->GetGameObj().GetScale() !=
            source::GameObject::Vector3{0.0F, 0.0F, 0.0F})
        return 112;

    auto minePieceDescription = sourceDescription;
    minePieceDescription.type = 13U;
    minePieceDescription.secondaryVisual = {};
    source::Proj minePieceProjectile;
    minePieceProjectile.PrepareSource(
        minePieceDescription, nullptr, source::Proj::ShotContext{});
    if (minePieceProjectile.GetSourceTimer() != -1.0F)
        return 113;

    auto craterDescription = sourceDescription;
    craterDescription.type = 20U;
    craterDescription.secondaryVisual = {};
    source::Proj craterProjectile;
    craterProjectile.PrepareSource(
        craterDescription, nullptr, source::Proj::ShotContext{});
    auto protonDescription = sourceDescription;
    protonDescription.type = 24U;
    protonDescription.secondaryVisual = {};
    source::Proj protonProjectile;
    protonProjectile.PrepareSource(
        protonDescription, nullptr, source::Proj::ShotContext{});
    if (craterProjectile.GetSourceTimer() != 0.0F ||
        protonProjectile.GetSourceTimer() != 0.0F)
        return 128;

    auto frostDescription = sourceDescription;
    frostDescription.type = 18U;
    source::Weapon frostWeapon;
    source::Proj frostProjectile;
    frostProjectile.PrepareSource(
        frostDescription, &frostWeapon,
        source::Proj::ShotContext{});
    if (!frostProjectile.GetIgnoreContactProj() ||
        frostProjectile.GetParent() != &frostWeapon)
        return 114;
    source::Player frostTargetPlayer;
    source::GameObject frostNonCarTarget;
    frostDescription.tertiaryVisual.maximumTimeLife = 2.5F;
    source::Proj concreteFrostProjectile;
    concreteFrostProjectile.PrepareSource(
        frostDescription, &frostWeapon,
        source::Proj::ShotContext{});
    const auto concreteFrost = concreteFrostProjectile.ProgressFrostRay(
        100.0F, true, 20.0F, 0.1F, 5.0F,
        0.0F, 1.0F, {1.0F, 0.0F, 0.0F});
    if (!concreteFrost.applyDamage || concreteFrost.distance != 20.0F ||
        !concreteFrostProjectile.AttachFrostSlow(
            &frostTargetPlayer.gameCar, &frostTargetPlayer, 8U, 2U) ||
        concreteFrostProjectile.AttachFrostSlow(
            &frostTargetPlayer.gameCar, &frostTargetPlayer, 9U, 3U) ||
        frostTargetPlayer.slowEffect.GetRemainingSeconds() != 2.5F ||
        frostTargetPlayer.slowEffect.GetWeapon() != 8U ||
        frostTargetPlayer.slowEffect.GetProjectile() != 2U ||
        concreteFrostProjectile.AttachFrostSlow(
            &frostNonCarTarget, &frostTargetPlayer, 8U, 2U))
        return 138;

    source::Proj projectileObject;
    projectileObject.ConfigureDeathEffect(true, true);
    auto* projectileDeathBehavior =
        projectileObject.GetDeathEffectBehavior();
    source::GameObject projectileTarget;
    projectileTarget.ResetGameObject(100.0F);
    if (projectileDeathBehavior == nullptr ||
        projectileDeathBehavior->GetGameObj() != &projectileObject ||
        projectileObject.GetBehaviors().Find(
            source::BehaviorType::DeathEffect) !=
            projectileDeathBehavior ||
        projectileObject.GetListenerCount() != 1U)
        return 6;
    const auto projectileDeath =
        projectileObject.DestroyWithEffect(
            &projectileTarget, true, true);
    if (!projectileObject.destroyed ||
        !projectileDeath.createEffect ||
        !projectileDeath.targetChild ||
        !projectileDeath.ignoreSenderCar ||
        !projectileDeathBehavior->IsEffectMaked() ||
        projectileObject.DestroyWithEffect(
            &projectileTarget, true, true).createEffect)
        return 6;

    source::RockCar rackCar;
    auto& rack = rackCar.GetWeapons();
    std::array<r3d::game::originalrace::ProjectileDefinition, 2U>
        itemProjectiles{};
    itemProjectiles[0].type = 0U;
    itemProjectiles[0].speed = 40.0F;
    itemProjectiles[0].maximumDistance = 300.0F;
    itemProjectiles[0].damage = 6.0F;
    itemProjectiles[1] = itemProjectiles[0];
    itemProjectiles[1].damage = 6.5F;
    source::Weapon::Desc primaryDescription;
    primaryDescription.shotDelay = 0.2F;
    primaryDescription.projectiles.assign(
        itemProjectiles.begin(), itemProjectiles.end());
    source::Weapon::Desc hyperDescription;
    hyperDescription.shotDelay = 0.3F;
    hyperDescription.projectiles.resize(1U);
    hyperDescription.projectiles.front().type = 1U;
    source::Weapon::Desc mineDescription;
    mineDescription.shotDelay = 0.4F;
    mineDescription.projectiles.resize(1U);
    mineDescription.projectiles.front().type = 11U;
    source::Weapon::Desc emptyDescription;
    auto& primaryWeapon0 = *rack.Add(
        emptyDescription, "Weapon\\primary0").GetWeapon();
    auto& primaryWeapon1 = *rack.Add(
        emptyDescription, "Weapon\\primary1").GetWeapon();
    auto& primaryWeapon2 = *rack.Add(
        primaryDescription, "Weapon\\primary2").GetWeapon();
    auto& hyperWeapon = *rack.Add(
        hyperDescription, "Weapon\\hyper").GetWeapon();
    auto& mineWeapon = *rack.Add(
        mineDescription, "Weapon\\mine").GetWeapon();
    rack.OnProgress(0.5F);
    if (!primaryWeapon2.IsReadyShot() ||
        !hyperWeapon.IsReadyShot() || !mineWeapon.IsReadyShot() ||
        rack.GetHyperDrive() != &hyperWeapon ||
        rack.GetMines() != &mineWeapon ||
        primaryWeapon2.GetParent() != &rackCar)
        return 7;
    primaryWeapon0.Reset();
    primaryWeapon1.Reset();
    primaryWeapon2.Reset();
    hyperWeapon.Reset();
    mineWeapon.Reset();
    if (primaryWeapon2.IsReadyShot() ||
        hyperWeapon.IsReadyShot() || mineWeapon.IsReadyShot())
        return 8;

    std::uint32_t charge = 2U;
    source::WeaponItem item(
        &primaryWeapon2, 7U, 4U, &charge, 2U, 12.5F, 100);
    primaryWeapon2.OnProgress(0.3F);
    if (item.IsInstalled() || item.IsReadyShot() ||
        item.GetWeapon() != nullptr || item.Shot(true) || charge != 2U)
        return 8;
    item.OnCreateCar();
    if (!item.IsInstalled() || !item.IsReadyShot() ||
        !item.HasShotCharge() || item.GetMaxCharge() != 7U ||
        item.GetCntCharge() != 4U || item.GetCurCharge() != 2U ||
        item.GetChargeStep() != 2U ||
        std::abs(item.GetDamage() - 12.5F) > 0.0001F ||
        item.GetChargeCost() != 100)
        return 8;
    if (item.Shot(false) || charge != 2U || !item.IsReadyShot())
        return 9;
    if (!item.Shot(true) || item.GetCurCharge() != 1U ||
        charge != 2U || item.IsReadyShot())
        return 10;
    item.Reload();
    if (item.GetCurCharge() != 4U || charge != 2U)
        return 11;
    item.OnDestroyCar();
    if (item.IsInstalled() || item.IsReadyShot() ||
        item.GetWeapon() != nullptr || item.Shot(true) || charge != 2U)
        return 11;
    item.SetMaxCharge(9U);
    item.SetCntCharge(5U);
    item.SetCurCharge(3U);
    item.SetChargeStep(3U);
    item.SetDamage(13.5F);
    item.SetChargeCost(125);
    if (item.GetMaxCharge() != 9U || item.GetCntCharge() != 5U ||
        item.GetCurCharge() != 3U || item.GetChargeStep() != 3U ||
        std::abs(item.GetDamage() - 12.5F) > 0.0001F ||
        item.GetChargeCost() != 125 || charge != 2U)
        return 11;

    source::Weapon::Desc replacementDescription;
    replacementDescription.shotDelay = 0.75F;
    auto replacementProjectile = itemProjectiles[0];
    replacementProjectile.type = 2U;
    replacementProjectile.speed = 60.0F;
    replacementProjectile.maximumDistance = 500.0F;
    replacementProjectile.damage = 8.0F;
    replacementDescription.projectiles.push_back(
        replacementProjectile);
    replacementProjectile.damage = 5.5F;
    replacementDescription.projectiles.push_back(
        replacementProjectile);
    item.SetWpnDesc(replacementDescription);
    if (item.GetWpnDesc().projectiles.size() != 2U ||
        item.GetDesc().Front().type != 2U ||
        std::abs(item.GetDamage(true) - 13.5F) > 0.0001F)
        return 11;
    item.AttachWeapon(&primaryWeapon2);
    item.OnCreateCar();
    if (primaryWeapon2.GetDesc().Front().type != 2U ||
        std::abs(primaryWeapon2.GetDesc().shotDelay - 0.75F) >
            0.0001F)
        return 11;
    item.OnDestroyCar();

    // maxCharge==0 is the original infinite-ammunition sentinel.  It must
    // still create a shot at currentCharge==0 and clamp the decrement to 0.
    charge = 0U;
    source::WeaponItem infinite(
        &mineWeapon, 0U, 0U, &charge);
    infinite.OnCreateCar();
    mineWeapon.OnProgress(1.0F);
    if (!infinite.HasShotCharge() || !infinite.Shot(true) || charge != 0U)
        return 12;

    // NetPlayer::DoShot supplies an explicit current-1 charge.  WeaponItem
    // applies it even if projectile preparation fails, exactly as Player.cpp.
    charge = 3U;
    source::WeaponItem replicated(
        &hyperWeapon, 7U, 3U, &charge);
    replicated.OnCreateCar();
    if (replicated.Shot(false, 1) ||
        replicated.GetCurCharge() != 1U || charge != 3U)
        return 13;

    primaryWeapon0.SetDesc(
        0.1F, std::span<const std::uint32_t>{});
    primaryWeapon1.SetDesc(
        0.1F, std::span<const std::uint32_t>{});
    primaryWeapon0.OnProgress(0.2F);
    std::uint32_t firstCharge = 1U;
    std::uint32_t secondCharge = 1U;
    std::array<source::WeaponItem, 2U> primary{
        source::WeaponItem(&primaryWeapon0, 7U, 1U, &firstCharge),
        source::WeaponItem(&primaryWeapon1, 7U, 1U, &secondCharge)};
    std::array<source::WeaponItem*, 2U> primaryItems{
        &primary[0], &primary[1]};
    for (auto& primaryItem : primary)
        primaryItem.OnCreateCar();
    const auto allPlan = source::Logic::ShotAll(primaryItems, true);
    if (!allPlan.humanShotEvent || allPlan.shotCount != 1U ||
        !allPlan.Get(source::Logic::SlotType::Weapon1) ||
        allPlan.Get(source::Logic::SlotType::Weapon2))
        return 14;
    const auto hyperPlan = source::Logic::Shot(
        &replicated, source::Logic::SlotType::Hyper, true);
    if (hyperPlan.humanShotEvent)
        return 15;
    const auto dryMinePlan = source::Logic::Shot(
        nullptr, source::Logic::SlotType::Mine, true);
    if (!dryMinePlan.humanShotEvent || dryMinePlan.shotCount != 0U)
        return 16;

    std::uint32_t droidCharge = 0U;
    source::DroidItem droid(
        &primaryWeapon0, 1U, 1U, &droidCharge, 17.0F, 1.0F);
    float life = 80.0F;
    droid.OnCreateCar();
    if (!droid.IsProgressRegistered() ||
        droid.GetRepairValue() != 17.0F ||
        droid.OnProgress(1.0F, life, 100.0F, false) != 0.0F ||
        life != 80.0F)
        return 17;
    const float healed = droid.OnProgress(
        0.001F, life, 100.0F, false);
    // The serialized repairValue is deliberately ignored by Windows
    // DroidItem::OnProgress, which calls Healt(5.0f).
    if (healed != 5.0F || life != 85.0F)
        return 18;
    droid.OnDestroyCar();
    if (droid.IsProgressRegistered() ||
        droid.OnProgress(2.0F, life, 100.0F, false) != 0.0F ||
        life != 85.0F)
        return 19;
    droid.AttachWeapon(&primaryWeapon0);
    droid.OnCreateCar();
    if (droid.GetRepairTime() != 0.0F)
        return 20;

    source::Player supportPlayer;
    supportPlayer.Reset(100.0F, 1U);
    r3d::game::originalrace::OriginalWorkshopItem firstDroidRecord;
    firstDroidRecord.record =
        "world\\race\\workshopRoot\\workshop\\droid1";
    firstDroidRecord.type = static_cast<std::uint32_t>(
        source::SlotType::Droid);
    auto secondDroidRecord = firstDroidRecord;
    secondDroidRecord.record =
        "world\\race\\workshopRoot\\workshop\\droid2";
    supportPlayer.BindSlots(
        {firstDroidRecord, secondDroidRecord},
        {{firstDroidRecord.record, "stWeapon1", 1U},
         {secondDroidRecord.record, "stWeapon2", 1U}});
    supportPlayer.weaponSlots[0] = 0U;
    supportPlayer.weaponSlots[1] = 1U;
    supportPlayer.weaponCapacity[0] = 1U;
    supportPlayer.weaponCapacity[1] = 1U;
    supportPlayer.weaponCharges[0] = 1U;
    supportPlayer.weaponCharges[1] = 1U;
    std::array<r3d::game::originalrace::WeaponDefinition, 2U>
        droidDefinitions{};
    for (auto& definition : droidDefinitions)
    {
        definition.itemType =
            r3d::game::originalrace::WeaponItemType::Droid;
        definition.maximumCharge = 1U;
        definition.reloadCharge = 1U;
        definition.repairPeriod = 1.0F;
    }
    supportPlayer.BindWeaponItems(droidDefinitions);
    supportPlayer.SetLife(80.0F);
    supportPlayer.CreateCar(true);
    auto* firstDroid = dynamic_cast<source::DroidItem*>(
        &supportPlayer
             .GetSlotInst(source::SlotType::Droid)
             ->GetItem());
    if (firstDroid == nullptr || !firstDroid->IsProgressRegistered())
        return 21;
    supportPlayer.ProgressBehaviors(1.001F, 0.35F, 0.0F);
    if (supportPlayer.GetLife() != 90.0F)
        return 22;
    supportPlayer.FreeCar(false);
    supportPlayer.ProgressBehaviors(2.0F, 0.35F, 0.0F);
    if (firstDroid->IsProgressRegistered() ||
        supportPlayer.GetLife() != 90.0F)
        return 23;

    source::Player damagePlayer;
    damagePlayer.Reset(100.0F, 1U);
    r3d::game::originalrace::OriginalWorkshopItem firstReflectorRecord;
    firstReflectorRecord.record =
        "world\\race\\workshopRoot\\workshop\\reflector1";
    firstReflectorRecord.type = static_cast<std::uint32_t>(
        source::SlotType::Reflector);
    auto secondReflectorRecord = firstReflectorRecord;
    secondReflectorRecord.record =
        "world\\race\\workshopRoot\\workshop\\reflector2";
    damagePlayer.BindSlots(
        {firstReflectorRecord, secondReflectorRecord},
        {{firstReflectorRecord.record, "stWeapon1", 1U},
         {secondReflectorRecord.record, "stWeapon3", 1U}});
    damagePlayer.weaponSlots[0] = 0U;
    damagePlayer.weaponSlots[2] = 1U;
    damagePlayer.weaponCapacity[0] = 1U;
    damagePlayer.weaponCapacity[2] = 1U;
    damagePlayer.weaponCharges[0] = 1U;
    damagePlayer.weaponCharges[2] = 1U;
    std::array<r3d::game::originalrace::WeaponDefinition, 2U>
        reflectorDefinitions{};
    for (auto& definition : reflectorDefinitions)
    {
        definition.itemType =
            r3d::game::originalrace::WeaponItemType::Reflector;
        definition.maximumCharge = 1U;
        definition.reloadCharge = 1U;
    }
    reflectorDefinitions[0].reflectValue = 0.4F;
    reflectorDefinitions[1].reflectValue = 0.9F;
    damagePlayer.BindWeaponItems(reflectorDefinitions);
    if (std::abs(source::Logic::ResolveDamage(
                     &damagePlayer, 100.0F,
                     r3d::game::originalrace::DamageType::Simple) -
                 60.0F) > 0.001F ||
        source::Logic::ResolveDamage(
            &damagePlayer, 100.0F,
            r3d::game::originalrace::DamageType::Touch) != 100.0F)
        return 24;
    source::GameObject damageTarget;
    damageTarget.ResetGameObject(100.0F);
    const auto resolved = source::Logic::ResolveDamage(
        &damagePlayer, 100.0F,
        r3d::game::originalrace::DamageType::Simple);
    const auto damageResult = source::Logic::Damage(
        damageTarget, 3U, resolved,
        r3d::game::originalrace::DamageType::Simple);
    if (damageResult.death ||
        std::abs(damageTarget.GetLife() - 40.0F) > 0.001F)
        return 25;
    const auto authoritative = source::Logic::Damage(
        damageTarget, 3U, 5.0F, -2.0F, true,
        r3d::game::originalrace::DamageType::Energy);
    if (!authoritative.death || authoritative.killCredit ||
        damageTarget.GetLife() != -2.0F)
        return 26;

    source::Player bonusPlayer;
    bonusPlayer.Reset(100.0F, 1U);
    source::GameObject bonusObject;
    bonusObject.ResetGameObject(-1.0F);
    BonusDeathOrder bonusDeathOrder;
    bonusDeathOrder.player = &bonusPlayer;
    bonusObject.InsertListener(&bonusDeathOrder);
    const auto moneyBonus = source::Logic::TakeBonus(
        &bonusPlayer, &bonusObject,
        source::PlayerBonusType::Money, 25.0F, {}, 0.0F);
    if (!moneyBonus.taken || !bonusObject.destroyed ||
        bonusPlayer.GetPickMoney() != 25U ||
        bonusDeathOrder.moneyAtDeath != 0U)
        return 27;
    if (source::Logic::TakeBonus(
            &bonusPlayer, &bonusObject,
            source::PlayerBonusType::Money, 25.0F, {}, 0.0F).taken)
        return 28;

    const auto speedArrow = source::Proj::SpeedArrowContact(
        {3.0F, 4.0F, 0.0F}, 10.0F);
    if (!speedArrow.setLinearVelocity ||
        !speedArrow.sendSpeedArrowEvent ||
        std::abs(speedArrow.linearVelocity.x - 6.0F) > 0.001F ||
        std::abs(speedArrow.linearVelocity.y - 8.0F) > 0.001F)
        return 29;
    const auto lusha = source::Proj::LushaContact(
        {30.0F, 0.0F, 0.0F}, 20.0F);
    if (!lusha.setLinearVelocity ||
        std::abs(lusha.linearVelocity.x - 20.0F) > 0.001F ||
        source::Proj::LushaContact(
            {15.0F, 0.0F, 0.0F}, 20.0F).setLinearVelocity)
        return 30;
    source::GameObject velocityContactTarget;
    velocityContactTarget.ResetGameObject(100.0F);
    auto speedArrowDescription = sourceDescription;
    speedArrowDescription.type = 8U;
    speedArrowDescription.damage = 10.0F;
    source::Proj speedArrowProjectile;
    source::Proj::ShotContext velocityContactContext;
    velocityContactContext.rotation =
        {0.0F, 0.0F, 0.70710677F, 0.70710677F};
    speedArrowProjectile.PrepareSource(
        speedArrowDescription, nullptr, velocityContactContext);
    const auto concreteSpeedArrow =
        speedArrowProjectile.ContactSpeedArrow(&velocityContactTarget);
    if (!concreteSpeedArrow.setLinearVelocity ||
        !concreteSpeedArrow.sendSpeedArrowEvent ||
        std::abs(concreteSpeedArrow.linearVelocity.x) > 0.001F ||
        std::abs(concreteSpeedArrow.linearVelocity.y - 10.0F) > 0.001F ||
        speedArrowProjectile.ContactSpeedArrow(nullptr).setLinearVelocity)
        return 140;
    auto lushaDescription = sourceDescription;
    lushaDescription.type = 9U;
    lushaDescription.damage = 20.0F;
    source::Proj lushaProjectile;
    lushaProjectile.PrepareSource(
        lushaDescription, nullptr, velocityContactContext);
    if (!lushaProjectile.ContactLusha(
            &velocityContactTarget,
            {30.0F, 0.0F, 0.0F}).setLinearVelocity ||
        lushaProjectile.ContactLusha(
            &velocityContactTarget,
            {15.0F, 0.0F, 0.0F}).setLinearVelocity ||
        lushaProjectile.ContactLusha(
            nullptr, {30.0F, 0.0F, 0.0F}).setLinearVelocity)
        return 141;
    const auto oilRight = source::Proj::MasloContact(
        {}, {0.0F, 1.0F, 0.0F}, {0.0F, 1.0F, 0.0F},
        {4.0F, 0.0F, 0.0F}, 1.5F,
        false, false, false, false);
    const auto oilLeft = source::Proj::MasloContact(
        {}, {0.0F, 1.0F, 0.0F}, {0.0F, -1.0F, 0.0F},
        {4.0F, 0.0F, 0.0F}, 1.5F,
        false, false, false, false);
    if (!oilRight.lockClutch || oilRight.clutchStrength != -1.5F ||
        !oilLeft.lockClutch || oilLeft.clutchStrength != 1.5F)
        return 31;
    if (source::Proj::MasloContact(
            {}, {0.0F, 1.0F, 0.0F}, {},
            {4.0F, 0.0F, 0.0F}, 1.5F,
            true, false, false, false).lockClutch ||
        source::Proj::MasloContact(
            {}, {0.0F, 1.0F, 0.0F}, {},
            {3.0F, 0.0F, 0.0F}, 1.5F,
            false, false, false, false).lockClutch ||
        source::Proj::MasloContact(
            {}, {0.0F, 1.0F, 0.0F}, {},
            {4.0F, 0.0F, 0.0F}, 1.5F,
            false, true, false, false).lockClutch ||
        source::Proj::MasloContact(
            {}, {0.0F, 1.0F, 0.0F}, {},
            {4.0F, 0.0F, 0.0F}, 1.5F,
            false, false, true, false).lockClutch ||
        source::Proj::MasloContact(
            {}, {0.0F, 1.0F, 0.0F}, {},
            {4.0F, 0.0F, 0.0F}, 1.5F,
            false, false, false, true).lockClutch)
        return 32;

    source::AutoProj autoOil;
    r3d::game::originalrace::ProjectileDefinition autoOilDescription;
    autoOilDescription.type = 10U;
    autoOilDescription.damage = 1.5F;
    autoOilDescription.visual.record = "Bonus\\maslo";
    autoOil.Reset(autoOilDescription);
    autoOil.SetWorldPos({4.0F, 5.0F, 6.0F});
    autoOil.SetMaxLife(12.0F);
    autoOil.SetLife(9.0F);
    autoOil.SetMaxTimeLife(8.0F);
    autoOil.SetTimeLife(2.0F);
    if (autoOil.IsPrepared())
        return 62;
    source::Logic autoProjectileLogic;
    autoOil.SetLogic(&autoProjectileLogic);
    if (!autoOil.IsPrepared() || !autoOil.IsArming() ||
        autoOil.GetModelScale() != 0.0F ||
        autoOil.GetDesc().type != 10U ||
        autoOil.GetDesc().damage != 1.5F ||
        autoOil.GetSourceModel() == nullptr ||
        autoOil.GetSourceModel()->GetParent() != &autoOil ||
        autoOil.GetIncludeList().GetLiveCount() != 1U ||
        autoOil.GetWorldPos() !=
            source::GameObject::Vector3{4.0F, 5.0F, 6.0F} ||
        autoOil.GetMaxLife() != 12.0F || autoOil.GetLife() != 9.0F ||
        autoOil.GetMaxTimeLife() != 8.0F ||
        autoOil.GetTimeLife() != 2.0F ||
        autoOil.IsExternalLifetimeManaged())
        return 63;
    autoOil.OnProgress(0.125F);
    if (!autoOil.IsArming() ||
        std::abs(autoOil.GetModelScale() - 0.5F) > 0.001F ||
        autoOil.GetSourceModel()->GetGameObj().GetScale() !=
            source::GameObject::Vector3{0.5F, 0.5F, 0.5F})
        return 64;
    autoOil.OnProgress(0.125F);
    if (autoOil.IsArming() ||
        std::abs(autoOil.GetModelScale() - 1.0F) > 0.001F)
        return 65;
    autoOil.SetLogic(nullptr);
    if (autoOil.IsPrepared() || autoOil.GetLogic() != nullptr)
        return 66;

    source::AutoProj autoMinePiece;
    autoMinePiece.Reset(13U);
    autoMinePiece.SetLogic(&autoProjectileLogic);
    autoMinePiece.OnProgress(1.0F);
    if (!autoMinePiece.IsPrepared() || autoMinePiece.IsArming() ||
        autoMinePiece.GetModelScale() >= 0.0F)
        return 67;

    auto* timedObject = new source::GameObject();
    timedObject->ResetGameObject(-1.0F);
    timedObject->SetMaxTimeLife(0.25F);
    autoProjectileLogic.RegGameObj(timedObject);
    autoProjectileLogic.RegGameObj(timedObject);
    if (autoProjectileLogic.GetGameObjCount() != 1U ||
        timedObject->GetLogic() != &autoProjectileLogic)
        return 94;
    const auto exactLifetime =
        autoProjectileLogic.ProgressGameObjs(0.25F);
    if (exactLifetime.progressed != 1U || exactLifetime.removed != 0U ||
        autoProjectileLogic.GetGameObjCount() != 1U)
        return 95;
    const auto expiredLifetime =
        autoProjectileLogic.ProgressGameObjs(0.001F);
    if (expiredLifetime.progressed != 1U ||
        expiredLifetime.removed != 1U ||
        autoProjectileLogic.GetGameObjCount() != 0U)
        return 96;
    auto* logicProjectile = new source::Proj();
    r3d::game::originalrace::ProjectileDefinition logicDescription;
    logicDescription.type = 17U;
    source::Proj::ShotContext logicContext;
    logicContext.logic = &autoProjectileLogic;
    logicContext.maximumLife = 0.01F;
    logicProjectile->PrepareSource(
        logicDescription, nullptr, logicContext);
    autoProjectileLogic.RegGameObj(logicProjectile);
    const auto managedLifetime =
        autoProjectileLogic.ProgressGameObjs(0.02F);
    if (managedLifetime.progressed != 1U ||
        managedLifetime.removed != 0U ||
        !autoProjectileLogic.HasGameObj(logicProjectile) ||
        logicProjectile->GetLiveState() !=
            source::GameObject::LiveState::Live ||
        logicProjectile->GetTimeLife() != 0.02F)
        return 102;
    logicProjectile->Death();
    const auto releasedProjectile =
        autoProjectileLogic.ProgressGameObjs(0.0F);
    if (releasedProjectile.progressed != 1U ||
        releasedProjectile.removed != 1U ||
        autoProjectileLogic.HasGameObj(logicProjectile))
        return 103;

    source::Weapon factoryWeapon;
    factoryWeapon.OnProgress(1.0F);
    source::GameObject factoryTarget;
    r3d::game::originalrace::ProjectileDefinition factoryDescription;
    factoryDescription.type = 17U;
    factoryDescription.position = {1.0F, 2.0F, 3.0F};
    source::Proj::ShotContext factoryContext;
    factoryContext.logic = &autoProjectileLogic;
    factoryContext.shot.targetMapObject = &factoryTarget;
    factoryContext.shot.target = {9.0F, 8.0F, 7.0F};
    factoryContext.playerId = 4U;
    factoryContext.maximumLife = 3.0F;
    factoryContext.position = {6.0F, 5.0F, 4.0F};
    auto* factoryProjectile = source::Weapon::CreateShot(
        &factoryWeapon, factoryDescription, factoryContext);
    if (factoryProjectile == nullptr ||
        !autoProjectileLogic.HasGameObj(factoryProjectile) ||
        autoProjectileLogic.GetGameObjCount() != 1U ||
        factoryProjectile->GetLogic() != &autoProjectileLogic ||
        factoryProjectile->GetSourceTarget() != &factoryTarget ||
        factoryProjectile->GetShot().targetMapObject != &factoryTarget ||
        factoryProjectile->GetShot().target !=
            source::Proj::Vec3{9.0F, 8.0F, 7.0F} ||
        factoryProjectile->GetSourcePlayerId() != 4U ||
        factoryProjectile->GetWorldPos() !=
            source::GameObject::Vector3{6.0F, 5.0F, 4.0F} ||
        factoryWeapon.GetShotTime() != 0.0F ||
        factoryWeapon.GetShotEffect().GetShotCount() != 1U ||
        factoryWeapon.GetLastShotPosition() !=
            std::array<float, 3U>{1.0F, 2.0F, 3.0F})
        return 104;
    source::Proj::ShotContext invalidFactoryContext;
    if (source::Weapon::CreateShot(
            &factoryWeapon, factoryDescription,
            invalidFactoryContext) != nullptr ||
        autoProjectileLogic.GetGameObjCount() != 1U)
        return 105;
    factoryProjectile->Death();
    const auto releasedFactoryProjectile =
        autoProjectileLogic.ProgressGameObjs(0.0F);
    if (releasedFactoryProjectile.removed != 1U ||
        autoProjectileLogic.GetGameObjCount() != 0U ||
        factoryTarget.GetListenerCount() != 0U)
        return 106;
    source::Proj::ShotContext rejectedFactoryContext = factoryContext;
    rejectedFactoryContext.preparationAccepted = false;
    factoryWeapon.OnProgress(0.5F);
    const float rejectedShotTime = factoryWeapon.GetShotTime();
    const auto rejectedShotEffects =
        factoryWeapon.GetShotEffect().GetShotCount();
    if (source::Weapon::CreateShot(
            &factoryWeapon, factoryDescription,
            rejectedFactoryContext) != nullptr ||
        autoProjectileLogic.GetGameObjCount() != 0U ||
        factoryWeapon.GetShotTime() != rejectedShotTime ||
        factoryWeapon.GetShotEffect().GetShotCount() !=
            rejectedShotEffects)
        return 107;

    source::Weapon::Desc batchDescription;
    batchDescription.shotDelay = 0.2F;
    batchDescription.projectiles.resize(2U);
    batchDescription.projectiles[0].type = 0U;
    batchDescription.projectiles[0].position = {1.0F, 0.0F, 0.0F};
    batchDescription.projectiles[0].speed = 10.0F;
    batchDescription.projectiles[0].maximumDistance = 20.0F;
    batchDescription.projectiles[1] =
        batchDescription.projectiles[0];
    batchDescription.projectiles[1].type = 2U;
    source::Weapon batchWeapon(batchDescription);
    batchWeapon.SetLogic(&autoProjectileLogic);
    batchWeapon.SetWorldPos({5.0F, 6.0F, 7.0F});
    batchWeapon.OnProgress(1.0F);
    source::Weapon::ProjList batchProjectiles;
    if (!batchWeapon.Shot(
            source::Proj::Vec3{9.0F, 8.0F, 7.0F},
            &batchProjectiles) ||
        batchProjectiles.size() != 2U ||
        autoProjectileLogic.GetGameObjCount() != 2U ||
        batchWeapon.GetShotEffect().GetShotCount() != 2U ||
        batchWeapon.GetShotTime() != 0.0F ||
        batchProjectiles[0]->GetShot().target !=
            source::Proj::Vec3{9.0F, 8.0F, 7.0F} ||
        batchProjectiles[0]->GetWorldPos() !=
            source::GameObject::Vector3{6.0F, 6.0F, 7.0F})
        return 124;
    for (auto* projectile : batchProjectiles)
        projectile->Death();
    if (autoProjectileLogic.ProgressGameObjs(0.0F).removed != 2U)
        return 125;

    std::array<source::Proj::ShotContext, 2U> partialContexts{};
    for (auto& context : partialContexts)
        context.logic = &autoProjectileLogic;
    partialContexts[0].preparationAccepted = false;
    source::Weapon::ProjList partialProjectiles;
    const auto effectsBeforePartial =
        batchWeapon.GetShotEffect().GetShotCount();
    if (!source::Weapon::CreateShot(
            &batchWeapon, batchDescription, partialContexts,
            &partialProjectiles) ||
        partialProjectiles.size() != 1U ||
        batchWeapon.GetShotEffect().GetShotCount() !=
            effectsBeforePartial + 1U)
        return 126;
    partialProjectiles.front()->Death();
    autoProjectileLogic.ProgressGameObjs(0.0F);

    source::Weapon::Desc autonomousDescription;
    autonomousDescription.projectiles.resize(2U);
    autonomousDescription.projectiles[0].type = 0U;
    autonomousDescription.projectiles[1].type = 10U;
    std::array<source::Proj::ShotContext, 2U> autonomousContexts{};
    for (auto& context : autonomousContexts)
        context.logic = &autoProjectileLogic;
    source::Weapon::ProjList autonomousProjectiles;
    if (!source::Weapon::CreateShot(
            nullptr, autonomousDescription, autonomousContexts,
            &autonomousProjectiles) ||
        autonomousProjectiles.size() != 1U ||
        autonomousProjectiles.front()->GetDesc().type != 10U)
        return 127;
    autonomousProjectiles.front()->Death();
    autoProjectileLogic.ProgressGameObjs(0.0F);
    batchWeapon.SetLogic(nullptr);

    r3d::game::originalrace::ProjectileDefinition boundsDescription;
    boundsDescription.size = {2.0F, 4.0F, 6.0F};
    boundsDescription.offset = {1.0F, -1.0F, 2.0F};
    boundsDescription.modelSize = true;
    boundsDescription.modelBoundsValid = true;
    boundsDescription.modelBounds.center = {2.0F, 0.0F, -2.0F};
    boundsDescription.modelBounds.halfExtents = {1.0F, 2.0F, 1.0F};
    const auto contactBounds =
        source::Proj::ComputeAABB(boundsDescription, false);
    const auto placementBounds =
        source::Proj::ComputeAABB(boundsDescription, true);
    boundsDescription.modelSize = false;
    const auto fallbackModelBounds =
        source::Proj::ComputeAABB(boundsDescription, true);
    const auto nearVector = [](const auto& value, float x, float y,
                               float z) {
        return std::abs(value.x - x) < 0.0001F &&
               std::abs(value.y - y) < 0.0001F &&
               std::abs(value.z - z) < 0.0001F;
    };
    if (!nearVector(contactBounds.center, 1.5F, -0.5F, 1.0F) ||
        !nearVector(contactBounds.halfExtents, 1.5F, 2.5F, 4.0F) ||
        !nearVector(placementBounds.center, 1.5F, 0.0F, -1.5F) ||
        !nearVector(placementBounds.halfExtents, 1.5F, 2.0F, 1.5F) ||
        !nearVector(fallbackModelBounds.center, 0.0F, 0.0F, 0.0F) ||
        !nearVector(
            fallbackModelBounds.halfExtents, 0.05F, 0.05F, 0.05F))
        return 108;
    autoProjectileLogic.RegGameObj(new source::GameObject());
    autoProjectileLogic.CleanGameObjs();
    if (autoProjectileLogic.GetGameObjCount() != 0U)
        return 97;

    const auto firstRocketHeight = source::Proj::RocketUpdate(
        10.0F, 3.0F, 2.0F, 0.0F, true);
    const auto lowerRocketHeight = source::Proj::RocketUpdate(
        6.0F, 3.0F, 2.0F, firstRocketHeight.clearance, true);
    const auto stableRocketHeight = source::Proj::RocketUpdate(
        5.95F, 3.0F, 2.0F, lowerRocketHeight.clearance, true);
    if (firstRocketHeight.clearance != 7.0F ||
        firstRocketHeight.positionZ != 10.0F ||
        lowerRocketHeight.clearance != 3.0F ||
        lowerRocketHeight.positionZ != 6.0F ||
        stableRocketHeight.clearance != 3.0F ||
        stableRocketHeight.positionZ != 6.0F)
        return 42;
    const auto missedTrack = source::Proj::RocketUpdate(
        9.0F, 0.0F, 2.0F, 3.0F, false);
    if (missedTrack.positionZ != 9.0F ||
        missedTrack.clearance != 3.0F)
        return 43;

    auto rocketDescription = sourceDescription;
    rocketDescription.type = 0U;
    rocketDescription.secondaryVisual = {};
    source::Proj rocketProjectile;
    rocketProjectile.PrepareSource(
        rocketDescription, nullptr, source::Proj::ShotContext{});
    const auto concreteRocket =
        rocketProjectile.ProgressRocket(10.0F, 3.0F, 2.0F, true);
    if (concreteRocket.clearance != 7.0F ||
        rocketProjectile.GetSourceVector().z != 7.0F)
        return 115;

    const auto waitingTorpeda = source::Proj::TorpedaUpdate(
        0.1F, {}, {}, {12.0F, 0.0F, 0.0F}, 0.4F,
        true, {0.0F, 10.0F, 0.0F}, 10.0F, true, 4.0F);
    if (waitingTorpeda.setLinearVelocity ||
        std::abs(waitingTorpeda.homingDelay - 0.3F) > 0.001F)
        return 44;
    const auto aimedTorpeda = source::Proj::TorpedaUpdate(
        0.1F, {}, {}, {10.0F, 0.0F, 0.0F}, 0.0F,
        true, {0.0F, 10.0F, 0.0F}, 20.0F, false, 0.0F);
    if (!aimedTorpeda.setLinearVelocity ||
        std::abs(aimedTorpeda.direction.y - 1.0F) > 0.001F ||
        std::abs(aimedTorpeda.linearVelocity.y - 20.0F) > 0.001F)
        return 45;
    const auto nearTorpeda = source::Proj::TorpedaUpdate(
        0.1F, {}, {}, {12.0F, 0.0F, 0.0F}, 0.0F,
        true, {}, 10.0F, true, 4.0F);
    if (!nearTorpeda.setLinearVelocity ||
        std::abs(nearTorpeda.direction.x - 1.0F) > 0.001F ||
        std::abs(nearTorpeda.linearVelocity.x - 12.0F) > 0.001F)
        return 46;

    const auto concreteTorpeda = torpedaProjectile.ProgressTorpeda(
        0.1F, {}, {}, true, {0.0F, 10.0F, 0.0F},
        20.0F, false, 0.0F);
    if (concreteTorpeda.setLinearVelocity ||
        std::abs(torpedaProjectile.GetSourceTimer() - 0.3F) > 0.001F ||
        torpedaProjectile.GetSourceVector() !=
            source::Proj::Vec3{3.0F, 4.0F, 5.0F})
        return 116;

    if (std::abs(source::Proj::ThunderUpdate(0.1F, 0.16F) + 0.06F) >
        0.001F)
        return 47;
    const auto thunderReflection = source::Proj::ThunderContact(
        {10.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
        0.0F, true);
    const auto thunderReverse = source::Proj::ThunderContact(
        {0.0F, 10.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
        0.0F, true);
    if (!thunderReflection.setLinearVelocity ||
        thunderReflection.linearVelocity.x != -10.0F ||
        thunderReflection.reflectionCooldown != 0.1F ||
        !thunderReverse.setLinearVelocity ||
        thunderReverse.linearVelocity.y != -10.0F ||
        source::Proj::ThunderContact(
            {4.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
            0.0F, true).setLinearVelocity)
        return 48;

    auto thunderDescription = sourceDescription;
    thunderDescription.type = 22U;
    thunderDescription.secondaryVisual = {};
    source::Proj thunderProjectile;
    thunderProjectile.PrepareSource(
        thunderDescription, nullptr, source::Proj::ShotContext{});
    if (std::abs(thunderProjectile.ProgressThunder(0.16F) + 0.16F) >
            0.001F ||
        !thunderProjectile.ContactThunder(
            {10.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, true)
             .setLinearVelocity ||
        std::abs(thunderProjectile.GetSourceTimer() - 0.1F) > 0.001F)
        return 117;

    const auto resonance = source::Proj::ResonanseUpdate(
        {}, 3.14159265358979323846F, 0.5F);
    if (std::abs(resonance.x - 0.70710678F) > 0.001F ||
        std::abs(resonance.w - 0.70710678F) > 0.001F)
        return 49;
    const auto rocketTorque = source::Proj::RocketContactTorque(
        {0.0F, 1.0F, 0.0F}, {10.0F, 0.0F, 0.0F}, 10.0F);
    if (!rocketTorque.apply ||
        std::abs(rocketTorque.localVelocityChange.z + 2.0F) > 0.001F ||
        source::Proj::RocketContactTorque(
            {0.0F, 1.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
            10.0F).apply)
        return 50;

    const auto relativeLaunch = source::Proj::CalcSpeed(
        {1.0F, 0.0F, 0.5F}, {5.0F, 0.0F, 0.0F},
        10.0F, 13.0F, true);
    const auto minimumLaunch = source::Proj::CalcSpeed(
        {1.0F, 0.0F, 0.5F}, {5.0F, 0.0F, 0.0F},
        10.0F, 13.0F, false);
    if (std::abs(relativeLaunch.direction.z) > 0.001F ||
        std::abs(relativeLaunch.speed - 14.472136F) > 0.001F ||
        std::abs(minimumLaunch.speed - 17.472136F) > 0.001F)
        return 51;

    const auto armingMine = source::Proj::MineUpdate(
        0.0F, 0.1F);
    const auto armedMine = source::Proj::MineUpdate(
        armingMine.timer, 0.15F);
    if (armingMine.armed ||
        std::abs(armingMine.visualScale - 0.4F) > 0.001F ||
        !armedMine.armed || armedMine.timer != -1.0F ||
        armedMine.visualScale != 1.0F)
        return 52;
    const auto concreteOil = oilProjectile.ProgressMine(0.1F);
    if (std::abs(concreteOil.visualScale - 0.4F) > 0.001F ||
        std::abs(oilProjectile.GetSourceTimer() - 0.1F) > 0.001F ||
        oilProjectile.GetSourceModel() == nullptr ||
        oilProjectile.GetSourceModel()->GetGameObj().GetScale() !=
            source::GameObject::Vector3{0.4F, 0.4F, 0.4F})
        return 118;
    if (source::Proj::MineContactAllowed(
            true, true, true, true, -1.0F, false) ||
        source::Proj::MineContactAllowed(
            true, false, false, false, 0.1F, true) ||
        !source::Proj::MineContactAllowed(
            true, false, false, false, 0.1F, false) ||
        !source::Proj::MineContactAllowed(
            true, false, false, false, -1.0F, true))
        return 53;
    source::GameCar contactCar;
    contactCar.BindWheels({false}, {false});
    contactCar.SetWheelContact(0U, true, 0.0F, 0.0F);
    contactCar.UpdateContactState(false);
    source::Weapon contactWeapon;
    contactWeapon.SetParent(&contactCar);
    const auto concreteSpring = source::Proj::SpringPrepare(
        &contactWeapon, 6.0F);
    if (!concreteSpring.prepared || !concreteSpring.lockSpring ||
        concreteSpring.localVelocityChange.z != 6.0F ||
        !contactCar.IsSpringLocked())
        return 134;
    source::GameCar mineTargetCar;
    source::GameObject nonCarTarget;
    oilProjectile.SetSourceTimer(-1.0F);
    if (!oilProjectile.ContactMine(
            &mineTargetCar, true, true) ||
        oilProjectile.ContactMine(
            &nonCarTarget, false, false))
        return 135;
    mineTargetCar.LockMine(0.4F);
    if (oilProjectile.ContactMine(
            &mineTargetCar, true, true))
        return 136;
    source::GameCar oilTargetCar;
    oilTargetCar.BindWheels({false}, {false});
    oilProjectile.SetSourceTimer(-1.0F);
    const auto concreteMaslo = oilProjectile.ContactMaslo(
        oilTargetCar.GetWheel(0U), {}, {0.0F, 1.0F, 0.0F},
        {0.0F, 1.0F, 0.0F}, {4.0F, 0.0F, 0.0F}, 1.5F);
    if (source::Proj::ResolveContactCar(
            oilTargetCar.GetWheel(0U)) != &oilTargetCar ||
        !concreteMaslo.lockClutch ||
        concreteMaslo.clutchStrength != -1.5F ||
        !oilTargetCar.IsClutchLocked())
        return 137;
    if (source::Proj::MineRipUpdate(2.0F, 2.0F, false) ||
        !source::Proj::MineRipUpdate(2.001F, 2.0F, false) ||
        source::Proj::MineRipUpdate(3.0F, 2.0F, true))
        return 54;

    const auto firstImpulse = source::Proj::ImpulseContact(
        true, true, true, 0U, 12.0F);
    const auto thirdImpulse = source::Proj::ImpulseContact(
        true, true, true, 2U, 12.0F);
    const auto untargetedImpulse = source::Proj::ImpulseContact(
        true, false, false, 0U, 12.0F);
    if (!firstImpulse.applyDamage ||
        firstImpulse.damage != 12.0F ||
        firstImpulse.hitCount != 1U ||
        !firstImpulse.findNextTarget || firstImpulse.destroy ||
        thirdImpulse.damage != 4.0F ||
        thirdImpulse.hitCount != 3U || !thirdImpulse.destroy ||
        !untargetedImpulse.applyDamage ||
        untargetedImpulse.damage != 12.0F ||
        !untargetedImpulse.destroy ||
        source::Proj::ImpulseContact(
            true, true, false, 0U, 12.0F).applyDamage)
        return 55;

    auto impulseDescription = sourceDescription;
    impulseDescription.type = 21U;
    impulseDescription.secondaryVisual = {};
    source::GameObject firstImpulseTarget;
    source::GameObject secondImpulseTarget;
    source::Proj::ShotContext impulseContext;
    impulseContext.shot.targetMapObject = &firstImpulseTarget;
    source::Proj impulseProjectile;
    impulseProjectile.PrepareSource(
        impulseDescription, nullptr, impulseContext);
    const auto concreteFirstImpulse =
        impulseProjectile.ContactImpulse(true, true, true, 12.0F);
    impulseProjectile.RetargetImpulse(&secondImpulseTarget);
    const auto concreteSecondImpulse =
        impulseProjectile.ContactImpulse(true, true, true, 12.0F);
    if (concreteFirstImpulse.hitCount != 1U ||
        concreteFirstImpulse.damage != 12.0F ||
        concreteSecondImpulse.hitCount != 2U ||
        concreteSecondImpulse.damage != 6.0F ||
        impulseProjectile.GetSourceTick() != 2U ||
        impulseProjectile.GetSourceTarget() != &secondImpulseTarget ||
        impulseProjectile.GetSourceTimer() != 0.0F)
        return 119;

    if (source::Proj::PrepareMaximumLife(
            10.0F, 100.0F, 12.0F) != 12.0F ||
        source::Proj::PrepareMaximumLife(
            10.0F, 100.0F, 2.0F) != 10.0F ||
        source::Proj::PrepareMaximumLife(
            0.0F, 100.0F, 0.0F) != 0.0F)
        return 56;
    const auto laser = source::Proj::LaserUpdate(
        100.0F, true, 80.0F, 0.5F, 10.0F,
        true, 2.0F, 4.0F);
    const auto maximumRangeLaser = source::Proj::LaserUpdate(
        100.0F, true, 100.0F, 0.5F, 10.0F,
        false, 2.0F, 4.0F);
    if (laser.distance != 80.0F || laser.damage != 5.0F ||
        !laser.applyDamage || laser.beamWidthScale != 2.0F ||
        laser.textureScale != 8.0F ||
        maximumRangeLaser.applyDamage)
        return 57;
    const auto fireContact = source::Proj::FireContact(
        true, 8.0F, 0.25F);
    const auto drobilkaContact = source::Proj::DrobilkaContact(
        true, 12.0F, 0.25F);
    const auto sonarContact = source::Proj::SonarContact(
        true, {3.0F, 0.0F, 0.0F}, 2.0F,
        8.0F, 0.25F);
    if (fireContact.damage != 2.0F ||
        drobilkaContact.damage != 3.0F ||
        sonarContact.damage != 2.0F ||
        !sonarContact.applyImpulse ||
        sonarContact.impulse.x != 6.0F ||
        source::Proj::SonarContact(
            false, {3.0F, 0.0F, 0.0F}, 2.0F,
            8.0F, 0.25F).applyImpulse)
        return 58;
    auto fireDescription = sourceDescription;
    fireDescription.type = 14U;
    fireDescription.damage = 8.0F;
    source::Proj fireProjectile;
    fireProjectile.PrepareSource(
        fireDescription, &sourceWeapon, linkedContext);
    const auto concreteFire = fireProjectile.ContactFire(
        &velocityContactTarget, 0.25F);
    if (concreteFire.damage != 2.0F ||
        fireProjectile.ContactFire(nullptr, 0.25F).damage != 0.0F)
        return 142;
    auto sonarDescription = sourceDescription;
    sonarDescription.type = 16U;
    sonarDescription.damage = 8.0F;
    sonarDescription.mass = 2.0F;
    source::Proj sonarProjectile;
    sonarProjectile.PrepareSource(
        sonarDescription, &sourceWeapon, linkedContext);
    const auto concreteSonar = sonarProjectile.ContactSonar(
        &velocityContactTarget, {3.0F, 0.0F, 0.0F}, 0.25F);
    if (concreteSonar.damage != 2.0F ||
        !concreteSonar.applyImpulse ||
        concreteSonar.impulse.x != 6.0F ||
        sonarProjectile.ContactSonar(
            nullptr, {3.0F, 0.0F, 0.0F}, 0.25F).applyImpulse)
        return 143;
    const auto springPrepared = source::Proj::SpringPrepare(
        true, true, 17.0F);
    if (!springPrepared.prepared || !springPrepared.lockSpring ||
        springPrepared.localVelocityChange.z != 17.0F ||
        source::Proj::SpringPrepare(
            true, false, 17.0F).prepared)
        return 59;
    const auto rocketRules = source::Proj::GetTypeRules(0U);
    const auto laserRules = source::Proj::GetTypeRules(3U);
    const auto fireRules = source::Proj::GetTypeRules(14U);
    const auto mortarRules = source::Proj::GetTypeRules(19U);
    if (!rocketRules.rocketPrepare || rocketRules.ray ||
        !laserRules.attached || !laserRules.linkedToWeapon ||
        !laserRules.ray ||
        !fireRules.rocketPrepare || !fireRules.attached ||
        fireRules.linkedToWeapon ||
        !mortarRules.rocketPrepare || !mortarRules.ballistic ||
        !source::Proj::GetTypeRules(1U).linkedToWeapon ||
        !source::Proj::GetTypeRules(17U).linkedToWeapon ||
        !source::Proj::GetTypeRules(11U).mineTestsLock ||
        source::Proj::GetTypeRules(24U).mineTestsLock)
        return 60;
    bool allPreparationHandlers = true;
    for (std::uint32_t type = 0U; type <= 24U; ++type)
    {
        const auto route = source::Proj::PreparationRouteFor(type);
        allPreparationHandlers =
            allPreparationHandlers && route.valid &&
            static_cast<std::uint32_t>(route.handler) == type + 1U;
    }
    const auto laserPreparation =
        source::Proj::PreparationRouteFor(3U);
    const auto minePreparation =
        source::Proj::PreparationRouteFor(11U);
    const auto drobilkaPreparation =
        source::Proj::PreparationRouteFor(15U);
    const auto springPreparationRoute =
        source::Proj::PreparationRouteFor(17U);
    const auto craterPreparation =
        source::Proj::PreparationRouteFor(20U);
    const auto protonPreparation =
        source::Proj::PreparationRouteFor(24U);
    if (!allPreparationHandlers ||
        source::Proj::PreparationRouteFor(25U).valid ||
        !laserPreparation.initializeModel ||
        !laserPreparation.initializeSecondaryModel ||
        !laserPreparation.attached ||
        !laserPreparation.linkedToWeapon || !laserPreparation.ray ||
        !laserPreparation.ignoreWeaponContact ||
        !laserPreparation.requiresWeapon ||
        !minePreparation.minePlacement ||
        !minePreparation.lockMineOnPlacement ||
        !minePreparation.mineTestsLock ||
        drobilkaPreparation.initializeModel ||
        !drobilkaPreparation.attached ||
        !drobilkaPreparation.ignoreWeaponContact ||
        springPreparationRoute.initializeModel ||
        !springPreparationRoute.linkedToWeapon ||
        springPreparationRoute.ignoreWeaponContact ||
        !craterPreparation.minePlacement ||
        craterPreparation.lockMineOnPlacement ||
        !craterPreparation.requiresWeapon ||
        !protonPreparation.minePlacement ||
        !protonPreparation.lockMineOnPlacement ||
        protonPreparation.mineTestsLock ||
        !source::Proj::PreparationRouteFor(0U).requiresWeapon ||
        !source::Proj::PreparationRouteFor(20U).requiresWeapon ||
        source::Proj::PreparationRouteFor(9U).requiresWeapon ||
        source::Proj::PreparationRouteFor(24U).requiresWeapon)
        return 132;
    source::Logic damageLogic;
    source::GameObject damageCar;
    damageCar.SetLogic(&damageLogic);
    source::Weapon damageWeapon;
    damageWeapon.SetParent(&damageCar);
    source::GameObject projectileDamageTarget;
    source::Proj damageProjectile;
    source::Proj::ShotContext damageContext;
    damageContext.logic = &damageLogic;
    damageContext.playerId = 7U;
    damageProjectile.SetLogic(&damageLogic);
    damageProjectile.PrepareSource(
        sourceDescription, &damageWeapon, damageContext);
    const auto damageCommand = damageProjectile.DamageTarget(
        &projectileDamageTarget, 17.0F,
        r3d::game::originalrace::DamageType::Energy);
    const source::GameObject& genericProjectile = damageProjectile;
    if (!damageCommand.valid || damageCommand.logic != &damageLogic ||
        damageCommand.senderCar != &damageCar ||
        damageCommand.target != &projectileDamageTarget ||
        damageCommand.playerId != 7U || damageCommand.damage != 17.0F ||
        damageCommand.damageType !=
            r3d::game::originalrace::DamageType::Energy ||
        damageProjectile.IsProj() != &damageProjectile ||
        genericProjectile.IsProj() != &damageProjectile ||
        projectileDamageTarget.IsProj() != nullptr ||
        damageProjectile.DamageTarget(nullptr, 1.0F).valid)
        return 133;
    const auto rocketRoute = source::Proj::ContactRouteFor(
        0U, false, false);
    const auto laserRoute = source::Proj::ContactRouteFor(
        3U, false, false);
    const auto mineRoute = source::Proj::ContactRouteFor(
        11U, false, false);
    const auto craterRoute = source::Proj::ContactRouteFor(
        20U, false, false);
    const auto impulseRoute = source::Proj::ContactRouteFor(
        21U, false, false);
    const auto protonRoute = source::Proj::ContactRouteFor(
        24U, false, false);
    if (rocketRoute.handler !=
            source::Proj::ContactHandler::Rocket ||
        !rocketRoute.rocketResponse || !rocketRoute.appliesDamage ||
        laserRoute.handler != source::Proj::ContactHandler::None ||
        laserRoute.damageType !=
            r3d::game::originalrace::DamageType::Energy ||
        mineRoute.handler != source::Proj::ContactHandler::Mine ||
        !mineRoute.testMineLock ||
        mineRoute.damageType !=
            r3d::game::originalrace::DamageType::Mine ||
        craterRoute.handler != source::Proj::ContactHandler::Crater ||
        craterRoute.testMineLock ||
        impulseRoute.handler != source::Proj::ContactHandler::Impulse ||
        impulseRoute.damageType !=
            r3d::game::originalrace::DamageType::Energy ||
        protonRoute.handler !=
            source::Proj::ContactHandler::MineProton ||
        source::Proj::ContactRouteFor(22U, true, false).handler !=
            source::Proj::ContactHandler::None ||
        source::Proj::ContactRouteFor(22U, false, true).handler !=
            source::Proj::ContactHandler::None)
        return 129;
    if (rocketProjectile.RouteContact(false).handler !=
            source::Proj::ContactHandler::Rocket ||
        rocketProjectile.RouteContact(true).handler !=
            source::Proj::ContactHandler::None)
        return 130;
    const auto rocketProgress = source::Proj::ProgressRouteFor(0U);
    const auto torpedaProgress = source::Proj::ProgressRouteFor(2U);
    const auto laserProgress = source::Proj::ProgressRouteFor(3U);
    const auto oilProgress = source::Proj::ProgressRouteFor(10U);
    const auto ripProgress = source::Proj::ProgressRouteFor(12U);
    const auto craterProgress = source::Proj::ProgressRouteFor(20U);
    const auto thunderProgress = source::Proj::ProgressRouteFor(22U);
    const auto resonanceProgress = source::Proj::ProgressRouteFor(23U);
    const auto protonProgress = source::Proj::ProgressRouteFor(24U);
    if (rocketProgress.handler !=
            source::Proj::ProgressHandler::Rocket ||
        !rocketProgress.rocketHeight || rocketProgress.homing ||
        torpedaProgress.handler !=
            source::Proj::ProgressHandler::Torpeda ||
        !torpedaProgress.homing || torpedaProgress.rocketHeight ||
        laserProgress.handler !=
            source::Proj::ProgressHandler::Laser ||
        !laserProgress.attached || !laserProgress.ray ||
        oilProgress.handler != source::Proj::ProgressHandler::Maslo ||
        !oilProgress.mineArming ||
        ripProgress.handler != source::Proj::ProgressHandler::MineRip ||
        !ripProgress.mineArming ||
        craterProgress.handler != source::Proj::ProgressHandler::None ||
        craterProgress.mineArming ||
        thunderProgress.handler !=
            source::Proj::ProgressHandler::Thunder ||
        !thunderProgress.rocketHeight ||
        resonanceProgress.handler !=
            source::Proj::ProgressHandler::Resonanse ||
        !resonanceProgress.rocketHeight ||
        protonProgress.handler !=
            source::Proj::ProgressHandler::MineProton ||
        !protonProgress.mineArming ||
        source::Proj::ProgressRouteFor(16U).handler !=
            source::Proj::ProgressHandler::None ||
        rocketProjectile.RouteProgress().handler !=
            source::Proj::ProgressHandler::Rocket)
        return 131;
    const auto fullMedpack = source::Proj::BonusContact(
        4U, true, 0.0F, 80.0F);
    const auto partialMedpack = source::Proj::BonusContact(
        4U, true, 5.0F, 80.0F);
    const auto chargeBonus = source::Proj::BonusContact(
        5U, true, 3.0F, 80.0F);
    if (!fullMedpack.take ||
        fullMedpack.type !=
            source::Proj::BonusContactType::Medpack ||
        fullMedpack.value != 80.0F ||
        partialMedpack.value != 5.0F ||
        chargeBonus.type !=
            source::Proj::BonusContactType::Charge ||
        chargeBonus.value != 3.0F ||
        source::Proj::BonusContact(
            4U, false, 0.0F, 80.0F).take ||
        source::Proj::BonusContact(
            8U, true, 3.0F, 80.0F).take)
        return 61;
    source::Player contactBonusPlayer;
    contactBonusPlayer.Reset(80.0F, 1U);
    source::MapObj contactBonusTargetMap;
    contactBonusTargetMap.SetType(source::GameObjType::RockCar);
    contactBonusTargetMap.BindGameObj(contactBonusPlayer.gameCar);
    contactBonusTargetMap.SetPlayer(&contactBonusPlayer);
    contactBonusPlayer.gameCar.ResetGameObject(80.0F);
    auto medpackDescription = sourceDescription;
    medpackDescription.type = 4U;
    source::Proj medpackProjectile;
    medpackProjectile.PrepareSource(
        medpackDescription, nullptr, source::Proj::ShotContext{});
    const auto concreteMedpack = medpackProjectile.ContactBonus(
        &contactBonusPlayer.gameCar, &contactBonusPlayer, 0.0F);
    source::Player wrongBonusPlayer;
    if (!concreteMedpack.take ||
        concreteMedpack.type !=
            source::Proj::BonusContactType::Medpack ||
        concreteMedpack.value != 80.0F ||
        medpackProjectile.ContactBonus(
            &contactBonusPlayer.gameCar,
            &wrongBonusPlayer, 0.0F).take ||
        medpackProjectile.ContactBonus(
            &frostNonCarTarget,
            &contactBonusPlayer, 0.0F).take)
        return 139;
    const auto linkedDestroy = source::Proj::OnDestroy(
        true, true, false);
    const auto unlinkedDestroy = source::Proj::OnDestroy(
        true, false, false);
    const auto targetDestroy = source::Proj::OnDestroy(
        false, false, true);
    if (!linkedDestroy.destroy || !linkedDestroy.clearWeapon ||
        linkedDestroy.clearTarget || unlinkedDestroy.destroy ||
        !unlinkedDestroy.clearWeapon ||
        !targetDestroy.clearTarget || targetDestroy.clearWeapon ||
        targetDestroy.destroy)
        return 62;

    source::Logic logic;
    logic.SetTouchBorderDamage({10.0F, 20.0F});
    logic.SetTouchBorderDamageForce({30.0F, 40.0F});
    logic.SetTouchCarDamage({50.0F, 60.0F});
    logic.SetTouchCarDamageForce({70.0F, 80.0F});
    logic.ResetContactBehavior(3U);
    auto& contacts = logic.GetPairPxContactEffect();
    if (logic.GetTouchBorderDamage() !=
            source::Logic::ContactRange{10.0F, 20.0F} ||
        logic.GetTouchBorderDamageForce() !=
            source::Logic::ContactRange{30.0F, 40.0F} ||
        logic.GetTouchCarDamage() !=
            source::Logic::ContactRange{50.0F, 60.0F} ||
        logic.GetTouchCarDamageForce() !=
            source::Logic::ContactRange{70.0F, 80.0F})
        return 63;
    const source::PairPxContactEffect::Key contactKey{4U, 9U};
    const std::array<source::PairPxContactEffect::Point, 3U>
        contactPoints{{{1.0F, 2.0F, 3.0F},
                       {4.0F, 5.0F, 6.0F},
                       {7.0F, 8.0F, 9.0F}}};
    if (contacts.OnContact(
            contactKey, 10000.0F, false, false,
            contactPoints, 0.5F).accepted ||
        contacts.OnContact(
            contactKey, 10001.0F, true, false,
            contactPoints, 0.5F).accepted)
        return 33;
    const auto firstContact = contacts.OnContact(
        contactKey, 10001.0F, false, false,
        contactPoints, 1.0F);
    if (!firstContact.accepted || !firstContact.pairCreated ||
        !firstContact.playSound || firstContact.sound != 2U ||
        firstContact.points.size() != 2U ||
        !firstContact.points[0].createdEffect ||
        !firstContact.points[1].createdEffect ||
        contacts.GetPairCount() != 1U ||
        contacts.GetContactCount(contactKey) != 2U)
        return 34;
    if (!contacts.OnProgress(0.1F).empty())
        return 35;
    const std::array<source::PairPxContactEffect::Point, 1U>
        onePoint{{{10.0F, 11.0F, 12.0F}}};
    const auto refreshed = contacts.OnContact(
        contactKey, 10001.0F, false, false, onePoint, 0.0F);
    if (refreshed.pairCreated || refreshed.points.size() != 1U ||
        refreshed.points.front().createdEffect ||
        refreshed.sound != 2U)
        return 36;
    if (!contacts.OnProgress(0.001F).empty() ||
        contacts.GetContactCount(contactKey) != 2U)
        return 37;
    const auto tailReleased = contacts.OnProgress(0.1F);
    if (tailReleased.size() != 1U ||
        tailReleased.front().slot != 1U ||
        contacts.GetContactCount(contactKey) != 1U)
        return 38;
    contacts.OnContact(
        contactKey, 10001.0F, false, false, onePoint, 0.0F);
    if (!contacts.OnProgress(0.1F).empty())
        return 39;
    if (!contacts.OnProgress(0.1F).empty())
        return 40;
    const auto finalReleased = contacts.OnProgress(0.001F);
    if (finalReleased.size() != 1U ||
        finalReleased.front().slot != 0U ||
        contacts.GetPairCount() != 0U)
        return 41;

    auto* hyperMapObject = hyperWeapon.GetMapObj();
    auto* mineMapObject = mineWeapon.GetMapObj();
    if (!rack.Remove(hyperMapObject) ||
        rack.GetHyperDrive() != nullptr ||
        rack.GetMines() != &mineWeapon ||
        !rack.Remove(mineMapObject) ||
        rack.GetMines() != nullptr ||
        rack.GetLiveCount() != 3U)
        return 64;

    std::cout << "original Weapon/Proj/WeaponItem/Droid/Reflector/Logic "
                 "source rules passed\n";
    return 0;
}
