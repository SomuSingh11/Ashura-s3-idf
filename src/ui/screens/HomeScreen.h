#pragma once

#include "config.h"
#include "hal.h"
#include <string>
#include <ctime>
#include <cstdio>
#include <algorithm>  // std::min

#include "IScreen.h"
#include "../../core/DisplayManager.h"
#include "../../core/TimeManager.h"
#include "../../core/NotificationManager.h"

#include "../../companion/CompanionRenderer.h"


// ================================================================
//  HomeScreen  —  Ashura OS Desktop
//
//  128×64 layout:
//
//  ┌──────────────────────────────────────────────────────────────┐
//  │  Wed 05 Jan                    14:32              y=8        │
//  │                                  :45  ▓▓▓▓▓▓     y=18-20   │
//  │                                                              │
//  │  ╭──╮  ╭──╮                                                 │
//  │  │  │  │  │   companion eyes  x=0 y=28 w=56 h=26           │
//  │  ╰──╯  ╰──╯                                                 │
//  ├──────────────────────────────────────────────────────────────┤  y=53
//  │  · notification ticker ·            [● *]  ②    y=63        │
//  └──────────────────────────────────────────────────────────────┘
//
//  Status bar (y=54..63):
//    Left  — notification ticker (scrolls if long, fades after timeout)
//    Right — compound badge [WiFi WS] + unread dot
//
//  Badge symbols:
//    WiFi:  · idle   o trying   * connected   ! failed
//    WS:    - idle   o trying   ~ connected   * registered   ! failed
//
//  SELECT / DOWN → wantsMenu()
//  Idle > SCREENSAVER_TIMEOUT → wantsScreenSaver()
// ================================================================

static const char* const _dayName[] = { 
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static const char* const _monthName[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul","Aug", "Sep", "Oct", "Nov", "Dec"
};

class HomeScreen : public IScreen {
    public:
        explicit HomeScreen(DisplayManager& display, CompanionRenderer& companion) 
            : _display(display), _companion(companion) {};

        void onEnter() override {
            _lastInteraction    = millis();
            _tickerOffset       = 0;
            _lastTickerScroll   = millis();
        }

        bool needsContinuousUpdate() const override { return true; }

        // ── Called by AshuraCore ──────────────────────────────────
        void setConnectionStatus(const std::string& s){ _connectionStatus = s; }
        void setLastMessage(const std::string& msg){
            _lastMessage = msg;
            _lastMessageAt = millis();
            _tickerOffset = 0;                  // reset scroll to start on new message
        }

        // ── Signals polled by AshuraCore ─────────────────────────
        bool wantsMenu(){
            if(_wantsMenu) { _wantsMenu = false; return true; }
            return false;
        }
        bool wantsScreenSaver(){
            if(_wantsScreensaver){ _wantsScreensaver = false; return true; }
            return false;
        }

        // ── Buttons ──────────────────────────────────────────────
        void onButtonSelect() override  { _lastInteraction = millis(); _wantsMenu = true; }
        void onButtonDown()   override  { _lastInteraction = millis(); _wantsMenu = true; }
        void onButtonUp()     override  { _lastInteraction = millis(); }
        void onButtonBack()   override  { _lastInteraction = millis(); }

        
        // ── Render ───────────────────────────────────────────────
        void update() override {
            unsigned long now = millis();

            // 1. Screensaver trigger
            if(now - _lastInteraction > SCREENSAVER_TIMEOUT){ _wantsScreensaver = true; _lastInteraction = now; }

            // 2. Trigger Scroll - advance every 200 ms if message is too long
            if(!_lastMessage.empty() &&
                now - _lastMessageAt < (unsigned long) LASTMESSAGE_HOMESCREEN_TIMEOUT &&
                _lastMessage.length() > TICKER_MAX_CHARS &&
                now - _lastTickerScroll > TICKER_SCROLL_MS ) 
                {
                    _lastTickerScroll = now;
                    _tickerOffset++;
                    if(_tickerOffset > (int)_lastMessage.length()) _tickerOffset = 0;
                }

            auto& u = _display.raw();
            u.clearBuffer();

            // ── Companion eyes — left side, vertically centred ────
            _companion.draw(u.native(), 0, 28, 56, 26);

            u.setFont(u8g2_font_5x7_tr);

            // ── Date (top-left) ───────────────────────────────────
            if (Time().isSynced()) {
                // IDF: use time() + localtime_r instead of getLocalTime()
                time_t now_t;
                struct tm t = {};
                time(&now_t);
                localtime_r(&now_t, &t);

                char buf[14];
                snprintf(buf, sizeof(buf), "%s %02d %s",
                        _dayName[t.tm_wday],
                        t.tm_mday,
                        _monthName[t.tm_mon]);
                u.drawStr(0, 8, buf);
            } else {
                u.drawStr(0, 8, "Syncing...");
            }

            // ── HH:MM (top-right, large) ──────────────────────────
            u.setFont(u8g2_font_ncenB14_tr);
            char timeBuf[6];
            snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d",
                    Time().getHH(), Time().getMM());
            // Right-align: measure width, pin to x=127
            int tw = u.getStrWidth(timeBuf);
            u.drawStr(127 - tw, 16, timeBuf);

            // ── Seconds + progress bar (below time) ───────────────
            u.setFont(u8g2_font_5x7_tr);
            char secBuf[4];
            snprintf(secBuf, sizeof(secBuf), ":%02d", Time().getSS());
            int sw = u.getStrWidth(secBuf);
            u.drawStr(127 - sw, 24, secBuf);

            // Seconds bar — right-aligned, 40px wide
            int barFill = (Time().getSS() * 40) / 60;
            u.drawFrame(87, 26, 40, 2);
            if (barFill > 0) u.drawBox(87, 26, barFill, 2);

            // ── Divider above status bar ──────────────────────────
            u.drawLine(0, 53, 127, 53);

            // ── Status bar ────────────────────────────────────────
            _drawStatusBar(u, now);

            u.sendBuffer();
        }
        
    
    private:
        // ── Status bar constants ──────────────────────────────────
        static constexpr int  TICKER_MAX_CHARS = 18;
        static constexpr int  TICKER_SCROLL_MS = 200;
        static constexpr int  STATUS_Y         = 63;   // text baseline
        static constexpr int  BADGE_X          = 95;   // badge left edge

        void _drawStatusBar(U8G2& u, unsigned long now) {
            u.setFont(u8g2_font_5x7_tr);

            // ── Badge (right side) ────────────────────────────────
            // _connStatus is "[w s]" format from AshuraCore::_updateBadge()
            int bw = u.getStrWidth(_connectionStatus.c_str());
            u.drawStr(127 - bw, STATUS_Y, _connectionStatus.c_str());

            // ── Unread dot + count ────────────────────────────────
            int unread = NotifMgr().unreadCount();
            if (unread > 0) {
                char dot[4];
                snprintf(dot, sizeof(dot), "%d",std::min(unread, 9));
                // Small filled circle + count just left of badge
                u.drawDisc(BADGE_X - 6, STATUS_Y - 3, 3);
                u.setDrawColor(0);
                u.drawStr(BADGE_X - 8, STATUS_Y, dot);
                u.setDrawColor(1);
            }

            // ── Notification ticker (left + centre) ───────────────
            bool msgActive = !_lastMessage.empty() &&
                            now - _lastMessageAt < (unsigned long)LASTMESSAGE_HOMESCREEN_TIMEOUT;

            if (msgActive) {
                // Scroll window into message
                std::string visible;
                int len = _lastMessage.length();
                if (len <= TICKER_MAX_CHARS) {
                    visible = _lastMessage;
                } else {
                    int start = _tickerOffset % len;
                    // Wrap-around substring
                    if (start + TICKER_MAX_CHARS <= len) {
                        visible = _lastMessage.substr(start, TICKER_MAX_CHARS);
                    } else {
                        visible = _lastMessage.substr(start) +
                                " " +
                                _lastMessage.substr(0, TICKER_MAX_CHARS -
                                                        (len - start) - 1);
                    }
                }
                u.drawStr(0, STATUS_Y, visible.c_str());
            } else {
                // No active message — show subtle idle hint
                u.drawStr(0, STATUS_Y, "Ashura OS");
            }
        }

        // ── Members ───────────────────────────────────────────────
        DisplayManager&     _display;
        CompanionRenderer&  _companion;

        std::string         _connectionStatus   = "[- -]";
        std::string         _lastMessage;
        uint64_t            _lastMessageAt      = 0;
        uint64_t            _lastInteraction    = 0;
        uint64_t            _lastTickerScroll   = 0;
        int                 _tickerOffset       = 0;
        bool                _wantsMenu          = false;
        bool                _wantsScreensaver   = false;
};