#pragma once
#include <string>
#include <functional>
#include <algorithm>
#include "hal.h"
#include "../../ui/screens/IScreen.h"
#include "../../core/DisplayManager.h"

// ================================================================
//  CharPickerWidget  —  QWERTY keyboard for 4-button input
//
//  128×64 layout:
//  ┌──────────────────────────────────────────────────────────────┐
//  │  Title                                          [abc/CAP/123]│  y=8
//  │  typed text_                                                 │  y=16
//  ├──────────────────────────────────────────────────────────────┤  y=17
//  │  q w e r t y u i o p   (letter) / 1 2 3 4 5 6 7 8 9 0 (num)  │  y=28
//  │  a s d f g h j k l     (letter) / - _ . , @ # & ( ) / (num)  │  y=39
//  │  z x c v b n m         (letter) / ! ? + = * % ^ ~ ; : (num)  │  y=50
//  │  [cap][spc][123][.,!][del][        SAVE        ]             │  y=63
//  └──────────────────────────────────────────────────────────────┘
//
//  Controls:
//    UP          → move cursor left within row (wraps)
//    DOWN        → move cursor right within row (wraps)
//    SELECT      → type character / activate special key
//    LONG SELECT → confirm (same as SAVE key)
//    LONG UP     → move cursor up one row
//    LONG DOWN   → move cursor down one row
//    BACK        → delete last character (never pops)
//    LONG BACK   → discard and pop without saving
//
//  Special keys (row 3):
//    [cap/CAP] → toggle caps lock — label shows current state
//    [spc]     → append space
//    [123/ABC] → toggle number+symbol mode
//    [.,!]     → cycle: . , ! ? @  (quick punctuation)
//    [del]     → delete last character
//    [SAVE]    → confirm — fire onDone(result), pop
//
//  Letter mode rows:
//    0: q w e r t y u i o p   (10 keys)
//    1: a s d f g h j k l     ( 9 keys)
//    2: z x c v b n m         ( 7 keys)
//
//  Number/symbol mode rows:
//    0: 1 2 3 4 5 6 7 8 9 0   (10 keys)
//    1: - _ . , @ # & ( ) /   (10 keys)
//    2: ! ? + = * % ^ ~ ; :   (10 keys)
// ================================================================

class CharPickerWidget : public IScreen {
public:

    // ── Character rows ────────────────────────────────────────────
    static constexpr const char* ROWS_LO[3] = {
        "qwertyuiop",
        "asdfghjkl",
        "zxcvbnm",
    };
    static constexpr const char* ROWS_HI[3] = {
        "QWERTYUIOP",
        "ASDFGHJKL",
        "ZXCVBNM",
    };
    static constexpr const char* ROWS_NUM[3] = {
        "1234567890",
        "-_.,@#&()/",
        "!?+=*%^~;:",
    };

    // Special key slot indices in row 3
    // .,! removed — punctuation is directly reachable in 123 mode
    static constexpr int SK_CAP   = 0;
    static constexpr int SK_SPC   = 1;
    static constexpr int SK_MODE  = 2;
    static constexpr int SK_DEL   = 3;
    static constexpr int SK_SAVE  = 4;
    static constexpr int SK_COUNT = 5;

    static constexpr int ROW_COUNT  = 4;
    static constexpr int MAX_LENGTH = 64;

    // ── Constructor ───────────────────────────────────────────────
    CharPickerWidget(DisplayManager&                         display,
                     const std::string&                      title,
                     std::string&                            initial,
                     std::function<void(const std::string&)> onDone)
        : _display(display)
        , _title(title)
        , _onDone(onDone)
        , _result(initial)
    {}

    // ── Lifecycle ─────────────────────────────────────────────────
    void onEnter() override {
        _row      = 0;
        _col      = 0;
        _caps     = false;
        _numMode  = false;
        _wantsPop = false;
        _dirty    = true;
    }

    bool needsContinuousUpdate() const override { return false; }
    bool wantsPop()              const override { return _wantsPop; }

    // ── Buttons ───────────────────────────────────────────────────

    // UP → move cursor LEFT within row (wraps)
    void onButtonUp() override {
        int len = _rowLen(_row);
        _col    = (_col - 1 + len) % len;
        _dirty  = true;
    }

    // DOWN → move cursor RIGHT within row (wraps)
    void onButtonDown() override {
        int len = _rowLen(_row);
        _col    = (_col + 1) % len;
        _dirty  = true;
    }

    // LONG UP → move cursor UP one row
    void onLongPressUp() override {
        _row = (_row - 1 + ROW_COUNT) % ROW_COUNT;
        _clampCol();
        _dirty = true;
    }

    // LONG DOWN → move cursor DOWN one row
    void onLongPressDown() override {
        _row = (_row + 1) % ROW_COUNT;
        _clampCol();
        _dirty = true;
    }

    void onButtonSelect() override {
        if (_row < 3) {
            const char* row = _charRow(_row);
            if ((int)_result.length() < MAX_LENGTH)
                _result += row[_col];
        } else {
            _activateSpecial(_col);
        }
        _dirty = true;
    }

    void onLongPressSelect() override {
        if (_onDone) _onDone(_result);
        _wantsPop = true;
    }

    // BACK deletes, never pops
    void onButtonBack() override {
        if (!_result.empty()) {
            _result.pop_back();
            _dirty = true;
        }
    }

    // LONG BACK discards and exits
    void onLongPressBack() override {
        _wantsPop = true;
    }

    void update() override {
        if (!_dirty) return;
        _dirty = false;
        _draw();
    }

private:

    // ── Layout constants ──────────────────────────────────────────
    //
    //  Vertical budget (64px total, no hint bar):
    //    y  0- 8 : title
    //    y  9-16 : input text
    //    y    17 : divider
    //    y 18-28 : row 0  (baseline y=28, 11px tall)
    //    y 29-39 : row 1  (baseline y=39, 11px tall)
    //    y 40-50 : row 2  (baseline y=50, 11px tall)
    //    y 51-63 : row 3  (baseline y=63, 13px tall — special keys)
    //
    static constexpr int TITLE_Y = 8;
    static constexpr int TEXT_Y  = 16;
    static constexpr int DIV_Y   = 17;
    static constexpr int ROW0_Y  = 28;   // baseline of first char row
    static constexpr int ROW_H   = 11;   // pixels per row (was 10, now 11)
    static constexpr int KB_W    = 128;  // wall-to-wall

    // Special row geometry
    // 4 utility keys (cap, spc, 123, del) share LEFT_W; SAVE takes the rest
    // SAVE_W=30, so utility keys get 98/4 = ~24px each — comfortable spacing
    static constexpr int SAVE_W  = 30;
    static constexpr int LEFT_W  = KB_W - SAVE_W;  // 98px / 4 = ~24px each
    static constexpr int UK_W    = LEFT_W / 4;      // utility key width

    // Special key row baseline — flush to bottom of display
    static constexpr int ROW3_Y  = 63;
    static constexpr int ROW3_H  = 11;  // slightly shorter → 2px gap above special keys

    // ── Members ───────────────────────────────────────────────────
    DisplayManager&                         _display;
    std::string                             _title;
    std::function<void(const std::string&)> _onDone;
    std::string                             _result;

    int  _row      = 0;
    int  _col      = 0;
    bool _caps     = false;
    bool _numMode  = false;
    bool _wantsPop = false;
    bool _dirty    = true;

    // ── Helpers ───────────────────────────────────────────────────

    const char* _charRow(int row) const {
        if (_numMode) return ROWS_NUM[row];
        return _caps ? ROWS_HI[row] : ROWS_LO[row];
    }

    int _rowLen(int row) const {
        if (row == 3) return SK_COUNT;
        return (int)strlen(_charRow(row));
    }

    // After changing row, clamp col so it never exceeds new row length
    void _clampCol() {
        int len = _rowLen(_row);
        if (_col >= len) _col = len - 1;
        if (_col < 0)    _col = 0;
    }

    void _activateSpecial(int key) {
        switch (key) {
            case SK_CAP:
                if (!_numMode) _caps = !_caps;
                break;

            case SK_SPC:
                if ((int)_result.length() < MAX_LENGTH)
                    _result += ' ';
                break;

            case SK_MODE:
                _numMode = !_numMode;
                _clampCol();
                break;

            case SK_DEL:
                if (!_result.empty()) _result.pop_back();
                break;

            case SK_SAVE:
                if (_onDone) _onDone(_result);
                _wantsPop = true;
                break;
        }
    }

    // ── Render ────────────────────────────────────────────────────
    void _draw() {
        auto& u = _display.raw();
        u.clearBuffer();
        u.setFont(u8g2_font_5x7_tr);

        // ── Title ─────────────────────────────────────────────────
        u.drawStr(0, TITLE_Y, _title.c_str());

        // Mode tag top-right
        const char* tag = _numMode ? "123" : (_caps ? "CAP" : "abc");
        int tw = u.getStrWidth(tag);
        u.drawStr(127 - tw, TITLE_Y, tag);

        // ── Input text ────────────────────────────────────────────
        // 5px/char, 128px → ~25 chars visible. Scroll from left.
        std::string shown = _result + "_";
        if ((int)shown.length() > 24)
            shown = ".." + shown.substr(shown.length() - 22);
        u.drawStr(0, TEXT_Y, shown.c_str());

        // ── Divider ───────────────────────────────────────────────
        u.drawLine(0, DIV_Y, 127, DIV_Y);

        // ── Character rows 0..2 ───────────────────────────────────
        for (int r = 0; r < 3; r++) {
            const char* row = _charRow(r);
            int         len = (int)strlen(row);
            int         y   = ROW0_Y + r * ROW_H;

            // Divide 128px evenly among len keys — wall to wall
            int cellW  = KB_W / len;
            int totalW = cellW * len;
            int startX = (KB_W - totalW) / 2;  // centre leftover pixel(s)

            for (int c = 0; c < len; c++) {
                int  x   = startX + c * cellW;
                bool sel = (_row == r && _col == c);
                char buf[2] = { row[c], '\0' };

                if (sel) {
                    // Full cell inverted
                    u.setDrawColor(1);
                    u.drawBox(x, y - (ROW_H - 2), cellW, ROW_H);
                    u.setDrawColor(0);
                    u.setFontMode(1);
                    int cw = u.getStrWidth(buf);
                    u.drawStr(x + (cellW - cw) / 2, y, buf);
                    u.setDrawColor(1);
                    u.setFontMode(0);
                } else {
                    // Centred character, no box
                    int cw = u.getStrWidth(buf);
                    u.drawStr(x + (cellW - cw) / 2, y, buf);
                }
            }
        }

        // ── Special keys row 3 ────────────────────────────────────
        // Baseline flush to bottom of screen; box spans ROW3_H px tall
        const int y3    = ROW3_Y;
        const int boxH  = ROW3_H;
        const int boxT  = y3 - boxH + 1;  // top of box

        // Labels for utility keys 0..3
        const char* ulabels[4] = {
            _caps    ? "CAP" : "cap",   // SK_CAP
            "spc",                       // SK_SPC
            _numMode ? "ABC" : "123",   // SK_MODE
            "del",                       // SK_DEL
        };

        // Draw utility keys
        for (int k = 0; k < 4; k++) {
            int  x   = k * UK_W;
            bool sel = (_row == 3 && _col == k);
            int  lw  = u.getStrWidth(ulabels[k]);

            if (sel) {
                // Inverted — selected utility key
                u.setDrawColor(1);
                u.drawBox(x, boxT, UK_W, boxH);
                u.setDrawColor(0);
                u.setFontMode(1);
                u.drawStr(x + (UK_W - lw) / 2, y3, ulabels[k]);
                u.setDrawColor(1);
                u.setFontMode(0);
            } else {
                // Framed, normal
                u.drawFrame(x, boxT, UK_W, boxH);
                u.drawStr(x + (UK_W - lw) / 2, y3, ulabels[k]);
            }
        }

        // Draw SAVE key — always filled (prominent), inner border when selected
        int  saveX   = LEFT_W;
        bool saveSel = (_row == 3 && _col == SK_SAVE);

        u.setDrawColor(1);
        u.drawBox(saveX, boxT, SAVE_W, boxH);
        u.setDrawColor(0);
        u.setFontMode(1);
        int sw = u.getStrWidth("SAVE");
        u.drawStr(saveX + (SAVE_W - sw) / 2, y3, "SAVE");
        u.setDrawColor(1);
        u.setFontMode(0);

        // When SAVE is selected, draw inner border to indicate focus
        if (saveSel) {
            u.drawFrame(saveX + 2, boxT + 2, SAVE_W - 4, boxH - 4);
        }

        u.sendBuffer();
    }
};