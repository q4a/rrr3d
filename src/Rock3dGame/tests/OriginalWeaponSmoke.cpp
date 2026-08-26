#include "OriginalLogic.h"

#include <array>
#include <cmath>
#include <iostream>

namespace source = r3d::game::originalrace::source;

namespace
{

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
    if (weapon.IsReadyShot() || !weapon.IsMaslo() ||
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
    weapon.OnProjectilePrepared();
    if (weapon.GetShotEffect().GetShotCount() != 2U)
        return 6;

    source::WeaponRack rack;
    std::array<r3d::game::originalrace::ProjectileDefinition, 2U>
        itemProjectiles{};
    itemProjectiles[0].type = 0U;
    itemProjectiles[0].speed = 40.0F;
    itemProjectiles[0].maximumDistance = 300.0F;
    itemProjectiles[0].damage = 6.0F;
    itemProjectiles[1] = itemProjectiles[0];
    itemProjectiles[1].damage = 6.5F;
    rack.primary[2].SetDesc(0.2F, itemProjectiles);
    rack.hyper.SetDesc(
        0.3F, std::span<const std::uint32_t>{});
    rack.mine.SetDesc(0.4F, projectiles);
    rack.OnProgress(0.5F);
    if (!rack.primary[2].IsReadyShot() ||
        !rack.hyper.IsReadyShot() || !rack.mine.IsReadyShot())
        return 7;
    rack.Reset();
    if (rack.primary[2].IsReadyShot() ||
        rack.hyper.IsReadyShot() || rack.mine.IsReadyShot())
        return 8;

    std::uint32_t charge = 2U;
    source::WeaponItem item(
        &rack.primary[2], 7U, 4U, &charge, 2U, 12.5F, 100);
    rack.primary[2].OnProgress(0.3F);
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
    item.OnCreateCar();
    if (rack.primary[2].GetDesc().Front().type != 2U ||
        std::abs(rack.primary[2].GetDesc().shotDelay - 0.75F) >
            0.0001F)
        return 11;
    item.OnDestroyCar();

    // maxCharge==0 is the original infinite-ammunition sentinel.  It must
    // still create a shot at currentCharge==0 and clamp the decrement to 0.
    charge = 0U;
    source::WeaponItem infinite(
        &rack.mine, 0U, 0U, &charge);
    infinite.OnCreateCar();
    rack.mine.OnProgress(1.0F);
    if (!infinite.HasShotCharge() || !infinite.Shot(true) || charge != 0U)
        return 12;

    // NetPlayer::DoShot supplies an explicit current-1 charge.  WeaponItem
    // applies it even if projectile preparation fails, exactly as Player.cpp.
    charge = 3U;
    source::WeaponItem replicated(
        &rack.hyper, 7U, 3U, &charge);
    replicated.OnCreateCar();
    if (replicated.Shot(false, 1) ||
        replicated.GetCurCharge() != 1U || charge != 3U)
        return 13;

    rack.primary[0].SetDesc(
        0.1F, std::span<const std::uint32_t>{});
    rack.primary[1].SetDesc(
        0.1F, std::span<const std::uint32_t>{});
    rack.primary[0].OnProgress(0.2F);
    std::uint32_t firstCharge = 1U;
    std::uint32_t secondCharge = 1U;
    std::array<source::WeaponItem, 2U> primary{
        source::WeaponItem(&rack.primary[0], 7U, 1U, &firstCharge),
        source::WeaponItem(&rack.primary[1], 7U, 1U, &secondCharge)};
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
        &rack.primary[0], 1U, 1U, &droidCharge, 17.0F, 1.0F);
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
    if (!authoritative.death || !authoritative.killCredit ||
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
    autoOil.Reset(10U);
    autoOil.LogicInited(false);
    if (autoOil.IsPrepared())
        return 62;
    autoOil.LogicInited();
    if (!autoOil.IsPrepared() || !autoOil.IsArming() ||
        autoOil.GetModelScale() != 0.0F)
        return 63;
    autoOil.OnProgress(0.125F);
    if (!autoOil.IsArming() ||
        std::abs(autoOil.GetModelScale() - 0.5F) > 0.001F)
        return 64;
    autoOil.OnProgress(0.125F);
    if (autoOil.IsArming() ||
        std::abs(autoOil.GetModelScale() - 1.0F) > 0.001F)
        return 65;
    autoOil.LogicReleased();
    if (autoOil.IsPrepared())
        return 66;

    source::AutoProj autoMinePiece;
    autoMinePiece.Reset(13U);
    autoMinePiece.LogicInited();
    autoMinePiece.OnProgress(1.0F);
    if (!autoMinePiece.IsPrepared() || autoMinePiece.IsArming() ||
        autoMinePiece.GetModelScale() >= 0.0F)
        return 67;

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
    if (source::Proj::MineContactAllowed(
            true, true, true, true, -1.0F, false) ||
        source::Proj::MineContactAllowed(
            true, false, false, false, 0.1F, true) ||
        !source::Proj::MineContactAllowed(
            true, false, false, false, 0.1F, false) ||
        !source::Proj::MineContactAllowed(
            true, false, false, false, -1.0F, true))
        return 53;
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

    std::cout << "original Weapon/Proj/WeaponItem/Droid/Reflector/Logic "
                 "source rules passed\n";
    return 0;
}
