#pragma once

#include "InputActions.h"
#include "OriginalRace.h"
#include "OriginalRaceSession.h"
#include "physics/OriginalVehiclePhysics.h"
#include "renderer/Renderer.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rrr3d::debug
{

enum class Command
{
    None,
    ResetVehicles,
    ToggleFullscreen,
};

// Portable counterpart of the source AIDebug/DEBUG_PX controls. It can be
// enabled alone by --game-debug, while --legacy-windows-debug composes it
// with the separate Windows-wide scenario changes in the application flow.
class OriginalGameDebug
{
public:
    OriginalGameDebug(bool enabled, std::uint32_t sourcePostEffect) noexcept;

    bool enabled() const noexcept;
    bool overlayVisible() const noexcept;
    bool traceVisible() const noexcept;
    bool humanAiControl() const noexcept;
    std::uint32_t effectivePostEffect(
        std::uint32_t sourcePostEffect) const noexcept;
    std::size_t page() const noexcept;

    Command handle(rrr3d::input::Action action, bool active,
                   bool repeated) noexcept;
    void resetRaceState() noexcept;
    void updateFrame(float seconds) noexcept;
    bool takeOverlayRefresh() noexcept;

    std::vector<std::string> lines(
        const r3d::game::originalrace::Race& race,
        const r3d::game::originalrace::OriginalRaceSession& session,
        const std::vector<r3d::physics::VehicleState>& vehicles,
        const r3d::physics::WorldDescription& physics,
        const r3d::renderer::RenderTelemetry& telemetry) const;

private:
    bool enabled_ = false;
    bool overlayVisible_ = true;
    bool traceVisible_ = false;
    bool humanAiControl_ = false;
    bool postEffectsEnabled_ = false;
    std::uint32_t sourcePostEffect_ = 0U;
    std::size_t page_ = 0U;
    float smoothedFrameSeconds_ = 0.0F;
    float refreshSeconds_ = 0.0F;
    bool refreshRequested_ = true;
};

bool runOriginalGameDebugSmokeTest(std::string& error);

} // namespace rrr3d::debug
