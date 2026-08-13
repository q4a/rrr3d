#include "OriginalUserChat.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace r3d::game::originalui
{

float UserChatLine::alpha() const noexcept
{
    return 1.0F - std::clamp(
        (time - OriginalUserChat::lineLifeSeconds) /
            OriginalUserChat::lineFadeSeconds,
        0.0F, 1.0F);
}

void OriginalUserChat::show(bool value) noexcept
{
    if (visible_ == value && (value || !inputVisible_))
        return;
    visible_ = value;
    if (!visible_)
    {
        inputVisible_ = false;
        inputText_.clear();
    }
    ++revision_;
}

bool OriginalUserChat::visible() const noexcept
{
    return visible_;
}

void OriginalUserChat::showInput(
    bool value, std::string name, std::string text,
    std::array<float, 4> nameColor)
{
    inputVisible_ = value && visible_;
    inputName_ = std::move(name);
    inputText_ = std::move(text);
    inputNameColor_ = nameColor;
    ++revision_;
}

bool OriginalUserChat::inputVisible() const noexcept
{
    return visible_ && inputVisible_;
}

const std::string& OriginalUserChat::inputName() const noexcept
{
    return inputName_;
}

const std::string& OriginalUserChat::inputText() const noexcept
{
    return inputText_;
}

const std::array<float, 4>&
OriginalUserChat::inputNameColor() const noexcept
{
    return inputNameColor_;
}

void OriginalUserChat::clearInput()
{
    if (inputText_.empty())
        return;
    inputText_.clear();
    ++revision_;
}

void OriginalUserChat::appendInput(std::string_view utf8)
{
    if (!inputVisible() || utf8.empty())
        return;
    inputText_.append(utf8);
    ++revision_;
}

void OriginalUserChat::backspaceInput()
{
    if (!inputVisible() || inputText_.empty())
        return;

    // DialogMenu2 removes one wchar_t. SDL supplies UTF-8, so walk back over
    // continuation bytes to preserve the same one-codepoint operation.
    std::size_t offset = inputText_.size() - 1U;
    while (offset > 0U &&
           (static_cast<unsigned char>(inputText_[offset]) & 0xc0U) == 0x80U)
    {
        --offset;
    }
    inputText_.erase(offset);
    ++revision_;
}

void OriginalUserChat::pushLine(
    std::string name, std::string text,
    std::array<float, 4> nameColor)
{
    lines_.insert(
        lines_.begin(),
        {std::move(name), std::move(text), nameColor, 0.0F});
    // cMaxLines is present in the Windows class but its trim was omitted in
    // the recovered implementation. Apply the declared source invariant so
    // a long network session cannot retain unbounded GUI labels.
    if (lines_.size() > maxLines)
        lines_.resize(maxLines);
    ++revision_;
}

void OriginalUserChat::clearLines()
{
    if (lines_.empty())
        return;
    lines_.clear();
    ++revision_;
}

void OriginalUserChat::update(float deltaSeconds)
{
    if (!visible_ || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0F)
        return;
    for (auto& line : lines_)
        line.time += deltaSeconds;
    const auto firstExpired = std::remove_if(
        lines_.begin(), lines_.end(), [](const UserChatLine& line) {
            return line.time >= lineLifeSeconds + lineFadeSeconds;
        });
    if (firstExpired != lines_.end())
    {
        lines_.erase(firstExpired, lines_.end());
        ++revision_;
    }
}

const std::vector<UserChatLine>& OriginalUserChat::lines() const noexcept
{
    return lines_;
}

std::uint64_t OriginalUserChat::revision() const noexcept
{
    return revision_;
}

} // namespace r3d::game::originalui
