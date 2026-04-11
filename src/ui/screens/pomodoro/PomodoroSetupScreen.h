#pragma once
 
#include <string>
#include <algorithm>
 
#include "hal.h"
#include "../IScreen.h"
#include "../../../core/DisplayManager.h"
#include "../../../core/UIManager.h"
#include "../../../storage/AshuraPrefs.h"
#include "../../../application/pomodoro/PomodoroEngine.h"
#include "../../../application/pomodoro/PomodoroConfig.h"
#include "../../../application/vibes/VibeRegistry.h"
#include "../vibes/VibePickerScreen.h"
#include "../../../ui/screens/pomodoro/PomodoroActiveScreen.h"
 
// ================================================================
//  PomodoroSetupScreen  —  Configure and launch a Pomodoro session
//
//  128×64 layout:
//
//  ┌──────────────────────────────────────────────────────────┐
//  │  Pomodoro                                                │
//  │ ──────────────────────────────────────────────────────── │
//  │▶ Work           25 min                                   │
//  │  Short Break     5 min                                   │
//  │  Long Break     15 min                                   │
//  │  Sessions           4                                    │
//  │  [SEL]+/-  [LSEL] Start  [BCK] Back                      │
//  └──────────────────────────────────────────────────────────┘
//
//  UP/DOWN  → navigate rows
//  SELECT   → increment value (wraps at max)
//  LONG UP  → decrement value
//  LONG SEL → save prefs + start session → push PomodoroActiveScreen
//  BACK     → pop
//
//  Values are saved to NVS on LSEL so they persist as defaults.
// ================================================================

class PomodoroSetupScreen : public IScreen {
    public:
        PomodoroSetupScreen(DisplayManager&  display, 
                            UIManager&       ui,
                            PomodoroEngine&  engine
        ): _display(display), _ui(ui), _engine(engine){}

        void onEnter() override {
            // Load persisted values
            _workMin   = AshuraPrefs::getPomodoroWorkMin();
            _shortMin  = AshuraPrefs::getPomodoroShortBreakMin();
            _longMin   = AshuraPrefs::getPomodoroLongBreakMin();
            _sessions  = AshuraPrefs::getPomodoroSessionsGoal();
            _pos       = 0;
            _dirty     = true;
        }

        bool needsContinuousUpdate() const override { return false; }
        bool wantsPop()              const override { return _wantsPop; }

        
        // ── Navigation ────────────────────────────────────────────

        void onButtonUp() override {
            _pos = (_pos-1+ITEM_COUNT) % ITEM_COUNT;
            _dirty = true;
        }

        void onButtonDown() override {
            _pos = (_pos + 1) % ITEM_COUNT;
            _dirty = true;
        }

        // SELECT → increment current field
        void onButtonSelect() override {
            _increment(_pos, +1);
            _dirty = true;
        }

        // LONG UP → decrement current field
        void onLongPressUp() override {
            _increment(_pos, -1);
            _dirty = true;
        }

        // LONG DOWN → also decrement (ergonomic alternative)
        void onLongPressDown() override {
            _increment(_pos, -1);
            _dirty = true;
        }

        // LONG SELECT → save + start
        void onLongPressSelect() override {
            _savePrefs();
    
            PomodoroConfig cfg = PomodoroConfig::fromMinutes(
                _workMin, _shortMin, _longMin, _sessions);
    
            _engine.start(cfg);
    
            // Push active screen — it will subscribe to engine events
            _ui.pushScreen(new PomodoroActiveScreen(_display, _ui, _engine));
        }
    
        void onButtonBack() override { _wantsPop = true; }
    
        void update() override {
            if (!_dirty) return;
            _dirty = false;
            _draw();
        }

    private:
        static constexpr int ITEM_COUNT = 4;
        static constexpr int ITEM_H = 11;
        static constexpr int LIST_Y = 12;

        DisplayManager&     _display;
        UIManager&          _ui;
        PomodoroEngine&     _engine;

        int     _workMin    = 25;
        int     _shortMin   = 5;
        int     _longMin    = 15;
        int     _sessions   = 4;
        int     _pos        = 0;
        bool    _wantsPop   = false;

        void _increment(int  row, int dir) {
            switch(row) {
                case 0: _workMin    = _clamp(_workMin  + dir * 5, 1, 90); break;
                case 1: _shortMin   = _clamp(_shortMin + dir * 1, 1, 30); break;
                case 2: _longMin    = _clamp(_longMin  + dir * 5, 5, 60); break;
                case 3: _sessions   = _clamp(_sessions + dir * 1, 1, 8); break;
            }
        }

        static int _clamp(int v,  int lo, int hi) {
            return v < lo ? lo : (v > hi ? hi : v);
        }

        void _savePrefs() {
            AshuraPrefs::setPomodoroWorkMin(_workMin);
            AshuraPrefs::setPomodoroShortBreakMin(_shortMin);
            AshuraPrefs::setPomodoroLongBreakMin(_longMin);
            AshuraPrefs::setPomodoroSessionsGoal(_sessions);
        }

        void _draw() {
            auto& u = _display.raw();
            u.clearBuffer();
            u.setFont(u8g2_font_5x7_tr);
    
            // Header
            u.drawStr(2, 8, "Pomodoro");
            u.drawLine(0, 9, 127, 9);
    
            const char* labels[ITEM_COUNT] = {
                "Work", "Short Break", "Long Break", "Sessions"
            };
    
            int values[ITEM_COUNT] = {
                _workMin, _shortMin, _longMin, _sessions
            };
    
            const char* units[ITEM_COUNT] = {
                "min", "min", "min", ""
            };
    
            for (int i = 0; i < ITEM_COUNT; i++) {
                int  y   = LIST_Y + i * ITEM_H;
                bool sel = (i == _pos);
    
                if (sel) {
                    u.setDrawColor(1);
                    u.drawRBox(0, y, 127, ITEM_H - 1, 1);
                    u.setDrawColor(0);
                    u.setFontMode(1);
                }
    
                u.drawStr(4, y + 8, labels[i]);
    
                // Right-side value + unit
                char buf[16];
                if (units[i][0] != '\0') {
                    snprintf(buf, sizeof(buf), "%d %s", values[i], units[i]);
                } else {
                    snprintf(buf, sizeof(buf), "%d", values[i]);
                }
                int vw = u.getStrWidth(buf);
                u.drawStr(125 - vw, y + 8, buf);
    
                if (sel) {
                    u.setDrawColor(1);
                    u.setFontMode(0);
                }
            }
    
            // Footer hint
            u.drawLine(0, 53, 127, 53);
            u.drawStr(2, 63, "[SEL]+  [LUP]-  [LSEL] Start");
    
            u.sendBuffer();
        }
};