#pragma once
#include <string>
#include <algorithm>

#include "hal.h"
#include "config.h"
#include "../../IScreen.h"
#include "../../../../core/DisplayManager.h"
#include "../../../../core/UIManager.h"
#include "../../../../network/WebSocketManager.h"
#include "ServerCredentialsScreen.h"

// ================================================================
//  ServerStatusScreen  —  WebSocket server status + retry
//
//  Shows current WsState, host, attempt count.
//  Actions:
//    IDLE/FAILED  → [SEL] Configure / Retry
//    REGISTERED   → read-only status
//    CONNECTING   → read-only, shows attempt progress
//
//  Layout:
//  ┌──────────────────────────────────────────────────────────┐
//  │  Server                                                  │
//  │ ─────────────────────────────────────────────────────── │
//  │  Status     Registered                                   │
//  │  Host       192.168.1.100                                │
//  │  Port       3000                                         │
//  │  Path       /ws                                          │
//  │  [SEL] Configure        [LONG SEL] Retry                 │
//  └──────────────────────────────────────────────────────────┘
// ================================================================

class ServerStatusScreen : public IScreen {
    public:
        ServerStatusScreen(DisplayManager& display, 
                           UIManager& ui, 
                           WebSocketManager& ws)
                : _display(display), _ui(ui), _ws(ws) {}
        
        void onEnter() override {
            _dirty = true;
            _feedback = "";
        }

        bool needsContinuousUpdate() const override { return true; }

        void onButtonSelect() override {
            switch(_ws.webSocketState()){
                case WebSocketState::IDLE:
                case WebSocketState::FAILED:
                    if(_ws.hasConfig()){
                        _retry();           // Has config — retry
                    } else {
                        _pushConfig();      // No config — push credential entry
                    }
                    break;
                
                case WebSocketState::REGISTERED:
                case WebSocketState::CONNECTED:
                case WebSocketState::CONNECTING:
                    _pushConfig();
                    break;
            }
        }

        void onLongPressSelect() override {
            _retry();
        }

        void update() override {
            _draw();
        }

    // ── Members ───────────────────────────────────────────────
    private:
        static constexpr int ROW_H = 11;
        static constexpr int ROW_Y = 12;

        DisplayManager&   _display;
        UIManager&        _ui;
        WebSocketManager& _ws;
        std::string       _feedback;


        void _retry() {
            if(_ws.hasConfig()) {
                _ws.manualRetry();
                _feedback = "Retrying...";
                _dirty = true;
            } else {
                _pushConfig();
            }
        }

        void _pushConfig() {
            _ui.pushScreen(new ServerCredentialsScreen(_display, _ui, _ws));
        }

        // ── Status Strings ─────────────────────────────────────────
        static std::string _stateStr(WebSocketState s) {
            switch (s) {
                case WebSocketState::IDLE:        return "Idle";
                case WebSocketState::CONNECTING:  return "Connecting...";
                case WebSocketState::CONNECTED:   return "Connected";
                case WebSocketState::REGISTERED:  return "Registered";
                case WebSocketState::FAILED:      return "Failed";
            }
            return "";
        }

        // ── Render ─────────────────────────────────────────────────
        void _draw() {
            auto& u = _display.raw();
            u.clearBuffer();
            u.setFont(u8g2_font_5x7_tr);

            u.drawStr(2, 8, "Server");
            u.drawLine(0, 9, 127, 9);

            WebSocketState st = _ws.webSocketState();
            int y = ROW_Y;

            auto row = [&](const char* label, const std::string& val) {
                u.drawStr(2, y + 8, label);
                int vw = u.getStrWidth(val.c_str());
                u.drawStr(126 - vw, y + 8, val.c_str());
                y += ROW_H;
            };

            row("Status", _stateStr(st));

            if (_ws.hasConfig()) {
                row("Host", _ws.host());
                row("Port", std::to_string(_ws.port()));
                row("Path", _ws.path());
            } else {
                row("Host", "Not configured");
            }

            if (st == WebSocketState::CONNECTING) {
                row("Attempt", std::to_string(_ws.attemptCount()) +
                            "/" + std::to_string(Config::WebSocket::MAX_ATTEMPTS));
            }

            u.drawLine(0, 53, 127, 53);
            if (_feedback.length() > 0) {
                u.drawStr(2, 63, _feedback.c_str());
            } else {
                switch (st) {
                    case WebSocketState::IDLE:
                    case WebSocketState::FAILED:
                        u.drawStr(2, 63, _ws.hasConfig()
                            ? "[SEL] Retry  [LSEL] Configure"
                            : "[SEL] Configure");
                        break;
                    case WebSocketState::REGISTERED:
                    case WebSocketState::CONNECTED:
                        u.drawStr(2, 63, "[SEL] Change server");
                        break;
                    case WebSocketState::CONNECTING:
                        u.drawStr(2, 63, "Connecting...");
                        break;
                }
            }

            u.sendBuffer();
        }
};