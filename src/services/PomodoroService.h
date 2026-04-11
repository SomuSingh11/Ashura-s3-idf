#pragma once

#include "IService.h"
#include "esp_log.h"
#include "core/AshuraRecord.h"
#include "application/pomodoro/PomodoroEngine.h"
#include "application/pomodoro/PomodoroConfig.h"
#include "storage/AshuraPrefs.h"

// ================================================================
//  PomodoroService  —  External command handler for Pomodoro
//
//  Registered with MessageRouter in AshuraCore::_initNetwork().
//  Handles "command" type messages with pomodoro_ commands.
//
//  Supported commands (WebSocket / future Voice):
//
//    pomodoro_start        → start with saved prefs config
//    pomodoro_pause        → pause active session
//    pomodoro_resume       → resume paused session
//    pomodoro_abort        → abort and return to IDLE
//    pomodoro_skip         → skip current phase (e.g. skip break)
//
//  Optional data fields on pomodoro_start:
//    work_min      (int)   override work duration
//    break_min     (int)   override short break duration
//    long_break_min(int)   override long break duration
//    sessions      (int)   override sessions goal
//
//  Example WebSocket message:
//    {
//      "type": "command",
//      "data": {
//        "command": "pomodoro_start",
//        "work_min": 30,
//        "sessions": 2
//      }
//    }
//
//  Voice path (future):
//    VoiceService parses intent → builds same JsonDocument →
//    calls handleMessage(doc) directly. Zero engine/screen changes.
// ================================================================

class PomodoroService : public IService {
    public:
        void init() override {}

        bool handleMessage(const JsonDocument& doc) override {
            const char* type = doc["type"];

            if (!type || strcmp(type, "command") != 0) return false;
            
            const char* command = doc["data"]["command"];
            if (!command) return false;

            // Only handle pomodoro_ prefixed commands
            if (strncmp(command, "pomodoro_", 9) != 0) return false;

            PomodoroEngine* engine = record_get<PomodoroEngine>(RECORD_POMODORO);
            if(!engine) {
                ESP_LOGE(TAG, "PomodoroEngine not found in record");
                return true; // consumed — don't pass to other services
            }

            if (strcmp(command, "pomodoro_start") == 0) {
                _handleStart(doc, *engine);
            } else if (strcmp(command, "pomodoro_pause") == 0) {
                engine->pause();
                ESP_LOGI(TAG, "Remote: pause");
            } else if (strcmp(command, "pomodoro_resume") == 0) {
                engine->resume();
                ESP_LOGI(TAG, "Remote: resume");
            } else if (strcmp(command, "pomodoro_abort") == 0) {
                engine->abort();
                ESP_LOGI(TAG, "Remote: abort");
            } else if (strcmp(command, "pomodoro_skip") == 0) {
                engine->skipPhase();
                ESP_LOGI(TAG, "Remote: skip phase");
            } else {
                ESP_LOGW(TAG, "Unknown pomodoro command: %s", command);
            }
    
            return true; // consumed
        }

        const char* getName() const override { return "PomodoroService"; }
        
    private:
        static constexpr const char* TAG = "PomodoroService";

        void _handleStart(const JsonDocument& doc, PomodoroEngine& engine) {
            // Build config from saved prefs, allow per-command overrides
            int workMin     = AshuraPrefs::getPomodoroWorkMin();
            int shortMin    = AshuraPrefs::getPomodoroShortBreakMin();
            int longMin     = AshuraPrefs::getPomodoroLongBreakMin();
            int sessions    = AshuraPrefs::getPomodoroSessionsGoal();

            // Apply optional overrides from command payload
            if(!doc["data"]["work_min"].isNull()){
                workMin = doc["data"]["work_min"].as<int>();
            }
            if (!doc["data"]["break_min"].isNull()) {
                shortMin = doc["data"]["short_break_min"].as<int>();
            }
            if (!doc["data"]["long_break_min"].isNull()) {
                longMin  = doc["data"]["long_break_min"].as<int>();
            }    
            if (!doc["data"]["sessions"].isNull()) {
                sessions = doc["data"]["sessions"].as<int>();
            }
            
            PomodoroConfig cfg = PomodoroConfig::fromMinutes(
                workMin,
                shortMin,
                longMin,
                sessions
            );

            engine.start(cfg);
            ESP_LOGI(TAG, "Remote: start — work=%dmin break=%dmin sessions=%d", workMin, shortMin, sessions);
        }
};