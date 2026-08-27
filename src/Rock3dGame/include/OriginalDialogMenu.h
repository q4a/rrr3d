#pragma once

#include "OriginalMenuSystem.h"

#include <optional>
#include <string>

namespace r3d::game::originalmenu
{

struct AcceptLayout
{
    Vec2 frameSize{};
    Vec2 infoSize{325.0F, 65.0F};
    Vec2 infoOffset{0.0F, -25.0F};
    Vec2 buttonSize{};
    Vec2 yesOffset{-70.0F, 32.0F};
    Vec2 noOffset{70.0F, 32.0F};
};

struct AcceptDialogState
{
    bool visible = false;
    bool resultYes = false;
    bool yesFocused = true;
    bool maxMode = false;
    bool disableFocus = false;
    std::optional<bool> hoveredChoice;
    std::string message;
    std::string yesText;
    std::string noText;
    Vec2 center{};
    AcceptLayout layout{};
};

struct WeaponDialogState
{
    bool visible = false;
    std::string title;
    std::string message;
    std::string moneyText;
    std::string damageText;
    Vec2 center{};
    Vec2 frameSize{};
    Vec2 infoOffset{3.0F, -3.0F};
    Vec2 infoSize{280.0F, 75.0F};
    Vec2 moneyOffset{-60.0F, 54.0F};
    Vec2 damageOffset{80.0F, 54.0F};
    Vec2 titleOffset{0.0F, -58.0F};
};

struct InfoDialogState
{
    bool visible = false;
    bool dismissable = true;
    std::string title;
    std::string message;
    std::string okText;
    Vec2 center{};
    Vec2 frameSize{};
    Vec2 titleOffset{-27.0F, -105.0F};
    Vec2 infoOffset{0.0F, 5.0F};
    Vec2 infoSize{245.0F, 135.0F};
    Vec2 okOffset{0.0F, 105.0F};
};

struct MusicDialogState
{
    bool visible = false;
    std::string title;
    std::string message;
    Vec2 frameSize{};
    Vec2 center{};
    Vec2 titleOffset{-130.0F, -21.0F};
    Vec2 infoOffset{-130.0F, 17.0F};
    Vec2 infoSize{290.0F, 45.0F};
    float offset = 0.0F;
};

// Backend-neutral state from DialogMenu2 plus Menu's delay/animation owner.
// GPU text and image handles deliberately stay outside this class.
class DialogSystem
{
  public:
    explicit DialogSystem(MenuSystem& menu);

    const AcceptDialogState& ShowAccept(
        std::string message, std::string yesText, std::string noText,
        Vec2 position, Anchor anchor, Vec2 sourceFrameSize,
        Vec2 sourceButtonSize, bool maxButtonsSize = false,
        bool maxMode = false, bool disableFocus = false);
    void SetAcceptVisible(bool visible);
    void SetAcceptFocus(bool yes) noexcept;
    void SetAcceptHover(std::optional<bool> yes) noexcept;
    bool ChooseAccept(bool yes);
    void HideAccept();
    const AcceptDialogState& Accept() const noexcept;

    const WeaponDialogState& ShowWeapon(
        std::string title, std::string message, std::string moneyText,
        std::string damageText, Vec2 position, Anchor anchor,
        Vec2 frameSize, float timeDelay);
    void SetWeaponVisible(bool visible);
    void HideWeapon();
    const WeaponDialogState& Weapon() const noexcept;

    const InfoDialogState& ShowInfo(
        std::string title, std::string message, std::string okText,
        Vec2 position, Anchor anchor, Vec2 frameSize,
        float timeDelay = 0.0F, bool okButton = true);
    void SetInfoDismissable(bool dismissable);
    void HideInfo();
    const InfoDialogState& Info() const noexcept;

    const MusicDialogState& ShowMusicInfo(
        std::string title, std::string message, Vec2 frameSize);
    void HideMusicInfo();
    const MusicDialogState& Music() const noexcept;

    void Progress(float deltaTime, Vec2 viewport);

  private:
    MenuSystem* menu_ = nullptr;
    AcceptDialogState accept_{};
    WeaponDialogState weapon_{};
    InfoDialogState info_{};
    MusicDialogState music_{};
    float weaponTime_ = -1.0F;
    float messageTime_ = -1.0F;
    float musicTime_ = -1.0F;
};

} // namespace r3d::game::originalmenu
