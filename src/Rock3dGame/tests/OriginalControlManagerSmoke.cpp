#include "OriginalControlManager.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{

using r3d::game::originalcontrol::ControlEvent;
using r3d::game::originalcontrol::ControlManager;
using r3d::game::originalcontrol::ControllerType;
using r3d::game::originalcontrol::InputMessage;
using rrr3d::input::Action;
using rrr3d::input::Source;

class Probe final : public ControlEvent
{
public:
    Probe(int id, std::vector<int>& order, bool consume = false)
        : id_(id), order_(order), consume_(consume)
    {
    }

    bool OnHandleInput(const InputMessage& message) override
    {
        order_.push_back(id_);
        messages.push_back(message);
        return consume_;
    }

    void OnInputProgress(float) override
    {
        order_.push_back(id_ * 10);
    }

    void OnInputFrame(float) override
    {
        order_.push_back(id_ * 100);
    }

    std::vector<InputMessage> messages;

private:
    int id_ = 0;
    std::vector<int>& order_;
    bool consume_ = false;
};

bool close(float left, float right)
{
    return std::abs(left - right) < 0.0001F;
}

int fail(const std::string& message)
{
    std::cerr << "original ControlManager smoke failed: " << message << '\n';
    return 1;
}

} // namespace

int main()
{
    ControlManager controls;
    controls.ApplyBindings(ControllerType::Keyboard,
                           {{"gaAccel", "A"}, {"gaAction", "A"}});
    const auto mapped = controls.GetGameActions(
        ControllerType::Keyboard, "A");
    if (mapped != std::vector<Action>{Action::Accelerate,
                                      Action::MenuConfirm})
        return fail("shared key actions are not in source enum order");

    std::vector<int> order;
    Probe first(1, order);
    Probe consuming(2, order, true);
    Probe third(3, order);
    controls.InsertEvent(&first);
    controls.InsertEvent(&first);
    controls.InsertEvent(&consuming);
    controls.InsertEvent(&third);
    const auto consumed = controls.OnVirtualKey(
        ControllerType::Keyboard, "A", 1, false, Source::Keyboard);
    if (consumed.size() != 1U ||
        consumed.front().action != Action::Accelerate ||
        order != std::vector<int>{1, 2})
        return fail("ordered dispatch/consumption differs from source");

    controls.RemoveEvent(&consuming);
    order.clear();
    const auto dispatched = controls.OnVirtualKey(
        ControllerType::Keyboard, "A", 1, true, Source::Keyboard);
    if (dispatched.size() != 2U ||
        !dispatched.front().repeated ||
        order != std::vector<int>{1, 3, 1, 3})
        return fail("all mapped actions were not dispatched in order");
    if (!close(controls.GetGameActionState(
                   ControllerType::Keyboard, Action::Accelerate), 1.0F) ||
        controls.GetGameActionState(
            ControllerType::Keyboard, Action::Accelerate, true) != 0.0F)
        return fail("digital polled state/withAlpha behavior differs");

    controls.ApplyBindings(
        ControllerType::Gamepad,
        {{"gaWheelLeft", "L.Thumb Left"},
         {"gaHyper", "Left Trigger"}});
    const int steeringRaw = -24000;
    const float expectedSigned =
        static_cast<float>(steeringRaw + 7849) /
        static_cast<float>(32767 - 7849);
    const auto steering = controls.OnVirtualKey(
        ControllerType::Gamepad, "L.Thumb Left", steeringRaw, false,
        Source::GamepadAxis, 7U);
    if (steering.size() != 1U ||
        steering.front().action != Action::TurnLeft ||
        !close(steering.front().value, std::abs(expectedSigned)) ||
        !close(controls.GetGameActionState(
                   ControllerType::Gamepad, Action::TurnLeft),
               expectedSigned) ||
        !close(controls.HeldValue(Action::TurnLeft, Source::GamepadAxis),
               std::abs(expectedSigned)))
        return fail("source signed axis state/portable magnitude diverged");

    const auto threshold = controls.OnVirtualKey(
        ControllerType::Gamepad, "Left Trigger", 30, false,
        Source::GamepadAxis, 7U);
    if (threshold.size() != 1U || threshold.front().active)
        return fail("trigger threshold must normalize to zero");
    const auto trigger = controls.OnVirtualKey(
        ControllerType::Gamepad, "Left Trigger", 31, false,
        Source::GamepadAxis, 7U);
    if (trigger.size() != 1U || !trigger.front().active ||
        !close(trigger.front().value, 1.0F / 225.0F))
        return fail("trigger normalization differs from XInput source");

    order.clear();
    controls.OnProgress(0.016F);
    controls.OnFrame(0.016F);
    if (order != std::vector<int>{10, 30, 100, 300})
        return fail("progress/frame event list order differs from source");

    controls.ClearHeldDevice(7U);
    if (controls.HeldValue(Action::TurnLeft) != 0.0F ||
        controls.GetGameActionState(
            ControllerType::Gamepad, Action::TurnLeft) != 0.0F)
        return fail("device removal did not clear polling state");
    controls.ResetInput();
    if (controls.HeldValue(Action::Accelerate) != 0.0F)
        return fail("focus reset did not clear keyboard state");

    std::cout << "original ControlManager smoke passed\n";
    return 0;
}
