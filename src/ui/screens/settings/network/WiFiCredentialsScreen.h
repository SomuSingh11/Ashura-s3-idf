#pragma once
#include <string>
#include <functional>
#include <algorithm>

#include "hal.h"
#include "../../IScreen.h"
#include "../../../../core/DisplayManager.h"
#include "../../../../core/UIManager.h"
#include "../../../../core/WiFiManager.h"
#include "../../../widgets/CharPickerWidget.h"

// ================================================================
//  WiFiCredentialsScreen  —  SSID + Password editor
//
//  Plain two-row menu. Each row edits one field independently.
//
//  Controls:
//    UP / DOWN     move cursor between SSID and Password rows
//    SELECT        push CharPicker for the highlighted field
//    LONG SELECT   save credentials + pop (only if both fields filled)
//    BACK          pop without saving — UIManager handles naturally
//
//  CharPicker confirms via onDone lambda → updates field in place.
//  CharPicker BACK → pops back here, field unchanged.
//
//  Layout:
//  ┌──────────────────────────────────────────────────────────┐
//  │  WiFi Credentials                                        │
//  │ ─────────────────────────────────────────────────────── │
//  │▶ SSID       MyNetwork                                    │
//  │  Password   ••••••••                                     │
//  │                                                          │
//  │  [SEL] Edit            [LSEL] Save & Connect             │
//  └──────────────────────────────────────────────────────────┘
// ================================================================

class WiFiCredentialsScreen : public IScreen {
public:
    WiFiCredentialsScreen(DisplayManager& display,
                          UIManager&      ui,
                          WiFiManager&    wifi)
        : _display(display), _ui(ui), _wifi(wifi) {}

    void onEnter() override {
        // Pre-fill with saved values on every entry
        // Safe to call on return from picker too —
        // picker has already updated _ssid/_pass via lambda
        if (_ssid.empty()) _ssid = _wifi.ssid();
        _dirty = true;
    }

    bool needsContinuousUpdate() const override { return false; }
    bool wantsPop()              const override { return _wantsPop; }

    // ── Buttons ───────────────────────────────────────────────
    void onButtonUp() override {
        _cursor = (_cursor - 1 + FIELD_COUNT) % FIELD_COUNT;
        _dirty  = true;
    }

    void onButtonDown() override {
        _cursor = (_cursor + 1) % FIELD_COUNT;
        _dirty  = true;
    }

    void onButtonSelect() override {
        if (_cursor == 0){
            _pushSsidPicker();
        } else  {
            _pushPassPicker();
        }            
    }

    // LONG SELECT — save only if both fields have content
    void onLongPressSelect() override {
        if (_ssid.empty()) {
            _hint = "SSID required";
            _dirty = true;
            return;
        }
        if (_pass.empty()) {
            _hint = "Password required";
            _dirty = true;
            return;
        }
        _wifi.saveCredentials(_ssid, _pass);
        _wantsPop = true;
    }

    // BACK — UIManager pops naturally, nothing to do
    void onButtonBack() override {}

    void update() override {
        if (!_dirty) return;
        _dirty = false;
        _draw();
    }

private:
    static constexpr int FIELD_COUNT = 2;
    static constexpr int ROW_H       = 13;
    static constexpr int LIST_Y      = 12;

    DisplayManager& _display;
    UIManager&      _ui;
    WiFiManager&    _wifi;

    std::string _ssid;
    std::string _pass;
    std::string _empty = "";
    std::string _hint;
    int    _cursor   = 0;
    bool   _wantsPop = false;

    void _pushSsidPicker() {
        _hint = "";
        _ui.pushScreen(new CharPickerWidget(
            _display, "WiFi SSID", _ssid,
            [this](const std::string& result) {
                _ssid  = result;
                _dirty = true;
            }
        ));
    }

    void _pushPassPicker() {
        _hint = "";
        _ui.pushScreen(new CharPickerWidget(
            _display, "WiFi Password", _empty,
            [this](const std::string& result) {
                _pass  = result;
                _dirty = true;
            }
        ));
    }

    void _draw() {
        auto& u = _display.raw();
        u.clearBuffer();
        u.setFont(u8g2_font_5x7_tr);

        // Header
        u.drawStr(2, 8, "WiFi Credentials");
        u.drawLine(0, 9, 127, 9);

        // Rows
        const char* labels[FIELD_COUNT] = { "SSID", "Password" };
        std::string values[FIELD_COUNT] = {
            _ssid.empty() ? "(not set)" : _ssid,
            _pass.empty() ? "(not set)" : _maskedPass()
        };

        for (int i = 0; i < FIELD_COUNT; i++) {
            int  y   = LIST_Y + i * ROW_H;
            bool sel = (i == _cursor);

            if (sel) {
                u.drawRBox(0, y, 127, ROW_H - 1, 1);
                u.setDrawColor(0);
                u.setFontMode(1);
            }

            u.drawStr(4, y + 9, labels[i]);
            int vw = u.getStrWidth(values[i].c_str());
            u.drawStr(125 - vw, y + 9, values[i].c_str());

            if (sel) {
                u.setDrawColor(1);
                u.setFontMode(0);
            }
        }

        // Hint or footer
        u.drawLine(0, 53, 127, 53);
        if (!_hint.empty()) {
            u.drawStr(2, 63, _hint.c_str());
        } else {
            u.drawStr(2, 63, "[SEL] Edit  [LSEL] Save");
        }

        u.sendBuffer();
    }

    std::string _maskedPass() const {
        std::string s = "";
        int n = std::min((int)_pass.length(), 12);
        for (int i = 0; i < n; i++) s += '*';
        return s;
    }
};