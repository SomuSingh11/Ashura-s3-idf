#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include "config.h"
#include "hal.h"

#include "../IScreen.h"
#include "../../../core/DisplayManager.h"
#include "../../../core/UIManager.h"
#include "../../../application/wled/WledManager.h"


// Forward declare to avoid circular include
class WledMainScreen;
#include "../../../ui/screens/wled/WledMainScreen.h"

// ================================================================
//  WledDeviceScreen  —  Device list + discovery
//
//  Shows saved devices. UP = scan for new devices.
//  SELECT = connect to highlighted device → push WledMainScreen.
//
//  Layout:
//  ┌──────────────────────────────────────────────────────────┐
//  │  WLED Devices              [UP] Scan                     │
//  │ ─────────────────────────────────────────────────────── │
//  │  ● Living Room   192.168.1.42                            │
//  │    Bedroom       192.168.1.43                            │
//  │    Desk          192.168.1.44                            │
//  │  [SEL] Connect                              [BCK] Back   │
//  └──────────────────────────────────────────────────────────┘
// ================================================================

class WledDeviceScreen : public IScreen {
    public:
        WledDeviceScreen(DisplayManager& display, UIManager& ui, WledManager& wled)
            : _display(display), _ui(ui), _wled(wled) {}

        void onEnter() override {
            _pos        = std::max(0, _wled.activeIndex());
            _scanning   = false;
            _msg        = "";
            _dirty      = true;
        }

        bool needsContinuousUpdate() const override { return _scanning; }
        bool wantsPop() const override { return _wantsPop; }

        void onButtonUp() override {
            if(_scanning) return;
            if(_wled.deviceCount() > 0){
                _pos = (_pos - 1 + _wled.deviceCount()) % _wled.deviceCount();
                _dirty = true;
            }
        }

        void onButtonDown() override {
            if(_scanning) return;
            if(_wled.deviceCount() > 0){
                _pos = (_pos + 1) % _wled.deviceCount();
                _dirty = true;
            }
        }

        void onButtonSelect() override {
            if(_scanning) return;
            
            if(_wled.deviceCount() == 0){
                _startScan();
                return;
            }

            // Connect to selected device and push WledMainScreen on success
            _msg = "Connecting...";
            _dirty = true;
            _draw(); // Ensure "Connecting..." message is shown before potentially long (blocking) connect() call

            if(_wled.connect(_pos)){
                _ui.pushScreen(new WledMainScreen(_display, _ui, _wled));
            } else {
                _msg = "Connection failed";
                _dirty = true;
            }
        }

        void onButtonBack() override { _wantsPop = true; }
        
        // Long press UP = scan (also triggered from select when no devices)
        void onLongPressUp() { _startScan(); }

        void update() override {
            if(_scanPending) {
                _scanPending = false;
                _runScan();
            }

            if(!_dirty) return;
            _dirty = false;
            _draw();
        }

    private:
        DisplayManager&    _display;
        UIManager&         _ui;
        WledManager&       _wled;

        int                _pos         =  0;
        int                _winPos      =  0;
        bool               _scanning    =  false;
        bool               _scanPending = false;
        bool               _wantsPop    = false;
        std::string        _msg;

        void _startScan() {
            _scanning    = true;
            _scanPending = true; 
            _msg         = "Scanning...";
            _dirty       = true;
            _draw(); 
        }

        void _runScan() {
            int found = _wled.discover();
            _scanning = false;
            _msg      = found > 0 ? "Found " + std::to_string(found) + " device(s)"
                                  : "No new devices found"; 

            if(_wled.deviceCount() > 0) {
                _pos = 0;
            }
            _dirty    = true;
        }

        void _draw() {
            auto& u = _display.raw();
            u.clearBuffer();

            // Header
            u.setFont(u8g2_font_5x7_tr);
            u.drawStr(2, 8, "WLED Devices");
            if (!_scanning) {
                int w = u.getStrWidth("[UP] Scan");
                u.drawStr(126 - w, 8, "[UP] Scan");
            }
            u.drawLine(0, 9, 127, 9);

            if (_scanning) {
                // Spinner
                _drawSpinner(u);
                u.drawStr(44, 40, "Scanning...");
            } else if (_wled.deviceCount() == 0) {
                u.drawStr(10, 35, "No devices found.");
                u.drawStr(10, 47, "[SEL] to scan");
            } else {
                // Device list
                int count = _wled.deviceCount();
                // Scroll window
                if (_pos < _winPos) _winPos = _pos;
                if (_pos >= _winPos + WLED_DEVICE_MENU_ITEMS_ON_SCREEN) _winPos = _pos - WLED_DEVICE_MENU_ITEMS_ON_SCREEN + 1;

                for (int i = 0; i < WLED_DEVICE_MENU_ITEMS_ON_SCREEN; i++) {
                    int idx = _winPos + i;
                    if (idx >= count) break;

                    int y = WLED_DEVICE_MENU_START_Y + i * WLED_DEVICE_MENU_ITEM_HEIGHT;
                    bool isSel    = (idx == _pos);
                    bool isActive = (idx == _wled.activeIndex());

                    if (isSel) {
                        u.setDrawColor(1);
                        u.drawRBox(0, y, 125, WLED_DEVICE_MENU_ITEM_HEIGHT - 1, 1);
                        u.setDrawColor(0);
                        u.setFontMode(1);
                    }

                    const WledDevice& dev = _wled.devices()[idx];
                    u.drawStr(8, y + 9, dev.name.c_str());

                    // Show IP right-aligned
                    int w = u.getStrWidth(dev.ip.c_str());
                    u.drawStr(124 - w, y + 9, dev.ip.c_str());

                    // Active dot
                    if (isActive) u.drawDisc(3, y + 6, 2);

                    if (isSel) {
                        u.setDrawColor(1);
                        u.setFontMode(0);
                    }
                }
            }

            // Message / footer
            if (_msg.length() > 0) {
                u.setDrawColor(1);
                u.setFontMode(0);
                u.setFont(u8g2_font_5x7_tr);
                u.drawStr(2, 63, _msg.c_str());
            } else {
                u.drawStr(2, 63, "[SEL] Connect");
            }

            u.sendBuffer();
        }

        void _drawSpinner(U8G2& u) {
            static int frame = 0;
            frame = (frame + 1) % 8;
            const char* frames[] = {"|", "/", "-", "\\", "|", "/", "-", "\\"};
            int w = u.getStrWidth(frames[frame]);
            u.drawStr((128 - w) / 2, 28, frames[frame]);
        }
};