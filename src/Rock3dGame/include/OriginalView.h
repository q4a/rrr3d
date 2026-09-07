#pragma once

namespace r3d::game::originalview
{

struct Point
{
    float x = 0.0F;
    float y = 0.0F;
};

struct Size
{
    float width = 0.0F;
    float height = 0.0F;
};

// macOS presentation policy: preserve the Windows Full HD widget footprint
// on higher-density drawables, without changing the 3D render resolution.
// Below Full HD keep the original fixed-pixel layout. Uniform scaling also
// keeps circles round on a 16:10 MacBook panel.
Size MenuViewport(Size drawable) noexcept;

enum class MouseKey
{
    Left,
    Middle,
    Right,
    Other,
};

struct MouseClick
{
    MouseKey key = MouseKey::Left;
    bool down = false;
    bool shift = false;
    bool control = false;
    Point coord;
    Point projection;
};

struct MouseMove
{
    bool shift = false;
    bool control = false;
    Point coord;
    Point projection;
    Point delta;
    Point offset;
    MouseClick click;
};

// Backend-neutral owner of the coordinate and pointer-state policy from
// game/View.cpp. The SDL adapter supplies logical client and GUI canvas sizes;
// MenuViewport adapts Retina drawables while preserving source coordinates.
class ViewState
{
public:
    void Reset(Size windowSize, Size viewportSize) noexcept;

    bool IsValid() const noexcept;
    Size GetWindowSize() const noexcept;
    Size GetViewportSize() const noexcept;
    float GetAspect() const noexcept;

    Point ScreenToView(Point point) const noexcept;
    Point ViewToProj(Point point) const noexcept;
    Point ProjToView(Point point) const noexcept;

    const MouseClick& OnMouseClick(
        MouseKey key, bool down, Point screenCoord, bool shift,
        bool control) noexcept;
    const MouseMove& OnMouseMove(
        Point screenCoord, bool shift, bool control) noexcept;

    const MouseClick& GetMouseClick() const noexcept;
    const MouseMove& GetMouseMove() const noexcept;
    Point GetMousePos() const noexcept;

private:
    Size windowSize_;
    Size viewportSize_;
    MouseClick mouseClick_;
    MouseMove mouseMove_;
};

} // namespace r3d::game::originalview
