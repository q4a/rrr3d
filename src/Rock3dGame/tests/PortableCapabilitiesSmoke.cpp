#include "PortableEngine.h"
#include "PortableGame.h"

#include <iostream>

int main()
{
    const auto engine = r3d::portable::engine_capabilities();
    const auto game = r3d::portable::game_capabilities();

#ifdef RRR3D_RENDERER_BGFX
    constexpr bool expectedRenderer = true;
#else
    constexpr bool expectedRenderer = false;
#endif
#if defined(RRR3D_PHYSICS_JOLT) || defined(RRR3D_PHYSICS_MINIMAL)
    constexpr bool expectedPhysics = true;
#else
    constexpr bool expectedPhysics = false;
#endif
#ifdef RRR3D_NETWORK
    constexpr bool expectedNetwork = true;
#else
    constexpr bool expectedNetwork = false;
#endif
#ifdef RRR3D_VIDEO
    constexpr bool expectedVideo = true;
#else
    constexpr bool expectedVideo = false;
#endif
#ifdef RRR3D_AUDIO
    constexpr bool expectedAudio = true;
#else
    constexpr bool expectedAudio = false;
#endif

    if (engine.renderer != expectedRenderer ||
        engine.physics != expectedPhysics || game.world ||
        game.network != expectedNetwork || game.video != expectedVideo ||
        game.steam || game.audio != expectedAudio)
    {
        std::cerr << "portable capability flags do not match the build\n";
        return 1;
    }
    std::cout << "portable capability smoke passed\n";
    return 0;
}
