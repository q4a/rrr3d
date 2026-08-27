#include "OriginalLogic.h"
#include "OriginalPlayer.h"
#include "OriginalRace.h"
#include "OriginalWeapon.h"

#include <array>
#include <cmath>
#include <iostream>
#include <type_traits>
#include <vector>

namespace source = r3d::game::originalrace::source;

int main()
{
    source::Player player;
    player.Reset(80.0F, 3U);
    static_assert(!std::is_base_of_v<source::GameObject, source::Player>);
    if (player.gameCar.GetLife() != player.GetLife() ||
        player.gameCar.GetEventSink() != &player ||
        player.gameCar.GetBehaviors().GetCount() != 3U ||
        player.gameCar.GetBehaviors().Find(
            source::BehaviorType::LowLifePoints) == nullptr ||
        player.gameCar.GetBehaviors().Find(
            source::BehaviorType::ImmortalEffect) == nullptr ||
        player.gameCar.GetBehaviors().Find(
            source::BehaviorType::DamageEffect) == nullptr ||
        player.gameCar.GetListenerCount() != 4U)
        return 71;

    // FrostRayUpdate dynamically adds SlowEffect only once. Its state expires
    // during the behavior callback; the owner erases the removed behavior at
    // the beginning of the next source behavior pass.
    source::Player slowPlayer;
    slowPlayer.Reset(100.0F, 1U);
    if (!slowPlayer.AttachSlowEffect(0.5F, 4U, 2U) ||
        slowPlayer.AttachSlowEffect(2.0F, 5U, 3U) ||
        slowPlayer.gameCar.GetBehaviors().GetCount() != 4U ||
        slowPlayer.gameCar.GetBehaviors().Find(
            source::BehaviorType::SlowEffect) == nullptr ||
        slowPlayer.gameCar.GetListenerCount() != 5U)
        return 72;
    const auto slowActive = slowPlayer.ProgressBehaviors(
        0.25F, 0.35F, 30.0F);
    if (!slowActive.slowSpeedLimited || slowActive.slowReleased ||
        slowActive.gameObject.behaviorsProgressed != 4U ||
        slowPlayer.slowEffect.GetWeapon() != 4U ||
        slowPlayer.slowEffect.GetProjectile() != 2U)
        return 73;
    const auto slowReleased = slowPlayer.ProgressBehaviors(
        0.251F, 0.35F, 30.0F);
    if (!slowReleased.slowSpeedLimited || !slowReleased.slowReleased ||
        slowPlayer.slowEffect.IsEffectMaked() ||
        slowPlayer.gameCar.GetBehaviors().Find(
            source::BehaviorType::SlowEffect) == nullptr)
        return 74;
    const auto slowRemoved = slowPlayer.ProgressBehaviors(
        0.0F, 0.35F, 30.0F);
    if (slowRemoved.gameObject.behaviorsRemoved != 1U ||
        slowRemoved.gameObject.behaviorsProgressed != 3U ||
        slowPlayer.gameCar.GetBehaviors().GetCount() != 3U ||
        slowPlayer.gameCar.GetBehaviors().Find(
            source::BehaviorType::SlowEffect) != nullptr ||
        slowPlayer.gameCar.GetListenerCount() != 4U)
        return 75;
    if (!slowPlayer.AttachSlowEffect(1.0F, 1U, 1U))
        return 76;
    slowPlayer.Destroy();
    if (slowPlayer.gameCar.GetBehaviors().Find(
            source::BehaviorType::SlowEffect) != nullptr)
        return 77;
    if (slowPlayer.gameCar.GetBehaviors().GetCount() != 3U)
        return 78;
    if (slowPlayer.gameCar.GetListenerCount() != 3U)
        return 79;
    player.car.SetSize(7.5F);
    player.ConfigureIdentity(
        source::Player::humanId, 7, 3U, "Tyler", "Network Tyler",
        {0.2F, 0.3F, 0.4F, 1.0F});
    if (player.GetLife() != 80.0F || player.GetMaxLife() != 80.0F ||
        player.GetPlace() != 3U || player.GetFinished() ||
        player.IsDestroyed())
        return 1;
    if (std::abs(player.car.GetSize() - 7.5F) > 0.001F ||
        std::abs(player.car.GetRadius() - 3.75F) > 0.001F)
        return 51;
    if (!player.IsHuman() || player.IsComputer() || player.IsOpponent() ||
        player.GetId() != source::Player::humanId ||
        player.GetGamerId() != 7 || player.GetNetSlot() != 3U ||
        player.GetName() != "Network Tyler" ||
        std::abs(player.GetColor()[2] - 0.4F) > 0.001F)
        return 41;
    if (player.GetHeadLight() != source::Player::HeadLightMode::None ||
        !player.GetReflScene())
        return 45;
    player.SetMoney(10U);
    player.SetPoints(5U);
    player.AddMoney(-3);
    player.AddPoints(2);
    if (player.GetMoney() != 7U || player.GetPoints() != 7U)
        return 55;
    player.SetMoney(0U);
    player.SetPoints(0U);
    player.SetHeadlight(source::Player::HeadLightMode::Two);
    player.SetReflScene(false);
    if (player.GetHeadLight() != source::Player::HeadLightMode::Two ||
        player.GetReflScene() || player.HasCar() ||
        player.HasAttachedLights())
        return 46;
    player.CreateCar(true);
    if (!player.HasCar() || !player.HasAttachedLights())
        return 47;
    player.FreeCar(false);
    if (player.HasCar() || player.HasAttachedLights() ||
        player.GetHeadLight() != source::Player::HeadLightMode::Two)
        return 48;
    player.CreateCar(false);
    if (!player.HasAttachedLights())
        return 49;

    r3d::game::originalrace::Vehicle visualVehicle;
    visualVehicle.bodyVisuals.push_back({});
    visualVehicle.bodyVisuals.front().meshPath = "car.r3dMesh";
    visualVehicle.nightLights = {
        {true, {1.7F, 0.4F, 0.02F}, {1.5F, 1.5F}},
        {false, {-1.57F, 0.45F, 0.28F}, {1.0F, 1.0F}}};
    source::Player visualPlayer;
    visualPlayer.Reset(100.0F, 1U);
    visualPlayer.ConfigureIdentity(
        source::Player::humanId, 0, 0U, "visual", {},
        {0.2F, 0.4F, 0.6F, 1.0F});
    visualPlayer.SetCar(&visualVehicle);
    visualPlayer.SetHeadlight(source::Player::HeadLightMode::Two);
    const auto& detachedPresentation =
        visualPlayer.GetPresentationState();
    if (!detachedPresentation.headLights[0].created ||
        !detachedPresentation.headLights[1].created ||
        detachedPresentation.headLights[0].enabled ||
        detachedPresentation.headLights[1].enabled ||
        !detachedPresentation.headLights[0].highQualityShadow ||
        detachedPresentation.headLights[1].highQualityShadow ||
        !detachedPresentation.nightFlareCreated ||
        detachedPresentation.nightFlareAttached ||
        !detachedPresentation.nightLights.empty())
        return 80;
    visualPlayer.CreateCar(true);
    const auto& attachedPresentation =
        visualPlayer.GetPresentationState();
    if (!attachedPresentation.headLights[0].enabled ||
        !attachedPresentation.headLights[1].enabled ||
        attachedPresentation.headLights[0].position.y != 1.0F ||
        attachedPresentation.headLights[1].position.y != -1.0F ||
        attachedPresentation.headLights[0].farDistance != 50.0F ||
        !attachedPresentation.nightFlareAttached ||
        attachedPresentation.nightLights.size() != 2U ||
        !attachedPresentation.nightLights[0].head ||
        attachedPresentation.nightLights[1].head ||
        !attachedPresentation.colorMaterialCreated ||
        !attachedPresentation.colorMaterialAttached ||
        attachedPresentation.color !=
            std::array<float, 4U>{0.2F, 0.4F, 0.6F, 1.0F})
        return 81;
    visualPlayer.SetColor({0.7F, 0.6F, 0.5F, 1.0F});
    visualPlayer.SetReflScene(false);
    visualPlayer.SetHeadlight(source::Player::HeadLightMode::One);
    const auto& oneLightPresentation =
        visualPlayer.GetPresentationState();
    if (oneLightPresentation.reflectionScene ||
        oneLightPresentation.color !=
            std::array<float, 4U>{0.7F, 0.6F, 0.5F, 1.0F} ||
        oneLightPresentation.headLights[0].position.y != 0.0F ||
        oneLightPresentation.headLights[1].created ||
        !oneLightPresentation.nightFlareAttached ||
        oneLightPresentation.nightLights.size() != 2U)
        return 82;
    visualPlayer.FreeCar(false);
    const auto& releasedPresentation =
        visualPlayer.GetPresentationState();
    if (releasedPresentation.headLights[0].enabled ||
        releasedPresentation.nightFlareAttached ||
        !releasedPresentation.nightLights.empty() ||
        !releasedPresentation.colorMaterialCreated ||
        releasedPresentation.colorMaterialAttached ||
        visualPlayer.GetHeadLight() !=
            source::Player::HeadLightMode::One)
        return 83;
    visualPlayer.CreateCar(false);
    if (!visualPlayer.GetPresentationState().headLights[0].enabled ||
        !visualPlayer.GetPresentationState().nightFlareAttached ||
        visualPlayer.GetPresentationState().nightLights.size() != 2U ||
        !visualPlayer.GetPresentationState().colorMaterialAttached)
        return 84;
    visualPlayer.SetHeadlight(source::Player::HeadLightMode::None);
    if (visualPlayer.GetPresentationState().headLights[0].created ||
        visualPlayer.GetPresentationState().headLights[1].created ||
        visualPlayer.GetPresentationState().nightFlareCreated ||
        visualPlayer.GetPresentationState().nightFlareAttached)
        return 85;
    visualPlayer.FreeCar(true);
    player.SetNetName({});
    if (player.GetName() != "Tyler")
        return 42;
    player.SetId(4 << source::Player::opponentBit);
    if (player.IsHuman() || player.IsComputer() || !player.IsOpponent() ||
        !player.IsHumanOrOpponent())
        return 43;
    player.SetId(3);
    if (player.IsHuman() || !player.IsComputer() || player.IsOpponent() ||
        player.IsHumanOrOpponent())
        return 44;
    player.SetCheat(
        source::Player::cheatEnableFaster |
        source::Player::cheatEnableSlower);
    if (player.GetCheat() !=
        (source::Player::cheatEnableFaster |
         source::Player::cheatEnableSlower))
        return 50;
    player.SetCheat(source::Player::cheatDisabled);

    r3d::game::originalrace::Vehicle firstCar;
    firstCar.record = "world\\db\\root\\ctCar\\marauder";
    r3d::game::originalrace::Vehicle secondCar;
    secondCar.record = "world\\db\\root\\ctCar\\buggi";
    secondCar.slotMounts[6].position = {2.0F, 3.0F, 4.0F};
    secondCar.slotMounts[6].placements.push_back(
        {"world\\race\\workshopRoot\\workshop\\droid",
         {0.1F, 0.2F, 0.3F, 0.9F}, {0.5F, -0.5F, 1.0F}});
    secondCar.slotMounts[4].position = {4.0F, 5.0F, 6.0F};
    secondCar.slotMounts[4].placements.push_back(
        {"world\\race\\workshopRoot\\workshop\\hyperdrive",
         {0.0F, 0.0F, 1.0F, 0.0F}, {-1.5F, 0.0F, 0.35F}});
    player.SetCar(&firstCar);
    player.CreateCar(true);
    if (player.GetCarRecord() != &firstCar || !player.HasCar())
        return 57;
    player.car.numLaps = 2U;
    player.SetCar(&secondCar);
    if (player.GetCarRecord() != &secondCar || player.HasCar() ||
        player.car.numLaps != 0U)
        return 58;

    auto& playerWeapons = player.GetWeaponRack();
    if (&playerWeapons != &player.gameCar.GetWeapons())
        return 78;
    source::Weapon::Desc droidWeaponDescription;
    droidWeaponDescription.shotDelay = 0.5F;
    droidWeaponDescription.projectiles.resize(1U);
    droidWeaponDescription.projectiles.front().type = 1U;
    auto& playerWeaponObject = playerWeapons.Add(
        droidWeaponDescription, "Weapon\\testDroid");
    auto* playerWeapon = playerWeaponObject.GetWeapon();
    playerWeapon->OnShot();
    if (playerWeapon->IsReadyShot())
        return 62;
    player.gameCar.OnProgress(0.51F);
    if (!playerWeapon->IsReadyShot())
        return 63;
    r3d::game::originalrace::OriginalWorkshopItem droidRecord;
    droidRecord.record =
        "world\\race\\workshopRoot\\workshop\\droid";
    droidRecord.type = static_cast<std::uint32_t>(
        source::SlotType::Droid);
    r3d::game::originalrace::OriginalWorkshopItem hyperRecord;
    hyperRecord.record =
        "world\\race\\workshopRoot\\workshop\\hyperdrive";
    hyperRecord.type = static_cast<std::uint32_t>(
        source::SlotType::Hyper);
    player.BindSlots(
        {droidRecord, hyperRecord},
        {{droidRecord.record, "stWeapon1", 1U},
         {hyperRecord.record, "stHyper", 1U}});
    player.weaponSlots[0] = 0U;
    player.weaponCapacity[0] = 1U;
    player.weaponCharges[0] = 1U;
    std::array<r3d::game::originalrace::WeaponDefinition, 1U>
        droidDefinitions{};
    droidDefinitions[0].itemType =
        r3d::game::originalrace::WeaponItemType::Droid;
    droidDefinitions[0].maximumCharge = 1U;
    droidDefinitions[0].reloadCharge = 1U;
    droidDefinitions[0].repairValue = 17.0F;
    droidDefinitions[0].repairPeriod = 0.1F;
    player.BindWeaponItems(droidDefinitions);
    auto* droidSlot = player.GetSlotInst(source::SlotType::Droid);
    auto* droid = droidSlot == nullptr
                      ? nullptr
                      : dynamic_cast<source::DroidItem*>(
                            &droidSlot->GetItem());
    if (droid == nullptr || droid->IsInstalled() ||
        droid->GetPos() !=
            std::array<float, 3>{2.5F, 2.5F, 5.0F} ||
        droid->GetRot() !=
            std::array<float, 4>{0.1F, 0.2F, 0.3F, 0.9F} ||
        player.GetSlotInst(source::PlayerSlotType::Hyper) == nullptr ||
        player.GetSlotInst(source::PlayerSlotType::Hyper)
                ->GetItem().GetPos() !=
            std::array<float, 3>{2.5F, 5.0F, 6.35F})
        return 67;
    player.SetLife(60.0F);
    player.CreateCar(true);
    if (droid == nullptr || !droid->IsProgressRegistered() ||
        !droid->IsInstalled())
        return 59;
    player.ProgressBehaviors(0.101F, 0.35F, 0.0F);
    if (std::abs(player.GetLife() - 65.0F) > 0.001F)
        return 60;
    player.FreeCar(false);
    if (droid->IsProgressRegistered() || droid->IsInstalled() ||
        droid->IsReadyShot())
        return 61;
    player.CreateCar(true);
    if (!droid->IsInstalled())
        return 61;

    // Garage::InstalSlot replaces the physical object, applies the selected
    // car's PlaceSlot/PlaceItem transform and immediately connects a Droid
    // when the car actor already exists.
    r3d::game::originalrace::OriginalWorkshopItem liveDroidRecord;
    liveDroidRecord.record =
        "world\\race\\workshopRoot\\workshop\\droid";
    liveDroidRecord.type = static_cast<std::uint32_t>(
        source::SlotType::Droid);
    r3d::game::originalrace::OriginalWorkshopItem liveReflectorRecord;
    liveReflectorRecord.record =
        "world\\race\\workshopRoot\\workshop\\reflector";
    liveReflectorRecord.type = static_cast<std::uint32_t>(
        source::SlotType::Reflector);
    player.SetSlot(
        source::PlayerSlotType::Weapon1, &liveDroidRecord,
        {1.0F, 2.0F, 3.0F}, {0.1F, 0.2F, 0.3F, 0.9F});
    const auto* liveDroidSlot = player.GetSlotInst(
        source::SlotType::Droid);
    const auto* liveDroid =
        liveDroidSlot == nullptr
            ? nullptr
            : dynamic_cast<const source::DroidItem*>(
                  &liveDroidSlot->GetItem());
    if (liveDroid == nullptr || !liveDroid->IsProgressRegistered() ||
        liveDroid->GetPos() !=
            std::array<float, 3>{1.0F, 2.0F, 3.0F} ||
        liveDroid->GetRot() !=
            std::array<float, 4>{0.1F, 0.2F, 0.3F, 0.9F})
        return 65;
    player.SetSlot(
        source::PlayerSlotType::Weapon1, &liveReflectorRecord,
        {-1.0F, 0.5F, 4.0F}, {0.0F, 0.0F, 0.0F, 1.0F});
    const auto* liveReflectorSlot = player.GetSlotInst(
        source::SlotType::Reflector);
    if (liveReflectorSlot == nullptr ||
        liveReflectorSlot->GetItem().GetPos() !=
            std::array<float, 3>{-1.0F, 0.5F, 4.0F})
        return 66;
    player.SetSlot(source::PlayerSlotType::Weapon1, nullptr);

    player.weaponSlots = {2U, source::Player::invalidWeapon, 4U, 5U};
    player.weaponCapacity = {6U, 0U, 3U, 2U};
    player.weaponCharges = {1U, 0U, 0U, 1U};
    player.selectedWeaponSlot = 1U;
    player.hyperWeapon = 0U;
    player.hyperCapacity = 2U;
    player.hyperCharge = 0U;
    player.mineWeapon = 1U;
    player.mineCapacity = 4U;
    player.mines = 1U;
    player.SyncSelectedWeapon(6U);
    if (player.selectedWeaponSlot != 2U || player.selectedWeapon != 4U ||
        player.ammunition != 0U)
        return 2;

    const std::vector<std::uint32_t> maximumCharges{
        10U, 8U, 6U, 4U, 3U, 2U};
    std::vector<r3d::game::originalrace::WeaponDefinition>
        playerWeaponDefinitions(maximumCharges.size());
    for (std::size_t index = 0U;
         index < playerWeaponDefinitions.size(); ++index)
    {
        playerWeaponDefinitions[index].maximumCharge =
            maximumCharges[index];
        playerWeaponDefinitions[index].reloadCharge =
            maximumCharges[index];
    }
    player.BindWeaponItems(playerWeaponDefinitions);
    const auto firstWeaponItems = player.GetPrimaryWeaponItems();
    const auto secondWeaponItems = player.GetPrimaryWeaponItems();
    if (firstWeaponItems[0] == nullptr ||
        firstWeaponItems[0] != secondWeaponItems[0] ||
        !firstWeaponItems[0]->IsInstalled() ||
        firstWeaponItems[0]->GetCurCharge() != 1U ||
        player.GetHyperWeaponItem() == nullptr ||
        player.GetHyperWeaponItem()->GetCurCharge() != 0U ||
        player.GetMineWeaponItem() == nullptr ||
        player.GetMineWeaponItem()->GetCurCharge() != 1U)
        return 64;
    if (player.GetWeaponRack().GetLiveCount() != 5U ||
        firstWeaponItems[0]->GetWeapon()->GetParent() != &player.gameCar ||
        firstWeaponItems[0]->GetWeapon()->GetMapObj() == nullptr ||
        firstWeaponItems[0]->GetWeapon()->GetMapObj()->GetOwner() !=
            &player.GetWeaponRack())
        return 80;
    const auto hyper = player.TakeAmmunition(
        0.5F, maximumCharges, 0.0F);
    if (hyper.slot != source::PlayerBonusSlot::Hyper ||
        hyper.weapon != 0U ||
        player.GetHyperWeaponItem()->GetCurCharge() != 2U ||
        player.hyperCharge != 0U)
        return 3;
    const auto mine = player.TakeAmmunition(
        0.5F, maximumCharges, 0.0F);
    if (mine.slot != source::PlayerBonusSlot::Mine ||
        mine.weapon != 1U ||
        player.GetMineWeaponItem()->GetCurCharge() != 4U ||
        player.mines != 1U)
        return 4;

    player.ReloadWeapons(maximumCharges.size());
    if (firstWeaponItems[0]->GetCurCharge() != 6U ||
        firstWeaponItems[2]->GetCurCharge() != 3U ||
        firstWeaponItems[3]->GetCurCharge() != 2U ||
        player.GetHyperWeaponItem()->GetCurCharge() != 2U ||
        player.GetMineWeaponItem()->GetCurCharge() != 4U)
        return 5;
    player.OnLapPass(maximumCharges.size());
    if (player.car.numLaps != 1U ||
        firstWeaponItems[0]->GetCurCharge() != 6U)
        return 27;

    source::Weapon::Desc shotDescription;
    shotDescription.shotDelay = 0.5F;
    shotDescription.projectiles.resize(1U);
    shotDescription.projectiles.front().type = 11U;
    source::Weapon shotWeapon(shotDescription);
    std::uint32_t shotCharge = 2U;
    source::WeaponItem shotItem(
        &shotWeapon, 2U, 2U, &shotCharge);
    shotItem.OnCreateCar();
    source::Logic shotLogic;
    std::array<source::Weapon::ShotContext, 1U> shotContexts{};
    shotContexts.front().logic = &shotLogic;
    source::Weapon::ProjList firstShotProjectiles;
    if (!player.Shot(
            shotItem, shotContexts, false, 21U, -1,
            &firstShotProjectiles) ||
        firstShotProjectiles.size() != 1U ||
        shotItem.GetCurCharge() != 1U || shotCharge != 2U ||
        player.HasBonusProjectile(21U) ||
        player.GetNextBonusProjectileId() != 1U)
        return 52;
    source::Weapon::ProjList secondShotProjectiles;
    if (!player.Shot(
            shotItem, shotContexts, true, 21U, -1,
            &secondShotProjectiles) ||
        secondShotProjectiles.size() != 1U ||
        shotItem.GetCurCharge() != 0U || shotCharge != 2U ||
        !player.HasBonusProjectile(21U) ||
        player.GetNextBonusProjectileId() != 22U)
        return 53;
    // NetPlayer::DoShot supplies its replicated charge even if projectile
    // preparation fails; a failed stMine must not enter _bonusProjs.
    auto rejectedShotContexts = shotContexts;
    rejectedShotContexts.front().preparationAccepted = false;
    if (player.Shot(
            shotItem, rejectedShotContexts, true, 30U, 5) ||
        shotItem.GetCurCharge() != 5U || shotCharge != 2U ||
        player.HasBonusProjectile(30U) ||
        player.GetNextBonusProjectileId() != 22U)
        return 54;
    for (auto* projectile : firstShotProjectiles)
        projectile->Death();
    for (auto* projectile : secondShotProjectiles)
        projectile->Death();
    if (shotLogic.ProgressGameObjs(0.0F).removed != 2U)
        return 54;

    player.SetLife(50.0F);
    player.TakeMedpack(7.5F);
    if (std::abs(player.GetLife() - 57.5F) > 0.001F)
        return 6;
    player.TakeMedpack(100.0F);
    if (player.GetLife() != player.GetMaxLife())
        return 7;
    player.TakeMoney(19.9F);
    player.ResetPickMoney();
    if (player.GetPickMoney() != 0U)
        return 56;
    player.TakeMoney(19.9F);
    player.TakeImmortal(4.5F);
    if (player.GetPickMoney() != 19U ||
        std::abs(player.GetShieldSeconds() - 4.5F) > 0.001F ||
        player.immortalEffect.GetFadeInTime() != 0.0F)
        return 8;
    const float immortalLife = player.GetLife();
    player.Damage(1U, 5.0F, r3d::game::originalrace::DamageType::Energy);
    if (player.GetLife() != immortalLife ||
        player.immortalEffect.GetDamageTime() != 0.0F ||
        !player.ConsumeEnergyDamageEffectCreated())
        return 21;
    const auto immortalDamageEvents = player.TakeGameEvents();
    if (immortalDamageEvents.size() != 1U ||
        immortalDamageEvents[0].kind != source::PlayerGameEventKind::Damage ||
        immortalDamageEvents[0].otherPlayerId != 1U ||
        immortalDamageEvents[0].value != 5.0F)
        return 23;
    player.Damage(1U, 5.0F, r3d::game::originalrace::DamageType::Energy);
    if (player.ConsumeEnergyDamageEffectCreated() ||
        player.TakeGameEvents().size() != 1U)
        return 22;

    player.Complete(1U, 100U, 20U, 10.0F);
    if (!player.GetFinished() || player.GetPlace() != 1U ||
        player.FinishBrake(10.29F) != 0.0F ||
        player.FinishBrake(10.3F) != 1.0F)
        return 9;
    player.SetBlockTime(source::Player::finishBlockSeconds);
    if (!player.IsBlock() ||
        player.ProgressBlock(0.1F) != source::PlayerBlockMove::Coast ||
        std::abs(player.GetBlockTime() - 0.2F) > 0.001F ||
        player.ProgressBlock(0.21F) != source::PlayerBlockMove::Brake ||
        player.GetBlockTime() != 0.0F)
        return 19;
    player.ResetBlock(false);
    if (player.IsBlock() || player.GetBlockTime() != -1.0F ||
        player.ProgressBlock(1.0F) !=
            source::PlayerBlockMove::Unblocked)
        return 20;

    source::Player progressPlayer;
    progressPlayer.Reset(80.0F, 1U);
    progressPlayer.SetBlockTime(0.2F);
    const std::vector<source::Player::CheatPlayerView> progressViews{
        {0U, true, true, 0.0F},
        {1U, false, true, 1.0F}};
    const auto progress = progressPlayer.OnProgress(
        0.1F, true, source::Player::cheatDisabled,
        0U, 1U, progressViews);
    if (progress.cheat.faster || progress.cheat.slower ||
        progress.restore != source::PlayerRestoreStep::None ||
        progress.blockMove != source::PlayerBlockMove::Coast)
        return 31;
    progressPlayer.Destroy();
    progressPlayer.car.cheatFaster = true;
    progressPlayer.car.cheatSlower = true;
    const auto destroyedProgress = progressPlayer.OnProgress(
        0.25F, false, source::Player::cheatEnableFaster,
        0U, 1U, progressViews);
    if (destroyedProgress.restore != source::PlayerRestoreStep::None ||
        progressPlayer.car.cheatFaster ||
        progressPlayer.car.cheatSlower)
        return 32;

    source::Player bonusProjectilePlayer;
    bonusProjectilePlayer.Reset(80.0F, 1U);
    if (bonusProjectilePlayer.GetNextBonusProjectileId() != 1U ||
        bonusProjectilePlayer.HasBonusProjectile(1U))
        return 35;
    bonusProjectilePlayer.InsertBonusProjectile(7U);
    if (bonusProjectilePlayer.GetNextBonusProjectileId() != 8U ||
        !bonusProjectilePlayer.HasBonusProjectile(7U) ||
        !bonusProjectilePlayer.RemoveBonusProjectile(7U) ||
        bonusProjectilePlayer.HasBonusProjectile(7U) ||
        bonusProjectilePlayer.GetNextBonusProjectileId() != 8U)
        return 36;
    bonusProjectilePlayer.InsertBonusProjectile(12U);
    bonusProjectilePlayer.Disconnect();
    if (bonusProjectilePlayer.HasBonusProjectile(12U))
        return 37;
    player.ApplyRaceReward();
    if (player.GetMoney() != 119U || player.GetPoints() != 20U)
        return 10;

    player.Destroy();
    if (!player.IsDestroyed() || player.GetLife() != 0.0F ||
        player.HasCar() ||
        player.HasAttachedLights() ||
        player.ProgressRestore(1.0F) != source::PlayerRestoreStep::None ||
        player.ProgressRestore(1.0F) !=
            source::PlayerRestoreStep::QueueRespawn ||
        player.GetLife() != player.GetMaxLife() ||
        !player.IsDestroyed() ||
        player.ProgressRestore(0.01F) !=
            source::PlayerRestoreStep::ActivateCar ||
        player.IsDestroyed() || !player.HasCar() ||
        !player.HasAttachedLights())
        return 11;

    if (source::Player::RoundedRandomIndex(4U, 0.16F) != 0U ||
        source::Player::RoundedRandomIndex(4U, 0.5F) != 2U ||
        source::Player::BonusCharge(3U, 0.5F) != 1U ||
        source::Player::BonusCharge(10U, 0.0F) != 1U)
        return 12;

    source::Player lethalPlayer;
    lethalPlayer.Reset(50.0F, 1U);
    lethalPlayer.Damage(
        2U, 60.0F, r3d::game::originalrace::DamageType::Simple);
    const auto lethalEvents = lethalPlayer.TakeGameEvents();
    if (lethalEvents.size() != 3U ||
        lethalEvents[0].kind != source::PlayerGameEventKind::Damage ||
        lethalEvents[1].kind != source::PlayerGameEventKind::Kill ||
        lethalEvents[2].kind != source::PlayerGameEventKind::Death ||
        lethalEvents[1].otherPlayerId != 2U)
        return 24;

    source::Player minePlayer;
    minePlayer.Reset(50.0F, 1U);
    minePlayer.Damage(
        3U, 60.0F, r3d::game::originalrace::DamageType::Mine);
    const auto mineEvents = minePlayer.TakeGameEvents();
    if (mineEvents.size() != 3U ||
        mineEvents[0].kind != source::PlayerGameEventKind::Damage ||
        mineEvents[1].kind != source::PlayerGameEventKind::DeathMine ||
        mineEvents[2].kind != source::PlayerGameEventKind::Death)
        return 25;

    source::Player overboardPlayer;
    overboardPlayer.Reset(50.0F, 1U);
    overboardPlayer.Damage(
        4U, 0.0F, r3d::game::originalrace::DamageType::Touch);
    overboardPlayer.TakeGameEvents();
    overboardPlayer.Death(
        r3d::game::originalrace::DamageType::DeathPlane);
    const auto overboardEvents = overboardPlayer.TakeGameEvents();
    if (overboardEvents.size() != 2U ||
        overboardEvents[0].kind !=
            source::PlayerGameEventKind::Overboard ||
        overboardEvents[1].kind != source::PlayerGameEventKind::Death ||
        overboardEvents[1].otherPlayerId != 4U)
        return 26;

    source::Trace trace(4U);
    auto* first = trace.AddPoint(1U);
    first->SetPos({0.0F, 0.0F, 0.0F});
    first->SetSize(30.0F);
    auto* second = trace.AddPoint(2U);
    second->SetPos({100.0F, 0.0F, 0.0F});
    second->SetSize(30.0F);
    auto* third = trace.AddPoint(3U);
    third->SetPos({200.0F, 0.0F, 0.0F});
    third->SetSize(30.0F);
    auto* branchMiddle = trace.AddPoint(4U);
    branchMiddle->SetPos({100.0F, 100.0F, 0.0F});
    branchMiddle->SetSize(30.0F);
    auto* mainPath = trace.AddPath();
    mainPath->Add(first);
    mainPath->Add(second);
    mainPath->Add(third);
    auto* branchPath = trace.AddPath();
    branchPath->Add(second);
    branchPath->Add(branchMiddle);
    branchPath->Add(third);

    source::Player carLifecyclePlayer;
    carLifecyclePlayer.Reset(100.0F, 1U, &trace);
    carLifecyclePlayer.InsertBonusProjectile(9U);
    if (carLifecyclePlayer.car.GetMapPos().x != 0.0F ||
        carLifecyclePlayer.car.GetMapPos().y != 0.0F)
        return 38;
    carLifecyclePlayer.CreateCar(true);
    if (carLifecyclePlayer.car.GetLastNode() != mainPath->GetFirst() ||
        carLifecyclePlayer.car.GetLiveTile() != mainPath->GetFirst() ||
        carLifecyclePlayer.HasBonusProjectile(9U) ||
        carLifecyclePlayer.GetNextBonusProjectileId() != 1U)
        return 39;
    carLifecyclePlayer.car.numLaps = 3U;
    carLifecyclePlayer.FreeCar(true);
    if (carLifecyclePlayer.car.GetLastNode() != nullptr ||
        carLifecyclePlayer.car.GetLiveTile() != nullptr ||
        carLifecyclePlayer.car.numLaps != 0U ||
        carLifecyclePlayer.car.GetMapPos().x != 0.0F ||
        carLifecyclePlayer.car.GetMapPos().y != 0.0F)
        return 40;

    source::Player tracedPlayer;
    tracedPlayer.Reset(100.0F, 1U, &trace);
    const auto firstUpdate = tracedPlayer.car.Update(
        trace, {40.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
        10.0F, 1.0F / 60.0F);
    const auto firstMap = tracedPlayer.car.GetMapPos();
    if (!firstUpdate.lastNodeChanged ||
        firstUpdate.previousLast.valid() ||
        firstUpdate.lastNode.path != 0U ||
        tracedPlayer.car.GetPathIndex() != 0 ||
        std::abs(firstMap.x - 40.0F) > 0.001F)
        return 13;

    source::Player planeNearPlayer;
    source::Player forwardPlayer;
    source::Player backPlayer;
    source::Player lowerLevelPlayer;
    planeNearPlayer.Reset(100.0F, 2U, &trace);
    forwardPlayer.Reset(100.0F, 3U, &trace);
    backPlayer.Reset(100.0F, 4U, &trace);
    lowerLevelPlayer.Reset(100.0F, 5U, &trace);
    planeNearPlayer.car.Update(
        trace, {50.0F, 100.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
        0.0F, 1.0F / 60.0F);
    forwardPlayer.car.Update(
        trace, {80.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
        0.0F, 1.0F / 60.0F);
    backPlayer.car.Update(
        trace, {0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
        0.0F, 1.0F / 60.0F);
    lowerLevelPlayer.car.Update(
        trace, {60.0F, 0.0F, -1000.0F}, {1.0F, 0.0F, 0.0F},
        0.0F, 1.0F / 60.0F);
    std::vector<source::Player*> enemyPlayers{
        &tracedPlayer, &planeNearPlayer, &forwardPlayer, &backPlayer};
    if (tracedPlayer.FindClosestEnemy(
            1.57079632679489661923F, false, enemyPlayers) !=
            &planeNearPlayer ||
        tracedPlayer.FindClosestEnemy(
            -0.78539816339744830962F, false, enemyPlayers) !=
            &backPlayer)
    {
        return 33;
    }
    enemyPlayers = {&tracedPlayer, &lowerLevelPlayer};
    if (tracedPlayer.FindClosestEnemy(0.0F, false, enemyPlayers) !=
            &lowerLevelPlayer ||
        tracedPlayer.FindClosestEnemy(0.0F, true, enemyPlayers) != nullptr)
    {
        return 34;
    }

    std::vector<source::Player::CheatPlayerView> cheatPlayers{
        {0U, true, true, tracedPlayer.car.GetLap() + 0.25F},
        {1U, false, true, tracedPlayer.car.GetLap() + 0.49F}};
    const auto faster = tracedPlayer.CheatUpdate(
        source::Player::cheatEnableFaster |
            source::Player::cheatEnableSlower,
        2U, 1U, cheatPlayers);
    if (!faster.faster || faster.slower ||
        faster.torqueScale <= 1.0F ||
        !tracedPlayer.car.cheatFaster)
        return 28;

    tracedPlayer.car.Update(
        trace, {40.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
        40.0F, 1.0F / 60.0F);
    cheatPlayers[0].lap = tracedPlayer.car.GetLap() - 0.25F;
    const auto slower = tracedPlayer.CheatUpdate(
        source::Player::cheatEnableFaster |
            source::Player::cheatEnableSlower,
        2U, 1U, cheatPlayers);
    if (!slower.slower || slower.faster ||
        !tracedPlayer.car.cheatSlower)
        return 29;

    cheatPlayers[0].lap = tracedPlayer.car.GetLap() + 0.05F;
    cheatPlayers[1].lap = tracedPlayer.car.GetLap() + 0.49F;
    const auto computersExcluded = tracedPlayer.CheatUpdate(
        source::Player::cheatEnableFaster,
        2U, 1U, cheatPlayers);
    if (computersExcluded.faster || tracedPlayer.car.cheatFaster)
        return 30;

    tracedPlayer.car.Update(
        trace, {75.0F, 0.0F, 0.0F}, {-1.0F, 0.0F, 0.0F},
        10.0F, 1.0F / 60.0F);
    const auto inverse = tracedPlayer.car.Update(
        trace, {50.0F, 0.0F, 0.0F}, {-1.0F, 0.0F, 0.0F},
        10.0F, 1.0F / 60.0F);
    if (!inverse.moveInverseStarted || !tracedPlayer.car.moveInverse)
        return 14;
    tracedPlayer.car.Update(
        trace, {55.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
        100.0F, 1.0F / 60.0F);
    const auto lostControl = tracedPlayer.car.Update(
        trace, {60.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
        10.0F, 1.0F / 60.0F);
    if (tracedPlayer.car.moveInverse || !lostControl.lostControl)
        return 15;

    const auto branch = tracedPlayer.car.Update(
        trace, {100.0F, 50.0F, 0.0F}, {0.0F, 1.0F, 0.0F},
        10.0F, 1.0F / 60.0F);
    if (!branch.lastNodeChanged || branch.lastNode.path != 1U ||
        tracedPlayer.car.GetPathIndex() != 1)
        return 16;
    const auto lastMap = tracedPlayer.car.GetMapPos();
    tracedPlayer.car.Update(
        trace, {1000.0F, 1000.0F, 0.0F}, {1.0F, 0.0F, 0.0F},
        10.0F, 1.0F / 60.0F);
    const auto retainedMap = tracedPlayer.car.GetMapPos();
    if (tracedPlayer.car.GetLiveTileRef().valid() ||
        !tracedPlayer.car.GetLastNodeRef().valid() ||
        std::abs(lastMap.x - retainedMap.x) > 0.001F ||
        std::abs(lastMap.y - retainedMap.y) > 0.001F)
        return 17;

    source::Trace resetTrace(4U);
    auto* resetFirst = resetTrace.AddPoint(11U);
    resetFirst->SetPos({0.0F, 0.0F, 0.0F});
    resetFirst->SetSize(8.0F);
    auto* resetSecond = resetTrace.AddPoint(12U);
    resetSecond->SetPos({100.0F, 0.0F, 0.0F});
    resetSecond->SetSize(24.0F);
    auto* resetThird = resetTrace.AddPoint(13U);
    resetThird->SetPos({200.0F, 0.0F, 0.0F});
    resetThird->SetSize(8.0F);
    auto* resetPath = resetTrace.AddPath();
    resetPath->Add(resetFirst);
    resetPath->Add(resetSecond);
    resetPath->Add(resetThird);

    source::Player resetPlayer;
    resetPlayer.Reset(100.0F, 1U, &resetTrace);
    resetPlayer.car.Update(
        resetTrace, {75.0F, 0.0F, 2.0F}, {1.0F, 0.0F, 0.0F},
        10.0F, 1.0F / 60.0F);
    std::vector<source::TraceVec3> resetRays;
    const auto resetPose = resetPlayer.ResetCar(
        [&](const source::TraceVec3& origin) {
            resetRays.push_back(origin);
            return source::ResetCarRayKind::TrackPlane;
        });
    // ResetCar uses the retained coordinate and the midpoint trace height,
    // not the height at each of the 0/-2/+2 longitudinal samples.
    if (!resetPose.valid || resetPose.node.path != 0U ||
        resetPose.node.node != 0U || resetRays.size() != 3U ||
        std::abs(resetPose.position.x - 75.0F) > 0.001F ||
        std::abs(resetPose.position.z - 4.0F) > 0.001F ||
        std::abs(resetRays[1].x - 73.0F) > 0.001F ||
        std::abs(resetRays[2].x - 77.0F) > 0.001F)
        return 23;

    std::size_t blockedRays = 0U;
    const auto blockedPose = resetPlayer.ResetCar(
        [&](const source::TraceVec3&) {
            ++blockedRays;
            return source::ResetCarRayKind::Blocked;
        });
    // Every attempted location is blocked. Windows retains the original
    // first fallback; it does not return the last failed six-metre step.
    if (!blockedPose.valid || blockedRays != 5U ||
        std::abs(blockedPose.position.x - 75.0F) > 0.001F)
        return 24;

    source::Player deathPlaneResetPlayer;
    deathPlaneResetPlayer.Reset(100.0F, 1U, &resetTrace);
    std::size_t deathPlaneRays = 0U;
    const auto deathPlanePose = deathPlaneResetPlayer.ResetCar(
        [&](const source::TraceVec3&) {
            return deathPlaneRays++ == 0U
                       ? source::ResetCarRayKind::DeathPlane
                       : source::ResetCarRayKind::TrackPlane;
        });
    if (!deathPlanePose.valid || deathPlaneRays != 4U ||
        deathPlanePose.node.node != 0U ||
        std::abs(deathPlanePose.position.x) > 0.001F ||
        std::abs(deathPlanePose.position.z - 4.0F) > 0.001F)
        return 25;

    player.Disconnect();
    if (!player.disconnected || !player.IsDestroyed() ||
        player.GetFinished() ||
        player.GetLife() != 0.0F)
        return 18;

    std::cout << "original Player source rules passed\n";
    return 0;
}
