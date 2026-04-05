#pragma once
#include <string>
#include <vector>
#include <algorithm>

#include "hal.h"
#include "../IScreen.h"
#include "../../../core/DisplayManager.h"
#include "../../../core/UIManager.h"
#include "../../../application/wled/WledManager.h"
#include "../../../ui/screens/wled/WledPowerScreen.h"
#include "../../../ui/screens/wled/WledBrightnessScreen.h"
#include "../../../ui/screens/wled/WledEffectsScreen.h"
#include "../../../ui/screens/wled/WledColorScreen.h"
#include "../../../ui/screens/wled/WledSpeedScreen.h"

// ================================================================
//  WledMainScreen  —  Menu for active WLED device
//
//  Shows device name + power state in header.
//  5 items: Power, Brightness, Effects, Colors, Speed.
//  Each item shows current value on the right.
//
//  ┌──────────────────────────────────────────────────────────┐
//  │  Living Room                              ● ON           │
//  │ ─────────────────────────────────────────────────────── │
//  │  Power                                    ON            │
//  │▶ Brightness                               180           │
//  │  Effects                            Colorloop           │
//  │  Colors                                                  │
//  │  Speed                                    128           │
//  └──────────────────────────────────────────────────────────┘
// ================================================================

class WledMainScreen : public IScreen {
    public:
        WledMainScreen(DisplayManager& display, UIManager& ui, WledManager& wled)
            : _display(display), _ui(ui), _wled(wled) {}

        void onEnter() override {
            _pos        = 0;
            _dirty      = true;
            _wled.client().fetchState(_wled.state()); // Ensure we have the latest state when entering
        }

        bool needsContinuousUpdate() const override { return false; }
        bool wantsPop() const override { return _wantsPop; }

        void onButtonUp() override {
            _pos = (_pos - 1 + ITEM_COUNT) % ITEM_COUNT;
            _dirty = true;
        }

        void onButtonDown() override {
            _pos = (_pos + 1) % ITEM_COUNT;
            _dirty = true;
        }

        void onButtonSelect() override {
            switch (_pos) {
                case 0: _ui.pushScreen(new WledPowerScreen     (_display, _ui, _wled)); break;
                case 1: _ui.pushScreen(new WledBrightnessScreen(_display, _ui, _wled)); break;
                case 2: _ui.pushScreen(new WledEffectsScreen   (_display, _ui, _wled)); break;
                case 3: _ui.pushScreen(new WledColorScreen     (_display, _ui, _wled)); break;
                case 4: _ui.pushScreen(new WledSpeedScreen     (_display, _ui, _wled)); break;
            }
        }

        void onButtonBack() override { _wantsPop = true; }

        void update() override {
            if (!_dirty) return;
            _dirty = false;
            _draw();
        }

    private:
        static constexpr int ITEM_COUNT = 5;
        static constexpr int ITEM_H     = 11;
        static constexpr int LIST_Y     = 12;

        DisplayManager& _display;
        UIManager&      _ui;
        WledManager&    _wled;

        int  _pos      = 0;
        bool _wantsPop = false;

        void _draw() {
            auto& u = _display.raw();
            u.clearBuffer();
            u.setFont(u8g2_font_5x7_tr);

            const WledState& s = _wled.state();
            const WledDevice* dev = _wled.activeDevice();

            // Header: device name + power state
            std::string devName = dev ? dev->name : "Unknown";
            u.drawStr(2, 8, devName.c_str());
            const char* pwrStr = s.on ? "● ON" : "○ OFF";
            int w = u.getStrWidth(pwrStr);
            u.drawStr(126 - w, 8, pwrStr);
            u.drawLine(0, 9, 127, 9);

            // Menu items
            const char* labels[] = { "Power", "Brightness", "Effects", "Colors", "Speed" };

            for (int i = 0; i < ITEM_COUNT; i++) {
                int y = LIST_Y + i * ITEM_H;
                bool sel = (i == _pos);

                if (sel) {
                    u.setDrawColor(1);
                    u.drawRBox(0, y, 127, ITEM_H - 1, 1);
                    u.setDrawColor(0);
                    u.setFontMode(1);
                }

                u.drawStr(6, y + 8, labels[i]);

                // Right-side value
                std::string val = _itemValue(i);
                int vw = u.getStrWidth(val.c_str());
                u.drawStr(125 - vw, y + 8, val.c_str());

                if (sel) {
                    u.setDrawColor(1);
                    u.setFontMode(0);
                }
            }

            u.sendBuffer();
        }

        std::string _itemValue(int idx) {
            const WledState& s = _wled.state();
            switch (idx) {
                case 0: return s.on ? "ON" : "OFF";
                case 1: return std::to_string(s.brightness);
                case 2: return _wled.effectName(s.effectIndex);
                case 3: return "R" + std::to_string(s.color.r) +
                            " G" + std::to_string(s.color.g) +
                            " B" + std::to_string(s.color.b);
                case 4: return std::to_string(s.speed);
                default: return "";
            }
        }
};