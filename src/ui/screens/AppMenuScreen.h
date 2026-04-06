#pragma once
 
// ================================================================
//  AppMenuScreen  —  horizontal carousel menu
//
//  128×64 OLED layout:
//
//  ┌────────────────────────────────────────────────────────────┐
//  │  )       [48×48 animated icon — owns the screen]       (  │
//  │  •                                                      •  │
//  │  ●              App Name (tight below icon)             ●  │
//  │  •                                                      •  │
//  └────────────────────────────────────────────────────────────┘
//
//  Center slot:
//    - 48×48 pixel bitmap animation (AppIconAnim frames)
//    - Falls back to 14×14 XBM scaled/centred if no anim
//    - Label rendered tight below icon, centered
//
//  Side dot indicators (left & right, mirrored  )( shape):
//    - Up to DOTS_MAX dots per side, vertically centred
//    - Outward bow: center dot pushed toward screen edge
//    - Selected = large filled disc, others = small hollow circle
//    - No arc line — dots only
//    - Columns sit close enough inward to never clip screen edge
//
//  Navigation:
//    UP     → previous app (wraps)
//    DOWN   → next app (wraps)
//    SELECT → launch selected app
// ================================================================

#include "IScreen.h"
#include "../../core/DisplayManager.h"
#include "config.h"
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include "../assets/icons/AppIcon.h"
#include "hal.h"


// ── Animated icon descriptor ─────────────────────────────────
struct AppIconAnim {
    const uint8_t (*frames)[288];  // pointer to 2D array in flash
    int frameCount;
    int frameDelayMs;
};

// ── App descriptor ────────────────────────────────────────────
struct AppItem {
    std::string           icon;         // key for 14×14 XBM (flanking + fallback)
    std::string           name;         // display label
    const AppIconAnim*          anim;         // nullptr → static XBM fallback
    std::function<void()> onLaunch;
};

// ── Layout constants ──────────────────────────────────────────

// ── Center icon layout ────────────────────────────────────────
static constexpr int CEN_W    = 48;
static constexpr int CEN_H    = 48;
static constexpr int CEN_X    = (128 - CEN_W) / 2;  // = 40
static constexpr int CEN_Y    = 2;                   // tight top margin
 
// Fallback small icon
static constexpr int SM_W     = 14;
static constexpr int SM_H     = 14;
 
// Label: sits just below icon with a small gap
static constexpr int LABEL_Y  = CEN_Y + CEN_H + 10; // = 60
 
// ── Side dot indicator constants ──────────────────────────────
static constexpr int DOTS_MAX    = 5;
 
static constexpr int DOT_R_SEL  = 4;   // selected: large filled disc
static constexpr int DOT_R      = 2;   // others:   small hollow circle
 
static constexpr int DOT_SPACING = 12; // vertical gap between dot centres
 
// Arc bow: how many px the center dot is pushed outward from base
static constexpr int ARC_BOW    = 5;
 
// Base x for each column — inset enough that even with ARC_BOW
// the outermost dot pixel stays on screen (DOT_R_SEL=4, so need ≥4px margin)
// Left:  base=18, bow pushes to 18-5=13 → leftmost pixel at 13-4=9  ✓
// Right: base=110, bow pushes to 110+5=115 → rightmost pixel at 115+4=119 ✓
static constexpr int LEFT_BASE_X  = 18;
static constexpr int RIGHT_BASE_X = 110;
 
// Vertical centre of dot columns (screen centre)
static constexpr int DOT_CY = 32;

class AppMenuScreen : public IScreen {
public:
    AppMenuScreen(DisplayManager& display, std::vector<AppItem> apps)
        : _display(display), _apps(std::move(apps)) {
            _frame = 0;
            _lastFrameTick = 0;
        }

    void onEnter() override {
        _cursor = 0;
        _frame  = 0;
        _lastFrameTick = millis();
        _dirty  = true;
    }

    bool needsContinuousUpdate() const override { return true; }

    void onButtonUp() override {
        int n = (int)_apps.size();
        _cursor = (_cursor - 1 + n) % n;
        _frame  = 0; // reset anim to first frame on app change
        _lastFrameTick = millis();
        _dirty  = true;
    }

    void onButtonDown() override {
        int n = (int)_apps.size();
        _cursor = (_cursor + 1) % n;
        _frame         = 0;
        _lastFrameTick = millis();
        _dirty         = true;
    }

    void onButtonSelect() override {
        if (_cursor < (int)_apps.size() && _apps[_cursor].onLaunch)
            _apps[_cursor].onLaunch();
    }

    void update() override {
        const AppItem& cur = _apps[_cursor];
 
        // Tick animation
        if (cur.anim && cur.anim->frameCount > 1) {
            uint64_t now = millis();
            if (now - _lastFrameTick >= (uint64_t)cur.anim->frameDelayMs) {
                _lastFrameTick = now;
                _frame = (_frame + 1) % cur.anim->frameCount;
                _dirty = true;
            }
        }
 
        if (!_dirty) return;
        _dirty = false;
 
        int n = (int)_apps.size();
        if (n == 0) return;
 
        auto& u = _display.raw();
        u.clearBuffer();
 
        // ── 1. Center icon ─────────────────────────────────────
        if (cur.anim && cur.anim->frames) {
            // 48×48 bitmap: wBytes = 48/8 = 6
            u.drawBitmap(CEN_X, CEN_Y, 6, CEN_H, cur.anim->frames[_frame]);
        } else {
            // Fallback: 14×14 XBM centred in 48×48 slot
            int fbX = CEN_X + (CEN_W - SM_W) / 2;
            int fbY = CEN_Y + (CEN_H - SM_H) / 2;
            u.drawXBM(fbX, fbY, SM_W, SM_H, _appIcon(cur.icon));
        }
 
        // ── 2. Label ───────────────────────────────────────────
        u.setFont(u8g2_font_6x10_tr);
        int lw = u.getStrWidth(cur.name.c_str());
        u.drawStr((128 - lw) / 2, LABEL_Y, cur.name.c_str());
 
        // ── 3. Side dot indicators ─────────────────────────────
        _drawSideDots(u, n);
 
        u.sendBuffer();
    }

private:
    DisplayManager&      _display;
    std::vector<AppItem> _apps;
    int                  _cursor = 0;
    int                  _frame  = 0;
    uint64_t             _lastFrameTick = 0;

    // ── Dot indicator ─────────────────────────────────────────
    // Quadratic arc offset — no floats, no math.h
    // Returns outward bow offset in px for dot i of n.
    // Center dot gets ARC_BOW, end dots get 0.
    int _arcOffset(int i, int n) const {
        if (n <= 1) return 0;
        int t100 = (200 * i / (n - 1)) - 100; // maps i → [-100, 100]
        int t2   = t100 * t100;                // t² × 10000
        return (ARC_BOW * (10000 - t2)) / 10000;
    }
 
    void _drawSideDots(U8G2& u, int n) {
        if (n <= 1) return;
 
        int visible = std::min(n, DOTS_MAX);
 
        // Sliding window centred on _cursor
        int start = 0;
        if (n > visible) {
            start = _cursor - visible / 2;
            start = std::max(0, std::min(start, n - visible));
        }
 
        int totalH = (visible - 1) * DOT_SPACING;
        int topY   = DOT_CY - totalH / 2;
 
        for (int i = 0; i < visible; i++) {
            int appIdx = start + i;
            int bow    = _arcOffset(i, visible);
            int lx     = LEFT_BASE_X  - bow;  // left: bow pushes left (outward)
            int rx     = RIGHT_BASE_X + bow;  // right: bow pushes right (outward)
            int y      = topY + i * DOT_SPACING;
 
            if (appIdx == _cursor) {
                u.drawDisc(lx, y, DOT_R_SEL);
                u.drawDisc(rx, y, DOT_R_SEL);
            } else {
                u.drawCircle(lx, y, DOT_R);
                u.drawCircle(rx, y, DOT_R);
            }
        }
    }
};