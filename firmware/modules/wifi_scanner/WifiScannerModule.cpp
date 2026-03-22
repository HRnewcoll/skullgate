/**
 * @file WifiScannerModule.cpp
 * @brief Passive Wi-Fi scanner with LVGL UI and SD JSON logging.
 */

#include "WifiScannerModule.h"
#include <ArduinoJson.h>

namespace skullgate {

// ── Manifest ─────────────────────────────────────────────────────────────────

static const ModuleManifest WIFI_SCANNER_MANIFEST = {
    "wifi_scanner",
    "Wi-Fi Scanner",
    "1.0.0",
    "SkullGate",
    "Passive Wi-Fi AP scanner with SD JSON logging",
    {"wifi_scan", "sd_write", "ui"},
    false, // lab_mode not required
    true   // valid
};

// ── Constructor ───────────────────────────────────────────────────────────────

WifiScannerModule::WifiScannerModule()
    : _api(nullptr)
    , _manifest(WIFI_SCANNER_MANIFEST)
    , _running(false)
    , _lastScan(0)
    , _screen(nullptr)
    , _list(nullptr)
    , _lblStatus(nullptr)
    , _spinner(nullptr)
{}

// ── IModule lifecycle ─────────────────────────────────────────────────────────

bool WifiScannerModule::init(CoreAPI& api) {
    _api = &api;

    // Build the LVGL screen
    _buildScreen();

    // Register it with the UI manager
    if (_api->hasPermission("ui") && _screen) {
        _api->registerScreen("wifi_scanner", _screen);
    }

    _api->log("wifi_scanner", "Module initialised");
    return true;
}

void WifiScannerModule::start() {
    _running = true;
    _lastScan = 0; // Force an immediate scan on next loop()

    if (_screen) _api->showScreen("wifi_scanner");
    if (_lblStatus) lv_label_set_text(_lblStatus, "Scanning...");
    if (_spinner)   lv_obj_clear_flag(_spinner, LV_OBJ_FLAG_HIDDEN);

    _api->log("wifi_scanner", "Started");
}

void WifiScannerModule::loop() {
    if (!_running) return;

    uint32_t now = millis();
    if (now - _lastScan >= SCAN_INTERVAL_MS) {
        _lastScan = now;
        _runScan();
    }
}

void WifiScannerModule::stop() {
    _running = false;
    _api->log("wifi_scanner", "Stopped");
}

// ── Private helpers ───────────────────────────────────────────────────────────

void WifiScannerModule::_buildScreen() {
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_black(), 0);

    // Title bar
    lv_obj_t* title = lv_label_create(_screen);
    lv_label_set_text(title, "Wi-Fi Scanner");
    lv_obj_set_style_text_color(title, lv_color_make(0xFF, 0x22, 0x22), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    // Status label
    _lblStatus = lv_label_create(_screen);
    lv_label_set_text(_lblStatus, "Idle");
    lv_obj_set_style_text_color(_lblStatus, lv_color_make(0xAA, 0xAA, 0xAA), 0);
    lv_obj_align(_lblStatus, LV_ALIGN_TOP_RIGHT, -5, 6);

    // Scanning spinner (hidden by default)
    _spinner = lv_spinner_create(_screen, 1000, 60);
    lv_obj_set_size(_spinner, 20, 20);
    lv_obj_align(_spinner, LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_obj_add_flag(_spinner, LV_OBJ_FLAG_HIDDEN);

    // AP list (scrollable)
    _list = lv_list_create(_screen);
    lv_obj_set_size(_list, 230, 220);
    lv_obj_align(_list, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(_list, lv_color_make(0x11, 0x11, 0x11), 0);

    // Back button
    lv_obj_t* btnBack = lv_btn_create(_screen);
    lv_obj_align(btnBack, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_size(btnBack, 80, 30);
    lv_obj_t* lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, "< Back");

    // Navigation callback — back to home
    lv_obj_add_event_cb(btnBack, [](lv_event_t* e) {
        auto* mod = static_cast<WifiScannerModule*>(lv_event_get_user_data(e));
        if (mod && mod->_api) mod->_api->showScreen("home");
    }, LV_EVENT_CLICKED, this);
}

void WifiScannerModule::_runScan() {
    _api->log("wifi_scanner", "Scanning...");

    if (_spinner) lv_obj_clear_flag(_spinner, LV_OBJ_FLAG_HIDDEN);
    if (_lblStatus) lv_label_set_text(_lblStatus, "Scanning...");

    auto aps = _api->wifiScan();

    if (_spinner) lv_obj_add_flag(_spinner, LV_OBJ_FLAG_HIDDEN);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d APs", (int)aps.size());
    if (_lblStatus) lv_label_set_text(_lblStatus, buf);

    _updateList(aps);
    _logToSD(aps);

    _api->log("wifi_scanner", String("Scan complete: ") + aps.size() + " APs");
}

void WifiScannerModule::_updateList(const std::vector<ApInfo>& aps) {
    if (!_list) return;

    lv_obj_clean(_list);

    if (aps.empty()) {
        lv_obj_t* lbl = lv_label_create(_list);
        lv_label_set_text(lbl, "No APs found");
        return;
    }

    for (const auto& ap : aps) {
        // Format: "SSID  [Ch:N]  -XXdBm"
        String ssid = ap.ssid.isEmpty() ? "(hidden)" : ap.ssid;
        char entry[64];
        snprintf(entry, sizeof(entry), "%-18s Ch:%d %ddBm",
                 ssid.substring(0, 18).c_str(),
                 ap.channel,
                 ap.rssi);

        lv_obj_t* btn = lv_list_add_btn(_list, NULL, entry);

        // Colour by RSSI strength
        lv_color_t col;
        if      (ap.rssi >= -60) col = lv_color_make(0x00, 0xFF, 0x00); // Green
        else if (ap.rssi >= -75) col = lv_color_make(0xFF, 0xAA, 0x00); // Orange
        else                     col = lv_color_make(0xFF, 0x44, 0x44); // Red

        lv_obj_t* lblEntry = lv_obj_get_child(btn, 0);
        if (lblEntry) lv_obj_set_style_text_color(lblEntry, col, 0);
    }
}

void WifiScannerModule::_logToSD(const std::vector<ApInfo>& aps) {
    if (!_api->hasPermission("sd_write")) return;

    // Build JSON document
    StaticJsonDocument<4096> doc;
    doc["timestamp"] = millis();

    JsonArray arr = doc.createNestedArray("aps");
    for (const auto& ap : aps) {
        JsonObject obj = arr.createNestedObject();
        obj["ssid"]    = ap.ssid;
        obj["bssid"]   = ap.bssid;
        obj["rssi"]    = ap.rssi;
        obj["channel"] = ap.channel;
        obj["enc"]     = ap.encryption;
    }

    // Serialize to string
    String json;
    serializeJson(doc, json);

    // Write to /wifi_scans/scan_<millis>.json
    String path = "/wifi_scans/scan_" + String(millis()) + ".json";
    if (_api->writeFile(path, json)) {
        _api->log("wifi_scanner", "Saved: " + path);
    }
}

} // namespace skullgate
