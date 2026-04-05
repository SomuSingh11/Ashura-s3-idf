#pragma once
#include <string>
#include <functional>
#include <algorithm>

#include "hal.h"
#include "../../IScreen.h"
#include "../../../../core/DisplayManager.h"
#include "../../../../core/UIManager.h"
#include "../../../../network/WebSocketManager.h"
#include "../../../widgets/CharPickerWidget.h"

// ================================================================
//  ServerCredentialsScreen  —  Host / Port / Path editor
//
//  Plain three-row menu. Each field edited independently.
//
//  Controls:
//    UP / DOWN     move cursor
//    SELECT        push CharPicker for highlighted field
//    LONG SELECT   save config + pop (only if host filled)
//    BACK          pop without saving — UIManager handles naturally
//
//  Layout:
//  ┌──────────────────────────────────────────────────────────┐
//  │  Server Config                                           │
//  │ ─────────────────────────────────────────────────────── │
//  │▶ Host        192.168.1.100                               │
//  │  Port        3000                                        │
//  │  Path        /ws                                         │
//  │  [SEL] Edit            [LSEL] Save                       │
//  └──────────────────────────────────────────────────────────┘
// ================================================================

class ServerCredentialsScreen : public IScreen {
public:
    ServerCredentialsScreen(DisplayManager&   display,
                             UIManager&        ui,
                             WebSocketManager& ws)
        : _display(display), _ui(ui), _ws(ws) {}

    void onEnter() override {
        // Pre-fill on every entry — picker lambdas update in place
        if (_host.empty()) _host    = _ws.host();
        if (_port.empty()) _port    = std::string(_ws.port());
        if (_path.empty()) _path    = _ws.path();
        _dirty = true;
    }

    bool needsContinuousUpdate() const override { return false; }
    bool wantsPop()              const override { return _wantsPop; }

    void onButtonUp() override {
        _cursor = (_cursor - 1 + FIELD_COUNT) % FIELD_COUNT;
        _dirty  = true;
    }

    void onButtonDown() override {
        _cursor = (_cursor + 1) % FIELD_COUNT;
        _dirty  = true;
    }

    void onButtonSelect() override {
        switch (_cursor) {
            case 0: _pushHostPicker(); break;
            case 1: _pushPortPicker(); break;
            case 2: _pushPathPicker(); break;
        }
    }

    void onLongPressSelect() override {
        if (_host.empty()) {
            _hint  = "Host required";
            _dirty = true;
            return;
        }
        int port = std::stoi(_port.empty() ? "3000" : _port);
        if (port <= 0 || port > 65535) port = 3000;
        _ws.saveConfig(_host, port, _path.empty() ? "/ws" : _path);
        _wantsPop = true;
    }

    void onButtonBack() override {}   // UIManager pops naturally

    void update() override {
        if (!_dirty) return;
        _dirty = false;
        _draw();
    }

private:
    static constexpr int FIELD_COUNT = 3;
    static constexpr int ROW_H       = 13;
    static constexpr int LIST_Y      = 12;

    DisplayManager&   _display;
    UIManager&        _ui;
    WebSocketManager& _ws;

    std::string _host;
    std::string _port;
    std::string _path;
    std::string _hint;
    int    _cursor   = 0;
    bool   _wantsPop = false;

    void _pushHostPicker() {
        _hint = "";
        _ui.pushScreen(new CharPickerWidget(
            _display, "Server Host", _host,
            [this](const std::string& r) { _host = r; _dirty = true; }
        ));
    }

    void _pushPortPicker() {
        _hint = "";
        _ui.pushScreen(new CharPickerWidget(
            _display, "Server Port", _port,
            [this](const std::string& r) { _port = r; _dirty = true; }
        ));
    }

    void _pushPathPicker() {
        _hint = "";
        _ui.pushScreen(new CharPickerWidget(
            _display, "Server Path", _path,
            [this](const std::string& r) { _path = r; _dirty = true; }
        ));
    }

    void _draw() {
        auto& u = _display.raw();
        u.clearBuffer();
        u.setFont(u8g2_font_5x7_tr);

        u.drawStr(2, 8, "Server Config");
        u.drawLine(0, 9, 127, 9);

        const char* labels[FIELD_COUNT] = { "Host", "Port", "Path" };
        std::string values[FIELD_COUNT] = {
            _host.empty() ? "(not set)" : _host,
            _port.empty() ? "3000"      : _port,
            _path.empty() ? "/ws"       : _path
        };

        for (int i = 0; i < FIELD_COUNT; i++) {
            int  y   = LIST_Y + i * ROW_H;
            bool sel = (i == _cursor);

            if (sel) {
                u.drawRBox(0, y, 127, ROW_H - 1, 1);
                u.setDrawColor(0);
                u.setFontMode(1);
            }

            u.drawStr(4, y + 9, labels[i]);
            int vw = u.getStrWidth(values[i].c_str());
            u.drawStr(125 - vw, y + 9, values[i].c_str());

            if (sel) {
                u.setDrawColor(1);
                u.setFontMode(0);
            }
        }

        u.drawLine(0, 53, 127, 53);
        if (!_hint.empty()) {
            u.drawStr(2, 63, _hint.c_str());
        } else {
            u.drawStr(2, 63, "[SEL] Edit  [LSEL] Save");
        }

        u.sendBuffer();
    }
};