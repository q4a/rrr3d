#include "OriginalHudMenu.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <list>

namespace r3d::game::originalrace::source
{

bool MiniMapFrame::Build(const Race& race, float viewportWidth)
{
    Clear();
    if (race.tracePath.size() < 2U || race.tracePoints.empty())
        return false;

    auto tracePoint =
        [&](std::uint32_t id) -> const TracePoint* {
        const auto found = std::find_if(
            race.tracePoints.begin(), race.tracePoints.end(),
            [id](const TracePoint& candidate) {
                return candidate.id == id;
            });
        return found == race.tracePoints.end() ? nullptr : &*found;
    };

    using Vec2 = std::array<float, 2>;
    auto add = [](Vec2 first, Vec2 second) {
        return Vec2{first[0] + second[0], first[1] + second[1]};
    };
    auto subtract = [](Vec2 first, Vec2 second) {
        return Vec2{first[0] - second[0], first[1] - second[1]};
    };
    auto multiply = [](Vec2 value, float amount) {
        return Vec2{value[0] * amount, value[1] * amount};
    };
    auto dot = [](Vec2 first, Vec2 second) {
        return first[0] * second[0] + first[1] * second[1];
    };
    auto normalize = [](Vec2 value) {
        const float length = std::hypot(value[0], value[1]);
        return length > 0.00001F
                   ? Vec2{value[0] / length, value[1] / length}
                   : Vec2{1.0F, 0.0F};
    };
    auto normalCcw = [](Vec2 value) {
        return Vec2{-value[1], value[0]};
    };
    auto normalCw = [](Vec2 value) {
        return Vec2{value[1], -value[0]};
    };

    struct Node
    {
        Vec2 position{};
        float size = 0.0F;
        Vec2 direction{};
        Vec2 previousDirection{};
        Vec2 middleDirection{};
        Vec2 middleNormal{};
        Vec2 edgeNormal{};
        float cosineDelta = 1.0F;
        float sineHalfAngle = 1.0F;
        float radius = 0.0F;
        bool counterClockwise = false;
    };
    using Nodes = std::list<Node>;

    auto computeNode = [&](Nodes& nodes, Nodes::iterator iterator) {
        const auto next = std::next(iterator);
        auto previous = iterator;
        if (iterator != nodes.begin())
            --previous;
        else
            previous = nodes.end();
        iterator->direction =
            next != nodes.end()
                ? normalize(subtract(next->position,
                                     iterator->position))
                : normalize(subtract(iterator->position,
                                     previous->position));
        iterator->previousDirection =
            previous != nodes.end()
                ? normalize(subtract(iterator->position,
                                     previous->position))
                : iterator->direction;
        iterator->middleDirection = normalize(
            add(iterator->previousDirection, iterator->direction));
        iterator->middleNormal = normalCcw(iterator->middleDirection);
        iterator->cosineDelta = std::abs(
            dot(iterator->direction, iterator->previousDirection));
        iterator->sineHalfAngle = std::sqrt(
            (1.0F + iterator->cosineDelta) * 0.5F);
        iterator->radius = 0.5F * iterator->size /
            std::max(iterator->sineHalfAngle, 0.00001F);
        iterator->counterClockwise =
            iterator->previousDirection[0] * iterator->direction[1] -
                iterator->previousDirection[1] *
                    iterator->direction[0] >
            0.0F;
        iterator->edgeNormal =
            iterator->counterClockwise
                ? normalCcw(iterator->middleDirection)
                : normalCw(iterator->middleDirection);
    };
    auto alignNode = [&](const Node& source, Node& destination,
                         float cosineError, float sizeError) {
        const auto direction = normalize(
            subtract(destination.position, source.position));
        if (std::abs(direction[0]) > cosineError)
            destination.position[1] = source.position[1];
        if (std::abs(direction[1]) > cosineError)
            destination.position[0] = source.position[0];
        if (std::abs(destination.size - source.size) < sizeError)
            destination.size = source.size;
    };
    auto alignMiddleNodes =
        [&](Node& first, Node& second, float cosineError,
            float sizeError) {
        const auto direction = normalize(
            subtract(second.position, first.position));
        if (std::abs(direction[0]) > cosineError)
        {
            first.position[1] =
                (first.position[1] + second.position[1]) * 0.5F;
            second.position[1] = first.position[1];
        }
        if (std::abs(direction[1]) > cosineError)
        {
            first.position[0] =
                (first.position[0] + second.position[0]) * 0.5F;
            second.position[0] = first.position[0];
        }
        if (std::abs(second.size - first.size) < sizeError)
        {
            first.size = (first.size + second.size) * 0.5F;
            second.size = first.size;
        }
    };
    auto makeNodes =
        [&](const std::vector<std::uint32_t>& ids) {
        Nodes nodes;
        for (const auto id : ids)
        {
            if (const auto* point = tracePoint(id))
            {
                nodes.push_back(
                    {{point->position.x, point->position.y},
                     point->width});
            }
        }
        if (nodes.size() < 2U)
            return nodes;

        constexpr float radians20 =
            20.0F * 3.14159265358979323846F / 180.0F;
        const float cosineError = std::cos(radians20);
        constexpr float sizeError = 2.0F;
        constexpr float smoothingRadius = 10.0F;
        constexpr int smoothingSlices = 2;
        for (auto iterator = nodes.begin(); iterator != nodes.end();)
        {
            const auto next = std::next(iterator);
            if (next == nodes.end())
                break;
            const auto last = std::prev(nodes.end());
            if (next != last)
                alignNode(*iterator, *next, cosineError, sizeError);
            else
            {
                alignMiddleNodes(*next, nodes.front(), cosineError,
                                 sizeError);
                alignNode(*next, *iterator, cosineError, sizeError);
                alignNode(nodes.front(), *std::next(nodes.begin()),
                          cosineError, sizeError);
            }

            computeNode(nodes, iterator);
            if (smoothingSlices > 0 &&
                iterator->cosineDelta < cosineError)
            {
                const float cosineHalfAngle = std::sqrt(std::max(
                    0.0F, 1.0F - iterator->sineHalfAngle *
                                      iterator->sineHalfAngle));
                if (cosineHalfAngle <= 0.00001F)
                {
                    iterator = next;
                    continue;
                }
                const float size = iterator->size;
                const auto smoothingCenter = add(
                    iterator->position,
                    multiply(iterator->edgeNormal,
                             smoothingRadius / cosineHalfAngle));
                const auto smoothingVector =
                    multiply(iterator->edgeNormal, -1.0F);
                const float halfAngle = std::asin(std::clamp(
                    iterator->sineHalfAngle, 0.0F, 1.0F));
                const bool counterClockwise =
                    iterator->counterClockwise;
                iterator = nodes.erase(iterator);
                for (int slice = -smoothingSlices;
                     slice <= smoothingSlices; ++slice)
                {
                    float angle =
                        static_cast<float>(slice) /
                        static_cast<float>(smoothingSlices) * halfAngle;
                    if (!counterClockwise)
                        angle = -angle;
                    const float sine = std::sin(angle);
                    const float cosine = std::cos(angle);
                    const Vec2 rotated{
                        smoothingVector[0] * cosine -
                            smoothingVector[1] * sine,
                        smoothingVector[0] * sine +
                            smoothingVector[1] * cosine};
                    iterator = nodes.insert(
                        iterator,
                        {add(smoothingCenter,
                             multiply(rotated, smoothingRadius)),
                         size});
                    ++iterator;
                }
            }
            else
                iterator = next;
        }
        for (auto iterator = nodes.begin(); iterator != nodes.end();
             ++iterator)
            computeNode(nodes, iterator);
        return nodes;
    };

    auto pathIds = race.tracePaths;
    if (pathIds.empty())
        pathIds.push_back(race.tracePath);
    std::vector<Nodes> paths;
    paths.reserve(pathIds.size());
    for (const auto& ids : pathIds)
    {
        auto nodes = makeNodes(ids);
        if (nodes.size() > 1U)
            paths.push_back(std::move(nodes));
    }
    if (paths.empty())
        return false;

    float maximumX = -std::numeric_limits<float>::max();
    float maximumY = -std::numeric_limits<float>::max();
    minimumX_ = std::numeric_limits<float>::max();
    minimumY_ = std::numeric_limits<float>::max();
    for (const auto& nodes : paths)
    {
        for (const auto& node : nodes)
        {
            const auto first = add(
                node.position,
                multiply(node.middleNormal, node.radius));
            const auto second = subtract(
                node.position,
                multiply(node.middleNormal, node.radius));
            minimumX_ = std::min(
                {minimumX_, first[0], second[0]});
            minimumY_ = std::min(
                {minimumY_, first[1], second[1]});
            maximumX = std::max({maximumX, first[0], second[0]});
            maximumY = std::max({maximumY, first[1], second[1]});
        }
    }

    const float mapSize = HudMenu::GetMiniMapRect().size.x;
    const float worldWidth =
        std::max(maximumX - minimumX_, 0.001F);
    const float worldHeight =
        std::max(maximumY - minimumY_, 0.001F);
    const float maximumScale = std::max(
        std::hypot(worldWidth, worldHeight), 0.001F);
    maximumY_ = maximumY;
    scale_ = mapSize / maximumScale;
    originX_ = viewportWidth - mapSize * 0.5F -
        worldWidth * scale_ * 0.5F;
    originY_ = mapSize * 0.5F - worldHeight * scale_ * 0.5F;

    auto mapPosition = [&](Vec2 position) {
        return HudPoint{
            originX_ + (position[0] - minimumX_) * scale_,
            originY_ + (maximumY_ - position[1]) * scale_};
    };
    for (const auto& nodes : paths)
    {
        const std::size_t firstVertex = geometry_.vertices.size();
        std::size_t index = 0U;
        for (const auto& node : nodes)
        {
            const auto first = mapPosition(add(
                node.position,
                multiply(node.middleNormal, node.radius)));
            const auto second = mapPosition(subtract(
                node.position,
                multiply(node.middleNormal, node.radius)));
            const float u = static_cast<float>(index % 2U);
            geometry_.vertices.push_back(
                {first.x, first.y, u, 0.0F});
            geometry_.vertices.push_back(
                {second.x, second.y, u, 1.0F});
            ++index;
        }
        for (std::size_t node = 0U; node + 1U < nodes.size(); ++node)
        {
            const auto first = static_cast<std::uint16_t>(
                firstVertex + node * 2U);
            const auto second =
                static_cast<std::uint16_t>(first + 2U);
            const auto firstNext =
                static_cast<std::uint16_t>(first + 1U);
            const auto secondNext =
                static_cast<std::uint16_t>(second + 1U);
            geometry_.indices.insert(
                geometry_.indices.end(),
                {first, firstNext, second,
                 firstNext, secondNext, second});
        }
    }

    const auto* first = tracePoint(race.tracePath.front());
    const auto* next = tracePoint(race.tracePath[1]);
    if (first != nullptr && next != nullptr)
    {
        geometry_.start = MapPosition(first->position);
        const auto nextPosition = MapPosition(next->position);
        geometry_.startAngle = std::atan2(
            nextPosition.y - geometry_.start.y,
            nextPosition.x - geometry_.start.x);
        geometry_.startWidth = first->width * scale_ * 0.5F;
        geometry_.startHeight = first->width * scale_;
    }
    valid_ = geometry_.vertices.size() >= 4U &&
        !geometry_.indices.empty();
    return valid_;
}

void MiniMapFrame::Clear() noexcept
{
    geometry_ = {};
    minimumX_ = 0.0F;
    minimumY_ = 0.0F;
    maximumY_ = 0.0F;
    scale_ = 1.0F;
    originX_ = 0.0F;
    originY_ = 0.0F;
    valid_ = false;
}

HudPoint MiniMapFrame::MapPosition(Vec3 position) const noexcept
{
    return {originX_ + (position.x - minimumX_) * scale_,
            originY_ + (maximumY_ - position.y) * scale_};
}

const HudMiniMapGeometry& MiniMapFrame::GetGeometry() const noexcept
{
    return geometry_;
}

bool MiniMapFrame::IsValid() const noexcept
{
    return valid_;
}

HudItemId PlayerStateFrame::NewPickItem(float imageWidth, float now)
{
    HudPickItem item;
    item.id = nextId_++;
    item.started = now;
    item.position = {
        imageWidth * 0.5F, HudMenu::GetPickItemsPos().y};
    item.targetX = item.position.x + 30.0F;
    pickItems_.insert(pickItems_.begin(), item);
    return item.id;
}

HudItemId PlayerStateFrame::NewAchievment(
    float slotWidth, float slotHeight, float imageHeight,
    float viewportWidth, float viewportHeight, float now,
    std::size_t startPosition)
{
    HudAchievmentItem item;
    item.id = nextId_++;
    item.started = now;
    item.slotHeight = slotHeight;
    item.imageHeight = imageHeight;
    item.targetX =
        HudMenu::GetAchievmentItemsPos(viewportWidth).x;
    item.lastIndex = static_cast<float>(achievmentItems_.size());
    if (startPosition >= randomAchievmentPosition)
    {
        startPosition = static_cast<std::size_t>(
            static_cast<double>(std::rand()) /
            (static_cast<double>(RAND_MAX) + 1.0) *
            static_cast<double>(randomAchievmentPosition));
    }
    const float quarter = viewportHeight * 0.25F;
    switch (startPosition)
    {
    case 0U:
        item.position = {-slotWidth * 2.0F, quarter};
        break;
    case 1U:
        item.position = {-slotWidth, quarter * 2.0F};
        break;
    case 2U:
        item.position = {-slotWidth, quarter * 3.0F};
        break;
    case 3U:
        item.position = {0.0F, viewportHeight + slotHeight};
        break;
    case 4U:
        item.position = {
            viewportWidth + slotHeight * 2.0F, quarter};
        break;
    case 5U:
        item.position = {
            viewportWidth + slotHeight, quarter * 2.0F};
        break;
    case 6U:
        item.position = {
            viewportWidth + slotHeight, quarter * 3.0F};
        break;
    default:
        item.position = {
            viewportWidth, viewportHeight + slotHeight};
        break;
    }
    achievmentItems_.insert(achievmentItems_.begin(), item);
    return item.id;
}

void PlayerStateFrame::OnProgress(float deltaTime, float now)
{
    deltaTime = std::max(deltaTime, 0.0F);
    pickItems_.erase(
        std::remove_if(
            pickItems_.begin(), pickItems_.end(),
            [now](const HudPickItem& item) {
                return now - item.started >= 5.0F;
            }),
        pickItems_.end());
    for (std::size_t index = 0U; index < pickItems_.size(); ++index)
    {
        auto& item = pickItems_[index];
        const float age = std::max(now - item.started, 0.0F);
        const bool fadingOut = age > 4.7F;
        item.alpha =
            age < 0.3F
                ? std::clamp(age / 0.3F, 0.0F, 1.0F)
                : fadingOut
                ? 1.0F - std::clamp(
                      (age - 4.7F) / 0.3F, 0.0F, 1.0F)
                : 1.0F;
        const float targetY = HudMenu::GetPickItemsPos().y +
            static_cast<float>(index) * 85.0F +
            (fadingOut ? 30.0F : 0.0F);
        item.position.x = std::min(
            item.position.x + 90.0F * deltaTime, item.targetX);
        item.position.y = std::min(
            item.position.y + 120.0F * deltaTime, targetY);
    }

    achievmentItems_.erase(
        std::remove_if(
            achievmentItems_.begin(), achievmentItems_.end(),
            [now](const HudAchievmentItem& item) {
                return now - item.started >= 5.0F;
            }),
        achievmentItems_.end());
    const float originY = HudMenu::GetAchievmentItemsPos(0.0F).y;
    for (std::size_t index = 0U;
         index < achievmentItems_.size(); ++index)
    {
        auto& item = achievmentItems_[index];
        const float age = std::max(now - item.started, 0.0F);
        float stackIndex = static_cast<float>(index);
        if (item.indexTime < 0.0F && stackIndex != item.lastIndex)
            item.indexTime = 0.0F;
        if (item.indexTime >= 0.0F)
        {
            item.indexTime += deltaTime;
            const float amount = std::clamp(
                item.indexTime / 0.15F, 0.0F, 1.0F);
            stackIndex = item.lastIndex +
                (stackIndex - item.lastIndex) * amount;
            if (amount >= 1.0F)
            {
                item.lastIndex = static_cast<float>(index);
                item.indexTime = -1.0F;
            }
        }
        const HudPoint target{
            item.targetX,
            originY + item.imageHeight * 0.5F +
                stackIndex * item.slotHeight};
        const float fly = std::clamp(age / 0.3F, 0.0F, 1.0F);
        item.position.x += (target.x - item.position.x) * fly;
        item.position.y += (target.y - item.position.y) * fly;
        const float out = std::clamp(
            (age - 4.7F) / 0.3F, 0.0F, 1.0F);
        item.alpha = 1.0F - out;
        item.pointsAlpha = std::clamp(
            (age - 0.8F) / 0.15F, 0.0F, 1.0F) - out;
        const float ping = std::clamp(
            (age - 0.2F) / 0.1F, 0.0F, 1.0F) -
            std::clamp((age - 0.3F) / 0.1F, 0.0F, 1.0F);
        item.scale = 1.0F + ping;
    }
}

void PlayerStateFrame::ShowCarLife(
    std::size_t slot, std::size_t racer, float timeMax) noexcept
{
    if (slot >= carLifeItems_.size())
        return;
    auto& item = carLifeItems_[slot];
    item.barAlpha = item.barAlpha > 0.99F
        ? 0.0F : std::min(item.barAlpha, 0.5F);
    if (item.racer != racer)
        item.backgroundAlpha = 0.0F;
    item.racer = racer;
    item.timer = 0.0F;
    item.timeMax = timeMax;
    item.visible = true;
}

void PlayerStateFrame::ProgressCarLife(
    std::size_t slot, const HudCarLifeInput& input,
    float deltaTime) noexcept
{
    if (slot >= carLifeItems_.size())
        return;
    auto& item = carLifeItems_[slot];
    if (item.timer < 0.0F ||
        item.racer == HudCarLife::invalidRacer)
        return;
    if (!input.targetAlive)
    {
        item = {};
        item.barAlpha = 1.0F;
        return;
    }

    deltaTime = std::max(deltaTime, 0.0F);
    float targetAlpha = 1.0F;
    item.timer += deltaTime;
    if (item.timer > item.timeMax)
    {
        targetAlpha = item.backgroundAlpha;
        if (targetAlpha > 0.0F)
            targetAlpha = 0.0F;
        else
        {
            item = {};
            item.barAlpha = 1.0F;
            return;
        }
    }
    if (input.atEdge)
        targetAlpha = 0.0F;

    item.life = std::clamp(input.life, 0.0F, 1.0F);
    const float maximumX =
        std::max(input.viewportWidth - input.backWidth, 0.0F);
    const float clampedX = std::clamp(
        input.projected.x, 0.0F, maximumX);
    const float clampedY = std::clamp(
        input.projected.y, input.backHeight,
        std::max(input.viewportHeight, input.backHeight));
    item.position = {
        clampedX + input.backWidth * 0.5F,
        clampedY - input.backHeight * 0.5F};

    auto stepLerp = [](float value, float target, float step) {
        return target > value ? std::min(value + step, target)
                              : std::max(value - step, target);
    };
    const float alphaStep = deltaTime / 0.3F;
    item.backgroundAlpha = stepLerp(
        item.backgroundAlpha, targetAlpha, alphaStep);
    item.barAlpha = stepLerp(
        item.barAlpha, targetAlpha, alphaStep);
    item.visible = true;
}

void PlayerStateFrame::Reset() noexcept
{
    nextId_ = 1U;
    pickItems_.clear();
    achievmentItems_.clear();
    carLifeItems_ = {};
    for (auto& item : carLifeItems_)
        item.barAlpha = 1.0F;
}

const std::vector<HudPickItem>&
PlayerStateFrame::GetPickItems() const noexcept
{
    return pickItems_;
}

const std::vector<HudAchievmentItem>&
PlayerStateFrame::GetAchievmentItems() const noexcept
{
    return achievmentItems_;
}

const HudPickItem* PlayerStateFrame::FindPickItem(
    HudItemId id) const noexcept
{
    const auto found = std::find_if(
        pickItems_.begin(), pickItems_.end(),
        [id](const HudPickItem& item) { return item.id == id; });
    return found == pickItems_.end() ? nullptr : &*found;
}

const HudAchievmentItem* PlayerStateFrame::FindAchievmentItem(
    HudItemId id) const noexcept
{
    const auto found = std::find_if(
        achievmentItems_.begin(), achievmentItems_.end(),
        [id](const HudAchievmentItem& item) {
            return item.id == id;
        });
    return found == achievmentItems_.end() ? nullptr : &*found;
}

const std::array<HudCarLife, 2>&
PlayerStateFrame::GetCarLifeItems() const noexcept
{
    return carLifeItems_;
}

bool PlayerStateFrame::HasCarLife(std::size_t racer) const noexcept
{
    return std::any_of(
        carLifeItems_.begin(), carLifeItems_.end(),
        [racer](const HudCarLife& item) {
            return item.racer == racer;
        });
}

void HudMenu::Reset() noexcept
{
    state_ = HudMenuState::Main;
    countdown_ = {};
    applyState();
}

HudMenuState HudMenu::GetState() const noexcept
{
    return state_;
}

void HudMenu::SetState(HudMenuState value) noexcept
{
    if (state_ == value)
        return;
    state_ = value;
    applyState();
}

bool HudMenu::IsMiniMapVisible() const noexcept
{
    return miniMapVisible_;
}

bool HudMenu::IsPlayerStateVisible() const noexcept
{
    return playerStateVisible_;
}

HudMenuCommand HudMenu::OnHandleInput(
    bool escapeDown, bool repeated, bool paused) const noexcept
{
    if (!escapeDown || repeated)
        return HudMenuCommand::None;
    return paused ? HudMenuCommand::HideExitConfirmation
                  : HudMenuCommand::ShowExitConfirmation;
}

void HudMenu::OnCountdownEvent(int image) noexcept
{
    if (image < 0 || image > 4)
        return;
    countdown_.image = image;
    countdown_.alpha = 1.0F;
    countdown_.growthSeconds = 0.0F;
}

void HudMenu::OnProgress(float deltaTime) noexcept
{
    if (countdown_.image != 4)
        return;
    deltaTime = std::max(deltaTime, 0.0F);
    countdown_.alpha -= deltaTime / 1.5F;
    if (countdown_.alpha > 0.0F)
    {
        countdown_.growthSeconds += deltaTime;
        return;
    }
    countdown_.image = -1;
    countdown_.alpha = 0.0F;
}

const HudCountdownVisual& HudMenu::GetCountdownVisual() const noexcept
{
    return countdown_;
}

HudRect HudMenu::GetMiniMapRect() noexcept
{
    return {{320.0F, 320.0F}};
}

HudPoint HudMenu::GetWeaponPos() noexcept
{
    return {155.0F, 50.0F};
}

HudPoint HudMenu::GetWeaponBoxPos() noexcept
{
    return {5.0F, -15.0F};
}

HudPoint HudMenu::GetWeaponLabelPos() noexcept
{
    return {-10.0F, 26.0F};
}

HudPoint HudMenu::GetWeaponPosMine() noexcept
{
    return {30.0F, 140.0F};
}

HudPoint HudMenu::GetWeaponPosMineLabel() noexcept
{
    return {105.0F, 159.0F};
}

HudPoint HudMenu::GetWeaponPosHyper() noexcept
{
    return {30.0F, 32.0F};
}

HudPoint HudMenu::GetWeaponPosHyperLabel() noexcept
{
    return {105.0F, 15.0F};
}

HudPoint HudMenu::GetPlacePos() noexcept
{
    return {105.0F, 88.0F};
}

HudPoint HudMenu::GetLapPos() noexcept
{
    return {0.0F, 200.0F};
}

HudPoint HudMenu::GetLifeBarPos() noexcept
{
    return {165.0F, 0.0F};
}

HudPoint HudMenu::GetPickItemsPos() noexcept
{
    return {0.0F, 255.0F};
}

HudPoint HudMenu::GetAchievmentItemsPos(float viewportWidth) noexcept
{
    return {(100.0F + viewportWidth) * 0.5F, 15.0F};
}

HudPoint HudMenu::GetCarLifeBarPos() noexcept
{
    return {4.0F, -10.0F};
}

void HudMenu::applyState() noexcept
{
    const bool main = state_ == HudMenuState::Main;
    miniMapVisible_ = main;
    playerStateVisible_ = main;
}

} // namespace r3d::game::originalrace::source
