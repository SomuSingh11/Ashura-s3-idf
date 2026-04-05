#pragma once

// ================================================================
//  SubMenuScreen
//
//  y_offset = 0 (was 16 with header), items_on_screen = 4.
//
//  128×64 layout — 4 items visible at once:
//
//  ┌────────────────────────────────────────────────────────────┐
//  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓  │
//  │▓ Read RAW          ← selected: filled box, white text     ▓│  y=1..14
//  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓  │
//  │  Read                                                       │  y=12..28
//  │  Saved                                                      │  y=28..44
//  │  Add Manually                                               │  y=44..60
//  │                                                ▐ scrollbar │
//  └────────────────────────────────────────────────────────────┘
//
//  Values (no header variant):
//    item_height      = 16px
//    items_on_screen  = 4
//    y_offset         = 0   (no header)
//    text x           = 6
//    text y           = item_pos * 16 + 12
//    selected box     = drawRBox(0, itemY+1, 123, 14, 1)
//    scrollbar        = dotted track + filled block at x=125
// ================================================================

#include "IScreen.h"
#include "../../core/DisplayManager.h"
#include "config.h"
#include "hal.h"

#include <string>
#include <vector>
#include <functional>
#include <algorithm>


struct SubMenuItem {
    std::string           label;
    std::function<void()> onSelect;
};

class SubMenuScreen : public IScreen {
public:
    // Header param kept for API compatibility — silently ignored
    SubMenuScreen(DisplayManager& display,
                  const std::string& /* header — ignored */,
                  std::vector<SubMenuItem> items)
        : _display(display), _items(std::move(items)) {}

    SubMenuScreen(DisplayManager& display,
                  std::vector<SubMenuItem> items)
        : _display(display), _items(std::move(items)) {}

    void onEnter() override {
        _pos    = 0;
        _winPos = 0;
        _dirty  = true;
    }

    void setSelected(int index) {
        if (index >= 0 && index < (int)_items.size()) {
            _pos    = index;
            _winPos = std::max(0, index - (SUBMENU_ITEMS_ON_SCREEN - 1));
        }
    }

    void onButtonUp() override {
        if (_pos > 0) {
            _pos--;
            if (_pos < _winPos) _winPos = _pos;
        } else {
            _pos    = (int)_items.size() - 1;
            _winPos = std::max(0, _pos - (SUBMENU_ITEMS_ON_SCREEN - 1));
        }
        _dirty = true;
    }

    void onButtonDown() override {
        if (_pos < (int)_items.size() - 1) {
            _pos++;
            if (_pos - _winPos > SUBMENU_ITEMS_ON_SCREEN - 2 &&
                _winPos < (int)_items.size() - SUBMENU_ITEMS_ON_SCREEN)
                _winPos++;
        } else {
            _pos    = 0;
            _winPos = 0;
        }
        _dirty = true;
    }

    void onButtonSelect() override {
        if (_pos < (int)_items.size() && _items[_pos].onSelect)
            _items[_pos].onSelect();
    }

    void update() override {
        if (!_dirty) return;

        auto& u = _display.raw();
        u.clearBuffer();
        u.setFont(u8g2_font_5x7_tr);  

        for (int i = 0; i < SUBMENU_ITEMS_ON_SCREEN; i++) {
            int idx = _winPos + i;
            if (idx >= (int)_items.size()) break;

            int itemY = i * SUBMENU_ITEM_HEIGHT;            // top of row (y_offset=0, no header)
            int txtY  = itemY + SUBMENU_ITEM_HEIGHT - 4;    // baseline = itemY + 12

            if (idx == _pos) {
                // Filled rounded box
                u.setDrawColor(1);
                u.drawRBox(0, itemY + 1, 123, SUBMENU_ITEM_HEIGHT - 2, 1);
                // White text
                u.setDrawColor(0);
                u.setFontMode(1);
                u.drawStr(6, txtY, _items[idx].label.c_str());
                u.setDrawColor(1);
                u.setFontMode(0);
            } else {
                u.drawStr(6, txtY, _items[idx].label.c_str());
            }
        }

        _drawScrollbar((int)_items.size(), _pos, u);
        u.sendBuffer();
        _dirty = false;
    }

private:

    void _drawScrollbar(int total, int pos, U8G2& u) {
        const int H = 64;
        // Dotted track
        for (int y = 0; y < H; y += 2) u.drawPixel(126, y);
        if (total <= 1) return;
        int blockH = std::max(6, H / total);
        int blockY = (int)((float)(H - blockH) * pos / (float)(total - 1));
        u.drawBox(125, blockY, 3, blockH);
    }

    DisplayManager&          _display;
    std::vector<SubMenuItem> _items;
    int                      _pos    = 0;
    int                      _winPos = 0;
};