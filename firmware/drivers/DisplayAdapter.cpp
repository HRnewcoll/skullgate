/**
 * @file DisplayAdapter.cpp
 * @brief LovyanGFX display driver, configured entirely from BoardProfile.
 *
 * LovyanGFX v1 supports "runtime" (dynamic) bus + panel configuration.
 * We use lgfx::Panel_ST7789 / ILI9341 etc. chosen from the profile driver name.
 */

#include "DisplayAdapter.h"

namespace skullgate {

DisplayAdapter::DisplayAdapter(const DisplayPins& pins)
    : _pins(pins)
{}

bool DisplayAdapter::init() {
    // ── Configure SPI bus dynamically ────────────────────────────────────────
    auto* bus = new lgfx::Bus_SPI();
    lgfx::Bus_SPI::config_t busCfg;

    // Map spi_host string to SPI host number
    if (_pins.spi_host == "VSPI") {
        busCfg.spi_host = SPI3_HOST; // VSPI on ESP32
    } else {
        busCfg.spi_host = SPI2_HOST; // HSPI on ESP32
    }

    busCfg.spi_clock      = _pins.freq_write;
    busCfg.pin_sclk       = _pins.sclk;
    busCfg.pin_mosi       = _pins.mosi;
    busCfg.pin_miso       = (_pins.miso == 0xFF) ? -1 : _pins.miso;
    busCfg.pin_dc         = _pins.dc;
    busCfg.spi_3wire      = false;
    busCfg.use_lock       = true;
    busCfg.dma_channel    = AUTO;
    bus->config(busCfg);

    // ── Configure panel ───────────────────────────────────────────────────────
    lgfx::IPanel* panel = nullptr;

    if (_pins.driver == "ST7789") {
        auto* p = new lgfx::Panel_ST7789();
        lgfx::Panel_ST7789::config_t panelCfg;
        panelCfg.pin_cs        = (_pins.cs  == 0xFF) ? -1 : _pins.cs;
        panelCfg.pin_rst       = (_pins.rst == 0xFF) ? -1 : _pins.rst;
        panelCfg.pin_busy      = -1;
        panelCfg.panel_width   = _pins.width;
        panelCfg.panel_height  = _pins.height;
        panelCfg.rotation      = _pins.rotation;
        panelCfg.rgb_order     = (_pins.color_order == "BGR");
        p->config(panelCfg);
        panel = p;
    } else if (_pins.driver == "ILI9341") {
        auto* p = new lgfx::Panel_ILI9341();
        lgfx::Panel_ILI9341::config_t panelCfg;
        panelCfg.pin_cs       = (_pins.cs  == 0xFF) ? -1 : _pins.cs;
        panelCfg.pin_rst      = (_pins.rst == 0xFF) ? -1 : _pins.rst;
        panelCfg.pin_busy     = -1;
        panelCfg.panel_width  = _pins.width;
        panelCfg.panel_height = _pins.height;
        panelCfg.rotation     = _pins.rotation;
        panelCfg.rgb_order    = (_pins.color_order == "BGR");
        p->config(panelCfg);
        panel = p;
    } else {
        Serial.printf("[DisplayAdapter] Unknown driver: %s\n",
                      _pins.driver.c_str());
        return false;
    }

    // ── Backlight ─────────────────────────────────────────────────────────────
    if (_pins.bl != 0xFF) {
        auto* bl = new lgfx::Light_PWM();
        lgfx::Light_PWM::config_t blCfg;
        blCfg.pin_bl     = _pins.bl;
        blCfg.invert     = !_pins.bl_active_high;
        blCfg.freq       = 44100;
        blCfg.pwm_channel = 7;
        bl->config(blCfg);
        panel->setLight(bl);
    }

    // ── Assemble display ──────────────────────────────────────────────────────
    panel->setBus(bus);
    _gfx.setPanel(panel);
    _gfx.init();
    _gfx.setRotation(_pins.rotation);

    Serial.printf("[DisplayAdapter] %s %dx%d ready\n",
                  _pins.driver.c_str(), _pins.width, _pins.height);
    return true;
}

void DisplayAdapter::pushPixels(int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                                 uint16_t* pixels) {
    _gfx.startWrite();
    _gfx.setAddrWindow(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
    _gfx.writePixels(pixels, (x2 - x1 + 1) * (y2 - y1 + 1));
    _gfx.endWrite();
}

void DisplayAdapter::fillScreen(uint16_t color) {
    _gfx.fillScreen(color);
}

} // namespace skullgate
