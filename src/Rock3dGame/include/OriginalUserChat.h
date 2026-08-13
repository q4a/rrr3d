#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace r3d::game::originalui
{

struct UserChatLine
{
    std::string name;
    std::string text;
    std::array<float, 4> nameColor{1.0F, 1.0F, 1.0F, 1.0F};
    float time = 0.0F;

    float alpha() const noexcept;
};

// Renderer-independent transfer of game::n::UserChat from DialogMenu2.cpp.
// The platform front end owns the labels, while this class preserves the
// source input, newest-first history and 10 s hold + 1 s fade lifecycle.
class OriginalUserChat
{
public:
    static constexpr std::size_t maxLines = 50U;
    static constexpr float lineLifeSeconds = 10.0F;
    static constexpr float lineFadeSeconds = 1.0F;

    void show(bool value) noexcept;
    bool visible() const noexcept;

    void showInput(bool value, std::string name = {},
                   std::string text = {},
                   std::array<float, 4> nameColor =
                       {1.0F, 1.0F, 1.0F, 1.0F});
    bool inputVisible() const noexcept;
    const std::string& inputName() const noexcept;
    const std::string& inputText() const noexcept;
    const std::array<float, 4>& inputNameColor() const noexcept;
    void clearInput();
    void appendInput(std::string_view utf8);
    void backspaceInput();

    void pushLine(std::string name, std::string text,
                  std::array<float, 4> nameColor =
                      {1.0F, 1.0F, 1.0F, 1.0F});
    void clearLines();
    void update(float deltaSeconds);

    const std::vector<UserChatLine>& lines() const noexcept;
    std::uint64_t revision() const noexcept;

private:
    bool visible_ = false;
    bool inputVisible_ = false;
    std::string inputName_;
    std::string inputText_;
    std::array<float, 4> inputNameColor_{1.0F, 1.0F, 1.0F, 1.0F};
    std::vector<UserChatLine> lines_;
    std::uint64_t revision_ = 0U;
};

} // namespace r3d::game::originalui
