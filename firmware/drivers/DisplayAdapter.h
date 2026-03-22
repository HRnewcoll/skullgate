/**
 * @file DisplayAdapter.h
 * @brief LovyanGFX-based display adapter.
 *
 * Abstracts the display so that modules never touch LovyanGFX directly.
 * Display parameters come from the BoardProfile — no hardcoded pins.
 */

#pragma once

#include <Arduino.h>
#include "../core/BoardProfile.h"

// LovyanGFX dynamic configuration header
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

namespace skullgate {

/**
 * @class DisplayAdapter
 * @brief Wraps LovyanGFX with board-profile-driven configuration.
 */
class DisplayAdapter {
public:
    explicit DisplayAdapter(const DisplayPins& pins);

    /// Initialise the display hardware. Returns false on failure.
    bool init();

    /// Push a rectangle of 16-bit pixels to the display.
    void pushPixels(int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                    uint16_t* pixels);

    /// Fill the screen with a single colour.
    void fillScreen(uint16_t color);

    uint16_t width()  const { return _pins.width; }
    uint16_t height() const { return _pins.height; }

    /// Access the underlying LovyanGFX instance for advanced use.
    LGFX& lgfx() { return _gfx; }

private:
    DisplayPins _pins;

    // LovyanGFX runtime-configured instance
    // Uses LGFX_Device (dynamic config) rather than a compile-time class.
    LGFX _gfx;
};

} // namespace skullgate
