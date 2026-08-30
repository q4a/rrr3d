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

struct RemovingDestroyListener final : source::GameObjectListener
{
    source::GameObjectListener* remove = nullptr;
    bool called = false;

    void OnDestroy(source::GameObject& sender) noexcept override
    {
        called = true;
        sender.RemoveListener(remove);
    }
};

struct TrackingBehavior final : source::Behavior
{
    TrackingBehavior(source::Behaviors* owner, int identifier,
                     std::vector<int>* events, bool removeOnProgress)
        : Behavior(owner), identifier(identifier), events(events),
          removeOnProgress(removeOnProgress)
    {
    }

    void OnProgress(float) noexcept override
    {
        events->push_back(identifier);
        if (removeOnProgress)
            Remove();
    }

    void OnDamage(source::GameObject&, float,
                  original::DamageType) noexcept override
    {
        events->push_back(identifier + 10);
    }

protected:
    void OnShot(const std::array<float, 3U>&) noexcept override
    {
        events->push_back(identifier + 20);
    }

    void OnMotor(float, float, float, float) noexcept override
    {
        events->push_back(identifier + 30);
    }

    void OnImmortalStatus(bool status) noexcept override
    {
        events->push_back(identifier + (status ? 40 : 50));
    }

private:
    int identifier = 0;
    std::vector<int>* events = nullptr;
    bool removeOnProgress = false;
};

struct EventRegistrationProbe final : source::GameObject
{
    void AddFrame() { RegFrameEvent(); }
    void DropFrame() { UnregFrameEvent(); }
    void AddProgress() { RegProgressEvent(); }
    void DropProgress() { UnregProgressEvent(); }
    void AddLate() { RegLateProgressEvent(); }
    void DropLate() { UnregLateProgressEvent(); }
    void AddFixed() { RegFixedStepEvent(); }
    void DropFixed() { UnregFixedStepEvent(); }

    int fixedCalls = 0;
    int lateCalls = 0;
    int frameCalls = 0;

protected:
    void OnFixedStep(float) noexcept override { ++fixedCalls; }
    void OnLateProgress(float, bool) noexcept override { ++lateCalls; }
    void OnFrame(float, float) noexcept override { ++frameCalls; }
};

} // namespace

int main()
{
    if (std::string(source::BehaviorTypeName(
            source::BehaviorType::ShotEffect)) != "btShotEffect" ||
        std::string(source::BehaviorTypeName(
            source::BehaviorType::PodushkaAnim)) != "btPodushkaAnim")
        return 65;

    source::GameObject behaviorOwner;
    behaviorOwner.ResetGameObject(20.0F);
    auto& timedInclude = behaviorOwner.GetIncludeList().Add(
        source::GameObjType::GameObj, "timedInclude");
    timedInclude.GetGameObj().ResetGameObject(-1.0F);
    timedInclude.GetGameObj().SetMaxTimeLife(0.01F);
    std::vector<int> behaviorEvents;
    auto& firstBehavior =
        behaviorOwner.GetBehaviors().Add<TrackingBehavior>(
            source::BehaviorType::DamageEffect, 1,
            &behaviorEvents, true);
    auto& secondBehavior =
        behaviorOwner.GetBehaviors().Add<TrackingBehavior>(
            source::BehaviorType::LifeEffect, 2,
            &behaviorEvents, false);
    if (firstBehavior.GetGameObj() != &behaviorOwner ||
        firstBehavior.GetLogic() != nullptr ||
        behaviorOwner.GetBehaviors().Find(
            source::BehaviorType::DamageEffect) != &firstBehavior ||
        behaviorOwner.GetBehaviors().GetCount() != 2U ||
        behaviorOwner.GetListenerCount() != 2U)
        return 66;
    behaviorOwner.Damage(0U, 1.0F, original::DamageType::Energy);
    behaviorOwner.GetBehaviors().OnShot({1.0F, 2.0F, 3.0F});
    behaviorOwner.GetBehaviors().OnMotor(0.1F, 1000.0F, 500.0F, 5000.0F);
    behaviorOwner.Immortal(1.0F);
    if (behaviorEvents !=
        std::vector<int>({11, 12, 21, 22, 31, 32, 41, 42}))
        return 67;
    behaviorEvents.clear();
    const auto firstBehaviorProgress = behaviorOwner.OnProgress(0.5F);
    if (firstBehaviorProgress.includedProgressed != 1U ||
        firstBehaviorProgress.includedRemoved != 1U ||
        firstBehaviorProgress.behaviorsProgressed != 2U ||
        firstBehaviorProgress.behaviorsRemoved != 0U ||
        behaviorEvents != std::vector<int>({1, 2}) ||
        behaviorOwner.GetIncludeList().GetLiveCount() != 0U ||
        behaviorOwner.GetBehaviors().GetCount() != 2U)
        return 68;
    behaviorEvents.clear();
    const auto secondBehaviorProgress = behaviorOwner.OnProgress(0.5F);
    if (!secondBehaviorProgress.immortalityEnded ||
        secondBehaviorProgress.behaviorsProgressed != 1U ||
        secondBehaviorProgress.behaviorsRemoved != 1U ||
        behaviorEvents != std::vector<int>({51, 52, 2}) ||
        behaviorOwner.GetBehaviors().GetCount() != 1U ||
        behaviorOwner.GetListenerCount() != 1U ||
        !behaviorOwner.GetBehaviors().Delete(&secondBehavior) ||
        behaviorOwner.GetBehaviors().GetCount() != 0U ||
        behaviorOwner.GetListenerCount() != 0U)
        return 69;

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

    source::GameObject assignSource;
    assignSource.ResetGameObject(75.0F);
    assignSource.SetLife(25.0F);
    assignSource.SetName("sourceName");
    assignSource.SetLogic(&firstLogic);
    source::GameObject assignTarget;
    assignTarget.ResetGameObject(10.0F);
    assignTarget.SetName("targetName");
    assignTarget.AssignSource(assignSource);
    if (assignTarget.GetLogic() != &firstLogic ||
        assignTarget.GetLife() != 10.0F ||
        assignTarget.GetName() != "targetName")
        return 63;

    source::WorldEventPump firstEventWorld;
    source::WorldEventPump secondEventWorld;
    source::Logic firstEventLogic;
    source::Logic secondEventLogic;
    firstEventLogic.AttachWorld(&firstEventWorld);
    secondEventLogic.AttachWorld(&secondEventWorld);
    EventRegistrationProbe eventProbe;
    eventProbe.AddFixed();
    eventProbe.AddFixed();
    eventProbe.AddLate();
    eventProbe.AddFrame();
    eventProbe.AddProgress();
    eventProbe.SetLogic(&firstEventLogic);
    if (eventProbe.GetFixedStepEventCount() != 2U ||
        eventProbe.GetLateProgressEventCount() != 1U ||
        eventProbe.GetFrameEventCount() != 1U ||
        eventProbe.GetProgressEventCount() != 1U ||
        firstEventWorld.FixedStepEventCount() != 1U ||
        firstEventWorld.LateProgressEventCount() != 1U ||
        firstEventWorld.FrameEventCount() != 1U)
        return 101;
    if (!firstEventWorld.DispatchFixedStepEvent(&eventProbe, 0.01F) ||
        !firstEventWorld.DispatchFrameEvent(&eventProbe, 0.01F, 0.5F) ||
        eventProbe.fixedCalls != 1 || eventProbe.frameCalls != 1)
        return 102;
    firstEventWorld.LateProgress(0.01F, true);
    if (eventProbe.lateCalls != 1)
        return 103;
    eventProbe.SetLogic(&secondEventLogic);
    if (firstEventWorld.FixedStepEventCount() != 0U ||
        firstEventWorld.LateProgressEventCount() != 0U ||
        firstEventWorld.FrameEventCount() != 0U ||
        !secondEventWorld.HasFixedStepEvent(&eventProbe) ||
        !secondEventWorld.HasLateProgressEvent(&eventProbe) ||
        !secondEventWorld.HasFrameEvent(&eventProbe))
        return 104;
    eventProbe.DropFixed();
    if (!secondEventWorld.HasFixedStepEvent(&eventProbe) ||
        eventProbe.GetFixedStepEventCount() != 1U)
        return 105;
    eventProbe.DropFixed();
    eventProbe.DropLate();
    eventProbe.DropFrame();
    eventProbe.DropProgress();
    if (secondEventWorld.FixedStepEventCount() != 0U ||
        secondEventWorld.LateProgressEventCount() != 0U ||
        secondEventWorld.FrameEventCount() != 0U ||
        eventProbe.GetProgressEventCount() != 0U)
        return 106;

    source::GameObject proxySource;
    proxySource.ResetGameObject(80.0F);
    proxySource.SetLife(31.0F);
    proxySource.SetPos({1.0F, 2.0F, 3.0F});
    proxySource.SetScale({4.0F, 5.0F, 6.0F});
    proxySource.SetRot({0.1F, 0.2F, 0.3F, 0.9F});
    proxySource.SetMaxTimeLife(7.0F);
    proxySource.SetTimeLife(2.5F);
    source::GameObject proxyTarget;
    proxyTarget.ResetGameObject(12.0F);
    proxyTarget.CopyProxyStateFrom(proxySource);
    if (proxyTarget.GetMaxLife() != 12.0F ||
        proxyTarget.GetLife() != 31.0F ||
        proxyTarget.GetPos() != source::GameObject::Vector3{1.0F, 2.0F, 3.0F} ||
        proxyTarget.GetScale() != source::GameObject::Vector3{4.0F, 5.0F, 6.0F} ||
        proxyTarget.GetRot() !=
            source::GameObject::Quaternion{0.1F, 0.2F, 0.3F, 0.9F} ||
        proxyTarget.GetMaxTimeLife() != 7.0F ||
        proxyTarget.GetTimeLife() != 2.5F)
        return 64;

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
    if (!lethal.death || lethal.killCredit ||
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

    source::GameObject frameSyncOwner;
    auto& frameSync = frameSyncOwner.GetFrameSync();
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
    frameSync.SetPosSync({1.0F, 0.0F, 0.0F});
    frameSyncOwner.ResetGameObject(-1.0F);
    if (&frameSyncOwner.GetFrameSync() != &frameSync ||
        frameSync.HasActiveCorrection())
        return 96;
    const source::GameObjectFrameSync::Pose physicsStart{
        {0.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, 0.0F, 1.0F}};
    const source::GameObjectFrameSync::Pose physicsEnd{
        {10.0F, 4.0F, 0.0F},
        {0.0F, 0.0F, halfQuarterTurn, halfQuarterTurn}};
    frameSync.OnPhysicsState(
        physicsStart, {1.0F, 0.0F, 0.0F}, true);
    frameSync.OnPhysicsState(
        physicsEnd, {3.0F, 2.0F, 0.0F}, true);
    const auto halfPhysics = frameSync.OnFrame({}, 0.0F, 0.5F);
    if (!frameSync.HasPhysicsState() ||
        !frameSync.IsBodyProgressEvent() ||
        std::abs(halfPhysics.position.x - 5.0F) > 0.0001F ||
        std::abs(halfPhysics.position.y - 2.0F) > 0.0001F ||
        std::abs(frameSync.GetRenderVelocity().x - 2.0F) > 0.0001F ||
        std::abs(frameSync.GetRenderVelocity().y - 1.0F) > 0.0001F ||
        halfPhysics.rotation.z <= 0.3F ||
        halfPhysics.rotation.z >= halfQuarterTurn)
        return 97;
    frameSync.OnPhysicsState(
        physicsEnd, {0.0F, 0.0F, 0.0F}, false);
    const source::GameObjectFrameSync::Pose invalidBackendPose{
        {99.0F, 99.0F, 99.0F},
        {0.0F, 0.0F, 0.0F, 1.0F}};
    const auto sleepingPose = frameSync.OnFrame(
        invalidBackendPose, 0.0F, 1.0F);
    if (frameSync.IsBodyProgressEvent() ||
        std::abs(sleepingPose.position.x - physicsEnd.position.x) >
            0.0001F ||
        std::abs(sleepingPose.position.y - physicsEnd.position.y) >
            0.0001F)
        return 98;
    frameSync.OnPhysicsState(
        physicsStart, {4.0F, 0.0F, 0.0F}, true);
    const auto wokenPose = frameSync.OnFrame({}, 0.0F, 1.0F);
    if (!frameSync.IsBodyProgressEvent() ||
        std::abs(wokenPose.position.x) > 0.0001F ||
        std::abs(frameSync.GetRenderVelocity().x - 4.0F) > 0.0001F)
        return 99;

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

    source::GameObject mutatingDispatch;
    TrackingListener removedDuringDestroy;
    RemovingDestroyListener remover;
    remover.remove = &removedDuringDestroy;
    mutatingDispatch.InsertListener(&remover);
    mutatingDispatch.InsertListener(&removedDuringDestroy);
    if (!mutatingDispatch.DestroyObject() || !remover.called ||
        !removedDuringDestroy.order.empty() ||
        mutatingDispatch.GetListenerCount() != 1U)
        return 100;

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
    energyDamage.ConfigureSounds({"damage-a.ogg", "damage-b.ogg"});
    if (energyDamage.OnDamage(original::DamageType::Simple) ||
        energyDamage.HasPlayRequest() ||
        !energyDamage.OnDamage(original::DamageType::Energy) ||
        !energyDamage.HasPlayRequest())
        return 19;
    const auto* damageSound = energyDamage.ConsumePlayRequest(0.75F);
    if (damageSound == nullptr || *damageSound != "damage-b.ogg" ||
        energyDamage.HasPlayRequest())
        return 112;
    const auto damageSpawn = energyDamage.GetSpawnResult(true);
    if (damageSpawn.owner == nullptr ||
        !damageSpawn.owner->OnDestroyEffect() ||
        energyDamage.IsEffectMaked() ||
        !energyDamage.OnDamage(original::DamageType::Energy) ||
        energyDamage.ConsumePlayRequest(0.0F) == nullptr ||
        energyDamage.OnDamage(original::DamageType::Energy) ||
        !energyDamage.HasPlayRequest() ||
        energyDamage.ConsumePlayRequest(0.0F) == nullptr)
        return 98;
    energyDamage.OnProgress(0.5F);
    if (!energyDamage.IsEffectMaked())
        return 20;
    energyDamage.OnProgress(0.001F);
    if (energyDamage.IsEffectMaked())
        return 21;

    source::ImmortalEffect shieldEffect;
    shieldEffect.ConfigureSounds({"shield-a.ogg"});
    shieldEffect.OnImmortalStatus(true);
    if (!shieldEffect.IsEffectMaked() ||
        shieldEffect.GetFadeInTime() != 0.0F ||
        shieldEffect.GetScale() != 0.0F ||
        !shieldEffect.HasPlayRequest())
        return 22;
    const auto* shieldSound = shieldEffect.ConsumePlayRequest(0.0F);
    if (shieldSound == nullptr || *shieldSound != "shield-a.ogg" ||
        shieldEffect.HasPlayRequest())
        return 113;
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
    source::GameObject deathPlane;
    if (deathPlane.GetBehaviors().RequiresPhysicsNotify(
            source::BehaviorPhysicsNotify::Contact) ||
        deathPlane.GetBehaviors().RequiresPhysicsNotify(
            source::BehaviorPhysicsNotify::ContactModify))
        return 84;
    auto& touchDeath = deathPlane.GetBehaviors()
        .Add<source::TouchDeath>(source::BehaviorType::TouchDeath);
    deathPlane.OnContact(&falling);
    deathPlane.OnContact(&falling);
    if (deathPlane.GetBehaviors().Find(
            source::BehaviorType::TouchDeath) != &touchDeath ||
        !deathPlane.GetBehaviors().RequiresPhysicsNotify(
            source::BehaviorPhysicsNotify::Contact) ||
        deathPlane.GetBehaviors().RequiresPhysicsNotify(
            source::BehaviorPhysicsNotify::ContactModify) ||
        deathPlane.GetListenerCount() != 1U || !falling.destroyed ||
        falling.GetTouchPlayerId() != 8U)
        return 30;
    if (!deathPlane.GetBehaviors().Delete(&touchDeath) ||
        deathPlane.GetBehaviors().RequiresPhysicsNotify(
            source::BehaviorPhysicsNotify::Contact))
        return 85;

    source::GameObject particleOwner;
    particleOwner.ResetGameObject(-1.0F);
    auto& resurrectBehavior = particleOwner.GetBehaviors()
        .Add<source::ResurrectObj>(
            source::BehaviorType::ResurrectObj);
    if (!particleOwner.Death() || particleOwner.destroyed ||
        !resurrectBehavior.IsResurrect() ||
        particleOwner.GetBehaviors().Find(
            source::BehaviorType::ResurrectObj) !=
            &resurrectBehavior)
        return 32;
    if (!particleOwner.Death() || !particleOwner.destroyed)
        return 33;

    source::EventEffect eventEffect;
    eventEffect.ConfigureSounds({"sound0.ogg", "sound1.ogg", "sound2.ogg"});
    if (!eventEffect.MakeEffect())
        return 36;
    const auto makeEffectId = eventEffect.GetMakeEffectId();
    if (makeEffectId == source::EventEffect::invalidEffect ||
        eventEffect.GetEffectCount() != 1U ||
        !eventEffect.HasEffect(makeEffectId) ||
        eventEffect.MakeEffect() || !eventEffect.IsEffectMaked() ||
        !eventEffect.FreeEffect() ||
        eventEffect.FreeEffect() || eventEffect.IsEffectMaked() ||
        eventEffect.GetEffectCount() != 0U ||
        eventEffect.HasEffect(makeEffectId) ||
        eventEffect.GetSoundPaths().size() != 3U ||
        eventEffect.SelectSoundPath(0.0F) == nullptr ||
        *eventEffect.SelectSoundPath(0.0F) != "sound0.ogg" ||
        *eventEffect.SelectSoundPath(1.0F / 3.0F) != "sound1.ogg" ||
        *eventEffect.SelectSoundPath(1.0F) != "sound2.ogg")
        return 36;
    eventEffect.MakeEffect();
    const auto destroyedEffectId = eventEffect.GetMakeEffectId();
    if (!eventEffect.OnDestroyEffect(destroyedEffectId) ||
        eventEffect.GetEffectCount() != 0U ||
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

    source::GameObject behaviorEffectOwner;
    behaviorEffectOwner.ResetGameObject(-1.0F);
    auto& waitingBehavior =
        behaviorEffectOwner.GetBehaviors()
            .Add<source::FxSystemWaitingEndBehavior>(
                source::BehaviorType::FxSystemWaitingEnd);
    waitingBehavior.SetLiveParticleCount(2U);
    if (behaviorEffectOwner.GetBehaviors().GetCount() != 1U ||
        behaviorEffectOwner.GetListenerCount() != 1U ||
        !behaviorEffectOwner.Death() ||
        behaviorEffectOwner.destroyed ||
        !waitingBehavior.IsResurrect() ||
        !waitingBehavior.IsFading() ||
        !waitingBehavior.ConsumeBeginFading())
        return 93;
    behaviorEffectOwner.OnProgress(0.1F);
    if (behaviorEffectOwner.destroyed ||
        waitingBehavior.ConsumeFinalDeath())
        return 94;
    waitingBehavior.SetLiveParticleCount(0U);
    behaviorEffectOwner.OnProgress(0.1F);
    if (!behaviorEffectOwner.destroyed ||
        !waitingBehavior.ConsumeFinalDeath())
        return 95;

    source::GameObject behaviorSoundOwner;
    behaviorSoundOwner.ResetGameObject(-1.0F);
    auto& lifeBehavior =
        behaviorSoundOwner.GetBehaviors()
            .Add<source::LifeEffectBehavior>(
                source::BehaviorType::LifeEffect);
    lifeBehavior.ConfigureSounds({"life0.ogg", "life1.ogg"});
    lifeBehavior.SetSourceAvailable(false);
    behaviorSoundOwner.OnProgress(0.1F);
    if (lifeBehavior.HasPlayed() || lifeBehavior.ConsumePlayRequest())
        return 96;
    lifeBehavior.SetSourceAvailable(true);
    lifeBehavior.SetSoundSelectionUnit(0.75F);
    behaviorSoundOwner.OnProgress(0.1F);
    const auto* selectedLifeSound = lifeBehavior.ConsumePlayRequest();
    if (!lifeBehavior.HasPlayed() || selectedLifeSound == nullptr ||
        *selectedLifeSound != "life1.ogg" ||
        lifeBehavior.ConsumePlayRequest())
        return 97;

    r3d::game::originalrace::ObjectDefinition deathVisual;
    deathVisual.record = "Effects\\death";
    source::DeathEffect deathEffect(true, true);
    deathEffect.ConfigureSource(
        &deathVisual, {1.0F, 2.0F, 3.0F},
        {4.0F, 5.0F, 6.0F}, true,
        {"death0.ogg", "death1.ogg"});
    if (deathEffect.OnDeath(false, true, true).createEffect ||
        deathEffect.IsEffectMaked())
        return 40;
    const auto attachedDeath = deathEffect.OnDeath(true, true, true);
    if (!attachedDeath.createEffect || !attachedDeath.targetChild ||
        !attachedDeath.ignoreSenderCar ||
        attachedDeath.owner != &deathEffect ||
        attachedDeath.effectId == source::EventEffect::invalidEffect ||
        attachedDeath.definition != &deathVisual ||
        attachedDeath.position !=
            std::array<float, 3U>{1.0F, 2.0F, 3.0F} ||
        attachedDeath.impulse !=
            std::array<float, 3U>{4.0F, 5.0F, 6.0F} ||
        !attachedDeath.ignoreRotation ||
        !deathEffect.HasEffect(attachedDeath.effectId) ||
        !deathEffect.IsEffectMaked())
        return 41;
    if (deathEffect.OnDeath(true, true, true).createEffect)
        return 42;
    if (!attachedDeath.owner->OnDestroyEffect(
            attachedDeath.effectId) ||
        deathEffect.GetEffectCount() != 0U)
        return 100;
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

    source::GameObject sourceSpeedOwner;
    sourceSpeedOwner.ResetGameObject(-1.0F);
    auto& sourceSpeedBehavior =
        sourceSpeedOwner.GetBehaviors()
            .Add<source::FxSystemSrcSpeedBehavior>(
                source::BehaviorType::FxSystemSrcSpeed);
    sourceSpeedBehavior.SetPhysicsInput(
        true, {0.0F, 4.0F, 10.0F}, &speedParent);
    sourceSpeedOwner.OnProgress(0.1F);
    const auto behaviorLocalSpeed =
        sourceSpeedBehavior.GetSourceSpeed();
    const auto behaviorWorldSpeed =
        sourceSpeedBehavior.GetWorldSourceSpeed();
    if (sourceSpeedOwner.GetBehaviors().GetCount() != 1U ||
        sourceSpeedOwner.GetListenerCount() != 1U ||
        !sourceSpeedBehavior.HasPhysicsActor() ||
        std::abs(behaviorLocalSpeed.x - 2.0F) > 0.0001F ||
        std::abs(behaviorLocalSpeed.z - 2.0F) > 0.0001F ||
        std::abs(behaviorWorldSpeed.y - 4.0F) > 0.0001F ||
        std::abs(behaviorWorldSpeed.z - 10.0F) > 0.0001F)
        return 98;
    sourceSpeedBehavior.SetPhysicsInput(
        false, {0.0F, 0.0F, 0.0F});
    sourceSpeedOwner.OnProgress(0.1F);
    if (sourceSpeedBehavior.HasPhysicsActor() ||
        std::abs(sourceSpeedBehavior.GetSourceSpeed().x - 2.0F) >
            0.0001F ||
        std::abs(sourceSpeedBehavior.GetWorldSourceSpeed().y - 4.0F) >
            0.0001F)
        return 99;

    std::cout << "original GameObject/DestrObj/effect behavior source "
                 "rules passed\n";
    return 0;
}
