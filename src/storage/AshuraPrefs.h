#pragma once

#include <string>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

// ============================================================
//  AshuraPrefs  —  Persistent user preferences via ESP32 NVS
//
//  Wraps ESP32's NVS flash key-value store.
//  Keys are namespaced to avoid collisions.
//
//  Namespaces:
//    "ashura_vibes"  → animation selection (screensaver, boot, home)
//    "ashura_pomo"   → Pomodoro config + vibe selection
//
//  NVS survives power cycles and OTA updates.
//  Flash wear: NVS uses wear-levelling, safe for frequent writes.
// ============================================================

class AshuraPrefs {
    public:

        // ── Screen Preferences: Vibe selections ──────────────────

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


        // ── Pomodoro : Vibe selections ────────────────────────────

        // ── workVibe ──────────────────────────────────────────────
        static int getPomodoroWorkVibe() { return _getInt("ashura_pomo", "work_vibe", 0); }

        static void setPomodoroWorkVibe(int index) {
            _setInt("ashura_pomo", "work_vibe", index);
            ESP_LOGI(TAG, "Pomodoro work vibe → %d", index);
        }

        // ── breakVibe ──────────────────────────────────────────────
        static int getPomodoroBreakVibe() { return _getInt("ashura_pomo", "break_vibe", 3); }

        static void setPomodoroBreakVibe(int index) {
            _setInt("ashura_pomo", "break_vibe", index);
            ESP_LOGI(TAG, "Pomodoro break vibe → %d", index);
        }

        // ── completeVibe ──────────────────────────────────────────────
        static int getPomodoroCompleteVibe() { return _getInt("ashura_pomo", "complete_vibe", 6); }

        static void setPomodoroCompleteVibe(int index) {
            _setInt("ashura_pomo", "complete_vibe", index);
            ESP_LOGI(TAG, "Pomodoro complete vibe → %d", index);
        }


        // ── Pomodoro : Timer durations ───────────────────────────
        static int getPomodoroWorkMin()      { return _getInt("ashura_pomo", "work_min",       25); }
        static void setPomodoroWorkMin(int m) {
            _setInt("ashura_pomo", "work_min", m);
        }

        static int getPomodoroShortBreakMin(){ return _getInt("ashura_pomo", "short_break_min", 5); }
        static void setPomodoroShortBreakMin(int m) {
            _setInt("ashura_pomo", "short_break_min", m);
        }

        static int getPomodoroLongBreakMin() { return _getInt("ashura_pomo", "long_break_min", 15); }
        static void setPomodoroLongBreakMin(int m) {
            _setInt("ashura_pomo", "long_break_min", m);
        }

        static int getPomodoroSessionsGoal() { return _getInt("ashura_pomo", "sessions_goal",   4); }
        static void setPomodoroSessionsGoal(int n) {
            _setInt("ashura_pomo", "sessions_goal", n);
        }


        // ── Reset all prefs to defaults ──────────────────────────
        static void resetAll() {
            nvs_handle_t handle;
            if (nvs_open("ashura_vibes", NVS_READWRITE, &handle) == ESP_OK) {
                nvs_erase_all(handle);
                nvs_commit(handle);
                nvs_close(handle);
            }
            if (nvs_open("ashura_pomo", NVS_READWRITE, &handle) == ESP_OK) {
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