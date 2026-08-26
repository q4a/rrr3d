#include "OriginalRockCar.h"

namespace r3d::game::originalrace::source
{

GameCar::ProgressResult RockCar::OnProgress(float deltaTime) noexcept
{
    auto result = GameCar::OnProgress(deltaTime);
    weapons_.OnProgress(deltaTime);
    return result;
}

WeaponRack& RockCar::GetWeapons() noexcept
{
    return weapons_;
}

const WeaponRack& RockCar::GetWeapons() const noexcept
{
    return weapons_;
}

void RockCar::SetEventSink(RockCarEventSink* value) noexcept
{
    eventSink_ = value;
}

RockCarEventSink* RockCar::GetEventSink() noexcept
{
    return eventSink_;
}

const RockCarEventSink* RockCar::GetEventSink() const noexcept
{
    return eventSink_;
}

void RockCar::OnDamageDispatchEvent(
    std::size_t senderPlayerId, float value,
    DamageType damageType) noexcept
{
    if (eventSink_ != nullptr)
        eventSink_->OnRockCarDamageDispatch(
            senderPlayerId, value, damageType);
}

void RockCar::OnKillDispatchEvent(
    std::size_t senderPlayerId, float value,
    DamageType damageType) noexcept
{
    if (eventSink_ != nullptr)
        eventSink_->OnRockCarKillDispatch(
            senderPlayerId, value, damageType);
}

} // namespace r3d::game::originalrace::source
