#pragma once

#include "OriginalGameCar.h"
#include "OriginalMapObj.h"
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
    // Direct transcription of RockCar::Weapons. Every installed slot is a
    // concrete Weapon MapObj parented to the car; Hyper and Mine retain the
    // source first-projectile lookup cache.
    class Weapons final : public MapObjects
    {
    public:
        explicit Weapons(RockCar* owner) noexcept;

        MapObj& Add(
            const Weapon::Desc& description,
            std::string record = {});
        Weapon* GetWeapon(std::size_t slot) noexcept;
        const Weapon* GetWeapon(std::size_t slot) const noexcept;
        Weapon* GetHyperDrive() noexcept;
        const Weapon* GetHyperDrive() const noexcept;
        Weapon* GetMines() noexcept;
        const Weapon* GetMines() const noexcept;
        void CopyFrom(const Weapons& value);

    protected:
        void InsertItem(MapObj& value) override;
        void RemoveItem(MapObj& value) noexcept override;

    private:
        void RefreshSpecial(Weapon& value) noexcept;

        Weapon* hyperDrive_ = nullptr;
        Weapon* mines_ = nullptr;
    };

    RockCar();
    RockCar(const RockCar& value);
    RockCar& operator=(const RockCar& value);
    RockCar(RockCar&& value) noexcept;
    RockCar& operator=(RockCar&& value) noexcept;
    ~RockCar() override;

    ProgressResult OnProgress(float deltaTime) noexcept;
    Weapons& GetWeapons() noexcept;
    const Weapons& GetWeapons() const noexcept;
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
    std::unique_ptr<Weapons> weapons_;
    RockCarEventSink* eventSink_ = nullptr;
};

} // namespace r3d::game::originalrace::source
