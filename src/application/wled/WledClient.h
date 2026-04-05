#pragma once

#include <string>
#include <vector>
#include <cstring>

#include "esp_log.h"
#include "esp_http_client.h"

#include "config.h"
#include "application/wled/WledDevice.h"
#include "application/wled/WledState.h"


// ================================================================
//  WledClient  —  HTTP interface to one WLED device
//
//  All calls are blocking. Caller should show a loading indicator
//  before calling and hide it after.
//
//  Usage:
//    WledClient client;
//    client.setDevice(device);
//    WledState state;
//    if (client.fetchState(state)) { ... }
//    if (client.fetchEffects(effects)) { ... }
//    client.postJson(state.jsonPower());
// ================================================================

class WledClient {
    public:
        void setDevice(const WledDevice& dev){
            _device = dev;
        }

        const WledDevice& device() const { return _device; }

        bool hasDevice() const { return _device.isValid(); } 

        // ── Fetch current state ───────────────────────────────────
        // GET /json/state
        bool fetchState(WledState& out) {
            std::string body;
            if (!_get(_device.stateUrl(), body)) return false;

            cJSON* doc = cJSON_Parse(body.c_str());
            if (!doc) return false;

            cJSON* on  = cJSON_GetObjectItem(doc, "on");
            cJSON* bri = cJSON_GetObjectItem(doc, "bri");

            out.on         = on  ? cJSON_IsTrue(on)  : false;
            out.brightness = bri ? (uint8_t)bri->valueint : 128;

            cJSON* segs = cJSON_GetObjectItem(doc, "seg");
            if (segs && cJSON_IsArray(segs) && cJSON_GetArraySize(segs) > 0) {
                cJSON* seg0 = cJSON_GetArrayItem(segs, 0);
                if (seg0) {
                    cJSON* fx = cJSON_GetObjectItem(seg0, "fx");
                    cJSON* sx = cJSON_GetObjectItem(seg0, "sx");
                    cJSON* ix = cJSON_GetObjectItem(seg0, "ix");

                    out.effectIndex = fx ? fx->valueint : 0;
                    out.speed       = sx ? sx->valueint : 128;
                    out.intensity   = ix ? ix->valueint : 128;

                    cJSON* col = cJSON_GetObjectItem(seg0, "col");
                    if (col && cJSON_IsArray(col) &&
                        cJSON_GetArraySize(col) > 0) {
                        cJSON* col0 = cJSON_GetArrayItem(col, 0);
                        if (col0 && cJSON_IsArray(col0) &&
                            cJSON_GetArraySize(col0) >= 3) {
                            out.color.r = (uint8_t)cJSON_GetArrayItem(
                                            col0, 0)->valueint;
                            out.color.g = (uint8_t)cJSON_GetArrayItem(
                                            col0, 1)->valueint;
                            out.color.b = (uint8_t)cJSON_GetArrayItem(
                                            col0, 2)->valueint;
                        }
                    }
                }
            }

            out.valid = true;
            cJSON_Delete(doc);
            ESP_LOGI(TAG, "State fetched from %s", _device.ip.c_str());
            return true;
        }

        // ── Fetch effects list ────────────────────────────────────
        bool fetchEffects(std::vector<std::string>& out) {
            std::string body;
            if (!_get(_device.jsonUrl(), body)) return false;

            cJSON* doc = cJSON_Parse(body.c_str());
            if (!doc) return false;

            out.clear();
            cJSON* effects = cJSON_GetObjectItem(doc, "effects");
            if (effects && cJSON_IsArray(effects)) {
                cJSON* fx;
                cJSON_ArrayForEach(fx, effects) {
                    if (cJSON_IsString(fx)) {
                        out.push_back(std::string(fx->valuestring));
                    }
                }
            }

            cJSON_Delete(doc);
            ESP_LOGI(TAG, "%d effects fetched", (int)out.size());
            return true;
        }

        // ── Fetch device info (name) ──────────────────────────────
        bool fetchInfo(std::string& outName) {
            std::string body;
            if (!_get(_device.infoUrl(), body)) return false;

            cJSON* doc = cJSON_Parse(body.c_str());
            if (!doc) return false;

            cJSON* name = cJSON_GetObjectItem(doc, "name");
            if (name && cJSON_IsString(name)) {
                outName = name->valuestring;
            } else {
                outName = _device.ip;
            }

            cJSON_Delete(doc);
            return true;
        }

        // ── Send partial JSON ─────────────────────────────────────
        bool postJson(const std::string& json) {
            if (!_device.isValid()) return false;
            ESP_LOGI(TAG, "POST %s", json.c_str());
            return _post(_device.stateUrl(), json);
        }

        // ── Convenience senders ───────────────────────────────────
        bool setPower(bool on) {
            WledState s;
            s.on = on;
            return postJson(s.jsonPower());
        }

        bool setBrightness(uint8_t bri) {
            WledState s;
            s.brightness = bri;
            return postJson(s.jsonBrightness());
        }

        bool setEffect(int idx) {
            WledState s;
            s.effectIndex = idx;
            return postJson(s.jsonEffect());
        }

        bool setSpeed(uint8_t sx, uint8_t ix) {
            WledState s;
            s.speed     = sx;
            s.intensity = ix;
            return postJson(s.jsonSpeed());
        }

        bool setColor(WledColor c) {
            WledState s;
            s.color = c;
            return postJson(s.jsonColor());
        }

        const std::string& lastError() const { return _lastError; }

    private:
        WledDevice  _device;
        String      _lastError;

        static constexpr const char* TAG = "WledClient";

        // Perform HTTP GET request and return response body if successful
        bool _get(const std::string& url, std::string& body) {
            // Response buffer — WLED responses are small
            // but effects list can be large, so allocate in PSRAM
            const size_t BUF_SIZE = 32768;
            char* buf = (char*)heap_caps_malloc(
                BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!buf) {
                // Fallback to internal RAM if PSRAM unavailable
                buf = (char*)malloc(BUF_SIZE);
            }
            if (!buf) {
                _lastError = "OOM";
                return false;
            }
            memset(buf, 0, BUF_SIZE);

            esp_http_client_config_t config = {};
            config.url             = url.c_str();
            config.timeout_ms      = WLED_HTTPCLIENT_TIMEOUT;
            config.user_data       = buf;
            config.buffer_size     = 2048;

            esp_http_client_handle_t client =
                esp_http_client_init(&config);

            // Register event handler to capture response body
            esp_http_client_register_events(
                client,
                HTTP_EVENT_ON_DATA,
                [](esp_http_client_event_t* evt) -> esp_err_t {
                    if (evt->event_id == HTTP_EVENT_ON_DATA &&
                        evt->user_data) {
                        // Append to buffer
                        char*  outBuf = (char*)evt->user_data;
                        size_t curLen = strlen(outBuf);
                        if (curLen + evt->data_len < 32767) {
                            memcpy(outBuf + curLen,
                                evt->data,
                                evt->data_len);
                        }
                    }
                    return ESP_OK;
                },
                nullptr
            );

            esp_err_t err    = esp_http_client_perform(client);
            int       status = esp_http_client_get_status_code(client);
            esp_http_client_cleanup(client);

            if (err == ESP_OK && status == 200) {
                body = std::string(buf);
                free(buf);
                return true;
            }

            _lastError = "GET " + url + " -> " + std::to_string(status);
            ESP_LOGE(TAG, "%s", _lastError.c_str());
            free(buf);
            return false;
        }

        // Perform HTTP POST request with JSON payload
        bool _post(const std::string& url, const std::string& json) {
            esp_http_client_config_t config = {};
            config.url        = url.c_str();
            config.method     = HTTP_METHOD_POST;
            config.timeout_ms = WLED_HTTPCLIENT_TIMEOUT;

            esp_http_client_handle_t client =
                esp_http_client_init(&config);

            esp_http_client_set_header(
                client, "Content-Type", "application/json");

            esp_http_client_set_post_field(
                client,
                json.c_str(),
                (int)json.length()
            );

            esp_err_t err    = esp_http_client_perform(client);
            int       status = esp_http_client_get_status_code(client);
            esp_http_client_cleanup(client);

            if (err == ESP_OK && (status == 200 || status == 204)) {
                return true;
            }

            _lastError = "POST " + url + " -> " + std::to_string(status);
            ESP_LOGE(TAG, "%s", _lastError.c_str());
            return false;
        }
};