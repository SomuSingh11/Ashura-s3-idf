#pragma once

#include "config.h"
#include "../IScreen.h"
#include "../../../core/DisplayManager.h"
#include "../../../application/vibes/VibeRegistry.h"
#include "../../../core/UIManager.h"
#include "../../../ui/screens/vibes/VibePreviewScreen.h"

// ================================================================
//  VibePickerScreen  —  List of animations to choose from
//
//  Used for Screensaver, Boot Screen, and Home Screen selections.
//  Active selection shown with a filled dot (●) on the right.
//
//  128×64 layout (4 items visible):
//
//  ┌────────────────────────────────────────────────────────────┐
//  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓  │
//  │▓ Kakashi                                               ●  ▓│  ← active
//  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓  │
//  │  Shoyo                                                    │
//  │  Itachi                                                   │  
//  │  Naruto                                                   │
//  │                                                ▐ scrollbar │
//  └────────────────────────────────────────────────────────────┘
//
//  SELECT → pushes VibePreviewScreen for that animation
//  BACK   → pops (UIManager handles)
//
//  _activeIndex: which vibe is currently saved (shown with ●)
//  Set by VibeScreen when pushing this screen.
// ================================================================

class VibePickerScreen : public IScreen {
    public:
        // Mode: 
        // 0 : ScreenSaver
        // 1 : Boot
        VibePickerScreen(DisplayManager& display, UIManager& ui, int mode, int activeIndex)
            : _display(display), _ui(ui), _mode(mode), _activeIndex(activeIndex) {}
        
        void onEnter() override {
            _pos    = 0;
            _winPos = 0;

            if(_activeIndex > 0) {
                _pos = _activeIndex;
                _winPos = std::max(0, _activeIndex-(SUBMENU_ITEMS_ON_SCREEN-1));
            }
            _dirty = true;
        }

        // Called by VibePreviewScreen after user confirms a selection
        void setActiveIndex(int idx) {
            _activeIndex = idx;
            _dirty = true;
        }

        void onButtonUp() override {
            if (_pos > 0) {
                _pos--;
                if (_pos < _winPos) _winPos = _pos;
            } else {
                _pos    = VIBE_COUNT - 1;
                _winPos = std::max(0, _pos - (SUBMENU_ITEMS_ON_SCREEN - 1));
            }
            _dirty = true;
        }

        void onButtonDown() override {
            if (_pos < VIBE_COUNT - 1) {
                _pos++;
                if (_pos - _winPos > SUBMENU_ITEMS_ON_SCREEN - 2 &&
                    _winPos < VIBE_COUNT - SUBMENU_ITEMS_ON_SCREEN)
                    _winPos++;
            } else {
                _pos    = 0;
                _winPos = 0;
            }
            _dirty = true;
        }

        void onButtonSelect() override {
            // Push preview screen — pass pointer to self so preview can call setActiveIndex
            _ui.pushScreen(
                new VibePreviewScreen(_display, _ui, ALL_VIBES[_pos].animation, _pos, _mode, [this](int idx) {setActiveIndex(idx);
                })
            );
        }

        void update() override {
            if (!_dirty) return;
            _dirty = false;

            auto& u = _display.raw();
            u.clearBuffer();
            u.setFont(u8g2_font_5x7_tr);

            for (int i = 0; i < SUBMENU_ITEMS_ON_SCREEN; i++) {
                int idx = _winPos + i;
                if (idx >= VIBE_COUNT) break;

                int itemY = i * SUBMENU_ITEM_HEIGHT;
                int txtY  = itemY + SUBMENU_ITEM_HEIGHT - 4;

                bool isActive = (idx == _activeIndex);

                if (idx == _pos) {
                    u.setDrawColor(1);
                    u.drawRBox(0, itemY + 1, 123, SUBMENU_ITEM_HEIGHT - 2, 1);
                    u.setDrawColor(0);
                    u.setFontMode(1);
                    u.drawStr(6, txtY, ALL_VIBES[idx].name);
                    if (isActive) {
                        u.drawDisc(118, itemY + 8, 3);
                    }
                    u.setDrawColor(1);
                    u.setFontMode(0);
                } else {
                    u.drawStr(6, txtY, ALL_VIBES[idx].name);
                    if (isActive) {
                        u.drawDisc(118, itemY + 8, 3);
                    }
                }
            }

            _drawScrollbar(u);
            u.sendBuffer();
        }

    private:
        void _drawScrollbar(U8G2& u){
            for (int y = 0; y < 64; y += 2) u.drawPixel(126, y);
            if (VIBE_COUNT <= 1) return;
            int blockH = std::max(6, 64 / VIBE_COUNT);
            int blockY = (int)((float)(64 - blockH) * _pos / (float)(VIBE_COUNT - 1));
            u.drawBox(125, blockY, 3, blockH);
        }

        DisplayManager&     _display;
        UIManager&          _ui;

        int                 _mode;
        int                 _activeIndex = 0; 
        int                 _pos = 0;
        int                 _winPos = 0;
};