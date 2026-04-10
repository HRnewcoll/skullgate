/**
 * @file BleScannerModule.cpp
 * @brief Passive BLE scanner with LVGL UI and SD JSON logging.
 */

#include "BleScannerModule.h"
#include <ArduinoJson.h>

namespace skullgate {

// ── Manifest ─────────────────────────────────────────────────────────────────

static const ModuleManifest BLE_SCANNER_MANIFEST = {
    "ble_scanner",
    "BLE Scanner",
    "1.0.0",
    "SkullGate",
    "Passive BLE device scanner — no scan requests, no connections",
    {"ble_scan", "sd_write", "ui"},
    false, // Lab Mode not required
    true   // valid
};

// ── Constructor ───────────────────────────────────────────────────────────────

BleScannerModule::BleScannerModule()
    : _api(nullptr)
    , _manifest(BLE_SCANNER_MANIFEST)
    , _running(false)
    , _lastScan(0)
    , _screen(nullptr)
    , _list(nullptr)
    , _lblStatus(nullptr)
    , _spinner(nullptr)
{}

// ── IModule lifecycle ─────────────────────────────────────────────────────────

bool BleScannerModule::init(CoreAPI& api) {
    _api = &api;
    _buildScreen();
    if (_api->hasPermission("ui") && _screen) {
        _api->registerScreen("ble_scanner", _screen);
    }
    _api->log("ble_scanner", "Module initialised");
    return true;
}

void BleScannerModule::start() {
    _running  = true;
    _lastScan = 0; // Force immediate scan

    if (_screen)    _api->showScreen("ble_scanner");
    if (_lblStatus) lv_label_set_text(_lblStatus, "Starting scan...");
    if (_spinner)   lv_obj_clear_flag(_spinner, LV_OBJ_FLAG_HIDDEN);

    _api->log("ble_scanner", "Started");
}

void BleScannerModule::loop() {
    if (!_running) return;

    uint32_t now = millis();
    if (now - _lastScan >= SCAN_INTERVAL_MS) {
        _lastScan = now;
        _runScan();
    }
}

void BleScannerModule::stop() {
    _running = false;
    _api->log("ble_scanner", "Stopped");
}

// ── Private helpers ───────────────────────────────────────────────────────────

void BleScannerModule::_buildScreen() {
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_black(), 0);

    // Title
    lv_obj_t* title = lv_label_create(_screen);
    lv_label_set_text(title, "BLE Scanner");
    lv_obj_set_style_text_color(title, lv_color_make(0x00, 0xCC, 0xFF), 0);
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

    // Device list (scrollable)
    _list = lv_list_create(_screen);
    lv_obj_set_size(_list, 230, 220);
    lv_obj_align(_list, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(_list, lv_color_make(0x0A, 0x0A, 0x18), 0);

    // Back button
    lv_obj_t* btnBack = lv_btn_create(_screen);
    lv_obj_align(btnBack, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_size(btnBack, 80, 30);
    lv_obj_t* lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, "< Back");

    lv_obj_add_event_cb(btnBack, [](lv_event_t* e) {
        auto* mod = static_cast<BleScannerModule*>(lv_event_get_user_data(e));
        if (mod && mod->_api) mod->_api->showScreen("home");
    }, LV_EVENT_CLICKED, this);
}

void BleScannerModule::_runScan() {
    _api->log("ble_scanner", "Scanning...");
    if (_spinner) lv_obj_clear_flag(_spinner, LV_OBJ_FLAG_HIDDEN);
    if (_lblStatus) lv_label_set_text(_lblStatus, "Scanning...");

    // Drive LVGL once so the spinner appears before the blocking scan
    lv_task_handler();

    auto devs = _api->bleScan(SCAN_DURATION_MS);

    if (_spinner) lv_obj_add_flag(_spinner, LV_OBJ_FLAG_HIDDEN);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d devices", (int)devs.size());
    if (_lblStatus) lv_label_set_text(_lblStatus, buf);

    _updateList(devs);
    _logToSD(devs);

    _api->log("ble_scanner",
              String("Scan complete: ") + devs.size() + " devices");
}

void BleScannerModule::_updateList(const std::vector<BleDevice>& devs) {
    if (!_list) return;
    lv_obj_clean(_list);

    if (devs.empty()) {
        lv_obj_t* lbl = lv_label_create(_list);
        lv_label_set_text(lbl, "No BLE devices found");
        return;
    }

    for (const auto& dev : devs) {
        String label = dev.mac;
        if (!dev.name.isEmpty()) {
            label += " [";
            label += dev.name.substring(0, 12);
            label += "]";
        }
        char rssi_str[16];
        snprintf(rssi_str, sizeof(rssi_str), " %ddBm", dev.rssi);
        label += rssi_str;

        lv_obj_t* btn = lv_list_add_btn(_list, NULL, label.c_str());

        // Colour by RSSI
        lv_color_t col;
        if      (dev.rssi >= -60) col = lv_color_make(0x00, 0xFF, 0xCC);
        else if (dev.rssi >= -75) col = lv_color_make(0x00, 0xAA, 0xFF);
        else                      col = lv_color_make(0x44, 0x44, 0xFF);

        lv_obj_t* lbl = lv_obj_get_child(btn, 0);
        if (lbl) lv_obj_set_style_text_color(lbl, col, 0);
    }
}

void BleScannerModule::_logToSD(const std::vector<BleDevice>& devs) {
    if (!_api->hasPermission("sd_write")) return;

    const size_t capacity = JSON_OBJECT_SIZE(2)
                          + JSON_ARRAY_SIZE(devs.size())
                          + devs.size() * JSON_OBJECT_SIZE(5)
                          + 64 + devs.size() * 80;

    DynamicJsonDocument doc(capacity);
    doc["timestamp"] = millis();

    JsonArray arr = doc.createNestedArray("devices");
    for (const auto& dev : devs) {
        JsonObject obj = arr.createNestedObject();
        obj["mac"]         = dev.mac;
        obj["name"]        = dev.name;
        obj["rssi"]        = dev.rssi;
        obj["connectable"] = dev.connectable;
        obj["addr_type"]   = dev.addrType;
    }

    String json;
    serializeJson(doc, json);

    String path = "/ble_scans/scan_" + String(millis()) + ".json";
    if (_api->writeFile(path, json)) {
        _api->log("ble_scanner", "Saved: " + path);
    }
}

} // namespace skullgate
