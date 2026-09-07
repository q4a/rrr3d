#include "OriginalView.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{

using r3d::game::originalview::MouseKey;
using r3d::game::originalview::Point;
using r3d::game::originalview::Size;
using r3d::game::originalview::ViewState;

bool close(float left, float right)
{
    return std::abs(left - right) < 0.0001F;
}

bool close(Point left, Point right)
{
    return close(left.x, right.x) && close(left.y, right.y);
}

int fail(const std::string& message)
{
    std::cerr << "original View smoke failed: " << message << '\n';
    return 1;
}

} // namespace

int main()
{
    for (const auto drawable : {Size{1280, 800}, Size{1920, 1080},
                                Size{3456, 2234}, Size{3840, 2160}})
    {
        const auto canvas = r3d::game::originalview::MenuViewport(drawable);
        const float scale = drawable.width / canvas.width;
        if (!close(scale, drawable.height / canvas.height) ||
            (drawable.width >= 1920 && !close(canvas.width, 1920)))
            return fail("Retina menu scale is not uniform/Full HD based");
        ViewState pointer;
        pointer.Reset({drawable.width / 2, drawable.height / 2}, canvas);
        const Point button{canvas.width / 2 - 532, canvas.height / 2 - 125};
        const auto mapped = pointer.ScreenToView({button.x * scale / 2,
                                                  button.y * scale / 2});
        if (std::abs(mapped.x - button.x) > 0.51F ||
            std::abs(mapped.y - button.y) > 0.51F)
            return fail("Retina pointer does not hit the rendered tab");
    }
    ViewState view;
    if (view.IsValid() || !close(view.ScreenToView({10.0F, 20.0F}), {}))
        return fail("invalid dimensions are not guarded");

    view.Reset({1280.0F, 800.0F}, {2560.0F, 1600.0F});
    if (!view.IsValid() || !close(view.GetAspect(), 1.6F))
        return fail("window/viewport description was not retained");
    if (!close(view.ScreenToView({100.25F, 50.25F}), {201.0F, 101.0F}))
        return fail("source rounded ScreenToView formula differs");
    if (!close(view.ViewToProj({0.0F, 0.0F}), {-1.0F, 1.0F}) ||
        !close(view.ViewToProj({1280.0F, 800.0F}), {0.0F, 0.0F}) ||
        !close(view.ProjToView({1.0F, -1.0F}), {2560.0F, 1600.0F}))
        return fail("source view/projection conversion differs");

    const auto& click = view.OnMouseClick(
        MouseKey::Right, true, {100.0F, 75.0F}, true, false);
    if (click.key != MouseKey::Right || !click.down || !click.shift ||
        click.control || !close(click.coord, {200.0F, 150.0F}))
        return fail("mouse-click snapshot differs from source");
    const auto& firstMove = view.OnMouseMove(
        {110.0F, 80.0F}, false, true);
    if (!close(firstMove.coord, {220.0F, 160.0F}) ||
        !close(firstMove.delta, {220.0F, 160.0F}) ||
        !close(firstMove.offset, {20.0F, 10.0F}) ||
        firstMove.shift || !firstMove.control ||
        !close(firstMove.click.coord, click.coord))
        return fail("first mouse-move state differs from source");
    const auto& secondMove = view.OnMouseMove(
        {112.0F, 82.0F}, false, false);
    if (!close(secondMove.delta, {4.0F, 4.0F}) ||
        !close(view.GetMousePos(), secondMove.coord))
        return fail("mouse delta/current position differs from source");

    view.Reset({640.0F, 400.0F}, {2560.0F, 1600.0F});
    if (!close(view.ScreenToView({100.0F, 75.0F}), {400.0F, 300.0F}) ||
        !close(view.GetMousePos(), secondMove.coord))
        return fail("Reset does not preserve source pointer state");

    std::cout << "original View smoke passed\n";
    return 0;
}
