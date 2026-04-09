/**
 * @file LoraScannerModule.h
 * @brief Passive LoRa / sub-GHz packet scanner module.
 *
 * Listens for LoRa packets on a configurable frequency and spreading factor
 * using a RadioLib-compatible SX127x or SX126x transceiver.
 *
 * ── Hardware requirement ─────────────────────────────────────────────────────
 * A SX1276/SX1278 (or compatible) LoRa module must be wired to the ESP32 SPI
 * bus.  The board profile must include "lora" in its capabilities array.
 * If the module is absent the module displays a "hardware not detected"
 * message and disables itself cleanly.
 *
 * ── Safety ───────────────────────────────────────────────────────────────────
 * RX-only operation — the radio is never configured to transmit.
 * All captured packets are logged to /lora/ on the SD card as JSON.
 *
 * Permissions: radio_rx, sd_write, ui
 * Lab Mode: NOT required (passive RX only)
 */

#pragma once

#include "../../core/ModuleManager.h"
#include <vector>

namespace skullgate {

struct LoraPacket {
    int8_t   rssi;       ///< dBm
    float    snr;        ///< dB
    uint32_t ts;         ///< millis() timestamp
    uint8_t  data[255];
    uint8_t  len;
};

class LoraScannerModule : public IModule {
public:
    LoraScannerModule();
    ~LoraScannerModule() override = default;

    bool init(CoreAPI& api)       override;
    void start()                  override;
    void loop()                   override;
    void stop()                   override;
    const ModuleManifest& manifest() const override { return _manifest; }

private:
    void _buildScreen();
    void _buildNoHwScreen();
    bool _initRadio();
    void _pollRadio();
    void _updateList();
    void _logToSD(const LoraPacket& pkt);

    CoreAPI*       _api;
    ModuleManifest _manifest;
    bool           _running;
    bool           _hwPresent;

    std::vector<LoraPacket> _packets; ///< Last N received packets

    static constexpr uint8_t MAX_PACKETS = 16;

    // LVGL widgets
    lv_obj_t* _screen;
    lv_obj_t* _list;
    lv_obj_t* _lblStatus;
    lv_obj_t* _lblFreq;
};

} // namespace skullgate

// ── Self-registration ─────────────────────────────────────────────────────────
namespace {
    REGISTER_MODULE("lora_scanner", []() -> skullgate::IModule* {
        return new skullgate::LoraScannerModule();
    });
}
