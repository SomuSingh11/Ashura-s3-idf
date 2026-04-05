#pragma once

#include <string>
#include <vector>

#include "esp_log.h"
#include "mdns.h"

#include "application/wled/WledDevice.h"
#include "application/wled/WledClient.h"

// ================================================================
//  WledDiscovery  —  mDNS scan for WLED devices
//
//  Interface identical to Arduino version.
//  Backend swapped to IDF mdns component.
// ================================================================

class WledDiscovery {
public:
    static int scan(std::vector<WledDevice>& out, WledClient& client) {
        out.clear();

        // Initialize mDNS — safe to call multiple times
        esp_err_t err = mdns_init();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "mDNS init failed: %s",
                     esp_err_to_name(err));
            return 0;
        }

        // Set hostname
        mdns_hostname_set("ashura");
        mdns_instance_name_set("Ashura Core");

        ESP_LOGI(TAG, "Scanning for WLED devices...");

        // Query for _wled._tcp services
        // Blocks for up to 3000ms
        mdns_result_t* results = nullptr;
        err = mdns_query_ptr("_wled", "_tcp", 3000, 10, &results);

        if (err != ESP_OK || !results) {
            ESP_LOGI(TAG, "No WLED devices found");
            mdns_free(results);
            return 0;
        }

        // Count results first
        int found = 0;
        mdns_result_t* r = results;
        while (r) { found++; r = r->next; }
        ESP_LOGI(TAG, "Found %d device(s)", found);

        // Process each result
        r = results;
        while (r) {
            WledDevice dev;

            // Get IP address from address records
            if (r->addr) {
                char ip_str[16];
                esp_ip4addr_ntoa(
                    &r->addr->addr.u_addr.ip4,
                    ip_str,
                    sizeof(ip_str)
                );
                dev.ip = std::string(ip_str);
            }

            // Get port
            dev.port = r->port > 0 ? r->port : 80;

            // Hostname as fallback name
            if (r->hostname) {
                dev.name = std::string(r->hostname);
            }

            if (dev.isValid()) {
                // Try to get friendly name from /json/info
                client.setDevice(dev);
                std::string friendlyName;
                if (client.fetchInfo(friendlyName) &&
                    !friendlyName.empty()) {
                    dev.name = friendlyName;
                }

                out.push_back(dev);
                ESP_LOGI(TAG, "+ %s @ %s",
                         dev.name.c_str(), dev.ip.c_str());
            }

            r = r->next;
        }

        mdns_free(results);
        return (int)out.size();
    }

private:
    static constexpr const char* TAG = "WledDiscovery";
};