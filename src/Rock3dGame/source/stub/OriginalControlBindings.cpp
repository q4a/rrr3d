#include "OriginalControlBindings.h"

#include <algorithm>

namespace r3d::game::originalcontrol
{
namespace
{

constexpr std::uint32_t gamepadTriggerMax = 255U;
constexpr std::uint32_t gamepadTriggerThreshold = 30U;
constexpr std::uint32_t gamepadThumbMax = 32767U;
constexpr std::uint32_t gamepadLeftThumbThreshold = 7849U;
constexpr std::uint32_t gamepadRightThumbThreshold = 8689U;

constexpr VirtualKeyTable keyboardKeys{{
    {"Left Arrow", 0, 0},
    {"Right Arrow", 0, 0},
    {"Up Arrow", 0, 0},
    {"Down Arrow", 0, 0},
    {"A", 0, 0},
    {"B", 0, 0},
    {"X", 0, 0},
    {"Space", 0, 0},
    {"F6", 0, 0},
    {"F7", 0, 0},
    {"F4", 0, 0},
    {"F5", 0, 0},
    {"F1", 0, 0},
    {"F2", 0, 0},
    {"F3", 0, 0},
    {"L.Thumb Move Y", 0, 0},
    {"R.Thumb Move X", 0, 0},
    {"R.Thumb Move Y", 0, 0},
    {"L.Thumb Left", 0, 0},
    {"L.Thumb Right", 0, 0},
    {"L.Thumb Up", 0, 0},
    {"L.Thumb Down", 0, 0},
    {"R.Thumb Left", 0, 0},
    {"R.Thumb Right", 0, 0},
    {"R.Thumb Up", 0, 0},
    {"R.Thumb Down", 0, 0},
    {"Escape", 0, 0},
    {"Enter", 0, 0},
    {"None", 0, 0},
}};

constexpr VirtualKeyTable gamepadKeys{{
    {"DPad Left", 0, 0},
    {"DPad Right", 0, 0},
    {"DPad Up", 0, 0},
    {"DPad Down", 0, 0},
    {"A", 0, 0},
    {"B", 0, 0},
    {"X", 0, 0},
    {"Y", 0, 0},
    {"Left Trigger", gamepadTriggerMax, gamepadTriggerThreshold},
    {"Right Trigger", gamepadTriggerMax, gamepadTriggerThreshold},
    {"Left Shoulder", 0, 0},
    {"Right Shoulder", 0, 0},
    {"L.Thumb Press", 0, 0},
    {"R.Thumb Press", 0, 0},
    {"L.Thumb Move X", gamepadThumbMax, gamepadLeftThumbThreshold},
    {"L.Thumb Move Y", gamepadThumbMax, gamepadLeftThumbThreshold},
    {"R.Thumb Move X", gamepadThumbMax, gamepadRightThumbThreshold},
    {"R.Thumb Move Y", gamepadThumbMax, gamepadRightThumbThreshold},
    {"L.Thumb Left", gamepadThumbMax, gamepadLeftThumbThreshold},
    {"L.Thumb Right", gamepadThumbMax, gamepadLeftThumbThreshold},
    {"L.Thumb Up", gamepadThumbMax, gamepadLeftThumbThreshold},
    {"L.Thumb Down", gamepadThumbMax, gamepadLeftThumbThreshold},
    {"R.Thumb Left", gamepadThumbMax, gamepadLeftThumbThreshold},
    {"R.Thumb Right", gamepadThumbMax, gamepadLeftThumbThreshold},
    {"R.Thumb Up", gamepadThumbMax, gamepadLeftThumbThreshold},
    {"R.Thumb Down", gamepadThumbMax, gamepadLeftThumbThreshold},
    {"Back", 0, 0},
    {"Start", 0, 0},
    {"None", 0, 0},
}};

constexpr GameActionTable gameActions{{
    "gaAccel", "gaBreak", "gaWheelLeft", "gaWheelRight", "gaShot",
    "gaShot1", "gaShot2", "gaShot3", "gaShot4", "gaShotAll",
    "gaHyper", "gaMine", "gaWeaponDown", "gaWeaponUp", "gaViewSwitch",
    "gaAction", "gaEscape", "gaResetCar", "gaDebug1", "gaDebug2",
    "gaDebug3", "gaDebug4", "gaDebug5", "gaDebug6", "gaDebug7",
}};

constexpr std::array<std::size_t, gameActionCount> keyboardDefaults{{
    2U, 3U, 0U, 1U, 128U + 'W', 128U + '1', 128U + '2',
    128U + '3', 128U + '4', 7U, 128U + 'Q', 128U + 'E', 28U, 28U,
    128U + 'C', 27U, 26U, 128U + 'R', 12U, 13U, 14U, 10U, 11U, 8U,
    9U,
}};

constexpr std::array<std::size_t, gameActionCount> gamepadDefaults{{
    4U, 5U, 0U, 1U, 6U, 28U, 28U, 28U, 28U, 7U, 8U, 9U, 10U,
    11U, 13U, 4U, 27U, 26U, 28U, 28U, 28U, 28U, 28U, 28U, 28U,
}};

std::string keyName(const VirtualKeyTable& table, std::size_t key)
{
    if (key < virtualKeyCount)
        return std::string(table[key].name);
    if (key > 128U && key <= 128U + 255U)
        return std::string(1, static_cast<char>(key - 128U));
    return std::string(table.back().name);
}

} // namespace

const VirtualKeyTable& virtualKeyTable(ControllerType controller) noexcept
{
    return controller == ControllerType::Gamepad ? gamepadKeys
                                                  : keyboardKeys;
}

const GameActionTable& gameActionTable() noexcept
{
    return gameActions;
}

std::string canonicalVirtualKeyName(
    ControllerType controller, std::string_view serializedName)
{
    const auto& table = virtualKeyTable(controller);
    if (serializedName == table.back().name)
        return std::string(table.back().name);
    const auto found = std::find_if(
        table.begin(), table.end() - 1,
        [&](const VirtualKeyInfo& info) {
            return info.name == serializedName;
        });
    if (found != table.end() - 1)
        return std::string(found->name);
    if (!serializedName.empty())
        return std::string(1, serializedName.front());
    return std::string(table.back().name);
}

BindingMap makeDefaultBindings(ControllerType controller)
{
    const auto& table = virtualKeyTable(controller);
    const auto& defaults = controller == ControllerType::Gamepad
                               ? gamepadDefaults
                               : keyboardDefaults;
    BindingMap bindings;
    for (std::size_t index = 0; index < gameActions.size(); ++index)
        bindings.emplace(gameActions[index], keyName(table, defaults[index]));
    return bindings;
}

} // namespace r3d::game::originalcontrol
