/**
 * @file TouchAdapter.cpp
 * @brief XPT2046 resistive and GT911 capacitive touch implementation.
 *
 * Reads raw ADC values from the XPT2046 over SPI, then maps them to screen
 * coordinates using the calibration values from the board profile.
 *
 * For GT911 (capacitive), we use I2C register reads (stub — extend as needed).
 */

#include "TouchAdapter.h"
#include <SPI.h>

namespace skullgate {

// XPT2046 command bytes
static constexpr uint8_t XPT_CMD_X = 0xD0; // Measure X
static constexpr uint8_t XPT_CMD_Y = 0x90; // Measure Y
// Minimum valid ADC reading — values below this are noise/not-touched
static constexpr int16_t XPT2046_MIN_VALID_ADC = 100;

TouchAdapter::TouchAdapter(const TouchPins& pins,
                           uint16_t dispWidth,
                           uint16_t dispHeight)
    : _pins(pins), _dispWidth(dispWidth), _dispHeight(dispHeight), _ready(false)
{}

bool TouchAdapter::init() {
    if (_pins.driver == "None") {
        _ready = false;
        return true; // Not an error — just no touch hardware
    }

    if (_pins.driver == "XPT2046") {
        // CS pin setup — SPI is managed by BusManager
        if (_pins.cs != 0xFF) {
            pinMode(_pins.cs, OUTPUT);
            digitalWrite(_pins.cs, HIGH);
        }
        if (_pins.irq != 0xFF) {
            pinMode(_pins.irq, INPUT);
        }
        _ready = true;
        Serial.println("[TouchAdapter] XPT2046 ready");
        return true;
    }

    if (_pins.driver == "GT911") {
        // GT911 is capacitive, uses I2C — stub for now
        _ready = true;
        Serial.println("[TouchAdapter] GT911 stub ready");
        return true;
    }

    Serial.printf("[TouchAdapter] Unknown driver: %s\n", _pins.driver.c_str());
    return false;
}

bool TouchAdapter::read(int16_t& x, int16_t& y) {
    if (!_ready) return false;

    if (_pins.driver == "XPT2046") {
        // Check IRQ line — low means touched
        if (_pins.irq != 0xFF && digitalRead(_pins.irq) == HIGH) {
            return false; // Not pressed
        }

        // Read X and Y ADC values over SPI
        SPISettings settings(2000000, MSBFIRST, SPI_MODE0);
        SPI.beginTransaction(settings);
        digitalWrite(_pins.cs, LOW);

        SPI.transfer(XPT_CMD_X);
        int16_t rawX = (SPI.transfer(0x00) << 5) | (SPI.transfer(0x00) >> 3);

        SPI.transfer(XPT_CMD_Y);
        int16_t rawY = (SPI.transfer(0x00) << 5) | (SPI.transfer(0x00) >> 3);

        digitalWrite(_pins.cs, HIGH);
        SPI.endTransaction();

        // Sanity-check raw values — valid XPT2046 range is ~0–4095
        if (rawX < XPT2046_MIN_VALID_ADC || rawY < XPT2046_MIN_VALID_ADC) return false;

        _calibrate(rawX, rawY, x, y);
        return true;
    }

    // GT911 stub — always returns not pressed
    return false;
}

void TouchAdapter::_calibrate(int16_t rawX, int16_t rawY,
                               int16_t& outX, int16_t& outY) const {
    // Map raw [x_min..x_max] → [0..display_width]
    // and  raw [y_min..y_max] → [0..display_height]
    // Swap/invert axes per board profile.

    float nx = (float)(rawX - _pins.x_min) / (float)(_pins.x_max - _pins.x_min);
    float ny = (float)(rawY - _pins.y_min) / (float)(_pins.y_max - _pins.y_min);

    // Clamp to [0, 1]
    nx = nx < 0 ? 0 : (nx > 1 ? 1 : nx);
    ny = ny < 0 ? 0 : (ny > 1 ? 1 : ny);

    if (_pins.invert_x) nx = 1.0f - nx;
    if (_pins.invert_y) ny = 1.0f - ny;

    if (_pins.swap_xy) {
        outX = (int16_t)(ny * _dispWidth);
        outY = (int16_t)(nx * _dispHeight);
    } else {
        outX = (int16_t)(nx * _dispWidth);
        outY = (int16_t)(ny * _dispHeight);
    }
}

} // namespace skullgate
