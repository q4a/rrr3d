#pragma once

#include "OriginalGameCar.h"
#include "OriginalWeapon.h"

namespace r3d::game::originalrace::source
{

class RockCarEventSink
{
public:
    virtual ~RockCarEventSink() = default;
    virtual void OnRockCarDamageDispatch(
        std::size_t senderPlayerId, float value,
        DamageType damageType) noexcept = 0;
    virtual void OnRockCarKillDispatch(
        std::size_t senderPlayerId, float value,
        DamageType damageType) noexcept = 0;
};

// Backend-neutral transcription of the original RockCar owner. Windows
// keeps the six installed Weapon map objects below RockCar and progresses
// that collection immediately after GameCar. Map/serialization remain at
// the portable adapter boundary; gameplay lifetime and update order live
// here.
class RockCar : public GameCar
{
public:
    RockCar() = default;
    RockCar(const RockCar&) = default;
    RockCar& operator=(const RockCar&) = default;
    RockCar(RockCar&&) noexcept = default;
    RockCar& operator=(RockCar&&) noexcept = default;
    ~RockCar() override = default;

    ProgressResult OnProgress(float deltaTime) noexcept;
    WeaponRack& GetWeapons() noexcept;
    const WeaponRack& GetWeapons() const noexcept;
    void SetEventSink(RockCarEventSink* value) noexcept;
    RockCarEventSink* GetEventSink() noexcept;
    const RockCarEventSink* GetEventSink() const noexcept;

protected:
    void OnDamageDispatchEvent(
        std::size_t senderPlayerId, float value,
        DamageType damageType) noexcept override;
    void OnKillDispatchEvent(
        std::size_t senderPlayerId, float value,
        DamageType damageType) noexcept override;

private:
    WeaponRack weapons_;
    RockCarEventSink* eventSink_ = nullptr;
};

} // namespace r3d::game::originalrace::source
