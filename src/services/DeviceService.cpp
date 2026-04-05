#include "DeviceService.h"
#include "config.h"
#include "core/EventBus.h"
#include "esp_log.h"

static const char* TAG = "DeviceService";

// Forward declaration - WebSocketManager is not directly accessible here;
// we use EventBus to send outgoing messages too.
// Add AppEvent::SendMessage if you need WS sends from services.

bool DeviceService::handleMessage(const JsonDocument& doc){
    const char* type = doc["type"];
    if(!type) return false;

    if(strcmp(type, "command") == 0){
        _handleCommand(doc);
        return true;
    }

    if (strcmp(type, "notification") == 0) {
        _handleNotification(doc);
        return true;
    }

    return false;
}

void DeviceService::_handleCommand(const JsonDocument& doc){
    const char* command = doc["data"]["command"];
    if(!command) return;

    ESP_LOGI(TAG, "Command: %s", command);

    if (strcmp(command, "display_message") == 0) {
        const char* text = doc["data"]["text"];
        if (text) {
            Bus().publish(AppEvent::NotificationReceived, std::string(text));
            _sendAck(command);
        }
    }

    // TODO: Add more commands here...
    // else if (strcmp(command, "reboot") == 0) { ESP.restart(); }
    // else if (strcmp(command, "set_brightness") == 0) { ... }
}

void DeviceService::_handleNotification(const JsonDocument& doc) {
    const char* event = doc["data"]["event"];
    if (!event) return;

    ESP_LOGI(TAG, "Event: %s", event);
     Bus().publish(AppEvent::NotificationReceived, std::string(event));
}

void DeviceService::_sendAck(const char* command) {
  // To send WS messages from services, publish a "SendMessage" event
  // and have WebSocketManager subscribe to it.
  // For now: log only.
  ESP_LOGI(TAG, "ACK: %s", command);

  // TODO: publish AppEvent::SendMessage with JSON payload
  // Bus().publish(AppEvent::SendMessage, ackJson);
}