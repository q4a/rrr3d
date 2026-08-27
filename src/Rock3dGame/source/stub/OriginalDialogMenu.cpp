#include "OriginalDialogMenu.h"

#include <algorithm>

namespace r3d::game::originalmenu
{

DialogSystem::DialogSystem(MenuSystem& menu) : menu_(&menu)
{
}

const AcceptDialogState& DialogSystem::ShowAccept(
    std::string message, std::string yesText, std::string noText,
    Vec2 position, Anchor anchor, Vec2 sourceFrameSize,
    Vec2 sourceButtonSize, bool maxButtonsSize, bool maxMode,
    bool disableFocus)
{
    accept_.message = std::move(message);
    accept_.yesText = std::move(yesText);
    accept_.noText = std::move(noText);
    accept_.maxMode = maxMode;
    accept_.disableFocus = disableFocus;
    accept_.yesFocused = true;
    accept_.hoveredChoice.reset();
    accept_.layout = {};
    accept_.layout.frameSize = sourceFrameSize;
    accept_.layout.buttonSize = sourceButtonSize;
    if (maxMode)
    {
        accept_.layout.frameSize.x *= 1.7F;
        accept_.layout.frameSize.y *= 1.7F;
        accept_.layout.infoSize.x *= 1.7F;
        accept_.layout.infoSize.y *= 1.7F;
        accept_.layout.buttonSize.x *= 1.5F;
        accept_.layout.yesOffset = {-100.0F, 72.0F};
        accept_.layout.noOffset = {100.0F, 72.0F};
    }
    if (maxButtonsSize)
    {
        accept_.layout.buttonSize.x *= 1.5F;
        accept_.layout.yesOffset.x -= 10.0F;
        accept_.layout.noOffset.x += 10.0F;
    }
    accept_.center = menu_->SetPos(
        FrameId::Accept, position, anchor, accept_.layout.frameSize,
        menu_->Frame(FrameId::Screen).viewport);
    accept_.visible = true;
    menu_->ShowModal(FrameId::Accept, true);
    return accept_;
}

void DialogSystem::SetAcceptVisible(bool visible)
{
    accept_.visible = visible;
    menu_->ShowModal(FrameId::Accept, visible);
}

void DialogSystem::SetAcceptFocus(bool yes) noexcept
{
    accept_.yesFocused = yes;
}

void DialogSystem::SetAcceptHover(std::optional<bool> yes) noexcept
{
    accept_.hoveredChoice = yes;
}

bool DialogSystem::ChooseAccept(bool yes)
{
    accept_.resultYes = yes;
    HideAccept();
    return accept_.resultYes;
}

void DialogSystem::HideAccept()
{
    accept_.visible = false;
    accept_.hoveredChoice.reset();
    menu_->ShowModal(FrameId::Accept, false);
}

const AcceptDialogState& DialogSystem::Accept() const noexcept
{
    return accept_;
}

const WeaponDialogState& DialogSystem::ShowWeapon(
    std::string title, std::string message, std::string moneyText,
    std::string damageText, Vec2 position, Anchor anchor, Vec2 frameSize,
    float timeDelay)
{
    weapon_.title = std::move(title);
    weapon_.message = std::move(message);
    weapon_.moneyText = std::move(moneyText);
    weapon_.damageText = std::move(damageText);
    weapon_.frameSize = frameSize;
    weapon_.center = menu_->SetPos(
        FrameId::Weapon, position, anchor, frameSize,
        menu_->Frame(FrameId::Screen).viewport);
    menu_->Show(FrameId::Weapon, true);
    if (weaponTime_ != -2.0F)
    {
        weapon_.visible = false;
        menu_->Show(FrameId::Weapon, false);
        weaponTime_ = timeDelay;
    }
    else
    {
        weapon_.visible = true;
    }
    return weapon_;
}

void DialogSystem::SetWeaponVisible(bool visible)
{
    weapon_.visible = visible;
    menu_->Show(FrameId::Weapon, visible);
}

void DialogSystem::HideWeapon()
{
    weapon_.visible = false;
    weaponTime_ = -1.0F;
    menu_->Show(FrameId::Weapon, false);
}

const WeaponDialogState& DialogSystem::Weapon() const noexcept
{
    return weapon_;
}

const InfoDialogState& DialogSystem::ShowInfo(
    std::string title, std::string message, std::string okText,
    Vec2 position, Anchor anchor, Vec2 frameSize, float timeDelay,
    bool okButton)
{
    info_.title = std::move(title);
    info_.message = std::move(message);
    info_.okText = std::move(okText);
    info_.frameSize = frameSize;
    info_.dismissable = okButton;
    info_.center = menu_->SetPos(
        FrameId::Message, position, anchor, frameSize,
        menu_->Frame(FrameId::Screen).viewport);
    info_.visible = timeDelay == 0.0F;
    messageTime_ = timeDelay == 0.0F ? -1.0F : timeDelay;
    menu_->ShowModal(FrameId::Message, info_.visible);
    return info_;
}

void DialogSystem::SetInfoDismissable(bool dismissable)
{
    info_.dismissable = dismissable;
}

void DialogSystem::HideInfo()
{
    info_.visible = false;
    messageTime_ = -1.0F;
    menu_->ShowModal(FrameId::Message, false);
}

const InfoDialogState& DialogSystem::Info() const noexcept
{
    return info_;
}

const MusicDialogState& DialogSystem::ShowMusicInfo(
    std::string title, std::string message, Vec2 frameSize)
{
    music_.title = std::move(title);
    music_.message = std::move(message);
    music_.frameSize = frameSize;
    menu_->Show(FrameId::Music, true);
    if (musicTime_ == -1.0F)
    {
        music_.visible = false;
        menu_->Show(FrameId::Music, false);
        musicTime_ = -0.999F;
    }
    return music_;
}

void DialogSystem::HideMusicInfo()
{
    music_.visible = false;
    music_.offset = 0.0F;
    musicTime_ = -1.0F;
    menu_->Show(FrameId::Music, false);
}

const MusicDialogState& DialogSystem::Music() const noexcept
{
    return music_;
}

void DialogSystem::Progress(float deltaTime, Vec2 viewport)
{
    if (weaponTime_ >= 0.0F &&
        (weaponTime_ -= deltaTime) <= 0.0F)
    {
        weaponTime_ = -2.0F;
        weapon_.visible = true;
        menu_->Show(FrameId::Weapon, true);
    }

    if (messageTime_ >= 0.0F &&
        (messageTime_ -= deltaTime) <= 0.0F)
    {
        messageTime_ = -1.0F;
        info_.visible = true;
        menu_->ShowModal(FrameId::Message, true);
    }

    music_.visible = false;
    music_.offset = 0.0F;
    if (musicTime_ == -1.0F)
        return;

    constexpr float delay = 1.0F;
    constexpr float life = 3.0F;
    music_.offset =
        std::clamp(musicTime_ / delay, 0.0F, 1.0F) -
        std::clamp((musicTime_ - delay - life) / delay, 0.0F, 1.0F);
    const Vec2 size = music_.frameSize;
    music_.center = {
        -5.0F + (40.0F + size.x) * music_.offset - size.x * 0.5F,
        viewport.y - 30.0F - size.y * 0.5F};

    if (musicTime_ >= life + 2.0F * delay)
    {
        HideMusicInfo();
    }
    else
    {
        musicTime_ += deltaTime;
        music_.visible = true;
        menu_->Show(FrameId::Music, true);
    }
}

} // namespace r3d::game::originalmenu
