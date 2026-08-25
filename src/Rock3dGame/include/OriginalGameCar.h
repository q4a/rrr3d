#pragma once

namespace r3d::game::originalrace::source
{

// Gameplay-owned lock state from GameCar. Wheel/actor operations are Jolt
// adapter responsibilities; timer ownership and source gates stay here.
class GameCar
{
public:
    static constexpr float clutchLockSeconds = 0.38F;
    static constexpr float springLockSeconds = 1.5F;

    struct ProgressResult
    {
        bool clutchReleased = false;
        bool springReleased = false;
        bool mineReleased = false;
    };

    void Reset() noexcept;
    ProgressResult OnProgress(float deltaTime) noexcept;

    bool LockClutch(float strength, bool clutchImmunity) noexcept;
    void CancelClutch() noexcept;
    bool IsClutchLocked() const noexcept;
    float GetClutchTime() const noexcept;
    float ConsumeClutchStrength() noexcept;

    void LockSpring() noexcept;
    bool IsSpringLocked() const noexcept;
    float GetSpringTime() const noexcept;

    void LockMine(float time) noexcept;
    bool IsMineLocked() const noexcept;
    float GetMineTime() const noexcept;

private:
    float clutchStrength_ = 0.0F;
    float clutchTime_ = 0.0F;
    float springTime_ = 0.0F;
    float mineTime_ = 0.0F;
};

} // namespace r3d::game::originalrace::source
