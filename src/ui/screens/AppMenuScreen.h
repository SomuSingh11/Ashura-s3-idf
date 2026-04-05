#pragma once

#pragma once

// ================================================================
//  AppMenuScreen  —  Exact 3-row vertical menu
//
//  128×64 OLED layout:
//
//  ┌────────────────────────────────────────────────────────────┐
//  │ [icon]  Clock                    ← previous item           │
//  │┌─────────────────────────────────────────────────────────┓ │
//  ││[icon]  Spotify    ← selected row, framed                ║ │
//  │┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━╝ │
//  │ [icon]  Games                    ← next item               │
//  │                                                ▐ scrollbar │
//  └────────────────────────────────────────────────────────────┘
//
//  Selection frame styling:
//    • Thin top + left border (1px)
//    • Thick bottom + right border (2px)
//    Creates subtle depth / shadow effect
//
//  Layout coordinates:
//
//    Previous row:
//        icon  → (4, 3)
//        label → (22, 14)
//        FontSecondary (5×7)
//
//    Selected row:
//        icon  → (4, 25)
//        label → (22, 36)
//        FontPrimary (6×10)
//
//    Next row:
//        icon  → (4, 47)
//        label → (22, 58)
//        FontSecondary (5×7)
//
//    Frame region:
//        x = 0
//        y = 21
//        w = 123
//        h = 21
//
//  Rendering behaviour:
//    • No animation
//    • No blinking
//    • Dirty-flag redraws only
// ================================================================

#include "IScreen.h"
#include "../../core/DisplayManager.h"
#include "config.h"
#include <string>
#include <vector>
#include <functional>
#include <algorithm>

struct AppItem {
    std::string           icon;
    std::string           name;
    std::function<void()> onLaunch;
};

// ── 14×14 XBM icons ──────────────────────────────────────────
static const uint8_t  _ic_clock[28] = {
    0xF8,0x1F, 0x04,0x20, 0xC2,0x43, 0x02,0x40,
    0x02,0x40, 0xE2,0x47, 0xF2,0x4F, 0x02,0x40,
    0x02,0x40, 0xC2,0x43, 0x04,0x20, 0xF8,0x1F,
    0x00,0x00, 0x00,0x00,
};
static const uint8_t  _ic_games[28] = {
    0xF8,0x1F, 0x04,0x20, 0x64,0x26, 0xF4,0x2F,
    0x64,0x26, 0x04,0x20, 0xE4,0x27, 0xE4,0x27,
    0x04,0x20, 0x64,0x26, 0xF4,0x2F, 0xF8,0x1F,
    0x00,0x00, 0x00,0x00,
};
static const uint8_t  _ic_spotify[28] = {
    0xF8,0x1F, 0x04,0x20, 0xE4,0x27, 0x34,0x2C,
    0x1C,0x38, 0xE4,0x27, 0x34,0x2C, 0x1C,0x38,
    0xE4,0x27, 0x04,0x20, 0x04,0x20, 0xF8,0x1F,
    0x00,0x00, 0x00,0x00,
};
static const uint8_t  _ic_ai[28] = {
    0xF8,0x1F, 0xFC,0x3F, 0xA4,0x25, 0xFC,0x3F,
    0xA4,0x25, 0xFC,0x3F, 0x04,0x20, 0x84,0x21,
    0xC4,0x23, 0xA4,0x25, 0x94,0x29, 0xF8,0x1F,
    0x00,0x00, 0x00,0x00,
};
static const uint8_t  _ic_settings[28] = {
    0x60,0x06, 0x60,0x06, 0xF8,0x1F, 0xFC,0x3F,
    0x0E,0x70, 0x06,0x60, 0x06,0x60, 0x0E,0x70,
    0xFC,0x3F, 0xF8,0x1F, 0x60,0x06, 0x60,0x06,
    0x00,0x00, 0x00,0x00,
};
static const uint8_t  _ic_wifi[28] = {
    0xF8,0x1F, 0x04,0x20, 0xF0,0x0F, 0x18,0x18,
    0xE0,0x07, 0x30,0x0C, 0xC0,0x03, 0xC0,0x03,
    0x80,0x01, 0x80,0x01, 0x00,0x00, 0x80,0x01,
    0x00,0x00, 0x00,0x00,
};
static const uint8_t  _ic_wled[28] = {
    0xFF,0x7F, 0x01,0x40, 0x01,0x40, 0xFD,0x5F,
    0x05,0x50, 0xFD,0x5F, 0x05,0x50, 0xFD,0x5F,
    0x01,0x40, 0x01,0x40, 0xFF,0x7F, 0x00,0x00,
    0x00,0x00, 0x00,0x00,
};
static const uint8_t  _ic_default[28] = {
    0xF8,0x1F, 0x04,0x20, 0x04,0x20, 0x04,0x20,
    0x04,0x20, 0x84,0x21, 0x84,0x21, 0x84,0x21,
    0x04,0x20, 0x04,0x20, 0x04,0x20, 0xF8,0x1F,
    0x00,0x00, 0x00,0x00,
};

inline const uint8_t* _appIcon(const std::string& id) {
    if (id == "clock"   ) return _ic_clock;
    if (id == "games"   ) return _ic_games;
    if (id == "spotify" ) return _ic_spotify;
    if (id == "ai"      ) return _ic_ai;
    if (id == "settings") return _ic_settings;
    if (id == "wifi"    ) return _ic_wifi;
    if (id == "wled"    ) return _ic_wled;
    return _ic_default;
}

// ──  Menu coordinates ─────────────────────────
static constexpr uint8_t IC_W       = 14;
static constexpr uint8_t IC_H       = 14;

static constexpr uint8_t PREV_ICX   = 4;
static constexpr uint8_t PREV_ICY   = 3;
static constexpr uint8_t PREV_LX    = 22;
static constexpr uint8_t PREV_LY    = 14;

static constexpr uint8_t SEL_ICX    = 4;
static constexpr uint8_t SEL_ICY    = 25;
static constexpr uint8_t SEL_LX     = 22;
static constexpr uint8_t SEL_LY     = 36;

static constexpr uint8_t NEXT_ICX   = 4;
static constexpr uint8_t NEXT_ICY   = 47;
static constexpr uint8_t NEXT_LX    = 22;
static constexpr uint8_t NEXT_LY    = 58;

// Frame region
static constexpr uint8_t FX         = 0;
static constexpr uint8_t FY         = 21;
static constexpr uint8_t FW         = 123;
static constexpr uint8_t FH         = 21;

class AppMenuScreen : public IScreen {
public:
    AppMenuScreen(DisplayManager& display, std::vector<AppItem> apps)
        : _display(display), _apps(std::move(apps)) {}

    void onEnter() override {
        _cursor = 0;
        _dirty  = true;
    }

    void onButtonUp() override {
        _cursor = (_cursor - 1 + (int)_apps.size()) % (int)_apps.size();
        _dirty  = true;
    }

    void onButtonDown() override {
        _cursor = (_cursor + 1) % (int)_apps.size();
        _dirty  = true;
    }

    void onButtonSelect() override {
        if (_cursor < (int)_apps.size() && _apps[_cursor].onLaunch)
            _apps[_cursor].onLaunch();
    }

    void update() override {
        if (!_dirty) return;

        int   n = (int)_apps.size();
        auto& u = _display.raw();
        u.clearBuffer();

        if (n == 0) {
            u.setFont(u8g2_font_6x10_tr);
            u.drawStr(32, 35, "No apps");
            u.sendBuffer();
            _dirty = false;
            return;
        }

        int iPrev = (_cursor - 1 + n) % n;
        int iNext = (_cursor + 1) % n;

        // ── Prev row ──────────────────────────────────────────
        u.setFont(u8g2_font_5x7_tr);
        u.drawXBM(PREV_ICX, PREV_ICY, IC_W, IC_H, _appIcon(_apps[iPrev].icon));
        u.drawStr(PREV_LX, PREV_LY, _apps[iPrev].name.c_str());

        // ── Selection frame — thin top+left, thick bottom+right ──
        // Top edge (1px)
        u.drawHLine(FX, FY, FW);
        // Left edge (1px)
        u.drawVLine(FX, FY, FH);
        // Bottom edge (2px thick)
        u.drawHLine(FX, FY + FH - 1, FW);
        u.drawHLine(FX, FY + FH,     FW);
        // Right edge (2px thick)
        u.drawVLine(FX + FW - 1, FY, FH);
        u.drawVLine(FX + FW,     FY, FH);

        // ── Selected row ──────────────────────────────────────
        u.drawXBM(SEL_ICX, SEL_ICY, IC_W, IC_H, _appIcon(_apps[_cursor].icon));
        u.setFont(u8g2_font_6x10_tr);
        u.drawStr(SEL_LX, SEL_LY, _apps[_cursor].name.c_str());

        // ── Next row ──────────────────────────────────────────
        u.setFont(u8g2_font_5x7_tr);
        u.drawXBM(NEXT_ICX, NEXT_ICY, IC_W, IC_H, _appIcon(_apps[iNext].icon));
        u.drawStr(NEXT_LX, NEXT_LY, _apps[iNext].name.c_str());

        // ── Scrollbar ─────────────────────────────────────────
        for (int y = 0; y < 64; y += 2) u.drawPixel(126, y);
        if (n > 1) {
            int bH = std::max(6, 64 / n);
            int bY = (int)((float)(64 - bH) * _cursor / (float)(n - 1));
            u.drawBox(125, bY, 3, bH);
        }

        u.sendBuffer();
        _dirty = false;
    }

private:
    DisplayManager&      _display;
    std::vector<AppItem> _apps;
    int                  _cursor = 0;
};