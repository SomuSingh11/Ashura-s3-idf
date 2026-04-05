#pragma once

#include "pins.h"
#include "esp_log.h"
#include "driver/i2c.h"

#include <U8g2lib.h>

// ============================================================
//  DisplayManager  —  OLED driver wrapper
//
//  U8G2 with IDF I2C HAL.
//  All drawing calls identical to Arduino version.
// ============================================================

// ── U8G2 IDF HAL ─────────────────────────────────────────────
// U8G2 needs a callback for I2C communication under IDF.
// We implement the two required functions here.


static uint8_t u8g2_esp32_i2c_byte_cb(
    u8x8_t* u8x8,
    uint8_t msg,
    uint8_t arg_int,
    void*   arg_ptr)
{
    static i2c_cmd_handle_t cmd = nullptr;

    switch (msg) {
        case U8X8_MSG_BYTE_SEND: {
            uint8_t* data = (uint8_t*)arg_ptr;
            i2c_master_write(cmd, data, arg_int, true);
            break;
        }

        case U8X8_MSG_BYTE_INIT:
            break;

        case U8X8_MSG_BYTE_SET_DC:
            break;

        case U8X8_MSG_BYTE_START_TRANSFER: {
            cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(
                cmd,
                (u8x8_GetI2CAddress(u8x8) << 1) | I2C_MASTER_WRITE,
                true
            );
            break;
        }

        case U8X8_MSG_BYTE_END_TRANSFER: {
            i2c_master_stop(cmd);
            i2c_master_cmd_begin(
                I2C_NUM_0,
                cmd,
                pdMS_TO_TICKS(20)
            );
            i2c_cmd_link_delete(cmd);
            cmd = nullptr;
            break;
        }

        default:
            return 0;
    }
    return 1;
}

static uint8_t u8g2_esp32_gpio_and_delay_cb(
    u8x8_t* u8x8,
    uint8_t msg,
    uint8_t arg_int,
    void*   arg_ptr)
{
    switch (msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            break;

        case U8X8_MSG_DELAY_MILLI:
            vTaskDelay(pdMS_TO_TICKS(arg_int));
            break;

        case U8X8_MSG_DELAY_10MICRO:
            esp_rom_delay_us(arg_int * 10);
            break;

        case U8X8_MSG_DELAY_100NANO:
            esp_rom_delay_us(1);
            break;

        case U8X8_MSG_GPIO_RESET:
            break;

        default:
            return 0;
    }
    return 1;
}

// ── DisplayManager class ─────────────────────────────────────

class DisplayManager {
    public:
        void init() {
            // Initialize I2C driver
            i2c_config_t conf = {};
            conf.mode             = I2C_MODE_MASTER;
            conf.sda_io_num       = OLED_SDA;
            conf.scl_io_num       = OLED_SCL;
            conf.sda_pullup_en    = GPIO_PULLUP_ENABLE;
            conf.scl_pullup_en    = GPIO_PULLUP_ENABLE;
            conf.master.clk_speed = 400000; // 400kHz fast mode

            i2c_param_config(I2C_NUM_0, &conf);
            i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);

            // Initialize U8G2 with IDF callbacks
            u8g2_Setup_ssd1306_i2c_128x64_noname_f(
                _display.getU8g2(),
                U8G2_R0,
                u8g2_esp32_i2c_byte_cb,
                u8g2_esp32_gpio_and_delay_cb
            );

            // Set I2C address
            u8x8_SetI2CAddress(_display.getU8x8(), OLED_ADDRESS << 1);

            _display.begin();
            _display.setContrast(255);
            clear();
            sendBuffer();

            ESP_LOGI(TAG, "SSD1306 ready");
        };

        // ── Buffer control ────────────────────────────────────────
        void clear()      { _display.clearBuffer(); }
        void sendBuffer() { _display.sendBuffer();  }

        // ── Font helpers ──────────────────────────────────────────
        void setFontLarge(){ _display.setFont(u8g2_font_5x7_tr); }
        void setFontMedium(){ _display.setFont(u8g2_font_6x10_tr); }
        void setFontSmall(){ _display.setFont(u8g2_font_10x20_tr); }

        // ── Drawing primitives ────────────────────────────────────
        void drawStr(int x, int y, const char* str){
            _display.drawStr(x, y, str);
        };
        void drawLine(int x1, int x2, int y1, int y2) {
            _display.drawLine(x1, y1, x2, y2);
        };
        void drawRect(int x, int y, int w, int h) {
            _display.drawFrame(x, y, w, h);
        };
        void drawFilledRect(int x, int y, int w, int h) {
            _display.drawBox(x, y, w, h);
        };
        void drawCircle(int x, int y, int r) {
            _display.drawCircle(x, y, r);
        };
        void drawPixel(int x, int y) {
            _display.drawPixel(x, y);
        };

        // ── Dimensions ────────────────────────────────────────────
        int getWidth() const {return OLED_WIDTH;};
        int getHeight() const {return OLED_HEIGHT;};

        // ── Raw access for screens that use U8G2 directly ─────────
        U8G2& raw () { return _display; }

    private:
        // Use full buffer variant
        U8G2_SSD1306_128X64_NONAME_F_2ND_HW_I2C _display{U8G2_R0};

        static constexpr const char* TAG = "DisplayManager";
};