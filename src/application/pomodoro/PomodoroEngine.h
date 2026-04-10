#pragma once
 
#include <cstdint>
#include <string>
#include "hal.h"
#include "esp_log.h"
#include "application/pomodoro/PomodoroConfig.h"
#include "core/EventBus.h"
 
// ================================================================
//  PomodoroEngine  —  Pure timer state machine
//
//  Owns ALL Pomodoro logic. Zero UI, zero display dependency.
//  AshuraCore owns one instance, calls update(now) every loop.
//
//  State machine:
//
//    IDLE
//      │  start()
//      ▼
//    WORK ────────── pause() ──► PAUSED
//      │  timer done               │ resume()
//      ▼                           ▼
//    SHORT_BREAK ◄────────────── WORK (resumed)
//      │  timer done
//      ▼
//    WORK (next session)
//      │  after sessionsGoal work sessions
//      ▼
//    LONG_BREAK
//      │  timer done
//      ▼
//    DONE ──► IDLE
//
//    abort() from any state → IDLE
//
//  EventBus events published:
//    PomodoroStarted    payload = ""
//    PomodoroTick       payload = remaining ms as decimal string
//    PomodoroPaused     payload = ""
//    PomodoroResumed    payload = ""
//    PomodoroBreak      payload = "short" | "long"
//    PomodoroCompleted  payload = ""     (all sessions done)
//    PomodoroAborted    payload = ""
//
//  Public API — callable from screens, PomodoroService, VoiceService:
//    start(config)   resume()   pause()   abort()   skipPhase()
//
//  Accessors (safe to call every frame):
//    state()         remaining()   elapsed()
//    sessionsDone()  isActive()    isPaused()
// ================================================================

enum class PomodoroState {
    IDLE,
    WORK,
    SHORT_BREAK,
    LONG_BREAK,
    PAUSED,
    DONE
};

class PomodoroEngine {
    public:
        // ── Lifecycle (called by AshuraCore) ──────────────────────
        void update(uint64_t now) {
            if(_state == PomodoroState::IDLE || 
               _state == PomodoroState::PAUSED || 
               _state == PomodoroState::DONE) return;
            
            
            // for state == WORK or SHORT_BREAK or LONG_BREAK 
            uint64_t elapsed = now - _phaseStart;

            // Publish tick every second while active
            if(elapsed - _lastTickPublish >= 1000UL) {
                _lastTickPublish = now;
                uint32_t remaining = _remaining(now);
                Bus().publish(AppEvent::PomodoroTick, std::to_string(remaining));
            }

            // Phase complete
            if(elapsed >= _phaseDuration) {
                _onPhaseComplete(now);
            }
        }

        // ── Public API ────────────────────────────────────────────

        void start(const PomodoroConfig& cfg) {
            _cfg            = cfg;
            _cfg.clamp();
            _sessionsDone   = 0;
            _enterWork(millis());

            ESP_LOGI(TAG, "Started — %d sessions, work=%lums", _cfg.sessionsGoal, (unsigned long)_cfg.workMs);
        }

        void pause() {
            if(_state != PomodoroState::WORK ||
               _state != PomodoroState::SHORT_BREAK ||
               _state != PomodoroState::LONG_BREAK) return;

            _pausedState        = _state; // (work || long-break || short-break)
            _pausedRemaining    = _remaining(millis());
            _state              = PomodoroState::PAUSED;

            ESP_LOGI(TAG, "Paused — %lu ms remaining", (unsigned long)_pausedRemaining);
            Bus().publish(AppEvent::PomodoroPaused);
        }

        void resume() {
            if(_state != PomodoroState::PAUSED) return;

            _state = _pausedState;
            _phaseStart = millis() - (_phaseDuration - _pausedRemaining); // shift the _phaseStart (as if the pause never happened)

            ESP_LOGI(TAG, "Resumed");
            Bus().publish(AppEvent::PomodoroResumed);
        }

        void abort() {
            if(_state == PomodoroState::IDLE) return;
            
            _state = PomodoroState::IDLE;
            ESP_LOGI(TAG, "Aborted");

            Bus().publish(AppEvent::PomodoroAborted);
        }

        // Skip current phase — useful for voice: "skip break"
        void skipPhase() {
            if (_state == PomodoroState::IDLE   ||
                _state == PomodoroState::PAUSED ||
                _state == PomodoroState::DONE)   return;
            _onPhaseComplete(millis());
        }


        // ── Accessors ─────────────────────────────────────────────
        PomodoroState   state()         const { return _state; }
        int             sessionsDone()  const { return _sessionsDone; }
        int             sessionsGoal()  const { return _cfg.sessionsGoal; }
        bool            isActive()      const { return _state != PomodoroState::IDLE && _state != PomodoroState::DONE; }
        bool            isPaused()      const { return _state == PomodoroState::PAUSED; }
        bool            isWork()        const { return _state == PomodoroState::WORK; }
        bool            isBreak()       const { return _state == PomodoroState::SHORT_BREAK || _state == PomodoroState::LONG_BREAK; }


        // Milliseconds remaining in current phase (0 when idle/done)
        uint32_t remaining() const {
            if (_state == PomodoroState::IDLE || _state == PomodoroState::DONE)
                return 0;
            if (_state == PomodoroState::PAUSED)
                return _pausedRemaining;
            return _remaining(millis());
        }

        // Milliseconds elapsed in current phase
        uint32_t elapsed() const {
            if (_state == PomodoroState::IDLE || _state == PomodoroState::DONE)
                return 0;
            if (_state == PomodoroState::PAUSED)
                return _phaseDuration - _pausedRemaining;
            uint64_t e = millis() - _phaseStart;
            return (uint32_t)(e > _phaseDuration ? _phaseDuration : e);
        }

        // Total duration of current phase (for progress bar math)
        uint32_t phaseDuration() const { return _phaseDuration; }
    
        // Human-readable phase label
        const char* phaseLabel() const {
            switch (_state) {
                case PomodoroState::WORK:        return "WORK";
                case PomodoroState::SHORT_BREAK: return "BREAK";
                case PomodoroState::LONG_BREAK:  return "LONG BREAK";
                case PomodoroState::PAUSED:
                    switch (_pausedState) {
                        case PomodoroState::WORK:        return "WORK (paused)";
                        case PomodoroState::SHORT_BREAK: return "BREAK (paused)";
                        case PomodoroState::LONG_BREAK:  return "L.BREAK (paused)";
                        default: break;
                    }
                    return "PAUSED";
                case PomodoroState::DONE:        return "DONE";
                default:                         return "IDLE";
            }
        }
    
        const PomodoroConfig& config() const { return _cfg; }

    private:
        static constexpr const char* TAG = "PomodoroEngine";

        PomodoroState   _state          = PomodoroState::IDLE; // current state
        PomodoroState   _pausedState    = PomodoroState::IDLE; // state before pausing
        PomodoroConfig  _cfg;

        uint64_t    _phaseStart         = 0;
        uint32_t    _phaseDuration      = 0;
        uint32_t    _pausedRemaining    = 0;
        int         _sessionsDone       = 0;
        uint64_t    _lastTickPublish    = 0;


        // ── Phase transitions ─────────────────────────────────────

        void _enterWork(uint64_t now) {
            _state          = PomodoroState::WORK;        // new work session
            _phaseDuration  = _cfg.workMs;                //duration depends on config
            _phaseStart     = now;                        // reset timer

            ESP_LOGI(TAG, "Work phase %d/%d", _sessionsDone + 1, _cfg.sessionsGoal);
            Bus().publish(AppEvent::PomodoroStarted);      // notify system
        }

        void _enterShortBreak(uint64_t now) {
            _state          = PomodoroState::SHORT_BREAK;
            _phaseDuration  = _cfg.shortBreakMs;
            _phaseStart     = now;

            ESP_LOGI(TAG, "Short break");
            Bus().publish(AppEvent::PomodoroBreak, "short-break");
        }

        void _enterLongBreak(uint64_t now) {
            _state          = PomodoroState::LONG_BREAK;
            _phaseDuration  = _cfg.longBreakMs;
            _phaseStart     = now;

            ESP_LOGI(TAG, "Long break");
            Bus().publish(AppEvent::PomodoroBreak, "long-break");
        }

        void _enterDone() {
            _state          = PomodoroState::DONE;

            ESP_LOGI(TAG, "All %d sessions complete", _sessionsDone);
            Bus().publish(AppEvent::PomodoroCompleted);
        }

        void _onPhaseComplete(uint64_t now) {
            switch(_state) {
                case PomodoroState::WORK:
                    _sessionsDone++;
                    if(_sessionsDone >= _cfg.sessionsGoal) {
                        _enterLongBreak(now);
                    } else {
                        _enterShortBreak(now);
                    }
                    break;

                case PomodoroState::SHORT_BREAK:
                    _enterWork(now);
                    break;
                case PomodoroState::LONG_BREAK:
                    _enterDone();
                    break;

                default:
                    break;

            }
        }

        uint32_t _remaining(uint64_t now) const {
            uint64_t elapsed = now - _phaseStart;
            if(elapsed >= _phaseDuration) return 0;
            return (uint32_t)(_phaseDuration-elapsed);
        }
};