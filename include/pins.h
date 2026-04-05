#pragma once
// ============================================
// OLED PIN Configuration (SSD1306 128x64)
// ============================================
// I2C pins for ESP32-S3
#define OLED_SDA 12
#define OLED_SCL 11
#define OLED_ADDRESS 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

// ============================================
// Button PIN Configuration 
// ============================================
#define PIN_BUTTON_UP       4
#define PIN_BUTTON_DOWN     5
#define PIN_BUTTON_SELECT   7
#define PIN_BUTTON_BACK     15