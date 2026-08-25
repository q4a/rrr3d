#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace r3d::game::originalcontrol
{

enum class ControllerType : std::uint8_t
{
    Keyboard,
    Gamepad,
};

struct VirtualKeyInfo
{
    std::string_view name;
    std::uint32_t alphaMax = 0;
    std::uint32_t alphaThreshold = 0;
};

inline constexpr std::size_t virtualKeyCount = 29U;
inline constexpr std::size_t gameActionCount = 25U;

using VirtualKeyTable = std::array<VirtualKeyInfo, virtualKeyCount>;
using GameActionTable = std::array<std::string_view, gameActionCount>;
using BindingMap = std::map<std::string, std::string>;

// Exact portable ownership of ControlManager.cpp's cVirtualKeyInfo and
// cGameActionStr tables.  The final virtual-key entry is cVirtualKeyEnd and
// is serialized as the literal "None" by the Windows game.
const VirtualKeyTable& virtualKeyTable(ControllerType controller) noexcept;
const GameActionTable& gameActionTable() noexcept;

// Mirrors ControlManager::GetVirtualKeyFromName followed by
// GetVirtualKeyInfo. Unknown non-empty names become their first character;
// an empty name becomes cVirtualKeyEnd.
std::string canonicalVirtualKeyName(
    ControllerType controller, std::string_view serializedName);

// Constructor defaults installed by ControlManager before user.xml is read.
BindingMap makeDefaultBindings(ControllerType controller);

} // namespace r3d::game::originalcontrol
