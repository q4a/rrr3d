#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <vector>

namespace r3d::game::originalmenu
{

enum class MenuState
{
    Main,
    Race,
    Hud,
    Finish,
    Info,
    Final,
};

enum class MenuScreen
{
    Main,
    GameMode,
    Tournament,
    Difficulty,
    Profiles,
    Network,
    NetworkServerType,
    NetworkClientType,
    NetworkBrowser,
    NetworkIpAddress,
    Options,
    Credits,
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
};

enum class FrameId
{
    Screen,
    Main,
    Race,
    Hud,
    Finish,
    Final,
    Info,
    Options,
    StartOptions,
    Accept,
    Weapon,
    Message,
    Music,
    UserChat,
    Cursor,
    Loading,
    Count,
};

enum class Anchor
{
    Center,
    Left,
    Right,
    Top,
    Bottom,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
};

struct Vec2
{
    float x = 0.0F;
    float y = 0.0F;
};

struct FrameState
{
    bool visible = false;
    bool modal = false;
    int topmost = 0;
    Vec2 position{};
    Vec2 viewport{};
    std::uint64_t showRevision = 0U;
    std::uint64_t invalidateRevision = 0U;
    std::uint64_t layoutRevision = 0U;
    std::uint64_t modalOrder = 0U;
};

struct MenuVisibility
{
    bool guiMode = true;
    bool invertY = false;
    bool cursor = true;
    bool screen = true;
    bool main = true;
    bool race = false;
    bool hud = false;
    bool finish = false;
    bool final = false;
    bool info = false;
    bool options = false;
    bool startOptions = false;
};

class MenuSystem;

// Source-owned replacement for the host's vector of active MenuFrame states.
// The small vector-like surface keeps the renderer integration mechanical,
// while every screen transition still passes through MenuSystem ownership.
class ScreenStack
{
  public:
    explicit ScreenStack(MenuSystem& owner);

    MenuScreen& back();
    const MenuScreen& back() const;
    bool empty() const noexcept;
    std::size_t size() const noexcept;
    void set_back(MenuScreen screen);
    void push_back(MenuScreen screen);
    void pop_back();
    void clear();
    ScreenStack& operator=(std::initializer_list<MenuScreen> screens);
    ScreenStack& operator=(const std::vector<MenuScreen>& screens);

    const std::vector<MenuScreen>& values() const noexcept;

  private:
    void changed();

    MenuSystem* owner_ = nullptr;
    std::vector<MenuScreen> values_;
};

class MenuSystem
{
  public:
    static constexpr int topmostDefault = 0;
    static constexpr int topmostPopup = 1;
    static constexpr int topmostModal = 2;
    static constexpr int topmostCursor = 4;
    static constexpr int topmostLoading = 5;

    MenuSystem();

    MenuState State() const noexcept;
    const MenuVisibility& Visibility() const noexcept;
    void SetState(MenuState state);
    void SetLoadingVisible(bool visible);
    void SetOptionsVisible(bool visible);
    void SetStartOptionsVisible(bool visible);

    void Show(FrameId frame, bool visible);
    void ShowModal(FrameId frame, bool visible,
                   int level = topmostModal);
    bool Visible(FrameId frame) const noexcept;
    const FrameState& Frame(FrameId frame) const noexcept;
    void AdjustLayout(FrameId frame, Vec2 viewport);
    void AdjustLayout(Vec2 viewport);
    void Invalidate(FrameId frame);
    Vec2 SetPos(FrameId frame, Vec2 pos, Anchor anchor, Vec2 size,
                Vec2 viewport);

    std::optional<FrameId> TopModal() const noexcept;
    bool HasModal() const noexcept;

    ScreenStack& Screens() noexcept;
    const ScreenStack& Screens() const noexcept;
    std::size_t& Selection() noexcept;
    std::size_t Selection() const noexcept;

    bool ConsumeInputReset() noexcept;
    std::uint64_t InputResetRevision() const noexcept;

  private:
    friend class ScreenStack;

    static std::size_t index(FrameId frame) noexcept;
    void ApplyState();
    void MarkInputReset() noexcept;

    MenuState state_ = MenuState::Main;
    bool loadingVisible_ = false;
    bool optionsVisible_ = false;
    bool startOptionsVisible_ = false;
    MenuVisibility visibility_{};
    std::array<FrameState, static_cast<std::size_t>(FrameId::Count)>
        frames_{};
    ScreenStack screens_;
    std::size_t selection_ = 0U;
    std::uint64_t inputResetRevision_ = 0U;
    bool inputResetPending_ = false;
    Vec2 viewport_{};
    std::uint64_t modalOrder_ = 0U;
};

} // namespace r3d::game::originalmenu
