#pragma once

// ================================================================
//  VibePreviewScreen  —  Fullscreen animation preview + confirm
//
//  Plays the selected animation fullscreen with a bar overlay.
//  Uses AnimationPlayer Mode 2 (tick + drawFrame) so the screen
//  can draw the confirm bar on top of the animation each frame.
//
//  Flow:
//    tick()      → advances frame counter if time elapsed
//    clearBuffer → wipe display
//    drawFrame() → writes animation XBM into buffer
//    _drawBar()  → draws confirm bar on top of buffer
//    sendBuffer  → push to OLED
//
//  SELECT → save to NVS, update picker dot, show ✓ flash, pop
//  BACK   → pop without saving
//
//  MODE:
//    0 = Screensaver
//    1 = Boot Screen
//    2 = Home Screen
//    3 = Pomodoro Work vibe
//    4 = Pomodoro Break vibe
//    5 = Pomodoro Complete vibe
// ================================================================

#include "hal.h"
#include <functional>
#include "../IScreen.h"
#include "../../../core/DisplayManager.h"
#include "../../../core/UIManager.h"
#include "../../../storage/AshuraPrefs.h"
#include "../../../application/vibes/AnimationPlayer.h"

class VibePreviewScreen : public IScreen {
    public:
        VibePreviewScreen(DisplayManager& display, 
                          UIManager& ui, 
                          const Animation* animation, 
                          int vibeIndex, 
                          int mode,
                          std::function<void(int)> onConfirmed) 
            : _display(display)
            , _ui(ui)
            , _vibeIndex(vibeIndex)
            , _mode(mode)
            , _onConfirmed(onConfirmed)
        { 
        _player.setAnimation(animation);
        }

        void onEnter() override {
            _player.reset();
            _confirmed = false;
            _wantsPop = false;
            _confirmedAt = 0;
        }

        bool needsContinuousUpdate() const override { return true; }
        bool wantsPop()              const override { return _wantsPop; }

        void onButtonSelect() override {
            if(_confirmed) return;

            switch(_mode) {
                case 0: AshuraPrefs::setScreensaver(_vibeIndex); break;
                case 1: AshuraPrefs::setBoot(_vibeIndex); break;
                // case 2: AshuraPrefs::setHomeScreen(_vibeIndex); break;
                case 2: AshuraPrefs::setPomodoroWorkVibe(_vibeIndex); break;
                case 3: AshuraPrefs::setPomodoroBreakVibe(_vibeIndex); break;
                case 4: AshuraPrefs::setPomodoroCompleteVibe(_vibeIndex); break;
            }

            if (_onConfirmed) _onConfirmed(_vibeIndex);

            _confirmed   = true;
            _confirmedAt = millis();
            _dirty       = true;
        }

        void onButtonBack() override { _wantsPop = true; }

        void update() override {
            // Pop after confirm flash - 1s delay
            if(_confirmed && millis()-_confirmedAt > 1000){
                _wantsPop = true;
                return;
            }

            // Mode 2a: tick advances frame, returns true if changed
            bool advanced = _player.tick();
            if(!advanced && !_dirty) return;

            // Consumed the dirty flag, reset it
            // Once you redraw, that change has been visually applied.
            _dirty = false; 

            auto& u = _display.raw();
            u.clearBuffer();
            _player.drawFrame(_display);

            if(!_confirmed) {
                _drawBar(u); 
            } else {
                _drawConfirmed(u);
            }

            u.sendBuffer();
        }

    private:
        DisplayManager&          _display;
        UIManager&               _ui;
        AnimationPlayer          _player;
        int                      _vibeIndex;
        int                      _mode;
        std::function<void(int)> _onConfirmed;

        bool                _confirmed = false;
        uint64_t            _confirmedAt = 0;
        bool                _wantsPop = false;

        void _drawBar(U8G2& u){
            u.setDrawColor(1);
            u.drawBox(0, 54, 128, 10);
            u.setDrawColor(0);
            u.setFontMode(1);
            u.setFont(u8g2_font_5x7_tr);
            u.drawStr(4, 62, "[SEL] Set    [BCK] Back");
            u.setDrawColor(1);
            u.setFontMode(0);
        }

        void _drawConfirmed(U8G2& u) {
            u.setDrawColor(1);
            u.drawRBox(34, 24, 60, 18, 2);
            u.setDrawColor(0);
            u.setFontMode(1);
            u.setFont(u8g2_font_6x10_tr);
            u.drawStr(44, 36, "Set!");
            u.setDrawColor(1);
            u.setFontMode(0);
        }
};