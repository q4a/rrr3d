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

    std::cout << "original GameObject/DestrObj source rules passed\n";
    return 0;
}
