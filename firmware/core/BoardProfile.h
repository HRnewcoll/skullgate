/**
 * @file BoardProfile.h
 * @brief Loads and exposes a board's hardware profile from a JSON file on SD.
 *
 * Each physical board ships (or can be given) a JSON profile stored at
 *   /boards/<BOARD_ID>.json
 * on the SD card (or compiled into flash as a fallback).
 *
 * The profile describes:
 *   - MCU type, flash size, PSRAM availability
 *   - Pin numbers for display, touch, SD, UART, I2C, GPIO
 *   - Driver names so the DriverRegistry can instantiate the right adapter
 *   - A capability list so modules can query what this board supports
 *
 * SkullGate Safety Model
 * ──────────────────────
 * BoardProfile itself is read-only. No module may alter pin configuration at
 * runtime.  All active features (non-passive scanning, transmit, etc.) are
 * gated by the ModuleManager permission system — not by this file.
 */

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <string>

namespace skullgate {

// ── Pin bundle structs ───────────────────────────────────────────────────────

struct DisplayPins {
    uint8_t sclk, mosi, miso, cs, dc, rst, bl;
    bool    bl_active_high;
    uint32_t freq_write;
    String   driver;       ///< e.g. "ST7789", "ILI9341"
    String   bus;          ///< "SPI" | "I2C" | "Parallel8"
    String   spi_host;     ///< "VSPI" | "HSPI"
    uint16_t width, height;
    uint8_t  rotation;
    String   color_order;  ///< "RGB" | "BGR"
};

struct TouchPins {
    uint8_t sclk, mosi, miso, cs, irq;
    String  driver;        ///< "XPT2046" | "GT911" | "None"
    String  bus;
    String  spi_host;
    int16_t x_min, x_max, y_min, y_max;
    bool    swap_xy, invert_x, invert_y;
};

struct SdPins {
    uint8_t sclk, mosi, miso, cs;
    String  bus;
    String  spi_host;
};

struct UartConfig {
    uint8_t id, pin_tx, pin_rx;
};

struct I2cConfig {
    uint8_t  id, pin_sda, pin_scl;
    uint32_t freq;
};

struct GpioConfig {
    int8_t led_builtin;
    int8_t btn_boot;
    int8_t neopixel;
    uint8_t neopixel_count;
};

// ── Main class ───────────────────────────────────────────────────────────────

/**
 * @class BoardProfile
 * @brief Parses and holds a board's complete hardware description.
 *
 * Usage:
 * @code
 *   BoardProfile profile;
 *   if (profile.loadFromSD("/boards/2432S022.json")) {
 *       if (profile.has("display")) { ... }
 *       auto& dp = profile.display();
 *   }
 * @endcode
 */
class BoardProfile {
public:
    BoardProfile() : _loaded(false) {}

    /**
     * @brief Load profile JSON from the SD card.
     * @param path  Absolute path on SD, e.g. "/boards/2432S022.json"
     * @return true on success
     */
    bool loadFromSD(const char* path);

    /**
     * @brief Load profile from a JSON string (e.g. compiled-in fallback).
     * @param json  Raw JSON string
     * @return true on success
     */
    bool loadFromString(const char* json);

    /// Returns true if the profile was successfully parsed.
    bool isLoaded() const { return _loaded; }

    /// Returns true if this board declares the given capability string.
    /// Known values: "display", "touch", "sd", "wifi", "ble", "uart", "i2c"
    bool has(const String& capability) const;

    /// Board identification
    const String& boardId()  const { return _boardId; }
    const String& mcu()      const { return _mcu; }
    int           flashMb()  const { return _flashMb; }
    bool          hasPsram() const { return _psram; }

    /// Hardware pin/config accessors
    const DisplayPins& display()                        const { return _display; }
    const TouchPins&   touch()                          const { return _touch; }
    const SdPins&      sd()                             const { return _sd; }
    const GpioConfig&  gpio()                           const { return _gpio; }
    const std::vector<UartConfig>& uarts()              const { return _uarts; }
    const std::vector<I2cConfig>&  i2cs()               const { return _i2cs; }
    const std::vector<String>&     capabilities()       const { return _capabilities; }

private:
    bool _parse(JsonDocument& doc);

    bool   _loaded;
    String _boardId;
    String _mcu;
    int    _flashMb;
    bool   _psram;

    DisplayPins _display;
    TouchPins   _touch;
    SdPins      _sd;
    GpioConfig  _gpio;

    std::vector<UartConfig> _uarts;
    std::vector<I2cConfig>  _i2cs;
    std::vector<String>     _capabilities;
};

} // namespace skullgate
