#include "PortableGame.h"
#include "Rock3dGame.h"

#include "PortableEngine.h"
#include "xplatform.h"

#include <stdexcept>

namespace r3d::portable
{

GameCapabilities game_capabilities() noexcept
{
    return {false, false, false, false, false};
}

void log_game_capabilities() noexcept
{
#ifdef RRR3D_PHYSICS_MINIMAL
    rrr3d::platform::report_error(
        "Rock3dGame",
        "portable race vertical slice enabled; legacy IWorld unavailable");
#else
    rrr3d::platform::report_error(
        "Rock3dGame", "portable target linked; gameplay world unavailable");
#endif
    log_engine_capabilities();
    rrr3d::platform::report_error(
        "Rock3dGame", "network disabled (RRR3D_ENABLE_NETWORK=OFF)");
    rrr3d::platform::report_error(
        "Rock3dGame", "video disabled (RRR3D_ENABLE_VIDEO=OFF)");
    rrr3d::platform::report_error(
        "Rock3dGame", "Steam disabled (RRR3D_ENABLE_STEAM=OFF)");
    rrr3d::platform::report_error(
        "Rock3dGame", "legacy IWorld audio integration unavailable");
}

} // namespace r3d::portable

namespace r3d
{

ROCK3DGAME_API IWorld* CreateWorld(const IView::Desc& view_desc, bool steam_init)
{
    portable::log_game_capabilities();
#ifdef RRR3D_PHYSICS_MINIMAL
    throw std::runtime_error(
        "Legacy Rock3dGame IWorld is unavailable; use PortableRaceSession");
#else
    throw std::runtime_error(
        "Rock3dGame world is unavailable while renderer and physics are disabled");
#endif
}

ROCK3DGAME_API void ReleaseWorld(IWorld* world)
{
    if (world)
    {
        rrr3d::platform::report_error(
            "Rock3dGame",
            "ReleaseWorld rejected an object not created by the portable stub");
    }
}

} // namespace r3d
