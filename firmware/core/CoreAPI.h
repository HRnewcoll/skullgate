/**
 * @file CoreAPI.h
 * @brief The interface exposed to every module.
 *
 * Modules MUST NOT access hardware directly. They interact with SkullGate
 * exclusively through CoreAPI.  This enforces:
 *   - Permission boundaries (only granted permissions are accessible)
 *   - Board-agnostic operation (modules don't need to know pin numbers)
 *   - Consistent logging and storage
 *
 * CoreAPI is constructed by the firmware core and passed to each module's
 * init() function.
 */

#pragma once

#include <Arduino.h>
#include <vector>
#include <functional>

// Forward-declare lvgl to keep this header free of LVGL includes where unused.
struct _lv_obj_t;
typedef _lv_obj_t lv_obj_t;

namespace skullgate {

// ── Wi-Fi scan result ────────────────────────────────────────────────────────

struct ApInfo {
    String  ssid;
    String  bssid;    ///< MAC as "AA:BB:CC:DD:EE:FF"
    int8_t  rssi;     ///< dBm
    uint8_t channel;
    uint8_t encryption; ///< WIFI_AUTH_* constant
};

// ── BLE scan result ──────────────────────────────────────────────────────────

struct BleDevice {
    String  mac;         ///< MAC as "AA:BB:CC:DD:EE:FF"
    String  name;        ///< Advertised name (empty if not broadcast)
    int8_t  rssi;        ///< dBm
    bool    connectable;
    uint8_t addrType;    ///< 0=public, 1=random
};

// ── ESP-NOW message ───────────────────────────────────────────────────────────

struct EspNowMessage {
    uint8_t src_mac[6];
    uint8_t data[250];
    uint8_t len;
};

// ── CoreAPI ──────────────────────────────────────────────────────────────────

class CoreAPI {
public:
    // ── Permission gate ───────────────────────────────────────────────────────
    /**
     * @brief Returns true if the current module holds the given permission.
     * Modules should check before attempting restricted operations.
     */
    virtual bool hasPermission(const String& perm) const = 0;

    // ── Logging ───────────────────────────────────────────────────────────────
    /// Log a message to serial and optionally SD log file.
    virtual void log(const String& tag, const String& msg) = 0;

    // ── Storage ───────────────────────────────────────────────────────────────
    /// Requires "sd_read" permission.
    virtual String readFile(const String& path) = 0;

    /// Requires "sd_write" permission.
    virtual bool writeFile(const String& path, const String& data,
                           bool append = false) = 0;

    /// Requires "sd_write" permission.
    virtual bool deleteFile(const String& path) = 0;

    // ── Wi-Fi ─────────────────────────────────────────────────────────────────
    /**
     * @brief Perform a passive Wi-Fi scan (Recon-Only safe).
     * Requires "wifi_scan" permission.
     * @return Vector of discovered APs, or empty if not permitted.
     */
    virtual std::vector<ApInfo> wifiScan() = 0;

    /**
     * @brief Connect to an AP. Lab Mode ONLY.
     * Requires "wifi_connect" permission.
     * @param ssid      Network SSID
     * @param password  WPA2 passphrase (empty for open)
     * @param timeoutMs Connection timeout in ms
     * @return true on success
     */
    virtual bool wifiConnect(const String& ssid, const String& password,
                             uint32_t timeoutMs = 10000) = 0;

    /// Disconnect from current AP. Requires "wifi_connect" permission.
    virtual void wifiDisconnect() = 0;

    /// Returns true if currently associated with an AP.
    virtual bool wifiIsConnected() const = 0;

    /// Returns current IP address string, or "" if not connected.
    virtual String wifiIpAddress() const = 0;

    // ── BLE ───────────────────────────────────────────────────────────────────
    /**
     * @brief Perform a passive BLE scan (Recon-Only safe).
     * Requires "ble_scan" permission.
     * @param durationMs  Scan window in milliseconds (500–10000).
     * @return Vector of discovered BLE devices sorted by RSSI, or empty.
     */
    virtual std::vector<BleDevice> bleScan(uint32_t durationMs = 3000) = 0;

    // ── ESP-NOW ───────────────────────────────────────────────────────────────
    /**
     * @brief Initialise the ESP-NOW protocol.
     * Works in both Recon-Only and Lab Mode.
     * Requires "esp_now" permission.
     * @return true if initialised successfully.
     */
    virtual bool espNowInit() = 0;

    /**
     * @brief Send an ESP-NOW message.
     * Requires "esp_now" permission.
     * @param mac   6-byte destination MAC (use broadcast FF:FF:FF:FF:FF:FF).
     * @param data  Payload (max 250 bytes).
     * @param len   Payload length.
     * @return true if sent (not necessarily received).
     */
    virtual bool espNowSend(const uint8_t mac[6],
                            const uint8_t* data, size_t len) = 0;

    /**
     * @brief Retrieve any ESP-NOW messages received since the last call.
     * Requires "esp_now" permission.
     * Non-blocking; returns immediately with whatever is in the buffer.
     */
    virtual std::vector<EspNowMessage> espNowReceive() = 0;

    // ── UI ────────────────────────────────────────────────────────────────────
    /**
     * @brief Register an LVGL screen object as a named module screen.
     * Requires "ui" permission.
     * The home dashboard will show this screen in the module launcher.
     * @param id     Short screen identifier (matches module id)
     * @param screen LVGL screen object created by the module
     */
    virtual void registerScreen(const String& id, lv_obj_t* screen) = 0;

    /// Navigate to a previously registered screen by id.
    virtual void showScreen(const String& id) = 0;

    // ── Lab Mode ──────────────────────────────────────────────────────────────
    /// Returns true when Lab Mode is currently active.
    virtual bool isLabMode() const = 0;

    virtual ~CoreAPI() = default;
};

// ── Concrete implementation ──────────────────────────────────────────────────
// (forward-declared here; implemented in CoreAPIImpl.cpp)
class WifiManager;
class SdManager;
class UiManager;
class BleManager;

class CoreAPIImpl : public CoreAPI {
public:
    CoreAPIImpl(WifiManager* wifi, SdManager* sd, UiManager* ui,
                BleManager* ble = nullptr,
                bool labMode = false);

    void setPermissions(const std::vector<String>& perms);
    void setLabMode(bool v) { _labMode = v; }

    bool hasPermission(const String& perm) const override;
    void log(const String& tag, const String& msg) override;
    String readFile(const String& path) override;
    bool writeFile(const String& path, const String& data,
                   bool append = false) override;
    bool deleteFile(const String& path) override;

    // Wi-Fi
    std::vector<ApInfo> wifiScan() override;
    bool wifiConnect(const String& ssid, const String& password,
                     uint32_t timeoutMs = 10000) override;
    void wifiDisconnect() override;
    bool wifiIsConnected() const override;
    String wifiIpAddress() const override;

    // BLE
    std::vector<BleDevice> bleScan(uint32_t durationMs = 3000) override;

    // ESP-NOW
    bool espNowInit() override;
    bool espNowSend(const uint8_t mac[6],
                    const uint8_t* data, size_t len) override;
    std::vector<EspNowMessage> espNowReceive() override;

    // UI
    void registerScreen(const String& id, lv_obj_t* screen) override;
    void showScreen(const String& id) override;

    // Misc
    bool isLabMode() const override { return _labMode; }

    /// Called by the ESP-NOW receive callback to queue a message.
    void _espNowEnqueue(const uint8_t src[6], const uint8_t* data, int len);

private:
    WifiManager* _wifi;
    SdManager*   _sd;
    UiManager*   _ui;
    BleManager*  _ble;
    bool         _labMode;
    std::vector<String>        _permissions;
    std::vector<EspNowMessage> _espNowQueue;
    bool                       _espNowReady;
};

} // namespace skullgate
