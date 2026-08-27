#include "OriginalControlManager.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace r3d::game::originalcontrol
{
namespace
{

using Action = rrr3d::input::Action;

constexpr std::array<Action, gameActionCount> portableActions{{
    Action::Accelerate,
    Action::Brake,
    Action::TurnLeft,
    Action::TurnRight,
    Action::UseWeapon,
    Action::SelectWeapon1,
    Action::SelectWeapon2,
    Action::SelectWeapon3,
    Action::SelectWeapon4,
    Action::UseAllWeapons,
    Action::UseHyper,
    Action::UseMine,
    Action::PreviousWeapon,
    Action::NextWeapon,
    Action::ToggleCamera,
    Action::MenuConfirm,
    Action::Pause,
    Action::ResetVehicle,
    Action::Debug1,
    Action::Debug2,
    Action::Debug3,
    Action::Debug4,
    Action::Debug5,
    Action::Debug6,
    Action::Debug7,
}};

} // namespace

ControlManager::ControlManager()
{
    bindings_[ControllerIndex(ControllerType::Keyboard)] =
        makeDefaultBindings(ControllerType::Keyboard);
    bindings_[ControllerIndex(ControllerType::Gamepad)] =
        makeDefaultBindings(ControllerType::Gamepad);
}

void ControlManager::ApplyBindings(
    ControllerType controller, const BindingMap& bindings)
{
    const auto index = ControllerIndex(controller);
    auto& target = bindings_[index];
    target.clear();
    for (const auto actionName : gameActionTable())
        target.emplace(std::string(actionName), "None");
    for (const auto& [actionName, virtualKey] : bindings)
    {
        const auto action = std::find(
            gameActionTable().begin(), gameActionTable().end(), actionName);
        if (action == gameActionTable().end())
            continue;
        target[std::string(*action)] =
            canonicalVirtualKeyName(controller, virtualKey);
    }

    if (controller == ControllerType::Keyboard)
        ClearHeldSource(Source::Keyboard);
    else
    {
        ClearHeldSource(Source::GamepadButton);
        ClearHeldSource(Source::GamepadAxis);
    }
}

const BindingMap& ControlManager::GetBindings(
    ControllerType controller) const noexcept
{
    return bindings_[ControllerIndex(controller)];
}

std::vector<ControlManager::Action> ControlManager::GetGameActions(
    ControllerType controller, std::string_view virtualKey) const
{
    std::vector<Action> actions;
    const std::string canonical =
        canonicalVirtualKeyName(controller, virtualKey);
    if (canonical == "None")
        return actions;

    const auto& bindings = GetBindings(controller);
    const auto& names = gameActionTable();
    for (std::size_t index = 0U; index < names.size(); ++index)
    {
        const auto binding = bindings.find(std::string(names[index]));
        if (binding != bindings.end() && binding->second == canonical)
            actions.push_back(PortableAction(index));
    }
    return actions;
}

VirtualKeyInfo ControlManager::GetVirtualKeyInfo(
    ControllerType controller, std::string_view virtualKey) const
{
    const std::string canonical =
        canonicalVirtualKeyName(controller, virtualKey);
    const auto& keys = virtualKeyTable(controller);
    const auto found = std::find_if(
        keys.begin(), keys.end(), [&](const VirtualKeyInfo& info) {
            return info.name == canonical;
        });
    if (found != keys.end())
        return *found;
    // Character virtual keys are encoded outside the fixed table. Their
    // display name is irrelevant to the source normalization formula.
    return {"Character", 0U, 0U};
}

VirtualKeyInfo ControlManager::GetGameActionInfo(
    ControllerType controller, Action action) const
{
    const auto sourceIndex = SourceActionIndex(action);
    if (!sourceIndex)
        return {"None", 0U, 0U};
    const auto& actionName = gameActionTable()[*sourceIndex];
    const auto& bindings = GetBindings(controller);
    const auto binding = bindings.find(std::string(actionName));
    return GetVirtualKeyInfo(
        controller, binding != bindings.end() ? binding->second : "None");
}

float ControlManager::NormalizeVirtualKey(
    ControllerType controller, std::string_view virtualKey,
    int rawValue, bool withAlpha) const
{
    const VirtualKeyInfo info = GetVirtualKeyInfo(controller, virtualKey);
    if (withAlpha && info.alphaMax == 0U)
        return 0.0F;

    const float range = static_cast<float>(
        info.alphaMax - info.alphaThreshold);
    if (range == 0.0F && rawValue != 0)
        return static_cast<float>(rawValue);
    if (rawValue > static_cast<int>(info.alphaThreshold))
    {
        return static_cast<float>(
                   rawValue - static_cast<int>(info.alphaThreshold)) /
               range;
    }
    if (rawValue < -static_cast<int>(info.alphaThreshold))
    {
        return static_cast<float>(
                   rawValue + static_cast<int>(info.alphaThreshold)) /
               range;
    }
    return 0.0F;
}

std::vector<ControlManager::ActionEvent> ControlManager::OnVirtualKey(
    ControllerType controller, std::string_view virtualKey,
    int rawValue, bool repeated, Source source,
    std::uint32_t deviceId, char32_t unicode)
{
    std::vector<ActionEvent> events;
    const std::string canonical =
        canonicalVirtualKeyName(controller, virtualKey);
    if (canonical == "None")
        return events;

    rawVirtualKeyValues_[{controller, canonical, source, deviceId}] = rawValue;
    const float normalized = NormalizeVirtualKey(
        controller, canonical, rawValue, false);
    const auto actions = GetGameActions(controller, canonical);
    if (actions.empty())
    {
        HandleInput({std::nullopt, canonical, rawValue != 0, repeated,
                     controller, unicode, normalized, source, deviceId});
        return events;
    }

    for (const Action action : actions)
    {
        const ActionEvent event{
            action,
            std::abs(normalized),
            normalized != 0.0F,
            repeated,
            source,
            deviceId,
        };
        UpdateActionState(event);
        events.push_back(event);
        if (HandleInput({action, canonical, event.active, repeated,
                         controller, unicode, normalized, source, deviceId}))
            break;
    }
    return events;
}

int ControlManager::GetVirtualKeyState(
    ControllerType controller, std::string_view virtualKey) const
{
    const std::string canonical =
        canonicalVirtualKeyName(controller, virtualKey);
    int result = 0;
    for (const auto& [key, value] : rawVirtualKeyValues_)
    {
        if (std::get<0>(key) == controller &&
            std::get<1>(key) == canonical &&
            std::abs(value) > std::abs(result))
            result = value;
    }
    return result;
}

float ControlManager::GetGameActionState(
    ControllerType controller, Action action, bool withAlpha) const
{
    const auto sourceIndex = SourceActionIndex(action);
    if (!sourceIndex)
        return 0.0F;
    const auto& bindings = GetBindings(controller);
    const auto binding = bindings.find(
        std::string(gameActionTable()[*sourceIndex]));
    if (binding == bindings.end() || binding->second == "None")
        return 0.0F;
    return NormalizeVirtualKey(
        controller, binding->second,
        GetVirtualKeyState(controller, binding->second), withAlpha);
}

float ControlManager::GetGameActionState(
    Action action, bool withAlpha) const
{
    for (const auto controller :
         {ControllerType::Keyboard, ControllerType::Gamepad})
    {
        const float value = GetGameActionState(
            controller, action, withAlpha);
        if (value != 0.0F)
            return value;
    }
    return 0.0F;
}

void ControlManager::UpdateActionState(const ActionEvent& event)
{
    if (event.source == Source::System)
        return;
    heldActionValues_[{event.action, event.source, event.device_id}] =
        event.active
            ? std::clamp(std::abs(event.value), 0.0F, 1.0F)
            : 0.0F;
}

float ControlManager::HeldValue(Action action) const noexcept
{
    float result = 0.0F;
    for (const auto& [key, value] : heldActionValues_)
    {
        if (std::get<0>(key) == action)
            result = std::max(result, value);
    }
    return result;
}

float ControlManager::HeldValue(Action action, Source source) const noexcept
{
    float result = 0.0F;
    for (const auto& [key, value] : heldActionValues_)
    {
        if (std::get<0>(key) == action && std::get<1>(key) == source)
            result = std::max(result, value);
    }
    return result;
}

void ControlManager::ClearHeldSource(Source source) noexcept
{
    for (auto entry = heldActionValues_.begin();
         entry != heldActionValues_.end();)
    {
        if (std::get<1>(entry->first) == source)
            entry = heldActionValues_.erase(entry);
        else
            ++entry;
    }
    for (auto entry = rawVirtualKeyValues_.begin();
         entry != rawVirtualKeyValues_.end();)
    {
        if (std::get<2>(entry->first) == source)
            entry = rawVirtualKeyValues_.erase(entry);
        else
            ++entry;
    }
}

void ControlManager::ClearHeldDevice(std::uint32_t deviceId) noexcept
{
    for (auto entry = heldActionValues_.begin();
         entry != heldActionValues_.end();)
    {
        if (std::get<2>(entry->first) == deviceId)
            entry = heldActionValues_.erase(entry);
        else
            ++entry;
    }
    for (auto entry = rawVirtualKeyValues_.begin();
         entry != rawVirtualKeyValues_.end();)
    {
        if (std::get<3>(entry->first) == deviceId)
            entry = rawVirtualKeyValues_.erase(entry);
        else
            ++entry;
    }
}

void ControlManager::ResetInput() noexcept
{
    heldActionValues_.clear();
    rawVirtualKeyValues_.clear();
}

void ControlManager::InsertEvent(ControlEvent* event)
{
    if (event == nullptr ||
        std::find(eventList_.begin(), eventList_.end(), event) !=
            eventList_.end())
        return;
    eventList_.push_back(event);
}

void ControlManager::RemoveEvent(ControlEvent* event) noexcept
{
    const auto found = std::find(eventList_.begin(), eventList_.end(), event);
    if (found != eventList_.end())
        eventList_.erase(found);
}

bool ControlManager::HandleInput(const InputMessage& message)
{
    for (auto* event : eventList_)
    {
        if (event->OnHandleInput(message))
            return true;
    }
    return false;
}

void ControlManager::OnProgress(float deltaTime)
{
    for (auto* event : eventList_)
        event->OnInputProgress(deltaTime);
}

void ControlManager::OnFrame(float deltaTime)
{
    for (auto* event : eventList_)
        event->OnInputFrame(deltaTime);
}

std::optional<std::size_t> ControlManager::SourceActionIndex(
    Action action) noexcept
{
    const auto found = std::find(
        portableActions.begin(), portableActions.end(), action);
    if (found == portableActions.end())
        return std::nullopt;
    return static_cast<std::size_t>(
        std::distance(portableActions.begin(), found));
}

ControlManager::Action ControlManager::PortableAction(
    std::size_t sourceIndex) noexcept
{
    return portableActions[std::min(sourceIndex, portableActions.size() - 1U)];
}

std::size_t ControlManager::ControllerIndex(
    ControllerType controller) noexcept
{
    return controller == ControllerType::Gamepad ? 1U : 0U;
}

} // namespace r3d::game::originalcontrol
