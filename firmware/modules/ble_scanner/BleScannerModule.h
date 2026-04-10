/**
 * @file BleScannerModule.h
 * @brief BLE Scanner module — passive BLE device discovery.
 *
 * Scans for BLE advertising packets and displays discovered devices
 * (MAC, name, RSSI) in an LVGL scrollable list.  Logs results to SD
 * as JSON.
 *
 * SAFETY: scan is passive (no scan requests, no connections).
 * Permissions: ble_scan, sd_write, ui
 * Lab Mode: NOT required
 */

#pragma once

#include "../../core/ModuleManager.h"

namespace skullgate {

class BleScannerModule : public IModule {
public:
    BleScannerModule();
    ~BleScannerModule() override = default;

    bool init(CoreAPI& api)       override;
    void start()                  override;
    void loop()                   override;
    void stop()                   override;
    const ModuleManifest& manifest() const override { return _manifest; }

private:
    void _buildScreen();
    void _runScan();
    void _updateList(const std::vector<BleDevice>& devs);
    void _logToSD(const std::vector<BleDevice>& devs);

    CoreAPI*       _api;
    ModuleManifest _manifest;
    bool           _running;

    // Scan interval — every 60 s while module is active (BLE scan takes time)
    uint32_t _lastScan;
    static constexpr uint32_t SCAN_INTERVAL_MS = 60000;
    static constexpr uint32_t SCAN_DURATION_MS  = 5000; // 5 s passive window

    // LVGL widgets
    lv_obj_t* _screen;
    lv_obj_t* _list;
    lv_obj_t* _lblStatus;
    lv_obj_t* _spinner;
};

} // namespace skullgate

// ── Self-registration ─────────────────────────────────────────────────────────
namespace {
    REGISTER_MODULE("ble_scanner", []() -> skullgate::IModule* {
        return new skullgate::BleScannerModule();
    });
}
