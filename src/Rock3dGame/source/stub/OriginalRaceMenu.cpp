#include "OriginalRaceMenu.h"

#include <algorithm>
#include <cmath>

namespace r3d::game::originalracemenu
{

RaceMenuState::RaceMenuState() noexcept
{
    applyState();
}

State RaceMenuState::state() const noexcept
{
    return state_;
}

State RaceMenuState::lastState() const noexcept
{
    return lastState_;
}

const Visibility& RaceMenuState::visibility() const noexcept
{
    return visibility_;
}

bool RaceMenuState::setState(State state) noexcept
{
    if (state >= State::Count || state_ == state)
        return false;
    lastState_ = state_;
    state_ = state;
    applyState();
    return true;
}

void RaceMenuState::applyState() noexcept
{
    visibility_ = {};
    visibility_.car =
        state_ == State::Main || state_ == State::Garage ||
        state_ == State::Workshop;
    visibility_.spaceship = state_ == State::Angar;
    visibility_.main = state_ == State::Main;
    visibility_.garage = state_ == State::Garage;
    visibility_.workshop = state_ == State::Workshop;
    visibility_.gamers = state_ == State::Gamers;
    visibility_.angar = state_ == State::Angar;
    visibility_.achievements = state_ == State::Achievements;
}

RaceMainFrameState::RaceMainFrameState()
{
    show(false);
}

void RaceMainFrameState::show(bool clientRaceReady) noexcept
{
    invalidate(clientRaceReady);
}

void RaceMainFrameState::invalidate(bool clientRaceReady) noexcept
{
    enabled_.fill(true);
    if (clientRaceReady)
        std::fill(enabled_.begin() + 1, enabled_.end(), false);
}

const std::array<bool, RaceMainFrameState::itemCount>&
RaceMainFrameState::enabledItems() const noexcept
{
    return enabled_;
}

bool RaceMainFrameState::enabled(std::size_t item) const noexcept
{
    return item < enabled_.size() && enabled_[item];
}

std::size_t RaceMainFrameState::firstEnabled() const noexcept
{
    const auto found = std::find(enabled_.begin(), enabled_.end(), true);
    return found == enabled_.end()
               ? 0U
               : static_cast<std::size_t>(found - enabled_.begin());
}

std::size_t RaceMainFrameState::moveSelection(
    std::size_t selection, int direction) const noexcept
{
    if (selection >= itemCount)
        selection = firstEnabled();
    for (std::size_t attempts = 0U; attempts < itemCount; ++attempts)
    {
        selection = direction < 0
                        ? (selection == 0U ? itemCount - 1U
                                           : selection - 1U)
                        : (selection + 1U) % itemCount;
        if (enabled(selection))
            return selection;
    }
    return firstEnabled();
}

std::optional<RaceMainCommand> RaceMainFrameState::command(
    std::size_t selection) const noexcept
{
    if (!enabled(selection))
        return std::nullopt;
    switch (selection)
    {
    case 0U:
        return RaceMainCommand::StartRace;
    case 1U:
        return RaceMainCommand::Workshop;
    case 2U:
        return RaceMainCommand::Garage;
    case 3U:
        return RaceMainCommand::Angar;
    case 4U:
        return RaceMainCommand::Achievements;
    case 5U:
        return RaceMainCommand::Options;
    case 6U:
        return RaceMainCommand::Exit;
    default:
        return std::nullopt;
    }
}

float RaceMainFrameState::itemX(float viewportWidth, std::size_t item,
                                float itemWidth, float spacing) const noexcept
{
    const float first = viewportWidth * 0.5F -
                        (static_cast<float>(itemCount) * itemWidth +
                         static_cast<float>(itemCount - 1U) * spacing) *
                            0.5F +
                        itemWidth * 0.5F;
    return first + static_cast<float>(item) * (itemWidth + spacing);
}

float RaceMainFrameState::itemY(float viewportHeight,
                                float bottomPanelHeight) const noexcept
{
    return viewportHeight - bottomPanelHeight * 0.5F - 72.0F;
}

void GamersFrameState::show(std::vector<GamerEntry> entries,
                            std::uint32_t currentGamerId)
{
    entries_ = std::move(entries);
    focus_ = GamerFocus::Next;
    selection_ = 0U;
    const auto current = std::find_if(
        entries_.begin(), entries_.end(),
        [currentGamerId](const GamerEntry& entry) {
            return entry.available && entry.gamerId == currentGamerId;
        });
    if (current != entries_.end())
    {
        selection_ = static_cast<std::size_t>(current - entries_.begin());
        return;
    }
    const auto first = std::find_if(
        entries_.begin(), entries_.end(),
        [](const GamerEntry& entry) { return entry.available; });
    if (first != entries_.end())
        selection_ = static_cast<std::size_t>(first - entries_.begin());
}

void GamersFrameState::updateEntries(std::vector<GamerEntry> entries)
{
    const auto selected = selectedGamerId();
    entries_ = std::move(entries);
    if (selected)
    {
        const auto restored = std::find_if(
            entries_.begin(), entries_.end(),
            [&](const GamerEntry& entry) {
                return entry.available && entry.gamerId == *selected;
            });
        if (restored != entries_.end())
        {
            selection_ = static_cast<std::size_t>(
                restored - entries_.begin());
            return;
        }
    }
    const auto first = std::find_if(
        entries_.begin(), entries_.end(),
        [](const GamerEntry& entry) { return entry.available; });
    selection_ = first == entries_.end()
                     ? 0U
                     : static_cast<std::size_t>(first - entries_.begin());
}

std::size_t GamersFrameState::selection() const noexcept
{
    return selection_;
}

std::optional<std::uint32_t>
GamersFrameState::selectedGamerId() const noexcept
{
    if (!available(selection_))
        return std::nullopt;
    return entries_[selection_].gamerId;
}

GamerFocus GamersFrameState::focus() const noexcept
{
    return focus_;
}

void GamersFrameState::setFocus(GamerFocus focus) noexcept
{
    focus_ = focus;
}

bool GamersFrameState::available(std::size_t index) const noexcept
{
    return index < entries_.size() && entries_[index].available;
}

std::optional<std::size_t> GamersFrameState::adjacent(
    std::size_t from, int direction) const noexcept
{
    if (direction > 0)
    {
        for (std::size_t index = from + 1U; index < entries_.size(); ++index)
            if (entries_[index].available)
                return index;
    }
    else
    {
        for (std::size_t index = from; index > 0U; --index)
            if (entries_[index - 1U].available)
                return index - 1U;
    }
    return std::nullopt;
}

std::optional<std::size_t> GamersFrameState::previous() const noexcept
{
    return adjacent(selection_, -1);
}

std::optional<std::size_t> GamersFrameState::next() const noexcept
{
    return adjacent(selection_, 1);
}

std::optional<GamerCommand> GamersFrameState::select(
    std::size_t index) noexcept
{
    if (!available(index) || index == selection_)
        return std::nullopt;
    selection_ = index;
    return GamerCommand{
        GamerCommandType::SelectionChanged, selection_,
        entries_[selection_].gamerId};
}

std::optional<GamerCommand> GamersFrameState::handle(
    const rrr3d::input::ActionEvent& event) noexcept
{
    if (!event.active)
        return std::nullopt;
    const auto previousIndex = previous();
    const auto nextIndex = next();
    using rrr3d::input::Action;
    if (event.action == Action::MenuUp || event.action == Action::MenuDown)
    {
        if (focus_ == GamerFocus::Next)
            focus_ = nextIndex ? GamerFocus::Right
                               : previousIndex ? GamerFocus::Left
                                               : GamerFocus::Next;
        else
            focus_ = GamerFocus::Next;
        return std::nullopt;
    }
    if (event.action == Action::TurnLeft ||
        event.action == Action::TurnRight)
    {
        if (focus_ == GamerFocus::Left && nextIndex)
            focus_ = GamerFocus::Right;
        else if (focus_ == GamerFocus::Right && previousIndex)
            focus_ = GamerFocus::Left;
        return std::nullopt;
    }
    if (!event.repeated && event.action == Action::PreviousWeapon &&
        previousIndex)
        return select(*previousIndex);
    if (!event.repeated && event.action == Action::NextWeapon && nextIndex)
        return select(*nextIndex);
    if (event.repeated || event.action != Action::MenuConfirm)
        return std::nullopt;
    if (focus_ == GamerFocus::Left && previousIndex)
        return select(*previousIndex);
    if (focus_ == GamerFocus::Right && nextIndex)
        return select(*nextIndex);
    if (focus_ == GamerFocus::Next && available(selection_))
    {
        return GamerCommand{
            GamerCommandType::Confirm, selection_,
            entries_[selection_].gamerId};
    }
    return std::nullopt;
}

GamerLayout GamersFrameState::layout(
    float viewportWidth, float viewportHeight, float bottomPanelHeight,
    float arrowWidth, float nextArrowWidth) const noexcept
{
    GamerLayout result;
    result.planetRadius = (viewportHeight - bottomPanelHeight) * 0.5F;
    result.planetX = viewportWidth * 0.5F - 25.0F;
    result.planetY = result.planetRadius;
    result.viewportSize =
        std::min(result.planetRadius, 300.0F) * 3.1F;
    result.leftX = result.planetX - result.planetRadius - 40.0F + 3.0F -
                   arrowWidth * 0.5F;
    result.rightX = result.planetX + result.planetRadius + 40.0F + 3.0F +
                    arrowWidth * 0.5F;
    result.nextX = viewportWidth * 0.5F + 400.0F + nextArrowWidth * 0.5F;
    result.nextY = viewportHeight - 100.0F;
    return result;
}

void GarageFrameState::show(
    std::vector<GarageCarCandidate> candidates,
    std::size_t currentCatalogIndex, bool campaign,
    const std::array<bool, colorCount>& colorsAvailable)
{
    std::vector<GarageCarEntry> available;
    std::vector<GarageCarEntry> secret;
    std::vector<GarageCarEntry> locked;
    available.reserve(candidates.size());
    secret.reserve(candidates.size());
    locked.reserve(candidates.size());
    for (const auto& candidate : candidates)
    {
        if (campaign && candidate.secret)
            continue;
        const GarageCarEntry entry{
            candidate.catalogIndex,
            !(candidate.owned && candidate.achievementUnlocked)};
        if (candidate.secret && candidate.achievementUnlocked)
            secret.push_back({candidate.catalogIndex, false});
        else if (candidate.owned && candidate.achievementUnlocked)
            available.push_back(entry);
        else
            locked.push_back(entry);
    }
    cars_.clear();
    cars_.reserve(available.size() + secret.size() + locked.size());
    cars_.insert(cars_.end(), available.begin(), available.end());
    cars_.insert(cars_.end(), secret.begin(), secret.end());
    cars_.insert(cars_.end(), locked.begin(), locked.end());

    selection_ = 0U;
    const auto current = std::find_if(
        cars_.begin(), cars_.end(),
        [currentCatalogIndex](const GarageCarEntry& entry) {
            return entry.catalogIndex == currentCatalogIndex;
        });
    if (current != cars_.end())
        selection_ = static_cast<std::size_t>(current - cars_.begin());
    colorsAvailable_ = colorsAvailable;
    focus_ = 0U;
    visibleFirst_ = 0U;
    visibleCount_ = 0U;
    lastVisibleSelection_ = static_cast<std::size_t>(-1);
}

void GarageFrameState::hide() noexcept
{
    cars_.clear();
    selection_ = 0U;
    focus_ = 0U;
    visibleFirst_ = 0U;
    visibleCount_ = 0U;
    lastVisibleSelection_ = static_cast<std::size_t>(-1);
}

void GarageFrameState::updateColors(
    const std::array<bool, colorCount>& colorsAvailable) noexcept
{
    colorsAvailable_ = colorsAvailable;
    if (!focusAvailable(focus_))
        focus_ = 0U;
}

const std::vector<GarageCarEntry>& GarageFrameState::cars() const noexcept
{
    return cars_;
}

bool GarageFrameState::empty() const noexcept
{
    return cars_.empty();
}

std::size_t GarageFrameState::selection() const noexcept
{
    return selection_;
}

const GarageCarEntry* GarageFrameState::selectedCar() const noexcept
{
    return selection_ < cars_.size() ? &cars_[selection_] : nullptr;
}

bool GarageFrameState::canPrevious() const noexcept
{
    return selection_ > 0U && selection_ < cars_.size();
}

bool GarageFrameState::canNext() const noexcept
{
    return selection_ + 1U < cars_.size();
}

bool GarageFrameState::colorAvailable(std::size_t colorIndex) const noexcept
{
    return colorIndex < colorsAvailable_.size() &&
           colorsAvailable_[colorIndex];
}

std::size_t GarageFrameState::focus() const noexcept
{
    return focus_;
}

bool GarageFrameState::focusAvailable(std::size_t focus) const noexcept
{
    if (focus >= focusCount)
        return false;
    if (focus == 0U)
        return !cars_.empty();
    if (focus == 1U)
    {
        const auto* car = selectedCar();
        return car != nullptr && !car->locked;
    }
    if (focus == 2U)
        return canPrevious();
    if (focus == 3U)
        return canNext();
    return colorAvailable(focus - 4U);
}

bool GarageFrameState::setFocus(std::size_t focus) noexcept
{
    if (!focusAvailable(focus))
        return false;
    focus_ = focus;
    return true;
}

std::size_t GarageFrameState::neighbor(
    std::size_t focus, rrr3d::input::Action action) const noexcept
{
    using rrr3d::input::Action;
    const bool left = action == Action::TurnLeft;
    const bool right = action == Action::TurnRight;
    const bool up = action == Action::MenuUp;
    if (focus == 0U)
        return 1U;
    if (focus == 1U)
        return left ? 10U : right ? 17U : up ? 2U : 0U;
    if (focus == 2U)
        return left ? 4U : right ? 3U : 1U;
    if (focus == 3U)
        return left ? 2U : right ? 11U : 1U;
    const bool leftGrid = focus < 11U;
    const std::size_t first = leftGrid ? 4U : 11U;
    const std::size_t last = first + 6U;
    if (left)
        return leftGrid ? 11U : 3U;
    if (right)
        return leftGrid ? 2U : 4U;
    if (up)
        return focus == first ? 1U : focus - 1U;
    return focus == last ? 1U : focus + 1U;
}

std::optional<GarageCommand> GarageFrameState::select(
    std::size_t selection) noexcept
{
    if (selection >= cars_.size() || selection == selection_)
        return std::nullopt;
    selection_ = selection;
    return GarageCommand{
        GarageCommandType::SelectionChanged,
        cars_[selection_].catalogIndex, 0U};
}

std::optional<GarageCommand> GarageFrameState::handle(
    const rrr3d::input::ActionEvent& event) noexcept
{
    if (!event.active)
        return std::nullopt;
    using rrr3d::input::Action;
    if (event.action == Action::MenuUp ||
        event.action == Action::MenuDown ||
        event.action == Action::TurnLeft ||
        event.action == Action::TurnRight)
    {
        std::size_t candidate = focus_;
        for (std::size_t attempts = 0U; attempts < focusCount; ++attempts)
        {
            candidate = neighbor(candidate, event.action);
            if (focusAvailable(candidate))
            {
                focus_ = candidate;
                break;
            }
        }
        return std::nullopt;
    }
    if (!event.repeated && event.action == Action::PreviousWeapon &&
        canPrevious())
        return select(selection_ - 1U);
    if (!event.repeated && event.action == Action::NextWeapon && canNext())
        return select(selection_ + 1U);
    if (event.repeated)
        return std::nullopt;
    if (event.action == Action::MenuBack || event.action == Action::Pause)
        return GarageCommand{GarageCommandType::Back, 0U, 0U};
    if (event.action != Action::MenuConfirm)
        return std::nullopt;
    if (focus_ == 0U)
        return GarageCommand{GarageCommandType::Back, 0U, 0U};
    if (focus_ == 1U && focusAvailable(focus_))
    {
        return GarageCommand{
            GarageCommandType::BuyOrSelect,
            cars_[selection_].catalogIndex, 0U};
    }
    if (focus_ == 2U && canPrevious())
        return select(selection_ - 1U);
    if (focus_ == 3U && canNext())
        return select(selection_ + 1U);
    if (focus_ >= 4U && focusAvailable(focus_))
    {
        return GarageCommand{
            GarageCommandType::SelectColor,
            cars_[selection_].catalogIndex, focus_ - 4U};
    }
    return std::nullopt;
}

GarageVisibleRange GarageFrameState::visibleRange(
    float viewportWidth, float carCellWidth) noexcept
{
    GarageVisibleRange result;
    if (cars_.empty() || carCellWidth <= 0.0F)
        return result;
    const std::size_t count = std::min(
        cars_.size(), std::max<std::size_t>(
                          1U, static_cast<std::size_t>(
                                  std::max(0.0F, viewportWidth - 10.0F) /
                                  carCellWidth)));
    const bool layoutChanged = visibleCount_ != count;
    const bool selectedVisible =
        selection_ >= visibleFirst_ &&
        selection_ < visibleFirst_ + visibleCount_;
    if (lastVisibleSelection_ == static_cast<std::size_t>(-1) ||
        layoutChanged)
    {
        const std::size_t right = std::clamp(
            selection_ + count / 2U, count - 1U, cars_.size() - 1U);
        visibleFirst_ = right + 1U - count;
    }
    else if (selection_ != lastVisibleSelection_ && !selectedVisible)
    {
        if (selection_ > lastVisibleSelection_)
            visibleFirst_ = selection_ + 1U - count;
        else
            visibleFirst_ = std::min(selection_, cars_.size() - count);
    }
    visibleCount_ = count;
    lastVisibleSelection_ = selection_;
    result.first = visibleFirst_;
    result.count = visibleCount_;
    const float space =
        (viewportWidth - static_cast<float>(count) * carCellWidth) * 0.5F;
    result.firstCenterX = space + carCellWidth * 0.5F;
    return result;
}

GarageLayout GarageFrameState::layout(
    float viewportWidth, float viewportHeight, float topPanelHeight,
    float bottomPanelHeight, float leftPanelWidth,
    float rightPanelWidth) const noexcept
{
    GarageLayout result;
    result.topPanelX = viewportWidth * 0.5F;
    result.rightPanelX = viewportWidth + 1.0F;
    result.sidePanelY =
        (topPanelHeight + viewportHeight - bottomPanelHeight) * 0.5F;
    result.leftArrowX = leftPanelWidth + 45.0F;
    result.rightArrowX = viewportWidth - rightPanelWidth - 45.0F;
    result.arrowY = viewportHeight * 0.5F;
    result.backX = -viewportWidth * 0.5F;
    result.backY = -bottomPanelHeight - 8.0F;
    result.buyX = 15.0F;
    result.buyY = -bottomPanelHeight;
    return result;
}

std::vector<WorkshopGoodEntry> WorkshopFrameState::buildGoods(
    std::vector<WorkshopGoodCandidate> candidates)
{
    std::vector<WorkshopGoodEntry> result;
    result.reserve(candidates.size());
    for (const auto& candidate : candidates)
    {
        if (candidate.mobilityFamily || !candidate.unlocked)
            continue;
        result.push_back({candidate.catalogIndex, candidate.cost});
    }
    std::stable_sort(
        result.begin(), result.end(),
        [](const WorkshopGoodEntry& left,
           const WorkshopGoodEntry& right) {
            return left.cost < right.cost;
        });
    return result;
}

void WorkshopFrameState::show(
    std::vector<WorkshopGoodCandidate> candidates,
    const std::array<WorkshopSlotState, slotCount>& slots)
{
    goods_ = buildGoods(std::move(candidates));
    slots_ = slots;
    scroll_ = 0U;
    focus_ = 0U;
    drag_ = {};
    confirmation_ = {};
}

void WorkshopFrameState::hide() noexcept
{
    goods_.clear();
    scroll_ = 0U;
    focus_ = 0U;
    drag_ = {};
    confirmation_ = {};
}

void WorkshopFrameState::updateGoods(
    std::vector<WorkshopGoodCandidate> candidates)
{
    goods_ = buildGoods(std::move(candidates));
    scroll_ = std::min(scroll_, maximumScroll());
    if (!pointerFocusAvailable(focus_))
        focus_ = 0U;
}

void WorkshopFrameState::updateSlots(
    const std::array<WorkshopSlotState, slotCount>& slots) noexcept
{
    slots_ = slots;
    if (focus_ >= firstSlotFocus && !pointerFocusAvailable(focus_))
        focus_ = 0U;
}

const std::vector<WorkshopGoodEntry>&
WorkshopFrameState::goods() const noexcept
{
    return goods_;
}

std::size_t WorkshopFrameState::scroll() const noexcept
{
    return scroll_;
}

std::size_t WorkshopFrameState::maximumScroll() const noexcept
{
    const std::size_t rows =
        (goods_.size() + goodColumns - 1U) / goodColumns;
    return rows > 4U ? rows - 4U : 0U;
}

bool WorkshopFrameState::scrollGoods(int step) noexcept
{
    const std::size_t previous = scroll_;
    if (step < 0)
        scroll_ = scroll_ == 0U ? 0U : scroll_ - 1U;
    else if (step > 0)
        scroll_ = std::min(scroll_ + 1U, maximumScroll());
    return scroll_ != previous;
}

const WorkshopGoodEntry* WorkshopFrameState::visibleGood(
    std::size_t visibleIndex) const noexcept
{
    if (visibleIndex >= visibleGoodCount)
        return nullptr;
    const std::size_t index =
        scroll_ * goodColumns + visibleIndex;
    return index < goods_.size() ? &goods_[index] : nullptr;
}

std::size_t WorkshopFrameState::focus() const noexcept
{
    return focus_;
}

bool WorkshopFrameState::pointerFocusAvailable(
    std::size_t focus) const noexcept
{
    if (focus == 0U)
        return true;
    if (focus >= firstGoodFocus && focus < firstSlotFocus)
        return visibleGood(focus - firstGoodFocus) != nullptr;
    if (focus >= firstSlotFocus && focus < firstSlotFocus + slotCount)
        return slots_[focus - firstSlotFocus].active;
    return false;
}

bool WorkshopFrameState::keyboardFocusAvailable(
    std::size_t focus) const noexcept
{
    if (focus == 0U)
        return true;
    if (focus < firstSlotFocus || focus >= firstSlotFocus + slotCount)
        return false;
    const auto& slot = slots_[focus - firstSlotFocus];
    return slot.active && slot.installed && slot.controlEnabled &&
           (slot.chargeControlVisible || slot.levelControlVisible);
}

bool WorkshopFrameState::setPointerFocus(std::size_t focus) noexcept
{
    if (!pointerFocusAvailable(focus))
        return false;
    focus_ = focus;
    return true;
}

std::size_t WorkshopFrameState::neighbor(
    std::size_t focus, rrr3d::input::Action action) const noexcept
{
    using rrr3d::input::Action;
    if (focus > 0U && focus < firstSlotFocus)
        focus = 0U;
    if (focus == 0U)
        return firstSlotFocus + 2U;
    const std::size_t slot = focus - firstSlotFocus;
    std::size_t lastWeapon = 6U;
    for (std::size_t index = 6U; index <= 9U; ++index)
        if (slots_[index].chargeControlVisible)
            lastWeapon = index;
    const bool left = action == Action::TurnLeft;
    const bool right = action == Action::TurnRight;
    const bool up = action == Action::MenuUp;
    std::size_t next = slot;
    if (slot == 2U)
        next = left || up ? 0U : right ? 3U : slotCount;
    else if (slot == 3U)
        next = left ? 2U : right || up ? 5U : slotCount;
    else if (slot == 5U)
        next = left || (!up && !right) ? 3U : 4U;
    else if (slot == 4U)
        next = right || (!up && !left) ? 5U : lastWeapon;
    else if (slot == 9U)
        next = left || up ? 8U : 4U;
    else if (slot >= 7U && slot <= 8U)
        next = left || up ? slot - 1U : slot + 1U;
    else if (slot == 6U)
        next = right ? 7U : 1U;
    else if (slot == 1U)
        next = left || (!up && !right) ? 0U : 6U;
    else if (slot == 0U)
        next = left || up ? 1U : 2U;
    return next == slotCount ? 0U : firstSlotFocus + next;
}

std::optional<WorkshopCommand> WorkshopFrameState::handle(
    const rrr3d::input::ActionEvent& event,
    bool pointerSlotPlane) noexcept
{
    if (!event.active)
        return std::nullopt;
    using rrr3d::input::Action;
    if (event.action == Action::MenuUp ||
        event.action == Action::MenuDown ||
        event.action == Action::TurnLeft ||
        event.action == Action::TurnRight)
    {
        // Goods are mouse-only in the original navigation registry.  A raw
        // direction after hovering one starts from the registered Back key,
        // not from the unregistered good widget.
        if (focus_ > 0U && focus_ < firstSlotFocus)
            focus_ = 0U;
        std::size_t candidate = focus_;
        for (std::size_t attempts = 0U; attempts < slotCount + 1U;
             ++attempts)
        {
            candidate = neighbor(candidate, event.action);
            if (keyboardFocusAvailable(candidate))
            {
                focus_ = candidate;
                break;
            }
        }
        return std::nullopt;
    }
    if (event.repeated)
        return std::nullopt;
    if (event.action == Action::MenuBack || event.action == Action::Pause)
        return WorkshopCommand{WorkshopCommandType::Back, 0U, false};
    if (event.action != Action::MenuConfirm)
        return std::nullopt;
    if (focus_ == 0U)
        return WorkshopCommand{WorkshopCommandType::Back, 0U, false};
    if (focus_ >= firstGoodFocus && focus_ < firstSlotFocus)
    {
        const auto* good = visibleGood(focus_ - firstGoodFocus);
        if (good != nullptr)
        {
            return WorkshopCommand{
                WorkshopCommandType::ActivateGood,
                good->catalogIndex, false};
        }
        return std::nullopt;
    }
    if (focus_ >= firstSlotFocus &&
        focus_ < firstSlotFocus + slotCount)
    {
        const std::size_t slot = focus_ - firstSlotFocus;
        if (!pointerSlotPlane && !keyboardFocusAvailable(focus_))
            return std::nullopt;
        return WorkshopCommand{
            WorkshopCommandType::ActivateSlot, slot,
            pointerSlotPlane};
    }
    return std::nullopt;
}

WorkshopDragState& WorkshopFrameState::drag() noexcept
{
    return drag_;
}

const WorkshopDragState& WorkshopFrameState::drag() const noexcept
{
    return drag_;
}

void WorkshopFrameState::startDrag(
    originalrace::ProfileSlot item,
    std::optional<originalrace::GarageSlotType> origin)
{
    drag_.item = std::move(item);
    drag_.origin = origin;
}

void WorkshopFrameState::clearDrag() noexcept
{
    drag_ = {};
}

const WorkshopConfirmationState&
WorkshopFrameState::confirmation() const noexcept
{
    return confirmation_;
}

void WorkshopFrameState::beginConfirmation(
    WorkshopConfirmationType type,
    std::size_t pendingCatalogIndex) noexcept
{
    confirmation_.type = type;
    confirmation_.pendingCatalogIndex = pendingCatalogIndex;
    confirmation_.yesFocused = true;
}

void WorkshopFrameState::cancelConfirmation() noexcept
{
    confirmation_ = {};
}

void WorkshopFrameState::setConfirmationYesFocused(bool value) noexcept
{
    confirmation_.yesFocused = value;
}

WorkshopLayout WorkshopFrameState::layout(
    float viewportWidth, float viewportHeight, float topPanelHeight,
    float bottomPanelHeight, float leftPanelWidth, float leftPanelHeight,
    float slotWidth, float slotHeight) const noexcept
{
    WorkshopLayout result;
    const float panelCenterY =
        (topPanelHeight - 30.0F + viewportHeight - bottomPanelHeight) *
        0.5F;
    const float panelTop = panelCenterY - leftPanelHeight * 0.5F;
    constexpr float cell = 100.0F;
    const float firstX = 30.0F + 22.0F + 39.0F;
    const float firstY = panelTop + 11.0F + 38.0F;
    for (std::size_t index = 0U; index < result.goods.size(); ++index)
    {
        result.goods[index] = {
            firstX + static_cast<float>(index % 3U) * cell,
            firstY + static_cast<float>(index / 3U) * cell};
    }
    const float scale = viewportHeight / 720.0F;
    const float centerX = viewportWidth * 0.63F;
    const float centerY = viewportHeight * 0.451F;
    const float leftOffset = std::max(
        centerX - 330.0F * scale,
        30.0F + leftPanelWidth + 80.0F);
    const float rightOffset = std::min(
        centerX + 330.0F * scale, viewportWidth - 120.0F);
    const float topOffset = std::max(
        centerY - 180.0F * scale, topPanelHeight + 65.0F);
    const float bottomOffset = std::min(
        centerY + 180.0F * scale,
        viewportHeight - bottomPanelHeight - 65.0F);
    constexpr float slotSpace = 15.0F;
    result.slots = {{
        {leftOffset, centerY + slotHeight * 0.5F + slotSpace},
        {leftOffset, centerY - slotHeight * 0.5F - slotSpace},
        {centerX - slotWidth * 0.5F - 6.0F * slotSpace, bottomOffset},
        {centerX + slotWidth * 0.5F + 6.0F * slotSpace, bottomOffset},
        {rightOffset, centerY - slotHeight * 0.5F - slotSpace},
        {rightOffset, centerY + slotHeight * 0.5F + slotSpace},
        {centerX - (slotWidth + 4.5F * slotSpace) * 1.5F, topOffset},
        {centerX - (slotWidth + 4.5F * slotSpace) * 0.5F, topOffset},
        {centerX + (slotWidth + 4.5F * slotSpace) * 0.5F, topOffset},
        {centerX + (slotWidth + 4.5F * slotSpace) * 1.5F, topOffset},
    }};
    result.arrowX = 30.0F + leftPanelWidth * 0.5F;
    result.upArrowY = panelCenterY - leftPanelHeight * 0.5F + 65.0F;
    result.downArrowY =
        panelCenterY + leftPanelHeight * 0.5F - 42.0F;
    result.backY = viewportHeight - bottomPanelHeight + 40.0F;
    return result;
}

SpaceshipLampState SpaceshipFrameState::progress(
    float deltaTime) noexcept
{
    redLampTime_ = std::fmod(redLampTime_, 3.0F);
    const float intensity = 0.7F * (
        std::clamp((redLampTime_ - 1.5F) / 0.15F, 0.0F, 1.0F) -
        std::clamp((redLampTime_ - 2.85F) / 0.15F, 0.0F, 1.0F));
    redLampTime_ += std::max(deltaTime, 0.0F);
    sceneSeconds_ += std::max(deltaTime, 0.0F);
    return {redLampTime_ >= 1.5F, intensity};
}

float SpaceshipFrameState::sceneSeconds() const noexcept
{
    return sceneSeconds_;
}

float AngarLayout::planetX(std::size_t index) const noexcept
{
    return firstPlanetX + static_cast<float>(index) * 224.0F;
}

void AngarFrameState::show(
    std::vector<AngarPlanetEntry> planets, bool planetChampion,
    bool campaign, bool networkClient, std::size_t currentPlanet)
{
    planets_ = std::move(planets);
    planetChampion_ = planetChampion;
    campaign_ = campaign;
    networkClient_ = networkClient;
    currentPlanet_ = currentPlanet;
    selection_ = -1;
    if (planetChampion_ && currentPlanet_ + 1U < planets_.size() &&
        planets_[currentPlanet_ + 1U].next)
    {
        selection_ = static_cast<int>(currentPlanet_ + 1U);
    }
    previousSelection_ = -1;
    doorTime_ = -1.0F;
    focus_ = selection_ >= 0
                 ? static_cast<std::size_t>(selection_)
                 : planets_.size();
    travelDialog_ = {};
}

void AngarFrameState::hide() noexcept
{
    planets_.clear();
    selection_ = -1;
    previousSelection_ = -1;
    doorTime_ = -1.0F;
    focus_ = 0U;
    travelDialog_ = {};
}

const std::vector<AngarPlanetEntry>&
AngarFrameState::planets() const noexcept
{
    return planets_;
}

std::size_t AngarFrameState::planetCount() const noexcept
{
    return planets_.size();
}

int AngarFrameState::selection() const noexcept
{
    return selection_;
}

int AngarFrameState::previousSelection() const noexcept
{
    return previousSelection_;
}

std::size_t AngarFrameState::focus() const noexcept
{
    return focus_;
}

bool AngarFrameState::selectPlanet(int index) noexcept
{
    if (index < -1 ||
        (index >= 0 && static_cast<std::size_t>(index) >= planets_.size()) ||
        index == selection_)
    {
        return false;
    }
    previousSelection_ = selection_;
    selection_ = index;
    doorTime_ = 0.0F;
    return true;
}

bool AngarFrameState::setPointerFocus(std::size_t focus) noexcept
{
    if (focus > planets_.size())
        return false;
    focus_ = focus;
    selectPlanet(
        focus < planets_.size() ? static_cast<int>(focus) : -1);
    return true;
}

void AngarFrameState::closeInfo() noexcept
{
    focus_ = planets_.size();
    selectPlanet(-1);
}

void AngarFrameState::progress(float deltaTime) noexcept
{
    if (doorTime_ < 0.0F)
        return;
    const float alpha = std::clamp(doorTime_ / 0.25F, 0.0F, 1.0F);
    doorTime_ += std::max(deltaTime, 0.0F);
    if (alpha >= 1.0F)
        doorTime_ = -1.0F;
}

float AngarFrameState::doorAlpha(std::size_t index) const noexcept
{
    if (networkClient_)
        return 0.0F;
    float alpha = static_cast<int>(index) == selection_ ? 1.0F : 0.0F;
    if (doorTime_ >= 0.0F)
    {
        const float progress =
            std::clamp(doorTime_ / 0.25F, 0.0F, 1.0F);
        if (static_cast<int>(index) == selection_)
            alpha = progress;
        else if (static_cast<int>(index) == previousSelection_)
            alpha = 1.0F - progress;
    }
    return alpha;
}

const AngarTravelDialogState&
AngarFrameState::travelDialog() const noexcept
{
    return travelDialog_;
}

void AngarFrameState::cancelTravel() noexcept
{
    travelDialog_ = {};
}

void AngarFrameState::setTravelYesFocused(bool value) noexcept
{
    travelDialog_.yesFocused = value;
}

std::optional<AngarCommand> AngarFrameState::requestTravel(
    std::size_t index, bool fromPlanetSlot) noexcept
{
    if (networkClient_ || index >= planets_.size())
        return std::nullopt;
    travelDialog_ = {true, index, true, fromPlanetSlot};
    return AngarCommand{
        AngarCommandType::RequestTravel, index, fromPlanetSlot};
}

std::optional<AngarCommand> AngarFrameState::activateFocus() noexcept
{
    if (focus_ >= planets_.size())
    {
        if (planetChampion_ && currentPlanet_ < planets_.size())
            return requestTravel(currentPlanet_, false);
        return AngarCommand{AngarCommandType::Back, 0U, false};
    }
    if (networkClient_)
        return std::nullopt;
    const std::size_t index = focus_;
    const auto& planet = planets_[index];
    if (planetChampion_ && (planet.current || planet.next))
        return requestTravel(index, true);
    if (!campaign_ && planet.state == AngarPlanetState::Open)
        return AngarCommand{
            AngarCommandType::ChangePlanet, index, true};
    if (planet.current)
        return AngarCommand{AngarCommandType::Back, index, true};
    return AngarCommand{
        AngarCommandType::CannotTravel, index, true};
}

std::optional<AngarCommand> AngarFrameState::handle(
    const rrr3d::input::ActionEvent& event) noexcept
{
    if (!event.active)
        return std::nullopt;
    using rrr3d::input::Action;
    if (travelDialog_.visible)
    {
        if (event.action == Action::TurnLeft ||
            event.action == Action::MenuUp)
        {
            travelDialog_.yesFocused = true;
        }
        else if (event.action == Action::TurnRight ||
                 event.action == Action::MenuDown)
        {
            travelDialog_.yesFocused = false;
        }
        else if (!event.repeated &&
                 (event.action == Action::MenuBack ||
                  event.action == Action::Pause))
        {
            cancelTravel();
        }
        else if (!event.repeated &&
                 event.action == Action::MenuConfirm)
        {
            const auto dialog = travelDialog_;
            cancelTravel();
            if (dialog.yesFocused)
            {
                return AngarCommand{
                    AngarCommandType::ChangePlanet, dialog.target,
                    dialog.fromPlanetSlot};
            }
        }
        return std::nullopt;
    }
    if (event.action == Action::TurnLeft ||
        event.action == Action::TurnRight)
    {
        if (event.repeated || planets_.empty() ||
            focus_ >= planets_.size())
        {
            return std::nullopt;
        }
        if (event.action == Action::TurnLeft)
            focus_ = focus_ == 0U ? planets_.size() - 1U : focus_ - 1U;
        else
            focus_ = (focus_ + 1U) % planets_.size();
        selectPlanet(static_cast<int>(focus_));
        return std::nullopt;
    }
    if (event.action == Action::MenuUp ||
        event.action == Action::MenuDown)
    {
        if (planets_.empty())
            return std::nullopt;
        if (focus_ < planets_.size())
        {
            focus_ = planets_.size();
            selectPlanet(-1);
        }
        else
        {
            focus_ = 0U;
            selectPlanet(0);
        }
        return std::nullopt;
    }
    if (event.repeated)
        return std::nullopt;
    if (event.action == Action::MenuBack || event.action == Action::Pause)
    {
        if (planetChampion_ && currentPlanet_ < planets_.size())
            return requestTravel(currentPlanet_, false);
        return AngarCommand{AngarCommandType::Back, 0U, false};
    }
    if (event.action == Action::MenuConfirm)
        return activateFocus();
    return std::nullopt;
}

AngarLayout AngarFrameState::layout(
    float viewportWidth, float viewportHeight, float bottomPanelWidth,
    float bottomPanelHeight, float infoWidth, float infoHeight,
    float backWidth) const noexcept
{
    AngarLayout result;
    result.bottomPanelX = viewportWidth * 0.5F;
    result.bottomPanelY =
        viewportHeight - bottomPanelHeight * 0.5F - 20.0F;
    result.firstPlanetX =
        result.bottomPanelX - bottomPanelWidth * 0.5F + 125.0F;
    result.planetY =
        result.bottomPanelY - bottomPanelHeight * 0.5F + 90.0F;
    result.slotY = result.bottomPanelY + 67.0F;
    result.backX = backWidth * 0.5F;
    result.backY = 40.0F;
    if (selection_ >= 0)
    {
        result.infoX = std::clamp(
            result.planetX(static_cast<std::size_t>(selection_)),
            infoWidth * 0.5F, viewportWidth - infoWidth * 0.5F);
        result.infoY = result.bottomPanelY - 260.0F;
        result.infoLeft = result.infoX - infoWidth * 0.5F;
        result.infoTop = result.infoY - infoHeight * 0.5F;
        result.closeX = result.infoX + 180.0F;
        result.closeY = result.infoTop + 15.0F;
    }
    return result;
}

namespace
{

constexpr std::array<AchievementDefinition, 9U>
    achievementDefinitions{{
        {"viper", "Data/GUI/Rewards/viperLock.png",
         "Data/GUI/Rewards/viper.png", 200.0F, 105.0F},
        {"buggi", "Data/GUI/Rewards/buggiLock.png",
         "Data/GUI/Rewards/buggi.png", 405.0F, 155.0F},
        {"airblade", "Data/GUI/Rewards/airbladeLock.png",
         "Data/GUI/Rewards/airblade.png", 0.0F, 245.0F},
        {"reflector", "Data/GUI/Rewards/reflectorLock.png",
         "Data/GUI/Rewards/reflector.png", -190.0F, 125.0F},
        {"droid", "Data/GUI/Rewards/droidLock.png",
         "Data/GUI/Rewards/droid.png", -380.0F, 95.0F},
        {"tankchetti", "Data/GUI/Rewards/tankchettiLock.png",
         "Data/GUI/Rewards/tankchetti.png", -325.0F, 265.0F},
        {"phaser", "Data/GUI/Rewards/phaserLock.png",
         "Data/GUI/Rewards/phaser.png", -190.0F, 400.0F},
        {"mustang", "Data/GUI/Rewards/mustangLock.png",
         "Data/GUI/Rewards/mustang.png", 205.0F, 375.0F},
        {"armor4", "Data/GUI/Rewards/musicTrackLock.png",
         "Data/GUI/Rewards/musicTrack.png", 445.0F, 315.0F},
    }};

constexpr std::size_t noTarget = AchievementFrameState::noFocus;

// Menu::NavDir order is left, right, up, down.  This is the literal graph
// registered by AchievmentFrame::UpdateAchievments.
constexpr std::array<std::array<std::size_t, 4U>, 10U>
    achievementNavigation{{
        {3U, 1U, 9U, 2U},          // viper
        {0U, 4U, 9U, 8U},          // buggi
        {5U, 8U, 3U, 6U},          // airblade
        {4U, 0U, 9U, 5U},          // reflector
        {1U, 3U, 9U, 5U},          // droid
        {8U, 2U, 4U, 6U},          // tankchetti
        {5U, 7U, 2U, 9U},          // phaser
        {6U, 8U, 2U, 9U},          // mustang
        {7U, 5U, 1U, 9U},          // musicTrack
        {noTarget, noTarget, 6U, 6U}, // Back
    }};

std::array<std::size_t, 4U> directionOrder(
    std::size_t direction) noexcept
{
    const std::size_t opposite = direction == 0U ? 1U
        : direction == 1U ? 0U
        : direction == 2U ? 3U
                          : 2U;
    const std::size_t third = direction == 0U ? 2U
        : direction == 1U ? 3U
        : direction == 2U ? 0U
                          : 1U;
    const std::size_t fourth = direction == 0U ? 3U
        : direction == 1U ? 2U
        : direction == 2U ? 1U
                          : 0U;
    return {direction, opposite, third, fourth};
}

bool sameAxis(std::size_t left, std::size_t right) noexcept
{
    return (left < 2U) == (right < 2U);
}

} // namespace

const std::array<AchievementDefinition,
                 AchievementFrameState::achievementCount>&
AchievementFrameState::definitions() noexcept
{
    return achievementDefinitions;
}

float AchievementLayout::cardX(std::size_t index) const noexcept
{
    if (index >= achievementDefinitions.size())
        return centerX;
    return centerX + (achievementDefinitions[index].x - 30.0F) * scale;
}

float AchievementLayout::cardY(std::size_t index) const noexcept
{
    if (index >= achievementDefinitions.size())
        return centerY;
    return achievementDefinitions[index].y * scale;
}

void AchievementFrameState::show(
    const std::array<AchievementEntry, achievementCount>& entries) noexcept
{
    entries_ = entries;
    focus_ = noFocus;
    confirmation_ = {};
}

void AchievementFrameState::hide() noexcept
{
    entries_ = {};
    focus_ = noFocus;
    confirmation_ = {};
}

void AchievementFrameState::update(
    const std::array<AchievementEntry, achievementCount>& entries) noexcept
{
    entries_ = entries;
    if (focus_ < achievementCount && !focusable(focus_))
        focus_ = noFocus;
}

const std::array<AchievementEntry,
                 AchievementFrameState::achievementCount>&
AchievementFrameState::entries() const noexcept
{
    return entries_;
}

const AchievementEntry* AchievementFrameState::entry(
    std::size_t index) const noexcept
{
    return index < achievementCount ? &entries_[index] : nullptr;
}

std::size_t AchievementFrameState::focus() const noexcept
{
    return focus_;
}

bool AchievementFrameState::setPointerFocus(std::size_t focus) noexcept
{
    if (focus > backFocus)
        return false;
    focus_ = focus;
    return true;
}

bool AchievementFrameState::focusable(std::size_t focus) const noexcept
{
    return focus == backFocus ||
           (focus < achievementCount &&
            entries_[focus].state == AchievementState::Unlocked);
}

const AchievementConfirmationState&
AchievementFrameState::confirmation() const noexcept
{
    return confirmation_;
}

void AchievementFrameState::cancelPurchase() noexcept
{
    confirmation_ = {};
}

void AchievementFrameState::setPurchaseYesFocused(bool value) noexcept
{
    confirmation_.yesFocused = value;
}

std::size_t AchievementFrameState::direction(
    rrr3d::input::Action action) noexcept
{
    using rrr3d::input::Action;
    return action == Action::TurnLeft ? 0U
        : action == Action::TurnRight ? 1U
        : action == Action::MenuUp ? 2U
                                   : 3U;
}

std::size_t AchievementFrameState::findFocusable(
    std::size_t element, std::vector<std::size_t> ignored,
    std::size_t navigationDirection) const noexcept
{
    if (element > backFocus || navigationDirection >= 4U)
        return noFocus;
    for (const std::size_t edge : directionOrder(navigationDirection))
    {
        const std::size_t next = achievementNavigation[element][edge];
        if (next == noTarget)
            continue;
        if (!ignored.empty() && next == ignored.front() &&
            sameAxis(edge, navigationDirection) && focusable(element))
        {
            return element;
        }
        if (std::find(ignored.begin(), ignored.end(), next) != ignored.end())
            continue;
        if (sameAxis(edge, navigationDirection) && focusable(next))
            return next;
        ignored.push_back(element);
        const std::size_t found =
            findFocusable(next, ignored, navigationDirection);
        if (found != noFocus)
            return found;
    }
    return noFocus;
}

void AchievementFrameState::moveFocus(
    std::size_t navigationDirection) noexcept
{
    if (navigationDirection >= 4U)
        return;
    // Menu::OnHandleInput focuses the registered key (Back) on the first
    // direction when no widget currently owns focus; it does not also move.
    if (focus_ == noFocus)
    {
        focus_ = backFocus;
        return;
    }
    const std::size_t next =
        achievementNavigation[focus_][navigationDirection];
    if (next == noTarget)
        return;
    if (focusable(next))
    {
        focus_ = next;
        return;
    }
    const std::size_t found = findFocusable(
        next, {focus_}, navigationDirection);
    if (found != noFocus)
        focus_ = found;
}

std::optional<AchievementCommand> AchievementFrameState::handle(
    const rrr3d::input::ActionEvent& event) noexcept
{
    if (!event.active)
        return std::nullopt;
    using rrr3d::input::Action;
    if (confirmation_.visible)
    {
        if (event.action == Action::TurnLeft ||
            event.action == Action::MenuUp)
        {
            confirmation_.yesFocused = true;
        }
        else if (event.action == Action::TurnRight ||
                 event.action == Action::MenuDown)
        {
            confirmation_.yesFocused = false;
        }
        else if (!event.repeated &&
                 (event.action == Action::MenuBack ||
                  event.action == Action::Pause))
        {
            cancelPurchase();
        }
        else if (!event.repeated &&
                 event.action == Action::MenuConfirm)
        {
            const auto pending = confirmation_.pending;
            const bool accepted = confirmation_.yesFocused;
            cancelPurchase();
            if (accepted && pending < achievementCount &&
                entries_[pending].state == AchievementState::Unlocked)
            {
                return AchievementCommand{
                    AchievementCommandType::Purchase, pending};
            }
        }
        return std::nullopt;
    }
    if (event.action == Action::TurnLeft ||
        event.action == Action::TurnRight ||
        event.action == Action::MenuUp ||
        event.action == Action::MenuDown)
    {
        moveFocus(direction(event.action));
        return std::nullopt;
    }
    if (event.repeated)
        return std::nullopt;
    if (event.action == Action::MenuBack || event.action == Action::Pause)
        return AchievementCommand{AchievementCommandType::Back, 0U};
    if (event.action != Action::MenuConfirm)
        return std::nullopt;
    if (focus_ == backFocus)
        return AchievementCommand{AchievementCommandType::Back, 0U};
    if (focus_ < achievementCount &&
        entries_[focus_].state == AchievementState::Unlocked)
    {
        confirmation_ = {true, focus_, true};
        return AchievementCommand{
            AchievementCommandType::RequestPurchase, focus_};
    }
    return std::nullopt;
}

AchievementLayout AchievementFrameState::layout(
    float viewportWidth, float viewportHeight, float backWidth,
    float backHeight) const noexcept
{
    AchievementLayout result;
    result.scale = std::min(
        viewportWidth / 1090.0F, viewportHeight / 720.0F);
    result.centerX = viewportWidth * 0.5F;
    result.centerY = viewportHeight * 0.5F;
    result.bottomPanelY = viewportHeight - 80.0F;
    result.rewardsY = 555.0F * result.scale;
    result.pointsY = result.bottomPanelY + 40.0F;
    result.backX = backWidth * 0.5F;
    result.backY = result.bottomPanelY + 17.0F + backHeight * 0.5F;
    return result;
}

} // namespace r3d::game::originalracemenu
