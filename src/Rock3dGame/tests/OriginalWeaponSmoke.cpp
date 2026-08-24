#include "OriginalWeapon.h"

#include <array>
#include <cmath>
#include <iostream>

int main()
{
    namespace source = r3d::game::originalrace::source;

    const std::array<std::uint32_t, 2> projectiles{10U, 11U};
    source::Weapon weapon;
    weapon.SetDesc(0.1F, projectiles);
    if (weapon.IsReadyShot() || !weapon.IsMaslo() ||
        std::abs(weapon.GetShotTime()) > 0.0001F)
        return 1;

    weapon.OnProgress(0.1F);
    if (weapon.IsReadyShot())
        return 2;
    weapon.OnProgress(0.001F);
    if (!weapon.IsReadyShot())
        return 3;

    weapon.OnShot(false);
    if (!weapon.IsReadyShot())
        return 4;
    weapon.OnShot(true);
    if (weapon.IsReadyShot() ||
        std::abs(weapon.GetShotTime()) > 0.0001F)
        return 5;

    source::WeaponRack rack;
    rack.primary[2].SetDesc(
        0.2F, std::span<const std::uint32_t>{});
    rack.hyper.SetDesc(
        0.3F, std::span<const std::uint32_t>{});
    rack.mine.SetDesc(0.4F, projectiles);
    rack.OnProgress(0.5F);
    if (!rack.primary[2].IsReadyShot() ||
        !rack.hyper.IsReadyShot() || !rack.mine.IsReadyShot())
        return 6;
    rack.Reset();
    if (rack.primary[2].IsReadyShot() ||
        rack.hyper.IsReadyShot() || rack.mine.IsReadyShot())
        return 7;

    std::cout << "original Weapon timer/Desc source rules passed\n";
    return 0;
}
