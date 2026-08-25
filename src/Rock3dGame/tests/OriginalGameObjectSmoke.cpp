#include "OriginalGameObject.h"

#include <cmath>
#include <iostream>

namespace original = r3d::game::originalrace;
namespace source = r3d::game::originalrace::source;

int main()
{
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

    std::cout << "original GameObject/DestrObj/effect behavior source "
                 "rules passed\n";
    return 0;
}
