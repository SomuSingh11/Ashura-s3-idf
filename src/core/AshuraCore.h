#pragma once

#include <functional>
#include <string>

// IDF headers
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"

// Project headers
#include "WiFiManager.h"
#include "NotificationManager.h"

#include "../network/WebSocketManager.h"
#include "../network/MessageRouter.h"

#include "../core/UIManager.h"
#include "../core/DisplayManager.h"
#include "../core/Loader.h"

#include "../services/DeviceService.h"

#include "../ui/screens/HomeScreen.h"
#include "../companion/MoodEngine.h"
#include "../application/wled/WledManager.h"

// ============================================================
//  AshuraCore  —  Top-level OS kernel
//
//  Boot sequence:
//    1.  Serial / debug
//    2.  Display service  → record_create(RECORD_DISPLAY)
//    3.  GUI / UIManager  → record_create(RECORD_GUI)
//    4.  Loader service   → record_create(RECORD_LOADER)  
//    5.  Register all apps with Loader                    
//    6.  Push boot screens (Splash → Home)
//    7.  Wire EventBus subscriptions
//    8.  Button GPIO init
//    9.  WiFi + NTP + WebSocket init
//
//  Update loop (mirrors Flipper's OS tick):
//    - WiFi watchdog
//    - NTP retry
//    - WebSocket poll
//    - Companion update (mood decay, lerp, blink)
//    - UI tick
//    - HomeScreen signals
//    - Physical button debounce → onInteraction()
// ============================================================

static const char* CORE_TAG = "AshuraCore";

class AshuraCore {
    public: 
        void init();
        void update();

        // ========================================================
        // Button Debounce State
        // ======================================================== 
        struct BtnState {
            bool            lastRaw         = true;     // Last raw reading from pin (IDF gpio_get_level)
            bool            state           = true;     // Debounced state
            uint64_t        lastDebounce    = 0;        // Timestamp of last state change for debounce
            uint64_t        pressStart      = 0;        // Timestamp when button was pressed (for long press)
            bool            longFired       = false;    // Whether long press action has been fired
        };

    private:
        // ========================================================
        // Boot Helpers
        // ========================================================
        //void _initServices();
        void _registerApps();
        void _bootUI();
        void _wireEvents();
        void _initButtons();
        void _initNetwork();

        // ========================================================
        // Runtime Helpers
        // ========================================================
        void _updateNetwork(uint64_t now);    // coordinates WiFi + WS
        void _updateBadge();
        void _buildAppMenu();
        void _launchScreenSaver();
        bool _readButton(gpio_num_t pin, BtnState& state);
        void _pollButton(
            gpio_num_t pin, 
            BtnState& state, 
            std::function<void()> onShortPress, 
            std::function<void()> onLongPress, 
            bool& anyPressed
        );

        // ========================================================
        // Services (owned by OS, registered in AshuraRecord)
        // ========================================================
        DisplayManager      _display;
        UIManager           _ui;
        WiFiManager         _wifi;
        WebSocketManager    _websocket;
        MessageRouter       _router;
        DeviceService       _deviceService;
        Loader              _loader;
        WledManager         _wled; 

        // ========================================================
        // Companion Subsystems
        // ========================================================
        MoodEngine          _mood;                                  // tracks emotional state
        CompanionRenderer   _companion{_mood};                      // renders eyes from mood

        // ========================================================
        // ASHURA CORE State
        // ========================================================
        HomeScreen*     _homeScreen         = nullptr;
        NetState        _prevNetState       = NetState::IDLE;       // last known WiFi state
        WebSocketState  _prevWsState        = WebSocketState::IDLE; // last known WS state
        uint64_t        _lastNtpSync        = 0;

        // ========================================================
        // Button States
        // ========================================================
        BtnState _btnUp;
        BtnState _btnDown;
        BtnState _btnSelect;
        BtnState _btnBack;
};