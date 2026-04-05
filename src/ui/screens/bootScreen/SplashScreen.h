#pragma once
#include "hal.h"
#include "../IScreen.h"
#include "../../../core/DisplayManager.h"
#include "../../../core/UIManager.h"
#include "../../../application/wled/WledManager.h"

// ================================================================
//  WledSpeedScreen  —  Effect speed + intensity sliders
//
//  Two sliders, SELECT toggles between them.
//  UP/DOWN adjusts active slider.
//  Debounced live send: 150ms after change → POST.
//
//  SELECT → toggle active slider (Speed ↔ Intensity)
//  BACK   → revert + pop
// ================================================================

class WledSpeedScreen : public IScreen {
public:
    WledSpeedScreen(DisplayManager& display, UIManager& ui, WledManager& wled)
        : _display(display), _ui(ui), _wled(wled) {}

    void onEnter() override {
        _origSpeed     = _wled.state().speed;
        _origIntensity = _wled.state().intensity;
        _speed         = _origSpeed;
        _intensity     = _origIntensity;
        _activeSlider  = 0;   // 0=speed, 1=intensity
        _lastChange    = 0;
        _dirty         = true;
    }

    bool needsContinuousUpdate() const override { return true; }
    bool wantsPop() const override { return _wantsPop; }

    void onButtonUp() override {
        if (_activeSlider == 0)
            _speed     = min(255, _speed + 5);
        else
            _intensity = min(255, _intensity + 5);
        _onChange();
    }

    void onButtonDown() override {
        if (_activeSlider == 0)
            _speed     = max(0, _speed - 5);
        else
            _intensity = max(0, _intensity - 5);
        _onChange();
    }

    void onButtonSelect() override {
        // Toggle active slider
        _activeSlider = (_activeSlider == 0) ? 1 : 0;
        _dirty = true;
    }

    void onButtonBack() override {
        _wled.client().setSpeed(_origSpeed, _origIntensity);
        _wled.state().speed     = _origSpeed;
        _wled.state().intensity = _origIntensity;
        _wantsPop = true;
    }

    void update() override {
        if (_lastChange > 0 && millis() - _lastChange > 150) {
            _lastChange = 0;
            _wled.client().setSpeed(_speed, _intensity);
            _wled.state().speed     = _speed;
            _wled.state().intensity = _intensity;
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

    int           _speed         = 128;
    int           _intensity     = 128;
    int           _origSpeed     = 128;
    int           _origIntensity = 128;
    int           _activeSlider  = 0;
    unsigned long _lastChange    = 0;
    bool          _wantsPop      = false;

    void _onChange() {
        _lastChange = millis();
        _dirty      = true;
    }

    void _drawSlider(U8G2& u, const char* label, int value, int y, bool active) {
        u.setFont(u8g2_font_5x7_tr);

        // Label + value
        if (active) {
            u.setDrawColor(1);
            u.drawBox(0, y - 1, 128, 10);
            u.setDrawColor(0);
            u.setFontMode(1);
        }

        u.drawStr(2, y + 7, label);
        char buf[6];
        snprintf(buf, sizeof(buf), "%d", value);
        int w = u.getStrWidth(buf);
        u.drawStr(126 - w, y + 7, buf);

        if (active) {
            u.setDrawColor(1);
            u.setFontMode(0);
        }

        // Bar
        constexpr int BAR_X = 4;
        int BAR_Y = y + 12;
        constexpr int BAR_W = 120;
        constexpr int BAR_H = 7;

        u.drawFrame(BAR_X, BAR_Y, BAR_W, BAR_H);
        int fill = (int)((float)value / 255.0f * (BAR_W - 2));
        if (fill > 0) u.drawBox(BAR_X + 1, BAR_Y + 1, fill, BAR_H - 2);
    }

    void _draw() {
        auto& u = _display.raw();
        u.clearBuffer();
        u.setFont(u8g2_font_5x7_tr);

        // Header
        u.drawStr(2, 8, "Speed & Intensity");
        u.drawLine(0, 9, 127, 9);

        // Two sliders
        _drawSlider(u, "Speed",     _speed,     12, _activeSlider == 0);
        _drawSlider(u, "Intensity", _intensity, 36, _activeSlider == 1);

        // Footer
        u.drawStr(2, 63, "[SEL] Switch  [BCK] Revert");

        u.sendBuffer();
    }
};