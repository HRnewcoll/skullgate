/**
 * @file OtaUpdateModule.cpp
 * @brief OTA firmware update — Lab Mode only, config from SD.
 *
 * SAFETY: Only operates in Lab Mode.  The firmware URL must be stored on the
 * physical SD card.  The device must explicitly connect to a Wi-Fi AP first.
 */

#include "OtaUpdateModule.h"
#include <ArduinoJson.h>

// HTTPUpdate and Update are part of the ESP32 Arduino SDK.
// We guard with an ifdef so the firmware compiles on platforms without HTTP.
#ifndef SKULLGATE_NO_HTTP
#  include <HTTPUpdate.h>
#endif

namespace skullgate {

// ── Manifest ─────────────────────────────────────────────────────────────────

static const ModuleManifest OTA_MANIFEST = {
    "ota_update",
    "OTA Update",
    "1.0.0",
    "SkullGate",
    "Download and flash firmware OTA from URL in /ota/config.json. Lab Mode required.",
    {"wifi_connect", "sd_read", "ui", "ota"},
    true,  // Lab Mode required
    true   // valid
};

// ── Constructor ───────────────────────────────────────────────────────────────

OtaUpdateModule::OtaUpdateModule()
    : _api(nullptr)
    , _manifest(OTA_MANIFEST)
    , _running(false)
    , _updating(false)
    , _screen(nullptr)
    , _lblStatus(nullptr)
    , _bar(nullptr)
    , _lblPercent(nullptr)
    , _btnStart(nullptr)
    , _btnCancel(nullptr)
{}

// ── IModule lifecycle ─────────────────────────────────────────────────────────

bool OtaUpdateModule::init(CoreAPI& api) {
    _api = &api;

    if (!_api->isLabMode()) {
        _api->log("ota_update", "Lab Mode required — module disabled");
        return false;
    }

    _buildScreen();
    if (_api->hasPermission("ui") && _screen) {
        _api->registerScreen("ota_update", _screen);
    }
    _api->log("ota_update", "Module initialised");
    return true;
}

void OtaUpdateModule::start() {
    _running = false; // wait for user to press Start
    if (_screen) _api->showScreen("ota_update");
    _setStatus("Ready. Insert SD with /ota/config.json");
    _api->log("ota_update", "Started");
}

void OtaUpdateModule::loop() {
    // Nothing periodic — OTA is triggered by the UI button.
}

void OtaUpdateModule::stop() {
    _running  = false;
    _updating = false;
    _api->log("ota_update", "Stopped");
}

// ── Private helpers ───────────────────────────────────────────────────────────

void OtaUpdateModule::_buildScreen() {
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_black(), 0);

    // Title
    lv_obj_t* title = lv_label_create(_screen);
    lv_label_set_text(title, "OTA Firmware Update");
    lv_obj_set_style_text_color(title, lv_color_make(0xFF, 0x88, 0x00), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    // Lab Mode badge
    lv_obj_t* badge = lv_label_create(_screen);
    lv_label_set_text(badge, "[LAB MODE]");
    lv_obj_set_style_text_color(badge, lv_color_make(0xFF, 0x22, 0x22), 0);
    lv_obj_align(badge, LV_ALIGN_TOP_MID, 0, 26);

    // Status label
    _lblStatus = lv_label_create(_screen);
    lv_label_set_text(_lblStatus, "Idle");
    lv_obj_set_style_text_color(_lblStatus, lv_color_make(0xAA, 0xAA, 0xAA), 0);
    lv_obj_set_width(_lblStatus, 220);
    lv_label_set_long_mode(_lblStatus, LV_LABEL_LONG_WRAP);
    lv_obj_align(_lblStatus, LV_ALIGN_CENTER, 0, -20);

    // Progress bar
    _bar = lv_bar_create(_screen);
    lv_obj_set_size(_bar, 200, 16);
    lv_obj_align(_bar, LV_ALIGN_CENTER, 0, 20);
    lv_bar_set_value(_bar, 0, LV_ANIM_OFF);

    // Percentage label
    _lblPercent = lv_label_create(_screen);
    lv_label_set_text(_lblPercent, "0%");
    lv_obj_set_style_text_color(_lblPercent, lv_color_white(), 0);
    lv_obj_align(_lblPercent, LV_ALIGN_CENTER, 0, 42);

    // Start button
    _btnStart = lv_btn_create(_screen);
    lv_obj_align(_btnStart, LV_ALIGN_BOTTOM_LEFT, 15, -8);
    lv_obj_set_size(_btnStart, 90, 32);
    lv_obj_t* lblStart = lv_label_create(_btnStart);
    lv_label_set_text(lblStart, "Start OTA");

    lv_obj_add_event_cb(_btnStart, [](lv_event_t* e) {
        auto* mod = static_cast<OtaUpdateModule*>(lv_event_get_user_data(e));
        if (mod) mod->_beginUpdate();
    }, LV_EVENT_CLICKED, this);

    // Back button
    _btnCancel = lv_btn_create(_screen);
    lv_obj_align(_btnCancel, LV_ALIGN_BOTTOM_RIGHT, -15, -8);
    lv_obj_set_size(_btnCancel, 70, 32);
    lv_obj_t* lblCancel = lv_label_create(_btnCancel);
    lv_label_set_text(lblCancel, "< Back");

    lv_obj_add_event_cb(_btnCancel, [](lv_event_t* e) {
        auto* mod = static_cast<OtaUpdateModule*>(lv_event_get_user_data(e));
        if (mod && mod->_api) mod->_api->showScreen("home");
    }, LV_EVENT_CLICKED, this);
}

void OtaUpdateModule::_beginUpdate() {
    if (_updating) return;

    // Read OTA config from SD
    String cfgJson = _api->readFile("/ota/config.json");
    if (cfgJson.isEmpty()) {
        _setStatus("ERROR: /ota/config.json not found", true);
        return;
    }

    DynamicJsonDocument doc(512);
    if (deserializeJson(doc, cfgJson) != DeserializationError::Ok) {
        _setStatus("ERROR: invalid /ota/config.json", true);
        return;
    }

    String url      = doc["url"]      | "";
    String ssid     = doc["ssid"]     | "";
    String password = doc["password"] | "";

    if (url.isEmpty()) {
        _setStatus("ERROR: 'url' missing in config", true);
        return;
    }

    _setStatus("Connecting to Wi-Fi...");
    lv_task_handler();

    if (!ssid.isEmpty()) {
        if (!_api->wifiConnect(ssid, password, 15000)) {
            _setStatus("ERROR: Wi-Fi connection failed", true);
            return;
        }
    } else if (!_api->wifiIsConnected()) {
        _setStatus("ERROR: no Wi-Fi and no SSID in config", true);
        return;
    }

    _setStatus("Downloading firmware...");
    _updating = true;
    lv_task_handler();

#ifndef SKULLGATE_NO_HTTP
    // Set progress callback so the bar updates during download.
    httpUpdate.onProgress([](int cur, int total) {
        // This is called from HTTPUpdate context; we can't safely call LVGL
        // here.  Progress is shown as serial log only.
        Serial.printf("[ota_update] Progress: %d / %d\n", cur, total);
    });

    WiFiClient client;
    t_httpUpdate_return ret = httpUpdate.update(client, url);

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            _setStatus(String("ERROR: ") + httpUpdate.getLastErrorString(), true);
            _updating = false;
            break;
        case HTTP_UPDATE_NO_UPDATES:
            _setStatus("No update available");
            _updating = false;
            break;
        case HTTP_UPDATE_OK:
            // Device will reboot automatically after a successful update.
            _setStatus("Update successful — rebooting...");
            break;
    }
#else
    _setStatus("OTA not supported on this build");
    _updating = false;
#endif
}

void OtaUpdateModule::_setStatus(const String& msg, bool isError) {
    _api->log("ota_update", msg);
    if (_lblStatus) {
        lv_label_set_text(_lblStatus, msg.c_str());
        lv_color_t col = isError
            ? lv_color_make(0xFF, 0x44, 0x44)
            : lv_color_make(0xAA, 0xAA, 0xAA);
        lv_obj_set_style_text_color(_lblStatus, col, 0);
    }
}

} // namespace skullgate
