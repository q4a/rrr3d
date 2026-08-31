#pragma once

#include "InputActions.h"
#include "MainMenu2Spec.h"
#include "OriginalGameData.h"
#include "OriginalMenuSystem.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace r3d::resource
{
class ResourceFileSystem;
}

namespace r3d::game::mainmenu2
{

enum class ImageStorage
{
    Rgba8,
    EncodedContainer,
};

struct Image
{
    std::string virtualPath;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    ImageStorage storage = ImageStorage::Rgba8;
    std::vector<std::uint8_t> bytes;
};

struct ResourceAudit
{
    std::size_t catalogFiles = 0;
    std::uintmax_t catalogBytes = 0;
    std::size_t guiImages = 0;
    std::size_t guiMeshes = 0;
    std::uintmax_t guiBytes = 0;
    std::size_t localizedStrings = 0;
};

struct Model
{
    std::string language;
    std::vector<std::string> items;
    std::string versionText;
    Image backgroundImage;
    Image topPanelImage;
    Image bottomPanelImage;
    Image selectionImage;
    Image cursorImage;
    ResourceAudit audit;
};

class Controller
{
public:
    explicit Controller(std::size_t itemCount);

    std::size_t selectedItem() const noexcept;
    bool select(std::size_t item) noexcept;
    std::optional<Command> handle(const rrr3d::input::ActionEvent& event);

private:
    std::size_t itemCount_ = 0;
    std::size_t selectedItem_ = 0;
};

// Backend-neutral state copied from MainMenu2::{MainFrame,GameModeFrame,
// TournamentFrame,DifficultyFrame,ProfileFrame}.  The renderer supplies text
// and textures, while this owner supplies the original enabled-item policy,
// navigation graph and fixed-pixel layout.
struct FrameContext
{
    bool tutorialFirstStageComplete = false;
    bool hasProfiles = false;
    bool hasLastProfile = false;
};

class FrameController
{
public:
    void show(originalmenu::MenuScreen screen, std::size_t itemCount,
              FrameContext context = {});

    originalmenu::MenuScreen screen() const noexcept;
    const std::vector<bool>& enabledItems() const noexcept;
    bool enabled(std::size_t item) const noexcept;
    std::size_t firstEnabled() const noexcept;
    std::size_t moveSelection(std::size_t current,
                              int direction) const noexcept;
    bool owns(originalmenu::MenuScreen screen) const noexcept;
    bool detachedBackItem() const noexcept;
    float itemX(float viewportWidth) const noexcept;
    float itemY(float viewportHeight, std::size_t item,
                float itemHeight = 48.0F) const noexcept;

private:
    originalmenu::MenuScreen screen_ = originalmenu::MenuScreen::Main;
    std::vector<bool> enabled_;
};

enum class NetworkFrameCommand : std::uint8_t
{
    Create,
    Connect,
    Back,
};

// Source-owned state of MainMenu2.cpp::NetworkFrame. Network discovery and
// socket lifetime stay in the platform transport; this object owns the menu
// policy and the six-address presentation payload.
class NetworkFrameState
{
public:
    void show(std::vector<std::string> adapterAddresses);
    std::optional<NetworkFrameCommand> activate(
        std::size_t item) const noexcept;
    const std::vector<std::string>& addressLines() const noexcept;

private:
    std::vector<std::string> addressLines_;
};

enum class MainMenuServerType : std::uint8_t
{
    None,
    Local,
    Steam,
    Lobby,
};

enum class ServerTypeCommand : std::uint8_t
{
    StartGameMode,
    Back,
};

struct ServerTypeResult
{
    ServerTypeCommand command = ServerTypeCommand::Back;
    MainMenuServerType type = MainMenuServerType::None;
};

class ServerTypeFrameState
{
public:
    void show(bool steamAvailable = false,
              bool lobbyAvailable = false) noexcept;
    std::size_t itemCount() const noexcept;
    std::optional<ServerTypeResult> activate(std::size_t item) noexcept;
    MainMenuServerType type() const noexcept;
    void reset() noexcept;

private:
    std::vector<MainMenuServerType> choices_;
    MainMenuServerType type_ = MainMenuServerType::None;
};

enum class ClientTypeCommand : std::uint8_t
{
    BrowseLan,
    EnterIpAddress,
    BrowseSteam,
    BrowseSteamLan,
    Matchmaking,
    Back,
};

class ClientTypeFrameState
{
public:
    void show(bool steamAvailable = false,
              bool lobbyAvailable = false) noexcept;
    std::size_t itemCount() const noexcept;
    std::optional<ClientTypeCommand> activate(
        std::size_t item) const noexcept;

private:
    std::vector<ClientTypeCommand> choices_;
};

enum class NetworkHint : std::uint8_t
{
    None,
    Refreshing,
    Connecting,
    HostListEmpty,
    ConnectionFailed,
    EnterIpAddress,
};

enum class NetBrowserCommandType : std::uint8_t
{
    Connect,
    Back,
};

struct NetBrowserCommand
{
    NetBrowserCommandType type = NetBrowserCommandType::Back;
    std::size_t endpoint = 0U;
};

class NetBrowserFrameState
{
public:
    static constexpr std::size_t visibleRows = 4U;

    void show() noexcept;
    void update(std::vector<std::string> endpoints,
                bool waiting, NetworkHint hint);
    std::optional<NetBrowserCommand> activate(
        std::size_t item) const noexcept;
    bool scroll(int step) noexcept;
    std::size_t scrollOffset() const noexcept;
    bool canScrollUp() const noexcept;
    bool canScrollDown() const noexcept;
    bool waiting() const noexcept;
    NetworkHint hint() const noexcept;
    const std::vector<std::string>& endpoints() const noexcept;

private:
    std::vector<std::string> endpoints_;
    std::size_t scroll_ = 0U;
    bool waiting_ = false;
    NetworkHint hint_ = NetworkHint::None;
};

enum class NetIpCommand : std::uint8_t
{
    Connect,
    Back,
};

class NetIpAddressFrameState
{
public:
    void show() noexcept;
    void startWaiting(bool waiting, NetworkHint hint) noexcept;
    bool append(std::string_view text);
    bool backspace() noexcept;
    std::optional<NetIpCommand> activate(
        std::size_t item) const noexcept;
    const std::string& displayText() const noexcept;
    std::string address() const;
    bool waiting() const noexcept;
    NetworkHint hint() const noexcept;

private:
    void pushLine(std::string text);

    std::string text_ = "_";
    bool waiting_ = false;
    NetworkHint hint_ = NetworkHint::EnterIpAddress;
};

enum class ProfileFocus : std::uint8_t
{
    Item,
    Close,
    Up,
    Down,
    Back,
};

enum class ProfileCommandType : std::uint8_t
{
    Back,
    Scrolled,
    Select,
    Delete,
};

struct ProfileCommand
{
    ProfileCommandType type = ProfileCommandType::Back;
    std::size_t index = 0U;
};

class ProfileFrameState
{
public:
    static constexpr std::size_t visibleRows = 4U;
    static constexpr float gridOffsetY = -90.0F;
    static constexpr float upArrowOffsetY = -108.0F;
    static constexpr float downArrowOffsetY = 120.0F;
    static constexpr float backOffsetY = 150.0F;

    void show(std::size_t profileCount) noexcept;
    void setProfileCount(std::size_t profileCount) noexcept;

    std::size_t profileCount() const noexcept;
    std::size_t scroll() const noexcept;
    std::size_t visibleBegin() const noexcept;
    std::size_t visibleEnd() const noexcept;
    bool canScrollUp() const noexcept;
    bool canScrollDown() const noexcept;
    ProfileFocus focus() const noexcept;
    std::size_t focusIndex() const noexcept;

    bool focusItem(std::size_t index) noexcept;
    bool focusClose(std::size_t index) noexcept;
    bool focusUp() noexcept;
    bool focusDown() noexcept;
    void focusBack() noexcept;

    std::optional<ProfileCommand> handle(
        const rrr3d::input::ActionEvent& event) noexcept;

    float rowY(float viewportHeight, std::size_t index,
               float itemHeight = 48.0F) const noexcept;
    float upArrowY(float viewportHeight) const noexcept;
    float downArrowY(float viewportHeight) const noexcept;
    float backY(float viewportHeight) const noexcept;

private:
    void focusFirstVisible() noexcept;
    void focusLastVisible() noexcept;

    std::size_t profileCount_ = 0U;
    std::size_t scroll_ = 0U;
    ProfileFocus focus_ = ProfileFocus::Back;
    std::size_t focusIndex_ = 0U;
};

struct FinalCreditSection
{
    std::string caption;
    std::string text;
};

struct FinalMenuLayout
{
    float backX = 0.0F;
    float backY = 0.0F;
    float slideX = 0.0F;
    float slideY = 0.0F;
    float slideMaximumWidth = 0.0F;
    float slideMaximumHeight = 0.0F;
    float creditsX = 0.0F;
    float creditsY = 0.0F;
    float creditsWidth = 480.0F;
};

// Backend-neutral transcription of FinalMenu.  The host implements music,
// image upload and text drawing; this owner keeps the source credit sections,
// 107-second clock, slide alphas, Back command and layout.
class FinalMenuFrameState
{
public:
    static constexpr std::size_t slideCount = 9U;
    static constexpr float duration = 107.0F;

    void invalidate(std::string credits);
    void show() noexcept;
    void hide() noexcept;
    bool shown() const noexcept;
    bool handle(const rrr3d::input::ActionEvent& event,
                bool pointerOnBack = true) const noexcept;
    bool progress(float deltaTime) noexcept;

    const std::vector<FinalCreditSection>& credits() const noexcept;
    float time() const noexcept;
    float progressValue() const noexcept;
    float slideAlpha(std::size_t index) const noexcept;
    FinalMenuLayout layout(float viewportWidth, float viewportHeight,
                           float linesHeight,
                           float backWidth) const noexcept;

private:
    std::vector<FinalCreditSection> credits_;
    float time_ = 0.0F;
    bool shown_ = false;
};

bool runOriginalMainMenuInputSmoke(std::string& error);

Image loadOriginalImage(const resource::ResourceFileSystem& resources,
                        std::string virtualPath);

// Loads the assets and localized strings named by the original
// ResourceManager/MainMenu2 code. No portable menu.cfg or replacement bitmap
// font participates in this path.
Model loadOriginalMainMenu(const resource::ResourceFileSystem& resources,
                           const originalgamedata::Catalog& gameData,
                           std::string language);

} // namespace r3d::game::mainmenu2
