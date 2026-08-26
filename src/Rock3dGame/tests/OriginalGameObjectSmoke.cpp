#include "OriginalGameObject.h"
#include "OriginalLogic.h"
#include "OriginalMapObj.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace original = r3d::game::originalrace;
namespace source = r3d::game::originalrace::source;

namespace
{

struct TrackingListener final : source::GameObjectListener
{
    std::vector<int> order;
    float lifeAtDamage = 0.0F;
    source::GameObject* deathTarget = nullptr;

    void OnDestroy(source::GameObject&) noexcept override
    {
        order.push_back(4);
    }

    void OnDeath(
        source::GameObject&, original::DamageType,
        source::GameObject* target) noexcept override
    {
        order.push_back(2);
        deathTarget = target;
    }

    void OnDamage(
        source::GameObject& sender, float,
        original::DamageType) noexcept override
    {
        order.push_back(1);
        lifeAtDamage = sender.GetLife();
    }

    void OnLowLife(source::GameObject&) noexcept override
    {
        order.push_back(3);
    }
};

} // namespace

int main()
{
    source::Logic firstLogic;
    source::Logic secondLogic;
    source::GameObject graphParent;
    source::GameObject graphChild;
    graphParent.SetLogic(&firstLogic);
    graphChild.SetParent(&graphParent);
    if (graphChild.GetParent() != &graphParent ||
        graphParent.GetChildren().size() != 1U ||
        graphParent.GetChildren().front() != &graphChild ||
        graphChild.GetLogic() != &firstLogic)
        return 59;
    auto& included = graphChild.GetIncludeList().Add(
        source::GameObjType::GameObj, "nestedEffect");
    if (included.GetParent() != &graphChild ||
        included.GetGameObj().GetLogic() != &firstLogic ||
        graphChild.GetChildren().size() != 1U)
        return 60;
    graphParent.SetLogic(&secondLogic);
    if (graphChild.GetLogic() != &secondLogic ||
        included.GetGameObj().GetLogic() != &secondLogic)
        return 61;
    graphChild.GetIncludeList().Clear();
    graphChild.SetParent(nullptr);
    if (!graphChild.GetChildren().empty() ||
        !graphParent.GetChildren().empty() ||
        graphChild.GetParent() != nullptr ||
        graphChild.GetLogic() != &secondLogic)
        return 62;

    source::GameObject object;
    object.ResetGameObject(100.0F);
    if (object.GetLife() != 100.0F || object.IsImmortal() ||
        object.GetLiveState() != source::GameObject::LiveState::Live)
        return 1;

    const auto ordinary = object.Damage(
        2U, 25.0F, original::DamageType::Energy);
    if (!ordinary.wasLive || ordinary.death ||
        ordinary.killCredit || object.GetLife() != 75.0F)
        return 2;

    object.Immortal(1.0F);
    const auto immortal = object.Damage(
        3U, 500.0F, original::DamageType::Simple);
    if (immortal.death || object.GetLife() != 75.0F ||
        !object.IsImmortal())
        return 3;
    if (!object.OnProgress(1.0F).immortalityEnded ||
        object.IsTimedImmortal())
        return 4;

    const auto touch = object.Damage(
        7U, 0.0F, original::DamageType::Touch);
    if (touch.death || object.GetTouchPlayerId() != 7U ||
        std::abs(object.touchAttributionSeconds - 3.0F) > 0.0001F)
        return 5;
    object.OnProgress(2.999F);
    if (object.GetTouchPlayerId() != 7U)
        return 6;
    if (!object.OnProgress(0.001F).touchAttributionEnded ||
        object.GetTouchPlayerId() != source::GameObject::undefinedPlayerId)
        return 7;

    object.SetImmortalFlag(true);
    object.Damage(1U, 500.0F, original::DamageType::Simple);
    if (object.GetLife() != 75.0F)
        return 8;
    object.SetImmortalFlag(false);

    const auto lethal = object.Damage(
        4U, 100.0F, original::DamageType::Simple);
    if (!lethal.death || !lethal.killCredit ||
        object.GetLiveState() != source::GameObject::LiveState::Death ||
        object.GetLife() != -25.0F)
        return 9;
    const auto replicated = object.Damage(
        5U, 1.0F, -30.0F, true, original::DamageType::Mine);
    if (replicated.wasLive || object.GetLife() != -30.0F)
        return 10;
    if (!object.Resc() || object.GetLife() != 100.0F ||
        object.GetTimeLife() != 0.0F)
        return 11;

    object.SetLife(90.0F);
    object.Healt(50.0F);
    if (object.GetLife() != 100.0F)
        return 12;
    object.SetMaxTimeLife(0.5F);
    if (object.OnProgress(0.5F).lifetimeDeath ||
        !object.OnProgress(0.001F).lifetimeDeath)
        return 13;

    source::GameObjectFrameSync frameSync;
    constexpr float halfQuarterTurn =
        0.70710678118654752440F;
    const source::GameObjectFrameSync::Pose targetPose{
        {4.1F, 0.0F, 0.0F},
        {0.0F, 0.0F, halfQuarterTurn, halfQuarterTurn}};
    const auto correction = frameSync.OnNetworkPose(
        {}, {}, {}, targetPose.position, targetPose.rotation);
    const auto firstGraphPose = frameSync.OnFrame(targetPose, 0.0F);
    if (!correction.snapPosition || !correction.snapRotation ||
        std::abs(firstGraphPose.position.x) > 0.0001F ||
        std::abs(firstGraphPose.rotation.z) > 0.0001F ||
        std::abs(firstGraphPose.rotation.w - 1.0F) > 0.0001F)
        return 49;
    const auto progressingGraphPose =
        frameSync.OnFrame(targetPose, 0.2F);
    if (std::abs(progressingGraphPose.position.x - 1.0F) > 0.0001F ||
        progressingGraphPose.rotation.z <= 0.2F ||
        progressingGraphPose.rotation.z >= halfQuarterTurn)
        return 50;
    const auto completedGraphPose =
        frameSync.OnFrame(targetPose, 1.0F);
    if (std::abs(completedGraphPose.position.x - 4.1F) > 0.0001F ||
        std::abs(completedGraphPose.rotation.z - halfQuarterTurn) >
            0.0001F ||
        frameSync.HasActiveCorrection())
        return 51;

    frameSync.Reset();
    const source::GameObjectFrameSync::Pose largeSnapPose{
        {6.0F, 0.0F, 0.0F}, {}};
    if (!frameSync.OnNetworkPose(
             {}, {}, {}, largeSnapPose.position,
             largeSnapPose.rotation).snapPosition ||
        std::abs(
            frameSync.OnFrame(largeSnapPose, 0.0F).position.x -
            6.0F) > 0.0001F)
        return 52;

    frameSync.Reset();
    frameSync.SetPosSync2(
        {3.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F});
    const source::GameObjectFrameSync::Pose originPose{};
    if (std::abs(
            frameSync.OnFrame(originPose, 0.0F).position.x - 3.0F) >
            0.0001F ||
        std::abs(
            frameSync.OnFrame(originPose, 0.2F).position.x - 2.0F) >
            0.0001F ||
        std::abs(
            frameSync.OnFrame(originPose, 0.2F).position.x - 1.0F) >
            0.0001F)
        return 53;

    source::GameObject listened;
    listened.ResetGameObject(20.0F);
    TrackingListener listener;
    if (!listened.InsertListener(&listener) ||
        listened.InsertListener(&listener) ||
        listened.GetListenerCount() != 1U)
        return 54;
    listened.Damage(1U, 5.0F, original::DamageType::Energy);
    if (listener.order != std::vector<int>{1} ||
        std::abs(listener.lifeAtDamage - 15.0F) > 0.0001F)
        return 55;
    listened.Damage(1U, 20.0F, original::DamageType::Simple);
    if (listener.order != std::vector<int>({1, 1, 2}) ||
        listener.deathTarget != nullptr)
        return 56;
    listened.Resc();
    listened.LowLife();
    source::GameObject deathTarget;
    if (!listened.Death(
            original::DamageType::DeathPlane, &deathTarget) ||
        listener.order != std::vector<int>({1, 1, 2, 3, 2}) ||
        listener.deathTarget != &deathTarget ||
        !listened.DestroyObject() || listened.DestroyObject() ||
        listener.order.back() != 4)
        return 57;
    if (!listened.RemoveListener(&listener) ||
        listened.RemoveListener(&listener) ||
        listened.GetListenerCount() != 0U)
        return 58;

    source::DestrObj destructible;
    destructible.ResetGameObject(10.0F);
    if (destructible.Damage(
            2U, 10.0F, original::DamageType::Energy).death == false ||
        !destructible.HasPendingDestruction())
        return 14;
    if (!destructible.OnProgress(0.0F) ||
        destructible.HasPendingDestruction() ||
        destructible.OnProgress(0.0F))
        return 15;
    destructible.Resc();
    if (!destructible.Death(original::DamageType::DeathPlane) ||
        !destructible.HasPendingDestruction() ||
        !destructible.OnProgress(0.0F))
        return 31;

    source::GameObject effectOwner;
    effectOwner.ResetGameObject(100.0F);
    source::LowLifePoints lowLife;
    effectOwner.SetLife(34.0F);
    const auto lowLifeStart = lowLife.OnProgress(effectOwner, 0.1F);
    if (!lowLifeStart.activated || !lowLife.IsEffectMaked() ||
        std::abs(lowLife.GetEffectSeconds() - 0.1F) > 0.0001F)
        return 16;
    if (lowLife.OnProgress(effectOwner, 0.1F).activated)
        return 17;
    effectOwner.Healt(100.0F);
    if (!lowLife.OnProgress(effectOwner, 0.1F).released ||
        lowLife.IsEffectMaked())
        return 18;

    source::DamageEffect energyDamage(original::DamageType::Energy);
    if (energyDamage.OnDamage(original::DamageType::Simple) ||
        !energyDamage.OnDamage(original::DamageType::Energy) ||
        energyDamage.OnDamage(original::DamageType::Energy))
        return 19;
    energyDamage.OnProgress(0.5F);
    if (!energyDamage.IsEffectMaked())
        return 20;
    energyDamage.OnProgress(0.001F);
    if (energyDamage.IsEffectMaked())
        return 21;

    source::ImmortalEffect shieldEffect;
    shieldEffect.OnImmortalStatus(true);
    if (!shieldEffect.IsEffectMaked() ||
        shieldEffect.GetFadeInTime() != 0.0F ||
        shieldEffect.GetScale() != 0.0F)
        return 22;
    shieldEffect.OnProgress(0.1F);
    shieldEffect.OnDamage();
    if (std::abs(shieldEffect.GetScale() - 0.2F) > 0.0001F ||
        std::abs(shieldEffect.GetDamageAlpha() - 3.5F) > 0.0001F)
        return 23;
    shieldEffect.OnImmortalStatus(false);
    shieldEffect.OnProgress(0.25F);
    if (std::abs(shieldEffect.GetScale() - 0.5F) > 0.0001F)
        return 24;
    shieldEffect.OnProgress(0.25F);
    if (shieldEffect.IsEffectMaked() ||
        shieldEffect.GetEffectSeconds() != 0.0F)
        return 25;

    source::SlowEffect slowEffect;
    if (!slowEffect.Attach(1.0F, 4U, 2U) ||
        slowEffect.Attach(5.0F, 7U, 3U) ||
        slowEffect.GetWeapon() != 4U ||
        slowEffect.GetProjectile() != 2U)
        return 26;
    if (!slowEffect.OnProgress(0.25F, 30.0F).limitSpeed ||
        slowEffect.OnProgress(0.25F, 15.0F).limitSpeed ||
        std::abs(slowEffect.GetRemainingSeconds() - 0.5F) > 0.0001F)
        return 27;
    if (slowEffect.OnProgress(0.5F, 30.0F).released ||
        !slowEffect.IsEffectMaked())
        return 28;
    const auto slowReleased = slowEffect.OnProgress(0.001F, 30.0F);
    if (!slowReleased.limitSpeed || !slowReleased.released ||
        slowEffect.IsEffectMaked())
        return 29;

    source::GameObject falling;
    falling.ResetGameObject(10.0F);
    falling.Damage(8U, 0.0F, original::DamageType::Touch);
    source::TouchDeath touchDeath;
    if (!touchDeath.OnContact(&falling) || !falling.destroyed ||
        falling.GetTouchPlayerId() != 8U ||
        touchDeath.OnContact(&falling))
        return 30;

    source::GameObject particleOwner;
    particleOwner.ResetGameObject(-1.0F);
    source::FxSystemWaitingEnd waitingEnd;
    if (!particleOwner.Death() ||
        !waitingEnd.OnDeath(particleOwner).beginFading ||
        particleOwner.destroyed || !waitingEnd.IsResurrect())
        return 32;
    if (waitingEnd.OnDeath(particleOwner).beginFading ||
        waitingEnd.OnProgress(particleOwner, 3U).finalDeath ||
        particleOwner.destroyed)
        return 33;
    if (!waitingEnd.OnProgress(particleOwner, 0U).finalDeath ||
        !particleOwner.destroyed ||
        waitingEnd.OnProgress(particleOwner, 0U).finalDeath)
        return 34;
    waitingEnd.Reset();
    if (waitingEnd.IsResurrect() || waitingEnd.IsFading())
        return 35;

    source::EventEffect eventEffect;
    if (!eventEffect.MakeEffect() || eventEffect.MakeEffect() ||
        !eventEffect.IsEffectMaked() || !eventEffect.FreeEffect() ||
        eventEffect.FreeEffect() || eventEffect.IsEffectMaked())
        return 36;
    eventEffect.MakeEffect();
    if (!eventEffect.OnDestroyEffect() ||
        eventEffect.OnDestroyEffect())
        return 37;

    source::LifeEffect lifeEffect;
    if (lifeEffect.OnProgress(false) || lifeEffect.HasPlayed() ||
        !lifeEffect.OnProgress(true) || !lifeEffect.HasPlayed() ||
        lifeEffect.OnProgress(true))
        return 38;
    lifeEffect.Reset();
    if (lifeEffect.HasPlayed() || !lifeEffect.OnProgress(true))
        return 39;

    source::DeathEffect deathEffect(true, true);
    if (deathEffect.OnDeath(false, true, true).createEffect ||
        deathEffect.IsEffectMaked())
        return 40;
    const auto attachedDeath = deathEffect.OnDeath(true, true, true);
    if (!attachedDeath.createEffect || !attachedDeath.targetChild ||
        !attachedDeath.ignoreSenderCar ||
        !deathEffect.IsEffectMaked())
        return 41;
    if (deathEffect.OnDeath(true, true, true).createEffect)
        return 42;
    deathEffect.OnDestroyEffect();
    const auto worldDeath = deathEffect.OnDeath(true, false, false);
    if (!worldDeath.createEffect || worldDeath.targetChild ||
        worldDeath.ignoreSenderCar)
        return 43;
    deathEffect.Reset(false, false);
    if (deathEffect.GetEffectPxIgnoreSenderCar() ||
        deathEffect.GetTargetChild() || deathEffect.IsEffectMaked())
        return 44;

    source::FxSystemSrcSpeed sourceSpeed;
    if (sourceSpeed.OnProgress(false, {3.0F, 4.0F, 5.0F}))
        return 45;
    const source::FxSystemSrcSpeed::ParentTransform speedParent{
        {2.0F, 4.0F, 5.0F},
        {0.0F, 0.0F, 0.70710678F, 0.70710678F}};
    if (!sourceSpeed.OnProgress(
            true, {0.0F, 4.0F, 10.0F}, &speedParent))
        return 46;
    const auto localSourceSpeed = sourceSpeed.GetSourceSpeed();
    if (std::abs(localSourceSpeed.x - 2.0F) > 0.0001F ||
        std::abs(localSourceSpeed.y) > 0.0001F ||
        std::abs(localSourceSpeed.z - 2.0F) > 0.0001F)
        return 47;
    if (sourceSpeed.OnProgress(false, {0.0F, 0.0F, 0.0F}) ||
        std::abs(sourceSpeed.GetSourceSpeed().x - 2.0F) > 0.0001F)
        return 48;
    sourceSpeed.Reset();
    if (sourceSpeed.GetSourceSpeed().x != 0.0F ||
        sourceSpeed.GetSourceSpeed().y != 0.0F ||
        sourceSpeed.GetSourceSpeed().z != 0.0F)
        return 49;

    std::cout << "original GameObject/DestrObj/effect behavior source "
                 "rules passed\n";
    return 0;
}
