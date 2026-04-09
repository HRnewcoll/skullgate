/**
 * @file OtaUpdateModule.h
 * @brief OTA firmware update module.
 *
 * Downloads and installs a firmware binary from a URL specified in
 * /ota/config.json on the SD card.  Performs the update using the
 * ESP32 Arduino Update library.
 *
 * ── Safety ────────────────────────────────────────────────────────────────────
 * This module requires Lab Mode because it must connect to an external server
 * and flash new firmware, both of which constitute active network operations.
 * The OTA config is read exclusively from the SD card so no remote code
 * execution without physical SD card access is possible.
 *
 * ── OTA config format (/ota/config.json) ─────────────────────────────────────
 * {
 *   "url":     "http://192.168.1.100/firmware.bin",
 *   "ssid":    "MyLab",
 *   "password":"secret"
 * }
 *
 * Permissions: wifi_connect, sd_read, ui, ota
 * Lab Mode: REQUIRED
 */

#pragma once

#include "../../core/ModuleManager.h"

namespace skullgate {

class OtaUpdateModule : public IModule {
public:
    OtaUpdateModule();
    ~OtaUpdateModule() override = default;

    bool init(CoreAPI& api)       override;
    void start()                  override;
    void loop()                   override;
    void stop()                   override;
    const ModuleManifest& manifest() const override { return _manifest; }

private:
    void _buildScreen();
    void _beginUpdate();
    void _setStatus(const String& msg, bool isError = false);

    CoreAPI*       _api;
    ModuleManifest _manifest;
    bool           _running;
    bool           _updating;

    // LVGL widgets
    lv_obj_t* _screen;
    lv_obj_t* _lblStatus;
    lv_obj_t* _bar;          ///< Progress bar (0–100)
    lv_obj_t* _lblPercent;
    lv_obj_t* _btnStart;
    lv_obj_t* _btnCancel;
};

} // namespace skullgate

// ── Self-registration ─────────────────────────────────────────────────────────
namespace {
    REGISTER_MODULE("ota_update", []() -> skullgate::IModule* {
        return new skullgate::OtaUpdateModule();
    });
}
