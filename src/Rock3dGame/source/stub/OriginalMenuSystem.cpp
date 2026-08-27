#include "OriginalMenuSystem.h"

#include <algorithm>
#include <cassert>

namespace r3d::game::originalmenu
{

ScreenStack::ScreenStack(MenuSystem& owner)
    : owner_(&owner), values_{MenuScreen::Main}
{
}

MenuScreen& ScreenStack::back()
{
    assert(!values_.empty());
    return values_.back();
}

const MenuScreen& ScreenStack::back() const
{
    assert(!values_.empty());
    return values_.back();
}

bool ScreenStack::empty() const noexcept
{
    return values_.empty();
}

std::size_t ScreenStack::size() const noexcept
{
    return values_.size();
}

void ScreenStack::set_back(MenuScreen screen)
{
    if (values_.empty())
    {
        values_.push_back(screen);
        changed();
        return;
    }
    if (values_.back() == screen)
        return;
    values_.back() = screen;
    changed();
}

void ScreenStack::push_back(MenuScreen screen)
{
    values_.push_back(screen);
    changed();
}

void ScreenStack::pop_back()
{
    if (values_.size() <= 1U)
        return;
    values_.pop_back();
    changed();
}

void ScreenStack::clear()
{
    if (values_.empty())
        return;
    values_.clear();
    changed();
}

ScreenStack& ScreenStack::operator=(
    std::initializer_list<MenuScreen> screens)
{
    values_.assign(screens.begin(), screens.end());
    if (values_.empty())
        values_.push_back(MenuScreen::Main);
    changed();
    return *this;
}

ScreenStack& ScreenStack::operator=(
    const std::vector<MenuScreen>& screens)
{
    values_ = screens;
    if (values_.empty())
        values_.push_back(MenuScreen::Main);
    changed();
    return *this;
}

const std::vector<MenuScreen>& ScreenStack::values() const noexcept
{
    return values_;
}

void ScreenStack::changed()
{
    if (owner_ != nullptr)
        owner_->MarkInputReset();
}

MenuSystem::MenuSystem() : screens_(*this)
{
    ApplyState();
}

MenuState MenuSystem::State() const noexcept
{
    return state_;
}

const MenuVisibility& MenuSystem::Visibility() const noexcept
{
    return visibility_;
}

void MenuSystem::SetState(MenuState state)
{
    if (state_ == state)
        return;

    state_ = state;
    // Menu::ApplyState always closes these transient source frames.
    ShowModal(FrameId::Accept, false);
    Show(FrameId::Music, false);
    MarkInputReset();
    ApplyState();
}

void MenuSystem::SetLoadingVisible(bool visible)
{
    if (loadingVisible_ == visible)
        return;
    loadingVisible_ = visible;
    ApplyState();
}

void MenuSystem::SetOptionsVisible(bool visible)
{
    if (optionsVisible_ == visible)
        return;
    optionsVisible_ = visible;
    ApplyState();
}

void MenuSystem::SetStartOptionsVisible(bool visible)
{
    if (startOptionsVisible_ == visible)
        return;
    startOptionsVisible_ = visible;
    ApplyState();
}

void MenuSystem::Show(FrameId frame, bool visible)
{
    auto& state = frames_[index(frame)];
    if (state.visible == visible)
        return;
    state.visible = visible;
    ++state.showRevision;
    if (visible)
        Invalidate(frame);
}

void MenuSystem::ShowModal(FrameId frame, bool visible, int level)
{
    Show(frame, visible);
    auto& state = frames_[index(frame)];
    const bool modal = visible;
    const int topmost = visible ? level : topmostDefault;
    if (state.modal != modal || state.topmost != topmost)
    {
        state.modal = modal;
        state.topmost = topmost;
        if (visible)
            state.modalOrder = ++modalOrder_;
        ++state.showRevision;
    }
}

bool MenuSystem::Visible(FrameId frame) const noexcept
{
    return frames_[index(frame)].visible;
}

const FrameState& MenuSystem::Frame(FrameId frame) const noexcept
{
    return frames_[index(frame)];
}

void MenuSystem::AdjustLayout(FrameId frame, Vec2 viewport)
{
    auto& state = frames_[index(frame)];
    if (!state.visible)
        return;
    state.viewport = viewport;
    ++state.layoutRevision;
}

void MenuSystem::AdjustLayout(Vec2 viewport)
{
    viewport_ = viewport;
    for (std::size_t value = 0U; value < frames_.size(); ++value)
        AdjustLayout(static_cast<FrameId>(value), viewport);
}

void MenuSystem::Invalidate(FrameId frame)
{
    auto& state = frames_[index(frame)];
    ++state.invalidateRevision;
    AdjustLayout(frame, viewport_);
}

Vec2 MenuSystem::SetPos(FrameId frame, Vec2 pos, Anchor anchor,
                        Vec2 size, Vec2 viewport)
{
    Vec2 alignment{};
    switch (anchor)
    {
    case Anchor::Left:
        alignment.x = size.x * 0.5F;
        break;
    case Anchor::Right:
        alignment.x = -size.x * 0.5F;
        break;
    case Anchor::Top:
        alignment.y = size.y * 0.5F;
        break;
    case Anchor::Bottom:
        alignment.y = -size.y * 0.5F;
        break;
    case Anchor::TopLeft:
        alignment = {size.x * 0.5F, size.y * 0.5F};
        break;
    case Anchor::TopRight:
        alignment = {-size.x * 0.5F, size.y * 0.5F};
        break;
    case Anchor::BottomLeft:
        alignment = {size.x * 0.5F, -size.y * 0.5F};
        break;
    case Anchor::BottomRight:
        alignment = {-size.x * 0.5F, -size.y * 0.5F};
        break;
    case Anchor::Center:
        break;
    }

    Vec2 result{pos.x + alignment.x, pos.y + alignment.y};
    const float minimumX = size.x * 0.5F + 15.0F;
    const float maximumX = viewport.x - size.x * 0.5F - 15.0F;
    const float minimumY = size.y * 0.5F + 15.0F;
    const float maximumY = viewport.y - size.y * 0.5F - 15.0F;
    result.x = minimumX <= maximumX
                   ? std::clamp(result.x, minimumX, maximumX)
                   : viewport.x * 0.5F;
    result.y = minimumY <= maximumY
                   ? std::clamp(result.y, minimumY, maximumY)
                   : viewport.y * 0.5F;
    auto& state = frames_[index(frame)];
    state.position = result;
    state.viewport = viewport;
    viewport_ = viewport;
    return result;
}

std::optional<FrameId> MenuSystem::TopModal() const noexcept
{
    std::optional<FrameId> result;
    int level = topmostDefault - 1;
    std::uint64_t order = 0U;
    for (std::size_t value = 0U; value < frames_.size(); ++value)
    {
        const auto& frame = frames_[value];
        if (frame.visible && frame.modal &&
            (frame.topmost > level ||
             (frame.topmost == level && frame.modalOrder >= order)))
        {
            result = static_cast<FrameId>(value);
            level = frame.topmost;
            order = frame.modalOrder;
        }
    }
    return result;
}

bool MenuSystem::HasModal() const noexcept
{
    return TopModal().has_value();
}

ScreenStack& MenuSystem::Screens() noexcept
{
    return screens_;
}

const ScreenStack& MenuSystem::Screens() const noexcept
{
    return screens_;
}

std::size_t& MenuSystem::Selection() noexcept
{
    return selection_;
}

std::size_t MenuSystem::Selection() const noexcept
{
    return selection_;
}

bool MenuSystem::ConsumeInputReset() noexcept
{
    const bool pending = inputResetPending_;
    inputResetPending_ = false;
    return pending;
}

std::uint64_t MenuSystem::InputResetRevision() const noexcept
{
    return inputResetRevision_;
}

std::size_t MenuSystem::index(FrameId frame) noexcept
{
    return static_cast<std::size_t>(frame);
}

void MenuSystem::ApplyState()
{
    visibility_.guiMode =
        state_ != MenuState::Hud && state_ != MenuState::Race;
    visibility_.invertY = state_ == MenuState::Info;
    visibility_.cursor =
        state_ != MenuState::Hud && state_ != MenuState::Info;
    visibility_.screen = visibility_.guiMode;
    visibility_.main = state_ == MenuState::Main;
    visibility_.race = state_ == MenuState::Race;
    visibility_.hud = state_ == MenuState::Hud;
    visibility_.finish = state_ == MenuState::Finish;
    visibility_.final = state_ == MenuState::Final;
    visibility_.info = state_ == MenuState::Info || loadingVisible_;
    visibility_.options = optionsVisible_;
    visibility_.startOptions = startOptionsVisible_;

    Show(FrameId::Screen, visibility_.screen);
    Show(FrameId::Main, visibility_.main);
    Show(FrameId::Race, visibility_.race);
    Show(FrameId::Hud, visibility_.hud);
    Show(FrameId::Finish, visibility_.finish);
    Show(FrameId::Final, visibility_.final);
    Show(FrameId::Info, visibility_.info);
    ShowModal(FrameId::Options, visibility_.options, topmostDefault);
    ShowModal(FrameId::StartOptions, visibility_.startOptions);
    Show(FrameId::Cursor, visibility_.cursor);
    ShowModal(FrameId::Loading, loadingVisible_, topmostLoading);
}

void MenuSystem::MarkInputReset() noexcept
{
    ++inputResetRevision_;
    inputResetPending_ = true;
}

} // namespace r3d::game::originalmenu
