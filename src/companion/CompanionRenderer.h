#pragma once

#include <cmath>
#include <cstdlib>  // esp_random() replacement
#include <algorithm>

#include <U8g2lib.h>
#include "../companion/EyeParams.h"
#include "MoodEngine.h"

#include "hal.h"

// ================================================================
//  CompanionRenderer  —  draws the companion's eyes anywhere
//
//  Resolution-independent. Give any (rx, ry, rw, rh) region:
//
//    companion.draw(u, 0, 38, 48, 26)   → small HomeScreen corner
//    companion.draw(u, 64, 4, 60, 52)   → right half Pomodoro
//    companion.draw(u, 0,  0, 128, 64)  → full screen
//
//  Layers (drawn in this order every frame):
//    1. Mood lerp  — current EyeParams drifts toward MoodEngine target
//    2. Blink      — separate lid override, state machine, random timing
//    3. Micro      — small random glances/wide eyes every few seconds
//    4. Draw       — two eyes rendered from current params using U8G2
//
//  Call begin() once after construction.
//  Call update(millis()) every loop().
//  Call draw(u8g2, x, y, w, h) inside any screen's update().
// ================================================================

class CompanionRenderer {
public:
    explicit CompanionRenderer(MoodEngine& mood) : _mood(mood) {}

    void begin() {
        _lastUpdate = millis();
        _scheduleNextBlink();
        _scheduleNextMicro();
    }

    // ── Call every loop() ─────────────────────────────────────
    void update(uint64_t now) {
        float dt = (now - _lastUpdate) / 1000.0f;
        _lastUpdate = now;
        if (dt <= 0.0f || dt > 0.15f) dt = 0.016f;  // clamp stutter

        // 1. Lerp current toward mood target
        _lerpTo(_cur, _mood.getTarget(), 7.0f * dt);

        // 2. Blink
        _tickBlink(now);

        // 3. Micro-behaviours
        _tickMicro(now);
    }

    // ── Draw into any screen region ───────────────────────────
    void draw(U8G2& u, int rx, int ry, int rw, int rh) {
        // Eye boxes: 30% of region width, 56% of region height each
        // Larger than before — rounded rects need more pixels to look good
        int eW = std::max(8,  (int)(rw * 0.30f));
        int eH = std::max(6,  (int)(rh * 0.56f));

        // Apply squish from params
        int sW = std::max(6, (int)(eW * _cur.squishX));
        int sH = std::max(4, (int)(eH * _cur.squishY));

        // Corner radius — scales with eye size, capped so it stays rounded
        int cr = std::max(2, std::min(sW, sH) / 3);

        // Eye centers: left=27% right=73% across region, vertically centred
        int cy  = ry + rh / 2;
        int lcx = rx + (int)(rw * 0.27f);
        int rcx = rx + (int)(rw * 0.73f);

        // Blink overrides mood lid — use whichever is higher
        float topLid = (_blinkLid > _cur.lidTop) ? _blinkLid : _cur.lidTop;

        _drawEye(u, lcx, cy, sW, sH, cr, topLid, _cur.lidBottom,
                 _cur.pupilSize, _cur.pupilX, _cur.pupilY);
        _drawEye(u, rcx, cy, sW, sH, cr, topLid, _cur.lidBottom,
                 _cur.pupilSize, -_cur.pupilX, _cur.pupilY);  // mirror gaze X
    }

private:
    MoodEngine& _mood;
    EyeParams   _cur;               // current (interpolated) params
    uint64_t _lastUpdate = 0;

    // ── Lerp helpers ─────────────────────────────────────────
    static float _lF(float a, float b, float t) { return a + (b - a) * t; }

    void _lerpTo(EyeParams& c, const EyeParams& tgt, float spd) {
        c.lidTop    = _lF(c.lidTop,    tgt.lidTop,    spd);
        c.lidBottom = _lF(c.lidBottom, tgt.lidBottom, spd);
        c.pupilSize = _lF(c.pupilSize, tgt.pupilSize, spd);
        c.pupilX    = _lF(c.pupilX,    tgt.pupilX,    spd);
        c.pupilY    = _lF(c.pupilY,    tgt.pupilY,    spd);
        c.squishX   = _lF(c.squishX,   tgt.squishX,   spd);
        c.squishY   = _lF(c.squishY,   tgt.squishY,   spd);
    }

    // ── Blink state machine ───────────────────────────────────
    enum class BlinkSt { OPEN, CLOSING, CLOSED, OPENING };
    BlinkSt       _blinkSt    = BlinkSt::OPEN;
    float         _blinkLid   = 0.0f;
    uint64_t _blinkStart = 0;
    uint64_t _nextBlink  = 0;

    void _scheduleNextBlink() {
        _nextBlink = millis() + 2500 + _random(3000);
    }

    void _tickBlink(uint64_t now) {
        switch (_blinkSt) {
            case BlinkSt::OPEN:
                if (now >= _nextBlink) {
                    _blinkSt    = BlinkSt::CLOSING;
                    _blinkStart = now;
                }
                break;

            case BlinkSt::CLOSING:
                _blinkLid = (float)(now - _blinkStart) / 70.0f;
                if (_blinkLid >= 1.0f) {
                    _blinkLid   = 1.0f;
                    _blinkSt    = BlinkSt::CLOSED;
                    _blinkStart = now;
                }
                break;

            case BlinkSt::CLOSED:
                if (now - _blinkStart >= 40) {
                    _blinkSt    = BlinkSt::OPENING;
                    _blinkStart = now;
                }
                break;

            case BlinkSt::OPENING:
                _blinkLid = 1.0f - (float)(now - _blinkStart) / 90.0f;
                if (_blinkLid <= 0.0f) {
                    _blinkLid = 0.0f;
                    _blinkSt  = BlinkSt::OPEN;
                    _scheduleNextBlink();
                }
                break;
        }
    }

    // ── Micro-behaviours ─────────────────────────────────────
    // Small random movements that fire every 2–7s
    enum class MicroT { NONE, GLANCE_L, GLANCE_R, LOOK_UP, WIDE };
    MicroT        _micro      = MicroT::NONE;
    uint64_t _microStart = 0;
    uint64_t _nextMicro  = 0;

    void _scheduleNextMicro() {
        _nextMicro = millis() + 2000 + _random(5000);
    }

    void _tickMicro(uint64_t now) {
        if (_micro == MicroT::NONE) {
            if (now >= _nextMicro) {
                switch (_random(4)) {
                    case 0: _micro = MicroT::GLANCE_L; break;
                    case 1: _micro = MicroT::GLANCE_R; break;
                    case 2: _micro = MicroT::LOOK_UP;  break;
                    case 3: _micro = MicroT::WIDE;     break;
                }
                _microStart = now;
            }
            return;
        }

        // Sinusoidal arc — goes and comes back over MICRO_DUR ms
        const float MICRO_DUR = 700.0f;
        float t = (now - _microStart) / MICRO_DUR;
        if (t >= 1.0f) {
            _micro = MicroT::NONE;
            _scheduleNextMicro();
            return;
        }
        float s = sinf(t * 3.14159f);   // 0→1→0 arc

        switch (_micro) {
            case MicroT::GLANCE_L: _cur.pupilX = -0.65f * s; break;
            case MicroT::GLANCE_R: _cur.pupilX =  0.65f * s; break;
            case MicroT::LOOK_UP:  _cur.pupilY = -0.45f * s; break;
            case MicroT::WIDE:
                _cur.squishX = 1.0f + 0.28f * s;
                _cur.squishY = 1.0f + 0.28f * s;
                break;
            default: break;
        }
    }

    // ── Draw one eye (rounded-rect style) ────────────────────
    //
    //  Approach:
    //    1. Filled white rounded rect  — the sclera
    //    2. Black erase rect from top  — top lid cutoff
    //    3. Black erase rect from bot  — bottom lid cutoff
    //    4. Redraw rounded border      — keeps corners crisp
    //    5. Filled black disc          — pupil
    //
    //  cx,cy  — eye centre
    //  eW,eH  — eye box dimensions (squish already applied)
    //  cr     — corner radius
    //  topLid — 0.0 fully open → 1.0 closed from top
    //  botLid — 0.0 fully open → 1.0 closed from bottom
    //  pSize  — pupil size (normalised to eH)
    //  pX,pY  — pupil offset (−1..+1 mapped to box interior)
    void _drawEye(U8G2& u,
                  int cx, int cy,
                  int eW, int eH, int cr,
                  float topLid, float botLid,
                  float pSize, float pX, float pY)
    {
        int ex = cx - eW / 2;
        int ey = cy - eH / 2;

        // ── 1. Filled white sclera ────────────────────────────
        u.setDrawColor(1);
        u.drawRBox(ex, ey, eW, eH, cr);

        // ── 2. Erase top lid ──────────────────────────────────
        if (topLid > 0.01f) {
            int lidH = (int)(topLid * eH) + 1;
            u.setDrawColor(0);
            u.drawBox(ex, ey, eW, lidH);
            u.setDrawColor(1);
        }

        // ── 3. Erase bottom lid ───────────────────────────────
        if (botLid > 0.01f) {
            int lidH = (int)(botLid * eH) + 1;
            u.setDrawColor(0);
            u.drawBox(ex, ey + eH - lidH, eW, lidH + 1);
            u.setDrawColor(1);
        }

        // ── 4. Redraw border (erase rects cut into corners) ───
        u.drawRFrame(ex, ey, eW, eH, cr);

        // ── 5. Pupil ──────────────────────────────────────────
        float effectiveLid = (topLid > botLid) ? topLid : botLid;
        float openFrac     = 1.0f - effectiveLid;
        int   pr           = std::max(2, (int)(pSize * eH * 0.36f));

        int maxPX = eW / 2 - pr - 1;
        int maxPY = (int)((eH / 2 - pr - 1) * openFrac);

        int px = cx + (int)(pX * maxPX);
        int py = cy + (int)(pY * maxPY);

        px = std::clamp(px, cx - maxPX, cx + maxPX);
        py = std::clamp(py, cy - maxPY, cy + maxPY);

        u.setDrawColor(1);
        u.drawDisc(px, py, pr);
    }
};