#pragma once

#include <string>
#include <functional>
#include <vector>

// ============================================================
//  AppEvent  —  all system-wide events
// ============================================================
enum class AppEvent {
    // ── WiFi ──────────────────────────────────────────────────
    WifiConnected,              // just connected, payload = IP
    WifiDisconnected,           // dropped mid-session
    WifiFailed,                 // gave up after N attempts
    WifiIdle,                   // no credentials, never tried

    // ── WebSocket ─────────────────────────────────────────────
    WebSocketConnected,         // socket open, not yet registered
    WebSocketDisconnected,      // dropped
    WebSocketRegistered,        // fully operational
    WebSocketFailed,            // gave up after N attempts

    // ── Notifications ─────────────────────────────────────────
    NotificationPushed,         // new notification added to queue

    // ── Messaging ─────────────────────────────────────────────
    CommandReceived,
    NotificationReceived,
    SendMessage,          // for outgoing WebSocket messages

    // ── Input ─────────────────────────────────────────────────
    ButtonUp,
    ButtonDown,
    ButtonSelect,
    ButtonBack,

    // ── UI ────────────────────────────────────────────────────
    DisplayNeedsUpdate,
    ScreensaverStart,
    ScreensaverStop,

    // ── System ────────────────────────────────────────────────
    SystemTick,    // published every loop(), used by games/animations
    SystemBoot,    // published at the end of init()

    // ── Companion + Pomodoro ───────────────────────────────────
    PomodoroStarted,            // work session began   → companion FOCUSED
    PomodoroTick,               // every second while active, payload = remaining ms string
    PomodoroPaused,             // session paused
    PomodoroResumed,            // session resumed
    PomodoroCompleted,          // all sessions done   → companion HAPPY
    PomodoroAborted,            // user cancelled      → companion IDLE
    PomodoroBreak,              // break started, payload = "short" | "long" → companion HAPPY
};

struct EventSubscription {
    AppEvent event;
    std::function<void(const std::string& payload)> callback;
};

class EventBus {
    public: 
        static EventBus& instance(){
            static EventBus bus;
            return bus;
        }

    void subscribe(AppEvent event, std::function<void(const std::string& payload)> cb){
        _subscriptions.push_back({event, cb});
    }

    // subscribe without caring about payload
    void subscribe(AppEvent event, std::function<void()> cb){
         _subscriptions.push_back({ event, [cb](const std::string&) { cb(); } });
    } 

    void publish(AppEvent event, const std::string& payload = "") {
    for (auto& sub : _subscriptions) {
      if (sub.event == event) {
        sub.callback(payload);
      }
    }
  }

  private:
    EventBus() = default;
    std::vector<EventSubscription> _subscriptions;
};

// Global shorthand
inline EventBus& Bus() {return EventBus::instance();}
