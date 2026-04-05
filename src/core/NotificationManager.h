#pragma once

#include <string>
#include <cstdint>
#include <algorithm>

#include "EventBus.h"
#include "config.h"
#include "hal.h"

// ================================================================
//  NotificationManager  —  Ring buffer, session-only
//
//  Holds up to MAX_NOTIFICATIONS notifications in a circular buffer.
//  Oldest is overwritten when full.
//
//  Fields designed for future persistence (NVS) and actionability
//  (Todoist, Calendar) without requiring rework:
//    id         → auto-increment, stable across mark/read ops
//    source     → "system" | "todoist" | "calendar" etc (String for now,
//                  enum later when source set is stable)
//    actionType → empty now, populated by future integrations
//    actionData → empty now, JSON payload for action
//
//  Persistence upgrade path:
//    Add save() after push() / markRead()
//    Add load() in init()
//    NotificationManager interface stays identical
//
//  Global accessor: Notifs()
// ================================================================

// ── Notification type ─────────────────────────────────────────
//
//  SYSTEM  — low priority, OS events (boot, WiFi connected, etc.)
//  ALERT   — high priority, time-sensitive warnings (WiFi fail, errors)
//  MESSAGE — normal priority, incoming from server / integrations
enum class NotificationType : uint8_t {
    SYSTEM  = 0,
    ALERT   = 1,
    MESSAGE = 2,
};

// Helper - return string literal for display
inline char* notifTypeName(NotificationType t){
    switch(t){
        case NotificationType::SYSTEM:
            return "SYS";
        case NotificationType::ALERT:
            return "ALERT";
        case NotificationType::MESSAGE:
            return "MSG";
    }
    return "UNK";
}

// ── Notification struct ───────────────────────────────────────
struct Notification {
    uint32_t                    id;         //auto-increment (unique per session)
    std::string                 title;      
    std::string                 body;
    NotificationType            type;       // SYSTEM | ALERT | MESSAGE (1 byte)
    std::string                 source;     // "system" | "todoist" | "calendar" | ...
    std::string                 actionType; // "" | "open_screen" | "todoist_done" | "snooze"
    std::string                 actionData; // JSON payload for action — empty for now
    uint32_t                    timestamp;  // millis() when pushed
    bool                        read;       // false = unread, true = read

    bool hasAction() const { return !actionType.empty(); }

};


// ── NotificationManager ───────────────────────────────────────
class NotificationManager {
    public:
        // ── Push ─────────────────────────────────────────────────

        int push(
            const std::string&       title,
            const std::string&       body        = "",
            NotificationType         type        = NotificationType::SYSTEM,
            const std::string&       source      = "system",
            const std::string&       actionType  = "",
            const std::string&       actionData  = ""
        ){
            Notification n;
            n.id            = _nextId++;
            n.title         = title;
            n.body          = body;
            n.type          = type;
            n.source        = source;
            n.actionType    = actionType;
            n.actionData    = actionData;
            n.timestamp     = millis();
            n.read          = false;

            _buffer[_head] = n;

            _head = (_head + 1) % MAX_NOTIFICATION_BUFFER;
            if(_count < MAX_NOTIFICATION_BUFFER){
                _count++;
            } else {
                _tail = (_tail + 1) % MAX_NOTIFICATION_BUFFER; // overwrite the oldest
            }

            _unreadCount++;
            return 0; // newest is always logical index 0
        }


        // ── Access ────────────────────────────────────────────────

        // Logical index: 0 = newest, count-1 = oldest
        const Notification* get(int logicalIdx) const {
            if(logicalIdx < 0 || logicalIdx >= _count) return nullptr;
            int physical = (_head - 1 - logicalIdx + MAX_NOTIFICATION_BUFFER)%MAX_NOTIFICATION_BUFFER;
            return &_buffer[physical];
        }

        // Mutable access for marking read and delete
        Notification* getMutable(int logicalIdx){
            if(logicalIdx < 0 || logicalIdx >= _count) return nullptr;
            int physical = (_head - 1 - logicalIdx + MAX_NOTIFICATION_BUFFER)%MAX_NOTIFICATION_BUFFER;
            return &_buffer[physical];
        }

        // Getters
        int     count()         const { return _count; }
        int     unreadCount()   const { return _unreadCount; }
        bool    isEmpty()       const { return _count == 0; }

        // Return logical index of most recent unread, or -1 if none
        int latestUnreadIndex() const {
            for(int i=0; i<_count; i++){
                const Notification* n = get(i);
                if(n && !n->read) return i;
            }
            return -1;
        }

        // Returns pointer to newest notification, or nullptr if empty
        const Notification* latest() const {
            return get(0);
        }


        // ── Mark read ─────────────────────────────────────────────
        void markRead(int logicalIndex){
            Notification* n = getMutable(logicalIndex);
            if(n && !n->read){
                n->read = true;
                if(_unreadCount > 0) _unreadCount--;
            }
        }

        void markAllRead(){
            for(int i=0; i<_count; i++){
                Notification* n = getMutable(i);
                if(n) n->read = true;
            }
            _unreadCount = 0;
        }

        
        // ── Delete ────────────────────────────────────────────────
        // Removes notification at logical index, rebuilds ring buffer without the deleted item.

        bool remove(int logicalIndex){
            if(logicalIndex < 0 || logicalIndex >= _count) return false;

            // Adjust unread count before removing
            Notification* n = getMutable(logicalIndex);
            if(n && !n->read){
                if(_unreadCount > 0)_unreadCount--;
            }

            // Copy all except the deleted item into temp array (newest first)
            Notification temp[MAX_NOTIFICATION_BUFFER];
            int tempCount = 0;
            for(int i=0; i<_count; i++){
                if(i == logicalIndex) continue;
                const Notification* src = get(i);
                if(src) temp[tempCount++] = *src;
            }

            // Rebuild ring buffer - insert oldest first so newest ends at head-1
            _count = 0;
            _head  = 0;
            _tail  = 0;
            for (int i = tempCount - 1; i >= 0; i--) {
                _buffer[_head] = temp[i];
                _head = (_head + 1) % MAX_NOTIFICATION_BUFFER;
                _count++;
            }
    
            return true;
        }

        // ── Utility ───────────────────────────────────────────────
        std::string timeAgo(uint32_t timestamp) const {
            uint32_t elapsed = (millis() - timestamp) / 1000; // seconds
            if(elapsed < 60) return "just now";
            if(elapsed < 3600) return std::to_string(elapsed / 60) + "m ago";
            if(elapsed < 86400) return std::to_string(elapsed / 3600) + "h ago";
            return std::to_string(elapsed / 86400) + "d ago";
        }

        //  Overlaod - time ago for notification at logical index
        std::string timeAgo(int logicalIndex) const {
            const Notification* n = get(logicalIndex);
            if(!n) return "";
            return timeAgo(n->timestamp);
        } 
        

    private:
        Notification    _buffer[MAX_NOTIFICATION_BUFFER];
        int             _head = 0;              // next write position
        int             _tail = 0;              // oldest item position
        int             _count = 0;
        int             _unreadCount = 0;
        uint32_t        _nextId = 1;            // auto-increment ID
};

// ── Global accessor - Single instance, accessible everywhere via Notifs() ───
inline NotificationManager& NotifMgr() { 
    static NotificationManager instance;
    return instance;
}