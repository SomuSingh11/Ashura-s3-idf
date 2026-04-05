#pragma once
#include <string>
#include <algorithm>
#include <vector>

#include "hal.h"
#include "../IScreen.h"
#include "../../../core/DisplayManager.h"
#include "../../../core/UIManager.h"
#include "../../../application/wled/WledManager.h"

// ================================================================
//  WledEffectsScreen  —  Scrollable effects list
//
//  Effects list fetched from WledManager cache (loaded on connect).
//  Live debounced preview: 150ms after scroll stops → POST effect.
//
//  SELECT → confirm current effect + pop
//  BACK   → revert to original effect + pop
// ================================================================

class WledEffectsScreen : public IScreen {
public:
    WledEffectsScreen(DisplayManager& display, UIManager& ui, WledManager& wled)
        : _display(display), _ui(ui), _wled(wled) {}

    void onEnter() override {
        _original   = _wled.state().effectIndex;
        _pos        = _original;
        _winPos     = std::max(0, _pos - (ITEMS_VIS / 2));
        _lastChange = 0;
        _applying   = false;
        _dirty      = true;
    }

    bool needsContinuousUpdate() const override { return true; }
    bool wantsPop() const override { return _wantsPop; }

    void onButtonUp() override {
        int count = _wled.effectCount();
        if (count == 0) return;
        _pos = (_pos - 1 + count) % count;
        if (_pos < _winPos) _winPos = _pos;
        if (_winPos > 0 && _pos == count - 1) _winPos = count - ITEMS_VIS;
        _onChange();
    }

    void onButtonDown() override {
        int count = _wled.effectCount();
        if (count == 0) return;
        _pos = (_pos + 1) % count;
        if (_pos >= _winPos + ITEMS_VIS)
            _winPos = _pos - ITEMS_VIS + 1;
        if (_pos == 0) _winPos = 0;
        _onChange();
    }

    void onButtonSelect() override {
        // Confirm
        _wled.state().effectIndex = _pos;
        _wantsPop = true;
    }

    void onButtonBack() override {
        // Revert
        _wled.client().setEffect(_original);
        _wled.state().effectIndex = _original;
        _wantsPop = true;
    }

    void update() override {
        // Debounced send
        if (_lastChange > 0 && millis() - _lastChange > 150) {
            _lastChange = 0;
            _applying   = true;
            _dirty      = true;
            _draw();   // show "applying" state
            _wled.client().setEffect(_pos);
            _applying = false;
            _dirty    = true;
        }

        if (!_dirty) return;
        _dirty = false;
        _draw();
    }

private:
    static constexpr int ITEMS_VIS = 4;
    static constexpr int ITEM_H    = 13;
    static constexpr int LIST_Y    = 11;

    DisplayManager& _display;
    UIManager&      _ui;
    WledManager&    _wled;

    int           _pos        = 0;
    int           _winPos     = 0;
    int           _original   = 0;
    uint64_t      _lastChange = 0;
    bool          _applying   = false;
    bool          _wantsPop   = false;

    void _onChange() {
        _lastChange = millis();
        _dirty      = true;
    }

    void _draw() {
        auto& u = _display.raw();
        u.clearBuffer();
        u.setFont(u8g2_font_5x7_tr);

        // Header
        u.drawStr(2, 8, "Effects");
        if (_applying) {
            u.drawStr(80, 8, "Applying...");
        }
        u.drawLine(0, 9, 127, 9);

        int count = _wled.effectCount();

        if (count == 0) {
            u.drawStr(10, 35, "No effects loaded");
            u.drawStr(10, 47, "Reconnect device");
        } else {
            for (int i = 0; i < ITEMS_VIS; i++) {
                int idx = _winPos + i;
                if (idx >= count) break;

                int y   = LIST_Y + i * ITEM_H;
                bool sel = (idx == _pos);

                if (sel) {
                    u.setDrawColor(1);
                    u.drawRBox(0, y, 123, ITEM_H - 1, 1);
                    u.setDrawColor(0);
                    u.setFontMode(1);
                }

                u.drawStr(6, y + 9, _wled.effectName(idx).c_str());

                // Original marker
                if (idx == _original) {
                    u.drawStr(118, y + 9, "*");
                }

                if (sel) {
                    u.setDrawColor(1);
                    u.setFontMode(0);
                }
            }

            // Scrollbar
            if (count > ITEMS_VIS) {
                for (int y = LIST_Y; y < 63; y += 2) u.drawPixel(126, y);
                int blockH = std::max(6, (63 - LIST_Y) / count * ITEMS_VIS);
                int blockY = LIST_Y + (int)((float)(_pos) / count * (63 - LIST_Y - blockH));
                u.drawBox(125, blockY, 3, blockH);
            }
        }

        // Footer
        u.drawStr(2, 63, "[SEL] Confirm  [BCK] Revert");

        u.sendBuffer();
    }
};