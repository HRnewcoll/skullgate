/**
 * @file ModuleManager.h
 * @brief Scans the SD card for modules, validates manifests, enforces
 *        permissions, and manages the module lifecycle.
 *
 * ── Module layout on SD ──────────────────────────────────────────────────────
 *   /modules/
 *     wifi_scanner/
 *       manifest.json      ← required
 *       wifi_scanner.cpp   ← compiled into firmware (not loaded at runtime)
 *     board_test/
 *       manifest.json
 *       board_test.cpp
 *
 * ── manifest.json schema ─────────────────────────────────────────────────────
 * {
 *   "id":          "wifi_scanner",
 *   "name":        "Wi-Fi Scanner",
 *   "version":     "1.0.0",
 *   "author":      "SkullGate",
 *   "description": "Passive Wi-Fi AP scanner",
 *   "permissions": ["wifi_scan", "sd_write", "ui"],
 *   "lab_mode":    false
 * }
 *
 * ── Permission model ─────────────────────────────────────────────────────────
 * Permissions are declared in manifest.json and enforced at runtime.
 * Modules requesting "lab_mode": true are only loaded when Lab Mode is active.
 * Lab Mode requires both an SD flag file (/lab_mode.flag) AND a PIN.
 *
 * Known permissions:
 *   wifi_scan   — passive Wi-Fi scanning (allowed in Recon-Only mode)
 *   wifi_connect — associate with an AP (Lab Mode only)
 *   wifi_tx      — transmit frames (Lab Mode only)
 *   ble_scan     — passive BLE scan (Recon-Only)
 *   ble_tx       — BLE advertising/connect (Lab Mode only)
 *   sd_read      — read files from SD
 *   sd_write     — write/create files on SD
 *   ui           — register LVGL screens
 *   serial       — access Serial interface
 *   net_proxy    — local proxy features (Lab Mode only)
 */

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <functional>
#include "CoreAPI.h"

namespace skullgate {

// ── Module permission flags ──────────────────────────────────────────────────

/// All known permission strings understood by the permission gate.
static const char* const KNOWN_PERMISSIONS[] = {
    "wifi_scan", "wifi_connect", "wifi_tx",
    "ble_scan",  "ble_tx",
    "sd_read",   "sd_write",
    "ui",        "serial",
    "net_proxy", "esp_now",
    "radio_rx",  "nfc_read",  "ir_tx",
    "ota",       "wifi_promiscuous",
    nullptr
};

/// Permissions that require Lab Mode to be active.
static const char* const LAB_MODE_PERMISSIONS[] = {
    "wifi_connect", "wifi_tx", "ble_tx", "net_proxy",
    "ota", "wifi_promiscuous",
    nullptr
};

// ── Manifest ─────────────────────────────────────────────────────────────────

struct ModuleManifest {
    String id;
    String name;
    String version;
    String author;
    String description;
    std::vector<String> permissions;
    bool   lab_mode;   ///< true = only loaded when Lab Mode is active
    bool   valid;      ///< true if manifest was parsed without errors
};

// ── Module interface ─────────────────────────────────────────────────────────

/**
 * @brief Every module must implement this interface.
 *
 * The lifecycle is: init() → start() → [loop() called from main loop] → stop()
 * Modules are responsible for registering their own LVGL screens in init().
 */
struct IModule {
    virtual ~IModule() = default;

    /// Called once after permissions are granted. Return false to abort load.
    virtual bool init(CoreAPI& api) = 0;

    /// Called when the module is activated (e.g. user launches it).
    virtual void start() = 0;

    /// Called every main-loop iteration while the module is active.
    virtual void loop() = 0;

    /// Called when the module is deactivated or firmware shuts down.
    virtual void stop() = 0;

    /// Returns this module's manifest.
    virtual const ModuleManifest& manifest() const = 0;
};

// ── Factory registry ─────────────────────────────────────────────────────────

using ModuleFactory = std::function<IModule*()>;

/**
 * @brief Static registry of module factory functions.
 *
 * Compiled-in modules register themselves here so the ModuleManager can
 * instantiate them after validating their manifest.
 *
 * Usage (in module .cpp):
 * @code
 *   REGISTER_MODULE("wifi_scanner", []() -> IModule* { return new WifiScannerModule(); });
 * @endcode
 */
class ModuleRegistry {
public:
    static ModuleRegistry& instance();

    void registerFactory(const String& id, ModuleFactory factory);
    IModule* create(const String& id) const;

    /// Returns all registered module IDs.
    std::vector<String> listIds() const {
        std::vector<String> ids;
        for (const auto& e : _factories) ids.push_back(e.first);
        return ids;
    }

private:
    ModuleRegistry() = default;
    std::vector<std::pair<String, ModuleFactory>> _factories;
};

/// Convenience macro to register a module at static-init time.
#define REGISTER_MODULE(id, factory_lambda) \
    static bool _sg_reg_##__LINE__ = []{ \
        skullgate::ModuleRegistry::instance().registerFactory(id, factory_lambda); \
        return true; \
    }()

// ── ModuleManager ────────────────────────────────────────────────────────────

class ModuleManager {
public:
    explicit ModuleManager(CoreAPI& api);

    /**
     * @brief Scan /modules/ on SD, load manifests, instantiate permitted modules.
     * @return Number of modules successfully loaded.
     */
    int loadAll();

    /**
     * @brief Activate a module by its id string.
     * @return false if id not found or module refused to start.
     */
    bool activate(const String& id);

    /**
     * @brief Deactivate the currently active module.
     */
    void deactivate();

    /// Forward loop() to the active module (if any).
    void loop();

    /// Returns the list of loaded module manifests.
    const std::vector<ModuleManifest>& manifests() const { return _manifests; }

    /// Returns true if Lab Mode is currently active.
    bool isLabMode() const { return _labMode; }

    /**
     * @brief Attempt to unlock Lab Mode.
     * Requires /lab_mode.flag on SD and the correct PIN.
     * @param pin   User-entered PIN string
     * @return true if Lab Mode unlocked successfully
     */
    bool unlockLabMode(const String& pin);

    /// Lock Lab Mode (reverts to Recon-Only).
    void lockLabMode();

private:
    bool _checkPermissions(const ModuleManifest& manifest) const;
    bool _parseManifest(const String& json, ModuleManifest& out) const;

    CoreAPI&                  _api;
    bool                      _labMode;
    std::vector<ModuleManifest> _manifests;
    std::vector<IModule*>       _modules;
    IModule*                    _active;
};

} // namespace skullgate
