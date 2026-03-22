/**
 * @file WifiScannerModule.h
 * @brief Wi-Fi Scanner module interface.
 *
 * Implements IModule for the wifi_scanner plugin:
 *   - Passive Wi-Fi scan (beacon frames only — no probe requests)
 *   - Logs scan results as JSON to /wifi_scans/YYYYMMDD_HHMMSS.json on SD
 *   - Registers an LVGL screen showing the AP list with RSSI bars
 *
 * Permissions required: wifi_scan, sd_write, ui
 * Lab Mode: NOT required — passive scan is Recon-Only safe
 */

#pragma once

#include "../../core/ModuleManager.h"

namespace skullgate {

class WifiScannerModule : public IModule {
public:
    WifiScannerModule();
    ~WifiScannerModule() override = default;

    bool init(CoreAPI& api) override;
    void start() override;
    void loop() override;
    void stop() override;
    const ModuleManifest& manifest() const override { return _manifest; }

private:
    void _buildScreen();
    void _runScan();
    void _updateList(const std::vector<ApInfo>& aps);
    void _logToSD(const std::vector<ApInfo>& aps);

    CoreAPI*       _api;
    ModuleManifest _manifest;
    bool           _running;

    // Scan interval — every 30 seconds while module is active
    uint32_t _lastScan;
    static constexpr uint32_t SCAN_INTERVAL_MS = 30000;

    // LVGL widgets
    lv_obj_t* _screen;
    lv_obj_t* _list;
    lv_obj_t* _lblStatus;
    lv_obj_t* _spinner;
};

} // namespace skullgate

// ── Self-registration ─────────────────────────────────────────────────────────
// This causes the module to register itself at static-init time.
// Include this header once in main.cpp to pull in the registration.
#include "../../core/ModuleManager.h"

namespace {
    REGISTER_MODULE("wifi_scanner", []() -> skullgate::IModule* {
        return new skullgate::WifiScannerModule();
    });
}
