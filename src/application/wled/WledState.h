#pragma once

#include <string>
#include <cstdint>
#include <cstdio>
#include <algorithm>

// ================================================================
//  WledState  —  Current state of a WLED device
//
//  Populated by WledClient::fetchState().
//  Screens read this to show current values.
//  Screens write to this then call WledClient::pushState().
// ================================================================

struct WledColor {
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;

    WledColor() {}
    WledColor(uint8_t red, uint8_t green, uint8_t blue): r(red), g(green), b(blue) {}

    // ── Convert to/from packed 24-bit int ─────────────────────────────────────────────
    //Convert RGB → 24-bit number
    uint32_t toInt() const {
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
    // Convert 24-bit → RGB
    static WledColor fromInt(uint32_t color){
        return {
            (uint8_t)((color >> 16) & 0xFF),
            (uint8_t)((color >> 8)  & 0xFF),
            (uint8_t)(color & 0xFF)
        };
    }

    // ── Named Presets ─────────────────────────────────────────────
    static WledColor Red()     { return {255,   0,   0}; }
    static WledColor Green()   { return {  0, 255,   0}; }
    static WledColor Blue()    { return {  0,   0, 255}; }
    static WledColor White()   { return {255, 255, 255}; }
    static WledColor Warm()    { return {255, 180,  80}; }
    static WledColor Orange()  { return {255, 100,   0}; }
    static WledColor Purple()  { return {180,   0, 255}; }
    static WledColor Cyan()    { return {  0, 255, 255}; }
    static WledColor Pink()    { return {255,  20, 147}; }
    static WledColor Off()     { return {  0,   0,   0}; }
};

struct WledState {
    bool        on              = false;  // Power state
    uint8_t     brightness      = 128;    // Brightness (0-255)
    int         effectIndex     = 0;      // Effect index (0 = solid color)
    uint32_t    speed           = 128;    // Effect speed (0-255)
    uint32_t    intensity       = 128;    // Effect intensity (0-255)
    WledColor   color;

    bool        valid           = false;  // Whether this state has been successfully fetched from the device

    // ── Build JSON for POST /json/state ─────────────────────────────────────────────
    // Only includes fields that are set
    std::string toJson() const {
        char buf[256];
        snprintf(buf, sizeof(buf),
            "{\"on\":%s,\"bri\":%d,"
            "\"seg\":[{\"fx\":%d,\"sx\":%lu,\"ix\":%lu,"
            "\"col\":[[%d,%d,%d]]}]}",
            on ? "true" : "false",
            brightness,
            effectIndex,
            (unsigned long)speed,
            (unsigned long)intensity,
            color.r, color.g, color.b
        );
        return std::string(buf);
    }
    // Example JSON output:
    // {
    //     "on": true,
    //     "bri": 128,
    //     "seg": [{
    //         "fx": 3,
    //         "sx": 150,
    //         "ix": 200,
    //         "col": [[255,0,0]]
    //     }]
    // }

    // ── Partial JSON helpers — send only what changed ─────────────────────────────────────────────
    std::string jsonPower() const {
        char buf[24];
        snprintf(buf, sizeof(buf),
                 "{\"on\":%s}", on ? "true" : "false");
        return std::string(buf);
    }

    std::string jsonBrightness() const {
        char buf[16];
        snprintf(buf, sizeof(buf), "{\"bri\":%d}", brightness);
        return std::string(buf);
    }

    std::string jsonEffect() const {
        char buf[32];
        snprintf(buf, sizeof(buf),
                 "{\"seg\":[{\"fx\":%d}]}", effectIndex);
        return std::string(buf);
    }

    std::string jsonSpeed() const {
        char buf[48];
        snprintf(buf, sizeof(buf),
                 "{\"seg\":[{\"sx\":%lu,\"ix\":%lu}]}",
                 (unsigned long)speed,
                 (unsigned long)intensity);
        return std::string(buf);
    }

    std::string jsonColor() const {
        char buf[64];
        snprintf(buf, sizeof(buf),
                 "{\"seg\":[{\"col\":[[%d,%d,%d]]}]}",
                 color.r, color.g, color.b);
        return std::string(buf);
    }
};