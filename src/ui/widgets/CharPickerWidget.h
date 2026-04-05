#pragma once
#include <string>
#include <functional>
#include <algorithm> 
#include "hal.h"
#include "../../ui/screens/IScreen.h"
#include "../../core/DisplayManager.h"

// ================================================================
//  CharPickerWidget  —  On-device text entry for 4-button input
//
//  A full IScreen that renders a character wheel + typed string.
//  Caller pushes it onto the UIManager stack, provides a title
//  and an onDone callback. On confirm the callback fires with the
//  result string, then the screen pops itself.
//
//  Controls:
//    UP            scroll character wheel up
//    DOWN          scroll character wheel down
//    LONG UP       jump to next character category
//    LONG DOWN     jump to prev character category
//    SELECT        append current character to string
//    LONG SELECT   confirm — fire onDone(result), pop
//    LONG BACK     clears result string
//
//  Character categories (LONG UP/DOWN jumps between them):
//    [0]  a-z        (26 chars)
//    [1]  A-Z        (26 chars)
//    [2]  0-9        (10 chars)
//    [3]  symbols    ! @ # $ % & * - _ . , space
//    [4]  backspace  ⌫  (displayed as "<")
//
//  Layout (128×64):
//  ┌──────────────────────────────────────────────────────────────┐
//  │  WiFi Password                                               │  y=8  title
//  │ ──────────────────────────────────────────────────────────── │
//  │  mypassword_                                                 │  y=22 typed string
//  │                                                              │
//  │              ↑                                               │
//  │         [ p  q  r  s ]                                       │  y=42 wheel
//  │              ↓                                               │
//  │  [SEL] add  [LSEL] done  [LUP/DN] category   [LBCK] cancel  │  y=63 hint
//  └──────────────────────────────────────────────────────────────┘
//
//  Usage:
//    auto* picker = new CharPickerWidget(
//        _display, "WiFi Password", currentValue,
//        [this](const String& result) {
//            _wifi.saveCredentials(_ssid, result);
//        }
//    );
//    _ui.pushScreen(picker);
// ================================================================


class CharPickerWidget : public IScreen {
    public:
        // ── Character set ─────────────────────────────────────────
        static constexpr const char* CHARSET = 
            "abcdefghijklmnopqrstuvwxyz"   // 0-25
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"   // 26-51
            "0123456789"                   // 52-61
            "!@#$%&*-_., "                 // 62-73  (space is last of symbols)
            "<";
        
        static constexpr int CHARSET_LEN    = 75;
        static constexpr int MAX_LENGTH     = 64;

        // Category start indices into CHARSET
        static constexpr int CAT_LOWER      = 0;
        static constexpr int CAT_UPPER      = 26;
        static constexpr int CAT_DIGITS     = 52;
        static constexpr int CAT_SYMBOLS    = 62;
        static constexpr int CAT_BACK       = 74;
        
        static constexpr int CAT_COUNT      = 5;

        // stores all the start indices in one array.
        inline static constexpr int CAT_STARTS[CAT_COUNT] = {
            CAT_LOWER, CAT_UPPER, CAT_DIGITS, CAT_SYMBOLS, CAT_BACK
        };
        inline static constexpr const char* CAT_NAMES[CAT_COUNT] = {
            "a-z", "A-Z", "0-9", "!@#", "DEL"
        };

        CharPickerWidget(DisplayManager& display, const std::string& title, 
                         std::string& initial, std::function<void(const std::string&)> onDone)
                        : _display(display)
                        , _title(title)
                        ,_onDone(onDone)
                        ,_result(initial)
                    {}
        
        void onEnter() override {
            _charIdx    = 0;
            _dirty      = true;
            _wantsPop   = false;
        }

        bool needsContinuousUpdate() const override { return false; }
        bool wantsPop()              const override { return _wantsPop; }

        
        // ── Buttons ───────────────────────────────────────────────
        void onButtonUp()   override { _charIdx = (_charIdx-1+CHARSET_LEN) % CHARSET_LEN; _dirty = true; }
        void onButtonDown() override { _charIdx = (_charIdx+1) % CHARSET_LEN; _dirty = true; }
        void onButtonBack() override {
            // Intentionally empty — UIManager handles the pop
        }

        // LongUp - Jump to next category
        void onLongPressUp() override { _jumpCategory(1); _dirty = true; }

        // LONG DOWN — jump to prev category
        void onLongPressDown() override { _jumpCategory(-1); _dirty = true; }

        // SELECT — append current character (or backspace)
        void onButtonSelect() override {
            char c = CHARSET[_charIdx];
            if(c == '<'){
                if(_result.length() > 0){
                    _result = _result.substr(0, _result.length()-1);
                }
            } else if((int)_result.length() < MAX_LENGTH) {
                _result += c;
            }
            _dirty = true;
        }

        // Long Select - confirm and pop
        void onLongPressSelect() override { if(_onDone) _onDone(_result); _wantsPop = true; }

        // Long Back - clear entire screen
        void onLongPressBack() override {_result = ""; _dirty=  true; }

        // ── Render ────────────────────────────────────────────────
        void update() override {
            if(!_dirty) return;
            _dirty = false;
            _draw();
        }

    private:
        // ── Wheel constants ───────────────────────────────────────
        static constexpr int WHEEL_VISIBLE = 5; // number of chars visible in wheel at once
        static constexpr int WHEEL_Y       = 42; // Centre char baseline

        // ── Members ───────────────────────────────────────────────
        DisplayManager&                         _display;
        std::string                             _title;
        std::function<void(const std::string&)> _onDone;
        std::string                             _result;
        int                                     _charIdx  = 0;
        bool                                    _wantsPop = false;


        // ── Helpers ───────────────────────────────────────────────
        // Jump char index to start of next/prev category
        void _jumpCategory(int dir) {
            int cur = _currentCategory();
            int next = (cur + dir + CAT_COUNT) % CAT_COUNT;
            _charIdx = CAT_STARTS[next];
        }

        // Returns which category _charIdx currently fall in
        int _currentCategory() const {
            for(int i=CAT_COUNT-1; i>=0; i--){
                if(_charIdx >= CAT_STARTS[i]) return i;
            }
            return 0;
        }


        // ── Render ─────────────────────────────────────────────────
        void _draw() {
            auto& u = _display.raw();
            u.clearBuffer();
            u.setFont(u8g2_font_5x7_tr);

            // ── Title ─────────────────────────────────────────────
            u.drawStr(0, 8, _title.c_str());
            u.drawLine(0, 9, 127, 9);

            // ── Typed string with cursor ──────────────────────────
            std::string display = _result + "_";
            // Scroll so cursor is always visible — show last N chars
            if (display.length() > 21) {
                display = "..." + display.substr(display.length() - 18);
            }
            u.drawStr(0, 21, display.c_str());

            // ── Category label (top-right) ────────────────────────
            int cat = _currentCategory();
            int lw  = u.getStrWidth(CAT_NAMES[cat]);
            u.drawStr(127 - lw, 21, CAT_NAMES[cat]);

            // ── Character wheel ───────────────────────────────────
            // Show WHEEL_VISIBLE chars centred on _charIdx
            // Centre char is highlighted (inverted)
            int half = WHEEL_VISIBLE / 2;

            for (int i = -half; i <= half; i++) {
                int idx = (_charIdx + i + CHARSET_LEN) % CHARSET_LEN;
                char c  = CHARSET[idx];
                char buf[3] = { c == '<' ? (char)'<' : c, '\0', '\0' };

                // X position — evenly spaced, centred at 64
                int x = 64 + i * 14 - 3;

                if (i == 0) {
                    // Highlighted centre — inverted box
                    u.drawRBox(x - 3, WHEEL_Y - 9, 13, 11, 2);
                    u.setDrawColor(0);
                    u.setFontMode(1);
                    u.setFont(u8g2_font_6x10_tr);
                    u.drawStr(x - 1, WHEEL_Y, buf);
                    u.setDrawColor(1);
                    u.setFontMode(0);
                    u.setFont(u8g2_font_5x7_tr);
                } else {
                    // Fade neighbours — draw smaller, dim outer ones
                    u.drawStr(x, WHEEL_Y - 1, buf);
                }
            }

            // ── Up/down arrows ────────────────────────────────────
            // Small triangles above and below the wheel centre
            u.drawTriangle(64, 30,  61, 35,  67, 35);   // up arrow
            u.drawTriangle(64, 51,  61, 46,  67, 46);   // down arrow

            // ── Footer hint ───────────────────────────────────────
            u.setFont(u8g2_font_4x6_tr);
            u.drawStr(0, 63, "[SEL]add [LSEL]done [LUP/DN]cat [LBCK]clear");

            u.sendBuffer();
        }
};