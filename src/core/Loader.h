#pragma once

#include <string>
#include <map>
#include <functional>

#include "../ui/screens/IScreen.h"
#include "../core/DisplayManager.h"

// ============================================================
// Loader — Application registry and builder
// ============================================================
//
// Purpose:
// --------
// Acts as an application registry and builder.
//
// Responsibilities:
// -----------------
// ✔ Keeps track of all available applications
// ✔ Stores app metadata (id, name, icon)
// ✔ Creates application screens on demand
//
// How it works:
// -------------
// ✔ Applications register themselves at boot: loader.registerApp({...});
// ✔ Later, any part of the firmware can request: loader.buildApp("clock");
//
// Loader then calls the app’s factory function
// and returns a newly created IScreen*.
//
// IMPORTANT:
// ----------
// Loader ONLY builds screens.
// It does NOT:
//  ❌ Manage navigation
//  ❌ Switch views
//  ❌ Push scenes
// Example Usage:
// --------------
//
// Registering an app:
//
//   loader.registerApp({
//       "clock",
//       "Clock",
//       "CL",
//       createClockScreen
//   });
//
// Launching an app:
//
//   IScreen* screen = loader.buildApp("clock", display);
//   if(screen) {
//       sceneManager.nextScene(Scene::Clock);
//       viewDispatcher.switchTo(View::Clock);
//   }
//
// ============================================================

using AppFactory = std::function<IScreen*(DisplayManager&)>;

struct AppDescriptor {
    std::string      appid;
    std::string      name;
    std::string      icon;
    AppFactory       factory;
};

class Loader {
    public:
    // ========================================================
    // App Registration
    // ========================================================

    void registerApp(AppDescriptor descriptor){
        _apps[descriptor.appid] = descriptor;
    }

    // ========================================================
    // App Launching
    // ========================================================

    // Returns a new IScreen* for the caller to push; nullptr if unknown
    IScreen* buildApp(const std::string& appid, DisplayManager& display){
        auto it = _apps.find(appid);
        if(it == _apps.end()) return nullptr;
        return it->second.factory(display);
    }

    // ========================================================
    // Listing
    // ========================================================
    const std::map<std::string, AppDescriptor>& apps() const { return _apps; }

    bool appExists(const std::string& appid) const {
        return _apps.find(appid) != _apps.end();
    }

    private:
        std::map<std::string, AppDescriptor> _apps;
};

