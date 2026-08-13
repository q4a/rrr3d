#include "CoreTextRasterizer.h"
#include "OriginalAudioSpec.h"
#include "OriginalMainMenu.h"
#ifdef RRR3D_NETWORK
#include "OriginalNetwork.h"
#endif
#ifdef RRR3D_PHYSICS
#include "OriginalGarage.h"
#include "OriginalProfile.h"
#include "OriginalRace.h"
#include "OriginalRaceHud.h"
#include "OriginalRaceRenderer.h"
#include "OriginalRaceSession.h"
#include "OriginalUserChat.h"
#include "OriginalWorkshopRenderer.h"
#include "physics/OriginalVehiclePhysics.h"
#endif
#include "renderer/BgfxGraphicsDevice.h"
#include "resource/ResourceFileSystem.h"
#include "xplatform.h"

#ifdef RRR3D_GAMEPAD_INPUT
#include "SdlInputManager.h"
#include "SdlInputSmoke.h"
#endif
#ifdef RRR3D_AUDIO
#include "OriginalMenuMusic.h"
#include "OriginalMenuSounds.h"
#ifdef RRR3D_PHYSICS
#include "OriginalRaceCommentator.h"
#include "OriginalSpatialAudio.h"
#endif
#include "SdlAudioBackend.h"
#include "SdlAudioSmoke.h"
#include "audio/AudioBackend.h"
#endif
#ifdef RRR3D_VIDEO
#include "MacVideoPlayer.h"
#endif

#include <SDL3/SDL.h>
#include <bx/math.h>

#include "rrr3d_fs_static_scene.bin.h"
#include "rrr3d_vs_static_scene.bin.h"
#ifdef RRR3D_PHYSICS
#include "rrr3d_fs_original_race.bin.h"
#include "rrr3d_vs_original_race.bin.h"
#endif

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using namespace r3d::renderer;
namespace menu = r3d::game::mainmenu2;
namespace originalaudio = r3d::game::originalaudio;

constexpr int initialWidth = 1280;
constexpr int initialHeight = 733;
constexpr std::string_view smokePrefix = "--smoke-test-frames=";
constexpr std::string_view dataPrefix = "--data-dir=";
constexpr std::string_view languagePrefix = "--language=";
constexpr std::string_view trackPrefix = "--track=";
constexpr std::string_view carPrefix = "--car=";
constexpr std::string_view weatherPrefix = "--weather=";

struct Options
{
    std::uint32_t smokeFrames = 0;
    std::filesystem::path dataDirectory;
    std::string language;
    bool languageSelected = false;
    bool verifyResources = false;
    bool startupSmokeTest = false;
    bool finalMenuSmokeTest = false;
#ifdef RRR3D_NETWORK
    bool networkMenuSmokeTest = false;
#endif
#ifdef RRR3D_VIDEO
    bool videoSmokeTest = false;
#endif
#ifdef RRR3D_PHYSICS
    bool physicsSmokeTest = false;
    bool raceRenderSmokeTest = false;
    bool finishMenuSmokeTest = false;
    bool gamersFrameSmokeTest = false;
    bool startOptionsSmokeTest = false;
    std::uint32_t trackIndex = 0;
    bool trackSelected = false;
    std::string car;
    bool carSelected = false;
    std::string weather = "fair";
    bool weatherSelected = false;
#endif
#ifdef RRR3D_GAMEPAD_INPUT
    bool inputSmokeTest = false;
#endif
#ifdef RRR3D_AUDIO
    bool audioSmokeTest = false;
#endif
};

#ifdef RRR3D_PHYSICS
struct OriginalAchievementVisual
{
    std::string_view name;
    std::string_view lockedImage;
    std::string_view openedImage;
    float x;
    float y;
};

// RaceMenu2::AchievmentFrame::UpdateAchievments keeps these arrays in Box
// order.  armor4 intentionally uses the original musicTrack artwork.
constexpr std::array<OriginalAchievementVisual, 9>
    originalAchievementVisuals{{
        {"viper", "Data/GUI/Rewards/viperLock.png",
         "Data/GUI/Rewards/viper.png", 200.0F, 105.0F},
        {"buggi", "Data/GUI/Rewards/buggiLock.png",
         "Data/GUI/Rewards/buggi.png", 405.0F, 155.0F},
        {"airblade", "Data/GUI/Rewards/airbladeLock.png",
         "Data/GUI/Rewards/airblade.png", 0.0F, 245.0F},
        {"reflector", "Data/GUI/Rewards/reflectorLock.png",
         "Data/GUI/Rewards/reflector.png", -190.0F, 125.0F},
        {"droid", "Data/GUI/Rewards/droidLock.png",
         "Data/GUI/Rewards/droid.png", -380.0F, 95.0F},
        {"tankchetti", "Data/GUI/Rewards/tankchettiLock.png",
         "Data/GUI/Rewards/tankchetti.png", -325.0F, 265.0F},
        {"phaser", "Data/GUI/Rewards/phaserLock.png",
         "Data/GUI/Rewards/phaser.png", -190.0F, 400.0F},
        {"mustang", "Data/GUI/Rewards/mustangLock.png",
         "Data/GUI/Rewards/mustang.png", 205.0F, 375.0F},
        {"armor4", "Data/GUI/Rewards/musicTrackLock.png",
         "Data/GUI/Rewards/musicTrack.png", 445.0F, 315.0F},
    }};

constexpr std::size_t originalAchievementBack =
    originalAchievementVisuals.size();
constexpr std::size_t originalAchievementNoTarget =
    originalAchievementBack + 1U;

r3d::physics::Vec3 rotateRaceVector(
    const r3d::physics::Quat& rotation,
    const r3d::physics::Vec3& value) noexcept
{
    const r3d::physics::Vec3 twiceCross{
        2.0F * (rotation.y * value.z - rotation.z * value.y),
        2.0F * (rotation.z * value.x - rotation.x * value.z),
        2.0F * (rotation.x * value.y - rotation.y * value.x)};
    return {
        value.x + rotation.w * twiceCross.x +
            rotation.y * twiceCross.z - rotation.z * twiceCross.y,
        value.y + rotation.w * twiceCross.y +
            rotation.z * twiceCross.x - rotation.x * twiceCross.z,
        value.z + rotation.w * twiceCross.z +
            rotation.x * twiceCross.y - rotation.y * twiceCross.x};
}

// Menu::NavDir order is left, right, up, down.  This is the literal
// AchievmentFrame navigation graph; locked buttons are skipped by following
// the same direction, as Menu::NavElementFind did on Windows.
constexpr std::array<std::array<std::size_t, 4>, 10>
    originalAchievementNavigation{{
        {3U, 1U, originalAchievementBack, 2U}, // viper
        {0U, 4U, originalAchievementBack, 8U}, // buggi
        {5U, 8U, 3U, 6U},                     // airblade
        {4U, 0U, originalAchievementBack, 5U}, // reflector
        {1U, 3U, originalAchievementBack, 5U}, // droid
        {8U, 2U, 4U, 6U},                     // tankchetti
        {5U, 7U, 2U, originalAchievementBack}, // phaser
        {6U, 8U, 2U, originalAchievementBack}, // mustang
        {7U, 5U, 1U, originalAchievementBack}, // musicTrack
        {originalAchievementNoTarget, originalAchievementNoTarget, 6U,
         6U}, // Back
    }};
#endif

struct TextVisual
{
    Texture texture;
    float width = 0.0F;
    float height = 0.0F;
};

#ifdef RRR3D_PHYSICS
struct UserChatLineVisual
{
    TextVisual name;
    std::vector<TextVisual> text;
};

struct UserChatVisual
{
    std::vector<UserChatLineVisual> lines;
    TextVisual inputName;
    TextVisual inputText;
    std::uint64_t revision = std::numeric_limits<std::uint64_t>::max();
};

#ifdef RRR3D_NETWORK
struct NetworkRacePlayerVisual
{
    r3d::game::originalnetwork::NetworkPlayerState player;
    TextVisual name;
    TextVisual readyLabel;
    std::optional<std::size_t> photoIndex;
};
#endif

struct WorkshopWeaponDialogVisual
{
    TextVisual name;
    std::vector<TextVisual> info;
    TextVisual money;
    TextVisual damage;
    std::string itemRecord;
    std::uint32_t cost = 0U;
    float centerX = 0.0F;
    float centerY = 0.0F;
    bool visible = false;
};

struct InfoDialogVisual
{
    TextVisual title;
    std::vector<TextVisual> info;
    TextVisual ok;
    float centerX = 0.0F;
    float centerY = 0.0F;
    bool visible = false;
    bool dismissable = true;
};

struct AcceptDialogVisual
{
    std::vector<TextVisual> info;
    TextVisual yes;
    TextVisual no;
    float centerX = 0.0F;
    float centerY = 0.0F;
    float frameWidth = 0.0F;
    float frameHeight = 0.0F;
    float infoWidth = 0.0F;
    float infoHeight = 0.0F;
    float buttonWidth = 0.0F;
    float buttonHeight = 0.0F;
    float yesOffsetX = 0.0F;
    float noOffsetX = 0.0F;
    float buttonOffsetY = 0.0F;
    bool maxMode = false;
    bool disableFocus = false;
    std::optional<bool> hoveredChoice;
};
#endif

#ifdef RRR3D_AUDIO
struct MusicDialogVisual
{
    TextVisual title;
    TextVisual info;
};
#endif

std::string_view recordName(std::string_view record)
{
    const auto separator = record.find_last_of("\\/");
    return record.substr(separator == std::string_view::npos
                             ? 0
                             : separator + 1);
}

#ifdef RRR3D_VIDEO
std::filesystem::path movieCachePath(
    const std::filesystem::path& gameDataRoot,
    std::string_view sourceMovie)
{
    std::string normalized(sourceMovie);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    auto name = std::filesystem::path(normalized).stem();
    name += ".mp4";
    return gameDataRoot.parent_path() / "video-cache" / name;
}
#endif

#ifdef RRR3D_AUDIO
template <std::size_t Count>
std::vector<r3d::game::MusicCatTrack> musicTracks(
    const std::array<originalaudio::TrackSpec, Count>& source)
{
    std::vector<r3d::game::MusicCatTrack> result;
    result.reserve(source.size());
    for (const auto& track : source)
        result.push_back(
            {track.path, track.name, track.band, track.group});
    return result;
}

std::vector<std::size_t> musicPlaylist(std::string_view source,
                                       std::size_t trackCount)
{
    std::vector<std::size_t> result;
    while (!source.empty())
    {
        const auto separator = source.find(',');
        const auto token = source.substr(0, separator);
        std::size_t index = 0;
        const auto parsed = std::from_chars(
            token.data(), token.data() + token.size(), index);
        if (parsed.ec == std::errc{} &&
            parsed.ptr == token.data() + token.size() &&
            index < trackCount &&
            std::find(result.begin(), result.end(), index) ==
                result.end())
        {
            result.push_back(index);
        }
        if (separator == std::string_view::npos)
            break;
        source.remove_prefix(separator + 1U);
    }
    return result;
}
#endif

std::optional<Options> parseOptions(int argc, char** argv)
{
    Options options;
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);
        if (argument == "--verify-resources")
        {
            options.verifyResources = true;
            continue;
        }
        if (argument == "--startup-smoke-test")
        {
            options.startupSmokeTest = true;
            if (options.smokeFrames == 0)
                options.smokeFrames = 60;
            continue;
        }
        if (argument == "--final-menu-smoke-test")
        {
            options.finalMenuSmokeTest = true;
            if (options.smokeFrames == 0)
                options.smokeFrames = 1800;
            continue;
        }
#ifdef RRR3D_NETWORK
        if (argument == "--network-menu-smoke-test")
        {
            options.networkMenuSmokeTest = true;
            if (options.smokeFrames == 0)
                options.smokeFrames = 330;
            continue;
        }
#endif
#ifdef RRR3D_VIDEO
        if (argument == "--video-smoke-test")
        {
            options.videoSmokeTest = true;
            if (options.smokeFrames == 0)
                options.smokeFrames = 600;
            continue;
        }
#endif
#ifdef RRR3D_PHYSICS
        if (argument.substr(0, trackPrefix.size()) == trackPrefix)
        {
            const auto value = argument.substr(trackPrefix.size());
            const auto result = std::from_chars(
                value.data(), value.data() + value.size(),
                options.trackIndex);
            if (result.ec != std::errc{} ||
                result.ptr != value.data() + value.size())
                return std::nullopt;
            options.trackSelected = true;
            continue;
        }
        if (argument.substr(0, carPrefix.size()) == carPrefix)
        {
            options.car = argument.substr(carPrefix.size());
            if (options.car.empty())
                return std::nullopt;
            options.carSelected = true;
            continue;
        }
        if (argument.substr(0, weatherPrefix.size()) == weatherPrefix)
        {
            options.weather = argument.substr(weatherPrefix.size());
            if (options.weather != "fair" &&
                options.weather != "night" &&
                options.weather != "cloudy" &&
                options.weather != "rainy" &&
                options.weather != "sahara" &&
                options.weather != "hell" &&
                options.weather != "snow")
                return std::nullopt;
            options.weatherSelected = true;
            continue;
        }
        if (argument == "--physics-smoke-test")
        {
            options.physicsSmokeTest = true;
            continue;
        }
        if (argument == "--race-render-smoke-test")
        {
            options.raceRenderSmokeTest = true;
            if (options.smokeFrames == 0)
                options.smokeFrames = 240;
            continue;
        }
        if (argument == "--finish-menu-smoke-test")
        {
            options.finishMenuSmokeTest = true;
            if (options.smokeFrames == 0)
                options.smokeFrames = 300;
            continue;
        }
        if (argument == "--gamers-frame-smoke-test")
        {
            options.gamersFrameSmokeTest = true;
            if (options.smokeFrames == 0)
                options.smokeFrames = 120;
            continue;
        }
        if (argument == "--start-options-smoke-test")
        {
            options.startOptionsSmokeTest = true;
            if (options.smokeFrames == 0)
                options.smokeFrames = 90;
            continue;
        }
#endif
#ifdef RRR3D_GAMEPAD_INPUT
        if (argument == "--input-smoke-test")
        {
            options.inputSmokeTest = true;
            if (options.smokeFrames == 0)
                options.smokeFrames = 120;
            continue;
        }
#endif
#ifdef RRR3D_AUDIO
        if (argument == "--audio-smoke-test")
        {
            options.audioSmokeTest = true;
            if (options.smokeFrames == 0)
                options.smokeFrames = 180;
            continue;
        }
#endif
        if (argument.substr(0, dataPrefix.size()) == dataPrefix)
        {
            const auto value = argument.substr(dataPrefix.size());
            if (value.empty())
                return std::nullopt;
            options.dataDirectory = std::filesystem::path(value);
            continue;
        }
        if (argument.substr(0, languagePrefix.size()) == languagePrefix)
        {
            options.language = argument.substr(languagePrefix.size());
            if (options.language.empty())
                return std::nullopt;
            options.languageSelected = true;
            continue;
        }
        if (argument.substr(0, smokePrefix.size()) == smokePrefix)
        {
            const auto value = argument.substr(smokePrefix.size());
            const auto result = std::from_chars(
                value.data(), value.data() + value.size(),
                options.smokeFrames);
            if (result.ec != std::errc{} ||
                result.ptr != value.data() + value.size() ||
                options.smokeFrames == 0)
                return std::nullopt;
            continue;
        }
        return std::nullopt;
    }
    if (options.language.empty())
        options.language = rrr3d::macos::preferredGameLanguage();
    return options;
}

bool valid(Texture value)
{
    return value.value != invalid_resource;
}

bool valid(Mesh value)
{
    return value.vertices.value != invalid_resource &&
           value.indices.value != invalid_resource;
}

bool valid(Shader value)
{
    return value.value != invalid_resource;
}

Camera makeCamera(const GraphicsDevice& device)
{
    Camera camera;
    bx::mtxIdentity(camera.view.data());
    bx::mtxOrtho(camera.projection.data(), 0.0F, menu::virtualWidth,
                 menu::virtualHeight, 0.0F, 0.0F, 100.0F, 0.0F,
                 device.usesHomogeneousDepth());
    return camera;
}

#ifdef RRR3D_PHYSICS
r3d::physics::VehicleState makeGarageVehicleState(
    const r3d::game::originalrace::Vehicle& vehicle)
{
    r3d::physics::VehicleState result;
    float wheelOffset = 0.0F;
    const std::size_t wheelCount = std::min(
        vehicle.physics.wheels.size(),
        vehicle.wheelVisualOffsets.size());
    for (std::size_t index = 0; index < wheelCount; ++index)
    {
        const auto& wheel = vehicle.physics.wheels[index];
        const auto& offset = vehicle.wheelVisualOffsets[index];
        const float adjustedZ =
            wheel.position.z - 0.5F * wheel.suspensionTravel +
            offset.z;
        // CarFrame::SetCar overwrites this on every wheel; four-wheel
        // source cars therefore use the final serialized wheel exactly.
        wheelOffset =
            std::abs(adjustedZ) + wheel.radius + offset.z;
    }
    result.body.position = {0.0F, 0.0F, wheelOffset - 0.71F};
    result.wheels.reserve(wheelCount);
    result.wheelAngularSpeeds.assign(wheelCount, 0.0F);
    result.wheelContacts.resize(wheelCount);
    for (std::size_t index = 0; index < wheelCount; ++index)
    {
        const auto& wheel = vehicle.physics.wheels[index];
        r3d::physics::Transform state;
        state.position = {
            wheel.position.x,
            wheel.position.y,
            result.body.position.z + wheel.position.z -
                0.5F * wheel.suspensionTravel};
        if (index < vehicle.wheelVisualTransforms.size() &&
            vehicle.wheelVisualTransforms[index].scale.y < 0.0F)
        {
            // CarWheel::invertWheel in CarFrame::SetCar.
            state.rotation = {0.0F, 0.0F, 1.0F, 0.0F};
        }
        result.wheels.push_back(state);
    }
    return result;
}

r3d::game::originalrace::PresentationCamera
makeWorkshopPresentationCamera(
    const r3d::game::originalrace::PresentationCamera& current) noexcept
{
    using Camera =
        r3d::game::originalrace::PresentationCamera;
    using Vec3 = r3d::physics::Vec3;
    using Quat = r3d::physics::Quat;
    constexpr std::array<Vec3, 8> positions{{
        {-5.6156259F, 4.3894496F, 1.3072476F},
        {1.0063084F, 6.9253764F, 1.7360222F},
        {5.6724834F, 4.9537153F, 1.3952403F},
        {7.0655332F, -1.0402107F, 1.2024049F},
        {5.4610982F, -5.3067584F, 1.2650701F},
        {-1.1062316F, -7.5020962F, 1.1599010F},
        {-5.9399834F, -4.8825927F, 1.0367264F},
        {-7.4102926F, 0.61909121F, 1.1492375F},
    }};
    constexpr std::array<Quat, 8> rotations{{
        {0.021078700F, 0.10266567F, -0.20001189F, 0.97417259F},
        {0.093931124F, 0.099822313F, -0.67882264F, 0.72140080F},
        {0.098612130F, 0.053653944F, -0.87284911F, 0.47491166F},
        {0.10475823F, 0.0032271212F, -0.99402952F, 0.030623097F},
        {0.10137362F, -0.026610103F, -0.96191424F, -0.25249690F},
        {0.070100352F, -0.063905962F, -0.73568070F, -0.67066783F},
        {0.041057255F, -0.077143900F, -0.46803388F, -0.87939179F},
        {0.0077582477F, -0.092043117F, -0.083636492F, -0.99221748F},
    }};
    auto length = [](const Quat& value) {
        return std::sqrt(
            value.x * value.x + value.y * value.y +
            value.z * value.z + value.w * value.w);
    };
    const float currentLength = std::max(length(current.rotation), 0.0001F);
    std::size_t nearest = 0U;
    float smallestAngle = std::numeric_limits<float>::max();
    for (std::size_t index = 0U; index < rotations.size(); ++index)
    {
        const auto& candidate = rotations[index];
        const float dot =
            current.rotation.x * candidate.x +
            current.rotation.y * candidate.y +
            current.rotation.z * candidate.z +
            current.rotation.w * candidate.w;
        const float cosine = std::clamp(
            std::abs(dot) /
                (currentLength * std::max(length(candidate), 0.0001F)),
            0.0F, 1.0F);
        const float angle = std::acos(cosine) * 2.0F;
        if (angle < smallestAngle)
        {
            smallestAngle = angle;
            nearest = index;
        }
    }
    Camera result = current;
    result.position = positions[nearest];
    result.rotation = rotations[nearest];
    return result;
}

r3d::game::originalrace::PresentationCamera
makeAutoObserverPresentationCamera(
    const r3d::game::originalrace::PresentationCamera& source,
    float seconds, float angularSpeed) noexcept
{
    auto result = source;
    const float angle = seconds * angularSpeed;
    const float sine = std::sin(angle);
    const float cosine = std::cos(angle);
    result.position = {
        cosine * source.position.x - sine * source.position.y,
        sine * source.position.x + cosine * source.position.y,
        source.position.z};
    const float halfSine = std::sin(angle * 0.5F);
    const float halfCosine = std::cos(angle * 0.5F);
    result.rotation = {
        halfCosine * source.rotation.x -
            halfSine * source.rotation.y,
        halfCosine * source.rotation.y +
            halfSine * source.rotation.x,
        halfCosine * source.rotation.z +
            halfSine * source.rotation.w,
        halfCosine * source.rotation.w -
            halfSine * source.rotation.z};
    return result;
}
#endif

Transform makeTransform(float width, float height, float centerX,
                        float centerY, float depth)
{
    Transform transform;
    bx::mtxSRT(transform.matrix.data(), width, height, 1.0F, 0.0F, 0.0F,
               0.0F, centerX, centerY, depth);
    return transform;
}

Texture createImageTexture(GraphicsDevice& device, const menu::Image& image)
{
    if (image.storage == menu::ImageStorage::EncodedContainer)
    {
        return device.createTextureContainer(image.bytes.data(),
                                             image.bytes.size(),
                                             image.virtualPath);
    }
    return device.createTextureRgba8(image.width, image.height,
                                     image.bytes.data(), image.bytes.size());
}

Texture createImageTextureWithAlpha(
    GraphicsDevice& device, const menu::Image& image,
    std::uint8_t alphaNumerator, std::uint8_t alphaDenominator)
{
    if (image.storage != menu::ImageStorage::Rgba8 ||
        alphaDenominator == 0U)
    {
        return createImageTexture(device, image);
    }
    auto bytes = image.bytes;
    for (std::size_t index = 3U; index < bytes.size(); index += 4U)
    {
        bytes[index] = static_cast<std::uint8_t>(
            static_cast<std::uint16_t>(bytes[index]) *
            alphaNumerator / alphaDenominator);
    }
    return device.createTextureRgba8(
        image.width, image.height, bytes.data(), bytes.size());
}

TextVisual createText(GraphicsDevice& device, std::string_view text,
                      float pointSize, bool bold, menu::Rgba8 color,
                      std::string& resolvedFont)
{
    auto bitmap = rrr3d::macos::rasterizeText(
        text, menu::fontFace, pointSize, bold, color);
    if (resolvedFont.empty())
        resolvedFont = bitmap.resolvedFontName;
    const Texture texture = device.createTextureRgba8(
        bitmap.width, bitmap.height, bitmap.rgba.data(), bitmap.rgba.size());
    return {texture, static_cast<float>(bitmap.width),
            static_cast<float>(bitmap.height)};
}

#ifdef RRR3D_PHYSICS
void destroyUserChatVisual(GraphicsDevice& device, UserChatVisual& visual)
{
    for (auto& line : visual.lines)
    {
        if (valid(line.name.texture))
            device.destroy(line.name.texture);
        for (auto& text : line.text)
            if (valid(text.texture))
                device.destroy(text.texture);
    }
    if (valid(visual.inputName.texture))
        device.destroy(visual.inputName.texture);
    if (valid(visual.inputText.texture))
        device.destroy(visual.inputText.texture);
    visual = {};
    visual.revision = std::numeric_limits<std::uint64_t>::max();
}

void destroyWorkshopWeaponDialog(
    GraphicsDevice& device, WorkshopWeaponDialogVisual& dialog)
{
    if (valid(dialog.name.texture))
        device.destroy(dialog.name.texture);
    for (auto& line : dialog.info)
    {
        if (valid(line.texture))
            device.destroy(line.texture);
    }
    if (valid(dialog.money.texture))
        device.destroy(dialog.money.texture);
    if (valid(dialog.damage.texture))
        device.destroy(dialog.damage.texture);
    dialog = {};
}

void destroyInfoDialog(
    GraphicsDevice& device, InfoDialogVisual& dialog)
{
    if (valid(dialog.title.texture))
        device.destroy(dialog.title.texture);
    for (auto& line : dialog.info)
    {
        if (valid(line.texture))
            device.destroy(line.texture);
    }
    if (valid(dialog.ok.texture))
        device.destroy(dialog.ok.texture);
    dialog = {};
}

void destroyAcceptDialog(
    GraphicsDevice& device, AcceptDialogVisual& dialog)
{
    for (auto& line : dialog.info)
    {
        if (valid(line.texture))
            device.destroy(line.texture);
    }
    if (valid(dialog.yes.texture))
        device.destroy(dialog.yes.texture);
    if (valid(dialog.no.texture))
        device.destroy(dialog.no.texture);
    dialog = {};
}
#endif

void drawQuad(GraphicsDevice& device, Mesh quad, Shader shader,
              Texture texture, float width, float height, float centerX,
              float centerY, float depth, const PipelineState& pipeline)
{
    device.draw(quad, shader, texture,
                makeTransform(width, height, centerX, centerY, depth),
                pipeline);
}

void drawQuadTinted(GraphicsDevice& device, Mesh quad, Shader shader,
                    Texture texture, float width, float height,
                    float centerX, float centerY, float depth,
                    const PipelineState& pipeline,
                    const std::array<float, 4>& color)
{
    MaterialState material;
    material.color = color;
    device.draw(quad, shader, texture,
                makeTransform(width, height, centerX, centerY, depth),
                pipeline, {}, material);
}

void drawQuadRotated(GraphicsDevice& device, Mesh quad, Shader shader,
                     Texture texture, float width, float height,
                     float centerX, float centerY, float depth,
                     float rotation, const PipelineState& pipeline)
{
    Transform transform;
    bx::mtxSRT(
        transform.matrix.data(), width, height, 1.0F, 0.0F, 0.0F,
        rotation, centerX, centerY, depth);
    device.draw(quad, shader, texture, transform, pipeline);
}

#ifdef RRR3D_GAMEPAD_INPUT
std::optional<std::size_t> hoveredItem(SDL_Window* window, float windowX,
                                       float windowY, std::size_t itemCount,
                                       float itemWidth, float itemHeight,
                                       bool lastItemAtBackPosition)
{
    int windowWidth = 0;
    int windowHeight = 0;
    if (!SDL_GetWindowSize(window, &windowWidth, &windowHeight) ||
        windowWidth <= 0 || windowHeight <= 0)
        return std::nullopt;

    const float virtualX =
        windowX * menu::virtualWidth / static_cast<float>(windowWidth);
    const float virtualY =
        windowY * menu::virtualHeight / static_cast<float>(windowHeight);
    const float centerX =
        menu::virtualWidth * 0.5F + menu::itemCenterOffsetX;
    if (std::abs(virtualX - centerX) > itemWidth * 0.5F)
        return std::nullopt;
    for (std::size_t index = 0; index < itemCount; ++index)
    {
        const float centerY =
            lastItemAtBackPosition && index + 1U == itemCount
                ? menu::virtualHeight * 0.5F + 150.0F
                : menu::virtualHeight * 0.5F +
                      menu::firstItemOffsetY +
                      static_cast<float>(index) *
                          menu::itemSpacing;
        if (std::abs(virtualY - centerY) <= itemHeight * 0.5F)
            return index;
    }
    return std::nullopt;
}
#endif

#ifdef RRR3D_PHYSICS
void applyWeather(
    r3d::game::originalrace::EnvironmentDescription& environment,
    std::string_view weather, std::string_view levelPath)
{
    using r3d::game::originalrace::Weather;
    environment.rain = false;
    environment.directionalLightEnabled = weather != "night";
    environment.directionalShadowMinimumQuality =
        weather == "snow" ? 2U : 1U;
    if (weather == "night")
    {
        environment.weather = Weather::Night;
        environment.skyTexturePath = "Data/Misc/nightSky.dds";
        environment.fogColor = {15.0F / 255.0F, 25.0F / 255.0F,
                                31.0F / 255.0F, 1.0F};
        environment.ambientColor =
            {138.0F / 255.0F, 144.0F / 255.0F,
             174.0F / 255.0F, 1.0F};
        environment.fogIntensity = 1.0F;
        environment.perspectiveFarDistance = 120.0F;
    }
    else if (weather == "cloudy" || weather == "rainy")
    {
        environment.weather =
            weather == "rainy" ? Weather::Rainy : Weather::Cloudy;
        environment.skyTexturePath =
            "Data/World2/texture/skyTex1.dds";
        environment.fogColor =
            {192.0F / 255.0F, 189.0F / 255.0F,
             184.0F / 255.0F, 0.0F};
        environment.ambientColor = {0.0F, 0.0F, 0.0F, 1.0F};
        environment.fogIntensity = 1.0F;
        environment.rain = weather == "rainy";
        environment.perspectiveFarDistance =
            weather == "rainy" ? 100.0F : 120.0F;
    }
    else if (weather == "sahara")
    {
        environment.weather = Weather::Sahara;
        environment.skyTexturePath =
            "Data/World3/Texture/skyTex1.dds";
        environment.fogColor =
            {87.0F / 255.0F, 81.0F / 255.0F,
             115.0F / 255.0F, 1.0F};
        environment.ambientColor = {0.0F, 0.0F, 0.0F, 1.0F};
        environment.fogIntensity = 0.5F;
        environment.perspectiveFarDistance = 100.0F;
    }
    else if (weather == "hell")
    {
        environment.weather = Weather::Hell;
        environment.skyTexturePath =
            "Data/World4/Texture/skyTex1.dds";
        environment.fogColor =
            {82.0F / 255.0F, 12.0F / 255.0F,
             8.0F / 255.0F, 1.0F};
        environment.ambientColor = {0.0F, 0.0F, 0.0F, 1.0F};
        environment.fogIntensity = 0.5F;
        environment.perspectiveFarDistance = 100.0F;
    }
    else if (weather == "snow")
    {
        environment.weather = Weather::Snow;
        environment.skyTexturePath =
            "Data/World5/Texture/sky_text.dds";
        environment.fogColor =
            {156.0F / 255.0F, 166.0F / 255.0F,
             181.0F / 255.0F, 1.0F};
        environment.ambientColor = {0.0F, 0.0F, 0.0F, 1.0F};
        environment.fogIntensity = 0.5F;
        environment.perspectiveFarDistance = 100.0F;
    }
    else
    {
        environment.weather = Weather::Fair;
        environment.skyTexturePath =
            "Data/World1/Texture/skyTex1.dds";
        environment.fogColor =
            {148.0F / 255.0F, 193.0F / 255.0F,
             235.0F / 255.0F, 1.0F};
        environment.ambientColor = {0.0F, 0.0F, 0.0F, 1.0F};
        environment.fogIntensity = 0.5F;
        environment.perspectiveFarDistance = 120.0F;
    }
    environment.surfaceCloudColor = environment.fogColor;
    if (levelPath.find("World3") != std::string_view::npos)
    {
        environment.surfaceCloudColor = {
            87.0F / 255.0F, 81.0F / 255.0F,
            115.0F / 255.0F, 1.0F};
    }
    else if (levelPath.find("World4") != std::string_view::npos)
    {
        environment.surfaceCloudColor = {1.0F, 1.0F, 1.0F, 1.0F};
    }
}

std::string_view weatherToken(
    r3d::game::originalrace::Weather weather) noexcept
{
    using r3d::game::originalrace::Weather;
    switch (weather)
    {
    case Weather::Night:
        return "night";
    case Weather::Cloudy:
        return "cloudy";
    case Weather::Rainy:
        return "rainy";
    case Weather::Sahara:
        return "sahara";
    case Weather::Hell:
        return "hell";
    case Weather::Snow:
        return "snow";
    case Weather::Fair:
        return "fair";
    }
    return "fair";
}
#endif

} // namespace

int main(int argc, char** argv)
{
    const auto options = parseOptions(argc, argv);
    if (!options)
    {
        std::cerr << "Usage: RRR3d [--data-dir=PATH] "
                     "[--language=english|russian] [--verify-resources] "
                     "[--smoke-test-frames=N] "
                     "[--startup-smoke-test] "
                     "[--final-menu-smoke-test]"
#ifdef RRR3D_NETWORK
                     " [--network-menu-smoke-test]"
#endif
#ifdef RRR3D_VIDEO
                     " [--video-smoke-test]"
#endif
#ifdef RRR3D_GAMEPAD_INPUT
                     " [--input-smoke-test]"
#endif
#ifdef RRR3D_AUDIO
                     " [--audio-smoke-test]"
#endif
#ifdef RRR3D_PHYSICS
                     " [--track=0..87] [--car=garage-record] "
                     "[--weather=fair|night|cloudy|rainy|sahara|hell|snow] "
                     "[--physics-smoke-test] [--race-render-smoke-test] "
                     "[--finish-menu-smoke-test] "
                     "[--gamers-frame-smoke-test] "
                     "[--start-options-smoke-test]"
#endif
                     "\n";
        return EXIT_FAILURE;
    }
    // World::RunGame calls GameMode::Run(true) in the shipped build.  Keep
    // renderer/test fixtures immediate, but preserve the release startup
    // sequence for an ordinary launch and for its dedicated regression.
    const bool sourceStartupRequested =
        options->startupSmokeTest || options->smokeFrames == 0U;

    std::string directoryError;
    if (!rrr3d::platform::ensure_application_directories(directoryError))
    {
        std::cerr << "Application-directory setup failed: "
                  << directoryError << '\n';
        return EXIT_FAILURE;
    }

    const auto dataDirectory =
        options->dataDirectory.empty()
            ? r3d::resource::defaultGameDataDirectory()
            : options->dataDirectory;

    std::optional<r3d::resource::ResourceFileSystem> resources;
    std::optional<menu::Model> model;
    std::string activeLanguage = options->language;
#ifdef RRR3D_PHYSICS
    std::optional<r3d::game::originalrace::Race> originalRace;
    std::optional<r3d::game::originalrace::Race> originalGarageScene;
    std::optional<r3d::game::originalrace::Race> originalAngarScene;
    std::optional<r3d::game::originalrace::OriginalGarageCatalog>
        originalGarage;
    std::optional<r3d::physics::WorldDescription> physicsDescription;
    r3d::game::originalrace::OriginalProfileStore profileStore(
        rrr3d::platform::save_directory(), dataDirectory);
    std::string profileWarning;
    auto profileState = profileStore.load(profileWarning);
    if (!profileWarning.empty())
        std::cerr << "Profile import warning: " << profileWarning << '\n';
    if (options->physicsSmokeTest)
    {
        profileState =
            r3d::game::originalrace::makeOriginalDefaultProfileState();
    }
    // GameMode::LoadGameOpt does not treat pcIsometric as proof that the
    // player selected a camera: an absent prefCamera field opens the
    // mandatory StartOptionsMenu.  Other regression fixtures intentionally
    // start at their target screen; the dedicated fixture forces this path.
    bool sourcePreferredCameraAutodetect =
        options->startOptionsSmokeTest ||
        (options->smokeFrames == 0U &&
         !profileState.preferredCameraSerialized);
    // Metal on Apple Silicon is one capable unified GPU.  Report it through
    // the source's "discrete" compatibility bit so CheckStartupMenu keeps
    // sfrFixed without showing a misleading Windows hybrid-GPU warning.
    constexpr bool sourceCurrentDiscreteVideoCard = true;
    bool sourceDiscreteVideoChanged =
        options->smokeFrames == 0U &&
        (!profileState.discreteVideoCardSerialized ||
         profileState.config.discreteVideoCard !=
             sourceCurrentDiscreteVideoCard);
    if (options->languageSelected)
        profileState.config.language = options->language;
    else
        activeLanguage = profileState.config.language;
    std::size_t selectedTrack =
        options->trackSelected ? options->trackIndex : 0U;
    bool weatherNightPassed = false;
    const std::string selectedCar =
        options->carSelected ? options->car
                             : profileState.player.currentCar;
#endif
    try
    {
        resources.emplace(dataDirectory);
        model.emplace(
            menu::loadOriginalMainMenu(*resources, activeLanguage));
#ifdef RRR3D_PHYSICS
        originalGarage.emplace(
            r3d::game::originalrace::loadOriginalGarage(*resources));
        originalRace.emplace(r3d::game::originalrace::loadOriginalRace(
            *resources, selectedTrack, selectedCar));
        // The provenance/physics smoke has exact World1/map1 assertions and
        // must not depend on whichever tournament track a prior GUI run
        // persisted in user.xml.
        if (!options->trackSelected && !options->physicsSmokeTest)
        {
            selectedTrack =
                r3d::game::originalrace::resolveOriginalTournamentTrack(
                    *originalRace, profileState.player);
            if (selectedTrack != 0U)
            {
                originalRace.emplace(
                    r3d::game::originalrace::loadOriginalRace(
                        *resources, selectedTrack, selectedCar));
            }
        }
        if (!options->physicsSmokeTest)
        {
            r3d::game::originalrace::selectOriginalWeather(
                *resources, *originalRace,
                profileState.config.quality.light >= 1U &&
                    !weatherNightPassed,
                profileState.tutorialStage < 3U,
                static_cast<float>(std::rand()) /
                    static_cast<float>(RAND_MAX));
            weatherNightPassed =
                originalRace->environment.weather ==
                r3d::game::originalrace::Weather::Night;
        }
        if (options->weatherSelected)
            applyWeather(originalRace->environment, options->weather,
                         originalRace->levelPath);
        profileState.player.currentCar = originalRace->vehicle.record;
        if (!originalRace->racers.empty())
            originalRace->racers.front().name =
                profileState.player.name;
        r3d::game::originalrace::applyOriginalPlayerProfile(
            *originalRace, *resources, profileState.player);
        r3d::game::originalrace::writeOriginalTournamentSelection(
            *originalRace, selectedTrack, profileState.player);
        originalGarageScene.emplace(
            r3d::game::originalrace::loadOriginalGarageScene(
                *resources, *originalRace));
        originalAngarScene.emplace(
            r3d::game::originalrace::loadOriginalAngarScene(
                *resources));
        physicsDescription.emplace(
            r3d::game::originalrace::makePhysicsDescription(
                *originalRace, *resources));
#endif
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Original menu resource loading failed: "
                  << exception.what() << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "Original game-data root: " << resources->root()
              << '\n'
              << "MainMenu2 language: " << model->language << '\n'
              << "MainMenu2 strings:";
    for (const auto& item : model->items)
        std::cout << " [" << item << ']';
    std::cout << '\n'
              << "Original asset audit: " << model->audit.catalogFiles
              << " files (" << model->audit.catalogBytes << " bytes), "
              << model->audit.guiImages << " GUI images, "
              << model->audit.guiMeshes << " GUI meshes, "
              << model->audit.localizedStrings << " localized strings\n";

    if (options->verifyResources)
    {
        std::cout << "Milestone 6 original resource/MainMenu2 specification "
                     "verification passed\n";
#ifdef RRR3D_PHYSICS
        if (originalRace->vehicle.physics.maximumTorque <= 0.0F ||
            originalRace->vehicle.maximumLife <= 1.0F)
        {
            std::cerr
                << "Original player mobility loadout audit failed: torque="
                << originalRace->vehicle.physics.maximumTorque
                << ", life=" << originalRace->vehicle.maximumLife << '\n';
            return EXIT_FAILURE;
        }
        std::cout << "Milestone 9 selected race audit passed: track "
                  << selectedTrack << ' ' << originalRace->levelPath
                  << ", " << originalRace->trackInstances.size()
                  << " track objects, "
                  << originalRace->decorationInstances.size()
                  << " decorations, " << originalRace->bonuses.size()
                  << " bonuses, " << originalRace->racers.size()
                  << " racers, car "
                  << recordName(originalRace->vehicle.record)
                  << ", torque "
                  << originalRace->vehicle.physics.maximumTorque
                  << ", life " << originalRace->vehicle.maximumLife
                  << '\n';
#endif
        return EXIT_SUCCESS;
    }

#ifdef RRR3D_PHYSICS
    if (options->physicsSmokeTest)
    {
        std::string physicsError;
        if (!r3d::game::originalrace::runOriginalRaceResourceSmokeTest(
                *originalRace, *resources, physicsError) ||
            !r3d::game::originalrace::runOriginalGarageSmokeTest(
                *resources, physicsError) ||
            !r3d::game::originalrace::runOriginalProfileFlowSmokeTest(
                physicsError) ||
            !r3d::game::originalrace::
                runOriginalTournamentProgressSmokeTest(
                    physicsError) ||
            !r3d::game::originalrace::runOriginalRaceSessionSmokeTest(
                *originalRace, physicsError) ||
            !r3d::physics::runOriginalVehiclePhysicsSmokeTest(
                *physicsDescription, physicsError))
        {
            std::cerr << "Milestone 9 original race/physics smoke failed: "
                      << physicsError << '\n';
            return EXIT_FAILURE;
        }
        std::size_t collisionTriangles = 0;
        for (const auto& mesh : physicsDescription->collisionMeshes)
            collisionTriangles += mesh.indices.size() / 3U;
        std::cout << "Milestone 9 original race smoke passed: "
                  << originalRace->levelPath << ", "
                  << originalRace->trackInstances.size()
                  << " placed track objects, " << collisionTriangles
                  << " original collision triangles, "
                  << recordName(originalRace->vehicle.record)
                  << "/Jolt vehicle"
                     " source torque/gears/reverse, braking, steering,"
                     " airborne/stabilization, suspension/tire contacts,"
                     " trace reset, countdown, checkpoint/lap/finish,"
                     " source border/car contacts, weapon/damage, bonus,"
                     " garage/workshop, NetPlayer disconnect removal,"
                     " and respawn state passed\n";
        return EXIT_SUCCESS;
    }
#endif

    if (!SDL_SetAppMetadata("Motor Rock", "1.3.1",
                            "org.rrr3d.motorrock"))
    {
        std::cerr << "Unable to set SDL metadata: " << SDL_GetError()
                  << '\n';
        return EXIT_FAILURE;
    }
    SDL_InitFlags sdlFlags = SDL_INIT_VIDEO;
#ifdef RRR3D_GAMEPAD_INPUT
    sdlFlags |= SDL_INIT_GAMEPAD;
#endif
#ifdef RRR3D_AUDIO
    sdlFlags |= SDL_INIT_AUDIO;
#endif
    if (!SDL_Init(sdlFlags))
    {
        std::cerr << "Unable to initialize SDL3: " << SDL_GetError()
                  << '\n';
        return EXIT_FAILURE;
    }

#ifdef RRR3D_GAMEPAD_INPUT
    rrr3d::input::SdlInputManager input;
    std::string inputError;
    if (!input.initialize(inputError))
    {
        std::cerr << "Input initialization failed: " << inputError << '\n';
        SDL_Quit();
        return EXIT_FAILURE;
    }
#ifdef RRR3D_PHYSICS
    input.applyKeyboardBindings(
        profileState.config.keyboardControls);
    input.applyGamepadBindings(
        profileState.config.gamepadControls);
#endif
    bool runInputSmoke = options->inputSmokeTest;
#ifdef RRR3D_AUDIO
    runInputSmoke = runInputSmoke || options->audioSmokeTest;
#endif
    if (runInputSmoke &&
        (!rrr3d::input::runSdlInputSmokeTest(input, inputError) ||
         !menu::runOriginalMainMenuInputSmoke(inputError)))
    {
        std::cerr << "Milestone 7 input smoke test failed: " << inputError
                  << '\n';
        input.shutdown();
        SDL_Quit();
        return EXIT_FAILURE;
    }
    if (runInputSmoke)
    {
        std::cout << "Milestone 7 input smoke: keyboard, mouse, focus reset, "
                     "gamepad hot-plug, axes, dead zones, buttons, rumble, "
                     "and MainMenu2 command order passed\n";
#ifdef RRR3D_PHYSICS
        input.applyKeyboardBindings(
            profileState.config.keyboardControls);
        input.applyGamepadBindings(
            profileState.config.gamepadControls);
#endif
    }
#endif

    SDL_Window* window = SDL_CreateWindow(
#ifdef RRR3D_PHYSICS
        "Motor Rock - Original Race Physics (Milestone 9)", initialWidth,
#elif defined(RRR3D_AUDIO)
        "Motor Rock - Original MainMenu2 Audio (Milestone 8)", initialWidth,
#elif defined(RRR3D_GAMEPAD_INPUT)
        "Motor Rock - Original MainMenu2 Input (Milestone 7)", initialWidth,
#else
        "Motor Rock - Original MainMenu2 (Milestone 6)", initialWidth,
#endif
        initialHeight, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr)
    {
        std::cerr << "Unable to create window: " << SDL_GetError() << '\n';
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }

    const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    void* nativeWindow = SDL_GetPointerProperty(
        properties, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    int pixelWidth = 0;
    int pixelHeight = 0;
    if (nativeWindow == nullptr ||
        !SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight))
    {
        std::cerr << "Unable to obtain Cocoa drawable: " << SDL_GetError()
                  << '\n';
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }
#ifdef RRR3D_VIDEO
    rrr3d::video::MacVideoPlayer videoPlayer(nativeWindow);
#endif

    auto device = createBgfxGraphicsDevice();
    std::string rendererError;
    if (!device->initialize(
            {{nativeWindow}, static_cast<std::uint32_t>(pixelWidth),
             static_cast<std::uint32_t>(pixelHeight), true},
            rendererError))
    {
        std::cerr << "Renderer initialization failed: " << rendererError
                  << '\n';
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }

    constexpr std::array<Vertex, 4> quadVertices{{
        {-0.5F, -0.5F, 0.0F, 0xffffffffU, 0.0F, 0.0F},
        {0.5F, -0.5F, 0.0F, 0xffffffffU, 1.0F, 0.0F},
        {-0.5F, 0.5F, 0.0F, 0xffffffffU, 0.0F, 1.0F},
        {0.5F, 0.5F, 0.0F, 0xffffffffU, 1.0F, 1.0F},
    }};
    constexpr std::array<std::uint16_t, 6> quadIndices{{0, 1, 2, 1, 3, 2}};

    const Shader shader = device->createShader(
        {rrr3d_vs_static_scene, sizeof(rrr3d_vs_static_scene)},
        {rrr3d_fs_static_scene, sizeof(rrr3d_fs_static_scene)},
        "original-main-menu");
#ifdef RRR3D_PHYSICS
    const Shader raceShader = device->createShader(
        {rrr3d_vs_original_race, sizeof(rrr3d_vs_original_race)},
        {rrr3d_fs_original_race, sizeof(rrr3d_fs_original_race)},
        "original-race-r3d");
#endif
    const Mesh quad = device->createMesh(
        quadVertices.data(), quadVertices.size(), quadIndices.data(),
        quadIndices.size());
    const Texture background =
        createImageTexture(*device, model->backgroundImage);
    const Texture topPanel = createImageTexture(*device, model->topPanelImage);
    const Texture bottomPanel =
        createImageTexture(*device, model->bottomPanelImage);
    const Texture selection =
        createImageTexture(*device, model->selectionImage);
    const Texture cursor = createImageTexture(*device, model->cursorImage);
    const auto startupYardImage = menu::loadOriginalImage(
        *resources, "Data/GUI/yardLogo.png");
    const auto startupLabImage = menu::loadOriginalImage(
        *resources, "Data/GUI/laboratoria24.png");
    const auto startupLoadImage = menu::loadOriginalImage(
        *resources, "Data/GUI/startLogo.dds");
    const Texture startupYard =
        createImageTexture(*device, startupYardImage);
    const Texture startupLab =
        createImageTexture(*device, startupLabImage);
    const Texture startupLoad =
        createImageTexture(*device, startupLoadImage);
#ifdef RRR3D_AUDIO
    const auto musicDialogFrameImage = menu::loadOriginalImage(
        *resources, "Data/GUI/dlgFrame2.png");
    const Texture musicDialogFrame =
        createImageTexture(*device, musicDialogFrameImage);
#endif
    const auto finalBackImage = menu::loadOriginalImage(
        *resources, "Data/GUI/buttonBg2.png");
    const auto finalBackSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/buttonBgSel2.png");
    const std::array<menu::Image, 9> finalSlideImages{
        menu::loadOriginalImage(
            *resources, "Data/GUI/Slides/slide1.dds"),
        menu::loadOriginalImage(
            *resources, "Data/GUI/Slides/slide2.dds"),
        menu::loadOriginalImage(
            *resources, "Data/GUI/Slides/slide3.dds"),
        menu::loadOriginalImage(
            *resources, "Data/GUI/Slides/slide4.dds"),
        menu::loadOriginalImage(
            *resources, "Data/GUI/Slides/slide5.dds"),
        menu::loadOriginalImage(
            *resources, "Data/GUI/Slides/slide6.dds"),
        menu::loadOriginalImage(
            *resources, "Data/GUI/Slides/slide7.dds"),
        menu::loadOriginalImage(
            *resources, "Data/GUI/Slides/slide8.dds"),
        menu::loadOriginalImage(
            *resources, "Data/GUI/Slides/slide9.dds")};
    const Texture finalBack =
        createImageTexture(*device, finalBackImage);
    const Texture finalBackSelected =
        createImageTexture(*device, finalBackSelectedImage);
    std::array<Texture, finalSlideImages.size()> finalSlides{};
    for (std::size_t index = 0U; index < finalSlides.size(); ++index)
        finalSlides[index] =
            createImageTexture(*device, finalSlideImages[index]);
#ifdef RRR3D_PHYSICS
    const auto acceptFrameImage = menu::loadOriginalImage(
        *resources, "Data/GUI/dlgFrame1.png");
    const auto acceptButtonImage = menu::loadOriginalImage(
        *resources, "Data/GUI/dlgButton1.png");
    const auto acceptButtonSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/dlgButtonSel1.png");
    const Texture acceptFrame =
        createImageTexture(*device, acceptFrameImage);
    const Texture acceptButton =
        createImageTexture(*device, acceptButtonImage);
    const Texture acceptButtonSelected =
        createImageTexture(*device, acceptButtonSelectedImage);
    const auto infoDialogFrameImage = menu::loadOriginalImage(
        *resources, "Data/GUI/dlgFrame4.png");
    const auto infoDialogButtonImage = menu::loadOriginalImage(
        *resources, "Data/GUI/dlgButton2.png");
    const auto infoDialogButtonSelectedImage =
        menu::loadOriginalImage(
            *resources, "Data/GUI/dlgButtonSel2.png");
    const Texture infoDialogFrame =
        createImageTexture(*device, infoDialogFrameImage);
    const Texture infoDialogButton =
        createImageTexture(*device, infoDialogButtonImage);
    const Texture infoDialogButtonSelected =
        createImageTexture(
            *device, infoDialogButtonSelectedImage);
    const auto profileArrowImage = menu::loadOriginalImage(
        *resources, "Data/GUI/arrow1.png");
    const auto profileArrowSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/arrowSel1.png");
    const Texture profileArrow =
        createImageTexture(*device, profileArrowImage);
    const Texture profileArrowSelected =
        createImageTexture(*device, profileArrowSelectedImage);
    const Texture profileArrowDisabled =
        createImageTextureWithAlpha(
            *device, profileArrowImage, 1U, 4U);
    const auto gamersSpaceImage = menu::loadOriginalImage(
        *resources, "Data/GUI/space1.dds");
    const auto gamersBottomPanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/bottomPanel4.png");
    const auto gamersPhotoLightImage = menu::loadOriginalImage(
        *resources, "Data/GUI/wndLight4.png");
    const auto gamersNextArrowImage = menu::loadOriginalImage(
        *resources, "Data/GUI/arrow2.png");
    const auto gamersNextArrowSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/arrowSel2.png");
    const Texture gamersSpace =
        createImageTexture(*device, gamersSpaceImage);
    const Texture gamersBottomPanel =
        createImageTexture(*device, gamersBottomPanelImage);
    const Texture gamersPhotoLight =
        createImageTexture(*device, gamersPhotoLightImage);
    const Texture gamersNextArrow =
        createImageTexture(*device, gamersNextArrowImage);
    const Texture gamersNextArrowSelected =
        createImageTexture(*device, gamersNextArrowSelectedImage);
    std::vector<menu::Image> gamersBossImages;
    std::vector<Texture> gamersBossTextures;
    gamersBossImages.reserve(originalGarage->gamers.size());
    gamersBossTextures.reserve(originalGarage->gamers.size());
    for (const auto& gamer : originalGarage->gamers)
    {
        gamersBossImages.push_back(menu::loadOriginalImage(
            *resources, gamer.bossPhotoPath));
        gamersBossTextures.push_back(
            createImageTexture(*device, gamersBossImages.back()));
    }
    const auto optionsBackgroundImage = menu::loadOriginalImage(
        *resources, "Data/GUI/optionsBg.png");
    const auto startOptionsBackgroundImage = menu::loadOriginalImage(
        *resources, "Data/GUI/startMenuBg.png");
    const auto loadingFrameImage = menu::loadOriginalImage(
        *resources, "Data/GUI/loadingFrame.dds");
    const auto optionsRowImage = menu::loadOriginalImage(
        *resources, "Data/GUI/labelBg1.png");
    const auto controlsRowImage = menu::loadOriginalImage(
        *resources, "Data/GUI/labelBg2.png");
    const auto optionsArrowImage = menu::loadOriginalImage(
        *resources, "Data/GUI/arrow3.png");
    const auto optionsArrowSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/arrowSel3.png");
    const auto optionsBarBackgroundImage = menu::loadOriginalImage(
        *resources, "Data/GUI/optBarBg.png");
    const auto optionsBarImage = menu::loadOriginalImage(
        *resources, "Data/GUI/optBar.png");
    const auto optionsButtonImage = menu::loadOriginalImage(
        *resources, "Data/GUI/buttonBg4.png");
    const auto optionsButtonSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/buttonBgSel4.png");
    const auto startOptionsButtonImage = menu::loadOriginalImage(
        *resources, "Data/GUI/buttonBg5.png");
    const auto optionsKeyImage = menu::loadOriginalImage(
        *resources, "Data/GUI/keyBg.png");
    const auto optionsKeySelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/keyBgSel.png");
    const auto keyboardIconImage = menu::loadOriginalImage(
        *resources, "Data/GUI/ctKeyboard.png");
    const auto gamepadIconImage = menu::loadOriginalImage(
        *resources, "Data/GUI/ctGamepad.png");
    const Texture optionsBackground =
        createImageTexture(*device, optionsBackgroundImage);
    const Texture startOptionsBackground =
        createImageTexture(*device, startOptionsBackgroundImage);
    const Texture loadingFrame =
        createImageTexture(*device, loadingFrameImage);
    const Texture optionsRow =
        createImageTexture(*device, optionsRowImage);
    const Texture controlsRow =
        createImageTexture(*device, controlsRowImage);
    const Texture optionsArrow =
        createImageTexture(*device, optionsArrowImage);
    const Texture optionsArrowSelected =
        createImageTexture(*device, optionsArrowSelectedImage);
    const Texture optionsBarBackground =
        createImageTexture(*device, optionsBarBackgroundImage);
    const Texture optionsBar =
        createImageTexture(*device, optionsBarImage);
    const Texture optionsButton =
        createImageTexture(*device, optionsButtonImage);
    const Texture optionsButtonSelected =
        createImageTexture(*device, optionsButtonSelectedImage);
    const Texture startOptionsButton =
        createImageTexture(*device, startOptionsButtonImage);
    const Texture optionsKey =
        createImageTexture(*device, optionsKeyImage);
    const Texture optionsKeySelected =
        createImageTexture(*device, optionsKeySelectedImage);
    const Texture keyboardIcon =
        createImageTexture(*device, keyboardIconImage);
    const Texture gamepadIcon =
        createImageTexture(*device, gamepadIconImage);
    const std::array<std::uint8_t, 4> optionsMaskPixel{
        0U, 0U, 0U, 204U};
    const Texture optionsMask = device->createTextureRgba8(
        1U, 1U, optionsMaskPixel.data(), optionsMaskPixel.size());
    const auto raceTopPanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/topPanel.png");
    const auto raceBottomPanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/bottomPanel.png");
    const auto raceMenuButtonImage = menu::loadOriginalImage(
        *resources, "Data/GUI/buttonBg1.png");
    const auto raceMenuButtonSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/buttonBgSel1.png");
    const auto raceMoneyImage = menu::loadOriginalImage(
        *resources, "Data/GUI/moneyBg.png");
    const auto raceStatsImage = menu::loadOriginalImage(
        *resources, "Data/GUI/statFrame.png");
    const auto raceImageFrameImage = menu::loadOriginalImage(
        *resources, "Data/GUI/imageFrame1.png");
    const auto raceChargeBarImage = menu::loadOriginalImage(
        *resources, "Data/GUI/chargeBar1.png");
    const auto raceStatBarImage = menu::loadOriginalImage(
        *resources, "Data/GUI/statBar.png");
#ifdef RRR3D_NETWORK
    const auto networkPlayerFrameImage = menu::loadOriginalImage(
        *resources, "Data/GUI/netPlayerFrame.png");
    const auto networkPlayerKickImage = menu::loadOriginalImage(
        *resources, "Data/GUI/netPlayerKick.png");
    const auto networkPlayerKickSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/netPlayerKickSel.png");
    const auto networkPlayerReadyImage = menu::loadOriginalImage(
        *resources, "Data/GUI/netPlayerReadyState.png");
    const auto networkPlayerReadySelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/netPlayerReadyStateSel.png");
#endif
    const std::array<menu::Image, 7> raceMenuIconImages{
        menu::loadOriginalImage(*resources, "Data/GUI/icoStart.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/icoWorkshop.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/icoGarage.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/icoSpace.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/icoAchivment.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/icoOptions.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/icoExit.png")};
    const std::array<menu::Image, 4> raceWeatherImages{
        menu::loadOriginalImage(*resources, "Data/GUI/fair.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/night.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/cloudy.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/rainy.png")};
    const Texture raceTopPanel =
        createImageTexture(*device, raceTopPanelImage);
    const Texture raceBottomPanel =
        createImageTexture(*device, raceBottomPanelImage);
    const Texture raceMenuButton =
        createImageTexture(*device, raceMenuButtonImage);
    const Texture raceMenuButtonSelected =
        createImageTexture(*device, raceMenuButtonSelectedImage);
    const Texture raceMoney =
        createImageTexture(*device, raceMoneyImage);
    const Texture raceStats =
        createImageTexture(*device, raceStatsImage);
    const Texture raceImageFrame =
        createImageTexture(*device, raceImageFrameImage);
    const Texture raceChargeBar =
        createImageTexture(*device, raceChargeBarImage);
    const Texture raceStatBar =
        createImageTexture(*device, raceStatBarImage);
#ifdef RRR3D_NETWORK
    const Texture networkPlayerFrame =
        createImageTexture(*device, networkPlayerFrameImage);
    const Texture networkPlayerKick =
        createImageTexture(*device, networkPlayerKickImage);
    const Texture networkPlayerKickSelected =
        createImageTexture(*device, networkPlayerKickSelectedImage);
    const Texture networkPlayerReady =
        createImageTexture(*device, networkPlayerReadyImage);
    const Texture networkPlayerReadySelected =
        createImageTexture(*device, networkPlayerReadySelectedImage);
#endif
    std::array<Texture, 7> raceMenuIcons{};
    for (std::size_t index = 0U; index < raceMenuIcons.size(); ++index)
        raceMenuIcons[index] =
            createImageTexture(*device, raceMenuIconImages[index]);
    std::array<Texture, 4> raceWeatherIcons{};
    for (std::size_t index = 0U; index < raceWeatherIcons.size(); ++index)
        raceWeatherIcons[index] =
            createImageTexture(*device, raceWeatherImages[index]);
    const auto garageTopPanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/topPanel2.png");
    const auto garageBottomPanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/bottomPanel2.png");
    const auto garageSidePanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/rightPanel2.png");
    const auto garageMoneyImage = menu::loadOriginalImage(
        *resources, "Data/GUI/moneyBg.png");
    const auto garageStatsImage = menu::loadOriginalImage(
        *resources, "Data/GUI/statFrame2.png");
    const auto garageStatBarImage = menu::loadOriginalImage(
        *resources, "Data/GUI/statBar2.png");
    const auto garageCarBoxImage = menu::loadOriginalImage(
        *resources, "Data/GUI/carBox.png");
    const auto garageCarBoxSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/carBoxSel.png");
    const auto garageLockImage = menu::loadOriginalImage(
        *resources, "Data/GUI/lock.png");
    const auto garageColorBoxImage = menu::loadOriginalImage(
        *resources, "Data/GUI/colorBox.png");
    const auto garageColorBoxBackgroundImage = menu::loadOriginalImage(
        *resources, "Data/GUI/colorBoxBg.png");
    const auto garageColorBoxSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/colorBoxBgSel.png");
    const auto garageArrowImage = menu::loadOriginalImage(
        *resources, "Data/GUI/arrow1.png");
    const auto garageArrowSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/arrowSel1.png");
    const auto garageBackImage = menu::loadOriginalImage(
        *resources, "Data/GUI/buttonBg2.png");
    const auto garageBackSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/buttonBgSel2.png");
    const std::string garageLanguage =
        activeLanguage == "russian" ? "russian" : "english";
    const auto garageBuyImage = menu::loadOriginalImage(
        *resources,
        "Data/GUI/buyButton_" + garageLanguage + ".png");
    const auto garageBuySelectedImage = menu::loadOriginalImage(
        *resources,
        "Data/GUI/buyButtonSel_" + garageLanguage + ".png");
    const Texture garageTopPanel =
        createImageTexture(*device, garageTopPanelImage);
    const Texture garageBottomPanel =
        createImageTexture(*device, garageBottomPanelImage);
    const Texture garageSidePanel =
        createImageTexture(*device, garageSidePanelImage);
    const Texture garageMoney =
        createImageTexture(*device, garageMoneyImage);
    const Texture garageStats =
        createImageTexture(*device, garageStatsImage);
    const Texture garageStatBar =
        createImageTexture(*device, garageStatBarImage);
    const Texture garageCarBox =
        createImageTexture(*device, garageCarBoxImage);
    const Texture garageCarBoxSelected =
        createImageTexture(*device, garageCarBoxSelectedImage);
    const Texture garageLock =
        createImageTexture(*device, garageLockImage);
    const Texture garageColorBox =
        createImageTexture(*device, garageColorBoxImage);
    const Texture garageColorBoxBackground =
        createImageTexture(*device, garageColorBoxBackgroundImage);
    const Texture garageColorBoxSelected =
        createImageTexture(*device, garageColorBoxSelectedImage);
    const Texture garageArrow =
        createImageTexture(*device, garageArrowImage);
    const Texture garageArrowSelected =
        createImageTexture(*device, garageArrowSelectedImage);
    const Texture garageBack =
        createImageTexture(*device, garageBackImage);
    const Texture garageBackSelected =
        createImageTexture(*device, garageBackSelectedImage);
    const Texture garageBuy =
        createImageTexture(*device, garageBuyImage);
    const Texture garageBuySelected =
        createImageTexture(*device, garageBuySelectedImage);
    const auto workshopTopPanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/topPanel3.png");
    const auto workshopBottomPanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/bottomPanel3.png");
    const auto workshopLeftPanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/leftPanel3.png");
    const auto workshopSlotImage = menu::loadOriginalImage(
        *resources, "Data/GUI/slot2.png");
    const auto workshopSlotFrameImage = menu::loadOriginalImage(
        *resources, "Data/GUI/slot2Frame.png");
    const auto workshopChargeBoxImage = menu::loadOriginalImage(
        *resources, "Data/GUI/chargeBox.png");
    const auto workshopChargeBarImage = menu::loadOriginalImage(
        *resources, "Data/GUI/chargeBar.png");
    const auto workshopChargeButtonImage = menu::loadOriginalImage(
        *resources, "Data/GUI/chargeButton.png");
    const auto workshopChargeButtonSelectedImage =
        menu::loadOriginalImage(
            *resources, "Data/GUI/chargeButtonSel.png");
    const auto workshopStatBarPlusImage = menu::loadOriginalImage(
        *resources, "Data/GUI/statBar2Plus.png");
    const std::array<menu::Image, 3> workshopUpgradeImages{
        menu::loadOriginalImage(*resources, "Data/GUI/upLevel1.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/upLevel2.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/upLevel3.png")};
    const std::array<menu::Image, 4> workshopSlotIconImages{
        menu::loadOriginalImage(*resources, "Data/GUI/hyperSlot.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/mineSlot.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/wpnSlot.png"),
        menu::loadOriginalImage(*resources, "Data/GUI/wpnSlotSel.png")};
    const auto workshopInfoFrameImage = menu::loadOriginalImage(
        *resources, "Data/GUI/dlgFrame3.png");
    const Texture workshopTopPanel =
        createImageTexture(*device, workshopTopPanelImage);
    const Texture workshopBottomPanel =
        createImageTexture(*device, workshopBottomPanelImage);
    const Texture workshopLeftPanel =
        createImageTexture(*device, workshopLeftPanelImage);
    const Texture workshopSlot =
        createImageTexture(*device, workshopSlotImage);
    const Texture workshopSlotFrame =
        createImageTexture(*device, workshopSlotFrameImage);
    const Texture workshopChargeBox =
        createImageTexture(*device, workshopChargeBoxImage);
    const Texture workshopChargeBar =
        createImageTexture(*device, workshopChargeBarImage);
    const Texture workshopChargeButton =
        createImageTexture(*device, workshopChargeButtonImage);
    const Texture workshopChargeButtonSelected =
        createImageTexture(*device, workshopChargeButtonSelectedImage);
    const Texture workshopStatBarPlus =
        createImageTexture(*device, workshopStatBarPlusImage);
    std::array<Texture, 3> workshopUpgradeTextures{};
    for (std::size_t index = 0U;
         index < workshopUpgradeTextures.size(); ++index)
    {
        workshopUpgradeTextures[index] =
            createImageTexture(*device, workshopUpgradeImages[index]);
    }
    std::array<Texture, 4> workshopSlotIconTextures{};
    for (std::size_t index = 0U;
         index < workshopSlotIconTextures.size(); ++index)
    {
        workshopSlotIconTextures[index] =
            createImageTexture(*device, workshopSlotIconImages[index]);
    }
    const Texture workshopInfoFrame =
        createImageTexture(*device, workshopInfoFrameImage);
    const auto angarBottomPanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/bottomPanel6.png");
    const auto angarPlanetInfoImage = menu::loadOriginalImage(
        *resources, "Data/GUI/planetInfo.png");
    const auto angarCloseImage = menu::loadOriginalImage(
        *resources, "Data/GUI/buttonBg6.png");
    const auto angarCloseSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/buttonBgSel6.png");
    const auto angarDoorSlotImage = menu::loadOriginalImage(
        *resources, "Data/GUI/doorSlot.png");
    const auto angarDoorSlotSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/doorSlotSel.png");
    const auto angarDoorDownImage = menu::loadOriginalImage(
        *resources, "Data/GUI/doorDown.png");
    const auto angarDoorUpImage = menu::loadOriginalImage(
        *resources, "Data/GUI/doorUp.png");
    const Texture angarBottomPanel =
        createImageTexture(*device, angarBottomPanelImage);
    const Texture angarPlanetInfo =
        createImageTexture(*device, angarPlanetInfoImage);
    const Texture angarClose =
        createImageTexture(*device, angarCloseImage);
    const Texture angarCloseSelected =
        createImageTexture(*device, angarCloseSelectedImage);
    const Texture angarDoorSlot =
        createImageTexture(*device, angarDoorSlotImage);
    const Texture angarDoorSlotSelected =
        createImageTexture(*device, angarDoorSlotSelectedImage);
    const Texture angarDoorDown =
        createImageTexture(*device, angarDoorDownImage);
    const Texture angarDoorUp =
        createImageTexture(*device, angarDoorUpImage);
    std::vector<menu::Image> angarBossImages;
    std::vector<Texture> angarBossTextures;
    angarBossImages.reserve(originalGarage->planets.size());
    angarBossTextures.reserve(originalGarage->planets.size());
    for (const auto& planet : originalGarage->planets)
    {
        angarBossImages.push_back(
            menu::loadOriginalImage(
                *resources, planet.bossPhotoPath));
        angarBossTextures.push_back(
            createImageTexture(*device, angarBossImages.back()));
    }
    const auto achievementBackgroundImage = menu::loadOriginalImage(
        *resources, "Data/GUI/achievmentBg.dds");
    const auto achievementBottomPanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/achievmentBottomPanel.png");
    const auto achievementPanelImage = menu::loadOriginalImage(
        *resources, "Data/GUI/achievmentPanel.png");
    const auto achievementCloseImage = menu::loadOriginalImage(
        *resources, "Data/GUI/closeBut.png");
    const auto achievementOkImage = menu::loadOriginalImage(
        *resources, "Data/GUI/okBut.png");
    const auto achievementOkSelectedImage = menu::loadOriginalImage(
        *resources, "Data/GUI/okButSel.png");
    const Texture achievementBackground =
        createImageTexture(*device, achievementBackgroundImage);
    const Texture achievementBottomPanel =
        createImageTexture(*device, achievementBottomPanelImage);
    const Texture achievementPanel =
        createImageTexture(*device, achievementPanelImage);
    const Texture achievementClose =
        createImageTexture(*device, achievementCloseImage);
    const Texture achievementOk =
        createImageTexture(*device, achievementOkImage);
    const Texture achievementOkSelected =
        createImageTexture(*device, achievementOkSelectedImage);
    const auto finishLeftFrameImage = menu::loadOriginalImage(
        *resources, "Data/GUI/playerLeftFrame.png");
    const auto finishRightFrameImage = menu::loadOriginalImage(
        *resources, "Data/GUI/playerRightFrame.png");
    const auto finishLineFrameImage = menu::loadOriginalImage(
        *resources, "Data/GUI/playerLineFrame.png");
    const Texture finishLeftFrame =
        createImageTexture(*device, finishLeftFrameImage);
    const Texture finishRightFrame =
        createImageTexture(*device, finishRightFrameImage);
    const Texture finishLineFrame =
        createImageTexture(*device, finishLineFrameImage);
    const std::array<menu::Image, 3> finishCupImages{
        menu::loadOriginalImage(*resources, "Data/GUI/cup1.dds"),
        menu::loadOriginalImage(*resources, "Data/GUI/cup2.dds"),
        menu::loadOriginalImage(*resources, "Data/GUI/cup3.dds")};
    std::array<Texture, finishCupImages.size()> finishCups{};
    for (std::size_t index = 0U; index < finishCups.size(); ++index)
        finishCups[index] =
            createImageTexture(*device, finishCupImages[index]);
    std::vector<menu::Image> achievementLockedImages;
    std::vector<menu::Image> achievementOpenedImages;
    std::vector<Texture> achievementLockedTextures;
    std::vector<Texture> achievementOpenedTextures;
    achievementLockedImages.reserve(originalAchievementVisuals.size());
    achievementOpenedImages.reserve(originalAchievementVisuals.size());
    achievementLockedTextures.reserve(originalAchievementVisuals.size());
    achievementOpenedTextures.reserve(originalAchievementVisuals.size());
    for (const auto& visual : originalAchievementVisuals)
    {
        achievementLockedImages.push_back(
            menu::loadOriginalImage(
                *resources, std::string(visual.lockedImage)));
        achievementOpenedImages.push_back(
            menu::loadOriginalImage(
                *resources, std::string(visual.openedImage)));
        achievementLockedTextures.push_back(
            createImageTexture(*device, achievementLockedImages.back()));
        achievementOpenedTextures.push_back(
            createImageTexture(*device, achievementOpenedImages.back()));
    }
    std::vector<menu::Image> garageCarImages;
    std::vector<Texture> garageCarTextures;
    garageCarImages.reserve(originalGarage->cars.size());
    garageCarTextures.reserve(originalGarage->cars.size());
    for (const auto& car : originalGarage->cars)
    {
        garageCarImages.push_back(menu::loadOriginalImage(
            *resources,
            "Data/GUI/Cars/" +
                std::string(recordName(car.record)) + ".png"));
        garageCarTextures.push_back(
            createImageTexture(*device, garageCarImages.back()));
    }
    constexpr std::array<std::array<std::uint8_t, 4>, 14>
        garageColorPixels{{
            {255U, 255U, 255U, 255U},
            {0U, 0U, 255U, 255U},
            {255U, 0U, 0U, 255U},
            {0U, 255U, 0U, 255U},
            {255U, 255U, 0U, 255U},
            {255U, 144U, 0U, 255U},
            {51U, 51U, 51U, 255U},
            {6U, 175U, 250U, 255U},
            {183U, 11U, 174U, 255U},
            {177U, 201U, 3U, 255U},
            {169U, 57U, 0U, 255U},
            {48U, 55U, 61U, 255U},
            {0U, 159U, 21U, 255U},
            {112U, 118U, 156U, 255U},
        }};
    std::array<Texture, garageColorPixels.size()>
        garageColorTextures{};
    for (std::size_t index = 0U;
         index < garageColorTextures.size(); ++index)
    {
        garageColorTextures[index] = device->createTextureRgba8(
            1U, 1U, garageColorPixels[index].data(),
            garageColorPixels[index].size());
    }
#endif

    struct MenuPageVisual
    {
        std::vector<std::string> labels;
        std::vector<TextVisual> normal;
        std::vector<TextVisual> selected;
        std::vector<TextVisual> disabled;
        std::vector<bool> enabled;
    };
    auto localized = [&](std::string_view key) {
        const auto found =
            model->localizedStrings.find(std::string(key));
        return found == model->localizedStrings.end()
                   ? std::string(key)
                   : found->second;
    };
    auto labels = [&](std::initializer_list<const char*> keys) {
        std::vector<std::string> output;
        output.reserve(keys.size());
        for (const char* key : keys)
            output.push_back(localized(key));
        return output;
    };
    MenuPageVisual mainPage;
    MenuPageVisual gameModePage;
    MenuPageVisual tournamentPage;
    MenuPageVisual difficultyPage;
    MenuPageVisual profilePage;
    MenuPageVisual networkPage;
#ifdef RRR3D_NETWORK
    MenuPageVisual networkServerTypePage;
    MenuPageVisual networkClientTypePage;
    MenuPageVisual networkBrowserPage;
    MenuPageVisual networkIpPage;
    MenuPageVisual networkAddressInfoPage;
    MenuPageVisual networkStatusPage;
    MenuPageVisual networkIpValuePage;
#endif
    MenuPageVisual optionsPage;
    MenuPageVisual creditsPage;
#ifdef RRR3D_PHYSICS
    MenuPageVisual raceMenuPage;
    MenuPageVisual gamersNamePage;
    MenuPageVisual gamersInfoPage;
    MenuPageVisual gamersBonusPage;
    MenuPageVisual raceMainHeadersPage;
    MenuPageVisual raceMainInfoPage;
    MenuPageVisual raceMainStatsPage;
    MenuPageVisual garagePage;
    MenuPageVisual garageInfoPage;
    MenuPageVisual garageStatsPage;
    MenuPageVisual workshopPage;
    MenuPageVisual workshopControlsPage;
    MenuPageVisual workshopStatsPage;
    MenuPageVisual workshopHintPage;
    WorkshopWeaponDialogVisual workshopWeaponDialog;
    InfoDialogVisual infoDialog;
    AcceptDialogVisual acceptDialog;
    MenuPageVisual planetsPage;
    MenuPageVisual angarInfoPage;
    MenuPageVisual achievementsPage;
    MenuPageVisual achievementPricePage;
    MenuPageVisual gameOptionsPage;
    MenuPageVisual graphicsOptionsPage;
    MenuPageVisual soundOptionsPage;
    MenuPageVisual controlsOptionsPage;
    MenuPageVisual gameOptionNamesPage;
    MenuPageVisual graphicsOptionNamesPage;
    MenuPageVisual soundOptionNamesPage;
    MenuPageVisual controlsKeyboardValuesPage;
    MenuPageVisual controlsGamepadValuesPage;
    MenuPageVisual optionsHeaderPage;
    MenuPageVisual optionsStatePage;
    MenuPageVisual optionsActionPage;
    MenuPageVisual startOptionsLabelPage;
    MenuPageVisual startOptionsValuePage;
    MenuPageVisual startOptionsActionPage;
    std::vector<TextVisual> startOptionsInfoLines;
#ifdef RRR3D_NETWORK
    std::vector<NetworkRacePlayerVisual> networkRacePlayerVisuals;
#endif
#endif
    std::string resolvedFont;
    auto createPage = [&](std::vector<std::string> pageLabels) {
        MenuPageVisual page;
        page.labels = std::move(pageLabels);
        page.enabled.assign(page.labels.size(), true);
        for (const auto& item : page.labels)
        {
            page.normal.push_back(createText(
                *device, item, menu::headerFontHeight, false,
                menu::normalTextColor, resolvedFont));
            page.selected.push_back(createText(
                *device, item, menu::headerFontHeight, false,
                menu::selectedTextColor, resolvedFont));
            auto disabledColor = menu::normalTextColor;
            disabledColor.alpha =
                static_cast<std::uint8_t>(disabledColor.alpha / 4U);
            page.disabled.push_back(createText(
                *device, item, menu::headerFontHeight, false,
                disabledColor, resolvedFont));
        }
        return page;
    };
    auto createStyledPage =
        [&](std::vector<std::string> pageLabels, float pointSize,
            menu::Rgba8 normalColor, menu::Rgba8 selectedColor) {
            MenuPageVisual page;
            page.labels = std::move(pageLabels);
            page.enabled.assign(page.labels.size(), true);
            for (const auto& item : page.labels)
            {
                page.normal.push_back(createText(
                    *device, item, pointSize, false, normalColor,
                    resolvedFont));
                page.selected.push_back(createText(
                    *device, item, pointSize, false, selectedColor,
                    resolvedFont));
                auto disabledColor = normalColor;
                disabledColor.alpha =
                    static_cast<std::uint8_t>(
                        disabledColor.alpha / 4U);
                page.disabled.push_back(createText(
                    *device, item, pointSize, false, disabledColor,
                    resolvedFont));
            }
            return page;
        };
    auto destroyPage = [&](const MenuPageVisual& page) {
        for (const auto& item : page.disabled)
            device->destroy(item.texture);
        for (const auto& item : page.selected)
            device->destroy(item.texture);
        for (const auto& item : page.normal)
            device->destroy(item.texture);
    };
#ifdef RRR3D_PHYSICS
    constexpr menu::Rgba8 optionsTextColor{175, 175, 175, 255};
    constexpr menu::Rgba8 optionsStateSelectedColor{235, 115, 62, 255};
    constexpr menu::Rgba8 raceTextColor{214, 214, 214, 255};
    constexpr menu::Rgba8 raceInfoColor{214, 184, 164, 255};
    auto optionsDraftConfig = profileState.config;
    auto optionsDraftDifficulty = profileState.player.difficulty;
    auto sourceDifficultyIndex = [](std::string_view difficulty) {
        if (difficulty == "gdEasy")
            return 0;
        if (difficulty == "gdHard")
            return 2;
        return 1;
    };
    auto sourceDifficultyName = [](std::int32_t difficulty) {
        return std::array<std::string_view, 3>{
            "gdEasy", "gdNormal", "gdHard"}
            [static_cast<std::size_t>(
                std::clamp(difficulty, 0, 2))];
    };
    std::vector<std::pair<std::uint32_t, std::uint32_t>>
        originalDisplayModes;
    int displayModeCount = 0;
    SDL_DisplayMode** displayModes = SDL_GetFullscreenDisplayModes(
        SDL_GetDisplayForWindow(window), &displayModeCount);
    for (int index = 0; displayModes != nullptr &&
                        index < displayModeCount;
         ++index)
    {
        if (displayModes[index] == nullptr ||
            displayModes[index]->w <= 0 ||
            displayModes[index]->h <= 0)
        {
            continue;
        }
        const auto mode = std::pair{
            static_cast<std::uint32_t>(displayModes[index]->w),
            static_cast<std::uint32_t>(displayModes[index]->h)};
        if (std::find(
                originalDisplayModes.begin(), originalDisplayModes.end(),
                mode) == originalDisplayModes.end())
        {
            originalDisplayModes.push_back(mode);
        }
    }
    SDL_free(displayModes);
    const auto configuredDisplayMode = std::pair{
        optionsDraftConfig.resolutionWidth,
        optionsDraftConfig.resolutionHeight};
    if (std::find(
            originalDisplayModes.begin(), originalDisplayModes.end(),
            configuredDisplayMode) == originalDisplayModes.end())
    {
        originalDisplayModes.push_back(configuredDisplayMode);
    }
    if (originalDisplayModes.empty())
        originalDisplayModes.push_back({initialWidth, initialHeight});
    std::sort(
        originalDisplayModes.begin(), originalDisplayModes.end(),
        [](const auto& first, const auto& second) {
            return first.first * first.second <
                   second.first * second.second;
        });
    // Literal order from Data/game.xml, consumed by
    // StartOptionsMenu::StartOptionsMenu.
    constexpr std::array<std::string_view, 6> sourceLanguages{
        "english", "russian", "portuguese", "french", "spain",
        "german"};
    constexpr std::array<std::string_view, 2> sourceCommentators{
        "russian", "english"};
    auto sourceListIndex = [](const auto& values,
                              std::string_view selected) {
        const auto found =
            std::find(values.begin(), values.end(), selected);
        return found == values.end()
                   ? std::size_t{0}
                   : static_cast<std::size_t>(
                         found - values.begin());
    };
    std::size_t startOptionsCameraIndex = 2U;
    std::size_t startOptionsResolutionIndex =
        static_cast<std::size_t>(std::distance(
            originalDisplayModes.begin(),
            std::find(
                originalDisplayModes.begin(),
                originalDisplayModes.end(), configuredDisplayMode)));
    if (startOptionsResolutionIndex >= originalDisplayModes.size())
        startOptionsResolutionIndex = 0U;
    std::size_t startOptionsLanguageIndex = sourceListIndex(
        sourceLanguages, optionsDraftConfig.language);
    std::size_t startOptionsCommentatorIndex = sourceListIndex(
        sourceCommentators, optionsDraftConfig.commentatorStyle);
    std::size_t startOptionsFocus = 0U;
    bool startOptionsApplyEnabled = false;
    auto startOptionsValues = [&]() {
        const auto& resolution =
            originalDisplayModes[startOptionsResolutionIndex];
        return std::vector<std::string>{
            startOptionsCameraIndex == 0U
                ? localized("svCameraSecView")
                : startOptionsCameraIndex == 1U
                      ? localized("svCameraOrtho")
                      : localized("svSelectItem"),
            std::to_string(resolution.first) + " x " +
                std::to_string(resolution.second),
            std::string(sourceLanguages[startOptionsLanguageIndex]),
            std::string(
                sourceCommentators[startOptionsCommentatorIndex])};
    };
    auto onOff = [&](bool value) {
        return localized(value ? "svOn" : "svOff");
    };
    auto qualityName = [&](std::uint32_t value) {
        return localized(
            std::array<std::string_view, 3>{
                "svLow", "svMiddle", "svHigh"}
                [std::min<std::uint32_t>(value, 2U)]);
    };
    auto volumeName = [](float value) {
        return std::to_string(
                   static_cast<int>(std::lround(
                       std::clamp(value, 0.0F, 2.0F) * 50.0F))) +
               "%";
    };
    auto distanceName = [](float value) {
        std::ostringstream stream;
        stream.setf(std::ios::fixed);
        stream.precision(2);
        stream << value << " x";
        return stream.str();
    };
    static constexpr std::array<std::string_view, 18>
        originalControlActions{
            "gaAccel", "gaBreak", "gaWheelLeft", "gaWheelRight",
            "gaShot", "gaShot1", "gaShot2", "gaShot3", "gaShot4",
            "gaShotAll", "gaHyper", "gaMine", "gaWeaponDown",
            "gaWeaponUp", "gaViewSwitch", "gaAction", "gaEscape",
            "gaResetCar"};
    static constexpr std::size_t controlsVisibleRows = 6U;
    bool controlsUseGamepad = false;
    auto gameOptionsLabels = [&]() {
        return std::vector<std::string>{
            localized(
                    optionsDraftConfig.preferredCamera ==
                            r3d::game::originalrace::
                                PreferredCamera::ThirdPerson
                        ? "svCameraSecView"
                        : "svCameraOrtho"),
            distanceName(optionsDraftConfig.cameraDistance),
            onOff(optionsDraftConfig.enableHud),
            localized(optionsDraftDifficulty),
            onOff(optionsDraftConfig.springBorders),
            std::to_string(
                std::min(optionsDraftConfig.upgradeMaxLevel, 2U) + 1U),
            std::to_string(std::clamp(
                optionsDraftConfig.weaponMaxLevel, 1U, 4U)),
            std::to_string(std::clamp(
                optionsDraftConfig.maxPlayers, 2U, 6U)),
            std::to_string(optionsDraftConfig.maxComputers),
            std::to_string(optionsDraftConfig.lapsCount),
            onOff(optionsDraftConfig.enableMineBug),
            onOff(optionsDraftConfig.disableVideo),
            localized("svBack"), localized("svApply")};
    };
    auto graphicsOptionsLabels = [&]() {
        // Preserve OptionsMenu.cpp's shipped ordering verbatim: its
        // Filtering row uses aaLevel and Multisampling uses msLevel.
        static constexpr std::array<std::string_view, 4>
            filteringNames{
                "none", "aa 2x", "aa 4x", "aa 8x"};
        static constexpr std::array<std::string_view, 3> msaaNames{
            "linear", "af 2x", "af 4x"};
        const std::string resolution =
            std::to_string(optionsDraftConfig.resolutionWidth) + " x " +
            std::to_string(optionsDraftConfig.resolutionHeight);
        return std::vector<std::string>{
            resolution,
            std::string(filteringNames[
                    std::min<std::uint32_t>(
                        optionsDraftConfig.quality.filtering,
                        filteringNames.size() - 1U)]),
            std::string(msaaNames[
                    std::min<std::uint32_t>(
                        optionsDraftConfig.quality.msaa,
                        msaaNames.size() - 1U)]),
            qualityName(optionsDraftConfig.quality.shadow),
            qualityName(optionsDraftConfig.quality.environment),
            qualityName(optionsDraftConfig.quality.light),
            qualityName(optionsDraftConfig.quality.postEffect),
            onOff(!optionsDraftConfig.fullScreen),
            localized("svBack"), localized("svApply")};
    };
    auto soundOptionsLabels = [&]() {
        return std::vector<std::string>{
            localized(optionsDraftConfig.language == "russian"
                              ? "svRussian"
                              : "svEnglish"),
            localized(
                    optionsDraftConfig.commentatorStyle == "russian"
                        ? "svRussian"
                        : "svEnglish"),
            volumeName(optionsDraftConfig.musicVolume),
            volumeName(optionsDraftConfig.effectsVolume),
            volumeName(optionsDraftConfig.voiceVolume),
            localized("svBack"), localized("svApply")};
    };
    auto controlsOptionsLabels = [&]() {
        std::vector<std::string> output;
        output.reserve(originalControlActions.size() + 2U);
        for (const auto action : originalControlActions)
            output.push_back(localized(action));
        output.push_back(localized("svBack"));
        output.push_back(localized("svApply"));
        return output;
    };
    auto controlValues = [&](bool gamepad) {
        std::vector<std::string> output;
        output.reserve(originalControlActions.size());
        const auto& bindings =
            gamepad ? optionsDraftConfig.gamepadControls
                    : optionsDraftConfig.keyboardControls;
        for (const auto action : originalControlActions)
        {
            const auto found = bindings.find(std::string(action));
            output.push_back(
                found == bindings.end() ? localized("svNull")
                                        : found->second);
        }
        return output;
    };
    auto formatOriginalRaceInfo =
        [](std::string pattern, const std::string& textValue,
           std::uint32_t firstValue, std::uint32_t secondValue) {
            auto replaceFirst = [&](std::string_view token,
                                    const std::string& value) {
                const auto position = pattern.find(token);
                if (position != std::string::npos)
                    pattern.replace(position, token.size(), value);
            };
            replaceFirst("%s", textValue);
            replaceFirst("%d", std::to_string(firstValue));
            replaceFirst("%d", std::to_string(secondValue));
            return pattern;
        };
    auto raceMainInfoLabels = [&]() {
        const auto trackIndex = std::min(
            selectedTrack, originalRace->trackCatalog.size() - 1U);
        const auto& selected =
            originalRace->trackCatalog[trackIndex];
        std::uint32_t trackNumber = 0U;
        std::uint32_t trackCount = 0U;
        for (std::size_t index = 0U;
             index < originalRace->trackCatalog.size(); ++index)
        {
            const auto& candidate =
                originalRace->trackCatalog[index];
            if (candidate.planetIndex != selected.planetIndex ||
                candidate.racePass != selected.racePass)
            {
                continue;
            }
            ++trackCount;
            if (index <= trackIndex)
                ++trackNumber;
        }
        const std::string planetName =
            selected.planetIndex < originalGarage->planets.size()
                ? localized(
                      originalGarage
                          ->planets[selected.planetIndex]
                          .name)
                : selected.worldType;
        const auto pass = std::max(selected.racePass, 1U);
        const auto required =
            pass <= originalRace->requiredPoints.size()
                ? originalRace->requiredPoints[pass - 1U]
                : 0U;
        const std::string passInfo = formatOriginalRaceInfo(
            localized("svPassInfo"), planetName,
            std::max(trackNumber, 1U),
            std::max(trackCount, 1U));
        const std::string tournamentInfo =
            formatOriginalRaceInfo(
                localized("svTournamentInfo"),
                pass <= 1U ? "B" : "A", required,
                profileState.player.points);
        return std::vector<std::string>{
            passInfo, tournamentInfo,
            "$" + std::to_string(profileState.player.money)};
    };
    auto raceMainStatsLabels = [&]() {
        const auto* car = originalGarage->findCar(
            profileState.player.currentCar);
        const auto stats =
            car != nullptr
                ? r3d::game::originalrace::originalGarageStats(
                      *originalGarage, *car, profileState.player)
                : r3d::game::originalrace::OriginalGarageStats{};
        auto rounded = [](float value) {
            return std::to_string(
                static_cast<long long>(std::llround(value)));
        };
        return std::vector<std::string>{
            rounded(stats.damage) + "/" +
                rounded(stats.maximumDamage),
            rounded(stats.armor) + "/" +
                rounded(stats.maximumArmor),
            rounded(stats.speedProgress * 300.0F) + "/300"};
    };
#endif
    try
    {
        mainPage = createPage(model->items);
        gameModePage = createPage(labels(
            {"svChampionship", "svSkirmish", "svBack"}));
        tournamentPage = createPage(labels(
            {"svContinue", "svNewGame", "svLoad", "svBack"}));
        difficultyPage = createPage(
            labels({"gdEasy", "gdNormal", "gdHard", "svBack"}));
        std::vector<std::string> profileLabels;
#ifdef RRR3D_PHYSICS
        profileLabels = profileState.profiles;
#endif
        if (profileLabels.empty())
            profileLabels.push_back("profile1");
        profileLabels.push_back(localized("svBack"));
        profilePage = createPage(std::move(profileLabels));
        networkPage = createPage(
            labels({"svNetCreate", "svConnect", "svBack"}));
#ifdef RRR3D_NETWORK
        networkServerTypePage = createPage(
            labels({"svLocalServer", "svBack"}));
        networkClientTypePage = createPage(
            labels({"svConnectLan", "svConnectIP", "svBack"}));
        networkBrowserPage = createPage(labels({"svBack"}));
        networkIpPage = createPage(labels({"svConnect", "svBack"}));
        networkAddressInfoPage = createStyledPage(
            {" "}, menu::smallFontHeight,
            menu::Rgba8{214, 214, 214, 255},
            menu::selectedTextColor);
        networkStatusPage = createStyledPage(
            {" "}, menu::smallFontHeight,
            menu::Rgba8{214, 214, 214, 255},
            menu::selectedTextColor);
        networkIpValuePage = createStyledPage(
            {"_"}, menu::headerFontHeight,
            menu::normalTextColor, menu::selectedTextColor);
#endif
        optionsPage = createPage(labels(
            {"svGame", "svGraphic", "svSound", "svControls",
             "svBack"}));
        creditsPage = createPage(labels({"svBack"}));
#ifdef RRR3D_PHYSICS
        raceMenuPage = createPage(
            {"Start race", localized("svWorkshop"),
             localized("svGarage"), "Planets",
             localized("svRewards"), localized("svOptions"),
             localized("svExit")});
        gamersNamePage = createStyledPage(
            {" "}, menu::headerFontHeight,
            menu::Rgba8{255, 255, 255, 255},
            menu::selectedTextColor);
        gamersInfoPage = createStyledPage(
            {" "}, menu::smallFontHeight,
            menu::Rgba8{214, 214, 214, 255},
            menu::selectedTextColor);
        gamersBonusPage = createStyledPage(
            {" "}, 30.0F,
            menu::Rgba8{214, 214, 214, 255},
            menu::selectedTextColor);
        raceMainHeadersPage = createStyledPage(
            labels(
                {"svPlayer", "svPassing", "svTournament",
                 "svWeapons", "svBossName"}),
            menu::smallFontHeight, raceTextColor,
            menu::selectedTextColor);
        raceMainInfoPage = createStyledPage(
            raceMainInfoLabels(), menu::smallFontHeight,
            raceInfoColor, menu::selectedTextColor);
        raceMainStatsPage = createStyledPage(
            raceMainStatsLabels(), menu::smallFontHeight,
            raceTextColor, menu::selectedTextColor);
        garagePage = createPage(
            {localized("svGarage"), localized("svMoney"), "-",
             localized("svBack"), localized("svBuy")});
        garageInfoPage = createStyledPage(
            {" "}, menu::smallFontHeight, raceTextColor,
            menu::selectedTextColor);
        garageStatsPage = createStyledPage(
            {"0/0", "0/0", "0/300"},
            menu::smallFontHeight, raceTextColor,
            menu::selectedTextColor);
        workshopPage = createStyledPage(
            {localized("svWorkshop")},
            menu::headerFontHeight, raceTextColor,
            menu::selectedTextColor);
        workshopControlsPage = createStyledPage(
            {localized("svBack"),
             std::to_string(profileState.player.money)},
            menu::smallFontHeight, raceTextColor,
            menu::selectedTextColor);
        workshopStatsPage = createStyledPage(
            {"0/0", "0/0", "0/300"},
            menu::smallFontHeight, raceTextColor,
            menu::selectedTextColor);
        workshopHintPage = createStyledPage(
            {" "}, menu::smallFontHeight, raceTextColor,
            menu::selectedTextColor);
        planetsPage = createPage(
            {"Planets", localized("svBack")});
        angarInfoPage = createStyledPage(
            {" ", " ", " "}, menu::smallFontHeight,
            menu::Rgba8{118, 206, 242, 255},
            menu::selectedTextColor);
        achievementsPage = createPage(
            {"viper", "buggi", "airblade", "reflector", "droid",
             "tankchetti", "phaser", "mustang", "armor4",
             localized("svBack")});
        achievementPricePage = createStyledPage(
            {"0", "0", "0", "0", "0", "0", "0", "0", "0"},
            menu::smallFontHeight,
            menu::Rgba8{195, 194, 192, 255},
            menu::Rgba8{195, 194, 192, 255});
        gameOptionsPage = createStyledPage(
            gameOptionsLabels(), menu::smallFontHeight,
            optionsTextColor, menu::selectedTextColor);
        graphicsOptionsPage = createStyledPage(
            graphicsOptionsLabels(), menu::smallFontHeight,
            optionsTextColor, menu::selectedTextColor);
        soundOptionsPage = createStyledPage(
            soundOptionsLabels(), menu::smallFontHeight,
            optionsTextColor, menu::selectedTextColor);
        controlsOptionsPage = createStyledPage(
            controlsOptionsLabels(), menu::smallFontHeight,
            optionsTextColor, menu::selectedTextColor);
        gameOptionNamesPage = createStyledPage(
            labels(
                {"svCamera", "svCameraDist", "svEnableHUD",
                 "svDifficulty", "svSpringBorders",
                 "svUpgradeMaxLevel", "svWeaponMaxLevel",
                 "svMaxPlayers", "svMaxComputers", "svLapsCount",
                 "svEnableMineBug", "svDisableVideo"}),
            menu::smallFontHeight, optionsTextColor,
            menu::selectedTextColor);
        graphicsOptionNamesPage = createStyledPage(
            labels(
                {"svResolution", "svFiltering", "svMultisampling",
                 "svShadow", "svEnv", "svLight", "svPostProcess",
                 "svWindowMode"}),
            menu::smallFontHeight, optionsTextColor,
            menu::selectedTextColor);
        soundOptionNamesPage = createStyledPage(
            labels(
                {"svLanguage", "svCommentator", "svMusic",
                 "svSound", "svSoundDicter"}),
            menu::smallFontHeight, optionsTextColor,
            menu::selectedTextColor);
        controlsKeyboardValuesPage = createStyledPage(
            controlValues(false), menu::smallFontHeight,
            optionsTextColor, menu::selectedTextColor);
        controlsGamepadValuesPage = createStyledPage(
            controlValues(true), menu::smallFontHeight,
            optionsTextColor, menu::selectedTextColor);
        optionsHeaderPage = createStyledPage(
            labels({"svOptions"}), menu::headerFontHeight,
            optionsTextColor, menu::selectedTextColor);
        optionsStatePage = createStyledPage(
            labels({"svGame", "svGraphic", "svNetwork", "svControls"}),
            menu::headerFontHeight, optionsTextColor,
            optionsStateSelectedColor);
        optionsActionPage = createStyledPage(
            labels({"svBack", "svApply"}), menu::headerFontHeight,
            optionsTextColor, menu::selectedTextColor);
        startOptionsLabelPage = createStyledPage(
            labels(
                {"svCamera", "svResolution", "svLanguage",
                 "svCommentator"}),
            menu::smallFontHeight, optionsTextColor,
            menu::selectedTextColor);
        startOptionsValuePage = createStyledPage(
            startOptionsValues(), menu::smallFontHeight,
            optionsTextColor, menu::selectedTextColor);
        startOptionsActionPage = createStyledPage(
            labels({"svApply"}), menu::headerFontHeight,
            optionsTextColor, menu::selectedTextColor);
        {
            std::istringstream words(
                localized("svStartOptionsInfo"));
            std::string word;
            std::string line;
            std::vector<std::string> wrapped;
            while (words >> word)
            {
                const std::string candidate =
                    line.empty() ? word : line + " " + word;
                const auto measured = rrr3d::macos::rasterizeText(
                    candidate, menu::fontFace,
                    menu::smallFontHeight, false,
                    optionsTextColor);
                if (!line.empty() && measured.width > 750U)
                {
                    wrapped.push_back(line);
                    line = word;
                }
                else
                {
                    line = candidate;
                }
            }
            if (!line.empty())
                wrapped.push_back(line);
            for (const auto& infoLine : wrapped)
            {
                startOptionsInfoLines.push_back(createText(
                    *device, infoLine, menu::smallFontHeight,
                    false, optionsTextColor, resolvedFont));
            }
        }
#endif
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Original MainMenu2 font creation failed: "
                  << exception.what() << '\n';
    }
    const TextVisual version = createText(
        *device, model->versionText, menu::smallFontHeight, true,
        menu::selectedTextColor, resolvedFont);
#ifdef RRR3D_AUDIO
    auto createMusicDialogVisuals = [&](const auto& tracks) {
        std::vector<MusicDialogVisual> result;
        result.reserve(tracks.size());
        for (const auto& track : tracks)
        {
            result.push_back(
                {createText(
                     *device, track.band, 32.0F, false,
                     menu::Rgba8{255U, 255U, 255U, 255U},
                     resolvedFont),
                 createText(
                     *device, track.name, 24.0F, false,
                     menu::Rgba8{175U, 175U, 175U, 255U},
                     resolvedFont)});
        }
        return result;
    };
    const auto menuMusicDialogVisuals =
        createMusicDialogVisuals(originalaudio::menuTracks);
#ifdef RRR3D_PHYSICS
    const auto gameMusicDialogVisuals =
        createMusicDialogVisuals(originalaudio::gameTracks);
#endif
#endif
    const TextVisual finalBackText = createText(
        *device, localized("svBack"), menu::headerFontHeight, false,
        menu::Rgba8{214U, 214U, 214U, 255U}, resolvedFont);
    struct FinalCreditSection
    {
        TextVisual caption;
        std::vector<TextVisual> lines;
        float height = 0.0F;
    };
    std::vector<FinalCreditSection> finalCredits;
    float finalCreditsHeight = 0.0F;
    auto trimCreditText = [](std::string_view source) {
        const auto first = source.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos)
            return std::string{};
        const auto last = source.find_last_not_of(" \t\r\n");
        return std::string(source.substr(first, last - first + 1U));
    };
    const std::string finalCreditSource = localized("svCredits");
    std::size_t finalSectionBegin = 0U;
    while (finalSectionBegin < finalCreditSource.size())
    {
        const auto separator =
            finalCreditSource.find("\n\n", finalSectionBegin);
        const auto block = trimCreditText(std::string_view(
            finalCreditSource.data() + finalSectionBegin,
            (separator == std::string::npos
                 ? finalCreditSource.size()
                 : separator) -
                finalSectionBegin));
        if (!block.empty())
        {
            const auto captionEnd = block.find('\n');
            FinalCreditSection section;
            section.caption = createText(
                *device, block.substr(0U, captionEnd),
                menu::smallFontHeight, false,
                menu::Rgba8{220U, 0U, 0U, 255U}, resolvedFont);
            section.height = section.caption.height + 10.0F;
            if (captionEnd != std::string::npos)
            {
                std::size_t lineBegin = captionEnd + 1U;
                while (lineBegin <= block.size())
                {
                    const auto lineEnd = block.find('\n', lineBegin);
                    const auto line = trimCreditText(std::string_view(
                        block.data() + lineBegin,
                        (lineEnd == std::string::npos
                             ? block.size()
                             : lineEnd) -
                            lineBegin));
                    if (!line.empty())
                    {
                        section.lines.push_back(createText(
                            *device, line, menu::smallFontHeight, false,
                            menu::Rgba8{255U, 214U, 205U, 255U},
                            resolvedFont));
                        section.height +=
                            section.lines.back().height;
                    }
                    if (lineEnd == std::string::npos)
                        break;
                    lineBegin = lineEnd + 1U;
                }
            }
            section.height += 40.0F;
            finalCreditsHeight += section.height;
            finalCredits.push_back(std::move(section));
        }
        if (separator == std::string::npos)
            break;
        finalSectionBegin = separator + 2U;
    }
#ifdef RRR3D_PHYSICS
    const TextVisual finishRewardTitle = createText(
        *device, localized("svPrice"), menu::headerFontHeight, false,
        menu::Rgba8{233U, 167U, 63U, 255U}, resolvedFont);
    const TextVisual finishMoneyTitle = createText(
        *device, localized("svMoney"),
        menu::headerFontHeight, false,
        menu::Rgba8{225U, 225U, 225U, 255U}, resolvedFont);
    const TextVisual finishPointsTitle = createText(
        *device, localized("svPoints"),
        menu::headerFontHeight, false,
        menu::Rgba8{225U, 225U, 225U, 255U}, resolvedFont);
    struct FinishRowVisual
    {
        std::size_t racer = 0U;
        Texture photo;
        float photoWidth = 0.0F;
        float photoHeight = 0.0F;
        TextVisual name;
        TextVisual rewardMoney;
        TextVisual rewardPoints;
    };
    std::vector<FinishRowVisual> finishRows;
    auto clearFinishRows = [&]() {
        for (const auto& row : finishRows)
        {
            device->destroy(row.rewardPoints.texture);
            device->destroy(row.rewardMoney.texture);
            device->destroy(row.name.texture);
            device->destroy(row.photo);
        }
        finishRows.clear();
    };
    const TextVisual achievementRewards = createText(
        *device, localized("svRewards"), menu::headerFontHeight,
        false, menu::normalTextColor, resolvedFont);
    TextVisual achievementPoints = createText(
        *device,
        localized("svPoints") + " " +
            std::to_string(profileState.achievementPoints),
        menu::headerFontHeight, false,
        menu::Rgba8{250, 88, 0, 255}, resolvedFont);
#endif

    auto pageValid = [](const MenuPageVisual& page) {
        return !page.labels.empty() &&
               page.normal.size() == page.labels.size() &&
               page.selected.size() == page.labels.size() &&
               page.disabled.size() == page.labels.size() &&
               page.enabled.size() == page.labels.size() &&
               std::all_of(
                   page.normal.begin(), page.normal.end(),
                   [](const TextVisual& item) {
                       return valid(item.texture);
                   }) &&
               std::all_of(
                   page.selected.begin(), page.selected.end(),
                   [](const TextVisual& item) {
                       return valid(item.texture);
                   }) &&
               std::all_of(
                   page.disabled.begin(), page.disabled.end(),
                   [](const TextVisual& item) {
                       return valid(item.texture);
                   });
    };

    const bool gpuResourcesValid =
        valid(shader) &&
#ifdef RRR3D_PHYSICS
        valid(raceShader) &&
#endif
        valid(quad) && valid(background) && valid(topPanel) &&
        valid(bottomPanel) && valid(selection) && valid(cursor) &&
        valid(startupYard) && valid(startupLab) &&
        valid(startupLoad) &&
#ifdef RRR3D_AUDIO
        valid(musicDialogFrame) &&
        std::all_of(
            menuMusicDialogVisuals.begin(),
            menuMusicDialogVisuals.end(),
            [](const MusicDialogVisual& item) {
                return valid(item.title.texture) &&
                       valid(item.info.texture);
            }) &&
#ifdef RRR3D_PHYSICS
        std::all_of(
            gameMusicDialogVisuals.begin(),
            gameMusicDialogVisuals.end(),
            [](const MusicDialogVisual& item) {
                return valid(item.title.texture) &&
                       valid(item.info.texture);
            }) &&
#endif
#endif
        valid(version.texture) && valid(finalBack) &&
        valid(finalBackSelected) && valid(finalBackText.texture) &&
        std::all_of(
            finalSlides.begin(), finalSlides.end(),
            [](Texture texture) { return valid(texture); }) &&
        !finalCredits.empty() &&
        std::all_of(
            finalCredits.begin(), finalCredits.end(),
            [](const FinalCreditSection& section) {
                return valid(section.caption.texture) &&
                       std::all_of(
                           section.lines.begin(), section.lines.end(),
                           [](const TextVisual& line) {
                               return valid(line.texture);
                           });
            }) &&
        pageValid(mainPage) && pageValid(gameModePage) &&
        pageValid(tournamentPage) && pageValid(difficultyPage) &&
        pageValid(profilePage) && pageValid(networkPage) &&
#ifdef RRR3D_NETWORK
        pageValid(networkServerTypePage) &&
        pageValid(networkClientTypePage) &&
        pageValid(networkBrowserPage) && pageValid(networkIpPage) &&
        pageValid(networkAddressInfoPage) &&
        pageValid(networkStatusPage) &&
        pageValid(networkIpValuePage) &&
#endif
        pageValid(optionsPage) && pageValid(creditsPage);
#ifdef RRR3D_PHYSICS
    const bool optionsResourcesValid =
        pageValid(raceMenuPage) &&
        pageValid(gamersNamePage) &&
        pageValid(gamersInfoPage) &&
        pageValid(gamersBonusPage) &&
        pageValid(raceMainHeadersPage) &&
        pageValid(raceMainInfoPage) &&
        pageValid(raceMainStatsPage) &&
        pageValid(garagePage) &&
        pageValid(garageInfoPage) &&
        pageValid(garageStatsPage) &&
        pageValid(workshopPage) &&
        pageValid(planetsPage) &&
        pageValid(angarInfoPage) &&
        pageValid(achievementsPage) &&
        pageValid(achievementPricePage) &&
        pageValid(gameOptionsPage) &&
        pageValid(graphicsOptionsPage) &&
        pageValid(soundOptionsPage) &&
        pageValid(controlsOptionsPage) &&
        pageValid(gameOptionNamesPage) &&
        pageValid(graphicsOptionNamesPage) &&
        pageValid(soundOptionNamesPage) &&
        pageValid(controlsKeyboardValuesPage) &&
        pageValid(controlsGamepadValuesPage) &&
        pageValid(optionsHeaderPage) &&
        pageValid(optionsStatePage) &&
        pageValid(optionsActionPage) &&
        pageValid(startOptionsLabelPage) &&
        pageValid(startOptionsValuePage) &&
        pageValid(startOptionsActionPage) &&
        !startOptionsInfoLines.empty() &&
        std::all_of(
            startOptionsInfoLines.begin(),
            startOptionsInfoLines.end(),
            [](const TextVisual& line) {
                return valid(line.texture);
            }) &&
        valid(finishLeftFrame) && valid(finishRightFrame) &&
        valid(finishLineFrame) &&
        std::all_of(
            finishCups.begin(), finishCups.end(),
            [](Texture texture) { return valid(texture); }) &&
        valid(finishRewardTitle.texture) &&
        valid(finishMoneyTitle.texture) &&
        valid(finishPointsTitle.texture) && valid(acceptFrame) &&
        valid(acceptButton) && valid(acceptButtonSelected) &&
        valid(infoDialogFrame) && valid(infoDialogButton) &&
        valid(infoDialogButtonSelected) &&
        valid(profileArrow) && valid(profileArrowSelected) &&
        valid(profileArrowDisabled) &&
        valid(gamersSpace) && valid(gamersBottomPanel) &&
        valid(gamersPhotoLight) && valid(gamersNextArrow) &&
        valid(gamersNextArrowSelected) &&
        gamersBossTextures.size() == originalGarage->gamers.size() &&
        std::all_of(
            gamersBossTextures.begin(), gamersBossTextures.end(),
            [](Texture texture) { return valid(texture); }) &&
        valid(optionsBackground) && valid(startOptionsBackground) &&
        valid(loadingFrame) &&
        valid(startOptionsButton) && valid(optionsRow) &&
        valid(controlsRow) && valid(optionsArrow) &&
        valid(optionsArrowSelected) &&
        valid(optionsBarBackground) && valid(optionsBar) &&
        valid(optionsButton) && valid(optionsButtonSelected) &&
        valid(optionsKey) && valid(optionsKeySelected) &&
        valid(keyboardIcon) && valid(gamepadIcon) &&
        valid(optionsMask) &&
        valid(raceTopPanel) && valid(raceBottomPanel) &&
        valid(raceMenuButton) &&
        valid(raceMenuButtonSelected) &&
        valid(raceMoney) && valid(raceStats) &&
        valid(raceImageFrame) && valid(raceChargeBar) &&
        valid(raceStatBar) &&
#ifdef RRR3D_NETWORK
        valid(networkPlayerFrame) && valid(networkPlayerKick) &&
        valid(networkPlayerKickSelected) &&
        valid(networkPlayerReady) &&
        valid(networkPlayerReadySelected) &&
#endif
        std::all_of(
            raceMenuIcons.begin(), raceMenuIcons.end(),
            [](Texture texture) { return valid(texture); }) &&
        std::all_of(
            raceWeatherIcons.begin(), raceWeatherIcons.end(),
            [](Texture texture) { return valid(texture); }) &&
        valid(garageTopPanel) && valid(garageBottomPanel) &&
        valid(garageSidePanel) && valid(garageMoney) &&
        valid(garageStats) && valid(garageStatBar) &&
        valid(garageCarBox) && valid(garageCarBoxSelected) &&
        valid(garageLock) && valid(garageColorBox) &&
        valid(garageColorBoxBackground) &&
        valid(garageColorBoxSelected) && valid(garageArrow) &&
        valid(garageArrowSelected) && valid(garageBack) &&
        valid(garageBackSelected) && valid(garageBuy) &&
        valid(garageBuySelected) && valid(workshopTopPanel) &&
        valid(workshopBottomPanel) && valid(workshopLeftPanel) &&
        valid(workshopSlot) && valid(workshopSlotFrame) &&
        valid(workshopChargeBox) && valid(workshopChargeBar) &&
        valid(workshopChargeButton) &&
        valid(workshopChargeButtonSelected) &&
        valid(workshopStatBarPlus) && valid(workshopInfoFrame) &&
        valid(angarBottomPanel) && valid(angarPlanetInfo) &&
        valid(angarClose) && valid(angarCloseSelected) &&
        valid(angarDoorSlot) && valid(angarDoorSlotSelected) &&
        valid(angarDoorDown) && valid(angarDoorUp) &&
        valid(achievementBackground) &&
        valid(achievementBottomPanel) && valid(achievementPanel) &&
        valid(achievementClose) && valid(achievementOk) &&
        valid(achievementOkSelected) &&
        achievementLockedTextures.size() ==
            originalAchievementVisuals.size() &&
        achievementOpenedTextures.size() ==
            originalAchievementVisuals.size() &&
        std::all_of(
            achievementLockedTextures.begin(),
            achievementLockedTextures.end(),
            [](Texture texture) { return valid(texture); }) &&
        std::all_of(
            achievementOpenedTextures.begin(),
            achievementOpenedTextures.end(),
            [](Texture texture) { return valid(texture); }) &&
        angarBossTextures.size() ==
            originalGarage->planets.size() &&
        std::all_of(
            angarBossTextures.begin(), angarBossTextures.end(),
            [](Texture texture) { return valid(texture); }) &&
        std::all_of(
            workshopUpgradeTextures.begin(),
            workshopUpgradeTextures.end(),
            [](Texture texture) { return valid(texture); }) &&
        std::all_of(
            workshopSlotIconTextures.begin(),
            workshopSlotIconTextures.end(),
            [](Texture texture) { return valid(texture); }) &&
        garageCarTextures.size() == originalGarage->cars.size() &&
        std::all_of(
            garageCarTextures.begin(), garageCarTextures.end(),
            [](Texture texture) { return valid(texture); }) &&
        std::all_of(
            garageColorTextures.begin(),
            garageColorTextures.end(),
            [](Texture texture) { return valid(texture); }) &&
        valid(achievementRewards.texture) &&
        valid(achievementPoints.texture);
#else
    const bool optionsResourcesValid = true;
#endif

    auto releaseResources = [&]() {
#ifdef RRR3D_AUDIO
#ifdef RRR3D_PHYSICS
        for (const auto& item : gameMusicDialogVisuals)
        {
            device->destroy(item.info.texture);
            device->destroy(item.title.texture);
        }
#endif
        for (const auto& item : menuMusicDialogVisuals)
        {
            device->destroy(item.info.texture);
            device->destroy(item.title.texture);
        }
        device->destroy(musicDialogFrame);
#endif
#ifdef RRR3D_PHYSICS
        clearFinishRows();
#ifdef RRR3D_NETWORK
        for (const auto& player : networkRacePlayerVisuals)
        {
            device->destroy(player.readyLabel.texture);
            device->destroy(player.name.texture);
        }
        networkRacePlayerVisuals.clear();
#endif
        for (const auto& line : startOptionsInfoLines)
            device->destroy(line.texture);
        destroyPage(startOptionsActionPage);
        destroyPage(startOptionsValuePage);
        destroyPage(startOptionsLabelPage);
        destroyPage(raceMainStatsPage);
        device->destroy(finishPointsTitle.texture);
        device->destroy(finishMoneyTitle.texture);
        device->destroy(finishRewardTitle.texture);
        device->destroy(achievementPoints.texture);
        device->destroy(achievementRewards.texture);
#endif
        for (const auto& section : finalCredits)
        {
            for (const auto& line : section.lines)
                device->destroy(line.texture);
            device->destroy(section.caption.texture);
        }
        device->destroy(finalBackText.texture);
        for (const auto texture : finalSlides)
            device->destroy(texture);
        device->destroy(finalBackSelected);
        device->destroy(finalBack);
        device->destroy(version.texture);
#ifdef RRR3D_PHYSICS
        destroyPage(garageStatsPage);
        destroyPage(garageInfoPage);
        destroyPage(achievementPricePage);
        destroyPage(achievementsPage);
        destroyPage(angarInfoPage);
        destroyPage(planetsPage);
        destroyWorkshopWeaponDialog(*device, workshopWeaponDialog);
        destroyInfoDialog(*device, infoDialog);
        destroyAcceptDialog(*device, acceptDialog);
        destroyPage(workshopHintPage);
        destroyPage(workshopStatsPage);
        destroyPage(workshopControlsPage);
        destroyPage(workshopPage);
        destroyPage(garagePage);
        destroyPage(raceMainInfoPage);
        destroyPage(raceMainHeadersPage);
        destroyPage(gamersBonusPage);
        destroyPage(gamersInfoPage);
        destroyPage(gamersNamePage);
        destroyPage(raceMenuPage);
        destroyPage(controlsOptionsPage);
        destroyPage(soundOptionsPage);
        destroyPage(graphicsOptionsPage);
        destroyPage(gameOptionsPage);
        destroyPage(optionsActionPage);
        destroyPage(optionsStatePage);
        destroyPage(optionsHeaderPage);
        destroyPage(controlsGamepadValuesPage);
        destroyPage(controlsKeyboardValuesPage);
        destroyPage(soundOptionNamesPage);
        destroyPage(graphicsOptionNamesPage);
        destroyPage(gameOptionNamesPage);
        device->destroy(optionsMask);
        for (const auto texture : garageColorTextures)
            device->destroy(texture);
        for (const auto texture : garageCarTextures)
            device->destroy(texture);
        for (const auto texture : finishCups)
            device->destroy(texture);
        device->destroy(finishLineFrame);
        device->destroy(finishRightFrame);
        device->destroy(finishLeftFrame);
        for (const auto texture : workshopSlotIconTextures)
            device->destroy(texture);
        for (const auto texture : workshopUpgradeTextures)
            device->destroy(texture);
        for (const auto texture : angarBossTextures)
            device->destroy(texture);
        for (const auto texture : achievementOpenedTextures)
            device->destroy(texture);
        for (const auto texture : achievementLockedTextures)
            device->destroy(texture);
        device->destroy(achievementOkSelected);
        device->destroy(achievementOk);
        device->destroy(achievementClose);
        device->destroy(achievementPanel);
        device->destroy(achievementBottomPanel);
        device->destroy(achievementBackground);
        device->destroy(angarDoorUp);
        device->destroy(angarDoorDown);
        device->destroy(angarDoorSlotSelected);
        device->destroy(angarDoorSlot);
        device->destroy(angarCloseSelected);
        device->destroy(angarClose);
        device->destroy(angarPlanetInfo);
        device->destroy(angarBottomPanel);
        device->destroy(workshopStatBarPlus);
        device->destroy(workshopInfoFrame);
        device->destroy(workshopChargeButtonSelected);
        device->destroy(workshopChargeButton);
        device->destroy(workshopChargeBar);
        device->destroy(workshopChargeBox);
        device->destroy(workshopSlotFrame);
        device->destroy(workshopSlot);
        device->destroy(workshopLeftPanel);
        device->destroy(workshopBottomPanel);
        device->destroy(workshopTopPanel);
        device->destroy(garageBuySelected);
        device->destroy(garageBuy);
        device->destroy(garageBackSelected);
        device->destroy(garageBack);
        device->destroy(garageArrowSelected);
        device->destroy(garageArrow);
        device->destroy(garageColorBoxSelected);
        device->destroy(garageColorBoxBackground);
        device->destroy(garageColorBox);
        device->destroy(garageLock);
        device->destroy(garageCarBoxSelected);
        device->destroy(garageCarBox);
        device->destroy(garageStatBar);
        device->destroy(garageStats);
        device->destroy(garageMoney);
        device->destroy(garageSidePanel);
        device->destroy(garageBottomPanel);
        device->destroy(garageTopPanel);
        for (const auto texture : raceWeatherIcons)
            device->destroy(texture);
        for (const auto texture : raceMenuIcons)
            device->destroy(texture);
#ifdef RRR3D_NETWORK
        device->destroy(networkPlayerReadySelected);
        device->destroy(networkPlayerReady);
        device->destroy(networkPlayerKickSelected);
        device->destroy(networkPlayerKick);
        device->destroy(networkPlayerFrame);
#endif
        device->destroy(raceStatBar);
        device->destroy(raceChargeBar);
        device->destroy(raceImageFrame);
        device->destroy(raceStats);
        device->destroy(raceMoney);
        device->destroy(raceMenuButtonSelected);
        device->destroy(raceMenuButton);
        device->destroy(raceBottomPanel);
        device->destroy(raceTopPanel);
        device->destroy(gamepadIcon);
        device->destroy(keyboardIcon);
        device->destroy(optionsKeySelected);
        device->destroy(optionsKey);
        device->destroy(optionsButtonSelected);
        device->destroy(optionsButton);
        device->destroy(startOptionsButton);
        device->destroy(optionsBar);
        device->destroy(optionsBarBackground);
        device->destroy(optionsArrowSelected);
        device->destroy(optionsArrow);
        device->destroy(controlsRow);
        device->destroy(optionsRow);
        device->destroy(loadingFrame);
        device->destroy(startOptionsBackground);
        device->destroy(optionsBackground);
        device->destroy(acceptButtonSelected);
        device->destroy(acceptButton);
        device->destroy(acceptFrame);
        device->destroy(infoDialogButtonSelected);
        device->destroy(infoDialogButton);
        device->destroy(infoDialogFrame);
        device->destroy(profileArrowDisabled);
        device->destroy(profileArrowSelected);
        device->destroy(profileArrow);
        for (const auto texture : gamersBossTextures)
            device->destroy(texture);
        device->destroy(gamersNextArrowSelected);
        device->destroy(gamersNextArrow);
        device->destroy(gamersPhotoLight);
        device->destroy(gamersBottomPanel);
        device->destroy(gamersSpace);
#endif
        destroyPage(creditsPage);
        destroyPage(optionsPage);
#ifdef RRR3D_NETWORK
        destroyPage(networkIpValuePage);
        destroyPage(networkStatusPage);
        destroyPage(networkAddressInfoPage);
        destroyPage(networkIpPage);
        destroyPage(networkBrowserPage);
        destroyPage(networkClientTypePage);
        destroyPage(networkServerTypePage);
#endif
        destroyPage(networkPage);
        destroyPage(profilePage);
        destroyPage(difficultyPage);
        destroyPage(tournamentPage);
        destroyPage(gameModePage);
        destroyPage(mainPage);
        device->destroy(startupLoad);
        device->destroy(startupLab);
        device->destroy(startupYard);
        device->destroy(cursor);
        device->destroy(selection);
        device->destroy(bottomPanel);
        device->destroy(topPanel);
        device->destroy(background);
        device->destroy(quad);
#ifdef RRR3D_PHYSICS
        device->destroy(raceShader);
#endif
        device->destroy(shader);
    };

    if (!gpuResourcesValid || !optionsResourcesValid)
    {
        std::cerr << "Unable to create original MainMenu2 GPU resources\n";
        releaseResources();
        device.reset();
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }

    std::cout << "RRR3D renderer backend: bgfx/" << device->backendName()
              << '\n'
              << "MainMenu2 artwork: " << model->backgroundImage.virtualPath
              << ", " << model->topPanelImage.virtualPath << ", "
              << model->selectionImage.virtualPath << '\n'
              << "MainMenu2 source: shared specification used by legacy "
                 "MainMenu2.cpp\n"
              << "MainMenu2 font: requested " << menu::fontFace
              << ", resolved " << resolvedFont << '\n';
#ifdef RRR3D_GAMEPAD_INPUT
    std::cout << "Input: SDL3 keyboard/mouse/gamepad, "
              << input.connectedGamepadCount() << " gamepad(s)\n";
#endif

#ifdef RRR3D_PHYSICS
    std::string physicsError;
    auto physicsWorld = r3d::physics::createOriginalVehicleWorld(
        *physicsDescription, physicsError);
    r3d::game::originalrace::OriginalRaceSession raceSession(*originalRace);
    raceSession.setCampaign(true);
    raceSession.applyPlayerProfile(profileState.player);
    raceSession.applyAchievementProfile(profileState);
    raceSession.setEnableMineBug(profileState.config.enableMineBug);
    raceSession.setSpringBorders(profileState.config.springBorders);
    rrr3d::race::OriginalRaceRenderer raceRenderer;
    rrr3d::race::OriginalRaceRenderer garageRenderer;
    rrr3d::race::OriginalRaceRenderer angarRenderer;
    rrr3d::race::OriginalWorkshopRenderer workshopRenderer;
    rrr3d::race::OriginalRaceHud raceHud;
    r3d::game::originalui::OriginalUserChat userChat;
    UserChatVisual userChatVisual;
    if (!physicsWorld ||
        !raceRenderer.initialize(*device, *resources, *originalRace,
                                 static_cast<std::uint32_t>(pixelWidth),
                                 static_cast<std::uint32_t>(pixelHeight),
                                 physicsError) ||
        !garageRenderer.initialize(
            *device, *resources, *originalGarageScene,
            static_cast<std::uint32_t>(pixelWidth),
            static_cast<std::uint32_t>(pixelHeight), physicsError) ||
        !angarRenderer.initialize(
            *device, *resources, *originalAngarScene,
            static_cast<std::uint32_t>(pixelWidth),
            static_cast<std::uint32_t>(pixelHeight), physicsError) ||
        !workshopRenderer.initialize(
            *device, *resources, *originalGarage, *originalRace,
            physicsError) ||
        !raceHud.initialize(*device, *resources, *originalRace,
                            activeLanguage,
                            profileState.player.difficulty,
                            true,
                            physicsError))
    {
        std::cerr << "Original race initialization failed: " << physicsError
                  << '\n';
        workshopRenderer.shutdown(*device);
        angarRenderer.shutdown(*device);
        garageRenderer.shutdown(*device);
        raceRenderer.shutdown(*device);
        raceHud.shutdown(*device);
        releaseResources();
        device.reset();
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }
    if (options->raceRenderSmokeTest)
    {
        std::string resizeError;
        const auto smokeWidth = static_cast<std::uint32_t>(
            std::max(pixelWidth / 2, 320));
        const auto smokeHeight = static_cast<std::uint32_t>(
            std::max(pixelHeight / 2, 200));
        if (!raceRenderer.resize(
                *device, smokeWidth, smokeHeight, resizeError) ||
            !raceRenderer.resize(
                *device, static_cast<std::uint32_t>(pixelWidth),
                static_cast<std::uint32_t>(pixelHeight), resizeError) ||
            !garageRenderer.resize(
                *device, smokeWidth, smokeHeight, resizeError) ||
            !garageRenderer.resize(
                *device, static_cast<std::uint32_t>(pixelWidth),
                static_cast<std::uint32_t>(pixelHeight), resizeError))
        {
            std::cerr << "M9.3 renderer target resize round-trip failed: "
                      << resizeError << '\n';
            workshopRenderer.shutdown(*device);
            angarRenderer.shutdown(*device);
            garageRenderer.shutdown(*device);
            raceRenderer.shutdown(*device);
            raceHud.shutdown(*device);
            releaseResources();
            device.reset();
            SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
            input.shutdown();
#endif
            SDL_Quit();
            return EXIT_FAILURE;
        }
    }
    std::vector<r3d::physics::VehicleState> garageVehicles;
    garageVehicles.reserve(originalGarageScene->vehicles.size());
    for (const auto& vehicle : originalGarageScene->vehicles)
        garageVehicles.push_back(makeGarageVehicleState(vehicle));
    std::vector<r3d::game::originalrace::RacerRuntime>
        garageRacerRuntime(originalGarageScene->racers.size());
    std::vector<bool> garageDecorationActive{true, false};
    const std::vector<
        r3d::game::originalrace::DecorationFragmentState>
        garageDecorationFragments;
    const std::vector<
        r3d::game::originalrace::VehicleDeathFragmentState>
        garageVehicleDeathFragments;
    const std::vector<bool> garageBonusActive;
    const std::vector<r3d::game::originalrace::RaceEffect>
        garageEffects;
    const std::vector<r3d::game::originalrace::MineRuntime>
        garageMines;
    const std::vector<r3d::game::originalrace::ProjectileRuntime>
        garageProjectiles;
    const std::vector<r3d::physics::VehicleState> angarVehicles;
    const std::vector<
        r3d::game::originalrace::RacerRuntime> angarRacerRuntime;
    std::vector<bool> angarDecorationActive{true, true};
    float garageSceneSeconds = 0.0F;
    std::cout << "Milestone 9 race: " << originalRace->levelPath << ", "
              << originalRace->lapCount << " laps, "
              << originalRace->trackInstances.size()
              << " original track placements, car "
              << originalRace->vehicle.record
              << ", Jolt backend with original db.xml parameters\n";
#endif

#ifdef RRR3D_AUDIO
    rrr3d::audio::SdlAudioBackend audio;
    std::string audioError;
    if (!audio.initialize(audioError))
    {
        std::cerr << "Audio initialization failed: " << audioError << '\n';
        releaseResources();
        device.reset();
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }

    std::cout << "Audio: SDL3/" << audio.driverName()
              << ", 48 kHz stereo float mixer, default output '"
              << audio.outputDeviceName() << "'\n";
    if (options->audioSmokeTest &&
        !rrr3d::audio::runSdlAudioSmokeTest(audio, *resources, audioError))
    {
        std::cerr << "Milestone 8 audio smoke test failed: " << audioError
                  << '\n';
        audio.shutdown();
        releaseResources();
        device.reset();
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }
    if (options->audioSmokeTest)
    {
        std::cout << "Milestone 8 audio smoke: 182 original Ogg containers; "
                     "music, MainMenu2 click, and gameplay decode; PCM "
                     "signal, mixing, legacy volumes, pause/resume, loop "
                     "boundary, device events, and release passed\n";
    }

    if (options->audioSmokeTest &&
        !r3d::game::runMusicCatSmokeTest(audioError))
    {
        std::cerr << "Milestone 8 MusicCat policy smoke test failed: "
                  << audioError << '\n';
        audio.shutdown();
        releaseResources();
        device.reset();
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }

    rrr3d::audio::OriginalMenuSounds menuSounds(audio, *resources);
    if (!menuSounds.initialize(audioError))
    {
        std::cerr << "Original Menu SoundSheme loading failed: "
                  << audioError << '\n';
        menuSounds.shutdown();
        audio.shutdown();
        releaseResources();
        device.reset();
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }

    // snd::Engine::Init sets the XAudio2 mastering voice to 0.1 before the
    // three Logic submix/category gains. The previous 1.0 made the portable
    // game mix ten times hotter than the Windows source and hard-clipped
    // simultaneous race effects in the final SDL mixer.
    audio.setMasterVolume(originalaudio::masteringVoiceVolume);
#ifdef RRR3D_PHYSICS
    audio.setBusVolume(r3d::audio::Bus::Music,
                       profileState.config.musicVolume);
    audio.setBusVolume(r3d::audio::Bus::Effects,
                       profileState.config.effectsVolume);
    audio.setBusVolume(r3d::audio::Bus::Voice,
                       profileState.config.voiceVolume);
#else
    audio.setBusVolume(r3d::audio::Bus::Music,
                       originalaudio::defaultMusicVolume);
    audio.setBusVolume(r3d::audio::Bus::Effects,
                       originalaudio::defaultEffectsVolume);
    audio.setBusVolume(r3d::audio::Bus::Voice,
                       originalaudio::defaultVoiceVolume);
#endif

    auto musicStatePath =
        rrr3d::platform::save_directory() / "menu-music.state";
    if (options->audioSmokeTest)
    {
        musicStatePath =
            rrr3d::platform::save_directory() / "menu-music-smoke.state";
        std::error_code removeError;
        std::filesystem::remove(musicStatePath, removeError);
        auto temporary = musicStatePath;
        temporary += ".tmp";
        std::filesystem::remove(temporary, removeError);
    }
    rrr3d::audio::OriginalMenuMusic music(
        audio, *resources, musicStatePath,
        options->audioSmokeTest ? 0x4d75736963436174ULL
                                : rrr3d::platform::steady_nanoseconds(),
        options->audioSmokeTest,
        musicTracks(originalaudio::menuTracks),
        options->audioSmokeTest
            ? std::vector<std::size_t>{}
            : musicPlaylist(profileState.config.menuMusicPlaylist,
                            originalaudio::menuTracks.size()));
    if (!music.initialize(audioError))
    {
        std::cerr << "Original MusicCat initialization failed: "
                  << audioError << '\n';
        music.shutdown();
        menuSounds.shutdown();
        audio.shutdown();
        releaseResources();
        device.reset();
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }

    const auto finalMusicStatePath =
        rrr3d::platform::save_directory() / "final-music.state";
    rrr3d::audio::OriginalMenuMusic finalMusic(
        audio, *resources, finalMusicStatePath,
        rrr3d::platform::steady_nanoseconds() ^
            0x46696e616c4d7573ULL,
        false, musicTracks(originalaudio::finalTracks), {0U});
    if (!finalMusic.initialize(audioError) ||
        !finalMusic.pause(true, audioError))
    {
        std::cerr << "Original FinalMenu music initialization failed: "
                  << audioError << '\n';
        finalMusic.shutdown();
        music.shutdown();
        menuSounds.shutdown();
        audio.shutdown();
        releaseResources();
        device.reset();
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }

#ifdef RRR3D_PHYSICS
    const auto gameMusicStatePath =
        rrr3d::platform::save_directory() / "game-music.state";
    rrr3d::audio::OriginalMenuMusic gameMusic(
        audio, *resources, gameMusicStatePath,
        rrr3d::platform::steady_nanoseconds() ^
            0x47616d654d757369ULL,
        false, musicTracks(originalaudio::gameTracks),
        musicPlaylist(profileState.config.gameMusicPlaylist,
                      originalaudio::gameTracks.size()));
    if (!gameMusic.initialize(audioError) ||
        !gameMusic.pause(true, audioError))
    {
        std::cerr << "Original game MusicCat initialization failed: "
                  << audioError << '\n';
        gameMusic.shutdown();
        finalMusic.shutdown();
        music.shutdown();
        menuSounds.shutdown();
        audio.shutdown();
        releaseResources();
        device.reset();
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }
#endif

    enum class OriginalMusicDialogSource
    {
        Menu,
#ifdef RRR3D_PHYSICS
        Game,
#endif
    };
    OriginalMusicDialogSource musicDialogSource =
        OriginalMusicDialogSource::Menu;
    std::size_t musicDialogTrack = 0U;
    float musicDialogTime = -1.0F;
    float musicDialogOffset = 0.0F;
    bool musicDialogVisible = false;
    bool menuMusicDialogObserved = false;
#ifdef RRR3D_PHYSICS
    bool raceMusicDialogObserved =
        !options->raceRenderSmokeTest;
#endif
    auto lastMenuMusicTrack = music.currentTrack();
#ifdef RRR3D_PHYSICS
    auto lastGameMusicTrack = gameMusic.currentTrack();
#endif
    auto showOriginalMusicInfo =
        [&](OriginalMusicDialogSource source,
            std::optional<std::size_t> track) {
            if (!track)
                return;
            std::size_t visualCount = menuMusicDialogVisuals.size();
#ifdef RRR3D_PHYSICS
            if (source == OriginalMusicDialogSource::Game)
                visualCount = gameMusicDialogVisuals.size();
#endif
            if (*track >= visualCount)
                return;
            musicDialogSource = source;
            musicDialogTrack = *track;
            // Menu::ShowMusicInfo updates the two labels immediately but
            // restarts the five-second animation only after the previous
            // popup has completely hidden.
            if (musicDialogTime == -1.0F)
                musicDialogTime = -0.999F;
        };
    // GameMode::StartGame shows the current track only after FreeIntro.
    if (!sourceStartupRequested)
    {
        showOriginalMusicInfo(
            OriginalMusicDialogSource::Menu, lastMenuMusicTrack);
    }

    std::cout << "Original MusicCat: background decode, shuffled playlist, "
                 "auto Next, pause/resume, state "
              << musicStatePath << "\nOriginal menu tracks:";
    for (const auto& track : originalaudio::menuTracks)
        std::cout << " [" << track.band << " - " << track.name
                  << ": Data/" << track.path << ']';
    std::cout << "\nOriginal Menu SoundSheme: "
              << menuSounds.loadedSoundCount()
              << " source UI sounds, one interrupting Effects voice\n";

    auto playOriginalMenuSound =
        [&](rrr3d::audio::OriginalMenuSound sound) {
        std::string clickError;
        if (!menuSounds.play(sound, clickError))
        {
            SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO,
                        "Unable to play original Menu SoundSheme: %s",
                        clickError.c_str());
            return false;
        }
        return true;
    };
    auto playMainButtonClick = [&]() {
        return playOriginalMenuSound(
            rrr3d::audio::OriginalMenuSound::ButtonClick);
    };
#ifdef RRR3D_PHYSICS
    struct EngineAudio
    {
        r3d::audio::SoundHandle idle = r3d::audio::invalidSound;
        r3d::audio::SoundHandle rpm = r3d::audio::invalidSound;
        r3d::audio::VoiceHandle idleVoice = r3d::audio::invalidVoice;
        r3d::audio::VoiceHandle rpmVoice = r3d::audio::invalidVoice;
        // SoundMotor approaches the reported RPM at 10000 RPM/s before it
        // mixes the idle and high layers.
        float currentRpm = 0.0F;
        bool spatialProxyPlaying = true;
    };
    struct WheelSlipAudio
    {
        r3d::audio::VoiceHandle voice = r3d::audio::invalidVoice;
        bool spatialProxyPlaying = false;
    };
    struct ShotEffectAudio
    {
        std::size_t owner = 0;
        std::size_t source = 0;
        std::string path;
        r3d::physics::Vec3 position;
        r3d::audio::SoundHandle sound = r3d::audio::invalidSound;
        r3d::audio::VoiceHandle voice = r3d::audio::invalidVoice;
        bool spatialProxyPlaying = false;
    };
    struct ContactEffectAudio
    {
        std::size_t racer = 0;
        std::uint32_t actor = 0;
        r3d::physics::CollisionSurface surface =
            r3d::physics::CollisionSurface::TrackPlane;
        r3d::physics::Vec3 position;
        r3d::audio::SoundHandle sound = r3d::audio::invalidSound;
        r3d::audio::VoiceHandle voice = r3d::audio::invalidVoice;
        bool spatialProxyPlaying = false;
    };
    struct TimedEffectAudio
    {
        std::size_t followRacer =
            r3d::game::originalrace::RacerRuntime::invalidWeapon;
        float remainingSeconds = 0.0F;
        r3d::physics::Vec3 position;
        r3d::physics::Vec3 followOffset;
        r3d::audio::SoundHandle sound = r3d::audio::invalidSound;
        r3d::audio::VoiceHandle voice = r3d::audio::invalidVoice;
        bool spatialProxyPlaying = false;
    };
    std::map<std::string, r3d::audio::SoundHandle> engineSounds;
    std::map<r3d::audio::SoundHandle, float> engineSoundVolumes;
    std::vector<EngineAudio> engineAudio(originalRace->racers.size());
    std::vector<std::vector<WheelSlipAudio>>
        wheelSlipVoices(originalRace->racers.size());
    std::vector<ShotEffectAudio> shotEffectAudio;
    std::vector<ContactEffectAudio> contactEffectAudio;
    std::vector<TimedEffectAudio> timedEffectAudio;
    auto loadEngineSound = [&](const std::string& path) {
        const auto found = engineSounds.find(path);
        if (found != engineSounds.end())
            return found->second;
        r3d::audio::SoundInfo info;
        const auto sound =
            audio.loadOgg(resources->resolve(path), info, audioError);
        if (sound != r3d::audio::invalidSound)
        {
            engineSounds.emplace(path, sound);
            engineSoundVolumes.emplace(
                sound, rrr3d::audio::originalSoundVolume(path));
        }
        return sound;
    };
    bool engineAudioValid =
        rrr3d::audio::runOriginalSpatialAudioSmokeTest();
    const auto wheelSlipSound =
        loadEngineSound(originalRace->wheelSlipSoundPath);
    engineAudioValid =
        wheelSlipSound != r3d::audio::invalidSound;
    for (std::size_t racer = 0;
         racer < originalRace->racers.size(); ++racer)
    {
        const auto& sourceRacer = originalRace->racers[racer];
        const auto& vehicle =
            sourceRacer.hasConfiguredVehicle
                ? sourceRacer.configuredVehicle
                : originalRace->vehicles.at(sourceRacer.vehicle);
        engineAudio[racer].idle =
            loadEngineSound(vehicle.idleSoundPath);
        engineAudio[racer].rpm =
            loadEngineSound(vehicle.rpmSoundPath);
        engineAudioValid =
            engineAudioValid &&
            engineAudio[racer].idle != r3d::audio::invalidSound &&
            engineAudio[racer].rpm != r3d::audio::invalidSound;
    }
    auto preloadEffectAudio = [&]() {
        bool valid = true;
        const auto preloadDefinition = [&](const auto& definition) {
            for (const auto& path : definition.soundPaths)
            {
                valid =
                    loadEngineSound(path) !=
                        r3d::audio::invalidSound &&
                    valid;
            }
        };
        for (const auto& sourceRacer : originalRace->racers)
        {
            const auto& vehicle =
                sourceRacer.hasConfiguredVehicle
                    ? sourceRacer.configuredVehicle
                    : originalRace->vehicles.at(sourceRacer.vehicle);
            preloadDefinition(vehicle.lowLifeEffect);
            preloadDefinition(vehicle.energyDamageEffect);
            preloadDefinition(vehicle.shieldEffect);
            for (const auto& death : vehicle.deathEffects)
                preloadDefinition(death.visual);
        }
        for (const auto& bonus : originalRace->bonuses)
        {
            preloadDefinition(bonus.visual);
            preloadDefinition(bonus.deathEffect.visual);
        }
        for (const auto& weapon : originalRace->weapons)
        {
            for (const auto& path : weapon.shotEffect.soundPaths)
            {
                valid =
                    loadEngineSound(path) !=
                        r3d::audio::invalidSound &&
                    valid;
            }
            preloadDefinition(weapon.shotEffect.visual);
            for (const auto& projectile : weapon.projectiles)
            {
                preloadDefinition(projectile.visual);
                preloadDefinition(projectile.secondaryVisual);
                preloadDefinition(projectile.tertiaryVisual);
                preloadDefinition(projectile.deathEffect.visual);
                if (projectile.secondaryProjectile.valid)
                    preloadDefinition(
                        projectile.secondaryProjectile
                            .deathEffect.visual);
                if (projectile.tertiaryProjectile.valid)
                    preloadDefinition(
                        projectile.tertiaryProjectile
                            .deathEffect.visual);
            }
        }
        preloadDefinition(originalRace->contactEffect);
        preloadDefinition(originalRace->wheelSmokeEffect);
        for (const auto& path : originalRace->contactSoundPaths)
        {
            valid =
                loadEngineSound(path) !=
                    r3d::audio::invalidSound &&
                valid;
        }
        return valid;
    };
    engineAudioValid = engineAudioValid && preloadEffectAudio();
    rrr3d::audio::OriginalRaceCommentator commentator(
        audio, *resources);
    const bool commentatorValid = commentator.initialize(
        profileState.config.commentatorStyle, audioError);
    if (commentatorValid)
    {
        std::cout << "Original commentator: "
                  << commentator.commentCount()
                  << " serialized comments, "
                  << commentator.loadedVoiceCount()
                  << " available voice files\n";
    }
    engineAudioValid =
        engineAudioValid &&
        commentatorValid;
    if (!engineAudioValid)
    {
        std::cerr << "Original race engine audio loading failed: "
                  << audioError << '\n';
        commentator.shutdown();
        gameMusic.shutdown();
        finalMusic.shutdown();
        music.shutdown();
        for (const auto& [path, sound] : engineSounds)
        {
            static_cast<void>(path);
            audio.unloadSound(sound);
        }
        menuSounds.shutdown();
        audio.shutdown();
        raceHud.shutdown(*device);
        workshopRenderer.shutdown(*device);
        angarRenderer.shutdown(*device);
        garageRenderer.shutdown(*device);
        raceRenderer.shutdown(*device);
        releaseResources();
        device.reset();
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }
    auto startRaceAudio = [&]() {
        audio.setBusVolume(r3d::audio::Bus::Effects,
                           profileState.config.effectsVolume);
        music.pause(true, audioError);
        gameMusic.pause(false, audioError);
        lastGameMusicTrack = gameMusic.currentTrack();
        showOriginalMusicInfo(
            OriginalMusicDialogSource::Game, lastGameMusicTrack);
        commentator.pause(false);
        commentator.reset();
        for (const auto& source : shotEffectAudio)
            audio.stop(source.voice);
        shotEffectAudio.clear();
        for (const auto& source : contactEffectAudio)
            audio.stop(source.voice);
        contactEffectAudio.clear();
        for (const auto& source : timedEffectAudio)
            audio.stop(source.voice);
        timedEffectAudio.clear();
        for (std::size_t racer = 0; racer < engineAudio.size();
             ++racer)
        {
            engineAudio[racer].currentRpm = 0.0F;
            engineAudio[racer].spatialProxyPlaying = true;
            r3d::audio::PlayOptions options;
            options.bus = r3d::audio::Bus::Effects;
            options.loop = true;
            options.volume = racer == 0 ? 0.5F : 0.0F;
            engineAudio[racer].idleVoice = audio.play(
                engineAudio[racer].idle, options, audioError);
            options.volume = racer == 0 ? 0.2F : 0.0F;
            engineAudio[racer].rpmVoice = audio.play(
                engineAudio[racer].rpm, options, audioError);
            const auto& sourceRacer = originalRace->racers[racer];
            const auto& vehicle =
                sourceRacer.hasConfiguredVehicle
                    ? sourceRacer.configuredVehicle
                    : originalRace->vehicles.at(sourceRacer.vehicle);
            wheelSlipVoices[racer].assign(
                vehicle.physics.wheels.size(),
                WheelSlipAudio{});
        }
    };
    auto stopRaceAudio = [&](bool advanceGameTrack = true) {
        for (auto& engine : engineAudio)
        {
            audio.stop(engine.idleVoice);
            audio.stop(engine.rpmVoice);
            engine.idleVoice = r3d::audio::invalidVoice;
            engine.rpmVoice = r3d::audio::invalidVoice;
        }
        for (auto& wheels : wheelSlipVoices)
        {
            for (auto& voice : wheels)
            {
                audio.stop(voice.voice);
                voice = {};
            }
        }
        for (const auto& source : shotEffectAudio)
            audio.stop(source.voice);
        shotEffectAudio.clear();
        for (const auto& source : contactEffectAudio)
            audio.stop(source.voice);
        contactEffectAudio.clear();
        for (const auto& source : timedEffectAudio)
            audio.stop(source.voice);
        timedEffectAudio.clear();
        commentator.stop();
        commentator.pause(true);
        gameMusic.pause(true, audioError);
        if (advanceGameTrack)
            gameMusic.next(audioError);
        music.pause(false, audioError);
    };
#endif
#endif

    PipelineState opaque;
    opaque.faceCulling = PipelineState::FaceCulling::None;
    opaque.writeDepth = false;
    opaque.depthTest = false;
    PipelineState transparent = opaque;
    transparent.alphaBlend = true;
#ifdef RRR3D_PHYSICS
    PipelineState racePipeline;
    // ContextInfo.cpp uses MatrixLookAtRH/MatrixPerspectiveFovRH and the
    // default D3D9 state D3DCULL_CW.  Preserve that state directly; mirrored
    // nodes carry their original cullMode/invertCullFace overrides.
    racePipeline.faceCulling =
        PipelineState::FaceCulling::Clockwise;
#endif
    const Camera camera = makeCamera(*device);

    bool running = true;
    bool runtimeSmokeFailed = false;
    std::uint32_t renderedFrames = 0;
    bool sourceStartupActive = sourceStartupRequested;
    float sourceStartupSeconds = 0.0F;
    bool startupYardFadeObserved = !options->startupSmokeTest;
    bool startupYardHoldObserved = !options->startupSmokeTest;
    bool startupLabFadeObserved = !options->startupSmokeTest;
    bool startupLabHoldObserved = !options->startupSmokeTest;
    bool startupInitialBlankObserved = !options->startupSmokeTest;
    bool startupInterlogoBlankObserved = !options->startupSmokeTest;
    bool startupLoadFrameObserved = !options->startupSmokeTest;
    bool startupMenuTransitionObserved = !options->startupSmokeTest;
    bool startupEscapeQueued = false;
    bool startupEscapeObserved = !options->startupSmokeTest;
#ifdef RRR3D_PHYSICS
    bool sourceStartOptionsActive =
        !sourceStartupRequested && sourcePreferredCameraAutodetect;
    if (sourceStartOptionsActive)
        sourcePreferredCameraAutodetect = false;
    const auto startOptionsConfigBefore = profileState.config;
    bool startOptionsFrameObserved =
        !options->startOptionsSmokeTest;
    bool startOptionsSelectGateObserved =
        !options->startOptionsSmokeTest;
    bool startOptionsAllRowsObserved =
        !options->startOptionsSmokeTest;
    bool startOptionsCameraAppliedObserved =
        !options->startOptionsSmokeTest;
    bool startOptionsSavedObserved =
        !options->startOptionsSmokeTest;
    bool startOptionsMainTransitionObserved =
        !options->startOptionsSmokeTest;
    bool startOptionsReloadDialogPending = false;
    std::uint32_t startOptionsSmokeStep = 0U;
    std::uint32_t startOptionsSmokeNextFrame = 1U;
#endif
#ifdef RRR3D_NETWORK
    r3d::game::originalnetwork::OriginalNetworkSession networkSession;
    r3d::game::originalnetwork::SessionSnapshot networkSnapshot;
    std::uint64_t renderedNetworkRevision =
        std::numeric_limits<std::uint64_t>::max();
    std::uint64_t handledNetworkFailureRevision = 0U;
    enum class NetworkFailureDialogAction
    {
        None,
        ExitMatch,
    };
    NetworkFailureDialogAction networkFailureDialogAction =
        NetworkFailureDialogAction::None;
    std::string networkIpInput = "_";
    bool networkHostRequested = false;
    bool networkMatchStarted = false;
    bool networkRaceStarted = false;
    bool networkClientMatchEntered = false;
    bool networkLocalCarSelected = true;
    float networkHostRaceGoSeconds = -1.0F;
    std::int32_t networkAppliedRaceGoStage = -1;
    bool networkLocalReadyPublished = false;
    bool networkLocalGoWaitPublished = false;
    bool networkLocalFinishPublished = false;
    bool networkHostFinishTimerStarted = false;
    bool networkRaceExitApplied = false;
    std::uint64_t networkLastGameplayEventSequence = 0U;
    std::uint64_t networkLastShotEventSequence = 0U;
    std::uint64_t networkLastBonusEventSequence = 0U;
    std::uint64_t networkLastMineEventSequence = 0U;
    std::uint64_t networkLastChatEventSequence = 0U;
    std::uint64_t networkLastIdentityEventSequence = 0U;
    std::uint64_t networkLastLifecycleEventSequence = 0U;
    std::int32_t networkAppliedPlanet =
        std::numeric_limits<std::int32_t>::min();
    std::int32_t networkAppliedTrack =
        std::numeric_limits<std::int32_t>::min();
    std::int32_t networkAppliedWeather =
        std::numeric_limits<std::int32_t>::min();
    std::vector<std::uint32_t> networkRaceModelOrder;
    std::map<std::uint32_t, std::uint64_t>
        networkAppliedVehicleRevisions;
    std::optional<r3d::game::originalnetwork::NetworkPlayerState>
        networkPublishedPlayer;
#ifdef RRR3D_PHYSICS
    std::optional<r3d::game::originalrace::Weather>
        networkWeatherOverride;
    std::optional<std::int32_t> networkPendingGamerId;
    bool networkLeaverStartDialogVisible = false;
    bool networkLeaverStartYesFocused = true;
    std::optional<std::uint32_t> networkKickHoverOwner;
#endif
    bool networkFrameObserved = !options->networkMenuSmokeTest;
    bool networkServerTypeObserved = !options->networkMenuSmokeTest;
    bool networkClientTypeObserved = !options->networkMenuSmokeTest;
    bool networkBrowserObserved = !options->networkMenuSmokeTest;
    bool networkIpObserved = !options->networkMenuSmokeTest;
    bool networkHostReadyGateObserved = !options->networkMenuSmokeTest;
    bool networkFailureDialogObserved = !options->networkMenuSmokeTest;
    std::uint32_t networkSmokeStep = 0U;
    std::uint32_t networkSmokeNextFrame = 1U;

    auto replaceNetworkAuxPage = [&](MenuPageVisual& page,
                                     std::vector<std::string> lines,
                                     float pointSize) {
        if (lines.empty())
            lines.emplace_back(" ");
        for (auto& line : lines)
        {
            if (line.empty())
                line = " ";
        }
        auto replacement = createStyledPage(
            std::move(lines), pointSize,
            menu::Rgba8{214, 214, 214, 255},
            menu::selectedTextColor);
        destroyPage(page);
        page = std::move(replacement);
    };
    auto refreshNetworkAddressPage = [&]() {
        std::vector<std::string> lines;
        if (!networkSnapshot.adapterAddresses.empty())
        {
            lines.emplace_back("My IP:");
            const auto count = std::min<std::size_t>(
                networkSnapshot.adapterAddresses.size(), 6U);
            lines.insert(
                lines.end(), networkSnapshot.adapterAddresses.begin(),
                networkSnapshot.adapterAddresses.begin() +
                    static_cast<std::ptrdiff_t>(count));
        }
        replaceNetworkAuxPage(
            networkAddressInfoPage, std::move(lines),
            menu::smallFontHeight);
    };
    auto refreshNetworkIpPage = [&]() {
        replaceNetworkAuxPage(
            networkIpValuePage, {networkIpInput},
            menu::headerFontHeight);
    };
    auto refreshNetworkRuntimePages = [&]() {
        networkSnapshot = networkSession.snapshot();
        if (networkSnapshot.revision == renderedNetworkRevision)
            return;
        renderedNetworkRevision = networkSnapshot.revision;
        refreshNetworkAddressPage();

        std::vector<std::string> hostLabels;
        hostLabels.reserve(
            networkSnapshot.discoveredHosts.size() + 1U);
        for (const auto& endpoint : networkSnapshot.discoveredHosts)
            hostLabels.push_back(endpoint.address);
        hostLabels.push_back(localized("svBack"));
        auto browserReplacement = createPage(std::move(hostLabels));
        destroyPage(networkBrowserPage);
        networkBrowserPage = std::move(browserReplacement);

        std::string status;
        using State = r3d::game::originalnetwork::SessionState;
        switch (networkSnapshot.state)
        {
        case State::Searching:
            status = localized("svHintRefreshing");
            break;
        case State::Connecting:
            status = localized("svHintConnecting");
            break;
        case State::Connected:
            // MainMenu::OnConnectedPlayer leaves the source loading hint
            // visible until the replicated NetPlayer model exists.
            status = localized("svHintConnecting");
            break;
        case State::Hosting:
            break;
        case State::Failed:
            status = localized("svHintHostConnectionFailed");
            if (!networkSnapshot.lastErrorMessage.empty())
                status += ": " + networkSnapshot.lastErrorMessage;
            break;
        case State::Idle:
            if (networkSnapshot.discoveredHosts.empty())
                status = localized("svHintHostListEmpty");
            break;
        case State::Stopped:
            break;
        }
        replaceNetworkAuxPage(
            networkStatusPage, {std::move(status)},
            menu::smallFontHeight);
    };
    auto initializeNetwork = [&]() {
        std::vector<std::int32_t> gamerIds;
        gamerIds.reserve(originalGarage->gamers.size());
        for (const auto& gamer : originalGarage->gamers)
            gamerIds.push_back(static_cast<std::int32_t>(gamer.bossId));
        networkSession.setGamerCatalog(std::move(gamerIds));
        std::string error;
        if (!networkSession.initialize(error))
        {
            std::cerr << "Original NetGame initialization failed: "
                      << error << '\n';
            return false;
        }
        renderedNetworkRevision =
            std::numeric_limits<std::uint64_t>::max();
        handledNetworkFailureRevision = 0U;
        networkFailureDialogAction =
            NetworkFailureDialogAction::None;
        refreshNetworkRuntimePages();
        std::cout << "Original NetGame initialized: port "
                  << r3d::game::originalnetwork::defaultPort
                  << ", adapters="
                  << networkSnapshot.adapterAddresses.size() << '\n';
        return true;
    };
#endif
    enum class MenuScreen
    {
        Main,
        GameMode,
        Tournament,
        Difficulty,
        Profiles,
        Network,
#ifdef RRR3D_NETWORK
        NetworkServerType,
        NetworkClientType,
        NetworkBrowser,
        NetworkIpAddress,
#endif
        Options,
        Credits,
#ifdef RRR3D_PHYSICS
        Gamers,
        RaceMenu,
        Garage,
        Workshop,
        Planets,
        Achievements,
        GameOptions,
        GraphicsOptions,
        SoundOptions,
        ControlsOptions,
        Finish,
#endif
    };
    std::vector<MenuScreen> menuStack{MenuScreen::Main};
    std::size_t menuSelection = 0;
    bool championshipMode = true;
    bool newTournamentProfile = false;
    std::uint64_t previousFrameTicks = SDL_GetTicksNS();
    float finalMenuSeconds = 0.0F;
    std::array<bool, 9> finalSlidesObserved{};
    bool finalCreditsMotionObserved =
        !options->finalMenuSmokeTest;
    bool finalBackFrameObserved =
        !options->finalMenuSmokeTest;
    bool finalAutoCloseObserved =
        !options->finalMenuSmokeTest;
#ifdef RRR3D_AUDIO
    bool finalMusicPlaybackObserved =
        !options->finalMenuSmokeTest;
#endif
#ifdef RRR3D_PHYSICS
    enum class GamersFocus
    {
        Next,
        Left,
        Right,
    };
    GamersFocus gamersFocus = GamersFocus::Next;
    std::size_t gamerPlanetIndex = 0U;
    float gamersSceneSeconds = 0.0F;
    bool gamersFrameObserved = !options->gamersFrameSmokeTest;
    bool gamersPlanet3DObserved = !options->gamersFrameSmokeTest;
    bool gamersSelectionChangedObserved =
        !options->gamersFrameSmokeTest;
    bool gamersGarageObserved = !options->gamersFrameSmokeTest;
    std::uint32_t gamersSmokeStep = 0U;
    std::uint32_t gamersSmokeNextFrame = 0U;
    std::optional<r3d::game::originalrace::PlayerProfile>
        championshipPlayerBeforeSkirmish;
    enum class ProfileFocus
    {
        Item,
        Close,
        Up,
        Down,
        Back
    };
    ProfileFocus profileFocus = ProfileFocus::Back;
    std::size_t profileFocusIndex = 0U;
    std::size_t profileGridScroll = 0U;
    bool profileDeleteDialogVisible = false;
    bool profileDeleteYesFocused = true;
    std::size_t profileDeleteIndex =
        std::numeric_limits<std::size_t>::max();
    struct GarageCarView
    {
        std::size_t catalogIndex = 0U;
        bool locked = false;
    };
    std::vector<GarageCarView> garageCarOrder;
    std::size_t garageCarIndex = 0;
    std::size_t garageViewIndex = 0;
    bool garagePurchaseDialogVisible = false;
    bool garagePurchaseYesFocused = true;
    for (std::size_t index = 0;
         index < originalGarage->cars.size(); ++index)
    {
        if (originalGarage->cars[index].record ==
            profileState.player.currentCar)
        {
            garageCarIndex = index;
            break;
        }
    }
    struct WorkshopDrag
    {
        r3d::game::originalrace::ProfileSlot item;
        std::optional<r3d::game::originalrace::GarageSlotType> origin;

        bool active() const noexcept
        {
            return !item.record.empty();
        }
    };
    WorkshopDrag workshopDrag;
    enum class WorkshopConfirmation
    {
        None,
        Buy,
        Sell
    };
    WorkshopConfirmation workshopConfirmation =
        WorkshopConfirmation::None;
    const r3d::game::originalrace::OriginalWorkshopItem*
        workshopPendingPurchase = nullptr;
    bool workshopConfirmationYesFocused = true;
    float workshopDragX = menu::virtualWidth * 0.5F;
    float workshopDragY = menu::virtualHeight * 0.5F;
    std::vector<const r3d::game::originalrace::OriginalWorkshopItem*>
        workshopGoods;
    std::size_t workshopGoodScroll = 0U;
    bool achievementPurchaseDialogVisible = false;
    bool achievementPurchaseYesFocused = true;
    std::size_t achievementPendingPurchase =
        originalAchievementNoTarget;
#endif
    auto activeMenuPage = [&]() -> MenuPageVisual& {
        switch (menuStack.back())
        {
        case MenuScreen::Main:
            return mainPage;
        case MenuScreen::GameMode:
            return gameModePage;
        case MenuScreen::Tournament:
            return tournamentPage;
        case MenuScreen::Difficulty:
            return difficultyPage;
        case MenuScreen::Profiles:
            return profilePage;
        case MenuScreen::Network:
            return networkPage;
#ifdef RRR3D_NETWORK
        case MenuScreen::NetworkServerType:
            return networkServerTypePage;
        case MenuScreen::NetworkClientType:
            return networkClientTypePage;
        case MenuScreen::NetworkBrowser:
            return networkBrowserPage;
        case MenuScreen::NetworkIpAddress:
            return networkIpPage;
#endif
        case MenuScreen::Options:
            return optionsPage;
        case MenuScreen::Credits:
            return creditsPage;
#ifdef RRR3D_PHYSICS
        case MenuScreen::Gamers:
            // GamersFrame owns a three-widget navigation graph and is
            // rendered from the source layout rather than this list page.
            return gamersNamePage;
        case MenuScreen::RaceMenu:
            return raceMenuPage;
        case MenuScreen::Garage:
            return garagePage;
        case MenuScreen::Workshop:
            return workshopPage;
        case MenuScreen::Planets:
            return planetsPage;
        case MenuScreen::Achievements:
            return achievementsPage;
        case MenuScreen::GameOptions:
            return gameOptionsPage;
        case MenuScreen::GraphicsOptions:
            return graphicsOptionsPage;
        case MenuScreen::SoundOptions:
            return soundOptionsPage;
        case MenuScreen::ControlsOptions:
            return controlsOptionsPage;
        case MenuScreen::Finish:
            // FinishMenu has no selectable widgets.  Its original
            // ControlEvent closes the frame on Action/Escape or any left
            // click, so the shared page is never drawn or navigated.
            return mainPage;
#endif
        }
        return mainPage;
    };
    auto refreshProfilePage = [&]() {
#ifdef RRR3D_PHYSICS
        auto profileLabels = profileState.profiles;
        const auto maximumScroll =
            profileLabels.size() > 4U
                ? profileLabels.size() - 4U
                : 0U;
        profileGridScroll =
            std::min(profileGridScroll, maximumScroll);
        if ((profileFocus == ProfileFocus::Item ||
             profileFocus == ProfileFocus::Close) &&
            profileFocusIndex >= profileLabels.size())
        {
            profileFocus = ProfileFocus::Back;
            profileFocusIndex = 0U;
        }
#else
        std::vector<std::string> profileLabels{"profile1"};
#endif
        profileLabels.push_back(localized("svBack"));
        auto replacement = createPage(std::move(profileLabels));
        destroyPage(profilePage);
        profilePage = std::move(replacement);
    };
#ifdef RRR3D_PHYSICS
    auto refreshRaceMainPages = [&]() {
        auto infoReplacement = createStyledPage(
            raceMainInfoLabels(), menu::smallFontHeight,
            raceInfoColor, menu::selectedTextColor);
        auto statsReplacement = createStyledPage(
            raceMainStatsLabels(), menu::smallFontHeight,
            raceTextColor, menu::selectedTextColor);
        destroyPage(raceMainInfoPage);
        destroyPage(raceMainStatsPage);
        raceMainInfoPage = std::move(infoReplacement);
        raceMainStatsPage = std::move(statsReplacement);
    };
#endif
    auto refreshSharedMenuAvailability = [&](MenuScreen screen) {
        auto enableAll = [](MenuPageVisual& page) {
            std::fill(
                page.enabled.begin(), page.enabled.end(), true);
        };
        switch (screen)
        {
        case MenuScreen::GameMode:
            enableAll(gameModePage);
#ifdef RRR3D_PHYSICS
            gameModePage.enabled[1] =
                profileState.tutorialStage >= 1U;
#endif
            break;
        case MenuScreen::Tournament:
            enableAll(tournamentPage);
#ifdef RRR3D_PHYSICS
            tournamentPage.enabled[0] =
                !profileState.profiles.empty();
            tournamentPage.enabled[2] =
                !profileState.profiles.empty();
#endif
            break;
        default:
            break;
        }
    };
    auto firstEnabledMenuItem = [&]() {
        const auto& page = activeMenuPage();
        const auto first = std::find(
            page.enabled.begin(), page.enabled.end(), true);
        return first == page.enabled.end()
                   ? std::size_t{0}
                   : static_cast<std::size_t>(
                         std::distance(page.enabled.begin(), first));
    };
    auto usesSharedBackPosition = [](MenuScreen screen) {
        switch (screen)
        {
        case MenuScreen::GameMode:
        case MenuScreen::Tournament:
        case MenuScreen::Difficulty:
        case MenuScreen::Profiles:
        case MenuScreen::Network:
#ifdef RRR3D_NETWORK
        case MenuScreen::NetworkServerType:
        case MenuScreen::NetworkClientType:
        case MenuScreen::NetworkBrowser:
        case MenuScreen::NetworkIpAddress:
#endif
        case MenuScreen::Credits:
            return true;
        default:
            return false;
        }
    };
    auto sharedMenuItemY =
        [&](MenuScreen screen, std::size_t index,
            std::size_t count) {
            if (usesSharedBackPosition(screen) &&
                index + 1U == count)
            {
                return menu::virtualHeight * 0.5F + 150.0F;
            }
            return menu::virtualHeight * 0.5F +
                   menu::firstItemOffsetY +
                   static_cast<float>(index) *
                       menu::itemSpacing;
        };
    auto pushMenu = [&](MenuScreen screen) {
        if (screen == MenuScreen::Profiles)
        {
#ifdef RRR3D_PHYSICS
            profileGridScroll = 0U;
            profileFocus = ProfileFocus::Back;
            profileFocusIndex = 0U;
            profileDeleteDialogVisible = false;
            profileDeleteIndex =
                std::numeric_limits<std::size_t>::max();
#endif
            refreshProfilePage();
        }
        menuStack.push_back(screen);
        refreshSharedMenuAvailability(screen);
        menuSelection = firstEnabledMenuItem();
    };
    auto backMenu = [&]() {
#ifdef RRR3D_NETWORK
        const auto leavingScreen = menuStack.back();
        if (leavingScreen == MenuScreen::NetworkBrowser)
        {
            networkSession.cancelLanSearch();
            networkSession.close();
        }
        else if (leavingScreen == MenuScreen::NetworkIpAddress)
        {
            SDL_StopTextInput(window);
            networkSession.close();
        }
#endif
        if (menuStack.size() > 1U)
            menuStack.pop_back();
#ifdef RRR3D_NETWORK
        if (leavingScreen == MenuScreen::Network)
        {
            networkHostRequested = false;
            networkMatchStarted = false;
            networkRaceStarted = false;
            networkClientMatchEntered = false;
            networkLocalCarSelected = true;
            networkHostRaceGoSeconds = -1.0F;
            networkAppliedRaceGoStage = -1;
            networkLocalReadyPublished = false;
            networkLocalGoWaitPublished = false;
            networkLocalFinishPublished = false;
            networkHostFinishTimerStarted = false;
            networkRaceExitApplied = false;
            networkLastGameplayEventSequence = 0U;
            networkLastShotEventSequence = 0U;
            networkLastBonusEventSequence = 0U;
            networkLastMineEventSequence = 0U;
            networkLastChatEventSequence = 0U;
            networkLastIdentityEventSequence = 0U;
            networkLastLifecycleEventSequence = 0U;
            networkPublishedPlayer.reset();
            networkRaceModelOrder.clear();
            networkAppliedVehicleRevisions.clear();
#ifdef RRR3D_PHYSICS
            networkWeatherOverride.reset();
            networkPendingGamerId.reset();
#endif
            networkSession.finalize();
            networkSnapshot = {};
            renderedNetworkRevision =
                std::numeric_limits<std::uint64_t>::max();
            handledNetworkFailureRevision = 0U;
            networkFailureDialogAction =
                NetworkFailureDialogAction::None;
        }
#endif
#ifdef RRR3D_PHYSICS
        if (menuStack.back() == MenuScreen::RaceMenu)
            refreshRaceMainPages();
#endif
        refreshSharedMenuAvailability(menuStack.back());
        menuSelection = firstEnabledMenuItem();
    };
    auto showOriginalFinalMenu = [&]() {
        finalMenuSeconds = 0.0F;
        finalSlidesObserved.fill(false);
        finalCreditsMotionObserved =
            !options->finalMenuSmokeTest;
        finalBackFrameObserved =
            !options->finalMenuSmokeTest;
        finalAutoCloseObserved =
            !options->finalMenuSmokeTest;
#ifdef RRR3D_AUDIO
        finalMusicPlaybackObserved =
            !options->finalMenuSmokeTest;
        bool audioReady = music.pause(true, audioError) &&
                          finalMusic.pause(true, audioError);
        if (audioReady)
        {
            const auto track = finalMusic.currentTrack();
            if (track != std::nullopt &&
                finalMusic.trackInfo(*track) != nullptr)
            {
                audioReady =
                    finalMusic.seekCurrent(0U, audioError);
            }
        }
        if (audioReady)
            audioReady = finalMusic.pause(false, audioError);
        if (!audioReady)
        {
            std::cerr << "Original FinalMenu::OnShow music failed: "
                      << audioError << '\n';
            runtimeSmokeFailed = true;
            running = false;
        }
#endif
        menuStack = {MenuScreen::Main, MenuScreen::Credits};
        menuSelection = 0U;
        std::cout
            << "Original FinalMenu::OnShow: 9 slides, 107 seconds, "
               "source credits and TrackFinal.ogg\n";
    };
    auto closeOriginalFinalMenu = [&]() {
        if (menuStack.back() != MenuScreen::Credits)
            return;
#ifdef RRR3D_AUDIO
        if (!finalMusic.pause(true, audioError) ||
            !music.pause(false, audioError))
        {
            std::cerr << "Original FinalMenu::OnShow(false) music failed: "
                      << audioError << '\n';
            runtimeSmokeFailed = true;
            running = false;
        }
#endif
        menuStack = {MenuScreen::Main};
        menuSelection = 0U;
        finalMenuSeconds = 0.0F;
        std::cout << "Original FinalMenu -> MainMenu2\n";
    };
#ifdef RRR3D_VIDEO
    enum class OriginalMovieCompletion
    {
        None,
        RaceMenu,
        GamersGarage,
        FinalMenu,
        TournamentStart,
    };
    std::function<void()> originalMovieStartMatch;
    std::function<void()> originalMovieGamersGarage;
    OriginalMovieCompletion originalMovieCompletion =
        OriginalMovieCompletion::None;
    bool originalMovieActive = false;
    bool videoFrameObserved = !options->videoSmokeTest;
    bool videoAudioObserved = !options->videoSmokeTest;
    bool videoCompletionObserved = !options->videoSmokeTest;
    bool videoTournamentStartObserved = !options->videoSmokeTest;
    bool videoSmokeSeeked = false;
    const std::uint64_t videoSmokeDeadline =
        options->videoSmokeTest ? SDL_GetTicks() + 15000U : 0U;
    auto finishOriginalMovie = [&]() {
        if (!originalMovieActive)
            return;
        const auto completion = originalMovieCompletion;
        videoPlayer.stop();
        originalMovieActive = false;
        originalMovieCompletion = OriginalMovieCompletion::None;
#ifdef RRR3D_AUDIO
        if (!music.pause(false, audioError))
        {
            std::cerr
                << "Original movie menu-music resume failed: "
                << audioError << '\n';
            runtimeSmokeFailed = true;
            running = false;
        }
#endif
        switch (completion)
        {
        case OriginalMovieCompletion::RaceMenu:
            if (menuStack.size() > 1U &&
                menuStack.back() == MenuScreen::Planets)
            {
                backMenu();
            }
            break;
        case OriginalMovieCompletion::GamersGarage:
            if (originalMovieGamersGarage)
                originalMovieGamersGarage();
            break;
        case OriginalMovieCompletion::FinalMenu:
            showOriginalFinalMenu();
            break;
        case OriginalMovieCompletion::TournamentStart:
            if (originalMovieStartMatch)
            {
                const auto startMatch = originalMovieStartMatch;
                originalMovieStartMatch = {};
                startMatch();
            }
            break;
        case OriginalMovieCompletion::None:
            break;
        }
    };
    auto playOriginalMovie =
        [&](std::string_view sourceMovie,
            OriginalMovieCompletion completion) {
            const auto cache =
                movieCachePath(resources->root(), sourceMovie);
#ifdef RRR3D_AUDIO
            if (!music.pause(true, audioError))
            {
                std::cerr
                    << "Original movie menu-music pause failed: "
                    << audioError << '\n';
                runtimeSmokeFailed = true;
                return false;
            }
#endif
            std::string videoError;
            if (!videoPlayer.play(
                    cache,
                    options->videoSmokeTest ? 0.0F : 1.0F,
                    videoError))
            {
                std::cerr << "Original movie playback failed for "
                          << sourceMovie << ": " << videoError << '\n';
#ifdef RRR3D_AUDIO
                music.pause(false, audioError);
#endif
                runtimeSmokeFailed = true;
                return false;
            }
            originalMovieActive = true;
            originalMovieCompletion = completion;
            std::cout << "Original GameMode::PlayMovie: "
                      << sourceMovie << '\n';
            return true;
        };
    if (options->videoSmokeTest)
    {
        originalMovieStartMatch = [&]() {
            videoCompletionObserved = true;
            videoTournamentStartObserved = true;
        };
        if (!playOriginalMovie(
                "Data/Video/Main_eng.avi",
                OriginalMovieCompletion::TournamentStart))
        {
            running = false;
        }
    }
#endif
#ifdef RRR3D_PHYSICS
    std::optional<std::string> bindingCaptureAction;
    bool bindingCaptureGamepad = false;
    auto originalKeyName = [](SDL_Scancode scancode) {
        switch (scancode)
        {
        case SDL_SCANCODE_UP:
            return std::string("Up Arrow");
        case SDL_SCANCODE_DOWN:
            return std::string("Down Arrow");
        case SDL_SCANCODE_LEFT:
            return std::string("Left Arrow");
        case SDL_SCANCODE_RIGHT:
            return std::string("Right Arrow");
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER:
            return std::string("Enter");
        default:
            return std::string(SDL_GetScancodeName(scancode));
        }
    };
    auto originalGamepadButtonName = [](SDL_GamepadButton button)
        -> std::optional<std::string> {
        switch (button)
        {
        case SDL_GAMEPAD_BUTTON_SOUTH:
            return "A";
        case SDL_GAMEPAD_BUTTON_EAST:
            return "B";
        case SDL_GAMEPAD_BUTTON_WEST:
            return "X";
        case SDL_GAMEPAD_BUTTON_NORTH:
            return "Y";
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
            return "DPad Up";
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
            return "DPad Down";
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            return "DPad Left";
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            return "DPad Right";
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
            return "Left Shoulder";
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
            return "Right Shoulder";
        case SDL_GAMEPAD_BUTTON_LEFT_STICK:
            return "L.Thumb Press";
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
            return "R.Thumb Press";
        case SDL_GAMEPAD_BUTTON_BACK:
            return "Back";
        case SDL_GAMEPAD_BUTTON_START:
            return "Start";
        default:
            return std::nullopt;
        }
    };
    auto originalGamepadAxisName = [](SDL_GamepadAxis axis, Sint16 value)
        -> std::optional<std::string> {
        if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER && value > 15000)
            return "Left Trigger";
        if (axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER && value > 15000)
            return "Right Trigger";
        if (std::abs(static_cast<int>(value)) < 20000)
            return std::nullopt;
        switch (axis)
        {
        case SDL_GAMEPAD_AXIS_LEFTX:
            return value < 0 ? "L.Thumb Left" : "L.Thumb Right";
        case SDL_GAMEPAD_AXIS_LEFTY:
            return value < 0 ? "L.Thumb Up" : "L.Thumb Down";
        case SDL_GAMEPAD_AXIS_RIGHTX:
            return value < 0 ? "R.Thumb Left" : "R.Thumb Right";
        case SDL_GAMEPAD_AXIS_RIGHTY:
            return value < 0 ? "R.Thumb Up" : "R.Thumb Down";
        default:
            return std::nullopt;
        }
    };
#endif
#ifdef RRR3D_PHYSICS
    bool inRace = false;
    bool raceLoadingActive = false;
    std::uint32_t raceLoadingPresentedFrames = 0U;
    bool exitRaceDialogVisible = false;
    bool exitRaceYesFocused = true;
    r3d::physics::VehicleInput raceInput;
    bool raceUseWeaponRequested = false;
    bool raceUseAllWeaponsRequested = false;
    bool raceUseMine = false;
    bool raceUseHyper = false;
    bool raceChangeWeaponRequested = false;
    int raceWeaponChangeDirection = 1;
    int raceFireWeaponSlotRequested = -1;
    bool raceResetRequested = false;
    std::vector<r3d::physics::VehicleState> raceVehicles(
        physicsWorld->vehicleCount());
    struct DecorationDebrisBinding
    {
        std::size_t instance = 0;
        std::size_t piece = 0;
        std::size_t debris = 0;
    };
    std::vector<DecorationDebrisBinding> decorationDebrisBindings;
    std::vector<r3d::game::originalrace::DecorationFragmentState>
        decorationFragments;
    struct VehicleDebrisBinding
    {
        std::size_t racer = 0;
        std::size_t effect = 0;
        std::size_t debris = 0;
    };
    std::vector<VehicleDebrisBinding> vehicleDebrisBindings;
    std::vector<r3d::game::originalrace::VehicleDeathFragmentState>
        vehicleDeathFragments;
    for (std::size_t index = 0; index < physicsWorld->vehicleCount();
         ++index)
        raceVehicles[index] = physicsWorld->vehicle(index);
    float raceElapsedSeconds = 0.0F;
    bool integratedRaceStartObserved = !options->raceRenderSmokeTest;
    bool raceLoadingFrameObserved = !options->raceRenderSmokeTest;
    bool raceLoadingDeferredObserved = !options->raceRenderSmokeTest;
    bool gameModeFrameObserved = !options->raceRenderSmokeTest;
    bool tournamentFrameObserved = !options->raceRenderSmokeTest;
    bool profileFrameObserved = !options->raceRenderSmokeTest;
    bool profileDeleteDialogObserved =
        !options->raceRenderSmokeTest;
    bool raceMainFrameObserved = !options->raceRenderSmokeTest;
    bool raceMain3DObserved = !options->raceRenderSmokeTest;
    bool raceMainSourceVisualsObserved =
        !options->raceRenderSmokeTest;
    bool raceGarageFrameObserved = !options->raceRenderSmokeTest;
    bool raceGarage3DObserved = !options->raceRenderSmokeTest;
    bool raceWorkshopFrameObserved = !options->raceRenderSmokeTest;
    bool raceWorkshop3DObserved = !options->raceRenderSmokeTest;
    bool raceWorkshopWeaponDialogObserved =
        !options->raceRenderSmokeTest;
    bool raceWorkshopWeaponDialogMotionQueued = false;
    bool raceInfoDialogObserved = !options->raceRenderSmokeTest;
    bool raceInfoDialogSmokeShown = false;
    bool raceInfoDialogCloseQueued = false;
    bool raceAngarFrameObserved = !options->raceRenderSmokeTest;
    bool raceAngar3DObserved = !options->raceRenderSmokeTest;
    bool raceAchievementFrameObserved =
        !options->raceRenderSmokeTest;
    bool racePauseDialogObserved = !options->raceRenderSmokeTest;
    bool racePauseResumeObserved = !options->raceRenderSmokeTest;
    bool racePauseFrozenObserved = !options->raceRenderSmokeTest;
    bool raceChatInputObserved = !options->raceRenderSmokeTest;
    bool raceChatLineObserved = !options->raceRenderSmokeTest;
    bool racePlayerDestroyedObserved = false;
    float minimumRacePlayerLife =
        std::numeric_limits<float>::max();
    std::uint32_t racePauseSmokeStep = 0U;
    float racePauseElapsedSnapshot = -1.0F;
    r3d::physics::Vec3 racePausePositionSnapshot;
    float maximumRaceSmokeSpeed = 0.0F;
    std::vector<float> maximumRaceAiSpeeds(raceVehicles.size(), 0.0F);
    std::vector<float> maximumRaceAiProgress(raceVehicles.size(), 0.0F);
    std::vector<std::uint32_t> raceAiThrottleFrames(raceVehicles.size(), 0U);
    std::vector<std::uint32_t> raceAiBrakeFrames(raceVehicles.size(), 0U);
    std::vector<std::uint32_t> raceAiReverseFrames(raceVehicles.size(), 0U);
    std::uint32_t maximumRaceSmokeContacts = 0;
    std::array<std::uint32_t, r3d::renderer::renderPassCount>
        maximumRacePassBegins{};
    std::array<std::uint32_t, r3d::renderer::renderPassCount>
        maximumRacePassDraws{};
    std::array<std::uint32_t, 7> maximumRaceLightingDraws{};
    std::uint32_t maximumEnvironmentMappedDraws = 0;
    std::uint32_t maximumNormalMappedDraws = 0;
    std::uint32_t maximumTransientDraws = 0;
    std::uint32_t maximumActiveSpotLights = 0;
    std::array<bool, 2> raceCameraStylesObserved{};
    bool raceProgressSaved = false;
    bool finishMenuShown = false;
    bool finishMenuFrameObserved =
        !options->finishMenuSmokeTest;
    float finishAnimationSeconds = 0.0F;
    std::size_t finishVoiceIndex = 0U;
    bool finishLastVoiceDispatched = false;
    std::uint32_t raceSmokeMenuStep = 0;
    std::uint32_t raceSmokeNextMenuFrame = 0;
    bool raceSmokeAccelerateQueued = false;
    std::uint32_t raceChatSmokeStep = 0U;
    std::uint32_t raceChatSmokeNextFrame = 0U;
    r3d::game::originalrace::TournamentAdvance
        raceTournamentAdvance;
    bool racePlanetChampion = false;
    int angarPlanetIndex = -1;
    int angarPreviousPlanetIndex = -1;
    float angarDoorTime = -1.0F;
    float angarSceneSeconds = 0.0F;
    bool angarTravelDialogVisible = false;
    bool angarTravelYesFocused = true;
    std::size_t angarTravelTarget = 0U;
    auto acceptDialogVisible = [&]() {
        return exitRaceDialogVisible ||
#ifdef RRR3D_NETWORK
               networkLeaverStartDialogVisible ||
#endif
               profileDeleteDialogVisible ||
               garagePurchaseDialogVisible ||
               workshopConfirmation !=
                   WorkshopConfirmation::None ||
               angarTravelDialogVisible ||
               achievementPurchaseDialogVisible ||
               bindingCaptureAction.has_value();
    };
    auto acceptDialogYesFocused = [&]() {
        if (exitRaceDialogVisible)
            return exitRaceYesFocused;
#ifdef RRR3D_NETWORK
        if (networkLeaverStartDialogVisible)
            return networkLeaverStartYesFocused;
#endif
        if (profileDeleteDialogVisible)
            return profileDeleteYesFocused;
        if (garagePurchaseDialogVisible)
            return garagePurchaseYesFocused;
        if (workshopConfirmation != WorkshopConfirmation::None)
            return workshopConfirmationYesFocused;
        if (angarTravelDialogVisible)
            return angarTravelYesFocused;
        if (achievementPurchaseDialogVisible)
            return achievementPurchaseYesFocused;
        return false;
    };
    auto setAcceptDialogFocus = [&](bool yes) {
        acceptDialog.hoveredChoice = yes;
        if (exitRaceDialogVisible)
            exitRaceYesFocused = yes;
#ifdef RRR3D_NETWORK
        else if (networkLeaverStartDialogVisible)
            networkLeaverStartYesFocused = yes;
#endif
        else if (profileDeleteDialogVisible)
            profileDeleteYesFocused = yes;
        else if (garagePurchaseDialogVisible)
            garagePurchaseYesFocused = yes;
        else if (
            workshopConfirmation != WorkshopConfirmation::None)
            workshopConfirmationYesFocused = yes;
        else if (angarTravelDialogVisible)
            angarTravelYesFocused = yes;
        else if (achievementPurchaseDialogVisible)
            achievementPurchaseYesFocused = yes;
    };
    auto wrapAcceptDialogMessage =
        [&](std::string_view value, float maximumWidth,
            float fontHeight, std::size_t maximumLines) {
            constexpr menu::Rgba8 color{175, 175, 175, 255};
            std::vector<std::string> lines;
            std::istringstream words{std::string(value)};
            std::string line;
            std::string word;
            while (words >> word)
            {
                std::string candidate = line;
                if (!candidate.empty())
                    candidate.push_back(' ');
                candidate += word;
                const auto measured =
                    rrr3d::macos::rasterizeText(
                        candidate, menu::fontFace, fontHeight,
                        false, color);
                if (!line.empty() &&
                    static_cast<float>(measured.width) >
                        maximumWidth)
                {
                    lines.push_back(std::move(line));
                    line = std::move(word);
                    if (lines.size() == maximumLines)
                        break;
                }
                else
                {
                    line = std::move(candidate);
                }
            }
            if (!line.empty() && lines.size() < maximumLines)
                lines.push_back(std::move(line));
            if (lines.empty())
                lines.emplace_back(" ");
            return lines;
        };
#ifdef RRR3D_PHYSICS
    auto chatColor = [](const std::array<float, 4>& value) {
        auto channel = [](float component) {
            return static_cast<std::uint8_t>(std::lround(
                std::clamp(component, 0.0F, 1.0F) * 255.0F));
        };
        return menu::Rgba8{
            channel(value[0]), channel(value[1]),
            channel(value[2]), channel(value[3])};
    };
    auto sourceGamerName = [&](std::int32_t gamerId) {
        if (gamerId < 0)
            return std::string{};
        const auto gamer = std::find_if(
            originalGarage->gamers.begin(),
            originalGarage->gamers.end(),
            [&](const auto& value) {
                return value.bossId ==
                       static_cast<std::uint32_t>(gamerId);
            });
        return gamer == originalGarage->gamers.end()
                   ? std::string{}
                   : localized(gamer->bossName);
    };
#ifdef RRR3D_NETWORK
    auto networkRacePlayerCenter = [&](std::size_t index) {
        const float frameWidth = static_cast<float>(
            networkPlayerFrameImage.width);
        const float frameHeight = static_cast<float>(
            networkPlayerFrameImage.height);
        const float leftSpace =
            menu::virtualHeight -
            static_cast<float>(raceTopPanelImage.height) -
            static_cast<float>(raceBottomPanelImage.height) -
            static_cast<float>(raceStatsImage.height);
        const auto leftCount = std::max<std::size_t>(
            1U, static_cast<std::size_t>(std::floor(
                    leftSpace / (frameHeight + 12.5F))));
        const auto column = std::min<std::size_t>(
            index / leftCount, 1U);
        const auto row = index - leftCount * column;
        return std::array<float, 3>{
            frameWidth * 0.5F + 8.0F +
                static_cast<float>(column) *
                    (menu::virtualWidth - frameWidth - 16.0F),
            static_cast<float>(raceTopPanelImage.height) + 30.0F +
                frameHeight * 0.5F +
                static_cast<float>(row) * (frameHeight + 25.0F),
            column > 0U ? -1.0F : 1.0F};
    };
    auto clearNetworkRacePlayerVisuals = [&]() {
        for (const auto& player : networkRacePlayerVisuals)
        {
            device->destroy(player.readyLabel.texture);
            device->destroy(player.name.texture);
        }
        networkRacePlayerVisuals.clear();
        networkKickHoverOwner.reset();
    };
    auto refreshNetworkRacePlayerVisuals = [&]() {
        const bool clientReady =
            networkMatchStarted && !networkHostRequested &&
            networkLocalReadyPublished;
        std::fill(
            raceMenuPage.enabled.begin(), raceMenuPage.enabled.end(), true);
        if (clientReady && raceMenuPage.enabled.size() > 1U)
        {
            std::fill(
                raceMenuPage.enabled.begin() + 1,
                raceMenuPage.enabled.end(), false);
            if (!menuStack.empty() &&
                menuStack.back() == MenuScreen::RaceMenu)
            {
                menuSelection = 0U;
            }
        }

        std::vector<r3d::game::originalnetwork::NetworkPlayerState>
            opponents;
        if (networkMatchStarted &&
            networkSnapshot.models.matchActive)
        {
            for (const auto& player : networkSnapshot.models.players)
            {
                // NetGame::netOpponents contains remote human Player
                // models, not the locally owned human or host-owned AI.
                if (!player.owner && player.playerId == 0U)
                    opponents.push_back(player);
            }
        }
        const bool unchanged =
            opponents.size() == networkRacePlayerVisuals.size() &&
            std::equal(
                opponents.begin(), opponents.end(),
                networkRacePlayerVisuals.begin(),
                [](const auto& player, const auto& visual) {
                    return player.modelId == visual.player.modelId &&
                           player.ownerId == visual.player.ownerId &&
                           player.netSlot == visual.player.netSlot &&
                           player.gamerId == visual.player.gamerId &&
                           player.car == visual.player.car &&
                           player.raceReady ==
                               visual.player.raceReady;
                });
        if (unchanged)
            return;

        clearNetworkRacePlayerVisuals();
        networkRacePlayerVisuals.reserve(opponents.size());
        for (const auto& player : opponents)
        {
            NetworkRacePlayerVisual visual;
            visual.player = player;
            auto name = sourceGamerName(player.gamerId);
            if (name.empty())
            {
                name = localized("svPlayer");
                if (name == "svPlayer")
                    name = "Player";
                name += " " + std::to_string(player.netSlot);
            }
            visual.name = createText(
                *device, name, 18.0F, false,
                menu::Rgba8{255, 255, 255, 255}, resolvedFont);
            const auto readyLabel =
                player.ownerId ==
                        r3d::game::originalnetwork::serverOwnerId
                    ? localized("svHostLabel")
                    : localized(
                          player.raceReady ? "svReadyRace"
                                           : "svCancelReadyRace");
            visual.readyLabel = createText(
                *device, readyLabel, 18.0F, false,
                menu::Rgba8{255, 255, 255, 255}, resolvedFont);
            const auto gamer = std::find_if(
                originalGarage->gamers.begin(),
                originalGarage->gamers.end(), [&](const auto& value) {
                    return value.bossId ==
                           static_cast<std::uint32_t>(player.gamerId);
                });
            if (gamer != originalGarage->gamers.end())
            {
                visual.photoIndex = static_cast<std::size_t>(
                    std::distance(originalGarage->gamers.begin(), gamer));
            }
            networkRacePlayerVisuals.push_back(std::move(visual));
        }
    };
#endif
    auto sourceLocalChatName = [&]() {
        auto name = sourceGamerName(static_cast<std::int32_t>(
            profileState.player.gamerId));
        if (name.empty() && !originalRace->racers.empty())
            name = localized(originalRace->racers.front().name);
        if (name.empty())
            name = profileState.player.name;
        return name;
    };
    auto wrapChatText = [&](std::string_view value, float maximumWidth) {
        constexpr menu::Rgba8 white{255U, 255U, 255U, 255U};
        std::vector<std::string> lines;
        std::istringstream words{std::string(value)};
        std::string line;
        std::string word;
        while (words >> word)
        {
            std::string candidate = line;
            if (!candidate.empty())
                candidate.push_back(' ');
            candidate += word;
            const auto measured = rrr3d::macos::rasterizeText(
                candidate, menu::fontFace, menu::smallFontHeight,
                false, white);
            if (!line.empty() &&
                static_cast<float>(measured.width) > maximumWidth)
            {
                lines.push_back(std::move(line));
                line = std::move(word);
            }
            else
            {
                line = std::move(candidate);
            }
        }
        if (!line.empty())
            lines.push_back(std::move(line));
        if (lines.empty())
            lines.emplace_back(" ");
        return lines;
    };
    auto refreshUserChatVisual = [&]() {
        if (userChatVisual.revision == userChat.revision())
            return;
        destroyUserChatVisual(*device, userChatVisual);
        userChatVisual.lines.reserve(userChat.lines().size());
        constexpr menu::Rgba8 white{255U, 255U, 255U, 255U};
        for (const auto& line : userChat.lines())
        {
            UserChatLineVisual visual;
            visual.name = createText(
                *device, line.name, menu::smallFontHeight, false,
                chatColor(line.nameColor), resolvedFont);
            const float maximumTextWidth = std::max(
                menu::virtualWidth / 3.0F - visual.name.width,
                menu::smallFontHeight * 4.0F);
            for (const auto& text :
                 wrapChatText(line.text, maximumTextWidth))
            {
                visual.text.push_back(createText(
                    *device, text, menu::smallFontHeight, false,
                    white, resolvedFont));
            }
            userChatVisual.lines.push_back(std::move(visual));
        }
        if (userChat.inputVisible())
        {
            userChatVisual.inputName = createText(
                *device, userChat.inputName(), menu::smallFontHeight,
                false, chatColor(userChat.inputNameColor()),
                resolvedFont);
            if (!userChat.inputText().empty())
            {
                userChatVisual.inputText = createText(
                    *device, userChat.inputText(),
                    menu::smallFontHeight, false, white,
                    resolvedFont);
            }
        }
        userChatVisual.revision = userChat.revision();
    };
    auto drawUserChat = [&]() {
        if (!userChat.visible() ||
            (!inRace &&
             (menuStack.empty() ||
              menuStack.back() != MenuScreen::RaceMenu)))
            return;
        refreshUserChatVisual();
        if (options->raceRenderSmokeTest)
        {
            raceChatInputObserved =
                raceChatInputObserved ||
                (userChat.inputVisible() &&
                 userChat.inputName() ==
                     sourceLocalChatName() + ": " &&
                 valid(userChatVisual.inputName.texture));
            raceChatLineObserved =
                raceChatLineObserved ||
                (!userChat.lines().empty() &&
                 userChat.lines().front().name ==
                     "<" + sourceLocalChatName() &&
                 !userChatVisual.lines.empty() &&
                 valid(userChatVisual.lines.front().name.texture) &&
                 !userChatVisual.lines.front().text.empty() &&
                 valid(userChatVisual.lines.front().text.front().texture));
        }

        const float linesRight = menu::virtualWidth - 10.0F;
        const float linesTop = inRace
                                   ? 320.0F
                                   : static_cast<float>(
                                         raceTopPanelImage.height);
        const float maximumTextWidth = menu::virtualWidth / 3.0F;
        float lineY = linesTop;
        const auto& lines = userChat.lines();
        for (std::size_t index = 0U;
             index < lines.size() &&
             index < userChatVisual.lines.size(); ++index)
        {
            const auto& source = lines[index];
            const auto& visual = userChatVisual.lines[index];
            const float alpha = source.alpha();
            if (alpha <= 0.0F)
                continue;
            const std::array<float, 4> tint{1.0F, 1.0F, 1.0F, alpha};
            drawQuadTinted(
                *device, quad, shader, visual.name.texture,
                visual.name.width, visual.name.height,
                linesRight - visual.name.width * 0.5F,
                lineY + visual.name.height * 0.5F,
                1.0F, transparent, tint);
            float textY = lineY;
            for (const auto& text : visual.text)
            {
                const float available = std::max(
                    maximumTextWidth - visual.name.width,
                    menu::smallFontHeight * 4.0F);
                const float scale = std::min(
                    1.0F, available / std::max(text.width, 1.0F));
                drawQuadTinted(
                    *device, quad, shader, text.texture,
                    text.width * scale, text.height * scale,
                    linesRight - visual.name.width -
                        text.width * scale * 0.5F,
                    textY + text.height * scale * 0.5F,
                    1.0F, transparent, tint);
                textY += std::max(
                    text.height * scale, menu::smallFontHeight);
            }
            lineY += std::max(
                textY - lineY,
                std::max(visual.name.height,
                         menu::smallFontHeight));
        }

        if (!userChat.inputVisible())
            return;
        const float inputLeft = inRace
                                    ? 300.0F
                                    : static_cast<float>(
                                          raceStatsImage.width);
        const float inputBottom = inRace
                                      ? menu::virtualHeight - 10.0F
                                      : menu::virtualHeight -
                                            static_cast<float>(
                                                raceBottomPanelImage.height) -
                                            10.0F;
        drawQuad(
            *device, quad, shader, userChatVisual.inputName.texture,
            userChatVisual.inputName.width,
            userChatVisual.inputName.height,
            inputLeft + userChatVisual.inputName.width * 0.5F,
            inputBottom - userChatVisual.inputName.height * 0.5F,
            1.0F, transparent);
        if (valid(userChatVisual.inputText.texture))
        {
            const float maximumInputWidth =
                inRace
                    ? menu::virtualWidth - 600.0F -
                          userChatVisual.inputName.width
                    : menu::virtualWidth -
                          static_cast<float>(raceStatsImage.width) -
                          static_cast<float>(raceMoneyImage.width) -
                          userChatVisual.inputName.width;
            const float scale = std::min(
                1.0F,
                maximumInputWidth /
                    std::max(userChatVisual.inputText.width, 1.0F));
            drawQuad(
                *device, quad, shader,
                userChatVisual.inputText.texture,
                userChatVisual.inputText.width * scale,
                userChatVisual.inputText.height * scale,
                inputLeft + userChatVisual.inputName.width +
                    userChatVisual.inputText.width * scale * 0.5F,
                inputBottom -
                    userChatVisual.inputText.height * scale * 0.5F,
                1.0F, transparent);
        }
    };
#endif
    auto showAcceptDialog =
        [&](std::string_view message, std::string_view yesText,
            std::string_view noText, float centerX, float centerY,
            bool maxButtonsSize = false,
            bool maxMode = false, bool disableFocus = false) {
            const float sourceFrameWidth =
                static_cast<float>(acceptFrameImage.width);
            const float sourceFrameHeight =
                static_cast<float>(acceptFrameImage.height);
            const float sourceButtonWidth =
                static_cast<float>(acceptButtonImage.width);
            const float sourceButtonHeight =
                static_cast<float>(acceptButtonImage.height);
            const float frameScale = maxMode ? 1.7F : 1.0F;
            const float infoScale = maxMode ? 1.7F : 1.0F;
            float buttonScaleX = maxMode ? 1.5F : 1.0F;
            float yesOffsetX = maxMode ? -100.0F : -70.0F;
            float noOffsetX = maxMode ? 100.0F : 70.0F;
            if (maxButtonsSize)
            {
                buttonScaleX *= 1.5F;
                yesOffsetX -= 10.0F;
                noOffsetX += 10.0F;
            }
            destroyAcceptDialog(*device, acceptDialog);
            acceptDialog.frameWidth =
                sourceFrameWidth * frameScale;
            acceptDialog.frameHeight =
                sourceFrameHeight * frameScale;
            acceptDialog.infoWidth = 325.0F * infoScale;
            acceptDialog.infoHeight = 65.0F * infoScale;
            acceptDialog.buttonWidth =
                sourceButtonWidth * buttonScaleX;
            acceptDialog.buttonHeight = sourceButtonHeight;
            acceptDialog.yesOffsetX = yesOffsetX;
            acceptDialog.noOffsetX = noOffsetX;
            acceptDialog.buttonOffsetY =
                maxMode ? 72.0F : 32.0F;
            acceptDialog.maxMode = maxMode;
            acceptDialog.disableFocus = disableFocus;
            acceptDialog.hoveredChoice.reset();
            centerX = std::clamp(
                centerX,
                acceptDialog.frameWidth * 0.5F + 15.0F,
                menu::virtualWidth -
                    acceptDialog.frameWidth * 0.5F - 15.0F);
            centerY = std::clamp(
                centerY,
                acceptDialog.frameHeight * 0.5F + 15.0F,
                menu::virtualHeight -
                    acceptDialog.frameHeight * 0.5F - 15.0F);
            acceptDialog.centerX = centerX;
            acceptDialog.centerY = centerY;
            const float fontHeight =
                maxMode ? 24.0F : 32.0F;
            const std::size_t maximumLines =
                maxMode ? 3U : 2U;
            for (const auto& line : wrapAcceptDialogMessage(
                     message, acceptDialog.infoWidth,
                     fontHeight, maximumLines))
            {
                acceptDialog.info.push_back(createText(
                    *device, line, fontHeight, false,
                    menu::Rgba8{175, 175, 175, 255},
                    resolvedFont));
            }
            acceptDialog.yes = createText(
                *device, yesText, 32.0F, false,
                menu::Rgba8{175, 175, 175, 255}, resolvedFont);
            acceptDialog.no = createText(
                *device, noText, 32.0F, false,
                menu::Rgba8{175, 175, 175, 255}, resolvedFont);
#ifdef RRR3D_AUDIO
            playOriginalMenuSound(
                rrr3d::audio::OriginalMenuSound::Acceptance);
#endif
        };
    auto drawAcceptDialog = [&]() {
        if (!acceptDialogVisible() ||
            acceptDialog.info.empty())
        {
            return;
        }
        drawQuad(
            *device, quad, shader, acceptFrame,
            acceptDialog.frameWidth, acceptDialog.frameHeight,
            acceptDialog.centerX, acceptDialog.centerY, 8.0F,
            transparent);
        const float fontHeight =
            acceptDialog.maxMode ? 24.0F : 32.0F;
        const float lineStep = fontHeight * 1.15F;
        const float firstLineY =
            acceptDialog.centerY - 25.0F -
            static_cast<float>(acceptDialog.info.size() - 1U) *
                lineStep * 0.5F;
        for (std::size_t line = 0U;
             line < acceptDialog.info.size(); ++line)
        {
            const auto& text = acceptDialog.info[line];
            drawQuad(
                *device, quad, shader, text.texture,
                text.width, text.height, acceptDialog.centerX,
                firstLineY +
                    static_cast<float>(line) * lineStep,
                6.0F, transparent);
        }
        auto drawChoice = [&](bool yes) {
            const bool selected =
                acceptDialog.disableFocus
                    ? acceptDialog.hoveredChoice &&
                          *acceptDialog.hoveredChoice == yes
                    : acceptDialogYesFocused() == yes;
            const float x =
                acceptDialog.centerX +
                (yes ? acceptDialog.yesOffsetX
                     : acceptDialog.noOffsetX);
            const float y =
                acceptDialog.centerY +
                acceptDialog.buttonOffsetY;
            drawQuad(
                *device, quad, shader,
                selected ? acceptButtonSelected : acceptButton,
                acceptDialog.buttonWidth,
                acceptDialog.buttonHeight, x, y, 5.0F,
                transparent);
            const auto& label =
                yes ? acceptDialog.yes : acceptDialog.no;
            drawQuad(
                *device, quad, shader, label.texture,
                label.width, label.height, x, y, 3.0F,
                transparent);
        };
        drawChoice(true);
        drawChoice(false);
        if (profileDeleteDialogVisible)
        {
            profileDeleteDialogObserved =
                acceptDialog.frameWidth ==
                    static_cast<float>(
                        acceptFrameImage.width) &&
                acceptDialog.frameHeight ==
                    static_cast<float>(
                        acceptFrameImage.height) &&
                acceptDialog.infoWidth == 325.0F &&
                acceptDialog.infoHeight == 65.0F &&
                acceptDialog.buttonWidth ==
                    static_cast<float>(
                        acceptButtonImage.width) &&
                acceptDialog.buttonHeight ==
                    static_cast<float>(
                        acceptButtonImage.height) &&
                acceptDialog.yesOffsetX == -70.0F &&
                acceptDialog.noOffsetX == 70.0F &&
                acceptDialog.buttonOffsetY == 32.0F &&
                !acceptDialog.maxMode &&
                !acceptDialog.disableFocus &&
                valid(acceptDialog.yes.texture) &&
                valid(acceptDialog.no.texture) &&
                std::all_of(
                    acceptDialog.info.begin(),
                    acceptDialog.info.end(),
                    [](const TextVisual& line) {
                        return valid(line.texture);
                    });
        }
    };
    auto saveRaceProfile = [&]() {
        if (!raceSession.racers().empty())
        {
            raceSession.writePlayerProfile(profileState.player);
            raceSession.writeAchievementProfile(profileState);
            if (championshipMode &&
                raceSession.racers().front().finished &&
                !raceProgressSaved)
            {
                const auto completedTrack = selectedTrack;
                const auto advance =
                    r3d::game::originalrace::
                        completeOriginalTournamentTrack(
                            *originalRace, selectedTrack, profileState);
                raceTournamentAdvance = advance;
                selectedTrack = advance.trackIndex;
                racePlanetChampion = advance.planetChampion;
                raceProgressSaved = true;
                std::cout
                    << "Original Tournament::CompleteTrack: track "
                    << completedTrack << " -> " << advance.trackIndex
                    << ", passComplete=" << advance.passComplete
                    << ", passChampion=" << advance.passChampion
                    << ", planetChampion=" << advance.planetChampion
                    << '\n';
            }
        }
        auto persistedState = profileState;
        if (championshipPlayerBeforeSkirmish)
        {
            persistedState =
                r3d::game::originalrace::
                    makeOriginalSkirmishPersistenceState(
                        profileState,
                        *championshipPlayerBeforeSkirmish);
        }
        std::string profileError;
        if (!profileStore.save(persistedState, profileError))
            std::cerr << "Unable to save original profile: "
                      << profileError << '\n';
    };
    auto reloadCurrentRace = [&]() {
        try
        {
#ifdef RRR3D_AUDIO
            stopRaceAudio(false);
#endif
            raceHud.shutdown(*device);
            raceRenderer.shutdown(*device);
            *originalRace =
                r3d::game::originalrace::loadOriginalRace(
                    *resources, selectedTrack,
                    profileState.player.currentCar);
            r3d::game::originalrace::selectOriginalWeather(
                *resources, *originalRace,
                profileState.config.quality.light >= 1U &&
                    !weatherNightPassed,
                profileState.tutorialStage < 3U,
                static_cast<float>(std::rand()) /
                    static_cast<float>(RAND_MAX));
            weatherNightPassed = weatherNightPassed ||
                originalRace->environment.weather ==
                    r3d::game::originalrace::Weather::Night;
#ifdef RRR3D_NETWORK
            if (networkWeatherOverride)
            {
                applyWeather(
                    originalRace->environment,
                    weatherToken(*networkWeatherOverride),
                    originalRace->levelPath);
            }
            else
#endif
            if (options->weatherSelected)
                applyWeather(
                    originalRace->environment, options->weather,
                    originalRace->levelPath);
            profileState.player.currentCar =
                originalRace->vehicle.record;
            bool networkRosterApplied = false;
#ifdef RRR3D_NETWORK
            if (networkMatchStarted)
            {
                auto models = networkSession.snapshot().models;
                if (models.raceActive && !models.players.empty() &&
                    !originalRace->racers.empty())
                {
                    std::stable_sort(
                        models.players.begin(), models.players.end(),
                        [](const auto& left, const auto& right) {
                            if (left.owner != right.owner)
                                return left.owner;
                            const bool leftHuman = left.playerId == 0U;
                            const bool rightHuman = right.playerId == 0U;
                            if (leftHuman != rightHuman)
                                return leftHuman;
                            return leftHuman
                                       ? left.netSlot < right.netSlot
                                       : left.playerId < right.playerId;
                        });
                    const auto sourceRacers = originalRace->racers;
                    const auto count = std::clamp<std::size_t>(
                        models.players.size(), 1U,
                        sourceRacers.size());
                    originalRace->racers.resize(count);
                    networkRaceModelOrder.clear();
                    networkAppliedVehicleRevisions.clear();
                    networkRaceModelOrder.reserve(count);
                    static constexpr std::array<
                        std::string_view, 10> slotTypes{
                        "stWheel", "stTruba", "stArmor", "stMotor",
                        "stHyper", "stMine", "stWeapon1", "stWeapon2",
                        "stWeapon3", "stWeapon4"};
                    for (std::size_t index = 0U; index < count; ++index)
                    {
                        const auto& player = models.players[index];
                        networkRaceModelOrder.push_back(player.modelId);
                        const auto baseIndex = player.owner
                                                   ? 0U
                                                   : std::min<std::size_t>(
                                                         player.playerId == 0U
                                                             ? index
                                                             : player.playerId,
                                                         sourceRacers.size() -
                                                             1U);
                        auto racer = sourceRacers[baseIndex];
                        racer.human = player.playerId == 0U;
                        racer.gamerId = static_cast<std::uint32_t>(
                            std::max(player.gamerId, 0));
                        racer.color = player.color;
                        if (!player.car.empty())
                        {
                            const auto vehicle = std::find_if(
                                originalRace->vehicles.begin(),
                                originalRace->vehicles.end(),
                                [&](const auto& candidate) {
                                    return recordName(candidate.record) ==
                                           recordName(player.car);
                                });
                            if (vehicle != originalRace->vehicles.end())
                            {
                                racer.vehicle = static_cast<std::size_t>(
                                    std::distance(
                                        originalRace->vehicles.begin(),
                                        vehicle));
                                racer.configuredVehicle = *vehicle;
                                racer.hasConfiguredVehicle = true;
                            }
                        }
                        const bool hasNetworkLoadout =
                            player.playerId == 0U ||
                            std::any_of(
                                player.slots.begin(), player.slots.end(),
                                [](const auto& slot) {
                                    return !slot.record.empty();
                                });
                        if (hasNetworkLoadout)
                        {
                            racer.loadout.clear();
                            for (std::size_t slot = 0U;
                                 slot < player.slots.size(); ++slot)
                            {
                                if (player.slots[slot].record.empty())
                                    continue;
                                racer.loadout.push_back(
                                    {player.slots[slot].record,
                                     std::string(slotTypes[slot]),
                                     player.slots[slot].chargeCount});
                            }
                        }
                        originalRace->racers[index] = std::move(racer);
                    }
                    networkRosterApplied = true;
                }
            }
#endif
            if (!originalRace->racers.empty())
                originalRace->racers.front().name =
                    profileState.player.name;
            r3d::game::originalrace::applyOriginalPlayerProfile(
                *originalRace, *resources, profileState.player);
            r3d::game::originalrace::
                writeOriginalTournamentSelection(
                    *originalRace, selectedTrack,
                    profileState.player);
            if (!championshipMode)
            {
                // GameMode::StartRace applies these GameFrame values only
                // to rmSkirmish.  Championship keeps the tournament's own
                // lap and six-racer definitions.
                originalRace->lapCount =
                    std::clamp<std::uint32_t>(
                        profileState.config.lapsCount, 1U, 8U);
                if (!networkRosterApplied)
                {
                    const auto skirmishRacers =
                        std::min<std::size_t>(
                            originalRace->racers.size(),
                            std::clamp<std::uint32_t>(
                                profileState.config.maxComputers,
                                0U, 5U) +
                                1U);
                    originalRace->racers.resize(
                        std::max<std::size_t>(
                            skirmishRacers, 1U));
                }
            }
            *physicsDescription =
                r3d::game::originalrace::makePhysicsDescription(
                    *originalRace, *resources);
            std::string reloadError;
            physicsWorld =
                r3d::physics::createOriginalVehicleWorld(
                    *physicsDescription, reloadError);
            decorationDebrisBindings.clear();
            decorationFragments.clear();
            vehicleDebrisBindings.clear();
            vehicleDeathFragments.clear();
            if (!physicsWorld ||
                !raceRenderer.initialize(
                    *device, *resources, *originalRace,
                    static_cast<std::uint32_t>(pixelWidth),
                    static_cast<std::uint32_t>(pixelHeight),
                    reloadError) ||
                !raceHud.initialize(
                    *device, *resources, *originalRace,
                    activeLanguage, profileState.player.difficulty,
                    championshipMode,
                    reloadError))
            {
                std::cerr
                    << "Unable to reload original tournament race: "
                    << reloadError << '\n';
                return false;
            }
            raceSession.reset();
            raceSession.setCampaign(championshipMode);
            raceSession.applyPlayerProfile(profileState.player);
            raceSession.applyAchievementProfile(profileState);
            raceSession.setEnableMineBug(
                profileState.config.enableMineBug);
            raceSession.setSpringBorders(
                profileState.config.springBorders);
#ifdef RRR3D_AUDIO
            engineAudio.assign(
                originalRace->racers.size(), EngineAudio{});
            wheelSlipVoices.assign(
                originalRace->racers.size(), {});
            for (std::size_t racer = 0;
                 racer < originalRace->racers.size(); ++racer)
            {
                const auto vehicleIndex =
                    originalRace->racers[racer].vehicle;
                if (vehicleIndex >= originalRace->vehicles.size())
                    continue;
                const auto& vehicle =
                    originalRace->racers[racer]
                            .hasConfiguredVehicle
                        ? originalRace->racers[racer]
                              .configuredVehicle
                        : originalRace->vehicles[vehicleIndex];
                engineAudio[racer].idle =
                    loadEngineSound(vehicle.idleSoundPath);
                engineAudio[racer].rpm =
                    loadEngineSound(vehicle.rpmSoundPath);
            }
            if (!preloadEffectAudio())
            {
                std::cerr
                    << "Unable to preload original EventEffect audio\n";
                return false;
            }
#endif
            raceVehicles.resize(physicsWorld->vehicleCount());
            for (std::size_t index = 0;
                 index < physicsWorld->vehicleCount(); ++index)
                raceVehicles[index] = physicsWorld->vehicle(index);
            return true;
        }
        catch (const std::exception& exception)
        {
            std::cerr
                << "Unable to reload original tournament data: "
                << exception.what() << '\n';
            return false;
        }
    };
    auto restoreChampionshipProfile = [&]() {
        if (!championshipPlayerBeforeSkirmish)
            return true;
        profileState.player =
            *championshipPlayerBeforeSkirmish;
        championshipMode = true;
        selectedTrack =
            r3d::game::originalrace::
                resolveOriginalTournamentTrack(
                    *originalRace, profileState.player);
        if (!reloadCurrentRace())
            return false;
        championshipPlayerBeforeSkirmish.reset();
        raceProgressSaved = false;
        refreshProfilePage();
        return true;
    };
#ifdef RRR3D_NETWORK
    auto makeLocalNetworkPlayer = [&]() {
        using NetworkPlayerState =
            r3d::game::originalnetwork::NetworkPlayerState;
        NetworkPlayerState state;
        state.playerId = static_cast<std::uint8_t>(
            std::min<std::uint32_t>(profileState.player.playerId, 255U));
        state.netSlot = profileState.player.networkSlot;
        state.color = profileState.player.color;
        state.car = std::string(recordName(profileState.player.currentCar));
        state.gamerId = static_cast<std::int32_t>(
            std::min<std::uint32_t>(
                profileState.player.gamerId,
                static_cast<std::uint32_t>(
                    std::numeric_limits<std::int32_t>::max())));
        state.money = static_cast<std::int32_t>(
            std::min<std::uint32_t>(
                profileState.player.money,
                static_cast<std::uint32_t>(
                    std::numeric_limits<std::int32_t>::max())));
        state.raceReady = networkLocalReadyPublished;
        state.raceGoWait = networkLocalGoWaitPublished;
        state.raceFinish = networkLocalFinishPublished;
        for (std::size_t index = 0U;
             index < state.slots.size() &&
             index < profileState.player.slots.size(); ++index)
        {
            const auto& source = profileState.player.slots[index];
            state.slots[index].record =
                std::string(recordName(source.record));
            state.slots[index].chargeCount = source.charge;
        }
        if (!raceVehicles.empty())
        {
            const auto& vehicle = raceVehicles.front();
            state.vehicle.position = {
                vehicle.body.position.x, vehicle.body.position.y,
                vehicle.body.position.z};
            state.vehicle.rotation = {
                vehicle.body.rotation.x, vehicle.body.rotation.y,
                vehicle.body.rotation.z, vehicle.body.rotation.w};
            float mass = originalRace->vehicle.physics.mass;
            if (!originalRace->racers.empty())
            {
                const auto& racer = originalRace->racers.front();
                const auto& source = racer.hasConfiguredVehicle
                                         ? racer.configuredVehicle
                                         : originalRace->vehicles.at(
                                               racer.vehicle);
                mass = source.physics.mass;
            }
            state.vehicle.linearMomentum = {
                vehicle.linearVelocity.x * mass,
                vehicle.linearVelocity.y * mass,
                vehicle.linearVelocity.z * mass};
            state.vehicle.angularMomentum = {
                vehicle.angularMomentum.x,
                vehicle.angularMomentum.y,
                vehicle.angularMomentum.z};
            state.vehicle.moveState =
                raceInput.throttle > 0.01F
                    ? 3U
                    : (raceInput.reverse > 0.01F
                           ? 2U
                           : (raceInput.brake > 0.01F ? 1U : 0U));
            state.vehicle.steerState =
                raceInput.steering > 0.01F
                    ? 1U
                    : (raceInput.steering < -0.01F ? 2U : 0U);
            state.vehicle.steerWheelsAngle =
                raceInput.steering *
                originalRace->vehicle.physics.steerAngle;
        }
        return state;
    };
    auto startHostedNetworkMatch = [&]() {
        if (!networkHostRequested || networkMatchStarted)
            return true;
        std::string error;
        if (!networkSession.createHost(error))
        {
            std::cerr << "Original NetGame::CreateHost failed: "
                      << error << '\n';
            return false;
        }
        r3d::game::originalnetwork::NetworkMatchState match;
        match.mode = championshipMode ? 0 : 1;
        match.upgradeMaxLevel = static_cast<std::int32_t>(
            profileState.config.upgradeMaxLevel);
        match.weaponMaxLevel = static_cast<std::int32_t>(
            profileState.config.weaponMaxLevel);
        match.lapsCount = originalRace->lapCount;
        match.maxPlayers = profileState.config.maxPlayers;
        match.maxComputers = profileState.config.maxComputers;
        match.springBorders = profileState.config.springBorders;
        match.enableMineBug = profileState.config.enableMineBug;
        if (selectedTrack < originalRace->trackCatalog.size())
        {
            const auto planet =
                originalRace->trackCatalog[selectedTrack].planetIndex;
            match.planet = static_cast<std::int32_t>(planet);
            match.track = static_cast<std::int32_t>(std::count_if(
                originalRace->trackCatalog.begin(),
                originalRace->trackCatalog.begin() +
                    static_cast<std::ptrdiff_t>(selectedTrack),
                [planet](const auto& entry) {
                    return entry.planetIndex == planet;
                }));
        }
        match.weather = static_cast<std::int32_t>(
            originalRace->environment.weather);
        match.profileXml =
            r3d::game::originalrace::serializeOriginalNetworkProfile(
                profileState.player, championshipMode);
        if (!networkSession.startMatch(
                match, makeLocalNetworkPlayer(), error))
        {
            std::cerr << "Original NetRace::StartMatch failed: "
                      << error << '\n';
            networkSession.close();
            return false;
        }
        networkMatchStarted = true;
        networkLocalReadyPublished = false;
        networkPublishedPlayer = makeLocalNetworkPlayer();
        renderedNetworkRevision =
            std::numeric_limits<std::uint64_t>::max();
        refreshNetworkRuntimePages();
        networkLastIdentityEventSequence =
            networkSnapshot.models.events.empty()
                ? 0U
                : networkSnapshot.models.events.back().sequence;
        networkLastLifecycleEventSequence =
            networkLastIdentityEventSequence;
        networkPendingGamerId.reset();
        std::cout
            << "Original NetRace::StartMatch: classId=1, playerClassId=2, "
            << "planet=" << match.planet << ", track=" << match.track
            << ", profileBytes=" << match.profileXml.size() << '\n';
        return true;
    };
    auto publishLocalNetworkPlayer = [&]() {
        if (!networkMatchStarted)
            return true;
        auto state = makeLocalNetworkPlayer();
        if (!networkLocalCarSelected)
        {
            state.car.clear();
            state.money = 0;
            state.slots = {};
        }
        if (networkPublishedPlayer && *networkPublishedPlayer == state)
            return true;
        std::string error;
        if (!networkSession.setLocalPlayerState(state, error))
        {
            std::cerr << "Original NetPlayer state update failed: "
                      << error << '\n';
            return false;
        }
        networkPublishedPlayer = state;
        renderedNetworkRevision =
            std::numeric_limits<std::uint64_t>::max();
        return true;
    };
#endif
    auto doStartCurrentRace = [&]() {
        raceLoadingDeferredObserved =
            raceLoadingDeferredObserved ||
            raceLoadingPresentedFrames >= 2U;
        raceLoadingActive = false;
        // An explicit command-line track is a diagnostic/runtime override.
        // ProfileFrame selection in the integrated menu fixture reloads the
        // profile's campaign cursor, but must not silently discard that
        // override before the requested race is constructed.
        if (options->trackSelected)
            selectedTrack = options->trackIndex;
        if (!reloadCurrentRace())
        {
            runtimeSmokeFailed = true;
            running = false;
            return;
        }
#ifdef RRR3D_NETWORK
        if (networkMatchStarted)
        {
            // Windows GameMode::DoStartRace enters cGoRaceWait first. The
            // host does not start its one-second-lagged countdown until every
            // human NetPlayer has acknowledged that loading is complete.
            raceSession.synchronizeNetworkCountdown(0);
            raceSession.setNetworkFinishControlled(true);
            std::vector<bool> ownedRacers(
                networkRaceModelOrder.size(), false);
            for (std::size_t index = 0U;
                 index < networkRaceModelOrder.size(); ++index)
            {
                const auto player = std::find_if(
                    networkSnapshot.models.players.begin(),
                    networkSnapshot.models.players.end(),
                    [&](const auto& candidate) {
                        return candidate.modelId ==
                               networkRaceModelOrder[index];
                    });
                if (player == networkSnapshot.models.players.end())
                    continue;
                // The host owns the source AIPlayer graph. Human models are
                // controlled only by the peer which owns that NetPlayer.
                ownedRacers[index] =
                    player->owner ||
                    (networkHostRequested && player->playerId != 0U);
            }
            raceSession.setNetworkGameplayRole(
                true, networkHostRequested, std::move(ownedRacers));
            networkLastGameplayEventSequence =
                networkSnapshot.models.events.empty()
                    ? 0U
                    : networkSnapshot.models.events.back().sequence;
            networkLastShotEventSequence =
                networkLastGameplayEventSequence;
            networkLastBonusEventSequence =
                networkLastGameplayEventSequence;
            networkLastMineEventSequence =
                networkLastGameplayEventSequence;
            networkHostRaceGoSeconds = -1.0F;
            networkAppliedRaceGoStage = 0;
            networkHostFinishTimerStarted = false;
            networkRaceExitApplied = false;
            if (!publishLocalNetworkPlayer())
            {
                runtimeSmokeFailed = true;
                running = false;
                return;
            }
            std::string error;
            if (!networkSession.setLocalPlayerGoWait(true, error))
                std::cerr << "Original NetPlayer::RaceGoWait failed: "
                          << error << '\n';
            else
                networkLocalGoWaitPublished = true;
        }
#endif
        raceRenderer.resetCamera();
        raceInput = {};
        raceUseWeaponRequested = false;
        raceUseAllWeaponsRequested = false;
        raceUseMine = false;
        raceUseHyper = false;
        raceChangeWeaponRequested = false;
        raceWeaponChangeDirection = 1;
        raceFireWeaponSlotRequested = -1;
        raceResetRequested = false;
        exitRaceDialogVisible = false;
        exitRaceYesFocused = true;
        racePauseElapsedSnapshot = -1.0F;
        raceElapsedSeconds = 0.0F;
        raceProgressSaved = false;
        finishMenuShown = false;
        finishVoiceIndex = 0U;
        finishLastVoiceDispatched = false;
        raceVehicles.resize(physicsWorld->vehicleCount());
        maximumRaceAiSpeeds.assign(raceVehicles.size(), 0.0F);
        maximumRaceAiProgress.assign(raceVehicles.size(), 0.0F);
        raceAiThrottleFrames.assign(raceVehicles.size(), 0U);
        raceAiBrakeFrames.assign(raceVehicles.size(), 0U);
        raceAiReverseFrames.assign(raceVehicles.size(), 0U);
        for (std::size_t index = 0;
             index < physicsWorld->vehicleCount(); ++index)
            raceVehicles[index] = physicsWorld->vehicle(index);
        inRace = true;
        if (options->raceRenderSmokeTest)
            integratedRaceStartObserved = true;
        previousFrameTicks = SDL_GetTicksNS();
#ifdef RRR3D_AUDIO
        startRaceAudio();
#endif
        std::cout << "MainMenu2 -> original race: "
                  << originalRace->levelPath << '\n';
    };
    auto startCurrentRace = [&]() {
        if (inRace || raceLoadingActive)
            return;
#ifdef RRR3D_NETWORK
        if (networkHostRequested && !networkRaceStarted)
        {
            if (!startHostedNetworkMatch())
            {
                runtimeSmokeFailed = true;
                running = false;
                return;
            }
            std::string error;
            if (!networkSession.startRace(error))
            {
                std::cerr << "Original NetRace::StartRace failed: "
                          << error << '\n';
                runtimeSmokeFailed = true;
                running = false;
                return;
            }
            networkLocalReadyPublished = false;
            networkLocalGoWaitPublished = false;
            networkLocalFinishPublished = false;
            networkHostFinishTimerStarted = false;
            networkRaceExitApplied = false;
            networkPublishedPlayer.reset();
            networkRaceStarted = true;
            renderedNetworkRevision =
                std::numeric_limits<std::uint64_t>::max();
            refreshNetworkRuntimePages();
        }
#endif
        // GameMode::StartRace sets _startRace=0 and Menu::msInfo.
        // OnFrame invokes DoStartRace only when (++_startRace)>1, ensuring
        // loadingFrame.dds reaches the display before synchronous world load.
        raceLoadingActive = true;
        raceLoadingPresentedFrames = 0U;
#ifdef RRR3D_AUDIO
        musicDialogTime = -1.0F;
        musicDialogVisible = false;
#endif
        previousFrameTicks = SDL_GetTicksNS();
        std::cout
            << "Original GameMode::StartRace -> Menu::msInfo\n";
    };
    auto clearRaceControls = [&]() {
        raceInput = {};
        raceUseWeaponRequested = false;
        raceUseAllWeaponsRequested = false;
        raceUseMine = false;
        raceUseHyper = false;
        raceChangeWeaponRequested = false;
        raceWeaponChangeDirection = 1;
        raceFireWeaponSlotRequested = -1;
        raceResetRequested = false;
    };
    auto closeExitRaceDialog = [&]() {
        if (options->raceRenderSmokeTest &&
            racePauseElapsedSnapshot >= 0.0F &&
            physicsWorld->vehicleCount() > 0U)
        {
            const auto currentPosition =
                physicsWorld->vehicle().body.position;
            const float dx = currentPosition.x -
                             racePausePositionSnapshot.x;
            const float dy = currentPosition.y -
                             racePausePositionSnapshot.y;
            const float dz = currentPosition.z -
                             racePausePositionSnapshot.z;
            racePauseFrozenObserved =
                std::abs(raceSession.elapsedSeconds() -
                         racePauseElapsedSnapshot) < 0.0001F &&
                dx * dx + dy * dy + dz * dz < 0.000001F;
        }
        exitRaceDialogVisible = false;
        raceSession.setPaused(false);
        clearRaceControls();
        previousFrameTicks = SDL_GetTicksNS();
        racePauseResumeObserved = true;
    };
    auto openExitRaceDialog = [&]() {
        exitRaceDialogVisible = true;
        exitRaceYesFocused = true;
        showAcceptDialog(
            localized("svHintExitRace"), localized("svYes"),
            localized("svNo"), menu::virtualWidth * 0.5F,
            menu::virtualHeight * 0.5F);
        raceSession.setPaused(true);
        clearRaceControls();
        racePauseElapsedSnapshot = raceSession.elapsedSeconds();
        if (physicsWorld->vehicleCount() > 0U)
            racePausePositionSnapshot =
                physicsWorld->vehicle().body.position;
        racePauseDialogObserved = true;
    };
#ifdef RRR3D_NETWORK
    auto collectNetworkRaceResults = [&]() {
        const auto toSourceInt = [](std::uint32_t value) {
            return static_cast<std::int32_t>(
                std::min<std::uint32_t>(
                    value,
                    static_cast<std::uint32_t>(
                        std::numeric_limits<std::int32_t>::max())));
        };
        std::vector<r3d::game::originalnetwork::NetworkRaceResult>
            results;
        const auto count = std::min(
            networkRaceModelOrder.size(),
            raceSession.racers().size());
        results.reserve(count);
        for (std::size_t index = 0U; index < count; ++index)
        {
            const auto& racer = raceSession.racers()[index];
            if (racer.disconnected)
                continue;
            r3d::game::originalnetwork::NetworkRaceResult result;
            result.playerModelId = networkRaceModelOrder[index];
            result.playerPoints = toSourceInt(racer.points);
            result.playerMoney = toSourceInt(racer.money);
            result.money = toSourceInt(racer.rewardMoney);
            result.pickedMoney = toSourceInt(racer.pickedMoney);
            result.place = racer.place;
            result.points = toSourceInt(racer.rewardPoints);
            results.push_back(result);
        }
        return results;
    };
#endif
    auto leaveCurrentRace = [&](bool publishNetworkRaceExit = true) {
#ifdef RRR3D_NETWORK
        if (publishNetworkRaceExit && networkMatchStarted &&
            networkHostRequested && networkRaceStarted &&
            !networkRaceExitApplied)
        {
            std::string error;
            if (!networkSession.exitRace(
                    networkSnapshot.models.match.track,
                    networkSnapshot.models.match.weather,
                    collectNetworkRaceResults(), error))
            {
                std::cerr << "Original NetRace::ExitRace failed: "
                          << error << '\n';
            }
            else
            {
                networkRaceExitApplied = true;
                networkRaceStarted = false;
                refreshNetworkRuntimePages();
            }
        }
#else
        static_cast<void>(publishNetworkRaceExit);
#endif
        saveRaceProfile();
        raceSession.setPaused(false);
        exitRaceDialogVisible = false;
        inRace = false;
        clearRaceControls();
#ifdef RRR3D_AUDIO
        stopRaceAudio();
#endif
        previousFrameTicks = SDL_GetTicksNS();
        std::cout << "Original HudMenu accept: Race -> RaceMenu2\n";
    };
    auto replaceOptionsPage =
        [&](MenuPageVisual& page,
            std::vector<std::string> pageLabels) {
            auto replacement = createStyledPage(
                std::move(pageLabels), menu::smallFontHeight,
                optionsTextColor, menu::selectedTextColor);
            destroyPage(page);
            page = std::move(replacement);
            menuSelection =
                std::min(menuSelection, page.labels.size() - 1U);
        };
    auto originalCurrency = [](std::uint32_t value) {
        std::string result = std::to_string(value);
        for (std::ptrdiff_t index =
                 static_cast<std::ptrdiff_t>(result.size()) - 3;
             index > 0; index -= 3)
        {
            result.insert(static_cast<std::size_t>(index), ",");
        }
        return result;
    };
    auto hideWorkshopWeaponDialog = [&]() {
        workshopWeaponDialog.visible = false;
    };
    auto wrapWorkshopWeaponInfo = [&](std::string_view value) {
        constexpr float maximumWidth = 280.0F;
        constexpr std::size_t maximumLines = 4U;
        constexpr menu::Rgba8 infoColor{175, 175, 175, 255};
        std::vector<std::string> lines;
        std::istringstream words{std::string(value)};
        std::string line;
        std::string word;
        while (words >> word)
        {
            std::string candidate = line;
            if (!candidate.empty())
                candidate.push_back(' ');
            candidate += word;
            const auto measured = rrr3d::macos::rasterizeText(
                candidate, menu::fontFace, 18.0F, true, infoColor);
            if (!line.empty() &&
                static_cast<float>(measured.width) > maximumWidth)
            {
                lines.push_back(std::move(line));
                line = std::move(word);
                if (lines.size() == maximumLines)
                    break;
            }
            else
            {
                line = std::move(candidate);
            }
        }
        if (!line.empty() && lines.size() < maximumLines)
            lines.push_back(std::move(line));
        if (lines.empty())
            lines.emplace_back(" ");
        return lines;
    };
    auto showWorkshopWeaponDialog =
        [&](const r3d::game::originalrace::OriginalWorkshopItem& item,
            std::uint32_t cost, float senderX, float senderY,
            float slotWidth, float slotHeight) {
            const float frameWidth =
                static_cast<float>(workshopInfoFrameImage.width);
            const float frameHeight =
                static_cast<float>(workshopInfoFrameImage.height);
            const float centerX = std::clamp(
                senderX + slotWidth * 0.25F + frameWidth * 0.5F,
                frameWidth * 0.5F + 15.0F,
                menu::virtualWidth - frameWidth * 0.5F - 15.0F);
            const float centerY = std::clamp(
                senderY - slotHeight * 0.25F - frameHeight * 0.5F,
                frameHeight * 0.5F + 15.0F,
                menu::virtualHeight - frameHeight * 0.5F - 15.0F);
            if (workshopWeaponDialog.itemRecord == item.record &&
                workshopWeaponDialog.cost == cost &&
                std::abs(workshopWeaponDialog.centerX - centerX) <
                    0.01F &&
                std::abs(workshopWeaponDialog.centerY - centerY) <
                    0.01F &&
                valid(workshopWeaponDialog.name.texture))
            {
                workshopWeaponDialog.visible = true;
                return;
            }

            destroyWorkshopWeaponDialog(
                *device, workshopWeaponDialog);
            workshopWeaponDialog.name = createText(
                *device, localized(item.name), 24.0F, false,
                menu::Rgba8{255, 255, 255, 255}, resolvedFont);
            for (const auto& line :
                 wrapWorkshopWeaponInfo(localized(item.info)))
            {
                workshopWeaponDialog.info.push_back(createText(
                    *device, line, 18.0F, true,
                    menu::Rgba8{175, 175, 175, 255},
                    resolvedFont));
            }
            workshopWeaponDialog.money = createText(
                *device, originalCurrency(cost), 24.0F, false,
                menu::Rgba8{255, 255, 255, 255}, resolvedFont);
            const std::string damage =
                item.type > 4U
                    ? std::to_string(static_cast<int>(
                          std::lround(item.projectileDamage)))
                    : "-";
            workshopWeaponDialog.damage = createText(
                *device, damage, 24.0F, false,
                menu::Rgba8{255, 255, 255, 255}, resolvedFont);
            workshopWeaponDialog.itemRecord = item.record;
            workshopWeaponDialog.cost = cost;
            workshopWeaponDialog.centerX = centerX;
            workshopWeaponDialog.centerY = centerY;
            workshopWeaponDialog.visible = true;
        };
    auto wrapInfoDialogMessage = [&](std::string_view value) {
        constexpr float maximumWidth = 245.0F;
        constexpr std::size_t maximumLines = 5U;
        constexpr menu::Rgba8 infoColor{255, 255, 255, 255};
        std::vector<std::string> lines;
        std::istringstream words{std::string(value)};
        std::string line;
        std::string word;
        while (words >> word)
        {
            std::string candidate = line;
            if (!candidate.empty())
                candidate.push_back(' ');
            candidate += word;
            const auto measured = rrr3d::macos::rasterizeText(
                candidate, menu::fontFace, 24.0F, false, infoColor);
            if (!line.empty() &&
                static_cast<float>(measured.width) > maximumWidth)
            {
                lines.push_back(std::move(line));
                line = std::move(word);
                if (lines.size() == maximumLines)
                    break;
            }
            else
            {
                line = std::move(candidate);
            }
        }
        if (!line.empty() && lines.size() < maximumLines)
            lines.push_back(std::move(line));
        if (lines.empty())
            lines.emplace_back(" ");
        return lines;
    };
    auto hideInfoDialog = [&]() {
        infoDialog.visible = false;
    };
    auto showInfoDialog =
        [&](std::string_view title, std::string_view message,
            std::string_view ok, float centerX, float centerY) {
            const float frameWidth =
                static_cast<float>(infoDialogFrameImage.width);
            const float frameHeight =
                static_cast<float>(infoDialogFrameImage.height);
            centerX = std::clamp(
                centerX, frameWidth * 0.5F + 15.0F,
                menu::virtualWidth - frameWidth * 0.5F - 15.0F);
            centerY = std::clamp(
                centerY, frameHeight * 0.5F + 15.0F,
                menu::virtualHeight - frameHeight * 0.5F - 15.0F);
            destroyInfoDialog(*device, infoDialog);
            infoDialog.title = createText(
                *device, title, 44.0F, false,
                menu::Rgba8{175, 175, 175, 255}, resolvedFont);
            for (const auto& line :
                 wrapInfoDialogMessage(message))
            {
                infoDialog.info.push_back(createText(
                    *device, line, 24.0F, false,
                    menu::Rgba8{255, 255, 255, 255},
                    resolvedFont));
            }
            infoDialog.ok = createText(
                *device, ok, 32.0F, false,
                menu::Rgba8{255, 255, 255, 255}, resolvedFont);
            infoDialog.centerX = centerX;
            infoDialog.centerY = centerY;
            infoDialog.visible = true;
            infoDialog.dismissable = true;
            hideWorkshopWeaponDialog();
#ifdef RRR3D_AUDIO
            playOriginalMenuSound(
                rrr3d::audio::OriginalMenuSound::Warning);
#endif
        };
    auto showLoadingInfoDialog = [&]() {
        showInfoDialog(
            localized("svWarning"), localized("svHintPleaseWait"),
            localized("svOk"), menu::virtualWidth * 0.5F,
            menu::virtualHeight * 0.5F);
        if (valid(infoDialog.ok.texture))
            device->destroy(infoDialog.ok.texture);
        infoDialog.ok = {};
        infoDialog.dismissable = false;
    };
#ifdef RRR3D_NETWORK
    auto exitNetworkMatch = [&](bool publishExitRpc) {
        networkFailureDialogAction =
            NetworkFailureDialogAction::None;
        if (inRace)
            leaveCurrentRace(false);
        else
        {
            raceSession.setPaused(false);
            raceLoadingActive = false;
            clearRaceControls();
            saveRaceProfile();
        }
        if (publishExitRpc && networkSession.initialized() &&
            (networkMatchStarted || networkClientMatchEntered))
        {
            std::string error;
            if (!networkSession.exitMatch(error))
            {
                std::cerr << "Original NetRace::ExitMatch failed: "
                          << error << '\n';
            }
        }
        raceLoadingActive = false;
        clearNetworkRacePlayerVisuals();
        networkSession.close();
        networkSession.finalize();
        networkHostRequested = false;
        networkMatchStarted = false;
        networkRaceStarted = false;
        networkClientMatchEntered = false;
        networkLocalCarSelected = true;
        networkHostRaceGoSeconds = -1.0F;
        networkAppliedRaceGoStage = -1;
        networkLocalReadyPublished = false;
        networkLocalGoWaitPublished = false;
        networkLocalFinishPublished = false;
        networkHostFinishTimerStarted = false;
        networkRaceExitApplied = false;
        networkLastGameplayEventSequence = 0U;
        networkLastShotEventSequence = 0U;
        networkLastBonusEventSequence = 0U;
        networkLastMineEventSequence = 0U;
        networkLastChatEventSequence = 0U;
        networkLastIdentityEventSequence = 0U;
        networkLastLifecycleEventSequence = 0U;
        networkPublishedPlayer.reset();
        networkRaceModelOrder.clear();
        networkAppliedVehicleRevisions.clear();
        networkWeatherOverride.reset();
        networkPendingGamerId.reset();
        networkSnapshot = {};
        renderedNetworkRevision =
            std::numeric_limits<std::uint64_t>::max();
        handledNetworkFailureRevision = 0U;
        if (!restoreChampionshipProfile())
        {
            runtimeSmokeFailed = true;
            running = false;
            return;
        }
        menuStack = {MenuScreen::Main};
        menuSelection = 0U;
        refreshSharedMenuAvailability(MenuScreen::Main);
        std::cout << (publishExitRpc
                          ? "Original Menu::ExitMatch -> NetRace::ExitMatch\n"
                          : "Original Menu::MyDisconnectEvent/OnExitMatch -> "
                            "ExitRace/ExitMatch\n");
    };
    auto presentNetworkFailure = [&]() {
        using SessionFailure =
            r3d::game::originalnetwork::SessionFailure;
        using SessionState =
            r3d::game::originalnetwork::SessionState;
        if (networkSnapshot.state != SessionState::Failed ||
            networkSnapshot.revision == handledNetworkFailureRevision)
        {
            return;
        }
        handledNetworkFailureRevision = networkSnapshot.revision;
        const bool matchActive =
            networkMatchStarted || networkClientMatchEntered ||
            inRace || raceLoadingActive;
        const bool critical =
            networkSnapshot.failure == SessionFailure::Critical;
        const bool lostActiveHost =
            networkSnapshot.failure ==
                SessionFailure::HostDisconnected &&
            matchActive;

        hideInfoDialog();
        networkFailureDialogAction =
            critical || lostActiveHost
                ? NetworkFailureDialogAction::ExitMatch
                : NetworkFailureDialogAction::None;
        if (networkFailureDialogAction ==
            NetworkFailureDialogAction::ExitMatch)
        {
            raceSession.setPaused(true);
            clearRaceControls();
        }
        const auto message =
            critical
                ? localized("svCriticalNetError")
                : (lostActiveHost
                       ? localized("svHintDisconnect")
                       : localized("svHintHostConnectionFailed"));
        showInfoDialog(
            localized("svWarning"), message, localized("svOk"),
            menu::virtualWidth * 0.5F,
            menu::virtualHeight * 0.5F);
        networkFailureDialogObserved =
            infoDialog.visible && infoDialog.dismissable;
        std::cout << "Original network failure callback: failure="
                  << static_cast<int>(networkSnapshot.failure)
                  << ", exitMatch="
                  << (networkFailureDialogAction ==
                      NetworkFailureDialogAction::ExitMatch)
                  << '\n';
    };
    auto reconcileDisconnectedNetworkRacers = [&]() {
        if (!networkMatchStarted || !networkRaceStarted || !inRace)
            return;
        for (std::size_t racer = 0U;
             racer < networkRaceModelOrder.size() &&
             racer < raceSession.racers().size(); ++racer)
        {
            const auto modelId = networkRaceModelOrder[racer];
            const bool modelPresent = std::any_of(
                networkSnapshot.models.players.begin(),
                networkSnapshot.models.players.end(),
                [modelId](const auto& player) {
                    return player.modelId == modelId;
                });
            if (modelPresent ||
                !raceSession.disconnectNetworkRacer(racer))
            {
                continue;
            }

            // NetPlayer::~NetPlayer calls Player::FreeCar(true) and then
            // Race::DelPlayer for a remote model. Stable portable indices
            // retain the slot, but its Jolt body must leave the simulation.
            physicsWorld->setVehicleEnabled(racer, false);
#ifdef RRR3D_AUDIO
            if (racer < engineAudio.size())
            {
                audio.stop(engineAudio[racer].idleVoice);
                audio.stop(engineAudio[racer].rpmVoice);
                engineAudio[racer].idleVoice =
                    r3d::audio::invalidVoice;
                engineAudio[racer].rpmVoice =
                    r3d::audio::invalidVoice;
            }
            if (racer < wheelSlipVoices.size())
            {
                for (auto& wheel : wheelSlipVoices[racer])
                {
                    audio.stop(wheel.voice);
                    wheel = {};
                }
            }
            std::erase_if(
                shotEffectAudio, [&](const auto& source) {
                    if (source.owner != racer)
                        return false;
                    audio.stop(source.voice);
                    return true;
                });
            std::erase_if(
                contactEffectAudio, [&](const auto& source) {
                    if (source.racer != racer)
                        return false;
                    audio.stop(source.voice);
                    return true;
                });
            std::erase_if(
                timedEffectAudio, [&](const auto& source) {
                    if (source.followRacer != racer)
                        return false;
                    audio.stop(source.voice);
                    return true;
                });
#endif
            std::cout
                << "Original NetPlayer::~NetPlayer -> FreeCar/DelPlayer: "
                << "model=" << modelId << ", racer=" << racer << '\n';
        }
    };
#endif
    auto activateRaceMenuStart = [&]() {
#ifdef RRR3D_NETWORK
        if (networkMatchStarted)
        {
            if (!networkHostRequested)
            {
                const bool ready = !networkLocalReadyPublished;
                std::string error;
                if (!networkSession.setLocalPlayerReady(ready, error))
                {
                    std::cerr
                        << "Original NetPlayer::RaceReady failed: "
                        << error << '\n';
                    return;
                }
                networkLocalReadyPublished = ready;
                networkPublishedPlayer = makeLocalNetworkPlayer();
                renderedNetworkRevision =
                    std::numeric_limits<std::uint64_t>::max();
                refreshNetworkRuntimePages();
                refreshNetworkRacePlayerVisuals();
                std::cout << "Original NetPlayer::RaceReady("
                          << ready << ")\n";
                return;
            }

            std::vector<const r3d::game::originalnetwork::
                            NetworkPlayerState*>
                opponents;
            for (const auto& player : networkSnapshot.models.players)
            {
                if (!player.owner && player.playerId == 0U)
                    opponents.push_back(&player);
            }
            const bool allReady =
                !opponents.empty() &&
                std::all_of(
                    opponents.begin(), opponents.end(),
                    [](const auto* player) {
                        return player->raceReady;
                    });
            if (!allReady)
            {
                showInfoDialog(
                    localized("svWarning"),
                    localized("svHintPlayersIsNotReady"),
                    localized("svOk"),
                    menu::virtualWidth * 0.5F,
                    menu::virtualHeight * 0.5F);
                return;
            }

            const bool hasLeavers = std::any_of(
                networkRaceModelOrder.begin(),
                networkRaceModelOrder.end(),
                [&](std::uint32_t modelId) {
                    return std::none_of(
                        networkSnapshot.models.players.begin(),
                        networkSnapshot.models.players.end(),
                        [&](const auto& player) {
                            return player.modelId == modelId;
                        });
                });
            if (hasLeavers)
            {
                networkLeaverStartDialogVisible = true;
                networkLeaverStartYesFocused = true;
                auto message = localized(
                    "svHintLeaversWillBeRemoved");
                if (message == "svHintLeaversWillBeRemoved")
                    message = "Players who left will be removed";
                showAcceptDialog(
                    message, localized("svYes"), localized("svNo"),
                    menu::virtualWidth * 0.5F,
                    menu::virtualHeight * 0.5F);
                return;
            }
        }
#endif
        startCurrentRace();
    };
    auto wrapGarageInfo = [](std::string_view value) {
        constexpr std::size_t maximumCharacters = 78U;
        std::vector<std::string> lines;
        std::istringstream words{std::string(value)};
        std::string line;
        std::string word;
        while (words >> word)
        {
            if (!line.empty() &&
                line.size() + 1U + word.size() >
                    maximumCharacters)
            {
                lines.push_back(std::move(line));
                line.clear();
            }
            if (!line.empty())
                line.push_back(' ');
            line += word;
        }
        if (!line.empty())
            lines.push_back(std::move(line));
        if (lines.empty())
            lines.push_back(" ");
        if (lines.size() > 4U)
            lines.resize(4U);
        return lines;
    };
    auto rebuildGarageCarOrder = [&]() {
        garageCarOrder.clear();
        std::vector<GarageCarView> available;
        std::vector<GarageCarView> secret;
        std::vector<GarageCarView> locked;
        for (std::size_t index = 0U;
             index < originalGarage->cars.size(); ++index)
        {
            const auto& car = originalGarage->cars[index];
            const bool isSecret = std::none_of(
                originalGarage->carUnlocks.begin(),
                originalGarage->carUnlocks.end(),
                [&](const auto& rule) {
                    return rule.record == car.record;
                });
            if (championshipMode && isSecret)
                continue;
            const bool achievement =
                r3d::game::originalrace::
                    originalRecordAchievementUnlocked(
                        profileState, car.record);
            const bool unlocked =
                r3d::game::originalrace::originalCarUnlocked(
                    *originalGarage, profileState, car,
                    championshipMode);
            const GarageCarView view{index, !unlocked};
            if (isSecret && achievement)
                secret.push_back(view);
            else if (unlocked && achievement)
                available.push_back(view);
            else
                locked.push_back(view);
        }
        garageCarOrder.insert(
            garageCarOrder.end(), available.begin(), available.end());
        garageCarOrder.insert(
            garageCarOrder.end(), secret.begin(), secret.end());
        garageCarOrder.insert(
            garageCarOrder.end(), locked.begin(), locked.end());
        if (garageCarOrder.empty())
            return;
        const auto selected = std::find_if(
            garageCarOrder.begin(), garageCarOrder.end(),
            [&](const auto& view) {
                return originalGarage->cars[view.catalogIndex].record ==
                       profileState.player.currentCar;
            });
        garageViewIndex =
            selected == garageCarOrder.end()
                ? std::min(
                      garageViewIndex,
                      garageCarOrder.size() - 1U)
                : static_cast<std::size_t>(
                      std::distance(
                          garageCarOrder.begin(), selected));
        garageCarIndex =
            garageCarOrder[garageViewIndex].catalogIndex;
    };
    auto refreshGaragePage = [&]() {
        if (originalGarage->cars.empty())
            return;
        if (garageCarOrder.empty())
            rebuildGarageCarOrder();
        if (garageCarOrder.empty())
            return;
        garageViewIndex =
            std::min(garageViewIndex,
                     garageCarOrder.size() - 1U);
        garageCarIndex =
            garageCarOrder[garageViewIndex].catalogIndex;
        garageCarIndex =
            std::min(garageCarIndex,
                     originalGarage->cars.size() - 1U);
        const auto& car = originalGarage->cars[garageCarIndex];
        const bool locked =
            garageCarOrder[garageViewIndex].locked;
        auto replacement = createPage(
            {localized(
                 locked ? "svLockedCarName" : car.name),
             originalCurrency(profileState.player.money),
             locked ? "-" : originalCurrency(car.cost),
             localized("svBack"), localized("svBuy")});
        destroyPage(garagePage);
        garagePage = std::move(replacement);

        auto infoReplacement = createStyledPage(
            wrapGarageInfo(localized(
                locked ? "svLockedCarInfo" : car.info)),
            menu::smallFontHeight, raceTextColor,
            menu::selectedTextColor);
        destroyPage(garageInfoPage);
        garageInfoPage = std::move(infoReplacement);

        const auto stats =
            r3d::game::originalrace::originalGarageStats(
                *originalGarage, car);
        auto statValue = [](float value) {
            return std::to_string(
                static_cast<int>(std::lround(value)));
        };
        auto statsReplacement = createStyledPage(
            {locked
                 ? "0/0"
                 : statValue(stats.armor) + "/" +
                       statValue(stats.maximumArmor),
             locked
                 ? "0/0"
                 : statValue(stats.damage) + "/" +
                       statValue(stats.maximumDamage),
             locked
                 ? "0/300"
                 : statValue(stats.speedProgress * 300.0F) +
                       "/300"},
            menu::smallFontHeight, raceTextColor,
            menu::selectedTextColor);
        destroyPage(garageStatsPage);
        garageStatsPage = std::move(statsReplacement);

    };
    auto garageColorAvailable = [&](std::size_t colorIndex) {
        if (colorIndex >= garageColorPixels.size())
            return false;
#ifdef RRR3D_NETWORK
        if (networkMatchStarted)
        {
            for (const auto& player : networkSnapshot.models.players)
            {
                if (player.owner || player.playerId != 0U)
                    continue;
                bool same = true;
                for (std::size_t component = 0U;
                     component < player.color.size(); ++component)
                {
                    const float color = static_cast<float>(
                                            garageColorPixels[colorIndex]
                                                              [component]) /
                                        255.0F;
                    same = same &&
                           std::abs(player.color[component] - color) <
                               0.001F;
                }
                if (same)
                    return false;
            }
        }
#endif
        return true;
    };
    auto moveGarageFocus = [&](int direction) {
        constexpr std::size_t focusCount = 18U;
        for (std::size_t step = 0U; step < focusCount; ++step)
        {
            menuSelection = direction < 0
                                ? (menuSelection + focusCount - 1U) %
                                      focusCount
                                : (menuSelection + 1U) % focusCount;
            if (menuSelection < 4U ||
                garageColorAvailable(menuSelection - 4U))
            {
                return;
            }
        }
    };
    auto gamerUnlocked = [&](std::size_t index) {
        if (index >= originalGarage->gamers.size() ||
            !r3d::game::originalrace::originalGamerUnlocked(
                profileState, originalGarage->gamers[index].bossId))
        {
            return false;
        }
#ifdef RRR3D_NETWORK
        if (networkMatchStarted)
        {
            const auto gamerId = static_cast<std::int32_t>(
                originalGarage->gamers[index].bossId);
            for (const auto& player : networkSnapshot.models.players)
            {
                if (!player.owner && player.playerId == 0U &&
                    player.gamerId == gamerId)
                {
                    return false;
                }
            }
        }
#endif
        return true;
    };
    auto adjacentGamerIndex =
        [&](std::size_t from, int direction)
            -> std::optional<std::size_t> {
            if (direction > 0)
            {
                for (std::size_t index = from + 1U;
                     index < originalGarage->gamers.size(); ++index)
                {
                    if (gamerUnlocked(index))
                        return index;
                }
            }
            else
            {
                for (std::size_t index = from; index > 0U; --index)
                {
                    if (gamerUnlocked(index - 1U))
                        return index - 1U;
                }
            }
            return std::nullopt;
        };
    auto wrapGamersInfo = [&](std::string_view value) {
        constexpr float maximumWidth = 475.0F;
        constexpr std::size_t maximumLines = 7U;
        constexpr menu::Rgba8 color{214, 214, 214, 255};
        std::vector<std::string> lines;
        std::istringstream words{std::string(value)};
        std::string line;
        std::string word;
        while (words >> word)
        {
            std::string candidate = line;
            if (!candidate.empty())
                candidate.push_back(' ');
            candidate += word;
            const auto measured = rrr3d::macos::rasterizeText(
                candidate, menu::fontFace, menu::smallFontHeight,
                false, color);
            if (!line.empty() &&
                static_cast<float>(measured.width) > maximumWidth)
            {
                lines.push_back(std::move(line));
                line = std::move(word);
                if (lines.size() == maximumLines)
                    break;
            }
            else
            {
                line = std::move(candidate);
            }
        }
        if (!line.empty() && lines.size() < maximumLines)
            lines.push_back(std::move(line));
        if (lines.empty())
            lines.emplace_back(" ");
        return lines;
    };
    auto refreshGamersFrame = [&]() {
        if (originalGarage->gamers.empty())
            return;
        gamerPlanetIndex = std::min(
            gamerPlanetIndex, originalGarage->gamers.size() - 1U);
        const auto& gamer =
            originalGarage->gamers[gamerPlanetIndex];
        auto nameReplacement = createStyledPage(
            {localized(gamer.name)}, menu::headerFontHeight,
            menu::Rgba8{255, 255, 255, 255},
            menu::selectedTextColor);
        auto infoReplacement = createStyledPage(
            wrapGamersInfo(localized(gamer.info)),
            menu::smallFontHeight,
            menu::Rgba8{214, 214, 214, 255},
            menu::selectedTextColor);
        auto bonusReplacement = createStyledPage(
            {localized(gamer.bossBonus)}, 30.0F,
            menu::Rgba8{214, 214, 214, 255},
            menu::selectedTextColor);
        destroyPage(gamersNamePage);
        destroyPage(gamersInfoPage);
        destroyPage(gamersBonusPage);
        gamersNamePage = std::move(nameReplacement);
        gamersInfoPage = std::move(infoReplacement);
        gamersBonusPage = std::move(bonusReplacement);
    };
    auto selectGamerPlanet = [&](std::size_t index) {
        if (index >= originalGarage->gamers.size() ||
            !gamerUnlocked(index) || index == gamerPlanetIndex)
            return;
        gamerPlanetIndex = index;
        gamersSelectionChangedObserved = true;
        refreshGamersFrame();
    };
    auto showOriginalGamers = [&]() {
        gamerPlanetIndex = 0U;
        const auto selected = std::find_if(
            originalGarage->gamers.begin(),
            originalGarage->gamers.end(),
            [&](const auto& gamer) {
                return gamer.bossId ==
                           profileState.player.gamerId &&
                       r3d::game::originalrace::originalGamerUnlocked(
                           profileState, gamer.bossId);
            });
        if (selected != originalGarage->gamers.end())
        {
            gamerPlanetIndex = static_cast<std::size_t>(
                std::distance(
                    originalGarage->gamers.begin(), selected));
        }
        else
        {
            for (std::size_t index = 0U;
                 index < originalGarage->gamers.size(); ++index)
                if (gamerUnlocked(index))
                {
                    gamerPlanetIndex = index;
                    break;
                }
        }
        gamersFocus = GamersFocus::Next;
        gamersSceneSeconds = 0.0F;
        refreshGamersFrame();
        menuStack = championshipMode
                        ? std::vector<MenuScreen>{
                              MenuScreen::Main,
                              MenuScreen::GameMode,
                              MenuScreen::Tournament,
                              MenuScreen::Gamers}
                        : std::vector<MenuScreen>{
                              MenuScreen::Main,
                              MenuScreen::GameMode,
                              MenuScreen::Gamers};
        menuSelection = 0U;
        std::cout << "Original RaceMenu2::GamersFrame: gamer "
                  << originalGarage->gamers[gamerPlanetIndex].record
                  << '\n';
    };
    auto showOriginalGarageAfterGamers = [&]() {
        refreshRaceMainPages();
        rebuildGarageCarOrder();
        refreshGaragePage();
        menuStack = championshipMode
                        ? std::vector<MenuScreen>{
                              MenuScreen::Main,
                              MenuScreen::GameMode,
                              MenuScreen::Tournament,
                              MenuScreen::RaceMenu,
                              MenuScreen::Garage}
                        : std::vector<MenuScreen>{
                              MenuScreen::Main,
                              MenuScreen::GameMode,
                              MenuScreen::RaceMenu,
                              MenuScreen::Garage};
        menuSelection = 0U;
        std::cout
            << "Original GamersFrame::cVideoStopped -> GarageFrame\n";
    };
#ifdef RRR3D_VIDEO
    originalMovieGamersGarage = showOriginalGarageAfterGamers;
#endif
    auto confirmOriginalGamer = [&]() {
        if (gamerPlanetIndex >= originalGarage->gamers.size())
            return;
        const auto& gamer =
            originalGarage->gamers[gamerPlanetIndex];
#ifdef RRR3D_NETWORK
        if (networkMatchStarted)
        {
            std::string error;
            const auto requestedGamerId =
                static_cast<std::int32_t>(gamer.bossId);
            if (!networkSession.setLocalPlayerGamerId(
                    requestedGamerId, error))
            {
                std::cerr << "Original NetPlayer::SetGamerId failed: "
                          << error << '\n';
                showInfoDialog(
                    localized("svWarning"),
                    localized("svHintHostConnectionFailed"),
                    localized("svOk"), menu::virtualWidth * 0.5F,
                    menu::virtualHeight * 0.5F);
                return;
            }
            networkPendingGamerId = requestedGamerId;
            renderedNetworkRevision =
                std::numeric_limits<std::uint64_t>::max();
            showLoadingInfoDialog();
            return;
        }
#endif
        profileState.player.gamerId = gamer.bossId;
        if (!reloadCurrentRace())
        {
            runtimeSmokeFailed = true;
            running = false;
            return;
        }
        if (championshipMode && !options->gamersFrameSmokeTest)
            saveRaceProfile();
#ifdef RRR3D_VIDEO
        if (championshipMode &&
            !profileState.config.disableVideo)
        {
            const auto movie =
                activeLanguage == "russian"
                    ? "Data/Video/intaria.avi"
                    : "Data/Video/intaria_eng.avi";
            if (playOriginalMovie(
                    movie,
                    OriginalMovieCompletion::GamersGarage))
            {
                return;
            }
        }
#endif
        showOriginalGarageAfterGamers();
    };
#ifdef RRR3D_NETWORK
    auto processNetworkIdentityEvents = [&]() {
        for (const auto& event : networkSnapshot.models.events)
        {
            if (event.sequence <= networkLastIdentityEventSequence)
                continue;
            networkLastIdentityEventSequence = std::max(
                networkLastIdentityEventSequence, event.sequence);
            if (event.kind != r3d::game::originalnetwork::
                                  NetworkEventKind::PlayerGamerId &&
                event.kind != r3d::game::originalnetwork::
                                  NetworkEventKind::PlayerColor)
            {
                continue;
            }
            const auto owner = std::find_if(
                networkSnapshot.models.players.begin(),
                networkSnapshot.models.players.end(),
                [&](const auto& player) {
                    return player.owner &&
                           player.modelId == event.playerModelId;
                });
            if (owner == networkSnapshot.models.players.end())
                continue;

            if (event.kind == r3d::game::originalnetwork::
                                  NetworkEventKind::PlayerGamerId)
            {
                profileState.player.gamerId =
                    static_cast<std::uint32_t>(
                        std::max(owner->gamerId, 0));
                networkPublishedPlayer = makeLocalNetworkPlayer();
                if (!networkPendingGamerId)
                    continue;

                networkPendingGamerId.reset();
                hideInfoDialog();
                if (event.flag)
                {
                    showInfoDialog(
                        localized("svWarning"),
                        localized("svHintSetGamerFailed"),
                        localized("svOk"),
                        menu::virtualWidth * 0.5F,
                        menu::virtualHeight * 0.5F);
                    continue;
                }
                if (!reloadCurrentRace())
                {
                    runtimeSmokeFailed = true;
                    running = false;
                    return;
                }
                showOriginalGarageAfterGamers();
                continue;
            }

            profileState.player.color = owner->color;
            networkPublishedPlayer = makeLocalNetworkPlayer();
            refreshGaragePage();
            if (event.flag)
            {
                showInfoDialog(
                    localized("svWarning"),
                    localized("svHintSetColorFailed"),
                    localized("svOk"),
                    menu::virtualWidth * 0.5F,
                    menu::virtualHeight * 0.5F);
            }
        }
    };
    auto processNetworkLifecycleEvents = [&]() {
        for (const auto& event : networkSnapshot.models.events)
        {
            if (event.sequence <= networkLastLifecycleEventSequence)
                continue;
            networkLastLifecycleEventSequence = std::max(
                networkLastLifecycleEventSequence, event.sequence);
            if (event.kind != r3d::game::originalnetwork::
                                  NetworkEventKind::MatchExited ||
                (!networkMatchStarted && !networkClientMatchEntered))
            {
                continue;
            }
            std::cout
                << "Original NetRace::OnExitMatch: sender="
                << event.sender << '\n';
            exitNetworkMatch(false);
            return true;
        }
        return false;
    };
#endif
    auto refreshWorkshopPage = [&]() {
        workshopGoods.clear();
        for (const auto& item : originalGarage->workshop)
        {
            // Workshop::_items contains pass rewards.  The four mobility
            // families are exposed only by the installed-slot level button.
            if (item.type <= 4U ||
                !r3d::game::originalrace::
                    originalWorkshopItemUnlocked(
                        *originalGarage, profileState, item))
            {
                continue;
            }
            workshopGoods.push_back(&item);
        }
        std::stable_sort(
            workshopGoods.begin(), workshopGoods.end(),
            [](const auto* left, const auto* right) {
                return left->cost < right->cost;
            });
        const std::size_t rowCount =
            (workshopGoods.size() + 2U) / 3U;
        const std::size_t maximumScroll =
            rowCount > 4U ? rowCount - 4U : 0U;
        workshopGoodScroll =
            std::min(workshopGoodScroll, maximumScroll);

        auto pageReplacement = createStyledPage(
            {localized("svWorkshop")},
            menu::headerFontHeight, raceTextColor,
            menu::selectedTextColor);
        destroyPage(workshopPage);
        workshopPage = std::move(pageReplacement);
        auto controlsReplacement = createStyledPage(
            {localized("svBack"),
             originalCurrency(profileState.player.money)},
            menu::smallFontHeight, raceTextColor,
            menu::selectedTextColor);
        destroyPage(workshopControlsPage);
        workshopControlsPage = std::move(controlsReplacement);

        if (const auto* car = originalGarage->findCar(
                profileState.player.currentCar))
        {
            const auto stats =
                r3d::game::originalrace::originalGarageStats(
                    *originalGarage, *car, profileState.player);
            auto statValue = [](float value) {
                return std::to_string(
                    static_cast<int>(std::lround(value)));
            };
            auto statsReplacement = createStyledPage(
                {statValue(stats.damage) + "/" +
                     statValue(stats.maximumDamage),
                 statValue(stats.armor) + "/" +
                     statValue(stats.maximumArmor),
                 statValue(stats.speedProgress * 300.0F) +
                     "/300"},
                menu::smallFontHeight, raceTextColor,
                menu::selectedTextColor);
            destroyPage(workshopStatsPage);
            workshopStatsPage =
                std::move(statsReplacement);

            auto hintReplacement = createStyledPage(
                wrapGarageInfo(localized(car->info)),
                menu::smallFontHeight, raceTextColor,
                menu::selectedTextColor);
            destroyPage(workshopHintPage);
            workshopHintPage =
                std::move(hintReplacement);
        }

    };
    auto workshopSlotCenters = [&]() {
        std::array<std::array<float, 2>,
                   static_cast<std::size_t>(
                       r3d::game::originalrace::
                           GarageSlotType::Count)>
            result{};
        const float scale = menu::virtualHeight / 720.0F;
        const float centerX = menu::virtualWidth * 0.63F;
        const float centerY = menu::virtualHeight * 0.451F;
        const float leftPanelRight =
            30.0F +
            static_cast<float>(workshopLeftPanelImage.width);
        const float leftOffset = std::max(
            centerX - 330.0F * scale, leftPanelRight + 80.0F);
        const float rightOffset = std::min(
            centerX + 330.0F * scale,
            menu::virtualWidth - 120.0F);
        const float topOffset = std::max(
            centerY - 180.0F * scale,
            static_cast<float>(workshopTopPanelImage.height) +
                65.0F);
        const float bottomOffset = std::min(
            centerY + 180.0F * scale,
            menu::virtualHeight -
                static_cast<float>(
                    workshopBottomPanelImage.height) -
                65.0F);
        const float slotWidth =
            static_cast<float>(workshopSlotImage.width);
        const float slotHeight =
            static_cast<float>(workshopSlotImage.height);
        constexpr float slotSpace = 15.0F;
        result = {{
            {leftOffset,
             centerY + slotHeight * 0.5F + slotSpace},
            {leftOffset,
             centerY - slotHeight * 0.5F - slotSpace},
            {centerX - slotWidth * 0.5F - 6.0F * slotSpace,
             bottomOffset},
            {centerX + slotWidth * 0.5F + 6.0F * slotSpace,
             bottomOffset},
            {rightOffset,
             centerY - slotHeight * 0.5F - slotSpace},
            {rightOffset,
             centerY + slotHeight * 0.5F + slotSpace},
            {centerX -
                 (slotWidth + 4.5F * slotSpace) * 1.5F,
             topOffset},
            {centerX -
                 (slotWidth + 4.5F * slotSpace) * 0.5F,
             topOffset},
            {centerX +
                 (slotWidth + 4.5F * slotSpace) * 0.5F,
             topOffset},
            {centerX +
                 (slotWidth + 4.5F * slotSpace) * 1.5F,
             topOffset},
        }};
        return result;
    };
    auto workshopGoodCenters = [&]() {
        std::array<std::array<float, 2>, 12> result{};
        const float panelCenterY =
            (static_cast<float>(workshopTopPanelImage.height) -
                 30.0F +
             menu::virtualHeight -
             static_cast<float>(
                 workshopBottomPanelImage.height)) *
            0.5F;
        const float panelTop =
            panelCenterY -
            static_cast<float>(workshopLeftPanelImage.height) *
                0.5F;
        constexpr float cell = 100.0F;
        const float firstX = 30.0F + 22.0F + 39.0F;
        const float firstY = panelTop + 11.0F + 38.0F;
        for (std::size_t index = 0U; index < result.size(); ++index)
        {
            result[index] = {
                firstX + static_cast<float>(index % 3U) * cell,
                firstY + static_cast<float>(index / 3U) * cell};
        }
        return result;
    };
    auto workshopSlotAccepts =
        [&](std::size_t slotIndex,
            const r3d::game::originalrace::ProfileSlot& item) {
            const auto* car = originalGarage->findCar(
                profileState.player.currentCar);
            if (car == nullptr ||
                slotIndex >= car->placements.size())
                return false;
            const auto& placement = car->placements[slotIndex];
            if (!placement.active || placement.locked)
                return false;
            if (!championshipMode &&
                slotIndex >=
                    r3d::game::originalrace::PlayerProfile::
                        firstWeaponSlot &&
                slotIndex -
                        r3d::game::originalrace::PlayerProfile::
                            firstWeaponSlot >=
                    profileState.config.weaponMaxLevel)
            {
                return false;
            }
            return std::find(
                       placement.supportedItems.begin(),
                       placement.supportedItems.end(),
                       item.record) !=
                   placement.supportedItems.end();
        };
    auto showWorkshopConfirmation =
        [&](WorkshopConfirmation confirmation,
            const r3d::game::originalrace::
                OriginalWorkshopItem& item,
            std::uint32_t value) {
            std::string message = localized(
                confirmation == WorkshopConfirmation::Buy
                    ? "svBuyWeapon"
                    : "svSellWeapon");
            if (const auto marker = message.find("%s");
                marker != std::string::npos)
            {
                message.replace(
                    marker, 2U, originalCurrency(value));
            }
            workshopConfirmation = confirmation;
            workshopPendingPurchase =
                confirmation == WorkshopConfirmation::Buy
                    ? &item
                    : nullptr;
            workshopConfirmationYesFocused = true;
            hideWorkshopWeaponDialog();
            const bool buying =
                confirmation == WorkshopConfirmation::Buy;
            float senderX = workshopDragX;
            float senderY = workshopDragY;
            float senderWidth =
                static_cast<float>(workshopSlotImage.width);
            float senderHeight =
                static_cast<float>(workshopSlotImage.height);
            if (buying)
            {
                constexpr std::size_t firstGoodFocus = 1U;
                const std::size_t visible =
                    menuSelection >= firstGoodFocus
                        ? menuSelection - firstGoodFocus
                        : 0U;
                const auto centers = workshopGoodCenters();
                const auto& sender =
                    centers[std::min(
                        visible, centers.size() - 1U)];
                senderX = sender[0];
                senderY = sender[1];
                senderWidth = 100.0F;
                senderHeight = 100.0F;
            }
            const float posX =
                senderX + senderWidth * 0.25F;
            const float posY =
                senderY - senderHeight * 0.25F;
            showAcceptDialog(
                message, localized("svYes"), localized("svNo"),
                posX +
                    static_cast<float>(
                        acceptFrameImage.width) *
                        0.5F,
                posY -
                    static_cast<float>(
                        acceptFrameImage.height) *
                        0.5F);
        };
    auto showWorkshopGoodMessage = [&](std::string_view messageKey) {
        constexpr std::size_t firstGoodFocus = 1U;
        const std::size_t visible =
            menuSelection >= firstGoodFocus
                ? menuSelection - firstGoodFocus
                : 0U;
        const auto centers = workshopGoodCenters();
        const auto& sender =
            centers[std::min(visible, centers.size() - 1U)];
        const float posX = sender[0] + 25.0F;
        const float posY = sender[1] - 25.0F;
        showInfoDialog(
            localized("svWarning"), localized(messageKey),
            localized("svOk"),
            posX +
                static_cast<float>(infoDialogFrameImage.width) *
                    0.5F,
            posY -
                static_cast<float>(infoDialogFrameImage.height) *
                    0.5F);
    };
    auto buyWorkshopGood =
        [&](const r3d::game::originalrace::
                OriginalWorkshopItem& item) {
            r3d::game::originalrace::ProfileSlot purchased;
            std::string workshopError;
            if (!r3d::game::originalrace::
                    buyOriginalWorkshopItem(
                        *originalGarage, profileState, item,
                        championshipMode, purchased,
                        workshopError))
            {
                std::cerr
                    << "Original WorkshopFrame buy: "
                    << workshopError << '\n';
                showWorkshopGoodMessage("svHintCantMoney");
                return false;
            }
            workshopDrag.item = std::move(purchased);
            workshopDrag.origin.reset();
#ifdef RRR3D_AUDIO
            playOriginalMenuSound(
                rrr3d::audio::OriginalMenuSound::PickupDown);
#endif
            saveRaceProfile();
            refreshWorkshopPage();
            return true;
        };
    auto stopWorkshopDrag =
        [&](bool intoGoods, bool accepted = false) {
        if (!workshopDrag.active())
            return true;
        std::string workshopError;
        if (!intoGoods && workshopDrag.origin)
        {
            const auto originIndex =
                static_cast<std::size_t>(*workshopDrag.origin);
            if (originIndex < profileState.player.slots.size() &&
                profileState.player.slots[originIndex].record.empty())
            {
                r3d::game::originalrace::ProfileSlot replaced;
                if (!r3d::game::originalrace::
                        installOriginalWorkshopSlot(
                            *originalGarage, profileState,
                            *workshopDrag.origin, workshopDrag.item,
                            replaced, workshopError))
                {
                    std::cerr
                        << "Original WorkshopFrame restore: "
                        << workshopError << '\n';
                    return false;
                }
                workshopDrag = {};
#ifdef RRR3D_AUDIO
                playOriginalMenuSound(
                    rrr3d::audio::OriginalMenuSound::PickupUp);
#endif
                saveRaceProfile();
                refreshWorkshopPage();
                return true;
            }
        }
        const bool discount = workshopDrag.origin.has_value();
        if (championshipMode && discount && !accepted)
        {
            const auto* item = originalGarage->findItem(
                workshopDrag.item.record);
            if (item == nullptr)
                return false;
            showWorkshopConfirmation(
                WorkshopConfirmation::Sell, *item,
                r3d::game::originalrace::
                    originalWorkshopSellValue(
                        *originalGarage,
                        workshopDrag.item, true));
            return false;
        }
        if (!r3d::game::originalrace::sellOriginalWorkshopItem(
                *originalGarage, profileState, workshopDrag.item,
                discount, championshipMode, workshopError))
        {
            std::cerr << "Original WorkshopFrame sell: "
                      << workshopError << '\n';
            return false;
        }
        workshopDrag = {};
#ifdef RRR3D_AUDIO
        playOriginalMenuSound(
            rrr3d::audio::OriginalMenuSound::PickupUp);
#endif
        saveRaceProfile();
        refreshWorkshopPage();
        return true;
    };
    auto activateWorkshopFocus =
        [&](bool pointerSlotPlane) {
            hideWorkshopWeaponDialog();
            constexpr std::size_t firstGoodFocus = 1U;
            constexpr std::size_t visibleGoods = 12U;
            constexpr std::size_t firstSlotFocus =
                firstGoodFocus + visibleGoods;
            if (menuSelection == 0U)
            {
                if (workshopDrag.active())
                    stopWorkshopDrag(false);
                else
                    backMenu();
                return;
            }
            if (menuSelection < firstSlotFocus)
            {
                if (workshopDrag.active())
                {
                    stopWorkshopDrag(true);
                    return;
                }
                const std::size_t itemIndex =
                    workshopGoodScroll * 3U +
                    menuSelection - firstGoodFocus;
                if (itemIndex >= workshopGoods.size())
                    return;
                const auto* item = workshopGoods[itemIndex];
                const bool hasCompatibleSlot = [&]() {
                    for (std::size_t slot = 0U;
                         slot < profileState.player.slots.size();
                         ++slot)
                    {
                        if (workshopSlotAccepts(
                                slot,
                                r3d::game::originalrace::ProfileSlot{
                                    item->record, item->defaultCharge,
                                    item->maximumCharge > 0U}))
                            return true;
                    }
                    return false;
                }();
                if (!hasCompatibleSlot)
                {
                    std::cerr
                        << "Original WorkshopFrame: "
                        << localized("svHintWeaponNotSupport")
                        << '\n';
                    showWorkshopGoodMessage(
                        "svHintWeaponNotSupport");
                    return;
                }
                if (championshipMode)
                {
                    showWorkshopConfirmation(
                        WorkshopConfirmation::Buy, *item,
                        item->cost);
                    return;
                }
                buyWorkshopGood(*item);
                return;
            }
            const std::size_t slotIndex =
                menuSelection - firstSlotFocus;
            if (slotIndex >= profileState.player.slots.size())
                return;
            const auto slotType =
                static_cast<r3d::game::originalrace::
                                GarageSlotType>(slotIndex);
            if (workshopDrag.active())
            {
                if (!workshopSlotAccepts(
                        slotIndex, workshopDrag.item))
                    return;
                r3d::game::originalrace::ProfileSlot replaced;
                std::string workshopError;
                if (!r3d::game::originalrace::
                        installOriginalWorkshopSlot(
                            *originalGarage, profileState, slotType,
                            workshopDrag.item, replaced,
                            workshopError))
                {
                    std::cerr
                        << "Original WorkshopFrame install: "
                        << workshopError << '\n';
                    return;
                }
                if (replaced.record.empty())
                {
                    workshopDrag = {};
#ifdef RRR3D_AUDIO
                    playOriginalMenuSound(
                        rrr3d::audio::OriginalMenuSound::PickupUp);
#endif
                }
                else
                {
                    workshopDrag.item = std::move(replaced);
                    workshopDrag.origin = slotType;
                }
                saveRaceProfile();
                refreshWorkshopPage();
                return;
            }

            auto& installed = profileState.player.slots[slotIndex];
            const auto* item =
                originalGarage->findItem(installed.record);
            if (item == nullptr)
                return;
            if (pointerSlotPlane)
            {
                workshopDrag.item = installed;
                workshopDrag.origin = slotType;
                installed = {};
#ifdef RRR3D_AUDIO
                playOriginalMenuSound(
                    rrr3d::audio::OriginalMenuSound::PickupDown);
#endif
                saveRaceProfile();
                refreshWorkshopPage();
                return;
            }
            std::string workshopError;
            bool changed = false;
#ifdef RRR3D_AUDIO
            // Charge/upgrade controls are RaceMenu::CreatePlusButton and
            // therefore use ssButton3 rather than the generic button sound.
            playOriginalMenuSound(
                rrr3d::audio::OriginalMenuSound::PickupDown);
#endif
            if (item->maximumCharge > 0U)
            {
                changed = r3d::game::originalrace::
                    rechargeOriginalWorkshopItem(
                        *originalGarage, profileState, slotType,
                        championshipMode, workshopError);
            }
            else if (slotIndex < 4U)
            {
                const auto* car = originalGarage->findCar(
                    profileState.player.currentCar);
                const int level =
                    r3d::game::originalrace::
                        originalWorkshopUpgradeLevel(
                            installed.record, slotType);
                const auto* upgrade =
                    car == nullptr
                        ? nullptr
                        : r3d::game::originalrace::
                              originalWorkshopUpgradeItem(
                                  *originalGarage, *car, slotType,
                                  level + 1);
                if (upgrade != nullptr)
                {
                    changed = r3d::game::originalrace::
                        installOriginalWorkshopItem(
                            *originalGarage, profileState, slotType,
                            *upgrade, championshipMode,
                            workshopError);
                }
            }
            if (!changed && !workshopError.empty())
            {
                std::cerr << "Original WorkshopFrame upgrade: "
                          << workshopError << '\n';
                const auto centers = workshopSlotCenters();
                const float senderX =
                    centers[slotIndex][0] +
                    (item->maximumCharge > 0U ? 68.0F : 51.0F);
                const float senderY =
                    centers[slotIndex][1] +
                    (item->maximumCharge > 0U ? 27.0F : 38.0F);
                const float controlWidth =
                    item->maximumCharge > 0U
                        ? static_cast<float>(
                              workshopChargeButtonImage.width) *
                              4.0F
                        : static_cast<float>(
                              workshopUpgradeImages[0].width);
                const float controlHeight =
                    item->maximumCharge > 0U
                        ? static_cast<float>(
                              workshopChargeButtonImage.height) *
                              4.0F
                        : static_cast<float>(
                              workshopUpgradeImages[0].height);
                const float posX =
                    senderX + controlWidth * 0.25F;
                const float posY =
                    senderY - controlHeight * 0.25F;
                showInfoDialog(
                    localized("svWarning"),
                    localized("svHintCantMoney"),
                    localized("svOk"),
                    posX +
                        static_cast<float>(
                            infoDialogFrameImage.width) *
                            0.5F,
                    posY -
                        static_cast<float>(
                            infoDialogFrameImage.height) *
                            0.5F);
            }
            if (changed)
                saveRaceProfile();
            refreshWorkshopPage();
        };
    auto refreshPlanetsPage = [&]() {
        std::vector<std::string> output;
        const auto count = std::min(
            originalGarage->planets.size(),
            profileState.player.planets.size());
        output.reserve(count + 1U);
        const std::size_t nextPlanet =
            std::min<std::size_t>(
                profileState.player.currentPlanet + 1U,
                count);
        for (std::size_t index = 0; index < count; ++index)
        {
            const auto& progress =
                profileState.player.planets[index];
            std::string state;
            if (progress.state == 0U ||
                (racePlanetChampion && index == nextPlanet))
            {
                state = localized("svOpen");
            }
            else if (progress.state == 1U)
            {
                const auto& points =
                    originalGarage->planets[index].requestPoints;
                const auto pass =
                    std::max<std::uint32_t>(progress.pass, 1U);
                const auto required =
                    pass <= points.size() ? points[pass - 1U] : 0U;
                state = localized("svRequestPoints");
                const auto marker = state.find("%d");
                if (marker != std::string::npos)
                {
                    state.replace(
                        marker, 2U, std::to_string(required));
                }
            }
            else if (progress.state == 2U)
            {
                state =
                    championshipMode && index == nextPlanet
                        ? localized("svUnavailableTitulA")
                        : localized("svClosed");
            }
            else
            {
                state = localized("svCompleted");
            }
            output.push_back(std::move(state));
        }
        output.push_back(localized("svBack"));
        auto replacement = createStyledPage(
            std::move(output), menu::smallFontHeight,
            menu::Rgba8{255, 255, 255, 255},
            menu::selectedTextColor);
        destroyPage(planetsPage);
        planetsPage = std::move(replacement);
        menuSelection = std::min(
            menuSelection, planetsPage.labels.size() - 1U);

        std::vector<std::string> info{" ", " ", " "};
        if (angarPlanetIndex >= 0 &&
            static_cast<std::size_t>(angarPlanetIndex) < count)
        {
            const auto& planet = originalGarage->planets[
                static_cast<std::size_t>(angarPlanetIndex)];
            info = {localized(planet.name),
                    localized(planet.bossName)};
            std::istringstream words(localized(planet.info));
            std::string line;
            std::string word;
            while (words >> word)
            {
                if (!line.empty() &&
                    line.size() + word.size() + 1U > 34U)
                {
                    info.push_back(std::move(line));
                    line.clear();
                }
                if (!line.empty())
                    line.push_back(' ');
                line += word;
            }
            if (!line.empty())
                info.push_back(std::move(line));
            if (info.size() > 10U)
                info.resize(10U);
        }
        auto infoReplacement = createStyledPage(
            std::move(info), menu::smallFontHeight,
            menu::Rgba8{118, 206, 242, 255},
            menu::selectedTextColor);
        destroyPage(angarInfoPage);
        angarInfoPage = std::move(infoReplacement);
    };
    auto selectAngarPlanet = [&](int index) {
        if (index == angarPlanetIndex)
            return;
        angarPreviousPlanetIndex = angarPlanetIndex;
        angarPlanetIndex = index;
        angarDoorTime = 0.0F;
#ifdef RRR3D_AUDIO
        if (index >= 0)
        {
            // Planet ViewPort3d registers ssButton5::focused.
            playOriginalMenuSound(
                rrr3d::audio::OriginalMenuSound::ShowPlanet);
        }
#endif
        refreshPlanetsPage();
    };
    auto persistAngarProfile = [&]() {
        saveRaceProfile();
    };
    auto changeAngarPlanet = [&](std::size_t index) {
#ifdef RRR3D_NETWORK
        if (networkClientMatchEntered)
            return;
#endif
        const auto count = std::min(
            originalGarage->planets.size(),
            profileState.player.planets.size());
        if (index >= count)
            return;
        auto& progress = profileState.player.planets[index];
        const bool newPlanet =
            progress.state == 1U || progress.state == 2U;
        if (progress.state == 1U || progress.state == 2U)
        {
            // Planet::Unlock followed by Tournament::ChangePlanet/Open.
            progress.state = 0U;
            progress.pass = 1U;
        }
        profileState.player.currentPlanet =
            static_cast<std::uint32_t>(index);
        profileState.player.currentPass =
            std::max<std::uint32_t>(progress.pass, 1U);
        profileState.player.currentTrack = 0U;
        selectedTrack =
            r3d::game::originalrace::resolveOriginalTournamentTrack(
                *originalRace, profileState.player);
        raceTournamentAdvance = {};
        racePlanetChampion = false;
        persistAngarProfile();
#ifdef RRR3D_NETWORK
        if (networkMatchStarted && networkHostRequested)
        {
            if (!reloadCurrentRace())
            {
                runtimeSmokeFailed = true;
                running = false;
                return;
            }
            const auto& entry =
                originalRace->trackCatalog[selectedTrack];
            const auto track = static_cast<std::int32_t>(
                std::count_if(
                    originalRace->trackCatalog.begin(),
                    originalRace->trackCatalog.begin() +
                        static_cast<std::ptrdiff_t>(selectedTrack),
                    [&](const auto& candidate) {
                        return candidate.planetIndex ==
                               entry.planetIndex;
                    }));
            std::string networkError;
            if (!networkSession.setPlanet(
                    static_cast<std::int32_t>(entry.planetIndex),
                    track,
                    static_cast<std::int32_t>(
                        originalRace->environment.weather),
                    networkError))
            {
                std::cerr
                    << "Original NetRace::ChangePlanet failed: "
                    << networkError << '\n';
                runtimeSmokeFailed = true;
                running = false;
                return;
            }
            renderedNetworkRevision =
                std::numeric_limits<std::uint64_t>::max();
            refreshNetworkRuntimePages();
        }
#endif
        angarTravelDialogVisible = false;
#ifdef RRR3D_VIDEO
        if (newPlanet && championshipMode &&
            !profileState.config.disableVideo)
        {
            const std::string movie =
                "Data/Video/" +
                originalGarage->planets[index].name +
                (activeLanguage == "russian"
                     ? ".avi"
                     : "_eng.avi");
            if (playOriginalMovie(
                    movie,
                    OriginalMovieCompletion::RaceMenu))
            {
                return;
            }
        }
#endif
        backMenu();
    };
    auto requestAngarTravel =
        [&](std::size_t index, bool fromPlanetSlot = true) {
#ifdef RRR3D_NETWORK
        if (networkClientMatchEntered)
            return;
#endif
        angarTravelTarget = index;
        angarTravelYesFocused = true;
        const auto key =
            index == profileState.player.currentPlanet
                ? "svYouReadyStayPlanet"
                : "svYouReadyFlyPlanet";
        angarTravelDialogVisible = true;
        float posX = menu::virtualWidth * 0.5F;
        float posY = menu::virtualHeight * 0.5F;
        if (fromPlanetSlot)
        {
            const float panelCenterY =
                menu::virtualHeight -
                static_cast<float>(
                    angarBottomPanelImage.height) *
                    0.5F -
                20.0F;
            posX =
                menu::virtualWidth * 0.5F -
                static_cast<float>(
                    angarBottomPanelImage.width) *
                    0.5F +
                125.0F +
                static_cast<float>(index) * 224.0F;
            posY =
                panelCenterY -
                static_cast<float>(
                    angarBottomPanelImage.height) *
                    0.5F +
                90.0F -
                static_cast<float>(
                    angarDoorSlotImage.height) *
                    0.5F;
        }
        showAcceptDialog(
            localized(key), localized("svYes"),
            localized("svNo"), posX,
            posY -
                static_cast<float>(
                    acceptFrameImage.height) *
                    0.5F);
    };
    auto refreshAchievementsPage = [&]() {
        std::vector<std::string> prices;
        prices.reserve(originalAchievementVisuals.size());
        for (const auto& visual : originalAchievementVisuals)
        {
            const auto item =
                profileState.achievementItems.find(
                    std::string(visual.name));
            if (item == profileState.achievementItems.end())
            {
                prices.emplace_back(" ");
                continue;
            }
            const auto state = item->second.values.find("state");
            const auto price = item->second.values.find("price");
            prices.push_back(
                state != item->second.values.end() &&
                        state->second == "asUnlocked" &&
                        price != item->second.values.end()
                    ? originalCurrency(
                          static_cast<std::uint32_t>(
                              std::max(
                                  std::strtol(
                                      price->second.c_str(), nullptr, 10),
                                  0L)))
                    : " ");
        }
        auto replacement = createStyledPage(
            std::move(prices), menu::smallFontHeight,
            menu::Rgba8{195, 194, 192, 255},
            menu::Rgba8{195, 194, 192, 255});
        destroyPage(achievementPricePage);
        achievementPricePage = std::move(replacement);
        auto pointsReplacement = createText(
            *device,
            localized("svPoints") + " " +
                originalCurrency(profileState.achievementPoints),
            menu::headerFontHeight, false,
            menu::Rgba8{250, 88, 0, 255}, resolvedFont);
        device->destroy(achievementPoints.texture);
        achievementPoints = pointsReplacement;
        menuSelection =
            std::min(menuSelection, originalAchievementBack);
    };
    auto achievementState =
        [&](std::size_t index) -> std::string_view {
            if (index >= originalAchievementVisuals.size())
                return {};
            const auto item =
                profileState.achievementItems.find(
                    std::string(
                        originalAchievementVisuals[index].name));
            if (item == profileState.achievementItems.end())
                return {};
            const auto state = item->second.values.find("state");
            return state == item->second.values.end()
                       ? std::string_view{}
                       : std::string_view(state->second);
        };
    auto achievementPrice = [&](std::size_t index) {
        std::uint32_t price = 0U;
        if (index >= originalAchievementVisuals.size())
            return price;
        const auto item =
            profileState.achievementItems.find(
                std::string(originalAchievementVisuals[index].name));
        if (item == profileState.achievementItems.end())
            return price;
        const auto value = item->second.values.find("price");
        if (value == item->second.values.end())
            return price;
        const auto parsed = std::from_chars(
            value->second.data(),
            value->second.data() + value->second.size(), price);
        return parsed.ec == std::errc{} ? price : 0U;
    };
    auto achievementFocusable = [&](std::size_t index) {
        return index == originalAchievementBack ||
               achievementState(index) == "asUnlocked";
    };
    auto moveAchievementFocus = [&](std::size_t direction) {
        if (direction >= 4U ||
            menuSelection > originalAchievementBack)
            return;
        std::size_t next =
            originalAchievementNavigation[menuSelection][direction];
        for (std::size_t attempts = 0U;
             attempts < originalAchievementNavigation.size();
             ++attempts)
        {
            if (next == originalAchievementNoTarget)
                return;
            if (achievementFocusable(next))
            {
                menuSelection = next;
                return;
            }
            next = originalAchievementNavigation[next][direction];
        }
    };
    auto showOriginalRaceMenu = [&]() {
#ifdef RRR3D_NETWORK
        if (networkHostRequested && !startHostedNetworkMatch())
        {
            runtimeSmokeFailed = true;
            running = false;
            return;
        }
#endif
        refreshRaceMainPages();
        menuStack.push_back(MenuScreen::RaceMenu);
        menuSelection = 0;
#ifdef RRR3D_NETWORK
        refreshNetworkRacePlayerVisuals();
#endif
    };
#ifdef RRR3D_NETWORK
    auto enterConnectedNetworkMatch = [&]() {
        using SessionState =
            r3d::game::originalnetwork::SessionState;
        if (networkHostRequested || networkClientMatchEntered ||
            networkSnapshot.state != SessionState::Connected ||
            !networkSnapshot.models.matchActive)
        {
            return true;
        }

        const auto owner = std::find_if(
            networkSnapshot.models.players.begin(),
            networkSnapshot.models.players.end(),
            [](const auto& player) { return player.owner; });
        if (owner == networkSnapshot.models.players.end())
            return true;

        const auto& match = networkSnapshot.models.match;
        championshipMode = match.mode == 0;
        profileState.config.upgradeMaxLevel = static_cast<std::uint32_t>(
            std::clamp(match.upgradeMaxLevel, 0, 2));
        profileState.config.weaponMaxLevel = static_cast<std::uint32_t>(
            std::clamp(match.weaponMaxLevel, 1, 4));
        profileState.config.lapsCount =
            std::clamp<std::uint32_t>(match.lapsCount, 1U, 8U);
        profileState.config.maxPlayers =
            std::clamp<std::uint32_t>(match.maxPlayers, 2U, 6U);
        profileState.config.maxComputers =
            std::min<std::uint32_t>(match.maxComputers, 5U);
        profileState.config.springBorders = match.springBorders;
        profileState.config.enableMineBug = match.enableMineBug;

        auto decodedProfile = profileState.player;
        std::string error;
        if (!r3d::game::originalrace::
                deserializeOriginalNetworkProfile(
                    match.profileXml, championshipMode,
                    decodedProfile, error))
        {
            std::cerr << "Original NetRace::ReadMatch profile failed: "
                      << error << '\n';
            return false;
        }
        profileState.player.difficulty = decodedProfile.difficulty;
        if (championshipMode)
        {
            profileState.player.carChanged = decodedProfile.carChanged;
            profileState.player.minimumDifficulty =
                decodedProfile.minimumDifficulty;
            profileState.player.planets = decodedProfile.planets;
            profileState.player.currentPass = decodedProfile.currentPass;
        }

        const auto requestedPlanet =
            static_cast<std::uint32_t>(std::max(match.planet, 0));
        const auto requestedTrack =
            static_cast<std::uint32_t>(std::max(match.track, 0));
        std::uint32_t localTrack = 0U;
        bool trackFound = false;
        for (std::size_t index = 0U;
             index < originalRace->trackCatalog.size(); ++index)
        {
            const auto& entry = originalRace->trackCatalog[index];
            if (entry.planetIndex != requestedPlanet)
                continue;
            if (localTrack == requestedTrack)
            {
                selectedTrack = index;
                trackFound = true;
                break;
            }
            ++localTrack;
        }
        if (!trackFound)
        {
            std::cerr << "Original NetRace::ReadMatch invalid planet/track: "
                      << match.planet << '/' << match.track << '\n';
            return false;
        }

        if (match.weather < 0 || match.weather > 6)
        {
            std::cerr << "Original NetRace::ReadMatch invalid weather: "
                      << match.weather << '\n';
            return false;
        }
        networkWeatherOverride =
            static_cast<r3d::game::originalrace::Weather>(match.weather);

        if (!owner->car.empty())
        {
            profileState.player.currentCar = owner->car;
            profileState.player.gamerId = static_cast<std::uint32_t>(
                std::max(owner->gamerId, 0));
            profileState.player.networkSlot = owner->netSlot;
            profileState.player.color = owner->color;
            profileState.player.money = static_cast<std::uint32_t>(
                std::max(owner->money, 0));
            for (std::size_t index = 0U;
                 index < profileState.player.slots.size(); ++index)
            {
                profileState.player.slots[index].record =
                    owner->slots[index].record;
                profileState.player.slots[index].charge =
                    owner->slots[index].chargeCount;
                profileState.player.slots[index].hasCharge =
                    !owner->slots[index].record.empty();
            }
        }

        if (!reloadCurrentRace())
            return false;
        networkAppliedPlanet = match.planet;
        networkAppliedTrack = match.track;
        networkAppliedWeather = match.weather;
        networkMatchStarted = true;
        networkClientMatchEntered = true;
        networkLocalReadyPublished = owner->raceReady;
        networkLocalCarSelected = !owner->car.empty();
        networkPublishedPlayer = *owner;
        networkLastIdentityEventSequence =
            networkSnapshot.models.events.empty()
                ? 0U
                : networkSnapshot.models.events.back().sequence;
        networkLastLifecycleEventSequence =
            networkLastIdentityEventSequence;
        networkPendingGamerId.reset();
        hideInfoDialog();
        networkFailureDialogAction =
            NetworkFailureDialogAction::None;
        menuStack = {MenuScreen::Main};
        if (owner->car.empty())
            showOriginalGamers();
        else
            showOriginalRaceMenu();
        std::cout
            << "Original MainMenu::OnConnectedPlayer -> MatchConnected: "
            << "planet=" << match.planet << ", track=" << match.track
            << ", ownerModel=" << owner->modelId << '\n';
        return true;
    };
#endif
    auto refreshCurrentOptionsPage = [&]() {
        switch (menuStack.back())
        {
        case MenuScreen::GameOptions:
            replaceOptionsPage(
                gameOptionsPage, gameOptionsLabels());
#ifdef RRR3D_NETWORK
            if (networkMatchStarted && !networkHostRequested)
            {
                // OptionsMenu::GameFrame::LoadCfg disables every setting
                // owned by NetRace when this peer is not the host. Camera,
                // HUD and video remain local exactly as in Menu.cpp.
                for (std::size_t index = 3U; index <= 10U; ++index)
                    gameOptionsPage.enabled[index] = false;
                if (menuSelection < gameOptionsPage.enabled.size() &&
                    !gameOptionsPage.enabled[menuSelection])
                {
                    menuSelection = 2U;
                }
            }
#endif
            break;
        case MenuScreen::GraphicsOptions:
            replaceOptionsPage(
                graphicsOptionsPage, graphicsOptionsLabels());
            break;
        case MenuScreen::SoundOptions:
            replaceOptionsPage(
                soundOptionsPage, soundOptionsLabels());
            break;
        case MenuScreen::ControlsOptions:
            replaceOptionsPage(
                controlsOptionsPage, controlsOptionsLabels());
            replaceOptionsPage(
                controlsKeyboardValuesPage,
                controlValues(false));
            replaceOptionsPage(
                controlsGamepadValuesPage,
                controlValues(true));
            break;
        default:
            break;
        }
    };
#ifdef RRR3D_NETWORK
    auto synchronizeReplicatedNetworkOptions = [&]() {
        if (!networkClientMatchEntered ||
            !networkSnapshot.models.matchActive)
        {
            return;
        }

        const auto& match = networkSnapshot.models.match;
        const bool selectionChanged =
            match.planet != networkAppliedPlanet ||
            match.track != networkAppliedTrack ||
            match.weather != networkAppliedWeather;
        if (selectionChanged && !inRace && !raceLoadingActive)
        {
            const auto requestedPlanet = static_cast<std::uint32_t>(
                std::max(match.planet, 0));
            const auto requestedTrack = static_cast<std::uint32_t>(
                std::max(match.track, 0));
            std::uint32_t localTrack = 0U;
            bool trackFound = false;
            for (std::size_t index = 0U;
                 index < originalRace->trackCatalog.size(); ++index)
            {
                const auto& entry = originalRace->trackCatalog[index];
                if (entry.planetIndex != requestedPlanet)
                    continue;
                if (localTrack == requestedTrack)
                {
                    selectedTrack = index;
                    trackFound = true;
                    break;
                }
                ++localTrack;
            }
            if (!trackFound || match.weather < 0 || match.weather > 6)
            {
                std::cerr
                    << "Original NetRace::OnSetPlanet invalid "
                       "planet/track/weather: "
                    << match.planet << '/' << match.track << '/'
                    << match.weather << '\n';
                runtimeSmokeFailed = true;
                running = false;
                return;
            }
            networkWeatherOverride =
                static_cast<r3d::game::originalrace::Weather>(
                    match.weather);
            if (!reloadCurrentRace())
            {
                runtimeSmokeFailed = true;
                running = false;
                return;
            }
            networkAppliedPlanet = match.planet;
            networkAppliedTrack = match.track;
            networkAppliedWeather = match.weather;
            refreshRaceMainPages();
        }
        const auto upgrade = static_cast<std::uint32_t>(
            std::clamp(match.upgradeMaxLevel, 0, 2));
        const auto weapons = static_cast<std::uint32_t>(
            std::clamp(match.weaponMaxLevel, 1, 4));
        const auto laps =
            std::clamp<std::uint32_t>(match.lapsCount, 1U, 8U);
        const auto players =
            std::clamp<std::uint32_t>(match.maxPlayers, 2U, 6U);
        const auto computers =
            std::min<std::uint32_t>(match.maxComputers, 5U);
        const auto difficulty =
            networkSnapshot.models.currentDifficultySet
                ? std::string(sourceDifficultyName(
                      networkSnapshot.models.currentDifficulty))
                : profileState.player.difficulty;
        const bool changed =
            profileState.config.upgradeMaxLevel != upgrade ||
            profileState.config.weaponMaxLevel != weapons ||
            profileState.config.lapsCount != laps ||
            profileState.config.maxPlayers != players ||
            profileState.config.maxComputers != computers ||
            profileState.config.springBorders != match.springBorders ||
            profileState.config.enableMineBug != match.enableMineBug ||
            profileState.player.difficulty != difficulty;
        if (!changed)
            return;

        profileState.config.upgradeMaxLevel = upgrade;
        profileState.config.weaponMaxLevel = weapons;
        profileState.config.lapsCount = laps;
        profileState.config.maxPlayers = players;
        profileState.config.maxComputers = computers;
        profileState.config.springBorders = match.springBorders;
        profileState.config.enableMineBug = match.enableMineBug;
        profileState.player.difficulty = difficulty;
        optionsDraftConfig.upgradeMaxLevel = upgrade;
        optionsDraftConfig.weaponMaxLevel = weapons;
        optionsDraftConfig.lapsCount = laps;
        optionsDraftConfig.maxPlayers = players;
        optionsDraftConfig.maxComputers = computers;
        optionsDraftConfig.springBorders = match.springBorders;
        optionsDraftConfig.enableMineBug = match.enableMineBug;
        optionsDraftDifficulty = difficulty;
        raceSession.setSpringBorders(match.springBorders);
        raceSession.setEnableMineBug(match.enableMineBug);
        if (menuStack.back() == MenuScreen::GameOptions ||
            menuStack.back() == MenuScreen::GraphicsOptions ||
            menuStack.back() == MenuScreen::SoundOptions ||
            menuStack.back() == MenuScreen::ControlsOptions)
            refreshCurrentOptionsPage();
    };
#endif
    auto isOriginalOptionsScreen = [](MenuScreen screen) {
        return screen == MenuScreen::GameOptions ||
               screen == MenuScreen::GraphicsOptions ||
               screen == MenuScreen::SoundOptions ||
               screen == MenuScreen::ControlsOptions;
    };
    auto optionsStateIndex = [](MenuScreen screen) {
        switch (screen)
        {
        case MenuScreen::GraphicsOptions:
            return 1U;
        case MenuScreen::SoundOptions:
            return 2U;
        case MenuScreen::ControlsOptions:
            return 3U;
        default:
            return 0U;
        }
    };
    auto refreshStartOptionsValues = [&]() {
        auto replacement = createStyledPage(
            startOptionsValues(), menu::smallFontHeight,
            optionsTextColor, menu::selectedTextColor);
        destroyPage(startOptionsValuePage);
        startOptionsValuePage = std::move(replacement);
    };
    auto cycleStartOptionsIndex = [](std::size_t index,
                                     std::size_t count,
                                     int direction) {
        if (count == 0U)
            return std::size_t{0};
        if (direction < 0)
            return index == 0U ? count - 1U : index - 1U;
        return (index + 1U) % count;
    };
    auto adjustStartOption = [&](int direction) {
#ifdef RRR3D_AUDIO
        playOriginalMenuSound(
            rrr3d::audio::OriginalMenuSound::ChangeOption);
#endif
        switch (startOptionsFocus)
        {
        case 0U:
            // StartOptionsMenu::OnSelect always resolves the initial
            // cPrefCameraEnd/"Select" sentinel to pcIsometric.  Subsequent
            // arrow presses loop between the two real values.
            if (startOptionsCameraIndex >= 2U)
                startOptionsCameraIndex = 1U;
            else
                startOptionsCameraIndex = cycleStartOptionsIndex(
                    startOptionsCameraIndex, 2U, direction);
            startOptionsApplyEnabled = true;
            break;
        case 1U:
            startOptionsResolutionIndex = cycleStartOptionsIndex(
                startOptionsResolutionIndex,
                originalDisplayModes.size(), direction);
            break;
        case 2U:
            startOptionsLanguageIndex = cycleStartOptionsIndex(
                startOptionsLanguageIndex,
                sourceLanguages.size(), direction);
            break;
        case 3U:
            startOptionsCommentatorIndex = cycleStartOptionsIndex(
                startOptionsCommentatorIndex,
                sourceCommentators.size(), direction);
            break;
        default:
            return;
        }
        refreshStartOptionsValues();
    };
    auto finishStartOptions = [&]() {
        sourceStartOptionsActive = false;
        startOptionsReloadDialogPending = false;
        startOptionsMainTransitionObserved = true;
        if (sourceDiscreteVideoChanged)
        {
            // CheckStartupMenu's Apple-Silicon branch: a capable current GPU
            // retains the original fixed-frame scheduling mode.
            profileState.config.quality.frameRateMode = "sfrFixed";
            sourceDiscreteVideoChanged = false;
        }
        const auto& resolution =
            originalDisplayModes[startOptionsResolutionIndex];
        std::cout
            << "Original StartOptionsMenu -> MainMenu2: camera="
            << (startOptionsCameraIndex == 0U
                    ? "pcThirdPerson"
                    : "pcIsometric")
            << ", resolution=" << resolution.first << 'x'
            << resolution.second << ", language="
            << sourceLanguages[startOptionsLanguageIndex]
            << ", commentator="
            << sourceCommentators[startOptionsCommentatorIndex]
            << '\n';
    };
    auto applyStartOptions = [&]() {
        if (!startOptionsApplyEnabled ||
            startOptionsCameraIndex >= 2U)
        {
            return;
        }
        const auto previous = profileState.config;
        const auto& resolution =
            originalDisplayModes[startOptionsResolutionIndex];
        profileState.config.preferredCamera =
            startOptionsCameraIndex == 0U
                ? r3d::game::originalrace::
                      PreferredCamera::ThirdPerson
                : r3d::game::originalrace::
                      PreferredCamera::Isometric;
        profileState.config.resolutionWidth = resolution.first;
        profileState.config.resolutionHeight = resolution.second;
        profileState.config.language =
            sourceLanguages[startOptionsLanguageIndex];
        profileState.config.commentatorStyle =
            sourceCommentators[startOptionsCommentatorIndex];
        profileState.config.discreteVideoCard =
            sourceCurrentDiscreteVideoCard;
        profileState.preferredCameraSerialized = true;
        profileState.discreteVideoCardSerialized = true;
        raceRenderer.resetCamera();
        if (!profileState.config.fullScreen &&
            (profileState.config.resolutionWidth !=
                 previous.resolutionWidth ||
             profileState.config.resolutionHeight !=
                 previous.resolutionHeight))
        {
            SDL_SetWindowSize(
                window,
                static_cast<int>(
                    profileState.config.resolutionWidth),
                static_cast<int>(
                    profileState.config.resolutionHeight));
        }
#ifdef RRR3D_AUDIO
        if (profileState.config.commentatorStyle !=
            previous.commentatorStyle)
        {
            commentator.shutdown();
            if (!commentator.initialize(
                    profileState.config.commentatorStyle,
                    audioError))
            {
                std::cerr
                    << "Unable to apply StartOptionsMenu commentator: "
                    << audioError << '\n';
            }
        }
#endif
        startOptionsCameraAppliedObserved =
            profileState.config.preferredCamera ==
            r3d::game::originalrace::PreferredCamera::Isometric;
        if (options->startOptionsSmokeTest)
        {
            const auto smokeDirectory =
                std::filesystem::temp_directory_path() /
                ("rrr3d-start-options-smoke-" +
                 std::to_string(reinterpret_cast<std::uintptr_t>(
                     &profileState)));
            std::error_code fileError;
            std::filesystem::remove_all(smokeDirectory, fileError);
            r3d::game::originalrace::OriginalProfileStore smokeStore(
                smokeDirectory);
            std::string smokeError;
            if (smokeStore.save(profileState, smokeError))
            {
                std::string smokeWarning;
                const auto reloaded = smokeStore.load(smokeWarning);
                startOptionsSavedObserved =
                    smokeWarning.empty() &&
                    reloaded.preferredCameraSerialized &&
                    reloaded.discreteVideoCardSerialized &&
                    reloaded.config.preferredCamera ==
                        profileState.config.preferredCamera;
            }
            else
            {
                std::cerr
                    << "StartOptionsMenu smoke save failed: "
                    << smokeError << '\n';
            }
            std::filesystem::remove_all(smokeDirectory, fileError);
            profileState.config = startOptionsConfigBefore;
            profileState.preferredCameraSerialized = true;
            profileState.discreteVideoCardSerialized = true;
        }
        else
        {
            saveRaceProfile();
            startOptionsSavedObserved = true;
        }
        if (!options->startOptionsSmokeTest &&
            profileState.config.language != previous.language)
        {
            // StartOptionsMenu::OnClick keeps the modal frame alive until
            // the original reload warning is acknowledged.
            showInfoDialog(
                localized("svWarning"),
                localized("svHintNeedReload"),
                localized("svOk"),
                menu::virtualWidth * 0.5F,
                menu::virtualHeight * 0.5F);
            startOptionsReloadDialogPending = true;
        }
        else
        {
            finishStartOptions();
        }
    };
    auto setOptionsState = [&](std::size_t state) {
        menuStack.back() =
            std::array{
                MenuScreen::GameOptions,
                MenuScreen::GraphicsOptions,
                MenuScreen::SoundOptions,
                MenuScreen::ControlsOptions}
                [std::min<std::size_t>(state, 3U)];
        menuSelection = 0U;
        bindingCaptureAction.reset();
        refreshCurrentOptionsPage();
    };
    auto beginOriginalOptions = [&]() {
        optionsDraftConfig = profileState.config;
        optionsDraftDifficulty = profileState.player.difficulty;
        controlsUseGamepad = false;
        bindingCaptureAction.reset();
        pushMenu(MenuScreen::GameOptions);
        refreshCurrentOptionsPage();
    };
    auto cancelOriginalOptions = [&]() {
#ifdef RRR3D_AUDIO
        audio.setBusVolume(
            r3d::audio::Bus::Music,
            profileState.config.musicVolume);
        audio.setBusVolume(
            r3d::audio::Bus::Effects,
            profileState.config.effectsVolume);
        audio.setBusVolume(
            r3d::audio::Bus::Voice,
            profileState.config.voiceVolume);
#endif
        optionsDraftConfig = profileState.config;
        optionsDraftDifficulty = profileState.player.difficulty;
        bindingCaptureAction.reset();
        backMenu();
    };
    auto applyOriginalOptions = [&]() {
        const auto previousConfig = profileState.config;
        profileState.config = optionsDraftConfig;
        profileState.player.difficulty = optionsDraftDifficulty;

        if (profileState.config.fullScreen !=
            previousConfig.fullScreen)
        {
            if (!SDL_SetWindowFullscreen(
                    window, profileState.config.fullScreen))
            {
                std::cerr
                    << "Unable to apply original window mode: "
                    << SDL_GetError() << '\n';
                profileState.config.fullScreen =
                    previousConfig.fullScreen;
                optionsDraftConfig.fullScreen =
                    previousConfig.fullScreen;
            }
        }
        if (!profileState.config.fullScreen &&
            (profileState.config.resolutionWidth !=
                 previousConfig.resolutionWidth ||
             profileState.config.resolutionHeight !=
                 previousConfig.resolutionHeight))
        {
            SDL_SetWindowSize(
                window,
                static_cast<int>(
                    profileState.config.resolutionWidth),
                static_cast<int>(
                    profileState.config.resolutionHeight));
        }
        raceRenderer.resetCamera();
        raceSession.setSpringBorders(
            profileState.config.springBorders);
        raceSession.setEnableMineBug(
            profileState.config.enableMineBug);
#ifdef RRR3D_NETWORK
        if (networkMatchStarted && networkHostRequested)
        {
            std::string networkError;
            bool published = true;
            auto publishOption =
                [&](bool result, std::string_view sourceFunction) {
                    if (result)
                        return;
                    published = false;
                    std::cerr << "Original NetRace::"
                              << sourceFunction << " failed: "
                              << networkError << '\n';
                };
            publishOption(
                networkSession.setUpgradeMaxLevel(
                    static_cast<std::int32_t>(
                        profileState.config.upgradeMaxLevel),
                    networkError),
                "SetUpgradeMaxLevel");
            publishOption(
                networkSession.setWeaponMaxLevel(
                    static_cast<std::int32_t>(
                        profileState.config.weaponMaxLevel),
                    networkError),
                "SetWeaponMaxLevel");
            publishOption(
                networkSession.setCurrentDifficulty(
                    sourceDifficultyIndex(
                        profileState.player.difficulty),
                    networkError),
                "SetCurrentDifficulty");
            publishOption(
                networkSession.setLapsCount(
                    profileState.config.lapsCount, networkError),
                "SetLapsCount");
            publishOption(
                networkSession.setMaxPlayers(
                    profileState.config.maxPlayers, networkError),
                "SetMaxPlayers");
            publishOption(
                networkSession.setMaxComputers(
                    profileState.config.maxComputers, networkError),
                "SetMaxComputers");
            publishOption(
                networkSession.setSpringBorders(
                    profileState.config.springBorders, networkError),
                "SetSpringBorders");
            publishOption(
                networkSession.setEnableMineBug(
                    profileState.config.enableMineBug, networkError),
                "SetEnableMineBug");
            if (published)
            {
                renderedNetworkRevision =
                    std::numeric_limits<std::uint64_t>::max();
                refreshNetworkRuntimePages();
                std::cout
                    << "Original OptionsMenu::ApplyChanges -> "
                       "NetRace host options\n";
            }
        }
#endif
#ifdef RRR3D_AUDIO
        audio.setBusVolume(
            r3d::audio::Bus::Music,
            profileState.config.musicVolume);
        audio.setBusVolume(
            r3d::audio::Bus::Effects,
            profileState.config.effectsVolume);
        audio.setBusVolume(
            r3d::audio::Bus::Voice,
            profileState.config.voiceVolume);
        if (profileState.config.commentatorStyle !=
            previousConfig.commentatorStyle)
        {
            commentator.shutdown();
            if (!commentator.initialize(
                    profileState.config.commentatorStyle,
                    audioError))
            {
                std::cerr
                    << "Unable to switch original commentator: "
                    << audioError << '\n';
            }
        }
#endif
#ifdef RRR3D_GAMEPAD_INPUT
        input.applyKeyboardBindings(
            profileState.config.keyboardControls);
        input.applyGamepadBindings(
            profileState.config.gamepadControls);
#endif
        saveRaceProfile();
        bindingCaptureAction.reset();
        backMenu();
    };
    auto cycleValue = [](std::uint32_t value,
                         std::uint32_t count, int direction) {
        if (count == 0U)
            return 0U;
        const int normalized =
            (static_cast<int>(value % count) + direction +
             static_cast<int>(count)) %
            static_cast<int>(count);
        return static_cast<std::uint32_t>(normalized);
    };
    auto adjustCurrentOption = [&](int direction) {
        direction = direction < 0 ? -1 : 1;
        const auto& optionPage = activeMenuPage();
        if (menuSelection >= optionPage.enabled.size() ||
            !optionPage.enabled[menuSelection])
        {
            return;
        }
#ifdef RRR3D_AUDIO
        // Options steppers and volume bars emit SoundSheme::selectItem.
        playOriginalMenuSound(
            rrr3d::audio::OriginalMenuSound::ChangeOption);
#endif
        switch (menuStack.back())
        {
        case MenuScreen::GameOptions:
            switch (menuSelection)
            {
            case 0:
                optionsDraftConfig.preferredCamera =
                    optionsDraftConfig.preferredCamera ==
                            r3d::game::originalrace::
                                PreferredCamera::ThirdPerson
                        ? r3d::game::originalrace::
                              PreferredCamera::Isometric
                        : r3d::game::originalrace::
                              PreferredCamera::ThirdPerson;
                break;
            case 1:
                optionsDraftConfig.cameraDistance =
                    std::clamp(
                        optionsDraftConfig.cameraDistance +
                            static_cast<float>(direction) * 0.25F,
                        1.0F, 2.0F);
                break;
            case 2:
                optionsDraftConfig.enableHud =
                    !optionsDraftConfig.enableHud;
                break;
            case 3: {
                static constexpr std::array<std::string_view, 3>
                    difficulties{
                        "gdEasy", "gdNormal", "gdHard"};
                const auto found = std::find(
                    difficulties.begin(), difficulties.end(),
                    optionsDraftDifficulty);
                const auto index =
                    found == difficulties.end()
                        ? 1U
                        : static_cast<std::uint32_t>(
                              found - difficulties.begin());
                optionsDraftDifficulty =
                    difficulties[cycleValue(index, 3U, direction)];
                break;
            }
            case 4:
                optionsDraftConfig.springBorders =
                    !optionsDraftConfig.springBorders;
                break;
            case 5:
                optionsDraftConfig.upgradeMaxLevel =
                    cycleValue(
                        optionsDraftConfig.upgradeMaxLevel,
                        3U, direction);
                break;
            case 6:
                optionsDraftConfig.weaponMaxLevel =
                    cycleValue(
                        std::clamp(
                            optionsDraftConfig.weaponMaxLevel,
                            1U, 4U) -
                            1U,
                        4U, direction) +
                    1U;
                break;
            case 7:
                optionsDraftConfig.maxPlayers =
                    cycleValue(
                        std::clamp(
                            optionsDraftConfig.maxPlayers,
                            2U, 6U) -
                            2U,
                        5U, direction) +
                    2U;
                break;
            case 8:
                optionsDraftConfig.maxComputers =
                    cycleValue(
                        optionsDraftConfig.maxComputers,
                        6U, direction);
                break;
            case 9:
                optionsDraftConfig.lapsCount =
                    std::clamp(
                        static_cast<int>(
                            optionsDraftConfig.lapsCount) +
                            direction,
                        1, 8);
                break;
            case 10:
                optionsDraftConfig.enableMineBug =
                    !optionsDraftConfig.enableMineBug;
                break;
            case 11:
                optionsDraftConfig.disableVideo =
                    !optionsDraftConfig.disableVideo;
                break;
            default:
                return;
            }
            break;
        case MenuScreen::GraphicsOptions:
            switch (menuSelection)
            {
            case 0: {
                const auto current = std::find(
                    originalDisplayModes.begin(),
                    originalDisplayModes.end(),
                    std::pair{
                        optionsDraftConfig.resolutionWidth,
                        optionsDraftConfig.resolutionHeight});
                const auto index =
                    current == originalDisplayModes.end()
                        ? 0U
                        : static_cast<std::uint32_t>(
                              current - originalDisplayModes.begin());
                const auto& mode = originalDisplayModes[cycleValue(
                    index,
                    static_cast<std::uint32_t>(
                        originalDisplayModes.size()),
                    direction)];
                optionsDraftConfig.resolutionWidth = mode.first;
                optionsDraftConfig.resolutionHeight = mode.second;
                break;
            }
            case 1:
                optionsDraftConfig.quality.filtering =
                    cycleValue(
                        optionsDraftConfig.quality.filtering,
                        4U, direction);
                break;
            case 2:
                optionsDraftConfig.quality.msaa =
                    cycleValue(
                        optionsDraftConfig.quality.msaa,
                        3U, direction);
                break;
            case 3:
                optionsDraftConfig.quality.shadow =
                    cycleValue(
                        optionsDraftConfig.quality.shadow,
                        3U, direction);
                break;
            case 4:
                optionsDraftConfig.quality.environment =
                    cycleValue(
                        optionsDraftConfig.quality.environment,
                        3U, direction);
                break;
            case 5:
                optionsDraftConfig.quality.light =
                    cycleValue(
                        optionsDraftConfig.quality.light,
                        3U, direction);
                break;
            case 6:
                optionsDraftConfig.quality.postEffect =
                    cycleValue(
                        optionsDraftConfig.quality.postEffect,
                        3U, direction);
                break;
            case 7:
                optionsDraftConfig.fullScreen =
                    !optionsDraftConfig.fullScreen;
                break;
            default:
                return;
            }
            break;
        case MenuScreen::SoundOptions:
            switch (menuSelection)
            {
            case 0:
                optionsDraftConfig.language =
                    optionsDraftConfig.language == "russian"
                        ? "english"
                        : "russian";
                break;
            case 1:
                optionsDraftConfig.commentatorStyle =
                    optionsDraftConfig.commentatorStyle == "russian"
                        ? "english"
                        : "russian";
                break;
            case 2:
                optionsDraftConfig.musicVolume =
                    std::clamp(
                        optionsDraftConfig.musicVolume +
                            static_cast<float>(direction) * 0.1F,
                        0.0F, 2.0F);
#ifdef RRR3D_AUDIO
                audio.setBusVolume(
                    r3d::audio::Bus::Music,
                    optionsDraftConfig.musicVolume);
#endif
                break;
            case 3:
                optionsDraftConfig.effectsVolume =
                    std::clamp(
                        optionsDraftConfig.effectsVolume +
                            static_cast<float>(direction) * 0.1F,
                        0.0F, 2.0F);
#ifdef RRR3D_AUDIO
                audio.setBusVolume(
                    r3d::audio::Bus::Effects,
                    optionsDraftConfig.effectsVolume);
#endif
                break;
            case 4:
                optionsDraftConfig.voiceVolume =
                    std::clamp(
                        optionsDraftConfig.voiceVolume +
                            static_cast<float>(direction) * 0.1F,
                        0.0F, 2.0F);
#ifdef RRR3D_AUDIO
                audio.setBusVolume(
                    r3d::audio::Bus::Voice,
                    optionsDraftConfig.voiceVolume);
#endif
                break;
            default:
                return;
            }
            break;
        default:
            return;
        }
        refreshCurrentOptionsPage();
    };
    auto showFinishMenu = [&](bool persistProgress = true) {
        if (finishMenuShown || raceSession.racers().empty())
            return;
        finishMenuShown = true;
        finishAnimationSeconds = 0.0F;
        finishVoiceIndex = 0U;
        finishLastVoiceDispatched = false;
        if (persistProgress)
            saveRaceProfile();
#ifdef RRR3D_AUDIO
        // GameMode::ExitRace always stops the race commentator/audio before
        // Menu::ExitRaceGoFinish, on both the host and a receiving client.
        // The Commentator object remains alive for cPlayerFinish* events.
        stopRaceAudio();
        commentator.pause(false);
#endif
        std::vector<std::size_t> order(
            raceSession.racers().size(), 0U);
        std::iota(order.begin(), order.end(), 0U);
        order.erase(
            std::remove_if(
                order.begin(), order.end(),
                [&](std::size_t racer) {
                    return !raceSession.racers()[racer].finished;
                }),
            order.end());
        std::stable_sort(
            order.begin(), order.end(),
            [&](std::size_t first, std::size_t second) {
                return raceSession.racers()[first].place <
                       raceSession.racers()[second].place;
            });
        if (order.size() > 3U)
            order.resize(3U);
        clearFinishRows();
        try
        {
            finishRows.reserve(order.size());
            for (const auto racer : order)
            {
                const auto& result = raceSession.racers()[racer];
                const auto& definition = originalRace->racers[racer];
                finishRows.emplace_back();
                auto& row = finishRows.back();
                row.racer = racer;
                row.name = createText(
                    *device, localized(definition.name),
                    menu::headerFontHeight, false,
                    menu::Rgba8{233U, 167U, 63U, 255U},
                    resolvedFont);
                std::string rewardMoney =
                    std::to_string(result.rewardMoney);
                if (result.pickedMoney > 0U)
                {
                    rewardMoney +=
                        " + " + std::to_string(result.pickedMoney);
                }
                row.rewardMoney = createText(
                    *device, rewardMoney,
                    menu::headerFontHeight, false,
                    menu::Rgba8{132U, 188U, 67U, 255U},
                    resolvedFont);
                row.rewardPoints = createText(
                    *device, std::to_string(result.rewardPoints),
                    menu::headerFontHeight, false,
                    menu::Rgba8{132U, 188U, 67U, 255U},
                    resolvedFont);
                if (!definition.photoPath.empty())
                {
                    const auto photo = menu::loadOriginalImage(
                        *resources, definition.photoPath);
                    row.photo =
                        createImageTexture(*device, photo);
                    const float photoScale = std::min(
                        {1.0F,
                         198.0F /
                             std::max(
                                 static_cast<float>(photo.width), 1.0F),
                         193.0F /
                             std::max(
                                 static_cast<float>(photo.height), 1.0F)});
                    row.photoWidth =
                        static_cast<float>(photo.width) * photoScale;
                    row.photoHeight =
                        static_cast<float>(photo.height) * photoScale;
                }
            }
        }
        catch (const std::exception& exception)
        {
            clearFinishRows();
            std::cerr << "Unable to create source FinishMenu rows: "
                      << exception.what() << '\n';
            runtimeSmokeFailed = true;
        }
        const auto& player = raceSession.racers().front();
        inRace = false;
        raceInput = {};
        raceUseWeaponRequested = false;
        raceUseAllWeaponsRequested = false;
        raceUseMine = false;
        raceUseHyper = false;
        raceChangeWeaponRequested = false;
        raceWeaponChangeDirection = 1;
        raceFireWeaponSlotRequested = -1;
        raceResetRequested = false;
        menuStack = {MenuScreen::Main, MenuScreen::Finish};
        menuSelection = 0;
        std::cout << "Original FinishMenu: place "
                  << player.place << ", money +"
                  << player.rewardMoney + player.pickedMoney
                  << ", points +" << player.rewardPoints << '\n';
    };
    auto closeFinishMenu = [&]() {
        if (!finishMenuShown)
            return;
        finishMenuShown = false;
        finishAnimationSeconds = 0.0F;
        finishVoiceIndex = 0U;
        finishLastVoiceDispatched = false;
        const auto raceMenuPath = [&]() {
            return championshipMode
                       ? std::vector<MenuScreen>{
                             MenuScreen::Main, MenuScreen::GameMode,
                             MenuScreen::Tournament,
                             MenuScreen::RaceMenu}
                       : std::vector<MenuScreen>{
                             MenuScreen::Main, MenuScreen::GameMode,
                             MenuScreen::RaceMenu};
        };
        menuStack = raceMenuPath();
        menuSelection = 0U;
        const auto transition =
            r3d::game::originalrace::originalFinishTransition(
                raceTournamentAdvance,
                profileState.player.currentPlanet,
                championshipMode);
        switch (transition)
        {
        case r3d::game::originalrace::FinishTransition::Final:
#ifdef RRR3D_VIDEO
            if (!profileState.config.disableVideo)
            {
                // Preserve the legacy Menu.cpp language branch literally:
                // Russian selects final_eng, all other languages final.
                const auto movie =
                    activeLanguage == "russian"
                        ? "Data/Video/final_eng.avi"
                        : "Data/Video/final.avi";
                if (playOriginalMovie(
                        movie,
                        OriginalMovieCompletion::FinalMenu))
                {
                    std::cout
                        << "Original Menu::OnFinishClose -> final movie\n";
                    break;
                }
            }
#endif
            showOriginalFinalMenu();
            std::cout
                << "Original Menu::OnFinishClose -> FinalMenu\n";
            break;
        case r3d::game::originalrace::FinishTransition::
            PlanetCompleted:
        {
            menuStack.push_back(MenuScreen::Planets);
            const auto planetCount = std::min(
                originalGarage->planets.size(),
                profileState.player.planets.size());
            const auto nextPlanet =
                std::min<std::size_t>(
                    profileState.player.currentPlanet + 1U,
                    planetCount > 0U ? planetCount - 1U : 0U);
            angarPlanetIndex =
                planetCount > 0U
                    ? static_cast<int>(nextPlanet)
                    : -1;
            angarPreviousPlanetIndex = -1;
            angarDoorTime = -1.0F;
            angarTravelDialogVisible = false;
            menuSelection =
                planetCount > 0U ? nextPlanet : planetCount;
            refreshPlanetsPage();
            showInfoDialog(
                localized("svWarning"),
                localized("svHintYouCanFlyPlanet"),
                localized("svOk"),
                menu::virtualWidth * 0.5F,
                menu::virtualHeight * 0.5F);
            std::cout
                << "Original Menu::OnFinishClose -> AngarFrame: "
                << localized("svHintYouCanFlyPlanet") << '\n';
            break;
        }
        case r3d::game::originalrace::FinishTransition::
            PassCompleted:
            showInfoDialog(
                localized("svWarning"),
                localized("svHintYouCompletePass"),
                localized("svOk"),
                menu::virtualWidth * 0.5F,
                menu::virtualHeight * 0.5F);
            std::cout
                << "Original Menu::OnFinishClose: "
                << localized("svHintYouCompletePass") << '\n';
            break;
        case r3d::game::originalrace::FinishTransition::PassFailed:
            showInfoDialog(
                localized("svWarning"),
                localized("svHintYouNotCompletePass"),
                localized("svOk"),
                menu::virtualWidth * 0.5F,
                menu::virtualHeight * 0.5F);
            std::cout
                << "Original Menu::OnFinishClose: "
                << localized("svHintYouNotCompletePass") << '\n';
            break;
        case r3d::game::originalrace::FinishTransition::RaceMenu:
            std::cout
                << "Original FinishMenu::OnFinishClose -> RaceMenu2\n";
            break;
        }
    };
    if (options->finishMenuSmokeTest)
    {
        // This is an isolated renderer fixture: it uses the current source
        // race definitions and rewards but deliberately skips profile writes
        // and tournament advancement.
        auto& smokeRacers =
            const_cast<std::vector<
                r3d::game::originalrace::RacerRuntime>&>(
                raceSession.racers());
        const auto count = smokeRacers.size();
        for (std::size_t index = 0U; index < count; ++index)
        {
            auto& racer = smokeRacers[index];
            racer.finished = true;
            racer.place = static_cast<std::uint32_t>(index + 1U);
            const auto reward = std::min(
                index, originalRace->rewardMoney.size() - 1U);
            racer.rewardMoney = originalRace->rewardMoney[reward];
            racer.rewardPoints = originalRace->rewardPoints[reward];
            racer.pickedMoney = index == 0U ? 25U : 0U;
        }
        showFinishMenu(false);
    }
    if (options->gamersFrameSmokeTest)
    {
        championshipMode = false;
        showOriginalGamers();
    }
#endif
    if (options->finalMenuSmokeTest)
        showOriginalFinalMenu();
#ifdef RRR3D_NETWORK
    if (options->networkMenuSmokeTest)
    {
        if (initializeNetwork())
        {
            menuStack = {MenuScreen::Main, MenuScreen::Network};
            menuSelection = 0U;
        }
        else
        {
            runtimeSmokeFailed = true;
            running = false;
        }
    }
#endif
#ifdef RRR3D_AUDIO
    bool integratedAudioInputObserved = !options->audioSmokeTest;
    enum class MusicSmokePhase
    {
        WaitingForDecode,
        WaitingWhilePaused,
        WaitingForResume,
        WaitingForAutomaticNext,
        WaitingForManualNext,
        Complete
    };
    MusicSmokePhase musicSmokePhase = MusicSmokePhase::WaitingForDecode;
    std::array<std::size_t, 3> smokeTrackOrder{};
    std::uint64_t smokePausePosition = 0;
    std::uint64_t smokePhaseTicks = SDL_GetTicks();
    const std::uint64_t musicSmokeDeadline = SDL_GetTicks() + 30000;
    auto drawOriginalMusicDialog = [&]() {
        if (!musicDialogVisible)
            return;
        const MusicDialogVisual* visual = nullptr;
        if (musicDialogSource == OriginalMusicDialogSource::Menu)
        {
            if (musicDialogTrack < menuMusicDialogVisuals.size())
                visual =
                    &menuMusicDialogVisuals[musicDialogTrack];
        }
#ifdef RRR3D_PHYSICS
        else if (musicDialogTrack < gameMusicDialogVisuals.size())
        {
            visual = &gameMusicDialogVisuals[musicDialogTrack];
        }
#endif
        if (visual == nullptr)
            return;

        const float width =
            static_cast<float>(musicDialogFrameImage.width);
        const float height =
            static_cast<float>(musicDialogFrameImage.height);
        // Literal Menu::OnProgress placement.  Widget positions are their
        // centres, hence subtracting half the dlgFrame2 size leaves the
        // fully shown frame 35 px from the left and 30 px from the bottom.
        const float centerX =
            -5.0F + (40.0F + width) * musicDialogOffset -
            width * 0.5F;
        const float centerY =
            menu::virtualHeight - 30.0F - height * 0.5F;
        // DialogMenu2's z values express widget order, not camera-space
        // depth.  Map that order into the established overlay depth band;
        // literal z=3/2 is clipped by the Metal orthographic projection.
        drawQuad(
            *device, quad, shader, musicDialogFrame, width, height,
            centerX, centerY, 60.0F, transparent);
        drawQuad(
            *device, quad, shader, visual->title.texture,
            visual->title.width, visual->title.height,
            centerX - 130.0F + visual->title.width * 0.5F,
            centerY - 21.0F, 59.0F, transparent);
        drawQuad(
            *device, quad, shader, visual->info.texture,
            visual->info.width, visual->info.height,
            centerX - 130.0F + visual->info.width * 0.5F,
            centerY + 17.0F, 59.0F, transparent);
        if (musicDialogOffset > 0.01F)
        {
            if (musicDialogSource ==
                OriginalMusicDialogSource::Menu)
                menuMusicDialogObserved = true;
#ifdef RRR3D_PHYSICS
            else
                raceMusicDialogObserved = true;
#endif
        }
    };
#endif
#if defined(RRR3D_AUDIO) && defined(RRR3D_GAMEPAD_INPUT)
    if (options->audioSmokeTest)
    {
        SDL_Event down{};
        down.key.type = SDL_EVENT_KEY_DOWN;
        down.key.down = true;
        down.key.scancode = SDL_SCANCODE_DOWN;
        SDL_Event confirm{};
        confirm.key.type = SDL_EVENT_KEY_DOWN;
        confirm.key.down = true;
        confirm.key.scancode = SDL_SCANCODE_RETURN;
        if (!SDL_PushEvent(&down) || !SDL_PushEvent(&confirm))
        {
            std::cerr << "Unable to queue integrated M8 menu/audio events: "
                      << SDL_GetError() << '\n';
            runtimeSmokeFailed = true;
        }
    }
#endif
    while (running)
    {
#ifdef RRR3D_NETWORK
        if (networkSession.initialized())
        {
            networkSession.process(
                static_cast<std::uint32_t>(SDL_GetTicks()));
            refreshNetworkRuntimePages();
#ifdef RRR3D_PHYSICS
            presentNetworkFailure();
            const bool networkMatchExited =
                networkSnapshot.state !=
                    r3d::game::originalnetwork::SessionState::Failed &&
                processNetworkLifecycleEvents();
            if (!networkMatchExited &&
                networkSnapshot.state !=
                r3d::game::originalnetwork::SessionState::Failed)
            {
                if (!enterConnectedNetworkMatch())
                {
                    runtimeSmokeFailed = true;
                    running = false;
                }
                synchronizeReplicatedNetworkOptions();
                processNetworkIdentityEvents();
                reconcileDisconnectedNetworkRacers();
                if (networkClientMatchEntered &&
                    networkSnapshot.models.raceActive &&
                    !networkRaceStarted && !inRace &&
                    !raceLoadingActive)
                {
                    networkRaceStarted = true;
                    networkLocalReadyPublished = false;
                    networkLocalGoWaitPublished = false;
                    networkLocalFinishPublished = false;
                    networkHostFinishTimerStarted = false;
                    networkRaceExitApplied = false;
                    networkPublishedPlayer.reset();
                    startCurrentRace();
                }
                if (networkMatchStarted && !inRace &&
                    !publishLocalNetworkPlayer())
                {
                    runtimeSmokeFailed = true;
                    running = false;
                }
                refreshNetworkRacePlayerVisuals();
            }
#endif
        }
        if (options->networkMenuSmokeTest &&
            renderedFrames >= networkSmokeNextFrame &&
            networkSmokeStep < 10U)
        {
            std::string error;
            switch (networkSmokeStep)
            {
            case 0U:
                pushMenu(MenuScreen::NetworkServerType);
                break;
            case 1U:
                backMenu();
                pushMenu(MenuScreen::NetworkClientType);
                break;
            case 2U:
                if (!networkSession.beginLanSearch(error))
                {
                    std::cerr << "Network menu smoke PingHosts failed: "
                              << error << '\n';
                    runtimeSmokeFailed = true;
                }
                pushMenu(MenuScreen::NetworkBrowser);
                renderedNetworkRevision =
                    std::numeric_limits<std::uint64_t>::max();
                refreshNetworkRuntimePages();
                break;
            case 3U:
                backMenu();
                networkIpInput = "127.0.0.1";
                refreshNetworkIpPage();
                replaceNetworkAuxPage(
                    networkStatusPage,
                    {localized("svEnterIP")},
                    menu::smallFontHeight);
                pushMenu(MenuScreen::NetworkIpAddress);
                SDL_StartTextInput(window);
                break;
            case 4U:
                backMenu();
                backMenu();
                backMenu();
                break;
            case 5U:
#ifdef RRR3D_PHYSICS
                if (!initializeNetwork())
                {
                    runtimeSmokeFailed = true;
                    break;
                }
                networkHostRequested = true;
                championshipMode = false;
                menuStack = {MenuScreen::Main};
                showOriginalRaceMenu();
#endif
                break;
            case 6U:
#ifdef RRR3D_PHYSICS
                activateRaceMenuStart();
                break;
#else
                break;
#endif
            case 7U:
#ifdef RRR3D_PHYSICS
                networkHostReadyGateObserved =
                    infoDialog.visible && networkMatchStarted &&
                    networkRacePlayerVisuals.empty();
                hideInfoDialog();
                exitNetworkMatch(true);
#else
                networkHostReadyGateObserved = true;
#endif
                break;
            case 8U:
#ifdef RRR3D_PHYSICS
                if (!initializeNetwork())
                {
                    runtimeSmokeFailed = true;
                    break;
                }
                showLoadingInfoDialog();
                if (!networkSession.connect(
                        {"127.0.0.1",
                         r3d::game::originalnetwork::defaultPort},
                        error))
                {
                    std::cerr
                        << "Network menu smoke refused-connect start: "
                        << error << '\n';
                }
#endif
                break;
            case 9U:
#ifdef RRR3D_PHYSICS
                networkFailureDialogObserved =
                    networkFailureDialogObserved &&
                    networkSnapshot.state ==
                        r3d::game::originalnetwork::
                            SessionState::Failed &&
                    networkSnapshot.failure ==
                        r3d::game::originalnetwork::
                            SessionFailure::ConnectionFailed &&
                    infoDialog.visible && infoDialog.dismissable;
                hideInfoDialog();
                networkFailureDialogAction =
                    NetworkFailureDialogAction::None;
                networkSession.close();
                networkSession.finalize();
                networkSnapshot = {};
                renderedNetworkRevision =
                    std::numeric_limits<std::uint64_t>::max();
                handledNetworkFailureRevision = 0U;
                menuStack = {MenuScreen::Main};
                menuSelection = 0U;
#else
                networkFailureDialogObserved = true;
#endif
                break;
            default:
                break;
            }
            ++networkSmokeStep;
            networkSmokeNextFrame = renderedFrames + 30U;
        }
#endif
#if defined(RRR3D_PHYSICS) && defined(RRR3D_GAMEPAD_INPUT)
        if (options->startOptionsSmokeTest &&
            sourceStartOptionsActive &&
            startOptionsSmokeStep < 6U &&
            renderedFrames >= startOptionsSmokeNextFrame)
        {
            constexpr std::array<SDL_Scancode, 6>
                startOptionsSmokeKeys{
                    SDL_SCANCODE_RIGHT,
                    SDL_SCANCODE_DOWN, SDL_SCANCODE_DOWN,
                    SDL_SCANCODE_DOWN, SDL_SCANCODE_DOWN,
                    SDL_SCANCODE_RETURN};
            SDL_Event press{};
            press.key.type = SDL_EVENT_KEY_DOWN;
            press.key.down = true;
            press.key.scancode =
                startOptionsSmokeKeys[startOptionsSmokeStep];
            SDL_Event release = press;
            release.key.type = SDL_EVENT_KEY_UP;
            release.key.down = false;
            if (!SDL_PushEvent(&press) ||
                !SDL_PushEvent(&release))
            {
                std::cerr
                    << "Unable to queue StartOptionsMenu smoke step "
                    << startOptionsSmokeStep << ": "
                    << SDL_GetError() << '\n';
                runtimeSmokeFailed = true;
            }
            ++startOptionsSmokeStep;
            startOptionsSmokeNextFrame = renderedFrames + 2U;
        }
        if (options->gamersFrameSmokeTest && !inRace &&
            menuStack.back() == MenuScreen::Gamers &&
            gamersSmokeStep < 4U &&
            renderedFrames >= gamersSmokeNextFrame)
        {
            constexpr std::array<SDL_Scancode, 4> gamersSmokeKeys{
                SDL_SCANCODE_UP, SDL_SCANCODE_RETURN,
                SDL_SCANCODE_DOWN, SDL_SCANCODE_RETURN};
            SDL_Event press{};
            press.key.type = SDL_EVENT_KEY_DOWN;
            press.key.down = true;
            press.key.scancode = gamersSmokeKeys[gamersSmokeStep];
            SDL_Event release = press;
            release.key.type = SDL_EVENT_KEY_UP;
            release.key.down = false;
            if (!SDL_PushEvent(&press) || !SDL_PushEvent(&release))
            {
                std::cerr
                    << "Unable to queue GamersFrame smoke step "
                    << gamersSmokeStep << ": " << SDL_GetError()
                    << '\n';
                runtimeSmokeFailed = true;
            }
            ++gamersSmokeStep;
            // Keep each source focus/selection state visible for several
            // frames so the renderer fixture checks sustained presentation,
            // not only one event-loop tick.
            gamersSmokeNextFrame = renderedFrames + std::max(
                30U, options->smokeFrames / 5U);
        }
        // Do not enqueue all confirms before the event loop.  SDL's input
        // layer intentionally suppresses repeats while a key is held, and
        // the old batch therefore never exercised Main -> GameMode ->
        // Tournament -> Continue -> RaceMenu -> WorkshopFrame ->
        // GarageFrame -> AngarFrame -> AchievmentFrame -> Race.
        // Advance one real press/release pair per rendered menu frame.
        if (options->raceRenderSmokeTest && !inRace &&
            menuStack.back() == MenuScreen::Workshop &&
            raceWorkshopWeaponDialogObserved &&
            !raceInfoDialogSmokeShown)
        {
            const auto centers = workshopGoodCenters();
            const auto& sender = centers.front();
            const float posX = sender[0] + 25.0F;
            const float posY = sender[1] - 25.0F;
            showInfoDialog(
                localized("svWarning"),
                localized("svHintWeaponNotSupport"),
                localized("svOk"),
                posX +
                    static_cast<float>(
                        infoDialogFrameImage.width) *
                        0.5F,
                posY -
                    static_cast<float>(
                        infoDialogFrameImage.height) *
                        0.5F);
            raceInfoDialogSmokeShown = true;
        }
        if (options->raceRenderSmokeTest && infoDialog.visible &&
            raceInfoDialogObserved && !raceInfoDialogCloseQueued)
        {
            SDL_Event press{};
            press.key.type = SDL_EVENT_KEY_DOWN;
            press.key.down = true;
            press.key.scancode = SDL_SCANCODE_RETURN;
            SDL_Event release = press;
            release.key.type = SDL_EVENT_KEY_UP;
            release.key.down = false;
            if (!SDL_PushEvent(&press) ||
                !SDL_PushEvent(&release))
            {
                std::cerr
                    << "Unable to close source InfoDialog smoke: "
                    << SDL_GetError() << '\n';
                runtimeSmokeFailed = true;
            }
            raceInfoDialogCloseQueued = true;
        }
        const bool waitingForWorkshopDialogs =
            options->raceRenderSmokeTest && !inRace &&
            menuStack.back() == MenuScreen::Workshop &&
            (!raceWorkshopWeaponDialogObserved ||
             !raceInfoDialogObserved || infoDialog.visible);
        if (options->raceRenderSmokeTest && !inRace &&
            !waitingForWorkshopDialogs &&
            raceSmokeMenuStep < 33U &&
            renderedFrames >= raceSmokeNextMenuFrame)
        {
            constexpr std::array<SDL_Scancode, 33> smokeKeys{
                SDL_SCANCODE_RETURN, SDL_SCANCODE_RETURN,
                SDL_SCANCODE_DOWN, SDL_SCANCODE_DOWN,
                SDL_SCANCODE_RETURN, SDL_SCANCODE_DOWN,
                SDL_SCANCODE_RIGHT, SDL_SCANCODE_RETURN,
                SDL_SCANCODE_RIGHT, SDL_SCANCODE_RETURN,
                SDL_SCANCODE_DOWN, SDL_SCANCODE_RETURN,
                SDL_SCANCODE_RIGHT,
                SDL_SCANCODE_RETURN, SDL_SCANCODE_ESCAPE,
                SDL_SCANCODE_RIGHT, SDL_SCANCODE_RIGHT,
                SDL_SCANCODE_RETURN, SDL_SCANCODE_RIGHT,
                SDL_SCANCODE_ESCAPE, SDL_SCANCODE_RIGHT,
                SDL_SCANCODE_RIGHT, SDL_SCANCODE_RIGHT,
                SDL_SCANCODE_RETURN, SDL_SCANCODE_LEFT,
                SDL_SCANCODE_ESCAPE, SDL_SCANCODE_RIGHT,
                SDL_SCANCODE_RIGHT, SDL_SCANCODE_RIGHT,
                SDL_SCANCODE_RIGHT, SDL_SCANCODE_RETURN,
                SDL_SCANCODE_ESCAPE,
                SDL_SCANCODE_RETURN};
            SDL_Event press{};
            press.key.type = SDL_EVENT_KEY_DOWN;
            press.key.down = true;
            press.key.scancode = smokeKeys[raceSmokeMenuStep];
            SDL_Event release = press;
            release.key.type = SDL_EVENT_KEY_UP;
            release.key.down = false;
            if (!SDL_PushEvent(&press) ||
                !SDL_PushEvent(&release))
            {
                std::cerr << "Unable to queue integrated M9 menu step "
                          << raceSmokeMenuStep << ": "
                          << SDL_GetError() << '\n';
                runtimeSmokeFailed = true;
            }
            ++raceSmokeMenuStep;
            raceSmokeNextMenuFrame = renderedFrames + 1U;
        }
        if (waitingForWorkshopDialogs &&
            !raceWorkshopWeaponDialogObserved &&
            !raceWorkshopWeaponDialogMotionQueued)
        {
            std::optional<std::array<float, 2>> target;
            if (!workshopGoods.empty())
                target = workshopGoodCenters()[0];
            else
            {
                const auto slotCenters = workshopSlotCenters();
                for (std::size_t slot = 0U;
                     slot < profileState.player.slots.size(); ++slot)
                {
                    if (originalGarage->findItem(
                            profileState.player.slots[slot].record) !=
                        nullptr)
                    {
                        target = slotCenters[slot];
                        break;
                    }
                }
            }
            int windowWidth = 0;
            int windowHeight = 0;
            if (!target ||
                !SDL_GetWindowSize(
                    window, &windowWidth, &windowHeight) ||
                windowWidth <= 0 || windowHeight <= 0)
            {
                std::cerr
                    << "Unable to target original WorkshopFrame "
                       "WeaponDialog smoke input\n";
                runtimeSmokeFailed = true;
                raceWorkshopWeaponDialogMotionQueued = true;
            }
            else
            {
                SDL_Event motion{};
                motion.motion.type = SDL_EVENT_MOUSE_MOTION;
                motion.motion.x =
                    (*target)[0] *
                    static_cast<float>(windowWidth) /
                    menu::virtualWidth;
                motion.motion.y =
                    (*target)[1] *
                    static_cast<float>(windowHeight) /
                    menu::virtualHeight;
                if (!SDL_PushEvent(&motion))
                {
                    std::cerr
                        << "Unable to queue original WorkshopFrame "
                           "WeaponDialog mouse motion: "
                        << SDL_GetError() << '\n';
                    runtimeSmokeFailed = true;
                }
                raceWorkshopWeaponDialogMotionQueued = true;
            }
        }
#endif
#ifdef RRR3D_PHYSICS
        if (options->raceRenderSmokeTest && inRace &&
            !raceSmokeAccelerateQueued)
        {
            SDL_Event accelerate{};
            accelerate.key.type = SDL_EVENT_KEY_DOWN;
            accelerate.key.down = true;
            accelerate.key.scancode = SDL_SCANCODE_UP;
            if (!SDL_PushEvent(&accelerate))
            {
                std::cerr
                    << "Unable to queue integrated Up Arrow acceleration: "
                    << SDL_GetError() << '\n';
                runtimeSmokeFailed = true;
            }
            raceSmokeAccelerateQueued = true;
        }
        if (options->raceRenderSmokeTest && inRace &&
            renderedFrames >= 20U + racePauseSmokeStep &&
            racePauseSmokeStep < 3U)
        {
            if (racePauseSmokeStep == 0U)
                openExitRaceDialog();
            else if (racePauseSmokeStep == 1U)
                exitRaceYesFocused = false;
            else
                closeExitRaceDialog();
            ++racePauseSmokeStep;
        }
        if (options->raceRenderSmokeTest && inRace &&
            racePauseSmokeStep == 3U &&
            raceChatSmokeStep < 4U &&
            renderedFrames >= raceChatSmokeNextFrame)
        {
            SDL_Event smokeEvent{};
            SDL_Event release{};
            bool queued = false;
            if (raceChatSmokeStep == 1U)
            {
                smokeEvent.text.type = SDL_EVENT_TEXT_INPUT;
                smokeEvent.text.text = "source UserChat";
                queued = SDL_PushEvent(&smokeEvent);
            }
            else
            {
                smokeEvent.key.type = SDL_EVENT_KEY_DOWN;
                smokeEvent.key.down = true;
                smokeEvent.key.scancode =
                    raceChatSmokeStep == 3U
                        ? SDL_SCANCODE_UP
                        : SDL_SCANCODE_RETURN;
                queued = SDL_PushEvent(&smokeEvent);
                if (raceChatSmokeStep != 3U)
                {
                    release = smokeEvent;
                    release.key.type = SDL_EVENT_KEY_UP;
                    release.key.down = false;
                    queued = queued && SDL_PushEvent(&release);
                }
            }
            if (!queued)
            {
                std::cerr
                    << "Unable to queue source UserChat smoke step "
                    << raceChatSmokeStep << ": "
                    << SDL_GetError() << '\n';
                runtimeSmokeFailed = true;
            }
            ++raceChatSmokeStep;
            raceChatSmokeNextFrame = renderedFrames + 2U;
        }
#endif
        if (options->startupSmokeTest && sourceStartupActive &&
            startupLabHoldObserved && !startupEscapeQueued)
        {
            SDL_Event press{};
            press.key.type = SDL_EVENT_KEY_DOWN;
            press.key.down = true;
            press.key.scancode = SDL_SCANCODE_ESCAPE;
            SDL_Event release = press;
            release.key.type = SDL_EVENT_KEY_UP;
            release.key.down = false;
            if (!SDL_PushEvent(&press) || !SDL_PushEvent(&release))
            {
                std::cerr
                    << "Unable to queue source startup Escape: "
                    << SDL_GetError() << '\n';
                runtimeSmokeFailed = true;
            }
            startupEscapeQueued = true;
        }
#ifdef RRR3D_PHYSICS
        const bool sourceChatVisible =
            inRace ||
            (!raceLoadingActive && !menuStack.empty() &&
             menuStack.back() == MenuScreen::RaceMenu);
        if (sourceChatVisible != userChat.visible())
        {
            const bool inputWasVisible = userChat.inputVisible();
            userChat.show(sourceChatVisible);
            if (inputWasVisible && !sourceChatVisible)
                SDL_StopTextInput(window);
        }
#ifdef RRR3D_NETWORK
        if (networkMatchStarted)
        {
            std::uint64_t lastSequence =
                networkLastChatEventSequence;
            for (const auto& networkEvent :
                 networkSnapshot.models.events)
            {
                if (networkEvent.sequence <=
                    networkLastChatEventSequence)
                {
                    continue;
                }
                lastSequence = std::max(
                    lastSequence, networkEvent.sequence);
                if (networkEvent.kind !=
                    r3d::game::originalnetwork::
                        NetworkEventKind::ChatLine)
                {
                    continue;
                }

                std::string senderName;
                std::array<float, 4> senderColor{
                    1.0F, 1.0F, 1.0F, 1.0F};
                const auto sender = std::find_if(
                    networkSnapshot.models.players.begin(),
                    networkSnapshot.models.players.end(),
                    [&](const auto& player) {
                        return player.ownerId == networkEvent.sender;
                    });
                if (sender != networkSnapshot.models.players.end())
                {
                    senderColor = sender->color;
                    // Player::GetName first uses connection userName (empty
                    // for the original TCP backend), then resolves the exact
                    // Tournament::GetPlayerData(gamerId) character.
                    senderName = sourceGamerName(sender->gamerId);
                    if (senderName.empty())
                    {
                        const auto model = std::find(
                            networkRaceModelOrder.begin(),
                            networkRaceModelOrder.end(),
                            sender->modelId);
                        if (model != networkRaceModelOrder.end())
                        {
                            const auto racer = static_cast<std::size_t>(
                                std::distance(
                                    networkRaceModelOrder.begin(), model));
                            if (racer < originalRace->racers.size())
                            {
                                senderName = localized(
                                    originalRace->racers[racer].name);
                            }
                        }
                    }
                    if (senderName.empty() &&
                        sender->playerId < originalRace->racers.size())
                    {
                        senderName = localized(originalRace->racers[
                            sender->playerId].name);
                    }
                }
                userChat.pushLine(
                    "<" + senderName, networkEvent.text,
                    senderColor);
            }
            networkLastChatEventSequence = lastSequence;
        }
#endif
#endif
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
#ifdef RRR3D_AUDIO
            if ((event.type == SDL_EVENT_AUDIO_DEVICE_ADDED ||
                 event.type == SDL_EVENT_AUDIO_DEVICE_REMOVED ||
                 event.type == SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED) &&
                !event.adevice.recording)
            {
                if (event.type == SDL_EVENT_AUDIO_DEVICE_ADDED)
                {
                    audio.notifyPlaybackDeviceEvent(
                        r3d::audio::PlaybackDeviceEvent::Added,
                        event.adevice.which);
                }
                else if (event.type == SDL_EVENT_AUDIO_DEVICE_REMOVED)
                {
                    audio.notifyPlaybackDeviceEvent(
                        r3d::audio::PlaybackDeviceEvent::Removed,
                        event.adevice.which);
                }
                else
                {
                    audio.notifyPlaybackDeviceEvent(
                        r3d::audio::PlaybackDeviceEvent::FormatChanged,
                        event.adevice.which);
                }
            }
#endif
#ifdef RRR3D_VIDEO
            if (originalMovieActive &&
                event.type != SDL_EVENT_QUIT &&
                event.type != SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.type != SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
            {
                const auto movieInputEvents =
                    input.processEvent(event);
                for (const auto& inputEvent : movieInputEvents)
                {
                    if (inputEvent.active && !inputEvent.repeated &&
                        (inputEvent.action ==
                             rrr3d::input::Action::MenuBack ||
                         inputEvent.action ==
                             rrr3d::input::Action::Pause))
                    {
                        finishOriginalMovie();
                        break;
                    }
                }
                continue;
            }
#endif
            if (sourceStartupActive &&
                event.type != SDL_EVENT_QUIT &&
                event.type != SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.type != SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
            {
                bool skipIntro =
                    event.type == SDL_EVENT_KEY_DOWN &&
                    !event.key.repeat &&
                    event.key.scancode == SDL_SCANCODE_ESCAPE;
#ifdef RRR3D_GAMEPAD_INPUT
                const auto startupInputEvents =
                    input.processEvent(event);
                skipIntro = skipIntro || std::any_of(
                    startupInputEvents.begin(),
                    startupInputEvents.end(),
                    [](const rrr3d::input::ActionEvent& inputEvent) {
                        return inputEvent.active &&
                               !inputEvent.repeated &&
                               inputEvent.action ==
                                   rrr3d::input::Action::MenuBack;
                    });
#endif
                if (skipIntro)
                {
                    // GameMode::OnHandleInput changes _startUpTime to -2:
                    // one startLogo frame is still presented before menu.
                    sourceStartupSeconds = 12.0F;
                    startupEscapeObserved = true;
                }
                continue;
            }
#ifdef RRR3D_PHYSICS
            const bool chatEnter =
                event.type == SDL_EVENT_KEY_DOWN &&
                !event.key.repeat &&
                (event.key.scancode == SDL_SCANCODE_RETURN ||
                 event.key.scancode == SDL_SCANCODE_KP_ENTER);
            bool chatInteractive = inRace;
#ifdef RRR3D_NETWORK
            chatInteractive = chatInteractive || networkMatchStarted;
#endif
            if (userChat.visible() && chatInteractive && chatEnter)
            {
#ifdef RRR3D_GAMEPAD_INPUT
                static_cast<void>(input.processEvent(event));
#endif
                if (userChat.inputVisible())
                {
                    const std::string text = userChat.inputText();
                    if (!text.empty())
                    {
                        const std::string playerName =
                            sourceLocalChatName();
                        userChat.pushLine(
                            "<" + playerName, text,
                            profileState.player.color);
#ifdef RRR3D_NETWORK
                        if (networkMatchStarted)
                        {
                            std::string error;
                            if (!networkSession.pushLine(text, error))
                            {
                                std::cerr
                                    << "Original NetRace::PushLine failed: "
                                    << error << '\n';
                            }
                        }
#endif
                    }
                    const std::string playerName =
                        sourceLocalChatName();
                    userChat.showInput(
                        false, playerName + ": ", {},
                        profileState.player.color);
                    SDL_StopTextInput(window);
                }
                else
                {
                    const std::string playerName =
                        sourceLocalChatName();
                    userChat.showInput(
                        true, playerName + ": ", {},
                        profileState.player.color);
                    clearRaceControls();
                    if (!SDL_StartTextInput(window))
                    {
                        std::cerr
                            << "SDL_StartTextInput for source UserChat "
                               "failed: "
                            << SDL_GetError() << '\n';
                    }
                }
                continue;
            }
            if (userChat.inputVisible())
            {
                if (event.type == SDL_EVENT_TEXT_INPUT)
                {
                    userChat.appendInput(event.text.text);
                    continue;
                }
                if (event.type == SDL_EVENT_KEY_DOWN &&
                    !event.key.repeat &&
                    event.key.scancode == SDL_SCANCODE_BACKSPACE)
                {
#ifdef RRR3D_GAMEPAD_INPUT
                    static_cast<void>(input.processEvent(event));
#endif
                    userChat.backspaceInput();
                    continue;
                }
                const bool gameplayInput =
                    event.type == SDL_EVENT_KEY_DOWN ||
                    event.type == SDL_EVENT_KEY_UP ||
                    event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ||
                    event.type == SDL_EVENT_GAMEPAD_BUTTON_UP ||
                    event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION;
                if (gameplayInput)
                {
#ifdef RRR3D_GAMEPAD_INPUT
                    static_cast<void>(input.processEvent(event));
#endif
                    // HumanPlayer::OnHandleInput returns before every race
                    // action while Menu::IsChatInputVisible is true.
                    clearRaceControls();
                    continue;
                }
            }
#endif
#ifdef RRR3D_NETWORK
            if (menuStack.back() == MenuScreen::NetworkIpAddress)
            {
                bool changed = false;
                if (event.type == SDL_EVENT_TEXT_INPUT)
                {
                    if (networkIpInput == "_")
                        networkIpInput.clear();
                    for (const char* character = event.text.text;
                         *character != '\0'; ++character)
                    {
                        if ((std::isdigit(
                                 static_cast<unsigned char>(*character)) ||
                             *character == '.') &&
                            networkIpInput.size() < 15U &&
                            !(*character == '.' &&
                              !networkIpInput.empty() &&
                              networkIpInput.back() == '.'))
                        {
                            networkIpInput.push_back(*character);
                            changed = true;
                        }
                    }
                    if (networkIpInput.empty())
                        networkIpInput = "_";
                    if (changed)
                        refreshNetworkIpPage();
                    continue;
                }
                if (event.type == SDL_EVENT_KEY_DOWN &&
                    !event.key.repeat &&
                    event.key.scancode == SDL_SCANCODE_BACKSPACE)
                {
                    if (networkIpInput != "_" &&
                        !networkIpInput.empty())
                    {
                        networkIpInput.pop_back();
                        if (networkIpInput.empty())
                            networkIpInput = "_";
                        refreshNetworkIpPage();
                    }
                    continue;
                }
            }
#endif
#ifdef RRR3D_GAMEPAD_INPUT
#ifdef RRR3D_PHYSICS
            if (raceLoadingActive &&
                event.type != SDL_EVENT_QUIT &&
                event.type != SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.type != SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
            {
                // Menu::msInfo is modal, hides the cursor, and does not
                // dispatch menu/gameplay actions while the world is loaded.
                input.processEvent(event);
                continue;
            }
            if (sourceStartOptionsActive &&
                event.type != SDL_EVENT_QUIT &&
                event.type != SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.type != SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
            {
                if (startOptionsReloadDialogPending &&
                    infoDialog.visible)
                {
                    const auto dialogEvents = input.processEvent(event);
                    const bool acknowledged = std::any_of(
                        dialogEvents.begin(), dialogEvents.end(),
                        [](const rrr3d::input::ActionEvent& inputEvent) {
                            return inputEvent.active &&
                                   !inputEvent.repeated &&
                                   inputEvent.action ==
                                       rrr3d::input::Action::MenuConfirm;
                        });
                    if (acknowledged)
                    {
                        hideInfoDialog();
                        finishStartOptions();
                    }
                    continue;
                }
                if (event.type == SDL_EVENT_MOUSE_MOTION ||
                    (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                     event.button.button == SDL_BUTTON_LEFT))
                {
                    int windowWidth = 0;
                    int windowHeight = 0;
                    const float pointerX =
                        event.type == SDL_EVENT_MOUSE_MOTION
                            ? event.motion.x
                            : event.button.x;
                    const float pointerY =
                        event.type == SDL_EVENT_MOUSE_MOTION
                            ? event.motion.y
                            : event.button.y;
                    if (SDL_GetWindowSize(
                            window, &windowWidth, &windowHeight) &&
                        windowWidth > 0 && windowHeight > 0)
                    {
                        const float virtualX =
                            pointerX * menu::virtualWidth /
                            static_cast<float>(windowWidth);
                        const float virtualY =
                            pointerY * menu::virtualHeight /
                            static_cast<float>(windowHeight);
                        const float centerX =
                            menu::virtualWidth * 0.5F;
                        const float centerY =
                            menu::virtualHeight * 0.5F;
                        bool pointerHandled = false;
                        for (std::size_t row = 0U; row < 4U; ++row)
                        {
                            const float rowY = centerY - 145.0F +
                                static_cast<float>(row) * 50.0F;
                            if (std::abs(virtualY - rowY) <= 23.0F &&
                                std::abs(
                                    virtualX - (centerX + 100.0F)) <=
                                    145.0F)
                            {
                                startOptionsFocus = row;
                                pointerHandled = true;
                                if (event.type ==
                                    SDL_EVENT_MOUSE_BUTTON_DOWN)
                                {
                                    adjustStartOption(
                                        virtualX < centerX + 100.0F
                                            ? -1
                                            : 1);
                                }
                                break;
                            }
                        }
                        const float applyX = centerX - 10.0F;
                        const float applyY = centerY + 138.0F;
                        if (!pointerHandled &&
                            std::abs(virtualX - applyX) <=
                                static_cast<float>(
                                    startOptionsButtonImage.width) *
                                    0.5F &&
                            std::abs(virtualY - applyY) <=
                                static_cast<float>(
                                    startOptionsButtonImage.height) *
                                    0.5F)
                        {
                            startOptionsFocus = 4U;
                            if (event.type ==
                                SDL_EVENT_MOUSE_BUTTON_DOWN)
                            {
#ifdef RRR3D_AUDIO
                                playMainButtonClick();
#endif
                                applyStartOptions();
                            }
                        }
                    }
                    // Do not pass modal pointer activity to MainMenu2.
                    input.processEvent(event);
                    continue;
                }
                const auto startOptionEvents = input.processEvent(event);
                for (const auto& inputEvent : startOptionEvents)
                {
                    if (!inputEvent.active || inputEvent.repeated)
                        continue;
                    if (inputEvent.action ==
                        rrr3d::input::Action::MenuUp)
                    {
                        startOptionsFocus =
                            startOptionsFocus == 0U
                                ? 4U
                                : startOptionsFocus - 1U;
                    }
                    else if (inputEvent.action ==
                             rrr3d::input::Action::MenuDown)
                    {
                        startOptionsFocus =
                            (startOptionsFocus + 1U) % 5U;
                    }
                    else if (
                        startOptionsFocus < 4U &&
                        (inputEvent.action ==
                             rrr3d::input::Action::TurnLeft ||
                         inputEvent.action ==
                             rrr3d::input::Action::TurnRight))
                    {
                        adjustStartOption(
                            inputEvent.action ==
                                    rrr3d::input::Action::TurnLeft
                                ? -1
                                : 1);
                    }
                    else if (
                        inputEvent.action ==
                            rrr3d::input::Action::MenuConfirm &&
                        startOptionsFocus == 4U)
                    {
#ifdef RRR3D_AUDIO
                        playMainButtonClick();
#endif
                        applyStartOptions();
                    }
                    // StartOptionsMenu is deliberately modal.  Escape and
                    // every unrelated game action are consumed.
                }
                continue;
            }
            if (!inRace && bindingCaptureAction)
            {
                std::optional<std::string> bindingName;
                bool consumedCaptureEvent = false;
                if (event.type == SDL_EVENT_KEY_DOWN &&
                    !event.key.repeat)
                {
                    consumedCaptureEvent = true;
                    if (!bindingCaptureGamepad)
                    {
                        bindingName =
                            originalKeyName(event.key.scancode);
                    }
                }
                else if (bindingCaptureGamepad &&
                         event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
                {
                    consumedCaptureEvent = true;
                    bindingName = originalGamepadButtonName(
                        static_cast<SDL_GamepadButton>(
                            event.gbutton.button));
                }
                else if (bindingCaptureGamepad &&
                         event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION)
                {
                    bindingName = originalGamepadAxisName(
                        static_cast<SDL_GamepadAxis>(event.gaxis.axis),
                        event.gaxis.value);
                    consumedCaptureEvent = bindingName.has_value();
                }
                if (bindingName && !bindingName->empty())
                {
                    auto& bindings =
                        bindingCaptureGamepad
                            ? optionsDraftConfig.gamepadControls
                            : optionsDraftConfig.keyboardControls;
                    bindings[*bindingCaptureAction] = *bindingName;
                    std::cout
                        << "Original ControlsFrame: "
                        << *bindingCaptureAction << " -> "
                        << *bindingName << '\n';
                    bindingCaptureAction.reset();
                    refreshCurrentOptionsPage();
                }
                if (consumedCaptureEvent)
                    continue;
            }
#endif
            bool pointerTargetsItem = true;
            bool pointerHandledOriginalOptions = false;
            bool workshopPointerSlotPlane = false;
#ifdef RRR3D_PHYSICS
            std::optional<bool> pointerAcceptChoice;
            if (infoDialog.visible && infoDialog.dismissable &&
                (event.type == SDL_EVENT_MOUSE_MOTION ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN))
            {
                int windowWidth = 0;
                int windowHeight = 0;
                const float pointerX =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.x
                        : event.button.x;
                const float pointerY =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.y
                        : event.button.y;
                bool hoveredOk = false;
                if (SDL_GetWindowSize(
                        window, &windowWidth, &windowHeight) &&
                    windowWidth > 0 && windowHeight > 0)
                {
                    const float virtualX =
                        pointerX * menu::virtualWidth /
                        static_cast<float>(windowWidth);
                    const float virtualY =
                        pointerY * menu::virtualHeight /
                        static_cast<float>(windowHeight);
                    hoveredOk =
                        std::abs(
                            virtualX - infoDialog.centerX) <=
                            static_cast<float>(
                                infoDialogButtonSelectedImage.width) *
                                0.5F &&
                        std::abs(
                            virtualY -
                            (infoDialog.centerY + 105.0F)) <=
                            static_cast<float>(
                                infoDialogButtonSelectedImage.height) *
                                0.5F;
                }
                pointerTargetsItem =
                    hoveredOk ||
                    event.type == SDL_EVENT_MOUSE_MOTION ||
                    event.button.button != SDL_BUTTON_LEFT;
            }
            else if (acceptDialogVisible() &&
                (event.type == SDL_EVENT_MOUSE_MOTION ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN))
            {
                int windowWidth = 0;
                int windowHeight = 0;
                const float pointerX =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.x
                        : event.button.x;
                const float pointerY =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.y
                        : event.button.y;
                if (SDL_GetWindowSize(
                        window, &windowWidth, &windowHeight) &&
                    windowWidth > 0 && windowHeight > 0)
                {
                    const float virtualX =
                        pointerX * menu::virtualWidth /
                        static_cast<float>(windowWidth);
                    const float virtualY =
                        pointerY * menu::virtualHeight /
                        static_cast<float>(windowHeight);
                    const float buttonY =
                        acceptDialog.centerY +
                        acceptDialog.buttonOffsetY;
                    const float yesX =
                        acceptDialog.centerX +
                        acceptDialog.yesOffsetX;
                    const float noX =
                        acceptDialog.centerX +
                        acceptDialog.noOffsetX;
                    if (std::abs(virtualY - buttonY) <=
                            acceptDialog.buttonHeight * 0.5F &&
                        std::abs(virtualX - yesX) <=
                            acceptDialog.buttonWidth * 0.5F)
                    {
                        pointerAcceptChoice = true;
                    }
                    else if (
                        std::abs(virtualY - buttonY) <=
                            acceptDialog.buttonHeight * 0.5F &&
                        std::abs(virtualX - noX) <=
                            acceptDialog.buttonWidth * 0.5F)
                    {
                        pointerAcceptChoice = false;
                    }
                }
                acceptDialog.hoveredChoice = pointerAcceptChoice;
                if (pointerAcceptChoice)
                    setAcceptDialogFocus(*pointerAcceptChoice);
                pointerTargetsItem =
                    pointerAcceptChoice.has_value() ||
                    event.type == SDL_EVENT_MOUSE_MOTION ||
                    event.button.button != SDL_BUTTON_LEFT;
            }
            else if (!inRace &&
                menuStack.back() == MenuScreen::Gamers &&
                (event.type == SDL_EVENT_MOUSE_MOTION ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN))
            {
                int windowWidth = 0;
                int windowHeight = 0;
                const float pointerX =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.x
                        : event.button.x;
                const float pointerY =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.y
                        : event.button.y;
                bool hoveredGamerControl = false;
                if (SDL_GetWindowSize(
                        window, &windowWidth, &windowHeight) &&
                    windowWidth > 0 && windowHeight > 0)
                {
                    const float virtualX =
                        pointerX * menu::virtualWidth /
                        static_cast<float>(windowWidth);
                    const float virtualY =
                        pointerY * menu::virtualHeight /
                        static_cast<float>(windowHeight);
                    constexpr float planetRadius =
                        (menu::virtualHeight - 254.0F) * 0.5F;
                    constexpr float planetX =
                        menu::virtualWidth * 0.5F - 25.0F;
                    const float leftX =
                        planetX - planetRadius - 40.0F + 3.0F -
                        static_cast<float>(
                            garageArrowSelectedImage.width) *
                            0.5F;
                    const float rightX =
                        planetX + planetRadius + 40.0F + 3.0F +
                        static_cast<float>(
                            garageArrowSelectedImage.width) *
                            0.5F;
                    const float nextX =
                        menu::virtualWidth * 0.5F + 400.0F +
                        static_cast<float>(
                            gamersNextArrowSelectedImage.width) *
                            0.5F;
                    constexpr float nextY =
                        menu::virtualHeight - 100.0F;
                    if (adjacentGamerIndex(gamerPlanetIndex, -1) &&
                        std::abs(virtualX - leftX) <= 62.0F &&
                        std::abs(virtualY - planetRadius) <= 82.0F)
                    {
                        gamersFocus = GamersFocus::Left;
                        hoveredGamerControl = true;
                    }
                    else if (
                        adjacentGamerIndex(gamerPlanetIndex, 1) &&
                        std::abs(virtualX - rightX) <= 62.0F &&
                        std::abs(virtualY - planetRadius) <= 82.0F)
                    {
                        gamersFocus = GamersFocus::Right;
                        hoveredGamerControl = true;
                    }
                    else if (
                        std::abs(virtualX - nextX) <= 85.0F &&
                        std::abs(virtualY - nextY) <= 75.0F)
                    {
                        gamersFocus = GamersFocus::Next;
                        hoveredGamerControl = true;
                    }
                }
                pointerTargetsItem =
                    hoveredGamerControl ||
                    event.type == SDL_EVENT_MOUSE_MOTION ||
                    event.button.button != SDL_BUTTON_LEFT;
            }
            else if (!inRace &&
                menuStack.back() == MenuScreen::Profiles &&
                (event.type == SDL_EVENT_MOUSE_MOTION ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN))
            {
                int windowWidth = 0;
                int windowHeight = 0;
                const float pointerX =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.x
                        : event.button.x;
                const float pointerY =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.y
                        : event.button.y;
                bool hoveredProfileControl = false;
                if (SDL_GetWindowSize(
                        window, &windowWidth, &windowHeight) &&
                    windowWidth > 0 && windowHeight > 0)
                {
                    const float virtualX =
                        pointerX * menu::virtualWidth /
                        static_cast<float>(windowWidth);
                    const float virtualY =
                        pointerY * menu::virtualHeight /
                        static_cast<float>(windowHeight);
                    const float centerX =
                        menu::virtualWidth * 0.5F;
                    const float centerY =
                        menu::virtualHeight * 0.5F;
                        const auto visibleEnd = std::min(
                            profileGridScroll + 4U,
                            profileState.profiles.size());
                        for (std::size_t index = profileGridScroll;
                             index < visibleEnd; ++index)
                        {
                            const float rowY =
                                centerY - 90.0F +
                                static_cast<float>(
                                    index - profileGridScroll) *
                                    menu::itemSpacing;
                            const float closeX =
                                centerX +
                                static_cast<float>(
                                    model->selectionImage.width) *
                                    0.5F -
                                40.0F;
                            if (std::abs(virtualX - closeX) <= 22.0F &&
                                std::abs(virtualY - rowY) <= 22.0F)
                            {
                                profileFocus = ProfileFocus::Close;
                                profileFocusIndex = index;
                                hoveredProfileControl = true;
                                break;
                            }
                            if (std::abs(virtualX - centerX) <=
                                    static_cast<float>(
                                        model->selectionImage.width) *
                                        0.5F &&
                                std::abs(virtualY - rowY) <= 24.0F)
                            {
                                profileFocus = ProfileFocus::Item;
                                profileFocusIndex = index;
                                hoveredProfileControl = true;
                                break;
                            }
                        }
                        const float backY = centerY + 150.0F;
                        if (!hoveredProfileControl &&
                            std::abs(virtualX - centerX) <=
                                static_cast<float>(
                                    model->selectionImage.width) *
                                    0.5F &&
                            std::abs(virtualY - backY) <= 24.0F)
                        {
                            profileFocus = ProfileFocus::Back;
                            hoveredProfileControl = true;
                        }
                        if (!hoveredProfileControl &&
                            std::abs(virtualX - centerX) <= 26.0F &&
                            std::abs(
                                virtualY -
                                (centerY - 108.0F)) <= 22.0F &&
                            profileGridScroll > 0U)
                        {
                            profileFocus = ProfileFocus::Up;
                            hoveredProfileControl = true;
                        }
                        if (!hoveredProfileControl &&
                            std::abs(virtualX - centerX) <= 26.0F &&
                            std::abs(
                                virtualY -
                                (centerY + 120.0F)) <= 22.0F &&
                            profileGridScroll + 4U <
                                profileState.profiles.size())
                        {
                            profileFocus = ProfileFocus::Down;
                            hoveredProfileControl = true;
                        }
                }
                pointerTargetsItem =
                    hoveredProfileControl ||
                    event.type == SDL_EVENT_MOUSE_MOTION ||
                    event.button.button != SDL_BUTTON_LEFT;
            }
            else if (!inRace &&
                menuStack.back() == MenuScreen::Garage &&
                (event.type == SDL_EVENT_MOUSE_MOTION ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN))
            {
                int windowWidth = 0;
                int windowHeight = 0;
                const float pointerX =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.x
                        : event.button.x;
                const float pointerY =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.y
                        : event.button.y;
                std::optional<std::size_t> hoveredGarageItem;
                if (SDL_GetWindowSize(
                        window, &windowWidth, &windowHeight) &&
                    windowWidth > 0 && windowHeight > 0)
                {
                    const float virtualX =
                        pointerX * menu::virtualWidth /
                        static_cast<float>(windowWidth);
                    const float virtualY =
                        pointerY * menu::virtualHeight /
                        static_cast<float>(windowHeight);
                        const float bottomCenterY =
                            menu::virtualHeight -
                            static_cast<float>(
                                garageBottomPanelImage.height) *
                                0.5F;
                        if (virtualX <= 250.0F &&
                            std::abs(
                                virtualY -
                                (bottomCenterY + 8.0F)) <= 45.0F)
                        {
                            hoveredGarageItem = 0U;
                        }
                        else if (
                            std::abs(
                                virtualX -
                                (menu::virtualWidth * 0.5F +
                                 15.0F)) <= 135.0F &&
                            std::abs(
                                virtualY -
                                (menu::virtualHeight -
                                 static_cast<float>(
                                     garageBottomPanelImage.height))) <=
                                40.0F)
                        {
                            hoveredGarageItem = 1U;
                        }
                        else if (
                            std::abs(virtualX - 181.0F) <= 58.0F &&
                            std::abs(
                                virtualY -
                                menu::virtualHeight * 0.5F) <= 80.0F)
                        {
                            hoveredGarageItem = 2U;
                        }
                        else if (
                            std::abs(
                                virtualX -
                                (menu::virtualWidth - 181.0F)) <=
                                58.0F &&
                            std::abs(
                                virtualY -
                                menu::virtualHeight * 0.5F) <= 80.0F)
                        {
                            hoveredGarageItem = 3U;
                        }
                        else
                        {
                            const float firstColorY =
                                menu::virtualHeight * 0.5F -
                                144.0F;
                            for (std::size_t index = 0U;
                                 index < 7U; ++index)
                            {
                                const float colorY =
                                    firstColorY +
                                    static_cast<float>(index) *
                                        48.0F;
                                if (std::abs(virtualY - colorY) >
                                    22.0F)
                                {
                                    continue;
                                }
                                if (virtualX <= 120.0F)
                                    hoveredGarageItem = 4U + index;
                                else if (
                                    virtualX >=
                                    menu::virtualWidth - 120.0F)
                                {
                                    hoveredGarageItem =
                                        11U + index;
                                }
                                if (hoveredGarageItem)
                                {
                                    if (*hoveredGarageItem >= 4U &&
                                        !garageColorAvailable(
                                            *hoveredGarageItem - 4U))
                                    {
                                        hoveredGarageItem.reset();
                                    }
                                    break;
                                }
                            }
                        }
                }
                if (hoveredGarageItem)
                {
                    menuSelection = *hoveredGarageItem;
                }
                pointerTargetsItem =
                    hoveredGarageItem.has_value() ||
                    event.type == SDL_EVENT_MOUSE_MOTION ||
                    event.button.button != SDL_BUTTON_LEFT;
            }
            else if (
                !inRace &&
                menuStack.back() == MenuScreen::Workshop &&
                (event.type == SDL_EVENT_MOUSE_MOTION ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN))
            {
                int windowWidth = 0;
                int windowHeight = 0;
                const float pointerX =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.x
                        : event.button.x;
                const float pointerY =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.y
                        : event.button.y;
                std::optional<std::size_t> hoveredWorkshopItem;
                bool workshopDialogShown = false;
                if (SDL_GetWindowSize(
                        window, &windowWidth, &windowHeight) &&
                    windowWidth > 0 && windowHeight > 0)
                {
                    const float virtualX =
                        pointerX * menu::virtualWidth /
                        static_cast<float>(windowWidth);
                    const float virtualY =
                        pointerY * menu::virtualHeight /
                        static_cast<float>(windowHeight);
                    workshopDragX = virtualX;
                    workshopDragY = virtualY;
                    const float backY =
                        menu::virtualHeight -
                        static_cast<float>(
                            workshopBottomPanelImage.height) +
                        40.0F;
                    if (virtualX <= 250.0F &&
                        std::abs(virtualY - backY) <= 45.0F)
                    {
                        hoveredWorkshopItem = 0U;
                    }
                    const auto goodCenters =
                        workshopGoodCenters();
                    for (std::size_t index = 0U;
                         !hoveredWorkshopItem &&
                         index < goodCenters.size();
                         ++index)
                    {
                        if (std::abs(
                                virtualX -
                                goodCenters[index][0]) <= 48.0F &&
                            std::abs(
                                virtualY -
                                goodCenters[index][1]) <= 48.0F)
                        {
                            hoveredWorkshopItem = 1U + index;
                            const std::size_t itemIndex =
                                workshopGoodScroll * 3U + index;
                            if (!workshopDrag.active() &&
                                itemIndex < workshopGoods.size())
                            {
                                showWorkshopWeaponDialog(
                                    *workshopGoods[itemIndex],
                                    workshopGoods[itemIndex]->cost,
                                    goodCenters[index][0],
                                    goodCenters[index][1],
                                    100.0F, 100.0F);
                                workshopDialogShown = true;
                            }
                        }
                    }
                    const auto slotCenters =
                        workshopSlotCenters();
                    const auto* workshopCar =
                        originalGarage->findCar(
                            profileState.player.currentCar);
                    for (std::size_t index = 0U;
                         !hoveredWorkshopItem &&
                         index < slotCenters.size();
                         ++index)
                    {
                        if (workshopCar == nullptr ||
                            !workshopCar->placements[index].active)
                            continue;
                        const float slotX =
                            slotCenters[index][0];
                        const float slotY =
                            slotCenters[index][1];
                        const auto* installedItem =
                            originalGarage->findItem(
                                profileState.player.slots[index]
                                    .record);
                        const bool overCharge =
                            installedItem != nullptr &&
                            installedItem->maximumCharge > 0U &&
                            std::abs(
                                virtualX - (slotX + 68.0F)) <=
                                static_cast<float>(
                                    workshopChargeButtonImage.width) *
                                    0.5F &&
                            std::abs(
                                virtualY - (slotY + 27.0F)) <=
                                static_cast<float>(
                                    workshopChargeButtonImage.height) *
                                    0.5F;
                        const bool overLevel =
                            installedItem != nullptr &&
                            installedItem->maximumCharge == 0U &&
                            index < 4U &&
                            std::abs(
                                virtualX - (slotX + 51.0F)) <=
                                static_cast<float>(
                                    workshopUpgradeImages[0].width) *
                                    0.5F &&
                            std::abs(
                                virtualY - (slotY + 38.0F)) <=
                                static_cast<float>(
                                    workshopUpgradeImages[0].height) *
                                    0.5F;
                        const bool overPlane =
                            std::abs(virtualX - slotX) <=
                                static_cast<float>(
                                    workshopSlotImage.width) *
                                    0.5F &&
                            std::abs(virtualY - slotY) <=
                                static_cast<float>(
                                    workshopSlotImage.height) *
                                    0.5F;
                        if (overCharge || overLevel || overPlane)
                        {
                            hoveredWorkshopItem = 13U + index;
                            workshopPointerSlotPlane = overPlane &&
                                !overCharge && !overLevel;
                            if (installedItem != nullptr &&
                                !workshopDrag.active())
                            {
                                const auto* dialogItem =
                                    installedItem;
                                std::uint32_t dialogCost =
                                    installedItem->cost;
                                float senderX = slotX;
                                float senderY = slotY;
                                float senderWidth =
                                    static_cast<float>(
                                        workshopSlotImage.width);
                                float senderHeight =
                                    static_cast<float>(
                                        workshopSlotImage.height);
                                if (overCharge)
                                {
                                    dialogCost =
                                        installedItem->chargeCost *
                                        installedItem->chargeStep;
                                    senderX = slotX + 68.0F;
                                    senderY = slotY + 27.0F;
                                    senderWidth =
                                        static_cast<float>(
                                            workshopChargeButtonImage
                                                .width) *
                                        4.0F;
                                    senderHeight =
                                        static_cast<float>(
                                            workshopChargeButtonImage
                                                .height) *
                                        4.0F;
                                }
                                else if (overLevel)
                                {
                                    const auto slotType =
                                        static_cast<
                                            r3d::game::originalrace::
                                                GarageSlotType>(
                                            index);
                                    const int level =
                                        r3d::game::originalrace::
                                            originalWorkshopUpgradeLevel(
                                                installedItem->record,
                                                slotType);
                                    if (const auto* upgrade =
                                            r3d::game::originalrace::
                                                originalWorkshopUpgradeItem(
                                                    *originalGarage,
                                                    *workshopCar,
                                                    slotType,
                                                    level + 1))
                                    {
                                        dialogItem = upgrade;
                                        dialogCost = upgrade->cost;
                                    }
                                    senderX = slotX + 51.0F;
                                    senderY = slotY + 38.0F;
                                    senderWidth =
                                        static_cast<float>(
                                            workshopUpgradeImages[0]
                                                .width);
                                    senderHeight =
                                        static_cast<float>(
                                            workshopUpgradeImages[0]
                                                .height);
                                }
                                showWorkshopWeaponDialog(
                                    *dialogItem, dialogCost,
                                    senderX, senderY, senderWidth,
                                    senderHeight);
                                workshopDialogShown = true;
                            }
                        }
                    }
                    if (event.type ==
                            SDL_EVENT_MOUSE_BUTTON_DOWN &&
                        event.button.button == SDL_BUTTON_LEFT)
                    {
                        const float panelCenterY =
                            (static_cast<float>(
                                 workshopTopPanelImage.height) -
                                 30.0F +
                             menu::virtualHeight -
                             static_cast<float>(
                                 workshopBottomPanelImage.height)) *
                            0.5F;
                        const float upY =
                            panelCenterY -
                            static_cast<float>(
                                workshopLeftPanelImage.height) *
                                0.5F +
                            65.0F;
                        const float downY =
                            panelCenterY +
                            static_cast<float>(
                                workshopLeftPanelImage.height) *
                                0.5F -
                            42.0F;
                        const float arrowX =
                            30.0F +
                            static_cast<float>(
                                workshopLeftPanelImage.width) *
                                0.5F;
                        const std::size_t rowCount =
                            (workshopGoods.size() + 2U) / 3U;
                        const std::size_t maximumScroll =
                            rowCount > 4U ? rowCount - 4U : 0U;
                        if (std::abs(virtualX - arrowX) <= 35.0F &&
                            std::abs(virtualY - upY) <= 35.0F &&
                            workshopGoodScroll > 0U)
                        {
                            --workshopGoodScroll;
                            refreshWorkshopPage();
                            pointerHandledOriginalOptions = true;
                        }
                        else if (
                            std::abs(virtualX - arrowX) <= 35.0F &&
                            std::abs(virtualY - downY) <= 35.0F &&
                            workshopGoodScroll < maximumScroll)
                        {
                            ++workshopGoodScroll;
                            refreshWorkshopPage();
                            pointerHandledOriginalOptions = true;
                        }
                    }
                }
                if (!workshopDialogShown)
                    hideWorkshopWeaponDialog();
                if (hoveredWorkshopItem)
                {
                    menuSelection = *hoveredWorkshopItem;
                }
                pointerTargetsItem =
                    hoveredWorkshopItem.has_value() ||
                    pointerHandledOriginalOptions ||
                    event.type == SDL_EVENT_MOUSE_MOTION ||
                    event.button.button != SDL_BUTTON_LEFT;
            }
            else if (
                !inRace &&
                menuStack.back() == MenuScreen::Planets &&
                (event.type == SDL_EVENT_MOUSE_MOTION ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN))
            {
                int windowWidth = 0;
                int windowHeight = 0;
                const float pointerX =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.x
                        : event.button.x;
                const float pointerY =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.y
                        : event.button.y;
                std::optional<std::size_t> hoveredAngarItem;
                if (SDL_GetWindowSize(
                        window, &windowWidth, &windowHeight) &&
                    windowWidth > 0 && windowHeight > 0)
                {
                    const float virtualX =
                        pointerX * menu::virtualWidth /
                        static_cast<float>(windowWidth);
                    const float virtualY =
                        pointerY * menu::virtualHeight /
                        static_cast<float>(windowHeight);
                    const auto planetCount = std::min(
                        originalGarage->planets.size(),
                        profileState.player.planets.size());
                        const float panelCenterX =
                            menu::virtualWidth * 0.5F;
                        const float panelCenterY =
                            menu::virtualHeight -
                            static_cast<float>(
                                angarBottomPanelImage.height) *
                                0.5F -
                            20.0F;
                        const float firstPlanetX =
                            panelCenterX -
                            static_cast<float>(
                                angarBottomPanelImage.width) *
                                0.5F +
                            125.0F;
                        const float planetY =
                            panelCenterY -
                            static_cast<float>(
                                angarBottomPanelImage.height) *
                                0.5F +
                            90.0F;
                        for (std::size_t index = 0U;
                             index < planetCount; ++index)
                        {
                            const float centerX =
                                firstPlanetX +
                                static_cast<float>(index) *
                                    224.0F;
                            if (std::abs(virtualX - centerX) <=
                                    90.0F &&
                                std::abs(virtualY - planetY) <=
                                    90.0F)
                            {
                                hoveredAngarItem = index;
                                break;
                            }
                        }
                        const float backX =
                            static_cast<float>(
                                garageBackImage.width) *
                            0.5F;
                        if (!hoveredAngarItem &&
                            std::abs(virtualX - backX) <=
                                static_cast<float>(
                                    garageBackImage.width) *
                                    0.5F &&
                            std::abs(virtualY - 40.0F) <= 40.0F)
                        {
                            hoveredAngarItem = planetCount;
                        }
                        if (event.type ==
                                SDL_EVENT_MOUSE_BUTTON_DOWN &&
                            event.button.button == SDL_BUTTON_LEFT &&
                            angarPlanetIndex >= 0)
                        {
                            const auto selected =
                                static_cast<std::size_t>(
                                    angarPlanetIndex);
                            const float selectedX =
                                firstPlanetX +
                                static_cast<float>(selected) *
                                    224.0F;
                            const float infoWidth =
                                static_cast<float>(
                                    angarPlanetInfoImage.width);
                            const float infoX = std::clamp(
                                selectedX, infoWidth * 0.5F,
                                menu::virtualWidth -
                                    infoWidth * 0.5F);
                            const float infoY =
                                panelCenterY - 260.0F;
                            const float closeX =
                                infoX + 180.0F;
                            const float closeY =
                                infoY -
                                static_cast<float>(
                                    angarPlanetInfoImage.height) *
                                    0.5F +
                                15.0F;
                            if (std::abs(virtualX - closeX) <=
                                    16.0F &&
                                std::abs(virtualY - closeY) <=
                                    16.0F)
                            {
                                menuSelection = planetCount;
                                selectAngarPlanet(-1);
                                pointerHandledOriginalOptions = true;
                            }
                        }
                }
                if (hoveredAngarItem &&
                    !pointerHandledOriginalOptions)
                {
                    menuSelection = *hoveredAngarItem;
                    selectAngarPlanet(
                        menuSelection <
                                originalGarage->planets.size()
                            ? static_cast<int>(menuSelection)
                            : -1);
                }
                pointerTargetsItem =
                    hoveredAngarItem.has_value() ||
                    pointerHandledOriginalOptions ||
                    event.type == SDL_EVENT_MOUSE_MOTION ||
                    event.button.button != SDL_BUTTON_LEFT;
            }
            else if (
                !inRace &&
                menuStack.back() == MenuScreen::Achievements &&
                (event.type == SDL_EVENT_MOUSE_MOTION ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN))
            {
                int windowWidth = 0;
                int windowHeight = 0;
                const float pointerX =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.x
                        : event.button.x;
                const float pointerY =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.y
                        : event.button.y;
                std::optional<std::size_t> hoveredAchievement;
                if (SDL_GetWindowSize(
                        window, &windowWidth, &windowHeight) &&
                    windowWidth > 0 && windowHeight > 0)
                {
                    const float virtualX =
                        pointerX * menu::virtualWidth /
                        static_cast<float>(windowWidth);
                    const float virtualY =
                        pointerY * menu::virtualHeight /
                        static_cast<float>(windowHeight);
                        const float scale = std::min(
                            menu::virtualWidth / 1090.0F,
                            menu::virtualHeight / 720.0F);
                        for (std::size_t reverse = 0U;
                             reverse <
                             originalAchievementVisuals.size();
                             ++reverse)
                        {
                            const std::size_t index =
                                originalAchievementVisuals.size() -
                                1U - reverse;
                            const auto& visual =
                                originalAchievementVisuals[index];
                            const auto& image =
                                achievementState(index) == "asOpened"
                                    ? achievementOpenedImages[index]
                                    : achievementLockedImages[index];
                            const float centerX =
                                menu::virtualWidth * 0.5F +
                                (visual.x - 30.0F) * scale;
                            const float centerY = visual.y * scale;
                            if (std::abs(virtualX - centerX) <=
                                    static_cast<float>(image.width) *
                                        scale * 0.5F &&
                                std::abs(virtualY - centerY) <=
                                    static_cast<float>(image.height) *
                                        scale * 0.5F)
                            {
                                hoveredAchievement = index;
                                break;
                            }
                        }
                        const float backX =
                            static_cast<float>(
                                garageBackImage.width) *
                            0.5F;
                        const float backY =
                            menu::virtualHeight - 80.0F + 17.0F +
                            static_cast<float>(
                                garageBackImage.height) *
                                0.5F;
                        if (!hoveredAchievement &&
                            std::abs(virtualX - backX) <=
                                static_cast<float>(
                                    garageBackImage.width) *
                                    0.5F &&
                            std::abs(virtualY - backY) <=
                                static_cast<float>(
                                    garageBackImage.height) *
                                    0.5F)
                        {
                            hoveredAchievement =
                                originalAchievementBack;
                        }
                }
                if (hoveredAchievement)
                {
                    menuSelection = *hoveredAchievement;
                }
                pointerTargetsItem =
                    hoveredAchievement.has_value() ||
                    event.type == SDL_EVENT_MOUSE_MOTION ||
                    event.button.button != SDL_BUTTON_LEFT;
            }
            else if (!inRace &&
                menuStack.back() == MenuScreen::RaceMenu &&
                (event.type == SDL_EVENT_MOUSE_MOTION ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN))
            {
                int windowWidth = 0;
                int windowHeight = 0;
                const float pointerX =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.x
                        : event.button.x;
                const float pointerY =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.y
                        : event.button.y;
                std::optional<std::size_t> hoveredRaceMenuItem;
                bool pointerHandledNetworkKick = false;
                if (SDL_GetWindowSize(
                        window, &windowWidth, &windowHeight) &&
                    windowWidth > 0 && windowHeight > 0)
                {
                    const float virtualX =
                        pointerX * menu::virtualWidth /
                        static_cast<float>(windowWidth);
                    const float virtualY =
                        pointerY * menu::virtualHeight /
                        static_cast<float>(windowHeight);
#ifdef RRR3D_NETWORK
                    networkKickHoverOwner.reset();
                    if (networkHostRequested)
                    {
                        for (std::size_t index = 0U;
                             index < networkRacePlayerVisuals.size(); ++index)
                        {
                            const auto layout =
                                networkRacePlayerCenter(index);
                            const float kickX =
                                layout[0] + 128.0F * layout[2];
                            const float kickY = layout[1] - 68.0F;
                            if (std::abs(virtualX - kickX) > 20.0F ||
                                std::abs(virtualY - kickY) > 20.0F)
                            {
                                continue;
                            }
                            const auto ownerId =
                                networkRacePlayerVisuals[index]
                                    .player.ownerId;
                            networkKickHoverOwner = ownerId;
                            pointerHandledNetworkKick = true;
                            if (event.type ==
                                    SDL_EVENT_MOUSE_BUTTON_DOWN &&
                                event.button.button == SDL_BUTTON_LEFT)
                            {
#ifdef RRR3D_AUDIO
                                playMainButtonClick();
#endif
                                std::string error;
                                if (!networkSession.disconnectPlayer(
                                        ownerId, error))
                                {
                                    std::cerr
                                        << "Original NetGame::"
                                           "DisconnectPlayer failed: "
                                        << error << '\n';
                                }
                                renderedNetworkRevision =
                                    std::numeric_limits<
                                        std::uint64_t>::max();
                                refreshNetworkRuntimePages();
                                refreshNetworkRacePlayerVisuals();
                            }
                            break;
                        }
                    }
#endif
                    constexpr float itemWidth = 110.0F;
                    constexpr float itemSpacing = 50.0F;
                    const float firstX =
                        menu::virtualWidth * 0.5F -
                        (7.0F * itemWidth +
                         6.0F * itemSpacing) *
                            0.5F +
                        itemWidth * 0.5F;
                    const float itemY =
                        menu::virtualHeight -
                        static_cast<float>(
                            raceBottomPanelImage.height) *
                            0.5F -
                        72.0F;
                    for (std::size_t index = 0U;
                         !pointerHandledNetworkKick &&
                         index < raceMenuIcons.size(); ++index)
                    {
                        const float itemX =
                            firstX +
                            static_cast<float>(index) *
                                (itemWidth + itemSpacing);
                        if (std::abs(virtualX - itemX) <= 68.0F &&
                            std::abs(virtualY - itemY) <= 62.0F)
                        {
                            hoveredRaceMenuItem = index;
                            break;
                        }
                    }
                }
                if (hoveredRaceMenuItem &&
                    raceMenuPage.enabled[*hoveredRaceMenuItem])
                {
#ifdef RRR3D_AUDIO
                    if (event.type == SDL_EVENT_MOUSE_MOTION &&
                        menuSelection != *hoveredRaceMenuItem)
                    {
                        // RaceMenu icon buttons use ssButton2::mouseEnter.
                        playOriginalMenuSound(
                            rrr3d::audio::OriginalMenuSound::Rollover);
                    }
#endif
                    menuSelection = *hoveredRaceMenuItem;
                }
                else if (hoveredRaceMenuItem)
                {
                    hoveredRaceMenuItem.reset();
                }
                pointerTargetsItem =
                    !pointerHandledNetworkKick &&
                    (hoveredRaceMenuItem.has_value() ||
                     event.type == SDL_EVENT_MOUSE_MOTION ||
                     event.button.button != SDL_BUTTON_LEFT);
            }
            else if (!inRace &&
                isOriginalOptionsScreen(menuStack.back()) &&
                (event.type == SDL_EVENT_MOUSE_MOTION ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN))
            {
                int windowWidth = 0;
                int windowHeight = 0;
                const float pointerX =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.x
                        : event.button.x;
                const float pointerY =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.y
                        : event.button.y;
                std::optional<std::size_t> hoveredOption;
                std::optional<std::size_t> hoveredState;
                if (SDL_GetWindowSize(
                        window, &windowWidth, &windowHeight) &&
                    windowWidth > 0 && windowHeight > 0)
                {
                    const float virtualX =
                        pointerX * menu::virtualWidth /
                        static_cast<float>(windowWidth);
                    const float virtualY =
                        pointerY * menu::virtualHeight /
                        static_cast<float>(windowHeight);
                    const float centerX =
                        menu::virtualWidth * 0.5F;
                    const float centerY =
                        menu::virtualHeight * 0.5F;

                    for (std::size_t state = 0U; state < 4U;
                         ++state)
                    {
                        const float stateY =
                            centerY - 125.0F +
                            static_cast<float>(state) * 100.0F;
                        if (virtualX >= centerX - 600.0F &&
                            virtualX <= centerX - 250.0F &&
                            std::abs(virtualY - stateY) <= 45.0F)
                        {
                            hoveredState = state;
                            break;
                        }
                    }

                    const std::size_t rowCount =
                        menuStack.back() == MenuScreen::GameOptions
                            ? gameOptionNamesPage.labels.size()
                        : menuStack.back() ==
                                  MenuScreen::GraphicsOptions
                            ? graphicsOptionNamesPage.labels.size()
                        : menuStack.back() ==
                                  MenuScreen::SoundOptions
                            ? soundOptionNamesPage.labels.size()
                            : originalControlActions.size();
                    const std::size_t visibleRows =
                        menuStack.back() == MenuScreen::GameOptions
                            ? 7U
                        : menuStack.back() ==
                                  MenuScreen::ControlsOptions
                            ? controlsVisibleRows
                            : rowCount;
                    const std::size_t scrollAnchor =
                        menuSelection < rowCount
                            ? menuSelection
                            : rowCount - 1U;
                    const std::size_t firstVisible =
                        scrollAnchor < visibleRows
                            ? 0U
                            : std::min(
                                  scrollAnchor - visibleRows + 1U,
                                  rowCount - visibleRows);
                    const float firstRowY =
                        centerY -
                        (menuStack.back() ==
                                 MenuScreen::ControlsOptions
                             ? 130.0F
                         : menuStack.back() ==
                                   MenuScreen::GameOptions
                             ? 178.0F
                             : 174.0F);
                    if (virtualX >= centerX - 420.0F &&
                        virtualX <= centerX + 520.0F)
                    {
                        for (std::size_t slot = 0U;
                             slot < visibleRows; ++slot)
                        {
                            const float rowY =
                                firstRowY +
                                static_cast<float>(slot) * 50.0F;
                            if (std::abs(virtualY - rowY) <= 23.0F)
                            {
                                hoveredOption =
                                    firstVisible + slot;
                                if (menuStack.back() ==
                                        MenuScreen::
                                            ControlsOptions &&
                                    virtualX >= centerX + 165.0F)
                                {
                                    controlsUseGamepad =
                                        virtualX >=
                                        centerX + 345.0F;
                                }
                                break;
                            }
                        }
                    }
                    const float actionY = centerY + 240.0F;
                    if (std::abs(virtualY - actionY) <= 50.0F)
                    {
                        if (virtualX >= centerX - 80.0F &&
                            virtualX <= centerX + 280.0F)
                            hoveredOption = rowCount;
                        else if (
                            virtualX >= centerX + 280.0F &&
                            virtualX <= centerX + 650.0F)
                            hoveredOption = rowCount + 1U;
                    }
                    if (event.type ==
                            SDL_EVENT_MOUSE_BUTTON_DOWN &&
                        event.button.button == SDL_BUTTON_LEFT)
                    {
                        const float upY =
                            centerY -
                            (menuStack.back() ==
                                     MenuScreen::ControlsOptions
                                 ? 150.0F
                                 : 200.0F);
                        const float downY = centerY + 195.0F;
                        if (std::abs(
                                virtualX -
                                (centerX + 150.0F)) <= 35.0F &&
                            std::abs(virtualY - upY) <= 35.0F &&
                            menuSelection > 0U)
                        {
                            --menuSelection;
                            pointerHandledOriginalOptions = true;
                        }
                        else if (
                            std::abs(
                                virtualX -
                                (centerX + 150.0F)) <= 35.0F &&
                            std::abs(virtualY - downY) <= 35.0F &&
                            menuSelection + 1U < rowCount)
                        {
                            ++menuSelection;
                            pointerHandledOriginalOptions = true;
                        }
                        else if (hoveredState)
                        {
                            setOptionsState(*hoveredState);
                            pointerHandledOriginalOptions = true;
                        }
                    }
                }
                if (hoveredOption)
                    menuSelection = *hoveredOption;
                pointerTargetsItem =
                    hoveredOption.has_value() ||
                    hoveredState.has_value() ||
                    event.type == SDL_EVENT_MOUSE_MOTION ||
                    event.button.button != SDL_BUTTON_LEFT;
            }
            else
#endif
            if (
#ifdef RRR3D_PHYSICS
                !inRace &&
#endif
                menuStack.back() == MenuScreen::Credits &&
                (event.type == SDL_EVENT_MOUSE_MOTION ||
                 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN))
            {
                int windowWidth = 0;
                int windowHeight = 0;
                const float pointerX =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.x
                        : event.button.x;
                const float pointerY =
                    event.type == SDL_EVENT_MOUSE_MOTION
                        ? event.motion.y
                        : event.button.y;
                bool hoveredBack = false;
                if (SDL_GetWindowSize(
                        window, &windowWidth, &windowHeight) &&
                    windowWidth > 0 && windowHeight > 0)
                {
                    const float virtualX =
                        pointerX * menu::virtualWidth /
                        static_cast<float>(windowWidth);
                    const float virtualY =
                        pointerY * menu::virtualHeight /
                        static_cast<float>(windowHeight);
                    const float backX =
                        static_cast<float>(finalBackImage.width) * 0.5F;
                    const float backY =
                        menu::virtualHeight - 60.0F;
                    hoveredBack =
                        std::abs(virtualX - backX) <=
                            static_cast<float>(finalBackImage.width) * 0.5F &&
                        std::abs(virtualY - backY) <=
                            static_cast<float>(finalBackImage.height) * 0.5F;
                }
                menuSelection = 0U;
                pointerTargetsItem =
                    hoveredBack ||
                    event.type == SDL_EVENT_MOUSE_MOTION ||
                    event.button.button != SDL_BUTTON_LEFT;
            }
            else if (
#ifdef RRR3D_PHYSICS
                !inRace &&
#endif
                event.type == SDL_EVENT_MOUSE_MOTION)
            {
                const auto& hoverPage = activeMenuPage();
                const auto hovered = hoveredItem(
                    window, event.motion.x, event.motion.y,
                    hoverPage.labels.size(),
                    static_cast<float>(model->selectionImage.width),
                    static_cast<float>(model->selectionImage.height),
                    usesSharedBackPosition(menuStack.back()));
                if (hovered && hoverPage.enabled[*hovered])
                    menuSelection = *hovered;
            }
            else if (
#ifdef RRR3D_PHYSICS
                !inRace &&
#endif
                event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            {
                const auto& hoverPage = activeMenuPage();
                const auto hovered = hoveredItem(
                    window, event.button.x, event.button.y,
                    hoverPage.labels.size(),
                    static_cast<float>(model->selectionImage.width),
                    static_cast<float>(model->selectionImage.height),
                    usesSharedBackPosition(menuStack.back()));
                const bool enabledHover =
                    hovered && hoverPage.enabled[*hovered];
                pointerTargetsItem = enabledHover ||
                                     event.button.button != SDL_BUTTON_LEFT;
                if (enabledHover)
                    menuSelection = *hovered;
            }
            const auto inputEvents = input.processEvent(event);
            for (const auto& inputEvent : inputEvents)
            {
#ifdef RRR3D_PHYSICS
                if (bindingCaptureAction &&
                    acceptDialog.disableFocus &&
                    inputEvent.active &&
                    !inputEvent.repeated &&
                    inputEvent.source ==
                        rrr3d::input::Source::Mouse &&
                    inputEvent.action ==
                        rrr3d::input::Action::MenuConfirm)
                {
                    if (!pointerAcceptChoice)
                        continue;
#ifdef RRR3D_AUDIO
                    playMainButtonClick();
#endif
                    if (*pointerAcceptChoice)
                    {
                        auto& bindings =
                            bindingCaptureGamepad
                                ? optionsDraftConfig
                                      .gamepadControls
                                : optionsDraftConfig
                                      .keyboardControls;
                        bindings[*bindingCaptureAction] = "None";
                    }
                    bindingCaptureAction.reset();
                    refreshCurrentOptionsPage();
                    continue;
                }
                if (infoDialog.visible)
                {
                    if (!inputEvent.active || inputEvent.repeated)
                        continue;
                    if (!infoDialog.dismissable)
                        continue;
                    if (inputEvent.action ==
                        rrr3d::input::Action::MenuConfirm)
                    {
                        if (inputEvent.source ==
                                rrr3d::input::Source::Mouse &&
                            !pointerTargetsItem)
                        {
                            continue;
                        }
#ifdef RRR3D_AUDIO
                        playMainButtonClick();
#endif
#ifdef RRR3D_NETWORK
                        const auto networkAction =
                            networkFailureDialogAction;
#endif
                        hideInfoDialog();
#ifdef RRR3D_NETWORK
                        if (networkAction ==
                            NetworkFailureDialogAction::ExitMatch)
                        {
                            exitNetworkMatch(false);
                        }
#endif
                    }
                    continue;
                }
#ifdef RRR3D_NETWORK
                if (!inRace && networkLeaverStartDialogVisible)
                {
                    if (!inputEvent.active || inputEvent.repeated)
                        continue;
                    if (inputEvent.action ==
                            rrr3d::input::Action::TurnLeft ||
                        inputEvent.action ==
                            rrr3d::input::Action::MenuUp)
                    {
                        networkLeaverStartYesFocused = true;
                    }
                    else if (inputEvent.action ==
                                 rrr3d::input::Action::TurnRight ||
                             inputEvent.action ==
                                 rrr3d::input::Action::MenuDown)
                    {
                        networkLeaverStartYesFocused = false;
                    }
                    else if (inputEvent.action ==
                                 rrr3d::input::Action::MenuBack ||
                             inputEvent.action ==
                                 rrr3d::input::Action::Pause)
                    {
                        networkLeaverStartDialogVisible = false;
                    }
                    else if (inputEvent.action ==
                             rrr3d::input::Action::MenuConfirm)
                    {
                        if (inputEvent.source ==
                                rrr3d::input::Source::Mouse &&
                            !pointerTargetsItem)
                        {
                            continue;
                        }
#ifdef RRR3D_AUDIO
                        playMainButtonClick();
#endif
                        const bool accepted =
                            networkLeaverStartYesFocused;
                        networkLeaverStartDialogVisible = false;
                        if (accepted)
                            startCurrentRace();
                    }
                    continue;
                }
#endif
                if (inRace && exitRaceDialogVisible)
                {
                    if (!inputEvent.active || inputEvent.repeated)
                        continue;
                    if (inputEvent.action ==
                            rrr3d::input::Action::TurnLeft ||
                        inputEvent.action ==
                            rrr3d::input::Action::MenuUp)
                    {
                        exitRaceYesFocused = true;
                    }
                    else if (inputEvent.action ==
                                 rrr3d::input::Action::TurnRight ||
                             inputEvent.action ==
                                 rrr3d::input::Action::MenuDown)
                    {
                        exitRaceYesFocused = false;
                    }
                    else if (inputEvent.action ==
                                 rrr3d::input::Action::MenuBack ||
                             inputEvent.action ==
                                 rrr3d::input::Action::Pause)
                    {
                        closeExitRaceDialog();
                        racePauseResumeObserved = true;
                    }
                    else if (inputEvent.action ==
                             rrr3d::input::Action::MenuConfirm)
                    {
                        if (inputEvent.source ==
                                rrr3d::input::Source::Mouse &&
                            !pointerTargetsItem)
                            continue;
                        if (exitRaceYesFocused)
                        {
#ifdef RRR3D_NETWORK
                            // HudMenu::OnClick exits only the race for a
                            // host, but a client also leaves the whole match.
                            if (networkMatchStarted &&
                                !networkHostRequested)
                            {
                                exitNetworkMatch(true);
                            }
                            else
#endif
                            {
                                leaveCurrentRace();
                            }
                        }
                        else
                        {
                            closeExitRaceDialog();
                            racePauseResumeObserved = true;
                        }
                    }
                    continue;
                }
                if (inRace)
                {
                    switch (inputEvent.action)
                    {
                    case rrr3d::input::Action::Accelerate:
                        raceInput.throttle = inputEvent.active
                                                 ? inputEvent.value
                                                 : 0.0F;
                        break;
                    case rrr3d::input::Action::Brake:
                        raceInput.reverse = inputEvent.active
                                                ? inputEvent.value
                                                : 0.0F;
                        break;
                    case rrr3d::input::Action::TurnLeft:
                        if (inputEvent.active)
                            raceInput.steering = inputEvent.value;
                        else if (raceInput.steering > 0.0F)
                            raceInput.steering = 0.0F;
                        break;
                    case rrr3d::input::Action::TurnRight:
                        if (inputEvent.active)
                            raceInput.steering = -inputEvent.value;
                        else if (raceInput.steering < 0.0F)
                            raceInput.steering = 0.0F;
                        break;
                    case rrr3d::input::Action::UseWeapon:
                        if (inputEvent.active && !inputEvent.repeated)
                            raceUseWeaponRequested = true;
                        break;
                    case rrr3d::input::Action::UseAllWeapons:
                        if (inputEvent.active && !inputEvent.repeated)
                            raceUseAllWeaponsRequested = true;
                        break;
                    case rrr3d::input::Action::UseMine:
                        raceUseMine = inputEvent.active;
                        break;
                    case rrr3d::input::Action::UseHyper:
                        raceUseHyper = inputEvent.active;
                        break;
                    case rrr3d::input::Action::ChangeWeapon:
                    case rrr3d::input::Action::NextWeapon:
                        if (inputEvent.active && !inputEvent.repeated)
                        {
                            raceChangeWeaponRequested = true;
                            raceWeaponChangeDirection = 1;
                        }
                        break;
                    case rrr3d::input::Action::PreviousWeapon:
                        if (inputEvent.active && !inputEvent.repeated)
                        {
                            raceChangeWeaponRequested = true;
                            raceWeaponChangeDirection = -1;
                        }
                        break;
                    case rrr3d::input::Action::SelectWeapon1:
                    case rrr3d::input::Action::SelectWeapon2:
                    case rrr3d::input::Action::SelectWeapon3:
                    case rrr3d::input::Action::SelectWeapon4:
                        if (inputEvent.active && !inputEvent.repeated)
                        {
                            raceFireWeaponSlotRequested =
                                static_cast<int>(inputEvent.action) -
                                static_cast<int>(
                                    rrr3d::input::Action::SelectWeapon1);
                        }
                        break;
                    case rrr3d::input::Action::ToggleCamera:
                        if (inputEvent.active && !inputEvent.repeated)
                        {
                            using CameraStyle =
                                r3d::game::originalrace::PreferredCamera;
                            profileState.config.preferredCamera =
                                profileState.config.preferredCamera ==
                                        CameraStyle::Isometric
                                    ? CameraStyle::ThirdPerson
                                    : CameraStyle::Isometric;
                            raceRenderer.resetCamera();
                            saveRaceProfile();
                        }
                        break;
                    case rrr3d::input::Action::ResetVehicle:
                        if (inputEvent.active && !inputEvent.repeated)
                            raceResetRequested = true;
                        break;
                    case rrr3d::input::Action::Pause:
                        if (inputEvent.active && !inputEvent.repeated)
                            openExitRaceDialog();
                        break;
                    case rrr3d::input::Action::MenuBack:
                        // GUI Back is deliberately ignored during gameplay.
                        // The source exits through gaEscape/Start; on the
                        // default gamepad B is the brake and must never leave
                        // the race merely because it is also GUI Back.
                        break;
                    default:
                        break;
                    }
                    continue;
                }
#endif
                if (pointerHandledOriginalOptions &&
                    inputEvent.source ==
                        rrr3d::input::Source::Mouse &&
                    inputEvent.action ==
                        rrr3d::input::Action::MenuConfirm)
                {
                    continue;
                }
                if (!pointerTargetsItem &&
                    inputEvent.source == rrr3d::input::Source::Mouse &&
                    inputEvent.action ==
                        rrr3d::input::Action::MenuConfirm)
                    continue;
                if (!inputEvent.active)
                    continue;
#ifdef RRR3D_PHYSICS
                if (menuStack.back() == MenuScreen::Gamers)
                {
                    const auto previous =
                        adjacentGamerIndex(gamerPlanetIndex, -1);
                    const auto next =
                        adjacentGamerIndex(gamerPlanetIndex, 1);
                    if (inputEvent.action ==
                            rrr3d::input::Action::MenuUp ||
                        inputEvent.action ==
                            rrr3d::input::Action::MenuDown)
                    {
                        if (gamersFocus == GamersFocus::Next)
                            gamersFocus = next ? GamersFocus::Right
                                               : GamersFocus::Left;
                        else
                            gamersFocus = GamersFocus::Next;
                    }
                    else if (
                        inputEvent.action ==
                            rrr3d::input::Action::TurnLeft ||
                        inputEvent.action ==
                            rrr3d::input::Action::TurnRight)
                    {
                        if (gamersFocus == GamersFocus::Left && next)
                            gamersFocus = GamersFocus::Right;
                        else if (
                            gamersFocus == GamersFocus::Right &&
                            previous)
                            gamersFocus = GamersFocus::Left;
                    }
                    else if (
                        !inputEvent.repeated &&
                        inputEvent.action ==
                            rrr3d::input::Action::PreviousWeapon &&
                        previous)
                    {
                        selectGamerPlanet(*previous);
                    }
                    else if (
                        !inputEvent.repeated &&
                        inputEvent.action ==
                            rrr3d::input::Action::NextWeapon && next)
                    {
                        selectGamerPlanet(*next);
                    }
                    else if (
                        !inputEvent.repeated &&
                        inputEvent.action ==
                            rrr3d::input::Action::MenuConfirm)
                    {
#ifdef RRR3D_AUDIO
                        playMainButtonClick();
#endif
                        if (gamersFocus == GamersFocus::Left && previous)
                            selectGamerPlanet(*previous);
                        else if (
                            gamersFocus == GamersFocus::Right && next)
                            selectGamerPlanet(*next);
                        else if (gamersFocus == GamersFocus::Next)
                            confirmOriginalGamer();
                    }
                    continue;
                }
                if (menuStack.back() == MenuScreen::Profiles)
                {
                    const auto profileCount =
                        profileState.profiles.size();
                    const auto visibleEnd = std::min(
                        profileGridScroll + 4U, profileCount);
                    const bool canScrollUp =
                        profileGridScroll > 0U;
                    const bool canScrollDown =
                        profileGridScroll + 4U < profileCount;
                    auto focusFirstVisible = [&]() {
                        if (profileGridScroll < visibleEnd)
                        {
                            profileFocus = ProfileFocus::Item;
                            profileFocusIndex = profileGridScroll;
                        }
                        else
                        {
                            profileFocus = ProfileFocus::Back;
                        }
                    };
                    auto focusLastVisible = [&]() {
                        if (profileGridScroll < visibleEnd)
                        {
                            profileFocus = ProfileFocus::Item;
                            profileFocusIndex = visibleEnd - 1U;
                        }
                        else
                        {
                            profileFocus = ProfileFocus::Back;
                        }
                    };

                    if (profileDeleteDialogVisible)
                    {
                        if (inputEvent.action ==
                                rrr3d::input::Action::TurnLeft ||
                            inputEvent.action ==
                                rrr3d::input::Action::MenuUp)
                        {
                            profileDeleteYesFocused = true;
                        }
                        else if (
                            inputEvent.action ==
                                rrr3d::input::Action::TurnRight ||
                            inputEvent.action ==
                                rrr3d::input::Action::MenuDown)
                        {
                            profileDeleteYesFocused = false;
                        }
                        else if (
                            !inputEvent.repeated &&
                            (inputEvent.action ==
                                 rrr3d::input::Action::MenuBack ||
                             inputEvent.action ==
                                 rrr3d::input::Action::Pause))
                        {
                            profileDeleteDialogVisible = false;
                        }
                        else if (
                            !inputEvent.repeated &&
                            inputEvent.action ==
                                rrr3d::input::Action::MenuConfirm)
                        {
                            if (profileDeleteYesFocused &&
                                profileDeleteIndex < profileCount)
                            {
                                saveRaceProfile();
                                const auto previousProfile =
                                    profileState.player.name;
                                std::string profileError;
                                if (!profileStore.deleteProfile(
                                        profileState,
                                        profileState.profiles[
                                            profileDeleteIndex],
                                        profileError))
                                {
                                    std::cerr
                                        << "ProfileFrame delete failed: "
                                        << profileError << '\n';
                                }
                                else
                                {
                                    refreshProfilePage();
                                    if (!profileState.profiles.empty() &&
                                        profileState.player.name !=
                                            previousProfile)
                                    {
                                        selectedTrack =
                                            r3d::game::originalrace::
                                                resolveOriginalTournamentTrack(
                                                    *originalRace,
                                                    profileState.player);
                                        if (!reloadCurrentRace())
                                        {
                                            runtimeSmokeFailed = true;
                                            running = false;
                                        }
                                    }
                                    std::cout
                                        << "Race::DelProfile: "
                                        << profileDeleteIndex << '\n';
                                }
                            }
                            profileDeleteDialogVisible = false;
                            profileDeleteIndex =
                                std::numeric_limits<
                                    std::size_t>::max();
                            profileFocus = ProfileFocus::Back;
                            profileFocusIndex = 0U;
                        }
                        continue;
                    }

                    if (!inputEvent.repeated &&
                        (inputEvent.action ==
                             rrr3d::input::Action::MenuBack ||
                         inputEvent.action ==
                             rrr3d::input::Action::Pause))
                    {
                        backMenu();
                        continue;
                    }
                    if (inputEvent.action ==
                            rrr3d::input::Action::TurnLeft ||
                        inputEvent.action ==
                            rrr3d::input::Action::TurnRight)
                    {
                        if (profileFocus == ProfileFocus::Item)
                            profileFocus = ProfileFocus::Close;
                        else if (
                            profileFocus == ProfileFocus::Close)
                            profileFocus = ProfileFocus::Item;
                        continue;
                    }
                    if (inputEvent.action ==
                        rrr3d::input::Action::MenuUp)
                    {
                        switch (profileFocus)
                        {
                        case ProfileFocus::Item:
                        case ProfileFocus::Close:
                            if (profileFocusIndex >
                                profileGridScroll)
                                --profileFocusIndex;
                            else if (canScrollUp)
                                profileFocus = ProfileFocus::Up;
                            else
                                profileFocus = ProfileFocus::Back;
                            break;
                        case ProfileFocus::Up:
                            profileFocus = ProfileFocus::Back;
                            break;
                        case ProfileFocus::Down:
                            focusLastVisible();
                            break;
                        case ProfileFocus::Back:
                            if (canScrollDown)
                                profileFocus = ProfileFocus::Down;
                            else
                                focusLastVisible();
                            break;
                        }
                        continue;
                    }
                    if (inputEvent.action ==
                        rrr3d::input::Action::MenuDown)
                    {
                        switch (profileFocus)
                        {
                        case ProfileFocus::Item:
                        case ProfileFocus::Close:
                            if (profileFocusIndex + 1U <
                                visibleEnd)
                                ++profileFocusIndex;
                            else if (canScrollDown)
                                profileFocus = ProfileFocus::Down;
                            else
                                profileFocus = ProfileFocus::Back;
                            break;
                        case ProfileFocus::Up:
                            focusFirstVisible();
                            break;
                        case ProfileFocus::Down:
                            profileFocus = ProfileFocus::Back;
                            break;
                        case ProfileFocus::Back:
                            if (canScrollUp)
                                profileFocus = ProfileFocus::Up;
                            else
                                focusFirstVisible();
                            break;
                        }
                        continue;
                    }
                    if (inputEvent.action !=
                            rrr3d::input::Action::MenuConfirm ||
                        inputEvent.repeated)
                    {
                        continue;
                    }
                    switch (profileFocus)
                    {
                    case ProfileFocus::Back:
                        backMenu();
                        break;
                    case ProfileFocus::Up:
                        if (canScrollUp)
                            --profileGridScroll;
                        break;
                    case ProfileFocus::Down:
                        if (canScrollDown)
                            ++profileGridScroll;
                        break;
                    case ProfileFocus::Close:
                        if (profileFocusIndex < profileCount)
                        {
                            profileDeleteIndex =
                                profileFocusIndex;
                            profileDeleteYesFocused = true;
                            profileDeleteDialogVisible = true;
                            showAcceptDialog(
                                localized(
                                    "svHintDeleteProfile"),
                                localized("svYes"),
                                localized("svNo"),
                                menu::virtualWidth * 0.5F,
                                menu::virtualHeight * 0.5F);
                            std::cout
                                << "ProfileFrame: "
                                << localized(
                                       "svHintDeleteProfile")
                                << '\n';
                        }
                        break;
                    case ProfileFocus::Item:
                        if (profileFocusIndex < profileCount)
                        {
                            saveRaceProfile();
                            std::string profileError;
                            if (!profileStore.selectProfile(
                                    profileState,
                                    profileState.profiles[
                                        profileFocusIndex],
                                    profileError))
                            {
                                std::cerr
                                    << "ProfileFrame load failed: "
                                    << profileError << '\n';
                                break;
                            }
                            std::cout
                                << "ProfileFrame selection: "
                                << profileState.player.name
                                << '\n';
                            championshipMode = true;
                            selectedTrack =
                                r3d::game::originalrace::
                                    resolveOriginalTournamentTrack(
                                        *originalRace,
                                        profileState.player);
                            if (!reloadCurrentRace())
                            {
                                runtimeSmokeFailed = true;
                                running = false;
                                break;
                            }
                            input.applyKeyboardBindings(
                                profileState.config.keyboardControls);
                            input.applyGamepadBindings(
                                profileState.config.gamepadControls);
                            saveRaceProfile();
                            menuStack.pop_back();
                            showOriginalRaceMenu();
                        }
                        break;
                    }
                    continue;
                }
                if (menuStack.back() == MenuScreen::Garage)
                {
                    if (garagePurchaseDialogVisible)
                    {
                        if (inputEvent.action ==
                                rrr3d::input::Action::TurnLeft ||
                            inputEvent.action ==
                                rrr3d::input::Action::MenuUp)
                        {
                            garagePurchaseYesFocused = true;
                        }
                        else if (
                            inputEvent.action ==
                                rrr3d::input::Action::TurnRight ||
                            inputEvent.action ==
                                rrr3d::input::Action::MenuDown)
                        {
                            garagePurchaseYesFocused = false;
                        }
                        else if (
                            !inputEvent.repeated &&
                            (inputEvent.action ==
                                 rrr3d::input::Action::MenuBack ||
                             inputEvent.action ==
                                 rrr3d::input::Action::Pause))
                        {
                            garagePurchaseDialogVisible = false;
                        }
                        else if (
                            !inputEvent.repeated &&
                            inputEvent.action ==
                                rrr3d::input::Action::MenuConfirm)
                        {
#ifdef RRR3D_AUDIO
                            playMainButtonClick();
#endif
                            if (!garagePurchaseYesFocused)
                            {
                                garagePurchaseDialogVisible = false;
                            }
                            else
                            {
                                const auto& car =
                                    originalGarage
                                        ->cars[garageCarIndex];
                                std::string garageError;
                                if (r3d::game::originalrace::
                                        selectOriginalGarageCar(
                                            *originalGarage,
                                            profileState, car,
                                            championshipMode,
                                            garageError))
                                {
                                    garagePurchaseDialogVisible =
                                        false;
                                    saveRaceProfile();
                                    backMenu();
                                }
                                else
                                {
                                    std::cerr
                                        << "Original GarageFrame: "
                                        << garageError << '\n';
                                    garagePurchaseDialogVisible =
                                        false;
                                    showInfoDialog(
                                        localized("svWarning"),
                                        localized(
                                            "svHintCantMoney"),
                                        localized("svOk"),
                                        menu::virtualWidth * 0.5F,
                                        menu::virtualHeight * 0.5F);
                                    refreshGaragePage();
                                }
                            }
                        }
                        continue;
                    }

                    constexpr std::size_t garageFocusCount = 18U;
                    if (inputEvent.action ==
                        rrr3d::input::Action::MenuUp)
                    {
                        moveGarageFocus(-1);
                        continue;
                    }
                    if (inputEvent.action ==
                        rrr3d::input::Action::MenuDown)
                    {
                        moveGarageFocus(1);
                        continue;
                    }
                    if (inputEvent.action ==
                            rrr3d::input::Action::TurnLeft ||
                        inputEvent.action ==
                            rrr3d::input::Action::TurnRight)
                    {
                        if (!inputEvent.repeated &&
                            !garageCarOrder.empty())
                        {
                            if (inputEvent.action ==
                                    rrr3d::input::Action::
                                        TurnLeft &&
                                garageViewIndex > 0U)
                            {
                                --garageViewIndex;
                            }
                            else if (
                                inputEvent.action ==
                                    rrr3d::input::Action::
                                        TurnRight &&
                                garageViewIndex + 1U <
                                    garageCarOrder.size())
                            {
                                ++garageViewIndex;
                            }
                            refreshGaragePage();
                        }
                        continue;
                    }
                    if (inputEvent.repeated)
                        continue;
                    if (inputEvent.action ==
                            rrr3d::input::Action::MenuBack ||
                        inputEvent.action ==
                            rrr3d::input::Action::Pause)
                    {
#ifdef RRR3D_AUDIO
                        playMainButtonClick();
#endif
                        backMenu();
                        continue;
                    }
                    if (inputEvent.action !=
                        rrr3d::input::Action::MenuConfirm)
                    {
                        continue;
                    }
#ifdef RRR3D_AUDIO
                    playOriginalMenuSound(
                        menuSelection >= 4U
                            ? rrr3d::audio::OriginalMenuSound::Repaint
                            : rrr3d::audio::OriginalMenuSound::ButtonClick);
#endif
                    if (menuSelection == 0U)
                    {
                        backMenu();
                    }
                    else if (
                        menuSelection == 1U &&
                        !garageCarOrder[garageViewIndex].locked)
                    {
                        const auto& car =
                            originalGarage->cars[garageCarIndex];
                        if (car.record ==
                            profileState.player.currentCar)
                        {
                            backMenu();
                        }
                        else if (championshipMode)
                        {
                            garagePurchaseYesFocused = true;
                            garagePurchaseDialogVisible = true;
                            std::string purchase =
                                localized("svBuyCar");
                            if (const auto marker =
                                    purchase.find("%s");
                                marker != std::string::npos)
                            {
                                purchase.replace(
                                    marker, 2U,
                                    originalCurrency(car.cost));
                            }
                            showAcceptDialog(
                                purchase, localized("svYes"),
                                localized("svNo"),
                                menu::virtualWidth * 0.5F,
                                menu::virtualHeight * 0.5F);
                        }
                        else
                        {
                            std::string garageError;
                            if (r3d::game::originalrace::
                                    selectOriginalGarageCar(
                                        *originalGarage,
                                        profileState, car, false,
                                        garageError))
                            {
                                saveRaceProfile();
                                backMenu();
                            }
                            else
                            {
                                std::cerr
                                    << "Original GarageFrame: "
                                    << garageError << '\n';
                            }
                        }
                    }
                    else if (menuSelection == 2U &&
                             garageViewIndex > 0U)
                    {
                        --garageViewIndex;
                        refreshGaragePage();
                    }
                    else if (
                        menuSelection == 3U &&
                        garageViewIndex + 1U <
                            garageCarOrder.size())
                    {
                        ++garageViewIndex;
                        refreshGaragePage();
                    }
                    else if (
                        menuSelection >= 4U &&
                        menuSelection < garageFocusCount)
                    {
                        const std::size_t colorIndex =
                            menuSelection - 4U;
                        if (!garageColorAvailable(colorIndex))
                            continue;
                        for (std::size_t component = 0U;
                             component < 4U; ++component)
                        {
                            profileState.player.color[component] =
                                static_cast<float>(
                                    garageColorPixels[colorIndex]
                                                      [component]) /
                                255.0F;
                        }
                        saveRaceProfile();
                    }
                    continue;
                }
                if (menuStack.back() == MenuScreen::Workshop)
                {
                    if (workshopConfirmation !=
                        WorkshopConfirmation::None)
                    {
                        if (inputEvent.action ==
                                rrr3d::input::Action::TurnLeft ||
                            inputEvent.action ==
                                rrr3d::input::Action::MenuUp)
                        {
                            workshopConfirmationYesFocused = true;
                        }
                        else if (
                            inputEvent.action ==
                                rrr3d::input::Action::TurnRight ||
                            inputEvent.action ==
                                rrr3d::input::Action::MenuDown)
                        {
                            workshopConfirmationYesFocused = false;
                        }
                        else if (
                            !inputEvent.repeated &&
                            (inputEvent.action ==
                                 rrr3d::input::Action::MenuBack ||
                             inputEvent.action ==
                                 rrr3d::input::Action::Pause))
                        {
                            workshopConfirmation =
                                WorkshopConfirmation::None;
                            workshopPendingPurchase = nullptr;
                        }
                        else if (
                            !inputEvent.repeated &&
                            inputEvent.action ==
                                rrr3d::input::Action::MenuConfirm)
                        {
#ifdef RRR3D_AUDIO
                            playMainButtonClick();
#endif
                            const auto confirmation =
                                workshopConfirmation;
                            const auto* pending =
                                workshopPendingPurchase;
                            const bool accepted =
                                workshopConfirmationYesFocused;
                            workshopConfirmation =
                                WorkshopConfirmation::None;
                            workshopPendingPurchase = nullptr;
                            if (accepted)
                            {
                                if (confirmation ==
                                        WorkshopConfirmation::Buy &&
                                    pending != nullptr)
                                {
                                    buyWorkshopGood(*pending);
                                }
                                else if (
                                    confirmation ==
                                    WorkshopConfirmation::Sell)
                                {
                                    stopWorkshopDrag(true, true);
                                }
                            }
                        }
                        continue;
                    }
                    constexpr std::size_t workshopFocusCount = 23U;
                    if (inputEvent.action ==
                        rrr3d::input::Action::MenuUp)
                    {
                        menuSelection =
                            menuSelection == 0U
                                ? workshopFocusCount - 1U
                                : menuSelection - 1U;
                        refreshWorkshopPage();
                        continue;
                    }
                    if (inputEvent.action ==
                        rrr3d::input::Action::MenuDown)
                    {
                        menuSelection =
                            (menuSelection + 1U) %
                            workshopFocusCount;
                        refreshWorkshopPage();
                        continue;
                    }
                    if (inputEvent.action ==
                            rrr3d::input::Action::TurnLeft ||
                        inputEvent.action ==
                            rrr3d::input::Action::TurnRight)
                    {
                        if (!inputEvent.repeated)
                        {
                            const std::size_t rowCount =
                                (workshopGoods.size() + 2U) / 3U;
                            const std::size_t maximumScroll =
                                rowCount > 4U ? rowCount - 4U : 0U;
                            if (inputEvent.action ==
                                    rrr3d::input::Action::TurnLeft &&
                                workshopGoodScroll > 0U)
                            {
                                --workshopGoodScroll;
                                hideWorkshopWeaponDialog();
                            }
                            else if (
                                inputEvent.action ==
                                    rrr3d::input::Action::TurnRight &&
                                workshopGoodScroll < maximumScroll)
                            {
                                ++workshopGoodScroll;
                                hideWorkshopWeaponDialog();
                            }
                            refreshWorkshopPage();
                        }
                        continue;
                    }
                    if (inputEvent.repeated)
                        continue;
                    if (inputEvent.action ==
                            rrr3d::input::Action::MenuBack ||
                        inputEvent.action ==
                            rrr3d::input::Action::Pause)
                    {
#ifdef RRR3D_AUDIO
                        playMainButtonClick();
#endif
                        hideWorkshopWeaponDialog();
                        if (workshopDrag.active())
                            stopWorkshopDrag(false);
                        else
                            backMenu();
                        continue;
                    }
                    if (inputEvent.action ==
                        rrr3d::input::Action::MenuConfirm)
                    {
#ifdef RRR3D_AUDIO
                        playMainButtonClick();
#endif
                        activateWorkshopFocus(
                            inputEvent.source ==
                                rrr3d::input::Source::Mouse &&
                            workshopPointerSlotPlane);
                    }
                    continue;
                }
                if (menuStack.back() == MenuScreen::Achievements)
                {
                    if (achievementPurchaseDialogVisible)
                    {
                        if (inputEvent.action ==
                                rrr3d::input::Action::TurnLeft ||
                            inputEvent.action ==
                                rrr3d::input::Action::MenuUp)
                        {
                            achievementPurchaseYesFocused = true;
                        }
                        else if (
                            inputEvent.action ==
                                rrr3d::input::Action::TurnRight ||
                            inputEvent.action ==
                                rrr3d::input::Action::MenuDown)
                        {
                            achievementPurchaseYesFocused = false;
                        }
                        else if (
                            !inputEvent.repeated &&
                            (inputEvent.action ==
                                 rrr3d::input::Action::MenuBack ||
                             inputEvent.action ==
                                 rrr3d::input::Action::Pause))
                        {
                            achievementPurchaseDialogVisible = false;
                            achievementPendingPurchase =
                                originalAchievementNoTarget;
                        }
                        else if (
                            !inputEvent.repeated &&
                            inputEvent.action ==
                                rrr3d::input::Action::MenuConfirm)
                        {
#ifdef RRR3D_AUDIO
                            playMainButtonClick();
#endif
                            const auto pending =
                                achievementPendingPurchase;
                            const bool accepted =
                                achievementPurchaseYesFocused;
                            achievementPurchaseDialogVisible = false;
                            achievementPendingPurchase =
                                originalAchievementNoTarget;
                            if (accepted &&
                                pending <
                                    originalAchievementVisuals.size() &&
                                achievementState(pending) ==
                                    "asUnlocked")
                            {
                                const auto price =
                                    achievementPrice(pending);
                                if (profileState.achievementPoints <
                                    price)
                                {
                                    showInfoDialog(
                                        localized("svWarning"),
                                        localized(
                                            "svHintCantPoints"),
                                        localized("svOk"),
                                        menu::virtualWidth * 0.5F,
                                        menu::virtualHeight * 0.5F);
                                }
                                else
                                {
                                    profileState.achievementPoints -=
                                        price;
                                    profileState
                                        .achievementItems[
                                            std::string(
                                                originalAchievementVisuals
                                                    [pending]
                                                    .name)]
                                        .values["state"] = "asOpened";
                                    saveRaceProfile();
                                    refreshAchievementsPage();
                                    std::cout
                                        << "Original AchievmentFrame "
                                           "reward opened: "
                                        << originalAchievementVisuals
                                               [pending]
                                               .name
                                        << '\n';
                                }
                            }
                        }
                        continue;
                    }
                    if (inputEvent.action ==
                        rrr3d::input::Action::TurnLeft)
                    {
                        moveAchievementFocus(0U);
                        continue;
                    }
                    if (inputEvent.action ==
                        rrr3d::input::Action::TurnRight)
                    {
                        moveAchievementFocus(1U);
                        continue;
                    }
                    if (inputEvent.action ==
                        rrr3d::input::Action::MenuUp)
                    {
                        moveAchievementFocus(2U);
                        continue;
                    }
                    if (inputEvent.action ==
                        rrr3d::input::Action::MenuDown)
                    {
                        moveAchievementFocus(3U);
                        continue;
                    }
                    if (inputEvent.repeated)
                        continue;
                    if (inputEvent.action ==
                            rrr3d::input::Action::MenuBack ||
                        inputEvent.action ==
                            rrr3d::input::Action::Pause)
                    {
#ifdef RRR3D_AUDIO
                        playMainButtonClick();
#endif
                        backMenu();
                        continue;
                    }
                    if (inputEvent.action ==
                        rrr3d::input::Action::MenuConfirm)
                    {
#ifdef RRR3D_AUDIO
                        playMainButtonClick();
#endif
                        if (menuSelection ==
                            originalAchievementBack)
                        {
                            backMenu();
                        }
                        else if (
                            achievementState(menuSelection) ==
                            "asUnlocked")
                        {
                            achievementPendingPurchase =
                                menuSelection;
                            achievementPurchaseYesFocused = true;
                            achievementPurchaseDialogVisible = true;
                            showAcceptDialog(
                                localized("svBuyReward"),
                                localized("svYes"),
                                localized("svNo"),
                                menu::virtualWidth * 0.5F,
                                menu::virtualHeight * 0.5F);
                        }
                    }
                    continue;
                }
                if (menuStack.back() == MenuScreen::Planets)
                {
                    const auto planetCount = std::min(
                        originalGarage->planets.size(),
                        profileState.player.planets.size());
                    if (angarTravelDialogVisible)
                    {
                        if (inputEvent.action ==
                                rrr3d::input::Action::TurnLeft ||
                            inputEvent.action ==
                                rrr3d::input::Action::MenuUp)
                        {
                            angarTravelYesFocused = true;
                        }
                        else if (
                            inputEvent.action ==
                                rrr3d::input::Action::TurnRight ||
                            inputEvent.action ==
                                rrr3d::input::Action::MenuDown)
                        {
                            angarTravelYesFocused = false;
                        }
                        else if (
                            !inputEvent.repeated &&
                            (inputEvent.action ==
                                 rrr3d::input::Action::MenuBack ||
                             inputEvent.action ==
                                 rrr3d::input::Action::Pause))
                        {
                            angarTravelDialogVisible = false;
                        }
                        else if (
                            !inputEvent.repeated &&
                            inputEvent.action ==
                                rrr3d::input::Action::MenuConfirm)
                        {
#ifdef RRR3D_AUDIO
                            playMainButtonClick();
#endif
                            if (angarTravelYesFocused)
                                changeAngarPlanet(angarTravelTarget);
                            else
                                angarTravelDialogVisible = false;
                        }
                        continue;
                    }
                    if (inputEvent.action ==
                            rrr3d::input::Action::TurnLeft ||
                        inputEvent.action ==
                            rrr3d::input::Action::TurnRight)
                    {
                        if (!inputEvent.repeated && planetCount > 0U)
                        {
                            if (menuSelection >= planetCount)
                            {
                                menuSelection = 0U;
                            }
                            else if (
                                inputEvent.action ==
                                rrr3d::input::Action::TurnLeft)
                            {
                                menuSelection =
                                    menuSelection == 0U
                                        ? planetCount - 1U
                                        : menuSelection - 1U;
                            }
                            else
                            {
                                menuSelection =
                                    (menuSelection + 1U) %
                                    planetCount;
                            }
                            selectAngarPlanet(
                                static_cast<int>(menuSelection));
                        }
                        continue;
                    }
                    if (inputEvent.action ==
                            rrr3d::input::Action::MenuUp ||
                        inputEvent.action ==
                            rrr3d::input::Action::MenuDown)
                    {
                        menuSelection =
                            menuSelection < planetCount
                                ? planetCount
                                : 0U;
                        selectAngarPlanet(
                            menuSelection < planetCount
                                ? static_cast<int>(menuSelection)
                                : -1);
                        continue;
                    }
                    if (inputEvent.repeated)
                        continue;
                    const bool backRequested =
                        inputEvent.action ==
                            rrr3d::input::Action::MenuBack ||
                        inputEvent.action ==
                            rrr3d::input::Action::Pause ||
                        (inputEvent.action ==
                             rrr3d::input::Action::MenuConfirm &&
                         menuSelection >= planetCount);
                    if (backRequested)
                    {
#ifdef RRR3D_AUDIO
                        playMainButtonClick();
#endif
                        if (racePlanetChampion)
                        {
                            requestAngarTravel(
                                std::min<std::size_t>(
                                    profileState.player.currentPlanet,
                                    planetCount - 1U),
                                false);
                        }
                        else
                        {
                            backMenu();
                        }
                        continue;
                    }
                    if (inputEvent.action !=
                            rrr3d::input::Action::MenuConfirm ||
                        menuSelection >= planetCount)
                    {
                        continue;
                    }
                    const auto index = menuSelection;
#ifdef RRR3D_AUDIO
                    // Planet ViewPort3d uses ssButton5::clickDown.
                    playOriginalMenuSound(
                        rrr3d::audio::OriginalMenuSound::ShowPlanet);
#endif
                    const bool current =
                        index == profileState.player.currentPlanet;
                    const bool next =
                        racePlanetChampion &&
                        index ==
                            profileState.player.currentPlanet + 1U;
                    if (racePlanetChampion && (current || next))
                    {
                        requestAngarTravel(index);
                    }
                    else if (
                        !championshipMode &&
                        profileState.player.planets[index].state == 0U)
                    {
                        changeAngarPlanet(index);
                    }
                    else if (current)
                    {
                        backMenu();
                    }
                    else
                    {
                        const float panelCenterX =
                            menu::virtualWidth * 0.5F;
                        const float panelCenterY =
                            menu::virtualHeight -
                            static_cast<float>(
                                angarBottomPanelImage.height) *
                                0.5F -
                            20.0F;
                        const float senderX =
                            panelCenterX -
                            static_cast<float>(
                                angarBottomPanelImage.width) *
                                0.5F +
                            125.0F +
                            static_cast<float>(index) * 224.0F;
                        const float senderY =
                            panelCenterY -
                            static_cast<float>(
                                angarBottomPanelImage.height) *
                                0.5F +
                            90.0F;
                        showInfoDialog(
                            localized("svWarning"),
                            localized("svHintCantFlyPlanet"),
                            localized("svOk"), senderX,
                            senderY -
                                static_cast<float>(
                                    angarDoorSlotImage.height) *
                                    0.5F -
                                static_cast<float>(
                                    infoDialogFrameImage.height) *
                                    0.5F);
                        std::cout
                            << "Original AngarFrame warning: "
                            << localized("svHintCantFlyPlanet")
                            << '\n';
                    }
                    continue;
                }
                if (menuStack.back() == MenuScreen::Finish)
                {
                    if (!inputEvent.repeated &&
                        (inputEvent.action ==
                             rrr3d::input::Action::MenuConfirm ||
                         inputEvent.action ==
                             rrr3d::input::Action::MenuBack ||
                         inputEvent.action ==
                             rrr3d::input::Action::Pause))
                    {
                        // FinishMenu::OnHandleInput maps both gaAction and
                        // gaEscape to Menu::OnFinishClose.  Mouse left is
                        // translated to MenuConfirm by SdlInputManager.
                        closeFinishMenu();
                    }
                    continue;
                }
#endif
                if (menuStack.back() == MenuScreen::Credits)
                {
                    const bool closeRequested =
                        !inputEvent.repeated &&
                        (inputEvent.action ==
                             rrr3d::input::Action::MenuBack ||
                         inputEvent.action ==
                             rrr3d::input::Action::Pause ||
                         (inputEvent.action ==
                              rrr3d::input::Action::MenuConfirm &&
                          (inputEvent.source !=
                               rrr3d::input::Source::Mouse ||
                           pointerTargetsItem)));
                    if (closeRequested)
                        closeOriginalFinalMenu();
                    continue;
                }
                auto& page = activeMenuPage();
                if (inputEvent.action ==
                    rrr3d::input::Action::MenuUp)
                {
                    for (std::size_t attempts = 0U;
                         attempts < page.labels.size(); ++attempts)
                    {
                        menuSelection =
                            menuSelection == 0U
                                ? page.labels.size() - 1U
                                : menuSelection - 1U;
                        if (page.enabled[menuSelection])
                            break;
                    }
                    continue;
                }
                if (inputEvent.action ==
                    rrr3d::input::Action::MenuDown)
                {
                    for (std::size_t attempts = 0U;
                         attempts < page.labels.size(); ++attempts)
                    {
                        menuSelection =
                            (menuSelection + 1U) %
                            page.labels.size();
                        if (page.enabled[menuSelection])
                            break;
                    }
                    continue;
                }
#ifdef RRR3D_PHYSICS
                if (menuStack.back() == MenuScreen::RaceMenu &&
                    (inputEvent.action ==
                         rrr3d::input::Action::TurnLeft ||
                     inputEvent.action ==
                         rrr3d::input::Action::TurnRight))
                {
                    if (!inputEvent.repeated)
                    {
                        if (inputEvent.action ==
                            rrr3d::input::Action::TurnLeft)
                        {
                            menuSelection =
                                menuSelection == 0U
                                    ? raceMenuIcons.size() - 1U
                                    : menuSelection - 1U;
                        }
                        else
                        {
                            menuSelection =
                                (menuSelection + 1U) %
                                raceMenuIcons.size();
                        }
                    }
                    continue;
                }
                if (menuStack.back() == MenuScreen::Garage &&
                    (inputEvent.action ==
                         rrr3d::input::Action::TurnLeft ||
                     inputEvent.action ==
                         rrr3d::input::Action::TurnRight))
                {
                    if (!inputEvent.repeated &&
                        !originalGarage->cars.empty())
                    {
                        if (inputEvent.action ==
                            rrr3d::input::Action::TurnLeft)
                        {
                            garageCarIndex =
                                garageCarIndex == 0U
                                    ? originalGarage->cars.size() - 1U
                                    : garageCarIndex - 1U;
                        }
                        else
                        {
                            garageCarIndex =
                                (garageCarIndex + 1U) %
                                originalGarage->cars.size();
                        }
                        refreshGaragePage();
                    }
                    continue;
                }
                const bool adjustableOptions =
                    menuStack.back() == MenuScreen::GameOptions ||
                    menuStack.back() ==
                        MenuScreen::GraphicsOptions ||
                    menuStack.back() == MenuScreen::SoundOptions;
                if (adjustableOptions &&
                    (inputEvent.action ==
                         rrr3d::input::Action::TurnLeft ||
                     inputEvent.action ==
                         rrr3d::input::Action::TurnRight))
                {
                    if (!inputEvent.repeated)
                    {
                        adjustCurrentOption(
                            inputEvent.action ==
                                    rrr3d::input::Action::TurnLeft
                                ? -1
                                : 1);
                    }
                    continue;
                }
                if (menuStack.back() ==
                        MenuScreen::ControlsOptions &&
                    (inputEvent.action ==
                         rrr3d::input::Action::TurnLeft ||
                     inputEvent.action ==
                         rrr3d::input::Action::TurnRight))
                {
                    if (!inputEvent.repeated &&
                        menuSelection <
                            originalControlActions.size())
                    {
                        controlsUseGamepad =
                            inputEvent.action ==
                            rrr3d::input::Action::TurnRight;
                    }
                    continue;
                }
#endif
                if (inputEvent.repeated)
                    continue;
                if (inputEvent.action ==
                        rrr3d::input::Action::MenuBack ||
                    inputEvent.action ==
                        rrr3d::input::Action::Pause)
                {
#ifdef RRR3D_AUDIO
                    playMainButtonClick();
#endif
                    if (
#ifdef RRR3D_PHYSICS
                        isOriginalOptionsScreen(menuStack.back()))
                    {
                        cancelOriginalOptions();
                    }
                    else if (
#endif
                        menuStack.size() == 1U)
                        running = false;
                    else
                        backMenu();
                    continue;
                }
                if (inputEvent.action !=
                    rrr3d::input::Action::MenuConfirm)
                    continue;
                if (menuSelection >= page.enabled.size() ||
                    !page.enabled[menuSelection])
                {
                    continue;
                }
#ifdef RRR3D_AUDIO
                const bool optionValue =
#ifdef RRR3D_PHYSICS
                    (menuStack.back() == MenuScreen::GameOptions &&
                     menuSelection < 12U) ||
                    (menuStack.back() == MenuScreen::GraphicsOptions &&
                     menuSelection < 8U) ||
                    (menuStack.back() == MenuScreen::SoundOptions &&
                     menuSelection < 5U);
#else
                    false;
#endif
                // Value rows emit ssStepper::selectItem from
                // adjustCurrentOption. Other shared buttons use ssButton1.
                const bool clickStarted =
                    optionValue ? true : playMainButtonClick();
#if defined(RRR3D_GAMEPAD_INPUT)
                if (options->audioSmokeTest && clickStarted &&
                    menuStack.back() == MenuScreen::Main &&
                    menuSelection == 1U)
                {
                    integratedAudioInputObserved = true;
                }
#endif
#endif
                std::cout << "MainMenu2 selection: "
                          << page.labels[menuSelection] << " via "
                          << rrr3d::input::sourceName(inputEvent.source)
                          << '\n';
                switch (menuStack.back())
                {
                case MenuScreen::Main:
                    if (menuSelection == 0U)
                        pushMenu(MenuScreen::GameMode);
                    else if (menuSelection == 1U)
#ifdef RRR3D_NETWORK
                    {
                        if (initializeNetwork())
                            pushMenu(MenuScreen::Network);
                    }
#else
                        pushMenu(MenuScreen::Network);
#endif
                    else if (menuSelection == 2U)
                    {
#ifdef RRR3D_PHYSICS
                        beginOriginalOptions();
#else
                        pushMenu(MenuScreen::Options);
#endif
                    }
                    else if (menuSelection == 3U)
                        showOriginalFinalMenu();
                    else
                        running = false;
                    break;
                case MenuScreen::GameMode:
                    if (menuSelection == 0U)
                    {
                        championshipMode = true;
                        pushMenu(MenuScreen::Tournament);
                    }
                    else if (menuSelection == 1U)
                    {
                        championshipMode = false;
                        newTournamentProfile = false;
                        pushMenu(MenuScreen::Difficulty);
                    }
                    else
                        backMenu();
                    break;
                case MenuScreen::Tournament:
                    if (menuSelection == 0U)
                    {
#ifdef RRR3D_PHYSICS
                        showOriginalRaceMenu();
#endif
                    }
                    else if (menuSelection == 1U)
                    {
                        championshipMode = true;
                        newTournamentProfile = true;
                        pushMenu(MenuScreen::Difficulty);
                    }
                    else if (menuSelection == 2U)
                        pushMenu(MenuScreen::Profiles);
                    else
                        backMenu();
                    break;
                case MenuScreen::Difficulty:
                {
                    if (menuSelection >= 3U)
                    {
                        backMenu();
                        break;
                    }
#ifdef RRR3D_PHYSICS
                    const auto difficulty =
                        std::array<std::string, 3>{
                            "gdEasy", "gdNormal", "gdHard"}
                            [menuSelection];
                    const auto startSelectedMatch = [&, difficulty]() {
                        const bool needsGamerSelection =
                            !championshipMode || newTournamentProfile;
                        if (championshipMode &&
                            newTournamentProfile)
                        {
                            // Race::NewProfile(rmChampionship) creates the
                            // first absent profileN. It must not reset or
                            // overwrite the campaign profile selected in
                            // race.xml.
                            saveRaceProfile();
                            const auto created =
                                r3d::game::originalrace::
                                    beginOriginalChampionshipProfile(
                                        profileState, difficulty);
                            selectedTrack = 0U;
                            newTournamentProfile = false;
                            refreshProfilePage();
                            std::cout
                                << "Race::NewProfile championship: "
                                << created << '\n';
                        }
                        else if (!championshipMode)
                        {
                            // Race owns one separate SkProfile named
                            // "skirmish". Its SaveGame is empty and it is
                            // absent from the persistent championship list.
                            saveRaceProfile();
                            championshipPlayerBeforeSkirmish =
                                profileState.player;
                            profileState.player =
                                r3d::game::originalrace::
                                    makeOriginalSkirmishProfile(
                                        profileState, difficulty);
                            selectedTrack = 0U;
                            std::cout
                                << "Race::NewProfile skirmish: temporary "
                                   "skirmish\n";
                        }
                        else
                        {
                            profileState.player.difficulty = difficulty;
                        }
                        if (!reloadCurrentRace())
                        {
                            runtimeSmokeFailed = true;
                            running = false;
                            return;
                        }
#ifdef RRR3D_NETWORK
                        if (!startHostedNetworkMatch())
                        {
                            runtimeSmokeFailed = true;
                            running = false;
                            return;
                        }
#endif
                        saveRaceProfile();
                        if (needsGamerSelection)
                            showOriginalGamers();
                        else
                            showOriginalRaceMenu();
                    };
#ifdef RRR3D_VIDEO
                    if (championshipMode &&
                        newTournamentProfile &&
                        !profileState.config.disableVideo)
                    {
                        // DifficultyFrame::OnClick hides the menu, plays
                        // main/main_eng, and calls StartMatch only from
                        // cVideoStopped.
                        originalMovieStartMatch = startSelectedMatch;
                        const auto movie =
                            activeLanguage == "russian"
                                ? "Data/Video/Main.avi"
                                : "Data/Video/Main_eng.avi";
                        if (playOriginalMovie(
                                movie,
                                OriginalMovieCompletion::
                                    TournamentStart))
                        {
                            break;
                        }
                        originalMovieStartMatch = {};
                    }
#endif
                    startSelectedMatch();
#endif
                    break;
                }
                case MenuScreen::Profiles:
                    if (menuSelection + 1U >= page.labels.size())
                        backMenu();
                    else
                    {
#ifdef RRR3D_PHYSICS
                        // Persist the profile being left before replacing
                        // PlayerProfile.  Merely changing its name used to
                        // save the current car/progress into another file
                        // and never loaded the chosen Windows profile.
                        saveRaceProfile();
                        std::string profileError;
                        if (!profileStore.selectProfile(
                                profileState,
                                page.labels[menuSelection],
                                profileError))
                        {
                            std::cerr
                                << "Unable to load original profile: "
                                << profileError << '\n';
                            break;
                        }
                        selectedTrack =
                            r3d::game::originalrace::
                                resolveOriginalTournamentTrack(
                                    *originalRace,
                                    profileState.player);
                        if (!reloadCurrentRace())
                        {
                            runtimeSmokeFailed = true;
                            running = false;
                            break;
                        }
                        input.applyKeyboardBindings(
                            profileState.config.keyboardControls);
                        input.applyGamepadBindings(
                            profileState.config.gamepadControls);
                        saveRaceProfile();
#endif
                        backMenu();
                    }
                    break;
                case MenuScreen::Network:
                    if (menuSelection + 1U >= page.labels.size())
                        backMenu();
#ifdef RRR3D_NETWORK
                    else if (menuSelection == 0U)
                        pushMenu(MenuScreen::NetworkServerType);
                    else
                        pushMenu(MenuScreen::NetworkClientType);
#else
                    else
                        std::cout
                            << "Network mode requires the pending "
                               "non-Windows NetLib transport port\n";
#endif
                    break;
#ifdef RRR3D_NETWORK
                case MenuScreen::NetworkServerType:
                    if (menuSelection + 1U >= page.labels.size())
                    {
                        backMenu();
                    }
                    else
                    {
                        // ServerTypeFrame stores stLocal, then follows the
                        // ordinary GameMode flow. The listener is created by
                        // the later StartMatch boundary, not by this button.
                        networkHostRequested = true;
                        pushMenu(MenuScreen::GameMode);
                        std::cout
                            << "Original ServerTypeFrame: stLocal\n";
                    }
                    break;
                case MenuScreen::NetworkClientType:
                    if (menuSelection == 0U)
                    {
                        std::string error;
                        if (networkSession.beginLanSearch(error))
                        {
                            pushMenu(MenuScreen::NetworkBrowser);
                            renderedNetworkRevision =
                                std::numeric_limits<std::uint64_t>::max();
                            refreshNetworkRuntimePages();
                        }
                        else
                        {
                            std::cerr
                                << "Original NetGame::PingHosts failed: "
                                << error << '\n';
                        }
                    }
                    else if (menuSelection == 1U)
                    {
                        networkIpInput = "_";
                        refreshNetworkIpPage();
                        replaceNetworkAuxPage(
                            networkStatusPage,
                            {localized("svEnterIP")},
                            menu::smallFontHeight);
                        pushMenu(MenuScreen::NetworkIpAddress);
                        if (!SDL_StartTextInput(window))
                        {
                            std::cerr
                                << "SDL_StartTextInput failed: "
                                << SDL_GetError() << '\n';
                        }
                    }
                    else
                    {
                        backMenu();
                    }
                    break;
                case MenuScreen::NetworkBrowser:
                    if (menuSelection + 1U >= page.labels.size())
                    {
                        backMenu();
                    }
                    else if (
                        menuSelection <
                        networkSnapshot.discoveredHosts.size())
                    {
                        const auto endpoint =
                            networkSnapshot.discoveredHosts[menuSelection];
                        std::string error;
                        if (!networkSession.connect(endpoint, error))
                        {
                            std::cerr
                                << "Original NetGame::Connect failed: "
                                << error << '\n';
                        }
                        else
                        {
                            // MainMenu keeps its non-dismissable wait
                            // message until OnConnectedPlayer or a failure
                            // callback resolves the asynchronous connect.
                            showLoadingInfoDialog();
                        }
                        renderedNetworkRevision =
                            std::numeric_limits<std::uint64_t>::max();
                        refreshNetworkRuntimePages();
                    }
                    break;
                case MenuScreen::NetworkIpAddress:
                    if (menuSelection == 0U)
                    {
                        std::string address = networkIpInput;
                        if (!address.empty() && address.front() == '_')
                            address.erase(address.begin());
                        std::string error;
                        if (!networkSession.connect(
                                {address,
                                 r3d::game::originalnetwork::defaultPort},
                                error))
                        {
                            std::cerr
                                << "Original NetGame::Connect(IP) failed: "
                                << error << '\n';
                        }
                        else
                        {
                            showLoadingInfoDialog();
                        }
                        renderedNetworkRevision =
                            std::numeric_limits<std::uint64_t>::max();
                        refreshNetworkRuntimePages();
                    }
                    else
                    {
                        backMenu();
                    }
                    break;
#endif
                case MenuScreen::Options:
#ifdef RRR3D_PHYSICS
                    beginOriginalOptions();
#else
                    backMenu();
#endif
                    break;
                case MenuScreen::Credits:
                    closeOriginalFinalMenu();
                    break;
#ifdef RRR3D_PHYSICS
                case MenuScreen::Gamers:
                    // Handled by the literal GamersFrame navigation graph
                    // before shared list-page dispatch.
                    break;
                case MenuScreen::RaceMenu:
                    if (menuSelection == 0U)
                    {
                        activateRaceMenuStart();
                    }
                    else if (menuSelection == 1U)
                    {
                        workshopDrag = {};
                        workshopGoodScroll = 0U;
                        hideWorkshopWeaponDialog();
                        pushMenu(MenuScreen::Workshop);
                        refreshWorkshopPage();
                    }
                    else if (menuSelection == 2U)
                    {
                        rebuildGarageCarOrder();
                        refreshGaragePage();
                        pushMenu(MenuScreen::Garage);
                    }
                    else if (menuSelection == 3U)
                    {
                        pushMenu(MenuScreen::Planets);
                        const auto planetCount = std::min(
                            originalGarage->planets.size(),
                            profileState.player.planets.size());
                        if (racePlanetChampion &&
                            profileState.player.currentPlanet + 1U <
                                planetCount)
                        {
                            angarPlanetIndex = static_cast<int>(
                                profileState.player.currentPlanet + 1U);
                            menuSelection =
                                static_cast<std::size_t>(
                                    angarPlanetIndex);
                        }
                        else
                        {
                            angarPlanetIndex = -1;
                            menuSelection = planetCount;
                        }
                        angarPreviousPlanetIndex = -1;
                        angarDoorTime = -1.0F;
                        angarTravelDialogVisible = false;
                        refreshPlanetsPage();
                    }
                    else if (menuSelection == 4U)
                    {
                        achievementPurchaseDialogVisible = false;
                        achievementPendingPurchase =
                            originalAchievementNoTarget;
                        refreshAchievementsPage();
                        pushMenu(MenuScreen::Achievements);
                    }
                    else if (menuSelection == 5U)
                    {
                        beginOriginalOptions();
                    }
                    else
                    {
#ifdef RRR3D_NETWORK
                        if (networkMatchStarted)
                        {
                            exitNetworkMatch(true);
                            break;
                        }
#endif
                        saveRaceProfile();
                        if (!restoreChampionshipProfile())
                        {
                            runtimeSmokeFailed = true;
                            running = false;
                            break;
                        }
                        menuStack = {MenuScreen::Main};
                        menuSelection = 0;
                    }
                    break;
                case MenuScreen::Garage:
                    if (menuSelection == 0U)
                    {
                        garageCarIndex =
                            (garageCarIndex + 1U) %
                            originalGarage->cars.size();
                        refreshGaragePage();
                    }
                    else if (menuSelection == 3U)
                    {
                        const auto& car =
                            originalGarage->cars[garageCarIndex];
                        std::string garageError;
                        if (!r3d::game::originalrace::
                                selectOriginalGarageCar(
                                    *originalGarage, profileState,
                                    car, championshipMode,
                                    garageError))
                        {
                            std::cerr
                                << "Original GarageFrame: "
                                << garageError << '\n';
                        }
                        else
                        {
#ifdef RRR3D_NETWORK
                            if (networkClientMatchEntered)
                                networkLocalCarSelected = true;
#endif
                            saveRaceProfile();
                        }
                        refreshGaragePage();
                    }
                    else if (menuSelection + 1U >=
                             page.labels.size())
                    {
                        backMenu();
                    }
                    break;
                case MenuScreen::Workshop:
                    break;
                case MenuScreen::Planets:
                    break;
                case MenuScreen::Achievements:
                    break;
                case MenuScreen::GameOptions:
                    if (menuSelection == 12U)
                        cancelOriginalOptions();
                    else if (menuSelection == 13U)
                        applyOriginalOptions();
                    else
                        adjustCurrentOption(1);
                    break;
                case MenuScreen::GraphicsOptions:
                    if (menuSelection == 8U)
                        cancelOriginalOptions();
                    else if (menuSelection == 9U)
                        applyOriginalOptions();
                    else
                        adjustCurrentOption(1);
                    break;
                case MenuScreen::SoundOptions:
                    if (menuSelection == 5U)
                        cancelOriginalOptions();
                    else if (menuSelection == 6U)
                        applyOriginalOptions();
                    else
                        adjustCurrentOption(1);
                    break;
                case MenuScreen::ControlsOptions:
                {
                    if (menuSelection <
                        originalControlActions.size())
                    {
                        bindingCaptureAction =
                            originalControlActions[menuSelection];
                        bindingCaptureGamepad = controlsUseGamepad;
                        showAcceptDialog(
                            localized("svPressKey"),
                            localized("svDeleteKey"),
                            localized("svCancel"),
                            menu::virtualWidth * 0.5F,
                            menu::virtualHeight * 0.5F,
                            true, false, true);
                        std::cout
                            << "Original ControlsFrame: "
                            << localized("svPressKey") << " ("
                            << *bindingCaptureAction << ")\n";
                    }
                    else if (
                        menuSelection ==
                        originalControlActions.size())
                        cancelOriginalOptions();
                    else
                        applyOriginalOptions();
                    break;
                }
                case MenuScreen::Finish:
                    // Consumed by the source FinishMenu ControlEvent before
                    // entering the shared selectable-page switch.
                    break;
#endif
                }
            }
#endif
            if (event.type == SDL_EVENT_QUIT ||
                event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                running = false;
            }
            else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
            {
                pixelWidth = std::max(event.window.data1, 1);
                pixelHeight = std::max(event.window.data2, 1);
                device->resize(static_cast<std::uint32_t>(pixelWidth),
                               static_cast<std::uint32_t>(pixelHeight));
#ifdef RRR3D_VIDEO
                videoPlayer.resize();
#endif
#ifdef RRR3D_PHYSICS
                std::string resizeError;
                if (!raceRenderer.resize(
                        *device,
                        static_cast<std::uint32_t>(pixelWidth),
                        static_cast<std::uint32_t>(pixelHeight),
                        resizeError) ||
                    !garageRenderer.resize(
                        *device,
                        static_cast<std::uint32_t>(pixelWidth),
                        static_cast<std::uint32_t>(pixelHeight),
                        resizeError) ||
                    !angarRenderer.resize(
                        *device,
                        static_cast<std::uint32_t>(pixelWidth),
                        static_cast<std::uint32_t>(pixelHeight),
                        resizeError))
                {
                    std::cerr
                        << "Unable to resize M9.3 render targets: "
                        << resizeError << '\n';
                    running = false;
                }
#endif
            }
        }

#ifdef RRR3D_VIDEO
        if (originalMovieActive)
        {
            std::string videoError;
            const auto state = videoPlayer.update(videoError);
            videoFrameObserved =
                videoFrameObserved || videoPlayer.readyForDisplay();
            videoAudioObserved =
                videoAudioObserved || videoPlayer.hasAudioTrack();
            if (options->videoSmokeTest && videoFrameObserved &&
                !videoSmokeSeeked)
            {
                const double duration =
                    videoPlayer.durationSeconds();
                if (duration > 1.0)
                {
                    videoPlayer.seek(duration - 0.5);
                    videoSmokeSeeked = true;
                }
            }
            if (state == rrr3d::video::PlaybackState::Completed)
            {
                finishOriginalMovie();
            }
            else if (state ==
                     rrr3d::video::PlaybackState::Failed)
            {
                std::cerr
                    << "Original movie runtime failed: "
                    << videoError << '\n';
                runtimeSmokeFailed = true;
                finishOriginalMovie();
            }
            else if (options->videoSmokeTest &&
                     SDL_GetTicks() >= videoSmokeDeadline)
            {
                std::cerr
                    << "Source movie smoke timed out after 15 seconds\n";
                runtimeSmokeFailed = true;
                finishOriginalMovie();
            }
        }
#endif

        const std::uint64_t currentFrameTicks = SDL_GetTicksNS();
        float frameSeconds = std::clamp(
            static_cast<float>(currentFrameTicks - previousFrameTicks) /
                1000000000.0F,
            0.0F, 0.1F);
        if (options->startupSmokeTest)
        {
            // Exercise the complete twelve-second source timeline without
            // turning the regression into a wall-clock delay.
            frameSeconds = 0.25F;
        }
        else if (options->finalMenuSmokeTest)
        {
#ifdef RRR3D_AUDIO
            frameSeconds =
                menuStack.back() == MenuScreen::Credits &&
                        !finalMusic.currentVoiceActive()
                    ? 0.0F
                    : 0.4F;
#else
            frameSeconds = 0.4F;
#endif
        }
#ifdef RRR3D_PHYSICS
        else if (options->raceRenderSmokeTest ||
            options->finishMenuSmokeTest)
            frameSeconds = 1.0F / 60.0F;
#endif
#ifdef RRR3D_AUDIO
        else if (options->audioSmokeTest)
            frameSeconds = 1.0F / 60.0F;
#endif
#ifdef RRR3D_VIDEO
        if (originalMovieActive)
            frameSeconds = 0.0F;
#endif
        previousFrameTicks = currentFrameTicks;
#ifdef RRR3D_PHYSICS
        userChat.update(frameSeconds);
#endif
#ifdef RRR3D_AUDIO
        // Windows GameMode::Commentator::OnProgress is active in every menu,
        // not just while Race is being simulated.  Keeping one global tick
        // lets multi-part and queued FinishMenu utterances reach their next
        // stream after the race world has been left.
        commentator.progress(frameSeconds, audioError);
#endif
        if (sourceStartupActive)
        {
            sourceStartupSeconds = std::min(
                sourceStartupSeconds + frameSeconds, 12.25F);
        }
        if (menuStack.back() == MenuScreen::Credits)
        {
            finalMenuSeconds += frameSeconds;
            if (finalMenuSeconds >= 107.0F)
            {
                finalAutoCloseObserved = true;
                closeOriginalFinalMenu();
            }
        }
#ifdef RRR3D_PHYSICS
        if (inRace)
        {
#ifdef RRR3D_NETWORK
            if (networkMatchStarted && !networkHostRequested &&
                networkRaceStarted &&
                !networkSnapshot.models.raceActive &&
                !networkSnapshot.models.results.empty() &&
                !networkRaceExitApplied)
            {
                std::vector<r3d::game::originalrace::
                                ReplicatedRaceResult>
                    results;
                results.reserve(
                    networkSnapshot.models.results.size());
                for (const auto& source :
                     networkSnapshot.models.results)
                {
                    const auto model = std::find(
                        networkRaceModelOrder.begin(),
                        networkRaceModelOrder.end(),
                        source.playerModelId);
                    if (model == networkRaceModelOrder.end())
                        continue;
                    results.push_back(
                        {static_cast<std::size_t>(std::distance(
                             networkRaceModelOrder.begin(), model)),
                         source.money, source.pickedMoney,
                         source.place, source.points});
                }
                raceSession.synchronizeNetworkFinishResults(results);
                networkRaceExitApplied = true;
                networkRaceStarted = false;
            }
#endif
            // Windows ControlManager::GetGameActionState is polled every
            // update.  Keeping only KEY_DOWN/KEY_UP events here lost an
            // already-held accelerator across menu/race, countdown, pause,
            // and focus transitions.
            raceInput.throttle = input.heldValue(
                rrr3d::input::Action::Accelerate);
            raceInput.reverse = input.heldValue(
                rrr3d::input::Action::Brake);
            raceInput.brake = 0.0F;
            raceInput.steering =
                input.heldValue(rrr3d::input::Action::TurnLeft) -
                input.heldValue(rrr3d::input::Action::TurnRight);
            if (options->raceRenderSmokeTest)
            {
                raceInput.steering = renderedFrames >= 90 &&
                                             renderedFrames < 180
                                         ? 0.35F
                                         : 0.0F;
            }
            r3d::game::originalrace::RaceControl control;
            control.driving = raceInput;
            control.useWeapon = raceUseWeaponRequested;
            control.useAllWeapons = raceUseAllWeaponsRequested;
            control.useMine = raceUseMine;
            control.useHyper = raceUseHyper;
            control.changeWeapon = raceChangeWeaponRequested;
            control.weaponChange = raceWeaponChangeDirection;
            control.fireWeaponSlot = raceFireWeaponSlotRequested;
            control.reset = raceResetRequested;
#ifdef RRR3D_NETWORK
            if (networkMatchStarted)
            {
                if (networkHostRequested &&
                    networkSnapshot.models.raceActive)
                {
                    const bool allHumansWaiting =
                        !networkSnapshot.models.players.empty() &&
                        std::none_of(
                            networkSnapshot.models.players.begin(),
                            networkSnapshot.models.players.end(),
                            [](const auto& player) {
                                return player.playerId == 0U &&
                                       !player.raceGoWait;
                            });
                    if (networkHostRaceGoSeconds < 0.0F &&
                        allHumansWaiting)
                    {
                        networkHostRaceGoSeconds = 0.0F;
                    }
                    if (networkHostRaceGoSeconds >= 0.0F &&
                        networkAppliedRaceGoStage < 4)
                    {
                        networkHostRaceGoSeconds += frameSeconds;
                        const auto stage = std::clamp(
                            static_cast<std::int32_t>(
                                std::floor(networkHostRaceGoSeconds)),
                            std::int32_t{0}, std::int32_t{4});
                        if (stage > networkAppliedRaceGoStage)
                        {
                            std::string error;
                            if (!networkSession.setRaceGoStage(stage, error))
                            {
                                std::cerr
                                    << "Original NetRace::OnRaceGo failed: "
                                    << error << '\n';
                                runtimeSmokeFailed = true;
                                running = false;
                            }
                            else
                            {
                                networkAppliedRaceGoStage = stage;
                                raceSession.synchronizeNetworkCountdown(
                                    stage);
                                refreshNetworkRuntimePages();
                            }
                        }
                    }
                }
                else if (networkSnapshot.models.raceGoStage >= 0 &&
                         networkSnapshot.models.raceGoStage !=
                             networkAppliedRaceGoStage)
                {
                    networkAppliedRaceGoStage =
                        networkSnapshot.models.raceGoStage;
                    raceSession.synchronizeNetworkCountdown(
                        networkAppliedRaceGoStage);
                }
                for (std::size_t index = 0U;
                     index < networkRaceModelOrder.size() &&
                     index < raceVehicles.size(); ++index)
                {
                    const auto player = std::find_if(
                        networkSnapshot.models.players.begin(),
                        networkSnapshot.models.players.end(),
                        [&](const auto& candidate) {
                            return candidate.modelId ==
                                   networkRaceModelOrder[index];
                        });
                    if (player == networkSnapshot.models.players.end() ||
                        player->owner)
                    {
                        continue;
                    }
                    auto& appliedRevision =
                        networkAppliedVehicleRevisions[player->modelId];
                    if (player->vehicle.receivedRevision == 0U ||
                        player->vehicle.receivedRevision == appliedRevision)
                    {
                        continue;
                    }
                    physicsWorld->synchronizeNetworkVehicle(
                        index,
                        {player->vehicle.position[0],
                         player->vehicle.position[1],
                         player->vehicle.position[2]},
                        {player->vehicle.rotation[0],
                         player->vehicle.rotation[1],
                         player->vehicle.rotation[2],
                         player->vehicle.rotation[3]},
                        {player->vehicle.linearMomentum[0],
                         player->vehicle.linearMomentum[1],
                         player->vehicle.linearMomentum[2]},
                        {player->vehicle.angularMomentum[0],
                         player->vehicle.angularMomentum[1],
                         player->vehicle.angularMomentum[2]});
                    raceVehicles[index] =
                        physicsWorld->vehicle(index);
                    appliedRevision = player->vehicle.receivedRevision;
                }
                for (const auto& event :
                     networkSnapshot.models.events)
                {
                    if (event.sequence <=
                        networkLastShotEventSequence)
                        continue;
                    networkLastShotEventSequence = std::max(
                        networkLastShotEventSequence,
                        event.sequence);
                    if (event.kind !=
                        r3d::game::originalnetwork::
                            NetworkEventKind::Shot)
                        continue;
                    const auto shooterModel = std::find(
                        networkRaceModelOrder.begin(),
                        networkRaceModelOrder.end(),
                        event.playerModelId);
                    if (shooterModel ==
                        networkRaceModelOrder.end())
                        continue;
                    r3d::game::originalrace::ReplicatedShot shot;
                    shot.racer = static_cast<std::size_t>(
                        std::distance(
                            networkRaceModelOrder.begin(),
                            shooterModel));
                    shot.target =
                        raceSession.racerForMapObjectId(event.target);
                    shot.slotMask = event.slotMask;
                    shot.projectileId =
                        static_cast<std::uint32_t>(
                            std::max(event.intValue, 0));
                    shot.coordinates.reserve(
                        event.coordinates.size());
                    for (const auto& coordinate : event.coordinates)
                    {
                        shot.coordinates.push_back(
                            {coordinate[0], coordinate[1],
                             coordinate[2]});
                    }
                    raceSession.queueNetworkShot(std::move(shot));
                }
                for (const auto& event :
                     networkSnapshot.models.events)
                {
                    if (event.sequence <=
                        networkLastBonusEventSequence)
                        continue;
                    networkLastBonusEventSequence = std::max(
                        networkLastBonusEventSequence,
                        event.sequence);
                    if (event.kind !=
                            r3d::game::originalnetwork::
                                NetworkEventKind::Bonus ||
                        event.target == 0U)
                        continue;
                    const auto playerModel = std::find(
                        networkRaceModelOrder.begin(),
                        networkRaceModelOrder.end(),
                        event.playerModelId);
                    if (playerModel == networkRaceModelOrder.end())
                        continue;
                    using BonusKind =
                        r3d::game::originalrace::BonusKind;
                    BonusKind kind = BonusKind::Unknown;
                    switch (event.intValue)
                    {
                    case 0:
                        kind = BonusKind::Money;
                        break;
                    case 1:
                        kind = BonusKind::Ammunition;
                        break;
                    case 2:
                        kind = BonusKind::Medpack;
                        break;
                    case 3:
                        kind = BonusKind::Shield;
                        break;
                    default:
                        break;
                    }
                    const auto bonus =
                        raceSession.bonusForMapObjectId(event.target);
                    if (bonus == r3d::game::originalrace::
                                     RacerRuntime::invalidWeapon)
                        continue;
                    raceSession.queueNetworkBonus(
                        {static_cast<std::size_t>(std::distance(
                             networkRaceModelOrder.begin(), playerModel)),
                         bonus, kind, event.value});
                }
                for (const auto& event :
                     networkSnapshot.models.events)
                {
                    if (event.sequence <=
                        networkLastMineEventSequence)
                        continue;
                    networkLastMineEventSequence = std::max(
                        networkLastMineEventSequence,
                        event.sequence);
                    if (event.kind !=
                            r3d::game::originalnetwork::
                                NetworkEventKind::MineContact ||
                        event.coordinates.empty())
                        continue;
                    const auto targetModel = std::find(
                        networkRaceModelOrder.begin(),
                        networkRaceModelOrder.end(),
                        event.playerModelId);
                    if (targetModel == networkRaceModelOrder.end())
                        continue;
                    r3d::game::originalrace::ReplicatedMineContact
                        contact;
                    contact.racer = static_cast<std::size_t>(
                        std::distance(networkRaceModelOrder.begin(),
                                      targetModel));
                    contact.point = {
                        event.coordinates.front()[0],
                        event.coordinates.front()[1],
                        event.coordinates.front()[2]};
                    contact.mapProjectile = !event.flag;
                    if (contact.mapProjectile)
                    {
                        contact.projectileId = event.target;
                    }
                    else
                    {
                        const auto ownerModel = std::find(
                            networkRaceModelOrder.begin(),
                            networkRaceModelOrder.end(),
                            event.target);
                        if (ownerModel == networkRaceModelOrder.end() ||
                            event.intValue <= 0)
                            continue;
                        contact.projectileOwner =
                            static_cast<std::size_t>(std::distance(
                                networkRaceModelOrder.begin(),
                                ownerModel));
                        contact.projectileId =
                            static_cast<std::uint32_t>(event.intValue);
                    }
                    raceSession.queueNetworkMineContact(
                        std::move(contact));
                }
            }
#endif
            raceSession.update(frameSeconds, raceVehicles, control);
#ifdef RRR3D_NETWORK
            if (networkMatchStarted)
            {
                const auto racerForModel =
                    [&](std::uint32_t modelId) {
                        const auto model = std::find(
                            networkRaceModelOrder.begin(),
                            networkRaceModelOrder.end(), modelId);
                        return model == networkRaceModelOrder.end()
                                   ? r3d::game::originalrace::
                                         RacerRuntime::invalidWeapon
                                   : static_cast<std::size_t>(
                                         std::distance(
                                             networkRaceModelOrder.begin(),
                                             model));
                    };
                for (const auto& event :
                     networkSnapshot.models.events)
                {
                    if (event.sequence <=
                        networkLastGameplayEventSequence)
                        continue;
                    networkLastGameplayEventSequence =
                        std::max(
                            networkLastGameplayEventSequence,
                            event.sequence);
                    if (event.kind !=
                            r3d::game::originalnetwork::
                                NetworkEventKind::PlayerDamage &&
                        event.kind !=
                            r3d::game::originalnetwork::
                                NetworkEventKind::MapObjectDamage)
                        continue;
                    const auto attacker =
                        racerForModel(event.playerModelId);
                    const auto damageType =
                        static_cast<r3d::game::originalrace::
                                        DamageType>(
                            std::clamp(event.intValue, 0, 4));
                    if (event.kind ==
                        r3d::game::originalnetwork::
                            NetworkEventKind::PlayerDamage)
                    {
                        const auto target = racerForModel(event.target);
                        if (target >= raceVehicles.size() ||
                            target >= raceSession.racers().size())
                            continue;
                        const auto result =
                            raceSession.applyNetworkPlayerDamage(
                                target, attacker,
                                raceVehicles[target].body.position,
                                event.value, damageType,
                                raceVehicles[target],
                                !networkHostRequested,
                                event.targetLife, event.flag);
                        if (networkHostRequested)
                        {
                            std::string error;
                            if (!networkSession.sendPlayerDamage(
                                    event.playerModelId, event.target,
                                    event.value, event.intValue,
                                    result.life, result.death, error))
                            {
                                std::cerr
                                    << "Original NetRace::Damage1 host "
                                       "response failed: "
                                    << error << '\n';
                                runtimeSmokeFailed = true;
                                running = false;
                            }
                        }
                        continue;
                    }
                    if (raceSession.decorationForMapObjectId(
                            event.target) ==
                        r3d::game::originalrace::
                            RacerRuntime::invalidWeapon)
                        continue;
                    const auto result =
                        raceSession.applyNetworkMapObjectDamage(
                            event.target, attacker, event.value,
                            damageType, !networkHostRequested,
                            event.targetLife, event.flag);
                    if (networkHostRequested)
                    {
                        std::string error;
                        if (!networkSession.sendMapObjectDamage(
                                event.playerModelId, event.target,
                                event.value, event.intValue,
                                result.life, result.death, error))
                        {
                            std::cerr
                                << "Original NetRace::Damage2 host "
                                   "response failed: "
                                << error << '\n';
                            runtimeSmokeFailed = true;
                            running = false;
                        }
                    }
                }
            }
            if (networkMatchStarted &&
                !raceSession.racers().empty() &&
                raceSession.racers().front().finished &&
                !networkLocalFinishPublished)
            {
                std::string error;
                if (!networkSession.setLocalPlayerFinished(true, error))
                {
                    std::cerr
                        << "Original NetPlayer::RaceFinish failed: "
                        << error << '\n';
                    runtimeSmokeFailed = true;
                    running = false;
                }
                else
                {
                    networkLocalFinishPublished = true;
                    refreshNetworkRuntimePages();
                }
            }
            if (networkMatchStarted && networkHostRequested &&
                !networkHostFinishTimerStarted &&
                !raceSession.racers().empty() &&
                raceSession.racers().front().finished)
            {
                const bool allHumansFinished =
                    !networkSnapshot.models.players.empty() &&
                    std::none_of(
                        networkSnapshot.models.players.begin(),
                        networkSnapshot.models.players.end(),
                        [](const auto& player) {
                            return player.playerId == 0U &&
                                   !player.raceFinish;
                        });
                if (allHumansFinished)
                {
                    raceSession.startNetworkFinishTimer();
                    networkHostFinishTimerStarted = true;
                }
            }
#endif
            if (options->raceRenderSmokeTest &&
                !raceSession.racers().empty())
                minimumRacePlayerLife = std::min(
                    minimumRacePlayerLife,
                    raceSession.racers().front().life);
            raceUseWeaponRequested = false;
            raceUseAllWeaponsRequested = false;
            raceChangeWeaponRequested = false;
            raceWeaponChangeDirection = 1;
            raceFireWeaponSlotRequested = -1;
            raceResetRequested = false;
            for (const auto& event : raceSession.events())
            {
#ifdef RRR3D_NETWORK
                if (networkMatchStarted &&
                    (event.kind ==
                         r3d::game::originalrace::RaceEventKind::
                             WeaponFired ||
                     event.kind ==
                         r3d::game::originalrace::RaceEventKind::
                             MinePlaced ||
                     event.kind ==
                         r3d::game::originalrace::RaceEventKind::
                             HyperActivated) &&
                    !event.networkReplicated &&
                    event.networkSlotMask != 0U &&
                    event.racer < networkRaceModelOrder.size())
                {
                    const auto owner = std::find_if(
                        networkSnapshot.models.players.begin(),
                        networkSnapshot.models.players.end(),
                        [&](const auto& candidate) {
                            return candidate.modelId ==
                                       networkRaceModelOrder[event.racer] &&
                                   candidate.owner;
                        });
                    if (owner != networkSnapshot.models.players.end())
                    {
                        const std::uint32_t targetObjectId =
                            event.target < originalRace->racers.size()
                                ? originalRace->racers[event.target]
                                      .mapObjectId
                                : 0U;
                        std::vector<std::array<float, 3>> coordinates;
                        coordinates.reserve(
                            event.networkCoordinates.size());
                        for (const auto& coordinate :
                             event.networkCoordinates)
                        {
                            coordinates.push_back(
                                {coordinate.x, coordinate.y,
                                 coordinate.z});
                        }
                        std::string error;
                        if (!networkSession.sendOwnedPlayerShot(
                                networkRaceModelOrder[event.racer],
                                targetObjectId,
                                event.networkSlotMask,
                                event.networkProjectileId,
                                coordinates, error))
                        {
                            std::cerr
                                << "Original NetPlayer::Shot failed: "
                                << error << '\n';
                            runtimeSmokeFailed = true;
                            running = false;
                        }
                    }
                }
                if (networkMatchStarted &&
                    event.kind ==
                        r3d::game::originalrace::RaceEventKind::Bonus &&
                    !event.networkReplicated &&
                    event.racer < networkRaceModelOrder.size() &&
                    event.target < originalRace->bonuses.size())
                {
                    const auto owner = std::find_if(
                        networkSnapshot.models.players.begin(),
                        networkSnapshot.models.players.end(),
                        [&](const auto& candidate) {
                            return candidate.modelId ==
                                       networkRaceModelOrder[event.racer] &&
                                   candidate.owner;
                        });
                    if (owner != networkSnapshot.models.players.end())
                    {
                        std::int32_t bonusType = -1;
                        switch (originalRace->bonuses[event.target].kind)
                        {
                        case r3d::game::originalrace::BonusKind::Money:
                            bonusType = 0;
                            break;
                        case r3d::game::originalrace::BonusKind::Ammunition:
                            bonusType = 1;
                            break;
                        case r3d::game::originalrace::BonusKind::Medpack:
                            bonusType = 2;
                            break;
                        case r3d::game::originalrace::BonusKind::Shield:
                            bonusType = 3;
                            break;
                        default:
                            break;
                        }
                        if (bonusType >= 0)
                        {
                            std::string error;
                            if (!networkSession.sendOwnedPlayerBonus(
                                    networkRaceModelOrder[event.racer],
                                    originalRace->bonuses[event.target]
                                        .mapObjectId,
                                    bonusType, event.value, error))
                            {
                                std::cerr
                                    << "Original NetPlayer::TakeBonus "
                                       "failed: "
                                    << error << '\n';
                                runtimeSmokeFailed = true;
                                running = false;
                            }
                        }
                    }
                }
                if (networkMatchStarted &&
                    event.kind ==
                        r3d::game::originalrace::RaceEventKind::
                            MineContact &&
                    !event.networkReplicated &&
                    event.racer < networkRaceModelOrder.size())
                {
                    const auto owner = std::find_if(
                        networkSnapshot.models.players.begin(),
                        networkSnapshot.models.players.end(),
                        [&](const auto& candidate) {
                            return candidate.modelId ==
                                       networkRaceModelOrder[event.racer] &&
                                   candidate.owner;
                        });
                    if (owner != networkSnapshot.models.players.end())
                    {
                        const std::array<float, 3> point{
                            event.position.x, event.position.y,
                            event.position.z};
                        std::string error;
                        const bool sent = event.networkMapObject
                            ? networkSession
                                  .sendOwnedPlayerMineContactMap(
                                      networkRaceModelOrder[event.racer],
                                      event.networkProjectileId, point,
                                      error)
                            : (event.target <
                                       networkRaceModelOrder.size() &&
                               networkSession
                                   .sendOwnedPlayerMineContactPlayer(
                                       networkRaceModelOrder[event.racer],
                                       networkRaceModelOrder[event.target],
                                       event.networkProjectileId, point,
                                       error));
                        if (!sent)
                        {
                            std::cerr
                                << "Original NetPlayer::MineContact "
                                   "failed: "
                                << error << '\n';
                            runtimeSmokeFailed = true;
                            running = false;
                        }
                    }
                }
                if (networkMatchStarted &&
                    event.kind ==
                        r3d::game::originalrace::RaceEventKind::Damage &&
                    !event.networkReplicated &&
                    event.racer < networkRaceModelOrder.size())
                {
                    bool publishDamage = networkHostRequested;
                    if (!publishDamage)
                    {
                        const std::size_t ownerCandidate =
                            event.target < networkRaceModelOrder.size()
                                ? event.target
                                : event.racer;
                        const auto owner = std::find_if(
                            networkSnapshot.models.players.begin(),
                            networkSnapshot.models.players.end(),
                            [&](const auto& candidate) {
                                return candidate.modelId ==
                                           networkRaceModelOrder[
                                               ownerCandidate] &&
                                       candidate.owner;
                            });
                        publishDamage =
                            owner != networkSnapshot.models.players.end();
                    }
                    if (publishDamage)
                    {
                        const std::uint32_t senderModelId =
                            event.target < networkRaceModelOrder.size()
                                ? networkRaceModelOrder[event.target]
                                : std::numeric_limits<
                                      std::uint32_t>::max();
                        std::string error;
                        if (!networkSession.sendPlayerDamage(
                                senderModelId,
                                networkRaceModelOrder[event.racer],
                                event.value,
                                static_cast<std::int32_t>(
                                    event.damageType),
                                event.authoritativeLife,
                                event.authoritativeDeath, error))
                        {
                            std::cerr
                                << "Original NetRace::Damage1 failed: "
                                << error << '\n';
                            runtimeSmokeFailed = true;
                            running = false;
                        }
                    }
                }
                if (networkMatchStarted &&
                    event.kind ==
                        r3d::game::originalrace::RaceEventKind::
                            MapObjectDamage &&
                    !event.networkReplicated &&
                    event.racer < networkRaceModelOrder.size() &&
                    event.target <
                        originalRace->decorationInstances.size())
                {
                    bool publishDamage = networkHostRequested;
                    if (!publishDamage)
                    {
                        const auto owner = std::find_if(
                            networkSnapshot.models.players.begin(),
                            networkSnapshot.models.players.end(),
                            [&](const auto& candidate) {
                                return candidate.modelId ==
                                           networkRaceModelOrder[
                                               event.racer] &&
                                       candidate.owner;
                            });
                        publishDamage =
                            owner != networkSnapshot.models.players.end();
                    }
                    if (publishDamage)
                    {
                        std::string error;
                        if (!networkSession.sendMapObjectDamage(
                                networkRaceModelOrder[event.racer],
                                originalRace->decorationInstances[
                                    event.target].mapObjectId,
                                event.value,
                                static_cast<std::int32_t>(
                                    event.damageType),
                                event.authoritativeLife,
                                event.authoritativeDeath, error))
                        {
                            std::cerr
                                << "Original NetRace::Damage2 failed: "
                                << error << '\n';
                            runtimeSmokeFailed = true;
                            running = false;
                        }
                    }
                }
#endif
                if (options->raceRenderSmokeTest &&
                    event.kind ==
                        r3d::game::originalrace::RaceEventKind::Kill &&
                    event.target == 0U)
                    racePlayerDestroyedObserved = true;
                if (event.kind ==
                        r3d::game::originalrace::RaceEventKind::Kill &&
                    event.target < originalRace->racers.size() &&
                    event.target < raceVehicles.size())
                {
                    const auto& sourceRacer =
                        originalRace->racers[event.target];
                    const auto& vehicle =
                        sourceRacer.hasConfiguredVehicle
                            ? sourceRacer.configuredVehicle
                            : originalRace->vehicles.at(
                                  sourceRacer.vehicle);
                    for (std::size_t effectIndex = 0;
                         effectIndex < vehicle.deathEffects.size();
                         ++effectIndex)
                    {
                        const auto& effect =
                            vehicle.deathEffects[effectIndex];
                        if (!effect.visual.dynamicBody)
                            continue;
                        const auto runtimeEffect = std::find_if(
                            raceSession.effects().begin(),
                            raceSession.effects().end(),
                            [&](const auto& value) {
                                return value.kind ==
                                           r3d::game::originalrace::
                                               RaceEventKind::
                                                   VehicleDestroyed &&
                                       value.racer == event.target &&
                                       value.vehicleEffect == effectIndex;
                            });
                        r3d::physics::DebrisDescription debris;
                        debris.transform =
                            runtimeEffect != raceSession.effects().end()
                                ? runtimeEffect->transform
                                : raceVehicles[event.target].body;
                        debris.shapePosition =
                            effect.visual.bodyShapePosition;
                        debris.shapeRotation =
                            effect.visual.bodyShapeRotation;
                        debris.halfExtents =
                            effect.visual.bodyHalfExtents;
                        debris.localImpulse = effect.impulse;
                        debris.mass = effect.visual.bodyMass;
                        debris.lifetime =
                            effect.visual.maximumTimeLife;
                        const auto debrisIndex =
                            physicsWorld->addDebris(debris);
                        if (debrisIndex ==
                            std::numeric_limits<std::size_t>::max())
                            continue;
                        vehicleDebrisBindings.push_back(
                            {event.target, effectIndex, debrisIndex});
                    }
                    physicsWorld->setVehicleEnabled(
                        event.target, false);
                }
                if (event.kind !=
                        r3d::game::originalrace::RaceEventKind::
                            DecorationDestroyed ||
                    event.target >=
                        originalRace->decorationInstances.size())
                    continue;
                // The source removed the destroyed ctDecoration actor from
                // PhysX before spawning its destruction-list actors.  The
                // Jolt port must do the same for this individual instance;
                // leaving it in a map-wide merged mesh creates invisible
                // walls after the board/sign has disappeared.
                physicsWorld->setDecorationEnabled(event.target, false);
                const auto destruction =
                    r3d::game::originalrace::makeDecorationDestruction(
                        *originalRace, *resources, event.target);
                for (const auto& piece : destruction)
                {
                    const auto debrisIndex =
                        physicsWorld->addDebris(piece.physics);
                    if (debrisIndex ==
                        std::numeric_limits<std::size_t>::max())
                        continue;
                    decorationDebrisBindings.push_back(
                        {event.target, piece.piece, debrisIndex});
                }
            }
#ifdef RRR3D_AUDIO
            if (!raceVehicles.empty())
            {
                const auto listener =
                    raceVehicles.front().body.position;
                auto playSpatial =
                    [&](r3d::audio::SoundHandle sound,
                        const r3d::physics::Vec3& source) {
                    if (sound == r3d::audio::invalidSound)
                        return;
                    const float dx = source.x - listener.x;
                    const float dy = source.y - listener.y;
                    const float dz = source.z - listener.z;
                    const float distance =
                        std::sqrt(dx * dx + dy * dy + dz * dz);
                    // snd::Engine defaults to m3dFlat. A newly created
                    // Source3d starts only inside CurveDistanceScaler and
                    // has neither stereo pan nor Doppler in this mode.
                    const auto spatial =
                        rrr3d::audio::originalSource3dFlatMix(
                            distance, false);
                    if (!spatial.proxyPlaying)
                        return;
                    const auto sourceVolume =
                        engineSoundVolumes.find(sound);
                    const float volume =
                        spatial.gain *
                        (sourceVolume != engineSoundVolumes.end()
                             ? sourceVolume->second
                             : 1.0F);
                    r3d::audio::PlayOptions playOptions;
                    playOptions.bus = r3d::audio::Bus::Effects;
                    playOptions.volume = volume;
                    const auto voice =
                        audio.play(sound, playOptions, audioError);
                    if (voice != r3d::audio::invalidVoice)
                        audio.setVoiceParameters(
                            voice, volume, 1.0F, 0.0F);
                };
                for (const auto& event : raceSession.events())
                {
                    if (event.kind ==
                                 r3d::game::originalrace::RaceEventKind::
                                     EffectSound &&
                             !event.soundPath.empty())
                    {
                        const auto sound =
                            loadEngineSound(event.soundPath);
                        if (event.soundContactActor !=
                            std::numeric_limits<std::uint32_t>::max())
                        {
                            const auto active = std::find_if(
                                contactEffectAudio.begin(),
                                contactEffectAudio.end(),
                                [&](const ContactEffectAudio& source) {
                                    return source.racer == event.racer &&
                                           source.actor ==
                                               event.soundContactActor &&
                                           source.surface ==
                                               event.soundContactSurface;
                                });
                            if (active == contactEffectAudio.end() &&
                                sound != r3d::audio::invalidSound)
                            {
                                contactEffectAudio.push_back(
                                    {event.racer,
                                     event.soundContactActor,
                                     event.soundContactSurface,
                                     event.position, sound});
                            }
                        }
                        else if (event.soundSource !=
                            r3d::game::originalrace::RacerRuntime::
                                invalidWeapon)
                        {
                            // ShotEffect::GiveSource3d chooses an existing
                            // Source3d. Source3d::Play ignores the call while
                            // that exact source is already requested/playing.
                            const auto active = std::find_if(
                                shotEffectAudio.begin(),
                                shotEffectAudio.end(),
                                [&](const ShotEffectAudio& source) {
                                    return source.owner == event.racer &&
                                           source.source == event.soundSource &&
                                           source.path == event.soundPath;
                                });
                            if (active == shotEffectAudio.end() &&
                                sound != r3d::audio::invalidSound)
                            {
                                shotEffectAudio.push_back(
                                    {event.racer, event.soundSource,
                                     event.soundPath, event.position,
                                     sound});
                            }
                        }
                        else if (event.soundLifetimeSeconds > 0.0F)
                        {
                            if (sound != r3d::audio::invalidSound)
                            {
                                TimedEffectAudio source;
                                source.followRacer =
                                    event.soundFollowRacer;
                                source.remainingSeconds =
                                    event.soundLifetimeSeconds;
                                source.position = event.position;
                                source.sound = sound;
                                if (source.followRacer <
                                    raceVehicles.size())
                                {
                                    const auto& body = raceVehicles[
                                        source.followRacer].body;
                                    const r3d::physics::Quat inverse{
                                        -body.rotation.x,
                                        -body.rotation.y,
                                        -body.rotation.z,
                                        body.rotation.w};
                                    auto local = rotateRaceVector(
                                        inverse,
                                        {event.position.x - body.position.x,
                                         event.position.y - body.position.y,
                                         event.position.z - body.position.z});
                                    if (std::abs(body.scale.x) > 0.000001F)
                                        local.x /= body.scale.x;
                                    if (std::abs(body.scale.y) > 0.000001F)
                                        local.y /= body.scale.y;
                                    if (std::abs(body.scale.z) > 0.000001F)
                                        local.z /= body.scale.z;
                                    source.followOffset = local;
                                }
                                timedEffectAudio.push_back(
                                    std::move(source));
                            }
                        }
                        else
                        {
                            playSpatial(sound, event.position);
                        }
                    }
                }
                for (auto source = shotEffectAudio.begin();
                     source != shotEffectAudio.end();)
                {
                    if (source->voice != r3d::audio::invalidVoice &&
                        !audio.isVoiceActive(source->voice))
                    {
                        source = shotEffectAudio.erase(source);
                        continue;
                    }
                    // EventEffect::OnProgress moves all initialized
                    // ShotEffect sources with their owning weapon/car.
                    if (source->owner < raceVehicles.size())
                        source->position =
                            raceVehicles[source->owner].body.position;
                    const float dx = source->position.x - listener.x;
                    const float dy = source->position.y - listener.y;
                    const float dz = source->position.z - listener.z;
                    const auto spatial =
                        rrr3d::audio::originalSource3dFlatMix(
                            std::sqrt(dx * dx + dy * dy + dz * dz),
                            source->spatialProxyPlaying);
                    source->spatialProxyPlaying = spatial.proxyPlaying;
                    const auto resourceVolume =
                        engineSoundVolumes.find(source->sound);
                    const float volume =
                        spatial.gain *
                        (resourceVolume != engineSoundVolumes.end()
                             ? resourceVolume->second
                             : 1.0F);
                    if (source->voice == r3d::audio::invalidVoice &&
                        spatial.started)
                    {
                        r3d::audio::PlayOptions options;
                        options.bus = r3d::audio::Bus::Effects;
                        options.volume = volume;
                        source->voice = audio.play(
                            source->sound, options, audioError);
                        if (source->voice == r3d::audio::invalidVoice)
                            source->spatialProxyPlaying = false;
                    }
                    if (source->voice != r3d::audio::invalidVoice)
                    {
                        audio.setVoiceParameters(
                            source->voice, volume, 1.0F, 0.0F);
                        // Proxy::Stop does not rewind Streaming. The SDL
                        // paused voice therefore resumes at the same sample
                        // after Source3d's 30/45 metre hysteresis restarts it.
                        audio.setVoicePaused(
                            source->voice,
                            raceSession.phase() ==
                                    r3d::game::originalrace::RacePhase::Paused ||
                                !source->spatialProxyPlaying);
                    }
                    ++source;
                }
                for (auto source = timedEffectAudio.begin();
                     source != timedEffectAudio.end();)
                {
                    if (raceSession.phase() !=
                        r3d::game::originalrace::RacePhase::Paused)
                    {
                        source->remainingSeconds -= frameSeconds;
                    }
                    if (source->remainingSeconds <= 0.0F)
                    {
                        audio.stop(source->voice);
                        source = timedEffectAudio.erase(source);
                        continue;
                    }
                    if (source->voice != r3d::audio::invalidVoice &&
                        !audio.isVoiceActive(source->voice))
                    {
                        source = timedEffectAudio.erase(source);
                        continue;
                    }
                    if (source->followRacer < raceVehicles.size())
                    {
                        const auto& body =
                            raceVehicles[source->followRacer].body;
                        const auto offset = rotateRaceVector(
                            body.rotation,
                            {source->followOffset.x * body.scale.x,
                             source->followOffset.y * body.scale.y,
                             source->followOffset.z * body.scale.z});
                        source->position = {
                            body.position.x + offset.x,
                            body.position.y + offset.y,
                            body.position.z + offset.z};
                    }
                    const float dx = source->position.x - listener.x;
                    const float dy = source->position.y - listener.y;
                    const float dz = source->position.z - listener.z;
                    const auto spatial =
                        rrr3d::audio::originalSource3dFlatMix(
                            std::sqrt(dx * dx + dy * dy + dz * dz),
                            source->spatialProxyPlaying);
                    source->spatialProxyPlaying = spatial.proxyPlaying;
                    const auto resourceVolume =
                        engineSoundVolumes.find(source->sound);
                    const float volume =
                        spatial.gain *
                        (resourceVolume != engineSoundVolumes.end()
                             ? resourceVolume->second
                             : 1.0F);
                    if (source->voice == r3d::audio::invalidVoice &&
                        spatial.started)
                    {
                        r3d::audio::PlayOptions options;
                        options.bus = r3d::audio::Bus::Effects;
                        options.volume = volume;
                        source->voice = audio.play(
                            source->sound, options, audioError);
                        if (source->voice == r3d::audio::invalidVoice)
                            source->spatialProxyPlaying = false;
                    }
                    if (source->voice != r3d::audio::invalidVoice)
                    {
                        audio.setVoiceParameters(
                            source->voice, volume, 1.0F, 0.0F);
                        audio.setVoicePaused(
                            source->voice,
                            raceSession.phase() ==
                                    r3d::game::originalrace::RacePhase::Paused ||
                                !source->spatialProxyPlaying);
                    }
                    ++source;
                }
                for (auto source = contactEffectAudio.begin();
                     source != contactEffectAudio.end();)
                {
                    const auto contact = std::find_if(
                        raceSession.effects().begin(),
                        raceSession.effects().end(),
                        [&](const auto& effect) {
                            return effect.kind ==
                                       r3d::game::originalrace::
                                           RaceEventKind::ContactImpact &&
                                   effect.racer == source->racer &&
                                   effect.contactActor == source->actor &&
                                   effect.contactSurface == source->surface &&
                                   effect.emissionEndSeconds >=
                                       effect.ageSeconds;
                        });
                    if (contact == raceSession.effects().end())
                    {
                        audio.stop(source->voice);
                        source = contactEffectAudio.erase(source);
                        continue;
                    }
                    if (source->voice != r3d::audio::invalidVoice &&
                        !audio.isVoiceActive(source->voice))
                    {
                        // PairPxContactEffect never rewinds its Source3d;
                        // after pmOnce reaches EOF the pair-owned source is
                        // silent until this actor pair is released/recreated.
                        source = contactEffectAudio.erase(source);
                        continue;
                    }
                    source->position = contact->origin;
                    const float dx = source->position.x - listener.x;
                    const float dy = source->position.y - listener.y;
                    const float dz = source->position.z - listener.z;
                    const auto spatial =
                        rrr3d::audio::originalSource3dFlatMix(
                            std::sqrt(dx * dx + dy * dy + dz * dz),
                            source->spatialProxyPlaying);
                    source->spatialProxyPlaying = spatial.proxyPlaying;
                    const auto resourceVolume =
                        engineSoundVolumes.find(source->sound);
                    const float volume =
                        spatial.gain *
                        (resourceVolume != engineSoundVolumes.end()
                             ? resourceVolume->second
                             : 1.0F);
                    if (source->voice == r3d::audio::invalidVoice &&
                        spatial.started)
                    {
                        r3d::audio::PlayOptions options;
                        options.bus = r3d::audio::Bus::Effects;
                        options.volume = volume;
                        source->voice = audio.play(
                            source->sound, options, audioError);
                        if (source->voice == r3d::audio::invalidVoice)
                            source->spatialProxyPlaying = false;
                    }
                    if (source->voice != r3d::audio::invalidVoice)
                    {
                        audio.setVoiceParameters(
                            source->voice, volume, 1.0F, 0.0F);
                        audio.setVoicePaused(
                            source->voice,
                            raceSession.phase() ==
                                    r3d::game::originalrace::RacePhase::Paused ||
                                !source->spatialProxyPlaying);
                    }
                    ++source;
                }
                commentator.update(
                    *originalRace, raceSession, frameSeconds, audioError);
            }
#endif
            if (raceSession.phase() !=
                r3d::game::originalrace::RacePhase::Paused)
            {
                for (const auto& respawn : raceSession.takeRespawns())
                    physicsWorld->resetVehicle(
                        respawn.racer, respawn.position,
                        respawn.direction);
                for (const auto& velocity :
                     raceSession.takeVelocityRequests())
                {
                    physicsWorld->addLinearVelocity(
                        velocity.racer, velocity.delta);
                }
                for (const auto& velocity :
                     raceSession.takeAngularVelocityRequests())
                {
                    physicsWorld->addAngularVelocity(
                        velocity.racer, velocity.delta);
                }
                for (std::size_t racer = 0;
                     racer < raceSession.racers().size(); ++racer)
                {
                    physicsWorld->setWheelTractionEnabled(
                        racer,
                        raceSession.racers()[racer].clutchSeconds <= 0.0F);
                    if (raceSession.racers()[racer].slowSeconds > 0.0F)
                        physicsWorld->clampLinearSpeed(racer, 20.0F);
                }
                auto vehicleInputs = raceSession.vehicleInputs();
                if (options->raceRenderSmokeTest)
                {
                    for (std::size_t index = 1U;
                         index < vehicleInputs.size() &&
                         index < raceVehicles.size() &&
                         index < raceSession.racers().size(); ++index)
                    {
                        raceAiThrottleFrames[index] +=
                            vehicleInputs[index].throttle > 0.5F ? 1U : 0U;
                        raceAiBrakeFrames[index] +=
                            vehicleInputs[index].brake > 0.5F ? 1U : 0U;
                        raceAiReverseFrames[index] +=
                            vehicleInputs[index].reverse > 0.5F ? 1U : 0U;
                    }
                }
#ifdef RRR3D_NETWORK
                if (networkMatchStarted)
                {
                    for (std::size_t index = 0U;
                         index < networkRaceModelOrder.size() &&
                         index < vehicleInputs.size(); ++index)
                    {
                        const auto player = std::find_if(
                            networkSnapshot.models.players.begin(),
                            networkSnapshot.models.players.end(),
                            [&](const auto& candidate) {
                                return candidate.modelId ==
                                       networkRaceModelOrder[index];
                            });
                        if (player ==
                                networkSnapshot.models.players.end() ||
                            player->owner)
                        {
                            continue;
                        }
                        auto& input = vehicleInputs[index];
                        input = {};
                        switch (player->vehicle.moveState)
                        {
                        case 1U:
                            input.brake = 1.0F;
                            break;
                        case 2U:
                            input.reverse = 1.0F;
                            break;
                        case 3U:
                            input.throttle = 1.0F;
                            break;
                        default:
                            break;
                        }
                        if (player->vehicle.steerState == 1U)
                            input.steering = 1.0F;
                        else if (player->vehicle.steerState == 2U)
                            input.steering = -1.0F;
                        if (player->vehicle.steerState != 0U &&
                            index < physicsDescription->spawns.size())
                        {
                            const float maximum =
                                physicsDescription->spawns[index]
                                    .vehicle.steerAngle;
                            if (maximum > 0.0001F &&
                                std::abs(
                                    player->vehicle.steerWheelsAngle) >
                                    0.0001F)
                            {
                                input.steering = std::clamp(
                                    player->vehicle.steerWheelsAngle /
                                        maximum,
                                    -1.0F, 1.0F);
                            }
                        }
                    }
                }
#endif
                physicsWorld->step(frameSeconds, vehicleInputs);
                for (std::size_t index = 0;
                     index < physicsWorld->vehicleCount(); ++index)
                {
                    raceVehicles[index] = physicsWorld->vehicle(index);
                    if (options->raceRenderSmokeTest && index > 0U &&
                        index < raceSession.racers().size())
                    {
                        maximumRaceAiSpeeds[index] = std::max(
                            maximumRaceAiSpeeds[index],
                            std::abs(raceVehicles[index].speed));
                        maximumRaceAiProgress[index] = std::max(
                            maximumRaceAiProgress[index],
                            static_cast<float>(
                                raceSession.racers()[index].completedLaps) +
                                static_cast<float>(
                                    raceSession.racers()[index].nextPathNode) /
                                    static_cast<float>(std::max<std::size_t>(
                                        originalRace->tracePath.size(), 1U)));
                    }
                }
#ifdef RRR3D_NETWORK
                if (networkMatchStarted &&
                    !publishLocalNetworkPlayer())
                {
                    runtimeSmokeFailed = true;
                    running = false;
                }
#endif
            }
            for (std::size_t index = 0;
                 index < physicsWorld->decorationCount() &&
                 index < originalRace->decorationInstances.size(); ++index)
            {
                const auto& state = physicsWorld->decoration(index);
                const auto& instance =
                    originalRace->decorationInstances[index];
                const auto& definition =
                    originalRace->decorationDefinitions.at(
                        instance.definition);
                if (state.active && definition.dynamicBody)
                {
                    originalRace->decorationInstances[index].transform =
                        state.body;
                }
            }
            decorationFragments.clear();
            decorationFragments.reserve(
                decorationDebrisBindings.size());
            for (const auto& binding : decorationDebrisBindings)
            {
                if (binding.debris >= physicsWorld->debrisCount())
                    continue;
                const auto& debris =
                    physicsWorld->debris(binding.debris);
                if (!debris.active)
                    continue;
                decorationFragments.push_back(
                    {binding.instance, binding.piece,
                     debris.body});
            }
            vehicleDeathFragments.clear();
            vehicleDeathFragments.reserve(
                vehicleDebrisBindings.size());
            for (const auto& binding : vehicleDebrisBindings)
            {
                if (binding.debris >= physicsWorld->debrisCount())
                    continue;
                const auto& debris =
                    physicsWorld->debris(binding.debris);
                if (!debris.active)
                    continue;
                vehicleDeathFragments.push_back(
                    {binding.racer, binding.effect, debris.body});
            }
            raceElapsedSeconds = raceSession.elapsedSeconds();
#ifdef RRR3D_AUDIO
            if (!raceVehicles.empty())
            {
                const auto& listener =
                    raceVehicles.front().body.position;
                const bool audioPaused =
                    raceSession.phase() ==
                    r3d::game::originalrace::RacePhase::Paused;
                for (std::size_t racer = 0;
                     racer < engineAudio.size() &&
                     racer < raceVehicles.size(); ++racer)
                {
                    const auto& sourceRacer =
                        originalRace->racers[racer];
                    const auto& definition =
                        sourceRacer.hasConfiguredVehicle
                            ? sourceRacer.configuredVehicle
                            : originalRace->vehicles.at(
                                  sourceRacer.vehicle);
                    auto& motorAudio = engineAudio[racer];
                    const float targetRpm =
                        raceVehicles[racer].engineRpm;
                    const float distanceRpm =
                        targetRpm - motorAudio.currentRpm;
                    const float motorStep =
                        10000.0F * (audioPaused ? 0.0F : frameSeconds);
                    motorAudio.currentRpm += std::clamp(
                        distanceRpm, -motorStep, motorStep);
                    const float minimumRpm = std::max(
                        definition.physics.idlingRpm, 1.0F);
                    const float maximumRpm = std::max(
                        definition.physics.maximumRpm,
                        minimumRpm + 1.0F);
                    const float idleAlpha = std::clamp(
                        0.5F *
                            (motorAudio.currentRpm - minimumRpm) /
                            minimumRpm,
                        0.0F, 1.0F);
                    const float rpmAlpha = std::clamp(
                        (motorAudio.currentRpm - minimumRpm) /
                            (maximumRpm - minimumRpm),
                        0.0F, 1.0F);
                    const auto& source =
                        raceVehicles[racer].body.position;
                    const float dx = source.x - listener.x;
                    const float dy = source.y - listener.y;
                    const float dz = source.z - listener.z;
                    const float distance =
                        std::sqrt(dx * dx + dy * dy + dz * dz);
                    const auto spatial =
                        rrr3d::audio::originalSource3dFlatMix(
                            distance,
                            motorAudio.spatialProxyPlaying);
                    motorAudio.spatialProxyPlaying =
                        spatial.proxyPlaying;
                    const auto idleSourceVolume =
                        engineSoundVolumes.find(motorAudio.idle);
                    const auto rpmSourceVolume =
                        engineSoundVolumes.find(motorAudio.rpm);
                    const float idleVolume =
                        spatial.gain * (1.0F - idleAlpha) *
                        (idleSourceVolume != engineSoundVolumes.end()
                             ? idleSourceVolume->second
                             : 1.0F);
                    const float rpmVolume =
                        spatial.gain * idleAlpha *
                        (definition.rpmVolumeRange[0] +
                         rpmAlpha *
                             (definition.rpmVolumeRange[1] -
                              definition.rpmVolumeRange[0])) *
                        (rpmSourceVolume != engineSoundVolumes.end()
                             ? rpmSourceVolume->second
                             : 1.0F);
                    const float pitch =
                        definition.rpmFrequencyRange[0] +
                        rpmAlpha *
                            (definition.rpmFrequencyRange[1] -
                             definition.rpmFrequencyRange[0]);
                    audio.setVoiceParameters(
                        motorAudio.idleVoice, idleVolume, 1.0F, 0.0F);
                    audio.setVoiceParameters(
                        motorAudio.rpmVoice, rpmVolume, pitch,
                        0.0F);
                    audio.setVoicePaused(
                        motorAudio.idleVoice,
                        audioPaused ||
                            !motorAudio.spatialProxyPlaying);
                    audio.setVoicePaused(
                        motorAudio.rpmVoice,
                        audioPaused ||
                            !motorAudio.spatialProxyPlaying);
                    if (racer >= wheelSlipVoices.size())
                        continue;
                    auto& slipVoices = wheelSlipVoices[racer];
                    const auto wheelCount = std::min(
                        {raceVehicles[racer].wheelContacts.size(),
                         definition.wheelSlipEffects.size(),
                         slipVoices.size()});
                    for (std::size_t wheel = 0;
                         wheel < wheelCount; ++wheel)
                    {
                        const auto& contact =
                            raceVehicles[racer].wheelContacts[wheel];
                        const float slip =
                            definition.wheelSlipEffects[wheel] &&
                                    contact.hasContact
                                ? std::max(
                                      std::abs(
                                          contact.longitudinalSlip) -
                                          0.4F,
                                      0.0F) +
                                      std::max(
                                          std::abs(
                                              contact.lateralSlip) -
                                              0.7F,
                                          0.0F)
                                : 0.0F;
                        auto& voice = slipVoices[wheel];
                        if (slip <= 0.0F)
                        {
                            if (voice.voice !=
                                r3d::audio::invalidVoice)
                            {
                                audio.stop(voice.voice);
                                voice = {};
                            }
                            continue;
                        }
                        const auto& wheelPosition =
                            contact.position;
                        const float wheelDx =
                            wheelPosition.x - listener.x;
                        const float wheelDy =
                            wheelPosition.y - listener.y;
                        const float wheelDz =
                            wheelPosition.z - listener.z;
                        const auto wheelSpatial =
                            rrr3d::audio::originalSource3dFlatMix(
                                std::sqrt(
                                    wheelDx * wheelDx +
                                    wheelDy * wheelDy +
                                    wheelDz * wheelDz),
                                voice.spatialProxyPlaying);
                        voice.spatialProxyPlaying =
                            wheelSpatial.proxyPlaying;
                        if (voice.voice ==
                                r3d::audio::invalidVoice &&
                            voice.spatialProxyPlaying)
                        {
                            r3d::audio::PlayOptions options;
                            options.bus = r3d::audio::Bus::Effects;
                            options.loop = true;
                            options.volume = 0.0F;
                            voice.voice = audio.play(
                                wheelSlipSound, options, audioError);
                        }
                        if (voice.voice !=
                            r3d::audio::invalidVoice)
                        {
                            const auto slipSourceVolume =
                                engineSoundVolumes.find(
                                    wheelSlipSound);
                            audio.setVoiceParameters(
                                voice.voice,
                                wheelSpatial.gain *
                                    std::clamp(
                                        slip * 4.0F, 0.0F, 1.0F) *
                                    (slipSourceVolume !=
                                             engineSoundVolumes.end()
                                         ? slipSourceVolume->second
                                         : 1.0F),
                                1.0F, 0.0F);
                            audio.setVoicePaused(
                                voice.voice,
                                audioPaused ||
                                    !voice.spatialProxyPlaying);
                        }
                    }
                }
            }
#endif
            maximumRaceSmokeSpeed = std::max(
                maximumRaceSmokeSpeed,
                std::sqrt(
                    physicsWorld->vehicle().linearVelocity.x *
                        physicsWorld->vehicle().linearVelocity.x +
                    physicsWorld->vehicle().linearVelocity.y *
                        physicsWorld->vehicle().linearVelocity.y +
                    physicsWorld->vehicle().linearVelocity.z *
                        physicsWorld->vehicle().linearVelocity.z));
            maximumRaceSmokeContacts = std::max(
                maximumRaceSmokeContacts,
                physicsWorld->vehicle().contactCount);
#ifdef RRR3D_NETWORK
            if (networkMatchStarted && networkHostRequested &&
                networkRaceStarted && !networkRaceExitApplied &&
                raceSession.finishPresentationReady())
            {
                std::string error;
                if (!networkSession.exitRace(
                        networkSnapshot.models.match.track,
                        networkSnapshot.models.match.weather,
                        collectNetworkRaceResults(), error))
                {
                    std::cerr << "Original NetRace::ExitRace failed: "
                              << error << '\n';
                    runtimeSmokeFailed = true;
                    running = false;
                }
                else
                {
                    networkRaceExitApplied = true;
                    networkRaceStarted = false;
                    refreshNetworkRuntimePages();
                }
            }
#endif
            if (!raceSession.racers().empty() &&
                raceSession.finishPresentationReady())
            {
#ifdef RRR3D_NETWORK
                showFinishMenu(
                    !networkMatchStarted || networkHostRequested);
#else
                showFinishMenu();
#endif
            }
        }
#endif

#ifdef RRR3D_AUDIO
#ifdef RRR3D_PHYSICS
        if (inRace && !gameMusic.update(audioError))
        {
            std::cerr << "Original game MusicCat runtime failed: "
                      << audioError << '\n';
            runtimeSmokeFailed = true;
            running = false;
        }
#endif
        if (!finalMusic.update(audioError))
        {
            std::cerr << "Original FinalMenu music runtime failed: "
                      << audioError << '\n';
            runtimeSmokeFailed = true;
            running = false;
        }
        else if (menuStack.back() == MenuScreen::Credits &&
                 finalMusic.currentVoiceActive())
        {
            finalMusicPlaybackObserved = true;
        }
        if (!music.update(audioError))
        {
            std::cerr << "Original MusicCat runtime failed: " << audioError
                      << '\n';
            runtimeSmokeFailed = true;
            running = false;
        }

        if (running && options->audioSmokeTest &&
            musicSmokePhase != MusicSmokePhase::Complete)
        {
            if (SDL_GetTicks() >= musicSmokeDeadline)
            {
                std::cerr << "Milestone 8 MusicCat transition smoke timed "
                             "out while the render loop remained active\n";
                runtimeSmokeFailed = true;
                running = false;
            }
            else if (musicSmokePhase ==
                         MusicSmokePhase::WaitingForDecode &&
                     music.allTracksLoaded() &&
                     music.currentVoiceActive())
            {
                bool metadataValid = renderedFrames > 1;
                for (std::size_t index = 0;
                     index < originalaudio::menuTracks.size(); ++index)
                {
                    const auto* info = music.trackInfo(index);
                    metadataValid = metadataValid && info != nullptr &&
                                    info->sourceSampleRate == 44100 &&
                                    info->sourceChannels == 2 &&
                                    info->durationSeconds > 60.0 &&
                                    info->mixerFrames != 0 &&
                                    info->peakAmplitude > 0.0F &&
                                    info->rmsAmplitude > 0.0F;
                }
                const auto current = music.currentTrack();
                if (!metadataValid || !current ||
                    !music.pause(true, audioError))
                {
                    std::cerr << "Milestone 8 background decode/pause "
                                 "verification failed: "
                              << (audioError.empty()
                                      ? "invalid three-track metadata"
                                      : audioError)
                              << '\n';
                    runtimeSmokeFailed = true;
                    running = false;
                }
                else
                {
                    smokeTrackOrder[0] = *current;
                    smokePausePosition =
                        music.currentPositionFrames();
                    smokePhaseTicks = SDL_GetTicks();
                    musicSmokePhase =
                        MusicSmokePhase::WaitingWhilePaused;
                }
            }
            else if (musicSmokePhase ==
                         MusicSmokePhase::WaitingWhilePaused &&
                     SDL_GetTicks() - smokePhaseTicks >= 100)
            {
                if (!music.paused() ||
                    music.currentPositionFrames() != smokePausePosition ||
                    !music.pause(false, audioError))
                {
                    std::cerr << "Milestone 8 MusicCat pause/resume "
                                 "verification failed: "
                              << audioError << '\n';
                    runtimeSmokeFailed = true;
                    running = false;
                }
                else
                {
                    musicSmokePhase =
                        MusicSmokePhase::WaitingForResume;
                }
            }
            else if (musicSmokePhase ==
                         MusicSmokePhase::WaitingForResume &&
                     music.currentPositionFrames() > smokePausePosition)
            {
                const auto* info =
                    music.trackInfo(smokeTrackOrder[0]);
                const std::uint64_t tailFrames = 4800;
                if (info == nullptr || info->mixerFrames <= tailFrames ||
                    !music.seekCurrent(info->mixerFrames - tailFrames,
                                       audioError))
                {
                    std::cerr << "Milestone 8 MusicCat resume/automatic-Next "
                                 "setup failed: "
                              << audioError << '\n';
                    runtimeSmokeFailed = true;
                    running = false;
                }
                else
                {
                    musicSmokePhase =
                        MusicSmokePhase::WaitingForAutomaticNext;
                }
            }
            else if (musicSmokePhase ==
                         MusicSmokePhase::WaitingForAutomaticNext)
            {
                const auto current = music.currentTrack();
                if (current && *current != smokeTrackOrder[0] &&
                    music.currentVoiceActive())
                {
                    smokeTrackOrder[1] = *current;
                    if (music.transitionCount() == 0 ||
                        !music.next(audioError))
                    {
                        std::cerr << "Milestone 8 MusicCat automatic/manual "
                                     "Next verification failed: "
                                  << audioError << '\n';
                        runtimeSmokeFailed = true;
                        running = false;
                    }
                    else
                    {
                        musicSmokePhase =
                            MusicSmokePhase::WaitingForManualNext;
                    }
                }
            }
            else if (musicSmokePhase ==
                         MusicSmokePhase::WaitingForManualNext)
            {
                const auto current = music.currentTrack();
                if (current && *current != smokeTrackOrder[1] &&
                    music.currentVoiceActive())
                {
                    smokeTrackOrder[2] = *current;
                    const bool allDistinct =
                        smokeTrackOrder[0] != smokeTrackOrder[1] &&
                        smokeTrackOrder[0] != smokeTrackOrder[2] &&
                        smokeTrackOrder[1] != smokeTrackOrder[2];
                    if (!allDistinct ||
                        !music.verifySavedState(audioError))
                    {
                        std::cerr << "Milestone 8 MusicCat shuffle/state "
                                     "verification failed: "
                                  << (audioError.empty()
                                          ? "track order repeated"
                                          : audioError)
                                  << '\n';
                        runtimeSmokeFailed = true;
                        running = false;
                    }
                    else
                    {
                        musicSmokePhase = MusicSmokePhase::Complete;
                        std::cout
                            << "Milestone 8 MusicCat follow-up smoke: all "
                               "three original menu tracks decoded in the "
                               "background while rendering; shuffle, pause/"
                               "resume, automatic Next, manual Next, and "
                               "state round-trip passed\n";
                    }
                }
            }
        }

        const auto currentMenuMusicTrack = music.currentTrack();
        if (currentMenuMusicTrack != lastMenuMusicTrack)
        {
            lastMenuMusicTrack = currentMenuMusicTrack;
            if (!sourceStartupActive)
            {
                showOriginalMusicInfo(
                    OriginalMusicDialogSource::Menu,
                    currentMenuMusicTrack);
            }
        }
#ifdef RRR3D_PHYSICS
        if (inRace)
        {
            const auto currentGameMusicTrack =
                gameMusic.currentTrack();
            if (currentGameMusicTrack != lastGameMusicTrack)
            {
                lastGameMusicTrack = currentGameMusicTrack;
                showOriginalMusicInfo(
                    OriginalMusicDialogSource::Game,
                    currentGameMusicTrack);
            }
        }
#endif
        musicDialogVisible = false;
        musicDialogOffset = 0.0F;
        if (musicDialogTime != -1.0F)
        {
            constexpr float musicDelay = 1.0F;
            constexpr float musicLife = 3.0F;
            musicDialogOffset =
                std::clamp(
                    musicDialogTime / musicDelay, 0.0F, 1.0F) -
                std::clamp(
                    (musicDialogTime - musicDelay - musicLife) /
                        musicDelay,
                    0.0F, 1.0F);
            if (musicDialogTime >=
                musicLife + 2.0F * musicDelay)
            {
                musicDialogTime = -1.0F;
            }
            else
            {
                musicDialogTime += frameSeconds;
                musicDialogVisible = true;
            }
        }
#endif

        if (sourceStartupActive)
        {
            constexpr float logoDelay = 1.0F;
            constexpr float logoFade = 1.0F;
            constexpr float logoHold = 3.0F;
            const float yardAlpha =
                std::clamp(
                    (sourceStartupSeconds - logoDelay) / logoFade,
                    0.0F, 1.0F) -
                std::clamp(
                    (sourceStartupSeconds -
                     (logoDelay + logoFade + logoHold)) /
                        logoFade,
                    0.0F, 1.0F);
            // Literal GameMode::OnFrame logo2Delay:
            // fade + hold + fade + two one-second blank delays = 7 s.
            constexpr float secondLogoDelay =
                logoFade + logoHold + logoFade +
                logoDelay + logoDelay;
            const float labAlpha =
                std::clamp(
                    (sourceStartupSeconds - secondLogoDelay) /
                        logoFade,
                    0.0F, 1.0F) -
                std::clamp(
                    (sourceStartupSeconds -
                     (secondLogoDelay + logoFade + logoHold)) /
                        logoFade,
                    0.0F, 1.0F);

            startupYardFadeObserved = startupYardFadeObserved ||
                (yardAlpha > 0.05F && yardAlpha < 0.95F);
            startupYardHoldObserved = startupYardHoldObserved ||
                yardAlpha > 0.99F;
            startupLabFadeObserved = startupLabFadeObserved ||
                (labAlpha > 0.05F && labAlpha < 0.95F);
            startupLabHoldObserved = startupLabHoldObserved ||
                labAlpha > 0.99F;
            startupInitialBlankObserved =
                startupInitialBlankObserved ||
                (sourceStartupSeconds < 1.0F &&
                 yardAlpha <= 0.0F && labAlpha <= 0.0F);
            startupInterlogoBlankObserved =
                startupInterlogoBlankObserved ||
                (sourceStartupSeconds >= 6.0F &&
                 sourceStartupSeconds < 7.0F &&
                 yardAlpha <= 0.0F && labAlpha <= 0.0F);

            device->beginFrame(camera, 0x000000ffU);
            if (sourceStartupSeconds < 12.0F)
            {
                if (yardAlpha > 0.0F)
                {
                    drawQuadTinted(
                        *device, quad, shader, startupYard,
                        static_cast<float>(startupYardImage.width),
                        static_cast<float>(startupYardImage.height),
                        menu::virtualWidth * 0.5F,
                        menu::virtualHeight * 0.5F, 10.0F,
                        transparent,
                        {1.0F, 1.0F, 1.0F, yardAlpha});
                }
                if (labAlpha > 0.0F)
                {
                    drawQuadTinted(
                        *device, quad, shader, startupLab,
                        static_cast<float>(startupLabImage.width),
                        static_cast<float>(startupLabImage.height),
                        menu::virtualWidth * 0.5F,
                        menu::virtualHeight * 0.5F, 10.0F,
                        transparent,
                        {1.0F, 1.0F, 1.0F, labAlpha});
                }
            }
            else
            {
                const float aspect =
                    static_cast<float>(startupLoadImage.width) /
                    static_cast<float>(startupLoadImage.height);
                const float width = std::min(
                    menu::virtualWidth,
                    menu::virtualHeight * aspect);
                const float height = width / aspect;
                drawQuad(
                    *device, quad, shader, startupLoad,
                    width, height, menu::virtualWidth * 0.5F,
                    menu::virtualHeight * 0.5F, 10.0F, opaque);
                startupLoadFrameObserved = true;
            }
            device->endFrame();
            ++renderedFrames;

            if (sourceStartupSeconds >= 12.25F)
            {
                sourceStartupActive = false;
                startupMenuTransitionObserved = true;
#ifdef RRR3D_PHYSICS
                if (sourcePreferredCameraAutodetect)
                {
                    sourceStartOptionsActive = true;
                    sourcePreferredCameraAutodetect = false;
                }
                else if (sourceDiscreteVideoChanged)
                {
                    profileState.config.quality.frameRateMode =
                        "sfrFixed";
                    sourceDiscreteVideoChanged = false;
                }
#endif
#ifdef RRR3D_AUDIO
                showOriginalMusicInfo(
                    OriginalMusicDialogSource::Menu,
                    music.currentTrack());
#endif
                previousFrameTicks = SDL_GetTicksNS();
                std::cout
                    << "Original GameMode::Run startup -> MainMenu2\n";
            }
            continue;
        }

#ifdef RRR3D_VIDEO
        if (originalMovieActive)
        {
            // GameMode::OnProgress switched the Windows renderer to a
            // dedicated video mode.  Continuing to submit the complete
            // bgfx/Metal frame below contends with hardware movie decode.
            SDL_Delay(4U);
            continue;
        }
#endif

#ifdef RRR3D_PHYSICS
        if (raceLoadingActive &&
            raceLoadingPresentedFrames >= 2U)
        {
            doStartCurrentRace();
        }
        if (raceLoadingActive)
        {
            const float aspect =
                static_cast<float>(loadingFrameImage.width) /
                static_cast<float>(loadingFrameImage.height);
            const float width = std::min(
                menu::virtualWidth,
                menu::virtualHeight * aspect);
            const float height = width / aspect;
            device->beginFrame(camera, 0x000000ffU);
            drawQuad(
                *device, quad, shader, loadingFrame,
                width, height, menu::virtualWidth * 0.5F,
                menu::virtualHeight * 0.5F, 10.0F, opaque);
            device->endFrame();
            raceLoadingFrameObserved =
                raceLoadingFrameObserved ||
                (loadingFrameImage.width == 1920U &&
                 loadingFrameImage.height == 900U &&
                 width <= menu::virtualWidth &&
                 height <= menu::virtualHeight);
            ++raceLoadingPresentedFrames;
            ++renderedFrames;
            continue;
        }
        if (inRace)
        {
            const float raceRenderSeconds =
                exitRaceDialogVisible ? 0.0F : frameSeconds;
            auto cameraStyle =
                profileState.config.preferredCamera;
            if (options->raceRenderSmokeTest)
            {
                cameraStyle =
                    renderedFrames < options->smokeFrames / 2U
                        ? r3d::game::originalrace::
                              PreferredCamera::Isometric
                        : r3d::game::originalrace::
                              PreferredCamera::ThirdPerson;
                raceCameraStylesObserved[
                    cameraStyle ==
                            r3d::game::originalrace::
                                PreferredCamera::ThirdPerson
                        ? 0U
                        : 1U] = true;
            }
            const auto raceCamera = raceRenderer.makeCamera(
                *device, physicsWorld->vehicle(),
                static_cast<std::uint32_t>(pixelWidth),
                static_cast<std::uint32_t>(pixelHeight),
                cameraStyle,
                profileState.config.cameraDistance, raceRenderSeconds);
            raceRenderer.renderFrame(
                *device, raceShader, raceCamera, 0x6b91b8ffU,
                *originalRace, raceVehicles, racePipeline,
                raceSession.decorationActive(),
                decorationFragments, vehicleDeathFragments,
                raceSession.bonusActive(),
                raceSession.racers(),
                raceSession.effects(), raceSession.mines(),
                raceSession.projectiles(), raceElapsedSeconds,
                profileState.config.quality,
                raceSession.countdownStage());
            raceHud.update(*device, *originalRace, raceSession,
                           raceVehicles, raceCamera, raceRenderSeconds);
            device->beginOverlay(camera);
            // PlayerStateFrame::OnInvalidate hides only _raceState when
            // enableHUD is false; MiniMapFrame likewise hides only its lap
            // widgets. Event overlays, the map and the countdown remain.
            raceHud.draw(*device, quad, shader, raceShader,
                         profileState.config.enableHud);
            drawUserChat();
            drawAcceptDialog();
#ifdef RRR3D_AUDIO
            drawOriginalMusicDialog();
#endif
            device->endFrame();
            const auto& telemetry = device->renderTelemetry();
            for (std::size_t pass = 0;
                 pass < r3d::renderer::renderPassCount; ++pass)
            {
                maximumRacePassBegins[pass] = std::max(
                    maximumRacePassBegins[pass],
                    telemetry.beginCount[pass]);
                maximumRacePassDraws[pass] = std::max(
                    maximumRacePassDraws[pass],
                    telemetry.drawCount[pass]);
            }
            for (std::size_t mode = 0;
                 mode < maximumRaceLightingDraws.size(); ++mode)
            {
                maximumRaceLightingDraws[mode] = std::max(
                    maximumRaceLightingDraws[mode],
                    telemetry.lightingDrawCount[mode]);
            }
            maximumEnvironmentMappedDraws = std::max(
                maximumEnvironmentMappedDraws,
                telemetry.environmentMappedDrawCount);
            maximumNormalMappedDraws = std::max(
                maximumNormalMappedDraws,
                telemetry.normalMappedDrawCount);
            maximumTransientDraws = std::max(
                maximumTransientDraws,
                telemetry.transientDrawCount);
            maximumActiveSpotLights = std::max(
                maximumActiveSpotLights,
                telemetry.activeSpotLightCount);
        }
        else
        {
#endif
        const bool drawingOriginalFinal =
            menuStack.back() == MenuScreen::Credits;
#ifdef RRR3D_PHYSICS
        const bool drawingOriginalProfiles =
            menuStack.back() == MenuScreen::Profiles;
        const bool drawingSourceStartOptions =
            sourceStartOptionsActive;
        const bool drawingOriginalOptions =
            isOriginalOptionsScreen(menuStack.back());
        const bool drawingOriginalGamers =
            menuStack.back() == MenuScreen::Gamers;
        const bool drawingOriginalRaceMenu =
            menuStack.back() == MenuScreen::RaceMenu;
        const bool drawingOriginalGarage =
            menuStack.back() == MenuScreen::Garage;
        const bool drawingOriginalWorkshop =
            menuStack.back() == MenuScreen::Workshop;
        const bool drawingOriginalAngar =
            menuStack.back() == MenuScreen::Planets;
        const bool drawingOriginalAchievements =
            menuStack.back() == MenuScreen::Achievements;
        const bool drawingOriginalFinish =
            menuStack.back() == MenuScreen::Finish;
        const r3d::game::originalrace::OriginalGarageCar*
            presentationCar = nullptr;
        bool presentationCarLocked = false;
        if (drawingOriginalGarage && !garageCarOrder.empty())
        {
            const auto& selectedView =
                garageCarOrder[garageViewIndex];
            presentationCar =
                &originalGarage->cars[selectedView.catalogIndex];
            presentationCarLocked = selectedView.locked;
        }
        else if (drawingOriginalWorkshop)
        {
            presentationCar = originalGarage->findCar(
                profileState.player.currentCar);
        }
        else if (drawingOriginalRaceMenu)
        {
            // RaceMenu::ApplyState keeps CarFrame visible in msMain and
            // RaceMainFrame::OnInvalidate applies the player's actual
            // car/loadout rather than the Garage defaults.
            presentationCar = originalGarage->findCar(
                profileState.player.currentCar);
        }
        if (drawingOriginalGamers)
            gamersSceneSeconds += frameSeconds;
        if (drawingOriginalAngar)
        {
            angarSceneSeconds += frameSeconds;
            const float redTime =
                std::fmod(angarSceneSeconds, 3.0F);
            const float redIntensity = 0.7F * (
                std::clamp(
                    (redTime - 1.5F) / 0.15F, 0.0F, 1.0F) -
                std::clamp(
                    (redTime - 2.85F) / 0.15F, 0.0F, 1.0F));
            auto& redLamp = originalAngarScene->environment.lamps[1];
            redLamp.enabled = redTime >= 1.5F;
            redLamp.color = {
                redIntensity, 0.0F, 0.0F, redIntensity};
            auto angarSourceCamera =
                originalAngarScene->presentationCamera;
            if (angarSceneSeconds >= 3.0F)
            {
                // CameraManager::csAutoObserver waits three seconds, then
                // left-multiplies the source camera by dRotZ at pi/96 rad/s
                // and recomputes position from the exact 50-unit target.
                const float angle =
                    (angarSceneSeconds - 3.0F) *
                    bx::kPi / 96.0F;
                const float sine = std::sin(angle);
                const float cosine = std::cos(angle);
                const auto base =
                    originalAngarScene->presentationCamera;
                angarSourceCamera.position = {
                    cosine * base.position.x -
                        sine * base.position.y,
                    sine * base.position.x +
                        cosine * base.position.y,
                    base.position.z};
                const float halfSine =
                    std::sin(angle * 0.5F);
                const float halfCosine =
                    std::cos(angle * 0.5F);
                angarSourceCamera.rotation = {
                    halfCosine * base.rotation.x -
                        halfSine * base.rotation.y,
                    halfCosine * base.rotation.y +
                        halfSine * base.rotation.x,
                    halfCosine * base.rotation.z +
                        halfSine * base.rotation.w,
                    halfCosine * base.rotation.w -
                        halfSine * base.rotation.z};
            }
            const auto angarCamera =
                angarRenderer.makePresentationCamera(
                    *device, angarSourceCamera,
                    static_cast<std::uint32_t>(pixelWidth),
                    static_cast<std::uint32_t>(pixelHeight));
            angarRenderer.renderFrame(
                *device, raceShader, angarCamera, 0x040818ffU,
                *originalAngarScene, angarVehicles, racePipeline,
                angarDecorationActive, garageDecorationFragments,
                garageVehicleDeathFragments, garageBonusActive,
                angarRacerRuntime, garageEffects, garageMines,
                garageProjectiles, angarSceneSeconds,
                profileState.config.quality);
            const auto& telemetry = device->renderTelemetry();
            const auto scenePass = static_cast<std::size_t>(
                r3d::renderer::RenderPass::Scene);
            const auto shadowPass = static_cast<std::size_t>(
                r3d::renderer::RenderPass::Shadow);
            const auto shadowFarPass = static_cast<std::size_t>(
                r3d::renderer::RenderPass::ShadowFar);
            const bool sourceLampShadowsObserved =
                profileState.config.quality.shadow < 1U ||
                (telemetry.beginCount[shadowPass] > 0U &&
                 telemetry.beginCount[shadowFarPass] > 0U);
            raceAngar3DObserved =
                raceAngar3DObserved ||
                (telemetry.drawCount[scenePass] > 0U &&
                 sourceLampShadowsObserved);
            // AngarFrame contains nested ViewPort3d widgets. The source
            // GUI renders them against a fresh viewport depth buffer; keep
            // the completed HDR scene color but clear its world depth.
            device->beginPass(
                r3d::renderer::RenderPass::Overlay, {}, camera,
                0U, false, true);
        }
        else if (presentationCar != nullptr)
        {
            garageSceneSeconds += frameSeconds;
            const auto& selectedCar = *presentationCar;
            const auto selectedVehicle = std::find_if(
                originalGarageScene->vehicles.begin(),
                originalGarageScene->vehicles.end(),
                [&](const auto& vehicle) {
                    return vehicle.record == selectedCar.record ||
                           recordName(vehicle.record) ==
                               recordName(selectedCar.record);
                });
            const std::size_t selectedRacer =
                selectedVehicle ==
                        originalGarageScene->vehicles.end()
                    ? originalGarageScene->racers.size()
                    : static_cast<std::size_t>(
                          std::distance(
                              originalGarageScene->vehicles.begin(),
                              selectedVehicle));
            for (std::size_t racer = 0;
                 racer < garageRacerRuntime.size(); ++racer)
            {
                auto& runtime = garageRacerRuntime[racer];
                runtime.destroyed =
                    presentationCarLocked ||
                    racer != selectedRacer;
                runtime.weaponSlots.fill(
                    r3d::game::originalrace::RacerRuntime::
                        invalidWeapon);
                if (racer < originalGarageScene->racers.size())
                    originalGarageScene->racers[racer].color =
                        profileState.player.color;
            }
            if (!presentationCarLocked &&
                selectedRacer < garageRacerRuntime.size())
            {
                auto& runtime =
                    garageRacerRuntime[selectedRacer];
                constexpr std::size_t firstWeaponPlacement =
                    static_cast<std::size_t>(
                        r3d::game::originalrace::GarageSlotType::
                            Weapon1);
                for (std::size_t slot = 0;
                     slot < runtime.weaponSlots.size(); ++slot)
                {
                    const auto& weaponRecord =
                        (drawingOriginalWorkshop ||
                         drawingOriginalRaceMenu)
                            ? profileState.player
                                  .slots[firstWeaponPlacement + slot]
                                  .record
                            : selectedCar
                                  .placements[
                                      firstWeaponPlacement + slot]
                                  .defaultItem;
                    const auto weapon = std::find_if(
                        originalGarageScene->weapons.begin(),
                        originalGarageScene->weapons.end(),
                        [&](const auto& candidate) {
                            return candidate.record == weaponRecord ||
                                   recordName(candidate.record) ==
                                       recordName(weaponRecord);
                        });
                    if (weapon !=
                        originalGarageScene->weapons.end())
                    {
                        runtime.weaponSlots[slot] =
                            static_cast<std::size_t>(
                                std::distance(
                                    originalGarageScene->weapons.begin(),
                                    weapon));
                    }
                }
            }
            garageDecorationActive[1] = presentationCarLocked;
            const float halfQuestionAngle =
                bx::kPi * garageSceneSeconds * 0.1F;
            originalGarageScene->decorationInstances[1]
                .transform.rotation = {
                    0.0F, 0.0F,
                    std::sin(halfQuestionAngle),
                    std::cos(halfQuestionAngle)};
            const auto presentationSourceCamera =
                drawingOriginalWorkshop
                    ? makeWorkshopPresentationCamera(
                          originalGarageScene
                              ->presentationCamera)
                    : makeAutoObserverPresentationCamera(
                          originalGarageScene
                              ->presentationCamera,
                          garageSceneSeconds, bx::kPi / 48.0F);
            const auto garageCamera =
                garageRenderer.makePresentationCamera(
                    *device, presentationSourceCamera,
                    static_cast<std::uint32_t>(pixelWidth),
                    static_cast<std::uint32_t>(pixelHeight));
            garageRenderer.renderFrame(
                *device, raceShader, garageCamera, 0x040818ffU,
                *originalGarageScene, garageVehicles, racePipeline,
                garageDecorationActive, garageDecorationFragments,
                garageVehicleDeathFragments, garageBonusActive,
                garageRacerRuntime, garageEffects, garageMines,
                garageProjectiles, garageSceneSeconds,
                profileState.config.quality);
            const auto& garageTelemetry =
                device->renderTelemetry();
            const auto scenePass = static_cast<std::size_t>(
                r3d::renderer::RenderPass::Scene);
            const auto shadowPass = static_cast<std::size_t>(
                r3d::renderer::RenderPass::Shadow);
            const auto shadowFarPass = static_cast<std::size_t>(
                r3d::renderer::RenderPass::ShadowFar);
            const bool observedPresentation3D =
                garageTelemetry.drawCount[scenePass] > 0U &&
                (profileState.config.quality.shadow < 1U ||
                 (garageTelemetry.beginCount[shadowPass] > 0U &&
                  garageTelemetry.beginCount[shadowFarPass] > 0U)) &&
                std::any_of(
                    garageTelemetry.lightingDrawCount.begin(),
                    garageTelemetry.lightingDrawCount.end(),
                    [](std::uint32_t draws) {
                        return draws > 0U;
                    });
            if (drawingOriginalWorkshop)
            {
                raceWorkshop3DObserved =
                    raceWorkshop3DObserved ||
                    observedPresentation3D;
                device->beginPass(
                    r3d::renderer::RenderPass::Overlay, {}, camera,
                    0U, false, true);
            }
            else if (drawingOriginalRaceMenu)
            {
                raceMain3DObserved =
                    raceMain3DObserved ||
                    observedPresentation3D;
                device->beginPass(
                    r3d::renderer::RenderPass::Overlay, {}, camera,
                    0U, false, true);
            }
            else
            {
                raceGarage3DObserved =
                    raceGarage3DObserved ||
                    observedPresentation3D;
                device->beginOverlay(camera);
            }
        }
        else
#endif
        {
            device->beginFrame(
                camera, drawingOriginalFinal ? 0x000000ffU
                                             : 0x040818ffU);
            if (!drawingOriginalFinal)
            {
                drawQuad(
                    *device, quad, shader, background,
                    menu::virtualWidth, menu::virtualHeight,
                    menu::virtualWidth * 0.5F,
                    menu::virtualHeight * 0.5F, 90.0F, opaque);
                drawQuad(
                    *device, quad, shader, topPanel,
                    static_cast<float>(model->topPanelImage.width),
                    static_cast<float>(model->topPanelImage.height),
                    menu::virtualWidth * 0.5F, 200.0F, 70.0F,
                    transparent);
            }
        }

        auto& activePage = activeMenuPage();
        if (drawingOriginalFinal)
        {
            constexpr float duration = 107.0F;
            const float progress =
                std::clamp(finalMenuSeconds / duration, 0.0F, 1.0F);
            const float slideMaximumWidth =
                menu::virtualWidth - 500.0F;
            const float slideMaximumHeight =
                menu::virtualHeight - 300.0F;
            const float slideAspect =
                static_cast<float>(finalSlideImages.front().width) /
                static_cast<float>(finalSlideImages.front().height);
            const float slideWidth = std::min(
                slideMaximumWidth, slideMaximumHeight * slideAspect);
            const float slideHeight = slideWidth / slideAspect;
            const float slideX =
                (menu::virtualWidth - 400.0F) * 0.5F;
            const float slideY = menu::virtualHeight * 0.5F;
            for (std::size_t index = 0U;
                 index < finalSlides.size(); ++index)
            {
                const float alpha1 =
                    static_cast<float>(index) /
                    static_cast<float>(finalSlides.size());
                const float alpha2 =
                    static_cast<float>(index + 1U) /
                    static_cast<float>(finalSlides.size());
                const float slideDuration =
                    (alpha2 - alpha1) * duration;
                const float slideTime = std::clamp(
                    (progress - alpha1) * duration,
                    0.0F, slideDuration);
                const float alpha =
                    std::clamp(slideTime, 0.0F, 1.0F) -
                    std::clamp(
                        slideTime - slideDuration, 0.0F, 1.0F);
                if (alpha <= 0.0F)
                    continue;
                finalSlidesObserved[index] =
                    finalSlidesObserved[index] || alpha >= 0.5F;
                drawQuadTinted(
                    *device, quad, shader, finalSlides[index],
                    slideWidth, slideHeight, slideX, slideY,
                    60.0F, transparent,
                    {1.0F, 1.0F, 1.0F, alpha});
            }

            constexpr float creditWidth = 480.0F;
            const float creditX = menu::virtualWidth - 250.0F;
            const float creditRootY =
                menu::virtualHeight -
                progress *
                    (finalCreditsHeight + menu::virtualHeight);
            finalCreditsMotionObserved =
                finalCreditsMotionObserved ||
                creditRootY < menu::virtualHeight - 1.0F;
            float sectionTop = creditRootY;
            for (const auto& section : finalCredits)
            {
                drawQuad(
                    *device, quad, shader, section.caption.texture,
                    std::min(section.caption.width, creditWidth),
                    section.caption.height, creditX,
                    sectionTop + section.caption.height * 0.5F,
                    35.0F, transparent);
                float lineTop =
                    sectionTop + section.caption.height + 10.0F;
                for (const auto& line : section.lines)
                {
                    drawQuad(
                        *device, quad, shader, line.texture,
                        std::min(line.width, creditWidth), line.height,
                        creditX, lineTop + line.height * 0.5F,
                        35.0F, transparent);
                    lineTop += line.height;
                }
                sectionTop += section.height;
            }

            const float backX =
                static_cast<float>(finalBackSelectedImage.width) * 0.5F;
            const float backY = menu::virtualHeight - 60.0F;
            drawQuad(
                *device, quad, shader, finalBackSelected,
                static_cast<float>(finalBackSelectedImage.width),
                static_cast<float>(finalBackSelectedImage.height),
                backX, backY, 20.0F, transparent);
            drawQuad(
                *device, quad, shader, finalBackText.texture,
                finalBackText.width, finalBackText.height,
                backX, backY, 10.0F, transparent);
            finalBackFrameObserved = true;
        }
#ifdef RRR3D_PHYSICS
        else if (drawingSourceStartOptions)
        {
            const float centerX = menu::virtualWidth * 0.5F;
            const float centerY = menu::virtualHeight * 0.5F;
            drawQuad(
                *device, quad, shader, optionsMask,
                menu::virtualWidth, menu::virtualHeight,
                centerX, centerY, 65.0F, transparent);
            drawQuad(
                *device, quad, shader, startOptionsBackground,
                static_cast<float>(
                    startOptionsBackgroundImage.width),
                static_cast<float>(
                    startOptionsBackgroundImage.height),
                centerX, centerY, 60.0F, transparent);

            for (std::size_t row = 0U; row < 4U; ++row)
            {
                const float rowY = centerY - 145.0F +
                    static_cast<float>(row) * 50.0F;
                const bool selected = startOptionsFocus == row;
                drawQuad(
                    *device, quad, shader, optionsRow,
                    static_cast<float>(optionsRowImage.width),
                    static_cast<float>(optionsRowImage.height),
                    centerX - 235.0F, rowY, 50.0F,
                    transparent);
                const auto& label =
                    selected
                        ? startOptionsLabelPage.selected[row]
                        : startOptionsLabelPage.normal[row];
                drawQuad(
                    *device, quad, shader, label.texture,
                    label.width, label.height,
                    centerX - 360.0F + label.width * 0.5F,
                    rowY, 25.0F, transparent);
                const auto& value =
                    selected
                        ? startOptionsValuePage.selected[row]
                        : startOptionsValuePage.normal[row];
                drawQuad(
                    *device, quad, shader, value.texture,
                    value.width, value.height,
                    centerX + 100.0F, rowY, 20.0F,
                    transparent);
                const Texture arrow =
                    selected ? optionsArrowSelected : optionsArrow;
                const auto& arrowImage =
                    selected ? optionsArrowSelectedImage
                             : optionsArrowImage;
                drawQuad(
                    *device, quad, shader, arrow,
                    static_cast<float>(arrowImage.width),
                    static_cast<float>(arrowImage.height),
                    centerX - 20.0F, rowY, 20.0F,
                    transparent);
                drawQuadRotated(
                    *device, quad, shader, arrow,
                    static_cast<float>(arrowImage.width),
                    static_cast<float>(arrowImage.height),
                    centerX + 220.0F, rowY, 20.0F,
                    bx::kPi, transparent);
            }

            constexpr float infoLineStep = 27.0F;
            const float firstInfoY =
                centerY + 70.0F -
                static_cast<float>(
                    startOptionsInfoLines.size() - 1U) *
                    infoLineStep * 0.5F;
            for (std::size_t line = 0U;
                 line < startOptionsInfoLines.size(); ++line)
            {
                const auto& info = startOptionsInfoLines[line];
                drawQuad(
                    *device, quad, shader, info.texture,
                    info.width, info.height, centerX,
                    firstInfoY +
                        static_cast<float>(line) * infoLineStep,
                    18.0F, transparent);
            }

            const float applyX = centerX - 10.0F;
            const float applyY = centerY + 138.0F;
            drawQuad(
                *device, quad, shader, startOptionsButton,
                static_cast<float>(startOptionsButtonImage.width),
                static_cast<float>(startOptionsButtonImage.height),
                applyX, applyY, 35.0F, transparent);
            const auto& apply =
                !startOptionsApplyEnabled
                    ? startOptionsActionPage.disabled.front()
                    : startOptionsFocus == 4U
                          ? startOptionsActionPage.selected.front()
                          : startOptionsActionPage.normal.front();
            drawQuad(
                *device, quad, shader, apply.texture,
                apply.width, apply.height, applyX, applyY,
                15.0F, transparent);

            startOptionsFrameObserved = true;
            startOptionsSelectGateObserved =
                startOptionsSelectGateObserved ||
                (startOptionsCameraIndex == 2U &&
                 !startOptionsApplyEnabled);
            startOptionsAllRowsObserved =
                startOptionsAllRowsObserved ||
                (startOptionsLabelPage.labels.size() == 4U &&
                 startOptionsValuePage.labels.size() == 4U &&
                 valid(startOptionsBackground) &&
                 valid(startOptionsButton) &&
                 valid(optionsRow) && valid(optionsArrow) &&
                 valid(optionsArrowSelected));
        }
        else if (drawingOriginalProfiles)
        {
            profileFrameObserved =
                profilePage.labels.size() ==
                    profileState.profiles.size() + 1U &&
                profileGridScroll <=
                    (profileState.profiles.size() > 4U
                         ? profileState.profiles.size() - 4U
                         : 0U);
            const float centerX =
                menu::virtualWidth * 0.5F;
            const float centerY =
                menu::virtualHeight * 0.5F;
            const auto visibleEnd = std::min(
                profileGridScroll + 4U,
                profileState.profiles.size());
            for (std::size_t index = profileGridScroll;
                 index < visibleEnd; ++index)
            {
                const float rowY =
                    centerY - 90.0F +
                    static_cast<float>(
                        index - profileGridScroll) *
                        menu::itemSpacing;
                const bool itemFocused =
                    profileFocus == ProfileFocus::Item &&
                    profileFocusIndex == index &&
                    !profileDeleteDialogVisible;
                if (itemFocused)
                {
                    drawQuad(
                        *device, quad, shader, selection,
                        static_cast<float>(
                            model->selectionImage.width),
                        static_cast<float>(
                            model->selectionImage.height),
                        centerX, rowY, 50.0F, transparent);
                }
                const auto& profileText =
                    itemFocused
                        ? profilePage.selected[index]
                        : profilePage.normal[index];
                drawQuad(
                    *device, quad, shader,
                    profileText.texture, profileText.width,
                    profileText.height, centerX, rowY,
                    25.0F, transparent);

                const bool closeFocused =
                    profileFocus == ProfileFocus::Close &&
                    profileFocusIndex == index &&
                    !profileDeleteDialogVisible;
                const float closeX =
                    centerX +
                    static_cast<float>(
                        model->selectionImage.width) *
                        0.5F -
                    40.0F;
                drawQuad(
                    *device, quad, shader,
                    closeFocused ? angarCloseSelected
                                 : angarClose,
                    static_cast<float>(
                        closeFocused
                            ? angarCloseSelectedImage.width
                            : angarCloseImage.width) *
                        1.8F,
                    static_cast<float>(
                        closeFocused
                            ? angarCloseSelectedImage.height
                            : angarCloseImage.height) *
                        1.8F,
                    closeX, rowY, 20.0F, transparent);
            }

            const bool canScrollUp =
                profileGridScroll > 0U;
            const bool canScrollDown =
                profileGridScroll + 4U <
                profileState.profiles.size();
            auto drawProfileArrow =
                [&](bool up, float y, bool enabled) {
                    const bool focused =
                        enabled && !profileDeleteDialogVisible &&
                        profileFocus ==
                            (up ? ProfileFocus::Up
                                : ProfileFocus::Down);
                    const auto texture =
                        !enabled
                            ? profileArrowDisabled
                            : focused
                                  ? profileArrowSelected
                                  : profileArrow;
                    const float sourceWidth =
                        static_cast<float>(
                            focused
                                ? profileArrowSelectedImage.width
                                : profileArrowImage.width);
                    const float sourceHeight =
                        static_cast<float>(
                            focused
                                ? profileArrowSelectedImage.height
                                : profileArrowImage.height);
                    constexpr float scale = 0.3F;
                    drawQuadRotated(
                        *device, quad, shader, texture,
                        sourceWidth * scale,
                        sourceHeight * scale,
                        centerX, y, 22.0F,
                        up ? bx::kPiHalf : -bx::kPiHalf,
                        transparent);
                };
            drawProfileArrow(
                true, centerY - 108.0F, canScrollUp);
            drawProfileArrow(
                false, centerY + 120.0F, canScrollDown);

            const auto backIndex =
                profilePage.labels.size() - 1U;
            const float backY = centerY + 150.0F;
            const bool backFocused =
                profileFocus == ProfileFocus::Back &&
                !profileDeleteDialogVisible;
            if (backFocused)
            {
                drawQuad(
                    *device, quad, shader, selection,
                    static_cast<float>(
                        model->selectionImage.width),
                    static_cast<float>(
                        model->selectionImage.height),
                    centerX, backY, 50.0F, transparent);
            }
            const auto& backText =
                backFocused
                    ? profilePage.selected[backIndex]
                    : profilePage.normal[backIndex];
            drawQuad(
                *device, quad, shader, backText.texture,
                backText.width, backText.height,
                centerX, backY, 25.0F, transparent);

        }
        else if (drawingOriginalOptions)
        {
            const float optionsCenterX = menu::virtualWidth * 0.5F;
            const float optionsCenterY = menu::virtualHeight * 0.5F;
            drawQuad(
                *device, quad, shader, optionsMask,
                menu::virtualWidth, menu::virtualHeight,
                optionsCenterX, optionsCenterY, 65.0F, transparent);
            drawQuad(
                *device, quad, shader, optionsBackground,
                static_cast<float>(optionsBackgroundImage.width),
                static_cast<float>(optionsBackgroundImage.height),
                optionsCenterX, optionsCenterY, 60.0F, transparent);

            const auto& header = optionsHeaderPage.normal.front();
            drawQuad(
                *device, quad, shader, header.texture,
                header.width, header.height, optionsCenterX,
                optionsCenterY - 317.0F, 20.0F, transparent);

            const auto state = optionsStateIndex(menuStack.back());
            for (std::size_t index = 0;
                 index < optionsStatePage.labels.size(); ++index)
            {
                const bool selectedState = index == state;
                const float buttonX = optionsCenterX - 558.0F;
                const float buttonY =
                    optionsCenterY - 125.0F +
                    static_cast<float>(index) * 100.0F;
                const auto& stateText =
                    selectedState
                        ? optionsStatePage.selected[index]
                        : optionsStatePage.normal[index];
                drawQuad(
                    *device, quad, shader,
                    selectedState ? optionsButtonSelected
                                  : optionsButton,
                    static_cast<float>(
                        selectedState
                            ? optionsButtonSelectedImage.width
                            : optionsButtonImage.width),
                    static_cast<float>(
                        selectedState
                            ? optionsButtonSelectedImage.height
                            : optionsButtonImage.height),
                    buttonX +
                        static_cast<float>(
                            selectedState
                                ? optionsButtonSelectedImage.width
                                : optionsButtonImage.width) *
                            0.5F,
                    buttonY, 45.0F, transparent);
                drawQuad(
                    *device, quad, shader, stateText.texture,
                    stateText.width, stateText.height,
                    buttonX + 62.0F + stateText.width * 0.5F,
                    buttonY, 20.0F, transparent);
            }

            const MenuPageVisual* names = nullptr;
            std::size_t rowCount = 0U;
            std::size_t visibleRows = 0U;
            float firstRowY = optionsCenterY - 174.0F;
            if (menuStack.back() == MenuScreen::GameOptions)
            {
                names = &gameOptionNamesPage;
                rowCount = gameOptionNamesPage.labels.size();
                visibleRows = 7U;
                firstRowY = optionsCenterY - 178.0F;
            }
            else if (
                menuStack.back() == MenuScreen::GraphicsOptions)
            {
                names = &graphicsOptionNamesPage;
                rowCount = graphicsOptionNamesPage.labels.size();
                visibleRows = rowCount;
            }
            else if (
                menuStack.back() == MenuScreen::SoundOptions)
            {
                names = &soundOptionNamesPage;
                rowCount = soundOptionNamesPage.labels.size();
                visibleRows = rowCount;
            }
            else
            {
                names = &controlsOptionsPage;
                rowCount = originalControlActions.size();
                visibleRows = controlsVisibleRows;
                firstRowY = optionsCenterY - 130.0F;
                drawQuad(
                    *device, quad, shader, keyboardIcon,
                    static_cast<float>(keyboardIconImage.width),
                    static_cast<float>(keyboardIconImage.height),
                    optionsCenterX + 255.0F,
                    optionsCenterY - 190.0F, 25.0F, transparent);
                drawQuad(
                    *device, quad, shader, gamepadIcon,
                    static_cast<float>(gamepadIconImage.width),
                    static_cast<float>(gamepadIconImage.height),
                    optionsCenterX + 435.0F,
                    optionsCenterY - 190.0F, 25.0F, transparent);
            }
            visibleRows = std::min(visibleRows, rowCount);
            const std::size_t scrollAnchor =
                menuSelection < rowCount ? menuSelection
                                         : rowCount - 1U;
            const std::size_t firstVisible =
                scrollAnchor < visibleRows
                    ? 0U
                    : std::min(
                          scrollAnchor - visibleRows + 1U,
                          rowCount - visibleRows);

            auto drawTextAt = [&](const TextVisual& text, float x,
                                  float y, float maxWidth) {
                const float scale = std::min(
                    1.0F,
                    maxWidth / std::max(text.width, 1.0F));
                drawQuad(
                    *device, quad, shader, text.texture,
                    text.width * scale, text.height * scale,
                    x + text.width * scale * 0.5F, y, 15.0F,
                    transparent);
            };
            for (std::size_t slot = 0U; slot < visibleRows; ++slot)
            {
                const std::size_t index = firstVisible + slot;
                const bool selectedRow = index == menuSelection;
                const bool rowEnabled =
                    index < activePage.enabled.size() &&
                    activePage.enabled[index];
                const float rowY =
                    firstRowY + static_cast<float>(slot) * 50.0F;
                const bool controls =
                    menuStack.back() == MenuScreen::ControlsOptions;
                drawQuad(
                    *device, quad, shader,
                    controls ? controlsRow : optionsRow,
                    static_cast<float>(
                        controls ? controlsRowImage.width
                                 : optionsRowImage.width),
                    static_cast<float>(
                        controls ? controlsRowImage.height
                                 : optionsRowImage.height),
                    optionsCenterX - 235.0F +
                        (controls
                             ? 0.0F
                             : static_cast<float>(
                                   optionsRowImage.width) *
                                   0.5F),
                    rowY, 45.0F,
                    transparent);
                const auto& name =
                    !rowEnabled ? names->disabled[index]
                    : selectedRow ? names->selected[index]
                                  : names->normal[index];
                drawTextAt(
                    name,
                    controls ? optionsCenterX - 374.0F
                             : optionsCenterX - 200.0F,
                    rowY, controls ? 300.0F : 420.0F);

                if (controls)
                {
                    const std::array<const MenuPageVisual*, 2>
                        valuePages{
                            &controlsKeyboardValuesPage,
                            &controlsGamepadValuesPage};
                    for (std::size_t controller = 0U;
                         controller < valuePages.size(); ++controller)
                    {
                        const bool selectedKey =
                            selectedRow &&
                            controlsUseGamepad ==
                                (controller == 1U);
                        const float keyX =
                            optionsCenterX + 255.0F +
                            static_cast<float>(controller) * 180.0F;
                        drawQuad(
                            *device, quad, shader,
                            selectedKey ? optionsKeySelected
                                        : optionsKey,
                            static_cast<float>(
                                selectedKey
                                    ? optionsKeySelectedImage.width
                                    : optionsKeyImage.width),
                            static_cast<float>(
                                selectedKey
                                    ? optionsKeySelectedImage.height
                                    : optionsKeyImage.height),
                            keyX, rowY, 35.0F, transparent);
                        const auto& value =
                            selectedKey
                                ? valuePages[controller]
                                      ->selected[index]
                                : valuePages[controller]
                                      ->normal[index];
                        const float scale = std::min(
                            1.0F,
                            135.0F /
                                std::max(value.width, 1.0F));
                        drawQuad(
                            *device, quad, shader, value.texture,
                            value.width * scale,
                            value.height * scale, keyX, rowY,
                            12.0F, transparent);
                    }
                    continue;
                }

                const bool volumeRow =
                    menuStack.back() == MenuScreen::SoundOptions &&
                    index >= 2U;
                if (volumeRow)
                {
                    const float volume =
                        std::array{
                            optionsDraftConfig.musicVolume,
                            optionsDraftConfig.effectsVolume,
                            optionsDraftConfig.voiceVolume}
                            [index - 2U];
                    const float normalized =
                        std::clamp(volume * 0.5F, 0.0F, 1.0F);
                    const float barX = optionsCenterX + 260.0F;
                    const float barWidth =
                        static_cast<float>(
                            optionsBarBackgroundImage.width);
                    drawQuad(
                        *device, quad, shader,
                        optionsBarBackground, barWidth,
                        static_cast<float>(
                            optionsBarBackgroundImage.height),
                        barX, rowY, 35.0F, transparent);
                    if (normalized > 0.0F)
                    {
                        drawQuad(
                            *device, quad, shader, optionsBar,
                            barWidth * normalized,
                            static_cast<float>(
                                optionsBarImage.height),
                            barX - barWidth * 0.5F +
                                barWidth * normalized * 0.5F,
                            rowY, 30.0F, transparent);
                    }
                }
                else
                {
                    const auto& value =
                        !rowEnabled
                            ? activePage.disabled[index]
                        : selectedRow
                            ? activePage.selected[index]
                            : activePage.normal[index];
                    drawQuad(
                        *device, quad, shader, value.texture,
                        value.width, value.height,
                        optionsCenterX + 260.0F, rowY, 12.0F,
                        transparent);
                }
                if (rowEnabled)
                {
                    const Texture arrow =
                        selectedRow ? optionsArrowSelected
                                    : optionsArrow;
                    drawQuad(
                        *device, quad, shader, arrow,
                        static_cast<float>(
                            selectedRow
                                ? optionsArrowSelectedImage.width
                                : optionsArrowImage.width),
                        static_cast<float>(
                            selectedRow
                                ? optionsArrowSelectedImage.height
                                : optionsArrowImage.height),
                        optionsCenterX + 140.0F, rowY, 20.0F,
                        transparent);
                    drawQuadRotated(
                        *device, quad, shader, arrow,
                        static_cast<float>(
                            selectedRow
                                ? optionsArrowSelectedImage.width
                                : optionsArrowImage.width),
                        static_cast<float>(
                            selectedRow
                                ? optionsArrowSelectedImage.height
                                : optionsArrowImage.height),
                        optionsCenterX + 380.0F, rowY, 20.0F,
                        bx::kPi, transparent);
                }
            }

            if (firstVisible > 0U)
            {
                drawQuadRotated(
                    *device, quad, shader, optionsArrowSelected,
                    30.0F, 30.0F, optionsCenterX + 150.0F,
                    optionsCenterY -
                        (menuStack.back() ==
                                 MenuScreen::ControlsOptions
                             ? 150.0F
                             : 200.0F),
                    20.0F, bx::kPiHalf, transparent);
            }
            if (firstVisible + visibleRows < rowCount)
            {
                drawQuadRotated(
                    *device, quad, shader, optionsArrowSelected,
                    30.0F, 30.0F, optionsCenterX + 150.0F,
                    optionsCenterY + 195.0F, 20.0F,
                    -bx::kPiHalf, transparent);
            }

            for (std::size_t action = 0U; action < 2U; ++action)
            {
                const std::size_t selectionIndex = rowCount + action;
                const bool selectedAction =
                    menuSelection == selectionIndex;
                const float buttonX =
                    optionsCenterX - 30.0F +
                    static_cast<float>(action) * 360.0F;
                const float buttonY = optionsCenterY + 240.0F;
                drawQuad(
                    *device, quad, shader,
                    selectedAction ? optionsButtonSelected
                                   : optionsButton,
                    static_cast<float>(
                        selectedAction
                            ? optionsButtonSelectedImage.width
                            : optionsButtonImage.width),
                    static_cast<float>(
                        selectedAction
                            ? optionsButtonSelectedImage.height
                            : optionsButtonImage.height),
                    buttonX +
                        static_cast<float>(
                            selectedAction
                                ? optionsButtonSelectedImage.width
                                : optionsButtonImage.width) *
                            0.5F,
                    buttonY, 40.0F, transparent);
                const auto& actionText =
                    selectedAction
                        ? optionsActionPage.selected[action]
                        : optionsActionPage.normal[action];
                drawQuad(
                    *device, quad, shader, actionText.texture,
                    actionText.width, actionText.height,
                    buttonX + 62.0F +
                        actionText.width * 0.5F,
                    buttonY, 15.0F, transparent);
            }
        }
        else if (drawingOriginalGamers)
        {
            gamersFrameObserved = true;
            drawQuad(
                *device, quad, shader, gamersSpace,
                static_cast<float>(gamersSpaceImage.width),
                static_cast<float>(gamersSpaceImage.height),
                menu::virtualWidth * 0.5F,
                menu::virtualHeight * 0.5F, 65.0F, opaque);

            constexpr float planetRadius =
                (menu::virtualHeight - 254.0F) * 0.5F;
            constexpr float planetX =
                menu::virtualWidth * 0.5F - 25.0F;
            constexpr float viewportSize =
                (planetRadius < 300.0F ? planetRadius : 300.0F) *
                3.1F;
            if (gamerPlanetIndex < originalGarage->gamers.size())
            {
                const auto& telemetry = device->renderTelemetry();
                const auto before = std::accumulate(
                    telemetry.drawCount.begin(),
                    telemetry.drawCount.end(), 0U);
                workshopRenderer.drawPlanet(
                    *device, raceShader,
                    originalGarage->gamers[gamerPlanetIndex],
                    planetX, planetRadius, viewportSize, viewportSize,
                    gamersSceneSeconds * bx::kPi / 24.0F,
                    racePipeline);
                const auto after = std::accumulate(
                    telemetry.drawCount.begin(),
                    telemetry.drawCount.end(), 0U);
                gamersPlanet3DObserved =
                    gamersPlanet3DObserved || after > before;
            }

            const float panelCenterY =
                menu::virtualHeight -
                static_cast<float>(gamersBottomPanelImage.height) *
                    0.5F;
            drawQuad(
                *device, quad, shader, gamersBottomPanel,
                static_cast<float>(gamersBottomPanelImage.width),
                static_cast<float>(gamersBottomPanelImage.height),
                menu::virtualWidth * 0.5F, panelCenterY,
                40.0F, transparent);

            if (gamerPlanetIndex < gamersBossTextures.size())
            {
                const auto& photo =
                    gamersBossImages[gamerPlanetIndex];
                const float photoScale = std::min(
                    {1.0F,
                     190.0F /
                         std::max(
                             static_cast<float>(photo.width), 1.0F),
                     190.0F /
                         std::max(
                             static_cast<float>(photo.height), 1.0F)});
                const float photoWidth =
                    static_cast<float>(photo.width) * photoScale;
                const float photoHeight =
                    static_cast<float>(photo.height) * photoScale;
                drawQuad(
                    *device, quad, shader,
                    gamersBossTextures[gamerPlanetIndex],
                    photoWidth, photoHeight,
                    menu::virtualWidth * 0.5F - 30.0F,
                    menu::virtualHeight - 18.0F -
                        photoHeight * 0.5F,
                    20.0F, transparent);
            }
            drawQuad(
                *device, quad, shader, gamersPhotoLight,
                static_cast<float>(gamersPhotoLightImage.width),
                static_cast<float>(gamersPhotoLightImage.height),
                menu::virtualWidth * 0.5F - 30.0F,
                menu::virtualHeight - 100.0F, 18.0F,
                transparent);

            if (!gamersNamePage.normal.empty())
            {
                const auto& name = gamersNamePage.normal.front();
                drawQuad(
                    *device, quad, shader, name.texture,
                    name.width, name.height,
                    menu::virtualWidth * 0.5F - 25.0F,
                    menu::virtualHeight -
                        static_cast<float>(
                            gamersBottomPanelImage.height) +
                        23.0F,
                    15.0F, transparent);
            }
            const float infoLeft =
                menu::virtualWidth * 0.5F - 390.0F - 237.5F;
            const float infoTop =
                menu::virtualHeight - 100.0F - 80.0F;
            for (std::size_t line = 0U;
                 line < gamersInfoPage.normal.size(); ++line)
            {
                const auto& text = gamersInfoPage.normal[line];
                drawQuad(
                    *device, quad, shader, text.texture,
                    text.width, text.height,
                    infoLeft + text.width * 0.5F,
                    infoTop + text.height * 0.5F +
                        static_cast<float>(line) * 21.0F,
                    15.0F, transparent);
            }
            if (!gamersBonusPage.normal.empty())
            {
                const auto& bonus = gamersBonusPage.normal.front();
                drawQuad(
                    *device, quad, shader, bonus.texture,
                    bonus.width, bonus.height,
                    menu::virtualWidth * 0.5F + 245.0F,
                    menu::virtualHeight - 105.0F,
                    15.0F, transparent);
            }

            const auto previous =
                adjacentGamerIndex(gamerPlanetIndex, -1);
            const auto next =
                adjacentGamerIndex(gamerPlanetIndex, 1);
            if (previous)
            {
                const bool focused =
                    gamersFocus == GamersFocus::Left;
                const auto& image =
                    focused ? garageArrowSelectedImage
                            : garageArrowImage;
                drawQuadRotated(
                    *device, quad, shader,
                    focused ? garageArrowSelected : garageArrow,
                    static_cast<float>(image.width),
                    static_cast<float>(image.height),
                    planetX - planetRadius - 40.0F + 3.0F -
                        static_cast<float>(image.width) * 0.5F,
                    planetRadius, 12.0F, 0.0F, transparent);
            }
            if (next)
            {
                const bool focused =
                    gamersFocus == GamersFocus::Right;
                const auto& image =
                    focused ? garageArrowSelectedImage
                            : garageArrowImage;
                drawQuadRotated(
                    *device, quad, shader,
                    focused ? garageArrowSelected : garageArrow,
                    static_cast<float>(image.width),
                    static_cast<float>(image.height),
                    planetX + planetRadius + 40.0F + 3.0F +
                        static_cast<float>(image.width) * 0.5F,
                    planetRadius, 12.0F, bx::kPi, transparent);
            }
            const bool nextFocused =
                gamersFocus == GamersFocus::Next;
            const auto& nextImage =
                nextFocused ? gamersNextArrowSelectedImage
                            : gamersNextArrowImage;
            drawQuad(
                *device, quad, shader,
                nextFocused ? gamersNextArrowSelected
                            : gamersNextArrow,
                static_cast<float>(nextImage.width),
                static_cast<float>(nextImage.height),
                menu::virtualWidth * 0.5F + 400.0F +
                    static_cast<float>(nextImage.width) * 0.5F,
                menu::virtualHeight - 100.0F, 12.0F,
                transparent);
        }
        else if (drawingOriginalRaceMenu)
        {
            raceMainFrameObserved = true;
            const float centerX = menu::virtualWidth * 0.5F;
            const float topCenterY =
                static_cast<float>(raceTopPanelImage.height) * 0.5F;
            const float bottomCenterY =
                menu::virtualHeight -
                static_cast<float>(raceBottomPanelImage.height) *
                    0.5F;
            drawQuad(
                *device, quad, shader, raceTopPanel,
                static_cast<float>(raceTopPanelImage.width),
                static_cast<float>(raceTopPanelImage.height),
                centerX, topCenterY, 60.0F, transparent);
            drawQuad(
                *device, quad, shader, raceBottomPanel,
                static_cast<float>(raceBottomPanelImage.width),
                static_cast<float>(raceBottomPanelImage.height),
                centerX, bottomCenterY, 60.0F, transparent);
            drawQuad(
                *device, quad, shader, raceStats,
                static_cast<float>(raceStatsImage.width),
                static_cast<float>(raceStatsImage.height),
                static_cast<float>(raceStatsImage.width) * 0.5F,
                menu::virtualHeight -
                    static_cast<float>(raceStatsImage.height) *
                        0.5F,
                45.0F, transparent);
            drawQuad(
                *device, quad, shader, raceMoney,
                static_cast<float>(raceMoneyImage.width),
                static_cast<float>(raceMoneyImage.height),
                menu::virtualWidth -
                    static_cast<float>(raceMoneyImage.width) *
                        0.5F,
                menu::virtualHeight -
                    static_cast<float>(raceMoneyImage.height) *
                        0.5F,
                45.0F, transparent);

#ifdef RRR3D_NETWORK
            for (std::size_t index = 0U;
                 index < networkRacePlayerVisuals.size(); ++index)
            {
                const auto& visual = networkRacePlayerVisuals[index];
                const auto layout = networkRacePlayerCenter(index);
                const float playerX = layout[0];
                const float playerY = layout[1];
                const float direction = layout[2];
                if (!visual.player.car.empty())
                {
                    workshopRenderer.drawCar(
                        *device, raceShader, visual.player.car,
                        playerX + 72.0F * direction, playerY,
                        130.0F, 130.0F,
                        garageSceneSeconds * bx::kPi * 0.5F,
                        racePipeline);
                }
                drawQuadRotated(
                    *device, quad, shader, networkPlayerFrame,
                    static_cast<float>(networkPlayerFrameImage.width),
                    static_cast<float>(networkPlayerFrameImage.height),
                    playerX, playerY, 35.0F,
                    direction < 0.0F ? bx::kPi : 0.0F, transparent);
                if (visual.photoIndex &&
                    *visual.photoIndex < gamersBossTextures.size())
                {
                    const auto photoIndex = *visual.photoIndex;
                    const auto& photo = gamersBossImages[photoIndex];
                    const float scale = std::min(
                        100.0F /
                            std::max(
                                static_cast<float>(photo.width), 1.0F),
                        97.0F /
                            std::max(
                                static_cast<float>(photo.height), 1.0F));
                    drawQuad(
                        *device, quad, shader,
                        gamersBossTextures[photoIndex],
                        static_cast<float>(photo.width) * scale,
                        static_cast<float>(photo.height) * scale,
                        playerX - 60.0F * direction, playerY,
                        30.0F, transparent);
                }
                const float nameScale = std::min(
                    1.0F, 190.0F /
                              std::max(visual.name.width, 1.0F));
                drawQuad(
                    *device, quad, shader, visual.name.texture,
                    visual.name.width * nameScale,
                    visual.name.height * nameScale,
                    playerX, playerY - 70.0F, 25.0F, transparent);
                const float readyScale = std::min(
                    1.0F, 210.0F /
                              std::max(visual.readyLabel.width, 1.0F));
                drawQuad(
                    *device, quad, shader, visual.readyLabel.texture,
                    visual.readyLabel.width * readyScale,
                    visual.readyLabel.height * readyScale,
                    playerX, playerY + 68.0F, 25.0F, transparent);
                if (visual.player.ownerId !=
                    r3d::game::originalnetwork::serverOwnerId)
                {
                    const auto readyTexture =
                        visual.player.raceReady
                            ? networkPlayerReadySelected
                            : networkPlayerReady;
                    const auto& readyImage =
                        visual.player.raceReady
                            ? networkPlayerReadySelectedImage
                            : networkPlayerReadyImage;
                    drawQuad(
                        *device, quad, shader, readyTexture,
                        static_cast<float>(readyImage.width),
                        static_cast<float>(readyImage.height),
                        playerX + 128.0F * direction,
                        playerY + 68.0F, 24.0F, transparent);
                }
                if (networkHostRequested)
                {
                    const bool hovered =
                        networkKickHoverOwner &&
                        *networkKickHoverOwner == visual.player.ownerId;
                    const auto kickTexture =
                        hovered ? networkPlayerKickSelected
                                : networkPlayerKick;
                    const auto& kickImage =
                        hovered ? networkPlayerKickSelectedImage
                                : networkPlayerKickImage;
                    drawQuad(
                        *device, quad, shader, kickTexture,
                        static_cast<float>(kickImage.width),
                        static_cast<float>(kickImage.height),
                        playerX + 128.0F * direction,
                        playerY - 68.0F, 24.0F, transparent);
                }
            }
#endif

            constexpr std::array<float, 5> headerOffsets{
                -548.0F, -273.0F, 2.0F, 277.0F, 552.0F};
            for (std::size_t index = 0U;
                 index < raceMainHeadersPage.normal.size(); ++index)
            {
                const auto& text =
                    raceMainHeadersPage.normal[index];
                drawQuad(
                    *device, quad, shader, text.texture,
                    text.width, text.height,
                    centerX + headerOffsets[index],
                    topCenterY + 18.0F, 20.0F, transparent);
            }

            bool sourcePortraitsDrawn = false;
            bool sourceBossCarDrawn = false;
            bool sourceLoadoutDrawn = false;
            const float frameWidth = static_cast<float>(
                raceImageFrameImage.width);
            const float frameHeight = static_cast<float>(
                raceImageFrameImage.height);
            auto drawFittedPortrait =
                [&](const menu::Image& image, Texture texture,
                    float x) {
                    const float scale = std::min(
                        frameWidth /
                            std::max(
                                static_cast<float>(image.width), 1.0F),
                        frameHeight /
                            std::max(
                                static_cast<float>(image.height), 1.0F));
                    drawQuad(
                        *device, quad, shader, texture,
                        static_cast<float>(image.width) * scale,
                        static_cast<float>(image.height) * scale,
                        x, topCenterY + 87.0F, 40.0F,
                        transparent);
                };
            const auto playerGamer = std::find_if(
                originalGarage->gamers.begin(),
                originalGarage->gamers.end(),
                [&](const auto& gamer) {
                    return gamer.bossId ==
                           profileState.player.gamerId;
                });
            const auto planetIndex =
                profileState.player.currentPlanet;
            if (playerGamer != originalGarage->gamers.end() &&
                planetIndex < originalGarage->planets.size())
            {
                const auto gamerIndex = static_cast<std::size_t>(
                    std::distance(
                        originalGarage->gamers.begin(), playerGamer));
                if (gamerIndex < gamersBossTextures.size() &&
                    planetIndex < angarBossTextures.size())
                {
                    drawFittedPortrait(
                        gamersBossImages[gamerIndex],
                        gamersBossTextures[gamerIndex],
                        centerX - 550.0F);
                    drawFittedPortrait(
                        angarBossImages[planetIndex],
                        angarBossTextures[planetIndex],
                        centerX + 475.0F);
                    sourcePortraitsDrawn = true;
                }
                const auto& bossCar =
                    originalGarage->planets[planetIndex]
                        .bossCarRecord;
                if (!bossCar.empty())
                {
                    const auto& telemetry =
                        device->renderTelemetry();
                    const auto before = std::accumulate(
                        telemetry.drawCount.begin(),
                        telemetry.drawCount.end(), 0U);
                    const float viewportSize =
                        std::max(frameWidth, frameHeight);
                    workshopRenderer.drawCar(
                        *device, raceShader, bossCar,
                        centerX + 605.0F,
                        topCenterY + 87.0F,
                        viewportSize, viewportSize,
                        garageSceneSeconds * bx::kPi * 0.5F,
                        racePipeline);
                    const auto after = std::accumulate(
                        telemetry.drawCount.begin(),
                        telemetry.drawCount.end(), 0U);
                    sourceBossCarDrawn = after > before;
                }
            }

            constexpr std::array<float, 3> frameOffsets{
                -550.0F, 475.0F, 605.0F};
            for (const float offset : frameOffsets)
            {
                drawQuad(
                    *device, quad, shader, raceImageFrame,
                    static_cast<float>(raceImageFrameImage.width),
                    static_cast<float>(raceImageFrameImage.height),
                    centerX + offset, topCenterY + 87.0F,
                    35.0F, transparent);
            }
            const auto& passInfo = raceMainInfoPage.normal[0];
            const auto& tournamentInfo =
                raceMainInfoPage.normal[1];
            const auto& money = raceMainInfoPage.normal[2];
            auto drawInfoLeft =
                [&](const TextVisual& text, float x, float y,
                    float maximumWidth) {
                    const float scale = std::min(
                        1.0F,
                        maximumWidth /
                            std::max(text.width, 1.0F));
                    drawQuad(
                        *device, quad, shader, text.texture,
                        text.width * scale,
                        text.height * scale,
                        x + text.width * scale * 0.5F, y,
                        15.0F, transparent);
                };
            drawInfoLeft(
                passInfo, centerX - 377.0F,
                topCenterY + 86.0F, 260.0F);
            drawInfoLeft(
                tournamentInfo, centerX - 102.0F,
                topCenterY + 86.0F, 260.0F);
            drawQuad(
                *device, quad, shader, money.texture,
                money.width, money.height,
                menu::virtualWidth - 53.0F -
                    money.width * 0.5F,
                menu::virtualHeight - 29.0F, 15.0F,
                transparent);

            std::size_t weatherIndex = 0U;
            using Weather =
                r3d::game::originalrace::Weather;
            if (originalRace->environment.weather == Weather::Night)
                weatherIndex = 1U;
            else if (
                originalRace->environment.weather ==
                    Weather::Cloudy ||
                originalRace->environment.weather == Weather::Hell)
                weatherIndex = 2U;
            else if (
                originalRace->environment.weather ==
                Weather::Rainy)
                weatherIndex = 3U;
            drawQuad(
                *device, quad, shader,
                raceWeatherIcons[weatherIndex],
                static_cast<float>(
                    raceWeatherImages[weatherIndex].width),
                static_cast<float>(
                    raceWeatherImages[weatherIndex].height),
                centerX - 282.0F, topCenterY + 112.0F,
                15.0F, transparent);

            constexpr std::array<std::size_t, 6> sourceChargeSlots{
                6U, 7U, 8U, 9U, 4U, 5U};
            const std::size_t visibleChargeCount =
                static_cast<std::size_t>(std::count_if(
                    sourceChargeSlots.begin(),
                    sourceChargeSlots.end(),
                    [&](std::size_t slotIndex) {
                        return slotIndex <
                                   profileState.player.slots.size() &&
                               !profileState.player.slots[slotIndex]
                                    .record.empty();
                    }));
            std::size_t visibleCharge = 0U;
            const float chargeStride =
                static_cast<float>(raceChargeBarImage.width) +
                40.0F;
            const float chargeWidth =
                visibleChargeCount > 1U
                    ? static_cast<float>(visibleChargeCount - 1U) *
                          chargeStride
                    : 0.0F;
            const auto& loadoutTelemetry =
                device->renderTelemetry();
            const auto loadoutDrawsBefore = std::accumulate(
                loadoutTelemetry.drawCount.begin(),
                loadoutTelemetry.drawCount.end(), 0U);
            for (const auto slotIndex : sourceChargeSlots)
            {
                if (slotIndex >= profileState.player.slots.size())
                    continue;
                const auto& slot =
                    profileState.player.slots[slotIndex];
                if (slot.record.empty())
                    continue;
                const float progress = std::clamp(
                    static_cast<float>(
                        slot.hasCharge ? slot.charge : 0U) /
                        7.0F,
                    0.0F, 1.0F);
                const float chargeX =
                    centerX + 257.0F +
                    static_cast<float>(raceChargeBarImage.width) *
                        0.5F -
                    chargeWidth * 0.5F +
                    static_cast<float>(visibleCharge) *
                        chargeStride;
                const float fullHeight =
                    static_cast<float>(raceChargeBarImage.height);
                if (progress > 0.0F)
                {
                    drawQuad(
                        *device, quad, shader, raceChargeBar,
                        static_cast<float>(
                            raceChargeBarImage.width),
                        fullHeight * progress, chargeX,
                        topCenterY + 120.0F +
                            fullHeight * (1.0F - progress) *
                                0.5F,
                        18.0F, transparent);
                }
                if (const auto* item =
                        originalGarage->findItem(slot.record))
                {
                    workshopRenderer.drawItem(
                        *device, raceShader, *item,
                        chargeX, topCenterY + 65.0F,
                        50.0F, 50.0F,
                        garageSceneSeconds * bx::kPi * 0.5F,
                        racePipeline);
                }
                ++visibleCharge;
            }
            const auto loadoutDrawsAfter = std::accumulate(
                loadoutTelemetry.drawCount.begin(),
                loadoutTelemetry.drawCount.end(), 0U);
            sourceLoadoutDrawn =
                visibleChargeCount == 0U ||
                loadoutDrawsAfter > loadoutDrawsBefore;

            const auto* currentCar = originalGarage->findCar(
                profileState.player.currentCar);
            const auto stats =
                currentCar != nullptr
                    ? r3d::game::originalrace::originalGarageStats(
                          *originalGarage, *currentCar,
                          profileState.player)
                    : r3d::game::originalrace::
                          OriginalGarageStats{};
            const std::array<float, 3> statProgress{
                stats.damageProgress, stats.armorProgress,
                stats.speedProgress};
            constexpr std::array<float, 3> statOffsetX{
                128.0F, 150.0F, 173.0F};
            constexpr std::array<float, 3> statOffsetY{
                -98.0F, -61.0F, -23.0F};
            const float statsCenterX =
                static_cast<float>(raceStatsImage.width) * 0.5F;
            const float statsCenterY =
                menu::virtualHeight -
                static_cast<float>(raceStatsImage.height) * 0.5F;
            for (std::size_t index = 0U;
                 index < statProgress.size(); ++index)
            {
                const float progress = std::clamp(
                    statProgress[index], 0.0F, 1.0F);
                const float barWidth =
                    static_cast<float>(raceStatBarImage.width);
                const float barCenterX =
                    statsCenterX + statOffsetX[index];
                const float barCenterY =
                    statsCenterY + statOffsetY[index];
                if (progress > 0.0F)
                {
                    drawQuad(
                        *device, quad, shader, raceStatBar,
                        barWidth * progress,
                        static_cast<float>(raceStatBarImage.height),
                        barCenterX - barWidth * 0.5F +
                            barWidth * progress * 0.5F,
                        barCenterY, 30.0F, transparent);
                }
                const auto& value =
                    raceMainStatsPage.normal[index];
                drawQuad(
                    *device, quad, shader, value.texture,
                    value.width, value.height,
                    barCenterX + barWidth * 0.5F - 15.0F -
                        value.width * 0.5F,
                    barCenterY, 15.0F, transparent);
            }
            raceMainSourceVisualsObserved =
                raceMainSourceVisualsObserved ||
                (sourcePortraitsDrawn && sourceBossCarDrawn &&
                 sourceLoadoutDrawn);

            constexpr float itemWidth = 110.0F;
            constexpr float itemSpacing = 50.0F;
            const float firstX =
                centerX -
                (7.0F * itemWidth + 6.0F * itemSpacing) *
                    0.5F +
                itemWidth * 0.5F;
            const float itemY = bottomCenterY - 72.0F;
            for (std::size_t index = 0U;
                 index < raceMenuIcons.size(); ++index)
            {
                const bool selectedItem = index == menuSelection;
                const bool enabled =
                    index < raceMenuPage.enabled.size() &&
                    raceMenuPage.enabled[index];
                const float itemX =
                    firstX +
                    static_cast<float>(index) *
                        (itemWidth + itemSpacing);
                drawQuad(
                    *device, quad, shader,
                    selectedItem ? raceMenuButtonSelected
                                 : raceMenuButton,
                    static_cast<float>(
                        selectedItem
                            ? raceMenuButtonSelectedImage.width
                            : raceMenuButtonImage.width),
                    static_cast<float>(
                        selectedItem
                            ? raceMenuButtonSelectedImage.height
                            : raceMenuButtonImage.height),
                    itemX, itemY, 35.0F, transparent);
                if (enabled)
                {
                    drawQuad(
                        *device, quad, shader, raceMenuIcons[index],
                        static_cast<float>(
                            raceMenuIconImages[index].width),
                        static_cast<float>(
                            raceMenuIconImages[index].height),
                        itemX, itemY, 20.0F, transparent);
                }
                else
                {
                    drawQuadTinted(
                        *device, quad, shader, raceMenuIcons[index],
                        static_cast<float>(
                            raceMenuIconImages[index].width),
                        static_cast<float>(
                            raceMenuIconImages[index].height),
                        itemX, itemY, 20.0F, transparent,
                        {1.0F, 1.0F, 1.0F, 0.25F});
                }
            }
        }
        else if (drawingOriginalGarage)
        {
            raceGarageFrameObserved = true;
            if (options->gamersFrameSmokeTest)
                gamersGarageObserved = true;
            const float centerX = menu::virtualWidth * 0.5F;
            const float centerY = menu::virtualHeight * 0.5F;
            const float topCenterY =
                static_cast<float>(garageTopPanelImage.height) *
                0.5F;
            const float bottomCenterY =
                menu::virtualHeight -
                static_cast<float>(
                    garageBottomPanelImage.height) *
                    0.5F;
            const float sideCenterY =
                (static_cast<float>(garageTopPanelImage.height) +
                 menu::virtualHeight -
                 static_cast<float>(
                     garageBottomPanelImage.height)) *
                0.5F;
            drawQuad(
                *device, quad, shader, garageTopPanel,
                static_cast<float>(garageTopPanelImage.width),
                static_cast<float>(garageTopPanelImage.height),
                centerX, topCenterY, 60.0F, transparent);
            drawQuad(
                *device, quad, shader, garageBottomPanel,
                static_cast<float>(garageBottomPanelImage.width),
                static_cast<float>(
                    garageBottomPanelImage.height),
                centerX, bottomCenterY, 60.0F, transparent);
            drawQuadRotated(
                *device, quad, shader, garageSidePanel,
                static_cast<float>(garageSidePanelImage.width),
                static_cast<float>(garageSidePanelImage.height),
                static_cast<float>(garageSidePanelImage.width) *
                    0.5F,
                sideCenterY, 55.0F, bx::kPi, transparent);
            drawQuad(
                *device, quad, shader, garageSidePanel,
                static_cast<float>(garageSidePanelImage.width),
                static_cast<float>(garageSidePanelImage.height),
                menu::virtualWidth -
                    static_cast<float>(
                        garageSidePanelImage.width) *
                        0.5F,
                sideCenterY, 55.0F, transparent);

            drawQuad(
                *device, quad, shader, garageMoney,
                static_cast<float>(garageMoneyImage.width),
                static_cast<float>(garageMoneyImage.height),
                menu::virtualWidth -
                    static_cast<float>(garageMoneyImage.width) *
                        0.5F,
                menu::virtualHeight -
                    static_cast<float>(
                        garageMoneyImage.height) *
                        0.5F,
                42.0F, transparent);
            constexpr float statsLeft = 418.0F;
            constexpr float statsTop = 889.0F + 60.0F;
            drawQuad(
                *device, quad, shader, garageStats,
                static_cast<float>(garageStatsImage.width),
                static_cast<float>(garageStatsImage.height),
                statsLeft +
                    static_cast<float>(garageStatsImage.width) *
                        0.5F,
                statsTop +
                    static_cast<float>(garageStatsImage.height) *
                        0.5F,
                42.0F, transparent);

            const auto& selectedCar =
                originalGarage->cars[garageCarIndex];
            const bool selectedLocked =
                garageCarOrder[garageViewIndex].locked;
            const auto selectedStats =
                r3d::game::originalrace::originalGarageStats(
                    *originalGarage, selectedCar);
            const std::array<float, 3> statProgress{
                selectedLocked ? 0.0F
                               : selectedStats.damageProgress,
                selectedLocked ? 0.0F
                               : selectedStats.armorProgress,
                selectedLocked ? 0.0F
                               : selectedStats.speedProgress};
            constexpr std::array<float, 3> statOffsetY{
                3.0F, 41.0F, 78.0F};
            for (std::size_t index = 0U;
                 index < statProgress.size(); ++index)
            {
                const float progress = std::clamp(
                    statProgress[index], 0.0F, 1.0F);
                if (progress > 0.0F)
                {
                    const float fullWidth = static_cast<float>(
                        garageStatBarImage.width);
                    drawQuad(
                        *device, quad, shader, garageStatBar,
                        fullWidth * progress,
                        static_cast<float>(
                            garageStatBarImage.height),
                        statsLeft + 48.0F +
                            fullWidth * progress * 0.5F,
                        statsTop + statOffsetY[index] +
                            static_cast<float>(
                                garageStatBarImage.height) *
                                0.5F,
                        30.0F, transparent);
                }
                const auto& value =
                    garageStatsPage.normal[index];
                drawQuad(
                    *device, quad, shader, value.texture,
                    value.width, value.height,
                    statsLeft + 48.0F +
                        static_cast<float>(
                            garageStatBarImage.width) -
                        value.width * 0.5F - 4.0F,
                    statsTop + statOffsetY[index] + 13.0F,
                    15.0F, transparent);
            }

            const std::size_t visibleCars = std::max<std::size_t>(
                1U,
                static_cast<std::size_t>(
                    (menu::virtualWidth - 10.0F) /
                    static_cast<float>(
                        garageCarBoxImage.width)));
            std::size_t firstVisible = 0U;
            if (garageCarOrder.size() > visibleCars)
            {
                const std::size_t right = std::min(
                    garageViewIndex + visibleCars / 2U,
                    garageCarOrder.size() - 1U);
                firstVisible =
                    right + 1U > visibleCars
                        ? right + 1U - visibleCars
                        : 0U;
                firstVisible = std::min(
                    firstVisible,
                    garageCarOrder.size() - visibleCars);
            }
            const std::size_t visibleEnd = std::min(
                firstVisible + visibleCars,
                garageCarOrder.size());
            const float visibleWidth =
                static_cast<float>(
                    visibleEnd - firstVisible) *
                static_cast<float>(garageCarBoxImage.width);
            const float firstCarX =
                (menu::virtualWidth - visibleWidth) * 0.5F +
                static_cast<float>(garageCarBoxImage.width) *
                    0.5F;
            for (std::size_t view = firstVisible;
                 view < visibleEnd; ++view)
            {
                const auto& carView = garageCarOrder[view];
                const bool selected = view == garageViewIndex;
                const float x =
                    firstCarX +
                    static_cast<float>(view - firstVisible) *
                        static_cast<float>(
                            garageCarBoxImage.width);
                drawQuad(
                    *device, quad, shader,
                    selected ? garageCarBoxSelected
                             : garageCarBox,
                    static_cast<float>(
                        selected
                            ? garageCarBoxSelectedImage.width
                            : garageCarBoxImage.width),
                    static_cast<float>(
                        selected
                            ? garageCarBoxSelectedImage.height
                            : garageCarBoxImage.height),
                    x, 71.5F, 35.0F, transparent);
                if (carView.locked)
                {
                    drawQuad(
                        *device, quad, shader, garageLock,
                        static_cast<float>(garageLockImage.width),
                        static_cast<float>(
                            garageLockImage.height),
                        x, 71.5F, 20.0F, transparent);
                }
                else
                {
                    const auto& image =
                        garageCarImages[carView.catalogIndex];
                    drawQuad(
                        *device, quad, shader,
                        garageCarTextures[
                            carView.catalogIndex],
                        static_cast<float>(image.width),
                        static_cast<float>(image.height),
                        x, 71.5F, 20.0F, transparent);
                }
            }

            const bool leftArrowVisible = garageViewIndex > 0U;
            const bool rightArrowVisible =
                garageViewIndex + 1U < garageCarOrder.size();
            if (leftArrowVisible)
            {
                const bool selected = menuSelection == 2U;
                drawQuad(
                    *device, quad, shader,
                    selected ? garageArrowSelected
                             : garageArrow,
                    static_cast<float>(
                        selected
                            ? garageArrowSelectedImage.width
                            : garageArrowImage.width),
                    static_cast<float>(
                        selected
                            ? garageArrowSelectedImage.height
                            : garageArrowImage.height),
                    181.0F, centerY, 35.0F, transparent);
            }
            if (rightArrowVisible)
            {
                const bool selected = menuSelection == 3U;
                drawQuadRotated(
                    *device, quad, shader,
                    selected ? garageArrowSelected
                             : garageArrow,
                    static_cast<float>(
                        selected
                            ? garageArrowSelectedImage.width
                            : garageArrowImage.width),
                    static_cast<float>(
                        selected
                            ? garageArrowSelectedImage.height
                            : garageArrowImage.height),
                    menu::virtualWidth - 181.0F, centerY,
                    35.0F, bx::kPi, transparent);
            }

            const float firstColorY = centerY - 144.0F;
            for (std::size_t side = 0U; side < 2U; ++side)
            {
                const float colorX =
                    side == 0U
                        ? static_cast<float>(
                              garageSidePanelImage.width) *
                              0.5F
                        : menu::virtualWidth -
                              static_cast<float>(
                                  garageSidePanelImage.width) *
                                  0.5F;
                for (std::size_t index = 0U; index < 7U;
                     ++index)
                {
                    const std::size_t colorIndex =
                        side * 7U + index;
                    const std::size_t focusIndex =
                        4U + colorIndex;
                    if (!garageColorAvailable(colorIndex))
                        continue;
                    const bool focused =
                        menuSelection == focusIndex;
                    bool activeColor = true;
                    for (std::size_t component = 0U;
                         component < 4U; ++component)
                    {
                        const int profileComponent =
                            static_cast<int>(std::lround(
                                std::clamp(
                                    profileState.player
                                        .color[component],
                                    0.0F, 1.0F) *
                                255.0F));
                        activeColor =
                            activeColor &&
                            profileComponent ==
                                garageColorPixels[colorIndex]
                                                  [component];
                    }
                    const float colorY =
                        firstColorY +
                        static_cast<float>(index) * 48.0F;
                    drawQuad(
                        *device, quad, shader,
                        focused || activeColor
                            ? garageColorBoxSelected
                            : garageColorBoxBackground,
                        static_cast<float>(
                            focused || activeColor
                                ? garageColorBoxSelectedImage.width
                                : garageColorBoxBackgroundImage.width),
                        static_cast<float>(
                            focused || activeColor
                                ? garageColorBoxSelectedImage.height
                                : garageColorBoxBackgroundImage.height),
                        colorX, colorY, 34.0F, transparent);
                    drawQuad(
                        *device, quad, shader,
                        garageColorTextures[colorIndex],
                        static_cast<float>(
                            garageColorBoxImage.width),
                        static_cast<float>(
                            garageColorBoxImage.height),
                        colorX, colorY, 20.0F, transparent);
                    drawQuad(
                        *device, quad, shader, garageColorBox,
                        static_cast<float>(
                            garageColorBoxImage.width),
                        static_cast<float>(
                            garageColorBoxImage.height),
                        colorX, colorY, 18.0F, transparent);
                }
            }

            const float carNameY = bottomCenterY - 155.0F;
            const auto& carName = garagePage.normal[0];
            drawQuad(
                *device, quad, shader, carName.texture,
                carName.width, carName.height, centerX + 10.0F,
                carNameY, 18.0F, transparent);
            for (std::size_t line = 0U;
                 line < garageInfoPage.normal.size(); ++line)
            {
                const auto& text = garageInfoPage.normal[line];
                drawQuad(
                    *device, quad, shader, text.texture,
                    text.width, text.height, centerX,
                    bottomCenterY - 76.0F +
                        static_cast<float>(line) * 20.0F,
                    18.0F, transparent);
            }
            const auto& money = garagePage.normal[1];
            drawQuad(
                *device, quad, shader, money.texture,
                money.width, money.height,
                menu::virtualWidth - 53.0F -
                    money.width * 0.5F,
                menu::virtualHeight - 29.0F, 15.0F,
                transparent);
            const auto& price = garagePage.normal[2];
            drawQuad(
                *device, quad, shader, price.texture,
                price.width, price.height, centerX - 523.0F,
                bottomCenterY - 80.0F, 15.0F, transparent);

            const bool backFocused = menuSelection == 0U;
            const float backX =
                static_cast<float>(garageBackImage.width) * 0.5F;
            const float backY =
                menu::virtualHeight -
                static_cast<float>(
                    garageBottomPanelImage.height) +
                14.0F;
            drawQuad(
                *device, quad, shader,
                backFocused ? garageBackSelected : garageBack,
                static_cast<float>(
                    backFocused
                        ? garageBackSelectedImage.width
                        : garageBackImage.width),
                static_cast<float>(
                    backFocused
                        ? garageBackSelectedImage.height
                        : garageBackImage.height),
                backX, backY, 35.0F, transparent);
            const auto& backText =
                backFocused ? garagePage.selected[3]
                            : garagePage.normal[3];
            drawQuad(
                *device, quad, shader, backText.texture,
                backText.width, backText.height, backX, backY,
                15.0F, transparent);

            const bool buyFocused = menuSelection == 1U;
            const float buyX = centerX + 15.0F;
            const float buyY =
                menu::virtualHeight -
                static_cast<float>(
                    garageBottomPanelImage.height) +
                static_cast<float>(garageBuyImage.height) *
                    0.5F;
            drawQuad(
                *device, quad, shader,
                buyFocused && !selectedLocked
                    ? garageBuySelected
                    : garageBuy,
                static_cast<float>(
                    buyFocused && !selectedLocked
                        ? garageBuySelectedImage.width
                        : garageBuyImage.width),
                static_cast<float>(
                    buyFocused && !selectedLocked
                        ? garageBuySelectedImage.height
                        : garageBuyImage.height),
                buyX, buyY, 35.0F, transparent);

        }
        else if (drawingOriginalWorkshop)
        {
            raceWorkshopFrameObserved = true;
            const float centerX = menu::virtualWidth * 0.5F;
            const float topCenterY =
                static_cast<float>(
                    workshopTopPanelImage.height) *
                0.5F;
            const float bottomCenterY =
                menu::virtualHeight -
                static_cast<float>(
                    workshopBottomPanelImage.height) *
                    0.5F;
            const float leftPanelCenterY =
                (static_cast<float>(
                     workshopTopPanelImage.height) -
                     30.0F +
                 menu::virtualHeight -
                 static_cast<float>(
                     workshopBottomPanelImage.height)) *
                0.5F;
            const float leftPanelCenterX =
                30.0F +
                static_cast<float>(
                    workshopLeftPanelImage.width) *
                    0.5F;
            drawQuad(
                *device, quad, shader, workshopTopPanel,
                static_cast<float>(
                    workshopTopPanelImage.width),
                static_cast<float>(
                    workshopTopPanelImage.height),
                centerX, topCenterY, 60.0F, transparent);
            drawQuad(
                *device, quad, shader, workshopBottomPanel,
                static_cast<float>(
                    workshopBottomPanelImage.width),
                static_cast<float>(
                    workshopBottomPanelImage.height),
                centerX, bottomCenterY, 60.0F, transparent);
            drawQuad(
                *device, quad, shader, workshopLeftPanel,
                static_cast<float>(
                    workshopLeftPanelImage.width),
                static_cast<float>(
                    workshopLeftPanelImage.height),
                leftPanelCenterX, leftPanelCenterY, 55.0F,
                transparent);

            drawQuad(
                *device, quad, shader, garageMoney,
                static_cast<float>(garageMoneyImage.width),
                static_cast<float>(garageMoneyImage.height),
                menu::virtualWidth -
                    static_cast<float>(
                        garageMoneyImage.width) *
                        0.5F,
                menu::virtualHeight -
                    static_cast<float>(
                        garageMoneyImage.height) *
                        0.5F,
                42.0F, transparent);
            const float statsLeft =
                menu::virtualWidth * 0.5F -
                static_cast<float>(
                    garageStatsImage.width) -
                15.0F;
            const float statsTop =
                menu::virtualHeight -
                static_cast<float>(
                    workshopBottomPanelImage.height) +
                25.0F;
            drawQuad(
                *device, quad, shader, garageStats,
                static_cast<float>(garageStatsImage.width),
                static_cast<float>(garageStatsImage.height),
                statsLeft +
                    static_cast<float>(
                        garageStatsImage.width) *
                        0.5F,
                statsTop +
                    static_cast<float>(
                        garageStatsImage.height) *
                        0.5F,
                42.0F, transparent);

            const auto* currentCar =
                originalGarage->findCar(
                    profileState.player.currentCar);
            const auto baseStats =
                currentCar == nullptr
                    ? r3d::game::originalrace::
                          OriginalGarageStats{}
                    : r3d::game::originalrace::
                          originalGarageStats(
                              *originalGarage, *currentCar,
                              profileState.player);
            const r3d::game::originalrace::
                OriginalWorkshopItem* previewItem = nullptr;
            r3d::game::originalrace::ProfileSlot previewProfileSlot;
            constexpr std::size_t firstGoodFocus = 1U;
            constexpr std::size_t firstSlotFocus = 13U;
            if (workshopDrag.active())
            {
                previewItem = originalGarage->findItem(
                    workshopDrag.item.record);
                previewProfileSlot = workshopDrag.item;
            }
            else if (
                menuSelection >= firstGoodFocus &&
                menuSelection < firstSlotFocus)
            {
                const std::size_t itemIndex =
                    workshopGoodScroll * 3U +
                    menuSelection - firstGoodFocus;
                if (itemIndex < workshopGoods.size())
                {
                    previewItem = workshopGoods[itemIndex];
                    previewProfileSlot = {
                        previewItem->record,
                        previewItem->defaultCharge,
                        previewItem->maximumCharge > 0U};
                }
            }
            else if (
                currentCar != nullptr &&
                menuSelection >= firstSlotFocus &&
                menuSelection < firstSlotFocus + 4U)
            {
                const std::size_t slotIndex =
                    menuSelection - firstSlotFocus;
                const auto slotType =
                    static_cast<r3d::game::originalrace::
                                    GarageSlotType>(slotIndex);
                const int level =
                    r3d::game::originalrace::
                        originalWorkshopUpgradeLevel(
                            profileState.player
                                .slots[slotIndex]
                                .record,
                            slotType);
                previewItem =
                    r3d::game::originalrace::
                        originalWorkshopUpgradeItem(
                            *originalGarage, *currentCar,
                            slotType, level + 1);
                if (previewItem != nullptr)
                {
                    previewProfileSlot = {
                        previewItem->record,
                        previewItem->defaultCharge,
                        previewItem->maximumCharge > 0U};
                }
            }

            std::optional<std::size_t> previewSlot;
            float leastInstalledDamage =
                std::numeric_limits<float>::max();
            if (previewItem != nullptr &&
                !previewProfileSlot.record.empty())
            {
                for (std::size_t slot = 0U;
                     slot <
                     profileState.player.slots.size();
                     ++slot)
                {
                    if (!workshopSlotAccepts(
                            slot, previewProfileSlot))
                        continue;
                    const auto* installed =
                        originalGarage->findItem(
                            profileState.player
                                .slots[slot]
                                .record);
                    const float installedDamage =
                        installed == nullptr
                            ? 0.0F
                            : installed->projectileDamage;
                    if (!previewSlot ||
                        installedDamage <
                            leastInstalledDamage)
                    {
                        previewSlot = slot;
                        leastInstalledDamage =
                            installedDamage;
                    }
                    if (installed == nullptr)
                        break;
                }
            }

            auto bonusStats = baseStats;
            if (currentCar != nullptr && previewSlot)
            {
                auto previewPlayer = profileState.player;
                previewPlayer.slots[*previewSlot] =
                    previewProfileSlot;
                bonusStats =
                    r3d::game::originalrace::
                        originalGarageStats(
                            *originalGarage, *currentCar,
                            previewPlayer);
            }
            const std::array<float, 3> baseProgress{
                baseStats.damageProgress,
                baseStats.armorProgress,
                baseStats.speedProgress};
            const std::array<float, 3> bonusProgress{
                bonusStats.damageProgress,
                bonusStats.armorProgress,
                bonusStats.speedProgress};
            constexpr std::array<float, 3> statOffsetY{
                3.0F, 41.0F, 78.0F};
            const float fullStatWidth =
                static_cast<float>(
                    garageStatBarImage.width);
            for (std::size_t index = 0U;
                 index < baseProgress.size(); ++index)
            {
                const float preview = std::clamp(
                    bonusProgress[index], 0.0F, 1.0F);
                if (preview > 0.0F)
                {
                    drawQuad(
                        *device, quad, shader,
                        workshopStatBarPlus,
                        fullStatWidth * preview,
                        static_cast<float>(
                            garageStatBarImage.height),
                        statsLeft + 48.0F +
                            fullStatWidth * preview * 0.5F,
                        statsTop + statOffsetY[index] +
                            static_cast<float>(
                                garageStatBarImage.height) *
                                0.5F,
                        32.0F, transparent);
                }
                const float installed = std::clamp(
                    baseProgress[index], 0.0F, 1.0F);
                if (installed > 0.0F)
                {
                    drawQuad(
                        *device, quad, shader,
                        garageStatBar,
                        fullStatWidth * installed,
                        static_cast<float>(
                            garageStatBarImage.height),
                        statsLeft + 48.0F +
                            fullStatWidth * installed *
                                0.5F,
                        statsTop + statOffsetY[index] +
                            static_cast<float>(
                                garageStatBarImage.height) *
                                0.5F,
                        30.0F, transparent);
                }
                if (index <
                    workshopStatsPage.normal.size())
                {
                    const auto& value =
                        workshopStatsPage.normal[index];
                    drawQuad(
                        *device, quad, shader,
                        value.texture, value.width,
                        value.height,
                        statsLeft + 48.0F +
                            fullStatWidth -
                            value.width * 0.5F - 4.0F,
                        statsTop + statOffsetY[index] +
                            13.0F,
                        15.0F, transparent);
                }
            }

            const auto goodCenters =
                workshopGoodCenters();
            for (std::size_t visible = 0U;
                 visible < goodCenters.size(); ++visible)
            {
                const std::size_t itemIndex =
                    workshopGoodScroll * 3U + visible;
                drawQuad(
                    *device, quad, shader,
                    workshopSlotFrame,
                    static_cast<float>(
                        workshopSlotFrameImage.width),
                    static_cast<float>(
                        workshopSlotFrameImage.height),
                    goodCenters[visible][0],
                    goodCenters[visible][1], 35.0F,
                    transparent);
                if (itemIndex >= workshopGoods.size())
                    continue;
                float itemRotation =
                    2.0F * bx::kPi *
                    static_cast<float>(itemIndex) / 12.0F;
                if (menuSelection == 1U + visible)
                {
                    itemRotation +=
                        garageSceneSeconds * bx::kPi * 0.5F;
                }
                workshopRenderer.drawItem(
                    *device, raceShader,
                    *workshopGoods[itemIndex],
                    goodCenters[visible][0],
                    goodCenters[visible][1],
                    static_cast<float>(
                        workshopSlotFrameImage.width),
                    static_cast<float>(
                        workshopSlotFrameImage.height),
                    itemRotation, racePipeline);
            }

            const auto slotCenters =
                workshopSlotCenters();
            if (currentCar != nullptr)
            {
                for (std::size_t slot = 0U;
                     slot < slotCenters.size(); ++slot)
                {
                    const auto& placement =
                        currentCar->placements[slot];
                    const bool enabled =
                        placement.active &&
                        (championshipMode ||
                         slot <
                             r3d::game::originalrace::
                                 PlayerProfile::
                                     firstWeaponSlot ||
                         slot -
                                 r3d::game::originalrace::
                                     PlayerProfile::
                                         firstWeaponSlot <
                             profileState.config
                                 .weaponMaxLevel);
                    if (!enabled)
                        continue;
                    const float x = slotCenters[slot][0];
                    const float y = slotCenters[slot][1];
                    drawQuad(
                        *device, quad, shader, workshopSlot,
                        static_cast<float>(
                            workshopSlotImage.width),
                        static_cast<float>(
                            workshopSlotImage.height),
                        x, y, 48.0F, transparent);

                    const auto& installed =
                        profileState.player.slots[slot];
                    const auto* installedItem =
                        originalGarage->findItem(
                            installed.record);
                    const bool compatible =
                        previewItem != nullptr &&
                        workshopSlotAccepts(
                            slot, previewProfileSlot);
                    if (installedItem != nullptr)
                    {
                        const float rotation =
                            menuSelection ==
                                    firstSlotFocus + slot
                                ? garageSceneSeconds *
                                      bx::kPi * 0.5F
                                : 0.0F;
                        const float viewportSize = std::min(
                            static_cast<float>(
                                workshopSlotImage.width),
                            static_cast<float>(
                                workshopSlotImage.height));
                        workshopRenderer.drawItem(
                            *device, raceShader,
                            *installedItem, x, y,
                            viewportSize, viewportSize,
                            rotation, racePipeline);
                    }

                    if (slot >= 4U &&
                        (installedItem == nullptr ||
                         compatible))
                    {
                        std::size_t iconIndex =
                            slot == 4U
                                ? 0U
                                : slot == 5U ? 1U : 2U;
                        if (slot >= 6U && compatible)
                            iconIndex = 3U;
                        drawQuad(
                            *device, quad, shader,
                            workshopSlotIconTextures[
                                iconIndex],
                            static_cast<float>(
                                workshopSlotIconImages[
                                    iconIndex]
                                    .width),
                            static_cast<float>(
                                workshopSlotIconImages[
                                    iconIndex]
                                    .height),
                            x, y, 20.0F, transparent);
                    }

                    if (installedItem == nullptr)
                        continue;
                    if (installedItem->maximumCharge == 0U)
                    {
                        const auto slotType =
                            static_cast<
                                r3d::game::originalrace::
                                    GarageSlotType>(slot);
                        const int level =
                            std::clamp(
                                r3d::game::originalrace::
                                    originalWorkshopUpgradeLevel(
                                        installed.record,
                                        slotType),
                                0, 2);
                        const bool focused =
                            menuSelection ==
                            firstSlotFocus + slot;
                        const std::size_t imageIndex =
                            static_cast<std::size_t>(
                                std::min(
                                    level +
                                        (focused ? 1 : 0),
                                    2));
                        drawQuad(
                            *device, quad, shader,
                            workshopUpgradeTextures[
                                imageIndex],
                            static_cast<float>(
                                workshopUpgradeImages[
                                    imageIndex]
                                    .width),
                            static_cast<float>(
                                workshopUpgradeImages[
                                    imageIndex]
                                    .height),
                            x + 51.0F, y + 38.0F,
                            18.0F, transparent);
                    }
                    else
                    {
                        const float chargeX = x + 68.0F;
                        const float chargeY = y - 12.0F;
                        drawQuad(
                            *device, quad, shader,
                            workshopChargeBox,
                            static_cast<float>(
                                workshopChargeBoxImage.width),
                            static_cast<float>(
                                workshopChargeBoxImage.height),
                            chargeX, chargeY, 22.0F,
                            transparent);
                        const float progress =
                            std::clamp(
                                static_cast<float>(
                                    installed.hasCharge
                                        ? installed.charge
                                        : installedItem
                                              ->defaultCharge) /
                                    static_cast<float>(
                                        std::max(
                                            installedItem
                                                ->maximumCharge,
                                            1U)),
                                0.0F, 1.0F);
                        const float barHeight =
                            static_cast<float>(
                                workshopChargeBarImage.height) *
                            progress;
                        if (barHeight > 0.0F)
                        {
                            const float barBottom =
                                chargeY + 24.0F;
                            drawQuad(
                                *device, quad, shader,
                                workshopChargeBar,
                                static_cast<float>(
                                    workshopChargeBarImage.width),
                                barHeight, chargeX,
                                barBottom -
                                    barHeight * 0.5F,
                                18.0F, transparent);
                        }
                        const bool focused =
                            menuSelection ==
                            firstSlotFocus + slot;
                        drawQuad(
                            *device, quad, shader,
                            focused
                                ? workshopChargeButtonSelected
                                : workshopChargeButton,
                            static_cast<float>(
                                focused
                                    ? workshopChargeButtonSelectedImage
                                          .width
                                    : workshopChargeButtonImage
                                          .width),
                            static_cast<float>(
                                focused
                                    ? workshopChargeButtonSelectedImage
                                          .height
                                    : workshopChargeButtonImage
                                          .height),
                            chargeX, chargeY + 39.0F,
                            16.0F, transparent);
                    }
                }
            }

            const std::size_t rowCount =
                (workshopGoods.size() + 2U) / 3U;
            const std::size_t maximumScroll =
                rowCount > 4U ? rowCount - 4U : 0U;
            const float upY =
                leftPanelCenterY -
                static_cast<float>(
                    workshopLeftPanelImage.height) *
                    0.5F +
                65.0F;
            const float downY =
                leftPanelCenterY +
                static_cast<float>(
                    workshopLeftPanelImage.height) *
                    0.5F -
                42.0F;
            if (workshopGoodScroll > 0U)
            {
                drawQuadRotated(
                    *device, quad, shader, garageArrow,
                    30.0F, 30.0F, leftPanelCenterX, upY,
                    18.0F, bx::kPi * 0.5F, transparent);
            }
            if (workshopGoodScroll < maximumScroll)
            {
                drawQuadRotated(
                    *device, quad, shader, garageArrow,
                    30.0F, 30.0F, leftPanelCenterX, downY,
                    18.0F, -bx::kPi * 0.5F, transparent);
            }

            const auto& header = workshopPage.normal[0];
            drawQuad(
                *device, quad, shader, header.texture,
                header.width, header.height,
                20.0F + header.width * 0.5F, 40.0F,
                15.0F, transparent);
            const auto& money =
                workshopControlsPage.normal[1];
            drawQuad(
                *device, quad, shader, money.texture,
                money.width, money.height,
                menu::virtualWidth - 53.0F -
                    money.width * 0.5F,
                menu::virtualHeight - 29.0F,
                15.0F, transparent);

            const bool backFocused =
                menuSelection == 0U;
            const float backX =
                static_cast<float>(
                    garageBackImage.width) *
                0.5F;
            const float backY =
                menu::virtualHeight -
                static_cast<float>(
                    workshopBottomPanelImage.height) +
                40.0F;
            drawQuad(
                *device, quad, shader,
                backFocused ? garageBackSelected
                            : garageBack,
                static_cast<float>(
                    backFocused
                        ? garageBackSelectedImage.width
                        : garageBackImage.width),
                static_cast<float>(
                    backFocused
                        ? garageBackSelectedImage.height
                        : garageBackImage.height),
                backX, backY, 35.0F, transparent);
            const auto& backText =
                backFocused
                    ? workshopControlsPage.selected[0]
                    : workshopControlsPage.normal[0];
            drawQuad(
                *device, quad, shader, backText.texture,
                backText.width, backText.height, backX,
                backY, 15.0F, transparent);

            for (std::size_t line = 0U;
                 line < workshopHintPage.normal.size();
                 ++line)
            {
                const auto& hint =
                    workshopHintPage.normal[line];
                drawQuad(
                    *device, quad, shader, hint.texture,
                    hint.width, hint.height, 480.0F,
                    menu::virtualHeight - 93.0F +
                        static_cast<float>(line) *
                            19.0F,
                    15.0F, transparent);
            }

            if (workshopWeaponDialog.visible &&
                !workshopDrag.active() &&
                workshopConfirmation ==
                    WorkshopConfirmation::None)
            {
                const float anchorX =
                    workshopWeaponDialog.centerX;
                const float anchorY =
                    workshopWeaponDialog.centerY;
                drawQuad(
                    *device, quad, shader,
                    workshopInfoFrame,
                    static_cast<float>(
                        workshopInfoFrameImage.width),
                    static_cast<float>(
                        workshopInfoFrameImage.height),
                    anchorX, anchorY, 60.0F,
                    transparent);

                const auto& name =
                    workshopWeaponDialog.name;
                drawQuad(
                    *device, quad, shader, name.texture,
                    name.width, name.height, anchorX,
                    anchorY - 58.0F, 59.0F,
                    transparent);
                constexpr float infoLineStep = 18.0F;
                const float infoFirstY =
                    anchorY - 3.0F -
                    static_cast<float>(
                        workshopWeaponDialog.info.size() - 1U) *
                        infoLineStep * 0.5F;
                for (std::size_t line = 0U;
                     line < workshopWeaponDialog.info.size();
                     ++line)
                {
                    const auto& info =
                        workshopWeaponDialog.info[line];
                    drawQuad(
                        *device, quad, shader,
                        info.texture, info.width, info.height,
                        anchorX - 137.0F +
                            info.width * 0.5F,
                        infoFirstY +
                            static_cast<float>(line) *
                                infoLineStep,
                        59.0F, transparent);
                }
                const auto& money =
                    workshopWeaponDialog.money;
                drawQuad(
                    *device, quad, shader,
                    money.texture, money.width, money.height,
                    anchorX - 60.0F, anchorY + 54.0F,
                    59.0F, transparent);
                const auto& damage =
                    workshopWeaponDialog.damage;
                drawQuad(
                    *device, quad, shader,
                    damage.texture, damage.width, damage.height,
                    anchorX + 80.0F, anchorY + 54.0F,
                    59.0F, transparent);
                raceWorkshopWeaponDialogObserved = true;
            }

            if (workshopDrag.active())
            {
                if (const auto* dragged =
                        originalGarage->findItem(
                            workshopDrag.item.record))
                {
                    const float viewportSize = std::min(
                        static_cast<float>(
                            workshopSlotImage.width),
                        static_cast<float>(
                            workshopSlotImage.height));
                    workshopRenderer.drawItem(
                        *device, raceShader, *dragged,
                        workshopDragX, workshopDragY,
                        viewportSize, viewportSize, 0.0F,
                        racePipeline);
                }
            }
        }
        else if (drawingOriginalAngar)
        {
            raceAngarFrameObserved = true;
            const auto planetCount = std::min(
                originalGarage->planets.size(),
                profileState.player.planets.size());
            const float panelCenterX = menu::virtualWidth * 0.5F;
            const float panelCenterY =
                menu::virtualHeight -
                static_cast<float>(angarBottomPanelImage.height) *
                    0.5F -
                20.0F;
            drawQuad(
                *device, quad, shader, angarBottomPanel,
                static_cast<float>(angarBottomPanelImage.width),
                static_cast<float>(angarBottomPanelImage.height),
                panelCenterX, panelCenterY, 60.0F, transparent);

            float doorAlpha = 1.0F;
            if (angarDoorTime >= 0.0F)
            {
                doorAlpha = std::clamp(
                    angarDoorTime / 0.25F, 0.0F, 1.0F);
                angarDoorTime += frameSeconds;
                if (doorAlpha >= 1.0F)
                    angarDoorTime = -1.0F;
            }
            constexpr float planetSpacing = 224.0F;
            const float firstPlanetX =
                panelCenterX -
                static_cast<float>(
                    angarBottomPanelImage.width) *
                    0.5F +
                35.0F + 90.0F;
            const float planetY =
                panelCenterY -
                static_cast<float>(
                    angarBottomPanelImage.height) *
                    0.5F +
                90.0F;
            const float slotY = panelCenterY + 67.0F;
            for (std::size_t index = 0U;
                 index < planetCount; ++index)
            {
                const float x =
                    firstPlanetX +
                    static_cast<float>(index) * planetSpacing;
                const bool selected =
                    static_cast<int>(index) == angarPlanetIndex;
                const bool previous =
                    static_cast<int>(index) ==
                    angarPreviousPlanetIndex;
                const float animation =
                    selected
                        ? angarSceneSeconds * bx::kPi / 24.0F
                        : 0.0F;
                workshopRenderer.drawPlanet(
                    *device, raceShader,
                    originalGarage->planets[index],
                    x, planetY, 180.0F, 180.0F,
                    animation, racePipeline);

                const bool focused =
                    menuSelection == index && selected;
                drawQuad(
                    *device, quad, shader,
                    focused ? angarDoorSlotSelected
                            : angarDoorSlot,
                    static_cast<float>(
                        focused
                            ? angarDoorSlotSelectedImage.width
                            : angarDoorSlotImage.width),
                    static_cast<float>(
                        focused
                            ? angarDoorSlotSelectedImage.height
                            : angarDoorSlotImage.height),
                    x, slotY, 35.0F, transparent);

                float alpha = selected ? 1.0F : 0.0F;
                if (angarDoorTime >= 0.0F ||
                    doorAlpha < 1.0F)
                {
                    if (selected)
                        alpha = doorAlpha;
                    else if (previous)
                        alpha = 1.0F - doorAlpha;
                }
                const float offset = 16.0F * alpha;
                drawQuad(
                    *device, quad, shader, angarDoorDown,
                    static_cast<float>(angarDoorDownImage.width),
                    static_cast<float>(angarDoorDownImage.height),
                    x - 1.0F,
                    slotY -
                        static_cast<float>(
                            angarDoorSlotImage.height) *
                            0.5F +
                        static_cast<float>(
                            angarDoorDownImage.height) *
                            0.5F -
                        4.0F + offset,
                    24.0F, transparent);
                drawQuad(
                    *device, quad, shader, angarDoorUp,
                    static_cast<float>(angarDoorUpImage.width),
                    static_cast<float>(angarDoorUpImage.height),
                    x - 1.0F,
                    slotY +
                        static_cast<float>(
                            angarDoorSlotImage.height) *
                            0.5F -
                        static_cast<float>(
                            angarDoorUpImage.height) *
                            0.5F +
                        4.0F - offset,
                    24.0F, transparent);
                if (index < planetsPage.normal.size())
                {
                    const auto& status =
                        focused
                            ? planetsPage.selected[index]
                            : planetsPage.normal[index];
                    const float scale = std::min(
                        1.0F,
                        150.0F /
                            std::max(status.width, 1.0F));
                    drawQuad(
                        *device, quad, shader,
                        status.texture,
                        status.width * scale,
                        status.height * scale,
                        x, slotY, 15.0F, transparent);
                }
            }

            const std::size_t backIndex = planetCount;
            const bool backFocused =
                menuSelection == backIndex;
            const float backX =
                static_cast<float>(garageBackImage.width) * 0.5F;
            const float backY = 40.0F;
            drawQuad(
                *device, quad, shader,
                backFocused ? garageBackSelected : garageBack,
                static_cast<float>(
                    backFocused
                        ? garageBackSelectedImage.width
                        : garageBackImage.width),
                static_cast<float>(
                    backFocused
                        ? garageBackSelectedImage.height
                        : garageBackImage.height),
                backX, backY, 35.0F, transparent);
            if (backIndex < planetsPage.normal.size())
            {
                const auto& backText =
                    backFocused
                        ? planetsPage.selected[backIndex]
                        : planetsPage.normal[backIndex];
                drawQuad(
                    *device, quad, shader, backText.texture,
                    backText.width, backText.height,
                    backX, backY, 15.0F, transparent);
            }

            if (angarPlanetIndex >= 0 &&
                static_cast<std::size_t>(angarPlanetIndex) <
                    planetCount)
            {
                const auto index =
                    static_cast<std::size_t>(angarPlanetIndex);
                const float planetX =
                    firstPlanetX +
                    static_cast<float>(index) * planetSpacing;
                const float infoWidth =
                    static_cast<float>(angarPlanetInfoImage.width);
                const float infoHeight =
                    static_cast<float>(angarPlanetInfoImage.height);
                const float infoX = std::clamp(
                    planetX, infoWidth * 0.5F,
                    menu::virtualWidth - infoWidth * 0.5F);
                const float infoY = panelCenterY - 260.0F;
                const float infoLeft = infoX - infoWidth * 0.5F;
                const float infoTop = infoY - infoHeight * 0.5F;
                drawQuad(
                    *device, quad, shader, angarPlanetInfo,
                    infoWidth, infoHeight, infoX, infoY,
                    13.0F, transparent);
                drawQuad(
                    *device, quad, shader, angarClose,
                    static_cast<float>(angarCloseImage.width),
                    static_cast<float>(angarCloseImage.height),
                    infoX + 180.0F, infoTop + 15.0F,
                    10.0F, transparent);

                const auto& photoImage = angarBossImages[index];
                const float photoScale = std::min(
                    110.0F /
                        std::max<float>(
                            static_cast<float>(photoImage.width),
                            1.0F),
                    85.0F /
                        std::max<float>(
                            static_cast<float>(photoImage.height),
                            1.0F));
                drawQuad(
                    *device, quad, shader,
                    angarBossTextures[index],
                    static_cast<float>(photoImage.width) *
                        photoScale,
                    static_cast<float>(photoImage.height) *
                        photoScale,
                    infoX + 107.0F, infoTop + 89.0F,
                    9.0F, transparent);
                workshopRenderer.drawCar(
                    *device, raceShader,
                    originalGarage->planets[index]
                        .bossCarRecord,
                    infoX + 107.0F, infoTop + 202.0F,
                    110.0F, 110.0F,
                    angarSceneSeconds * bx::kPi * 0.5F,
                    racePipeline);

                if (!angarInfoPage.normal.empty())
                {
                    const auto& name =
                        angarInfoPage.normal[0];
                    drawQuad(
                        *device, quad, shader,
                        name.texture, name.width,
                        name.height, infoX,
                        infoTop + 11.0F, 7.0F,
                        transparent);
                }
                if (angarInfoPage.normal.size() > 1U)
                {
                    const auto& boss =
                        angarInfoPage.normal[1];
                    drawQuad(
                        *device, quad, shader,
                        boss.texture, boss.width,
                        boss.height, infoX - 60.0F,
                        infoTop + 42.0F, 7.0F,
                        transparent);
                }
                for (std::size_t line = 2U;
                     line < angarInfoPage.normal.size();
                     ++line)
                {
                    const auto& text =
                        angarInfoPage.normal[line];
                    drawQuad(
                        *device, quad, shader,
                        text.texture, text.width,
                        text.height,
                        infoLeft + 15.0F +
                            text.width * 0.5F,
                        infoTop + 64.0F +
                            static_cast<float>(line - 2U) *
                                18.0F,
                        7.0F, transparent);
                }
            }

        }
        else if (drawingOriginalFinish)
        {
            constexpr float boxDelay = 0.15F;
            constexpr float voiceDuration = 1.5F;
            const float leftWidth =
                static_cast<float>(finishLeftFrameImage.width);
            const float leftHeight =
                static_cast<float>(finishLeftFrameImage.height);
            const float top =
                (menu::virtualHeight - 3.0F * leftHeight) * 0.5F;
            const float leftLabelX =
                (leftWidth + menu::virtualWidth * 0.5F) * 0.5F;
            const float rightLabelX =
                (menu::virtualWidth * 0.5F +
                 menu::virtualWidth - leftWidth) *
                0.5F;
            float accumulatedDuration = 0.0F;
            for (std::size_t index = 0U;
                 index < finishRows.size() && index < 3U; ++index)
            {
                const float alpha = std::clamp(
                    (finishAnimationSeconds -
                     accumulatedDuration - boxDelay) /
                        0.5F,
                    0.0F, 1.0F);
                accumulatedDuration += voiceDuration;
                if (alpha <= 0.0F)
                    continue;
#ifdef RRR3D_AUDIO
                if (index == finishVoiceIndex)
                {
                    const auto& result =
                        raceSession.racers()[finishRows[index].racer];
                    commentator.finishPlace(
                        *originalRace, finishRows[index].racer,
                        result.place, audioError);
                    ++finishVoiceIndex;
                }
#endif
                const float offsetX =
                    (1.0F - alpha) *
                    (menu::virtualWidth + 25.0F) *
                    (index % 2U == 1U ? 1.0F : -1.0F);
                const float rowTop =
                    top + static_cast<float>(index) * leftHeight;
                const float rowCenterY = rowTop + leftHeight * 0.5F;
                const float lineWidth =
                    menu::virtualWidth - 2.0F * leftWidth;

                drawQuad(
                    *device, quad, shader, finishLeftFrame,
                    leftWidth, leftHeight,
                    offsetX + leftWidth * 0.5F,
                    rowCenterY, 55.0F, transparent);
                drawQuad(
                    *device, quad, shader, finishLineFrame,
                    lineWidth, leftHeight,
                    offsetX + menu::virtualWidth * 0.5F,
                    rowCenterY, 55.0F, transparent);
                drawQuad(
                    *device, quad, shader, finishRightFrame,
                    leftWidth, leftHeight,
                    offsetX + menu::virtualWidth -
                        leftWidth * 0.5F,
                    rowCenterY, 55.0F, transparent);

                const auto& row = finishRows[index];
                if (valid(row.photo))
                {
                    drawQuad(
                        *device, quad, shader, row.photo,
                        row.photoWidth, row.photoHeight,
                        offsetX + 128.0F, rowTop + 116.0F,
                        35.0F, transparent);
                }
                const auto& cupImage = finishCupImages[index];
                const float cupScale = std::min(
                    {1.0F,
                     190.0F /
                         std::max(
                             static_cast<float>(cupImage.width), 1.0F),
                     160.0F /
                         std::max(
                             static_cast<float>(cupImage.height), 1.0F)});
                drawQuad(
                    *device, quad, shader, finishCups[index],
                    static_cast<float>(cupImage.width) * cupScale,
                    static_cast<float>(cupImage.height) * cupScale,
                    offsetX + menu::virtualWidth - leftWidth +
                        160.0F,
                    rowTop + 115.0F, 35.0F, transparent);

                drawQuad(
                    *device, quad, shader, row.name.texture,
                    row.name.width, row.name.height,
                    offsetX + leftLabelX, rowTop + 63.0F,
                    20.0F, transparent);
                drawQuad(
                    *device, quad, shader, finishRewardTitle.texture,
                    finishRewardTitle.width,
                    finishRewardTitle.height,
                    offsetX + rightLabelX, rowTop + 63.0F,
                    20.0F, transparent);
                drawQuad(
                    *device, quad, shader, finishMoneyTitle.texture,
                    finishMoneyTitle.width,
                    finishMoneyTitle.height,
                    offsetX + leftLabelX, rowTop + 136.0F,
                    20.0F, transparent);
                drawQuad(
                    *device, quad, shader, finishPointsTitle.texture,
                    finishPointsTitle.width,
                    finishPointsTitle.height,
                    offsetX + leftLabelX, rowTop + 172.0F,
                    20.0F, transparent);
                drawQuad(
                    *device, quad, shader, row.rewardMoney.texture,
                    row.rewardMoney.width, row.rewardMoney.height,
                    offsetX + rightLabelX, rowTop + 136.0F,
                    20.0F, transparent);
                drawQuad(
                    *device, quad, shader, row.rewardPoints.texture,
                    row.rewardPoints.width,
                    row.rewardPoints.height,
                    offsetX + rightLabelX, rowTop + 172.0F,
                    20.0F, transparent);
            }
            const float totalDuration =
                static_cast<float>(finishRows.size()) *
                    voiceDuration +
                boxDelay;
            if (finishAnimationSeconds >= totalDuration)
            {
#ifdef RRR3D_AUDIO
                // FinishMenu.cpp emits the first three place events as their
                // boxes appear, then emits cPlayerFinishLast for the final
                // Race::Result after the reveal if at least four racers took
                // part.  The fourth event is deliberately not tied to a row.
                if (!finishLastVoiceDispatched)
                {
                    const auto finishedCount = std::count_if(
                        raceSession.racers().begin(),
                        raceSession.racers().end(),
                        [](const auto& racer) {
                            return !racer.disconnected && racer.finished;
                    });
                    if (finishedCount >= 4)
                    {
                        std::size_t lastRacer =
                            raceSession.racers().size();
                        for (std::size_t racer = 0U;
                             racer < raceSession.racers().size(); ++racer)
                        {
                            const auto& candidate =
                                raceSession.racers()[racer];
                            if (candidate.disconnected ||
                                !candidate.finished)
                                continue;
                            if (lastRacer == raceSession.racers().size() ||
                                candidate.place >
                                    raceSession.racers()[lastRacer].place)
                                lastRacer = racer;
                        }
                        if (lastRacer < raceSession.racers().size())
                        {
                            commentator.finishPlace(
                                *originalRace, lastRacer,
                                raceSession.racers()[lastRacer].place,
                                audioError);
                        }
                    }
                    finishLastVoiceDispatched = true;
                }
#endif
                finishAnimationSeconds = totalDuration;
                finishMenuFrameObserved =
                    !finishRows.empty() &&
                    finishRows.size() <= 3U &&
                    std::all_of(
                        finishRows.begin(), finishRows.end(),
                        [](const FinishRowVisual& row) {
                            return valid(row.name.texture) &&
                                   valid(row.rewardMoney.texture) &&
                                   valid(row.rewardPoints.texture) &&
                                   valid(row.photo);
                        });
            }
            else
                finishAnimationSeconds += frameSeconds;
        }
        else if (drawingOriginalAchievements)
        {
            raceAchievementFrameObserved = true;
            const float centerX = menu::virtualWidth * 0.5F;
            const float centerY = menu::virtualHeight * 0.5F;
            const float scale = std::min(
                menu::virtualWidth / 1090.0F,
                menu::virtualHeight / 720.0F);
            drawQuad(
                *device, quad, shader, achievementBackground,
                menu::virtualWidth, menu::virtualHeight,
                centerX, centerY, 85.0F, transparent);
            drawQuad(
                *device, quad, shader, achievementPanel,
                static_cast<float>(achievementPanelImage.width) *
                    scale,
                static_cast<float>(achievementPanelImage.height) *
                    scale,
                centerX,
                static_cast<float>(achievementPanelImage.height) *
                    scale * 0.5F,
                60.0F, transparent);

            const float bottomPanelPositionY =
                menu::virtualHeight - 80.0F;
            drawQuad(
                *device, quad, shader, achievementBottomPanel,
                menu::virtualWidth,
                static_cast<float>(
                    achievementBottomPanelImage.height),
                centerX,
                bottomPanelPositionY -
                    static_cast<float>(
                        achievementBottomPanelImage.height) *
                        0.5F,
                60.0F, transparent);

            drawQuad(
                *device, quad, shader, achievementRewards.texture,
                achievementRewards.width * scale,
                achievementRewards.height * scale, centerX,
                555.0F * scale, 15.0F, transparent);
            drawQuad(
                *device, quad, shader, achievementPoints.texture,
                achievementPoints.width, achievementPoints.height,
                centerX, bottomPanelPositionY + 40.0F,
                15.0F, transparent);

            for (std::size_t index = 0U;
                 index < originalAchievementVisuals.size(); ++index)
            {
                const auto state = achievementState(index);
                const bool opened = state == "asOpened";
                const bool unlocked = state == "asUnlocked";
                const auto& visual =
                    originalAchievementVisuals[index];
                const auto& image =
                    opened ? achievementOpenedImages[index]
                           : achievementLockedImages[index];
                const Texture imageTexture =
                    opened ? achievementOpenedTextures[index]
                           : achievementLockedTextures[index];
                const float imageX =
                    centerX + (visual.x - 30.0F) * scale;
                const float imageY = visual.y * scale;
                const float imageWidth =
                    static_cast<float>(image.width) * scale;
                const float imageHeight =
                    static_cast<float>(image.height) * scale;
                drawQuad(
                    *device, quad, shader, imageTexture,
                    imageWidth, imageHeight, imageX, imageY,
                    45.0F, transparent);

                const bool focused =
                    menuSelection == index && unlocked;
                const Texture buttonTexture =
                    opened
                        ? achievementOkSelected
                        : unlocked
                              ? (focused
                                     ? achievementOkSelected
                                     : achievementOk)
                              : achievementClose;
                const auto& buttonImage =
                    opened || (unlocked && focused)
                        ? achievementOkSelectedImage
                        : unlocked ? achievementOkImage
                                   : achievementCloseImage;
                const float buttonX =
                    imageX +
                    (static_cast<float>(image.width) * 0.5F -
                     15.0F) *
                        scale;
                const float buttonY =
                    imageY +
                    (static_cast<float>(image.height) * 0.5F -
                     25.0F) *
                        scale;
                drawQuad(
                    *device, quad, shader, buttonTexture,
                    static_cast<float>(buttonImage.width) * scale,
                    static_cast<float>(buttonImage.height) * scale,
                    buttonX, buttonY, 30.0F, transparent);
                if (unlocked &&
                    index < achievementPricePage.normal.size())
                {
                    const auto& price =
                        achievementPricePage.normal[index];
                    drawQuad(
                        *device, quad, shader, price.texture,
                        price.width * scale,
                        price.height * scale, imageX, buttonY,
                        18.0F, transparent);
                }
            }

            const bool backFocused =
                menuSelection == originalAchievementBack;
            const float backX =
                static_cast<float>(garageBackImage.width) * 0.5F;
            const float backY =
                bottomPanelPositionY + 17.0F +
                static_cast<float>(garageBackImage.height) * 0.5F;
            drawQuad(
                *device, quad, shader,
                backFocused ? garageBackSelected : garageBack,
                static_cast<float>(
                    backFocused ? garageBackSelectedImage.width
                                : garageBackImage.width),
                static_cast<float>(
                    backFocused ? garageBackSelectedImage.height
                                : garageBackImage.height),
                backX, backY, 35.0F, transparent);
            const auto& backText =
                backFocused
                    ? achievementsPage.selected[
                          originalAchievementBack]
                    : achievementsPage.normal[
                          originalAchievementBack];
            drawQuad(
                *device, quad, shader, backText.texture,
                backText.width, backText.height, backX, backY,
                15.0F, transparent);

        }
        else
#endif
        {
#ifdef RRR3D_NETWORK
            if (menuStack.back() == MenuScreen::Network)
            {
                networkFrameObserved =
                    networkFrameObserved ||
                    (networkSession.initialized() &&
                     activePage.labels.size() == 3U);
            }
            else if (
                menuStack.back() == MenuScreen::NetworkServerType)
            {
                networkServerTypeObserved =
                    networkServerTypeObserved ||
                    activePage.labels.size() == 2U;
            }
            else if (
                menuStack.back() == MenuScreen::NetworkClientType)
            {
                networkClientTypeObserved =
                    networkClientTypeObserved ||
                    activePage.labels.size() == 3U;
            }
            else if (
                menuStack.back() == MenuScreen::NetworkBrowser)
            {
                networkBrowserObserved =
                    networkBrowserObserved ||
                    (networkSnapshot.state ==
                         r3d::game::originalnetwork::
                             SessionState::Searching ||
                     networkSnapshot.state ==
                         r3d::game::originalnetwork::SessionState::Idle);
            }
            else if (
                menuStack.back() == MenuScreen::NetworkIpAddress)
            {
                networkIpObserved =
                    networkIpObserved ||
                    (activePage.labels.size() == 2U &&
                     networkIpInput == "127.0.0.1");
            }
#endif
#ifdef RRR3D_PHYSICS
            if (menuStack.back() == MenuScreen::GameMode)
            {
                gameModeFrameObserved =
                    activePage.labels.size() == 3U &&
                    activePage.enabled[1] ==
                        (profileState.tutorialStage >= 1U) &&
                    sharedMenuItemY(
                        MenuScreen::GameMode, 2U, 3U) ==
                        menu::virtualHeight * 0.5F + 150.0F;
            }
            else if (menuStack.back() == MenuScreen::Tournament)
            {
                const bool hasProfiles =
                    !profileState.profiles.empty();
                tournamentFrameObserved =
                    activePage.labels.size() == 4U &&
                    activePage.enabled[0] == hasProfiles &&
                    activePage.enabled[2] == hasProfiles &&
                    sharedMenuItemY(
                        MenuScreen::Tournament, 3U, 4U) ==
                        menu::virtualHeight * 0.5F + 150.0F;
            }
#endif
            for (std::size_t index = 0;
                 index < activePage.normal.size(); ++index)
            {
                const float centerX =
                    menu::virtualWidth * 0.5F +
                    menu::itemCenterOffsetX;
                const float centerY =
                    sharedMenuItemY(
                        menuStack.back(), index,
                        activePage.normal.size());
                const bool enabled = activePage.enabled[index];
                if (enabled && index == menuSelection)
                {
                    drawQuad(
                        *device, quad, shader, selection,
                        static_cast<float>(
                            model->selectionImage.width),
                        static_cast<float>(
                            model->selectionImage.height),
                        centerX, centerY, 50.0F, transparent);
                }
                const auto& text =
                    !enabled
                        ? activePage.disabled[index]
                        : index == menuSelection
                              ? activePage.selected[index]
                              : activePage.normal[index];
                drawQuad(
                    *device, quad, shader, text.texture,
                    text.width, text.height, centerX, centerY,
                    25.0F, transparent);
            }
#ifdef RRR3D_NETWORK
            auto drawNetworkLines =
                [&](const MenuPageVisual& textPage,
                    float firstY) {
                    for (std::size_t index = 0U;
                         index < textPage.normal.size(); ++index)
                    {
                        const auto& line = textPage.normal[index];
                        drawQuad(
                            *device, quad, shader, line.texture,
                            line.width, line.height,
                            menu::virtualWidth * 0.5F,
                            firstY + static_cast<float>(index) *
                                         (menu::smallFontHeight + 4.0F),
                            20.0F, transparent);
                    }
                };
            if (menuStack.back() == MenuScreen::Network)
            {
                drawNetworkLines(
                    networkAddressInfoPage,
                    menu::virtualHeight * 0.5F + 75.0F);
            }
            else if (
                menuStack.back() == MenuScreen::NetworkBrowser)
            {
                drawNetworkLines(
                    networkStatusPage,
                    menu::virtualHeight * 0.5F);
            }
            else if (
                menuStack.back() == MenuScreen::NetworkIpAddress)
            {
                drawNetworkLines(
                    networkStatusPage,
                    menu::virtualHeight * 0.5F + 15.0F);
                drawNetworkLines(
                    networkIpValuePage,
                    menu::virtualHeight * 0.5F + 45.0F);
            }
#endif
        }
        if (!drawingOriginalFinal
#ifdef RRR3D_PHYSICS
            && !drawingSourceStartOptions &&
            !drawingOriginalOptions && !drawingOriginalGamers &&
            !drawingOriginalRaceMenu &&
            !drawingOriginalGarage && !drawingOriginalWorkshop &&
            !drawingOriginalAngar && !drawingOriginalAchievements &&
            !drawingOriginalFinish
#endif
        )
        {
            const float versionX =
                menu::virtualWidth - 25.0F -
                version.width * 0.5F;
            const float versionY =
                menu::virtualHeight - 25.0F -
                version.height * 0.5F;
            drawQuad(
                *device, quad, shader, version.texture,
                version.width, version.height, versionX,
                versionY, 25.0F, transparent);
        }
#ifdef RRR3D_PHYSICS
        drawUserChat();
        drawAcceptDialog();
        if (infoDialog.visible)
        {
            drawQuad(
                *device, quad, shader, infoDialogFrame,
                static_cast<float>(infoDialogFrameImage.width),
                static_cast<float>(infoDialogFrameImage.height),
                infoDialog.centerX, infoDialog.centerY, 6.0F,
                transparent);
            drawQuad(
                *device, quad, shader, infoDialog.title.texture,
                infoDialog.title.width, infoDialog.title.height,
                infoDialog.centerX - 27.0F,
                infoDialog.centerY - 105.0F, 4.0F,
                transparent);
            constexpr float infoLineStep = 27.0F;
            const float firstLineY =
                infoDialog.centerY + 5.0F -
                static_cast<float>(infoDialog.info.size() - 1U) *
                    infoLineStep * 0.5F;
            for (std::size_t line = 0U;
                 line < infoDialog.info.size(); ++line)
            {
                const auto& text = infoDialog.info[line];
                drawQuad(
                    *device, quad, shader, text.texture,
                    text.width, text.height,
                    infoDialog.centerX - 122.5F +
                        text.width * 0.5F,
                    firstLineY +
                        static_cast<float>(line) *
                            infoLineStep,
                    4.0F, transparent);
            }
            if (infoDialog.dismissable)
            {
                drawQuad(
                    *device, quad, shader,
                    infoDialogButtonSelected,
                    static_cast<float>(
                        infoDialogButtonSelectedImage.width),
                    static_cast<float>(
                        infoDialogButtonSelectedImage.height),
                    infoDialog.centerX,
                    infoDialog.centerY + 105.0F, 3.0F,
                    transparent);
                drawQuad(
                    *device, quad, shader, infoDialog.ok.texture,
                    infoDialog.ok.width, infoDialog.ok.height,
                    infoDialog.centerX,
                    infoDialog.centerY + 105.0F, 2.0F,
                    transparent);
            }
            raceInfoDialogObserved =
                valid(infoDialog.title.texture) &&
                !infoDialog.info.empty() &&
                std::all_of(
                    infoDialog.info.begin(),
                    infoDialog.info.end(),
                    [](const TextVisual& line) {
                        return valid(line.texture);
                    }) &&
                valid(infoDialog.ok.texture);
        }
#endif
#ifdef RRR3D_AUDIO
        drawOriginalMusicDialog();
#endif
        device->endFrame();
#ifdef RRR3D_PHYSICS
        }
#endif

        ++renderedFrames;
        if (options->smokeFrames != 0 &&
            renderedFrames >= options->smokeFrames
#ifdef RRR3D_AUDIO
            && (!options->audioSmokeTest ||
                musicSmokePhase == MusicSmokePhase::Complete)
#endif
#ifdef RRR3D_VIDEO
            && (!options->videoSmokeTest ||
                videoCompletionObserved)
#endif
        )
        {
#ifdef RRR3D_VIDEO
            if (options->videoSmokeTest)
            {
                if (!videoFrameObserved || !videoAudioObserved ||
                    !videoCompletionObserved ||
                    !videoTournamentStartObserved ||
                    !videoSmokeSeeked)
                {
                    std::cerr
                        << "Source movie smoke failed: frame="
                        << videoFrameObserved
                        << ", audio=" << videoAudioObserved
                        << ", completion="
                        << videoCompletionObserved
                        << ", tournament-start="
                        << videoTournamentStartObserved
                        << ", seek=" << videoSmokeSeeked << '\n';
                    runtimeSmokeFailed = true;
                }
                else
                {
                    std::cout
                        << "Source movie smoke passed after "
                        << renderedFrames
                        << " frames: original H.264/AAC movie, "
                           "AVFoundation audio/video playback, display, "
                           "seek, cVideoStopped and tournament callback "
                           "verified\n";
                }
            }
            else
#endif
#ifdef RRR3D_PHYSICS
            if (options->startOptionsSmokeTest)
            {
                if (sourceStartOptionsActive ||
                    !startOptionsFrameObserved ||
                    !startOptionsSelectGateObserved ||
                    !startOptionsAllRowsObserved ||
                    !startOptionsCameraAppliedObserved ||
                    !startOptionsSavedObserved ||
                    !startOptionsMainTransitionObserved ||
                    startOptionsSmokeStep != 6U ||
                    menuStack.back() != MenuScreen::Main)
                {
                    std::cerr
                        << "Source StartOptionsMenu smoke failed: active="
                        << sourceStartOptionsActive
                        << ", frame/gate/rows="
                        << startOptionsFrameObserved << '/'
                        << startOptionsSelectGateObserved << '/'
                        << startOptionsAllRowsObserved
                        << ", camera/save="
                        << startOptionsCameraAppliedObserved << '/'
                        << startOptionsSavedObserved
                        << ", transition="
                        << startOptionsMainTransitionObserved
                        << ", steps=" << startOptionsSmokeStep
                        << '\n';
                    runtimeSmokeFailed = true;
                }
                else
                {
                    std::cout
                        << "Source StartOptionsMenu smoke passed after "
                        << renderedFrames
                        << " frames: Select camera gate, four source "
                           "steppers, Apply persistence and MainMenu2 "
                           "transition verified\n";
                }
            }
            else
#endif
            if (options->startupSmokeTest)
            {
                if (sourceStartupActive ||
                    !startupYardFadeObserved ||
                    !startupYardHoldObserved ||
                    !startupLabFadeObserved ||
                    !startupLabHoldObserved ||
                    !startupInitialBlankObserved ||
                    !startupInterlogoBlankObserved ||
                    !startupLoadFrameObserved ||
                    !startupMenuTransitionObserved ||
                    !startupEscapeObserved ||
                    menuStack.back() != MenuScreen::Main)
                {
                    std::cerr
                        << "Source GameMode startup smoke failed: active="
                        << sourceStartupActive
                        << ", yard=" << startupYardFadeObserved << '/'
                        << startupYardHoldObserved
                        << ", lab=" << startupLabFadeObserved << '/'
                        << startupLabHoldObserved
                        << ", blanks="
                        << startupInitialBlankObserved << '/'
                        << startupInterlogoBlankObserved
                        << ", load=" << startupLoadFrameObserved
                        << ", escape=" << startupEscapeObserved
                        << ", menu="
                        << startupMenuTransitionObserved << '/'
                        << (menuStack.back() == MenuScreen::Main)
                        << '\n';
                    runtimeSmokeFailed = true;
                }
                else
                {
                    std::cout
                        << "Source GameMode startup smoke passed after "
                        << renderedFrames
                        << " frames: yardLogo/laboratoria24 fade/hold, "
                           "blank delays, startLogo aspect frame, Escape "
                           "contract and MainMenu2 transition verified\n";
                }
            }
#ifdef RRR3D_NETWORK
            else if (options->networkMenuSmokeTest)
            {
                const bool returnedToMain =
                    menuStack.back() == MenuScreen::Main;
                if (!networkFrameObserved ||
                    !networkServerTypeObserved ||
                    !networkClientTypeObserved ||
                    !networkBrowserObserved ||
                    !networkIpObserved ||
                    !networkHostReadyGateObserved ||
                    !networkFailureDialogObserved ||
                    networkSmokeStep != 10U ||
                    networkSession.initialized() || !returnedToMain)
                {
                    std::cerr
                        << "Source LAN menu smoke failed: network="
                        << networkFrameObserved
                        << ", serverType="
                        << networkServerTypeObserved
                        << ", clientType="
                        << networkClientTypeObserved
                        << ", browser=" << networkBrowserObserved
                        << ", ip=" << networkIpObserved
                        << ", hostReadyGate="
                        << networkHostReadyGateObserved
                        << ", failureDialog="
                        << networkFailureDialogObserved
                        << ", steps=" << networkSmokeStep
                        << ", finalized="
                        << !networkSession.initialized()
                        << ", main=" << returnedToMain << '\n';
                    runtimeSmokeFailed = true;
                }
                else
                {
                    std::cout
                        << "Source LAN menu smoke passed after "
                        << renderedFrames
                        << " frames: NetworkFrame, ServerTypeFrame, "
                           "ClientTypeFrame, LAN browser, manual IP, "
                           "adapter list, NetGame lifecycle and host "
                           "AllPlayersReady gate plus source connection-"
                           "failure dialog verified\n";
                }
            }
#endif
            else if (options->finalMenuSmokeTest)
            {
                const bool allSlidesObserved =
                    std::all_of(
                        finalSlidesObserved.begin(),
                        finalSlidesObserved.end(),
                        [](bool observed) { return observed; });
                const bool finalMusicObserved =
#ifdef RRR3D_AUDIO
                    finalMusicPlaybackObserved;
#else
                    true;
#endif
                if (!allSlidesObserved ||
                    !finalCreditsMotionObserved ||
                    !finalBackFrameObserved ||
                    !finalAutoCloseObserved ||
                    !finalMusicObserved ||
                    menuStack.back() != MenuScreen::Main)
                {
                    std::cerr
                        << "Source FinalMenu renderer smoke failed: "
                        << "slides=" << allSlidesObserved
                        << ", credits=" << finalCreditsMotionObserved
                        << ", back=" << finalBackFrameObserved
                        << ", autoClose=" << finalAutoCloseObserved
                        << ", music=" << finalMusicObserved
                        << ", main="
                        << (menuStack.back() == MenuScreen::Main)
                        << '\n';
                    runtimeSmokeFailed = true;
                }
                else
                {
                    std::cout
                        << "Source FinalMenu smoke passed after "
                        << renderedFrames
                        << " frames: nine source slides, fade timing, "
                           "sectioned scrolling credits, Back, "
                           "TrackFinal.ogg and 107-second return verified\n";
                }
            }
#ifdef RRR3D_PHYSICS
            else if (options->gamersFrameSmokeTest)
            {
                if (!gamersFrameObserved ||
                    !gamersPlanet3DObserved ||
                    !gamersSelectionChangedObserved ||
                    !gamersGarageObserved || gamersSmokeStep != 4U ||
                    profileState.player.gamerId != 4U ||
                    menuStack.back() != MenuScreen::Garage)
                {
                    std::cerr
                        << "Source GamersFrame smoke failed: frame/3D="
                        << gamersFrameObserved << '/'
                        << gamersPlanet3DObserved
                        << ", selection/garage="
                        << gamersSelectionChangedObserved << '/'
                        << gamersGarageObserved << ", steps="
                        << gamersSmokeStep << ", gamerId="
                        << profileState.player.gamerId << '\n';
                    runtimeSmokeFailed = true;
                }
                else
                {
                    std::cout
                        << "Source GamersFrame smoke passed after "
                        << renderedFrames
                        << " frames: seven source gamers, achievement gate, "
                           "rotating planet, photo/text layout, navigation, "
                           "gamerId and Garage transition verified without "
                           "profile writes\n";
                }
            }
            else if (options->raceRenderSmokeTest)
            {
                auto passObserved =
                    [&](r3d::renderer::RenderPass pass,
                        bool requireDraw = true) {
                        const auto index =
                            static_cast<std::size_t>(pass);
                        return maximumRacePassBegins[index] > 0U &&
                               (!requireDraw ||
                                maximumRacePassDraws[index] > 0U);
                    };
                const auto& smokeQuality = profileState.config.quality;
                const bool expectsTrueReflections =
                    smokeQuality.light >= 2U &&
                    originalRace->environment.dynamicReflectionsEnabled;
                const bool expectsShadows =
                    smokeQuality.shadow >=
                        originalRace->environment
                            .directionalShadowMinimumQuality &&
                    originalRace->environment.directionalLightEnabled;
                const bool weatherAllowsPostEffects =
                    originalRace->environment.weather !=
                    r3d::game::originalrace::Weather::Night;
                const bool expectsBloom =
                    smokeQuality.postEffect >= 1U &&
                    weatherAllowsPostEffects;
                const bool expectsRefraction =
                    smokeQuality.postEffect >= 1U;
                const bool expectsHdr =
                    smokeQuality.postEffect >= 2U &&
                    weatherAllowsPostEffects;
                const bool expectsSunShaft =
                    smokeQuality.postEffect >= 2U &&
                    weatherAllowsPostEffects &&
                    originalRace->environment.directionalLightEnabled;
                bool renderGraphComplete =
                    passObserved(r3d::renderer::RenderPass::Scene) &&
                    passObserved(
                        r3d::renderer::RenderPass::Composite) &&
                    passObserved(
                        r3d::renderer::RenderPass::Overlay, false);
                if (expectsTrueReflections)
                {
                    renderGraphComplete =
                        renderGraphComplete &&
                        passObserved(
                            r3d::renderer::RenderPass::
                                EnvironmentPositiveX) &&
                        passObserved(
                            r3d::renderer::RenderPass::
                                EnvironmentNegativeX) &&
                        passObserved(
                            r3d::renderer::RenderPass::
                                EnvironmentPositiveY) &&
                        passObserved(
                            r3d::renderer::RenderPass::
                                EnvironmentNegativeY) &&
                        passObserved(
                            r3d::renderer::RenderPass::
                                EnvironmentPositiveZ) &&
                        passObserved(
                            r3d::renderer::RenderPass::
                                EnvironmentNegativeZ);
                }
                if (expectsShadows)
                {
                    renderGraphComplete =
                        renderGraphComplete &&
                        passObserved(
                            r3d::renderer::RenderPass::Shadow) &&
                        passObserved(
                            r3d::renderer::RenderPass::ShadowFar);
                }
                if (expectsHdr)
                {
                    renderGraphComplete =
                        renderGraphComplete &&
                        passObserved(
                            r3d::renderer::RenderPass::Luminance64) &&
                        passObserved(
                            r3d::renderer::RenderPass::Luminance16) &&
                        passObserved(
                            r3d::renderer::RenderPass::Luminance4) &&
                        passObserved(
                            r3d::renderer::RenderPass::Luminance1) &&
                        passObserved(
                            r3d::renderer::RenderPass::LuminanceAdapt);
                }
                if (expectsBloom)
                {
                    renderGraphComplete =
                        renderGraphComplete &&
                        passObserved(
                            r3d::renderer::RenderPass::BloomExtract) &&
                        passObserved(
                            r3d::renderer::RenderPass::BloomHorizontal) &&
                        passObserved(
                            r3d::renderer::RenderPass::BloomVertical);
                }
                if (expectsRefraction)
                {
                    renderGraphComplete =
                        renderGraphComplete &&
                        passObserved(
                            r3d::renderer::RenderPass::RefractionCopy) &&
                        passObserved(
                            r3d::renderer::RenderPass::Refraction,
                            false);
                }
                else
                {
                    const auto copyPass = static_cast<std::size_t>(
                        r3d::renderer::RenderPass::RefractionCopy);
                    const auto refractionPass = static_cast<std::size_t>(
                        r3d::renderer::RenderPass::Refraction);
                    renderGraphComplete =
                        renderGraphComplete &&
                        maximumRacePassBegins[copyPass] == 0U &&
                        maximumRacePassBegins[refractionPass] == 0U;
                }
                if (expectsSunShaft)
                {
                    const auto blurPass = static_cast<std::size_t>(
                        r3d::renderer::RenderPass::SunShaftBlur);
                    renderGraphComplete =
                        renderGraphComplete &&
                        passObserved(
                            r3d::renderer::RenderPass::ToneMap) &&
                        passObserved(
                            r3d::renderer::RenderPass::SunShaftPrepare) &&
                        maximumRacePassBegins[blurPass] >= 8U &&
                        maximumRacePassDraws[blurPass] >= 8U;
                }
                else
                {
                    const auto preparePass = static_cast<std::size_t>(
                        r3d::renderer::RenderPass::SunShaftPrepare);
                    const auto blurPass = static_cast<std::size_t>(
                        r3d::renderer::RenderPass::SunShaftBlur);
                    renderGraphComplete =
                        renderGraphComplete &&
                        maximumRacePassBegins[preparePass] == 0U &&
                        maximumRacePassBegins[blurPass] == 0U;
                }
                const bool expectsReflection =
                    originalRace->environment.planarReflection ||
                    (smokeQuality.environment >= 1U &&
                     originalRace->environment.surface ==
                        r3d::game::originalrace::
                            EnvironmentSurface::Water);
                const bool expectsWater =
                    smokeQuality.environment >= 1U &&
                    originalRace->environment.surface ==
                    r3d::game::originalrace::
                        EnvironmentSurface::Water;
                const bool expectsVolumeSurface =
                    smokeQuality.environment >= 1U &&
                    (originalRace->environment.surface ==
                         r3d::game::originalrace::
                             EnvironmentSurface::GroundFog ||
                     originalRace->environment.surface ==
                         r3d::game::originalrace::
                             EnvironmentSurface::Magma);
                const bool expectsBumpMapping =
                    std::any_of(
                        originalRace->trackDefinitions.begin(),
                        originalRace->trackDefinitions.end(),
                        [](const auto& definition) {
                            return definition.lighting ==
                                   r3d::game::originalrace::
                                       LightingMode::Bump;
                        });
                // FxTrail is driven by the source per-wheel slip thresholds,
                // not by vehicle speed. The canonical World1 smoke validates
                // its render path; a high-grip surface is allowed to finish
                // this short run without inventing a skid solely for test
                // coverage.
                const bool expectsWheelSlipTrail =
                    originalRace->levelPath ==
                    "Data/Map/World1/map1.r3dMap";
                const bool expectsHeadlights =
                    originalRace->environment.weather ==
                        r3d::game::originalrace::Weather::Night;
                const std::size_t competitiveAiCount =
                    static_cast<std::size_t>(std::count_if(
                        maximumRaceAiSpeeds.begin() +
                            std::min<std::size_t>(
                                1U, maximumRaceAiSpeeds.size()),
                        maximumRaceAiSpeeds.end(),
                        [](float speed) { return speed >= 25.0F; }));
                const std::size_t progressingAiCount =
                    static_cast<std::size_t>(std::count_if(
                        maximumRaceAiProgress.begin() +
                            std::min<std::size_t>(
                                1U, maximumRaceAiProgress.size()),
                        maximumRaceAiProgress.end(),
                        [](float progress) { return progress >= 0.5F; }));
                const std::size_t expectedCompetitiveAi =
                    options->smokeFrames >= 1800U &&
                            originalRace->levelPath ==
                                "Data/Map/World1/map1.r3dMap" &&
                            maximumRaceAiSpeeds.size() > 1U
                        ? std::min<std::size_t>(
                              3U, maximumRaceAiSpeeds.size() - 1U)
                        : 0U;
                const std::size_t aheadAiCount =
                    raceSession.racers().empty()
                        ? 0U
                        : static_cast<std::size_t>(std::count_if(
                              raceSession.racers().begin() + 1U,
                              raceSession.racers().end(),
                              [&](const auto& racer) {
                                  return !racer.disconnected &&
                                         racer.place <
                                             raceSession.racers().front().place;
                              }));
                const std::size_t expectedAheadAi =
                    expectedCompetitiveAi > 0U ? 1U : 0U;
                const auto expectedHeadlightCount =
                    static_cast<std::uint32_t>(
                        originalRace->racers.size() +
                        std::count_if(
                            originalRace->racers.begin(),
                            originalRace->racers.end(),
                            [](const auto& racer) {
                                return racer.human;
                            }));
                if (expectsReflection)
                    renderGraphComplete =
                        renderGraphComplete &&
                        passObserved(
                            r3d::renderer::RenderPass::Reflection);
                if (expectsWater || expectsVolumeSurface)
                    renderGraphComplete =
                        renderGraphComplete &&
                        passObserved(
                            r3d::renderer::RenderPass::Water);
                if (!integratedRaceStartObserved || !inRace ||
                    !raceLoadingFrameObserved ||
                    !raceLoadingDeferredObserved ||
                    !gameModeFrameObserved ||
                    !tournamentFrameObserved ||
                    !profileFrameObserved ||
                    !profileDeleteDialogObserved ||
                    !raceMainFrameObserved ||
                    !raceMain3DObserved ||
                    !raceMainSourceVisualsObserved ||
                    !raceGarageFrameObserved ||
                    !raceGarage3DObserved ||
                    !raceWorkshopFrameObserved ||
                    !raceWorkshop3DObserved ||
                    !raceWorkshopWeaponDialogObserved ||
                    !raceInfoDialogObserved ||
                    !raceAngarFrameObserved ||
                    !raceAngar3DObserved ||
                    !raceAchievementFrameObserved ||
#ifdef RRR3D_AUDIO
                    !raceMusicDialogObserved ||
#endif
                    !racePauseDialogObserved ||
                    !racePauseResumeObserved ||
                    !racePauseFrozenObserved ||
                    !raceChatInputObserved ||
                    !raceChatLineObserved ||
                    raceChatSmokeStep != 4U ||
                    racePlayerDestroyedObserved ||
                    minimumRacePlayerLife <= 0.0F ||
                    maximumRaceSmokeContacts == 0 ||
                    maximumRaceSmokeSpeed < 0.2F ||
                    competitiveAiCount < expectedCompetitiveAi ||
                    progressingAiCount < expectedCompetitiveAi ||
                    aheadAiCount < expectedAheadAi ||
                    raceVehicles.size() < 2U ||
                    !raceCameraStylesObserved[0] ||
                    !raceCameraStylesObserved[1] ||
                    !renderGraphComplete ||
                    maximumEnvironmentMappedDraws == 0U ||
                    (expectsBumpMapping &&
                     maximumNormalMappedDraws == 0U) ||
                    (expectsWheelSlipTrail &&
                     maximumTransientDraws == 0U) ||
                    (expectsHeadlights &&
                     maximumActiveSpotLights <
                         expectedHeadlightCount))
                {
                    std::cerr
                        << "Milestone 9 integrated Single Player/race render "
                           "verification failed: started="
                        << integratedRaceStartObserved << ", inRace="
                        << inRace << ", loading="
                        << raceLoadingFrameObserved << '/'
                        << raceLoadingDeferredObserved
                        << ", gameMode/tournament="
                        << gameModeFrameObserved << '/'
                        << tournamentFrameObserved
                        << ", profile/dialog="
                        << profileFrameObserved << '/'
                        << profileDeleteDialogObserved
                        << ", raceMain="
                        << raceMainFrameObserved << '/'
                        << raceMain3DObserved << '/'
                        << raceMainSourceVisualsObserved
                        << ", garage="
                        << raceGarageFrameObserved << '/'
                        << raceGarage3DObserved
                        << ", workshop="
                        << raceWorkshopFrameObserved << '/'
                        << raceWorkshop3DObserved << '/'
                        << raceWorkshopWeaponDialogObserved << '/'
                        << raceInfoDialogObserved
                        << ", angar="
                        << raceAngarFrameObserved << '/'
                        << raceAngar3DObserved
                        << ", achievement="
                        << raceAchievementFrameObserved
#ifdef RRR3D_AUDIO
                        << ", musicDialog="
                        << raceMusicDialogObserved
#endif
                        << ", pause="
                        << racePauseDialogObserved << '/'
                        << racePauseResumeObserved << '/'
                        << racePauseFrozenObserved << ", chat="
                        << raceChatInputObserved << '/'
                        << raceChatLineObserved << '/'
                        << raceChatSmokeStep << ", destroyed="
                        << racePlayerDestroyedObserved << ", minLife="
                        << minimumRacePlayerLife << ", contacts="
                        << maximumRaceSmokeContacts << ", maxSpeed="
                        << maximumRaceSmokeSpeed
                        << ", competitive/progressing AI="
                        << competitiveAiCount << '/'
                        << progressingAiCount << '/'
                        << expectedCompetitiveAi
                        << ", AI ahead=" << aheadAiCount << '/'
                        << expectedAheadAi
                        << ", renderGraph="
                        << renderGraphComplete
                        << ", envMapped="
                        << maximumEnvironmentMappedDraws
                        << ", normalMapped="
                        << maximumNormalMappedDraws
                        << ", transient="
                        << maximumTransientDraws
                        << ", spotLights="
                        << maximumActiveSpotLights << '/'
                        << expectedHeadlightCount
                        << ", cars=" << raceVehicles.size()
                        << ", cameras="
                        << raceCameraStylesObserved[0] << '/'
                        << raceCameraStylesObserved[1] << '\n';
                    runtimeSmokeFailed = true;
                }
                else
                {
                    std::cout
                        << "Milestone 9.5 original Single Player/"
                        << originalRace->levelPath << '/'
                        << recordName(originalRace->vehicle.record)
                        << "/Jolt/bgfx/Metal smoke test completed after "
                        << renderedFrames << " frames; max speed "
                        << maximumRaceSmokeSpeed << ", wheel contacts "
                        << maximumRaceSmokeContacts
                        << ", AI=";
                    for (std::size_t index = 1U;
                         index < maximumRaceAiSpeeds.size(); ++index)
                    {
                        std::cout << (index == 1U ? "" : ";")
                                  << index << ':'
                                  << maximumRaceAiSpeeds[index] << '/'
                                  << maximumRaceAiProgress[index] << '/'
                                  << raceAiThrottleFrames[index] << '/'
                                  << raceAiBrakeFrames[index] << '/'
                                  << raceAiReverseFrames[index];
                    }
                    std::cout
                        << ", ahead=" << aheadAiCount
                        << ", renderer passes cube6/shadow2/scene/HDR64-1/"
                           "adapt/bloom/composite/HUD"
                        << "/loadingFrame"
                        << (expectsReflection ? "/reflection" : "")
                        << (expectsWater ? "/water" : "")
                        << (expectsVolumeSurface ? "/volume-surface" : "")
                        << (expectsRefraction ? "/refraction" : "")
                        << (expectsSunShaft ? "/sun-shaft8" : "")
                        << " verified; cube reflection "
                        << maximumEnvironmentMappedDraws
                        << ", normal map "
                        << maximumNormalMappedDraws
                        << ", FxTrail "
                        << maximumTransientDraws << "; "
                        << raceVehicles.size()
                        << " cars, both original camera modes and "
                           "an unshielded surviving player start (minimum "
                           "life "
                        << minimumRacePlayerLife << "), "
                           "source HudMenu pause/accept/frozen-world, "
                           "source UserChat Enter/input/history/fade, "
                           "source GameModeFrame/TournamentFrame layout, "
                           "source ProfileFrame/delete dialog, "
                           "source RaceMain portraits/boss/loadout/stats, "
                           "source WorkshopFrame/GarageFrame/3D CarFrame/"
                           "SpaceshipFrame/AngarFrame/AchievmentFrame and "
                           "render-target "
                           "resize "
                           "round-trip passed\n";
                }
            }
            else if (options->finishMenuSmokeTest)
            {
                if (!finishMenuFrameObserved ||
                    finishRows.size() != 3U
#ifdef RRR3D_AUDIO
                    || !finishLastVoiceDispatched
#endif
                )
                {
                    std::cerr
                        << "Source FinishMenu renderer smoke failed: "
                        << "observed=" << finishMenuFrameObserved
                        << ", rows=" << finishRows.size()
#ifdef RRR3D_AUDIO
                        << ", lastVoice="
                        << finishLastVoiceDispatched
#endif
                        << '\n';
                    runtimeSmokeFailed = true;
                }
                else
                {
                    std::cout
                        << "Source FinishMenu renderer smoke passed after "
                        << renderedFrames
                        << " frames: player frames, photos, cups, "
                           "Money/Points, picked-money, alternating reveal, "
                           "global commentator queue and last-place event "
                           "verified without profile writes\n";
                }
            }
#endif
            else
#ifdef RRR3D_AUDIO
            if (!integratedAudioInputObserved ||
                !menuMusicDialogObserved)
            {
                std::cerr << "Milestone 8 integrated input/MainMenu2/audio "
                             "dispatch/source MusicDialog was not observed: "
                          << "input=" << integratedAudioInputObserved
                          << ", musicDialog="
                          << menuMusicDialogObserved << '\n';
                runtimeSmokeFailed = true;
            }
            else
            {
                std::cout << "Milestone 8 original MainMenu2/input/audio/"
                             "MusicCat/source MusicDialog/bgfx/Metal smoke "
                             "test completed after "
                          << renderedFrames << " frames\n";
            }
#elif defined(RRR3D_GAMEPAD_INPUT)
            std::cout << "Milestone 7 original MainMenu2/input/bgfx/Metal "
                         "smoke test completed after "
#else
            std::cout << "Milestone 6 original MainMenu2/bgfx/Metal smoke "
                         "test completed after "
#endif
#ifndef RRR3D_AUDIO
                      << renderedFrames << " frames\n";
#endif
            running = false;
        }
    }

#ifdef RRR3D_AUDIO
#ifdef RRR3D_PHYSICS
    stopRaceAudio(false);
    commentator.shutdown();
    gameMusic.shutdown();
    for (const auto& [path, sound] : engineSounds)
    {
        static_cast<void>(path);
        audio.unloadSound(sound);
    }
#endif
    finalMusic.shutdown();
    music.shutdown();
    menuSounds.shutdown();
    audio.shutdown();
    if (options->audioSmokeTest)
    {
        std::error_code removeError;
        std::filesystem::remove(musicStatePath, removeError);
        auto temporary = musicStatePath;
        temporary += ".tmp";
        std::filesystem::remove(temporary, removeError);
    }
#endif
#ifdef RRR3D_PHYSICS
    if (!options->finishMenuSmokeTest &&
        !options->gamersFrameSmokeTest &&
        !options->finalMenuSmokeTest)
        saveRaceProfile();
    if (userChat.inputVisible())
        SDL_StopTextInput(window);
    destroyUserChatVisual(*device, userChatVisual);
    physicsWorld.reset();
    raceHud.shutdown(*device);
    workshopRenderer.shutdown(*device);
    angarRenderer.shutdown(*device);
    garageRenderer.shutdown(*device);
    raceRenderer.shutdown(*device);
#endif
    releaseResources();
    device.reset();
    SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
    input.shutdown();
#endif
    SDL_Quit();
    return runtimeSmokeFailed ? EXIT_FAILURE : EXIT_SUCCESS;
}
