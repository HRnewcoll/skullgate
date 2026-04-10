/**
 * @file WifiManager.h
 * @brief Wi-Fi driver — passive scan only in Recon-Only mode.
 *
 * ── Safety Model ────────────────────────────────────────────────────────────
 * On boot, Wi-Fi is set to STA mode with no AP association.
 * The only operation available in Recon-Only mode is a PASSIVE scan
 * (listens for beacon frames; does NOT send probe requests).
 *
 * Active operations (connect, transmit) require Lab Mode and explicit
 * "wifi_connect" or "wifi_tx" permissions granted by ModuleManager.
 *
 * Modules MUST NOT call WiFi.begin() or any transmit API directly.
 * All access goes through CoreAPI → WifiManager.
 */

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "../core/CoreAPI.h"   // for ApInfo

namespace skullgate {

class WifiManager {
public:
    WifiManager();

    /// Initialise Wi-Fi in station mode (no AP connection). Safe to call once.
    bool init();

    bool isReady() const { return _ready; }

    /**
     * @brief Perform a passive Wi-Fi scan.
     *
     * Uses ESP32 passive scan (scan_type = WIFI_SCAN_TYPE_PASSIVE).
     * Does NOT associate with any AP.
     * Returns a vector of discovered APs, sorted by RSSI (strongest first).
     */
    std::vector<ApInfo> scan();

    /**
     * @brief Connect to an AP.
     * Lab Mode ONLY — must be called only after permission check.
     * @param ssid      SSID to connect to
     * @param password  WPA2 passphrase (empty for open)
     * @return true if association succeeds within timeout
     */
    bool connect(const String& ssid, const String& password,
                 uint32_t timeoutMs = 10000);

    /// Disconnect from any AP.
    void disconnect();

    /// Returns true if currently associated with an AP.
    bool isConnected() const;

    /// Returns the current IP address string (or "" if not connected).
    String ipAddress() const;

private:
    bool _ready;
};

} // namespace skullgate
