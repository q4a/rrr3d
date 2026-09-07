#include "OriginalView.h"

#include <cmath>
#include <algorithm>

namespace r3d::game::originalview
{
namespace
{

Point subtract(Point left, Point right) noexcept
{
    return {left.x - right.x, left.y - right.y};
}

} // namespace

Size MenuViewport(Size drawable) noexcept
{
    const float scale = std::max(1.0F, std::min(
        drawable.width / 1920.0F, drawable.height / 1080.0F));
    return {drawable.width / scale, drawable.height / scale};
}

void ViewState::Reset(Size windowSize, Size viewportSize) noexcept
{
    windowSize_ = windowSize;
    viewportSize_ = viewportSize;
}

bool ViewState::IsValid() const noexcept
{
    return std::isfinite(windowSize_.width) &&
           std::isfinite(windowSize_.height) &&
           std::isfinite(viewportSize_.width) &&
           std::isfinite(viewportSize_.height) &&
           windowSize_.width > 0.0F && windowSize_.height > 0.0F &&
           viewportSize_.width > 0.0F && viewportSize_.height > 0.0F;
}

Size ViewState::GetWindowSize() const noexcept
{
    return windowSize_;
}

Size ViewState::GetViewportSize() const noexcept
{
    return viewportSize_;
}

float ViewState::GetAspect() const noexcept
{
    return IsValid() ? viewportSize_.width / viewportSize_.height : 0.0F;
}

Point ViewState::ScreenToView(Point point) const noexcept
{
    if (!IsValid())
        return {};
    return {
        std::round(point.x * viewportSize_.width / windowSize_.width),
        std::round(point.y * viewportSize_.height / windowSize_.height),
    };
}

Point ViewState::ViewToProj(Point point) const noexcept
{
    if (!IsValid())
        return {};
    Point projection{
        point.x / viewportSize_.width * 2.0F - 1.0F,
        point.y / viewportSize_.height * 2.0F - 1.0F,
    };
    projection.y = -projection.y;
    return projection;
}

Point ViewState::ProjToView(Point point) const noexcept
{
    if (!IsValid())
        return {};
    point.y = -point.y;
    point.x = (point.x * 0.5F + 0.5F) * viewportSize_.width;
    point.y = (point.y * 0.5F + 0.5F) * viewportSize_.height;
    return point;
}

const MouseClick& ViewState::OnMouseClick(
    MouseKey key, bool down, Point screenCoord, bool shift,
    bool control) noexcept
{
    mouseClick_.key = key;
    mouseClick_.down = down;
    mouseClick_.shift = shift;
    mouseClick_.control = control;
    mouseClick_.coord = ScreenToView(screenCoord);
    mouseClick_.projection = ViewToProj(mouseClick_.coord);
    return mouseClick_;
}

const MouseMove& ViewState::OnMouseMove(
    Point screenCoord, bool shift, bool control) noexcept
{
    const Point newCoord = ScreenToView(screenCoord);
    mouseMove_.shift = shift;
    mouseMove_.control = control;
    mouseMove_.delta = subtract(newCoord, mouseMove_.coord);
    mouseMove_.offset = subtract(newCoord, mouseClick_.coord);
    mouseMove_.coord = newCoord;
    mouseMove_.projection = ViewToProj(newCoord);
    mouseMove_.click = mouseClick_;
    return mouseMove_;
}

const MouseClick& ViewState::GetMouseClick() const noexcept
{
    return mouseClick_;
}

const MouseMove& ViewState::GetMouseMove() const noexcept
{
    return mouseMove_;
}

Point ViewState::GetMousePos() const noexcept
{
    return mouseMove_.coord;
}

} // namespace r3d::game::originalview
