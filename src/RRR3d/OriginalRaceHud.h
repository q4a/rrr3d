#pragma once

#include "OriginalGameData.h"
#include "OriginalHudMenu.h"
#include "OriginalMainMenu.h"
#include "OriginalRaceSession.h"
#include "OriginalResourceManager.h"
#include "renderer/Renderer.h"

#include <array>
#include <cstddef>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace rrr3d::race
{

class OriginalRaceHud
{
public:
    bool initialize(
        r3d::renderer::GraphicsDevice& device,
        OriginalResourceManager& resources,
        const r3d::game::originalgamedata::Catalog& gameData,
        const r3d::game::originalrace::Race& race,
        std::string_view language, std::string_view difficulty,
        bool campaign, std::string& error);
    void shutdown(r3d::renderer::GraphicsDevice& device) noexcept;
    void update(r3d::renderer::GraphicsDevice& device,
                const r3d::game::originalrace::Race& race,
                const r3d::game::originalrace::OriginalRaceSession& session,
                const std::vector<r3d::physics::VehicleState>& vehicles,
                const r3d::renderer::Camera& camera,
                float seconds);
    void draw(r3d::renderer::GraphicsDevice& device,
              r3d::renderer::Mesh quad,
              r3d::renderer::Shader shader,
              r3d::renderer::Shader meshShader,
              bool enableRaceState = true) const;
    [[nodiscard]] std::string_view localizedLapName() const noexcept;
    [[nodiscard]] std::string_view localizedPriceName() const noexcept;
    [[nodiscard]] r3d::game::originalrace::source::HudMenuCommand
    handleEscape(bool active, bool repeated, bool paused) const noexcept;

private:
    struct ImageAsset
    {
        r3d::renderer::Texture texture;
        float width = 0.0F;
        float height = 0.0F;
    };

    struct TextAsset
    {
        r3d::renderer::Texture texture;
        float width = 0.0F;
        float height = 0.0F;
        std::string value;
    };

    struct OpponentLabel
    {
        TextAsset name;
        std::size_t racer =
            r3d::game::originalrace::source::HudCarLife::invalidRacer;
    };

    struct WeaponVisual
    {
        r3d::renderer::Mesh mesh;
        std::vector<r3d::renderer::Texture> textures;
        std::vector<r3d::game::originalrace::MaterialDefinition>
            materials;
        std::vector<r3d::renderer::DrawRange> groups;
    };

    struct PickNotification
    {
        r3d::game::originalrace::source::HudPickVisual visual =
            r3d::game::originalrace::source::HudPickVisual::None;
        std::size_t target = std::numeric_limits<std::size_t>::max();
        int targetGamerId = -1;
        TextAsset label;
        r3d::game::originalrace::source::HudItemId id = 0U;
    };

    struct FinishRow
    {
        TextAsset name;
        TextAsset value;
        std::size_t racer = std::numeric_limits<std::size_t>::max();
        int gamerId = -1;
    };

    struct AchievementNotification
    {
        std::size_t achievement = 0;
        r3d::game::originalrace::source::HudItemId id = 0U;
    };

    bool loadImage(r3d::renderer::GraphicsDevice& device,
                   OriginalResourceManager& resources,
                   std::string path, ImageAsset& output,
                   std::string& error);
    void setText(r3d::renderer::GraphicsDevice& device,
                 TextAsset& output, std::string value, float pointSize,
                 bool bold,
                 r3d::game::mainmenu2::Rgba8 color);
    void buildMiniMap(
        r3d::renderer::GraphicsDevice& device,
        const r3d::game::originalrace::Race& race);
    std::string racerName(
        const r3d::game::originalrace::Race& race,
        const r3d::game::originalrace::OriginalRaceSession& session,
        std::size_t racer) const;
    const ImageAsset* racerPhoto(int gamerId,
                                 std::size_t racer) const noexcept;

    ImageAsset placeFrame_;
    ImageAsset lifeBack_;
    ImageAsset lifeBar_;
    ImageAsset lapBack_;
    ImageAsset mapStrip_;
    ImageAsset mapPlayer_;
    ImageAsset mapOpponent_;
    ImageAsset mapStart_;
    ImageAsset weaponSlot_;
    ImageAsset weaponSlotSelected_;
    ImageAsset pickArmor_;
    ImageAsset pickAmmo_;
    ImageAsset pickIntro_;
    ImageAsset pickMine_;
    ImageAsset pickMoney_;
    ImageAsset pickShield_;
    ImageAsset playerKill_;
    ImageAsset opponentLifeBack_;
    ImageAsset opponentLifeBar_;
    std::array<ImageAsset, 5> countdownImages_;
    TextAsset place_;
    TextAsset lap_;
    std::array<TextAsset,
               r3d::game::originalrace::PlayerProfile::weaponSlotCount>
        weaponAmmo_;
    TextAsset mineAmmo_;
    TextAsset hyperAmmo_;
    r3d::renderer::Mesh mapMesh_;
    r3d::game::originalrace::source::MiniMapFrame miniMapState_;
    std::vector<OpponentLabel> opponentLabels_;
    std::vector<WeaponVisual> weaponVisuals_;
    ImageAsset finishLeftFrame_;
    ImageAsset finishRightFrame_;
    ImageAsset finishLineFrame_;
    std::array<ImageAsset, 3> finishCups_;
    std::vector<ImageAsset> racerPhotos_;
    std::map<int, ImageAsset> gamerPhotos_;
    std::vector<ImageAsset> achievementImages_;
    std::vector<ImageAsset> achievementPointsImages_;
    ImageAsset achievementMultiplierImage_;
    TextAsset finishPrice_;
    TextAsset finishMoneyPoints_;
    std::array<FinishRow, 3> finishRows_;
    std::vector<PickNotification> notifications_;
    std::vector<AchievementNotification> achievementNotifications_;
    r3d::game::originalrace::source::PlayerStateFrame playerStateFrame_;
    r3d::game::originalrace::source::HudMenu hudMenuState_;
    std::array<std::string, 8> placeNames_{
        "1st", "2nd", "3rd", "4th",
        "5th", "6th", "7th", "8th"};
    std::string lapName_ = "Lap";
    std::string namePlaceFormat_ = "%d place\n%s";
    std::string priceName_ = "Reward";
    std::string moneyName_ = "Money";
    std::string pointsName_ = "Points";
    std::vector<std::string> localizedRacerNames_;
    std::map<int, std::string> localizedGamerNames_;
    float uiSeconds_ = 0.0F;
    float finishStarted_ = -1.0F;
    bool finishVisible_ = false;
    bool campaign_ = true;
};

} // namespace rrr3d::race
