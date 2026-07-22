#pragma once

namespace r3d::portable
{

struct EngineCapabilities
{
    bool renderer;
    bool physics;
};

EngineCapabilities engine_capabilities() noexcept;
void log_engine_capabilities() noexcept;

} // namespace r3d::portable
