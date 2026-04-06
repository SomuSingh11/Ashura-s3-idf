#pragma once
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

#include <cmath>
#include <algorithm>
#include <cstdint>

#include "u8g2.h"
#include "esp_random.h"

#include "../companion/EyeParams.h"
#include "MoodEngine.h"
#include "hal.h"

class CompanionRenderer {
public:
    explicit CompanionRenderer(MoodEngine& mood) : _mood(mood) {}

    void begin() {
        _lastUpdate = millis();
        _scheduleNextBlink();
        _scheduleNextMicro();
    }

    void update(uint64_t now) {
        float dt = (now - _lastUpdate) / 1000.0f;
        _lastUpdate = now;
        if (dt <= 0.0f || dt > 0.15f) dt = 0.016f;

        _lerpTo(_cur, _mood.getTarget(), 7.0f * dt);
        _tickBlink(now);
        _tickMicro(now);
    }

    // Pure C API handle
    void draw(u8g2_t* u, int rx, int ry, int rw, int rh) {
        int eW = std::max(8, static_cast<int>(rw * 0.30f));
        int eH = std::max(6, static_cast<int>(rh * 0.56f));

        int sW = std::max(6, static_cast<int>(eW * _cur.squishX));
        int sH = std::max(4, static_cast<int>(eH * _cur.squishY));

        int cr = std::max(2, std::min(sW, sH) / 3);

        int cy  = ry + rh / 2;
        int lcx = rx + static_cast<int>(rw * 0.27f);
        int rcx = rx + static_cast<int>(rw * 0.73f);

        float topLid = (_blinkLid > _cur.lidTop) ? _blinkLid : _cur.lidTop;

        _drawEye(u, lcx, cy, sW, sH, cr, topLid, _cur.lidBottom,
                 _cur.pupilSize, _cur.pupilX, _cur.pupilY);
        _drawEye(u, rcx, cy, sW, sH, cr, topLid, _cur.lidBottom,
                 _cur.pupilSize, -_cur.pupilX, _cur.pupilY);
    }

private:
    MoodEngine& _mood;
    EyeParams _cur;
    uint64_t _lastUpdate = 0;

    static float _lF(float a, float b, float t) { return a + (b - a) * t; }

    static uint32_t _randRange(uint32_t maxExclusive) {
        return (maxExclusive == 0) ? 0 : (esp_random() % maxExclusive);
    }

    void _lerpTo(EyeParams& c, const EyeParams& tgt, float spd) {
        c.lidTop    = _lF(c.lidTop,    tgt.lidTop,    spd);
        c.lidBottom = _lF(c.lidBottom, tgt.lidBottom, spd);
        c.pupilSize = _lF(c.pupilSize, tgt.pupilSize, spd);
        c.pupilX    = _lF(c.pupilX,    tgt.pupilX,    spd);
        c.pupilY    = _lF(c.pupilY,    tgt.pupilY,    spd);
        c.squishX   = _lF(c.squishX,   tgt.squishX,   spd);
        c.squishY   = _lF(c.squishY,   tgt.squishY,   spd);
    }

    enum class BlinkSt { OPEN, CLOSING, CLOSED, OPENING };
    BlinkSt _blinkSt = BlinkSt::OPEN;
    float _blinkLid = 0.0f;
    uint64_t _blinkStart = 0;
    uint64_t _nextBlink = 0;

    void _scheduleNextBlink() {
        _nextBlink = millis() + 2500 + _randRange(3000);
    }

    void _tickBlink(uint64_t now) {
        switch (_blinkSt) {
            case BlinkSt::OPEN:
                if (now >= _nextBlink) {
                    _blinkSt = BlinkSt::CLOSING;
                    _blinkStart = now;
                }
                break;

            case BlinkSt::CLOSING:
                _blinkLid = static_cast<float>(now - _blinkStart) / 70.0f;
                if (_blinkLid >= 1.0f) {
                    _blinkLid = 1.0f;
                    _blinkSt = BlinkSt::CLOSED;
                    _blinkStart = now;
                }
                break;

            case BlinkSt::CLOSED:
                if (now - _blinkStart >= 40) {
                    _blinkSt = BlinkSt::OPENING;
                    _blinkStart = now;
                }
                break;

            case BlinkSt::OPENING:
                _blinkLid = 1.0f - static_cast<float>(now - _blinkStart) / 90.0f;
                if (_blinkLid <= 0.0f) {
                    _blinkLid = 0.0f;
                    _blinkSt = BlinkSt::OPEN;
                    _scheduleNextBlink();
                }
                break;
        }
    }

    enum class MicroT { NONE, GLANCE_L, GLANCE_R, LOOK_UP, WIDE };
    MicroT _micro = MicroT::NONE;
    uint64_t _microStart = 0;
    uint64_t _nextMicro = 0;

    void _scheduleNextMicro() {
        _nextMicro = millis() + 2000 + _randRange(5000);
    }

    void _tickMicro(uint64_t now) {
        if (_micro == MicroT::NONE) {
            if (now >= _nextMicro) {
                switch (_randRange(4)) {
                    case 0: _micro = MicroT::GLANCE_L; break;
                    case 1: _micro = MicroT::GLANCE_R; break;
                    case 2: _micro = MicroT::LOOK_UP;  break;
                    default: _micro = MicroT::WIDE;    break;
                }
                _microStart = now;
            }
            return;
        }

        constexpr float PI = 3.14159265f;
        constexpr float MICRO_DUR = 700.0f;
        float t = static_cast<float>(now - _microStart) / MICRO_DUR;

        if (t >= 1.0f) {
            _micro = MicroT::NONE;
            _scheduleNextMicro();
            return;
        }

        float s = sinf(t * PI);

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

    void _drawEye(u8g2_t* u,
                  int cx, int cy,
                  int eW, int eH, int cr,
                  float topLid, float botLid,
                  float pSize, float pX, float pY) {
        int ex = cx - eW / 2;
        int ey = cy - eH / 2;

        u8g2_SetDrawColor(u, 1);
        u8g2_DrawRBox(u, ex, ey, eW, eH, cr);

        if (topLid > 0.01f) {
            int lidH = static_cast<int>(topLid * eH) + 1;
            u8g2_SetDrawColor(u, 0);
            u8g2_DrawBox(u, ex, ey, eW, lidH);
            u8g2_SetDrawColor(u, 1);
        }

        if (botLid > 0.01f) {
            int lidH = static_cast<int>(botLid * eH) + 1;
            u8g2_SetDrawColor(u, 0);
            u8g2_DrawBox(u, ex, ey + eH - lidH, eW, lidH + 1);
            u8g2_SetDrawColor(u, 1);
        }

        u8g2_DrawRFrame(u, ex, ey, eW, eH, cr);

        float effectiveLid = (topLid > botLid) ? topLid : botLid;
        float openFrac = 1.0f - effectiveLid;
        int pr = std::max(2, static_cast<int>(pSize * eH * 0.36f));

        int maxPX = eW / 2 - pr - 1;
        int maxPY = static_cast<int>((eH / 2 - pr - 1) * openFrac);

        int px = cx + static_cast<int>(pX * maxPX);
        int py = cy + static_cast<int>(pY * maxPY);

        px = std::clamp(px, cx - maxPX, cx + maxPX);
        py = std::clamp(py, cy - maxPY, cy + maxPY);

        u8g2_SetDrawColor(u, 1);
        u8g2_DrawDisc(u, px, py, pr, U8G2_DRAW_ALL);
    }
};