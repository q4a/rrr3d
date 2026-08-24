#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace r3d::game::originalrace::source
{

// Backend-neutral transcription of the original Weapon timer and Desc
// ownership. Projectile preparation remains at the Jolt/bgfx session
// boundary; readiness and successful-shot lifetime belong here.
class Weapon
{
public:
    struct Desc
    {
        float shotDelay = 0.0F;
        std::vector<std::uint32_t> projectileTypes;
    };

    Weapon() = default;
    explicit Weapon(const Desc& desc);

    void Reset() noexcept;
    void OnProgress(float deltaTime) noexcept;
    float GetShotTime() const noexcept;
    bool IsReadyShot(float delay) const noexcept;
    bool IsReadyShot() const noexcept;
    bool IsMaslo() const noexcept;
    void OnShot(bool projectileCreated = true) noexcept;

    const Desc& GetDesc() const noexcept;
    void SetDesc(const Desc& value);
    void SetDesc(float shotDelay,
                 std::span<const std::uint32_t> projectileTypes);

private:
    Desc desc_;
    float shotTime_ = 0.0F;
};

// One source Weapon map object exists for every installed slot, including
// Hyper and Mine. This small owner replaces three unrelated session timers.
struct WeaponRack
{
    static constexpr std::size_t primarySlotCount = 4U;

    void Reset() noexcept;
    void OnProgress(float deltaTime) noexcept;

    std::array<Weapon, primarySlotCount> primary;
    Weapon hyper;
    Weapon mine;
};

} // namespace r3d::game::originalrace::source
