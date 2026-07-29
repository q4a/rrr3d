#pragma once

#include "OriginalMainMenu.h"
#include "OriginalRaceSession.h"
#include "renderer/Renderer.h"

#include <array>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace r3d::resource
{
class ResourceFileSystem;
}

namespace rrr3d::race
{

class OriginalRaceHud
{
public:
    bool initialize(
        r3d::renderer::GraphicsDevice& device,
        const r3d::resource::ResourceFileSystem& resources,
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
              r3d::renderer::Shader meshShader) const;

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

    struct MiniMapMarker
    {
        float x = 0.0F;
        float y = 0.0F;
        float angle = 0.0F;
        std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
    };

    struct OpponentLabel
    {
        TextAsset name;
        float x = 0.0F;
        float y = 0.0F;
        float radius = 0.0F;
        float alpha = 1.0F;
        bool visible = false;
    };

    struct CarLifeOverlay
    {
        std::size_t racer = std::numeric_limits<std::size_t>::max();
        float visibleUntil = 0.0F;
        float duration = 0.0F;
        float x = 0.0F;
        float y = 0.0F;
        float life = 1.0F;
        float alpha = 0.0F;
        bool visible = false;
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
        r3d::game::originalrace::BonusKind kind =
            r3d::game::originalrace::BonusKind::Unknown;
        r3d::game::originalrace::PickSlot slot =
            r3d::game::originalrace::PickSlot::None;
        std::size_t target = std::numeric_limits<std::size_t>::max();
        TextAsset label;
        float started = 0.0F;
        float x = 0.0F;
        float y = 0.0F;
        float targetX = 0.0F;
        float alpha = 0.0F;
    };

    struct FinishRow
    {
        TextAsset name;
        TextAsset value;
        std::size_t racer = std::numeric_limits<std::size_t>::max();
    };

    struct AchievementNotification
    {
        std::size_t achievement = 0;
        float started = 0.0F;
        float startX = 0.0F;
        float startY = 0.0F;
        float x = 0.0F;
        float y = 0.0F;
        float alpha = 1.0F;
        float pointsAlpha = 0.0F;
        float scale = 1.0F;
    };

    bool loadImage(r3d::renderer::GraphicsDevice& device,
                   const r3d::resource::ResourceFileSystem& resources,
                   std::string path, ImageAsset& output,
                   std::string& error);
    void setText(r3d::renderer::GraphicsDevice& device,
                 TextAsset& output, std::string value, float pointSize,
                 bool bold,
                 r3d::game::mainmenu2::Rgba8 color);
    void buildMiniMap(
        r3d::renderer::GraphicsDevice& device,
        const r3d::game::originalrace::Race& race);

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
    ImageAsset mineSlot_;
    ImageAsset hyperSlot_;
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
    std::vector<MiniMapMarker> mapMarkers_;
    std::vector<OpponentLabel> opponentLabels_;
    std::array<CarLifeOverlay, 2> carLifeOverlays_;
    std::vector<WeaponVisual> weaponVisuals_;
    ImageAsset finishLeftFrame_;
    ImageAsset finishRightFrame_;
    ImageAsset finishLineFrame_;
    std::array<ImageAsset, 3> finishCups_;
    std::vector<ImageAsset> racerPhotos_;
    std::vector<ImageAsset> achievementImages_;
    std::vector<ImageAsset> achievementPointsImages_;
    ImageAsset achievementMultiplierImage_;
    TextAsset finishPrice_;
    TextAsset finishMoneyPoints_;
    std::array<FinishRow, 3> finishRows_;
    float mapMinimumX_ = 0.0F;
    float mapMinimumY_ = 0.0F;
    float mapMaximumY_ = 0.0F;
    float mapScale_ = 1.0F;
    float mapOriginX_ = 0.0F;
    float mapOriginY_ = 0.0F;
    float startX_ = 0.0F;
    float startY_ = 0.0F;
    float startAngle_ = 0.0F;
    float startWidth_ = 0.0F;
    float startHeight_ = 0.0F;
    std::vector<PickNotification> notifications_;
    std::vector<AchievementNotification> achievementNotifications_;
    int countdownImage_ = -1;
    float countdownAlpha_ = 1.0F;
    float countdownGrowthSeconds_ = 0.0F;
    float countdownFinishUntil_ = 0.0F;
    float lifeFraction_ = 1.0F;
    std::array<bool,
               r3d::game::originalrace::PlayerProfile::weaponSlotCount>
        weaponVisible_{};
    std::array<std::size_t,
               r3d::game::originalrace::PlayerProfile::weaponSlotCount>
        weaponVisualIndices_{
            r3d::game::originalrace::RacerRuntime::invalidWeapon,
            r3d::game::originalrace::RacerRuntime::invalidWeapon,
            r3d::game::originalrace::RacerRuntime::invalidWeapon,
            r3d::game::originalrace::RacerRuntime::invalidWeapon};
    std::size_t mineVisualIndex_ =
        r3d::game::originalrace::RacerRuntime::invalidWeapon;
    std::size_t hyperVisualIndex_ =
        r3d::game::originalrace::RacerRuntime::invalidWeapon;
    std::size_t selectedWeaponSlot_ = 0;
    std::array<std::string, 8> placeNames_{
        "1st", "2nd", "3rd", "4th",
        "5th", "6th", "7th", "8th"};
    std::string lapName_ = "Lap";
    std::string namePlaceFormat_ = "%d place\n%s";
    std::string priceName_ = "Reward";
    std::string moneyName_ = "Money";
    std::string pointsName_ = "Points";
    std::vector<std::string> localizedRacerNames_;
    float uiSeconds_ = 0.0F;
    std::size_t achievementSerial_ = 0;
    float finishStarted_ = -1.0F;
    bool finishVisible_ = false;
    bool campaign_ = true;
};

} // namespace rrr3d::race
