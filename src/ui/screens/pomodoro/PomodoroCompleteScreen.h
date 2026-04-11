#pragma once
 
#include "hal.h"
#include "../IScreen.h"
#include "../../../core/DisplayManager.h"
#include "../../../core/UIManager.h"
#include "../../../core/NotificationManager.h"
#include "../../../storage/AshuraPrefs.h"
#include "../../../application/pomodoro/PomodoroEngine.h"
#include "../../../application/vibes/AnimationPlayer.h"
#include "../../../application/vibes/VibeRegistry.h"
 
// ================================================================
//  PomodoroCompleteScreen  —  Session done celebration
//
//  Plays the configured completion vibe for exactly ONE full cycle,
//  then auto-pops. Any button press also pops immediately.
//
//  On enter: pushes a notification to NotifMgr so the HomeScreen
//  badge lights up when the user returns.
//
//  Uses AnimationPlayer Mode 1 (fully autonomous update).
//  The screen tracks whether one full cycle has elapsed by
//  watching the frame counter wrap back to 0.
// ================================================================

class PomodoroCompleteScreen : public IScreen {
    public:
        PomodoroCompleteScreen(DisplayManager& display,
                               UIManager& ui, 
                               PomodoroEngine& engine
                            ) : _display(display), _ui(ui), _engine(engine) {}

        void onEnter() override {
            _wantsPop    = false;
            _cyclesDone  = 0;
            _lastFrame   = 0;
    
            // Load completion vibe
            int vibeIdx = AshuraPrefs::getPomodoroCompleteVibe();
            if (vibeIdx < 0 || vibeIdx >= VIBE_COUNT) vibeIdx = 0;
            _player.setAnimation(ALL_VIBES[vibeIdx].animation);
            _player.reset();
    
            // Push notification — HomeScreen badge will light up
            NotifMgr().push(
                "Pomodoro done!",
                std::to_string(_engine.sessionsDone()) + " sessions completed.",
                NotificationType::MESSAGE,
                "pomodoro"
            );
        }

        bool needsContinuousUpdate() const override { return true; }
        bool wantsPop()              const override { return _wantsPop; }
    
        void onButtonUp()     override { _wantsPop = true; }
        void onButtonDown()   override { _wantsPop = true; }
        void onButtonSelect() override { _wantsPop = true; }
        void onButtonBack()   override { _wantsPop = true; }
    
        void update() override {
            if (!_player.isPlaying()) { _wantsPop = true; return; }
    
            int frameBefore = _player.currentFrame();
    
            // Mode 1 — player owns clear + draw + send
            _player.update(_display);
    
            int frameAfter = _player.currentFrame();
    
            // Detect frame wrap (one full cycle complete)
            if (frameAfter < frameBefore) {
                _cyclesDone++;
                if (_cyclesDone >= 5) {
                    _wantsPop = true;
                }
            }
        }
    private:
        DisplayManager&     _display;
        UIManager&          _ui;
        PomodoroEngine&     _engine;
        AnimationPlayer     _player;

        bool    _wantsPop   = false;
        int     _cyclesDone = 0;
        int     _lastFrame  =  0;
};