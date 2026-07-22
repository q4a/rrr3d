#include "PortableEngine.h"

#include "xplatform.h"

namespace r3d::portable
{

EngineCapabilities engine_capabilities() noexcept
{
#ifdef RRR3D_RENDERER_BGFX
    constexpr bool renderer = true;
#else
    constexpr bool renderer = false;
#endif
#ifdef RRR3D_PHYSICS_MINIMAL
    constexpr bool physics = true;
#else
    constexpr bool physics = false;
#endif
    return {renderer, physics};
}

void log_engine_capabilities() noexcept
{
#ifdef RRR3D_RENDERER_BGFX
    rrr3d::platform::report_error(
        "Rock3dEngine", "portable renderer enabled (bgfx/Metal)");
#else
    rrr3d::platform::report_error(
        "Rock3dEngine", "renderer disabled (RRR3D_ENABLE_RENDERER=OFF)");
#endif
#ifdef RRR3D_PHYSICS_MINIMAL
    rrr3d::platform::report_error(
        "Rock3dEngine", "portable vehicle physics enabled (fixed-step minimal backend)");
#else
    rrr3d::platform::report_error(
        "Rock3dEngine", "physics disabled (RRR3D_ENABLE_PHYSICS=OFF)");
#endif
}

} // namespace r3d::portable
