#pragma once
#include <string>
#include <algorithm>
#include "hal.h"
#include <cmath>

#include "../IScreen.h"
#include "../../../core/DisplayManager.h"
#include "../../../core/UIManager.h"
#include "../../../application/wled/WledManager.h"

// ================================================================
//  WledColorScreen  —  Color presets + hue scrubber
//
//  Two modes toggled by SELECT:
//    Mode 0 — Preset list (Red, Green, Blue, White...)
//    Mode 1 — Hue scrubber (UP/DOWN scrubs 0-255 hue)
//
//  In preset mode: SELECT applies color + pops
//  In hue mode:    SELECT confirms hue + pops
//  BACK always reverts + pops
// ================================================================

class WledColorScreen : public IScreen {
public:
    WledColorScreen(DisplayManager& display, UIManager& ui, WledManager& wled)
        : _display(display), _ui(ui), _wled(wled) {}

    void onEnter() override {
        _original   = _wled.state().color;
        _mode       = 0;
        _presetPos  = 0;
        _hue        = 0;
        _lastChange = 0;
        _dirty      = true;
    }

    bool needsContinuousUpdate() const override { return true; }
    bool wantsPop() const override { return _wantsPop; }

    void onButtonUp() override {
        if (_mode == 0) {
            _presetPos = (_presetPos - 1 + PRESET_COUNT) % PRESET_COUNT;
        } else {
            _hue = (_hue + 5) % 256;
            _lastChange = millis();
        }
        _dirty = true;
    }

    void onButtonDown() override {
        if (_mode == 0) {
            _presetPos = (_presetPos + 1) % PRESET_COUNT;
        } else {
            _hue = (_hue - 5 + 256) % 256;
            _lastChange = millis();
        }
        _dirty = true;
    }

    void onButtonSelect() override {
        if (_mode == 0) {
            // Apply preset + pop
            WledColor c = _presets[_presetPos].color;
            _wled.client().setColor(c);
            _wled.state().color = c;
            _wantsPop = true;
        } else {
            // Switch modes
            _mode = (_mode == 0) ? 1 : 0;
            _dirty = true;
        }
    }

    void onButtonBack() override {
        _wled.client().setColor(_original);
        _wled.state().color = _original;
        _wantsPop = true;
    }

    void update() override {
        // Debounced hue send
        if (_mode == 1 && _lastChange > 0 && millis() - _lastChange > 150) {
            _lastChange = 0;
            WledColor c = _hueToColor(_hue);
            _wled.client().setColor(c);
            _wled.state().color = c;
            _dirty = true;
        }

        if (!_dirty) return;
        _dirty = false;
        _draw();
    }

private:
    struct Preset {
        const char* name;
        WledColor   color;
    };

    static constexpr int PRESET_COUNT = 9;
    static constexpr int ITEMS_VIS    = 4;
    static constexpr int ITEM_H       = 13;

    const Preset _presets[PRESET_COUNT] = {
        { "Red",    WledColor::Red()    },
        { "Green",  WledColor::Green()  },
        { "Blue",   WledColor::Blue()   },
        { "White",  WledColor::White()  },
        { "Warm",   WledColor::Warm()   },
        { "Orange", WledColor::Orange() },
        { "Purple", WledColor::Purple() },
        { "Cyan",   WledColor::Cyan()   },
        { "Pink",   WledColor::Pink()   },
    };

    DisplayManager& _display;
    UIManager&      _ui;
    WledManager&    _wled;

    WledColor     _original;
    int           _mode      = 0;
    int           _presetPos = 0;
    int           _hue       = 0;
    unsigned long _lastChange = 0;
    bool          _wantsPop  = false;

    // Simple HSV hue → RGB (S=255, V=255)
    WledColor _hueToColor(int hue) {
        float h = hue / 255.0f * 360.0f;
        float s = 1.0f, v = 1.0f;
        int   i = (int)(h / 60) % 6;
        float f = h / 60 - (int)(h / 60);
        float p = v * (1 - s);
        float q = v * (1 - f * s);
        float t = v * (1 - (1 - f) * s);
        float r, g, b;
        switch (i) {
            case 0: r=v; g=t; b=p; break;
            case 1: r=q; g=v; b=p; break;
            case 2: r=p; g=v; b=t; break;
            case 3: r=p; g=q; b=v; break;
            case 4: r=t; g=p; b=v; break;
            default:r=v; g=p; b=q; break;
        }
        return { (uint8_t)(r*255), (uint8_t)(g*255), (uint8_t)(b*255) };
    }

    void _draw() {
        auto& u = _display.raw();
        u.clearBuffer();
        u.setFont(u8g2_font_5x7_tr);

        // Header
        u.drawStr(2, 8, "Color");
        const char* modeStr = (_mode == 0) ? "[Presets]" : "[Hue]";
        int w = u.getStrWidth(modeStr);
        u.drawStr(126 - w, 8, modeStr);
        u.drawLine(0, 9, 127, 9);

        if (_mode == 0) {
            _drawPresets(u);
        } else {
            _drawHue(u);
        }

        u.sendBuffer();
    }

    void _drawPresets(U8G2& u) {
        int start = std::max(0, _presetPos - (ITEMS_VIS - 1));

        for (int i = 0; i < ITEMS_VIS; i++) {
            int idx = start + i;
            if (idx >= PRESET_COUNT) break;

            int  y   = 12 + i * ITEM_H;
            bool sel = (idx == _presetPos);

            if (sel) {
                u.setDrawColor(1);
                u.drawRBox(0, y, 123, ITEM_H - 1, 1);
                u.setDrawColor(0);
                u.setFontMode(1);
            }

            // Color swatch — simple filled square
            u.drawBox(6, y + 2, 8, 8);

            u.drawStr(20, y + 9, _presets[idx].name);

            if (sel) {
                u.setDrawColor(1);
                u.setFontMode(0);
            }
        }

        u.drawStr(2, 63, "[SEL] Apply  [BCK] Revert");
    }

    void _drawHue(U8G2& u) {
        // Hue bar — dithered approximation on mono OLED
        // Draw alternating pixels to give sense of gradient
        for (int x = 4; x < 124; x++) {
            int h = (x - 4) * 255 / 120;
            WledColor c = _hueToColor(h);
            // Brightness approximation for monochrome
            int brightness = (c.r + c.g + c.b) / 3;
            if (brightness > 96) u.drawPixel(x, 28);
            if (brightness > 64) u.drawPixel(x, 29);
            if (brightness > 32) u.drawPixel(x, 30);
        }

        // Frame
        u.drawFrame(3, 26, 122, 8);

        // Cursor
        int cx = 4 + _hue * 120 / 255;
        u.drawLine(cx, 22, cx, 36);

        // Hue value
        char buf[16];
        snprintf(buf, sizeof(buf), "Hue: %d", _hue);
        int w = u.getStrWidth(buf);
        u.drawStr((128 - w) / 2, 48, buf);

        // Color preview — show RGB values
        WledColor c = _hueToColor(_hue);
        char rgb[24];
        snprintf(rgb, sizeof(rgb), "R%d G%d B%d", c.r, c.g, c.b);
        w = u.getStrWidth(rgb);
        u.drawStr((128 - w) / 2, 58, rgb);

        u.drawStr(2, 63, "[UP/DN] Hue  [SEL] Mode  [BCK] Revert");
    }
};