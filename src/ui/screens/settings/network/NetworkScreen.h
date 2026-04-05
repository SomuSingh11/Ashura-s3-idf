#pragma once
#include <string>
#include <algorithm>

#include "../../IScreen.h"
#include "../../../../core/DisplayManager.h"
#include "../../../../core/UIManager.h"
#include "../../../../core/WiFiManager.h"
#include "../../../../network/WebSocketManager.h"
#include "../../../../ui/screens/settings/network/WiFiStatusScreen.h"
#include "../../../../ui/screens/settings/network/ServerStatusScreen.h"

// ================================================================
//  NetworkScreen  —  Settings > Network submenu
//
//  Two items:
//    WiFi   → WiFiStatusScreen
//    Server → ServerStatusScreen
//
//  Shows live status inline so user can see state at a glance
//  without having to enter the sub-screen.
//
//  ┌──────────────────────────────────────────────────────────┐
//  │  Network                                                 │
//  │ ─────────────────────────────────────────────────────── │
//  │▶ WiFi                                       Connected   │
//  │  Server                                     Registered  │
//  │                                                          │
//  └──────────────────────────────────────────────────────────┘
// ================================================================

class NetworkScreen : public IScreen {
public:
    NetworkScreen(DisplayManager&   display, 
                  UIManager&        ui, 
                  WiFiManager&      wifi, 
                  WebSocketManager& ws)
        : _display(display), _ui(ui), _wifi(wifi), _ws(ws) {}
    
    void onEnter() override {
        _pos = 0;
        _dirty = true;
    }

    bool needsContinuousUpdate() const override { return true; }

    void onButtonUp() override {
        _pos = (_pos - 1 + ITEM_COUNT) % ITEM_COUNT;
        _dirty = true;
    }
    void onButtonDown() override {
        _pos = (_pos + 1) % ITEM_COUNT;
        _dirty = true;
    }

    void onButtonSelect() override {
        switch(_pos){
            case 0: _ui.pushScreen(new WiFiStatusScreen(_display, _ui, _wifi)); break;
            case 1: _ui.pushScreen(new ServerStatusScreen(_display, _ui, _ws)); break;
        }
    }

    void update() override {
        _draw();
    }

private:
    // ── Members ───────────────────────────────────────────────

    static constexpr int ITEM_COUNT = 2;
    static constexpr int ITEM_H = 13;
    static constexpr int LIST_Y = 12;

    DisplayManager&     _display;
    UIManager&          _ui;
    WiFiManager&        _wifi;
    WebSocketManager&   _ws;

    int _pos = 0;


    // ── Status Strings ─────────────────────────────────────────

    std::string _wifiStatusStr() const {
        switch(_wifi.state()){
            case NetState::IDLE:        return "No Credentials";
            case NetState::CONNECTING:  return "Connecting...";
            case NetState::CONNECTED:   return "Connected";
            case NetState::FAILED:      return "Failed !";
            case NetState::LOST:        return "Reconnecting...";
        }
        return "";
    };

    std::string _wsStatusStr() const {
        switch (_ws.webSocketState()) {
            case WebSocketState::IDLE:        return "Idle";
            case WebSocketState::CONNECTING:  return "Connecting...";
            case WebSocketState::CONNECTED:   return "Connected";
            case WebSocketState::REGISTERED:  return "Registered";
            case WebSocketState::FAILED:      return "Failed !";
        }
        return "";
    }


    // ── Render ─────────────────────────────────────────────────
    void _draw() {
        auto& u = _display.raw();
        u.clearBuffer();
        u.setFont(u8g2_font_5x7_tr);

        u.drawStr(2, 8, "Network");
        u.drawLine(0, 9, 127, 9);

        const char* labels[ITEM_COUNT] = {"WiFi", "Server"};
        std::string values[ITEM_COUNT] = {_wifiStatusStr(), _wsStatusStr()};

        for (int i = 0; i < ITEM_COUNT; i++) {
            int  y   = LIST_Y + i * ITEM_H;
            bool sel = (i == _pos);

            if (sel) {
                u.setDrawColor(1);
                u.drawRBox(0, y, 127, ITEM_H - 1, 1);
                u.setDrawColor(0);
                u.setFontMode(1);
            }

            u.drawStr(6, y + 9, labels[i]);

            int vw = u.getStrWidth(values[i].c_str());
            u.drawStr(125 - vw, y + 9, values[i].c_str());

            if (sel) {
                u.setDrawColor(1);
                u.setFontMode(0);
            }
        }

        u.sendBuffer();
    }
};  