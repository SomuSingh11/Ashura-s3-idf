#pragma once

#include <string>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

// ============================================================
//  AshuraPrefs  —  Persistent user preferences via ESP32 NVS
//
//  Wraps ESP32's Preferences library (key-value NVS flash store).
//  Keys are namespaced under "ashura" to avoid collisions.
//
//  Current keys:
//    aura_ss    → int  screensaver vibe index (default 0)
//    aura_boot  → int  boot animation index   (default 0)
//
//  Usage:
//    AshuraPrefs::setScreensaver(1);
//    int idx = AshuraPrefs::getScreensaver(); // → 1
//
//  NVS survives power cycles and OTA updates.
//  Flash wear: NVS uses wear-levelling, safe for frequent writes.
// ============================================================

class AshuraPrefs {
    public:

        // ── Screensaver index ────────────────────────────────────
        static int getScreensaver() {
            return _getInt("ashura_vibes", "screensaver", 0);
        }

        static void setScreensaver(int index){
            _setInt("ashura_vibes", "screensaver", index);
            ESP_LOGI(TAG, "Screensaver → %d", index);
        }

        // ── Boot animation index ─────────────────────────────────
        static int getBoot() {
            return _getInt("ashura_vibes", "vibes_boot", 0);
        }

        static void setBoot(int index) {
            _setInt("ashura_vibes", "vibes_boot", index);
            ESP_LOGI(TAG, "Boot → %d", index);
        }

        // ── HomeScreen animation index ───────────────────────────
        static int getHomeScreen() {
            return _getInt("ashura_vibes", "vibes_home", 0);
        }

        static void setHomeScreen(int index) {
            _setInt("ashura_vibes", "vibes_home", index);
            ESP_LOGI(TAG, "HomeScreen → %d", index);
        }

        // ── Reset all prefs to defaults ──────────────────────────
        static void resetAll() {
            nvs_handle_t handle;
            if (nvs_open("ashura_vibes", NVS_READWRITE, &handle) == ESP_OK) {
                nvs_erase_all(handle);
                nvs_commit(handle);
                nvs_close(handle);
            }
            ESP_LOGI(TAG, "Reset to defaults");
        }

    private:
        static constexpr const char* TAG = "AshuraPrefs";

        static int _getInt(const char* ns, const char* key, int defaultVal) {
            nvs_handle_t handle;
            if (nvs_open(ns, NVS_READONLY, &handle) != ESP_OK) {
                return defaultVal;
            }

            int32_t value = defaultVal;
            // Ignore error — value stays at default if key missing
            nvs_get_i32(handle, key, &value);
            nvs_close(handle);
            return (int)value;
        }

        static void _setInt(const char* ns, const char* key, int value) {
            nvs_handle_t handle;
            if (nvs_open(ns, NVS_READWRITE, &handle) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to open NVS namespace: %s", ns);
                return;
            }
            
            nvs_set_i32(handle, key, (int32_t)value);
            nvs_commit(handle);
            nvs_close(handle);
        }
};