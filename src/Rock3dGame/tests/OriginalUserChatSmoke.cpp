#include "OriginalUserChat.h"

#include <cmath>
#include <iostream>
#include <string>

int main()
{
    using r3d::game::originalui::OriginalUserChat;

    OriginalUserChat chat;
    chat.show(true);
    chat.showInput(true, "Player: ", {}, {0.2F, 0.4F, 0.8F, 1.0F});
    chat.appendInput("Motor ");
    chat.appendInput("Рок");
    chat.backspaceInput();
    if (!chat.visible() || !chat.inputVisible() ||
        chat.inputText() != "Motor Ро")
        return 1;

    chat.pushLine("<Player", chat.inputText(),
                  chat.inputNameColor());
    for (std::size_t index = 1U;
         index < OriginalUserChat::maxLines + 5U; ++index)
    {
        chat.pushLine("<Peer", std::to_string(index));
    }
    if (chat.lines().size() != OriginalUserChat::maxLines ||
        chat.lines().front().text != "54")
        return 2;

    chat.update(10.5F);
    if (chat.lines().empty() ||
        std::abs(chat.lines().front().alpha() - 0.5F) > 0.0001F)
        return 3;
    chat.update(0.5F);
    if (!chat.lines().empty())
        return 4;

    chat.showInput(false, "Player: ");
    chat.show(false);
    if (chat.visible() || chat.inputVisible())
        return 5;

    std::cout
        << "Original UserChat input/UTF-8 backspace/newest-first 50-line "
           "history/10+1 second fade passed\n";
    return 0;
}
