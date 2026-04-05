#pragma once

#include <cstdint>
#include "esp_log.h"

#include "Animation.h"
#include "../../core/DisplayManager.h"
#include "hal.h"

// ============================================================
//  AnimationPlayer  —  Shared PROGMEM frame driver
//
//  Used by VibePlayerScreen and VibePreviewScreen.
//  Handles all frame timing and PROGMEM reads in one place.
//
//  Two draw modes:
//
//  1. update(display)
//     Full autonomous render — clears buffer, draws frame, sends.
//     Use this when nothing else is drawn on top (VibePlayerScreen).
//
//  2. tick()  +  drawFrame(display)
//     Separated — tick() advances the frame counter on schedule,
//     drawFrame() writes the XBM into the current buffer only.
//     The caller controls clearBuffer() and sendBuffer().
//     Use this when you need to draw UI on top (VibePreviewScreen).
//
//  ESP32 flash notes:
//    const data is placed in flash automatically by the linker.
//    No PROGMEM macro needed — it was a no-op on ESP32 anyway.
//    pgm_read_ptr() is also gone — direct pointer access works fine
//    because ESP32 has a memory-mapped flash cache (IRAM/DRAM).
// ============================================================

class AnimationPlayer {
    public:
        AnimationPlayer() = default;

        void setAnimation(const Animation* animation) {
            _animation  = animation;
            _frame      = 0;
            _lastTick   = 0;
            if (animation) ESP_LOGI("AnimPlayer", "> %s", animation->name);
        }

        void reset() {
            _frame      = 0;
            _lastTick   = millis();
        }

        // ── Mode 1: fully autonomous ─────────────────────────────
        // Advances frame + clears buffer + draws + sends. One call does all.
        // Returns true if a new frame was drawn this tick.
        
        bool update(DisplayManager& display) {
            if(!_animation) return false;
            bool advanced = _advance();
            auto& u = display.raw();
            u.clearBuffer();
            _drawCurrentFrame(u);
            u.sendBuffer();
            return advanced;
        }

        // ── Mode 2a: advance frame counter only ──────────────────
        // Returns true if the frame changed — caller should redraw.
        bool tick(){
            if(!_animation) return false;
            return _advance();
        }

        // ── Mode 2b: draw current frame into buffer only ─────────
        // Does NOT clear or send — caller owns the buffer lifecycle.
        // Call after clearBuffer(), call sendBuffer() after your overlays.
        void drawFrame(DisplayManager& display) {
            if (!_animation) return;
            _drawCurrentFrame(display.raw());
        }

        // ── Accessors ─────────────────────────────────────────────
        bool                isPlaying()        const { return _animation != nullptr; }
        int                 currentFrame()  const { return _frame; }
        const Animation*    anim()          const { return _animation; }



    private:
        const Animation*    _animation  = nullptr;
        int                 _frame      = 0;
        uint64_t            _lastTick   = 0;

        bool _advance() {
            uint64_t now = millis();
            if(now - _lastTick >= (uint64_t)_animation->frameDelayMs){
                _lastTick = now;
                _frame = (_frame+1) % _animation->frameCount;
                return true;
            }
            return false;
        }

        void _drawCurrentFrame(U8G2& u) {
            const unsigned char* frame = _animation->frames[_frame];
            u.drawBitmap(0, 0, 16, 64, frame);
        }
};