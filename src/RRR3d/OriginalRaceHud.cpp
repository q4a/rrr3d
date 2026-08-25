#include "OriginalRaceHud.h"

#include "CoreTextRasterizer.h"
#include "resource/R3DMeshAsset.h"
#include "resource/ResourceFileSystem.h"

#include <bx/math.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <list>
#include <limits>
#include <numeric>
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

bool valid(Mesh value) noexcept
{
    return value.vertices.value != invalid_resource &&
           value.indices.value != invalid_resource;
}

Transform transform(float width, float height, float centerX,
                    float centerY, float depth, float angle = 0.0F)
{
    Transform result;
    bx::mtxSRT(result.matrix.data(), width, height, 1.0F,
               0.0F, 0.0F, angle, centerX, centerY, depth);
    return result;
}

void drawAsset(GraphicsDevice& device, Mesh quad, Shader shader,
               Texture texture, float width, float height, float centerX,
               float centerY, float depth, const PipelineState& pipeline,
               float angle = 0.0F)
{
    if (valid(texture) && width > 0.0F && height > 0.0F)
    {
        MaterialState material;
        material.emissive = 1.0F;
        material.specular = 0.0F;
        device.draw(
            quad, shader, texture,
            transform(width, height, centerX, centerY, depth, angle),
            pipeline, {}, material);
    }
}

void drawTintedAsset(
    GraphicsDevice& device, Mesh quad, Shader shader, Texture texture,
    float width, float height, float centerX, float centerY, float depth,
    const PipelineState& pipeline, const std::array<float, 4>& color,
    float angle = 0.0F)
{
    if (!valid(texture) || width <= 0.0F || height <= 0.0F)
        return;
    MaterialState material;
    material.color = color;
    material.emissive = 1.0F;
    material.specular = 0.0F;
    device.draw(quad, shader, texture,
                transform(width, height, centerX, centerY, depth, angle),
                pipeline, {}, material);
}

std::string decodeUtf16Le(std::string_view bytes)
{
    if (bytes.size() < 2U ||
        static_cast<unsigned char>(bytes[0]) != 0xffU ||
        static_cast<unsigned char>(bytes[1]) != 0xfeU)
        return std::string(bytes);
    std::string result;
    for (std::size_t index = 2; index + 1U < bytes.size(); index += 2U)
    {
        const auto value = static_cast<std::uint16_t>(
            static_cast<unsigned char>(bytes[index]) |
            (static_cast<unsigned char>(bytes[index + 1U]) << 8U));
        if (value < 0x80U)
        {
            result.push_back(static_cast<char>(value));
        }
        else if (value < 0x800U)
        {
            result.push_back(static_cast<char>(0xc0U | (value >> 6U)));
            result.push_back(
                static_cast<char>(0x80U | (value & 0x3fU)));
        }
        else
        {
            result.push_back(static_cast<char>(0xe0U | (value >> 12U)));
            result.push_back(static_cast<char>(
                0x80U | ((value >> 6U) & 0x3fU)));
            result.push_back(
                static_cast<char>(0x80U | (value & 0x3fU)));
        }
    }
    return result;
}

std::string localizedValue(std::string_view text,
                           std::string_view key)
{
    const std::string prefix = std::string(key) + " \"";
    std::size_t line = 0;
    while (line < text.size())
    {
        const auto end = text.find_first_of("\r\n", line);
        const auto length =
            (end == std::string_view::npos ? text.size() : end) - line;
        const auto value = text.substr(line, length);
        if (value.rfind(prefix, 0) == 0 && value.size() > prefix.size() &&
            value.back() == '"')
        {
            std::string result(
                value.substr(prefix.size(),
                             value.size() - prefix.size() - 1U));
            std::size_t escapedNewline = 0;
            while ((escapedNewline =
                        result.find("\\n", escapedNewline)) !=
                   std::string::npos)
                result.replace(escapedNewline, 2, 1, '\n');
            return result;
        }
        if (end == std::string_view::npos)
            break;
        line = end + 1U;
        while (line < text.size() &&
               (text[line] == '\r' || text[line] == '\n'))
            ++line;
    }
    return {};
}

std::string formatNamePlace(std::string format, std::uint32_t place,
                            std::string_view name)
{
    if (const auto at = format.find("%d"); at != std::string::npos)
        format.replace(at, 2, std::to_string(place));
    if (const auto at = format.find("%s"); at != std::string::npos)
        format.replace(at, 2, name);
    return format;
}

Transform identityTransform()
{
    Transform result;
    bx::mtxIdentity(result.matrix.data());
    return result;
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
    const originalrace::Race& race, std::string_view language,
    std::string_view difficulty, bool campaign, std::string& error)
{
    campaign_ = campaign;
    // These are the same assets created by PlayerStateFrame and
    // MiniMapFrame in the Windows HUD.
    if (!loadImage(device, resources, "Data/GUI/placeMineHyper.png",
                   placeFrame_, error) ||
        !loadImage(device, resources, "Data/GUI/lifeBarBack.png",
                   lifeBack_, error) ||
        !loadImage(device, resources, "Data/GUI/lifeBar.png",
                   lifeBar_, error) ||
        !loadImage(device, resources, "Data/GUI/lap.png", lapBack_,
                   error) ||
        !loadImage(device, resources, "Data/GUI/mapFrame.png",
                   mapStrip_, error) ||
        !loadImage(device, resources, "Data/GUI/playerPoint2.png",
                   mapPlayer_, error) ||
        !loadImage(device, resources, "Data/GUI/playerPoint.png",
                   mapOpponent_, error) ||
        !loadImage(device, resources, "Data/GUI/start.png",
                   mapStart_, error) ||
        !loadImage(device, resources, "Data/GUI/slot.png",
                   weaponSlot_, error) ||
        !loadImage(device, resources, "Data/GUI/slotSel.png",
                   weaponSlotSelected_, error) ||
        !loadImage(device, resources, "Data/GUI/mineSlot.png",
                   mineSlot_, error) ||
        !loadImage(device, resources, "Data/GUI/hyperSlot.png",
                   hyperSlot_, error) ||
        !loadImage(device, resources, "Data/GUI/pickArmor.png",
                   pickArmor_, error) ||
        !loadImage(device, resources, "Data/GUI/pickWeapon.png",
                   pickAmmo_, error) ||
        !loadImage(device, resources, "Data/GUI/pickIntro.png",
                   pickIntro_, error) ||
        !loadImage(device, resources, "Data/GUI/pickMine.png",
                   pickMine_, error) ||
        !loadImage(device, resources, "Data/GUI/pickMoney.png",
                   pickMoney_, error) ||
        !loadImage(device, resources, "Data/GUI/pickImmortal.png",
                   pickShield_, error) ||
        !loadImage(device, resources, "Data/GUI/playerKill.png",
                   playerKill_, error) ||
        !loadImage(device, resources, "Data/GUI/carLifeBack.png",
                   opponentLifeBack_, error) ||
        !loadImage(device, resources, "Data/GUI/carLifeBar.png",
                   opponentLifeBar_, error) ||
        !loadImage(device, resources, "Data/GUI/playerLeftFrame.png",
                   finishLeftFrame_, error) ||
        !loadImage(device, resources, "Data/GUI/playerRightFrame.png",
                   finishRightFrame_, error) ||
        !loadImage(device, resources, "Data/GUI/playerLineFrame.png",
                   finishLineFrame_, error))
    {
        shutdown(device);
        return false;
    }
    for (std::size_t index = 0; index < countdownImages_.size();
         ++index)
    {
        if (!loadImage(device, resources,
                       "Data/GUI/tablo" + std::to_string(index) + ".png",
                       countdownImages_[index], error))
        {
            shutdown(device);
            return false;
        }
    }
    for (std::size_t index = 0; index < finishCups_.size(); ++index)
    {
        if (!loadImage(device, resources,
                       "Data/GUI/cup" + std::to_string(index + 1U) +
                           ".dds",
                       finishCups_[index], error))
        {
            shutdown(device);
            return false;
        }
    }
    racerPhotos_.resize(race.racers.size());
    for (std::size_t index = 0; index < race.racers.size(); ++index)
    {
        const std::string photo =
            race.racers[index].photoPath.empty()
                ? "Data/GUI/Chars/tyler.png"
                : race.racers[index].photoPath;
        if (!loadImage(device, resources, photo, racerPhotos_[index],
                       error))
        {
            shutdown(device);
            return false;
        }
    }
    for (const auto& identity : race.playerIdentities)
    {
        // Player ids are reused by every planet. Cache only the entry that
        // Tournament::GetPlayerData would resolve for this active race.
        if (originalrace::findOriginalPlayerIdentity(race, identity.id) !=
            &identity)
            continue;
        if (identity.photoPath.empty() ||
            gamerPhotos_.contains(identity.id))
            continue;
        ImageAsset photo;
        if (!loadImage(device, resources, identity.photoPath, photo, error))
        {
            shutdown(device);
            return false;
        }
        gamerPhotos_.emplace(identity.id, std::move(photo));
    }
    achievementImages_.resize(race.achievements.size());
    achievementPointsImages_.resize(race.achievements.size());
    for (std::size_t index = 0;
         index < race.achievements.size(); ++index)
    {
        const auto& achievement = race.achievements[index];
        if (!loadImage(
                device, resources,
                "Data/GUI/Achievments/" + achievement.name + ".png",
                achievementImages_[index], error) ||
            !loadImage(
                device, resources,
                "Data/GUI/Achievments/points" +
                    std::to_string(achievement.reward) + ".png",
                achievementPointsImages_[index], error))
        {
            shutdown(device);
            return false;
        }
    }
    if (difficulty != "gdEasy")
    {
        const std::string multiplier =
            difficulty == "gdHard" ? "points1_5.png"
                                    : "points1_2.png";
        if (!loadImage(
                device, resources,
                "Data/GUI/Achievments/" + multiplier,
                achievementMultiplierImage_, error))
        {
            shutdown(device);
            return false;
        }
    }
    try
    {
        weaponVisuals_.resize(race.weapons.size());
        for (std::size_t index = 0; index < race.weapons.size(); ++index)
        {
            const auto& source = race.weapons[index].visual;
            auto& visual = weaponVisuals_[index];
            const auto mesh = r3d::resource::loadR3DMeshAsset(
                resources, source.meshPath);
            const std::array<float, 3> center{
                (mesh.minimum[0] + mesh.maximum[0]) * 0.5F,
                (mesh.minimum[1] + mesh.maximum[1]) * 0.5F,
                (mesh.minimum[2] + mesh.maximum[2]) * 0.5F};
            const float extent = std::max(
                {mesh.maximum[0] - mesh.minimum[0],
                 mesh.maximum[1] - mesh.minimum[1],
                 mesh.maximum[2] - mesh.minimum[2], 0.001F});
            const float scale = 48.0F / extent;
            std::vector<StaticMeshVertex> vertices;
            vertices.reserve(mesh.vertices.size());
            for (const auto& vertex : mesh.vertices)
            {
                vertices.push_back(
                    {(vertex.position[0] - center[0]) * scale,
                     (vertex.position[1] - center[1]) * scale,
                     (vertex.position[2] - center[2]) * scale,
                     vertex.normal[0], vertex.normal[1],
                     vertex.normal[2], vertex.texcoord[0],
                     vertex.texcoord[1]});
            }
            visual.mesh = device.createMesh(
                vertices.data(), vertices.size(), mesh.indices.data(),
                mesh.indices.size());
            visual.materials = source.materials;
            for (const auto& material : source.materials)
            {
                const auto bytes =
                    resources.readBinary(material.texturePath);
                visual.textures.push_back(
                    device.createTextureContainer(
                        bytes.data(), bytes.size(),
                        material.texturePath));
            }
            for (const auto& group : mesh.materialGroups)
            {
                visual.groups.push_back(
                    {group.firstIndex, group.indexCount});
            }
            if (!valid(visual.mesh) || visual.textures.empty() ||
                std::any_of(
                    visual.textures.begin(), visual.textures.end(),
                    [](Texture texture) { return !valid(texture); }))
            {
                throw r3d::resource::ResourceError(
                    "Unable to upload HUD weapon " +
                    source.meshPath);
            }
        }
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        shutdown(device);
        return false;
    }
    try
    {
        std::string localizationPath =
            "Data/" + std::string(language) + ".txt";
        if (!resources.exists(localizationPath))
            localizationPath = "Data/english.txt";
        const auto localization =
            decodeUtf16Le(resources.readText(localizationPath));
        if (const auto value = localizedValue(localization, "svLap");
            !value.empty())
            lapName_ = value;
        if (const auto value =
                localizedValue(localization, "svNamePlaceMarker");
            !value.empty())
            namePlaceFormat_ = value;
        if (const auto value = localizedValue(localization, "svPrice");
            !value.empty())
            priceName_ = value;
        if (const auto value = localizedValue(localization, "svMoney");
            !value.empty())
            moneyName_ = value;
        if (const auto value = localizedValue(localization, "svPoints");
            !value.empty())
            pointsName_ = value;
        for (std::size_t index = 0; index < placeNames_.size(); ++index)
        {
            const auto value = localizedValue(
                localization, "svPlace" + std::to_string(index + 1U));
            if (!value.empty())
                placeNames_[index] = value;
        }
        localizedRacerNames_.reserve(race.racers.size());
        for (const auto& racer : race.racers)
        {
            auto name = localizedValue(localization, racer.name);
            localizedRacerNames_.push_back(
                name.empty() ? racer.name : std::move(name));
        }
        for (const auto& identity : race.playerIdentities)
        {
            if (originalrace::findOriginalPlayerIdentity(
                    race, identity.id) != &identity)
                continue;
            if (localizedGamerNames_.contains(identity.id))
                continue;
            auto name = localizedValue(localization, identity.name);
            localizedGamerNames_.emplace(
                identity.id,
                name.empty() ? identity.name : std::move(name));
        }
    }
    catch (const std::exception&)
    {
        // The original English strings above are the fallback locale.
    }
    buildMiniMap(device, race);
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
    for (auto& visual : weaponVisuals_)
    {
        for (const auto texture : visual.textures)
            if (valid(texture))
                device.destroy(texture);
        if (valid(visual.mesh))
            device.destroy(visual.mesh);
        visual = {};
    }
    weaponVisuals_.clear();
    for (auto& opponent : opponentLabels_)
        releaseText(opponent.name);
    for (auto& row : finishRows_)
    {
        releaseText(row.name);
        releaseText(row.value);
    }
    releaseText(finishMoneyPoints_);
    releaseText(finishPrice_);
    releaseText(hyperAmmo_);
    releaseText(mineAmmo_);
    for (auto& label : weaponAmmo_)
        releaseText(label);
    releaseText(lap_);
    releaseText(place_);
    for (auto& notification : notifications_)
        releaseText(notification.label);
    releaseImage(playerKill_);
    releaseImage(pickShield_);
    releaseImage(opponentLifeBar_);
    releaseImage(opponentLifeBack_);
    for (auto& image : countdownImages_)
        releaseImage(image);
    releaseImage(pickMoney_);
    releaseImage(pickMine_);
    releaseImage(pickIntro_);
    releaseImage(pickAmmo_);
    releaseImage(pickArmor_);
    releaseImage(hyperSlot_);
    releaseImage(mineSlot_);
    releaseImage(weaponSlotSelected_);
    releaseImage(weaponSlot_);
    releaseImage(mapStart_);
    releaseImage(mapOpponent_);
    releaseImage(mapPlayer_);
    releaseImage(mapStrip_);
    releaseImage(lapBack_);
    releaseImage(lifeBar_);
    releaseImage(lifeBack_);
    releaseImage(placeFrame_);
    releaseImage(achievementMultiplierImage_);
    for (auto& image : achievementPointsImages_)
        releaseImage(image);
    achievementPointsImages_.clear();
    for (auto& image : achievementImages_)
        releaseImage(image);
    achievementImages_.clear();
    for (auto& photo : racerPhotos_)
        releaseImage(photo);
    racerPhotos_.clear();
    for (auto& [gamerId, photo] : gamerPhotos_)
    {
        (void)gamerId;
        releaseImage(photo);
    }
    gamerPhotos_.clear();
    for (auto& cup : finishCups_)
        releaseImage(cup);
    releaseImage(finishLineFrame_);
    releaseImage(finishRightFrame_);
    releaseImage(finishLeftFrame_);
    if (valid(mapMesh_))
        device.destroy(mapMesh_);
    mapMesh_ = {};
    mapMarkers_.clear();
    opponentLabels_.clear();
    carLifeOverlays_ = {};
    notifications_.clear();
    achievementNotifications_.clear();
    localizedRacerNames_.clear();
    localizedGamerNames_.clear();
    finishRows_ = {};
    uiSeconds_ = 0.0F;
    finishStarted_ = -1.0F;
    finishVisible_ = false;
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

std::string OriginalRaceHud::racerName(
    const originalrace::Race& race,
    const originalrace::OriginalRaceSession& session,
    std::size_t racer) const
{
    if (racer < session.racers().size())
    {
        const auto& player = session.racers()[racer];
        // Player::GetName gives NetPlayer::_netName priority. A service name
        // is already display text and must not be replaced by the localized
        // tournament token cached when HUD resources were initialized.
        if (!player.GetNetName().empty())
            return player.GetNetName();
        const auto gamerName = localizedGamerNames_.find(
            player.GetGamerId());
        if (gamerName != localizedGamerNames_.end())
            return gamerName->second;
        if (racer < race.racers.size() &&
            player.GetName() != race.racers[racer].name)
            return player.GetName();
    }
    if (racer < localizedRacerNames_.size())
        return localizedRacerNames_[racer];
    if (racer < session.racers().size())
        return session.racers()[racer].GetName();
    return racer < race.racers.size() ? race.racers[racer].name
                                      : std::string{};
}

const OriginalRaceHud::ImageAsset* OriginalRaceHud::racerPhoto(
    int gamerId, std::size_t racer) const noexcept
{
    const auto found = gamerPhotos_.find(gamerId);
    if (found != gamerPhotos_.end())
        return &found->second;
    return racer < racerPhotos_.size() ? &racerPhotos_[racer] : nullptr;
}

void OriginalRaceHud::buildMiniMap(GraphicsDevice& device,
                                   const originalrace::Race& race)
{
    if (valid(mapMesh_))
        device.destroy(mapMesh_);
    mapMesh_ = {};
    if (race.tracePath.size() < 2 || race.tracePoints.empty())
        return;

    auto tracePoint =
        [&](std::uint32_t id) -> const originalrace::TracePoint* {
        const auto found = std::find_if(
            race.tracePoints.begin(), race.tracePoints.end(),
            [id](const auto& candidate) { return candidate.id == id; });
        return found == race.tracePoints.end() ? nullptr : &*found;
    };

    using Vec2 = std::array<float, 2>;
    auto add = [](Vec2 first, Vec2 second) {
        return Vec2{first[0] + second[0], first[1] + second[1]};
    };
    auto subtract = [](Vec2 first, Vec2 second) {
        return Vec2{first[0] - second[0], first[1] - second[1]};
    };
    auto multiply = [](Vec2 value, float scale) {
        return Vec2{value[0] * scale, value[1] * scale};
    };
    auto dot = [](Vec2 first, Vec2 second) {
        return first[0] * second[0] + first[1] * second[1];
    };
    auto normalize = [](Vec2 value) {
        const float length =
            std::sqrt(value[0] * value[0] + value[1] * value[1]);
        return length > 0.00001F
                   ? Vec2{value[0] / length, value[1] / length}
                   : Vec2{1.0F, 0.0F};
    };
    auto normalCcw = [](Vec2 value) {
        return Vec2{-value[1], value[0]};
    };
    auto normalCw = [](Vec2 value) {
        return Vec2{value[1], -value[0]};
    };

    struct Node
    {
        Vec2 position{};
        float size = 0.0F;
        Vec2 direction{};
        Vec2 previousDirection{};
        Vec2 middleDirection{};
        Vec2 middleNormal{};
        Vec2 edgeNormal{};
        float cosineDelta = 1.0F;
        float sineHalfAngle = 1.0F;
        float radius = 0.0F;
        bool counterClockwise = false;
    };
    using Nodes = std::list<Node>;

    auto computeNode = [&](Nodes& nodes, Nodes::iterator iterator) {
        auto next = std::next(iterator);
        auto previous = iterator;
        if (iterator != nodes.begin())
            --previous;
        else
            previous = nodes.end();

        iterator->direction =
            next != nodes.end()
                ? normalize(subtract(next->position,
                                     iterator->position))
                : normalize(subtract(iterator->position,
                                     previous->position));
        iterator->previousDirection =
            previous != nodes.end()
                ? normalize(subtract(iterator->position,
                                     previous->position))
                : iterator->direction;
        iterator->middleDirection = normalize(
            add(iterator->previousDirection, iterator->direction));
        iterator->middleNormal =
            normalCcw(iterator->middleDirection);
        iterator->cosineDelta =
            std::abs(dot(iterator->direction,
                         iterator->previousDirection));
        iterator->sineHalfAngle = std::sqrt(
            (1.0F + iterator->cosineDelta) * 0.5F);
        iterator->radius =
            0.5F * iterator->size /
            std::max(iterator->sineHalfAngle, 0.00001F);
        iterator->counterClockwise =
            iterator->previousDirection[0] *
                    iterator->direction[1] -
                iterator->previousDirection[1] *
                    iterator->direction[0] >
            0.0F;
        iterator->edgeNormal =
            iterator->counterClockwise
                ? normalCcw(iterator->middleDirection)
                : normalCw(iterator->middleDirection);
    };
    auto alignNode = [&](const Node& source, Node& destination,
                         float cosineError, float sizeError) {
        const auto direction = normalize(
            subtract(destination.position, source.position));
        if (std::abs(direction[0]) > cosineError)
            destination.position[1] = source.position[1];
        if (std::abs(direction[1]) > cosineError)
            destination.position[0] = source.position[0];
        if (std::abs(destination.size - source.size) < sizeError)
            destination.size = source.size;
    };
    auto alignMiddleNodes =
        [&](Node& first, Node& second, float cosineError,
            float sizeError) {
        const auto direction =
            normalize(subtract(second.position, first.position));
        if (std::abs(direction[0]) > cosineError)
        {
            first.position[1] =
                (first.position[1] + second.position[1]) * 0.5F;
            second.position[1] = first.position[1];
        }
        if (std::abs(direction[1]) > cosineError)
        {
            first.position[0] =
                (first.position[0] + second.position[0]) * 0.5F;
            second.position[0] = first.position[0];
        }
        if (std::abs(second.size - first.size) < sizeError)
        {
            first.size = (first.size + second.size) * 0.5F;
            second.size = first.size;
        }
    };

    auto makeNodes =
        [&](const std::vector<std::uint32_t>& ids) {
        Nodes nodes;
        for (const auto id : ids)
        {
            if (const auto* point = tracePoint(id))
                nodes.push_back(
                    {{point->position.x, point->position.y},
                     point->width});
        }
        if (nodes.size() < 2U)
            return nodes;

        constexpr float radians20 =
            20.0F * 3.14159265358979323846F / 180.0F;
        const float cosineError = std::cos(radians20);
        constexpr float sizeError = 2.0F;
        constexpr float smoothingRadius = 10.0F;
        constexpr int smoothingSlices = 2;
        for (auto iterator = nodes.begin(); iterator != nodes.end();)
        {
            auto next = std::next(iterator);
            if (next == nodes.end())
                break;
            auto last = std::prev(nodes.end());
            if (next != last)
            {
                alignNode(*iterator, *next, cosineError, sizeError);
            }
            else
            {
                alignMiddleNodes(*next, nodes.front(), cosineError,
                                 sizeError);
                alignNode(*next, *iterator, cosineError, sizeError);
                alignNode(nodes.front(), *std::next(nodes.begin()),
                          cosineError, sizeError);
            }

            computeNode(nodes, iterator);
            if (smoothingSlices > 0 &&
                iterator->cosineDelta < cosineError)
            {
                const float cosineHalfAngle = std::sqrt(std::max(
                    0.0F, 1.0F - iterator->sineHalfAngle *
                                      iterator->sineHalfAngle));
                if (cosineHalfAngle <= 0.00001F)
                {
                    iterator = next;
                    continue;
                }
                const float size = iterator->size;
                const auto smoothingCenter = add(
                    iterator->position,
                    multiply(iterator->edgeNormal,
                             smoothingRadius / cosineHalfAngle));
                const auto smoothingVector =
                    multiply(iterator->edgeNormal, -1.0F);
                const float halfAngle = std::asin(std::clamp(
                    iterator->sineHalfAngle, 0.0F, 1.0F));
                const bool counterClockwise =
                    iterator->counterClockwise;
                iterator = nodes.erase(iterator);
                for (int slice = -smoothingSlices;
                     slice <= smoothingSlices; ++slice)
                {
                    float angle =
                        static_cast<float>(slice) /
                        static_cast<float>(smoothingSlices) *
                        halfAngle;
                    if (!counterClockwise)
                        angle = -angle;
                    const float sine = std::sin(angle);
                    const float cosine = std::cos(angle);
                    const Vec2 rotated{
                        smoothingVector[0] * cosine -
                            smoothingVector[1] * sine,
                        smoothingVector[0] * sine +
                            smoothingVector[1] * cosine};
                    iterator = nodes.insert(
                        iterator,
                        {add(smoothingCenter,
                             multiply(rotated, smoothingRadius)),
                         size});
                    ++iterator;
                }
            }
            else
            {
                iterator = next;
            }
        }
        for (auto iterator = nodes.begin(); iterator != nodes.end();
             ++iterator)
            computeNode(nodes, iterator);
        return nodes;
    };

    std::vector<std::vector<std::uint32_t>> pathIds =
        race.tracePaths;
    if (pathIds.empty())
        pathIds.push_back(race.tracePath);
    std::vector<Nodes> paths;
    paths.reserve(pathIds.size());
    for (const auto& ids : pathIds)
    {
        auto nodes = makeNodes(ids);
        if (nodes.size() > 1U)
            paths.push_back(std::move(nodes));
    }
    if (paths.empty())
        return;

    float maximumX = -std::numeric_limits<float>::max();
    float maximumY = -std::numeric_limits<float>::max();
    mapMinimumX_ = std::numeric_limits<float>::max();
    mapMinimumY_ = std::numeric_limits<float>::max();
    for (const auto& nodes : paths)
    {
        for (const auto& node : nodes)
        {
            const auto first = add(
                node.position,
                multiply(node.middleNormal, node.radius));
            const auto second = subtract(
                node.position,
                multiply(node.middleNormal, node.radius));
            mapMinimumX_ =
                std::min({mapMinimumX_, first[0], second[0]});
            mapMinimumY_ =
                std::min({mapMinimumY_, first[1], second[1]});
            maximumX = std::max({maximumX, first[0], second[0]});
            maximumY = std::max({maximumY, first[1], second[1]});
        }
    }

    constexpr float mapSize = 320.0F;
    const float worldWidth =
        std::max(maximumX - mapMinimumX_, 0.001F);
    const float worldHeight =
        std::max(maximumY - mapMinimumY_, 0.001F);
    const float maximumScale =
        std::max(std::sqrt(worldWidth * worldWidth +
                           worldHeight * worldHeight),
                 0.001F);
    mapMaximumY_ = maximumY;
    mapScale_ = mapSize / maximumScale;
    mapOriginX_ =
        menu::virtualWidth - mapSize * 0.5F -
        worldWidth * mapScale_ * 0.5F;
    mapOriginY_ =
        mapSize * 0.5F - worldHeight * mapScale_ * 0.5F;

    auto mapPosition = [&](Vec2 position) {
        return Vec2{
            mapOriginX_ +
                (position[0] - mapMinimumX_) * mapScale_,
            mapOriginY_ +
                (mapMaximumY_ - position[1]) * mapScale_};
    };
    std::vector<Vertex> vertices;
    std::vector<std::uint16_t> indices;
    for (const auto& nodes : paths)
    {
        const std::size_t firstVertex = vertices.size();
        std::size_t index = 0;
        for (const auto& node : nodes)
        {
            const auto first = mapPosition(add(
                node.position,
                multiply(node.middleNormal, node.radius)));
            const auto second = mapPosition(subtract(
                node.position,
                multiply(node.middleNormal, node.radius)));
            const float u = static_cast<float>(index % 2U);
            vertices.push_back(
                {first[0], first[1], 72.0F, 0xffffffffU, u, 0.0F});
            vertices.push_back(
                {second[0], second[1], 72.0F, 0xffffffffU, u, 1.0F});
            ++index;
        }
        for (std::size_t node = 0; node + 1U < nodes.size(); ++node)
        {
            const std::uint16_t first =
                static_cast<std::uint16_t>(
                    firstVertex + node * 2U);
            const std::uint16_t second =
                static_cast<std::uint16_t>(first + 2U);
            const std::uint16_t firstNext =
                static_cast<std::uint16_t>(first + 1U);
            const std::uint16_t secondNext =
                static_cast<std::uint16_t>(second + 1U);
            indices.insert(indices.end(),
                           {first, firstNext, second,
                            firstNext, secondNext, second});
        }
    }
    if (vertices.size() >= 4U && !indices.empty())
    {
        mapMesh_ = device.createMesh(
            vertices.data(), vertices.size(), indices.data(),
            indices.size());
    }

    const auto* first = tracePoint(race.tracePath.front());
    const auto* next = tracePoint(race.tracePath[1]);
    if (first != nullptr && next != nullptr)
    {
        const auto position =
            mapPosition({first->position.x, first->position.y});
        const auto nextPosition =
            mapPosition({next->position.x, next->position.y});
        startX_ = position[0];
        startY_ = position[1];
        startAngle_ = std::atan2(nextPosition[1] - position[1],
                                 nextPosition[0] - position[0]);
        // Plane3d::SetSize stores half-extents: the legacy renderer emits
        // vertices at +/-size. Preserve the source start marker dimensions.
        startWidth_ = first->width * mapScale_ * 0.5F;
        startHeight_ = first->width * mapScale_;
    }
}

void OriginalRaceHud::update(
    GraphicsDevice& device, const originalrace::Race& race,
    const originalrace::OriginalRaceSession& session,
    const std::vector<r3d::physics::VehicleState>& vehicles,
    const Camera& camera, float seconds)
{
    const std::size_t humanRacer = session.humanRacer();
    if (humanRacer >= session.racers().size() ||
        humanRacer >= vehicles.size())
        return;
    seconds = std::clamp(seconds, 0.0F, 0.1F);
    uiSeconds_ += seconds;
    const auto& player = session.racers()[humanRacer];
    const auto white = menu::Rgba8{255, 255, 255, 255};

    const auto placeIndex = std::min<std::size_t>(
        player.place > 0U ? player.place - 1U : 0U,
        placeNames_.size() - 1U);
    setText(device, place_, placeNames_[placeIndex],
            30.0F, true, white);
    const std::uint32_t shownLap =
        std::min(player.car.numLaps + 1U, race.lapCount);
    setText(device, lap_,
            lapName_ + " " + std::to_string(shownLap) + "/" +
                std::to_string(race.lapCount),
            25.0F, true, white);
    selectedWeaponSlot_ = player.selectedWeaponSlot;
    for (std::size_t slot = 0; slot < weaponAmmo_.size(); ++slot)
    {
        weaponVisible_[slot] =
            player.weaponSlots[slot] !=
                originalrace::RacerRuntime::invalidWeapon &&
            player.weaponSlots[slot] < race.weapons.size();
        weaponVisualIndices_[slot] = player.weaponSlots[slot];
        setText(device, weaponAmmo_[slot],
                weaponVisible_[slot]
                    ? std::to_string(player.weaponCharges[slot]) + "/" +
                          std::to_string(player.weaponCapacity[slot])
                    : std::string{},
                18.0F, true, white);
    }
    setText(device, mineAmmo_,
            std::to_string(player.mines) + "/" +
                std::to_string(player.mineCapacity),
            18.0F, true, white);
    setText(device, hyperAmmo_,
            std::to_string(player.hyperCharge) + "/" +
                std::to_string(player.hyperCapacity),
            18.0F, true, white);
    mineVisualIndex_ = player.mineWeapon;
    hyperVisualIndex_ = player.hyperWeapon;

    const float elapsed = session.elapsedSeconds();
    for (const auto& event : session.events())
    {
        if (event.kind == originalrace::RaceEventKind::Achievement &&
            event.target < achievementImages_.size() &&
            event.target < achievementPointsImages_.size())
        {
            AchievementNotification notification;
            notification.achievement = event.target;
            notification.started = uiSeconds_;
            const auto& image = achievementImages_[event.target];
            const auto& points =
                achievementPointsImages_[event.target];
            const float slotWidth =
                std::max(image.width, points.width);
            const float slotHeight =
                image.height + points.height + 15.0F;
            const float quarter =
                menu::virtualHeight * 0.25F;
            // PlayerStateFrame::NewAchievment chooses a fresh source RNG
            // position for every popup; repeated/skipped positions are
            // therefore intentional.
            const auto startPosition = static_cast<std::size_t>(
                static_cast<double>(std::rand()) /
                (static_cast<double>(RAND_MAX) + 1.0) * 8.0);
            switch (startPosition)
            {
            case 0U:
                notification.startX = -slotWidth * 2.0F;
                notification.startY = quarter;
                break;
            case 1U:
                notification.startX = -slotWidth;
                notification.startY = quarter * 2.0F;
                break;
            case 2U:
                notification.startX = -slotWidth;
                notification.startY = quarter * 3.0F;
                break;
            case 3U:
                notification.startX = 0.0F;
                notification.startY =
                    menu::virtualHeight + slotHeight;
                break;
            case 4U:
                notification.startX =
                    menu::virtualWidth + slotHeight * 2.0F;
                notification.startY = quarter;
                break;
            case 5U:
                notification.startX =
                    menu::virtualWidth + slotHeight;
                notification.startY = quarter * 2.0F;
                break;
            case 6U:
                notification.startX =
                    menu::virtualWidth + slotHeight;
                notification.startY = quarter * 3.0F;
                break;
            default:
                notification.startX = menu::virtualWidth;
                notification.startY =
                    menu::virtualHeight + slotHeight;
                break;
            }
            notification.x = notification.startX;
            notification.y = notification.startY;
            achievementNotifications_.insert(
                achievementNotifications_.begin(), notification);
        }
        else if (event.kind == originalrace::RaceEventKind::Bonus &&
            event.racer == humanRacer &&
            event.target < race.bonuses.size())
        {
            PickNotification notification;
            notification.kind = race.bonuses[event.target].kind;
            notification.slot = event.pickSlot;
            notification.started = uiSeconds_;
            const ImageAsset* image = nullptr;
            switch (notification.kind)
            {
            case originalrace::BonusKind::Medpack:
                image = &pickArmor_;
                break;
            case originalrace::BonusKind::Ammunition:
                if (notification.slot == originalrace::PickSlot::Mine)
                    image = &pickMine_;
                else if (notification.slot ==
                         originalrace::PickSlot::Hyper)
                    image = &pickIntro_;
                else
                    image = &pickAmmo_;
                break;
            case originalrace::BonusKind::Money:
                image = &pickMoney_;
                break;
            case originalrace::BonusKind::Shield:
                image = &pickShield_;
                break;
            case originalrace::BonusKind::Speed:
            case originalrace::BonusKind::SlowHazard:
            case originalrace::BonusKind::OilHazard:
            case originalrace::BonusKind::MineHazard:
            case originalrace::BonusKind::Unknown:
                break;
            }
            if (image != nullptr)
            {
                notification.x = image->width * 0.5F;
                notification.y = 255.0F;
                notification.targetX = notification.x + 30.0F;
                notifications_.insert(notifications_.begin(),
                                      notification);
            }
        }
        else if (event.kind == originalrace::RaceEventKind::Kill &&
                 event.killCredit &&
                 event.racer == humanRacer &&
                 event.target < race.racers.size())
        {
            PickNotification notification;
            notification.target = event.target;
            notification.targetGamerId =
                session.racers()[event.target].GetGamerId();
            notification.started = uiSeconds_;
            notification.x = playerKill_.width * 0.5F;
            notification.y = 255.0F;
            notification.targetX = notification.x + 30.0F;
            const auto name = racerName(race, session, event.target);
            setText(device, notification.label, name, 24.0F, false,
                    {214, 214, 214, 255});
            notifications_.insert(notifications_.begin(),
                                  std::move(notification));
        }
        else if (event.kind ==
                     originalrace::RaceEventKind::CountdownChanged &&
                 event.value <= 0.0F)
        {
            countdownFinishUntil_ = elapsed + 1.5F;
        }
        else if (event.kind == originalrace::RaceEventKind::Damage &&
                 event.value > 0.0F)
        {
            std::size_t overlay = carLifeOverlays_.size();
            std::size_t racer = event.racer;
            float duration = 0.0F;
            if (event.racer == humanRacer)
            {
                overlay = 0;
                duration = 1.5F;
            }
            else if (event.target == humanRacer)
            {
                overlay = 1;
                duration = 4.0F;
            }
            if (overlay < carLifeOverlays_.size() &&
                racer < session.racers().size() &&
                racer < vehicles.size())
            {
                auto& life = carLifeOverlays_[overlay];
                life.racer = racer;
                life.duration = duration;
                life.visibleUntil = elapsed + duration;
                life.alpha = 0.0F;
                life.visible = true;
            }
        }
    }
    for (auto iterator = notifications_.begin();
         iterator != notifications_.end();)
    {
        if (uiSeconds_ - iterator->started < 5.0F)
        {
            ++iterator;
            continue;
        }
        if (valid(iterator->label.texture))
            device.destroy(iterator->label.texture);
        iterator = notifications_.erase(iterator);
    }
    for (std::size_t index = 0; index < notifications_.size(); ++index)
    {
        auto& notification = notifications_[index];
        const float age = uiSeconds_ - notification.started;
        const bool fadingOut = age > 4.7F;
        notification.alpha =
            age < 0.3F
                ? std::clamp(age / 0.3F, 0.0F, 1.0F)
                : (fadingOut
                       ? 1.0F -
                             std::clamp((age - 4.7F) / 0.3F,
                                        0.0F, 1.0F)
                       : 1.0F);
        const float targetY =
            255.0F + static_cast<float>(index) * 85.0F +
            (fadingOut ? 30.0F : 0.0F);
        notification.x =
            std::min(notification.x + 90.0F * seconds,
                     notification.targetX);
        notification.y =
            std::min(notification.y + 120.0F * seconds, targetY);
    }
    achievementNotifications_.erase(
        std::remove_if(
            achievementNotifications_.begin(),
            achievementNotifications_.end(),
            [&](const AchievementNotification& notification) {
                return uiSeconds_ - notification.started >= 5.0F;
            }),
        achievementNotifications_.end());
    for (std::size_t index = 0;
         index < achievementNotifications_.size(); ++index)
    {
        auto& notification = achievementNotifications_[index];
        if (notification.achievement >= achievementImages_.size() ||
            notification.achievement >=
                achievementPointsImages_.size())
            continue;
        const float age = uiSeconds_ - notification.started;
        const float fly = std::clamp(age / 0.3F, 0.0F, 1.0F);
        const float out =
            std::clamp((age - 4.7F) / 0.3F, 0.0F, 1.0F);
        notification.alpha = 1.0F - out;
        notification.pointsAlpha =
            std::clamp((age - 0.8F) / 0.15F, 0.0F, 1.0F) - out;
        const float ping =
            std::clamp((age - 0.2F) / 0.1F, 0.0F, 1.0F) -
            std::clamp((age - 0.3F) / 0.1F, 0.0F, 1.0F);
        notification.scale = 1.0F + ping;
        const auto& image =
            achievementImages_[notification.achievement];
        const auto& points =
            achievementPointsImages_[notification.achievement];
        const float slotHeight =
            image.height + points.height + 15.0F;
        const float targetX =
            (100.0F + menu::virtualWidth) * 0.5F;
        float stackIndex = static_cast<float>(index);
        if (notification.indexTime < 0.0F &&
            stackIndex != notification.lastIndex)
        {
            notification.indexTime = 0.0F;
        }
        if (notification.indexTime >= 0.0F)
        {
            notification.indexTime += seconds;
            const float reindex = std::clamp(
                notification.indexTime / 0.15F, 0.0F, 1.0F);
            stackIndex = notification.lastIndex +
                (stackIndex - notification.lastIndex) * reindex;
            if (reindex >= 1.0F)
            {
                notification.lastIndex =
                    static_cast<float>(index);
                notification.indexTime = -1.0F;
            }
        }
        const float targetY =
            15.0F + image.height * 0.5F +
            stackIndex * slotHeight;
        notification.x =
            notification.startX +
            (targetX - notification.startX) * fly;
        notification.y =
            notification.startY +
            (targetY - notification.startY) * fly;
    }

    countdownAlpha_ = 1.0F;
    countdownGrowthSeconds_ = 0.0F;
    if (session.phase() == originalrace::RacePhase::Countdown)
    {
        // PlayerStateFrame selects tablo0 for cRaceStartWait and then
        // tablo1..tablo3 for the three timed stages. countdownSeconds alone
        // cannot represent the distinct wait and first-red-light states.
        countdownImage_ = std::clamp(session.countdownStage(), 0, 3);
    }
    else if (session.phase() == originalrace::RacePhase::Racing &&
             elapsed < countdownFinishUntil_)
    {
        countdownImage_ = 4;
        countdownGrowthSeconds_ =
            std::clamp(1.5F - (countdownFinishUntil_ - elapsed),
                       0.0F, 1.5F);
        countdownAlpha_ =
            std::clamp((countdownFinishUntil_ - elapsed) / 1.5F,
                       0.0F, 1.0F);
    }
    else
        countdownImage_ = -1;

    lifeFraction_ =
        std::clamp(player.life / std::max(player.maximumLife, 1.0F),
                   0.0F, 1.0F);

    mapMarkers_.clear();
    mapMarkers_.reserve(std::min(vehicles.size(), race.racers.size()));
    for (std::size_t index = 0;
         index < vehicles.size() && index < race.racers.size(); ++index)
    {
        if (index < session.racers().size() &&
            session.racers()[index].disconnected)
        {
            continue;
        }
        // MiniMapFrame::UpdatePlayers uses CarState::GetMapPos(), which is a
        // trace projection and retains the last valid tile coordinate while
        // a car is off-road. A raw body coordinate makes an AI reset look
        // like a marker teleport across the map.
        const auto position = session.mapPosition(index);
        mapMarkers_.push_back(
            {mapOriginX_ + (position.x - mapMinimumX_) * mapScale_,
             mapOriginY_ + (mapMaximumY_ - position.y) * mapScale_,
             0.0F,
             index < session.racers().size()
                 ? session.racers()[index].GetColor()
                 : race.racers[index].color});
    }

    std::vector<std::size_t> opponentRacers;
    const std::size_t visibleRacers =
        std::min({vehicles.size(), race.racers.size(),
                  session.racers().size()});
    opponentRacers.reserve(visibleRacers > 0U ? visibleRacers - 1U : 0U);
    for (std::size_t racer = 0U; racer < visibleRacers; ++racer)
        if (racer != humanRacer)
            opponentRacers.push_back(racer);
    const std::size_t opponentCount = opponentRacers.size();
    opponentLabels_.resize(opponentCount);
    std::array<float, 16> viewProjection{};
    bx::mtxMul(viewProjection.data(), camera.view.data(),
               camera.projection.data());

    auto project = [&](const originalrace::Vec3& position,
                       originalrace::Vec3 offset, float& screenX,
                       float& screenY) {
        const float worldX = position.x + offset.x;
        const float worldY = position.y + offset.y;
        const float worldZ = position.z + offset.z;
        const float x = worldX * viewProjection[0] +
                        worldY * viewProjection[4] +
                        worldZ * viewProjection[8] +
                        viewProjection[12];
        const float y = worldX * viewProjection[1] +
                        worldY * viewProjection[5] +
                        worldZ * viewProjection[9] +
                        viewProjection[13];
        const float z = worldX * viewProjection[2] +
                        worldY * viewProjection[6] +
                        worldZ * viewProjection[10] +
                        viewProjection[14];
        const float w = worldX * viewProjection[3] +
                        worldY * viewProjection[7] +
                        worldZ * viewProjection[11] +
                        viewProjection[15];
        float projectedX =
            std::abs(w) > 0.000001F ? x / w : x;
        float projectedY =
            std::abs(w) > 0.000001F ? y / w : y;
        // PlayerStateFrame keeps a point behind the camera on the edge of
        // the viewport while fading it.  It normalizes the projected vector
        // to sqrt(2), then clamps both coordinates to [-1, 1].
        if (z < 0.0F)
        {
            const float length = std::sqrt(
                projectedX * projectedX +
                projectedY * projectedY);
            if (length > 0.000001F)
            {
                constexpr float diagonal =
                    1.4142135623730950488F;
                projectedX = projectedX / length * diagonal;
                projectedY = projectedY / length * diagonal;
            }
            else
            {
                projectedX = 1.0F;
            }
        }
        projectedX = std::clamp(projectedX, -1.0F, 1.0F);
        projectedY = std::clamp(projectedY, -1.0F, 1.0F);
        const bool atEdge =
            std::abs(projectedX) >= 0.999999F ||
            std::abs(projectedY) >= 0.999999F;
        screenX =
            (projectedX * 0.5F + 0.5F) * menu::virtualWidth;
        screenY =
            (-projectedY * 0.5F + 0.5F) * menu::virtualHeight;
        return atEdge;
    };

    for (auto& overlay : carLifeOverlays_)
    {
        const bool hasRacer =
            overlay.racer < session.racers().size() &&
            overlay.racer < vehicles.size() &&
            !session.racers()[overlay.racer].destroyed;
        if (!hasRacer)
        {
            overlay.alpha = std::max(
                overlay.alpha - seconds / 0.3F, 0.0F);
            overlay.visible = overlay.alpha > 0.0F;
            continue;
        }
        const bool atEdge = project(
            vehicles[overlay.racer].body.position, {},
            overlay.x, overlay.y);
        const float targetAlpha =
            elapsed < overlay.visibleUntil && !atEdge ? 1.0F : 0.0F;
        const float alphaStep = seconds / 0.3F;
        if (targetAlpha > overlay.alpha)
            overlay.alpha =
                std::min(overlay.alpha + alphaStep, targetAlpha);
        else
            overlay.alpha =
                std::max(overlay.alpha - alphaStep, targetAlpha);
        overlay.visible =
            elapsed < overlay.visibleUntil || overlay.alpha > 0.0F;
        const auto& runtime = session.racers()[overlay.racer];
        overlay.life = std::clamp(
            runtime.life / std::max(runtime.maximumLife, 1.0F),
            0.0F, 1.0F);
        overlay.x = std::clamp(
            overlay.x, opponentLifeBack_.width * 0.5F,
            menu::virtualWidth - opponentLifeBack_.width * 0.5F);
        overlay.y = std::clamp(
            overlay.y, opponentLifeBack_.height * 0.5F,
            menu::virtualHeight -
                opponentLifeBack_.height * 0.5F);
    }

    for (std::size_t index = 0; index < opponentCount; ++index)
    {
        auto& label = opponentLabels_[index];
        const std::size_t racerIndex = opponentRacers[index];
        const auto name = racerName(race, session, racerIndex);
        const auto& runtime = session.racers()[racerIndex];
        setText(device, label.name,
                formatNamePlace(namePlaceFormat_, runtime.place, name),
                15.0F, true, white);
        const bool atEdge = project(
            vehicles[racerIndex].body.position,
            {1.0F, -0.5F, 0.0F}, label.x, label.y);
        label.visible = !runtime.destroyed && !runtime.disconnected;
        bool hasLifeOverlay = false;
        if (label.visible)
        {
            for (const auto& overlay : carLifeOverlays_)
            {
                if (overlay.visible && overlay.racer == racerIndex)
                {
                    hasLifeOverlay = true;
                    break;
                }
            }
        }
        label.x = std::clamp(label.x, 40.5F,
                             menu::virtualWidth - 40.5F);
        label.y = std::clamp(label.y, label.name.height,
                             menu::virtualHeight - 11.5F);
        label.radius =
            std::max({label.name.width, label.name.height, 1.0F});
        if (!label.visible)
            label.alpha = 0.0F;
        else if (atEdge || hasLifeOverlay)
            label.alpha = std::max(
                label.alpha - 4.0F * seconds, 0.0F);
        else
            label.alpha = 1.0F;
    }
    std::vector<std::size_t> labelOrder(opponentCount);
    std::iota(labelOrder.begin(), labelOrder.end(), 0U);
    std::stable_sort(
        labelOrder.begin(), labelOrder.end(),
        [&](std::size_t first, std::size_t second) {
            return session.racers()[opponentRacers[first]].place >
                   session.racers()[opponentRacers[second]].place;
        });
    for (std::size_t order = 0; order < labelOrder.size(); ++order)
    {
        auto& opponent = opponentLabels_[labelOrder[order]];
        if (!opponent.visible || opponent.alpha <= 0.0F)
            continue;
        const float targetAlpha = opponent.alpha;
        float alpha = 1.0F;
        for (std::size_t previous = 0; previous < order; ++previous)
        {
            const auto& other =
                opponentLabels_[labelOrder[previous]];
            if (!other.visible)
                continue;
            const float dx = other.x - opponent.x;
            const float dy = other.y - opponent.y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            const float radius = opponent.radius + other.radius;
            alpha = std::min(
                alpha, radius > 0.0F ? distance / radius : 0.0F);
        }
        opponent.alpha = std::min(alpha, targetAlpha);
    }

    if (session.phase() == originalrace::RacePhase::Finished &&
        !finishVisible_)
    {
        finishVisible_ = true;
        finishStarted_ = uiSeconds_;
        setText(device, finishPrice_, priceName_, 30.0F, true,
                {233, 167, 63, 255});
        setText(device, finishMoneyPoints_,
                moneyName_ + "\n" + pointsName_, 30.0F, true,
                {225, 225, 225, 255});
        std::vector<std::size_t> order;
        order.reserve(session.racers().size());
        for (std::size_t racer = 0U;
             racer < session.racers().size(); ++racer)
        {
            if (!session.racers()[racer].disconnected)
                order.push_back(racer);
        }
        std::stable_sort(
            order.begin(), order.end(),
            [&](std::size_t first, std::size_t second) {
                return session.racers()[first].place <
                       session.racers()[second].place;
            });
        for (std::size_t row = 0; row < finishRows_.size(); ++row)
        {
            auto& output = finishRows_[row];
            if (row >= order.size())
            {
                output.racer =
                    std::numeric_limits<std::size_t>::max();
                output.gamerId = -1;
                continue;
            }
            output.racer = order[row];
            const auto& runtime = session.racers()[output.racer];
            output.gamerId = runtime.GetGamerId();
            const auto* sourceResult =
                session.resultForRacer(output.racer);
            const auto place =
                std::min<std::size_t>(
                    runtime.place > 0 ? runtime.place - 1U : row,
                    race.rewardMoney.size() - 1U);
            const auto rewardMoney =
                sourceResult != nullptr
                    ? sourceResult->money
                    : runtime.rewardMoney > 0
                    ? runtime.rewardMoney
                    : race.rewardMoney[place];
            const auto rewardPoints =
                sourceResult != nullptr
                    ? sourceResult->points
                    : runtime.rewardPoints > 0
                    ? runtime.rewardPoints
                    : race.rewardPoints[place];
            const auto name = racerName(race, session, output.racer);
            setText(device, output.name, name, 30.0F, true,
                    {233, 167, 63, 255});
            std::string value = std::to_string(rewardMoney);
            const auto pickedMoney =
                sourceResult != nullptr ? sourceResult->pickedMoney
                                        : runtime.pickedMoney;
            if (pickedMoney > 0)
                value += " + " + std::to_string(pickedMoney);
            value += "\n" + std::to_string(rewardPoints);
            setText(device, output.value, std::move(value), 30.0F,
                    true, {132, 188, 67, 255});
        }
    }
}

void OriginalRaceHud::draw(GraphicsDevice& device, Mesh quad,
                           Shader shader, Shader meshShader,
                           bool enableRaceState) const
{
    PipelineState pipeline;
    pipeline.faceCulling = PipelineState::FaceCulling::None;
    pipeline.writeDepth = false;
    pipeline.depthTest = false;
    pipeline.alphaBlend = true;

    if (finishVisible_)
    {
        constexpr float boxHeight = 240.0F;
        const float firstY =
            (menu::virtualHeight -
             static_cast<float>(finishRows_.size()) * boxHeight) *
            0.5F;
        const float leftOffsetX =
            (finishLeftFrame_.width + menu::virtualWidth * 0.5F) *
            0.5F;
        const float rightOffsetX =
            (menu::virtualWidth * 0.5F + menu::virtualWidth -
             finishLeftFrame_.width) *
            0.5F;
        const float shownSeconds =
            std::max(uiSeconds_ - finishStarted_, 0.0F);
        const float alpha = std::clamp(
            (shownSeconds - 0.15F) / 0.5F, 0.0F, 1.0F);
        const std::array<float, 4> tint{
            1.0F, 1.0F, 1.0F, alpha};
        auto stretchedSize = [](const ImageAsset& image, float width,
                                float height) {
            const float scale =
                std::min(width / std::max(image.width, 1.0F),
                         height / std::max(image.height, 1.0F));
            return std::array<float, 2>{
                image.width * scale, image.height * scale};
        };
        for (std::size_t row = 0; row < finishRows_.size(); ++row)
        {
            if (finishRows_[row].racer ==
                std::numeric_limits<std::size_t>::max())
                continue;
            const float offsetX =
                (1.0F - alpha) * (menu::virtualWidth + 25.0F) *
                (row % 2U == 1U ? 1.0F : -1.0F);
            const float top =
                firstY + static_cast<float>(row) * boxHeight;
            drawTintedAsset(
                device, quad, shader, finishLeftFrame_.texture,
                finishLeftFrame_.width, finishLeftFrame_.height,
                offsetX + finishLeftFrame_.width * 0.5F,
                top + finishLeftFrame_.height * 0.5F, 40.0F,
                pipeline, tint);
            drawTintedAsset(
                device, quad, shader, finishRightFrame_.texture,
                finishRightFrame_.width, finishRightFrame_.height,
                offsetX + menu::virtualWidth -
                    finishRightFrame_.width * 0.5F,
                top + finishRightFrame_.height * 0.5F, 40.0F,
                pipeline, tint);
            const float lineWidth =
                menu::virtualWidth - finishLeftFrame_.width -
                finishRightFrame_.width;
            drawTintedAsset(
                device, quad, shader, finishLineFrame_.texture,
                lineWidth, finishLineFrame_.height,
                offsetX + menu::virtualWidth * 0.5F,
                top + finishLineFrame_.height * 0.5F, 40.0F,
                pipeline, tint);

            const auto racer = finishRows_[row].racer;
            if (const auto* photo =
                    racerPhoto(finishRows_[row].gamerId, racer))
            {
                const auto size = stretchedSize(*photo, 198.0F, 193.0F);
                drawTintedAsset(
                    device, quad, shader, photo->texture,
                    size[0], size[1], offsetX + 128.0F,
                    top + 116.0F, 28.0F, pipeline, tint);
            }
            const auto cupSize =
                stretchedSize(finishCups_[row], 190.0F, 160.0F);
            drawTintedAsset(
                device, quad, shader, finishCups_[row].texture,
                cupSize[0], cupSize[1],
                offsetX + menu::virtualWidth -
                    finishRightFrame_.width + 160.0F,
                top + 115.0F, 28.0F, pipeline, tint);
            drawTintedAsset(
                device, quad, shader, finishRows_[row].name.texture,
                finishRows_[row].name.width,
                finishRows_[row].name.height,
                offsetX + leftOffsetX, top + 63.0F, 20.0F,
                pipeline, tint);
            drawTintedAsset(
                device, quad, shader, finishPrice_.texture,
                finishPrice_.width, finishPrice_.height,
                offsetX + rightOffsetX, top + 63.0F, 20.0F,
                pipeline, tint);
            drawTintedAsset(
                device, quad, shader, finishMoneyPoints_.texture,
                finishMoneyPoints_.width,
                finishMoneyPoints_.height,
                offsetX + leftOffsetX, top + 154.0F, 20.0F,
                pipeline, tint);
            drawTintedAsset(
                device, quad, shader, finishRows_[row].value.texture,
                finishRows_[row].value.width,
                finishRows_[row].value.height,
                offsetX + rightOffsetX, top + 154.0F, 20.0F,
                pipeline, tint);
        }
        return;
    }

    SceneLighting hudLighting;
    hudLighting.lightDirection =
        {-0.45F, -0.35F, 0.82F, 0.0F};
    hudLighting.ambient = {0.62F, 0.62F, 0.62F, 1.0F};
    hudLighting.fogColor = {0.0F, 0.0F, 0.0F, 0.0F};
    hudLighting.cameraPosition =
        {menu::virtualWidth * 0.5F,
         menu::virtualHeight * 0.5F, -80.0F, 1.0F};
    device.setSceneLighting(hudLighting);

    auto drawWeaponVisual = [&](std::size_t index, float x, float y,
                                float depth) {
        if (index == originalrace::RacerRuntime::invalidWeapon ||
            index >= weaponVisuals_.size())
            return;
        const auto& visual = weaponVisuals_[index];
        if (!valid(visual.mesh) || visual.textures.empty())
            return;
        Transform model;
        bx::mtxSRT(model.matrix.data(), 1.0F, 1.0F, 1.0F,
                   -0.82F, 0.0F, 0.62F, x, y, depth);
        auto meshPipeline = pipeline;
        meshPipeline.faceCulling =
            PipelineState::FaceCulling::None;
        auto materialAt = [&](std::size_t group) {
            MaterialState material;
            if (!visual.materials.empty())
            {
                const auto& source = visual.materials[
                    std::min(group, visual.materials.size() - 1U)];
                material.color = source.color;
                material.alphaReference = source.alphaReference;
                material.emissive = std::max(source.emissive, 0.35F);
                material.specular = source.specular;
                material.shininess = source.shininess;
                material.ignoreFog = source.ignoreFog;
            }
            return material;
        };
        if (visual.groups.empty())
        {
            device.draw(visual.mesh, meshShader,
                        visual.textures.front(), model, meshPipeline,
                        {}, materialAt(0));
            return;
        }
        for (std::size_t group = 0; group < visual.groups.size();
             ++group)
        {
            device.draw(
                visual.mesh, meshShader,
                visual.textures[std::min(
                    group, visual.textures.size() - 1U)],
                model, meshPipeline, visual.groups[group],
                materialAt(group));
        }
    };

    // MiniMapFrame::GetMiniMapRect is 320x320 and anchored to the
    // right/top of the viewport.
    if (valid(mapMesh_) && valid(mapStrip_.texture))
    {
        MaterialState material;
        material.emissive = 1.0F;
        material.specular = 0.0F;
        device.draw(mapMesh_, shader, mapStrip_.texture,
                    identityTransform(), pipeline, {}, material);
    }
    drawAsset(device, quad, shader, mapStart_.texture, startWidth_,
              startHeight_, startX_, startY_, 66.0F, pipeline,
              startAngle_);
    for (const auto& marker : mapMarkers_)
    {
        // Legacy Plane3d renders from -size to +size, so SetSize(10, 10)
        // corresponds to a 20x20 marker in the final viewport.
        drawTintedAsset(device, quad, shader, mapPlayer_.texture,
                        20.0F, 20.0F, marker.x, marker.y, 58.0F,
                        pipeline, marker.color, marker.angle);
    }
    for (const auto& opponent : opponentLabels_)
    {
        if (!opponent.visible)
            continue;
        const std::array<float, 4> tint{
            1.0F, 1.0F, 1.0F, opponent.alpha};
        drawTintedAsset(
            device, quad, shader, mapOpponent_.texture, 81.0F,
            23.0F, opponent.x, opponent.y, 44.0F, pipeline, tint);
        drawTintedAsset(
            device, quad, shader, opponent.name.texture,
            opponent.name.width, opponent.name.height, opponent.x,
            opponent.y - 20.0F, 36.0F, pipeline, tint);
    }
    for (const auto& overlay : carLifeOverlays_)
    {
        if (!overlay.visible)
            continue;
        const std::array<float, 4> tint{
            1.0F, 1.0F, 1.0F, overlay.alpha};
        drawTintedAsset(
            device, quad, shader, opponentLifeBack_.texture,
            opponentLifeBack_.width, opponentLifeBack_.height,
            overlay.x, overlay.y, 42.0F, pipeline, tint);
        const float width = opponentLifeBar_.width * overlay.life;
        drawTintedAsset(
            device, quad, shader, opponentLifeBar_.texture, width,
            opponentLifeBar_.height,
            overlay.x - opponentLifeBar_.width * 0.5F +
                width * 0.5F,
            overlay.y, 34.0F, pipeline, tint);
    }

    // PlayerStateFrame::_raceState and MiniMapFrame's lap widgets obey
    // enableHUD. Their sibling widgets (map, markers, event overlays and
    // countdown) deliberately remain visible in the Windows implementation.
    if (enableRaceState)
    {
        drawAsset(device, quad, shader, placeFrame_.texture,
                  placeFrame_.width, placeFrame_.height,
                  placeFrame_.width * 0.5F,
                  placeFrame_.height * 0.5F,
                  38.0F, pipeline);
        drawAsset(device, quad, shader, place_.texture, place_.width,
                  place_.height, 105.0F, 88.0F, 28.0F, pipeline);
        drawAsset(device, quad, shader, lifeBack_.texture,
                  lifeBack_.width, lifeBack_.height,
                  165.0F + lifeBack_.width * 0.5F,
                  lifeBack_.height * 0.5F, 28.0F, pipeline);
        const float lifeWidth = lifeBar_.width * lifeFraction_;
        drawAsset(device, quad, shader, lifeBar_.texture, lifeWidth,
                  lifeBar_.height,
                  165.0F + lifeBack_.width * 0.5F -
                      lifeBar_.width * 0.5F + lifeWidth * 0.5F,
                  lifeBack_.height * 0.5F,
                  18.0F, pipeline);

        for (std::size_t slot = 0; slot < weaponAmmo_.size(); ++slot)
        {
            if (!weaponVisible_[slot])
                continue;
            const bool selected = slot == selectedWeaponSlot_;
            const auto& image =
                selected ? weaponSlotSelected_ : weaponSlot_;
            const float centerX =
                155.0F + image.width * 0.5F +
                static_cast<float>(slot) * (image.width - 25.0F);
            const float centerY = 50.0F + image.height * 0.5F;
            drawAsset(device, quad, shader, image.texture, image.width,
                      image.height, centerX, centerY, 34.0F, pipeline);
            drawWeaponVisual(weaponVisualIndices_[slot],
                             centerX + 5.0F, centerY - 15.0F,
                             28.0F);
            drawAsset(device, quad, shader, weaponAmmo_[slot].texture,
                      weaponAmmo_[slot].width,
                      weaponAmmo_[slot].height,
                      centerX - 10.0F, centerY + 26.0F, 22.0F,
                      pipeline);
        }
        drawAsset(device, quad, shader, mineSlot_.texture,
                  mineSlot_.width, mineSlot_.height, 30.0F, 140.0F,
                  34.0F, pipeline);
        drawWeaponVisual(mineVisualIndex_, 30.0F, 140.0F, 28.0F);
        drawAsset(device, quad, shader, mineAmmo_.texture,
                  mineAmmo_.width, mineAmmo_.height, 105.0F, 159.0F,
                  22.0F, pipeline);
        drawAsset(device, quad, shader, hyperSlot_.texture,
                  hyperSlot_.width, hyperSlot_.height, 30.0F, 32.0F,
                  34.0F, pipeline);
        drawWeaponVisual(hyperVisualIndex_, 30.0F, 32.0F, 28.0F);
        drawAsset(device, quad, shader, hyperAmmo_.texture,
                  hyperAmmo_.width, hyperAmmo_.height, 105.0F, 15.0F,
                  22.0F, pipeline);

        drawAsset(device, quad, shader, lapBack_.texture,
                  lapBack_.width, lapBack_.height,
                  lapBack_.width * 0.5F, 200.0F, 34.0F, pipeline);
        drawAsset(device, quad, shader, lap_.texture, lap_.width,
                  lap_.height, lapBack_.width * 0.5F - 10.0F,
                  201.0F, 22.0F, pipeline);
    }

    for (const auto& item : notifications_)
    {
        const ImageAsset* notification = nullptr;
        if (item.target != std::numeric_limits<std::size_t>::max())
            notification = &playerKill_;
        switch (item.kind)
        {
        case originalrace::BonusKind::Medpack:
            notification = &pickArmor_;
            break;
        case originalrace::BonusKind::Ammunition:
            if (item.slot == originalrace::PickSlot::Mine)
                notification = &pickMine_;
            else if (item.slot == originalrace::PickSlot::Hyper)
                notification = &pickIntro_;
            else
                notification = &pickAmmo_;
            break;
        case originalrace::BonusKind::Money:
            notification = &pickMoney_;
            break;
        case originalrace::BonusKind::Shield:
            notification = &pickShield_;
            break;
        case originalrace::BonusKind::Speed:
        case originalrace::BonusKind::SlowHazard:
        case originalrace::BonusKind::OilHazard:
        case originalrace::BonusKind::MineHazard:
        case originalrace::BonusKind::Unknown:
            break;
        }
        if (notification != nullptr)
        {
            drawTintedAsset(
                device, quad, shader, notification->texture,
                notification->width, notification->height,
                item.x, item.y, 20.0F, pipeline,
                {1.0F, 1.0F, 1.0F, item.alpha});
            if (const auto* photo =
                    racerPhoto(item.targetGamerId, item.target))
            {
                drawTintedAsset(
                    device, quad, shader,
                    photo->texture, 50.0F, 50.0F,
                    item.x - 40.0F, item.y, 18.0F, pipeline,
                    {1.0F, 1.0F, 1.0F, item.alpha});
                drawTintedAsset(
                    device, quad, shader, item.label.texture,
                    item.label.width, item.label.height,
                    item.x - 15.0F + item.label.width * 0.5F,
                    item.y, 16.0F, pipeline,
                    {1.0F, 1.0F, 1.0F, item.alpha});
            }
        }
    }

    for (const auto& item : achievementNotifications_)
    {
        if (item.achievement >= achievementImages_.size() ||
            item.achievement >= achievementPointsImages_.size())
            continue;
        const auto& image = achievementImages_[item.achievement];
        const auto& points =
            achievementPointsImages_[item.achievement];
        drawTintedAsset(
            device, quad, shader, image.texture,
            image.width * item.scale, image.height * item.scale,
            item.x, item.y, 12.0F, pipeline,
            {1.0F, 1.0F, 1.0F, item.alpha});
        if (campaign_)
        {
            const float pointsY =
                item.y + image.height * 0.5F + 15.0F +
                points.height * 0.5F;
            drawTintedAsset(
                device, quad, shader, points.texture,
                points.width, points.height, item.x, pointsY,
                10.0F, pipeline,
                {1.0F, 1.0F, 1.0F, item.pointsAlpha});
            if (valid(achievementMultiplierImage_.texture))
            {
                drawTintedAsset(
                    device, quad, shader,
                    achievementMultiplierImage_.texture,
                    achievementMultiplierImage_.width,
                    achievementMultiplierImage_.height,
                    item.x + 95.0F, pointsY - 3.0F,
                    8.0F, pipeline,
                    {1.0F, 1.0F, 1.0F, item.pointsAlpha});
            }
        }
    }

    if (countdownImage_ >= 0 &&
        static_cast<std::size_t>(countdownImage_) <
            countdownImages_.size())
    {
        const auto& image =
            countdownImages_[static_cast<std::size_t>(countdownImage_)];
        const float growth = 200.0F * countdownGrowthSeconds_;
        drawTintedAsset(
            device, quad, shader, image.texture,
            image.width * 0.5F + growth,
            image.height * 0.5F + growth,
            menu::virtualWidth * 0.5F,
            menu::virtualHeight * 0.5F, 4.0F, pipeline,
            {1.0F, 1.0F, 1.0F, countdownAlpha_});
    }
}

} // namespace rrr3d::race
