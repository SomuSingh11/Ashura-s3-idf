#pragma once
#include <vector>
#include <string>
#include <mutex>

#include "hal.h"
#include "../../application/wled/WledDevice.h"
#include "../../application/wled/WledClient.h"
#include "../../application/wled/WledState.h"
#include "../../application/wled/WledDiscovery.h"

// ================================================================
//  WledManager  —  Central WLED controller
//
//  Owns:
//    - list of known devices (loaded from NVS + updated by discovery)
//    - active device index
//    - WledClient (HTTP)
//    - current WledState
//    - effects list cache (fetched once per device connection)
//
//  Lives in AshuraCore as `WledManager _wled`.
//  Passed by reference to all WLED screens.
//
//  NVS layout (namespace "wled"):
//    active_idx         → last selected device index
//    dev_count          → number of saved devices
//    dev_0_ip           → device 0 IP
//    dev_0_name         → device 0 name
//    dev_0_port         → device 0 port
//    dev_1_ip ... etc
// ================================================================

class WledManager {
    public:
    // ── Lifecycle ─────────────────────────────────────────────
    void begin();

    // ── Devices ───────────────────────────────────────────────
    const std::vector<WledDevice>&  devices()       const { return _devices; }
    int                             deviceCount()   const { return _devices.size(); }
    bool                            hasDevices()    const { return !_devices.empty(); }
    const WledDevice*               activeDevice()  const;
    int                             activeIndex()   const { return _activeIndex; }

    // ── Connect ───────────────────────────────────────────────
    // Sets active device, fetches state + effects, saves index to NVS.
    bool connect(int idx);          // Returns true on success.

    // ── Discovery ─────────────────────────────────────────────
    // mDNS scan, merges new devices, saves to NVS.
    int discover();                 // Returns number of NEW devices found.

    void removeDevice(int idx);     // Remove a device by index

    // ── State + client ────────────────────────────────────────
    WledState&        state()        { return _state; }
    const WledState&  state()  const { return _state; }
    WledClient&       client()       { return _client; }

    // ── Effects cache ─────────────────────────────────────────
    const std::vector<std::string>& effects()           const { return _effects; }
    int                             effectCount()       const { return _effects.size(); }
    std::string                     effectName(int idx) const;

    // ── NVS ───────────────────────────────────────────────────
    void loadFromNVS();
    void saveToNVS();

    private:
        std::vector<WledDevice>     _devices;
        std::vector<std::string>    _effects;
        WledState                   _state;
        WledClient                  _client;
        int                         _activeIndex = 0;

        bool _hasDevice(const std::string& ip) const;
};