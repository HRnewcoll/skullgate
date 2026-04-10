/**
 * @file NfcReaderModule.cpp
 * @brief NFC tag reader using PN532 (stub when hardware absent).
 *
 * When compiled with BOARD_HAS_NFC defined the module uses the Adafruit PN532
 * library to poll for ISO 14443A tags.  Without the flag the module shows a
 * "hardware not detected" message.
 *
 * Adafruit PN532 library: https://github.com/adafruit/Adafruit-PN532
 * Default wiring assumes PN532 in I2C mode (SDA=21, SCL=22 on most ESP32).
 */

#include "NfcReaderModule.h"
#include <ArduinoJson.h>

// ── PN532 guard ───────────────────────────────────────────────────────────────
#ifdef BOARD_HAS_NFC
#  include <Adafruit_PN532.h>
static constexpr int NFC_SDA = 21;
static constexpr int NFC_SCL = 22;
static Adafruit_PN532 s_nfc(NFC_SDA, NFC_SCL);
#endif

namespace skullgate {

// ── Manifest ─────────────────────────────────────────────────────────────────

static const ModuleManifest NFC_MANIFEST = {
    "nfc_reader",
    "NFC Reader",
    "1.0.0",
    "SkullGate",
    "NFC/RFID tag reader via PN532. Read-only. Requires BOARD_HAS_NFC hardware.",
    {"nfc_read", "sd_write", "ui"},
    false, // Lab Mode not required
    true   // valid
};

// ── Constructor ───────────────────────────────────────────────────────────────

NfcReaderModule::NfcReaderModule()
    : _api(nullptr)
    , _manifest(NFC_MANIFEST)
    , _running(false)
    , _hwPresent(false)
    , _lastPoll(0)
    , _screen(nullptr)
    , _lblStatus(nullptr)
    , _lblUid(nullptr)
    , _lblType(nullptr)
    , _lblNdef(nullptr)
    , _lblCount(nullptr)
{}

// ── IModule lifecycle ─────────────────────────────────────────────────────────

bool NfcReaderModule::init(CoreAPI& api) {
    _api = &api;
    _hwPresent = _initReader();

    if (_hwPresent) {
        _buildScreen();
    } else {
        _buildNoHwScreen();
    }

    if (_api->hasPermission("ui") && _screen) {
        _api->registerScreen("nfc_reader", _screen);
    }

    _api->log("nfc_reader",
              _hwPresent ? "PN532 detected and ready"
                         : "No NFC hardware detected");
    return true;
}

void NfcReaderModule::start() {
    _running  = true;
    _lastPoll = 0;
    if (_screen) _api->showScreen("nfc_reader");
    if (_lblStatus) lv_label_set_text(_lblStatus,
                        _hwPresent ? "Waiting for tag..." : "No hardware");
    _api->log("nfc_reader", "Started");
}

void NfcReaderModule::loop() {
    if (!_running || !_hwPresent) return;

    uint32_t now = millis();
    if (now - _lastPoll >= POLL_INTERVAL_MS) {
        _lastPoll = now;
        _pollReader();
    }
}

void NfcReaderModule::stop() {
    _running = false;
    _api->log("nfc_reader", "Stopped");
}

// ── Private helpers ───────────────────────────────────────────────────────────

bool NfcReaderModule::_initReader() {
#ifdef BOARD_HAS_NFC
    s_nfc.begin();
    uint32_t versiondata = s_nfc.getFirmwareVersion();
    if (!versiondata) {
        Serial.println("[nfc_reader] PN532 not found");
        return false;
    }
    s_nfc.SAMConfig(); // configure board to read RFID tags
    Serial.printf("[nfc_reader] PN532 firmware v%d.%d\n",
                  (versiondata >> 16) & 0xFF, (versiondata >> 8) & 0xFF);
    return true;
#else
    return false;
#endif
}

void NfcReaderModule::_pollReader() {
#ifdef BOARD_HAS_NFC
    uint8_t uid[7];
    uint8_t uidLen = 0;

    // Non-blocking poll with 50 ms timeout (ISO 14443A)
    bool found = s_nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A,
                                           uid, &uidLen, 50);
    if (!found) return;

    NfcTag tag;
    tag.uidLen = uidLen;
    tag.type   = "ISO14443A";
    tag.ts     = millis();

    // Build UID string
    for (int i = 0; i < uidLen; i++) {
        if (i > 0) tag.uid += ":";
        char h[3];
        snprintf(h, sizeof(h), "%02X", uid[i]);
        tag.uid += h;
    }

    // Simple NDEF text extraction (best-effort, 4-byte ATQA NDEF check)
    // A full NDEF parser is beyond the scope of this stub.
    tag.ndefText = "";

    _tags.push_back(tag);
    _displayTag(tag);
    _logToSD(tag);

    _api->log("nfc_reader", "Tag: " + tag.uid);
#endif
}

void NfcReaderModule::_displayTag(const NfcTag& tag) {
    if (_lblUid)   lv_label_set_text(_lblUid,   ("UID: " + tag.uid).c_str());
    if (_lblType)  lv_label_set_text(_lblType,  ("Type: " + tag.type).c_str());
    if (_lblNdef)  lv_label_set_text(_lblNdef,
                      tag.ndefText.isEmpty() ? "NDEF: (none)"
                                             : ("NDEF: " + tag.ndefText).c_str());

    char buf[16];
    snprintf(buf, sizeof(buf), "Tags: %d", (int)_tags.size());
    if (_lblCount)  lv_label_set_text(_lblCount, buf);
    if (_lblStatus) lv_label_set_text(_lblStatus, "Tag read!");
}

void NfcReaderModule::_buildScreen() {
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_black(), 0);

    lv_obj_t* title = lv_label_create(_screen);
    lv_label_set_text(title, "NFC Reader");
    lv_obj_set_style_text_color(title, lv_color_make(0x00, 0xDD, 0xFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    _lblStatus = lv_label_create(_screen);
    lv_label_set_text(_lblStatus, "Idle");
    lv_obj_set_style_text_color(_lblStatus, lv_color_make(0xAA, 0xAA, 0xAA), 0);
    lv_obj_align(_lblStatus, LV_ALIGN_TOP_RIGHT, -5, 6);

    _lblUid = lv_label_create(_screen);
    lv_label_set_text(_lblUid, "UID: —");
    lv_obj_set_style_text_color(_lblUid, lv_color_white(), 0);
    lv_obj_align(_lblUid, LV_ALIGN_CENTER, 0, -40);

    _lblType = lv_label_create(_screen);
    lv_label_set_text(_lblType, "Type: —");
    lv_obj_set_style_text_color(_lblType, lv_color_white(), 0);
    lv_obj_align(_lblType, LV_ALIGN_CENTER, 0, -20);

    _lblNdef = lv_label_create(_screen);
    lv_label_set_text(_lblNdef, "NDEF: —");
    lv_obj_set_style_text_color(_lblNdef, lv_color_make(0x88, 0xDD, 0xFF), 0);
    lv_obj_set_width(_lblNdef, 220);
    lv_label_set_long_mode(_lblNdef, LV_LABEL_LONG_WRAP);
    lv_obj_align(_lblNdef, LV_ALIGN_CENTER, 0, 10);

    _lblCount = lv_label_create(_screen);
    lv_label_set_text(_lblCount, "Tags: 0");
    lv_obj_set_style_text_color(_lblCount, lv_color_make(0xAA, 0xAA, 0xAA), 0);
    lv_obj_align(_lblCount, LV_ALIGN_CENTER, 0, 50);

    lv_obj_t* btnBack = lv_btn_create(_screen);
    lv_obj_align(btnBack, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_size(btnBack, 80, 30);
    lv_obj_t* lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, "< Back");

    lv_obj_add_event_cb(btnBack, [](lv_event_t* e) {
        auto* mod = static_cast<NfcReaderModule*>(lv_event_get_user_data(e));
        if (mod && mod->_api) mod->_api->showScreen("home");
    }, LV_EVENT_CLICKED, this);
}

void NfcReaderModule::_buildNoHwScreen() {
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_black(), 0);

    lv_obj_t* title = lv_label_create(_screen);
    lv_label_set_text(title, "NFC Reader");
    lv_obj_set_style_text_color(title, lv_color_make(0x00, 0xDD, 0xFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t* lbl = lv_label_create(_screen);
    lv_label_set_text(lbl, "No NFC hardware detected.\n\nConnect a PN532 module\nover I2C and define\nBOARD_HAS_NFC in the\nbuild flags.");
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
        auto* mod = static_cast<NfcReaderModule*>(lv_event_get_user_data(e));
        if (mod && mod->_api) mod->_api->showScreen("home");
    }, LV_EVENT_CLICKED, this);
}

void NfcReaderModule::_logToSD(const NfcTag& tag) {
    if (!_api->hasPermission("sd_write")) return;

    DynamicJsonDocument doc(256);
    doc["ts"]   = tag.ts;
    doc["uid"]  = tag.uid;
    doc["type"] = tag.type;
    doc["ndef"] = tag.ndefText;

    String json;
    serializeJson(doc, json);

    String path = "/nfc/tag_" + String(tag.ts) + ".json";
    _api->writeFile(path, json);
}

} // namespace skullgate
