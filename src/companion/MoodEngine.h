#pragma once
#include "hal.h"
#include "../companion/EyeParams.h"
#include "../core/EventBus.h"

// ================================================================
//  MoodEngine  —  owns the companion's current mood
//  It's a simple state machine that listens to system events and 
//  updates its TARGET EyeParams accordingly.
//
//  Call  init()            once after EventBus is ready
//  Call  update(now)       every loop()
//  Call  onInteraction()   on any physical button press
//  Call  getTarget()       to get EyeParams for CompanionRenderer
//
//  Mood priority (highest wins):
//    SURPRISED   800ms flash, any notification
//    FOCUSED     while Pomodoro running
//    EXCITED     while Spotify playing
//    ANNOYED     after WiFi drop (auto-clears 3s)
//    HAPPY       after Pomodoro complete (auto-clears 3min)
//    BORED       idle > 5 min
//    SLEEPY      idle > 15 min
//    IDLE        default
// ================================================================

enum class CompanionMood {
    IDLE,
    HAPPY,
    BORED,
    SLEEPY,
    FOCUSED,
    SURPRISED,
    ANNOYED,
    EXCITED
};

class MoodEngine {
    public:
        // ── Call once after EventBus is ready ─────────────────────
        void init(){
            _lastInteraction = millis();

            // Notification → SURPRISED flash
            Bus().subscribe(AppEvent::NotificationReceived, [this](const std::string&) {
                _triggerSurprise();
            });

            // WiFi lost → ANNOYED briefly
            Bus().subscribe(AppEvent::WifiDisconnected, [this]() {
                _annoyed   = true;
                _annoyedAt = millis();
                if (!_pomodoroActive) _setMood(CompanionMood::ANNOYED);
            });

            // WebSocket fully active → HAPPY
            Bus().subscribe(AppEvent::WebSocketRegistered, [this]() {
                if (!_pomodoroActive) {
                    _happyAt = millis();
                    _setMood(CompanionMood::HAPPY);
                }
            });

            // ── Pomodoro ──────────────────────────────────────────
            Bus().subscribe(AppEvent::PomodoroStarted, [this]() {
                _pomodoroActive = true;
                _setMood(CompanionMood::FOCUSED);
            });
            Bus().subscribe(AppEvent::PomodoroCompleted, [this]() {
                _pomodoroActive = false;
                _happyAt = millis();
                _setMood(CompanionMood::HAPPY);
            });
            Bus().subscribe(AppEvent::PomodoroAborted, [this]() {
                _pomodoroActive = false;
                _setMood(CompanionMood::IDLE);
            });
            Bus().subscribe(AppEvent::PomodoroBreak, [this]() {
                _happyAt = millis();
                _setMood(CompanionMood::HAPPY);  // relaxed during break
            });

            // ── Spotify ───────────────────────────────────────────
            Bus().subscribe(AppEvent::SpotifyPlaying, [this]() {
                _spotifyPlaying = true;
                if (!_pomodoroActive) _setMood(CompanionMood::EXCITED);
            });
            Bus().subscribe(AppEvent::SpotifyPaused, [this]() {
                _spotifyPlaying = false;
                if (_mood == CompanionMood::EXCITED) _setMood(CompanionMood::IDLE);
            });
        }


        // ── Call every loop() ─────────────────────────────────────
        void update(unsigned long now) {
            // Surprise flash timeout - 700ms
            if(_surprised && now - _surprisedAt > 700UL){
                _surprised = false;
                _mood = _preSurpriseMood; // restore without lerp delay
            }

            // Annoyed auto-clear - 3s
            if(_annoyed && now - _annoyedAt > 3000UL) {
                _annoyed = false;
                if(_mood == CompanionMood::ANNOYED){
                    _setMood(CompanionMood::IDLE);
                }
            }

            // Happy Duration - 8s
            if (_mood == CompanionMood::HAPPY && now - _happyAt > 8000UL) {
                _setMood(CompanionMood::IDLE);
            }

            // Idle decay - only ehn nothing active overrides
            if(!_pomodoroActive && !_spotifyPlaying
                && _mood != CompanionMood::SURPRISED
                && _mood != CompanionMood::ANNOYED
                && _mood != CompanionMood::HAPPY
            ) {
                unsigned long idleTime = now - _lastInteraction;
                if (idleTime > 80000UL) { // >80s
                    _setMood(CompanionMood::SLEEPY);
                } else if (idleTime > 40000UL) { // >40s
                    _setMood(CompanionMood::BORED);
                } else {
                    if(_mood == CompanionMood::BORED || _mood == CompanionMood::SLEEPY){
                        _setMood(CompanionMood::IDLE);
                    }
                }
            }
        }

        // ── Call on any button press ──────────────────────────────
        void onInteraction () {
            _lastInteraction = millis();
            if (_mood == CompanionMood::SLEEPY || _mood == CompanionMood::BORED) { 
                _setMood(CompanionMood::IDLE);               
            }
        }

        // ── Returns target EyeParams for current mood ─────────────
        EyeParams getTarget() const {
            if (_surprised) return Mood::SURPRISED();
            switch (_mood) {
                case CompanionMood::HAPPY:     return Mood::HAPPY();
                case CompanionMood::BORED:     return Mood::BORED();
                case CompanionMood::SLEEPY:    return Mood::SLEEPY();
                case CompanionMood::FOCUSED:   return Mood::FOCUSED();
                case CompanionMood::ANNOYED:   return Mood::ANNOYED();
                case CompanionMood::EXCITED:   return Mood::EXCITED();
                default:                       return Mood::IDLE();
            }
        }

        CompanionMood getMood() const { return _mood; }


    private:
        void _setMood(CompanionMood m) { _mood = m; }

        void _triggerSurprise() {
            if(!_surprised) _preSurpriseMood = _mood;
            _surprised   = true;
            _surprisedAt = millis();
            _mood        = CompanionMood::SURPRISED;
        }

        CompanionMood _mood             = CompanionMood::IDLE;
        CompanionMood _preSurpriseMood = CompanionMood::IDLE;


        bool            _surprised          = false;
        uint64_t        _surprisedAt        = 0;
        bool            _annoyed            = false;
        uint64_t        _annoyedAt          = 0;
        bool            _pomodoroActive     = false;
        bool            _spotifyPlaying     = false;
        uint64_t        _lastInteraction    = 0;
        uint64_t        _happyAt            = 0;

};