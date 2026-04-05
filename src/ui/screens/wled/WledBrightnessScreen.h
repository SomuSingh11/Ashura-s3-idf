#pragma once
#include <string>
#include <algorithm>

#include "hal.h"
#include "../IScreen.h"
#include "../../../core/DisplayManager.h"
#include "../../../core/UIManager.h"
#include "../../../application/wled/WledManager.h"

// ================================================================
//  WledBrightnessScreen  —  Brightness slider 0-255
//
//  UP   → increase (hold accelerates)
//  DOWN → decrease (hold accelerates)
//  Debounced live send: 150ms after last change → POST
//
//  SELECT → confirm + pop
//  BACK   → revert to original + pop
// ================================================================

class WledBrightnessScreen : public IScreen {
    public:
        WledBrightnessScreen(DisplayManager& display, UIManager& ui, WledManager& wled)
        : _display(display), _ui(ui), _wled(wled) {}

        void onEnter() override {
            _originalBrightness = _wled.state().brightness;
            _brightness         = _originalBrightness;
            _lastChangeTime     = 0;
            _lastSendTime       = 0;
            _dirty              = true;
            _msg                = "";
        }

        bool needsContinuousUpdate() const override { return true; }
        bool wantsPop() const override { return _wantsPop; }

        void onButtonUp() override {
            _brightness = std::min(255, _brightness + _step());
            _onChange();
        }

        void onButtonDown() override {
            _brightness = std::max(0, _brightness - _step());
            _onChange();
        }

        void onButtonSelect() override {
            _wled.client().setBrightness(_brightness);
            _wled.state().brightness = _brightness;
            _wantsPop = true;
        }

        void onButtonBack() override {
            _wled.client().setBrightness(_originalBrightness);
            _wled.state().brightness = _originalBrightness;
            _wantsPop = true;
        }

        void update() override {
            if (_lastChangeTime > 0 && millis() - _lastChangeTime > 150) {
                _lastChangeTime = 0;
                _wled.client().setBrightness(_brightness);
                _wled.state().brightness = _brightness;
                _msg   = "";
                _dirty = true;
            }

            if (!_dirty) return;
            _dirty = false;
            _draw();
        }

    private:
        DisplayManager& _display;
        UIManager&      _ui;
        WledManager&    _wled;

        uint8_t         _brightness         = 128;
        uint8_t         _originalBrightness = 128;

        uint64_t        _lastChangeTime     = 0;
        uint64_t        _lastSendTime       = 0;
        uint64_t        _holdStart          = 0;

        bool            _wantsPop           = false;
        std::string     _msg;

        // Acceleration: hold longer = bigger steps
        int _step() {
            uint64_t held = millis() - _holdStart;
            if (held > 1000) return 20;
            if (held > 500)  return 10;
            return 5;
        }

        void _onChange() {
            if (_holdStart == 0) _holdStart = millis();
            _lastChangeTime = millis();
            _msg        = "...";
            _dirty      = true;
        }

        void _draw() {
            auto& u = _display.raw();
            u.clearBuffer();
            u.setFont(u8g2_font_5x7_tr);

            // Header
            u.drawStr(2, 8, "Brightness");
            u.drawLine(0, 9, 127, 9);

            // Value text
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", _brightness);
            int w = u.getStrWidth(buf);
            u.drawStr(126 - w, 8, buf);

            // Slider bar
            constexpr int BAR_X = 4;
            constexpr int BAR_Y = 28;
            constexpr int BAR_W = 120;
            constexpr int BAR_H = 10;

            u.drawFrame(BAR_X, BAR_Y, BAR_W, BAR_H);
            int fill = (int)((float)_brightness / 255.0f * (BAR_W - 2));
            if (fill > 0) u.drawBox(BAR_X + 1, BAR_Y + 1, fill, BAR_H - 2);

            // Percentage
            char pct[8];
            snprintf(pct, sizeof(pct), "%d%%", (int)(_brightness * 100 / 255));
            w = u.getStrWidth(pct);
            u.drawStr((128 - w) / 2, 48, pct);

            // Footer
            if (_msg.length() > 0) {
                u.drawStr(2, 62, _msg.c_str());
            } else {
                u.drawStr(2, 62, "[SEL] Done  [BCK] Cancel");
            }

            u.sendBuffer();
        }
};