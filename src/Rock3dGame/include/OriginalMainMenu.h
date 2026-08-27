#pragma once

#include "InputActions.h"
#include "MainMenu2Spec.h"
#include "OriginalGameData.h"
#include "OriginalMenuSystem.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
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
    originalgamedata::StringLibrary localizedStrings;
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
