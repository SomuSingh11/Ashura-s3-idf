#pragma once

#include <string>
#include <map>
/*
============================================================
 AshuraRecord — Global Service Registry

 Purpose:
   Provides a centralized registry where core system services
   can be registered and accessed by name.

 Why this exists:
   - Avoid passing service pointers everywhere
   - Reduce coupling between modules
   - Enable OS-like service discovery

 Typical Usage:

   // During system boot
   record_create(RECORD_GUI, &guiService);

   // Anywhere in firmware
   GuiService* gui = record_get<GuiService>(RECORD_GUI);
   gui->draw();

============================================================
*/

// ============================================================
// Well-known record names (prevents magic strings)
// ============================================================

#define RECORD_GUI          "gui"
#define RECORD_WIFI         "wifi"
#define RECORD_WEBSOCKET    "websocket"
#define RECORD_DISPLAY      "display"
#define RECORD_LOADER       "loader"
#define RECORD_NOTIFICATION "notification"
#define RECORD_WLED         "wled"
// #define RECORD_POWER        "power"
// #define RECORD_DIALOGS      "dialogs"
// #define RECORD_STORAGE      "storage"
// #define RECORD_EXPANSION    "expansion"

// ============================================================
// AshuraRecord — Singleton Service Registry
// ============================================================
class AshuraRecord {
    public:
        // This creates one global registry.
        // Entire firmware → shares ONE registry
        static AshuraRecord& instance(){
            static AshuraRecord rec;
            return rec;
        }

        // Register a service pointer under a name (call once, during boot)
        void create(const char* name, void* ptr){
            _registry[std::string(name)] = ptr;
        }

        // Retrieve a service pointer (returns nullptr if not found)
        void* open(const char* name){
            auto it = _registry.find(std::string(name));
            return (it != _registry.end()) ? it->second : nullptr;
        }

        // Unregister (for orderly shutdown)
        void destroy(const char* name){
            _registry.erase(std::string(name));
        }
    
    private:
        AshuraRecord() = default;
        std::map<std::string, void*> _registry;
};

// ============================================================
// Convenience Wrapper Functions
// ============================================================
inline void record_create(const char* name, void* ptr) {
    AshuraRecord::instance().create(name, ptr);
}

inline void* record_open(const char* name) {
    return AshuraRecord::instance().open(name);
}

inline void record_destroy(const char* name){
    AshuraRecord::instance().destroy(name);
}

// ============================================================
// Typed Helper — Safe & Clean Access ⭐
// ============================================================
template<typename T>
inline T* record_get(const char* name) {
    return static_cast<T*>(record_open(name));
}