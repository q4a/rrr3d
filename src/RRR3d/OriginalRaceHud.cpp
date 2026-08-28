#include "OriginalRaceHud.h"

#include "CoreTextRasterizer.h"
#include "resource/R3DMeshAsset.h"
#include "resource/ResourceFileSystem.h"

#include <bx/math.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <iterator>
#include <limits>
#include <sstream>

namespace rrr3d::race
{
namespace
{

using namespace r3d::renderer;
namespace menu = r3d::game::mainmenu2;
namespace originalrace = r3d::game::originalrace;
namespace source = r3d::game::originalrace::source;

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
    OriginalResourceManager& resources, std::string path,
    ImageAsset& output, std::string& error)
{
    try
    {
        const auto& image = resources.GetTexture(path);
        output.width = static_cast<float>(image.width);
        output.height = static_cast<float>(image.height);
        output.texture = image.texture;
        if (!valid(output.texture))
            throw std::runtime_error("unable to upload " +
                                     image.name);
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
    OriginalResourceManager& resources,
    const r3d::game::originalgamedata::Catalog& gameData,
    const originalrace::Race& race, std::string_view language,
    std::string_view difficulty, bool campaign, std::string& error)
{
    campaign_ = campaign;
    hudMenuState_.Reset();
    playerStateFrame_.Reset();
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
                   opponentLifeBar_, error))
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
            const auto& shared = resources.GetMesh(source.meshPath);
            const auto& mesh = *shared.source;
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
                visual.textures.push_back(
                    resources.GetTexture(
                        material.texturePath).texture);
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
        const auto* selectedLanguage =
            r3d::game::originalgamedata::findLanguage(
                gameData, language);
        if (selectedLanguage == nullptr)
        {
            throw r3d::resource::ResourceError(
                "HUD language is absent from game.xml: " +
                std::string(language));
        }
        const auto localization =
            r3d::game::originalgamedata::loadOriginalStringLibrary(
                resources.GetFileSystem(), *selectedLanguage);
        lapName_ = localization.get("svLap");
        namePlaceFormat_ = localization.get("svNamePlaceMarker");
        priceName_ = localization.get("svPrice");
        for (std::size_t index = 0; index < placeNames_.size(); ++index)
        {
            placeNames_[index] = localization.get(
                "svPlace" + std::to_string(index + 1U));
        }
        localizedRacerNames_.reserve(race.racers.size());
        for (const auto& racer : race.racers)
            localizedRacerNames_.push_back(
                localization.get(racer.name));
        for (const auto& identity : race.playerIdentities)
        {
            if (originalrace::findOriginalPlayerIdentity(
                    race, identity.id) != &identity)
                continue;
            if (localizedGamerNames_.contains(identity.id))
                continue;
            localizedGamerNames_.emplace(
                identity.id, localization.get(identity.name));
        }
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        shutdown(device);
        return false;
    }
    buildMiniMap(device, race);
    error.clear();
    return true;
}

void OriginalRaceHud::shutdown(GraphicsDevice& device) noexcept
{
    auto releaseImage = [&](ImageAsset& asset) {
        asset = {};
    };
    auto releaseText = [&](TextAsset& asset) {
        if (valid(asset.texture))
            device.destroy(asset.texture);
        asset = {};
    };
    for (auto& visual : weaponVisuals_)
    {
        if (valid(visual.mesh))
            device.destroy(visual.mesh);
        visual = {};
    }
    weaponVisuals_.clear();
    for (auto& opponent : opponentLabels_)
        releaseText(opponent.name);
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
    if (valid(mapMesh_))
        device.destroy(mapMesh_);
    mapMesh_ = {};
    miniMapState_.Clear();
    opponentLabels_.clear();
    notifications_.clear();
    achievementNotifications_.clear();
    playerStateFrame_.Reset();
    localizedRacerNames_.clear();
    localizedGamerNames_.clear();
    uiSeconds_ = 0.0F;
    hudMenuState_.Reset();
}

std::string_view OriginalRaceHud::localizedLapName() const noexcept
{
    return lapName_;
}

std::string_view OriginalRaceHud::localizedPriceName() const noexcept
{
    return priceName_;
}

source::HudMenuCommand OriginalRaceHud::handleEscape(
    bool active, bool repeated, bool paused) const noexcept
{
    return hudMenuState_.OnHandleInput(active, repeated, paused);
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
    if (!miniMapState_.Build(race, menu::virtualWidth))
        return;

    const auto& geometry = miniMapState_.GetGeometry();
    std::vector<Vertex> vertices;
    vertices.reserve(geometry.vertices.size());
    for (const auto& vertex : geometry.vertices)
    {
        vertices.push_back(
            {vertex.x, vertex.y, 72.0F, 0xffffffffU,
             vertex.u, vertex.v});
    }
    mapMesh_ = device.createMesh(
        vertices.data(), vertices.size(), geometry.indices.data(),
        geometry.indices.size());
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

    source::HudRaceStateInput raceStateInput;
    raceStateInput.place = player.GetPlace();
    raceStateInput.life = player.GetLife();
    raceStateInput.maximumLife = player.GetMaxLife();
    raceStateInput.carAlive = !player.IsDestroyed();
    raceStateInput.selectedPrimarySlot = player.selectedWeaponSlot;
    raceStateInput.primaryBoxWidth = weaponSlot_.width;
    raceStateInput.primaryBoxHeight = weaponSlot_.height;
    const auto primaryItems = player.GetPrimaryWeaponItems();
    for (std::size_t slot = 0U; slot < primaryItems.size(); ++slot)
    {
        auto& input = raceStateInput.weapons[slot + 2U];
        input.visual = player.weaponSlots[slot];
        input.mounted = primaryItems[slot] != nullptr &&
            input.visual != originalrace::RacerRuntime::invalidWeapon &&
            input.visual < race.weapons.size();
        if (input.mounted)
        {
            input.currentCharge = primaryItems[slot]->GetCurCharge();
            input.totalCharge = primaryItems[slot]->GetCntCharge();
        }
    }
    const auto* hyperItem = player.GetHyperWeaponItem();
    auto& hyperInput = raceStateInput.weapons[0];
    hyperInput.visual = player.hyperWeapon;
    hyperInput.mounted = hyperItem != nullptr &&
        hyperInput.visual != originalrace::RacerRuntime::invalidWeapon &&
        hyperInput.visual < race.weapons.size();
    if (hyperInput.mounted)
    {
        hyperInput.currentCharge = hyperItem->GetCurCharge();
        hyperInput.totalCharge = hyperItem->GetCntCharge();
    }
    const auto* mineItem = player.GetMineWeaponItem();
    auto& mineInput = raceStateInput.weapons[1];
    mineInput.visual = player.mineWeapon;
    mineInput.mounted = mineItem != nullptr &&
        mineInput.visual != originalrace::RacerRuntime::invalidWeapon &&
        mineInput.visual < race.weapons.size();
    if (mineInput.mounted)
    {
        mineInput.currentCharge = mineItem->GetCurCharge();
        mineInput.totalCharge = mineItem->GetCntCharge();
    }
    playerStateFrame_.UpdateRaceState(raceStateInput);
    miniMapState_.UpdateLap(player.car.numLaps, race.lapCount);
    const auto& raceState = playerStateFrame_.GetRaceState();
    const auto placeIndex = std::min<std::size_t>(
        raceState.place > 0U ? raceState.place - 1U : 0U,
        placeNames_.size() - 1U);
    setText(device, place_, placeNames_[placeIndex],
            30.0F, true, white);
    setText(device, lap_,
            lapName_ + " " +
                std::to_string(miniMapState_.GetShownLap()) + "/" +
                std::to_string(miniMapState_.GetTotalLaps()),
            25.0F, true, white);
    for (std::size_t slot = 0; slot < weaponAmmo_.size(); ++slot)
    {
        const auto& weapon = raceState.weapons[slot + 2U];
        setText(device, weaponAmmo_[slot],
                weapon.visible
                    ? std::to_string(weapon.currentCharge) + "/" +
                          std::to_string(weapon.totalCharge)
                    : std::string{},
                18.0F, true, white);
    }
    const auto& mineState = raceState.weapons[1];
    setText(device, mineAmmo_,
            mineState.visible
                ? std::to_string(mineState.currentCharge) + "/" +
                      std::to_string(mineState.totalCharge)
                : std::string{},
            18.0F, true, white);
    const auto& hyperState = raceState.weapons[0];
    setText(device, hyperAmmo_,
            hyperState.visible
                ? std::to_string(hyperState.currentCharge) + "/" +
                      std::to_string(hyperState.totalCharge)
                : std::string{},
            18.0F, true, white);

    auto sourcePickSlot = [](originalrace::PickSlot slot) {
        switch (slot)
        {
        case originalrace::PickSlot::Primary:
            return source::HudPickSlot::Primary;
        case originalrace::PickSlot::Hyper:
            return source::HudPickSlot::Hyper;
        case originalrace::PickSlot::Mine:
            return source::HudPickSlot::Mine;
        case originalrace::PickSlot::None:
            return source::HudPickSlot::None;
        }
        return source::HudPickSlot::None;
    };
    auto pickImage = [&](source::HudPickVisual visual)
        -> const ImageAsset* {
        switch (visual)
        {
        case source::HudPickVisual::Armor:
            return &pickArmor_;
        case source::HudPickVisual::Weapon:
            return &pickAmmo_;
        case source::HudPickVisual::Hyper:
            return &pickIntro_;
        case source::HudPickVisual::Mine:
            return &pickMine_;
        case source::HudPickVisual::Money:
            return &pickMoney_;
        case source::HudPickVisual::Immortal:
            return &pickShield_;
        case source::HudPickVisual::Kill:
            return &playerKill_;
        case source::HudPickVisual::None:
            return nullptr;
        }
        return nullptr;
    };
    for (const auto& event : session.events())
    {
        source::HudPlayerEventInput input;
        input.human = humanRacer;
        input.now = uiSeconds_;
        input.viewportWidth = menu::virtualWidth;
        input.viewportHeight = menu::virtualHeight;
        if (event.kind == originalrace::RaceEventKind::Achievement &&
            event.target < achievementImages_.size() &&
            event.target < achievementPointsImages_.size())
        {
            const auto& image = achievementImages_[event.target];
            const auto& points =
                achievementPointsImages_[event.target];
            input.kind = source::HudPlayerEventKind::Achievement;
            input.slotWidth =
                std::max(image.width, points.width);
            input.slotHeight =
                image.height + points.height + 15.0F;
            input.imageHeight = image.height;
        }
        else if (event.kind == originalrace::RaceEventKind::Bonus &&
                 event.target < race.bonuses.size())
        {
            input.kind = source::HudPlayerEventKind::Pick;
            input.player = event.racer;
            input.pickVisual = source::PlayerStateFrame::ResolvePickVisual(
                race.bonuses[event.target].kind,
                sourcePickSlot(event.pickSlot));
            const auto* image = pickImage(input.pickVisual);
            if (image != nullptr)
                input.itemWidth = image->width;
        }
        else if (event.kind == originalrace::RaceEventKind::Kill)
        {
            input.kind = source::HudPlayerEventKind::Kill;
            input.player = event.racer;
            input.target = event.target;
            input.killCredit = event.killCredit;
            input.targetAvailable =
                event.target < session.racers().size();
            input.itemWidth = playerKill_.width;
        }
        else if (event.kind ==
                     originalrace::RaceEventKind::CountdownChanged)
        {
            input.kind = source::HudPlayerEventKind::Countdown;
            input.countdownImage = session.countdownStage();
        }
        else if (event.kind == originalrace::RaceEventKind::Damage)
        {
            input.kind = source::HudPlayerEventKind::Damage;
            // Portable RaceEvent stores victim in racer and attacker in
            // target; source cPlayerDamage uses the opposite field names.
            input.player = event.target;
            input.target = event.racer;
            input.value = event.value;
            input.targetAvailable =
                event.racer < session.racers().size() &&
                event.racer < vehicles.size();
        }
        else
            continue;

        const auto result = playerStateFrame_.ProcessEvent(input);
        if (result.kind == source::HudPlayerEventKind::Achievement)
        {
            achievementNotifications_.insert(
                achievementNotifications_.begin(),
                {event.target, result.item});
        }
        else if (result.kind == source::HudPlayerEventKind::Pick)
        {
            PickNotification notification;
            notification.visual = result.pickVisual;
            notification.id = result.item;
            notifications_.insert(
                notifications_.begin(), std::move(notification));
        }
        else if (result.kind == source::HudPlayerEventKind::Kill)
        {
            PickNotification notification;
            notification.visual = result.pickVisual;
            notification.target = result.target;
            notification.targetGamerId =
                session.racers()[result.target].GetGamerId();
            notification.id = result.item;
            const auto name = racerName(race, session, result.target);
            setText(device, notification.label, name, 24.0F, false,
                    {214, 214, 214, 255});
            notifications_.insert(
                notifications_.begin(), std::move(notification));
        }
        else if (result.kind ==
                 source::HudPlayerEventKind::Countdown)
        {
            hudMenuState_.OnCountdownEvent(result.countdownImage);
        }
    }
    playerStateFrame_.OnProgress(seconds, uiSeconds_);
    for (auto iterator = notifications_.begin();
         iterator != notifications_.end();)
    {
        if (playerStateFrame_.FindPickItem(iterator->id) != nullptr)
        {
            ++iterator;
            continue;
        }
        if (valid(iterator->label.texture))
            device.destroy(iterator->label.texture);
        iterator = notifications_.erase(iterator);
    }
    achievementNotifications_.erase(
        std::remove_if(
            achievementNotifications_.begin(),
            achievementNotifications_.end(),
            [&](const AchievementNotification& notification) {
                return playerStateFrame_.FindAchievmentItem(
                           notification.id) == nullptr;
            }),
        achievementNotifications_.end());
    hudMenuState_.OnProgress(seconds);

    std::vector<source::HudMiniMapPlayerInput> miniMapPlayers;
    miniMapPlayers.reserve(session.racers().size());
    for (std::size_t index = 0U;
         index < session.racers().size(); ++index)
    {
        if (session.racers()[index].disconnected)
            continue;
        // MiniMapFrame::UpdatePlayers uses CarState::GetMapPos(), which is a
        // trace projection and retains the last valid tile coordinate while
        // a car is off-road. A raw body coordinate makes an AI reset look
        // like a marker teleport across the map.
        miniMapPlayers.push_back(
            {index, session.mapPosition(index),
             session.racers()[index].GetColor()});
    }
    miniMapState_.UpdatePlayers(miniMapPlayers);

    const std::size_t visibleRacers =
        std::min({vehicles.size(), race.racers.size(),
                  session.racers().size()});
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

    for (std::size_t slot = 0U;
         slot < playerStateFrame_.GetCarLifeItems().size(); ++slot)
    {
        const auto& overlay =
            playerStateFrame_.GetCarLifeItems()[slot];
        if (overlay.racer == source::HudCarLife::invalidRacer)
            continue;
        source::HudCarLifeInput input;
        input.viewportWidth = menu::virtualWidth;
        input.viewportHeight = menu::virtualHeight;
        input.backWidth = opponentLifeBack_.width;
        input.backHeight = opponentLifeBack_.height;
        input.targetAlive =
            overlay.racer < session.racers().size() &&
            overlay.racer < vehicles.size() &&
            !session.racers()[overlay.racer].IsDestroyed();
        if (input.targetAlive)
        {
            input.atEdge = project(
                vehicles[overlay.racer].body.position, {},
                input.projected.x, input.projected.y);
            const auto& runtime = session.racers()[overlay.racer];
            input.life = std::clamp(
                runtime.GetLife() /
                    std::max(runtime.GetMaxLife(), 1.0F),
                0.0F, 1.0F);
        }
        playerStateFrame_.ProgressCarLife(slot, input, seconds);
    }
    std::vector<source::HudOpponentInput> opponentInputs;
    opponentInputs.reserve(
        visibleRacers > 0U ? visibleRacers - 1U : 0U);
    for (std::size_t racerIndex = 0U;
         racerIndex < visibleRacers; ++racerIndex)
    {
        if (racerIndex == humanRacer ||
            session.racers()[racerIndex].disconnected)
            continue;
        auto label = std::find_if(
            opponentLabels_.begin(), opponentLabels_.end(),
            [racerIndex](const OpponentLabel& candidate) {
                return candidate.racer == racerIndex;
            });
        if (label == opponentLabels_.end())
        {
            opponentLabels_.push_back({});
            label = std::prev(opponentLabels_.end());
            label->racer = racerIndex;
        }
        const auto name = racerName(race, session, racerIndex);
        const auto& runtime = session.racers()[racerIndex];
        setText(device, label->name,
                formatNamePlace(
                    namePlaceFormat_, runtime.GetPlace(), name),
                15.0F, true, white);
        source::HudOpponentInput input;
        input.racer = racerIndex;
        input.place = static_cast<int>(runtime.GetPlace());
        input.viewportWidth = menu::virtualWidth;
        input.viewportHeight = menu::virtualHeight;
        input.pointWidth = mapOpponent_.width;
        input.pointHeight = mapOpponent_.height;
        input.carLifeBackWidth = opponentLifeBack_.width;
        input.carLifeBackHeight = opponentLifeBack_.height;
        input.labelWidth = label->name.width;
        input.labelHeight = label->name.height;
        input.labelAabbMinY = -label->name.height * 0.5F;
        input.atEdge = project(
            vehicles[racerIndex].body.position,
            {1.0F, -0.5F, 0.0F}, input.projected.x,
            input.projected.y);
        input.targetAlive = !runtime.IsDestroyed();
        opponentInputs.push_back(input);
    }
    for (auto label = opponentLabels_.begin();
         label != opponentLabels_.end();)
    {
        const auto input = std::find_if(
            opponentInputs.begin(), opponentInputs.end(),
            [&](const source::HudOpponentInput& candidate) {
                return candidate.racer == label->racer;
            });
        if (input != opponentInputs.end())
        {
            ++label;
            continue;
        }
        if (valid(label->name.texture))
            device.destroy(label->name.texture);
        label = opponentLabels_.erase(label);
    }
    playerStateFrame_.ProgressOpponents(opponentInputs, seconds);

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
    const auto& miniMapGeometry = miniMapState_.GetGeometry();
    drawAsset(device, quad, shader, mapStart_.texture,
              miniMapGeometry.startWidth,
              miniMapGeometry.startHeight,
              miniMapGeometry.start.x, miniMapGeometry.start.y,
              66.0F, pipeline, miniMapGeometry.startAngle);
    for (const auto& marker : miniMapState_.GetPlayers())
    {
        // Legacy Plane3d renders from -size to +size, so SetSize(10, 10)
        // corresponds to a 20x20 marker in the final viewport.
        drawTintedAsset(device, quad, shader, mapPlayer_.texture,
                        20.0F, 20.0F, marker.position.x,
                        marker.position.y, 58.0F, pipeline,
                        marker.color);
    }
    for (const auto& opponent : playerStateFrame_.GetOpponents())
    {
        if (!opponent.visible)
            continue;
        const auto label = std::find_if(
            opponentLabels_.begin(), opponentLabels_.end(),
            [&](const OpponentLabel& candidate) {
                return candidate.racer == opponent.racer;
            });
        if (label == opponentLabels_.end())
            continue;
        const std::array<float, 4> tint{
            1.0F, 1.0F, 1.0F, opponent.alpha};
        drawTintedAsset(
            device, quad, shader, mapOpponent_.texture,
            mapOpponent_.width, mapOpponent_.height,
            opponent.pointPosition.x, opponent.pointPosition.y,
            44.0F, pipeline, tint);
        drawTintedAsset(
            device, quad, shader, label->name.texture,
            label->name.width, label->name.height,
            opponent.labelPosition.x, opponent.labelPosition.y,
            36.0F, pipeline, tint);
    }
    for (const auto& overlay : playerStateFrame_.GetCarLifeItems())
    {
        if (!overlay.visible)
            continue;
        const std::array<float, 4> backgroundTint{
            1.0F, 1.0F, 1.0F, overlay.backgroundAlpha};
        drawTintedAsset(
            device, quad, shader, opponentLifeBack_.texture,
            opponentLifeBack_.width, opponentLifeBack_.height,
            overlay.position.x, overlay.position.y, 42.0F, pipeline,
            backgroundTint);
        const float width = opponentLifeBar_.width * overlay.life;
        drawTintedAsset(
            device, quad, shader, opponentLifeBar_.texture, width,
            opponentLifeBar_.height,
            overlay.position.x - opponentLifeBar_.width * 0.5F +
                width * 0.5F,
            overlay.position.y, 34.0F, pipeline,
            {1.0F, 1.0F, 1.0F, overlay.barAlpha});
    }

    // PlayerStateFrame::_raceState and MiniMapFrame's lap widgets obey
    // enableHUD. Their sibling widgets (map, markers, event overlays and
    // countdown) deliberately remain visible in the Windows implementation.
    if (enableRaceState)
    {
        const auto& raceState = playerStateFrame_.GetRaceState();
        const auto placePos = source::HudMenu::GetPlacePos();
        const auto lifePos = source::HudMenu::GetLifeBarPos();
        const auto lapPos = source::HudMenu::GetLapPos();
        drawAsset(device, quad, shader, placeFrame_.texture,
                  placeFrame_.width, placeFrame_.height,
                  placeFrame_.width * 0.5F,
                  placeFrame_.height * 0.5F,
                  38.0F, pipeline);
        drawAsset(device, quad, shader, place_.texture, place_.width,
                  place_.height, placePos.x, placePos.y, 28.0F,
                  pipeline);
        drawAsset(device, quad, shader, lifeBack_.texture,
                  lifeBack_.width, lifeBack_.height,
                  lifePos.x + lifeBack_.width * 0.5F,
                  lifePos.y + lifeBack_.height * 0.5F, 28.0F,
                  pipeline);
        const float lifeWidth = lifeBar_.width * raceState.life;
        drawAsset(device, quad, shader, lifeBar_.texture, lifeWidth,
                  lifeBar_.height,
                  lifePos.x + lifeBack_.width * 0.5F -
                      lifeBar_.width * 0.5F + lifeWidth * 0.5F,
                  lifePos.y + lifeBack_.height * 0.5F,
                  18.0F, pipeline);

        for (const auto& weapon : raceState.weapons)
        {
            if (!weapon.visible)
                continue;
            const TextAsset* ammo = nullptr;
            if (weapon.primary)
            {
                const auto& image = weapon.selected
                    ? weaponSlotSelected_ : weaponSlot_;
                drawAsset(
                    device, quad, shader, image.texture, image.width,
                    image.height, weapon.boxPosition.x,
                    weapon.boxPosition.y, 34.0F, pipeline);
                const std::size_t primarySlot = weapon.type - 2U;
                if (primarySlot < weaponAmmo_.size())
                    ammo = &weaponAmmo_[primarySlot];
            }
            else
            {
                ammo = weapon.type == 0U ? &hyperAmmo_ : &mineAmmo_;
            }
            drawWeaponVisual(
                weapon.visual, weapon.viewPosition.x,
                weapon.viewPosition.y, 28.0F);
            if (ammo != nullptr)
            {
                drawAsset(
                    device, quad, shader, ammo->texture, ammo->width,
                    ammo->height, weapon.labelPosition.x,
                    weapon.labelPosition.y, 22.0F, pipeline);
            }
        }

        drawAsset(device, quad, shader, lapBack_.texture,
                  lapBack_.width, lapBack_.height,
                  lapPos.x + lapBack_.width * 0.5F, lapPos.y,
                  34.0F, pipeline);
        drawAsset(device, quad, shader, lap_.texture, lap_.width,
                  lap_.height,
                  lapPos.x + lapBack_.width * 0.5F - 10.0F,
                  lapPos.y + 1.0F, 22.0F, pipeline);
    }

    for (const auto& item : notifications_)
    {
        const auto* state = playerStateFrame_.FindPickItem(item.id);
        if (state == nullptr)
            continue;
        const ImageAsset* notification = nullptr;
        switch (item.visual)
        {
        case source::HudPickVisual::Armor:
            notification = &pickArmor_;
            break;
        case source::HudPickVisual::Weapon:
            notification = &pickAmmo_;
            break;
        case source::HudPickVisual::Hyper:
            notification = &pickIntro_;
            break;
        case source::HudPickVisual::Mine:
            notification = &pickMine_;
            break;
        case source::HudPickVisual::Money:
            notification = &pickMoney_;
            break;
        case source::HudPickVisual::Immortal:
            notification = &pickShield_;
            break;
        case source::HudPickVisual::Kill:
            notification = &playerKill_;
            break;
        case source::HudPickVisual::None:
            break;
        }
        if (notification != nullptr)
        {
            drawTintedAsset(
                device, quad, shader, notification->texture,
                notification->width, notification->height,
                state->position.x, state->position.y, 20.0F, pipeline,
                {1.0F, 1.0F, 1.0F, state->alpha});
            if (const auto* photo =
                    racerPhoto(item.targetGamerId, item.target))
            {
                drawTintedAsset(
                    device, quad, shader,
                    photo->texture, 50.0F, 50.0F,
                    state->position.x - 40.0F, state->position.y,
                    18.0F, pipeline,
                    {1.0F, 1.0F, 1.0F, state->alpha});
                drawTintedAsset(
                    device, quad, shader, item.label.texture,
                    item.label.width, item.label.height,
                    state->position.x - 15.0F +
                        item.label.width * 0.5F,
                    state->position.y, 16.0F, pipeline,
                    {1.0F, 1.0F, 1.0F, state->alpha});
            }
        }
    }

    for (const auto& item : achievementNotifications_)
    {
        const auto* state =
            playerStateFrame_.FindAchievmentItem(item.id);
        if (state == nullptr)
            continue;
        if (item.achievement >= achievementImages_.size() ||
            item.achievement >= achievementPointsImages_.size())
            continue;
        const auto& image = achievementImages_[item.achievement];
        const auto& points =
            achievementPointsImages_[item.achievement];
        drawTintedAsset(
            device, quad, shader, image.texture,
            image.width * state->scale, image.height * state->scale,
            state->position.x, state->position.y, 12.0F, pipeline,
            {1.0F, 1.0F, 1.0F, state->alpha});
        if (campaign_)
        {
            const float pointsY =
                state->position.y + image.height * 0.5F + 15.0F +
                points.height * 0.5F;
            drawTintedAsset(
                device, quad, shader, points.texture,
                points.width, points.height, state->position.x, pointsY,
                10.0F, pipeline,
                {1.0F, 1.0F, 1.0F, state->pointsAlpha});
            if (valid(achievementMultiplierImage_.texture))
            {
                drawTintedAsset(
                    device, quad, shader,
                    achievementMultiplierImage_.texture,
                    achievementMultiplierImage_.width,
                    achievementMultiplierImage_.height,
                    state->position.x + 95.0F, pointsY - 3.0F,
                    8.0F, pipeline,
                    {1.0F, 1.0F, 1.0F, state->pointsAlpha});
            }
        }
    }

    const auto& countdown = hudMenuState_.GetCountdownVisual();
    if (countdown.image >= 0 &&
        static_cast<std::size_t>(countdown.image) <
            countdownImages_.size())
    {
        const auto& image =
            countdownImages_[static_cast<std::size_t>(countdown.image)];
        const float growth = 200.0F * countdown.growthSeconds;
        drawTintedAsset(
            device, quad, shader, image.texture,
            image.width * 0.5F + growth,
            image.height * 0.5F + growth,
            menu::virtualWidth * 0.5F,
            menu::virtualHeight * 0.5F, 4.0F, pipeline,
            {1.0F, 1.0F, 1.0F, countdown.alpha});
    }
}

} // namespace rrr3d::race
