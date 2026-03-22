/**
 * @file DriverRegistry.cpp
 * @brief Implements DriverRegistry — instantiates drivers from a BoardProfile.
 */

#include "DriverRegistry.h"
#include "../drivers/DisplayAdapter.h"
#include "../drivers/TouchAdapter.h"
#include "../drivers/SdManager.h"
#include "../drivers/WifiManager.h"
#include "../drivers/BusManager.h"

namespace skullgate {

DriverRegistry::DriverRegistry(const BoardProfile& profile)
    : _profile(profile)
    , _display(nullptr)
    , _touch(nullptr)
    , _sd(nullptr)
    , _wifi(nullptr)
    , _bus(nullptr)
{}

DriverRegistry::~DriverRegistry() {
    // Drivers are heap-allocated; clean up in reverse init order.
    delete _wifi;
    delete _sd;
    delete _touch;
    delete _display;
    delete _bus;
}

bool DriverRegistry::init() {
    bool ok = true;

    // ── Bus manager must be first (SPI/I2C arbitration) ──────────────────────
    _bus = new BusManager(_profile);
    if (!_bus->init()) {
        Serial.println("[DriverRegistry] BusManager init failed");
        ok = false;
    }

    // ── Display ───────────────────────────────────────────────────────────────
    if (_profile.has("display")) {
        _display = new DisplayAdapter(_profile.display());
        if (!_display->init()) {
            Serial.println("[DriverRegistry] DisplayAdapter init failed");
            ok = false;
        }
    } else {
        Serial.println("[DriverRegistry] No display in profile — headless mode");
    }

    // ── Touch ─────────────────────────────────────────────────────────────────
    if (_profile.has("touch") && _profile.touch().driver != "None") {
        // Pass display dimensions from the profile for accurate coordinate mapping
        uint16_t dw = _profile.has("display") ? _profile.display().width  : 240;
        uint16_t dh = _profile.has("display") ? _profile.display().height : 320;
        _touch = new TouchAdapter(_profile.touch(), dw, dh);
        if (!_touch->init()) {
            Serial.println("[DriverRegistry] TouchAdapter init failed (continuing)");
            // Non-fatal — fallback to button navigation
        }
    }

    // ── SD card ───────────────────────────────────────────────────────────────
    if (_profile.has("sd")) {
        _sd = new SdManager(_profile.sd());
        if (!_sd->init()) {
            Serial.println("[DriverRegistry] SdManager init failed");
            // Non-fatal — logging goes to serial only
        }
    }

    // ── Wi-Fi (passive / Recon-Only on boot) ─────────────────────────────────
    _wifi = new WifiManager();
    if (!_wifi->init()) {
        Serial.println("[DriverRegistry] WifiManager init failed");
        ok = false;
    }

    return ok;
}

} // namespace skullgate
