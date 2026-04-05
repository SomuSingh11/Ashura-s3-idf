#pragma once
#include <string>
#include <algorithm>

#include "hal.h"
#include "../IScreen.h"
#include "../../../core/DisplayManager.h"
#include "../../../core/UIManager.h"
#include "../../../application/wled/WledManager.h"

// ================================================================
//  WledPowerScreen  —  Toggle WLED power on/off
//
//  SELECT → toggle + send immediately + pop
//  BACK   → pop without change
// ================================================================

class WledPowerScreen : public IScreen {
    public:
        WledPowerScreen(DisplayManager& display, UIManager& ui, WledManager& wled)
        : _display(display), _ui(ui), _wled(wled) {}

        void onEnter() override {
            _on = _wled.state().on;
            _dirty = true;
            _msg = "";
        }

        bool needsContinuousUpdate() const override { return false; }
        bool wantsPop() const override { return _wantsPop; }

        void onButtonSelect() override {
            _on = !_on;
            _msg = "Sending...";
            _dirty = true;
            _draw();

            if (_wled.client().setPower(_on)) {
                _wled.state().on = _on;
                _wantsPop = true;
            } else {
                _on  = !_on;   // revert
                _msg = "Failed!";
                _dirty = true;
            }
        }

        void onButtonBack() override { _wantsPop = true; }

        void update() override {
            if (!_dirty) return;
            _dirty = false;
            _draw();
        }
    private:
        DisplayManager& _display;
        UIManager&      _ui;
        WledManager&    _wled;

        bool            _on       = false;
        bool            _wantsPop = false;
        std::string     _msg;

        void _draw() {
            auto& u = _display.raw();
            u.clearBuffer();
            u.setFont(u8g2_font_5x7_tr);

            u.drawStr(2, 8, "Power");
            u.drawLine(0, 9, 127, 9);

            // Big status
            u.setFont(u8g2_font_ncenB14_tr);
            const char* status = _on ? "ON" : "OFF";
            int w = u.getStrWidth(status);
            u.drawStr((128 - w) / 2, 38, status);

            // Indicator circle
            if (_on)
                u.drawDisc(64, 48, 4);
            else
                u.drawCircle(64, 48, 4);

            u.setFont(u8g2_font_5x7_tr);
            if (_msg.length() > 0) {
                w = u.getStrWidth(_msg.c_str());
                u.drawStr((128 - w) / 2, 62, _msg.c_str());
            } else {
                u.drawStr(16, 62, "[SEL] Toggle  [BCK] Back");
            }

            u.sendBuffer();
        }
};