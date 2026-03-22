/**
 * @file BoardProfile.cpp
 * @brief Implementation of BoardProfile JSON loader.
 */

#include "BoardProfile.h"
#include <SD.h>

namespace skullgate {

// ── Helpers ──────────────────────────────────────────────────────────────────

static uint8_t _pin(JsonObject obj, const char* key, uint8_t def = 0xFF) {
    if (obj[key].isNull()) return def;
    int v = obj[key].as<int>();
    return (v < 0) ? 0xFF : static_cast<uint8_t>(v);
}

static int8_t _spin(JsonObject obj, const char* key, int8_t def = -1) {
    if (obj[key].isNull()) return def;
    return static_cast<int8_t>(obj[key].as<int>());
}

// ── Public API ───────────────────────────────────────────────────────────────

bool BoardProfile::loadFromSD(const char* path) {
    File f = SD.open(path, FILE_READ);
    if (!f) {
        Serial.printf("[BoardProfile] Cannot open %s\n", path);
        return false;
    }

    // Read entire file into a string (profiles are small, ~2 KB)
    String buf;
    buf.reserve(f.size());
    while (f.available()) buf += (char)f.read();
    f.close();

    return loadFromString(buf.c_str());
}

bool BoardProfile::loadFromString(const char* json) {
    // Use a 4 KB stack document — profiles are small
    StaticJsonDocument<4096> doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        Serial.printf("[BoardProfile] JSON parse error: %s\n", err.c_str());
        return false;
    }
    return _parse(doc);
}

bool BoardProfile::has(const String& capability) const {
    for (const auto& c : _capabilities) {
        if (c == capability) return true;
    }
    return false;
}

// ── Private parser ───────────────────────────────────────────────────────────

bool BoardProfile::_parse(JsonDocument& doc) {
    _boardId = doc["board_id"] | "unknown";
    _mcu     = doc["mcu"]      | "esp32";
    _flashMb = doc["flash_mb"] | 4;
    _psram   = doc["psram"]    | false;

    // ── Display ──────────────────────────────────────────────────────────────
    if (doc.containsKey("display")) {
        JsonObject d = doc["display"].as<JsonObject>();
        _display.driver       = d["driver"]      | "ST7789";
        _display.bus          = d["bus"]          | "SPI";
        _display.spi_host     = d["spi_host"]     | "VSPI";
        _display.width        = d["width"]        | 240;
        _display.height       = d["height"]       | 320;
        _display.rotation     = d["rotation"]     | 0;
        _display.color_order  = d["color_order"]  | "RGB";
        _display.freq_write   = d["freq_write"]   | 40000000u;
        _display.bl_active_high = d["bl_active_high"] | true;
        _display.sclk = _pin(d, "pin_sclk");
        _display.mosi = _pin(d, "pin_mosi");
        _display.miso = _pin(d, "pin_miso");
        _display.cs   = _pin(d, "pin_cs");
        _display.dc   = _pin(d, "pin_dc");
        _display.rst  = _pin(d, "pin_rst", 0xFF);
        _display.bl   = _pin(d, "pin_bl",  0xFF);
    }

    // ── Touch ─────────────────────────────────────────────────────────────────
    if (doc.containsKey("touch")) {
        JsonObject t = doc["touch"].as<JsonObject>();
        _touch.driver    = t["driver"]    | "None";
        _touch.bus       = t["bus"]       | "SPI";
        _touch.spi_host  = t["spi_host"]  | "VSPI";
        _touch.x_min     = t["x_min"]     | 300;
        _touch.x_max     = t["x_max"]     | 3900;
        _touch.y_min     = t["y_min"]     | 300;
        _touch.y_max     = t["y_max"]     | 3900;
        _touch.swap_xy   = t["swap_xy"]   | false;
        _touch.invert_x  = t["invert_x"]  | false;
        _touch.invert_y  = t["invert_y"]  | false;
        _touch.sclk = _pin(t, "pin_sclk");
        _touch.mosi = _pin(t, "pin_mosi");
        _touch.miso = _pin(t, "pin_miso");
        _touch.cs   = _pin(t, "pin_cs");
        _touch.irq  = _pin(t, "pin_irq", 0xFF);
    }

    // ── SD ────────────────────────────────────────────────────────────────────
    if (doc.containsKey("sd")) {
        JsonObject s = doc["sd"].as<JsonObject>();
        _sd.bus      = s["bus"]      | "SPI";
        _sd.spi_host = s["spi_host"] | "HSPI";
        _sd.sclk = _pin(s, "pin_sclk");
        _sd.mosi = _pin(s, "pin_mosi");
        _sd.miso = _pin(s, "pin_miso");
        _sd.cs   = _pin(s, "pin_cs");
    }

    // ── GPIO ─────────────────────────────────────────────────────────────────
    if (doc.containsKey("gpio")) {
        JsonObject g = doc["gpio"].as<JsonObject>();
        _gpio.led_builtin   = _spin(g, "led_builtin");
        _gpio.btn_boot      = _spin(g, "btn_boot");
        _gpio.neopixel      = _spin(g, "neopixel");
        _gpio.neopixel_count = g["neopixel_count"] | 0;
    }

    // ── UART ─────────────────────────────────────────────────────────────────
    if (doc.containsKey("uart")) {
        for (JsonObject u : doc["uart"].as<JsonArray>()) {
            UartConfig cfg;
            cfg.id     = u["id"]     | 0;
            cfg.pin_tx = _pin(u, "pin_tx");
            cfg.pin_rx = _pin(u, "pin_rx");
            _uarts.push_back(cfg);
        }
    }

    // ── I2C ──────────────────────────────────────────────────────────────────
    if (doc.containsKey("i2c")) {
        for (JsonObject i : doc["i2c"].as<JsonArray>()) {
            I2cConfig cfg;
            cfg.id      = i["id"]   | 0;
            cfg.pin_sda = _pin(i, "pin_sda");
            cfg.pin_scl = _pin(i, "pin_scl");
            cfg.freq    = i["freq"] | 400000u;
            _i2cs.push_back(cfg);
        }
    }

    // ── Capabilities ─────────────────────────────────────────────────────────
    if (doc.containsKey("capabilities")) {
        for (const char* cap : doc["capabilities"].as<JsonArray>()) {
            _capabilities.push_back(String(cap));
        }
    }

    _loaded = true;
    Serial.printf("[BoardProfile] Loaded board: %s (%s)\n",
                  _boardId.c_str(), _mcu.c_str());
    return true;
}

} // namespace skullgate
