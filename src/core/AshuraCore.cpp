#include "config.h"
#include "pins.h"
#include "hal.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "nvs_flash.h"

#include "AshuraRecord.h"
#include "EventBus.h"
#include "TimeManager.h"
#include "core/AshuraCore.h"
#include "storage/AshuraPrefs.h"

#include "../ui/screens/AppMenuScreen.h"
#include "../ui/screens/SubMenuScreen.h"
#include "../ui/screens/bootScreen/SplashScreen.h"

// ── Apps ──────────────────────────────────────────────────────
#include "../ui/screens/wled/WledDeviceScreen.h"
#include "../ui/screens/clockApp/ClockFaceScreen.h"

// ── Settings screens ────────────────────────────────────────────
#include "../ui/screens/settings/SystemStatsScreen.h"
#include "../ui/screens/settings/network/NetworkScreen.h"

// ── Vibe system ───────────────────────────────────────────────
#include "../ui/screens/vibes/VibePickerScreen.h"
#include "../ui/screens/vibes/VibePreviewScreen.h"
#include "../ui/screens/vibes/VibePlayerScreen.h"
#include "../application/vibes/VibeRegistry.h"

// ── Pomodoro ──────────────────────────────────────────────────
#include "../ui/screens/pomodoro/PomodoroSetupScreen.h"

#include "ui/assets/icons/clock.h"
#include "ui/assets/icons/games.h"
#include "ui/assets/icons/settings.h"
#include "ui/assets/icons/vibes.h"
#include "ui/assets/icons/wled.h"

#include <vector>
#include <string>

// ============================================================
//  AshuraOS::init  —  Full system boot
// ============================================================

void AshuraCore::init()
{
  // ── NVS init — must be first ──────────────────────────────
  // Required before WiFi, Preferences, anything that uses NVS
  esp_err_t ret = nvs_flash_init();
  if(ret == ESP_ERR_NVS_NO_FREE_PAGES ||
     ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      // NVS partition was truncated or version mismatch
      // Erase and reinit — loses saved data but recovers cleanly
    ESP_LOGW(CORE_TAG, "NVS Truncating, erasing...");
    nvs_flash_erase();
    nvs_flash_init();
  }

  ESP_LOGI(CORE_TAG, "================================================");
  ESP_LOGI(CORE_TAG, "        ASHURA OS  —  Booting...");
  ESP_LOGI(CORE_TAG, "================================================");

  // ── 1. Display Service ──────────────────────────────────────────
  _display.init();
  record_create(RECORD_DISPLAY, &_display);

  // ── 2. GUI / UIManager Service ──────────────────────────────────
  _ui.init(&_display);
  record_create(RECORD_GUI, &_ui);

  // ── 3. Loader Service ───────────────────────────────────────────
  record_create(RECORD_LOADER, &_loader);

  // ── 4. Register Built-in Apps ───────────────────────────────────
  _registerApps();

  // ── 5. Mood Engine & Companion ──────────────────────────────────
  _mood.init();
  _companion.begin();

  // ── 6. Wled ─────────────────────────────────────────────────────
  _wled.begin();
  record_create(RECORD_WLED, &_wled);

  // ── 7. Pomodoro Engine ──────────────────────────────────────────
  record_create(RECORD_POMODORO, &_pomodoro);
  ESP_LOGI(CORE_TAG, "PomodoroEngine ready");

  // ── 8. Boot UI Sequence ─────────────────────────────────────────
  _bootUI();

  // ── 9. EventBus Wiring ──────────────────────────────────────────
  _wireEvents();

  // ── 10. Button Initialization ────────────────────────────────────
  _initButtons();

  // ── 11. Network & WebSocket Initialization ───────────────────────
  _initNetwork();

  ESP_LOGI(CORE_TAG, "Boot complete");
}


// ============================================================
//  AshuraCore::update
// ============================================================

void AshuraCore::update(){
  uint64_t now = millis();
  
  _updateNetwork(now);

  // ── Pomodoro Engine Tick ─────────────────────────────────────
  // Engine publishes EventBus events which drive MoodEngine
  _pomodoro.update(now);

  // ── Companion Systems (Mood Update — mood decay + lerp + blink + micro) ──────────
  _mood.update(now);
  _companion.update(now);

  // ── UI Tick ─────────────────────────────────────────────────
  _ui.update();

  // ── HomeScreen Signals ──────────────────────────────────────
  if (_ui.currentScreen() == _homeScreen) {
    if(_homeScreen->wantsMenu())        _buildAppMenu();
    if(_homeScreen->wantsScreenSaver()) _launchScreenSaver();
  }

  // ── Physical Buttons ────────────────────────────────────────
  bool anyPressed = false;

  _pollButton(
    (gpio_num_t)PIN_BUTTON_UP,_btnUp,
    [this]{ _ui.onButtonUp(); }, 
    [this]{ _ui.onLongPressUp();  }, 
    anyPressed
  );
  _pollButton(
    (gpio_num_t)PIN_BUTTON_DOWN, _btnDown, 
    [this]{ _ui.onButtonDown(); }, 
    [this]{ _ui.onLongPressDown(); }, 
    anyPressed
  );
  _pollButton(
    (gpio_num_t)PIN_BUTTON_SELECT, _btnSelect, 
    [this]{ _ui.onButtonSelect(); }, 
    [this]{ _ui.onLongPressSelect();}, 
    anyPressed
  );
  _pollButton(
    (gpio_num_t)PIN_BUTTON_BACK, _btnBack, 
    [this]{ _ui.onButtonBack(); }, 
    [this]{ _ui.onLongPressBack();}, 
    anyPressed
  );

  // ── Notify mood engine — resets idle timer, wakes from bored/sleepy ───────────────
  if (anyPressed) _mood.onInteraction();
}


// ============================================================
//  _registerApps
// ============================================================

void AshuraCore::_registerApps()
{
  // ── Clock ─────────────────────────────────────────────────
  _loader.registerApp({
        "clock", "Clock", "clock",
        [this](DisplayManager& d) -> IScreen* {
            return new SubMenuScreen(d, "Clock", {
                { "Clock Face", [this]() { _ui.pushScreen(new ClockFaceScreen(_display)); } },
                { "Pomodoro",   [this]() { _ui.pushScreen(new PomodoroSetupScreen(_display, _ui, _pomodoro)); } },
                { "TODO: Stopwatch",  [this]() { } },
            });
        }
    });

  // ── Games ─────────────────────────────────────────────────
  _loader.registerApp({
        "games", "Games", "games",
        [this](DisplayManager& d) -> IScreen* {
            return new SubMenuScreen(d, "Games", {
                { "TODO: Snake",     [this]() {  } },
            });
        }
    });

  // ── AI Assitant ───────────────────────────────────────────────
  // _loader.registerApp({
  //       "ai", "AI", "ai",
  //       [this](DisplayManager& d) -> IScreen* {
  //           return new SubMenuScreen(d, "AI Assistant", {
  //               { "Chat",     [this]() { /* TODO */ } },
  //               { "History",  [this]() { /* TODO */ } },
  //               { "Settings", [this]() { /* TODO */ } },
  //           });
  //       }
  //   });

  // ── Vibes ──────────────────────────────────────────────────
  _loader.registerApp({
        "vibes", "Vibes", "vibes",
        [this](DisplayManager& d) -> IScreen* {
            return new SubMenuScreen(d, "Vibes", {
                { "Screensaver", [this]() {
                    _ui.pushScreen(new VibePickerScreen(
                        _display, _ui, 0, AshuraPrefs::getScreensaver()));
                }},
                { "Boot Screen", [this]() {
                    _ui.pushScreen(new VibePickerScreen(
                        _display, _ui, 1, AshuraPrefs::getBoot()));
                }},
                { "TODO: Home Screen", [this]() {
                    _ui.pushScreen(new VibePickerScreen(
                        _display, _ui, 2, AshuraPrefs::getHomeScreen()));
                }},
            });
        }
    });

  // ── Spotify ───────────────────────────────────────────────
  // _loader.registerApp({
  //       "spotify", "Spotify", "spotify",
  //       [this](DisplayManager& d) -> IScreen* {
  //           return new SubMenuScreen(d, "Spotify", {
  //               { "Now Playing", [this]() { /* TODO */ } },
  //               { "Browse",      [this]() { /* TODO */ } },
  //               { "Search",      [this]() { /* TODO */ } },
  //           });
  //       }
  //   });

  // ── WLED ──────────────────────────────────────────────────
  _loader.registerApp({
        "wled", "WLED", "wled",
        [this](DisplayManager& d) -> IScreen* {
          return new WledDeviceScreen(d, _ui, _wled);
        }
    });

  // ── Settings ──────────────────────────────────────────────
  _loader.registerApp({
        "settings", "Settings", "settings",
        [this](DisplayManager& d) -> IScreen* {
            return new SubMenuScreen(d, "Settings", {
                { "Network",      [this]() { _ui.pushScreen(new NetworkScreen(_display, _ui, _wifi, _websocket)); } },
                { "System Stats", [this]() { _ui.pushScreen(new SystemStatsScreen(_display)); } },
            });
        }
    });

  ESP_LOGI(CORE_TAG, "%d apps registered", (int)_loader.apps().size());
}


// ============================================================
//  _bootUI  —  Read NVS boot pref, push BootScreen
// ============================================================

void AshuraCore::_bootUI()
{
  _homeScreen = new HomeScreen(_display, _companion);
  _ui.pushScreen(_homeScreen);

  int bootIdx = AshuraPrefs::getBoot();
  if (bootIdx < 0 || bootIdx >= VIBE_COUNT) bootIdx = 0;
  
  ESP_LOGI(CORE_TAG, "Boot anim: %d / %s", bootIdx, ALL_VIBES[bootIdx].name);

  const Animation* bootAnimation = ALL_VIBES[bootIdx].animation;

  if(bootAnimation == nullptr){
    ESP_LOGI(CORE_TAG, "Using SplashScreen");
    _ui.pushScreen(new SplashScreen(_display));
  } else {
    ESP_LOGI(CORE_TAG, "Using VibePlayerScreen: %s", bootAnimation->name);
    _ui.pushScreen(new VibePlayerScreen(_display, bootAnimation));
  }
}


// ============================================================
//  _launchScreenSaver —  Read NVS screensaver pref, push VibePlayerScreen
// ============================================================

void AshuraCore::_launchScreenSaver() {
    int idx = AshuraPrefs::getScreensaver();
    if (idx < 0 || idx >= VIBE_COUNT) idx = 0;

    const Animation* anim = ALL_VIBES[idx].animation;
    _ui.pushScreen(new VibePlayerScreen(_display, anim));
}



// ============================================================
//  _wireEvents
// ============================================================

void AshuraCore::_wireEvents()
{
  // ── WiFi Events ──────────────────────────────────────────────
  Bus().subscribe(AppEvent::WifiConnected, [this](const std::string& ip) {
    ESP_LOGI(CORE_TAG, "WiFi connected: %s", ip.c_str());
  });

  Bus().subscribe(AppEvent::WifiDisconnected, [this]() {
    ESP_LOGW(CORE_TAG, "WiFi disconnected");
  });

  Bus().subscribe(AppEvent::WifiFailed, [this]() {
    ESP_LOGE(CORE_TAG, "WiFi FAILED");
    // Notification already pushed by WiFiManager
    // MoodEngine → ANNOYED (brief)
  });

  Bus().subscribe(AppEvent::WifiIdle, [this]() {
    ESP_LOGI(CORE_TAG, "WiFi IDLE — credentials cleared");
  });


  // ── WebSocket → HomeScreen Badge ─────────────────────────────
  Bus().subscribe(AppEvent::WebSocketConnected, [this]() {
    ESP_LOGI(CORE_TAG, "WS connected");
  });

  Bus().subscribe(AppEvent::WebSocketDisconnected, [this]() {
    ESP_LOGW(CORE_TAG, "WS disconnected");
  });

  Bus().subscribe(AppEvent::WebSocketRegistered, [this]() {
    ESP_LOGI(CORE_TAG, "WS registered");
  });

  Bus().subscribe(AppEvent::WebSocketFailed, [this]() {
    ESP_LOGE(CORE_TAG, "WS FAILED");
  });


  // ── Incoming Commands → Router ───────────────────────────────
  Bus().subscribe(AppEvent::CommandReceived, [this](const std::string& json) {
    _router.route(json);
  });


  // ── Notifications → HomeScreen Ticker ───────────────────────
  // (MoodEngine also subscribed → SURPRISED flash)
  Bus().subscribe(AppEvent::NotificationReceived, [this](const std::string& payload) {
    if (_homeScreen) _homeScreen->setLastMessage(payload);
  });


  // ── Outgoing Messages → WebSocket ───────────────────────────
  Bus().subscribe(AppEvent::SendMessage, [this](const std::string& json) {
    _websocket.send(json);
  });

  // ── Notification badge dot ────────────────────────────────
  Bus().subscribe(AppEvent::NotificationPushed, [this](const std::string& title) {
    // HomeScreen reads Notifs().unreadCount() each frame
    // so no explicit call needed — just log here
    ESP_LOGI(CORE_TAG, "Notification: %s", title.c_str());
  });

  // ── Pomodoro Events → HomeScreen ticker ──────────────────────
  Bus().subscribe(AppEvent::PomodoroStarted, [this]() {
    ESP_LOGI(CORE_TAG, "Pomodoro started");
    if (_homeScreen) _homeScreen->setLastMessage("Pomodoro started!");
  });
 
  Bus().subscribe(AppEvent::PomodoroBreak, [this](const std::string& kind) {
    ESP_LOGI(CORE_TAG, "Pomodoro break: %s", kind.c_str());
    std::string msg = (kind == "long") ? "Long break — well done!"
                                       : "Short break — take a breath.";
    if (_homeScreen) _homeScreen->setLastMessage(msg);
  });
 
  Bus().subscribe(AppEvent::PomodoroCompleted, [this]() {
    ESP_LOGI(CORE_TAG, "Pomodoro completed");
    if (_homeScreen) _homeScreen->setLastMessage("All sessions complete!");
  });
 
  Bus().subscribe(AppEvent::PomodoroAborted, [this]() {
    ESP_LOGI(CORE_TAG, "Pomodoro aborted");
  });
}


// ============================================================
//  _initButtons
// ============================================================

void AshuraCore::_initButtons()
{
  // Configure all buttons in one call
  uint64_t pinBitmask = 
    (1ULL << PIN_BUTTON_UP)     |
    (1ULL << PIN_BUTTON_DOWN)   |
    (1ULL << PIN_BUTTON_SELECT) |
    (1ULL << PIN_BUTTON_BACK);

  gpio_config_t io_conf = {
    .pin_bit_mask = pinBitmask,
    .mode         = GPIO_MODE_INPUT,
    .pull_up_en   = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type    = GPIO_INTR_DISABLE
  };

  gpio_config(&io_conf);

  // Seed initial button states to match physical hardware,
  // preventing ghost edges on the first update cycle.
  auto seedButton = [](gpio_num_t pin, BtnState& btn) {
      bool raw = (gpio_get_level(pin) == 0); // 0 means pressed for active-low
      btn.lastRaw      = raw;
      btn.state        = raw;
      btn.lastDebounce = millis();
      btn.longFired    = false;
  };
  ESP_LOGI(CORE_TAG, "Buttons initialized");
}


// ============================================================
//  _initNetwork
// ============================================================

void AshuraCore::_initNetwork()
{
  // ── WiFi Service ─────────────────────────────────────────────
  record_create(RECORD_WIFI, &_wifi);
  _wifi.init(); // loads NVS creds, starts CONNECTING if found

  // ── Initial Time Sync ───────────────────────────────────────
  if (_wifi.isConnected()){
    Time().sync();
    _lastNtpSync = millis();
  }

  // ── WebSocket Service ───────────────────────────────────────
  record_create(RECORD_WEBSOCKET, &_websocket);
  _websocket.init(); // loads NVS config, wires callbacks | state machine starts in IDLE — update() drives it

  // ── Message Routing Services ────────────────────────────────
  _router.registerService(&_deviceService);
  _router.registerService(&_pomodoroService);

  // ── Seed state tracking ───────────────────────────────────
    _prevNetState = _wifi.state();
    _prevWsState  = _websocket.webSocketState();

    _updateBadge();   // set initial badge on HomeScreen
}


// ============================================================
//  _updateNetwork
//
//  Coordinates WiFi + WebSocket with a simple state machine:
//
//  WiFi down  → only run WiFiManager (handles reconnect internally)
//               WebSocket is NOT polled — pointless without WiFi
//
//  WiFi restored → immediately trigger WS connect + NTP sync
//
//  WiFi up    → poll WebSocket normally (handles its own reconnect)
//
//  WiFi lost  → stop polling WebSocket, update home screen badge
// ============================================================

void AshuraCore::_updateNetwork(uint64_t now){
  NetState        prevNet   = _wifi.state();
  //WebSocketState  prevWs    = _websocket.webSocketState();

  // ── 1. Always tick WiFiManager ────────────────────────────
    _wifi.update();

  // ── 2. React to WiFi state transitions ────────────────────
  NetState curNet = _wifi.state();

  if(prevNet != NetState::CONNECTED && curNet == NetState::CONNECTED) {
    // WiFi just came up — sync NTP, let WS state machine take over
    ESP_LOGI(CORE_TAG, "WiFi up → NTP sync");
    Time().sync();
    _lastNtpSync = now;
  }

  if(prevNet == NetState::CONNECTED && curNet != NetState::CONNECTED) {
    // WiFi just dropped — reset WebSocket cleanly
    ESP_LOGW(CORE_TAG, "WiFi lost → reset WS");
    _websocket.resetForWifi(); 
  }

  // ── 3. Tick WebSocket only when WiFi is up ────────────────
  if (curNet == NetState::CONNECTED) {
    _websocket.update();
  }

  // ── 4. NTP re-sync on interval ────────────────────────────
  if (curNet == NetState::CONNECTED &&
    now - _lastNtpSync > NTP_SYNC_INTERVAL &&
    !Time().isSynced()) {
    _lastNtpSync = now;
    Time().sync();
  }

  // ── 5. Always tick TimeManager ────────────────────────────
  Time().update();

  // ── 6. Update badge if either state changed ───────────────
  if (_wifi.state() != _prevNetState || _websocket.webSocketState() != _prevWsState) {
    _prevNetState = _wifi.state();
    _prevWsState  = _websocket.webSocketState();
    _updateBadge();
  }
}


// ============================================================
//  _updateBadge
//
//  Computes compound [WiFi WS] badge from both state machines.
//  Called whenever either state changes — not every tick.
//
//  WiFi symbols:   · idle   ○ connecting/lost   ● connected   ! failed
//  WS symbols:     - idle   ~ connected         * registered  ! failed
// ============================================================

void AshuraCore::_updateBadge() {
  // ── WiFi symbol ───────────────────────────────────────────
  char wSym;
  switch (_wifi.state()) {
        case NetState::CONNECTED:            wSym = '*'; break;  // filled

        case NetState::CONNECTING:
        case NetState::LOST:                 wSym = 'o'; break;  // trying

        case NetState::FAILED:               wSym = '!'; break;  // gave up

        case NetState::IDLE:
        default:                             wSym = '-'; break;  // no creds
    }
  
  // ── WebSocket symbol ──────────────────────────────────────
  char wsSym;
  switch (_websocket.webSocketState()) {
        case WebSocketState::REGISTERED:            wsSym = '*'; break;
        case WebSocketState::CONNECTED:             wsSym = '~'; break;
        case WebSocketState::CONNECTING:            wsSym = 'o'; break;
        case WebSocketState::FAILED:                wsSym = '!'; break;
        case WebSocketState::IDLE:
        default:                             wsSym = '-'; break;
  }

  char badge[8];
  snprintf(badge, sizeof(badge), "[%c %c]", wSym, wsSym);

  if (_homeScreen) _homeScreen->setConnectionStatus(badge);

  ESP_LOGD(CORE_TAG, "Badge: %s  WiFi:%d  WS:%d",
            badge,
            (int)_wifi.state(),
            (int)_websocket.webSocketState());
}


// ============================================================
//  _buildAppMenu
// ============================================================

void AshuraCore::_buildAppMenu()
{
  // Build item list from Loader registry
  std::vector<AppItem> items;

  for (auto& [id, desc] : _loader.apps())
  {
    AppItem item;
    item.icon = desc.icon;
    item.name = desc.name;
    item.anim = nullptr;

    // Assign per app id
    if      (id == "wled"    ) item.anim = &wledAnim;
    else if (id == "clock"   ) item.anim = &clockAnim;
    else if (id == "settings") item.anim = &settingsAnim;
    else if (id == "vibes"   ) item.anim = &vibesAnim;
    else if (id == "games"   ) item.anim = &gamesAnim;

    // Capture id by value for lambda
    std::string capturedId = id;
    item.onLaunch = [this, capturedId]()
      {
        IScreen* s = _loader.buildApp(capturedId, _display);
        if (s)
          _ui.pushScreen(s);
      };
    items.push_back(std::move(item));
  }
  _ui.pushScreen(new AppMenuScreen(_display, items));
}


// ============================================================
//  _pollButton  —  Full press/long-press/release handler
//
//  onShortPress fires on RELEASE if held < LONG_PRESS_MS
//  onLongPress  fires ONCE after LONG_PRESS_MS while held
//  anyPressed   set true on any event (for mood interaction)
// ============================================================

void AshuraCore::_pollButton(
  gpio_num_t pin, 
  BtnState& btn,
  std::function<void()> onShortPress,
  std::function<void()> onLongPress,
  bool& anyPressed)
{
    // gpio_get_level returns 0 (LOW) when pressed (pull-up + active low)
    bool raw = (gpio_get_level(pin) == 0);

    // Debounce
    if (raw != btn.lastRaw) {
      btn.lastDebounce = millis(); // Every time the signal flickers, reset the timer
    }
    btn.lastRaw = raw;

    if ((millis() - btn.lastDebounce) <= BUTTON_DEBOUNCE_MS) return;

    bool isPressed = raw;  // true = physically pressed

    if (isPressed && btn.state == false) {
      // Falling edge — button down
      btn.state      = true;
      btn.pressStart = millis();
      btn.longFired  = false;

    } else if (isPressed && btn.state == true) {
        // Held — check long press
        if (!btn.longFired &&
          (millis() - btn.pressStart) >= LONG_PRESS_MS) {
          btn.longFired = true;
          onLongPress();
          anyPressed = true;
        }

    } else if (!isPressed && btn.state == true) {
        // Rising edge — button released
        btn.state = false;
        if (!btn.longFired) {
          onShortPress();
          anyPressed = true;
        }
    }
}