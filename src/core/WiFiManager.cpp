#include "WiFiManager.h"
#include "EventBus.h"
#include "NotificationManager.h"
#include "config.h"

#include "nvs.h"            // Low-level NVS API (read/write key-value pairs)
#include "nvs_flash.h"      // NVS flash init/erase (must call at boot)
#include "esp_wifi.h"       // Core WiFi driver (init, connect, config)
#include "esp_netif.h"      // Network interface (IP handling, DHCP)
#include "lwip/ip4_addr.h"  // IPv4 address utilities (ip4_addr_t, formatting)

#include <cstring>
#include <string>

const char* WiFiManager::TAG = "WiFiManager";


// ================================================================
//  init  —  Load credentials from NVS, start connecting if available
// ================================================================

void WiFiManager::init() {
    esp_netif_init();           // Initialize TCP/IP stack — safe to call multiple times

    // Create default event loop if not already created
    // AshuraCore calls nvs_flash_init before us so NVS is ready
    esp_event_loop_create_default();

    // Create default STA interface
    _netif = esp_netif_create_default_wifi_sta();

    // Init WiFi driver with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    // Set station mode, no auto-reconnect
    // We manage reconnect ourselves via state machine
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_ps(WIFI_PS_NONE); // disable power save for lower latency

    // Start WiFi — must be called before connect
    esp_wifi_start();

    loadCredentials();

    if (!_ssid.empty()) {
        ESP_LOGI(TAG, "Credentials found, connecting to: %s", _ssid.c_str());
        _startConnecting();
    } else {
        ESP_LOGI(TAG, "No credentials — IDLE");
        _state = NetState::IDLE;
    }
}

// ============================================================
//  update — called every loop tick
// ============================================================

void WiFiManager::update() {
    switch(_state){
        case NetState::IDLE:
        case NetState::FAILED:
            // Do nothing - waiting for manual retry
            break;     
        
        case NetState::CONNECTED: {
            if(!_checkConnected()){
                ESP_LOGW(TAG, "Connection lost");
                _state              = NetState::LOST;
                _attempts           = 0;
                _attemptInFlight    = false;
                _retryAfter         = 0;
                Bus().publish(AppEvent::WifiDisconnected);
            }
            break;
        }

        case NetState::CONNECTING:
        case NetState::LOST: {
            // 1. Check if WiFi came up
            if (_checkConnected()) {
                _onConnected();
                break;
            }

            uint64_t now = millis();

            // 2. Sitting out a backoff delay — do nothing
            if (now < _retryAfter) break;


            // 3. Attempt is in flight — check for timeout
            if (_attemptInFlight) {
                if (now - _attemptStart < Config::WiFi::CONNECT_TIMEOUT) break; // still waiting

                // Timed out
                _attemptInFlight = false;
                _attempts++;
                ESP_LOGW(TAG, "Attempt %d/%d timed out", _attempts, Config::WiFi::MAX_ATTEMPTS);

                if (_attempts >= Config::WiFi::MAX_ATTEMPTS) {
                    _onFailed();
                } else {
                    uint64_t wait = _nextBackoff();
                    _retryAfter = now + wait;
                    ESP_LOGI(TAG, "Backoff %llu s", wait / 1000ULL);
                }
                break;
            }

            // 4. Nothing in flight, backoff elapsed — fire next attempt
            ESP_LOGI(TAG, "Attempt %d/%d", _attempts + 1, Config::WiFi::MAX_ATTEMPTS);
            
            wifi_config_t wifi_config = {};
            strncpy((char*)wifi_config.sta.ssid, _ssid.c_str(), sizeof(wifi_config.sta.ssid) - 1);
            strncpy((char*)wifi_config.sta.password, _password.c_str(), sizeof(wifi_config.sta.password) - 1);
            
            // Threshold for connecting — WPA2 only by default
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

            esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            esp_wifi_connect();

            _attemptStart    = now;
            _attemptInFlight = true;
            break;
        }
    }
}


// ============================================================
//  Manual actions
// ============================================================

void WiFiManager::manualRetry() {
    if(_ssid.length() == 0) {
        ESP_LOGW(TAG, "manualRetry — no credentials");
        return;
    }

    ESP_LOGI(TAG, "Manual retry");
    _attempts        = 0;
    _attemptInFlight = false;
    _retryAfter      = 0;
    _startConnecting();
}

void WiFiManager::forget() {
    ESP_LOGI(TAG, "Forgetting credentials");
    esp_wifi_disconnect();
    
    _ssid            = "";
    _password        = "";
    _state           = NetState::IDLE;
    _attempts        = 0;
    _attemptInFlight = false;

    nvs_handle_t handle;
    if(nvs_open("network", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_erase_key(handle, "net_ssid");
        nvs_erase_key(handle, "net_pass");
        nvs_commit(handle);
        nvs_close(handle);
    }

    Bus().publish(AppEvent::WifiIdle);
}

void WiFiManager::saveCredentials(const std::string& ssid, const std::string& pass) {
    _ssid       = ssid;
    _password   = pass;

    nvs_handle_t handle;
    if(nvs_open("network", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_str(handle, "net_ssid", _ssid.c_str());
        nvs_set_str(handle, "net_pass", _password.c_str());
        nvs_commit(handle);
        nvs_close(handle);
    }

    ESP_LOGI(TAG, "Credentials saved for: %s", ssid.c_str());

    // Immediately start connecting
    _attempts        = 0;
    _attemptInFlight = false;
    _retryAfter      = 0;
    _startConnecting(); 
}


// ============================================================
//  NVS
// ============================================================

void WiFiManager::loadCredentials(){
    nvs_handle_t handle;
    if (nvs_open("network", NVS_READONLY, &handle) != ESP_OK) {
        ESP_LOGI(TAG, "No saved credentials");
        return;
    }

    char buf[64] = {};
    size_t len   = sizeof(buf);

    if (nvs_get_str(handle, "net_ssid", buf, &len) == ESP_OK) {
        _ssid = buf;
    }

    len = sizeof(buf);
    memset(buf, 0, sizeof(buf));
    if (nvs_get_str(handle, "net_pass", buf, &len) == ESP_OK) {
        _password = buf;
    }

    nvs_close(handle);

    if (!_ssid.empty()) {
        ESP_LOGI(TAG, "Loaded SSID: %s", _ssid.c_str());
    } else {
        ESP_LOGI(TAG, "No saved SSID");
    }
}

// ============================================================
//  State accessors
// ============================================================

std::string WiFiManager::localIp() const {
    if (!_netif) return "0.0.0.0";

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(_netif, &ip_info);

    char buf[16];
    esp_ip4addr_ntoa(&ip_info.ip, buf, sizeof(buf));
    return std::string(buf);
}

int WiFiManager::rssi() const {
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return ap_info.rssi;
    }
    return 0;
}


// ============================================================
//  Private
// ============================================================
bool WiFiManager::_checkConnected() {
    if (!_netif) return false;

    // esp_netif_get_ip_info is not safe to call from main task context
    // while WiFi driver is running on core 0.
    // Use wifi_ap_record which is thread-safe, combined with IP check.
    
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
        return false; // Not associated with any AP
    }

    // Associated with AP — now safely get IP
    esp_netif_ip_info_t ip_info = {};
    esp_netif_get_ip_info(_netif, &ip_info);

    return ip_info.ip.addr != 0;
}

void WiFiManager::_startConnecting(){
    _state = NetState::CONNECTING;

    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, _ssid.c_str(), sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, _password.c_str(), sizeof(wifi_config.sta.password) - 1);
    
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_connect();

    _attemptStart     = millis();
    _attemptInFlight  = true;
    _retryAfter       = 0;

    ESP_LOGI(TAG, "Connecting to: %s", _ssid.c_str());
}

void WiFiManager::_onConnected() {
    _state          = NetState::CONNECTED;
    _attempts       = 0;
    _attemptInFlight= false;

    std::string ip = localIp();
    ESP_LOGI(TAG, "Connected! IP: %s", ip.c_str());
    Bus().publish(AppEvent::WifiConnected, ip);
}

void WiFiManager::_onFailed() {
    _state   = NetState::FAILED;
    _attemptInFlight = false;
    esp_wifi_disconnect();
    
    ESP_LOGE(TAG, "Failed after %d attempts", _attempts);
    
    NotifMgr().push(
        "WiFi Unavailable",
        "Failed after " + std::to_string(Config::WiFi::MAX_ATTEMPTS) +
        " attempts. Go to Settings > Network to retry.",
        NotificationType::SYSTEM
    );
    Bus().publish(AppEvent::WifiFailed);
}

uint64_t WiFiManager::_nextBackoff() {
    // 2s 4s 8s 16s 32s 60s 60s 60s...
    uint64_t backoff = Config::WiFi::BACKOFF_BASE;
    for (int i = 0; i < _attempts && backoff < Config::WiFi::BACKOFF_CAP; i++) {
        backoff *= 2;
    }
    
    return (backoff < Config::WiFi::BACKOFF_CAP)
                ? backoff
                : (uint64_t)Config::WiFi::BACKOFF_CAP;
}