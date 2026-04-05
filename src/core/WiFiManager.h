#pragma once

// ── ESP-IDF Networking Stack ───────────────────────────────
#include "esp_wifi.h"   // Core WiFi driver (init, start, connect, mode)
#include "esp_event.h"  // Event loop system (WiFi/IP events, callbacks)
#include "esp_netif.h"  // Network interface (IP, DHCP, TCP/IP integration)

#include "esp_log.h"
#include "esp_timer.h"

#include "config.h"
#include <string>

// ================================================================
//  WiFiManager  —  State machine WiFi with exponential backoff
//
//  States:
//    IDLE        no credentials saved, never tried
//    CONNECTING  actively attempting, backoff running
//    CONNECTED   WiFi up and running
//    LOST        was connected, dropped, reconnecting
//    FAILED      gave up after MAX_ATTEMPTS, needs manual retry
//
//  Backoff sequence (ms): 2000 4000 8000 16000 32000 60000 60000 60000
//  MAX_ATTEMPTS: 8  →  after that, state = FAILED, stops retrying
//
//  Boot behavior:
//    init() loads credentials from NVS.
//    If no SSID saved → stays IDLE (no spam, no false failures).
//    If SSID saved → starts CONNECTING immediately.
//
//  NVS keys (namespace "network"):
//    net_ssid   → WiFi SSID
//    net_pass   → WiFi password
// ================================================================

enum class NetState {
    IDLE,
    CONNECTING,
    CONNECTED, 
    LOST,
    FAILED 
};

class WiFiManager {
    public:
        // ── Lifecycle ─────────────────────────────────────────────
        void init();        // loads NVS creds, starts connecting if available
        void update();      // call every loop()

        // ── Manual actions ────────────────────────────────────────
        void manualRetry();         // Settings -> Retry
        void forget();              // Clear NVS -> go IDLE
        void saveCredentials(const std::string& ssid, 
                             const std::string& password); // Save + Trigger connect

        // ── State ─────────────────────────────────────────────────
        NetState    state()          const { return _state; }
        bool        isConnected()    const {return _state == NetState::CONNECTED; }
        int         attemptCount()   const { return _attempts; }
        std::string localIp()        const;
        std::string ssid()           const { return _ssid; }
        int         rssi()           const;
        bool        hasCredentials() const { return !_ssid.empty(); }

        // ── NVS ───────────────────────────────────────────────────
        void loadCredentials();

    private:
        void            _startConnecting();
        void            _onConnected();
        void            _onFailed();
        uint64_t        _nextBackoff();
        bool            _checkConnected();

        std::string     _ssid;
        std::string     _password;
        NetState        _state              = NetState::IDLE;
        int             _attempts           = 0;
        uint64_t        _retryAfter         = 0;        // millis() when next attempt is allowed
        uint64_t        _attemptStart       = 0;        // millis() when current WiFi.begin() fired
        bool            _attemptInFlight    = false;        

        // IDF netif handle — needed for IP queries
        esp_netif_t*  _netif            = nullptr;

        static const char* TAG;
};
