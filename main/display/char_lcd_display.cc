#include "char_lcd_display.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include <esp_log.h>
#include "device_state_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// -----------------------------------------------------------------------------
// Global handle used by LCD I2C write callback
// -----------------------------------------------------------------------------
static i2c_master_dev_handle_t g_lcd_dev = nullptr;
static volatile uint32_t g_message_token = 0;

// -----------------------------------------------------------------------------
// I2C write helper used by the HD44780 driver
// -----------------------------------------------------------------------------
static esp_err_t lcd_write_i2c(const hd44780_t * /*lcd*/, uint8_t data)
{
    // send one byte to the PCF8574 through the new I2C driver
    if (!g_lcd_dev) {
        return ESP_FAIL;
    }
    return i2c_master_transmit(g_lcd_dev, &data, 1, 100);
}

// -----------------------------------------------------------------------------
static const char *TAG = "CharLcdDisplay";

static std::string lcd_filter_text(const char* content) {
    if (content == nullptr) return std::string();
    const unsigned char* s = reinterpret_cast<const unsigned char*>(content);
    size_t n = std::strlen(content);
    std::string out;
    out.reserve(n);
    size_t i = 0;
    while (i < n) {
        unsigned char b0 = s[i];
        if (b0 < 0x80) {
            char ch;
            if (b0 == '\n' || b0 == '\r' || b0 == '\t') {
                ch = ' ';
            } else if (b0 < 32 || b0> 126) {
                ch = ' ';
            } else {
                switch (b0) {
                    case '{': // A00 uses Katakana/Symbol here
                        ch = '(';
                        break;
                    case '}': // A00 uses Katakana/Symbol here
                        ch = ')';
                        break;
                    case '|': // A00 uses a custom arrow symbol (0x7C)
                        ch = ' ';
                        break;
                    case '~': // A00 uses a custom block symbol (0x7E)
                        ch = '^'; 
                        break;
                    default:
                        ch = static_cast<char>(b0);
                        break;
                }
            }
            out.push_back(ch);
            i++;
        } else if (b0 >= 0xE0 && b0 <= 0xEF && i + 2 < n) {
            unsigned char b1 = s[i + 1];
            unsigned char b2 = s[i + 2];
            if ((b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80) {
                uint32_t cp = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
                char ch = ' ';
                if (cp == 0x2018 || cp == 0x2019) ch = '\''; // Smart Single Quotes
                else if (cp == 0x201C || cp == 0x201D) ch = '"'; // Smart Double Quotes
                else if (cp == 0xFF08) ch = '('; // Fullwidth Parenthesis (
                else if (cp == 0xFF09) ch = ')'; // Fullwidth Parenthesis )
                else if (cp == 0x20AC) ch = 'E'; // Euro (€)
                else if (cp == 0x2013 || cp == 0x2014) ch = '-'; // En/Em Dash (–, —)
                else if (cp == 0x2026) ch = '.'; // Ellipsis (...)
                else if (cp == 0xA3) ch = 'L'; // Pound (£)
                else if (cp == 0xA5) ch = 'Y'; // Yen (¥)
                else if (cp == 0xAE || cp == 0x2122) ch = 'R'; // Registered/Trademark (®, ™)
                
                // Add Degree Symbol conversion
                else if (cp == 0xB0) ch = '*'; // Degree symbol (°)
                
                out.push_back(ch);
                i += 3;
            } else {
                out.push_back(' ');
                i++;
            }
        } else if (b0 >= 0xC2 && b0 <= 0xDF && i + 1 < n) {
            unsigned char b1 = s[i + 1];
            if ((b1 & 0xC0) == 0x80) {
                // Two-byte UTF-8 sequences are usually unsupported diacritics
                out.push_back(' '); 
                i += 2;
            } else {
                out.push_back(' ');
                i++;
            }
        } else if (b0 >= 0xF0 && b0 <= 0xF4 && i + 3 < n) {
            // Four-byte UTF-8 sequences are typically emojis or rare characters
            out.push_back(' ');
            i += 4;
        } else {
            out.push_back(' ');
            i++;
        }
    }
    return out;
}

// -----------------------------------------------------------------------------
CharLcdDisplay::CharLcdDisplay(i2c_port_num_t i2c_port,
                               gpio_num_t sda_gpio,
                               gpio_num_t scl_gpio,
                               uint8_t i2c_addr,
                               int cols,
                               int rows)
    : i2c_port_(i2c_port),
      sda_gpio_(sda_gpio),
      scl_gpio_(scl_gpio),
      i2c_addr_(i2c_addr),
      cols_(cols),
      rows_(rows)
{
    ESP_LOGI(TAG, "Initializing I2C bus and LCD (addr=0x%02X)", i2c_addr_);

    // ---- Configure and create I2C bus ----
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = i2c_port_,
        .sda_io_num = sda_gpio_,
        .scl_io_num = scl_gpio_,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = { .enable_internal_pullup = 0, .allow_pd = 0 },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus_handle_));

    // ---- Add PCF8574 device ----
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = i2c_addr_,
        .scl_speed_hz = 100000,
        .scl_wait_us = 0,
        .flags = { .disable_ack_check = 0 }
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle_, &dev_cfg, &dev_handle_));

    

    // Make the handle available for the write callback
    g_lcd_dev = dev_handle_;

    // ---- Configure hd44780 descriptor ----
    memset(&lcd_, 0, sizeof(lcd_));
    lcd_.pins.rs = 0;   // PCF8574 bit mapping
    lcd_.pins.e  = 2;
    lcd_.pins.d4 = 4;
    lcd_.pins.d5 = 5;
    lcd_.pins.d6 = 6;
    lcd_.pins.d7 = 7;
    lcd_.pins.bl = 3;

    lcd_.font = HD44780_FONT_5X8;
    lcd_.lines = rows_;
    lcd_.backlight = true;

    // Set callback that performs I2C write
    lcd_.write_cb = lcd_write_i2c;

    // ---- Initialize the LCD using I2C callback ----
    ESP_ERROR_CHECK(hd44780_init(&lcd_));
    ESP_ERROR_CHECK(hd44780_switch_backlight(&lcd_, true));
    ESP_ERROR_CHECK(hd44780_clear(&lcd_));

    ESP_LOGI(TAG, "LCD initialized successfully");

    DeviceStateEventManager::GetInstance().RegisterStateChangeCallback([this](DeviceState /*prev*/, DeviceState curr){
        bool on = !(curr == kDeviceStateIdle);
        hd44780_switch_backlight(&lcd_, on);
    });
}

// -----------------------------------------------------------------------------
CharLcdDisplay::~CharLcdDisplay()
{
    if (dev_handle_) {
        i2c_master_bus_rm_device(dev_handle_);
        dev_handle_ = nullptr;
    }
    if (bus_handle_) {
        i2c_del_master_bus(bus_handle_);
        bus_handle_ = nullptr;
    }
    g_lcd_dev = nullptr;
}

// -----------------------------------------------------------------------------
void CharLcdDisplay::SetChatMessage(const char * /*role*/, const char *content)
{
    if (!content) {
        hd44780_clear(&lcd_);
        return;
    }
    std::string filtered = lcd_filter_text(content);

    std::vector<std::string> lines;
    lines.reserve((filtered.size() + cols_ - 1) / cols_);
    for (size_t off = 0; off < filtered.size(); off += cols_) {
        lines.emplace_back(filtered.substr(off, std::min(static_cast<size_t>(cols_), filtered.size() - off)));
    }

    hd44780_clear(&lcd_);
    int delay_ms = 20;
    int initial_rows = std::min(rows_, static_cast<int>(lines.size()));
    uint32_t my_token = ++g_message_token;
    for (int r = 0; r < initial_rows; ++r) {
        hd44780_gotoxy(&lcd_, 0, r);
        const auto &ln = lines[r];
        for (size_t i = 0; i < ln.size(); ++i) {
            hd44780_putc(&lcd_, ln[i]);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
        for (int c = static_cast<int>(ln.size()); c < cols_; ++c) {
            hd44780_putc(&lcd_, ' ');
        }
        //vTaskDelay(pdMS_TO_TICKS(delay_ms));
        if (my_token != g_message_token) return;
    }
    for (int r = initial_rows; r < rows_; ++r) {
        hd44780_gotoxy(&lcd_, 0, r);
        for (int c = 0; c < cols_; ++c) {
            hd44780_putc(&lcd_, ' ');
        }
        //vTaskDelay(pdMS_TO_TICKS(delay_ms));
        if (my_token != g_message_token) return;
    }

    for (size_t i = rows_; i < lines.size(); ++i) {
        size_t start = i - rows_ + 1;
        for (int r = 0; r < rows_; ++r) {
            const auto &ln = lines[start + r];
            hd44780_gotoxy(&lcd_, 0, r);
            for (size_t k = 0; k < ln.size(); ++k) {
                hd44780_putc(&lcd_, ln[k]);
                vTaskDelay(pdMS_TO_TICKS(delay_ms));
            }
            for (int c = static_cast<int>(ln.size()); c < cols_; ++c) {
                hd44780_putc(&lcd_, ' ');
            }
            //vTaskDelay(pdMS_TO_TICKS(delay_ms));
            if (my_token != g_message_token) return;
        }
    }
}