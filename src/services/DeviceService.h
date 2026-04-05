#pragma once

#include "IService.h"
#include "esp_log.h"

// ============================================
// DeviceService
// Handles: command/display_message, notification/*
// Publishes UI updates via EventBus
// ============================================

class DeviceService : public IService {
    public:
        void init()                                 override{}
        bool handleMessage(const JsonDocument& doc) override;
        const char* getName() const                 override{return "Device Service";}

    private:
        void _handleCommand(const JsonDocument& doc);
        void _handleNotification(const JsonDocument& doc);
        void _sendAck(const char* command);
};