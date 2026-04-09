/**
 * @file NfcReaderModule.h
 * @brief NFC / RFID tag reader module via PN532.
 *
 * Reads UID and NDEF data from NFC tags (ISO 14443A/B, ISO 15693) using a
 * PN532 module connected over I2C or SPI.
 *
 * ── Hardware requirement ─────────────────────────────────────────────────────
 * A PN532 breakout board must be wired to the ESP32.
 * The board profile must include "nfc" in its capabilities array.
 * If the module is absent the module displays a "hardware not detected"
 * message and disables itself cleanly.
 *
 * ── Safety ───────────────────────────────────────────────────────────────────
 * Read-only operation — the module only reads tags, it never writes.
 * Only use on tags you own.
 * All tag data is logged to /nfc/ on the SD card as JSON.
 *
 * Permissions: nfc_read, sd_write, ui
 * Lab Mode: NOT required (read-only)
 */

#pragma once

#include "../../core/ModuleManager.h"
#include <vector>

namespace skullgate {

struct NfcTag {
    String   uid;        ///< UID as hex string "AA:BB:CC:DD"
    uint8_t  uidLen;
    String   type;       ///< "ISO14443A", "ISO14443B", "ISO15693"
    String   ndefText;   ///< First NDEF Text record (if present), else ""
    uint32_t ts;         ///< millis() timestamp
};

class NfcReaderModule : public IModule {
public:
    NfcReaderModule();
    ~NfcReaderModule() override = default;

    bool init(CoreAPI& api)       override;
    void start()                  override;
    void loop()                   override;
    void stop()                   override;
    const ModuleManifest& manifest() const override { return _manifest; }

private:
    void _buildScreen();
    void _buildNoHwScreen();
    bool _initReader();
    void _pollReader();
    void _displayTag(const NfcTag& tag);
    void _logToSD(const NfcTag& tag);

    CoreAPI*       _api;
    ModuleManifest _manifest;
    bool           _running;
    bool           _hwPresent;

    std::vector<NfcTag> _tags; ///< All scanned tags this session

    uint32_t _lastPoll;
    static constexpr uint32_t POLL_INTERVAL_MS = 500;

    // LVGL widgets
    lv_obj_t* _screen;
    lv_obj_t* _lblStatus;
    lv_obj_t* _lblUid;
    lv_obj_t* _lblType;
    lv_obj_t* _lblNdef;
    lv_obj_t* _lblCount;
};

} // namespace skullgate

// ── Self-registration ─────────────────────────────────────────────────────────
namespace {
    REGISTER_MODULE("nfc_reader", []() -> skullgate::IModule* {
        return new skullgate::NfcReaderModule();
    });
}
