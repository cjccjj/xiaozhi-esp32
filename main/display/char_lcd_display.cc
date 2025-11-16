#include "char_lcd_display.h"
#include <algorithm>

void CharLcdDisplay::SetChatMessage(const char* /*role*/, const char* content) {
    if (content == nullptr) {
        disp_.clear();
        return;
    }

    // Replace newline with space and take first 40 chars
    std::string text(content);
    std::replace(text.begin(), text.end(), '\n', ' ');
    if (text.size() > 40) {
        text.resize(40);
    }

    // Clear and write from beginning
    disp_.clear();
    disp_.write_cstr(text.c_str(), 0);
}