/**
 * @file BleManager.h
 * @brief BLE passive scanner driver.
 *
 * ── Safety Model ────────────────────────────────────────────────────────────
 * BLE is always operated in PASSIVE scan mode (scan type 0x00).
 * No scan requests (active scan) are sent.
 * No BLE advertisements are emitted.
 * No BLE connections are initiated.
 *
 * BLE and Wi-Fi share the 2.4 GHz radio on ESP32; the ESP32 coexistence
 * firmware time-multiplexes them automatically.  Wi-Fi performance may be
 * slightly reduced during a BLE scan.
 *
 * Modules MUST NOT call BLEDevice APIs directly. All BLE access goes
 * through CoreAPI → BleManager.
 */

#pragma once

#include <Arduino.h>
#include <vector>

// BleDevice is defined in CoreAPI.h; include it here so BleManager::scan()
// can return the shared type without duplicating the struct.
#include "../core/CoreAPI.h"

namespace skullgate {

// ── BleManager ───────────────────────────────────────────────────────────────

class BleManager {
public:
    BleManager();
    ~BleManager();

    /// Initialise BLE stack in passive-scan-only mode. Returns false on error.
    bool init();

    bool isReady() const { return _ready; }

    /**
     * @brief Perform a passive BLE scan (no scan requests transmitted).
     *
     * Blocks for durationMs milliseconds while listening for advertising packets.
     * Does NOT connect to or interact with any device.
     *
     * @param durationMs  Scan window in milliseconds (min 500, max 10000).
     * @return Vector of discovered BLE devices sorted by RSSI (strongest first).
     */
    std::vector<BleDevice> scan(uint32_t durationMs = 3000);

    /// Release BLE resources (call before re-initialising or when no longer needed).
    void deinit();

private:
    bool _ready;
    bool _bleInited; ///< Guards against double-init of BLEDevice subsystem
};

} // namespace skullgate
