#include "wifi_board.h"
#include "codecs/box_audio_codec.h"
#include "display/char_lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"

#include <esp_log.h>
#include "i2c_device.h"
#include <driver/i2c_master.h>
#include <wifi_station.h>
#include <esp_timer.h>
#include "esp_io_expander_tca95xx_16bit.h"
#include "esp32_camera.h"
#include "led/circular_strip.h"

#define TAG "waveshare_s3_audio_board_lcd_2004"

class CustomBoard : public WifiBoard {
private:
    Button boot_button_;
    i2c_master_bus_handle_t i2c_bus_;
    i2c_master_bus_handle_t lcd_i2c_bus_; //for lcd char 2004
    esp_io_expander_handle_t io_expander = NULL;
    CharLcdDisplay* display_;
    Esp32Camera* camera_;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = I2C_SDA_IO,
            .scl_io_num = I2C_SCL_IO,
            .clk_source = I2C_CLK_SRC_DEFAULT,
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    // Initialize I2C for character LCD
    void InitializeLcdI2c() {
        i2c_master_bus_config_t lcd_i2c_bus_cfg = {
            .i2c_port = I2C_NUM_1,          // <-- second controller
            .sda_io_num = LCD_I2C_SDA,       // <-- your LCD SDA
            .scl_io_num = LCD_I2C_SCL,       // <-- your LCD SCL
            .clk_source = I2C_CLK_SRC_DEFAULT,
        };
    ESP_ERROR_CHECK(i2c_new_master_bus(&lcd_i2c_bus_cfg, &lcd_i2c_bus_));
    }


    void InitializeTca9555(void)
    {
        esp_err_t ret = esp_io_expander_new_i2c_tca95xx_16bit(i2c_bus_, I2C_ADDRESS, &io_expander);
        if (ret != ESP_OK)
            ESP_LOGE(TAG, "TCA9555 create returned error");

        ret = esp_io_expander_set_dir(io_expander,
                                       IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 |
                                       IO_EXPANDER_PIN_NUM_8 | IO_EXPANDER_PIN_NUM_5 |
                                       IO_EXPANDER_PIN_NUM_6,
                                       IO_EXPANDER_OUTPUT);
        ESP_ERROR_CHECK(ret);

        // Reset LCD and TouchPad
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1, 1);
        ESP_ERROR_CHECK(ret);
        vTaskDelay(pdMS_TO_TICKS(10));
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1, 0);
        ESP_ERROR_CHECK(ret);
        vTaskDelay(pdMS_TO_TICKS(10));
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1, 1);
        ESP_ERROR_CHECK(ret);

        // Enable amplifier and camera
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_8, 1);  // speaker amp enable
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_5, false); // camera reset low
        vTaskDelay(pdMS_TO_TICKS(5));
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_6, true);
        vTaskDelay(pdMS_TO_TICKS(5));
        ESP_ERROR_CHECK(ret);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
    }

    void InitializeCamera() {
        static esp_cam_ctlr_dvp_pin_config_t dvp_pin_config = {
            .data_width = CAM_CTLR_DATA_WIDTH_8,
            .data_io = {
                [0] = CAMERA_PIN_D0, [1] = CAMERA_PIN_D1, [2] = CAMERA_PIN_D2, [3] = CAMERA_PIN_D3,
                [4] = CAMERA_PIN_D4, [5] = CAMERA_PIN_D5, [6] = CAMERA_PIN_D6, [7] = CAMERA_PIN_D7,
            },
            .vsync_io = CAMERA_PIN_VSYNC,
            .de_io = CAMERA_PIN_HREF,
            .pclk_io = CAMERA_PIN_PCLK,
            .xclk_io = CAMERA_PIN_XCLK,
        };

        esp_video_init_sccb_config_t sccb_config = {
            .init_sccb = false,
            .i2c_handle = i2c_bus_,
            .freq = 100000,
        };

        esp_video_init_dvp_config_t dvp_config = {
            .sccb_config = sccb_config,
            .reset_pin = CAMERA_PIN_RESET,
            .pwdn_pin = CAMERA_PIN_PWDN,
            .dvp_pin = dvp_pin_config,
            .xclk_freq = 12000000,
        };

        esp_video_init_config_t video_config = {
            .dvp = &dvp_config,
        };

        camera_ = new Esp32Camera(video_config);
    }

public:
    CustomBoard() :
        boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeTca9555();
        InitializeButtons();

        // Character LCD at 0x27, 20x4
        display_ = new CharLcdDisplay(lcd_i2c_bus_, 0x27, 20, 4);

        InitializeCamera();

        GetBacklight()->RestoreBrightness();
    }

    virtual Led* GetLed() override {
        static CircularStrip led(BUILTIN_LED_GPIO, 6);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(i2c_bus_,
                                         AUDIO_INPUT_SAMPLE_RATE,
                                         AUDIO_OUTPUT_SAMPLE_RATE,
                                         AUDIO_I2S_GPIO_MCLK,
                                         AUDIO_I2S_GPIO_BCLK,
                                         AUDIO_I2S_GPIO_WS,
                                         AUDIO_I2S_GPIO_DOUT,
                                         AUDIO_I2S_GPIO_DIN,
                                         AUDIO_CODEC_PA_PIN,
                                         AUDIO_CODEC_ES8311_ADDR,
                                         AUDIO_CODEC_ES7210_ADDR,
                                         AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, false);
        return &backlight;
    }

    virtual Camera* GetCamera() override {
        return camera_;
    }
};

DECLARE_BOARD(CustomBoard);