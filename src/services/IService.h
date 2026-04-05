#pragma once
#include <ArduinoJson.h>

// ============================================
// IService - Interface for all message handlers
//
// Implement handleMessage() to consume messages
// by type. Return true if consumed, false to pass
// the message to the next service in the chain.
// ============================================

class IService {
    public: 
        // Initialize the service (e.g., set up state or register with EventBus)
        virtual void        init() = 0;

        // Process an incoming JSON message; 
        // return true if the service consumed it, false to pass to the next service
        virtual bool        handleMessage(const JsonDocument& doc) = 0;

        // Return the service name (useful for logging/debugging)
        virtual const char* getName() const = 0;

        // Virtual destructor ensures proper cleanup of derived classes
        virtual             ~IService() = default;  
};