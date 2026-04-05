#pragma once
// ================================================================
//  SystemStatsScreen  —  Live device diagnostics
//
//  128×64 layout:
//
//  ┌──────────────────────────────────────────────────────────────┐
//  │  SYSTEM STATS                                               │
//  │  Uptime   00:04:32                                          │
//  │  Heap     142 KB free                                       │
//  │  WiFi     -62 dBm  192.168.1.42                            │
//  │  WS       Active                                            │
//  │  CPU      240 MHz   Flash 4MB                              │
//  │                                          [BCK] Close        │
//  └──────────────────────────────────────────────────────────────┘
//
//  Refreshes every 1s.
//  BACK or SELECT → pop
// ================================================================

#include "../IScreen.h"
#include "../../../core/DisplayManager.h"
#include "../../../core/TimeManager.h"

#include <string>
#include <esp_system.h>
#include <esp_spi_flash.h>
#include <esp_heap_caps.h>
#include <vector>
#include <algorithm>
#include "hal.h"
#include "esp_wifi.h"
#include "esp_netif.h"

class SystemStatsScreen : public IScreen {
public:
    explicit SystemStatsScreen(DisplayManager& d) : _display(d) {}

    void onEnter() override {
        _pos = 0;
        _winPos = 0;
        _lastRefresh = 0;
        _buildItems();
        _dirty = true;
    }

    bool needsContinuousUpdate() const override { return true; }
    bool wantsPop() const override { return _wantsPop; }

    void onButtonBack() override { _wantsPop = true; }
    void onButtonSelect() override { _wantsPop = true; }

    void onButtonUp() override {
        if (_pos > 0) {
            _pos--;
            if (_pos < _winPos) _winPos = _pos;
        } else {
            _pos = _items.size() - 1;
            _winPos = max(0, _pos - (SUBMENU_ITEMS_ON_SCREEN - 1));
        }
        _dirty = true;
    }

    void onButtonDown() override {
        if (_pos < (int)_items.size() - 1) {
            _pos++;
            if (_pos - _winPos > SUBMENU_ITEMS_ON_SCREEN - 2 &&
                _winPos < (int)_items.size() - SUBMENU_ITEMS_ON_SCREEN)
                _winPos++;
        } else {
            _pos = 0;
            _winPos = 0;
        }
        _dirty = true;
    }

    void update() override {
        uint64_t now = millis();
        if (now - _lastRefresh >= 1000) {
            _lastRefresh = now;
            _buildItems();
            _dirty = true;
        }

        if (!_dirty) return;

        auto& u = _display.raw();
        u.clearBuffer();
        u.setFont(u8g2_font_5x7_tr);

        for (int i = 0; i < SUBMENU_ITEMS_ON_SCREEN; i++) {
            int idx = _winPos + i;
            if (idx >= (int)_items.size()) break;

            int itemY = i * SUBMENU_ITEM_HEIGHT;
            int txtY  = itemY + 12;

            if (idx == _pos) {
                u.drawRBox(0, itemY + 1, 123, SUBMENU_ITEM_HEIGHT - 2, 1);
                u.setDrawColor(0);
                u.drawStr(6, txtY, _items[idx].c_str());
                u.setDrawColor(1);
            } else {
                u.drawStr(6, txtY, _items[idx].c_str());
            }
        }

        _drawScrollbar(u);
        u.sendBuffer();
        _dirty = false;
    }

private:

    DisplayManager&      _display;
    std::vector<std::string>  _items;
    wifi_ap_record_t ap_info;

    int           _pos        = 0;
    int           _winPos     = 0;
    uint64_t _lastRefresh = 0;
    bool          _wantsPop    = false;
    bool          _dirty       = true;

    void _buildItems() {
        _items.clear();

        uint64_t s = millis() / 1000;
        uint64_t mn = s / 60;
        uint64_t hr = mn / 60;

        char buf[64];

        // ---------------- DEVICE INFO ----------------
        snprintf(buf, sizeof(buf), "%-9s %s", "Device", DEVICE_NAME);
        _items.push_back(std::string(buf));

        snprintf(buf, sizeof(buf), "%-9s %s", "Dev ID", DEVICE_ID);
        _items.push_back(std::string(buf));


        // ---------------- DEVICE STATUS ----------------
        snprintf(buf, sizeof(buf),
         "%-9s %02lu:%02lu:%02lu",
         "Uptime",
         hr % 24, mn % 60, s % 60);
        _items.push_back(std::string(buf));

        snprintf(buf, sizeof(buf),
         "%-9s %s",
         "Restart",
         _resetReasonToString(esp_reset_reason()).c_str());
        _items.push_back(std::string(buf));

        snprintf(buf, sizeof(buf),
         "%-9s %s",
         "Clock",
         Time().isSynced() ? "NTP Sync" : "No Sync");
        _items.push_back(std::string(buf));

        // ---------------- NETWORK ----------------
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {

            // 1. Signal Strength (RSSI)
            snprintf(buf, sizeof(buf), "%-9s %d dBm", "Signal", ap_info.rssi);
            _items.push_back(buf);

            // 2. SSID
            snprintf(buf, sizeof(buf), "%-9s %s", "SSID", (char*)ap_info.ssid);
            _items.push_back(buf);

            // 3. IP Address (Requires Netif)
            esp_netif_ip_info_t ip_info;
            esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
                snprintf(buf, sizeof(buf), "%-9s " IPSTR, "IP", IP2STR(&ip_info.ip));
                _items.push_back(buf);
            }

            // 4. MAC Address
            uint8_t mac[6];
            esp_wifi_get_mac(WIFI_IF_STA, mac);
            snprintf(buf, sizeof(buf), "%-9s %02X:%02X:%02X:%02X:%02X:%02X", 
                    "MAC", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            _items.push_back(buf);

            // 5. WiFi Mode
            wifi_mode_t mode;
            esp_wifi_get_mode(&mode);
            const char* modeStr = (mode == WIFI_MODE_STA) ? "STA" : (mode == WIFI_MODE_AP) ? "AP" : "Off";
            snprintf(buf, sizeof(buf), "%-9s %s", "WiFi", modeStr);
            _items.push_back(buf);

        } else {
            _items.push_back("WiFi Disconnected");
        }

        // ---------------- PERFORMANCE ----------------

        uint32_t totalInternal =
            heap_caps_get_total_size(MALLOC_CAP_DEFAULT) / 1024;

        uint32_t freeInternal =
            heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024;

        snprintf(buf, sizeof(buf),
         "%-9s %lu/%lu KB",
         "RAM",
         freeInternal,
         totalInternal);
        _items.push_back(std::string(buf));


        // ---------- PSRAM ----------
        if (psramFound()) {
            uint32_t totalPsram = ESP.getPsramSize() / 1024;
            uint32_t freePsram  =
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024;

            snprintf(buf, sizeof(buf),
             "%-9s %lu/%lu KB",
             "PSRAM",
             freePsram,
             totalPsram);
            _items.push_back(std::string(buf));

        } else {
            snprintf(buf, sizeof(buf),
             "%-9s %s",
             "Ext RAM",
             "N/A");
            _items.push_back(std::string(buf));
        }

        snprintf(buf, sizeof(buf),
         "%-9s %lu MHz",
         "CPU",
         (uint64_t)getCpuFrequencyMhz());
        _items.push_back(std::string(buf));

        // ---------------- HARDWARE ----------------
        snprintf(buf, sizeof(buf),
         "%-9s %lu MB",
         "Flash",
         spi_flash_get_chip_size() / (1024 * 1024));
        _items.push_back(std::string(buf));

        snprintf(buf, sizeof(buf),
         "%-9s %s",
         "Chip",
         ESP.getChipModel());
        _items.push_back(std::string(buf));

        snprintf(buf, sizeof(buf),
         "%-9s %d",
         "Chip Rev",
         ESP.getChipRevision());
        _items.push_back(std::string(buf));

        // ---------------- SYSTEM ----------------
        snprintf(buf, sizeof(buf),
         "%-9s %s",
         "SDK",
         ESP.getSdkVersion());
        _items.push_back(std::string(buf));

        snprintf(buf, sizeof(buf),
         "%-9s %s",
         "FW",
         FIRMWARE_VERSION);
        _items.push_back(std::string(buf));

        snprintf(buf, sizeof(buf),
         "%-9s %s",
         "Dev By",
         DEVELOPED_BY);
        _items.push_back(std::string(buf));
    }

    // ============================================================
    // RESET REASON STRING
    // ============================================================
    std::string _resetReasonToString(esp_reset_reason_t r) {
        switch (r) {
            case ESP_RST_POWERON:   return "Power On";
            case ESP_RST_SW:        return "Software";
            case ESP_RST_PANIC:     return "Crash";
            case ESP_RST_INT_WDT:   return "Watchdog";
            case ESP_RST_TASK_WDT:  return "Task Watchdog";
            case ESP_RST_DEEPSLEEP: return "Wake From Sleep";
            case ESP_RST_BROWNOUT:  return "Low Power";
            default:                return "Unknown";
        }
    }

    // ============================================================
    // SCROLLBAR
    // ============================================================
    void _drawScrollbar(U8G2& u) {
        const int H = 64;
        for (int y = 0; y < H; y += 2) u.drawPixel(126, y);

        if (_items.size() <= 1) return;

        int blockH = max(6, H / (int)_items.size());
        int blockY = (int)((float)(H - blockH) *
                           _pos / (float)(_items.size() - 1));
        u.drawBox(125, blockY, 3, blockH);
    }
};