#ifndef CHAR_LCD_DISPLAY_H
#define CHAR_LCD_DISPLAY_H

#include "display.h"
#include <pcf8574.h>
#include <hd44780.h>
#include <string>
#include <driver/gpio.h>
#include <driver/i2c_master.h>  // new unified ESP-IDF I2C driver

class CharLcdDisplay : public Display {
public:
    CharLcdDisplay(i2c_port_num_t i2c_port,
                   gpio_num_t sda_gpio,
                   gpio_num_t scl_gpio,
                   uint8_t i2c_addr = 0x27,
                   int cols = 20,
                   int rows = 4);

    virtual ~CharLcdDisplay();

    // Display interface overrides (most are stubs right now)
    virtual void SetStatus(const char* status) override {}
    virtual void ShowNotification(const char* notification, int duration_ms = 3000) override {}
    virtual void ShowNotification(const std::string& notification, int duration_ms = 3000) override {}
    virtual void SetEmotion(const char* emotion) override {}
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void UpdateStatusBar(bool update_all = false) override {}
    virtual void SetPowerSaveMode(bool on) override {}

protected:
    virtual bool Lock(int timeout_ms = 0) override { return true; }
    virtual void Unlock() override {}

private:
    i2c_port_num_t i2c_port_;
    gpio_num_t sda_gpio_;
    gpio_num_t scl_gpio_;
    uint8_t i2c_addr_;
    int cols_;
    int rows_;

    // New I2C handles
    i2c_master_bus_handle_t bus_handle_{nullptr};
    i2c_master_dev_handle_t dev_handle_{nullptr};

    // old-style device structs still needed for HD44780/PCF8574 logic
    hd44780_t lcd_;
};

#endif // CHAR_LCD_DISPLAY_H