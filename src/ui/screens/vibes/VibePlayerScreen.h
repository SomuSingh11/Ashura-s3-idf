#pragma once

#include "../IScreen.h"
#include "../../../core/DisplayManager.h"
#include "../../../application/vibes/AnimationPlayer.h"

// ================================================================
//  VibePlayerScreen  —  Fullscreen animation loop (screensaver)
//
//  Plays a PROGMEM animation indefinitely using AnimationPlayer.
//  No confirm bar, no save logic — just plays.
//  Used by AshuraCore::_launchScreenSaver() and _bootUI().
//
//  Any button press → wantsPop = true → UIManager pops it.
//
//  Uses AnimationPlayer::update() — Mode 1 (fully autonomous).
//  Player owns clearBuffer + drawXBM + sendBuffer entirely.
// ================================================================

class VibePlayerScreen : public IScreen {
    public:
        VibePlayerScreen(DisplayManager& display, const Animation* animation) : _display(display) {
            _player.setAnimation(animation);
        }

        void onEnter() override {
            _player.reset();
            _wantsPop = false;
        }

        bool needsContinuousUpdate() const override { return true; }
        bool wantsPop()              const override { return _wantsPop; }

        void onButtonUp()       override { _wantsPop = true; }
        void onButtonDown()     override { _wantsPop = true; }
        void onButtonSelect() override {
    ESP_LOGW("BTN", "SELECT PRESSED");
    _wantsPop = true;
}
        void onButtonBack()     override { _wantsPop = true; }

        void update() override {
            if(!_player.isPlaying()) { _wantsPop = true; return; }
            _player.update(_display); // Mode 1
        }
    private:
        DisplayManager&     _display;   // screen does NOT own the display, hence "&"
        AnimationPlayer     _player;    // VibePlayerScreen OWNS the player
        bool                _wantsPop = false;
        bool                _autoExit = false;
};