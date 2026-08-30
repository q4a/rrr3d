#include "OriginalMainMenu.h"

#include "OriginalGameData.h"

#include "resource/R3DMeshAsset.h"
#include "resource/ResourceFileSystem.h"

#include <bimg/decode.h>
#include <bx/allocator.h>
#include <bx/error.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace r3d::game::mainmenu2
{
namespace
{

struct CatalogEntry
{
    std::string path;
    std::uintmax_t bytes = 0;
};

using ImagePointer =
    std::unique_ptr<bimg::ImageContainer, void (*)(bimg::ImageContainer*)>;

bx::DefaultAllocator imageAllocator;

std::string bimgErrorMessage(const bx::Error& error)
{
    const auto& message = error.getMessage();
    return std::string(message.getCPtr(),
                       static_cast<std::size_t>(message.getLength()));
}

std::string dataPath(std::string_view legacyPath)
{
    std::string result = "Data/";
    result.reserve(result.size() + legacyPath.size());
    for (const char value : legacyPath)
        result.push_back(value == '\\' ? '/' : value);
    return result;
}

std::string lowerExtension(std::string_view path)
{
    const auto dot = path.find_last_of('.');
    std::string result(dot == std::string_view::npos
                           ? std::string_view{}
                           : path.substr(dot));
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::tolower(value));
                   });
    return result;
}

std::uintmax_t parseSize(std::string_view value,
                         std::string_view resourceName,
                         std::size_t lineNumber)
{
    std::uintmax_t result = 0;
    const auto conversion = std::from_chars(
        value.data(), value.data() + value.size(), result);
    if (conversion.ec != std::errc{} ||
        conversion.ptr != value.data() + value.size())
    {
        throw resource::ResourceError(
            std::string(resourceName) + ":" + std::to_string(lineNumber) +
            ": invalid byte count");
    }
    return result;
}

std::vector<CatalogEntry> loadAndValidateCatalog(
    const resource::ResourceFileSystem& resources, ResourceAudit& audit)
{
    constexpr std::string_view catalogPath = "legacy-assets.catalog";
    std::vector<CatalogEntry> entries;
    std::istringstream lines(resources.readText(catalogPath));
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(lines, line))
    {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line.front() == '#')
            continue;

        const auto separator = line.find('\t');
        if (separator == std::string::npos || separator + 1 == line.size())
        {
            throw resource::ResourceError(
                std::string(catalogPath) + ":" +
                std::to_string(lineNumber) +
                ": expected size<TAB>resource");
        }
        CatalogEntry entry;
        entry.bytes = parseSize(std::string_view(line).substr(0, separator),
                                catalogPath, lineNumber);
        entry.path = line.substr(separator + 1);
        const auto actualSize = resources.fileSize(entry.path);
        if (actualSize != entry.bytes)
        {
            throw resource::ResourceError(
                "Resource size differs from original catalog: " +
                entry.path);
        }
        audit.catalogBytes += entry.bytes;
        entries.push_back(std::move(entry));
    }
    audit.catalogFiles = entries.size();
    if (entries.size() < 1000)
    {
        throw resource::ResourceError(
            "legacy-assets.catalog does not describe the full game data");
    }
    return entries;
}

ImagePointer parseImage(const std::vector<std::uint8_t>& bytes,
                        bimg::TextureFormat::Enum outputFormat,
                        std::string_view path)
{
    if (bytes.empty() ||
        bytes.size() > std::numeric_limits<std::uint32_t>::max())
    {
        throw resource::ResourceError(std::string(path) +
                                      ": empty or oversized image");
    }
    bx::Error error;
    ImagePointer image(
        bimg::imageParse(&imageAllocator, bytes.data(),
                         static_cast<std::uint32_t>(bytes.size()),
                         outputFormat, &error),
        &bimg::imageFree);
    if (!image)
    {
        throw resource::ResourceError(std::string(path) +
                                      ": image decode failed: " +
                                      bimgErrorMessage(error));
    }
    if (image->m_width == 0 || image->m_height == 0 ||
        image->m_width > UINT16_MAX || image->m_height > UINT16_MAX ||
        image->m_depth > 1 || image->m_numLayers != 1 || image->m_cubeMap ||
        image->m_orientation != bimg::Orientation::R0)
    {
        throw resource::ResourceError(std::string(path) +
                                      ": unsupported image shape");
    }
    return image;
}

void validateGuiResources(const resource::ResourceFileSystem& resources,
                          const std::vector<CatalogEntry>& catalog,
                          ResourceAudit& audit)
{
    for (const auto& entry : catalog)
    {
        if (entry.path.rfind("Data/GUI/", 0) != 0)
            continue;

        audit.guiBytes += entry.bytes;
        const auto extension = lowerExtension(entry.path);
        if (extension == ".png" || extension == ".dds")
        {
            const auto bytes = resources.readBinary(entry.path);
            static_cast<void>(parseImage(
                bytes, bimg::TextureFormat::Count, entry.path));
            ++audit.guiImages;
        }
        else if (extension == ".r3d")
        {
            static_cast<void>(
                resource::loadR3DMeshAsset(resources, entry.path));
            ++audit.guiMeshes;
        }
        else
        {
            throw resource::ResourceError(
                "Unexpected original GUI resource type: " + entry.path);
        }
    }
    if (audit.guiImages == 0 || audit.guiMeshes == 0)
    {
        throw resource::ResourceError(
            "Original GUI image/mesh catalog is incomplete");
    }
}

Image loadImage(const resource::ResourceFileSystem& resources,
                std::string virtualPath)
{
    Image result;
    result.virtualPath = std::move(virtualPath);
    const auto encoded = resources.readBinary(result.virtualPath);
    const auto extension = lowerExtension(result.virtualPath);

    if (extension == ".dds")
    {
        const auto parsed = parseImage(encoded, bimg::TextureFormat::Count,
                                       result.virtualPath);
        result.width = static_cast<std::uint16_t>(parsed->m_width);
        result.height = static_cast<std::uint16_t>(parsed->m_height);
        result.storage = ImageStorage::EncodedContainer;
        result.bytes = encoded;
        return result;
    }

    const auto parsed = parseImage(encoded, bimg::TextureFormat::RGBA8,
                                   result.virtualPath);
    bimg::ImageMip mip;
    if (!bimg::imageGetRawData(*parsed, 0, 0, parsed->m_data,
                               parsed->m_size, mip) ||
        mip.m_format != bimg::TextureFormat::RGBA8)
    {
        throw resource::ResourceError(result.virtualPath +
                                      ": RGBA pixels are unavailable");
    }
    result.width = static_cast<std::uint16_t>(parsed->m_width);
    result.height = static_cast<std::uint16_t>(parsed->m_height);
    const std::size_t expected =
        static_cast<std::size_t>(result.width) * result.height * 4U;
    if (mip.m_data == nullptr || mip.m_size < expected)
    {
        throw resource::ResourceError(result.virtualPath +
                                      ": decoded pixels are truncated");
    }
    result.bytes.assign(mip.m_data, mip.m_data + expected);
    return result;
}

originalgamedata::Language languageDefinition(
    const originalgamedata::Catalog& gameData,
    std::string& language)
{
    std::transform(language.begin(), language.end(), language.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::tolower(value));
                   });
    if (language.empty())
        language = "english";
    const auto* selected =
        originalgamedata::findLanguage(gameData, language);
    if (selected == nullptr)
    {
        throw resource::ResourceError(
            "game.xml does not declare requested language: " + language);
    }
    return *selected;
}

} // namespace

Controller::Controller(std::size_t itemCount) : itemCount_(itemCount)
{
    if (itemCount_ != itemCommands.size())
        throw std::invalid_argument(
            "MainMenu2 controller item count differs from shared spec");
}

std::size_t Controller::selectedItem() const noexcept
{
    return selectedItem_;
}

bool Controller::select(std::size_t item) noexcept
{
    if (item >= itemCount_)
        return false;
    selectedItem_ = item;
    return true;
}

std::optional<Command> Controller::handle(
    const rrr3d::input::ActionEvent& event)
{
    if (!event.active)
        return std::nullopt;

    switch (event.action)
    {
    case rrr3d::input::Action::MenuUp:
        selectedItem_ =
            selectedItem_ == 0 ? itemCount_ - 1 : selectedItem_ - 1;
        break;
    case rrr3d::input::Action::MenuDown:
        selectedItem_ = (selectedItem_ + 1) % itemCount_;
        break;
    case rrr3d::input::Action::MenuConfirm:
        if (!event.repeated)
            return itemCommands[selectedItem_];
        break;
    case rrr3d::input::Action::MenuBack:
        if (!event.repeated)
            return Command::Back;
        break;
    default:
        break;
    }
    return std::nullopt;
}

void FrameController::show(originalmenu::MenuScreen screen,
                           std::size_t itemCount, FrameContext context)
{
    screen_ = screen;
    enabled_.assign(itemCount, true);
    if (screen_ == originalmenu::MenuScreen::GameMode && itemCount > 1U)
        enabled_[1] = context.tutorialFirstStageComplete;
    else if (screen_ == originalmenu::MenuScreen::Tournament)
    {
        if (!enabled_.empty())
            enabled_[0] = context.hasLastProfile;
        if (enabled_.size() > 2U)
            enabled_[2] = context.hasProfiles;
    }
}

originalmenu::MenuScreen FrameController::screen() const noexcept
{
    return screen_;
}

const std::vector<bool>& FrameController::enabledItems() const noexcept
{
    return enabled_;
}

bool FrameController::enabled(std::size_t item) const noexcept
{
    return item < enabled_.size() && enabled_[item];
}

std::size_t FrameController::firstEnabled() const noexcept
{
    const auto item = std::find(enabled_.begin(), enabled_.end(), true);
    return item == enabled_.end()
               ? 0U
               : static_cast<std::size_t>(item - enabled_.begin());
}

std::size_t FrameController::moveSelection(std::size_t current,
                                           int direction) const noexcept
{
    if (enabled_.empty())
        return 0U;
    current = std::min(current, enabled_.size() - 1U);
    for (std::size_t attempts = 0U; attempts < enabled_.size(); ++attempts)
    {
        if (direction < 0)
            current = current == 0U ? enabled_.size() - 1U : current - 1U;
        else
            current = (current + 1U) % enabled_.size();
        if (enabled_[current])
            break;
    }
    return current;
}

bool FrameController::owns(originalmenu::MenuScreen screen) const noexcept
{
    using originalmenu::MenuScreen;
    switch (screen)
    {
    case MenuScreen::Main:
    case MenuScreen::GameMode:
    case MenuScreen::Tournament:
    case MenuScreen::Difficulty:
    case MenuScreen::Profiles:
    case MenuScreen::Network:
    case MenuScreen::NetworkServerType:
    case MenuScreen::NetworkClientType:
    case MenuScreen::NetworkBrowser:
    case MenuScreen::NetworkIpAddress:
    case MenuScreen::Credits:
        return true;
    default:
        return false;
    }
}

bool FrameController::detachedBackItem() const noexcept
{
    return owns(screen_) && screen_ != originalmenu::MenuScreen::Main;
}

float FrameController::itemX(float viewportWidth) const noexcept
{
    return viewportWidth * 0.5F + itemCenterOffsetX;
}

float FrameController::itemY(float viewportHeight, std::size_t item,
                             float itemHeight) const noexcept
{
    if (detachedBackItem() && item + 1U == enabled_.size())
        return viewportHeight * 0.5F + 150.0F;
    return viewportHeight * 0.5F + firstItemOffsetY +
           static_cast<float>(item) * (itemHeight + 5.0F);
}

void ProfileFrameState::show(std::size_t profileCount) noexcept
{
    profileCount_ = profileCount;
    scroll_ = 0U;
    focusBack();
}

void ProfileFrameState::setProfileCount(std::size_t profileCount) noexcept
{
    profileCount_ = profileCount;
    const auto maximumScroll =
        profileCount_ > visibleRows ? profileCount_ - visibleRows : 0U;
    scroll_ = std::min(scroll_, maximumScroll);
    if ((focus_ == ProfileFocus::Item || focus_ == ProfileFocus::Close) &&
        focusIndex_ >= profileCount_)
    {
        focusBack();
    }
    else if ((focus_ == ProfileFocus::Up && !canScrollUp()) ||
             (focus_ == ProfileFocus::Down && !canScrollDown()))
    {
        focusBack();
    }
}

std::size_t ProfileFrameState::profileCount() const noexcept
{
    return profileCount_;
}

std::size_t ProfileFrameState::scroll() const noexcept
{
    return scroll_;
}

std::size_t ProfileFrameState::visibleBegin() const noexcept
{
    return scroll_;
}

std::size_t ProfileFrameState::visibleEnd() const noexcept
{
    return std::min(scroll_ + visibleRows, profileCount_);
}

bool ProfileFrameState::canScrollUp() const noexcept
{
    return scroll_ > 0U;
}

bool ProfileFrameState::canScrollDown() const noexcept
{
    return scroll_ + visibleRows < profileCount_;
}

ProfileFocus ProfileFrameState::focus() const noexcept
{
    return focus_;
}

std::size_t ProfileFrameState::focusIndex() const noexcept
{
    return focusIndex_;
}

bool ProfileFrameState::focusItem(std::size_t index) noexcept
{
    if (index < visibleBegin() || index >= visibleEnd())
        return false;
    focus_ = ProfileFocus::Item;
    focusIndex_ = index;
    return true;
}

bool ProfileFrameState::focusClose(std::size_t index) noexcept
{
    if (!focusItem(index))
        return false;
    focus_ = ProfileFocus::Close;
    return true;
}

bool ProfileFrameState::focusUp() noexcept
{
    if (!canScrollUp())
        return false;
    focus_ = ProfileFocus::Up;
    return true;
}

bool ProfileFrameState::focusDown() noexcept
{
    if (!canScrollDown())
        return false;
    focus_ = ProfileFocus::Down;
    return true;
}

void ProfileFrameState::focusBack() noexcept
{
    focus_ = ProfileFocus::Back;
    focusIndex_ = 0U;
}

void ProfileFrameState::focusFirstVisible() noexcept
{
    if (!focusItem(visibleBegin()))
        focusBack();
}

void ProfileFrameState::focusLastVisible() noexcept
{
    const auto end = visibleEnd();
    if (end == visibleBegin() || !focusItem(end - 1U))
        focusBack();
}

std::optional<ProfileCommand> ProfileFrameState::handle(
    const rrr3d::input::ActionEvent& event) noexcept
{
    using rrr3d::input::Action;
    if (!event.active)
        return std::nullopt;

    if (!event.repeated &&
        (event.action == Action::MenuBack || event.action == Action::Pause))
    {
        return ProfileCommand{ProfileCommandType::Back, 0U};
    }
    if (event.action == Action::TurnLeft ||
        event.action == Action::TurnRight)
    {
        if (focus_ == ProfileFocus::Item)
            focus_ = ProfileFocus::Close;
        else if (focus_ == ProfileFocus::Close)
            focus_ = ProfileFocus::Item;
        return std::nullopt;
    }
    if (event.action == Action::MenuUp)
    {
        switch (focus_)
        {
        case ProfileFocus::Item:
        case ProfileFocus::Close:
            if (focusIndex_ > visibleBegin())
                --focusIndex_;
            else if (!focusUp())
                focusBack();
            break;
        case ProfileFocus::Up:
            focusBack();
            break;
        case ProfileFocus::Down:
            focusLastVisible();
            break;
        case ProfileFocus::Back:
            if (!focusDown())
                focusLastVisible();
            break;
        }
        return std::nullopt;
    }
    if (event.action == Action::MenuDown)
    {
        switch (focus_)
        {
        case ProfileFocus::Item:
        case ProfileFocus::Close:
            if (focusIndex_ + 1U < visibleEnd())
                ++focusIndex_;
            else if (!focusDown())
                focusBack();
            break;
        case ProfileFocus::Up:
            focusFirstVisible();
            break;
        case ProfileFocus::Down:
            focusBack();
            break;
        case ProfileFocus::Back:
            if (!focusUp())
                focusFirstVisible();
            break;
        }
        return std::nullopt;
    }
    if (event.action != Action::MenuConfirm || event.repeated)
        return std::nullopt;

    switch (focus_)
    {
    case ProfileFocus::Back:
        return ProfileCommand{ProfileCommandType::Back, 0U};
    case ProfileFocus::Up:
        if (canScrollUp())
        {
            --scroll_;
            return ProfileCommand{ProfileCommandType::Scrolled, scroll_};
        }
        break;
    case ProfileFocus::Down:
        if (canScrollDown())
        {
            ++scroll_;
            return ProfileCommand{ProfileCommandType::Scrolled, scroll_};
        }
        break;
    case ProfileFocus::Close:
        if (focusIndex_ < profileCount_)
            return ProfileCommand{ProfileCommandType::Delete, focusIndex_};
        break;
    case ProfileFocus::Item:
        if (focusIndex_ < profileCount_)
            return ProfileCommand{ProfileCommandType::Select, focusIndex_};
        break;
    }
    return std::nullopt;
}

float ProfileFrameState::rowY(float viewportHeight, std::size_t index,
                              float itemHeight) const noexcept
{
    return viewportHeight * 0.5F + gridOffsetY +
           static_cast<float>(index - visibleBegin()) *
               (itemHeight + 5.0F);
}

float ProfileFrameState::upArrowY(float viewportHeight) const noexcept
{
    return viewportHeight * 0.5F + upArrowOffsetY;
}

float ProfileFrameState::downArrowY(float viewportHeight) const noexcept
{
    return viewportHeight * 0.5F + downArrowOffsetY;
}

float ProfileFrameState::backY(float viewportHeight) const noexcept
{
    return viewportHeight * 0.5F + backOffsetY;
}

void FinalMenuFrameState::invalidate(std::string credits)
{
    const auto trimBlock = [](std::string value) {
        const auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            return std::string{};
        const auto last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1U);
    };
    credits_.clear();
    std::size_t begin = 0U;
    while (begin < credits.size())
    {
        const auto separator = credits.find("\n\n", begin);
        const auto end = separator == std::string::npos
                             ? credits.size()
                             : separator;
        const std::string block =
            trimBlock(credits.substr(begin, end - begin));
        if (!block.empty())
        {
            const auto captionEnd = block.find('\n');
            credits_.push_back(
                {block.substr(0U, captionEnd),
                 captionEnd == std::string::npos
                     ? std::string{}
                     : block.substr(captionEnd + 1U)});
        }
        if (separator == std::string::npos)
            break;
        begin = separator + 2U;
    }
}

void FinalMenuFrameState::show() noexcept
{
    time_ = 0.0F;
    shown_ = true;
}

void FinalMenuFrameState::hide() noexcept
{
    time_ = 0.0F;
    shown_ = false;
}

bool FinalMenuFrameState::shown() const noexcept
{
    return shown_;
}

bool FinalMenuFrameState::handle(
    const rrr3d::input::ActionEvent& event,
    bool pointerOnBack) const noexcept
{
    if (!event.active || event.repeated)
        return false;
    using rrr3d::input::Action;
    if (event.action == Action::MenuBack ||
        event.action == Action::Pause)
        return true;
    if (event.action != Action::MenuConfirm)
        return false;
    return event.source != rrr3d::input::Source::Mouse || pointerOnBack;
}

bool FinalMenuFrameState::progress(float deltaTime) noexcept
{
    if (!shown_)
        return false;
    time_ += deltaTime;
    return progressValue() == 1.0F;
}

const std::vector<FinalCreditSection>&
FinalMenuFrameState::credits() const noexcept
{
    return credits_;
}

float FinalMenuFrameState::time() const noexcept
{
    return time_;
}

float FinalMenuFrameState::progressValue() const noexcept
{
    return std::clamp(time_ / duration, 0.0F, 1.0F);
}

float FinalMenuFrameState::slideAlpha(std::size_t index) const noexcept
{
    if (index >= slideCount)
        return 0.0F;
    const float alpha1 =
        static_cast<float>(index) / static_cast<float>(slideCount);
    const float alpha2 =
        static_cast<float>(index + 1U) /
        static_cast<float>(slideCount);
    const float slideDuration = (alpha2 - alpha1) * duration;
    const float slideTime = std::clamp(
        (progressValue() - alpha1) * duration,
        0.0F, slideDuration);
    return std::clamp(slideTime / 1.0F, 0.0F, 1.0F) -
           std::clamp(
               (slideTime - slideDuration) / 1.0F,
               0.0F, 1.0F);
}

FinalMenuLayout FinalMenuFrameState::layout(
    float viewportWidth, float viewportHeight, float linesHeight,
    float backWidth) const noexcept
{
    FinalMenuLayout result;
    result.backX = backWidth * 0.5F;
    result.backY = viewportHeight - 60.0F;
    result.slideX = (viewportWidth - 400.0F) * 0.5F;
    result.slideY = viewportHeight * 0.5F;
    result.slideMaximumWidth = viewportWidth - 500.0F;
    result.slideMaximumHeight = viewportHeight - 300.0F;
    result.creditsX = viewportWidth - 250.0F;
    result.creditsY =
        viewportHeight -
        progressValue() * (linesHeight + viewportHeight);
    return result;
}

bool runOriginalMainMenuInputSmoke(std::string& error)
{
    Controller controller(itemCommands.size());
    using rrr3d::input::Action;
    using rrr3d::input::ActionEvent;
    using rrr3d::input::Source;

    controller.handle(
        {Action::MenuDown, 1.0F, true, false, Source::Keyboard, 0});
    controller.handle(
        {Action::MenuDown, 1.0F, true, true, Source::Keyboard, 0});
    if (controller.selectedItem() != 2)
    {
        error = "MainMenu2 down/repeat navigation failed";
        return false;
    }
    controller.handle(
        {Action::MenuUp, 1.0F, true, false, Source::GamepadButton, 1});
    if (controller.selectedItem() != 1)
    {
        error = "MainMenu2 up navigation failed";
        return false;
    }
    if (!controller.select(4) || controller.select(itemCommands.size()))
    {
        error = "MainMenu2 pointer selection bounds failed";
        return false;
    }
    const auto command = controller.handle(
        {Action::MenuConfirm, 1.0F, true, false, Source::Mouse, 0});
    if (command != Command::Exit)
    {
        error = "MainMenu2 command dispatch differs from legacy item order";
        return false;
    }
    const auto back = controller.handle(
        {Action::MenuBack, 1.0F, true, false, Source::GamepadButton, 1});
    if (back != Command::Back)
    {
        error = "MainMenu2 back dispatch failed";
        return false;
    }
    error.clear();
    return true;
}

Image loadOriginalImage(const resource::ResourceFileSystem& resources,
                        std::string virtualPath)
{
    return loadImage(resources, std::move(virtualPath));
}

Model loadOriginalMainMenu(const resource::ResourceFileSystem& resources,
                           const originalgamedata::Catalog& gameData,
                           std::string language)
{
    Model model;
    const auto catalog = loadAndValidateCatalog(resources, model.audit);
    validateGuiResources(resources, catalog, model.audit);

    const auto selectedLanguage =
        languageDefinition(gameData, language);
    auto strings = originalgamedata::loadOriginalStringLibrary(
        resources, selectedLanguage);
    model.language = std::move(language);
    model.audit.localizedStrings = strings.size();
    for (const char* key : itemStringKeys)
    {
        if (!strings.has(key))
            throw resource::ResourceError("Localized string is missing: " +
                                          std::string(key));
        model.items.push_back(strings.get(key));
    }
    model.versionText = version;

    model.backgroundImage = loadImage(resources, dataPath(background));
    model.topPanelImage = loadImage(resources, dataPath(topPanel));
    model.bottomPanelImage = loadImage(resources, dataPath(bottomPanel));
    model.selectionImage = loadImage(resources, dataPath(selection));
    model.cursorImage = loadImage(resources, dataPath(cursor));
    return model;
}

} // namespace r3d::game::mainmenu2
