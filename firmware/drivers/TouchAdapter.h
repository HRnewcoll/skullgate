/**
 * @file TouchAdapter.h
 * @brief Touch controller abstraction (XPT2046 resistive, GT911 capacitive).
 *
 * Touch access is always routed through this adapter — no module touches
 * hardware registers directly.
 */

#pragma once

#include <Arduino.h>
#include "../core/BoardProfile.h"

namespace skullgate {

class TouchAdapter {
public:
    /**
     * @param pins       Touch hardware configuration from the board profile.
     * @param dispWidth  Display width in pixels (used for coordinate mapping).
     * @param dispHeight Display height in pixels (used for coordinate mapping).
     */
    TouchAdapter(const TouchPins& pins,
                 uint16_t dispWidth  = 240,
                 uint16_t dispHeight = 320);

    /// Initialise the touch controller. Returns false on failure.
    bool init();

    /// Returns true if the touch controller was successfully initialised.
    bool isReady() const { return _ready; }

    /**
     * @brief Read current touch state.
     * @param x   Output: x coordinate (screen pixels)
     * @param y   Output: y coordinate (screen pixels)
     * @return true if screen is currently being touched
     */
    bool read(int16_t& x, int16_t& y);

private:
    /// Map raw ADC coordinates to screen pixels using the display dimensions.
    void _calibrate(int16_t rawX, int16_t rawY,
                    int16_t& outX, int16_t& outY) const;

    TouchPins _pins;
    uint16_t  _dispWidth;
    uint16_t  _dispHeight;
    bool      _ready;
};

} // namespace skullgate
