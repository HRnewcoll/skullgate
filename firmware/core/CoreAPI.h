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

class CoreAPIImpl : public CoreAPI {
public:
    CoreAPIImpl(WifiManager* wifi, SdManager* sd, UiManager* ui,
                bool labMode = false);

    void setPermissions(const std::vector<String>& perms);
    void setLabMode(bool v) { _labMode = v; }

    bool hasPermission(const String& perm) const override;
    void log(const String& tag, const String& msg) override;
    String readFile(const String& path) override;
    bool writeFile(const String& path, const String& data,
                   bool append = false) override;
    bool deleteFile(const String& path) override;
    std::vector<ApInfo> wifiScan() override;
    void registerScreen(const String& id, lv_obj_t* screen) override;
    void showScreen(const String& id) override;
    bool isLabMode() const override { return _labMode; }

private:
    WifiManager* _wifi;
    SdManager*   _sd;
    UiManager*   _ui;
    bool         _labMode;
    std::vector<String> _permissions;
};

} // namespace skullgate
