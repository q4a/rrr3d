#include "OriginalRaceHud.h"

#include "CoreTextRasterizer.h"
#include "resource/ResourceFileSystem.h"

#include <bx/math.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <iomanip>
#include <sstream>

namespace rrr3d::race
{
namespace
{

using namespace r3d::renderer;
namespace menu = r3d::game::mainmenu2;
namespace originalrace = r3d::game::originalrace;

bool valid(Texture value) noexcept
{
    return value.value != invalid_resource;
}

Transform transform(float width, float height, float centerX,
                    float centerY, float depth)
{
    Transform result;
    bx::mtxSRT(result.matrix.data(), width, height, 1.0F,
               0.0F, 0.0F, 0.0F, centerX, centerY, depth);
    return result;
}

void drawAsset(GraphicsDevice& device, Mesh quad, Shader shader,
               Texture texture, float width, float height, float centerX,
               float centerY, float depth, const PipelineState& pipeline)
{
    if (valid(texture) && width > 0.0F && height > 0.0F)
        device.draw(quad, shader, texture,
                    transform(width, height, centerX, centerY, depth),
                    pipeline);
}

} // namespace

bool OriginalRaceHud::loadImage(
    GraphicsDevice& device,
    const r3d::resource::ResourceFileSystem& resources, std::string path,
    ImageAsset& output, std::string& error)
{
    try
    {
        const auto image = menu::loadOriginalImage(resources, std::move(path));
        output.width = static_cast<float>(image.width);
        output.height = static_cast<float>(image.height);
        if (image.storage == menu::ImageStorage::EncodedContainer)
        {
            output.texture = device.createTextureContainer(
                image.bytes.data(), image.bytes.size(), image.virtualPath);
        }
        else
        {
            output.texture = device.createTextureRgba8(
                image.width, image.height, image.bytes.data(),
                image.bytes.size());
        }
        if (!valid(output.texture))
            throw std::runtime_error("unable to upload " +
                                     image.virtualPath);
        return true;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return false;
    }
}

bool OriginalRaceHud::initialize(
    GraphicsDevice& device,
    const r3d::resource::ResourceFileSystem& resources,
    std::string& error)
{
    if (!loadImage(device, resources, "Data/GUI/placeMineHyper.png",
                   placeFrame_, error) ||
        !loadImage(device, resources, "Data/GUI/lifeBarBack.png",
                   lifeBack_, error) ||
        !loadImage(device, resources, "Data/GUI/lifeBar.png",
                   lifeBar_, error) ||
        !loadImage(device, resources, "Data/GUI/lap.png", lapBack_,
                   error))
    {
        shutdown(device);
        return false;
    }
    error.clear();
    return true;
}

void OriginalRaceHud::shutdown(GraphicsDevice& device) noexcept
{
    auto releaseImage = [&](ImageAsset& asset) {
        if (valid(asset.texture))
            device.destroy(asset.texture);
        asset = {};
    };
    auto releaseText = [&](TextAsset& asset) {
        if (valid(asset.texture))
            device.destroy(asset.texture);
        asset = {};
    };
    releaseText(status_);
    releaseText(telemetry_);
    releaseText(lap_);
    releaseText(place_);
    releaseImage(lapBack_);
    releaseImage(lifeBar_);
    releaseImage(lifeBack_);
    releaseImage(placeFrame_);
}

void OriginalRaceHud::setText(GraphicsDevice& device, TextAsset& output,
                              std::string value, float pointSize,
                              bool bold, menu::Rgba8 color)
{
    if (value == output.value)
        return;
    if (valid(output.texture))
        device.destroy(output.texture);
    output = {};
    output.value = std::move(value);
    if (output.value.empty())
        return;
    const auto bitmap = rrr3d::macos::rasterizeText(
        output.value, menu::fontFace, pointSize, bold, color);
    output.texture = device.createTextureRgba8(
        bitmap.width, bitmap.height, bitmap.rgba.data(),
        bitmap.rgba.size());
    output.width = static_cast<float>(bitmap.width);
    output.height = static_cast<float>(bitmap.height);
}

void OriginalRaceHud::update(
    GraphicsDevice& device, const originalrace::Race& race,
    const originalrace::OriginalRaceSession& session,
    const r3d::physics::VehicleState& vehicle)
{
    if (session.racers().empty())
        return;
    const auto& player = session.racers().front();
    const auto white = menu::Rgba8{255, 255, 255, 255};
    const auto orange = menu::Rgba8{255, 138, 112, 255};
    const auto red = menu::Rgba8{255, 80, 60, 255};

    setText(device, place_,
            std::to_string(player.place) + " / " +
                std::to_string(session.racers().size()),
            30.0F, true, white);
    const std::uint32_t shownLap =
        std::min(player.completedLaps + 1U, race.lapCount);
    setText(device, lap_,
            "LAP " + std::to_string(shownLap) + " / " +
                std::to_string(race.lapCount),
            25.0F, true, white);

    std::ostringstream telemetry;
    telemetry << std::fixed << std::setprecision(0)
              << std::abs(vehicle.speed) * 3.6F << " km/h   "
              << vehicle.engineRpm << " RPM\n"
              << "AMMO " << player.ammunition << "   MINE "
              << player.mines << "   $ " << player.money;
    setText(device, telemetry_, telemetry.str(), 22.0F, true, orange);

    std::string status;
    menu::Rgba8 statusColor = white;
    if (session.phase() == originalrace::RacePhase::Countdown)
    {
        const int value = std::max(
            1, static_cast<int>(std::ceil(session.countdownSeconds())));
        status = std::to_string(value);
    }
    else if (session.phase() == originalrace::RacePhase::Paused)
    {
        status = "PAUSED";
    }
    else if (session.phase() == originalrace::RacePhase::Finished)
    {
        status = "FINISH  " + std::to_string(player.place) + " / " +
                 std::to_string(session.racers().size());
    }
    else if (player.wrongWay)
    {
        status = "WRONG WAY";
        statusColor = red;
    }
    setText(device, status_, std::move(status), 58.0F, true,
            statusColor);
    lifeFraction_ =
        std::clamp(player.life / std::max(player.maximumLife, 1.0F),
                   0.0F, 1.0F);
}

void OriginalRaceHud::draw(GraphicsDevice& device, Mesh quad,
                           Shader shader) const
{
    PipelineState pipeline;
    pipeline.faceCulling = PipelineState::FaceCulling::None;
    pipeline.writeDepth = false;
    pipeline.depthTest = false;
    pipeline.alphaBlend = true;

    drawAsset(device, quad, shader, placeFrame_.texture,
              placeFrame_.width, placeFrame_.height, 95.0F, 102.0F,
              30.0F, pipeline);
    drawAsset(device, quad, shader, place_.texture, place_.width,
              place_.height, 83.0F, 48.0F, 20.0F, pipeline);
    drawAsset(device, quad, shader, lifeBack_.texture, lifeBack_.width,
              lifeBack_.height, 140.0F, 174.0F, 20.0F, pipeline);
    const float lifeWidth = lifeBar_.width * lifeFraction_;
    drawAsset(device, quad, shader, lifeBar_.texture, lifeWidth,
              lifeBar_.height,
              31.0F + lifeWidth * 0.5F, 174.0F, 10.0F, pipeline);

    drawAsset(device, quad, shader, lapBack_.texture, lapBack_.width,
              lapBack_.height, menu::virtualWidth - 85.0F, 44.0F,
              20.0F, pipeline);
    drawAsset(device, quad, shader, lap_.texture, lap_.width,
              lap_.height, menu::virtualWidth - 85.0F, 43.0F, 10.0F,
              pipeline);
    drawAsset(device, quad, shader, telemetry_.texture,
              telemetry_.width, telemetry_.height,
              menu::virtualWidth - telemetry_.width * 0.5F - 28.0F,
              menu::virtualHeight - telemetry_.height * 0.5F - 28.0F,
              10.0F, pipeline);
    drawAsset(device, quad, shader, status_.texture, status_.width,
              status_.height, menu::virtualWidth * 0.5F,
              menu::virtualHeight * 0.36F, 5.0F, pipeline);
}

} // namespace rrr3d::race
