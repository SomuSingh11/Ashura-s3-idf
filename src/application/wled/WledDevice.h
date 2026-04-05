#pragma once
#include <string>

// ================================================================
//  WledDevice  —  A single discovered WLED device
//
//  Populated by WledDiscovery (mDNS) or loaded from NVS.
//  Passed around by value — small struct, safe to copy.
// ================================================================

struct WledDevice {
    std::string  name;      // Friendly device name from /json/info
    std::string  ip;        // Friendly device name from /json/info
    int     port = 80; // HTTP port (default = 80)

    bool isValid() const { return !ip.empty(); }

    std::string baseUrl() const { return "http://" + ip + ":" + std::to_string(port); }

    std::string stateUrl() const { return baseUrl() + "/json/state"; } // URL to control LED state (on/off, brightness, effects)
    std::string infoUrl()  const { return baseUrl() + "/json/info"; }  // URL to fetch device information
    std::string jsonUrl()  const { return baseUrl() + "/json"; }       // General JSON API endpoint
};