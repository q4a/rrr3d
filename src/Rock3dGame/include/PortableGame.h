#pragma once

namespace r3d::portable
{

struct GameCapabilities
{
    bool world;
    bool network;
    bool video;
    bool steam;
    bool audio;
};

GameCapabilities game_capabilities() noexcept;
void log_game_capabilities() noexcept;

} // namespace r3d::portable
