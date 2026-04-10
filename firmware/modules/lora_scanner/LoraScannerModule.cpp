/**
 * @file LoraScannerModule.cpp
 * @brief Passive LoRa packet scanner using RadioLib (stub when hardware absent).
 *
 * When compiled with BOARD_HAS_LORA defined (via board profile / build flags),
 * the module initialises the radio in continuous RX mode.  Without the flag
 * the module displays a "hardware not detected" status and returns cleanly.
 *
 * RadioLib integration: https://github.com/jgromes/RadioLib
 * Default pins assume a typical ESP32 + SX1276 wiring (can be overridden via
 * the board profile).  Adjust the pin constants to match your hardware.
 */

#include "LoraScannerModule.h"
#include <ArduinoJson.h>

// ── RadioLib guard ────────────────────────────────────────────────────────────
// Only compile radio code if the board declares LoRa hardware.
#ifdef BOARD_HAS_LORA
#  include <RadioLib.h>
// Default SX1276 wiring for a typical ESP32 + RA-02 module:
static constexpr int LORA_NSS  = 5;
static constexpr int LORA_DIO0 = 26;
static constexpr int LORA_RST  = 14;
static constexpr int LORA_DIO1 = 35;
static SX1276 radio = new Module(LORA_NSS, LORA_DIO0, LORA_RST, LORA_DIO1);
static volatile bool s_rxFlag = false;

// Called from ISR when a packet is received.
static void IRAM_ATTR _rxISR() { s_rxFlag = true; }
#endif

namespace skullgate {

// ── Manifest ─────────────────────────────────────────────────────────────────

static const ModuleManifest LORA_MANIFEST = {
    "lora_scanner",
    "LoRa Scanner",
    "1.0.0",
    "SkullGate",
    "Passive LoRa / sub-GHz packet scanner. Requires BOARD_HAS_LORA hardware.",
    {"radio_rx", "sd_write", "ui"},
    false, // Lab Mode not required
    true   // valid
};

// ── Constructor ───────────────────────────────────────────────────────────────

LoraScannerModule::LoraScannerModule()
    : _api(nullptr)
    , _manifest(LORA_MANIFEST)
    , _running(false)
    , _hwPresent(false)
    , _screen(nullptr)
    , _list(nullptr)
    , _lblStatus(nullptr)
    , _lblFreq(nullptr)
{}

// ── IModule lifecycle ─────────────────────────────────────────────────────────

bool LoraScannerModule::init(CoreAPI& api) {
    _api = &api;

    _hwPresent = _initRadio();

    if (_hwPresent) {
        _buildScreen();
    } else {
        _buildNoHwScreen();
    }

    if (_api->hasPermission("ui") && _screen) {
        _api->registerScreen("lora_scanner", _screen);
    }

    _api->log("lora_scanner",
              _hwPresent ? "Module initialised (hardware detected)"
                         : "Module initialised (no hardware — passive display only)");
    return true;
}

void LoraScannerModule::start() {
    _running = true;
    if (_screen) _api->showScreen("lora_scanner");
    _api->log("lora_scanner", _hwPresent ? "Listening..." : "No LoRa hardware detected");
}

void LoraScannerModule::loop() {
    if (!_running || !_hwPresent) return;
    _pollRadio();
}

void LoraScannerModule::stop() {
    _running = false;
#ifdef BOARD_HAS_LORA
    radio.standby();
#endif
    _api->log("lora_scanner", "Stopped");
}

// ── Private helpers ───────────────────────────────────────────────────────────

bool LoraScannerModule::_initRadio() {
#ifdef BOARD_HAS_LORA
    // 434 MHz, BW 125 kHz, SF7, CR 4/5, sync 0x12, power 10 dBm
    int state = radio.begin(434.0, 125.0, 7, 5, 0x12, 10, 8, 0);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[lora_scanner] Radio init failed: %d\n", state);
        return false;
    }
    // RX-only: set receive ISR and start continuous receive
    radio.setDio0Action(_rxISR, RISING);
    radio.startReceive();
    return true;
#else
    return false;
#endif
}

void LoraScannerModule::_pollRadio() {
#ifdef BOARD_HAS_LORA
    if (!s_rxFlag) return;
    s_rxFlag = false;

    LoraPacket pkt;
    uint8_t buf[255];
    int state = radio.readData(buf, 0);

    if (state == RADIOLIB_ERR_NONE) {
        pkt.rssi = static_cast<int8_t>(radio.getRSSI());
        pkt.snr  = radio.getSNR();
        pkt.ts   = millis();
        pkt.len  = static_cast<uint8_t>(radio.getPacketLength());
        uint8_t copyLen = pkt.len < sizeof(pkt.data) ? pkt.len : sizeof(pkt.data);
        memcpy(pkt.data, buf, copyLen);

        // Keep ring buffer limited
        if (_packets.size() >= MAX_PACKETS) {
            _packets.erase(_packets.begin());
        }
        _packets.push_back(pkt);

        _logToSD(pkt);
        _updateList();

        char status[32];
        snprintf(status, sizeof(status), "%d pkts | %d dBm",
                 (int)_packets.size(), (int)pkt.rssi);
        if (_lblStatus) lv_label_set_text(_lblStatus, status);
    }

    // Restart receive for next packet
    radio.startReceive();
#endif
}

void LoraScannerModule::_buildScreen() {
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_black(), 0);

    lv_obj_t* title = lv_label_create(_screen);
    lv_label_set_text(title, "LoRa Scanner");
    lv_obj_set_style_text_color(title, lv_color_make(0x00, 0xFF, 0x88), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    _lblFreq = lv_label_create(_screen);
    lv_label_set_text(_lblFreq, "434.0 MHz  SF7  BW125");
    lv_obj_set_style_text_color(_lblFreq, lv_color_make(0x88, 0x88, 0x88), 0);
    lv_obj_align(_lblFreq, LV_ALIGN_TOP_MID, 0, 26);

    _lblStatus = lv_label_create(_screen);
    lv_label_set_text(_lblStatus, "Listening...");
    lv_obj_set_style_text_color(_lblStatus, lv_color_make(0xAA, 0xAA, 0xAA), 0);
    lv_obj_align(_lblStatus, LV_ALIGN_TOP_RIGHT, -5, 6);

    _list = lv_list_create(_screen);
    lv_obj_set_size(_list, 230, 210);
    lv_obj_align(_list, LV_ALIGN_CENTER, 0, 8);
    lv_obj_set_style_bg_color(_list, lv_color_make(0x0A, 0x0A, 0x18), 0);

    lv_obj_t* btnBack = lv_btn_create(_screen);
    lv_obj_align(btnBack, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_size(btnBack, 80, 30);
    lv_obj_t* lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, "< Back");

    lv_obj_add_event_cb(btnBack, [](lv_event_t* e) {
        auto* mod = static_cast<LoraScannerModule*>(lv_event_get_user_data(e));
        if (mod && mod->_api) mod->_api->showScreen("home");
    }, LV_EVENT_CLICKED, this);
}

void LoraScannerModule::_buildNoHwScreen() {
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_black(), 0);

    lv_obj_t* title = lv_label_create(_screen);
    lv_label_set_text(title, "LoRa Scanner");
    lv_obj_set_style_text_color(title, lv_color_make(0x00, 0xFF, 0x88), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t* lbl = lv_label_create(_screen);
    lv_label_set_text(lbl, "No LoRa hardware detected.\n\nConnect a SX1276 / SX1278\nmodule and define\nBOARD_HAS_LORA in the\nbuild flags.");
    lv_obj_set_style_text_color(lbl, lv_color_make(0xFF, 0x88, 0x00), 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, 200);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* btnBack = lv_btn_create(_screen);
    lv_obj_align(btnBack, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_size(btnBack, 80, 30);
    lv_obj_t* lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, "< Back");

    lv_obj_add_event_cb(btnBack, [](lv_event_t* e) {
        auto* mod = static_cast<LoraScannerModule*>(lv_event_get_user_data(e));
        if (mod && mod->_api) mod->_api->showScreen("home");
    }, LV_EVENT_CLICKED, this);
}

void LoraScannerModule::_updateList() {
    if (!_list) return;
    lv_obj_clean(_list);

    // Show newest packet at the top
    for (int i = static_cast<int>(_packets.size()) - 1; i >= 0; i--) {
        const auto& p = _packets[i];
        char line[48];
        snprintf(line, sizeof(line), "%5ldms %4ddBm SNR%+.1f len%d",
                 (long)p.ts, (int)p.rssi, p.snr, (int)p.len);
        lv_list_add_btn(_list, NULL, line);
    }
}

void LoraScannerModule::_logToSD(const LoraPacket& pkt) {
    if (!_api->hasPermission("sd_write")) return;

    DynamicJsonDocument doc(512);
    doc["ts"]      = pkt.ts;
    doc["rssi"]    = pkt.rssi;
    doc["snr"]     = pkt.snr;
    doc["len"]     = pkt.len;

    // Encode payload as hex string
    String hex;
    hex.reserve(pkt.len * 2);
    for (int i = 0; i < pkt.len; i++) {
        char h[3];
        snprintf(h, sizeof(h), "%02X", pkt.data[i]);
        hex += h;
    }
    doc["data"] = hex;

    String json;
    serializeJson(doc, json);

    String path = "/lora/pkt_" + String(pkt.ts) + ".json";
    _api->writeFile(path, json);
}

} // namespace skullgate
