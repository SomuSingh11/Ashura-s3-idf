#pragma once

#include "../IScreen.h"
#include "../../../core/DisplayManager.h"
#include "../../../core/TimeManager.h"
#include "hal.h"
#include <math.h>

class ClockFaceScreen : public IScreen {
public:
    explicit ClockFaceScreen(DisplayManager& d) : _display(d) {}

    void onEnter() override {
        _lastSS   = -1;   // force immediate draw
        _wantsPop = false;
        _dirty    = true;
    }

    bool needsContinuousUpdate() const override { return true; }

    void onButtonBack()   override { _wantsPop = true; }
    void onButtonSelect() override { _wantsPop = true; }
    void onButtonUp()     override { _wantsPop = true; }
    void onButtonDown()   override { _wantsPop = true; }
    bool wantsPop()  const override { return _wantsPop; }

    // ── Update ────────────────────────────────────────────────
    void update() override {
        int ss = Time().getSS();
        int mm = Time().getMM();
        int hh = Time().getHH();

        // Only redraw once per second
        if (ss == _lastSS && !_dirty) return;
        _lastSS = ss;
        _dirty  = false;

        auto& u = _display.raw();
        u.clearBuffer();

        _drawAnalog(u, hh, mm, ss);
        _drawDivider(u);
        _drawHeader(u);
        _drawBigTime(u, hh, mm);
        _drawSeconds(u, ss);
        _drawFooter(u);

        u.sendBuffer();
    }

private:
    DisplayManager& _display;
    bool            _wantsPop = false;
    int             _lastSS   = -1;

    // ── Analog constants ─────────────────────────────────────
    static constexpr int   CX = 30;      // clock centre x
    static constexpr int   CY = 32;      // clock centre y
    static constexpr float R  = 29.0f;   // clock radius

    // Degree → radians
    static constexpr float DEG = 0.0174533f;

    // ── Analog clock (ported faithfully from oled_analog_clock.ino) ──
    void _drawAnalog(U8G2& u, int hh, int mm, int ss) {

        // Outer circle
        u.drawCircle(CX, CY, (int)R);

        // Centre dot
        u.drawDisc(CX, CY, 2);

        // ── 60 minute ticks (single pixels on the rim) ────────
        for (int j = 1; j <= 60; j++) {
            float a  = j * 6 * DEG;
            int   tx = CX + (int)(sinf(a) * R);
            int   ty = CY - (int)(cosf(a) * R);
            u.drawPixel(tx, ty);
        }

        // ── 12 hour ticks + digits ─────────────────────────────
        // Original number array: {"6","5","4","3","2","1","12","11","10","9","8","7"}
        // j=0 → angle=0 → top of clock → that maps to "6" in the original
        // (the original used sin for x and +cos for y, which is rotated 180°)
        // We use the correct clock convention: sin for x, -cos for y (12 at top)
        // So we reverse the digit mapping:
        static const char* digits[12] = {
            "12","1","2","3","4","5","6","7","8","9","10","11"
        };

        u8g2_font_chikita_tr;   // declare we'll use it below

        for (int j = 0; j < 12; j++) {
            float a = j * 30 * DEG;

            // Outer tick point (on rim)
            int ox = CX + (int)(sinf(a) * R);
            int oy = CY - (int)(cosf(a) * R);

            // Inner tick point (4px inward)
            int ix = CX + (int)(sinf(a) * (R - 4));
            int iy = CY - (int)(cosf(a) * (R - 4));

            u.drawLine(ox, oy, ix, iy);

            // Digit position (8px inward from rim)
            int dx = CX + (int)(sinf(a) * (R - 9));
            int dy = CY - (int)(cosf(a) * (R - 9));

            u.setFont(u8g2_font_chikita_tr);    // 3px tall — tiny, perfect for clock face
            // Centre the string: offset by ~2px left, 2px down for baseline
            int strW = u.getStrWidth(digits[j]);
            u.drawStr(dx - strW / 2, dy + 2, digits[j]);
        }

        // ── Second hand (thin, full length) ───────────────────
        {
            float a  = ss * 6 * DEG;
            int   ex = CX + (int)(sinf(a) * (R - 1));
            int   ey = CY - (int)(cosf(a) * (R - 1));
            u.drawLine(CX, CY, ex, ey);
        }

        // ── Minute hand (radius - 8) ──────────────────────────
        {
            float a  = (mm + ss / 60.0f) * 6 * DEG;
            int   ex = CX + (int)(sinf(a) * (R - 8));
            int   ey = CY - (int)(cosf(a) * (R - 8));
            u.drawLine(CX,     CY,     ex, ey);
            u.drawLine(CX + 1, CY,     ex, ey);   // slightly thick
        }

        // ── Hour hand (radius / 2, thick) ────────────────────
        {
            float a  = (hh % 12 * 30 + mm * 0.5f) * DEG;
            int   ex = CX + (int)(sinf(a) * (R / 2));
            int   ey = CY - (int)(cosf(a) * (R / 2));
            u.drawLine(CX,     CY,     ex, ey);
            u.drawLine(CX + 1, CY,     ex, ey);   // thick
            u.drawLine(CX,     CY + 1, ex, ey);   // thick
        }

        // Redraw centre dot on top of hands
        u.drawDisc(CX, CY, 2);
    }

    // ── Vertical divider ─────────────────────────────────────
    void _drawDivider(U8G2& u) {
        // Dotted vertical line at x=60
        for (int y = 3; y < 62; y += 3) u.drawPixel(62, y);
    }

    // ── header (right panel top) ────────────────────────
    void _drawHeader(U8G2& u) {
        
        u.setFont(u8g2_font_5x7_tr);
        const char* label = Time().isSynced() ? "Synced" : "Up-time";
        int w = u.getStrWidth(label);
        u.drawStr(76 + (66 - w) / 2, 8, label);

        // Thin divider under date
        u.drawLine(65, 10, 127, 10);
    }

    // ── Big HH:MM ────────────────────────────────────────────
    void _drawBigTime(U8G2& u, int hh, int mm) {
        char buf[6];
        snprintf(buf, sizeof(buf), "%02d:%02d", hh, mm);

        u.setFont(u8g2_font_ncenB14_tr);   // large bold ~14px tall
        int w = u.getStrWidth(buf);
        // Centre in right panel horizontally, vertically around y=36
        u.drawStr(62 + (66 - w) / 2, 32, buf);
    }

    // ── Medium :SS + progress bar ────────────────────────────
    void _drawSeconds(U8G2& u, int ss) {
        char buf[4];
        snprintf(buf, sizeof(buf), ":%02d", ss);

        // Medium font — 6x10
        u.setFont(u8g2_font_6x10_tr);
        int w = u.getStrWidth(buf);
        u.drawStr(69 + (66 - w) / 2, 44, buf);

        // Thin seconds progress bar below :SS
        // Full width of right panel = 64px
        const int BX = 65, BY = 51, BW = 63, BH = 2;
        u.drawFrame(BX, BY, BW, BH);
        int fill = (ss * (BW - 2)) / 60;
        if (fill > 0) u.drawBox(BX + 1, BY, fill, BH);
    }

    // ── Footer: sync status ───────────────────────────────────
    void _drawFooter(U8G2& u) {
        static const char* DAY[] = {
            "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
        };
        static const char* MON[] = {
            "Jan","Feb","Mar","Apr","May","Jun",
            "Jul","Aug","Sep","Oct","Nov","Dec"
        };

        u.setFont(u8g2_font_5x7_tr);

        if (Time().isSynced()) {
            struct tm t;
            if (getLocalTime(&t, 0)) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%s  %d%s",
                         DAY[t.tm_wday], t.tm_mday, MON[t.tm_mon]);
                // Centre in right panel (x=62..128 = 66px wide)
                int w = u.getStrWidth(buf);
                u.drawStr(62 + (66 - w) / 2, 62, buf);
            }
        } else {
            u.drawStr(68, 8, "No date");
        }
    }
};