#include "CoreTextRasterizer.h"
#include "OriginalAudioSpec.h"
#include "OriginalDialogMenu.h"
#include "OriginalGameData.h"
#include "OriginalMainMenu.h"
#include "OriginalMenuSystem.h"
#include "OriginalOptionsMenu.h"
#include "OriginalRaceMenu.h"
#ifdef RRR3D_NETWORK
#include "OriginalNetwork.h"
#endif
#ifdef RRR3D_PHYSICS
#include "OriginalEnvironment.h"
#include "OriginalGameDebug.h"
#include "OriginalGameMode.h"
#include "OriginalGarage.h"
#include "OriginalProfile.h"
#include "OriginalRace.h"
#include "OriginalRaceHud.h"
#include "OriginalRaceRenderer.h"
#include "OriginalRaceSession.h"
#include "OriginalUserChat.h"
#include "OriginalWorkshopRenderer.h"
#include "OriginalWorld.h"
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
#include <unordered_set>
#include <vector>

namespace
{

using namespace r3d::renderer;
namespace menu = r3d::game::mainmenu2;
namespace originalaudio = r3d::game::originalaudio;
namespace originalgamedata = r3d::game::originalgamedata;
namespace originalmenu = r3d::game::originalmenu;

constexpr int initialWidth = 1280;
constexpr int initialHeight = 733;
constexpr std::uint32_t originalMinimumWidth = 1280U;
constexpr std::uint32_t originalMinimumHeight = 720U;
constexpr std::uint32_t originalMaximumWidth = 1920U;
constexpr std::uint32_t originalMaximumHeight = 1080U;
constexpr std::string_view smokePrefix = "--smoke-test-frames=";
constexpr std::string_view dataPrefix = "--data-dir=";
constexpr std::string_view languagePrefix = "--language=";
constexpr std::string_view trackPrefix = "--track=";
constexpr std::string_view carPrefix = "--car=";
constexpr std::string_view weatherPrefix = "--weather=";

struct OriginalDisplayMode
{
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    SDL_DisplayMode sdl{};
    bool hasSdlMode = false;
};

constexpr int originalOptionsArrowDirection(
    float pointerX, float centerX, float halfWidth) noexcept
{
    const float left = centerX + 140.0F;
    const float right = centerX + 380.0F;
    if (pointerX >= left - halfWidth &&
        pointerX <= left + halfWidth)
        return -1;
    if (pointerX >= right - halfWidth &&
        pointerX <= right + halfWidth)
        return 1;
    return 0;
}

static_assert(originalOptionsArrowDirection(1100.0F, 960.0F, 20.0F) ==
              -1);
static_assert(originalOptionsArrowDirection(1340.0F, 960.0F, 20.0F) ==
              1);

std::pair<std::uint32_t, std::uint32_t> displayModePixelSize(
    const SDL_DisplayMode& mode) noexcept
{
    const float density =
        std::isfinite(mode.pixel_density) && mode.pixel_density > 0.0F
            ? mode.pixel_density
            : 1.0F;
    return {
        static_cast<std::uint32_t>(std::max(
            std::lround(static_cast<float>(mode.w) * density), 1L)),
        static_cast<std::uint32_t>(std::max(
            std::lround(static_cast<float>(mode.h) * density), 1L))};
}

bool sourceDisplayMode(const OriginalDisplayMode& mode) noexcept
{
    return mode.width >= originalMinimumWidth &&
           mode.width <= originalMaximumWidth &&
           mode.height >= originalMinimumHeight &&
           mode.height <= originalMaximumHeight;
}

std::vector<OriginalDisplayMode> originalDisplayModesForWindow(
    SDL_Window* window)
{
    std::vector<OriginalDisplayMode> candidates;
    const SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    std::optional<OriginalDisplayMode> nativeMode;
    if (const SDL_DisplayMode* desktop =
            SDL_GetDesktopDisplayMode(display);
        desktop != nullptr && desktop->w > 0 && desktop->h > 0)
    {
        const auto [width, height] = displayModePixelSize(*desktop);
        nativeMode = OriginalDisplayMode{
            width, height, *desktop, true};
    }
    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(
        display, &count);
    candidates.reserve(static_cast<std::size_t>(std::max(count, 0)));
    for (int index = 0; modes != nullptr && index < count; ++index)
    {
        if (modes[index] == nullptr || modes[index]->w <= 0 ||
            modes[index]->h <= 0)
        {
            continue;
        }
        const auto [width, height] = displayModePixelSize(*modes[index]);
        candidates.push_back(
            {width, height, *modes[index], true});
    }
    SDL_free(modes);

    // D3D9RenderDriver keeps 1280x720..1920x1080. If an adapter has no
    // mode inside that range, it falls back to the largest smaller and the
    // smallest larger mode. Keep that policy, but compare physical pixels:
    // SDL's macOS mode sizes are logical points and pixel_density carries
    // the Retina multiplier.
    std::vector<OriginalDisplayMode> filtered;
    for (const auto& candidate : candidates)
    {
        if (sourceDisplayMode(candidate))
            filtered.push_back(candidate);
    }
    if (filtered.empty() && !candidates.empty())
    {
        const auto area = [](const OriginalDisplayMode& mode) {
            return static_cast<std::uint64_t>(mode.width) * mode.height;
        };
        const OriginalDisplayMode* below = nullptr;
        const OriginalDisplayMode* above = nullptr;
        for (const auto& candidate : candidates)
        {
            if (candidate.width < originalMinimumWidth ||
                candidate.height < originalMinimumHeight)
            {
                if (below == nullptr || area(candidate) > area(*below))
                    below = &candidate;
            }
            if (candidate.width > originalMaximumWidth ||
                candidate.height > originalMaximumHeight)
            {
                if (above == nullptr || area(candidate) < area(*above))
                    above = &candidate;
            }
        }
        if (below != nullptr)
            filtered.push_back(*below);
        if (above != nullptr)
            filtered.push_back(*above);
    }

    // D3D9RenderDriver::FindPrefRate selects the available rate nearest
    // 60 Hz, then removes the rest. SDL reports fractional 59.94 Hz, so use
    // a small tolerance around the nearest value.
    float preferredRate = 0.0F;
    float preferredDistance = std::numeric_limits<float>::max();
    for (const auto& mode : filtered)
    {
        if (mode.sdl.refresh_rate <= 0.0F)
            continue;
        const float distance = std::abs(mode.sdl.refresh_rate - 60.0F);
        if (distance < preferredDistance)
        {
            preferredDistance = distance;
            preferredRate = mode.sdl.refresh_rate;
        }
    }

    std::vector<OriginalDisplayMode> preferredCandidates;
    for (const auto& mode : filtered)
    {
        if (preferredRate > 0.0F &&
            (mode.sdl.refresh_rate <= 0.0F ||
             std::abs(mode.sdl.refresh_rate - preferredRate) > 0.25F))
        {
            continue;
        }
        const auto duplicate = std::find_if(
            preferredCandidates.begin(), preferredCandidates.end(),
            [&](const auto& existing) {
                return existing.width == mode.width &&
                       existing.height == mode.height;
            });
        if (duplicate == preferredCandidates.end())
            preferredCandidates.push_back(mode);
    }

    // A modern Retina panel exposes a near-continuous ladder of synthetic
    // scaled modes (1313x820, 1348x842, ...). D3D9 adapters exposed the
    // conventional video modes that the source OptionsFrame expected. Keep
    // those source-facing values in the menu and associate each with the
    // closest real Cocoa mode using the same area-distance rule as
    // RenderDriver::FindNearMode. This also preserves the shipped 1280x800
    // profile value instead of silently replacing it with a Retina scale.
    constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 10>
        conventionalModes{{
            {1280U, 720U},
            {1280U, 800U},
            {1366U, 768U},
            {1280U, 960U},
            {1440U, 900U},
            {1280U, 1024U},
            {1600U, 900U},
            {1600U, 1024U},
            {1680U, 1050U},
            {1920U, 1080U},
        }};
    std::vector<OriginalDisplayMode> result;
    result.reserve(
        conventionalModes.size() + (nativeMode.has_value() ? 1U : 0U));
    for (const auto [width, height] : conventionalModes)
    {
        if (preferredCandidates.empty())
        {
            result.push_back({width, height, {}, false});
            continue;
        }
        const auto wantedArea =
            static_cast<std::uint64_t>(width) * height;
        const auto closest = std::min_element(
            preferredCandidates.begin(), preferredCandidates.end(),
            [&](const auto& left, const auto& right) {
                const auto leftArea =
                    static_cast<std::uint64_t>(left.width) * left.height;
                const auto rightArea =
                    static_cast<std::uint64_t>(right.width) * right.height;
                const auto leftDistance =
                    leftArea > wantedArea ? leftArea - wantedArea
                                          : wantedArea - leftArea;
                const auto rightDistance =
                    rightArea > wantedArea ? rightArea - wantedArea
                                           : wantedArea - rightArea;
                return leftDistance < rightDistance;
            });
        result.push_back({width, height, closest->sdl, true});
    }
    // The Windows renderer capped the adapter list at 1920x1080, but the
    // native macOS port must also expose the physical pixel size of the
    // active Retina display.  Use SDL's desktop mode instead of the largest
    // enumerated mode: on a scaled MacBook desktop it carries the logical
    // size together with the exact backing-pixel density (for example
    // 1728x1117 points at 2x is the panel-native 3456x2234 mode).
    if (nativeMode.has_value() &&
        std::none_of(
            result.begin(), result.end(),
            [&](const OriginalDisplayMode& mode) {
                return mode.width == nativeMode->width &&
                       mode.height == nativeMode->height;
            }))
    {
        result.push_back(*nativeMode);
    }
    std::sort(result.begin(), result.end(), [](const auto& left,
                                                const auto& right) {
        const auto leftArea =
            static_cast<std::uint64_t>(left.width) * left.height;
        const auto rightArea =
            static_cast<std::uint64_t>(right.width) * right.height;
        if (leftArea != rightArea)
            return leftArea < rightArea;
        if (left.width != right.width)
            return left.width < right.width;
        return left.height < right.height;
    });
    return result;
}

std::size_t nearestOriginalDisplayMode(
    const std::vector<OriginalDisplayMode>& modes,
    std::uint32_t width, std::uint32_t height) noexcept
{
    if (modes.empty())
        return 0U;
    const auto wantedArea = static_cast<std::uint64_t>(width) * height;
    std::size_t best = 0U;
    std::uint64_t bestDistance = std::numeric_limits<std::uint64_t>::max();
    for (std::size_t index = 0U; index < modes.size(); ++index)
    {
        if (modes[index].width == width && modes[index].height == height)
            return index;
        const auto area = static_cast<std::uint64_t>(modes[index].width) *
                          modes[index].height;
        const auto distance = area > wantedArea ? area - wantedArea
                                                 : wantedArea - area;
        if (distance < bestDistance)
        {
            best = index;
            bestDistance = distance;
        }
    }
    return best;
}

bool applyOriginalWindowMode(
    SDL_Window* window, const std::vector<OriginalDisplayMode>& modes,
    std::uint32_t width, std::uint32_t height, bool fullscreen,
    bool synchronize, std::string& error)
{
    if (fullscreen)
    {
        const auto index = nearestOriginalDisplayMode(
            modes, width, height);
        const SDL_DisplayMode* mode =
            index < modes.size() && modes[index].hasSdlMode
                ? &modes[index].sdl
                : nullptr;
        if (!SDL_SetWindowFullscreenMode(window, mode) ||
            !SDL_SetWindowFullscreen(window, true))
        {
            error = SDL_GetError();
            return false;
        }
    }
    else
    {
        if ((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0U &&
            !SDL_SetWindowFullscreen(window, false))
        {
            error = SDL_GetError();
            return false;
        }
        if (synchronize && !SDL_SyncWindow(window))
        {
            error = SDL_GetError();
            return false;
        }
        const float queriedDensity = SDL_GetWindowPixelDensity(window);
        const float density =
            std::isfinite(queriedDensity) && queriedDensity > 0.0F
                ? queriedDensity
                : 1.0F;
        const int windowWidth = std::max(
            static_cast<int>(std::lround(width / density)), 1);
        const int windowHeight = std::max(
            static_cast<int>(std::lround(height / density)), 1);
        if (!SDL_SetWindowSize(window, windowWidth, windowHeight))
        {
            error = SDL_GetError();
            return false;
        }
    }
    if (synchronize && !SDL_SyncWindow(window))
    {
        error = SDL_GetError();
        return false;
    }
    return true;
}

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
    bool gameDebug = false;
    bool legacyWindowsDebug = false;
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
const auto& originalAchievementVisuals =
    r3d::game::originalracemenu::AchievementFrameState::definitions();
constexpr std::size_t originalAchievementBack =
    r3d::game::originalracemenu::AchievementFrameState::backFocus;

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
};

struct InfoDialogVisual
{
    TextVisual title;
    std::vector<TextVisual> info;
    TextVisual ok;
};

struct AcceptDialogVisual
{
    std::vector<TextVisual> info;
    TextVisual yes;
    TextVisual no;
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

std::vector<std::size_t> musicPlaylist(std::string_view source)
{
    std::vector<std::size_t> result;
    while (!source.empty())
    {
        const auto separator = source.find(',');
        const auto token = source.substr(0, separator);
        long long index = 0;
        const auto parsed = std::from_chars(
            token.data(), token.data() + token.size(), index);
        if (parsed.ec == std::errc{} &&
            parsed.ptr == token.data() + token.size())
        {
            result.push_back(
                index < 0
                    ? std::numeric_limits<std::size_t>::max()
                    : static_cast<std::size_t>(index));
        }
        if (separator == std::string_view::npos)
            break;
        source.remove_prefix(separator + 1U);
    }
    return result;
}

std::string musicPlaylistString(const std::vector<std::size_t>& playlist)
{
    std::string result;
    for (const auto track : playlist)
    {
        if (!result.empty())
            result.push_back(',');
        result += std::to_string(track);
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
        if (argument == "--game-debug")
        {
            options.gameDebug = true;
            continue;
        }
        if (argument == "--legacy-windows-debug")
        {
            options.legacyWindowsDebug = true;
            continue;
        }
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
bool applyArmor4Presentation(
    r3d::game::originalrace::OriginalGarageCatalog& catalog) noexcept
{
    const auto armor = std::find_if(
        catalog.workshop.begin(), catalog.workshop.end(),
        [](const auto& item) {
            return recordName(item.record) == "armor3";
        });
    if (armor == catalog.workshop.end())
        return false;
    armor->name = "scArmor4";
    armor->info = "scArmor4Info";
    armor->meshPath = "Data/Upgrade/armor4.r3d";
    armor->texturePath = "Data/Upgrade/armor4.dds";
    return true;
}

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
            wheel.position.x + vehicle.wheelVisualOffsets[index].x,
            wheel.position.y + vehicle.wheelVisualOffsets[index].y,
            result.body.position.z + wheel.position.z -
                0.5F * wheel.suspensionTravel +
                vehicle.wheelVisualOffsets[index].z};
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

struct SourceAutoObserverState
{
    r3d::game::originalrace::PresentationCamera source;
    r3d::physics::Quat targetRotation{};
    r3d::physics::Quat cameraRotation{};
    float yaw = 0.0F;
    float pitch = 0.0F;
    float direction = 1.0F;
    float idleSeconds = 3.0F;
    float anchorX = 0.0F;
    float anchorY = 0.0F;
    float lastX = 0.0F;
    float lastY = 0.0F;
    bool initialized = false;
    bool leftDown = false;
    bool dragging = false;
};

r3d::physics::Quat normalizeObserverQuat(
    const r3d::physics::Quat& value) noexcept
{
    const float length = std::sqrt(
        value.x * value.x + value.y * value.y +
        value.z * value.z + value.w * value.w);
    if (length <= 0.000001F)
        return {0.0F, 0.0F, 0.0F, 1.0F};
    return {
        value.x / length, value.y / length,
        value.z / length, value.w / length};
}

r3d::physics::Quat multiplyObserverQuat(
    const r3d::physics::Quat& left,
    const r3d::physics::Quat& right) noexcept
{
    return normalizeObserverQuat({
        left.w * right.x + left.x * right.w +
            left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z +
            left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y -
            left.y * right.x + left.z * right.w,
        left.w * right.w - left.x * right.x -
            left.y * right.y - left.z * right.z});
}

r3d::physics::Quat observerAngleAxis(
    float angle, const r3d::physics::Vec3& axis) noexcept
{
    const float length = std::sqrt(
        axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
    if (length <= 0.000001F)
        return {0.0F, 0.0F, 0.0F, 1.0F};
    const float sine = std::sin(angle * 0.5F) / length;
    return normalizeObserverQuat({
        axis.x * sine, axis.y * sine, axis.z * sine,
        std::cos(angle * 0.5F)});
}

r3d::physics::Vec3 rotateObserverVector(
    const r3d::physics::Vec3& value,
    const r3d::physics::Quat& rotation) noexcept
{
    const r3d::physics::Vec3 q{rotation.x, rotation.y, rotation.z};
    const r3d::physics::Vec3 twiceCross{
        2.0F * (q.y * value.z - q.z * value.y),
        2.0F * (q.z * value.x - q.x * value.z),
        2.0F * (q.x * value.y - q.y * value.x)};
    return {
        value.x + rotation.w * twiceCross.x +
            (q.y * twiceCross.z - q.z * twiceCross.y),
        value.y + rotation.w * twiceCross.y +
            (q.z * twiceCross.x - q.x * twiceCross.z),
        value.z + rotation.w * twiceCross.z +
            (q.x * twiceCross.y - q.y * twiceCross.x)};
}

r3d::physics::Quat slerpObserverQuat(
    r3d::physics::Quat from, r3d::physics::Quat to,
    float alpha) noexcept
{
    from = normalizeObserverQuat(from);
    to = normalizeObserverQuat(to);
    float dot = from.x * to.x + from.y * to.y +
                from.z * to.z + from.w * to.w;
    if (dot < 0.0F)
    {
        dot = -dot;
        to = {-to.x, -to.y, -to.z, -to.w};
    }
    alpha = std::clamp(alpha, 0.0F, 1.0F);
    if (dot > 0.9995F)
    {
        return normalizeObserverQuat({
            from.x + (to.x - from.x) * alpha,
            from.y + (to.y - from.y) * alpha,
            from.z + (to.z - from.z) * alpha,
            from.w + (to.w - from.w) * alpha});
    }
    const float angle = std::acos(std::clamp(dot, -1.0F, 1.0F));
    const float sine = std::sin(angle);
    const float fromWeight = std::sin((1.0F - alpha) * angle) / sine;
    const float toWeight = std::sin(alpha * angle) / sine;
    return normalizeObserverQuat({
        from.x * fromWeight + to.x * toWeight,
        from.y * fromWeight + to.y * toWeight,
        from.z * fromWeight + to.z * toWeight,
        from.w * fromWeight + to.w * toWeight});
}

void handleSourceAutoObserverPointer(
    SourceAutoObserverState& state, const SDL_Event& event) noexcept
{
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
        event.button.button == SDL_BUTTON_LEFT)
    {
        state.leftDown = true;
        state.dragging = false;
        state.anchorX = state.lastX = event.button.x;
        state.anchorY = state.lastY = event.button.y;
        return;
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
        event.button.button == SDL_BUTTON_LEFT)
    {
        state.leftDown = false;
        state.dragging = false;
        state.lastX = event.button.x;
        state.lastY = event.button.y;
        return;
    }
    if (event.type != SDL_EVENT_MOUSE_MOTION)
        return;
    if (!state.leftDown)
    {
        state.anchorX = state.lastX = event.motion.x;
        state.anchorY = state.lastY = event.motion.y;
        return;
    }
    const float fromAnchorX = event.motion.x - state.anchorX;
    const float fromAnchorY = event.motion.y - state.anchorY;
    if (!state.dragging &&
        std::hypot(fromAnchorX, fromAnchorY) > 15.0F)
    {
        state.dragging = true;
    }
    if (state.dragging)
    {
        const float deltaX = event.motion.x - state.lastX;
        const float deltaY = event.motion.y - state.lastY;
        state.yaw += std::clamp(
            deltaX * bx::kPi * 0.001F,
            -bx::kPiHalf, bx::kPiHalf);
        state.pitch += std::clamp(
            -deltaY * bx::kPi * 0.001F,
            -bx::kPiHalf, bx::kPiHalf);
        state.idleSeconds = 0.0F;
    }
    state.lastX = event.motion.x;
    state.lastY = event.motion.y;
}

r3d::game::originalrace::PresentationCamera
updateSourceAutoObserver(
    SourceAutoObserverState& state,
    const r3d::game::originalrace::PresentationCamera& source,
    float deltaTime, float angularSpeed, float stablePitch,
    float minimumPitch, float maximumPitch,
    float positiveYawClamp = 0.0F,
    float negativeYawClamp = 0.0F) noexcept
{
    if (!state.initialized)
    {
        state.source = source;
        state.targetRotation = source.rotation;
        state.cameraRotation = source.rotation;
        state.idleSeconds = 3.0F;
        state.direction = 1.0F;
        state.initialized = true;
    }
    if (!state.dragging)
        state.idleSeconds += deltaTime;
    if (state.idleSeconds >= 3.0F)
    {
        state.yaw += angularSpeed * state.direction * deltaTime;
        state.pitch = 0.0F;
    }
    if (positiveYawClamp > 0.0F || negativeYawClamp > 0.0F)
    {
        if (state.yaw >= positiveYawClamp)
        {
            state.yaw = positiveYawClamp;
            state.direction = -1.0F;
        }
        else if (state.yaw <= -negativeYawClamp)
        {
            state.yaw = -negativeYawClamp;
            state.direction = 1.0F;
        }
    }
    state.pitch = std::clamp(
        state.pitch,
        minimumPitch - stablePitch,
        maximumPitch - stablePitch);
    const auto yawRotation = observerAngleAxis(
        state.yaw, {0.0F, 0.0F, 1.0F});
    const auto yawedSource = multiplyObserverQuat(
        yawRotation, state.source.rotation);
    const auto localY = rotateObserverVector(
        {0.0F, 1.0F, 0.0F}, yawedSource);
    state.targetRotation = multiplyObserverQuat(
        observerAngleAxis(state.pitch, localY), yawedSource);
    state.cameraRotation = slerpObserverQuat(
        state.cameraRotation, state.targetRotation,
        6.0F * deltaTime);

    const float distance = std::sqrt(
        source.position.x * source.position.x +
        source.position.y * source.position.y +
        source.position.z * source.position.z);
    const auto direction = rotateObserverVector(
        {1.0F, 0.0F, 0.0F}, state.cameraRotation);
    auto result = source;
    result.rotation = state.cameraRotation;
    result.position = {
        -direction.x * distance,
        -direction.y * distance,
        -direction.z * distance};
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
    r3d::game::originalrace::source::Environment::ApplyWeatherToken(
        environment, weather, levelPath);
}

std::string_view weatherToken(
    r3d::game::originalrace::Weather weather) noexcept
{
    return r3d::game::originalrace::source::Environment::WeatherToken(
        weather);
}
#endif

} // namespace

int main(int argc, char** argv)
{
    const auto options = parseOptions(argc, argv);
    if (!options)
    {
        std::cerr << "Usage: RRR3d [--data-dir=PATH] "
                     "[--language=english|russian|portuguese|french|spain|german] "
                     "[--verify-resources] "
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
                     " [--game-debug] [--legacy-windows-debug] "
                     "[--track=0..89] [--car=garage-record] "
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
        options->startupSmokeTest ||
        (options->smokeFrames == 0U &&
#ifdef RRR3D_PHYSICS
         !options->legacyWindowsDebug
#else
         true
#endif
        );

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
    originalgamedata::Catalog originalGameDataCatalog;
#ifdef RRR3D_AUDIO
    originalaudio::MusicCatalog originalMusicCatalog;
#endif
#ifdef RRR3D_PHYSICS
    const bool sourceNormalInteractiveLaunch =
        options->smokeFrames == 0U &&
        !options->verifyResources &&
        !options->physicsSmokeTest;
    const auto runtimeProfileDirectory =
        sourceNormalInteractiveLaunch
            ? rrr3d::platform::save_directory()
            : std::filesystem::temp_directory_path() /
                  ("rrr3d-runtime-profile-smoke-" +
                   std::to_string(reinterpret_cast<std::uintptr_t>(
                       &*options)));
    if (!sourceNormalInteractiveLaunch)
    {
        std::error_code cleanupError;
        std::filesystem::remove_all(
            runtimeProfileDirectory, cleanupError);
    }
    std::optional<r3d::game::originalrace::Race> originalRace;
    std::optional<r3d::game::originalrace::Race> originalGarageScene;
    std::optional<r3d::game::originalrace::Race> originalAngarScene;
    std::optional<r3d::game::originalrace::OriginalGarageCatalog>
        originalGarage;
    std::optional<r3d::physics::WorldDescription> physicsDescription;
    r3d::game::originalrace::OriginalProfileStore profileStore(
        runtimeProfileDirectory, dataDirectory);
    std::string profileWarning;
    auto profileState = profileStore.load(profileWarning);
    if (options->raceRenderSmokeTest &&
        profileState.profiles.empty())
    {
        // The shipped race.xml is intentionally empty.  The integrated
        // fixture must build its own Load/ProfileFrame scenario instead of
        // depending on profiles left in the player's Application Support by
        // an earlier interactive run.
        profileState =
            r3d::game::originalrace::makeOriginalDefaultProfileState();
        std::string seedError;
        if (!profileStore.save(profileState, seedError))
        {
            std::cerr
                << "Unable to seed isolated race-render profile: "
                << seedError << '\n';
            return EXIT_FAILURE;
        }
    }
    const auto achievementOpened =
        [&](std::string_view name) {
            const auto item = profileState.achievementItems.find(
                std::string(name));
            if (item == profileState.achievementItems.end())
                return false;
            const auto state = item->second.values.find("state");
            return state != item->second.values.end() &&
                   state->second == "asOpened";
        };
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
        (sourceNormalInteractiveLaunch &&
         !profileState.preferredCameraSerialized);
    // Metal on Apple Silicon is one capable unified GPU.  Report it through
    // the source's "discrete" compatibility bit so CheckStartupMenu keeps
    // sfrFixed without showing a misleading Windows hybrid-GPU warning.
    constexpr bool sourceCurrentDiscreteVideoCard = true;
    bool sourceDiscreteVideoChanged =
        sourceNormalInteractiveLaunch &&
        (!profileState.discreteVideoCardSerialized ||
         profileState.config.discreteVideoCard !=
             sourceCurrentDiscreteVideoCard);
    if (options->languageSelected)
    {
        profileState.config.language = options->language;
        profileState.languageSerialized = true;
    }
    else if (profileState.languageSerialized &&
             !profileState.config.language.empty())
        activeLanguage = profileState.config.language;
    else
        activeLanguage.clear();
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
        originalGameDataCatalog =
            originalgamedata::loadOriginalGameDataCatalog(*resources);
        if (activeLanguage.empty())
        {
            activeLanguage =
                originalgamedata::autodetectOriginalLanguage(
                    originalGameDataCatalog,
                    rrr3d::macos::preferredGamePrimaryLanguageId());
#ifdef RRR3D_PHYSICS
            profileState.config.language = activeLanguage;
#endif
        }
#ifdef RRR3D_PHYSICS
        if (!profileState.commentatorStyleSerialized ||
            profileState.config.commentatorStyle.empty())
        {
            profileState.config.commentatorStyle =
                originalgamedata::
                    autodetectOriginalCommentatorStyle(
                        originalGameDataCatalog, activeLanguage);
        }
#endif
        if (originalgamedata::findLanguage(
                originalGameDataCatalog, activeLanguage) == nullptr)
        {
            throw r3d::resource::ResourceError(
                "game.xml does not declare requested language: " +
                activeLanguage);
        }
#ifdef RRR3D_PHYSICS
        if (!profileState.configFileSerialized &&
            sourceNormalInteractiveLaunch)
        {
            // GameMode::LoadConfig catches an unavailable user.xml, calls
            // ResetConfig (whose language/commentator autodetection depends
            // on the already-loaded game.xml), then immediately SaveConfig.
            // Its first-launch camera/GPU flags remain active for this run
            // even though all values now exist on disk.
            profileState.config.language = activeLanguage;
            profileState.config.discreteVideoCard =
                sourceCurrentDiscreteVideoCard;
            std::string configError;
            if (!profileStore.saveConfig(profileState, configError))
            {
                throw r3d::resource::ResourceError(
                    "unable to recover original user.xml: " +
                    configError);
            }
            profileState.configFileSerialized = true;
            profileState.preferredCameraSerialized = true;
            profileState.discreteVideoCardSerialized = true;
            profileState.languageSerialized = true;
            profileState.commentatorStyleSerialized = true;
            std::cout
                << "Original GameMode::ResetConfig -> SaveConfig: "
                << profileStore.saveDirectory() / "user.xml" << '\n';
        }
#endif
        model.emplace(
            menu::loadOriginalMainMenu(
                *resources, originalGameDataCatalog, activeLanguage));
#ifdef RRR3D_AUDIO
        originalMusicCatalog = originalGameDataCatalog.music;
#endif
#ifdef RRR3D_PHYSICS
        originalGarage.emplace(
            r3d::game::originalrace::loadOriginalGarage(*resources));
        if (achievementOpened("armor4") &&
            !applyArmor4Presentation(*originalGarage))
        {
            throw r3d::resource::ResourceError(
                "armor4 reward requires the source armor3 workshop item");
        }
        originalRace.emplace(r3d::game::originalrace::loadOriginalRace(
            *resources, selectedTrack, selectedCar,
            options->legacyWindowsDebug));
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
                        *resources, selectedTrack, selectedCar,
                        options->legacyWindowsDebug));
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
        if (options->legacyWindowsDebug)
            applyWeather(originalRace->environment, "cloudy",
                         originalRace->levelPath);
        profileState.player.currentCar = originalRace->vehicle.record;
        if (!originalRace->racers.empty())
            originalRace->racers.front().name =
                profileState.player.name;
        r3d::game::originalrace::applyOriginalPlayerProfile(
            *originalRace, *resources, profileState.player,
            achievementOpened("armor4"));
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
    std::cout << "Serialized GameMode catalog: "
              << originalGameDataCatalog.languages.size()
              << " languages, "
              << originalGameDataCatalog.commentatorStyles.size()
              << " commentator styles, "
              << originalGameDataCatalog.music.menu.size()
              << " menu and "
              << originalGameDataCatalog.music.game.size()
              << " game music tracks\n";
#ifdef RRR3D_AUDIO
    std::cout << "Serialized MusicCat catalog: "
              << originalMusicCatalog.menu.size() << " menu, "
              << originalMusicCatalog.game.size() << " game tracks\n";
#endif

    if (options->verifyResources)
    {
        const auto& sourceStrings = model->localizedStrings;
        const bool sourceStringLibraryValid =
            sourceStrings.has("svPlayer") &&
            sourceStrings.has("svStartMatch") &&
            !sourceStrings.has("svNull") &&
            sourceStrings.get("svNull") == "svNull" &&
            !sourceStrings.has("svHintLeaversWillBeRemoved") &&
            sourceStrings.get("svHintLeaversWillBeRemoved") ==
                "svHintLeaversWillBeRemoved" &&
            sourceStrings.get("rrr3dMissingStringRegression") ==
                "rrr3dMissingStringRegression";
        if (!sourceStringLibraryValid)
        {
            std::cerr << "Source StringLibrary Get/Has semantics differ "
                         "from the shipped Windows data\n";
            return EXIT_FAILURE;
        }
        const auto& languages = originalGameDataCatalog.languages;
        const auto& commentators =
            originalGameDataCatalog.commentatorStyles;
        const auto& comments =
            originalGameDataCatalog.commentator.comments;
        const auto startComment = comments.find("raceStartTime2");
        const auto inverseComment =
            comments.find("playerMoveInverse");
        const auto lastComment = comments.find("playerFinishLast");
        const bool sourceGameDataCatalogValid =
            languages.size() == 6U &&
            languages[0].name == "english" &&
            languages[0].file == "Data\\english.txt" &&
            languages[0].locale == "english" &&
            languages[0].charset ==
                originalgamedata::LanguageCharset::EastEurope &&
            languages[0].primaryId == 9 &&
            languages[1].name == "russian" &&
            languages[1].charset ==
                originalgamedata::LanguageCharset::Russian &&
            languages[1].primaryId == 25 &&
            languages[2].name == "portuguese" &&
            languages[2].primaryId == 22 &&
            languages[3].name == "french" &&
            languages[3].primaryId == 12 &&
            languages[4].name == "spain" &&
            languages[4].locale == "spanish" &&
            languages[4].primaryId == 10 &&
            languages[5].name == "german" &&
            languages[5].primaryId == 7 &&
            originalgamedata::autodetectOriginalLanguage(
                originalGameDataCatalog, 25) == "russian" &&
            originalgamedata::autodetectOriginalLanguage(
                originalGameDataCatalog, 12) == "french" &&
            originalgamedata::autodetectOriginalLanguage(
                originalGameDataCatalog, 999) == "english" &&
            commentators == std::vector<std::string>{
                "russian", "english"} &&
            originalgamedata::autodetectOriginalCommentatorStyle(
                originalGameDataCatalog, "russian") == "russian" &&
            originalgamedata::autodetectOriginalCommentatorStyle(
                originalGameDataCatalog, "french") == "english" &&
            originalGameDataCatalog.commentator.delay == 0.0F &&
            comments.size() == 37U &&
            startComment != comments.end() &&
            startComment->second.voices.size() == 4U &&
            startComment->second.voices.front().sound ==
                "Voice\\start1.ogg" &&
            inverseComment != comments.end() &&
            inverseComment->second.busy ==
                originalgamedata::CommentatorBusyAction::Queue &&
            !inverseComment->second.repeatPlayer &&
            lastComment != comments.end() &&
            lastComment->second.voices.size() == 6U;
        if (!sourceGameDataCatalogValid)
        {
            std::cerr << "Serialized game.xml language/commentator catalog "
                         "does not match the shipped Windows data\n";
            return EXIT_FAILURE;
        }
#ifdef RRR3D_AUDIO
        const bool sourceMusicCatalogValid =
            originalMusicCatalog.menu.size() == 3U &&
            originalMusicCatalog.game.size() == 11U &&
            originalMusicCatalog.menu[0].path ==
                "Music\\Track1.ogg" &&
            originalMusicCatalog.menu[0].name ==
                "Peter Gunn Theme" &&
            originalMusicCatalog.menu[0].band == "Frantick" &&
            originalMusicCatalog.menu[1].path ==
                "Music\\Track14.ogg" &&
            originalMusicCatalog.menu[1].name ==
                "Bad to the Bone" &&
            originalMusicCatalog.menu[2].path ==
                "Music\\Track15.ogg" &&
            originalMusicCatalog.menu[2].band ==
                "The Ventures" &&
            originalMusicCatalog.game.front().group == 1 &&
            originalMusicCatalog.game.back().path ==
                "Music\\Track3.ogg";
        if (!sourceMusicCatalogValid)
        {
            std::cerr << "Serialized game.xml MusicCat catalog does not "
                         "match the shipped Windows data\n";
            return EXIT_FAILURE;
        }
#endif
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
            !rrr3d::debug::runOriginalGameDebugSmokeTest(physicsError) ||
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
    // The Windows renderer changes fullscreen state immediately. Cocoa's
    // default fullscreen Space animates asynchronously and can leave SDL
    // mouse events queued behind several seconds of resize traffic. Use an
    // immediate fullscreen window and keep the focus-acquiring click.
    SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, "0");
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
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
        initialHeight,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY |
            SDL_WINDOW_HIDDEN);
    if (window == nullptr)
    {
        std::cerr << "Unable to create window: " << SDL_GetError() << '\n';
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }

    const auto sourceDisplayModes = originalDisplayModesForWindow(window);
#ifdef RRR3D_PHYSICS
    const std::uint32_t startupWidth =
        profileState.config.resolutionWidth;
    const std::uint32_t startupHeight =
        profileState.config.resolutionHeight;
    // Regression fixtures must not seize the user's desktop. An ordinary
    // launch applies the persisted source View::Desc before the first frame.
    const bool startupFullscreen =
        profileState.config.fullScreen && options->smokeFrames == 0U;
#else
    const std::uint32_t startupWidth = initialWidth;
    const std::uint32_t startupHeight = initialHeight;
    const bool startupFullscreen = false;
#endif
    std::string startupWindowError;
    if (!applyOriginalWindowMode(
            window, sourceDisplayModes, startupWidth, startupHeight,
            startupFullscreen, false, startupWindowError))
    {
        std::cerr << "Unable to apply original startup display mode: "
                  << startupWindowError << '\n';
#ifdef RRR3D_PHYSICS
        profileState.config.fullScreen = false;
#endif
        startupWindowError.clear();
        if (!applyOriginalWindowMode(
                window, sourceDisplayModes, startupWidth, startupHeight,
                false, false, startupWindowError))
        {
            std::cerr << "Unable to create fallback window: "
                      << startupWindowError << '\n';
            SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
            input.shutdown();
#endif
            SDL_Quit();
            return EXIT_FAILURE;
        }
    }
    if (!SDL_ShowWindow(window) || !SDL_SyncWindow(window))
    {
        std::cerr << "Unable to show synchronized game window: "
                  << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
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
    // D3D9's GUI viewport was expressed in backbuffer pixels. SDL mouse
    // coordinates remain logical points on Retina, but the Metal drawable
    // and the original GUI assets use pixels; pointer conversion below
    // deliberately maps between those two coordinate spaces.
    menu::virtualWidth = static_cast<float>(pixelWidth);
    menu::virtualHeight = static_cast<float>(pixelHeight);
    int logicalWindowWidth = 0;
    int logicalWindowHeight = 0;
    SDL_GetWindowSize(window, &logicalWindowWidth, &logicalWindowHeight);
    std::cout << "GUI viewport: " << logicalWindowWidth << 'x'
              << logicalWindowHeight << " points, " << pixelWidth << 'x'
              << pixelHeight << " drawable pixels\n";
    // Release handles WM_SETCURSOR by hiding the system arrow. Windows
    // _DEBUG deliberately omits that call, so the compatibility mode keeps
    // the Cocoa cursor visible as well.
#ifdef RRR3D_PHYSICS
    if (options->legacyWindowsDebug)
        SDL_ShowCursor();
    else
#endif
        SDL_HideCursor();
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
#ifdef RRR3D_PHYSICS
    device->configureQuality(
        profileState.config.quality.filtering,
        profileState.config.quality.msaa);
#endif

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
        return model->localizedStrings.get(key);
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
    originalDisplayModes.reserve(sourceDisplayModes.size());
    for (const auto& mode : sourceDisplayModes)
        originalDisplayModes.emplace_back(mode.width, mode.height);
    if (originalDisplayModes.empty())
        originalDisplayModes.push_back(
            {originalMinimumWidth, originalMinimumHeight});
    std::cout << "Original D3D9 display modes (mapped to nearest macOS "
                 "60 Hz mode):";
    for (const auto& mode : originalDisplayModes)
        std::cout << ' ' << mode.first << 'x' << mode.second;
    std::cout << '\n';
    // GameMode::LoadGameData order, consumed directly by the source
    // OptionsMenu and StartOptionsMenu steppers.
    std::vector<std::string> sourceLanguages;
    sourceLanguages.reserve(originalGameDataCatalog.languages.size());
    for (const auto& language : originalGameDataCatalog.languages)
        sourceLanguages.push_back(language.name);
    const auto& sourceCommentators =
        originalGameDataCatalog.commentatorStyles;
    r3d::game::originaloptions::OptionsMenuState sourceOptionsMenu({
        originalDisplayModes, sourceLanguages,
        std::vector<std::string>(sourceCommentators.begin(),
                                 sourceCommentators.end())});
    r3d::game::originaloptions::StartOptionsMenuState
        sourceStartOptionsMenu({
            originalDisplayModes, sourceLanguages,
            std::vector<std::string>(sourceCommentators.begin(),
                                     sourceCommentators.end())});
    sourceOptionsMenu.begin(
        profileState.config, profileState.player.difficulty);
    sourceStartOptionsMenu.begin(profileState.config);
    auto& optionsDraftConfig = sourceOptionsMenu.draft();
    auto& optionsDraftDifficulty = sourceOptionsMenu.difficulty();
    auto startOptionsValues = [&]() {
        const auto& resolution = sourceStartOptionsMenu.resolution();
        return std::vector<std::string>{
            sourceStartOptionsMenu.cameraIndex() == 0U
                ? localized("svCameraSecView")
                : sourceStartOptionsMenu.cameraIndex() == 1U
                      ? localized("svCameraOrtho")
                      : localized("svSelectItem"),
            std::to_string(resolution.first) + " x " +
                std::to_string(resolution.second),
            std::string(sourceStartOptionsMenu.language()),
            std::string(sourceStartOptionsMenu.commentator())};
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
                optionsDraftConfig.maxPlayers, 2U,
                r3d::game::originalrace::originalMaximumPlayers)),
            std::to_string(std::min(
                optionsDraftConfig.maxComputers,
                r3d::game::originalrace::originalMaximumComputers)),
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
            optionsDraftConfig.language,
            optionsDraftConfig.commentatorStyle,
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
        const auto required = std::max(
            r3d::game::originalrace::originalTournamentRequestPoints(
                *originalRace, pass),
            0);
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
                      *originalGarage, *car, profileState.player,
                      achievementOpened("armor4"))
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
            {localized("svStartMatch"), localized("svWorkshop"),
             localized("svGarage"), localized("svCasePlanet"),
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
            {localized("svCasePlanet"), localized("svBack")});
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
        createMusicDialogVisuals(originalMusicCatalog.menu);
#ifdef RRR3D_PHYSICS
    const auto gameMusicDialogVisuals =
        createMusicDialogVisuals(originalMusicCatalog.game);
#endif
#endif
    const TextVisual finalBackText = createText(
        *device, localized("svBack"), menu::headerFontHeight, false,
        menu::Rgba8{214U, 214U, 214U, 255U}, resolvedFont);
    r3d::game::mainmenu2::FinalMenuFrameState sourceFinalFrame;
    sourceFinalFrame.invalidate(localized("svCredits"));
    struct FinalCreditSection
    {
        TextVisual caption;
        std::vector<TextVisual> lines;
        float height = 0.0F;
    };
    std::vector<FinalCreditSection> finalCredits;
    float finalCreditsHeight = 0.0F;
    finalCredits.reserve(sourceFinalFrame.credits().size());
    for (const auto& sourceSection : sourceFinalFrame.credits())
    {
        FinalCreditSection section;
        section.caption = createText(
            *device, sourceSection.caption,
            menu::smallFontHeight, false,
            menu::Rgba8{220U, 0U, 0U, 255U}, resolvedFont);
        if (!sourceSection.text.empty())
        {
            std::size_t lineBegin = 0U;
            while (lineBegin <= sourceSection.text.size())
            {
                const auto lineEnd =
                    sourceSection.text.find('\n', lineBegin);
                std::string line = sourceSection.text.substr(
                    lineBegin,
                    (lineEnd == std::string::npos
                         ? sourceSection.text.size()
                         : lineEnd) -
                        lineBegin);
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (!line.empty())
                {
                    section.lines.push_back(createText(
                        *device, line, menu::smallFontHeight, false,
                        menu::Rgba8{255U, 214U, 205U, 255U},
                        resolvedFont));
                }
                if (lineEnd == std::string::npos)
                    break;
                lineBegin = lineEnd + 1U;
            }
        }
        section.height = section.caption.height + 50.0F;
        for (const auto& line : section.lines)
            section.height += line.height;
        finalCreditsHeight += section.height;
        finalCredits.push_back(std::move(section));
    }
#ifdef RRR3D_PHYSICS
    const TextVisual finishRewardTitle = createText(
        *device, localized("svPrice"), menu::headerFontHeight, false,
        menu::Rgba8{233U, 167U, 63U, 255U}, resolvedFont);
    const TextVisual finishPriceInfo = createText(
        *device, localized("svMoney") + "\n" + localized("svPoints"),
        menu::headerFontHeight, false,
        menu::Rgba8{225U, 225U, 225U, 255U}, resolvedFont);
    struct FinishRowVisual
    {
        std::size_t racer = 0U;
        Texture photo;
        float photoWidth = 0.0F;
        float photoHeight = 0.0F;
        TextVisual name;
        TextVisual rewardValue;
    };
    std::vector<FinishRowVisual> finishRows;
    auto clearFinishRows = [&]() {
        for (const auto& row : finishRows)
        {
            device->destroy(row.rewardValue.texture);
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
        valid(finishPriceInfo.texture) && valid(acceptFrame) &&
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
        device->destroy(finishPriceInfo.texture);
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
    if (options->legacyWindowsDebug)
    {
        std::cout
            << "Legacy Windows _DEBUG compatibility: enabled; "
               "startup/campaign intros skipped, cloudy weather, "
               "immediate cGoRace, AIDebug/F1-F7, five debug cameras, "
               "90-track catalog with two 99-lap fixtures, debug LAN "
               "timing, skid/contact effects disabled\n";
    }
#endif

#ifdef RRR3D_PHYSICS
    std::string physicsError;
    auto physicsWorld = r3d::physics::createOriginalVehicleWorld(
        *physicsDescription, physicsError);
    r3d::game::originalrace::OriginalRaceSession raceSession(
        *originalRace, options->legacyWindowsDebug);
    r3d::game::originalrace::source::TraceGfx sourceTraceGfx(
        &raceSession.sourceMap().GetTrace());
    raceSession.setCampaign(true);
    raceSession.applyPlayerProfile(profileState.player);
    raceSession.applyAchievementProfile(profileState);
    raceSession.setEnableMineBug(profileState.config.enableMineBug);
    raceSession.setSpringBorders(profileState.config.springBorders);
    auto bindSourceVehicleFixedStep = [&]() {
        if (!physicsWorld)
            return;
        raceSession.setExternalVehicleFixedStep(true);
        physicsWorld->setVehicleFixedStepController(
            [&raceSession](
                std::size_t racer, float deltaTime,
                const r3d::physics::VehicleInput& input,
                const r3d::physics::VehicleFixedStepState& state) {
                return raceSession.racerFixedStepDrive(
                    racer, deltaTime, input, state);
            });
    };
    bindSourceVehicleFixedStep();
    rrr3d::debug::OriginalGameDebug gameDebug(
        options->gameDebug || options->legacyWindowsDebug,
        profileState.config.quality.postEffect);
    auto raceCameraStyle =
        profileState.config.preferredCamera ==
                r3d::game::originalrace::PreferredCamera::ThirdPerson
            ? rrr3d::race::RaceCameraStyle::ThirdPerson
            : rrr3d::race::RaceCameraStyle::Isometric;
    std::vector<TextVisual> gameDebugVisual;
    rrr3d::race::OriginalResourceManager originalResourceManager(
        *device, *resources);
    rrr3d::race::OriginalRaceRenderer raceRenderer;
    rrr3d::race::OriginalRaceRenderer garageRenderer;
    rrr3d::race::OriginalRaceRenderer angarRenderer;
    rrr3d::race::OriginalWorkshopRenderer workshopRenderer;
    rrr3d::race::OriginalRaceHud raceHud;
    r3d::game::originalui::OriginalUserChat userChat;
    UserChatVisual userChatVisual;
    if (!physicsWorld ||
        !raceRenderer.initialize(*device, originalResourceManager,
                                 *originalRace,
                                 static_cast<std::uint32_t>(pixelWidth),
                                 static_cast<std::uint32_t>(pixelHeight),
                                 physicsError) ||
        !garageRenderer.initialize(
            *device, originalResourceManager, *originalGarageScene,
            static_cast<std::uint32_t>(pixelWidth),
            static_cast<std::uint32_t>(pixelHeight), physicsError) ||
        !angarRenderer.initialize(
            *device, originalResourceManager, *originalAngarScene,
            static_cast<std::uint32_t>(pixelWidth),
            static_cast<std::uint32_t>(pixelHeight), physicsError) ||
        !workshopRenderer.initialize(
            *device, originalResourceManager, *originalGarage,
            *originalRace,
            physicsError) ||
        !raceHud.initialize(*device, originalResourceManager,
                            originalGameDataCatalog, *originalRace,
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
        originalResourceManager.Shutdown();
        releaseResources();
        device.reset();
        SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
        input.shutdown();
#endif
        SDL_Quit();
        return EXIT_FAILURE;
    }
    std::cout << "Original ResourceManager: "
              << originalResourceManager.GetMeshCount() << " meshes, "
              << originalResourceManager.GetTextureCount()
              << " textures, "
              << originalResourceManager.GetCacheHitCount() << '/'
              << originalResourceManager.GetRequestCount()
              << " shared requests reused\n";
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
            originalResourceManager.Shutdown();
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
    const std::vector<float> garageBonusScales;
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
    SourceAutoObserverState garageObserver;
    SourceAutoObserverState angarObserver;
    std::cout << "Milestone 9 race: " << originalRace->levelPath << ", "
              << originalRace->lapCount << " laps, "
              << originalRace->trackInstances.size()
              << " original track placements, car "
              << originalRace->vehicle.record
              << ", Jolt backend with original db.xml parameters\n"
              << "Original HUD localization (" << activeLanguage
              << "): lap='" << raceHud.localizedLapName()
              << "', reward='" << raceHud.localizedPriceName()
              << "'\n";
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

    originalResourceManager.AttachAudio(audio);
    rrr3d::audio::OriginalMenuSounds menuSounds(
        audio, originalResourceManager);
    if (!menuSounds.initialize(audioError))
    {
        std::cerr << "Original Menu SoundSheme loading failed: "
                  << audioError << '\n';
        menuSounds.shutdown();
        originalResourceManager.ShutdownSounds();
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
        originalMusicCatalog.menu,
        options->audioSmokeTest
            ? std::vector<std::size_t>{}
            : musicPlaylist(profileState.config.menuMusicPlaylist));
    if (!music.initialize(audioError))
    {
        std::cerr << "Original MusicCat initialization failed: "
                  << audioError << '\n';
        music.shutdown();
        menuSounds.shutdown();
        originalResourceManager.ShutdownSounds();
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
        originalResourceManager.ShutdownSounds();
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
        false, originalMusicCatalog.game,
        musicPlaylist(profileState.config.gameMusicPlaylist));
    // Windows GameMode only loads the game playlist here.  Its first entry is
    // consumed later by DoStartRace::_gameMusic->Play(), not while the menu is
    // starting.
    if (!gameMusic.initialize(audioError, false))
    {
        std::cerr << "Original game MusicCat initialization failed: "
                  << audioError << '\n';
        gameMusic.shutdown();
        finalMusic.shutdown();
        music.shutdown();
        menuSounds.shutdown();
        originalResourceManager.ShutdownSounds();
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
    const bool gameMusicDeferredSelectionObserved =
        !gameMusic.currentTrack().has_value();
    bool gameMusicZeroStartObserved =
        !options->raceRenderSmokeTest;
    // GameMode::_fadeMusic is the gain of its one shared music source.  Keep
    // an equivalent factor over the SDL Music bus for the only active source
    // fade used by OnFinishFrameClose.
    float sourceMenuMusicGain = 1.0F;
    bool finishMenuAudioHeldObserved =
        !options->finishMenuSmokeTest;
    bool finishMenuAudioCloseObserved =
        !options->finishMenuSmokeTest;
    bool finishMenuMusicFadeObserved =
        !options->finishMenuSmokeTest;
#endif

    originalmenu::MenuSystem sourceMenuSystem;
    sourceMenuSystem.AdjustLayout(
        {menu::virtualWidth, menu::virtualHeight});
    originalmenu::DialogSystem sourceDialogs(sourceMenuSystem);

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
            const auto& tracks =
                source == OriginalMusicDialogSource::Menu
                    ? originalMusicCatalog.menu
#ifdef RRR3D_PHYSICS
                    : originalMusicCatalog.game
#else
                    : originalMusicCatalog.menu
#endif
                    ;
            sourceDialogs.ShowMusicInfo(
                tracks[*track].band, tracks[*track].name,
                {static_cast<float>(musicDialogFrameImage.width),
                 static_cast<float>(musicDialogFrameImage.height)});
        };
    // GameMode::StartGame shows the current track only after FreeIntro.
    if (!sourceStartupRequested)
    {
        showOriginalMusicInfo(
            OriginalMusicDialogSource::Menu, lastMenuMusicTrack);
    }

    std::cout << "Original MusicCat: background decode, source playlist, "
                 "auto Next and in-process pause/resume";
    if (options->audioSmokeTest)
        std::cout << ", isolated test state " << musicStatePath;
    std::cout << "\nOriginal menu tracks:";
    for (const auto& track : originalMusicCatalog.menu)
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
        bool spatialProxyPlaying = false;
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
        r3d::physics::Vec3 followOffset;
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
    std::map<r3d::audio::SoundHandle, float> engineSoundVolumes;
    std::vector<EngineAudio> engineAudio(originalRace->racers.size());
    std::vector<std::vector<WheelSlipAudio>>
        wheelSlipVoices(originalRace->racers.size());
    std::unordered_set<r3d::audio::VoiceHandle> raceLoopVoices;
    bool raceLoopTeardownObserved = !options->raceRenderSmokeTest;
    std::vector<ShotEffectAudio> shotEffectAudio;
    std::vector<ContactEffectAudio> contactEffectAudio;
    std::vector<TimedEffectAudio> timedEffectAudio;
    auto loadEngineSound = [&](const std::string& path) {
        try
        {
            const auto& resource = originalResourceManager.GetSound(
                path, rrr3d::audio::originalSoundVolume(path));
            engineSoundVolumes.try_emplace(
                resource.sound, resource.volume);
            return resource.sound;
        }
        catch (const std::exception& exception)
        {
            audioError = exception.what();
            return r3d::audio::invalidSound;
        }
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
        audio, originalResourceManager, originalGameDataCatalog);
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
        commentatorValid &&
        originalResourceManager.GetSoundCount() >=
            menuSounds.loadedSoundCount() +
                commentator.loadedVoiceCount() &&
        originalResourceManager.GetSoundCacheHitCount() != 0U;
    std::cout << "Original ResourceManager SoundLib: "
              << originalResourceManager.GetSoundCount() << " sounds, "
              << originalResourceManager.GetSoundCacheHitCount() << '/'
              << originalResourceManager.GetSoundRequestCount()
              << " shared requests reused\n";
    if (!engineAudioValid)
    {
        std::cerr << "Original race engine audio loading failed: "
                  << audioError << '\n';
        commentator.shutdown();
        gameMusic.shutdown();
        finalMusic.shutdown();
        music.shutdown();
        menuSounds.shutdown();
        raceHud.shutdown(*device);
        workshopRenderer.shutdown(*device);
        angarRenderer.shutdown(*device);
        garageRenderer.shutdown(*device);
        raceRenderer.shutdown(*device);
        originalResourceManager.Shutdown();
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
    auto stopRaceLoopVoice = [&](r3d::audio::VoiceHandle& voice) {
        if (voice == r3d::audio::invalidVoice)
            return;
        audio.stop(voice);
        raceLoopVoices.erase(voice);
        voice = r3d::audio::invalidVoice;
    };
    auto playRaceLoop = [&](r3d::audio::SoundHandle sound,
                            const r3d::audio::PlayOptions& options) {
        const auto voice = audio.play(sound, options, audioError);
        if (voice != r3d::audio::invalidVoice)
            raceLoopVoices.insert(voice);
        return voice;
    };
    auto stopAllRaceLoops = [&]() {
        // This ownership set is deliberately authoritative.  A respawn or a
        // repeated race initialization may replace a per-wheel handle; the
        // source SoundMotor destructor still stops every loop it created.
        // Keeping all loop handles here gives the portable transition the
        // same guarantee when Race -> Menu tears the world down.
        const std::vector<r3d::audio::VoiceHandle> ownedVoices(
            raceLoopVoices.begin(), raceLoopVoices.end());
        for (const auto voice : ownedVoices)
            audio.stop(voice);
        raceLoopVoices.clear();
        for (auto& engine : engineAudio)
        {
            engine.idleVoice = r3d::audio::invalidVoice;
            engine.rpmVoice = r3d::audio::invalidVoice;
        }
        for (auto& wheels : wheelSlipVoices)
            for (auto& voice : wheels)
                voice = {};
        if (!ownedVoices.empty() &&
            std::none_of(
                ownedVoices.begin(), ownedVoices.end(),
                [&](const auto voice) {
                    return audio.isVoiceActive(voice);
                }))
        {
            raceLoopTeardownObserved = true;
        }
    };
    auto startRacerMotorAudio = [&](std::size_t racer) {
        if (racer >= engineAudio.size() ||
            racer >= originalRace->racers.size())
            return;
        auto& engine = engineAudio[racer];
        stopRaceLoopVoice(engine.idleVoice);
        stopRaceLoopVoice(engine.rpmVoice);
        if (racer < wheelSlipVoices.size())
        {
            for (auto& voice : wheelSlipVoices[racer])
                stopRaceLoopVoice(voice.voice);
        }
        // Source3d::Play allocates its Proxy, but ApplyX3dEffect starts that
        // Proxy only while the emitter is strictly inside distScaler.  Keep
        // the backend voice allocated and initially paused so emitters born
        // in the 30..45 m stop-lag band do not start prematurely.
        engine.spatialProxyPlaying = false;
        r3d::audio::PlayOptions options;
        options.bus = r3d::audio::Bus::Effects;
        options.loop = true;
        const bool localHuman =
            racer < raceSession.racers().size() &&
            raceSession.racers()[racer].IsHuman();
        options.volume = localHuman ? 0.5F : 0.0F;
        engine.idleVoice = playRaceLoop(engine.idle, options);
        options.volume = localHuman ? 0.2F : 0.0F;
        engine.rpmVoice = playRaceLoop(engine.rpm, options);
        const auto& sourceRacer = originalRace->racers[racer];
        const auto& vehicle =
            sourceRacer.hasConfiguredVehicle
                ? sourceRacer.configuredVehicle
                : originalRace->vehicles.at(sourceRacer.vehicle);
        wheelSlipVoices[racer].assign(
            vehicle.physics.wheels.size(), WheelSlipAudio{});
    };
    auto stopRacerMotorAudio = [&](std::size_t racer) {
        if (racer >= engineAudio.size())
            return;
        auto& engine = engineAudio[racer];
        stopRaceLoopVoice(engine.idleVoice);
        stopRaceLoopVoice(engine.rpmVoice);
        if (racer < wheelSlipVoices.size())
        {
            for (auto& voice : wheelSlipVoices[racer])
                stopRaceLoopVoice(voice.voice);
            wheelSlipVoices[racer].clear();
        }
    };
    auto startRaceAudio = [&]() {
        stopAllRaceLoops();
        sourceMenuMusicGain = 1.0F;
        audio.setBusVolume(r3d::audio::Bus::Music,
                           profileState.config.musicVolume);
        audio.setBusVolume(r3d::audio::Bus::Effects,
                           profileState.config.effectsVolume);
        if (!music.pause(true, audioError) ||
            !gameMusic.play(audioError))
        {
            std::cerr << "Original DoStartRace music transition failed: "
                      << audioError << '\n';
        }
        gameMusicZeroStartObserved =
            gameMusicZeroStartObserved ||
            (gameMusic.currentTrack().has_value() &&
             gameMusic.currentPositionFrames() == 0U);
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
            startRacerMotorAudio(racer);
    };
    auto stopRaceAudio = [&](bool resumeMenuMusic = true) {
        stopAllRaceLoops();
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
        // GameMode::ExitRace calls Stop rather than Pause+Next.  The next
        // playlist entry remains untouched until the following DoStartRace.
        gameMusic.stop();
        if (!music.pause(!resumeMenuMusic, audioError))
        {
            std::cerr
                << "Original ExitRace menu MusicCat transition failed: "
                << audioError << '\n';
        }
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
    Camera camera = makeCamera(*device);

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
    bool optionsReloadDialogPending = false;
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
    std::optional<r3d::game::originalrace::PlayerProfile>
        networkHostOfflineProfile;
    std::optional<r3d::game::originalrace::PlayerProfile>
        networkClientOfflineProfile;
    std::optional<r3d::game::originalrace::UserConfig>
        networkClientLocalConfig;
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
    using MenuScreen = originalmenu::MenuScreen;
    auto& menuStack = sourceMenuSystem.Screens();
    auto& menuSelection = sourceMenuSystem.Selection();
    r3d::game::mainmenu2::FrameController sourceMainMenuFrame;
    bool championshipMode = true;
    bool newTournamentProfile = false;
    std::uint64_t previousFrameTicks = SDL_GetTicksNS();
    std::array<float, 15> sourceFrameDeltas{};
    std::size_t sourceFrameDeltaCount = 0U;
    std::size_t sourceFrameDeltaCursor = 0U;
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
    r3d::game::originalracemenu::RaceMenuState sourceRaceMenu;
    r3d::game::originalracemenu::RaceMainFrameState
        sourceRaceMainFrame;
    r3d::game::originalracemenu::GamersFrameState
        sourceGamersFrame;
    r3d::game::originalracemenu::GarageFrameState
        sourceGarageFrame;
    r3d::game::originalracemenu::WorkshopFrameState
        sourceWorkshopFrame;
    r3d::game::originalracemenu::SpaceshipFrameState
        sourceSpaceshipFrame;
    r3d::game::originalracemenu::AngarFrameState
        sourceAngarFrame;
    r3d::game::originalracemenu::AchievementFrameState
        sourceAchievementFrame;
    r3d::game::originalracemenu::FinishMenuFrameState
        sourceFinishFrame;
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
    r3d::game::mainmenu2::ProfileFrameState sourceProfileFrame;
    bool profileDeleteDialogVisible = false;
    bool profileDeleteYesFocused = true;
    std::size_t profileDeleteIndex =
        std::numeric_limits<std::size_t>::max();
    bool garagePurchaseDialogVisible = false;
    bool garagePurchaseYesFocused = true;
    using WorkshopConfirmation =
        r3d::game::originalracemenu::WorkshopConfirmationType;
    float workshopDragX = menu::virtualWidth * 0.5F;
    float workshopDragY = menu::virtualHeight * 0.5F;
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
    auto activeProfileNames = [&]() -> std::vector<std::string>& {
#ifdef RRR3D_NETWORK
        if (networkHostRequested)
            return profileState.networkProfiles;
#endif
        return profileState.profiles;
    };
    auto refreshProfilePage = [&]() {
#ifdef RRR3D_PHYSICS
        auto profileLabels = activeProfileNames();
        sourceProfileFrame.setProfileCount(profileLabels.size());
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
        r3d::game::mainmenu2::FrameContext context;
#ifdef RRR3D_PHYSICS
        const auto& profileNames = activeProfileNames();
        context.tutorialFirstStageComplete =
            profileState.tutorialStage >= 1U;
        context.hasProfiles = !profileNames.empty();
        const std::string& lastProfile =
#ifdef RRR3D_NETWORK
            networkHostRequested
                ? profileState.lastNetworkProfile
                :
#endif
                  profileState.lastProfile;
        context.hasLastProfile =
            std::find(profileNames.begin(), profileNames.end(),
                      lastProfile) != profileNames.end();
#endif
        auto& page = activeMenuPage();
        sourceMainMenuFrame.show(screen, page.enabled.size(), context);
        page.enabled = sourceMainMenuFrame.enabledItems();
    };
    auto firstEnabledMenuItem = [&]() {
        return sourceMainMenuFrame.firstEnabled();
    };
    auto sharedMenuItemY =
        [&](MenuScreen screen, std::size_t index,
            std::size_t count) {
            if (sourceMainMenuFrame.screen() == screen &&
                sourceMainMenuFrame.enabledItems().size() == count)
            {
                return sourceMainMenuFrame.itemY(
                    menu::virtualHeight, index);
            }
            return menu::virtualHeight * 0.5F +
                   menu::firstItemOffsetY +
                   static_cast<float>(index) *
                       menu::itemSpacing;
        };
    auto pushMenu = [&](MenuScreen screen) {
#ifdef RRR3D_PHYSICS
        using RaceMenuState = r3d::game::originalracemenu::State;
        switch (screen)
        {
        case MenuScreen::RaceMenu:
            sourceRaceMenu.setState(RaceMenuState::Main);
            break;
        case MenuScreen::Garage:
            sourceRaceMenu.setState(RaceMenuState::Garage);
            break;
        case MenuScreen::Workshop:
            sourceRaceMenu.setState(RaceMenuState::Workshop);
            break;
        case MenuScreen::Gamers:
            sourceRaceMenu.setState(RaceMenuState::Gamers);
            break;
        case MenuScreen::Planets:
            sourceRaceMenu.setState(RaceMenuState::Angar);
            break;
        case MenuScreen::Achievements:
            sourceRaceMenu.setState(RaceMenuState::Achievements);
            break;
        default:
            break;
        }
#endif
        if (screen == MenuScreen::Profiles)
        {
#ifdef RRR3D_PHYSICS
            sourceProfileFrame.show(activeProfileNames().size());
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
        const auto leavingScreen = menuStack.back();
#ifdef RRR3D_NETWORK
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
            if (networkHostOfflineProfile)
            {
                profileState.player = *networkHostOfflineProfile;
                networkHostOfflineProfile.reset();
                championshipPlayerBeforeSkirmish.reset();
                championshipMode = true;
                refreshProfilePage();
            }
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
        if (leavingScreen == MenuScreen::Garage)
            sourceGarageFrame.hide();
        if (leavingScreen == MenuScreen::Workshop)
            sourceWorkshopFrame.hide();
        if (leavingScreen == MenuScreen::Planets)
            sourceAngarFrame.hide();
        if (leavingScreen == MenuScreen::Achievements)
            sourceAchievementFrame.hide();
        if (menuStack.back() == MenuScreen::RaceMenu)
        {
            sourceRaceMenu.setState(
                r3d::game::originalracemenu::State::Main);
            refreshRaceMainPages();
        }
#endif
        refreshSharedMenuAvailability(menuStack.back());
        menuSelection = firstEnabledMenuItem();
    };
    auto showOriginalFinalMenu = [&]() {
        sourceFinalFrame.show();
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
        sourceFinalFrame.hide();
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
    std::uint32_t originalMovieInputSuppressionFrames = 0U;
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
            // Video::Play calls World::ResetInput; the Windows main loop
            // drops ordinary input until the reset survives three frames.
            originalMovieInputSuppressionFrames = 3U;
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
#endif
#ifdef RRR3D_PHYSICS
    bool inRace = false;
    r3d::game::originalrace::source::GameModeState gameModeState;
    r3d::game::originalrace::source::WorldEventPump worldEventPump;
    worldEventPump.SetGameMode(&gameModeState);
    bool exitRaceDialogVisible = false;
    bool exitRaceYesFocused = true;
    r3d::physics::VehicleInput raceInput;
    bool raceUseWeaponRequested = false;
    bool raceUseAllWeaponsRequested = false;
    bool raceUseMineRequested = false;
    bool raceMineAnalogBinding = false;
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
    std::vector<r3d::physics::VehicleState> raceRenderVehicles =
        raceVehicles;
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
    bool raceEffectsMuteObserved = !options->raceRenderSmokeTest;
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
    std::array<bool, 5> legacyDebugCameraStylesObserved{};
    bool raceProgressSaved = false;
    bool raceExitLifecycleApplied = false;
    bool finishMenuFrameObserved =
        !options->finishMenuSmokeTest;
    bool finishLastEventObserved =
        !options->finishMenuSmokeTest;
    std::uint32_t raceSmokeMenuStep = 0;
    std::uint32_t raceSmokeNextMenuFrame = 0;
    bool raceSmokeAccelerateQueued = false;
    std::uint32_t raceChatSmokeStep = 0U;
    std::uint32_t raceChatSmokeNextFrame = 0U;
    r3d::game::originalrace::TournamentAdvance
        raceTournamentAdvance;
    bool racePlanetChampion = false;
    auto acceptDialogVisible = [&]() {
        return exitRaceDialogVisible ||
#ifdef RRR3D_NETWORK
               networkLeaverStartDialogVisible ||
#endif
               profileDeleteDialogVisible ||
               garagePurchaseDialogVisible ||
               sourceWorkshopFrame.confirmation().type !=
                   WorkshopConfirmation::None ||
               sourceAngarFrame.travelDialog().visible ||
               sourceAchievementFrame.confirmation().visible ||
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
        if (sourceWorkshopFrame.confirmation().type !=
            WorkshopConfirmation::None)
            return sourceWorkshopFrame.confirmation().yesFocused;
        if (sourceAngarFrame.travelDialog().visible)
            return sourceAngarFrame.travelDialog().yesFocused;
        if (sourceAchievementFrame.confirmation().visible)
            return sourceAchievementFrame.confirmation().yesFocused;
        return false;
    };
    auto setAcceptDialogFocus = [&](bool yes) {
        sourceDialogs.SetAcceptHover(yes);
        sourceDialogs.SetAcceptFocus(yes);
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
        else if (sourceWorkshopFrame.confirmation().type !=
                 WorkshopConfirmation::None)
            sourceWorkshopFrame.setConfirmationYesFocused(yes);
        else if (sourceAngarFrame.travelDialog().visible)
            sourceAngarFrame.setTravelYesFocused(yes);
        else if (sourceAchievementFrame.confirmation().visible)
            sourceAchievementFrame.setPurchaseYesFocused(yes);
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
        sourceRaceMainFrame.invalidate(clientReady);
        raceMenuPage.enabled.assign(
            sourceRaceMainFrame.enabledItems().begin(),
            sourceRaceMainFrame.enabledItems().end());
        if (clientReady)
        {
            if (!menuStack.empty() &&
                menuStack.back() == MenuScreen::RaceMenu)
            {
                menuSelection =
                    sourceRaceMainFrame.firstEnabled();
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
                           player.color == visual.player.color &&
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
            const auto& sourceDialog = sourceDialogs.ShowAccept(
                std::string(message), std::string(yesText),
                std::string(noText), {centerX, centerY},
                originalmenu::Anchor::Center,
                {static_cast<float>(acceptFrameImage.width),
                 static_cast<float>(acceptFrameImage.height)},
                {static_cast<float>(acceptButtonImage.width),
                 static_cast<float>(acceptButtonImage.height)},
                maxButtonsSize, maxMode, disableFocus);
            destroyAcceptDialog(*device, acceptDialog);
            const float fontHeight =
                sourceDialog.maxMode ? 24.0F : 32.0F;
            const std::size_t maximumLines =
                sourceDialog.maxMode ? 3U : 2U;
            for (const auto& line : wrapAcceptDialogMessage(
                     sourceDialog.message,
                     sourceDialog.layout.infoSize.x,
                     fontHeight, maximumLines))
            {
                acceptDialog.info.push_back(createText(
                    *device, line, fontHeight, false,
                    menu::Rgba8{175, 175, 175, 255},
                    resolvedFont));
            }
            acceptDialog.yes = createText(
                *device, sourceDialog.yesText, 32.0F, false,
                menu::Rgba8{175, 175, 175, 255}, resolvedFont);
            acceptDialog.no = createText(
                *device, sourceDialog.noText, 32.0F, false,
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
        sourceDialogs.SetAcceptVisible(acceptDialogVisible());
        sourceDialogs.SetAcceptFocus(acceptDialogYesFocused());
        const auto& sourceDialog = sourceDialogs.Accept();
        const auto& layout = sourceDialog.layout;
        drawQuad(
            *device, quad, shader, acceptFrame,
            layout.frameSize.x, layout.frameSize.y,
            sourceDialog.center.x, sourceDialog.center.y, 8.0F,
            transparent);
        const float fontHeight =
            sourceDialog.maxMode ? 24.0F : 32.0F;
        const float lineStep = fontHeight * 1.15F;
        const float firstLineY =
            sourceDialog.center.y + layout.infoOffset.y -
            static_cast<float>(acceptDialog.info.size() - 1U) *
                lineStep * 0.5F;
        for (std::size_t line = 0U;
             line < acceptDialog.info.size(); ++line)
        {
            const auto& text = acceptDialog.info[line];
            drawQuad(
                *device, quad, shader, text.texture,
                text.width, text.height, sourceDialog.center.x,
                firstLineY +
                    static_cast<float>(line) * lineStep,
                6.0F, transparent);
        }
        auto drawChoice = [&](bool yes) {
            const bool selected =
                sourceDialog.disableFocus
                    ? sourceDialog.hoveredChoice &&
                          *sourceDialog.hoveredChoice == yes
                    : sourceDialog.yesFocused == yes;
            const float x =
                sourceDialog.center.x +
                (yes ? layout.yesOffset.x : layout.noOffset.x);
            const float y =
                sourceDialog.center.y +
                (yes ? layout.yesOffset.y : layout.noOffset.y);
            drawQuad(
                *device, quad, shader,
                selected ? acceptButtonSelected : acceptButton,
                layout.buttonSize.x,
                layout.buttonSize.y, x, y, 5.0F,
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
                layout.frameSize.x ==
                    static_cast<float>(
                        acceptFrameImage.width) &&
                layout.frameSize.y ==
                    static_cast<float>(
                        acceptFrameImage.height) &&
                layout.infoSize.x == 325.0F &&
                layout.infoSize.y == 65.0F &&
                layout.buttonSize.x ==
                    static_cast<float>(
                        acceptButtonImage.width) &&
                layout.buttonSize.y ==
                    static_cast<float>(
                        acceptButtonImage.height) &&
                layout.yesOffset.x == -70.0F &&
                layout.noOffset.x == 70.0F &&
                layout.yesOffset.y == 32.0F &&
                !sourceDialog.maxMode &&
                !sourceDialog.disableFocus &&
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
    auto makePersistedProfileState = [&]() {
#ifdef RRR3D_AUDIO
        // GameMode::SaveConfig serializes the remaining MusicCat queues.  Do
        // this before every atomic user.xml write so a clean launch continues
        // the source shuffle order, but never resumes a PCM cursor mid-track.
        profileState.config.menuMusicPlaylist =
            musicPlaylistString(music.playlist());
        profileState.config.gameMusicPlaylist =
            musicPlaylistString(gameMusic.playlist());
#endif
        // GameMode::SaveConfig always writes both values, even when they
        // originated in first-launch autodetection.
        profileState.configFileSerialized = true;
        profileState.preferredCameraSerialized = true;
        profileState.discreteVideoCardSerialized = true;
        profileState.languageSerialized = true;
        profileState.commentatorStyleSerialized = true;
        auto persistedState = profileState;
#ifdef RRR3D_NETWORK
        // Race::_snClientProfile is transient.  Network host rules and the
        // received championship profile must never replace the client's
        // offline PlayerProfile or locally configured match defaults.
        if (networkClientOfflineProfile)
            persistedState.player = *networkClientOfflineProfile;
        if (networkClientLocalConfig)
            persistedState.config = *networkClientLocalConfig;
#endif
        if (championshipPlayerBeforeSkirmish)
        {
            persistedState =
                r3d::game::originalrace::
                    makeOriginalSkirmishPersistenceState(
                        profileState,
                        *championshipPlayerBeforeSkirmish);
        }
        return persistedState;
    };
    auto saveGameConfig = [&]() {
        const auto persistedState = makePersistedProfileState();
        std::string configError;
        if (!profileStore.saveConfig(persistedState, configError))
            std::cerr << "Unable to save original GameMode config: "
                      << configError << '\n';
    };
    auto applyRaceProfileState = [&]() {
        if (!raceSession.racers().empty())
        {
            raceSession.writePlayerProfile(profileState.player);
            raceSession.writeAchievementProfile(profileState);
            const auto humanRacer = raceSession.humanRacer();
            if (championshipMode &&
                humanRacer < raceSession.racers().size() &&
                raceSession.racers()[humanRacer].GetFinished() &&
                !raceProgressSaved)
            {
                // Race::GetTotalPoints includes the local Human and every
                // live NetPlayer Opponent.  ProfileState owns only the local
                // profile, so capture the source PlayerList aggregate before
                // advancing Tournament.
                const auto tournamentTotalPoints =
                    raceSession.totalHumanOrOpponentPoints();
                const auto tournamentHumanCount =
                    raceSession.humanOrOpponentCount();
                const auto completedTrack = selectedTrack;
                const auto advance =
                    r3d::game::originalrace::
                        completeOriginalTournamentTrack(
                            *originalRace, selectedTrack, profileState,
                            tournamentTotalPoints,
                            tournamentHumanCount);
                raceTournamentAdvance = advance;
                selectedTrack = advance.trackIndex;
                racePlanetChampion = advance.planetChampion;
                if (advance.passComplete)
                {
                    // Race::CompleteRace resets points on the complete
                    // active PlayerList after Tournament::CompleteTrack.
                    // This must precede NetRace::ExitRace serialization.
                    raceSession.resetTournamentPassPoints();
                    weatherNightPassed = false;
                }
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
    };
    auto saveRaceProfile = [&]() {
        applyRaceProfileState();
        const auto persistedState = makePersistedProfileState();
        std::string profileError;
        if (!profileStore.save(persistedState, profileError))
            std::cerr << "Unable to save original profile: "
                      << profileError << '\n';
    };
    auto reloadCurrentRace = [&]() {
        try
        {
#ifdef RRR3D_AUDIO
            stopRaceAudio();
#endif
            raceHud.shutdown(*device);
            raceRenderer.shutdown(*device);
            *originalRace =
                r3d::game::originalrace::loadOriginalRace(
                    *resources, selectedTrack,
                    profileState.player.currentCar,
                    options->legacyWindowsDebug);
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
            if (options->legacyWindowsDebug)
                applyWeather(
                    originalRace->environment, "cloudy",
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
                            // NetPlayer constructors append Race::Player in
                            // model creation order: connected humans first,
                            // then host-created computers at StartRace. Model
                            // IDs are the shared order on every peer; local
                            // owner flags are deliberately not canonical.
                            return left.modelId < right.modelId;
                        });
                    // The serialized planet owns only cComputer1..5, while
                    // Race::cMaxPlayers permits eight active NetPlayers.
                    // Grow source-compatible templates before canonical
                    // network order replaces the list.
                    r3d::game::originalrace::
                        reconcileOriginalPlayerRoster(
                            *originalRace,
                            static_cast<std::uint32_t>(
                                std::min<std::size_t>(
                                    models.players.size() - 1U,
                                    r3d::game::originalrace::
                                        originalMaximumComputers)),
                            championshipMode);
                    const auto sourceRacers = originalRace->racers;
                    const auto count = std::min(
                        models.players.size(), sourceRacers.size());
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
                        racer.playerId =
                            player.playerId == 0U
                                ? (player.owner
                                       ? r3d::game::originalrace::source::
                                             Player::humanId
                                       : static_cast<int>(
                                             player.netSlot <<
                                             r3d::game::originalrace::source::
                                                 Player::opponentBit))
                                : static_cast<int>(player.playerId);
                        racer.gamerId = static_cast<std::uint32_t>(
                            std::max(player.gamerId, 0));
                        racer.netSlot = player.netSlot;
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
                    // Cloning a cComputer template for a later NetPlayer can
                    // duplicate its MapObj ID. Race::StartRace creates cars
                    // in the canonical PlayerList order, so restore the same
                    // contiguous dynamic IDs after the replacement pass.
                    r3d::game::originalrace::
                        reconcileOriginalPlayerRoster(
                            *originalRace,
                            static_cast<std::uint32_t>(count - 1U),
                            championshipMode);
                    networkRosterApplied = true;
                }
            }
#endif
            if (!networkRosterApplied && !championshipMode)
            {
                r3d::game::originalrace::
                    reconcileOriginalPlayerRoster(
                        *originalRace,
                        std::min<std::uint32_t>(
                            profileState.config.maxComputers,
                            r3d::game::originalrace::
                                originalMaximumComputers),
                        false);
            }
            if (!originalRace->racers.empty())
                originalRace->racers.front().name =
                    profileState.player.name;
            r3d::game::originalrace::applyOriginalPlayerProfile(
                *originalRace, *resources, profileState.player,
                achievementOpened("armor4"));
            r3d::game::originalrace::
                writeOriginalTournamentSelection(
                    *originalRace, selectedTrack,
                    profileState.player);
            originalRace->lapCount =
                r3d::game::originalrace::originalEffectiveLapCount(
                    *originalRace, championshipMode,
                    std::clamp<std::uint32_t>(
                        profileState.config.lapsCount, 1U, 8U));
            if (!championshipMode)
            {
                // Planet::StartPass invokes Garage::MaxUpgradeCar for every
                // computer created in skirmish, then removes primary weapon
                // mounts above GameMode::_weaponMaxLevel.
                r3d::game::originalrace::
                    applyOriginalSkirmishComputerConfig(
                        *originalRace, *originalGarage,
                        profileState.config.upgradeMaxLevel,
                        profileState.config.weaponMaxLevel,
                        profileState.player.difficulty);
            }
            *physicsDescription =
                r3d::game::originalrace::makePhysicsDescription(
                    *originalRace, *resources);
            std::string reloadError;
            physicsWorld =
                r3d::physics::createOriginalVehicleWorld(
                    *physicsDescription, reloadError);
            bindSourceVehicleFixedStep();
            decorationDebrisBindings.clear();
            decorationFragments.clear();
            vehicleDebrisBindings.clear();
            vehicleDeathFragments.clear();
            if (!physicsWorld ||
                !raceRenderer.initialize(
                    *device, originalResourceManager, *originalRace,
                    static_cast<std::uint32_t>(pixelWidth),
                    static_cast<std::uint32_t>(pixelHeight),
                    reloadError) ||
                !raceHud.initialize(
                    *device, originalResourceManager,
                    originalGameDataCatalog,
                    *originalRace,
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
            gameDebug.resetRaceState();
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
            raceRenderVehicles = raceVehicles;
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
        const auto humanRacer = raceSession.humanRacer();
        if (humanRacer < raceVehicles.size() &&
            humanRacer < originalRace->racers.size())
        {
            const auto& vehicle = raceVehicles[humanRacer];
            state.vehicle.position = {
                vehicle.body.position.x, vehicle.body.position.y,
                vehicle.body.position.z};
            state.vehicle.rotation = {
                vehicle.body.rotation.x, vehicle.body.rotation.y,
                vehicle.body.rotation.z, vehicle.body.rotation.w};
            float mass = originalRace->vehicle.physics.mass;
            if (humanRacer < originalRace->racers.size())
            {
                const auto& racer = originalRace->racers[humanRacer];
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
                raceInput.manualSteering &&
                        std::abs(raceInput.steering) > 0.01F
                    ? 3U
                    : (raceInput.steering > 0.01F
                           ? 1U
                           : (raceInput.steering < -0.01F ? 2U : 0U));
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
        // Tournament::_lapsCount is a GameMode option in both modes and is
        // always serialized by NetRace::WriteMatch. Track::numLaps remains
        // the effective campaign lap count on each peer.
        match.lapsCount = std::clamp<std::uint32_t>(
            profileState.config.lapsCount, 1U, 8U);
        match.maxPlayers = profileState.config.maxPlayers;
        match.maxComputers = profileState.config.maxComputers;
        match.springBorders = profileState.config.springBorders;
        match.enableMineBug = profileState.config.enableMineBug;
        if (selectedTrack < originalRace->trackCatalog.size())
        {
            const auto planet =
                originalRace->trackCatalog[selectedTrack].planetIndex;
            match.planet = static_cast<std::int32_t>(planet);
            match.track = static_cast<std::int32_t>(
                r3d::game::originalrace::
                    originalTournamentTrackIndexInPlanet(
                        *originalRace, selectedTrack));
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
            gameModeState.LoadingPresentedFrames() >= 2U;
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
            raceSession.synchronizeNetworkCountdown(
                options->legacyWindowsDebug ? 4 : 0);
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
            networkAppliedRaceGoStage =
                options->legacyWindowsDebug ? 4 : 0;
            networkHostFinishTimerStarted = false;
            networkRaceExitApplied = false;
            if (!publishLocalNetworkPlayer())
            {
                runtimeSmokeFailed = true;
                running = false;
                return;
            }
            std::string error;
            if (options->legacyWindowsDebug)
            {
                if (networkHostRequested &&
                    !networkSession.setRaceGoStage(4, error))
                {
                    std::cerr
                        << "Original Windows DEBUG_PX cGoRace publish "
                           "failed: "
                        << error << '\n';
                }
            }
            else if (!networkSession.setLocalPlayerGoWait(true, error))
            {
                std::cerr << "Original NetPlayer::RaceGoWait failed: "
                          << error << '\n';
            }
            else
            {
                networkLocalGoWaitPublished = true;
            }
        }
#endif
        raceRenderer.resetCamera();
        raceCameraStyle =
            profileState.config.preferredCamera ==
                    r3d::game::originalrace::PreferredCamera::ThirdPerson
                ? rrr3d::race::RaceCameraStyle::ThirdPerson
                : rrr3d::race::RaceCameraStyle::Isometric;
        raceInput = {};
        raceUseWeaponRequested = false;
        raceUseAllWeaponsRequested = false;
        raceUseMineRequested = false;
        raceMineAnalogBinding = false;
        raceUseHyper = false;
        raceChangeWeaponRequested = false;
        raceWeaponChangeDirection = 1;
        raceFireWeaponSlotRequested = -1;
        raceResetRequested = false;
        gameDebug.resetRaceState();
        if (gameDebug.enabled() && options->raceRenderSmokeTest)
        {
            // Exercise the F6 TraceGfx submission in the automated Metal
            // race path. Ordinary --game-debug launches still start hidden,
            // matching AIDebug's constructor.
            gameDebug.handle(
                rrr3d::input::Action::Debug6, true, false);
        }
        raceSession.setDebugHumanAiControl(false);
        exitRaceDialogVisible = false;
        exitRaceYesFocused = true;
        racePauseElapsedSnapshot = -1.0F;
        raceElapsedSeconds = 0.0F;
        raceProgressSaved = false;
        raceExitLifecycleApplied = false;
        sourceFinishFrame.hide();
        raceVehicles.resize(physicsWorld->vehicleCount());
        maximumRaceAiSpeeds.assign(raceVehicles.size(), 0.0F);
        maximumRaceAiProgress.assign(raceVehicles.size(), 0.0F);
        raceAiThrottleFrames.assign(raceVehicles.size(), 0U);
        raceAiBrakeFrames.assign(raceVehicles.size(), 0U);
        raceAiReverseFrames.assign(raceVehicles.size(), 0U);
        for (std::size_t index = 0;
             index < physicsWorld->vehicleCount(); ++index)
            raceVehicles[index] = physicsWorld->vehicle(index);
        raceRenderVehicles = raceVehicles;
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
        if (inRace || gameModeState.IsRaceLoading())
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
        if (!gameModeState.IsMatchStarted())
            gameModeState.StartMatch();
        if (!gameModeState.StartRace())
            return;
        const auto startCommands = gameModeState.TakeCommands();
        if (startCommands !=
            std::vector<r3d::game::originalrace::source::GameModeCommand>{
                r3d::game::originalrace::source::GameModeCommand::
                    ShowRaceInfo})
        {
            runtimeSmokeFailed = true;
            running = false;
            return;
        }
#ifdef RRR3D_AUDIO
        sourceDialogs.HideMusicInfo();
#endif
        previousFrameTicks = SDL_GetTicksNS();
        std::cout
            << "Original GameMode::StartRace -> Menu::msInfo\n";
    };
    auto clearRaceControls = [&]() {
        raceInput = {};
        raceUseWeaponRequested = false;
        raceUseAllWeaponsRequested = false;
        raceUseMineRequested = false;
        raceUseHyper = false;
        raceChangeWeaponRequested = false;
        raceWeaponChangeDirection = 1;
        raceFireWeaponSlotRequested = -1;
        raceResetRequested = false;
    };
    auto setRacePaused = [&](bool paused) {
        // Exact GameMode::Pause boundary: the world clock is stopped while
        // Logic::scEffects is volume-muted. Music and Voice are deliberately
        // untouched by the Windows source.
        worldEventPump.Pause(paused);
        gameModeState.Pause(paused);
        static_cast<void>(gameModeState.TakeCommands());
        raceSession.setPaused(worldEventPump.IsPaused());
#ifdef RRR3D_AUDIO
        audio.setBusVolume(
            r3d::audio::Bus::Effects,
            raceSession.effectsMuted()
                ? 0.0F
                : profileState.config.effectsVolume);
#endif
    };
    auto closeExitRaceDialog = [&]() {
        const auto humanRacer = raceSession.humanRacer();
        if (options->raceRenderSmokeTest &&
            racePauseElapsedSnapshot >= 0.0F &&
            humanRacer < physicsWorld->vehicleCount())
        {
            const auto currentPosition =
                physicsWorld->vehicle(humanRacer).body.position;
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
        setRacePaused(false);
#ifdef RRR3D_AUDIO
        raceEffectsMuteObserved = raceEffectsMuteObserved &&
            std::abs(audio.busVolume(r3d::audio::Bus::Effects) -
                     profileState.config.effectsVolume) < 0.0001F;
#endif
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
#ifdef RRR3D_NETWORK
        // NetRace::Pause is intentionally inert in the original game.  The
        // confirmation remains modal for local input, but the network world
        // must continue to simulate behind it.
        if (!networkMatchStarted)
#endif
            setRacePaused(true);
#ifdef RRR3D_AUDIO
        raceEffectsMuteObserved = raceEffectsMuteObserved ||
            audio.busVolume(r3d::audio::Bus::Effects) == 0.0F;
#endif
        clearRaceControls();
        racePauseElapsedSnapshot = raceSession.elapsedSeconds();
        const auto humanRacer = raceSession.humanRacer();
        if (humanRacer < physicsWorld->vehicleCount())
            racePausePositionSnapshot =
                physicsWorld->vehicle(humanRacer).body.position;
        racePauseDialogObserved = true;
    };
    std::function<void(bool)> showFinishMenu;
    auto applyOriginalRaceExitLifecycle = [&]() {
        if (raceExitLifecycleApplied)
            return;
        // Race::ExitRace advances the shared tutorial and lowers the
        // SnProfile minimum difficulty exactly once while _startRace is set.
        // Both natural completion and the HudMenu exit pass this boundary.
        if (profileState.tutorialStage < 3U)
            ++profileState.tutorialStage;
        r3d::game::originalrace::completeOriginalRaceDifficulty(
            profileState.player);
        raceExitLifecycleApplied = true;
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
            const auto* sourceResult =
                raceSession.resultForRacer(index);
            result.playerModelId = networkRaceModelOrder[index];
            result.playerPoints = toSourceInt(racer.GetPoints());
            result.playerMoney = toSourceInt(racer.GetMoney());
            result.money = toSourceInt(
                sourceResult != nullptr ? sourceResult->money
                                        : racer.rewardMoney);
            result.pickedMoney = toSourceInt(
                sourceResult != nullptr ? sourceResult->pickedMoney
                                        : racer.GetPickMoney());
            result.place = sourceResult != nullptr
                               ? sourceResult->place
                               : racer.GetPlace();
            result.points = toSourceInt(
                sourceResult != nullptr ? sourceResult->points
                                        : racer.rewardPoints);
            results.push_back(result);
        }
        return results;
    };
#endif
    auto leaveCurrentRace = [&](bool publishNetworkRaceExit = true) {
        // Menu::ExitRace -> GameMode::ExitRace -> Race::ExitRace always
        // completes/ranks every remaining player before saving or publishing
        // network results, even for an early HudMenu exit.
        raceSession.completeRaceForExit(raceVehicles);
        applyOriginalRaceExitLifecycle();
        saveRaceProfile();
#ifdef RRR3D_NETWORK
        if (publishNetworkRaceExit && networkMatchStarted &&
            networkHostRequested && networkRaceStarted &&
            !networkRaceExitApplied)
        {
            std::string error;
            if (!networkSession.exitRace(
                    static_cast<std::int32_t>(
                        r3d::game::originalrace::
                            originalTournamentTrackIndexInPlanet(
                                *originalRace, selectedTrack)),
                    static_cast<std::int32_t>(
                        originalRace->environment.weather),
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
        setRacePaused(false);
        exitRaceDialogVisible = false;
        if (showFinishMenu)
            showFinishMenu(false);
        else
        {
            inRace = false;
            clearRaceControls();
#ifdef RRR3D_AUDIO
            stopRaceAudio();
#endif
            previousFrameTicks = SDL_GetTicksNS();
        }
        std::cout << "Original HudMenu accept: Race -> FinishMenu\n";
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
        sourceDialogs.HideWeapon();
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
                std::abs(sourceDialogs.Weapon().center.x - centerX) <
                    0.01F &&
                std::abs(sourceDialogs.Weapon().center.y - centerY) <
                    0.01F &&
                valid(workshopWeaponDialog.name.texture))
            {
                const auto& weapon = sourceDialogs.Weapon();
                sourceDialogs.ShowWeapon(
                    weapon.title, weapon.message,
                    weapon.moneyText, weapon.damageText,
                    {centerX, centerY}, originalmenu::Anchor::Center,
                    {frameWidth, frameHeight}, 0.0F);
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
            sourceDialogs.ShowWeapon(
                localized(item.name), localized(item.info),
                originalCurrency(cost), damage,
                {centerX, centerY}, originalmenu::Anchor::Center,
                {frameWidth, frameHeight}, 0.0F);
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
        sourceDialogs.HideInfo();
    };
    auto showInfoDialog =
        [&](std::string_view title, std::string_view message,
            std::string_view ok, float centerX, float centerY) {
            const float frameWidth =
                static_cast<float>(infoDialogFrameImage.width);
            const float frameHeight =
                static_cast<float>(infoDialogFrameImage.height);
            const auto& sourceDialog = sourceDialogs.ShowInfo(
                std::string(title), std::string(message),
                std::string(ok), {centerX, centerY},
                originalmenu::Anchor::Center,
                {frameWidth, frameHeight});
            destroyInfoDialog(*device, infoDialog);
            infoDialog.title = createText(
                *device, sourceDialog.title, 44.0F, false,
                menu::Rgba8{175, 175, 175, 255}, resolvedFont);
            for (const auto& line :
                 wrapInfoDialogMessage(sourceDialog.message))
            {
                infoDialog.info.push_back(createText(
                    *device, line, 24.0F, false,
                    menu::Rgba8{255, 255, 255, 255},
                    resolvedFont));
            }
            infoDialog.ok = createText(
                *device, sourceDialog.okText, 32.0F, false,
                menu::Rgba8{255, 255, 255, 255}, resolvedFont);
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
        sourceDialogs.SetInfoDismissable(false);
    };
#ifdef RRR3D_NETWORK
    auto exitNetworkMatch = [&](bool publishExitRpc) {
        networkFailureDialogAction =
            NetworkFailureDialogAction::None;
        if (inRace)
            leaveCurrentRace(false);
        else
        {
            setRacePaused(false);
            gameModeState.CancelRaceStart();
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
        gameModeState.CancelRaceStart();
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
        bool restoredNetworkProfile = false;
        if (networkClientOfflineProfile)
        {
            profileState.player = *networkClientOfflineProfile;
            networkClientOfflineProfile.reset();
            restoredNetworkProfile = true;
        }
        else if (networkHostOfflineProfile)
        {
            profileState.player = *networkHostOfflineProfile;
            networkHostOfflineProfile.reset();
            restoredNetworkProfile = true;
        }
        if (networkClientLocalConfig)
        {
            profileState.config = *networkClientLocalConfig;
            networkClientLocalConfig.reset();
        }
        if (restoredNetworkProfile)
        {
            championshipPlayerBeforeSkirmish.reset();
            championshipMode = true;
            selectedTrack =
                r3d::game::originalrace::
                    resolveOriginalTournamentTrack(
                        *originalRace, profileState.player);
            raceProgressSaved = false;
            if (!reloadCurrentRace())
            {
                runtimeSmokeFailed = true;
                running = false;
                return;
            }
            refreshProfilePage();
        }
        else if (!restoreChampionshipProfile())
        {
            runtimeSmokeFailed = true;
            running = false;
            return;
        }
        menuStack = {MenuScreen::Main};
        menuSelection = 0U;
        refreshSharedMenuAvailability(MenuScreen::Main);
        gameModeState.ExitMatch();
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
            inRace || gameModeState.IsRaceLoading();
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
            setRacePaused(true);
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
            sourceDialogs.Info().visible &&
            sourceDialogs.Info().dismissable;
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
                stopRaceLoopVoice(engineAudio[racer].idleVoice);
                stopRaceLoopVoice(engineAudio[racer].rpmVoice);
            }
            if (racer < wheelSlipVoices.size())
            {
                for (auto& wheel : wheelSlipVoices[racer])
                {
                    stopRaceLoopVoice(wheel.voice);
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
                showAcceptDialog(
                    localized("svHintLeaversWillBeRemoved"),
                    localized("svYes"), localized("svNo"),
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
    auto sourceGarageColors = [&]() {
        std::array<
            bool,
            r3d::game::originalracemenu::GarageFrameState::colorCount>
            available{};
        for (std::size_t index = 0U; index < available.size(); ++index)
            available[index] = garageColorAvailable(index);
        return available;
    };
    auto rebuildGarageCarOrder = [&]() {
        using GarageCarCandidate =
            r3d::game::originalracemenu::GarageCarCandidate;
        std::vector<GarageCarCandidate> candidates;
        candidates.reserve(originalGarage->cars.size());
        std::size_t currentCatalogIndex = 0U;
        for (std::size_t index = 0U;
             index < originalGarage->cars.size(); ++index)
        {
            const auto& car = originalGarage->cars[index];
            if (car.record == profileState.player.currentCar)
                currentCatalogIndex = index;
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
            candidates.push_back(
                {index, isSecret, unlocked, achievement});
        }
        sourceGarageFrame.show(
            std::move(candidates), currentCatalogIndex,
            championshipMode, sourceGarageColors());
    };
    auto refreshGaragePage = [&]() {
        if (originalGarage->cars.empty())
            return;
        if (sourceGarageFrame.empty())
            rebuildGarageCarOrder();
        const auto* selectedEntry = sourceGarageFrame.selectedCar();
        if (selectedEntry == nullptr ||
            selectedEntry->catalogIndex >= originalGarage->cars.size())
            return;
        const auto& car =
            originalGarage->cars[selectedEntry->catalogIndex];
        const bool locked = selectedEntry->locked;
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
    auto sourceGamerEntries = [&]() {
        std::vector<r3d::game::originalracemenu::GamerEntry>
            entries;
        entries.reserve(originalGarage->gamers.size());
        for (std::size_t index = 0U;
             index < originalGarage->gamers.size(); ++index)
        {
            entries.push_back({
                originalGarage->gamers[index].bossId,
                gamerUnlocked(index)});
        }
        return entries;
    };
    auto syncSourceGamers = [&]() {
        sourceGamersFrame.updateEntries(sourceGamerEntries());
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
        const auto gamerPlanetIndex = std::min(
            sourceGamersFrame.selection(),
            originalGarage->gamers.size() - 1U);
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
    auto showOriginalGamers = [&]() {
        sourceGamersFrame.show(
            sourceGamerEntries(), profileState.player.gamerId);
        sourceRaceMenu.setState(
            r3d::game::originalracemenu::State::Gamers);
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
                  << originalGarage
                         ->gamers[sourceGamersFrame.selection()]
                         .record
                  << '\n';
    };
    auto showOriginalGarageAfterGamers = [&]() {
        sourceRaceMenu.setState(
            r3d::game::originalracemenu::State::Garage);
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
        const auto gamerPlanetIndex = sourceGamersFrame.selection();
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
        if (!options->legacyWindowsDebug && championshipMode &&
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
    auto sourceWorkshopCandidates = [&]() {
        std::vector<
            r3d::game::originalracemenu::WorkshopGoodCandidate>
            candidates;
        candidates.reserve(originalGarage->workshop.size());
        for (std::size_t index = 0U;
             index < originalGarage->workshop.size(); ++index)
        {
            const auto& item = originalGarage->workshop[index];
            candidates.push_back({
                index, item.cost, item.type <= 4U,
                r3d::game::originalrace::
                    originalWorkshopItemUnlocked(
                        *originalGarage, profileState, item)});
        }
        return candidates;
    };
    auto sourceWorkshopSlots = [&]() {
        using WorkshopSlotState =
            r3d::game::originalracemenu::WorkshopSlotState;
        constexpr std::size_t slotCount =
            r3d::game::originalracemenu::WorkshopFrameState::slotCount;
        std::array<WorkshopSlotState, slotCount> slots{};
        const auto* car = originalGarage->findCar(
            profileState.player.currentCar);
        if (car == nullptr)
            return slots;
        for (std::size_t index = 0U; index < slots.size(); ++index)
        {
            const auto& placement = car->placements[index];
            auto& state = slots[index];
            state.active =
                placement.active &&
                (championshipMode ||
                 index < r3d::game::originalrace::PlayerProfile::
                             firstWeaponSlot ||
                 index - r3d::game::originalrace::PlayerProfile::
                             firstWeaponSlot <
                     profileState.config.weaponMaxLevel);
            state.locked = placement.locked;
            const auto& installed = profileState.player.slots[index];
            const auto* item = originalGarage->findItem(installed.record);
            state.installed = item != nullptr;
            if (item == nullptr)
                continue;
            state.chargeControlVisible = item->maximumCharge > 0U;
            state.levelControlVisible = item->maximumCharge == 0U;
            if (state.chargeControlVisible)
            {
                const std::uint32_t charge =
                    installed.hasCharge ? installed.charge
                                        : item->defaultCharge;
                state.controlEnabled = charge < item->maximumCharge;
            }
            else
            {
                const auto slotType = static_cast<
                    r3d::game::originalrace::GarageSlotType>(index);
                const int level = r3d::game::originalrace::
                    originalWorkshopUpgradeLevel(
                        installed.record, slotType);
                state.controlEnabled =
                    level < 2 &&
                    (championshipMode ||
                     level < static_cast<int>(
                                 profileState.config.upgradeMaxLevel));
            }
        }
        return slots;
    };
    auto refreshWorkshopPage = [&]() {
        sourceWorkshopFrame.updateGoods(sourceWorkshopCandidates());
        sourceWorkshopFrame.updateSlots(sourceWorkshopSlots());

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
                    *originalGarage, *car, profileState.player,
                    achievementOpened("armor4"));
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
    auto sourceWorkshopLayout = [&]() {
        return sourceWorkshopFrame.layout(
            menu::virtualWidth, menu::virtualHeight,
            static_cast<float>(workshopTopPanelImage.height),
            static_cast<float>(workshopBottomPanelImage.height),
            static_cast<float>(workshopLeftPanelImage.width),
            static_cast<float>(workshopLeftPanelImage.height),
            static_cast<float>(workshopSlotImage.width),
            static_cast<float>(workshopSlotImage.height));
    };
    auto workshopSlotCenters = [&]() {
        return sourceWorkshopLayout().slots;
    };
    auto workshopGoodCenters = [&]() {
        return sourceWorkshopLayout().goods;
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
            const auto pending = std::find_if(
                originalGarage->workshop.begin(),
                originalGarage->workshop.end(),
                [&](const auto& candidate) {
                    return candidate.record == item.record;
                });
            const std::size_t pendingCatalogIndex =
                pending == originalGarage->workshop.end()
                    ? static_cast<std::size_t>(-1)
                    : static_cast<std::size_t>(std::distance(
                          originalGarage->workshop.begin(), pending));
            sourceWorkshopFrame.beginConfirmation(
                confirmation, pendingCatalogIndex);
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
            sourceWorkshopFrame.startDrag(
                std::move(purchased), std::nullopt);
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
        if (!sourceWorkshopFrame.drag().active())
            return true;
        std::string workshopError;
        if (!intoGoods && sourceWorkshopFrame.drag().origin)
        {
            const auto originIndex =
                static_cast<std::size_t>(
                    *sourceWorkshopFrame.drag().origin);
            if (originIndex < profileState.player.slots.size() &&
                profileState.player.slots[originIndex].record.empty())
            {
                r3d::game::originalrace::ProfileSlot replaced;
                if (!r3d::game::originalrace::
                        installOriginalWorkshopSlot(
                            *originalGarage, profileState,
                            *sourceWorkshopFrame.drag().origin,
                            sourceWorkshopFrame.drag().item,
                            replaced, workshopError))
                {
                    std::cerr
                        << "Original WorkshopFrame restore: "
                        << workshopError << '\n';
                    return false;
                }
                sourceWorkshopFrame.clearDrag();
#ifdef RRR3D_AUDIO
                playOriginalMenuSound(
                    rrr3d::audio::OriginalMenuSound::PickupUp);
#endif
                saveRaceProfile();
                refreshWorkshopPage();
                return true;
            }
        }
        const bool discount =
            sourceWorkshopFrame.drag().origin.has_value();
        if (championshipMode && discount && !accepted)
        {
            const auto* item = originalGarage->findItem(
                sourceWorkshopFrame.drag().item.record);
            if (item == nullptr)
                return false;
            showWorkshopConfirmation(
                WorkshopConfirmation::Sell, *item,
                r3d::game::originalrace::
                    originalWorkshopSellValue(
                        *originalGarage,
                        sourceWorkshopFrame.drag().item, true));
            return false;
        }
        if (!r3d::game::originalrace::sellOriginalWorkshopItem(
                *originalGarage, profileState,
                sourceWorkshopFrame.drag().item,
                discount, championshipMode, workshopError))
        {
            std::cerr << "Original WorkshopFrame sell: "
                      << workshopError << '\n';
            return false;
        }
        sourceWorkshopFrame.clearDrag();
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
            constexpr std::size_t firstGoodFocus =
                r3d::game::originalracemenu::WorkshopFrameState::
                    firstGoodFocus;
            constexpr std::size_t firstSlotFocus =
                r3d::game::originalracemenu::WorkshopFrameState::
                    firstSlotFocus;
            if (menuSelection == 0U)
            {
                if (sourceWorkshopFrame.drag().active())
                    stopWorkshopDrag(false);
                else
                    backMenu();
                return;
            }
            if (menuSelection < firstSlotFocus)
            {
                if (sourceWorkshopFrame.drag().active())
                {
                    stopWorkshopDrag(true);
                    return;
                }
                const auto* good = sourceWorkshopFrame.visibleGood(
                    menuSelection - firstGoodFocus);
                if (good == nullptr ||
                    good->catalogIndex >=
                        originalGarage->workshop.size())
                    return;
                const auto* item = &originalGarage->workshop[
                    good->catalogIndex];
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
            if (sourceWorkshopFrame.drag().active())
            {
                if (!workshopSlotAccepts(
                        slotIndex, sourceWorkshopFrame.drag().item))
                    return;
                r3d::game::originalrace::ProfileSlot replaced;
                std::string workshopError;
                if (!r3d::game::originalrace::
                        installOriginalWorkshopSlot(
                            *originalGarage, profileState, slotType,
                            sourceWorkshopFrame.drag().item, replaced,
                            workshopError))
                {
                    std::cerr
                        << "Original WorkshopFrame install: "
                        << workshopError << '\n';
                    return;
                }
                if (replaced.record.empty())
                {
                    sourceWorkshopFrame.clearDrag();
#ifdef RRR3D_AUDIO
                    playOriginalMenuSound(
                        rrr3d::audio::OriginalMenuSound::PickupUp);
#endif
                }
                else
                {
                    sourceWorkshopFrame.startDrag(
                        std::move(replaced), slotType);
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
                sourceWorkshopFrame.startDrag(installed, slotType);
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
    auto sourceAngarPlanets = [&]() {
        using PlanetEntry =
            r3d::game::originalracemenu::AngarPlanetEntry;
        using PlanetState =
            r3d::game::originalracemenu::AngarPlanetState;
        std::vector<PlanetEntry> result;
        const auto count = std::min(
            originalGarage->planets.size(),
            profileState.player.planets.size());
        result.reserve(count);
        for (std::size_t index = 0U; index < count; ++index)
        {
            PlanetState state = PlanetState::Completed;
            switch (profileState.player.planets[index].state)
            {
            case 0U:
                state = PlanetState::Open;
                break;
            case 1U:
                state = PlanetState::Closed;
                break;
            case 2U:
                state = PlanetState::Unavailable;
                break;
            default:
                break;
            }
            result.push_back(
                {state,
                 index == profileState.player.currentPlanet,
                 racePlanetChampion &&
                     index == profileState.player.currentPlanet + 1U});
        }
        return result;
    };
    auto sourceAngarNetworkClient = [&]() {
#ifdef RRR3D_NETWORK
        return networkClientMatchEntered;
#else
        return false;
#endif
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
        if (sourceAngarFrame.selection() >= 0 &&
            static_cast<std::size_t>(sourceAngarFrame.selection()) < count)
        {
            const auto& planet = originalGarage->planets[
                static_cast<std::size_t>(sourceAngarFrame.selection())];
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
        const auto& progress = profileState.player.planets[index];
        const bool newPlanet =
            progress.state == 1U || progress.state == 2U;
        if (!r3d::game::originalrace::changeOriginalTournamentPlanet(
                *originalRace, index, profileState.player,
                racePlanetChampion))
        {
            std::cerr << "Original Tournament::ChangePlanet failed\n";
            runtimeSmokeFailed = true;
            return;
        }
        weatherNightPassed = false;
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
        sourceAngarFrame.cancelTravel();
#ifdef RRR3D_VIDEO
        if (!options->legacyWindowsDebug && newPlanet &&
            championshipMode &&
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
    auto showAngarTravelDialog =
        [&](std::size_t index, bool fromPlanetSlot) {
        const auto key =
            index == profileState.player.currentPlanet
                ? "svYouReadyStayPlanet"
                : "svYouReadyFlyPlanet";
        float posX = menu::virtualWidth * 0.5F;
        float posY = menu::virtualHeight * 0.5F;
        if (fromPlanetSlot)
        {
            const auto angarLayout = sourceAngarFrame.layout(
                menu::virtualWidth, menu::virtualHeight,
                static_cast<float>(angarBottomPanelImage.width),
                static_cast<float>(angarBottomPanelImage.height),
                static_cast<float>(angarPlanetInfoImage.width),
                static_cast<float>(angarPlanetInfoImage.height),
                static_cast<float>(garageBackImage.width));
            posX = angarLayout.planetX(index);
            posY = angarLayout.planetY -
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
    auto sourceAchievementEntries = [&]() {
        using Entry =
            r3d::game::originalracemenu::AchievementEntry;
        using State =
            r3d::game::originalracemenu::AchievementState;
        std::array<
            Entry,
            r3d::game::originalracemenu::
                AchievementFrameState::achievementCount>
            result{};
        for (std::size_t index = 0U;
             index < originalAchievementVisuals.size(); ++index)
        {
            const auto item = profileState.achievementItems.find(
                std::string(originalAchievementVisuals[index].name));
            if (item == profileState.achievementItems.end())
                continue;
            const auto state = item->second.values.find("state");
            if (state != item->second.values.end())
            {
                result[index].state =
                    state->second == "asLocked" ? State::Locked
                    : state->second == "asUnlocked" ? State::Unlocked
                    : state->second == "asOpened" ? State::Opened
                                                   : State::Missing;
            }
            const auto price = item->second.values.find("price");
            if (price != item->second.values.end())
            {
                const auto parsed = std::from_chars(
                    price->second.data(),
                    price->second.data() + price->second.size(),
                    result[index].price);
                if (parsed.ec != std::errc{})
                    result[index].price = 0U;
            }
        }
        return result;
    };
    auto refreshAchievementsPage = [&]() {
        sourceAchievementFrame.update(sourceAchievementEntries());
        std::vector<std::string> prices;
        prices.reserve(originalAchievementVisuals.size());
        for (std::size_t index = 0U;
             index < originalAchievementVisuals.size(); ++index)
        {
            prices.push_back(
                sourceAchievementFrame.entries()[index].state ==
                        r3d::game::originalracemenu::
                            AchievementState::Unlocked
                    ? originalCurrency(
                          sourceAchievementFrame.entries()[index].price)
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
        menuSelection = sourceAchievementFrame.focus();
    };
    auto achievementState = [&](std::size_t index) {
        const auto* entry = sourceAchievementFrame.entry(index);
        return entry == nullptr
                   ? r3d::game::originalracemenu::
                         AchievementState::Missing
                   : entry->state;
    };
    auto achievementPrice = [&](std::size_t index) {
        const auto* entry = sourceAchievementFrame.entry(index);
        return entry == nullptr ? 0U : entry->price;
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
        sourceRaceMenu.setState(
            r3d::game::originalracemenu::State::Main);
        sourceRaceMainFrame.show(false);
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
        saveRaceProfile();
        if (!networkClientOfflineProfile)
            networkClientOfflineProfile = profileState.player;
        if (!networkClientLocalConfig)
            networkClientLocalConfig = profileState.config;
        championshipMode = match.mode == 0;
        profileState.config.upgradeMaxLevel = static_cast<std::uint32_t>(
            std::clamp(match.upgradeMaxLevel, 0, 2));
        profileState.config.weaponMaxLevel = static_cast<std::uint32_t>(
            std::clamp(match.weaponMaxLevel, 1, 4));
        profileState.config.lapsCount =
            std::clamp<std::uint32_t>(match.lapsCount, 1U, 8U);
        profileState.config.maxPlayers =
            std::clamp<std::uint32_t>(
                match.maxPlayers, 2U,
                r3d::game::originalrace::originalMaximumPlayers);
        profileState.config.maxComputers =
            std::min<std::uint32_t>(
                match.maxComputers,
                r3d::game::originalrace::originalMaximumComputers);
        profileState.config.springBorders = match.springBorders;
        profileState.config.enableMineBug = match.enableMineBug;

        // Race::NewProfile(..., netClient=true) enters the dedicated
        // championshipClient SnProfile rather than the current local save.
        auto decodedProfile =
            r3d::game::originalrace::makeOriginalDefaultProfileState()
                .player;
        decodedProfile.name = "championshipClient";
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
        profileState.player = std::move(decodedProfile);

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
    int pendingPixelWidth = pixelWidth;
    int pendingPixelHeight = pixelHeight;
    bool drawableResizePending = false;
    auto requestDrawableResize = [&](int width, int height) {
        pendingPixelWidth = std::max(width, 1);
        pendingPixelHeight = std::max(height, 1);
        drawableResizePending = true;
    };
    auto applyPendingDrawableResize = [&]() {
        if (!drawableResizePending)
            return true;
        drawableResizePending = false;
        if (pendingPixelWidth == pixelWidth &&
            pendingPixelHeight == pixelHeight)
        {
            return true;
        }
        pixelWidth = pendingPixelWidth;
        pixelHeight = pendingPixelHeight;
        menu::virtualWidth = static_cast<float>(pixelWidth);
        menu::virtualHeight = static_cast<float>(pixelHeight);
        device->resize(static_cast<std::uint32_t>(pixelWidth),
                       static_cast<std::uint32_t>(pixelHeight));
        // The original GUI projection follows the active D3D backbuffer.
        // Rebuild it together with bgfx so resolution/fullscreen changes do
        // not keep hit testing and rendering in different coordinate spaces.
        camera = makeCamera(*device);
#ifdef RRR3D_VIDEO
        videoPlayer.resize();
#endif
#ifdef RRR3D_PHYSICS
        std::string resizeError;
        if (!raceRenderer.resize(
                *device, static_cast<std::uint32_t>(pixelWidth),
                static_cast<std::uint32_t>(pixelHeight), resizeError) ||
            !garageRenderer.resize(
                *device, static_cast<std::uint32_t>(pixelWidth),
                static_cast<std::uint32_t>(pixelHeight), resizeError) ||
            !angarRenderer.resize(
                *device, static_cast<std::uint32_t>(pixelWidth),
                static_cast<std::uint32_t>(pixelHeight), resizeError))
        {
            std::cerr << "Unable to resize M9.3 render targets: "
                      << resizeError << '\n';
            return false;
        }
#endif
        return true;
    };
    auto requestSynchronizedWindowDrawable = [&]() {
        // SDL_SyncWindow has finalized the Cocoa operation. Discard only
        // stale geometry notifications produced during that transition;
        // mouse and keyboard events remain untouched.
        SDL_FlushEvent(SDL_EVENT_WINDOW_RESIZED);
        SDL_FlushEvent(SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED);
        SDL_FlushEvent(SDL_EVENT_WINDOW_METAL_VIEW_RESIZED);
        int width = 0;
        int height = 0;
        if (!SDL_GetWindowSizeInPixels(window, &width, &height))
            return false;
        requestDrawableResize(width, height);
        return true;
    };
    auto refreshCurrentOptionsPage = [&]() {
        switch (menuStack.back())
        {
        case MenuScreen::GameOptions:
            replaceOptionsPage(
                gameOptionsPage, gameOptionsLabels());
            {
                r3d::game::originaloptions::Availability availability;
                availability.difficulty =
                    !profileState.player.name.empty();
#ifdef RRR3D_NETWORK
                availability.networkClient =
                    networkMatchStarted && !networkHostRequested;
#endif
                gameOptionsPage.enabled =
                    sourceOptionsMenu.enabledItems(
                        MenuScreen::GameOptions, availability);
                if (menuSelection < gameOptionsPage.enabled.size() &&
                    !gameOptionsPage.enabled[menuSelection])
                {
                    menuSelection = 2U;
                }
            }
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
        if (selectionChanged && !inRace &&
            !gameModeState.IsRaceLoading())
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
            std::clamp<std::uint32_t>(
                match.maxPlayers, 2U,
                r3d::game::originalrace::originalMaximumPlayers);
        const auto computers =
            std::min<std::uint32_t>(
                match.maxComputers,
                r3d::game::originalrace::originalMaximumComputers);
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
        return r3d::game::originaloptions::OptionsMenuState::owns(screen);
    };
    auto optionsStateIndex = [&](MenuScreen screen) {
        return static_cast<std::size_t>(
            r3d::game::originaloptions::OptionsMenuState::tabForScreen(
                screen));
    };
    auto refreshStartOptionsValues = [&]() {
        auto replacement = createStyledPage(
            startOptionsValues(), menu::smallFontHeight,
            optionsTextColor, menu::selectedTextColor);
        destroyPage(startOptionsValuePage);
        startOptionsValuePage = std::move(replacement);
    };
    auto adjustStartOption = [&](int direction) {
#ifdef RRR3D_AUDIO
        playOriginalMenuSound(
            rrr3d::audio::OriginalMenuSound::ChangeOption);
#endif
        if (!sourceStartOptionsMenu.adjustFocused(direction))
            return;
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
        const auto& resolution = sourceStartOptionsMenu.resolution();
        std::cout
            << "Original StartOptionsMenu -> MainMenu2: camera="
            << (sourceStartOptionsMenu.cameraIndex() == 0U
                    ? "pcThirdPerson"
                    : "pcIsometric")
            << ", resolution=" << resolution.first << 'x'
            << resolution.second << ", language="
            << sourceStartOptionsMenu.language()
            << ", commentator="
            << sourceStartOptionsMenu.commentator()
            << '\n';
    };
    auto applyStartOptions = [&]() {
        const auto previous = profileState.config;
        const auto sourceApply =
            sourceStartOptionsMenu.apply(profileState.config);
        if (!sourceApply.applied)
            return;
        profileState.config.discreteVideoCard =
            sourceCurrentDiscreteVideoCard;
        profileState.preferredCameraSerialized = true;
        profileState.discreteVideoCardSerialized = true;
        raceRenderer.resetCamera();
        if (!options->startOptionsSmokeTest &&
            (profileState.config.resolutionWidth !=
                 previous.resolutionWidth ||
             profileState.config.resolutionHeight !=
                 previous.resolutionHeight))
        {
            std::string windowError;
            if (!applyOriginalWindowMode(
                    window, sourceDisplayModes,
                    profileState.config.resolutionWidth,
                    profileState.config.resolutionHeight,
                    profileState.config.fullScreen, true,
                    windowError) ||
                !requestSynchronizedWindowDrawable())
            {
                std::cerr
                    << "Unable to apply StartOptionsMenu resolution: "
                    << (windowError.empty() ? SDL_GetError()
                                            : windowError)
                    << '\n';
                profileState.config.resolutionWidth =
                    previous.resolutionWidth;
                profileState.config.resolutionHeight =
                    previous.resolutionHeight;
            }
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
        const auto tab = static_cast<r3d::game::originaloptions::Tab>(
            std::min<std::size_t>(state, 3U));
        sourceOptionsMenu.setTab(tab);
        menuStack.set_back(
            r3d::game::originaloptions::OptionsMenuState::screenForTab(tab));
        menuSelection = 0U;
        bindingCaptureAction.reset();
        refreshCurrentOptionsPage();
    };
    auto beginOriginalOptions = [&]() {
        sourceOptionsMenu.begin(
            profileState.config, profileState.player.difficulty);
        sourceOptionsMenu.setControlsUseGamepad(false);
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
        sourceOptionsMenu.cancel(
            profileState.config, profileState.player.difficulty);
        bindingCaptureAction.reset();
        backMenu();
    };
    auto applyOriginalOptions = [&]() {
        const auto previousConfig = profileState.config;
        sourceOptionsMenu.commit(
            profileState.config, profileState.player.difficulty);

        const bool windowConfigurationChanged =
            profileState.config.fullScreen !=
                previousConfig.fullScreen ||
            profileState.config.resolutionWidth !=
                previousConfig.resolutionWidth ||
            profileState.config.resolutionHeight !=
                previousConfig.resolutionHeight;
        if (windowConfigurationChanged)
        {
            std::string windowError;
            if (!applyOriginalWindowMode(
                    window, sourceDisplayModes,
                    profileState.config.resolutionWidth,
                    profileState.config.resolutionHeight,
                    profileState.config.fullScreen, true,
                    windowError) ||
                !requestSynchronizedWindowDrawable())
            {
                std::cerr
                    << "Unable to apply original display mode: "
                    << (windowError.empty() ? SDL_GetError()
                                            : windowError)
                    << '\n';
                profileState.config.fullScreen =
                    previousConfig.fullScreen;
                optionsDraftConfig.fullScreen =
                    previousConfig.fullScreen;
                profileState.config.resolutionWidth =
                    previousConfig.resolutionWidth;
                profileState.config.resolutionHeight =
                    previousConfig.resolutionHeight;
                optionsDraftConfig.resolutionWidth =
                    previousConfig.resolutionWidth;
                optionsDraftConfig.resolutionHeight =
                    previousConfig.resolutionHeight;
            }
        }
        raceRenderer.resetCamera();
        device->configureQuality(
            profileState.config.quality.filtering,
            profileState.config.quality.msaa);
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
        if (profileState.config.language != previousConfig.language)
        {
            showInfoDialog(
                localized("svWarning"),
                localized("svHintNeedReload"),
                localized("svOk"),
                menu::virtualWidth * 0.5F,
                menu::virtualHeight * 0.5F);
            optionsReloadDialogPending = true;
        }
        else
        {
            backMenu();
        }
    };
    auto adjustCurrentOption = [&](int direction) {
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
        if (!sourceOptionsMenu.adjust(
                menuStack.back(), menuSelection, direction))
            return;
#ifdef RRR3D_AUDIO
        if (menuStack.back() == MenuScreen::SoundOptions)
        {
            if (menuSelection == 2U)
                audio.setBusVolume(
                    r3d::audio::Bus::Music,
                    optionsDraftConfig.musicVolume);
            else if (menuSelection == 3U)
                audio.setBusVolume(
                    r3d::audio::Bus::Effects,
                    optionsDraftConfig.effectsVolume);
            else if (menuSelection == 4U)
                audio.setBusVolume(
                    r3d::audio::Bus::Voice,
                    optionsDraftConfig.voiceVolume);
        }
#endif
        refreshCurrentOptionsPage();
    };
    showFinishMenu = [&](bool persistProgress) {
        if (sourceFinishFrame.shown() || raceSession.racers().empty())
            return;
        if (!options->finishMenuSmokeTest)
        {
            static_cast<void>(gameModeState.ExitRace(persistProgress));
            static_cast<void>(gameModeState.TakeCommands());
        }
        // Menu::OnProcessEvent(cRaceFinishTimeEnd) calls ExitRace before
        // ExitRaceGoFinish. This also executes on the network result path;
        // CompleteRace/ExitRace are deliberately idempotent there.
        raceSession.completeRaceForExit(raceVehicles);
        // --finish-menu-smoke-test is a renderer fixture, not a started
        // source race. Real host/client and offline finishes all execute the
        // Race::ExitRace state transition even when only the host persists.
        if (!options->finishMenuSmokeTest)
            applyOriginalRaceExitLifecycle();
        if (persistProgress)
            saveRaceProfile();
        else if (!options->finishMenuSmokeTest)
        {
            // NetRace::OnExitRace calls GameMode::ExitRace(false, results):
            // the received championship profile advances in memory and all
            // active Player points reset at a pass boundary, but the client
            // deliberately does not save that transient host-owned state.
            applyRaceProfileState();
        }
#ifdef RRR3D_AUDIO
        // GameMode::ExitRace always stops the race commentator/audio before
        // Menu::ExitRaceGoFinish, on both the host and a receiving client.
        // The Commentator object remains alive for cPlayerFinish* events.
        stopRaceAudio(false);
        commentator.pause(false);
        finishMenuAudioHeldObserved =
            finishMenuAudioHeldObserved ||
            (music.paused() && !music.currentVoiceActive() &&
             !gameMusic.currentVoiceActive());
#endif
        std::vector<r3d::game::originalracemenu::FinishEntry>
            finishEntries;
        finishEntries.reserve(raceSession.results().size());
        for (const auto& result : raceSession.results())
        {
            finishEntries.push_back(
                {result.playerId, result.playerId,
                 result.voiceNameDuration});
        }
        sourceFinishFrame.show(std::move(finishEntries));
        finishLastEventObserved = false;
        clearFinishRows();
        try
        {
            finishRows.reserve(sourceFinishFrame.playerCount());
            for (std::size_t index = 0U;
                 index < sourceFinishFrame.playerCount(); ++index)
            {
                const auto racer =
                    sourceFinishFrame.results()[index].racer;
                if (racer >= raceSession.racers().size() ||
                    racer >= originalRace->racers.size())
                    continue;
                const auto& result = raceSession.racers()[racer];
                const auto& sourceResult = raceSession.results()[index];
                const auto& definition = originalRace->racers[racer];
                finishRows.emplace_back();
                auto& row = finishRows.back();
                row.racer = racer;
                const auto* identity =
                    r3d::game::originalrace::findOriginalPlayerIdentity(
                        *originalRace, result.GetGamerId());
                const std::string displayName =
                    result.GetNetName().empty()
                        ? localized(identity != nullptr
                                        ? identity->name
                                        : result.GetName())
                        : result.GetNetName();
                row.name = createText(
                    *device, displayName,
                    menu::headerFontHeight, false,
                    menu::Rgba8{233U, 167U, 63U, 255U},
                    resolvedFont);
                std::string rewardMoney =
                    std::to_string(sourceResult.money);
                const auto pickedMoney = sourceResult.pickedMoney;
                if (pickedMoney > 0U)
                {
                    rewardMoney +=
                        " + " + std::to_string(pickedMoney);
                }
                row.rewardValue = createText(
                    *device,
                    rewardMoney + "\n" +
                        std::to_string(sourceResult.points),
                    menu::headerFontHeight, false,
                    menu::Rgba8{132U, 188U, 67U, 255U},
                    resolvedFont);
                const std::string& photoPath =
                    identity != nullptr && !identity->photoPath.empty()
                        ? identity->photoPath
                        : definition.photoPath;
                if (!photoPath.empty())
                {
                    const auto photo = menu::loadOriginalImage(
                        *resources, photoPath);
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
        const auto humanRacer = raceSession.humanRacer();
        const auto& player = raceSession.racers().at(humanRacer);
        inRace = false;
        raceInput = {};
        raceUseWeaponRequested = false;
        raceUseAllWeaponsRequested = false;
        raceUseMineRequested = false;
        raceMineAnalogBinding = false;
        raceUseHyper = false;
        raceChangeWeaponRequested = false;
        raceWeaponChangeDirection = 1;
        raceFireWeaponSlotRequested = -1;
        raceResetRequested = false;
        menuStack = {MenuScreen::Main, MenuScreen::Finish};
        menuSelection = 0;
        gameModeState.ExitRaceGoFinish();
        static_cast<void>(gameModeState.TakeCommands());
        std::cout << "Original FinishMenu: place "
                  << player.GetPlace() << ", money +"
                  << player.rewardMoney + player.GetPickMoney()
                  << ", points +" << player.rewardPoints << '\n';
    };
    auto closeFinishMenu = [&]() {
        if (!sourceFinishFrame.shown())
            return;
        gameModeState.OnFinishFrameClose();
        static_cast<void>(gameModeState.TakeCommands());
        sourceFinishFrame.hide();
#ifdef RRR3D_AUDIO
        // GameMode::OnFinishFrameClose: discard any remaining place call,
        // start menu music from the MusicCat's stored position at zero source
        // gain, then let OnFrame approach full gain over one second.
        commentator.stop();
        sourceMenuMusicGain = 0.0F;
        audio.setBusVolume(r3d::audio::Bus::Music, 0.0F);
        if (!music.pause(false, audioError))
        {
            std::cerr
                << "Original OnFinishFrameClose menu music failed: "
                << audioError << '\n';
        }
        finishMenuAudioCloseObserved =
            finishMenuAudioCloseObserved ||
            (!commentator.speaking() && !music.paused() &&
             std::abs(audio.busVolume(r3d::audio::Bus::Music)) <
                 0.0001F);
#endif
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
        sourceRaceMenu.setState(
            r3d::game::originalracemenu::State::Main);
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
            sourceRaceMenu.setState(
                r3d::game::originalracemenu::State::Angar);
            sourceAngarFrame.show(
                sourceAngarPlanets(), racePlanetChampion,
                championshipMode, sourceAngarNetworkClient(),
                profileState.player.currentPlanet);
            menuSelection = sourceAngarFrame.focus();
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
            racer.SetFinished(true);
            racer.SetPlace(static_cast<std::uint32_t>(index + 1U));
            const auto reward = std::min(
                index, originalRace->rewardMoney.size() - 1U);
            racer.rewardMoney = originalRace->rewardMoney[reward];
            racer.rewardPoints = originalRace->rewardPoints[reward];
            if (index == 0U)
                racer.TakeMoney(25.0F);
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
        const auto& sourceDialog = sourceDialogs.Music();
        if (!sourceDialog.visible)
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

        const float width = sourceDialog.frameSize.x;
        const float height = sourceDialog.frameSize.y;
        const float centerX = sourceDialog.center.x;
        const float centerY = sourceDialog.center.y;
        // DialogMenu2's z values express widget order, not camera-space
        // depth.  Map that order into the established overlay depth band;
        // literal z=3/2 is clipped by the Metal orthographic projection.
        drawQuad(
            *device, quad, shader, musicDialogFrame, width, height,
            centerX, centerY, 60.0F, transparent);
        drawQuad(
            *device, quad, shader, visual->title.texture,
            visual->title.width, visual->title.height,
            centerX + sourceDialog.titleOffset.x +
                visual->title.width * 0.5F,
            centerY + sourceDialog.titleOffset.y, 59.0F, transparent);
        drawQuad(
            *device, quad, shader, visual->info.texture,
            visual->info.width, visual->info.height,
            centerX + sourceDialog.infoOffset.x +
                visual->info.width * 0.5F,
            centerY + sourceDialog.infoOffset.y, 59.0F, transparent);
        if (sourceDialog.offset > 0.01F)
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
    auto drawOriginalCursor = [&](bool visible) {
        if (!visible)
            return;
        float pointerX = 0.0F;
        float pointerY = 0.0F;
        SDL_GetMouseState(&pointerX, &pointerY);
        int windowWidth = 0;
        int windowHeight = 0;
        if (!SDL_GetWindowSize(window, &windowWidth, &windowHeight) ||
            windowWidth <= 0 || windowHeight <= 0)
        {
            return;
        }
        const float virtualX = pointerX * menu::virtualWidth /
            static_cast<float>(windowWidth);
        const float virtualY = pointerY * menu::virtualHeight /
            static_cast<float>(windowHeight);
        drawQuad(
            *device, quad, shader, cursor,
            static_cast<float>(model->cursorImage.width),
            static_cast<float>(model->cursorImage.height),
            virtualX + static_cast<float>(model->cursorImage.width) * 0.5F,
            virtualY + static_cast<float>(model->cursorImage.height) * 0.5F,
            1.0F, transparent);
    };
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
                    !gameModeState.IsRaceLoading())
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
                if (!networkSession.beginLanSearch(
                        error, options->legacyWindowsDebug))
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
                    sourceDialogs.Info().visible && networkMatchStarted &&
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
                    sourceDialogs.Info().visible &&
                    sourceDialogs.Info().dismissable;
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
        if (options->raceRenderSmokeTest &&
            sourceDialogs.Info().visible &&
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
             !raceInfoDialogObserved || sourceDialogs.Info().visible);
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
            if (!sourceWorkshopFrame.goods().empty())
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
#ifdef RRR3D_AUDIO
        if (options->raceRenderSmokeTest && inRace &&
            racePauseSmokeStep == 3U &&
            raceChatSmokeStep == 4U &&
            !raceLoopTeardownObserved)
        {
            // Exercise the same authoritative loop teardown used by
            // leaveCurrentRace, then restore SoundMotor for the remaining
            // physics/render fixture.
            stopAllRaceLoops();
            for (std::size_t racer = 0U; racer < engineAudio.size();
                 ++racer)
            {
                startRacerMotorAudio(racer);
            }
        }
#endif
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
            (!gameModeState.IsRaceLoading() && !menuStack.empty() &&
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
        originalmenu::MenuState sourceMenuState =
            originalmenu::MenuState::Main;
        const MenuScreen sourceScreen = menuStack.back();
        if (sourceScreen == MenuScreen::Credits)
            sourceMenuState = originalmenu::MenuState::Final;
#ifdef RRR3D_PHYSICS
        if (inRace)
        {
            sourceMenuState = originalmenu::MenuState::Hud;
        }
        else if (sourceScreen == MenuScreen::Finish)
        {
            sourceMenuState = originalmenu::MenuState::Finish;
        }
        else if (sourceScreen == MenuScreen::RaceMenu ||
                 sourceScreen == MenuScreen::Gamers ||
                 sourceScreen == MenuScreen::Garage ||
                 sourceScreen == MenuScreen::Workshop ||
                 sourceScreen == MenuScreen::Planets ||
                 sourceScreen == MenuScreen::Achievements ||
                 sourceScreen == MenuScreen::GameOptions ||
                 sourceScreen == MenuScreen::GraphicsOptions ||
                 sourceScreen == MenuScreen::SoundOptions ||
                 sourceScreen == MenuScreen::ControlsOptions)
        {
            sourceMenuState = originalmenu::MenuState::Race;
        }
#endif
        sourceMenuSystem.SetState(sourceMenuState);
        const bool sourceOptionsVisible =
            sourceScreen == MenuScreen::Options
#ifdef RRR3D_PHYSICS
            || sourceScreen == MenuScreen::GameOptions ||
            sourceScreen == MenuScreen::GraphicsOptions ||
            sourceScreen == MenuScreen::SoundOptions ||
            sourceScreen == MenuScreen::ControlsOptions
#endif
            ;
        sourceMenuSystem.SetOptionsVisible(sourceOptionsVisible);
#ifdef RRR3D_PHYSICS
        sourceDialogs.SetAcceptVisible(acceptDialogVisible());
        sourceMenuSystem.ShowModal(
            originalmenu::FrameId::Message,
            sourceDialogs.Info().visible);
        sourceMenuSystem.ShowModal(
            originalmenu::FrameId::Loading,
            sourceDialogs.Info().visible &&
                !sourceDialogs.Info().dismissable,
            originalmenu::MenuSystem::topmostLoading);
        if (userChat.inputVisible())
        {
            sourceMenuSystem.ShowModal(
                originalmenu::FrameId::UserChat, true);
        }
        else
        {
            sourceMenuSystem.ShowModal(
                originalmenu::FrameId::UserChat, false);
            sourceMenuSystem.Show(
                originalmenu::FrameId::UserChat, userChat.visible());
        }
#endif
#ifdef RRR3D_GAMEPAD_INPUT
        if (sourceMenuSystem.ConsumeInputReset())
            input.resetInput();
#else
        static_cast<void>(sourceMenuSystem.ConsumeInputReset());
#endif
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
#ifdef RRR3D_PHYSICS
            // Domain callbacks may close a dialog while more SDL events are
            // already queued. Keep the source modal root authoritative for
            // every event, not merely at the beginning of the frame.
            sourceDialogs.SetAcceptVisible(acceptDialogVisible());
            sourceMenuSystem.ShowModal(
                originalmenu::FrameId::Message,
                sourceDialogs.Info().visible);
            sourceMenuSystem.ShowModal(
                originalmenu::FrameId::Loading,
                sourceDialogs.Info().visible &&
                    !sourceDialogs.Info().dismissable,
                originalmenu::MenuSystem::topmostLoading);
            if (options->legacyWindowsDebug && inRace &&
                event.type == SDL_EVENT_MOUSE_MOTION &&
                (raceCameraStyle ==
                     rrr3d::race::RaceCameraStyle::Lights ||
                 raceCameraStyle ==
                     rrr3d::race::RaceCameraStyle::FreeView) &&
                (event.motion.state & SDL_BUTTON_RMASK) != 0U)
            {
                raceRenderer.rotateDebugCamera(
                    event.motion.xrel, event.motion.yrel);
            }
#endif
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
                event.type != SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED &&
                event.type != SDL_EVENT_WINDOW_METAL_VIEW_RESIZED)
            {
                const auto movieInputEvents =
                    input.processEvent(event);
                for (const auto& inputEvent : movieInputEvents)
                {
                    if (originalMovieInputSuppressionFrames == 0U &&
                        inputEvent.active && !inputEvent.repeated &&
                        inputEvent.action ==
                            rrr3d::input::Action::Pause)
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
                event.type != SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED &&
                event.type != SDL_EVENT_WINDOW_METAL_VIEW_RESIZED)
            {
                bool skipIntro = false;
#ifdef RRR3D_GAMEPAD_INPUT
                const auto startupInputEvents =
                    input.processEvent(event);
                skipIntro = std::any_of(
                    startupInputEvents.begin(),
                    startupInputEvents.end(),
                    [](const rrr3d::input::ActionEvent& inputEvent) {
                        return inputEvent.active &&
                               !inputEvent.repeated &&
                               inputEvent.action ==
                                   rrr3d::input::Action::Pause;
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
                    const auto chatInputEvents =
                        input.processEvent(event);
                    const bool pauseRequested = std::any_of(
                        chatInputEvents.begin(),
                        chatInputEvents.end(),
                        [](const rrr3d::input::ActionEvent& inputEvent) {
                            return inputEvent.active &&
                                   !inputEvent.repeated &&
                                   inputEvent.action ==
                                       rrr3d::input::Action::Pause;
                        });
                    if (pauseRequested)
                        openExitRaceDialog();
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
            if (gameModeState.IsRaceLoading() &&
                event.type != SDL_EVENT_QUIT &&
                event.type != SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.type != SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED &&
                event.type != SDL_EVENT_WINDOW_METAL_VIEW_RESIZED)
            {
                // Menu::msInfo is modal, hides the cursor, and does not
                // dispatch menu/gameplay actions while the world is loaded.
                input.processEvent(event);
                continue;
            }
            if (sourceStartOptionsActive &&
                event.type != SDL_EVENT_QUIT &&
                event.type != SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.type != SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED &&
                event.type != SDL_EVENT_WINDOW_METAL_VIEW_RESIZED)
            {
                if (startOptionsReloadDialogPending &&
                    sourceDialogs.Info().visible)
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
                                sourceStartOptionsMenu.setFocus(row);
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
                            sourceStartOptionsMenu.setFocus(
                                r3d::game::originaloptions::
                                    StartOptionsMenuState::applyRow);
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
                        sourceStartOptionsMenu.moveFocus(-1);
                    }
                    else if (inputEvent.action ==
                             rrr3d::input::Action::MenuDown)
                    {
                        sourceStartOptionsMenu.moveFocus(1);
                    }
                    else if (
                        sourceStartOptionsMenu.focus() <
                            r3d::game::originaloptions::
                                StartOptionsMenuState::optionRows &&
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
                        sourceStartOptionsMenu.focus() ==
                            r3d::game::originaloptions::
                                StartOptionsMenuState::applyRow)
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
                            rrr3d::input::originalKeyboardBindingName(
                                event.key.scancode);
                    }
                }
                else if (bindingCaptureGamepad &&
                         event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
                {
                    consumedCaptureEvent = true;
                    bindingName =
                        rrr3d::input::originalGamepadButtonBindingName(
                        static_cast<SDL_GamepadButton>(
                            event.gbutton.button));
                }
                else if (bindingCaptureGamepad &&
                         event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION)
                {
                    bindingName =
                        rrr3d::input::originalGamepadAxisBindingName(
                        static_cast<SDL_GamepadAxis>(event.gaxis.axis),
                        event.gaxis.value);
                    consumedCaptureEvent = bindingName.has_value();
                }
                if (bindingName && !bindingName->empty())
                {
                    sourceOptionsMenu.setControlBinding(
                        *bindingCaptureAction, bindingCaptureGamepad,
                        *bindingName);
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
            const auto sourceTopModal = sourceMenuSystem.TopModal();
            const bool sourceInfoModal =
                sourceTopModal == originalmenu::FrameId::Message ||
                sourceTopModal == originalmenu::FrameId::Loading;
            const bool sourceAcceptModal =
                sourceTopModal == originalmenu::FrameId::Accept;
            if (!inRace && !sourceMenuSystem.HasModal())
            {
                if (menuStack.back() == MenuScreen::Garage)
                    handleSourceAutoObserverPointer(
                        garageObserver, event);
                else if (menuStack.back() == MenuScreen::Planets)
                    handleSourceAutoObserverPointer(
                        angarObserver, event);
            }
            std::optional<bool> pointerAcceptChoice;
            if (sourceInfoModal &&
                sourceDialogs.Info().dismissable &&
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
                            virtualX - sourceDialogs.Info().center.x) <=
                            static_cast<float>(
                                infoDialogButtonSelectedImage.width) *
                                0.5F &&
                        std::abs(
                            virtualY -
                            (sourceDialogs.Info().center.y +
                             sourceDialogs.Info().okOffset.y)) <=
                            static_cast<float>(
                                infoDialogButtonSelectedImage.height) *
                                0.5F;
                }
                pointerTargetsItem =
                    hoveredOk ||
                    event.type == SDL_EVENT_MOUSE_MOTION ||
                    event.button.button != SDL_BUTTON_LEFT;
            }
            else if (sourceAcceptModal &&
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
                    const auto& sourceAccept =
                        sourceDialogs.Accept();
                    const auto& acceptLayout =
                        sourceAccept.layout;
                    const float buttonY =
                        sourceAccept.center.y +
                        acceptLayout.yesOffset.y;
                    const float yesX =
                        sourceAccept.center.x +
                        acceptLayout.yesOffset.x;
                    const float noX =
                        sourceAccept.center.x +
                        acceptLayout.noOffset.x;
                    if (std::abs(virtualY - buttonY) <=
                            acceptLayout.buttonSize.y * 0.5F &&
                        std::abs(virtualX - yesX) <=
                            acceptLayout.buttonSize.x * 0.5F)
                    {
                        pointerAcceptChoice = true;
                    }
                    else if (
                        std::abs(virtualY - buttonY) <=
                            acceptLayout.buttonSize.y * 0.5F &&
                        std::abs(virtualX - noX) <=
                            acceptLayout.buttonSize.x * 0.5F)
                    {
                        pointerAcceptChoice = false;
                    }
                }
                sourceDialogs.SetAcceptHover(pointerAcceptChoice);
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
                    syncSourceGamers();
                    const auto gamersLayout =
                        sourceGamersFrame.layout(
                            menu::virtualWidth, menu::virtualHeight,
                            254.0F,
                            static_cast<float>(
                                garageArrowSelectedImage.width),
                            static_cast<float>(
                                gamersNextArrowSelectedImage.width));
                    if (sourceGamersFrame.previous() &&
                        std::abs(
                            virtualX - gamersLayout.leftX) <= 62.0F &&
                        std::abs(
                            virtualY - gamersLayout.planetY) <= 82.0F)
                    {
                        sourceGamersFrame.setFocus(
                            r3d::game::originalracemenu::
                                GamerFocus::Left);
                        hoveredGamerControl = true;
                    }
                    else if (
                        sourceGamersFrame.next() &&
                        std::abs(
                            virtualX - gamersLayout.rightX) <= 62.0F &&
                        std::abs(
                            virtualY - gamersLayout.planetY) <= 82.0F)
                    {
                        sourceGamersFrame.setFocus(
                            r3d::game::originalracemenu::
                                GamerFocus::Right);
                        hoveredGamerControl = true;
                    }
                    else if (
                        std::abs(
                            virtualX - gamersLayout.nextX) <= 85.0F &&
                        std::abs(
                            virtualY - gamersLayout.nextY) <= 75.0F)
                    {
                        sourceGamersFrame.setFocus(
                            r3d::game::originalracemenu::
                                GamerFocus::Next);
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
                    const auto visibleEnd =
                        sourceProfileFrame.visibleEnd();
                    for (std::size_t index =
                             sourceProfileFrame.visibleBegin();
                         index < visibleEnd; ++index)
                    {
                        const float rowY = sourceProfileFrame.rowY(
                            menu::virtualHeight, index);
                        const float closeX =
                            centerX +
                            static_cast<float>(
                                model->selectionImage.width) *
                                0.5F -
                            40.0F;
                        if (std::abs(virtualX - closeX) <= 22.0F &&
                            std::abs(virtualY - rowY) <= 22.0F)
                        {
                            sourceProfileFrame.focusClose(index);
                            hoveredProfileControl = true;
                            break;
                        }
                        if (std::abs(virtualX - centerX) <=
                                static_cast<float>(
                                    model->selectionImage.width) *
                                    0.5F &&
                            std::abs(virtualY - rowY) <= 24.0F)
                        {
                            sourceProfileFrame.focusItem(index);
                            hoveredProfileControl = true;
                            break;
                        }
                    }
                    const float backY = sourceProfileFrame.backY(
                        menu::virtualHeight);
                    if (!hoveredProfileControl &&
                        std::abs(virtualX - centerX) <=
                            static_cast<float>(
                                model->selectionImage.width) *
                                0.5F &&
                        std::abs(virtualY - backY) <= 24.0F)
                    {
                        sourceProfileFrame.focusBack();
                        hoveredProfileControl = true;
                    }
                    if (!hoveredProfileControl &&
                        std::abs(virtualX - centerX) <= 26.0F &&
                        std::abs(
                            virtualY -
                            sourceProfileFrame.upArrowY(
                                menu::virtualHeight)) <= 22.0F &&
                        sourceProfileFrame.canScrollUp())
                    {
                        sourceProfileFrame.focusUp();
                        hoveredProfileControl = true;
                    }
                    if (!hoveredProfileControl &&
                        std::abs(virtualX - centerX) <= 26.0F &&
                        std::abs(
                            virtualY -
                            sourceProfileFrame.downArrowY(
                                menu::virtualHeight)) <= 22.0F &&
                        sourceProfileFrame.canScrollDown())
                    {
                        sourceProfileFrame.focusDown();
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
                sourceGarageFrame.updateColors(sourceGarageColors());
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
                                        !sourceGarageFrame.colorAvailable(
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
                    if (sourceGarageFrame.setFocus(
                            *hoveredGarageItem))
                        menuSelection = sourceGarageFrame.focus();
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
                            const auto* good =
                                sourceWorkshopFrame.visibleGood(index);
                            if (!sourceWorkshopFrame.drag().active() &&
                                good != nullptr &&
                                good->catalogIndex <
                                    originalGarage->workshop.size())
                            {
                                const auto& item =
                                    originalGarage->workshop[
                                        good->catalogIndex];
                                showWorkshopWeaponDialog(
                                    item, item.cost,
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
                                !sourceWorkshopFrame.drag().active())
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
                        const auto workshopLayout =
                            sourceWorkshopLayout();
                        if (std::abs(
                                virtualX -
                                workshopLayout.arrowX) <= 35.0F &&
                            std::abs(
                                virtualY -
                                workshopLayout.upArrowY) <= 35.0F &&
                            sourceWorkshopFrame.scroll() > 0U)
                        {
                            sourceWorkshopFrame.scrollGoods(-1);
                            refreshWorkshopPage();
                            pointerHandledOriginalOptions = true;
                        }
                        else if (
                            std::abs(
                                virtualX -
                                workshopLayout.arrowX) <= 35.0F &&
                            std::abs(
                                virtualY -
                                workshopLayout.downArrowY) <= 35.0F &&
                            sourceWorkshopFrame.scroll() <
                                sourceWorkshopFrame.maximumScroll())
                        {
                            sourceWorkshopFrame.scrollGoods(1);
                            refreshWorkshopPage();
                            pointerHandledOriginalOptions = true;
                        }
                    }
                }
                if (!workshopDialogShown)
                    hideWorkshopWeaponDialog();
                if (hoveredWorkshopItem)
                {
                    sourceWorkshopFrame.updateSlots(
                        sourceWorkshopSlots());
                    if (sourceWorkshopFrame.setPointerFocus(
                            *hoveredWorkshopItem))
                        menuSelection = sourceWorkshopFrame.focus();
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
                        const auto angarLayout =
                            sourceAngarFrame.layout(
                                menu::virtualWidth,
                                menu::virtualHeight,
                                static_cast<float>(
                                    angarBottomPanelImage.width),
                                static_cast<float>(
                                    angarBottomPanelImage.height),
                                static_cast<float>(
                                    angarPlanetInfoImage.width),
                                static_cast<float>(
                                    angarPlanetInfoImage.height),
                                static_cast<float>(
                                    garageBackImage.width));
                        for (std::size_t index = 0U;
                             index < planetCount; ++index)
                        {
                            const float centerX =
                                angarLayout.planetX(index);
                            if (std::abs(virtualX - centerX) <=
                                    90.0F &&
                                std::abs(
                                    virtualY - angarLayout.planetY) <=
                                    90.0F)
                            {
                                hoveredAngarItem = index;
                                break;
                            }
                        }
                        if (!hoveredAngarItem &&
                            std::abs(
                                virtualX - angarLayout.backX) <=
                                static_cast<float>(
                                    garageBackImage.width) *
                                    0.5F &&
                            std::abs(
                                virtualY - angarLayout.backY) <= 40.0F)
                        {
                            hoveredAngarItem = planetCount;
                        }
                        if (event.type ==
                                SDL_EVENT_MOUSE_BUTTON_DOWN &&
                            event.button.button == SDL_BUTTON_LEFT &&
                            sourceAngarFrame.selection() >= 0)
                        {
                            if (std::abs(
                                    virtualX - angarLayout.closeX) <=
                                    16.0F &&
                                std::abs(
                                    virtualY - angarLayout.closeY) <=
                                    16.0F)
                            {
                                sourceAngarFrame.closeInfo();
                                menuSelection =
                                    sourceAngarFrame.focus();
                                refreshPlanetsPage();
                                pointerHandledOriginalOptions = true;
                            }
                        }
                }
                if (hoveredAngarItem &&
                    !pointerHandledOriginalOptions)
                {
                    const int previousSelection =
                        sourceAngarFrame.selection();
                    if (sourceAngarFrame.setPointerFocus(
                            *hoveredAngarItem))
                    {
                        menuSelection = sourceAngarFrame.focus();
                        if (sourceAngarFrame.selection() !=
                            previousSelection)
                        {
#ifdef RRR3D_AUDIO
                            if (sourceAngarFrame.selection() >= 0)
                            {
                                playOriginalMenuSound(
                                    rrr3d::audio::OriginalMenuSound::
                                        ShowPlanet);
                            }
#endif
                            refreshPlanetsPage();
                        }
                    }
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
                        const auto achievementLayout =
                            sourceAchievementFrame.layout(
                                menu::virtualWidth,
                                menu::virtualHeight,
                                static_cast<float>(
                                    garageBackImage.width),
                                static_cast<float>(
                                    garageBackImage.height));
                        for (std::size_t reverse = 0U;
                             reverse <
                             originalAchievementVisuals.size();
                             ++reverse)
                        {
                            const std::size_t index =
                                originalAchievementVisuals.size() -
                                1U - reverse;
                            const auto& image =
                                achievementState(index) ==
                                        r3d::game::originalracemenu::
                                            AchievementState::Opened
                                    ? achievementOpenedImages[index]
                                    : achievementLockedImages[index];
                            const float centerX =
                                achievementLayout.cardX(index);
                            const float centerY =
                                achievementLayout.cardY(index);
                            if (std::abs(virtualX - centerX) <=
                                    static_cast<float>(image.width) *
                                        achievementLayout.scale * 0.5F &&
                                std::abs(virtualY - centerY) <=
                                    static_cast<float>(image.height) *
                                        achievementLayout.scale * 0.5F)
                            {
                                hoveredAchievement = index;
                                break;
                            }
                        }
                        if (!hoveredAchievement &&
                            std::abs(
                                virtualX - achievementLayout.backX) <=
                                static_cast<float>(
                                    garageBackImage.width) *
                                    0.5F &&
                            std::abs(
                                virtualY - achievementLayout.backY) <=
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
                    if (sourceAchievementFrame.setPointerFocus(
                            *hoveredAchievement))
                        menuSelection = sourceAchievementFrame.focus();
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
                    for (std::size_t index = 0U;
                         !pointerHandledNetworkKick &&
                         index < raceMenuIcons.size(); ++index)
                    {
                        const float itemX =
                            sourceRaceMainFrame.itemX(
                                menu::virtualWidth, index);
                        const float itemY =
                            sourceRaceMainFrame.itemY(
                                menu::virtualHeight,
                                static_cast<float>(
                                    raceBottomPanelImage.height));
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
                            sourceOptionsMenu.stateButtonY(
                                menu::virtualHeight, state);
                        if (virtualX >= centerX - 600.0F &&
                            virtualX <= centerX - 250.0F &&
                            std::abs(virtualY - stateY) <= 45.0F)
                        {
                            hoveredState = state;
                            break;
                        }
                    }

                    const auto optionsTab =
                        r3d::game::originaloptions::OptionsMenuState::
                            tabForScreen(menuStack.back());
                    const std::size_t rowCount =
                        sourceOptionsMenu.rowCount(optionsTab);
                    const std::size_t visibleRows =
                        sourceOptionsMenu.visibleRowCount(optionsTab);
                    sourceOptionsMenu.ensureVisible(
                        optionsTab, menuSelection);
                    const std::size_t firstVisible =
                        sourceOptionsMenu.scroll(optionsTab);
                    const float firstRowY =
                        sourceOptionsMenu.firstRowY(
                            optionsTab, menu::virtualHeight);
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
                                    sourceOptionsMenu.setControlsUseGamepad(
                                        virtualX >= centerX + 345.0F);
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
                        const float arrowHalfWidth =
                            static_cast<float>(std::max(
                                optionsArrowImage.width,
                                optionsArrowSelectedImage.width)) *
                                0.5F +
                            8.0F;
                        const bool editableValueRow =
                            hoveredOption && *hoveredOption < rowCount &&
                            menuStack.back() !=
                                MenuScreen::ControlsOptions &&
                            *hoveredOption < activeMenuPage().enabled.size() &&
                            activeMenuPage().enabled[*hoveredOption];
                        const int arrowDirection =
                            editableValueRow
                                ? originalOptionsArrowDirection(
                                      virtualX, centerX,
                                      arrowHalfWidth)
                                : 0;
                        const float upY =
                            sourceOptionsMenu.upArrowY(
                                optionsTab, menu::virtualHeight);
                        const float downY =
                            sourceOptionsMenu.downArrowY(
                                menu::virtualHeight);
                        if (arrowDirection != 0)
                        {
                            menuSelection = *hoveredOption;
                            adjustCurrentOption(arrowDirection);
                            pointerHandledOriginalOptions = true;
                        }
                        else if (std::abs(
                                virtualX -
                                (centerX + 150.0F)) <= 35.0F &&
                            std::abs(virtualY - upY) <= 35.0F &&
                            sourceOptionsMenu.canScrollUp(optionsTab))
                        {
                            sourceOptionsMenu.scrollGrid(optionsTab, -1);
                            pointerHandledOriginalOptions = true;
                        }
                        else if (
                            std::abs(
                                virtualX -
                                (centerX + 150.0F)) <= 35.0F &&
                            std::abs(virtualY - downY) <= 35.0F &&
                            sourceOptionsMenu.canScrollDown(optionsTab))
                        {
                            sourceOptionsMenu.scrollGrid(optionsTab, 1);
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
                {
                    menuSelection = *hoveredOption;
                    sourceOptionsMenu.ensureVisible(
                        r3d::game::originaloptions::OptionsMenuState::
                            tabForScreen(menuStack.back()),
                        menuSelection);
                }
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
                    const auto finalLayout = sourceFinalFrame.layout(
                        menu::virtualWidth, menu::virtualHeight,
                        finalCreditsHeight,
                        static_cast<float>(finalBackImage.width));
                    hoveredBack =
                        std::abs(virtualX - finalLayout.backX) <=
                            static_cast<float>(finalBackImage.width) * 0.5F &&
                        std::abs(virtualY - finalLayout.backY) <=
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
                    sourceMainMenuFrame.owns(menuStack.back()) &&
                        menuStack.back() != MenuScreen::Main);
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
                    sourceMainMenuFrame.owns(menuStack.back()) &&
                        menuStack.back() != MenuScreen::Main);
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
                    sourceDialogs.Accept().disableFocus &&
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
                        sourceOptionsMenu.setControlBinding(
                            *bindingCaptureAction,
                            bindingCaptureGamepad, "None");
                    }
                    bindingCaptureAction.reset();
                    refreshCurrentOptionsPage();
                    continue;
                }
                if (sourceAcceptModal && inputEvent.active &&
                    !inputEvent.repeated &&
                    inputEvent.action ==
                        rrr3d::input::Action::MenuConfirm &&
                    (inputEvent.source !=
                         rrr3d::input::Source::Mouse ||
                     pointerTargetsItem))
                {
                    sourceDialogs.ChooseAccept(
                        pointerAcceptChoice.value_or(
                            acceptDialogYesFocused()));
                }
                if (sourceInfoModal)
                {
                    if (!inputEvent.active || inputEvent.repeated)
                        continue;
                    if (!sourceDialogs.Info().dismissable)
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
                        const bool closeOptionsAfterReload =
                            optionsReloadDialogPending;
                        optionsReloadDialogPending = false;
                        hideInfoDialog();
                        if (closeOptionsAfterReload)
                            backMenu();
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
                    case rrr3d::input::Action::Debug1:
                    case rrr3d::input::Action::Debug2:
                    case rrr3d::input::Action::Debug3:
                    case rrr3d::input::Action::Debug4:
                    case rrr3d::input::Action::Debug5:
                    case rrr3d::input::Action::Debug6:
                    case rrr3d::input::Action::Debug7:
                    case rrr3d::input::Action::DebugOverlay:
                    case rrr3d::input::Action::DebugPagePrevious:
                    case rrr3d::input::Action::DebugPageNext: {
                        const auto command = gameDebug.handle(
                            inputEvent.action, inputEvent.active,
                            inputEvent.repeated);
                        if (!gameDebug.enabled())
                            break;
                        if (command ==
                            rrr3d::debug::Command::ResetVehicles)
                        {
                            const std::size_t count = std::min(
                                physicsWorld->vehicleCount(),
                                physicsDescription->spawns.size());
                            for (std::size_t index = 0U; index < count;
                                 ++index)
                            {
                                physicsWorld->resetVehicle(
                                    index,
                                    physicsDescription->spawns[index]
                                        .position,
                                    physicsDescription->spawns[index]
                                        .direction);
                                if (index < raceVehicles.size())
                                    raceVehicles[index] =
                                        physicsWorld->vehicle(index);
                            }
                            raceRenderer.resetCamera();
                        }
                        else if (
                            command ==
                            rrr3d::debug::Command::ToggleFullscreen)
                        {
                            const bool fullscreen =
                                (SDL_GetWindowFlags(window) &
                                 SDL_WINDOW_FULLSCREEN) != 0U;
                            std::string windowError;
                            if (!applyOriginalWindowMode(
                                    window, sourceDisplayModes,
                                    profileState.config.resolutionWidth,
                                    profileState.config.resolutionHeight,
                                    !fullscreen, true, windowError) ||
                                !requestSynchronizedWindowDrawable())
                            {
                                std::cerr
                                    << "Game debug F3 display toggle failed: "
                                    << (windowError.empty()
                                            ? SDL_GetError()
                                            : windowError)
                                    << '\n';
                            }
                        }
                        raceSession.setDebugHumanAiControl(
                            gameDebug.humanAiControl());
                        break;
                    }
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
                        if (inputEvent.source ==
                            rrr3d::input::Source::GamepadAxis)
                        {
                            raceMineAnalogBinding = true;
                        }
                        else if (inputEvent.active &&
                                 !inputEvent.repeated)
                        {
                            raceUseMineRequested = true;
                        }
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
                            if (options->legacyWindowsDebug)
                            {
                                using Style =
                                    rrr3d::race::RaceCameraStyle;
                                switch (raceCameraStyle)
                                {
                                case Style::ThirdPerson:
                                    raceCameraStyle = Style::Isometric;
                                    break;
                                case Style::Isometric:
                                    raceCameraStyle = Style::Lights;
                                    break;
                                case Style::Lights:
                                    raceCameraStyle = Style::IsometricView;
                                    break;
                                case Style::IsometricView:
                                    raceCameraStyle = Style::FreeView;
                                    break;
                                case Style::FreeView:
                                    raceCameraStyle = Style::ThirdPerson;
                                    break;
                                }
                            }
                            else
                            {
                                using CameraStyle =
                                    r3d::game::originalrace::
                                        PreferredCamera;
                                profileState.config.preferredCamera =
                                    profileState.config.preferredCamera ==
                                            CameraStyle::Isometric
                                        ? CameraStyle::ThirdPerson
                                        : CameraStyle::Isometric;
                                raceCameraStyle =
                                    profileState.config.preferredCamera ==
                                            CameraStyle::ThirdPerson
                                        ? rrr3d::race::RaceCameraStyle::
                                              ThirdPerson
                                        : rrr3d::race::RaceCameraStyle::
                                              Isometric;
                                raceRenderer.resetCamera();
                                saveRaceProfile();
                            }
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
                    syncSourceGamers();
                    const auto gamerCommand =
                        sourceGamersFrame.handle(inputEvent);
                    if (gamerCommand)
                    {
#ifdef RRR3D_AUDIO
                        playMainButtonClick();
#endif
                        if (gamerCommand->type ==
                            r3d::game::originalracemenu::
                                GamerCommandType::SelectionChanged)
                        {
                            gamersSelectionChangedObserved = true;
                            refreshGamersFrame();
                        }
                        else
                        {
                            confirmOriginalGamer();
                        }
                    }
                    continue;
                }
                if (menuStack.back() == MenuScreen::Profiles)
                {
                    const auto profileCount =
                        activeProfileNames().size();
                    sourceProfileFrame.setProfileCount(profileCount);

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
                                        activeProfileNames()[
                                            profileDeleteIndex],
                                        profileError,
                                        networkHostRequested))
                                {
                                    std::cerr
                                        << "ProfileFrame delete failed: "
                                        << profileError << '\n';
                                }
                                else
                                {
                                    refreshProfilePage();
                                    if (!activeProfileNames().empty() &&
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
                            sourceProfileFrame.focusBack();
                        }
                        continue;
                    }

                    const auto profileCommand =
                        sourceProfileFrame.handle(inputEvent);
                    if (!profileCommand)
                        continue;
                    switch (profileCommand->type)
                    {
                    case r3d::game::mainmenu2::ProfileCommandType::Back:
                        backMenu();
                        break;
                    case r3d::game::mainmenu2::ProfileCommandType::Scrolled:
                        break;
                    case r3d::game::mainmenu2::ProfileCommandType::Delete:
                        if (profileCommand->index < profileCount)
                        {
                            profileDeleteIndex =
                                profileCommand->index;
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
                    case r3d::game::mainmenu2::ProfileCommandType::Select:
                        if (profileCommand->index < profileCount)
                        {
                            saveRaceProfile();
                            std::string profileError;
                            if (!profileStore.selectProfile(
                                    profileState,
                                    activeProfileNames()[
                                        profileCommand->index],
                                    profileError,
                                    networkHostRequested))
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
                                const auto* selectedEntry =
                                    sourceGarageFrame.selectedCar();
                                if (selectedEntry == nullptr ||
                                    selectedEntry->catalogIndex >=
                                        originalGarage->cars.size())
                                {
                                    garagePurchaseDialogVisible = false;
                                    continue;
                                }
                                const auto& car = originalGarage->cars[
                                    selectedEntry->catalogIndex];
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
                    sourceGarageFrame.updateColors(
                        sourceGarageColors());
                    const auto garageCommand =
                        sourceGarageFrame.handle(inputEvent);
                    menuSelection = sourceGarageFrame.focus();
                    if (!garageCommand)
                        continue;
                    using GarageCommandType =
                        r3d::game::originalracemenu::GarageCommandType;
                    if (garageCommand->type ==
                        GarageCommandType::SelectionChanged)
                    {
                        refreshGaragePage();
                        continue;
                    }
#ifdef RRR3D_AUDIO
                    playOriginalMenuSound(
                        garageCommand->type ==
                                GarageCommandType::SelectColor
                            ? rrr3d::audio::OriginalMenuSound::Repaint
                            : rrr3d::audio::OriginalMenuSound::ButtonClick);
#endif
                    if (garageCommand->type == GarageCommandType::Back)
                    {
                        backMenu();
                    }
                    else if (garageCommand->type ==
                             GarageCommandType::BuyOrSelect)
                    {
                        if (garageCommand->catalogIndex >=
                            originalGarage->cars.size())
                            continue;
                        const auto& car = originalGarage->cars[
                            garageCommand->catalogIndex];
                        if (car.record == profileState.player.currentCar)
                        {
                            backMenu();
                        }
                        else if (championshipMode)
                        {
                            garagePurchaseYesFocused = true;
                            garagePurchaseDialogVisible = true;
                            std::string purchase = localized("svBuyCar");
                            if (const auto marker = purchase.find("%s");
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
                                        *originalGarage, profileState,
                                        car, false, garageError))
                            {
                                saveRaceProfile();
                                backMenu();
                            }
                            else
                            {
                                std::cerr << "Original GarageFrame: "
                                          << garageError << '\n';
                            }
                        }
                    }
                    else if (garageCommand->type ==
                             GarageCommandType::SelectColor)
                    {
                        const std::size_t colorIndex =
                            garageCommand->colorIndex;
                        if (colorIndex >= garageColorPixels.size())
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
                    if (sourceWorkshopFrame.confirmation().type !=
                        WorkshopConfirmation::None)
                    {
                        if (inputEvent.action ==
                                rrr3d::input::Action::TurnLeft ||
                            inputEvent.action ==
                                rrr3d::input::Action::MenuUp)
                        {
                            sourceWorkshopFrame
                                .setConfirmationYesFocused(true);
                        }
                        else if (
                            inputEvent.action ==
                                rrr3d::input::Action::TurnRight ||
                            inputEvent.action ==
                                rrr3d::input::Action::MenuDown)
                        {
                            sourceWorkshopFrame
                                .setConfirmationYesFocused(false);
                        }
                        else if (
                            !inputEvent.repeated &&
                            (inputEvent.action ==
                                 rrr3d::input::Action::MenuBack ||
                             inputEvent.action ==
                                 rrr3d::input::Action::Pause))
                        {
                            sourceWorkshopFrame.cancelConfirmation();
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
                                sourceWorkshopFrame.confirmation();
                            const bool accepted =
                                confirmation.yesFocused;
                            const auto* pending =
                                confirmation.pendingCatalogIndex <
                                        originalGarage->workshop.size()
                                    ? &originalGarage->workshop[
                                          confirmation
                                              .pendingCatalogIndex]
                                    : nullptr;
                            sourceWorkshopFrame.cancelConfirmation();
                            if (accepted)
                            {
                                if (confirmation.type ==
                                        WorkshopConfirmation::Buy &&
                                    pending != nullptr)
                                {
                                    buyWorkshopGood(*pending);
                                }
                                else if (
                                    confirmation.type ==
                                    WorkshopConfirmation::Sell)
                                {
                                    stopWorkshopDrag(true, true);
                                }
                            }
                        }
                        continue;
                    }
                    sourceWorkshopFrame.updateSlots(
                        sourceWorkshopSlots());
                    const auto workshopCommand =
                        sourceWorkshopFrame.handle(
                            inputEvent,
                            inputEvent.source ==
                                    rrr3d::input::Source::Mouse &&
                                workshopPointerSlotPlane);
                    menuSelection = sourceWorkshopFrame.focus();
                    if (inputEvent.action ==
                            rrr3d::input::Action::MenuUp ||
                        inputEvent.action ==
                            rrr3d::input::Action::MenuDown ||
                        inputEvent.action ==
                            rrr3d::input::Action::TurnLeft ||
                        inputEvent.action ==
                            rrr3d::input::Action::TurnRight)
                    {
                        hideWorkshopWeaponDialog();
                        refreshWorkshopPage();
                    }
                    if (workshopCommand)
                    {
#ifdef RRR3D_AUDIO
                        playMainButtonClick();
#endif
                        if (workshopCommand->type ==
                            r3d::game::originalracemenu::
                                WorkshopCommandType::Back)
                        {
                            hideWorkshopWeaponDialog();
                            if (sourceWorkshopFrame.drag().active())
                                stopWorkshopDrag(false);
                            else
                                backMenu();
                        }
                        else
                        {
                            activateWorkshopFocus(
                                workshopCommand->pointerSlotPlane);
                        }
                    }
                    continue;
                }
                if (menuStack.back() == MenuScreen::Achievements)
                {
                    const bool dialogWasVisible =
                        sourceAchievementFrame.confirmation().visible;
                    const auto command =
                        sourceAchievementFrame.handle(inputEvent);
                    menuSelection = sourceAchievementFrame.focus();
#ifdef RRR3D_AUDIO
                    if (dialogWasVisible && !inputEvent.repeated &&
                        inputEvent.action ==
                            rrr3d::input::Action::MenuConfirm)
                    {
                        playMainButtonClick();
                    }
#endif
                    if (!command)
                        continue;
                    using AchievementCommand =
                        r3d::game::originalracemenu::
                            AchievementCommandType;
#ifdef RRR3D_AUDIO
                    if (!dialogWasVisible)
                        playMainButtonClick();
#endif
                    if (command->type == AchievementCommand::Back)
                    {
                        backMenu();
                    }
                    else if (command->type ==
                             AchievementCommand::RequestPurchase)
                    {
                        showAcceptDialog(
                            localized("svBuyReward"),
                            localized("svYes"),
                            localized("svNo"),
                            menu::virtualWidth * 0.5F,
                            menu::virtualHeight * 0.5F);
                    }
                    else
                    {
                        const auto pending = command->achievement;
                        const auto price = achievementPrice(pending);
                        if (profileState.achievementPoints < price)
                        {
                            showInfoDialog(
                                localized("svWarning"),
                                localized("svHintCantPoints"),
                                localized("svOk"),
                                menu::virtualWidth * 0.5F,
                                menu::virtualHeight * 0.5F);
                        }
                        else
                        {
                            profileState.achievementPoints -= price;
                            profileState
                                .achievementItems[std::string(
                                    originalAchievementVisuals[pending]
                                        .name)]
                                .values["state"] = "asOpened";
                            if (originalAchievementVisuals[pending].name ==
                                "armor4")
                            {
                                std::string armorError;
                                const bool substituted =
                                    applyArmor4Presentation(
                                        *originalGarage);
                                workshopRenderer.shutdown(*device);
                                if (!substituted ||
                                    !workshopRenderer.initialize(
                                        *device,
                                        originalResourceManager,
                                        *originalGarage, *originalRace,
                                        armorError))
                                {
                                    std::cerr
                                        << "Unable to apply source armor4 "
                                           "presentation: "
                                        << armorError << '\n';
                                    runtimeSmokeFailed = true;
                                    running = false;
                                }
                            }
                            saveRaceProfile();
                            refreshAchievementsPage();
                            std::cout
                                << "Original AchievmentFrame reward opened: "
                                << originalAchievementVisuals[pending].name
                                << '\n';
                        }
                    }
                    continue;
                }
                if (menuStack.back() == MenuScreen::Planets)
                {
                    const int previousSelection =
                        sourceAngarFrame.selection();
                    const auto command =
                        sourceAngarFrame.handle(inputEvent);
                    menuSelection = sourceAngarFrame.focus();
                    if (sourceAngarFrame.selection() != previousSelection)
                    {
#ifdef RRR3D_AUDIO
                        if (sourceAngarFrame.selection() >= 0)
                        {
                            playOriginalMenuSound(
                                rrr3d::audio::OriginalMenuSound::
                                    ShowPlanet);
                        }
#endif
                        refreshPlanetsPage();
                    }
                    if (!command)
                        continue;
                    using AngarCommand =
                        r3d::game::originalracemenu::AngarCommandType;
#ifdef RRR3D_AUDIO
                    if (command->fromPlanetSlot)
                    {
                        playOriginalMenuSound(
                            rrr3d::audio::OriginalMenuSound::ShowPlanet);
                    }
                    else
                    {
                        playMainButtonClick();
                    }
#endif
                    if (command->type == AngarCommand::Back)
                    {
                        backMenu();
                    }
                    else if (command->type ==
                             AngarCommand::RequestTravel)
                    {
                        showAngarTravelDialog(
                            command->planet,
                            command->fromPlanetSlot);
                    }
                    else if (command->type ==
                             AngarCommand::ChangePlanet)
                    {
                        changeAngarPlanet(command->planet);
                    }
                    else
                    {
                        const auto angarLayout =
                            sourceAngarFrame.layout(
                                menu::virtualWidth,
                                menu::virtualHeight,
                                static_cast<float>(
                                    angarBottomPanelImage.width),
                                static_cast<float>(
                                    angarBottomPanelImage.height),
                                static_cast<float>(
                                    angarPlanetInfoImage.width),
                                static_cast<float>(
                                    angarPlanetInfoImage.height),
                                static_cast<float>(
                                    garageBackImage.width));
                        showInfoDialog(
                            localized("svWarning"),
                            localized("svHintCantFlyPlanet"),
                            localized("svOk"),
                            angarLayout.planetX(command->planet),
                            angarLayout.planetY -
                                static_cast<float>(
                                    angarDoorSlotImage.height) * 0.5F -
                                static_cast<float>(
                                    infoDialogFrameImage.height) * 0.5F);
                        std::cout
                            << "Original AngarFrame warning: "
                            << localized("svHintCantFlyPlanet") << '\n';
                    }
                    continue;
                }
                if (menuStack.back() == MenuScreen::Finish)
                {
                    if (sourceFinishFrame.handle(inputEvent))
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
                    if (sourceFinalFrame.handle(
                            inputEvent, pointerTargetsItem))
                        closeOriginalFinalMenu();
                    continue;
                }
                auto& page = activeMenuPage();
                if (menuStack.back() == MenuScreen::RaceMenu &&
                    (inputEvent.action ==
                         rrr3d::input::Action::MenuUp ||
                     inputEvent.action ==
                         rrr3d::input::Action::MenuDown))
                {
                    // RaceMainFrame's source NavElements contain no up/down
                    // neighbors, so vertical input is intentionally inert.
                    continue;
                }
                if (inputEvent.action ==
                    rrr3d::input::Action::MenuUp)
                {
                    if (sourceMainMenuFrame.owns(menuStack.back()))
                    {
                        menuSelection = sourceMainMenuFrame.moveSelection(
                            menuSelection, -1);
                    }
                    else
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
                    }
#ifdef RRR3D_PHYSICS
                    if (isOriginalOptionsScreen(menuStack.back()))
                    {
                        const auto tab =
                            r3d::game::originaloptions::
                                OptionsMenuState::tabForScreen(
                                    menuStack.back());
                        if (menuSelection <
                            sourceOptionsMenu.rowCount(tab))
                        {
                            sourceOptionsMenu.ensureVisible(
                                tab, menuSelection);
                        }
                    }
#endif
                    continue;
                }
                if (inputEvent.action ==
                    rrr3d::input::Action::MenuDown)
                {
                    if (sourceMainMenuFrame.owns(menuStack.back()))
                    {
                        menuSelection = sourceMainMenuFrame.moveSelection(
                            menuSelection, 1);
                    }
                    else
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
                    }
#ifdef RRR3D_PHYSICS
                    if (isOriginalOptionsScreen(menuStack.back()))
                    {
                        const auto tab =
                            r3d::game::originaloptions::
                                OptionsMenuState::tabForScreen(
                                    menuStack.back());
                        if (menuSelection <
                            sourceOptionsMenu.rowCount(tab))
                        {
                            sourceOptionsMenu.ensureVisible(
                                tab, menuSelection);
                        }
                    }
#endif
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
                        const int direction =
                            inputEvent.action ==
                                    rrr3d::input::Action::TurnLeft
                                ? -1
                                : 1;
                        menuSelection =
                            sourceRaceMainFrame.moveSelection(
                                menuSelection, direction);
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
                        sourceOptionsMenu.setControlsUseGamepad(
                            inputEvent.action ==
                            rrr3d::input::Action::TurnRight);
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
                        // TournamentFrame::Continue uses lastNetProfile in
                        // the Network stack and lastProfile offline.
#ifdef RRR3D_NETWORK
                        if (networkHostRequested)
                        {
                            const auto& profiles = activeProfileNames();
                            const auto last = std::find(
                                profiles.begin(), profiles.end(),
                                profileState.lastNetworkProfile);
                            const auto& selected =
                                last != profiles.end()
                                    ? *last
                                    : profiles.front();
                            saveRaceProfile();
                            std::string profileError;
                            if (!profileStore.selectProfile(
                                    profileState, selected, profileError,
                                    true) ||
                                !reloadCurrentRace())
                            {
                                std::cerr
                                    << "Unable to continue original network "
                                       "profile: "
                                    << profileError << '\n';
                                break;
                            }
                        }
#endif
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
                                        profileState, difficulty,
#ifdef RRR3D_NETWORK
                                        networkHostRequested
#else
                                        false
#endif
                                    );
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
                    if (!options->legacyWindowsDebug &&
                        championshipMode &&
                        newTournamentProfile &&
#ifdef RRR3D_NETWORK
                        !networkHostRequested &&
#endif
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
                                profileError,
#ifdef RRR3D_NETWORK
                                networkHostRequested
#else
                                false
#endif
                                ))
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
                        saveRaceProfile();
                        if (!networkHostOfflineProfile)
                            networkHostOfflineProfile =
                                profileState.player;
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
                        if (networkSession.beginLanSearch(
                                error, options->legacyWindowsDebug))
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
                {
                    const auto raceMenuCommand =
                        sourceRaceMainFrame.command(menuSelection);
                    if (!raceMenuCommand)
                        break;
                    using RaceMainCommand =
                        r3d::game::originalracemenu::RaceMainCommand;
                    if (*raceMenuCommand == RaceMainCommand::StartRace)
                    {
                        activateRaceMenuStart();
                    }
                    else if (*raceMenuCommand ==
                             RaceMainCommand::Workshop)
                    {
                        sourceRaceMenu.setState(
                            r3d::game::originalracemenu::State::Workshop);
                        sourceWorkshopFrame.show(
                            sourceWorkshopCandidates(),
                            sourceWorkshopSlots());
                        hideWorkshopWeaponDialog();
                        pushMenu(MenuScreen::Workshop);
                        refreshWorkshopPage();
                    }
                    else if (*raceMenuCommand == RaceMainCommand::Garage)
                    {
                        sourceRaceMenu.setState(
                            r3d::game::originalracemenu::State::Garage);
                        rebuildGarageCarOrder();
                        refreshGaragePage();
                        pushMenu(MenuScreen::Garage);
                    }
                    else if (*raceMenuCommand == RaceMainCommand::Angar)
                    {
                        sourceRaceMenu.setState(
                            r3d::game::originalracemenu::State::Angar);
                        pushMenu(MenuScreen::Planets);
                        sourceAngarFrame.show(
                            sourceAngarPlanets(), racePlanetChampion,
                            championshipMode,
                            sourceAngarNetworkClient(),
                            profileState.player.currentPlanet);
                        menuSelection = sourceAngarFrame.focus();
                        refreshPlanetsPage();
                    }
                    else if (*raceMenuCommand ==
                             RaceMainCommand::Achievements)
                    {
                        sourceRaceMenu.setState(
                            r3d::game::originalracemenu::State::Achievements);
                        pushMenu(MenuScreen::Achievements);
                        sourceAchievementFrame.show(
                            sourceAchievementEntries());
                        refreshAchievementsPage();
                    }
                    else if (*raceMenuCommand == RaceMainCommand::Options)
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
                }
                case MenuScreen::Garage:
                    // GarageFrame input is dispatched by its source owner
                    // before the shared list-page command switch.
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
                        bindingCaptureGamepad =
                            sourceOptionsMenu.controlsUseGamepad();
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
            else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
                     event.type == SDL_EVENT_WINDOW_METAL_VIEW_RESIZED)
            {
                requestDrawableResize(
                    event.window.data1, event.window.data2);
            }
        }

#ifdef RRR3D_VIDEO
        if (originalMovieInputSuppressionFrames > 0U)
            --originalMovieInputSuppressionFrames;
#endif

        // A Cocoa fullscreen transition or a live resize may enqueue dozens
        // of pixel-size events. Recreate the bgfx backbuffer and the three
        // source render graphs only once, at the newest size, after input has
        // already been drained for this frame.
        if (!applyPendingDrawableResize())
            running = false;

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
        const float rawFrameSeconds = std::max(
            static_cast<float>(currentFrameTicks - previousFrameTicks) /
                1000000000.0F,
            0.0F);
        sourceFrameDeltas[sourceFrameDeltaCursor] = rawFrameSeconds;
        sourceFrameDeltaCursor =
            (sourceFrameDeltaCursor + 1U) % sourceFrameDeltas.size();
        sourceFrameDeltaCount = std::min(
            sourceFrameDeltaCount + 1U, sourceFrameDeltas.size());
        const float smoothedFrameSeconds = std::accumulate(
            sourceFrameDeltas.begin(),
            sourceFrameDeltas.begin() +
                static_cast<std::ptrdiff_t>(sourceFrameDeltaCount),
            0.0F) / static_cast<float>(sourceFrameDeltaCount);
        float frameSeconds = std::clamp(
            smoothedFrameSeconds, 0.0F, 7.0F / 60.0F);
        if (options->startupSmokeTest)
        {
            // Exercise the complete twelve-second source timeline without
            // turning the regression into a wall-clock delay.
            frameSeconds = 0.25F;
        }
        else if (options->finalMenuSmokeTest)
        {
#ifdef RRR3D_AUDIO
            const bool waitingForFinalMusic =
                menuStack.back() == MenuScreen::Credits &&
                !finalMusic.currentVoiceActive();
            frameSeconds = waitingForFinalMusic ? 0.0F : 0.4F;
            if (waitingForFinalMusic)
            {
                // The accelerated renderer fixture can otherwise exhaust its
                // frame budget before the asynchronous Ogg worker is given a
                // scheduling opportunity.  FinalMenu::OnProgress starts only
                // after the source TrackFinal voice has really started.
                SDL_Delay(1U);
            }
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
            if (sourceFinalFrame.progress(frameSeconds))
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
            // HumanPlayer polls these actions into bools and gives gaAccel
            // priority when both are held. Analog trigger bindings therefore
            // become binary gas/brake commands in the source as well.
            const bool accelerateHeld = input.heldValue(
                rrr3d::input::Action::Accelerate) > 0.0F;
            const bool reverseHeld = input.heldValue(
                rrr3d::input::Action::Brake) > 0.0F;
            const auto sourceSteering = [&](rrr3d::input::Action action) {
                const float keyboard = input.heldValue(
                    action, rrr3d::input::Source::Keyboard);
                if (keyboard > 0.0F)
                    return std::pair<float, bool>{1.0F, false};
                const float button = input.heldValue(
                    action, rrr3d::input::Source::GamepadButton);
                if (button > 0.0F)
                    return std::pair<float, bool>{button, false};
                const float axis = input.heldValue(
                    action, rrr3d::input::Source::GamepadAxis);
                return std::pair<float, bool>{axis, axis > 0.0F};
            };
            const auto leftSteering = sourceSteering(
                rrr3d::input::Action::TurnLeft);
            const auto rightSteering = sourceSteering(
                rrr3d::input::Action::TurnRight);
            const auto humanDriving =
                r3d::game::originalrace::source::HumanPlayer::
                    OnInputProgress(
                        accelerateHeld, reverseHeld,
                        leftSteering.first, rightSteering.first,
                        leftSteering.second, rightSteering.second);
            raceInput.throttle = humanDriving.throttle;
            raceInput.reverse = humanDriving.reverse;
            raceInput.brake = 0.0F;
            raceInput.steering = humanDriving.steering;
            raceInput.manualSteering = humanDriving.manualSteering;
            if (options->raceRenderSmokeTest)
            {
                raceInput.steering = renderedFrames >= 90 &&
                                             renderedFrames < 180
                                         ? 0.35F
                                         : 0.0F;
                raceInput.manualSteering =
                    std::abs(raceInput.steering) > 0.0001F;
            }
            r3d::game::originalrace::RaceControl control;
            control.driving = raceInput;
            control.useWeapon = raceUseWeaponRequested;
            control.useAllWeapons = raceUseAllWeaponsRequested;
            control.useMine = raceUseMineRequested;
            control.mineHeld = input.heldValue(
                rrr3d::input::Action::UseMine);
            control.mineAnalogBinding = raceMineAnalogBinding;
            control.useHyper = raceUseHyper;
            control.changeWeapon = raceChangeWeaponRequested;
            control.weaponChange = raceWeaponChangeDirection;
            control.fireWeaponSlot = raceFireWeaponSlotRequested;
            control.reset = raceResetRequested;
            control.chatMode = userChat.inputVisible();
#ifdef RRR3D_NETWORK
            if (networkMatchStarted &&
                !options->legacyWindowsDebug)
            {
                if (networkHostRequested &&
                    networkSnapshot.models.raceActive)
                {
                    if (networkHostRaceGoSeconds < 0.0F &&
                        networkSession.hostGoWaitComplete())
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
                        !raceSession.synchronizePlayerPresentation(
                            index, player->gamerId, player->color))
                    {
                        continue;
                    }
                    if (player->owner)
                        continue;
                    // NetPlayer::ResponseStream tests Player::GetFinished(),
                    // not only its replicated _raceFinish flag.  The local
                    // race state can reach the finish first; never let a late
                    // UDP pose move that already-finished car again.
                    if (index < raceSession.racers().size() &&
                        raceSession.racers()[index].GetFinished())
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
                    if (raceRenderVehicles.size() < raceVehicles.size())
                        raceRenderVehicles = raceVehicles;
                    const auto& physicsPose =
                        physicsWorld->vehicle(index).body;
                    const auto& graphPose =
                        raceRenderVehicles[index].body;
                    const auto networkCorrection =
                        raceSession.synchronizeRacerNetworkPose(
                            index,
                        {physicsPose.position.x,
                         physicsPose.position.y,
                         physicsPose.position.z},
                        {graphPose.position.x,
                         graphPose.position.y,
                         graphPose.position.z},
                        {graphPose.rotation.x,
                         graphPose.rotation.y,
                         graphPose.rotation.z,
                         graphPose.rotation.w},
                        {player->vehicle.position[0],
                         player->vehicle.position[1],
                         player->vehicle.position[2]},
                        {player->vehicle.rotation[0],
                         player->vehicle.rotation[1],
                         player->vehicle.rotation[2],
                         player->vehicle.rotation[3]});
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
                         player->vehicle.angularMomentum[2]},
                        networkCorrection.snapRotation);
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
            const auto humanRacer = raceSession.humanRacer();
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
            const bool localHumanFinished =
                humanRacer < raceSession.racers().size() &&
                raceSession.racers()[humanRacer].GetFinished();
            if (networkMatchStarted && localHumanFinished &&
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
                localHumanFinished)
            {
                if (networkSession.hostRaceFinishComplete())
                {
                    raceSession.startNetworkFinishTimer();
                    networkHostFinishTimerStarted = true;
                }
            }
#endif
            if (options->raceRenderSmokeTest &&
                humanRacer < raceSession.racers().size())
                minimumRacePlayerLife = std::min(
                    minimumRacePlayerLife,
                    raceSession.racers()[humanRacer].GetLife());
            raceUseWeaponRequested = false;
            raceUseAllWeaponsRequested = false;
            raceUseMineRequested = false;
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
                                ? raceSession.racerMapObjectId(
                                      event.target)
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
                    event.target == humanRacer)
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
#ifdef RRR3D_AUDIO
                    // SoundMotor belongs to the source Car. Its destructor
                    // frees both loops as soon as the car is destroyed.
                    stopRacerMotorAudio(event.target);
#endif
                }
#ifdef RRR3D_AUDIO
                if (event.kind ==
                        r3d::game::originalrace::RaceEventKind::Respawn)
                {
                    // ResetCar creates a fresh car and therefore a fresh
                    // SoundMotor instance.
                    startRacerMotorAudio(event.racer);
                }
#endif
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
            if (humanRacer < raceVehicles.size())
            {
                const auto listener =
                    raceVehicles[humanRacer].body.position;
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
                                ShotEffectAudio source;
                                source.owner = event.racer;
                                source.source = event.soundSource;
                                source.path = event.soundPath;
                                source.position = event.position;
                                source.sound = sound;
                                if (source.owner < raceVehicles.size())
                                {
                                    const auto& body = raceVehicles[
                                        source.owner].body;
                                    const r3d::physics::Quat inverse{
                                        -body.rotation.x,
                                        -body.rotation.y,
                                        -body.rotation.z,
                                        body.rotation.w};
                                    source.followOffset = rotateRaceVector(
                                        inverse,
                                        {event.position.x - body.position.x,
                                         event.position.y - body.position.y,
                                         event.position.z - body.position.z});
                                    if (std::abs(body.scale.x) > 0.000001F)
                                        source.followOffset.x /= body.scale.x;
                                    if (std::abs(body.scale.y) > 0.000001F)
                                        source.followOffset.y /= body.scale.y;
                                    if (std::abs(body.scale.z) > 0.000001F)
                                        source.followOffset.z /= body.scale.z;
                                }
                                shotEffectAudio.push_back(std::move(source));
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
                    {
                        const auto& body =
                            raceVehicles[source->owner].body;
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
                for (const auto& momentum :
                     raceSession.takeAngularMomentumRequests())
                {
                    physicsWorld->setAngularMomentum(
                        momentum.racer, momentum.momentum);
                }
                for (std::size_t racer = 0;
                     racer < raceSession.racers().size(); ++racer)
                {
                    physicsWorld->setWheelTractionEnabled(
                        racer,
                        !raceSession.racers()[racer]
                             .gameCar.IsClutchLocked());
                    if (raceSession.racers()[racer]
                            .slowEffect.IsEffectMaked())
                    {
                        physicsWorld->clampLinearSpeed(
                            racer,
                            r3d::game::originalrace::source::
                                SlowEffect::maximumSpeed);
                    }
                }
                auto vehicleInputs = raceSession.vehicleInputs();
                if (options->raceRenderSmokeTest)
                {
                    for (std::size_t index = 0U;
                         index < vehicleInputs.size() &&
                         index < raceVehicles.size() &&
                         index < raceSession.racers().size(); ++index)
                    {
                        if (!raceSession.racers()[index].IsComputer())
                            continue;
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
                        input.manualSteering =
                            player->vehicle.steerState == 3U;
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
                    const auto& state = raceVehicles[index];
                    raceSession.synchronizeRacerPhysicsState(
                        index,
                        {{state.body.position.x,
                          state.body.position.y,
                          state.body.position.z},
                         {state.body.rotation.x,
                          state.body.rotation.y,
                          state.body.rotation.z,
                          state.body.rotation.w}},
                        {state.linearVelocity.x,
                         state.linearVelocity.y,
                         state.linearVelocity.z},
                        state.bodyAwake);
                    if (options->raceRenderSmokeTest &&
                        index < raceSession.racers().size() &&
                        raceSession.racers()[index].IsComputer())
                    {
                        maximumRaceAiSpeeds[index] = std::max(
                            maximumRaceAiSpeeds[index],
                            std::abs(raceVehicles[index].speed));
                        maximumRaceAiProgress[index] = std::max(
                            maximumRaceAiProgress[index],
                            static_cast<float>(
                                raceSession.racers()[index].car.numLaps) +
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
            raceRenderVehicles = raceVehicles;
            {
                const auto syncCount = std::min(
                    raceRenderVehicles.size(),
                    raceSession.racers().size());
                for (std::size_t index = 0U;
                     index < syncCount; ++index)
                {
                    raceRenderVehicles[index] =
                        raceSession.racerFrameState(
                            index, raceVehicles[index], frameSeconds);
                }
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
            if (humanRacer < raceVehicles.size())
            {
                const auto& listener =
                    raceVehicles[humanRacer].body.position;
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
                    const auto motorMix =
                        raceSession.racerMotorMix(racer);
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
                        spatial.gain * motorMix.idleVolume *
                        (idleSourceVolume != engineSoundVolumes.end()
                             ? idleSourceVolume->second
                             : 1.0F);
                    const float rpmVolume =
                        spatial.gain * motorMix.rpmVolume *
                        (rpmSourceVolume != engineSoundVolumes.end()
                             ? rpmSourceVolume->second
                             : 1.0F);
                    audio.setVoiceParameters(
                        motorAudio.idleVoice, idleVolume, 1.0F, 0.0F);
                    audio.setVoiceParameters(
                        motorAudio.rpmVoice, rpmVolume,
                        motorMix.rpmFrequencyRatio,
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
                         raceVehicles[racer].wheels.size(),
                         definition.wheelSlipEffects.size(),
                         definition.wheelSlipSounds.size(),
                         slipVoices.size()});
                    for (std::size_t wheel = 0;
                         wheel < wheelCount; ++wheel)
                    {
                        auto& voice = slipVoices[wheel];
                        const auto slip =
                            raceSession.racers()[racer]
                                .gameCar.GetWheelSlipResult(wheel);
                        if (!slip.active)
                        {
                            if (slip.stopSound && voice.voice !=
                                r3d::audio::invalidVoice)
                            {
                                stopRaceLoopVoice(voice.voice);
                                voice = {};
                            }
                            continue;
                        }
                        // PxWheelSlipEffect places its visual child at the
                        // PhysX contact point, but EventEffect::OnProgress
                        // moves the Source3d to the owner CarWheel GameObject.
                        // Preserve that split: the sound follows the wheel,
                        // not a noisy road-contact sample.
                        const auto& wheelPosition =
                            raceVehicles[racer].wheels[wheel].position;
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
                            voice.voice = playRaceLoop(
                                wheelSlipSound, options);
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
                                    slip.volume *
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
                    physicsWorld->vehicle(humanRacer).linearVelocity.x *
                        physicsWorld->vehicle(humanRacer).linearVelocity.x +
                    physicsWorld->vehicle(humanRacer).linearVelocity.y *
                        physicsWorld->vehicle(humanRacer).linearVelocity.y +
                    physicsWorld->vehicle(humanRacer).linearVelocity.z *
                        physicsWorld->vehicle(humanRacer).linearVelocity.z));
            maximumRaceSmokeContacts = std::max(
                maximumRaceSmokeContacts,
                physicsWorld->vehicle(humanRacer).contactCount);
            if (!raceSession.racers().empty() &&
                raceSession.finishPresentationReady())
            {
#ifdef RRR3D_NETWORK
                showFinishMenu(
                    !networkMatchStarted || networkHostRequested);
                // NetRace::ExitRace calls GameMode::ExitRace and
                // ExitRaceGoFinish before serializing the RPC.  At this
                // point Tournament::CompleteTrack has advanced selectedTrack
                // and reset pass points, so publish those resulting values.
                if (networkMatchStarted && networkHostRequested &&
                    networkRaceStarted && !networkRaceExitApplied)
                {
                    std::string error;
                    if (!networkSession.exitRace(
                            static_cast<std::int32_t>(
                                r3d::game::originalrace::
                                    originalTournamentTrackIndexInPlanet(
                                        *originalRace, selectedTrack)),
                            static_cast<std::int32_t>(
                                originalRace->environment.weather),
                            collectNetworkRaceResults(), error))
                    {
                        std::cerr
                            << "Original NetRace::ExitRace failed: "
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
#else
                showFinishMenu(true);
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
        if (sourceMenuMusicGain < 1.0F)
        {
            // GameMode::OnFrame applies
            //   gain += (1 - gain) * dt / 1 second
            // after OnFinishFrameClose::FadeOutMusic(0).
            sourceMenuMusicGain = std::clamp(
                sourceMenuMusicGain +
                    (1.0F - sourceMenuMusicGain) * frameSeconds,
                0.0F, 1.0F);
            audio.setBusVolume(
                r3d::audio::Bus::Music,
                profileState.config.musicVolume *
                    sourceMenuMusicGain);
            finishMenuMusicFadeObserved =
                finishMenuMusicFadeObserved ||
                (sourceMenuMusicGain > 0.0F &&
                 sourceMenuMusicGain < 1.0F &&
                 audio.busVolume(r3d::audio::Bus::Music) > 0.0F);
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

#ifdef RRR3D_PHYSICS
        if (options->finishMenuSmokeTest &&
            sourceFinishFrame.shown() &&
            finishLastEventObserved &&
            renderedFrames >= 240U)
        {
            closeFinishMenu();
        }
#endif

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
                     index < originalMusicCatalog.menu.size(); ++index)
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
                if (!music.paused() || music.currentVoiceActive() ||
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
#endif
        sourceDialogs.Progress(
            frameSeconds, {menu::virtualWidth, menu::virtualHeight});

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
        worldEventPump.FrameStep(frameSeconds, 0.0F);
        const auto gameModeCommands = gameModeState.TakeCommands();
        if (std::find(
                gameModeCommands.begin(), gameModeCommands.end(),
                r3d::game::originalrace::source::GameModeCommand::
                    DoStartRace) != gameModeCommands.end())
            doStartCurrentRace();
        if (gameModeState.IsRaceLoading())
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
            gameModeState.OnLoadingFramePresented();
            ++renderedFrames;
            continue;
        }
        if (inRace)
        {
            const auto humanRacer = raceSession.humanRacer();
            gameDebug.updateFrame(frameSeconds);
            const float raceRenderSeconds =
                raceSession.phase() ==
                        r3d::game::originalrace::RacePhase::Paused
                    ? 0.0F
                    : frameSeconds;
            auto cameraStyle = raceCameraStyle;
            if (options->raceRenderSmokeTest)
            {
                if (options->legacyWindowsDebug)
                {
                    constexpr std::array styles{
                        rrr3d::race::RaceCameraStyle::ThirdPerson,
                        rrr3d::race::RaceCameraStyle::Isometric,
                        rrr3d::race::RaceCameraStyle::Lights,
                        rrr3d::race::RaceCameraStyle::IsometricView,
                        rrr3d::race::RaceCameraStyle::FreeView};
                    const auto styleIndex =
                        (renderedFrames / 8U) % styles.size();
                    cameraStyle = styles[styleIndex];
                    legacyDebugCameraStylesObserved[styleIndex] = true;
                }
                else
                {
                    cameraStyle =
                        renderedFrames < options->smokeFrames / 2U
                            ? rrr3d::race::RaceCameraStyle::Isometric
                            : rrr3d::race::RaceCameraStyle::ThirdPerson;
                }
                raceCameraStylesObserved[
                    cameraStyle ==
                            rrr3d::race::RaceCameraStyle::ThirdPerson
                        ? 0U
                        : 1U] = true;
            }
            if (options->legacyWindowsDebug &&
                (cameraStyle ==
                     rrr3d::race::RaceCameraStyle::Lights ||
                 cameraStyle ==
                     rrr3d::race::RaceCameraStyle::IsometricView ||
                 cameraStyle ==
                     rrr3d::race::RaceCameraStyle::FreeView))
            {
                const bool* keys = SDL_GetKeyboardState(nullptr);
                const float forward =
                    (keys[SDL_SCANCODE_W] ? 1.0F : 0.0F) -
                    (keys[SDL_SCANCODE_S] ? 1.0F : 0.0F);
                const float right =
                    (keys[SDL_SCANCODE_A] ? 1.0F : 0.0F) -
                    (keys[SDL_SCANCODE_D] ? 1.0F : 0.0F);
                raceRenderer.moveDebugCamera(
                    cameraStyle, forward, right, raceRenderSeconds);
            }
            const float sourceFreeWheelSpeed =
                raceSession.racers()[humanRacer]
                    .gameCar.GetDrivenWheelSpeed();
            const auto raceCamera = raceRenderer.makeCamera(
                *device, physicsWorld->vehicle(humanRacer),
                sourceFreeWheelSpeed,
                static_cast<std::uint32_t>(pixelWidth),
                static_cast<std::uint32_t>(pixelHeight),
                cameraStyle,
                profileState.config.cameraDistance, raceRenderSeconds);
            auto raceQuality = profileState.config.quality;
            raceQuality.postEffect = gameDebug.effectivePostEffect(
                raceQuality.postEffect);
            raceRenderer.renderFrame(
                *device, raceShader, raceCamera, 0x6b91b8ffU,
                *originalRace, raceRenderVehicles, racePipeline,
                raceSession.decorationActive(),
                decorationFragments, vehicleDeathFragments,
                raceSession.bonusActive(), raceSession.bonusScales(),
                raceSession.racers(),
                raceSession.effects(), raceSession.mines(),
                raceSession.projectiles(), raceElapsedSeconds,
                raceQuality, raceSession.countdownStage(),
                gameDebug.traceVisible() ? &sourceTraceGfx : nullptr);
            raceHud.update(*device, *originalRace, raceSession,
                           raceRenderVehicles, raceCamera,
                           raceRenderSeconds);
            device->beginOverlay(camera);
            // PlayerStateFrame::OnInvalidate hides only _raceState when
            // enableHUD is false; MiniMapFrame likewise hides only its lap
            // widgets. Event overlays, the map and the countdown remain.
            raceHud.draw(*device, quad, shader, raceShader,
                         profileState.config.enableHud);
            if (gameDebug.takeOverlayRefresh())
            {
                for (auto& line : gameDebugVisual)
                    if (valid(line.texture))
                        device->destroy(line.texture);
                gameDebugVisual.clear();
                const auto sourceLines = gameDebug.lines(
                    *originalRace, raceSession, raceVehicles,
                    *physicsDescription, device->renderTelemetry());
                gameDebugVisual.reserve(sourceLines.size());
                for (const auto& line : sourceLines)
                {
                    gameDebugVisual.push_back(createText(
                        *device, line, 17.0F, true,
                        menu::Rgba8{222U, 255U, 212U, 255U},
                        resolvedFont));
                }
            }
            if (gameDebug.overlayVisible() &&
                !gameDebugVisual.empty())
            {
                constexpr float debugLeft = 18.0F;
                constexpr float debugTop = 16.0F;
                constexpr float debugLineStep = 22.0F;
                const auto widest = std::max_element(
                    gameDebugVisual.begin(), gameDebugVisual.end(),
                    [](const TextVisual& first,
                       const TextVisual& second) {
                        return first.width < second.width;
                    });
                const float panelWidth = std::min(
                    widest->width + 24.0F,
                    menu::virtualWidth - debugLeft * 2.0F);
                const float panelHeight =
                    static_cast<float>(gameDebugVisual.size()) *
                        debugLineStep +
                    16.0F;
                drawQuadTinted(
                    *device, quad, shader, background,
                    panelWidth, panelHeight,
                    debugLeft + panelWidth * 0.5F,
                    debugTop + panelHeight * 0.5F,
                    7.0F, transparent,
                    {0.0F, 0.0F, 0.0F, 0.72F});
                for (std::size_t line = 0U;
                     line < gameDebugVisual.size(); ++line)
                {
                    const auto& text = gameDebugVisual[line];
                    const float scale = std::min(
                        1.0F,
                        (panelWidth - 16.0F) /
                            std::max(text.width, 1.0F));
                    drawQuad(
                        *device, quad, shader, text.texture,
                        text.width * scale, text.height * scale,
                        debugLeft + 8.0F +
                            text.width * scale * 0.5F,
                        debugTop + 8.0F +
                            text.height * scale * 0.5F +
                            static_cast<float>(line) *
                                debugLineStep,
                        6.0F, transparent);
                }
            }
            drawUserChat();
            drawAcceptDialog();
#ifdef RRR3D_AUDIO
            drawOriginalMusicDialog();
#endif
            drawOriginalCursor(exitRaceDialogVisible);
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
        const auto& raceMenuVisibility =
            sourceRaceMenu.visibility();
        const bool drawingOriginalGamers =
            menuStack.back() == MenuScreen::Gamers &&
            raceMenuVisibility.gamers;
        const bool drawingOriginalRaceMenu =
            menuStack.back() == MenuScreen::RaceMenu &&
            raceMenuVisibility.main;
        const bool drawingOriginalGarage =
            menuStack.back() == MenuScreen::Garage &&
            raceMenuVisibility.garage;
        const bool drawingOriginalWorkshop =
            menuStack.back() == MenuScreen::Workshop &&
            raceMenuVisibility.workshop;
        const bool drawingOriginalAngar =
            menuStack.back() == MenuScreen::Planets &&
            raceMenuVisibility.angar;
        const bool drawingOriginalAchievements =
            menuStack.back() == MenuScreen::Achievements &&
            raceMenuVisibility.achievements;
        const bool drawingOriginalFinish =
            menuStack.back() == MenuScreen::Finish;
        const r3d::game::originalrace::OriginalGarageCar*
            presentationCar = nullptr;
        bool presentationCarLocked = false;
        if (drawingOriginalGarage &&
            sourceGarageFrame.selectedCar() != nullptr)
        {
            const auto& selectedView =
                *sourceGarageFrame.selectedCar();
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
            sourceAngarFrame.progress(frameSeconds);
            const auto redLampState =
                sourceSpaceshipFrame.progress(frameSeconds);
            auto& redLamp = originalAngarScene->environment.lamps[1];
            redLamp.color = {
                redLampState.intensity, 0.0F, 0.0F,
                redLampState.intensity};
            redLamp.enabled = redLampState.enabled;
            const auto angarSourceCamera = updateSourceAutoObserver(
                angarObserver,
                originalAngarScene->presentationCamera,
                frameSeconds, bx::kPi / 96.0F,
                bx::toRad(65.0F), bx::toRad(45.0F),
                bx::toRad(80.0F), bx::toRad(40.0F),
                bx::toRad(30.0F));
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
                garageBonusScales,
                angarRacerRuntime, garageEffects, garageMines,
                garageProjectiles,
                sourceSpaceshipFrame.sceneSeconds(),
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
                runtime.gameCar.destroyed =
                    presentationCarLocked ||
                    racer != selectedRacer;
                runtime.weaponSlots.fill(
                    r3d::game::originalrace::RacerRuntime::
                        invalidWeapon);
                if (racer < originalGarageScene->racers.size())
                {
                    originalGarageScene->racers[racer].color =
                        profileState.player.color;
                    runtime.SetColor(profileState.player.color);
                }
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
                    : updateSourceAutoObserver(
                          garageObserver,
                          originalGarageScene
                              ->presentationCamera,
                          frameSeconds, bx::kPi / 48.0F,
                          bx::toRad(75.0F), bx::toRad(25.0F),
                          bx::toRad(80.0F));
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
                garageBonusScales,
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
            const auto finalLayout = sourceFinalFrame.layout(
                menu::virtualWidth, menu::virtualHeight,
                finalCreditsHeight,
                static_cast<float>(finalBackSelectedImage.width));
            for (std::size_t index = 0U;
                 index < finalSlides.size(); ++index)
            {
                const float alpha =
                    sourceFinalFrame.slideAlpha(index);
                if (alpha <= 0.0F)
                    continue;
                const float slideAspect =
                    static_cast<float>(finalSlideImages[index].width) /
                    std::max(
                        static_cast<float>(
                            finalSlideImages[index].height),
                        1.0F);
                const float slideWidth = std::min(
                    finalLayout.slideMaximumWidth,
                    finalLayout.slideMaximumHeight * slideAspect);
                const float slideHeight = slideWidth / slideAspect;
                finalSlidesObserved[index] =
                    finalSlidesObserved[index] || alpha >= 0.5F;
                drawQuadTinted(
                    *device, quad, shader, finalSlides[index],
                    slideWidth, slideHeight,
                    finalLayout.slideX, finalLayout.slideY,
                    60.0F, transparent,
                    {1.0F, 1.0F, 1.0F, alpha});
            }

            finalCreditsMotionObserved =
                finalCreditsMotionObserved ||
                finalLayout.creditsY < menu::virtualHeight - 1.0F;
            float sectionTop = finalLayout.creditsY;
            for (const auto& section : finalCredits)
            {
                drawQuad(
                    *device, quad, shader, section.caption.texture,
                    std::min(
                        section.caption.width,
                        finalLayout.creditsWidth),
                    section.caption.height, finalLayout.creditsX,
                    sectionTop + section.caption.height * 0.5F,
                    35.0F, transparent);
                const float textTop =
                    sectionTop + section.caption.height + 10.0F;
                float lineTop = textTop;
                for (const auto& line : section.lines)
                {
                    drawQuad(
                        *device, quad, shader, line.texture,
                        std::min(
                            line.width,
                            finalLayout.creditsWidth),
                        line.height,
                        finalLayout.creditsX,
                        lineTop + line.height * 0.5F,
                        35.0F, transparent);
                    lineTop += line.height;
                }
                sectionTop += section.height;
            }

            drawQuad(
                *device, quad, shader, finalBackSelected,
                static_cast<float>(finalBackSelectedImage.width),
                static_cast<float>(finalBackSelectedImage.height),
                finalLayout.backX, finalLayout.backY,
                20.0F, transparent);
            drawQuad(
                *device, quad, shader, finalBackText.texture,
                finalBackText.width, finalBackText.height,
                finalLayout.backX, finalLayout.backY,
                10.0F, transparent);
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
                const bool selected =
                    sourceStartOptionsMenu.focus() == row;
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
                !sourceStartOptionsMenu.applyEnabled()
                    ? startOptionsActionPage.disabled.front()
                    : sourceStartOptionsMenu.focus() ==
                              r3d::game::originaloptions::
                                  StartOptionsMenuState::applyRow
                          ? startOptionsActionPage.selected.front()
                          : startOptionsActionPage.normal.front();
            drawQuad(
                *device, quad, shader, apply.texture,
                apply.width, apply.height, applyX, applyY,
                15.0F, transparent);

            startOptionsFrameObserved = true;
            startOptionsSelectGateObserved =
                startOptionsSelectGateObserved ||
                (sourceStartOptionsMenu.cameraIndex() ==
                     r3d::game::originaloptions::
                         StartOptionsMenuState::cameraSentinel &&
                 !sourceStartOptionsMenu.applyEnabled());
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
            const auto& profileNames = activeProfileNames();
            profileFrameObserved =
                profilePage.labels.size() ==
                    profileNames.size() + 1U &&
                sourceProfileFrame.scroll() <=
                    (profileNames.size() > 4U
                         ? profileNames.size() - 4U
                         : 0U);
            const float centerX =
                menu::virtualWidth * 0.5F;
            const auto visibleEnd = sourceProfileFrame.visibleEnd();
            for (std::size_t index =
                     sourceProfileFrame.visibleBegin();
                 index < visibleEnd; ++index)
            {
                const float rowY = sourceProfileFrame.rowY(
                    menu::virtualHeight, index);
                const bool itemFocused =
                    sourceProfileFrame.focus() ==
                        r3d::game::mainmenu2::ProfileFocus::Item &&
                    sourceProfileFrame.focusIndex() == index &&
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
                    sourceProfileFrame.focus() ==
                        r3d::game::mainmenu2::ProfileFocus::Close &&
                    sourceProfileFrame.focusIndex() == index &&
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
                sourceProfileFrame.canScrollUp();
            const bool canScrollDown =
                sourceProfileFrame.canScrollDown();
            auto drawProfileArrow =
                [&](bool up, float y, bool enabled) {
                    const bool focused =
                        enabled && !profileDeleteDialogVisible &&
                        sourceProfileFrame.focus() ==
                            (up
                                 ? r3d::game::mainmenu2::ProfileFocus::Up
                                 : r3d::game::mainmenu2::ProfileFocus::Down);
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
                true,
                sourceProfileFrame.upArrowY(menu::virtualHeight),
                canScrollUp);
            drawProfileArrow(
                false,
                sourceProfileFrame.downArrowY(menu::virtualHeight),
                canScrollDown);

            const auto backIndex =
                profilePage.labels.size() - 1U;
            const float backY =
                sourceProfileFrame.backY(menu::virtualHeight);
            const bool backFocused =
                sourceProfileFrame.focus() ==
                    r3d::game::mainmenu2::ProfileFocus::Back &&
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
                    sourceOptionsMenu.stateButtonY(
                        menu::virtualHeight, index);
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
            const auto optionsTab =
                r3d::game::originaloptions::OptionsMenuState::
                    tabForScreen(menuStack.back());
            if (menuStack.back() == MenuScreen::GameOptions)
            {
                names = &gameOptionNamesPage;
            }
            else if (
                menuStack.back() == MenuScreen::GraphicsOptions)
            {
                names = &graphicsOptionNamesPage;
            }
            else if (
                menuStack.back() == MenuScreen::SoundOptions)
            {
                names = &soundOptionNamesPage;
            }
            else
            {
                names = &controlsOptionsPage;
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
            const std::size_t rowCount =
                sourceOptionsMenu.rowCount(optionsTab);
            const std::size_t visibleRows = std::min(
                sourceOptionsMenu.visibleRowCount(optionsTab),
                rowCount);
            if (menuSelection < rowCount)
                sourceOptionsMenu.ensureVisible(
                    optionsTab, menuSelection);
            const std::size_t firstVisible =
                sourceOptionsMenu.scroll(optionsTab);
            const float firstRowY =
                sourceOptionsMenu.firstRowY(
                    optionsTab, menu::virtualHeight);

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
                            sourceOptionsMenu.controlsUseGamepad() ==
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

            syncSourceGamers();
            const auto gamersLayout = sourceGamersFrame.layout(
                menu::virtualWidth, menu::virtualHeight, 254.0F,
                static_cast<float>(garageArrowSelectedImage.width),
                static_cast<float>(
                    gamersNextArrowSelectedImage.width));
            const auto gamerPlanetIndex =
                sourceGamersFrame.selection();
            if (gamerPlanetIndex < originalGarage->gamers.size())
            {
                const auto& telemetry = device->renderTelemetry();
                const auto before = std::accumulate(
                    telemetry.drawCount.begin(),
                    telemetry.drawCount.end(), 0U);
                workshopRenderer.drawPlanet(
                    *device, raceShader,
                    originalGarage->gamers[gamerPlanetIndex],
                    gamersLayout.planetX, gamersLayout.planetY,
                    gamersLayout.viewportSize,
                    gamersLayout.viewportSize,
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

            const auto previous = sourceGamersFrame.previous();
            const auto next = sourceGamersFrame.next();
            if (previous)
            {
                const bool focused =
                    sourceGamersFrame.focus() ==
                    r3d::game::originalracemenu::GamerFocus::Left;
                const auto& image =
                    focused ? garageArrowSelectedImage
                            : garageArrowImage;
                drawQuadRotated(
                    *device, quad, shader,
                    focused ? garageArrowSelected : garageArrow,
                    static_cast<float>(image.width),
                    static_cast<float>(image.height),
                    gamersLayout.leftX, gamersLayout.planetY,
                    12.0F, 0.0F, transparent);
            }
            if (next)
            {
                const bool focused =
                    sourceGamersFrame.focus() ==
                    r3d::game::originalracemenu::GamerFocus::Right;
                const auto& image =
                    focused ? garageArrowSelectedImage
                            : garageArrowImage;
                drawQuadRotated(
                    *device, quad, shader,
                    focused ? garageArrowSelected : garageArrow,
                    static_cast<float>(image.width),
                    static_cast<float>(image.height),
                    gamersLayout.rightX, gamersLayout.planetY,
                    12.0F, bx::kPi, transparent);
            }
            const bool nextFocused =
                sourceGamersFrame.focus() ==
                r3d::game::originalracemenu::GamerFocus::Next;
            const auto& nextImage =
                nextFocused ? gamersNextArrowSelectedImage
                            : gamersNextArrowImage;
            drawQuad(
                *device, quad, shader,
                nextFocused ? gamersNextArrowSelected
                            : gamersNextArrow,
                static_cast<float>(nextImage.width),
                static_cast<float>(nextImage.height),
                gamersLayout.nextX, gamersLayout.nextY, 12.0F,
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
                        racePipeline, &visual.player.color);
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
                          profileState.player,
                          achievementOpened("armor4"))
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

            const float itemY = sourceRaceMainFrame.itemY(
                menu::virtualHeight,
                static_cast<float>(raceBottomPanelImage.height));
            for (std::size_t index = 0U;
                 index < raceMenuIcons.size(); ++index)
            {
                const bool selectedItem = index == menuSelection;
                const bool enabled =
                    sourceRaceMainFrame.enabled(index);
                const float itemX =
                    sourceRaceMainFrame.itemX(
                        menu::virtualWidth, index);
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
        else if (drawingOriginalGarage &&
                 sourceGarageFrame.selectedCar() != nullptr)
        {
            sourceGarageFrame.updateColors(sourceGarageColors());
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

            const auto& selectedEntry =
                *sourceGarageFrame.selectedCar();
            const auto& selectedCar = originalGarage->cars[
                selectedEntry.catalogIndex];
            const bool selectedLocked = selectedEntry.locked;
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

            const auto visibleRange = sourceGarageFrame.visibleRange(
                menu::virtualWidth,
                static_cast<float>(garageCarBoxImage.width));
            const auto& garageCars = sourceGarageFrame.cars();
            const std::size_t visibleEnd = std::min(
                visibleRange.first + visibleRange.count,
                garageCars.size());
            for (std::size_t view = visibleRange.first;
                 view < visibleEnd; ++view)
            {
                const auto& carView = garageCars[view];
                const bool selected =
                    view == sourceGarageFrame.selection();
                const float x =
                    visibleRange.firstCenterX +
                    static_cast<float>(view - visibleRange.first) *
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

            const bool leftArrowVisible =
                sourceGarageFrame.canPrevious();
            const bool rightArrowVisible =
                sourceGarageFrame.canNext();
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
                    if (!sourceGarageFrame.colorAvailable(colorIndex))
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
                              profileState.player,
                              achievementOpened("armor4"));
            const r3d::game::originalrace::
                OriginalWorkshopItem* previewItem = nullptr;
            r3d::game::originalrace::ProfileSlot previewProfileSlot;
            constexpr std::size_t firstGoodFocus =
                r3d::game::originalracemenu::WorkshopFrameState::
                    firstGoodFocus;
            constexpr std::size_t firstSlotFocus =
                r3d::game::originalracemenu::WorkshopFrameState::
                    firstSlotFocus;
            if (sourceWorkshopFrame.drag().active())
            {
                previewItem = originalGarage->findItem(
                    sourceWorkshopFrame.drag().item.record);
                previewProfileSlot = sourceWorkshopFrame.drag().item;
            }
            else if (
                menuSelection >= firstGoodFocus &&
                menuSelection < firstSlotFocus)
            {
                const auto* good = sourceWorkshopFrame.visibleGood(
                    menuSelection - firstGoodFocus);
                if (good != nullptr &&
                    good->catalogIndex <
                        originalGarage->workshop.size())
                {
                    previewItem = &originalGarage->workshop[
                        good->catalogIndex];
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
                            previewPlayer,
                            achievementOpened("armor4"));
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
                    sourceWorkshopFrame.scroll() * 3U + visible;
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
                const auto* good =
                    sourceWorkshopFrame.visibleGood(visible);
                if (good == nullptr ||
                    good->catalogIndex >=
                        originalGarage->workshop.size())
                    continue;
                const auto& item = originalGarage->workshop[
                    good->catalogIndex];
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
                    item,
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

            const auto workshopLayout = sourceWorkshopLayout();
            if (sourceWorkshopFrame.scroll() > 0U)
            {
                drawQuadRotated(
                    *device, quad, shader, garageArrow,
                    30.0F, 30.0F, workshopLayout.arrowX,
                    workshopLayout.upArrowY,
                    18.0F, bx::kPi * 0.5F, transparent);
            }
            if (sourceWorkshopFrame.scroll() <
                sourceWorkshopFrame.maximumScroll())
            {
                drawQuadRotated(
                    *device, quad, shader, garageArrow,
                    30.0F, 30.0F, workshopLayout.arrowX,
                    workshopLayout.downArrowY,
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
            const float backY = workshopLayout.backY;
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

            const auto& sourceWeaponDialog = sourceDialogs.Weapon();
            if (sourceWeaponDialog.visible &&
                !sourceWorkshopFrame.drag().active() &&
                sourceWorkshopFrame.confirmation().type ==
                    WorkshopConfirmation::None)
            {
                const float anchorX =
                    sourceWeaponDialog.center.x;
                const float anchorY =
                    sourceWeaponDialog.center.y;
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
                    anchorY + sourceWeaponDialog.titleOffset.y,
                    59.0F,
                    transparent);
                constexpr float infoLineStep = 18.0F;
                const float infoFirstY =
                    anchorY + sourceWeaponDialog.infoOffset.y -
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
                        anchorX + sourceWeaponDialog.infoOffset.x -
                            sourceWeaponDialog.infoSize.x * 0.5F +
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
                    anchorX + sourceWeaponDialog.moneyOffset.x,
                    anchorY + sourceWeaponDialog.moneyOffset.y,
                    59.0F, transparent);
                const auto& damage =
                    workshopWeaponDialog.damage;
                drawQuad(
                    *device, quad, shader,
                    damage.texture, damage.width, damage.height,
                    anchorX + sourceWeaponDialog.damageOffset.x,
                    anchorY + sourceWeaponDialog.damageOffset.y,
                    59.0F, transparent);
                raceWorkshopWeaponDialogObserved = true;
            }

            if (sourceWorkshopFrame.drag().active())
            {
                if (const auto* dragged =
                        originalGarage->findItem(
                            sourceWorkshopFrame.drag().item.record))
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
            const auto angarLayout = sourceAngarFrame.layout(
                menu::virtualWidth, menu::virtualHeight,
                static_cast<float>(angarBottomPanelImage.width),
                static_cast<float>(angarBottomPanelImage.height),
                static_cast<float>(angarPlanetInfoImage.width),
                static_cast<float>(angarPlanetInfoImage.height),
                static_cast<float>(garageBackImage.width));
            drawQuad(
                *device, quad, shader, angarBottomPanel,
                static_cast<float>(angarBottomPanelImage.width),
                static_cast<float>(angarBottomPanelImage.height),
                angarLayout.bottomPanelX,
                angarLayout.bottomPanelY, 60.0F, transparent);
            for (std::size_t index = 0U;
                 index < planetCount; ++index)
            {
                const float x = angarLayout.planetX(index);
                const bool selected =
                    static_cast<int>(index) ==
                    sourceAngarFrame.selection();
                const float animation =
                    selected
                        ? sourceSpaceshipFrame.sceneSeconds() *
                              bx::kPi / 24.0F
                        : 0.0F;
                workshopRenderer.drawPlanet(
                    *device, raceShader,
                    originalGarage->planets[index],
                    x, angarLayout.planetY, 180.0F, 180.0F,
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
                    x, angarLayout.slotY, 35.0F, transparent);

                const float alpha =
                    sourceAngarFrame.doorAlpha(index);
                const float offset = 16.0F * alpha;
                drawQuad(
                    *device, quad, shader, angarDoorDown,
                    static_cast<float>(angarDoorDownImage.width),
                    static_cast<float>(angarDoorDownImage.height),
                    x - 1.0F,
                    angarLayout.slotY -
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
                    angarLayout.slotY +
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
                        x, angarLayout.slotY, 15.0F, transparent);
                }
            }

            const std::size_t backIndex = planetCount;
            const bool backFocused =
                menuSelection == backIndex;
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
                angarLayout.backX, angarLayout.backY,
                35.0F, transparent);
            if (backIndex < planetsPage.normal.size())
            {
                const auto& backText =
                    backFocused
                        ? planetsPage.selected[backIndex]
                        : planetsPage.normal[backIndex];
                drawQuad(
                    *device, quad, shader, backText.texture,
                    backText.width, backText.height,
                    angarLayout.backX, angarLayout.backY,
                    15.0F, transparent);
            }

            if (sourceAngarFrame.selection() >= 0 &&
                static_cast<std::size_t>(
                    sourceAngarFrame.selection()) <
                    planetCount)
            {
                const auto index =
                    static_cast<std::size_t>(
                        sourceAngarFrame.selection());
                const float infoWidth =
                    static_cast<float>(angarPlanetInfoImage.width);
                const float infoHeight =
                    static_cast<float>(angarPlanetInfoImage.height);
                const float infoX = angarLayout.infoX;
                const float infoY = angarLayout.infoY;
                const float infoLeft = angarLayout.infoLeft;
                const float infoTop = angarLayout.infoTop;
                drawQuad(
                    *device, quad, shader, angarPlanetInfo,
                    infoWidth, infoHeight, infoX, infoY,
                    13.0F, transparent);
                drawQuad(
                    *device, quad, shader, angarClose,
                    static_cast<float>(angarCloseImage.width),
                    static_cast<float>(angarCloseImage.height),
                    angarLayout.closeX, angarLayout.closeY,
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
                    sourceSpaceshipFrame.sceneSeconds() *
                        bx::kPi * 0.5F,
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
            const float leftWidth =
                static_cast<float>(finishLeftFrameImage.width);
            const float leftHeight =
                static_cast<float>(finishLeftFrameImage.height);
            const auto finishEvents = sourceFinishFrame.progress(
                frameSeconds, menu::virtualWidth);
            for (const auto& event : finishEvents)
            {
                using FinishEventType =
                    r3d::game::originalracemenu::FinishEventType;
                if (event.type == FinishEventType::Last)
                    finishLastEventObserved = true;
#ifdef RRR3D_AUDIO
                std::uint32_t sourcePlace = 4U;
                if (event.type == FinishEventType::First)
                    sourcePlace = 1U;
                else if (event.type == FinishEventType::Second)
                    sourcePlace = 2U;
                else if (event.type == FinishEventType::Third)
                    sourcePlace = 3U;
                commentator.finishPlace(
                    *originalRace, event.racer, sourcePlace, audioError);
#endif
            }
            const auto finishLayout = sourceFinishFrame.layout(
                menu::virtualWidth, menu::virtualHeight,
                leftWidth, leftHeight);
            for (std::size_t index = 0U;
                 index < finishRows.size() &&
                 index < sourceFinishFrame.playerCount(); ++index)
            {
                const auto& rowState = sourceFinishFrame.row(index);
                if (!rowState.visible)
                    continue;
                const float offsetX = rowState.offsetX;
                const float rowTop = finishLayout.rowTop(index);
                const float rowCenterY =
                    finishLayout.rowCenterY(index);

                drawQuad(
                    *device, quad, shader, finishLeftFrame,
                    leftWidth, leftHeight,
                    offsetX + leftWidth * 0.5F,
                    rowCenterY, 55.0F, transparent);
                drawQuad(
                    *device, quad, shader, finishLineFrame,
                    finishLayout.lineWidth, leftHeight,
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
                    offsetX + finishLayout.leftLabelX,
                    rowTop + 63.0F,
                    20.0F, transparent);
                drawQuad(
                    *device, quad, shader, finishRewardTitle.texture,
                    finishRewardTitle.width,
                    finishRewardTitle.height,
                    offsetX + finishLayout.rightLabelX,
                    rowTop + 63.0F,
                    20.0F, transparent);
                drawQuad(
                    *device, quad, shader, finishPriceInfo.texture,
                    finishPriceInfo.width,
                    finishPriceInfo.height,
                    offsetX + finishLayout.leftLabelX,
                    rowTop + 154.0F,
                    20.0F, transparent);
                drawQuad(
                    *device, quad, shader, row.rewardValue.texture,
                    row.rewardValue.width, row.rewardValue.height,
                    offsetX + finishLayout.rightLabelX,
                    rowTop + 154.0F,
                    20.0F, transparent);
            }
            if (sourceFinishFrame.animationComplete())
            {
                finishMenuFrameObserved =
                    !finishRows.empty() &&
                    finishRows.size() <= 3U &&
                    std::all_of(
                        finishRows.begin(), finishRows.end(),
                        [](const FinishRowVisual& row) {
                            return valid(row.name.texture) &&
                                   valid(row.rewardValue.texture) &&
                                   valid(row.photo);
                        });
            }
        }
        else if (drawingOriginalAchievements)
        {
            raceAchievementFrameObserved = true;
            const auto achievementLayout =
                sourceAchievementFrame.layout(
                    menu::virtualWidth, menu::virtualHeight,
                    static_cast<float>(garageBackImage.width),
                    static_cast<float>(garageBackImage.height));
            const float scale = achievementLayout.scale;
            drawQuad(
                *device, quad, shader, achievementBackground,
                menu::virtualWidth, menu::virtualHeight,
                achievementLayout.centerX,
                achievementLayout.centerY, 85.0F, transparent);
            drawQuad(
                *device, quad, shader, achievementPanel,
                static_cast<float>(achievementPanelImage.width) *
                    scale,
                static_cast<float>(achievementPanelImage.height) *
                    scale,
                achievementLayout.centerX,
                static_cast<float>(achievementPanelImage.height) *
                    scale * 0.5F,
                60.0F, transparent);

            drawQuad(
                *device, quad, shader, achievementBottomPanel,
                menu::virtualWidth,
                static_cast<float>(
                    achievementBottomPanelImage.height),
                achievementLayout.centerX,
                achievementLayout.bottomPanelY -
                    static_cast<float>(
                        achievementBottomPanelImage.height) *
                        0.5F,
                60.0F, transparent);

            drawQuad(
                *device, quad, shader, achievementRewards.texture,
                achievementRewards.width * scale,
                achievementRewards.height * scale,
                achievementLayout.centerX,
                achievementLayout.rewardsY, 15.0F, transparent);
            drawQuad(
                *device, quad, shader, achievementPoints.texture,
                achievementPoints.width, achievementPoints.height,
                achievementLayout.centerX,
                achievementLayout.pointsY,
                15.0F, transparent);

            for (std::size_t index = 0U;
                 index < originalAchievementVisuals.size(); ++index)
            {
                const auto state = achievementState(index);
                const bool opened = state ==
                    r3d::game::originalracemenu::
                        AchievementState::Opened;
                const bool unlocked = state ==
                    r3d::game::originalracemenu::
                        AchievementState::Unlocked;
                const auto& image =
                    opened ? achievementOpenedImages[index]
                           : achievementLockedImages[index];
                const Texture imageTexture =
                    opened ? achievementOpenedTextures[index]
                           : achievementLockedTextures[index];
                const float imageX =
                    achievementLayout.cardX(index);
                const float imageY =
                    achievementLayout.cardY(index);
                const float imageWidth =
                    static_cast<float>(image.width) * scale;
                const float imageHeight =
                    static_cast<float>(image.height) * scale;
                drawQuad(
                    *device, quad, shader, imageTexture,
                    imageWidth, imageHeight, imageX, imageY,
                    45.0F, transparent);

                const bool focused =
                    sourceAchievementFrame.focus() == index && unlocked;
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
                sourceAchievementFrame.focus() ==
                originalAchievementBack;
            drawQuad(
                *device, quad, shader,
                backFocused ? garageBackSelected : garageBack,
                static_cast<float>(
                    backFocused ? garageBackSelectedImage.width
                                : garageBackImage.width),
                static_cast<float>(
                    backFocused ? garageBackSelectedImage.height
                                : garageBackImage.height),
                achievementLayout.backX,
                achievementLayout.backY, 35.0F, transparent);
            const auto& backText =
                backFocused
                    ? achievementsPage.selected[
                          originalAchievementBack]
                    : achievementsPage.normal[
                          originalAchievementBack];
            drawQuad(
                *device, quad, shader, backText.texture,
                backText.width, backText.height,
                achievementLayout.backX,
                achievementLayout.backY,
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
                    !activeProfileNames().empty();
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
        if (sourceDialogs.Info().visible)
        {
            drawQuad(
                *device, quad, shader, infoDialogFrame,
                static_cast<float>(infoDialogFrameImage.width),
                static_cast<float>(infoDialogFrameImage.height),
                sourceDialogs.Info().center.x,
                sourceDialogs.Info().center.y, 6.0F,
                transparent);
            drawQuad(
                *device, quad, shader, infoDialog.title.texture,
                infoDialog.title.width, infoDialog.title.height,
                sourceDialogs.Info().center.x +
                    sourceDialogs.Info().titleOffset.x,
                sourceDialogs.Info().center.y +
                    sourceDialogs.Info().titleOffset.y,
                4.0F,
                transparent);
            constexpr float infoLineStep = 27.0F;
            const float firstLineY =
                sourceDialogs.Info().center.y +
                sourceDialogs.Info().infoOffset.y -
                static_cast<float>(infoDialog.info.size() - 1U) *
                    infoLineStep * 0.5F;
            for (std::size_t line = 0U;
                 line < infoDialog.info.size(); ++line)
            {
                const auto& text = infoDialog.info[line];
                drawQuad(
                    *device, quad, shader, text.texture,
                    text.width, text.height,
                    sourceDialogs.Info().center.x -
                        sourceDialogs.Info().infoSize.x * 0.5F +
                        text.width * 0.5F,
                    firstLineY +
                        static_cast<float>(line) *
                            infoLineStep,
                    4.0F, transparent);
            }
            if (sourceDialogs.Info().dismissable)
            {
                drawQuad(
                    *device, quad, shader,
                    infoDialogButtonSelected,
                    static_cast<float>(
                        infoDialogButtonSelectedImage.width),
                    static_cast<float>(
                        infoDialogButtonSelectedImage.height),
                    sourceDialogs.Info().center.x,
                    sourceDialogs.Info().center.y +
                        sourceDialogs.Info().okOffset.y,
                    3.0F,
                    transparent);
                drawQuad(
                    *device, quad, shader, infoDialog.ok.texture,
                    infoDialog.ok.width, infoDialog.ok.height,
                    sourceDialogs.Info().center.x,
                    sourceDialogs.Info().center.y +
                        sourceDialogs.Info().okOffset.y,
                    2.0F,
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
#ifdef RRR3D_PHYSICS
        drawOriginalCursor(!sourceDialogs.Info().visible);
#else
        drawOriginalCursor(true);
#endif
        device->endFrame();
#ifdef RRR3D_PHYSICS
        }
#endif

        ++renderedFrames;
        if (options->smokeFrames != 0 &&
            renderedFrames >= options->smokeFrames
            && (!options->finalMenuSmokeTest ||
                finalAutoCloseObserved)
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
#ifdef RRR3D_AUDIO
                        << " (loaded=" << finalMusic.loadedTrackCount()
                        << ", decoding="
                        << finalMusic.backgroundDecodeActive()
                        << ", paused=" << finalMusic.paused()
                        << ", voice="
                        << finalMusic.currentVoiceActive()
                        << ", position="
                        << finalMusic.currentPositionFrames() << ')'
#endif
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
                const auto smokeEnvironmentPolicy =
                    r3d::game::originalrace::source::Environment::
                        ApplyQuality(
                            originalRace->environment, smokeQuality,
                            false);
                const bool expectsTrueReflections =
                    smokeEnvironmentPolicy.trueReflections;
                const bool expectsShadows =
                    smokeEnvironmentPolicy.shadows;
                const bool expectsBloom =
                    smokeEnvironmentPolicy.bloom;
                const bool expectsRefraction =
                    smokeEnvironmentPolicy.refraction;
                const bool expectsHdr =
                    smokeEnvironmentPolicy.hdr;
                const bool expectsSunShaft =
                    smokeEnvironmentPolicy.sunShaft;
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
                    smokeEnvironmentPolicy.environmentReflection;
                const bool expectsWater =
                    smokeEnvironmentPolicy.highQualityWater;
                const bool expectsVolumeSurface =
                    smokeEnvironmentPolicy.volumeFog;
                const bool expectsBumpMapping =
                    smokeEnvironmentPolicy.bumpMapping &&
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
                std::size_t competitiveAiCount = 0U;
                std::size_t progressingAiCount = 0U;
                std::size_t sourceAiCount = 0U;
                for (std::size_t racer = 0U;
                     racer < raceSession.racers().size(); ++racer)
                {
                    if (!raceSession.racers()[racer].IsComputer())
                        continue;
                    ++sourceAiCount;
                    if (racer < maximumRaceAiSpeeds.size() &&
                        maximumRaceAiSpeeds[racer] >= 25.0F)
                        ++competitiveAiCount;
                    if (racer < maximumRaceAiProgress.size() &&
                        maximumRaceAiProgress[racer] >= 0.5F)
                        ++progressingAiCount;
                }
                const std::size_t expectedCompetitiveAi =
                    options->smokeFrames >= 1800U &&
                            originalRace->levelPath ==
                                "Data/Map/World1/map1.r3dMap" &&
                            sourceAiCount > 0U
                        ? std::min<std::size_t>(
                              3U, sourceAiCount)
                        : 0U;
                const auto humanRacer = raceSession.humanRacer();
                const std::size_t aheadAiCount =
                    humanRacer >= raceSession.racers().size()
                        ? 0U
                        : static_cast<std::size_t>(std::count_if(
                              raceSession.racers().begin(),
                              raceSession.racers().end(),
                              [&](const auto& racer) {
                                  return !racer.IsHuman() &&
                                         !racer.disconnected &&
                                         racer.GetPlace() <
                                             raceSession.racers()[humanRacer]
                                                 .GetPlace();
                              }));
                const std::size_t expectedAheadAi =
                    expectedCompetitiveAi > 0U ? 1U : 0U;
                const auto expectedHeadlightCount =
                    static_cast<std::uint32_t>(
                        std::accumulate(
                            raceSession.racers().begin(),
                            raceSession.racers().end(), 0U,
                            [](std::uint32_t count,
                               const auto& racer) {
                                using Player =
                                    r3d::game::originalrace::source::Player;
                                if (!racer.HasAttachedLights())
                                    return count;
                                return count +
                                    (racer.GetHeadLight() ==
                                             Player::HeadLightMode::Two
                                         ? 2U
                                         : racer.GetHeadLight() ==
                                                   Player::HeadLightMode::One
                                               ? 1U
                                               : 0U);
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
                    !raceLoopTeardownObserved ||
                    !gameMusicDeferredSelectionObserved ||
                    !gameMusicZeroStartObserved ||
#endif
                    !racePauseDialogObserved ||
                    !racePauseResumeObserved ||
                    !racePauseFrozenObserved ||
                    !raceEffectsMuteObserved ||
                    !raceChatInputObserved ||
                    !raceChatLineObserved ||
                    raceChatSmokeStep != 4U ||
                    racePlayerDestroyedObserved ||
                    minimumRacePlayerLife <= 0.0F ||
                    maximumRaceSmokeContacts == 0 ||
                    maximumRaceSmokeSpeed < 0.2F ||
                    originalResourceManager.GetCacheHitCount() == 0U ||
                    competitiveAiCount < expectedCompetitiveAi ||
                    progressingAiCount < expectedCompetitiveAi ||
                    aheadAiCount < expectedAheadAi ||
                    raceVehicles.size() < 2U ||
                    !raceCameraStylesObserved[0] ||
                    !raceCameraStylesObserved[1] ||
                    (options->legacyWindowsDebug &&
                     !std::all_of(
                         legacyDebugCameraStylesObserved.begin(),
                         legacyDebugCameraStylesObserved.end(),
                         [](bool observed) { return observed; })) ||
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
                        << ", raceLoopTeardown="
                        << raceLoopTeardownObserved
                        << ", gameMusicSourceLifecycle="
                        << gameMusicDeferredSelectionObserved << '/'
                        << gameMusicZeroStartObserved
#endif
                        << ", pause="
                        << racePauseDialogObserved << '/'
                        << racePauseResumeObserved << '/'
                        << racePauseFrozenObserved << '/'
                        << raceEffectsMuteObserved << ", chat="
                        << raceChatInputObserved << '/'
                        << raceChatLineObserved << '/'
                        << raceChatSmokeStep << ", destroyed="
                        << racePlayerDestroyedObserved << ", minLife="
                        << minimumRacePlayerLife << ", contacts="
                        << maximumRaceSmokeContacts << ", maxSpeed="
                        << maximumRaceSmokeSpeed
                        << ", resourceCache="
                        << originalResourceManager.GetCacheHitCount() << '/'
                        << originalResourceManager.GetRequestCount()
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
                        << raceCameraStylesObserved[1]
                        << ", legacyDebugCameras=";
                    for (const bool observed :
                         legacyDebugCameraStylesObserved)
                        std::cerr << observed;
                    std::cerr << '\n';
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
                        << (options->legacyWindowsDebug
                                ? ", Windows _DEBUG cameras=5"
                                : "")
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
#ifdef RRR3D_AUDIO
                           "authoritative SoundMotor/wheel-loop teardown and "
                           "source MusicCat deferred zero-frame start, "
#endif
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
                    || !finishLastEventObserved ||
                    !finishMenuAudioHeldObserved ||
                    !finishMenuAudioCloseObserved ||
                    !finishMenuMusicFadeObserved
#endif
                )
                {
                    std::cerr
                        << "Source FinishMenu renderer smoke failed: "
                        << "observed=" << finishMenuFrameObserved
                        << ", rows=" << finishRows.size()
#ifdef RRR3D_AUDIO
                        << ", lastVoice="
                        << finishLastEventObserved
                        << ", audio="
                        << finishMenuAudioHeldObserved << '/'
                        << finishMenuAudioCloseObserved << '/'
                        << finishMenuMusicFadeObserved
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
                           "global commentator queue, last-place event and "
                           "OnFinishFrameClose commentator/menu-music fade "
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

#ifdef RRR3D_PHYSICS
    // MainMenu2 Exit -> Menu::Terminate -> GameMode::Terminate persists only
    // user.xml.  Race/profile/achievement writes occur at their explicit
    // source menu/race boundaries, never as a side effect of process exit.
    // Automated runs write this same boundary into their temporary store.
    saveGameConfig();
#endif
#ifdef RRR3D_AUDIO
#ifdef RRR3D_PHYSICS
    stopRaceAudio();
    commentator.shutdown();
    gameMusic.shutdown();
#endif
    finalMusic.shutdown();
    music.shutdown();
    menuSounds.shutdown();
    originalResourceManager.ShutdownSounds();
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
    if (userChat.inputVisible())
        SDL_StopTextInput(window);
    for (auto& line : gameDebugVisual)
        if (valid(line.texture))
            device->destroy(line.texture);
    gameDebugVisual.clear();
    destroyUserChatVisual(*device, userChatVisual);
    physicsWorld.reset();
    raceHud.shutdown(*device);
    workshopRenderer.shutdown(*device);
    angarRenderer.shutdown(*device);
    garageRenderer.shutdown(*device);
    raceRenderer.shutdown(*device);
    originalResourceManager.Shutdown();
    if (!sourceNormalInteractiveLaunch)
    {
        std::error_code cleanupError;
        std::filesystem::remove_all(
            runtimeProfileDirectory, cleanupError);
    }
#endif
    releaseResources();
    device.reset();
    SDL_ShowCursor();
    SDL_DestroyWindow(window);
#ifdef RRR3D_GAMEPAD_INPUT
    input.shutdown();
#endif
    SDL_Quit();
    return runtimeSmokeFailed ? EXIT_FAILURE : EXIT_SUCCESS;
}
