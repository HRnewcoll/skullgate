/**
 * @file BusManager.h
 * @brief SPI and I2C bus arbitration for board-profile-driven hardware.
 *
 * Multiple devices may share the same SPI bus (display, touch, SD, radio).
 * BusManager initialises each bus once and provides a locking mechanism
 * so that drivers can acquire/release the bus safely.
 */

#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "../core/BoardProfile.h"

namespace skullgate {

class BusManager {
public:
    explicit BusManager(const BoardProfile& profile);

    /// Initialise all buses described in the board profile.
    bool init();

    // ── SPI ───────────────────────────────────────────────────────────────────
    /// Begin an SPI transaction on the VSPI bus.
    void beginVspi(uint32_t freq, uint8_t mode = SPI_MODE0,
                   uint8_t bitOrder = MSBFIRST);
    void endVspi();

    /// Begin an SPI transaction on the HSPI bus.
    void beginHspi(uint32_t freq, uint8_t mode = SPI_MODE0,
                   uint8_t bitOrder = MSBFIRST);
    void endHspi();

    // ── I2C ───────────────────────────────────────────────────────────────────
    /// Returns a reference to the I2C bus instance at the given index.
    TwoWire& i2c(int index = 0);

private:
    const BoardProfile& _profile;
    SPIClass*  _vspi;
    SPIClass*  _hspi;
    TwoWire*   _i2c0;
    TwoWire*   _i2c1;
};

} // namespace skullgate
