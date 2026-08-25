#include "OriginalWeapon.h"

#include <array>
#include <cmath>
#include <iostream>

int main()
{
    namespace source = r3d::game::originalrace::source;

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
    if (!weapon.IsReadyShot())
        return 4;
    weapon.OnShot(true);
    if (weapon.IsReadyShot() ||
        std::abs(weapon.GetShotTime()) > 0.0001F)
        return 5;

    source::WeaponRack rack;
    rack.primary[2].SetDesc(
        0.2F, std::span<const std::uint32_t>{});
    rack.hyper.SetDesc(
        0.3F, std::span<const std::uint32_t>{});
    rack.mine.SetDesc(0.4F, projectiles);
    rack.OnProgress(0.5F);
    if (!rack.primary[2].IsReadyShot() ||
        !rack.hyper.IsReadyShot() || !rack.mine.IsReadyShot())
        return 6;
    rack.Reset();
    if (rack.primary[2].IsReadyShot() ||
        rack.hyper.IsReadyShot() || rack.mine.IsReadyShot())
        return 7;

    std::uint32_t charge = 2U;
    source::WeaponItem item(
        &rack.primary[2], 7U, 4U, &charge, 2U, 12.5F, 100);
    rack.primary[2].OnProgress(0.3F);
    if (!item.IsInstalled() || !item.IsReadyShot() ||
        !item.HasShotCharge() || item.GetMaxCharge() != 7U ||
        item.GetCntCharge() != 4U || item.GetCurCharge() != 2U ||
        item.GetChargeStep() != 2U ||
        std::abs(item.GetDamage() - 12.5F) > 0.0001F ||
        item.GetChargeCost() != 100)
        return 8;
    if (item.Shot(false) || charge != 2U || !item.IsReadyShot())
        return 9;
    if (!item.Shot(true) || charge != 1U || item.IsReadyShot())
        return 10;
    item.Reload();
    if (charge != 4U)
        return 11;

    // maxCharge==0 is the original infinite-ammunition sentinel.  It must
    // still create a shot at currentCharge==0 and clamp the decrement to 0.
    charge = 0U;
    source::WeaponItem infinite(
        &rack.mine, 0U, 0U, &charge);
    rack.mine.OnProgress(1.0F);
    if (!infinite.HasShotCharge() || !infinite.Shot(true) || charge != 0U)
        return 12;

    // NetPlayer::DoShot supplies an explicit current-1 charge.  WeaponItem
    // applies it even if projectile preparation fails, exactly as Player.cpp.
    charge = 3U;
    source::WeaponItem replicated(
        &rack.hyper, 7U, 3U, &charge);
    if (replicated.Shot(false, 1) || charge != 1U)
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
    const auto allPlan = source::Logic::ShotAll(primary, true);
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

    source::PlayerItemRack supportRack;
    std::uint32_t firstReflectorCharge = 0U;
    std::uint32_t secondReflectorCharge = 0U;
    supportRack.BindReflector(
        2U, &rack.primary[2], 1U, 1U,
        &secondReflectorCharge, 0.9F);
    supportRack.BindReflector(
        0U, &rack.primary[0], 1U, 1U,
        &firstReflectorCharge, 0.4F);
    if (supportRack.GetType(0U) !=
            source::PlayerItemRack::Type::Reflector ||
        supportRack.GetReflector(0U) == nullptr ||
        std::abs(supportRack.Reflect(100.0F) - 60.0F) > 0.001F)
        return 21;

    std::uint32_t secondDroidCharge = 0U;
    supportRack.Reset();
    supportRack.BindDroid(
        0U, &rack.primary[0], 1U, 1U,
        &droidCharge, 5.0F, 1.0F);
    supportRack.BindDroid(
        1U, &rack.primary[1], 1U, 1U,
        &secondDroidCharge, 5.0F, 1.0F);
    supportRack.OnCreateCar();
    life = 80.0F;
    if (supportRack.OnProgress(
            1.001F, life, 100.0F, false) != 10.0F ||
        life != 90.0F)
        return 22;
    supportRack.OnDestroyCar();
    if (supportRack.OnProgress(
            2.0F, life, 100.0F, false) != 0.0F ||
        life != 90.0F)
        return 23;

    std::cout << "original Weapon/WeaponItem/Droid/Reflector/Logic "
                 "source rules passed\n";
    return 0;
}
