#pragma once

#include <string>
#include <ctime>
#include <cstdio>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sntp.h"

#include "config.h"
#include "hal.h"

// ============================================================
//  TimeManager  —  Non-blocking NTP time service
// ============================================================

class TimeManager {
    public:
        //Singleton Pattern
        static TimeManager& instance() {
            static TimeManager tm;
            return tm;
        }

        // ── Request NTP sync ─────────────────────────────────────
        void sync() {
            if (_sntpStarted) {
                // Already running — just restart
                sntp_restart();
            } else {
                // First sync — initialize SNTP
                esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
                esp_sntp_setservername(0, "pool.ntp.org");
                esp_sntp_setservername(1, "time.google.com");
                esp_sntp_setservername(2, "time.nist.gov");

                // Set timezone via POSIX TZ string
                // IST = UTC+5:30
                setenv("TZ", "IST-5:30", 1);
                tzset();

                esp_sntp_init();
                _sntpStarted = true;
            }

            _syncRequested = true;
            ESP_LOGI(TAG, "NTP sync requested");
        }   

        // Poll once per loop — marks synced after first successful getLocalTime
        void update() {
            if (_synced || !_syncRequested) return;
            
            // Check SNTP sync status
            sntp_sync_status_t status = sntp_get_sync_status();

            if (status == SNTP_SYNC_STATUS_COMPLETED) {
                _synced = true;
                ESP_LOGI(TAG, "NTP synced");
            }
        }

        bool isSynced() const { return _synced; }

        int getHH() const { return _get().tm_hour; }
        int getMM() const { return _get().tm_min;  }
        int getSS() const { return _get().tm_sec;  }

        std::string getDateString() const {
            char buf[6];
            struct tm ti = _get();
            snprintf(buf, sizeof(buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
            return std::string(buf);
        }

    private:
        TimeManager()                           = default;

        bool                    _synced         = false;
        bool                    _syncRequested  = false;
        bool                    _sntpStarted    = false;

        static constexpr const char* TAG = "TimeManager";

        struct tm _get() const {
            struct tm ti = {};
            time_t now;
            time(&now);

            // localtime_r is thread-safe
            localtime_r(&now, &ti);

            // If time not synced yet, derive from uptime
            if (!_synced) {
                uint64_t s = millis() / 1000ULL;
                ti.tm_hour = (int)((s / 3600) % 24);
                ti.tm_min  = (int)((s / 60)   % 60);
                ti.tm_sec  = (int)(s           % 60);
            }

            return ti;
        }
};

inline TimeManager& Time() { return TimeManager::instance(); }