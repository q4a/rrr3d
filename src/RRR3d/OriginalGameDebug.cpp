#include "OriginalGameDebug.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace rrr3d::debug
{
namespace
{

constexpr std::size_t pageCount = 3U;

const char* phaseName(r3d::game::originalrace::RacePhase phase) noexcept
{
    using Phase = r3d::game::originalrace::RacePhase;
    switch (phase)
    {
    case Phase::Countdown:
        return "countdown";
    case Phase::Racing:
        return "racing";
    case Phase::Finished:
        return "finished";
    case Phase::Paused:
        return "paused";
    }
    return "unknown";
}

std::string vectorText(const r3d::physics::Vec3& value)
{
    std::ostringstream text;
    text << std::fixed << std::setprecision(2) << value.x << ", "
         << value.y << ", " << value.z;
    return text.str();
}

std::string floatText(float value, int precision = 2)
{
    std::ostringstream text;
    text << std::fixed << std::setprecision(precision) << value;
    return text.str();
}

const r3d::physics::VehicleDescription& vehicleDescription(
    const r3d::physics::WorldDescription& physics,
    std::size_t racer)
{
    return racer < physics.spawns.size()
               ? physics.spawns[racer].vehicle
               : physics.vehicle;
}

} // namespace

OriginalGameDebug::OriginalGameDebug(
    bool enabled, std::uint32_t sourcePostEffect) noexcept
    : enabled_(enabled), postEffectsEnabled_(sourcePostEffect > 0U),
      sourcePostEffect_(sourcePostEffect)
{
}

bool OriginalGameDebug::enabled() const noexcept { return enabled_; }
bool OriginalGameDebug::overlayVisible() const noexcept
{
    return enabled_ && overlayVisible_;
}
bool OriginalGameDebug::traceVisible() const noexcept
{
    return enabled_ && traceVisible_;
}
bool OriginalGameDebug::humanAiControl() const noexcept
{
    return enabled_ && humanAiControl_;
}
std::size_t OriginalGameDebug::page() const noexcept { return page_; }

std::uint32_t OriginalGameDebug::effectivePostEffect(
    std::uint32_t sourcePostEffect) const noexcept
{
    if (!enabled_)
        return sourcePostEffect;
    return postEffectsEnabled_
               ? std::max(sourcePostEffect_, sourcePostEffect)
               : 0U;
}

Command OriginalGameDebug::handle(
    rrr3d::input::Action action, bool active, bool repeated) noexcept
{
    if (!enabled_ || !active || repeated)
        return Command::None;
    using Action = rrr3d::input::Action;
    switch (action)
    {
    case Action::Debug1:
        return Command::ResetVehicles;
    case Action::Debug2:
        postEffectsEnabled_ = !postEffectsEnabled_;
        if (postEffectsEnabled_)
            sourcePostEffect_ = std::max(sourcePostEffect_, 2U);
        refreshRequested_ = true;
        return Command::None;
    case Action::Debug3:
        return Command::ToggleFullscreen;
    case Action::Debug4:
    case Action::Debug5:
        // These are the two commented Steam service calls in GameMode.cpp.
        return Command::None;
    case Action::Debug6:
        traceVisible_ = !traceVisible_;
        refreshRequested_ = true;
        return Command::None;
    case Action::Debug7:
        humanAiControl_ = !humanAiControl_;
        refreshRequested_ = true;
        return Command::None;
    case Action::DebugOverlay:
        overlayVisible_ = !overlayVisible_;
        refreshRequested_ = true;
        return Command::None;
    case Action::DebugPagePrevious:
        page_ = (page_ + pageCount - 1U) % pageCount;
        refreshRequested_ = true;
        return Command::None;
    case Action::DebugPageNext:
        page_ = (page_ + 1U) % pageCount;
        refreshRequested_ = true;
        return Command::None;
    default:
        return Command::None;
    }
}

void OriginalGameDebug::resetRaceState() noexcept
{
    traceVisible_ = false;
    humanAiControl_ = false;
    refreshRequested_ = true;
}

void OriginalGameDebug::updateFrame(float seconds) noexcept
{
    if (!enabled_)
        return;
    seconds = std::clamp(seconds, 0.0F, 0.25F);
    if (seconds > 0.0F)
    {
        smoothedFrameSeconds_ = smoothedFrameSeconds_ <= 0.0F
            ? seconds
            : smoothedFrameSeconds_ * 0.9F + seconds * 0.1F;
    }
    refreshSeconds_ += seconds;
    if (refreshSeconds_ >= 0.25F)
    {
        refreshSeconds_ = std::fmod(refreshSeconds_, 0.25F);
        refreshRequested_ = true;
    }
}

bool OriginalGameDebug::takeOverlayRefresh() noexcept
{
    if (!overlayVisible() || !refreshRequested_)
        return false;
    refreshRequested_ = false;
    return true;
}

std::vector<std::string> OriginalGameDebug::lines(
    const r3d::game::originalrace::Race& race,
    const r3d::game::originalrace::OriginalRaceSession& session,
    const std::vector<r3d::physics::VehicleState>& vehicles,
    const r3d::physics::WorldDescription& physics,
    const r3d::renderer::RenderTelemetry& telemetry) const
{
    std::vector<std::string> result;
    const float fps = smoothedFrameSeconds_ > 0.00001F
                          ? 1.0F / smoothedFrameSeconds_
                          : 0.0F;
    result.push_back(
        "RRR3D GAME DEBUG  page " + std::to_string(page_ + 1U) +
        "/" + std::to_string(pageCount) +
        "  [F10 hide, PgUp/PgDn pages]");
    result.push_back(
        "F1 reset cars | F2 post FX " +
        std::string(postEffectsEnabled_ ? "ON" : "OFF") +
        " | F3 fullscreen | F6 trace " +
        std::string(traceVisible_ ? "ON" : "OFF") +
        " | F7 player AI " +
        std::string(humanAiControl_ ? "ON" : "OFF"));
    result.push_back(
        "FPS " + floatText(fps, 1) + "  frame " +
        floatText(smoothedFrameSeconds_ * 1000.0F, 2) + " ms");

    const std::size_t humanRacer = session.humanRacer();
    if (humanRacer >= vehicles.size() ||
        humanRacer >= session.racers().size())
    {
        result.push_back("No active source vehicle/session state");
        return result;
    }

    const auto& vehicle = vehicles[humanRacer];
    const auto& racer = session.racers()[humanRacer];
    const auto& input = humanRacer >= session.vehicleInputs().size()
                            ? r3d::physics::VehicleInput{}
                            : session.vehicleInputs()[humanRacer];
    const auto& description = vehicleDescription(physics, humanRacer);

    if (page_ == 0U)
    {
        result.push_back(
            "Phase " + std::string(phaseName(session.phase())) +
            "  countdown " + std::to_string(session.countdownStage()) +
            "  time " + floatText(session.elapsedSeconds()) + " s");
        result.push_back(
            "Place " + std::to_string(racer.GetPlace()) + "/" +
            std::to_string(race.racers.size()) + "  lap " +
            std::to_string(racer.car.numLaps) + "/" +
            std::to_string(race.lapCount) + "  next node " +
            std::to_string(racer.nextPathNode));
        result.push_back(
            "Wrong way " +
            std::string(racer.car.moveInverse ? "YES" : "no") +
            "  life " + floatText(racer.life) + "/" +
            floatText(racer.maximumLife) + "  resets " +
            std::to_string(vehicle.resetCount));
        result.push_back("Position " + vectorText(vehicle.body.position));
        result.push_back(
            "Speed " + floatText(vehicle.speed * 3.6F) +
            " km/h  Axle " + floatText(vehicle.drivenWheelSpeed * 3.6F) +
            " km/h");
        result.push_back(
            "RPM " + floatText(vehicle.engineRpm, 0) + "  Gear " +
            std::to_string(vehicle.gear) + "  contacts " +
            std::to_string(vehicle.contactCount));
        result.push_back(
            "Input gas " + floatText(input.throttle) + " reverse " +
            floatText(input.reverse) + " brake " + floatText(input.brake) +
            " steer " + floatText(input.steering) +
            (input.manualSteering ? " manual" : " digital"));
        const auto totalDraws = std::accumulate(
            telemetry.drawCount.begin(), telemetry.drawCount.end(), 0U);
        result.push_back(
            "Renderer draws " + std::to_string(totalDraws) +
            "  transient " + std::to_string(telemetry.transientDrawCount) +
            "  lights " + std::to_string(telemetry.activeSpotLightCount));
    }
    else if (page_ == 1U)
    {
        result.push_back("Linear velocity " + vectorText(vehicle.linearVelocity));
        result.push_back("Angular momentum " + vectorText(vehicle.angularMomentum));
        result.push_back(
            "Body contacts " + std::to_string(vehicle.bodyContacts.size()) +
            "  wheel contacts " + std::to_string(vehicle.contactCount));
        const std::size_t wheelCount = std::max(
            {vehicle.wheels.size(), vehicle.wheelAngularSpeeds.size(),
             vehicle.wheelContacts.size(), description.wheels.size()});
        for (std::size_t index = 0U; index < wheelCount; ++index)
        {
            const bool contact = index < vehicle.wheelContacts.size() &&
                                 vehicle.wheelContacts[index].hasContact;
            const float angular = index < vehicle.wheelAngularSpeeds.size()
                                      ? vehicle.wheelAngularSpeeds[index]
                                      : 0.0F;
            const float longitudinal = index < vehicle.wheelContacts.size()
                                           ? vehicle.wheelContacts[index]
                                                 .longitudinalSlip
                                           : 0.0F;
            const float lateral = index < vehicle.wheelContacts.size()
                                      ? vehicle.wheelContacts[index].lateralSlip
                                      : 0.0F;
            result.push_back(
                "Wheel " + std::to_string(index) +
                (contact ? " ground" : " AIR") + "  omega " +
                floatText(angular) + "  slip long/lat " +
                floatText(longitudinal) + "/" + floatText(lateral));
        }
    }
    else
    {
        result.push_back(
            "Mass " + floatText(description.mass) + "  COM " +
            vectorText(description.centerOfMass));
        result.push_back(
            "Max speed " + floatText(description.maximumSpeed * 3.6F) +
            " km/h  max RPM " + floatText(description.maximumRpm, 0) +
            "  torque " + floatText(description.maximumTorque));
        result.push_back(
            "Brake torque " + floatText(description.brakeTorque) +
            "  diff ratio " + floatText(description.differentialRatio) +
            "  tire spring " + floatText(description.tireSpring));
        result.push_back(
            "Steer angle/speed/rotation " +
            floatText(description.steerAngle) + "/" +
            floatText(description.steerSpeed) + "/" +
            floatText(description.steerRotation));
        for (std::size_t index = 0U; index < description.wheels.size(); ++index)
        {
            const auto& wheel = description.wheels[index];
            result.push_back(
                "Wheel " + std::to_string(index) + " r/w " +
                floatText(wheel.radius) + "/" + floatText(wheel.width) +
                " spring/damper/travel " + floatText(wheel.spring) + "/" +
                floatText(wheel.damper) + "/" +
                floatText(wheel.suspensionTravel) +
                (wheel.driven ? " driven" : "") +
                (wheel.steering ? " steering" : ""));
        }
    }
    return result;
}

bool runOriginalGameDebugSmokeTest(std::string& error)
{
    OriginalGameDebug disabled(false, 2U);
    if (disabled.handle(rrr3d::input::Action::Debug2, true, false) !=
            Command::None ||
        disabled.effectivePostEffect(2U) != 2U ||
        disabled.overlayVisible())
    {
        error = "disabled game debug changed release runtime state";
        return false;
    }

    OriginalGameDebug enabled(true, 2U);
    enabled.handle(rrr3d::input::Action::Debug2, true, false);
    if (enabled.effectivePostEffect(2U) != 0U)
    {
        error = "F2 did not disable source post effects";
        return false;
    }
    OriginalGameDebug bloomOnly(true, 1U);
    if (bloomOnly.effectivePostEffect(1U) != 1U)
    {
        error = "enabling game debug changed the active profile quality";
        return false;
    }
    OriginalGameDebug initiallyDisabledEffects(true, 0U);
    initiallyDisabledEffects.handle(
        rrr3d::input::Action::Debug2, true, false);
    if (initiallyDisabledEffects.effectivePostEffect(0U) != 2U)
    {
        error = "F2 did not enable source Bloom/HDR from an off profile";
        return false;
    }
    enabled.handle(rrr3d::input::Action::Debug6, true, false);
    enabled.handle(rrr3d::input::Action::Debug7, true, false);
    enabled.handle(rrr3d::input::Action::DebugPageNext, true, false);
    if (!enabled.traceVisible() || !enabled.humanAiControl() ||
        enabled.page() != 1U ||
        enabled.handle(rrr3d::input::Action::Debug1, true, false) !=
            Command::ResetVehicles ||
        enabled.handle(rrr3d::input::Action::Debug3, true, false) !=
            Command::ToggleFullscreen)
    {
        error = "original F-key debug control state failed";
        return false;
    }
    enabled.resetRaceState();
    if (enabled.traceVisible() || enabled.humanAiControl())
    {
        error = "race debug state survived a source race reset";
        return false;
    }
    error.clear();
    return true;
}

} // namespace rrr3d::debug
