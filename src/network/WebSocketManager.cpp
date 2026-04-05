#include "WebSocketManager.h"
#include "EventBus.h"
#include "NotificationManager.h"
#include "config.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "cJSON.h"

#include <cstring>
#include <string>

const char* WebSocketManager::TAG = "WebSocketManager";

// ============================================================
//  init
// ============================================================

void WebSocketManager::init() {
    loadConfig();
    ESP_LOGI(TAG, "Ready — %s:%d%s", _host.c_str(), _port, _path.c_str());
}

// ============================================================
//  update 
// ============================================================
void WebSocketManager::update() {
    switch(_state){

        // ── Waiting for WiFi — AshuraCore only calls us when WiFi up ──
        case WebSocketState::IDLE:
            if(hasConfig()){
                ESP_LOGI(TAG, "WiFi up, starting connection");
                _state              = WebSocketState::CONNECTING;
                _attemptInFlight    = false;
                _retryAfter         = 0;
            }
            break;

        case WebSocketState::CONNECTING: {
            uint64_t now = millis();

            // 1. Sitting out backoff delay
            if(now < _retryAfter) break;

            // 2. Attempt in flight — check for timeout
            if(_attemptInFlight){
                if(now - _attemptStart < Config::WebSocket::CONNECT_TIMEOUT) break;

                // Timed out — onEvent(ConnectionOpened) never fired
                _attemptInFlight = false;
                _attempts++;
                
                ESP_LOGW(TAG, "Attempt %d/%d timed out", _attempts, Config::WebSocket::MAX_ATTEMPTS);

                if (_attempts >= Config::WebSocket::MAX_ATTEMPTS) {
                    _onFailed();
                } else {
                    uint64_t delay = _nextBackoff();
                    _retryAfter = now + delay;
                    ESP_LOGI(TAG, "Backoff %llu s", delay / 1000ULL);
                }
                break;
            }

            // 3. Nothing in flight, backoff elapsed — fire attempt
            ESP_LOGI(TAG, "Attempt %d/%d", _attempts + 1, Config::WebSocket::MAX_ATTEMPTS);
            _beginConnect();
            break;
        }

        // ── Socket open, waiting for registration ──────────────
        case WebSocketState::CONNECTED:
            // Waiting for registration ack
            // If _connected flag drops, socket closed before registration
            if (!_connected) {
                ESP_LOGW(TAG, "Dropped before registration");
                _attempts++;
                _attemptInFlight = false;
                if (_client) {
                    esp_websocket_client_destroy(_client);
                    _client = nullptr;
                }
                if (_attempts >= Config::WebSocket::MAX_ATTEMPTS) {
                    _onFailed();
                } else {
                    uint64_t wait = _nextBackoff();
                    _retryAfter = millis() + wait;
                    _state      = WebSocketState::CONNECTING;
                    Bus().publish(AppEvent::WebSocketDisconnected);
                }
            }
            break;
            
        // ── Fully operational ──────────────────────────────────
        case WebSocketState::REGISTERED: {
            if (!_connected) {
                // Dropped — restart with fresh backoff
                ESP_LOGW(TAG, "Connection dropped");
                _state           = WebSocketState::CONNECTING;
                _attempts        = 0;
                _attemptInFlight = false;
                _retryAfter      = 0;
                if (_client) {
                    esp_websocket_client_destroy(_client);
                    _client = nullptr;
                }
                Bus().publish(AppEvent::WebSocketDisconnected);
                break;
            }

            // Heartbeat
            uint64_t now = millis();
            if (now - _lastHeartbeat > HEARTBEAT_INTERVAL) {
                _lastHeartbeat = now;
                _sendHeartbeat();
            }
            break;
        }

            
        // ── Gave up — waiting for manualRetry() ───────────────
        case WebSocketState::FAILED:
            break;
    }
}


// ============================================================
//  resetForWifi — called by AshuraCore when WiFi drops
// ============================================================

void WebSocketManager::resetForWifi() {
    _disconnect();
    _state           = WebSocketState::IDLE;
    _attempts        = 0;
    _retryAfter      = 0;
    _attemptInFlight = false;
    _connected       = false;
    ESP_LOGI(TAG, "Reset — WiFi dropped");
}


// ============================================================
//  Manual actions
// ============================================================

void WebSocketManager::manualRetry() {
    if (!hasConfig()) {
        ESP_LOGW(TAG, "manualRetry — no config");
        return;
    }
    ESP_LOGI(TAG, "Manual retry");
    _disconnect();
    _attempts        = 0;
    _attemptInFlight = false;
    _retryAfter      = 0;
    _connected       = false;
    _state           = WebSocketState::CONNECTING;
}

void WebSocketManager::saveConfig(const std::string& host, int port, const std::string& path) {
    _host = host;
    _port = port;
    _path = path;

    nvs_handle_t handle;
    if (nvs_open("network", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_str(handle, "net_ws_host", host.c_str());
        nvs_set_i32(handle, "net_ws_port", (int32_t)port);
        nvs_set_str(handle, "net_ws_path", path.c_str());
        nvs_commit(handle);
        nvs_close(handle);
    }

    ESP_LOGI(TAG, "Config saved: %s:%d%s",
             host.c_str(), port, path.c_str());

    _disconnect();
    _attempts        = 0;
    _attemptInFlight = false;
    _retryAfter      = 0;
    _connected       = false;
    _state           = WebSocketState::CONNECTING;
}

void WebSocketManager::send(const std::string& json) {
    if (_state != WebSocketState::REGISTERED &&
        _state != WebSocketState::CONNECTED) {
        ESP_LOGW(TAG, "send() dropped — not connected");
        return;
    }
    if (_client) {
        esp_websocket_client_send_text(
            _client,
            json.c_str(),
            (int)json.length(),
            pdMS_TO_TICKS(1000)
        );
    }
}


// ============================================================
//  NVS
// ============================================================

void WebSocketManager::loadConfig() {
    nvs_handle_t handle;
    if (nvs_open("network", NVS_READONLY, &handle) != ESP_OK) {
        ESP_LOGI(TAG, "No server config saved");
        return;
    }

    char   buf[128] = {};
    size_t len      = sizeof(buf);

    if (nvs_get_str(handle, "net_ws_host", buf, &len) == ESP_OK) {
        _host = buf;
    }

    int32_t port = 3000;
    if (nvs_get_i32(handle, "net_ws_port", &port) == ESP_OK) {
        _port = (int)port;
    }

    len = sizeof(buf);
    memset(buf, 0, sizeof(buf));
    if (nvs_get_str(handle, "net_ws_path", buf, &len) == ESP_OK) {
        _path = buf;
    }

    nvs_close(handle);

    if (!_host.empty()) {
        ESP_LOGI(TAG, "Config: %s:%d%s",
                 _host.c_str(), _port, _path.c_str());
    } else {
        ESP_LOGI(TAG, "No server config saved");
    }
}



// ============================================================
//  Private — connection management
// ============================================================

void WebSocketManager::_beginConnect() {
    // Build full URI: ws://host:port/path
    char uri[256];
    snprintf(uri, sizeof(uri), "ws://%s:%d%s", _host.c_str(), _port, _path.c_str());

    ESP_LOGI(TAG, "Connecting to: %s", uri);

    // Configure IDF websocket client
    esp_websocket_client_config_t ws_cfg = {};
    ws_cfg.uri                  = uri;
    ws_cfg.reconnect_timeout_ms = 0;   // we manage reconnect ourselves
    ws_cfg.network_timeout_ms   = (int)Config::WebSocket::CONNECT_TIMEOUT;
    ws_cfg.buffer_size          = 4096;
    ws_cfg.task_stack           = 8192;
    ws_cfg.task_prio            = 5;

    // Destroy previous client if any
    if (_client) {
        esp_websocket_client_destroy(_client);
        _client = nullptr;
    }

    _client = esp_websocket_client_init(&ws_cfg);

    // Register event handler — passes `this` as user data
    esp_websocket_register_events(
        _client,
        WEBSOCKET_EVENT_ANY,
        _wsEventHandler,
        (void*)this
    );

    esp_websocket_client_start(_client);

    _attemptStart    = millis();
    _attemptInFlight = true;
}

void WebSocketManager::_disconnect() {
    if (_client) {
        esp_websocket_client_stop(_client);
        esp_websocket_client_destroy(_client);
        _client = nullptr;
    }
    _connected = false;
}

void WebSocketManager::_onFailed() {
    _state           = WebSocketState::FAILED;
    _attemptInFlight = false;
    _disconnect();

    ESP_LOGE(TAG, "FAILED after %d attempts",
             Config::WebSocket::MAX_ATTEMPTS);

    NotifMgr().push(
        "Server Unavailable",
        "Could not connect after " +
            std::to_string(Config::WebSocket::MAX_ATTEMPTS) +
            " attempts. Go to Settings > Network to retry.",
        NotificationType::ALERT
    );

    Bus().publish(AppEvent::WebSocketFailed);
}

void WebSocketManager::_registerDevice() {
    // Use cJSON — available in IDF without extra libs
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type",       "register");
    cJSON_AddStringToObject(root, "deviceId",   DEVICE_ID);
    cJSON_AddStringToObject(root, "deviceType", "esp32");

    cJSON* data = cJSON_CreateObject();

    // Get IP
    esp_netif_t* netif =
        esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    char ip_str[16] = "0.0.0.0";
    if (netif) {
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(netif, &ip_info);
        esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));
    }

    // Get MAC
    uint8_t mac[6];
    char mac_str[18];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(mac_str, sizeof(mac_str),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    cJSON_AddStringToObject(data, "name", DEVICE_NAME);
    cJSON_AddStringToObject(data, "ip",   ip_str);
    cJSON_AddStringToObject(data, "mac",  mac_str);
    cJSON_AddItemToObject(root, "data", data);

    cJSON_AddNumberToObject(root, "timestamp", (double)millis());

    char* msg = cJSON_PrintUnformatted(root);
    if (msg) {
        send(std::string(msg));
        cJSON_free(msg);
    }
    cJSON_Delete(root);

    ESP_LOGI(TAG, "Registration sent");
}

void WebSocketManager::_sendHeartbeat() {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type",      "heartbeat");
    cJSON_AddStringToObject(root, "deviceId",  DEVICE_ID);
    cJSON_AddNumberToObject(root, "timestamp", (double)millis());

    char* msg = cJSON_PrintUnformatted(root);
    if (msg) {
        send(std::string(msg));
        cJSON_free(msg);
    }
    cJSON_Delete(root);

    ESP_LOGD(TAG, "Heartbeat sent");
}

void WebSocketManager::_onMessage(const std::string& data) {
    ESP_LOGI(TAG, "RX: %s", data.c_str());

    cJSON* doc = cJSON_Parse(data.c_str());
    if (!doc) {
        ESP_LOGW(TAG, "Invalid JSON");
        return;
    }

    cJSON* typeItem = cJSON_GetObjectItem(doc, "type");
    if (!typeItem || !cJSON_IsString(typeItem)) {
        cJSON_Delete(doc);
        return;
    }

    const char* type = typeItem->valuestring;

    if (strcmp(type, "status") == 0) {
        cJSON* dataObj   = cJSON_GetObjectItem(doc, "data");
        cJSON* statusObj = cJSON_GetObjectItem(dataObj, "status");

        if (statusObj && cJSON_IsString(statusObj) &&
            strcmp(statusObj->valuestring, "registered") == 0) {
            ESP_LOGI(TAG, "Registered");
            _state         = WebSocketState::REGISTERED;
            _attempts      = 0;
            _lastHeartbeat = millis();
            Bus().publish(AppEvent::WebSocketRegistered);
        }

    } else if (strcmp(type, "command")      == 0 ||
               strcmp(type, "notification") == 0) {
        _lastHeartbeat = millis();
        Bus().publish(AppEvent::CommandReceived, data);
    }

    cJSON_Delete(doc);
}


// ============================================================
//  Static IDF event handler
// ============================================================

void WebSocketManager::_wsEventHandler(
    void*            handler_args,
    esp_event_base_t base,
    int32_t          event_id,
    void*            event_data)
{
    // Recover `this` from handler_args
    WebSocketManager* self =
        static_cast<WebSocketManager*>(handler_args);

    esp_websocket_event_data_t* data =
        (esp_websocket_event_data_t*)event_data;

    switch (event_id) {

        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Socket opened");
            self->_connected       = true;
            self->_state           = WebSocketState::CONNECTED;
            self->_attemptInFlight = false;
            self->_attempts        = 0;
            Bus().publish(AppEvent::WebSocketConnected);
            self->_registerDevice();
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Socket closed");
            self->_connected = false;
            // update() handles state transition on next tick
            break;

        case WEBSOCKET_EVENT_DATA:
            if (data->op_code == 0x01 && // text frame
                data->data_len > 0) {
                std::string msg(
                    (char*)data->data_ptr,
                    data->data_len
                );
                self->_onMessage(msg);
            }
            break;

        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WebSocket error");
            self->_connected = false;
            break;

        default:
            break;
    }
}

// ============================================================
//  _nextBackoff
// ============================================================

uint64_t WebSocketManager::_nextBackoff() const {
    uint64_t b = Config::WebSocket::BACKOFF_BASE;
    for (int i = 0;
         i < _attempts && b < Config::WebSocket::BACKOFF_CAP;
         i++) {
        b *= 2;
    }
    return (b < Config::WebSocket::BACKOFF_CAP)
               ? b
               : (uint64_t)Config::WebSocket::BACKOFF_CAP;
}