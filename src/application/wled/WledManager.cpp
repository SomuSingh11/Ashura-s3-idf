#include "WledManager.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <algorithm>
#include <string>
#include <cstring>

static const char* TAG = "WledManager";

// ================================================================
// WledManager is responsible for:
//         1. Storing discovered devices
//         2. Selecting an active device
//         3. Talking to it through _client
//         4. Caching its state and effects
//         5. Persisting everything in NVS (ESP Preferences)
//  So this class is:
//          "Device registry + persistence layer + active-session manager"
// ================================================================


// ── Lifecycle ─────────────────────────────────────────────────

void WledManager::begin() {
    loadFromNVS();
    ESP_LOGI(TAG, "Ready — %d saved device(s)", (int)_devices.size());
}


// ── Devices ───────────────────────────────────────────────────

const WledDevice* WledManager::activeDevice() const {
    if(_activeIndex < 0 || _activeIndex >= (int)_devices.size()) return nullptr;
    return &_devices[_activeIndex];
}

std::string WledManager::effectName(int index) const {
    if(index < 0 || index >= (int)_effects.size()) return "Unknown";
    return _effects[index];
}


// ── Connect ───────────────────────────────────────────────────

bool WledManager::connect(int index){
    if(index < 0 || index >= (int)_devices.size()){
        ESP_LOGW(TAG, "connect() — invalid index %d", index);
        return false;
    }

    _activeIndex = index;
    _client.setDevice(_devices[index]);

    _effects.clear();
    _state.valid = false;

    ESP_LOGI(TAG, "Connecting to %s @ %s", _devices[index].name.c_str(), _devices[index].ip.c_str());

    // Fetch current state
    // _State -> populated | _state.valid = true
    if(!_client.fetchState(_state)){
        ESP_LOGE(TAG, "Failed to fetch state");
        return false;
    }

    // Fetch effects list and cache it
    if (_client.fetchEffects(_effects)) {
        ESP_LOGI(TAG, "Fetched %d effects", (int)_effects.size());
    } else {
        ESP_LOGW(TAG, "Warning: could not load effects list");
    }

    // Persist active index
    nvs_handle_t handle;
    if (nvs_open("wled", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_i32(handle, "activeIdx", (int32_t)_activeIndex);
        nvs_commit(handle);
        nvs_close(handle);
    }

    return true;
}


// ── Discovery ─────────────────────────────────────────────────

int WledManager::discover() {
    // Scan for devices, merge with existing list, persist new list.
    ESP_LOGI(TAG, "Starting discovery...");

    std::vector<WledDevice> found;
    WledDiscovery::scan(found, _client);

    int newDevicesCount = 0;

    for(WledDevice dev: found){
        if(!_hasDevice(dev.ip)){
            if((int)_devices.size() < WLED_MAX_DEVICES) {
                _devices.push_back(dev);
                newDevicesCount++;
                ESP_LOGI(TAG, "New device added: %s @ %s", dev.name.c_str(), dev.ip.c_str());
            } else {
                ESP_LOGW(TAG, "Device limit reached, skipping: %s @ %s", dev.name.c_str(), dev.ip.c_str());
            }
        } else {
            // Update existing device info (e.g. name) in case it changed
            for(WledDevice& existing: _devices){
                if(existing.ip == dev.ip){
                    existing.name = dev.name;
                    break;
                }
            }
            ESP_LOGI(TAG, "Device already known, skipping: %s @ %s", dev.name.c_str(), dev.ip.c_str());
        }
    }

    if(newDevicesCount > 0) {
        saveToNVS();
    }

    ESP_LOGI(TAG, "Discovery done — %d new device(s)", newDevicesCount);
    return newDevicesCount;

}


// ── Remove device ─────────────────────────────────────────────

void WledManager::removeDevice(int index) {
    if(index < 0|| index >= (int)_devices.size()) return;

    ESP_LOGI(TAG, "Removing: %s", _devices[index].name.c_str());
    _devices.erase(_devices.begin() + index);

    // clamp active index
    if(_activeIndex >= (int)_devices.size()) {
        _activeIndex = std::max(0, (int)_devices.size() - 1);
    }

    saveToNVS();
}


// ── NVS ───────────────────────────────────────────────────────

void WledManager::loadFromNVS() {
    nvs_handle_t handle;
    if (nvs_open("wled", NVS_READONLY, &handle) != ESP_OK) {
        ESP_LOGI(TAG, "No saved devices");
        return;
    }

    int32_t activeIdx = 0;
    nvs_get_i32(handle, "activeIndex", &activeIdx);
    _activeIndex = (int)activeIdx;

    int32_t deviceCount = 0;
    nvs_get_i32(handle, "deviceCount", &deviceCount);
    deviceCount = std::min((int)deviceCount, WLED_MAX_DEVICES);

    _devices.clear();

    for (int i = 0; i < deviceCount; i++) {
        WledDevice dev;

        // Build keys
        std::string ipKey   = "dev_" + std::to_string(i) + "_ip";
        std::string nameKey = "dev_" + std::to_string(i) + "_name";
        std::string portKey = "dev_" + std::to_string(i) + "_port";

        char buf[128] = {};
        size_t len = sizeof(buf);

        if (nvs_get_str(handle, ipKey.c_str(),
                        buf, &len) == ESP_OK) {
            dev.ip = std::string(buf);
        }

        len = sizeof(buf);
        memset(buf, 0, sizeof(buf));
        if (nvs_get_str(handle, nameKey.c_str(),
                        buf, &len) == ESP_OK) {
            dev.name = std::string(buf);
        }

        int32_t port = 80;
        nvs_get_i32(handle, portKey.c_str(), &port);
        dev.port = (int)port;

        if (dev.isValid()) {
            _devices.push_back(dev);
        }
    }

    nvs_close(handle);

    // Clamp active index to valid range
    if (_activeIndex >= (int)_devices.size()) _activeIndex = 0;

    ESP_LOGI(TAG, "Loaded %d devices", (int)_devices.size());
}

void WledManager::saveToNVS() {
    nvs_handle_t handle;
    if (nvs_open("wled", NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace");
        return;
    }

    int count = std::min((int)_devices.size(), WLED_MAX_DEVICES);

    nvs_set_i32(handle, "activeIndex", (int32_t)_activeIndex);
    nvs_set_i32(handle, "deviceCount", (int32_t)count);

    for (int i = 0; i < count; i++) {
        const WledDevice& dev = _devices[i];

        std::string ipKey   = "dev_" + std::to_string(i) + "_ip";
        std::string nameKey = "dev_" + std::to_string(i) + "_name";
        std::string portKey = "dev_" + std::to_string(i) + "_port";

        nvs_set_str(handle, ipKey.c_str(),   dev.ip.c_str());
        nvs_set_str(handle, nameKey.c_str(), dev.name.c_str());
        nvs_set_i32(handle, portKey.c_str(), (int32_t)dev.port);
    }

    nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGI(TAG, "Saved %d devices", count);
}

// ── Private helpers ───────────────────────────────────────────

bool WledManager::_hasDevice(const std::string& ip) const {
    for (const WledDevice& dev : _devices) {
        if (dev.ip == ip) return true;
    }
    return false;
}