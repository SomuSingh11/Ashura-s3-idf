#pragma once

#include <string>
#include <functional>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"

#include "config.h"
#include "core/hal.h"


// ================================================================
//  WebSocketManager  —  State machine WS with exponential backoff
//
//  States:
//    IDLE          waiting for WiFi, not trying
//    CONNECTING    attempt in flight, backoff running
//    CONNECTED     socket open, registration pending
//    REGISTERED    fully operational, heartbeat running
//    FAILED        gave up after MAX_ATTEMPTS
//
//  Backoff: 1s 2s 4s 8s 16s 30s → FAILED after MAX_ATTEMPTS
//
//  AshuraCore responsibilities:
//    - Call update() only when WiFi is CONNECTED
//    - Call resetForWifi() immediately when WiFi drops
//    - Call manualRetry() from Settings screen
//
//  NVS keys (namespace "network"):
//    net_ws_host  → server hostname or IP
//    net_ws_port  → server port  (default 3000)
//    net_ws_path  → server path  (default "/ws")
// ================================================================

enum class WebSocketState {
    IDLE,
    CONNECTING,
    CONNECTED,
    REGISTERED,
    FAILED
};

class WebSocketManager {
    public: 

        // ── Lifecycle ─────────────────────────────────────────────
        void init();
        void update();
        void resetForWifi();  // call immediately when WiFi drops

        // ── Manual actions ────────────────────────────────────────
        void manualRetry();   // Settings -> Retry
        void saveConfig(const std::string& host, int port, const std::string& path);

        // ── Send ──────────────────────────────────────────────────
        void send(const std::string& json);

        // ── State ─────────────────────────────────────────────────
        WebSocketState  webSocketState()    const { return _state; }
        bool            isRegistered()      const { return _state == WebSocketState::REGISTERED; }
        int             attemptCount()      const { return _attempts; }
        std::string     host()              const { return _host; }
        int             port()              const { return _port; }
        std::string     path()              const { return _path; }
        bool            hasConfig()         const { return !_host.empty(); }

        // ── NVS ───────────────────────────────────────────────────
        void loadConfig();

    private: 
        // Internal state machine helpers
        void                _beginConnect();
        void                _disconnect();
        void                _onFailed();
        void                _registerDevice();
        void                _sendHeartbeat();
        void                _onMessage(const std::string& data);
        uint64_t            _nextBackoff() const;

        // IDF WebSocket event handler — must be static for C callback
        static void _wsEventHandler(
            void*                       handler_args,
            esp_event_base_t            base,
            int32_t                     event_id,
            void*                       event_data
        );

        esp_websocket_client_handle_t _client           = nullptr;
        WebSocketState                _state            = WebSocketState::IDLE;
        int                           _attempts         = 0;
        uint64_t                      _retryAfter       = 0;
        uint64_t                      _attemptStart     = 0;
        bool                          _attemptInFlight  = false;
        uint64_t                      _lastHeartbeat    = 0;
        bool                          _connected        = false; // set by event handler

        std::string _host;
        int         _port = 3000;
        std::string _path = "/ws";

        static const char* TAG;
};