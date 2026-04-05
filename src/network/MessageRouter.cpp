#include "MessageRouter.h"
#include "ArduinoJson.h"
#include "esp_log.h"

static const char* TAG = "MessageRouter";

//adds service to the list
void MessageRouter::registerService(IService* service){
    _services.push_back(service);
    ESP_LOGI(TAG, "Service registered: %s", service->getName());
}

void MessageRouter::route(const std::string& json){
    JsonDocument doc;
    if(deserializeJson(doc, json) != DeserializationError::Ok){
        ESP_LOGE(TAG, "MessageRouter: bad JSON");
        return;
    }

    for(auto* svc: _services){
        if (svc->handleMessage(doc)) {
            ESP_LOGI(TAG, "MessageRouter: Handled by: %s", svc->getName());
            return;
        }
    }

    ESP_LOGW(TAG, "MessageRouter: No service handled message type: %s", doc["type"].as<const char*>());
}