/**
 * @file BusManager.cpp
 * @brief SPI and I2C bus initialisation for board-profile-driven hardware.
 */

#include "BusManager.h"

namespace skullgate {

BusManager::BusManager(const BoardProfile& profile)
    : _profile(profile)
    , _vspi(nullptr)
    , _hspi(nullptr)
    , _i2c0(nullptr)
    , _i2c1(nullptr)
{}

bool BusManager::init() {
    bool ok = true;

    // ── SPI buses ─────────────────────────────────────────────────────────────
    // Check if any SPI device uses VSPI or HSPI in the profile.
    // The display and touch share VSPI; SD uses HSPI on the 2432S022 profile.

    bool needVspi = (_profile.display().bus == "SPI" &&
                     _profile.display().spi_host == "VSPI") ||
                    (_profile.touch().bus == "SPI"   &&
                     _profile.touch().spi_host == "VSPI");

    bool needHspi = (_profile.has("sd") &&
                     _profile.sd().spi_host == "HSPI");

    if (needVspi) {
        _vspi = new SPIClass(VSPI);
        _vspi->begin(
            _profile.display().sclk,
            _profile.display().miso == 0xFF ? -1 : _profile.display().miso,
            _profile.display().mosi,
            -1 // No default CS here; each device drives its own CS
        );
        Serial.println("[BusManager] VSPI initialised");
    }

    if (needHspi) {
        _hspi = new SPIClass(HSPI);
        _hspi->begin(
            _profile.sd().sclk,
            _profile.sd().miso,
            _profile.sd().mosi,
            -1
        );
        Serial.println("[BusManager] HSPI initialised");
    }

    // ── I2C buses ─────────────────────────────────────────────────────────────
    const auto& i2cs = _profile.i2cs();
    if (i2cs.size() > 0) {
        _i2c0 = new TwoWire(0);
        _i2c0->begin(i2cs[0].pin_sda, i2cs[0].pin_scl, i2cs[0].freq);
        Serial.printf("[BusManager] I2C0 on SDA=%d SCL=%d @ %lu Hz\n",
                      i2cs[0].pin_sda, i2cs[0].pin_scl, (unsigned long)i2cs[0].freq);
    }
    if (i2cs.size() > 1) {
        _i2c1 = new TwoWire(1);
        _i2c1->begin(i2cs[1].pin_sda, i2cs[1].pin_scl, i2cs[1].freq);
        Serial.printf("[BusManager] I2C1 on SDA=%d SCL=%d @ %lu Hz\n",
                      i2cs[1].pin_sda, i2cs[1].pin_scl, (unsigned long)i2cs[1].freq);
    }

    return ok;
}

void BusManager::beginVspi(uint32_t freq, uint8_t mode, uint8_t bitOrder) {
    if (_vspi) _vspi->beginTransaction(SPISettings(freq, bitOrder, mode));
}

void BusManager::endVspi() {
    if (_vspi) _vspi->endTransaction();
}

void BusManager::beginHspi(uint32_t freq, uint8_t mode, uint8_t bitOrder) {
    if (_hspi) _hspi->beginTransaction(SPISettings(freq, bitOrder, mode));
}

void BusManager::endHspi() {
    if (_hspi) _hspi->endTransaction();
}

TwoWire& BusManager::i2c(int index) {
    if (index == 1 && _i2c1) return *_i2c1;
    if (_i2c0) return *_i2c0;
    return Wire; // Fallback to global Wire instance
}

} // namespace skullgate
