#pragma once
#include <string>
#include "hal.h"
#include "../IScreen.h"
#include "../../../core/DisplayManager.h"


class SplashScreen: public IScreen {
    public:
        explicit SplashScreen(DisplayManager& display) : _display(display){}

        void onEnter() override {
            _startTime = 0;
            _phase = 0;
            _letterCount = 0;
            _scanX = 0;
            _dirty = true;
            _started = false;
        }

        bool needsContinuousUpdate() const override { return true; }
        bool wantsPop() const override { return _wantsPop; }

        void onButtonSelect() override { _wantsPop = true; }
        void onButtonBack() override { _wantsPop = true; }

        void update() override {
            // if(_startTime == 0) _startTime = millis();
            // unsigned long elapsed = millis() - _startTime;
            if (!_started) {
                _started   = true;
                _startTime = millis();
            }
            unsigned long elapsed = millis() - _startTime;

            // Phase Selection
            if (elapsed > 2600) { 
                _wantsPop = true; 
                return; 
            } 
            else if (elapsed > 2200) {
                _phase = 3;
            }
            else if (elapsed > 1600) {
                _phase = 2;
            }
            else if (elapsed > 800) {
                _phase = 1;
            }
            else {
                _phase = 0;
            }

            _display.clear();

            const char* title = "ASHURA";
            int titleLen = strlen(title);

            // === Phase 0: Type in "ASHURA" ===
            if (_phase == 0) {
                _letterCount = std::min((int)((elapsed) / 130), titleLen);
            } else {
                _letterCount = titleLen;
            }

            _display.setFontLarge();
            char buf[8] = {0};
            strncpy(buf, title, _letterCount); 
            
            // Draw revealed letters — large font, centered
            // Center on 128px wide screen (each char ~10px wide in large font)
            int textX = (128 - _letterCount * 10) / 2;
            if (textX < 0) textX = 0;
            _display.drawStr(textX, 28, buf);

            // Blinking cursor during typing
            if (_phase == 0 && (elapsed / 200) % 2 == 0) {
                _display.drawStr(textX + _letterCount * 10, 28, "_");
            }

            // === Phase 1: Scan line sweeps across ===
            if (_phase >= 1) {
                // Draw a simple horizontal line under the title
                _display.drawLine(0, 127, 33, 33);   // full width line at y=33
                _display.setFontSmall();
                _display.drawStr(0,  38, "[");
                _display.drawStr(118, 38, "]");
            }

            // === Phase 2: Subtitle appears ===
            if (_phase >= 2) {
                _display.setFontSmall();
                _display.drawStr(38, 44, "OS  v1.0");
            }

            // === Phase 3: Bottom tagline ===
            if (_phase >= 3) {
                _display.setFontSmall();
                _display.drawStr(20, 58, "Loading system...");
            }

            _display.sendBuffer();
            yield(); 
        }

    private:
        DisplayManager& _display;
        unsigned long   _startTime = 0;
        int             _phase = 0;
        int             _letterCount=0;
        int             _scanX=0;
        bool            _wantsPop = false;
        bool            _started = false;
};