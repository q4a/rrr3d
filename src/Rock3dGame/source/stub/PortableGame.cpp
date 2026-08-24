#include "PortableGame.h"
#include "Rock3dGame.h"

#include "PortableEngine.h"
#include "xplatform.h"

#include <stdexcept>

namespace r3d::portable
{

GameCapabilities game_capabilities() noexcept
{
    return {
        false,
#ifdef RRR3D_NETWORK
        true,
#else
        false,
#endif
#ifdef RRR3D_VIDEO
        true,
#else
        false,
#endif
        false,
#ifdef RRR3D_AUDIO
        true};
#else
        false};
#endif
}

void log_game_capabilities() noexcept
{
#if defined(RRR3D_PHYSICS_JOLT) || defined(RRR3D_PHYSICS_MINIMAL)
    rrr3d::platform::report_error(
        "Rock3dGame",
        "source-derived race runtime enabled; legacy IWorld unavailable");
#else
    rrr3d::platform::report_error(
        "Rock3dGame", "portable target linked; gameplay world unavailable");
#endif
    log_engine_capabilities();
#ifdef RRR3D_NETWORK
    rrr3d::platform::report_error(
        "Rock3dGame",
        "source NetLib LAN transport/session and race models enabled");
#else
    rrr3d::platform::report_error(
        "Rock3dGame", "network disabled (RRR3D_ENABLE_NETWORK=OFF)");
#endif
#ifdef RRR3D_VIDEO
    rrr3d::platform::report_error(
        "Rock3dGame", "AVFoundation video playback enabled");
#else
    rrr3d::platform::report_error(
        "Rock3dGame", "video disabled (RRR3D_ENABLE_VIDEO=OFF)");
#endif
    rrr3d::platform::report_error(
        "Rock3dGame", "Steam integration unavailable in the portable runtime");
#ifdef RRR3D_AUDIO
    rrr3d::platform::report_error(
        "Rock3dGame", "SDL3/Vorbis audio runtime enabled");
#else
    rrr3d::platform::report_error(
        "Rock3dGame", "audio disabled (RRR3D_ENABLE_AUDIO=OFF)");
#endif
}

} // namespace r3d::portable

namespace r3d
{

ROCK3DGAME_API IWorld* CreateWorld(const IView::Desc& view_desc, bool steam_init)
{
    portable::log_game_capabilities();
#if defined(RRR3D_PHYSICS_JOLT) || defined(RRR3D_PHYSICS_MINIMAL)
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
