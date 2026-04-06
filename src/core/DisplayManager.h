#pragma once

#include "pins.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "u8g2.h"

static uint8_t u8g2_esp32_i2c_byte_cb(
    u8x8_t* u8x8,
    uint8_t msg,
    uint8_t arg_int,
    void* arg_ptr)
{
    static i2c_cmd_handle_t cmd = nullptr;

    switch (msg) {
        case U8X8_MSG_BYTE_SEND:
            i2c_master_write(cmd, (uint8_t*)arg_ptr, arg_int, true);
            break;

        case U8X8_MSG_BYTE_INIT:
        case U8X8_MSG_BYTE_SET_DC:
            break;

        case U8X8_MSG_BYTE_START_TRANSFER:
            cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(
                cmd,
                (u8x8_GetI2CAddress(u8x8) << 1) | I2C_MASTER_WRITE,
                true
            );
            break;

        case U8X8_MSG_BYTE_END_TRANSFER:
            i2c_master_stop(cmd);
            i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(20));
            i2c_cmd_link_delete(cmd);
            cmd = nullptr;
            break;

        default:
            return 0;
    }
    return 1;
}

static uint8_t u8g2_esp32_gpio_and_delay_cb(
    u8x8_t*,
    uint8_t msg,
    uint8_t arg_int,
    void*)
{
    switch (msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
        case U8X8_MSG_GPIO_RESET:
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

        default:
            return 0;
    }
    return 1;
}

class U8G2 {
public:
    U8G2() = default;

    void attach(u8g2_t* native) { _native = native; }
    u8g2_t* native() { return _native; }
    const u8g2_t* native() const { return _native; }

    void clearBuffer()          { u8g2_ClearBuffer(_native); }
    void sendBuffer()           { u8g2_SendBuffer(_native); }
    void setFont(const uint8_t* font) { u8g2_SetFont(_native, font); }
    void setDrawColor(uint8_t c){ u8g2_SetDrawColor(_native, c); }
    void setFontMode(uint8_t m) { u8g2_SetFontMode(_native, m); }
    void setContrast(uint8_t c) { u8g2_SetContrast(_native, c); }

    void drawStr(int x, int y, const char* s)       { u8g2_DrawStr(_native, x, y, s); }
    void drawLine(int x1, int y1, int x2, int y2)   { u8g2_DrawLine(_native, x1, y1, x2, y2); }
    void drawFrame(int x, int y, int w, int h)       { u8g2_DrawFrame(_native, x, y, w, h); }
    void drawBox(int x, int y, int w, int h)         { u8g2_DrawBox(_native, x, y, w, h); }
    void drawRBox(int x, int y, int w, int h, int r) { u8g2_DrawRBox(_native, x, y, w, h, r); }
    void drawRFrame(int x, int y, int w, int h, int r){ u8g2_DrawRFrame(_native, x, y, w, h, r); }
    void drawCircle(int x, int y, int r)  { u8g2_DrawCircle(_native, x, y, r, U8G2_DRAW_ALL); }
    void drawDisc(int x, int y, int r)    { u8g2_DrawDisc(_native, x, y, r, U8G2_DRAW_ALL); }
    void drawPixel(int x, int y)          { u8g2_DrawPixel(_native, x, y); }
    void drawHLine(int x, int y, int w)   { u8g2_DrawHLine(_native, x, y, w); }
    void drawVLine(int x, int y, int h)   { u8g2_DrawVLine(_native, x, y, h); }

    void drawTriangle(int x1, int y1, int x2, int y2, int x3, int y3) {
        u8g2_DrawTriangle(_native, x1, y1, x2, y2, x3, y3);
    }

    int getStrWidth(const char* s) { return u8g2_GetStrWidth(_native, s); }

    // ── Bitmap drawing ────────────────────────────────────────
    // For your frame data (MSB-first raw bitmap):
    //   wBytes = image width in BYTES = pixels / 8
    //   For 128px wide: wBytes = 16
    void drawBitmap(int x, int y, int wBytes, int h, const uint8_t* bmp) {
        u8g2_DrawBitmap(_native, x, y, wBytes, h, bmp);
    }

    // For actual XBM format data (LSB-first):
    void drawXBM(int x, int y, int w, int h, const uint8_t* bmp) {
        u8g2_DrawXBMP(_native, x, y, w, h, bmp);
    }

private:
    u8g2_t* _native = nullptr;
};

class DisplayManager {
public:
    void init() {
        // ── Reset I2C driver if already installed (prevents freeze on reset) ──
        i2c_driver_delete(I2C_NUM_0);

        i2c_config_t conf = {};
        conf.mode             = I2C_MODE_MASTER;
        conf.sda_io_num       = OLED_SDA;
        conf.scl_io_num       = OLED_SCL;
        conf.sda_pullup_en    = GPIO_PULLUP_ENABLE;
        conf.scl_pullup_en    = GPIO_PULLUP_ENABLE;
        conf.master.clk_speed = 400000;

        i2c_param_config(I2C_NUM_0, &conf);
        i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);

        u8g2_Setup_ssd1306_i2c_128x64_noname_f(
            &_u8g2,
            U8G2_R0,
            u8g2_esp32_i2c_byte_cb,
            u8g2_esp32_gpio_and_delay_cb
        );

        // NOTE: u8g2_SetI2CAddress expects the 7-bit address directly.
        // The <<1 shift happens inside u8g2_esp32_i2c_byte_cb already.
        // OLED_ADDRESS = 0x3C (7-bit) — do NOT shift here.
        u8g2_SetI2CAddress(&_u8g2, OLED_ADDRESS);
        u8g2_InitDisplay(&_u8g2);
        u8g2_SetPowerSave(&_u8g2, 0);
        u8g2_SetContrast(&_u8g2, 255);

        _raw.attach(&_u8g2);

        clear();
        sendBuffer();

        ESP_LOGI(TAG, "SSD1306 ready");
    }

    U8G2& raw() { return _raw; }

    void clear()      { _raw.clearBuffer(); }
    void sendBuffer() { _raw.sendBuffer(); }

    void setFontLarge()  { _raw.setFont(u8g2_font_10x20_tr); }
    void setFontMedium() { _raw.setFont(u8g2_font_6x10_tr); }
    void setFontSmall()  { _raw.setFont(u8g2_font_5x7_tr); }

    void drawStr(int x, int y, const char* str)            { _raw.drawStr(x, y, str); }
    void drawLine(int x1, int y1, int x2, int y2)          { _raw.drawLine(x1, y1, x2, y2); }
    void drawRect(int x, int y, int w, int h)              { _raw.drawFrame(x, y, w, h); }
    void drawFilledRect(int x, int y, int w, int h)        { _raw.drawBox(x, y, w, h); }
    void drawCircle(int x, int y, int r)                   { _raw.drawCircle(x, y, r); }
    void drawPixel(int x, int y)                           { _raw.drawPixel(x, y); }

    int getWidth()  const { return OLED_WIDTH; }
    int getHeight() const { return OLED_HEIGHT; }

private:
    u8g2_t _u8g2 {};
    U8G2   _raw;

    static constexpr const char* TAG = "DisplayManager";
};