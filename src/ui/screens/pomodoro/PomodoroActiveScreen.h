#pragma once
 
#include <string>
#include <cstdio>
 
#include "hal.h"
#include "../IScreen.h"
#include "../../../core/DisplayManager.h"
#include "../../../core/UIManager.h"
#include "../../../core/EventBus.h"
#include "../../../storage/AshuraPrefs.h"
#include "../../../application/pomodoro/PomodoroEngine.h"
#include "../../../application/vibes/AnimationPlayer.h"
#include "../../../application/vibes/VibeRegistry.h"
 
#include "../../../ui/screens/pomodoro/PomodoroCompleteScreen.h"
 
// ================================================================
//  PomodoroActiveScreen  —  Running Pomodoro session display
//
//  Layout: Animation fullscreen, HUD overlaid on top.
//
//  ┌──────────────────────────────────────────────────────────┐
//  │  WORK                                    ● ● ○ ○        │  ← y=8  (HUD top bar)
//  │                                                          │
//  │  [animation fills entire 128×64 buffer]                  │
//  │                                                          │
//  │ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ │
//  │ ▓  24:59            [SEL] Pause   [BCK] Abort          ▓ │  ← bottom bar y=54..63
//  │ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ │
//  └──────────────────────────────────────────────────────────┘
//
//  AnimationPlayer Mode 2 (tick + drawFrame):
//    - drawFrame writes XBM into buffer
//    - HUD is drawn on top each frame
//    - clearBuffer + sendBuffer owned by this screen
//
//  Phase transitions (WORK → BREAK → WORK → ... → DONE):
//    - Screen subscribes to PomodoroEngine state changes
//    - Detects state change each update() tick
//    - Swaps animation automatically on phase change
//    - When DONE → pushes PomodoroCompleteScreen
//
//  Buttons:
//    SELECT      → pause / resume toggle
//    BACK        → abort (direct, no confirm dialog at v1)
// ================================================================

class PomodoroActiveScreen : public IScreen {
    public:
    PomodoroActiveScreen(DisplayManager& display,
                         UIManager& ui,
                         PomodoroEngine& engine
    ): _display(display), _ui(ui), _engine(engine) {}

    void onEnter() override {
        _wantsPop       = false;
        _lastState      = _engine.state();
        _completePushed = false;
        _loadAnimationForState(_engine.state());
        _player.reset();
        _dirty          = true;
    }

    void onExit() override {
        // If we're leaving because the session was aborted via BACK,
        // the engine.abort() already published PomodoroAborted.
    }

    bool needsContinuousUpdate() const override { return true; }
    bool wantsPop()              const override { return _wantsPop; }

    // ── Buttons ───────────────────────────────────────────────

    void onButtonSelect() override {
        if(_engine.isPaused()){
            _engine.resume();
        } else if(_engine.isActive()) {
            _engine.pause();
        }

        _dirty = true; // to update HUD immediately on pause/resume
    }

    void onButtonBack() override {
        _engine.abort();
        _wantsPop = true;
    }


    void update() override {
        PomodoroState currState = _engine.state();

        // ── Detect phase transition ────────────────────────────
        if(currState != _lastState) {
            _onStateChanged(currState);
            _lastState = currState;
        }

        // ── Session complete → push complete screen once ────────
        if(currState == PomodoroState::DONE && !_completePushed) {
            _completePushed = true;
           _ui.pushScreen(new PomodoroCompleteScreen(_display, _ui, _engine));
           wantsPop = true;
           return; 
        }

        // ── Advance animation frame ────────────────────────────
        bool advanced = false;
        if(!_engine.isPaused()){
            advanced = _player.tick();
        }

        if(!advanced && !_dirty) return;
        _dirty = false;

        // ── Draw ───────────────────────────────────────────────
        auto& u = _display.raw();
        u.clearBuffer();
 
        // 1. Animation fullscreen (Mode 2b)
        if (_player.isPlaying()) {
            _player.drawFrame(_display);
        }
 
        // 2. HUD overlay on top
        _drawHUD(u);
 
        u.sendBuffer();
    }


    private:
        DisplayManager&  _display;
        UIManager&       _ui;
        PomodoroEngine&  _engine;
        AnimationPlayer  _player;

        PomodoroState    _lastState         = PomodoroState::IDLE;
        bool             _wantsPop          = false;
        bool             _completePushed    = false;

        // ── Load animation for a given engine state ────────────────
        void _loadAnimationForState(PomodoroState state) {
            int vibeIdx = 0;

            if(state == PomodoroState::WORK) {
                vibeIdx = AshuraPrefs::getPomodoroWorkVibe();
            } else if(state == PomodoroState::SHORT_BREAK || state == PomodoroState::LONG_BREAK){
                vibeIdx = AshuraPrefs::getPomodoroBreakVibe();
            } else if(state == PomodoroState::PAUSED) {
                // Keep current animation — don't swap on pause
                return;
            }

            // Clamp to valid range
            if (vibeIdx < 0 || vibeIdx >= VIBE_COUNT) vibeIdx = 0;
    
            _player.setAnimation(ALL_VIBES[vibeIdx].animation);
            _player.reset();
        }

        void _onStateChanged(PomodoroState newState) {
            if (newState == PomodoroState::DONE) return; // handled above
            if (newState == PomodoroState::PAUSED) return; // keep current anim
    
            _loadAnimationForState(newState);
            _dirty = true;
        }

        // ── HUD overlay ───────────────────────────────────────────
        void _drawHUD(U8G2& u) {
            // ── Top bar (semi-transparent: phase label + session dots) ──
            // Draw a thin solid strip at top so text is always readable
            u.setDrawColor(0);
            u.drawBox(0, 0, 128, 11);
            u.setDrawColor(1);
            u.drawHLine(0, 10, 128);
    
            u.setFont(u8g2_font_5x7_tr);
            u.setFontMode(1);
    
            // Phase label — top left
            const char* label = _engine.phaseLabel();
            u.drawStr(3, 8, label);
    
            // Session dots — top right
            _drawSessionDots(u);
    
            u.setFontMode(0);
    
            // ── Bottom bar (solid background strip) ────────────────
            u.setDrawColor(1);
            u.drawBox(0, 53, 128, 11);
            u.setDrawColor(0);
            u.setFontMode(1);
            u.setFont(u8g2_font_5x7_tr);
    
            // Big countdown — left side of bottom bar
            char timeBuf[8];
            _formatRemaining(timeBuf, sizeof(timeBuf));
            u.drawStr(3, 62, timeBuf);
    
            // Hint — right side
            const char* hint = _engine.isPaused() ? "[SEL] Resume" : "[SEL] Pause";
            int hw = u.getStrWidth(hint);
            u.drawStr(126 - hw, 62, hint);
    
            u.setDrawColor(1);
            u.setFontMode(0);
        }
    
        void _drawSessionDots(U8G2& u) {
            int total   = _engine.sessionsGoal();
            int done    = _engine.sessionsDone();
            int dotSize = 3;
            int spacing = 8;
            int startX  = 125 - (total - 1) * spacing;
    
            for (int i = 0; i < total; i++) {
                int x = startX + i * spacing;
                if (i < done) {
                    u.drawDisc(x, 5, dotSize);   // filled = completed
                } else {
                    u.drawCircle(x, 5, dotSize); // hollow = remaining
                }
            }
        }
    
        void _formatRemaining(char* buf, size_t len) {
            uint32_t rem = _engine.remaining();
            uint32_t mm  = rem / 60000UL;
            uint32_t ss  = (rem % 60000UL) / 1000UL;
            snprintf(buf, len, "%02lu:%02lu", (unsigned long)mm, (unsigned long)ss);
        }
}