#pragma once
#include <string>
#include <algorithm>

#include "../../IScreen.h"
#include "hal.h"
#include "../../../../core/DisplayManager.h"
#include "../../../../core/UIManager.h"
#include "../../../../core/WiFiManager.h"
#include "../../../../core/NotificationManager.h"
#include "WiFiCredentialsScreen.h"
#include "config.h"

// ================================================================
//  WiFiStatusScreen  —  WiFi status + retry + forget
//
//  Shows current state, IP, SSID, signal strength.
//  Actions depend on current NetState:
//
//    IDLE/FAILED  → [SEL] Connect  (push credentials or retry)
//    CONNECTED    → [SEL] Forget   [LONG SEL] Retry
//    CONNECTING   → read-only, shows attempt progress
//
//  Layout:
//  ┌──────────────────────────────────────────────────────────┐
//  │  WiFi                                                    │
//  │ ─────────────────────────────────────────────────────── │
//  │  Status     Connected                                    │
//  │  SSID       MyNetwork                                    │
//  │  IP         192.168.1.42                                 │
//  │  Signal     -62 dBm                                      │
//  │  [SEL] Forget           [LONG SEL] Retry                 │
//  └──────────────────────────────────────────────────────────┘
// ================================================================

class WiFiStatusScreen : public IScreen {
    public:
        WiFiStatusScreen(DisplayManager& display, 
                         UIManager& ui, 
                         WiFiManager& wifi)
                : _display(display), _ui(ui), _wifi(wifi) {}
        
        void onEnter() override {
            _dirty = true;
            _feedback = "";
        }

        bool needsContinuousUpdate() const override { return true; }

        void onButtonSelect() override {
            switch(_wifi.state()) {
                case NetState::IDLE:
                    // No credentials — push credential entry
                    _pushCredentials();
                    break;

                case NetState::FAILED:
                    // Has credentials, just failed — retry
                    _wifi.manualRetry();
                    _feedback = "Retrying...";
                    _dirty = true;
                    break;
                
                case NetState::CONNECTED:
                    // Forget network
                    _wifi.forget();
                    _feedback = "Network forgotten";
                    _dirty = true;
                    break;

                case NetState::CONNECTING:
                case NetState::LOST:
                    // Do nothing — in progress
                    break;
            }
        }

        // Long select — retry regardless of state
        void onLongPressSelect() override {
            if(_wifi.hasCredentials()){
                _wifi.manualRetry();
                _feedback = "Retrying...";
                _dirty = true;
            } else {
                _pushCredentials();
            }
        }

        void update() override {
            _draw();
        }

    private:
        static constexpr int ROW_H = 11;
        static constexpr int ROW_Y = 12;

        // ── Members ───────────────────────────────────────────────
        DisplayManager&     _display;
        UIManager&          _ui;
        WiFiManager&        _wifi;
        std::string         _feedback;

        void _pushCredentials() {
            _ui.pushScreen(new WiFiCredentialsScreen(_display, _ui, _wifi));
        }

        static std::string _stateStr(NetState state) {
            switch(state){
                case NetState::IDLE:        return "No credentials";
                case NetState::CONNECTING:  return "Connecting...";
                case NetState::CONNECTED:   return "Connected";
                case NetState::LOST:        return "Reconnecting...";
                case NetState::FAILED:      return "Failed !";
            }
            return "Unknown";
        }

        // ── Render ─────────────────────────────────────────────────
        void _draw() {
            auto& u = _display.raw();
            u.clearBuffer();
            u.setFont(u8g2_font_5x7_tr);

            // Header
            u.drawStr(2, 8, "WiFi");
            u.drawLine(0, 9, 127, 9);

            NetState st = _wifi.state();

            // Row helper
            int y = ROW_Y;
            auto row = [&](const char* label, const std::string& val) {
                u.drawStr(2,  y + 8, label);
                int vw = u.getStrWidth(val.c_str());
                u.drawStr(126 - vw, y + 8, val.c_str());
                y += ROW_H;
            };

            // Status row
            row("Status", _stateStr(st));

            // SSID
            if (_wifi.hasCredentials())
                row("SSID", _wifi.ssid());
            else
                row("SSID", "Not set");

            // IP + signal — only when connected
            if (st == NetState::CONNECTED) {
                row("IP",     _wifi.localIp());
                row("Signal", std::to_string(_wifi.rssi()) + " dBm");
            } else if (st == NetState::CONNECTING || st == NetState::LOST) {
                row("Attempt", std::to_string(_wifi.attemptCount()) +
                            "/" + std::to_string(Config::WiFi::MAX_ATTEMPTS));
            }

            // Feedback or action hint
            u.drawLine(0, 53, 127, 53);
            if (_feedback.length() > 0) {
                u.drawStr(2, 63, _feedback.c_str());
            } else {
                switch (st) {
                    case NetState::IDLE:
                    case NetState::FAILED:
                        u.drawStr(2, 63, "[SEL] Connect");
                        break;
                    case NetState::CONNECTED:
                        u.drawStr(2, 63, "[SEL] Forget  [LSEL] Retry");
                        break;
                    case NetState::CONNECTING:
                    case NetState::LOST:
                        u.drawStr(2, 63, "Connecting...");
                        break;
                }
            }

            u.sendBuffer();
        }
};
