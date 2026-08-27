#pragma once

#include "InputActions.h"
#include "OriginalControlBindings.h"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace r3d::game::originalcontrol
{

struct InputMessage
{
    std::optional<rrr3d::input::Action> action;
    std::string key;
    bool active = false;
    bool repeat = false;
    ControllerType controller = ControllerType::Keyboard;
    char32_t unicode = U'\0';
    float value = 0.0F;
    rrr3d::input::Source source = rrr3d::input::Source::System;
    std::uint32_t deviceId = 0U;
};

class ControlEvent
{
public:
    virtual ~ControlEvent() = default;

    virtual bool OnHandleInput(const InputMessage& message)
    {
        static_cast<void>(message);
        return false;
    }
    virtual void OnInputProgress(float deltaTime)
    {
        static_cast<void>(deltaTime);
    }
    virtual void OnInputFrame(float deltaTime)
    {
        static_cast<void>(deltaTime);
    }
};

// Backend-neutral owner of ControlManager.cpp's action table, analog
// normalization, polled action state and ordered ControlEvent list. Platform
// adapters provide canonical VirtualKey names plus raw SDL/XInput-range state.
class ControlManager
{
public:
    using Action = rrr3d::input::Action;
    using ActionEvent = rrr3d::input::ActionEvent;
    using Source = rrr3d::input::Source;

    ControlManager();

    void ApplyBindings(ControllerType controller, const BindingMap& bindings);
    const BindingMap& GetBindings(ControllerType controller) const noexcept;

    std::vector<Action> GetGameActions(
        ControllerType controller, std::string_view virtualKey) const;
    VirtualKeyInfo GetVirtualKeyInfo(
        ControllerType controller, std::string_view virtualKey) const;
    VirtualKeyInfo GetGameActionInfo(
        ControllerType controller, Action action) const;

    // Mirrors the source GetGameActionState(..., withAlpha) formula. The raw
    // value must use the source range: digital 0/1, triggers 0..255 and thumb
    // axes -32768..32767.
    float NormalizeVirtualKey(
        ControllerType controller, std::string_view virtualKey,
        int rawValue, bool withAlpha = false) const;

    std::vector<ActionEvent> OnVirtualKey(
        ControllerType controller, std::string_view virtualKey,
        int rawValue, bool repeated, Source source,
        std::uint32_t deviceId = 0U, char32_t unicode = U'\0');

    int GetVirtualKeyState(
        ControllerType controller, std::string_view virtualKey) const;
    float GetGameActionState(
        ControllerType controller, Action action,
        bool withAlpha = false) const;
    float GetGameActionState(Action action, bool withAlpha = false) const;

    // Portable menu/debug actions share the source polling store but do not
    // alter the original 25-entry game-action table.
    void UpdateActionState(const ActionEvent& event);
    float HeldValue(Action action) const noexcept;
    float HeldValue(Action action, Source source) const noexcept;
    void ClearHeldSource(Source source) noexcept;
    void ClearHeldDevice(std::uint32_t deviceId) noexcept;
    void ResetInput() noexcept;

    void InsertEvent(ControlEvent* event);
    void RemoveEvent(ControlEvent* event) noexcept;
    bool HandleInput(const InputMessage& message);
    void OnProgress(float deltaTime);
    void OnFrame(float deltaTime);

private:
    using HeldKey = std::tuple<Action, Source, std::uint32_t>;
    using RawKey =
        std::tuple<ControllerType, std::string, Source, std::uint32_t>;

    static std::optional<std::size_t> SourceActionIndex(Action action) noexcept;
    static Action PortableAction(std::size_t sourceIndex) noexcept;
    static std::size_t ControllerIndex(ControllerType controller) noexcept;

    std::array<BindingMap, 2U> bindings_;
    std::map<HeldKey, float> heldActionValues_;
    std::map<RawKey, int> rawVirtualKeyValues_;
    std::vector<ControlEvent*> eventList_;
};

} // namespace r3d::game::originalcontrol
