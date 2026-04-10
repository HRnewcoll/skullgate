/**
 * @file DriverRegistry.h
 * @brief Instantiates and owns all hardware driver adapters for a given board.
 *
 * The DriverRegistry reads a BoardProfile and sets up:
 *   - DisplayAdapter  (LovyanGFX)
 *   - TouchAdapter    (XPT2046 / GT911 / none)
 *   - SdManager       (SPI SD card)
 *   - WifiManager     (passive scan only — Recon-Only safe)
 *   - BleManager      (passive BLE scan only — Recon-Only safe)
 *   - BusManager      (SPI / I2C arbitration)
 *
 * SAFETY: Wi-Fi is initialised in STATION+SCAN mode with no AP, no
 * transmission except probe requests implicit in active scanning.
 * BLE is initialised in passive scan mode — no advertisements emitted.
 * Lab Mode features that transmit are gated by the ModuleManager permission
 * system and are NEVER enabled here.
 */

#pragma once

#include "BoardProfile.h"

// Forward-declare driver classes to keep this header light.
namespace skullgate {
    class DisplayAdapter;
    class TouchAdapter;
    class SdManager;
    class WifiManager;
    class BleManager;
    class BusManager;
}

namespace skullgate {

class DriverRegistry {
public:
    explicit DriverRegistry(const BoardProfile& profile);
    ~DriverRegistry();

    /**
     * @brief Initialise all drivers described by the board profile.
     * Call this once from setup().
     * @return true if all mandatory drivers initialised successfully.
     */
    bool init();

    // ── Accessors ─────────────────────────────────────────────────────────────
    /// Returns the display adapter, or nullptr if none.
    DisplayAdapter* display() const { return _display; }

    /// Returns the touch adapter, or nullptr if not present.
    TouchAdapter*   touch()   const { return _touch; }

    /// Returns the SD manager, or nullptr if SD is absent.
    SdManager*      sd()      const { return _sd; }

    /// Returns the Wi-Fi manager (always present on ESP32).
    WifiManager*    wifi()    const { return _wifi; }

    /// Returns the BLE manager (always present on ESP32 variants with BLE).
    BleManager*     ble()     const { return _ble; }

    /// Returns the bus manager (SPI/I2C arbitration).
    BusManager*     bus()     const { return _bus; }

private:
    const BoardProfile& _profile;

    DisplayAdapter* _display;
    TouchAdapter*   _touch;
    SdManager*      _sd;
    WifiManager*    _wifi;
    BleManager*     _ble;
    BusManager*     _bus;
};

} // namespace skullgate
