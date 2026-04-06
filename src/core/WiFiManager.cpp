#include "WiFiManager.h"
#include "EventBus.h"
#include "NotificationManager.h"
#include "config.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "lwip/ip4_addr.h"

#include <cstring>
#include <string>

const char* WiFiManager::TAG = "WiFiManager";


// ================================================================
//  init
// ================================================================

void WiFiManager::init() {
    esp_netif_init();
    esp_event_loop_create_default();
    

    _netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    // Register event handlers BEFORE esp_wifi_start()
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,    &WiFiManager::_wifiEventHandler, this);
    esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, &WiFiManager::_ipEventHandler,   this);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_wifi_start();

    //_debugScan();  // optional — helps confirm WiFi is working and shows nearby SSIDs

    loadCredentials();

    if (!_ssid.empty()) {
        ESP_LOGI(TAG, "Credentials found, connecting to: %s", _ssid.c_str());
        _startConnecting();
    } else {
        ESP_LOGI(TAG, "No credentials — IDLE");
        _state = NetState::IDLE;
    }
}


// ================================================================
//  update — called every loop tick
// ================================================================

void WiFiManager::update() {
    switch (_state) {
        case NetState::IDLE:
        case NetState::FAILED:
            break;

        case NetState::CONNECTED: {
            if (!_checkConnected()) {
                ESP_LOGW(TAG, "Connection lost");
                _state           = NetState::LOST;
                _attempts        = 0;
                _attemptInFlight = false;
                _retryAfter      = 0;
                esp_wifi_disconnect();
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

            // 3. Attempt is in flight — check for fast-fail or timeout
            if (_attemptInFlight) {

                // Fast-fail: driver sent DISCONNECTED event mid-attempt
                if (_driverRejected) {
                    _driverRejected  = false;
                    _attemptInFlight = false;
                    _attempts++;
                    ESP_LOGW(TAG, "Attempt %d/%d rejected by driver (reason: %d)",
                             _attempts, Config::WiFi::MAX_ATTEMPTS, _lastDisconnectReason);

                    esp_wifi_disconnect();
                    vTaskDelay(pdMS_TO_TICKS(200));

                    if (_attempts >= Config::WiFi::MAX_ATTEMPTS) {
                        _onFailed();
                    } else {
                        uint64_t wait = _nextBackoff();
                        _retryAfter = millis() + wait;
                        ESP_LOGI(TAG, "Backoff %llu s", wait / 1000ULL);
                    }
                    break;
                }

                // Timeout check
                if (now - _attemptStart < Config::WiFi::CONNECT_TIMEOUT) break;

                _attemptInFlight = false;
                _attempts++;
                ESP_LOGW(TAG, "Attempt %d/%d timed out", _attempts, Config::WiFi::MAX_ATTEMPTS);

                esp_wifi_disconnect();
                vTaskDelay(pdMS_TO_TICKS(200));

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

            esp_wifi_disconnect();
            vTaskDelay(pdMS_TO_TICKS(200));

            _applyConfigAndConnect();

            _attemptStart    = now;
            _attemptInFlight = true;
            break;
        }
    }
}


// ================================================================
//  Manual actions
// ================================================================

void WiFiManager::manualRetry() {
    if (_ssid.empty()) {
        ESP_LOGW(TAG, "manualRetry — no credentials");
        return;
    }

    ESP_LOGI(TAG, "Manual retry");
    _attempts        = 0;
    _attemptInFlight = false;
    _driverRejected  = false;
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
    _driverRejected  = false;

    nvs_handle_t handle;
    if (nvs_open("network", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_erase_key(handle, "net_ssid");
        nvs_erase_key(handle, "net_pass");
        nvs_commit(handle);
        nvs_close(handle);
    }

    Bus().publish(AppEvent::WifiIdle);
}

void WiFiManager::saveCredentials(const std::string& ssid, const std::string& pass) {
    _ssid     = ssid;
    _password = pass;

    nvs_handle_t handle;
    if (nvs_open("network", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_str(handle, "net_ssid", _ssid.c_str());
        nvs_set_str(handle, "net_pass", _password.c_str());
        nvs_commit(handle);
        nvs_close(handle);
    }

    ESP_LOGI(TAG, "Credentials saved for: %s", ssid.c_str());

    _attempts        = 0;
    _attemptInFlight = false;
    _driverRejected  = false;
    _retryAfter      = 0;
    _startConnecting();
}


// ================================================================
//  NVS
// ================================================================

void WiFiManager::loadCredentials() {
    nvs_handle_t handle;
    if (nvs_open("network", NVS_READONLY, &handle) != ESP_OK) {
        ESP_LOGI(TAG, "No saved credentials");
        return;
    }

    char buf[64] = {};
    size_t len   = sizeof(buf);

    if (nvs_get_str(handle, "net_ssid", buf, &len) == ESP_OK) _ssid = buf;

    len = sizeof(buf);
    memset(buf, 0, sizeof(buf));
    if (nvs_get_str(handle, "net_pass", buf, &len) == ESP_OK) _password = buf;

    nvs_close(handle);

    if (!_ssid.empty())
        ESP_LOGI(TAG, "Loaded SSID: %s", _ssid.c_str());
    else
        ESP_LOGI(TAG, "No saved SSID");
}


// ================================================================
//  State accessors
// ================================================================

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
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) return ap_info.rssi;
    return 0;
}


// ================================================================
//  Private
// ================================================================

bool WiFiManager::_checkConnected() {
    if (!_netif) return false;

    esp_netif_ip_info_t ip_info = {};
    esp_netif_get_ip_info(_netif, &ip_info);
    return (ip_info.ip.addr != 0);
}

// Single place that owns wifi_config — no duplication, no drift
void WiFiManager::_applyConfigAndConnect() {
    wifi_config_t wifi_config = {};

    strncpy((char*)wifi_config.sta.ssid, _ssid.c_str(), sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, _password.c_str(), sizeof(wifi_config.sta.password) - 1);

    // 🔥 MOST COMPATIBLE SETTINGS
    wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;

    wifi_config.sta.pmf_cfg.capable  = false;
    wifi_config.sta.pmf_cfg.required = false;

    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());
}

void WiFiManager::_startConnecting() {
    _state           = NetState::CONNECTING;
    _driverRejected  = false;

    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(200));

    _applyConfigAndConnect();

    _attemptStart    = millis();
    _attemptInFlight = true;
    _retryAfter      = 0;

    ESP_LOGI(TAG, "Connecting to: %s", _ssid.c_str());
}

void WiFiManager::_onConnected() {
    _state           = NetState::CONNECTED;
    _attempts        = 0;
    _attemptInFlight = false;
    _driverRejected  = false;

    std::string ip = localIp();
    ESP_LOGI(TAG, "Connected! IP: %s", ip.c_str());
    Bus().publish(AppEvent::WifiConnected, ip);
}

void WiFiManager::_onFailed() {
    _state           = NetState::FAILED;
    _attemptInFlight = false;
    _driverRejected  = false;
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
    uint64_t backoff = Config::WiFi::BACKOFF_BASE;
    for (int i = 0; i < _attempts && backoff < Config::WiFi::BACKOFF_CAP; i++) {
        backoff *= 2;
    }
    return (backoff < Config::WiFi::BACKOFF_CAP)
               ? backoff
               : (uint64_t)Config::WiFi::BACKOFF_CAP;
}

void WiFiManager::_wifiEventHandler(void* arg, esp_event_base_t base,
                                     int32_t id, void* data) {
    auto* self = static_cast<WiFiManager*>(arg);

    if (id == WIFI_EVENT_STA_DISCONNECTED) {
        auto* disc = static_cast<wifi_event_sta_disconnected_t*>(data);
        self->_lastDisconnectReason = disc->reason;

        ESP_LOGW(self->TAG, "WiFi disconnected event — reason: %d", disc->reason);

        // Reason codes to watch:
        //   2  = auth expired        → wrong password likely
        //   3  = deauth from AP      → AP rejected us
        //  15  = 4-way handshake TO  → wrong password
        // 202  = auth fail           → wrong password confirmed
        // 204  = auth fail           → wrong password confirmed
        // 205  = handshake timeout   → wrong password or interference

        if (self->_attemptInFlight) {
            self->_driverRejected = true;  // update() will fast-fail this attempt
        }
    }
}

void WiFiManager::_ipEventHandler(void* arg, esp_event_base_t base,
                                   int32_t id, void* data) {
    auto* self  = static_cast<WiFiManager*>(arg);
    auto* event = static_cast<ip_event_got_ip_t*>(data);
    ESP_LOGI(self->TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    // _checkConnected() now returns true — _onConnected() fires on next update() tick
}

// void WiFiManager::_debugScan() {
//     ESP_LOGI(TAG, "=== Starting WiFi scan ===");

//     wifi_scan_config_t scan_cfg = {};
//     scan_cfg.show_hidden = true;
//     esp_wifi_scan_start(&scan_cfg, true);  // blocking scan

//     uint16_t count = 0;
//     esp_wifi_scan_get_ap_num(&count);

//     wifi_ap_record_t* list = new wifi_ap_record_t[count];
//     esp_wifi_scan_get_ap_records(&count, list);

//     ESP_LOGI(TAG, "Found %d networks:", count);
//     for (int i = 0; i < count; i++) {
//         ESP_LOGI(TAG, "  [%d] SSID: '%s'  RSSI: %d  Auth: %d  Channel: %d",
//                  i, list[i].ssid, list[i].rssi,
//                  list[i].authmode, list[i].primary);
//     }

//     delete[] list;
//     ESP_LOGI(TAG, "=== Your target SSID: '%s' ===", _ssid.c_str());
// }