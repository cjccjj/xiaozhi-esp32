#ifndef CHAR_LCD_DISPLAY_H
#define CHAR_LCD_DISPLAY_H

#include "display.h"

#include <driver/i2c_master.h>

// HD44780 header-only library (place inside main/include/HD44780/) https://github.com/dawinaj/HD44780
#include "HD44780/PCF8574.h"
#include "HD44780.h"

class CharLcdDisplay : public Display {
public:
    CharLcdDisplay(i2c_master_bus_handle_t bus, uint8_t i2c_addr = 0x27, int cols = 20, int rows = 4)
        : bus_(bus),
          comm_(bus_, i2c_addr, 400000),
          disp_(&comm_, {static_cast<uint8_t>(rows), static_cast<uint8_t>(cols)}, LCD_2LINE) {
        width_ = cols;
        height_ = rows;

        comm_.init();
        disp_.init();
        disp_.backlight(true);
        disp_.clear();
    }

    virtual ~CharLcdDisplay() = default;

    virtual void SetStatus(const char* /*status*/) override {
        // no-op for char display
    }

    virtual void ShowNotification(const char* /*notification*/, int /*duration_ms*/ = 3000) override {
        // no-op for char display
    }

    virtual void ShowNotification(const std::string &/*notification*/, int /*duration_ms*/ = 3000) override {
        // no-op for char display
    }

    virtual void SetEmotion(const char* /*emotion*/) override {
        // no-op for char display
    }

    virtual void SetChatMessage(const char* /*role*/, const char* content) override;

    virtual void UpdateStatusBar(bool /*update_all*/ = false) override {
        // no-op for char display
    }

    virtual void SetPowerSaveMode(bool /*on*/) override {
        // no-op for char display
    }

protected:
    virtual bool Lock(int /*timeout_ms*/ = 0) override {
        return true; // no LVGL lock needed
    }
    virtual void Unlock() override {
        // no-op
    }

private:
    i2c_master_bus_handle_t bus_;
    HD44780_PCF8574 comm_;
    HD44780 disp_;
};

#endif // CHAR_LCD_DISPLAY_H